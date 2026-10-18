#include <stdio.h>
#include <stdlib.h>   /* malloc / free */
#include <string.h>   /* memcpy */
#include "dijkstra.h"

/* 本文件里所有打印给中文读者看的引号都用「」，不用 ASCII 双引号 ——
 * 因为 ASCII 双引号写在字符串里会把字符串提前截断，编译器直接报错。 */

/* =====================================================================
 * 第 0 部分：图怎么建
 * ===================================================================== */

/*
 * 建一张空图。
 *
 * 为什么矩阵要填 INF_DIST 而不是 0？
 *   因为在最短路里「0」是一个合法的距离（起点到自己就是 0），
 *   「没有边」必须用一个不可能出现的值表示，否则区分不了「零距离的边」和「没有边」。
 *
 * 边界情况：
 *   - n 超过 MAX_V 直接退出：这是教学 demo，不能让越界写内存悄悄发生。
 *   - 对角线 w[i][i] 写 0，表示自己到自己距离 0；松弛时不会把别人改坏。
 */
void graph_init(Graph *g, int n, int directed)
{
    if (n > MAX_V) {
        printf("[错误] 顶点数 %d 超过本 demo 上限 %d\n", n, MAX_V);
        return;
    }
    g->n = n;
    g->directed = directed;
    g->m = 0;
    for (int u = 0; u < n; u++) {
        for (int v = 0; v < n; v++) {
            g->w[u][v] = (u == v) ? 0 : INF_DIST;   /* 无边 = ∞，自己到自己 = 0 */
        }
    }
}

/*
 * 加一条边。
 *
 * 无向图（directed == 0）：一条边 0-1 会存成两条弧 0->1 和 1->0，
 *   因为「从 0 能走到 1」和「从 1 能走到 0」是同一件事的两个方向。
 *   矩阵里 w[0][1] 和 w[1][0] 都写，所以无向图的矩阵是左右对称的。
 *
 * 边界情况：
 *   - 重边（同两点加两次）：矩阵只保留最后写进去的那条，边表两条都留。
 *     本 demo 没有重边；正式代码应该在这里取 min。
 *   - 弧条数超过 MAX_E：打印警告并忽略，避免数组越界。
 */
void graph_add_edge(Graph *g, int u, int v, int weight)
{
    if (u < 0 || u >= g->n || v < 0 || v >= g->n) {
        printf("[错误] 边 %d->%d 有顶点越界（n=%d）\n", u, v, g->n);
        return;
    }
    if (g->m + 2 > MAX_E) {
        printf("[错误] 弧条数超过上限 %d，这条边被忽略\n", MAX_E);
        return;
    }

    g->w[u][v] = weight;
    g->e[g->m].from = u; g->e[g->m].to = v; g->e[g->m].weight = weight;
    g->m++;

    if (!g->directed) {                        /* 反向那条弧 */
        g->w[v][u] = weight;
        g->e[g->m].from = v; g->e[g->m].to = u; g->e[g->m].weight = weight;
        g->m++;
    }
}

/*
 * 本仓库 07-graph 各节共用的示例图：无向带权，6 个顶点 0..5，9 条边。
 *
 *   主链： (0) ──7── (1) ──10── (2) ──11── (3) ──6── (4) ──9── (5)
 *   跨点： (0) ──9── (2)    (0) ──14── (5)    (1) ──15── (3)    (2) ──2── (5)
 *
 * 这张图专门留了一个教学陷阱：0 到 5 有一条直连边权 14，
 * 但 0 -> 2 -> 5 只要 9 + 2 = 11。
 * 「眼前最便宜的边」不等于「最短的路」：dist[5] 一开始必须被写成 14（这是当时唯一知道的路），
 * 之后在第 3 轮用顶点 2 松弛才降到 11。这个「先接受一个较大的临时值，之后发现更小再改」的动作
 * 就叫松弛（relax），它是 Dijkstra 的心脏。
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

/* 打印边表：这就是「图长什么样」的机器可读版本，方便逐条核对 */
void graph_print_edges(const Graph *g)
{
    printf("  %-12s: %s\n", "图的类型", g->directed ? "有向图" : "无向图");
    if (g->directed) {
        printf("  顶点数 = %d, 弧数 = %d\n", g->n, g->m);
        for (int i = 0; i < g->m; i++) {
            printf("      弧 %2d:  %d --> %-2d   权 %d\n", i,
                   g->e[i].from, g->e[i].to, g->e[i].weight);
        }
    } else {
        printf("  顶点数 = %d, 边数 = %d（无向边在边表里存成两条弧，所以弧数 = %d）\n",
               g->n, g->m / 2, g->m);
        for (int i = 0; i + 1 < g->m; i += 2) {
            printf("      边 %2d:  %d <-> %-2d   权 %d\n", i / 2,
                   g->e[i].from, g->e[i].to, g->e[i].weight);
        }
    }
}

/* =====================================================================
 * 第 0.5 部分：打印小工具
 * ===================================================================== */

/*
 * 把一个距离格式化成人眼好读的 4 列宽字符串：∞ 代表「还到不了」。
 *
 * 为什么不直接 printf("%4d") 然后把 INF 打印成一堆 1？
 *   因为 1000000000 会把表格撑爆，而且读者根本不知道那是「无穷」还是真实距离。
 * 为什么手工补三个空格而不是 printf("%4s", "∞")？
 *   因为 ∞ 在 UTF-8 里占 3 个字节，%4s 按字节算宽度，会补错位置。
 *   终端里 ∞ 显示出来是 1 列宽，所以「3 空格 + ∞」正好对齐 4 列。
 */
static void fmt_val(int v, char *buf, size_t sz)
{
    if (v >= INF_DIST) {
        snprintf(buf, sz, "   %s", "∞");
    } else {
        snprintf(buf, sz, "%4d", v);
    }
}

void print_dist_row(const char *label, const int dist[], int n)
{
    char cell[8];
    printf("      %-11s [", label);
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
    printf("      %-11s [", label);
    for (int v = 0; v < n; v++) {
        printf("%4d", parent[v]);   /* -1 表示「还没有前驱」：要么没访问到，要么就是起点 */
        if (v + 1 < n) {
            printf(", ");
        }
    }
    printf(" ]\n");
}

/*
 * 回溯打印最短路径：0 -> 2 -> 5   (11)
 *
 * 原理：parent[v] 记的是「v 的 dist 是被谁更新出来的」，
 *       把所有 parent 链连起来就是一棵【最短路径树】。从终点往回跳到起点即可。
 *       往回跳得到的是倒序，所以先攒进数组再倒着打印 —— 这就是栈的用法。
 *
 * 边界情况：
 *   - dst == src：循环一次都不进，直接输出「0」。
 *   - dist[dst] == ∞：根本到不了，打印「不可达」而不是硬走 parent 链（链上是 -1，会读脏数据）。
 *   - parent 链万一被打断成 -1（图不连通 / 代码写错），len 上限保护，绝不无限循环。
 */
void print_shortest_path(const int parent[], int src, int dst, const int dist[])
{
    int trail[MAX_V];
    int len = 0;

    if (dist != NULL && dist[dst] >= INF_DIST) {
        printf("  从 %d 到 %d：不可达（dist = ∞，没有任何一条路能走过去）\n", src, dst);
        return;
    }

    int cur = dst;
    while (cur != src && cur != -1 && len < MAX_V) {
        trail[len++] = cur;
        cur = parent[cur];
    }
    trail[len++] = src;

    printf("  从 %d 到 %d 的最短路径：", src, dst);
    for (int i = len - 1; i >= 0; i--) {
        printf("%d", trail[i]);
        if (i > 0) {
            printf(" -> ");
        }
    }
    if (dist != NULL) {
        printf("   (%d)\n", dist[dst]);
    } else {
        printf("\n");
    }
}

/*
 * 整张图里有没有负权边？
 * 单独扫一遍边表来回答，而不是在松弛过程里顺手看：
 *   因为负权边很可能压根不会被松弛到（它指向一个已经定稿的点），
 *   那样就漏报了。检查前提条件必须【无条件全扫】。
 */
static int graph_has_negative_edge(const Graph *g)
{
    for (int i = 0; i < g->m; i++) {
        if (g->e[i].weight < 0) {
            return 1;
        }
    }
    return 0;
}

/* =====================================================================
 * 第 1 部分：朴素 Dijkstra  O(V^2)
 * =====================================================================
 *
 * 一句话原理：
 *   把点分成两拨 —— 「已定稿」和「还没定稿」。
 *   每一轮从【还没定稿】的点里挑 dist 最小的那个 u，宣布「u 的最短距离就是 dist[u]」，
 *   然后拿 u 去更新它所有邻居的距离。这个更新动作叫「松弛」。
 *
 * 为什么「挑最小就敢定稿」？（反证法 —— 这是 Dijkstra 正确性的命根子）
 *   假设 u 是未定稿里 dist 最小的，但真正的最短距离比 dist[u] 还小。
 *   那条更短的路总有一个时刻第一次离开「已定稿集合」，走到某个未定稿的点 x。
 *   因为边权非负，从 x 再往后走只会越来越远，而 dist[x] >= dist[u]（u 才是未定稿里最小的），
 *   所以这条路不可能比 dist[u] 短 —— 矛盾。故 u 可以安全定稿。
 *
 * ⚠ 证明里「边权非负」用在了「从 x 往后走只会更远」这一步。
 *   有负权边时这一步塌了，见第 3 部分的反例。这就是 Dijkstra 和 Bellman-Ford 的分界线。
 *
 * 内存布局（共用图，第 2 轮松弛完的样子）：
 *
 *   下标 v  :    0      1      2      3      4      5
 *   dist  [    0,     7,     9,    22,     ∞,    14   ]   ← 临时值：目前知道的最短距离
 *   parent[   -1,     0,     0,     1,    -1,     0   ]   ← 这个 dist 是被谁更新出来的
 *   定稿  [    ★,     ★,     ·,     ·,     ·,     ·  ]   ← ★ 已定稿，之后绝不再看第二次
 *                        ↑
 *              下一轮就选未定稿里最小的 2（dist=9）
 */
int dijkstra_naive(const Graph *g, int src, int dist[], int parent[], int show_steps)
{
    int visited[MAX_V];                       /* visited[v] = 1 表示 v 已定稿 */
    int has_neg = graph_has_negative_edge(g);

    if (has_neg && show_steps) {
        printf("      ⚠ 警告：这张图里有负权边，Dijkstra 的前提被破坏，下面的结果【可能是错的】。\n");
        printf("        这正是它和 Bellman-Ford 的分界线，详见 README 的「负权反例」一节。\n");
    }

    for (int v = 0; v < g->n; v++) {
        dist[v] = INF_DIST;                   /* 一开始谁都不认识，除了起点全是 ∞ */
        parent[v] = -1;
        visited[v] = 0;
    }
    dist[src] = 0;                            /* 唯一已知的真实值 */

    if (show_steps) {
        printf("      开局：只有起点 %d 的 dist = 0，其他全 ∞（意思是「还不知道怎么去」）\n", src);
        print_dist_row("dist", dist, g->n);
        print_parent_row("parent", parent, g->n);
    }

    for (int round = 1; round <= g->n; round++) {
        /* ---- 步骤 1：在未定稿的点里线性扫一遍，找 dist 最小的 ----
         * 这一句就是 O(V^2) 里的那个 V：V 轮 × 每轮扫 V 个点。
         * 堆优化要省掉的正是这次扫描。
         *
         * 并列最小怎么办？用的是严格小于 <，所以留下编号最小的那个。
         * 共用图第 5 轮 dist[3] 和 dist[4] 都是 20 打平（4 已经先定稿了，所以这里其实无并列），
         * 打平选谁都不影响 dist 的正确性，只可能影响 parent 长什么样。
         */
        int u = -1;
        for (int v = 0; v < g->n; v++) {
            if (!visited[v] && dist[v] < INF_DIST && (u == -1 || dist[v] < dist[u])) {
                u = v;
            }
        }

        if (u == -1) {
            /* 未定稿里已经没有可达点 —— 剩下的点全都不可达，dist 保持 ∞。
             * 这是「非连通图」的边界情况，必须在这里 break，
             * 否则 u = -1 会去读 w[-1][...]，那是数组越界（ASan 会当场抓住）。 */
            if (show_steps) {
                printf("\n      第 %d 轮：未定稿集合里已经没有可达点了 —— 剩下的点不可达，提前结束\n", round);
            }
            break;
        }

        visited[u] = 1;                       /* 定稿！从此再不看它第二次（负权出错的根源就在这一步） */

        int before[MAX_V];
        memcpy(before, dist, sizeof before);   /* 先留一份松弛前的快照，方便对照看变化 */

        if (show_steps) {
            char cb[8];
            fmt_val(dist[u], cb, sizeof cb);
            printf("\n      第 %d 轮：选定顶点 %d（未定稿里 dist 最小，= %s）→ 定稿，不再修改\n", round, u, cb);
            print_dist_row("松弛前", before, g->n);
        }

        /* ---- 步骤 2：拿 u 去松弛它的所有出边 ----
         * 这里遍历 v = 0..n-1 而不是「u 的邻居列表」，因为用的是邻接矩阵：
         * 矩阵不会告诉你谁和谁相连，只能一行全扫。
         * 这正是邻接矩阵在稀疏图上的浪费（也是堆优化版改用边表的原因）。
         */
        int hit_to[MAX_V];                     /* 本轮被改小的点，攒起来打印 */
        int hit_old[MAX_V];
        int hit_new[MAX_V];
        int nhit = 0;

        for (int v = 0; v < g->n; v++) {
            if (visited[v]) {
                continue;                      /* 已定稿的点不可能更短 —— 这个「不可能」正是负权会出错的地方 */
            }
            if (g->w[u][v] >= INF_DIST) {
                continue;                      /* u 和 v 之间没有边 */
            }
            int nd = dist[u] + g->w[u][v];     /* 候选新距离：绕 u 去 v 要多远 */
            if (nd < dist[v]) {
                /* 只有「变得更小」才改。nd == dist[v] 时不改，所以并列情况保留先来的那条路径 */
                hit_old[nhit] = dist[v];
                hit_new[nhit] = nd;
                hit_to[nhit] = v;
                nhit++;
                dist[v] = nd;
                parent[v] = u;                 /* 记住「你是从 u 来的」，最后才能回溯出完整路径 */
            }
        }

        if (show_steps) {
            if (nhit == 0) {
                printf("            本轮没有松弛任何边（%d 的邻居要么已定稿，要么绕它并不更便宜）\n", u);
            } else {
                printf("            本轮松弛了 %d 条边：\n", nhit);
                for (int k = 0; k < nhit; k++) {
                    char ob[8], nb[8];
                    fmt_val(hit_old[k], ob, sizeof ob);
                    fmt_val(hit_new[k], nb, sizeof nb);
                    printf("              %d -> %d（边权 %d）: dist[%d] 从 %s 改成 %s，parent[%d] = %d\n",
                           u, hit_to[k], g->w[u][hit_to[k]], hit_to[k], ob, nb, hit_to[k], u);
                }
            }
            print_dist_row("松弛后", dist, g->n);
            print_parent_row("parent", parent, g->n);
            printf("            已定稿集合 = { ");
            for (int v = 0; v < g->n; v++) {
                if (visited[v]) {
                    printf("%d ", v);
                }
            }
            printf("}\n");
        }
    }

    return has_neg ? 1 : 0;
}

/* =====================================================================
 * 第 2 部分：自己写的极简小顶堆
 * =====================================================================
 *
 * 为什么 Dijkstra 需要堆？
 *   朴素版每轮都要「扫全部点找最小」：V 轮 × O(V) = O(V^2)。
 *   如果有结构能 O(log V) 把「当前最小的」递给我，总代价就变成 O((V + E) log V)。
 *   点少边多（稠密图）时堆反而不划算；点多边少（稀疏图）时堆完胜。
 *
 * 堆就是一棵「用数组存的完全二叉树」，只保证一条性质：父亲 <= 两个孩子。
 * 注意【只保证父子，不保证兄弟】，所以堆里第 2 小的不一定在 index 1。
 *
 *   数组：  [ 2, 7, 9, 22, 30, 14 ]
 *   画成树：        0          ← 下标 0，值 2，全局最小
 *                /   \
 *               1     2        ← 下标 1、2，值 7、9
 *              / \   /
 *             3   4 5             值 22、30、14
 *   下标关系：i 的孩子 = 2i+1、2i+2；i 的父亲 = (i-1)/2  ← 「数组下标即父子」，不用指针
 *
 * 上浮 sift-up：新元素放数组末尾，不断和父亲比，比父亲小就换上去。
 * 下沉 sift-down：末尾元素搬到堆顶，不断和「两个孩子里更小的那个」比，比孩子大就换下去。
 *   ★ 必须是「更小的那个孩子」：跟大的换会留下「父亲比小儿子还大」，堆性质当场破。
 *
 * 完整例子（依次 push 5,3,7,2,9,4，再一直 pop；下面每一行都是堆数组的真实状态）：
 *
 *   push 5   [5]
 *   push 3   [3,5]                3 落在下标 1，父亲 (1-1)/2=0 是 5，3<5 ⇒ 上浮换一次
 *   push 7   [3,5,7]              7 在下标 2，父亲是 3，7>3 ⇒ 不动
 *   push 2   [2,3,7,5]            2 在下标 3，先和父亲 7 换，再和父亲 3 换，一路顶到堆顶
 *   push 9   [2,3,7,5,9]          9 在下标 4，父亲 3，不动
 *   push 4   [2,3,4,5,9,7]        4 在下标 5，父亲 7 ⇒ 换；再和父亲 2 比，2<4 ⇒ 停
 *   pop 2    [3,5,4,7,9]          返回堆顶 2；把末位 7 搬到堆顶 ⇒ [7,3,4,5,9]；
 *                                 下沉：孩子 3、4 中较小的是 3，7>3 ⇒ 换 ⇒ [3,7,4,5,9]；
 *                                 再到下标 1，孩子 5、9 中较小的是 5，7>5 ⇒ 换 ⇒ [3,5,4,7,9]；停
 *   pop 3    [4,5,9,7]
 *   pop 4    [5,7,9]
 *   pop 5    [7,9]
 *   pop 7    [9]
 *   pop 9    []
 *
 *   读出三件事：
 *     1) 弹出顺序是 2,3,4,5,7,9 —— 这就是「堆排序」的骨架：全 push 再全 pop 即得有序序列。
 *     2) 数组任何时候都不是排好序的（[2,3,4,5,9,7] 里 9 在 7 前面），堆只管「爹 <= 儿」。
 *     3) push/pop 每次只沿一条「根 ↔ 叶子」的路线走，长度 = 树高 = log2(size) ⇒ O(log n)。
 */
MinHeap *heap_create(int cap)
{
    if (cap <= 0) {
        cap = 8;
    }
    /* 一次 malloc 把「结构体 + 数组」连着要下来：少一次分配、也少一次 free。
     * 代价是 item 这个指针必须指向结构体后面的那截内存，所以 HeapItem 的分配量单独算。 */
    MinHeap *h = malloc(sizeof(MinHeap) + (size_t)cap * sizeof(HeapItem));
    if (h == NULL) {
        printf("[错误] 堆内存申请失败（想要 %d 个元素）\n", cap);
        return NULL;
    }
    h->item = (HeapItem *)(h + 1);   /* h + 1 是「结构体末尾之后的地址」，正好是数组入口 */
    h->size = 0;
    h->cap = cap;
    return h;
}

void heap_destroy(MinHeap *h)
{
    free(h);   /* 只申请了一块，free 一次就够。free(NULL) 是合法空操作，所以不用判空 */
}

int heap_push(MinHeap *h, int vertex, int dist)
{
    if (h == NULL || h->size >= h->cap) {
        return -1;                  /* 满了。教学 demo 用固定容量；正式代码这里该 realloc 扩容 */
    }
    int i = h->size++;
    h->item[i].vertex = vertex;
    h->item[i].dist = dist;

    while (i > 0) {                 /* 上浮：只要比父亲小就一直往上换 */
        int father = (i - 1) / 2;
        if (h->item[father].dist <= h->item[i].dist) {
            break;                  /* 「父 <= 子」已经成立，停 */
        }
        HeapItem tmp = h->item[father];
        h->item[father] = h->item[i];
        h->item[i] = tmp;
        i = father;
    }
    return 0;
}

int heap_pop(MinHeap *h, HeapItem *out)
{
    if (h == NULL || h->size == 0 || out == NULL) {
        return -1;                  /* 空堆拿不出最小值 —— 边界情况交给调用者处理 */
    }
    *out = h->item[0];              /* 堆顶一定是全局最小 —— 这就是堆对 Dijkstra 的全部价值 */

    h->size--;
    if (h->size > 0) {
        h->item[0] = h->item[h->size];   /* 拿最后一个元素补到顶上（它可能很大，所以要下沉） */
        int i = 0;
        for (;;) {
            int left = 2 * i + 1;
            int right = 2 * i + 2;
            int smallest = i;
            if (left < h->size && h->item[left].dist < h->item[smallest].dist) {
                smallest = left;
            }
            if (right < h->size && h->item[right].dist < h->item[smallest].dist) {
                smallest = right;   /* 关键：和「两个孩子里更小的那个」比。
                                     * 跟大的换会留下「父亲比小儿子还大」，堆性质当场破。 */
            }
            if (smallest == i) {
                break;              /* 已经不比任何孩子大，堆性质恢复 */
            }
            HeapItem tmp = h->item[i];
            h->item[i] = h->item[smallest];
            h->item[smallest] = tmp;
            i = smallest;
        }
    }
    return 0;
}

void heap_print(const MinHeap *h)
{
    if (h == NULL) {
        return;
    }
    printf("              堆里的历史快照: [");
    for (int i = 0; i < h->size; i++) {
        printf("%d:%d", h->item[i].vertex, h->item[i].dist);   /* 顶点:距离 */
        if (i + 1 < h->size) {
            printf(" ");
        }
    }
    printf("]   （共 %d 条，堆顶在数组第 0 格）\n", h->size);
}

/* =====================================================================
 * 第 2.5 部分：堆优化版 Dijkstra   O((V + E) log V)
 * =====================================================================
 *
 * 和朴素版唯一的区别：把「扫一遍找最小」换成「从堆里弹一个」。
 *
 * ★ 本节最大的坑，必须讲透：【弹出来的距离可能已经过期】
 *
 *   我们每次松弛成功就往堆里塞一条新记录，但从来不修改堆里已有的旧记录。
 *   （那个操作叫 decrease-key「降低关键字」，手写堆要额外存「顶点在堆数组里的下标」才能做，
 *    C 里的常见工程折中是：塞新记录 + 弹出时丢掉过期的，叫 lazy deletion 延迟删除。）
 *
 *   于是同一个顶点可能在堆里留好几条：
 *        第 1 轮入堆：(5, 14)      ← 那时候只知道 0->5 这条 14 的路
 *        第 3 轮入堆：(5, 11)      ← 发现 0->2->5 只要 11
 *   先弹出 (5,11)：有效，定稿。后弹出 (5,14)：dist[5] 已经是 11，14 是张历史照片，必须扔掉：
 *
 *        if (d > dist[u]) continue;      ← 就这一行。忘了它，白干还可能有 bug
 *
 *   为什么判「>」就够、不会反过来「d < dist[u]」？
 *     因为入堆时写的就是当时的 dist，之后 dist 只会往小改，所以历史快照只会 >= 当前值。
 *     这个「只会往小改」同样依赖边权非负。
 */
int dijkstra_heap(const Graph *g, int src, int dist[], int parent[], int show_steps,
                  int *stale_pops_out)
{
    int visited[MAX_V];
    int has_neg = graph_has_negative_edge(g);
    int stale = 0;                          /* 丢掉了多少条过期记录 */
    int accepted = 0;                       /* 定稿了多少个点 */

    MinHeap *h = heap_create(MAX_E + 4);    /* 每条弧最多引起一次入堆，容量给够，demo 里就不会满 */
    if (h == NULL) {
        return -1;
    }

    for (int v = 0; v < g->n; v++) {
        dist[v] = INF_DIST;
        parent[v] = -1;
        visited[v] = 0;
    }
    dist[src] = 0;
    heap_push(h, src, 0);

    if (show_steps) {
        if (has_neg) {
            printf("      ⚠ 这张图有负权边，堆优化版同样会错（错法见 README）\n");
        }
        printf("      起点 %d 入堆。堆里存的是「(顶点, 当时的距离)」快照，不是指针，不会自动跟着 dist 变。\n", src);
        heap_print(h);
    }

    while (h->size > 0) {
        HeapItem cur;
        heap_pop(h, &cur);                  /* 取出当前最小的一条 */

        int u = cur.vertex;
        int d = cur.dist;

        if (d > dist[u]) {
            stale++;                        /* ★ 过期记录，直接丢弃 */
            if (show_steps) {
                printf("      弹出 (顶点%d, 距离%d)：但 dist[%d] 现在只有 %d —— 这条是历史照片，丢弃\n",
                       u, d, u, dist[u]);
            }
            continue;
        }
        if (visited[u]) {
            /* d == dist[u] 但上一轮已经定稿过：同一距离的重复条目，也丢弃。
             * 不加这个判断，同一个点会被「二次定稿」，邻居被重复松弛一遍，
             * 结果虽然还对，但复杂度会退化。 */
            if (show_steps) {
                printf("      弹出 (顶点%d, 距离%d)：它上一轮已经定稿，重复条目丢弃\n", u, d);
            }
            continue;
        }

        visited[u] = 1;
        accepted++;
        if (show_steps) {
            printf("      第 %d 步：弹出 (顶点%d, 距离%d) —— 有效，定稿 %d\n", accepted, u, d, u);
        }

        /* 遍历 u 的出边：这里扫边表，只关心 from == u 的弧。
         * 大图应该把「同一个起点的弧」链在一起（邻接表），那样才是真的 O(E) 总扫描。 */
        for (int i = 0; i < g->m; i++) {
            if (g->e[i].from != u) {
                continue;
            }
            int v = g->e[i].to;
            int weight = g->e[i].weight;
            if (visited[v]) {
                continue;                   /* 定稿的不再更新 */
            }
            if (dist[u] + weight < dist[v]) {
                dist[v] = dist[u] + weight;
                parent[v] = u;
                heap_push(h, v, dist[v]);   /* 只塞新的，旧的留在堆里等着被丢 */
                if (show_steps) {
                    printf("              松弛 %d -> %d（边权 %d）: dist[%d] = %d，把 (顶点%d, %d) 入堆\n",
                           u, v, weight, v, dist[v], v, dist[v]);
                }
            }
        }
        if (show_steps) {
            print_dist_row("dist", dist, g->n);
            heap_print(h);
        }
    }

    if (stale_pops_out != NULL) {
        *stale_pops_out = stale;
    }
    if (show_steps) {
        printf("      堆空了，算法结束：定稿 %d 个点，丢弃过期/重复记录 %d 条。\n", accepted, stale);
        printf("      → 这就是「堆里塞记录很便宜，代价延后到弹出时结算」，也就是延迟删除。\n");
    }
    heap_destroy(h);                        /* 谁申请谁负责：出口前必须 free */
    return has_neg ? 1 : 0;
}

/* =====================================================================
 * 第 3 部分：暴力求最短路（只给「负权反例」当标准答案用）
 * =====================================================================
 *
 * 做法：把从 src 出发的【每一条简单路径】（不重复经过点）都走一遍，记最小值。
 * 路径数是指数级的，所以只能用在 3~4 个点的小图上。
 *
 * 为什么它能处理负权？
 *   因为它从不「定稿」任何东西 —— 所有可能性都试完了才给答案。
 *   Dijkstra 错在「过早定稿」，暴力没有「过早」这回事。
 *
 * ⚠ 前提：不能有负环。有负环时最短路本身没有定义，
 *   这个暴力会给出一个有限但无意义的值 —— 判负环请用 07-graph/bellman-ford。
 */
static void dfs_all_paths(const Graph *g, int u, int walked, int visited[], int dist[])
{
    for (int i = 0; i < g->m; i++) {
        if (g->e[i].from != u) {
            continue;
        }
        int v = g->e[i].to;
        if (visited[v]) {
            continue;                       /* 不重复经过点：保证只枚举简单路径 */
        }
        int nd = walked + g->e[i].weight;
        if (nd < dist[v]) {
            dist[v] = nd;                   /* 发现一条更短的，记下来 */
        }
        visited[v] = 1;
        dfs_all_paths(g, v, nd, visited, dist);   /* 继续往下走，说不定后面还有惊喜 */
        visited[v] = 0;                     /* 回溯：擦掉「我来过」的记号，好让别的路径也能走这里 */
    }
}

void shortest_by_brute_force(const Graph *g, int src, int dist[])
{
    int visited[MAX_V];
    for (int v = 0; v < g->n; v++) {
        dist[v] = INF_DIST;
        visited[v] = 0;
    }
    dist[src] = 0;
    visited[src] = 1;
    dfs_all_paths(g, src, 0, visited, dist);
}
