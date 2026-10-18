#ifndef DLIST_H
#define DLIST_H

/*
 * ===================================================================
 * 双向链表 dlist —— 这一节的"菜单"
 * ===================================================================
 * 和单链表唯一的结构差别：每个节点【多一根 prev 纸条】，指回上一个节点。
 * 就多了这一根线，删除一个"已经拿到手的节点"从 O(n) 变成了 O(1)。
 *
 *   单链表：  NULL ◄─ 不可能          [1|●] ──► [2|●] ──► [3|●] ──► NULL
 *                                          next     next     next
 *             节点只知道"下一个是谁"，想知道上一个必须从头再走一遍。
 *
 *   双向链表：NULL ◄─[1|●] ──► [2|●] ──► [3|●] ─► NULL
 *                  └─prev─►◄─prev─┘   （prev 和 next 方向相反，两两互指）
 *             每个节点既知道上一个，也知道下一个 → 两头都能走。
 *
 * 本节还把 head / tail / size 三个字段打包进一个 DList 结构体：
 *   · head：往左走的入口，头插 O(1)
 *   · tail：往右走的入口，尾插 O(1)   ← 单链表做不到，它每次尾插都要从头走到尾
 *   · size：长度直接记着，O(1) 回答"有多少个"
 *
 * 注意传参方式：函数参数是 DList *list（结构体的地址）。
 * 这和单链表里 Node **head 是【同一个道理】：
 *   要改调用者手里的东西（这里是 head/tail/size 三个字段），就得把地址传进来。
 *   假如写成 void push_back(DList list, int v)，函数改的就是拷贝，外面毫无变化 ——
 *   跟 00-c-foundation/pointer-basics 里 swap_by_value 的错一模一样。
 */

/* 节点：数据 + 两根纸条 */
typedef struct Node {
    int val;
    struct Node *prev;   /* 指向上一个节点；我是头时它是 NULL */
    struct Node *next;   /* 指向下一个节点；我是尾时它是 NULL */
} Node;

/* 一条链表的"把手"：三个字段全部要在增删时同步维护 */
typedef struct {
    Node *head;   /* 第一个节点；空表时 NULL */
    Node *tail;   /* 最后一个节点；空表时 NULL */
    int   size;   /* 节点个数；忘记维护就会给出错误长度 */
} DList;

/* ---------- 1. 生命周期 ---------- */
void dlist_init(DList *list);                 /* 造一条空表（把 head/tail 置 NULL，size 置 0） */
void dlist_free(DList *list);                 /* 释放全部节点，并把把手复位 */

/* ---------- 2. 两头插入：都是 O(1) ---------- */
int  dlist_push_front(DList *list, int val);  /* 头插：直接改 head，不用遍历 */
int  dlist_push_back(DList *list, int val);   /* 尾插：直接改 tail，不用遍历（单链表的痛在此被治好） */

/* ---------- 3. 在指定节点旁边插：O(1) ---------- */
int  dlist_insert_after(DList *list, Node *pos, int val);   /* 插在 pos 后面 */
int  dlist_insert_before(DList *list, Node *pos, int val);  /* 插在 pos 前面 */

/* ---------- 4. 删除：本节最大卖点 ---------- */
void dlist_remove(DList *list, Node *node);   /* ★ 删【已知地址】的节点：O(1)，不用找前驱 */
int  dlist_remove_by_val(DList *list, int val); /* 按值删第一个匹配：O(n)（找值那一步省不掉） */
int  dlist_pop_front(DList *list, int *val_out); /* 删头并把值带出来（做栈/队列要用） */
int  dlist_pop_back(DList *list, int *val_out);  /* 删尾并把值带出来 */

/* ---------- 5. 查找 / 遍历 ---------- */
Node *dlist_find(DList *list, int val);            /* 从头往后找 */
Node *dlist_find_from_tail(DList *list, int val);  /* 从尾往前找：证明 prev 真的能用 */
int   dlist_size(const DList *list);               /* O(1) 拿长度 */
void  dlist_print_forward(const DList *list);      /* NULL <= 1 <=> 2 <=> 3 => NULL */
void  dlist_print_backward(const DList *list);     /* 反向再走一遍，证明能倒着走 */

#endif /* DLIST_H */
