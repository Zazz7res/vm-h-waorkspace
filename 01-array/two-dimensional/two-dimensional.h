#ifndef TWO_DIMENSIONAL_H
#define TWO_DIMENSIONAL_H

/*
 * ===================================================================
 * two-dimensional.h —— 二维数组在内存里到底是怎么躺的
 * ===================================================================
 * 【本节最重要的一句话：C 里根本没有"二维数组"这种内存形状】
 *   `int m[2][3]` 编译出来是【一整块 6 个 int 的连续内存】，中间没有任何分隔线：
 *
 *         下标:   [0][0]  [0][1]  [0][2]  [1][0]  [1][1]  [1][2]
 *         地址:    +0      +4      +8      +12     +16     +20
 *         内容:     1       2       3       4       5       6
 *                 └────── 第 0 行 ──────┘└────── 第 1 行 ──────┘   ← 行只是"每 3 格一组"的读法
 *
 *   "行"和"列"不是物理存在，是【换算规则】：m[r][c] 被翻译成 *(base + r*cols + c)，
 *   乘的那个 cols 就是"一行占几格"。这一条决定了本节所有事情：
 *     1) 为什么进函数必须把【列数】写死（行数是记账用的，列数是换算公式的一部分）；
 *     2) 为什么 `int (*p)[3]` 能遍历二维数组，而 `int *p[3]` 完全不是同一回事；
 *     3) 为什么动态二维有两种做法：一次 malloc 一整块（真连续）vs 指针表 + 每行一次 malloc
 *        （行间可以不连续）—— 申请次数、free 顺序、缓存性能全都跟着不一样。
 *
 * 【四个长得都像"m"的表达式，值一样但类型不一样，走一步的代价也不同】
 *     &m[0][0]  → int *        走一步 +4  字节（一个元素）
 *     m[0]      → int *        走一步 +4  字节（它就是第 0 行首格，退化后和上面同值）
 *     m         → int (*)[3]   走一步 +12 字节（一整行 = cols*sizeof(int)）
 *     &m        → int (*)[2][3] 走一步 +24 字节（整个矩阵 = rows*cols*sizeof(int)）
 *   main 第 2 幕把这四个的真实地址和"+1 之后落在哪儿"全打出来 —— 看数字比看文字管用。
 *
 * 【这一节的两个结构】
 *   FlatMatrix  ：一次 malloc(rows*cols) 的"真连续"二维，靠 r*cols+c 换算下标；
 *   TableMatrix ：int ** 指针表 + 每行一次 malloc，长得像教科书里的图，但行间不保证连续。
 *   两个都配 free()，而且 TableMatrix 的 free 顺序是硬约束（先每行、再表），
 *   main 第 9 幕用命令行开关故意反着来一次，让 ASan 抓给你看。
 */

#include <stdbool.h>   /* bool / true / false */

/*
 * 演示用矩阵的固定维度：本节大量地方要"列数写死在类型里"，
 * 所以给一对编译期常量，免得各处手写数字对不上。
 */
#define DEMO_ROWS 2
#define DEMO_COLS 3

/* 缓存实测用的大方阵边长：1000*1000*4 字节 = 4MB，明显超过 L1/L2，才能把差异逼出来 */
#define CACHE_TEST_N 1000

/* 计时跑几轮取最小值：第一轮有"第一次碰这块内存"的额外成本，取最小值才公平 */
#define CACHE_TEST_ROUNDS 3

/* ===================================================================
 * 结构一：FlatMatrix —— 一整块连续内存冒充的二维数组
 * ===================================================================
 * 只有三个字段，堆上只有【一处】内存。行和列纯粹是解读方式。
 * 好处：一次申请一次归还、可以整块 memcpy、局部性最好、没有"行指针被写坏"的风险。
 */
typedef struct {
    int *data;      /* 一块 malloc(rows*cols*sizeof(int))，全连续 */
    int rows;
    int cols;
} FlatMatrix;

/*
 * 建一个 rows*cols 的连续矩阵。用 malloc（不是 calloc），
 * 然后把每一格涂成 -9000-线性下标 的毒值：
 * 为什么要涂？"忘了 set 就读"的时候，打出来一眼就是假的数字，而不是运气好的 0。
 */
bool flat_matrix_init(FlatMatrix *m, int rows, int cols);

/* 归还内存：free + data=NULL + rows/cols 归零（和上一节 destroy 三件套同一个规矩） */
void flat_matrix_free(FlatMatrix *m);

/* 越界检查版写入：行或列出界就打印一条拒绝信息，什么都不改 */
bool flat_matrix_set(FlatMatrix *m, int r, int c, int value);

/* 越界检查版读取：出界返回 false，*out 原样不动 */
bool flat_matrix_get(const FlatMatrix *m, int r, int c, int *out);

/* 不设防的下标换算（内层循环专用）：为什么单独写成 inline 函数？
 * 因为这行 `r * m->cols + c` 就是"二维其实不存在"的全部证据 —— 把它显式写出来比藏着好。 */
int flat_matrix_index(const FlatMatrix *m, int r, int c);

/* 打印成表格（按行列排版，看不出内存形状，所以后面还有一版地址地图） */
void flat_matrix_print(const FlatMatrix *m, const char *tag);

/* 地址地图：按【物理顺序】把每一格摊成一行：线性下标 / 行列身份 / 地址 / 值 */
void flat_matrix_print_cell_map(const FlatMatrix *m, const char *tag);

/* 两种遍历顺序求和：工作量完全相同（都读 rows*cols 次），只是读的节奏不一样 */
long long flat_matrix_sum_row_major(const FlatMatrix *m);   /* 先行后列：内存上是顺着走 */
long long flat_matrix_sum_col_major(const FlatMatrix *m);   /* 先列后行：每步跳 cols 格 */

/*
 * 实测两种读法谁快：n*n 的连续矩阵，交替跑 rounds 轮，每种取【最快一轮】。
 * 为什么要取最快而不是取第一次？因为第一次要"把页真正拿到手"（缺页中断），
 * 那个成本会盖过缓存效应；取最小值才是稳态下的差距。
 * 计时用 clock_gettime(CLOCK_MONOTONIC)（需要 _POSIX_C_SOURCE 200809L）。
 * 两个和必须相等 —— 读的是同一块内存，这也顺带证明了"只是读法不同"。
 */
void bench_row_vs_col_major(int n, int rounds);

/* ===================================================================
 * 结构二：TableMatrix —— 教科书里那种 int** 二维
 * ===================================================================
 * 堆上有【rows+1 处】内存：一张指针表 + 每行一条。
 * 表里第 i 格存着第 i 行的入口地址，所以 m->row[i][c] 能读；
 * 但"每行各自是独立一次 malloc"意味着：行与行之间【没有】连续的保证。
 */
typedef struct {
    int **row;      /* 指针表：row[i] = 第 i 行的首地址（各自单独 malloc 来的） */
    int rows;
    int cols;
} TableMatrix;

bool table_matrix_init(TableMatrix *m, int rows, int cols);

/* 正确顺序：先把每一行 free 掉，再 free 那张指针表。顺序反了就是野指针 + 泄漏。 */
void table_matrix_free(TableMatrix *m);

/*
 * ⚠ 反面教材专用：先 free 指针表，再去读 row[i] —— 那就是 use-after-free。
 *   它存在的唯一理由是让 main 第 9 幕（命令行开关触发）演示"free 顺序不是洁癖"。
 *   正式代码里绝对不要调用它。
 */
void table_matrix_free_wrong_order(TableMatrix *m);

void table_matrix_print(const TableMatrix *m, const char *tag);

/* 打印指针表本身 + 每行入口地址 + 相邻两行的地址差：行差 = cols*sizeof(int) 只是运气，不是保证 */
void table_matrix_print_row_table(const TableMatrix *m, const char *tag);

/* ===================================================================
 * 三个"把二维数组传进函数"的签名 —— 同一块内存，三种写法，只有一种不用带列数
 * ===================================================================
 * 这三个函数存在的意义是把坑摆成可运行的事实：
 *   1) 列数必须出现在类型里（3 个乘数之一），行数才是纯记账；
 *   2) `int m[][3]` 和 `int (*m)[3]` 是【同一个类型】的两种写法；
 *   3) 一旦决定"列数运行时才知道"，就必须摊平成 int* + 自己写 r*cols+c 换算。
 */
void pass_2d_row_pointer(const int (*m)[DEMO_COLS], int rows, const char *tag);
void pass_2d_omitted_rows(int m[][DEMO_COLS], int rows, const char *tag);
void pass_2d_flat(const int *data, int rows, int cols, const char *tag);

/* ===================================================================
 * 两个长得极像、意思完全相反的类型
 * ===================================================================
 *   int (*p)[3] ：p 是"指向 3 个 int 那一排的指针" → p+1 走 12 字节，天生用来遍历二维数组
 *   int *p[3]   ：p 是"3 个 int* 组成的数组"        → p+1 走 8  字节，每格存的是别的地址
 * 两个函数各拿一种类型，把 sizeof 和 +1 的字节差打出来。
 */
void show_row_pointer(const int (*p)[DEMO_COLS], int rows);
void show_array_of_pointers(int *p[DEMO_COLS]);

#endif /* TWO_DIMENSIONAL_H */
