#ifndef BELLMAN_H
#define BELLMAN_H

/* =====================================================================
 * 这一节：Bellman-Ford 单源最短路（允许负权边，还能判负环）
 * =====================================================================
 *
 * 它和 Dijkstra 的关系，一句话概括：
 *   Dijkstra 每轮【挑一个点定稿】，只信「非负权」这个前提；
 *   Bellman-Ford 不挑点、不定稿，每轮把【所有边】全都试一遍松弛，
 *   一共试 V-1 轮。因为它从不宣布「这个答案永远不会变了」，
 *   所以负权边伤不到它。
 *
 * 为什么是 V-1 轮？
 *   一条不含环的最短路径，最多经过 V-1 条边（再多就重复经过某个点，也就是绕了环）。
 *   第 1 轮把所有「1 条边」的路确定下来，第 2 轮把「2 条边」的确定下来……
 *   第 k 轮至少把「k 条边」的正确距离往前推一格。推 V-1 轮，全部推完。
 *
 * 第 V 轮还能松弛 ⇒ 说明存在一条「用了 V 条边还能更短」的路，
 *   这只可能是绕了一个总权为负的环 ⇒ 【负环】，最短路没有意义（可以无限小）。
 *
 * ⚠ 本项目硬约定：只用 int，不用 void*、不用函数指针。
 * ⚠ 图是硬编码固定示例，README 里的输出可逐格核对。
 */

#define MAX_V     8
#define MAX_E     48           /* 弧的条数上限：共用图 9 条无向边 = 18 条弧 */
#define INF_DIST  1000000000   /* "还到不了"的哨兵值，理由同 dijkstra 那节 */

typedef struct {
    int from;
    int to;
    int weight;    /* 这一节的 weight 【可以为负】，这正是本节存在的意义 */
} Edge;

typedef struct {
    int n;                  /* 顶点数，编号 0 .. n-1 */
    int directed;           /* 0 = 无向图（加一条边自动存两条弧）；1 = 有向图 */
    int m;                  /* 当前弧数 */
    Edge e[MAX_E];          /* 边表。Bellman-Ford 只要边表，不要邻接矩阵 ——
                             * 因为它每轮的动作就是"逐条边过一遍"，边表天然契合 */
} Graph;

/* ---------- 建图 ---------- */
void graph_init(Graph *g, int n, int directed);
void graph_add_edge(Graph *g, int u, int v, int weight);
void graph_print_edges(const Graph *g);

/* 三张演示图，都在这个文件里硬编码 */
void graph_build_neg_no_cycle(Graph *g);  /* ① 带负权但没有负环 */
void graph_build_neg_cycle(Graph *g);     /* ② 带负环，专门用来触发检测 */
void graph_build_shared(Graph *g);        /* ③ 全仓库共用的那张 6 点图（无负权） */

/* ---------- 主算法 ----------
 * 返回：0 = 正常算完；1 = 检测到负环（此时 dist 里的值不可信）
 *
 * show_steps        = 1 时每轮打印 dist 快照，"逐轮逼近"全过程可见
 * rounds_used_out   = 实际跑了几个有效轮（可能提前收敛）
 * bad_from_out/bad_to_out = 第 V 轮仍能松弛的那条边（负环的证据），NULL 表示不关心
 */
int bellman_ford(const Graph *g, int src, int dist[], int parent[], int show_steps,
                 int *rounds_used_out, int *bad_from_out, int *bad_to_out);

/* ---------- 只为"和 Dijkstra 对照"而重写的极简朴素 Dijkstra ----------
 * 每个叶子目录必须自包含，不许 include 隔壁 dijkstra 目录的文件，
 * 所以这里抄一份 20 行的最小版本。它要求边权非负，非负时才可以用它当参照。
 */
void mini_dijkstra(const Graph *g, int src, int dist[]);

/* ---------- 打印小工具 ---------- */
void print_dist_row(const char *label, const int dist[], int n);
void print_parent_row(const char *label, const int parent[], int n);
void print_path(const int parent[], int src, int dst, const int dist[]);

#endif /* BELLMAN_H */
