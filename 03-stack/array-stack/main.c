#include <stdio.h>
#include <stdlib.h>
#include "astack.h"

/*
 * main.c —— 数组栈的四幕演示
 *
 * 每一幕之后都会把栈里所有元素打印出来，所以你能看见
 * 「栈底在左、栈顶在右、top 指针一路往上爬」这件事。
 *
 * 编译运行（在项目根目录）：
 *   make one DIR=03-stack/array-stack
 */

/* 打印一条分隔线 + 标题，让输出好读 */
static void scene(const char *title)
{
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n");
}

/* push 一个值并把栈打出来 —— demo 里几乎每步都要，抽成小函数少写重复代码 */
static void push_and_show(AStack *s, int value)
{
    printf("\n  push(%d)\n", value);
    if (astack_push(s, value)) {
        astack_print(s, "现在栈里：");
    } else {
        printf("    → 这次 push 没成功，上面的 [失败] 就是原因\n");
    }
}

/* pop 一个值、把弹出来的东西显示出来、再打印栈 */
static void pop_and_show(AStack *s)
{
    int value = 0;   /* 故意先放个 0，方便看出「是不是真被写进去了」 */
    printf("\n  pop()\n");
    if (astack_pop(s, &value)) {
        printf("    弹出的是 %d\n", value);
        astack_print(s, "现在栈里：");
    } else {
        printf("    → 这次 pop 没成功（栈空），见上面的 [失败]\n");
    }
}

int main(void)
{
    /* ============================================================
       第 1 幕：固定容量的栈（auto_grow = false）
       目标：看清 top 怎么一格一格往上爬、以及「栈满了只能报错」
       ============================================================ */
    scene("第 1 幕：固定容量 capacity=4，不开扩容");

    AStack fixed;
    astack_init(&fixed, 4, false);
    astack_print(&fixed, "刚初始化：");
    printf("    注意 top=-1 —— 这就是「流派 A」表示空栈的方式；元素个数 = top+1 = 0\n");

    push_and_show(&fixed, 10);
    push_and_show(&fixed, 20);
    push_and_show(&fixed, 30);

    printf("\n  peek() 只看不拿：\n");
    int top_value = 0;
    if (astack_peek(&fixed, &top_value)) {
        printf("    栈顶是 %d（data[%d]），栈大小没变\n", top_value, fixed.top);
        astack_print(&fixed, "peek 之后栈里还是：");
    }

    pop_and_show(&fixed);

    push_and_show(&fixed, 40);
    push_and_show(&fixed, 50);   /* 这一步之后 top == capacity-1，满了 */

    printf("\n  现在再硬塞一个 60 看看（固定容量栈的经典结局）：\n");
    push_and_show(&fixed, 60);
    printf("    → 数组就 4 格，不扩容就只能报错。要么把栈开大点，要么用下一幕的动态扩容。\n");

    /* ============================================================
       第 2 幕：动态扩容（auto_grow = true）
       目标：亲眼看 capacity 2→4→8→16 一路翻倍，并数清扩容次数
       ============================================================ */
    scene("第 2 幕：从 capacity=2 起步，开自动扩容（满了就翻倍）");

    AStack dyn;
    astack_init(&dyn, 2, true);
    astack_print(&dyn, "刚初始化：");

    /* 压 9 个数：2 → 4 → 8 → 16，应该正好扩容 3 次 */
    for (int v = 1; v <= 9; v++) {
        push_and_show(&dyn, v * 100);
    }

    printf("\n  小结：压了 9 个元素，只扩容 3 次（不是 9 次）。\n");
    printf("    搬旧数据的次数依次是 2、4、8 次，总共 14 次 ≈ 2n，均摊到每个 push 上就是常数。\n");
    printf("    代价：capacity=16 但 size=9，浪费了 7 个格子 —— 空间换时间，最多多浪费一倍。\n");
    printf("    dyn.grow_count = %d, 现在 capacity=%d, size=%d\n",
           dyn.grow_count, astack_capacity(&dyn), astack_size(&dyn));

    /* ============================================================
       第 3 幕：后进先出（LIFO）到底长什么样
       目标：把 1、2、3 压进去，再全弹出来，出来的顺序必须是 3、2、1
       ============================================================ */
    scene("第 3 幕：后进先出 —— 压入 1 2 3，弹出顺序一定是 3 2 1");

    AStack lifo;
    astack_init(&lifo, 4, true);

    printf("  依次 push 1、2、3（想象往桌上摞盘子）：\n");
    push_and_show(&lifo, 1);
    push_and_show(&lifo, 2);
    push_and_show(&lifo, 3);

    printf("\n  现在全部弹光（最先弹出的必须是最晚压入的 3）：\n");
    printf("  弹出顺序：");
    while (!astack_is_empty(&lifo)) {
        int v = 0;
        astack_pop(&lifo, &v);
        printf("%d ", v);
    }
    printf("\n");
    printf("  看到没：压入 1→2→3，弹出 3→2→1。栈的全部灵魂就这一句话。\n");
    printf("  真实用途：浏览器后退、Ctrl+Z 撤销、函数调用栈、DFS、表达式求值。\n");

    /* ============================================================
       第 4 幕：边界情况 —— 空栈 pop / 空栈 peek / 重复销毁
       ============================================================ */
    scene("第 4 幕：边界情况都安全（该报错的报错，不该崩的绝不崩）");

    printf("  此时 lifo 已经是空栈，top=%d\n", lifo.top);
    astack_print(&lifo, "空栈：");

    pop_and_show(&lifo);                 /* 空栈 pop */

    printf("\n  空栈 peek：\n");
    int dummy = 0;
    if (!astack_peek(&lifo, &dummy)) {
        printf("    → 返回 false，dummy 保持原值 %d，没被写脏\n", dummy);
    }

    printf("\n  空栈上再 push 一个 7（栈是可以「复活」的，top 从 -1 变 0）：\n");
    push_and_show(&lifo, 7);

    printf("\n  非法初始化（capacity=0）会怎样：\n");
    AStack broken;
    astack_init(&broken, 0, true);
    push_and_show(&broken, 1);           /* 应该失败，且绝不解引用空指针 */

    /* ============================================================
       收尾：谁 malloc 谁 free
       ============================================================ */
    scene("收尾：每个 init 出来的栈都要 destroy，否则 ASan 会报内存泄漏");

    astack_destroy(&fixed);
    printf("  fixed  已 destroy：data=%p, top=%d（再误用会立刻崩，而不是悄悄算错）\n",
           (void *)fixed.data, fixed.top);
    astack_destroy(&dyn);
    printf("  dyn    已 destroy\n");
    astack_destroy(&lifo);
    printf("  lifo   已 destroy\n");
    astack_destroy(&broken);
    printf("  broken 已 destroy（本来就没申请内存，free(NULL) 是合法空操作）\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住三句话：\n");
    printf("  1) 栈 = 数组 + 一个 top 指针；push/pop 只动 top，所以永远 O(1)。\n");
    printf("  2) top=-1 表示空（流派 A）；pop 只是把 top 往下挪，格子里的值没人清。\n");
    printf("  3) 固定容量栈满了只能报错；翻倍扩容把它变成摊还 O(1)，代价是最多多一倍空间。\n");

    return 0;
}
