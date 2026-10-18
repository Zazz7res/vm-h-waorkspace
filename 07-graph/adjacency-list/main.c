#include <stdio.h>
#include <stdlib.h>
#include "glist.h"

/*
 * main.c —— 邻接表分幕演示
 *
 * 这一节有两个看点：
 *   A. 数据结构角度：同一张图 G，换成"数组 + 一串小方块"长什么样，
 *      和上一节邻接矩阵的输出并排对照。
 *   B. C 语言角度：第一次真正在堆上**多层**申请和释放内存
 *      （GList → adj 指针数组 → 每个链表节点）。ASan 就盯着这一条。
 */

/* 小工具：打印分隔线（每个 main.c 自带一份，叶子目录互不引用） */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/* 小工具：单独展示"头插版"建出来的图，用完立刻释放 */
static void build_front_version_demo(void)
{
    GList *gf = glist_create(8);
    if (gf == NULL) {
        return;
    }
    /* 故意用和尾插版**完全相同**的加边顺序，只差"头插/尾插"这一件事 */
    glist_add_edge_front(gf, 0, 1);
    glist_add_edge_front(gf, 0, 2);
    glist_add_edge_front(gf, 1, 2);
    glist_add_edge_front(gf, 2, 3);
    glist_print(gf);
    glist_free(gf);
    gf = NULL;   /* free 完立刻置 NULL：这是 00-c-foundation 教的护身符 */
    printf("  （这份图已经逐顶点逐节点 free 干净了，ASan 会替我们验证）\n");
}

/*
 * 小工具：现场演示"删头节点"为什么要用 dummy 哨兵。
 * 这条链是 5 -> 3 -> 7，我们要删掉 5（就是头）。
 */
static void remove_head_demo(void)
{
    Node *list = NULL;

    /* 手工搭一条 5 -> 3 -> 7 */
    Node *n1 = malloc(sizeof(Node)); n1->dest = 5; n1->next = NULL;
    Node *n2 = malloc(sizeof(Node)); n2->dest = 3; n2->next = NULL;
    Node *n3 = malloc(sizeof(Node)); n3->dest = 7; n3->next = NULL;
    list = n1; n1->next = n2; n2->next = n3;

    printf("  删之前： 5 -> 3 -> 7 -> NULL，头指针 = %p\n", (void *)list);
    printf("  如果只传 Node *list 进函数，在函数里 list = list->next 改不到外面的 list\n");
    printf("  （改的只是拷贝 —— 见 00-c-foundation/pointer-basics），\n");
    printf("  所以 glist 里所有增删都写成 Node **head，传的是 &g->adj[i]。\n");

    /* 用我们的哨兵写法删掉 5：Node ** 语义，所以传 &list */
    /* 这里就地手写一遍，演示"删头"是怎么被统一处理的 */
    Node dummy;
    dummy.dest = -1;
    dummy.next = list;
    Node *prev = &dummy;
    Node *victim = prev->next;         /* victim = 节点 5 */
    prev->next = victim->next;         /* dummy.next = 3，5 被跨过 */
    free(victim);
    list = dummy.next;                 /* 头指针换成 3 —— 这一行就是"删头"的全部难点 */

    printf("  删之后： ");
    for (const Node *p = list; p != NULL; p = p->next) {
        printf("%d -> ", p->dest);
    }
    printf("NULL，头指针 = %p（变了！）\n", (void *)list);

    /* 剩下的还回去，保证 0 泄漏 */
    while (list != NULL) {
        Node *nxt = list->next;
        free(list);
        list = nxt;
    }
}

int main(void)
{
    /* ============================================================
       第 1 幕：把共用示例图 G 建成邻接表
       ============================================================ */
    section("1. 建图：同一张图 G，这次用『数组 + 一串小方块』");

    GList *g = glist_sample();          /* malloc 出来的，最后必须 glist_free */
    if (g == NULL) {
        printf("  建图失败，退出\n");
        return 1;
    }
    printf("  顶点数 v = %d，边数 e = %d\n", g->v, g->e);
    printf("  三层内存：GList %zu 字节 + adj 数组 %zu 字节 + %d 个链表节点 × %zu 字节 = %zu 字节\n",
           sizeof(GList), (size_t)g->v * sizeof(Node *), 2 * g->e, sizeof(Node),
           sizeof(GList) + (size_t)g->v * sizeof(Node *)
               + (size_t)(2 * g->e) * sizeof(Node));
    printf("\n");
    glist_print(g);

    printf("\n  和上一节邻接矩阵并排看：\n");
    printf("    矩阵第 0 行  = 0 1 1 0 0 0 0 0   （8 格，其中 6 个是废 0）\n");
    printf("    邻接表第 0 链 = [1] -> [2] -> NULL（2 个节点，一个废格子都没有）\n");
    printf("    矩阵第 7 行  = 0 0 0 0 0 0 0 0   （8 格全 0）\n");
    printf("    邻接表第 7 链 = NULL              （1 个指针，孤立点在这里最省）\n");

    /* ============================================================
       第 2 幕：判边 —— 邻接表的短板，必须顺链走
       ============================================================ */
    section("2. 判边 has_edge：顺着链表找，O(度)");

    printf("  (0,1) 有边吗？ %d   ← 走 1 格就命中\n", glist_has_edge(g, 0, 1));
    printf("  (0,2) 有边吗？ %d   ← 走 2 格命中\n", glist_has_edge(g, 0, 2));
    printf("  (0,3) 有边吗？ %d   ← 走到底没找到（0 的链只有 2 格）\n",
           glist_has_edge(g, 0, 3));
    printf("  (3,2) 有边吗？ %d   ← 3 的链只有 1 格，反着问也对（无向对称）\n",
           glist_has_edge(g, 3, 2));
    printf("  (4,7) 有边吗？ %d   ← 7 的链是空的；问 (7,4) 也一样\n",
           glist_has_edge(g, 4, 7));
    printf("  (7,4) 有边吗？ %d\n", glist_has_edge(g, 7, 4));
    printf("  对比：邻接矩阵问同样的问题只需 edge[a][b] 一次取下标，O(1)。\n");
    printf("  差在哪：这里的位置算不出来，只能一格一格问 —— 这就是链表的税。\n");

    /* ============================================================
       第 3 幕：查度 + 节点总数自检
       ============================================================ */
    section("3. 查度 degree = 数链长；用节点数做免费自检");

    for (int u = 0; u < g->v; u++) {
        printf("  顶点 %d：度 = %d%s\n", u, glist_degree(g, u),
               (glist_degree(g, u) == 0) ? "   ← 孤立点（表头 NULL）" : "");
    }
    printf("  度之和 = %d，节点总数 = %d，2×边数 = %d\n",
           14, glist_node_count(g), 2 * g->e);
    printf("  三个数字必须相等，否则说明某条边只挂了一边（对称性被破坏）。\n");

    /* ============================================================
       第 4 幕：头插 vs 尾插 —— 同一张图，两种画法
       ============================================================ */
    section("4. 头插 vs 尾插：顺序会变，正确性不变");

    printf("  尾插保住的是『加边顺序』。本节的示例图特意把加边顺序排成\n");
    printf("  (0,1)(0,2)(1,2)(2,3)(4,5)(4,6)(5,6)，于是每条链恰好都是升序：\n");
    printf("    顶点 2 的链 = [0] -> [1] -> [3]\n");
    printf("  头插版用同样的顺序加，结果是这样：\n");
    build_front_version_demo();
    printf("  结论：两条链描述的是同一个集合 {0,1,3}，遍历结果、距离、连通性全一样。\n");
    printf("  但输出顺序变了 → README 里的画面不可复现，也难逐行核对；\n");
    printf("  而且 BFS/DFS『先看到谁』的顺序会变。所以教材一律固定邻居顺序。\n");
    printf("  头插的真正价值是 O(1)：尾插每次要走到链尾，稠密图上建图会慢一截。\n");

    /* ============================================================
       第 5 幕：删边 —— 第 02 章按值删节点的实战
       ============================================================ */
    section("5. 删边：指针手术 + 为什么必须传 Node**");

    printf("  删掉边 (0,1)：要在 0 的链里摘掉 dest=1，还要在 1 的链里摘掉 dest=0\n");
    int ok_rm = glist_remove_edge(g, 0, 1);   /* ★ 先单独调用，再打印 g->e。
                                              *   写成 printf("%d %d", glist_remove_edge(...), g->e)
                                              *   的话，C 语言**不保证**参数的求值顺序，
                                              *   很可能先读到了旧的 e，打印出骗人的数字。
                                              *   这是本节踩过的一次真坑：有副作用的函数调用
                                              *   永远单独一行，不要塞进 printf 的参数列表。 */
    printf("    返回 %d\n", ok_rm);
    glist_print(g);
    printf("  现在 (0,1) 还有边吗？ %d，e = %d\n", glist_has_edge(g, 0, 1), g->e);

    printf("\n  删一条不存在的边 (0,3)：返回 %d，e = %d（不会被减成负数）\n",
           glist_remove_edge(g, 0, 3), g->e);   /* 这里没改动 e，同参数打印才安全 */

    printf("\n  加一条新边 (0,3)：");
    int ok_add = glist_add_edge(g, 0, 3);
    printf("返回 %d，e = %d；0 的链现在是 2 -> 3\n", ok_add, g->e);
    printf("    （这次恰好也是升序，但那是运气：尾插保证的是『按加边先后排队』，不是『按编号排序』）\n");

    int e_before_dup = g->e;
    int ok_dup = glist_add_edge(g, 0, 3);
    printf("  再重复加一次 (0,3)：返回 %d，e 仍是 %d（幂等，没有变成 %d）\n",
           ok_dup, g->e, e_before_dup + 1);

    printf("\n  单独看『删头节点』这一件事为什么麻烦：\n");
    remove_head_demo();

    printf("\n  故意越界，看代码怎么挡：\n");
    glist_add_edge(g, 2, 99);
    glist_add_edge(g, 5, 5);
    printf("  e 仍然是 %d，图没被污染\n", g->e);

    /* ============================================================
       第 6 幕：内存账 —— 什么时候邻接表才真的划算
       ============================================================ */
    section("6. 内存账：邻接表不一定更省");

    printf("  本图（V=8, E=7）：\n");
    printf("    邻接矩阵 = 8×8×4 = 256 字节\n");
    printf("    邻接表   = 8 个头指针(%d B) + 14 个节点 × %zu B = %zu 字节\n",
           (int)(8 * sizeof(Node *)), sizeof(Node),
           (size_t)(8 * sizeof(Node *) + 14 * sizeof(Node)));
    printf("    → 小图上邻接表反而**更费内存**：每个节点要多背 %zu 字节的 next 指针。\n",
           sizeof(Node *) );
    printf("      （而且 %zu 是 sizeof(Node) = int 4B + padding 4B + 指针 8B，对齐补了 4 字节）\n",
           sizeof(Node));

    printf("\n  大图（V=100000, E=200000）：\n");
    printf("    邻接矩阵 = 100000×100000×4 ≈ 40 GB\n");
    printf("    邻接表   = 100000×%zu + 400000×%zu = %zu 字节 ≈ %.2f MB\n",
           sizeof(Node *), sizeof(Node),
           (size_t)(100000 * sizeof(Node *) + 400000ull * sizeof(Node)),
           (double)(100000 * sizeof(Node *) + 400000ull * sizeof(Node)) / (1024.0 * 1024.0));
    printf("    → 差五千多倍。而且邻接表的大小只跟 **真实边数** 有关，图再稀疏都扛得住。\n");

    printf("\n  遍历全图所有边：\n");
    printf("    邻接矩阵要扫 V² = 100 亿格（绝大多数是 0，白扫）\n");
    printf("    邻接表只走 V + 2E = %d 步 —— 这才是它的主场（BFS/DFS 都靠这个）\n",
           8 + 2 * g->e);

    /* ============================================================
       第 7 幕：收摊 —— 必须逐顶点逐节点 free
       ============================================================ */
    section("7. 释放：三层内存倒着剥");

    printf("  释放顺序：① 每条链的每个节点 ② adj 指针数组 ③ GList 本身\n");
    printf("  为什么要先剥节点？因为 free 掉 adj 数组后，你就再也拿不到\n");
    printf("  那些节点的地址了 —— 拿不到地址的堆内存就是永久泄漏。\n");
    glist_free(g);
    g = NULL;
    printf("  已释放，并把 g 置 NULL（此后误用会立刻崩，而不是悄悄读垃圾）\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住三句话：\n");
    printf("  1) 邻接表 = 『V 个表头的指针数组』+『每条链一个邻居节点』，空间 O(V+E)。\n");
    printf("  2) 判边要从 O(1) 降级到 O(度)，换来的是遍历邻居 O(V+E) 和大图不爆炸。\n");
    printf("  3) 无向图一条边挂两个节点；增删都要对称处理两边，失败要回滚，否则图半连通。\n");
    return 0;
}
