#include <stdio.h>
#include <stdlib.h>
#include "uf.h"

/*
 * ============================================================================
 * 并查集的实现：说穿了就是"两个 int 数组 + 三个 while/递归"
 * ============================================================================
 *
 * 先把最核心的直觉画清楚。假设 8 个元素，现在被分成了两个集合：
 *
 *   集合 A = {0, 1, 3, 7}        集合 B = {2, 4, 5, 6}
 *
 * 它们在内存里不是"两组数字"，而是 parent 数组里的指针关系：
 *
 *   下标   :  0  1  2  3  4  5  6  7
 *   parent :  0  0  2  1  2  4  4  3
 *             ↑        ↑        ↑
 *   谁是谁爹（画成树）:
 *
 *     树A:  0            树B:  2
 *           ├─ 1               ├─ 5
 *           │   └─ 3           └─ 4
 *           │        └─ 7           ├─ 6
 *
 *   find(7)：7 → 3 → 1 → 0，parent[0]==0，撞到根了，返回 0。
 *   find(6)：6 → 4 → 2，parent[2]==2，返回 2。
 *   0 != 2 ⇒ 不同集合。如果这里两个根相等，那才叫同一集合。
 *
 * 关键认知：**parent 数组里存的数字，永远只是"父亲的下标"，不是集合编号。**
 * 集合编号是"爬到顶之后那个数的下标"，它随时可能因为合并而变。
 * 所以任何时候都别把根记死了，要判断就现爬。
 */

/* ---------- 生命周期 ---------- */

/*
 * 造一个 n 个元素的并查集。
 *
 * 初始状态是"最碎"的：每个人自己一个集合，所以 parent[i] = i（自己就是自己的爹 = 根），
 * rank[i] = 0（一个孤点的高度算 0 层），components = n（一共 n 个集合）。
 *
 * 内存布局（n = 5）：
 *   uf.parent ──→ [ 0 ][ 1 ][ 2 ][ 3 ][ 4 ]     每个格子指向自己 ⇒ 5 棵树
 *   uf.rank   ──→ [ 0 ][ 0 ][ 0 ][ 0 ][ 0 ]     都是孤点，高度 0
 */
int uf_init(UF *uf, int n)
{
    if (uf == NULL || n <= 0 || n > UF_MAX_N) {
        printf("      [错误] uf_init 参数不对：n=%d（合法范围 1..%d）\n", n, UF_MAX_N);
        return -1;
    }

    /* calloc 而不是 malloc：申请回来顺手清零，省掉"忘了初始化就用"的坑 */
    uf->parent = calloc((size_t)n, sizeof(int));
    uf->rank   = calloc((size_t)n, sizeof(int));
    if (uf->parent == NULL || uf->rank == NULL) {
        /* 任何一块没申请到，另一块要立刻还回去，不然就漏了 */
        free(uf->parent);
        free(uf->rank);
        uf->parent = NULL;
        uf->rank = NULL;
        printf("      [错误] 内存申请失败，想要 %d 个元素\n", n);
        return -1;
    }

    uf->n = n;
    uf->components = n;   /* 一开始谁也不认识谁，n 个点就是 n 个集合 */
    uf->walks = 0;

    for (int i = 0; i < n; i++) {
        uf->parent[i] = i;   /* 自己指自己 = 我是根 */
        uf->rank[i] = 0;
    }
    return 0;
}

/*
 * 谁申请谁还。free 之后立刻把指针置 NULL：
 *   - 万一后面误用，程序当场崩，比悄悄读到垃圾值好查一万倍
 *   - free(NULL) 是合法空操作，所以重复调用 uf_free 也不会炸
 */
void uf_free(UF *uf)
{
    if (uf == NULL) {
        return;
    }
    free(uf->parent);
    free(uf->rank);
    uf->parent = NULL;
    uf->rank = NULL;
    uf->n = 0;
    uf->components = 0;
}

/*
 * 恢复到"每个人各自一个集合"。做第二遍实验时用它，就不用重新 malloc 了。
 * 注意它不会改变 n（数组还是那么大）。
 */
void uf_reset(UF *uf)
{
    for (int i = 0; i < uf->n; i++) {
        uf->parent[i] = i;
        uf->rank[i] = 0;
    }
    uf->components = uf->n;
    uf->walks = 0;
}

/* ---------- 核心操作 1：朴素 find（不压缩，只负责让你看见路径） ---------- */

/*
 * 一路往上爬，爬到"parent[cur] == cur"为止，那个 cur 就是根。
 *
 * 为什么要有这个"不压缩"的版本？因为它能老老实实地把路径抄出来：
 *
 *   find_naive(7) 在这样一棵树上：
 *        0
 *        ├─ 1
 *        │    └─ 3
 *        │         └─ 7
 *   爬的路径：  7 → 3 → 1 → 0        path_out = [7, 3, 1, 0]，长度 4，爬了 3 条边
 *
 * 边界情况：
 *   - x 本身就是根：循环一次都不进，路径只有 [x] 一个点，走了 0 条边
 *   - path_out 传 NULL：只想要根，不想要路径，照样能跑
 *   - 路径比 path_out 数组还长：只抄前 path_cap 个，不越界写（并查集正常用不会这么深）
 *   - 如果 union 写错形成了"环"（比如 parent[i] 指到自己链上的祖先），
 *     这个 while 会永远爬不到根 —— 这是并查集最经典的死循环 bug，
 *     原因是 union 里挂错了对象（挂了非根节点上去）。本文件的 union 只挂根，所以安全。
 */
int uf_find_naive(UF *uf, int x, int path_out[], int path_cap, int *path_len_out)
{
    if (x < 0 || x >= uf->n) {
        printf("      [错误] uf_find_naive 下标越界：%d（合法 0..%d）\n", x, uf->n - 1);
        if (path_len_out != NULL) {
            *path_len_out = 0;
        }
        return -1;
    }

    int len = 0;
    int cur = x;
    long long hops = 0;

    /* 抄下起点，然后一步一挪 */
    if (path_out != NULL && len < path_cap) {
        path_out[len++] = cur;
    }
    while (uf->parent[cur] != cur) {
        cur = uf->parent[cur];
        hops++;
        if (path_out != NULL && len < path_cap) {
            path_out[len++] = cur;
        }
    }

    if (path_len_out != NULL) {
        *path_len_out = len;
    }
    uf->walks = hops;
    return cur;
}

/* ---------- 核心操作 2：带路径压缩的 find（正式代码用它） ---------- */

/*
 * 内部递归。每层干三件事：
 *   1) 我若是根，直接回自己
 *   2) 否则让父亲去爬，拿到根
 *   3) 【关键】把父亲的父亲改成根 —— 也就是"我直接挂到根上"
 *
 *                    压缩前                        压缩后
 *              0                      0
 *              ├─ 1                   ├─ 1 ─┐
 *              │    └─ 3                   ├─ 3 ─┤  ← 全挂到 0 上
 *              │         └─ 7              └─ 7
 *              └─ 2                         └─ 2
 *   find(7) 爬过 7→3→1→0，回来时（递归的return路上）依次执行：
 *     parent[3] = 0    parent[1] = 0    ...   一句话："我爬过的点，以后一步到顶"
 *
 * 为什么这样写是合法的？因为 parent[x] 只要"和根同集合"就行，
 * 树只是用来快速找根的工具，工具内部怎么 rearrange 都不改变"谁和谁一个集合"这个事实。
 *
 * 边界：递归深度 = 树高。最坏（不优化）会到 n，本文件演示 n≤12，栈绝对够用；
 *      真正的工程中 n=1e6 时也建议写成"先爬到根、再倒着回写"的迭代版，避免爆栈。
 *      这里为了代码最短最清楚，用的是递归版。
 */
static int find_compress(UF *uf, int x, long long *hops)
{
    if (uf->parent[x] == x) {
        return x;                    /* 我是根，递归到底 */
    }
    (*hops)++;                       /* 又要多爬一条边，记账 */
    int root = find_compress(uf, uf->parent[x], hops);
    uf->parent[x] = root;            /* ★路径压缩就这一行：把我直接挂到根上 */
    return root;
}

int uf_find(UF *uf, int x)
{
    if (x < 0 || x >= uf->n) {
        printf("      [错误] uf_find 下标越界：%d（合法 0..%d）\n", x, uf->n - 1);
        return -1;
    }
    /* 这里是"入口"，所以把步数清零再爬；递归调用走的是上面那个 static 函数，不会重复清 */
    long long hops = 0;
    int root = find_compress(uf, x, &hops);
    uf->walks = hops;          /* 把这次爬的边数留下，demo 里要拿它跟朴素版对比 */
    return root;
}

/* ---------- 核心操作 3：按秩合并 ---------- */

/*
 * rank[i] 的含义：**以 i 为根的这棵树"至少有多高"的估计值**，只有根上的 rank 才是准的。
 * 为什么叫"秩"？因为英文是 rank，本意是树的高度上界。也有教材用 size[]（集合大小），
 * 叫"按大小合并"，两者效果一样：都是"小挂到大"，只是比的是高度还是人数。
 *
 * 规则（三行代码就能写完）：
 *   1) 两根相同 ⇒ 本来一家人，返回 0，什么都不做
 *   2) 谁的 rank 小，谁的根就当别人的儿子（矮树挂到高树下面）
 *   3) 两个 rank 一样时，随便选一边挂，挂完这棵新树的高度要 +1
 *
 *   rank 2 的树 + rank 0 的树 ⇒ 还是 rank 2 的树（矮的挂进去，高度没变，不用+1）
 *   rank 2 的树 + rank 2 的树 ⇒ rank 3 的树（必须 +1，否则下次判断就低估了）
 *
 * 为什么这样能保证树矮？每棵树的 rank 要涨一次，就得撞上另一棵一样高的树，
 * 而"凑齐两棵高 h 的树"至少需要 2^h 个元素 ⇒ h ≤ log2(n)。
 *
 * 返回 0（本来就在一个集合）这一条非常重要：判无向图有没有环、Kruskal 判该不该选这条边，
 * 靠的全是它。
 */
int uf_union(UF *uf, int a, int b)
{
    if (a < 0 || a >= uf->n || b < 0 || b >= uf->n) {
        printf("      [错误] uf_union 下标越界：(%d,%d)，合法 0..%d\n", a, b, uf->n - 1);
        return -1;
    }

    int ra = uf_find(uf, a);   /* 先各自爬到根（顺手压缩了路径） */
    int rb = uf_find(uf, b);
    if (ra == rb) {
        return 0;              /* 同一个根 ⇒ 同一个集合 ⇒ 没合 */
    }

    /* 让 ra 成为"较高"的那一棵，后面就只用管 rb 往哪挂 */
    if (uf->rank[ra] < uf->rank[rb]) {
        int tmp = ra;
        ra = rb;
        rb = tmp;
    }

    uf->parent[rb] = ra;       /* ★ 矮树的根，变成高树的儿子（只改这一个格子） */

    if (uf->rank[ra] == uf->rank[rb]) {
        uf->rank[ra]++;        /* 只有两棵一样高时，新树才真的变高了 */
    }

    uf->components--;          /* 少了一个集合，连通分量记账 */
    return 1;
}

/*
 * 【故意不优化】的 union：无条件把 a 的根挂到 b 的根下面，不看 rank。
 * 只按顺序做 union(0,1)、union(1,2)、union(2,3)... 就会得到一条"链"：
 *
 *      0 → 1 → 2 → 3 → 4 → 5      （0 是最深的叶子，find(0) 要爬 5 条边）
 *
 * 这就是"并查集退化回链表"的样子，复杂度从近似 O(1) 掉到 O(n)。
 * 留着它是为了让下一次对比实验有说服力。
 */
int uf_union_naive(UF *uf, int a, int b)
{
    if (a < 0 || a >= uf->n || b < 0 || b >= uf->n) {
        printf("      [错误] uf_union_naive 下标越界：(%d,%d)\n", a, b);
        return -1;
    }

    /* 注意：这里刻意用朴素 find（不压缩），完全模拟"两个优化都没有"的原始版本 */
    int ra = uf_find_naive(uf, a, NULL, 0, NULL);
    int rb = uf_find_naive(uf, b, NULL, 0, NULL);
    if (ra == rb) {
        return 0;
    }

    uf->parent[ra] = rb;       /* 不看高度，硬挂 ⇒ 可能挂出长链 */
    uf->components--;
    return 1;
}

/* ---------- 判连通：并查集 90% 的用法就是这一行 ---------- */

/*
 * "a 和 b 在不在同一个集合" ⇔ "它们爬到的根是不是同一个"。
 * 内部各做一次 find。返回值：1 是同一集合，0 不是，-1 下标非法。
 */
int uf_connected(UF *uf, int a, int b)
{
    if (a < 0 || a >= uf->n || b < 0 || b >= uf->n) {
        printf("      [错误] uf_connected 下标越界：(%d,%d)\n", a, b);
        return -1;
    }
    return uf_find(uf, a) == uf_find(uf, b) ? 1 : 0;
}

/* 剩几个集合？合并一次就 --，所以这里 O(1)，不用遍历。 */
int uf_component_count(const UF *uf)
{
    return uf->components;
}

/* ---------- 辅助：真实树高（只给打印用，不属于并查集本体） ---------- */

/*
 * 递归算"以 root 为根的子树有几层"。孤点算 0 层。
 * 为什么要遍历全部下标找孩子？因为 parent 数组只有" upward 指针"（记爹），
 * 没有" downward 指针"（记儿子）。想反过来找儿子，只能全表扫一遍。
 * 这也是并查集的一个真实缺点：**它不支持"列出某个集合的所有成员"这种操作**（除非自己再加链表）。
 */
int uf_tree_height(const UF *uf, int root)
{
    int tallest = -1;      /* 没找到一个孩子时，最后 -1+1 = 0，正好是孤点高度 */
    for (int j = 0; j < uf->n; j++) {
        if (j != root && uf->parent[j] == root) {
            int child_h = uf_tree_height(uf, j);
            if (child_h > tallest) {
                tallest = child_h;
            }
        }
    }
    return tallest + 1;
}

/* ---------- 打印 ---------- */

/*
 * 把 parent / rank 两条数组整排打出来，再打上"现在有几个集合、根分别是谁"。
 * 每次 union/find 之后都调它，是本节 demo 的灵魂：森林的变化全在眼皮底下。
 */
void uf_print_parent_array(const UF *uf, const char *tag)
{
    if (tag != NULL && tag[0] != '\0') {   /* tag 给空串就只打表，不打标题行 */
        printf("    %s\n", tag);
    }
    printf("      下标   :");
    for (int i = 0; i < uf->n; i++) {
        printf("%3d", i);
    }
    printf("\n      parent :");
    for (int i = 0; i < uf->n; i++) {
        printf("%3d", uf->parent[i]);
    }
    printf("\n      rank   :");
    for (int i = 0; i < uf->n; i++) {
        printf("%3d", uf->rank[i]);
    }

    printf("\n      集合数 = %d，根分别是：", uf->components);
    for (int i = 0; i < uf->n; i++) {
        if (uf->parent[i] == i) {        /* parent[i]==i ⇔ i 是根 */
            printf("%d ", i);
        }
    }
    printf("\n");
}

/* 递归打印一棵树：depth 表示现在第几层，用来缩进 */
static void print_subtree(const UF *uf, int node, int depth)
{
    for (int d = 0; d < depth; d++) {
        printf("        ");   /* 每深一层缩进 8 空格 */
    }
    if (depth > 0) {
        printf("└─ ");
    }
    printf("%d", node);
    if (uf->parent[node] == node) {
        printf("  <根, rank=%d, 树高=%d>", uf->rank[node], uf_tree_height(uf, node));
    }
    printf("\n");

    for (int j = 0; j < uf->n; j++) {
        if (j != node && uf->parent[j] == node) {   /* j 是 node 的儿子 */
            print_subtree(uf, j, depth + 1);
        }
    }
}

/* 把所有根都列出来，各挂一棵树 ⇒ 整个森林 */
void uf_print_forest(const UF *uf)
{
    for (int i = 0; i < uf->n; i++) {
        if (uf->parent[i] == i) {
            print_subtree(uf, i, 0);
        }
    }
}
