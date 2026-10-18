/*
 * main.c —— 旋转数组查找：八幕实测
 *
 * 跑法： make one DIR=11-searching/rotated-array
 *
 * 幕次安排（每幕只证明一件事，看完能自己复述）：
 *   第 1 幕  旋转数组长什么样：升序 + 断崖 + 旋转点
 *   第 2 幕  招牌：为什么判据必须是 a[mid] vs a[right]（含故意写错的对照版）
 *   第 3 幕  找最小值 / 最大值 / 旋转次数，附轮数
 *   第 4 幕  查找目标：写法 A 与写法 B 逐步打印 + 互相验收
 *   第 5 幕  有重复元素：判据失效 → 扔端点 → 轮数退化到 O(n) 的实测数字
 *   第 6 幕  边界：n=1 / 未旋转 / 旋转 n 次 / 非旋转数组
 *   第 7 幕  和线性查找全量对拍（自动验收，不靠人眼）
 *   第 8 幕  同族应用：旋转数组第 k 小只要 O(log n)
 */

#include <stdio.h>
#include "ra.h"

/* 每幕之前打一条分隔线，让输出好切段；README 里引用时也方便定位 */
static void act(const char *title)
{
    printf("\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  %s\n", title);
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

int main(void)
{
    /* ============ 第 1 幕：先看清结构 ============ */
    act("第 1 幕  旋转数组长什么样（升序段 + 断崖 + 旋转点）");

    int sorted[7] = { 0, 1, 2, 3, 4, 5, 6 };
    ra_dump("    原始升序数组：", sorted, 7);

    printf("\n");
    /* 同一个数组，剪开不同位置：让读者看到"旋转点会跑" */
    int r0[7] = { 0, 1, 2, 3, 4, 5, 6 };   /* 旋转 0 次 */
    int r3[7] = { 3, 4, 5, 6, 0, 1, 2 };   /* 旋转 3 次 */
    int r6[7] = { 6, 0, 1, 2, 3, 4, 5 };   /* 旋转 6 次 */

    ra_dump("    旋转 0 次（等价于没转）：", r0, 7);
    printf("\n");
    ra_dump("    旋转 3 次：", r3, 7);
    printf("\n");
    ra_dump("    旋转 6 次（只把最后一个挪到最前）：", r6, 7);

    printf("\n    读图要点：不管怎么转，数组永远是「一段大值 + 一段小值」，\n");
    printf("    两段各自升序，中间只有一个断崖。找到断崖 = 找到最小值 = 找到旋转点。\n");

    /* ============ 第 2 幕：招牌演示 ============ */
    act("第 2 幕  为什么判据是 a[mid] vs a[right]，而不是 a[mid] vs a[left]");
    ra_demo_why_left_fails();

    /* ============ 第 3 幕：找旋转点 ============ */
    act("第 3 幕  最小值 / 最大值 / 旋转次数（一次二分全拿到）");

    int g[7] = { 3, 4, 5, 6, 0, 1, 2 };
    ra_dump("    待测数组：", g, 7);

    int steps = 0;
    int pivot = ra_min_index(g, 7, &steps);
    int maxi = ra_max_index(g, 7);
    printf("\n");
    printf("    ra_min_index   → 下标 %d，值 %d，循环 %d 轮\n", pivot, g[pivot], steps);
    printf("    ra_max_index   → 下标 %d，值 %d（就是旋转点左边那一格 (%d+7-1)%%7=%d）\n",
           maxi, g[maxi], pivot, (pivot + 7 - 1) % 7);
    printf("    ra_rotations   → 旋转了 %d 次\n", ra_rotations(g, 7));
    printf("    对照 log2(7) ≈ 2.8，实际 %d 轮 —— 7 个元素最多也就 3 轮，这就是 O(log n) 的体感\n", steps);

    /* ============ 第 4 幕：两种写法逐步看 ============ */
    act("第 4 幕  查找目标：写法 A（判半区）vs 写法 B（pivot + 取模复用标准二分）");

    int t1 = 1;   /* 在小值段 */
    int t2 = 5;   /* 在大值段 */
    int t3 = 99;  /* 不存在 */

    for (int q = 0; q < 3; q++) {
        int target = (q == 0) ? t1 : (q == 1) ? t2 : t3;
        int s = 0;

        printf("\n    ── 查找目标 %d ──────────────────────────────────\n", target);
        printf("    写法 A（一次二分，边判半区边收缩）：\n");
        int ia = ra_search(g, 7, target, 1, &s);
        printf("      → 返回下标 %d，%d 轮\n", ia, s);

        printf("    写法 B（先找旋转点，再 (虚拟+pivot)%%7 换算）：\n");
        int ib = ra_search_by_offset(g, 7, target, 1);
        printf("      → 返回下标 %d\n", ib);

        printf("    两种写法一致吗？ %s\n", ia == ib ? "一致 ✓" : "不一致 ✗（那就是 bug）");
    }

    /* ============ 第 5 幕：重复值让复杂度塌掉 ============ */
    act("第 5 幕  有重复元素：判据失效，只能扔端点，最坏退化到 O(n)");

    int d1[5] = { 1, 0, 1, 1, 1 };   /* 旋转过的 [0,1,1,1,1] */
    ra_dump("    数组 D1：", d1, 5);
    int s5 = 0;
    int id5 = ra_search_dup(d1, 5, 0, 1, &s5);
    printf("      → 找到 0 在下标 %d，共 %d 轮\n", id5, s5);

    printf("\n");
    int d2[8] = { 2, 2, 2, 2, 2, 2, 2, 3 };  /* 旋转 0 次，全是大重复 */
    ra_dump("    数组 D2（几乎全是同一个值）：", d2, 8);
    int s6 = 0;
    int id6 = ra_search_dup(d2, 8, 99, 1, &s6);
    printf("      → 99 不存在，返回 %d，但花了 %d 轮\n", id6, s6);
    printf("\n");
    printf("    ★ 看这两个轮数：n=%d 时轮数=%d，n=%d 时轮数=%d —— 已经是**线性**的了。\n", 5, s5, 8, s6);
    printf("      不是代码写得烂，是「三个数全都相等」这件事本身不含信息，\n");
    printf("      算法只能一个格子一个格子地排除。所以含重复版的复杂度上界就是 O(n)。\n");

    /* ============ 第 6 幕：边界 ============ */
    act("第 6 幕  边界情况：这些是最容易在面试/工程里翻车的地方");

    int one[1] = { 42 };
    printf("    ① 单元素 42：       min_index=%d  rotations=%d  找 42 → %d  找 7 → %d\n",
           ra_min_index(one, 1, NULL), ra_rotations(one, 1),
           ra_search(one, 1, 42, 0, NULL), ra_search(one, 1, 7, 0, NULL));

    int noshift[5] = { 1, 2, 3, 4, 5 };
    printf("    ② 完全没旋转：      min_index=%d（应为 0）  rotations=%d  max_index=%d（应为 4）\n",
           ra_min_index(noshift, 5, NULL), ra_rotations(noshift, 5), ra_max_index(noshift, 5));

    int back[5] = { 1, 2, 3, 4, 5 };
    printf("    ③ 旋转 n 次 = 回到原地：min_index=%d  和②一样，算法无法区分「没转」和「转满一圈」\n",
           ra_min_index(back, 5, NULL));

    printf("    ④ n=0 / NULL：     min_index=%d  ra_min_value=%d  ra_search=%d（都是安全的 -1/0，不崩）\n",
           ra_min_index(NULL, 0, NULL), ra_min_value(NULL, 0), ra_search(NULL, 0, 1, 0, NULL));

    int bad[5] = { 3, 1, 4, 1, 5 };
    printf("    ⑤ 根本不是旋转数组 3 1 4 1 5： ra_is_rotated_sorted=%d（0=不是）\n",
           ra_is_rotated_sorted(bad, 5));
    printf("       → 此时二分结果**无意义**：这是前置条件的责任，不是算法的锅。工程里要么先校验，\n");
    printf("         要么在文档里写明「输入必须是旋转过的升序数组」。\n");

    int equal2[4] = { 7, 7, 7, 7 };
    printf("    ⑥ 全相同 7 7 7 7：   is_rotated_sorted=%d  is_strictly=%d（有相等相邻，严格版为 0）\n",
           ra_is_rotated_sorted(equal2, 4), ra_is_strictly_rotated_asc(equal2, 4));

    /* ============ 第 7 幕：全量对拍 ============ */
    act("第 7 幕  自动验收：拿 O(n) 线性查找给所有二分对拍（人眼看不出的错，机器能）");

    /* 一份严格升序数组，右移 r 格得到 7 种旋转形态；对每种形态把 -1..7 全查一遍 */
    int base[7] = { 0, 1, 2, 3, 4, 5, 6 };
    int total = 0;
    int wrong = 0;

    for (int r = 0; r < 7; r++) {
        int rot[7];
        for (int i = 0; i < 7; i++) {
            rot[i] = base[(i + 7 - r) % 7];     /* 右移 r 格 */
        }

        int ok_shape = ra_is_strictly_rotated_asc(rot, 7);
        for (int target = -1; target <= 7; target++) {
            int lin = ra_search_linear(rot, 7, target);
            int sa = ra_search(rot, 7, target, 0, NULL);
            int sb = ra_search_by_offset(rot, 7, target, 0);
            int sd = ra_search_dup(rot, 7, target, 0, NULL);
            total++;

            /* 存在性必须一致（下标可以不同吗？严格升序下值唯一 ⇒ 下标也必须唯一一致） */
            if (lin != sa || lin != sb || lin != sd || !ok_shape) {
                wrong++;
                printf("    ✗ 旋转 %d 次 目标 %d：线性=%d  A=%d  B=%d  dup=%d  形态合法=%d\n",
                       r, target, lin, sa, sb, sd, ok_shape);
            }
        }
    }
    printf("    共比对 %d 组（7 种旋转形态 × 9 个目标值），不一致 %d 组 → %s\n",
           total, wrong, wrong == 0 ? "全部通过 ✓" : "有 bug！");
    printf("    为什么值得写这一幕：二分的错误往往是「某个具体下标才触发」，\n");
    printf("    手测两三个案例很容易全对，穷举一遍才敢说自己写对了。\n");

    /* ============ 第 8 幕：同族应用 ============ */
    act("第 8 幕  同族应用：旋转数组里「第 k 小」只要 O(log n)");

    int h[8] = { 5, 7, 9, 11, 1, 2, 3, 4 };
    ra_dump("    数组 H：", h, 8);
    printf("\n");
    printf("    k   下标   值     含义\n");
    for (int k = 1; k <= 8; k++) {
        int idx = ra_kth_smallest_index(h, 8, k);
        printf("    %d   %2d    %3d    第 %d 小\n", k, idx, h[idx], k);
    }
    printf("\n    打出来的这一列值 1 2 3 4 5 7 9 11 正好是升序 —— 说明「旋转点 + 取模」确实把数组复原了。\n");
    printf("    对比 11-searching/kth-smallest：那里对**无序**数组求第 k 小要 O(n)（快速选择），\n");
    printf("    这里因为数组「其实已经排过序」，一次二分 + 一次取模就够了。信息越多，代价越小。\n");

    printf("\n全部八幕跑完，本节没有任何 malloc，因此没有 free 责任。\n");
    return 0;
}
