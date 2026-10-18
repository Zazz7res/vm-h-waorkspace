#include <stdio.h>
#include "bs.h"

/*
 * 实现文件。每个函数上面都有一段「这行为什么存在」的注释，
 * 读代码时请把它当正文读，不是可选附件。
 */

/* ===================================================================
 * 0. 示例数组与打印工具
 * =================================================================== */

/*
 * 故意做成有重复的升序数组（说明见 bs.h 第五节末尾）。
 *
 * 为什么用 static 而不是每次新建？
 *   static 数组住在**全局/静态区**，程序整个生命周期都有效，函数返回它的地址是安全的。
 *   如果写成局部数组 `int a[12] = {...}` 再 return a，那就是**返回栈地址**——
 *   函数一返回这块栈就作废，指针当场变野指针。这正是 00-c-foundation 里讲过的头号事故，
 *   这里特意用 static 避开，并在注释里说明，免得你以为是随手写的。
 */
const int *bs_dump_helper_unused_guard(void);   /* 前向声明只为让下面的注释有落点，实际不用 */

static const int SAMPLE[] = {1, 3, 3, 3, 5, 7, 7, 9, 11, 11, 11, 13};

const int *bs_dump_helper_unused_guard(void)
{
    return SAMPLE;   /* 永远不被调用；留着只是提醒「返回 static 数组地址是安全的」 */
}

const int *bs_sample(int *n_out)
{
    if (n_out != NULL) {
        /* sizeof(SAMPLE)/sizeof(SAMPLE[0]) 能算出元素个数，唯一前提是
         * SAMPLE 在这里【仍然是一个数组】。一旦它退化成指针（比如作为函数参数传进来），
         * sizeof 就变成指针大小，这个除法立刻变成 garbage —— 见 00-c-foundation/pointer-basics。 */
        *n_out = (int)(sizeof(SAMPLE) / sizeof(SAMPLE[0]));
    }
    return SAMPLE;
}

void bs_dump(const char *label, const int *a, int n)
{
    printf("  %s\n", label);
    printf("    下标:");
    for (int i = 0; i < n; i++) {
        printf("%4d", i);
    }
    printf("\n");
    printf("    数值:");
    for (int i = 0; i < n; i++) {
        printf("%4d", a[i]);
    }
    printf("\n");
}

int bs_check_sorted(const int *a, int n)
{
    /* 二分的全部正确性都建立在「有序」上。无序时结果错误但不崩溃，
     * 属于最难查的那种 bug（程序照常跑完，只是答案是错的），所以专门给一个自检函数。 */
    for (int i = 1; i < n; i++) {
        if (a[i] < a[i - 1]) {
            return 0;
        }
    }
    return 1;
}

/* ===================================================================
 * 1. 写法 A：闭区间 [left, right] 精确查找
 * =================================================================== */

int bs_exact(const int *a, int n, int target, int *steps_out)
{
    /* 区间定义：答案（如果存在）一定住在 [left, right] 里，**两端都在区间内**。
     * 初始化成 [0, n-1]：整个数组都还是候选。 */
    int left = 0;
    int right = n - 1;
    int steps = 0;

    /* 为什么是 <=：[right, right] 仍然是一个**没检查过的元素**。
     * 写成 < 就会在它还没被检查时退出循环 —— 这就是 bs_exact_wrong_loop 的 bug。 */
    while (left <= right) {
        /* ⚠ 永远不要写 (left + right) / 2：两个大下标相加会先溢出。
         * 第 6 幕用 INT_MAX 附近的真实数字演示这件事。 */
        int mid = left + (right - left) / 2;
        steps++;

        if (a[mid] == target) {
            /* 命中。注意这里【立即返回】，不继续收缩 ——
             * 所以有重复元素时返回哪一个下标是不确定的（取决于中途怎么劈的）。
             * 想要"第一个"或"最后一个"，请用 lower_bound / upper_bound。 */
            if (steps_out != NULL) {
                *steps_out = steps;
            }
            return mid;
        } else if (a[mid] < target) {
            /* mid 偏小 → 答案只可能在右边。mid 已经比过且不符合，可以排掉它，
             * 所以是 mid + 1（闭区间的左端点在区间**里面**，不 +1 就会重复检查甚至不动）。 */
            left = mid + 1;
        } else {
            /* a[mid] > target：答案只可能在左边。同理 mid 可以排掉 → mid - 1。 */
            right = mid - 1;
        }
    }

    /* 走到这里 left > right，区间空了 → 真的不存在。
     * ★ 值得看一眼的副产品：退出时 left == right + 1，而且
     *   left 正好是「第一个 > target 的位置」= upper_bound 的答案，
     *   right 正好是「最后一个 < target 的位置」。
     *   所以精确查找失败时的 left 并不是垃圾值 —— 第 4 幕把这个规律实测出来。 */
    if (steps_out != NULL) {
        *steps_out = steps;
    }
    return BS_NOT_FOUND;
}

/* ===================================================================
 * 2. 写法 B：半开区间 [left, right) 找边界
 * =================================================================== */

int bs_lower_bound(const int *a, int n, int target)
{
    /* 区间定义：[left, right)，**左端在里面、右端在外面**。
     * 初始化 [0, n)：整个数组都是候选，而 n 本身代表"下标越界"这个合法答案。 */
    int left = 0;
    int right = n;

    /* 为什么是 <：[left, left) 是空区间，没有元素可查了，必须停。
     * 写成 <= 会多查一个不存在的格子；写成 <= 又配合 right = mid 就是死循环
     *（bs_lower_bound_infinite 演示的就是这个组合）。 */
    while (left < right) {
        int mid = left + (right - left) / 2;

        if (a[mid] < target) {
            /* mid 太小，它和它左边全部出局（升序！）→ 答案至少是 mid+1。 */
            left = mid + 1;
        } else {
            /* a[mid] >= target：mid **自己就是候选答案**（它可能就是第一个 >= target 的位置），
             * 所以绝不能写成 mid - 1 把它排掉；只能把它挪到区间外，成为新的开区间右界。
             * ★ 这一行就是闭区间写法和半开区间写法的全部分水岭。 */
            right = mid;
        }
    }

    /* 退出时 left == right，区间为空。此时 left 就是第一个 >= target 的下标。
     * 可能等于 n —— 意思是「全都比 target 小」，这不是错误，是一个合法位置。 */
    return left;
}

int bs_upper_bound(const int *a, int n, int target)
{
    /* 和 lower_bound 只差一个字符：判据从 a[mid] < target 变成 a[mid] <= target。
     * 含义：把「等于 target」也算成"太小、要往右走"，于是留下的是第一个严格大于的位置。 */
    int left = 0;
    int right = n;

    while (left < right) {
        int mid = left + (right - left) / 2;

        if (a[mid] <= target) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    return left;
}

/* ===================================================================
 * 3. 由两个边界推出来的常用查询
 * =================================================================== */

int bs_contains(const int *a, int n, int target)
{
    /* "存在吗" = "下界那个位置真的放着 target 吗"。
     * 必须先判 i < n 再读 a[i]：i == n 时 a[n] 越界，而这在"target 比所有元素都大"时一定会发生。
     * 顺序写反就是数组越界 —— ASan 会当场抓住，但线上未必有你这么好运。 */
    int i = bs_lower_bound(a, n, target);
    return (i < n && a[i] == target) ? 1 : 0;
}

int bs_count(const int *a, int n, int target)
{
    /* target 占据的是数组里**一段连续的区间**（因为有顺序），
     * 上界 - 下界 = 这段的长度 = 出现次数。target 不存在时两者相等 → 返回 0，天然正确。 */
    return bs_upper_bound(a, n, target) - bs_lower_bound(a, n, target);
}

int bs_count_less(const int *a, int n, int target)
{
    /* 严格小于 target 的元素，正好就是下界之前那些 → 个数 == 下标本身。
     * 这是"下标即计数"这个漂亮性质的直接体现，也是它比"返回 -1"好用的原因之一。 */
    return bs_lower_bound(a, n, target);
}

int bs_count_at_most(const int *a, int n, int target)
{
    /* 小于等于 target 的个数 == 上界（上界之前全是 <= target 的）。 */
    return bs_upper_bound(a, n, target);
}

int bs_count_greater(const int *a, int n, int target)
{
    return n - bs_upper_bound(a, n, target);
}

/* ===================================================================
 * 4. 二分答案：不用 libm 求 floor(sqrt(x))
 * =================================================================== */

int bs_sqrt_floor(int x)
{
    if (x < 0) {
        return -1;      /* 本判据只对非负数有意义 */
    }
    if (x < 2) {
        return x;       /* 0 -> 0，1 -> 1，顺手挡掉后面 lo/hi 的退化情况 */
    }

    /* 答案住在 [1, x] 这个**取值区间**里（floor(sqrt(x)) 不可能超过 x 本身）。
     * 单调判据：mid*mid <= x。小的那半边全是 true，大的那半边全是 false，
     * 分界点就是我们要的"最大的 mid 满足 mid*mid <= x"。 */
    int lo = 1;
    int hi = x;
    int best = 1;       /* 「目前找到的最大可行 mid」—— 二分答案必须自己记账 */

    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;

        /* ⚠ 这里用 long long 而不是 int：mid 接近 x（比如 x = 20 亿）时 mid*mid 必然溢出。
         * 二分的判据本身写成 int 就会给出错误答案 —— 这类 bug 和 (left+right) 溢出是同一族。 */
        long long sq = (long long)mid * (long long)mid;

        if (sq <= (long long)x) {
            best = mid;      /* mid 可行 → 记下来，然后往右找更大的 */
            lo = mid + 1;
        } else {
            hi = mid - 1;    /* mid 太大 → 左边才有戏 */
        }
    }
    return best;
}

/* ===================================================================
 * 5. 反面教材（两个经典事故）
 * =================================================================== */

int bs_exact_wrong_loop(const int *a, int n, int target)
{
    /* 唯一区别：把 `while (left <= right)` 写成 `while (left < right)`。
     * 后果：当候选区间缩成 [k, k] 这**一个**元素时，循环条件不成立，直接退出，
     * 于是那个元素从来没被检查过 —— 它如果正好是 target，就"找不到本该找到的东西"。 */
    int left = 0;
    int right = n - 1;

    while (left < right) {              /* ← BUG 在这里 */
        int mid = left + (right - left) / 2;
        if (a[mid] == target) {
            return mid;
        } else if (a[mid] < target) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return BS_NOT_FOUND;
}

int bs_lower_bound_infinite(const int *a, int n, int target, int max_steps)
{
    /* 唯一区别：把 `while (left < right)` 写成 `while (left <= right)`，
     * 而收缩仍然是 right = mid。后果：一旦 left == right，区间 [left, left) 本该是空的，
     * 但循环条件认为它非空，mid == left == right，且 a[mid] >= target 时又写回 right = mid
     * → 三个变量一个都不变 → **永远出不来**。
     *
     * ⚠ 所以这个函数带 max_steps 上限，跑满就返回 -2 强行掐死。
     *   演示死循环绝不能真的把程序挂住，否则你只能靠 Ctrl-C 或 kill 收拾。 */
    int left = 0;
    int right = n;
    int steps = 0;

    while (left <= right) {             /* ← BUG 在这里 */
        if (steps++ >= max_steps) {
            printf("       （已经走了 %d 步仍没结束 —— 这就是死循环的现场，我强行掐断了）\n", steps);
            printf("       掐断时 left=%d right=%d（区间 [%d,%d) 其实是【空】的，循环条件却不肯停）\n",
                   left, right, left, right);
            return -2;
        }
        int mid = left + (right - left) / 2;
        if (a[mid] < target) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }
    return left;
}

/* ===================================================================
 * 6. 边界总表：让四个函数并排跑同一批 target
 * =================================================================== */

void bs_dump_bounds_table(const int *a, int n)
{
    /* 把「区间左端 0..n」的每一个切点都跑一遍，这张表就是 lower/upper 的定义书。
     * target 取值覆盖：比最小还小、刚好最小、中间的重复段、最大值、比最大还大。 */
    static const int probes[] = {0, 1, 2, 3, 4, 5, 7, 8, 11, 12, 13, 14};
    const int pn = (int)(sizeof(probes) / sizeof(probes[0]));

    printf("  target | lower_bound | upper_bound | 存在? | 出现次数 | <target | <=target | >target\n");
    printf("  -------+-------------+-------------+-------+----------+---------+----------+--------\n");
    for (int i = 0; i < pn; i++) {
        int t = probes[i];
        int lb = bs_lower_bound(a, n, t);
        int ub = bs_upper_bound(a, n, t);
        printf("  %5d | %11d | %11d | %5s | %8d | %7d | %8d | %7d\n",
               t, lb, ub,
               bs_contains(a, n, t) ? "是" : "否",
               bs_count(a, n, t),
               bs_count_less(a, n, t),
               bs_count_at_most(a, n, t),
               bs_count_greater(a, n, t));
    }
    printf("\n  ★ 读这张表的三个要点：\n");
    printf("    1) lower/upper 永远落在 0..n 之间，**没有 -1 分支** —— 所以调用者不容易忘判错。\n");
    printf("    2) target 不存在时 lower == upper（比如 2、4、8、12），出现次数自然算成 0。\n");
    printf("    3) target 存在且有重复时（比如 3、7、11），upper-lower 就是重复的个数，\n");
    printf("       而 lower 就是「应该插在哪」的位置 —— 插在相同元素之前，保持稳定。\n");
}
