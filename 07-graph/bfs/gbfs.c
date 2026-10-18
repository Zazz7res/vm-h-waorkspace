#include <stdio.h>
#include "gbfs.h"

/*
 * ===================================================================
 * 0. 就地重写的极简 int 队列（第 04 章的复用，20 行版本）
 * ===================================================================
 * 叶子目录必须自包含，所以不能 #include "../../04-queue/.../cqueue.h"，
 * 这里自己写一个够用的。
 *
 * 结构：
 *     data[]   存顶点的数组
 *     head     下一个要"取"的位置
 *     tail     下一个要"放"的位置
 *
 *     ┌───┬───┬───┬───┬───┐
 *     │ 1 │ 2 │ 3 │   │   │
 *     └───┴───┴───┴───┴───┘
 *       ▲           ▲
 *       head        tail      ← head==tail 就是空；tail==容量 就是"写满了"
 *
 * 这是**朴素数组队列**（pop 只让 head 往前走，不回收、不取模、不绕回）。
 * 第 04 章 `array-queue` 那节说过它"会假满"，那 `circular-queue` 才是正式版。
 *
 * ★ 那为什么 BFS 这里敢用朴素版？（这是本节一个很漂亮的观察）
 *   因为**一个顶点最多进队一次**：我们是"入队的同时就打 visited 标记"，
 *   所以后来任何想把同一个点再推进来的边，都会被 visited 直接拦掉。
 *   于是：进队总次数 ≤ V，tail 单调递增也永远不超过 V。
 *   数组开到 V+1 就绝对够用，根本不会假满。
 *
 *   一般化成一句话：**当你证明了一个操作最多发生 N 次，
 *   就可以用"只往前走不回绕"的简单结构，把环形缓冲那套复杂度省掉。**
 *
 *   ⚠ 但这个结论**完全依赖"入队即标记"**。如果把打标记推迟到"出队时"，
 *   同一个点会被多条边重复入队，进队次数能冲到 O(E)，朴素数组就会真溢出。
 *   这是 BFS 最常见的实现 bug，详见 README 的坑清单。
 */
typedef struct {
    int data[BFS_MAX_V + 1];
    int head;
    int tail;
} IntQueue;

/* 建一个空队列：head 和 tail 都指到 0，"head == tail" 即空 */
static void q_init(IntQueue *q)
{
    q->head = 0;
    q->tail = 0;
    /* data 里不用清：没被写过的格子我们永远不会去读（head/tail 划出了有效区间） */
}

static int q_is_empty(const IntQueue *q)
{
    return q->head == q->tail;   /* 两个指针碰头 = 中间一个元素都没有 */
}

/* 进队：写到 tail 那一格，然后 tail 前进一步。返回 0 表示满了（本场景不该发生） */
static int q_push(IntQueue *q, int x)
{
    if (q->tail >= BFS_MAX_V + 1) {
        return 0;    /* 防御性检查：真走到这里说明"入队即标记"的前提被破坏了 */
    }
    q->data[q->tail] = x;
    q->tail++;
    return 1;
}

/* 出队：读 head 那一格，然后 head 前进一步。空队列返回 0，并把 *out 保持原样 */
static int q_pop(IntQueue *q, int *out)
{
    if (q_is_empty(q)) {
        return 0;
    }
    *out = q->data[q->head];
    q->head++;
    return 1;
}

/* 把队列当前内容打出来，让人"看见"队尾到队头。BFS 的教学全靠这个画面 */
static void q_dump(const IntQueue *q)
{
    int n = q->tail - q->head;
    if (n == 0) {
        printf("[ 空 ]");
        return;
    }
    printf("[ ");
    for (int i = q->head; i < q->tail; i++) {
        printf("%d ", q->data[i]);   /* head 在左 = 先出队的那一端 */
    }
    printf("] 共 %d 个", n);
}

/*
 * ===================================================================
 * 1. 造图 / 打印图
 * ===================================================================
 * 邻接矩阵在这里只是"容器"，本节真正的主题是遍历。
 * 为什么不用刚学的邻接表？两个理由：
 *   1) 叶子目录自包含，不想把邻接表那套 malloc/free 再抄一遍；
 *   2) 矩阵按 j = 0..v-1 扫邻居，顺序天生固定 → 输出可复现、能和 README 逐行核对。
 * 代价要诚实记账：矩阵版 BFS 是 O(V²)，邻接表版是 O(V+E)。V=8 无所谓，大图必须换。
 */
void bfs_graph_sample(BfsGraph *g)
{
    for (int i = 0; i < BFS_MAX_V; i++) {
        for (int j = 0; j < BFS_MAX_V; j++) {
            g->edge[i][j] = 0;     /* 栈上的内存是垃圾，必须自己清 0（见邻接矩阵那节） */
        }
    }
    g->v = 8;
    g->e = 0;

    /* 分量一：0-1-2 三角环 + 挂着 3 */
    static const int pairs[][2] = { {0, 1}, {1, 2}, {0, 2}, {2, 3} };
    /* 分量二：4-5-6 三角环 */
    static const int pairs2[][2] = { {4, 5}, {5, 6}, {4, 6} };

    for (int k = 0; k < 4; k++) {
        int a = pairs[k][0], b = pairs[k][1];
        g->edge[a][b] = 1;
        g->edge[b][a] = 1;         /* 无向：对称两格一起写，这是矩阵表示的老规矩 */
        g->e++;
    }
    for (int k = 0; k < 3; k++) {
        int a = pairs2[k][0], b = pairs2[k][1];
        g->edge[a][b] = 1;
        g->edge[b][a] = 1;
        g->e++;
    }
    /* 顶点 7 什么都不加：孤立点，专门用来演示"BFS 从 0 出发碰不到它" */
}

void bfs_graph_print(const BfsGraph *g)
{
    printf("    邻接矩阵（行=起点，列=终点，1 表示有边）：\n");
    printf("        j= ");
    for (int j = 0; j < g->v; j++) {
        printf("%3d", j);
    }
    printf("\n");
    for (int i = 0; i < g->v; i++) {
        printf("    i=%-4d [ ", i);
        for (int j = 0; j < g->v; j++) {
            printf("%3d", g->edge[i][j]);
        }
        printf(" ]   邻居：");
        int any = 0;
        for (int j = 0; j < g->v; j++) {
            if (g->edge[i][j]) {
                printf("%d ", j);
                any = 1;
            }
        }
        if (!any) {
            printf("（孤立点）");
        }
        printf("\n");
    }
}

/*
 * ===================================================================
 * 2. bfs_run：从单个起点做一轮 BFS（本节的核心函数）
 * ===================================================================
 * 干什么：从 start 出发，一圈一圈往外扩，直到队列空。
 *         顺便填三张表：dist[]（最少几步）、parent[]（被谁发现）、order[]（访问顺序）。
 *
 * ------------------------------------------------------------------
 * 算法骨架只有 8 行，背下来：
 * ------------------------------------------------------------------
 *     把 start 入队，并标记已访问，dist[start] = 0
 *     while 队列非空:
 *         u = 出队
 *         把 u 记进访问顺序
 *         for u 的每个邻居 v:            ← 矩阵版：扫第 u 行
 *             if v 没被访问过:
 *                 标记 v 已访问          ← ★ 关键：在入队这一刻就标记，不是出队时
 *                 parent[v] = u          ← 记下"是谁发现了你"，这就是 BFS 树的边
 *                 dist[v] = dist[u] + 1  ← ★ 最短路性质的落点
 *                 v 入队
 *
 * ------------------------------------------------------------------
 * 三个设计点逐个讲清"为什么"
 * ------------------------------------------------------------------
 * ① **入队即标记**（不是出队时标记）
 *    目的是保证"每个顶点最多进队一次"。如果等到出队才标记，
 *    一个点可能同时被它的多个邻居发现，于是被推进队列好几次：
 *      - 逻辑上：order[] 里会出现重复的顶点；
 *      - 队列上：进队次数从 O(V) 变成 O(E)，我们的朴素数组可能溢出；
 *      - 结果上：dist/parent 会被"较晚发现的那条路"覆盖掉，最短性丢掉。
 *    所以"什么时候打标记"不是风格问题，是**正确性问题**。
 *
 * ② **dist[v] = dist[u] + 1 为什么就是最短距离**
 *    因为 v 是在"第一次被发现的瞬间"被赋值的，而那一刻发现它的 u 已经在队列里
 *    排了正确的位置。队列的 FIFO 保证：**入队顺序的 dist 值单调不减**
 *    （只可能等于 dist[u] 或 dist[u]+1 —— 同层或下一层，绝不会倒退）。
 *    于是所有距离 d 的点必然在所有距离 d+1 的点之前处理完，
 *    "第一次到达"必然是"最短到达"。
 *    这一条就是本节菜单注释里那段反证的可执行版本。
 *
 * ③ **parent[v] = u 有什么用处**
 *    它把"遍历过程"变成一棵树：每个点记住"是谁把我发现的"，
 *    起点没有发现者，parent[start] = -1。
 *    有了 parent，就能从任意点**倒着回溯**出起点到它的具体路径
 *    （不只有长度，还有走法）：见 bfs_print_result 里的"回溯路径"那段。
 *    注意：这棵树不唯一 —— 同一个点可能被多个同层邻居发现，
 *    谁先扫到就记谁。所以 parent 树依赖**邻居遍历顺序**，
 *    这也是"固定顺序让结果可复现"在 BFS 里的具体意义。
 *
 * ------------------------------------------------------------------
 * 边界情况
 * ------------------------------------------------------------------
 *  - start 越界：直接返回 0，不越界读内存。
 *  - start 是孤立点（我们的 7 号）：入队、出队、扫一整行全是 0、队列空 →
 *    本轮只访问 1 个点。这不是错误，这就是"连通分量只有一个点"。
 *  - 图不连通：这一轮只能覆盖 start 所在的那个分量，其它点 dist 保持 -1。
 *    → 所以需要 bfs_run_all 补齐。
 *  - 调用者必须传入长度至少 v 的三个数组；本函数按 g->v 写，不越界。
 *  - 三张表都会被本函数**全部重写**（包括填 -1），调用者不用自己预置，
 *    少一个"忘了初始化 parent 结果读到垃圾"的坑。
 */
int bfs_run(const BfsGraph *g, int start, int dist_out[], int parent_out[],
            int order_out[], int *order_len_out, int verbose)
{
    if (g == NULL || dist_out == NULL || parent_out == NULL || order_out == NULL
        || order_len_out == NULL) {
        if (order_len_out != NULL) {
            *order_len_out = 0;
        }
        return 0;
    }
    if (start < 0 || start >= g->v) {
        printf("      [错误] 起点 %d 越界，合法编号 0..%d\n", start, g->v - 1);
        *order_len_out = 0;
        return 0;
    }

    /* visited 单独用一张 bool 数组（用 int 表示 0/1，本仓库约定不引 stdbool 也行，
     * 这里刻意用 int：和 dist/parent 一样是"每点一格"的表，读起来整齐） */
    int visited[BFS_MAX_V];
    for (int i = 0; i < g->v; i++) {
        visited[i] = 0;
        dist_out[i] = -1;       /* -1 的本义是"还没轮到"；本轮结束后仍为 -1 = 到不了 */
        parent_out[i] = -1;     /* -1 = 没有被任何人发现（起点自己也是 -1，含义不同，见下） */
    }
    *order_len_out = 0;

    IntQueue q;
    q_init(&q);

    /* 起点入队 + 立即标记 + 距离 0 */
    visited[start] = 1;
    dist_out[start] = 0;
    parent_out[start] = -1;      /* 约定：起点的 parent 是 -1，表示"没人发现我，我是出发点" */
    q_push(&q, start);

    if (verbose) {
        printf("  【起点 %d 入队】parent=-1（它就是源头），dist=0，队列：", start);
        q_dump(&q);
        printf("\n");
    }

    int visited_cnt = 1;
    int printed_layer = -1;      /* 只用来在打印时插入"第 k 层"的分隔行 */

    while (!q_is_empty(&q)) {
        int u;
        q_pop(&q, &u);

        /* 层边界：出队的点距离变了，说明上一层全部处理完了，新的一层开始 */
        if (verbose && dist_out[u] != printed_layer) {
            printed_layer = dist_out[u];
            printf("\n    ┌── 开始处理第 %d 层（离起点 %d 步的那一圈顶点）──┐\n",
                   printed_layer, printed_layer);
        }

        order_out[*order_len_out] = u;
        (*order_len_out)++;

        if (verbose) {
            printf("    │ 出队 %d（它离起点 %d 步）→ 扫它的邻居（按编号从小到大）：\n",
                   u, dist_out[u]);
        }

        int discovered = 0;      /* 本次出队发现了几个新点，只为了打印好看 */
        for (int w = 0; w < g->v; w++) {
            if (g->edge[u][w] == 0) {
                continue;                    /* 这条格子是 0：不是邻居，跳过 */
            }
            if (visited[w]) {
                if (verbose) {
                    printf("    │     邻居 %d：已经访问过了 → 跳过。"
                           "（它的 dist=%d ≤ 现在的 %d+1，走这条路不可能更短）\n",
                           w, dist_out[w], dist_out[u]);
                }
                continue;
            }
            /* ★ 第一次看到这个点：就地标记 + 记账 + 入队。顺序很重要 */
            visited[w] = 1;
            parent_out[w] = u;
            dist_out[w] = dist_out[u] + 1;
            q_push(&q, w);
            visited_cnt++;
            discovered++;
            if (verbose) {
                printf("    │     邻居 %d：没访问过 → 收进队列！"
                       "parent[%d]=%d，dist[%d]=%d+1=%d\n",
                       w, w, u, w, dist_out[u], dist_out[w]);
            }
        }

        if (verbose) {
            printf("    │     本次新发现 %d 个；已访问 %d 个；队列里排着：",
                   discovered, visited_cnt);
            q_dump(&q);
            printf("\n    └────────────────────────────────────────\n");
        }
    }

    if (verbose) {
        /* 把"这一轮没碰到的点"现场算出来，而不是硬写一串数字 ——
         * 硬写的注释会随起点变化而说谎，算出来的不会。 */
        printf("\n  【队列空了】本轮 BFS 结束。这一轮**没碰到**的顶点：");
        int miss = 0;
        for (int u = 0; u < g->v; u++) {
            if (!visited[u]) {
                printf("%d ", u);
                miss++;
            }
        }
        if (miss == 0) {
            printf("（无 → 从 %d 出发能到全图所有点，这张图是连通的）\n", start);
        } else {
            printf("\n   从 %d 出发**走不到**这 %d 个点 —— 它们属于别的连通分量。\n",
                   start, miss);
        }
    }
    return visited_cnt;
}

/*
 * ===================================================================
 * 3. bfs_run_all：把"一个图可能有几个连通分量"补齐
 * ===================================================================
 * 干什么：外层依次看每个顶点，只要它还没被访问过，就以它为起点再起一轮 BFS。
 *
 * 为什么必须要这个"外层循环"？—— 这是本节最重要的认知升级：
 *   单次 bfs_run 只覆盖 start 所在的**连通分量**。
 *   上一轮从 0 出发，只碰得到 {0,1,2,3}；如果你以为"跑完一次 BFS 就遍历了全图"，
 *   那么 4、5、6、7 被你整片漏掉了，而且**程序不会报错**：
 *   队列正常清空、返回值正常、看着一切健康。
 *   这类 bug 的特征是"结果少了一坨但毫无异常"，最难发现。
 *
 *   所以凡是"要处理整张图"的问题（连通分量、判环、拓扑排序、最小生成树、
 *   求最短路径长度分布……），骨架都必须是这两层：
 *       for 每个顶点 u:
 *           if u 没被访问:
 *               从 u 起一次遍历            ← 这一轮覆盖一个新的连通分量
 *   记住这个形状，它和"排序要写两层循环"一样基础。
 *
 * 外层循环从 0 扫到 v-1，所以：
 *   - 每一轮的起点都是"编号最小的、还没被任何分量收走的点"
 *   - 起了几轮 == 有几个连通分量（函数返回值就是这个）
 *   - visited 表跨轮**不清空**，靠它防止重复覆盖：这是"外层循环"能正确的关键。
 *
 * ⚠ dist[] 在这里的含义变了：它不再是"到全局起点的距离"，
 *   而是"到**本轮那个起点**的距离"（每轮自己的起点 dist=0）。
 *   不连通的点之间距离本来就是"无穷"，所以分轮各自记 0 起点是诚实的做法；
 *   真正要跨分量算距离，得先按分量拆开分别处理（dijkstra 那节会更明确地讲这事）。
 */
int bfs_run_all(const BfsGraph *g, int dist_out[], int parent_out[],
                int order_out[], int *order_len_out, int verbose)
{
    if (g == NULL || dist_out == NULL || parent_out == NULL || order_out == NULL
        || order_len_out == NULL) {
        if (order_len_out != NULL) {
            *order_len_out = 0;
        }
        return 0;
    }

    /* 先整体标记为未访问：下面要跨轮共享这张 visited 表 */
    int visited[BFS_MAX_V];
    for (int i = 0; i < g->v; i++) {
        visited[i] = 0;
    }

    int round = 0;
    int total = 0;               /* order_out[] 的写入位置，跨轮累加 */

    for (int s = 0; s < g->v; s++) {
        if (visited[s]) {
            continue;            /* 已经被某一轮收走了：跳过，这就是"不重复起 BFS"的关键 */
        }
        round++;

        /*
         * 这里没有直接复用 bfs_run，而是把它的循环体内联了一遍，
         * 原因是 visited / dist / parent 三张表要**跨轮保留**：
         *   bfs_run 每次进来都会把三张表重置，会抹掉之前几轮的结果。
         * 教学代码宁可有三十行相似逻辑，也不要为了"复用"把语义绕弯。
         *   （工程上正确做法是把 bfs_run 拆成"接受一张外部 visited 表"的形式，
         *     或者让每轮只返回自己那批点，再由调用者合并。这里先讲清代价。）
         */
        if (verbose) {
            printf("\n  ═══ 第 %d 轮 BFS：顶点 %d 还没被访问过，以它为起点 ═══\n", round, s);
        }

        IntQueue q;
        q_init(&q);
        visited[s] = 1;
        dist_out[s] = 0;
        parent_out[s] = -1;
        q_push(&q, s);

        int reached = 0;         /* 本轮访问到的点数（含起点） */
        int printed_layer = -1;

        while (!q_is_empty(&q)) {
            int u;
            q_pop(&q, &u);
            reached++;
            order_out[total] = u;
            total++;

            if (verbose && dist_out[u] != printed_layer) {
                printed_layer = dist_out[u];
                printf("    ┌── 第 %d 层（离本轮起点 %d 步）──┐\n", printed_layer, printed_layer);
            }
            if (verbose) {
                printf("    │ 出队 %d（dist=%d）：", u, dist_out[u]);
            }

            for (int w = 0; w < g->v; w++) {
                if (g->edge[u][w] == 0) {
                    continue;
                }
                if (visited[w]) {
                    if (verbose) {
                        printf("%d(已访问,跳过) ", w);
                    }
                    continue;
                }
                visited[w] = 1;
                parent_out[w] = u;
                dist_out[w] = dist_out[u] + 1;
                q_push(&q, w);
                if (verbose) {
                    printf("%d(新发现 parent=%d dist=%d) ", w, u, dist_out[w]);
                }
            }
            if (verbose) {
                if (q_is_empty(&q)) {
                    printf("→ 没有新邻居");
                }
                printf("\n    │ 队列：");
                q_dump(&q);
                printf("\n");
            }
        }

        if (verbose) {
            printf("    本轮覆盖 %d 个顶点，结束。\n", reached);
        }
    }

    *order_len_out = total;
    return round;
}

/*
 * ===================================================================
 * 4. bfs_print_result：把 dist / parent / order 三张表摊开
 * ===================================================================
 * 干什么：打印本轮的访问顺序、每个点的距离和发现者，并给一个可达性小结。
 *
 * 为什么单独做一个函数：BFS 的"过程"和"结果"要分开看才不会乱。
 * 过程 = verbose 那段一步步演的；结果 = 这三张表。
 *
 * 特别注意打印里那句"可达 X 个 / 共 V 个"：这一行就是"图不连通"的量化证据。
 * 从 0 出发是 4/8 —— 少的那 4 个点不是 bug，是图的结构。
 */
void bfs_print_result(const BfsGraph *g, int start, const int dist_out[],
                      const int parent_out[], const int order_out[], int order_len)
{
    if (g == NULL) {
        return;
    }
    printf("  访问顺序（共 %d 个）：", order_len);
    for (int i = 0; i < order_len; i++) {
        printf("%d%s", order_out[i], (i + 1 == order_len) ? "\n" : " -> ");
    }

    printf("  顶点    dist   parent   可达性        （dist/parent 都是 -1 表示这轮没走到）\n");
    printf("  ----    ----   ------   --------\n");
    int reachable = 0;
    for (int u = 0; u < g->v; u++) {
        if (dist_out[u] < 0) {
            printf("  %-5d  %-4d  %-6d  到不了 X\n", u, dist_out[u], parent_out[u]);
        } else {
            reachable++;
            printf("  %-5d  %-4d  %-6d  可达 OK%s\n", u, dist_out[u], parent_out[u],
                   (u == start) ? "   <- 本轮起点（parent=-1 表示没人发现它）" : "");
        }
    }
    printf("  从 %d 出发可达 %d / %d 个顶点 → %s\n",
           start, reachable, g->v,
           (reachable == g->v) ? "这张图是连通的"
                               : "图不连通！剩下的点在别的连通分量里，必须另起 BFS");

    /*
     * 有了 parent[] 就不只知道"几步到"，还能回溯出"具体怎么走"：
     *     从目标点开始，一路 parent[parent[...]] 回到起点，再倒过来打印。
     * 这就是" BFS 生成一棵树"的实际用途 —— 最短路树。
     * ⚠ 倒着走要用一个临时数组存路径再反向打印，因为链表能正着走不能倒着走；
     *   这里我们用的是"数组当下标栈"的老技巧（第 03 章用数组模拟栈的同一个思路）。
     */
    printf("  回溯 parent 得到具体最短路径：\n");
    for (int u = 0; u < g->v; u++) {
        if (u == start || dist_out[u] < 0) {
            continue;
        }
        int path[BFS_MAX_V];
        int len = 0;
        int cur = u;
        while (cur >= 0 && len < BFS_MAX_V) {
            path[len++] = cur;      /* 先把 u、parent[u]、parent[parent[u]]… 收到数组里 */
            cur = parent_out[cur];
        }
        printf("    %d → %d（%d 步）：", start, u, dist_out[u]);
        for (int i = len - 1; i >= 0; i--) {   /* 再倒序打出来，就是起点到目标的方向 */
            printf("%d%s", path[i], (i == 0) ? "\n" : " → ");
        }
    }
    if (reachable == 1) {
        printf("    （起点自己是唯一可达点，没有路径可回溯 —— 这就是孤立点）\n");
    }
}
