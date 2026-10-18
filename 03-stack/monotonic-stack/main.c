#include <stdio.h>
#include <stdlib.h>
#include "mstack.h"

/*
 * main.c —— 单调栈的四幕演示
 *
 * 编译运行（在项目根目录）：
 *   make one DIR=03-stack/monotonic-stack
 */

static void scene(const char *title)
{
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n");
}

/*
 * 暴力法求「下一个更大元素」，顺便数清比较次数，用来和第 1 幕的单调栈对比。
 * 每个 i 都往右一个个看，看到比自己大的就收下答案 break。
 * 它没有任何「复用」机制：同一个数会被右边的人反复比很多次 —— 这就是 O(n²) 的来源。
 */
static void brute_force_nge(const int *nums, int n, int *ans_out, int *comparisons_out)
{
    int comparisons = 0;
    for (int i = 0; i < n; i++) {
        ans_out[i] = -1;
        for (int j = i + 1; j < n; j++) {
            comparisons++;
            if (nums[j] > nums[i]) {
                ans_out[i] = nums[j];
                break;              /* 找到「第一个」就够了 */
            }
        }
    }
    *comparisons_out = comparisons;
}

int main(void)
{
    /* ============================================================
       第 1 幕：下一个更大元素 —— 栈的变化 + 每个答案怎么产生的
       ============================================================ */
    scene("第 1 幕：应用①「下一个更大元素」（逐步打印栈的变化）");

    const int nums[] = {5, 3, 8, 1, 4, 7, 2};
    const int n = (int)(sizeof(nums) / sizeof(nums[0]));

    /* ans 要在堆上：n 由数组自己算出来，长度不定 → malloc，最后 free */
    int *ans = malloc((size_t)n * sizeof(int));
    if (ans == NULL) {
        printf("  [错误] malloc 答案数组失败\n");
        return 1;
    }

    mstack_next_greater(nums, n, ans);

    printf("\n  对着上面重讲一遍这幕的三个看点：\n");
    printf("    1) 栈里存的永远是【下标】。弹出下标 j、写入 ans[j] = 新来的值 —— 位置必须靠下标。\n");
    printf("    2) 「一个元素被弹出去」的那一刻，就是它的下一个更大元素被找到的那一刻。\n");
    printf("    3) 扫完还留在栈里的元素，答案保持 -1：它们右边确实没有更大的了。\n");

    free(ans);                        /* 谁 malloc 谁 free */
    ans = NULL;

    /* ============================================================
       第 2 幕：极端情况 —— 整个数组递减
       ============================================================ */
    scene("第 2 幕：极端情况：数组一路递减（栈会涨到最长，答案全是 -1）");

    const int desc[] = {9, 7, 5, 3, 1};
    const int nd = (int)(sizeof(desc) / sizeof(desc[0]));
    int *ans_desc = malloc((size_t)nd * sizeof(int));
    if (ans_desc == NULL) {
        printf("  [错误] malloc 失败\n");
        return 1;
    }

    printf("  这种输入下 while 一次都不会执行（新值总比栈顶小），栈从 1 个涨到 %d 个：\n", nd);
    printf("  → 空间 O(n) 就是这么来的；时间上更省，只有 n 次入栈、0 次出栈。\n\n");
    mstack_next_greater(desc, nd, ans_desc);
    printf("  再看一个反面的极端：一路递增 1 2 3 4 —— 每来一个新值就把栈清空一次，"
           "但每次只弹 1 个，总共也只弹 n-1 次。\n");

    free(ans_desc);
    ans_desc = NULL;

    /* ============================================================
       第 3 幕：柱状图接雨水（简单版）
       ============================================================ */
    scene("第 3 幕：应用②「柱状图接雨水」—— 逐层结算，打印每根柱子接到的水");

    const int short_plan[] = {4, 2, 0, 3, 2, 5};   /* 小例子：逐列水量 0,2,4,1,2,0，总量 9 */
    const int ns = (int)(sizeof(short_plan) / sizeof(short_plan[0]));
    printf("  【例 1】先拿一个短的看清楚（逐列验一下：0+2+4+1+2+0，总量应该是 9）\n");
    mstack_trapping_rain_water(short_plan, ns);

    printf("\n  【例 2】经典例子 LeetCode 42（答案总量应该是 6）\n");
    const int lc42[] = {0, 1, 0, 2, 1, 0, 1, 3, 2, 1, 2, 1};
    const int nl = (int)(sizeof(lc42) / sizeof(lc42[0]));
    mstack_trapping_rain_water(lc42, nl);

    /* ============================================================
       第 4 幕：为什么是 O(n)：把「操作次数」真的数出来对比
       ============================================================ */
    scene("第 4 幕：单调栈凭什么是 O(n) —— 和暴力法数同样的输入");

    /* 三组同样长度（n=10）但形状不同的输入，专门用来把「暴力 vs 单调栈」的差距摆出来 */
    const int desc_case[] = {10, 9, 8, 7, 6, 5, 4, 3, 2, 1};   /* 暴力的最坏情况 */
    const int asc_case[]  = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};   /* 暴力的最好情况 */
    const int mix_case[]  = {5, 3, 8, 1, 4, 7, 2, 6, 9, 0};    /* 一般情况 */

    const int *cases[3]    = {desc_case, asc_case, mix_case};
    /* 名字都取 4 个汉字（终端里正好 8 列），这样表格才对得齐 —— 中文列宽是个真实坑 */
    const char *case_names[3] = {"递减序列", "递增序列", "乱序序列"};

    const int nc = (int)(sizeof(desc_case) / sizeof(desc_case[0]));

    int *ans_mono = malloc((size_t)nc * sizeof(int));
    int *ans_brute = malloc((size_t)nc * sizeof(int));
    if (ans_mono == NULL || ans_brute == NULL) {
        printf("  [错误] malloc 失败\n");
        free(ans_mono);
        free(ans_brute);
        return 1;
    }

    printf("  两种做法都跑同样的输入（静默版，不打印过程），把操作次数数出来：\n");
    printf("  输入长度 n = %d\n\n", nc);
    printf("    输入形状    暴力比较次数   单调栈入栈  出栈   合计   答案是否一致\n");
    printf("    ---------  ------------  --------  ----  -----  ----------\n");

    for (int c = 0; c < 3; c++) {
        int comparisons = 0;
        int pushes = 0;
        int pops = 0;

        brute_force_nge(cases[c], nc, ans_brute, &comparisons);
        mstack_next_greater_counted(cases[c], nc, ans_mono, &pushes, &pops);

        /* 校验两者答案完全一致 —— 这是「单调栈没写错」最硬的证据 */
        bool same = true;
        for (int i = 0; i < nc; i++) {
            if (ans_mono[i] != ans_brute[i]) {
                same = false;
            }
        }

        printf("    %s   %6d 次      %6d  %6d  %6d   %s\n",
               case_names[c], comparisons, pushes, pops, pushes + pops,
               same ? "一致 ✔" : "不一致 ✘");
    }

    printf("\n  读这张表的三个要点：\n");
    printf("    1) 【递减序列】是暴力法的最坏情况：每个数都要一路看到数组末尾才发现没有更大的，\n");
    printf("       比较次数 = n(n-1)/2 = %d，正好是 n=10 时的理论上限；单调栈只做了 %d 次入栈、0 次出栈。\n",
           nc * (nc - 1) / 2, nc);
    printf("    2) 【一路递增】反过来：暴力法每次看一眼就 break，比较 n-1 = %d 次；\n", nc - 1);
    printf("       单调栈要把前一个全部弹光才压新的，出栈 n-1 次 → 合计 %d 次。所以「比大小」不是重点，\n", 2 * nc - 1);
    printf("       重点是【上界】：暴力的最坏是 n²/2 量级，单调栈永远是 2n 量级，谁都会被谁压住。\n");
    printf("    3) 入栈次数【永远等于 n】（每个下标恰好被压一次），出栈次数【永远 ≤ n】。\n");
    printf("       n=1000 时：暴力最坏约 50 万次比较，单调栈最多 2000 次操作。\n");
    printf("\n  把道理说透：\n");
    printf("    · 外层 for 一定走 n 步 —— 这是固定的。\n");
    printf("    · 内层 while 只在【弹栈】，而每个下标一生最多被弹 1 次（弹掉就再也不回来了）。\n");
    printf("    · 所以内层「总共」最多执行 n 次。单次可能弹好几个，但全局有上限 —— 这就叫【摊还 O(1)】，\n");
    printf("      合起来就是 O(n)。数「总操作次数」而不是数「两层循环」，是分析单调栈的唯一正确姿势。\n");
    printf("\n  为什么栈里必须存下标而不是值？\n");
    printf("    如果存值，弹出时只知道「这个数有了答案」，却不知道它原来在第几格，\n");
    printf("    ans[] 就没法写；接雨水那题更极端，宽度 = i - left - 1 全靠下标相减。\n");
    printf("    值随时能用 nums[idx] 取回来，下标一丢就彻底找不回来 —— 所以存下标。\n");
    printf("\n  递减栈 / 递增栈怎么选？\n");
    printf("    求「右边第一个更大」→ 递减栈（新值大就弹小的，本幕演示的）\n");
    printf("    求「右边第一个更小」→ 递增栈（把比较号反过来写 nums[top] > nums[i] 就行）\n");
    printf("    求「左边第一个更大」→ 还是递减栈，从右往左扫；或者柱状图最大矩形那种双向用法\n");

    free(ans_mono);
    ans_mono = NULL;                  /* 置空，防止后面手滑再读它 */
    free(ans_brute);
    ans_brute = NULL;

    printf("\n=== 本节结束 ===\n");
    printf("记住四句话：\n");
    printf("  1) 单调栈 = 普通栈 + 「栈内保持单调」这条纪律；纪律是靠 push 前不停地弹栈维持的。\n");
    printf("  2) 弹栈那一下通常就是答案诞生的那一下（谁被弹出，谁就找到了它要的答案）。\n");
    printf("  3) 栈里存【下标】不存值：位置和宽度都要靠下标算，值可以 nums[idx] 现取。\n");
    printf("  4) 每个下标最多进出各一次 → 总操作 ≤ 2n → O(n)。别盯着两层循环就喊 O(n²)。\n");

    return 0;
}
