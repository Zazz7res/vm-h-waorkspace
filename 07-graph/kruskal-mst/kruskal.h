#ifndef KRUSKAL_H
#define KRUSKAL_H

/* =====================================================================
 * 这一节：Kruskal 最小生成树 + 本目录自带的并查集
 * =====================================================================
 *
 * Kruskal 的思路朴素得离谱：
 *   1) 把所有边按权重从小到大排一遍
 *   2) 从最便宜的开始一条条试：「这条边收下会不会成环？」
 *        不会 ⇒ 收下
 *        会   ⇒ 扔掉，看下一条
 *   3) 收满 V-1 条就完工
 *
 * 和 Prim 的分水岭就在这里：
 *   Prim 每轮加【一个点】（新点必然不在树里 ⇒ 天生不成环，不用判环）
 *   Kruskal 每轮加【一条边】（两个端点可能已经在同一棵树里 ⇒ 必须判环）
 *   「判断两点是否已在同一块里」这件事，用【并查集】做正好 —— 这就是本节要花大篇幅
 *   讲并查集的原因：并查集就是 Kruskal 的那颗心脏。
 *
 * 为什么它是对的？（和 Prim 的割性质是一对镜像，用的是「环性质」）
 *   设当前森林 F，加入边 e 会成环，环里 e 是最长的那条 ⇒ e 一定不属于任何 MST。
 *   因为一棵 MST 若想连通环上这些点，只要留环上其余边就够了，删掉最长的 e 反而更省。
 *   所以「跳过成环的贵边」是安全的；反过来「收下的边」都是此刻跨两棵子树最便宜的桥。∎
 *
 * ⚠ 本项目硬约定：只用 int，不用 void*、不用函数指针。
 *   所以：排序不能用 qsort（它要你递一个函数指针当比较器），本目录手写插入排序。
 *   并查集也不去 include 别的目录，直接在 kruskal.h / kruskal.c 里自带一份。
 */

#define MAX_V   8
#define MAX_E   16              /* 无向边「条数」上限（本节的图都很小，够用） */
#define INF_W   1000000000      /* Prim 对照实现里用，表示两点间没有边 */

/* 一条待处理的边。u、v 谁大谁小无所谓，排序会打乱顺序 */
typedef struct {
    int u;
    int v;
    int w;
} WEdge;

/* 一条被选进 MST 的边（单独一个类型，是为了打印时和「候选边」区分开） */
typedef struct {
    int u;
    int v;
    int w;
} MSTEdge;

/* Kruskal 只要边表！不需要邻接矩阵 —— 这是它适合稀疏图的根本原因 */
typedef struct {
    int n;                      /* 顶点数，编号 0..n-1 */
    int m;                      /* 边数 */
    WEdge e[MAX_E];
} Graph;

/* ---------- 图 ---------- */
void graph_init(Graph *g, int n);
void graph_add_edge(Graph *g, int u, int v, int w);
void graph_print_edges(const Graph *g, const char *title);

void graph_build_shared(Graph *g);        /* 全仓库共用的 6 点图（MST 总权 = 33） */
void graph_build_ties(Graph *g);          /* 三角形等权图：看 Kruskal 和 Prim 选了不同的边 */
void graph_build_disconnected(Graph *g);  /* 不连通图：得到「最小生成森林」 */
void graph_build_big(Graph *g, int tie_order);   /* 8 点图；tie_order 改变同权环边的边表顺序 */

/* ---------- 手写插入排序（按 w 升序，稳定） ----------
 * 稳定很重要：权重相同的边，谁先被试取决于原表顺序。
 * 这也解释了为什么「同一张图，Kruskal 换一种边表顺序可能给另一棵 MST」。 */
void insertion_sort_edges(WEdge a[], int n);
void sort_edges_copy(const Graph *g, WEdge out[]);   /* 排序不改原图，先拷贝一份 */

/* ---------- 并查集（本目录自带，带路径压缩 + 按秩合并） ----------
 * fa[x] ：x 的父亲；fa[x] == x 表示 x 是这一棵树的根（也就是「集合的代表」）
 * rnk[x]：以 x 为根的那棵树的【高度上界】，只在根位置上有意义
 */
typedef struct {
    int fa[MAX_V];
    int rnk[MAX_V];
    int n;
} DSU;

void dsu_init(DSU *d, int n);
int  dsu_find(DSU *d, int x);                       /* 查代表元，顺手把路径压平 */
int  dsu_union(DSU *d, int a, int b);               /* 合并；返回 1 = 真合并了，0 = 本来就是一家人 */
int  dsu_same(DSU *d, int a, int b);                /* 是否同一集合 */
int  dsu_find_probe(const DSU *d, int x, int *hops);/* 只数「爬了几跳」，不改结构（演示用） */
void dsu_print(const DSU *d, const char *title);

/* ---------- Kruskal 主函数 ----------
 * out / cnt_out : 选中的边
 * show_steps    : 1 = 每看一条边就打印「find 结果 + 收下/跳过」
 * 返回值        : 选中的边的总权。注意：图不连通时它照样返回一个数，
 *                 但那只是「最小生成森林」的总权 —— 要靠 cnt_out 是否 == n-1 来区分！
 */
int kruskal_mst(const Graph *g, MSTEdge out[], int *cnt_out, int show_steps);

/* ---------- 工具与对照 ---------- */
void mst_print_edges(const MSTEdge edges[], int cnt, const char *title);

/* 把两份结果摆一起对账：总权是否相同、边集是否相同，各配一句人话解释 */
void mst_compare(const MSTEdge a[], int na, const MSTEdge b[], int nb,
                 const char *la, const char *lb);

int  mst_check_valid(const Graph *g, const MSTEdge edges[], int cnt);   /* 1 = 是合法生成树 */

/* Prim 对照实现（本目录自带一份极简版，不去 include 隔壁 prim-mst）：
 * 返回 MST 总权，out/cnt 给出边集。用来验证「两种算法总权相同」。 */
int mini_prim(const Graph *g, int start, MSTEdge out[], int *cnt_out);

#endif /* KRUSKAL_H */
