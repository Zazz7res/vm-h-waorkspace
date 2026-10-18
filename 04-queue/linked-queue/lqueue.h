#ifndef LQUEUE_H
#define LQUEUE_H

/*
 * ===================================================================
 * lqueue.h —— 链表队列的「菜单」
 * ===================================================================
 * 队列 = 排队，先进先出（FIFO）。上一节用「数组 + 取模」实现，
 * 本节换成「一堆自己 malloc 的节点用 next 串起来」，于是：
 *   · 永远不会满（只要堆还有内存就能一直入队），不需要预估容量；
 *   · 每一次入队/出队都是严格 O(1)，不搬数据、不取模；
 *   · 代价：每个元素多背一个 8 字节指针 + 一次 malloc/free，cache 不友好。
 *
 * ★★ 本节最要紧的一条结论：链表当队列，必须【头部出队、尾部入队】。★★
 * 为什么？单链表只有一个方向的能力：从前往后走（顺着 next）。
 *   · 头出：摘掉第一个节点，只改 1 个指针 → O(1) ✓
 *   · 尾进：tail 就指在最后一个节点上，接一个新节点、把 tail 换过去 → O(1) ✓
 * 反过来（头进尾出）会怎样？
 *   · 头进：还是 O(1)（头插）；
 *   · 尾出：要把最后一个节点摘掉，可是单链表【没有 prev 指针】，
 *           必须从头一路走到倒数第二个节点去找它的"前驱" → O(n) ✗
 *   于是队列退化成"每次出队都遍历一遍"，FIFO 的意义全丢。
 * 本节把这个反例也实现了（lqueue_pop_from_tail_count_steps），
 * demo 里会数清到底走了几步，让 O(n) 变成看得见的数字。
 *
 * 另一个必须讲清的 C 语言问题：
 *   enqueue 会改 tail 这个指针本身，dequeue 会改 head 这个指针本身（甚至要改成 NULL）。
 *   C 传参永远是拷贝（见 00-c-foundation/pointer-basics），所以：
 *     写法一（本节主用）：把 head/tail/size 装进一个 LQueue 结构体，函数收 LQueue *
 *                        —— 盒子本身地址稳定，改盒子里的字段就行，签名干净。
 *     写法二（对照演示）：传两个二级指针 LQNode **head, LQNode **tail
 *                        —— 能跑，但签名长、调用处全是 &，容易写错。
 *
 * 还有本节最大的坑，代码里会专门防它：
 *   ⚠ 出队把【最后一个】节点摘掉时，tail 还指着那块已经 free 掉的内存！
 *     必须在 free 之前判断「我摘的是不是最后一个」，是的话把 tail 一起置 NULL，
 *     否则下一次 enqueue 会往悬空指针里写数据 —— ASan 立刻报 heap-use-after-free。
 */

#include <stdbool.h>

/*
 * 队列节点：我的值 + 下一个节点的地址（单向，只能往后看）。
 * 为什么写 struct LQNode *next 而不是 LQNode *next？
 *   编译到这一行时 LQNode 这个名字还没定义完，不允许自我引用；
 *   带上 struct 关键字就是告诉编译器"你正在定义的那个类型"，C 的固定写法。
 */
typedef struct LQNode {
    int             val;   /* 队伍里这个人手上举的号码牌 */
    struct LQNode  *next;  /* 纸条：记下"后面那个人"在堆上的地址；NULL = 队尾到底了 */
} LQNode;

/*
 * 把两个指针装进结构体 —— 本节的"清爽写法"。
 * head 出、tail 进，中间顺着 next 串成一条队。
 */
typedef struct {
    LQNode *head;   /* 队头节点（下一个要出队的人）；NULL = 空队列 */
    LQNode *tail;   /* 队尾节点（最新进来的那个人）；NULL = 空队列 */
    int     size;   /* 当前人数：判空/取大小 O(1)，不用一路 next 数过去 */
} LQueue;

/* ---------- 生命周期 ---------- */

/* 初始化成空队列：head = tail = NULL, size = 0。链表队列不预申请内存，所以不会失败 */
void lqueue_init(LQueue *q);

/*
 * 销毁：从 head 一路 free 到尾巴，再把两个指针置 NULL、size 归 0。
 * ⚠ 死规矩：必须先用 next 把"下一个地址"存到局部变量，再 free 当前节点；
 *    反过来写就是 free 之后又读 node->next —— 悬空指针（ASan 会报警）。
 */
void lqueue_destroy(LQueue *q);

/* ---------- 核心操作（头出尾进，都是严格 O(1)）---------- */

/*
 * enqueue：入队，站到队尾。
 * 三步：新建节点 → 让老 tail 的 next 指向它 → 把 tail 换成它。
 * 空队列（head==NULL）要特殊处理：head 和 tail 都指向这个新节点。
 * 成功 true；malloc 失败 false（并且不会把队列改坏）。
 */
bool lqueue_enqueue(LQueue *q, int value);

/*
 * dequeue：出队，让队头那个人走掉。
 * 三步：把 head 往后挪一格 → 记下要 free 的老节点 → free。
 * ⚠ 如果摘掉之后队列空了（size 变 0），必须顺手把 tail 也置 NULL！
 * 值通过 out_value 带回（可传 NULL 表示不要值）。空队列返回 false，且不写脏 out_value。
 */
bool lqueue_dequeue(LQueue *q, int *out_value);

/* 看队头 / 看队尾，但不删。空队列返回 false */
bool lqueue_peek_front(const LQueue *q, int *out_value);
bool lqueue_peek_rear(const LQueue *q, int *out_value);

/* ---------- 查询 ---------- */

bool lqueue_is_empty(const LQueue *q);   /* head == NULL（也可写 size == 0，两处必须一致）*/
int  lqueue_size(const LQueue *q);       /* 直接读 size 字段，O(1) */

/* ---------- 教学辅助 ---------- */

/* 打印整条队：head → [10] → [20] → [30] → NULL，左边是队头（先走的人）*/
void lqueue_print(const LQueue *q, const char *label);

/*
 * 造一个孤立节点（next = NULL），malloc 失败返回 NULL。
 * 正式操作里 enqueue 内部自己造；这个函数是给 demo 演示"写法二"和"失败写法"用的，
 * 让调用者拿着地址、从而能负责 free —— 见 lqueue_enqueue_broken 的说明。
 */
LQNode *lqueue_make_node(int value);

/*
 * 反例：坚持【尾部出队】（把队列倒过来用：头进尾出）。
 * 为了让你看清代价，函数返回"这一趟访问了多少个节点"。
 * 队伍里有 n 个人时，摘尾巴要走 n-1 步（找到倒数第二个）—— 人数翻倍，步数也翻倍，
 * 这就是 O(n) 的现场证据。demo 里连着摘 5 次，步数是 4、3、2、1、1。
 * 注意它仍然是正确出队（值确实拿到了、链也断干净了），只是慢。
 */
int lqueue_pop_from_tail_count_steps(LQueue *q, int *out_value);

/*
 * 写法二（不包结构体时的正统写法）：把 head 和 tail 的【地址】传进来。
 * 因为 enqueue 要改 tail、dequeue 要改 head，两个都得是二级指针。
 * demo 里用它建一条独立的队，跟写法一并排对比，用完照样 free。
 */
bool lqueue_enqueue_double(LQNode **head, LQNode **tail, int value, int *size);

/*
 * 失败写法（一级指针）：函数里只收 LQNode *tail，试图把它改成新节点。
 * 出了函数外面纹丝不动 —— 演示"C 传指针传的也是拷贝"。
 * ⚠ 这里故意让【调用者】去 malloc 新节点并负责 free：
 *    真实 bug 写法是把 malloc 写在函数内部，那样这个节点除了函数里那份拷贝以外
 *    没人记得它，就变成彻底的内存泄漏。本仓库要求 ASan 0 leak，
 *    所以用"调用者给节点"的方式演示同一个指针问题。
 */
void lqueue_enqueue_broken(LQNode *tail, LQNode *new_node);

#endif /* LQUEUE_H */
