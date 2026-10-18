#include <stdio.h>
#include "bellman.h"

/* 本文件里中文引号一律用「」：ASCII 双引号写在字符串里会把字符串提前结束。 */

/* =====================================================================
 * 第 0 部分：建图
 * ===================================================================== */

/*
 * Bellman-Ford 只需要边表，不需要邻接矩阵。
 * 原因很直白：它每轮的循环体就是「把 m 条边一条条拿出来试」，
 * 邻接矩阵反而要多做 V^2 次"这两点之间有没有边"的判断，纯浪费。
 * 这也是 BF 是"面向边"算法的典型：复杂度天生写成 O(V * E)。
 */
void graph_init(Graph *g, int n, int directed)
{
    if (n > MAX_V) {
        printf("[错误] 顶点数 %d 超过上限 %d\n", n, MAX_V);
        return;
    }
    g->n = n;
    g->directed = directed;
    g->m = 0;
}

/*
 * 加一条边。无向图存两条弧，因为「u 能到 v」和「v 能到 u」都得让松弛时试到。
 *
 * ⚠ 边界情况：无向图里如果真有一条负权边，那么它存成的两条弧 u->v 和 v->u
 *   加起来就是 2 * w < 0 —— 于是【任意一条负权无向边本身就是一个负环】。
 *   所以「带负权的无向图」问最短路基本没有意义，本节的两张负权演示图都刻意用有向图。
 */
void graph_add_edge(Graph *g, int u, int v, int weight)
{
    if (u < 0 || u >= g->n || v < 0 || v >= g->n) {
        printf("[错误] 边 %d->%d 顶点越界（n=%d）\n", u, v, g->n);
        return;
    }
    if (g->m + 2 > MAX_E) {
        printf("[错误] 弧数超过上限 %d，忽略\n", MAX_E);
        return;
    }
    g->e[g->m].from = u; g->e[g->m].to = v; g->e[g->m].weight = weight;
    g->m++;
    if (!g->directed) {
        g->e[g->m].from = v; g->e[g->m].to = u; g->e[g->m].weight = weight;
        g->m++;
    }
}

void graph_print_edges(const Graph *g)
{
    printf("  %-12s: %s\n", "图的类型", g->directed ? "有向图" : "无向图");
    if (g->directed) {
        printf("  顶点数 = %d, 弧数 = %d\n", g->n, g->m);
        for (int i = 0; i < g->m; i++) {
            printf("      弧 %2d:  %d --> %-2d   权 %3d\n", i,
                   g->e[i].from, g->e[i].to, g->e[i].weight);
        }
    } else {
        printf("  顶点数 = %d, 边数 = %d（无向边存成两条弧，弧数 = %d）\n", g->n, g->m / 2, g->m);
        for (int i = 0; i + 1 < g->m; i += 2) {
            printf("      边 %2d:  %d <-> %-2d   权 %3d\n", i / 2,
                   g->e[i].from, g->e[i].to, g->e[i].weight);
        }
    }
}

/*
 * 演示图 ①：带负权，但没有负环。4 个顶点，5 条有向边。
 *
 *      (0) ─────4────> (1) ─────2────> (3)
 *        │               ^              ^
 *        │ 1             │ -3           │ 5
 *        ▼               │              │
 *      (2) ──────────────┴──────────────┘
 *
 *    0 -> 1 =  4
 *    0 -> 2 =  1
 *    2 -> 1 = -3      ← 负权边：走它反而「省钱」
 *    1 -> 3 =  2
 *    2 -> 3 =  5
 *
 * 正确答案（从 0 出发）：
 *    dist[0] =  0
 *    dist[2] =  1           （0 -> 2）
 *    dist[1] = -2          （0 -> 2 -> 1 = 1 + (-3)，比直连的 4 短得多！）
 *    dist[3] =  0          （0 -> 2 -> 1 -> 3 = 1 - 3 + 2）
 *
 * ⚠ 边的【存放顺序】是故意排成「倒着」的（1->3、2->3、2->1 排在前面，0->2、0->1 排在最后）：
 *   这样每一轮信息只能往前推一格，你就能看到「逐轮逼近」的完整过程。
 *   如果按 0->2、0->1、2->1、1->3、2->3 这种「顺着」的顺序存，一轮就全算完了 ——
 *   结果一模一样，但你看不见过程。算法正确性不依赖边序，【收敛快慢】依赖边序，
 *   这一点正是 SPFA（队列优化）想解决的问题，详见 README。
 */
void graph_build_neg_no_cycle(Graph *g)
{
    graph_init(g, 4, 1);
    graph_add_edge(g, 1, 3,  2);   /* 故意排在最前：第 1 轮时 dist[1] 还是 ∞，这条边只能空跑 */
    graph_add_edge(g, 2, 3,  5);
    graph_add_edge(g, 2, 1, -3);   /* 负权边 */
    graph_add_edge(g, 0, 2,  1);
    graph_add_edge(g, 0, 1,  4);
}

/*
 * 演示图 ②：含负环。4 个顶点，其中 3 号点不连任何边（顺便演示"不可达"）。
 *
 *      (0) ──1──> (1) ──(-3)──> (2)
 *                  (1) <──(-1)── (2)      ← 1 -> 2 -> 1 的总权 = -3 + (-1) = -4 < 0
 *
 *   这个环每绕一圈就把 dist 减掉 4，所以"最短路"可以无限小 —— 没有答案。
 *   Bellman-Ford 干的事就是【明确告诉你这件事】，而不是硬给一个数。
 */
void graph_build_neg_cycle(Graph *g)
{
    graph_init(g, 4, 1);
    graph_add_edge(g, 0, 1,  1);
    graph_add_edge(g, 1, 2, -3);
    graph_add_edge(g, 2, 1, -1);
    /* 3 号点没有任何边：不可达，dist 应该一直保持 ∞ */
}

/*
 * 演示图 ③：全仓库共用的那张无向带权图（无负权），用来和 Dijkstra 对照。
 *   主链： (0) ──7── (1) ──10── (2) ──11── (3) ──6── (4) ──9── (5)
 *   跨点： (0) ──9── (2)   (0) ──14── (5)   (1) ──15── (3)   (2) ──2── (5)
 * 参考答案（Dijkstra 与 Bellman-Ford 都必须给出）：dist = [0, 7, 9, 20, 20, 11]
 */
void graph_build_shared(Graph *g)
{
    graph_init(g, 6, 0);
    graph_add_edge(g, 0, 1, 7);
    graph_add_edge(g, 0, 2, 9);
    graph_add_edge(g, 0, 5, 14);
    graph_add_edge(g, 1, 2, 10);
    graph_add_edge(g, 1, 3, 15);
    graph_add_edge(g, 2, 3, 11);
    graph_add_edge(g, 2, 5, 2);
    graph_add_edge(g, 3, 4, 6);
    graph_add_edge(g, 4, 5, 9);
}

/* =====================================================================
 * 打印小工具（每节自包含重写一份，代码量很小，好处是目录能单独编译）
 * ===================================================================== */

static void fmt_val(int v, char *buf, size_t sz)
{
    /* ∞ 在 UTF-8 里占 3 个字节，printf 的 %4s 按字节补位会错位，
     * 所以手工补 3 个空格：显示出来正好和 %4d 的 4 列对齐 */
    if (v >= INF_DIST) {
        snprintf(buf, sz, "   %s", "∞");
    } else if (v <= -INF_DIST) {
        snprintf(buf, sz, "  -%s", "∞");
    } else {
        snprintf(buf, sz, "%4d", v);
    }
}

void print_dist_row(const char *label, const int dist[], int n)
{
    char cell[8];
    printf("        %-10s [", label);
    for (int v = 0; v < n; v++) {
        fmt_val(dist[v], cell, sizeof cell);
        printf("%s", cell);
        if (v + 1 < n) {
            printf(", ");
        }
    }
    printf(" ]\n");
}

void print_parent_row(const char *label, const int parent[], int n)
{
    printf("        %-10s [", label);
    for (int v = 0; v < n; v++) {
        printf("%4d", parent[v]);
        if (v + 1 < n) {
            printf(", ");
        }
    }
    printf(" ]\n");
}

/* 回溯打印路径。同 dijkstra 那节：从终点沿 parent 往回跳，攒进数组再倒着打 */
void print_path(const int parent[], int src, int dst, const int dist[])
{
    int trail[MAX_V];
    int len = 0;

    if (dist[dst] >= INF_DIST) {
        printf("    从 %d 到 %d：不可达（dist 仍是 ∞）\n", src, dst);
        return;
    }
    int cur = dst;
    while (cur != src && cur != -1 && len < MAX_V) {
        trail[len++] = cur;
        cur = parent[cur];
    }
    trail[len++] = src;

    printf("    从 %d 到 %d：", src, dst);
    for (int i = len - 1; i >= 0; i--) {
        printf("%d", trail[i]);
        if (i > 0) {
            printf(" -> ");
        }
    }
    printf("   (%d)\n", dist[dst]);
}

/* =====================================================================
 * 第 1 部分：Bellman-Ford 主算法
 * =====================================================================
 *
 * 代码短得离谱，核心就是三层：轮数 × 边数 × 一次比较。
 *
 *   for (round = 1; round <= V-1; round++)
 *       for (每条边 u -> v，权 w)
 *           if (dist[u] 已知 && dist[u] + w < dist[v])
 *               dist[v] = dist[u] + w, parent[v] = u
 *
 * 为什么「V-1 轮」一定够？（这才是本节要理解的东西，不是代码）
 *   最短路径不会含环（含环的话把环去掉更短或等长；负环则根本没有最短路）。
 *   V 个点的路径不重复经过点，最多 V-1 条边。
 *    induction（归纳）：第 1 轮结束时，所有「1 条边就到」的点距离已正确；
 *   假设第 k-1 轮结束时「最多 k-1 条边」的路都正确，
 *   那么一条 k 条边的最短路，它的前 k-1 条边已经正确，第 k 条边在这一轮被试到，于是也正确。
 *   所以第 k 轮把「k 条边」的层级推平。推 V-1 轮，全部层级推平。
 *
 * 为什么要判 dist[u] != ∞？
 *   因为 ∞ 是个大整数 1e9，加上一个负权就会变成一个"看起来有限"的假距离，
 *   于是本来不可达的点会被"算出"一个距离 —— 这是非常经典的错误。
 *   所以松弛前必须先把"还到不了"的边跳过。
 *
 * 提前结束（第 k 轮一条边都没改）为什么可以 break？
 *   一轮没改说明 dist 已经是这个不动点了，后面再多来几轮也不会改。
 *   同时也说明【没有负环】—— 有负环的话每一轮一定会改。
 *
 * 第 V 轮还能松弛 ⇒ 负环，为什么？
 *   V-1 轮已经把"最多 V-1 条边"的所有路径推平了，还能变短就说明
 *   存在一条"用了 V 条边却更短"的路，而 V 条边经过 V+1 个点，6 个点里必有重点，
 *   也就是绕了环；绕环还能更短，那这个环总权必为负。
 */
int bellman_ford(const Graph *g, int src, int dist[], int parent[], int show_steps,
                 int *rounds_used_out, int *bad_from_out, int *bad_to_out)
{
    if (src < 0 || src >= g->n) {
        printf("[错误] 起点 %d 越界\n", src);
        return -1;
    }

    for (int v = 0; v < g->n; v++) {
        dist[v] = INF_DIST;
        parent[v] = -1;
    }
    dist[src] = 0;

    if (show_steps) {
        printf("        开局：dist[%d] = 0，其余 ∞（表示「还不知道能不能到」）。边表共 %d 条弧，一轮就把它们全试一遍。\n",
               src, g->m);
        print_dist_row("dist", dist, g->n);
    }

    int total_relax = 0;                     /* 全程一共发生过多少次松弛，只为打印统计用 */

    /* ---------- 主循环：V-1 轮 ---------- */
    for (int round = 1; round <= g->n - 1; round++) {
        int changed = 0;                     /* 本轮改动了多少处；一处都没改就说明已经收敛 */

        for (int i = 0; i < g->m; i++) {
            int u = g->e[i].from;
            int v = g->e[i].to;
            int w = g->e[i].weight;

            if (dist[u] >= INF_DIST) {
                continue;                  /* ★ 到不了的点，不能拿它去更新别人（否则 ∞ + 负数 = 假距离）*/
            }
            if (dist[u] + w < dist[v]) {   /* 松弛：绕 u 去 v 更近，就改 */
                dist[v] = dist[u] + w;
                parent[v] = u;
                changed++;
                total_relax++;
                if (show_steps) {
                    printf("              松弛 %d -> %d（权 %3d）: dist[%d] = %d\n", u, v, w, v, dist[v]);
                }
            }
        }

        if (show_steps) {
            print_dist_row("轮末 dist", dist, g->n);
            printf("            第 %d 轮结束：本轮改动了 %d 处。", round, changed);
        }

        if (changed == 0) {
            if (show_steps) {
                printf("一处都没改 ⇒ 已经收敛，再多跑几轮也只是重复同样的结果 → 提前结束。\n");
                printf("            顺带得到一个结论：这张图【没有负环】，因为有负环时每轮一定还在变小。\n");
                printf("            全程共发生 %d 次松弛，只用了 %d 轮（上限本可以是 V-1 = %d 轮）。\n",
                       total_relax, round, g->n - 1);
            }
            if (rounds_used_out) {
                *rounds_used_out = round;
            }
            if (bad_from_out) *bad_from_out = -1;
            if (bad_to_out) *bad_to_out = -1;
            return 0;
        }
        if (show_steps) {
            if (round < g->n - 1) {
                printf("还有改动 → 进入第 %d 轮（最多跑到 V-1 = %d 轮）。\n", round + 1, g->n - 1);
            } else {
                printf("已经跑满 V-1 = %d 轮，还有改动 → 再跑第 V 轮，专门用来判负环。\n", g->n - 1);
            }
        }
    }

    /* ---------- 第 V 轮：只用来判负环 ----------
     * 不真的写回 dist（只报告），因为一旦有负环，dist 里存什么都已经不是"最短路"了。
     */
    int still_relaxable = 0;
    int bad_from = -1, bad_to = -1, bad_w = 0, bad_new = 0;
    for (int i = 0; i < g->m; i++) {
        int u = g->e[i].from;
        int v = g->e[i].to;
        int w = g->e[i].weight;
        if (dist[u] >= INF_DIST) {
            continue;
        }
        if (dist[u] + w < dist[v]) {
            still_relaxable++;
            if (bad_from < 0) {          /* 记下第一条"还能松弛"的边，作为负环的证据 */
                bad_from = u;
                bad_to = v;
                bad_w = w;
                bad_new = dist[u] + w;
            }
        }
    }

    if (rounds_used_out) {
        *rounds_used_out = g->n - 1;
    }
    if (bad_from_out) *bad_from_out = bad_from;
    if (bad_to_out) *bad_to_out = bad_to;

    if (still_relaxable > 0) {
        if (show_steps) {
            printf("\n        ★ 第 %d 轮（也就是第 V 轮）竟然还能松弛 %d 处！\n", g->n, still_relaxable);
            printf("          证据边：%d -> %d（权 %d）：dist[%d] 还能从 %d 被改成 %d\n",
                   bad_from, bad_to, bad_w, bad_to, dist[bad_to], bad_new);
            printf("          ⇒ 检测到负环。这条边上一定挂着（或能被它拉到）一个总权为负的环。\n");
            printf("          ⇒ 此时 dist 数组里的值【全部作废】，它只是「第 V-1 轮结束时刚好停在哪算哪」。\n");
        }
        return 1;
    }

    if (show_steps) {
        printf("\n        第 %d 轮（第 V 轮）一条边都松弛不动 ⇒ 没有负环，答案可信。\n", g->n);
        printf("        全程共 %d 轮、%d 次松弛（上限本应是 V-1 = %d 轮 × %d 条弧）。\n",
               g->n - 1, total_relax, g->n - 1, g->m);
    }
    return 0;
}

/* =====================================================================
 * 第 2 部分：只为对照而写的极简 Dijkstra（朴素版，20 行）
 * =====================================================================
 * 本目录不许 include 隔壁 07-graph/dijkstra 的文件，所以抄一份最小的。
 * 用它可以验证"没有负权时，Bellman-Ford 和 Dijkstra 给出完全相同的 dist"。
 * ⚠ 有负权时它的结果是错的，那种图别拿来对照。
 */
void mini_dijkstra(const Graph *g, int src, int dist[])
{
    int done[MAX_V];
    for (int v = 0; v < g->n; v++) {
        dist[v] = INF_DIST;
        done[v] = 0;
    }
    dist[src] = 0;

    for (int round = 0; round < g->n; round++) {
        int u = -1;
        for (int v = 0; v < g->n; v++) {                 /* 未定稿里挑 dist 最小的 */
            if (!done[v] && dist[v] < INF_DIST && (u == -1 || dist[v] < dist[u])) {
                u = v;
            }
        }
        if (u == -1) {
            break;                                       /* 剩下的都不可达 */
        }
        done[u] = 1;
        for (int i = 0; i < g->m; i++) {                 /* 拿 u 松弛它的出边 */
            if (g->e[i].from != u || done[g->e[i].to]) {
                continue;
            }
            if (dist[u] + g->e[i].weight < dist[g->e[i].to]) {
                dist[g->e[i].to] = dist[u] + g->e[i].weight;
            }
        }
    }
}
