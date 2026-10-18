#include <stdio.h>
#include <stdlib.h>
#include "lstack.h"

/*
 * main.c —— 链表栈的四幕演示
 *
 * 编译运行（在项目根目录）：
 *   make one DIR=03-stack/linked-stack
 */

static void scene(const char *title)
{
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n");
}

static void push_and_show(LStack *s, int value)
{
    printf("\n  push(%d)\n", value);
    if (lstack_push(s, value)) {
        lstack_print(s, "现在栈里：");
    }
}

int main(void)
{
    /* ============================================================
       第 1 幕：push 全在头部做 —— 每压一个，head 就换一次
       ============================================================ */
    scene("第 1 幕：链表栈长什么样（栈顶 = 链表头）");

    LStack s;
    lstack_init(&s);
    lstack_print(&s, "刚初始化：");
    printf("    head = %p（NULL 就是空栈），size=%d\n", (void *)s.head, s.size);

    push_and_show(&s, 10);
    push_and_show(&s, 20);
    push_and_show(&s, 30);

    printf("\n  看清楚了吗：10 明明最先压入，现在却被挤到最右边（栈底）。\n");
    printf("  新节点永远插在【左边】，head 每次都被换成刚建的那个节点 —— 这就是「头部压栈」。\n");

    int top_value = 0;
    if (lstack_peek(&s, &top_value)) {
        printf("\n  peek() 只读 head 那个节点：栈顶 = %d，链表一根指针都没动\n", top_value);
        lstack_print(&s, "peek 之后：");
    }

    int popped = 0;
    printf("\n  pop()：摘掉 head 节点，free 掉它，head 跳到 next\n");
    if (lstack_pop(&s, &popped)) {
        printf("    弹出 %d（顺便把它 malloc 的那块内存 free 了）\n", popped);
        lstack_print(&s, "现在栈里：");
    }

    printf("\n  把所有元素弹光，看弹出顺序：");
    while (!lstack_is_empty(&s)) {
        int v = 0;
        lstack_pop(&s, &v);
        printf("%d ", v);
    }
    printf("← 后压入的先弹出（LIFO）\n");
    lstack_print(&s, "现在栈里：");

    /* ============================================================
       第 2 幕：为什么必须头部 push —— 尾部 push 到底多慢（现场数步数）
       ============================================================ */
    scene("第 2 幕：头部 push 是 O(1)，尾部 push 是 O(n) —— 用步数实测");

    LStack tail_try;
    lstack_init(&tail_try);

    printf("\n  【头部压栈】（正式做法）：只改 2 个指针，跟栈里已有几个元素完全无关\n");
    for (int i = 1; i <= 5; i++) {
        lstack_push(&tail_try, i);
    }
    lstack_print(&tail_try, "  压 5 个之后：");
    printf("  每次 push 的代价恒定 = 1 次 malloc + 2 行指针赋值 → O(1)\n");

    printf("\n  【尾部压栈】（反面做法）：必须先从头走到尾找最后一个节点，走几步打印几步\n");
    LStack bad;
    lstack_init(&bad);
    for (int i = 1; i <= 5; i++) {
        int steps = lstack_push_at_tail_count_steps(&bad, i);
        printf("  第 %d 次尾部 push → 访问了 %d 个节点才找到尾巴，size=%d\n", i, steps, bad.size);
    }
    printf("  步数 0、1、2、3、4 一路涨：压 n 个元素总共 0+1+2+...+(n-1) ≈ n²/2 步 → O(n²)\n");
    printf("  尾部 pop 更糟：单链表不能倒退，删尾巴还得先找「倒数第二个」，照样 O(n)。\n");
    printf("  所以栈顶只能放在【头部】。（真想两端都快 → 双向链表，或 04-queue/linked-queue 的头出尾进）\n");

    lstack_destroy(&tail_try);
    lstack_destroy(&bad);   /* 尾部压进去的 5 个节点也要还回去，否则 ASan 报泄漏 */

    /* ============================================================
       第 3 幕：头部操作会「改掉 head 本身」，所以要么传 LNode**，要么把 head 包进结构体
       ============================================================ */
    scene("第 3 幕：想改到 head 这个指针本身，该怎么办（本节的 C 语言重点）");

    /* --- 反例：只传一级指针 --- */
    printf("\n  --- 反例：void f(LNode *head, LNode *new_node) ---\n");
    LStack box;
    lstack_init(&box);
    lstack_push(&box, 100);
    lstack_print(&box, "外面原本的栈：");

    LNode *fresh = lstack_make_node(999);          /* 调用者造节点，也由调用者负责 free */
    printf("  调用前：外面 box.head = %p\n", (void *)box.head);
    lstack_push_broken(box.head, fresh);            /* 传的是「head 里存的地址」的一份拷贝 */
    printf("  调用后：外面 box.head = %p   ← 一模一样，新节点根本没接上\n", (void *)box.head);
    printf("  原因：C 传参永远是拷贝。函数里的 head 是另一变量，它指向哪、被改成谁，\n");
    printf("        都影响不到 box.head。想改「指针本身」，就得传「指针的地址」。\n");
    lstack_print(&box, "外面的栈还是：");
    free(fresh);                                     /* 反例里没接上链，谁造的就谁来还 */
    fresh = NULL;
    lstack_destroy(&box);

    /* --- 正统 A：结构体把 head 包起来（本项目采用的写法）--- */
    printf("\n  --- 正统写法 A：把 head 装进 LStack 结构体，传 LStack *（本项目采用）---\n");
    LStack good;
    lstack_init(&good);
    printf("  box 本身在栈上的地址 = %p，里面的 head 字段地址 = %p（两者其实同一个位置）\n",
           (void *)&good, (void *)&good.head);
    printf("  函数收到的是 LStack *s，`s->head = new_node;` 就是「往那个字段里写」→ 真能改到外面\n");
    lstack_push(&good, 1);
    lstack_push(&good, 2);
    lstack_print(&good, "结构体版压栈后：");

    /* --- 正统 B：二级指针（不包结构体时的标准签名）--- */
    printf("\n  --- 正统写法 B：传二级指针 LNode **head（写算法题/函数式风格时常见）---\n");
    LNode *bare_head = NULL;   /* 这次没有盒子，只有一个裸的 LNode* 变量 */
    int bare_size = 0;
    printf("  调用前 bare_head = %p\n", (void *)bare_head);
    lstack_push_double_pointer(&bare_head, 7, &bare_size);   /* ★ 传 &head：head 自己的地址 */
    lstack_push_double_pointer(&bare_head, 8, &bare_size);
    printf("  两次 push 之后 bare_head = %p, bare_size = %d   ← 变量真的被改到了\n",
           (void *)bare_head, bare_size);
    printf("  链：head → ");
    for (LNode *cur = bare_head; cur != NULL; cur = cur->next) {
        printf("[%d] → ", cur->val);
    }
    printf("NULL\n");
    printf("  两种写法对比：\n");
    printf("    A 结构体包 head：签名干净（LStack *），还能顺手放 size，本节选它；\n");
    printf("    B 二级指针 LNode **：不用结构体，但要自己带 size、每处都要写 *head，容易漏括号。\n");

    /* 手动的收尾：裸链表没人帮我 destroy，自己一路 free */
    int bare_freed = 0;
    while (bare_head != NULL) {
        LNode *nx = bare_head->next;   /* 先备份 next，再 free（顺序反了就是 use-after-free）*/
        free(bare_head);
        bare_head = nx;
        bare_freed++;
    }
    printf("  手工 free 掉 %d 个节点，bare_head 已置 %p\n", bare_freed, (void *)bare_head);

    /* ============================================================
       第 4 幕：边界 —— 空栈 pop / 空栈 peek / push 后又能 pop
       ============================================================ */
    scene("第 4 幕：边界情况");

    lstack_print(&good, "good 现在：");
    int v = 0;
    printf("\n  pop 掉剩下的元素：\n");
    while (lstack_pop(&good, &v)) {
        lstack_print(&good, "    弹出后：");
    }
    printf("  空栈上再 pop 一次（上面 [失败] 就是它的输出，程序不会崩）\n");

    printf("\n  空栈 peek：\n");
    int dummy = 0;
    if (!lstack_peek(&good, &dummy)) {
        printf("    → false，dummy 仍是 %d，没被写脏\n", dummy);
    }

    printf("\n  空栈上再 push(42)，栈可以无限复活（不像数组栈有容量天花板）：\n");
    push_and_show(&good, 42);

    /* ============================================================
       收尾
       ============================================================ */
    scene("收尾：destroy 会把还没弹走的节点全部 free");

    lstack_push(&good, 43);
    lstack_push(&good, 44);
    lstack_print(&good, "残留着 3 个节点的栈：");
    lstack_destroy(&good);
    lstack_print(&good, "destroy 之后：");

    lstack_destroy(&s);   /* 第 1 幕那个栈当时已弹空，但照样要 destroy 才算成对写完整：
                             养成 init / destroy 成对出现的习惯，比每次去数「还剩几个」可靠 */
    printf("  第 1 幕的 s 也 destroy 了一遍（当时已空，free 掉 0 个节点）\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住四句话：\n");
    printf("  1) 栈顶放【链表头部】：push/pop 都是 O(1)；放尾部单链表就是 O(n)。\n");
    printf("  2) push 两行顺序不能反：先 new->next = head; 再 head = new;（反了就自环）。\n");
    printf("  3) pop/destroy 必须先存好 next，再 free 当前（free 后读 next = use-after-free）。\n");
    printf("  4) 改 head 本身要么传 LNode **，要么把 head 装进结构体传 LStack *（本节选后者）。\n");

    return 0;
}
