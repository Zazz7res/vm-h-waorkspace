#ifndef NONSORT_H
#define NONSORT_H

/*
 * ===================================================================
 * 非比较排序 non-comparison —— 这一节的"菜单"
 * ===================================================================
 * 一句话：冒泡/快排/归并都在【问"谁比谁大"】，这一节的三个算法【不问】——
 *         它们直接利用"这个数是多少"这个信息，把元素一步放到该待的位置上。
 *         代价是必须知道（或能算出）数据的【值域】，所以它们不是万能的，
 *         但在合适的前提下能打破比较排序的 O(n log n) 下界，快到离谱。
 *
 *   计数 counting ：值域不大（n 个数、值在 0..k，k 也小）—— O(n + k)，"按号占座"
 *   基数 radix    ：位数不多（int 最多 10 进制 10 位 / 2 进制 4 轮字节）—— O(d·(n+k))
 *   桶   bucket   ：数据在值域上【均匀分布】 —— 平均 O(n + n/桶数)，分桶后各桶内小排序
 *
 * -------------------------------------------------------------------
 * 本节把三件事分别"演"给你看，不是只说一遍
 * -------------------------------------------------------------------
 *   ① 计数的两个版本：
 *        展开版（每个值重复几次就往回写几次）—— 简单，但【不稳定】
 *        前缀和版（先累加出"≤ v 的有几个"，再从后往前放）—— 【稳定】
 *      两个版本都【必须先减去最小值】才能处理负数，现场喂一组带负数的数据看效果。
 *   ② 基数：按个位、十位、百位一轮轮排，每一轮用的是【上面那个稳定的计数排序】；
 *      每一轮之后整个数组快照都打出来（这是看懂基数的唯一办法）。
 *   ③ 桶：均匀数据（每桶没几个）vs 全挤一块（一个桶吃下全部）→ 后者退化成 O(n²)，
 *      每桶的大小都会打印出来，退化看得见。
 *   ④ 计数排序的"值域爆炸"防护：n=10万、值是 32 位整数 ⇒ count[] 要 16GB，
 *      代码遇到这种值域会【直接拒绝并打印原因】，而不是真去 malloc 把机器打爆。
 *
 * -------------------------------------------------------------------
 * 元素类型：全部 int。没有 void*，没有函数指针比较器。
 * -------------------------------------------------------------------
 *   桶内排序、参照用的归并/插入，全部在本目录里重写了一份极简版（目录自包含，
 *   Makefile 是按目录独立编译的，不能 #include 兄弟目录的文件）。
 */

/*
 * 统计口径：
 *   cmp       —— 大小比较次数（非比较排序的核心卖点就是它【几乎不比大小】）
 *   move      —— 往数组里写值的次数（放置/回写）
 *   aux_cells —— 额外申请的格子数（计数数组、临时数组、桶的总容量）：
 *                "用空间换时间"换掉了多少空间，这个数字直接摊开给你看
 */
typedef struct {
    long cmp;
    long move;
    long aux_cells;
} NSStat;

/* ---------- 1. 极值与验货 ---------- */
void arr_min_max(const int a[], int n, int *min_out, int *max_out);
void print_arr(const char *label, const int a[], int n);
int  is_sorted_ascending(const int a[], int n);
int  same_elements(const int a[], const int b[], int n);      /* 逐位置比对（要求两边都已排序）*/
int  same_multiset(const int a[], const int b[], int n);      /* 多重集合相同（直方图法，O(n+值域)）*/

/* 返回值约定（三个非比较排序都用它）：
 *   1  = 成功排好
 *   0  = 内存申请失败
 *  -1  = 值域太大，主动拒绝（计数数组会大到不划算/不可能）—— 这不是 bug，是防护
 */

/* ---------- 2. 计数排序两个版本 ---------- */
int counting_sort_expand(int a[], int n, int verbose, NSStat *st);   /* 展开版：不稳定 */
int counting_sort_stable(int a[], int n, int verbose, NSStat *st);   /* 前缀和版：稳定 */

/* ---------- 3. 基数排序（LSD，十进制一位一轮；内部调用稳定计数排序） ---------- */
int radix_sort_lsd(int a[], int n, int verbose, NSStat *st);

/* ---------- 4. 桶排序（前提：值域上均匀分布；桶内用插入排序） ---------- */
int bucket_sort(int a[], int n, int bucket_count, int verbose, NSStat *st);

/* ---------- 5. 稳定性实验：分数 + 身份证（两条平行数组） ---------- */
void print_tagged(const char *label, const int key[], const int tag[], int n);
int  counting_expand_tagged(int key[], int tag[], int n);   /* 展开版：会看到不稳定 */
int  counting_stable_tagged(int key[], int tag[], int n);   /* 前缀和版：稳定 */

/* ---------- 6. 参照实现（本目录自带的极简版，用来对照 O(n log n) 下界） ---------- */
void merge_sort_ref(int a[], int n, long *cmp_out);     /* 稳定 O(n log n)，要 O(n) 空间 */
void insertion_sort_ref(int a[], int n, long *cmp_out); /* O(n²)，小数组的王者 */

#endif /* NONSORT_H */
