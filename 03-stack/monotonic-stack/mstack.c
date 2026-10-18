#include <stdio.h>
#include <stdlib.h>   /* malloc / calloc / free */
#include "mstack.h"

/*
 * ===================================================================
 * mstack.c —— 单调栈：小栈 + 两个应用
 * ===================================================================
 * 先看图：求「每个元素的下一个更大元素」，nums = [5, 3, 8, 1, 4, 7, 2]
 *
 *   栈里存的是【下标】，从栈底到栈顶保证 nums[下标] 一路【递减】。
 *   为什么是递减栈？因为「新来的数比栈顶大」这件事，正是栈顶在等的那个答案。
 *
 *   i=0  来了 5 ：栈空，直接压            栈(底→顶): (0:5)
 *   i=1  来了 3 ：3 < 5，不破坏递减，压    栈: (0:5) (1:3)
 *   i=2  来了 8 ：8 > 3 → 弹出 1，ans[1]=8
 *                8 > 5 → 弹出 0，ans[0]=8
 *                栈空 → 压 2               栈: (2:8)
 *   i=3  来了 1 ：1 < 8，压                栈: (2:8) (3:1)
 *   i=4  来了 4 ：4 > 1 → 弹出 3，ans[3]=4；4 < 8，停；压 4   栈: (2:8) (4:4)
 *   i=5  来了 7 ：7 > 4 → 弹出 4，ans[4]=7；7 < 8，停；压 5   栈: (2:8) (5:7)
 *   i=6  来了 2 ：2 < 7，压                                    栈: (2:8) (5:7) (6:2)
 *   扫完了，栈里还剩 (2:8)(5:7)(6:2) 三个 → 它们右边【没有】更大的，ans 保持 -1
 *
 *   答案：ans = [8, 8, -1, 4, 7, -1, -1]
 *
 *   ★ 关键画面：一个元素被弹出 = 它的「下一个更大元素」被找到的那一瞬间。
 *     所以「弹栈」不只是维护单调性，它本身就是写答案的动作。
 *
 * 复杂度为什么是 O(n)：
 *   外层 for 固定 n 步；内层 while 每执行一次就【弹掉一个下标】。
 *   下标总共只有 n 个，每个一生最多入栈 1 次、出栈 1 次
 *   → 全部 while 加起来最多执行 n 次 → 总操作 ≤ 2n = O(n)。
 *   暴力法（每个 i 往右扫）最坏要 n + (n-1) + … = O(n²)，而且没有任何回收机制。
 *   下面两个函数都自己数了 push / pop 次数，运行结果末尾会打印，你可以亲眼核对。
 */

/*
 * -------------------------------------------------------------------
 * MiniStack 的实现：就是「数组栈流派 A」的最小版
 * -------------------------------------------------------------------
 * 为什么这里再写一遍，而不是 include 03-stack/array-stack/astack.h？
 *   1) 硬性约定：每个叶子目录必须自包含，Makefile 是按目录独立编译的，
 *      跨目录 #include 根本编不过（-I 路径都没有）。
 *   2) 单调栈不需要扩容开关：栈里最多同时装 n 个下标，一开始 malloc(n) 就绝对够，
 *      少一个失败分支，读代码时更聚焦在「单调性」上。
 * -------------------------------------------------------------------
 */
void mini_init(MiniStack *s, int capacity)
{
    s->data = NULL;
    s->capacity = 0;
    s->top = -1;                       /* 流派 A：空栈 top = -1 */

    if (capacity <= 0) {
        printf("      [警告] mini_init 容量非法(%d)，建成空壳\n", capacity);
        return;
    }
    s->data = malloc((size_t)capacity * sizeof(int));
    if (s->data == NULL) {
        printf("      [错误] mini_init malloc %d 个 int 失败\n", capacity);
        return;
    }
    s->capacity = capacity;
}

void mini_destroy(MiniStack *s)
{
    if (s == NULL) {
        return;
    }
    free(s->data);
    s->data = NULL;                    /* 撕掉纸条，防悬空指针 */
    s->capacity = 0;
    s->top = -1;
}

bool mini_push(MiniStack *s, int value)
{
    if (s->data == NULL || s->top + 1 >= s->capacity) {
        /* demo 里提前算好了容量，理论上走不到；走到了就是 bug，明确报出来 */
        printf("      [BUG] mini_push 溢出：capacity=%d top=%d\n", s->capacity, s->top);
        return false;
    }
    s->top += 1;                       /* 先挪指针 */
    s->data[s->top] = value;           /* 再写数据 */
    return true;
}

bool mini_pop(MiniStack *s, int *out_value)
{
    if (s->top < 0) {
        return false;                  /* 空栈：不写 out_value */
    }
    int v = s->data[s->top];
    s->top -= 1;
    if (out_value != NULL) {
        *out_value = v;
    }
    return true;
}

bool mini_peek(const MiniStack *s, int *out_value)
{
    if (s == NULL || s->top < 0 || s->data == NULL) {
        return false;
    }
    if (out_value != NULL) {
        *out_value = s->data[s->top];
    }
    return true;
}

bool mini_is_empty(const MiniStack *s)
{
    return s == NULL || s->top < 0;
}

int mini_size(const MiniStack *s)
{
    return (s == NULL || s->top < 0) ? 0 : s->top + 1;
}

/*
 * 把栈里的「下标」连同 nums[下标] 的值一起打出来，形如：
 *     栈(底→顶): (0:5) (1:3)          ← 递减栈：值一路变小
 * 为什么要连值一起打？因为单调栈的纪律是加在【值】上的，我们存的却是【下标】，
 * 只打下标看不出单调性；把 nums[下标] 一起显示，一眼就能验证「是不是真的递减」。
 * ⚠ 这是教学打印，正式的单调栈实现不会遍历整个栈。
 */
void mini_print_with_values(const MiniStack *s, const int *nums, const char *label)
{
    printf("    %s  size=%d：栈(底→顶) ", label, mini_size(s));
    if (mini_is_empty(s)) {
        printf("(空)");
    } else {
        for (int i = 0; i <= s->top; i++) {
            int idx = s->data[i];
            printf("(%d:%d) ", idx, nums[idx]);   /* (下标:值) */
        }
    }
    printf("\n");
}

/*
 * ===================================================================
 * 应用 ①：下一个更大元素（右边第一个严格更大的数，找不到填 -1）
 * ===================================================================
 * 一步步（对每个 i）：
 *   ① 【清理】：只要栈顶元素的值 < nums[i]，就把栈顶弹出来，并立刻写它的答案 ans[j] = nums[i]。
 *      反复弹，直到栈空 或 栈顶 >= nums[i]（递减纪律恢复了）。
 *   ② 【压入】：把 i 压进栈（此时它右边还没有比它大的，先欠着答案 -1）。
 *   ③ 扫完之后，栈里剩下的所有下标的答案都保持 -1（右边确实没有更大的了）。
 *
 * 边界情况：
 *   · nums == NULL 或 n <= 0：直接返回，不写 ans_out。
 *   · 整个数组递减（如 5 4 3 2 1）：while 一次都不执行，栈一路涨到 n，
 *     最后所有答案都是 -1。这是栈最长的极端情况 —— 空间 O(n) 就是这么来的。
 *   · 相等值（如 4 4）：用的是严格小于 nums[top] < nums[i]，相等【不弹】，
 *     所以「下一个更大」不含相等。想求「下一个 greater-or-equal」就把条件改成 <=。
 *   · ans_out 必须先全部填 -1：这是「找不到」的默认答案，弹到谁才覆盖谁。
 * ===================================================================
 */
/*
 * nge_core —— 真正的实现，用 verbose 开关决定「刷屏」还是「静默计数」
 *
 * 为什么要一个内部函数 + 两个外壳？
 *   演示时我们希望每一步都打印（第 1 幕），
 *   但第 4 幕要跟暴力法【数操作次数】做对比，刷屏会把表格冲得一干二净。
 *   如果写成两份代码，两份迟早不一致，读者就不知道信哪个了 ——
 *   所以只做一份，用参数控制打不打印。这是「一套逻辑，两种呈现」的常见手法。
 */
static void nge_core(const int *nums, int n, int *ans_out, bool verbose,
                     int *push_count_out, int *pop_count_out)
{
    if (nums == NULL || ans_out == NULL || n <= 0) {
        if (verbose) {
            printf("  [跳过] 数组或输出指针不合法（n=%d）\n", n);
        }
        if (push_count_out != NULL) {
            *push_count_out = 0;
        }
        if (pop_count_out != NULL) {
            *pop_count_out = 0;
        }
        return;
    }

    /* 先把答案全填 -1：「暂时不知道」= 「后面找不到就是 -1」 */
    for (int i = 0; i < n; i++) {
        ans_out[i] = -1;
    }

    MiniStack st;
    mini_init(&st, n);                  /* 栈最多同时装 n 个下标，一次到位 */

    int push_count = 0;                 /* 用来证明 O(n)：一共压入 n 次 */
    int pop_count = 0;                  /* 一共弹出 ≤ n 次 */

    if (verbose) {
        printf("  输入 nums = [");
        for (int i = 0; i < n; i++) {
            printf("%d%s", nums[i], (i + 1 < n) ? ", " : "");
        }
        printf("]，栈里存【下标】，保持 nums[下标] 从底到顶【递减】\n\n");
    }

    for (int i = 0; i < n; i++) {
        if (verbose) {
            printf("  ── 第 %d 步：来了 nums[%d] = %d ─────────────────\n", i, i, nums[i]);
        }

        /* ① 清理：凡是「比我小的栈顶」，此刻都找到了答案，全部弹掉 */
        while (!mini_is_empty(&st)) {
            int top_idx = 0;
            mini_peek(&st, &top_idx);   /* 先看栈顶是谁，决定要不要弹 */

            if (nums[top_idx] >= nums[i]) {
                /* 栈顶不小于新元素，递减纪律没被破坏：新元素只是「又欠着一个答案」 */
                if (verbose) {
                    printf("      栈顶是下标 %d（值 %d），%d >= %d → 递减纪律还在，停止弹栈\n",
                           top_idx, nums[top_idx], nums[top_idx], nums[i]);
                }
                break;
            }

            int bottom_idx = 0;
            mini_pop(&st, &bottom_idx);
            pop_count++;
            ans_out[bottom_idx] = nums[i];   /* ★ 答案诞生的瞬间 */
            if (verbose) {
                printf("      栈顶下标 %d（值 %d）比 %d 小 → 弹出它，并写下答案："
                       "ans[%d] = %d（下标 %d 右边第一个比它大的数就是 %d）\n",
                       bottom_idx, nums[bottom_idx], nums[i], bottom_idx, nums[i],
                       bottom_idx, nums[i]);
            }
        }

        /* ② 压入当前下标（注意压的是 i，不是 nums[i]） */
        mini_push(&st, i);
        push_count++;
        if (verbose) {
            printf("      把下标 %d 压入栈（它的答案暂时是 -1，等后面有更大的来认领）\n", i);
            mini_print_with_values(&st, nums, "压入后");
            printf("\n");
        }
    }

    if (verbose) {
        /* ③ 收尾：栈里剩下的都是「右边没有更大元素」的 */
        printf("  扫完了。栈里还欠着答案的下标有：");
        for (int i = 0; i <= st.top; i++) {
            printf("%d(值%d) ", st.data[i], nums[st.data[i]]);
        }
        printf("→ 它们的 ans 保持 -1。\n\n");

        printf("  最终答案 ans = [");
        for (int i = 0; i < n; i++) {
            printf("%d%s", ans_out[i], (i + 1 < n) ? ", " : "");
        }
        printf("]\n");

        /* 复杂度实证：把真实操作次数数出来 */
        printf("  操作计数：入栈 %d 次 + 出栈 %d 次 = %d 次（n=%d，最多 2n=%d）→ 这就是 O(n) 的来历\n",
               push_count, pop_count, push_count + pop_count, n, 2 * n);
        printf("  注意：出栈次数 %d 最多等于元素个数 %d，扫完还留 %d 个在栈里没被弹出。\n",
               pop_count, n, mini_size(&st));
    }

    if (push_count_out != NULL) {
        *push_count_out = push_count;
    }
    if (pop_count_out != NULL) {
        *pop_count_out = pop_count;
    }

    mini_destroy(&st);                  /* mini_init 的 malloc 在这里还回去 */
}

/*
 * 外壳一：逐步打印版（第 1、2 幕用）
 */
void mstack_next_greater(const int *nums, int n, int *ans_out)
{
    nge_core(nums, n, ans_out, true, NULL, NULL);
}

/*
 * 外壳二：静默计数版（第 4 幕拿它和暴力法比操作次数）
 *   push_count_out / pop_count_out 都可以传 NULL，表示那个数我不要。
 */
void mstack_next_greater_counted(const int *nums, int n, int *ans_out,
                                 int *push_count_out, int *pop_count_out)
{
    nge_core(nums, n, ans_out, false, push_count_out, pop_count_out);
}

/*
 * ===================================================================
 * 应用 ②：柱状图接雨水（简单版：柱宽 1、间距 0）
 * ===================================================================
 * 思路（单调递减栈，栈里存「柱子下标」，高度一路递减）：
 *   遇到一根比栈顶高的柱子，说明栈顶那根矮柱子被【左墙 + 右墙】夹住了 → 有个水槽 → 能存水。
 *   结算一层：
 *        槽底 = 弹出来的那根柱子 bottom
 *        左墙 = 弹完之后新的栈顶 left（★ 必须是弹出之后的栈顶，因为它是 bottom 左边那根更高的）
 *        右墙 = 当前的 i
 *        宽度 = i - left - 1        （夹住的中间那些柱子数）
 *        水深 = min(height[left], height[i]) - height[bottom]   （短板决定水面，再减去槽底高度）
 *        这一层的水量 = 宽度 × 水深，全部加到 left+1 … i-1 这几根柱子头上
 *   然后继续看新的栈顶是不是也比 height[i] 矮 —— 可能能结算好几层（像爬楼梯一样往上叠）。
 *   直到栈空 或 栈顶 >= height[i]，才把 i 压进去。
 *
 * 边界情况：
 *   · 弹完发现栈空 → left 不存在（bottom 就是当时最左也是最高的墙），水从左边流走了，接不住，直接停。
 *   · 水深 = 0 的情况：左墙和槽底一样高（平台），这一层没有水，照样不加水。
 *   · 最后一根柱子之后不再处理：右边没墙，栈里剩下的都接不住水 —— 循环天然结束，无需特判。
 *   · 空数组 / 长度 1：不可能有水，仍然走完流程并把 0 打出来。
 *
 * 图（height = [0,1,0,2,1,0,1,3]，先看前 8 根，水会一层层叠起来）：
 *
 *      ▒▒▒▒▒▒▒▒▒▒▒        ← 3
 *      █ ░░ █ ▒▒▒ ░░ █   ← 水面高度 2 的那层，被下标 3（高2）和下标 7（高3）夹住
 *      █ █  █  █  █ █ █
 *     0 1  2  3  4 5 6 7
 *     █=柱子  ░=水
 * ===================================================================
 */
void mstack_trapping_rain_water(const int *height, int n)
{
    if (height == NULL || n <= 0) {
        printf("  [跳过] 高度数组不合法（n=%d）\n", n);
        return;
    }

    /* 每根柱子上方积了多少水，先全部清零；总量用它统计 */
    int *water = calloc((size_t)n, sizeof(int));
    if (water == NULL) {
        printf("  [错误] calloc %d 个 int 失败\n", n);
        return;
    }

    MiniStack st;
    mini_init(&st, n);                  /* 栈里存柱子下标，最多 n 个 */

    long total = 0;                     /* 用 long 累加：宽×深连乘可能超 int（这里也顺便讲溢出意识）*/
    int push_count = 0;
    int pop_count = 0;

    printf("  输入 height = [");
    for (int i = 0; i < n; i++) {
        printf("%d%s", height[i], (i + 1 < n) ? ", " : "");
    }
    printf("]，栈里存【柱子下标】，保持高度从底到顶【递减】\n\n");

    for (int i = 0; i < n; i++) {
        printf("  ── 第 %d 步：来了一根高 %d 的柱子 height[%d]=%d ─────────\n", i, height[i], i, height[i]);

        /* 只要栈顶比我矮，就一层一层结算水槽 */
        while (!mini_is_empty(&st)) {
            int top_idx = 0;
            mini_peek(&st, &top_idx);

            if (height[top_idx] >= height[i]) {
                printf("      栈顶是下标 %d（高 %d），%d >= %d → 递减纪律还在，停止结算\n",
                       top_idx, height[top_idx], height[top_idx], height[i]);
                break;
            }

            int bottom = 0;
            mini_pop(&st, &bottom);
            pop_count++;

            /* ★ 弹出之后再看栈：空了就没有左墙，水从左边流光，接不住 */
            if (mini_is_empty(&st)) {
                printf("      弹掉槽底 %d（高 %d）后栈空了 → 它左边没有更高的墙，"
                       "水会从左边流走，这一槽接不住\n", bottom, height[bottom]);
                break;
            }

            int left = 0;
            mini_peek(&st, &left);            /* 新的栈顶就是左墙 */

            int width = i - left - 1;         /* 夹在中间的柱子根数 */
            int shorter = (height[left] < height[i]) ? height[left] : height[i];  /* 短板决定水面 */
            int depth = shorter - height[bottom];   /* 水面到槽底的距离 */

            printf("      结算一层水槽：左墙=下标 %d(高%d)，槽底=下标 %d(高%d)，右墙=下标 %d(高%d)\n",
                   left, height[left], bottom, height[bottom], i, height[i]);
            printf("        宽度 = i - left - 1 = %d - %d - 1 = %d\n", i, left, width);
            printf("        水深 = min(左墙%d, 右墙%d) - 槽底%d = %d\n",
                   height[left], height[i], height[bottom], depth);

            if (width > 0 && depth > 0) {
                int add = width * depth;
                total += add;
                for (int k = left + 1; k <= i - 1; k++) {
                    water[k] += depth;        /* 这层水摊到中间每根柱子头上 */
                }
                printf("        本层水量 = %d × %d = %d，加到柱子 %d..%d 头上（各 +%d）→ 累计总量 %ld\n",
                       width, depth, add, left + 1, i - 1, depth, total);
            } else {
                printf("        宽度或水深为 0 → 这一层没有水\n");
            }
        }

        mini_push(&st, i);
        push_count++;
        printf("      把下标 %d 压入栈（它现在是最右边那堵墙，等更矮的槽来找它）\n", i);
        mini_print_with_values(&st, height, "压入后");
        printf("\n");
    }

    /* 每根柱子的水量，横着打一遍 */
    printf("  逐根统计（★ 栈里最后剩下的柱子不用再算：它们右边没有墙，接不住水）\n");
    printf("    下标   :");
    for (int i = 0; i < n; i++) {
        printf("%4d", i);
    }
    printf("\n    柱高   :");
    for (int i = 0; i < n; i++) {
        printf("%4d", height[i]);
    }
    printf("\n    本柱水量:");
    for (int i = 0; i < n; i++) {
        printf("%4d", water[i]);
    }
    printf("\n");

    /* 竖着画一张图：█ = 柱子，░ = 水。从最高一层往下打 */
    int max_level = 0;
    for (int i = 0; i < n; i++) {
        int top_of_column = height[i] + water[i];
        if (top_of_column > max_level) {
            max_level = top_of_column;
        }
    }
    printf("    示意图（█=柱子 ░=水，从上往下画）：\n");
    for (int level = max_level; level >= 1; level--) {
        printf("    高度%2d |", level);
        for (int i = 0; i < n; i++) {
            if (level <= height[i]) {
                printf(" █");
            } else if (level <= height[i] + water[i]) {
                printf(" ░");
            } else {
                printf("  ");
            }
        }
        printf("\n");
    }
    /* 轴行的前缀要和上面「    高度%2d |」一样宽（4 空格 + 中文 2 字占 4 列 + 2 列数字 + 空格 + | = 12 列），
       所以这里用 11 个空格再加一个 |；每格标签用 %2d 而不是 " %d"，两位数下标才不会把后面顶歪。*/
    printf("           |");
    for (int i = 0; i < n; i++) {
        printf("%2d", i);
    }
    printf("\n");

    long check = 0;
    for (int i = 0; i < n; i++) {
        check += water[i];
    }
    printf("  总水量 = %ld（逐根加起来 = %ld，两者必须相等，这是自检）\n", total, check);
    printf("  操作计数：入栈 %d 次 + 出栈 %d 次 = %d 次（n=%d）→ 每根柱子最多进出栈各 1 次，所以 O(n)\n",
           push_count, pop_count, push_count + pop_count, n);

    mini_destroy(&st);
    free(water);                        /* calloc 来的必须 free；ASan 会盯着这一处 */
}
