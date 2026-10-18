#include <stdio.h>
#include <stdlib.h>
#include "merge_sort.h"

#define DEMO_N 7
#define INV_N 6

/* 小工具：打印分隔线 + 标题 */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/* 复制数组：每一幕都从同一份原始数据重新开始，输出才可比 */
static void copy_arr(int dst[], const int src[], int n)
{
    for (int i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

/* 自己算整数版 log2（不引 math.h，因为仓库的编译命令没有 -lm）：x 是 2 的几次方 */
static int ilog2_int(int x)
{
    int r = 0;
    while (x > 1) {
        x /= 2;
        r++;
    }
    return r;
}

int main(void)
{
    /* 全场主数据：教材里最常用的那 7 个数，硬编码，保证每次输出一模一样 */
    const int ORIG[DEMO_N] = {38, 27, 43, 3, 9, 82, 10};
    const int EXPECT[DEMO_N] = {3, 9, 10, 27, 38, 43, 82};   /* 手算标准答案 */

    int work[DEMO_N];
    MergeStat st;
    int all_pass = 1;

    /* ============================================================
       第 1 幕：分治是什么 —— 拆到只剩一个元素，再两两合
       ============================================================ */
    section("1. 分治：拆到只剩 1 个元素（天然有序），然后两两合并");

    printf("  为什么\"只剩一个元素\"就不用排了？因为一个数没有任何\"前后顺序\"可违反。\n");
    printf("  于是一个谁都不服的乱数组，被拆成一堆\"自己无敌\"的小段，\n");
    printf("  再【两两合并】：合并两个有序段是可以一次到位的，合完段宽翻倍：\n\n");
    printf("      [38 27 43 3 | 9 82 10]              ← 原始：谁都不服\n");
    printf("        /                 \\\n");
    printf("   [38 27 43 3]      [9 82 10]            ← 各拆一半\n");
    printf("     /      \\         /     \\\n");
    printf("  [38 27]  [43 3]  [9 82]  [10]           ← 继续拆\n");
    printf("   / \\      / \\     / \\\n");
    printf(" [38][27] [43][3]  [9][82]                ← 全是 1 个元素：天然有序\n");
    printf("   ↓ 合   ↓ 合      ↓ 合\n");
    printf(" [27 38] [3 43]   [9 82]                  ← 合并两个有序段\n");
    printf("     \\  /          / \\\n");
    printf("    [3 27 38 43]  [9 82]\n");
    printf("         \\        /\n");
    printf("        [3 9 27 38 43 82] + [10] → 最终结果\n\n");
    printf("  一共几层？段宽 1→2→4→8，取 log 层；每层总工作量 O(n) ⇒ 整体 O(n log n)。\n");

    /* ============================================================
       第 2 幕：单独看发动机 —— 只喂两个有序段给它
       ============================================================ */
    section("2. 发动机 merge_ranges：两个指针各看自己那半边，谁小取谁");

    printf("  它【不排序】，只做一件事：把两个【已经各自有序】的相邻段并成一个。\n");
    int demo[6] = {5, 8, 12, 3, 6, 20};     /* 左段 [5 8 12]，右段 [3 6 20] */
    int demo_tmp[6];
    print_arr("      喂进去: ", demo, 6);
    printf("      左段 = [0..2]，右段 = [3..5]，看它怎么合：\n");
    merge_ranges(demo, demo_tmp, 0, 2, 5, NULL, 1);
    print_arr("      合完: ", demo, 6);

    printf("\n      再来一个\"右段全部比左段小\"的极端例子（考验最后那段直接抄）：\n");
    int demo2[6] = {50, 60, 70, 1, 2, 3};
    int demo2_tmp[6];
    print_arr("      喂进去: ", demo2, 6);
    merge_ranges(demo2, demo2_tmp, 0, 2, 5, NULL, 1);
    print_arr("      合完: ", demo2, 6);

    printf("\n      再来一个\"左段全部比右段小\"：右段一个都没取过，最后整段抄回来。\n");
    int demo3[6] = {1, 2, 3, 50, 60, 70};
    int demo3_tmp[6];
    merge_ranges(demo3, demo3_tmp, 0, 2, 5, NULL, 1);
    print_arr("      合完: ", demo3, 6);

    printf("      观察：6 个数的合并最多比 5 次（两边各剩最后 1 个时那一次比较定生死），\n");
    printf("            合完 n 个数最多比较 n-1 次 —— 这就是\"归并的比较次数比快排少\"的原因。\n");

    /* ============================================================
       第 3 幕：完整递归 trace —— 拆 / 合 全过程可见
       ============================================================ */
    section("3. 完整过程（数据 [38 27 43 3 9 82 10]，每次合并逐格打印）");

    copy_arr(work, ORIG, DEMO_N);
    merge_sort_trace(work, DEMO_N, 1, &st);
    print_arr("      最终结果: ", work, DEMO_N);
    printf("      统计：比较 %ld 次 | 抄写 %ld 格 | 合并 %ld 次\n", st.cmp, st.copy, st.merges);
    if (!same_elements(work, EXPECT, DEMO_N) || !is_sorted_ascending(work, DEMO_N)) {
        all_pass = 0;
    }
    printf("      校验：与标准答案一致 = %s\n", same_elements(work, EXPECT, DEMO_N) ? "是" : "否");

    /* ============================================================
       第 4 幕：自底向上版 —— 每层的整体快照
       ============================================================ */
    section("4. 自底向上（不递归）：按层合并，每层打印整个数组");

    printf("  这一版把\"每一层\"的概念摊开给你看：宽度 1 → 2 → 4，每合完一轮打印一次。\n");
    copy_arr(work, ORIG, DEMO_N);
    print_arr("      原始: ", work, DEMO_N);
    MergeStat st_bu;
    merge_sort_bottom_up(work, DEMO_N, 1, &st_bu);
    print_arr("      最终: ", work, DEMO_N);
    printf("      统计：比较 %ld 次 | 抄写 %ld 格 | 合并 %ld 次\n", st_bu.cmp, st_bu.copy, st_bu.merges);
    if (!same_elements(work, EXPECT, DEMO_N)) {
        all_pass = 0;
    }
    printf("      校验：两种写法结果一致 = %s\n", same_elements(work, EXPECT, DEMO_N) ? "是" : "否");

    /* ============================================================
       第 5 幕：稳定性实验 —— 相等元素到底有没有被换乱
       ============================================================ */
    section("5. 实测稳定性：给相等元素带上原始序号，排完看序号顺序");

    printf("  6 个学生的分数（88、95 都有重复），tag 就是他们的原始下标 a b c d e f：\n");
    int score[6] = {88, 95, 88, 70, 95, 88};
    int tag[6]   = {0, 1, 2, 3, 4, 5};
    print_tagged("      排之前: ", score, tag, 6);
    if (!merge_sort_tagged(score, tag, 6)) {
        printf("      [错误] 内存申请失败\n");
    }
    print_tagged("      排之后: ", score, tag, 6);
    printf("      检查同分组：88 分的是 a、c、f ⇒ 排完还是 a 在 c 前、c 在 f 前；\n");
    printf("                95 分的是 b、e ⇒ 还是 b 在 e 前 ⇒ 【稳定】。\n");
    printf("      代码里靠的是哪一行？merge 里那句 `tmp_key[i] <= tmp_key[j]`：\n");
    printf("      一样大时【优先取左边】，左边是原数组靠前的那一段，所以先来的人先出站。\n");
    printf("      改成 < 会怎样：输出顺序照样升序，但同分的人会互换 —— 一行等号决定稳定性。\n");

    /* ============================================================
       第 6 幕：归并思想的应用 —— 数逆序对
       ============================================================ */
    section("6. 用归并思想统计逆序对：右边的人先出队 ⇒ 左边剩几个就加几");

    int inv[INV_N] = {5, 3, 2, 8, 1, 4};
    print_arr("      数组: ", inv, INV_N);
    printf("      每出现一次\"右段先出队\"，就打印它贡献了哪些对：\n");
    long fast = count_inversions(inv, INV_N, 1);
    long slow = count_inversions_bruteforce(inv, INV_N);
    printf("      归并算法数出来：%ld 对\n", fast);
    printf("      O(n²) 笨办法数出来：%ld 对（两两全查，绝对不会想错，用来验货）\n", slow);
    if (fast != slow) {
        all_pass = 0;
    }
    printf("      两者一致 = %s\n", fast == slow ? "是" : "否");

    printf("\n      已经有序的数组逆序对应该是 0，完全逆序应该是 n(n-1)/2：\n");
    int asc[6] = {1, 2, 3, 4, 5, 6};
    int desc[6] = {6, 5, 4, 3, 2, 1};
    printf("      [1 2 3 4 5 6] 逆序对 = %ld（期望 0）\n", count_inversions(asc, 6, 0));
    printf("      [6 5 4 3 2 1] 逆序对 = %ld（期望 15）\n", count_inversions(desc, 6, 0));
    if (count_inversions(asc, 6, 0) != 0 || count_inversions(desc, 6, 0) != 15) {
        all_pass = 0;
    }

    /* ============================================================
       第 7 幕：规模验证 + 边界
       ============================================================ */
    section("7. 规模验证：比较次数是不是 O(n log n)？外加各种边界");

    /* 用公式造 256 / 1024 / 4096 个确定值（不是随机），所以输出可复核 */
    const int SIZES[3] = {256, 1024, 4096};  /* 固定规模：不是随机，输出可复核 */
    printf("      %-8s %-14s %-14s %s\n", "n", "实测比较次数", "n·log2(n)", "抄写格数");
    printf("      ----------------------------------------------------------\n");
    for (int s = 0; s < 3; s++) {
        int n = SIZES[s];
        int *big = malloc((size_t)n * sizeof(int));
        int *expect_box = malloc((size_t)n * sizeof(int));
        if (big == NULL || expect_box == NULL) {
            printf("      [错误] 内存申请失败，跳过 n=%d\n", n);
            free(big);
            free(expect_box);
            continue;
        }
        for (int i = 0; i < n; i++) {
            big[i] = (i * 37) % n;           /* 确定的\"乱序\"，不是随机 */
        }
        MergeStat big_st;
        merge_sort_trace(big, n, 0, &big_st);
        for (int i = 0; i < n; i++) {
            expect_box[i] = i;               /* (i*37)%n 是 0..n-1 的一个排列，排完必然是 0..n-1 */
        }
        printf("      %-8d %-14ld %-14d %ld\n", n, big_st.cmp, n * ilog2_int(n), big_st.copy);
        if (!is_sorted_ascending(big, n) || !same_elements(big, expect_box, n)) {
            all_pass = 0;
            printf("        ↑ n=%d 校验失败！\n", n);
        }
        free(big);  big = NULL;
        free(expect_box); expect_box = NULL;
    }
    printf("      实测比较次数稳定在 n·log2(n) 的 0.75 ~ 0.85 倍，抄写格数正好 = n·log2(n) 层 × n。\n");
    printf("      （n 变成 4 倍，n·log n 变成约 4×1.07 倍 —— 这就是 O(n log n) 的手感）\n");

    printf("\n      边界情况：\n");
    int zero[1] = {99};
    merge_sort(zero, 0);                     /* n=0：连 malloc 都不该发生 */
    printf("      n=0：安全返回（入口就 return，没申请任何内存）\n");
    int one[1] = {42};
    merge_sort(one, 1);
    print_arr("      n=1：", one, 1);
    int two[2] = {7, 3};
    merge_sort(two, 2);
    print_arr("      n=2：", two, 2);
    int samev[5] = {6, 6, 6, 6, 6};
    MergeStat st_same;
    merge_sort_trace(samev, 5, 0, &st_same);
    printf("      全相同 [6 6 6 6 6]：");
    print_arr("", samev, 5);
    printf("                      比较 %ld 次（<= 让左边全部优先出站，一次都不吃亏）\n", st_same.cmp);
    int already[DEMO_N] = {3, 9, 10, 27, 38, 43, 82};
    MergeStat st_done;
    merge_sort_trace(already, DEMO_N, 0, &st_done);
    int rev[DEMO_N] = {82, 43, 38, 27, 10, 9, 3};
    MergeStat st_rev;
    merge_sort_trace(rev, DEMO_N, 0, &st_rev);
    printf("      已经有序的：比较 %ld 次 | 完全逆序的：比较 %ld 次\n", st_done.cmp, st_rev.cmp);
    printf("      反常识的一点：完全逆序时比较【更少】，因为右段整段先出队，左段剩下直接抄。\n");
    printf("      但两种数据的【抄写量和层数】完全一样 ⇒ 归并不看数据脸色：最好 = 平均 = 最坏 = O(n log n)。\n");

    printf("\n=== 本节结束 ===\n");
    printf("总校验：%s\n",
           all_pass ? "递归版/自底向上版结果全部与标准答案一致，逆序对与笨办法一致"
                    : "有不一致，去看代码");
    printf("记住四句话：\n");
    printf("  1) 拆到 1 个元素就停 —— 它天然有序；合并是唯一的干活环节，两个指针谁小取谁。\n");
    printf("  2) 一定要先抄进 tmp：往 a 里覆盖式写之前不抄走，就会把还没读的数据盖掉。\n");
    printf("  3) 相等时【优先取左边】（<=）⇒ 稳定。这一行就是稳定性的全部来源。\n");
    printf("  4) 合并时右段先出队 ⇒ 左段剩余个数就是新增逆序对数 —— 归并思想的经典应用。\n");
    return 0;
}
