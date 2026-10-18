#include <stdio.h>
#include <stdlib.h>   /* malloc / free */
#include "lqueue.h"

/*
 * ===================================================================
 * lqueue.c —— 链表队列的实现：head 出队、tail 入队
 * ===================================================================
 * 先看一眼内存长什么样。队列里有 10、20、30 三个人（10 最早来，30 最晚来）：
 *
 *        head                          tail
 *          │                              │
 *          ▼                              ▼
 *        ┌───────┐      ┌───────┐      ┌───────┐
 *   q → │  10   │ ───→ │  20   │ ───→ │  30   │ ───→ NULL
 *        └───────┘      └───────┘      └───────┘
 *        ▲ 出队从这里走                入队从这里加 ▲
 *        （摘头节点，改 1 个指针 = O(1)）（接新节点，改 2 个指针 = O(1)）
 *        每个方框就是一次 malloc 出来的 LQNode（val + next 两个字段）
 *
 *   三个节点是【三次独立的 malloc】，地址在堆上东一块西一块，
 *   靠 next 串起来才是"一条队" —— 跟数组队列那种"一整块连续内存"完全不同：
 *     · 好处：永远不用预估容量、永远不会满、不需要取模、每次操作严格 O(1) 且不搬数据。
 *     · 坏处：每个节点多背一个 8 字节指针 + malloc 管理头（实际占用常常 32 字节起），
 *             而且节点散落各处，一路 next 跳来跳去，CPU cache 命中率差。
 *
 *   入队 enqueue(40)：
 *       tail->next = 新节点;  tail = 新节点;      ← 只改 2 个指针，O(1)
 *       ┌───────┐      ┌───────┐      ┌───────┐      ┌───────┐
 *       │ 10    │ ───→ │ 20    │ ───→ │ 30    │ ───→ │ 40    │ → NULL
 *       └───────┘      └───────┘      └───────┘      └───────┘
 *          ▲head                                      ▲tail（换了）
 *
 *   出队 dequeue()：
 *       老 = head;  head = head->next;  free(老);   ← 只改 1 个指针，O(1)
 *       ┌───────┐      ┌───────┐      ┌───────┐
 *       │ 20    │ ───→ │ 30    │ ───→ │ 40    │ → NULL
 *       └───────┘      └───────┘      └───────┘
 *          ▲head（换了）                ▲tail
 *
 *   ⚠⚠ 本文件最重要的三行（出队摘掉最后一个节点时）：
 *       如果队列里只剩 1 个人，摘掉之后 head 变成 NULL，而 tail 还指着【刚 free 的那块】！
 *       必须补一句 `q->tail = NULL;`，否则下一次 enqueue 会去读/写已释放内存
 *       （heap-use-after-free，ASan 一跑就炸）。这也是"结构体包两个指针"的价值所在：
 *       两个指针在同一处维护，才容易一起改对。
 *
 *   为什么不能反过来（头进尾出）？
 *       尾出要摘最后一个节点，可是摘它得先知道它的【前驱】（倒数第二个节点），
 *       单链表没有 prev 指针 → 只能从头一路走到倒数第二 → O(n)。
 *       本节把这条反例实现成 lqueue_pop_from_tail_count_steps，并返回走过的节点数，
 *       demo 里连着摘 5 次，步数依次是 4、3、2、1、1 —— O(n) 变成看得见的数字。
 */

/*
 * -------------------------------------------------------------------
 * 初始化
 * -------------------------------------------------------------------
 * 什么都不用申请，三个字段归零就是合法的空队列。
 * 注意 head 和 tail 必须【同时】为 NULL：
 *   空队列的定义是 head == NULL；如果这时 tail 不是 NULL，就是脏状态，
 *   下一次 enqueue 会走"接到老尾巴上"的分支 → 往野指针里写数据。
 * -------------------------------------------------------------------
 */
void lqueue_init(LQueue *q)
{
    if (q == NULL) {
        return;
    }
    q->head = NULL;
    q->tail = NULL;     /* ⚠ 和 head 一起置 NULL，两者状态必须一致 */
    q->size = 0;
}

/*
 * -------------------------------------------------------------------
 * 销毁
 * -------------------------------------------------------------------
 * 链表队列可能有任意多个节点，每一个都是独立 malloc 的，必须全部 free。
 * 顺序死规矩：先记下 next，再 free 当前。
 *   node = q->head;
 *   while (node != NULL) { next = node->next; free(node); node = next; }
 * 写成 free(node) 之后再取 node->next 就是读已释放内存 —— 典型的悬空指针访问。
 * -------------------------------------------------------------------
 */
void lqueue_destroy(LQueue *q)
{
    if (q == NULL) {
        return;
    }

    LQNode *node = q->head;
    while (node != NULL) {
        LQNode *next = node->next;   /* ① 先把"下一个"抄下来（node 一 free 就读不到了）*/
        free(node);                  /* ② 再归还这一小块内存 */
        node = next;                 /* ③ 往前走 */
    }

    q->head = NULL;
    q->tail = NULL;                  /* 两个指针一起清干净，结构体回到"合法空队列" */
    q->size = 0;
}

/* 造一个孤立节点：值填好、next 置 NULL。malloc 失败返回 NULL（调用者负责 free） */
LQNode *lqueue_make_node(int value)
{
    LQNode *node = malloc(sizeof(LQNode));   /* 单位是字节：sizeof(整个节点)，别漏 next 那 8 字节 */
    if (node == NULL) {
        printf("      [错误] malloc 一个 LQNode 失败（堆内存不够）\n");
        return NULL;
    }
    node->val  = value;
    node->next = NULL;    /* ⚠ 新节点必须置 NULL：它是队尾，不然 next 是垃圾地址，
                              一路 next 打印会跑到野内存里去 */
    return node;
}

/*
 * -------------------------------------------------------------------
 * enqueue：入队（站到队尾）
 * -------------------------------------------------------------------
 * 一步步：
 *   ① malloc 新节点，失败就返回 false —— 队列一个字节都没被改动，还是完好状态。
 *   ② 分两种情况：
 *      · 空队列（head == NULL）：head 和 tail 都指向新节点。
 *        ⚠ 这一句不能省：漏了 head 的话，出队会以为队列是空的，节点永远 free 不掉。
 *      · 非空：tail->next = 新节点; 然后 tail = 新节点;
 *        顺序很关键：必须【先接上、再挪 tail】。反过来写，老 tail 就找不到新节点了。
 *   ③ size++。
 *
 * 为什么放尾巴是 O(1)？因为 tail 这个字段帮我们记住了最后一个节点在哪，
 *   省掉了"从头走到尾"的 n 步 —— 这就是队列比栈多维护一个指针的全部意义。
 * -------------------------------------------------------------------
 */
bool lqueue_enqueue(LQueue *q, int value)
{
    if (q == NULL) {
        printf("      [失败] enqueue(%d)：传进来的队列是空指针\n", value);
        return false;
    }

    LQNode *node = lqueue_make_node(value);
    if (node == NULL) {
        printf("      [失败] enqueue(%d)：造不出节点，队列保持原样（size=%d）\n",
               value, q->size);
        return false;
    }

    if (q->tail == NULL) {
        /* 空队列：第一个节点既是队头也是队尾 */
        q->head = node;
        q->tail = node;
        printf("      （空队列入队：head 和 tail 同时指向新节点 —— 漏掉 head 就会内存泄漏）\n");
    } else {
        q->tail->next = node;   /* ② 先让老尾巴牵上新人 */
        q->tail       = node;   /*   再把尾巴指针换过来（顺序反了新人就挂了）*/
    }

    q->size += 1;
    return true;
}

/*
 * -------------------------------------------------------------------
 * dequeue：出队（让队头那个人走掉）
 * -------------------------------------------------------------------
 * 一步步：
 *   ① 判空：head == NULL → false，绝不 free、绝不写脏 out_value。
 *   ② 把要摘的节点存进 old（因为 head 一挪就找不到它了，可它还得被 free）。
 *   ③ head = head->next —— 先读后改，这一步之后 old->next 仍是合法值，可以用它判断
 *      "我摘的是不是最后一个"。
 *   ④ ⚠★ 关键边界：如果 old->next == NULL，说明摘的是唯一一个节点，
 *      此时 head 已经是 NULL，而 tail 还指着 old —— 必须 tail = NULL，
 *      否则 tail 变成悬空指针，下一次 enqueue 直接往已释放内存里写。
 *   ⑤ free(old) —— 值先存到局部变量 v（free 之后 node->val 也不能读了）。
 *   ⑥ size--，需要就把 v 带回。
 *
 * 为什么出队放头部是 O(1)？摘头只需要改 head 一个指针，不需要前驱信息；
 *   摘尾需要前驱（单链表没有 prev），那就得遍历 —— 一进一出各挑最省事的那端。
 * -------------------------------------------------------------------
 */
bool lqueue_dequeue(LQueue *q, int *out_value)
{
    if (q == NULL) {
        printf("      [失败] dequeue：传进来的是空指针\n");
        return false;
    }

    if (q->head == NULL) {                /* 空队列 */
        printf("      [失败] dequeue：队列是空的（head==NULL, size=%d）\n", q->size);
        return false;
    }

    LQNode *old = q->head;                /* ② 先记住要扔掉的人 */
    int v = old->val;                     /*    值要在 free 之前取出来 */
    bool was_last = (old->next == NULL);  /* ④ 提前判断：他是不是队伍里最后一个？ */

    q->head = old->next;                  /* ③ 头指针往后挪一格 */
    if (was_last) {
        q->tail = NULL;                   /* ④ ★★ 本节最容易漏的一行 ★★
                                                队伍空了，tail 必须跟着归零，
                                                否则它就成悬空指针（use-after-free）*/
        printf("      ⚠ 摘掉的是【最后一个】节点：head 变 NULL，tail 也必须一起置 NULL！"
               "（漏了这行，下次 enqueue 就会往已释放的内存里写 → ASan 报 use-after-free）\n");
    }

    old->next = NULL;                     /* 小习惯：把摘下来的节点 next 抹掉再 free，
                                             万一别处还错误引用它，读到 NULL 比读到垃圾安全 */
    free(old);                            /* ⑤ 归还内存 —— 谁 malloc 谁 free */
    q->size -= 1;                         /* ⑥ 人数 -1 */

    if (out_value != NULL) {
        *out_value = v;
    }
    return true;
}

/*
 * -------------------------------------------------------------------
 * peek_front / peek_rear：只看，不让人走
 * -------------------------------------------------------------------
 * 队列的两个"只读口子"：队头 head->val（下一个要服务的），队尾 tail->val（最新来的）。
 * 注意 peek_rear 之所以 O(1)，全靠 tail 字段 —— 只有 head 的话就得一路走到尾。
 * 签名里的 const：承诺"只读不改"，误写编译器立刻报错。
 * -------------------------------------------------------------------
 */
bool lqueue_peek_front(const LQueue *q, int *out_value)
{
    if (q == NULL || q->head == NULL) {
        printf("      [失败] peek_front：队列是空的，没有队头可看\n");
        return false;
    }
    if (out_value != NULL) {
        *out_value = q->head->val;
    }
    return true;
}

bool lqueue_peek_rear(const LQueue *q, int *out_value)
{
    if (q == NULL || q->tail == NULL) {
        printf("      [失败] peek_rear：队列是空的，没有队尾可看\n");
        return false;
    }
    if (out_value != NULL) {
        *out_value = q->tail->val;
    }
    return true;
}

/* ---------- 查询：都是读字段，O(1) ---------- */

bool lqueue_is_empty(const LQueue *q)
{
    if (q == NULL) {
        return true;
    }
    return q->head == NULL;    /* 判空只看 head；size==0 应该和它等价，
                                  demo 里会顺手验证一次（不等价就说明代码有 bug）*/
}

int lqueue_size(const LQueue *q)
{
    return (q == NULL) ? 0 : q->size;
}

/*
 * -------------------------------------------------------------------
 * 教学打印：head → [10] → [20] → [30] → NULL
 * -------------------------------------------------------------------
 * 真正的队列不需要把整条队倒出来（抽象上只能碰两头），这是为了让你看见
 * "先来的排在左边、后来的排在右边"，以及 head/tail 各自站在哪一端。
 * -------------------------------------------------------------------
 */
void lqueue_print(const LQueue *q, const char *label)
{
    if (q == NULL) {
        printf("  (空指针，没有队列可打印)\n");
        return;
    }

    printf("  %s  size=%d, head=%s, tail=%s\n",
           label, q->size,
           q->head == NULL ? "NULL" : "→节点",
           q->tail == NULL ? "NULL" : "→节点");

    if (q->head == NULL) {
        printf("    head → NULL   （空队列，尾巴也是 NULL）\n");
        return;
    }

    printf("    head → ");
    int visited = 0;
    for (LQNode *node = q->head; node != NULL; node = node->next) {
        printf("[%d]", node->val);            /* 一个方框 = 一个堆上的节点 */
        if (node == q->tail) {
            printf(" ▲tail");                  /* 尾巴指针就挂在这个节点上 */
        }
        if (node->next != NULL) {
            printf(" → ");
        }
        visited++;
    }
    printf(" → NULL\n");

    /* 顺手验证 size 字段没跑偏：走一遍数出来的节点数应该等于 size。
       不等就说明某处 enqueue/dequeue 忘了 size++/size--（这是最常见的 bug 之一）。
       注意「走一遍」本身是 O(n) 的，正式代码不会每步都数 —— 所以才要 size 字段。 */
    if (visited != q->size) {
        printf("    ⚠ 警告：一路 next 数出来是 %d 个，但 size 字段写着 %d —— 两者必须一致！\n",
               visited, q->size);
    }
}

/*
 * -------------------------------------------------------------------
 * 反例：坚持【尾部出队】，看看代价有多大
 * -------------------------------------------------------------------
 * 逻辑：
 *   ① 从头开始走，一直走到【倒数第二个】节点（它的 next 就是尾巴）。
 *   ② 把它的 next 置 NULL（摘掉尾巴），tail 挪到它，free 掉老尾巴。
 *   ③ 返回"访问过多少个节点"，让调用者能亲眼看到步数随队伍长度增长。
 * 为什么必须这么麻烦？
 *   要摘掉节点 X，必须修改 X 的【前驱】的 next；单链表只能往后看，
 *   所以只能从头遍历找那个前驱 —— 这就是 O(n)。
 *   （双向链表有 prev 就能 O(1) 摘尾，但那是另一个结构：04-queue/deque 用循环数组，
 *     02-linked-list 里的双向链表才配两头 O(1)。）
 * -------------------------------------------------------------------
 */
int lqueue_pop_from_tail_count_steps(LQueue *q, int *out_value)
{
    if (q == NULL || q->head == NULL) {
        printf("      [失败] 尾部出队：队列是空的\n");
        return -1;
    }

    int steps = 0;                    /* 数清楚访问了多少个节点 */
    int v = q->tail->val;             /* 队尾元素的值（要返回给调用者）*/

    /* 只有一个节点的特殊情况：摘掉它之后 head 和 tail 都要归 NULL */
    if (q->head == q->tail) {
        steps = 1;
        free(q->head);
        q->head = NULL;
        q->tail = NULL;
        q->size -= 1;
        if (out_value != NULL) {
            *out_value = v;
        }
        printf("      （队列只有 1 个节点，访问 %d 个就够了）\n", steps);
        return steps;
    }

    /* 一般情况：走到倒数第二个节点，把最后一个摘掉 */
    LQNode *prev = q->head;
    steps = 1;                              /* head 也算访问过 */
    while (prev->next != q->tail) {
        prev = prev->next;
        steps++;                            /* ⚠ 每往前一步都记一笔，这就是"钱" */
    }
    LQNode *victim = prev->next;            /* 要摘掉的尾巴 */
    prev->next = NULL;                      /* 先断链 —— 顺序别反：free 之后就读不到 prev 了 */
    q->tail  = prev;                        /* 尾巴往前缩一格 */
    free(victim);
    q->size -= 1;

    if (out_value != NULL) {
        *out_value = v;
    }
    return steps;
}

/*
 * -------------------------------------------------------------------
 * 写法二：不包结构体，用二级指针
 * -------------------------------------------------------------------
 * enqueue 会改 tail 本身，所以 tail 要传"地址的地址"；
 * head 这里也要传，是因为空队列时第一个节点同时要赋给 head（不传就没法改）。
 * 于是签名变成 LQNode **, LQNode **, int, int * —— 能用，但一眼看去很累。
 * 这就是本仓库选择"结构体包指针"的原因：状态集中在一个盒子里，不容易改错。
 * -------------------------------------------------------------------
 */
bool lqueue_enqueue_double(LQNode **head, LQNode **tail, int value, int *size)
{
    if (head == NULL || tail == NULL || size == NULL) {
        printf("      [失败] 二级指针版 enqueue：参数里有 NULL\n");
        return false;
    }

    LQNode *node = lqueue_make_node(value);
    if (node == NULL) {
        return false;
    }

    if (*tail == NULL) {
        *head = node;          /* 注意每改一次都要写 *：这一层解引用就是"改调用者那个指针" */
        *tail = node;
    } else {
        (*tail)->next = node;  /* 括号不能省：*tail->next 会被解析成 *(tail->next)，完全错 */
        *tail = node;
    }
    *size += 1;
    return true;
}

/*
 * -------------------------------------------------------------------
 * 失败写法（只传一级指针）—— 专门给 demo 撞一次
 * -------------------------------------------------------------------
 * 传进来的 tail 是【拷贝】：函数里 `tail = new_node;` 改的是这份拷贝，
 * 出了函数就销毁，外面结构体里的 q->tail 纹丝不动 → 新节点没接上，队列看起来没变。
 * 这就是 00-c-foundation/pointer-basics 里 swap_by_value 的翻版，只不过对象换成了指针。
 *
 * 为了不把内存搞漏，这里连 new_node 都不 free —— 它由调用者 malloc、调用者 free。
 * （如果把 malloc 写在函数内部，那块内存就只有函数里那份拷贝认识，
 *   外面完全不知道它的存在 → 彻底泄漏。本仓库要求 ASan 0 leak，所以换个方式演示。）
 * -------------------------------------------------------------------
 */
void lqueue_enqueue_broken(LQNode *tail, LQNode *new_node)
{
    if (tail == NULL || new_node == NULL) {
        printf("      [跳过] 失败写法演示：参数里有 NULL\n");
        return;
    }
    tail->next = new_node;   /* 这一句【是有效的】：改的是别人的节点内部 */
    tail       = new_node;   /* ⚠ 这一句【毫无用处】：只改了形参这份拷贝 */
    printf("      （函数里 tail 现在 = %d 那个节点，地址=%p；"
           "但这只是拷贝，出了函数就作废）\n", new_node->val, (void *)tail);
}
