#include <stdio.h>
#include <stdlib.h>
#include "aqueue.h"

/*
 * main.c —— 朴素数组队列的四幕演示
 *
 * 每一幕之后都会把整排格子打印出来，并在第二行标出 front（F）和 rear（R）的位置，
 * 所以你能亲眼看见「两个指针一起往右爬、左边留下一串作废格子」这件事，
 * 也就是本节最重要的教学目标：**假满是怎么一步步发生的**。
 *
 * 编译运行（在项目根目录）：
 *   make one DIR=04-queue/array-queue
 */

/* 打印一条分隔线 + 标题，让输出好读 */
static void scene(const char *title)
{
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n");
}

/* 入队一个值并把队列打出来 —— demo 里几乎每步都要，抽成小函数少写重复代码 */
static void enqueue_and_show(AQueue *q, int value)
{
    printf("\n  enqueue(%d)\n", value);
    if (aqueue_enqueue(q, value)) {
        aqueue_print(q, "现在队列：");
    } else {
        printf("    → 这次 enqueue 没成功，上面的 [失败] 就是原因\n");
    }
}

/* 出队一个值、把出队的人显示出来、再打印队列 */
static void dequeue_and_show(AQueue *q)
{
    int value = 0;   /* 故意先放个 0，方便看出"是不是真被写进去了" */
    printf("\n  dequeue()\n");
    if (aqueue_dequeue(q, &value)) {
        printf("    出队的是 %d（队头，最早进来的那个）\n", value);
        aqueue_print(q, "现在队列：");
    } else {
        printf("    → 这次 dequeue 没成功（队列空），见上面的 [失败]\n");
    }
}

int main(void)
{
    /* ============================================================
       第 1 幕：先进先出到底长什么样
       目标：看清 front / rear 各自的位置，以及"后进的人排在后面、先来的先走"
       ============================================================ */
    scene("第 1 幕：FIFO —— 入队 10 20 30，出队顺序一定是 10 20 30");

    AQueue q;
    aqueue_init(&q, 6);
    aqueue_print(&q, "刚初始化：");
    printf("    front==rear==0 —— 两个指针重合就表示队伍是空的；人数 = rear - front = 0\n");

    enqueue_and_show(&q, 10);
    enqueue_and_show(&q, 20);
    enqueue_and_show(&q, 30);

    printf("\n  看一眼两头，但不让人走：\n");
    int head = 0, tail = 0;
    aqueue_peek_front(&q, &head);
    aqueue_peek_rear(&q, &tail);
    printf("    队头是 %d（data[%d]），队尾是 %d（data[%d]）；"
           "注意队尾在 rear-1 那格，因为 rear 指的是「下一个入队位置」\n",
           head, q.front, tail, q.rear - 1);

    dequeue_and_show(&q);      /* 10 */
    dequeue_and_show(&q);      /* 20 */
    dequeue_and_show(&q);      /* 30 */

    printf("\n  看到没：入队 10→20→30，出队 10→20→30。队列的全部灵魂就这一句话。\n");
    printf("  真实用途：打印任务队列、BFS 逐层扩展、消息队列、线程池任务分发、抢票排队。\n");
    aqueue_print(&q, "全部出队之后：");
    printf("    ⚠ 注意 front 和 rear 都停在 3，**不是回到 0**！格子 [0][1][2] 里的 10/20/30\n");
    printf("      数值还在内存里，只是被 front 划到区间外面去了 → 这就是浪费的开始。\n");

    /* ============================================================
       第 2 幕：现场制造「假满」—— 本节最重要的一幕
       容量 6，front/rear 从 3 出发：再进 3 个人 rear 就到墙了，
       此时出队 2 个人，队列里只剩 1 个人，却已经一格都塞不进去。
       ============================================================ */
    scene("第 2 幕：⚠ 现场制造「假满」—— front/rear 只会往右，从不回头");

    printf("  第 1 步：接着往右塞 3 个人（100、101、102），rear 就会顶到墙 capacity=6\n");
    enqueue_and_show(&q, 100);
    enqueue_and_show(&q, 101);
    enqueue_and_show(&q, 102);
    printf("\n  此时 size=3, front=3, rear=6，右边可用格子 = 0，左边作废格子 = 3。\n");
    printf("  这还算「正常满了」吗？不算 —— 队列里只有 3 个人，容量是 6，明明还能装 3 个。\n");

    printf("\n  第 2 步：放走 2 个人（100、101），front 往右爬，腾出左边 5 个空格\n");
    dequeue_and_show(&q);
    dequeue_and_show(&q);

    printf("\n  第 3 步：现在队列里只剩 1 个人（102），试着再塞一个 103 —— 见证假满：\n");
    enqueue_and_show(&q, 103);
    printf("\n  ★ 把这一路的账算给你看：\n");
    aqueue_stats(&q);
    printf("    等式：size(%d) + 作废格子(%d) + 右边可用(%d) = capacity(%d)\n",
           aqueue_size(&q), aqueue_wasted_cells(&q),
           q.capacity - q.rear, q.capacity);
    printf("    明明有 %d 个格子是空的（前面 %d 个 + 后面 %d 个），程序却说「满了」——\n",
           aqueue_wasted_cells(&q) + (q.capacity - q.rear),
           aqueue_wasted_cells(&q), q.capacity - q.rear);
    printf("    is_full=%s（rear 真的顶到墙了）、is_fake_full=%s（但前面有空位）\n",
           aqueue_is_full(&q) ? "true" : "false",
           aqueue_is_fake_full(&q) ? "true" : "false");
    printf("    ⚠ 这就是假满，下一节用取模解决：04-queue/circular-queue/\n");
    printf("      核心就一行：rear = (rear + 1) %% capacity; —— 到墙了不报错，而是绕回第 0 格。\n");

    /* ============================================================
       第 3 幕：笨办法救急 —— 整体左搬（compact）
       目标：证明「假满能治」，同时证明「这么治代价太大」，从而引出取模方案
       ============================================================ */
    scene("第 3 幕：笨办法 compact：把活着的元素整体搬回 data[0]");

    printf("  搬之前（102 卡在 data[5]，左边 0..4 全是作废空格）：\n");
    aqueue_print(&q, "  ");
    printf("\n  调用 aqueue_compact()：\n");
    aqueue_compact(&q);
    aqueue_print(&q, "搬完之后：");

    printf("\n  现在能塞人了（front=0，右边整片都是空的）：\n");
    enqueue_and_show(&q, 103);
    enqueue_and_show(&q, 104);
    enqueue_and_show(&q, 105);

    printf("\n  结论：compact 确实能救急，但它一次要搬 size 个元素 = O(n)。\n");
    printf("    如果每次出队都顺手搬一次，出队就从 O(1) 退化成 O(n)，\n");
    printf("    队列「两头都快」的优势全丢了 —— 所以工程上不这么干，而是用取模绕圈。\n");

    /* ============================================================
       第 4 幕：两个极端 —— 「真满」和「空队列却塞不进」
       ============================================================ */
    scene("第 4 幕：两个极端：真满 vs 空队列塞不进（假满的终极形态）");

    printf("  接着塞，把 front 挪回 0 之后的这排格子用到头（此时左边一格没浪费）：\n");
    enqueue_and_show(&q, 106);
    enqueue_and_show(&q, 107);
    printf("\n  再塞一个 108 —— 这回是【真满】（size=6 == capacity=6，front=0 一个格子都没浪费）：\n");
    enqueue_and_show(&q, 108);
    printf("    is_full=%s, is_fake_full=%s（真满时 fake_full 必须是 false，两者要能区分）\n",
           aqueue_is_full(&q) ? "true" : "false",
           aqueue_is_fake_full(&q) ? "true" : "false");

    printf("\n  现在把 6 个人全部放走，队列彻底空了（size=0）：\n");
    while (!aqueue_is_empty(&q)) {
        dequeue_and_show(&q);
    }

    printf("\n  ★ 假满的终极形态：队列里【一个人也没有】，却还是塞不进东西 ——\n");
    enqueue_and_show(&q, 1);
    printf("    front=%d、rear=%d 一起躺在 capacity=%d 上，两个指针都「退休」了：\n",
           q.front, q.rear, q.capacity);
    printf("    这就是朴素数组队列最离谱的地方 —— 不是容量不够，是下标不会绕回来。\n");
    aqueue_stats(&q);

    printf("\n  空队列上的读取操作也都是安全的（返回 false，不崩）：\n");
    int dummy = 0;
    if (!aqueue_peek_front(&q, &dummy)) {
        printf("    → peek_front 返回 false，dummy 保持原值 %d，没被写脏\n", dummy);
    }
    aqueue_peek_rear(&q, &dummy);

    printf("\n  另一种「满」：非法初始化（capacity=0）会怎样：\n");
    AQueue broken;
    aqueue_init(&broken, 0);
    aqueue_enqueue(&broken, 1);           /* 应该失败，且绝不解引用空指针 */
    aqueue_dequeue(&broken, NULL);

    /* ============================================================
       收尾：谁 malloc 谁 free
       ============================================================ */
    scene("收尾：每个 init 出来的队列都要 destroy，否则 ASan 会报内存泄漏");

    aqueue_destroy(&q);
    printf("  q      已 destroy：data=%p, front=%d, rear=%d（再误用会立刻崩，而不是悄悄算错）\n",
           (void *)q.data, q.front, q.rear);
    aqueue_destroy(&broken);
    printf("  broken 已 destroy（本来就没申请内存，free(NULL) 是合法空操作）\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住三句话：\n");
    printf("  1) 队列 = 数组 + front/rear 两个下标；元素住在 [front, rear) 里，size = rear - front。\n");
    printf("  2) 出队只 front++、绝不搬数据 → 左边的格子被永久作废 → rear 顶到墙就报「假满」。\n");
    printf("  3) 治法：搬数据（O(n)，笨）或让下标绕圈（取模，O(1)，下一节）。真实代码都用后者。\n");

    return 0;
}
