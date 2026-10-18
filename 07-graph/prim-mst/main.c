#include <stdio.h>
#include "prim.h"

/* 一行分隔线。static = 只给本文件用，不会和其它目录的 section 撞名字 */
static void section(const char *title)
{
    printf("\n==================================================\n");
    printf("%s\n", title);
    printf("==================================================\n");
}

/* 把两条 MST 边集放在一起比：先看总权，再看边集是否一模一样 */
static void compare_two_msts(const MSTEdge a[], int na, const MSTEdge b[], int nb,
                             const char *la, const char *lb)
{
    int sa = 0, sb = 0;
    for (int i = 0; i < na; i++) {
        sa += a[i].weight;
    }
    for (int i = 0; i < nb; i++) {
        sb += b[i].weight;
    }

    printf("    %-14s 边数 %d  总权 %d\n", la, na, sa);
    printf("    %-14s 边数 %d  总权 %d\n", lb, nb, sb);

    /* 先算边集是否相同，再和总权配成一句人话 */
    int same_edges = (na == nb);
    for (int i = 0; same_edges && i < na; i++) {
        int found = 0;
        for (int j = 0; j < nb; j++) {
            if (a[i].u == b[j].u && a[i].v == b[j].v) {
                found = 1;
            }
        }
        if (!found) {
            same_edges = 0;
        }
    }

    if (sa != sb) {
        printf("    总权不同 ⇒ 它们优化的目标不一样（谁也不是谁的替代品）；"
               "边集%s。\n", same_edges ? "相同" : "也不同");
    } else if (same_edges) {
        printf("    总权相同、边集也相同 ⇒ 这张图的 MST 是唯一的。\n");
    } else {
        printf("    总权相同、边集不同 ⇒ 完全正常：MST 在有并列便宜的边时不唯一，"
               "两种写法都对。\n");
    }
}

int main(void)
{
    /* ============================================================
       第 1 幕：先把图和"生成树"这个概念摆清楚
       ============================================================ */
    section("幕 1 · 共用示例图（6 点 9 边，无向带权）");

    printf("  主链：(0) ──7── (1) ──10── (2) ──11── (3) ──6── (4) ──9── (5)\n");
    printf("  跨点：(0) ──9── (2)   (0) ──14── (5)   (1) ──15── (3)   (2) ──2── (5)\n");
    printf("  边表：0-1:7  0-2:9  0-5:14  1-2:10  1-3:15  2-3:11  2-5:2  3-4:6  4-5:9\n");

    MGraph g;
    mgraph_build_shared(&g);
    mgraph_print(&g, "邻接矩阵 w[u][v]（∞ = 没有这条边）");

    printf("\n  本节要解决的问题不是「从 A 到 B 怎么走最短」，而是：\n");
    printf("  「怎么用最少的钱，把所有点全连起来」—— 这叫【最小生成树】，MST。\n");
    printf("  合法生成树必须：覆盖 6 个点、正好 5 条边、不成环。\n");

    /* ============================================================
       第 2 幕：Prim 逐轮画面（本节最重要的输出）
       ============================================================ */
    section("幕 2 · Prim 的「养墙」过程：逐轮打印候选跨墙边");

    printf("  规则回顾：每一轮只看【墙内 → 墙外】这些跨墙边，\n");
    printf("  挑其中最便宜的一条，顺着它把那个墙外点圈进墙。一轮只进一个点 ⇒ 天生不成环。\n");

    MSTEdge mst[MAX_V];
    int cnt = 0;
    int total = prim_mst(&g, 0, mst, &cnt, 1);

    printf("\n  prim_mst() 返回值 = %d，边数 = %d\n", total, cnt);

    /* ============================================================
       第 3 幕：最终边集 + 自检 + 手算核对
       ============================================================ */
    section("幕 3 · 最终 MST 边集 / 合法性自检 / 手算核对");

    mst_print_edges(mst, cnt, "Prim 给出的最小生成树");
    mst_check_valid(&g, mst, cnt);

    printf("\n  这棵 MST 画出来长这样（注意它被拉成了一条链）：\n");
    printf("      1 ──7── 0 ──9── 2 ──2── 5 ──9── 4 ──6── 3\n");
    printf("    它只保证「5 条边把钱花得最省」，完全不关心 1 到 3 要跨几段（这里要跨 5 段）。\n");

    printf("\n  手算核对（这一步很重要，别只信代码）：\n");
    printf("    点 5 挂在树上的最便宜办法：2-5(2)。\n");
    printf("    点 3 挂在树上的最便宜办法：3-4(6)。\n");
    printf("    点 4 挂在树上的最便宜办法：3-4(6) 或 4-5(9) —— 两者只能留一条，否则 3-4-5-3 成环。\n");
    printf("    点 0、1 之间：0-1(7)。\n");
    printf("    把两侧连起来最便宜的桥：0-2(9)（比 1-2(10)、1-3(15)、0-5(14) 都便宜）。\n");
    printf("    ⇒ {0-1(7), 0-2(9), 2-5(2), 3-4(6), 4-5(9)} = 7+9+2+6+9 = 33。\n");

    printf("\n  「35 那棵树」差在哪（顺手验证它只是生成树，不是【最小】生成树）：\n");
    MSTEdge t35[5] = { {0, 1, 7}, {0, 2, 9}, {2, 5, 2}, {2, 3, 11}, {3, 4, 6} };
    mst_print_edges(t35, 5, "另一棵合法的生成树（点 3 用 2-3 挂上来）");
    mst_check_valid(&g, t35, 5);
    printf("    两棵树只差一条边，其余 4 条一模一样：\n");
    printf("      共同部分：0-1(7) + 0-2(9) + 2-5(2) + 3-4(6) = 24\n");
    printf("      35 那棵再补 2-3(11) ⇒ 24 + 11 = 35\n");
    printf("      Prim 那棵再补 4-5(9) ⇒ 24 +  9 = 33   ← 便宜 2\n");
    printf("    怎么想到换的：往 35 那棵树里加一条 4-5(9)，会成环 2-3-4-5-2，\n");
    printf("    环上最贵的是 2-3(11)，删掉它总权立刻降 2，树还是树 —— 这就是「交换性质」。\n");

    printf("\n  再验证一遍 Prim 结果 <= 上面这棵：33 <= 35 ✓（穷举所有生成树的最小值就是 33）\n");

    /* ============================================================
       第 4 幕：起点无关性
       ============================================================ */
    section("幕 4 · 换起点会改变答案吗？（MST 与起点无关）");

    for (int start = 0; start < g.n; start++) {
        MSTEdge m[MAX_V];
        int c = 0;
        int t = prim_mst(&g, start, m, &c, 0);
        printf("  start = %d → 边数 %d，总权 %d，边集：", start, c, t);
        for (int i = 0; i < c; i++) {
            printf(" %d-%d(%d)", m[i].u, m[i].v, m[i].weight);
        }
        printf("\n");
    }
    printf("\n  6 个起点的总权全是 33 ⇒ 总权与起点无关（这张图的 MST 恰好唯一，边集也相同）。\n");
    printf("  如果图里有并列便宜的边，换起点可能给出一棵【边集不同、总权相同】的 MST，\n");
    printf("  那不算 bug：题目一般只要求总权最小。\n");

    /* ============================================================
       第 5 幕：MST ≠ 最短路径树（本目录最重要的对照）
       ============================================================ */
    section("幕 5 · MST vs 最短路径树（SPT）：两个完全不同的东西");

    /*  用本目录自带的极简 Dijkstra（prim.c 第 3 部分）算出 parent[]，
     *  再把 parent 链上的边权加起来，就是「最短路径树」的总造价。 */
    int dist[MAX_V];
    int parent[MAX_V];
    spt_by_dijkstra(&g, 0, dist, parent);

    MSTEdge spt[MAX_V];
    int nspt = 0;
    int spt_total = 0;
    for (int v = 0; v < g.n; v++) {
        if (parent[v] != -1) {
            spt[nspt].u = parent[v] < v ? parent[v] : v;
            spt[nspt].v = parent[v] < v ? v : parent[v];
            spt[nspt].weight = g.w[parent[v]][v];
            spt_total += spt[nspt].weight;
            nspt++;
        }
    }

    printf("  最短路径树（根 = 0，让每个点到 0 都最便宜）：\n");
    printf("          0\n");
    printf("         / \\\n");
    printf("        1   2          ← 又矮又胖：任何点离根都很近\n");
    printf("           / \\\n");
    printf("          3   5\n");
    printf("              |\n");
    printf("              4\n");
    mst_print_edges(spt, nspt, "SPT 边集");
    printf("  它的 dist 数组：[");
    for (int v = 0; v < g.n; v++) {
        printf(" %d", dist[v]);
    }
    printf(" ]   ← 到点 3 的距离是 20（0->2->3）\n");

    printf("\n  Prim 的 MST：\n");
    mst_print_edges(mst, cnt, "MST 边集");

    printf("\n  两棵树的差距一目了然：\n");
    compare_two_msts(mst, cnt, spt, nspt, "MST(Prim)", "SPT(Dijkstra)");
    printf("    差的那条边：MST 用 4-5(9) 让点 4 进树，SPT 用 2-3(11) 让点 3 的距离保持 20。\n");
    printf("    拿 MST 当最短路会怎样？在 MST 里 0 到 3 只能走 0-2-5-4-3 = 9+2+9+6 = 26，\n");
    printf("    而真正的最短路是 20。反过来拿 SPT 铺线，造价 38 > 33，白花 5 元。\n");
    printf("  ★ 结论：MST 优化「全局总权」，SPT 优化「单源距离」。目标不同 ⇒ 结果不同，别混用。\n");

    /* ============================================================
       第 6 幕：并列便宜的边（tie-break）
       ============================================================ */
    section("幕 6 · 三条边一样贵的三角形：MST 不唯一");

    MGraph gt;
    mgraph_build_ties(&gt);
    mgraph_print(&gt, "三角形图（三条边权都是 1）");

    MSTEdge tie[MAX_V];
    int ctie = 0;
    int ttie = prim_mst(&gt, 0, tie, &ctie, 1);
    printf("\n");
    mst_print_edges(tie, ctie, "Prim 从这个三角形选出的边");
    printf("  总权 %d。任选两条边都是总权 2 的 MST，所以【边集不唯一】。\n", ttie);
    printf("  本实现 tie-break 规则：key 用严格小于，于是并列时优先选【下标小的墙外点】，\n");
    printf("  且 from[] 保持【最早给出这张票的墙内点】⇒ 结果里出现了 0-1 和 0-2。\n");
    printf("  （隔壁 kruskal-mst 用另一套 tie-break，会选 0-1 和 1-2：总权相同、边集不同。）\n");

    /* ============================================================
       第 7 幕：不连通图的边界情况
       ============================================================ */
    section("幕 7 · 图不连通：Prim 的边界情况");

    MGraph gd;
    mgraph_build_disconnected(&gd);
    mgraph_print(&gd, "两块互不相连的图（{0,1,2} 和 {3,4}）");

    MSTEdge dis[MAX_V];
    int cdis = 0;
    int rtn = prim_mst(&gd, 0, dis, &cdis, 1);
    printf("\n  prim_mst() 返回 %d（-1 = 失败），只收下了 %d 条边。\n", rtn, cdis);
    mst_print_edges(dis, cdis, "圈住的那一块的边");
    mst_check_valid(&gd, dis, cdis);
    printf("  解释：生成树【必须覆盖所有顶点】，而 3、4 这两点和 0 之间一条边都没有，\n");
    printf("  再怎么挑也连不上 ⇒ 这张图压根不存在生成树，返回值 -1 就是诚实的失败信号。\n");
    printf("  实际工程里对应的需求叫「最小生成森林」= 每个连通分量各求一棵 MST（Prim 只要\n");
    printf("  外层再对每个未访问分量各起一次即可；Kruskal 天然就会把每块都求出来）。\n");

    /* ============================================================
       收尾
       ============================================================ */
    section("本节小结");
    printf("  1) Prim = 「养墙」：每轮收【最便宜的跨墙边】，把对面的点圈进来，V-1 轮完工。\n");
    printf("  2) 一轮只加一个点 ⇒ 那条边的另一端从没进过树 ⇒ 不可能成环 ⇒ Prim 不需要判环。\n");
    printf("  3) 贪心安全的理由叫割性质（cut property）：跨任一割的最便宜边必属于某棵 MST。\n");
    printf("  4) 朴素版 O(V^2)，每轮扫一遍 key[]。稠密图（边多）用它最划算：\n");
    printf("     邻接矩阵 + 扫 key，正好和边数同量级；堆优化 O((V+E)logV) 在稀疏图上更值，见 README。\n");
    printf("  5) MST 总权与起点无关，但边集在有并列便宜边时不唯一；边权全不相等时 MST 唯一。\n");
    printf("  6) MST ≠ 最短路径树：一个管「总造价最低」，一个管「每个点离家最近」。\n");
    printf("  7) 图不连通就没有生成树，函数返回 -1，别硬把森林当树用。\n");
    return 0;
}
