#include <stdio.h>
#include <stddef.h>   /* ptrdiff_t：两个地址相减结果的类型，配合 %td 打印最准确 */
#include <string.h>   /* strcmp：解析命令行开关 */
#include "static-array.h"

/* 小工具：打印一行分隔线，让输出好读（和第 00 节同款） */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/*
 * 小工具：给任意一段「连续的 int 格子」画内存地图（下标行 / 地址行 / 数值行）。
 * 为什么起点是 int*，还得额外传一个 count？
 *   因为传进来的已经是「退化之后」的指针 —— 长度信息早就掉了，只能靠调用者补上。
 *   这个函数本身就在演示本节主题，所以特意让 main 里也踩一遍。
 */
static void print_cell_map(const char *label, const int *cells, int count)
{
    printf("  %s 首地址 = %p\n", label, (void *)cells);
    printf("        下标: ");
    for (int i = 0; i < count; i++) {
        printf("%-15d", i);
    }
    printf("\n        地址: ");
    for (int i = 0; i < count; i++) {
        printf("%-15p", (void *)&cells[i]);
    }
    printf("\n      相对偏移: ");
    for (int i = 0; i < count; i++) {
        printf("+%-14d", i * (int)sizeof(int));
    }
    printf("\n        数值: ");
    for (int i = 0; i < count; i++) {
        printf("%-15d", cells[i]);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    /* ============================================================
       第 1 幕：int a[5] 到底在内存里怎么摆
       ============================================================ */
    section("第 1 幕：`int a[5]` 在栈上怎么摆 —— 5 个连号的一模一样的格子");

    int a[5] = {10, 20, 30, 40, 50};
    int neighbour = 7;   /* 故意再放一个局部变量，等下看它和数组在不在同一片栈地盘 */

    /*
     * 在数组出生的作用域里，长度当场就算得出来（下一步第 3 幕会演示：
     * 同一个数组一旦传进函数，这个宏立刻作废）。
     */
    int len = STATIC_ARRAY_LEN(a);

    printf("  写这一行声明，编译器当场做了三个决定：\n");
    printf("    1) 长度 5 被写死进二进制，运行时不许改（想改只能改源码重编译）\n");
    printf("    2) 在【栈】上圈出 5*%zu = %zu 字节连号的地盘，一次到位\n",
           sizeof(int), sizeof(a));
    printf("    3) 没有任何地方记录「我有 5 个元素」—— 长度只活在【类型】里，不在内存里\n");
    print_cell_map("a[5] 的地图", a, len);
    printf("  观察一：地址逐格【递增】，间隔恰好 %zu 字节 = sizeof(int)。\n", sizeof(int));
    printf("          「连续」的物理含义就是这句：a[i] 的地址 = 首地址 + i*%zu。\n", sizeof(int));
    printf("          所以按下标访问是 O(1) —— 它不是「一家一家找」，是「一次乘法算出来」。\n");
    printf("  观察二：同屏的局部变量 neighbour 住在 %p，a 的首格在 %p，差 %td 字节。\n",
           (void *)&neighbour, (void *)&a[0],
           (ptrdiff_t)((const char *)&neighbour - (const char *)&a[0]));
    printf("          本机实测：后声明的变量放在【更低】的地址 —— 栈是从高地址往低地址长的，\n");
    printf("          它俩同属 main 这一帧，函数一返回全部一起作废（谁也不比谁活得久）。\n");
    printf("          ⚠ 不过 C 标准【不保证】局部变量之间的相对顺序，别把这个规律当规矩背；\n");
    printf("             能保证的只有一件事：同一个数组内部的 %d 个格子一定是连续的。\n", len);

    /* sizeof 算长度：只在数组出生的作用域里有效（len 已经在上面这一层算好了） */
    printf("\n  sizeof(a) = %zu，sizeof(a[0]) = %zu，相除 = %d 个元素\n",
           sizeof(a), sizeof(a[0]), len);
    printf("  这一步为什么成立？因为在这里 a 的类型还是【完整的数组类型】 int[5]，\n");
    printf("  sizeof 是编译期运算符，它问的是【类型】不是内存 —— 所以运行时零开销、也零风险。\n");

    /* ============================================================
       第 2 幕：数组名在表达式里「退化」成指针
       ============================================================ */
    section("第 2 幕：数组名 == 指向首元素的指针（但它有三个不肯退化的场合）");

    printf("  a       的值 = %p   （退化后：首元素地址，类型 int *）\n", (void *)a);
    printf("  &a[0]   的值 = %p   （同一个地址）\n", (void *)&a[0]);
    printf("  两者数值完全相同：绝大多数表达式里 a 就等价于 &a[0]。\n");

    /* 指针加法的「单位」是元素，不是字节：把这几个差值看清楚 */
    printf("\n  走一步实际跨多少字节（指针加法的单位 = 元素类型的大小）：\n");
    printf("    (a + 1)  比 a  靠后 %td 字节   ← int * 加 1 = 前进 1 个元素 = %zu 字节\n",
           (ptrdiff_t)((const char *)(a + 1) - (const char *)a), sizeof(int));
    printf("    (&a + 1) 比 &a 靠后 %td 字节   ← 整个数组类型加 1 = 前进一整块 = sizeof(a)\n",
           (ptrdiff_t)((const char *)(&a + 1) - (const char *)&a));
    printf("  &a 的类型是 int (*)[5]（「指向 5 个 int 的数组」的指针）—— 数值和 a 一样，步子大 5 倍。\n");
    printf("  这个「同地址、不同类型」的东西在 01-array/two-dimensional 里是主角，那里实测给你看。\n");

    printf("\n  sizeof 的三个版本，一眼看出谁退化成了指针：\n");
    printf("    sizeof(a)   = %zu   ← 数组本体（编译期已知总字节）\n", sizeof(a));
    printf("    sizeof(&a)  = %zu    ← 指针（任何指针都这么大）\n", sizeof(&a));
    printf("    sizeof(a+1) = %zu    ← a+1 已经是 int *，所以只能是指针大小\n", sizeof(a + 1));
    printf("  sizeof 的三个「不退化」场合：① sizeof(数组) ② &数组 ③ 用字符串初始化数组。\n");

    /* [] 就是 *( + ) 的语法糖 */
    printf("\n  两种写法读到的必须是同一个东西：\n");
    for (int i = 0; i < len; i++) {
        printf("    i=%d: a[i] = %d，*(a+i) = %d，%s\n",
               i, a[i], *(a + i), (a[i] == *(a + i)) ? "两者相同" : "!!不同");
    }
    printf("  遍历求和：下标版 = %d，指针走格版 = %d（后者 cursor++ 一次跳 %zu 字节）\n",
           sum_by_index(a, len), sum_by_pointer_walk(a, len), sizeof(int));

    /* ============================================================
       第 3 幕：sizeof 一进函数就废了（退化的代价现场）
       ============================================================ */
    section("第 3 幕：为什么 sizeof(a)/sizeof(a[0]) 一进函数就算错");

    printf("  在 main 里（a 出生的地方）：sizeof(a) = %zu，长度算得出 = %d\n",
           sizeof(a), STATIC_ARRAY_LEN(a));
    printf("  现在把同一个 a 交给函数，函数体里打印 sizeof(它的参数)：\n");
    sizeof_inside_function(a);
    printf("  所以本节的每个函数都长这样：static_array_get(sa, index, &out)、sum_by_index(arr, n) ——\n");
    printf("  长度要么自己随身携带（结构体里的 length），要么当参数额外传进来，绝无第三种可能。\n");

    /* ============================================================
       第 4 幕：StaticArray —— 把「格子总数」和「元素个数」分开记账
       ============================================================ */
    section("第 4 幕：顺序表 = 编译期定死的数组 + 一个 length 记账");

    StaticArray sa;
    printf("  sizeof(StaticArray) = %zu 字节 = data(%d*%zu) + length(%zu) + moved_cells(%zu)\n",
           sizeof(sa), SARRAY_CAPACITY, sizeof(int), sizeof(int), sizeof(int));
    printf("  sa 是 main 的局部变量，住在栈上，地址 = %p —— 全程一个 malloc 都没有\n", (void *)&sa);

    static_array_init(&sa);
    static_array_print(&sa, "  初始化后      ");

    int seed[] = {10, 20, 30, 40, 50};
    int seed_n = STATIC_ARRAY_LEN(seed);          /* 在 seed 出生的地方才能这么算 */
    static_array_fill(&sa, seed, seed_n);
    static_array_print(&sa, "  灌入 5 个初值 ");
    static_array_print_memory_map(&sa, "灌完之后的内存地图");

    printf("\n  O(1) 读第 2 格：");
    int value = 0;
    if (static_array_get(&sa, 2, &value)) {
        printf("拿到 %d\n", value);
    }
    printf("  读第 5 格（只有 5 个元素，合法下标 0..4）：");
    if (!static_array_get(&sa, 5, &value)) {
        printf("  → 函数拦下了，value 保持原来的 %d\n", value);
    }
    printf("  按值找 40 在第 %d 格；找不存在的 999 返回 %d（找不到用 -1，因为 0 是合法下标）\n",
           static_array_find(&sa, 40), static_array_find(&sa, 999));
    printf("  把第 0 格改成 15：");
    static_array_set(&sa, 0, 15);
    static_array_print(&sa, "  改完之后      ");

    /* ============================================================
       第 5 幕：插入 —— 现场看 memmove 挪格子
       ============================================================ */
    section("第 5 幕：按位置插入为什么是 O(n) —— 挪格子的全过程");

    printf("  现在表里有 %d 个元素。往 index=1 插 99（memmove 的工程写法）：\n",
           static_array_size(&sa));
    printf("      要把下标 [1, %d) 这 %d 格整体右移一格，腾出 1 号位，再写 99\n",
           static_array_size(&sa), static_array_size(&sa) - 1);
    static_array_insert_at(&sa, 1, 99);
    static_array_print(&sa, "  插入后        ");

    printf("\n  再来一次，用逐格打印版（往 index=0 插 1），每一步都印出来：\n");
    static_array_insert_at_stepwise(&sa, 0, 1);
    printf("  结论：插到最前面挪了 %d 格 —— 元素越多挪得越多，这就是 O(n) 里的那个 n。\n",
           sa.moved_cells);

    printf("\n  往末尾插（index == length）挪几格？\n");
    static_array_insert_at(&sa, static_array_size(&sa), 77);
    static_array_print(&sa, "  尾部插入后    ");
    printf("  挪 %d 格 —— 插到末尾谁也不用搬。push_back 就是这个特例，所以它是 O(1)。\n",
           sa.moved_cells);
    printf("  不过表此刻已经 %d/%d 装满，先记一笔：第 6 幕要靠删除腾出位置，第 8 幕专门演示装满的绝望。\n",
           sa.length, SARRAY_CAPACITY);

    /* ============================================================
       第 6 幕：删除 —— 整排左移，方向和插入相反
       ============================================================ */
    section("第 6 幕：删除也是 O(n)，只不过这次往左挪");

    int removed = 0;
    printf("  删 index=2（逐格打印版，正着搬）：\n");
    static_array_remove_at_stepwise(&sa, 2, &removed);
    printf("  被删掉的值是 %d，这次挪了 %d 格\n", removed, sa.moved_cells);

    printf("\n  删最后一个元素（memmove 版，挪 0 格）：\n");
    static_array_remove_at(&sa, static_array_size(&sa) - 1, &removed);
    static_array_print(&sa, "  删尾部之后    ");
    printf("  删尾部只挪 %d 格，和插尾部一样便宜 —— 数组「头重脚轻」是它的天性。\n",
           sa.moved_cells);

    printf("\n  删完腾出了 %d 个空位，push_back 又能用了（它就是「插到末尾」的专用快捷版）：\n",
           SARRAY_CAPACITY - sa.length);
    static_array_push_back(&sa, 88);
    static_array_print(&sa, "  push_back 后  ");

    /* ============================================================
       第 7 幕：算一笔总账 —— 头部增删 n 次 = O(n^2)
       ============================================================ */
    section("第 7 幕：连续往头部插入，总挪动次数是平方级");

    StaticArray bill;
    static_array_init(&bill);
    int total_moves = 0;
    int insert_times = 6;
    printf("  连续往 index=0 插 %d 次，逐次记录挪动格数：\n", insert_times);
    for (int i = 0; i < insert_times; i++) {
        if (static_array_insert_at(&bill, 0, (i + 1) * 10)) {
            total_moves += bill.moved_cells;
            printf("    第 %d 次：插完表里有 %d 个元素 → 这次挪 %d 格（累计 %d 格）\n",
                   i + 1, bill.length, bill.moved_cells, total_moves);
        }
    }
    printf("  合计挪 %d 格 = 0+1+2+...+%d = %d*(%d-1)/2 —— 「在头部做 n 次插入」整体是 O(n^2)。\n",
           total_moves, insert_times - 1, insert_times, insert_times);
    printf("  这正是链表存在的理由（见 02-linked-list/）：链表头插只改一个指针，O(1)。\n");
    static_array_print(&bill, "  这本账表现在  ");
    static_array_destroy(&bill);      /* 每个结构都要 destroy —— 为什么它里面没有 free，见 README */

    /* ============================================================
       第 8 幕：容量天花板 —— 静态数组最现实的痛点
       ============================================================ */
    section("第 8 幕：装满之后，静态数组彻底没辙");

    StaticArray full;
    static_array_init(&full);
    for (int i = 0; i < SARRAY_CAPACITY; i++) {
        static_array_push_back(&full, (i + 1) * 100);
    }
    static_array_print(&full, "  装到满        ");
    printf("  再往头部插一个试试：");
    if (!static_array_insert_at(&full, 0, 9999)) {
        printf("      ↑ 上面就是静态数组的天花板：它不能变大，只能失败。\n");
    }
    printf("  要突破天花板只有两条路：① 改 #define SARRAY_CAPACITY 重新编译；② 换下一节的动态数组。\n");

    /* ============================================================
       第 9 幕（命令行开关才触发）：真的越界会发生什么
       ============================================================ */
    section("第 9 幕：越界访问 —— C 一行都不拦你");

    if (argc > 1 && strcmp(argv[1], "--show-oob") == 0) {
        printf("  你传了 --show-oob，现在【故意】读越界的一格（栈上缓冲区越界读）。\n");
        printf("  这段代码在默认运行里永远不会执行 —— 本仓库的规矩是默认必须 ASan 干净。\n");
        int probe[4] = {11, 22, 33, 44};
        volatile int bad_index = 4;   /* volatile：防止编译器在编译期就把这次越界识破并警告，
                                         我们想要的是「运行时真的这么读了」这个现场 */
        printf("  现在执行 probe[%d]（这个数组只有 4 格，合法下标 0..3）：\n", bad_index);
        /*
         * fflush(stdout)：ASan 一旦判定出错就【直接终止进程】，
         * 此时 stdout 缓冲区里还没写出去的内容会整段丢掉（你会看到输出莫名其妙断在半行）。
         * 先把话说完再动手，这样"越界前最后一句解释"能留在输出里 —— 这也是个通用小技巧。
         */
        fflush(stdout);
        int oob_value = probe[bad_index];   /* ← 越界读：ASan 就在这一行报 stack-buffer-overflow */
        printf("  probe[4] = %d   ← 编译时没报错、运行时没停住，值就这么被「读走」了。\n", oob_value);
        printf("  它读到的是紧挨着这块数组的内存（可能是别的局部变量、可能是保存的寄存器）。\n");
        printf("  换个编译器、换个优化级别，可能读到 0、可能直接段错误、也可能被攻击者利用\n");
        printf("  —— 这就是「缓冲区溢出」这类经典漏洞的全部成因。\n");
    } else {
        printf("  C 语言里 a[i] 只做一件事：算地址（首地址 + i*sizeof(元素)），然后照办。\n");
        printf("  它【没有】「i 到底超没超」这一步，原因有二：\n");
        printf("    ① 数组变量本身不存长度，编译器想知道也没地方查（长度只在类型里，一退化就没了）；\n");
        printf("    ② 每次访问都补一遍边界比较要有开销，C 的哲学是「不为安全多付钱」。\n");
        printf("  所以越界不会报错，只会静悄悄读写隔壁内存 —— 未定义行为，什么都能发生。\n");
        printf("  想亲眼看现场？带开关重跑（会故意越界，ASan 立刻报 stack-buffer-overflow）：\n");
        printf("      gcc -Wall -Wextra -Wshadow -std=c11 -g -fsanitize=address -static-libasan \\\n");
        printf("          01-array/static-array/*.c -o /tmp/verify_arr_1 && /tmp/verify_arr_1 --show-oob\n");
        printf("  平时怎么防：① 所有下标都走 static_array_get/set 这种自带检查的入口；\n");
        printf("             ② 写完就 `make asan` 跑一遍，越界、泄漏、悬空一次全抓出来。\n");
    }

    /* ============================================================
       收尾：每个结构都要 destroy
       ============================================================ */
    section("收尾：destroy（顺便确认「静态数组到底要不要 free」）");
    static_array_print(&sa, "  演示结束时    ");
    static_array_destroy(&sa);
    static_array_destroy(&full);
    printf("  两个 StaticArray 都 destroy 过了。它们从头到尾没 malloc 过一次，\n");
    printf("  所以不存在「忘记 free」这种事故 —— 这是静态数组最大的优点：\n");
    printf("  内存自动、零成本、不会忘还；代价就是第 8 幕那个撞了墙的天花板。\n");
    static_array_print(&sa, "  destroy 之后  ");

    printf("\n=== 本节结束 ===\n");
    printf("记住五句话：\n");
    printf("  1) 静态数组长度编译期定死，格子内嵌在结构体里，跟着作用域自动生灭，不用也不能 free。\n");
    printf("  2) a[i] 的地址 = 首地址 + i*sizeof(int)，一次乘法算出来 → 随机访问 O(1)。\n");
    printf("  3) sizeof(a)/sizeof(a[0]) 只在数组出生的作用域有效；一当参数就退化成指针，长度必须另传。\n");
    printf("  4) 插入删除要把后半排格子整体挪动（区间重叠 → 必须 memmove），平均 n/2 次 → O(n)。\n");
    printf("  5) C 不做边界检查；检查写进自己的 get/set，再用 make asan 兜底。\n");
    return 0;
}
