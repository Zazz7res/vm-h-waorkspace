#include <stdio.h>
#include <stdlib.h>
#include "dlist.h"

/* 小工具：分幕标题（和 pointer-basics / singly 那节同款，保持手感一致） */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/* 小工具：换一条干净的链表来演示下一幕。
 * ⚠ 必须先 dlist_free 再 dlist_init！
 * 只调 dlist_init 的话，head/tail 直接被置成 NULL，上一幕那些还挂在链上的节点
 * 就【谁也找不到谁】了 —— 没有任何指针指向它们，也永远 free 不掉 = 内存泄漏。
 * 这类"覆盖把手造成的孤儿"是链表最隐蔽的泄漏来源，ASan 会把它报成 Indirect leak。 */
static void reset(DList *list)
{
    dlist_free(list);      /* 先沿着 head 一路 free 干净，顺手把三个字段复位 */
    dlist_init(list);      /* 再正式初始化一次（其实上一步已经复位了，这里是为了读起来清楚） */
}

int main(void)
{
    DList list;      /* 栈上的"把手"：里面就三个字段 head / tail / size */

    /* ============================================================
       第 1 幕：空表是什么样子
       ============================================================ */
    section("1. 空表：head、tail 都是 NULL，size 是 0（三个字段必须同时成立）");

    dlist_init(&list);
    printf("  正向看: ");
    dlist_print_forward(&list);
    printf("  反向看: ");
    dlist_print_backward(&list);
    printf("  size = %d\n", dlist_size(&list));
    printf("  空表能删吗？试着弹一个：\n");
    int ghost = -1;
    int popped = dlist_pop_front(&list, &ghost);
    printf("  pop_front 返回 %d（0=空表弹不出），ghost 仍是 %d\n", popped, ghost);

    /* ============================================================
       第 2 幕：头插 & 尾插都 O(1)，因为 head / tail 两个把手都在手里
       ============================================================ */
    section("2. 头插 O(1) + 尾插 O(1)：单链表只有 head，尾插必须走一整趟");

    dlist_push_back(&list, 20);       /* 空表插第一个：head 和 tail 同时指它 */
    printf("  尾插 20（空表→单节点）: ");
    dlist_print_forward(&list);
    printf("    此时 head 和 tail 指的是同一个节点\n");

    dlist_push_back(&list, 30);
    printf("  尾插 30:                 ");
    dlist_print_forward(&list);
    dlist_push_front(&list, 10);
    printf("  头插 10:                 ");
    dlist_print_forward(&list);
    dlist_push_front(&list, 5);
    printf("  头插 5:                  ");
    dlist_print_forward(&list);
    dlist_push_back(&list, 99);
    printf("  尾插 99:                 ");
    dlist_print_forward(&list);
    printf("  size = %d（是每次增删维护出来的账，不用现场遍历）\n", dlist_size(&list));

    printf("\n  ★ 反向遍历（从 tail 出发一路 prev 往回走）：\n  ");
    dlist_print_backward(&list);
    printf("  → 打印出来是倒序的，这就是 prev 那根线真的接好了的铁证。\n");
    printf("    单链表想反着走？只能从头再遍历一遍，或者先把值全存进数组。\n");

    /* ============================================================
       第 3 幕：在指定节点前 / 后插入
       ============================================================ */
    section("3. 在指定节点前后插入（前提：手里已经有那个节点的地址）");

    Node *pos = dlist_find(&list, 30);
    printf("  当前链表:                ");
    dlist_print_forward(&list);
    if (pos != NULL) {
        printf("  find(30) 命中，在 30 【后面】插 35：\n");
        dlist_insert_after(&list, pos, 35);
        printf("  结果:                  ");
        dlist_print_forward(&list);
        printf("  在 30 【前面】插 25：\n");
        dlist_insert_before(&list, pos, 25);
        printf("  结果:                  ");
        dlist_print_forward(&list);
        printf("    （注意：pos 一直是同一个节点，插完前插后 30 还在原位，两根新线各自挂上）\n");
    }

    printf("\n  在【头节点】前面插 = 头插的效果：\n");
    dlist_insert_before(&list, list.head, 1);
    printf("  结果:                  ");
    dlist_print_forward(&list);
    printf("  在【尾节点】后面插 = 尾插的效果（tail 把手会自动挪到新节点）：\n");
    dlist_insert_after(&list, list.tail, 200);
    printf("  结果:                  ");
    dlist_print_forward(&list);
    printf("  验证 tail 确实跟着挪了：tail->val = %d，head->val = %d\n", list.tail->val, list.head->val);

    printf("\n  故意传 pos = NULL 试试：\n");
    int bad = dlist_insert_after(&list, NULL, 777);
    printf("  返回值 = %d（拒绝执行），链表不变: ", bad);
    dlist_print_forward(&list);

    /* ============================================================
       第 4 幕：删【已知节点】是 O(1) —— 双向链表最大的卖点
       ============================================================ */
    section("4. 删除：双向链表删一个已知节点是 O(1)，单链表同一步是 O(n)");

    printf("  ★ 为什么单链表删节点非 O(n) 不可？\n");
    printf("    要摘掉 X 必须改【X 前驱】的 next，而单链表节点不知道前驱是谁，只能从 head 走一趟。\n");
    printf("  ★ 双向链表：X->prev 就写在节点自己身上，前驱 O(1) 到手，改 4 根线就完事。\n");
    printf("    （代价：每个节点多存一个指针，16 字节 → 24 字节）\n\n");

    printf("  起始:                  ");
    dlist_print_forward(&list);

    Node *mid = dlist_find(&list, 30);
    if (mid != NULL) {
        printf("  删【中间节点 30】(dlist_remove，找它才是 O(n)，动手 O(1)):\n");
        dlist_remove(&list, mid);
        mid = NULL;                       /* ★ 它已经被 free 了，地址必须作废，否则就是悬空指针 */
        printf("  结果:                  ");
        dlist_print_forward(&list);
    }

    printf("  删【头节点】(head 自动更新):\n");
    dlist_remove(&list, list.head);
    printf("  结果:                  ");
    dlist_print_forward(&list);

    printf("  删【尾节点】(tail 自动更新):\n");
    dlist_remove(&list, list.tail);
    printf("  结果:                  ");
    dlist_print_forward(&list);
    printf("  现在 head->val = %d, tail->val = %d, size = %d\n",
           list.head->val, list.tail->val, dlist_size(&list));

    printf("\n  故意传 node = NULL：\n");
    dlist_remove(&list, NULL);
    printf("  链表没被破坏:          ");
    dlist_print_forward(&list);

    /* ============================================================
       第 5 幕：按值删 + 两头弹出
       ============================================================ */
    section("5. 按值删除（找 O(n) + 删 O(1)）与 pop_front / pop_back");

    reset(&list);                       /* 先 free 掉上一幕的节点，再重来一条干净的 */
    printf("  （这一幕开头调了 reset()：先 dlist_free 再 dlist_init。"
           "只 init 不 free 就会把上一幕的节点全变成找不回来的孤儿 → 内存泄漏）\n");
    dlist_push_back(&list, 1);
    dlist_push_back(&list, 2);
    dlist_push_back(&list, 3);
    dlist_push_back(&list, 2);
    dlist_push_back(&list, 4);
    printf("  起始（有重复值 2）:    ");
    dlist_print_forward(&list);

    printf("  按值删 2（只删最靠前那个）:\n");
    dlist_remove_by_val(&list, 2);
    printf("  结果:                  ");
    dlist_print_forward(&list);

    printf("  按值删 99（不存在）:\n");
    int ok = dlist_remove_by_val(&list, 99);
    printf("  返回值 = %d，链表不变: ", ok);
    dlist_print_forward(&list);

    int v = 0;
    dlist_pop_front(&list, &v);
    printf("  pop_front 弹出 %d:     ", v);
    dlist_print_forward(&list);
    dlist_pop_back(&list, &v);
    printf("  pop_back  弹出 %d:     ", v);
    dlist_print_forward(&list);
    dlist_pop_back(&list, NULL);
    printf("  pop_back 弹出不取值:   ");
    dlist_print_forward(&list);
    printf("  size = %d\n", dlist_size(&list));

    printf("\n  一直弹到空：\n");
    while (dlist_pop_back(&list, &v)) {
        printf("    弹出 %d → ", v);
        dlist_print_forward(&list);
    }
    printf("  空表时 head = %s, tail = %s, size = %d\n",
           list.head == NULL ? "NULL" : "非 NULL",
           list.tail == NULL ? "NULL" : "非 NULL",
           dlist_size(&list));

    /* ============================================================
       第 6 幕：两个方向查找，验证"双向"确实双向
       ============================================================ */
    section("6. 两个方向都能找：find 从头走 / find_from_tail 从尾走");

    reset(&list);
    for (int i = 1; i <= 6; i++) {
        dlist_push_back(&list, i * 10);
    }
    printf("  链表:                  ");
    dlist_print_forward(&list);
    Node *f1 = dlist_find(&list, 50);
    printf("  find(50)      从头往后找: %s\n", f1 != NULL ? "命中" : "未命中");
    Node *f2 = dlist_find_from_tail(&list, 50);
    printf("  find_from_tail 从尾往前找: %s\n", f2 != NULL ? "命中" : "未命中");
    printf("  → 两个函数返回的是【同一个节点】：%s\n", (f1 == f2) ? "指针相等，正确" : "竟然不等，错了");
    printf("  找 50 从头要走 5 步、从尾只要 2 步 —— 目标在后半段时，反向查找更划算\n");
    Node *f3 = dlist_find(&list, 55);
    printf("  find(55) 未命中:        %s\n", f3 == NULL ? "返回 NULL" : "不可能");
    Node *f4 = dlist_find_from_tail(&list, 55);
    printf("  find_from_tail(55):     %s\n", f4 == NULL ? "返回 NULL" : "不可能");

    printf("\n  最后再看一次正向 & 反向：\n  ");
    dlist_print_forward(&list);
    printf("  ");
    dlist_print_backward(&list);

    /* ============================================================
       收尾：必须 free，否则 ASan 会报泄漏
       ============================================================ */
    dlist_free(&list);
    printf("\n  dlist_free 之后:        ");
    dlist_print_forward(&list);
    printf("  再 free 一次（head 已是 NULL，free(NULL) 合法）：安全，不会 double free\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住三句话：\n");
    printf("  1) 多一根 prev 的钱，买到的是『删/插已知节点 O(1)』和『能倒着走』。\n");
    printf("  2) 有 head 和 tail 两个把手，两头插、两头弹才是真 O(1)；单链表尾插必须走一趟。\n");
    printf("  3) 删节点的四种情况（普通/头/尾/唯一）被『node->prev 是不是 NULL』两支代码包干，\n");
    printf("     不用像单链表那样造 dummy —— NULL 就是天然的哨兵。\n");
    return 0;
}
