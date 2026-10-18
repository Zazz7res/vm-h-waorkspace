#include <stdio.h>
#include "basic_sort.h"

/*
 * N 为什么用 #define 而不是 const int N = 8; ？
 *   C 语言里 `const int N = 8; int a[N];` 的 a 是「变长数组 VLA」，
 *   而且【不能带初始化列表】，直接编译报错。
 *   #define 是编译前的文字替换，a[N] 就变成了 a[8]，是普通数组，可以初始化。
 */
#define N 8

/* 打印一行分隔线 + 标题，让每一幕开头清楚 */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/* 把原始数据复制一份到 work[]：每一幕都从同一份数据重新开始，输出才可比 */
static void copy_arr(int dst[], const int src[], int n)
{
    for (int i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

/* 把 SortStat 打成一行，方便三个算法横向对比 */
static void report_stat(const char *name, const SortStat *st)
{
    printf("      %-11s 比较 %2ld 次 | 挪动/交换 %2ld 次 | 跑了 %ld 趟%s\n",
           name, st->cmp, st->swap, st->rounds,
           st->early_exit ? "（提前收工）" : "");
}

int main(void)
{
    /* 全场统一数据：8 个硬编码数字，保证每次跑出来的输出一模一样 */
    const int ORIG[N]   = {5, 2, 9, 1, 7, 3, 8, 4};
    const int EXPECT[N] = {1, 2, 3, 4, 5, 7, 8, 9};   /* 手算的标准答案 */

    int work[N];
    SortStat st;
    int all_pass = 1;      /* 最后统一汇报：所有算法是否都排对了 */

    /* ============================================================
       第 1 幕：先搞懂「稳定」这两个字到底在说什么
       ============================================================ */
    section("1. 稳定是什么？为什么它不是可有可无的形容词");

    printf("  场景：5 个学生的分数，同分的人希望【保持报名时的先后顺序】。\n");
    printf("        下标就是报名顺序，打成 a b c d e：\n");

    int score[5]   = {88, 95, 88, 70, 95};
    int name_id[5] = {0, 1, 2, 3, 4};
    print_tagged("      原始（报名顺序）: ", score, name_id, 5);
    printf("        即：88 分的是 a 和 c，95 分的是 b 和 e。\n\n");

    int s2[5], t2[5];
    copy_arr(s2, score, 5);
    copy_arr(t2, name_id, 5);
    insertion_sort_tagged(s2, t2, 5);
    print_tagged("      插入排序之后:     ", s2, t2, 5);
    printf("        → 88 分还是 a 在 c 前，95 分还是 b 在 e 前 ⇒ 稳定\n\n");

    int s3[5], t3[5];
    copy_arr(s3, score, 5);
    copy_arr(t3, name_id, 5);
    selection_sort_tagged(s3, t3, 5);
    print_tagged("      选择排序之后:     ", s3, t3, 5);
    printf("        → 分数完全一样，但 88 组变成 c 在 a 前 ⇒ 不稳定\n");
    printf("  定义：稳定 = 相等元素的相对顺序，排序前后不变。\n");
    printf("  为什么重要：真实业务里「分数」常常只是第一关键字，第二关键字（报名顺序/学号）\n");
    printf("            还要留着。多条件排序可以靠【从弱关键字到强关键字，反复用稳定排序】叠出来；\n");
    printf("            一旦中间用了不稳定排序，上一趟排好的顺序就被冲光。\n");

    /* ============================================================
       第 2 幕：冒泡 —— 两个优化都跑给你看
       ============================================================ */
    section("2. 冒泡排序：每趟把「还没排好的最大值」冒到最后");

    copy_arr(work, ORIG, N);
    print_arr("      排之前: ", work, N);
    printf("      （[] 圈住的是已经排好的后缀；每趟之后打印整个数组）\n");
    bubble_sort_trace(work, N, 1, &st);
    report_stat("冒泡", &st);
    printf("      校验：确实升序 = %s，与标准答案一致 = %s\n",
           is_sorted_ascending(work, N) ? "是" : "否",
           same_elements(work, EXPECT, N) ? "是" : "否");
    if (!is_sorted_ascending(work, N) || !same_elements(work, EXPECT, N)) {
        all_pass = 0;
    }

    printf("\n      【优化①】内层上界每趟 -1：尾巴那几格已经排好，不再碰。\n");
    printf("                比较次数上限 7+6+5+4+3+2+1 = 28，而不是无脑全比的 7×7 = 49。\n");

    printf("\n      【优化②】一轮没发生交换就提前结束 —— 喂一个【已经有序】的数组：\n");
    const int SORTED[N] = {1, 2, 3, 4, 5, 6, 7, 8};
    copy_arr(work, SORTED, N);
    bubble_sort_trace(work, N, 1, &st);
    report_stat("冒泡(已有序)", &st);
    printf("      只跑 1 趟、%ld 次比较、0 次交换就收工 ⇒ 最好情况真的是 O(n)\n", st.cmp);

    printf("\n      再喂一个【只差一张牌】的近有序数组：\n");
    const int NEARLY[N] = {1, 2, 4, 3, 5, 6, 7, 8};
    copy_arr(work, NEARLY, N);
    bubble_sort_trace(work, N, 1, &st);
    report_stat("冒泡(近有序)", &st);

    /* ============================================================
       第 3 幕：选择 —— 交换最少，但它是三兄弟里唯一不稳定的
       ============================================================ */
    section("3. 选择排序：每趟只交换 1 次，代价全押在比较上");

    copy_arr(work, ORIG, N);
    print_arr("      排之前: ", work, N);
    selection_sort_trace(work, N, 1, &st);
    report_stat("选择", &st);
    printf("      不管数据长什么样，选择排序的交换次数最多就是 %d 次（每趟 1 次）。\n", N - 1);
    printf("      冒泡/插入在最坏数据下要挪 28 次。所以「交换比比较贵得多」的场景 ——\n");
    printf("      每条记录几 KB、或者数据在磁盘上、写一次闪存很贵 —— 就选它。\n");
    if (!is_sorted_ascending(work, N) || !same_elements(work, EXPECT, N)) {
        all_pass = 0;
    }

    printf("\n      【它为什么不稳定？】最小的反例只要 3 个数：5a 5b 2\n");
    int tiny_key[3] = {5, 5, 2};
    int tiny_tag[3] = {0, 1, 2};
    print_tagged("        排之前: ", tiny_key, tiny_tag, 3);
    selection_sort_tagged(tiny_key, tiny_tag, 3);
    print_tagged("        排之后: ", tiny_key, tiny_tag, 3);
    printf("        第一趟把「下标 0 的 5a」和「最小值 2」远距离交换，\n");
    printf("        5a 被扔到最后一格，落到原本在它【后面】的 5b 之后 ⇒ 顺序反了。\n");

    /* ============================================================
       第 4 幕：插入 —— 理扑克牌
       ============================================================ */
    section("4. 插入排序：摸一张新牌，从右往左比，比它大的往后挪");

    copy_arr(work, ORIG, N);
    print_arr("      排之前: ", work, N);
    printf("      （[] 圈住的是左手已经理好的牌）\n");
    insertion_sort_trace(work, N, 1, &st);
    report_stat("插入", &st);
    if (!is_sorted_ascending(work, N) || !same_elements(work, EXPECT, N)) {
        all_pass = 0;
    }

    printf("\n      【为什么近乎有序时它几乎是 O(n)】三种数据只统计不打印：\n");
    SortStat st_near, st_rand, st_worst;
    copy_arr(work, NEARLY, N);
    insertion_sort_trace(work, N, 0, &st_near);
    copy_arr(work, ORIG, N);
    insertion_sort_trace(work, N, 0, &st_rand);
    const int REVERSE[N] = {8, 7, 6, 5, 4, 3, 2, 1};   /* 完全逆序 = 插入的最坏情况 */
    copy_arr(work, REVERSE, N);
    insertion_sort_trace(work, N, 0, &st_worst);
    printf("      近有序 [1 2 4 3 5 6 7 8]："); report_stat("插入", &st_near);
    printf("      乱序   [5 2 9 1 7 3 8 4]："); report_stat("插入", &st_rand);
    printf("      逆序   [8 7 6 5 4 3 2 1]："); report_stat("插入", &st_worst);
    printf("      比较次数 8 → 18 → 28：近有序时新牌往左比一两下就落位，内层循环几乎不进。\n");
    printf("      它的总代价 ≈ 逆序对个数。数据大致有序 ⇒ 逆序对 ≈ O(n) ⇒ 总代价 ≈ O(n)。\n");
    printf("      所以标准库里的小数组都拿它兜底（V8 的 Array#sort、Python 的 Timsort）：\n");
    printf("      代码 5 行、无递归、零额外内存、相邻访问缓存友好，而且真实数据常常大致有序。\n");

    /* ============================================================
       第 5 幕：三个算法同台对比 + 正确性总校验
       ============================================================ */
    section("5. 同一批数据 [5 2 9 1 7 3 8 4]，三个算法的活量对比");

    int b[N], s[N], ins[N];
    SortStat st_b, st_s, st_i;
    copy_arr(b, ORIG, N);
    copy_arr(s, ORIG, N);
    copy_arr(ins, ORIG, N);
    bubble_sort_trace(b, N, 0, &st_b);
    selection_sort_trace(s, N, 0, &st_s);
    insertion_sort_trace(ins, N, 0, &st_i);

    printf("      %-8s %-10s %-16s %s\n", "算法", "比较次数", "挪动/交换次数", "提前收工");
    printf("      ------------------------------------------------------------\n");
    printf("      %-8s %-10ld %-16ld %s\n", "冒泡", st_b.cmp, st_b.swap, st_b.early_exit ? "是" : "否");
    printf("      %-8s %-10ld %-16ld %s\n", "选择", st_s.cmp, st_s.swap, st_s.early_exit ? "是" : "否");
    printf("      %-8s %-10ld %-16ld %s\n", "插入", st_i.cmp, st_i.swap, st_i.early_exit ? "是" : "否");

    printf("\n      工程版（无打印无计数）也验一遍，防止教学版和工程版跑偏：\n");
    int c1[N], c2[N], c3[N];
    copy_arr(c1, ORIG, N);
    copy_arr(c2, ORIG, N);
    copy_arr(c3, ORIG, N);
    bubble_sort(c1, N);
    selection_sort(c2, N);
    insertion_sort(c3, N);
    print_arr("      bubble_sort    : ", c1, N);
    print_arr("      selection_sort : ", c2, N);
    print_arr("      insertion_sort : ", c3, N);
    if (!same_elements(c1, EXPECT, N) || !same_elements(c2, EXPECT, N) || !same_elements(c3, EXPECT, N)) {
        all_pass = 0;
    }

    printf("\n      边界情况（这几个都是最容易写出越界 bug 的地方）：\n");
    int zero_len[1] = {42};
    insertion_sort(zero_len, 0);          /* n = 0：循环条件一次都不成立，不能崩 */
    bubble_sort(zero_len, 0);
    selection_sort(zero_len, 0);
    printf("      n=0：三个函数都安全返回（没进循环）\n");

    int one[1] = {42};
    selection_sort(one, 1);
    print_arr("      n=1：", one, 1);

    int two[2] = {9, 1};
    bubble_sort(two, 2);
    print_arr("      n=2：", two, 2);

    int dup[4] = {7, 7, 7, 7};
    insertion_sort(dup, 4);
    print_arr("      全相同元素：", dup, 4);

    /* 排序不能把元素弄丢、也不能凭空变出别的数 */
    int perm_check[N];
    copy_arr(perm_check, ORIG, N);
    insertion_sort(perm_check, N);
    if (!is_permutation(ORIG, perm_check, N)) {
        all_pass = 0;
    }
    printf("      元素守恒检查（没丢数、没变数）：%s\n",
           is_permutation(ORIG, perm_check, N) ? "通过" : "失败");

    printf("\n=== 本节结束 ===\n");
    printf("总校验：%s\n",
           all_pass ? "三个算法（工程版 + 教学版）结果全部正确，且与标准答案一致"
                    : "有算法结果不对，去看代码");
    printf("记住三句话：\n");
    printf("  1) 冒泡两个优化：内层上界每趟 -1；一趟没交换就收工（⇒ 最好 O(n)）。\n");
    printf("  2) 选择把代价全押在比较上，交换最多 n-1 次 —— 交换贵时用它，代价是不稳定。\n");
    printf("  3) 插入的代价 = 逆序对个数，近有序数据近乎线性，所以它是小数组的兜底之王。\n");
    return 0;
}
