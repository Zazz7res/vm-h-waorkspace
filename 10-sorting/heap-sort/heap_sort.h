#ifndef HEAP_SORT_H
#define HEAP_SORT_H

/*
 * ===================================================================
 * 堆排序 heap-sort —— 这一节的"菜单"
 * ===================================================================
 * 一句话：**把数组本身当成一棵"完全二叉树"来看（下标算父子，一个结构体都不用），
 *          先自底向上把它整理成"每个爹都 ≥ 两个孩子"的最大堆，
 *          然后反复【把堆顶换到数组末尾、堆缩小 1、再把新堆顶沉下去】。**
 *          每取一次就锁定一个位置，取 n 次就排完了 —— 全程 O(1) 额外空间。
 *
 * -------------------------------------------------------------------
 * 为什么本节【不用】写一个 Heap 结构体（但还是一起写了个极简的）
 * -------------------------------------------------------------------
 *   堆排序最漂亮的地方就是：堆只是数组的一种"看法"，不是另一种"存法"。
 *       根 = a[0]；a[i] 的父 = a[(i-1)/2]；a[i] 的左孩子 = a[2i+1]；右孩子 = a[2i+2]
 *   三个公式就把"树"安进了连续内存里，所以建堆、排序全部原地完成，不需要额外数组。
 *   为了对照，本目录还是写了一个带 push/pop 的极简动态堆（类型 Heap），
 *   它用的下标公式和原地版【一模一样】—— 用完记得 heap_free，别漏。
 *
 * -------------------------------------------------------------------
 * 两阶段（顺序不能反，也别以为建堆很贵）
 * -------------------------------------------------------------------
 *   阶段 1 建堆：从最后一个非叶子 a[n/2-1] 开始，倒着往前逐个【下沉】。
 *                看起来是 n 次 O(log n) 操作 = O(n log n)，其实绝大部分节点在
 *                最底层、根本不用沉 ⇒ 真正的代价是 O(n)（第 5 幕用实测比较次数说明）。
 *   阶段 2 取数：堆顶 ↔ 当前末尾交换（堆 size 减 1），把换上来的小值沉下去。
 *                做 n-1 次 ⇒ 数组右半边从后往前长出一段"已经排好"的区。
 *
 * -------------------------------------------------------------------
 * 本节的两个"反直觉"知识点
 * -------------------------------------------------------------------
 *   ① 它 O(1) 空间、最坏也是 O(n log n)，可实测【比快排慢大约一倍】。
 *      原因不在复杂度里：下沉是"从根跳到叶子"的跳跃访问，缓存命中率差；
 *      每层要做两次孩子比较 + 一次交换；常数大。第 7 幕用 clock() 跑真实数字。
 *   ② 既然慢，为什么还要学？因为它是【最坏情况的兜底】：
 *      introsort = 快排 + "递归太深就切换成堆排序"（第 6 幕本目录真写了一个 intro_sort，
      还故意把深度阈值压小，逼兜底分支真的被走一遍），
 *      快排在有序数据上塌成 O(n²) 时，堆排序保证整体退回 O(n log n)。
 *
 * -------------------------------------------------------------------
 * 元素类型：全部 int。没有 void*，没有函数指针比较器。
 * -------------------------------------------------------------------
 *   不稳定的证据用两条平行数组（分数 key + 身份证 tag）跑给你看：
 *   "堆顶和末尾那次远距离交换"就是打破顺序的凶手，和第 6 幕输出一致。
 */

/*
 * 统计口径（三个计数分别对应三种代价）：
 *   cmp  —— 大小比较次数：a[左] > a[?] 和 a[右] > a[?] 各算一次
 *   swap —— 交换次数：下沉过程中的交换 + 阶段 2 的"堆顶↔末尾"交换
 *   sink —— 下沉层数：所有节点累计往下走了几层（看"跳跃访问"有多离谱最直观）
 */
typedef struct {
    long cmp;
    long swap;
    long sink;
} HeapStat;

/* ---------- 1. 核心三件套（都是原地、都支持子区间，所以 introsort 能复用） ---------- */

/* 把 a[lo..hi] 里下标 root 这个节点往下沉到该待的位置（假设它俩孩子已经是合法堆） */
void sink_range(int a[], int lo, int hi, int root, int verbose, HeapStat *st);

/* 阶段 1：原地建最大堆。从最后一个非叶子开始倒着逐个下沉 */
void build_max_heap(int a[], int lo, int hi, int verbose, HeapStat *st);

/* 阶段 2：反复 堆顶↔末尾交换 + 缩小 + 下沉。建堆 + 取数一整轮 */
void heap_sort_range(int a[], int lo, int hi, int verbose, HeapStat *st);

/* ---------- 2. 排序入口 ---------- */
void heap_sort(int a[], int n);                                   /* 工程版：干净、无打印、无计数 */
void heap_sort_trace(int a[], int n, int verbose, HeapStat *st);  /* 教学版：两阶段全部现形 */

/* ---------- 3. 可视化：把数组当树看 ---------- */
void print_heap_tree(const char *label, const int a[], int lo, int hi);   /* 缩进画出父子 */
void print_split(const char *label, const int a[], int heap_end, int n);  /* 堆区 | 已排好区 */

/* ---------- 4. 极简动态最大堆（只在本目录用；对照"堆就是数组 + 三个公式"） ---------- */
typedef struct {
    int *data;    /* malloc 出来的数组；下标公式和原地版完全一致 */
    int  cap;     /* data 能装多少个 */
    int  size;    /* 当前装了多少个 */
} Heap;

int  heap_init(Heap *h, int cap);                  /* 成功返回 1，失败返回 0 */
void heap_free(Heap *h);                           /* 必须调用，否则 ASan 会报泄漏 */
void heap_push(Heap *h, int v, HeapStat *st);      /* 尾部塞进去，然后往上冒 */
int  heap_pop_max(Heap *h, HeapStat *st);          /* 取走 a[0]（最大值），末尾补到堆顶再下沉 */
int  heap_peek_max(const Heap *h);                 /* 只看最大值，不动（空堆返回 INT_MIN） */

/* ---------- 5. 稳定性实验：分数 + 身份证（两条平行数组） ---------- */
void print_tagged(const char *label, const int key[], const int tag[], int n);
void heap_sort_tagged(int key[], int tag[], int n);

/* ---------- 6. 对照算法（本目录自带极简版，不引用兄弟目录的文件） ---------- */
void mini_quick_sort(int a[], int n, long *cmp_out);   /* Lomuto + 固定末位枢轴：故意保留它的毒药脾气 */
void mini_quick_sort3(int a[], int n, long *cmp_out);  /* 同上但带三数取中：这是"公平对手"，用来比常数 */
void mini_insertion_sort(int a[], int n);
/*
 * introsort：三数取中快排 + 小段切插入 + 【递归太深就把那一段交给堆排序】。
 * depth_limit <= 0 表示用经典阈值 2*floor(log2(n))；第 6 幕会故意传一个很小的值，
 * 逼着兜底分支真的被走进来，验证"切过去之后结果仍然正确"。
 */
void intro_sort(int a[], int n, int depth_limit, long *cmp_out, long *switch_to_heap_out);

/* ---------- 7. 验货工具（目录自包含，本目录再写一份极简版） ---------- */
void print_arr(const char *label, const int a[], int n);
int  is_sorted_ascending(const int a[], int n);
int  same_elements(const int a[], const int b[], int n);
int  is_permutation(const int a[], const int b[], int n);

#endif /* HEAP_SORT_H */
