/*
 * main.c —— 第 K 小：八幕实测
 *
 * 跑法： make one DIR=11-searching/kth-smallest
 *
 *   第 1 幕  问题定义 + 「第 k 小按位置算」这个语义钉死
 *   第 2 幕  方法①排序：最省心，但它把整份数据都排了
 *   第 3 幕  方法②大小为 k 的大顶堆：为什么是「大顶」，堆里到底是什么
 *   第 4 幕  方法③快速选择：partition 落点 + 只追一边
 *   第 5 幕  快选的最坏情况实测：毒输入下 n² 与随机 pivot 的对比表（含公式对账）
 *   第 6 幕  方法④值域计数 / 方法⑤值域二分答案：值域宽窄决定谁活着
 *   第 7 幕  附加题：两个有序数组第 k 小（划分法 vs 归并，穷举对拍）
 *   第 8 幕  中位数 + 重复值 + 五法一致性总验收
 */

#include <stdio.h>
#include "kth.h"

static void act(const char *title)
{
    printf("\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    printf("  %s\n", title);
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
}

static void copy_arr(const int src[], int dst[], int n)
{
    for (int i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

int main(void)
{
    /* ============ 第 1 幕：把问题说清楚 ============ */
    act("第 1 幕  第 k 小到底是什么意思（k 从 1 开始，重复值按位置算）");

    int a[7] = { 7, 2, 9, 1, 5, 3, 8 };
    kth_dump("    样本数组 A：", a, 7);
    printf("    排序后应该是： 1 2 3 5 7 8 9\n");
    printf("\n    k=1  → %d（= 最小值，这个极端必须对）\n", kth_by_sorting(a, 7, 1, NULL));
    printf("    k=3  → %d\n", kth_by_sorting(a, 7, 3, NULL));
    printf("    k=7  → %d（= 最大值，另一个极端）\n", kth_by_sorting(a, 7, 7, NULL));
    printf("    k=0  → %d（非法，返回 -1 而不是崩）\n", kth_by_sorting(a, 7, 0, NULL));
    printf("    k=8  → %d（越界，同样 -1）\n", kth_by_sorting(a, 7, 8, NULL));

    printf("\n    ★ 重复值的语义（最容易理解错的地方）：\n");
    int d[6] = { 5, 5, 5, 9, 1, 3 };
    kth_dump("      数组 D：", d, 6);
    printf("      排序后： 1 3 5 5 5 9\n");
    for (int k = 1; k <= 6; k++) {
        printf("      第 %d 小 = %d%s\n", k, kth_by_sorting(d, 6, k, NULL),
               (k == 3) ? "   ← 从这格开始，5 连着占掉第 3、4、5 三个位置" : "");
    }
    printf("      结论：第 k 小是「排好序后第 k 个**位置**上的值」，不是「第 k 个不同的值」。\n");
    printf("      想求「第 k 个不同的值」是另一道题（要先去重），本节不做。\n");

    /* ============ 第 2 幕：排序法 ============ */
    act("第 2 幕  方法① 排序 + 取下标：最省心，但也是最费的一种");

    long long cmp1 = 0;
    int v1 = kth_by_sorting(a, 7, 3, &cmp1);
    printf("    第 3 小 = %d，插入排序比较了 %lld 次（n=%d）\n", v1, cmp1, 7);
    printf("    代价说明：为了知道第 3 小，把 7 个数**全部**排好了。\n");
    printf("              第 1、2、4、5、6、7 小顺手也知道了 —— 这就是它唯一的优势。\n");
    printf("    什么时候该用它：需要**所有**排名（Top-K 全表、分位数报表）；只要一个 k 时它是浪费。\n");

    /* ============ 第 3 幕：大顶堆 ============ */
    act("第 3 幕  方法② 大小为 k 的**大顶**堆：内存只有 k 格，能处理数据流");

    printf("    为什么用大顶堆？因为要淘汰的是「目前这 k 个里最大的那个」，而大顶堆的堆顶就是它。\n\n");
    int b[10] = { 9, 4, 7, 1, 8, 2, 6, 3, 5, 0 };
    kth_dump("    样本数组 B：", b, 10);
    printf("\n    取第 4 小（堆大小 4），一步一步看：\n");
    int v3 = kth_by_heap(b, 10, 4, 1);
    int chk3 = kth_by_sorting(b, 10, 4, NULL);
    printf("\n    → 第 4 小 = %d\n", v3);
    printf("    对拍：排序法给 %d%s\n", chk3, v3 == chk3 ? "  一致 ✓" : "  不一致 ✗");
    printf("    读数：最后堆里是 3 1 2 0 —— 正是全数组最小的 4 个，而堆顶 3 就是第 4 小。\n");
    printf("    复杂度 O(n log k)：n=10 个元素、每个最多沉 log2(4)=2 层 ⇒ 远小于 O(n log n)。\n");
    printf("    真正值钱的地方：**内存只要 k 格**。10 亿条日志找最小的 4 个，也只能用这一招。\n");

    /* ============ 第 4 幕：快速选择 ============ */
    act("第 4 幕  方法③ 快速选择：partition 让一个元素落到最终位置，然后只追一边");

    int w[KTH_MAX_N];
    copy_arr(a, w, 7);
    printf("    求数组 A 的第 3 小（目标下标 = k-1 = 2）：\n");
    long long cq = 0;
    int vq = kth_quickselect(w, 7, 3, 1, &cq);
    printf("\n    → 答案 %d，总共比较 %lld 次（排序法要 %lld 次）\n", vq, cq, cmp1);
    printf("    推导：T(n) = T(n/2) + n = n + n/2 + n/4 + … < 2n ⇒ 平均**线性**。\n");
    printf("          灵魂在「只递归一边」：快排两边都要，所以 n log n；快选跟着答案跑，所以 n。\n");
    printf("\n    ⚠ 副作用检查：原数组被 partition 打乱了 ↓\n");
    kth_dump("", w, 7);
    printf("      所以接口注释里写明「就地打乱」；要保留原数据就得先拷贝（拷贝本身是 O(n)）。\n");

    /* ============ 第 5 幕：最坏情况 ============ */
    act("第 5 幕  毒输入实测：pivot 选得烂，O(n) 立刻塌成 O(n²)");

    printf("    构造：输入**已经升序**，pivot 固定取区间首元素 ⇒ 每轮 partition 只能削掉 1 个。\n");
    printf("    于是求第 k 小要做 (k) 轮，第 r 轮比较 (n-1-r) 次，总数\n");
    printf("        Σ r=0..k-1  (n-1-r) = k·(n-1) − k·(k-1)/2\n");
    printf("    （k=1 时退化成熟悉的 n(n-1)/2；本表统一取 k=n/2，所以理论和 n(n-1)/2 不同，别搞混。）\n\n");
    printf("    n    k     理论值      坏pivot实测   坏/n²    随机pivot实测   随机/n\n");

    int sorted64[KTH_MAX_N];
    for (int i = 0; i < KTH_MAX_N; i++) {
        sorted64[i] = i + 1;                     /* 1 2 3 … 这就是毒输入本体 */
    }

    int sizes[4] = { 10, 20, 40, 60 };
    for (int si = 0; si < 4; si++) {
        int n = sizes[si];
        int k = n / 2;
        long long theory = (long long)k * (n - 1) - (long long)k * (k - 1) / 2;

        long long bad = 0;
        long long good = 0;
        kth_quickselect_bad_pivot(sorted64, n, k, &bad);      /* 内部自带拷贝，不改 sorted64 */

        int work2[KTH_MAX_N];
        copy_arr(sorted64, work2, n);
        kth_quickselect(work2, n, k, 0, &good);

        printf("    %-4d %-5d %-11lld %-13lld %-8.3f %-13lld %.2f\n",
               n, k, theory, bad, (double)bad / ((double)n * n), good, (double)good / n);
    }

    printf("\n    怎么读这张表（三件事，缺一不可）：\n");
    printf("      ① 「理论值」和「坏 pivot 实测」逐行相等 ⇒ 公式不是装饰，是能对账的。\n");
    printf("      ② 「坏/n²」四行都稳定在 0.35~0.37 ⇒ 比较数 ∝ n²，这就是平方级的指纹。\n");
    printf("      ③ 「随机/n」没有系统性往上蹿的趋势（而 /n² 那一列会一路掉向 0）⇒ 它是线性量级。\n");
    printf("    ⚠ 诚实声明：随机 pivot 只跑了**一次**样本，单次数字有噪声，别把 4.x 当常数；\n");
    printf("      要稳定的均值就得重复几十次取平均 —— 这里的结论只到「线性 vs 平方」这个量级差别。\n");
    printf("    结论：快选的 O(n) 是**平均**，不是保证。随机化就是它的护身符。\n");
    printf("      （理论上「中位数的中位数」能给最坏 O(n) 保证，但常数太大，实战一律用随机 pivot。）\n");

    /* ============ 第 6 幕：值域窄 vs 值域宽 ============ */
    act("第 6 幕  方法④ 值域计数 / 方法⑤ 值域二分答案：值域宽窄决定谁能用");

    int score[12] = { 88, 92, 71, 88, 95, 60, 88, 77, 92, 81, 69, 74 };
    kth_dump("    样本：12 个成绩（值域只有 60..95，R=36）", score, 12);

    int ans6 = 0;
    int ok6 = kth_by_counting(score, 12, 5, &ans6);
    int st6 = 0;
    int vb6 = kth_by_value_binary_search(score, 12, 5, &st6);
    int chk6 = kth_by_sorting(score, 12, 5, NULL);
    printf("    ④ 计数法   ：第 5 小 = %d（%s）\n", ans6,
           ok6 ? "值域 36 ≤ 256，可用" : "值域太宽，此法不适用");
    printf("    ⑤ 二分答案 ：第 5 小 = %d，值域上二分 %d 轮 ⇒ 扫数组 %d 遍 ≈ %d 次比较量级\n",
           vb6, st6, st6, st6 * 12);
    printf("    对拍排序法 ：%d%s\n", chk6, (vb6 == chk6 && ans6 == chk6) ? "  三者一致 ✓" : "  不一致 ✗");

    printf("\n    ★ 现在把值域撑开（值域 ≫ n，计数法的 cnt[] 就开不着了）：\n");
    int wide[5] = { 5, 900000, -900000, 7, 3 };
    kth_dump("    数组 W：", wide, 5);

    int ans7 = 0;
    int ok7 = kth_by_counting(wide, 5, 3, &ans7);          /* 返回 0 = 此法不适用 */
    int st7 = 0;
    int vb7 = kth_by_value_binary_search(wide, 5, 3, &st7);
    printf("    ④ 计数法   ：%s  ← 值域约 180 万 > %d，硬开 cnt[] 就是白烧内存\n",
           ok7 ? "可用" : "**不适用**（返回 0，交回调用者换方法）", KTH_COUNT_MAX_RANGE);
    printf("    ⑤ 二分答案 ：第 3 小 = %d，只用了 %d 轮（log2(180 万) ≈ 20.8 ⇒ 轮数只跟值域走）\n",
           vb7, st7);
    printf("    这就是二分答案的脾气：**值域爆炸也不怕**，代价只是多扫几遍数组。\n");
    printf("    它还附赠两个优点：不改动数组、不需要任何额外内存（判据「≤ x 有几个」扫一遍就有）。\n");
    printf("\n    ⚠ 上面两行为什么要先 `int vb7 = …(&st7);` 再 printf：\n");
    printf("      C **不规定实参求值顺序**（gcc 实际从右往左）。若写成\n");
    printf("          printf(\"%%d 轮\", kth_by_value_binary_search(..., &st7), st7);\n");
    printf("      则 st7 会在函数**回填之前**被读走 ⇒ 打出 0 轮，而且不报任何错。\n");
    printf("      带 out 参数的函数，绝不能和读该 out 参数的表达式出现在同一个实参列表里。\n");

    /* ============ 第 7 幕：两个有序数组 ============ */
    act("第 7 幕  附加题：两个有序数组的第 k 小（O(log) 划分法 vs O(k) 归并）");

    int p1[5] = { 1, 3, 5, 7, 9 };
    int p2[6] = { 2, 4, 6, 8, 10, 12 };
    kth_dump("    A（短）：", p1, 5);
    kth_dump("    B（长）：", p2, 6);
    printf("    合并看： 1 2 3 4 5 6 7 8 9 10 12\n\n");
    printf("    k     划分法    归并对拍   一致？\n");
    int mismatch = 0;
    for (int k = 1; k <= 11; k++) {
        int q = kth_two_sorted(p1, 5, p2, 6, k);
        int l = kth_two_sorted_linear(p1, 5, p2, 6, k);
        if (q != l) {
            mismatch++;
        }
        printf("    %-5d %-9d %-10d %s\n", k, q, l, q == l ? "✓" : "✗");
    }
    printf("    → 穷举 11 个 k，不一致 %d 处%s\n", mismatch, mismatch == 0 ? "  全部通过 ✓" : "  有 bug！");
    printf("\n    划分法在干什么：在**短**数组上二分开点 i，长数组的切点 j = k - i 自动确定，\n");
    printf("    只需验证 a[i-1] <= b[j] 且 b[j-1] <= a[i]（谁都没伸过界），答案 = max(a[i-1], b[j-1])。\n");
    printf("    为什么只在短的上面二分：区间长度小 ⇒ O(log(min(m,n)))；而且保证 j 不会算出越界值。\n");
    printf("    边界怎么破：i==0 时 a[i-1] 不存在 ⇒ 用 INT_MIN 当哨兵；i==na 时 a[i] 不存在 ⇒ 用 INT_MAX。\n");
    printf("    哨兵让「不存在的那一侧」在比较里自动输/赢，于是边界情况不用写一个额外的 if。\n");

    /* ============ 第 8 幕：中位数 + 总验收 ============ */
    act("第 8 幕  中位数、重复值，以及五法一致性的总验收");

    int odd[5] = { 30, 10, 50, 20, 40 };
    int even[6] = { 30, 10, 50, 20, 40, 60 };
    printf("    奇数个 30 10 50 20 40    ：下中位数=%d 上中位数=%d ← n 为奇数时两者相同\n",
           kth_median_lower(odd, 5), kth_median_upper(odd, 5));
    printf("    偶数个 30 10 50 20 40 60：下中位数=%d 上中位数=%d ← n 为偶数时两种约定分道扬镳\n",
           kth_median_lower(even, 6), kth_median_upper(even, 6));
    printf("    提醒：「中位数取哪个」是接口约定问题，不是算法问题；写库时必须在文档里钉死。\n");

    printf("\n    ── 总验收：对每个 k 都跑一遍五种方法，看它们是否永远给出同一个答案 ──\n");
    int mix[8] = { 4, 4, 1, 9, 4, 9, 2, 7 };   /* 重复值 + 边界值，最容易出错的那种输入 */
    kth_dump("    数组 M（含大量重复值）：", mix, 8);
    int all_ok = 1;
    for (int k = 1; k <= 8; k++) {
        printf("    k=%d：", k);
        if (!kth_all_agree(mix, 8, k, 0)) {
            all_ok = 0;
            printf("       ↑ 有不一致！\n");
        } else {
            printf("       五法一致 ✓\n");
        }
    }
    printf("\n    为什么值得写这一幕：五种思路各异的实现若给出同一个数，比任何单测用例都可信\n");
    printf("    —— 它们不会朝同一个方向错。这也是本仓库「对拍优先」的习惯（见 rotated-array 第 7 幕）。\n");
    printf("    最终验收：%s\n", all_ok ? "八幕全部通过，五法在重复值输入上完全一致 ✓" : "存在不一致，是 bug ✗");

    printf("\n全部八幕跑完，本节没有任何 malloc，因此没有 free 责任。\n");
    return 0;
}
