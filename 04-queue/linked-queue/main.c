#include <stdio.h>
#include <stdlib.h>
#include "lqueue.h"

/*
 * main.c —— 链表队列的五幕演示
 *
 * 打印的都是「head → [10] → [20] → [30] → NULL」这种链式视图，
 * 你能直接看到 head 站在队头、tail 站在队尾，以及它们各自怎么挪。
 *
 * 编译运行（在项目根目录）：
 *   make one DIR=04-queue/linked-queue
 */

static void scene(const char *title)
{
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n");
}

static void enqueue_and_show(LQueue *q, int value)
{
    printf("\n  enqueue(%d)\n", value);
    if (lqueue_enqueue(q, value)) {
        lqueue_print(q, "现在队列：");
    } else {
        printf("    → 这次 enqueue 没成功，上面的 [失败] 就是原因\n");
    }
}

static void dequeue_and_show(LQueue *q)
{
    int value = 0;
    printf("\n  dequeue()\n");
    if (lqueue_dequeue(q, &value)) {
        printf("    出队的是 %d（队头，最早进来的那个）\n", value);
        lqueue_print(q, "现在队列：");
    } else {
        printf("    → 这次 dequeue 没成功（队列空），见上面的 [失败]\n");
    }
}

int main(void)
{
    /* ============================================================
       第 1 幕：FIFO + head/tail 各站在哪一端
       ============================================================ */
    scene("第 1 幕：head 出、tail 进 —— 先进先出（FIFO）");

    LQueue q;
    lqueue_init(&q);
    lqueue_print(&q, "刚初始化：");
    printf("    head 和 tail 【必须同时】是 NULL：只清一个就是脏状态，下次 enqueue 会写到野指针\n");

    enqueue_and_show(&q, 10);
    enqueue_and_show(&q, 20);
    enqueue_and_show(&q, 30);

    printf("\n  两头都能 O(1) 看一眼（不删）：\n");
    int head_v = 0, tail_v = 0;
    lqueue_peek_front(&q, &head_v);
    lqueue_peek_rear(&q, &tail_v);
    printf("    队头（下一个出队的）= %d，队尾（最新来的）= %d\n", head_v, tail_v);
    printf("    为什么 peek_rear 也是 O(1)？因为有 tail 字段直接告诉你最后一个节点在哪；\n");
    printf("    只存 head 的链表想知道队尾就得一路 next 走到底 —— 这正是队列要多一个指针的理由。\n");

    dequeue_and_show(&q);
    dequeue_and_show(&q);
    dequeue_and_show(&q);

    printf("\n  看到没：入队 10→20→30，出队 10→20→30，顺序和数组队列完全一样，\n");
    printf("  区别只是「一整块连续内存」换成了「三次 malloc 的节点用 next 串起来」。\n");
    printf("  顺便一提：lqueue_print 为了演示会走一遍 next 数节点，这本身是 O(n)；\n");
    printf("  正式代码判断大小只用 size 字段（O(1)），绝不遍历。\n");

    /* ============================================================
       第 2 幕：本节最大的坑 —— 摘掉最后一个节点时 tail 怎么办
       ============================================================ */
    scene("第 2 幕：⚠ 出队把【最后一个】节点摘掉，tail 必须跟着归 NULL");

    enqueue_and_show(&q, 100);
    enqueue_and_show(&q, 200);
    dequeue_and_show(&q);      /* 100 走掉，队里剩 200，head 和 tail 都指着 200 */

    printf("\n  现在 head 和 tail 指向【同一个】节点：\n");
    printf("    head=%p, tail=%p%s\n", (void *)q.head, (void *)q.tail,
           q.head == q.tail ? "  ← 一样，说明队里只剩 1 个节点" : "");
    printf("  这次出队之后，那个节点被 free 了。看 lqueue_dequeue 里那句关键判断：\n");

    dequeue_and_show(&q);      /* 200 走掉，同时 tail 被置 NULL（会打印警告说明）*/

    printf("\n  现在再 enqueue 一次，证明队列完全健康（tail 没变成悬空指针）：\n");
    enqueue_and_show(&q, 300);
    printf("    ★ 如果 dequeue 里漏掉 `q->tail = NULL;`，这一步就会去写\n");
    printf("      已经 free 的那块内存 —— 表现为「看起来塞进去了但打不出来」或者直接崩，\n");
    printf("      ASan 会报 heap-use-after-free。这是链表队列的头号事故。\n");

    /* ============================================================
       第 3 幕：反例 —— 为什么不能「头进尾出」
       ============================================================ */
    scene("第 3 幕：反例：坚持在【尾部】出队，代价是 O(n)（把步数数给你看）");

    LQueue r;
    lqueue_init(&r);
    printf("  先正常入队 5 个人（1、2、3、4、5）：\n");
    for (int i = 1; i <= 5; i++) {
        lqueue_enqueue(&r, i);
    }
    lqueue_print(&r, "入队完：");

    printf("\n  现在每次出队都从【尾巴】摘，函数返回「这一趟访问了几个节点」：\n");
    printf("  （摘尾巴必须修改它前驱的 next，而单链表只能从头往后找前驱）\n");
    for (int k = 1; k <= 5; k++) {
        int v = 0;
        int steps = lqueue_pop_from_tail_count_steps(&r, &v);
        printf("    第 %d 次尾部出队：拿走 %d，访问了 %d 个节点 → 队伍越长越慢 = O(n)\n",
               k, v, steps);
    }
    printf("\n  对比：本节正常做法（尾部入队、头部出队）每次只改 1~2 个指针，恒为 O(1)。\n");
    printf("  一句话记法：单链表只有「往前走」的能力，所以要摘掉的端必须有【现成的前驱】，\n");
    printf("              也就是【头】。尾巴要摘就得起码一路找过去。\n");
    printf("  （要两头都能 O(1) 得用双向链表，或者干脆用循环数组 —— 见 04-queue/deque。）\n");

    lqueue_destroy(&r);
    printf("\n  （这一幕的队列已经 destroy，剩下两个队列在收尾时释放）\n");

    /* ============================================================
       第 4 幕：写法二（二级指针）vs 失败写法（一级指针）
       ============================================================ */
    scene("第 4 幕：不包结构体行不行？二级指针可以，一级指针是坑");

    /* ---- 写法二：纯用两个裸指针 + 二级指针参数 ---- */
    LQNode *h = NULL;
    LQNode *t = NULL;
    int     n = 0;
    printf("  【写法二】LQNode **head, LQNode **tail —— 也能跑：\n");
    for (int i = 1; i <= 3; i++) {
        if (lqueue_enqueue_double(&h, &t, i * 10, &n)) {
            printf("    enqueue(%d) 成功：head=%p, tail=%p, size=%d\n",
                   i * 10, (void *)h, (void *)t, n);
        }
    }
    printf("    手工打一条队：head → ");
    for (LQNode *p = h; p != NULL; p = p->next) {
        printf("[%d] → ", p->val);
        if (p == t) {
            printf("▲tail ");
        }
    }
    printf("NULL\n");
    printf("    代价：签名变成一长串 &，每个字段都要写 * 解引用，\n");
    printf("          `(*tail)->next` 的括号漏了就变成 `*(tail->next)`，逻辑彻底错。\n");
    printf("          所以本仓库用【结构体包指针】：状态集中在一个盒子里，改不容易错。\n");

    /* 把写法二建的这条队手工 free 干净（它没装在 LQueue 里，destroy 帮不上忙）*/
    LQNode *p = h;
    while (p != NULL) {
        LQNode *next = p->next;
        free(p);
        p = next;
    }
    h = NULL;
    t = NULL;
    n = 0;
    printf("    已手工 free 掉写法二的 3 个节点（head/tail 置回 NULL，size=%d）\n", n);

    /* ---- 失败写法：一级指针 ---- */
    printf("\n  【失败写法】只传一级指针 LQNode *tail（现成的 q 里还剩 1 个节点 300）：\n");
    lqueue_print(&q, "    改动前：");
    LQNode *extra = lqueue_make_node(999);      /* 调用者 malloc，调用者负责让链能 free 到它 */
    printf("    调用 lqueue_enqueue_broken(q.tail, extra)：\n");
    lqueue_enqueue_broken(q.tail, extra);
    printf("    结果：tail->next = 新节点【生效了】（改的是别人节点里的字段），\n");
    printf("          但 `tail = new_node;` 只是改拷贝 → q.tail 还指着老节点：\n");
    lqueue_print(&q, "    改动后：");
    printf("          沿 next 其实已经能走到 999（上面打印里 size 和实际节点数也对不上），\n");
    printf("          但 tail 没换，所以队尾读出来还是老节点：\n");
    int rear_v = 0;
    lqueue_peek_rear(&q, &rear_v);
    printf("          peek_rear → %d（本该是 999）\n", rear_v);

    printf("\n    手工把状态补对（相当于结构体版帮你做的那两件事）：\n");
    q.tail = extra;      /* 尾巴指针换到新节点 */
    q.size += 1;         /* 计数补上 */
    lqueue_print(&q, "    修好之后：");
    printf("    ★ 教训：C 里传指针【传的也是拷贝】。想改调用者那个指针本身，\n");
    printf("      要么传它的地址（二级指针），要么把它装进结构体传结构体的地址。\n");

    /* ============================================================
       第 5 幕：边界情况
       ============================================================ */
    scene("第 5 幕：边界情况都安全（空队出队/peek、重复 destroy）");

    while (!lqueue_is_empty(&q)) {
        dequeue_and_show(&q);
    }
    lqueue_print(&q, "空队列：");

    dequeue_and_show(&q);
    int dummy = 0;
    if (!lqueue_peek_front(&q, &dummy)) {
        printf("\n    → peek_front 返回 false，dummy 仍是 %d，没被写脏\n", dummy);
    }
    lqueue_peek_rear(&q, &dummy);

    printf("\n  head==NULL 和 size==0 两种判据必须永远一致（不一致就是 bug）：\n");
    printf("    is_empty=%s, size=%d\n",
           lqueue_is_empty(&q) ? "true" : "false", lqueue_size(&q));

    printf("\n  空队列上再来一轮入队/出队（证明队列可以反复使用，没有残留脏指针）：\n");
    enqueue_and_show(&q, 7);
    dequeue_and_show(&q);

    /* ============================================================
       收尾：谁 malloc 谁 free
       ============================================================ */
    scene("收尾：lqueue_destroy 会把还留在队里的节点全部走一遍 free 掉");

    lqueue_destroy(&q);
    printf("  q  已 destroy：head=%p, tail=%p, size=%d\n",
           (void *)q.head, (void *)q.tail, q.size);
    lqueue_destroy(&q);        /* 故意重复调一次：destroy 里先判 head==NULL，free(NULL) 合法 */
    printf("  q  重复 destroy 也安全（ASan 不报 double free）\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住四句话：\n");
    printf("  1) 链表队列 = 每个节点独立 malloc + head/tail 两个指针；永不满、无需取模、操作严格 O(1)。\n");
    printf("  2) 必须【头部出队、尾部入队】：单链表只能往前走，摘尾要找前驱 = O(n)。\n");
    printf("  3) 出队摘掉最后一个节点时，tail 必须一起置 NULL —— 链表队列头号 bug。\n");
    printf("  4) 改 head/tail 本身要「结构体包指针」（推荐）或二级指针；传一级指针等于白改。\n");

    return 0;
}
