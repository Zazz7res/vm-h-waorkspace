#include <stdio.h>
#include "gdfs.h"

/*
 * main.c —— DFS 分幕演示
 *
 * 五幕：
 *   1. 先把要遍历的图摊开看清楚（顺便固定住"邻居顺序"这件事）
 *   2. 递归版从 0 出发：用缩进把"一路钻到底 + 一层层回溯"整个画面演出来
 *   3. 显式栈·帧模拟版：程序自动逐格比对，证明它和递归版结果完全一致
 *      → 这就是"递归就是系统在帮你维护栈"的证据
 *   4. 显式栈·朴素版四种组合：看清楚压栈方向和打标记时机各自是怎么改变结果的
 *   5. 全图遍历：一轮 DFS 只覆盖一个连通分量，必须外层再扫一遍
 */

/* 把两份 DfsResult 逐格比对，返回"不同的格子数"。0 表示完全一致 */
static int diff_count(const DfsResult *a, const DfsResult *b)
{
    int bad = 0;
    if (a->order_len != b->order_len || a->post_len != b->post_len) {
        bad++;
    }
    for (int i = 0; i < GDFS_MAX_V; i++) {
        if (a->order[i] != b->order[i] || a->post[i] != b->post[i]) {
            bad++;
        }
        if (a->parent[i] != b->parent[i] || a->depth[i] != b->depth[i]) {
            bad++;
        }
        if (a->tick_in[i] != b->tick_in[i] || a->tick_out[i] != b->tick_out[i]) {
            bad++;
        }
    }
    return bad;
}

/* 打一条"某种跑法的结果"小结。刻意不用宽度对齐的表格：
 * 中文在 printf 里按字节算宽度，一 %-20s 就错行 —— 详见 README 的坑清单。 */
static void print_row(const char *tag, const char *desc, const DfsResult *r, int v)
{
    printf("  %s  %s\n", tag, desc);
    printf("      前序（%d 个）：", r->order_len);
    for (int i = 0; i < r->order_len; i++) {
        printf("%d ", r->order[i]);
    }
    printf("\n      parent ：");
    for (int u = 0; u < v; u++) {
        printf("%d ", r->parent[u]);
    }
    printf("   depth ：");
    for (int u = 0; u < v; u++) {
        printf("%d ", r->depth[u]);
    }
    printf("\n      后序（%d 个）：", r->post_len);
    for (int i = 0; i < r->post_len; i++) {
        printf("%d ", r->post[i]);
    }
    if (r->post_len == 0) {
        printf("（空：这一版没有『离开』这一刻）");
    }
    printf("   重复出栈：%d 次\n", r->dup_pops);
}

static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

int main(void)
{
    DfsGraph g;
    dfs_graph_sample(&g);

    /* ============================================================
       第 1 幕：先看清这张图
       ============================================================ */
    section("1. 要遍历的图：共用示例图 G（8 点 7 边，故意不连通）");

    printf("  顶点 v = %d，边 e = %d\n\n", g.v, g.e);
    dfs_graph_print(&g);
    printf("  ★ 全节统一规矩：扫邻居 = 扫矩阵第 u 行，列号 j 从小到大。\n");
    printf("    DFS 的结果本来就依赖邻居顺序，先把顺序钉死，输出才可复现、才能逐行核对。\n");

    /* ============================================================
       第 2 幕：递归版，缩进 = 钻得有多深
       ============================================================ */
    section("2. 递归版：一条路走到黑（缩进表示递归深度，也就是系统栈里有几层帧）");

    DfsResult rec;
    int n_rec = dfs_recursive(&g, 0, &rec, /* verbose=*/1);
    printf("  这一轮访问到 %d 个顶点。\n", n_rec);

    printf("\n  看上面那个画面，把『深度优先』四个字读成动词：\n");
    printf("    第 1 行缩进 0 格：站在 0，先看邻居 1 → 立刻钻；\n");
    printf("    缩进 2 格：站在 1，邻居 0 是父亲（回来的路），邻居 2 没来过 → 再钻；\n");
    printf("    缩进 4 格：站在 2，邻居 0 已访问又不是父亲（后向边，环的证据）、\n");
    printf("              邻居 1 是父亲、邻居 3 没来过 → 继续钻；\n");
    printf("    缩进 6 格：站在 3，唯一的邻居 2 是父亲 → 没路了，开始回溯；\n");
    printf("    然后一层层退回来：3 → 2 → 1 → 0，缩进每次少两格。\n");
    printf("  最深处缩进 6 格 = 系统栈里同时躺着 4 个帧（dfs_go(0)/(1)/(2)/(3)）。\n");
    printf("  注意『当前路径』这一行：进到 3 时它是 0 -> 1 -> 2 -> 3，回溯后逐格缩短，\n");
    printf("  这就是 trail[]，它和帧栈里的内容是一一对应的。\n");

    printf("\n  === 递归版的四张表 ===\n");
    dfs_print_result(&g, &rec);

    /* ============================================================
       第 3 幕：帧模拟版 —— 把系统藏起来的栈摆到桌面上
       ============================================================ */
    section("3. 显式栈·帧模拟版：自己压 (顶点, 邻居游标)，看它能不能完全复刻递归版");

    DfsResult frm;
    int n_frm = dfs_stack_frames(&g, 0, &frm, /* verbose=*/1);
    printf("  本轮访问 %d 个顶点（递归版是 %d 个）。\n", n_frm, n_rec);

    printf("\n  === 帧模拟版的四张表 ===\n");
    dfs_print_result(&g, &frm);

    int bad = diff_count(&rec, &frm);
    printf("\n  ★ 自动逐格比对（前序、后序、parent、depth、进刻度、出刻度，共 6 张表 × %d 格）：\n",
           GDFS_MAX_V);
    printf("    不同的格子数 = %d → %s\n", bad,
           bad == 0 ? "完全一致！递归版干的每件事都能自己手写出来" : "有差异，见上面两张表");
    printf("    对照翻译表：栈帧 ↔ Frame{v,next}；局部变量 j ↔ Frame.next；\n");
    printf("              函数调用 ↔ fs_push；return/回溯 ↔ fs_pop；栈空 ↔ 整轮结束。\n");
    printf("    结论：**递归就是系统在帮你维护栈**，它不是魔法，只是那个栈你看不见。\n");

    /* ============================================================
       第 4 幕：朴素栈版 —— 顺序为什么会不一样，树为什么会塌
       ============================================================ */
    section("4. 显式栈·朴素版（一发现邻居就全压进去）：四种组合的结果全打出来对比");

    printf("  参照答案（递归版）：前序 0 1 2 3 / parent -1 0 1 2 ... / depth 0 1 2 3 ... / 后序 3 2 1 0\n\n");

    printf("  先带解说地跑一种：压栈=升序、标记=入栈时（这就是最常见的写法）\n\n");
    DfsResult one;
    dfs_stack_naive(&g, 0, /* push_ascending=*/1, /* mark_on_pop=*/0, &one, /* verbose=*/1);

    printf("\n  四种组合对比：\n");
    DfsResult na, nb, nc, nd;
    dfs_stack_naive(&g, 0, 1, 0, &na, 0);
    print_row("A)", "压栈=升序 / 标记=入栈时  ← 前序和递归版不同（栈把顺序反过来了）", &na, g.v);
    dfs_stack_naive(&g, 0, 0, 0, &nb, 0);
    print_row("B)", "压栈=降序 / 标记=入栈时  ← 前序和递归版相同，但 parent/depth 变了！", &nb, g.v);
    dfs_stack_naive(&g, 0, 1, 1, &nc, 0);
    print_row("C)", "压栈=升序 / 标记=出栈时  ← 危险写法：同一个点被重复压栈", &nc, g.v);
    dfs_stack_naive(&g, 0, 0, 1, &nd, 0);
    print_row("D)", "压栈=降序 / 标记=出栈时  ← 同样会重复压栈，parent 被最后一个压它的人抢走", &nd, g.v);

    printf("\n  四个结论，逐条对着上面看：\n");
    printf("  ① 压栈方向决定前序：A/C 按升序压 → 栈顶剩下最大的那个 → 先处理 2，前序变成 0 2 3 1。\n");
    printf("     B/D 按降序压 → 先处理 1 → 前序回到 0 1 2 3。**栈是后进先出，压栈顺序必须反着来**。\n");
    printf("  ② 打标记时机决定树：B 的前序 0 1 2 3 和递归版一模一样，\n");
    printf("     但 parent 是 -1 0 0 2，递归版是 -1 0 1 2 —— 顶点 2 的『发现者』被 0 抢走了。\n");
    printf("     因为 0 一出场就把 1 和 2 **全都**压进栈并标记了，\n");
    printf("     递归版则是 0 先看 1、1 一路钻到 2，2 才是被 1 带进来的。\n");
    printf("     前序相同、DFS 树不同 —— 所以『顺序对得上』不等于『实现等价』。\n");
    printf("  ③ 出栈才标记（C/D）会把同一个点压好几次：看『重复出栈 1 次』那一栏。\n");
    printf("     弹到一个已处理过的点必须 continue，否则同一个点会被处理两遍、前序里出现重复。\n");
    printf("     另外 parent 会被『最后一个压它的人』覆盖（C 里 parent[1]=2，可 1 明明是 0 先发现的）。\n");
    printf("  ④ 四种组合的**后序全是空的**：朴素版弹出即结束，根本没有『离开』这个时刻。\n");
    printf("     需要后序的地方（下一节的拓扑排序、判环的细节）只能用递归版或帧模拟版。\n");

    /* ============================================================
       第 5 幕：全图遍历 —— 一轮 DFS 只覆盖一个连通分量
       ============================================================ */
    section("5. 全图遍历：从 0 出发只覆盖了一个分量，剩下 4 个点怎么补？");

    printf("  上面递归版从 0 出发，前序只有 %d 个点，顶点 4/5/6/7 的 parent 和 depth 全是 -1：\n",
           rec.order_len);
    printf("      前序：");
    for (int i = 0; i < rec.order_len; i++) {
        printf("%d ", rec.order[i]);
    }
    printf("\n      栈自己走空了，也没有任何报错 —— 但你整片漏掉了 4 个顶点。\n");
    printf("  所以全图遍历必须套两层：\n");
    printf("      for 每个顶点 u:\n");
    printf("          if u 没被访问过:\n");
    printf("              从 u 起一轮 DFS        ← 这一轮覆盖一个新的连通分量\n");
    printf("\n  现在用 dfs_all 重跑，看补齐之后的全过程：\n");

    DfsResult all;
    int rounds = dfs_all(&g, &all, /* verbose=*/1);
    printf("\n  ═══ 全图遍历结束 ═══\n");
    printf("  一共起了 %d 轮 DFS → 说明这张图有 %d 个连通分量\n", rounds, rounds);
    printf("\n  === 全图遍历的四张表 ===\n");
    dfs_print_result(&g, &all);

    printf("  关于这张表还有两点要说清：\n");
    printf("  1) parent 里出现了三个 -1：0、4、7 —— 它们是三轮 DFS 各自的根。\n");
    printf("     所以跑完 dfs_all 之后，**-1 不再等于『没访问到』**，它表示『我是某轮的根』。\n");
    printf("     真要区分这两种 -1，看 tick_in：还是 -1 才是压根没访问到。\n");
    printf("     （本节图 G 全被访问到了，所以没有真的漏点。上一节 BFS 的 dist 表也是同一个坑。）\n");
    printf("  2) 后序 3 2 1 0 | 6 5 4 | 7 是『每轮内部，子先父后』。\n");
    printf("     把这个顺序**倒过来**，对无环的有向图就是拓扑序 —— 这正是下一节要用的东西。\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住五句话：\n");
    printf("  1) DFS = 一条路走到黑再回头；和 BFS 的差别只在待办用**栈**还是用**队列**。\n");
    printf("  2) 进函数第一件事就是打标记。图里有环，不标记就是无限递归 → 爆栈（段错误）。\n");
    printf("  3) 递归不是魔法：系统在每个帧里替你记着『我是谁带来的、我的邻居扫到第几个了』。\n");
    printf("     手写栈要存的是**帧（顶点 + 游标）**，只存顶点会得出另一棵树（本节实测给你看了）。\n");
    printf("  4) DFS 的 depth 不是最短距离：本节 depth[3]=3，而 BFS 的 dist[3]=2。\n");
    printf("  5) 一轮 DFS 只覆盖一个连通分量；全图遍历要外层扫每个未访问点。\n");
    return 0;
}
