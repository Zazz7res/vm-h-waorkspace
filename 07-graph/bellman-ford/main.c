#include <stdio.h>
#include "bellman.h"

static void section(const char *title)
{
    printf("\n==================================================\n");
    printf("%s\n", title);
    printf("==================================================\n");
}

/* 把一个 int 打成 8 列宽的字符串（∞ 特殊处理），给对照表用 */
static void fmt_cell(int v, char *buf, size_t sz)
{
    if (v >= INF_DIST) {
        snprintf(buf, sz, "     %s", "∞");     /* 手工补空格：∞ 占 3 字节但只显示 1 列 */
    } else {
        snprintf(buf, sz, "%6d", v);
    }
}

int main(void)
{
    /* ============================================================
       第 1 幕：先把「V-1 轮为什么够」讲清楚
       ============================================================ */
    section("幕 1 · 算法长什么样：三层循环，核心只有一行比较");

    printf("  for (第 round 轮; round <= V-1; round++)\n");
    printf("      for (每条边 u -> v，权 w)         ← 注意：是【所有边】，不是【某个点的邻居】\n");
    printf("          if (dist[u] != ∞ && dist[u] + w < dist[v])\n");
    printf("              dist[v] = dist[u] + w,  parent[v] = u;\n\n");
    printf("  为什么 V-1 轮一定够？\n");
    printf("    最短路径不会绕环（绕环要么更长要么等长；真有负环则压根没有最短路）。\n");
    printf("    V 个点、不重复经过点的路径，最多 V-1 条边。\n");
    printf("    第 1 轮把所有「1 条边就到」的点定对；第 2 轮把「2 条边」的定对……\n");
    printf("    每轮至少把一条边的正确距离往前推一格 ⇒ 推 V-1 轮全部到位。\n\n");
    printf("  第 V 轮还能松弛 ⇒ 存在一条「用了 V 条边反而更短」的路\n");
    printf("    ⇒ 它必然绕过一个环，而且这个环总权是负的 ⇒【负环】。\n");

    /* ============================================================
       第 2 幕：演示图 ① —— 带负权，没有负环
       ============================================================ */
    section("幕 2 · 图①：有负权边但没有负环（这是 Dijkstra 处理不了、BF 能处理的情形）");

    Graph g1;
    graph_build_neg_no_cycle(&g1);
    graph_print_edges(&g1);
    printf("  负权边只有一条：2 -> 1，权 -3。没有环，所以最短路是有定义的。\n");

    int d1[MAX_V];
    int p1[MAX_V];
    int rounds1 = 0, badf1 = -1, badt1 = -1;
    printf("\n  ---- 逐轮逼近（每轮把 5 条弧全试一遍）----\n");
    int r1 = bellman_ford(&g1, 0, d1, p1, 1, &rounds1, &badf1, &badt1);

    printf("\n  返回值 = %d（0 = 没有负环，答案可信）\n", r1);
    printf("  最终 dist：");
    for (int v = 0; v < g1.n; v++) {
        printf(" %d", d1[v]);
    }
    printf("\n  最终 parent：");
    for (int v = 0; v < g1.n; v++) {
        printf(" %d", p1[v]);
    }
    printf("\n");
    printf("  手算核对：dist[1] 应该是 -2（0->2->1 = 1 + (-3)），不是直连的 4；\n");
    printf("            dist[3] 应该是 0（0->2->1->3 = 1 - 3 + 2）。\n");
    printf("  路径：\n");
    for (int dst = 0; dst < g1.n; dst++) {
        print_path(p1, 0, dst, d1);
    }

    /* ============================================================
       第 3 幕：演示图 ② —— 含负环
       ============================================================ */
    section("幕 3 · 图②：含负环 —— Bellman-Ford 的第二件独门兵器：判负环");

    Graph g2;
    graph_build_neg_cycle(&g2);
    graph_print_edges(&g2);
    printf("  环是 1 -> 2 -> 1，总权 (-3) + (-1) = -4：每绕一圈，距离就少 4。\n");
    printf("  所以「从 0 到 2 的最短路」根本不存在（可以无限小），程序必须明说这件事。\n");

    int d2[MAX_V];
    int p2[MAX_V];
    int rounds2 = 0, badf2 = -1, badt2 = -1;
    printf("\n  ---- 盯着 dist 每轮掉 4，永远掉不完 ----\n");
    int r2 = bellman_ford(&g2, 0, d2, p2, 1, &rounds2, &badf2, &badt2);

    printf("\n  返回值 = %d", r2);
    if (r2 == 1) {
        printf("（1 = 检测到负环！）");
    }
    printf("\n");
    printf("  负环证据边：%d -> %d\n", badf2, badt2);
    printf("  此时 dist 数组只是「第 V-1 轮结束时刚好停在哪算哪」的中间产物，不能用：\n");
    print_dist_row("dist", d2, g2.n);
    printf("  注意顶点 3：它谁也不连，dist[3] 始终是 ∞ —— 「不可达」和「无穷小」是两回事。\n");
    printf("  如果想找出负环上到底是哪些点，标准做法是：把第 V 轮还能松弛的那条边的终点\n");
    printf("  「染色」后沿 parent 再走 V 步，一定落在环上。本 demo 不展开，见 README 扩展。\n");

    /* ============================================================
       第 4 幕：演示图 ③ —— 共用图，和 Dijkstra 对照
       ============================================================ */
    section("幕 4 · 图③：全仓库共用示例图（无负权）—— 和 Dijkstra 对照");

    printf("  主链：(0) ──7── (1) ──10── (2) ──11── (3) ──6── (4) ──9── (5)\n");
    printf("  跨点：(0) ──9── (2)   (0) ──14── (5)   (1) ──15── (3)   (2) ──2── (5)\n\n");

    Graph g3;
    graph_build_shared(&g3);
    graph_print_edges(&g3);

    int d3[MAX_V];
    int p3[MAX_V];
    int rounds3 = 0, badf3 = -1, badt3 = -1;
    printf("\n  ---- Bellman-Ford 跑共用图 ----\n");
    int r3 = bellman_ford(&g3, 0, d3, p3, 1, &rounds3, &badf3, &badt3);
    printf("  返回值 = %d（0 = 无负环）\n", r3);

    int d_dij[MAX_V];
    mini_dijkstra(&g3, 0, d_dij);            /* 本目录自带 20 行极简版，不 import 隔壁目录 */

    printf("\n  ---- 两种算法逐格对照 ----\n");
    printf("    顶点   Bellman-Ford   Dijkstra   是否一致\n");
    int same_all = 1;
    for (int v = 0; v < g3.n; v++) {
        char bf[8], dj[8];
        fmt_cell(d3[v], bf, sizeof bf);
        fmt_cell(d_dij[v], dj, sizeof dj);
        int same = (d3[v] == d_dij[v]);
        if (!same) {
            same_all = 0;
        }
        printf("      %d       %s       %s      %s\n", v, bf, dj, same ? "✓" : "✗ 不同！");
    }
    printf("\n  非负权时两者结果必须完全一致：%s\n", same_all ? "✓ 一致" : "✗ 有实现写错了");
    printf("  参考答案（手算 + 07-graph/dijkstra 实测都是这个）：dist = [0, 7, 9, 20, 20, 11]\n");
    printf("  区别只在代价：BF 这一轮就收敛（因为边序刚好顺），最坏要 V-1 = 5 轮 × %d 条弧；\n", g3.m);
    printf("  Dijkstra 只要 O(V^2) 或 O((V+E)logV)，快得多。没有负权就别用 BF。\n");

    printf("\n  Bellman-Ford 在这张图上的路径回溯（和 Dijkstra 打印的一模一样）：\n");
    for (int dst = 0; dst < g3.n; dst++) {
        print_path(p3, 0, dst, d3);
    }

    /* ============================================================
       第 5 幕：SPFA 是什么，一句话说清
       ============================================================ */
    section("幕 5 · SPFA（队列优化的 Bellman-Ford）是什么");

    printf("  BF 的浪费：某一轮里，只有少数几条边的 dist[u] 刚刚变过，\n");
    printf("            但我们把 %d 条弧全试了一遍 —— 大量比较是白做的。\n", g3.m);
    printf("  SPFA 的改造：只把「dist 刚被改过、因此有可能让邻居变短」的点丢进队列，\n");
    printf("            每次取一个点 u，只松弛 u 的出边，邻居被改小了就把邻居入队。\n");
    printf("            这本质就是 BFS 版的 Bellman-Ford，平均很快，代码见 README「扩展」一节。\n");
    printf("  为什么竞赛里它常被卡：最坏复杂度是 O(V*E)，而构造一张「一个点反复进队」的图\n");
    printf("            非常轻松（网格图、随机负权边都是常见卡法）。所以判负环 / 数据不可控时，\n");
    printf("            宁可用老老实实的 BF，或者用 SLF 等小优化，但别指望它一定能过。\n");

    /* ============================================================
       收尾
       ============================================================ */
    section("本节小结");
    printf("  1) BF = 「不定稿，把所有边重复松弛 V-1 轮」。它敢处理负权，就是因为它从不过早宣布定稿。\n");
    printf("  2) 第 V 轮还能松弛 ⇒ 负环 ⇒ 最短路没有意义。判负环是 BF 的独门能力。\n");
    printf("  3) 松弛前必须判 dist[u] != ∞，否则「∞ + 负数」会凭空造出一条到不可达点的路。\n");
    printf("  4) 复杂度 O(V*E)：比 Dijkstra 慢。没有负权时该用 Dijkstra，别因为 BF 好写就用它。\n");
    return 0;
}
