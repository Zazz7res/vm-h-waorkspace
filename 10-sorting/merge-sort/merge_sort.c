#include <stdio.h>
#include <stdlib.h>
#include "merge_sort.h"

/*
 * ===================================================================
 * 0. 内部小工具
 * ===================================================================
 * 本目录必须自包含（Makefile 按目录独立编译，跨目录 include 编译不过），
 * 所以 print_arr / is_sorted_ascending / same_elements 在这里再写一份极简版。
 */
void print_arr(const char *label, const int a[], int n)
{
    printf("%s[", label);
    for (int i = 0; i < n; i++) {
        printf(" %d", a[i]);
    }
    printf(" ]\n");
}

int is_sorted_ascending(const int a[], int n)
{
    for (int i = 1; i < n; i++) {
        if (a[i - 1] > a[i]) {
            return 0;
        }
    }
    return 1;
}

int same_elements(const int a[], const int b[], int n)
{
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

/* 打印 a[lo..hi] 这一小段，用中括号包起来，方便看"段"的概念 */
static void print_span(const int a[], int lo, int hi)
{
    printf("[");
    for (int i = lo; i <= hi; i++) {
        printf(" %d", a[i]);
    }
    printf(" ]");
}

/* 按递归深度缩进，让"拆 - 合"的树形在终端里看得见 */
static void indent(int depth)
{
    for (int i = 0; i < depth; i++) {
        printf("    ");
    }
    printf("| ");
}

/*
 * ===================================================================
 * 1. 发动机：merge_ranges —— 全算法就这十几行，必须逐行看懂
 * ===================================================================
 * 【干什么】a[lo..mid] 和 a[mid+1..hi] 这两段【各自】已经有序，
 *          把它们合成一段有序的，结果写回 a[lo..hi]。
 *
 * 【为什么要先抄进 tmp】
 *   待会儿要往 a[lo..hi] 从头开始覆盖式写回。如果直接边比边写 a，
 *   写 a[lo] 的时候还没读的数据就在 a[lo+1]、a[lo+2]…… 一覆盖就丢。
 *   所以先把这两段【原样】抄到 tmp 里当作"只读的原料"，
 *   然后放心地往 a 里写 —— 这就是归并需要 O(n) 额外空间的根本原因。
 *
 * 【怎么合：两个指针各看自己那半边，谁小取谁】
 *   i 从头扫左半（只允许走 lo..mid），j 从头扫右半（只允许走 mid+1..hi），k 是写回位置。
 *   每步比较 tmp[i] 和 tmp[j]，小的那个抄到 a[k]，然后那个指针 +1、k +1。
 *   因为两段【内部已经有序】，"当前最小的一定在两个指针之一上"—— 这就是敢只比两个数的理由。
 *
 *   tmp:      [ 27 38 ]      [  3 43  ]      ← 两段各自有序
 *              ↑i                 ↑j
 *   a :  [ __ __ __ __ ]         ← k 从这里往里写
 *         ↑k
 *   比 27 vs 3 → 3 小 → a[0]=3, j++, k++
 *   比 27 vs 43 → 27 小 → a[1]=27, i++, k++
 *   比 38 vs 43 → 38 小 → a[2]=38, i++, k++
 *   i 越界（左半空了）→ 把右半剩下的 43 直接抄完 —— 不用再比，它本来就有序
 *
 * 【★ 稳定性就在这一行：tmp[i] <= tmp[j] 取左边】
 *   两边一样大时，我们【优先取左边】。左边那段是原始数组里靠前的部分，
 *   所以相等元素的相对顺序永远保持"前面的还在前面"。
 *   把 <= 改成 < 试试：结果照样升序，但稳定性立刻消失（本节实验能测出来）。
 */
void merge_ranges(int a[], int tmp[], int lo, int mid, int hi, MergeStat *st, int verbose)
{
    /* ① 把 a[lo..hi] 原样抄到 tmp：tmp 变成只读原料 */
    for (int p = lo; p <= hi; p++) {
        tmp[p] = a[p];
        if (st != NULL) {
            st->copy++;
        }
    }

    if (verbose) {
        printf("        合并 左段 [%d..%d]=", lo, mid);
        print_span(tmp, lo, mid);
        printf(" + 右段 [%d..%d]=", mid + 1, hi);
        print_span(tmp, mid + 1, hi);
        printf(" →");
    }

    int i = lo;         /* 左段指针：只走 lo..mid */
    int j = mid + 1;    /* 右段指针：只走 mid+1..hi */
    int k = lo;         /* 写回位置 */

    /* ② 两段都还有货时，两两对比，小的抄走 */
    while (i <= mid && j <= hi) {
        if (st != NULL) {
            st->cmp++;
        }
        if (tmp[i] <= tmp[j]) {           /* ★ 等号给左边 ⇒ 稳定 */
            a[k++] = tmp[i++];
            if (verbose) {
                printf(" 取左%d", a[k - 1]);
            }
        } else {
            a[k++] = tmp[j++];
            if (verbose) {
                printf(" 取右%d", a[k - 1]);
            }
        }
        if (st != NULL) {
            st->copy++;
        }
    }

    /* ③ 左段还剩：直接抄完（它们都比已经写进去的大，而且自己已经有序） */
    while (i <= mid) {
        a[k++] = tmp[i++];
        if (st != NULL) {
            st->copy++;
        }
    }
    /* ④ 右段还剩：同样直接抄完 —— 注意这一步【不需要】任何比较 */
    while (j <= hi) {
        a[k++] = tmp[j++];
        if (st != NULL) {
            st->copy++;
        }
    }

    if (st != NULL) {
        st->merges++;
    }
    if (verbose) {
        printf("  得");
        print_span(a, lo, hi);
        printf("\n");
    }
}

/*
 * ===================================================================
 * 2. 递归骨架：拆 → 递归拆 → 合
 * ===================================================================
 * 只有一句核心：lo >= hi 就是"这段只剩 0 或 1 个元素"，天然有序，直接返回。
 * mid 用 lo + (hi - lo) / 2 而不是 (lo + hi) / 2 —— 后者在 lo、hi 都很大时
 * 会先相加溢出（11-searching/binary-search 有专门一课讲这个坑）。
 */
static void merge_sort_range(int a[], int tmp[], int lo, int hi, int n,
                             int depth, int verbose, MergeStat *st)
{
    if (lo >= hi) {
        return;                    /* 递归出口：一段只剩 1 个（或 0 个）元素 */
    }

    int mid = lo + (hi - lo) / 2;

    if (verbose) {
        indent(depth);
        printf("拆 [%d..%d] → [%d..%d] + [%d..%d]\n", lo, hi, lo, mid, mid + 1, hi);
    }

    merge_sort_range(a, tmp, lo, mid, n, depth + 1, verbose, st);         /* 左边排好 */
    merge_sort_range(a, tmp, mid + 1, hi, n, depth + 1, verbose, st);     /* 右边排好 */
    merge_ranges(a, tmp, lo, mid, hi, st, verbose);                    /* 再合起来 */

    if (verbose) {
        indent(depth);
        printf("合并完 [%d..%d]，整个数组现在长这样：", lo, hi);
        print_span(a, 0, n - 1);
        printf("\n");
    }
}

/*
 * 工程版入口：自己 malloc 一份 tmp，全程复用，最后 free。
 * 谁申请谁释放 —— 这里申请就在这里还掉，调用者拿不到 tmp，也就不会泄漏。
 */
void merge_sort(int a[], int n)
{
    if (n <= 1) {
        return;                          /* 0 个或 1 个：没什么可排 */
    }
    int *tmp = malloc((size_t)n * sizeof(int));
    if (tmp == NULL) {
        printf("      [错误] merge_sort 申请 %d 个 int 的临时数组失败\n", n);
        return;                          /* 本节不做复杂错误传播：直接放弃排序 */
    }
    merge_sort_range(a, tmp, 0, n - 1, n, 0, 0, NULL);
    free(tmp);
    tmp = NULL;                          /* 好习惯：free 完置 NULL，防止误用 */
}

/*
 * 教学版入口：逻辑和工程版完全一样，只是把 verbose / 统计带进递归。
 */
void merge_sort_trace(int a[], int n, int verbose, MergeStat *st)
{
    MergeStat local;
    local.cmp = 0; local.copy = 0; local.merges = 0;

    if (n > 1) {
        int *tmp = malloc((size_t)n * sizeof(int));
        if (tmp == NULL) {
            printf("      [错误] merge_sort_trace 申请 %d 个 int 的临时数组失败\n", n);
            return;
        }
        if (verbose) {
            print_arr("      排之前: ", a, n);
        }
        merge_sort_range(a, tmp, 0, n - 1, n, 0, verbose, &local);
        free(tmp);
        tmp = NULL;
    }

    if (st != NULL) {
        *st = local;
    }
}

/*
 * ===================================================================
 * 3. 自底向上版：不用递归，按"段宽"一层一层合
 * ===================================================================
 * 为什么也要会这一版：
 *   ① 递归版在 n 很大时栈上有 log n 层调用；这版一行递归都没有，栈开销 0。
 *   ② 它把"每一层"的概念摊得很开：宽 1 的两两合 → 宽 2 的两两合 → 宽 4 → ...
 *      正好对应"归并排序每一层都是 O(n)，一共 log n 层 ⇒ O(n log n)"。
 *   ③ 合并时最麻烦的是"尾巴不够长"（奇数段、最后一段短），下面用 mid/hi 取 min 处理。
 */
void merge_sort_bottom_up(int a[], int n, int verbose, MergeStat *st)
{
    MergeStat local;
    local.cmp = 0; local.copy = 0; local.merges = 0;

    if (n <= 1) {
        if (st != NULL) { *st = local; }
        return;
    }
    int *tmp = malloc((size_t)n * sizeof(int));
    if (tmp == NULL) {
        printf("      [错误] merge_sort_bottom_up 申请 %d 个 int 的临时数组失败\n", n);
        return;
    }

    int level = 0;
    for (int width = 1; width < n; width *= 2) {        /* 每合完一轮，段宽翻倍 */
        if (verbose) {
            printf("      第 %d 层（每次合并的两段宽度都是 %d）：\n", level + 1, width);
        }
        for (int lo = 0; lo < n - width; lo += 2 * width) {
            int mid = lo + width - 1;                       /* 左段最后一个下标 */
            int hi = mid + width;                           /* 右段理论最后下标 */
            if (hi > n - 1) {
                hi = n - 1;                                 /* 尾巴不够长就截到 n-1 */
            }
            merge_ranges(a, tmp, lo, mid, hi, &local, verbose);
        }
        if (verbose) {
            print_arr("        这一层合完整个数组 = ", a, n);
        }
        level++;
    }

    free(tmp);
    tmp = NULL;
    if (st != NULL) {
        *st = local;
    }
}

/*
 * ===================================================================
 * 4. 稳定性实验：key 比大小，tag 跟着人一起搬
 * ===================================================================
 * 和主算法完全同构，唯一区别是：每个格子搬的时候都"搬两份"（分数 + 身份证）。
 * 稳定性证明的做法：排序前 tag 是 0,1,2,...（报名顺序），
 *                  排序后把每段同分的人的 tag 顺序打出来，看有没有变成倒序。
 */
void print_tagged(const char *label, const int key[], const int tag[], int n)
{
    printf("%s[", label);
    for (int i = 0; i < n; i++) {
        printf(" %d%c", key[i], (char)('a' + tag[i]));
    }
    printf(" ]\n");
}

static void merge_tagged_ranges(int key[], int tag[],
                                int tmp_key[], int tmp_tag[],
                                int lo, int mid, int hi)
{
    for (int p = lo; p <= hi; p++) {          /* ① 两条数组一起抄进临时区 */
        tmp_key[p] = key[p];
        tmp_tag[p] = tag[p];
    }

    int i = lo;
    int j = mid + 1;
    int k = lo;
    while (i <= mid && j <= hi) {
        if (tmp_key[i] <= tmp_key[j]) {       /* ② ★ 等号优先取左边 ⇒ 稳定 */
            key[k] = tmp_key[i];
            tag[k] = tmp_tag[i];              /*    身份证跟着分数一起走 */
            i++;
        } else {
            key[k] = tmp_key[j];
            tag[k] = tmp_tag[j];
            j++;
        }
        k++;
    }
    while (i <= mid) {
        key[k] = tmp_key[i];
        tag[k] = tmp_tag[i];
        i++; k++;
    }
    while (j <= hi) {
        key[k] = tmp_key[j];
        tag[k] = tmp_tag[j];
        j++; k++;
    }
}

static void merge_tagged_range(int key[], int tag[],
                               int tmp_key[], int tmp_tag[], int lo, int hi)
{
    if (lo >= hi) {
        return;
    }
    int mid = lo + (hi - lo) / 2;
    merge_tagged_range(key, tag, tmp_key, tmp_tag, lo, mid);
    merge_tagged_range(key, tag, tmp_key, tmp_tag, mid + 1, hi);
    merge_tagged_ranges(key, tag, tmp_key, tmp_tag, lo, mid, hi);
}

int merge_sort_tagged(int key[], int tag[], int n)
{
    if (n <= 1) {
        return 1;
    }
    int *tmp_key = malloc((size_t)n * sizeof(int));
    int *tmp_tag = malloc((size_t)n * sizeof(int));
    if (tmp_key == NULL || tmp_tag == NULL) {
        free(tmp_key);                 /* free(NULL) 合法，所以不用分别判空 */
        free(tmp_tag);
        return 0;
    }
    merge_tagged_range(key, tag, tmp_key, tmp_tag, 0, n - 1);
    free(tmp_key);  tmp_key = NULL;    /* 谁申请谁释放，申请两次释放两次 */
    free(tmp_tag);  tmp_tag  = NULL;
    return 1;
}

/*
 * ===================================================================
 * 5. 归并思想的应用：统计逆序对
 * ===================================================================
 * 逆序对定义：i < j 但 a[i] > a[j]（前面有个比后面大的）—— 数组"乱度"的量化。
 * 朴素算法：双重循环 O(n²)。
 * 归并算法：只在【合并】的那一刻顺手统计，总代价还是 O(n log n)。
 *
 * 核心那一句话：
 *   合并时两段各自有序。一旦【右段的那个人先出队】（tmp[j] < tmp[i]），
 *   说明左段里从 i 到 mid 的这些人【全部】都比他大（因为左段有序，a[i..mid] 递增，
 *   连最小的 tmp[i] 都比他大，后面的只会更大）。
 *   而他们在原数组里又都排在他【前面】⇒ 一次性产生 (mid - i + 1) 个逆序对。
 *   这就是"合并时右边先出队 ⇒ 加上左边剩余个数"。
 *
 *   左段 [ 3 5 7 ]      右段 [ 2 8 ]        ← 两段各自已经有序
 *          ↑i                ↑j
 *   比 3 vs 2 → 2 小 → 2 先出队！左段还剩 3 个数（3 5 7），它们都比 2 大、
 *                     而且原来都在 2 前面 ⇒ 逆序对 +3：(3,2)(5,2)(7,2)
 */
static long inv_range(int a[], int tmp[], int lo, int hi, int verbose, int depth)
{
    if (lo >= hi) {
        return 0;                       /* 一个元素内部没有逆序对 */
    }

    int mid = lo + (hi - lo) / 2;
    long total = inv_range(a, tmp, lo, mid, verbose, depth + 1)      /* 左段内部的 */
               + inv_range(a, tmp, mid + 1, hi, verbose, depth + 1); /* 右段内部的 */

    for (int p = lo; p <= hi; p++) {
        tmp[p] = a[p];
    }

    int i = lo;
    int j = mid + 1;
    int k = lo;
    while (i <= mid && j <= hi) {
        if (tmp[i] <= tmp[j]) {
            a[k++] = tmp[i++];          /* 左边先出队 ⇒ 不产生跨段逆序对 */
        } else {
            /* 右边先出队 ⇒ 左段 [i..mid] 全部 > tmp[j]，每人配一对 */
            long add = (long)(mid - i + 1);
            total += add;
            if (verbose) {
                indent(depth);
                printf("右段 %d 先出队：左段还剩 %ld 个都比它大（", tmp[j], add);
                for (int p = i; p <= mid; p++) {
                    printf("(%d,%d) ", tmp[p], tmp[j]);
                }
                printf("）⇒ 新增 %ld 对，这段 [ %d..%d ] 小计 %ld 对\n", add, lo, hi, total);
            }
            a[k++] = tmp[j++];
        }
    }
    while (i <= mid) { a[k++] = tmp[i++]; }
    while (j <= hi)  { a[k++] = tmp[j++]; }

    return total;
}

long count_inversions(const int a[], int n, int verbose)
{
    if (n <= 1) {
        return 0;
    }
    /* 归并会打乱数组，所以先造一份工作副本，输入数组 const 只读 */
    int *work = malloc((size_t)n * sizeof(int));
    int *tmp  = malloc((size_t)n * sizeof(int));
    if (work == NULL || tmp == NULL) {
        free(work);
        free(tmp);
        printf("      [错误] count_inversions 内存申请失败\n");
        return -1;                      /* -1 = 失败标记 */
    }
    for (int i = 0; i < n; i++) {
        work[i] = a[i];
    }

    long result = inv_range(work, tmp, 0, n - 1, verbose, 0);

    free(work); work = NULL;
    free(tmp);  tmp  = NULL;
    return result;
}

/*
 * 笨办法：两个 for 直接数。O(n²)，但【绝对不会想错】，
 * 用它来验证上面那个聪明办法对不对 —— 这叫"对拍"，11-searching 会大量用到。
 */
long count_inversions_bruteforce(const int a[], int n)
{
    long cnt = 0;
    for (int i = 0; i < n; i++) {
        for (int j = i + 1; j < n; j++) {
            if (a[i] > a[j]) {
                cnt++;
            }
        }
    }
    return cnt;
}
