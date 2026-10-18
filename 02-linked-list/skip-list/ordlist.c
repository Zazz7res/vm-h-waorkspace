#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "ordlist.h"

/*
 * ===================================================================
 * 有序单链表 —— 跳表的"陪练"
 * ===================================================================
 * 这个文件存在的唯一理由：把"一格一格走有多贵"真的量出来。
 * 代码本身没有任何新东西（就是 02-linked-list/singly 那套），
 * 所以要读的注释主要在"比较次数是在哪一行累加的" —— 它必须和跳表同口径，
 * 否则两边的数字没法放在一起比。
 */

void ol_init(OrdList *l)
{
    if (l == NULL) {
        return;
    }

    /* 哨兵头：key 用 INT_MIN，于是"第一个 >= key 的节点"永远有一个稳定的起点。
     * 有了它，"插在链表最前面"和"插在中间"就是同一行代码 —— 单链表哨兵的老招数。*/
    l->head = malloc(sizeof(ONode));
    if (l->head == NULL) {
        fprintf(stderr, "ol_init: 内存不足\n");   /* 正常运行走不到这里，stderr 依然是空的 */
        l->tail = NULL;
        l->size = 0;
        l->cmp_count = 0;
        return;
    }
    l->head->key  = INT_MIN;
    l->head->next = NULL;
    l->tail       = l->head;                      /* 空表时"尾巴"就是哨兵自己，省掉一堆特判 */
    l->size       = 0;
    l->cmp_count  = 0;
}

/*
 * 释放：先记下 next 再 free（和单链表同一条铁律：销毁一个格子之前先把它的值抄出来）。
 * 哨兵也是 malloc 来的，同样要还 —— 很多人只 free 数据节点，把哨兵漏了。
 */
void ol_destroy(OrdList *l)
{
    if (l == NULL || l->head == NULL) {
        return;
    }

    ONode *cur = l->head;
    while (cur != NULL) {
        ONode *nxt = cur->next;                   /* ① 先抄住下一个 */
        free(cur);                                /* ② 再还掉当前 */
        cur = nxt;                                /* ③ 走到抄下来的地址 */
    }
    l->head = NULL;                               /* 置空，杜绝悬空指针 */
    l->tail = NULL;
    l->size = 0;
}

bool ol_insert_sorted(OrdList *l, int key)
{
    if (l == NULL || l->head == NULL) {
        return false;
    }

    ONode *prev = l->head;
    while (prev->next != NULL) {
        l->cmp_count++;                           /* ★ 同口径：每"看一眼某节点的 key"记 1 次 */
        if (prev->next->key >= key) {
            break;                                /* 找到第一个 >= key 的节点，插在它前面 */
        }
        prev = prev->next;
    }
    l->cmp_count++;                               /* 收尾那次"撞见 NULL / 判定位置"也算一次 */

    if (prev->next != NULL && prev->next->key == key) {
        return false;                             /* 已存在，不插重复值（和跳表口径一致）*/
    }

    ONode *n = malloc(sizeof(ONode));
    if (n == NULL) {
        return false;
    }
    n->key  = key;
    n->next = prev->next;                         /* ① 先接右边 */
    prev->next = n;                               /* ② 再接左边；顺序反了就断链 */
    l->size++;

    if (n->next == NULL) {
        l->tail = n;                              /* 插在了最后 → 尾巴把手要跟着挪，
                                                     否则后面 ol_append 会接错地方 */
    }
    return true;
}

bool ol_append(OrdList *l, int key)
{
    if (l == NULL || l->head == NULL) {
        return false;
    }

    /* 前提检查：必须真的比尾巴大，才能 O(1) 往后挂。
     * 违反前提时报错并原样返回 —— 悄悄插出错位的东西比报错难查一百倍。*/
    if (l->tail != NULL && l->tail->key >= key) {
        fprintf(stderr, "ol_append: key=%d 不比当前尾巴 %d 大，请用 ol_insert_sorted\n",
                key, l->tail->key);               /* 正常运行走不到这里 */
        return false;
    }

    ONode *n = malloc(sizeof(ONode));
    if (n == NULL) {
        return false;
    }
    n->key  = key;
    n->next = NULL;
    l->tail->next = n;                            /* 有 tail 把手 → 一句话挂到尾巴后面，O(1) */
    l->tail = n;
    l->size++;
    return true;
}

/*
 * 有序查找 + 提前刹车。
 *
 *   head(INT_MIN) ──► 1 ──► 3 ──► 5 ──► 7 ──► 9 ──► 11 ──► 13 ──► NULL
 *   找 13：从哨兵开始，一路问"下一个是不是 >= 13"，问 7 次才到。
 *
 * ⚠ 这就是"数据有序"能给链表的**全部**帮助：能提前停，但不能跳。
 *   平均仍然要走 n/2 步。想跳，就得上多层索引 —— 也就是本节的主角。
 */
bool ol_search(OrdList *l, int key)
{
    if (l == NULL || l->head == NULL) {
        return false;
    }

    ONode *x = l->head;
    for (;;) {
        if (x->next == NULL) {
            l->cmp_count++;                       /* 撞到尽头也要记一次"看一眼" */
            break;
        }
        l->cmp_count++;                           /* ★ 和跳表同口径的那一行 */
        if (x->next->key >= key) {
            break;                                /* 下一个已经不小于目标：停，别再走了 */
        }
        x = x->next;
    }

    bool hit = (x->next != NULL && x->next->key == key);
    if (hit) {
        l->cmp_count++;                           /* 最后确认"就是这个"的一次比较 */
    }
    return hit;
}

void ol_print(const OrdList *l)
{
    if (l == NULL || l->head == NULL) {
        printf("(没有内存)\n");
        return;
    }

    ONode *cur = l->head->next;                   /* 哨兵不算数据，从它的下一个开始打 */
    while (cur != NULL) {
        printf("%d -> ", cur->key);
        cur = cur->next;
    }
    printf("NULL\n");
}
