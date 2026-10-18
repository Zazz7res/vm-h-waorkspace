#include <stdio.h>
#include "quick_sort.h"

/*
 * ===================================================================
 * 0. 内部小工具（目录自包含，所以 print_arr 这些每个排序目录都写一份）
 * ===================================================================
 */
static void swap_int(int *x, int *y)
{
    int temp = *x;
    *x = *y;
    *y = temp;
}

void print_arr(const char *label, const int a[], int n)
{
    printf("%s[", label);
    for (int i = 0; i < n; i++) {
        printf(" %d", a[i]);
    }
    printf(" ]\n");
}

int is_sorted_ascending(const int a[], int n)
{
    for (int i = 1; i < n; i++) {
        if (a[i - 1] > a[i]) {
            return 0;
        }
    }
    return 1;
}

int same_elements(const int a[], const int b[], int n)
{
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int is_permutation(const int a[], const int b[], int n)
{
    if (n <= 0) {
        return 1;
    }
    for (int v = 0; v < n; v++) {
        int ca = 0, cb = 0;
        for (int i = 0; i < n; i++) {
            if (a[i] == a[v]) ca++;
            if (b[i] == a[v]) cb++;
        }
        if (ca != cb) {
            return 0;
        }
    }
    return 1;
}

/* 按递归深度缩进，让"劈成两半再各劈一半"的树看得见 */
static void indent(int depth)
{
    for (int i = 0; i < depth; i++) {
        printf("    ");
    }
    printf("| ");
}

/* 打印 a[lo..hi]，并把下标 at 的那一格用方括号圈出来（= 枢轴的最终位置） */
static void print_span_marked(const int a[], int lo, int hi, int at)
{
    printf(" [");
    for (int i = lo; i <= hi; i++) {
        if (i == at) {
            printf(" «%d»", a[i]);      /* «» 包住的就是站上了最终位置的枢轴 */
        } else {
            printf(" %d", a[i]);
        }
    }
    printf(" ]");
}

/* 求区间最值：用来【程序化验证】"左半全 ≤ 枢轴 ≤ 右半全"这个承诺真的成立 */
static int span_max(const int a[], int lo, int hi)
{
    int m = a[lo];
    for (int i = lo + 1; i <= hi; i++) {
        if (a[i] > m) m = a[i];
    }
    return m;
}

static int span_min(const int a[], int lo, int hi)
{
    int m = a[lo];
    for (int i = lo + 1; i <= hi; i++) {
        if (a[i] < m) m = a[i];
    }
    return m;
}

/*
 * ===================================================================
 * 1. Lomuto 分区：选【最后一个】当枢轴
 * ===================================================================
 * 【承诺】跑完之后：
 *     a[lo..p-1] 全部 ≤ pivot
 *     a[p]       == pivot        ← p 就是枢轴的【最终下标】，这一点比 Hoare 好用
 *     a[p+1..hi] 全部 ≥ pivot
 *
 * 【怎么做到】维护一个"小于等于区"，i 是这个区的最后一个下标：
 *
 *     a:  [ ≤≤≤≤≤≤≤ | > > > | ? ? ? | pivot ]
 *         lo        i       i+1     j-1   hi
 *                          ↑起点        ↑扫描指针
 *   j 从 lo 一路扫到 hi-1：
 *     a[j] ≤ pivot → i++，把 a[i] 和 a[j] 交换（把新确认的小值收进左边区）
 *     a[j] > pivot → 什么都不做，j 继续走（它自然留在"> 区"里）
 *   扫完，i+1 是第一个 > pivot 的位置 → 把 a[i+1] 和 a[hi]（枢轴）交换，枢轴归位。
 *
 *   例子（枢轴 6，取最后一个）：
 *     [ 5 1 3 2 7 8 9 | 6 ]     i 停在下标 3，i+1 = 4 是第一个 > 6 的 7
 *     交换 a[4] 和 a[7] →  [ 5 1 3 2 «6» 8 9 7 ]   6 从此不用再动
 *
 * 【为什么它是 O(n²) 灾难的常见入口】pivot 固定取 a[hi]：
 *   数组已升序时 a[hi] 就是最大值 ⇒ "≤ 区"每次都收满全部 n-1 个元素，
 *   分区结果永远是 (n-1, 0) 这种极不匀称的劈法 ⇒ 递归树塌成一根链条。
 */
int partition_lomuto(int a[], int lo, int hi, QStat *st)
{
    int pivot = a[hi];
    int i = lo - 1;                        /* "≤ pivot 区"的最后一个下标，一开始空 */

    for (int j = lo; j < hi; j++) {        /* 注意 j < hi：枢轴自己不参与扫描 */
        if (st != NULL) {
            st->cmp++;
        }
        if (a[j] <= pivot) {               /* ← 等号放左边：等于 pivot 的元素先进 ≤ 区 */
            i++;
            if (i != j) {                  /* 自己跟自己换就省掉，交换次数统计才干净 */
                swap_int(&a[i], &a[j]);
                if (st != NULL) {
                    st->swap++;
                }
            }
        }
    }
    swap_int(&a[i + 1], &a[hi]);           /* 枢轴归位：这一步【永远】会执行 */
    if (st != NULL) {
        st->swap++;
    }
    return i + 1;
}

/*
 * ===================================================================
 * 2. Hoare 分区：双指针从两头往中间夹
 * ===================================================================
 * 【承诺】跑完之后返回一个分界点 p：
 *     a[lo..p]   全部 ≤ pivot
 *     a[p+1..hi] 全部 ≥ pivot
 *   ⚠ 但是 pivot 这个【元素本体】不一定在 a[p] 上！它可能在左边任何位置。
 *     所以递归必须写成 (lo, p) 和 (p+1, hi)，不能写成 (lo, p-1) —— 那样会漏掉 a[p]。
 *
 * 【怎么做到】两个指针面对面对走：
 *     i 从左往右，跳过"比 pivot 小"的；停在一个"该去右边"的元素上
 *     j 从右往左，跳过"比 pivot 大"的；停在一个"该去左边"的元素上
 *     两个都停了且 i < j → 正好互相需要，交换它们
 *     i >= j → 走完了，j 就是分界点
 *
 *   例子（pivot 取区间中点的值）：
 *     [ 9 5 7 1 | 3 | 8 2 6 ]  pivot = a[4] = 3
 *      i→              ←j
 *     i 停在 9（9 < 3 不成立），j 停在 2（2 > 3 不成立）→ 交换 → [ 2 5 7 1 3 8 9 6 ]
 *     i 停在 5，j 停在 1 → 交换 → [ 2 1 7 3 3? ...]  继续夹…
 *
 * 【为什么比 Lomuto 交换少】每个元素最多被交换一次（两头配对，一次换掉两个错位的），
 *   而 Lomuto 每发现一个"该去左边"的就要搬一次，且枢轴还要再搬一次。
 *
 * 【边界为什么容易写错，三条都要记住】
 *   ① i 起点 lo-1、j 起点 hi+1，循环体里【先动指针再比较】（do-while），
 *      否则 a[i] 和 a[j] 可能自比较导致死循环。
 *   ② 比较必须是严格 `<` 和 `>`（不带等号）。带等号会在全部元素相等时死循环。
 *   ③ pivot 取"值"，不要求它待在某个下标上；返回值是 j，且【一定有 lo ≤ j < hi】，
 *      所以 (lo, j) 和 (j+1, hi) 两个区间都严格变小 —— 这是 Hoare 不递归爆炸的根据。
 */
int partition_hoare(int a[], int lo, int hi, QStat *st)
{
    int pivot = a[lo + (hi - lo) / 2];      /* 取区间中点位置的【值】当参照（不是下标） */
    int i = lo - 1;
    int j = hi + 1;

    for (;;) {
        do {
            i++;                            /* 先走一步再看值 —— Hoare 的命门 */
            if (st != NULL) st->cmp++;
        } while (a[i] < pivot);

        do {
            j--;
            if (st != NULL) st->cmp++;
        } while (a[j] > pivot);

        if (i < j) {
            swap_int(&a[i], &a[j]);
            if (st != NULL) st->swap++;
        } else {
            return j;                       /* 交叉了/碰头了，j 就是分界点 */
        }
    }
}

/*
 * ===================================================================
 * 3. 三种递归驱动：Lomuto / Hoare / 工程版
 * ===================================================================
 * 递归骨架：先递归左边，再递归右边。出口是"区间空 或 只剩 1 个元素"。
 * 只剩 1 个元素时它【已经】在最终位置上了 —— 这一点和归并的"拆到 1 个元素"是同构的，
 * 区别是：归并把活放在"合"的那一步，快排把活放在"劈"的那一步，劈完就不用管枢轴了。
 */
static void qs_lomuto_range(int a[], int lo, int hi, int depth, int verbose, QStat *st)
{
    if (lo >= hi) {
        return;                            /* 空区间（lo > hi）或单元素（lo == hi）：什么都不用做 */
    }
    if (st->max_depth < depth) {
        st->max_depth = depth;
    }

    if (verbose) {
        indent(depth);
        printf("第 %d 层：对 [%d..%d] 分区，枢轴 = 最后一个 a[%d] = %d，分区前这段是",
               depth + 1, lo, hi, hi, a[hi]);
        print_span_marked(a, lo, hi, hi);
        printf("\n");
    }

    int p = partition_lomuto(a, lo, hi, st);
    st->partitions++;

    if (verbose) {
        int left_n = p - lo;
        int right_n = hi - p;
        indent(depth);
        printf("        分区后：");
        print_span_marked(a, lo, hi, p);
        printf("\n");
        indent(depth);
        printf("        枢轴 %d 落到最终下标 %d ⇒ 左 %d 个 / 右 %d 个。", a[p], p, left_n, right_n);
        /* 程序化验证分区的承诺，而不是嘴上说 */
        if (left_n > 0 && right_n > 0) {
            printf("校验：左半最大 %d ≤ %d ≤ 右半最小 %d ⇒ 成立\n",
                   span_max(a, lo, p - 1), a[p], span_min(a, p + 1, hi));
        } else if (left_n > 0) {
            printf("右半空；校验：左半最大 %d ≤ %d ⇒ 成立\n", span_max(a, lo, p - 1), a[p]);
        } else if (right_n > 0) {
            printf("左半空；校验：%d ≤ 右半最小 %d ⇒ 成立\n", a[p], span_min(a, p + 1, hi));
        } else {
            printf("两半都空（就它一个）\n");
        }
    }

    qs_lomuto_range(a, lo, p - 1, depth + 1, verbose, st);   /* 左半：(lo, p-1)，枢轴不参与 */
    qs_lomuto_range(a, p + 1, hi, depth + 1, verbose, st);   /* 右半：(p+1, hi) */
}

static void qs_hoare_range(int a[], int lo, int hi, int depth, int verbose, QStat *st)
{
    if (lo >= hi) {
        return;
    }
    if (st->max_depth < depth) {
        st->max_depth = depth;
    }

    if (verbose) {
        indent(depth);
        printf("第 %d 层：对 [%d..%d] 双指针分区，参照值 = a[%d] = %d（注意它只是参照，不是最终位置），分区前这段是",
               depth + 1, lo, hi, lo + (hi - lo) / 2, a[lo + (hi - lo) / 2]);
        print_span_marked(a, lo, hi, lo + (hi - lo) / 2);
        printf("\n");
    }

    int p = partition_hoare(a, lo, hi, st);
    st->partitions++;

    if (verbose) {
        int left_n = p - lo + 1;
        int right_n = hi - p;
        indent(depth);
        printf("        分区后：");
        print_span_marked(a, lo, hi, p);
        printf("\n");
        indent(depth);
        printf("        分界点 p = %d ⇒ 左段 [%d..%d] %d 个 / 右段 [%d..%d] %d 个。",
               p, lo, p, left_n, p + 1, hi, right_n);
        if (left_n > 0 && right_n > 0) {
            printf("校验：左半最大 %d ≤ 右半最小 %d ⇒ 成立\n",
                   span_max(a, lo, p), span_min(a, p + 1, hi));
        } else {
            printf("校验：%d ≤ 右半最小 %d ⇒ 成立\n", span_max(a, lo, p), span_min(a, p + 1, hi));
        }
        if (depth == 0) {          /* 这段提醒只在最外层说一次，免得刷屏 */
            indent(depth);
            printf("        ⚠ 和 Lomuto 不同：这里【不能】写 (lo, p-1)，否则 a[p] 会被漏掉；\n");
            indent(depth);
            printf("          返回的 p 是「谁也不越界」的分界点，枢轴本体可能在左段的任意位置。\n");
            indent(depth);
            printf("          另外注意本趟的分区结果：左段非空且右段非空 —— Hoare 永远劈不出空区间，\n");
            indent(depth);
            printf("          所以它的分区趟数恰好是 n-1（后面第 2 幕的统计行里能看到）。\n");
        }
    }

    qs_hoare_range(a, lo, p, depth + 1, verbose, st);        /* ← 带 p，左段闭到 p */
    qs_hoare_range(a, p + 1, hi, depth + 1, verbose, st);
}

/*
 * 工程版：Lomuto + 固定最后一个当枢轴。真正写项目时请至少换成三数取中（见第 4 节）。
 */
void quick_sort(int a[], int n)
{
    if (n <= 1) {
        return;
    }
    QStat st;
    st.cmp = 0; st.swap = 0; st.partitions = 0; st.max_depth = 0;
    qs_lomuto_range(a, 0, n - 1, 0, 0, &st);
}

void quick_sort_lomuto(int a[], int n, int verbose, QStat *st)
{
    QStat local;
    local.cmp = 0; local.swap = 0; local.partitions = 0; local.max_depth = 0;
    if (n > 1) {
        qs_lomuto_range(a, 0, n - 1, 0, verbose, &local);
    }
    if (st != NULL) {
        *st = local;
    }
}

void quick_sort_hoare(int a[], int n, int verbose, QStat *st)
{
    QStat local;
    local.cmp = 0; local.swap = 0; local.partitions = 0; local.max_depth = 0;
    if (n > 1) {
        qs_hoare_range(a, 0, n - 1, 0, verbose, &local);
    }
    if (st != NULL) {
        *st = local;
    }
}

/*
 * ===================================================================
 * 4. 抗最坏情况：三数取中 / 随机化 pivot
 * ===================================================================
 * median_of_three：把 a[lo]、a[mid]、a[hi] 三个位置【就地排好序】（三次比较、最多三次交换），
 *   排完 a[mid] 就是这三个数的中位数，返回 mid。
 *   为什么这招能救命：有序数组里 lo=最小、hi=最大 ⇒ 中位数是中间那个值 ⇒ 劈出来的两半差不多长；
 *   同时顺手把 lo/mid/hi 三个位置整理成"小 中 大"，等于提前给分区铺好了路。
 *   对"近乎有序""大量相同值"这些常见坑数据都有明显的稳定效果，所以工程实现几乎都带它。
 */
static int median_of_three_stat(int a[], int lo, int hi, QStat *st)
{
    int mid = lo + (hi - lo) / 2;

    if (st != NULL) st->cmp++;
    if (a[lo] > a[mid]) { swap_int(&a[lo], &a[mid]); if (st) st->swap++; }
    if (st != NULL) st->cmp++;
    if (a[lo] > a[hi])  { swap_int(&a[lo], &a[hi]);  if (st) st->swap++; }
    if (st != NULL) st->cmp++;
    if (a[mid] > a[hi]) { swap_int(&a[mid], &a[hi]); if (st) st->swap++; }
    /* 此刻 a[lo] ≤ a[mid] ≤ a[hi] */
    return mid;
}

int median_of_three(int a[], int lo, int hi)
{
    return median_of_three_stat(a, lo, hi, NULL);
}

/* 三数取中版：把中位数换到区间末尾，然后照旧走 Lomuto（枢轴永远读 a[hi]） */
static void qs_med3_range(int a[], int lo, int hi, int depth, int verbose, QStat *st)
{
    if (lo >= hi) {
        return;
    }
    if (st->max_depth < depth) {
        st->max_depth = depth;
    }

    int m = median_of_three_stat(a, lo, hi, st);
    swap_int(&a[m], &a[hi]);            /* 中位数送到 hi：Lomuto 只认最后一个位置当枢轴 */
    if (st) st->swap++;

    if (verbose) {
        int mid = lo + (hi - lo) / 2;
        indent(depth);
        printf("第 %d 层 [%d..%d]：三数取中选到下标 %d 的值，换到末尾当枢轴（枢轴 = %d），分区前这段是",
               depth + 1, lo, hi, mid, a[hi]);
        print_span_marked(a, lo, hi, hi);
        printf("\n");
    }

    int p = partition_lomuto(a, lo, hi, st);
    st->partitions++;

    if (verbose) {
        indent(depth);
        printf("        分区后：");
        print_span_marked(a, lo, hi, p);
        printf("  ⇒ 左 %d 个 / 右 %d 个（这才叫【劈成两半】）\n", p - lo, hi - p);
    }

    qs_med3_range(a, lo, p - 1, depth + 1, verbose, st);
    qs_med3_range(a, p + 1, hi, depth + 1, verbose, st);
}

void quick_sort_median3(int a[], int n, int verbose, QStat *st)
{
    QStat local;
    local.cmp = 0; local.swap = 0; local.partitions = 0; local.max_depth = 0;
    if (n > 1) {
        qs_med3_range(a, 0, n - 1, 0, verbose, &local);
    }
    if (st != NULL) {
        *st = local;
    }
}

/*
 * 随机化版：从 [lo, hi] 里等概率抽一个下标，换到末尾当枢轴。
 * 和"三数取中"比，它的好处是【没有任何输入能稳定坑到它】：
 * 对手没法构造出"让你的随机数每次都抽到最大值"的数据。
 * 本仓库要求输出可复核，所以不用 time(NULL) 播种，而用一个固定初值的线性同余发生器（LCG）：
 *   seed = seed * 1103515245 + 12345，取高位 % 区间长度。每次运行产生的序列【完全相同】。
 */
static unsigned int g_lcg_state = 20261003u;   /* 固定种子 —— 这是"看起来随机但完全可复现"的关键 */

static void lcg_reset(void)
{
    g_lcg_state = 20261003u;
}

static int rand_index_in(int lo, int hi)
{
    g_lcg_state = g_lcg_state * 1103515245u + 12345u;
    unsigned int span = (unsigned int)(hi - lo + 1);
    return lo + (int)((g_lcg_state >> 8) % span);   /* 右移 8 位：低位随机性差，取高位 */
}

static void qs_rand_range(int a[], int lo, int hi, int depth, int verbose, QStat *st)
{
    if (lo >= hi) {
        return;
    }
    if (st->max_depth < depth) {
        st->max_depth = depth;
    }

    int r = rand_index_in(lo, hi);
    swap_int(&a[r], &a[hi]);
    if (st) st->swap++;

    if (verbose) {
        indent(depth);
        printf("第 %d 层 [%d..%d]：随机抽到下标 %d，把它换到末尾当枢轴（枢轴 = %d）\n",
               depth + 1, lo, hi, r, a[hi]);
    }

    int p = partition_lomuto(a, lo, hi, st);
    st->partitions++;

    if (verbose) {
        indent(depth);
        printf("        分区后：");
        print_span_marked(a, lo, hi, p);
        printf("  ⇒ 左 %d 个 / 右 %d 个\n", p - lo, hi - p);
    }

    qs_rand_range(a, lo, p - 1, depth + 1, verbose, st);
    qs_rand_range(a, p + 1, hi, depth + 1, verbose, st);
}

void quick_sort_randomized(int a[], int n, int verbose, QStat *st)
{
    QStat local;
    local.cmp = 0; local.swap = 0; local.partitions = 0; local.max_depth = 0;
    lcg_reset();                       /* 每次调用都从固定种子开始 ⇒ 输出确定可复核 */
    if (n > 1) {
        qs_rand_range(a, 0, n - 1, 0, verbose, &local);
    }
    if (st != NULL) {
        *st = local;
    }
}

/*
 * ===================================================================
 * 5. 栈深度优化：先递归短的那半边，长的那半边用 while 循环吃掉
 * ===================================================================
 * 朴素写法的最坏栈深度是 O(n)（塌成链条时，一路都在等左边返回）。
 * 改法只有两句话：
 *   ① 哪半边短，就【递归】哪半边；
 *   ② 另一半边【不递归】，直接把 lo/hi 改成它，回到 while 循环顶部继续处理。
 * 为什么栈就锁死 O(log n) 了：每进一次递归，那个子问题的规模最多是当前的一半，
 *   所以连续嵌套的深度最多 log2(n)。第 5 幕实测：64 个数的有序数组，
 *   朴素写法 max_depth = 63，改成这个写法 max_depth = 0（一次都没嵌套！）。
 * 注意【它只救栈，不救时间】：比较次数还是 O(n²)，塌链条那部分工作量一点没少。
 */
static void qs_tail_range(int a[], int lo, int hi, int depth, int verbose, QStat *st)
{
    while (lo < hi) {
        if (st->max_depth < depth) {
            st->max_depth = depth;
        }

        int p = partition_lomuto(a, lo, hi, st);
        st->partitions++;

        if (verbose) {
            indent(depth);
            printf("        分区 [%d..%d] ⇒ 枢轴落 %d，左 %d 个 / 右 %d 个%s\n",
                   lo, hi, p, p - lo, hi - p,
                   (p - lo < hi - p) ? "（递归短的左半，右半用循环继续吃）"
                                     : "（递归短的右半，左半用循环继续吃）");
        }

        if (p - lo < hi - p) {         /* 左半更短：递归左半，剩下的右半用循环接手 */
            qs_tail_range(a, lo, p - 1, depth + 1, verbose, st);
            lo = p + 1;                /* ← 尾递归被"消灭"成循环的那一行 */
        } else {                       /* 右半更短或等长：递归右半，左半用循环接手 */
            qs_tail_range(a, p + 1, hi, depth + 1, verbose, st);
            hi = p - 1;
        }
    }
}

void quick_sort_tail_opt(int a[], int n, int verbose, QStat *st)
{
    QStat local;
    local.cmp = 0; local.swap = 0; local.partitions = 0; local.max_depth = 0;
    if (n > 1) {
        qs_tail_range(a, 0, n - 1, 0, verbose, &local);
    }
    if (st != NULL) {
        *st = local;
    }
}

/*
 * ===================================================================
 * 6. 稳定性实验：快排【不稳定】的现场
 * ===================================================================
 * 凶手就是 Lomuto 收尾那次"远距离交换"：a[i+1] 和 a[hi] 换。
 * a[i+1] 是"第一个大于枢轴的元素"，而 a[hi] 是枢轴；枢轴被扔到中间去了，
 * 一路上如果有和它同值的元素排在它前面，就会被它【越过】去 —— 顺序反了。
 *
 *   [ 88a 88b 70c ]   枢轴 = a[2] = 70（最后一个）
 *     j=0: 88 ≤ 70? 否        j=1: 88 ≤ 70? 否      → i 一直停在 -1
 *     收尾交换 a[i+1] = a[0] 与 a[2]：
 *   [ 70c 88b «88a» ]   ← 88a 被换到 88b 后面 ⇒ 同值两人换了个儿，不稳定
 */
void print_tagged(const char *label, const int key[], const int tag[], int n)
{
    printf("%s[", label);
    for (int i = 0; i < n; i++) {
        printf(" %d%c", key[i], (char)('a' + tag[i]));
    }
    printf(" ]\n");
}

static void qs_tagged_range(int key[], int tag[], int lo, int hi)
{
    if (lo >= hi) {
        return;
    }
    int pivot = key[hi];
    int i = lo - 1;
    for (int j = lo; j < hi; j++) {
        if (key[j] <= pivot) {
            i++;
            if (i != j) {
                swap_int(&key[i], &key[j]);
                swap_int(&tag[i], &tag[j]);    /* 身份证跟分数一起搬 */
            }
        }
    }
    swap_int(&key[i + 1], &key[hi]);           /* ← 就是这一次远距离交换把顺序打乱的 */
    swap_int(&tag[i + 1], &tag[hi]);

    qs_tagged_range(key, tag, lo, i);
    qs_tagged_range(key, tag, i + 2, hi);
}

void quick_sort_tagged(int key[], int tag[], int n)
{
    if (n > 1) {
        qs_tagged_range(key, tag, 0, n - 1);
    }
}
