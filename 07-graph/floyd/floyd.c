#include <stdio.h>
#include "floyd.h"

/* 中文注释里出现的引号一律用「」；ASCII 双引号放进字符串字面量会把字符串截断。 */

/* =====================================================================
 * 第 0 部分：邻接矩阵存图
 * =====================================================================
 *
 * Floyd 天生就该用邻接矩阵：它的状态就是「一张 dist[i][j] 的表」，
 * 而初值恰好是「直接边权表」。用邻接表反而要先 O(V^2) 铺一张矩阵出来。
 *
 * 内存布局（MAX_V = 8，C 语言按行连续存放）：
 *
 *   结构体里 int w[8][8] 占 64 × 4 = 256 字节，是一块连着的内存：
 *
 *   w[0][0] w[0][1] ... w[0][7] | w[1][0] ... w[1][7] | ... | w[7][0] ... w[7][7]
 *   └──────── 第 0 行 ────────┘   └───── 第 1 行 ────┘
 *
 *   所以 &w[i][j] == (int*)&w + i*8 + j —— 二维下标其实就是"行偏移 × 行宽 + 列"。
 *   这也解释了为什么传参时必须写第二维：编译器要知道「一行多宽」才能跳行。
 */
void mat_init(Matrix *g, int n)
{
    if (n > MAX_V) {
        printf("[错误] 顶点数 %d 超过上限 %d\n", n, MAX_V);
        return;
    }
    g->n = n;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            g->w[i][j] = (i == j) ? 0 : INF_DIST;
        }
    }
}

void mat_add_arc(Matrix *g, int u, int v, int weight)
{
    if (u < 0 || u >= g->n || v < 0 || v >= g->n) {
        printf("[错误] 弧 %d->%d 越界（n=%d）\n", u, v, g->n);
        return;
    }
    /* 重边取较小者：Floyd 的初值必须是「两点之间所有直接边里最短的那条」，
     * 直接覆盖就会把短边丢掉，这是建表阶段最容易犯的错。 */
    if (weight < g->w[u][v]) {
        g->w[u][v] = weight;
    }
}

void mat_add_edge(Matrix *g, int u, int v, int weight)
{
    mat_add_arc(g, u, v, weight);
    mat_add_arc(g, v, u, weight);    /* 无向 = 两条方向相反的有向弧 */
}

void mat_print(const Matrix *g, const char *title)
{
    printf("  %s 的邻接矩阵（∞ 表示没有直接边）：\n", title);
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
            if (g->w[i][j] >= INF_DIST) {
                printf("   %s", "∞");
            } else {
                printf("%4d", g->w[i][j]);
            }
        }
        printf("\n");
    }
}

/* ---------------------------------------------------------------------
 * 演示图 ①：本节主角。4 个顶点的有向带权图。
 *
 *   边（权值）： 0->1 = 3    0->2 = 8    1->2 = 2
 *               1->3 = 9    2->3 = 1    3->0 = 4
 *
 *        示意：一个顺环 0 -> 1 -> 2 -> 3 -> 0，再斜跨两条 0 -> 2、1 -> 3
 *
 *                     3              2
 *          (0) ──────────> (1) ──────────> (2)
 *            ^   ╲           ╲              │
 *          4 │     ╲ 8         ╲ 9          │ 1
 *            │       ╲           ╲          v
 *            + <──────╲──────── (3) <───────┘
 *                     （斜线只是表示"跨过中间点直连"，别按几何长度理解权值）
 *
 *   这张图保证任意两点都可达（有 0->1->2->3->0 这个大环兜着），所以矩阵里没有 ∞。
 *
 * 为什么选这张图：
 *   1) 它有一条「绕远反而更近」的路：0->2 直连是 8，但 0->1->2 只要 3+2 = 5；
 *   2) 它有一条「必须绕两个中转」的路：1->0 = 7，走的是 1->2->3->0 = 2+1+4，
 *      这条路正是【k 放最内层会算错】的那条 —— 第 3 幕会现场演示它错成 13。
 * --------------------------------------------------------------------- */
void mat_build_demo(Matrix *g)
{
    mat_init(g, 4);
    mat_add_arc(g, 0, 1, 3);
    mat_add_arc(g, 0, 2, 8);
    mat_add_arc(g, 1, 2, 2);
    mat_add_arc(g, 1, 3, 9);
    mat_add_arc(g, 2, 3, 1);
    mat_add_arc(g, 3, 0, 4);
}

/* ---------------------------------------------------------------------
 * 演示图 ②：含负权、无负环（Floyd 能吃负权，这点比 Dijkstra 强）
 *
 *   0->1 = 2    0->2 = 5    1->2 = -4    1->3 = 10    2->3 = 3
 *
 *   这是一张 DAG（有向无环图），所以不可能有负环。
 *   预期第 0 行：dist[0][0..3] = [0, 2, -2, 1]
 *     dist[0][2] = 0->1->2 = 2 + (-4) = -2   ← 比直连的 5 小，而且是负数
 *     dist[0][3] = 0->1->2->3 = 2 - 4 + 3 = 1
 * --------------------------------------------------------------------- */
void mat_build_negative(Matrix *g)
{
    mat_init(g, 4);
    mat_add_arc(g, 0, 1, 2);
    mat_add_arc(g, 0, 2, 5);
    mat_add_arc(g, 1, 2, -4);
    mat_add_arc(g, 1, 3, 10);
    mat_add_arc(g, 2, 3, 3);
}

/* ---------------------------------------------------------------------
 * 演示图 ③：含负环 0->1->2->0，总权 1 + (-1) + (-1) = -1
 *
 *   Floyd 结束后 dist[i][i] 会变成负数（"从 i 出发绕一圈回到 i 反而更短"），
 *   这就是判负环最省事的办法：只看对角线。
 * --------------------------------------------------------------------- */
void mat_build_neg_cycle(Matrix *g)
{
    mat_init(g, 3);
    mat_add_arc(g, 0, 1, 1);
    mat_add_arc(g, 1, 2, -1);
    mat_add_arc(g, 2, 0, -1);
}

/* ---------------------------------------------------------------------
 * 演示图 ④：4 顶点 DAG，用来演示传递闭包（可达性）
 *   0->1    1->2    2->3    0->3 之外不加边：
 *   0 -> 1 -> 2 -> 3   是一条链
 *   预期可达矩阵（行 i 能到哪些列 j）：
 *     0: 1,2,3    1: 2,3    2: 3    3: 谁都到不了
 * --------------------------------------------------------------------- */
void mat_build_dag(Matrix *g)
{
    mat_init(g, 4);
    mat_add_arc(g, 0, 1, 1);
    mat_add_arc(g, 1, 2, 1);
    mat_add_arc(g, 2, 3, 1);
}

/* ---------------------------------------------------------------------
 * 演示图 ⑤：全仓库共用的那张 6 点无向带权图。
 *   主链： (0) ──7── (1) ──10── (2) ──11── (3) ──6── (4) ──9── (5)
 *   跨点： (0) ──9── (2)   (0) ──14── (5)   (1) ──15── (3)   (2) ──2── (5)
 * Floyd 算完，第 0 行必须等于 Dijkstra 的 dist = [0, 7, 9, 20, 20, 11]。
 * --------------------------------------------------------------------- */
void mat_build_shared(Matrix *g)
{
    mat_init(g, 6);
    mat_add_edge(g, 0, 1, 7);
    mat_add_edge(g, 0, 2, 9);
    mat_add_edge(g, 0, 5, 14);
    mat_add_edge(g, 1, 2, 10);
    mat_add_edge(g, 1, 3, 15);
    mat_add_edge(g, 2, 3, 11);
    mat_add_edge(g, 2, 5, 2);
    mat_add_edge(g, 3, 4, 6);
    mat_add_edge(g, 4, 5, 9);
}

/* =====================================================================
 * 打印工具
 * ===================================================================== */

/*
 * 打一张 n × n 的表。cell_kind：
 *   "dist"  → 大于等于 INF_DIST 的打 ∞（否则那串 1000000000 会把表格撑爆）
 *   "reach" → 只打 0/1
 *   "next"  → -1 打 "-"（表示"没有路"）
 * 手工补空格而不用 %4s，是因为 ∞ 在 UTF-8 里占 3 字节而显示只占 1 列，
 * printf 的宽度参数按字节算，用 %4s 会少补 2 个空格、表格错位。
 */
void table_print(const char *title, const Table t, int n, const char *cell_kind)
{
    printf("%s\n", title);        /* 缩进由调用方写在 title 里，这里不再另加，免得对不齐 */
    printf("        j ");
    for (int j = 0; j < n; j++) {
        printf("%4d", j);
    }
    printf("\n      ------");
    for (int j = 0; j < n; j++) {
        printf("----");
    }
    printf("\n");
    for (int i = 0; i < n; i++) {
        printf("    i %d |", i);
        for (int j = 0; j < n; j++) {
            int v = t[i][j];
            if (cell_kind[0] == 'd' && v >= INF_DIST) {
                printf("   %s", "∞");
            } else if (cell_kind[0] == 'n' && v < 0) {
                printf("   -");
            } else {
                printf("%4d", v);
            }
        }
        printf("\n");
    }
}

/* 比较两张表，把「变小的格子」列出来：这就是每轮 k 到底改了什么 */
void table_diff_note(const Table before, const Table after, int n)
{
    int cnt = 0;
    printf("      本轮变小的格子：");
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (after[i][j] < before[i][j]) {
                if (before[i][j] >= INF_DIST) {
                    printf(" [%d][%d] ∞ -> %d", i, j, after[i][j]);
                } else {
                    printf(" [%d][%d] %d -> %d", i, j, before[i][j], after[i][j]);
                }
                cnt++;
            }
        }
    }
    if (cnt == 0) {
        printf("（一个都没有，本轮白跑）");
    } else {
        printf("   → 共 %d 格", cnt);
    }
    printf("\n");
}

/* =====================================================================
 * 第 1 部分：Floyd 主体（正确版，k 在最外层）
 * =====================================================================
 *
 * 全部代码就这 5 行：
 *
 *   for (k = 0; k < n; k++)
 *       for (i = 0; i < n; i++)
 *           for (j = 0; j < n; j++)
 *               if (dist[i][k] != ∞ && dist[k][j] != ∞ && dist[i][k] + dist[k][j] < dist[i][j])
 *                   dist[i][j] = dist[i][k] + dist[k][j];
 *
 * ★ 最要紧的一件事：k 到底是什么意思？
 *   第 k 轮结束时，dist[i][j] = 「从 i 到 j，只允许把 {0, 1, ..., k} 当【中转站】，
 *   能走出来的最短路长度」。注意 i 和 j 本身不在"中转"的计数里，它们是端点。
 *
 *   所以 k 从 0 推到 n-1，就是【中转站许可证一张张发出去】：
 *     k = 0：只能借道 0
 *     k = 1：能借道 {0,1}
 *     ...
 *     k = n-1：随便借道 ⇒ 真·全图最短路
 *
 *   为什么这个递推成立？看第 k 轮的两条分支：
 *     (a) 最短路径不经过 k ⇒ 它在第 k-1 轮就已经算出来了，这轮不变；
 *     (b) 最短路径经过 k ⇒ 把它在 k 处劈成 i ~> k 和 k ~> j 两段，
 *         这两段都只用 {0..k-1} 当中转（因为最短路不会重复经过同一个点，
 *         否则可以把绕 k 的那一圈删掉更短），所以两段在第 k-1 轮就已经是正确值了。
 *     于是 min(旧值, 新值两段相加) 就得到了第 k 轮的正确值。
 *
 * ★ 为什么 k 必须在外层？
 *   因为第 k 轮用到的 dist[i][k] 和 dist[k][j] 必须是「许可证 = {0..k-1}」那一轮的值。
 *   把 k 放最内层，算 dist[i][j] 时 dist[i][k] 可能还没被更新（同一行里 j 还没扫到），
 *   于是用一个「过时的、更 optimistic 都没有的」大值参与比较 —— 结果就是漏掉最短路。
 *   第 3 幕现场跑一个错版给你看，它把 1->0 = 7 算成 13。
 *
 * 顺便：为什么 dist[i][k] + dist[k][j] 要判 ∞？
 *   ∞ 在这里是 1e9，加一下就 2e9 —— 超过 int 上限 2147483647 一点点没事，
 *   但如果 ∞ 取的是 INT_MAX，加法必然溢出成负数，然后"负数 < 任何值"，
 *   整张表被垃圾值污染。所以要么判 ∞ 跳过，要么 ∞ 取小一点。这里两件事都做了。
 */
int floyd_all_pairs(const Matrix *g, Table dist_out, Table next_out, int show_snapshots)
{
    const int n = g->n;

    /* 初始化：dist = 直接边权表；next[i][j] = 有直接边时先记 j */
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            dist_out[i][j] = g->w[i][j];
            next_out[i][j] = (i != j && g->w[i][j] < INF_DIST) ? j : -1;
        }
    }

    if (show_snapshots) {
        table_print("  k = -1（还没开始，dist = 直接边权表；∞ = 没有直接边）：", dist_out, n, "dist");
        printf("      读法：dist[i][j] 是「只准走一条直接边」的距离，绕路的一律还是 ∞。\n");
    }

    for (int k = 0; k < n; k++) {
        Table before;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                before[i][j] = dist_out[i][j];
            }
        }

        for (int i = 0; i < n; i++) {
            /* 这一句可以省，但省了会白算 n 次：dist[i][k] 是 ∞ 时 i->k 都到不了，
             * 后面无论 j 是什么都不可能变短。大图上这个判断能省掉大量内层循环。 */
            if (dist_out[i][k] >= INF_DIST) {
                continue;
            }
            for (int j = 0; j < n; j++) {
                if (dist_out[k][j] >= INF_DIST) {
                    continue;
                }
                int via = dist_out[i][k] + dist_out[k][j];   /* 绕 k 这一趟的总长 */
                if (via < dist_out[i][j]) {
                    dist_out[i][j] = via;
                    /* 路径还原表的唯一一行改动：从 i 去 j，先按「从 i 去 k」的第一步走 */
                    next_out[i][j] = next_out[i][k];
                }
            }
        }

        if (show_snapshots) {
            char title[192];   /* 给足：中文一个字就占 3 个字节，缓冲区小了会被 snprintf 截断 */
            snprintf(title, sizeof title,
                     "  k = %d 结束后（此刻只准借道编号 <= %d 的点当中转站）：", k, k);
            table_print(title, dist_out, n, "dist");
            table_diff_note(before, dist_out, n);
        }
    }

    /* 对角线判负环：dist[i][i] < 0 表示"从 i 出发绕一圈回 i 竟然更短" ⇒ 环上必有负权 */
    for (int i = 0; i < n; i++) {
        if (dist_out[i][i] < 0) {
            if (show_snapshots) {
                printf("      ⚠ dist[%d][%d] = %d < 0 ⇒ 顶点 %d 能绕过一个【负环】回到自己，\n",
                       i, i, dist_out[i][i], i);
                printf("        这张图的「最短距离」没有意义（再绕一圈它还能更小）。\n");
            }
            return 1;
        }
    }
    return 0;
}

/* =====================================================================
 * 第 2 部分：【故意写错】的循环顺序 —— 只为教学
 * =====================================================================
 *
 * 把 k 放到最内层：
 *
 *   for (i)  for (j)  for (k)  dist[i][j] = min(dist[i][j], dist[i][k] + dist[k][j])
 *
 * 看起来"反正三重循环都跑了"，其实不是：
 *   算 dist[i][j] 时要拿 dist[i][k] 当零件，可这个 k 行的 dist[i][k]
 *   可能对应 j' = k 的那一格，而 j 是【从小到大】扫的，j' = k 还没轮到 ⇒ 零件是旧的。
 *   零件旧 = 值偏大 = 该更新的没更新 ⇒ 最终答案偏大，甚至把"可达"报成"不可达"。
 *
 * 正确的三个可接受顺序（k 必须在 i、j 之前）：k-i-j、k-j-i。
 * 错误的：i-j-k、i-k-j（k 不在最外层）、j-i-k ……
 * 本 demo 现场跑 i-j-k，它在演示图①上把 dist[1][0] 从 7 算成 13。
 */
void floyd_wrong_loop_order(const Matrix *g, Table dist_out)
{
    const int n = g->n;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            dist_out[i][j] = g->w[i][j];
        }
    }
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            for (int k = 0; k < n; k++) {          /* ← k 在最内层，错就错在这 */
                if (dist_out[i][k] >= INF_DIST || dist_out[k][j] >= INF_DIST) {
                    continue;
                }
                int via = dist_out[i][k] + dist_out[k][j];
                if (via < dist_out[i][j]) {
                    dist_out[i][j] = via;
                }
            }
        }
    }
}

/* 找第一个 dist[i][i] < 0 的顶点；没有就返回 -1 */
int floyd_negative_cycle_vertex(const Table dist, int n)
{
    for (int i = 0; i < n; i++) {
        if (dist[i][i] < 0) {
            return i;
        }
    }
    return -1;
}

/* =====================================================================
 * 第 3 部分：路径还原
 * =====================================================================
 *
 * 只有 dist 矩阵的话，你知道"1 到 0 最短是 7"，但不知道【怎么走**。
 * 加一张 next 表即可：next[i][j] = 从 i 走到 j（按最短路）时，迈出的第一步落在哪个点。
 *
 *   初始：有直接边 i->j 时 next[i][j] = j
 *   松弛：dist[i][j] 被 dist[i][k] + dist[k][j] 改小时，next[i][j] = next[i][k]
 *         （从 i 出发去 j，现在要先走去 k，所以第一步和"去 k 的第一步"相同）
 *
 * 还原：从 i 开始，cur = next[cur][j] 一步步跳，跳到 j 为止。
 *
 * 边界情况：
 *   - next[i][j] == -1：压根不可达。
 *   - 有负环时 next 会在环上打转，所以步数必须设上限（max_len），否则死循环。
 */
int floyd_reconstruct(const Table next, int src, int dst, int out[], int max_len)
{
    if (src == dst) {
        out[0] = src;
        return 1;
    }
    if (next[src][dst] < 0) {
        return -1;                      /* 不可达 */
    }

    int len = 0;
    int cur = src;
    out[len++] = cur;
    while (cur != dst && len < max_len) {
        cur = next[cur][dst];           /* 跳到下一步 */
        if (cur < 0) {
            return -1;                  /* 理论上不该发生：半路断了说明 next 表没维护好 */
        }
        out[len++] = cur;
    }
    if (cur != dst) {
        return -1;                      /* 撞上限了（多半是负环），宁缺毋滥 */
    }
    return len;
}

/* =====================================================================
 * 第 4 部分：传递闭包（可达性）
 * =====================================================================
 *
 * 把 Floyd 的运算符换掉，同一套骨架立刻变成另一个算法：
 *
 *   最短路：  dist[i][j] = min(dist[i][j], dist[i][k] + dist[k][j])
 *   可达性：  reach[i][j] = reach[i][j] || (reach[i][k] && reach[k][j])
 *
 *   min  → 或(||)      因为"有任意一条路能到"就算到
 *    +   → 与(&&)      因为"i 能到 k 且 k 能到 j"才连得起来
 *
 * 这就是"最短路是半环上的 Floyd"的意思：把 (min, +) 这个半环换成 (或, 与)，
 * 算法框架一行都不用动，算的东西从"最短距离"变成"能不能到"。
 * 用途：判断图的强连通性、数据库里的传递依赖、编译器里的可达性分析。
 */
void floyd_transitive_closure(int reach[][MAX_V], int n)
{
    for (int k = 0; k < n; k++) {                  /* k 仍然必须在最外层，理由一模一样 */
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (reach[i][j] || (reach[i][k] && reach[k][j])) {
                    reach[i][j] = 1;
                }
            }
        }
    }
}
