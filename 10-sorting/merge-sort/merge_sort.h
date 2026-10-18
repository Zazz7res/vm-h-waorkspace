#ifndef MERGE_SORT_H
#define MERGE_SORT_H

/*
 * ===================================================================
 * 归并排序 merge-sort —— 这一节的"菜单"
 * ===================================================================
 * 一句话：**拆到只剩一个元素（一个元素天然有序）→ 两两合并，合一次长一倍。**
 *
 * 整个算法只有一个"发动机"：合并两个已排好的有序段。
 * 递归部分只有 3 行，真正需要看懂的只有 merge_ranges() 那十几行 ——
 * 所以本头文件把发动机单独暴露出来，让你可以只喂两个小数组给它，单独看它怎么工作。
 *
 * -------------------------------------------------------------------
 * 三种调用姿势（对应 main.c 的三幕）
 * -------------------------------------------------------------------
 *   merge_sort(a, n)                 —— 工程版：真的排序用，内部自己 malloc 一份临时数组
 *   merge_sort_trace(a, n, verbose)  —— 教学版：每次合并打印左段、右段、逐格取数过程、结果
 *   merge_sort_bottom_up(a, n)       —— 自底向上版：按"层"合并，每层之后打印整个数组
 *                                        （这一版没有递归，是归并的另一种写法，顺便给你看 iterative）
 *
 * -------------------------------------------------------------------
 * 为什么要传 tmp？为什么不每次合并都 malloc？
 * -------------------------------------------------------------------
 *   归并需要 O(n) 额外空间，但【只需要一份】：
 *   在入口处 malloc 一个长度 n 的数组，所有递归层共用它的不同区间（只用 tmp[lo..hi] 这一段）。
 *   如果每次合并都 malloc/free，光是申请释放就把 O(n log n) 的常数放大好几倍。
 *   这也是本节唯一需要 free 的地方 —— main.c 跑完必须干净退出（ASan 检查）。
 *
 * -------------------------------------------------------------------
 * 元素类型：全部 int。没有 void*，没有函数指针比较器。
 * -------------------------------------------------------------------
 *   稳定性证明怎么办？还是"两条平行数组"：key[] 是比较的分数，tag[] 是身份证，
 *   合并时【两边一样大便优先取左边】⇒ 左边那个人永远排在右边那个人前面 ⇒ 稳定。
 *   一个 <= 号就是稳定性的全部秘密，本节的 merge 里会在这一行专门加注释。
 */

/*
 * 统计口径：
 *   cmp    —— 合并时做了多少次"两边谁更小"的比较
 *   copy   —— 往 tmp 里抄 + 从 tmp 抄回来的总格子数（归并的真实内存搬运量）
 *   merges —— 一共执行了多少次合并
 * 用 long：n = 20 万时 copy 次数会到千万级，int 够用但没必要冒险。
 */
typedef struct {
    long cmp;
    long copy;
    long merges;
} MergeStat;

/* ---------- 1. 发动机：合并两个相邻的有序段 ---------- */
/*
 * 前提：a[lo..mid] 和 a[mid+1..hi] 【各自】已经排好。
 * 动作：把它们合成一个有序段，结果写回 a[lo..hi]。
 * tmp ：调用者准备的临时数组（长度至少 hi+1），本函数只使用 tmp[lo..hi]。
 * st  ：可以传 NULL；不传 NULL 就累加统计。
 * verbose：0 = 安静；1 = 把"每次比较取了哪边、取完数组长什么样"全打出来。
 */
void merge_ranges(int a[], int tmp[], int lo, int mid, int hi, MergeStat *st, int verbose);

/* ---------- 2. 三种整体排序 ---------- */
void merge_sort(int a[], int n);                                    /* 工程版：自顶向下 */
void merge_sort_trace(int a[], int n, int verbose, MergeStat *st);   /* 教学版：同逻辑 + 打印 */
void merge_sort_bottom_up(int a[], int n, int verbose, MergeStat *st); /* 自底向上：按层合并 + 每层快照 */

/* ---------- 3. 稳定性实验：分数 key + 身份证 tag ---------- */
void print_tagged(const char *label, const int key[], const int tag[], int n);
int  merge_sort_tagged(int key[], int tag[], int n);   /* 返回 0 失败（内存不够），1 成功 */

/* ---------- 4. 归并思想的应用：数逆序对 ---------- */
/* 一对 (i, j) 满足 i < j 且 a[i] > a[j] 就是逆序对。数组有多"乱"的量化指标。 */
long count_inversions(const int a[], int n, int verbose);
long count_inversions_bruteforce(const int a[], int n);   /* O(n²) 笨办法，用来验证上面那个对不对 */

/* ---------- 5. 验货工具（每个叶子目录自包含，所以这里再写一份极简版） ---------- */
void print_arr(const char *label, const int a[], int n);
int  is_sorted_ascending(const int a[], int n);
int  same_elements(const int a[], const int b[], int n);

#endif /* MERGE_SORT_H */
