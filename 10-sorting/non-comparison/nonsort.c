/*
 * nonsort.c —— 计数 / 基数 / 桶：三个"不问谁比谁大"的排序
 *
 * 阅读顺序建议：
 *   1) arr_min_max + range_too_big：非比较排序的【前提检查】（值域就是它们的命门）；
 *   2) counting_sort_expand → counting_sort_stable：同一件事的两种写法，差别就在"稳不稳定"；
 *   3) radix_sort_lsd：把上面的稳定计数当子程序，一轮一位；
 *   4) bucket_sort：唯一需要"数据均匀"这个假设的一个；
 *   5) 最后的 merge_sort_ref / insertion_sort_ref 是给第 6 幕当 O(n log n) 对照的参照实现。
 */

#include <stdio.h>
#include <stdlib.h>
#include "nonsort.h"

/* 值域上限：count[] 这种辅助数组最多允许开多少格。
 *
 * 为什么必须有这个防护？把数字摊开：
 *     n = 10 万个 int 待排            ⇒ 数据本身只有 400 KB
 *     值是 32 位整数 ⇒ 值域 k = 2^32 ⇒ count[] 要 2^32 * 4 字节 = 17,179,869,184 字节 = 16 GB
 *   为了排 400 KB 的数据去开 16 GB —— 这不是"慢一点"，这是【根本不可能】。
 *   所以真实实现必须在动手前先看一眼 k，超了就直接拒绝（返回 -1），把决定权交回调用方：
 *   该换基数（只看位数）还是该换快排（不看值域），是调用方的选择，不是这里的。
 */
static int range_too_big(int n, long k)
{
    long limit = (long)n * 8;             /* 辅助数组最多是数据本身的 8 倍大 */
    if (limit < (1L << 20)) {
        limit = (1L << 20);               /* 但再小也给到 1M 格（4 MB）：小数据别乱拒绝 */
    }
    return k > limit;
}

/* 十进制位数（用来告诉读者"基数要跑几轮"）*/
static int num_digits(int v)
{
    int d = 1;
    while (v >= 10) {
        v /= 10;
        d++;
    }
    return d;
}

/* =====================================================================
 * 极值与验货
 * ===================================================================== */

void arr_min_max(const int a[], int n, int *min_out, int *max_out)
{
    if (n <= 0) {
        *min_out = 0;
        *max_out = -1;   /* 空区间：min > max，后续算值域会直接 <= 0，方便判断 */
        return;
    }
    int mn = a[0];
    int mx = a[0];
    for (int i = 1; i < n; i++) {
        if (a[i] < mn) {
            mn = a[i];
        } else if (a[i] > mx) {      /* 用 else if：比 mn 小的数不可能同时比 mx 大，省一次比较 */
            mx = a[i];
        }
    }
    *min_out = mn;
    *max_out = mx;
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

/* 两个数组是不是"同一批数"（顺序无关）。
 * 值域窄 ⇒ 直方图 O(n + k)；值域宽 ⇒ 退回 O(n²) 数个数。
 * 为什么不能图省事直接"排序后逐个比"？因为被测对象就是排序算法本身 ——
 * 拿被测算法当标准答案，这个检查就毫无意义了。 */
int same_multiset(const int a[], const int b[], int n)
{
    if (n <= 0) {
        return 1;
    }
    int amin, amax, bmin, bmax;
    arr_min_max(a, n, &amin, &amax);
    arr_min_max(b, n, &bmin, &bmax);
    if (amin != bmin || amax != bmax) {
        return 0;   /* 最小值/最大值都不一样 ⇒ 一定不是同一批数，先免费排除掉一大半 */
    }
    long k = (long)amax - amin + 1;
    if (k <= 4L * n + 1024) {
        int *h = calloc((size_t)k, sizeof(int));
        if (h == NULL) {
            printf("      [警告] same_multiset 直方图申请失败\n");
            return 0;
        }
        for (int i = 0; i < n; i++) {
            h[(int)((long)a[i] - amin)]++;
        }
        for (int i = 0; i < n; i++) {
            int idx = (int)((long)b[i] - amin);   /* 注意先转 long 再减：见 README 坑 ② */
            if (--h[idx] < 0) {
                free(h);
                return 0;   /* b 里这个值多出来一个 ⇒ 不是同一批 */
            }
        }
        free(h);
        return 1;
    }
    /* 值域宽到不划算开直方图 ⇒ 老老实实 O(n²) 数个数（只在演示里用，n 很小） */
    for (int i = 0; i < n; i++) {
        int ca = 0;
        int cb = 0;
        for (int j = 0; j < n; j++) {
            if (a[j] == a[i]) ca++;
            if (b[j] == a[i]) cb++;
        }
        if (ca != cb) {
            return 0;
        }
    }
    return 1;
}

/* =====================================================================
 * 计数排序 · 展开版
 * =====================================================================
 * 思路（大白话）：值域就这么点，那干脆【按号占座】——
 *   ① 开一个 count[]，下标就代表"这个值"，count[v] 记它出现了几次；
 *   ② 扫一遍数组把计数填上；
 *   ③ 从最小的值开始，"值 v 出现 c 次就往回写 c 个 v"。
 * 全程【一次都没有比较过两个元素谁大】，只是数了数、写了几笔。
 *
 * 为什么必须先减去最小值（带负数的输入）？
 *   count[] 的下标是数组下标，只能是 0..k-1 的非负整数。
 *   直接 cnt[a[i]]++，遇到 a[i] = -5 就是写 cnt[-5] —— 越界、UB、当场乱套。
 *   平移一下：v' = v - min，则 v' 必然 ∈ [0, max-min]，写回时 a[i] = v' + min。
 *   第 1 幕会喂一组带负数的数据，把这件事演出来。
 *
 * 为什么不稳定？
 *   第 ③ 步写回去的是【值本身】，不是"原来那个元素"。同分的两个人在 count[] 里
 *   被合并成了一个数字 c，谁先谁后的信息【已经丢了】，再想还原只能靠额外结构。
 *   （下面的 tagged 版本会给出一个真实会写出来的不稳定实现，看第 3 幕输出。）
 * ===================================================================== */
int counting_sort_expand(int a[], int n, int verbose, NSStat *st)
{
    if (n <= 1) {
        return 1;
    }
    int mn, mx;
    arr_min_max(a, n, &mn, &mx);
    long k = (long)mx - (long)mn + 1;
    if (st != NULL) {
        st->cmp = 0;
        st->move = 0;
        st->aux_cells = 0;
    }
    if (range_too_big(n, k)) {
        printf("      [拒绝] 值域 k = %ld 太大：n = %d，count[] 要 %ld 格 = %.1f MB，"
               "超过数据本身 8 倍 ⇒ 该换基数排序或快排\n",
               k, n, k, (double)k * 4.0 / 1048576.0);
        return -1;
    }
    int *cnt = calloc((size_t)k, sizeof(int));
    if (cnt == NULL) {
        return 0;
    }
    if (st != NULL) {
        st->aux_cells += k;
    }
    /* ① 数数：注意 (long)a[i] - mn —— 先转 long 再减，防 a[i] - INT_MIN 溢出 */
    for (int i = 0; i < n; i++) {
        cnt[(int)((long)a[i] - (long)mn)]++;
    }
    if (verbose) {
        printf("      count[] 长这样（下标 = 值 - 最小值 %d）：\n", mn);
        for (long v = 0; v < k; v++) {
            if (cnt[v] > 0) {
                printf("        值 %3d 出现 %2d 次   （count[%ld] = %d）\n",
                       (int)(v + mn), cnt[v], v, cnt[v]);
            }
        }
    }
    /* ② 展开：把"出现 c 次"翻译成"往回写 c 个" */
    int idx = 0;
    for (long v = 0; v < k; v++) {
        for (int c = 0; c < cnt[v]; c++) {
            a[idx++] = (int)(v + mn);
            if (st != NULL) st->move++;
        }
    }
    free(cnt);
    return 1;
}

/* =====================================================================
 * 计数排序 · 前缀和版（稳定）
 * =====================================================================
 * 关键差别只有一句话：【先算出"值 ≤ v 一共有几个"，再【从后往前】扫原数组放人】。
 *
 *   cnt[v] 变成前缀和之后 = "最终数组里，值 ≤ v 的元素个数" = 值 v 这一段的右边界。
 *   从后往前遍历原数组：i 从 n-1 到 0，元素 a[i] 放到 --cnt[key(a[i])] 的位置上。
 *
 * 为什么"从后往前"就是稳定的？（这是本节最值得想清楚 30 秒的地方）
 *   两个同分的元素 x（在前）、y（在后）。从后往前先碰到 y，它拿走这段的【最后一个】格子；
 *   再碰到 x，它拿走前一个格子 ⇒ 排完 x 依然在 y 左边 ⇒ 原始顺序保住了。
 *   反过来若从前往后放，先碰到的 x 会拿第一个格子、y 拿第二个 —— 听着也保序？
 *   问题在于"拿第一个格子"要写成 `cnt[key]++`，而 cnt[key] 此时是【右边界】不是左边界，
 *   还要额外再算一遍起点。历史选择就是"前缀和 + 从后往前"这一套，背下来即可。
 *
 * 稳定性在这里【白送】，代价是一点都【没有多花】：还是 O(n + k) 时间、O(n + k) 空间。
 * 所以工程里的计数排序基本都是这一版。
 * ===================================================================== */
int counting_sort_stable(int a[], int n, int verbose, NSStat *st)
{
    if (n <= 1) {
        return 1;
    }
    int mn, mx;
    arr_min_max(a, n, &mn, &mx);
    long k = (long)mx - (long)mn + 1;
    if (st != NULL) {
        st->cmp = 0;
        st->move = 0;
        st->aux_cells = 0;
    }
    if (range_too_big(n, k)) {
        printf("      [拒绝] 值域 k = %ld 太大（n = %d）⇒ 计数排序不适用\n", k, n);
        return -1;
    }
    int *cnt = calloc((size_t)k, sizeof(int));
    int *tmp = malloc((size_t)n * sizeof(int));
    if (cnt == NULL || tmp == NULL) {
        free(cnt);
        free(tmp);
        return 0;
    }
    if (st != NULL) {
        st->aux_cells += k + n;
    }
    for (int i = 0; i < n; i++) {
        cnt[(int)((long)a[i] - (long)mn)]++;
    }
    /* 前缀和：cnt[v] = 值 ≤ v 的元素个数 = 值 v 那一段的右边界 */
    for (long v = 1; v < k; v++) {
        cnt[(int)v] += cnt[(int)v - 1];
    }
    if (verbose) {
        printf("      计数完 → 前缀和之后（cnt[v] = 值 ≤ %d+v 的共有几个 = 这一段的右边界）：\n", mn);
        for (long v = 0; v < k; v++) {
            printf("        v=%2ld 值=%3d  cnt(前缀和)=%2d\n", v, (int)(v + mn), cnt[(int)v]);
        }
        printf("      从后往前放人（同分的后来者先占【右边】那格 ⇒ 原始顺序被保住）：\n");
    }
    for (int i = n - 1; i >= 0; i--) {
        int key = (int)((long)a[i] - (long)mn);
        int pos = --cnt[key];
        tmp[pos] = a[i];
        if (st != NULL) st->move++;
        if (verbose) {
            printf("        a[%d]=%d（值 %d 这一段的第 %d 格）→ 放到下标 %d\n", i, a[i], (int)(key + mn),
                   cnt[key], pos);
        }
    }
    for (int i = 0; i < n; i++) {
        a[i] = tmp[i];
    }
    if (st != NULL) {
        st->move += n;
    }
    free(cnt);
    free(tmp);
    return 1;
}

/* =====================================================================
 * 基数排序 LSD
 * =====================================================================
 * 思路（大白话）：计数排序卡在值域 k 上 —— 值能到 20 亿，count[] 就开不出来。
 *   但【一位十进制数】只有 0..9 十个取值！于是把一个多位数【按位拆开】看：
 *   先只看个位排一遍（值域 10，计数排序毫无压力）→ 再只看十位排一遍 → 百位 → …
 *   排完最高位，整个数组就有序了。
 *
 * 为什么必须从【最低位】开始（LSD = Least Significant Digit first）？
 *   因为每一轮排序都会把上一轮的成果"重新分组"。只有从低位往高位，
 *   才能保证"最后一轮结束时，高位决定整体顺序、低位已经被高位覆盖过"。
 *   反过来说：排完最高位之后再排最低位 ⇒ 个位把按十位分好的组全打乱了，前功尽弃。
 *   想从高位开始也行（叫 MSD），但那要求"高位相同的内部再按低位递归排"，
 *   实现复杂得多，本节不做。
 *
 * 为什么每一轮【必须用稳定】的子排序？
 *   第 2 轮按十位排的时候，个位的顺序必须【不被破坏】——
 *   只有稳定排序才承诺"这一轮比较判为相等的元素，保持上一轮的先后顺序"。
 *   十位相同的那些元素，靠的就是上一轮个位排好的顺序 ⇒ 一旦某轮不稳定，全部白费。
 *   这正是"计数排序前缀和版"在这里的用途。
 *
 * 负数怎么办？基数排序只能对"非负的位序列"比大小，所以先把整批数【整体平移】
 *   成非负（减去最小值），排完再加回去。平移不改变相对顺序 ⇒ 合法。
 *   （前提：mx - mn 不能超过 INT_MAX，否则平移本身就溢出了 —— 代码里检查并拒绝。）
 * ===================================================================== */
int radix_sort_lsd(int a[], int n, int verbose, NSStat *st)
{
    if (n <= 1) {
        return 1;
    }
    int mn, mx;
    arr_min_max(a, n, &mn, &mx);
    if (st != NULL) {
        st->cmp = 0;
        st->move = 0;
        st->aux_cells = 0;
    }
    long span = (long)mx - (long)mn;
    if (span > 2147483647L) {
        printf("      [拒绝] 值域跨度 %ld 超过 int 能表示的范围，平移本身就会溢出 ⇒ 请用快排\n", span);
        return -1;
    }
    int *work = malloc((size_t)n * sizeof(int));
    int *tmp = malloc((size_t)n * sizeof(int));
    if (work == NULL || tmp == NULL) {
        free(work);
        free(tmp);
        return 0;
    }
    if (st != NULL) {
        st->aux_cells += 2L * n;
    }
    for (int i = 0; i < n; i++) {
        work[i] = (int)((long)a[i] - (long)mn);   /* 平移成非负 */
    }
    int maxw = (int)span;
    if (verbose) {
        printf("      平移（每个数减去最小值 %d）之后变成非负，最大 %d ⇒ 十进制 %d 位 ⇒ 要跑 %d 轮：\n",
               mn, maxw, num_digits(maxw), num_digits(maxw));
        print_arr("      ", work, n);
    }
    /* 一轮 = 按某一位做【一次稳定的】计数排序。cnt 只有 10 格，永远不用怕值域爆炸。*/
    int rounds = 0;
    long exp = 1;
    while (1) {
        int cnt[10];
        for (int d = 0; d < 10; d++) {
            cnt[d] = 0;
        }
        for (int i = 0; i < n; i++) {
            cnt[(int)((work[i] / exp) % 10)]++;     /* 数这一位上 0..9 各有几个 */
        }
        for (int d = 1; d < 10; d++) {
            cnt[d] += cnt[d - 1];                   /* 前缀和 ⇒ 右边界（稳定写法）*/
        }
        for (int i = n - 1; i >= 0; i--) {          /* 从后往前 ⇒ 这一轮是稳定的 */
            int d = (int)((work[i] / exp) % 10);
            tmp[--cnt[d]] = work[i];
            if (st != NULL) st->move += 2;
        }
        for (int i = 0; i < n; i++) {
            work[i] = tmp[i];
        }
        rounds++;
        if (verbose) {
            printf("      第 %d 轮：按 %s 位（exp=%ld）稳定计数排序之后：\n", rounds,
                   (exp == 1) ? "个" : (exp == 10) ? "十" : (exp == 100) ? "百" :
                   (exp == 1000) ? "千" : "更高", exp);
            print_arr("        ", work, n);
        }
        if (exp > (long)maxw / 10) {
            break;   /* 再往上的位全是 0，排了也不会有变化（顺便防住 exp *= 10 溢出）*/
        }
        exp *= 10;
    }
    for (int i = 0; i < n; i++) {
        a[i] = (int)((long)work[i] + (long)mn);     /* 平移回来 */
    }
    if (st != NULL) {
        st->move += n;
    }
    free(work);
    free(tmp);
    (void)rounds;
    return 1;
}

/* =====================================================================
 * 桶排序
 * =====================================================================
 * 思路（大白话）：知道数据在 [min, max] 上【均匀分布】，那就把值域切成 nb 段（桶），
 *   每个元素按值直接算出该进哪个桶 → 桶内随便用小排序（这里用插入排序）→
 *   按桶号从小到大把所有桶倒回数组。
 *
 * 为什么"均匀分布"是前提？
 *   元素 = (v - min) / 桶宽。均匀 ⇒ 每个桶约 n/nb 个 ⇒ 桶内插入排序总代价
 *   nb * (n/nb)² = n²/nb，取 nb ≈ n 就是 O(n)。
 *   一旦数据全挤在一小块（比如全班成绩都在 88 分附近），所有元素落进同一个桶 ⇒
 *   那一个桶要做 n 个元素的插入排序 = O(n²)，比快排还慢，还额外赔上桶的开销。
 *   第 5 幕把均匀 / 全挤一块两种情况各跑一遍，每桶大小都打出来。
 *
 * 和计数的区别：计数排序是"每个【取值】一个格子"，桶是"每【段值域】一个袋子"，
 *   袋子里能装任意多个 ⇒ 桶能处理值域很大的数据（只要均匀），计数不能。
 *   和基数的区别：基数不需要分布假设（它只看位数），桶需要。
 * ===================================================================== */

/* 一个桶：动态增长 int 数组。为什么不用链表？插入排序要随机访问，数组更顺手，
   也少一堆 malloc/free；代价是要 realloc（下面 free 干净，ASan 盯着）。 */
typedef struct {
    int *v;
    int size;
    int cap;
} Bucket;

static int bucket_push(Bucket *b, int value)
{
    if (b->size >= b->cap) {
        int newcap = (b->cap == 0) ? 4 : b->cap * 2;
        int *p = realloc(b->v, (size_t)newcap * sizeof(int));
        if (p == NULL) {
            return 0;
        }
        b->v = p;
        b->cap = newcap;
    }
    b->v[b->size++] = value;
    return 1;
}

int bucket_sort(int a[], int n, int bucket_count, int verbose, NSStat *st)
{
    if (n <= 1) {
        return 1;
    }
    if (bucket_count < 1) {
        bucket_count = 1;
    }
    int mn, mx;
    arr_min_max(a, n, &mn, &mx);
    if (st != NULL) {
        st->cmp = 0;
        st->move = 0;
        st->aux_cells = 0;
    }
    Bucket *bs = calloc((size_t)bucket_count, sizeof(Bucket));
    if (bs == NULL) {
        return 0;
    }
    if (st != NULL) {
        st->aux_cells += 3L * bucket_count;   /* 每个桶就是一个 {指针, size, cap} = 3 个 int 大 */
    }
    long span = (long)mx - (long)mn + 1;
    int fit_ok = 1;
    for (int i = 0; i < n; i++) {
        long offset = (long)a[i] - (long)mn;                 /* 0 .. span-1 */
        int idx = (int)((offset * bucket_count) / span);     /* 均匀分布 ⇒ 各桶约 n/nb 个 */
        if (idx >= bucket_count) {
            idx = bucket_count - 1;                          /* 理论上不会发生，防一手除法取整 */
        }
        if (!bucket_push(&bs[idx], a[i])) {
            fit_ok = 0;
            break;
        }
        if (st != NULL) st->move++;
    }
    if (!fit_ok) {
        for (int b = 0; b < bucket_count; b++) {
            free(bs[b].v);
        }
        free(bs);
        return 0;
    }
    if (verbose) {
        printf("      每桶大小（值域 [%d, %d] 切成 %d 桶）：\n        ", mn, mx, bucket_count);
        for (int b = 0; b < bucket_count; b++) {
            printf("%d ", bs[b].size);
        }
        printf("\n");
    }
    /* 桶内：插入排序（元素少的时候它就是最快的那个，见 ../comparison-basic/） */
    for (int b = 0; b < bucket_count; b++) {
        if (st != NULL) {
            st->aux_cells += bs[b].cap;
        }
        int m = bs[b].size;
        for (int i = 1; i < m; i++) {
            int key = bs[b].v[i];
            int j = i - 1;
            while (j >= 0) {
                if (st != NULL) st->cmp++;
                if (bs[b].v[j] <= key) {
                    break;
                }
                bs[b].v[j + 1] = bs[b].v[j];
                if (st != NULL) st->move++;
                j--;
            }
            bs[b].v[j + 1] = key;
            if (st != NULL) st->move++;
        }
    }
    /* 收集：按桶号从小到大倒回 a */
    int idx = 0;
    for (int b = 0; b < bucket_count; b++) {
        for (int i = 0; i < bs[b].size; i++) {
            a[idx++] = bs[b].v[i];
            if (st != NULL) st->move++;
        }
        free(bs[b].v);      /* 每个桶自己 v 都要释放，否则 ASan 立刻报泄漏 */
        bs[b].v = NULL;
    }
    free(bs);
    return 1;
}

/* =====================================================================
 * 稳定性实验：分数 + 身份证
 * =====================================================================
 * 展开版和分堆版"不稳定"在实现上到底长在哪一行？看 expand_tagged：
 *   为了给每个值维护"它的那些 tag"，最省事的写法就是【边扫边压栈】，
 *   而栈天生后进先出 ⇒ 同一摞分数吐出来时顺序整个反过来。
 *   这不是为了演示故意写坏 —— 只要你的实现"按值分堆、不记录原始次序"，就必然踩到。
 * 稳定版（前缀和 + 从后往前）不需要栈：后来者拿走这一段的最右格 ⇒ 天然保序。
 * ===================================================================== */

void print_tagged(const char *label, const int key[], const int tag[], int n)
{
    printf("%s[", label);
    for (int i = 0; i < n; i++) {
        printf(" %d%c", key[i], (char)('a' + tag[i]));
    }
    printf(" ]\n");
}

int counting_expand_tagged(int key[], int tag[], int n)
{
    if (n <= 1) {
        return 1;
    }
    int mn, mx;
    arr_min_max(key, n, &mn, &mx);
    long k = (long)mx - (long)mn + 1;
    if (range_too_big(n, k)) {
        return -1;
    }
    int *cnt = calloc((size_t)k, sizeof(int));
    int *start = calloc((size_t)k + 1, sizeof(int));
    int *top = malloc((size_t)k * sizeof(int));
    int *pool = malloc((size_t)n * sizeof(int));
    if (cnt == NULL || start == NULL || top == NULL || pool == NULL) {
        free(cnt); free(start); free(top); free(pool);
        return 0;
    }
    for (int i = 0; i < n; i++) {
        cnt[(int)((long)key[i] - mn)]++;
    }
    int acc = 0;
    for (long v = 0; v < k; v++) {
        start[v] = acc;
        top[v] = acc;                 /* 每个值在自己的区段里当栈用 */
        acc += cnt[(int)v];
    }
    for (int i = 0; i < n; i++) {
        int v = (int)((long)key[i] - mn);
        pool[top[v]++] = tag[i];      /* ← 正序压栈 */
    }
    int idx = 0;
    for (long v = 0; v < k; v++) {
        while (top[v] > start[(int)v]) {
            key[idx] = (int)(v + mn);
            tag[idx] = pool[--top[v]];  /* ← 逆序弹出：不稳定就生在这一行 */
            idx++;
        }
    }
    free(cnt); free(start); free(top); free(pool);
    return 1;
}

int counting_stable_tagged(int key[], int tag[], int n)
{
    if (n <= 1) {
        return 1;
    }
    int mn, mx;
    arr_min_max(key, n, &mn, &mx);
    long k = (long)mx - (long)mn + 1;
    if (range_too_big(n, k)) {
        return -1;
    }
    int *cnt = calloc((size_t)k, sizeof(int));
    int *tk = malloc((size_t)n * sizeof(int));
    int *kk = malloc((size_t)n * sizeof(int));
    if (cnt == NULL || tk == NULL || kk == NULL) {
        free(cnt); free(tk); free(kk);
        return 0;
    }
    for (int i = 0; i < n; i++) {
        cnt[(int)((long)key[i] - mn)]++;
    }
    for (long v = 1; v < k; v++) {
        cnt[(int)v] += cnt[(int)v - 1];
    }
    for (int i = n - 1; i >= 0; i--) {          /* 从后往前 ⇒ 稳定 */
        int v = (int)((long)key[i] - mn);
        int pos = --cnt[v];
        kk[pos] = key[i];
        tk[pos] = tag[i];
    }
    for (int i = 0; i < n; i++) {
        key[i] = kk[i];
        tag[i] = tk[i];
    }
    free(cnt); free(tk); free(kk);
    return 1;
}

/* =====================================================================
 * 参照实现：归并 + 插入
 * =====================================================================
 * 为什么要自带？因为"非比较排序比比较排序快多少"必须有一个【同为 O(n log n)】的
 * 参照物才有意义，而 Makefile 按目录独立编译，不能 #include ../merge-sort/。
 * 于是这里重写一份 30 行归并 + 一份 10 行插入，第 6 幕用真实毫秒数说话。
 * ===================================================================== */

static void merge_range(int a[], int tmp[], int lo, int mid, int hi, long *cmp)
{
    for (int i = lo; i <= hi; i++) {
        tmp[i] = a[i];
    }
    int i = lo;
    int j = mid + 1;
    for (int k = lo; k <= hi; k++) {
        if (i > mid) {
            a[k] = tmp[j++];
        } else if (j > hi) {
            a[k] = tmp[i++];
        } else {
            if (cmp != NULL) (*cmp)++;
            if (tmp[i] <= tmp[j]) {     /* ← 这个 <= 就是稳定性的全部来源 */
                a[k] = tmp[i++];
            } else {
                a[k] = tmp[j++];
            }
        }
    }
}

static void merge_ref_range(int a[], int tmp[], int lo, int hi, long *cmp)
{
    if (lo >= hi) {
        return;
    }
    int mid = lo + (hi - lo) / 2;
    merge_ref_range(a, tmp, lo, mid, cmp);
    merge_ref_range(a, tmp, mid + 1, hi, cmp);
    /* 已经有序就不必合并（小优化，不影响复杂度） */
    if (a[mid] <= a[mid + 1]) {
        return;
    }
    merge_range(a, tmp, lo, mid, hi, cmp);
}

void merge_sort_ref(int a[], int n, long *cmp_out)
{
    if (a == NULL || n < 2) {
        return;
    }
    if (cmp_out != NULL) {
        *cmp_out = 0;
    }
    int *tmp = malloc((size_t)n * sizeof(int));
    if (tmp == NULL) {
        return;
    }
    merge_ref_range(a, tmp, 0, n - 1, cmp_out);
    free(tmp);
}

void insertion_sort_ref(int a[], int n, long *cmp_out)
{
    if (a == NULL || n < 2) {
        return;
    }
    for (int i = 1; i < n; i++) {
        int key = a[i];
        int j = i - 1;
        while (j >= 0) {
            if (cmp_out != NULL) (*cmp_out)++;
            if (a[j] <= key) {
                break;
            }
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;
    }
}
