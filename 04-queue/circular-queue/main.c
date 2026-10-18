#include <stdio.h>
#include <stdlib.h>
#include "cqueue.h"

/*
 * main.c —— 循环队列的五幕演示
 *
 * 这一节的主角是"取模"，所以每一幕都会把 front / rear 移动到哪、有没有绕圈打印出来。
 * 上一节（array-queue）里"明明有空位却报满"的剧本，这里会原样重演一遍，
 * 对比着看就能记住循环队列到底修好了什么。
 *
 * 编译运行（在项目根目录）：
 *   make one DIR=04-queue/circular-queue
 */

static void scene(const char *title)
{
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n");
}

static void enqueue_and_show(CQueue *q, int value)
{
    printf("\n  enqueue(%d)\n", value);
    if (cqueue_enqueue(q, value)) {
        cqueue_print(q, "现在队列：");
    } else {
        printf("    → 这次 enqueue 没成功，上面的 [失败] 就是原因\n");
    }
}

static void dequeue_and_show(CQueue *q)
{
    int value = 0;
    printf("\n  dequeue()\n");
    if (cqueue_dequeue(q, &value)) {
        printf("    出队的是 %d（队头，最早进来的那个）\n", value);
        cqueue_print(q, "现在队列：");
    } else {
        printf("    → 这次 dequeue 没成功（队列空），见上面的 [失败]\n");
    }
}

/* 方案 B 的同名小助手，避免 demo 里重复代码 */
static void w_enqueue_and_show(CQueueWaste *q, int value)
{
    printf("\n  enqueue(%d)\n", value);
    if (cqueue_w_enqueue(q, value)) {
        cqueue_w_print(q, "现在队列：");
    } else {
        printf("    → 这次 enqueue 没成功（方案 B 的判满比方案 A 早一格）\n");
    }
}

int main(void)
{
    /* ============================================================
       第 1 幕：先建立"取模 = 时钟转圈"的直觉
       ============================================================ */
    scene("第 1 幕：取模为什么就是绕圈（先把 (i+1)%4 的结果摆出来看）");

    printf("  普通数组下标：0 1 2 3 之后就出界了（越界 = 段错误风险）。\n");
    printf("  加个取模：\n    ");
    for (int i = 0; i < 10; i++) {
        printf("%d→%d  ", i, (i + 1) % 4);   /* 现在是几 → 下一格是几 */
    }
    printf("\n");
    printf("  看到规律了吗：3 → 0、7 → 0、8 → 1 …… 只要跨过 capacity 这条墙，\n");
    printf("  取模就把它拉回 0..capacity-1 里面 —— 这就是「首尾相接成一个环」。\n");
    printf("  一句话记法：下标不往右撞墙，而是沿圆周往前迈一步。\n");

    CQueue q;
    cqueue_init(&q, 4);
    cqueue_print(&q, "刚初始化（capacity=4）：");
    printf("    front=rear=size=0：三个字段一起归零，两套判据（size==0 和 front==rear）才会一致\n");
    cqueue_ring(&q);

    /* ============================================================
       第 2 幕：入队到"绕圈"发生的那一刻
       ============================================================ */
    scene("第 2 幕：入队 10 20 30 40 —— 亲眼看 rear 从最后一格绕回 0");

    enqueue_and_show(&q, 10);
    enqueue_and_show(&q, 20);
    enqueue_and_show(&q, 30);
    enqueue_and_show(&q, 40);      /* 这一步 rear 会绕圈：3 → (3+1)%4 = 0 */

    printf("\n  ★ 这一步就是循环队列的命根子：\n");
    printf("    上一节遇到 rear==capacity 就直接报错了；这里 rear 变成了 0，\n");
    printf("    于是 data[0] 里躺着【最新】的 40，而 data[3] 里是最【老】的 10。\n");
    printf("    所以「谁先谁后」不能比下标大小，只能沿环从 front 走 size 步：\n");
    cqueue_print(&q, "  ");
    printf("    绕圈累计次数 wrap_count = %d（每绕一次就是省下一次「假满报错」）\n",
           q.wrap_count);

    printf("\n  再塞一个 50 —— 这次是【真满】（size==capacity==4，环上确实一格空位都没有）：\n");
    enqueue_and_show(&q, 50);
    printf("    is_full=%s（判的是 size==capacity，不是「rear 顶到墙」）\n",
           cqueue_is_full(&q) ? "true" : "false");

    /* ============================================================
       第 3 幕：拿上一节的剧本重演一遍 —— 假满没了
       ============================================================ */
    scene("第 3 幕：把 array-queue 那出戏重演一遍：进 3 出 2 再进 —— 这次不报假满");

    CQueue p;
    cqueue_init(&p, 6);
    printf("  先按上一节的节奏：入队 10、20、30，再全部出队（此时朴素队列 front=rear=3，\n");
    printf("  左边 3 个格子永久作废）。循环队列会怎样？\n");
    enqueue_and_show(&p, 10);
    enqueue_and_show(&p, 20);
    enqueue_and_show(&p, 30);
    dequeue_and_show(&p);
    dequeue_and_show(&p);
    dequeue_and_show(&p);
    printf("\n  出完之后 front=%d、rear=%d、size=%d —— front 一样往前走，"
           "但那些格子【没有作废】：\n", p.front, p.rear, p.size);
    printf("  因为 rear 绕回来还会经过它们。继续塞人就证明了：\n");
    enqueue_and_show(&p, 100);
    enqueue_and_show(&p, 101);
    enqueue_and_show(&p, 102);   /* rear 到 5 → 绕回 0？ capacity=6：rear 5→(5+1)%6=0 */

    printf("\n  ★ 对比结论：上一节这里已经报「假满」了，本节一路畅通 ——\n");
    printf("    因为 rear 到墙不用报错，取模绕回 0 继续用空位；空位用光（size==capacity）才是真满。\n");
    printf("    本 demo 里 p 一共绕了 %d 次圈，等于避免了 %d 次「明明有空位却报错」。\n",
           p.wrap_count, p.wrap_count);

    printf("\n  peek 队尾要用「+capacity 再取模」，否则 rear==0 时会算出 -1：\n");
    int tail = 0, head = 0;
    cqueue_peek_rear(&p, &tail);
    cqueue_peek_front(&p, &head);
    printf("    队头 %d（data[%d]）、队尾 %d（data[%d]）；"
           "队尾下标 = (rear-1+cap)%%cap = (%d-1+%d)%%%d = %d\n",
           head, p.front, tail, (p.rear - 1 + p.capacity) % p.capacity,
           p.rear, p.capacity, p.capacity, (p.rear - 1 + p.capacity) % p.capacity);
    printf("    顺手验证 C 语言负数取模：( %d - 1 ) %% %d = %d ← 负数！"
           "所以必须写成 (rear - 1 + capacity) %% capacity\n",
           p.rear, p.capacity, (p.rear - 1) % p.capacity);

    printf("\n  判断「某个下标此刻活着」的正确式子（demo 里也用它打图）：\n");
    printf("    从 front 走 size 步的格子都活着，等价式子：(i - front + cap) %% cap < size\n");
    printf("    本轮（front=%d, size=%d, cap=%d）逐个下标验一遍：\n    ",
           p.front, p.size, p.capacity);
    for (int i = 0; i < p.capacity; i++) {
        printf("[%d:%s] ", i, ((i - p.front + p.capacity) % p.capacity < p.size) ? "活" : "空");
    }
    printf("\n    这一段正好还连着（3、4、5），再塞一个人就会断成两段 —— 看下一幕：\n");

    enqueue_and_show(&p, 103);   /* rear 从 0 起步 → 活着的下标变成 3,4,5,0（断成两段）*/
    printf("    同样逐个下标验一遍（front=%d, size=%d, cap=%d）：\n    ",
           p.front, p.size, p.capacity);
    for (int i = 0; i < p.capacity; i++) {
        printf("[%d:%s] ", i, ((i - p.front + p.capacity) % p.capacity < p.size) ? "活" : "空");
    }
    printf("\n    ★ 活着的格子断成两段（3、4、5 加 0），所以千万别写 `front <= i && i < rear`，"
           "也别用下标大小判断先后。\n");

    /* ============================================================
       第 4 幕：方案 B —— 少用一个格子（老教材写法）
       ============================================================ */
    scene("第 4 幕：判满的第二种方案 —— 「少用一个格子」，现场撞一次墙");

    CQueueWaste w;
    cqueue_w_init(&w, 4);
    printf("  capacity=4，但可用只有 3 格：空 = (front==rear)，满 = ((rear+1)%%capacity==front)\n");
    printf("  为什么不带 size 字段也能分清空和满？靠的就是那一格【永远不许站人】的路标。\n");
    cqueue_w_print(&w, "刚初始化：");

    w_enqueue_and_show(&w, 1);
    w_enqueue_and_show(&w, 2);
    w_enqueue_and_show(&w, 3);
    printf("\n  已经装了 3 个（= capacity-1），再塞第 4 个看看：\n");
    w_enqueue_and_show(&w, 4);
    printf("\n  ★ 这就是方案 B 的代价：数组明明还有 1 格（rear 站的那格）却不能用，\n");
    printf("    它是「路标格」——留着它才能保证：装满时 front 和 rear 差一格而不是重合。\n");
    printf("    如果硬放进去，rear 会绕回 front，状态和空队列完全一样，程序会当队列是空的 → 数据全丢。\n");

    printf("\n  放走 2 个人之后，路标格跟着 rear 移动，又能塞人了：\n");
    int v = 0;
    cqueue_w_dequeue(&w, &v);
    printf("    出队 %d\n", v);
    cqueue_w_dequeue(&w, &v);
    printf("    出队 %d\n", v);
    cqueue_w_print(&w, "现在队列：");
    w_enqueue_and_show(&w, 5);
    printf("\n  方案 B 的 size 是【现算】的：(rear - front + cap) %% cap = (%d - %d + 4) %% 4 = %d\n",
           w.rear, w.front, cqueue_w_size(&w));

    printf("\n  非法 capacity 会直接被拒（方案 B 至少要 2 格，否则路标都留不出来）：\n");
    CQueueWaste bad;
    cqueue_w_init(&bad, 1);
    cqueue_w_enqueue(&bad, 7);
    cqueue_w_dequeue(&bad, NULL);

    /* ============================================================
       第 5 幕：边界与安全
       ============================================================ */
    scene("第 5 幕：边界情况都安全（空队 dequeue/peek、非法 init、空队再入队）");

    printf("  把 p 清空：\n");
    while (!cqueue_is_empty(&p)) {
        int tmp = 0;
        cqueue_dequeue(&p, &tmp);
    }
    cqueue_print(&p, "空队列：");
    printf("    注意 front==rear==%d —— 光看这两个下标【分不清】空和满，"
           "方案 A 靠 size=0 断案\n", p.front);

    dequeue_and_show(&p);
    int dummy = 0;
    if (!cqueue_peek_front(&p, &dummy)) {
        printf("\n    → peek_front 返回 false，dummy 仍是 %d，没被写脏\n", dummy);
    }

    printf("\n  空队列再入队（环是可以一直用下去的，指针不需要归零）：\n");
    enqueue_and_show(&p, 999);

    printf("\n  非法初始化（capacity=0）：\n");
    CQueue broken;
    cqueue_init(&broken, 0);
    cqueue_enqueue(&broken, 1);
    cqueue_dequeue(&broken, NULL);

    /* ============================================================
       收尾：谁 malloc 谁 free
       ============================================================ */
    scene("收尾：四个队列都要 destroy，ASan 才不报泄漏");

    cqueue_destroy(&q);
    printf("  q      已 destroy：data=%p, front=%d, rear=%d, size=%d\n",
           (void *)q.data, q.front, q.rear, q.size);
    cqueue_destroy(&p);
    printf("  p      已 destroy\n");
    cqueue_w_destroy(&w);
    printf("  w      已 destroy\n");
    cqueue_w_destroy(&bad);
    printf("  bad    已 destroy（本来就没申请内存，free(NULL) 是合法空操作）\n");
    cqueue_destroy(&broken);
    printf("  broken 已 destroy\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住四句话：\n");
    printf("  1) 取模 (i+1) %% capacity 让下标沿环走，永远不越界 —— 这就是循环队列的全部技术含量。\n");
    printf("  2) 环上「空」和「满」都长得像 front==rear，必须额外区分：\n");
    printf("     方案 A 用 size 计数（推荐，容量不打折）；方案 B 少用一个格子（老教材，永久浪费 1 格）。\n");
    printf("  3) 判满只认 size==capacity（方案 A），「rear 顶到墙」不再是理由 —— 假满被彻底消灭。\n");
    printf("  4) 凡是下标要减 1 的地方（peek_rear、下一节的 push_front），\n");
    printf("     一律写 (x - 1 + capacity) %% capacity：C 语言里 -1 %% 4 是 -1，不是 3。\n");

    return 0;
}
