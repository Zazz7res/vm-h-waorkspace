#include <stdio.h>
#include <string.h>
#include "kruskal.h"

/* =====================================================================
 * 第 0 部分：并查集（Disjoint Set Union, DSU）
 * =====================================================================
 *
 * 并查集只干两件事，名字就是这两件事：
 *   find(x)  ：x 属于哪一堆？（返回那一堆的「代表元」，实现上就是树根）
 *   union(a,b)：把 a、b 所在的两堆合成一堆
 *
 * 内存布局（n = 6，一开始每个点各自一堆）：
 *
 *   下标 x ：    0     1     2     3     4     5
 *   fa[x] ： [   0,    1,    2,    3,    4,    5  ]   ← fa[x]==x ⇒ x 就是根
 *   rnk[x]： [   0,    0,    0,    0,    0,    0  ]
 *
 *   union(0,2) 之后（把 2 挂到 0 下面；按秩合并时谁高谁当爹）：
 *   fa[x] ： [   0,    1,    0,    3,    4,    5  ]   ← 2 的父亲是 0
 *   rnk[x]： [   0,    0,    0,    0,    0,    0  ]   ← 两棵秩都是 0，挂完高度没变，还是 0
 *   此时 find(2)：2 → 0（0 是根）⇒ 代表元是 0。2 和 0 同堆。
 *
 *   继续 union(0,2)（也就是再问一次「2 和 0 能合并吗」）：
 *   find(2)=0, find(0)=0 ⇒ 同一个根 ⇒ 返回 0「本来就是一家人」。
 *   ★ 在 Kruskal 里，这个「本来就是一家人」就等于「收下这条边会成环」！
 *
 * 两个让它飞起来的关键优化（缺一不可，各管一件事）：
 *
 *  1) 按秩合并（union by rank）：union 时让【矮树】挂到【高树】下面。
 *     为什么？如果把高树整棵挂到矮树下面，树的高度会 +1；反过来挂高度不变。
 *     这样 n 个点的树高度最多 log2(n)，find 也就最多爬 log2(n) 跳 —— 从「可能退化成一条链」
 *     变成「保证是棵矮树」。代价：要额外一个 rnk[] 数组，只在根的位置有意义。
 *     （严格说 rnk 是高度的【上界】，因为我们压缩路径时会改 fa 而不回退 rnk，
 *       但用它来控制树高完全正确，教科书都这么叫，这里也沿用「秩」。）
 *
 *  2) 路径压缩（path compression）：find 爬到根之后，回头把这一路上每个点【直接挂到根】。
 *     为什么？同一个点往往会被反复问，第一次问完就该让它「以后一跳到位」。
 *     fa: 3→2→1→0  压缩后变成  fa: 3→0, 2→0, 1→0
 *     单个操作摊下来几乎是常数时间（阿克曼函数的反函数 α(n)，实际 n 再大也 ≤ 4）。
 *
 *  两个一起用：m 次操作摊还 O(m·α(n))；只用按秩合并：O(m·log n)；只用压缩（不按秩）：O(m·log n)；
 *  两个都不用（还总是把高的挂到矮下面）：可以退化到 O(m·n)，链一长就爬死人。
 */
void dsu_init(DSU *d, int n)
{
    d->n = n > MAX_V ? MAX_V : n;
    for (int i = 0; i < d->n; i++) {
        d->fa[i] = i;       /* 每个点自己就是一堆：自己是自己的爹 */
        d->rnk[i] = 0;      /* 单点树高度 0 */
    }
}

/*
 * find：先爬到根，再回头把路径压平。
 *
 * 这里刻意写成【两段循环】而不是递归，理由有两个：
 *   1) 递归版（return fa[x]==x ? x : fa[x]=find(fa[x]);）短是短，但压缩发生在回溯过程中，
 *      看代码不容易看出「哪些点被改了」；两段循环可以一行行指给你看。
 *   2) 链很长时递归深度也长，写成循环更稳。
 */
int dsu_find(DSU *d, int x)
{
    if (x < 0 || x >= d->n) {
        printf("[错误] dsu_find 下标 %d 越界（n=%d）\n", x, d->n);
        return -1;
    }

    int root = x;
    while (d->fa[root] != root) {
        root = d->fa[root];             /* 第一段：一路往上爬到根 */
    }

    int cur = x;
    while (d->fa[cur] != cur) {         /* 第二段：路上每个点改成直接指向根 */
        int next = d->fa[cur];
        d->fa[cur] = root;
        cur = next;
    }
    return root;
}

/*
 * 只读探测：从 x 往上爬，数一共爬了几跳，【不修改任何数组】。
 * 存在的意义就是给 main.c 演示「压缩前 4 跳、压缩后 1 跳」这个对比。
 * 生产代码里不需要它。
 */
int dsu_find_probe(const DSU *d, int x, int *hops)
{
    int root = x;
    int h = 0;
    while (d->fa[root] != root) {
        root = d->fa[root];
        h++;
    }
    if (hops) {
        *hops = h;
    }
    return root;
}

/*
 * union：把 a、b 两堆合并。
 * 返回 1 = 真的合并了两个不同的堆；返回 0 = 它俩本来就在一个堆里。
 * 在 Kruskal 里，返回值 0 就是「成环」的判据，比打印出来还重要。
 */
int dsu_union(DSU *d, int a, int b)
{
    int ra = dsu_find(d, a);
    int rb = dsu_find(d, b);
    if (ra == rb) {
        return 0;                       /* 同一个根 ⇒ 已经连通 ⇒ 再加边就成环 */
    }
    /* 按秩合并：矮树挂到高树下面，这样树高不涨 */
    if (d->rnk[ra] < d->rnk[rb]) {
        d->fa[ra] = rb;
    } else if (d->rnk[ra] > d->rnk[rb]) {
        d->fa[rb] = ra;
    } else {
        d->fa[rb] = ra;                 /* 一样高：随便选一个当爹，但新树高度要 +1 */
        d->rnk[ra]++;
    }
    return 1;
}

int dsu_same(DSU *d, int a, int b)
{
    return dsu_find(d, a) == dsu_find(d, b);
}

void dsu_print(const DSU *d, const char *title)
{
    printf("      %s\n", title);
    printf("        下标  :");
    for (int i = 0; i < d->n; i++) {
        printf("%5d", i);
    }
    printf("\n        fa[]  :");
    for (int i = 0; i < d->n; i++) {
        printf("%5d", d->fa[i]);
    }
    printf("\n        rnk[] :");
    for (int i = 0; i < d->n; i++) {
        printf("%5d", d->rnk[i]);
    }
    printf("   （fa[x]==x 的就是根；rnk 只在根的位置有意义）\n");
}

/* =====================================================================
 * 第 1 部分：图（只存边表）
 * =====================================================================
 *
 * Kruskal 的内存需求小到令人感动：一个数组的边而已。
 *
 *   Graph g;  g.n = 6, g.m = 9
 *   g.e[]:  [ {0,1,7} {0,2,9} {0,5,14} {1,2,10} {1,3,15} {2,3,11} {2,5,2} {3,4,6} {4,5,9} ]
 *      下标    0        1        2         3         4         5        6       7        8
 *
 *   不需要邻接矩阵（那是 Prim 的菜），不需要邻接表（那是 BFS/Dijkstra 的菜）。
 *   正因为只碰边表，Kruskal 的耗时只跟【边数】有关：O(E log E) 排序 + O(E·α) 并查集。
 *   这就是「稀疏图（E 接近 V）找 Kruskal，稠密图（E 接近 V^2）找 Prim」的由来：
 *   Prim 的 O(V^2) 不看边数，边再多也不影响它扫矩阵。
 */
void graph_init(Graph *g, int n)
{
    g->n = n;
    g->m = 0;
    memset(g->e, 0, sizeof(g->e));
}

void graph_add_edge(Graph *g, int u, int v, int w)
{
    if (u < 0 || u >= g->n || v < 0 || v >= g->n || u == v) {
        printf("[错误] 边 %d-%d 不合法（n=%d，禁止自环）\n", u, v, g->n);
        return;
    }
    if (g->m >= MAX_E) {
        printf("[错误] 边数已达上限 %d，加不下 %d-%d 了\n", MAX_E, u, v);
        return;
    }
    g->e[g->m].u = u;
    g->e[g->m].v = v;
    g->e[g->m].w = w;
    g->m++;
    /* 注意：这里【没有】把 (v,u) 再加一遍。无向图在 Kruskal 里存一条就够，
     * 因为并查集判的是「两端是否已连通」，和方向无关。 */
}

void graph_print_edges(const Graph *g, const char *title)
{
    printf("  %s（%d 个顶点，%d 条边）：\n", title, g->n, g->m);
    for (int i = 0; i < g->m; i++) {
        printf("      边表第 %d 条:  %d -- %d   权 %d\n", i, g->e[i].u, g->e[i].v, g->e[i].w);
    }
}

/*
 * 全仓库共用图：
 *   主链 (0)-7-(1)-10-(2)-11-(3)-6-(4)-9-(5)，跨边 0-2:9, 0-5:14, 1-3:15, 2-5:2
 *   最小生成树 = {0-1(7), 0-2(9), 2-5(2), 3-4(6), 4-5(9)}，总权 33（不是 35，见 README）
 */
void graph_build_shared(Graph *g)
{
    graph_init(g, 6);
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

/* 三角形：三条边都是 1 ⇒ MST 不唯一。Kruskal 会收下边表里靠前的两条（0-1、1-2）并跳过 0-2；
 * 隔壁 Prim 会收 0-1、0-2。总权同为 2，边集不同 —— 这个对照就是本节第 5 幕的主角。 */
void graph_build_ties(Graph *g)
{
    graph_init(g, 3);
    graph_add_edge(g, 0, 1, 1);
    graph_add_edge(g, 1, 2, 1);
    graph_add_edge(g, 0, 2, 1);
}

/* 不连通：{0,1,2} 和 {3,4} 两块。Kruskal【不会报错】，它会安静地给出「最小生成森林」
 * （两块各一棵树，边数 3 < V-1 = 4）。这是本节坑第 1 条。 */
void graph_build_disconnected(Graph *g)
{
    graph_init(g, 5);
    graph_add_edge(g, 0, 1, 3);
    graph_add_edge(g, 1, 2, 4);
    graph_add_edge(g, 3, 4, 5);
}

/*
 * 稍大的图（8 个点）：一个 4 元环 + 4 片叶子 + 2 条贵边。
 *
 *      (0) ──1── (1)              环上的 4 条边权全是 1
 *       │         │
 *       1         1               叶子边 0-4、1-5、2-6、3-7 权全是 5
 *       │         │               贵边   4-5、6-7   权全是 9
 *      (3) ──1── (2)              叶子：4 挂在 0 上、5 挂在 1 上、6 挂在 2 上、7 挂在 3 上
 *
 *   权威边表（图形画不准的时候以它为准）：
 *     环：0-1(1) 1-2(1) 2-3(1) 3-0(1)
 *     叶：0-4(5) 1-5(5) 2-6(5) 3-7(5)
 *     贵：4-5(9) 6-7(9)
 *
 *   每片叶子只有一条边可走 ⇒ 4 条叶子边【必须】全收 = 20；
 *   环上 4 条边只能收 3 条（收满 4 条就成环）= 3；两条贵边一定成环，白看。
 *   ⇒ MST 总权 = 23，边数 = 7 = V-1
 *
 * tie_order 只改变【四条同权环边在边表里的先后】，图本身一模一样。
 * 因为插入排序是稳定的，同权边会保持边表顺序 ⇒ 最后被跳过的那条环边会跟着变：
 *   tie_order = 0：顺序 0-1,1-2,2-3,3-0 ⇒ 跳过 3-0
 *   tie_order = 1：顺序 2-3,3-0,0-1,1-2 ⇒ 跳过 1-2
 * 两棵 MST 总权都是 23 —— 「同一张图、同一个算法、不同边表顺序 ⇒ 不同边集」的铁证。
 */
void graph_build_big(Graph *g, int tie_order)
{
    graph_init(g, 8);
    if (tie_order == 0) {
        graph_add_edge(g, 0, 1, 1);
        graph_add_edge(g, 1, 2, 1);
        graph_add_edge(g, 2, 3, 1);
        graph_add_edge(g, 3, 0, 1);
    } else {
        graph_add_edge(g, 2, 3, 1);
        graph_add_edge(g, 3, 0, 1);
        graph_add_edge(g, 0, 1, 1);
        graph_add_edge(g, 1, 2, 1);
    }
    graph_add_edge(g, 0, 4, 5);
    graph_add_edge(g, 1, 5, 5);
    graph_add_edge(g, 2, 6, 5);
    graph_add_edge(g, 3, 7, 5);
    graph_add_edge(g, 4, 5, 9);
    graph_add_edge(g, 6, 7, 9);
}

/* =====================================================================
 * 第 2 部分：手写插入排序
 * =====================================================================
 *
 * 为什么不用 qsort？因为 qsort 的原型是
 *     void qsort(void *base, size_t n, size_t sz, int (*cmp)(const void *, const void *));
 * 它要你把【比较函数】当参数传进去 —— 也就是函数指针。本项目的硬约定是「不用函数指针」，
 * 而且 qsort 还要 void* + 强制类型转换，两样都被禁了。
 * 所以老老实实手写一个：教学场景里边数只有 10 条上下，插入排序 O(E^2) 完全够，
 * 而且它【稳定】：权值相同的边会保持边表原有顺序，让输出可以逐字复现。
 *
 * 插入排序一句话：把手里的牌插进左边已经排好的堆里 —— 从后往前找到该插的位置，
 * 一路上把比它大的元素往后挪一格。
 *
 *   初始  [ 9, 2, 7, 6 ]
 *   i=1   [ 9, 9, 7, 6 ]  把 2 往后挪 → [ 2, 9, 7, 6 ]
 *   i=2   [ 2, 9, 9, 6 ]  把 7 往后挪 → [ 2, 7, 9, 6 ]
 *   i=3   [ 2, 7, 9, 9 ]  →          [ 2, 7, 6, 9 ] → [ 2, 6, 7, 9 ]
 */
void insertion_sort_edges(WEdge a[], int n)
{
    for (int i = 1; i < n; i++) {
        WEdge key = a[i];                 /* 手里这张新牌 */
        int j = i - 1;
        /* 比 key 大的统统往后挪一格。用 > 而不是 >= —— 这就是「稳定」的来源：
         * 等于 key 的边不挪，所以同权边的相对顺序不变。 */
        while (j >= 0 && a[j].w > key.w) {
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;                   /* 插到最后一个「不比它大」的边后面 */
    }
}

void sort_edges_copy(const Graph *g, WEdge out[])
{
    for (int i = 0; i < g->m; i++) {
        out[i] = g->e[i];
    }
    insertion_sort_edges(out, g->m);
}

/* =====================================================================
 * 第 3 部分：Kruskal 主函数
 * =====================================================================
 *
 * 全过程只有一个循环 + 一次 find：
 *
 *   把所有边按权排序
 *   for 每条边 (u, v, w)：
 *       if find(u) != find(v):        ← 「u、v 不在同一堆」=「收下它不成环」
 *           收下这条边
 *           union(u, v)               ← 把两堆合成一堆
 *       else:
 *           跳过（它俩已经通过别的路连上了，再加就是多余的一条边 ⇒ 成环）
 *
 * 为什么并查集判环这么优雅？
 *   因为「加一条边会不会成环」的充要条件就是「这条边的两个端点是否已经连通」。
 *   而在森林（已收的边必然无环）里，「连通」= 「在同一棵树里」= 「find 到同一个根」。
 *   于是一个本来要想「怎么走一遍看能不能绕回来」的问题（DFS 判环，O(V+E) 一条边），
 *   变成了两次数组查找（摊还近乎 O(1)）。整个 Kruskal 的判环部分一行代码。
 */
int kruskal_mst(const Graph *g, MSTEdge out[], int *cnt_out, int show_steps)
{
    WEdge sorted[MAX_E];
    DSU dsu;
    int cnt = 0;
    int total = 0;

    sort_edges_copy(g, sorted);
    dsu_init(&dsu, g->n);              /* 必须先初始化再打印：否则打印的是栈上的垃圾值 */

    if (show_steps) {
        printf("      第 1 步：边按权排序（插入排序，稳定 ⇒ 同权边保持边表里的先后）\n");
        printf("        排序后顺序：");
        for (int i = 0; i < g->m; i++) {
            printf("%d-%d(%d) ", sorted[i].u, sorted[i].v, sorted[i].w);
        }
        printf("\n      第 2 步：按这个顺序逐条试。并查集的初始状态（每个点各自一堆）：\n");
        dsu_print(&dsu, "初始并查集：");
    }

    int skipped = 0;
    for (int i = 0; i < g->m; i++) {
        int u = sorted[i].u;
        int v = sorted[i].v;
        int w = sorted[i].w;

        /* 先只查不改，是为了把「find 结果」打印清楚；下面再真正 union。
         * （写成 if (dsu_union(...)) 一步到位也行，那样就得在 union 内部打印了。） */
        int ru = dsu_find(&dsu, u);
        int rv = dsu_find(&dsu, v);

        if (show_steps) {
            printf("      [%2d/%d] 看边 %d-%d（权 %d）：find(%d)=%d, find(%d)=%d → ",
                   i + 1, g->m, u, v, w, u, ru, v, rv);
        }

        if (ru != rv) {
            out[cnt].u = u;
            out[cnt].v = v;
            out[cnt].w = w;
            cnt++;
            total += w;
            dsu_union(&dsu, u, v);
            if (show_steps) {
                printf("不同堆 ⇒ 【收下】（已收 %d 条，累计 %d）\n", cnt, total);
            }
            if (cnt == g->n - 1) {
                if (show_steps) {
                    printf("      已收满 V-1 = %d 条 ⇒ 生成树完成，剩下的边不用再看\n", g->n - 1);
                }
                break;
            }
        } else {
            skipped++;
            if (show_steps) {
                printf("同堆（根都是 %d）⇒ 【跳过（会成环）】，累计不变 = %d\n", ru, total);
            }
        }
    }

    if (cnt_out) {
        *cnt_out = cnt;
    }
    if (show_steps) {
        printf("      一共看了 %d / %d 条边：收下 %d 条，跳过 %d 条（跳过 = 两端早已连通）\n",
               skipped + cnt, g->m, cnt, skipped);
        if (cnt == g->n - 1) {
            printf("      提前收满 V-1 条就停手了，后面 %d 条边根本没看 —— 这也是 Kruskal 快的原因之一\n",
                   g->m - (skipped + cnt));
        }
        if (cnt != g->n - 1) {
            printf("      ⚠ 收的边数 %d != V-1 = %d ⇒ 这张图【不连通】，得到的是最小生成森林，不是生成树！\n",
                   cnt, g->n - 1);
        }
        dsu_print(&dsu, cnt == g->n - 1
                  ? "结束时的并查集（全图连通 ⇒ 只剩一个根；fa[] 被路径压缩压得很扁）："
                  : "结束时的并查集（【还有好几个根】= 图被切成了好几块，这就是森林的样子）：");
    }
    return total;
}

/* =====================================================================
 * 第 4 部分：打印、自检、Prim 对照
 * ===================================================================== */
void mst_print_edges(const MSTEdge edges[], int cnt, const char *title)
{
    printf("  %s（共 %d 条边）：\n", title, cnt);
    int sum = 0;
    for (int i = 0; i < cnt; i++) {
        int a = edges[i].u < edges[i].v ? edges[i].u : edges[i].v;
        int b = edges[i].u < edges[i].v ? edges[i].v : edges[i].u;
        printf("      第 %d 条:  %d <-> %d   权 %d\n", i + 1, a, b, edges[i].w);
        sum += edges[i].w;
    }
    printf("      边权求和 = %d\n", sum);
}

/* 把两条 MST 摆一起：总权同不同？边集同不同？（两个答案组合起来才有信息量） */
void mst_compare(const MSTEdge a[], int na, const MSTEdge b[], int nb,
                 const char *la, const char *lb)
{
    int sa = 0, sb = 0;
    for (int i = 0; i < na; i++) {
        sa += a[i].w;
    }
    for (int i = 0; i < nb; i++) {
        sb += b[i].w;
    }
    printf("    %-12s 边数 %d  总权 %d   边集:", la, na, sa);
    for (int i = 0; i < na; i++) {
        printf(" %d-%d", a[i].u < a[i].v ? a[i].u : a[i].v, a[i].u < a[i].v ? a[i].v : a[i].u);
    }
    printf("\n    %-12s 边数 %d  总权 %d   边集:", lb, nb, sb);
    for (int i = 0; i < nb; i++) {
        printf(" %d-%d", b[i].u < b[i].v ? b[i].u : b[i].v, b[i].u < b[i].v ? b[i].v : b[i].u);
    }
    printf("\n");

    int same_edges = (na == nb);
    for (int i = 0; same_edges && i < na; i++) {
        int ai = a[i].u < a[i].v ? a[i].u : a[i].v;
        int bi = a[i].u < a[i].v ? a[i].v : a[i].u;
        int found = 0;
        for (int j = 0; j < nb; j++) {
            int aj = b[j].u < b[j].v ? b[j].u : b[j].v;
            int bj = b[j].u < b[j].v ? b[j].v : b[j].u;
            if (ai == aj && bi == bj) {
                found = 1;
            }
        }
        if (!found) {
            same_edges = 0;
        }
    }
    if (sa == sb) {
        printf("    ⇒ 总权相同 ✓（这才是算法正确的证据）；边集%s。\n",
               same_edges ? "也相同" : "不同 —— 完全正常：MST 不唯一时两种贪心各挑各的");
    } else {
        printf("    ⇒ 总权不同 ✗ 有一处实现写错了！同图 MST 总权必须唯一。\n");
    }
}

/*
 * 自检三件事，和 prim-mst 那节一样：边数够不够、边真不真存在、连不连通且无环。
 * 这里判环就用本节的并查集 —— 顺便让你看看它在「非算法场景」里怎么用。
 */
int mst_check_valid(const Graph *g, const MSTEdge edges[], int cnt)
{
    int ok = 1;

    if (cnt != g->n - 1) {
        printf("      自检 ✗ 边数 = %d，生成树要求 V-1 = %d\n", cnt, g->n - 1);
        ok = 0;
    }

    for (int i = 0; i < cnt; i++) {
        int found = 0;
        for (int j = 0; j < g->m; j++) {
            int a = edges[i].u, b = edges[i].v;
            if ((g->e[j].u == a && g->e[j].v == b) || (g->e[j].u == b && g->e[j].v == a)) {
                if (g->e[j].w == edges[i].w) {
                    found = 1;
                }
            }
        }
        if (!found) {
            printf("      自检 ✗ 边 %d-%d（权 %d）在原图里不存在（幻觉边或权重写错了）\n",
                   edges[i].u, edges[i].v, edges[i].w);
            ok = 0;
        }
    }

    DSU d;
    dsu_init(&d, g->n);
    for (int i = 0; i < cnt; i++) {
        if (!dsu_union(&d, edges[i].u, edges[i].v)) {
            printf("      自检 ✗ 边 %d-%d 两端已连通 ⇒ 出现环，这不是生成树\n",
                   edges[i].u, edges[i].v);
            ok = 0;
        }
    }
    for (int v = 1; v < g->n; v++) {
        if (!dsu_same(&d, 0, v)) {
            printf("      自检 ✗ 顶点 %d 和顶点 0 没连通\n", v);
            ok = 0;
            break;
        }
    }
    if (ok) {
        printf("      自检 ✓ 边数 = V-1、每条边真实存在、连通、无环 ⇒ 合法的生成树\n");
    }
    return ok;
}

/*
 * 极简 Prim（邻接矩阵 + 扫最小 key），只为和本节的 Kruskal 结果对账。
 * 本目录不许 include 隔壁 prim-mst 的文件，所以这里自带 25 行版本；
 * 详细讲解在 07-graph/prim-mst/README.md。
 */
int mini_prim(const Graph *g, int start, MSTEdge out[], int *cnt_out)
{
    int mat[MAX_V][MAX_V];
    int in_wall[MAX_V];
    int key[MAX_V];
    int from[MAX_V];

    for (int i = 0; i < g->n; i++) {
        for (int j = 0; j < g->n; j++) {
            mat[i][j] = INF_W;
        }
    }
    for (int i = 0; i < g->m; i++) {       /* 边表 → 矩阵：无向就对称写两格 */
        int u = g->e[i].u;
        int v = g->e[i].v;
        if (g->e[i].w < mat[u][v]) {
            mat[u][v] = g->e[i].w;
            mat[v][u] = g->e[i].w;
        }
    }
    for (int v = 0; v < g->n; v++) {
        in_wall[v] = 0;
        key[v] = INF_W;
        from[v] = -1;
    }
    if (start < 0 || start >= g->n) {
        start = 0;
    }
    key[start] = 0;

    int cnt = 0;
    int total = 0;
    for (int round = 0; round < g->n; round++) {
        int u = -1;
        for (int v = 0; v < g->n; v++) {
            if (!in_wall[v] && key[v] < INF_W && (u == -1 || key[v] < key[u])) {
                u = v;
            }
        }
        if (u == -1) {
            break;                         /* 不连通：剩下的点没有门票 */
        }
        in_wall[u] = 1;
        if (from[u] != -1) {
            out[cnt].u = from[u];
            out[cnt].v = u;
            out[cnt].w = mat[from[u]][u];
            total += out[cnt].w;
            cnt++;
        }
        for (int v = 0; v < g->n; v++) {
            if (!in_wall[v] && mat[u][v] < key[v]) {
                key[v] = mat[u][v];
                from[v] = u;
            }
        }
    }
    if (cnt_out) {
        *cnt_out = cnt;
    }
    return total;
}
