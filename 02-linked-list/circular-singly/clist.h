#ifndef CLIST_H
#define CLIST_H

/*
 * ===================================================================
 * 循环单链表 clist —— 这一节的"菜单"
 * ===================================================================
 * 它和 02-linked-list/singly 的【唯一】结构差别就一句话：
 *
 *     单链表：  最后一个节点的 next = NULL        （有个终点站）
 *     循环链表：最后一个节点的 next = 第一个节点   （尾巴牵回头，成一个圈）
 *
 *   单链表：  head ──► [1|●] ──► [2|●] ──► [3|∅]
 *   循环单链表：       [1|●] ──► [2|●] ──► [3|●]
 *                       ▲                    │
 *                       └────────────────────┘  ← ★ 就多了这根箭头
 *
 * 别小看这根箭头：整条链表里【再也没有 NULL 了】。
 * 于是单链表那套"看到 NULL 就停"的代码全部作废，
 * 终止条件必须换成"走回到起点就停"：
 *
 *     单链表：  for (Node *p = head; p != NULL;      p = p->next)   ← 这里 NULL 是终点
 *     循环表：  do { ... } while (p != head);                        ← 这里起点是终点
 *
 * 忘记改这一句 = 死循环（或者越走越远、最后解引用野指针当场段错误）。
 * 本节末尾用 --bug 开关做一次受控的现场演示（默认运行是干净的）。
 *
 * -------------------------------------------------------------------
 * 本节第二个关键点：只存 tail，不存 head
 * -------------------------------------------------------------------
 * 直觉上"头指针 head 才是命根子"，但在循环表里 tail 才是：
 *
 *     tail ──► [3|●] ──► [1|●]
 *                尾巴        ▲
 *                             │  tail->next 就是 head，一行都不用走！
 *
 *   · head  = tail->next        （O(1) 免费拿到，所以根本不用存）
 *   · 前驱  = 从 tail 出发顺着走 （tail 天然就是 head 的前驱）
 *   · 头插 O(1)：新节点插到 tail->next 的位置，tail 一动不动
 *   · 尾插 O(1)：新节点接到 tail 后面，把 tail 挪过去
 *
 * 反过来只存 head 的话：头插 O(1)，尾插要先走完整条链找尾巴 = O(n) —— 单链表就是这么痛的。
 * 所以本节结构体 CList 里【只有 tail 和 size 两个字段】，这是它存在的理由。
 *
 * -------------------------------------------------------------------
 * 传参方式（还是 pointer-basics 那条铁律）
 * -------------------------------------------------------------------
 * 本节把 tail/size 打包进 CList 结构体，函数参数一律写成 CList *l：
 *   要改调用者手里的东西（tail、size 两个字段），就得把它们的地址传进来。
 *   写成 void push_back(CList l, int v) 就是改拷贝，外面毫无变化 ——
 *   跟 00-c-foundation/pointer-basics 里 swap_by_value 一模一样的错。
 *   （单链表那节为什么用 Node **head？同一个道理，只是那里"手里的东西"是一个裸指针。）
 */

/* 节点：和单链表完全一样，一个 int + 一张 next 纸条。
 * 区别只在"最后一个节点的 next 里写的是谁" —— 那是链表层面的事，节点自己不知道。 */
typedef struct Node {
    int val;
    struct Node *next;
} Node;

/* 一条循环单链表的全部家当：两个字段。
 * ⚠ 没有 head 字段，head 是 tail->next 算出来的（见 clist_head）。 */
typedef struct {
    Node *tail;   /* 最后一个节点；空表时 NULL。tail->next 就是第一个节点 */
    int   size;   /* 节点个数：增删时同步维护，长度就是 O(1) 的账本 */
} CList;

/* ---------- 1. 生命周期 ---------- */
Node *clist_make_node(int val);            /* 堆上租一个节点，next 先置 NULL */
int   clist_init(CList *l);                /* 建空表：tail = NULL, size = 0。返回 1 成功 */
void  clist_free(CList *l);                /* 拆掉整个环并 free，tail 置 NULL、size 归零 */

/* ---------- 2. 两头插入：都 O(1)，这就是只存 tail 的红利 ---------- */
int   clist_push_front(CList *l, int val); /* 头插：改 tail->next，tail 本身不动 */
int   clist_push_back(CList *l, int val);  /* 尾插：接在 tail 后面，再把 tail 挪过去 */

/* ---------- 3. 读 ---------- */
Node *clist_head(const CList *l);          /* ★ head = tail->next；空表返回 NULL */
Node *clist_find(CList *l, int val);       /* 按值找节点，终止条件 p != head；找不到返回 NULL */
int   clist_is_empty(const CList *l);      /* tail == NULL，1 = 空 */
int   clist_size(const CList *l);          /* O(1)：直接回答账本 */
int   clist_count_by_walk(const CList *l); /* O(n)：现绕一圈数出来，用来给账本对账 */

/* ---------- 4. 删：按值删第一个匹配，O(n) ---------- */
int   clist_remove(CList *l, int val);     /* 返回 1 = 删掉了一个，0 = 没找到 */

/* ---------- 5. 打印 ---------- */
void  clist_print(const CList *l);                       /* 1 -> 2 -> 3 -> (回到 1) */
void  clist_walk_ring(const CList *l, int steps);        /* 从 tail 出发无脑走 steps 步，直观看到"绕回来" */

/* ---------- 6. 两个应用：循环结构的原生场景 ---------- */
/* 约瑟夫环：n 个人围一圈（编号 1..n），从 1 号开始报数，数到 m 的出圈。
 * 把出圈顺序写进 out[]（最多 out_cap 个），返回实际出圈人数。过程中每一轮都打印出来。 */
int   clist_josephus(int n, int m, int out[], int out_cap);

/* 轮转发牌（时间片轮转调度同款）：把 deck[] 里的 ndeck 张牌，按 1..nplayers 的顺序轮流发。
 * 演示"推进 = 走一根 next"，不需要 (i + 1) % nplayers 这个取模。 */
void  clist_deal(int nplayers, const int *deck, int ndeck);

/* ---------- 7. ⚠ 反面教材：只有 main.c 的 --bug 分支才会调用 ---------- */
/* 用单链表的 `p != NULL` 条件去数循环表的长度。cap 是"最多走这么多步就强行刹车"，
 * 否则真的会走到天荒地老。*hit_null_out 返回有没有碰到 NULL（永远是 0）。 */
int   clist_count_broken_null(const CList *l, int cap, int *hit_null_out);
/* 空表上少写一次判空：直接 tail->next->val。⚠ 调用它会当场段错误（--bug 里 fork 出去崩） */
int   clist_head_val_unguarded(const CList *l);

#endif /* CLIST_H */
