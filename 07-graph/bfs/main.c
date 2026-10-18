#include <stdio.h>
#include "gbfs.h"

/*
 * main.c —— BFS 分幕演示
 *
 * 四幕：
 *   1. 先把要遍历的图摊开看清楚
 *   2. 从 0 出发做**带解说**的 BFS —— 一层一层往外扩的全过程
 *   3. 看结果三张表：访问顺序 / dist / parent + 回溯出具体路径
 *   4. 不连通的现实：从 0 出发访问不到 4、5、6、7 → 必须对每个未访问点再起一次 BFS
 */

static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

int main(void)
{
    int dist[BFS_MAX_V];
    int parent[BFS_MAX_V];
    int order[BFS_MAX_V];
    int order_len = 0;

    /* ============================================================
       第 1 幕：先看清这张图
       ============================================================ */
    section("1. 要遍历的图：共用示例图 G（8 点 7 边，故意不连通）");

    BfsGraph g;
    bfs_graph_sample(&g);
    printf("  顶点 v = %d，边 e = %d\n\n", g.v, g.e);
    bfs_graph_print(&g);
    printf("  说明：矩阵第 i 行的 1 就是 i 的邻居，BFS 里『扫邻居』= 扫第 i 行。\n");
    printf("  第 7 行全 0（孤立点）—— 待会儿你会亲眼看到：从 0 出发谁也到不了它。\n");

    /* ============================================================
       第 2 幕：从 0 出发的 BFS，全过程解说
       ============================================================ */
    section("2. 从顶点 0 出发：一层一层往外扩（verbose 模式）");

    int reached = bfs_run(&g, 0, dist, parent, order, &order_len, /* verbose=*/1);
    printf("\n  本轮访问到 %d 个顶点。\n", reached);

    /* ============================================================
       第 3 幕：把三张表摊开 + 回溯路径
       ============================================================ */
    section("3. 结果三张表：访问顺序 / 最少步数 dist / 发现者 parent");

    bfs_print_result(&g, 0, dist, parent, order, order_len);

    printf("\n  逐条解释这张表为什么是这样：\n");
    printf("    dist[0]=0     起点自己 0 步，天经地义\n");
    printf("    dist[1]=1     1 是 0 的直接邻居，1 步就到\n");
    printf("    dist[2]=1     2 也是 0 的直接邻居 —— 注意 0-1-2 成环，\n");
    printf("                  但 BFS 从 0 一圈就把 1、2 一起收进第 1 层了\n");
    printf("    dist[3]=2     3 只能经 2 到达：0→2→3，2 步\n");
    printf("    parent[1]=0、parent[2]=0、parent[3]=2 → 这三条父子边就是 BFS 树\n");
    printf("    ★ 为什么 dist[3] 不是 1？ 3 只连 2，走 0→2→3 必须 2 步，\n");
    printf("      BFS 第一次碰到 3 就是在第 2 层，之后从别的路再碰到它只会 ≥2 步。\n");
    printf("      这就是『第一次到达即最短』的现场。\n");

    /* 再换一个起点，证明 dist/parent 依赖起点（同一张图，树不一样） */
    section("3b. 换个起点：从 3 出发，同一张图的树长得不一样");

    bfs_run(&g, 3, dist, parent, order, &order_len, /* verbose=*/0);
    bfs_print_result(&g, 3, dist, parent, order, order_len);
    printf("  对比两次：从 0 出发 dist[3]=2；从 3 出发 dist[0]=2。\n");
    printf("  但 parent 树完全不同 —— 树的形状由『起点 + 邻居扫描顺序』共同决定。\n");
    printf("  所以 BFS 树不唯一，可『每个点的 dist』是唯一的（那才是真正的最短距离）。\n");

    /* ============================================================
       第 4 幕：不连通怎么办 —— 对每个未访问点都起一次 BFS
       ============================================================ */
    section("4. 关键教训：一次 BFS 只覆盖一个连通分量");

    bfs_run(&g, 0, dist, parent, order, &order_len, /* verbose=*/0);
    printf("  刚跑完『从 0 出发的 BFS』，逐点可达情况：\n");
    for (int u = 0; u < g.v; u++) {
        printf("    顶点 %d：dist=%-3d %s\n", u, dist[u],
               (dist[u] < 0) ? "← 到不了！" : "");
    }
    printf("  队列正常清空、返回值正常、没有任何报错 —— 但你**整片漏掉了 4 个顶点**。\n");
    printf("  这类『结果少了一坨但毫无异常』的 bug 最难发现，所以全图遍历必须套两层：\n");
    printf("      for 每个顶点 u:\n");
    printf("          if u 没被访问过:\n");
    printf("              从 u 起一次 BFS        ← 这一轮覆盖一个新的连通分量\n");
    printf("\n  现在用 bfs_run_all 重跑，看看补齐之后的全过程：\n");

    int rounds = bfs_run_all(&g, dist, parent, order, &order_len, /* verbose=*/1);

    printf("\n  ═══ 全图遍历结束 ═══\n");
    printf("  一共起了 %d 轮 BFS → 说明这张图有 %d 个连通分量\n", rounds, rounds);
    printf("  访问总顺序（%d 个，正好是全部顶点）：", order_len);
    for (int i = 0; i < order_len; i++) {
        printf("%d ", order[i]);
    }
    printf("\n");

    printf("\n  dist 表（注意含义变了：每一轮各自的起点 dist=0）：\n");
    printf("    顶点：");
    for (int u = 0; u < g.v; u++) {
        printf("%-4d", u);
    }
    printf("\n    dist ：");
    for (int u = 0; u < g.v; u++) {
        printf("%-4d", dist[u]);
    }
    printf("\n    parent：");
    for (int u = 0; u < g.v; u++) {
        printf("%-4d", parent[u]);
    }
    printf("\n");
    printf("  parent 里出现了三个 -1：0、4、7 —— 它们正是三轮 BFS 各自的起点。\n");
    printf("  所以跑完 bfs_run_all 之后，**-1 不再等于『到不了』**：它现在表示\n");
    printf("  『我是某个分量的源头』。想判断可达性要看 dist，不要只看 parent ——\n");
    printf("  这是复用同一张表带来的语义漂移，写代码时必须自己盯住（上面单轮跑法里，\n");
    printf("  dist=-1 才是真的到不了；这里每轮起点会重新把 dist 写成 0）。\n");
    printf("  孤立点 7 独占一轮：它自己就是它自己的连通分量 —— 度 0 也算一个分量。\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住四句话：\n");
    printf("  1) BFS 靠**队列**才能一圈一圈扩；换成栈就变成 DFS，最短路性质立刻失效。\n");
    printf("  2) 无权图里 dist[] 就是最短距离：第一次到达一定最短，后面再遇到只会更远。\n");
    printf("  3) 标记要打在**入队那一刻**，不是出队时。否则同点重复入队，队列和结果一起坏。\n");
    printf("  4) 图可能不连通：全图遍历必须『对每个未访问点都起一次 BFS』，起了几轮就是几个分量。\n");
    return 0;
}
