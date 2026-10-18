#include <stdio.h>
#include <stdlib.h>
#include "struct-and-typedef.h"

/*
 * ===================================================================
 * 0. sizeof 实测：结构体到底占几格？为什么 char + int 不等于 5？
 * ===================================================================
 * 编译器排结构体时不是"紧挨着塞"，而是给每个字段找一个"对齐友好"的位置：
 *
 *   内存地址:  0    1    2    3    4    5    6    7
 *            ┌────┬────┬────┬────┬────┬────┬────┬────┐
 *   CharInt  │ c  │ 填 │ 填 │ 填 │        n          │
 *            └────┴────┴────┴────┴────┴────┴────┴────┘
 *                 ↑ 1 字节        ↑ 中间 3 字节是垃圾(padding)
 *                                   ↑ int 必须放在 4 的倍数地址上，所以从 4 开始
 *
 * 所以 sizeof(CharInt) == 8，不是 5。
 * 结构体总大小还得是"最大字段对齐值"的整数倍 —— 这里最大对齐是 4，8 正好。
 *
 * 学这个有什么用？
 *   - 后面链表节点 sizeof(Node) = 4(val) + 4(填) + 8(next) = 16，
 *     知道它为什么不是 12，你就明白"指针占 8 字节、而且要摆在 8 的倍数地址上"。
 *   - 传大结构体的成本，就是这些字节要一个个抄进栈里，能省则省。
 */
size_t size_of_char_int(void) { return sizeof(CharInt); }
size_t size_of_point(void)    { return sizeof(Point); }
size_t size_of_rect(void)     { return sizeof(Rect); }
size_t size_of_node(void)     { return sizeof(Node); }

/*
 * ===================================================================
 * 1. 打印一个点
 * ===================================================================
 * 为什么这里按值传 Point 而不是传指针？
 *   因为函数只是"看一眼就还回去"，不打算改。8 字节的拷贝成本可以忽略，
 *   而且不用操心"调用者会不会传 NULL 进来"，更安全。
 * 规则：小结构体只读 → 按值；要改 → 按指针；很大（比如 Rect 16 字节以上）只读 → 按 const 指针。
 *
 * 注意 tag 是个 const char *：字符串字面量放在只读的常量区，我们只读不改，
 * 这是本项目唯一用到指针当参数的地方（它不是"元素类型"，不算泛型）。
 */
void print_point(const char *tag, Point p)
{
    printf("  %s: x=%d, y=%d   （&p=%p，这是函数内部那份拷贝的地址）\n",
           tag, p.x, p.y, (void *)&p);
}

/*
 * ===================================================================
 * 2. 值传递：改不到调用者的点（反面教材）
 * ===================================================================
 * main 里 p = (1, 1)，调 move_point_by_value(p, 10, 20)：
 *
 *   main 的 p        函数参数的 p
 *   ┌────┬────┐      ┌────┬────┐
 *   │  1 │  1 │  ==> │  1 │  1 │  ← 整块 8 字节抄一份过来
 *   └────┴────┘      └────┴────┘
 *    地址 0x7ff8…      地址 0x7ff8…（另一个位置！）
 *                         │
 *                    函数里改成 (11, 21)，只动了下面这份
 *
 * 函数一返回，参数那份拷贝随栈帧销毁，main 里的 p 一动不动。
 * 这就是第 1 节 swap_by_value 的结构体版本 —— 错误长得一模一样。
 */
void move_point_by_value(Point p, int dx, int dy)
{
    p.x += dx;          /* 改的是复印件 */
    p.y += dy;          /* 改的是复印件 */
    printf("      [函数内部] 把点搬成了 x=%d, y=%d  ← 看着成功了\n", p.x, p.y);
}

/*
 * ===================================================================
 * 3. 指针传递：真的改到原件（正确写法）
 * ===================================================================
 * 传进来的还是"整块拷贝"，但拷贝的只有 8 字节的地址；
 * 而且地址有个魔法：拷贝出来的地址，指向的仍然是同一块内存。
 *
 *   main 的 p (地址 0xA00)          函数参数 p（值 = 0xA00，本身住在 0xB00）
 *   ┌────┬────┐                        │
 *   │  1 │  1 │ <──────────────────────┘ p->x 就是 (*p).x，就是 0xA00 里那个 x
 *   └────┴────┘
 *
 * p->x 这个写法叫"解引用取成员"，等价于 (*p).x。
 * 记住这一句就不会混：
 *   手里是"东西"      → 用点： p.x
 *   手里是"地址"      → 用箭头：p->x
 */
void move_point_by_pointer(Point *p, int dx, int dy)
{
    if (p == NULL) {
        /* 别人传了个空地址进来，硬解引用就是段错误。真实函数都必须挡这一刀 */
        printf("      [错误] 传进来的是 NULL，没法改\n");
        return;
    }
    p->x += dx;         /* 等价于 (*p).x += dx;  箭头左边是地址，右边是成员名 */
    p->y += dy;
    printf("      [函数内部] p->x=%d, p->y=%d  （改的是 main 那块内存）\n", p->x, p->y);
}

/*
 * ===================================================================
 * 4. 结构体之间赋值 == 每个字段整块拷贝
 * ===================================================================
 * `Point t = *a;` 这一行，编译器会展开成"把 a 指向的 8 字节搬到 t 的 8 字节里"。
 * 所以交换两个点只要三次整块拷贝，不需要 x、y 分开写。
 *
 * ⚠ 重要边界情况：如果结构体里有 `int *data` 这种"指针字段"，
 *   赋值只会把地址抄过去，两个结构体会指向同一块堆内存 —— 这叫浅拷贝。
 *   之后谁 free 谁就留下悬空，还可能 free 两次。
 *   链表节点的 next 字段也是同理：`b = a` 之后两条链共用后面的节点。
 *   本节结构体全是 int / char，没有指针字段，所以浅拷贝 == 深拷贝，安全。
 */
void swap_points(Point *a, Point *b)
{
    if (a == NULL || b == NULL) {
        return;
    }
    Point temp = *a;    /* 整块抄 8 字节 */
    *a = *b;            /* 整块覆盖 */
    *b = temp;
}

/*
 * ===================================================================
 * 5. 结构体数组：n 个结构体肩并肩排成一排
 * ===================================================================
 *   pts  ┌─────────┬─────────┬─────────┬─────┐
 *        │(0,0)    │(1,2)    │(2,4)    │ ... │
 *        └─────────┴─────────┴─────────┴─────┘
 *         +0       +8       +16       +24      ← 每个 Point 8 字节，所以间隔 8
 *
 * 和第一节的"数组退化"一样：pts 作参数会退化成指针，长度必须另传 n。
 * 这里参数写成 `Point pts[]`，编译器偷偷当成 `Point *pts`，两者完全一样。
 */
void init_point_array(Point pts[], int n)
{
    if (pts == NULL || n <= 0) {
        return;
    }
    for (int i = 0; i < n; i++) {
        pts[i].x = i;        /* pts[i] 是一个 Point（东西），所以用点 */
        pts[i].y = 2 * i;
    }
}

void print_point_array(const Point *pts, int n)
{
    if (pts == NULL || n <= 0) {
        printf("  （空数组）\n");
        return;
    }
    printf("  结构体数组共 %d 个元素，总字节数 = %d × %zu = %zu\n",
           n, n, sizeof(Point), (size_t)n * sizeof(Point));
    for (int i = 0; i < n; i++) {
        /* 两种等价写法都打一遍，让你确认它们真的是一回事 */
        printf("    [%d] (%d, %d)  地址=%p    用指针写法 *(pts+%d) = (%d, %d)\n",
               i, pts[i].x, pts[i].y, (void *)&pts[i], i, (pts + i)->x, (pts + i)->y);
    }
}

/*
 * ===================================================================
 * 6. 大结构体：按值 vs 按指针的"成本"到底差在哪
 * ===================================================================
 * Rect 里装两个 Point，所以 sizeof(Rect) == 16。
 *
 *   rect_area_by_value(r)     ：把 16 字节整个抄进栈 → 拷贝 16 字节
 *   rect_area_by_pointer(&r)  ：只抄一个地址        → 拷贝 8  字节
 *
 * 一次两次无所谓，但后面写线段树、B 树的时候，函数被调用几百万次，
 * 每层递归都要抄一份结构体，差距就很实在了。
 * 更关键的是第二点：按值传进来的东西是"复印件"，函数里改不到原件；
 * 想让调用者看见修改，就必须传指针 —— 成本只是顺带的考虑。
 * const Rect * 的 const 是承诺"我只读"，写错了编译器立刻拦下。
 */
long long rect_area_by_value(Rect r)
{
    int w = r.bottom_right.x - r.top_left.x;
    int h = r.bottom_right.y - r.top_left.y;
    if (w < 0) { w = -w; }
    if (h < 0) { h = -h; }
    return (long long)w * h;
}

long long rect_area_by_pointer(const Rect *r)
{
    if (r == NULL) {
        return 0;
    }
    /* 嵌套 + 指针，箭头可以连着用：r->top_left.x 读作"r 指的那块内存的 top_left 里的 x" */
    int w = r->bottom_right.x - r->top_left.x;
    int h = r->bottom_right.y - r->top_left.y;
    if (w < 0) { w = -w; }
    if (h < 0) { h = -h; }
    return (long long)w * h;
}

void print_rect(const char *tag, const Rect *r)
{
    if (r == NULL) {
        printf("  %s: (NULL)\n", tag);
        return;
    }
    printf("  %s: 左上(%d,%d) 右下(%d,%d)  面积=%lld\n",
           tag,
           r->top_left.x, r->top_left.y,
           r->bottom_right.x, r->bottom_right.y,
           rect_area_by_pointer(r));
}

/*
 * ===================================================================
 * 7. 自引用结构：手工搭一条链
 * ===================================================================
 * build_node_chain(3, 10) 造出来的东西：
 *
 *   head
 *     │
 *     ↓
 *   ┌────┬──────┐      ┌────┬──────┐      ┌────┬──────┐
 *   │ 10 │ next ├─────>│ 11 │ next ├─────>│ 12 │ next ├─────> NULL
 *   └────┴──────┘      └────┴──────┘      └────┴──────┘
 *    堆上 16 字节        堆上 16 字节        堆上 16 字节
 *
 * 三件事必须注意：
 *   1) 每个节点单独 malloc。它们是三块互不相干的堆内存，地址不连续也完全正常
 *      —— 链表靠的就是"用 next 字段把散落的节点串起来"，不需要肩并肩。
 *   2) 最后一个节点的 next 必须显式置 NULL，否则它是随机值，遍历时会飞出去。
 *   3) malloc 后必须判空。这里失败了就把已申请的节点先 free 再返回 NULL，
 *      绝对不能"留着半成品跑掉"，那是泄漏。
 */
Node *build_node_chain(int count, int start_value)
{
    if (count <= 0) {
        return NULL;
    }

    Node *head = NULL;   /* 整条链目前还没有 */
    Node *tail = NULL;   /* 记住"最后一个节点在哪"，新节点直接接在它后面 */

    for (int i = 0; i < count; i++) {
        Node *fresh = malloc(sizeof(Node));   /* 一个节点 = sizeof(Node) 字节 */
        if (fresh == NULL) {
            printf("      [错误] 第 %d 个节点申请失败，已把前面的 %d 个还回去\n",
                   i + 1, node_chain_length(head));
            node_chain_free(head);            /* 别让做了一半的链泄漏 */
            return NULL;
        }
        fresh->val = start_value + i;
        fresh->next = NULL;                   /* 新节点暂时是"尾巴" */

        if (head == NULL) {
            head = fresh;                     /* 第一个节点：既当头像也当尾巴 */
            tail = fresh;
        } else {
            tail->next = fresh;               /* 后面的节点：接在尾巴后面 */
            tail = fresh;                     /* 尾巴往前挪一格 */
        }
    }
    return head;
}

/*
 * 量长度：经典写法 `for (const Node *cur = head; cur != NULL; cur = cur->next)`
 * 为什么不能写成 `head = head->next` 直接动 head？
 *   head 是参数（拷贝），动它虽然改不到调用者，但会让"函数自己的入口地址"丢掉，
 *   后面想再从头走一遍就没路了。用 cur 当前进指针是习惯，也是保险。
 * 参数是 const Node *：我承诺只走一遍看看，不改任何节点，也不 free。
 */
int node_chain_length(const Node *head)
{
    int len = 0;
    const Node *cur = head;          /* cur 跟着往前走，head 原地不动 */
    while (cur != NULL) {
        len++;
        cur = cur->next;             /* 顺着纸条走到下一个节点；NULL 时循环结束 */
    }
    return len;
}

void print_node_chain(const Node *head)
{
    printf("  链表: ");
    const Node *cur = head;
    while (cur != NULL) {
        if (cur->next == NULL) {
            printf("[%d|尾]", cur->val);   /* 尾节点的 next 是 NULL，不再画箭头 */
        } else {
            printf("[%d|%p] -> ", cur->val, (void *)cur->next);
        }
        cur = cur->next;
    }
    if (head == NULL) {
        printf("(空链)");
    }
    printf("   （共 %d 个节点）\n", node_chain_length(head));
}

/*
 * 拆链：必须"先记下下一个，再拆当前"
 *
 *   cur = head
 *   第 1 轮: next_to_keep = [10 的 next，即 11]     然后 free [10]   cur = 11
 *   第 2 轮: next_to_keep = 12                      然后 free [11]   cur = 12
 *   第 3 轮: next_to_keep = NULL                    然后 free [12]   cur = NULL → 结束
 *
 * ⚠ 如果顺序写反了 —— 先 free(cur) 再读 cur->next —— 就是从一块已经归还的
 *   内存里读地址，属于悬空指针（ASan 会当场报 heap-use-after-free）。
 *   这是链表里最常见也最阴的一个 bug，现在就把它记住。
 */
void node_chain_free(Node *head)
{
    Node *cur = head;
    while (cur != NULL) {
        Node *next_to_keep = cur->next;   /* 先把纸条抄下来 */
        free(cur);                        /* 再拆当前的 */
        cur = next_to_keep;
    }
}
