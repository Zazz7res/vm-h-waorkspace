#include <stdio.h>
#include <stddef.h>   /* ptrdiff_t */
#include <string.h>   /* strcmp：解析命令行开关 */
#include "two-dimensional.h"

/* 小工具：打印一行分隔线（和第 00 节同款） */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/*
 * 把「表达式 / 类型 / 地址 / 走一步多少字节」打成一张四行小表。
 * 为什么要单独写个函数？因为第 2 幕要连着问四个不同种类的"同一个地址"，
 * 表格里一眼就能看出：值全都一样，只有【类型】不一样 —— 而类型决定了走一步多远。
 * from / one_past 都是已经算好的地址，函数只负责把差值打成十进制字节。
 */
static void print_identity_row(const char *expr, const char *type,
                               const void *addr, long long step_bytes, int value_at_first)
{
    printf("  %-8s %-18s %-16p +1 之后走 %3lld 字节   首格内容 = %d\n",
           expr, type, addr, step_bytes, value_at_first);
}

int main(int argc, char **argv)
{
    /* 本节全场用的同一块"静态二维数组"：2 行 3 列，一共 6 个 int。
     * 它必须待在 main 这个作用域里 —— 只有在这里，数组的长度信息才还在（第 4 幕会演示它怎么丢）。 */
    int m[DEMO_ROWS][DEMO_COLS] = {
        {1, 2, 3},
        {4, 5, 6}
    };

    /* ============================================================
       第 1 幕：int m[2][3] 到底是"6 格平地"还是"两个数组"
       ============================================================ */
    section("第 1 幕：`int m[2][3]` 在内存里是一块 6 格的平地（不是数组的数组）");

    printf("  先把 6 个格子按【物理地址顺序】摊开，同时标上它是第几行第几列：\n");
    printf("    线性:      0      1      2      3      4      5\n");
    printf("    行列:  [0][0] [0][1] [0][2] [1][0] [1][1] [1][2]\n");
    printf("    内容:     %d      %d      %d      %d      %d      %d\n",
           m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2]);
    printf("    地址: ");
    for (int k = 0; k < DEMO_ROWS * DEMO_COLS; k++) {
        printf("%7p ", (void *)&m[0][0] + k);   /* 指针 + k 就是 + k 个 int，因为元素是 int */
    }
    printf("\n");

    printf("\n  三行数字就能把结论钉死：\n");
    printf("    ① 相邻两格地址差 = %lld 字节 = sizeof(int) —— 从头到尾一个样，中间【没有】行分隔线；\n",
           (long long)((const char *)&m[0][1] - (const char *)&m[0][0]));
    printf("    ② 第 0 行最后一格 &m[0][2]=%p 和第 1 行第一格 &m[1][0]=%p 相差 %lld 字节：\n",
           (void *)&m[0][2], (void *)&m[1][0],
           (long long)((const char *)&m[1][0] - (const char *)&m[0][2]));
    printf("       → 跨行和跨列一样便宜，因为压根就是同一排格子，只是下标换算换了个写法；\n");
    printf("    ③ sizeof(m) = %zu = 2*3*%zu；sizeof(m[0]) = %zu（一整行）；sizeof(m[0][0]) = %zu。\n",
           sizeof(m), sizeof(int), sizeof(m[0]), sizeof(m[0][0]));
    printf("  所以「行」和「列」是什么？是【读法】：m[r][c] ≡ *(&m[0][0] + r*%d + c)，乘的是列数。\n",
           DEMO_COLS);
    printf("  这一条也解释了为什么 C 的二维数组必须是矩形（每行同样长）——\n");
    printf("  换算公式里只有一个乘数，装不下「每行长度不一样」这种事。\n");

    /* ============================================================
       第 2 幕：四个都像"m"的表达式 —— 本节的核心
       ============================================================ */
    section("第 2 幕：&m[0][0] / m[0] / m / &m 四个表达式的真实地址（值一样，类型不一样）");

    printf("  %-8s %-18s %-16s %s\n", "表达式", "类型", "地址", "走一步（+1）意味着");
    printf("  ------------------------------------------------------------------\n");
    print_identity_row("&m[0][0]", "int *", (void *)&m[0][0],
                       (long long)((const char *)(&m[0][0] + 1) - (const char *)&m[0][0]),
                       m[0][0]);
    print_identity_row("m[0]", "int *（第 0 行退化）", (void *)m[0],
                       (long long)((const char *)(m[0] + 1) - (const char *)m[0]),
                       m[0][0]);
    print_identity_row("m", "int (*)[3]（整行是单位）", (void *)m,
                       (long long)((const char *)(m + 1) - (const char *)m),
                       m[0][0]);
    print_identity_row("&m", "int (*)[2][3]（整个矩阵是单位）", (void *)&m,
                       (long long)((const char *)(&m + 1) - (const char *)&m),
                       m[0][0]);

    printf("\n  看第一列的地址：四个数字【完全相同】= %p —— 它们都指向这块平地的开头。\n",
           (void *)&m[0][0]);
    printf("  但第三行和第四行的「+1 之后」完全不同：\n");
    printf("    m+1     = %p（跨了一整行 = %d 格 * %zu = %zu 字节）\n",
           (void *)(m + 1), DEMO_COLS, sizeof(int), (size_t)DEMO_COLS * sizeof(int));
    printf("    &m+1    = %p（跨了整个矩阵 = %zu 字节，下一块地皮了）\n",
           (void *)(&m + 1), sizeof(m));
    printf("    m[0]+1  = %p（只跨一格 = %zu 字节）\n", (void *)(m[0] + 1), sizeof(int));
    printf("  一句话记法：地址值看「指向哪儿」，类型看「走一步多大」。这俩是两件事，别混。\n");

    printf("\n  sizeof 的连锁反应（第 00 节 pointer-basics 那条规律在二维上的版本）：\n");
    printf("    sizeof(m)      = %zu 字节（整个矩阵，因为这里 m 是【真数组】）\n", sizeof(m));
    printf("    sizeof(m[0])   = %zu 字节（一整行）\n", sizeof(m[0]));
    printf("    sizeof(&m[0][0]) = %zu 字节（一个指针，和第 00 节一样）\n", sizeof(&m[0][0]));
    printf("    sizeof(m) / sizeof(m[0]) = %d → 行数；sizeof(m[0]) / sizeof(m[0][0]) = %d → 列数。\n",
           (int)(sizeof(m) / sizeof(m[0])), (int)(sizeof(m[0]) / sizeof(m[0][0])));
    printf("  两个除法都能算，是因为还站在数组出生的作用域里 —— 第 4 幕一过这道门就全废了。\n");

    /* ============================================================
       第 3 幕：int (*p)[3] 和 int *p[3] —— 长得像，意思相反
       ============================================================ */
    section("第 3 幕：行指针 int (*p)[3] vs 指针数组 int *p[3]");

    {
        /* 行指针：括号不能省。`int *p[3]` 会被读成"3 个 int*"，因为 [] 优先级比 * 高。 */
        int (*rowp)[DEMO_COLS] = m;
        printf("  【A】int (*rowp)[%d] = m;   —— 指向「一整行 3 个 int」那种东西的指针\n", DEMO_COLS);
        printf("        sizeof(rowp) = %zu（指针本身），sizeof(*rowp) = %zu（一整行）\n",
               sizeof(rowp), sizeof(*rowp));
        show_row_pointer(rowp, DEMO_ROWS);
        printf("        用 rowp 读第 1 行第 2 列：rowp[1][2] = %d，地址 %p\n",
               rowp[1][2], (void *)&rowp[1][2]);
        printf("        和原生写法比：&m[1][2] = %p —— 同一个地址，同一块内存，两种写法而已。\n",
               (void *)&m[1][2]);

        /* 指针数组：3 格，每格装一个 int*，指向哪儿完全自由 —— 它不是矩阵 */
        int x = 111, y = 222, z = 333;
        int *parr[DEMO_COLS] = { &x, &y, &z };
        printf("\n  【B】int *parr[%d] = {&x,&y,&z};  —— 3 个【指针】排成一排\n", DEMO_COLS);
        printf("        在数组出生的作用域里：sizeof(parr) = %zu = %d * %zu（真的 3 格指针）\n",
               sizeof(parr), DEMO_COLS, sizeof(int *));
        printf("        x=%p y=%p z=%p（三个各自独立的局部变量）\n",
               (void *)&x, (void *)&y, (void *)&z);
        show_array_of_pointers(parr);
        printf("        ⚠ parr 也能写成 parr[1][0] 吗？不能 —— parr[1] 已经是 int，再 [0] 就是拿 int 当下标。\n");
        printf("        所以「int** 看着像二维」的那套写法，本质是【先查表再跳地址】，不是二维数组。\n");
    }

    /* ============================================================
       第 4 幕：进函数就丢维度 —— 三种合法签名
       ============================================================ */
    section("第 4 幕：把 m 传进函数：列数必须写在类型里，行数只是记账");

    printf("  ① 行指针版（最正规，一眼看得出参数是「一整行的指针」）：\n");
    pass_2d_row_pointer(m, DEMO_ROWS, "  [①]");
    printf("\n  ② 数组写法、行数留空（和①是同一个类型，只是糖）：\n");
    pass_2d_omitted_rows(m, DEMO_ROWS, "  [②]");
    printf("\n  ③ 摊平版（列数改成运行时参数，代价是自己写换算式）：\n");
    pass_2d_flat(&m[0][0], DEMO_ROWS, DEMO_COLS, "  [③]");

    printf("\n  三种写法读的都是同一块 %zu 字节，只是「谁负责知道列数」不同：\n", sizeof(m));
    printf("    ①② 列数写在类型里 → 编译器帮你算 r*cols+c；行数必须自己传（sizeof 也帮不了你）；\n");
    printf("    ③   行列全自己传 → 函数签名最简单，但每个下标都要手写 r*cols+c，写错就是错一整片。\n");
    printf("  ⚠ 反面说法（这一句故意【不编译】，所以只写在注释里）：\n");
    printf("      void wrong(int **mm) 接收不了 m —— m 退化后是 int (*)[3]，不是 int **；\n");
    printf("      这两者中间隔着「一层真实存在的指针表」，硬转之后读 mm[1] 读的是第 4 个 int，不是第 1 行。\n");

    /* ============================================================
       第 5 幕：动态二维 A —— 一次 malloc 的真连续矩阵
       ============================================================ */
    section("第 5 幕：动态二维之一：malloc(rows*cols*sizeof(int)) 一整块，靠 r*cols+c 换算");

    {
        FlatMatrix fm;
        flat_matrix_init(&fm, 3, 4);            /* 3 行 4 列 = 12 格，一次 malloc */
        printf("  涂过毒的样子（还没 set，全是 -9000-线性下标）：\n");
        flat_matrix_print(&fm, "  刚 init");

        int value = 0;
        for (int r = 0; r < fm.rows; r++) {
            for (int c = 0; c < fm.cols; c++) {
                value++;
                flat_matrix_set(&fm, r, c, value * 10);
            }
        }
        flat_matrix_print(&fm, "  填完 120..40");
        printf("\n  换算验算：flat_matrix_index(&fm, 2, 1) = %d（= 2*cols + 1 = 2*%d+1）\n",
               flat_matrix_index(&fm, 2, 1), fm.cols);
        printf("  第 3 行那一格 &data[%d] = %p，比 &data[8]（=m[2][0]）大 %zu 字节：\n",
               9, (void *)&fm.data[9], sizeof(int));

        printf("\n  地址地图（按物理顺序摊开）—— 这就是「一整块」长什么样：\n");
        flat_matrix_print_cell_map(&fm, "物理地图");

        printf("\n  越界检查版 API：\n");
        int out = -1;
        if (flat_matrix_get(&fm, 1, 2, &out)) {
            printf("    get(1,2) 合法 → %d\n", out);
        }
        flat_matrix_get(&fm, 3, 0, &out);       /* 行越界：被拒绝 */
        flat_matrix_set(&fm, 0, 4, 999);        /* 列越界：被拒绝 */
        printf("    对照：不设防的写法就是 fm.data[r*cols+c]，写错了没人拦（第 4 幕③）。\n");

        flat_matrix_free(&fm);
    }

    /* ============================================================
       第 6 幕：动态二维 B —— int** 指针表 + 每行一次 malloc
       ============================================================ */
    section("第 6 幕：动态二维之二：int** 指针表（1 张表 + rows 次 malloc）");

    {
        TableMatrix tm;
        table_matrix_init(&tm, 3, 4);
        for (int r = 0; r < tm.rows; r++) {
            for (int c = 0; c < tm.cols; c++) {
                tm.row[r][c] = (r + 1) * 10 + c;
            }
        }
        table_matrix_print(&tm, "  填完之后的表格视图");
        printf("\n  和连续版最关键的差别，看这两张图：\n");
        table_matrix_print_row_table(&tm, "指针表地图");
        printf("\n  读一个格子要走【两跳】：先读表里的 row[1]（%p），再按列偏移跳到 %p。\n",
               (void *)tm.row[1], (void *)&tm.row[1][2]);
        printf("  值 = %d；这一跳在连续版里根本不存在（那里只有一跳）。\n", tm.row[1][2]);
        printf("\n  好处也要说公道：每行长度可以【不一样】（锯齿数组 jagged array），\n");
        printf("  连续版做不到 —— 因为它的换算式里只有一个 cols 乘数。\n");

        printf("\n  归还：顺序必须是【先每行，再表】（下面每一行 step 就是真实顺序）：\n");
        table_matrix_free(&tm);
        table_matrix_free(&tm);   /* 再 free 一次：已经置空，幂等，什么都不做 */
    }

    /* ============================================================
       第 7 幕：两种动态二维的账单对比
       ============================================================ */
    section("第 7 幕：FlatMatrix vs int** —— 该用哪个，把账摆开算");

    {
        int rows = 3, cols = 4;
        size_t cells = (size_t)rows * (size_t)cols;
        printf("  ┌────────────────────┬────────────────────┬────────────────────┐\n");
        printf("  │ 项目               │ FlatMatrix（连续） │ int **（指针表）   │\n");
        printf("  ├────────────────────┼────────────────────┼────────────────────┤\n");
        printf("  │ malloc 次数        │ %18d │ %18d │\n", 1, rows + 1);
        printf("  │ free 次数          │ %18d │ %18d │\n", 1, rows + 1);
        printf("  │ 数据字节           │ %18zu │ %18zu │\n", cells * sizeof(int),
               cells * sizeof(int) + (size_t)rows * sizeof(int *));
        printf("  │ 读一格要跳几次地址 │ %18s │ %18s │\n", "1 跳", "2 跳（查表再跳）");
        printf("  │ 行与行是否保证连续 │ %18s │ %18s │\n", "保证（就一块）", "不保证");
        printf("  │ 能否整块 memcpy    │ %18s │ %18s │\n", "能", "不能（只能逐行）");
        printf("  │ 每行长度能否不同   │ %18s │ %18s │\n", "不能", "能（锯齿数组）");
        printf("  └────────────────────┴────────────────────┴────────────────────┘\n");
        printf("  上面「数据字节」那一行的差值 = %d 行 * %zu 字节/指针 = %zu 字节，就是【表本身】的开销：\n",
               rows, sizeof(int *), (size_t)rows * sizeof(int *));
        printf("  结论（也是这一节工程上的实用建议）：\n");
        printf("    矩形矩阵 → 一律连续版。少 %d 次 malloc、少 %d 次 free、能整块拷贝、缓存友好；\n",
               rows, rows + 1);
        printf("    真要锯齿（每行长度不同，比如邻接表、三角形）→ 才用 int** 或一维 + 每行起始下标。\n");
        printf("    注意 C 里 int** 从来【不是】二维数组的指针；它是「指针表」，两回事。\n");
    }

    /* ============================================================
       第 8 幕：缓存友好度实测（同一块内存，两种读法）
       ============================================================ */
    section("第 8 幕：行优先 vs 列优先 —— clock_gettime 实测，数字自己看");

    printf("  两个循环读的格子数【完全一样】，唯一区别是顺序。所以这不是复杂度之争，是常数之争：\n");
    bench_row_vs_col_major(CACHE_TEST_N, CACHE_TEST_ROUNDS);

    /* ============================================================
       第 9 幕（开关才触发）：free 顺序反一反会怎样
       ============================================================ */
    section("第 9 幕：free 顺序反一反 —— 指针表先没，行就再也找不回来");

    if (argc > 1 && strcmp(argv[1], "--show-wrong-free") == 0) {
        printf("  你传了 --show-wrong-free，现在【故意】先 free 指针表，再去读它。\n");
        TableMatrix tm;
        table_matrix_init(&tm, 2, 3);
        tm.row[0][0] = 42;
        table_matrix_free_wrong_order(&tm);     /* 内含 fflush(stdout)：先把话说完再让 ASan 动手 */
        printf("  如果这一行打出来了，说明这次没被 ASan 抓到 —— 但「读废表」这件事已经做了。\n");
        table_matrix_free(&tm);                 /* 幂等：row 已经是 NULL，什么都不做 */
        printf("  至于那 2 行内存：线索全断，谁也还不回来 → 泄漏（ASan 会顺手报出来）。\n");
    } else {
        printf("  这一段的正确姿势（第 6 幕已经跑过）：先 free 每一行，最后 free 表。\n");
        printf("  想亲眼看反例（ASan 报 heap-use-after-free，进程非 0 退出）：\n");
        printf("      gcc -Wall -Wextra -Wshadow -std=c11 -g -fsanitize=address -static-libasan \\\n");
        printf("          01-array/two-dimensional/*.c -o /tmp/verify_arr_3 && /tmp/verify_arr_3 --show-wrong-free\n");
        printf("  为什么它不是「代码风格问题」：因为 free 之后那块地址【还在指针里】，\n");
        printf("  数字看起来完全正常，读它不会立刻崩 —— 它会等到某天给你一个匪夷所思的值。\n");
    }

    /* ============================================================
       收尾
       ============================================================ */
    section("收尾：这一节到底在记什么");
    printf("  本 main 里 FlatMatrix 和 TableMatrix 都在自己的作用域结束时 free 过了，\n");
    printf("  所以默认运行 ASan 必须报 0 泄漏 —— 这是交付闸门，不是可选项。\n");
    printf("\n记住五句话：\n");
    printf("  1) C 没有二维内存，只有一维格子 + 换算式 m[r][c] ≡ base[r*cols+c]；「行优先」就是这个乘数造成的。\n");
    printf("  2) &m[0][0]、m[0]、m、&m 四个地址数字相同，类型不同：类型决定 +1 走 %d 还是 %d 还是 %d 字节。\n",
           (int)sizeof(int), (int)sizeof(m[0]), (int)sizeof(m));
    printf("  3) 传进函数：列数必须写在类型里（int (*)[3] / int [][3]），行数另外传；列数运行时才知道就摊平用 int*。\n");
    printf("  4) int (*p)[3] 是「指向一行的指针」，int *p[3] 是「3 个指针的数组」，长得像，+1 差一倍以上。\n");
    printf("  5) 动态两种做法：连续版 1 次 malloc、free 顺序无所谓、缓存友好；int** 版 rows+1 次，必须先 free 行再 free 表。\n");
    return 0;
}
