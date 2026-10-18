#include <stdio.h>
#include "prim.h"

/* =====================================================================
 * 第 0 部分：建图
 * =====================================================================
 *
 * Prim 用【邻接矩阵】最顺手，因为它的核心动作是：
 *   "墙内每个点 → 墙外每个点，这些跨墙边里谁最便宜？"
 * 这本来就是对整张表的扫描，矩阵 O(1) 取值，写起来最短。
 *
 * 内存布局（n = 6，MAX_V = 8）：
 *
 *   实际用的只有左上角 6×6 这一块，外面两行两列是空着的（浪费但无害）
 *
 *          j=0   1    2    3    4    5    6   7
 *   w[0] [  ∞    7    9   14    ∞    ∞  | .   . ]
 *   w[1] [  7    ∞   10   15    ∞    ∞  | .   . ]     ← 沿主对角线镜像对称
 *   w[2] [  9   10    ∞   11    ∞    2  | .   . ]        这就是"无向图"在矩阵里的样子
 *   w[3] [ 14   15   11    ∞    6    ∞  | .   . ]
 *   w[4] [  ∞    ∞    ∞    6    ∞    9  | .   . ]
 *   w[5] [  ∞    ∞    2    ∞    9    ∞  | .   . ]
 *
 *   注意：这里 ∞ = INF_W 表示"没有这条边"，对角线也是 ∞（不允许自环）。
 *   Prim 只读矩阵不写它，所以不用担心 INF_W 参与加法溢出。
 */
void mgraph_init(MGraph *g, int n)
{
    if (n > MAX_V) {
        printf("[错误] 顶点数 %d 超过上限 %d\n", n, MAX_V);
        return;
    }
    g->n = n;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            g->w[i][j] = INF_W;            /* 先全部"没有边"，连上的再覆盖 */
        }
    }
}

void mgraph_add_edge(MGraph *g, int u, int v, int weight)
{
    if (u < 0 || u >= g->n || v < 0 || v >= g->n || u == v) {
        printf("[错误] 边 %d-%d 不合法（n=%d，且不允许自环）\n", u, v, g->n);
        return;
    }
    if (weight < 0) {
        /* 生成树的定义在负权上其实仍然成立，但负权会让"最小"的直觉变复杂，
         * 而且 Prim 的正确性证明只要求边可比大小。本节按约定只用正权。 */
        printf("[提醒] 边 %d-%d 权 %d 是负数，本节 demo 没准备这种图\n", u, v, weight);
    }
    if (weight < g->w[u][v]) {
        g->w[u][v] = weight;               /* 重边取最小，和矩阵类算法的惯例一致 */
        g->w[v][u] = weight;               /* 无向 ⇒ 对称两格一起写 */
    }
}

void mgraph_print(const MGraph *g, const char *title)
{
    printf("  %s：\n", title);
    printf("        j ");
    for (int j = 0; j < g->n; j++) {
        printf("%4d", j);
    }
    printf("\n      ------");
    for (int j = 0; j < g->n; j++) {
        printf("----");
    }
    printf("\n");
    for (int i = 0; i < g->n; i++) {
        printf("    i %d |", i);
        for (int j = 0; j < g->n; j++) {
            if (g->w[i][j] >= INF_W) {
                printf("   %s", "∞");
            } else {
                printf("%4d", g->w[i][j]);
            }
        }
        printf("\n");
    }
    /* 顺手自查一次对称性：不对称就说明加边时漏了反向那一格 */
    int symmetric = 1;
    for (int i = 0; i < g->n; i++) {
        for (int j = 0; j < g->n; j++) {
            if (g->w[i][j] != g->w[j][i]) {
                symmetric = 0;
            }
        }
    }
    printf("      自检：矩阵%s（无向图必须对称，否则是你加边时漏写了反向那一格）\n",
           symmetric ? "对称 ✓" : "【不对称】✗");
}

/*
 * 共用示例图（全仓库 5 节都用这一张，方便前后对照）：
 *   主链： (0) ──7── (1) ──10── (2) ──11── (3) ──6── (4) ──9── (5)
 *   跨点： (0) ──9── (2)   (0) ──14── (5)   (1) ──15── (3)   (2) ──2── (5)
 *
 * 这张图的最小生成树（用 Prim 或 Kruskal 都得到同一个总权）：
 *   边 = 2-5(2), 3-4(6), 0-1(7), 0-2(9), 4-5(9)  →  总权 = 33
 *   而最短路径树（起点 0）的边 = 0-1(7), 0-2(9), 2-5(2), 2-3(11), 5-4(9) → 总权 = 38
 *   两个差在一条边上：MST 用 3-4(6) 把点 3 挂上去，SPT 用 2-3(11) —— 因为 SPT 要保 3 的距离最短。
 */
void mgraph_build_shared(MGraph *g)
{
    mgraph_init(g, 6);
    mgraph_add_edge(g, 0, 1, 7);
    mgraph_add_edge(g, 0, 2, 9);
    mgraph_add_edge(g, 0, 5, 14);
    mgraph_add_edge(g, 1, 2, 10);
    mgraph_add_edge(g, 1, 3, 15);
    mgraph_add_edge(g, 2, 3, 11);
    mgraph_add_edge(g, 2, 5, 2);
    mgraph_add_edge(g, 3, 4, 6);
    mgraph_add_edge(g, 4, 5, 9);
}

/*
 * 三角形等权图：0-1(1) 1-2(1) 0-2(1)
 * 三条边一样便宜，最小生成树不唯一（任选两条都是总权 2）。
 * 用途：看 Prim 的 tie-break 规则 —— key 用严格小于，所以并列时选【下标最小】的墙外点，
 * 而它的 from[] 保持【最早写进去的那个墙内点】。
 * 结果：Prim 收 0-1 和 0-2；而 Kruskal 收 0-1 和 1-2。总权相同、边集不同。
 */
void mgraph_build_ties(MGraph *g)
{
    mgraph_init(g, 3);
    mgraph_add_edge(g, 0, 1, 1);
    mgraph_add_edge(g, 1, 2, 1);
    mgraph_add_edge(g, 0, 2, 1);
}

/*
 * 不连通图：{0,1,2} 一块，{3,4} 另一块，两块之间一条边都没有。
 * 结论：这种图【没有】生成树（生成树要求覆盖所有点且连通）。
 * Prim 会只圈住起点那一块就停，返回 -1；得到的其实是"最小生成森林"里的一棵树。
 */
void mgraph_build_disconnected(MGraph *g)
{
    mgraph_init(g, 5);
    mgraph_add_edge(g, 0, 1, 3);
    mgraph_add_edge(g, 1, 2, 4);
    mgraph_add_edge(g, 3, 4, 5);
}

/* =====================================================================
 * 第 1 部分：Prim 主函数
 * =====================================================================
 *
 * 数据结构（三张长度 n 的小数组，这就是 Prim 的全部内存）：
 *
 *   in_wall[v] ：1 = v 已经被圈进墙里（已进树）
 *   key[v]     ：v 到墙的【最低门票】= min{ w[u][v] | u 在墙内 }。墙外点才有意义
 *   from[v]    ：这张最低门票是从墙内哪个点 u 伸出来的（用来还原那条边）
 *
 *   下标 v ：   0     1     2     3     4     5
 *   in_wall：   1     1     1     0     0     0      ← 墙内 {0,1,2}
 *   key   ： [  0,    7,    9,   15,  INF,    2  ]   ← 墙外点的最低门票，INF = 还跨不过去
 *   from  ： [ -1,    0,    0,    1,   -1,    2  ]
 *                                          ↑                  ↑
 *                                     门票最便宜的是 5（2 元）→ 本轮圈进 5，加边 2-5
 *
 * 每轮两步：
 *   1) 在墙外点里挑 key 最小的 u（并列取下标小的），圈进墙；把边 (from[u], u) 记进 MST
 *   2) 用新来的 u 更新所有墙外点的门票：w[u][v] < key[v] 就换票（换得更便宜才换）
 *
 * 为什么"挑最便宜的跨墙边"是安全的？（cut property，割性质）
 *   墙把点集切成 (墙内 S, 墙外 V-S) 这个"割"。任何一棵生成树都必须至少有 1 条边跨过这个割。
 *   设跨割最便宜的边是 e = (u, v)。存在某棵 MST 包含 e：
 *     任取一棵 MST T，若 T 已经有 e 就完事；若没有，T 必须用另一条跨割边 f 把两侧连起来。
 *     把 e 加进 T 会成一个环，这个环上一定有 f；删掉 f 得到 T'，仍连通且边数不变，
 *     而 w(e) <= w(f)，所以 T' 也是 MST 并且含 e。∎
 *   于是"每轮贪心地收最便宜的跨墙边"不会把答案带偏。
 */
int prim_mst(const MGraph *g, int start, MSTEdge out[], int *cnt_out, int show_steps)
{
    int in_wall[MAX_V];
    int key[MAX_V];
    int from[MAX_V];

    if (start < 0 || start >= g->n) {
        printf("[错误] 起点 %d 越界\n", start);
        return -1;
    }

    for (int v = 0; v < g->n; v++) {
        in_wall[v] = 0;
        key[v] = INF_W;         /* 一开始墙内啥也没有，谁都还没有门票 */
        from[v] = -1;
    }
    key[start] = 0;             /* 起点的门票是 0：不需要任何边就能"在自己家里" */

    int cnt = 0;
    int total = 0;
    int added = 0;

    for (int round = 1; round <= g->n; round++) {
        /* ---- 第 1 步：在墙外点里找 key 最小的那个 ---- */
        int u = -1;
        for (int v = 0; v < g->n; v++) {
            if (!in_wall[v] && key[v] < INF_W && (u == -1 || key[v] < key[u])) {
                u = v;          /* 严格小于 ⇒ 并列时保留先扫到的，也就是下标小的 */
            }
        }

        if (u == -1) {
            /* 墙外还有点，但没有一张门票 —— 说明这些点和墙之间一条边都没有 ⇒ 图不连通 */
            if (show_steps) {
                printf("\n      第 %d 轮：墙外还剩点，但它们的 key 全是 ∞（没有任何跨墙边）\n", round);
                printf("            ⇒ 这张图【不连通】，压根不存在生成树。已经圈住的点是其中一个连通分量的 MST。\n");
            }
            if (cnt_out) {
                *cnt_out = cnt;
            }
            return -1;
        }

        if (show_steps) {
            printf("\n      ---- 第 %d 轮 ----\n", round);
            printf("      当前墙内（已进树）= { ");
            for (int v = 0; v < g->n; v++) {
                if (in_wall[v]) {
                    printf("%d ", v);
                }
            }
            printf("}   墙外 = { ");
            for (int v = 0; v < g->n; v++) {
                if (!in_wall[v]) {
                    printf("%d ", v);
                }
            }
            printf("}\n");

            /* ---- 候选跨墙边清单：把每一条 墙内 - 墙外 的边都列出来 ---- */
            int ncand = 0;
            printf("      候选跨墙边（墙内 → 墙外，一条都不能少地全列出来）：");
            for (int a = 0; a < g->n; a++) {
                if (!in_wall[a]) {
                    continue;               /* a 必须在墙里 */
                }
                for (int b = 0; b < g->n; b++) {
                    if (in_wall[b] || g->w[a][b] >= INF_W) {
                        continue;           /* b 必须在墙外，而且 a、b 之间真有边 */
                    }
                    int is_picked = ((a == from[u] && b == u) || (b == from[u] && a == u));
                    printf(" %d-%d(%d)%s", a, b, g->w[a][b], is_picked ? "★" : "");
                    ncand++;
                }
            }
            if (ncand == 0) {
                printf("（还没有跨墙边：墙此刻是空的，第一轮只是把起点单独圈进来）\n");
                printf("      ⇒ 本轮无候选可选，成本 0，直接进入「用起点给邻居报价」那一步\n");
            } else {
                printf("\n      共 %d 条候选，最便宜的一条 = %d-%d，权 %d ★\n",
                       ncand, from[u], u, key[u]);
            }
        }

        /* ---- 第 2 步：把 u 圈进墙，并收下这条边 ---- */
        in_wall[u] = 1;
        added++;
        if (from[u] != -1) {                /* 起点没有来路边，别把 (-1, start) 记成边 */
            out[cnt].u = from[u] < u ? from[u] : u;      /* 小的写前面，纯粹为了打印好看 */
            out[cnt].v = from[u] < u ? u : from[u];
            out[cnt].weight = g->w[from[u]][u];
            total += out[cnt].weight;
            cnt++;
            if (show_steps) {
                printf("      → 圈进顶点 %d，收下边 %d-%d（权 %d）\n",
                       u, out[cnt - 1].u, out[cnt - 1].v, out[cnt - 1].weight);
            }
        } else if (show_steps) {
            printf("      → 圈进起点 %d（它不需要边，成本 0）\n", u);
        }

        /* ---- 第 3 步：用新来的 u 给墙外点换更便宜的门票 ---- */
        int nupdate = 0;
        for (int v = 0; v < g->n; v++) {
            if (in_wall[v] || g->w[u][v] >= INF_W) {
                continue;
            }
            if (g->w[u][v] < key[v]) {      /* 只有更便宜才换票；相等不换（保持先来者）*/
                key[v] = g->w[u][v];
                from[v] = u;
                nupdate++;
            }
        }

        if (show_steps) {
            printf("      用顶点 %d 重新报价：更新了 %d 张门票\n", u, nupdate);
            printf("      墙外各点的最低门票 key[]：[");
            for (int v = 0; v < g->n; v++) {
                if (in_wall[v]) {
                    printf("   --,");        /* 已进树的点不需要门票了 */
                } else if (key[v] >= INF_W) {
                    printf("   ∞,");
                } else {
                    printf("%4d,", key[v]);
                }
            }
            printf("]\n      门票来自墙内哪个点 from[]：[");
            for (int v = 0; v < g->n; v++) {
                if (in_wall[v]) {
                    printf("   --,");
                } else {
                    printf("%4d,", from[v]);
                }
            }
            printf("]  （-- = 已在墙内）\n");
            /* 当前 MST 边集：就地打一行，方便和上面的候选清单对着看。
             * 没用 mst_print_edges() 是因为那个函数打的是多行带标题的详细版，
             * 逐轮画面要的是"一屏一轮"，所以这里手写一个紧凑版。 */
            printf("      当前 MST 边集：");
            if (cnt == 0) {
                printf("（还空着）");
            }
            for (int i = 0; i < cnt; i++) {
                printf(" %d-%d(%d)", out[i].u, out[i].v, out[i].weight);
            }
            printf("\n");
            printf("      累计权重 = %d\n", total);
        }
    }

    if (cnt_out) {
        *cnt_out = cnt;
    }
    if (show_steps) {
        printf("\n      一共圈进 %d 个点，收下 %d 条边（应当正好是 V-1 = %d），总权 = %d\n",
               added, cnt, g->n - 1, total);
    }
    return total;
}

/* =====================================================================
 * 第 2 部分：打印与自检
 * ===================================================================== */
void mst_print_edges(const MSTEdge edges[], int cnt, const char *title)
{
    printf("  %s（共 %d 条边）：\n", title, cnt);
    int sum = 0;
    for (int i = 0; i < cnt; i++) {
        printf("      第 %d 条:  %d <-> %d   权 %d\n", i + 1, edges[i].u, edges[i].v, edges[i].weight);
        sum += edges[i].weight;
    }
    printf("      边权求和 = %d\n", sum);
}

/*
 * 自检：这三条同时成立，才敢说自己算出来的真的是"最小【生成】树"。
 *   1) 恰好 V-1 条边
 *   2) 每条边在原图里真的存在（防止自己幻觉出一条边）
 *   3) 覆盖所有顶点（等价于：从任意一条边的端点出发 BFS，能走满 V 个点）
 * 有了 (1) + (3)，环是不可能存在的：连通的 V 点 V-1 边图必然是树。
 * 但为了讲清楚，这里还是显式做一次判环（用一个最土的"父亲数组"式并查集，
 * 真正带路径压缩 + 按秩合并的版本在 07-graph/kruskal-mst 那节，它是主角）。
 */
int mst_check_valid(const MGraph *g, const MSTEdge edges[], int cnt)
{
    int ok = 1;

    if (cnt != g->n - 1) {
        printf("      自检 ✗ 边数 = %d，但生成树要求 V-1 = %d\n", cnt, g->n - 1);
        ok = 0;
    }

    /* 条件 2：每条边都得是原图里真有的边 */
    for (int i = 0; i < cnt; i++) {
        if (g->w[edges[i].u][edges[i].v] >= INF_W) {
            printf("      自检 ✗ 边 %d-%d 在原图里不存在！（幻觉边）\n", edges[i].u, edges[i].v);
            ok = 0;
        }
    }

    /* 条件 3：用一条最土的并查集把边并起来，顺便判环 */
    int fa[MAX_V];
    for (int v = 0; v < g->n; v++) {
        fa[v] = v;
    }
    for (int i = 0; i < cnt; i++) {
        int a = edges[i].u;
        int b = edges[i].v;
        while (fa[a] != a) {
            a = fa[a];
        }
        while (fa[b] != b) {
            b = fa[b];
        }
        if (a == b) {
            printf("      自检 ✗ 边 %d-%d 两端早就在一个集合里 ⇒ 出现环了，这不是生成树\n",
                   edges[i].u, edges[i].v);
            ok = 0;
        } else {
            fa[a] = b;
        }
    }

    /* 条件 4：全部点被连上 */
    int root = -1;
    for (int v = 0; v < g->n; v++) {
        int r = v;
        while (fa[r] != r) {
            r = fa[r];
        }
        if (root == -1) {
            root = r;
        } else if (r != root) {
            printf("      自检 ✗ 顶点 %d 和其他点没连通\n", v);
            ok = 0;
        }
    }

    if (ok) {
        printf("      自检 ✓ 边数 = V-1、每条边真实存在、连通、无环 ⇒ 这是一棵合法的生成树\n");
    }
    return ok;
}

/* =====================================================================
 * 第 3 部分：最短路径树（极简 Dijkstra，只为对照）
 * =====================================================================
 * 邻接矩阵版朴素 Dijkstra：O(V^2)。本目录自带一份，不去 import 隔壁目录。
 * 它给出的 parent[] 连起来就是"最短路径树"，把它的边权和算出来跟 MST 比。
 */
void spt_by_dijkstra(const MGraph *g, int src, int dist[], int parent[])
{
    int done[MAX_V];
    for (int v = 0; v < g->n; v++) {
        dist[v] = INF_W;
        parent[v] = -1;
        done[v] = 0;
    }
    dist[src] = 0;

    for (int round = 0; round < g->n; round++) {
        int u = -1;
        for (int v = 0; v < g->n; v++) {
            if (!done[v] && dist[v] < INF_W && (u == -1 || dist[v] < dist[u])) {
                u = v;
            }
        }
        if (u == -1) {
            break;                      /* 剩下的点不可达 */
        }
        done[u] = 1;
        for (int v = 0; v < g->n; v++) {
            if (done[v] || g->w[u][v] >= INF_W) {
                continue;
            }
            if (dist[u] + g->w[u][v] < dist[v]) {
                dist[v] = dist[u] + g->w[u][v];
                parent[v] = u;
            }
        }
    }
}
