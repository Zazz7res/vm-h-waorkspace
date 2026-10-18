#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>     /* uintptr_t：用来把地址当纯整数保存，见下面 array_resize 里的注释 */
#include <string.h>   /* memset 用得到，它来自标准库 <string.h> */
#include "dynamic-memory.h"

/*
 * ===================================================================
 * 打印一个小整数数组：本节所有"看内存里到底装了啥"都靠它
 * ===================================================================
 * n 传进来是必要的：数组一旦变成指针，长度信息就没了（第 1 节讲过的退化）。
 */
void print_int_array(const char *tag, const int *arr, int n)
{
    if (arr == NULL) {
        printf("  %s: (NULL —— 内存没申请到)\n", tag);
        return;
    }
    printf("  %s 地址=%p: [", tag, (void *)arr);
    for (int i = 0; i < n; i++) {
        printf("%d%s", arr[i], (i + 1 < n) ? " " : "");
    }
    printf("]  %d 格\n", n);
}

/*
 * ===================================================================
 * 1. malloc：只说"给我多少字节"，内容一概不管
 * ===================================================================
 * malloc((size_t)n * sizeof(int)) 三个细节：
 *   - 参数单位是【字节】，不是"个数"。写 malloc(10) 只有 10 字节 = 2 个 int。
 *   - 前面乘 (size_t) 是为了防止 n 很大时 int 乘法溢出成负数/小值。
 *   - 失败返回 NULL，必须判空。申请不到是真实会发生的（尤其后面建大图、大表）。
 * malloc 不会帮你清零：拿到的可能是全 0（新页面），也可能是上一个主人留下的垃圾。
 */
int *arr_new_malloc(int n, int fill_value)
{
    if (n <= 0) {
        return NULL;
    }
    int *arr = malloc((size_t)n * sizeof(int));
    if (arr == NULL) {
        printf("      [错误] malloc 申请 %d 个 int 失败\n", n);
        return NULL;
    }
    for (int i = 0; i < n; i++) {
        arr[i] = fill_value;          /* 自己填，malloc 不管这事 */
    }
    printf("      malloc(%zu 字节) 拿到 %p，然后手工填成 %d\n",
           (size_t)n * sizeof(int), (void *)arr, fill_value);
    return arr;
}

/*
 * ===================================================================
 * 2. calloc：两个参数 + 自动清零
 * ===================================================================
 * calloc(count, size) 做三件事：
 *   1) 算总字节数 = count × size，而且【检查乘法有没有溢出】（比手写安全）；
 *   2) 申请内存；
 *   3) 全部填 0。
 * 什么时候必须用 calloc？
 *   - 数组当"计数表 / 出现次数 / 位图"用时：初值 0 就是算法的一部分，
 *     哈希表的桶计数、并查集的父数组、布隆过滤器的位数组，全都靠"默认没值"。
 *   - 用 malloc 就得再补一句 memset，两句代码干一句的活。
 * 代价：清零本身要花时间（O(n)），后面第 4 节动态数组扩容时我们会算这笔账。
 */
int *arr_new_calloc(int n)
{
    if (n <= 0) {
        return NULL;
    }
    int *arr = calloc((size_t)n, sizeof(int));   /* 注意：个数、每个多大，分开传 */
    if (arr == NULL) {
        printf("      [错误] calloc 申请 %d 个 int 失败\n", n);
        return NULL;
    }
    printf("      calloc(%d, %zu) 拿到 %p，全部已经是 0\n",
           n, sizeof(int), (void *)arr);
    return arr;
}

/*
 * ===================================================================
 * 3. malloc + memset：手工清零，效果等同 calloc
 * ===================================================================
 * memset(ptr, 0, bytes) 是"逐字节写 0"，第二个参数是【字节值】不是 int。
 * 所以 memset 只能安全地清零（0 的任意字节表示都是数值 0）；
 * 想"全填成 1"不能用 memset(…, 1, …)，那会得到 0x01010101 = 16843009。
 * 这是 memset 的经典误区，现在记住省得以后 debug 半天。
 *
 * 那 memset 版本和 calloc 版本选哪个？
 *   实践里两者几乎没差别（很多平台上 calloc 就是 malloc+memset）。
 *   写这里就是为了让你确认："清零"这件事本身没有魔法，就是往里写 0。
 */
int *arr_new_malloc_memset(int n)
{
    if (n <= 0) {
        return NULL;
    }
    size_t bytes = (size_t)n * sizeof(int);
    int *arr = malloc(bytes);
    if (arr == NULL) {
        printf("      [错误] malloc 申请 %zu 字节失败\n", bytes);
        return NULL;
    }
    memset(arr, 0, bytes);                      /* 从 arr 开始，连续 bytes 个字节全写 0 */
    printf("      malloc(%zu)+memset(0) 拿到 %p，也是全 0\n", bytes, (void *)arr);
    return arr;
}

/*
 * ===================================================================
 * 3b. 亲眼看看"malloc 不清零"
 * ===================================================================
 * 步骤：申请一块 → 全部写成脏值 → free 归还 → 再申请同样大小。
 * 堆管理器会把刚归还的那块再发给你（它只管"发还"，不管"擦洗"），
 * 于是新 malloc 出来的数组里赫然还是刚才的脏值。
 *
 * ⚠ 这个现象依赖实现（不同编译器/allocator 行为不同），
 *   所以我们要讲的结论不是"一定能看到脏值"，而是：
 *   【你绝对不能依赖 malloc 给你的初值，读到什么都有可能。】
 */
void dirty_block_setup(int n, int dirty_value)
{
    if (n <= 0) {
        return;
    }
    int *block = malloc((size_t)n * sizeof(int));
    if (block == NULL) {
        return;
    }
    for (int i = 0; i < n; i++) {
        block[i] = dirty_value;                 /* 涂脏 */
    }
    printf("      先申请 %d 格涂满 %d，地址 %p，然后 free 掉（归还时不擦洗）\n",
           n, dirty_value, (void *)block);
    free(block);                                /* free 只"归还"，不清内容 */
}

int *arr_new_malloc_untouched(int n)
{
    if (n <= 0) {
        return NULL;
    }
    int *arr = malloc((size_t)n * sizeof(int)); /* 只申请，一个字节都不写 */
    if (arr == NULL) {
        printf("      [错误] malloc 申请 %d 个 int 失败\n", n);
        return NULL;
    }
    printf("      再次 malloc(%d 格)，拿到 %p —— 里面的值见下方打印，全是垃圾\n",
           n, (void *)arr);
    return arr;
}

/*
 * ===================================================================
 * 4. realloc：换一块更大的（可能原地扩，可能整块搬家）
 * ===================================================================
 * realloc(旧指针, 新字节数) 的三种结局：
 *   结局 A：后面正好有空地 → 原地扩大，返回的地址和原来一模一样。
 *   结局 B：后面被别人占了 → 另外找一块大的，把老数据【整块抄过去】，然后 free 老的。
 *            这时返回的地址和原来不同，旧指针当场失效！
 *   结局 C：找不到 → 返回 NULL，【原来的内存原封不动还有效】。
 *
 * 因为结局 B 存在，唯一正确的写法是"接收返回值"：
 *     int *tmp = realloc(arr, bytes);
 *     if (tmp == NULL) { arr 还在，可以照常继续用 —— 别 free 它，也别丢它 }
 *     else { arr = tmp; }
 * 因为结局 C 存在，绝对不能写成 `arr = realloc(arr, bytes);` 然后不判空：
 * 一旦失败，arr 被 NULL 覆盖，原来那块内存就再也找不回来 —— 泄漏。
 * （本节把它包进 array_resize，返回"要么新地址要么原地址"，调用者只需照样接收。）
 *
 * 顺便：realloc(NULL, bytes) 等价于 malloc(bytes)，所以这个函数也能当"第一次申请"用。
 */
int *array_resize(int *arr, int new_n)
{
    /* 把旧地址存成**一个纯整数**（uintptr_t 专门用来装指针的数值类型）。
     * 为什么不直接存 void *？因为 realloc 成功时旧块已经被释放，
     * 拿一个已释放的 void * 去打印/比较，编译器会报 -Wuse-after-free。
     * 存成整数就彻底安全：我们比的只是两串地址数字，从来没有解引用它。 */
    uintptr_t old_addr = (uintptr_t)arr;
    if (new_n <= 0) {
        printf("      [错误] 容量必须是正数，这次不扩容，原内存保持不变\n");
        return arr;
    }
    size_t bytes = (size_t)new_n * sizeof(int);
    int *moved = realloc(arr, bytes);
    if (moved == NULL) {
        /* 结局 C：申请失败。原 arr 依然有效，长度还是老值，直接原样还回去 */
        printf("      [错误] realloc 到 %d 格失败，原内存 0x%lx 仍然可用（没有泄漏，也没有丢数据）\n",
               new_n, (unsigned long)old_addr);
        return arr;
    }
    printf("      realloc: 旧地址 0x%lx  ->  新地址 0x%lx   %s   (%d 格 = %zu 字节)\n",
           (unsigned long)old_addr, (unsigned long)moved,
           (moved == (int *)old_addr) ? "【原地扩大，地址没变】" : "【整块搬走了！旧指针当场作废】",
           new_n, bytes);
    return moved;
}

/*
 * ===================================================================
 * 5. 反面教材：用 Node *（一级指针）在头部插节点
 * ===================================================================
 * 这是 pointer-basics 里 swap_by_value 的链表版本，错误一模一样：
 *
 *    main 的 head（住在 0xA00）        函数参数 head（住在 0xB00，是拷贝）
 *    ┌──────────┐                       ┌──────────┐
 *    │ 0xF020   │ ──> [10] -> [11] -> 尾 │ 0xF020   │ ──> [10] -> [11] -> 尾
 *    └──────────┘                       └──────────┘
 *                                              │ fresh = malloc → [99]
 *                                              │ fresh->next = head（接上老链）
 *                                              │ head = fresh   ← 只改了 0xB00 这份拷贝！
 *                                              ↓
 *                                        [99] -> [10] -> [11] -> 尾   （函数里看着很美）
 *
 * 函数一返回，0xB00 那份拷贝连同"新头"一起蒸发，main 的 head 还是指向 [10]。
 * 更糟的是那个 fresh 节点外面谁也拿不到地址 —— 它注定要泄漏。
 * 我们在返回前把它 free 掉（否则 ASan 会报泄漏），而这本身就证明：
 * 这个函数白干了一场，造出来的节点送不出去。
 */
void push_front_broken(Node *head, int val)
{
    Node *fresh = malloc(sizeof(Node));
    if (fresh == NULL) {
        printf("      [错误] 新节点申请失败\n");
        return;
    }
    fresh->val = val;
    fresh->next = head;      /* 这一步是真的接上了：fresh -> 老链 */
    head = fresh;            /* ❌ 致命一行：改的是"头指针的拷贝"，改不到 main 的 head */

    print_list("      [函数内部] 我以为链表变成", head);
    free(fresh);             /* 不 free 就泄漏；free 它 = 承认这个节点送不出去 */
    /* 注意：这里 free(fresh) 不会伤到老链 —— 老链是 main 通过它自己的 head 持有的 */
}

/*
 * ===================================================================
 * 6. 正确写法：参数是 Node **（指针的指针）
 * ===================================================================
 * 为什么要二级？链条是这样的：
 *   想改 main 里的 head      → 得拿到 head 的地址
 *   head 本身是个指针        → head 的地址就是"指针的指针" = Node **
 *
 *    main 的 head (0xA00)          函数参数 head (0xB00)
 *    ┌──────────┐                   ┌──────────┐
 *    │ 0xF020   │ <─── *head ─────── │ 0xA00    │   head 里存的是"0xA00"
 *    └──────────┘                   └──────────┘    *head 就是 main 那个 head 本身
 *          │
 *          ↓                        两句关键代码：
 *       [10] -> [11] -> 尾           fresh->next = *head;   新节点先接上老链
 *                                    *head = fresh;         把 main 的 head 改成新节点
 *                                       ↑ 星号 = 往 0xA00 那个格子里写
 *
 * 顺序不能反！先写 *head = fresh 的话，老链的头就被覆盖丢了，fresh->next 拿不到值。
 * 口诀：【先接后断】—— 先把新节点的 next 安顿好，再动别人的头。
 */
void push_front_ok(Node **head, int val)
{
    if (head == NULL) {
        printf("      [错误] 连头指针的地址都没传进来，啥也改不了\n");
        return;
    }
    Node *fresh = malloc(sizeof(Node));
    if (fresh == NULL) {
        printf("      [错误] 新节点申请失败\n");
        return;
    }
    fresh->val = val;
    fresh->next = *head;    /* 第 1 步：新节点指向"当前真正的头"（*head 就是 main 的 head） */
    *head = fresh;          /* 第 2 步：把 main 的 head 改成新节点 —— 只有二级指针办得到 */

    print_list("      [函数内部] 改完之后（这次是真的）", *head);
}

/*
 * ===================================================================
 * 7. 弹头：另一个"必须二级"的铁证
 * ===================================================================
 * 弹出头节点时，头指针可能要变成下一个节点，甚至变成 NULL（链空了）。
 * 一级指针版本连"把 head 置 NULL"都做不到，main 会留着一个已 free 的悬空头指针。
 *
 * 边界情况：
 *   - head 本身是 NULL（调用者连地址都没给）→ 返回 -1。
 *   - *head 是 NULL（空链）→ 返回 -1，绝不 free 任何东西（free(NULL) 虽合法但逻辑上是错的）。
 *   - value_out 允许传 NULL，表示"我只删不看值"。
 * 顺序：先记下要删的节点 → 把头指针挪到下一个 → 再 free 记下的那个（先接后断）。
 */
int pop_front_ok(Node **head, int *value_out)
{
    if (head == NULL || *head == NULL) {
        printf("      [空链] 没有节点可弹\n");
        return -1;
    }
    Node *gone = *head;         /* 先把要拆的记下来 */
    if (value_out != NULL) {
        *value_out = gone->val; /* 值从指针参数带出去（输出参数） */
    }
    *head = gone->next;         /* 头指针挪到下一个；链里只剩一个节点时这里就变成 NULL */
    free(gone);                 /* 现在再拆，安全 */
    return 0;
}

/*
 * ===================================================================
 * 8. 建链 / 量长度 / 打印 / 拆链：这一节的"标准四件套"
 * ===================================================================
 * build_list(3, 10) 造出 [10] -> [11] -> [12] -> NULL。
 * 这里刻意用"记住尾巴往后接"（尾插），是为了让打印顺序和数字顺序一致，好读。
 * 如果改用头插（每步都 push_front_ok），造出来的链是反的：[12] -> [11] -> [10]。
 * 两种都是 O(n)，区别只在要不要多维护一个 tail 指针。
 */
Node *build_list(int n, int start_value)
{
    Node *head = NULL;
    Node *tail = NULL;
    for (int i = 0; i < n; i++) {
        Node *fresh = malloc(sizeof(Node));
        if (fresh == NULL) {
            printf("      [错误] 第 %d 个节点申请失败，先把手上的还回去\n", i + 1);
            free_list(head);    /* 半成品不留，直接全拆 —— 否则就是泄漏 */
            return NULL;
        }
        fresh->val = start_value + i;
        fresh->next = NULL;
        if (head == NULL) {
            head = fresh;
            tail = fresh;
        } else {
            tail->next = fresh;
            tail = fresh;
        }
    }
    printf("      build_list(%d, %d)：申请了 %d 次 malloc，每次 %zu 字节\n",
           n, start_value, n, sizeof(Node));
    return head;
}

int list_length(const Node *head)
{
    int len = 0;
    const Node *cur = head;      /* 用 cur 走，head 原地不动，函数后面还能从头再来 */
    while (cur != NULL) {
        len++;
        cur = cur->next;
    }
    return len;
}

void print_list(const char *tag, const Node *head)
{
    printf("%s: ", tag);
    const Node *cur = head;
    if (cur == NULL) {
        printf("(空链 NULL)\n");
        return;
    }
    while (cur != NULL) {
        if (cur->next == NULL) {
            printf("[%d|尾]", cur->val);
        } else {
            printf("[%d|%p] -> ", cur->val, (void *)cur->next);
        }
        cur = cur->next;
    }
    printf("   共 %d 个节点\n", list_length(head));
}

/*
 * 拆链的唯一正确顺序：抄下 next → free 当前 → 前进。
 * 写成 free(cur); cur = cur->next; 就是悬空指针（读已归还的内存）。
 * free_list(NULL) 什么都不做，安全。
 */
void free_list(Node *head)
{
    Node *cur = head;
    int freed = 0;
    while (cur != NULL) {
        Node *next_to_keep = cur->next;
        free(cur);
        freed++;
        cur = next_to_keep;
    }
    if (freed > 0) {
        printf("      free_list：归还了 %d 块内存\n", freed);
    }
}

/*
 * ===================================================================
 * 9. leak_demo：故意漏两块，只有命令行传参数才会被调用
 * ===================================================================
 * 这个函数【故意】不 free。跑法见 README：用第 1 节同款 ASan 编译选项
 * 把本目录所有 .c 编成一个二进制，然后带上参数 leak 运行，
 * ASan 会在程序退出时打印：
 *     ERROR: AddressSanitizer: NN byte(s) leaked in 2 allocation(s).
 * 而普通编译（make one）什么都不会报 —— 这就是内存泄漏"看不见摸不着"的地方。
 *
 * 泄漏为什么会发生？三个最常见的原因，这里一个都占：
 *   1) 申请了忘了还（leak_ints）；
 *   2) 指针变量出了作用域，地址丢了，想还也没处还（leak_node）；
 *   3) （见 array_resize 的讲解）realloc 失败还把返回值直接赋回原指针，老块丢失。
 */
void leak_demo(void)
{
    printf("\n  >>> 现在开始【故意泄漏】，ASan 会打红，这是设计好的演示 <<<\n");

    int *leak_ints = malloc(100 * sizeof(int));   /* 漏 1：400 字节，永远没人 free */
    if (leak_ints != NULL) {
        leak_ints[0] = 1;                        /* 摸一下，证明它真的被申请到了 */
    }
    printf("  漏掉 1：malloc 了 100 个 int = %zu 字节，地址 %p，函数结束指针就没了\n",
           (size_t)(100 * sizeof(int)), (void *)leak_ints);

    Node *leak_node = malloc(sizeof(Node));      /* 漏 2：一个节点，同样不还 */
    if (leak_node != NULL) {
        leak_node->val = 999;
        leak_node->next = NULL;
    }
    printf("  漏掉 2：malloc 了 1 个 Node = %zu 字节，地址 %p，val=%d\n",
           sizeof(Node), (void *)leak_node,
           (leak_node != NULL) ? leak_node->val : -1);

    printf("  一共故意漏掉 %zu 字节 / 2 次分配。\n",
           (size_t)(100 * sizeof(int)) + sizeof(Node));
    printf("  普通编译跑这个演示：程序照样正常退出，一个字都不提示 —— 泄漏就是这么阴。\n");
    printf("  带 ASan 编译再跑：退出时立刻打印 leaked in 2 allocation(s)，还附上申请时的调用栈。\n");
    printf("  ⚠ 这个函数只会在你运行 `./二进制 leak` 时被调用，默认运行是干净的。\n");
}
