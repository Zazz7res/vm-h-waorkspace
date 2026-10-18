#include <stdio.h>
#include <stdlib.h>
#include "heap.h"

/* 小工具：打印一行分隔线，让输出好读（沿用 00-c-foundation 的排版习惯） */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/* 打印一个 int 数组（带下标），用来在调用堆之前先看原数据长什么样 */
static void print_plain_array(const char *name, const int *arr, int n)
{
    printf("  %s (n=%d): [", name, n);
    for (int k = 0; k < n; k++) {
        printf(" %d", arr[k]);
    }
    printf(" ]\n");
}

/* 验证"数组是不是升序"，返回第一个破坏升序的下标；-1 表示整体升序 */
static int first_descent(const int *arr, int n)
{
    for (int k = 1; k < n; k++) {
        if (arr[k] < arr[k - 1]) {
            return k;
        }
    }
    return -1;
}

/*
 * 实测两种建堆方式各做多少次比较。
 *
 * 输入故意用【降序】arr[0] > arr[1] > ... （比如 1000, 999, ..., 1）。为什么？
 *   降序是"逐个 push"的最坏情况：每来一个新元素都比场上所有人都小，
 *   必须从最深的地方一路浮到堆顶，上浮距离 = 该层的深度 = log n，一点跑不掉。
 *   而 heapify 处理同一批数据的代价只和"节点的深度"有关，仍然是线性的。
 *   这样两者的差距才能被干净地量出来。
 *
 * ⚠ 一个很容易骗到自己的细节：如果输入是【随机】数据，逐个 push 的的平均上浮
 *   距离其实接近常数（新元素往往比父亲大一点点就停了），实测会看不出差距。
 *   渐进复杂度讲的是最坏/平均的增长【趋势】，所以要靠"把 n 翻倍看倍数"来说话，
 *   而不是看单点的绝对数值。所以下面同时给了随机数据和降序数据两组结果。
 */
static void measure_build(int n, int *arr, int descending, long *build_cmp_out, long *push_cmp_out)
{
    for (int k = 0; k < n; k++) {
        /* 降序：最坏情况；确定性打乱：接近随机情况。都不是 rand()，每次跑一模一样 */
        arr[k] = descending ? (n - k) : ((k * 37) % n);
    }

    Heap *hb = heap_new(n, HEAP_MIN);
    heap_set_verbose(hb, 0);
    heap_reset_stats(hb);
    heap_build(hb, arr, n);                     /* 方式一：整堆 heapify */
    *build_cmp_out = heap_cmp_count(hb);
    heap_free(hb);

    Heap *hp = heap_new(n, HEAP_MIN);
    heap_set_verbose(hp, 0);
    heap_reset_stats(hp);
    for (int k = 0; k < n; k++) {
        heap_push(hp, arr[k]);                  /* 方式二：从【空堆】开始一个个 push */
    }
    *push_cmp_out = heap_cmp_count(hp);
    heap_free(hp);
    /* ⚠ 这里必须是空堆重测。之前我在已经装满的堆上继续 push，测出来两者几乎一样快，
     *    那是假的 —— 旧元素已经替你挡住了上浮路径。这种"实验设置错"比代码 bug 更难发现，
     *    所以每次做性能实测，先把"两组实验是不是从零开始的"核对一遍。 */
}

/* 以 2 为底的对数（只要个大概，用来算 n log n 这条参考线） */
static double log2i(int n)
{
    double v = (double)n;
    double r = 0.0;
    while (v > 1.0) {
        v /= 2.0;
        r += 1.0;
    }
    return r;
}

int main(void)
{
    /* ============================================================
       第 1 幕：堆到底是什么 —— 上面是树，下面是同一个数组
       ============================================================ */
    section("1. 堆 = 一颗被压扁成数组的完全二叉树");

    /* {1,3,2,7,9,5,6} 本身已经是一个合法最小堆：
       1<=3, 1<=2, 3<=7, 3<=9, 2<=5, 2<=6 全部成立。
       所以 build 之后数组一格都不会动 —— 这正好演示"堆序性成立"是什么样子。 */
    int demo1[] = {1, 3, 2, 7, 9, 5, 6};
    print_plain_array("原数组", demo1, 7);

    Heap *h1 = heap_new(8, HEAP_MIN);
    heap_build(h1, demo1, 7);
    printf("\n  下面这两块【是同一批内存】，只是画成两种样子：\n\n");
    heap_print(h1);

    printf("\n  手工验一遍堆序性（父亲 <= 两个孩子）：\n");
    for (int i = 0; i < heap_size(h1); i++) {
        int l = heap_left(i);
        int r = heap_right(i);
        if (l < heap_size(h1)) {
            printf("    data[%d]=%d <= data[%d]=%d ? %s\n", i, h1->data[i],
                   l, h1->data[l], h1->data[i] <= h1->data[l] ? "成立" : "❌不成立");
        }
        if (r < heap_size(h1)) {
            printf("    data[%d]=%d <= data[%d]=%d ? %s\n", i, h1->data[i],
                   r, h1->data[r], h1->data[i] <= h1->data[r] ? "成立" : "❌不成立");
        }
    }
    printf("  → 每对父子都成立，所以这是一个合法最小堆。（注意：数组本身【不是】升序！）\n");
    heap_free(h1);

    /* ============================================================
       第 2 幕：三条下标公式，以及"为什么整数除法天然成立"
       ============================================================ */
    section("2. 父=(i-1)/2、左=2i+1、右=2i+2：为什么这么算就对");

    heap_print_relations(11);

    printf("\n  证明（把孩子的下标代回父亲公式，看能不能算回原来的父亲 p）：\n");
    printf("    左孩子 i = 2p+1 → (i-1)/2 = (2p+1-1)/2 = 2p/2 = p            ✅\n");
    printf("    右孩子 i = 2p+2 → (i-1)/2 = (2p+2-1)/2 = (2p+1)/2 = p.5 → 砍小数 = p ✅\n");
    printf("  关键：C 的整数除法是【向零截断】。分子 i-1 永远 >= 0（i 从 1 起），\n");
    printf("        正数的「向零截断」就等于「向下取整」，所以第二种情况才能安全落到 p。\n");
    printf("        （如果分子可能为负，比如 -3/2 在 C 里是 -1 而不是 -2，公式就崩了。\n");
    printf("          这就是为什么这个看似随便的 (i-1)/2 其实是精心设计过的。）\n");

    /* ============================================================
       第 3 幕：push（上浮），每一步数组状态都打出来
       ============================================================ */
    section("3. push：放到数组末尾，然后一路「上浮」");

    Heap *h = heap_new(4, HEAP_MIN);   /* 故意只给 4 的初始容量，好看见翻倍扩容 */
    printf("  依次 push 5, 3, 8, 1, 4, 9, 2（初始容量 4，会看到自动扩容）\n\n");

    int to_push[] = {5, 3, 8, 1, 4, 9, 2};
    for (int k = 0; k < 7; k++) {
        printf("  ---- push(%d) ----\n", to_push[k]);
        heap_push(h, to_push[k]);
    }

    printf("\n  最终堆的树形 vs 数组：\n");
    heap_print(h);

    /* ============================================================
       第 4 幕：pop（交换末尾 + 缩容 + 下沉）
       ============================================================ */
    section("4. pop 堆顶：交换末尾 → size-- → 下沉");

    printf("  【为什么必须和末尾交换？】\n");
    printf("    (a) 数组下标必须连续，中间不能有洞，否则 data[k] 是谁的孩子就说不清了；\n");
    printf("    (b) 删了堆顶，堆顶这格必须有值填。\n");
    printf("    同时满足 (a)(b) 的只有【末尾元素】：它一定是叶子，搬走它不会让任何节点失去父亲，\n");
    printf("    而且只需 1 次 O(1) 交换。反过来「删堆顶 + 后面整体前移」结构碰巧没坏，\n");
    printf("    但每次 pop 变成 O(n)，n 次 pop = O(n²)，堆就白学了。\n\n");

    printf("  当前堆顶（peek）=");
    int head_value = -1;
    heap_peek(h, &head_value);
    printf("%d，size=%d\n\n", head_value, heap_size(h));

    int popped[16];
    int popped_n = 0;
    for (int round = 0; round < 3; round++) {
        int got = 0;
        printf("  ---- pop 第 %d 次 ----\n", round + 1);
        if (heap_pop(h, &got) == 0) {
            popped[popped_n++] = got;
            printf("    >>> 拿到最小值 %d\n", got);
        }
    }
    printf("\n  连续 pop 出来的序列: ");
    for (int k = 0; k < popped_n; k++) {
        printf("%d ", popped[k]);
    }
    printf("  ← 严格升序，这就是堆的定义性特征\n");

    /* ============================================================
       第 5 幕：⚠ 堆【不是】排序 —— 初学者最大的误解
       ============================================================ */
    section("5. 大误区：堆只保证堆顶最小，整体【不】有序");

    Heap *h5 = heap_new(16, HEAP_MIN);
    int vals5[] = {5, 3, 8, 1, 4, 9, 2};
    for (int k = 0; k < 7; k++) {
        heap_set_verbose(h5, 0);
        heap_push(h5, vals5[k]);
    }
    heap_set_verbose(h5, 1);

    printf("  把 5 3 8 1 4 9 2 全部 push 进最小堆之后，数组长这样：\n");
    heap_dump(h5, "数组");
    printf("\n");
    heap_print(h5);

    int bad = first_descent(h5->data, h5->size);
    if (bad > 0) {
        printf("\n  ❗ 数组【不是】升序的：data[%d]=%d 竟然比 data[%d]=%d 还小。\n",
               bad, h5->data[bad], bad - 1, h5->data[bad - 1]);
        printf("   堆序性只说「父亲 <= 孩子」，它对【兄弟之间】、对【跨层】没有任何承诺。\n");
    }
    printf("  再看一个：data[1]=%d, data[2]=%d —— 它们是兄弟，谁大谁小堆根本不管。\n",
           h5->data[1], h5->data[2]);
    printf("  结论：\n");
    printf("    · 想知道「全局最小」→ 看 data[0]，O(1)，这是堆唯一的全局保证。\n");
    printf("    · 想知道「第 2 小」→ 只能把它 pop 掉再看新的堆顶，没有捷径。\n");
    printf("    · 想要整体有序   → 反复 pop（n 次 × O(log n) = O(n log n)），那就是堆排序，\n");
    printf("                       见 10-sorting/heap-sort。\n");
    printf("    · 顺便：堆也【不是】二叉搜索树，对它做中序遍历得到的不是升序，\n");
    printf("      （BST 要求左<根<右，堆只要求父<=子。这两个约束完全不是一回事。）\n");

    printf("\n  现在把剩下的全 pop 光，得到的就是完整升序：");
    heap_set_verbose(h5, 0);          /* 这一路只要结果，不用再打印交换细节 */
    int tail_value = 0;
    int prev = -1;
    int asc = 1;
    while (!heap_empty(h5)) {
        heap_peek(h5, &tail_value);
        if (prev > tail_value) {
            asc = 0;
        }
        prev = tail_value;
        printf("%d ", tail_value);
        int dummy = 0;
        heap_pop(h5, &dummy);
    }
    printf(" %s\n", asc ? "（✅ 升序）" : "（❌ 出错了）");
    heap_set_verbose(h5, 1);
    printf("  peek 空堆：返回 %d（-1 表示空，程序没崩）\n", heap_peek(h5, &tail_value));
    printf("  pop   空堆：返回 %d\n", heap_pop(h5, &tail_value));
    heap_free(h5);
    h5 = NULL;

    /* ============================================================
       第 6 幕：heap_build —— 把一堆乱序数据原地堆化
       ============================================================ */
    section("6. heapify：从 n/2-1 开始自底向上逐个下沉");

    int messy[] = {9, 5, 7, 1, 3, 8, 2, 4, 6};
    print_plain_array("乱序数组", messy, 9);
    Heap *h6 = heap_new(9, HEAP_MIN);
    heap_build(h6, messy, 9);
    printf("\n  建完之后：\n");
    heap_print(h6);
    printf("  原数组有没有被改掉？ messy[0] 现在还是 %d（没改，我们只改了堆里的拷贝）\n",
           messy[0]);
    heap_free(h6);

    /* ============================================================
       第 7 幕：O(n) 还是 O(n log n)？用比较次数实测
       ============================================================ */
    section("7. 复杂度实测：heapify 是 O(n)，逐个 push 是 O(n log n)");

    enum { MAXN = 8000 };
    static int big[MAXN];

    /* ---- 7.1 先看金字塔：每层多少人，每人最多挪几步 ---- */
    int n1 = 1000;
    int levels = 0;
    while ((1 << levels) <= n1) {
        levels++;
    }
    long step_bound = 0;
    printf("  【7.1】n=%d 时树有 %d 层。第 d 层最多 2^d 个节点，最多下沉 (层数-1-d) 步：\n",
           n1, levels);
    printf("    层 d | 本层节点数 | 最多下沉 | 该层步数上界\n");
    for (int d = 0; d < levels; d++) {
        int start = (1 << d) - 1;
        int end = (1 << (d + 1)) - 2;   /* 第 d 层占用的下标区间是 [2^d-1, 2^(d+1)-2] */
        if (end > n1 - 1) {
            end = n1 - 1;
        }
        int cnt = end - start + 1;
        int max_drop = levels - 1 - d;
        step_bound += (long)cnt * max_drop;
        if (d <= 1 || d >= levels - 3) {
            printf("    %5d | %10d | %8d | %11d\n", d, cnt, max_drop, cnt * max_drop);
        } else if (d == 2) {
            printf("    ...（中间层省略）\n");
        }
    }
    printf("    合计下沉步数上界 = %ld 步（每步最多 2 次比较）→ %.2f 倍 n，线性！\n",
           step_bound, (double)step_bound / n1);
    int last_level_cnt = n1 - ((1 << (levels - 1)) - 1);   /* 最后一层实际有多少人 */
    printf("    看清楚了吗：%d 个节点里，光最后一层就有 %d 人，他们下沉 0 步；\n",
           n1, last_level_cnt);
    printf("    倒数第二层最多 %d 人，每人最多挪 1 步。真正要挪很多步的只有顶上几层，\n",
           1 << (levels - 2));
    printf("    而那里总共才几个人。**人多的地方离地面近** —— 这就是 O(n) 的全部秘密。\n");

    /* ---- 7.2 实测：降序最坏输入，看 n 翻倍时两者的增长倍数 ---- */
    printf("\n  【7.2】实测（输入用【降序】，这是 push 的最坏情况）：\n");
    printf("        n  | heapify比较 | 增长倍数 | 逐个push比较 | 增长倍数 | n·log2n参考线\n");
    long prev_build = 0, prev_push = 0;
    for (int n = 1000; n <= MAXN; n *= 2) {
        long b = 0, p = 0;
        measure_build(n, big, 1, &b, &p);

        char ratio_b[16], ratio_p[16];
        if (prev_build > 0) {
            snprintf(ratio_b, sizeof ratio_b, "%6.2f x", (double)b / (double)prev_build);
            snprintf(ratio_p, sizeof ratio_p, "%6.2f x", (double)p / (double)prev_push);
        } else {
            snprintf(ratio_b, sizeof ratio_b, "     --");
            snprintf(ratio_p, sizeof ratio_p, "     --");
        }
        printf("  %6d | %11ld | %s | %12ld | %s | %9.0f\n",
               n, b, ratio_b, p, ratio_p, (double)n * log2i(n));

        prev_build = b;
        prev_push = p;
    }
    printf("    解读：n 翻倍时 heapify 的次数几乎正好翻倍（=一次函数），\n");
    printf("          而 push 翻的比 2 倍还多一点 —— 多出来的那一点就是 log n，\n");
    printf("          它就是 O(n log n) 和 O(n) 的分界线。n 越大这个差距越夸张。\n");

    /* ---- 7.3 反面教材：随机输入时 push 也不慢，别被数字骗了 ---- */
    printf("\n  【7.3】同一批规模，把输入换成【确定性打乱】（近似随机）：\n");
    for (int n = 1000; n <= 4000; n *= 2) {
        long b = 0, p = 0;
        measure_build(n, big, 0, &b, &p);
        printf("  n=%6d  heapify=%8ld  逐个 push=%8ld  （push 只慢 %.2f 倍）\n",
               n, b, p, (double)p / (double)b);
    }
    printf("    随机数据时新元素往往和父亲比一次就停了，平均上浮距离接近常数，\n");
    printf("    所以 7.2 和 7.3 的差距能看出：**渐进复杂度讲的是最坏趋势，\n");
    printf("    实测必须挑对输入，否则会得到【两者一样快】的错误结论。**\n");

    printf("\n  为什么差这么多（结论背下来）：\n");
    printf("    · heapify 从【最深的一层】往上处理：一半节点（n/2）是叶子，下沉 0 步；\n");
    printf("      四分之一节点最多 1 步……总功 = n x (0/2 + 1/4 + 2/8 + 3/16 + ...) = n x 1 = O(n)。\n");
    printf("      一句话：金字塔底层的砖头最多，但它们离地面最近，每人挪一点点。\n");
    printf("    · push 正好相反：新元素【永远被放在最深的地方】，降序输入时每次都要爬满\n");
    printf("      log n 层，n 次就是 O(n log n) —— 这个 log n 在最坏情况跑不掉。\n");
    printf("    · 所以标准库和教材都说：手里已经有一整批数据时用 heapify，\n");
    printf("      数据是源源不断流进来的才只能逐个 push。\n");

    /* ============================================================
       第 8 幕：最大堆 = 把比较号反过来；以及 std::priority_queue 的坑
       ============================================================ */
    section("8. 最小堆 / 最大堆：只差一个 kind 标记");

    int vals8[] = {5, 3, 8, 1, 4, 9, 2};
    Heap *hmin = heap_new(8, HEAP_MIN);
    Heap *hmax = heap_new(8, HEAP_MAX);
    heap_set_verbose(hmin, 0);
    heap_set_verbose(hmax, 0);
    for (int k = 0; k < 7; k++) {
        heap_push(hmin, vals8[k]);
        heap_push(hmax, vals8[k]);
    }
    /* push 的时候关掉了打印（两个堆一起跑，刷屏会交叉），现在打开来 dump */
    heap_set_verbose(hmin, 1);
    heap_set_verbose(hmax, 1);
    printf("  同一批数据 5 3 8 1 4 9 2，两个堆的数组长这样：\n");
    heap_dump(hmin, "最小堆数组");
    heap_dump(hmax, "最大堆数组");
    printf("\n  树形对照（同一批数字，只是比较号相反）：\n");
    printf("  —— 最小堆 ——\n");
    heap_print(hmin);
    printf("  —— 最大堆 ——\n");
    heap_print(hmax);

    int got_min = 0, got_max = 0;
    heap_peek(hmin, &got_min);
    heap_peek(hmax, &got_max);
    printf("\n  最小堆堆顶 = %d   最大堆堆顶 = %d   （代码完全相同，只差 kind 一个字段）\n",
           got_min, got_max);
    printf("\n  ⚠ C++ 的 std::priority_queue 默认是【最大堆】（less 比较 → 顶部最大）。\n");
    printf("    很多人写竞赛题想取最小值，忘了传 greater，结果 pop 出来全是最大值。\n");
    printf("    而 Python 的 heapq、Java 的 PriorityQueue 默认是【最小堆】。\n");
    printf("    没有道理可讲，纯历史决定，用之前先看文档，并且自己写一行验证。\n");

    heap_free(hmin);
    heap_free(hmax);
    heap_free(h);

    /* ============================================================
       收尾
       ============================================================ */
    printf("\n=== 本节结束 ===\n");
    printf("记住五句话：\n");
    printf("  1) 堆是【被压扁成数组的完全二叉树】，父子关系全靠下标算，数组里一个指针都没有。\n");
    printf("  2) 父=(i-1)/2 成立靠的是「下标非负 → C 的向零截断等于向下取整」，不是巧合。\n");
    printf("  3) pop 必须【和末尾交换再缩容】，因为只有末尾是叶子，搬走它形状不坏且只花 O(1)。\n");
    printf("  4) 堆只保证【堆顶】全局最小，整体【不】有序；想要有序就反复 pop（=堆排序）。\n");
    printf("  5) heapify 是 O(n)（人多的层浅），逐个 push 是 O(n log n)（新元素总在最深的地方）。\n");
    return 0;
}
