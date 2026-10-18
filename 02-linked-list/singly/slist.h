#ifndef SLIST_H
#define SLIST_H

/*
 * ===================================================================
 * 单链表 slist —— 这一节的"菜单"
 * ===================================================================
 * 这个文件里【只有声明，没有实现】。它的作用就是让你一眼看到：
 *   "单链表一共能幹哪几件事，每件事的输入输出长什么样"。
 * 真正的代码在 slist.c 里，想看细节去那边。
 *
 * -------------------------------------------------------------------
 * 先记住节点长什么样：一格数据 + 一张写着"下一个节点地址"的纸条
 * -------------------------------------------------------------------
 *     ┌─────────┬───────┐      ┌─────────┬───────┐      ┌─────────┬───────┐
 *     │  val 1  │ next ─┼─────►│  val 2  │ next ─┼─────►│  val 3  │ next ─┼────► NULL
 *     └─────────┴───────┘      └─────────┴───────┘      └─────────┴───────┘
 *
 * head（头指针）不属于任何一个节点，它只是"我攥着第一个节点的门牌号"。
 * 只要 head 还在，整条链就能顺着 next 一路找到底；head 一丢，全链失联（内存泄漏）。
 *
 * -------------------------------------------------------------------
 * 为什么下面一半函数都要写成 Node **head（二级指针）？
 * -------------------------------------------------------------------
 * 00-c-foundation/pointer-basics 里那条铁律在这里天天用到：
 *   「C 语言传参永远是拷贝，想改到外面就得传地址」
 *
 * swap_by_value(int a, int b)  改不到外面的 x、y；
 * swap_by_pointer(int *a, int *b) 才能改到。
 *
 * 同理：
 *   Node *head        ← 这是"第一个节点的地址"这个变量本身
 *   Node **head_param ← 函数收到的是 head 的【地址】，*head_param 就是 head 本身
 *
 * 头插、删头节点这类操作，要改的正是 head 这个变量的值（换成新节点的地址）。
 * 如果只传 Node *head，函数里 head = 新节点 只是改了自己的拷贝，出来一看还是老样子
 * —— 我们在下面故意留了 slist_push_front_bad() 这个失败标本，跑一遍就死记住了。
 *
 * 只需要"读"整条链的函数（求长度、查找、打印、反转内部遍历），
 * 传 Node *head 就够了，因为我不打算改 head 本身。
 */

/* 节点定义：一个 int + 一个指向同类型节点的指针。
 * 注意 struct 里写 struct Node *next 而不是 Node *next —— 因为这时 typedef 还没完成，
 * 名字 Node 还不存在，只能带上 struct 关键字。 */
typedef struct Node {
    int val;
    struct Node *next;
} Node;

/* ---------- 1. 建节点 / 插节点 ---------- */
Node *slist_make_node(int val);                       /* 在堆上租一个节点，next 先置 NULL */
int   slist_push_front(Node **head, int val);         /* 头插：O(1)。返回 1 成功 0 内存不够 */
void  slist_push_front_bad(Node *head, int val);      /* ⚠ 故意写错的版本（只传一层指针），演示"改不到外面" */
int   slist_push_back(Node **head, int val);          /* 尾插：要先走到尾巴，O(n) */
int   slist_insert_at(Node **head, int k, int val);   /* 插到第 k 个位置（下标从 0 起），O(n) */

/* ---------- 2. 找东西 ---------- */
Node *slist_find(Node *head, int val);                /* 按值找节点，返回地址；找不到返回 NULL */
int   slist_get_at(Node *head, int k, int *val_out);  /* 按下标取值（顺便让你体会链表随机访问有多慢） */

/* ---------- 3. 删节点（本节最大考点） ---------- */
int   slist_remove_val(Node **head, int val);         /* 按值删第一个匹配节点：哨兵 dummy 统一写法 */
int   slist_remove_val_two_cases(Node **head, int val); /* 同一件事的"土办法"：特判删头 + 循环删中间 */

/* ---------- 4. 整条链的操作 ---------- */
int   slist_length(Node *head);                       /* 数节点个数，O(n) */
void  slist_reverse_iter(Node **head);                /* 反转 · 迭代版（面试必会） */
void  slist_reverse_recursive(Node **head);           /* 反转 · 递归版 */
Node *slist_middle(Node *head);                       /* 快慢指针找中间节点 */

/* ---------- 5. 看 / 收摊 ---------- */
void  slist_print(Node *head);                        /* 打成 1 -> 2 -> 3 -> NULL */
void  slist_free(Node **head);                        /* 释放全部节点，并把头指针置 NULL */

#endif /* SLIST_H */
