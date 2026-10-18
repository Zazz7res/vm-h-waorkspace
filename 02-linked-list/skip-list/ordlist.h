#ifndef ORDLIST_H
#define ORDLIST_H

/*
 * ===================================================================
 * ordlist.h —— 有序单链表（本节的"陪练"）
 * ===================================================================
 * 跳表最大的卖点是"比有序链表快"。要把这句话变成证据，光靠嘴说没用，
 * 得在同一份数据上跑一遍、把比较次数**数出来**对照。
 * 所以本节额外带一个极简的有序单链表，专门当这个测量基线。
 *
 * 它只有三件事：有序插入 / 顺序查找（记比较次数）/ 释放。
 * 没有泛型、没有函数指针、元素就是 int —— 和跳表那边保持同样的口径，
 * 这样两边"一次比较"的含义才完全一致，对比才公平。
 *
 * ★ 它的查找就是"一格一格走"的全部痛处：
 *     head(INT_MIN 哨兵) ──► [1] ──► [3] ──► [5] ──► ... ──► [13] ──► NULL
 *     找 13 要把前面 12 个节点的 key 全比一遍。
 *     数据有序带来的唯一好处是"能提前刹车"（看到比目标大的就停），
 *     但平均还是要走 n/2 步 —— 有序 ≠ 能跳。
 */

#include <stdbool.h>

/* 普通单链表节点：一个 key + 一张 next 纸条。和 02-linked-list/singly 同款，
 * 但这里是【独立的一份代码】：目录之间不许互相 include，所以各写各的。 */
typedef struct ONode {
    int             key;
    struct ONode   *next;
} ONode;

typedef struct {
    ONode *head;      /* 哨兵头，key = INT_MIN：于是"插在最前面"也不用特判 */
    ONode *tail;      /* 尾巴把手：只为把"按升序灌 2000 个数据"从 O(n²) 降到 O(n) */
    int    size;
    long   cmp_count; /* 和跳表完全同口径的比较次数 */
} OrdList;

/* 建空表（内部只有一个哨兵节点，一次 malloc）*/
void ol_init(OrdList *l);

/* 释放：哨兵也要 free，不然 ASan 一样报泄漏 */
void ol_destroy(OrdList *l);

/* 有序插入：从 head 往后走到"第一个 >= key 的位置"再插进去，O(n)。
 * 只在演示小规模数据时用；灌 2000 个数请用 ol_append（下面那个）。*/
bool ol_insert_sorted(OrdList *l, int key);

/* 升序追加：要求 key 比当前尾巴大（main.c 按 1..N 顺序灌数据，正好满足）。
 * 有 tail 把手就是 O(1)。违反前提时返回 false 并且不改动链表。*/
bool ol_append(OrdList *l, int key);

/* 有序查找 + 提前刹车：每"看一眼某节点的 key"就把 s->cmp_count 加 1。
 * 返回 true = 找到。*/
bool ol_search(OrdList *l, int key);

/* 把链表打成 1 -> 3 -> 5 -> NULL（哨兵不打印）*/
void ol_print(const OrdList *l);

#endif /* ORDLIST_H */
