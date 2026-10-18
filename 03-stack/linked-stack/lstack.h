#ifndef LSTACK_H
#define LSTACK_H

/*
 * ===================================================================
 * lstack.h —— 链表栈的「菜单」
 * ===================================================================
 * 和数组栈（03-stack/array-stack）比，本节把「一摞盘子」换成「一摞自己 malloc 的节点」，
 * 每个节点记住「下一个节点的地址」。
 *
 * 只有一条规矩，但它是本节的全部重点：
 *   ★★ 栈顶必须放在链表【头部】，push/pop 都在头部做。★★
 * 因为单链表只有「从头往后走」的能力：
 *   头部插/删 = 改 2 个指针 = O(1)
 *   尾部插/删 = 得先从头走到尾找最后一个节点 = O(n)
 * 把栈顶放在尾部，就每次操作都要遍历一遍，栈瞬间退化成一个慢数组。
 *
 * 另一个必须讲清的 C 语言问题：
 *   在头部 push/pop 会【改掉 head 这个指针本身】（head 现在指向新的节点了）。
 *   C 语言传参永远是拷贝（见 00-c-foundation/pointer-basics），
 *   所以要么传「head 的地址」= 二级指针 LNode **，
 *   要么把 head 装进一个结构体里，传结构体的地址（本文件选这种，更清爽）。
 * 两种写法本节都实现了，放在一起对比：
 *   · 主流写法：LStack 结构体包 head —— 见 lstack_push / lstack_pop
 *   · 对比写法：LNode ** ——        见 lstack_push_double_pointer
 */

#include <stdbool.h>

/*
 * 链表节点：一小格内存，装「我的值」+「下一个节点的地址」。
 * 为什么 struct 里要写 struct LNode *next 而不能写 LNode *next？
 *   因为编译到这行时 LNode 这个名字还没定义完，不允许自我引用；
 *   带上 struct 关键字就是告诉编译器「你正在定义的那个类型」，这是 C 的固定写法。
 */
typedef struct LNode {
    int            val;    /* 盘子上写的数字 */
    struct LNode  *next;   /* 纸条：记下「下面那个盘子」在堆上的地址；NULL 表示到底了 */
} LNode;

/*
 * 用结构体把 head 包起来 —— 这就是本节的「清爽写法」。
 *
 * 为什么要包？
 *   head 是「栈顶节点的地址」，push/pop 都要换掉它。
 *   如果函数只收 LNode *head，那 head 只是拷贝，改完就丢，外面看到的还是老 head → 栈永远是空的。
 *   把 head 放进结构体，结构体本身是个稳定的「盒子」，调用者传盒子的地址（LStack *s），
 *   函数就能改盒子内部的 s->head。于是：
 *     · 不用再传二级指针，签名干净；
 *     · 顺便能装 size，判空 O(1)，不用遍历数节点。
 */
typedef struct {
    LNode *head;   /* 栈顶节点；NULL 就是空栈（栈里没有元素时 head 不指向任何东西）*/
    int    size;   /* 当前元素个数，push +1 / pop -1，专门用来避免「判空得遍历」*/
} LStack;

/* ---------- 生命周期 ---------- */

/* 初始化成一个空栈：head = NULL, size = 0。链表栈不预申请内存，所以不会失败 */
void lstack_init(LStack *s);

/*
 * 销毁：从头到尾一路 free 掉所有还没弹走的节点，再把 head 置 NULL。
 * ⚠ 顺序死规矩：必须先用 next 把「下一个地址」存好，再 free 当前节点。
 *    反过来写就是 free 之后读 node->next —— 悬空指针，ASan 立刻报警。
 */
void lstack_destroy(LStack *s);

/* ---------- 核心操作（全部在链表头部，全是严格 O(1)）---------- */

/* 压栈：新建节点，让它指向老 head，再把 head 改成新节点。成功 true，malloc 失败 false */
bool lstack_push(LStack *s, int value);

/* 弹栈：把 head 往后挪一格，free 掉摘下来的节点，值通过 out_value 带回（可传 NULL） */
bool lstack_pop(LStack *s, int *out_value);

/* 看栈顶但不删：只读 s->head->val */
bool lstack_peek(const LStack *s, int *out_value);

/* ---------- 查询 ---------- */

bool lstack_is_empty(const LStack *s);
int  lstack_size(const LStack *s);

/* ---------- 教学辅助：把「栈里全部元素」和「对比写法」打印出来 ---------- */

/* 打印整条链：head → [30] → [20] → [10] → NULL，左边是栈顶，右边是栈底 */
void lstack_print(const LStack *s, const char *label);

/*
 * 造一个孤立节点（next = NULL），malloc 失败返回 NULL。
 * 正式操作里 push 内部自己会造节点；这个函数是给 main.c 演示「失败写法」用的，
 * 让调用者拿着地址、从而能负责 free —— 见下面 lstack_push_broken 的说明。
 */
LNode *lstack_make_node(int value);

/*
 * 反例（一）：只传 LNode *head（一级指针），试图把新节点接成栈顶 —— 会失败，
 * 因为传进去的是拷贝，函数里改的 head 出了函数就销毁，外面的栈纹丝不动。
 *
 * ⚠ 这里故意让【调用者】去 malloc 新节点、并负责最后 free：
 *    真实 bug 写法是把 malloc 写在函数内部，那样这个节点除了函数里那份拷贝以外
 *    没人记得它，就变成一块彻底的内存泄漏。本仓库要求 ASan 0 leak，
 *    所以用「调用者给节点」的方式演示同一个指针问题，把泄漏留给读者体会。
 */
void lstack_push_broken(LNode *head, LNode *new_node);

/*
 * 另一种正统写法（不包结构体时用）：传 head 的地址，也就是二级指针。
 * 返回新的 head，同时把新建节点的数目通过 *size 维护。
 */
LNode *lstack_push_double_pointer(LNode **head, int value, int *size);

/*
 * 反例（二）：坚持「在链表尾部压栈」。
 * 为了能数清到底走了几步，函数返回「访问过多少个节点」。
 * 你会发现第 k 次 push 要走 k 步 —— 这就是 O(n) 的现场证据。
 */
int lstack_push_at_tail_count_steps(LStack *s, int value);

#endif /* LSTACK_H */
