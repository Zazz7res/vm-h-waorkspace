#include <stdio.h>
#include <stdlib.h>
#include "pqueue.h"

static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/*
 * 演示数据：写死的两列，禁止随机，保证每次跑出来的输出完全一样、可人工复核。
 * 8 个任务按到达顺序编号 101..108，priority 越小越紧急（1 = 最紧急）。
 */
static const int ARRIVE_PRIO[8] = {5, 1, 3, 2, 4, 1, 9, 2};
static const int ARRIVE_ID[8]   = {101, 102, 103, 104, 105, 106, 107, 108};

int main(void)
{
    /* ============================================================
       第 1 幕：先看清"普通队列"和"优先队列"的差别
       ============================================================ */
    section("1. 普通队列 vs 优先队列：同一批任务，两种命运");

    printf("  8 个任务按到达顺序排好（数字小的优先级高）：\n");
    printf("    到达顺序: ");
    for (int k = 0; k < 8; k++) {
        printf("#%d(p%d) ", ARRIVE_ID[k], ARRIVE_PRIO[k]);
    }
    printf("\n\n  普通队列 FIFO（先来后到）会这样服务：\n    ");
    for (int k = 0; k < 8; k++) {
        printf("#%d ", ARRIVE_ID[k]);
    }
    printf("\n    ← 毛病就在这：最不紧急的 #107(p9) 排在 #108(p2) 前面被服务了，\n");
    printf("      而最紧急的 #102(p1) 却要干等 #101(p5) 做完 —— 先来后到不等于该先做的先做\n");
    printf("\n  优先队列会这样服务：\n    ");

    PQueue *q = pq_new(4);       /* 故意给 4 的初始容量，好看到翻倍扩容 */
    for (int k = 0; k < 8; k++) {
        pq_set_verbose(q, 0);    /* 这一幕只看结果，入队细节留到第 2 幕 */
        pq_enqueue(q, ARRIVE_PRIO[k], ARRIVE_ID[k]);
    }
    int served[16];
    int served_prio[16];
    int n_served = 0;
    while (!pq_empty(q)) {
        int pr = 0, id = 0;
        pq_dequeue(q, &pr, &id);
        served_prio[n_served] = pr;
        served[n_served] = id;
        n_served++;
    }
    for (int k = 0; k < n_served; k++) {
        printf("#%d(p%d) ", served[k], served_prio[k]);
    }
    printf("\n    ← 优先级从 1 一路到 9，最不紧急的 #7 最后才轮到\n");
    printf("    代价：入队 8 次 + 出队 8 次，每次 O(log n)，总共 O(n log n)\n");
    pq_free(q);
    q = NULL;                    /* free 完立刻置空，防止后面误用（00-c-foundation 的铁律） */

    /* ============================================================
       第 2 幕：入队全过程 —— 每一次上浮都看得见
       ============================================================ */
    section("2. enqueue 全过程：站到队尾 + 上浮");

    q = pq_new(4);
    printf("  依次入队 6 个任务：#1(p5) #2(p1) #3(p3) #4(p2) #5(p6) #6(p0)\n\n");
    int p2[] = {5, 1, 3, 2, 6, 0};
    for (int k = 0; k < 6; k++) {
        pq_enqueue(q, p2[k], k + 1);
        printf("\n");
    }

    printf("  全部入队后，队列内部是一棵这样的树：\n");
    pq_print(q);
    printf("\n  核对：父亲优先级一定 <= 孩子。下标 0 是 p0(#6)——最紧急的插到了最前面，\n");
    printf("        尽管它是最后一个到达的。这就是「优先」两个字的含义。\n");

    /* ============================================================
       第 3 幕：出队全过程 —— 交换队尾 + 缩容 + 下沉
       ============================================================ */
    section("3. dequeue 全过程：每次拿到的都是当前最优先的");

    int prev_priority = -999;
    int monotone = 1;
    for (int k = 0; k < 6; k++) {
        int pr = 0, id = 0;
        printf("  ===== 第 %d 次出队（队列里还有 %d 人）=====\n", k + 1, pq_size(q));
        if (pq_dequeue(q, &pr, &id) == 0) {
            printf("    >>> 被服务的是 #%d，优先级 p%d，剩余 %d 人\n", id, pr, pq_size(q));
            if (pr < prev_priority) {
                monotone = 0;
            }
            prev_priority = pr;
        }
        printf("\n");
    }
    printf("  出队序列的优先级是否【非递减】： %s\n", monotone ? "是 ✅（这就是优先队列的定义）" : "否 ❌");

    /* ============================================================
       第 4 幕：peek —— 看一眼，不动结构
       ============================================================ */
    section("4. peek：只问下一个是谁，不真出队");

    pq_enqueue(q, 7, 21);
    pq_enqueue(q, 2, 22);
    pq_set_verbose(q, 0);
    pq_enqueue(q, 2, 23);
    pq_set_verbose(q, 1);

    int hp = -1, hi = -1;
    int size_before = pq_size(q);
    pq_peek(q, &hp, &hi);
    printf("  peek → 下一个会被服务的是 #%d（优先级 p%d），但队列长度仍是 %d（peek 不删任何东西）\n",
           hi, hp, pq_size(q));
    printf("  peek 前后长度一致：%s\n", size_before == pq_size(q) ? "✅ 没变" : "❌ 变了");
    printf("  现在队列长这样：\n");
    pq_print(q);

    /* ============================================================
       第 5 幕：⚠⚠ 本节最大坑 —— 优先级相同怎么办？
       ============================================================ */
    section("5. 关键坑：优先级相同的任务，先来【不】先服务");

    PQueue *same = pq_new(8);
    printf("  连续入队 4 个【优先级都是 1】的任务：#101 #102 #103 #104（按这个顺序到达）\n");
    pq_set_verbose(same, 0);
    for (int k = 0; k < 4; k++) {
        pq_enqueue(same, 1, 101 + k);
    }
    printf("  入队完，数组顺序: ");
    for (int k = 0; k < pq_size(same); k++) {
        printf("#%d ", same->data[k].id);
    }
    printf("  ← 看着是 FIFO 对吧？别急，那只是因为还没有任何元素需要搬动。\n\n");

    int out_ids[8];
    int out_n = 0;
    printf("  开始按顺序出队：\n");
    while (!pq_empty(same)) {
        int pr = 0, id = 0;
        pq_dequeue(same, &pr, &id);
        out_ids[out_n++] = id;
        printf("    出队 #%d（p%d）\n", id, pr);
    }

    printf("\n  期望（先来先服务 / FIFO 稳定）: 101 102 103 104\n");
    printf("  实际（堆的真实行为）        : ");
    for (int k = 0; k < out_n; k++) {
        printf("%d ", out_ids[k]);
    }
    printf("\n");

    int stable = 1;
    for (int k = 0; k < out_n; k++) {
        if (out_ids[k] != 101 + k) {
            stable = 0;
        }
    }
    printf("  是否稳定：%s   ← 注意程序【没有报错】，优先级也照样全对，只是同优先级之间不公平\n",
           stable ? "是（这次碰巧）" : "❌ 不稳定");

    printf("\n  为什么会这样？两个原因叠加：\n");
    printf("    ① 比较函数只看 priority。两个任务 p 相同 → a<b 和 b<a 都是假，\n");
    printf("       堆认为「谁在上面都行」，所以入队时【不会】为了先后顺序做任何事。\n");
    printf("    ② 出队时把【最后一个】元素搬到队首，再下沉。这个搬运规则和到达顺序毫无关系，\n");
    printf("       于是最后到的 #104 被直接提到队首附近，插到了 #102、#103 前面。\n");
    printf("       （你可以自己心算一遍：4 个元素出队第 1 次后，data[3] 的 #104 被换到 data[0]。）\n");
    printf("\n  工程上怎么破？—— 给每个任务发一个【自增序号】，比较按双键：\n");
    printf("       typedef struct { int priority; int id; int seq; } Task;\n");
    printf("       wins(a,b):  if (a->priority != b->priority) return a->priority < b->priority;\n");
    printf("                    return a->seq < b->seq;      // 优先级相同 → 序号小的先走\n");
    printf("       enqueue 时写 seq = q->enqueued++;\n");
    printf("  只改比较那 2 行 + 入队记 1 个字段，稳定性立刻就有，复杂度一点没变（O(log n)）。\n");
    printf("  本项目【不实现】它：因为「堆本身不保证稳定」这个事实值得你踩一次坑记住。\n");
    printf("  顺带：这也是为什么 C++ 的 priority_queue、甚至很多排序实现都标榜「堆排序不稳定」。\n");
    pq_free(same);
    same = NULL;

    /* ============================================================
       第 6 幕：真实节奏 —— 到达和处理交替进行
       ============================================================ */
    section("6. 混合节奏：入队、出队交替（并数一数搬动次数）");

    /* 事件流写死：+ 表示新任务到达，- 表示服务掉一个 */
    struct Event { char kind; int priority; int id; };
    static const struct Event stream[] = {
        {'+', 4, 201}, {'+', 2, 202}, {'-', 0, 0},   {'+', 7, 203},
        {'+', 1, 204}, {'-', 0, 0},   {'-', 0, 0},   {'+', 3, 205},
        {'+', 3, 206}, {'-', 0, 0},   {'+', 0, 207}, {'-', 0, 0},
        {'-', 0, 0},   {'-', 0, 0},   {'-', 0, 0},   {'-', 0, 0}
    };
    int stream_n = (int)(sizeof(stream) / sizeof(stream[0]));

    pq_set_verbose(q, 0);
    pq_reset_swaps(q);
    printf("  事件流（+ 是到达，- 是服务掉一个；队列从第 4 幕遗留状态继续，先清空）\n");
    while (!pq_empty(q)) {
        int pr = 0, id = 0;
        pq_dequeue(q, &pr, &id);
    }
    pq_set_verbose(q, 1);
    pq_reset_swaps(q);
    long swaps_before = 0;
    for (int k = 0; k < stream_n; k++) {
        if (stream[k].kind == '+') {
            printf("  [到达] #%d p%d\n", stream[k].id, stream[k].priority);
            swaps_before = pq_swaps(q);
            pq_enqueue(q, stream[k].priority, stream[k].id);
        } else if (pq_empty(q)) {
            printf("  [服务] 队列空着，工人休息\n");
            continue;
        } else {
            int pr = 0, id = 0;
            swaps_before = pq_swaps(q);
            pq_dequeue(q, &pr, &id);
            printf("  [服务] 拿走 #%d（p%d）；剩余 %d 人\n", id, pr, pq_size(q));
        }
        int tree_h = 0;
        while ((1 << tree_h) <= pq_size(q)) {
            tree_h++;                     /* 完全二叉树的层数：满足 2^0+…+2^(h-1) >= size */
        }
        printf("         本次操作发生交换 %ld 次（当前 %d 人，树高 %d 层 → 单次操作最多也就 %d 次交换）\n",
               pq_swaps(q) - swaps_before, pq_size(q), tree_h, tree_h);
    }
    printf("\n  全程 16 个操作累计交换 %ld 次。这里规模小（最多 3 人），所以一次操作最多 1~2 次；\n",
           pq_swaps(q));
    printf("  人数上万时一次操作会涨到 3~14 次交换 —— 注意它是【跟着 log 慢慢涨】，\n");
    printf("  而不是跟着人数线性涨。这就是 O(log n) 的实际样子（对比：线性扫是每人一次）。\n");

    /* ============================================================
       第 7 幕：边界情况
       ============================================================ */
    section("7. 边界：空队列、只有一个元素、负优先级");

    PQueue *tiny = pq_new(1);
    int pr = 123, id = 456;
    printf("  空队列 peek    → 返回 %d（-1=没有，且 pr/id 保持你给的初值 %d/%d，没被污染）\n",
           pq_peek(tiny, &pr, &id), pr, id);
    pr = 123; id = 456;
    printf("  空队列 dequeue → 返回 %d\n", pq_dequeue(tiny, &pr, &id));
    printf("  空队列 pq_print → ");
    pq_print(tiny);

    printf("\n  负优先级是完全合法的（数字小就是更优先）：入队 p-5 和 p3\n");
    pq_enqueue(tiny, 3, 300);
    pq_enqueue(tiny, -5, 301);
    int got_pr = 0, got_id = 0;
    pq_peek(tiny, &got_pr, &got_id);
    printf("  peek → #%d(p%d) ← 负数赢了\n", got_id, got_pr);
    pq_dequeue(tiny, &got_pr, &got_id);
    printf("  dequeue 一次后剩 %d 人：", pq_size(tiny));
    pq_print(tiny);
    pq_dequeue(tiny, &got_pr, &got_id);
    printf("  最后一个元素出队（不发生交换，直接 size--）：拿走 #%d(p%d)，剩余 %d 人\n",
           got_id, got_pr, pq_size(tiny));
    pq_free(tiny);
    tiny = NULL;
    pq_free(q);
    q = NULL;

    printf("\n=== 本节结束 ===\n");
    printf("记住四句话：\n");
    printf("  1) 优先队列 = 堆 + 一层薄壳接口。对外只有 enqueue / dequeue / peek / size。\n");
    printf("  2) dequeue 一定拿到当前最优先的那个；第二优先是谁，堆完全不知道。\n");
    printf("  3) 优先级相同的任务【不保证先来先服务】—— 想稳定就加自增序号做双键比较。\n");
    printf("  4) 每次操作 O(log n)（树高几层就最多几次交换）。普通队列入队 O(1)，但代价是「只能先来后到」。\n");
    return 0;
}
