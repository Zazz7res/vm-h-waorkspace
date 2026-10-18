#ifndef FLOYD_H
#define FLOYD_H

/* =====================================================================
 * 这一节：Floyd / Floyd-Warshall 多源最短路（三重循环）
 * =====================================================================
 *
 * 和前两节的根本区别：
 *   Dijkstra / Bellman-Ford 是【单源】：固定一个起点，算它到所有点。
 *   Floyd 是【多源 / 所有点对】：一次算出任意两点 i -> j 的最短距离。
 *   想要"全图任意两点"的答案，跑 V 次 Dijkstra 是 O(V^2 E logV) 级别，
 *   而 Floyd 只要 O(V^3)，并且代码只有 5 行。点少的时候它是绝对的首选。
 *
 * 唯一的硬核要求：**循环顺序必须是 k 在最外层**。
 *   k 的含义不是"第 k 步"，而是【只允许经过编号 <= k 的点当中转站】。
 *   这样 dist[i][j] 在第 k 轮结束时表示：
 *       "从 i 到 j，只准用 {0,1,...,k} 这些点当中转，能走出的最短路"
 *   k 从 0 推到 V-1，中转站权限逐步放宽，最后就是真正的最短路。
 *   —— 把 k 放最内层会用到"还没算出来"的中间值，结果会错，本节现场跑给你看。
 *
 * 能处理负权吗？能（没有负环就行）。有负环时对角线会出现 dist[i][i] < 0，顺手就能判。
 *
 * ⚠ 本项目硬约定：只用 int，不用 void*、不用函数指针。
 */

#define MAX_V     8
#define INF_DIST  1000000000     /* ∞ 的整数替身。为什么不用 INT_MAX：加法会溢出成负数 */

/* 二维表的类型别名。
 * 为什么要有这个 typedef？
 *   C 语言里传二维数组必须写出第二维的大小：int m[][MAX_V] 才合法，
 *   写成 int **m 是【错的】（那是"指针数组"，内存排布完全不同）。
 *   有了别名，函数签名一眼看得懂："这是一张 MAX_V × MAX_V 的 int 表"。 */
typedef int Table[MAX_V][MAX_V];

/* ---------- 用邻接矩阵存的一张图 ---------- */
typedef struct {
    int n;                         /* 顶点数，编号 0 .. n-1 */
    int w[MAX_V][MAX_V];           /* w[u][v] = 直接边权；INF_DIST = 没有这条直接边 */
} Matrix;

void mat_init(Matrix *g, int n);                          /* 建空图，矩阵填 ∞，对角线 0 */
void mat_add_arc(Matrix *g, int u, int v, int weight);    /* 加一条有向边 */
void mat_add_edge(Matrix *g, int u, int v, int weight);   /* 加一条无向边（对称写两次） */
void mat_print(const Matrix *g, const char *title);       /* 打印这张图的邻接矩阵 */

/* 四张演示图，全部硬编码 */
void mat_build_demo(Matrix *g);         /* ① 本节主角：4 顶点带权有向图（用于逐 k 轮快照 + 错序对照）*/
void mat_build_negative(Matrix *g);     /* ② 4 顶点，含负权边、无负环 */
void mat_build_neg_cycle(Matrix *g);    /* ③ 3 顶点，含负环 */
void mat_build_dag(Matrix *g);          /* ④ 4 顶点 DAG，专门用来演示传递闭包 */
void mat_build_shared(Matrix *g);       /* ⑤ 全仓库共用的那张 6 点图（和无权 Dijkstra 对照） */

/* ---------- 主算法 ---------- */
/* 正确版：k 在最外层。
 * dist_out / next_out 由调用者提供数组（本 demo 用栈上的 Table，够用且不涉及 malloc）
 * show_snapshots = 1 时每轮 k 结束打印整张矩阵 + 本轮改了哪些格子
 * 返回：0 = 正常；1 = 有负环（此时 dist 不可信）
 */
int floyd_all_pairs(const Matrix *g, Table dist_out, Table next_out, int show_snapshots);

/* 故意写错的版本：i, j 在外，k 在最内层。结果矩阵【会错】，就错在个别格子上。
 * 只有教学价值，不要在项目里调用它。 */
void floyd_wrong_loop_order(const Matrix *g, Table dist_out);

/* 判负环：跑完正确版后看对角线，dist[i][i] < 0 就说明 i 能绕一个负环回到自己 */
int floyd_negative_cycle_vertex(const Table dist, int n);   /* 返回一个满足条件的顶点，没有就 -1 */

/* ---------- 路径还原 ----------
 * next[i][j] = 从 i 到 j 的最短路上，i 之后的第一个点；不可达时为 -1。
 * 有了它就能打印 0 -> 2 -> 5 这种完整路径（只有 dist 矩阵是打不出路径的）。
 * 返回路径长度（点数），写进 len_out；不可达返回 -1。
 */
int floyd_reconstruct(const Table next, int src, int dst, int out[], int max_len);

/* ---------- 传递闭包（同一套三重循环，换个运算符）----------
 * reach[i][j] = 1 表示"从 i 出发能走到 j"，不关心距离。
 * 把 min 换成 或(|)、把 + 换成 与(&) —— 这就是 Floyd 求可达性。
 * 这里是原地修改。
 */
void floyd_transitive_closure(int reach[][MAX_V], int n);

/* ---------- 打印小工具 ---------- */
void table_print(const char *title, const Table t, int n, const char *cell_kind);
void table_diff_note(const Table before, const Table after, int n);   /* 列出变小的格子 */

#endif /* FLOYD_H */
