#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "bs.h"

/*
 * ===================================================================
 * 二分查找 · 演示主程序
 * ===================================================================
 * 八幕。重点不在"会写二分"，而在**区间定义自洽**：
 *   第 1 幕：为什么二分值得学（实测比较次数）
 *   第 2 幕：写法 A 闭区间精确查找
 *   第 3 幕：写法 B 半开区间 lower/upper —— 本节的真正主角
 *   第 4 幕：把两个边界并排跑一遍，用一张表讲清"下标即计数"
 *   第 5 幕：二分的另一种用法——二分答案（不开 libm 求平方根）
 *   第 6 幕：mid 的溢出陷阱（用 INT_MAX 附近的真实数字）
 *   第 7 幕：两个经典事故（死循环 / 漏查），挂在 --show-bug 开关后面
 *   第 8 幕：收摊
 *
 * 用法：
 *   ./main            正常演示（默认路径，干净退出）
 *   ./main --show-bug 额外跑第 7 幕的错误写法
 */

/* 顺序查找：只为给二分一个对照物。返回比较次数（找到/没找到都算）。 */
static int linear_search(const int *a, int n, int target, int *cmp_out)
{
    int cmp = 0;
    for (int i = 0; i < n; i++) {
        cmp++;
        if (a[i] == target) {
            if (cmp_out != NULL) {
                *cmp_out = cmp;
            }
            return i;
        }
    }
    if (cmp_out != NULL) {
        *cmp_out = cmp;
    }
    return BS_NOT_FOUND;
}

static void banner(int act, const char *title)
{
    printf("\n════════════════════════════════════════════════════════════\n");
    printf(" 第 %d 幕 · %s\n", act, title);
    printf("════════════════════════════════════════════════════════════\n");
}

static void sub(const char *t)
{
    printf("\n  --- %s ---\n", t);
}

int main(int argc, char **argv)
{
    /* 是否演示错误写法。默认【不演示】—— 这样直接 ./main 的输出是干净的，
     * README 里粘的也是干净版本。想看事故请跑 ./main --show-bug。 */
    int show_bug = (argc > 1 && strcmp(argv[1], "--show-bug") == 0);

    int n = 0;
    const int *a = bs_sample(&n);

    /* ================================================================
     * 第 1 幕：为什么值得学二分
     * ================================================================ */
    banner(1, "前提与动机：有序数组上，二分每步砍掉一半");
    printf("  二分唯一的前提是【数组已升序排好】。先自检一下，别拿无序数组硬跑：\n");
    bs_dump("示例数组（故意有重复，才看得出 lower/upper 的差别）：", a, n);
    printf("  bs_check_sorted → %s\n", bs_check_sorted(a, n) ? "确实升序 ✓" : "不是升序 ✗");

    sub("拿一个更大的数组实测比较次数：顺序 vs 二分");
    /* 现搭一个 1024 个元素的升序数组：1024 = 2^10，二分的理论上限正好是 10 次，
     * 打印出来能直接和 log2(n) 对上，比用 12 个元素有说服力。 */
    int big[1024];
    for (int i = 0; i < 1024; i++) {
        big[i] = i * 3;          /* 0, 3, 6, ... 严格升序 */
    }
    static const int probes[] = {0, 1500, 3069, 3072, 3075};   /* 含两个不存在的值 */
    printf("  数组：big[i] = i*3，n = 1024（= 2^10，所以二分最多 10 次比较）\n\n");
    printf("  target   存在?   顺序查找比较次数   二分比较次数   省下\n");
    printf("  -------  ------   --------------   ------------   ----\n");
    for (int k = 0; k < 5; k++) {
        int t = probes[k];
        int lcmp = 0, bcmp = 0;
        int li = linear_search(big, 1024, t, &lcmp);
        /* 这里只关心"比较了几次"，下标本身用不上，所以把返回值丢给 (void)。 */
        (void)bs_exact(big, 1024, t, &bcmp);
        printf("  %6d   %-5s   %14d   %12d   %4d 倍\n",
               t,
               (li != BS_NOT_FOUND) ? "是" : "否",
               lcmp, bcmp,
               bcmp > 0 ? lcmp / bcmp : 0);
    }
    printf("\n  ★ 注意最后两行：查【不存在】的值时顺序查找必须走完全程 1024 次，\n");
    printf("    二分照样只花十来次。二分的优势在「找不到」的时候最明显。\n");
    printf("    （上面「省下 N 倍」是整数除法，只给个量级感觉，别当精确比例。）\n");

    /* ================================================================
     * 第 2 幕：写法 A —— 闭区间 [left, right] 精确查找
     * ================================================================ */
    banner(2, "写法 A · 闭区间精确查找：两端都在区间【里面】");
    printf("  初始化 left=0, right=n-1；循环条件 while (left <= right)；\n");
    printf("  收缩 left=mid+1 / right=mid-1（mid 已经比过，可以排掉）。\n");

    static const int p2[] = {1, 5, 13, 4, 0, 14};
    for (int k = 0; k < 6; k++) {
        int t = p2[k];
        int steps = 0;
        int idx = bs_exact(a, n, t, &steps);
        if (idx == BS_NOT_FOUND) {
            printf("  查 %2d → 不存在（返回 %d），比较 %d 次\n", t, BS_NOT_FOUND, steps);
        } else {
            printf("  查 %2d → 下标 %2d，值 %2d，比较 %d 次\n", t, idx, a[idx], steps);
        }
    }
    printf("\n  ⚠ 查 3 和 7 这种重复值时，返回的是【中途碰到的那一个】，不保证第一个也不是最后一个。\n");
    printf("    想要「最左/最右」，请往下看 lower_bound / upper_bound —— 那才是本节的重点。\n");

    /* ================================================================
     * 第 3 幕：写法 B —— 半开区间 [left, right) 找边界
     * ================================================================ */
    banner(3, "写法 B · 半开区间找边界：lower_bound / upper_bound（本节主角）");
    printf("  定义：\n");
    printf("    lower_bound(t) = 第一个【>= t】的下标（全都 < t 时返回 n）\n");
    printf("    upper_bound(t) = 第一个【>  t】的下标（全都 <= t 时返回 n）\n");
    printf("  初始化 left=0, right=n（注意不是 n-1！）；循环 while (left < right)；\n");
    printf("  收缩 left=mid+1 / right=mid —— 【right 不减 1】，因为 mid 自己可能就是答案。\n");

    sub("用重复段 3 3 3（下标 1..3）看两个边界的差别");
    printf("  下标:");
    for (int i = 0; i < n; i++) {
        printf("%4d", i);
    }
    printf("\n  数值:");
    for (int i = 0; i < n; i++) {
        printf("%4d", a[i]);
    }
    printf("\n");
    printf("  查 target=3：lower_bound = %d （第一个 >= 3 的位置，正好落在第一个 3 上）\n",
           bs_lower_bound(a, n, 3));
    printf("             upper_bound = %d （第一个 >  3 的位置，落在 3 那一段的后面）\n",
           bs_upper_bound(a, n, 3));
    printf("  ⇒ 3 出现在区间 [%d, %d) 里，共 %d 个。这个「上界减下界」就是计数。\n",
           bs_lower_bound(a, n, 3), bs_upper_bound(a, n, 3), bs_count(a, n, 3));

    sub("target 落在两段之间：查不存在的 4");
    printf("  lower_bound(4) = %d, upper_bound(4) = %d → 两者相等，说明 4 一个都没有，\n",
           bs_lower_bound(a, n, 4), bs_upper_bound(a, n, 4));
    printf("  而这个相等的位置 %d 正是【把 4 插进去还能保持有序、最靠左的插槽】。\n",
           bs_lower_bound(a, n, 4));
    printf("  ★ 这就是边界写法比「返回 -1」高明的地方：**找不到时它给的是一个有用的位置**，\n");
    printf("    而不是一个需要你特判的失败码。\n");

    /* ================================================================
     * 第 4 幕：一张总表把五个查询全跑完
     * ================================================================ */
    banner(4, "总表：lower/upper 一旦有了，其他全推得出来");
    bs_dump_bounds_table(a, n);

    sub("顺带验证上界/下界的恒等关系：upper(t) == lower(t+1)（整数数组、无空档时）");
    for (int i = 0; i < n && i < 6; i++) {
        int t = a[i];
        printf("    t=%2d: lower(t+1)=%2d  upper(t)=%2d  %s\n",
               t, bs_lower_bound(a, n, t + 1), bs_upper_bound(a, n, t),
               bs_lower_bound(a, n, t + 1) == bs_upper_bound(a, n, t) ? "相等 ✓" : "不等（t+1 不在这个值域里，正常）");
    }

    /* ================================================================
     * 第 5 幕：二分不只是找数 —— 二分答案
     * ================================================================ */
    banner(5, "二分答案：取值区间 + 单调判据，骨架和 lower_bound 一模一样");
    printf("  例子：不开数学库求 floor(sqrt(x))。对 x=40 手算：6*6=36<=40，7*7=49>40 → 答案是 6。\n");
    printf("  判据「mid*mid <= x」在 mid 上是【单调】的：先一片真、后一片假 → 可以二分那个分界。\n");

    static const int sx[] = {0, 1, 2, 4, 10, 15, 36, 40, 99, 100, 10000, 2147395600};
    printf("\n      x | floor(sqrt) | 验算 k*k <= x < (k+1)*(k+1)\n");
    printf("    ------+-------------+------------------------------\n");
    for (int k = 0; k < 12; k++) {
        int x = sx[k];
        int r = bs_sqrt_floor(x);
        /* 用 long long 做验算：(r+1)*(r+1) 在 x 接近 INT_MAX 时一定会溢出 int。
         * 这行本身就是"二分答案里判据的算术要防溢出"的活教材。 */
        long long here = (long long)r * (long long)r;
        long long nxt = (long long)(r + 1) * (long long)(r + 1);
        printf("  %10d | %11d | %lld <= %d < %lld  %s\n",
               x, r, here, x, nxt,
               (here <= (long long)x && (long long)x < nxt) ? "✓" : "✗");
    }
    printf("\n  ★ 把 bs_sqrt_floor 和 bs_lower_bound 的代码并排看，结构完全相同：\n");
    printf("    都是「区间 + 中点 + 一个单调判据 + 收缩」。区别只有判据从 a[mid] < target\n");
    printf("    换成了 mid*mid <= x。这就是「二分答案」这个套路的全部秘密。\n");
    printf("    第 11-searching/kth-smallest 那节的解法三用的正是它。\n");

    /* ================================================================
     * 第 6 幕：mid 的溢出陷阱
     * ================================================================ */
    banner(6, "陷阱：mid = (left + right) / 2 会溢出");
    printf("  数组很大时 left 和 right 都是很大的下标。两个大数【相加】先溢出，再除。\n");
    printf("  下面不建真数组（建 INT_MAX 个元素要 8GB），直接算这两个表达式：\n");

    int left = INT_MAX - 3;
    int right = INT_MAX;
    int bad_mid = (left + right) / 2;          /* 错误写法 */
    int good_mid = left + (right - left) / 2;  /* 正确写法 */
    printf("  left = %d, right = %d\n", left, right);
    printf("  (left + right)        = 溢出后的值 %d   ← 已经是垃圾\n", left + right);
    printf("  错误写法 (left+right)/2 = %d\n", bad_mid);
    printf("  正确写法 left+(right-left)/2 = %d   ← 落在区间里，这才是对的\n", good_mid);
    printf("  ★ 记住这一行：mid = left + (right - left) / 2。先减后加，永远不溢出。\n");
    printf("    （严格说 (left+right) 的有符号溢出在 C 标准里是 UB，编译器可能给你\n");
    printf("     任何结果，包括看起来正确的那个 —— 所以「偶尔能用」绝不是理由。）\n");

    /* ================================================================
     * 第 7 幕：两个经典事故
     * ================================================================ */
    banner(7, "两个经典事故：区间开闭、循环条件、±1 三者不匹配");
    if (!show_bug) {
        printf("  这一幕默认【不跑】，因为里面全是错误代码。\n");
        printf("  想看现场请跑：./main --show-bug  （或 make one DIR=... 后再单独跑带开关的版本）\n");
    } else {
        sub("事故一：闭区间精确查找把 <= 写成 < → 漏查最后一个候选");
        int t = 13;   /* 数组里最大的那个，最容易在收缩末期成为唯一候选 */
        int ok = bs_exact(a, n, t, NULL);
        int wrong = bs_exact_wrong_loop(a, n, t);
        printf("  查 target=%d：正确写法返回下标 %d；错误写法返回 %d\n", t, ok, wrong);
        printf("  → 元素明明在数组里（target=%d 就在下标 %d），错误版本却报「不存在」。\n", t, ok);
        printf("    原因：候选缩到 [11,11] 时循环条件 left<right 不成立，那个格子从来没被看过。\n");

        sub("事故二：半开区间把 < 写成 <= → 死循环（这里带步数上限，强行掐断）");
        int r = bs_lower_bound_infinite(a, n, 1, /*max_steps=*/20);
        printf("  查 target=1：正确写法 lower_bound 返回 %d；错误写法返回 %d\n",
               bs_lower_bound(a, n, 1), r);
        printf("  → 返回 -2 表示「它自己停不下来，是我掐死的」。\n");
        printf("    原因：left==right 时区间 [left,left) 是【空】的，但 <= 认为它非空，\n");
        printf("    而 right=mid 又不改变任何变量 → 三个值原地不动，永远循环。\n");
        printf("\n  ★ 这两个事故合起来就是那句结论：\n");
        printf("    循环条件和±1 不是自由发挥，是被【区间开闭】唯一确定下来的。\n");
        printf("    写二分前先写下「我用的区间是闭的还是半开的」，其余三行照抄推导。\n");
    }

    /* ================================================================
     * 第 8 幕：收摊
     * ================================================================ */
    banner(8, "收摊与本节要点");
    printf("  本节【没有任何 malloc】：示例数组是 static 的，函数只读不写，\n");
    printf("  所有缓冲区都在 main 的栈上。所以没有需要 free 的东西 —— ASan 干净是应有的，不是成就。\n");
    printf("\n  带走四句话：\n");
    printf("    1) 精确查找用闭区间：while (left <= right)，收缩 mid±1，找不到返回 -1。\n");
    printf("    2) 找边界用半开区间：right=n，while (left < right)，收缩 left=mid+1 / right=mid。\n");
    printf("    3) 有了 lower/upper，「存在吗、几次、几个小于、插哪儿」全是减法，不必再写第三种二分。\n");
    printf("    4) mid 永远写 left + (right - left) / 2。\n");
    return 0;
}
