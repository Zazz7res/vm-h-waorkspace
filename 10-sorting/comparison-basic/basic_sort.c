#include <stdio.h>
#include "basic_sort.h"

/*
 * ===================================================================
 * 0. 内部小工具
 * ===================================================================
 * swap_int：交换两个 int。
 * 这里必须传【地址】（int *），因为函数要改的是调用者数组里的格子。
 * 传 int a, int b 就只能改自己的拷贝 —— 见 00-c-foundation/pointer-basics 那条铁律。
 */
static void swap_int(int *x, int *y)
{
    int temp = *x;   /* 先抄下来，否则 *x = *y 一执行，原来的值就丢了 */
    *x = *y;
    *y = temp;
}

/*
 * print_arr：给数组拍一张"全家福"。
 * const int a[] 是承诺：我只看不改。
 * 长度必须当参数传进来 —— 数组一传进函数就退化成指针，sizeof 只能看到 8。
 */
void print_arr(const char *label, const int a[], int n)
{
    printf("%s[", label);
    for (int i = 0; i < n; i++) {
        printf(" %d", a[i]);
    }
    printf(" ]\n");
}

/*
 * print_tail_bracket：冒泡专用。
 * 从下标 from 开始到结尾，这一段已经是"排好的后缀"，用方框圈出来：
 *     2 5 1 7 3 8 [ 4 9 ]   ← from = 6
 * from == n 表示还没有任何一段排好，就整行不加框。
 */
static void print_tail_bracket(const char *label, const int a[], int n, int from)
{
    printf("%s", label);
    for (int i = 0; i < n; i++) {
        if (i == from) {
            printf("[");        /* 进入"已有序后缀"，开始画框 */
        }
        printf(" %d", a[i]);
    }
    if (from < n) {
        printf(" ]");
    }
    printf("\n");
}

/*
 * print_head_bracket：插入排序专用。
 * 0 到 upto 这一段是"左手已经理好的牌"，画在前面：
     [ 1 2 5 9 ] 7 3 8 4  ← upto = 3
 */
static void print_head_bracket(const char *label, const int a[], int n, int upto)
{
    printf("%s", label);
    if (upto >= 0) {
        printf("[");
    }
    for (int i = 0; i < n; i++) {
        printf(" %d", a[i]);
        if (i == upto) {
            printf(" ]");       /* 左手牌区结束 */
        }
    }
    printf("\n");
}

/*
 * is_sorted_ascending：排序之后第一件事就是"验货"。
 * 只要发现某一对前 > 后，立刻判失败。
 * 注意比较的是 a[i-1] > a[i]（前一个大于后一个才算破绽），等号是允许的（不降序）。
 */
int is_sorted_ascending(const int a[], int n)
{
    for (int i = 1; i < n; i++) {
        if (a[i - 1] > a[i]) {
            return 0;
        }
    }
    return 1;
}

/*
 * same_elements：逐格比较，顺序也要一样。
 * 用来和"手算好的标准答案"对表：两个数组每一格都相等才算过。
 */
int same_elements(const int a[], const int b[], int n)
{
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

/*
 * is_permutation：只管"元素集合一样"，不管顺序。
 * 朴素 O(n²)：对 a 里每个值，数它在 a 里出现几次、在 b 里出现几次，次数必须相等。
 * 为什么不写得更聪明？因为不许用 qsort 的比较器，也不能用哈希表（本节自包含）。
 * n 很小的时候，这种笨办法最不容易出错，而它正是"排序没把数弄丢/没把数变多"的保证。
 */
int is_permutation(const int a[], const int b[], int n)
{
    if (n <= 0) {
        return 1;                      /* 空数组：两个空的东西当然是"一样的集合" */
    }
    for (int v = 0; v < n; v++) {
        int ca = 0, cb = 0;
        for (int i = 0; i < n; i++) {
            if (a[i] == a[v]) ca++;    /* a 里和 a[v] 同值的有几个 */
            if (b[i] == a[v]) cb++;    /* b 里同理 */
        }
        if (ca != cb) {
            return 0;                  /* 值被改写了，或者元素丢了 */
        }
    }
    return 1;
}

/*
 * ===================================================================
 * 1. 冒泡排序（工程版）
 * ===================================================================
 * 干什么：反复比较【相邻】两个数，大的往后放。
 * 为什么每趟内层上界要 -1：第 k 趟结束后，最后 k 格已经是最大的 k 个数，
 *                      再去比它们就是白干活。
 * 为什么要有 swapped 这个开关：一趟下来一次交换都没有 ⇒ 整段已经有序，
 *                              直接收工，这一条把最好情况从 O(n²) 拉回 O(n)。
 *
 * 一趟内部长这样（下标 0 开始，比较窗口一直往右滑）：
 *
 *     [ 5   2   9   1 ]        比 5>2 → 换 → [ 2 5 9 1 ]
 *        ▲   ▲
 *        │   └ 后一个
 *        └ 前一个
 *     [ 2   5   9   1 ]        比 5>9? 不换
 *            ▲   ▲
 *     [ 2   5   9   1 ]        比 9>1 → 换 → [ 2 5 1 9 ]  ← 9 冒到最右，本趟结束
 *                ▲   ▲
 */
void bubble_sort(int a[], int n)
{
    for (int round = 1; round <= n - 1; round++) {
        int swapped = 0;
        for (int j = 0; j < n - round; j++) {  /* 上界每趟 -1：尾巴那几格已经排好了 */
            if (a[j] > a[j + 1]) {
                swap_int(&a[j], &a[j + 1]);
                swapped = 1;
            }
        }
        if (!swapped) {
            break;                    /* 优化②：这趟没换 ⇒ 已经有序 */
        }
    }
}

/*
 * 教学版：和上面逻辑一字不差，只是多了三件事
 *   ① 每趟之后把整个数组打印出来（并且把"已有序后缀"画个框）
 *   ② 累加比较次数 / 交换次数
 *   ③ 记录"是不是提前收工的"，好让你亲眼看到最好情况 O(n)
 */
void bubble_sort_trace(int a[], int n, int verbose, SortStat *stat_out)
{
    SortStat st;
    st.cmp = 0; st.swap = 0; st.rounds = 0; st.early_exit = 0;

    for (int round = 1; round <= n - 1; round++) {
        int swapped = 0;
        long cmp_this = 0, swap_this = 0;

        for (int j = 0; j < n - round; j++) {
            st.cmp++; cmp_this++;                     /* 一次"比大小" */
            if (a[j] > a[j + 1]) {
                swap_int(&a[j], &a[j + 1]);
                st.swap++; swap_this++;
                swapped = 1;
            }
        }
        st.rounds++;

        if (verbose) {
            print_tail_bracket("      ", a, n, n - round);
            printf("        ↑ 本趟比较 %ld 次、交换 %ld 次；最后 %d 格已经是最大的 %d 个数，下一趟不再碰它们\n",
                   cmp_this, swap_this, round, round);
        }

        if (!swapped) {
            st.early_exit = 1;
            if (verbose) {
                printf("        ★ 这一趟一次交换都没发生 ⇒ 数组已经有序，提前收工！\n");
                printf("          这就是冒泡的第二个优化：最好情况下只跑一趟 = O(n)\n");
            }
            break;
        }
    }

    if (stat_out != NULL) {
        *stat_out = st;
    }
}

/*
 * ===================================================================
 * 2. 选择排序（工程版）
 * ===================================================================
 * 干什么：每趟在"还没排好的那段"里找最小值的【下标】，找到后和这段的第一个位置交换。
 * 为什么交换次数最少：一趟只交换 1 次，最多 n-1 次就结束；
 *                    冒泡/插入遇到逆序对就得挪一次，最坏能挪到 O(n²) 次。
 *                    所以"交换比比较贵得多"的场景（元素很大、写在磁盘上、每条记录几 KB）
 *                    选择排序是唯一划算的 O(n²) 方案。
 * 为什么它不稳定：见 tagged 版本 —— 它会把"半路杀出来的最小值"直接跟第一格换，
 *                于是原来排在前面的同值元素被甩到后面去了。
 *
 *     未排好段起点       本趟找到的最小值
 *            │                 │
 *     [ 5  2  9  1 ] ──── 找到 1（下标 3）─── 交换 0 和 3
 *     [ 1  2  9  5 ]  ← 一趟只动一次，[1] 从此归位
 */
void selection_sort(int a[], int n)
{
    for (int start = 0; start < n - 1; start++) {
        int min_idx = start;
        for (int j = start + 1; j < n; j++) {
            if (a[j] < a[min_idx]) {
                min_idx = j;             /* 只记下标，不交换 —— 这是"少交换"的关键 */
            }
        }
        if (min_idx != start) {          /* 已经是最小值就别自己跟自己换 */
            swap_int(&a[start], &a[min_idx]);
        }
    }
}

void selection_sort_trace(int a[], int n, int verbose, SortStat *stat_out)
{
    SortStat st;
    st.cmp = 0; st.swap = 0; st.rounds = 0; st.early_exit = 0;

    for (int start = 0; start < n - 1; start++) {
        int min_idx = start;
        for (int j = start + 1; j < n; j++) {
            st.cmp++;
            if (a[j] < a[min_idx]) {
                min_idx = j;
            }
        }
        if (min_idx != start) {
            swap_int(&a[start], &a[min_idx]);
            st.swap++;
        }
        st.rounds++;

        if (verbose) {
            print_head_bracket("      ", a, n, start);
            if (min_idx == start) {
                printf("        ↑ 第 %d 趟：比较了 %d 次，最小值 %d 本来就在段首 ⇒ 连这次交换都省了\n",
                       start + 1, n - start - 1, a[start]);
            } else {
                printf("        ↑ 第 %d 趟：在剩下的 %d 个里找到最小值 %d（它在下标 %d），全程只做了这 1 次交换\n",
                       start + 1, n - start - 1, a[start], min_idx);
            }
        }
    }

    if (stat_out != NULL) {
        *stat_out = st;
    }
}

/*
 * ===================================================================
 * 3. 插入排序（工程版）
 * ===================================================================
 * 干什么：像打扑克理牌。左手攥着已经理好的牌（下标 0..i-1），
 *        右手从桌上摸一张新牌 a[i]，从右手边那张开始一张一张往左比：
 *        比新牌大的就往右挪一格，直到碰到一张不比它大的，把新牌插进空位。
 * 为什么近乎有序时接近 O(n)：新牌往往往左比一两下就找到了位置（甚至一下都不比，
 *        因为它本来就比左手最大那张大），所以内层循环总代价 ≈ 逆序对个数。
 *        数据几乎有序 ⇒ 逆序对个数 ≈ 常数 × n ⇒ 总代价 O(n)。
 * 为什么它稳定：后移用的是"严格大于"（a[j] > key），
 *        跟新牌一样大的那张牌【不会被挪走】，新牌插在它后面 ⇒ 同值元素的先后顺序永远不变。
 * 为什么很多标准库里的小数组兜底就是它（V8 的 Array#sort、Python 的 Timsort、glibc 的部分路径）：
 *   ① 代码就 5 行，没有递归、没有额外内存，小 n 时开销为零；
 *   ② 对近乎有序的数据近乎线性，而真实世界的数据常常"大致有序"；
 *   ③ 缓存极其友好，访问的全是相邻内存。
 *
 *     左手已理好            新牌
 *          │                │
 *     [ 2  5  9 ]   1   7  3  8  4
 *                ← 9>1，9 右移
 *     [ 2  5  9  9 ]  7  3  8  4
 *             ← 5>1，5 右移
 *     [ 2  5  5  9 ]  7  3  8  4
 *        ← 2>1，2 右移
 *     [ 2  2  5  9 ]  7  3  8  4   → 挪到边界了，空位是下标 0，1 落进去
 *     [ 1  2  5  9 ]  7  3  8  4
 */
void insertion_sort(int a[], int n)
{
    for (int i = 1; i < n; i++) {
        int key = a[i];                  /* 先把新牌拿在手里，腾出 a[i] 这个空位 */
        int j = i - 1;
        while (j >= 0 && a[j] > key) {   /* 注意是 >，不是 >= —— 稳定性就靠这个等号 */
            a[j + 1] = a[j];             /* 比新牌大的，往后挪一格 */
            j--;
        }
        a[j + 1] = key;                  /* 落位：j+1 就是那个空出来的位置 */
    }
}

void insertion_sort_trace(int a[], int n, int verbose, SortStat *stat_out)
{
    SortStat st;
    st.cmp = 0; st.swap = 0; st.rounds = 0; st.early_exit = 0;

    for (int i = 1; i < n; i++) {
        int key = a[i];
        int j = i - 1;
        long moves_this = 0;

        while (j >= 0) {
            st.cmp++;
            if (a[j] > key) {
                a[j + 1] = a[j];
                st.swap++; moves_this++;
                j--;
            } else {
                break;                   /* 碰到不比新牌大的，停 */
            }
        }
        a[j + 1] = key;
        st.rounds++;

        if (verbose) {
            print_head_bracket("      ", a, n, i);
            if (moves_this == 0) {
                printf("        ↑ 摸到新牌 %d：左手最大那张都不比它大，一张都不用挪，原地落位\n", key);
            } else {
                printf("        ↑ 摸到新牌 %d：%ld 张比它大的牌被往后挪，它插在空出来的位置\n", key, moves_this);
            }
        }
    }

    if (stat_out != NULL) {
        *stat_out = st;
    }
}

/*
 * ===================================================================
 * 4. 稳定性实验：key 是分数，tag 是"第几个同学"
 * ===================================================================
 * key[] 和 tag[] 是两条平行数组，同一行的 (key[i], tag[i]) 属于同一个人。
 * 比较只看 key；一旦要挪动，key 和 tag 必须【一起】挪，否则人和分数就分家了。
 * 排完打印 tag 的顺序：同分的那几个编号如果还是原来的先后 ⇒ 稳定；反过来了 ⇒ 不稳定。
 *
 * 为什么不用 struct 而用两条数组？—— 更贴近本节"只有 int"的约定，也少一层语法噪音；
 * 三个函数里搬的都是"两个格子"，读起来跟搬一个 struct 是一样的。
 */
void print_tagged(const char *label, const int key[], const int tag[], int n)
{
    printf("%s[", label);
    for (int i = 0; i < n; i++) {
        /* tag 0,1,2... 打成 a,b,c... 于是一个 88 分的第三个同学就写成 88c */
        printf(" %d%c", key[i], (char)('a' + tag[i]));
    }
    printf(" ]\n");
}

/* 搬一个"人"：分数和身份证一起搬，这是本节的规矩 */
static void swap_person(int key[], int tag[], int i, int j)
{
    swap_int(&key[i], &key[j]);
    swap_int(&tag[i], &tag[j]);
}

/* 冒泡版：只在【严格大于】时交换 ⇒ 同分永远不换 ⇒ 稳定 */
void bubble_sort_tagged(int key[], int tag[], int n)
{
    for (int round = 1; round <= n - 1; round++) {
        int swapped = 0;
        for (int j = 0; j < n - round; j++) {
            if (key[j] > key[j + 1]) {     /* 同分（==）不进这个分支，就是稳定性的来源 */
                swap_person(key, tag, j, j + 1);
                swapped = 1;
            }
        }
        if (!swapped) {
            break;
        }
    }
}

/* 插入版：a[j] > key 才后移 ⇒ 同分的那张牌不挪，新牌插在它右边 ⇒ 稳定 */
void insertion_sort_tagged(int key[], int tag[], int n)
{
    for (int i = 1; i < n; i++) {
        int my_key = key[i];
        int my_tag = tag[i];
        int j = i - 1;
        while (j >= 0 && key[j] > my_key) {
            key[j + 1] = key[j];
            tag[j + 1] = tag[j];           /* 身份证跟着分数一起往后挪 */
            j--;
        }
        key[j + 1] = my_key;
        tag[j + 1] = my_tag;
    }
}

/*
 * 选择版：不稳定的活标本。
 * 问题出在"每趟只做一次远距离交换"：a[start] 被换到数组中间去了，
 * 而那个位置可能正好站着一个【和它同分、原本在它后面】的人 —— 顺序就这么被冲掉了。
 * 例子 [5a, 5b, 2]：第一趟把 2 和 5a 交换 ⇒ 变成 [2, 5b, 5a]，两个 5 分的人换了位置。
 */
void selection_sort_tagged(int key[], int tag[], int n)
{
    for (int start = 0; start < n - 1; start++) {
        int min_idx = start;
        for (int j = start + 1; j < n; j++) {
            if (key[j] < key[min_idx]) {
                min_idx = j;
            }
        }
        if (min_idx != start) {
            swap_person(key, tag, start, min_idx);   /* ← 罪魁祸首：跨越式的交换 */
        }
    }
}
