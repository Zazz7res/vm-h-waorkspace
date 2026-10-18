#include <stdio.h>
#include "dijkstra.h"

/* 打印一行分隔线，让输出好读。static = 只给本文件用，不污染别人 */
static void section(const char *title)
{
    printf("\n==================================================\n");
    printf("%s\n", title);
    printf("==================================================\n");
}

/* 把 ∞（那个 1e9 的哨兵值）打印成 ∞，而不是打印成一长串 1000000000。
 * 这里手工补空格，不用 %5s：printf 的宽度是按【字节】算的，而 ∞ 在 UTF-8 里占 3 个字节，
 * 用 %5s 只会补 2 个空格，屏幕上只占 3 列，和 %5d 出来的 5 列对不齐。
 * 补 4 个空格 + ∞（显示占 1 列）= 5 列，正好和 %5d 一致。 */
static void fmt_cell(int v, char *buf, size_t sz)
{
    if (v >= INF_DIST) {
        snprintf(buf, sz, "    %s", "∞");
    } else {
        snprintf(buf, sz, "%5d", v);
    }
}

int main(void)
{
    /* ============================================================
       第 1 幕：把共用的示例图摆出来
       ============================================================ */
    section("幕 1 · 共用示例图（6 个顶点 0..5，9 条带权边）");

    printf("  主链：(0) ──7── (1) ──10── (2) ──11── (3) ──6── (4) ──9── (5)\n");
    printf("  跨点：(0) ──9── (2)   (0) ──14── (5)   (1) ──15── (3)   (2) ──2── (5)\n\n");

    Graph g;
    graph_build_shared(&g);
    graph_print_edges(&g);

    printf("\n  手算一个关键看点：0 到 5 有直连边权 14，但 0 -> 2 -> 5 只要 9 + 2 = 11。\n");
    printf("  「眼前最便宜的边」不一定给出「最短的路」—— 记住这句，往下看每一轮 dist 怎么变。\n");

    /* ============================================================
       第 2 幕：朴素 Dijkstra，逐轮距离表（本节最重要的画面）
       ============================================================ */
    section("幕 2 · 朴素版 O(V^2)：每轮从未定稿里挑 dist 最小的点定稿");

    int dist_naive[MAX_V];
    int parent_naive[MAX_V];
    int neg_naive = dijkstra_naive(&g, 0, dist_naive, parent_naive, 1);
    printf("\n  负权检查：%s\n", neg_naive ? "有负权边（结果不可信）" : "全是非负权，Dijkstra 前提成立 ✓");

    /* ============================================================
       第 3 幕：最终结果 + 把路径回溯成人话
       ============================================================ */
    section("幕 3 · 最终 dist 数组 / parent 数组 / 完整路径");

    printf("  起点 src = 0\n");
    print_dist_row("dist", dist_naive, g.n);
    print_parent_row("parent", parent_naive, g.n);
    printf("\n  parent 数组读法：parent[v] = u 表示「到 v 最短的那一步是从 u 过来的」，\n");
    printf("  把所有点往回串，就得到一棵以 0 为根的【最短路径树】：\n");
    printf("        0\n");
    printf("       / \\\n");
    printf("      1   2\n");
    printf("          |\\\n");
    printf("          5 3\n");
    printf("          |\n");
    printf("          4\n");

    printf("\n  逐个终点回溯打印（下标 0..5）：\n");
    for (int dst = 0; dst < g.n; dst++) {
        print_shortest_path(parent_naive, 0, dst, dist_naive);
    }

    printf("\n  手算核对：\n");
    printf("    dist[0]=0（就是起点）\n");
    printf("    dist[1]=7 （只有 0->1 这一条直连）\n");
    printf("    dist[2]=9 （0->2 直连 9，比绕 0->1->2 = 7+10 = 17 短）\n");
    printf("    dist[3]=20（0->2->3 = 9+11 = 20，比 0->1->3 = 7+15 = 22 短）\n");
    printf("    dist[4]=20（0->2->5->4 = 9+2+9 = 20，比 0->2->3->4 = 9+11+6 = 26 短）\n");
    printf("    dist[5]=11（0->2->5 = 9+2 = 11，比直连的 14 短 ★ 本节主角）\n");

    /* ============================================================
       第 4 幕：小顶堆优化版
       ============================================================ */
    section("幕 4 · 堆优化版 O((V+E)logV)：把「扫一遍找最小」换成「弹一个」");

    int dist_heap[MAX_V];
    int parent_heap[MAX_V];
    int stale = 0;
    int neg_heap = dijkstra_heap(&g, 0, dist_heap, parent_heap, 1, &stale);
    (void)neg_heap;

    printf("\n  两个版本逐格对照：\n");
    printf("    顶点   dist(朴素)  dist(堆)   parent(朴素)  parent(堆)   是否一致\n");
    int all_same = 1;
    for (int v = 0; v < g.n; v++) {
        int same = (dist_naive[v] == dist_heap[v]);   /* parent 可以不同：并列时选谁都可能 */
        if (!same) {
            all_same = 0;
        }
        printf("      %d      %5d     %5d        %3d          %3d        %s\n",
               v, dist_naive[v], dist_heap[v], parent_naive[v], parent_heap[v],
               same ? "✓" : "✗ 不一样！");
    }
    printf("\n  dist 数组两种写法完全一致：%s（若不一致说明有一处实现写错了，最常见的漏掉是判过期）\n",
           all_same ? "✓" : "✗");
    printf("  本节丢弃了 %d 条过期记录 —— 它们就是「历史照片」，靠 if (d > dist[u]) continue; 扔掉。\n", stale);

    /* ============================================================
       第 5 幕：负权反例 —— Dijkstra 为什么会错
       ============================================================ */
    section("幕 5 · 负权反例：A->B=2, A->C=5, C->B=-4，Dijkstra 会答错");

    /*  有向图，4 个顶点：A=0, B=1, C=2, D=3（D 故意孤立，用来演示「不可达」这个边界）
     *
     *        2
     *    A -------→ B
     *    │          ↑
     *  5 │        -4│（负权边：从 C 到 B 反而「赚钱」）
     *    │       ↗  │
     *    └──→ C ────┘
     *
     *  Dijkstra 的走法：定稿 A(0) → 松弛出 B=2, C=5 → 未定稿里 B 最小，于是【定稿 B=2】
     *  → 之后再看 C，发现 C->B 能让 B 变成 5 + (-4) = 1，可是 B 已经定稿了，
     *    「已定稿不再修改」这条规矩把它挡在门外，于是 B 的答案永远停在 2。
     *  正确答案是 1（走 A -> C -> B）。
     */
    Graph gn;
    graph_init(&gn, 4, 1);                 /* 1 = 有向图 */
    graph_add_edge(&gn, 0, 1, 2);          /* A -> B = 2  */
    graph_add_edge(&gn, 0, 2, 5);          /* A -> C = 5  */
    graph_add_edge(&gn, 2, 1, -4);         /* C -> B = -4 */
    graph_print_edges(&gn);
    printf("  顶点含义：A=0  B=1  C=2   （3 号点故意不连任何边，用来演示「不可达」）\n");

    int dist_neg[MAX_V];
    int parent_neg[MAX_V];
    printf("\n  ---- Dijkstra（朴素版）跑这张有负权边的图 ----\n");
    dijkstra_naive(&gn, 0, dist_neg, parent_neg, 1);

    int dist_brute[MAX_V];
    shortest_by_brute_force(&gn, 0, dist_brute);   /* 暴力枚举所有简单路径 = 标准答案 */

    printf("\n  ---- 答案对照 ----\n");
    printf("    顶点        Dijkstra 给的   正确答案(暴力)\n");
    const char name_of[4] = { 'A', 'B', 'C', 'D' };
    for (int v = 0; v < gn.n; v++) {
        char dcell[8], bcell[8];
        fmt_cell(dist_neg[v], dcell, sizeof dcell);
        fmt_cell(dist_brute[v], bcell, sizeof bcell);
        printf("      %c (%d)      %5s          %5s       %s\n",
               name_of[v], v, dcell, bcell,
               dist_neg[v] == dist_brute[v] ? "✓ 对" : "✗ 错！");
    }
    printf("\n  错在哪：B 在第 2 轮就被【定稿】成 2，而真正的最短路 A -> C -> B = 5 + (-4) = 1\n");
    printf("  要经过一个「比 B 更远」的中间点 C。Dijkstra 的致命假设是\n");
    printf("  「未定稿里 dist 最小的点，它的距离已经不可能再变小了」——\n");
    printf("  这个假设依赖【后面再走的边不会让总距离变小】，也就是边权非负。\n");
    printf("  负权边正好破坏了它：定稿之后的 B 再也拿不到第二次被改进的机会。\n");
    printf("  → 有负权就得用 07-graph/bellman-ford（不定稿，把所有边反复试 V-1 轮）。\n");

    printf("\n  顺便看看「不可达」这个边界情况（注意：下面 B 那行是 Dijkstra 的【错误】答案，\n");
    printf("  它不但距离错，路径也错，而且打印得整整齐齐、毫无异常 —— 所以才叫「安静地算错」）：\n");
    for (int dst = 0; dst < gn.n; dst++) {
        print_shortest_path(parent_neg, 0, dst, dist_neg);
    }

    /* ============================================================
       收尾
       ============================================================ */
    section("本节小结");
    printf("  1) Dijkstra = 「每轮从未定稿里挑 dist 最小的定稿，然后用它松弛邻居」。\n");
    printf("     贪心得敢下定稿，靠的是【边权非负】这个前提。\n");
    printf("  2) 朴素版 O(V^2) 适合稠密图；堆优化版 O((V+E)logV) 适合稀疏图。\n");
    printf("     堆版必须写 if (d > dist[u]) continue; 丢掉过期记录，因为我们只塞新的、不改旧的。\n");
    printf("  3) 负权边会让 Dijkstra 交出「看起来很合理」的错误答案（本节的 2 而不是 1），\n");
    printf("     它不会报错，只是安静地算错 —— 这种 bug 最危险。\n");
    printf("  4) dist 数组给距离，parent 数组给路径；两者要一起维护，只留 dist 就回不了路径。\n");
    return 0;
}
