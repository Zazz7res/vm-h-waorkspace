#include <stdio.h>
#include <stdlib.h>
#include "dlist.h"

/*
 * ===================================================================
 * 0. 内部工具：造节点
 * ===================================================================
 * 双向链表的一个节点要装三样：int + prev + next（本平台 8 + 8 + 4 = 20 字节，
 * 对齐后 24 字节；单链表只有 16 字节）。
 * 两根纸条都必须显式置 NULL，否则拿到的是堆里的垃圾地址。
 */
static Node *make_node(int val)
{
    Node *n = malloc(sizeof(Node));
    if (n == NULL) {
        return NULL;
    }
    n->val  = val;
    n->prev = NULL;
    n->next = NULL;
    return n;
}

/*
 * ===================================================================
 * 1. dlist_init：造一条空表
 * ===================================================================
 * 空表的定义要非常明确，否则后面每个函数都没法判断边界：
 *   head == NULL 且 tail == NULL 且 size == 0（三个必须同时成立，别只设两个）
 *
 * 为什么这里不收返回值：只是往调用者给的格子里写初值，不 malloc 任何东西，
 * 因此不可能失败。参数是 DList *（地址），因为要写的正是调用者手里那个结构体 ——
 * 还是那条铁律：想改外面，就传地址。
 *
 * ⚠⚠ 大坑：对【一条还挂着节点的链表】再调一次 dlist_init 是灾难。
 *   head/tail 被直接抹成 NULL，之前那些节点瞬间变成"没有任何指针指向"的孤儿，
 *   既走不到也 free 不掉 → 内存泄漏（ASan 会报成 Indirect leak）。
 *   正确的"清空重来"姿势是：dlist_free(list) 之后再 dlist_init(list)，
 *   或者直接让 dlist_free 干完（它本来就会把三个字段复位）。
 */
void dlist_init(DList *list)
{
    if (list == NULL) {
        return;
    }
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

/*
 * ===================================================================
 * 2. dlist_push_front：头插，O(1)
 * ===================================================================
 *   第①步 n->next = head：
 *          [9|p∅|n] ──► [1|p∅|n] ──► [2] ──► NULL
 *            ▲            ▲
 *           head ─────────┘（head 还指着老的第一个）
 *   第②步 head->prev = n（老头的 prev 原本 NULL，现在回头指新节点）：
 *          [9] ◄──► [1] ──► [2] ──► NULL
 *   第③步 head = n：
 *     head [9] ◄──► [1] ◄──► [2] ──► NULL
 *                       （tail 完全不用动！）
 *
 * 边界：空表。这时没有"老头"可以回头指我，head 和 tail 都要指向新节点。
 *   所以第②步之前必须判一下 head 是否为 NULL —— 忘了判就是在 NULL 上写 ->prev，当场崩。
 * 别忘了 size++：这个字段是手工记账，漏一次以后每次读到的都是错数。
 */
int dlist_push_front(DList *list, int val)
{
    if (list == NULL) {
        return 0;
    }
    Node *n = make_node(val);
    if (n == NULL) {
        return 0;
    }

    n->next = list->head;             /* ① 新节点往后牵住老的第一节点（空表时这里是 NULL，正好） */

    if (list->head != NULL) {         /* ② 老的第一节点要回头指我 —— 但空表时没有"老的第一节点" */
        list->head->prev = n;
    } else {
        list->tail = n;               /* 空表：新节点既是头也是尾，tail 必须一起设，
                                       * 忘了这行的话，之后的尾插就会改到一个 NULL 上 */
    }

    list->head = n;                   /* ③ 头把手挪到新节点 */
    list->size++;                     /* ④ 记账 */
    return 1;
}

/*
 * ===================================================================
 * 3. dlist_push_back：尾插，O(1) ★单链表的痛在这里被治好
 * ===================================================================
 * 单链表尾插要先 while (cur->next != NULL) 从头走到尾 —— O(n)。
 * 双向链表手里一直攥着 tail，直接挂上去就行。

 *   第①步 n->prev = tail：
 *     NULL ◄── [1|●] ──► [2|●] ◄─── [9|p●|n∅]
 *                              ▲        ▲
 *                            tail ──────┘
 *   第②步 tail->next = n（老尾巴开口接住新节点）
 *   第③步 tail = n（尾巴把手挪一格）
 *
 * 边界：空表时同样要特判 —— 没有老尾巴可以接，head 也得指新节点。
 *
 * 三处修改（n->prev / 老尾巴->next / tail / head）顺序不能乱：
 * 都是"先接好新线，再挪把手"。挪了把手，老地址就找不回来了。
 */
int dlist_push_back(DList *list, int val)
{
    if (list == NULL) {
        return 0;
    }
    Node *n = make_node(val);
    if (n == NULL) {
        return 0;
    }

    n->prev = list->tail;             /* ① 新节点回头牵住老尾巴 */

    if (list->tail != NULL) {         /* ② 老尾巴前伸接住新节点 */
        list->tail->next = n;
    } else {
        list->head = n;               /* 空表：新节点同时是头和尾 */
    }

    list->tail = n;                   /* ③ 尾把手挪到新节点 */
    list->size++;                     /* ④ 记账 */
    return 1;
}

/*
 * ===================================================================
 * 4. dlist_insert_after：插在指定节点后面，O(1)
 * ===================================================================
 * 前提是【你已经有了 pos 的地址】（通常是 dlist_find 返回的，或者遍历过程中拿到的）。
 *
 *   原始：  [A] ◄──► [pos] ──► [B]            （B 可能是 NULL，说明 pos 是尾）
 *   目标：  [A] ◄──► [pos] ◄──► [新] ◄──► [B]
 *
 * 五根线要改，顺序很关键 —— 先把新节点的两根线接好，再动别人指向别人的线：
 *   ① n->prev = pos
 *   ② n->next = pos->next        ← 必须在这时候读 pos->next，因为④会把 pos->next 改掉
 *   ③ if (pos->next) pos->next->prev = n;   ← 也要在④之前：B 的 prev 要改
 *   ④ pos->next = n
 *   ⑤ if (!pos->next 原本是 NULL) 说明插到了尾巴后面 → tail = n
 *
 * 边界：
 *   · pos == NULL：调用者给错了地址。要么当作头插，要么直接报错；这里选择【明确拒绝】，
 *     因为"猜调用者的意思"是 bug 的温床。
 *   · pos 是尾（pos->next == NULL）：新节点成为新尾，tail 要挪。
 *   · pos 是头：完全正常，A 就是 head，什么都不用特殊处理。
 *   · pos 不在本链表里（比如从别的链表拿来的地址）：链表结构会被接错、且 ASan 也查不出语义错。
 *     工程做法是只暴露"内部找到的节点"，别让人随便递地址进来。
 */
int dlist_insert_after(DList *list, Node *pos, int val)
{
    if (list == NULL || pos == NULL) {
        printf("      [错误] insert_after：pos 是 NULL，没法在它后面插\n");
        return 0;
    }
    Node *n = make_node(val);
    if (n == NULL) {
        return 0;
    }

    n->prev = pos;                    /* ① */
    n->next = pos->next;              /* ② 先抄住 pos 后面的老节点（★pos->next 后面会被覆盖） */

    if (pos->next != NULL) {
        pos->next->prev = n;          /* ③ 老后继回头改指新节点 */
    } else {
        list->tail = n;               /* pos 本来是尾巴 → 新节点变成尾巴 */
    }

    pos->next = n;                    /* ④ 最后才改 pos->next（旧值已在②抄走了） */
    list->size++;
    return 1;
}

/*
 * ===================================================================
 * 5. dlist_insert_before：插在指定节点前面，O(1)
 * ===================================================================
 * 和 insert_after 是镜像的：把上面所有 next 换成 prev、prev 换成 next 就得到这一版。
 *
 *   原始：  [A] ──► [pos] ──► [B]      （A 可能是 NULL，说明 pos 是头）
 *   目标：  [A] ◄──► [新] ◄──► [pos] ──► [B]     （读法：A.next=新, 新.prev=A, 新.next=pos, pos.prev=新）
 *
 *   ① n->next = pos
 *   ② n->prev = pos->prev           ← 同样必须趁 pos->prev 还是老值时读出来
 *   ③ if (pos->prev) pos->prev->next = n;  else head = n;   （pos 本来是头 → 新节点成头）
 *   ④ pos->prev = n
 */
int dlist_insert_before(DList *list, Node *pos, int val)
{
    if (list == NULL || pos == NULL) {
        printf("      [错误] insert_before：pos 是 NULL，没法在它前面插\n");
        return 0;
    }
    Node *n = make_node(val);
    if (n == NULL) {
        return 0;
    }

    n->next = pos;                    /* ① */
    n->prev = pos->prev;              /* ② 抄住 pos 前面的老节点 */

    if (pos->prev != NULL) {
        pos->prev->next = n;          /* ③ 老前驱改口接新节点 */
    } else {
        list->head = n;               /* pos 本来是头 → 新节点变成头 */
    }

    pos->prev = n;                    /* ④ 最后改 pos->prev */
    list->size++;
    return 1;
}

/*
 * ===================================================================
 * 6. dlist_remove：删【已知地址】的节点 —— 本节最大卖点，O(1)
 * ===================================================================
 * 先回忆单链表删一个节点有多痛苦：
 *   单链表必须"从 head 开始遍历，一边走一边记 prev"，因为要摘掉 X 就得改 X 前驱的 next，
 *   而节点自己不知道前驱是谁 ——【找前驱 = O(n)】。
 *   所以单链表哪怕已经拿到了 X 的地址，删除仍然是 O(n)。
 *
 * 双向链表：X->prev 就明晃晃写在节点里！前驱 O(1) 拿到，接线本身也 O(1)，
 *   于是"删一个已知节点"整体 O(1)。这就是多存一根指针换来的东西。
 *   （典型应用：LRU 缓存里"把一个已知节点提到队头"、Linux 内核的 task list。）
 *
 * 图解：摘掉 pos=[2]
 *   之前：  [1] ◄──► [2] ◄──► [3]        （tail 若在 3 上则不用动；head 若在 1 上则要改）
 *   摘掉：       [1] ◄──── ────►[3]
 *   要做两行：  prev->next = node->next;   node->next->prev = node->prev;
 *
 * ★★ 怎么处理"我是头/我是尾"这两种特殊情况？
 *   双链表根本不用像单链表那样造 dummy —— prev/next 天生是对称的：
 *     · 我是头  ⟺ node->prev == NULL  ⟺ 该改的是 list->head
 *     · 我是尾  ⟺ node->next == NULL  ⟺ 该改的是 list->tail
 *   也就是说 "NULL" 本身就是一个天然的哨兵信号，用 if (node->prev) / if (node->next)
 *   两支各管一头，四种情况（普通/仅头/仅尾/唯一节点）被两段代码全覆盖：
 *
 *     普通节点：  前驱接后继、后继接前驱                    → 两个 if 都成立
 *     是头：      改 head                                   → 第一个 if 不成立，走 else
 *     是尾：      改 tail                                   → 第二个 if 不成立，走 else
 *     唯一节点：  head = NULL 且 tail = NULL（两个 else 同时命中，链表正好变空）
 *
 * ⚠ 调用者注意：本函数返回后，传进来的 node 已经 free 掉了，外面那个指针变成悬空指针。
 *   要么别再用它，要么在调用处立刻置 NULL。
 */
void dlist_remove(DList *list, Node *node)
{
    if (list == NULL || node == NULL) {
        printf("      [错误] remove：链表或节点是 NULL，什么都不做\n");
        return;                          /* 直接拒绝，别去解引用空指针 */
    }

    if (node->prev != NULL) {
        node->prev->next = node->next;   /* 前驱跨过我要删的节点，直接牵住后继 */
    } else {
        list->head = node->next;         /* 我是头：新的头 = 我的后继（可能为 NULL） */
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;   /* 后继回头牵住前驱 */
    } else {
        list->tail = node->prev;         /* 我是尾：新的尾 = 我的前驱（可能为 NULL） */
    }

    free(node);                          /* 内存一定要还；注意这行之后 node 已经是野指针 */
    list->size--;                        /* 记账（放 free 后面也行，但习惯上"改完线再记数"） */
}

/*
 * ===================================================================
 * 7. dlist_remove_by_val：按值删，O(n)
 * ===================================================================
 * 慢在【找】，不慢在【删】：遍历一遍拿到节点地址（O(n)），
 * 之后 dlist_remove 那一套是 O(1)。
 * 这个对比非常关键 —— 说明双向链表并没有让"查找"变快，
 * 它只让"我已经知道是哪个节点了，动手"这件事变快。
 *
 * 边界：值不存在 → 返回 0；有多个匹配 → 只删最靠前那个；删完正好空表 → head/tail 自动归 NULL。
 */
int dlist_remove_by_val(DList *list, int val)
{
    Node *cur = dlist_find(list, val);   /* 找：O(n) —— 这一步单双向都躲不掉 */
    if (cur == NULL) {
        printf("      [提示] remove_by_val(%d)：没找到，链表保持原样\n", val);
        return 0;
    }
    dlist_remove(list, cur);             /* 删：O(1) —— 双向链表的真正优势 */
    return 1;
}

/*
 * ===================================================================
 * 8. dlist_pop_front / dlist_pop_back：弹出并带出值
 * ===================================================================
 * 头尾各留一个把手的好处在这里体现：出队 / 出栈都是 O(1)，
 * 而且不用遍历（单链表 pop_back 得走一整趟找倒数第二个）。
 *
 * val_out 是"输出参数"：主结果（成功与否）用 return，节点的值让调用者先给个地址，我写进去。
 * 传 NULL 表示"值我不要，只要把这个节点摘掉"。
 */
int dlist_pop_front(DList *list, int *val_out)
{
    if (list == NULL || list->head == NULL) {
        return 0;                        /* 空表没东西可弹，绝不能去读 head->val */
    }
    Node *old = list->head;
    if (val_out != NULL) {
        *val_out = old->val;             /* ★ free 之前先把值抄出来 */
    }
    dlist_remove(list, old);             /* 复用上面的 O(1) 删除：head 会被自动更新 */
    return 1;
}

int dlist_pop_back(DList *list, int *val_out)
{
    if (list == NULL || list->tail == NULL) {
        return 0;
    }
    Node *old = list->tail;
    if (val_out != NULL) {
        *val_out = old->val;
    }
    dlist_remove(list, old);             /* 复用：tail 会被自动更新成 old->prev */
    return 1;
}

/*
 * ===================================================================
 * 9. dlist_find / dlist_find_from_tail：两个方向都能找
 * ===================================================================
 * 双向链表的"双向"就体现在这里：既能 head->next 往后走，也能 tail->prev 往前走。
 * 什么情况下从尾巴找更快？【你大致知道目标在链表后半段】的时候。
 * 比如实现"最近使用"的缓存，新东西都在尾巴上，从尾巴找命中最快。
 *
 * 遍历的写法要牢记：
 *   往后走：for (cur = head; cur != NULL; cur = cur->next)
 *   往前走：for (cur = tail; cur != NULL; cur = cur->prev)   ← 起始点和步进都换了
 * 空表时 head/tail 都是 NULL，for 的条件第一次就为假，循环体一次都不执行 —— 天然安全。
 */
Node *dlist_find(DList *list, int val)
{
    if (list == NULL) {
        return NULL;
    }
    for (Node *cur = list->head; cur != NULL; cur = cur->next) {
        if (cur->val == val) {
            return cur;
        }
    }
    return NULL;
}

Node *dlist_find_from_tail(DList *list, int val)
{
    if (list == NULL) {
        return NULL;
    }
    for (Node *cur = list->tail; cur != NULL; cur = cur->prev) {
        if (cur->val == val) {
            return cur;
        }
    }
    return NULL;
}

int dlist_size(const DList *list)
{
    if (list == NULL) {
        return 0;
    }
    return list->size;      /* O(1)：这是维护出来的账，不是现场数出来的 */
}

/*
 * ===================================================================
 * 10. 打印：正向 / 反向
 * ===================================================================
 * 正向画成：  NULL <= [1] <=> [2] <=> [3] => NULL
 *   "<=>" 表示 prev/next 互指；左端 "NULL <=" 表示 head 的 prev 是 NULL，
 *   右端 "=>" 表示 tail 的 next 是 NULL。
 *   空表要单独印一行：因为这时【一个节点都没有】，循环一次不进，
 *   只印 "NULL" 会看不出是"空"还是"忘了判边界"。
 *
 * 反向打印：从 tail 出发一路 cur = cur->prev 走到头，箭头画成 "<-"，
 *   印出来数字是倒序的 —— 这就【实测证明】prev 这根线真的能倒着走。
 *
 * 参数写成 const DList *list：承诺"我只读不改"。写错了编译器会提醒。
 */
void dlist_print_forward(const DList *list)
{
    if (list == NULL) {
        printf("(链表把手本身是 NULL)\n");
        return;
    }
    if (list->head == NULL) {
        printf("NULL <=> NULL   (空表)\n");
        return;
    }

    printf("NULL <=");
    for (const Node *cur = list->head; cur != NULL; cur = cur->next) {
        printf(" [%d]", cur->val);
        printf(cur->next != NULL ? " <=>" : " =>");   /* 最后一个后面是 tail->next == NULL */
    }
    printf(" NULL\n");
}

void dlist_print_backward(const DList *list)
{
    if (list == NULL) {
        printf("(链表把手本身是 NULL)\n");
        return;
    }
    if (list->tail == NULL) {
        printf("NULL <=> NULL   (空表)\n");
        return;
    }

    for (const Node *cur = list->tail; cur != NULL; cur = cur->prev) {
        printf("[%d]", cur->val);
        if (cur->prev != NULL) {
            printf(" <- ");
        }
    }
    printf("  <- NULL   (再往 prev 走就出表了)\n");
}

/*
 * ===================================================================
 * 11. dlist_free：全部释放
 * ===================================================================
 * 又是那条铁律：free 之前先把 next 抄住。
 *   ✔ 正确： nxt = cur->next; free(cur); cur = nxt;
 *   ❌ 错误： free(cur); cur = cur->next;   ← 读已释放内存（未定义行为）
 * 抄完还要把 head/tail/size 复位，否则这个把手里还存着已还不存在的地址（悬空），
 * 下次再被误用就出事；复位之后重复调用 dlist_free 也只是空转（free(NULL) 合法）。
 */
void dlist_free(DList *list)
{
    if (list == NULL) {
        return;
    }
    Node *cur = list->head;
    while (cur != NULL) {
        Node *nxt = cur->next;      /* ① 先抄住下一个 */
        free(cur);                  /* ② 再还掉当前 */
        cur = nxt;                  /* ③ 走到抄下来的地址 */
    }
    list->head = NULL;              /* ④ 三个字段一起复位，保持"空表"定义完整 */
    list->tail = NULL;
    list->size = 0;
}
