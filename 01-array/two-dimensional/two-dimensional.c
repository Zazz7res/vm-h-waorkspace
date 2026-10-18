#define _POSIX_C_SOURCE 200809L   /* 这行必须在所有 #include 之前：-std=c11 是严格模式，
                                     不声明它就看不见 POSIX 的函数（本节用 clock_gettime）。
                                     放在文件最顶端而不是 main.c 里，是因为计时函数就写在这里。 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "two-dimensional.h"

/* ===================================================================
 * 0. 小工具
 * =================================================================== */

/*
 * 两地址间差多少字节？为什么要绕一圈转成 char*（再转成 ptrdiff_t）？
 *   因为"指针减指针"只有在【同一个数组里】才有定义，而且减出来的是"元素个数"不是字节数；
 *   想问"隔几格 = 隔几字节"，最稳的写法就是先看成字节（char*），再相减。
 *   本节到处要回答"m+1 到底走了多远"，全靠这个函数把答案打成十进制字节数。
 */
static long long byte_delta(const void *from, const void *to)
{
    return (long long)((const char *)to - (const char *)from);
}

static long long now_nanoseconds(void)
{
    struct timespec ts;
    /* CLOCK_MONOTONIC：单调钟，不受系统时间被改走的影响，量"耗时"只能用这类钟。
     * 别用 time(NULL)（只有秒）或 CLOCK_REALTIME（管理员改表就穿帮）。 */
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
}

/* ===================================================================
 * 1. FlatMatrix —— 一次 malloc 的"真连续"二维
 * =================================================================== */

/*
 * 为什么一次 malloc(rows*cols*sizeof(int)) 就能当二维数组用？
 *   因为二维本来就不需要两块内存 —— 只要"每行格数"固定，一排连续的格子
 *   就能用 r*cols+c 换算出任何一格的偏移。反过来说：只要行列都能算，
 *   一整块平地就是最省事的形状。
 *
 * 注意 sizeof 里那个 (size_t)：rows*cols 是 int 相乘，直接乘进 sizeof 表达式
 * 在很大时会先溢出；先转成 size_t 再乘，是工程里的固定写法。
 */
bool flat_matrix_init(FlatMatrix *m, int rows, int cols)
{
    if (m == NULL || rows <= 0 || cols <= 0) {
        printf("      [拒绝] flat_matrix_init：rows=%d cols=%d 必须是正数\n",
               rows, cols);
        if (m != NULL) {
            m->data = NULL;
            m->rows = 0;
            m->cols = 0;
        }
        return false;
    }

    size_t cells = (size_t)rows * (size_t)cols;
    m->data = malloc(cells * sizeof(int));
    if (m->data == NULL) {
        printf("      [错误] malloc %zu 格失败，结构体保持全空（可以安全 free）\n", cells);
        m->rows = 0;
        m->cols = 0;
        return false;
    }
    m->rows = rows;
    m->cols = cols;

    /* 涂毒：第 k 格（线性下标）= -9000-k。读到负数就说明"这块还没被 set 过" */
    for (size_t k = 0; k < cells; k++) {
        m->data[k] = -9000 - (int)k;
    }

    printf("      flat_matrix_init(%d, %d)：一次 malloc %zu 格 = %zu 字节，入口 %p\n",
           rows, cols, cells, cells * sizeof(int), (void *)m->data);
    printf("      （整块地皮一次租下，第 %d 行首格 = 入口 + %d 行 * %d 列 * %zu 字节 = +%zu 字节）\n",
           1, 1, cols, sizeof(int), (size_t)cols * sizeof(int));
    return true;
}

/*
 * free 的规矩和上一节一模一样：归还 → 置 NULL → 账面归零。
 * 连续版只有一处内存要还，所以【不可能记错 free 顺序】—— 这就是它的第一条好处。
 */
void flat_matrix_free(FlatMatrix *m)
{
    if (m == NULL) {
        return;
    }
    if (m->data != NULL) {
        printf("      flat_matrix_free：free(%p)，一次归还 %d*%d*%zu = %zu 字节\n",
               (void *)m->data, m->rows, m->cols, sizeof(int),
               (size_t)m->rows * (size_t)m->cols * sizeof(int));
        free(m->data);
    } else {
        printf("      flat_matrix_free：data 已经是 NULL，free(NULL) 是合法空操作（幂等）\n");
    }
    m->data = NULL;
    m->rows = 0;
    m->cols = 0;
}

int flat_matrix_index(const FlatMatrix *m, int r, int c)
{
    /* 这一行就是"二维不存在"的证据：换算成线性下标而已 */
    return r * m->cols + c;
}

bool flat_matrix_set(FlatMatrix *m, int r, int c, int value)
{
    if (m == NULL || m->data == NULL) {
        printf("      [拒绝] set(%d,%d)：矩阵是空的\n", r, c);
        return false;
    }
    if (r < 0 || r >= m->rows || c < 0 || c >= m->cols) {
        printf("      [拒绝] set(%d,%d)：合法范围是 行 0..%d、列 0..%d —— 检查版就把你拦住\n",
               r, c, m->rows - 1, m->cols - 1);
        return false;
    }
    m->data[flat_matrix_index(m, r, c)] = value;
    return true;
}

bool flat_matrix_get(const FlatMatrix *m, int r, int c, int *out)
{
    if (m == NULL || m->data == NULL || r < 0 || r >= m->rows || c < 0 || c >= m->cols) {
        printf("      [拒绝] get(%d,%d)：越界或空矩阵，*out 保持原值不动\n", r, c);
        return false;
    }
    if (out != NULL) {
        *out = m->data[flat_matrix_index(m, r, c)];
    }
    return true;
}

/* 排成表格好看一点：这一版【看不出】内存形状，所以才还要下面那版地址地图 */
void flat_matrix_print(const FlatMatrix *m, const char *tag)
{
    if (m == NULL || m->data == NULL) {
        printf("%s: (空矩阵)\n", tag);
        return;
    }
    printf("%s（%d 行 %d 列，表格视图 —— 注意这只是「读法」，不是内存形状）\n",
           tag, m->rows, m->cols);
    for (int r = 0; r < m->rows; r++) {
        printf("        行%d: ", r);
        for (int c = 0; c < m->cols; c++) {
            printf("%6d", m->data[flat_matrix_index(m, r, c)]);
        }
        printf("\n");
    }
}

/*
 * 地址地图：按【物理顺序】（也就是线性下标 0,1,2,...）把每格摊开一行，
 * 同时标出"这格在行列坐标里是谁"。跑一遍就知道：
 *   物理顺序先变的是【列】，行是每 cols 格才进一位 —— 这就是"行优先"（row-major）。
 */
void flat_matrix_print_cell_map(const FlatMatrix *m, const char *tag)
{
    if (m == NULL || m->data == NULL) {
        printf("  %s: 没有内存可画\n", tag);
        return;
    }
    size_t cells = (size_t)m->rows * (size_t)m->cols;
    printf("  %s：data = %p，共 %zu 格 = %zu 字节（行优先：同一行内地址 +4，跨行时跳 %zu 字节）\n",
           tag, (void *)m->data, cells, cells * sizeof(int),
           (size_t)m->cols * sizeof(int));
    printf("        线性下标 | 行列身份  | 地址            | 相对入口 | 值\n");
    printf("        ---------+-----------+-----------------+----------+--------\n");
    for (size_t k = 0; k < cells; k++) {
        int r = (int)(k / (size_t)m->cols);
        int c = (int)(k % (size_t)m->cols);
        /* 行号是 %zu，这里只画得下小的演示矩阵；大方阵别调这个函数，会把屏幕刷爆 */
        printf("        %8zu | m[%d][%d]  | %-15p | +%6zu   | %d\n",
               k, r, c, (void *)&m->data[k], k * sizeof(int), m->data[k]);
    }
}

/*
 * 行优先求和：内层 c 连着涨 → 内存地址一格一格顺着走 → 缓存行一次装载就能连用好几次。
 * 列优先求和：内层 r 连着涨 → 每步跳 cols*sizeof(int) 字节 → 每一步几乎都是新缓存行。
 * 两个函数【读的元素总数完全一样】，唯一区别是读的顺序 —— 这正是第 8 幕要量的东西。
 */
long long flat_matrix_sum_row_major(const FlatMatrix *m)
{
    long long sum = 0;
    for (int r = 0; r < m->rows; r++) {
        for (int c = 0; c < m->cols; c++) {
            sum += m->data[r * m->cols + c];
        }
    }
    return sum;
}

long long flat_matrix_sum_col_major(const FlatMatrix *m)
{
    long long sum = 0;
    for (int c = 0; c < m->cols; c++) {          /* 外层换成列 */
        for (int r = 0; r < m->rows; r++) {      /* 内层换成行 → 每次访问跳一整行 */
            sum += m->data[r * m->cols + c];
        }
    }
    return sum;
}

/*
 * ===================================================================
 * 1b. 缓存友好度实测：同一块内存，两种读法
 * ===================================================================
 * 为什么要自己计时？因为"行优先快"这句话光靠嘴说没意义，而且要清楚它【不是算法复杂度】
 * 层面的差别：两个循环读的格子数一模一样，都是 O(rows*cols)。
 * 差的是【常数】：缓存行（通常 64 字节 = 16 个 int）是被整块装进来的，
 *   行优先：装一次缓存行，后面 15 次访问都白嫖 → 每 16 格才 miss 一次；
 *   列优先：每格都在自己的新缓存行上（步长 = cols*4 字节，1000 列就是 4000 字节，
 *           远超一行 64 字节）→ 基本每格 miss 一次 → 内存带宽被拉满。
 * 所以差距是"十几次"的量级，而不是"n 倍"的量级 —— 数字打出来自己看。
 *
 * ⚠ 一句诚实的话：闸门那遍是 -g + ASan，ASan 每次访问都要查影子内存，
 *   这份固定开销会把缓存差距【稀释】得比较小；换 -O2 不带 ASan 再跑，比值会明显拉大。
 *   本节只报实测数字，不替实测数字吹牛。
 */
void bench_row_vs_col_major(int n, int rounds)
{
    FlatMatrix big;
    if (!flat_matrix_init(&big, n, n)) {
        return;
    }

    /* 先把整块内存按行优先填一遍真实值：让"第一次碰页"的成本发生在填数阶段，
     * 而不是混进下面任何一次计时里 */
    for (int r = 0; r < n; r++) {
        for (int c = 0; c < n; c++) {
            big.data[(size_t)r * n + c] = r + c;
        }
    }

    printf("      矩阵 %d x %d = %d 格 = %zu MB（每格 %zu 字节）；一条缓存行 64 字节 = %zu 个 int\n",
           n, n, n * n, (size_t)(n * n) * sizeof(int) / (1024 * 1024), sizeof(int),
           (size_t)64 / sizeof(int));

    long long best_row_ns = -1;
    long long best_col_ns = -1;
    long long sum_row = 0;
    long long sum_col = 0;

    for (int k = 0; k < rounds; k++) {
        long long t0 = now_nanoseconds();
        sum_row = flat_matrix_sum_row_major(&big);
        long long t1 = now_nanoseconds();

        long long t2 = now_nanoseconds();
        sum_col = flat_matrix_sum_col_major(&big);
        long long t3 = now_nanoseconds();

        long long row_ns = t1 - t0;
        long long col_ns = t3 - t2;
        if (best_row_ns < 0 || row_ns < best_row_ns) {
            best_row_ns = row_ns;
        }
        if (best_col_ns < 0 || col_ns < best_col_ns) {
            best_col_ns = col_ns;
        }
        printf("      第 %d 轮：行优先 %8.3f ms   列优先 %8.3f ms   本轮比值 %.2fx\n",
               k + 1, (double)row_ns / 1e6, (double)col_ns / 1e6,
               (double)col_ns / (double)row_ns);
    }

    printf("      取最快一轮：行优先 %.3f ms，列优先 %.3f ms → 【列优先慢 %.2f 倍】\n",
           (double)best_row_ns / 1e6, (double)best_col_ns / 1e6,
           (double)best_col_ns / (double)best_row_ns);
    printf("      两个和：行优先 %lld，列优先 %lld —— 必然相等，因为读的是同一块内存，只是顺序不同\n",
           sum_row, sum_col);
    printf("      所以这不是「快慢算法」的选择，而是「顺着内存的脾气读」还是「横着读」的选择：\n");
    printf("      同样的 O(n^2) 复杂度，常数能差好几倍 —— 真实项目里图像/矩阵代码全都要考虑这一条。\n");

    flat_matrix_free(&big);
}

/* ===================================================================
 * 2. TableMatrix —— int** 指针表 + 每行一次 malloc
 * =================================================================== */

/*
 * 申请次数：1 张表 + rows 行 = rows+1 次 malloc。这就是教科书图"数组的数组"的真实成本。
 * 每一行各自成一块，行与行之间【不保证相邻】—— 分配器可能把它们排在一起（看着像连续的！），
 * 也可能中间塞了别的东西。所以任何"跨行整块 memcpy/整块排序"的想法在这版里都是错的。
 *
 * 失败处理用 goto 风格的一次性回收：前面第几行已经 malloc 成功了，
 * 后面失败时必须把已租的还掉，否则就是漏。这一条是 int** 最容易写错的地方。
 */
bool table_matrix_init(TableMatrix *m, int rows, int cols)
{
    if (m == NULL || rows <= 0 || cols <= 0) {
        printf("      [拒绝] table_matrix_init：rows=%d cols=%d 必须是正数\n", rows, cols);
        if (m != NULL) {
            m->row = NULL;
            m->rows = 0;
            m->cols = 0;
        }
        return false;
    }

    m->row = malloc((size_t)rows * sizeof(int *));   /* 表里存的是指针，不是 int */
    if (m->row == NULL) {
        printf("      [错误] 指针表 malloc 失败\n");
        m->rows = 0;
        m->cols = 0;
        return false;
    }
    m->rows = rows;
    m->cols = cols;

    for (int i = 0; i < rows; i++) {
        m->row[i] = NULL;      /* 先把没租的行置 NULL：回滚时 free(NULL) 是安全的 */
    }

    for (int i = 0; i < rows; i++) {
        m->row[i] = malloc((size_t)cols * sizeof(int));
        if (m->row[i] == NULL) {
            printf("      [错误] 第 %d 行 malloc 失败，回滚已租的 %d 行 + 表\n", i, i);
            for (int j = 0; j < i; j++) {
                free(m->row[j]);
                m->row[j] = NULL;
            }
            free(m->row);
            m->row = NULL;
            m->rows = 0;
            m->cols = 0;
            return false;
        }
        for (int c = 0; c < cols; c++) {
            m->row[i][c] = -9000 - (i * cols + c);   /* 和连续版同款毒值，方便对照 */
        }
    }

    printf("      table_matrix_init(%d, %d)：一共 malloc %d 次（1 张表 + %d 行）\n",
           rows, cols, rows + 1, rows);
    return true;
}

/*
 * free 的正确顺序：先每行，再表。
 * 为什么？指针表是"找到各行"的唯一线索。表先没了，行就再也找不回来（漏）；
 * 反过来行先没了，表里立刻就是几枚废地址 —— 但只要【马上】把表也还掉，就没有机会误用。
 * 所以顺序不是审美问题，是"线索必须活到它指的东西都处理完"。
 */
void table_matrix_free(TableMatrix *m)
{
    if (m == NULL) {
        return;
    }
    if (m->row == NULL) {
        printf("      table_matrix_free：已经是空表，什么都不用还（幂等）\n");
        return;
    }
    for (int i = 0; i < m->rows; i++) {
        printf("      step1 free(第%d行 %p)，归还 %d*%zu = %zu 字节\n",
               i, (void *)m->row[i], m->cols, sizeof(int),
               (size_t)m->cols * sizeof(int));
        free(m->row[i]);
        m->row[i] = NULL;    /* 顺手置空：万一后面出错，也不会读到废地址 */
    }
    printf("      step2 free(指针表 %p)，归还 %d*%zu = %zu 字节\n",
           (void *)m->row, m->rows, sizeof(int *),
           (size_t)m->rows * sizeof(int *));
    free(m->row);
    m->row = NULL;
    m->rows = 0;
    m->cols = 0;
}

/*
 * ⚠ 反面教材：顺序反一反 —— 先 free 指针表，再去 free"表里记着的各行地址"。
 *   读 m->row[i] 的时候表已经还不回去了 → heap-use-after-free；
 *   而且各行到底在哪儿都已经没人知道了 → 泄漏。
 *   main 第 9 幕用命令行开关才调用它，默认运行绝不出现。
 */
void table_matrix_free_wrong_order(TableMatrix *m)
{
    if (m == NULL || m->row == NULL) {
        return;
    }
    printf("      【故意犯错】step1 先 free(指针表 %p)\n", (void *)m->row);
    void *saved_table = (void *)m->row;      /* 只存地址，用于事后打印，绝不解引用 */
    free(m->row);
    m->row = NULL;
    /* 上面已经把 row 置 NULL 了，所以这一句不会真的读到废地址（那是我们自己写的保险）。
     * 但正式代码里"先 free 表"之后还要按行归还，就必须读 row[i] —— 那才是事故现场。
     * 这里为了演示，刻意再把它当"还有效"来读一次。 */
    int **dangling = (int **)saved_table;    /* 表本来就是 int* 的数组，所以按 int** 去看它 */
    printf("      【故意犯错】step2 现在去读那张废表里的 row[0]：\n");
    fflush(stdout);                          /* ASan 一报错就当场终止，先把话打完 */
    /* ⚠ 下面这一行就是 use-after-free 现场，ASan 会立刻抓到： */
    void *row0_addr = (void *)dangling[0];
    printf("      读到了 %p —— 这块内存早还给分配器了，值能不能对纯属运气\n", row0_addr);
    /* 真凶还没完：各行内存此刻仍在堆上，但没有任何线索指向它们 → 泄漏 */
}

void table_matrix_print(const TableMatrix *m, const char *tag)
{
    if (m == NULL || m->row == NULL) {
        printf("%s: (空表)\n", tag);
        return;
    }
    printf("%s（%d 行 %d 列，int** 版）\n", tag, m->rows, m->cols);
    for (int r = 0; r < m->rows; r++) {
        printf("        行%d: ", r);
        for (int c = 0; c < m->cols; c++) {
            printf("%6d", m->row[r][c]);
        }
        printf("\n");
    }
}

/*
 * 把"指针表 + 每行"两件事分开画 —— 这一张图就是 int** 的全部真相：
 *   表里每格存的是一个地址（所以一格 %zu 字节，不是 4 字节）；
 *   相邻两行的地址差 = ？如果刚好等于 cols*sizeof(int)，那是分配器凑巧，不能依赖。
 */
void table_matrix_print_row_table(const TableMatrix *m, const char *tag)
{
    if (m == NULL || m->row == NULL) {
        printf("  %s: 没有内存可画\n", tag);
        return;
    }
    printf("  %s：指针表 = %p（%d 格，每格 %zu 字节 = sizeof(int*)，共 %zu 字节）\n",
           tag, (void *)m->row, m->rows, sizeof(int *),
           (size_t)m->rows * sizeof(int *));
    printf("        表下标 | 表地址          | 表里存的值（= 该行首地址） | 该行 3 个格子的地址\n");
    printf("        -------+-----------------+----------------------------+-----------------------\n");
    for (int i = 0; i < m->rows; i++) {
        printf("        row[%d]    | %-15p | %-26p |", i, (void *)&m->row[i], (void *)m->row[i]);
        for (int c = 0; c < m->cols; c++) {
            printf(" %p", (void *)&m->row[i][c]);
        }
        printf("\n");
    }
    if (m->rows >= 2) {
        long long gap = byte_delta(m->row[0], m->row[1]);
        printf("        相邻两行首地址相差 %lld 字节；一行 = %d*%zu = %zu 字节 —— %s\n",
               gap, m->cols, sizeof(int), (size_t)m->cols * sizeof(int),
               (gap == (long long)m->cols * (long long)sizeof(int))
                   ? "刚好相等是【分配器凑巧】，不是语言保证，别写依赖它的代码"
                   : "对不上，这就证明了两行之间夹着别的东西");
    }
    printf("        结论：int** 是【两段东西】：一张表 + rows 条独立的小数组，行内连续、行间不保证。\n");
}

/* ===================================================================
 * 3. 传进函数的三种签名
 * =================================================================== */

/*
 * 签名里那个 [3] 不是装饰：编译器要靠它算出"下一行在哪儿"。
 * 所以打印 (m+1) 和 m 的字节差，答案永远是 12 = cols*sizeof(int)，
 * 而且这里的 rows 参数只是"循环几次"，删掉也不会影响换算 —— 分工就在这儿。
 */
void pass_2d_row_pointer(const int (*m)[DEMO_COLS], int rows, const char *tag)
{
    printf("  %s 签名 const int (*m)[%d]：m 的类型 = 指向 %d 个 int 的指针\n",
           tag, DEMO_COLS, DEMO_COLS);
    printf("        m=%p，m+1=%p → 走一步 %lld 字节（= 一整行 = %d*%zu）\n",
           (void *)m, (void *)(m + 1), byte_delta(m, m + 1),
           DEMO_COLS, sizeof(int));
    for (int r = 0; r < rows; r++) {
        printf("        m[%d]: ", r);
        for (int c = 0; c < DEMO_COLS; c++) {
            printf("%6d", m[r][c]);        /* 写成 m[r][c] 完全自然：行指针 + 列下标 */
        }
        printf("     &m[%d][0]=%p\n", r, (void *)&m[r][0]);
    }
}

/*
 * 这一版签名把行数留空 `int m[][3]` —— 合法，而且和上面【同一个类型】（数组当参数就退化）。
 * 想不通就记这句：行数可以省，列数绝对不行。
 */
void pass_2d_omitted_rows(int m[][DEMO_COLS], int rows, const char *tag)
{
    /* 先显式存成一个指针变量再 sizeof。
     * 直接写 sizeof(m) 会被 gcc 警告 -Wsizeof-array-argument："数组当参数就退化成指针了" ——
     * 那条警告本身就是本节的一条结论，所以这里换个写法把数字拿到手，同时不破坏零警告闸门。 */
    int (*decay)[DEMO_COLS] = m;
    printf("  %s 签名 int m[][%d]（行数留空，列数留着）：退化后和行指针版是同一类型\n",
           tag, DEMO_COLS);
    printf("        m=%p，m+1=%p → 走一步 %lld 字节；sizeof(decay) = %zu（是【指针】，不是矩阵！）\n",
           (void *)m, (void *)(m + 1), byte_delta(m, m + 1), sizeof(decay));
    printf("        但 sizeof(m[0]) = %zu —— 因为 m[0] 是【一整行】，行还没退化，列数还写在类型里\n",
           sizeof(m[0]));
    long long total = 0;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < DEMO_COLS; c++) {
            total += m[r][c];
        }
    }
    printf("        用这套签名把整块内存求和 = %lld（和第 1 版结果必然一样，读的是同一块地皮）\n", total);
}

/*
 * 列数运行时才知道，怎么办？只能摊平成 int* + 自己写换算式 r*cols+c。
 * 这也是所有真正的矩阵库/图像库的做法：内存是一维的，二维是约定。
 */
void pass_2d_flat(const int *data, int rows, int cols, const char *tag)
{
    printf("  %s 签名 const int *data + 运行时 cols：二维彻底变成「一维 + 换算公式」\n", tag);
    printf("        data=%p，data+1=%p → 走一步 %lld 字节（只能走一格，跨行要自己乘 cols）\n",
           (void *)data, (void *)(data + 1), byte_delta(data, data + 1));
    for (int r = 0; r < rows; r++) {
        printf("        行%d: ", r);
        for (int c = 0; c < cols; c++) {
            printf("%6d", data[r * cols + c]);      /* ← 这个乘法就是你替编译器补上的「列数」 */
        }
        /* 顺手把这一行最后一格的地址打出来：它 = data + (r*cols + cols-1)，纯算术，没有任何魔法 */
        int last_c = cols - 1;
        printf("     末格 (r=%d,c=%d) 地址 = %p\n", r, last_c, (void *)&data[r * cols + last_c]);
    }
}

/* ===================================================================
 * 4. int (*p)[3] vs int *p[3]
 * =================================================================== */

void show_row_pointer(const int (*p)[DEMO_COLS], int rows)
{
    printf("        int (*p)[%d]：sizeof(p) = %zu（它只是一个指针），sizeof(*p) = %zu（一整行 %d 个 int）\n",
           DEMO_COLS, sizeof(p), sizeof(*p), DEMO_COLS);
    printf("        p=%p，p+1=%p → %lld 字节；p 走一步 = 【跨一行】\n",
           (void *)p, (void *)(p + 1), byte_delta(p, p + 1));
    printf("        最后一格 p[%d][%d] 在 %p，第一格 p[0][0] 在 %p，跨度 %lld 字节 = %d 格 * %zu\n",
           rows - 1, DEMO_COLS - 1,
           (void *)&p[rows - 1][DEMO_COLS - 1], (void *)&p[0][0],
           byte_delta(&p[0][0], &p[rows - 1][DEMO_COLS - 1]),
           rows * DEMO_COLS, sizeof(int));
}

/*
 * int *p[3]：p 是【3 个 int* 排成一排】。它装的是地址，不是数据本身。
 * 所以它长得像二维数组的写法 p[r][c] 却根本不是矩阵 —— 这是新手最容易撞的墙。
 */
void show_array_of_pointers(int *p[DEMO_COLS])
{
    /* 同样：参数里的 p 已经是 int**，直接 sizeof(p) 会吃警告。
     * 这里用 decay 变量拿到 8，并且明说"在数组出生的作用域里 sizeof 才是 24"（main 里当场对照）。 */
    int **decay = p;
    printf("        int *p[%d] 当参数：退化成 int**，sizeof(decay) = %zu；一格 %zu 字节 = sizeof(int*)\n",
           DEMO_COLS, sizeof(decay), sizeof(*p));
    printf("        p=%p，p+1=%p → %lld 字节 = sizeof(int*)：走一步只跳【一格指针】\n",
           (void *)p, (void *)(p + 1), byte_delta(p, p + 1));
    for (int i = 0; i < DEMO_COLS; i++) {
        printf("        p[%d] 存在 %p，里面装的是 %p，指着的值是 %d\n",
               i, (void *)&p[i], (void *)p[i], p[i] ? *p[i] : 0);
    }
    printf("        三个被指的东西地址互不相干 —— 所以它是「指针的集合」，不是「一整块」。\n");
}
