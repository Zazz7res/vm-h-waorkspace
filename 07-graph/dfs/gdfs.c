#include <stdio.h>
#include "gdfs.h"

/*
 * ===================================================================
 * 0. 就地重写的两种栈：int 栈 + 帧栈（第 03 章的复用，极简版）
 * ===================================================================
 * 叶子目录必须自包含，所以不能 #include "../../03-stack/.../stack.h"，
 * 这里自己写够用的。
 *
 * 【int 栈】只存顶点号 —— 朴素版 DFS 用它
 *     ┌───┬───┬───┬───┬───┐
 *     │ 0 │ 2 │ 1 │   │   │            top = 3（下一个要放的位置）
 *     └───┴───┴───┴───┴───┘
 *       0   1   2   3   4      ← 数组下标
 *     栈顶就是 data[top-1] = 1。**后进的那个最先被弹出来**，这就是"栈"。
 *
 * 【帧栈】存（顶点，下次该从哪个邻居接着看） —— 帧模拟版 DFS 用它
 *     ┌──────────┬──────────┬──────────┐
 *     │ v=0      │ v=1      │ v=2      │   ← 栈顶在右
 *     │ next=2   │ next=3   │ next=0   │
 *     └──────────┴──────────┴──────────┘
 *     光存顶点不够！递归的每一层都记着"我的 for 循环走到第几个邻居了"，
 *     手写栈必须把这件事也存下来，否则回溯回来会从头再扫一遍邻居
 *     （逻辑上不算错，但复杂度会从 O(V+E) 退化成 O(V²·E)，而且过程画面也和
 *      递归对不上了）。这一对 (v, next) 就是一个"帧"，next 就是循环游标。
 *
 * ★ 栈的容量怎么定？（和第 04 章"队列多大才够"是同一类问题）
 *   帧栈：栈里最多同时躺着"从根到当前点的一条简单路径"，长度 ≤ V → 开 V+1 格。
 *   int 栈：如果做到"入栈即标记"，一个点最多进栈一次 → 也只要 V+1 格。
 *           但如果故意试一下"出栈时才标记"（dfs_stack_naive 的第二个开关），
 *           同一个点会被它的多个邻居分别压进来，压栈总次数最坏 = 1 + Σdeg = 1 + 2E。
 *           完全图 E = V(V-1)/2 → 1 + 2E = V(V-1) + 1 < V²。
 *           所以下面 GDFS_STACK_CAP 直接开 V² = 64 格，够装最坏情况。
 *           这就是本节给"内存预算"这件事交的作业：**先证上界，再定数组大小。**
 */
#define GDFS_STACK_CAP (GDFS_MAX_V * GDFS_MAX_V)

typedef struct {
    int data[GDFS_STACK_CAP];
    int top;
} IntStack;

static void s_init(IntStack *s)
{
    s->top = 0;    /* 空栈：top == 0。data 不用清，没写过的格子我们不会去读 */
}

static int s_is_empty(const IntStack *s)
{
    return s->top == 0;
}

/* 进栈：放到 data[top]，top 前进一格。返回 0 表示真满了（说明上界算错了） */
static int s_push(IntStack *s, int x)
{
    if (s->top >= GDFS_STACK_CAP) {
        return 0;
    }
    s->data[s->top] = x;
    s->top++;
    return 1;
}

/* 出栈：取 data[top-1]，top 退一格。空栈返回 0，*out 保持原样 */
static int s_pop(IntStack *s, int *out)
{
    if (s_is_empty(s)) {
        return 0;
    }
    s->top--;
    *out = s->data[s->top];
    return 1;
}

/* 把栈内容打出来，栈顶在右边（因为栈顶最后进、最先出） */
static void s_dump(const IntStack *s)
{
    if (s->top == 0) {
        printf("[ 空 ]");
        return;
    }
    printf("[ ");
    for (int i = 0; i < s->top; i++) {
        printf("%d ", s->data[i]);
    }
    printf("] 顶=%d 共 %d 个", s->data[s->top - 1], s->top);
}

/* 帧 = 顶点 + 邻居游标 */
typedef struct {
    int v;       /* 这一层站在哪个顶点上 */
    int next;    /* 下一个要检查的列号（相当于递归里 for 循环的 j） */
} Frame;

typedef struct {
    Frame data[GDFS_MAX_V + 1];
    int top;
} FrameStack;

static void fs_init(FrameStack *fs)
{
    fs->top = 0;
}

static int fs_is_empty(const FrameStack *fs)
{
    return fs->top == 0;
}

static int fs_push(FrameStack *fs, int v, int next)
{
    if (fs->top >= GDFS_MAX_V + 1) {
        return 0;    /* 深度超过 V 是不可能的（每条帧对应一个未访问过的点） */
    }
    fs->data[fs->top].v = v;
    fs->data[fs->top].next = next;
    fs->top++;
    return 1;
}

/* 帧栈的"弹"不需要返回值：调用方先用 fs_top 看过内容，再决定弹 */
static void fs_pop(FrameStack *fs)
{
    fs->top--;       /* 只退一格：data[top] 那个帧就此作废，跟递归 return 销毁栈帧同理 */
}

static Frame fs_top(const FrameStack *fs)
{
    return fs->data[fs->top - 1];   /* 按值返回一份拷贝，调用方改它不会影响栈 */
}

/* 帧栈画面：[顶点|看到第几列] 从栈底排到栈顶 */
static void fs_dump(const FrameStack *fs)
{
    if (fs->top == 0) {
        printf("[ 空 ]");
        return;
    }
    printf("[ ");
    for (int i = 0; i < fs->top; i++) {
        printf("%d|j=%d ", fs->data[i].v, fs->data[i].next);
    }
    printf("] 共 %d 帧", fs->top);
}

/*
 * ===================================================================
 * 1. 造图 / 打印图
 * ===================================================================
 * 邻接矩阵在这里只是"容器"，本节真正的主角是遍历。
 * 为什么不用邻接表？① 叶子目录自包含，不想把 malloc/free 再抄一遍；
 * ② 矩阵第 u 行按列号升序扫，邻居顺序天生固定 → 输出可复现、能和 README 逐行核对。
 * 代价：矩阵版 DFS 是 O(V²)，邻接表版是 O(V+E)。V=8 无所谓，大图必须换。
 */
void dfs_graph_sample(DfsGraph *g)
{
    for (int i = 0; i < GDFS_MAX_V; i++) {
        for (int j = 0; j < GDFS_MAX_V; j++) {
            g->edge[i][j] = 0;   /* 栈上的内存是垃圾，必须自己清 0 */
        }
    }
    g->v = 8;
    g->e = 0;

    /* 分量一：0-1-2 三角环 + 挂着 3 */
    static const int tri[][2] = { {0, 1}, {1, 2}, {0, 2}, {2, 3} };
    /* 分量二：4-5-6 三角环 */
    static const int tri2[][2] = { {4, 5}, {5, 6}, {4, 6} };

    for (int k = 0; k < 4; k++) {
        int a = tri[k][0], b = tri[k][1];
        g->edge[a][b] = 1;
        g->edge[b][a] = 1;       /* 无向：对称两格一起涂 */
        g->e++;
    }
    for (int k = 0; k < 3; k++) {
        int a = tri2[k][0], b = tri2[k][1];
        g->edge[a][b] = 1;
        g->edge[b][a] = 1;
        g->e++;
    }
    /* 顶点 7 什么都不加：孤立点，用来演示"一轮 DFS 只覆盖一个连通分量" */
}

void dfs_graph_print(const DfsGraph *g)
{
    printf("    邻接矩阵（行=起点，列=终点，1 表示有边）：\n");
    printf("        j= ");
    for (int j = 0; j < g->v; j++) {
        printf("%3d", j);
    }
    printf("\n");
    for (int i = 0; i < g->v; i++) {
        printf("    i=%-3d [ ", i);
        for (int j = 0; j < g->v; j++) {
            printf("%3d", g->edge[i][j]);
        }
        printf(" ]  邻居按固定顺序：");
        int any = 0;
        for (int j = 0; j < g->v; j++) {
            if (g->edge[i][j] && j != i) {
                printf("%d ", j);
                any = 1;
            }
        }
        if (!any) {
            printf("（孤立点，一个邻居都没有）");
        }
        printf("\n");
    }
}

/*
 * ===================================================================
 * 2. 往 DfsResult 里记账的三个小工具
 * ===================================================================
 * 为什么要单独抽出来？因为本节有**四种**跑法（递归、帧模拟、朴素栈、全图），
 * 如果每种各自手写记账代码，四份就会长出四个细微不同 —— 而我们要拿它们互相核对。
 * 统一走这三个函数，等于给四种实现装了同一块"计价表"，比较结果才有意义。
 *
 * 边界情况：
 *   - res_reset 会把按顶点索引的数组全填 -1。为什么不用 0？
 *     因为 0 是合法值：起点深度就是 0，起点前序位置就是第 0 个。
 *     只有 -1 是"绝不可能出现的记号"，才配当"没访问到"。
 *   - res_enter 里顺手把顶点压上 trail（当前路径），res_leave 里弹掉。
 *     trail 是"递归此刻站在哪条路上"的画面，也是帧模拟版的对照物。
 */
static void res_reset(DfsResult *r)
{
    r->order_len = 0;
    r->post_len = 0;
    r->trail_len = 0;
    r->clock = 0;
    r->dup_pops = 0;
    for (int i = 0; i < GDFS_MAX_V; i++) {
        r->order[i] = -1;
        r->post[i] = -1;
        r->trail[i] = -1;
        r->parent[i] = -1;
        r->depth[i] = -1;
        r->tick_in[i] = -1;
        r->tick_out[i] = -1;
    }
}

/* 进入顶点 u：记前序、父亲、深度、进场时间戳，并把它挂到"当前路径"末端 */
static void res_enter(DfsResult *r, int u, int par, int depth)
{
    r->order[r->order_len] = u;
    r->order_len++;                      /* 前序：第 order_len-1 个就是 u */
    r->parent[u] = par;
    r->depth[u] = depth;
    r->clock++;
    r->tick_in[u] = r->clock;            /* 进场刻度 */
    r->trail[r->trail_len] = u;
    r->trail_len++;
}

/* 离开顶点 u：记后序、出场时间戳，并从"当前路径"上把自己擦掉（= 回溯） */
static void res_leave(DfsResult *r, int u)
{
    r->post[r->post_len] = u;
    r->post_len++;
    r->clock++;
    r->tick_out[u] = r->clock;
    r->trail_len--;                      /* u 一定是 trail 的最后一个，直接缩掉 */
}

/* 缩进：一帧两个空格。这个画面是整节的核心教具 */
static void indent(int n)
{
    for (int i = 0; i < n; i++) {
        printf("  ");
    }
}

/* 把"当前路径"打成 0 → 1 → 2，让人看见一条钻到底的路 */
static void print_trail(const DfsResult *r)
{
    printf("当前路径（栈里从根到我）：");
    for (int i = 0; i < r->trail_len; i++) {
        if (i > 0) {
            printf(" -> ");
        }
        printf("%d", r->trail[i]);
    }
}

/*
 * ===================================================================
 * 3. dfs_go + dfs_recursive：递归版（本节主线）
 * ===================================================================
 * 骨架三行，背下来：
 *
 *     dfs_go(u):
 *         标记 u 已访问                      ← 进函数第一件事，防环命门
 *         for u 的每个邻居 v（固定升序）:     ← 这一句 j 停在哪儿，全靠栈帧记住
 *             if v 没访问过: dfs_go(v)
 *         （函数自然返回 = 回溯）
 *
 * ------------------------------------------------------------------
 * 为什么"标记"必须写在进函数的第一行
 * ------------------------------------------------------------------
 * 图里有环。0-1-2-0 这一圈，如果 1 不标记就往下钻，2 会看到 0 没访问过，
 * 于是 0 又钻回 1、1 又钻回 2……无限递归 → 栈溢出 → 程序当场没了
 * （不是死循环，是段错误，比死循环更难看懂）。
 *
 * ------------------------------------------------------------------
 * 为什么每一层的 for 循环"接着上次停下的地方走"
 * ------------------------------------------------------------------
 * 看 dfs_go(0)：它先看邻居 1，一头扎进去；等 1 那一整串全部返回，
 * 才轮回到它的下一个邻居 2。而"1 那一串"里 2 早被访问过了，
 * 所以回到 0 时再看到 2 只会跳一下。
 * 这个"回来之后还记得 j=2"的能力，就是**栈帧**给的：
 * j 是 dfs_go(0) 的局部变量，dfs_go(1) 执行的那一大段时间里，
 * 它就静静躺在调用栈里更靠下的那一格，谁也不会去动它。
 * 帧模拟版（第 4 节）就是把这件事明写在 Frame.next 里。
 *
 * ------------------------------------------------------------------
 * 顺带出现的"后向边"
 * ------------------------------------------------------------------
 * 无向图 DFS 里"碰到一个已访问的邻居，而且它不是我父亲" ⇒ 这个邻居和当前点
 * 构成了一个环。第 2 幕会看到好几条这样的边。本节只是**让它可见**，
 * 真正拿它做判环在 `07-graph/connected-components/`。
 */
static void dfs_go(const DfsGraph *g, int u, int par, int depth,
                   int visited[], DfsResult *r, int verbose)
{
    visited[u] = 1;                      /* ★ 第一件事：打标记。晚了就可能无限递归 */
    res_enter(r, u, par, depth);

    if (verbose) {
        indent(depth);
        printf("┌ 进入 u=%d  深度 %d  父亲 %-2d  前序第 %d 个\n",
               u, r->depth[u], r->parent[u], r->order_len);
        indent(depth);
        printf("│   ");
        print_trail(r);
        printf("\n");
    }

    for (int j = 0; j < g->v; j++) {     /* 固定升序：邻居顺序全节统一，结果才可复现 */
        if (!g->edge[u][j] || j == u) {
            continue;                     /* 没边 / 自环（示例图没有自环）都跳过 */
        }
        if (visited[j]) {
            if (verbose) {
                indent(depth + 1);
                if (j == par) {
                    printf("├ 邻居 %d：已访问，但它就是我父亲 → 正常，这只是我回来的那条路\n", j);
                } else {
                    printf("├ 邻居 %d：已访问，又不是我父亲 → 『后向边』，这里有环（下一节专讲）\n", j);
                }
            }
            continue;
        }
        if (verbose) {
            indent(depth + 1);
            printf("├ 邻居 %d：没来过 → 钻进去！\n", j);
        }
        dfs_go(g, j, u, depth + 1, visited, r, verbose);
        /* 递归返回后，for 循环从 j 的下一格继续 —— 这件事是系统白送的 */
    }

    if (verbose) {
        indent(depth);
        printf("│   （%d 的邻居全看完了）\n", u);
    }
    res_leave(r, u);                      /* 记后序 + 出场时间戳 + 从当前路径上弹掉 */

    if (verbose) {
        indent(depth);
        printf("└ 回溯：离开 u=%d  后序第 %d 个  出场刻度 %d",
               u, r->post_len, r->tick_out[u]);
        if (par >= 0) {
            printf("  → 回到父亲 %d，接着看它的下一个邻居\n", par);
        } else {
            printf("  → 我是这一轮的根，栈空了，这一轮到此结束\n");
        }
    }
}

int dfs_recursive(const DfsGraph *g, int start, DfsResult *r, int verbose)
{
    res_reset(r);

    int visited[GDFS_MAX_V];
    for (int i = 0; i < GDFS_MAX_V; i++) {
        visited[i] = 0;
    }

    if (verbose) {
        printf("  【递归版】从 %d 出发。缩进 = 系统调用栈的深度 = 钻得有多深\n\n", start);
    }
    dfs_go(g, start, -1, 0, visited, r, verbose);
    return r->order_len;
}

/*
 * ===================================================================
 * 4. dfs_stack_frames：显式栈"帧模拟"版 —— 把系统藏起来的栈摆到桌面上
 * ===================================================================
 * 目的只有一个：证明**递归版干的每件事都能自己手写出来，而且结果分毫不差**。
 *
 * 和递归版逐项对齐的翻译表（这张表就是本节要教会你的东西）：
 *
 *     递归里的东西                        手写版里的东西
 *     ----------------------------------  --------------------------------
 *     调用栈上的一个栈帧                  Frame{v, next}
 *     帧里的局部变量 u                    Frame.v
 *     帧里的 for 循环游标 j               Frame.next  ★ 关键：不存它就模仿不像
 *     函数调用 dfs_go(v)                  fs_push(v, 0)
 *     函数 return（回溯）                 fs_pop()
 *     "栈顶帧正在执行"                    while 循环这一轮取的 fs_top()
 *     栈空 = 最外层返回、函数结束          fs_is_empty() → 退出 while
 *
 * 循环体就三件事：
 *     ① 看栈顶帧：它站在 u 上，游标停在 next；
 *     ② 从 next 往后找第一个没访问过的邻居 v：
 *          找到 → 游标推进到 v 之后（**先记下游标再压栈**，否则回来会重复看 v），
 *                  然后把 v 压成新栈顶 → 下一轮循环自然就在 v 上继续，等于"钻进去"；
 *     ③ 找不到 → u 办完了 → 弹栈（= return），记后序。
 *
 * 边界情况：
 *   - 弹栈之后**必须回到 while 开头重新看新栈顶**，不能顺手也把它的邻居扫完，
 *     否则就成了"一次性处理多层"，深度优先的味道就散了。
 *   - 栈深最坏 = V（链状图），所以开 GDFS_MAX_V + 1 格够。
 *   - 复杂度：每个帧的游标只往前走，总共最多走 v 步 ⇒ 每个点扫一行 ⇒ O(V²)，
 *     和矩阵表示配套。邻接表版才是 O(V+E)。
 */
int dfs_stack_frames(const DfsGraph *g, int start, DfsResult *r, int verbose)
{
    res_reset(r);

    int visited[GDFS_MAX_V];
    for (int i = 0; i < GDFS_MAX_V; i++) {
        visited[i] = 0;
    }

    FrameStack fs;
    fs_init(&fs);

    visited[start] = 1;
    res_enter(r, start, -1, 0);
    fs_push(&fs, start, 0);
    if (verbose) {
        printf("  起点入栈：压帧 (%d, j=0)   ", start);
        fs_dump(&fs);
        printf("\n");
        printf("  ┌ 进入 u=%d  深度 0  父亲 -1  前序第 %d 个\n", start, r->order_len);
        printf("  │   ");
        print_trail(r);
        printf("\n");
        printf("  │\n");
    }

    while (!fs_is_empty(&fs)) {
        Frame cur = fs_top(&fs);          /* 拷贝一份：下面要改栈，别拿指针 */
        int u = cur.v;
        int next = cur.next;
        int depth = fs.top - 1;           /* 栈里已有几帧 = 当前这个点在第几层 */
        int par = r->parent[u];
        int found = -1;

        /* 从游标处往后找第一个没访问过的邻居；沿途把已访问的邻居"解说"掉 */
        while (next < g->v) {
            int j = next;
            next++;                       /* ★ 游标先前进：这一格已经处理过了 */
            if (!g->edge[u][j] || j == u) {
                continue;
            }
            if (visited[j]) {
                if (verbose) {
                    indent(depth + 1);
                    if (j == par) {
                        printf("├ u=%d 的邻居 %d：已访问，是我父亲 → 跳过\n", u, j);
                    } else {
                        printf("├ u=%d 的邻居 %d：已访问，不是我父亲 → 后向边，跳过\n", u, j);
                    }
                }
                continue;
            }
            found = j;
            break;
        }

        /* 把推进后的游标写回栈顶 —— 这一行就是"递归帮你记住 for 走到哪"的手写版 */
        fs.data[fs.top - 1].next = next;

        if (found < 0) {
            /* u 的邻居全看完了 = 递归里那个 for 自然结束 = 函数 return */
            fs_pop(&fs);
            res_leave(r, u);
            if (verbose) {
                indent(depth);
                printf("└ 弹栈 + 回溯：离开 u=%d  后序第 %d 个  出场刻度 %d",
                       u, r->post_len, r->tick_out[u]);
                if (par >= 0) {
                    printf("  → 回到父亲 %d\n", par);
                } else {
                    printf("  → 栈将空，这一轮结束\n");
                }
                indent(depth > 0 ? depth - 1 : 0);
                printf("    此刻帧栈：");
                fs_dump(&fs);
                printf("\n\n");
            }
            continue;
        }

        /* 发现新邻居：压一帧下去 = 递归里那一次函数调用 */
        visited[found] = 1;
        res_enter(r, found, u, depth + 1);        /* 注意：深度 = 弹之前的栈里帧数 */
        fs_push(&fs, found, 0);
        if (verbose) {
            indent(depth + 1);
            printf("├ u=%d 的邻居 %d：没来过 → 压帧 (%d, j=0)  %s\n", u, found, found, "（等于递归调用 dfs_go(新点)）");
            indent(depth + 1);
            printf("┌ 进入 u=%d  深度 %d  父亲 %-2d  前序第 %d 个\n",
                   found, r->depth[found], r->parent[found], r->order_len);
            indent(depth + 1);
            printf("│   ");
            print_trail(r);
            printf("\n");
            indent(depth + 1);
            printf("│   此刻帧栈：");
            fs_dump(&fs);
            printf("\n");
        }
    }

    if (verbose) {
        printf("  帧栈空了 → 整轮结束（这就是递归最外层 return 之后）\n");
    }
    return r->order_len;
}

/*
 * ===================================================================
 * 5. dfs_stack_naive：教科书最短写法，以及它为什么会和递归版有细微差别
 * ===================================================================
 * 代码最短的 DFS 长这样（8 行）：
 *
 *     start 入栈
 *     while 栈非空:
 *         u = 出栈
 *         if u 已经处理过了: continue            ← ★ 少了这一行就会重复处理
 *         记 u 的前序
 *         for u 的每个邻居 v:  v 入栈             ← ★ 一发现就"一压全压"
 *
 * ⚠ 这里有个非常容易被写错的点，本节代码是**踩过之后专门标出来的**：
 *   "处理过了没有" 和 "进过栈了没有" 是**两件事**，不能共用同一个数组。
 *   很多人写成：入栈时 visited[start]=1，出栈时 `if (visited[u]) continue;`
 *   —— 结果起点自己一出场就被判成"已处理"，整轮 DFS 从起点就开始漏，
 *   而漏的偏偏是根，症状是"前序少一个、parent 全乱"，很难想到是这一行。
 *   正确做法：**出栈时的去重看『我记没记过前序』**（代码里用 tick_in[u] >= 0 判断），
 *   **入栈时的去重才看『它进过栈没』**（代码里叫 marked[]）。两个检查管两头。
 *
 * 它和递归版差在两个地方，而且这两处都来自同一个根因：
 * **它只存了顶点，没存"这个顶点的邻居扫到第几个了"。**
 *
 * 差别①：邻居的处理顺序会反过来。
 *   递归是"升序看到谁就先钻谁"；一压全压之后，最后压的那个才在栈顶，
 *   于是最先被处理的是**最后压的**。所以：
 *       push_ascending=1（按升序压）→ 处理顺序变成降序 → 前序和递归版不同
 *       push_ascending=0（按降序压）→ 处理顺序变回升序 → 前序**在这类小图上**与递归版相同
 *   （"恰好相同"不是普遍保证，只是这一张图上如此；本目录的输出里能直接看出来。）
 *
 * 差别②：DFS 树（parent/depth）可能不同，而且这一条更重要。
 *   递归版里"是谁发现了我"发生在**我进栈那一刻**：0 先看 1，1 一路钻到 2，
 *   所以 parent[2] = 1。
 *   朴素版里 0 一口气把 1 和 2 **都**压了，两者都是 0 压的 → parent[2] = 0。
 *   结果：前序一样，树却从"一条深链"塌成"一层矮胖"。
 *   原因一句话：**朴素版"一压全压 + 提前标记"，把发现权判给了第一个把它压进栈的人，
 *   而递归版判给"顺着哪条路真的走进来的人"。**
 *   （如果把标记推迟到出栈时，还会更乱：同一个点被多个邻居重复压栈，
 *    谁最后压的 parent 就归谁 —— 下面 mark_on_pop=1 就是这个演示。）
 *
 * 差别③：朴素版**给不出后序**。
 *   它没有"离开 u"这个时刻（弹出即结束），所以 post[] 是空的，tick_out[] 全 -1。
 *   需要后序的场合（拓扑排序、算子树、判环的细节）必须用递归版或帧模拟版。
 *
 * 参数说明（两个开关正交，组合出 4 种跑法，main.c 里各演一遍）：
 *   push_ascending：非 0 → 按 j 升序压栈；0 → 按 j 降序压栈。
 *   mark_on_pop   ：非 0 → 出栈时才打标记（会重复压栈，看 dup_pops）；
 *                   0 → 入栈时就打标记（每个点最多进栈一次，栈里不会重复）。
 *
 * 边界情况：
 *   - mark_on_pop=1 时，压栈总次数 ≤ 1 + 2E，所以才需要 GDFS_STACK_CAP = V² 的容量。
 *   - 出栈时那句"已访问就跳过"两种模式下都必须留着：mark_on_pop=1 时它负责去重，
 *     mark_on_pop=0 时它是防御性检查（正常情况下一次都触发不了，dup_pops 会是 0）。
 */
int dfs_stack_naive(const DfsGraph *g, int start, int push_ascending, int mark_on_pop,
                    DfsResult *r, int verbose)
{
    res_reset(r);

    IntStack st;
    s_init(&st);

    /* marked[] 的含义随模式微调：
       mark_on_pop=0（入栈即标记）→ marked[x]==1 表示"x 已经进过栈了"，压栈前的去重靠它；
       mark_on_pop=1（出栈才标记）→ marked[x]==1 表示"x 已经处理完了"，
                                    还在栈里排队的那个 x 不算处理完 → 所以会被重复压栈。
       而"这个点我到底处理过没有"统一由 r->tick_in[u] 判断（-1 = 还没记过前序）。 */
    int marked[GDFS_MAX_V];
    for (int i = 0; i < GDFS_MAX_V; i++) {
        marked[i] = 0;
    }

    if (verbose) {
        printf("  【朴素栈版】压栈方向：%s   打标记时机：%s\n\n",
               push_ascending ? "升序（于是处理顺序变降序）" : "降序（于是处理顺序变升序）",
               mark_on_pop ? "出栈时（危险演示，会重复压栈）" : "入栈时（推荐）");
    }

    /* 起点入栈。parent/depth 一定要在这里就给好，
       否则第一层孩子的 depth = depth[起点]+1 会算成 0（-1+1）。 */
    r->parent[start] = -1;
    r->depth[start] = 0;
    s_push(&st, start);
    if (!mark_on_pop) {
        marked[start] = 1;                 /* 入栈即标记：以后谁也别想再压它第二次 */
    }
    if (verbose) {
        printf("  起点 %d 入栈   ", start);
        s_dump(&st);
        printf("\n");
    }

    while (!s_is_empty(&st)) {
        int u;
        s_pop(&st, &u);

        /* ★ 出栈去重看"记过前序没有"，不是看 marked —— 见上面那段警告 */
        if (r->tick_in[u] >= 0) {
            r->dup_pops++;
            if (verbose) {
                printf("  弹出 %d → 前序已经记过它了（刻度 %d）→ 跳过。重复出栈第 %d 次   栈剩：",
                       u, r->tick_in[u], r->dup_pops);
                s_dump(&st);
                printf("\n");
            }
            continue;
        }

        /* 真正"处理" u：记前序 + 进场时间戳 */
        if (mark_on_pop) {
            marked[u] = 1;                 /* 这一模式到此才打标；栈里排队的同伴看不到 */
        }
        r->order[r->order_len] = u;
        r->order_len++;
        r->clock++;
        r->tick_in[u] = r->clock;          /* tick_out[u] 保持 -1：这一版没有"离开"时刻 */
        if (verbose) {
            printf("  弹出 %d → 处理！前序第 %d 个  parent=%-2d depth=%d   ",
                   u, r->order_len, r->parent[u], r->depth[u]);
            s_dump(&st);
            printf("\n");
        }

        /* 把 u 的邻居压栈。两种模式的区别只在"什么时候打标记、发现者判给谁" */
        int pushed = 0;
        for (int step = 0; step < g->v; step++) {
            /* push_ascending=1 → j = 0,1,2,...；=0 → j = v-1,...,1,0。
               为什么要控制压栈方向？栈是后进先出，压栈顺序会把处理顺序**反过来**。 */
            int j = push_ascending ? step : (g->v - 1 - step);
            if (!g->edge[u][j] || j == u) {
                continue;
            }
            if (marked[j]) {
                /* 入栈即标记模式：进过栈的不再压；
                   出栈才标记模式：只拦得住"已经处理完"的，拦不住"还在栈里排队"的 */
                continue;
            }
            r->parent[j] = u;              /* 发现者判给"压它的人"：mark_on_pop 下会被后来者覆盖 */
            r->depth[j] = r->depth[u] + 1;
            if (!mark_on_pop) {
                marked[j] = 1;             /* ★ 入栈这一刻就标记，杜绝第二次入栈 */
            }
            if (!s_push(&st, j)) {
                printf("  !!! 栈容量不够（GDFS_STACK_CAP 算小了）\n");
                return r->order_len;
            }
            pushed++;
        }
        if (verbose && pushed > 0) {
            printf("        %d 压入 %d 个邻居，现在栈是：", u, pushed);
            s_dump(&st);
            printf("\n");
        }
    }

    if (verbose) {
        printf("\n  栈空 → 结束。前序 %d 个点；后序没记（这一版没有『离开』这个时刻）；",
               r->order_len);
        printf("重复出栈 %d 次。\n", r->dup_pops);
    }
    return r->order_len;
}

/*
 * ===================================================================
 * 6. dfs_all：全图遍历（一轮 DFS 只覆盖一个连通分量）
 * ===================================================================
 * 骨架两层，和 BFS 那节完全同构：
 *
 *     for 每个顶点 u:
 *         if u 没被访问过:
 *             从 u 起一轮 DFS        ← 这一轮覆盖的就是一个新的连通分量
 *
 * 为什么必须套外层？示例图 G 有三个分量（{0,1,2,3} {4,5,6} {7}），
 * 只从 0 起一次 DFS，4、5、6、7 永远碰不到，而程序一声不响、返回值也正常。
 * "结果少了一坨但毫无异常"是最难查的 bug，所以全图遍历要养成外层扫的习惯。
 *
 * 边界情况：
 *   - visited[] 跨轮共享（所以放外面），DFS 树、parent、深度则每轮各自新起一根。
 *   - 孤立点 7：度为 0，起一轮只访问它自己 —— 也算一个分量，别漏。
 *   - 跑完 dfs_all 之后 parent 里会有多个 -1（每轮的根都是 -1），
 *     此时 -1 **不再等于"没访问到"**，它表示"我是某轮的根"。
 *     想区分这两种情况要看 tick_in：还是 -1 才是真没访问到。
 */
int dfs_all(const DfsGraph *g, DfsResult *r, int verbose)
{
    res_reset(r);

    int visited[GDFS_MAX_V];
    for (int i = 0; i < GDFS_MAX_V; i++) {
        visited[i] = 0;
    }

    int rounds = 0;
    for (int u = 0; u < g->v; u++) {
        if (visited[u]) {
            if (verbose) {
                printf("  顶点 %d 上一轮已经访问过了 → 跳过，不用起新轮\n", u);
            }
            continue;
        }
        rounds++;
        if (verbose) {
            printf("\n  ═══ 第 %d 轮 DFS：顶点 %d 还没访问过，以它为根起一轮 ═══\n", rounds, u);
        }
        dfs_go(g, u, -1, 0, visited, r, verbose);
        if (verbose) {
            printf("  ═══ 第 %d 轮结束（这一轮覆盖了一个连通分量）═══\n", rounds);
        }
    }
    return rounds;
}

/*
 * ===================================================================
 * 7. dfs_print_result：把四张表摊开，并和"最短距离"当面对质
 * ===================================================================
 * 干什么：打印前序、后序、每个顶点的 parent/depth/进出刻度，再把 DFS 树
 *         的"父子链"从每个点往上追到根，最后用顶点 3 说明
 *         **DFS 的 depth 绝不是最短距离**。
 *
 * 为什么要把父子链打出来：parent[] 只是一堆数字，
 *   "3 的父亲是 2，2 的父亲是 1，1 的父亲是 0" 连起来读才看得出
 *   DFS 走的是一条**深链** 0-1-2-3；而 BFS 给的是**矮胖树**（0 直接管着 1 和 2）。
 *   这一眼就能让"两种遍历的区别"从口号变成画面。
 *
 * 边界情况：
 *   - 没访问到的顶点整行都打 -1，并注明"没访问到"，避免和"深度 0"混淆。
 *   - post[] 可能是空的（朴素栈版给不出后序），要显式说明原因，不能只打一行空白。
 *   - depth[3] 用真值打出来（=3），旁边写出上一节 BFS 实测的 dist[3]（=2），
 *     对比由代码现场给出，不靠 README 硬吹。
 */
void dfs_print_result(const DfsGraph *g, const DfsResult *r)
{
    printf("  前序（进入的顺序）  共 %d 个：", r->order_len);
    for (int i = 0; i < r->order_len; i++) {
        printf("%d ", r->order[i]);
    }
    printf("\n");

    if (r->post_len > 0) {
        printf("  后序（离开的顺序）  共 %d 个：", r->post_len);
        for (int i = 0; i < r->post_len; i++) {
            printf("%d ", r->post[i]);
        }
        printf("\n");
        printf("    读法：谁先被『办完』谁先出现。叶子/死路最先出局，根最后出局。\n");
    } else {
        printf("  后序：空！（这一版没有『离开』这一刻 —— 想要后序必须用递归版或帧模拟版）\n");
    }

    printf("\n");
    printf("  顶点  parent  depth  进刻度  出刻度  说明\n");
    for (int u = 0; u < g->v; u++) {
        printf("  %-4d  %-6d  %-5d  %-6d  %-6d  ",
               u, r->parent[u], r->depth[u], r->tick_in[u], r->tick_out[u]);
        if (r->tick_in[u] < 0) {
            printf("没访问到");
        } else if (r->tick_out[u] < 0) {
            printf("访问了，但没有离开时刻（朴素栈版）");
        } else if (r->parent[u] < 0) {
            printf("本轮的根");
        } else {
            printf("被 %d 钻进来的", r->parent[u]);
        }
        printf("\n");
    }

    printf("\n  DFS 树长什么样（顺着 parent 往上追到根）：\n");
    for (int u = 0; u < g->v; u++) {
        if (r->parent[u] < 0 && r->tick_in[u] < 0) {
            continue;                       /* 压根没访问到，跳过 */
        }
        printf("    顶点 %-2d：", u);
        int cur = u;
        int guard = 0;
        while (cur >= 0 && guard < GDFS_MAX_V + 1) {
            printf("%d", cur);
            cur = r->parent[cur];
            if (cur >= 0) {
                printf(" <- ");
            }
            guard++;                        /* 防御性上限：万一 parent 被写成环，不致死循环 */
        }
        printf("    深度 %d\n", r->depth[u]);
    }

    printf("\n  ★ 当面对质：DFS 的 depth 是不是最短距离？\n");
    printf("    顶点 3：本节点数 depth=%d（0 钻到 1，1 钻到 2，2 钻到 3，钻了 %d 层）\n",
           r->depth[3], r->depth[3]);
    printf("    上一节 BFS 实测 dist[3]=2（0 直接找 2，2 再找 3）\n");
    printf("    %d != 2 ⇒ DFS 的 depth 只是『这次运气走到的层数』，**不能当最短距离用**。\n",
           r->depth[3]);
    printf("    要最短距离请用 BFS（无权图）或 dijkstra（带权图）。\n");
}
