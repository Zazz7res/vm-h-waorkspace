#ifndef BASIC_SORT_H
#define BASIC_SORT_H

/*
 * ===================================================================
 * 冒泡 / 选择 / 插入 —— O(n²) 三兄弟，这一节的"菜单"
 * ===================================================================
 * 这个文件里【只有声明】。看完它就知道本节会写哪几个函数、每个函数吃进去什么、吐出来什么。
 * 实现和逐行讲解在 basic_sort.c，演示分幕在 main.c。
 *
 * -------------------------------------------------------------------
 * 三个算法各自的一句话画像
 * -------------------------------------------------------------------
 *   冒泡：相邻两个比，谁大谁往后挪。每趟结束，"这趟没排好的最大值"一定沉到最后。
 *   选择：每趟从头到尾找最小值的下标，只在【每趟最后】交换一次。比较多、交换极少。
 *   插入：像打扑克理牌。左手持牌（已有序），右手拿一张新牌，从右往左比，比它大的往右挪。
 *
 * -------------------------------------------------------------------
 * 为什么这里既有 xxx_sort() 又有 xxx_sort_trace()？
 * -------------------------------------------------------------------
 *   xxx_sort()        —— 工程版：干干净净 5~8 行，没有打印、没有计数，是你该背下来的形状。
 *   xxx_sort_trace()  —— 教学版：逻辑【完全一样】，只是每趟之后把数组打印出来、把比较/交换次数累加。
 *                        传 verbose = 0 就变成"只统计不打印"，用来对比不同数据的代价。
 * 两份代码故意写重复，是为了让工程版保持"一眼看懂"，不让 printf 污染它。
 *
 * -------------------------------------------------------------------
 * 元素类型：全部 int。没有 void*，没有函数指针比较器。
 * -------------------------------------------------------------------
 *   稳定性实验（"5a / 5b 这种相同分数不同人"）怎么办？
 *   用两个平行的 int 数组：key[] 存分数（比较看它），tag[] 存"这是第几个同学"（跟着一起搬）。
 *   搬的时候两个数组一起搬，最后打印 tag 就能看出：同分的两个人先后顺序有没有被打乱。
 *   这就是"稳定 / 不稳定"唯一可观测的证据。
 */

/*
 * 统计口径：一个 SortStat 记录这一趟排序到底干了多少活。
 *   cmp    ：做了多少次"比大小"
 *   swap   ：做了多少次"元素挪位置"（插入排序里指后移 + 落位，都算）
 *   rounds ：实际跑了多少趟（冒泡提前收工时，rounds 会远小于 n-1）
 *   early_exit：只有冒泡会置 1 —— "这趟一次交换都没发生，说明已经有序，直接结束"
 * 用 long 是因为 n 一大 int 会溢出（最坏比较次数 ~n²/2，n=5 万就顶到 10 亿级）。
 */
typedef struct {
    long cmp;
    long swap;
    long rounds;
    int  early_exit;
} SortStat;

/* ---------- 0. 三个小工具（正确性校验用，别的章节也会各自写一份） ---------- */
void print_arr(const char *label, const int a[], int n);          /* 打印成 [ 5 2 9 ... ] */
int  is_sorted_ascending(const int a[], int n);                    /* 1 = 确实是不降的，0 = 有问题 */
int  same_elements(const int a[], const int b[], int n);           /* 1 = 两个数组内容完全一样（顺序敏感） */
int  is_permutation(const int a[], const int b[], int n);          /* 1 = 只是顺序不同，元素一个不多一个不少 */

/* ---------- 1. 工程版：背这三个 ---------- */
void bubble_sort(int a[], int n);
void selection_sort(int a[], int n);
void insertion_sort(int a[], int n);

/* ---------- 2. 教学版：逐趟打印 + 计数；verbose=0 时只计数不打印 ---------- */
void bubble_sort_trace(int a[], int n, int verbose, SortStat *stat_out);
void selection_sort_trace(int a[], int n, int verbose, SortStat *stat_out);
void insertion_sort_trace(int a[], int n, int verbose, SortStat *stat_out);

/* ---------- 3. 稳定性实验：key 是比较对象，tag 是"谁是谁"的身份证 ---------- */
void print_tagged(const char *label, const int key[], const int tag[], int n);
void bubble_sort_tagged(int key[], int tag[], int n);      /* 预期：稳定，tag 顺序不变 */
void insertion_sort_tagged(int key[], int tag[], int n);   /* 预期：稳定 */
void selection_sort_tagged(int key[], int tag[], int n);   /* 预期：不稳定，同分的 tag 会被换乱 */

#endif /* BASIC_SORT_H */
