#ifndef PRIM_H
#define PRIM_H

/* =====================================================================
 * 这一节：Prim 最小生成树（Minimum Spanning Tree, MST）—— 朴素 O(V^2) 版
 * =====================================================================
 *
 * 先搞清楚"生成树"是什么。给定一张连通的无向带权图：
 *   生成树 = 挑出若干条边，满足三个条件
 *     1) 覆盖所有顶点（一个都不能少）
 *     2) 恰好 V-1 条边
 *     3) 不成环
 *   三条同时满足，它一定是一棵"Spanning"所有点的树。
 *   最小生成树 = 在所有生成树里，边权【总和】最小的那一棵。
 *
 * 生活例子：6 个村子要铺光缆，能铺的线路和造价都给你了。
 *   要求"任意两村之间都能通"且"总造价最低"，且"多余的一条线就是浪费"。
 *   这就是 MST。注意它【不关心】从 A 到 B 要经过几段（那是最短路），
 *   它只关心"总花费最少 + 全都连上"。
 *
 * ★ 全节最重要的一句话：
 *   最小生成树 ≠ 最短路径树。它们优化的东西根本不同：
 *     MST   ：所有边加起来最小（不 care 任何一个点到根的距离）
 *     SPT   ：每个点到起点的距离最小（边权和通常更大）
 *   本目录会用同一张图把两棵树都算出来，让你看见它们的边集不一样、总权不一样。
 *
 * Prim 的养墙比喻（大白话）：
 *   想象你站在一块地上，四周砌了一堵墙，墙里 = 已经进树的点。
 *   每一轮，你只看【从墙内出发、跨过墙、连到一个墙外点】的这些边
 *   （术语叫"割边 / crossing edge"），挑里面最便宜的一条，
 *   顺着它把那个点圈进墙里。V-1 轮以后墙圈住了所有点，就完工了。
 *
 *   为什么它永远不成环？因为每一轮【只新增一个点】，新点靠一条新边连进来，
 *   那条边的另一端在墙里、这一端从没出现过 —— 不可能构成回路。
 *   这也是它和 Kruskal 的本质区别：Kruskal 每次加一条边（可能一次并两棵子树），
 *   必须用并查集判环；Prim 每次加一个点，天生不会成环，不需要判环。
 *
 * ⚠ 本项目硬约定：只用 int，不用 void*、不用函数指针。
 */

#define MAX_V   8
#define INF_W   1000000000    /* "两点之间没有边"的替身；取大但不参与加法，不会溢出 */

/* 一条被选进生成树的边。u < v 只是打印好看，算法不依赖这个顺序 */
typedef struct {
    int u;
    int v;
    int weight;
} MSTEdge;

/* 无向带权图：邻接矩阵存，w[u][v] = w[v][u]，INF_W 表示没有这条边 */
typedef struct {
    int n;
    int w[MAX_V][MAX_V];
} MGraph;

void mgraph_init(MGraph *g, int n);
void mgraph_add_edge(MGraph *g, int u, int v, int weight);   /* 无向：对称写两格 */
void mgraph_print(const MGraph *g, const char *title);

void mgraph_build_shared(MGraph *g);      /* 全仓库共用的 6 点图（MST 总权应为 33，见 README） */
void mgraph_build_ties(MGraph *g);        /* 三角形等权图：演示"并列时 Prim 选谁" */
void mgraph_build_disconnected(MGraph *g);/* 不连通图：演示"根本没有生成树"这个边界 */

/* ---------- Prim 主函数 ----------
 * start        : 从哪个点开始砌墙（MST 与起点无关，随便选都是同一个总权）
 * out / cnt_out: 选中的 V-1 条边
 * show_steps   : 1 = 逐轮打印候选跨墙边清单（本节的核心画面）
 * 返回值        : 生成树总权；图不连通时返回 -1（这时 out 里是"最小生成森林"的边）
 */
int prim_mst(const MGraph *g, int start, MSTEdge out[], int *cnt_out, int show_steps);

/* ---------- 工具 ---------- */
void mst_print_edges(const MSTEdge edges[], int cnt, const char *title);

/* 自检：真的是生成树吗？检查 (1) 边数 == V-1 (2) 覆盖所有点 (3) 不成环。
 * 返回 1 = 是，0 = 不是（并打印原因）。写完算法一定要跑这种检查，教学项目尤其。 */
int mst_check_valid(const MGraph *g, const MSTEdge edges[], int cnt);

/* ---------- 最短路径树（只为对照而生的极简 Dijkstra）----------
 * 本目录不许 include 隔壁 07-graph/dijkstra 的文件，所以这里自带一份。
 * 它返回 parent 数组，我们把"parent 链构成的树"的边权和算出来，
 * 和 MST 的总权摆在一起，一眼看出【两者是不同的东西】。
 */
void spt_by_dijkstra(const MGraph *g, int src, int dist[], int parent[]);

#endif /* PRIM_H */
