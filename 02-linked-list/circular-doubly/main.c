#include <stdio.h>
#include <stdlib.h>
#include "cdlist.h"

/* 小工具：分幕标题（和 pointer-basics / singly / doubly / circular-singly 同款） */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/* 小工具：一行"当前状态"，省得每幕都写两行 */
static void show(const char *label, const CDList *l)
{
    printf("  %s: ", label);
    cdlist_print_forward(l);
}

/*
 * =====================================================================
 * 第 4 幕要用：把"删一个已知节点"的两种做法放在同一把尺子下量
 * =====================================================================
 * 场景设定（这是真实场景，不是编的）：LRU 缓存、任务调度器、浏览器历史里，
 * 代码常常【早就攥着某个节点的地址】（上一步 push 时返回的、或者哈希表里存的），
 * 此刻要删它。两种实现的差别只在一步：
 *
 *   cdlist_erase             ：node->prev 就写在节点身上 → 前驱零步到手
 *   cdlist_erase_singly_style：装作没有 prev，从哨兵沿 next 一路问"谁的 next 是它" → k 步
 *
 * k 有多大？就是它前面有几个节点。所以用计步器（cdlist_hops）现场数出来。
 * ⚠ 两种做法各建一条【全新的表】：A 做完，那个节点已经被 free 了，
 *   地址成了悬空指针，拿给 B 用就是 use-after-free —— 必须各建各的。
 */
#define MEASURE_MAX 256

static void measure_erase(int total, int victim_page)
{
    if (total < 1 || total > MEASURE_MAX || victim_page < 1 || victim_page > total) {
        printf("  （参数超出支持范围 1..%d，跳过这一组）\n", MEASURE_MAX);
        return;
    }
    printf("  【n = %d，删掉 val = %d 那个节点】\n", total, victim_page);

    /* --- 做法 A：单链表式，绕圈找前驱 --- */
    CDList a;
    cdlist_init(&a);
    Node *addr_a[MEASURE_MAX];
    for (int i = 1; i <= total; i++) {
        addr_a[i - 1] = cdlist_push_back(&a, i);   /* 建表时就把地址存住：全程不再 find */
    }
    cdlist_reset_hops();
    int ok_a = cdlist_erase_singly_style(&a, addr_a[victim_page - 1]);
    long hops_a = cdlist_hops();
    printf("    A 单链表式 erase_singly_style : 返回 %d, size %d → %d, 沿 next 走了 %ld 步才找到前驱\n",
           ok_a, total, cdlist_size(&a), hops_a);
    cdlist_free(&a);

    /* --- 做法 B：双向 + 已知地址 --- */
    CDList b;
    cdlist_init(&b);
    Node *addr_b[MEASURE_MAX];
    for (int i = 1; i <= total; i++) {
        addr_b[i - 1] = cdlist_push_back(&b, i);
    }
    cdlist_reset_hops();
    int ok_b = cdlist_erase(&b, addr_b[victim_page - 1]);
    long hops_b = cdlist_hops();
    printf("    B 双向式 cdlist_erase         : 返回 %d, size %d → %d, 走了 %ld 步（前驱就在 node->prev 里）\n",
           ok_b, total, cdlist_size(&b), hops_b);
    cdlist_free(&b);

    printf("    ⇒ 完全相同的一次删除：A 挪 %ld 次指针 / B 挪 %ld 次 —— n 越大差距越夸张\n",
           hops_a, hops_b);
}

/*
 * =====================================================================
 * 第 7 幕要用：浏览器前进 / 后退
 * =====================================================================
 * 历史表排成【循环双向链表】，另配一个游标 cur 表示"当前在看哪一页"：
 *   cur->prev = 后退一格能到的页面
 *   cur->next = 前进一格能到的页面
 * 哨兵在这里兼任"历史范围的边界"：撞上哨兵就是走出了历史（往前没有、往后也没有）。
 * 这一设计把"是不是第一页/最后一页"的特判全变成同一句 `== head`，正是哨兵的第二次红利。
 */
static void hist_visit(CDList *h, Node **cur, int page)
{
    /* ① 截断：新访问会抛弃"前进"分支 —— 把游标之后的节点全删掉。
     *    ⚠ 注意终止条件：绕回哨兵才停（上一章那条铁律，这里一模一样）。
     *      写成 p != NULL 的话，这里会走回哨兵继续 free —— 直接毁掉地基。 */
    int dropped = 0;
    while ((*cur)->next != h->head) {
        cdlist_erase(h, (*cur)->next);
        dropped++;
    }
    if (dropped > 0) {
        printf("    （新访问先截断：扔掉 %d 条『前进』历史）\n", dropped);
    }

    /* ② 在游标后面插一页，游标跟过去 —— 头插/尾插/中间插在这里是同一段代码 */
    Node *n = cdlist_insert_after(h, *cur, page);
    if (n != NULL) {
        *cur = n;
    }
    printf("    访问页面 %d\n", page);
    printf("    历史: ");
    cdlist_print_with_cursor(h, *cur);
}

static void hist_back(CDList *h, Node **cur)
{
    if ((*cur)->prev == h->head) {
        printf("    后退：最早的一页就是当前页，再退就出历史了（撞上哨兵 → 按钮置灰）\n");
        return;
    }
    *cur = (*cur)->prev;                     /* ★ 后退 = 走一根 prev：单链表在这里直接躺平 */
    printf("    后退 → 现在看页面 %d\n", (*cur)->val);
    printf("    历史: ");
    cdlist_print_with_cursor(h, *cur);
}

static void hist_forward(CDList *h, Node **cur)
{
    if ((*cur)->next == h->head) {
        printf("    前进：最新的一页就是当前页，没有可前进的历史（撞上哨兵 → 按钮置灰）\n");
        return;
    }
    *cur = (*cur)->next;
    printf("    前进 → 现在看页面 %d\n", (*cur)->val);
    printf("    历史: ");
    cdlist_print_with_cursor(h, *cur);
}

int main(void)
{
    CDList l;                                /* 栈上的把手；堆上只有哨兵和各个节点 */

    /* ============================================================
       第 1 幕：空表长什么样 —— 哨兵自己牵自己
       ============================================================ */
    section("1. 空表不是 head == NULL，而是【哨兵自己牵自己】");

    cdlist_init(&l);
    printf("  哨兵地址 = %p（它不存数据，永远不删）\n", (void *)l.head);
    printf("  head->next == head ? %s\n", l.head->next == l.head ? "成立" : "不成立");
    printf("  head->prev == head ? %s\n", l.head->prev == l.head ? "成立" : "不成立");
    printf("  正向看: ");
    cdlist_print_forward(&l);
    printf("  反向看: ");
    cdlist_print_backward(&l);
    printf("  is_empty = %d, size = %d（size 不含哨兵！哨兵不是元素）\n",
           cdlist_is_empty(&l), cdlist_size(&l));

    int ghost = -1;
    int popped = cdlist_pop_front(&l, &ghost);
    printf("  空表能弹吗？pop_front 返回 %d，ghost 仍是 %d（函数一个字节都没往里写）\n", popped, ghost);
    printf("  → 空表能删、能弹、能打印，全靠『哨兵永远在』这个前提：没有一支 if 在为空表特判。\n");

    /* ============================================================
       第 2 幕：两头插都只是 insert_after 的一行皮
       ============================================================ */
    section("2. push_front / push_back：两个 O(1) 都是「在某个节点后面插」的皮");

    cdlist_push_back(&l, 20);
    show("尾插 20（= insert_after(head->prev)）", &l);
    printf("      空表时 head->prev 就是 head 自己，所以『尾插』和『头插』在这一刻是同一件事\n");
    cdlist_push_back(&l, 30);
    show("尾插 30", &l);
    cdlist_push_front(&l, 10);
    show("头插 10（= insert_after(head)）", &l);
    cdlist_push_front(&l, 5);
    show("头插 5", &l);
    cdlist_push_back(&l, 99);
    show("尾插 99", &l);

    printf("\n  ★ 双向的证据：反向再走一遍（全程只走 prev）\n  ");
    cdlist_print_backward(&l);
    printf("  → 反向打印出来是倒序 = prev 链一根都没接错。\n");
    printf("    如果哪次插/删漏改了一根 prev，这里会【提前撞回哨兵】少打几个，\n");
    printf("    或者干脆绕不回来 —— 反向打印就是免费的自检工具。\n");
    printf("  size = %d（每次增删各维护一句，不是现遍历出来的）\n", cdlist_size(&l));

    /* ============================================================
       第 3 幕：在指定节点前后插
       ============================================================ */
    section("3. insert_after / insert_before：手里有地址，接在哪个节点旁边都行");

    show("起始", &l);
    cdlist_reset_hops();
    Node *pos = cdlist_find(&l, 30);
    printf("  find(30) %s，从头往后走了 %ld 步才拿到它的地址\n",
           pos != NULL ? "命中" : "未命中", cdlist_hops());
    printf("  （★ 记住这个数：『按值找』这一步永远是 O(n)，第 4 幕省掉的只是『摘』那一半）\n");
    if (pos != NULL) {
        cdlist_insert_after(&l, pos, 35);
        show("在 30 【后面】插 35", &l);
        cdlist_insert_before(&l, pos, 25);
        show("在 30 【前面】插 25", &l);
        printf("  → pos 一直是同一个节点：插前插后它都还在原位，只是多了两根挂上去的线。\n");
        printf("    insert_before 没有第二套改线代码，它就是把 pos->prev 喂给 insert_after\n");
        printf("    （一根 prev 白捡，这就是双向链表的『对称性』）\n");
    }

    printf("\n  在【哨兵】后面插 = 头插：\n");
    cdlist_insert_after(&l, l.head, 1);
    show("insert_after(head, 1)", &l);
    printf("  在【最后一个元素】后面插 = 尾插（head->prev 就是最后一个）：\n");
    cdlist_insert_after(&l, l.head->prev, 200);
    show("insert_after(head->prev, 200)", &l);
    printf("  头元素 = head->next = %d，尾元素 = head->prev = %d（两个方向各一步拿到）\n",
           l.head->next->val, l.head->prev->val);

    printf("\n  故意传 pos = NULL：\n");
    Node *bad = cdlist_insert_after(&l, NULL, 777);
    printf("  返回 %s（拒绝执行），链表原样: ", bad == NULL ? "NULL" : "非 NULL");
    cdlist_print_forward(&l);

    /* ============================================================
       第 4 幕：核心卖点实测 —— 删一个"只知道自己是哪个节点"的节点
       ============================================================ */
    section("4. ★ 实测卖点：给定节点地址直接 erase = 0 步遍历；单链表式删法要绕 k 步");

    printf("  前提复现：push 时就返回节点地址并【存进数组】（等价于 LRU 里哈希表存着节点），\n");
    printf("  之后删除全程不调 find —— 手里真的只有那个地址。\n");
    printf("  计步器只数『循环里挪 next/prev 的次数』，两边同一把尺子。\n\n");

    measure_erase(6, 1);          /* 删第 1 个：单链表式 0 步（哨兵就是它前驱）*/
    printf("\n");
    measure_erase(6, 4);          /* 删第 4 个：3 步 */
    printf("\n");
    measure_erase(6, 6);          /* 删最后 1 个：5 步（最坏） */
    printf("\n");
    measure_erase(200, 100);      /* 规模上去以后 */
    printf("\n");
    measure_erase(200, 200);      /* 最坏情况 */
    printf("\n");

    printf("  小结（数字都是上面实测出来的）：\n");
    printf("    单链表式找前驱：k = 目标前面有几个节点，删第 200 个要 199 步\n");
    printf("    双向 + 已知地址：永远 0 步，改 4 根线（其实是 2 句赋值）\n");
    printf("  ⚠ 别忘了另一半真相：如果只有【值】没有地址，还是得先 find（O(n)）。\n");
    printf("     『O(1) 删』的完整说法是：给定节点地址时，摘除动作本身 O(1)。\n");
    printf("  代价：每个节点从 16 字节涨到 24 字节（多一根 prev），而且要多维护一根线。\n");

    /* ============================================================
       第 5 幕：两头弹出 + 按值删 + 一路弹到空
       ============================================================ */
    section("5. pop_front / pop_back / remove_val：弹到空表为止，看看哨兵怎么兜住每一支特判");

    cdlist_free(&l);                             /* 换一条干净的表：先 free 再 init，不留孤儿 */
    cdlist_init(&l);
    for (int i =1; i <= 4; i++) {
        cdlist_push_back(&l, i * 10);            /* 10 20 30 40 */
    }
    show("起始", &l);

    int v = -1;
    cdlist_pop_front(&l, &v);
    printf("  pop_front 弹出 %d: ", v);
    cdlist_print_forward(&l);
    cdlist_pop_back(&l, &v);
    printf("  pop_back  弹出 %d: ", v);
    cdlist_print_forward(&l);
    printf("  ↑ pop_back 在循环【单】链表里做不到 O(1)：得绕一圈找尾巴的前驱；\n");
    printf("    这里 head->prev 就是尾巴，一步到位 —— 哨兵送的第二个好处。\n");

    cdlist_insert_after(&l, l.head, 5);          /* 5 */
    cdlist_insert_after(&l, l.head->prev, 45);   /* 45 */
    show("补两头 → 现在", &l);
    int hit = cdlist_remove_val(&l, 20);
    printf("  remove_val(20) 返回 %d: ", hit);
    cdlist_print_forward(&l);
    int miss = cdlist_remove_val(&l, 999);
    printf("  remove_val(999) 返回 %d（没找到，链表原样）: ", miss);
    cdlist_print_forward(&l);

    printf("\n  删【哨兵本身】会怎样？\n");
    int suicide = cdlist_erase(&l, l.head);
    printf("  cdlist_erase(&l, l.head) 返回 %d（拒绝：哨兵是地基，抽掉整条表就散了）\n", suicide);
    printf("  链表完好: ");
    cdlist_print_forward(&l);

    printf("\n  一路弹到空：\n");
    while (cdlist_pop_back(&l, &v)) {
        printf("    弹出 %-3d 剩 ", v);
        cdlist_print_forward(&l);
    }
    printf("  空表：head->next == head ? %s, head->prev == head ? %s, size = %d\n",
           l.head->next == l.head ? "成立" : "不成立",
           l.head->prev == l.head ? "成立" : "不成立",
           cdlist_size(&l));
    printf("  → 弹掉最后一个元素之后，两句改线都落在哨兵身上，环自动退回『自己牵自己』。\n");
    printf("    单链表在这里要写 if (head->next == NULL) head = NULL 之类的收尾，这里零支特判。\n");

    /* ============================================================
       第 6 幕：拿它当双端队列用
       ============================================================ */
    section("6. 循环双向链表当【双端队列】：两头进出，不需要预留容量");

    cdlist_free(&l);
    cdlist_init(&l);
    printf("  操作序列（一共 9 步，每步之后都把正向现状打出来）：\n");
    printf("    push_back 10 → push_front 5 → push_back 20 → pop_front → pop_back\n");
    printf("    → push_front 1 → push_back 99 → pop_front → pop_back\n\n");

    struct { const char *op; int kind; int val; } script[] = {
        { "push_back 10", 1, 10 },
        { "push_front 5", 2, 5  },
        { "push_back 20", 1, 20 },
        { "pop_front",    3, 0  },
        { "pop_back",     4, 0  },
        { "push_front 1", 2, 1  },
        { "push_back 99", 1, 99 },
        { "pop_front",    3, 0  },
        { "pop_back",     4, 0  },
    };
    int nops = (int)(sizeof(script) / sizeof(script[0]));

    for (int i = 0; i < nops; i++) {
        const char *name = script[i].op;
        int got = 0;
        int failed = 0;

        switch (script[i].kind) {
        case 1:  cdlist_push_back(&l, script[i].val);   break;
        case 2:  cdlist_push_front(&l, script[i].val);  break;
        case 3:  failed = !cdlist_pop_front(&l, &got);  break;
        default: failed = !cdlist_pop_back(&l, &got);   break;
        }

        printf("  %-14s → ", name);
        if (script[i].kind >= 3) {
            if (failed) {
                printf("空表，弹不出； ");
            } else {
                printf("弹出 %-3d；", got);
            }
        }
        cdlist_print_forward(&l);
    }

    printf("\n  ★ 实测『不需要预留容量』：接着往下连推 20000 个元素\n");
    for (int i = 0; i < 20000; i++) {
        cdlist_push_back(&l, 1000 + i);
    }
    printf("    size = %d，队头 = %d，队尾 = %d（两个都是一步从哨兵身上拿到的）\n",
           cdlist_size(&l), l.head->next->val, l.head->prev->val);
    printf("    全程一次『扩容 / 搬家』都没有：每 push_back 一次就是一回独立 malloc。\n");
    printf("    数组版在同一时刻要么已经报『满了』，要么正在 realloc 把两万个元素整体搬走。\n");
    cdlist_free(&l);
    cdlist_init(&l);
    printf("    （演示完立刻 free 掉这两万个节点 —— 留着的话 ASan 当场报泄漏：谁申请谁负责）\n");
    printf("\n");

    printf("  ★ 和 04-queue/deque/（数组实现的双端队列）的区别：\n");
    printf("    数组版：先 malloc 一整块 capacity，front/rear 靠取模在环上跳，\n");
    printf("            写满就【放不下】（或者搬家扩容 O(n)），元素紧凑、cache 友好，每个元素 4 字节。\n");
    printf("    本节版：来一个 malloc 一个，没有 capacity 这个概念，永远不会『写满』；\n");
    printf("            代价是每个元素多背 16 字节（两根指针）+ 一次堆分配，节点散落、cache 不友好；\n");
    printf("            换来的是【手里有节点地址就能 O(1) 摘】—— 数组版做不到（数组里只有下标）。\n");
    printf("    选型：数量有界、按下标读多 → 数组版；要频繁摘中间已知节点（LRU 那类）→ 链表版。\n");

    /* ============================================================
       第 7 幕：浏览器前进 / 后退
       ============================================================ */
    section("7. 应用：浏览器前进/后退历史（环上左右移动 + 新访问截断后面）");

    CDList hist;
    cdlist_init(&hist);
    Node *cur = hist.head;                       /* 游标停在哨兵 = 浏览器刚打开，还没访问任何页 */
    printf("  游标 = 哨兵，表示『还没打开任何页面』：\n");
    printf("    历史: ");
    cdlist_print_with_cursor(&hist, cur);

    hist_visit(&hist, &cur, 1);                  /* 百度 */
    hist_visit(&hist, &cur, 2);                  /* 知乎 */
    hist_visit(&hist, &cur, 3);                  /* 维基 */

    printf("\n    —— 连着后退（每退一步就是走一根 prev）——\n");
    hist_back(&hist, &cur);                      /* → 2 */
    hist_back(&hist, &cur);                      /* → 1 */
    hist_back(&hist, &cur);                      /* 已经是最早，撞哨兵 */

    printf("\n    —— 现在停在最早的页面 1 上【前进一步】（走 next）——\n");
    hist_forward(&hist, &cur);                   /* → 2 */

    printf("\n    —— 关键一幕：停在页面 2 上访问一个全新页面 9 ——\n");
    hist_visit(&hist, &cur, 9);                  /* 截断：3 被扔掉，历史变成 1 → 2 → 9 */

    printf("\n");
    show("截断后的历史（正向）", &hist);
    printf("  反向看一遍（prev 链依然正确，只是顺序反过来）: ");
    cdlist_print_backward(&hist);

    printf("\n    —— 收尾两个边界 ——\n");
    hist_forward(&hist, &cur);                   /* 9 是最新，前进无历史 */
    hist_back(&hist, &cur);                      /* → 2 */
    hist_back(&hist, &cur);                      /* → 1 */
    hist_back(&hist, &cur);                      /* 撞哨兵，置灰 */

    printf("  ★ 这个应用为什么非得是【循环 + 双向】：\n");
    printf("    后退 = 走 prev（单链表根本走不了回头路）；\n");
    printf("    前进 = 走 next；\n");
    printf("    边界判断 = 『是不是撞到哨兵』一句搞定，不用再存 first/last 两个指针；\n");
    printf("    截断 = while (cur->next != head) erase(cur->next)，\n");
    printf("    —— 注意这个 while 的终止条件正是上一章那条：撞回哨兵才停，绝不能判 NULL。\n");

    cdlist_free(&hist);
    printf("  hist 已释放（元素 + 哨兵一起还掉）\n");

    /* ============================================================
       收尾：必须 free，连哨兵一起
       ============================================================ */
    section("8. 收尾：谁 malloc 谁 free，哨兵也不例外");

    cdlist_free(&l);
    printf("  cdlist_free 之后 head = %s, size = %d\n",
           l.head == NULL ? "NULL（把手已作废，误用会当场崩）" : "非 NULL（不该发生）",
           l.size);
    printf("  再 free 一次：");
    cdlist_free(&l);
    printf("安全（head 已是 NULL，直接 return，不会 double free）\n");
    printf("  拆完之后打印：");
    cdlist_print_forward(&l);

    printf("\n=== 本节结束 ===\n");
    printf("记住五句话：\n");
    printf("  1) 哨兵不存数据、永远在场：空表是『自己牵自己』，不是 head == NULL。\n");
    printf("  2) 有了哨兵，全节只有一处在改线（insert_after）、一处在摘线（erase），头/尾/空零特判。\n");
    printf("  3) 多一根 prev 买到的东西：给定节点地址时删除 0 步遍历（实测：199 步 → 0 步）。\n");
    printf("  4) head->next 是头、head->prev 是尾：两头进出都 O(1)，直接当双端队列用，且不需要容量。\n");
    printf("  5) 环上遍历的终止条件永远是『撞回哨兵』；写 free 之前先想清楚哨兵自己也要 free。\n");
    return 0;
}
