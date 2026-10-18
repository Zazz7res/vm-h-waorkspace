#include <stdio.h>
#include <stdlib.h>
#include "slist.h"

/* 小工具：打印一行分隔线，让输出好读（和 pointer-basics 那节同款） */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/* 小工具：打印当前长度，省得每幕都写两行 */
static void show_len(Node *head)
{
    printf("  长度 = %d\n", slist_length(head));
}

int main(void)
{
    /* ============================================================
       第 1 幕：指针传参的坑 —— 为什么链表函数全是 Node **head
       ============================================================ */
    section("1. 为什么插入函数要写成 Node **head（传地址才改得到外面）");

    Node *head = NULL;                    /* 空表：头指针什么都不指 */
    printf("  初始状态（空表）: ");
    slist_print(head);

    printf("  先试【错误写法】slist_push_front_bad(9)，只传一层指针：\n");
    slist_push_front_bad(head, 9);        /* 故意传 Node*，函数改不到外面的 head */
    printf("  调用后 main 里的 head: ");
    slist_print(head);                    /* ← 9 根本没进来，节点成了孤儿（函数里已帮忙 free） */
    printf("  ↑ 这就是 pointer-basics 里 swap_by_value 的链表版：\n");
    printf("    函数拿到的 head 是一份拷贝，改拷贝等于白改。\n");

    printf("\n  再用【正确写法】slist_push_front(&head, 9)，把 head 的地址传进去：\n");
    slist_push_front(&head, 9);
    printf("  调用后 main 里的 head: ");
    slist_print(head);                    /* ← 9 真的在最前面了 */
    printf("\n  继续头插 7、5（注意头插是【后进先来】，最后插的在最前面）：\n");
    slist_push_front(&head, 7);
    printf("  头插 7 之后: ");
    slist_print(head);
    slist_push_front(&head, 5);
    printf("  头插 5 之后: ");
    slist_print(head);

    slist_free(&head);
    printf("  收尾释放完毕，main 里的 head 已被置成 NULL（不留悬空指针）\n");

    /* ============================================================
       第 2 幕：头插 vs 尾插
       ============================================================ */
    section("2. 头插 O(1) vs 尾插 O(n)");

    head = NULL;
    printf("  依次头插 1 2 3：\n");
    slist_push_front(&head, 1);
    slist_push_front(&head, 2);
    slist_push_front(&head, 3);
    printf("  结果: ");
    slist_print(head);
    printf("  → 头插是『后进来排在前面』，顺序反了；想保持插入顺序就得用尾插。\n");
    slist_free(&head);

    head = NULL;
    printf("\n  依次尾插 1 2 3（每次都要从头走到尾巴，3 次共走 0+1+2 步）：\n");
    slist_push_back(&head, 1);
    printf("  尾插 1: ");
    slist_print(head);
    slist_push_back(&head, 2);
    printf("  尾插 2: ");
    slist_print(head);
    slist_push_back(&head, 3);
    printf("  尾插 3: ");
    slist_print(head);
    printf("  → 顺序保住了，代价是每次 O(n)。这就是链表『没有尾巴把手』的痛。\n");
    slist_free(&head);

    /* ============================================================
       第 3 幕：在第 k 个位置插入（含越界保护）
       ============================================================ */
    section("3. 在第 k 个位置插入（下标从 0 起）");

    head = NULL;
    slist_push_back(&head, 10);
    slist_push_back(&head, 20);
    slist_push_back(&head, 30);
    printf("  原始链表: ");
    slist_print(head);

    slist_insert_at(&head, 0, 99);        /* 头插位置 */
    printf("  插到 k=0（最前面）放 99: ");
    slist_print(head);

    slist_insert_at(&head, 2, 55);        /* 中间位置 */
    printf("  插到 k=2 放 55: ");
    slist_print(head);

    int len = slist_length(head);
    slist_insert_at(&head, len, 77);      /* k == 长度 = 尾插 */
    printf("  插到 k=%d（= 长度，等于尾插）放 77: ", len);
    slist_print(head);

    printf("  故意插到越界位置 k=100：\n");
    int ok = slist_insert_at(&head, 100, 888);
    printf("    返回值 = %d（0 = 失败），链表保持原样: ", ok);
    slist_print(head);
    printf("  故意插到 k=-1：\n");
    ok = slist_insert_at(&head, -1, 888);
    printf("    返回值 = %d（0 = 失败）\n", ok);

    slist_free(&head);

    /* ============================================================
       第 4 幕：按值查找 / 按下标取值
       ============================================================ */
    section("4. 按值查找 & 按下标取值（都是 O(n)，链表没有随机访问）");

    head = NULL;
    slist_push_back(&head, 5);
    slist_push_back(&head, 8);
    slist_push_back(&head, 13);
    slist_push_back(&head, 21);
    printf("  链表: ");
    slist_print(head);

    Node *hit = slist_find(head, 13);
    if (hit != NULL) {
        printf("  find(13): 找到了，返回的节点 val = %d（拿到地址就能直接删它）\n", hit->val);
    } else {
        printf("  find(13): 没找到\n");
    }
    Node *miss = slist_find(head, 100);
    printf("  find(100): %s\n", miss != NULL ? "找到了" : "没找到（返回 NULL）");

    int v = -1;
    if (slist_get_at(head, 2, &v)) {
        printf("  get_at(2) = %d（想拿第 3 个，必须从第 1 个开始走 2 步）\n", v);
    }
    if (!slist_get_at(head, 99, &v)) {
        printf("  get_at(99): 下标越界，返回 0；v 仍是调用前的 %d（函数没往里写）\n", v);
    }
    show_len(head);

    slist_free(&head);

    /* ============================================================
       第 5 幕：按值删除 —— 哨兵写法 vs 土办法，两种代码行为必须一致
       ============================================================ */
    section("5. 按值删除：哨兵 dummy 统一『删头』和『删中间』");

    printf("  ★ 难点回顾：单链表节点不知道自己的前驱是谁，\n");
    printf("    所以要删一个节点，得改【它前驱】的 next；\n");
    printf("    可头节点没有前驱，只能改 head 变量本身 —— 这就冒出了两套代码。\n");
    printf("    造一个不存数据的假头 dummy 挂到最前面，头节点也就有了前驱，两套代码合成一套。\n");

    /* ---- 5A：土办法（特判删头 + 循环删中间） ---- */
    printf("\n  【土办法 slist_remove_val_two_cases】分两种情况写：\n");
    head = NULL;
    slist_push_back(&head, 1);
    slist_push_back(&head, 2);
    slist_push_back(&head, 3);
    slist_push_back(&head, 4);
    printf("    起始:               ");
    slist_print(head);
    slist_remove_val_two_cases(&head, 1);
    printf("    删 1（头节点）:      ");
    slist_print(head);
    slist_remove_val_two_cases(&head, 3);
    printf("    删 3（中间节点）:    ");
    slist_print(head);
    slist_remove_val_two_cases(&head, 4);
    printf("    删 4（尾巴节点）:    ");
    slist_print(head);
    slist_remove_val_two_cases(&head, 99);
    printf("    删 99（不存在）:     ");
    slist_print(head);
    slist_remove_val_two_cases(&head, 2);
    printf("    删 2（最后一个）:    ");
    slist_print(head);
    printf("    现在链表是空的吗？   ");
    slist_print(head);
    slist_free(&head);

    /* ---- 5B：哨兵写法（一套代码走天下） ---- */
    printf("\n  【哨兵写法 slist_remove_val】dummy 挂前，一路 prev/cur：\n");
    head = NULL;
    slist_push_back(&head, 1);
    slist_push_back(&head, 2);
    slist_push_back(&head, 3);
    slist_push_back(&head, 4);
    printf("    起始:               ");
    slist_print(head);
    printf("    （内部画面：dummy -> 1 -> 2 -> 3 -> 4 -> NULL，每个节点都有前驱了）\n");
    slist_remove_val(&head, 1);
    printf("    删 1（头节点）:      ");
    slist_print(head);
    slist_remove_val(&head, 3);
    printf("    删 3（中间节点）:    ");
    slist_print(head);
    slist_remove_val(&head, 4);
    printf("    删 4（尾巴节点）:    ");
    slist_print(head);
    slist_remove_val(&head, 99);
    printf("    删 99（不存在）:     ");
    slist_print(head);
    slist_remove_val(&head, 2);
    printf("    删 2（最后一个）:    ");
    slist_print(head);
    printf("    现在链表是空的吗？   ");
    slist_print(head);
    show_len(head);
    slist_free(&head);
    printf("  → 两种写法的输出【一模一样】，但哨兵版只有一处 free、零个特判。\n");

    /* ---- 5C：重复值只删最前面那个 ---- */
    head = NULL;
    slist_push_back(&head, 7);
    slist_push_back(&head, 7);
    slist_push_back(&head, 7);
    printf("\n  有重复值时删一次: ");
    slist_print(head);
    slist_remove_val(&head, 7);
    printf("  删掉一个 7 之后:    ");
    slist_print(head);
    slist_free(&head);

    /* ============================================================
       第 6 幕：反转（迭代版 + 递归版）
       ============================================================ */
    section("6. 反转：迭代版 & 递归版");

    head = NULL;
    slist_push_back(&head, 1);
    slist_push_back(&head, 2);
    slist_push_back(&head, 3);
    slist_push_back(&head, 4);
    slist_push_back(&head, 5);
    printf("  反转前（原样）:          ");
    slist_print(head);
    slist_reverse_iter(&head);
    printf("  迭代版反转之后:          ");
    slist_print(head);
    slist_reverse_recursive(&head);
    printf("  再拿递归版反转回来（等于原样）: ");
    slist_print(head);
    slist_reverse_recursive(&head);
    printf("  只用递归版反转一次:      ");
    slist_print(head);
    slist_reverse_iter(&head);
    printf("  再用迭代版反转回来:      ");
    slist_print(head);
    printf("  → 迭代版和递归版互为逆操作？不，反转两次才回到原样；两个版本结果必须相同。\n");

    printf("\n  边界：空表反转\n");
    Node *empty_head = NULL;
    slist_reverse_iter(&empty_head);
    printf("  空表迭代反转后:          ");
    slist_print(empty_head);
    slist_reverse_recursive(&empty_head);
    printf("  空表递归反转后:          ");
    slist_print(empty_head);

    printf("  边界：单节点反转\n");
    empty_head = slist_make_node(42);
    slist_reverse_iter(&empty_head);
    printf("  [42] 迭代反转后:         ");
    slist_print(empty_head);
    slist_reverse_recursive(&empty_head);
    printf("  [42] 递归反转后:         ");
    slist_print(empty_head);
    slist_free(&empty_head);

    printf("\n  ★ 为什么 nxt = cur->next 必须写在最前面？\n");
    printf("    因为下一行 cur->next = prev 要【覆盖】cur->next 这个格子。\n");
    printf("    不先抄住，后半条链就再也找不回来了（既走不到也 free 不掉）。\n");
    printf("    跟 swap 前必须先 int temp = *a; 是同一条铁律：覆盖之前先存旧值。\n");
    slist_free(&head);

    /* ============================================================
       第 7 幕：快慢指针找中间节点
       ============================================================ */
    section("7. 快慢指针找中间节点（一趟搞定，不用先数长度）");

    head = NULL;
    for (int i = 1; i <= 5; i++) {
        slist_push_back(&head, i * 10);          /* 10 20 30 40 50 */
    }
    printf("  链表（奇数个，n=5）:     ");
    slist_print(head);
    Node *mid = slist_middle(head);
    printf("  慢指针停在:             val = %d（第 3 个，正中间）\n", mid->val);
    slist_free(&head);

    head = NULL;
    for (int i = 1; i <= 4; i++) {
        slist_push_back(&head, i * 10);          /* 10 20 30 40 */
    }
    printf("\n  链表（偶数个，n=4）:     ");
    slist_print(head);
    mid = slist_middle(head);
    printf("  慢指针停在:             val = %d（中间两个里【偏右】那个；想拿偏左的要自己记下前一站）\n", mid->val);
    slist_free(&head);

    head = slist_make_node(1);
    printf("\n  链表（只有 1 个）:       ");
    slist_print(head);
    mid = slist_middle(head);
    printf("  慢指针停在:             val = %d\n", mid->val);
    slist_free(&head);

    head = NULL;
    mid = slist_middle(head);
    printf("\n  空表找中间:              %s\n", mid == NULL ? "NULL（安全，没崩）" : "竟然有值？");

    /* ============================================================
       收尾
       ============================================================ */
    printf("\n=== 本节结束 ===\n");
    printf("记住四句话：\n");
    printf("  1) 要改到调用者的 head，就得传 &head（二级指针）；只传 Node* 改的是拷贝。\n");
    printf("  2) 删头节点没有前驱、删中间节点要找前驱 —— 造个 dummy 假头，两件事变一件事。\n");
    printf("  3) 指针要覆盖哪个格子，就先把它旧值抄到 temp/nxt 里存住（反转、释放全靠这句）。\n");
    printf("  4) 链表换来换去都是 O(n) 遍历 + O(1) 改线；想 O(1) 拿到尾/长度，就多存字段。\n");
    return 0;
}
