#include <stdio.h>
#include <stdlib.h>
#include "topk.h"

/*
 * ===================================================================
 * 这一页把 Top-K 的三种解法都实现出来，并且【可测量】：
 *   ① topk_by_sort        —— 先全排序，再取尾巴 K 个
 *   ② topk_by_heap        —— 固定大小的 K 最小堆（标准答案）
 *   ③ topk_by_quickselect —— 快速选择（平均 O(n)）
 *
 * 三者都【不修改】调用者的输入数组，内部自己拷一份。
 * 这一点很重要：Top-K 的使用场景（日志、榜单、推荐）几乎都不能接受
 * "把用户的数据打乱"这个副作用。要改原数组必须由调用方明确同意。
 * ===================================================================
 */

void topk_print_array(const char *name, const int *a, int n)
{
    printf("  %s (n=%d): [", name, n);
    int show = n < 16 ? n : 16;
    for (int i = 0; i < show; i++) {
        printf(" %d", a[i]);
    }
    if (show < n) {
        printf(" ...");
    }
    printf(" ]\n");
}

/* =====================================================================
 * 通用工具：交换两个 int
 * ===================================================================== */
static void swap2(int *x, int *y)
{
    int t = *x;
    *x = *y;
    *y = t;
}

/* =====================================================================
 * 方法①用的排序：堆排序（升序），直接在拷贝上原地做
 * ---------------------------------------------------------------------
 * 为什么这里用堆排序而不是快排？
 *   - 它是 O(n log n) 【最坏也是】，不会因为测试数据碰巧有序而退化，
 *     这样和另外两个方法比"比较次数"才是干净的对比。
 *   - 顺便复习刚学的堆：建堆 O(n) + n 次 pop 变成 O(n log n)。
 *   （完整的堆排序讲解在 10-sorting/heap-sort，这里只用它当"全排序"的代表。）
 *
 * 堆排序的两个阶段：
 *   阶段 1：把整个数组 heapify 成【最大堆】（自底向上，i 从 n/2-1 到 0）
 *   阶段 2：反复"堆顶(最大值)和当前未排序段的最右端交换 → 未排序段缩 1 → 新堆顶下沉"
 *           这其实就是把 heap_pop 的"剪尾巴"改成"尾巴就是排好 seat"，
 *           于是数组尾巴那边逐渐变成【升序】。
 * ===================================================================== */
static void sift_down_max(int *a, int heap_n, int i, long *cmp)
{
    for (;;) {
        int l = 2 * i + 1;
        int r = 2 * i + 2;
        if (l >= heap_n) {
            return;                     /* 叶子 */
        }
        int big = l;
        if (r < heap_n) {
            (*cmp)++;
            if (a[r] > a[l]) {
                big = r;                /* 跟【更大的】孩子换（最大堆） */
            }
        }
        (*cmp)++;
        if (!(a[big] > a[i])) {
            return;                     /* 父亲已经压得住 */
        }
        swap2(&a[i], &a[big]);
        i = big;
    }
}

static void heapsort_asc(int *a, int n, long *cmp)
{
    for (int i = n / 2 - 1; i >= 0; i--) {
        sift_down_max(a, n, i, cmp);    /* 阶段 1：建最大堆，O(n) */
    }
    for (int end = n - 1; end >= 1; end--) {
        swap2(&a[0], &a[end]);          /* 阶段 2：最大值送去未排序段的最右边 */
        sift_down_max(a, end, 0, cmp);  /* 注意堆大小变成 end（尾巴已经排好） */
    }
}

void topk_by_sort(const int *data, int n, int k, int *out, long *cmp_out)
{
    long local_cmp = 0;

    if (k <= 0 || n <= 0) {
        if (cmp_out) {
            *cmp_out = 0;
        }
        return;
    }
    if (k > n) {
        k = n;                          /* 要的数量比总数还多，就全给 */
    }

    /* ⚠ 必须拷贝：不拷贝就会把调用者的数组排成升序（副作用），
     *   这也是"全排序"方案 O(n) 额外空间的来源。 */
    int *work = malloc((size_t)n * sizeof(int));
    if (work == NULL) {
        printf("  [错误] topk_by_sort 拷贝失败，需要 %d 个 int\n", n);
        if (cmp_out) {
            *cmp_out = -1;
        }
        return;
    }
    for (int i = 0; i < n; i++) {
        work[i] = data[i];
    }

    heapsort_asc(work, n, &local_cmp);  /* 排成升序 */

    /* 最大的 K 个就在尾巴上：work[n-K] .. work[n-1]。
     * 从后往前取，out 里自然就是【降序】（最大的在前）。 */
    for (int i = 0; i < k; i++) {
        out[i] = work[n - 1 - i];
    }

    free(work);
    if (cmp_out) {
        *cmp_out = local_cmp;
    }
}

/* =====================================================================
 * 方法②用的数据结构：一个【大小固定为 K 的最小堆】
 * ---------------------------------------------------------------------
 * 和 06-heap/binary-heap 的通用堆相比，这个"瘦版"堆有三点不同：
 *   ① 容量永远等于 K，不需要扩容，也不需要 realloc；
 *   ② 我们只关心堆顶（候选里最差的那个），从不需要 pop 中间的谁；
 *   ③ 有个专用操作 replace_root：把堆顶换成新值后【只下沉】，
 *      因为换进来的新值一定比被踢掉的旧堆顶大（否则根本不会替换），
 *      所以它不可能需要上浮 —— 省掉一半工作量。
 *      （pop + push 会做两次 log K；replace_root 只做一次。）
 *
 * 守门员逻辑（本节最该想通的一段）：
 *   堆里放的是"目前为止见过的最大的 K 个"，堆顶 = 这 K 个里【最小】的 = 门槛。
 *   新数 x 来了：
 *     x > 堆顶  →  x 至少比"门槛"强，把门槛踢出去（x 成为新的候选），门槛以下的老元素们自动保住位置
 *     x <= 堆顶 →  x 连最差的候选都不如，直接扔掉，一次比较就够了
 *   所以全程只用堆顶这一个数做判断 —— O(1) 判断 + 至多一次 O(log K) 替换。
 *   总代价 O(n log K)，空间 O(K)。K=10、n=1000 万时，这就是它能秒杀全排序的原因。
 * ===================================================================== */
typedef struct {
    int  *a;       /* 堆本体，大小永远是 cap（= K） */
    int   size;    /* 已经装了几个（<= cap）；没装满时守门判断还不生效 */
    int   cap;     /* 固定 = K */
    int   verbose; /* 1 = 每步都打印（只在小数组上开） */
    long  cmp;     /* 比较次数 */
} KHeap;

static void kh_dump(const KHeap *h, const char *note)
{
    if (!h->verbose) {
        return;
    }
    printf("      %s: 候选堆[", note);
    for (int i = 0; i < h->size; i++) {
        printf(" %d", h->a[i]);
    }
    printf(" ]");
    if (h->size == h->cap) {
        printf("  满员，门槛(堆顶/最差候选)=%d", h->a[0]);
    } else {
        printf("  还差 %d 个才满员", h->cap - h->size);
    }
    printf("\n");
}

/* 上浮：新元素在末尾，往上爬。最小堆。 */
static void kh_sift_up(KHeap *h, int i)
{
    while (i > 0) {
        int p = (i - 1) / 2;
        h->cmp++;
        if (h->a[i] >= h->a[p]) {
            return;                     /* 不比父亲小 → 停 */
        }
        swap2(&h->a[i], &h->a[p]);
        if (h->verbose) {
            printf("        上浮：下标 %d <-> %d\n", i, p);
        }
        i = p;
    }
}

/* 下沉：i 处太大了，往下走。最小堆，跟【更小的】孩子换。 */
static void kh_sift_down(KHeap *h, int i)
{
    for (;;) {
        int l = 2 * i + 1;
        int r = 2 * i + 2;
        if (l >= h->size) {
            return;
        }
        int small = l;
        if (r < h->size) {
            h->cmp++;
            if (h->a[r] < h->a[l]) {
                small = r;
            }
        }
        h->cmp++;
        if (!(h->a[small] < h->a[i])) {
            return;
        }
        swap2(&h->a[i], &h->a[small]);
        if (h->verbose) {
            printf("        下沉：下标 %d <-> %d\n", i, small);
        }
        i = small;
    }
}

/* 未满员时：放尾巴 + 上浮 */
static void kh_add(KHeap *h, int v)
{
    if (h->size >= h->cap) {
        return;                         /* 满了就该走 replace_root，不该调这个 */
    }
    h->a[h->size] = v;
    h->size++;
    kh_sift_up(h, h->size - 1);
}

/* 满员后：踢掉堆顶，把新值放到堆顶位置，只下沉 */
static void kh_replace_root(KHeap *h, int v)
{
    if (h->verbose) {
        printf("        踢掉门槛 %d，让 %d 站上堆顶，然后下沉归位\n", h->a[0], v);
    }
    h->a[0] = v;
    kh_sift_down(h, 0);
}

void topk_by_heap(const int *data, int n, int k, int *out, long *cmp_out)
{
    if (k <= 0 || n <= 0) {
        if (cmp_out) {
            *cmp_out = 0;
        }
        return;
    }
    if (k > n) {
        k = n;
    }

    KHeap h;
    h.a = malloc((size_t)k * sizeof(int));   /* ⚠ 只花 O(K) 的空间，不是 O(n) */
    if (h.a == NULL) {
        printf("  [错误] topk_by_heap 申请 K=%d 个 int 失败\n", k);
        if (cmp_out) {
            *cmp_out = -1;
        }
        return;
    }
    h.size = 0;
    h.cap = k;
    h.verbose = 0;
    h.cmp = 0;

    for (int i = 0; i < n; i++) {
        int x = data[i];
        if (h.size < h.cap) {
            kh_add(&h, x);              /* 还没装满 → 无脑收 */
        } else {
            h.cmp++;
            if (x > h.a[0]) {           /* 只和堆顶比【一次】 */
                kh_replace_root(&h, x);
            }
            /* 否则 x 连门槛都不如，扔掉，什么也不做 */
        }
    }

    for (int i = 0; i < k; i++) {
        out[i] = h.a[i];                /* 注意：堆里的 K 个【不是】有序的！ */
    }
    free(h.a);                          /* 只有这一块内存，还掉就干净了 */

    if (cmp_out) {
        *cmp_out = h.cmp;
    }
}

void topk_heap_trace(const int *data, int n, int k)
{
    if (k <= 0) {
        return;
    }
    KHeap h;
    h.a = malloc((size_t)k * sizeof(int));
    if (h.a == NULL) {
        return;
    }
    h.size = 0;
    h.cap = k > n ? n : k;
    h.verbose = 1;
    h.cmp = 0;

    for (int i = 0; i < n; i++) {
        int x = data[i];
        printf("    来 %2d | ", x);
        if (h.size < h.cap) {
            printf("堆只有 %d/%d 个候选 → 直接收下\n", h.size, h.cap);
            kh_add(&h, x);
            kh_dump(&h, "收下后");
        } else if (x > h.a[0]) {
            printf("堆满员，门槛=%d，%d > %d → 【有资格】，踢掉 %d\n", h.a[0], x, h.a[0], h.a[0]);
            kh_replace_root(&h, x);
            kh_dump(&h, "替换后");
        } else {
            printf("堆满员，门槛=%d，%d <= %d → 连最差的候选都不如，忽略\n", h.a[0], x, h.a[0]);
        }
    }
    printf("  最终 K 个候选（堆里【不】有序，要排名还得再排，共 O(K log K)）：");
    for (int i = 0; i < h.size; i++) {
        printf(" %d", h.a[i]);
    }
    printf("\n  比较总次数 = %ld\n", h.cmp);
    free(h.a);
}

/* =====================================================================
 * 方法③：快速选择 Quickselect（简版）
 * ---------------------------------------------------------------------
 * 思路：全排序是"把每个位置都排好"，而我们只要知道【第 (n-K) 小】落在哪个值上。
 *   快排每一轮 partition 会把某个 pivot 放到它【最终应在的位置】，
 *   那我们就只做一件事：反复 partition，直到 pivot 正好落在 target = n-K 上。
 *   每次 partition 后只递归【包含 target 的那一半】，另一半扔掉不管。
 *
 *   工作量：n + n/2 + n/4 + ... = 2n = 平均 O(n)（比 O(n log n) 少一个 log）。
 *   代价：① 实现比堆难写对；② 最坏 O(n²)（pivot 老是选在端点）；
 *         ③ 它【必须打乱】工作数组（所以这里还是拷了一份）。
 *
 * 这里只给"够用的简版"：Lomuto 分区 + 三数取中选 pivot + while 循环代替递归
 *   （循环版不会栈溢出，因为每轮只走一半，等价于尾递归）。
 * 完整版和各种边界（重复值、BFPRT 最坏 O(n) 保证）见 11-searching/kth-smallest。
 * ===================================================================== */

/* Lomuto 分区：把 pivot 放到 hi 位置，返回它最终的下标。
 * 不变量：lo..i 全部 <= pivot，i+1..j-1 全部 > pivot，j 是扫描指针。 */
static int partition_lomuto(int *a, int lo, int hi, long *cmp)
{
    /* 三数取中：把 lo / mid / hi 三个位置的中位数挑出来放到 hi 当 pivot。
     * 为什么？固定拿 a[hi] 当 pivot 时，遇到"几乎有序"的数据会退化成 O(n²)
     * （每轮只切掉一个元素）。三数取中是很便宜的防退化手段。 */
    int mid = lo + (hi - lo) / 2;
    (*cmp)++;
    if (a[lo] > a[mid]) {
        swap2(&a[lo], &a[mid]);
    }
    (*cmp)++;
    if (a[lo] > a[hi]) {
        swap2(&a[lo], &a[hi]);
    }
    (*cmp)++;
    if (a[mid] > a[hi]) {
        swap2(&a[mid], &a[hi]);
    }
    swap2(&a[mid], &a[hi]);             /* 中位数放到 hi，成为 pivot */

    int pivot = a[hi];
    int i = lo;                         /* i：下一个"该放左边"的位置 */
    for (int j = lo; j < hi; j++) {
        (*cmp)++;
        if (a[j] <= pivot) {
            swap2(&a[i], &a[j]);
            i++;
        }
    }
    swap2(&a[i], &a[hi]);               /* pivot 归位 */
    return i;
}

/* 把 a[lo..hi] 里第 target 小的元素放到 a[target]（其他位置乱序，但左边都 <= 它，右边都 >= 它） */
static void quickselect(int *a, int lo, int hi, int target, long *cmp)
{
    while (lo < hi) {
        int p = partition_lomuto(a, lo, hi, cmp);
        if (p == target) {
            return;                     /* 正好落在该在的位置，收工 */
        }
        if (target < p) {
            hi = p - 1;                 /* 只走左边，右边整个扔掉不排 */
        } else {
            lo = p + 1;
        }
    }
}

void topk_by_quickselect(const int *data, int n, int k, int *out, long *cmp_out)
{
    if (k <= 0 || n <= 0) {
        if (cmp_out) {
            *cmp_out = 0;
        }
        return;
    }
    if (k > n) {
        k = n;
    }

    int *work = malloc((size_t)n * sizeof(int));
    if (work == NULL) {
        printf("  [错误] topk_by_quickselect 拷贝失败\n");
        if (cmp_out) {
            *cmp_out = -1;
        }
        return;
    }
    for (int i = 0; i < n; i++) {
        work[i] = data[i];
    }

    long local_cmp = 0;
    /* 升序视角下，"最大的 K 个"的下界（门槛值）就是第 (n-K) 小的那个元素。
     * target = n - K：注意如果 K == n，target = 0，门槛就是最小值，全部入选。 */
    int target = n - k;
    quickselect(work, 0, n - 1, target, &local_cmp);
    int threshold = work[target];       /* 划分线：>= 它的就是候选 */

    /* 收集：先收 strictly greater 的，再用等于 threshold 的补齐到 K 个。
     * ⚠ 这一步是快选法最容易出错的地方：有重复值时
     *   "所有 >= threshold 的元素"可能【多于】K 个，必须补齐/截断，
     *   否则结果和另外两种方法对不上（README 的坑 3 专门讲这个）。 */
    int filled = 0;
    for (int i = 0; i < n && filled < k; i++) {
        if (work[i] > threshold) {
            out[filled++] = work[i];
        }
    }
    for (int i = 0; i < n && filled < k; i++) {
        if (work[i] == threshold) {
            out[filled++] = work[i];
        }
    }

    free(work);
    if (cmp_out) {
        *cmp_out = local_cmp;
    }
}

/* =====================================================================
 * 把快速选择的"每一轮砍掉多少"打出来 —— 用来验证"平均 O(n)"到底有多平均
 * ---------------------------------------------------------------------
 * 为什么要这个函数？因为"平均 O(n)"这句话里藏着一个前提：pivot 每次恰好
 * 把区间切成对半。可 pivot 是数据决定的，不是我们能挑的。换一份输入，
 * 每轮砍掉的比例可能惨不忍睹（本文件第 7 幕的实测就是活例子）。
 * 所以这里把每轮的区间长度、pivot 落在区间的百分之几、往哪边切，全打出来。
 * ===================================================================== */
void topk_quickselect_trace(const int *data, int n, int k)
{
    if (k <= 0 || k > n || n < 2) {
        printf("  （这个追踪只演示 2 <= K <= n 的情况，本次跳过）\n");
        return;
    }
    int *work = malloc((size_t)n * sizeof(int));
    if (work == NULL) {
        printf("  [错误] topk_quickselect_trace 拷贝失败\n");
        return;
    }
    for (int i = 0; i < n; i++) {
        work[i] = data[i];
    }

    int target = n - k;
    int lo = 0, hi = n - 1;
    long cmp = 0;
    int round = 0;
    int printed = 0;

    printf("    目标：把第 %d 小的元素放到下标 %d（= 最大的 %d 个的下界）\n", target, target, k);
    while (lo < hi) {
        int p = partition_lomuto(work, lo, hi, &cmp);
        int len = hi - lo + 1;
        round++;
        /* 只列前 6 轮 + 命中的那一轮，否则大数组会刷屏 */
        int show = (printed < 6) || (p == target);
        if (show) {
            printf("    第 %2d 轮：区间长 %6d，pivot 落在区间内 %5.1f%% 处（下标 %d）",
                   round, len, 100.0 * (double)(p - lo) / (double)(len - 1), p);
            printed++;
        }
        if (p == target) {
            if (show) {
                printf(" → 正中目标，收工\n");
            }
            break;
        }
        if (target < p) {
            if (show) {
                printf(" → 目标在【左边】，扔掉右边 %d 个\n", hi - p);
            }
            hi = p - 1;
        } else {
            if (show) {
                printf(" → 目标在【右边】，扔掉左边 %d 个\n", p - lo + 1);
            }
            lo = p + 1;
        }
    }
    if (round > printed) {
        printf("    …一共切了 %d 轮才命中（上面只列前 %d 轮）\n", round, printed);
    }
    printf("    轮数 %d，比较次数 %ld（%.2f n）\n",
           round, cmp, (double)cmp / (double)n);
    free(work);
}

/* =====================================================================
 * 归一化：把 out 里的 K 个数排成【降序】
 * =====================================================================
 * 为什么需要这个函数？因为三种算法给出的【顺序】互不相同：
 *   ① 全排序：天然降序（从尾巴倒着取）
 *   ② K 最小堆：堆数组顺序 —— 完全不代表名次！堆只保证堆顶最小
 *   ③ 快选：先收 > 门槛的（原数组顺序），再收 == 门槛的 —— 又是一套顺序
 * 所以要比较"三者结果是否一致"，必须先各自降序排一遍再逐个对比。
 * 这就是 Top-K 的一个真实坑：**Top-K ≠ 前 K 名排行榜**。
 * 要排行榜就必须对这 K 个再排一次（O(K log K)，因为 K 很小，可以接受）。
 *
 * 这里用最简单的插入排序（K 通常很小，代码短、无额外内存）。
 */
void topk_normalize_desc(int *out, int k)
{
    for (int i = 1; i < k; i++) {
        int key = out[i];
        int j = i - 1;
        while (j >= 0 && out[j] < key) {   /* 降序：比 key 小的往后挪 */
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = key;
    }
}
