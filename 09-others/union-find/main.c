#include <stdio.h>
#include "uf.h"

/* 小工具：打印一行分隔标题，让输出好读（全仓库统一手感） */
static void section(const char *title)
{
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n");
}

/* 把 find_naive 抄出来的路径打印成 "7 → 3 → 1 → 0" 这种一眼看懂的形式 */
static void show_path(const int path[], int len, int hops)
{
    printf("        爬的路径：");
    for (int i = 0; i < len; i++) {
        printf("%d", path[i]);
        if (i + 1 < len) {
            printf(" → ");
        }
    }
    printf("        （走了 %d 条边）\n", hops);
}

/* 按秩版 / 朴素版各 build 一遍，统计"每个点都 find 一次总共爬几条边" */
static long long total_hops_all_nodes(UF *uf)
{
    long long sum = 0;
    for (int i = 0; i < uf->n; i++) {
        uf_find_naive(uf, i, NULL, 0, NULL);   /* 用不压缩版，纯量"树本身有多深" */
        sum += uf->walks;
    }
    return sum;
}

int main(void)
{
    UF uf;

    /* ============================================================
       第 1 幕：初始状态 —— 每个人自己一个集合
       ============================================================ */
    section("第 1 幕：初始化 8 个元素，此时 8 个点就是 8 个集合");

    uf_init(&uf, 8);
    printf("      parent[i] == i 就说明 i 是根 ⇒ 现在人人都还是根\n");
    uf_print_parent_array(&uf, "刚 init 完：");

    /* 手动连出三棵小树，方便后面演示 find 怎么爬 */
    printf("\n      做三次 union：(0,1) (2,3) (0,2)\n");
    uf_union(&uf, 0, 1);
    uf_union(&uf, 2, 3);
    uf_union(&uf, 0, 2);
    uf_print_parent_array(&uf, "三次 union 之后：");
    printf("      现在的森林长这样：\n");
    uf_print_forest(&uf);
    printf("      ⇒ 4 个点 {0,1,2,3} 变成一个集合了，另外 4 个 {4,5,6,7} 还是各自独立\n");

    /* ============================================================
       第 2 幕：朴素 find —— 把"往上爬"这个过程看得清清楚楚
       ============================================================ */
    section("第 2 幕：朴素 find，一路往上爬到根，把爬过的路径打印出来");

    int path[UF_MAX_N];
    int plen = 0;
    printf("      find(3)：3 的父亲是 2，2 的父亲是 0，0 就是根\n");
    int root3 = uf_find_naive(&uf, 3, path, UF_MAX_N, &plen);
    show_path(path, plen, (int)uf.walks);
    printf("        ⇒ 根 = %d，所以 3 号所在集合的名字就叫 \"%d 集合\"\n", root3, root3);

    printf("\n      再 find(1)、find(0) 看看它们的根是不是同一个：\n");
    int root1 = uf_find_naive(&uf, 1, path, UF_MAX_N, &plen);
    show_path(path, plen, (int)uf.walks);
    int root0 = uf_find_naive(&uf, 0, path, UF_MAX_N, &plen);
    show_path(path, plen, (int)uf.walks);
    printf("        根相同吗？%d vs %d ⇒ %s\n", root0, root1,
           root0 == root1 ? "相同 ⇒ 1 和 0 是同一个集合" : "不同");

    /* ============================================================
       第 3 幕：一路 union，每步都把整个 parent 数组打出来
       ============================================================ */
    section("第 3 幕：按顺序合并 7 次，每合并一次就打印整个 parent 数组");

    uf_reset(&uf);
    printf("      reset 之后回到 8 个独立集合：\n");
    uf_print_parent_array(&uf, "");

    int edge_count = 7;
    int edges[7][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {4, 6}, {0, 4}};
    for (int e = 0; e < edge_count; e++) {
        int a = edges[e][0], b = edges[e][1];
        int ret = uf_union(&uf, a, b);
        char tag[96];
        snprintf(tag, sizeof(tag), "union(%d, %d) 之后   [返回值 %d：%s]",
                 a, b, ret, ret == 1 ? "真的合并了两个集合" : "本来就是一个集合");
        uf_print_parent_array(&uf, tag);
    }
    printf("      最后所有点都汇成一个集合，森林只有一棵：\n");
    uf_print_forest(&uf);
    printf("      ⇒ 注意 rank 的变化：0 的 rank 从 1 涨到 2 再涨到 3，说明只有'两边一样高'时高度才+1\n");

    /* ============================================================
       第 4 幕：不优化会退化成一条链 —— 这是痛点的现场取证
       ============================================================ */
    section("第 4 幕：【反面教材】完全不做优化，union 顺序稍一不留神就退化成链表");

    uf_reset(&uf);
    for (int i = 0; i + 1 < uf.n; i++) {
        uf_union_naive(&uf, i, i + 1);   /* 每次都把左边那棵的根挂到右边那棵下面 */
    }
    printf("      按 (0,1)(1,2)(2,3)...(6,7) 顺序做不优化的 union 之后：\n");
    uf_print_parent_array(&uf, "");
    printf("      森林（其实是一根'晾衣杆'）：\n");
    uf_print_forest(&uf);
    printf("      树高 = %d（8 个点退化成了 7 层）\n", uf_tree_height(&uf, 7));

    printf("\n      此时 find(0) 有多惨：\n");
    uf_find_naive(&uf, 0, path, UF_MAX_N, &plen);
    show_path(path, plen, (int)uf.walks);

    /* ============================================================
       第 5 幕：路径压缩 —— 爬过的点全部直接挂到根
       ============================================================ */
    section("第 5 幕：路径压缩 —— 还是上面那根晾衣杆，find 一次就把它压扁");

    printf("      压缩前 parent ：");
    for (int i = 0; i < uf.n; i++) {
        printf("%3d", uf.parent[i]);
    }
    printf("\n");
    int root_c = uf_find(&uf, 0);        /* 这一次 find 顺手把 0..6 全挂到根上 */
    printf("      find(0) 用了 %lld 条边爬到根 %d，回来路上把 0,1,2,3,4,5,6 全部直接挂到根\n",
           uf.walks, root_c);
    printf("      压缩后 parent ：");
    for (int i = 0; i < uf.n; i++) {
        printf("%3d", uf.parent[i]);
    }
    printf("\n");
    printf("      压扁后的森林（只剩两层了）：\n");
    uf_print_forest(&uf);
    printf("      现在再 find(5)：\n");
    uf_find_naive(&uf, 5, path, UF_MAX_N, &plen);
    show_path(path, plen, (int)uf.walks);
    printf("      ⇒ 代价从 7 条边降到 1 条边。压缩是一次性花费：第一次贵，之后全便宜\n");

    /* ============================================================
       第 6 幕：按秩合并 —— 干脆不让树长高
       ============================================================ */
    section("第 6 幕：按秩合并 —— 同样的边顺序，用 uf_union 得到的树完全不同");

    UF good;
    uf_init(&good, 8);
    for (int i = 0; i + 1 < good.n; i++) {
        uf_union(&good, i, i + 1);      /* 一模一样的边顺序，区别只是它按秩挂 */
    }
    printf("      同样的 (0,1)(1,2)...(6,7)，按秩合并之后：\n");
    uf_print_parent_array(&good, "");
    printf("      森林：\n");
    uf_print_forest(&good);

    printf("\n      两种做法'每个点各 find 一次'的总步数对比（都用不压缩的朴素 find 量的）：\n");
    uf_reset(&uf);
    for (int i = 0; i + 1 < uf.n; i++) {
        uf_union_naive(&uf, i, i + 1);
    }
    long long bad_total = total_hops_all_nodes(&uf);
    long long good_total = total_hops_all_nodes(&good);
    int bad_root = uf_find_naive(&uf, 0, NULL, 0, NULL);      /* 晾衣杆的根是谁 */
    int good_root = uf_find_naive(&good, 0, NULL, 0, NULL);   /* 按秩版的根是谁 */
    printf("        不优化（一条链）  ：每个点各 find 一次总共爬 %lld 条边，树高 %d\n",
           bad_total, uf_tree_height(&uf, bad_root));
    printf("        按秩合并（矮胖子）：每个点各 find 一次总共爬 %lld 条边，树高 %d\n",
           good_total, uf_tree_height(&good, good_root));
    printf("      ⇒ 光'按秩'这一项就能把树压矮；再叠上路径压缩，两个一起用才叫真正的并查集\n");

    /* ============================================================
       第 7 幕：应用① 动态判两点是否连通（边一条一条进来，问一句答一句）
       ============================================================ */
    section("第 7 幕：应用① 动态连通性 —— 边会陆续加进来，随时可能被问");

    uf_reset(&uf);
    printf("      已知边：先加 (0,1)、(1,2)、(3,4)\n");
    uf_union(&uf, 0, 1);
    uf_union(&uf, 1, 2);
    uf_union(&uf, 3, 4);
    uf_print_parent_array(&uf, "");

    int qs[3][2] = {{0, 2}, {0, 3}, {1, 3}};
    for (int q = 0; q < 3; q++) {
        int ans = uf_connected(&uf, qs[q][0], qs[q][1]);
        printf("      问：%d 和 %d 连通吗？ ⇒ %s\n", qs[q][0], qs[q][1],
               ans == 1 ? "连通 ✔" : "不连通 ✘");
    }

    printf("      现在补一条边 (2,3)，把两个分量焊起来\n");
    uf_union(&uf, 2, 3);
    uf_print_parent_array(&uf, "union(2,3) 之后：");
    for (int q = 0; q < 3; q++) {
        int ans = uf_connected(&uf, qs[q][0], qs[q][1]);
        printf("      再问：%d 和 %d 连通吗？ ⇒ %s\n", qs[q][0], qs[q][1],
               ans == 1 ? "连通 ✔" : "不连通 ✘");
    }
    printf("      ⇒ 这就是并查集最擅长的场景：'只加边、只问连通'，不用真去跑 BFS/DFS\n");

    /* ============================================================
       第 8 幕：应用② 判无向图有没有环
       ============================================================ */
    section("第 8 幕：应用② 判无向图是否有环 —— 加边时两端已连通 ⇒ 一定成环");

    uf_reset(&uf);
    /*
     * 7 个点的无向图，边按顺序加：
     *   (0,1) (1,2) (2,0) ← 第三条加之前 0 和 2 已经通过 1 连通了 ⇒ 环 0-1-2-0
     *   (2,3) (3,4) (4,5) (5,3) ← 环 3-4-5-3
     *   (5,6) 正常接上，不成环
     */
    int g_edges[8][2] = {{0, 1}, {1, 2}, {2, 0}, {2, 3}, {3, 4}, {4, 5}, {5, 3}, {5, 6}};
    int cycles_found = 0;
    for (int e = 0; e < 8; e++) {
        int a = g_edges[e][0], b = g_edges[e][1];
        int before = uf_connected(&uf, a, b);
        int ret = uf_union(&uf, a, b);
        if (before == 1) {
            cycles_found++;
            printf("      加边 (%d,%d)：两端早就连通了 ⇒ 【成环！】这条边把老路径闭合成圈\n", a, b);
        } else {
            printf("      加边 (%d,%d)：ret=%d，两端原来不连通，正常合并\n", a, b, ret);
        }
        uf_print_parent_array(&uf, "        加完这条边后：");
    }
    printf("      ⇒ 这张图一共发现了 %d 个环\n", cycles_found);
    printf("      原理：a、b 已连通 ⇒ 图上已经存在一条 a……b 的路径，\n");
    printf("            再直接连一条 a—b，就构成一个圈。反过来也成立：只要成环，环上最后加入的那条边一定会被这样抓到。\n");

    /* ============================================================
       第 9 幕：应用③ 连通分量个数统计
       ============================================================ */
    section("第 9 幕：应用③ 数一数这张图被分成了几块（连通分量个数）");

    uf_free(&uf);
    uf_init(&uf, 10);
    printf("      10 个点，边：(0,1)(1,2)(3,4)(5,6)(6,7)(7,5) 以及 (8,9)\n");
    int c_edges[7][2] = {{0, 1}, {1, 2}, {3, 4}, {5, 6}, {6, 7}, {7, 5}, {8, 9}};
    for (int e = 0; e < 7; e++) {
        int a = c_edges[e][0], b = c_edges[e][1];
        int ret = uf_union(&uf, a, b);
        printf("      union(%d,%d) ⇒ %s，当前集合数 = %d\n", a, b,
               ret == 1 ? "合并成功" : "已连通，跳过（这条边成环）",
               uf_component_count(&uf));
    }
    printf("\n      最终连通分量个数（直接读记账，O(1)）= %d\n", uf_component_count(&uf));

    /* 并查集本体不支持"列出集合成员"，所以要分组打印就得全表扫一遍 */
    printf("\n      每块里都有谁（按根分组扫一遍，O(n)）：\n");
    for (int r = 0; r < uf.n; r++) {
        if (uf.parent[r] != r) {
            continue;                /* 只从根开始打印，保证一块打一次 */
        }
        printf("        根 %d 这一块：", r);
        for (int j = 0; j < uf.n; j++) {
            if (uf_find(&uf, j) == r) {
                printf("%d ", j);
            }
        }
        printf("\n");
    }
    printf("      ⇒ 判'一张图分几块'：边全扫一遍做 union，答案就是 components 的余值\n");

    /* ============================================================
       收尾
       ============================================================ */
    printf("\n=== 本节结束 ===\n");
    printf("记住三句话：\n");
    printf("  1) 并查集 = parent[] 数组 + '往上爬'。根的下标就是集合的名字。\n");
    printf("  2) 路径压缩（爬过的点直接挂根）+ 按秩合并（小树挂大树）= 实际当 O(1) 用。\n");
    printf("  3) union 返回 0（两端已连通）是宝藏信息：判环、Kruskal 选边都靠它。\n");

    uf_free(&uf);
    uf_free(&good);
    return 0;
}
