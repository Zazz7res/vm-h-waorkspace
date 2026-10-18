#include <stdio.h>
#include "bit.h"

static void section(const char *title)
{
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n");
}

/*
 * 自己写的极小伪随机数（线性同余 LCG）：seed = seed*1103515245 + 12345，只保留 32 位。
 * 为什么不用标准库 rand()？因为 rand 的序列由各家 C 库自己定，换平台就变了，
 * README 里贴的"运行结果"就没法复核。公式写死 ⇒ 谁跑都是一模一样的 24 步。
 */
static unsigned int g_seed = 20261003u;      /* 固定种子，写死 */

static unsigned int next_u32(void)
{
    g_seed = g_seed * 1103515245u + 12345u;
    return g_seed >> 8;                       /* 低几位质量差，右移 8 位再用 */
}

/* 把数按二进制打出来（含符号位），用来演示 i & (-i) */
static void print_bits16(int v)
{
    for (int k = 15; k >= 0; k--) {
        printf("%d", (v >> k) & 1);
    }
}

int main(void)
{
    /* 全文统一的固定测试数据（不用随机数，方便逐格核对） */
    const int n = 8;
    const int data[8] = {2, 1, 1, 3, 2, 3, 4, 1};
    /* 先手算好这三件事，后面用来验收：
     *   总和 = 17；前 7 个 = 16；
     *   tree[4] = a1+a2+a3+a4 = 2+1+1+3 = 7；tree[6] = a5+a6 = 5；tree[8] = 17 */

    /* ============================================================
       第 1 幕：痛点 —— 三种办法各只快一头
       ============================================================ */
    section("第 1 幕：三种做法各自快在哪，实测碰了几个格子");

    printf("      原始数组 a[1..8] =");
    for (int i = 0; i < n; i++) {
        printf(" %d", data[i]);
    }
    printf("   （总和 17）\n\n");

    const int target = 7;                      /* 要查的前缀长度 */

    /* 方案 A：只存原数组，查前缀和就现场一个个加 */
    int sum_a = 0;
    int touches_a = 0;
    for (int i = 1; i <= target; i++) {
        sum_a += data[i - 1];
        touches_a++;
    }
    printf("      方案A 只存原数组   ：查 a[1..%d] ⇒ 现场加了 %d 个数 = %d ⇒ 查询 O(n)\n",
           target, touches_a, sum_a);
    printf("                            改一个数只写 1 格 ⇒ 改 O(1)\n");

    /* 方案 B：提前把前缀和都存好 */
    int pref[9];                               /* pref[i] = a[1]+..+a[i]，1 起点 */
    pref[0] = 0;
    for (int i = 1; i <= n; i++) {
        pref[i] = pref[i - 1] + data[i - 1];
    }
    printf("      方案B 存前缀和数组 ：查 a[1..%d] ⇒ 直接读 pref[%d] = %d，只碰 1 格 ⇒ 查询 O(1)\n",
           target, target, pref[target]);
    int touches_b = 0;
    for (int i = 3; i <= n; i++) {             /* 现在 a[3] 要 +5：它后面每个前缀和都得跟着改 */
        pref[i] += 5;
        touches_b++;
    }
    printf("                            但把 a[3] += 5 之后 pref[3..8] 全变了 ⇒ 改了 %d 格 ⇒ 修改 O(n)\n",
           touches_b);

    BIT bit;
    bit_init(&bit, data, n);
    bit_reset_touches(&bit);
    int s7 = bit_query(&bit, target);
    long long cells_q = bit.touches;
    bit_reset_touches(&bit);
    bit_update(&bit, 3, 5);                    /* 演示用：真的加一下 */
    long long cells_u = bit.touches;
    bit_update(&bit, 3, -5);                   /* 撤回来，保持数据干净 */
    printf("      方案C 树状数组     ：查 a[1..%d] 只碰 %lld 格 ⇒ %d；改 a[3] 只碰 %lld 格\n",
           target, cells_q, s7, cells_u);
    printf("                            ⇒ 两头都是 O(log n)，这就是它存在的全部理由\n");
    printf("      （n=8 时 log2(8)+1 = 4，所以最多碰 4 格；别小看这几格，n=100000 时是 17 格 vs 100000 格）\n");

    /* ============================================================
       第 2 幕：lowbit 是怎么"一下就算出来"的
       ============================================================ */
    section("第 2 幕：i & (-i) 为什么正好留下最低位的 1（补码 = 取反加一）");

    printf("         i    i 的 16 位二进制      -i 的 16 位二进制      i&-i 二进制              lowbit\n");
    for (int i = 1; i <= n; i++) {
        printf("       %3d   ", i);
        print_bits16(i);
        printf("   ");
        print_bits16(-i);
        printf("   ");
        for (int k = 5; k >= 0; k--) {         /* 结果很小，只打低 6 位就够看 */
            printf("%d", (i & -i) >> k & 1);
        }
        printf("           %3d\n", bit_lowbit(i));
    }
    printf("\n      拿 i=6 走一遍：6 = 0000 0110\n");
    printf("        取反         1111 1001\n");
    printf("        加一 = -6    1111 1010\n");
    printf("        两者相与     0000 0010 = 2  ⇒ tree[6] 管 2 个元素，正好是 a[5..6]\n");
    printf("      规律：i 是奇数 ⇒ lowbit=1（只管自己）；i 是 2 的幂 ⇒ lowbit=i（一个人管一大片）\n");

    /* ============================================================
       第 3 幕：那张灵魂覆盖图
       ============================================================ */
    section("第 3 幕：覆盖图 —— tree[i] 负责 a[i-lowbit(i)+1 .. i]");

    bit_print_coverage_chart(n);
    printf("\n      验证一下「拼前缀 [1..7]」：7 → 6 → 4 → 0\n");
    printf("        tree[7]=a[7]，tree[6]=a[5..6]，tree[4]=a[1..4] ⇒ 合起来正好覆盖 {1..7}，不重不漏 ✔\n");
    printf("        要跳的步数 = 7 的二进制 111 里 1 的个数 = 3\n");

    printf("\n");
    bit_print_cells(&bit, "建好之后的整张表（对照上面的图看）：");
    printf("      手算核对：tree[4] = 2+1+1+3 = 7 ✔    tree[6] = 2+3 = 5 ✔    tree[8] = 17 ✔\n");

    /* ============================================================
       第 4 幕：查询 —— i -= i&(-i)，一路"吃掉整块"
       ============================================================ */
    section("第 4 幕：query(7) 的跳格全过程（把每一格负责什么都印出来）");

    bit.trace = 1;
    int q7 = bit_query(&bit, 7);
    bit.trace = 0;
    printf("      ⇒ query(7) = %d（手算 2+1+1+3+2+3+4 = 16 ✔）\n", q7);

    printf("\n      区间和 range_query(3,6) = query(6) - query(2)：\n");
    bit.trace = 1;
    int q36 = bit_range_query(&bit, 3, 6);
    bit.trace = 0;
    printf("      ⇒ range(3,6) = %d（手算 a3+a4+a5+a6 = 1+3+2+3 = 9 ✔）\n", q36);

    printf("\n      顺手试一个越界的：query(13)，n 只有 8 ⇒\n");
    bit.trace = 1;
    int q13 = bit_query(&bit, 13);
    bit.trace = 0;
    printf("      ⇒ 被自动截断成 query(8) = %d（就是全部元素的和），不会读到数组外面\n", q13);

    /* ============================================================
       第 5 幕：修改 —— i += i&(-i)，通知所有"含我这个元素"的格子
       ============================================================ */
    section("第 5 幕：update(3, +5) 的跳格全过程");

    bit.trace = 1;
    bit_update(&bit, 3, 5);
    bit.trace = 0;
    printf("      ⇒ a[3] 从 1 变成 %d，tree[3]、tree[4]、tree[8] 三个格子各 +5\n", bit_get(&bit, 3));
    printf("\n      改完之后的整张表：\n");
    bit_print_cells(&bit, "");
    printf("      再查 query(7) = %d（应该比刚才的 16 多 5 = 21）\n", bit_query(&bit, 7));

    printf("\n      题目如果说「把 a[5] 改成 9」，树状数组只认「加多少」，所以先算差值：\n");
    printf("        delta = 9 - bit_get(5) = 9 - %d = %d\n", bit_get(&bit, 5), 9 - bit_get(&bit, 5));
    bit_update(&bit, 5, 9 - bit_get(&bit, 5));
    printf("        update(5, %+d) 之后 a[5] = %d，query(7) = %d\n",
           9 - 2, bit_get(&bit, 5), bit_query(&bit, 7));
    printf("      演示结束，把这两处改动撤回原状（update(3,-5)、update(5,-7)）\n");
    bit_update(&bit, 3, -5);
    bit_update(&bit, 5, -7);

    /* ============================================================
       第 6 幕：为什么下标必须从 1 开始
       ============================================================ */
    section("第 6 幕：为什么下标必须从 1 开始 —— 因为 lowbit(0) = 0");

    printf("      lowbit(0) = 0 & (-0) = %d\n", bit_lowbit(0));
    printf("      查询要跳 p -= lowbit(p)：p 一旦是 0 ⇒ 0 - 0 = 0，永远停在 0 ⇒ 死循环\n");
    printf("      修改要跳 p += lowbit(p)：p 一旦是 0 ⇒ 0 + 0 = 0，永远出不去 ⇒ 死循环 + 原地打转\n");
    printf("      所以：tree[0] 这一格申请了但永远不用，查询循环条件写成 p > 0，建树从 1 开始。\n");
    printf("      附带好处：query(0) 自然返回 0，而 range_query(1,r) 正好需要这个「空区间」：\n");
    printf("        range(1,1) = query(1) - query(0) = %d - %d = %d\n",
           bit_query(&bit, 1), bit_query(&bit, 0), bit_range_query(&bit, 1, 1));
    printf("      ⇒ 如果你的原始数据是 a[0..n-1]（C 数组的常态），进 BIT 时统一 +1 就好。\n");
    printf("      越界保护也试一下：update(0, +1) ⇒\n");
    bit_update(&bit, 0, 1);
    printf("      update(9, +1)（n=8，最大 8）⇒\n");
    bit_update(&bit, 9, 1);

    /* ============================================================
       第 7 幕：和朴素循环对拍 —— 证明这份实现是对的
       ============================================================ */
    section("第 7 幕：和朴素 O(n) 循环对拍（固定种子 24 步，每步两边结果必须相等）");

    int op_kind[24], op_l[24], op_r[24], op_delta[24];
    for (int k = 0; k < 24; k++) {
        if ((next_u32() & 1u) == 0u) {
            op_kind[k] = 1;                                        /* 修改 */
            op_l[k] = 1 + (int)(next_u32() % (unsigned int)n);
            int d = (int)(next_u32() % 6u) - 3;                /* 先取 -3..+2 */
            op_delta[k] = (d >= 0) ? d + 1 : d;                /* 再跳过 0，变成 -3..-1 / 1..3：
                                                                  免得出现 update(i,+0) 这种没信息量的步骤 */
            op_r[k] = op_l[k];
        } else {
            op_kind[k] = 2;                                        /* 区间查询 */
            int x = 1 + (int)(next_u32() % (unsigned int)n);
            int y = 1 + (int)(next_u32() % (unsigned int)n);
            op_l[k] = x < y ? x : y;                               /* 保证 l <= r */
            op_r[k] = x < y ? y : x;
            op_delta[k] = 0;
        }
    }
    printf("      生成的操作序列：\n");
    for (int k = 0; k < 24; k++) {
        if (op_kind[k] == 1) {
            printf("        %2d: update(%d, %+d)\n", k + 1, op_l[k], op_delta[k]);
        } else {
            printf("        %2d: range_query(%d, %d)\n", k + 1, op_l[k], op_r[k]);
        }
    }

    int mismatches = 0;
    printf("\n       步  操作                  BIT 答案  朴素答案  判定      工作量对比\n");
    printf("      ---------------------------------------------------------------\n");
    for (int k = 0; k < 24; k++) {
        int bit_ans = 0;
        int naive_ans = 0;
        int adds = 0;
        long long cells = 0;
        char desc[32];

        if (op_kind[k] == 1) {
            snprintf(desc, sizeof(desc), "update(%d,%+d)", op_l[k], op_delta[k]);
            bit_reset_touches(&bit);
            bit_update(&bit, op_l[k], op_delta[k]);
            cells = bit.touches;
            /* 改完立刻整体核对：BIT 眼里的总和 vs 朴素把 a[1..n] 全加起来 */
            bit_ans = bit_query(&bit, n);
            naive_ans = bit_naive_range_sum(&bit, 1, n, &adds);
        } else {
            snprintf(desc, sizeof(desc), "range_query(%d,%d)", op_l[k], op_r[k]);
            bit_reset_touches(&bit);
            bit_ans = bit_range_query(&bit, op_l[k], op_r[k]);
            cells = bit.touches;                                   /* 两趟前缀查一共碰了几格 */
            naive_ans = bit_naive_range_sum(&bit, op_l[k], op_r[k], &adds);
        }

        printf("      %3d  %-18s %8d %10d    %s   BIT %lld 格 / 朴素 %d 次相加\n",
               k + 1, desc, bit_ans, naive_ans,
               bit_ans == naive_ans ? "✔ 一致" : "✘ 不一致", cells, adds);
        if (bit_ans != naive_ans) {
            mismatches++;
        }
    }
    printf("      ---------------------------------------------------------------\n");
    printf("      对拍结论：24 步操作，不一致的步数 = %d ⇒ %s\n", mismatches,
           mismatches == 0 ? "这份树状数组实现和朴素循环完全等价 ✔" : "实现有问题，必须修！");
    printf("      最后再看一眼整张表（朴素版就是拿它的 a[] 逐格相加，所以它对才说明 BIT 对）：\n");
    bit_print_cells(&bit, "");

    /* ============================================================
       第 8 幕：应用 —— 逆序对
       ============================================================ */
    section("第 8 幕：应用 逆序对 —— 从右往左扫，把「值」当成 BIT 的下标用");

    const int m = 8;
    const int arr[8] = {3, 1, 4, 5, 2, 6, 2, 1};      /* 值域 1..6 */

    printf("      数组 a[1..%d] =", m);
    for (int i = 0; i < m; i++) {
        printf(" %d", arr[i]);
    }
    printf("   （值都在 1..6，所以可以拿值直接当下标）\n");

    /* 先用最笨的双层循环数一遍，当作标准答案 */
    long long naive_inv = 0;
    for (int i = 0; i < m; i++) {
        for (int j = i + 1; j < m; j++) {
            if (arr[i] > arr[j]) {
                naive_inv++;
            }
        }
    }
    printf("      朴素 O(n^2)：逆序对 = %lld 个（这是标准答案）\n", naive_inv);

    printf("\n      用树状数组（开一个长度 6 的「频率表」：下标 = 值，格子里 = 出现次数）：\n");
    BIT freq;
    bit_init(&freq, NULL, 6);                          /* 全 0 起步：谁都没出现过 */
    freq.trace = 1;
    long long bit_inv = bit_count_inversions(&freq, arr, m, 6);
    freq.trace = 0;
    printf("      树状数组：逆序对 = %lld 个 ⇒ 与朴素版%s\n",
           bit_inv, bit_inv == naive_inv ? "一致 ✔" : "不一致 ✘");
    printf("      ⇒ 两句关键话：query(v-1) = 「右边比我小的有几个」，update(v,+1) = 「把这个值登记上」。\n");
    printf("        因为从右往左扫，BIT 里存的恰好只有「当前元素右边」的那些值，所以不会数重。\n");
    bit_free(&freq);

    /* ============================================================
       收尾
       ============================================================ */
    printf("\n=== 本节结束 ===\n");
    printf("记住三句话：\n");
    printf("  1) tree[i] 管 a[i-lowbit(i)+1 .. i]，长度就是 lowbit(i) = i & (-i)。\n");
    printf("  2) 查询往下跳 p -= lowbit(p) 吃整块；修改往上跳 p += lowbit(p) 通知所有含我的格子。\n");
    printf("  3) 下标必须从 1 开始：lowbit(0)=0 会死循环；而 query(0)=0 正好是区间和要的「空地」。\n");

    bit_free(&bit);
    return 0;
}
