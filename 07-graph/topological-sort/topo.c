#include <stdio.h>
#include <stdlib.h>
#include "topo.h"

/*
 * ===================================================================
 * 0. 就地重写的极简"容器"：一个结构同时当队列和栈用
 * ===================================================================
 * 叶子目录必须自包含，不能 #include "../../04-queue/.../cqueue.h"，
 * 所以这里自己写一个够用的（第 04 章那两节我只读不抄）。
 *
 * 为什么是"容器"而不是"队列"？因为拓扑排序对容器的脾气**毫不敏感**：
 *   规矩只有一条 —— 入度清零才有资格进容器。
 *   至于同时有好几个候选时先取谁，算法没规定。
 *   于是同一个结构体加一个 fifo 开关，就能同时扮演队列和栈：
 *
 *     fifo = 1（队列）：从**左端**取，data[0] 是下一个出场的
 *     fifo = 0（栈）  ：从**右端**取，data[n-1] 是下一个出场的
 *
 *     数组：  ┌───┬───┬───┬───┬───┐
 *             │ 4 │ 5 │ 0 │ 2 │   │
 *             └───┴───┴───┴───┴───┘
 *               ▲               ▲
 *             队头            栈顶        ← 同一份数据，两种取法，两个合法答案
 *
 * 有效元素永远是 data[0..n-1]（紧凑地贴在数组左边）。
 * 队列出队时把整段往前挪一格：O(n)，但 n ≤ V ≤ 16，
 * 这点搬运换来的是"数组里看到的就是容器真实的樣子"，教学上非常值。
 * 真要论性能，把它换成第 04 章的循环队列即可，算法一行都不用改。
 */
typedef struct {
    int data[TOPO_MAX_V];
    int n;          /* 当前装了几个 */
    int fifo;       /* 1 = 队列（先入先出），0 = 栈（后入先出） */
} Workset;

static void ws_init(Workset *w, int fifo)
{
    w->n = 0;
    w->fifo = fifo;
    /* data 不清零：n 划出了有效区间，没被写过的格子我们永远不会去读 */
}

static int ws_empty(const Workset *w)
{
    return w->n == 0;
}

/* 进容器：一律追加到右端（队尾 / 栈顶都在右边，这样 dump 出来的画面统一） */
static int ws_push(Workset *w, int x)
{
    if (w->n >= TOPO_MAX_V) {
        return 0;   /* 防御：一个顶点最多进一次，真满说明前提被破坏 */
    }
    w->data[w->n] = x;
    w->n++;
    return 1;
}

/* 出容器：队列从左端取（整体左移），栈从右端取（直接缩掉） */
static int ws_pop(Workset *w, int *out)
{
    if (ws_empty(w)) {
        return 0;
    }
    if (w->fifo) {
        *out = w->data[0];
        for (int i = 1; i < w->n; i++) {
            w->data[i - 1] = w->data[i];
        }
        w->n--;
    } else {
        w->n--;
        *out = w->data[w->n];
    }
    return 1;
}

/* 把容器当前内容画出来。这一步是整节戏眼：让人"看见"谁在等、谁先上 */
static void ws_dump(const Workset *w)
{
    if (w->n == 0) {
        printf("[ 空 ]");
        return;
    }
    printf("[ ");
    for (int i = 0; i < w->n; i++) {
        printf("%d ", w->data[i]);
    }
    printf("]");
    if (w->fifo) {
        printf(" ← 队头在左，先取队头");
    } else {
        printf(" ← 栈顶在右，先取栈顶");
    }
}

/*
 * ===================================================================
 * 1. 建图 / 收摊 / 加边
 * ===================================================================
 * 有向邻接表：heads[i] 挂的是"从 i 能直接去谁"。
 * 为什么用邻接表不用矩阵？拓扑排序的全部动作就是"扫 u 的出边"，
 * 邻接表扫一条边就是一条边（O(V+E)），矩阵要把 V×V 全扫一遍（O(V²)）。
 */
TopoGraph *topo_create(int v)
{
    if (v <= 0 || v > TOPO_MAX_V) {
        return NULL;    /* 本节所有出参数组都按 TOPO_MAX_V 开，超了会写越界 */
    }

    TopoGraph *g = malloc(sizeof(TopoGraph));
    if (g == NULL) {
        return NULL;
    }
    g->heads = malloc(sizeof(ENode *) * (size_t)v);
    if (g->heads == NULL) {
        free(g);        /* 第二块没 alloc 成功，第一块要还回去，否则泄漏 */
        return NULL;
    }
    for (int i = 0; i < v; i++) {
        g->heads[i] = NULL;   /* 每个顶点一开始"没有出边" = 空链 */
    }
    g->v = v;
    g->e = 0;
    return g;
}

/*
 * 收摊要按"盖楼的倒序"拆：先拆最里面的边节点，再拆指针数组，最后拆图本体。
 * 只 free 数组不 free 链表 —— 就是 ASan 报的 direct leak。
 */
void topo_free(TopoGraph *g)
{
    if (g == NULL) {
        return;
    }
    for (int i = 0; i < g->v; i++) {
        ENode *cur = g->heads[i];
        while (cur != NULL) {
            ENode *nxt = cur->next;   /* ★ 先把下一个存手里，才能 safe 地 free 掉当前 */
            free(cur);
            cur = nxt;
        }
        g->heads[i] = NULL;
    }
    free(g->heads);
    g->heads = NULL;
    g->v = 0;
    g->e = 0;
    free(g);
}

/*
 * 加一条有向边 from → to，读作"to 依赖 from：from 做完了才轮到 to"。
 * 尾插：让邻居链的顺序 == 加边顺序，输出可复现，README 才能逐行核对。
 * （头插是 O(1)，但会把邻居清单整个倒过来 —— 见 adjacency-list 那节的对比。）
 */
int topo_add_edge(TopoGraph *g, int from, int to)
{
    if (g == NULL || g->heads == NULL) {
        return 0;
    }
    if (from < 0 || from >= g->v || to < 0 || to >= g->v) {
        return 0;                /* 越界必须挡住，否则就是写飞指针 */
    }
    if (from == to) {
        /* 自环 u→u：这个点的入度永远清不了零，等于"这科必须先修自己"。
         * 我们照样存进去（让算法诚实地报出环），但要点破它是个特殊的环。 */
        printf("    [提示] 加了自环 %d→%d：它必然导致排不下去\n", from, to);
    }

    ENode *nd = malloc(sizeof(ENode));
    if (nd == NULL) {
        return 0;
    }
    nd->dest = to;
    nd->next = NULL;

    if (g->heads[from] == NULL) {
        g->heads[from] = nd;     /* 空链：直接当头 */
    } else {
        ENode *tail = g->heads[from];
        while (tail->next != NULL) {
            tail = tail->next;   /* 走到链尾 —— 这就是"尾插要 O(度)"的代价 */
        }
        tail->next = nd;
    }
    g->e++;
    return 1;
}

/* 现场重算入度表：遍历每条链，链上每个 dest 计一票 "有人要求我先做完" */
void topo_indeg(const TopoGraph *g, int indeg_out[])
{
    for (int i = 0; i < g->v; i++) {
        indeg_out[i] = 0;
    }
    for (int u = 0; u < g->v; u++) {
        for (const ENode *ed = g->heads[u]; ed != NULL; ed = ed->next) {
            indeg_out[ed->dest]++;    /* u→dest 这一条边，就是 dest 欠的一笔债 */
        }
    }
}

/* 把图摊开看清楚：拓扑排序的一切推理都建立在"谁指向谁"上 */
void topo_print(const TopoGraph *g)
{
    int indeg[TOPO_MAX_V];
    topo_indeg(g, indeg);

    printf("    顶点  出边邻接表（我做完能解锁谁）        入度  出度\n");
    for (int u = 0; u < g->v; u++) {
        /* ⚠ 这里绝不能用「先打一行占位、再 printf("\r") 回到行首重打」的写法：
         *   \r 只把光标挪回行首，**不会擦掉已经打出去的字节**。终端里看着像覆盖了，
         *   但一旦输出被重定向进文件（README 里贴的就是文件内容），
         *   两行内容会叠在同一行里，变成一堆乱码。要改就直接改成一次打对。 */
        printf("    %-5d ", u);
        int out_deg = 0;
        for (const ENode *ed = g->heads[u]; ed != NULL; ed = ed->next) {
            printf("%-2d-> ", ed->dest);
            out_deg++;
        }
        if (out_deg == 0) {
            printf("（无出边）");
        }
        printf("        %-5d %-5d", indeg[u], out_deg);
        if (indeg[u] == 0) {
            printf("  ← 入度 0：它是起点，谁都不要求它先做");
        }
        printf("\n");
    }
    printf("    合计：V = %d 个顶点，E = %d 条有向边（有向边只挂 1 个节点，所以节点数 == E）\n",
           g->v, g->e);
}

/*
 * ===================================================================
 * 2. 入度表两行打印器 —— 本节的"核心可视化"
 * ===================================================================
 * 为什么单独抽出来？因为整个 Kahn 算法的真相全写在这两行里：
 *     顶点: 0    1    2    3    4    5
 *     入度: 1    1    1    1    -    0
 * 每一步都重画一次，你会亲眼看到"入度一格一格被清零 → 那个点才出场"。
 * "-" 表示这个顶点已经输出过了，它的入度不再参与判断（防止你误以为还能减它）。
 */
static void print_indeg_rows(int v, const int indeg[], const int done[])
{
    printf("       顶点:");
    for (int u = 0; u < v; u++) {
        printf("%-4d", u);
    }
    printf("\n");
    printf("       入度:");
    for (int u = 0; u < v; u++) {
        if (done[u]) {
            printf("%-4s", "-");   /* 已出场：从这张表里"退赛" */
        } else {
            printf("%-4d", indeg[u]);
        }
    }
    printf("\n");
}

/*
 * ===================================================================
 * 3. topo_kahn：入度法（BFS 型）—— 本节主角
 * ===================================================================
 * 算法骨架 6 行，背下来：
 *
 *     indeg[] = 统计每个顶点的入度
 *     把**所有** indeg[v]==0 的顶点进容器        ← ★ 是"所有"，不是"随便挑一个"
 *     while 容器非空:
 *         u = 取出；u 写进结果表
 *         for u 的每条出边 u→v:
 *             indeg[v]--                       ← u 这条约束消失了，v 少欠一笔
 *             if indeg[v]==0: v 进容器          ← 债还清，轮到它上场
 *     返回结果表长度；< V 即有环
 *
 * 和 07-graph/bfs 的 bfs_run 摆在一起看，差别只在"什么资格进队列"：
 *     BFS  ：这个点我**没见过** → 进队（资格 = 首次被发现）
 *     Kahn ：这个点入度**清零** → 进队（资格 = 所有前置都已完成）
 * 骨架、循环、出队打点，全都是同一套。所以可以说：**拓扑排序 = 以"入度 0"为起点集的 BFS。**
 *
 * 为什么正确（一句话）：结果表里 u 一定在 v 前面，因为 v 只有在"它所有前驱都出过场"
 * 之后入度才会归零，而它入队这件事又必然发生在它出队（被写进结果表）之前。
 */
int topo_kahn(const TopoGraph *g, int fifo, int verbose,
              int order_out[], int stuck_out[], int *stuck_len_out)
{
    int indeg[TOPO_MAX_V];
    int done[TOPO_MAX_V];
    Workset box;

    topo_indeg(g, indeg);                    /* 第一步：先把债记清楚 */
    for (int u = 0; u < g->v; u++) {
        done[u] = 0;
    }
    ws_init(&box, fifo);

    /* 第二步：所有入度 0 的顶点全部进容器。
     * ⚠ 图可能不连通（有好几条互不相干的起点链），只塞一个起点就会漏掉一整片 ——
     *   这是本节最常见的 bug，和第 4 幕"图不连通时 BFS 只覆盖一个分量"是同一种病。 */
    int sources = 0;
    for (int u = 0; u < g->v; u++) {
        if (indeg[u] == 0) {
            ws_push(&box, u);
            sources++;
        }
    }

    if (verbose) {
        printf("  起点集：入度为 0 的顶点有 %d 个 → 全部装进容器（这就是第一批能开工的活）\n", sources);
        printf("       容器（%s）：", fifo ? "FIFO 队列" : "LIFO 栈");
        ws_dump(&box);
        printf("\n");
        print_indeg_rows(g->v, indeg, done);
        printf("       （图例：入度行的 \"-\" 表示该顶点已输出；只有入度 = 0 的顶点才有资格进容器）\n");
    }

    int count = 0;
    int step = 0;
    while (!ws_empty(&box)) {
        int u;
        ws_pop(&box, &u);                    /* 谁在容器里等着，谁就上场 */
        order_out[count] = u;
        done[u] = 1;
        count++;
        step++;

        if (verbose) {
            printf("  ── 第 %d 步 ── 取出 %d → 写入结果表 #%d\n", step, u, count - 1);
        }

        /* 第三步：u 做完了，它提过的那些"必须先有我"的要求全部作废 → 邻居入度集体 -1 */
        int newly = 0;
        for (const ENode *ed = g->heads[u]; ed != NULL; ed = ed->next) {
            int v = ed->dest;
            int before = indeg[v];
            indeg[v]--;
            if (verbose) {
                printf("       %d 做完，解锁它的出边 %d→%d：入度 %d → %d %s\n",
                       u, u, v, before, indeg[v],
                       (indeg[v] == 0) ? "★ 清零，进容器！" : "（还没清零，继续等别的前驱）");
            }
            if (indeg[v] == 0) {
                ws_push(&box, v);
                newly++;
            }
        }
        if (verbose) {
            if (g->heads[u] == NULL) {
                printf("       %d 没有出边（它是链的末尾），谁也没被解锁\n", u);
            }
            printf("       本步新入队 %d 个；容器现在（%s）：", newly, fifo ? "FIFO" : "LIFO");
            ws_dump(&box);
            printf("\n");
            print_indeg_rows(g->v, indeg, done);
        }
    }

    /* 第四步：判环。数一数排出了几个 —— 少一个就说明有人永远等不到"入度 0" */
    if (stuck_out != NULL || stuck_len_out != NULL) {
        int stuck_n = 0;
        for (int u = 0; u < g->v; u++) {
            if (!done[u]) {
                if (stuck_out != NULL) {
                    stuck_out[stuck_n] = u;
                }
                stuck_n++;
            }
        }
        if (stuck_len_out != NULL) {
            *stuck_len_out = stuck_n;
        }
    }

    if (verbose) {
        if (count == g->v) {
            printf("  容器空了，结果表长度 %d == 顶点数 %d → 无环，这是一个 DAG\n", count, g->v);
        } else {
            printf("  容器空了，但结果表只有 %d 个 < 顶点数 %d → 有环！剩下的卡死了\n", count, g->v);
        }
    }
    return count;
}

/*
 * 在"卡住的那批顶点"内部做一次可达性 DFS：
 * 从 from 出发，只允许踩 stuck 集合里的点，问能不能走回 target。
 * 能走回自己 ⇒ 我就坐在一个环上；走不回 ⇒ 我只是环的下游，被连累着一起卡住。
 */
static int reach_in_stuck(const TopoGraph *g, int from, int target,
                          const int allowed[], int visited[])
{
    for (const ENode *ed = g->heads[from]; ed != NULL; ed = ed->next) {
        int nxt = ed->dest;
        if (!allowed[nxt]) {
            continue;                        /* 不在卡住集合里的点不能作为环的一部分 */
        }
        if (nxt == target) {
            return 1;
        }
        if (!visited[nxt]) {
            visited[nxt] = 1;
            if (reach_in_stuck(g, nxt, target, allowed, visited)) {
                return 1;
            }
        }
    }
    return 0;
}

/*
 * 卡住的顶点分类报告。
 * 为什么值得单独写一个函数？因为初学者的直觉是"没排出去的都点在环上"，
 * 这是错的：环下游那些点（比如 3→4，3 在环上）也永远等不到入度 0，
 * 它们自己**不在**任何环里。把两类分开，才算真的看懂了 Kahn 的失败方式。
 */
void topo_report_stuck(const TopoGraph *g, const int stuck[], int stuck_len)
{
    int allowed[TOPO_MAX_V];
    for (int i = 0; i < g->v; i++) {
        allowed[i] = 0;
    }
    for (int i = 0; i < stuck_len; i++) {
        allowed[stuck[i]] = 1;
    }

    printf("    ① 坐在环上的顶点（从它出发、只在卡住集合里走，还能回到它自己）：");
    int any_cycle = 0;
    for (int i = 0; i < stuck_len; i++) {
        int visited[TOPO_MAX_V];
        for (int k = 0; k < g->v; k++) {
            visited[k] = 0;
        }
        visited[stuck[i]] = 1;
        if (reach_in_stuck(g, stuck[i], stuck[i], allowed, visited)) {
            printf("%d ", stuck[i]);
            any_cycle = 1;
        }
    }
    if (!any_cycle) {
        printf("（无）");
    }
    printf("\n");

    printf("    ② 只是被环连累的下游顶点（自己不在环上，但前置永远做不完）：");
    int any_drag = 0;
    for (int i = 0; i < stuck_len; i++) {
        int visited[TOPO_MAX_V];
        for (int k = 0; k < g->v; k++) {
            visited[k] = 0;
        }
        visited[stuck[i]] = 1;
        if (!reach_in_stuck(g, stuck[i], stuck[i], allowed, visited)) {
            printf("%d ", stuck[i]);
            any_drag = 1;
        }
    }
    if (!any_drag) {
        printf("（无）");
    }
    printf("\n");
}

/*
 * ===================================================================
 * 4. 方法 B：DFS 后序 + 整个反转
 * ===================================================================
 * 后序（post-order）的含义：**轮到我记账时，我的整棵子树都已经记完了。**
 * 于是后序表里"我"出现在我所有下游**之后**。
 * 拓扑序要的是"我"出现在我所有下游**之前"——把后序表倒过来放就正好。
 *
 * 为什么必须反转？看一条边 u→v：DFS 里 u 一定会在 v 变黑之前等着，
 * 所以 v 先被写进后序表、u 后被写进去，即 post 里 v 在 u 前面；
 * 倒过来之后 u 就在 v 前面了 —— 这恰好就是拓扑序的要求。
 * 反过来想更直观：DFS 是"先一头扎到底再往回收"，它天然把**链尾**排在前面，
 * 而拓扑序要**链头**在前 → 所以只能倒放。忘记倒放是本节第一大坑。
 */
static void dfs_post_rec(const TopoGraph *g, int u, int visited[],
                         int post[], int *len, int depth, int verbose)
{
    visited[u] = 1;
    if (verbose) {
        printf("    %*s· 进入 %d（它的所有下游要等我，我还没资格记账）\n", depth * 3, "", u);
    }
    for (const ENode *ed = g->heads[u]; ed != NULL; ed = ed->next) {
        if (verbose) {
            printf("    %*s  %d 的出边 %d→%d：%s\n", depth * 3, "", u, u, ed->dest,
                   visited[ed->dest] ? "已访问，跳过（这不是新分支）" : "往下钻");
        }
        if (!visited[ed->dest]) {
            dfs_post_rec(g, ed->dest, visited, post, len, depth + 1, verbose);
        }
    }
    post[*len] = u;                          /* ★ 收尾时才记 —— 这就是"后序" */
    (*len)++;
    if (verbose) {
        printf("    %*s· 退出 %d：邻居全处理完 → 压进后序表 #%d\n", depth * 3, "", u, *len - 1);
    }
}

int topo_dfs_postorder(const TopoGraph *g, int verbose, int post_out[], int *post_len_out)
{
    int visited[TOPO_MAX_V];
    int len = 0;
    for (int u = 0; u < g->v; u++) {
        visited[u] = 0;
    }

    /* 外层要扫全部顶点：图不连通时，从 0 出发的 DFS 只能覆盖它自己的分量。
     * 这一句和 Kahn 里"所有入度 0 的点都要入队"是同一个教训的两种写法。 */
    for (int u = 0; u < g->v; u++) {
        if (!visited[u]) {
            if (verbose) {
                printf("  DFS 新一轮：从还没访问过的 %d 出发（覆盖一个新的分量）\n", u);
            }
            dfs_post_rec(g, u, visited, post_out, &len, 0, verbose);
        }
    }
    if (post_len_out != NULL) {
        *post_len_out = len;
    }
    return len;
}

/*
 * ===================================================================
 * 5. DFS 三色标记：顺手把环抓出来
 * ===================================================================
 * 普通 DFS 的"已访问/未访问"两态不够用：它分不清
 *     "这个点正在我的调用栈上（那就是绕回来了 = 环）"
 * 和
 *     "这个点早就收摊了（那只是另一条已经走过的支路，安全）"。
 * 所以把"已访问"劈成两半：
 *
 *     白 TOPO_WHITE：还没碰过         → 可以往下钻
 *     灰 TOPO_GRAY ：在调用栈上，邻居还没走完 → ★ 被我的边指到 = 回边 = 有环
 *     黑 TOPO_BLACK：所有邻居已处理完 → 安全（它排在后面，不会形成环）
 *
 *          进入时涂灰  ──►  走完整条子树  ──►  退出时涂黑
 *     白 ──┘                                              └── 后序表在这一刻写
 *
 *     u ──► v 且 v 是灰  ⇔  v 是 u 的祖先（还在栈上等 u 收摊）
 *                       ⇔  存在 v ⇝ u（栈上路径）+ u→v（这条边）= 一个环
 *
 * 这就是"回边（back edge）"的定义。有向图里 **有回边 ⇔ 有环**，
 * 所以三色 DFS 既能出拓扑序，又能判环，还是求强连通分量（Tarjan / Kosaraju）的地基。
 */
static int dfs_color_rec(const TopoGraph *g, int u, int color[],
                         int post[], int *len, int depth, int verbose)
{
    color[u] = TOPO_GRAY;                    /* 上栈：我现在是"进行中" */
    if (verbose) {
        printf("    %*s· 进入 %d → 涂灰（进行中）\n", depth * 3, "", u);
    }
    for (const ENode *ed = g->heads[u]; ed != NULL; ed = ed->next) {
        int v = ed->dest;
        if (color[v] == TOPO_GRAY) {
            if (verbose) {
                printf("    %*s★ 出边 %d→%d 指向**灰色**顶点 %d：它还在调用栈上、正等着我收摊，\n",
                       depth * 3, "", u, v, v);
                printf("    %*s  也就是说 %d ⇝ %d 的路上又绕回 %d —— 这是一条**回边**，判定：有环！\n",
                       depth * 3, "", v, u, v);
            }
            return 1;                        /* 立刻上报，别再往下走了 */
        }
        if (color[v] == TOPO_WHITE) {
            if (verbose) {
                printf("    %*s  出边 %d→%d：白色 → 往下钻\n", depth * 3, "", u, v);
            }
            if (dfs_color_rec(g, v, color, post, len, depth + 1, verbose)) {
                return 1;                    /* 子树里发现了环，一路往上传 */
            }
        } else if (verbose) {
            printf("    %*s  出边 %d→%d：黑色 → 它早就收摊了，安全（不是回边）\n", depth * 3, "", u, v);
        }
    }
    color[u] = TOPO_BLACK;                   /* 收摊：下栈 */
    post[*len] = u;
    (*len)++;
    if (verbose) {
        printf("    %*s· 退出 %d → 涂黑（完成），压进后序表 #%d\n", depth * 3, "", u, *len - 1);
    }
    return 0;
}

int topo_dfs_color(const TopoGraph *g, int verbose, int post_out[], int *post_len_out)
{
    int color[TOPO_MAX_V];
    int len = 0;
    for (int u = 0; u < g->v; u++) {
        color[u] = TOPO_WHITE;
    }

    for (int u = 0; u < g->v; u++) {
        if (color[u] == TOPO_WHITE) {
            if (verbose) {
                printf("  三色 DFS 新一轮：从白色顶点 %d 出发\n", u);
            }
            if (dfs_color_rec(g, u, color, post_out, &len, 0, verbose)) {
                if (post_len_out != NULL) {
                    *post_len_out = len;     /* 已经抓到现场了，后序表残不重要 */
                }
                return 1;
            }
        }
    }
    if (post_len_out != NULL) {
        *post_len_out = len;
    }
    return 0;
}

/*
 * ===================================================================
 * 6. 工具三件套
 * ===================================================================
 */

/* 后序 → 拓扑序：整个倒过来。少了这一步，输出的是"反拓扑序"，逐条边都违反 */
void topo_reverse(const int in[], int n, int out[])
{
    for (int i = 0; i < n; i++) {
        out[i] = in[n - 1 - i];
    }
}

/*
 * 验收器：给定一个序列，逐条边检查 "from 是否真的排在 to 前面"。
 * 为什么要专门写它？因为"拓扑序不唯一"这件事听起来像耍赖 ——
 * 只要有一个裁判函数，我就能同时给 FIFO 版和 LIFO 版各打一个勾，
 * 让"两个不同的答案都对"变成可验证的事实，而不是一句解释。
 */
int topo_order_is_valid(const TopoGraph *g, const int order[], int len)
{
    if (len != g->v) {
        return 0;                            /* 长度都不对，谈不上顺序 */
    }

    int pos[TOPO_MAX_V];
    for (int u = 0; u < g->v; u++) {
        pos[u] = -1;
    }
    for (int i = 0; i < len; i++) {
        if (order[i] < 0 || order[i] >= g->v) {
            return 0;
        }
        if (pos[order[i]] != -1) {
            return 0;                        /* 同一个顶点出现两次 → 不是排列 */
        }
        pos[order[i]] = i;
    }
    for (int u = 0; u < g->v; u++) {
        if (pos[u] == -1) {
            return 0;                        /* 有顶点缺席（图不连通时容易犯） */
        }
    }

    for (int a = 0; a < g->v; a++) {
        for (const ENode *ed = g->heads[a]; ed != NULL; ed = ed->next) {
            if (pos[a] >= pos[ed->dest]) {
                return 0;                    /* 违反了这条边：a 必须比 dest 早 */
            }
        }
    }
    return 1;
}

void topo_print_order(const char *label, const int order[], int len)
{
    printf("  %s：", label);
    for (int i = 0; i < len; i++) {
        printf("%d%s", order[i], (i + 1 == len) ? "" : " ");
    }
    if (len == 0) {
        printf("（空）");
    }
    printf("\n");
}

/*
 * ===================================================================
 * 7. 本节示例图
 * ===================================================================
 */

/*
 * 演示用 DAG（6 点 6 边）：
 *
 *        4 ──► 0 ◄── 5      起点有两个：4 和 5（入度 0）
 *        │      ▲      │
 *        │      │      │    边：4→0  4→1  5→0  5→2  2→3  3→1
 *        ▼      │      ▼
 *        1 ◄── 3 ◄── 2
 *
 * 为什么特意挑它？
 *   1) 它有两个入度 0 的顶点（4 和 5）→ 逼出"起点集要全部入队"这一课；
 *   2) 中途容器里会出现两个候选（0 和 2 同时就绪）→ FIFO 与 LIFO 就此分道扬镳，
 *      第 3 幕的对照实验必须依赖这个条件才能成立；
 *   3) 它无环，能一次排出 6/6，先建立"正常情况长啥样"的基准。
 */
TopoGraph *topo_graph_dag(void)
{
    TopoGraph *g = topo_create(6);
    if (g == NULL) {
        return NULL;
    }
    static const int pairs[][2] = {
        {4, 0}, {4, 1}, {5, 0}, {5, 2}, {2, 3}, {3, 1}
    };
    for (int k = 0; k < 6; k++) {
        topo_add_edge(g, pairs[k][0], pairs[k][1]);
    }
    return g;
}

/*
 * 带环图（7 点 7 边）：三块结构各教一件事
 *
 *     0 ──► 1 ──► 2                5 ──► 6
 *           ▲     │                （干净分量：与环无关，照样能排出去）
 *           │     ▼
 *           └──── 3 ──► 4 ◄── 0
 *                （1→2→3→1 是一个环）
 *
 *   边：0→1  1→2  2→3  3→1  3→4  0→4  5→6
 *   入度：0:0 1:2 2:1 3:1 4:2 5:0 6:1
 *
 *   Kahn 只能排出 0、5、6 三个：
 *     - 1、2、3 坐在环上，入度各自被环里那条边永久占住 1 → 永远等不到 0；
 *     - 4 自己不在环上，但它的前驱 3 卡住了 → 被连累（4 的入度停在 1）。
 *   这一张图同时讲清"环上"与"环下游"，所以特意把 4 设计成"双前驱"（3 和 0）。
 */
TopoGraph *topo_graph_cycle(void)
{
    TopoGraph *g = topo_create(7);
    if (g == NULL) {
        return NULL;
    }
    static const int pairs[][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 1}, {3, 4}, {0, 4}, {5, 6}
    };
    for (int k = 0; k < 7; k++) {
        topo_add_edge(g, pairs[k][0], pairs[k][1]);
    }
    return g;
}
