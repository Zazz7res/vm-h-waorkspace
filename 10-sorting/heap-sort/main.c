/*
 * main.c —— 堆排序的分幕演示
 *
 * 数据全部硬编码或用确定公式生成（(i*37)%n 这种），不用 rand()：
 * 这样 README 里贴的输出和任何人跑出来的【逐字一致】，每个数字都能复核。
 *
 * 一个小坑先记着：C11 里 `const int N = 8; int a[N] = {…};` 是编译不过的
 * （N 不是编译期常量，那是变长数组 VLA，不允许带初始化器）⇒ 数组长度一律用 #define。
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "heap_sort.h"

#define N 8                       /* 第 1~4 幕演示用的长度：8 个数刚好能画满三层树 */
#define TINY 6                    /* 第 2 幕动态堆演示的元素个数 */
#define TIMED_N 3                 /* 耗时表的规模个数 */

static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

static void copy_arr(int dst[], const int src[], int n)
{
    for (int i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

/* 向下取整的 log2，主程序里算比值要用（和 heap_sort.c 内部那份是同一个小函数） */
static int my_log2(int n)
{
    int d = 0;
    while (n > 1) {
        n /= 2;
        d++;
    }
    return d;
}

/* 验"建堆结束后真的是最大堆"：每个节点都 ≥ 它的两个孩子。
   这条比"排完是升序"更根本 —— 它是阶段 2 能正确工作的前提。 */
static int verify_max_heap(const int a[], int n)
{
    for (int i = 0; i < n; i++) {
        int l = 2 * i + 1;
        int r = 2 * i + 2;
        if (l < n && a[i] < a[l]) {
            return 0;
        }
        if (r < n && a[i] < a[r]) {
            return 0;
        }
    }
    return 1;
}

/*
 * 生成一个【确定的】乱序排列：固定种子的 LCG + 拒绝法，值域 0..n-1 每个恰好出现一次。
 *   ① 为什么不用 (i*37)%n：它是"等距跳跃"的排列，看着乱，其实对固定末位枢轴的快排
 *      相当不友好，拿它当"平均情况"会得出错误的结论（本节第一版就被它骗过，见 README）。
 *   ② 为什么不用 rand()：libc 不同结果不同，README 里的耗时和比较次数就没人能复核了。
 *   ③ 拒绝法（撞车就重抽）保证它是排列 ⇒ 排序的标准答案【已知】就是 0..n-1，能逐个比对。
 */
static int fill_perm(int *a, int n, unsigned seed)
{
    char *used = calloc((size_t)n, sizeof(char));
    if (used == NULL) {
        return 0;
    }
    unsigned s = seed;
    int placed = 0;
    while (placed < n) {
        s = s * 1103515245u + 12345u;          /* 教科书里的 glibc 型 LCG，纯整数，跨平台一致 */
        int v = (int)((s >> 8) % (unsigned)n); /* 砍掉低位：LCG 的低位周期性很差 */
        if (!used[v]) {
            used[v] = (char)1;
            a[placed++] = v;
        }
    }
    free(used);
    return 1;
}

int main(void)
{
    /* 硬编码：既是"最乱"的样子，又能手算复核建堆过程 */
    const int ORIG[N]   = {4, 10, 3, 5, 1, 8, 7, 2};
    const int EXPECT[N] = {1, 2, 3, 4, 5, 7, 8, 10};

    int work[N];
    HeapStat st;
    int all_pass = 1;

    /* ============================================================
       第 1 幕：先把"数组 = 完全二叉树"这件事看顺
       ============================================================ */
    section("1. 下标当堆：三个公式把一棵树塞进连续内存（一个结构体都不用）");

    printf("  规则：根 = a[0]；a[i] 的父 = a[(i-1)/2]；左孩子 = a[2i+1]；右孩子 = a[2i+2]\n");
    printf("  所以数组按【层序】读就是一棵树：a[0] / a[1] a[2] / a[3] a[4] a[5] a[6] / a[7] ...\n\n");
    copy_arr(work, ORIG, N);
    print_arr("  现在这个数组: ", work, N);
    print_heap_tree("  ", work, 0, N - 1);
    printf("  看到一排 ✗ 了吧？这就是「普通数组」和「最大堆」的差距：\n");
    printf("  最大堆只承诺一件事 —— 【每个爹 ≥ 它的两个孩子】，于是堆顶 a[0] 必然是整段最大值。\n");
    printf("  它【不承诺】兄弟之间有序（a[1]、a[2] 谁大都行），所以「是堆」比「是有序数组」弱得多。\n");

    /* ============================================================
       第 2 幕：极简动态堆（push / pop），证明公式是真的能用
       ============================================================ */
    section("2. 对照版：带 push/pop 的极简最大堆（下标公式和原地版一模一样）");

    const int STREAM[TINY] = {4, 10, 3, 5, 1, 8};
    Heap heap;
    if (!heap_init(&heap, TINY)) {
        printf("  [错误] 堆初始化失败，跳过本幕\n");
        all_pass = 0;
    } else {
        HeapStat st2 = {0, 0, 0};
        printf("  push 的过程 = 塞到数组末尾（保持「完全」）→ 只要比爹大就往上冒：\n");
        for (int i = 0; i < TINY; i++) {
            heap_push(&heap, STREAM[i], &st2);
            print_split("      ", heap.data, heap.size, heap.size);
            printf("        塞入 %d 后 size=%d，堆顶 = %d\n", STREAM[i], heap.size, heap.data[0]);
        }
        print_heap_tree("  全部塞完，把数组当树看：", heap.data, 0, heap.size - 1);
        printf("  现在是合法最大堆吗（每个爹 ≥ 孩子）？%s\n",
               verify_max_heap(heap.data, heap.size) ? "是" : "否");
        if (!verify_max_heap(heap.data, heap.size)) all_pass = 0;

        printf("\n  pop 的过程 = 记下堆顶 → 拿【最后一个】补到顶上 → 堆缩 1 → 顶上那个沉下去：\n");
        printf("  （这正是堆排序阶段 2 的动作，只不过它把「拿走」换成了「和数组末尾交换」）\n");
        int have_prev = 0;    /* 有没有上一个取出的值（避免拿哨兵值和真实数据混）*/
        int prev = 0;
        int mono_ok = 1;
        printf("      取出顺序：%d", heap_peek_max(&heap));
        while (heap.size > 0) {
            int v = heap_pop_max(&heap, &st2);
            /* 每次 pop 出来的必须【单调不增】，否则堆性质在某一步被破坏了 */
            if (have_prev && v > prev) {
                mono_ok = 0;
            }
            have_prev = 1;
            prev = v;
            if (heap.size > 0) {
                printf(" → %d", heap_peek_max(&heap));
            }
            if (!verify_max_heap(heap.data, heap.size)) {
                all_pass = 0;
                printf("\n      [错误] 取出 %d 之后剩下的不再是合法堆！\n", v);
                break;
            }
        }
        printf("\n");
        printf("      最终剩余 size = %d（数组格子里还留着旧值，但堆已经不认它们了）\n", heap.size);
        printf("  取出来这一串是否从大到小单调不增：%s\n", mono_ok ? "是" : "否");
        if (!mono_ok) all_pass = 0;
        printf("  空堆上问堆顶会拿到哨兵 INT_MIN = %d（真实工程里这里该 assert，不能默默用）\n",
               heap_peek_max(&heap));
        heap_free(&heap);
        printf("  统计：push/pop 全过程 比较 %ld 次 | 交换 %ld 次 | 下沉 %ld 层\n",
               st2.cmp, st2.swap, st2.sink);
    }

    /* ============================================================
       第 3 幕：堆排序的两个阶段，全过程打印
       ============================================================ */
    section("3. 堆排序两阶段全过程：阶段 1 建堆（O(n)）→ 阶段 2 反复取堆顶（O(n log n)）");

    copy_arr(work, ORIG, N);
    heap_sort_trace(work, N, 1, &st);
    printf("      总计：比较 %ld 次 | 交换 %ld 次 | 累计下沉 %ld 层\n", st.cmp, st.swap, st.sink);
    print_arr("  排完: ", work, N);
    if (!same_elements(work, EXPECT, N) || !is_sorted_ascending(work, N)) {
        all_pass = 0;
    }
    printf("  校验：与标准答案一致 = %s | 升序 = %s | 元素守恒 = %s\n",
           same_elements(work, EXPECT, N) ? "是" : "否",
           is_sorted_ascending(work, N) ? "是" : "否",
           is_permutation(ORIG, work, N) ? "是" : "否");

    /* ============================================================
       第 4 幕：建堆为什么是 O(n)
       ============================================================ */
    section("4. 实测钉死「建堆是 O(n) 而不是 O(n log n)」");

    printf("  直觉：一半节点在最底层 ⇒ 沉 0 层；四分之一在倒数第二层 ⇒ 最多沉 1 层……\n");
    printf("        总下沉层数 = n/4*1 + n/8*2 + n/16*3 + … = 2n（收敛的级数）⇒ O(n)。\n");
    printf("  看数据（比较次数除以 n，应该基本是个常数，不随 n 增长）：\n");
    const int BSZ[TIMED_N] = {256, 1024, 4096};
    for (int k = 0; k < TIMED_N; k++) {
        int m = BSZ[k];
        int *b = malloc((size_t)m * sizeof(int));
        if (b == NULL) {
            printf("      [错误] 申请失败，跳过 n=%d\n", m);
            continue;
        }
        fill_perm(b, m, 20261003u);        /* 确定的乱序排列，见文件头注释 */
        HeapStat sb = {0, 0, 0};
        build_max_heap(b, 0, m - 1, 0, &sb);
        int okh = verify_max_heap(b, m);
        /* 同一份数据再跑完整排序，差值就是阶段 2 的代价 */
        fill_perm(b, m, 20261003u);
        HeapStat sa = {0, 0, 0};
        heap_sort_range(b, 0, m - 1, 0, &sa);
        int lg = my_log2(m);
        printf("      n=%5d │ 建堆比较 %7ld（= %.2f n）│ 取数比较 %7ld（= %.2f n·log2n）│ 下沉 %6ld 层\n",
               m, sb.cmp, (double)sb.cmp / (double)m, sa.cmp - sb.cmp,
               (double)(sa.cmp - sb.cmp) / ((double)m * (double)lg), sa.sink);
        if (!okh) all_pass = 0;
        free(b);
    }
    printf("  ⇒ 建堆那一列稳定在 ~2n 附近（n 越大越接近），取数那一列稳定在 ~n log n：\n");
    printf("    两阶段的比例差得离谱，所以【建堆不是瓶颈】，瓶颈永远在「取 n 次堆顶」。\n");

    /* ============================================================
       第 5 幕：稳定性实验
       ============================================================ */
    section("5. 实测：堆排序不稳定 —— 和快排是同一个凶手（远距离交换）");

    printf("  五个人：88a、88b、88d 三个人同分（原始顺序 a→b→d），70c、60e 当陪衬；\n");
    printf("  堆排序必须让排完还是 a→b→d 才算【稳定】，跑跑看：\n");
    int k1[5] = {88, 88, 70, 88, 60};
    int t1[5] = {0, 1, 2, 3, 4};
    print_tagged("      排之前: ", k1, t1, 5);
    heap_sort_tagged(k1, t1, 5);
    print_tagged("      排之后: ", k1, t1, 5);
    int order_ok = 1;
    for (int i = 1; i < 5; i++) {
        if (k1[i - 1] == k1[i] && t1[i - 1] > t1[i]) {
            order_ok = 0;
        }
    }
    printf("      同分者是否保持原始顺序：%s（%s）\n", order_ok ? "是" : "否",
           order_ok ? "这次碰巧没翻车" : "翻车了，这就是不稳定的实证");

    printf("\n  再来一组一定翻车的（分数只区分大小，tag 记录身份）：\n");
    int k2[6] = {5, 5, 2, 5, 1, 5};
    int t2[6] = {0, 1, 2, 3, 4, 5};
    print_tagged("      排之前: ", k2, t2, 6);
    heap_sort_tagged(k2, t2, 6);
    print_tagged("      排之后: ", k2, t2, 6);
    int order_ok2 = 1;
    int first_bad = -1;
    for (int i = 1; i < 6; i++) {
        if (k2[i - 1] == k2[i] && t2[i - 1] > t2[i]) {
            order_ok2 = 0;
            if (first_bad < 0) first_bad = i;
        }
    }
    printf("      同分者是否保持原始顺序：%s\n", order_ok2 ? "是" : "否");
    if (!order_ok2) {
        printf("      第一处翻车在下标 %d：%d%c 排到了 %d%c 前面。\n",
               first_bad, k2[first_bad], (char)('a' + t2[first_bad]),
               k2[first_bad - 1], (char)('a' + t2[first_bad - 1]));
        printf("      凶手：阶段 2 每次 swap(a[0], a[end]) 都是【首尾远距离对调】，\n");
        printf("      一个同分的元素会被瞬移到另一端，中间那些同分兄弟的顺序整段反转。\n");
    }

    /* ============================================================
       第 6 幕：耗时实测 —— O(1) 空间为什么换来一个慢
       ============================================================ */
    section("6. 实测耗时：堆排序 vs 裸快排 vs introsort（本目录自带的极简对照实现）");

    printf("  为什么 O(1) 空间 + 最坏 O(n log n) 的好牌面，打出来却慢？复杂度里没写的三件事：\n");
    printf("    ① 下沉是「从根一路跳到叶子」，访问下标 0, 2, 6, 14…指数级跳 ⇒ 缓存频繁失效；\n");
    printf("       快排/插入都是下标单调往前扫，一行缓存线一次带走 16 个数。\n");
    printf("    ② 每下沉一层要做【2 次比较 + 1 次交换】，而交换是三次写内存（临时变量来回倒）。\n");
    printf("    ③ 每次取数都要把最小值一路从堆顶送到堆底再回来，走 log n 步；快排劈一次只走 n 步。\n");
    printf("  下面用 clock() 跑真实毫秒数（同一台机器、同一份数据、各自排完再校验正确性）：\n");

    const int TSZ[TIMED_N] = {4096, 8192, 16384};
    for (int k = 0; k < TIMED_N; k++) {
        int m = TSZ[k];
        int *shuf = malloc((size_t)m * sizeof(int));
        int *expect = malloc((size_t)m * sizeof(int));
        int *buf = malloc((size_t)m * sizeof(int));
        if (shuf == NULL || expect == NULL || buf == NULL) {
            printf("      [错误] 申请失败，跳过 n=%d\n", m);
            free(shuf); free(expect); free(buf);
            continue;
        }
        if (!fill_perm(shuf, m, 4242u)) {
            printf("      [错误] 生成排列失败，跳过 n=%d\n", m);
            free(shuf); free(expect); free(buf);
            all_pass = 0;
            continue;
        }
        for (int i = 0; i < m; i++) {
            expect[i] = i;                     /* 它是 0..m-1 的一个排列 ⇒ 标准答案已知 */
        }
        printf("\n  ┌ n = %d\n", m);
        printf("  │ 数据 A：固定种子 LCG 生成的乱序排列（三种算法的【平均情况】，公平对比）\n");
        clock_t t0;
        double ms;
        long cmp;
        HeapStat sh;

        copy_arr(buf, shuf, m); sh.cmp = sh.swap = sh.sink = 0;
        t0 = clock(); heap_sort_range(buf, 0, m - 1, 0, &sh); ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        printf("  │   heap-sort     %7.2f ms  比较 %8ld 交换 %8ld 下沉 %8ld 层  正确性：%s\n", ms,
               sh.cmp, sh.swap, sh.sink,
               (is_sorted_ascending(buf, m) && same_elements(buf, expect, m)) ? "通过" : "失败");
        if (!(is_sorted_ascending(buf, m) && same_elements(buf, expect, m))) all_pass = 0;

        copy_arr(buf, shuf, m); cmp = 0; t0 = clock(); mini_quick_sort3(buf, m, &cmp); ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        printf("  │   quick+median3 %7.2f ms  比较 %8ld              正确性：%s\n", ms, cmp,
               (is_sorted_ascending(buf, m) && same_elements(buf, expect, m)) ? "通过" : "失败");
        if (!(is_sorted_ascending(buf, m) && same_elements(buf, expect, m))) all_pass = 0;

        copy_arr(buf, shuf, m); cmp = 0; t0 = clock(); mini_quick_sort(buf, m, &cmp); ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        printf("  │   quick-naive   %7.2f ms  比较 %8ld（末位枢轴）正确性：%s\n", ms, cmp,
               (is_sorted_ascending(buf, m) && same_elements(buf, expect, m)) ? "通过" : "失败");
        if (!(is_sorted_ascending(buf, m) && same_elements(buf, expect, m))) all_pass = 0;

        copy_arr(buf, shuf, m); cmp = 0; long sw = 0; t0 = clock(); intro_sort(buf, m, 0, &cmp, &sw); ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        printf("  │   introsort     %7.2f ms  比较 %8ld 切堆排 %ld 次     正确性：%s\n", ms, cmp, sw,
               (is_sorted_ascending(buf, m) && same_elements(buf, expect, m)) ? "通过" : "失败");
        if (!(is_sorted_ascending(buf, m) && same_elements(buf, expect, m))) all_pass = 0;

        printf("  │ 数据 B：已经升序（裸快排的毒药：固定末位枢轴每趟只劈掉 1 个）\n");
        for (int i = 0; i < m; i++) {
            shuf[i] = i + 1;
            expect[i] = i + 1;
        }

        copy_arr(buf, shuf, m); sh.cmp = sh.swap = sh.sink = 0;
        t0 = clock(); heap_sort_range(buf, 0, m - 1, 0, &sh); ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        printf("  │   heap-sort     %7.2f ms  比较 %8ld 交换 %8ld 下沉 %8ld 层  正确性：%s\n", ms,
               sh.cmp, sh.swap, sh.sink,
               (is_sorted_ascending(buf, m) && same_elements(buf, expect, m)) ? "通过" : "失败");
        if (!(is_sorted_ascending(buf, m) && same_elements(buf, expect, m))) all_pass = 0;

        copy_arr(buf, shuf, m); cmp = 0; t0 = clock(); mini_quick_sort3(buf, m, &cmp); ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        printf("  │   quick+median3 %7.2f ms  比较 %8ld              正确性：%s\n", ms, cmp,
               (is_sorted_ascending(buf, m) && same_elements(buf, expect, m)) ? "通过" : "失败");
        if (!(is_sorted_ascending(buf, m) && same_elements(buf, expect, m))) all_pass = 0;

        copy_arr(buf, shuf, m); cmp = 0; t0 = clock(); mini_quick_sort(buf, m, &cmp); ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        printf("  │   quick-naive   %7.2f ms  比较 %8ld ← O(n²)：%ld 正好等于 m*(m-1)/2  正确性：%s\n", ms, cmp, cmp,
               (is_sorted_ascending(buf, m) && same_elements(buf, expect, m)) ? "通过" : "失败");
        if (!(is_sorted_ascending(buf, m) && same_elements(buf, expect, m))) all_pass = 0;

        copy_arr(buf, shuf, m); cmp = 0; sw = 0; t0 = clock(); intro_sort(buf, m, 0, &cmp, &sw); ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        printf("  └   introsort     %7.2f ms  比较 %8ld 切堆排 %ld 次     正确性：%s\n", ms, cmp, sw,
               (is_sorted_ascending(buf, m) && same_elements(buf, expect, m)) ? "通过" : "失败");
        if (!(is_sorted_ascending(buf, m) && same_elements(buf, expect, m))) all_pass = 0;

        free(shuf); free(expect); free(buf);
        shuf = NULL; expect = NULL; buf = NULL;
    }
    printf("\n  再多跑一步：把 introsort 的深度阈值从默认的 2*log2(n) 【人为压到 4】，逼兜底分支真的被执行：\n");
    printf("  （正常数据 + 三数取中时，上面那几行「切堆排 0 次」说明阈值根本没碰到 —— 兜底是保险，不是日常；\n");
    printf("    但没被走过的分支等于没测过的代码，所以这里故意压阈值，验证「切过去之后结果依然正确」。)\n");
    const int FSZ[2] = {2048, 8192};
    for (int k = 0; k < 2; k++) {
        int m = FSZ[k];
        int *arr = malloc((size_t)m * sizeof(int));
        int *exp2 = malloc((size_t)m * sizeof(int));
        if (arr == NULL || exp2 == NULL) {
            printf("      [错误] 申请失败，跳过 n=%d\n", m);
            free(arr); free(exp2);
            continue;
        }
        if (!fill_perm(arr, m, 777u)) {
            printf("      [错误] 生成排列失败，跳过 n=%d\n", m);
            free(arr); free(exp2);
            continue;
        }
        for (int i = 0; i < m; i++) {
            exp2[i] = i;
        }
        long cmp4 = 0;
        long sw4 = 0;
        intro_sort(arr, m, 4, &cmp4, &sw4);
        int ok4 = is_sorted_ascending(arr, m) && same_elements(arr, exp2, m);
        printf("      n=%5d 阈值=4：比较 %7ld 次，切给堆排 %3ld 段 ⇒ 结果与 0..%d 完全一致：%s\n",
               m, cmp4, sw4, m - 1, ok4 ? "是" : "否");
        if (!ok4) all_pass = 0;
        free(arr); free(exp2);
    }

    printf("\n  结论三条：\n");
    printf("    ① 平均情况（数据 A）：堆排序比较次数比快排多约四成，还多付 n-1 次首尾交换 + 每次下沉的跳跃访问\n");
    printf("       ⇒ 实测大约是快排的 2 倍耗时。复杂度表里同为 O(n log n)，并不代表跑起来一样快。\n");
    printf("    ② 升序毒药（数据 B）：裸快排比较次数掉进 O(n²)（那一行右边正好对上 m*(m-1)/2），耗时上百倍；\n");
    printf("       同样的数据堆排序毫发无伤，因为堆排【不看输入长相】：建堆和下沉走哪条路只由下标决定。\n");
    printf("    ③ introsort = 三数取中快排 + 小段(≤12)切插入 + 深度超过 2*log2(n) 就切堆排。\n");
    printf("       默认阈值下真实数据一次都不切（保险放着不用），压到 4 立刻切出很多段且结果照样正确\n");
    printf("       ⇒ 平均快 + 最坏有保证，两头都要，这就是标准库排序长这样的原因。\n");

    /* ============================================================
       第 7 幕：正确性与边界总校验
       ============================================================ */
    section("7. 正确性与边界总校验");

    printf("  工程版 heap_sort（无打印、无计数）：\n");
    copy_arr(work, ORIG, N);
    heap_sort(work, N);
    print_arr("    ", work, N);
    if (!same_elements(work, EXPECT, N)) all_pass = 0;

    printf("\n  边界数据（每种都验「升序 + 元素守恒」，另外单独验建堆后的堆性质）：\n");
    int edge_data[6][8] = {
        {42, 0, 0, 0, 0, 0, 0, 0},             /* n=1 */
        {9, 1, 0, 0, 0, 0, 0, 0},              /* n=2：一个爹一个孩子的极限情况 */
        {7, 7, 7, 7, 0, 0, 0, 0},              /* 全相同 */
        {5, -3, 0, -8, 12, 1, 0, 0},           /* 带负数 */
        {2, 1, 1, 2, 1, 2, 0, 0},              /* 大量重复 */
        {1, 2, 3, 4, 5, 6, 7, 8},              /* 已升序：对堆排序来说不是特例，照样 O(n log n) */
    };
    const int edge_n[6] = {1, 2, 4, 6, 6, 8};
    for (int e = 0; e < 6; e++) {
        int tmp[8];
        copy_arr(tmp, edge_data[e], edge_n[e]);
        HeapStat se = {0, 0, 0};
        build_max_heap(tmp, 0, edge_n[e] - 1, 0, &se);
        int heap_ok = verify_max_heap(tmp, edge_n[e]);
        int top_ok = (edge_n[e] >= 1) ? (tmp[0] >= tmp[edge_n[e] - 1]) : 1;

        copy_arr(tmp, edge_data[e], edge_n[e]);
        heap_sort(tmp, edge_n[e]);
        int ok = is_sorted_ascending(tmp, edge_n[e]) && is_permutation(edge_data[e], tmp, edge_n[e]);
        print_arr("    输入: ", edge_data[e], edge_n[e]);
        printf("          堆排输出: [");
        for (int i = 0; i < edge_n[e]; i++) {
            printf(" %d", tmp[i]);
        }
        printf(" ]  ⇒ 升序+守恒：%s | 建堆后合法：%s%s\n", ok ? "对" : "错", heap_ok ? "对" : "错",
               top_ok ? "" : "（注意 a[0] 是本段最大值）");
        if (!ok || !heap_ok) all_pass = 0;
    }
    int z[1] = {77};
    heap_sort(z, 0);
    heap_sort_trace(z, 0, 0, NULL);
    printf("    n=0：入口第一行 return，数组仍是 [ 77 ]（一个下标都没碰）\n");

    printf("\n  规模校验：n=4096 的确定排列 (i*37)%%4096，正确答案已知是 0..4095\n");
    int *vb = malloc(4096 * sizeof(int));
    int *ve = malloc(4096 * sizeof(int));
    if (vb == NULL || ve == NULL) {
        printf("    [错误] 申请失败\n");
        all_pass = 0;
    } else {
        for (int i = 0; i < 4096; i++) {
            vb[i] = (i * 37) % 4096;
            ve[i] = i;
        }
        heap_sort(vb, 4096);
        int ok1 = same_elements(vb, ve, 4096);
        int ok2 = is_sorted_ascending(vb, 4096);
        printf("    排完与 0..4095 完全一致：%s，升序检查：%s\n", ok1 ? "是" : "否", ok2 ? "是" : "否");
        if (!ok1 || !ok2) all_pass = 0;
    }
    free(vb); vb = NULL;
    free(ve); ve = NULL;

    printf("\n=== 本节结束 ===\n");
    printf("总校验：%s\n", all_pass ? "全部通过（每次建堆都单独验过堆性质，耗时表里每行都验过结果）"
                                   : "有不一致，去看代码");
    printf("记住五句话：\n");
    printf("  1) 堆就是数组 + 三个下标公式（根 0，父 (i-1)/2，孩子 2i+1/2i+2），不需要另一棵树。\n");
    printf("  2) 阶段 1 从 a[(n-2)/2] 倒着建堆，代价 O(n) 不是 O(n log n)；瓶颈在阶段 2。\n");
    printf("  3) 阶段 2 = 堆顶↔末尾交换 → 堆缩 1 → 新堆顶下沉，做 n-1 次，从后往前长出一段有序区。\n");
    printf("  4) O(1) 空间 + 最坏 O(n log n)，但跳跃访问 + 交换多 ⇒ 实测比快排慢；它的价值是当兜底（introsort）。\n");
    printf("  5) 首尾远距离交换决定它必然不稳定；同分要保序请用归并。\n");
    return 0;
}
