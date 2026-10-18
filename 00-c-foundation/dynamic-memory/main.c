#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dynamic-memory.h"

/* 小工具：打印一行分隔线，让输出好读（和第 1 节同款） */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/*
 * 命令行用法：
 *   不带参数  -> 干净跑完所有演示，ASan 也必须 0 报错（这是默认，也是 make one / make asan 的跑法）
 *   带任意参数 -> 额外执行第 6 幕【故意泄漏】，专门给 ASan 抓，报告会打红
 * 之所以要这样藏起来：本仓库的规矩是"每个 demo 默认必须干净通过"，
 * 泄漏演示天生不干净，所以让它只在人主动要求时才发生。
 */
int main(int argc, char **argv)
{
    /* ============================================================
       第 1 幕：malloc / calloc / malloc+memset 三兄弟对比
       ============================================================ */
    section("1. 三种申请方式：malloc / calloc / malloc+memset");

    printf("  先看这张表（本机：int=4 字节，指针=8 字节）：\n");
    printf("  ┌──────────┬────────────┬──────────┬────────────────────┐\n");
    printf("  │ 函数     │ 参数单位   │ 初值     │ 什么时候用         │\n");
    printf("  ├──────────┼────────────┼──────────┼────────────────────┤\n");
    printf("  │ malloc   │ 总字节数   │ 垃圾     │ 马上就会自己填全部 │\n");
    printf("  │ calloc   │ 个数+每个  │ 全 0     │ 0 本身就是有效初值 │\n");
    printf("  │ realloc  │ 新总字节数 │ 老数据抄走│ 数组扩容/缩容      │\n");
    printf("  │ free     │ 指针       │ —        │ 归还，谁申请谁还   │\n");
    printf("  └──────────┴────────────┴──────────┴────────────────────┘\n\n");

    int *m1 = arr_new_malloc(5, 42);
    int *c1 = arr_new_calloc(5);
    int *ms1 = arr_new_malloc_memset(5);

    print_int_array("  malloc+自己填  ", m1, 5);
    print_int_array("  calloc          ", c1, 5);
    print_int_array("  malloc+memset   ", ms1, 5);
    printf("  calloc 和 malloc+memset 结果一模一样；差别只在 calloc 顺手做了溢出检查。\n");

    free(m1);  m1 = NULL;    /* 归还 + 立刻置 NULL：本节所有 free 都这么写 */
    free(c1);  c1 = NULL;
    free(ms1); ms1 = NULL;
    printf("  三块都已经 free 并置 NULL（free 之后指针变量还在，但地址已经不作数了）\n");

    /* ============================================================
       第 2 幕：malloc 真的不清零吗？（涂脏 → 归还 → 再申请）
       ============================================================ */
    section("2. malloc 不清零：亲眼看到上一任主人留下的垃圾");

    dirty_block_setup(6, 777);              /* 申请、涂脏、free —— 归还时不擦洗 */
    int *g1 = arr_new_malloc_untouched(6);  /* 再申请同样大小，极可能捡到同一块 */
    print_int_array("  第二次 malloc 的内容（未写过一个字节）", g1, 6);
    printf("  ⚠ 看到 777 还是 0 都不重要，重点是：这些值【不由你决定，不能依赖】。\n");
    printf("    要么自己填（malloc + 循环），要么让它清零（calloc / memset）。\n");
    free(g1); g1 = NULL;

    /* ============================================================
       第 3 幕：realloc 扩容实测 —— 原地扩大 vs 整块搬家
       ============================================================ */
    section("3. realloc：小扩容可能原地，大扩容整块搬走（旧指针当场作废）");

    int *base = arr_new_malloc(4, 0);
    for (int i = 0; i < 4; i++) {
        base[i] = 11 + i;                    /* [11 12 13 14]，好确认搬家后数据有没有跟着走 */
    }
    print_int_array("  扩容前        ", base, 4);

    printf("\n  第 3a 步：从 4 格扩到 8 格（只多 16 字节）\n");
    int *grown = array_resize(base, 8);      /* 一定要接收返回值！ */
    for (int i = 4; i < 8; i++) {
        grown[i] = 100 + i;                  /* 新扩出来的格子是垃圾，必须自己填才能用 */
    }
    print_int_array("  扩到 8 格     ", grown, 8);

    printf("\n  第 3b 步：再从 8 格扩到 20000 格（要多 80000 字节）—— 后面被占了，只能搬家\n");
    int *before_move = grown;                /* 故意留一份"旧地址"，等下用它讲危险 */
    int *big = array_resize(grown, 20000);
    printf("  搬家后前 8 格的内容（realloc 已经帮你把老数据整块抄过来了）：\n");
    print_int_array("  扩到 20000 格  ", big, 8);

    if (before_move == big) {
        printf("\n  这次运气好，原地扩大了，地址没变（不能指望运气，下面按搬家讲）\n");
    } else {
        printf("\n  实测对比：旧地址 %p  ≠  新地址 %p  →  【真的搬走了】\n",
               (void *)before_move, (void *)big);
        printf("  ⚠ 危险：realloc 搬家时会在内部 free 掉旧块。此时 before_move 指向的内存\n");
        printf("     已经不属于你了，读它就是悬空指针。下面这行如果取消注释，ASan 立刻报\n");
        printf("     heap-use-after-free（我们故意注释掉，保证默认运行干净）：\n");
        printf("         /* printf(\"%%d\", before_move[0]); */   ← 千万别这么写\n");
        printf("     而它的值 %p 看上去还像个正经地址，编译器不会拦你 —— 所以最阴。\n",
               (void *)before_move);
        printf("  正确姿势只有两条：\n");
        printf("     1) 永远写成 p = realloc(p, bytes); 用返回值覆盖自己；\n");
        printf("     2) 申请失败时（返回 NULL）别把 NULL 赋回 p —— 老的还有效，要用临时指针接。\n");
    }

    /* free 只能 free 现在这一块。如果这里多写一句 free(before_move)，就是"释放不是你的内存" */
    printf("  free 之前先记下：当前合法地址 = %p，那个作废的旧地址 = %p\n",
           (void *)big, (void *)before_move);
    free(big); big = NULL;
    printf("  只 free 了合法地址；旧地址一个字节都没碰 —— free(旧地址) 就是非法释放，程序当场 abort\n");

    /* ============================================================
       第 4 幕：二级指针 Node ** 为什么是必需的（正反两幕）
       ============================================================ */
    section("4. 函数里插节点：一级指针（失败）vs 二级指针（成功）");

    Node *head = build_list(3, 10);
    print_list("\n  main 里的链表", head);
    printf("  关键信息：main 里那个 head 变量自己住在 %p，里面存的值 = %p\n",
           (void *)&head, (void *)head);

    printf("\n  【第 4a 幕 / 反面】void push_front_broken(Node *head, int val)  —— 传的是 head 本身\n");
    printf("  调用前 main 里 head = %p\n", (void *)head);
    push_front_broken(head, 99);
    printf("  调用后 main 里 head = %p   ← 没变，新节点根本没进来（函数里的成功是拷贝上的成功）\n",
           (void *)head);
    print_list("  main 里的链表", head);

    printf("\n  【第 4b 幕 / 正面】void push_front_ok(Node **head, int val)  —— 传的是 &head\n");
    printf("  调用前 main 里 head = %p\n", (void *)head);
    push_front_ok(&head, 99);               /* &head 的类型是 Node **，正好匹配 */
    printf("  调用后 main 里 head = %p   ← 变了！新节点真的成了头\n", (void *)head);
    print_list("  main 里的链表", head);
    printf("  内存图：\n");
    printf("    head(在 %p) ──> [99](在 %p) ──> [10] ──> [11] ──> [12] ──> 尾\n",
           (void *)&head, (void *)head);
    printf("    函数里做的两件事：fresh->next = *head;  然后  *head = fresh;\n");
    printf("    第二句的星号 = 往 %p 那个格子里写，也就是直接改 main 的 head。\n", (void *)&head);

    printf("\n  【第 4c 幕 / 更狠的例子】把链弹空：头指针必须能变成 NULL\n");
    int popped = -1;
    while (pop_front_ok(&head, &popped) == 0) {
        printf("  弹出 %d，此刻 main 里 head = %p\n", popped, (void *)head);
    }
    printf("  head == NULL ? %s  ← 一级指针版本永远做不到这一步，main 会留一个悬空头指针\n",
           (head == NULL) ? "是（链已空，所有节点都已归还）" : "否");

    /* ============================================================
       第 5 幕：free 的三条铁律
       ============================================================ */
    section("5. free 的三条铁律（背下来）");

    printf("  1) 谁 malloc 谁 free，一次且仅一次。\n");
    printf("     double free 会破坏堆管理器：free(p); free(p); -> 程序直接 abort。\n");
    printf("  2) free 之后立刻置 NULL。free(NULL) 是合法的空操作，\n");
    printf("     所以置 NULL 同时挡住了“误用”和“重删”两个坑。\n");
    printf("  3) 只能 free malloc/calloc/realloc【当前】返回的地址。\n");
    printf("     不能 free 数组中间的某个元素（arr+3），不能 free 栈上变量的地址，\n");
    printf("     搬家之后也不能 free 旧地址（第 3 幕那个 before_move）。\n");

    int *null_check = NULL;
    free(null_check);                        /* 合法：free(NULL) 什么都不做 */
    printf("  实测：free(NULL) 不报错、不退出，确实是个安全的空操作。\n");

    /* ============================================================
       第 6 幕：故意泄漏（默认不跑！）
       ============================================================ */
    if (argc > 1) {
        section("6. 【泄漏演示】你传了命令行参数，所以现在故意漏两块内存");
        printf("  触发的参数：argv[1] = %s\n", argv[1]);
        leak_demo();
        printf("  （程序正常退出；带 ASan 编译时，退出瞬间会打印 leaked in 2 allocation(s)）\n");
    } else {
        section("6. 【泄漏演示】默认跳过");
        printf("  想亲眼看 ASan 抓泄漏，请这样跑：\n");
        printf("    gcc -Wall -Wextra -Wshadow -std=c11 -g -fsanitize=address -static-libasan \\\n");
        printf("        00-c-foundation/dynamic-memory/*.c -o /tmp/dm && /tmp/dm leak\n");
        printf("  不带 leak 参数时本演示一律干净通过，这是本仓库的硬规矩。\n");
    }

    /* 收尾：本函数里所有 malloc 都有对应的 free（head 已在第 4c 幕弹干净了） */
    free_list(head);      /* head 现在是 NULL，这句是安全的空操作；万一上面没弹完，这里兜底 */
    head = NULL;

    printf("\n=== 本节结束 ===\n");
    printf("记住五句话：\n");
    printf("  1) malloc 管字节数、不清零；calloc 两个参数、自动清零；realloc 换大小、搬数据；free 归还。\n");
    printf("  2) realloc 可能搬家，所以必须 p = realloc(p, n)，而且失败时别把 NULL 赋回 p。\n");
    printf("  3) 函数要改调用者的指针 → 参数必须是指针的指针（Node **），口诀“先接后断”。\n");
    printf("  4) free 完立刻置 NULL：同时防住悬空指针和 double free。\n");
    printf("  5) 默认运行必须 0 泄漏；泄漏演示只在 `./binary leak` 时触发。\n");
    return 0;
}
