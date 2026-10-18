/*
 * ra.c —— 旋转数组查找的实现
 *
 * 阅读顺序建议：ra_min_index（心跳）→ ra_search（写法 A）→ ra_search_by_offset（写法 B）
 *              → ra_search_dup（退化）→ ra_demo_why_left_fails（为什么判据不能换）。
 *
 * 全节不 malloc：所有示例数组都在 main.c 的栈上，长度 ≤ RA_MAX_N。
 */

#include <stdio.h>   /* printf */
#include "ra.h"

/* ===================================================================
 * 0. 看清一个数组
 * =================================================================== */

/*
 * 打印数组，并顺手把两件事标出来：
 *   ① 下标行 —— 二分的每一步都在讨论下标，没有下标行图就白画了；
 *   ② 断崖行 —— 找出所有 a[i] > a[i+1] 的位置。合法旋转数组最多 1 个（再加首尾那个隐含的）。
 * 为什么要把断崖打出来？因为"找旋转点"这个问题**看图就会**，
 * 让读者先看图再看代码，比让他从 `while (left < right)` 里反推结构友好得多。
 */
void ra_dump(const char *label, const int a[], int n)
{
    if (a == NULL || n <= 0) {
        printf("%s（空数组或非法长度 n=%d）\n", label ? label : "数组", n);
        return;
    }

    if (label) {
        printf("%s\n", label);
    }

    printf("    下标 ");
    for (int i = 0; i < n; i++) {
        printf("%4d", i);
    }
    printf("\n    数值 ");
    for (int i = 0; i < n; i++) {
        printf("%4d", a[i]);
    }
    printf("\n");

    /* 断崖标记：a[i] > a[i+1] 就在这个 i 下面画个尖 */
    int drops = 0;
    printf("    断崖 ");
    for (int i = 0; i + 1 < n; i++) {
        if (a[i] > a[i + 1]) {
            drops++;
        }
    }
    if (drops == 0) {
        printf("（无断崖：这就是一份普通升序数组，旋转 0 次）\n");
    } else {
        for (int i = 0; i < n; i++) {
            int is_drop_here = (i + 1 < n && a[i] > a[i + 1]);
            if (is_drop_here) {
                printf("  ^=%d>%d", a[i], a[i + 1]);
            } else {
                printf("    ");
            }
        }
        printf("   下降点个数 = %d\n", drops);
    }

    /* 最小值下标：直接线性扫一遍。**这里故意不用二分**，
     * 因为 ra_dump 是"看图工具"，它要独立于被测算法 —— 用被测算法来显示被测算法的结果，
     * 万一算法错了图也会跟着错，那就失去对拍的意义了。 */
    int best = 0;
    for (int i = 1; i < n; i++) {
        if (a[i] < a[best]) {
            best = i;
        }
    }
    printf("    最小值 %d 在下标 %d（线性扫出来的，用来对拍二分的 ra_min_index）\n", a[best], best);
}

/* ===================================================================
 * 1. 找旋转点（最小值下标）—— 本节的心脏
 * =================================================================== */

/*
 * 模板：`while (left < right)` + `right = mid`（**不是 mid-1**）。
 * 为什么？因为这里的不变量是"最小值一定还在 [left, right] 里，且 right 是**候选**"。
 *   · 当 a[mid] > a[right]：mid 在大值段，它自己**不可能是**最小值 → left = mid + 1（敢跳过 mid）
 *   · 否则：mid **有可能就是**最小值 → right = mid（把 mid 留在窗口里）
 * 窗口每轮至少缩一格，最后 left == right 时窗口只剩一个候选，那个位置就是答案。
 *
 * 对比 11-searching/binary-search 那节的"精确查找"模板（while left<=right + return mid），
 * 这里就是"lower_bound"那一族：找**第一个满足条件的位置**，所以右端点不能扔。
 */
int ra_min_index(const int a[], int n, int *steps_out)
{
    if (a == NULL || n <= 0) {
        if (steps_out) {
            *steps_out = 0;
        }
        return -1;
    }

    int left = 0;
    int right = n - 1;
    int steps = 0;

    while (left < right) {
        steps++;
        /* 防溢出写法：left + (right - left) / 2。见 binary-search 那节第 6 幕的实测 */
        int mid = left + (right - left) / 2;

        if (a[mid] > a[right]) {
            left = mid + 1;      /* 断崖在 mid 右边 */
        } else {
            right = mid;         /* 断崖在 mid 左边，或者压根没断崖；mid 自己仍是候选 */
        }
    }

    if (steps_out) {
        *steps_out = steps;
    }
    return left;
}

/* 故意写错的版本：拿 a[mid] 和 a[left] 比。见 ra_demo_why_left_fails 的反例 */
int ra_min_index_wrong(const int a[], int n)
{
    if (a == NULL || n <= 0) {
        return -1;
    }

    int left = 0;
    int right = n - 1;

    while (left < right) {
        int mid = left + (right - left) / 2;
        if (a[mid] > a[left]) {
            /* 错误就错在这一句：a[mid] > a[left] 只说明"mid 比窗口最左边大"，
             * 它既可能意味着"整段升序、最小值就在 left"，
             * 也可能意味着"断崖还在右边没走到"。两种结论共用一个条件 ⇒ 必错一种。 */
            right = mid - 1;   /* 贸然往左扔：把可能藏着断崖的整段右半丢了 */
        } else {
            left = mid + 1;    /* 顺手把 mid 也扔了 —— 而 mid 完全可能就是最小值 */
        }
    }
    return left;
}

/*
 * ⚠ 顺带一个血泪教训：如果把上面那句写成看似更「保守」的 `left = mid`，
 *   当窗口只剩两格（right - left == 1）时 mid == left，于是 `left = mid` 什么都没推进 —— **死循环**。
 *   故意写错的算法不仅能给错答案，还能把整个程序卡死。
 *   所以本节把错版单独关在一个函数里、并且保证它一定停机，只让它「答错」不让它「跑飞」。
 */

int ra_min_value(const int a[], int n)
{
    int i = ra_min_index(a, n, NULL);
    return (i < 0) ? 0 : a[i];
}

/*
 * 最大值就在旋转点**左边一格**。
 * 为什么要用 (pivot + n - 1) % n 而不是 pivot - 1？因为未旋转时 pivot == 0，
 * pivot - 1 会算出 -1（越界），取模后正好是 n-1 —— 也就是"最大值在数组末尾"，完全正确。
 * 这一个 %n 就是"环形思维"的最小示范。
 */
int ra_max_index(const int a[], int n)
{
    if (a == NULL || n <= 0) {
        return -1;
    }
    int pivot = ra_min_index(a, n, NULL);
    if (pivot < 0) {
        return -1;
    }
    return (pivot + n - 1) % n;
}

int ra_rotations(const int a[], int n)
{
    return ra_min_index(a, n, NULL);
}

/* ===================================================================
 * 2. 写法 A：一次二分 + 判半区（无重复）
 * =================================================================== */

/*
 * 每一步的思考方式（对着注释读代码）：
 *   ① a[mid] 命中 → 直接返回。
 *   ② 否则判断 [left..mid] 是不是**有序的那半**：判据 a[left] <= a[mid]。
 *      无重复数组里，只要 a[left] <= a[mid]，这段就不含断崖（断崖会让值突然掉下去）。
 *   ③ 左半有序 → 看 target 有没有落在 [a[left], a[mid]) 这个**已知完整**的取值范围里：
 *      在 → 扔右半；不在 → 扔左半。
 *   ④ 左半无序（说明断崖在左半）→ 右半 [mid..right] 必然有序，反过来判 (a[mid], a[right]]。
 * 关键认知：**每一轮总有一半是干净的有序段**，二分的底气来自这一句，而不是"整个数组有序"。
 */
int ra_search(const int a[], int n, int target, int verbose, int *steps_out)
{
    if (a == NULL || n <= 0) {
        if (steps_out) {
            *steps_out = 0;
        }
        return -1;
    }

    int left = 0;
    int right = n - 1;
    int steps = 0;

    while (left <= right) {
        steps++;
        int mid = left + (right - left) / 2;

        if (a[mid] == target) {
            if (verbose) {
                printf("      第 %d 轮  left=%d right=%d mid=%d  a[mid]=%d == 目标 %d → 命中，返回 %d\n",
                       steps, left, right, mid, a[mid], target, mid);
            }
            if (steps_out) {
                *steps_out = steps;
            }
            return mid;
        }

        if (a[left] <= a[mid]) {
            /* 左半 [left..mid] 有序 */
            int go_left = (a[left] <= target && target < a[mid]);
            if (verbose) {
                printf("      第 %d 轮  left=%d right=%d mid=%d  a[mid]=%d  ≠ %d\n",
                       steps, left, right, mid, a[mid], target);
                printf("              a[left]=%d <= a[mid]=%d → 左半 [%d..%d] 有序，取值 [%d..%d]\n",
                       a[left], a[mid], left, mid, a[left], a[mid]);
                printf("              目标 %d %s [%d, %d) → %s\n",
                       target, go_left ? "落在" : "不在", a[left], a[mid],
                       go_left ? "扔右半：right = mid-1" : "扔左半：left = mid+1");
            }
            if (go_left) {
                right = mid - 1;
            } else {
                left = mid + 1;
            }
        } else {
            /* 断崖在左半 ⇒ 右半 [mid..right] 有序 */
            int go_right = (a[mid] < target && target <= a[right]);
            if (verbose) {
                printf("      第 %d 轮  left=%d right=%d mid=%d  a[mid]=%d  ≠ %d\n",
                       steps, left, right, mid, a[mid], target);
                printf("              a[left]=%d >  a[mid]=%d → 断崖在左半，右半 [%d..%d] 有序，取值 [%d..%d]\n",
                       a[left], a[mid], mid, right, a[mid], a[right]);
                printf("              目标 %d %s (%d, %d] → %s\n",
                       target, go_right ? "落在" : "不在", a[mid], a[right],
                       go_right ? "扔左半：left = mid+1" : "扔右半：right = mid-1");
            }
            if (go_right) {
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }
    }

    if (steps_out) {
        *steps_out = steps;
    }
    return -1;
}

/* ===================================================================
 * 2'. 写法 B：先找旋转点，再用 (虚拟下标 + pivot) % n 复用标准二分
 * =================================================================== */

/*
 * 这个写法的好处是**二分部分一行新代码都没有**：它就是普通二分，
 * 唯一的改动是把 `a[mid]` 换成了 `a[(mid + pivot) % n]`。
 * 于是读者能非常直观地看到：旋转这件事被 %n "洗掉"了。
 * 代价：要多一次 O(log n) 求 pivot（同一个数量级，常数变大一点点）。
 */
int ra_search_by_offset(const int a[], int n, int target, int verbose)
{
    if (a == NULL || n <= 0) {
        return -1;
    }

    int steps = 0;
    int pivot = ra_min_index(a, n, &steps);
    if (pivot < 0) {
        return -1;
    }
    if (verbose) {
        printf("      先花 %d 轮找到旋转点 pivot=%d（a[%d]=%d 是最小值），此后按升序虚拟下标二分\n",
               steps, pivot, pivot, a[pivot]);
    }

    int left = 0;            /* 虚拟下标：0 是最小值那一格 */
    int right = n - 1;       /* 虚拟下标的末端 = 最大值那一格 */
    int vsteps = 0;

    while (left <= right) {
        vsteps++;
        int mid = left + (right - left) / 2;
        int real = (mid + pivot) % n;     /* ★ 全节唯一的"洗掉旋转"动作 */

        if (a[real] == target) {
            if (verbose) {
                printf("      第 %d 轮  虚拟[%d..%d] mid=%d → 真实下标 (%d+%d)%%%d=%d  a=%d == 目标 → 命中\n",
                       vsteps, left, right, mid, pivot, mid, n, real, a[real]);
            }
            return real;
        }
        if (verbose) {
            printf("      第 %d 轮  虚拟[%d..%d] mid=%d → 真实 (%d+%d)%%%d=%d  a=%d %c 目标 %d → %s\n",
                   vsteps, left, right, mid, pivot, mid, n, real, a[real],
                   (a[real] < target) ? '<' : '>', target,
                   a[real] < target ? "left=mid+1" : "right=mid-1");
        }
        if (a[real] < target) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    return -1;
}

/* ===================================================================
 * 3. 有重复元素：判据失效时只能"退一步"
 * =================================================================== */

/*
 * 唯一的新增逻辑是开头那两句"扔掉重复端点"：
 *     if (a[mid] == a[left])  left++;
 *     if (a[mid] == a[right]) right--;
 * 为什么敢扔 a[left]？因为这一轮已经确认 a[mid] != target，而 a[left] == a[mid]，
 * 所以 a[left] 也必然 != target —— 扔掉的这个格子**本来就不可能是答案**，不丢解。
 * 这就是"信息不足时先扔一个一定不是答案的元素"的标准套路。
 *
 * 代价：最坏（全数组同值、目标不在其中）每轮只扔一格 ⇒ 轮数 == n-1 次量级 ⇒ O(n)。
 * steps_out 会把真实轮数打出来，main.c 第 5 幕就是拿这个数字证明退化。
 */
int ra_search_dup(const int a[], int n, int target, int verbose, int *steps_out)
{
    if (a == NULL || n <= 0) {
        if (steps_out) {
            *steps_out = 0;
        }
        return -1;
    }

    int left = 0;
    int right = n - 1;
    int steps = 0;

    while (left <= right) {
        steps++;
        int mid = left + (right - left) / 2;

        if (a[mid] == target) {
            if (verbose) {
                printf("      第 %d 轮  mid=%d a[mid]=%d == 目标 → 命中 %d\n", steps, mid, a[mid], mid);
            }
            if (steps_out) {
                *steps_out = steps;
            }
            return mid;
        }

        if (a[mid] == a[left]) {
            if (verbose) {
                printf("      第 %d 轮  a[mid]=%d == a[left]=%d → 无法判断 mid 在哪段，扔一格 left++\n",
                       steps, a[mid], a[left]);
            }
            left++;
            continue;
        }
        if (a[mid] == a[right]) {
            if (verbose) {
                printf("      第 %d 轮  a[mid]=%d == a[right]=%d → 无法判断，扔一格 right--\n",
                       steps, a[mid], a[right]);
            }
            right--;
            continue;
        }

        /* 到这里 a[mid] 和两端都不相等，判据重新变得可靠 */
        if (a[left] < a[mid]) {
            int go_left = (a[left] <= target && target < a[mid]);
            if (verbose) {
                printf("      第 %d 轮  a[left]=%d < a[mid]=%d → 左半有序，目标 %d %s → %s\n",
                       steps, a[left], a[mid], target,
                       go_left ? "在区间内" : "不在区间内",
                       go_left ? "right=mid-1" : "left=mid+1");
            }
            if (go_left) {
                right = mid - 1;
            } else {
                left = mid + 1;
            }
        } else {
            int go_right = (a[mid] < target && target <= a[right]);
            if (verbose) {
                printf("      第 %d 轮  a[left]=%d > a[mid]=%d → 右半有序，目标 %d %s → %s\n",
                       steps, a[left], a[mid], target,
                       go_right ? "在区间内" : "不在区间内",
                       go_right ? "left=mid+1" : "right=mid-1");
            }
            if (go_right) {
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }
    }

    if (steps_out) {
        *steps_out = steps;
    }
    return -1;
}

/* ===================================================================
 * 4. 对照组 / 验收工具
 * =================================================================== */

int ra_search_linear(const int a[], int n, int target)
{
    if (a == NULL || n <= 0) {
        return -1;
    }
    for (int i = 0; i < n; i++) {
        if (a[i] == target) {
            return i;
        }
    }
    return -1;
}

/*
 * 数下降点：a[i] > a[i+1] 出现几次。
 *   0 次 → 普通升序（相当于旋转 0 次，也算合法）
 *   1 次 → 合法旋转数组
 *   ≥2 次 → 绝不可能是旋转过的升序数组（因为两段各自升序，只能掉一次）
 * 注意这里只查了"相邻段内部升序"，**没有**再查"首段最小值 ≥ 尾段最大值"这一条。
 * 对 [0..n-1] 的相邻对来说，只要下降点 ≤ 1，那条隐含的环形边 (n-1 → 0) 就是唯一可能的断崖，
 * 所以这个判据是充分的。写成 O(n) 而不是 O(log n)：这是**校验器**，要的是显然正确。
 */
int ra_is_rotated_sorted(const int a[], int n)
{
    if (a == NULL || n <= 1) {
        return 1;                        /* 空数组 / 单元素： trivially 合法 */
    }
    int drops = 0;
    for (int i = 0; i + 1 < n; i++) {
        if (a[i] > a[i + 1]) {
            drops++;
            if (drops > 1) {
                return 0;
            }
        }
    }
    return 1;
}

/* 严格版：还要求相邻都不相等（ra_search 的前置条件） */
int ra_is_strictly_rotated_asc(const int a[], int n)
{
    if (a == NULL || n <= 1) {
        return 1;
    }
    if (!ra_is_rotated_sorted(a, n)) {
        return 0;
    }
    for (int i = 0; i + 1 < n; i++) {
        if (a[i] == a[i + 1]) {
            return 0;
        }
    }
    return 1;
}

/* ===================================================================
 * 5. 招牌演示：为什么判据不能换成 a[mid] vs a[left]
 * =================================================================== */

/*
 * 反例只有两个数组，构造原则是：让 a[left] < a[mid] 在两边**同时成立**，
 * 但最小值一个在 mid 左边、一个在 mid 右边。
 *   A = 1 2 3 4 5      ：left=0 right=4 mid=2 → a[left]=1 < a[mid]=3。最小值在**左**（下标 0）
 *   B = 3 4 5 1 2      ：left=0 right=4 mid=2 → a[left]=3 < a[mid]=5。最小值在**右**（下标 3）
 * 同一个条件、两种相反结论 ⇒ 这个条件不足以决策 ⇒ 用它写出来的算法必错一个。
 * 而 a[mid] vs a[right]：A 里 3 < 5 → 往左收；B 里 5 > 2 → 往右收。判据给的方向恰好都对。
 */
void ra_demo_why_left_fails(void)
{
    int A[5] = { 1, 2, 3, 4, 5 };
    int B[5] = { 3, 4, 5, 1, 2 };

    printf("    ┌── 两个数组，mid 都落在下标 2 ─────────────────────────────\n");
    printf("    │ A: 1 2 3 4 5   a[left]=%d  a[mid]=%d  a[right]=%d   最小值真实下标 = 0\n",
           A[0], A[2], A[4]);
    printf("    │ B: 3 4 5 1 2   a[left]=%d  a[mid]=%d  a[right]=%d   最小值真实下标 = 3\n",
           B[0], B[2], B[4]);
    printf("    └──────────────────────────────────────────────────────────\n\n");

    printf("    判据「a[mid] vs a[left]」：A 里 %d>%d 成立，B 里 %d>%d 也成立 ——\n",
           A[2], A[0], B[2], B[0]);
    printf("      同一个条件在两个数组上都为真，可答案一个在左（下标 0）一个在右（下标 3）。\n");
    printf("      ⇒ 这个条件**不含足够信息**。用它写出来的错版函数跑一下：\n");

    int wa = ra_min_index_wrong(A, 5);
    int wb = ra_min_index_wrong(B, 5);
    printf("        错版 A → 返回下标 %d（值 %d），正确答案是下标 0（值 %d）%s\n",
           wa, A[wa], A[0], wa == 0 ? "  ✓ 碰巧对了" : "  ✗ 错！");
    printf("        错版 B → 返回下标 %d（值 %d），正确答案是下标 3（值 %d）%s\n",
           wb, B[wb], B[3], wb == 3 ? "  ✓ 对了" : "  ✗ 错！");

    printf("\n    判据「a[mid] vs a[right]」（本节采用的正确写法）：\n");
    int sa = 0;
    int sb = 0;
    int ga = ra_min_index(A, 5, &sa);
    int gb = ra_min_index(B, 5, &sb);
    printf("        正版 A → 返回下标 %d（值 %d），%d 轮\n", ga, A[ga], sa);
    printf("        正版 B → 返回下标 %d（值 %d），%d 轮\n", gb, B[gb], sb);
    printf("\n    一句话记住：**右端点是「当前窗口最后一格」，拿 mid 和它比，问的是「mid 之后有没有掉下去」**；\n");
    printf("    而左端点只告诉你「mid 比开头大」，这句话在「整段升序」和「断崖还在右边」两种局面下都能为真。\n");
}

/* ===================================================================
 * 6. 同族应用：旋转数组的第 k 小
 * =================================================================== */

/*
 * 普通数组求第 k 小要 O(n)（见 kth-smallest 那节），这里只要 O(log n)，
 * 因为旋转数组**本身就是排好序的、只是被剪开挪了个位置**。
 * 所以第 k 小 = 从旋转点开始往后数第 k 格 = a[(pivot + k - 1) % n]。
 * k 从 1 开始数（"第 1 小"就是最小值），这和下标从 0 开始是两个不同的约定，
 * 代码里 k-1 那一步就是这两套约定之间的桥 —— 别省，省了就整体偏移一位。
 */
int ra_kth_smallest_index(const int a[], int n, int k)
{
    if (a == NULL || n <= 0 || k < 1 || k > n) {
        return -1;
    }
    int pivot = ra_min_index(a, n, NULL);
    if (pivot < 0) {
        return -1;
    }
    return (pivot + k - 1) % n;
}
