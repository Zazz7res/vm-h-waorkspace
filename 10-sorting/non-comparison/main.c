/*
 * main.c —— 计数 / 基数 / 桶：分幕演示
 *
 * 数据生成全部用【固定种子的 LCG】，不用 rand()：
 *   ① 保证 README 里贴的每一行输出、每一个毫秒数，别人重跑也能对上；
 *   ② 需要"正确答案"时可以用直方图/升序检查严格验，而不是"看起来对了"。
 * 数组长度一律 #define：C11 里 `const int n = 8; int a[n] = {…};` 编译不过（VLA 不许带初始化器）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "nonsort.h"

#define DEMO_N 8            /* 第 1~4 幕手工演示用的长度 */
#define TINY_N 5
#define BUCKET_N 2000       /* 第 5 幕桶排序：均匀 vs 全挤一块 */
#define BUCKET_CNT 10
#define BIG_N 20000         /* 第 6 幕实测对比表的规模 */
#define VAL_RANGE 1000      /* BIG_N 这批数据的值域：0..999 */

static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

static void copy_arr(int dst[], const int src[], int n)
{
    for (int i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

/* 固定种子的线性同余发生器：生成 [lo, hi] 区间内的整数。
 * 为什么自己写而不用 rand()：rand() 的实现随 libc 变，输出就没法复核了。 */
static void fill_range(int *a, int n, int lo, int hi, unsigned seed)
{
    unsigned s = seed;
    long span = (long)hi - (long)lo + 1;
    for (int i = 0; i < n; i++) {
        s = s * 1103515245u + 12345u;
        a[i] = lo + (int)(((s >> 8) % 100000u) % (unsigned long)span);
    }
}

/*
 * 自己的 log2：仓库的编译参数里没有 -lm，用 <math.h> 的 log2 在小配置上会链接失败，
 * 所以下面用纯四则运算手搓一个（精度 24 位二进制，对"算下界大概多少"完全够用）。
 *
 * 原理：v = 2^e * m，m ∈ [1,2) ⇒ log2(v) = e + log2(m)。
 * 求 log2(m) 用"反复平方取位"：m^2 若 ≥ 2，说明这一位是 1（并且 m^2/2 继续）；否则是 0。
 *   证：log2(m) = 0.b1b2b3… ⇒ m^2 = 2^b1 * (2^(0.b2b3…))，所以 m^2 ≥ 2 恰好等价于 b1 = 1。
 */
static double my_log2_int(int v)
{
    if (v <= 0) {
        return 0.0;
    }
    int e = 0;
    int t = v;
    while (t >= 2) {          /* 数一下二进制位数 ⇒ 整数部分 */
        t >>= 1;
        e++;
    }
    double m = (double)v;
    for (int k = 0; k < e; k++) {
        m *= 0.5;             /* 缩到 [1,2) */
    }
    double frac = 0.0;
    double bit = 0.5;
    for (int k = 0; k < 24; k++) {
        m = m * m;
        if (m >= 2.0) {
            m *= 0.5;
            frac += bit;
        }
        bit *= 0.5;
    }
    return (double)e + frac;
}

/* log2(n!) = Σ log2(i)：比较排序下界的真实数值
   （决策树至少要 n! 个叶子 ⇒ 从根到最深叶子的路径 ≥ log2(n!) 次比较）*/
static double log2_factorial(int n)
{
    double acc = 0.0;
    for (int i = 2; i <= n; i++) {
        acc += my_log2_int(i);
    }
    return acc;
}

int main(void)
{
    const int ORIG[DEMO_N]   = {-5, 3, -1, 3, 0, -5, 2, 1};   /* 带负数 + 带重复，专门用来打脸 */
    const int EXPECT[DEMO_N] = {-5, -5, -1, 0, 1, 2, 3, 3};   /* 手算标准答案 */

    int work[DEMO_N];
    NSStat st;
    int all_pass = 1;

    /* ============================================================
       第 1 幕：计数排序的两个版本
       ============================================================ */
    section("1. 计数排序：展开版 vs 前缀和版（同一件事，差一行就差了「稳定」）");

    printf("  喂进去的是一组【带负数、带重复】的数据 —— 负数会逼出「必须先减最小值」这件事：\n");
    copy_arr(work, ORIG, DEMO_N);
    print_arr("      ", work, DEMO_N);
    int mn, mx;
    arr_min_max(work, DEMO_N, &mn, &mx);
    printf("      min = %d，max = %d ⇒ 值域 k = %d。count[] 的下标只能是 0..k-1，\n", mn, mx, mx - mn + 1);
    printf("      所以每个数都要先算 v - min（= 平移到非负），写回时再加回来。\n");
    printf("      如果直接 cnt[a[i]]++，遇到 a[i] = %d 就是写 cnt[%d] —— 越界，程序当场乱掉。\n", mn, mn);

    printf("\n  ◆ 版本一：展开版（数完个数，值 v 出现 c 次就往回写 c 个 v）\n");
    copy_arr(work, ORIG, DEMO_N);
    int rc = counting_sort_expand(work, DEMO_N, 1, &st);
    print_arr("      排完: ", work, DEMO_N);
    printf("      返回码 %d | 大小比较 %ld 次 | 写数组 %ld 次 | 额外格子 %ld 个\n", rc, st.cmp, st.move, st.aux_cells);
    printf("      注意「大小比较 0 次」：它从头到尾【没问过谁比谁大】，只是数了数、按号占座。\n");
    if (rc == 1 && (!same_elements(work, EXPECT, DEMO_N) || !is_sorted_ascending(work, DEMO_N))) {
        all_pass = 0;
    }

    printf("\n  ◆ 版本二：前缀和版（cnt[v] 变成「值 ≤ v 有几个」，然后【从后往前】放人）\n");
    copy_arr(work, ORIG, DEMO_N);
    rc = counting_sort_stable(work, DEMO_N, 1, &st);
    print_arr("      排完: ", work, DEMO_N);
    printf("      返回码 %d | 大小比较 %ld 次 | 写数组 %ld 次 | 额外格子 %ld 个（= k 格计数 + n 格临时）\n",
           rc, st.cmp, st.move, st.aux_cells);
    if (rc == 1 && !same_elements(work, EXPECT, DEMO_N)) {
        all_pass = 0;
    }

    /* ============================================================
       第 2 幕：稳定性实测（分数 + 身份证）
       ============================================================ */
    section("2. 稳定性实测：两个 3 分的人，展开版会把他们倒过来，前缀和版不会");

    printf("  分数 88/88/70/88/60，原始顺序 a→b→c→d→e；关心的是"
           "「同分的三个人排完还是不是 a→b→d」：\n");
    int k1[DEMO_N] = {88, 88, 70, 88, 60, 88, 70, 88};
    int t1[DEMO_N] = {0, 1, 2, 3, 4, 5, 6, 7};
    int k2[DEMO_N], t2[DEMO_N];
    copy_arr(k2, k1, DEMO_N);
    copy_arr(t2, t1, DEMO_N);

    print_tagged("      原始:       ", k1, t1, DEMO_N);
    counting_expand_tagged(k1, t1, DEMO_N);
    print_tagged("      展开版排完: ", k1, t1, DEMO_N);
    counting_stable_tagged(k2, t2, DEMO_N);
    print_tagged("      前缀和排完: ", k2, t2, DEMO_N);

    int bad_expand = 0;
    int bad_stable = 0;
    for (int i = 1; i < DEMO_N; i++) {
        if (k1[i - 1] == k1[i] && t1[i - 1] > t1[i]) bad_expand++;
        if (k2[i - 1] == k2[i] && t2[i - 1] > t2[i]) bad_stable++;
    }
    printf("      展开版同分逆序处数 = %d ⇒ %s\n", bad_expand, bad_expand > 0 ? "不稳定（实锤）" : "这次碰巧对了");
    printf("      前缀和版同分逆序处数 = %d ⇒ %s\n", bad_stable, bad_stable == 0 ? "稳定（实锤）" : "不稳定");
    if (bad_expand == 0 || bad_stable != 0) all_pass = 0;
    printf("      差别就在写法上：展开版给每个值维护一摞 tag 用的是【栈】（正序压、逆序弹）；\n");
    printf("      前缀和版是【从后往前】扫原数组，后来者拿走这一段的最右格 ⇒ 顺序自然保住。\n");

    /* ============================================================
       第 3 幕：值域爆炸 —— 计数排序的死穴
       ============================================================ */
    section("3. 值域爆炸：n 很小也没用，count[] 的大小只看【值域】不看 n");

    printf("  把账算清楚（这是本节必须记住的一个数字）：\n");
    printf("    n = 10 万个 int 待排            ⇒ 数据本身 100000 * 4 = 0.4 MB\n");
    printf("    值是完整 32 位整数 ⇒ 值域 k = 2^32 ⇒ count[] 要 2^32 * 4 字节\n");
    printf("                                  = 17,179,869,184 字节 = 16 GB   ← 直接不可用\n");
    printf("  所以真实实现必须【先看值域再动手】。下面喂两组「看起来值域很宽」的数据，看它怎么拒绝：\n");

    const int WIDE[DEMO_N] = {-1000000000, 7, 999999999, -42, 123456, 0, -7, 999};
    copy_arr(work, WIDE, DEMO_N);
    print_arr("      数据 A（n=8，值域 20 亿）: ", work, DEMO_N);
    int rc_a = counting_sort_expand(work, DEMO_N, 0, &st);
    printf("      counting_sort_expand 返回码 = %d（-1 = 主动拒绝，没有 malloc、没有崩溃）\n", rc_a);
    copy_arr(work, WIDE, DEMO_N);
    int rc_a2 = radix_sort_lsd(work, DEMO_N, 0, &st);
    printf("      同一组数据交给基数排序：返回码 = %d，排完 = ", rc_a2);
    print_arr("", work, DEMO_N);
    printf("      ⇒ 基数完全不怕值域宽：它每轮只盯【一位十进制数】，那一位的值域永远是 0..9。\n");
    printf("        代价是轮数 = 位数（这组是 10 位），所以是 O(d*(n+10))。\n");

    printf("\n      数据 B：值域铺满整个 int（含 INT_MIN/INT_MAX）\n");
    const int FULL[DEMO_N] = {0, 2147483647, -2147483648, 5, -5, 1000000, -1000000, 42};
    copy_arr(work, FULL, DEMO_N);
    print_arr("      ", work, DEMO_N);
    int rc_b1 = counting_sort_stable(work, DEMO_N, 0, &st);
    copy_arr(work, FULL, DEMO_N);
    int rc_b2 = radix_sort_lsd(work, DEMO_N, 0, &st);
    printf("      计数返回 %d、基数返回 %d（两个都拒绝）⇒ 这时候正确答案就是快排/归并：\n", rc_b1, rc_b2);
    printf("      非比较排序不是「更高级」，是「多要一个前提」。前提不成立就得回去比大小。\n");
    if (rc_b1 != -1 || rc_b2 != -1) all_pass = 0;

    /* ============================================================
       第 4 幕：基数排序一轮一轮怎么做
       ============================================================ */
    section("4. 基数排序 LSD：个位→十位→百位，每一轮都是【一次稳定的计数排序】");

    const int RADIX[DEMO_N] = {170, 45, 75, 90, 802, 24, 2, 66};
    const int RADIX_OK[DEMO_N] = {2, 24, 45, 66, 75, 90, 170, 802};
    copy_arr(work, RADIX, DEMO_N);
    print_arr("      原始: ", work, DEMO_N);
    printf("      最高位是百位 ⇒ 要跑 3 轮。看每一轮之后数组变成什么样：\n");
    rc = radix_sort_lsd(work, DEMO_N, 1, &st);
    print_arr("      最终: ", work, DEMO_N);
    printf("      返回码 %d | 大小比较 %ld 次 | 写数组 %ld 次 | 额外格子 %ld\n", rc, st.cmp, st.move, st.aux_cells);
    printf("      整个基数排序【一次大小比较都没做】—— 它做的全是「按位分桶 + 前缀和定位」。\n");
    if (!same_elements(work, RADIX_OK, DEMO_N)) all_pass = 0;
    printf("      校验：与标准答案一致 = %s\n", same_elements(work, RADIX_OK, DEMO_N) ? "是" : "否");

    printf("\n      为什么必须【从最低位开始】？反过来试一次（下面这段只讲不跑，因为跑出来一定是错的）：\n");
    printf("        先按百位排：802、170 会被推到数组最后；再按个位排：个位把百位分好的组【完全打散】，\n");
    printf("        最后一轮结束时数组只保证「按个位有序」⇒ 前面几轮白干。\n");
    printf("        从低位往高位才是「越重要的位越最后处理」：最高位那一轮排完，低位次序靠稳定性保留，\n");
    printf("        高位次序刚刚定死 ⇒ 整体有序。\n");
    printf("      为什么每一轮【必须稳定】？举 45 和 75：个位相同（都是 5），十位排完后谁在前？\n");
    printf("        靠的是个位那轮留下的相对顺序 ⇒ 十位这轮只要不稳定，45/75 就可能被换到彼此后面。\n");

    printf("\n      带负数的基数（整体平移成非负再排，排完加回来）：\n");
    const int RNEG[TINY_N] = {-5, 42, -100, 7, 999};
    const int RNEG_OK[TINY_N] = {-100, -5, 7, 42, 999};
    int wsmall[TINY_N];
    copy_arr(wsmall, RNEG, TINY_N);
    print_arr("      原始: ", wsmall, TINY_N);
    rc = radix_sort_lsd(wsmall, TINY_N, 1, &st);
    print_arr("      最终: ", wsmall, TINY_N);
    printf("      返回码 %d，与标准答案一致 = %s\n", rc,
           same_elements(wsmall, RNEG_OK, TINY_N) ? "是" : "否");
    if (!same_elements(wsmall, RNEG_OK, TINY_N)) all_pass = 0;

    /* ============================================================
       第 5 幕：桶排序 —— 均匀 vs 全挤一块
       ============================================================ */
    section("5. 桶排序：前提「均匀分布」一旦不成立，退化成 O(n²) 是看得见的");

    int *bd = malloc((size_t)BUCKET_N * sizeof(int));
    int *be = malloc((size_t)BUCKET_N * sizeof(int));
    if (bd == NULL || be == NULL) {
        printf("  [错误] 内存申请失败，跳过本幕\n");
        all_pass = 0;
    } else {
        printf("  情况①：2000 个数均匀落在 [0, 9999]，切 10 个桶 ⇒ 每桶约 200 个\n");
        fill_range(bd, BUCKET_N, 0, 9999, 20261003u);
        clock_t t0 = clock();
        rc = bucket_sort(bd, BUCKET_N, BUCKET_CNT, 1, &st);
        double ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        printf("  返回码 %d | 桶内插入排序比较 %ld 次 | 写 %ld 次 | 额外格子 %ld | 耗时 %.2f ms\n",
               rc, st.cmp, st.move, st.aux_cells, ms);
        fill_range(be, BUCKET_N, 0, 9999, 20261003u);
        merge_sort_ref(be, BUCKET_N, &st.cmp);
        int ok1 = is_sorted_ascending(bd, BUCKET_N) && same_elements(bd, be, BUCKET_N);
        printf("  升序检查：%s | 与归并排序结果逐个一致：%s\n",
               is_sorted_ascending(bd, BUCKET_N) ? "是" : "否", same_elements(bd, be, BUCKET_N) ? "是" : "否");
        if (!ok1) all_pass = 0;

        printf("\n  情况②：1990 个数挤在 [0, 99]，再塞 10 个【离群值】到 9000 以上 ⇒ 分桶被离群值拉平\n");
        fill_range(bd, BUCKET_N - 10, 0, 99, 777u);
        for (int i = 0; i < 10; i++) {
            bd[BUCKET_N - 10 + i] = 9000 + i * 100;    /* 确定构造的离群值 */
        }
        t0 = clock();
        rc = bucket_sort(bd, BUCKET_N, BUCKET_CNT, 1, &st);
        ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        printf("  返回码 %d | 桶内插入排序比较 %ld 次 | 耗时 %.2f ms\n", rc, st.cmp, ms);
        fill_range(be, BUCKET_N - 10, 0, 99, 777u);
        for (int i = 0; i < 10; i++) {
            be[BUCKET_N - 10 + i] = 9000 + i * 100;
        }
        long mc2 = 0;                    /* 归并排序在这批退化数据上的比较次数 */
        merge_sort_ref(be, BUCKET_N, &mc2);
        int ok2 = is_sorted_ascending(bd, BUCKET_N) && same_elements(bd, be, BUCKET_N);
        printf("  结果与归并一致：%s（同样这批数据归并只用了 %ld 次比较）\n", ok2 ? "是" : "否", mc2);
        if (!ok2) all_pass = 0;
        printf("  ⇒ 正确性一点没坏，但代价完全变了：桶的分界是按【min..max】均分的，\n");
        printf("    10 个离群值把 max 顶到 9900 ⇒ 桶宽 990 ⇒ 那 1990 个 [0,99] 的数全掉进 0 号桶，\n");
        printf("    0 号桶单独做 1990 个元素的插入排序 ⇒ 比较次数从 10 万涨到上面那个数字。这就叫退化。\n");
        printf("    结论：用桶排序之前必须真的确认【分布均匀】（或先分位数切桶），否则不如快排。\n");
    }
    free(bd); bd = NULL;
    free(be); be = NULL;

    /* ============================================================
       第 6 幕：实测对比表 + 突破下界
       ============================================================ */
    section("6. 同一批数据（n=20000，值域 0..999）实测：比较排序 vs 三个非比较排序");

    printf("  先用一句话说清「下界」是什么：\n");
    printf("    任何【只靠比较大小】的排序，行为是一棵决策树；n 个元素的最终排列有 n! 种，\n");
    printf("    所以树至少要有 n! 个叶子 ⇒ 存在一条从根到叶的路径长度 ≥ log2(n!) ⇒ 最坏情况\n");
    printf("    至少 log2(n!) 次比较。斯特林公式给直觉：log2(n!) ≈ n*log2(n) - 1.44n = O(n log n)。\n");
    printf("    ⇒ 归并/快排/堆排的 O(n log n) 不是「实现得不够聪明」，是【数学天花板】。\n");

    int *big = malloc((size_t)BIG_N * sizeof(int));
    int *orig = malloc((size_t)BIG_N * sizeof(int));
    int *sorted_ref = malloc((size_t)BIG_N * sizeof(int));
    if (big == NULL || orig == NULL || sorted_ref == NULL) {
        printf("  [错误] 内存申请失败，跳过本幕\n");
        all_pass = 0;
    } else {
        fill_range(orig, BIG_N, 0, VAL_RANGE - 1, 4242u);
        printf("\n  先自检这个手搓 log2 准不准（不用 <math.h>，见文件头注释）：\n");
        printf("    my_log2(65536) 应当正好 = 16.0000（2^16 = 65536）… 算出来 %.4f\n", my_log2_int(65536));
        printf("    log2(10!) 真值 = 21.7910（10! = 3628800）… 算出来 %.4f\n", log2_factorial(10));
        if (my_log2_int(65536) < 15.99 || my_log2_int(65536) > 16.01) all_pass = 0;
        if (log2_factorial(10) < 21.78 || log2_factorial(10) > 21.80) all_pass = 0;
        double lb = log2_factorial(BIG_N);
        double stirling = (double)BIG_N * my_log2_int(BIG_N) - 1.4427 * (double)BIG_N;
        printf("  下界数值（本批数据 n = %d）：精确累加 log2(n!) = %.0f 次比较\n", BIG_N, lb);
        printf("  斯特林近似 n*log2(n) - 1.4427n = %.0f 次 ⇒ 和精确值只差 %.0f 次（%.4f%%）⇒ 量级完全一致\n",
               stirling, lb > stirling ? lb - stirling : stirling - lb,
               100.0 * (lb > stirling ? lb - stirling : stirling - lb) / lb);
        printf("  ⇒ 比较排序最坏至少要这么多次比较；下面每行都标出自己做了多少次，看谁绕过了它。\n\n");

        copy_arr(sorted_ref, orig, BIG_N);
        long merge_cmp = 0;
        clock_t t0m = clock();
        merge_sort_ref(sorted_ref, BIG_N, &merge_cmp);
        double msm = (double)(clock() - t0m) * 1000.0 / CLOCKS_PER_SEC;
        printf("  ┌ 参照（比较排序）\n");
        printf("  │   归并排序     %7.2f ms  比较 %9ld 次（下界 %.0f 的同量级）  O(n log n)，稳定，要 O(n) 空间\n",
               msm, merge_cmp, lb);
        printf("  │   注：下界 log2(n!) 是【最坏情况、n 个互不相同的元素】的账；这批数据只有 1000 个不同值，\n");
        printf("  │   可区分的排列远少于 n!，所以实测比它略小很正常。要记的是量级：约 2.5×10^5 次，\n");
        printf("  │   和 n*log2(n) = 20000*14.29 ≈ 2.86×10^5 同一档 —— 归并确实贴着下界在走。\n");

        copy_arr(big, orig, BIG_N);
        t0m = clock();
        insertion_sort_ref(big, BIG_N, &merge_cmp);
        msm = (double)(clock() - t0m) * 1000.0 / CLOCKS_PER_SEC;
        printf("  │   插入排序     %7.2f ms  比较 %9ld 次                   O(n²)，小数组的王者\n", msm, merge_cmp);
        if (!is_sorted_ascending(big, BIG_N) || !same_multiset(big, sorted_ref, BIG_N)) all_pass = 0;

        printf  ("  └ 正确性基准：上面两行排完的结果逐个相同（插入 vs 归并），后面都拿它来验货\n");

        printf("\n  ┌ 非比较排序（同一批数据、同一个朴素检查函数 same_multiset 验货）\n");
        /* 标签用 ASCII：%-14s 按字节算宽度，中文会让表格错位（00 章就踩过这个坑）*/
        struct { const char *name; int kind; } rows[4] = {
            {"count-expand", 0}, {"count-prefix", 1}, {"radix-LSD", 2}, {"bucket-200", 3}
        };
        for (int r = 0; r < 4; r++) {
            copy_arr(big, orig, BIG_N);
            st.cmp = st.move = st.aux_cells = 0;
            t0m = clock();
            int code;
            if (rows[r].kind == 0) {
                code = counting_sort_expand(big, BIG_N, 0, &st);
            } else if (rows[r].kind == 1) {
                code = counting_sort_stable(big, BIG_N, 0, &st);
            } else if (rows[r].kind == 2) {
                code = radix_sort_lsd(big, BIG_N, 0, &st);
            } else {
                code = bucket_sort(big, BIG_N, 200, 0, &st);
            }
            msm = (double)(clock() - t0m) * 1000.0 / CLOCKS_PER_SEC;
            int ok = (code == 1) && is_sorted_ascending(big, BIG_N) && same_multiset(big, sorted_ref, BIG_N);
            printf("  │   %-13s %7.2f ms  比较 %9ld 次  写 %8ld 次  额外格子 %7ld  正确性：%s\n",
                   rows[r].name, msm, st.cmp, st.move, st.aux_cells, ok ? "通过" : "失败");
            if (!ok) all_pass = 0;
        }
        printf("  └ 计数/基数的「比较 0 次」不是快，是【压根没在下界管辖范围内】：\n");
        printf("      它们用了「这个数是多少」这个信息，等价于直接查表；而下界只管【比大小】的算法。\n");
        printf("      桶排序是唯一还会比较的（桶内用插入排序），所以它的比较次数不为 0。\n");

        printf("\n  值域 k 的影响（同一批 n=20000，只改值域）：\n");
        const int RANGES[3] = {1000, 20000, 200000};
        for (int r = 0; r < 3; r++) {
            fill_range(orig, BIG_N, 0, RANGES[r] - 1, 4242u);
            copy_arr(big, orig, BIG_N);
            st.cmp = st.move = st.aux_cells = 0;
            t0m = clock();
            int code = counting_sort_stable(big, BIG_N, 0, &st);
            double msc = (double)(clock() - t0m) * 1000.0 / CLOCKS_PER_SEC;
            double cellsc = (double)st.aux_cells;

            copy_arr(sorted_ref, orig, BIG_N);
            merge_cmp = 0;
            t0m = clock();
            merge_sort_ref(sorted_ref, BIG_N, &merge_cmp);
            double msmg = (double)(clock() - t0m) * 1000.0 / CLOCKS_PER_SEC;

            copy_arr(big, orig, BIG_N);
            counting_sort_stable(big, BIG_N, 0, &st);
            int ok = (code == 1) && is_sorted_ascending(big, BIG_N) && same_elements(big, sorted_ref, BIG_N);
            printf("    k = %7d │ 计数 %6.2f ms（额外格子 %ld）│ 归并 %6.2f ms（比较 %ld 次）│ 两边结果一致：%s\n",
                   RANGES[r], msc, (long)cellsc, msmg, merge_cmp, ok ? "是" : "否");
            if (!ok) all_pass = 0;
        }
        printf("    ↑ 计数的【额外格子和时间】都跟着 k 线性涨，而归并的时间几乎不动（它压根不看值域）：\n");
        printf("      这就是 O(n+k) 里那个 k 不是免费的地方 —— k 一旦比 n 大出几十倍，计数就没优势了。\n");
    }
    free(big); big = NULL;
    free(orig); orig = NULL;
    free(sorted_ref); sorted_ref = NULL;

    /* ============================================================
       第 7 幕：正确性与边界总校验
       ============================================================ */
    section("7. 正确性与边界总校验");

    printf("  边界数据（每种都跑三个算法，三个都要对；用 same_multiset 做朴素验货）：\n");
    int edge_data[6][DEMO_N] = {
        {42, 0, 0, 0, 0, 0, 0, 0},                              /* n=1 */
        {9, 1, 0, 0, 0, 0, 0, 0},                               /* n=2 */
        {7, 7, 7, 7, 0, 0, 0, 0},                               /* 全相同：基数会跑几轮"全 0 位" */
        {5, -3, 0, -8, 12, 1, -1000000, 999999},                /* 负数 + 值域偏大 */
        {2, 1, 1, 2, 1, 2, 0, 0},                               /* 大量重复 */
        {1, 2, 3, 4, 5, 6, 7, 8},                               /* 已升序：非比较排序根本不关心 */
    };
    const int edge_n[6] = {1, 2, 4, 8, 6, 8};
    for (int e = 0; e < 6; e++) {
        int m = edge_n[e];
        int ref[DEMO_N];
        copy_arr(ref, edge_data[e], m);
        merge_sort_ref(ref, m, NULL);                    /* 参照答案 */

        int t[DEMO_N];
        copy_arr(t, edge_data[e], m);
        int c1 = counting_sort_expand(t, m, 0, &st);
        int o1 = (c1 == 1) && same_elements(t, ref, m);

        copy_arr(t, edge_data[e], m);
        int c2 = counting_sort_stable(t, m, 0, &st);
        int o2 = (c2 == 1) && same_elements(t, ref, m);

        copy_arr(t, edge_data[e], m);
        int c3 = radix_sort_lsd(t, m, 0, &st);
        int o3 = (c3 == 1) && same_elements(t, ref, m);

        copy_arr(t, edge_data[e], m);
        int c4 = bucket_sort(t, m, 4, 0, &st);
        int o4 = (c4 == 1) && same_elements(t, ref, m);
        (void)c1; (void)c2; (void)c3; (void)c4;

        printf("    输入 n=%d: [", m);
        for (int i = 0; i < m; i++) {
            printf(" %d", edge_data[e][i]);
        }
        printf(" ] → 期望 [");
        for (int i = 0; i < m; i++) {
            printf(" %d", ref[i]);
        }
        /* 返回 -1 = 值域过大主动拒绝 —— 这是【正确行为】，不能算排错 */
        printf(" ]  计数展开 %s | 计数稳定 %s | 基数 %s | 桶 %s\n",
               o1 ? "对" : (c1 == -1 ? "拒绝(值域过大,合理)" : "错"),
               o2 ? "对" : (c2 == -1 ? "拒绝(值域过大,合理)" : "错"),
               o3 ? "对" : (c3 == -1 ? "拒绝(值域过大,合理)" : "错"),
               o4 ? "对" : (c4 == -1 ? "拒绝(值域过大,合理)" : "错"));
        if ((!o1 && c1 != -1) || (!o2 && c2 != -1) || (!o3 && c3 != -1) || (!o4 && c4 != -1)) all_pass = 0;
    }
    int z[1] = {77};
    counting_sort_expand(z, 0, 0, &st);
    counting_sort_stable(z, 0, 0, &st);
    radix_sort_lsd(z, 0, 0, &st);
    bucket_sort(z, 0, 4, 0, &st);
    printf("    n=0：四个入口都在第一行返回，数组仍是 [ 77 ]（一个下标都没碰）\n");

    printf("\n  大样本一致性（n=5000，值域 0..99，四个算法两两结果必须逐个相同）：\n");
    const int CS_N = 5000;
    int *c1 = malloc((size_t)CS_N * sizeof(int));
    int *c2 = malloc((size_t)CS_N * sizeof(int));
    int *c3 = malloc((size_t)CS_N * sizeof(int));
    int *c4 = malloc((size_t)CS_N * sizeof(int));
    int *src = malloc((size_t)CS_N * sizeof(int));
    if (c1 == NULL || c2 == NULL || c3 == NULL || c4 == NULL || src == NULL) {
        printf("    [错误] 申请失败\n");
        all_pass = 0;
    } else {
        fill_range(src, CS_N, 0, 99, 31337u);
        copy_arr(c1, src, CS_N); counting_sort_expand(c1, CS_N, 0, &st);
        copy_arr(c2, src, CS_N); counting_sort_stable(c2, CS_N, 0, &st);
        copy_arr(c3, src, CS_N); radix_sort_lsd(c3, CS_N, 0, &st);
        copy_arr(c4, src, CS_N); bucket_sort(c4, CS_N, 500, 0, &st);
        int agree = same_elements(c1, c2, CS_N) && same_elements(c1, c3, CS_N) && same_elements(c1, c4, CS_N);
        int asc = is_sorted_ascending(c1, CS_N) && is_sorted_ascending(c2, CS_N)
                  && is_sorted_ascending(c3, CS_N) && is_sorted_ascending(c4, CS_N);
        int keep = same_multiset(c1, src, CS_N);
        printf("    四个结果逐个相同：%s | 全部升序：%s | 元素守恒（与原数据同多重集合）：%s\n",
               agree ? "是" : "否", asc ? "是" : "否", keep ? "是" : "否");
        if (!agree || !asc || !keep) all_pass = 0;
    }
    free(c1); free(c2); free(c3); free(c4); free(src);
    c1 = c2 = c3 = c4 = src = NULL;

    printf("\n=== 本节结束 ===\n");
    printf("总校验：%s\n", all_pass ? "全部通过（含与归并/插入的交叉对拍、值域爆炸的主动拒绝）" : "有不一致，去看代码");
    printf("记住五句话：\n");
    printf("  1) 计数排序 O(n+k)：k 是值域，不是 n —— k 一大就废（32 位整数 ⇒ count[] 要 16GB）。\n");
    printf("  2) 计数必须先减最小值才能处理负数；前缀和 + 从后往前 ⇒ 稳定，展开版 ⇒ 不稳定。\n");
    printf("  3) 基数 = 对每一位跑一次【稳定】计数排序，从最低位开始；它只看位数，不看值域大小。\n");
    printf("  4) 桶 = 均匀分布前提下的分治；数据全挤一块就退化成一个桶里的 O(n²)。\n");
    printf("  5) 比较排序的 O(n log n) 是数学下界（log2(n!) 次比较），计数/基数靠「知道值是多少」绕过去。\n");
    return 0;
}
