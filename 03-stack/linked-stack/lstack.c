#include <stdio.h>
#include <stdlib.h>   /* malloc / free */
#include "lstack.h"

/*
 * ===================================================================
 * lstack.c —— 链表栈的实现
 * ===================================================================
 * 内存图（连续 push 10、20、30 之后）。⚠ 注意节点在堆上是【散着】的，
 * 顺序完全靠 next 指针串起来，跟数组栈的「连续格子」是两个世界：
 *
 *   s->head ──┐
 *             ↓
 *   ┌───────────────┐      ┌───────────────┐      ┌───────────────┐
 *   │ val │ 30      │      │ val │ 20      │      │ val │ 10      │
 *   │ next│ 0x7f1b… ├──→    │ next│ 0x7f22… ├──→   │ next│  (NULL) │
 *   └───────────────┘      └───────────────┘      └───────────────┘
 *      栈顶（最新）                                    栈底（最老）
 *   size = 3
 *
 *   push(40) 只干三件事，一个节点都不用搬：
 *     ① 新节点 new = malloc
 *     ② new->next = s->head      ← 先接上老栈顶（顺序不能反！）
 *     ③ s->head   = new          ← 再把头指针换成新节点
 *
 *   pop() 也是三步：
 *     ① 记下 old = s->head（待会儿要 free 它）
 *     ② s->head = old->next      ← 先让 head 跳到下一个（必须在 free 之前！）
 *     ③ 读出 old->val，free(old)
 *
 *   ★ 为什么不在尾部操作？
 *     尾部 push 得先「从头走到最后一个节点」才能把新节点挂上去：
 *        head → … → … → tail → NULL ，要把 tail->next 改写就得先找到 tail，
 *     而找 tail 只能一路 next 走到底，第 k 次 push 走 k 步 → O(n)。
 *     尾部 pop 更糟：单链表不能倒着走，要删 tail 还得找到【倒数第二个】节点，
 *     还是 O(n)。所以「栈顶放尾部」直接把栈做废了。
 *     （要真想在两端都 O(1)，得用双向链表，或者干脆用 04-queue/linked-queue 的「头出尾进」。）
 */

/*
 * -------------------------------------------------------------------
 * 造一个孤立节点
 * -------------------------------------------------------------------
 * 单独抽出来是因为 main.c 的「失败写法」演示需要一个「调用者持有的节点」，
 * 好让外面负责 free。正式操作里 push 内部也复用它。
 * -------------------------------------------------------------------
 */
LNode *lstack_make_node(int value)
{
    LNode *node = malloc(sizeof(LNode));   /* 注意是 sizeof(LNode)：int + 一个指针 */
    if (node == NULL) {
        printf("      [错误] malloc 一个链表节点失败\n");
        return NULL;
    }
    node->val  = value;
    node->next = NULL;   /* ⚠ 一定要显式置 NULL。malloc 不清零，next 里是脏地址，
                            万一后面忘了接就顺着脏地址走下去 → 段错误或更隐蔽的破坏。*/
    return node;
}

/*
 * -------------------------------------------------------------------
 * 初始化
 * -------------------------------------------------------------------
 * 链表栈不需要预申请内存，所以 init 只是把两个字段清干净，永远不会失败。
 * head = NULL 就是「空栈」的唯一定义：不靠 size==0 判断（虽然两者等价），
 * 因为 size 可能被写错，而 head 是真实结构。
 * -------------------------------------------------------------------
 */
void lstack_init(LStack *s)
{
    if (s == NULL) {
        return;
    }
    s->head = NULL;
    s->size = 0;
}

/*
 * -------------------------------------------------------------------
 * 销毁：一路 free 掉所有节点
 * -------------------------------------------------------------------
 * 一步步：
 *   ① cur = head
 *   ② 循环：next_node = cur->next（先把下一个地址存好）→ free(cur) → cur = next_node
 *   ③ 出来时 cur 已经是 NULL，把 head、size 复位
 *
 * ⚠ 最容易写错的顺序：`free(cur); cur = cur->next;`
 *   —— free 之后再读 cur->next 是「释放后使用（use-after-free）」，
 *   内存可能被别的 malloc 复用，读到的 next 是垃圾，链表当场断裂。
 * -------------------------------------------------------------------
 */
void lstack_destroy(LStack *s)
{
    if (s == NULL) {
        return;
    }

    LNode *cur = s->head;
    int freed = 0;
    while (cur != NULL) {
        LNode *next_node = cur->next;   /* ★ 先备份下一个地址，才能安全 free 当前 */
        free(cur);
        cur = next_node;
        freed++;
    }

    printf("    destroy：一路 free 掉 %d 个还没弹走的节点（谁 malloc 谁 free）\n", freed);
    s->head = NULL;
    s->size = 0;
}

/*
 * -------------------------------------------------------------------
 * push：在链表头部插一个节点（严格 O(1)，不搬任何数据）
 * -------------------------------------------------------------------
 * 一步步：
 *   ① 造新节点，malloc 失败直接返回 false
 *   ② new->next = s->head        ← 新节点指向「老栈顶」
 *   ③ s->head   = new            ← 盒子 head 换成「新节点」
 *   ④ s->size++
 *
 * ⚠ ②③ 顺序绝对不能反！
 *   如果先写 s->head = new，那 s->head 已经变成 new 了，
 *   再写 new->next = s->head 就是 new->next = new —— 自己指向自己，
 *   链表变成一个长度为 1 的环，打印/销毁都会出大问题（destroy 里会无限循环）。
 *   这个 bug 编译器一点都不会报，只能靠画图想清楚。
 * -------------------------------------------------------------------
 */
bool lstack_push(LStack *s, int value)
{
    if (s == NULL) {
        return false;
    }

    LNode *new_node = lstack_make_node(value);
    if (new_node == NULL) {
        return false;
    }

    new_node->next = s->head;   /* ② 先接上老栈顶（空栈时 s->head 是 NULL，正好成为链尾）*/
    s->head = new_node;         /* ③ 再换头指针 —— 这一步改的是「盒子里的字段」，所以 s-> 就够了 */
    s->size++;                  /* ④ 记账：以后判空、报规模都不用遍历 */
    return true;
}

/*
 * -------------------------------------------------------------------
 * pop：摘掉头节点并 free
 * -------------------------------------------------------------------
 * 一步步：
 *   ① head == NULL → 空栈，false，不写 out_value
 *   ② old = s->head                       （存下待会儿要释放的节点）
 *   ③ s->head = old->next                 （★ head 必须先跳走，否则 free 后无路可走）
 *   ④ v = old->val → 写进 out_value（可 NULL）
 *   ⑤ free(old); s->size--
 *
 * 数组栈 pop 只是 top--，内存里旧值还在；
 * 链表栈 pop 是【真的把那块内存还给系统】，旧值当场失效 —— 所以③④必须在⑤之前。
 * -------------------------------------------------------------------
 */
bool lstack_pop(LStack *s, int *out_value)
{
    if (s == NULL) {
        return false;
    }

    if (s->head == NULL) {
        printf("      [失败] pop：栈是空的（head == NULL，size=%d）\n", s->size);
        return false;
    }

    LNode *old = s->head;              /* ② */
    s->head = old->next;               /* ③ 先跳走 */

    int v = old->val;                  /* ④ 先取值 */
    if (out_value != NULL) {
        *out_value = v;
    }

    free(old);                         /* ⑤ 最后释放：不 free 就是内存泄漏 */
    old = NULL;                        /* 局部变量置空，防止后面手滑再读它 */

    s->size--;                         /* ⑥ 记账：不维护 size 的话判空还得遍历整条链 */
    return true;
}

/*
 * -------------------------------------------------------------------
 * peek：只读栈顶，不摘节点
 * -------------------------------------------------------------------
 * 注意 head == NULL 时必须先挡住，否则 s->head->val 就是解引用空指针。
 * const LStack * 承诺只读：想改也改不了（编译报错），读者一看签名就放心。
 * -------------------------------------------------------------------
 */
bool lstack_peek(const LStack *s, int *out_value)
{
    if (s == NULL || s->head == NULL) {
        printf("      [失败] peek：栈是空的，没有栈顶可看\n");
        return false;
    }
    if (out_value != NULL) {
        *out_value = s->head->val;     /* 就这一行：栈顶永远是 head 指向的节点 */
    }
    return true;
}

bool lstack_is_empty(const LStack *s)
{
    return s == NULL || s->head == NULL;   /* 空栈的定义就是「head 不指向任何东西」*/
}

int lstack_size(const LStack *s)
{
    return (s == NULL) ? 0 : s->size;      /* 有 size 字段就不用遍历了，O(1) */
}

/*
 * -------------------------------------------------------------------
 * 教学打印：把整条链摊开
 * -------------------------------------------------------------------
 * 输出形如：
 *   head → [30] → [20] → [10] → NULL      size=3（左=栈顶，右=栈底）
 * 左边是栈顶，因为链表只能从 head 出发往后走，「最先被看到的」就是栈顶。
 * -------------------------------------------------------------------
 */
void lstack_print(const LStack *s, const char *label)
{
    if (s == NULL) {
        printf("  (空指针，没有栈可打印)\n");
        return;
    }

    printf("  %s  size=%d：    ", label, s->size);
    if (s->head == NULL) {
        printf("head → NULL   （空栈：一个节点都没有）\n");
        return;
    }

    printf("head → ");
    for (LNode *cur = s->head; cur != NULL; cur = cur->next) {
        printf("[%d] → ", cur->val);
    }
    printf("NULL   （左=栈顶/最新，右=栈底/最老）\n");
}

/*
 * -------------------------------------------------------------------
 * 反例：只传一级指针 LNode *head
 * -------------------------------------------------------------------
 * 函数里看起来做得很对：new_node->next = head; head = new_node;
 * 但 head 只是外面那个指针的【拷贝】，函数一返回拷贝就销毁，
 * 外面 LStack 里的 head 根本不知道有新节点插进来了。
 *
 * 这就是 00-c-foundation/pointer-basics 第 2 节 swap_by_value 的翻版，
 * 只不过这次拷贝的是指针而不是 int，看起来「明明是地址啊怎么还改不到」。
 * 关键区别：
 *   · 传 LNode *head  → 能改到【节点里的内容】（*head），改不到【head 自己】
 *   · 想改 head 本身  → 必须传它的地址：LNode **head（或者把 head 装进结构体，传 LStack *）
 * -------------------------------------------------------------------
 */
void lstack_push_broken(LNode *head, LNode *new_node)
{
    new_node->next = head;      /* 这行是有效的：改的是 new_node 指向的节点内部 */
    head = new_node;            /* ❌ 这行只改了参数拷贝！外面的 head 一动不动 */
    printf("      [函数内部] 我把 head 指到新节点了：head 现在 = %p（值=%d）\n",
           (void *)head, head->val);
}

/*
 * -------------------------------------------------------------------
 * 正统写法之二：传二级指针 LNode **head
 * -------------------------------------------------------------------
 * 不用结构体时（比如题目要求函数直接操作一个 Node* 变量），标准签名就是这个。
 *   head  ── 一级指针：指向某个节点
 *   &head ── 二级指针：指向「那个装着节点地址的指针」
 * 所以 `*head` 就是外面那个 head 本身，`*head = new_node;` 真的改到了外面。
 * 这里同时把新 head 也 return 回去，两种带出结果的方式都能用（demo 里用 *head）。
 * -------------------------------------------------------------------
 */
LNode *lstack_push_double_pointer(LNode **head, int value, int *size)
{
    if (head == NULL) {
        return NULL;
    }

    LNode *new_node = lstack_make_node(value);
    if (new_node == NULL) {
        return *head;
    }

    new_node->next = *head;   /* 老栈顶 = *head（「外面那个 head 里存的地址」）*/
    *head = new_node;         /* ★ 一个星号：写的就是外面那个指针本身，真的改到了 */

    if (size != NULL) {
        (*size)++;            /* 括号必须有：*size + 1 是「值加一」，先加后乘优先级会出错 */
    }
    return new_node;
}

/*
 * -------------------------------------------------------------------
 * 反例之二：偏要在链表尾部压栈，并且把走了几步数出来给你看
 * -------------------------------------------------------------------
 * 一步步：
 *   ① 造新节点（这一步还是 O(1)）
 *   ② 空栈的话直接 head = new，走 0 步
 *   ③ 否则 cur = head，一路 cur = cur->next 走到最后一个节点，每走一次 steps++
 *   ④ cur->next = new，size++
 *
 * 返回「访问过多少个节点」= 必须走的路径长度。第 k 次 push 要走 k 步 → O(n)。
 * 这就是为什么栈顶绝不能放在单链表尾部。
 * -------------------------------------------------------------------
 */
int lstack_push_at_tail_count_steps(LStack *s, int value)
{
    if (s == NULL) {
        return -1;
    }

    LNode *new_node = lstack_make_node(value);
    if (new_node == NULL) {
        return -1;
    }

    if (s->head == NULL) {          /* 空栈：不用走，直接当唯一节点 */
        s->head = new_node;
        s->size++;
        return 0;
    }

    int steps = 0;
    LNode *cur = s->head;
    while (cur->next != NULL) {     /* 走到「最后一个节点」：它的 next 是 NULL */
        cur = cur->next;
        steps++;                    /* 每挪一格记一步，这就是纯开销 */
    }
    cur->next = new_node;           /* 挂到尾巴上（新节点的 next 已经是 NULL）*/
    s->size++;
    return steps + 1;               /* +1：起点那个节点也算「访问过」*/
}
