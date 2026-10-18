#ifndef DIJKSTRA_H
#define DIJKSTRA_H

/* =====================================================================
 * 这一节：Dijkstra 单源最短路（要求边权非负）
 * =====================================================================
 *
 * "单源"是什么意思？
 *   固定一个起点 src，一次算出【从 src 到图上每一个点】的最短距离。
 *   不是"任意两点"（那是 Floyd），也不是"只算一个终点"（虽然可以提前停）。
 *
 * 这个头文件里放了三样东西：
 *   1) Graph —— 本目录自包含的极小图结构（邻接矩阵 + 边表各一份，谁好用谁）
 *   2) 两个 Dijkstra —— 朴素 O(V^2) 版 和 小顶堆优化 O((V+E)logV) 版
 *   3) MinHeap —— 自己写的极简 int 小顶堆（只依赖本目录，不 include 别的章节）
 *
 * ⚠ 本项目硬约定：元素类型全用 int，不用 void* 泛型，不用函数指针比较器。
 *   所以堆的比较规则（谁小）是直接写在代码里的，不可替换。
 *
 * ⚠ 这一节的图是"硬编码固定示例"，不是随机数据 —— README 里的输出可以逐格核对。
 */

#define MAX_V     8            /* 本 demo 最多几个顶点（共用示例图是 6 个） */
#define MAX_E     48           /* 最多几条"弧"。无向边会被拆成 2 条弧：9 条边 = 18 条弧 */
#define INF_DIST  1000000000   /* 用一个大整数代表"无穷大 / 还到不了"。
                                * 为什么是 1e9 而不是 INT_MAX？
                                *   因为松弛时要算 dist[u] + w，如果 dist[u] 就是 INT_MAX，
                                *   加一下就溢成负数，比较结果全乱套。1e9 加上任何小权值都不会溢出。 */

/* ---------------------------------------------------------------------
 * 一条边（准确说是一条"弧"：有方向的 u -> v）
 * --------------------------------------------------------------------- */
typedef struct {
    int from;      /* 起点编号 */
    int to;        /* 终点编号 */
    int weight;    /* 边权。本项目允许它是负数（用来演示 Dijkstra 会算错的反例），
                    * 但真实使用时必须非负，否则请改用 Bellman-Ford */
} Edge;

/* ---------------------------------------------------------------------
 * 图。同一份数据存两种形式，是为了让两种 Dijkstra 各用擅长的表示法：
 *   - 邻接矩阵 w[u][v] ：朴素版"扫一遍所有 v 找最小"时最方便，O(1) 判断两点有没有边
 *   - 边表 e[]         ：堆优化版"只关心 u 的出边"时最方便，不用扫全图
 * 小图无所谓浪费；大图只会用邻接表（见 07-graph/adjacency-list）。
 * --------------------------------------------------------------------- */
typedef struct {
    int n;                  /* 顶点个数，顶点编号固定为 0 .. n-1 */
    int directed;           /* 1 = 有向图；0 = 无向图（加一条边会自动存两条弧） */
    int m;                  /* 当前弧条数 */
    Edge e[MAX_E];          /* 边表 */
    int w[MAX_V][MAX_V];    /* 邻接矩阵：w[u][v] = 边权；INF_DIST 表示"没有这条边" */
} Graph;

/* ---------- 图的基本操作 ---------- */
void graph_init(Graph *g, int n, int directed);        /* 建一张空图（矩阵全填 INF） */
void graph_add_edge(Graph *g, int u, int v, int weight); /* 加一条边/弧 */
void graph_build_shared(Graph *g);                     /* 灌入本仓库 5 节共用的那张示例图 */
void graph_print_edges(const Graph *g);                /* 把边表打印出来，给人核对 */

/* ---------- 两种 Dijkstra ----------
 * 返回值：0 = 正常跑完；1 = 图里有负权边（结果不可信，只是给你看它错成什么样）
 * show_steps = 1 时逐轮/逐弹出打印，这是本节最重要的教学画面
 * stale_pops_out：堆版专用，带出"弹出了多少条过期记录"，NULL 表示不关心
 */
int dijkstra_naive(const Graph *g, int src, int dist[], int parent[], int show_steps);
int dijkstra_heap(const Graph *g, int src, int dist[], int parent[], int show_steps,
                  int *stale_pops_out);

/* ---------- 暴力求最短路（只枚举简单路径，仅用于 tiny 反例的"标准答案"）----------
 * 它能处理负权，所以用它给 Dijkstra 的错误答案当对照。
 * 代价是指数量级，顶点一多就炸，只在本节的 4 点小图上用。
 */
void shortest_by_brute_force(const Graph *g, int src, int dist[]);

/* ---------- 打印小工具 ---------- */
void print_dist_row(const char *label, const int dist[], int n);    /* dist 数组一行，INF 打印成 ∞ */
void print_parent_row(const char *label, const int parent[], int n);/* parent 数组一行，-1 打印成 - */
void print_shortest_path(const int parent[], int src, int dst, const int dist[]);
                                                                   /* 回溯打印 0 -> 2 -> 5  (11) */

/* =====================================================================
 * 极简小顶堆（MinHeap）
 * =====================================================================
 * 只装 (顶点编号, 距离) 这一对 int，比较规则固定："距离小的排前面"。
 * 不用函数指针，不通用，够用，一眼能看懂。
 *
 * 数组下标和树的关系（这就是"用数组表示完全二叉树"）：
 *            0
 *          /   \
 *         1     2
 *        / \   / \
 *       3   4 5   6
 *   节点 i 的两个孩子 = 2i+1, 2i+2 ；父亲 = (i-1)/2
 * ===================================================================== */
typedef struct {
    int vertex;   /* 堆里存的顶点编号 */
    int dist;     /* 入堆那一刻该顶点的距离快照 —— 注意它是"历史照片"，不是实时值 */
} HeapItem;

typedef struct {
    HeapItem *item;  /* 堆数组（malloc 来的，必须 free） */
    int size;        /* 当前有几个元素 */
    int cap;         /* 最多能放几个 */
} MinHeap;

MinHeap *heap_create(int cap);                 /* 调用者负责 heap_destroy */
void     heap_destroy(MinHeap *h);
int      heap_push(MinHeap *h, int vertex, int dist);  /* 入堆：上浮。返回 0 成功 -1 满了 */
int      heap_pop(MinHeap *h, HeapItem *out);          /* 弹出最小：下沉。空则返回 -1 */
void     heap_print(const MinHeap *h);                  /* 把堆数组画成一行，看堆长什么样 */

#endif /* DIJKSTRA_H */
