#include <stdio.h>
#include <stdlib.h>
#include "cdlist.h"

/*
 * ===================================================================
 * 计步器：把"复杂度"从嘴上的 O(1)/O(n) 变成能打印出来的数字
 * ===================================================================
 * 为什么要有这么个全局变量？因为这一节的核心卖点（删已知节点是 O(1)）
 * 光靠嘴说没有说服力，靠"看代码觉得好像是"也不算数 ——
 * 只有把"沿着 next/prev 挪指针的次数"数出来、和对照组并排打印，才算【实测】。
 *
 * 计数口径（很重要，两边必须同一把尺子）：
 *   · 只统计【循环里挪指针】的次数 —— 那才是决定 O(n) 的动作。
 *   · 读一下 node->prev、node->next 这种"固定一两步"不算步（否则 erase 就不是 0 了）。
 *   · 纯打印（print_*）不计步，免得把测量结果搅浑。
 *
 * ⚠ 全局状态是教学妥协：真实项目里一条链表一个计数器（放进 CDList 结构体），
 *   多线程下还得加锁。这里为了让 main.c 的对比代码只有两行，先糙一点，但把话说清楚。
 */
static long g_hops = 0;

long cdlist_hops(void)
{
    return g_hops;
}

void cdlist_reset_hops(void)
{
    g_hops = 0;
}

/*
 * ===================================================================
 * 1. cdlist_init：造一个"自己和自己成环"的哨兵
 * ===================================================================
 * 哨兵是一个【货真价实的节点】（要 malloc），只是它不存数据、永远不参与删除。
 * 建好之后：
 *
 *        head
 *          │
 *          ▼
 *      ┌───────┐
 *      │ 哨兵  │◄──┐
 *      │next ──┼───┘   自己指自己：next == head, prev == head
 *      │prev ──┼───┐
 *      └───────┘   │
 *          ▲───────┘
 *
 *   空表判据 = head->next == head（不是 head == NULL！哨兵永远在）。
 *   这就是"哨兵消灭特判"的第一次现身：连"空表"都不特殊，它就是一个自环。
 *
 * ⚠ head->val 全节【永远不读】，这里写 0 只是为了让它别是垃圾值（ASan 不管未初始化读，
 *   valgrind 管；填 0 是免费的保险）。调试打印时它显示成 [哨兵]，一眼区分。
 * 哨兵 malloc 失败时把 head 置 NULL，后续每个函数开头都判一下，宁可操作失败也不崩。
 */
int cdlist_init(CDList *l)
{
    Node *s = malloc(sizeof(Node));
    if (s == NULL) {
        l->head = NULL;                    /* 建表失败：留一个"全函数安全失败"的空壳 */
        l->size = 0;
        return 0;
    }
    s->val  = 0;
    s->prev = s;                           /* ★ 自己指向自己，环成立 */
    s->next = s;
    l->head = s;
    l->size = 0;
    return 1;
}

/*
 * ===================================================================
 * 2. cdlist_insert_after：全节【唯一】真正在改线的地方
 * ===================================================================
 * 把 n 塞到 pos 后面。要动 4 根线，顺序有讲究：
 *
 *   之前： pos ⇄ pos->next
 *                          （pos 是哨兵时 = 插到最前面；pos 是最后一个元素时 = 插到最后面）
 *
 *     ① n->prev = pos;              新节点认前驱
 *     ② n->next = pos->next;        新节点认后继（★ 必须在 ④ 之前，否则 pos->next 已被改掉）
 *     ③ pos->next->prev = n;        老后继回头认新节点（★ 也必须在 ④ 之前）
 *     ④ pos->next = n;              前驱把后继换成新节点
 *
 *   之后： pos ⇄ n ⇄ 老后继
 *
 * ★★ 为什么 ④ 必须放最后：④ 一改，"老后继"这个地址就从 pos->next 里消失了。
 *    ②③ 都要读它，所以先读后改 —— pointer-basics 那条"覆盖之前先抄旧值"的铁律，
 *    在双向链表里要守两次（② 和 ③）。
 *
 * ★★ 为什么这里【一支特判都没有】：
 *    pos 是哨兵   → 插到最前面，改的是哨兵的 next，哨兵不是"头元素"，没有任何把手要跟着挪
 *    pos 是尾元素 → pos->next 就是哨兵，②③ 天然把新节点接到尾部并让哨兵认它 —— 所谓"尾插"
 *    表是空的     → pos 只能是哨兵，它自己指自己，四步走完变成 哨兵 ⇄ n ⇄ 哨兵，一样对
 *    这就是哨兵存在的全部意义：把"头/尾/空"三种特例合并成一种通例。
 */
Node *cdlist_insert_after(CDList *l, Node *pos, int val)
{
    if (l->head == NULL || pos == NULL) {
        return NULL;                       /* 表没建成 / 位置是空指针：拒绝，链表原样不动 */
    }

    Node *n = malloc(sizeof(Node));
    if (n == NULL) {
        return NULL;
    }
    n->val = val;

    n->prev        = pos;                  /* ① */
    n->next        = pos->next;            /* ② 先抄住老后继 */
    pos->next->prev = n;                   /* ③ 老后继改认新节点当前驱 */
    pos->next       = n;                   /* ④ 最后才覆盖，之前所有读取都已做完 */

    l->size++;
    return n;                              /* ★ 把地址交出去：拿着它就能 O(1) 删（见 cdlist_erase）*/
}

/* 插在 pos 前面 = 插在 pos->prev 后面。一根 prev 白捡，零步到位。 */
Node *cdlist_insert_before(CDList *l, Node *pos, int val)
{
    if (pos == NULL) {
        return NULL;
    }
    return cdlist_insert_after(l, pos->prev, val);
}

/* 头插 = 在哨兵后面插。整条表最"特殊"的位置，代码里退化成一个参数。 */
Node *cdlist_push_front(CDList *l, int val)
{
    if (l->head == NULL) {
        return NULL;                       /* 表没建成（init 里 malloc 失败）：拒绝 */
    }
    return cdlist_insert_after(l, l->head, val);
}

/* 尾插 = 在"最后一个元素"后面插；而最后一个元素 = head->prev，一步白捡。
 * ⚠ 循环单链表（上一章）能靠只存 tail 做到这点，普通双向链表要存 tail 才行；
 *   这里连 tail 字段都不需要 —— 哨兵的 prev 就是尾巴。
 * ⚠ 为什么这里必须自己判空、不能全指望 insert_after：
 *   `l->head->prev` 是**取参数**时就算好的，l->head 是 NULL 的话这一句当场段错误，
 *   insert_after 里那道闸门根本没机会执行。判空要判在【解引用之前】，不是判在被调函数里。 */
Node *cdlist_push_back(CDList *l, int val)
{
    if (l->head == NULL) {
        return NULL;
    }
    return cdlist_insert_after(l, l->head->prev, val);
}

/*
 * ===================================================================
 * 3. cdlist_erase：本节最大卖点，改 4 根线，0 步遍历
 * ===================================================================
 *   之前：  A ⇄ [X] ⇄ B        （X 就是传进来的那个地址，别的什么都不知道）
 *   ① X->prev->next = X->next;    A 的 next 跳过 X，指向 B
 *   ② X->next->prev = X->prev;    B 的 prev 跳过 X，指向 A
 *   ③ free(X); size--;
 *   之后：  A ⇄ B
 *
 * ★ 为什么是 O(1)：前驱 A 就写在 X 自己身上（X->prev），不用去问任何人、不用走任何路。
 *   对照：单链表里同样的删除必须"从 head 走一趟找 A"，那就是 O(n)。
 *   本文件末尾的 cdlist_erase_singly_style() 就是那个对照组，步数当场数给你看。
 *
 * ★ 为什么又是一支特判都没有：
 *   X 是第一个元素 → X->prev 是哨兵，改的是哨兵的 next，不需要"更新 head"
 *   X 是最后一个   → X->next 是哨兵，改的是哨兵的 prev，不需要"更新 tail"
 *   X 是唯一元素   → X->prev == X->next == 哨兵，两句都落在哨兵身上 → 退回自环，天然正确
 *
 * ⚠ 唯一要挡的是"删哨兵本身"：它是地基，删了整条表就散了。
 *   调用者拿到的一定是 push/find 返回的节点地址，正常绝不会传成哨兵；防一手成本为零。
 */
int cdlist_erase(CDList *l, Node *node)
{
    if (l->head == NULL || node == NULL || node == l->head) {
        return 0;                          /* 空指针 / 想删哨兵：拒绝 */
    }

    node->prev->next = node->next;         /* ① 前驱越过它牵下一个（★ 读的是 node 自己身上的 prev，零步）*/
    node->next->prev = node->prev;         /* ② 后继越过它牵上一个 */
    free(node);
    l->size--;                             /* 计步器：一句都没挪指针，g_hops 不动 —— 这就是 O(1) */
    return 1;
}

/*
 * ===================================================================
 * 4. pop_front / pop_back：两头弹出都是 erase 的一行皮
 * ===================================================================
 * pop_back 尤其值得盯着看：它在【循环单链表】里做不到 O(1) ——
 * 那里要弹尾巴，必须绕一圈找尾巴的前驱；这里 head->prev 就是尾巴，一步到位。
 * val_out 允许传 NULL（"我只想弹掉，不关心值"），这是 pointer-basics 里输出参数的老约定。
 */
int cdlist_pop_front(CDList *l, int *val_out)
{
    if (l->head == NULL || l->head->next == l->head) {
        return 0;                          /* 空表：弹不出东西，val_out 一个字节都不写 */
    }
    Node *first = l->head->next;           /* 第一个元素 = 哨兵的 next */
    if (val_out != NULL) {
        *val_out = first->val;             /* ★ 必须在 free 之前读，free 之后读就是悬空 */
    }
    return cdlist_erase(l, first);
}

int cdlist_pop_back(CDList *l, int *val_out)
{
    if (l->head == NULL || l->head->prev == l->head) {
        return 0;
    }
    Node *last = l->head->prev;            /* 最后一个元素 = 哨兵的 prev ← 哨兵顺手送的第二个好处 */
    if (val_out != NULL) {
        *val_out = last->val;
    }
    return cdlist_erase(l, last);
}

/*
 * ===================================================================
 * 5. cdlist_find / cdlist_find_from_back
 * ===================================================================
 * 终止条件还是上一章那条：撞回哨兵 = 一整圈看完了（不是判 NULL）。
 * 两个方向的区别只在"从哨兵的哪一侧出发、沿哪根线走"：
 *   目标在前半段 → find 快；目标在后半段 → find_from_back 快（最少 min(i, n-i) 步）。
 * 计步器会把实际步数记下来，main.c 拿它和 erase 的 0 步并排比。
 */
Node *cdlist_find(CDList *l, int val)
{
    if (l->head == NULL) {
        return NULL;
    }
    Node *p = l->head->next;               /* 从第一个元素开始（哨兵不存数据，跳过它）*/
    while (p != l->head) {                 /* ★ 绕回哨兵 = 全看过了 */
        if (p->val == val) {
            return p;
        }
        p = p->next;
        g_hops++;                          /* 挪一次指针 = 记一步 */
    }
    return NULL;
}

Node *cdlist_find_from_back(CDList *l, int val)
{
    if (l->head == NULL) {
        return NULL;
    }
    Node *p = l->head->prev;               /* 从最后一个元素开始往回走 */
    while (p != l->head) {
        if (p->val == val) {
            return p;
        }
        p = p->prev;                       /* ← 只有双向链表才有的这条线 */
        g_hops++;
    }
    return NULL;
}

/* 按值删：找 O(n) + 摘 O(1)。总账还是 O(n)，但"摘"这一半已经不是负担了。 */
int cdlist_remove_val(CDList *l, int val)
{
    Node *t = cdlist_find(l, val);
    if (t == NULL) {
        return 0;
    }
    return cdlist_erase(l, t);
}

/* ---------- 6. 查询 ---------- */
int cdlist_size(const CDList *l)
{
    return (l->head == NULL) ? 0 : l->size;   /* 不含哨兵：哨兵不是元素 */
}

int cdlist_is_empty(const CDList *l)
{
    if (l->head == NULL) {
        return 1;
    }
    return (l->head->next == l->head) ? 1 : 0;   /* ★ 空表 = 哨兵自己牵自己，不是 head == NULL */
}

/*
 * ===================================================================
 * 7. 打印：正向一遍、反向一遍
 * ===================================================================
 * 反向打印不是"锦上添花"，它是【证据】：prev 链只要有一处没接对，
 * 反向就会提前撞回哨兵（少打几个）或者根本绕不回来（多打几个 / 死循环）。
 * 每次改完代码正反各看一眼，比写十个断言都直观。
 * 计步器在这里【不计数】—— 纯看不算算法开销，否则会把测量搅浑。
 */
void cdlist_print_forward(const CDList *l)
{
    if (l->head == NULL) {
        printf("(未建表)\n");
        return;
    }
    printf("[哨兵]");
    for (Node *p = l->head->next; p != l->head; p = p->next) {
        printf(" <=> %d", p->val);
    }
    printf(" <=> (回到哨兵)  size = %d\n", cdlist_size(l));
}

void cdlist_print_backward(const CDList *l)
{
    if (l->head == NULL) {
        printf("(未建表)\n");
        return;
    }
    printf("[哨兵]");
    for (Node *p = l->head->prev; p != l->head; p = p->prev) {
        printf(" <=> %d", p->val);          /* 走的全是 prev：这行能跑通就是 prev 接对了 */
    }
    printf(" <=> (回到哨兵)\n");
}

void cdlist_print_with_cursor(const CDList *l, const Node *cursor)
{
    if (l->head == NULL) {
        printf("(未建表)\n");
        return;
    }
    printf("  [哨兵]");
    if (cursor == l->head) {
        printf("◄当前");                     /* 游标停在哨兵上 = 还没打开任何页面 */
    }
    for (Node *p = l->head->next; p != l->head; p = p->next) {
        printf(" <=> %d", p->val);
        if ((const Node *)p == cursor) {
            printf("◄当前");
        }
    }
    printf("\n");
}

/*
 * ===================================================================
 * 8. cdlist_free：拆光元素，最后连哨兵一起还掉
 * ===================================================================
 * 顺序：先沿 next 把每个元素 free 掉（撞回哨兵就停），再 free 哨兵，
 * 最后 head = NULL、size = 0。
 * ⚠ 哨兵也要 free！它同样是 malloc 来的 —— "它只是个工具人"不是不还得理由。
 *   漏掉这一句，ASan 会实打实报一处 leak（很多教材代码就栽在这）。
 * free 完把 head 置 NULL：之后误用会当场崩（各函数的 l->head == NULL 判断也会安全拒绝），
 * 比"读到一片早已不属于你的内存"好查一万倍。
 */
void cdlist_free(CDList *l)
{
    if (l->head == NULL) {
        l->size = 0;
        return;                            /* 没建 / 已经拆过：安全空操作，不会 double free */
    }

    Node *p = l->head->next;
    while (p != l->head) {                 /* ★ 又是撞回哨兵就停 */
        Node *nxt = p->next;               /* 先抄下一个（free 之后再读 p->next 就是访问已释放内存）*/
        free(p);
        p = nxt;
    }
    free(l->head);                         /* 元素拆光了，才轮到哨兵 */
    l->head = NULL;
    l->size = 0;
}

/*
 * ===================================================================
 * 9. 对照组：cdlist_erase_singly_style —— 装作没有 prev 的删法
 * ===================================================================
 * 功能上和 cdlist_erase 完全等价，唯一区别是【怎么找到前驱】：
 *
 *   cdlist_erase            ：node->prev 就在手上        → 0 步遍历
 *   cdlist_erase_singly_style：从第一个元素出发沿 next 问"谁的 next 是它" → k 步遍历
 *
 * 后者就是单链表 / 循环单链表的算法。为什么还要在这里维护 prev 那根线？
 * 因为这条链表【物理上就是双边的】，只改 next 会把 prev 链留下指向已释放节点的脏线，
 * 反向遍历立刻变成 use-after-free。所以这里只借用单链表"绕圈找前驱"的那一步开销，
 * 收尾时照旧把 prev 接好 —— 两边唯一的公平差异就是步数，这正是我们要量的东西。
 *
 * 找不到 victim（它不在这条表上）时会绕一整圈回到哨兵，此时返回 0，绝不乱改线。
 */
int cdlist_erase_singly_style(CDList *l, Node *victim)
{
    if (l->head == NULL || victim == NULL || victim == l->head) {
        return 0;
    }

    Node *prev = l->head;                            /* 从哨兵出发：它"是"第一个元素的前驱 */
    while (prev->next != victim) {
        prev = prev->next;                           /* ← 单链表式的无奈：一路往后问 */
        g_hops++;
        if (prev == l->head) {
            return 0;                                /* 绕满一圈没遇到：不在表上，收工 */
        }
    }

    prev->next = victim->next;                       /* 以下两句和 cdlist_erase 一模一样 */
    victim->next->prev = prev;
    free(victim);
    l->size--;
    return 1;
}
