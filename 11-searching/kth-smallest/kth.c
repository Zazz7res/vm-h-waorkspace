/*
 * kth.c —— 第 K 小（顺序统计量）的五种实现 + 两有序数组附加题
 *
 * 阅读顺序建议：kth_by_sorting（基准）→ kth_by_heap（流式）→ kth_quickselect（主角）
 *              → kth_quickselect_bad_pivot（毒输入）→ kth_by_value_binary_search（不改数据）
 *              → kth_two_sorted（最烧脑的一个，放最后）。
 *
 * 全节不 malloc：所有临时数组都是栈上的 [KTH_MAX_N]，因此没有 free 责任。
 */

#include <stdio.h>     /* printf */
#include <limits.h>    /* INT_MIN / INT_MAX */
#include "kth.h"

/* ===================================================================
 * 小工具
 * =================================================================== */

static void swap_int(int x[], int i, int j)
{
    if (i == j) {
        return;                       /* 原地交换看着浪费，但 Lomuto 里 i==j 很常见，挡掉它 */
    }
    int t = x[i];
    x[i] = x[j];
    x[j] = t;
}

/*
 * 自写的 xorshift32 随机数。为什么不用 rand()？
 *   ① rand() 的具体序列由 libc 决定，换台机器结果就变 —— README 里的数字就对不上了；
 *   ② 教学代码要"可复现"：固定种子 ⇒ 每次跑输出逐字节相同，才能拿它对文档。
 * 种子必须非 0（xorshift 遇到全 0 会永远输出 0）。
 */
static unsigned kth_rng_state = 20261004u;

static int kth_rand_below(int bound)      /* 返回 [0, bound) */
{
    if (bound <= 1) {
        return 0;
    }
    kth_rng_state ^= kth_rng_state << 13;
    kth_rng_state ^= kth_rng_state >> 17;
    kth_rng_state ^= kth_rng_state << 5;
    return (int)(kth_rng_state % (unsigned)bound);
    /* 顺带一句：这里 %bound 有极轻微的分布不均（取模偏置），
     * 对"选 pivot"毫无影响；真要均匀得用拒绝采样，那是另一节课。 */
}

void kth_dump(const char *label, const int a[], int n)
{
    if (a == NULL || n <= 0) {
        printf("%s（空数组或非法长度 n=%d）\n", label ? label : "数组", n);
        return;
    }
    if (label) {
        printf("%s\n", label);
    }
    /* %8d 而不是 %4d：第 6 幕要拿"值域爆炸"的数组演示，四格宽会把负数和大数挤成一串。
     * 打印工具的对齐是为了让人**读得下去**，不是装饰。 */
    printf("    下标 ");
    for (int i = 0; i < n; i++) {
        printf("%8d", i);
    }
    printf("\n    数值 ");
    for (int i = 0; i < n; i++) {
        printf("%8d", a[i]);
    }
    printf("\n");
}

/* ===================================================================
 * 方法① 排序 + 取下标
 * =================================================================== */

/*
 * 内部先拷贝一份：这个函数承诺**不改调用者的数组**（和快选形成对照）。
 * 排序用插入排序而不是复用 10-sorting 里的快排：
 *   ① 十行就写完，读的人不必先懂分区；② 它能顺便把"排到一半"的状态打出来，
 *   让读者亲眼看到"答案要等到完全排好才出现" —— 这正是它相对快选的浪费之处。
 */
int kth_by_sorting(const int a[], int n, int k, long long *cmp_out)
{
    if (a == NULL || n <= 0 || k < 1 || k > n) {
        if (cmp_out) {
            *cmp_out = 0;
        }
        return -1;
    }

    int buf[KTH_MAX_N];
    for (int i = 0; i < n; i++) {
        buf[i] = a[i];
    }

    long long cmp = 0;
    for (int i = 1; i < n; i++) {
        int v = buf[i];
        int j = i - 1;
        while (j >= 0) {
            cmp++;
            if (buf[j] > v) {
                buf[j + 1] = buf[j];
                j--;
            } else {
                break;                 /* 遇到不大于 v 的就停：插入排序的提前退出 */
            }
        }
        buf[j + 1] = v;
    }

    if (cmp_out) {
        *cmp_out = cmp;
    }
    return buf[k - 1];                 /* k 从 1 数，下标从 0 数 ⇒ k-1 这道桥不能省 */
}

/* ===================================================================
 * 方法② 大小为 k 的大顶堆
 * =================================================================== */

/* 大顶堆：父 ≥ 子。sift_up 把新元素往上顶到合适位置 */
static void sift_up(int h[], int i)
{
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h[p] < h[i]) {             /* 父比子小 ⇒ 违反大顶堆，换 */
            swap_int(h, p, i);
            i = p;
        } else {
            break;
        }
    }
}

/* sift_down：堆顶被换成了一个小值，把它往下沉到位。n 是当前堆的有效大小 */
static void sift_down(int h[], int n, int i)
{
    for (;;) {
        int l = 2 * i + 1;
        int r = 2 * i + 2;
        int m = i;
        if (l < n && h[l] > h[m]) {
            m = l;
        }
        if (r < n && h[r] > h[m]) {
            m = r;
        }
        if (m == i) {
            break;                     /* 已经不比任何孩子小 */
        }
        swap_int(h, m, i);
        i = m;
    }
}

/*
 * 流程：
 *   ① 前 k 个元素直接建成一个 k 格大顶堆；
 *   ② 从第 k+1 个起：比堆顶小 ⇒ 堆顶（当前 k 个里最大的）不配留下，用它换掉堆顶、下沉；
 *      不比堆顶小 ⇒ 它连"最小的 k 个"都进不来，直接丢掉，一眼都不看第二遍。
 *   ③ 扫完之后堆里就是「全数组最小的 k 个」，堆顶是这 k 个里最大的 ⇒ 正好是第 k 小。
 * 这就是它能处理数据流的原因：**内存里永远只有 k 格**，来一个处理一个，不回头。
 */
int kth_by_heap(const int a[], int n, int k, int verbose)
{
    if (a == NULL || n <= 0 || k < 1 || k > n) {
        return -1;
    }

    int h[KTH_MAX_N];
    for (int i = 0; i < k; i++) {
        h[i] = a[i];
        sift_up(h, i);                 /* 逐个插入建堆：O(k log k)，简单起见不用 O(k) 的建堆法 */
    }

    if (verbose) {
        printf("      建堆：前 %d 个元素进堆（数组形态即堆的层序）\n        堆 = [", k);
        for (int i = 0; i < k; i++) {
            printf("%d ", h[i]);
        }
        printf("]  堆顶 h[0]=%d ← 这 %d 个里最大的那个\n", h[0], k);
    }

    for (int i = k; i < n; i++) {
        int top_before = h[0];          /* 先把"淘汰前的堆顶"存下来：换完之后 h[0] 已经不是它了 */
        int replaced = 0;
        if (a[i] < h[0]) {
            h[0] = a[i];               /* 换掉堆顶：那个"最可能不是答案"的最大值被淘汰 */
            sift_down(h, k, 0);
            replaced = 1;
        }
        if (verbose) {
            printf("      来了 a[%d]=%d  vs 堆顶 %d → %s\n        堆 = [",
                   i, a[i], top_before,
                   replaced ? "比堆顶小，换掉堆顶并下沉" : "不比堆顶小，直接丢弃");
            for (int t = 0; t < k; t++) {
                printf("%d ", h[t]);
            }
            printf("]  堆顶 = %d\n", h[0]);
        }
    }

    return h[0];
}

/* ===================================================================
 * 方法③ 快速选择
 * =================================================================== */

/*
 * Lomuto 分区：pivot 先换到 hi，i 指"下一个该放小值的位置"，j 一路扫过去。
 * 扫完把 pivot 换回 i 处 ⇒ **i 左边全 ≤ pivot，i 右边全 ≥ pivot**，pivot 落到了它最终的排名位置。
 *
 * 这一句是整个快选的立命之本：partition 结束后，落在 p 的那个元素**已经在它最终的位置上**。
 * 快排据此递归两侧；快选只看 p 和 target 谁大谁小，**只跟着一侧走** ⇒ n + n/2 + n/4 … < 2n。
 *
 * fixed_first 非 0 时永远拿区间第一个元素当 pivot（故意写坏，用来演示毒输入）。
 */
static int partition_lomuto(int a[], int lo, int hi, long long *cmp, int fixed_first)
{
    int pi = fixed_first ? lo : lo + kth_rand_below(hi - lo + 1);
    swap_int(a, pi, hi);                       /* 把 pivot 藏到末尾，循环里就不必再管它 */
    int pivot = a[hi];

    int i = lo;
    for (int j = lo; j < hi; j++) {
        if (cmp) {
            (*cmp)++;
        }
        if (a[j] <= pivot) {                   /* 带等号：重复值会被甩到左边，不影响正确性 */
            swap_int(a, i, j);
            i++;
        }
    }
    swap_int(a, i, hi);                        /* pivot 回到最终位置 */
    return i;
}

int kth_quickselect(int a[], int n, int k, int verbose, long long *cmp_out)
{
    if (a == NULL || n <= 0 || k < 1 || k > n) {
        if (cmp_out) {
            *cmp_out = 0;
        }
        return -1;
    }

    int target = k - 1;                        /* 第 k 小 ⇔ 下标 k-1 */
    int lo = 0;
    int hi = n - 1;
    long long cmp = 0;
    int round = 0;

    if (verbose) {
        kth_dump("      进入快选前，数组是：", a, n);
    }

    while (lo <= hi) {
        round++;
        int p = partition_lomuto(a, lo, hi, &cmp, 0);

        if (verbose) {
            kth_dump("      partition 之后：", a, n);
            printf("      第 %d 轮 区间[%d..%d]  pivot 落点 p=%d  a[p]=%d\n", round, lo, hi, p, a[p]);
            if (p == target) {
                printf("              p == 目标下标 %d → 答案就是它，不用再看任何一边\n", target);
            } else if (p < target) {
                printf("              p < %d ⇒ 答案在右半，扔左半：lo = %d\n", target, p + 1);
            } else {
                printf("              p > %d ⇒ 答案在左半，扔右半：hi = %d\n", target, p - 1);
            }
        }

        if (p == target) {
            if (cmp_out) {
                *cmp_out = cmp;
            }
            return a[p];
        }
        if (p < target) {
            lo = p + 1;
        } else {
            hi = p - 1;
        }
    }

    /* 理论上到不了这里：target 一直在 [0, n-1] 内，而每轮 partition 都会把某个元素放到最终位置 */
    if (cmp_out) {
        *cmp_out = cmp;
    }
    return -1;
}

/*
 * 毒输入版：**不改调用者数组**（内部拷贝），因为要拿同一个数组反复试。
 * pivot 固定取区间首元素 + 输入已升序 ⇒ 每轮 partition 只能把问题规模减 1，
 * 比较次数 1+2+…+(n-1) = n(n-1)/2 ⇒ O(n²)。第 5 幕就是量这个数。
 */
int kth_quickselect_bad_pivot(const int a[], int n, int k, long long *cmp_out)
{
    if (a == NULL || n <= 0 || k < 1 || k > n) {
        if (cmp_out) {
            *cmp_out = 0;
        }
        return -1;
    }

    int buf[KTH_MAX_N];
    for (int i = 0; i < n; i++) {
        buf[i] = a[i];
    }

    int target = k - 1;
    int lo = 0;
    int hi = n - 1;
    long long cmp = 0;

    while (lo <= hi) {
        int p = partition_lomuto(buf, lo, hi, &cmp, 1);
        if (p == target) {
            if (cmp_out) {
                *cmp_out = cmp;
            }
            return buf[p];
        }
        if (p < target) {
            lo = p + 1;
        } else {
            hi = p - 1;
        }
    }

    if (cmp_out) {
        *cmp_out = cmp;
    }
    return -1;
}

/* ===================================================================
 * 方法④ 值域计数
 * =================================================================== */

/*
 * 思路：既然值域很小，就别比大小了，直接**数**。
 * 从最小值往上累加个数，累到 ≥ k 的那个值就是答案。
 * 先算值域宽度并判断可行性：值域一爆炸（比如 int 全域）内存就没了 ⇒ 返回 0 让调用者换方法。
 * 注意 range 用 long long 算：max=INT_MAX、min=INT_MIN 时 int 装不下这个宽度。
 */
int kth_by_counting(const int a[], int n, int k, int *answer_out)
{
    if (a == NULL || n <= 0 || k < 1 || k > n) {
        return 0;
    }

    int mn = a[0];
    int mx = a[0];
    for (int i = 1; i < n; i++) {
        if (a[i] < mn) {
            mn = a[i];
        }
        if (a[i] > mx) {
            mx = a[i];
        }
    }

    long long range = (long long)mx - mn + 1;
    if (range > KTH_COUNT_MAX_RANGE) {
        return 0;                        /* 此法不适用：值域太宽 */
    }

    int cnt[KTH_COUNT_MAX_RANGE];
    for (int i = 0; i < (int)range; i++) {
        cnt[i] = 0;
    }
    for (int i = 0; i < n; i++) {
        cnt[a[i] - mn]++;                /* a[i]-mn 一定落在 [0, range-1]，无越界 */
    }

    int acc = 0;
    for (int v = 0; v < (int)range; v++) {
        acc += cnt[v];
        if (acc >= k) {
            if (answer_out) {
                *answer_out = mn + v;
            }
            return 1;
        }
    }
    return 0;                            /* 只有 k > n 才可能走到，前面已挡 */
}

/* ===================================================================
 * 方法⑤ 值域二分答案
 * =================================================================== */

/* 判据：≤ x 的元素有几个。O(n)，且**关于 x 单调不减** —— 单调 ⇒ 能二分 */
static int count_less_equal(const int a[], int n, int x)
{
    int c = 0;
    for (int i = 0; i < n; i++) {
        if (a[i] <= x) {
            c++;
        }
    }
    return c;
}

/*
 * 在**值域** [mn, mx] 上二分，找"第一个让个数 ≥ k 的值"。这就是 lower_bound 模板：
 *     while (lo < hi) { mid; if (判据(mid) 成立) hi = mid; else lo = mid + 1; }
 * 注意二分的对象是"值"而不是"下标" —— 这是二分答案和二分查找唯一的观念差别，
 * 也是 11-searching/binary-search 第 7 幕埋的那条线在这里接上的地方。
 *
 * 代价 O(n log R)：每轮判据要扫全数组（n），值域二分 log R 轮（R = mx-mn+1）。
 * 换来的是：**一行都不改数据、不需要额外内存、判据可以流式重放**。
 */
int kth_by_value_binary_search(const int a[], int n, int k, int *steps_out)
{
    if (a == NULL || n <= 0 || k < 1 || k > n) {
        if (steps_out) {
            *steps_out = 0;
        }
        return -1;
    }

    int mn = a[0];
    int mx = a[0];
    for (int i = 1; i < n; i++) {
        if (a[i] < mn) {
            mn = a[i];
        }
        if (a[i] > mx) {
            mx = a[i];
        }
    }

    int lo = mn;
    int hi = mx;
    int steps = 0;

    while (lo < hi) {
        steps++;
        /* 值域里做防溢出：lo/hi 可能是 INT_MIN/INT_MAX，(hi-lo) 用 int 算会溢出 */
        int mid = (int)((long long)lo + ((long long)hi - lo) / 2);
        if (count_less_equal(a, n, mid) >= k) {
            hi = mid;                    /* mid 自己可能就是答案，留在窗口里 */
        } else {
            lo = mid + 1;
        }
    }

    if (steps_out) {
        *steps_out = steps;
    }
    return lo;
}

/* ===================================================================
 * 附加题：两个有序数组的第 k 小
 * =================================================================== */

/*
 * 目标：把两个有序数组各切一刀，左半块**合起来正好 k 个数**，且左半块所有数 ≤ 右半块所有数。
 *      a:  a[0..i-1] | a[i..]        左半块 = a[i-1] 和 b[j-1] 里较大的那个
 *      b:  b[0..j-1] | b[j..]        j = k - i，所以只需要二分开 i
 * 合法条件只有两条：
 *      a[i-1] <= b[j]   且   b[j-1] <= a[i]      （交叉验证：谁都不能"伸过界"）
 * 满足了，答案就是 max(a[i-1], b[j-1])。
 *
 * 为什么只在**短**数组上二分：i 的可行范围是 [max(0, k-nb), min(k, na)]，
 * 短数组长度小 ⇒ 区间小 ⇒ log(min(m,n))；同时保证 j = k-i 一定落在 [0, nb] 内，不会越界。
 *
 * 边界怎么破：i==0 表示"a 这边一个都不取"，此时 a[i-1] 不存在；
 * i==na 表示"a 全取"，此时 a[i] 不存在。**用 INT_MIN / INT_MAX 当哨兵**，
 * 让"不存在的那一侧"在任何比较里自动输/赢，就不必为边界写一堆 if —— 这是本节最实用的一招。
 */
int kth_two_sorted(const int a[], int na, const int b[], int nb, int k)
{
    if (a == NULL || b == NULL || na < 0 || nb < 0 || k < 1 || k > na + nb) {
        return -1;
    }

    /* 保证 a 是较短的那个（必要时换一下指针和长度） */
    if (na > nb) {
        const int *t = a; a = b; b = t;
        int tn = na; na = nb; nb = tn;
    }

    int lo = (k - nb > 0) ? k - nb : 0;          /* i 太小会让 j = k-i 超过 nb */
    int hi = (k < na) ? k : na;                  /* i 太大则 a 不够取 */

    while (lo <= hi) {
        int i = lo + (hi - lo) / 2;
        int j = k - i;

        int a_left  = (i == 0)  ? INT_MIN : a[i - 1];
        int a_right = (i == na) ? INT_MAX : a[i];
        int b_left  = (j == 0)  ? INT_MIN : b[j - 1];
        int b_right = (j == nb) ? INT_MAX : b[j];

        if (a_left <= b_right && b_left <= a_right) {
            return (a_left > b_left) ? a_left : b_left;
        }
        if (a_left > b_right) {
            hi = i - 1;                          /* a 取多了，少取点 a */
        } else {
            lo = i + 1;                          /* a 取少了 */
        }
    }
    return -1;                                   /* 走到这里说明输入根本没排好序 */
}

/* 归并对拍：谁小就弹出谁，弹到第 k 个。O(k)，显然正确，用来验收划分法 */
int kth_two_sorted_linear(const int a[], int na, const int b[], int nb, int k)
{
    if (a == NULL || b == NULL || k < 1 || k > na + nb) {
        return -1;
    }
    int i = 0;
    int j = 0;
    int v = -1;
    for (int step = 0; step < k; step++) {
        if (i < na && (j >= nb || a[i] <= b[j])) {
            v = a[i++];
        } else {
            v = b[j++];
        }
    }
    return v;
}

/* ===================================================================
 * 衍生：中位数
 * =================================================================== */

/*
 * 中位数在 n 为偶数时有两种约定：中间偏左（下中位数）和中间偏右（上中位数）。
 * 本节两个都给，因为工程里"取哪一个"往往是接口约定问题而不是算法问题。
 * n 为奇数时两者相同。
 */
int kth_median_lower(const int a[], int n)
{
    return kth_by_sorting(a, n, (n + 1) / 2, NULL);
}

int kth_median_upper(const int a[], int n)
{
    return kth_by_sorting(a, n, n / 2 + 1, NULL);
}

/* ===================================================================
 * 验收：五法对拍
 * =================================================================== */

/*
 * 把同一个问题交给五种方法，横向对比答案和代价。
 * 为什么要专门写这个函数？因为**第 k 小的实现极易在"重复值 + 边界 k"上悄悄错一个**，
 * 而五种思路各异的实现给出同一个答案，比任何单测用例都可信（它们不会朝同一个方向错）。
 */
int kth_all_agree(const int a[], int n, int k, int verbose)
{
    if (a == NULL || n <= 0 || k < 1 || k > n) {
        return 0;
    }

    long long cmp = 0;
    int v1 = kth_by_sorting(a, n, k, &cmp);

    int v2 = kth_by_heap(a, n, k, 0);

    int work[KTH_MAX_N];
    for (int i = 0; i < n; i++) {
        work[i] = a[i];                          /* 快选会打乱数组 ⇒ 必须先拷贝，这是它的接口代价 */
    }
    long long qc = 0;
    int v3 = kth_quickselect(work, n, k, 0, &qc);

    int v4 = 0;
    int ok4 = kth_by_counting(a, n, k, &v4);

    int steps = 0;
    int v5 = kth_by_value_binary_search(a, n, k, &steps);

    printf("    方法             答案     代价（实测）\n");
    printf("    ① 排序取下标     %d        比较 %lld 次\n", v1, cmp);
    printf("    ② 大小 %d 大顶堆  %d        内存只用 %d 格\n", k, v2, k);
    printf("    ③ 快速选择       %d        比较 %lld 次（平均线性）\n", v3, qc);
    if (ok4) {
        printf("    ④ 值域计数       %d        值域 ≤ %d 才可用\n", v4, KTH_COUNT_MAX_RANGE);
    } else {
        printf("    ④ 值域计数       —          值域太宽，此法不适用\n");
    }
    printf("    ⑤ 值域二分答案   %d        扫数组 %d 遍（%d 轮 × n）\n", v5, steps, steps);

    int agree = (v1 == v2 && v1 == v3 && v1 == v5 && (!ok4 || v1 == v4));
    if (verbose) {
        printf("    → %s\n", agree ? "五法一致 ✓" : "五法不一致 ✗ 有 bug，回去查");
    }
    return agree;
}
