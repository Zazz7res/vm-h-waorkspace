#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "topk.h"

static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/* 把 out 的 K 个打印成一行（不带归一化） */
static void print_result(const char *tag, const int *out, int k)
{
    printf("  %s: [", tag);
    for (int i = 0; i < k; i++) {
        printf(" %d", out[i]);
    }
    printf(" ]\n");
}

/* ---------------------------------------------------------------------
 * 小工具：按【显示宽度】补空格，让中英混排的表格对齐
 * ---------------------------------------------------------------------
 * printf 的 %-19s 数的是【字节】，而 UTF-8 里一个汉字占 3 字节却只显示 2 格，
 * 所以 "① 全排序 O(n log n)" 这种字符串用 %-Ns 一定排不齐（这是写中文控制台
 * 输出的必经之坑）。这里自己算宽度：ASCII 算 1 格，其余（汉字、①②③）算 2 格。
 * ------------------------------------------------------------------ */
static int disp_width(const char *s)
{
    int w = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        if ((*p & 0x80u) == 0u) {
            w += 1;
            p += 1;
        } else if ((*p & 0xE0u) == 0xC0u) {
            w += 2;
            p += 2;
        } else if ((*p & 0xF0u) == 0xE0u) {
            w += 2;
            p += 3;
        } else {
            w += 2;
            p += 4;
        }
    }
    return w;
}

/* 左对齐打印，占满 target 个显示格 */
static void pad_to(const char *s, int target)
{
    printf("%s", s);
    for (int i = disp_width(s); i < target; i++) {
        printf(" ");
    }
}

/* 只问"花多少次比较"：把一份数据喂给快速选择，返回计数。
 * out 缓冲区按 n 个 int 申请 —— K 最大能等于 n，缓冲区必须按最坏情况给。 */
static long quickselect_cmp(const int *arr, int n, int k)
{
    int *out = malloc((size_t)n * sizeof(int));
    if (out == NULL) {
        return -1;
    }
    long c = 0;
    topk_by_quickselect(arr, n, k, out, &c);
    free(out);
    return c;
}

/* 三种"看起来乱、其实完全确定"的序列生成器（不用 rand，保证可复核） */
static void fill_arith(int *a, int n)        /* 等差取模：结构化，坑快速选择 */
{
    for (int i = 0; i < n; i++) {
        a[i] = (i * 7919) % 1000003;
    }
}

static void fill_lcg(int *a, int n)          /* 线性同余：打散得很均匀 */
{
    unsigned long long x = 12345;
    for (int i = 0; i < n; i++) {
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
        a[i] = (int)((x >> 33) % 1000003u);
    }
}

static void fill_desc(int *a, int n)         /* 直接递减：教科书里的"最坏输入" */
{
    for (int i = 0; i < n; i++) {
        a[i] = n - i;
    }
}

int main(void)
{
    /* ============================================================
       第 1 幕：问题 + 固定测试数据（可手算）
       ============================================================ */
    section("1. Top-K：从 n 个数里挑出最大的 K 个");

    /* 硬编码固定数组，不随机 —— 答案可以自己口算核对：
       降序全体是 92 88 78 67 60 55 41 31 23 14 12 9 7 5 3 */
    static const int DATA[] = {12, 5, 78, 3, 41, 67, 9, 55, 23, 88, 14, 92, 31, 7, 60};
    int n = (int)(sizeof(DATA) / sizeof(DATA[0]));
    int k = 3;

    topk_print_array("数据", DATA, n);
    printf("  n=%d，要最大的 K=%d 个 → 手算答案应该是 92、88、78\n", n, k);
    printf("  下面三种方法都要给出这个答案；区别只在【花多少力气】。\n");

    int r1[8], r2[8], r3[8];
    long c1 = 0, c2 = 0, c3 = 0;

    /* ============================================================
       第 2 幕：方法① 全排序取前 K
       ============================================================ */
    section("2. 方法① 全排序（内部用堆排序）→ 从尾巴取 K 个");

    topk_by_sort(DATA, n, k, r1, &c1);
    print_result("① 全排序结果", r1, k);
    printf("  做了 %ld 次比较。\n", c1);
    printf("  代价：为了 3 个答案，把 15 个数【全部】排好序。\n");
    printf("        时间 O(n log n)，额外空间 O(n)（必须拷贝，不能打乱用户的数组）。\n");

    /* ============================================================
       第 3 幕：方法② 大小为 K 的最小堆（本节主角）
       ============================================================ */
    section("3. 方法② 固定大小的【最小堆】守门：为什么求最大 K 个要用最小堆");

    printf("  先把守门过程一步不落地打出来（堆里始终装【目前为止最大的 K 个】）：\n");
    topk_heap_trace(DATA, n, k);

    printf("\n  这段过程说明白了三件事：\n");
    printf("    · 堆【满员】之前没有任何判断，来者不拒（前 K 个直接收下）。\n");
    printf("    · 满员之后，每个新元素只和【堆顶】比一次。堆顶是这 K 个候选里【最小】的那个，\n");
    printf("      也就是【门槛】/【最差候选】/【守门员】—— 三种叫法，同一个东西。\n");
    printf("    · 新数 > 门槛 → 它有资格，把门槛踢掉自己进去。\n");
    printf("      为什么踢门槛是【安全】的？因为堆里另外 K-1 个候选全都 >= 门槛，\n");
    printf("      它们本来就比被踢的那个更配待在「前 K」里；而刚踢掉的那个数，\n");
    printf("      从此再也不可能进前 K —— 后面就算来更大的，被踢的也只会是更大的新门槛。\n");
    printf("      新数 <= 门槛 → 连最差的候选都不如，一次比较就打发掉，什么都不做。\n");

    printf("\n  【反直觉点的正面回答】为什么是【最小】堆而不是最大堆？\n");
    printf("    因为我们要做的判断是「新来的有没有资格挤掉一个」，\n");
    printf("    需要的信息是【候选里最差的那个】。最小堆的堆顶恰好就是它 —— O(1) 白送。\n");
    printf("    假如用最大堆，堆顶是候选里【最好】的，跟新来的完全没法比出结论：\n");
    printf("    你想知道该踢谁，就得把整个堆扫一遍找最小值 → 每个元素 O(K)，\n");
    printf("    总共 O(nK)，比全排序还慢，堆的意义全丢。\n");
    printf("    记忆口诀：**留最大的 K 个 → 用最小堆守门；留最小的 K 个 → 用最大堆守门。\n");
    printf("              堆顶永远放「你最想淘汰的那个」，淘汰决策才是一次比较。**\n");

    topk_by_heap(DATA, n, k, r2, &c2);
    print_result("② K 最小堆结果（原样，堆内顺序）", r2, k);
    printf("  做了 %ld 次比较（对比方法①的 %ld 次）。\n", c2, c1);
    printf("  时间 O(n log K)，额外空间 O(K) —— 这里 K=3，也就是只多占 3 个 int。\n");

    /* 顺手把「Top-K ≠ 排行榜」这个坑先露个头 */
    int raw5[8];
    topk_by_heap(DATA, n, 5, raw5, NULL);
    print_result("  再跑 K=5，堆【原样】输出", raw5, 5);
    topk_normalize_desc(raw5, 5);
    print_result("  同一批数，降序归一化之后", raw5, 5);
    printf("  → 上一行就是本节坑 1：**堆里那 K 个数不代表名次**。要名次得再排一次 O(K log K)。\n");

    /* ============================================================
       第 4 幕：方法③ 快速选择（简版）
       ============================================================ */
    section("4. 方法③ 快速选择：只找『门槛值』，不排序全体");

    topk_by_quickselect(DATA, n, k, r3, &c3);
    print_result("③ 快速选择结果", r3, k);
    printf("  做了 %ld 次比较。\n", c3);
    printf("  思路：升序看，最大的 K 个的下界（门槛）就是第 (n-K)=%d 小的元素。\n", n - k);
    printf("        快排的 partition 每轮会把一个 pivot 放到它最终位置上，\n");
    printf("        那我们就【只递归包含 target 的那一半】，另一半整个扔掉 —— 平均 n + n/2 + n/4… = O(n)。\n");
    printf("        拿到门槛值 threshold 之后，扫一遍把 >= threshold 的都收走就是答案。\n");
    printf("  代价：① 必须打乱工作数组（这里仍拷贝了一份，不改用户数据）；\n");
    printf("        ② 最坏 O(n²)（pivot 老选在端点，本实现用三数取中缓解）；\n");
    printf("        ③ 有重复值时『>= 门槛』的数量可能超过 K，必须补齐/截断，很容易写错。\n");

    /* ============================================================
       第 5 幕：三种方法结果必须一致
       ============================================================ */
    section("5. 三种方法结果核对（先各自降序归一化，再逐位比）");

    print_result("① 归一化降序", r1, k);
    topk_normalize_desc(r2, k);
    print_result("② 归一化降序", r2, k);
    topk_normalize_desc(r3, k);
    print_result("③ 归一化降序", r3, k);

    int same = 1;
    for (int i = 0; i < k; i++) {
        if (r1[i] != r2[i] || r1[i] != r3[i]) {
            same = 0;
        }
    }
    printf("  三者是否完全一致：%s   （手算答案 92 88 78 → %s）\n",
           same ? "✅ 一致" : "❌ 不一致",
           (r1[0] == 92 && r1[1] == 88 && r1[2] == 78) ? "✅ 对上了" : "❌ 不对");
    printf("  比较次数：① %ld   ② %ld   ③ %ld\n", c1, c2, c3);

    /* ============================================================
       第 6 幕：大数据实测 —— clock() 真实耗时
       ============================================================ */
    section("6. 实测：n=20 万、K=1000，三种方法各跑 3 次取最快");

    int N = 200000;
    int BIGK = 1000;
    int *big = malloc((size_t)N * sizeof(int));
    int *bout = malloc((size_t)BIGK * sizeof(int));
    if (big == NULL || bout == NULL) {
        printf("  [错误] 大数组申请失败，跳过实测\n");
    } else {
        /* 固定公式生成的"看着乱"的序列：不是 rand()，每次跑一模一样，可复核。
           （i * 7919 mod 1000003，1000003 是质数，所以前 20 万个值互不相同）
           注意它其实是【等差取模】序列 —— 结构化得很，第 7 幕会现眼。 */
        fill_arith(big, N);
        printf("  数据前 12 个: ");
        for (int i = 0; i < 12; i++) {
            printf("%d ", big[i]);
        }
        printf("\n  n=%d, K=%d（K 只占 n 的 %.1f%%，这正是 Top-K 的典型比例）\n\n",
               N, BIGK, 100.0 * BIGK / N);

        printf("  ");
        pad_to("方法", 24);
        printf(" | ");
        pad_to("最快耗时(ms)", 13);
        printf("  | 比较次数\n");
        printf("  -------------------------+----------------+-----------\n");

        /* 每个方法跑 3 次取最快，减少系统调度带来的噪声；比较次数本身是确定的 */
        for (int m = 1; m <= 3; m++) {
            double best_ms = -1.0;
            long cmp = 0;
            for (int rep = 0; rep < 3; rep++) {
                clock_t t0 = clock();          /* clock()：本进程消耗的 CPU 时间 */
                if (m == 1) {
                    topk_by_sort(big, N, BIGK, bout, &cmp);
                } else if (m == 2) {
                    topk_by_heap(big, N, BIGK, bout, &cmp);
                } else {
                    topk_by_quickselect(big, N, BIGK, bout, &cmp);
                }
                clock_t t1 = clock();
                double ms = (double)(t1 - t0) / (double)CLOCKS_PER_SEC * 1000.0;
                if (best_ms < 0 || ms < best_ms) {
                    best_ms = ms;
                }
            }
            const char *name = m == 1 ? "① 全排序 O(n log n)"
                              : m == 2 ? "② K 最小堆 O(n log K)"
                                       : "③ 快速选择 平均 O(n)";
            printf("  ");
            pad_to(name, 24);
            printf(" | %13.2f  | %ld\n", best_ms, cmp);
        }

        /* 结果正确性：大数据下也互相比一遍 */
        int *a1 = malloc((size_t)BIGK * sizeof(int));
        int *a2 = malloc((size_t)BIGK * sizeof(int));
        int *a3 = malloc((size_t)BIGK * sizeof(int));
        if (a1 && a2 && a3) {
            topk_by_sort(big, N, BIGK, a1, NULL);
            topk_by_heap(big, N, BIGK, a2, NULL);
            topk_by_quickselect(big, N, BIGK, a3, NULL);
            topk_normalize_desc(a1, BIGK);
            topk_normalize_desc(a2, BIGK);
            topk_normalize_desc(a3, BIGK);
            int ok = 1;
            for (int i = 0; i < BIGK; i++) {
                if (a1[i] != a2[i] || a1[i] != a3[i]) {
                    ok = 0;
                }
            }
            printf("\n  大数据下三者结果一致性：%s\n", ok ? "✅ 完全一致" : "❌ 不一致");
            printf("  最大的 5 个（三种方法都是这个答案）：");
            for (int i = 0; i < 5; i++) {
                printf("%d ", a1[i]);
            }
            printf("\n  第 996~1000 名（K 边界附近，最容易写错的地方）：");
            for (int i = BIGK - 5; i < BIGK; i++) {
                printf("%d ", a1[i]);
            }
            printf("\n");
        }
        free(a1);
        free(a2);
        free(a3);

        /* ---- 追加实验 A：K 一路变大，三种方法各自的比较次数怎么走 ---- */
        printf("\n  【追加】n=20 万不动，把 K 一路放大，看比较次数：\n");
        printf("  ");
        pad_to("K", 8);
        printf(" | ");
        pad_to("① 全排序", 12);
        printf(" | ");
        pad_to("② K 最小堆", 12);
        printf(" | ");
        pad_to("③ 快速选择", 12);
        printf("\n");
        /* ⚠ 这里要输出 K 个结果，K 最大能到 N，所以必须有一块 N 个 int 的缓冲区。
         *    第一版我偷懒复用了只有 BIGK=1000 格的小数组，直接被 ASan 抓到越界写
         *    （"缓冲区大小必须按最坏情况算"这件事，ASan 一次就能教会你）。 */
        int *scratch = malloc((size_t)N * sizeof(int));
        int ks[5] = {1, 100, 10000, 100000, 200000};
        for (int ki = 0; ki < 5 && scratch != NULL; ki++) {
            int kk = ks[ki];
            long b1 = 0, b2 = 0, b3 = 0;
            topk_by_sort(big, N, kk, scratch, &b1);
            topk_by_heap(big, N, kk, scratch, &b2);
            topk_by_quickselect(big, N, kk, scratch, &b3);
            printf("  %8d | %12ld | %12ld | %12ld\n", kk, b1, b2, b3);
        }
        free(scratch);

        printf("  ① 那一列一格都不变（6399694）—— 全排序压根不看 K，它每次都把 20 万个数排完，\n");
        printf("     这就是「方法①无论 K 多小都是 O(n log n)」的意思。\n");
        printf("  ② 的读法（K 和 n 的比例决定行为）：\n");
        printf("     K=1     → 199999 = n-1 次，正好退化成「扫一遍记住最大值」，这是理论下限；\n");
        printf("     K=100 → 208682，K=10000 → 922307：K 涨了 100 倍，比较只涨了 4.4 倍，\n");
        printf("                    这就是「log K 涨得很慢」的实测样子；\n");
        printf("     K=100000(=n/2) → 2400352，门槛几乎每来一个数就要被踢一次，最费；\n");
        printf("     K=200000(=n)   → 反而掉回 367279！因为堆【从头到尾没满过】，\n");
        printf("                      全程只走「无脑收下 + 上浮」，一次门槛比较都没发生（相当于纯建堆）。\n");
        printf("     结论：② 的价值全在 K << n。K 接近 n 就别再谈 Top-K 了，直接排序。\n");
        printf("  ③ 这一列很反常：K 小的时候 1.4n，K 大的时候窜到 25n / 40n。原因见第 7 幕。\n");

        free(big);
        free(bout);
    }

    /* ============================================================
       第 7 幕：③ 的比较次数为什么这么不乖 —— 实测三种输入
       ============================================================ */
    section("7. 实测：同样的算法，换一份输入，快速选择的代价差多少");

    {
        int N2 = 200000;
        int *arr = malloc((size_t)N2 * sizeof(int));
        if (arr == NULL) {
            printf("  [错误] 申请失败，跳过这一幕\n");
        } else {
            int ks2[4] = {1, 1000, 100000, 200000};
            printf("  快速选择靠的是 partition 每轮切掉一半；pivot 挑得好不好，全看数据长什么样。\n");
            printf("  这里用三种【完全确定】的序列各测一遍（比较次数）：\n");
            printf("  ");
            pad_to("K", 8);
            printf(" | ");
            pad_to("等差取模序列", 12);
            printf(" | ");
            pad_to("LCG 随机序列", 12);
            printf(" | ");
            pad_to("纯递减序列", 12);
            printf("\n");
            /* 先各生成一次，把三列都算进数组，再按行打印 */
            fill_arith(arr, N2);
            long va[4], vb[4], vc[4];
            for (int i = 0; i < 4; i++) {
                va[i] = quickselect_cmp(arr, N2, ks2[i]);
            }
            fill_lcg(arr, N2);
            for (int i = 0; i < 4; i++) {
                vb[i] = quickselect_cmp(arr, N2, ks2[i]);
            }
            fill_desc(arr, N2);
            for (int i = 0; i < 4; i++) {
                vc[i] = quickselect_cmp(arr, N2, ks2[i]);
            }
            for (int i = 0; i < 4; i++) {
                printf("  %8d | %12ld | %12ld | %12ld\n", ks2[i], va[i], vb[i], vc[i]);
            }
            printf("\n  2n = %d，拿它当尺子量一下：\n", 2 * N2);
            printf("  · LCG 那一列全部在 2n 附近 → 这就是教科书说的「平均 O(n)」，真实随机数据长这样。\n");
            printf("  · 等差取模那一列：K 小（要找最大的几个）时很乖，K 大（要找中位数/最小值）时\n");
            printf("    涨到 25n、40n（是「2n」这个平均值的 12 倍、20 倍）—— 同一份数据、同一个算法，\n");
            printf("    只是目标位置不同就差这么多。下面【现场取证】把每轮切分情况拍给你看。\n");
            printf("  · 纯递减那一列反而在 K=100000 时只要 200002 次：递减序列里 mid 位置的值恰好\n");
            printf("    就是中位数，一枪命中，一次 partition 就收工 —— 别把「有序数据」想成一定是坏事。\n");

            printf("\n  【现场取证】把等差取模那份序列的切分过程打出来，看每轮到底砍掉多少：\n");
            fill_arith(arr, N2);
            printf("\n  A) K=1000（找最大的 1000 个 → 门槛是第 n-K=199000 小）\n");
            topk_quickselect_trace(arr, N2, 1000);
            printf("\n  B) K=200000（要的就是全部 → 门槛是第 0 小，等于找最小值）\n");
            topk_quickselect_trace(arr, N2, N2);
            printf("\n  看明白了吗：同一份数据、同一个算法，只是【目标位置】不同，\n");
            printf("  轮数和比较次数就差了一个数量级。原因是这份序列里 pivot 老是落在\n");
            printf("  区间偏大的那一侧（第 2 轮之后稳定在 99%% 处）：\n");
            printf("    · 要找【最大的】那几个 → 目标在右边那条小尾巴里，第一刀砍掉 79%%，18 轮收工；\n");
            printf("    · 要找【最小的】那一个 → 得一直泡在那个占 99%% 的大块里，一轮只砍掉 1%%（1583 个），\n");
            printf("      于是 104 轮才收敛到 1 个元素，比较次数 40n —— 这就是「最坏 O(n²)」的苗头。\n");
            printf("\n  结论：pivot 切得平不平衡，直接决定快速选择是 O(n) 还是 O(n²)；\n");
            printf("  而 ② 堆方案的比较次数【只跟 K 有关】，输入长什么样都动不了它。\n");
            printf("  工程上：要「稳定的最坏情况」选堆；要「平均最快」选快选，\n");
            printf("  并且要么随机化 pivot，要么上 BFPRT 那种最坏也 O(n) 的选法\n");
            printf("  （完整版在 11-searching/kth-smallest）。\n");
            free(arr);
        }
    }

    /* ============================================================
       收尾
       ============================================================ */
    printf("\n=== 本节结束 ===\n");
    printf("记住五句话：\n");
    printf("  1) 求【最大的 K 个】要用【最小堆】：堆顶是候选里最差的，来了更好的就踢掉它。\n");
    printf("  2) 堆方案 O(n log K) 时间 + O(K) 空间；全排序 O(n log n) + O(n)。n 大而 K 小时差一个量级。\n");
    printf("  3) 快速选择平均 O(n)，但要打乱数组、最坏 O(n²)、重复值边界容易写错 —— 不是无脑最优。\n");
    printf("  4) Top-K 的结果【不是名次】：堆里那 K 个数只有堆顶最小，要排行榜得再排 O(K log K)。\n");
    printf("  5) 实测要跑大数据 + 固定输入 + 打印比较次数；只跑 15 个数是量不出 log n 的。\n");
    printf("     而且换一份输入再测一遍：①② 对输入不敏感，③ 敏感得吓人（第 7 幕）。\n");
    return 0;
}
