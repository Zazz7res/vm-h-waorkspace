#ifndef CDLIST_H
#define CDLIST_H

/*
 * ===================================================================
 * 循环双向链表 cdlist —— 这一节的"菜单"
 * ===================================================================
 * 把上一章的两件事同时做掉：
 *   ① 首尾相接（循环）—— 尾巴牵回头，环里没有 NULL
 *   ② 每个节点多一根 prev（双向）—— 节点知道上一个是谁
 * 再外加一件：
 *   ③ 入口用一个【哨兵节点】(sentinel)：它不存数据，永远站在那儿当"第 0 个节点"
 *
 *   普通双向链表（02-linked-list/doubly）：
 *     NULL ◄─[1|●] ⇄ [2|●] ⇄ [3|●]─► NULL      两头各有一个 NULL 要特判
 *
 *   循环双向链表 + 哨兵（本节）：
 *
 *            head（哨兵，不存数据）
 *              │
 *              ▼
 *     ┌───────────────────────────────────────────────┐
 *     │  [哨兵] ⇄ [1] ⇄ [2] ⇄ [3] ⇄ [哨兵]  ← 绕回来  │
 *     └───────────────────────────────────────────────┘
 *       head->next = 第一个元素，head->prev = 最后一个元素
 *       空表：head->next == head && head->prev == head（自己和自己成环）
 *
 * -------------------------------------------------------------------
 * 哨兵存在的【唯一】理由：把所有"是不是第一个 / 是不是最后一个"的特判消灭干净
 * -------------------------------------------------------------------
 * 不用哨兵，删除要写四支代码：普通节点 / 头节点（改 head）/ 尾节点（改 tail）/ 唯一节点（两个都改）。
 * 有了哨兵：
 *     头节点的前驱 = 哨兵        → 改哨兵的 next，哨兵不是"头"，不用改任何把手
 *     尾节点的后继 = 哨兵        → 改哨兵的 prev，同上
 *     第一个元素 = head->next    → 不用再判"head 是不是 NULL"
 *     插到最前面 = insert_after(head, v)      ← 头插变成"在哨兵后面插"
 *     插到最后面 = insert_after(head->prev, v) ← 尾插变成"在最后一个后面插"
 * 于是全节【只有一个地方真正在改线】（insert_after）和【只有一个地方在摘线】（erase），
 * 其余函数全是它们的一行皮。特判从四支变成零支 —— 少写代码就是少写 bug。
 *
 * -------------------------------------------------------------------
 * 本节最大的卖点（会实测出数字给你看）：删一个"只知道自己是哪个节点"的节点是 O(1)
 * -------------------------------------------------------------------
 *   单链表/循环单链表：要摘 X 必须改 X 的前驱的 next，而节点不记前驱
 *                     → 从 head 走一趟找前驱 = O(n)
 *   本节：            X->prev 就写在 X 自己身上 → 前驱零步到手 → 改 4 根线 = O(1)
 *
 *   什么时候手里"只有节点地址"？—— 哈希表里存着它（LRU 缓存就是这么活的：
 *   key → 节点地址，命中后要把这个节点挪到队头，绝不能为了找前驱再遍历一遍）。
 *   为了把这件事量化，本文件带一个【计步器】：cdlist_hops() 报告"这一串操作里
 *   沿着 next/prev 挪了几次指针"，并自带一份"装作没有 prev"的单链表式删除做对照组。
 *
 * -------------------------------------------------------------------
 * 传参还是那条铁律：要改调用者手里的东西，就传它的地址
 * -------------------------------------------------------------------
 * 函数参数一律 CDList *l（结构体地址）。写成 void push_back(CDList l, int v)
 * 就是改拷贝，外面毫无变化 —— 和 00-c-foundation/pointer-basics 里 swap_by_value 同错。
 * 只有"只是读一读"的函数（size / is_empty / 打印）用 const CDList *l，
 * 顺手拿到编译器的"我不会改你"保证。
 */

/* 节点：一个 int + 两根纸条。前向和后向各走各的，两两互指。 */
typedef struct Node {
    int val;
    struct Node *prev;   /* 上一个节点；【第一个元素的前驱是哨兵】，不是 NULL */
    struct Node *next;   /* 下一个节点；【最后一个元素的后继是哨兵】，不是 NULL */
} Node;

/* 一条链表的把手。⚠ 这里只剩 head 一个指针 + size，没有 tail ——
 * 因为 head->prev 就是尾巴，尾巴白捡，再存一个 tail 反而多一份要对账的账。 */
typedef struct {
    Node *head;   /* 哨兵节点：不存数据，永远存在（这就是它和普通 head 的最大区别） */
    int   size;   /* 元素个数，不含哨兵 */
} CDList;

/* ---------- 1. 生命周期 ---------- */
int   cdlist_init(CDList *l);            /* malloc 一个哨兵，让它自己和自己成环 */
void  cdlist_free(CDList *l);            /* 摘光所有元素 + free 哨兵，把手复位 */

/* ---------- 2. 两头插：都是 insert_after 的一行皮，都 O(1) ----------
 * 返回新节点的地址（失败返回 NULL）。★ 为什么返回地址：拿着这个地址，
 * 以后就能"不给 key、不查表、不遍历"地直接删它 —— 本节卖点的入场券。 */
Node *cdlist_push_front(CDList *l, int val);
Node *cdlist_push_back(CDList *l, int val);

/* ---------- 3. 指定节点旁边插：O(1) ---------- */
Node *cdlist_insert_after(CDList *l, Node *pos, int val);  /* 插在 pos 后面（pos 是哨兵 = 头插）*/
Node *cdlist_insert_before(CDList *l, Node *pos, int val); /* 插在 pos 前面（等价于 insert_after(pos->prev)）*/

/* ---------- 4. 删：本节最大卖点 ---------- */
int   cdlist_erase(CDList *l, Node *node);        /* ★ 删【已知地址】的节点：0 步遍历，改 4 根线 */
int   cdlist_pop_front(CDList *l, int *val_out);  /* 弹头：erase(head->next) 的皮 */
int   cdlist_pop_back(CDList *l, int *val_out);   /* 弹尾：erase(head->prev) 的皮（单链表做不到 O(1)）*/
int   cdlist_remove_val(CDList *l, int val);      /* 按值删第一个匹配：找 O(n) + 摘 O(1) */

/* ---------- 5. 查询 / 遍历 ---------- */
Node *cdlist_find(CDList *l, int val);            /* 从头往后找（计步器会告诉你走了几步） */
Node *cdlist_find_from_back(CDList *l, int val);  /* 从尾往前找：目标在后半段时更划算 */
int   cdlist_size(const CDList *l);
int   cdlist_is_empty(const CDList *l);           /* head->next == head，不用判 NULL */

/* ---------- 6. 打印：正反各一遍，prev 链接对不对一眼看穿 ---------- */
void  cdlist_print_forward(const CDList *l);      /* [哨兵] <=> 1 <=> 2 <=> 3 <=> (回哨兵) */
void  cdlist_print_backward(const CDList *l);     /* [哨兵] <=> 3 <=> 2 <=> 1 <=> (回哨兵) */
void  cdlist_print_with_cursor(const CDList *l, const Node *cursor);  /* 正向打印 + 标出游标（浏览器历史那幕用） */

/* ---------- 7. 计步器：把"复杂度"从口号变成能打印的数字 ----------
 * 全局计数器（教学用的妥协：只在本节这一个编译单元里，别在多线程/多链表场合这么写）。
 * 只统计"沿着 next/prev 挪指针"的次数，也就是真正决定 O(n) 的那个动作。 */
long  cdlist_hops(void);                          /* 目前累计挪了多少次指针 */
void  cdlist_reset_hops(void);                    /* 归零，下一组测量从头算 */

/* ---------- 8. 对照组：装作没有 prev 的删法（就是单链表的算法） ----------
 * 只用 next 那半条链，绕圈找 victim 的前驱，然后改线。功能上和 cdlist_erase 完全等价，
 * 区别只有步数：这个要走 k 步，erase 走 0 步。main.c 第 4 幕把两个数字并排打出来。 */
int   cdlist_erase_singly_style(CDList *l, Node *victim);

#endif /* CDLIST_H */
