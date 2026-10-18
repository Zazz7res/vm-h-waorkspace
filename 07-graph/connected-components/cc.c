#include <stdio.h>
#include "cc.h"

/*
 * ===================================================================
 * 0. 三个小工具
 * ===================================================================
 * ① adjacent_on()：把"u 和 j 之间有没有边"这件事收在一处。
 *    无向图看 edge[u][j] 就够了（造图时对称涂了两格）。
 *    有向图算**弱**连通分量时要"无视箭头方向"，所以额外看 edge[j][u]。
 *    ★ 为什么只给"分量"这一组函数用？因为"可达"和"判环"必须严格顺着箭头走，
 *      一旦在它们里也这么"两边都看"，就会把无环的 DAG 判成有环 —— 这是本节最大的坑。
 * ② build_cycle()：命中那条"坏边"之后，把环上到底是哪几个点追出来。
 * ③ print_cycle()：把环打成一个圈的样子。
 */

static int adjacent_on(const CCGraph *g, int u, int j)
{
    if (g->edge[u][j]) {
        return 1;                        /* 有 u→j 这条边，无向有向都算数 */
    }
    if (g->directed && g->edge[j][u]) {
        return 1;                        /* 有向图：算"无视方向"的连通关系时才倒过来看 */
    }
    return 0;
}

/*
 * 命中点是 u，坏边是 u→j（j 已经在当前路径上）。
 * 环的形状：j → …… → u →（回到）j。
 * 怎么追：从 u 顺着 parent 往上爬，爬到 j 为止，中间这些点倒过来接在 j 后面。
 * 返回长度（不重复写收尾的那个 j）；cycle_out 至少要有 CC_MAX_V 格。
 */
static int build_cycle(const int parent[], int u, int j, int cycle_out[])
{
    int tmp[CC_MAX_V];
    int n = 0;
    int cur = u;

    while (cur >= 0 && cur != j && n < CC_MAX_V) {
        tmp[n++] = cur;                  /* 先把 u、parent[u]、parent[parent[u]]… 攒起来 */
        cur = parent[cur];
    }
    /* 走到这里 cur 应当正好是 j；如果不是，说明 parent 链断了（内部 bug），也不能死循环 */

    int len = 0;
    cycle_out[len++] = j;                /* 环的"入口"（它是 u 的祖先） */
    for (int i = n - 1; i >= 0; i--) {   /* 倒序 = 从祖先往后代的方向 */
        if (len < CC_MAX_V) {
            cycle_out[len++] = tmp[i];
        }
    }
    return len;
}

/* 把环打成"0 -> 1 -> 2 -> 0"这样一个闭合的圈（收尾重复写第一个点，才看得出是"环"）。
   这个函数公开给 main.c 用：判环函数只负责"找到环 + 回填数组"，怎么展示由调用方决定。 */
void cc_print_cycle(const char *prefix, const int cycle[], int len)
{
    printf("%s", prefix);
    for (int i = 0; i < len; i++) {
        printf("%d -> ", cycle[i]);
    }
    if (len > 0) {
        printf("%d\n", cycle[0]);          /* 回到起点 = 闭合 */
    } else {
        printf("（没有环）\n");
    }
}

static void indent(int n)
{
    for (int i = 0; i < n; i++) {
        printf("  ");
    }
}

/*
 * ===================================================================
 * 1. 造四张固定图 + 打印
 * ===================================================================
 * 全部硬编码，不用随机数据 —— 这样 README 里的输出可以逐字核对。
 * 为什么要四张而不是两张？因为每个判据都需要**阴性对照**：
 *   判环函数说"有环"不值钱，它还得在一棵老实的树上说"没环"；
 *   说"没环"也不值钱，它还得在"环藏在第二个分量里"的图上说"有环"。
 * 一正一反都过，判据才算被验证过。
 */

/* G：8 点 7 边，3 个分量 {0,1,2,3} {4,5,6} {7}，两个分量里各有一个三角环 */
void cc_sample_undirected(CCGraph *g)
{
    for (int i = 0; i < CC_MAX_V; i++) {
        for (int j = 0; j < CC_MAX_V; j++) {
            g->edge[i][j] = 0;
        }
    }
    g->v = 8;
    g->e = 0;
    g->directed = 0;

    static const int pairs[][2] = {
        {0, 1}, {1, 2}, {0, 2}, {2, 3},      /* 分量一：0,1,2 成环 + 挂着 3 */
        {4, 5}, {5, 6}, {4, 6}               /* 分量二：又一个环 */
    };
    for (int k = 0; k < 7; k++) {
        int a = pairs[k][0], b = pairs[k][1];
        g->edge[a][b] = 1;
        g->edge[b][a] = 1;                   /* 无向：对称涂两格 */
        g->e++;
    }
    /* 顶点 7 什么都不加 = 孤立点，它自己就是一个分量 */
}

/* T：5 点 4 边的树。E = V-1 且连通 ⇒ 无环。判环的"阴性对照" */
void cc_sample_tree(CCGraph *g)
{
    for (int i = 0; i < CC_MAX_V; i++) {
        for (int j = 0; j < CC_MAX_V; j++) {
            g->edge[i][j] = 0;
        }
    }
    g->v = 5;
    g->e = 0;
    g->directed = 0;

    static const int pairs[][2] = { {0, 1}, {1, 2}, {2, 3}, {1, 4} };
    for (int k = 0; k < 4; k++) {
        int a = pairs[k][0], b = pairs[k][1];
        g->edge[a][b] = 1;
        g->edge[b][a] = 1;
        g->e++;
    }
}

/*
 * C2：8 点，环**故意藏在第二个分量里**。
 *   分量一 0-1-2-3 是一条链（无环），分量二 4,5,6 是三角环（有环），7 孤立。
 * 这张图专门用来打脸"只从 0 起一轮 DFS"的写法：
 *   从 0 起一轮，看到的全是"已访问的邻居恰好是父亲"，会一本正经回答"无环"。
 */
void cc_sample_split_cycle(CCGraph *g)
{
    for (int i = 0; i < CC_MAX_V; i++) {
        for (int j = 0; j < CC_MAX_V; j++) {
            g->edge[i][j] = 0;
        }
    }
    g->v = 8;
    g->e = 0;
    g->directed = 0;

    static const int pairs[][2] = {
        {0, 1}, {1, 2}, {2, 3},              /* 分量一：链，无环 */
        {4, 5}, {5, 6}, {4, 6}               /* 分量二：环 */
    };
    for (int k = 0; k < 6; k++) {
        int a = pairs[k][0], b = pairs[k][1];
        g->edge[a][b] = 1;
        g->edge[b][a] = 1;
        g->e++;
    }
}

/* DAG：有向无环图，就是拓扑排序那节的示例图（7 点 8 条弧） */
void cc_sample_dag(CCGraph *g)
{
    for (int i = 0; i < CC_MAX_V; i++) {
        for (int j = 0; j < CC_MAX_V; j++) {
            g->edge[i][j] = 0;
        }
    }
    g->v = 7;
    g->e = 0;
    g->directed = 1;

    static const int arcs[][2] = {
        {5, 0}, {5, 2}, {4, 0}, {4, 1}, {2, 3}, {3, 1}, {6, 4}, {6, 2}
    };
    for (int k = 0; k < 8; k++) {
        int a = arcs[k][0], b = arcs[k][1];
        g->edge[a][b] = 1;                   /* 有向：只涂一格！对称那格保持 0 */
        g->e++;
    }
}

/* CYC：有向含环对照组。0 是孤立点，环 1→2→3→1 藏在"第二轮"里 */
void cc_sample_cyclic(CCGraph *g)
{
    for (int i = 0; i < CC_MAX_V; i++) {
        for (int j = 0; j < CC_MAX_V; j++) {
            g->edge[i][j] = 0;
        }
    }
    g->v = 4;
    g->e = 0;
    g->directed = 1;

    static const int arcs[][2] = { {1, 2}, {2, 3}, {3, 1} };
    for (int k = 0; k < 3; k++) {
        int a = arcs[k][0], b = arcs[k][1];
        g->edge[a][b] = 1;
        g->e++;
    }
    /* 顶点 0 不出现在任何弧里：故意留个"第一轮什么也找不到"的坑 */
}

void cc_print(const CCGraph *g)
{
    printf("    这张图是：%s   顶点 %d 个，边 %d 条\n",
           g->directed ? "有向图" : "无向图", g->v, g->e);
    printf("    邻接矩阵（1 = 有一条从行到列的边）：\n");
    printf("        j=  ");
    for (int j = 0; j < g->v; j++) {
        printf("%2d", j);
    }
    printf("\n");
    for (int i = 0; i < g->v; i++) {
        printf("    i=%-3d [ ", i);
        for (int j = 0; j < g->v; j++) {
            printf("%2d ", g->edge[i][j]);
        }
        printf("]  顺着箭头能到：");
        int any = 0;
        for (int j = 0; j < g->v; j++) {
            if (g->edge[i][j] && j != i) {
                printf("%d ", j);
                any = 1;
            }
        }
        if (!any) {
            printf("（没有出边）");
        }
        if (g->directed) {
            printf("   |  谁能直达我：");
            int any_in = 0;
            for (int j = 0; j < g->v; j++) {
                if (g->edge[j][i] && j != i) {
                    printf("%d ", j);
                    any_in = 1;
                }
            }
            if (!any_in) {
                printf("（没人）");
            }
        }
        printf("\n");
    }
}

/*
 * ===================================================================
 * 2. 连通分量：上一节那句"外层再扫一遍"的正式版本
 * ===================================================================
 * 骨架：
 *
 *     comp_id[] 全填 -1
 *     k = 0
 *     for 每个顶点 u:
 *         if comp_id[u] == -1:            ← 遇见一个没归队的点
 *             从 u 起一轮 DFS，把碰到的点全写 comp_id = k
 *             k++                          ← 一轮 = 一个分量
 *     return k
 *
 * 为什么可以直接拿 comp_id 当 visited 用？
 *   "归过队"和"访问过"在这里是同一件事，省一个数组，也少一个"两个数组不同步"的 bug。
 *
 * 边界情况：
 *   - 孤立点：一轮只覆盖它自己，也算一个分量（别写 `if 度为 0 就跳过`）。
 *   - 有向图：**无视箭头**算出来的是"弱连通分量"，这是有定义的、正确的东西；
 *     但它不等于强连通分量（见 cc.h 第五节 + README）。所以这里用 adjacent_on()，
 *     并在 directed 图上先打印一条醒目提示。
 *   - 复杂度：矩阵版每个点扫一行 ⇒ O(V²)；邻接表版 O(V+E)（有向图算弱连通时
 *     要能反向查邻居，得建反向图，或者干脆造图时就两向都存）。
 */

static void comp_go(const CCGraph *g, int u, int id, int comp_id[], int *count, int depth, int verbose)
{
    comp_id[u] = id;                       /* 归队。这一步=打标记，必须进门就做 */
    (*count)++;
    if (verbose) {
        indent(depth);
        printf("顶点 %d 归入分量 %d（此刻这一轮收了 %d 个点）\n", u, id, *count);
    }
    for (int j = 0; j < g->v; j++) {
        if (!adjacent_on(g, u, j) || j == u) {
            continue;                      /* 不相邻 / 自环：跳过 */
        }
        if (comp_id[j] != -1) {
            if (verbose) {
                indent(depth + 1);
                printf("%d 已经归队了（分量 %d）→ 不重复收\n", j, comp_id[j]);
            }
            continue;
        }
        comp_go(g, j, id, comp_id, count, depth + 1, verbose);
    }
}

int cc_find_all(const CCGraph *g, int comp_id[], int verbose)
{
    for (int i = 0; i < CC_MAX_V; i++) {
        comp_id[i] = -1;
    }

    if (g->directed) {
        printf("  ⚠ 提示：这是有向图。下面按『无视箭头』算出来的是**弱连通分量**。\n");
        printf("    有向图真正有用的那个概念是**强连通分量（SCC）**：块内任意两点互相可达。\n");
        printf("    它需要 Kosaraju / Tarjan，本节没实现（README 里把思路讲清楚了）。\n");
        printf("    先用第 5 幕的 cc_reachable 亲眼看看『单向可达』和『互相可达』差在哪。\n\n");
    }

    int k = 0;
    for (int u = 0; u < g->v; u++) {
        if (comp_id[u] != -1) {
            continue;                       /* 早就归队了，说明它和前面某个根连通 */
        }
        if (verbose) {
            printf("\n  ═══ 第 %d 轮：顶点 %d 还没归队 → 以它为根起一轮 DFS，收一个新的分量 ═══\n",
                   k, u);
        }
        int count = 0;
        comp_go(g, u, k, comp_id, &count, 0, verbose);
        if (verbose) {
            printf("  ─── 第 %d 轮结束，收了 %d 个点 ───\n", k, count);
        }
        k++;
    }
    return k;
}

void cc_print_components(const CCGraph *g, const int comp_id[], int comp_count)
{
    printf("  comp_id[]（每个点属于哪个分量）：\n    顶点   ");
    for (int u = 0; u < g->v; u++) {
        printf("%-4d", u);
    }
    printf("\n    编号   ");
    for (int u = 0; u < g->v; u++) {
        printf("%-4d", comp_id[u]);
    }
    printf("\n\n  按分量分组：\n");
    for (int k = 0; k < comp_count; k++) {
        printf("    分量 %d：{ ", k);
        int n = 0;
        for (int u = 0; u < g->v; u++) {
            if (comp_id[u] == k) {
                printf("%d ", u);
                n++;
            }
        }
        printf("}  共 %d 个顶点", n);
        if (n == 1) {
            printf("（孤立点也算一个分量）");
        }
        printf("\n");
    }
    printf("\n  两个白送的结论：\n");
    printf("    · 这张图整体连通吗 ⟺ 分量数 == 1（现在是 %d）\n", comp_count);
    printf("    · u 和 v 连通吗   ⟺ comp_id[u] == comp_id[v]（比一次数组就答完）\n");
}

/*
 * 每个分量的顶点数 V_k、边数 E_k，并和无向图的经典结论对一下：
 *
 *     连通且无环（= 树）  ⟺  E_k == V_k - 1
 *     连通且 E_k >= V_k        ⇒  这个分量里**一定**有环
 *
 * 为什么顺手算这个？因为它是判环结果的**免费交叉验证**：
 * 环的存在性被 DFS 判了一次，又被边数判了一次，两个答案必须一致。
 * （仓库里类似的手法：邻接矩阵那节用 Σ度 = 2E 自查、邻接表那节用节点数 = 2E 自查。）
 *
 * ⚠ 这个判据**只在无向图成立**。DAG 有 7 点 8 弧，E > V，却一个环都没有 ——
 *   所以下面遇到有向图会明确说"此判据不适用，请用三色法"。
 */
void cc_component_stats(const CCGraph *g, const int comp_id[], int comp_count)
{
    printf("  逐分量统计（顶点数 V、边数 E）：\n");
    for (int k = 0; k < comp_count; k++) {
        int vv = 0;
        for (int u = 0; u < g->v; u++) {
            if (comp_id[u] == k) {
                vv++;
            }
        }
        int ee = 0;
        for (int u = 0; u < g->v; u++) {
            if (comp_id[u] != k) {
                continue;
            }
            for (int j = 0; j < g->v; j++) {
                if (j <= u && !g->directed) {
                    continue;              /* 无向：只数 u<j 那半边，一条边数一次 */
                }
                if (j == u) {
                    continue;              /* 自环本节示例图里没有，统计时先不算进来 */
                }
                if (adjacent_on(g, u, j)) {
                    ee++;
                }
            }
        }
        printf("    分量 %d：V=%d  E=%d  ", k, vv, ee);
        if (g->directed) {
            printf("（有向图：E 与 V 的大小关系推不出有没有环，请用三色法）\n");
            continue;
        }
        if (ee == vv - 1) {
            printf("E == V-1 ⇒ 这分量是**树**，必然无环\n");
        } else if (ee >= vv) {
            printf("E >= V   ⇒ 这分量里**必然有环**（看 DFS 是不是也这么说）\n");
        } else {
            printf("E < V-1  ⇒ 不可能（说明它压根不是连通分量，代码有 bug）\n");
        }
    }
}

/*
 * ===================================================================
 * 3. 无向图判环：DFS 带父亲
 * ===================================================================
 * 判据一句话：**碰到"已访问且不是父亲"的邻居 ⇒ 有环。**
 *
 * 为什么一定要带父亲这个参数？
 *   无向图一条边在矩阵里涂了两格：u→v 和 v→u。
 *   我从 u 钻到 v，v 往回看第一眼就能看到 u，而且 u 已访问。
 *   不排掉父亲，一棵最老实的树也会被判成"处处有环"（本节用 T 图专门验证了这一点）。
 *
 * 为什么"已访问且非父亲"就真的是环？
 *   它已访问、又不是我父亲 ⇒ 它是我当前这条路径上**更早**的某个点（我的祖先）。
 *   于是 它 → …… → 我 →（这条边）→ 它，闭合了。所以 build_cycle 沿 parent 往上追就能把环打出来。
 *
 * 边界情况：
 *   - 自环 edge[u][u]：它自己就是环（长度 1），所以进门先查对角线。
 *   - 只从 start 起一轮会漏（cc_has_cycle_undirected_from 就是这个反面教材），
 *     正确版本 cc_has_cycle_undirected 外层扫每个未访问点。
 *   - 用错图类型：在有向图上跑无向判据会因为"回程边"而误判 ⇒ 直接拒绝并说明原因。
 */
static int cyc_und_go(const CCGraph *g, int u, int father, int visited[], int parent[],
                      int cycle_out[], int *len_out, int depth, int verbose)
{
    visited[u] = 1;
    if (verbose) {
        indent(depth);
        printf("进入 %d（父亲 %-2d）\n", u, father);
    }
    if (g->edge[u][u]) {
        cycle_out[0] = u;
        *len_out = 1;
        if (verbose) {
            indent(depth);
            printf("★ %d 有自环 → 这就是一个环\n", u);
        }
        return 1;
    }

    for (int j = 0; j < g->v; j++) {       /* 邻居顺序固定：列号升序，结果可复现 */
        if (!g->edge[u][j] || j == u) {
            continue;
        }
        if (visited[j]) {
            if (j == father) {
                if (verbose) {
                    indent(depth + 1);
                    printf("邻居 %d：已访问，但它是父亲 → 只是我回来的路，不算环\n", j);
                }
                continue;
            }
            if (verbose) {
                indent(depth + 1);
                printf("★ 邻居 %d：已访问，而且不是我父亲 → 后向边 ⇒ **有环**\n", j);
            }
            *len_out = build_cycle(parent, u, j, cycle_out);
            return 1;
        }
        if (verbose) {
            indent(depth + 1);
            printf("邻居 %d：没访问过 → 钻进去\n", j);
        }
        parent[j] = u;                     /* ★ 必须填：build_cycle 就靠这条 parent 链追环 */
        int found = cyc_und_go(g, j, u, visited, parent, cycle_out, len_out, depth + 1, verbose);
        if (found) {
            return 1;                      /* 子树里找到环 → 一路往上传，别再看别的邻居了 */
        }
    }
    if (verbose) {
        indent(depth);
        printf("离开 %d：这一带转了一圈，没发现环\n", u);
    }
    return 0;
}

int cc_has_cycle_undirected_from(const CCGraph *g, int start,
                                 int cycle_out[], int *len_out, int verbose)
{
    *len_out = 0;
    if (g->directed) {
        printf("  ✗ 拒绝：这是有向图，无向判环的『排掉父亲』在这里不成立（会把 DAG 判成有环）。\n");
        printf("    有向图请用 cc_has_cycle_directed（三色法）。\n");
        return -1;
    }
    int visited[CC_MAX_V];
    int parent[CC_MAX_V];
    for (int i = 0; i < CC_MAX_V; i++) {
        visited[i] = 0;
        parent[i] = -1;
    }
    if (verbose) {
        printf("  【只起一轮】从 %d 出发：\n", start);
    }
    int r = cyc_und_go(g, start, -1, visited, parent, cycle_out, len_out, 0, verbose);
    if (verbose && !r) {
        printf("  这一轮的可达范围里没找到环 —— 注意：这只说明『这一片』没环。\n");
    }
    return r ? 1 : 0;
}

int cc_has_cycle_undirected(const CCGraph *g, int cycle_out[], int *len_out, int verbose)
{
    *len_out = 0;
    if (g->directed) {
        printf("  ✗ 拒绝：这是有向图，无向判环的『排掉父亲』在这里不成立（会把 DAG 判成有环）。\n");
        printf("    有向图请用 cc_has_cycle_directed（三色法）。\n");
        return -1;
    }
    int visited[CC_MAX_V];
    int parent[CC_MAX_V];
    for (int i = 0; i < CC_MAX_V; i++) {
        visited[i] = 0;
        parent[i] = -1;
    }

    for (int u = 0; u < g->v; u++) {
        if (visited[u]) {
            continue;                      /* 已经在这张图的某轮里查过了 */
        }
        if (verbose) {
            printf("  【全图扫描】顶点 %d 还没查过 → 以它为起点再起一轮\n", u);
        }
        if (cyc_und_go(g, u, -1, visited, parent, cycle_out, len_out, 0, verbose)) {
            return 1;                      /* 任一轮找到就算有环，直接返回 */
        }
        if (verbose) {
            printf("  ─── 从 %d 起的这一轮查完了，没环 ───\n", u);
        }
    }
    return 0;                              /* 所有轮都查完，全无环 ⇒ 这是一片森林 */
}

/*
 * ===================================================================
 * 4. 有向图判环：三色法（白 / 灰 / 黑）
 * ===================================================================
 * 判据一句话：**DFS 中碰到灰色的邻居 ⇒ 有环**；碰到黑色完全正常。
 *
 *     白 = 没来过
 *     灰 = 来过但还没走完 = 还压在"当前这条往下钻的路"上（还在递归栈里）
 *     黑 = 走完了（它和它能到的地方全办完了），已经出栈
 *
 * 为什么灰色命中就是环：灰 ⇒ 它在我当前的路径上 ⇒ 从它出发有一条路走到我；
 * 现在我又有一条边指回它 ⇒ 它 → …… → 我 → 它，有向环闭合。
 * 为什么黑色不算：黑 ⇒ 它早就办完了 ⇒ 从我出发不可能再回到它（真能回去它就不会先变黑），
 * 这条边只是"指向一个更早 finished 的点"，不构成回路。
 *
 * 为什么不拿无向那套（visited + 排父亲）？
 *   有向图没有"父亲挡回来路"这回事（回程边压根不存在），
 *   而"已访问"太粗：0→1、0→2、2→1 这种菱形分叉里，1 被 0 先访问、2 再看到 1，
 *   按两态判据就误报有环 —— 三色的价值就是把"访问过"细分成
 *   "还在路上（灰）"和"已经下班（黑）"。
 *
 * 边界情况：
 *   - 一定要外层扫全图：对照图 CYC 里环是 1→2→3→1，可第一个顶点 0 是孤立点，
 *     只从 0 起一轮就会答"无环"。
 *   - 自环 u→u：灰色命中（u 自己就灰着），判出长度 1 的环，逻辑自然覆盖到，
 *     这里为省事先单独查一下，画面更好看。
 *   - 有向无环图上跑三色法，一定会看到"碰到黑色"这种情况（DAG 就有），那正是它该沉默的场合。
 */
static const char *color_name(int c)
{
    if (c == CC_WHITE) {
        return "白";
    }
    if (c == CC_GRAY) {
        return "灰";
    }
    return "黑";
}

/* 把全图当前颜色打一行。跑完之后再回头看这张表，"灰=在路上"这件事会变得非常直观：
   正常结束（不管是判出环还是判出无环）时，颜色表应当全是黑（除了环上那些还没退栈的点） */
static void dump_colors(const CCGraph *g, const int col[])
{
    printf("  此刻各顶点颜色：");
    for (int u = 0; u < g->v; u++) {
        printf("%d=%s ", u, color_name(col[u]));
    }
    printf("\n");
}

static int cyc_dir_go(const CCGraph *g, int u, int color[], int parent[],
                      int cycle_out[], int *len_out, int depth, int verbose)
{
    color[u] = CC_GRAY;                    /* 进门：我现在在"路上" */
    if (verbose) {
        indent(depth);
        printf("进入 %d → 涂灰（父亲 %-2d）\n", u, parent[u]);
    }
    if (g->edge[u][u]) {
        cycle_out[0] = u;
        *len_out = 1;
        if (verbose) {
            indent(depth);
            printf("★ %d 有自环 → 有向环（长度 1）\n", u);
        }
        return 1;
    }

    for (int j = 0; j < g->v; j++) {       /* 顺着箭头看邻居，列号升序 = 固定顺序 */
        if (!g->edge[u][j] || j == u) {
            continue;
        }
        if (color[j] == CC_GRAY) {
            if (verbose) {
                indent(depth + 1);
                printf("★ %d → %d：目标是**灰色**（还在当前这条路上）⇒ **有环**\n", u, j);
            }
            *len_out = build_cycle(parent, u, j, cycle_out);
            return 1;
        }
        if (color[j] == CC_BLACK) {
            if (verbose) {
                indent(depth + 1);
                printf("%d → %d：目标是**黑色**（比我先办完，已经下班）→ 正常，不成环\n", u, j);
            }
            continue;
        }
        if (verbose) {
            indent(depth + 1);
            printf("%d → %d：目标是**白色**（没来过）→ 钻进去\n", u, j);
        }
        parent[j] = u;
        if (cyc_dir_go(g, j, color, parent, cycle_out, len_out, depth + 1, verbose)) {
            return 1;
        }
    }

    color[u] = CC_BLACK;                   /* 出门：它和它能到的一切我都钻完了 */
    if (verbose) {
        indent(depth);
        printf("离开 %d → 涂黑\n", u);
    }
    return 0;
}

int cc_has_cycle_directed(const CCGraph *g, int cycle_out[], int *len_out, int verbose)
{
    *len_out = 0;
    if (!g->directed) {
        printf("  ✗ 拒绝：这是无向图。无向图每条边都是一对反向弧，三色法会立刻把任何一条边判成环。\n");
        printf("    无向图请用 cc_has_cycle_undirected（带父亲的 DFS）。\n");
        return -1;
    }
    int color[CC_MAX_V];
    int parent[CC_MAX_V];
    for (int i = 0; i < CC_MAX_V; i++) {
        color[i] = CC_WHITE;
        parent[i] = -1;
    }

    for (int u = 0; u < g->v; u++) {
        if (color[u] != CC_WHITE) {
            continue;                      /* 灰/黑 都说明这附近已经查过了 */
        }
        if (verbose) {
            printf("  【全图扫描】%d 还是白色 → 以它为根再起一轮（颜色数组**跨轮不清零**）\n", u);
        }
        if (cyc_dir_go(g, u, color, parent, cycle_out, len_out, 0, verbose)) {
            if (verbose) {
                printf("  命中那一刻的颜色快照（注意环上那几点还是灰的 —— 它们都还在路上）：\n");
                dump_colors(g, color);
            }
            return 1;
        }
        if (verbose) {
            printf("  ─── 从 %d 起的一轮全涂黑了，没环 ───\n", u);
            dump_colors(g, color);
        }
    }
    return 0;
}

/*
 * ===================================================================
 * 5. cc_reachable：只为把"强连通"这个问题演示出来
 * ===================================================================
 * 干什么：从 start 出发、严格**顺着箭头**能走到的点集（含 start 自己）。
 * 一次 DFS 就是它，所以代码只有十几行。
 *
 * 为什么本节要有这个函数：无向图里"能到"就是"连通"；有向图里"我能到你"和"你能到我"
 * 是两码事，而**强连通 = 互相都能到**。把两组可达集打出来一对比，
 * 读者自己就会问"那怎么把互相可达的点归类" —— 这就是 Kosaraju/Tarjan 解决的问题。
 *
 * 边界情况：
 *   - 顺着箭头看邻居（只看 g->edge[u][j]），**不要**用 adjacent_on()，
 *     那就变成"无视方向"的弱连通了。这两个函数故意长得很像，差别就在这一行。
 *   - 有向图从某些点出发可达集很小：DAG 里从 0 出发只有 {0}（0 没有任何出边）。
 */
int cc_reachable(const CCGraph *g, int start, int reach_out[], int verbose)
{
    int visited[CC_MAX_V];
    for (int i = 0; i < CC_MAX_V; i++) {
        visited[i] = 0;
    }

    int n = 0;
    reach_out[n++] = start;
    visited[start] = 1;
    if (verbose) {
        printf("    起点 %d（它自己当然可达）\n", start);
    }
    /* 这里刻意用"数组当下推队列"的写法（第 04 章朴素队列同款）：
       本节只关心"能到哪些点"这个**集合**，不关心访问顺序，所以谁先展开都行。
       换成 DFS 递归，最后得到的集合一模一样（这正是"遍历"两大流派结果集相同、顺序不同）。 */
    for (int k = 0; k < n; k++) {          /* n 会在循环里长大 → 边展开边扩集合 */
        int u = reach_out[k];
        for (int j = 0; j < g->v; j++) {
            if (!g->edge[u][j] || j == u || visited[j]) {
                continue;                  /* 严格按箭头方向：只看 edge[u][j] */
            }
            visited[j] = 1;
            reach_out[n++] = j;
            if (verbose) {
                printf("    %d → %d：新可达，加入集合\n", u, j);
            }
        }
    }
    return n;
}
