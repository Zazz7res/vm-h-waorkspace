#include <stdio.h>
#include "floyd.h"

static void section(const char *title)
{
    printf("\n==================================================\n");
    printf("%s\n", title);
    printf("==================================================\n");
}

/* 把两张表里「不一样的格子」找出来打 */
static void compare_tables(const Table a, const Table b, int n)
{
    int cnt = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (a[i][j] != b[i][j]) {
                cnt++;
                printf("      ✗ 第 %d 行第 %d 列不一样：正确 %d ，k 放最内层的版本 %d\n",
                       i, j,
                       a[i][j] >= INF_DIST ? -1 : a[i][j],
                       b[i][j] >= INF_DIST ? -1 : b[i][j]);
            }
        }
    }
    printf("      共有 %d 个格子不同\n", cnt);
}

int main(void)
{
    Table dist_ok, dist_bad, next_tab;

    /* ============================================================
       第 1 幕：先把「k 是什么」摆上桌
       ============================================================ */
    section("幕 1 · 全部代码只有 5 行，但 k 的含义是这一节的命");

    printf("  for (k = 0; k < n; k++)                     ← k 必须在【最外层】\n");
    printf("      for (i = 0; i < n; i++)\n");
    printf("          for (j = 0; j < n; j++)\n");
    printf("              if (d[i][k] != ∞ && d[k][j] != ∞ && d[i][k] + d[k][j] < d[i][j])\n");
    printf("                  d[i][j] = d[i][k] + d[k][j];\n\n");
    printf("  k 不是「第几步」，k 是【中转站许可证】：\n");
    printf("  第 k 轮结束时的 d[i][j] =「只准把 {0,1,...,k} 当中转站」时，i 到 j 的最短距离。\n");
    printf("  k 一路推到 n-1，许可证全发完了，得到的就是真正的最短路。\n\n");
    printf("  为什么这个递推对？第 k 轮只有两种情况：\n");
    printf("    (a) 最短路不经过 k → 上一轮已经算对了，这一轮不动它；\n");
    printf("    (b) 最短路经过 k  → 在 k 处劈成 i~>k 和 k~>j 两段，这两段都不会再经过 k\n");
    printf("        （否则会绕重复的圈，删掉那一圈只会更短），所以它们上一轮就已经是正确值。\n");

    /* ============================================================
       第 2 幕：逐 k 轮打印整张矩阵
       ============================================================ */
    section("幕 2 · 演示图①：把每一轮 k 结束后的 4×4 矩阵全部打出来");

    Matrix g1;
    mat_build_demo(&g1);
    mat_print(&g1, "演示图①");
    printf("  边表：0->1(3)  0->2(8)  1->2(2)  1->3(9)  2->3(1)  3->0(4)\n");
    printf("  先记住两个手算目标：d[0][2] 要从 8 降到 5（绕道 1）；\n");
    printf("                    d[1][0] 最后要是 7（走 1->2->3->0 = 2+1+4）。\n\n");

    int neg1 = floyd_all_pairs(&g1, dist_ok, next_tab, 1);
    printf("\n  负环检查：返回值 %d（0 = 没有负环，dist 可信）\n", neg1);

    printf("\n  逐格核对最终矩阵：\n");
    printf("    d[0][2]=5 = 0->1->2（3+2），比直连 8 短 ✓\n");
    printf("    d[0][3]=6 = 0->1->2->3（3+2+1）✓\n");
    printf("    d[1][0]=7 = 1->2->3->0（2+1+4）✓ 这条绕了三个点\n");
    printf("    d[2][1]=8 = 2->3->0->1（1+4+3）✓\n");
    printf("    d[3][2]=9 = 3->0->1->2（4+3+2）✓\n");

    /* ============================================================
       第 3 幕：现场跑一个错误循环顺序
       ============================================================ */
    section("幕 3 · 【现场翻车】把 k 放到最内层会怎样");

    floyd_wrong_loop_order(&g1, dist_bad);

    table_print("  正确版（k 最外层）最终矩阵：", dist_ok, g1.n, "dist");
    table_print("  错误版（i、j 在外，k 在最内层）最终矩阵：", dist_bad, g1.n, "dist");
    printf("  差异：\n");
    compare_tables(dist_ok, dist_bad, g1.n);

    printf("\n  为什么会错？盯住 d[1][0] 这一格：\n");
    printf("    正确答案 7 = 1->2->3->0 = 2 + 1 + 4。\n");
    printf("    错误版算 (i=1, j=0) 时，能用的零件是【此刻的】d[1][3] 和 d[3][0]。\n");
    printf("    可 d[1][3] 要从 9 降到 3，得靠 j = 3 那一格先被更新；而 j 是从小到大扫的，\n");
    printf("    (1,0) 比 (1,3) 先算 ⇒ 拿到的 d[1][3] 还是旧值 9 ⇒ 9 + 4 = 13。\n");
    printf("    之后 (1,3) 才被更新成 3，但 (1,0) 再也不会被回头看第二眼 ⇒ 13 就永远留在那。\n");
    printf("  一句话：d[i][k] 是【零件】，零件必须在该用它之前就加工好。\n");
    printf("  k 放最外层，保证的就是「这一轮用的所有零件都来自上一轮」。\n");
    printf("  ⚠ 注意它错得很安静：矩阵照样填满，程序一声不吭，只是数字不对。\n");

    /* ============================================================
       第 4 幕：next 表与完整路径
       ============================================================ */
    section("幕 4 · 只存 dist 只知道「多远」，再存一张 next 表才知道「怎么走」");

    table_print("  next 表（next[i][j] = 从 i 去 j 的最短路上，i 迈出的第一步落在哪个点）：",
                next_tab, g1.n, "next");
    printf("  读法举例：next[1][0] = 2 ⇒ 从 1 去 0 第一步走 2；next[2][0] = 3 ⇒ 再走 3；\n");
    printf("            next[3][0] = 0 ⇒ 到 0 了。拼起来就是 1 -> 2 -> 3 -> 0。\n");
    printf("  路径还原（每一对都打一遍）：\n");

    int trail[MAX_V];
    for (int src = 0; src < g1.n; src++) {
        for (int dst = 0; dst < g1.n; dst++) {
            if (src == dst) {
                continue;
            }
            int len = floyd_reconstruct(next_tab, src, dst, trail, MAX_V);
            if (len < 0) {
                printf("    %d -> %d：不可达\n", src, dst);
                continue;
            }
            printf("    %d -> %d  距离 %2d   路径 ", src, dst, dist_ok[src][dst]);
            for (int i = 0; i < len; i++) {
                printf("%d", trail[i]);
                if (i + 1 < len) {
                    printf(" -> ");
                }
            }
            printf("\n");
        }
    }

    /* ============================================================
       第 5 幕：负权 / 负环
       ============================================================ */
    section("幕 5 · Floyd 能吃负权（只要没负环），还能顺手把负环抓出来");

    Matrix g2;
    mat_build_negative(&g2);
    mat_print(&g2, "演示图②（含负权边 1->2 = -4，是 DAG 所以没负环）");
    Table d2, n2;
    int neg2 = floyd_all_pairs(&g2, d2, n2, 0);
    table_print("  最终 dist 矩阵：", d2, g2.n, "dist");
    printf("  负环检查返回值 = %d（0 = 没有负环）\n", neg2);
    printf("  手算核对第 0 行：d[0][2] = 0->1->2 = 2 + (-4) = -2 ✓ 是负数（Floyd 允许）\n");
    printf("                  d[0][3] = 0->1->2->3 = 2 - 4 + 3 = 1 ✓\n");
    printf("  → 能处理负权的前提是【没有负环】。有负环就得抓出来，见下面。\n");

    Matrix g3;
    mat_build_neg_cycle(&g3);
    mat_print(&g3, "演示图③（0->1(1), 1->2(-1), 2->0(-1)：绕一圈 = -1，是负环）");
    Table d3, n3;
    int neg3 = floyd_all_pairs(&g3, d3, n3, 1);
    table_print("  最终 dist 矩阵：", d3, g3.n, "dist");
    printf("  负环检查返回值 = %d（1 = 有负环！）\n", neg3);
    int vcyc = floyd_negative_cycle_vertex(d3, g3.n);
    printf("  抓到对角线：dist[%d][%d] = %d < 0 ⇒ 顶点 %d 能绕过一个负环回到自己。\n",
           vcyc, vcyc, d3[vcyc][vcyc], vcyc);
    printf("  判负环为什么只看对角线？dist[i][i] 的初值是 0（一步不走）。它变负，\n");
    printf("  只能是「从 i 出发绕一圈回到 i」这条路是负的 ⇒ 那个圈就是负环。\n");
    printf("  这时整张 dist 表全部作废：任意两点都能先绕负环 N 圈再走，距离想多小就多小。\n");

    /* ============================================================
       第 6 幕：传递闭包
       ============================================================ */
    section("幕 6 · 把 min/+ 换成 或/与：同一套三重循环求「可达性」（传递闭包）");

    Matrix g4;
    mat_build_dag(&g4);
    mat_print(&g4, "演示图④（链 0 -> 1 -> 2 -> 3，只问「能不能到」）");

    static int reach[MAX_V][MAX_V];
    for (int i = 0; i < g4.n; i++) {
        for (int j = 0; j < g4.n; j++) {
            /* 有直接边 = 1；对角线强制写 0 —— 因为这里问的是「走一条路能不能到」，
             * 站在原地不动不算"可达"。（有些教材把对角线写成 1，那是自反闭包，两种都有人用，
             * 关键是你自己得知道选的是哪种，别一处写 0 一处写 1。） */
            reach[i][j] = (i != j && g4.w[i][j] < INF_DIST) ? 1 : 0;
        }
    }
    table_print("  闭包前（只有直接边；对角线也是 0，因为「站在原地不动」不算一条路）：",
                reach, g4.n, "reach");
    floyd_transitive_closure(reach, g4.n);
    table_print("  闭包后（i 能走到 j 就是 1）：", reach, g4.n, "reach");
    printf("  读法：第 0 行变成 0 1 1 1 ⇒ 0 能到 1、2、3；第 3 行全 0 ⇒ 3 是死胡同。\n");
    printf("  这就是「传递」两个字：0->1 且 1->2 ⇒ 0->2。三重循环把这条规则推到不动点。\n");
    printf("  对比运算符的替换：\n");
    printf("     最短路   d[i][j] = min(d[i][j], d[i][k] + d[k][j])\n");
    printf("     可达性   r[i][j] = r[i][j] | (r[i][k] & r[k][j])\n");
    printf("  骨架一模一样 —— 所以 Floyd 是「半环上的最短路」，换个半环就换个用途。\n");

    /* ============================================================
       第 7 幕：回到共用图，和 Dijkstra 对照
       ============================================================ */
    section("幕 7 · 共用示例图：Floyd 的第 0 行 vs Dijkstra 的 dist 数组");

    printf("  主链：(0) ──7── (1) ──10── (2) ──11── (3) ──6── (4) ──9── (5)\n");
    printf("  跨点：(0) ──9── (2)   (0) ──14── (5)   (1) ──15── (3)   (2) ──2── (5)\n\n");

    Matrix g5;
    mat_build_shared(&g5);
    Table d5, n5;
    int neg5 = floyd_all_pairs(&g5, d5, n5, 0);
    table_print("  6×6 全点对最短距离矩阵（无向图 ⇒ 沿对角线完全对称，这本身就是一遍自检）：",
                d5, g5.n, "dist");
    printf("  负环检查返回值 = %d\n", neg5);
    printf("\n  Floyd 的第 0 行（0 到各点）    = [%d, %d, %d, %d, %d, %d]\n",
           d5[0][0], d5[0][1], d5[0][2], d5[0][3], d5[0][4], d5[0][5]);
    printf("  Dijkstra 的 dist 数组          = [0, 7, 9, 20, 20, 11]\n");
    const int dijkstra_answer[6] = { 0, 7, 9, 20, 20, 11 };
    int match = 1;
    for (int j = 0; j < 6; j++) {
        if (d5[0][j] != dijkstra_answer[j]) {
            match = 0;
        }
    }
    printf("  两节结果必须一致：%s（不一致就是有一处写错了）\n", match ? "✓ 一致" : "✗ 不一致");
    printf("  代价对比：只要这一行，Dijkstra 花 O(V^2) = 36 次比较；\n");
    printf("  Floyd 却顺手把【全部 6 行 36 对】都算完了，总共 6^3 = 216 次比较。\n");
    printf("  点数小的时候 Floyd 划算到离谱 —— 而且它只有 5 行代码，写不出 bug。\n");

    /* 顺手把共用图上的一条路径打出来，说明 next 表在这张图上也照样能用 */
    int len5 = floyd_reconstruct(n5, 0, 4, trail, MAX_V);
    printf("  用 next 表还原 0 到 4 的完整路径：");
    for (int i = 0; i < len5; i++) {
        printf("%d", trail[i]);
        if (i + 1 < len5) {
            printf(" -> ");
        }
    }
    printf("   (%d)   ← 和 Dijkstra 打印的 0 -> 2 -> 5 -> 4 (20) 一模一样\n", d5[0][4]);

    /* ============================================================
       收尾
       ============================================================ */
    section("本节小结");
    printf("  1) Floyd 一次算出【所有点对】的最短距离：O(V^3) 时间、O(V^2) 空间，代码 5 行。\n");
    printf("  2) k 的含义是「只允许编号 <= k 的点当中转站」，所以 k 【必须】在最外层。\n");
    printf("     把 k 放最内层会用到没算完的零件：本节实测 d[1][0] 正确 7，错版算成 13。\n");
    printf("  3) 能处理负权；对角线出现 dist[i][i] < 0 就说明有负环，此时整张表作废。\n");
    printf("  4) dist 表给距离，next 表给路径，两张要一起维护；只留 dist 打不出完整路径。\n");
    printf("  5) 把 min/+ 换成 或/与，同一个骨架就是传递闭包（求可达性）。\n");
    return 0;
}
