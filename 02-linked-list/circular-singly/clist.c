#include <stdio.h>
#include <stdlib.h>
#include "clist.h"

/*
 * ===================================================================
 * 0. clist_make_node / clist_init
 * ===================================================================
 * 造节点这件事和单链表【一模一样】：一个 int + 一根 next，malloc 完先把 next 置 NULL。
 * 为什么这里还置 NULL？因为新节点此刻还不属于任何环，NULL 是"我还没接线"的记号；
 * 一旦挂进环里，接线的那几个函数会立刻把 next 改成真实的地址。
 * 忘了置 NULL 的话 next 里是垃圾地址，挂进环的瞬间就是野指针 —— 和单链表同款事故。
 */
Node *clist_make_node(int val)
{
    Node *n = malloc(sizeof(Node));
    if (n == NULL) {
        return NULL;                  /* 堆不够：明确返回 NULL，让调用方决定怎么办 */
    }
    n->val  = val;
    n->next = NULL;                   /* ⚠ 必须先置空，绝不能省 */
    return n;
}

/*
 * 空表长什么样：tail 什么都不指。
 *
 *     CList:  { tail = NULL, size = 0 }
 *
 * ⚠ 循环表的"空"没有环可看，只能靠 tail == NULL 表示。
 * 这意味着：任何函数第一件事都得想清楚"tail 是 NULL 时我这行代码会不会解引用它"。
 * 单链表里空表是 head == NULL，这里是 tail == NULL —— 换了个把手，逻辑没变。
 */
int clist_init(CList *l)
{
    l->tail = NULL;
    l->size = 0;
    return 1;
}

/*
 * ===================================================================
 * 1. clist_head：head 不是存出来的，是算出来的
 * ===================================================================
 * 整个数据结构里没有任何地方"记着"头节点是谁，它就是尾巴的那根 next：
 *
 *     tail ──► [3|●] ──► [1|●] ──► [2|●] ──┐
 *                ▲                          │
 *                └──────────────────────────┘
 *         tail->next == 第一个节点，零步到达（O(1)）
 *
 * 空表时 tail 是 NULL，不能解引用，所以判一下返回 NULL。
 * 这一行判空就是本节所有"空表安全"的源头：别的函数都靠它。
 */
Node *clist_head(const CList *l)
{
    return (l->tail == NULL) ? NULL : l->tail->next;
}

/*
 * ===================================================================
 * 2. clist_push_back：尾插 O(1)
 * ===================================================================
 * 只存 tail 为什么能让尾插变成 O(1)？因为"插到最后"这件事，
 * 在循环表里等价于"插到 tail 后面"，而 tail 就在手里，一步都不用走。
 *
 *   插之前：          ┌──────────────────────┐
 *                     ▼                      │
 *        tail ──► [1|●] ──► [2|●] ──────────┘
 *
 *   第①步  n->next = tail->next（新节点牵住老 head）：
 *        tail ──► [1|●] ──► [2|●] ──┐
 *                     ▲              │
 *                     │       [9|●] ─┘
 *                     └──────────────┘（tail->next 还是 1，先记下）
 *
 *   第②步  tail->next = n（老尾巴改指新节点）：
 *        tail ──► [1|●] ──► [2|●] ──► [9|●] ──┐
 *                     ▲                        │
 *                     └────────────────────────┘
 *
 *   第③步  tail = n（把手挪到新节点上）：
 *                          tail ──► [9|●]
 *                                   ...
 *
 * ★ 三步的顺序绝不能乱：第①步必须在第②步之前。
 *   一旦先写 tail->next = n，老 head 的地址就被覆盖丢了，
 *   前面那一串节点全体失联（既走不到也 free 不掉）——
 *   pointer-basics 那条"覆盖之前先抄旧值"的铁律，换了个数据结构照样管用。
 */
int clist_push_back(CList *l, int val)
{
    Node *n = clist_make_node(val);
    if (n == NULL) {
        return 0;
    }

    if (l->tail == NULL) {
        /* 空表：新节点就是唯一节点，自己牵自己，环就立起来了。
         *
         *      tail ──► [9|●] ──┐
         *                 ▲──────┘   n->next == n
         *
         * 这个"自己指自己"是循环链表独有的画面，单链表在这里只能写 next = NULL。 */
        l->tail = n;
        n->next = n;
    } else {
        n->next        = l->tail->next;   /* ① 新节点先牵住老 head（此刻它就是环的"下一个"）*/
        l->tail->next  = n;               /* ② 老尾巴改指新节点 */
        l->tail        = n;               /* ③ 尾巴把手挪过来 */
    }
    l->size++;                            /* 账本 +1：谁增谁减都要记账，否则 size 就成了谎话 */
    return 1;
}

/*
 * ===================================================================
 * 3. clist_push_front：头插 O(1)，而且 tail 一动不动
 * ===================================================================
 * 想插到"第一个节点前面"。但循环表没有"前面"这回事 —— 插到尾巴后面，
 * 转一圈再看，新节点就是新的第一个：
 *
 *   之前（tail 停在 3）：  [1|●] ──► [2|●] ──► [3|●] ──┐
 *                           ▲                            │
 *                           └────────── tail ────────────┘
 *
 *   ① n->next = tail->next（新节点牵住老 head = 1）
 *   ② tail->next = n      （尾巴改指新节点，于是"1 前面"变成了 9）
 *   ③ tail 不动，size++
 *
 *   之后（从 tail->next 重新数一圈）：  [9] ──► [1] ──► [2] ──► [3] ──┐
 *                                        ▲                             │
 *                                        └─────────────────────────────┘
 *
 * ★ 这就是"只维护 tail"最漂亮的地方：头插只改一根线（tail->next），
 *   连 tail 这个字段本身都不用动，因为【尾巴还是那条尾巴】，只是它的下一个换了人。
 *   单链表头插要改 head 变量（所以得传 Node **），这里连"把手"都没换。
 */
int clist_push_front(CList *l, int val)
{
    if (l->tail == NULL) {
        /* 空表里"头"和"尾"是同一个位置，头插 == 尾插。
         * 转调一次就好，别复制粘贴第二套代码 —— 两套代码迟早会写歪一套。 */
        return clist_push_back(l, val);
    }

    Node *n = clist_make_node(val);
    if (n == NULL) {
        return 0;
    }

    n->next       = l->tail->next;   /* ① 新节点牵住老 head */
    l->tail->next = n;               /* ② 尾巴的 next 换成新节点 → 新节点成为新 head */
    l->size++;                       /* ③ tail 不变 */
    return 1;
}

/*
 * ===================================================================
 * 4. clist_find：终止条件从 p != NULL 改成 p != head
 * ===================================================================
 * 本节第一次正面撞墙。单链表的写法在这里会死循环：
 *
 *     ❌ for (p = head; p != NULL; p = p->next)     ← 环里根本没有 NULL，p 永远不等于 NULL
 *     ✅ do { ... } while (p != head);              ← 走回起点 = 已经看过所有节点
 *
 * 为什么用 do-while 而不是 while？
 *   因为【第一个节点必须被看一次】。写成 while (p != head) 的话，
 *   循环一开始 p 就等于 head，条件不成立，一次都不进 —— 整条表一个节点都没查。
 *   这是循环链表遍历的头号新手错，比终止条件写错还隐蔽。
 *
 * 返回值沿用单链表的约定：找到给节点地址，找不到给 NULL。
 * 这里 NULL 是"我们造出来的哨兵值"，链表本身里没有 NULL —— 说清楚能避免误会。
 */
Node *clist_find(CList *l, int val)
{
    Node *head = clist_head(l);
    if (head == NULL) {
        return NULL;                       /* 空表：连起点都没有，直接判无 */
    }

    Node *p = head;
    do {
        if (p->val == val) {
            return p;                      /* 命中：地址交出去，调用者想删想改都方便 */
        }
        p = p->next;
    } while (p != head);                   /* ★ 绕回起点就停，绝不写 p != NULL */

    return NULL;
}

/*
 * ===================================================================
 * 5. clist_count_by_walk：绕一圈现数
 * ===================================================================
 * 有了 size 账本，为什么还要一个 O(n) 的数法？
 * 为了【对账】。只要增删函数里漏掉一句 size++ / size--，账本就开始撒谎，
 * 而程序不会崩、只是长度不对 —— 这类 bug 最难查。
 * main.c 每一幕都会把"账本值"和"现数值"并排打出来，一眼看出有没有记错。
 */
int clist_count_by_walk(const CList *l)
{
    Node *head = clist_head(l);            /* clist_head 参数是 const CList*，这里在 const 上下文里调用 */
    if (head == NULL) {
        return 0;
    }

    int   cnt = 0;
    Node *p = head;
    do {
        cnt++;
        p = p->next;
    } while (p != head);
    return cnt;
}

/*
 * ===================================================================
 * 5b. clist_is_empty / clist_size：两个"账本型"查询
 * ===================================================================
 * 判空的依据只有一个字段：tail 有没有指向东西。size 不参与判空 ——
 * ⚠ 这意味着 tail 和 size 必须【成对维护】，任何增删函数漏掉其中一个，
 *   就会出现"表是空的但 size=3"这种自相矛盾的状态（本节所有函数都两句一起写）。
 * 判空为什么不用 size == 0？因为 tail 才是"真正有没有节点"的事实，
 * size 是抄下来的账；拿账来判断事实，一旦记错就全线崩盘。
 */
int clist_is_empty(const CList *l)
{
    return (l->tail == NULL) ? 1 : 0;
}

int clist_size(const CList *l)
{
    return l->size;                            /* O(1)：账本直接回答，不绕圈 */
}

/*
 * ===================================================================
 * 6. clist_remove：按值删第一个匹配，O(n)
 * ===================================================================
 * 单链表的删除之痛在这里一点没减轻：要摘掉 cur，必须改【cur 的前驱】的 next，
 * 而节点只写着"下一个是谁"，不知道上一个 → 只能一边走一边把前驱记在 prev 里。
 *
 *   prev      cur
 *    │         │
 *    ▼         ▼
 *   [1|●] ──► [2|●] ──► [3|●] ──┐
 *    ▲                            │
 *    └────────────────────────────┘
 *   prev->next = cur->next;  free(cur);
 *
 * 但"谁是 head 的前驱"这件事，在循环表里是免费的：就是 tail。
 * 所以我们【从 tail 起步】，prev = tail、cur = head，走一趟：
 *
 * 三种要特判的局面（循环表版，比单链表少一种）：
 *   A. 环里只剩这一个节点（cur == prev）：拆环 → tail = NULL
 *   B. 删的是尾巴（cur == tail）：新尾巴是它的前驱 → tail = prev
 *   C. 删的是头（cur == head）：只改 tail->next，tail 不动 —— 这句被通用逻辑 prev->next = cur->next 天然覆盖
 *   （单链表还得特判"改 head 变量本身"，这里 head 是算出来的，压根没有那个变量要改）
 */
int clist_remove(CList *l, int val)
{
    Node *head = clist_head(l);
    if (head == NULL) {
        return 0;                              /* 空表，无从删起 */
    }

    Node *prev = l->tail;                      /* tail 就是 head 的前驱，白捡 */
    Node *cur  = head;
    do {
        if (cur->val == val) {
            if (cur == prev) {
                /* A. 唯一节点：摘掉它环就没了。
                 *    ⚠ 不用去改 cur->next —— 它自己马上就要被 free 了，
                 *      改了也没人看，白白多一行让读者困惑。 */
                l->tail = NULL;
            } else {
                prev->next = cur->next;        /* 越过 cur 接线（★ 覆盖之前，cur->next 里那份"下一个"就已经被抄进 prev->next） */
                if (cur == l->tail) {
                    l->tail = prev;            /* B. 删尾巴：前驱升任新尾巴 */
                }
                /* C. 删头：上面那句 prev->next = cur->next 已经让尾巴牵住新的第一个节点了，
                 *    tail 不用动 —— 因为 tail->next 才是"头"，而头本来就是算出来的。 */
            }
            free(cur);
            l->size--;
            return 1;
        }
        prev = cur;
        cur  = cur->next;
    } while (cur != head);

    return 0;                                  /* 转一圈没找到 */
}

/*
 * ===================================================================
 * 7. clist_print：把环打印成一行
 * ===================================================================
 * 环没有"最后一个节点"（它的 next 不是 NULL），所以不能像单链表那样
 * 一路打到尾巴再补个 NULL。做法：从 head 打印到"回到 head 之前"，
 * 末尾补一句 "-> (回到 1)"，明晃晃地告诉读者这根线去哪了。
 */
void clist_print(const CList *l)
{
    Node *head = clist_head(l);
    if (head == NULL) {
        printf("(空表)\n");
        return;
    }

    Node *p = head;
    do {
        printf("%d -> ", p->val);
        p = p->next;
    } while (p != head);
    printf("(回到 %d，绕成一圈)\n", head->val);
}

/*
 * ===================================================================
 * 8. clist_walk_ring：从 tail 出发无脑走 N 步
 * ===================================================================
 * 这个函数的唯一目的，是让"绕回来"这件事从脑子里走到眼睛前：
 * 从尾巴出发，走 2×n 步，会清清楚楚看到序列是
 *     3 1 2 3 | 1 2 3 1 ...   ← 第二圈开始的瞬间，头又出现了
 * 而且【永远走不到 NULL】，这正是本节所有终止条件要改的原因。
 *
 * 注意：这里故意不设终止条件（只数步数），因为它要演示的就是"没有终点"。
 * 真实代码里绝不能这么干 —— 这是本节唯一的"许可犯规"。
 */
void clist_walk_ring(const CList *l, int steps)
{
    Node *head = clist_head(l);
    if (head == NULL) {
        printf("    (空表，一步也走不了)\n");
        return;
    }

    Node *p = l->tail;                         /* ★ 从尾巴出发 */
    for (int i = 0; i < steps; i++) {
        const char *tag;
        if (p == head && p == l->tail) {
            tag = "   ← 既是 head 也是 tail（环里只有它自己）";
        } else if (p == l->tail) {
            tag = "   ← tail（尾巴）";
        } else if (p == head) {
            tag = "   ← head！绕回来一圈了";
        } else {
            tag = "";
        }
        printf("    第 %2d 步: val = %d%s\n", i + 1, p->val, tag);
        p = p->next;
    }
}

/*
 * ===================================================================
 * 9. clist_free：拆环
 * ===================================================================
 * 和单链表的释放差在哪？只有循环条件：
 *
 *   单链表： while (cur != NULL) { nxt = cur->next; free(cur); cur = nxt; }
 *   循环表： do { nxt = cur->next; free(cur); cur = nxt; } while (cur != head);
 *
 * ★ 两条铁律一条都不能少：
 *   ① 先抄 nxt 再 free —— free 之后再读 cur->next 就是访问已释放内存（未定义行为）。
 *   ② 最后必须把 tail 置 NULL、size 归零 —— 不然调用者手里留着指向已释放节点的把手，
 *      再 print 一次就是 use-after-free。置 NULL 后误用会当场崩，比"看起来正常"好查一万倍。
 *
 * 小疑问：循环条件里的 head 指向的节点早就被 free 了，比较它安全吗？
 * 安全 —— 我们只比较【地址数值】，没有解引用它。ASan 也只管读写，不管比大小。
 */
void clist_free(CList *l)
{
    Node *head = clist_head(l);
    if (head == NULL) {
        l->size = 0;
        return;                                /* free(NULL) 合法，空表直接收摊 */
    }

    Node *cur = head;
    do {
        Node *nxt = cur->next;                 /* ① 先抄住下一个 */
        free(cur);                             /* ② 再还掉当前 */
        cur = nxt;
    } while (cur != head);                     /* ③ 绕回起点说明全拆完了 */

    l->tail = NULL;
    l->size = 0;
}

/*
 * ===================================================================
 * 10. clist_josephus：约瑟夫环 —— 循环链表的主场
 * ===================================================================
 * 题目：n 个人围成一圈（编号 1..n），从 1 号开始报数，数到 m 的人出圈，
 *       然后从他下一个人重新从 1 开始报，直到所有人都出圈。求出圈顺序。
 *
 * 为什么这题"天生就是循环链表"？
 *   数组版要写 (i + 1) % n 才能绕回去，而且每次出圈得把后面的元素整体往前搬一格（O(n)）；
 *   循环链表版本：报数 = 走一根 next（走到头自然回到起点，根本不需要取模），
 *   出圈 = 改一根线（O(1)，不用搬家）。"圈"这个语义，一个 next 就表达干净了。
 *
 * 数到 m 的人怎么找？
 *   cur 从 head 出发走 m-1 步就是第 m 个人；同时把前驱带在 prev 里（单链表的的老毛病）。
 *   起点 prev = tail —— 因为 tail 就是 head 的前驱，白捡一环。
 *
 *       prev = tail                            prev  cur
 *         │                                       │    │
 *         ▼                                       ▼    ▼
 *       [7] ◄── [1] ──► [2] ──► [3] ──► [4] ... [7] ──► [1]
 *                cur                                  （m=3：cur 停在 3，prev 停在 2）
 *
 * ★ m 比剩下的人还大也不用特殊处理：走过头自然绕回来，这就是环的好处
 *   （数组实现就得 (pos + m - 1) % size）。main.c 里专门有一幕试 m > size。
 */
int clist_josephus(int n, int m, int out[], int out_cap)
{
    if (n <= 0 || m <= 0) {
        printf("    (约瑟夫环要求 n >= 1 且 m >= 1；这次 n=%d, m=%d，拒绝执行)\n", n, m);
        return 0;
    }

    CList ring;
    clist_init(&ring);
    for (int i = 1; i <= n; i++) {
        clist_push_back(&ring, i);             /* 1..n 围成一圈，tail = n，head = 1 */
    }

    printf("    初始圈: ");
    clist_print(&ring);

    int removed       = 0;
    int round_counter = 0;                     /* 第几轮（名字避开 math.h 的 round()，防 -Wshadow）*/
    while (!clist_is_empty(&ring)) {
        Node *head = clist_head(&ring);
        Node *prev = ring.tail;                /* head 的前驱 = 尾巴，不用走 */
        Node *cur  = head;

        round_counter++;
        printf("    第 %2d 轮: 环长 %d, 从 %d 号开始报数, 数到 %d\n",
               round_counter, ring.size, head->val, m);

        /* 报数明细：m 不太大时才打，免得刷屏。
         * 注意这里【只是重走一遍看戏】，真正的删除位置由下面的主循环算，两者逻辑对称。 */
        if (m <= 20) {
            printf("      报数: ");
            Node *q = head;
            for (int k = 1; k <= m; k++) {
                printf("%d(报%d)%s", q->val, k, (k == m) ? " ★出圈" : "  ");
                q = q->next;                   /* 数过头了就自然绕回来 —— 环的好处 */
            }
            printf("\n");
        } else {
            printf("      （m = %d 太大，报数明细省略）\n", m);
        }

        for (int k = 1; k < m; k++) {          /* 走 m-1 步停在第 m 个人身上 */
            prev = cur;
            cur  = cur->next;
        }

        printf("      ⇒ %d 号出圈\n", cur->val);
        if (out != NULL && removed < out_cap) {
            out[removed] = cur->val;           /* 记答案，交给调用者去核对 */
        }
        removed++;

        /* 出圈 = 摘节点。和 clist_remove 同一套三支代码，这里直接写开看清楚 */
        if (cur == prev) {
            ring.tail = NULL;                  /* 只剩他一个：摘完连环都没了 */
        } else {
            prev->next = cur->next;            /* 越过 cur 接线 */
            ring.tail  = prev;                 /* ★★ 本节最妙的一行：把手挪到出圈者的【前驱】，
                                                *    于是 tail->next 正好是"他的下一个人"，
                                                *    下一轮的起点自动就接上了，一行特判都不用写。
                                                *    在单链表里想换起点得改 head 变量、还得搬家；
                                                *    循环表里"谁是头"本来就是约定 —— 转一下把手就是转圈。 */
        }
        free(cur);
        ring.size--;

        if (clist_is_empty(&ring)) {
            printf("      剩下: (没人了，圈已解散)\n");
        } else {
            printf("      剩下: ");
            clist_print(&ring);                /* 新一轮的起点自动是"出圈者的下一个" */
        }
    }

    /* ring 的节点在上面已经全部 free 掉了，这里只是把手复位；
     * 仍然调一次 clist_free 图个整齐（它会立刻看到 tail == NULL 然后返回）。 */
    clist_free(&ring);
    return removed;
}

/*
 * ===================================================================
 * 11. clist_deal：轮转发牌（时间片轮转调度的同款套路）
 * ===================================================================
 * 一副牌按顺序发给 k 个人，发完一圈回到第一个人。这就是 RR 调度：
 * 就绪队列排成环，每次给队头一个时间片，然后指针挪到下一个人。
 *
 * ★ 本节最想让你看见的一句话：
 *       cur = cur->next;            ← 轮转 = 走一根线
 *   数组版得写：
 *       i = (i + 1) % k;            ← 轮转 = 取模
 *   取模本身没错，但"为什么要 % k"得解释；环上走一格，"绕回去"是结构自带的，
 *   不需要向任何人解释。这就是循环链表活着的理由之一。
 *
 * 教学用的小常量：避免为了发牌再引入动态二维数组，把本节复杂度冲掉。
 * 超出上限就明确拒绝并说明原因，不偷偷截断。
 */
#define CLIST_DEAL_MAX_PLAYERS 8
#define CLIST_DEAL_MAX_CARDS   16

void clist_deal(int nplayers, const int *deck, int ndeck)
{
    if (nplayers < 1 || nplayers > CLIST_DEAL_MAX_PLAYERS) {
        printf("    (玩家数必须在 1..%d，本次 %d，拒绝执行)\n", CLIST_DEAL_MAX_PLAYERS, nplayers);
        return;
    }
    if (ndeck < 0 || ndeck > CLIST_DEAL_MAX_CARDS) {
        printf("    (牌数必须在 0..%d，本次 %d，拒绝执行)\n", CLIST_DEAL_MAX_CARDS, ndeck);
        return;
    }

    CList players;
    clist_init(&players);
    for (int i = 1; i <= nplayers; i++) {
        clist_push_back(&players, i);          /* 玩家排成一个环：1 -> 2 -> ... -> k -> 回到 1 */
    }

    int hands[CLIST_DEAL_MAX_PLAYERS][CLIST_DEAL_MAX_CARDS];
    int hcnt[CLIST_DEAL_MAX_PLAYERS];
    for (int i = 0; i < nplayers; i++) {
        hcnt[i] = 0;
    }

    printf("    玩家圈: ");
    clist_print(&players);

    Node *cur = clist_head(&players);          /* 当前该收牌的人 */
    for (int i = 0; i < ndeck; i++) {
        int pid = cur->val - 1;                /* 玩家号 1..k → 下标 0..k-1 */
        hands[pid][hcnt[pid]] = deck[i];
        hcnt[pid]++;
        printf("    第 %2d 张牌 %-4d → 玩家 %d\n", i + 1, deck[i], pid + 1);
        cur = cur->next;                       /* ★ 轮转：没有取模，没有"是不是最后一个人"的特判 */
    }

    for (int i = 0; i < nplayers; i++) {
        printf("    玩家 %d 的手牌(%d 张):", i + 1, hcnt[i]);
        for (int j = 0; j < hcnt[i]; j++) {
            printf(" %d", hands[i][j]);
        }
        printf("\n");
    }

    clist_free(&players);                      /* 玩家圈是 malloc 出来的，照样要还 */
}

/*
 * ===================================================================
 * 12. ⚠ 反面教材区：只有 main.c 的 --bug 分支会走到这里
 * ===================================================================
 * 12A. clist_count_broken_null：把单链表的 `p != NULL` 原封不动搬来数循环表。
 *      真实后果 = 死循环（链表还越数越长不了，指针永远绕圈）。
 *      为了不卡住终端，这里加了一个 cap 强行刹车，并把"有没有碰到 NULL"报告出来。
 *      cap 是本节的教学拐杖，正式代码里绝不该出现这种"走够一亿步才怀疑人生"的东西。
 */
int clist_count_broken_null(const CList *l, int cap, int *hit_null_out)
{
    int steps = 0;
    if (hit_null_out != NULL) {
        *hit_null_out = 0;                     /* 先假设"没碰到 NULL"，下面真碰到了再改 */
    }

    Node *p = clist_head(l);
    while (p != NULL) {                        /* ❌ 单链表写法：在循环表上永不为假 */
        if (steps >= cap) {
            return steps;                      /* 强行刹车；真实程序里这里就是 Ctrl-C 都拔不出来 */
        }
        steps++;
        p = p->next;
    }

    if (hit_null_out != NULL) {                /* 能走到这里才说明"确实撞到 NULL 了" */
        *hit_null_out = 1;
    }
    return steps;
}

/*
 * 12B. clist_head_val_unguarded：少写一次判空。
 *      clist_head() 里那句 `tail == NULL ? NULL : tail->next` 被省掉之后：
 *
 *          空表 tail == NULL  →  tail->next 就是"去地址 0x0 + 8 的格子里取值"  →  SIGSEGV
 *
 *      ⚠ 这个函数【一定会崩】，所以只允许在 fork 出来的子进程里调用，
 *      让父进程收尸并把"崩了"这件事打印出来 —— 默认运行绝不碰它。
 *      为什么能确定编译器不会把这次解引用优化掉？因为结果被 printf 用掉了，
 *      而且 tail 是从结构体字段读出来的，编译器无法证明它是 NULL。
 */
int clist_head_val_unguarded(const CList *l)
{
    return l->tail->next->val;                 /* ❌ 空表时这一行当场段错误 */
}
