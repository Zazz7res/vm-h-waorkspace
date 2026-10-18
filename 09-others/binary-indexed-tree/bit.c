#include <stdio.h>
#include <stdlib.h>
#include "bit.h"

/*
 * ============================================================================
 * 树状数组的实现。全部诀窍就藏在两个式子里：
 *     往上跳（修改）： i += i & (-i)
 *     往下跳（查询）： i -= i & (-i)
 * 下面把每个函数"为什么这么写"逐块讲清楚。
 * ============================================================================
 */

/* ---------- lowbit：整个结构的心脏 ---------- */

/*
 * lowbit(i) = i & (-i) = 把 i 写成二进制后，**只保留最低位的 1，其余全变 0**。
 *
 * 为什么"取反加一"能把最低位的 1 留下？看二进制（这里用 8 位演示）：
 *
 *     i       = 10  = 0000 1010
 *     ~i            = 1111 0101      （按位取反）
 *     ~i + 1 = -i   = 1111 0110      （这就是 -10 在补码里的样子）
 *     i & -i        = 0000 0010 = 2  ✔ 正好是最低位那个 1
 *
 * 关键在于：取反加一这个动作，会把**最低位 1 右边的所有 0 变成 1 再进位抵消**，
 * 最低位那个 1 本身保持不变，它左边的位全部翻转了 ⇒ 相与之后只剩它自己。
 *
 * 三个必须知道的取值：
 *   lowbit(0) = 0        ⇒ ★下标绝对不能用 0，否则 i += 0 原地踏步 = 死循环
 *   lowbit(奇数) = 1     ⇒ 奇数格子只管自己一个人
 *   lowbit(2^k) = 2^k    ⇒ 2、4、8、16 这些格子管得最宽，是"大区间块"
 */
int bit_lowbit(int i)
{
    return i & (-i);      /* 一行，没有分支，编译器就发一条 and + neg 指令 */
}

/* ---------- 生命周期 ---------- */

/*
 * 建树：把 src[0..n-1] 装进 tree[]。
 *
 * 内存布局（n = 8，src = {2,1,1,3,2,3,4,1}）：
 *
 *   b->a    ──→ [ 空 ][ 2 ][ 1 ][ 1 ][ 3 ][ 2 ][ 3 ][ 4 ][ 1 ]     下标 0 空着不用！
 *                 ↑0    ↑1    ↑2    ↑3    ↑4    ↑5    ↑6    ↑7    ↑8
 *   b->tree ──→ [ 空 ][ 2 ][ 3 ][ 1 ][ 7 ][ 2 ][ 5 ][ 4 ][ 17 ]    每个格子 = 它负责那几位的和
 *                    tree[4] = a1+a2+a3+a4 = 2+1+1+3 = 7
 *                    tree[6] = a5+a6       = 2+3     = 5
 *                    tree[8] = a1..a8      = 17
 *
 * 最直白的建法：从全 0 开始，把每个元素都 update 一遍 ⇒ O(n log n)。
 * 想 O(n) 建树有个技巧（这里不写，避免多一个容易看错的循环，知道有就行）：
 *   先令 tree[i] = a[i]，然后 for (i = 1..n) { j = i + lowbit(i); if (j <= n) tree[j] += tree[i]; }
 *   本质是"把自己这份和，交给下一个需要我的大格子"。
 */
int bit_init(BIT *b, const int *src, int n)
{
    if (b == NULL || n <= 0) {
        printf("      [错误] bit_init 参数不对：n=%d\n", n);
        return -1;
    }

    /* 申请 n+1 个：多出来的第 0 个格子是故意浪费的，用来躲开 lowbit(0)=0 的死循环 */
    b->tree = calloc((size_t)n + 1, sizeof(int));
    b->a    = calloc((size_t)n + 1, sizeof(int));
    if (b->tree == NULL || b->a == NULL) {
        free(b->tree);
        free(b->a);
        b->tree = NULL;
        b->a = NULL;
        printf("      [错误] 内存申请失败，想要 %d 个元素\n", n);
        return -1;
    }

    b->n = n;
    b->trace = 0;
    b->touches = 0;

    /* a[] 保持全 0（calloc 已经抹平了），然后把 src 的每个值当成"要加的量"交给 bit_update。
     * bit_update 里会顺手做 a[i] += delta，所以一轮下来 a[i] 正好等于 src[i-1]，
     * tree[] 也同步建好了，两边绝不会对不上。
     * ⚠ 常见错误：先 a[i] = src[i-1]，再 bit_update(b, i, a[i]) —— 那样 a[i] 会被加两次，
     *   变成双倍值，朴素对拍立刻露馅（我自己就是这么踩到的，写在这是为了让你也记住）。 */
    for (int i = 1; i <= n; i++) {
        if (src != NULL && src[i - 1] != 0) {
            bit_update(b, i, src[i - 1]);   /* 一句话建树：每个元素走一遍"往上跳" */
        }
    }
    b->touches = 0;      /* 建树的访问数不算进统计，不然第一次对比会吓人 */
    return 0;
}

void bit_free(BIT *b)
{
    if (b == NULL) {
        return;
    }
    free(b->tree);
    free(b->a);
    b->tree = NULL;      /* free 完置空，误用会当场崩，而不是悄悄读垃圾 */
    b->a = NULL;
    b->n = 0;
}

/* ---------- 核心操作 1：单点加（i += lowbit） ---------- */

/*
 * a[i] += delta。但 a 里只改一份不够 —— 有好几个 tree 格子里"含着"这个 a[i]，它们都得同步。
 *
 * 哪些格子含着 a[i]？就是 i 往上跳经过的那些：
 *
 *   改 a[3] 时（n = 8）：
 *     i = 3   lowbit(3) = 1   tree[3] 含 a[3] ⇒ 改它      下一个：3 + 1 = 4
 *     i = 4   lowbit(4) = 4   tree[4] 含 a[1..4] ⇒ 改它    下一个：4 + 4 = 8
 *     i = 8   lowbit(8) = 8   tree[8] 含 a[1..8] ⇒ 改它    下一个：8 + 8 = 16 > n ⇒ 停
 *   一共动了 3 个格子，而不是把 a[3] 后面的全部重算。
 *
 * 为什么这样跳一定对？因为 tree[j] 负责 a[j-lowbit(j)+1 .. j]，
 * "i 在不在 j 负责的那一段里"这件事，恰好等价于"从 i 一路 lowbit 往上跳会不会踩到 j"。
 *
 * 为什么不会死循环：i > 0 时 lowbit(i) ≥ 1 ⇒ i 严格变大，最多 O(log n) 步就冲出 n。
 *
 * 边界情况：
 *   - i 越界（<1 或 >n）：打印错误直接返回，绝不写越界内存。
 *   - delta = 0：白跑一趟，逻辑上无害。
 *   - 要"改成某个值"而不是"加"：先算差值，bit_update(i, new - b->a[i])。
 */
void bit_update(BIT *b, int i, int delta)
{
    if (b->tree == NULL) {
        printf("      [错误] bit_update：BIT 还没初始化\n");
        return;
    }
    if (i < 1 || i > b->n) {
        printf("      [错误] bit_update 下标越界：%d（合法 1..%d；0 是故意不用的）\n", i, b->n);
        return;
    }

    for (int p = i; p <= b->n; p += bit_lowbit(p)) {   /* p += lowbit(p)：一路往上通知 */
        b->tree[p] += delta;
        b->touches++;
        if (b->trace) {
            printf("        p=%-2d  tree[%2d] = %d（负责 a[%d..%d]）+= %+d ⇒ 现在 = %-3d    "
                   "下一个 p += lowbit(%d)=%d ⇒ p=%d\n",
                   p, p, b->tree[p] - delta, p - bit_lowbit(p) + 1, p, delta, b->tree[p],
                   p, bit_lowbit(p), p + bit_lowbit(p));
        }
    }

    b->a[i] += delta;    /* 自己那份原始值也要留着：打印、读当前值、朴素对拍全靠它 */
}

/*
 * 读"某个位置现在的值"。
 * 严格说树状数组本体没有这个操作（它只保证前缀和查得快），
 * 但我们本来就存了一份普通数组 a[] 用来对拍，所以顺手读它，O(1)。
 * 如果不想存副本，还有个漂亮的等价写法：bit_range_query(b, i, i) —— 单点区间和就是它自己的值。
 * 为什么要这个函数？因为题目常说"把 a[5] 改成 9"，而树状数组的接口是"加 delta"：
 *     delta = 9 - bit_get(b, 5);   bit_update(b, 5, delta);
 */
int bit_get(const BIT *b, int i)
{
    if (b->a == NULL || i < 1 || i > b->n) {
        printf("      [错误] bit_get 下标越界：%d\n", i);
        return 0;
    }
    return b->a[i];
}

/* 把"访问了几个格子"的累计计数清零。统计完再比，数字才有意义。 */
void bit_reset_touches(BIT *b)
{
    b->touches = 0;
}

/* ---------- 核心操作 2：前缀和查询（i -= lowbit） ---------- */

/*
 * 求 a[1] + ... + a[i]：从 i 开始，每次"吃掉一整块"，然后跳到这块的左边继续。
 *
 *   求前 7 个的和（n = 8）：
 *     i = 7   lowbit(7) = 1   加上 tree[7] = a[7]        剩下 a[1..6]，i = 7-1 = 6
 *     i = 6   lowbit(6) = 2   加上 tree[6] = a[5..6]     剩下 a[1..4]，i = 6-2 = 4
 *     i = 4   lowbit(4) = 4   加上 tree[4] = a[1..4]     剩下 a[1..0] = 空，i = 4-4 = 0
 *     i = 0   循环条件 i > 0 不成立 ⇒ 停。tree[0] 从来没被读，正好是"空地"。
 *   一共加 3 块：a[7] + a[5..6] + a[1..4]，不重不漏，等于前缀和。
 *
 * 为什么步数是 O(log n)？每跳一次就把 i 的**最低那个 1 抹掉**（i -= lowbit(i) 就是这个效果），
 * i 的二进制最多 log n 个 1 ⇒ 最多跳 log n 次。n = 8 时最多 3 次，n = 1e6 时最多 20 次。
 *
 * 边界情况：
 *   - query(0) 必须返回 0：这是"空区间"的正确定义，也是 range_query(l,r) 里 l=1 时
 *     要用到 query(0) 的原因（query(1-0-1) = query(0) = 0）。少这一条，区间和就全错。
 *   - i > n：按 n 截断（等价于"查全部"），并打印提示；不静默返回错值。
 *   - i < 0：非法，返回 0 并打印错误。
 */
int bit_query(BIT *b, int i)
{
    if (b->tree == NULL) {
        printf("      [错误] bit_query：BIT 还没初始化\n");
        return 0;
    }
    if (i < 0) {
        printf("      [错误] bit_query 下标为负：%d\n", i);
        return 0;
    }
    if (i > b->n) {
        printf("      [提示] bit_query(%d) 超过 n=%d，按查全部处理\n", i, b->n);
        i = b->n;
    }

    int sum = 0;
    for (int p = i; p > 0; p -= bit_lowbit(p)) {   /* p -= lowbit(p)：一路往下吃块 */
        sum += b->tree[p];
        b->touches++;
        if (b->trace) {
            printf("        p=%-2d  累加 tree[%2d]（负责 a[%d..%d] = %-3d）⇒ 累计 = %-3d   "
                   "下一个 p -= lowbit(%d)=%d ⇒ %d\n",
                   p, p, p - bit_lowbit(p) + 1, p, b->tree[p], sum,
                   p, bit_lowbit(p), p - bit_lowbit(p));
        }
    }
    return sum;
}

/*
 * 区间和 = 两个前缀和一减。这是树状数组"只会查前缀"这个限制的解药：
 *
 *     a[l..r] = (a[1..r]) - (a[1..l-1])
 *
 *               a[1] ... a[l-1] │ a[l] ... a[r]
 *             ←── query(r) 全长 ──────────────→
 *             ← query(l-1) →                     ⇒ 减掉就剩中间那段
 *
 * 边界：l == r 时就是单点值；l == 1 时 query(0) 必须是 0（上面已保证）。
 */
int bit_range_query(BIT *b, int l, int r)
{
    if (l < 1 || r > b->n || l > r) {
        printf("      [错误] bit_range_query 区间非法：[%d,%d]（合法 1..%d 且 l<=r）\n",
               l, r, b->n);
        return 0;
    }
    return bit_query(b, r) - bit_query(b, l - 1);
}

/* ---------- 对拍用的朴素版 ---------- */

/*
 * 完全不用任何技巧，老老实实把 a[l..r] 一个个加起来。
 * 它慢（O(n)），但**绝不会错**，所以拿来当"标准答案"验证上面的实现。
 * main.c 里那 24 步固定种子的对拍就是靠它把关的。
 */
int bit_naive_range_sum(const BIT *b, int l, int r, int *adds_out)
{
    int sum = 0;
    int adds = 0;
    for (int j = l; j <= r; j++) {
        sum += b->a[j];
        adds++;
    }
    if (adds_out != NULL) {
        *adds_out = adds;
    }
    return sum;
}

/* ---------- 打印 ---------- */

/*
 * 把 a[] 和 tree[] 并排打出来，并且逐个格子说明"你负责哪一段"。
 * 每次修改/查询之后打印，就能看着 tree[] 一格一格变。
 */
void bit_print_cells(const BIT *b, const char *tag)
{
    if (tag != NULL && tag[0] != '\0') {
        printf("    %s\n", tag);
    }
    printf("      index  :");   /* 全用 ASCII 定宽，避免中文在不同终端宽度下把表挤歪 */
    for (int i = 1; i <= b->n; i++) {
        printf("%3d", i);
    }
    printf("\n      a[i]   :");
    for (int i = 1; i <= b->n; i++) {
        printf("%3d", b->a[i]);
    }
    printf("\n      tree[i]:");
    for (int i = 1; i <= b->n; i++) {
        printf("%3d", b->tree[i]);
    }
    printf("\n      lowbit :");
    for (int i = 1; i <= b->n; i++) {
        printf("%3d", bit_lowbit(i));
    }
    printf("\n      每个格子负责的区间：\n");
    for (int i = 1; i <= b->n; i++) {
        int left = i - bit_lowbit(i) + 1;
        printf("        tree[%2d] = %-3d = a[%d]+…+a[%d]（%d 个，lowbit(%d)=%d）\n",
               i, b->tree[i], left, i, bit_lowbit(i), i, bit_lowbit(i));
    }
}

/*
 * 那张经典的条形覆盖图。它不需要真实的 tree 值 —— 每个格子盖多宽只由 lowbit 决定，
 * 所以只给一个 n 就能画。第一次看到这个图请横着对：
 *   · 每行的 ▓ 都**贴着自己的下标往左铺**，长度正好 lowbit(i)
 *   · 下标是 2 的幂的那几行（1、2、4、8）最长，它们是拼前缀和时的"大块"
 */
void bit_print_coverage_chart(int n)
{
    printf("      横轴 = 原始数组下标 1..%d；▓ = 这个 tree 格子覆盖到的位置\n", n);
    /* 注意这里表头故意用 ASCII 写（"index  i:" 正好 9 个字符），
     * 因为中文"下标"在某些终端占 2 格、某些占 1 格，混用会把竖排对齐搞乱。
     * 前缀都是 8 空格 + 9 字符 = 17 格，和下面的 "tree[ 1]:" 严格对齐。 */
    printf("        index  i:");
    for (int j = 1; j <= n; j++) {
        printf("%3d", j);
    }
    printf("\n");
    printf("%17s", "");                       /* 17 个空格，正好顶到第一个数据列 */
    for (int j = 1; j <= n; j++) {
        printf("---");                        /* 每列 3 格分隔线，和 %3d 列宽严格对齐 */
    }
    printf("\n");
    for (int i = 1; i <= n; i++) {
        int left = i - bit_lowbit(i) + 1;
        printf("        tree[%2d]:", i);
        for (int j = 1; j <= n; j++) {
            if (j >= left && j <= i) {
                printf(" ▓ ");      /* 一列固定 3 个显示格：空格+▓+空格，▓ 正好在数字正下方 */
            } else {
                printf(" · ");      /* 同样 3 格，用点表示"这一段不归这个格子管" */
            }
        }
        printf("   盖 a[%d..%d]，%d 个", left, i, bit_lowbit(i));
        if (i == n && bit_lowbit(i) == i) {
            printf("  ← 2 的幂，一个人管一整片");
        }
        printf("\n");
    }
    printf("        ↑ 每一行的 ▓ 都从自己的下标往左铺，铺的长度 = lowbit(下标)\n");
}

/* ---------- 应用：逆序对 ---------- */

/*
 * 逆序对：i > j 但 a[i] < a[j] 的一对下标。它是"数组有多不有序"的度量，
 * 也是归并排序课上的老朋友（归并时在合并阶段数它）。用树状数组数更直观。
 *
 * 【为什么可以把它翻译成"每个元素，右边有几个比它小的"】
 *   每一对逆序关系一定有唯一的"左边那个"和"右边那个"。
 *   所以对每个位置 j 数一遍"在它右边、且值比 a[j] 小的元素个数"，把这些数全加起来，
 *   正好把所有逆序对各数了一次，一个不多一个不少。
 *
 * 【树状数组怎么帮上忙】
 *   从**右往左**扫。扫到 j 时，BIT 里已经插好了 j 右边所有元素的信息。
 *   这里把 BIT 的下标当成"值"用（这叫频率树）：tree 里存的是"某个值出现过几次"。
 *     query(a[j] - 1) ⇒ 值比 a[j] 小的元素一共出现了几个 ⇒ 就是"右边比 a[j] 小的个数"
 *     update(a[j], 1) ⇒ 把 a[j] 这个值记一次，然后继续往左
 *
 * 【前提】值必须是 1..max_value 的小正整数（因为要拿值当下标）。
 *   值域很大时要先"离散化"（排序去重后换成名次），这属于另一个话题，README 里提了一句。
 *
 * 调用者负责先 bit_init(freq, NULL, max_value)，用完自己 bit_free。
 * freq->trace = 1 就能看到每一步的累计过程。
 */
long long bit_count_inversions(BIT *freq, const int *src, int n, int max_value)
{
    long long total = 0;

    /* freq->trace 在这里当"要不要讲步骤"的开关用。
     * 但要先把底层的跳格打印关掉：这里的下标是"值"不是"位置"，
     * 打着"负责 a[1..4]"的标签会让人以为在读原数组，越看越糊涂。
     * 关掉之后，每个元素只出一行大白话。 */
    int verbose = freq->trace;
    freq->trace = 0;

    for (int k = n; k >= 1; k--) {              /* ★从右往左 */
        int v = src[k - 1];                     /* src 是 0 起点 */
        if (v < 1 || v > max_value) {
            printf("      [错误] 值 %d 不在 1..%d 内，没法拿它当 BIT 下标\n", v, max_value);
            continue;
        }
        int smaller_on_right = bit_query(freq, v - 1);   /* 右边比我小的有几个 */
        total += smaller_on_right;
        bit_update(freq, v, 1);                          /* 把这个值登记上，继续往左 */

        if (verbose) {
            printf("        位置 %d（值 %d）：query(%d) = %d ⇒ 右边比 %d 小的有 %d 个"
                   "，逆序对累计 = %lld；再 update(%d,+1) 登记\n",
                   k, v, v - 1, smaller_on_right, v, smaller_on_right, total, v);
        }
    }

    freq->trace = verbose;
    return total;
}
