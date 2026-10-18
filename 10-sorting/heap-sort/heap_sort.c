/*
 * heap_sort.c —— 堆排序的实现
 *
 * 阅读顺序建议：
 *   1) 先看下面的「下标公式」注释和 print_heap_tree，把"数组 = 完全二叉树"这件事看顺；
 *   2) 再看 sink_range（唯一的发动机，二十来行）；
 *   3) 然后 build_max_heap + heap_sort_range 就是两句话；
 *   4) Heap 那几个函数是"带 push/pop 的对照版"，公式一模一样，只是多管一个 size；
 *   5) 最后的 mini_* / intro_sort 是给第 6 幕做耗时对照用的极简实现。
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "heap_sort.h"

/*
 * ===================================================================
 * 全节最重要的一张图：数组怎么变成树
 * ===================================================================
 *   约定【根在下标 0】（这是本节唯一的口径，换成 1 起步公式会变，别混着用）：
 *
 *                    a[0]                        ← 堆顶，最大堆里它就是整堆最大值
 *                 /        \
 *             a[1]          a[2]                 ← 2*0+1 = 1，2*0+2 = 2
 *            /     \        /     \
 *         a[3]    a[4]    a[5]    a[6]           ← 2*1+1 = 3，2*1+2 = 4 ...
 *          /  \
 *       a[7]  a[8]
 *
 *   反过来求父亲：parent(i) = (i - 1) / 2    （整数除法自动向下取整，左孩子右孩子同一个爹）
 *
 *   为什么这样"看"是合法的？因为这是【完全二叉树】：除了最后一层，每层都填满，
 *   最后一层从左往右连续填。这样的树有一个神奇性质：
 *       按层写出来（层序遍历），得到的正好是 a[0], a[1], a[2], ... 连续下标。
 *   所以「数组的第 i 格」和「树的第 i 个节点」是一一对应的，父子关系不需要存，
 *   用 2i+1 / 2i+2 / (i-1)/2 三个公式当场算就行。
 *   ⇒ 这就是堆排序能 O(1) 额外空间的根本原因：它没有"另一棵树"，只有同一个数组的两种看法。
 *
 *   最大堆的性质：对每个节点都有  a[i] >= a[2i+1] 且 a[i] >= a[2i+2]（孩子存在时）。
 *   注意它【只】约束爹和孩子，不约束兄弟之间 ⇒ a[1] 和 a[2] 谁大都行；
 *   也正因如此，「堆顶最大」这件事是 O(1) 能拿到的，而"整堆有序"根本没承诺。
 *
 * 子区间版本（给 introsort 兜底时要用 a[lo..hi] 当堆）：
 *   把 lo 看成新的 0。节点 i（绝对下标）：
 *       左孩子 = lo + 2*(i - lo) + 1
 *       右孩子 = 左孩子 + 1
 *       父     = lo + (i - lo - 1) / 2
 * ===================================================================
 */

/* 交换两个 int。堆排序的动作几乎全是交换，所以它值得单独一个函数 */
static void swap_int(int *x, int *y)
{
    int t = *x;
    *x = *y;
    *y = t;
}

/* =====================================================================
 * 验货三件套（每个目录自带一份，见 README「代码导读」）
 * ===================================================================== */

void print_arr(const char *label, const int a[], int n)
{
    printf("%s[", label);
    for (int i = 0; i < n; i++) {
        printf(" %d", a[i]);
    }
    printf(" ]\n");
}

/* 排完之后是不是真的升序（允许相等，所以用 <=） */
int is_sorted_ascending(const int a[], int n)
{
    for (int i = 1; i < n; i++) {
        if (a[i - 1] > a[i]) {
            return 0;
        }
    }
    return 1;
}

/* 和标准答案逐个位置比对 */
int same_elements(const int a[], const int b[], int n)
{
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

/* 元素守恒：a 和 b 是不是同一批数（O(n²) 数个数，够用且不需要额外内存） */
int is_permutation(const int a[], const int b[], int n)
{
    if (n <= 0) {
        return 1;
    }
    for (int i = 0; i < n; i++) {
        int in_a = 0;
        int in_b = 0;
        for (int k = 0; k < n; k++) {
            if (a[k] == a[i]) in_a++;
            if (b[k] == a[i]) in_b++;
        }
        if (in_a != in_b) {
            return 0;   /* 同一个值，两边出现次数不一样 ⇒ 丢数或造数了 */
        }
    }
    return 1;
}

/* =====================================================================
 * 可视化：缩进把父子关系画出来
 * ===================================================================== */

/* 绝对下标 i 在子区间 a[lo..hi] 里的左孩子（算出来可能 > hi，调用方自己判断越界） */
static int child_left(int lo, int i)
{
    return lo + 2 * (i - lo) + 1;
}

/* 绝对下标 i 的父亲；i == lo 时返回 -1 表示"它是根，没有爹" */
static int parent_of(int lo, int i)
{
    if (i == lo) {
        return -1;
    }
    return lo + (i - lo - 1) / 2;
}

/* 深度优先缩进打印：往右缩进 4 格 = 往下一层 */
static void tree_dfs(const int a[], int lo, int hi, int i, int depth)
{
    if (i > hi) {
        return;
    }
    for (int s = 0; s < depth; s++) {
        printf("    ");
    }
    int p = parent_of(lo, i);
    if (p < 0) {
        printf("a[%d]=%-4d ← 根（堆顶：整段最大值必须在这）\n", i, a[i]);
    } else {
        printf("a[%d]=%-4d ← 父 a[%d]=%d%s\n",
               i, a[i], p, a[p],
               (a[p] >= a[i]) ? "  ok(爹≥儿)" : "  ✗ 违反堆性质！");
    }
    tree_dfs(a, lo, hi, child_left(lo, i), depth + 1);
    tree_dfs(a, lo, hi, child_left(lo, i) + 1, depth + 1);
}

void print_heap_tree(const char *label, const int a[], int lo, int hi)
{
    int n = hi - lo + 1;
    if (n <= 0) {
        printf("%s 空堆（一个数都没有）\n", label);
        return;
    }
    printf("%s 按层读这个数组：", label);
    int first = lo;
    int count = 1;
    while (first <= hi) {
        printf(" ");
        int last = first + count - 1;
        if (last > hi) {
            last = hi;
        }
        for (int i = first; i <= last; i++) {
            printf("[%d]=%d ", i, a[i]);
        }
        if (last < hi) {
            printf("/");
        }
        first = last + 1;
        count *= 2;
    }
    printf("\n");
    printf("      缩进一层 = 往下走一层：\n");
    tree_dfs(a, lo, hi, lo, 0);
}

/* 打印"堆区 | 已排好区"：a[0..heap_end-1] 还是堆，a[heap_end..n-1] 已经锁死 */
void print_split(const char *label, const int a[], int heap_end, int n)
{
    printf("%s[", label);
    for (int i = 0; i < n; i++) {
        printf(" %d", a[i]);
        if (i == heap_end - 1) {
            printf(" |");     /* 竖线右边是"已经排好、再也不动"的区 */
        }
    }
    printf(" ]\n");
}

/* =====================================================================
 * 发动机：下沉 sink
 * =====================================================================
 * 干什么：a[root] 这个节点的值太小了（它的两棵子树却都已经是合法堆），
 *         把它一层层往下换，换到"比自己的孩子都大"为止。
 *
 * 为什么能只跟【较大的那个孩子】换？
 *   目标状态是 爹 ≥ 左 且 爹 ≥ 右。要是跟较小的孩子换，换完之后爹（=原来那个小值）
 *   在新位置上仍然可能小于另一边；而跟最大孩子换，换下来的那个孩子位置上的值
 *   恰好是"上一代的爹"，它只需要继续跟新孩子的最大者比 —— 每一步都保住"整棵树里
 *   除了正在下沉的这条路以外都是合法堆"这个不变式。
 *
 * 为什么写成 for(;;) 而不是递归？
 *   递归写法是 sink(child) —— 那是尾递归，编译器可能帮你优化，但不保证。
 *   直接 root = largest 回到循环顶部，是手动尾递归消除：栈上永远只有一层。
 *   （快排那边也有同一招，见 ../quick-sort/README.md 的第 5 幕。）
 *
 * 边界：
 *   - root 是叶子（left > hi）⇒ 第一次比较都不做，largest == root ⇒ 立刻返回。
 *     建堆阶段大量节点是这种情况 ⇒ 这就是"建堆其实是 O(n) 而不是 O(n log n)"的原因。
 *   - 只有左孩子没有右孩子（left == hi）⇒ 右孩子分支被 if 挡住，不能读 a[left+1]。
 *   - 区间大小 0（lo > hi）⇒ 函数根本不该被调用；这里也顺手挡一下。
 * ===================================================================== */
void sink_range(int a[], int lo, int hi, int root, int verbose, HeapStat *st)
{
    if (lo > hi || root < lo || root > hi) {
        return;
    }
    for (;;) {
        int left = child_left(lo, root);   /* 2i+1（子区间口径） */
        int right = left + 1;              /* 2i+2 */
        int largest = root;

        if (left <= hi) {
            if (st != NULL) st->cmp++;
            if (a[left] > a[largest]) {
                largest = left;
            }
        }
        if (right <= hi) {
            if (st != NULL) st->cmp++;
            /* 这里必须用 >（不是 >=）：左右孩子相等时任选谁都行，用 > 让行为确定，
               打印出来的过程才可复核。 */
            if (a[right] > a[largest]) {
                largest = right;
            }
        }
        if (largest == root) {
            return;   /* 已经 ≥ 两个孩子 ⇒ 这个子树是合法堆了 */
        }
        if (verbose) {
            printf("|          沉：a[%d]=%d < 孩子 a[%d]=%d ⇒ 换下去\n",
                   root, a[root], largest, a[largest]);
        }
        swap_int(&a[root], &a[largest]);
        if (st != NULL) {
            st->swap++;
            st->sink++;
        }
        root = largest;   /* 跟着这个值继续往下沉（= 尾递归手动消除） */
    }
}

/* =====================================================================
 * 阶段 1：原地建最大堆
 * =====================================================================
 * 从哪个下标开始？最后一个有孩子的节点。
 *   最后一层的最后一个节点是 hi（相对下标 n-1），它的爹 = (n-2)/2。
 *   下标比 (n-2)/2 更大的全是叶子，叶子自己就是合法堆，不用管。
 *   （n=8 ⇒ 从 3 开始；n=7 ⇒ 从 2 开始。别用 n/2 - 1 之外还多加 1 的写法，容易越界。）
 *
 * 为什么必须【从大到小】倒着来？
 *   sink 的前提是"它的两棵子树已经是合法堆"。倒着走，处理到某个节点时，
 *   它下面的所有节点都已经处理完了 ⇒ 前提天然成立。顺着走就是白做。
 *
 * 复杂度（本节最容易搞错的一条）：
 *   看起来 = n 次调用 × 每次 O(log n) = O(n log n)。
 *   实际上高度为 h 的节点最多 ceil(n / 2^(h+1)) 个，每个最多沉 h 层：
 *       总层数 = Σ n/2^(h+1) * h = n * Σ h/2^(h+1) = n * 2 = O(n)
 *   直觉：一半的节点在最底层（沉 0 层）、四分之一的节点沉最多 1 层……
 *   越靠下越多、越要沉的越少。第 5 幕用实测比较次数把这个结论钉死。
 * ===================================================================== */
void build_max_heap(int a[], int lo, int hi, int verbose, HeapStat *st)
{
    int size = hi - lo + 1;
    if (size < 2) {
        return;   /* 0 个或 1 个元素，本来就是合法堆 */
    }
    int last_parent = lo + (size - 2) / 2;
    for (int i = last_parent; i >= lo; i--) {
        if (verbose) {
            printf("|      处理 a[%d]=%d（它是倒数第二层往下的爹，它的孩子已经都合法了）\n",
                   i, a[i]);
        }
        sink_range(a, lo, hi, i, verbose, st);
    }
}

/* =====================================================================
 * 阶段 2：反复「堆顶 ↔ 末尾交换，堆缩一个，再下沉」
 * =====================================================================
 * 关键：排好序的部分【就在同一个数组的尾巴上】，用「堆的有效大小」把它和堆隔开：
 *
 *   [ ←—— 堆：a[0..end-1] ——→ ][ 已排好：a[end..n-1] ]
 *
 *   1) swap(a[0], a[end])：把当前最大值送到它在最终数组里该待的位置（末尾）。
 *      这一步【不能省】—— 不交换就想拿走堆顶，就得真删数组元素，那是 O(n) 搬动。
 *   2) end--：堆变小，被换出去的格子从此不再参与比较（所以它不会被"改坏"）。
 *   3) sink(0)：从末尾换上来的那个值大概率很小，得把它沉下去，重新维持堆性质。
 *   做 n-1 次，最后 end == 0 时堆里只剩一个数，它必然就是最小值 ⇒ 不用动。
 *
 * 顺序：这样排完是【升序】，因为每次拿走的是"当前剩下的最大值"，从后往前填。
 *       （想要降序就把堆建成最小堆，或者反过来从前往后填。）
 * ===================================================================== */
void heap_sort_range(int a[], int lo, int hi, int verbose, HeapStat *st)
{
    if (lo >= hi) {
        return;
    }
    build_max_heap(a, lo, hi, verbose, st);
    for (int end = hi; end > lo; end--) {
        swap_int(&a[lo], &a[end]);
        if (st != NULL) {
            st->swap++;
        }
        sink_range(a, lo, end - 1, lo, verbose, st);
    }
}

/* =====================================================================
 * 工程版入口：三行，什么都不打印
 * ===================================================================== */
void heap_sort(int a[], int n)
{
    if (a == NULL || n < 2) {
        return;
    }
    heap_sort_range(a, 0, n - 1, 0, NULL);
}

/* 教学版：把两个阶段分开走，每步都把数组和树打出来 */
void heap_sort_trace(int a[], int n, int verbose, HeapStat *st)
{
    if (st != NULL) {
        st->cmp = 0;
        st->swap = 0;
        st->sink = 0;
    }
    if (a == NULL || n < 2) {
        return;
    }
    if (verbose) {
        printf("  ◆ 起点：随便一个数组，此时它【不是】堆\n");
        print_arr("      数组: ", a, n);
        print_heap_tree("      ", a, 0, n - 1);
    }
    printf("  ◆ 阶段 1：建最大堆 —— 从 a[%d] 开始倒着把每个非叶子沉一次\n",
           (n - 2) / 2);
    build_max_heap(a, 0, n - 1, verbose, st);
    printf("      阶段 1 结束：\n");
    print_arr("      数组: ", a, n);
    print_heap_tree("      ", a, 0, n - 1);
    long cmp_after_build = (st != NULL) ? st->cmp : 0;
    printf("      建堆累计：比较 %ld 次（数组长度 n=%d ⇒ 大约 2n 的量级，不是 n log n）\n",
           cmp_after_build, n);

    printf("  ◆ 阶段 2：反复「堆顶 ↔ 末尾换 → 堆缩 1 → 新堆顶下沉」\n");
    for (int end = n - 1; end > 0; end--) {
        printf("|      第 %d 次取数：堆顶 a[0]=%d 是剩下 %d 个数里最大的 ⇒ 和末尾 a[%d]=%d 换\n",
               n - 1 - end + 1, a[0], end + 1, end, a[end]);
        swap_int(&a[0], &a[end]);
        if (st != NULL) {
            st->swap++;
        }
        print_split("      交换后（| 右边已锁死）: ", a, end, n);
        sink_range(a, 0, end - 1, 0, verbose, st);
        if (end - 1 == 3) {
            /* 顺便看一眼"堆缩到 4 个时树长什么样"，直观感受树在变矮 */
            print_heap_tree("      此时剩下的堆只有 4 个：", a, 0, end - 1);
        }
    }
    printf("      阶段 2 结束，堆缩到 1 个元素（它必然是最小值）⇒ 不用处理：\n");
    print_arr("      最终: ", a, n);
    if (st != NULL) {
        printf("      阶段 2 累计：比较 %ld 次，节点累计下沉 %ld 层（每取一次最多走 log n 层）\n",
               st->cmp - cmp_after_build, st->sink);
    }
}

/* =====================================================================
 * 极简动态最大堆：和原地版用同一套下标公式，只是多管一个 size
 * =====================================================================
 * 为什么还要写它？两个目的：
 *   ① 证明"堆 = 数组 + 三个公式"不是话术：push 时新元素就是"塞到数组末尾再往上冒"，
 *      pop 时就是"末尾补到堆顶再往下沉" —— 用的还是 sink 那套。
 *   ② 堆排序本身【不需要】这个结构体（数组长度已知、原地跑完就结束，没有增删）；
 *      一旦要处理"数据一条条来"（流式、Top-K），才需要它。
 * ===================================================================== */

int heap_init(Heap *h, int cap)
{
    if (h == NULL || cap <= 0) {
        return 0;
    }
    h->data = malloc((size_t)cap * sizeof(int));
    if (h->data == NULL) {
        h->cap = 0;
        h->size = 0;
        return 0;
    }
    h->cap = cap;
    h->size = 0;
    return 1;
}

void heap_free(Heap *h)
{
    if (h == NULL) {
        return;
    }
    free(h->data);
    h->data = NULL;   /* 置空：万一有人手贱再 push，会立刻在 heap_push 的容量检查里暴露 */
    h->cap = 0;
    h->size = 0;
}

/* 往上冒：新元素放在数组末尾（保持"完全"），只要比爹大就换上去 */
static void swim(int a[], int i, HeapStat *st)
{
    while (i > 0) {
        int p = (i - 1) / 2;
        if (st != NULL) st->cmp++;
        if (a[p] >= a[i]) {
            return;   /* 爹已经够大 ⇒ 堆性质恢复 */
        }
        swap_int(&a[p], &a[i]);
        if (st != NULL) st->swap++;
        i = p;
    }
}

void heap_push(Heap *h, int v, HeapStat *st)
{
    if (h == NULL || h->data == NULL || h->size >= h->cap) {
        printf("      [警告] heap_push：堆已满或未初始化，这个值 %d 被丢掉了\n", v);
        return;
    }
    h->data[h->size] = v;
    swim(h->data, h->size, st);
    h->size++;
}

int heap_peek_max(const Heap *h)
{
    if (h == NULL || h->data == NULL || h->size == 0) {
        return INT_MIN;   /* 空堆给个"比谁都小"的哨兵；真实工程里这里该 assert */
    }
    return h->data[0];
}

int heap_pop_max(Heap *h, HeapStat *st)
{
    if (h == NULL || h->data == NULL || h->size == 0) {
        printf("      [警告] heap_pop_max：空堆，返回 INT_MIN\n");
        return INT_MIN;
    }
    int top = h->data[0];
    h->size--;                       /* 堆先缩小：末尾那个格子就不参与下沉了 */
    h->data[0] = h->data[h->size];   /* 拿最后一个补到顶上（它就是"堆排序里的远距离交换"） */
    if (h->size > 0) {
        sink_range(h->data, 0, h->size - 1, 0, 0, st);
    }
    return top;
}

/* =====================================================================
 * 稳定性实验：分数 + 身份证（平行数组）
 * =====================================================================
 * key 相同的两个人，排完之后还想保持原来的先后顺序 ⇒ 才叫"稳定"。
 * 堆排序的 key 换到哪，tag 就同步换到哪 —— 两条数组用【同一个】swap，
 * 这样打印出来的 88a / 88b 就能直接看出顺序有没有被打乱。
 * ===================================================================== */

void print_tagged(const char *label, const int key[], const int tag[], int n)
{
    printf("%s[", label);
    for (int i = 0; i < n; i++) {
        printf(" %d%c", key[i], (char)('a' + tag[i]));
    }
    printf(" ]\n");
}

/* 把 sink / 建堆 / 取数整套重做一遍，但每次交换都同时作用于 key 和 tag。
   （不复用 heap_sort_range 是因为它只搬一个数组 —— 这就是 int-only 约束的代价，
     宁可写重复代码，也不引入 void* 或函数指针。） */
static void sink_tagged(int key[], int tag[], int root, int size)
{
    for (;;) {
        int left = 2 * root + 1;
        int right = left + 1;
        int largest = root;
        if (left < size && key[left] > key[largest]) {
            largest = left;
        }
        if (right < size && key[right] > key[largest]) {
            largest = right;
        }
        if (largest == root) {
            return;
        }
        swap_int(&key[root], &key[largest]);
        swap_int(&tag[root], &tag[largest]);   /* ← 关键：tag 跟着 key 一起走 */
        root = largest;
    }
}

void heap_sort_tagged(int key[], int tag[], int n)
{
    if (key == NULL || tag == NULL || n < 2) {
        return;
    }
    for (int i = (n - 2) / 2; i >= 0; i--) {
        sink_tagged(key, tag, i, n);
    }
    for (int end = n - 1; end > 0; end--) {
        swap_int(&key[0], &key[end]);
        swap_int(&tag[0], &tag[end]);
        sink_tagged(key, tag, 0, end);
    }
}

/* =====================================================================
 * 对照算法：极简快排 / 插入 / introsort
 * =====================================================================
 * 为什么本目录要自带快排？Makefile 是按目录独立编译的，不能 #include ../quick-sort/，
 * 但"堆排序比快排慢多少""堆排序作为快排兜底值多少钱"这两个结论必须实测出来才作数，
 * 所以这里重写一份 30 行的 Lomuto 快排当对手。
 *
 * 注意 mini_quick_sort 故意【不做】三数取中、固定取末位枢轴 ——
 * 因为它要扮演的正是"会被升序数据坑死的裸快排"，这样第 7 幕的对比才有戏剧性。
 * ===================================================================== */

static void mini_insertion_range(int a[], int lo, int hi)
{
    for (int i = lo + 1; i <= hi; i++) {
        int key = a[i];
        int j = i - 1;
        while (j >= lo && a[j] > key) {   /* >= 换成 > 才稳定；这里 > ⇒ 相等就停 ⇒ 稳定 */
            a[j + 1] = a[j];
            j--;
        }
        a[j + 1] = key;
    }
}

void mini_insertion_sort(int a[], int n)
{
    if (a == NULL || n < 2) {
        return;
    }
    mini_insertion_range(a, 0, n - 1);
}

static int mini_partition(int a[], int lo, int hi, long *cmp)
{
    int pivot = a[hi];
    int i = lo - 1;
    for (int j = lo; j < hi; j++) {
        if (cmp != NULL) (*cmp)++;
        if (a[j] <= pivot) {
            i++;
            if (i != j) {
                swap_int(&a[i], &a[j]);
            }
        }
    }
    if (i + 1 != hi) {
        swap_int(&a[i + 1], &a[hi]);      /* 就是这一次远距离交换让快排不稳定 */
    }
    return i + 1;
}

static void mini_quick_range(int a[], int lo, int hi, long *cmp)
{
    while (lo < hi) {
        int p = mini_partition(a, lo, hi, cmp);
        /* 先递归短的那半边，长的那半边用循环吃掉 ⇒ 栈 O(log n) */
        if (p - lo < hi - p) {
            mini_quick_range(a, lo, p - 1, cmp);
            lo = p + 1;
        } else {
            mini_quick_range(a, p + 1, hi, cmp);
            hi = p - 1;
        }
    }
}

void mini_quick_sort(int a[], int n, long *cmp_out)
{
    if (a == NULL || n < 2) {
        return;
    }
    mini_quick_range(a, 0, n - 1, cmp_out);
}

/*
 * 三数取中：先看 a[lo]、a[mid]、a[hi] 三个位置，把它们就地排成升序，
 * 这样中位数落在 a[mid]；最后再【把中位数换到 a[hi]】—— 那正是 Lomuto 要的枢轴位。
 *
 * ⚠ 容易漏的就是最后那一次 swap：只排三个数、不把中位数搬到 hi，
 *   枢轴仍然是 a[hi] = 三者里的【最大值】，那么喂进升序数组照样塌成 O(n²)。
 *   （本节第一版就漏了这一步，实测数据 B 上 median3 版和裸快排的比较次数一模一样，
 *    8398842 vs 8386560 —— 一眼看出"取中"根本没生效。补上之后才是下面看到的数字。）
 */
static void mini_median3(int a[], int lo, int hi, long *cmp)
{
    if (hi - lo < 2) {
        return;             /* 两个元素以下没有"取中"的意义 */
    }
    int mid = lo + (hi - lo) / 2;
    if (cmp != NULL) (*cmp)++;
    if (a[lo] > a[mid]) {
        swap_int(&a[lo], &a[mid]);
    }
    if (cmp != NULL) (*cmp)++;
    if (a[lo] > a[hi]) {
        swap_int(&a[lo], &a[hi]);
    }
    if (cmp != NULL) (*cmp)++;
    if (a[mid] > a[hi]) {
        swap_int(&a[mid], &a[hi]);
    }
    /* 现在 a[lo] ≤ a[mid] ≤ a[hi] ⇒ 把中位数搬到枢轴位 */
    swap_int(&a[mid], &a[hi]);
}

static void mini_quick3_range(int a[], int lo, int hi, long *cmp)
{
    while (lo < hi) {
        mini_median3(a, lo, hi, cmp);
        int p = mini_partition(a, lo, hi, cmp);
        /* 同样先递归短半边、长半边用循环吃掉 ⇒ 栈 O(log n) */
        if (p - lo < hi - p) {
            mini_quick3_range(a, lo, p - 1, cmp);
            lo = p + 1;
        } else {
            mini_quick3_range(a, p + 1, hi, cmp);
            hi = p - 1;
        }
    }
}

void mini_quick_sort3(int a[], int n, long *cmp_out)
{
    if (a == NULL || n < 2) {
        return;
    }
    mini_quick3_range(a, 0, n - 1, cmp_out);
}

/* 向下取整的 log2：depth 上限取 2*floor(log2(n))，是 introsort 的经典阈值。
   含义：如果一棵递归树还"健康"（每层至少劈掉一半），深度绝不会超过 log2(n)；
   一旦超过 2 倍，说明劈得非常不均匀 ⇒ 立刻交给堆排序，把最坏情况兜住。 */
static int floor_log2(int n)
{
    int d = 0;
    while (n > 1) {
        n /= 2;
        d++;
    }
    return d;
}

static void intro_range(int a[], int lo, int hi, int depth_left,
                        long *cmp, long *switch_to_heap)
{
    while (lo < hi) {
        int size = hi - lo + 1;
        if (size <= 12) {
            mini_insertion_range(a, lo, hi);   /* 小段切插入：递归开销比 O(n²) 更贵 */
            return;
        }
        if (depth_left == 0) {
            /* 兜底：这一段交给堆排序 —— 它最坏也是 O(m log m)，不会继续塌 */
            if (switch_to_heap != NULL) (*switch_to_heap)++;
            heap_sort_range(a, lo, hi, 0, NULL);
            return;
        }
        depth_left--;
        mini_median3(a, lo, hi, cmp);         /* 真实的 introsort 也是带三数取中的 */
        int p = mini_partition(a, lo, hi, cmp);
        if (p - lo < hi - p) {
            intro_range(a, lo, p - 1, depth_left, cmp, switch_to_heap);
            lo = p + 1;
        } else {
            intro_range(a, p + 1, hi, depth_left, cmp, switch_to_heap);
            hi = p - 1;
        }
    }
}

void intro_sort(int a[], int n, int depth_limit, long *cmp_out, long *switch_to_heap_out)
{
    if (a == NULL || n < 2) {
        return;
    }
    if (switch_to_heap_out != NULL) {
        *switch_to_heap_out = 0;
    }
    int limit = (depth_limit > 0) ? depth_limit : 2 * floor_log2(n);
    intro_range(a, 0, n - 1, limit, cmp_out, switch_to_heap_out);
}
