#include <stdio.h>
#include <stdlib.h>
#include "struct-and-typedef.h"

/* 小工具：打印一行分隔线，让输出好读（和第 1 节同款） */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

int main(void)
{
    /* ============================================================
       第 1 幕：struct 怎么定义、怎么初始化、到底占几格
       ============================================================ */
    section("1. struct 定义与初始化 + sizeof 实测");

    /* 初始化的四种写法，一次看清楚 */
    Point a = {3, 1};                       /* 最常用：按声明顺序填 */
    Point b = { .y = 7, .x = -2 };          /* 指定成员写法：顺序随便打乱，可读性最好 */
    Point c = {0};                          /* 只写 {0}：整个结构体清零，强烈推荐 */
    Point d;                                /* 不初始化：里面是栈上的垃圾值（故意演示，别学） */
    d.x = 100; d.y = 200;                   /* 也可以一个字段一个字段地塞 */

    printf("  a=(%d,%d)  b=(%d,%d)  c=(%d,%d)  d=(%d,%d)\n",
           a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y);
    printf("  字段访问用点：a.x 读作\"a 这个点里的 x\"\n");

    printf("\n  sizeof 实测（字节数由编译器算，别看人家用嘴猜）：\n");
    printf("    sizeof(Point)   = %zu   两个 int：4 + 4 = 8，刚好不用补\n", size_of_point());
    printf("    sizeof(CharInt) = %zu   char(1) + int(4) = 5 ？不，中间补了 3 格垃圾\n",
           size_of_char_int());
    printf("      地址   0  1  2  3  4  5  6  7\n");
    printf("            [c][填][填][填][      n      ]\n");
    printf("      int 必须摆在 4 的倍数地址上，所以 c 后面要塞 3 个空格子\n");
    printf("    sizeof(Rect)    = %zu   两个 Point：8 + 8 = 16\n", size_of_rect());
    printf("    sizeof(Node)    = %zu   int(4) + 补(4) + 指针(8) = 16 —— 链表节点就这么大\n",
           size_of_node());
    printf("    sizeof(Node *)  = %zu   指针大小和它指向谁无关，64 位机上永远是 8\n",
           sizeof(Node *));
    printf("  ⚠ 顺序会影响大小：把大字段先写、小字段后写，通常能少补几格。\n");

    /* ============================================================
       第 2 幕：typedef 到底省了什么
       ============================================================ */
    section("2. typedef：给结构体起个外号，从此不用写 struct");

    /* 写法 A：没有别名 —— 每次都得写 struct TagPoint，八个字母一个不能少 */
    struct TagPoint tp1 = {1, 2};
    struct TagPoint tp2 = {3, 4};
    printf("  不用 typedef: struct TagPoint tp1 = (%d,%d)，声明时 struct 关键字必须带着\n",
           tp1.x, tp1.y);

    /* 写法 B：有别名 —— Point 本身就是一个类型名，和 int 一个待遇 */
    Point p1 = {1, 2};
    Point p2 = {3, 4};
    printf("  用了 typedef: Point p1 = (%d,%d)，短一半\n", p1.x, p1.y);

    /* 真正麻烦的是数组和指针：没别名时声明会长成这样 */
    struct TagPoint arr_a[3];                  /* 数组：还好 */
    struct TagPoint *ptr_a = &tp1;             /* 指针：啰嗦 */
    const struct TagPoint *readonly_a = &tp2;  /* 加 const：更啰嗦 */
    printf("  没别名时声明一堆名字要写 3 遍 struct：arr_a（%zu 字节）/ ptr_a=%p / readonly_a=%p\n",
           sizeof arr_a, (void *)ptr_a, (void *)readonly_a);

    Point arr_b[3];
    Point *ptr_b = &p1;
    const Point *readonly_b = &p2;
    printf("  有别名后同样三个声明：arr_b（%zu 字节）/ ptr_b=%p / readonly_b=%p，清爽多了\n",
           sizeof arr_b, (void *)ptr_b, (void *)readonly_b);
    printf("  两种写法在内存里完全一样：sizeof(struct TagPoint)=%zu, sizeof(Point)=%zu\n",
           sizeof(struct TagPoint), sizeof(Point));

    /* 一个必须知道的坑：typedef 出来的是"类型"，不是"指针类型" */
    Point r1 = {9, 9};
    Point *pr1 = &r1;
    printf("  坑：`typedef Point MyPoint; MyPoint x, *y;` —— 这两个一个是点、一个是指针，\n");
    printf("      和 `int x, *y;` 一模一样。别名代替的是 Point 那两个词，不是整个声明。\n");
    printf("      实测：pr1->x=%d（箭头取到的就是 r1.x=%d），*pr1 和 r1 是同一块内存\n",
           pr1->x, r1.x);
    printf("  typedef 另一个价值：以后想给 Point 加个字段，只改定义那一处，\n");
    printf("  全项目几十处 `Point p;` 一个字都不用动 —— 这就是“把类型名字”的意义。\n");

    /* ============================================================
       第 3 幕：`.` 和 `->` 的区别（配内存图）
       ============================================================ */
    section("3. `.` 用点还是用箭头：看手里拿的是东西还是地址");

    Point origin = {0, 0};
    Point *po = &origin;
    printf("  声明：Point origin;  Point *po = &origin;\n");
    printf("  origin 的地址 = %p，po 里存的值 = %p（两者相同），po 自己住在 %p\n",
           (void *)&origin, (void *)po, (void *)&po);
    printf("\n");
    printf("     内存里长这样：\n");
    printf("       po (在 %p)          origin (在 %p)\n", (void *)&po, (void *)&origin);
    printf("       ┌──────────┐        ┌──────┬──────┐\n");
    printf("       │ %p │ ─────> │  0   │  0   │\n", (void *)&origin);
    printf("       └──────────┘        └──────┴──────┘\n");
    printf("        它是地址，所以用箭头   它是东西本身，所以用点\n");
    printf("\n");

    po->x = 42;                    /* 顺着地址进屋，把 x 改成 42 */
    printf("  执行 po->x = 42; 之后：\n");
    printf("    origin.x        = %d   ← 用点访问原件（main 手里的东西）\n", origin.x);
    printf("    (*po).x         = %d   ← 先解引用拿到东西，再用点：完全等价\n", (*po).x);
    printf("    po->x           = %d   ← 箭头就是 (*po).x 的简写，编译器帮你展开\n", po->x);

    origin.y = 7;
    printf("  反过来 origin.y = 7; 之后 po->y = %d —— 一块内存两个名字，改哪边都算数\n", po->y);

    /* 边界情况：手里是指针却用点，或手里是东西却用箭头 —— 都是编译错误 */
    printf("\n  两种写反法（编译器都会拦下来，不会带到运行时）：\n");
    printf("    origin->x  ❌ origin 是东西不是地址，箭头没意义\n");
    printf("    po.x       ❌ po 是指针，点会被当成\"指针自己有个叫 x 的字段\"\n");
    printf("    po->x 和 (*po).x 二者相同；写哪个看哪个读起来顺，正式代码几乎都用箭头。\n");

    /* ============================================================
       第 4 幕：结构体数组 —— n 个结构体肩并肩
       ============================================================ */
    section("4. 结构体数组：一整排点");

    Point pts[5];
    init_point_array(pts, 5);              /* 传数组必须连长度一起传 */
    print_point_array(pts, 5);
    printf("  相邻两个元素的地址差 = %ld 字节 = sizeof(Point) —— 它们肩并肩排着\n",
           (long)((const char *)&pts[1] - (const char *)&pts[0]));
    printf("  数组名会退化：pts 传进函数就只剩首地址，长度信息丢了，所以 n 必须自己带。\n");

    swap_points(&pts[0], &pts[4]);
    printf("  swap_points(&pts[0], &pts[4]) 之后: 第 0 个 = (%d,%d)，第 4 个 = (%d,%d)\n",
           pts[0].x, pts[0].y, pts[4].x, pts[4].y);
    printf("  （交换成功 = 参数是指针；`Point t = *a;` 那行就是整块 8 字节拷贝）\n");

    /* ============================================================
       第 5 幕：自引用结构 —— 链表节点的雏形（下一章的地基）
       ============================================================ */
    section("5. 自引用结构 typedef struct Node { int val; struct Node *next; } Node;");

    printf("  为什么 next 的类型要写 struct Node 而不是 Node？\n");
    printf("    因为读到 next 那一行时，别名 Node 还没生效（typedef 要等右花括号结束），\n");
    printf("    但标签 struct Node 在左花括号那一刻就已经登记了，可以先用。\n");
    printf("  为什么必须是指针？\n");
    printf("    写成 `struct Node next;` 就是 Node 里装 Node 里装 Node……大小算不出来，编译器直接报错。\n");
    printf("    指针只装地址，大小永远是 %zu，所以能算出来：sizeof(Node) = %zu。\n",
           sizeof(Node *), size_of_node());

    Node *chain = build_node_chain(3, 10);
    if (chain != NULL) {
        print_node_chain(chain);
        printf("  三个节点的地址：%p %p %p —— 不连续也无所谓，链靠 next 串起来\n",
               (void *)chain, (void *)chain->next, (void *)chain->next->next);
        printf("  遍历量出来的长度 = %d\n", node_chain_length(chain));
        printf("  头节点的值 = %d（head->val）\n", chain->val);
        node_chain_free(chain);           /* 谁申请谁负责：逐个 free */
        chain = NULL;                     /* free 完立刻置空，防止再次误用 */
        printf("  已经 node_chain_free 并把指针置 NULL —— ASan 报告里 0 泄漏\n");
    }

    /* ============================================================
       第 6 幕：传结构体 vs 传结构体指针 —— 正反例 + 成本
       ============================================================ */
    section("6. 传整个结构体（复印件） vs 传结构体指针（门牌号）");

    printf("  先说正确性：\n");
    Point mv = {1, 1};
    printf("    调用前 main 里的 mv = (%d,%d)，地址 %p\n", mv.x, mv.y, (void *)&mv);
    move_point_by_value(mv, 10, 20);
    printf("    调用后 main 里的 mv = (%d,%d)  ← 纹丝不动，函数改的是拷贝\n", mv.x, mv.y);

    printf("    调用前 main 里的 mv = (%d,%d)，地址 %p\n", mv.x, mv.y, (void *)&mv);
    move_point_by_pointer(&mv, 10, 20);
    printf("    调用后 main 里的 mv = (%d,%d)  ← 真的改了\n\n", mv.x, mv.y);

    move_point_by_pointer(NULL, 1, 1);      /* 演示判空保护：不判空这里就段错误了 */

    Rect room = { {2, 3}, {12, 8} };
    printf("  再说成本：同一个算法，两个版本结果一样\n");
    printf("    sizeof(Rect) = %zu 字节；按值传要抄 %zu 字节，按指针传只抄 %zu 字节（一个地址）\n",
           size_of_rect(), size_of_rect(), sizeof(Rect *));
    printf("    rect_area_by_value(room)   = %lld\n", rect_area_by_value(room));
    printf("    rect_area_by_pointer(&room) = %lld\n", rect_area_by_pointer(&room));
    printf("    差别不只在于字节数：按值传进来的那份是拷贝，函数里改不到原件；\n");
    printf("    想改必须传指针，这是“必须”，不是“建议”。\n");
    printf("    只读的大结构体用 const Rect *：既省拷贝，又承诺不改。\n");

    print_rect("room（用指针版打印）", &room);

    /* ============================================================
       第 7 幕：嵌套结构 —— 一层层往里套
       ============================================================ */
    section("7. 嵌套结构：Window 里有 Rect，Rect 里有 Point");

    Window win = { { {10, 10}, {80, 50} }, 7, 1 };
    printf("  声明长这样：Window win = { { {10,10}, {80,50} } , 7, 1 };\n");
    printf("  一层层剥：win.frame.top_left.x = %d，win.frame.bottom_right.y = %d\n",
           win.frame.top_left.x, win.frame.bottom_right.y);
    printf("  win.id = %d, win.layer = %d\n", win.id, win.layer);

    /* 拿内层的地址：指针可以只指向"结构体里的一小块"，这是合法的 */
    Point *corner = &win.frame.top_left;
    corner->x = 999;
    printf("  取内层地址 Point *corner = &win.frame.top_left; corner->x = 999;\n");
    printf("  现在 win.frame.top_left = (%d,%d) —— 通过内层指针真的改到了三层里面的那一格\n",
           win.frame.top_left.x, win.frame.top_left.y);
    printf("  嵌套的 sizeof 是各部分相加：sizeof(Rect)=%zu, sizeof(Window)=%zu\n",
           size_of_rect(), sizeof(Window));
    printf("  ⚠ 嵌套初始化花括号一层都不能少。少写一层，编译器会把数字塞错位置。\n");
    printf("    嫌麻烦就用指定成员初始化：win.frame.top_left = (Point){10,10}; 一眼看清。\n");

    /* ============================================================
       收尾
       ============================================================ */
    printf("\n=== 本节结束 ===\n");
    printf("记住四句话：\n");
    printf("  1) typedef 只是起外号：起完之后的 Point 和 int 一样是完整类型名，不用带 struct。\n");
    printf("  2) 手里是东西用 `.`，手里是地址用 `->`；p->x 就是 (*p).x 的简写。\n");
    printf("  3) 自引用结构 = 一个数据 + 一张写着下一个节点地址的纸条，这就是链表节点。\n");
    printf("  4) 想改到调用者手里的那份，只能传指针；传结构体 = 整块拷贝一份复印件。\n");
    printf("下一节 dynamic-memory：把这些节点用 malloc 一个一个造出来。\n");
    return 0;
}
