#ifndef QUICK_SORT_H
#define QUICK_SORT_H

/*
 * ===================================================================
 * 快速排序 quick-sort —— 这一节的"菜单"
 * ===================================================================
 * 一句话：**选一个数当"参照物"(pivot)，一趟就把数组劈成【左边都 ≤ 它 ≤ 右边都】，
 *          然后只对两半边各再来一遍。** 劈完的那一刻，pivot 已经站在它的最终位置上了。
 *
 * -------------------------------------------------------------------
 * 本节把两种分区写法都实现一遍（这是快排最容易踩坑的地方）
 * -------------------------------------------------------------------
 *   Lomuto（选最后一个当枢轴）  —— 代码最短、能给出"枢轴的最终下标"，但交换多
 *   Hoare（双指针从两头往中间夹）—— 交换少一半、枢轴【不在】返回的下标上，边界要格外小心
 *   两者的返回值含义不一样，所以递归写法也不一样：
 *       Lomuto: quick(a, lo, p-1) + quick(a, p+1, hi)，p 是枢轴最终位置
 *       Hoare : quick(a, lo, p)   + quick(a, p+1, hi)，p 只是"分界点"，不是枢轴位置
 *   把 Hoare 写成 (lo, p-1) —— 元素会被漏掉；把返回值当成枢轴位置去打印 —— 会看到"骗人"的结果。
 *
 * -------------------------------------------------------------------
 * 本节最大的知识点：最坏情况怎么发生、怎么救
 * -------------------------------------------------------------------
 *   固定选最后一个当 pivot + 喂进【已经升序】的数组 ⇒ 每趟只劈掉 1 个元素 ⇒
 *   递归树塌成一根链条，深度 n-1，比较次数 n(n-1)/2 = O(n²)。
 *   救法：三数取中（median-of-three）或随机化 pivot —— 同一个有序数组喂进来，
 *         劈出来的两半边差不多长，深度回到 log n。第 3、4 幕会跑出真实数字对比。
 *   另外：先递归【短的那半边】、用循环处理长半边 ⇒ 栈深度锁死在 O(log n)（第 5 幕实测）。
 *
 * -------------------------------------------------------------------
 * 元素类型：全部 int。没有 void*，没有函数指针比较器。
 * -------------------------------------------------------------------
 *   不稳定的证据用两条平行数组（分数 key + 身份证 tag）跑给你看：
 *   Lomuto 收尾那次"枢轴和 a[i+1] 的远距离交换"就是打破顺序的凶手。
 */

/*
 * 统计口径：
 *   cmp        —— "这个数该不该进左边"这类比较的总次数
 *   swap       —— 交换次数
 *   partitions —— 执行了多少趟分区（= 递归调用里非叶子的个数）
 *   max_depth  —— 递归到达过的最大深度（看"塌成链条"最直观的数字）
 */
typedef struct {
    long cmp;
    long swap;
    long partitions;
    int  max_depth;
} QStat;

/* ---------- 1. 两种分区（直接暴露出来，方便单独喂数据观察） ---------- */
int partition_lomuto(int a[], int lo, int hi, QStat *st);   /* 返回枢轴的【最终下标】 */
int partition_hoare (int a[], int lo, int hi, QStat *st);   /* 返回【分界点】：左半 ≤ pivot ≤ 右半，枢轴本体未必在 p 上 */

/* ---------- 2. 三种完整排序入口 ---------- */
void quick_sort_lomuto(int a[], int n, int verbose, QStat *st);   /* 固定选最后一个当枢轴 */
void quick_sort_hoare(int a[], int n, int verbose, QStat *st);    /* 双指针分区，枢轴取区间中点的值 */
void quick_sort(int a[], int n);                                  /* 工程版：干净无打印，Lomuto */

/* ---------- 3. 抗"最坏情况"的两种改法 ---------- */
int  median_of_three(int a[], int lo, int hi);        /* 返回 lo/mid/hi 三个数中【中位数】所在的下标 */
void quick_sort_median3(int a[], int n, int verbose, QStat *st);  /* 三数取中 + Lomuto */
void quick_sort_randomized(int a[], int n, int verbose, QStat *st); /* 随机化 pivot（LCG 固定种子，输出可复核） */

/* ---------- 4. 栈深度优化版：先递归短的那半边，长的用循环吃掉 ---------- */
void quick_sort_tail_opt(int a[], int n, int verbose, QStat *st);

/* ---------- 5. 稳定性实验：分数 + 身份证 ---------- */
void print_tagged(const char *label, const int key[], const int tag[], int n);
void quick_sort_tagged(int key[], int tag[], int n);   /* Lomuto 版，跑出来就是不稳定的那个 */

/* ---------- 6. 验货工具（目录自包含，本目录再写一份极简版） ---------- */
void print_arr(const char *label, const int a[], int n);
int  is_sorted_ascending(const int a[], int n);
int  same_elements(const int a[], const int b[], int n);
int  is_permutation(const int a[], const int b[], int n);

#endif /* QUICK_SORT_H */
