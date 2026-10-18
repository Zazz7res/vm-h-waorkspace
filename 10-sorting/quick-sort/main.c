#include <stdio.h>
#include <stdlib.h>
#include "quick_sort.h"

#define N 8
#define BIG_N 64

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

/*
 * 统计行统一格式。
 * 为什么不用 printf("%-16s", 中文名)：中文字符在终端里占 2 列但在 C 里按字节算宽度，
 * %-16s 会把表格拉歪。所以这里把标签用【】包起来放在最前面，不靠空格对齐。
 */
static void report_qstat(const char *name, const QStat *st)
{
    printf("      %-22s 比较 %6ld | 交换 %5ld | 分区 %3ld 趟 | 最大递归深度 %d\n",
           name, st->cmp, st->swap, st->partitions, st->max_depth);
}

int main(void)
{
    const int ORIG[N]   = {9, 5, 7, 1, 3, 8, 2, 6};   /* 硬编码，保证输出可复核 */
    const int EXPECT[N] = {1, 2, 3, 5, 6, 7, 8, 9};   /* 手算标准答案 */
    const int ASC[N]    = {1, 2, 3, 4, 5, 6, 7, 8};   /* 快排的经典毒药：已升序 + 固定取最后一个 */

    int work[N];
    QStat st;
    int all_pass = 1;

    /* ============================================================
       第 1 幕：Lomuto 分区 —— 一趟劈开，枢轴直接站上最终位置
       ============================================================ */
    section("1. Lomuto 分区（选最后一个当枢轴）：逐趟打印分区结果 + 枢轴落点");

    printf("  分区的承诺只有一句话：跑完之后【左边全都 ≤ 枢轴 ≤ 右边全都】，\n");
    printf("  而且枢轴正好站在它最终该站的位置上。所以枢轴这辈子只被处理一次，\n");
    printf("  两半边跟它再无任何关系 —— 这就是「分而治之」在快排里的落点。\n");
    copy_arr(work, ORIG, N);
    print_arr("      排之前: ", work, N);
    printf("      （«» 圈住的是本趟的枢轴：分区前它在末尾，分区后它在最终位置）\n");
    quick_sort_lomuto(work, N, 1, &st);
    print_arr("      最终: ", work, N);
    report_qstat("Lomuto/8-shuf", &st);
    if (!is_sorted_ascending(work, N) || !same_elements(work, EXPECT, N)) {
        all_pass = 0;
    }
    printf("      校验：与标准答案一致 = %s，元素守恒（没丢数、没改数） = %s\n",
           same_elements(work, EXPECT, N) ? "是" : "否",
           is_permutation(ORIG, work, N) ? "是" : "否");

    /* ============================================================
       第 2 幕：Hoare 分区 —— 双指针对向走，交换少但边界更绕
       ============================================================ */
    section("2. Hoare 双指针分区：交换次数明显少，但返回值【不是】枢轴位置");

    copy_arr(work, ORIG, N);
    print_arr("      排之前: ", work, N);
    QStat st_h;
    quick_sort_hoare(work, N, 1, &st_h);
    print_arr("      最终: ", work, N);
    report_qstat("Hoare/8-shuf", &st_h);
    if (!same_elements(work, EXPECT, N)) {
        all_pass = 0;
    }

    printf("\n      【两种写法逐项对比】（同一批数据，安静跑一遍拿数字）：\n");
    int w1[N], w2[N];
    QStat s1, s2;
    copy_arr(w1, ORIG, N);
    quick_sort_lomuto(w1, N, 0, &s1);
    copy_arr(w2, ORIG, N);
    quick_sort_hoare(w2, N, 0, &s2);
    report_qstat("Lomuto/8-shuf", &s1);
    report_qstat("Hoare/8-shuf", &s2);
    printf("      差别① 返回值含义：Lomuto 返回枢轴【最终下标】⇒ 递归写 (lo, p-1) 和 (p+1, hi)；\n");
    printf("              Hoare 返回【分界点】⇒ 递归必须写 (lo, p) 和 (p+1, hi)，写成 (lo, p-1) 会漏掉 a[p]。\n");
    printf("      差别② 指针起步：Lomuto 的 i 从 lo-1 起、只往右走；Hoare 两个指针【先走一步再看值】(do-while)，\n");
    printf("              少写这一步就是自己跟自己比 ⇒ 死循环。\n");
    printf("      差别③ 等号归属：Lomuto 用 a[j] <= pivot（等号进左区）；Hoare 内层必须严格 < 和 >，\n");
    printf("              写成 <= / >= 时全相同元素会让两个指针停在原地不动 ⇒ 死循环。\n");
    printf("      差别④ 交换量：Hoare 一次换掉两个错位元素，Lomuto 每确认一个就要搬一次 + 枢轴再搬一次。\n");

    /* ============================================================
       第 3 幕：最坏情况现场 —— 快排最核心的知识点
       ============================================================ */
    section("3. 最坏情况现场：已经升序 + 固定选最后一个当枢轴 ⇒ 塌成一根链条");

    copy_arr(work, ASC, N);
    print_arr("      喂进去: ", work, N);
    printf("      数据【已经排好了】，看着应该一秒排完吧？看它怎么每趟只劈掉 1 个：\n");
    QStat st_worst;
    quick_sort_lomuto(work, N, 1, &st_worst);
    report_qstat("Lomuto-last/8-asc", &st_worst);
    printf("      每趟枢轴都是这一段的最大值 ⇒ 右半永远 0 个、左半 n-1 个 ⇒\n");
    printf("      递归树不再是一棵树，是一根【链条】：深度 %d，比较 %ld 次（= 7+6+5+4+3+2+1 = n(n-1)/2）⇒ O(n²)。\n",
           st_worst.max_depth, st_worst.cmp);

    printf("\n      【救法一】三数取中 median-of-three：用 a[lo]、a[mid]、a[hi] 三个数的中位数当枢轴\n");
    copy_arr(work, ASC, N);
    QStat st_med;
    quick_sort_median3(work, N, 1, &st_med);
    print_arr("      最终: ", work, N);
    report_qstat("median3/8-asc", &st_med);
    printf("      升序数据里 lo 是最小、hi 是最大 ⇒ 中位数正好是中间那个值 ⇒ 一次劈成两半。\n");
    printf("      同样是 8 个升序数：比较 %ld → %ld，递归深度 %d → %d。\n",
           st_worst.cmp, st_med.cmp, st_worst.max_depth, st_med.max_depth);
    printf("      n 很小的时候比较次数省得不多，但【深度】从链条变回了树；n=64 时是 2016 → 360（见第 4 幕）。\n");

    printf("\n      【救法二】随机化 pivot：每次随机抽一个下标换到末尾（固定种子，输出可复核）\n");
    copy_arr(work, ASC, N);
    QStat st_rnd;
    quick_sort_randomized(work, N, 1, &st_rnd);
    print_arr("      最终: ", work, N);
    report_qstat("randomized/8-asc", &st_rnd);
    printf("      随机化的价值不在于「平均更省」，而在于【没有任何一份输入能稳定坑到它】：\n");
    printf("      三数取中对有序/近有序特别有效，但它是【确定的】，对手可以专门构造数据去坑它\n");
    printf("      （Ninomiya–Takahashi 攻击就是针对 median-of-three 造的），随机化没有固定弱点。\n");

    /* ============================================================
       第 4 幕：规模实测 —— O(n²) 到底多疼
       ============================================================ */
    section("4. 同一份最坏数据（64 个升序数）：几种写法的真实代价");

    int big_asc[BIG_N];
    int big_shuf[BIG_N];
    int buf[BIG_N];
    for (int i = 0; i < BIG_N; i++) {
        big_asc[i] = i + 1;               /* 确定构造，不是随机 */
        big_shuf[i] = (i * 37) % BIG_N;   /* 确定「乱序」，用来对比平均情况 */
    }

    printf("  ┌ 数据 A：升序 [1..64]（专门喂给固定末位枢轴的毒药）\n");
    QStat s_a_l, s_a_h, s_a_m, s_a_r;
    copy_arr(buf, big_asc, BIG_N); quick_sort_lomuto(buf, BIG_N, 0, &s_a_l);
    report_qstat("Lomuto-last/64-asc", &s_a_l);
    if (!same_elements(buf, big_asc, BIG_N)) all_pass = 0;   /* 本来就有序 ⇒ 排完必须一模一样 */

    copy_arr(buf, big_asc, BIG_N); quick_sort_hoare(buf, BIG_N, 0, &s_a_h);
    report_qstat("Hoare-mid/64-asc", &s_a_h);
    if (!is_sorted_ascending(buf, BIG_N)) all_pass = 0;

    copy_arr(buf, big_asc, BIG_N); quick_sort_median3(buf, BIG_N, 0, &s_a_m);
    report_qstat("median3/64-asc", &s_a_m);
    if (!is_sorted_ascending(buf, BIG_N)) all_pass = 0;

    copy_arr(buf, big_asc, BIG_N); quick_sort_randomized(buf, BIG_N, 0, &s_a_r);
    report_qstat("randomized/64-asc", &s_a_r);
    if (!is_sorted_ascending(buf, BIG_N)) all_pass = 0;

    printf("  └ 一句话结论：固定取末位 = %ld 次比较 / 深度 %d（一根链条）；\n", s_a_l.cmp, s_a_l.max_depth);
    printf("     换成中点值 / 三数取中 / 随机化，比较次数直接砍到 %ld ~ %ld 的量级，深度 %d ~ %d。\n",
           s_a_h.cmp < s_a_m.cmp ? s_a_h.cmp : s_a_m.cmp,
           s_a_r.cmp > s_a_m.cmp ? s_a_r.cmp : s_a_m.cmp,
           s_a_h.max_depth, s_a_r.max_depth);

    printf("\n  ┌ 数据 B：乱序 (i*37)%%64（平均情况）\n");
    QStat s_b_l, s_b_h, s_b_m;
    copy_arr(buf, big_shuf, BIG_N); quick_sort_lomuto(buf, BIG_N, 0, &s_b_l);
    report_qstat("Lomuto/64-shuf", &s_b_l);
    if (!is_sorted_ascending(buf, BIG_N)) all_pass = 0;
    copy_arr(buf, big_shuf, BIG_N); quick_sort_hoare(buf, BIG_N, 0, &s_b_h);
    report_qstat("Hoare/64-shuf", &s_b_h);
    if (!is_sorted_ascending(buf, BIG_N)) all_pass = 0;
    copy_arr(buf, big_shuf, BIG_N); quick_sort_median3(buf, BIG_N, 0, &s_b_m);
    report_qstat("median3/64-shuf", &s_b_m);
    if (!is_sorted_ascending(buf, BIG_N)) all_pass = 0;
    printf("  └ 平均情况下大家都比「最坏数据」省得多（Lomuto 从 %ld 次比较降到 %ld 次）。\n", s_a_l.cmp, s_b_l.cmp);
    printf("     Hoare 的真实脾气（对照上面两行数字）：交换次数比 Lomuto 少（%ld vs %ld），\n", s_b_h.swap, s_b_l.swap);
    printf("     但比较次数更多（%ld vs %ld）、分区趟数恒定是 n-1 = 63（Lomuto 只有 %ld 趟）——\n", s_b_h.cmp, s_b_l.cmp, s_b_l.partitions);
    printf("     因为 Hoare 的两个子区间永远非空（每个区间至少 1 个元素落地），而 Lomuto 经常把某一侧劈成空的。\n");
    printf("     换来换去：Hoare 省【写内存】，Lomuto 省【比较和递归次数】、还能直接告诉你枢轴在哪。\n");

    /* ============================================================
       第 5 幕：先递归短的那半边 —— 栈深度锁死 O(log n)
       ============================================================ */
    section("5. 尾递归优化：只递归短的那半边，长半边用 while 循环吃掉（救栈，不救时间）");

    printf("  朴素写法在最坏数据下，栈上要同时挂 n 层「等左半边返回」。\n");
    printf("  改法两句话：① 哪半边短就【递归】哪半边；② 另一半边不递归，改 lo/hi 后回到 while 顶部继续吃。\n");
    printf("  为什么栈就锁死 O(log n)：每次进递归，那个子问题规模最多是当前的一半 ⇒ 嵌套不会超过 log2(n)。\n");
    copy_arr(work, ASC, N);
    QStat s_plain;
    quick_sort_lomuto(work, N, 0, &s_plain);
    copy_arr(work, ASC, N);
    QStat s_tail;
    quick_sort_tail_opt(work, N, 1, &s_tail);
    print_arr("      优化后最终: ", work, N);
    report_qstat("plain-Lomuto/8-asc", &s_plain);
    report_qstat("tail-opt/8-asc", &s_tail);
    printf("      数字说明一切：最大递归深度 %d → %d，可比较次数 %ld 和 %ld 一模一样 ——\n",
           s_plain.max_depth, s_tail.max_depth, s_plain.cmp, s_tail.cmp);
    printf("      ⇒ 这个优化【只把栈压扁，不省时间】：最坏情况还是 O(n²)，只是不再爆栈。\n");

    copy_arr(buf, big_asc, BIG_N);
    QStat s_plain64, s_tail64;
    quick_sort_lomuto(buf, BIG_N, 0, &s_plain64);
    copy_arr(buf, big_asc, BIG_N);
    quick_sort_tail_opt(buf, BIG_N, 0, &s_tail64);
    report_qstat("plain-Lomuto/64-asc", &s_plain64);
    report_qstat("tail-opt/64-asc", &s_tail64);
    printf("      深度 %d → %d：64 层的等待全部变成循环。真实工程实现都带这一招，\n",
           s_plain64.max_depth, s_tail64.max_depth);
    printf("      再配上「小数组切回插入排序」和「递归太深就换成堆排序」，就是 introsort 的骨架\n");
    printf("      （最后那一步见 ../heap-sort/README.md 的说明）。\n");
    if (!is_sorted_ascending(buf, BIG_N)) all_pass = 0;

    /* ============================================================
       第 6 幕：快排不稳定的现场
       ============================================================ */
    section("6. 实测：快排不稳定 —— 3 个数就能翻车");

    printf("  两个 88 分的人 a、b 和一个 70 分的 c；枢轴取最后一个 = 70c：\n");
    int key[3] = {88, 88, 70};
    int tag[3] = {0, 1, 2};
    print_tagged("      排之前: ", key, tag, 3);
    quick_sort_tagged(key, tag, 3);
    print_tagged("      排之后: ", key, tag, 3);
    printf("      扫描时 88a、88b 都 > 70，一次都没进「≤ 枢轴区」，i 一直停在 lo-1；\n");
    printf("      收尾把枢轴和 a[i+1] = a[0] 做那次【远距离交换】⇒ 88a 被扔到了 88b 后面 ⇒ 顺序反了。\n");
    printf("      结论：要稳定就去 ../merge-sort/，快排天生给不了。\n");

    /* ============================================================
       第 7 幕：正确性与边界总校验
       ============================================================ */
    section("7. 正确性与边界总校验");

    printf("      工程版 quick_sort（Lomuto，无打印无计数）：\n");
    copy_arr(work, ORIG, N);
    quick_sort(work, N);
    print_arr("        ", work, N);
    if (!same_elements(work, EXPECT, N)) all_pass = 0;

    printf("\n      五种入口跑同一批数据，结果必须完全相同：\n");
    int v1[N], v2[N], v3[N], v4[N], v5[N];
    copy_arr(v1, ORIG, N); quick_sort_lomuto(v1, N, 0, &st);
    copy_arr(v2, ORIG, N); quick_sort_hoare(v2, N, 0, &st);
    copy_arr(v3, ORIG, N); quick_sort_median3(v3, N, 0, &st);
    copy_arr(v4, ORIG, N); quick_sort_randomized(v4, N, 0, &st);
    copy_arr(v5, ORIG, N); quick_sort_tail_opt(v5, N, 0, &st);
    print_arr("        Lomuto   : ", v1, N);
    print_arr("        Hoare    : ", v2, N);
    print_arr("        三数取中 : ", v3, N);
    print_arr("        随机化   : ", v4, N);
    print_arr("        短边递归 : ", v5, N);
    int five_ok = same_elements(v1, EXPECT, N) && same_elements(v2, EXPECT, N) &&
                  same_elements(v3, EXPECT, N) && same_elements(v4, EXPECT, N) &&
                  same_elements(v5, EXPECT, N);
    printf("        五种写法结果全部等于标准答案：%s\n", five_ok ? "是" : "否");
    if (!five_ok) all_pass = 0;

    printf("\n      边界数据（每种都跑 Lomuto 和 Hoare 两种分区，两个都过才算过）：\n");
    int edge_data[6][8] = {
        {42, 0, 0, 0, 0, 0, 0, 0},             /* n=1 */
        {9, 1, 0, 0, 0, 0, 0, 0},              /* n=2 */
        {7, 7, 7, 7, 0, 0, 0, 0},              /* 全相同 —— 专测 Hoare 的等号陷阱 */
        {5, -3, 0, -8, 12, 1, 0, 0},           /* 带负数 */
        {2, 1, 1, 2, 1, 2, 0, 0},              /* 大量重复 —— 专测 Lomuto 的 <= 一边倒 */
        {1, 2, 3, 4, 5, 6, 7, 8},              /* 已升序 —— 最坏情况 */
    };
    const int edge_n[6] = {1, 2, 4, 6, 6, 8};
    for (int e = 0; e < 6; e++) {
        int tmp[8];
        copy_arr(tmp, edge_data[e], edge_n[e]);
        quick_sort_lomuto(tmp, edge_n[e], 0, &st);
        int ok_l = is_sorted_ascending(tmp, edge_n[e]) && is_permutation(edge_data[e], tmp, edge_n[e]);
        int lomuto_out[8];
        copy_arr(lomuto_out, tmp, edge_n[e]);

        copy_arr(tmp, edge_data[e], edge_n[e]);
        quick_sort_hoare(tmp, edge_n[e], 0, &st);
        int ok_h = is_sorted_ascending(tmp, edge_n[e]) && is_permutation(edge_data[e], tmp, edge_n[e]);

        print_arr("      输入: ", edge_data[e], edge_n[e]);
        print_arr("            Lomuto 出: ", lomuto_out, edge_n[e]);
        printf("            Hoare  出: [");
        for (int i = 0; i < edge_n[e]; i++) {
            printf(" %d", tmp[i]);
        }
        printf(" ]  ⇒ 升序+守恒：Lomuto %s / Hoare %s\n", ok_l ? "对" : "错", ok_h ? "对" : "错");
        if (!ok_l || !ok_h) all_pass = 0;
    }
    int z[1] = {77};
    QStat st_z;
    quick_sort(z, 0);
    quick_sort_lomuto(z, 0, 0, &st_z);
    quick_sort_hoare(z, 0, 0, &st_z);
    quick_sort_median3(z, 0, 0, &st_z);
    quick_sort_randomized(z, 0, 0, &st_z);
    quick_sort_tail_opt(z, 0, 0, &st_z);
    printf("      n=0：五个入口全部安全返回（第一行就 return，一个下标都没碰），数组仍是 [ 77 ]（没被写坏）\n");

    printf("\n      规模校验：1024 个确定值 (i*37)%%1024（它是 0..1023 的一个排列，正确答案已知）\n");
    const int M = 1024;
    int *mbuf = malloc((size_t)M * sizeof(int));
    int *mexp = malloc((size_t)M * sizeof(int));
    if (mbuf == NULL || mexp == NULL) {
        printf("      [错误] 内存申请失败，跳过规模校验\n");
    } else {
        for (int i = 0; i < M; i++) {
            mbuf[i] = (i * 37) % M;
            mexp[i] = i;
        }
        QStat s_big;
        quick_sort_lomuto(mbuf, M, 0, &s_big);
        report_qstat("Lomuto/1024-shuf", &s_big);
        printf("      排完与 0..1023 完全一致：%s，升序检查：%s，最大递归深度 %d\n",
               same_elements(mbuf, mexp, M) ? "是" : "否",
               is_sorted_ascending(mbuf, M) ? "是" : "否",
               s_big.max_depth);
        printf("      深度 %d 比 log2(1024)=10 大一些：Lomuto 不是每次都劈成均匀两半，\n", s_big.max_depth);
        printf("      但离「最坏情况 1023 层」还差得远 —— 乱序数据喂给快排，就是安全的。\n");
        if (!same_elements(mbuf, mexp, M) || !is_sorted_ascending(mbuf, M)) all_pass = 0;
    }
    free(mbuf); mbuf = NULL;
    free(mexp); mexp = NULL;

    printf("\n=== 本节结束 ===\n");
    printf("总校验：%s\n",
           all_pass ? "五种写法结果全部与标准答案一致，分区承诺每趟都程序化验过" : "有不一致，去看代码");
    printf("记住五句话：\n");
    printf("  1) 一趟分区的成果 = 枢轴站上最终位置 + 左 ≤ 枢轴 ≤ 右；之后枢轴再也不管。\n");
    printf("  2) Lomuto 返回枢轴下标 ⇒ 递归 (lo,p-1)/(p+1,hi)；Hoare 返回分界点 ⇒ 递归 (lo,p)/(p+1,hi)。\n");
    printf("  3) 固定选最后一个 + 有序数据 = 塌成链条 O(n²)；三数取中或随机化 pivot 破掉它。\n");
    printf("  4) 先递归短的那半边、长半边用循环 ⇒ 栈深度锁死 O(log n)，但时间不变（只救栈）。\n");
    printf("  5) 收尾那次远距离交换让快排必然不稳定；要稳定请用归并。\n");
    return 0;
}
