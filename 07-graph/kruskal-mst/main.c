#include <stdio.h>
#include "kruskal.h"

static void section(const char *title)
{
    printf("\n==================================================\n");
    printf("%s\n", title);
    printf("==================================================\n");
}

/* 打印一行「排序前 / 排序后」的边表，用来肉眼确认插入排序做了什么 */
static void print_edge_line(const WEdge a[], int n)
{
    for (int i = 0; i < n; i++) {
        printf("%d-%d(%d)%s", a[i].u, a[i].v, a[i].w, i + 1 == n ? "" : "  ");
    }
    printf("\n");
}

int main(void)
{
    /* ============================================================
       第 1 幕：图 + 概念
       ============================================================ */
    section("幕 1 · Kruskal 只要一张边表");

    Graph g;
    graph_build_shared(&g);
    graph_print_edges(&g, "共用示例图");

    printf("\n  主链：(0) ──7── (1) ──10── (2) ──11── (3) ──6── (4) ──9── (5)\n");
    printf("  跨点：(0) ──9── (2)   (0) ──14── (5)   (1) ──15── (3)   (2) ──2── (5)\n");
    printf("\n  Kruskal 三步走：\n");
    printf("    ① 边按权从小到大排序\n");
    printf("    ② 从最便宜的边开始试：收下它会不会成环？不会就收\n");
    printf("    ③ 收满 V-1 条停手\n");
    printf("  「会不会成环」= 「两个端点是不是已经连通了」= 并查集的 find 一次搞定。\n");

    /* ============================================================
       第 2 幕：手写插入排序
       ============================================================ */
    section("幕 2 · 第 ① 步排序：为什么手写、为什么是插入排序");

    WEdge sorted[MAX_E];
    sort_edges_copy(&g, sorted);

    printf("  排序前（= 边表顺序，也就是你 add_edge 的先后）：\n      ");
    print_edge_line(g.e, g.m);
    printf("  排序后（按权升序）：\n      ");
    print_edge_line(sorted, g.m);

    printf("\n  两个细节：\n");
    printf("    1) 不用 qsort：qsort 要你传一个【函数指针】当比较器，还要 void* 强转 —— 本项目两条都禁用。\n");
    printf("       这里只有 %d 条边，手写插入排序 O(E^2) 毫无压力，而且看得懂。\n", g.m);
    printf("    2) 插入排序【稳定】（比较写成 a[j].w > key.w，等于不挪），所以同权边的先后 = 边表先后。\n");
    printf("       注意 0-2(9) 和 4-5(9) 同权：排序后 0-2 仍排在 4-5 前面，因为边表里它俩就是这个顺序。\n");
    printf("       这一点后面第 5、6 幕会看到后果：同权边谁先被试，会决定最后收的是哪一条。\n");

    /* ============================================================
       第 3 幕：Kruskal 逐边过程（本节最重要的画面）
       ============================================================ */
    section("幕 3 · 逐条边：find 结果 + 收下 / 跳过（会成环）");

    MSTEdge mst[MAX_E];
    int cnt = 0;
    int total = kruskal_mst(&g, mst, &cnt, 1);

    printf("\n  kruskal_mst() 返回总权 = %d，收下 %d 条边\n", total, cnt);

    /* ============================================================
       第 4 幕：结果 + 自检 + 和 Prim 对账
       ============================================================ */
    section("幕 4 · 最终 MST / 合法性自检 / 与 Prim 对账");

    mst_print_edges(mst, cnt, "Kruskal 的最小生成树");
    mst_check_valid(&g, mst, cnt);

    printf("\n  手算核对：\n");
    printf("    权 2 的 2-5：两端还没连通 ⇒ 收。\n");
    printf("    权 6 的 3-4：收。\n");
    printf("    权 7 的 0-1：收。（此刻三堆：{0,1} {2,5} {3,4}）\n");
    printf("    权 9 的 0-2：把 {0,1} 和 {2,5} 连起来 ⇒ 收。   权 9 的 4-5：把 {3,4} 和 {0,1,2,5} 连上 ⇒ 收。\n");
    printf("    到这里已经 5 条 = V-1，剩下的 10、11、14、15 要么成环要么用不上。\n");
    printf("    ⇒ 2 + 6 + 7 + 9 + 9 = 33。\n");

    MSTEdge pmst[MAX_E];
    int pcnt = 0;
    int ptotal = mini_prim(&g, 0, pmst, &pcnt);

    printf("\n  隔壁 Prim（本目录自带的 mini_prim）结果对照：\n");
    mst_compare(mst, cnt, pmst, pcnt, "Kruskal", "Prim");
    int psum = 0;
    for (int i = 0; i < pcnt; i++) {
        psum += pmst[i].w;
    }
    printf("  mini_prim 的返回值 = %d，把它的边集再加一遍 = %d（两个数一致才说明它自己没算错）\n",
           ptotal, psum);
    printf("  两种算法总权相同 = 33 ⇒ 互为验证。（若这里总权不同，一定是某个实现写错了，\n");
    printf("  因为「最小总权」是图的一个确定属性，与算法无关。）\n");

    /* ============================================================
       第 5 幕：总权相同、边集未必相同
       ============================================================ */
    section("幕 5 · 三角形等权图：同一张图，Kruskal 和 Prim 选了不同的边");

    Graph gt;
    graph_build_ties(&gt);
    graph_print_edges(&gt, "三角形（三条边全是 1）");

    MSTEdge km[MAX_E];
    int kcnt = 0;
    int ktot = kruskal_mst(&gt, km, &kcnt, 1);
    printf("\n  Kruskal 收了 %d 条，总权 %d：", kcnt, ktot);
    for (int i = 0; i < kcnt; i++) {
        printf(" %d-%d(%d)", km[i].u, km[i].v, km[i].w);
    }
    printf("\n");

    MSTEdge pm[MAX_E];
    int pc = 0;
    int pt = mini_prim(&gt, 0, pm, &pc);
    printf("  Prim   收了 %d 条，总权 %d：", pc, pt);
    for (int i = 0; i < pc; i++) {
        printf(" %d-%d(%d)", pm[i].u < pm[i].v ? pm[i].u : pm[i].v,
               pm[i].u < pm[i].v ? pm[i].v : pm[i].u, pm[i].w);
    }
    printf("（Prim 的墙从 0 开始扩，所以 0-1、0-2 先被看到）\n");

    printf("\n  mst_compare：\n");
    mst_compare(km, kcnt, pm, pc, "Kruskal", "Prim");

    printf("\n  ★ 为什么「总权相同、边集未必相同」是完全正常的？\n");
    printf("    三角形里任选两条边都是「覆盖 3 个点、2 条边、无环」的生成树，总权都是 2。\n");
    printf("    MST 的定义只要求【总权最小】，从没要求【边集唯一】。只要图里存在两条权重相同、\n");
    printf("    又可互换的边（专业说法：它们在同一个环上且都是环上最小），MST 就不唯一。\n");
    printf("    Prim 的取舍发生在「多个墙外点门票相同」时，Kruskal 的取舍发生在「同权边谁排在边表前面」时，\n");
    printf("    两套 tie-break 规则不同 ⇒ 边集可能不同。判分只要总权，所以两者都算对。\n");
    printf("    推论：题目若问「MST 是否唯一」，就去看有没有【可互换的同权边】；\n");
    printf("    一条判据：把所有边权看成「同权分组」，若某组边在某一个环上都是该组最小 ⇒ 不唯一。\n");
    printf("    特例：图中所有边权【两两不同】⇒ MST 唯一（因为交换性质再无适用余地）。\n");

    /* ============================================================
       第 6 幕：边表顺序如何影响结果
       ============================================================ */
    section("幕 6 · 8 点图：只改同权边在边表里的顺序，MST 边集会变、总权不变");

    Graph gb, gc;
    graph_build_big(&gb, 0);
    graph_build_big(&gc, 1);

    MSTEdge bm[MAX_E];
    int bcnt = 0;
    int btot = kruskal_mst(&gb, bm, &bcnt, 1);
    printf("\n  tie_order = 0：边表里环的顺序是 0-1,1-2,2-3,3-0 → 总权 %d，边数 %d\n", btot, bcnt);
    printf("    边集：");
    for (int i = 0; i < bcnt; i++) {
        printf(" %d-%d(%d)", bm[i].u, bm[i].v, bm[i].w);
    }
    printf("\n    被跳过的那条环边是 3-0（最后一条，此时 3 和 0 已经通过 3-2-1-0 连通）\n");

    MSTEdge cm[MAX_E];
    int ccnt = 0;
    int ctot = kruskal_mst(&gc, cm, &ccnt, 1);
    printf("\n  tie_order = 1：边表里环的顺序是 2-3,3-0,0-1,1-2 → 总权 %d，边数 %d\n", ctot, ccnt);
    printf("    边集：");
    for (int i = 0; i < ccnt; i++) {
        printf(" %d-%d(%d)", cm[i].u, cm[i].v, cm[i].w);
    }
    printf("\n    被跳过的那条环边变成了 1-2\n");

    printf("\n  mst_compare：\n");
    mst_compare(bm, bcnt, cm, ccnt, "tie_order=0", "tie_order=1");
    printf("  这张图结构上很好核对：4 片叶子（0-4、1-5、2-6、3-7，权各 5）非收不可，共 20；\n");
    printf("  4 元环 0-1-2-3-0 每条权 1，只能留 3 条，共 3 ⇒ 总权 23。两条权 9 的边（4-5、6-7）必成环，白看。\n");

    /* ============================================================
       第 7 幕：并查集单独拆开看
       ============================================================ */
    section("幕 7 · 并查集拆解：路径压缩 vs 按秩合并，各解决什么问题");

    DSU d;
    dsu_init(&d, 5);
    printf("  初始：\n");
    dsu_print(&d, "初始并查集");

    printf("\n  ① 按秩合并：每次都让矮树挂到高树下面，保证树高 <= log2(n)\n");
    int r1 = dsu_union(&d, 0, 1);
    printf("  union(0,1) 返回 %d（1 = 真合并了两个不同的堆）：两棵秩 0 的树并起来，新根的秩变成 1\n", r1);
    int r2 = dsu_union(&d, 2, 3);
    printf("  union(2,3) 返回 %d\n", r2);
    dsu_print(&d, "union(0,1) + union(2,3) 之后：");
    int r3 = dsu_union(&d, 0, 2);
    printf("  union(0,2) 返回 %d：两棵秩都是 1 的树 ⇒ 任挑一个当爹，当爹的秩再 +1\n", r3);
    dsu_print(&d, "再 union(0,2) 之后：");
    int again = dsu_union(&d, 1, 3);
    printf("  union(1,3) 返回 %d ⇒ 它俩 find 到同一个根 0，本来就是一家人。"
       "在 Kruskal 里这个返回值 0 就等于「收下这条边会成环」\n", again);
    dsu_print(&d, "尝试 union(1,3) 之后（什么都没改）：");

    printf("\n  ② 路径压缩：只数跳数、不改结构的 dsu_find_probe()，和真正的 dsu_find() 一比\n");
    DSU chain;
    dsu_init(&chain, 5);
    /* 手搓一条最难受的链：0 → 1 → 2 → 3 → 4（故意绕开按秩合并，模拟「只 union 不 rank」的最坏情况）*/
    chain.fa[0] = 1;
    chain.fa[1] = 2;
    chain.fa[2] = 3;
    chain.fa[3] = 4;
    dsu_print(&chain, "手搓的链式 fa[]（故意绕开按秩合并，模拟最坏情况）：");

    int hops_before = 0;
    int root_b = dsu_find_probe(&chain, 0, &hops_before);
    printf("  dsu_find_probe(0)：根 = %d，爬了 %d 跳（只读，fa[] 没变）\n", root_b, hops_before);
    dsu_print(&chain, "probe 之后（fa[] 一模一样，因为它只看不改）：");

    int root_a = dsu_find(&chain, 0);
    printf("  dsu_find(0)：根 = %d ⇒ 顺手把 0、1、2、3 全挂到根上\n", root_a);
    dsu_print(&chain, "压缩之后：0/1/2/3 全直接挂到根 4，以后谁再问都是 1 跳：");
    int hops_after = 0;
    (void)dsu_find_probe(&chain, 0, &hops_after);
    printf("  再 probe(0)：爬了 %d 跳（压缩前 %d 跳 → 压缩后 %d 跳，这就是路径压缩的全部价值）\n",
           hops_after, hops_before, hops_after);

    printf("\n  两个优化各治一种病：\n");
    printf("    按秩合并  ：防止【构造时】把树挂歪（保证任何时刻树高 <= log n）\n");
    printf("    路径压缩  ：消灭【重复查询】的开销（问过的点以后一跳到位）\n");
    printf("    一起用：m 次操作摊还 O(m · α(n))，α 是阿克曼反函数，实际永远 <= 4，等于常数。\n");

    /* ============================================================
       第 8 幕：不连通图（Kruskal 的「安静失败」）
       ============================================================ */
    section("幕 8 · 图不连通：Kruskal 不报错，只给你一片「森林」");

    Graph gd;
    graph_build_disconnected(&gd);
    graph_print_edges(&gd, "两块互不相连的图 {0,1,2} + {3,4}");

    MSTEdge dm[MAX_E];
    int dcnt = 0;
    int dtot = kruskal_mst(&gd, dm, &dcnt, 1);
    printf("\n  总权 = %d，边数 = %d，而 V-1 = %d ⇒ 少了一条！\n", dtot, dcnt, gd.n - 1);
    printf("  ★ 关键坑：kruskal_mst() 在这种图上【不会报错】，它照样返回一个总权。\n");
    printf("    必须自己判断 cnt == n - 1；不等就说明图不连通，你拿到的是最小生成森林。\n");
    printf("    （隔壁 prim-mst 的写法是返回 -1，两种风格都要在调用侧检查，别裸用返回值。）\n");
    mst_check_valid(&gd, dm, dcnt);
    printf("  森林长这样：{0-1, 1-2} 是一棵，{3-4} 是另一棵，两块之间没有边 ⇒ 谁也连不上谁。\n");

    /* ============================================================
       收尾
       ============================================================ */
    section("本节小结");
    printf("  1) Kruskal = 排序 + 逐条边试 + 并查集判环。收满 V-1 条就停。\n");
    printf("  2) 判环为什么优雅：「加边会成环」等价于「两端已连通」，后者就是 find(u)==find(v)，一行。\n");
    printf("  3) 复杂度 O(E log E + E·α(V))，瓶颈在排序；内存只要一张边表 ⇒ 稀疏图首选。\n");
    printf("     稠密图（E ~ V^2）它就吃亏了：排序要 O(V^2 log V)，比 Prim 的 O(V^2) 还慢 ⇒ 用 Prim。\n");
    printf("  4) 排序必须手写（本项目禁函数指针 ⇒ 不能用 qsort 比较器），且要用稳定排序，\n");
    printf("     这样同权边按边表顺序处理，输出可复现。\n");
    printf("  5) 总权唯一、边集未必唯一；Kruskal 与 Prim 总权必须相等，可以互相验证。\n");
    printf("  6) MST ≠ 最短路径树（见 prim-mst 第 5 幕的对照：33 vs 38）。\n");
    printf("  7) 图不连通时 Kruskal 安静地返回森林 ⇒ 一定要检查边数 == V-1。\n");
    return 0;
}
