#include <stdio.h>
#include "topo.h"

/*
 * ===================================================================
 * 拓扑排序 · 演示主程序
 * ===================================================================
 * 九幕，主线是三步：
 *   第 1~3 幕：Kahn 入度法。先看懂一遍全过程，再做「队列 vs 栈」的对照实验，
 *              亲手证明「拓扑序不唯一，但每一份都合法」。
 *   第 4~6 幕：环。Kahn 排不完 = 有环；再把卡住的人分成「坐在环上的」和「被连累的」；
 *              然后看 DFS 三色怎么用一条回边把环当场抓住。
 *   第 7~9 幕：两个真实应用（选课、编译依赖）+ 一个反面验收。
 *
 * ⚠ 全部例子都在 stdout 打印，没有一句用 stderr。
 */

static void banner(int act, const char *title)
{
    printf("\n════════════════════════════════════════════════════════════\n");
    printf(" 第 %d 幕 · %s\n", act, title);
    printf("════════════════════════════════════════════════════════════\n");
}

static void sub(const char *t)
{
    printf("\n  --- %s ---\n", t);
}

int main(void)
{
    /* 所有缓冲区都按 TOPO_MAX_V 开，因为 topo.h 保证了顶点编号 < TOPO_MAX_V */
    int order[TOPO_MAX_V];
    int stuck[TOPO_MAX_V];
    int stuck_len = 0;
    int post[TOPO_MAX_V];
    int post_len = 0;
    int toporder[TOPO_MAX_V];
    int indeg[TOPO_MAX_V];

    /* ================================================================
     * 第 1 幕：一张「必须先做什么，才能做什么」的图长什么样
     * ================================================================ */
    banner(1, "问题本身：把「先后依赖」画成有向图");
    printf("  顶点就是「一件事」，边 u→v 读作「u 做完了，v 才可以开始」。\n");
    printf("  拓扑排序要吐出一个序列，使得【每条边 u→v 里，u 都排在 v 前面】。\n\n");

    TopoGraph *g = topo_graph_dag();   /* 6 点 6 边的演示 DAG */
    if (g == NULL) {
        printf("  建图失败，退出。\n");
        return 1;
    }

    printf("  本节示例图（出边邻接表）：\n");
    topo_print(g);

    topo_indeg(g, indeg);
    printf("\n  ★ 注意这张图的顶点编号【不是】按依赖大小排的：\n");
    printf("    比如入度为 0 的是 4 和 5（它们谁都不依赖，是可以最先开工的活），\n");
    printf("    而编号最小的 0 反倒有两个前驱（4 和 5 都得先做完）。\n");
    printf("    这就是拓扑排序存在的意义：编号是随便起的，只有边才是真约束。\n");

    /* ================================================================
     * 第 2 幕：Kahn 入度法，一步一步看它怎么「还债」
     * ================================================================ */
    banner(2, "方法 A · Kahn 入度法（就是「入度为 0 的 BFS」）");
    printf("  规矩只有一条：【入度清零才能上场】。\n");
    printf("  取出 u 写进结果表 → u 提过的所有「必须先有我」全部作废 → 邻居入度集体 -1。\n");
    printf("  下面每一步都打印：容器现状 + 全表入度 + 这一步解锁了哪几条边。\n\n");

    int n = topo_kahn(g, /*fifo=*/1, /*verbose=*/1, order, stuck, &stuck_len);

    sub("Kahn（FIFO）的最终结果");
    printf("  排出了 %d 个顶点（图共 %d 个点）→ ", n, g->v);
    if (n == g->v) {
        printf("一个没漏，说明这图无环 ✓\n");
    } else {
        printf("漏了 %d 个 → 有环 ✗\n", g->v - n);
    }
    topo_print_order("  拓扑序", order, n);
    printf("  验收（逐条边检查前后继位置）：%s\n",
           topo_order_is_valid(g, order, n) ? "合法 ✓" : "违规 ✗");

    /* ================================================================
     * 第 3 幕：同一张图，队列 vs 栈 → 两份都合法的不同答案
     * ================================================================ */
    banner(3, "拓扑序不唯一：把容器从队列换成栈，答案变了但依然合法");
    printf("  算法只规定「入度清零才能上场」，\n");
    printf("  【同时有好几个点入度为 0 时先取谁】算法完全没规定 —— 这一份自由留给了容器。\n");

    sub("① 容器用 FIFO（队列）：先攒够资格的先开工");
    int n_fifo = topo_kahn(g, 1, 0, order, NULL, NULL);
    topo_print_order("     FIFO 结果", order, n_fifo);
    printf("     合法性：%s\n", topo_order_is_valid(g, order, n_fifo) ? "合法 ✓" : "违规 ✗");

    sub("② 容器用 LIFO（栈）：刚拿到资格的插队先开工");
    int n_lifo = topo_kahn(g, 0, 0, order, NULL, NULL);
    topo_print_order("     LIFO 结果", order, n_lifo);
    printf("     合法性：%s\n", topo_order_is_valid(g, order, n_lifo) ? "合法 ✓" : "违规 ✗");

    printf("\n  ★ 两份序列【不一样】，但【都合法】。谁对？都对。\n");
    printf("    所以「拓扑排序」输出的不是一个确定的序，而是一个【不违反任何依赖的可行序】。\n");
    printf("    考试题若问「拓扑序列是唯一的吗」，答案就藏在这一幕里：不唯一，\n");
    printf("    除非每时刻恰好只有一个入度为 0 的点（那时 DAG 其实是一条链）。\n");

    /* ================================================================
     * 第 4 幕：有环的图，Kahn 排不完
     * ================================================================ */
    banner(4, "判环（Kahn 版）：排出来的个数 < 顶点总数，就是有环");
    TopoGraph *gc = topo_graph_cycle();   /* 7 点 7 边，含一个 1→2→3→1 的环 */
    if (gc == NULL) {
        printf("  建图失败，退出。\n");
        topo_free(g);
        return 1;
    }

    printf("  带环示例图：\n");
    topo_print(gc);

    sub("跑 Kahn（安静模式），看它排出几个");
    stuck_len = 0;
    int n2 = topo_kahn(gc, 1, 0, order, stuck, &stuck_len);
    topo_print_order("     排出的序列", order, n2);
    printf("     排出 %d 个 / 共 %d 个 → 少 %d 个 ⇒ 【这图有环，不存在完整拓扑序】\n",
           n2, gc->v, gc->v - n2);
    printf("     卡住没排出的顶点：");
    for (int i = 0; i < stuck_len; i++) {
        printf("%d ", stuck[i]);
    }
    printf("\n");

    sub("卡住的人分两种：坐在环上的 vs 被环连累的下游");
    printf("  ⚠ 这一步很重要：剩下那批人并不是「都在环上」。\n");
    printf("    有的人只是它的前驱卡在环里，它自己干干净净 —— 把它误报成环，\n");
    printf("    你去拆环就会拆错地方（拆了它那条边，环照样存在）。\n");
    topo_report_stuck(gc, stuck, stuck_len);

    /* ================================================================
     * 第 5 幕：方法 B —— DFS 后序，整个反转
     * ================================================================ */
    banner(5, "方法 B · DFS 后序逆序法：走完所有下游才轮到我记账");
    printf("  前序是「进门就记」，后序是「收摊才记」——\n");
    printf("  收摊意味着【我的所有下游都已经收摊完毕】，所以后序里我比所有下游【晚】出现。\n");
    printf("  而拓扑序要求我比所有下游【早】出现。两者正好相反 ⇒ 后序表整个倒过来就是拓扑序。\n");

    post_len = 0;
    int vn = topo_dfs_postorder(g, /*verbose=*/1, post, &post_len);
    sub("把后序表反转");
    printf("  后序表（收摊顺序）：");
    for (int i = 0; i < post_len; i++) {
        printf("%d ", post[i]);
    }
    printf("\n");
    topo_reverse(post, post_len, toporder);
    topo_print_order("  反转后 = 拓扑序", toporder, post_len);
    printf("  访问到的顶点数 = %d，图共 %d 个点\n", vn, g->v);
    printf("  合法性验收：%s\n", topo_order_is_valid(g, toporder, post_len) ? "合法 ✓" : "违规 ✗");
    printf("\n  ★ 顺手不省的反面：如果忘了反转，直接用后序表当拓扑序会怎样？下面当场验给你看。\n");
    printf("    不反转、直接拿后序表 %d %d ... 当拓扑序去用 → 验收：%s\n",
           post[0], post[1],
           topo_order_is_valid(g, post, post_len) ? "居然合法（这张图太小，别当成规律）" : "违规 ✗ —— 反转这一步绝不能省");

    /* ================================================================
     * 第 6 幕：三色标记 —— DFS 也能当场抓出环
     * ================================================================ */
    banner(6, "DFS 三色标记：撞到【灰色】顶点就是找到了回边");
    printf("  白 = 还没碰过；灰 = 正在我的调用栈上（邻居还没走完）；黑 = 收摊了。\n");
    printf("  指向灰色顶点 = 「绕回了还在栈上的自己人」= 回边 = 有环。\n");
    printf("  指向黑色顶点是安全的：那是处理完的旁支，不构成环。\n");

    post_len = 0;
    int has_cycle = topo_dfs_color(gc, /*verbose=*/1, post, &post_len);
    sub("结论");
    printf("  topo_dfs_color 返回 %d ⇒ %s\n", has_cycle,
           has_cycle ? "发现回边，这图有环 ✗（此时后序表只填了一半，反转也没有意义）" : "无环 ✓");

    post_len = 0;
    int clean = topo_dfs_color(g, 0, post, &post_len);
    printf("  同一份代码跑第 1 幕那张 DAG：返回 %d ⇒ %s，后序表长度 %d（= 顶点数，说明填满了）\n",
           clean, clean ? "有环" : "无环 ✓", post_len);

    /* ================================================================
     * 第 7 幕：应用一 —— 选课先修表
     * ================================================================ */
    banner(7, "应用一 · 大学选课：哪学期能选哪些课");
    printf("  顶点 = 课程（顶点 0 是「什么都不用先修」的虚起点，方便把多门基础课挂上去）。\n");
    printf("  边 u→v 读作「学完 u 才有资格选 v」。\n");

    /* 8 门课：0 虚起点 / 1 高等数学 / 2 程序设计 / 3 数据结构 / 4 离散数学
     *         / 5 算法设计 / 6 操作系统 / 7 数据库 */
    static const char *course[8] = {
        "（虚起点）", "高等数学", "程序设计", "数据结构",
        "离散数学", "算法设计", "操作系统", "数据库"
    };
    TopoGraph *gs = topo_create(8);
    if (gs == NULL) {
        printf("  建图失败。\n");
    } else {
        topo_add_edge(gs, 0, 1);   /* 高数：只需虚起点 */
        topo_add_edge(gs, 0, 2);   /* 程设：只需虚起点 */
        topo_add_edge(gs, 0, 4);   /* 离散：只需虚起点 */
        topo_add_edge(gs, 2, 3);   /* 数据结构 ← 程设 */
        topo_add_edge(gs, 1, 5);   /* 算法设计 ← 高数 */
        topo_add_edge(gs, 4, 5);   /* 算法设计 ← 离散 */
        topo_add_edge(gs, 3, 5);   /* 算法设计 ← 数据结构 */
        topo_add_edge(gs, 3, 6);   /* 操作系统 ← 数据结构 */
        topo_add_edge(gs, 2, 7);   /* 数据库 ← 程设 */
        topo_add_edge(gs, 4, 7);   /* 数据库 ← 离散 */

        printf("  先修关系图：\n");
        topo_print(gs);

        sub("Kahn 排出一个合法选课顺序");
        int ns = topo_kahn(gs, 1, 0, order, NULL, NULL);
        printf("  排出 %d / %d 个 → %s\n\n", ns, gs->v, ns == gs->v ? "无环，能正常排完 ✓" : "有环（先修表自相矛盾）✗");
        for (int i = 0; i < ns; i++) {
            printf("    第 %d 门：%s（顶点 %d）\n", i + 1, course[order[i]], order[i]);
        }
        printf("  逐条先修约束验收：%s\n", topo_order_is_valid(gs, order, ns) ? "全部满足 ✓" : "有违反 ✗");

        sub("换成「先修表自相矛盾」的版本：加一条 数据结构→程设，和已有 程设→数据结构 成环");
        topo_add_edge(gs, 3, 2);
        int ns2 = topo_kahn(gs, 1, 0, order, stuck, &stuck_len);
        printf("  排出 %d / %d 个 → 少 %d 个 ⇒ 先修表里出现了循环依赖 ✗\n", ns2, gs->v, gs->v - ns2);
        printf("  卡住的课程：");
        for (int i = 0; i < stuck_len; i++) {
            printf("%s ", course[stuck[i]]);
        }
        printf("\n");
        printf("  ★ 这就是所有包管理器 / 构建系统报「循环依赖」的原理：\n");
        printf("    它们没有专门的「找环算法」，就是跑一遍拓扑排序看排不排得完。\n");

        topo_free(gs);
    }

    /* ================================================================
     * 第 8 幕：应用二 —— 编译任务的执行顺序（make -j1 的实质）
     * ================================================================ */
     banner(8, "应用二 · 构建任务调度：哪些任务必须串行，哪些能并行");
    printf("  顶点 = 一个构建任务，边 u→v = 「v 需要 u 的产物」。\n");

    /* 7 个任务：0 下载依赖 / 1 生成代码 / 2 编译核心 / 3 编译插件 / 4 链接 / 5 跑测试 / 6 打包 */
    static const char *task[7] = {
        "下载依赖", "生成代码", "编译核心", "编译插件", "链接", "跑测试", "打包"
    };
    TopoGraph *gb = topo_create(7);
    if (gb != NULL) {
        topo_add_edge(gb, 0, 1);   /* 生成代码 ← 下载依赖 */
        topo_add_edge(gb, 1, 2);   /* 编译核心 ← 生成代码 */
        topo_add_edge(gb, 1, 3);   /* 编译插件 ← 生成代码（和编译核心互不依赖 → 同一层，可并行）*/
        topo_add_edge(gb, 2, 4);   /* 链接 ← 编译核心 */
        topo_add_edge(gb, 3, 4);   /* 链接 ← 编译插件 */
        topo_add_edge(gb, 4, 5);   /* 跑测试 ← 链接 */
        topo_add_edge(gb, 5, 6);   /* 打包 ← 跑测试 */

        int nb = topo_kahn(gb, 1, 0, order, NULL, NULL);
        printf("  一个合法的串行执行顺序（make -j1 干的事）：\n");
        for (int i = 0; i < nb; i++) {
            printf("    %d) %s\n", i + 1, task[order[i]]);
        }

        /* 按「层」分组：这一层全做完才能进下一层 —— 这就是 -j 并行的分组依据。
         * 做法：用拓扑序把每个顶点标上「它所有前驱的最大层 + 1」，是 O(V+E) 的一趟 DP。 */
        sub("按「深度」分层：同一层内部没有依赖，可以并行");
        int depth[TOPO_MAX_V];
        for (int i = 0; i < gb->v; i++) {
            depth[i] = 0;
        }
        /* 关键：必须【按拓扑序】遍历，才能保证处理 u 时它的所有前驱都已经算完了 depth。
         * 如果乱序遍历，depth 就会算错 —— 这是「拓扑排序 + 一趟 DP」这个套路的命门。 */
        for (int i = 0; i < nb; i++) {
            int u = order[i];
            for (const ENode *ed = gb->heads[u]; ed != NULL; ed = ed->next) {
                if (depth[ed->dest] < depth[u] + 1) {
                    depth[ed->dest] = depth[u] + 1;
                }
            }
        }
        int maxd = 0;
        for (int i = 0; i < gb->v; i++) {
            if (depth[i] > maxd) {
                maxd = depth[i];
            }
        }
        for (int d = 0; d <= maxd; d++) {
            printf("    第 %d 层（可并行）：", d);
            for (int i = 0; i < nb; i++) {          /* 按拓扑序打印，层内顺序稳定可复现 */
                if (depth[order[i]] == d) {
                    printf("%s ", task[order[i]]);
                }
            }
            printf("\n");
        }
        printf("  ★ 一共 %d 层（最深层 depth = %d）⇒ 假设每个任务耗时 1、机器管够，最短总耗时就是 %d。\n",
               maxd + 1, maxd, maxd + 1);
        printf("    注意【关键路径】上的任务（下载依赖→生成代码→编译核心→链接→跑测试→打包）\n");
        printf("    一个都省不掉：就算给你无限个 CPU，总时间也不可能短过这条链。\n");
        topo_free(gb);
    }

    /* ================================================================
     * 第 9 幕：反面验收 —— 一个「看着挺像」但违反依赖的序列
     * ================================================================ */
     banner(9, "反面教材 · 把顺序写错会怎样：让 topo_order_is_valid 当场抓出来");
    /* 第 1 幕那张图的边是：4→0 4→1 5→0 5→2 2→3 3→1。
     * 下面这个序列把 1 排在 3 前面，而边要求 3 在 1 之前 → 应被判定违规。 */
    int wrong[6] = {4, 5, 0, 1, 2, 3};
    printf("  待检查序列：");
    for (int i = 0; i < 6; i++) {
        printf("%d ", wrong[i]);
    }
    printf("\n");
    printf("  它的问题：边 3→1 要求【3 排在 1 前面】，但这个序列里 1 在第 4 位、3 在第 6 位。\n");
    printf("  topo_order_is_valid 判定：%s\n",
           topo_order_is_valid(g, wrong, 6) ? "合法（说明你的验收函数写漏了！）" : "违规 ✗ 抓到了");
    printf("  ★ 这个函数就是「我怎么知道自己写的拓扑排序对不对」的答案：\n");
    printf("    不必比较两份序列是否逐位相同（那样只会因为容器不同而误报），\n");
    printf("    而是【逐条边检查前驱位置 < 后继位置】—— 一次 O(V+E) 的判据，通吃所有合法答案。\n");

    /* ================================================================
     * 收尾
     * ================================================================ */
    sub("收摊：三张图都是 topo_create/topo_graph_* malloc 出来的，必须全部 free");
    topo_free(g);
    topo_free(gc);
    printf("  已释放：演示 DAG、带环图（第 7、8 幕的图在各自幕内已释放）。\n");
    printf("  若漏掉任意一个 topo_free，ASan 会直接报泄漏 —— 这一节的内存责任田只有一处，就是 create/free。\n");

    return 0;
}
