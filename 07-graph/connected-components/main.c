#include <stdio.h>
#include "cc.h"

/*
 * main.c —— 连通分量 + 判环 分幕演示
 *
 * 五幕：
 *   1. 无向图 G：一轮 DFS = 一个连通分量，正式把上一节那句"教训"变成算法
 *   2. 顺手交叉验证：每个分量的 V 和 E（"E >= V 必有环"和 DFS 的判环结果必须一致）
 *   3. 无向图判环：G 有环 / 树 T 无环 / "环藏在第二个分量"的 C2 —— 只起一轮会漏
 *   4. 有向图判环（三色法）：DAG 无环（黑色邻居是正常的）/ CYC 有环（灰色命中）
 *      顺便演示"算法用错图类型"会被直接拒绝
 *   5. 有向图上"连通"这件事为什么不一样：可达集对比 + 弱连通 vs 强连通（SCC 不实现）
 */

static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/* 按顶点号打一行数组，标签和数字分开打印，避免中文宽度把表格挤歪 */
static void print_by_vertex(const char *label, const int arr[], int v)
{
    printf("      %s：", label);
    for (int u = 0; u < v; u++) {
        printf("%d ", arr[u]);
    }
    printf("\n");
}

int main(void)
{
    CCGraph g, tree, split, dag, cyc;
    cc_sample_undirected(&g);
    cc_sample_tree(&tree);
    cc_sample_split_cycle(&split);
    cc_sample_dag(&dag);
    cc_sample_cyclic(&cyc);

    int comp_id[CC_MAX_V];

    /* ============================================================
       第 1 幕：连通分量
       ============================================================ */
    section("1. 无向图 G 有几个连通分量？（上一节的 DFS 骨架，正式改成本节的算法）");

    cc_print(&g);
    printf("\n  算法骨架（就是上一节那句'外层再扫一遍'）：\n");
    printf("      comp_id[] 全填 -1\n");
    printf("      k = 0\n");
    printf("      for 每个顶点 u:\n");
    printf("          if comp_id[u] == -1:        ← 遇见一个没归队的点\n");
    printf("              从 u 起一轮 DFS，碰到的点全写 comp_id = k\n");
    printf("              k++                     ← 一轮 = 一个分量\n");
    printf("      return k\n");

    int k = cc_find_all(&g, comp_id, /* verbose=*/1);
    printf("\n  ═══ 一共 %d 轮 → 这张图有 %d 个连通分量 ═══\n", k, k);
    cc_print_components(&g, comp_id, k);

    /* ============================================================
       第 2 幕：边数交叉验证
       ============================================================ */
    section("2. 免费自查：每个分量的顶点数 V 和边数 E（结论必须和 DFS 判环一致）");

    printf("  无向图经典结论：连通分量里  E == V-1  ⇒ 这分量是树（无环）\n");
    printf("                  E >= V      ⇒ 这分量里必有环\n");
    printf("  为什么可以当交叉验证：环的存在性被 DFS 判了一次，又被边数判了一次，\n");
    printf("  两个答案必须吻合，否则代码有 bug。（同类手法：邻接矩阵那节的 Σ度=2E 自查）\n\n");
    cc_component_stats(&g, comp_id, k);
    printf("\n  和 DFS 的判定对照：分量 0、1 边数够了 → 后面第 3 幕会真的从这两个分量里各挖出一个环；\n");
    printf("                    分量 2 只有孤立点 7（V=1，E=0=V-1）→ 平凡树，无环。\n");

    printf("\n  再看那棵对照树 T（5 点 4 边）：\n");
    int comp_t[CC_MAX_V];
    int kt = cc_find_all(&tree, comp_t, /* verbose=*/0);
    printf("  分量数 = %d（连通）\n", kt);
    cc_component_stats(&tree, comp_t, kt);
    printf("  E = 4 = V-1 且连通 ⇒ 这是树。'树'的定义就是'连通 + 无环'，\n");
    printf("  所以 T 是判环算法的**阴性对照**：第 3 幕它在同一套代码下必须回答'无环'。\n");

    /* ============================================================
       第 3 幕：无向图判环
       ============================================================ */
    section("3. 无向图判环：碰到『已访问且不是父亲』的邻居 ⇒ 有环");

    printf("  先复习一下这个判据为什么成立：\n");
    printf("    · 它已访问、又不是我父亲 ⇒ 它是我当前这条路上更早的某个点（我的祖先）；\n");
    printf("    · 于是 它 → …… → 我 →（这条边）→ 它，闭合了 ⇒ 环。\n");
    printf("    · 反过来，如果每次碰到的已访问邻居**都恰好是父亲**，那这棵树就是最老实的树。\n");
    printf("  ★ 为什么必须带父亲：无向图一条边在矩阵里涂两格（u→v 和 v→u），\n");
    printf("    我从 u 钻到 v，v 一回头就能看见 u 且 u 已访问。不排掉父亲，树会被判成处处有环。\n\n");

    int cyc_v[CC_MAX_V];
    int cyc_len = 0;

    printf("  —— 图 G（预期：有环）——\n");
    int has_g = cc_has_cycle_undirected(&g, cyc_v, &cyc_len, /* verbose=*/1);
    printf("  返回 %d（1=有环）\n", has_g);
    cc_print_cycle("  挖出来的环：", cyc_v, cyc_len);
    printf("  这条边就是画面里那条『邻居 0：已访问，而且不是我父亲』：2→0，环是 0-1-2 这个三角形。\n\n");

    printf("  —— 图 T（预期：无环，阴性对照）——\n");
    int has_t = cc_has_cycle_undirected(&tree, cyc_v, &cyc_len, /* verbose=*/1);
    printf("  返回 %d（0=无环）→ 和 E==V-1 的边数结论吻合 ✔\n", has_t);

    printf("\n  —— 图 C2：环**藏在第二个分量**里，先看只起一轮的错误答案 ——\n");
    int has_one = cc_has_cycle_undirected_from(&split, 0, cyc_v, &cyc_len, /* verbose=*/1);
    printf("  只从 0 起一轮的答复：%d（0=无环）← 错！这一片确实没环，可整张图有环。\n", has_one);
    printf("  而且它不报错、返回值也正常，就是答案错 —— 和上一节'漏掉一整个分量'同一类 bug。\n\n");
    int has_split = cc_has_cycle_undirected(&split, cyc_v, &cyc_len, /* verbose=*/1);
    printf("  外层扫全图的答复：%d（1=有环）\n", has_split);
    cc_print_cycle("  挖出来的环：", cyc_v, cyc_len);
    printf("  判环也必须外层扫，一个都不能省。\n");

    /* ============================================================
       第 4 幕：有向图判环（三色法）
       ============================================================ */
    section("4. 有向图判环：白/灰/黑三色。碰到灰色的邻居 ⇒ 有环");

    printf("  为什么无向那套『已访问 + 排父亲』在这里不够用？看这个无环的例子：\n");
    printf("      0 → 1，0 → 2，2 → 1        （1 有两条路可以到，但一个环都没有）\n");
    printf("  DFS：0 → 1 回来，再 0 → 2，2 看到邻居 1『已访问且不是父亲』→\n");
    printf("  照无向判据就会喊有环，可这图清清楚楚无环。\n");
    printf("  问题出在『已访问』这个状态太粗，得细分成两档：\n");
    printf("      灰 = 来过但**还没走完** = 我还站在当前这条路上（还压在递归栈里）\n");
    printf("      黑 = 走完了（我和我能到的地方全办完了）→ 已经出栈\n");
    printf("  判据：碰到灰色 ⇒ 它在我当前的路上 ⇒ 它 → … → 我 → 它，有向环。\n");
    printf("        碰到黑色 ⇒ 完全正常，那只是'有人比我先下班'，不构成回路。\n\n");

    printf("  —— DAG（7 点 8 弧，预期：无环）——\n");
    int has_dag = cc_has_cycle_directed(&dag, cyc_v, &cyc_len, /* verbose=*/1);
    printf("  返回 %d（0=无环）\n", has_dag);
    printf("  注意画面里那一堆『目标是黑色 → 正常』：DAG 里全是这种复用，\n");
    printf("  用两态 visited 判就会全线误报，用三色一声不响。✔\n");

    printf("\n  —— CYC（预期：有环 1→2→3→1，而且第一轮从孤立点 0 出发啥也找不到）——\n");
    int has_cyc = cc_has_cycle_directed(&cyc, cyc_v, &cyc_len, /* verbose=*/1);
    printf("  返回 %d（1=有环）\n", has_cyc);
    cc_print_cycle("  挖出来的环：", cyc_v, cyc_len);
    printf("  第 1 轮从 0 起：0 没有出边，直接涂黑，一无所获 → 又证明了一次『必须外层扫全图』。\n");
    printf("  命中时刻：3 → 1，而 1 是灰色（还站在路上）→ 环 = 1 → 2 → 3 →（这条边）→ 1。\n");

    printf("\n  —— 算法用错了图类型会怎样？两个函数都会**拒绝作答** ——\n");
    cc_has_cycle_undirected(&dag, cyc_v, &cyc_len, 0);
    cc_has_cycle_directed(&g, cyc_v, &cyc_len, 0);
    printf("  为什么要拒绝而不是硬算：拿无向判据查有向图，每条弧的回程都会撞上『已访问非父亲』，\n");
    printf("  一个无环的 DAG 会被判成『到处是环』；拿三色法查无向图，任何一条边立刻灰命中。\n");
    printf("  两种情况都会给出**看着挺像样的错答案** —— 这种错最要命，所以宁可不答。\n");

    /* ============================================================
       第 5 幕：有向图上"连通"这件事为什么不一样
       ============================================================ */
    section("5. 有向图：'我能到你' ≠ '你能到我' —— 强连通分量（SCC）在解决什么问题");

    int reach[CC_MAX_V];
    printf("  还是那张 DAG。先看顺着箭头'能到哪些点'（可达集，本节用一次遍历算出来）：\n\n");

    int n6 = cc_reachable(&dag, 6, reach, /* verbose=*/1);
    printf("    → 从 6 出发能到 %d 个点：{ ", n6);
    for (int i = 0; i < n6; i++) {
        printf("%d ", reach[i]);
    }
    printf("}\n\n");

    int n0 = cc_reachable(&dag, 0, reach, /* verbose=*/0);
    printf("    → 从 0 出发能到 %d 个点：{ ", n0);
    for (int i = 0; i < n0; i++) {
        printf("%d ", reach[i]);
    }
    printf("}   （0 是汇，谁都能到它，它谁都到不了）\n\n");

    printf("  看清楚了吗：6 能到 0，但 0 一步也回不到 6 —— **单向可达**。\n");
    printf("  无向图里'能到'就是'连通'，因为路是双向的；有向图里必须区分三种'连通'：\n\n");

    printf("  ① 弱连通：把所有箭头当成无向线段来看，再算连通分量。这个本节代码就能算：\n");
    int comp_d[CC_MAX_V];
    int kd = cc_find_all(&dag, comp_d, /* verbose=*/0);
    printf("     分量数 = %d", kd);
    printf("     comp_id：");
    print_by_vertex("按顶点", comp_d, dag.v);
    printf("     （adjacent_on() 里多看了 edge[j][u] 这一眼，就是在'无视方向'，所以得到弱连通）\n");

    printf("\n  ② 强连通（SCC）：块内**任意两点互相都能到**。这才是有用的那个定义。\n");
    printf("     拿 CYC 举例：{1,2,3} 是一个强连通分量（1→2→3→1，互相都到得了），\n");
    printf("     {0} 自己是一个（谁也到不了它，它也到不了谁）→ 共 2 个 SCC。\n");
    printf("     拿 DAG 举例：7 个点里有向环一个都没有 ⇒ **每个顶点自己就是一个 SCC** ⇒ 7 个。\n");
    printf("     （这条'无环 ⇒ 单点 SCC'的判断很值钱：SCC 个数 == V 等价于'这图能拓扑排序'，\n");
    printf("      正好和下一节 topological-sort 接上。）\n");

    printf("\n  ③ 强连通分量怎么求？—— **本节没有实现**，只把思路说清楚（README 里讲得更细）：\n");
    printf("     Kosaraju（两遍 DFS，好懂）：\n");
    printf("       第一遍：在原图上做全图 DFS，记下**后序**（离开顺序，上一节刚打过这张表）；\n");
    printf("       第二遍：把图的所有箭头**反过来**，按后序**倒着**的顺序依次起 DFS，\n");
    printf("              每一轮圈住的点就是一个 SCC。\n");
    printf("       妙在哪：'后序倒着'保证先碰到的是『没人能从我这里跑出去』的那一块，\n");
    printf("              反向之后箭头指回原块内部，于是这一轮出不了界 —— 一个干净的割。\n");
    printf("     Tarjan（一遍 DFS，更快）：给每个点记 dfn（进场序号）和 low（\n");
    printf("       我这一族能回到的最早的祖先），DFS 回到某点时发现 low == dfn，\n");
    printf("       就说明它是某个 SCC 的『根』，把栈上到它为止的点全吐出来 = 一个 SCC。\n");
    printf("       这就是把'灰色 = 还在路上'这件事记到极致的产物。\n");
    printf("     两者都是 O(V+E)。本节刻意不写实现：判环的三色法还没练熟就上一套双数组的\n");
    printf("     low-link，性价比很低。先把'灰/黑'想通，再回头看那两个算法会轻松很多。\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住四句话：\n");
    printf("  1) 全图遍历的外层 for 不只是'别漏点'，它本身就是算法：\n");
    printf("     一轮 DFS = 一个连通分量，轮数 = 分量个数，comp_id 还能直接判两点连通。\n");
    printf("  2) 无向图判环：DFS 时碰到『已访问且不是父亲』的邻居 ⇒ 有环。父亲这个参数不能省，\n");
    printf("     因为无向边天生会撞回自己来的那条路。\n");
    printf("  3) 有向图判环：必须三色（白/灰/黑），碰到**灰色**才算环，碰到黑色是正常复用。\n");
    printf("     无向那套判据搬过来会把 DAG 判成有环 —— 两个函数都会拒答，别硬套。\n");
    printf("  4) 有向图上'连通'有三个版本：弱连通（本节会算）、强连通（SCC，本节只讲思路）。\n");
    printf("     判环和 SCC 用的是同一件事的两个角度：环 = 灰命中；互相可达的极大块 = SCC。\n");
    return 0;
}
