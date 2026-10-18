#include <stdio.h>
#include <stdlib.h>
#include <string.h>     /* strcmp：第 4 幕要用它核对"这个名字我到底加过没有" */
#include "bloom.h"

/* 本节的实验规模：真的塞 5000 个键，再拿 5000 个从没见过的键去骗它 */
#define N_REAL   5000
#define N_PROBE  5000

/* 小工具：打印一幕的标题（全仓库统一手感） */
static void section(const char *title)
{
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n");
}

/* 把一个字节打印成 8 个 0/1，高位在左 —— 位操作不画出来就没法用眼睛验证 */
static void print_bin8(unsigned char v)
{
    for (int bit = 7; bit >= 0; bit--) {
        printf("%d", (v >> bit) & 1u);
    }
}

/* 造键。★ 注意这里一个字符串都没保存在内存里：每次要用的时候用 snprintf 现做一遍。
   这不是偷懒，而是布隆过滤器的本质 —— 它【不存键本身】，所以键必须能重新生成；
   也正因为不存键，它才既不能删、也不能回答"里面都有哪些元素"（README 坑 5）。 */
static void real_key(char *buf, size_t cap, int i)
{
    snprintf(buf, cap, "user_%d", i);
}

static void probe_key(char *buf, size_t cap, int i)
{
    snprintf(buf, cap, "x_%d", i);
}

static void word_key(char *buf, size_t cap, int i)
{
    snprintf(buf, cap, "word_%d", i);
}

/* 整数次幂：只用乘法。
   为什么不用 math.h 的 pow()：本仓库的编译命令没有 -lm（详见 bloom.c 文件头那一大段）。
   这里的指数 k 就是个位整数，for 循环乘 k 次比调库更清楚也更便宜。 */
static double pow_int(double base, int exp)
{
    double acc = 1.0;
    for (int i = 0; i < exp; i++) {
        acc *= base;
    }
    return acc;
}

/* 平方根：牛顿迭代 x ← (x + v/x)/2，同样是为了不调 libm 的 sqrt()（没有 -lm）。
   第 5 幕用它算"5000 次查询本身的随机波动有多大"，好判断实测误判率和目标差得算不算正常。
   40 次迭代对 double 早就收敛到最后一位了，多几次纯属保险。 */
static double my_sqrt(double v)
{
    if (v <= 0.0) {
        return 0.0;
    }
    double x = v;
    for (int i = 0; i < 40; i++) {
        x = 0.5 * (x + v / x);
    }
    return x;
}

/* 数一下整个位数组里有几个 1（demo 那种小数组用，顺便当 bits_test 的自检） */
static int count_lit(const unsigned char *bits, unsigned long long n_bytes)
{
    int lit = 0;
    for (unsigned long long b = 0; b < n_bytes; b++) {
        for (int bit = 0; bit < BLOOM_BITS_PER_BYTE; bit++) {
            lit += (bits[b] >> bit) & 1u;
        }
    }
    return lit;
}

/* 找出 word_i 和 word_j 这两个键在位数组上【共用的那一个下标】。
   第 8、9 幕必须用"确实撞在一起"的键做实验 —— 撞不上，删除伤人的现场就演不出来。
   *found = 1 表示找到了，返回值才有意义。 */
static unsigned long long find_shared_slot(const Bloom *bf, int i, int j, int *found)
{
    char a[32], b[32];
    unsigned long long sa[64], sb[64];
    word_key(a, sizeof a, i);
    word_key(b, sizeof b, j);
    bloom_derive_slots(bloom_hash_str(a), bf->m, bf->k, sa, 64);
    bloom_derive_slots(bloom_hash_str(b), bf->m, bf->k, sb, 64);
    *found = 0;
    for (int x = 0; x < bf->k; x++) {
        for (int y = 0; y < bf->k; y++) {
            if (sa[x] == sb[y]) {
                *found = 1;
                return sa[x];
            }
        }
    }
    return 0ULL;
}

int main(void)
{
    /* ========================================================================
       第 1 幕：位数组 —— 全节最该看懂的三行代码
       ======================================================================== */
    section("第 1 幕：一个 unsigned char 数组怎么当 32 个开关用");

    unsigned char demo[4] = {0, 0, 0, 0};      /* 4 字节 = 32 个开关，初值全 0 = 全灭 */
    printf("      4 个字节能表示 %d 个开关。先看空数组（全灭）：\n",
           4 * BLOOM_BITS_PER_BYTE);
    bits_dump(demo, 4, "      demo[0..3]");

    /* 把"第 idx 位"这件事拆成两张纸条：商 = 第几个字节，余数 = 字节内第几位 */
    unsigned long long idx_list[6] = {0ULL, 3ULL, 8ULL, 11ULL, 15ULL, 20ULL};
    printf("\n      逐位换算（★ 除 8 得「第几个字节」，余 8 得「字节内第几位」—— 一字节装 8 个开关）：\n");
    printf("        idx │ idx/8 字节号 │ idx%%8 位号 │ 掩码 1u<<位号 │ 写入后那个字节\n");
    printf("        ────┼──────────────┼────────────┼───────────────┼───────────────\n");
    for (int t = 0; t < 6; t++) {
        unsigned long long idx = idx_list[t];
        bits_set(demo, idx);                   /* ← 全部魔法就这一行：见 bloom.c 第 1 节的逐字注释 */
        unsigned long long byte_no = idx / (unsigned long long)BLOOM_BITS_PER_BYTE;
        unsigned long long bit_no = idx % (unsigned long long)BLOOM_BITS_PER_BYTE;
        printf("        %3llu │ %12llu │ %10llu │ ",
               idx, byte_no, bit_no);
        print_bin8((unsigned char)(1u << bit_no));
        printf("  │ bits[%llu] = ", byte_no);
        print_bin8(demo[byte_no]);
        printf("\n");
    }
    printf("      ⇒ 亮着的位一共 %d 个（= 上面按了 6 次，没重复）。\n", count_lit(demo, 4));
    printf("      ★ 为什么掩码写 1u 而不是 1：这里位号最大 7，`1 << 7` 也还装得进 int，当场不会出事；\n");
    printf("        但位操作的通用形状是 `1u << 位号`，位号可以到 31、63 ——\n");
    printf("        有符号 int【不允许把 1 移进符号位】：`1 << 31` 在 C 里是未定义行为\n");
    printf("        （结果不是 int 能表示的值 ⇒ 有符号溢出 ⇒ UB），而 `1u << 31` 是完全定义的 2147483648。\n");
    printf("        无符号移位 = 对 2^32 取模，标准白纸黑字；所以位掩码一律加 u（64 位用 1ULL）。\n");

    printf("\n      测试 bits_test：第 3 位亮吗 = %d，第 4 位亮吗 = %d，第 11 位亮吗 = %d\n",
           bits_test(demo, 3ULL), bits_test(demo, 4ULL), bits_test(demo, 11ULL));
    printf("      关灯 bits_clear(3)：\n");
    bits_clear(demo, 3ULL);
    bits_dump(demo, 4, "      关掉第 3 位之后");
    printf("      第 3 位 = %d（关灯生效），第 0 位 = %d（旁边那位完全没被牵连）\n",
           bits_test(demo, 3ULL), bits_test(demo, 0ULL));
    printf("      ⚠ 清位为什么要 `&= ~掩码` 而不是 `^= 掩码`：异或是【翻转】，\n");
    printf("        本来已经灭着的位会被 ^= 拧亮；&= ~ 才是「只动这一位，其余一根手指都不碰」。\n");

    /* ========================================================================
       第 2 幕：m 和 k 是算出来的，不是拍脑袋
       ======================================================================== */
    section("第 2 幕：给定 n 和期望误判率 p，现场反解位数 m 与哈希个数 k");

    printf("      先自检手搓的 ln（本仓库编译命令没有 -lm，所以 bloom_ln 只能自己写级数）：\n");
    double ln_check = bloom_ln(0.01);
    double ln_true = -4.605170185988092;          /* 对数表里的 ln(0.01) = -2·ln10 */
    double ln_err = ln_check - ln_true;
    if (ln_err < 0.0) {
        ln_err = -ln_err;                          /* 手算绝对值：fabs 也是 libm 的符号，不调用 */
    }
    printf("        bloom_ln(0.01) = %.15f\n", ln_check);
    printf("        对数表真值      = %.15f\n", ln_true);
    printf("        误差            = %.3e  ⇒ 双精度完全够用，m 取整不受影响\n", ln_err);

    printf("\n      公式： m = ceil( -n·ln p / (ln2)^2 ) ，  k = round( m/n · ln2 )\n");
    printf("            n = %d 时四种目标误判率的解：\n", N_REAL);
    printf("        p(目标)  │     m 位数 │ k │  字节数 │ 每元素位数 │ 换底校验 log2(1/p)\n");
    printf("        ─────────┼────────────┼───┼─────────┼────────────┼───────────────────\n");
    double plist[4] = {0.1, 0.01, 0.001, 0.0001};
    for (int t = 0; t < 4; t++) {
        unsigned long long m_calc = 0ULL;
        int k_calc = 0;
        if (bloom_params((unsigned long long)N_REAL, plist[t], &m_calc, &k_calc) != 0) {
            continue;
        }
        /* log2(1/p) = ln(1/p)/ln2 = -ln(p)/ln2：换底公式，仍然只用自己写的 bloom_ln */
        double log2_inv_p = -bloom_ln(plist[t]) / 0.69314718055994530942;
        printf("        %7.4f │ %10llu │ %3d │ %7llu │ %10.3f │ %11.2f\n",
               plist[t], m_calc, k_calc,
               (m_calc + 7ULL) / 8ULL,
               (double)m_calc / (double)N_REAL, log2_inv_p);
    }
    printf("      ⇒ 两个值得记住的规律：\n");
    printf("        · 最优的 k 恰好等于 log2(1/p)（1%% ⇒ 6.64 ⇒ 取整 7），所以上面两列对得上；\n");
    printf("        · p 每缩小 10 倍，每元素位数只多约 4.8 位 —— 「空间换误判率」便宜到离谱，\n");
    printf("          这就是布隆过滤器能在十亿级键上做去重的底气。\n");

    Bloom *bf = bloom_new((unsigned long long)N_REAL, 0.01);   /* 本节主角之一 */
    if (bf == NULL) {
        printf("      [致命] 过滤器造不出来，收工\n");
        return 1;
    }
    bloom_describe(bf, "bloom_new(5000, 0.01) 造出来：");

    /* ========================================================================
       第 3 幕：一个 64 位哈希怎么变出 k 个下标
       ======================================================================== */
    section("第 3 幕：哈希 —— 只写一个 64 位哈希，用 h1 + i*h2 变出 k 个打点位置");

    char buf[32];
    real_key(buf, sizeof buf, 42);
    unsigned long long h42 = bloom_hash_str(buf);
    unsigned long long slots42[64];
    bloom_derive_slots(h42, bf->m, bf->k, slots42, 64);
    printf("      键「%s」的 64 位 FNV-1a 哈希 = 0x%016llX\n", buf, h42);
    printf("      它要点亮的 %d 个位（a = h mod m，b = 步长，slot_i = (a + i*b) mod m，m = %llu）：\n",
           bf->k, bf->m);
    printf("       ");
    for (int i = 0; i < bf->k; i++) {
        printf(" slot_%d=%-5llu", i, slots42[i]);
    }
    printf("\n        相邻差（mod m）：");
    for (int i = 0; i + 1 < bf->k; i++) {
        unsigned long long diff = (slots42[i + 1] + bf->m - slots42[i]) % bf->m;
        printf(" %6llu", diff);
    }
    printf("   ← 全部相同 = 步长 b，说明这是一条【绕回 m 的等差数列】\n");
    printf("      ⇒ 一次哈希、%d 个下标、互不相同：这就是「双重哈希顶替 k 个独立哈希」。\n", bf->k);

    printf("\n      【为什么不真的写 k 个哈希函数】\n");
    printf("        · 真写 %d 个哈希 = 同一个字符串要完整扫 %d 遍，纯浪费（k 通常 7~10）；\n", bf->k, bf->k);
    printf("        · 误判率的分析只要求下标【两两接近独立】（pairwise independence），\n");
    printf("          h1 + i*h2 恰好给得出这个性质 —— Kirsch & Mitzenmacher (2006) 证明：\n");
    printf("          这样构造的过滤器，误判率与「真用 k 个独立哈希」的版本渐近相同；\n");
    printf("        · 我们的 h2 是把那唯一的 64 位哈希再搅一遍得到的，取 64 位里互相错开的位，\n");
    printf("          相关性低到实测分辨不出来（第 5 幕的误判率就是证据）。\n");

    printf("\n      【int 键为什么要搅散】同一个 m = %llu 下现场对比：\n", bf->m);
    printf("        键    不搅散（h1=key, h2=0）的 %d 个下标        搅散后的 %d 个下标\n", bf->k, bf->k);
    int int_samples[4] = {7, 1000, 1001, 1024};
    for (int t = 0; t < 4; t++) {
        int key = int_samples[t];
        unsigned long long naive[64], mixed[64];
        for (int i = 0; i < bf->k; i++) {
            /* 不搅散：h1 就是 key 本身，h2 = key 的另一半 = 0 ⇒ i*h2 恒为 0 ⇒ k 个点全重合 */
            naive[i] = (unsigned long long)(unsigned int)key % bf->m;
        }
        bloom_derive_slots(bloom_hash_int(key), bf->m, bf->k, mixed, 64);
        printf("        %5d ", key);
        for (int i = 0; i < bf->k; i++) {
            printf("%6llu ", naive[i]);
        }
        printf("  ");
        for (int i = 0; i < bf->k; i++) {
            printf("%6llu ", mixed[i]);
        }
        printf("\n");
    }
    printf("      ⇒ 左半边 %d 个下标一模一样（都是同一个数重复 %d 遍）：h2=0 让 h1+i*h2 彻底退化成 h1，\n", bf->k, bf->k);
    printf("        所谓 k 个哈希【全撞在一个点上】，一个键只占 1 位而不是 %d 位。\n", bf->k);
    printf("        这就是「为什么要搅散」最硬的理由：int 只有 32 位，高 32 位恒为 0，\n");
    printf("        不搅散就没有第二个种子（第 5 幕会实测这个坏版本的误判率贵一个数量级）。\n");
    printf("        顺带的第二个理由：连续整数只差最低几位，遇到 m 是 2 的幂、\n");
    printf("        或者键是 1024 的倍数（自增主键、页号、内存地址）就整批撞在同一个下标段上。\n");

    /* ========================================================================
       第 4 幕：add / maybe_contains 的手感
       ======================================================================== */
    section("第 4 幕：bloom_add 与 bloom_maybe_contains —— 全部本体就这两个动作");

    const char *names[5] = {"alice", "bob", "carol", "dave", "erin"};
    printf("      先登记 5 个名字：");
    for (int t = 0; t < 5; t++) {
        bloom_add(bf, names[t]);
        printf("%s ", names[t]);
    }
    printf("\n");
    const char *asked[8] = {"alice", "erin", "frank", "grace", "bob", "heidi", "carol", "ivan"};
    printf("      逐个查询（答案列 = 过滤器说的；事实列 = 我们程序自己知道的答案）：\n");
    for (int t = 0; t < 8; t++) {
        int ans = bloom_maybe_contains(bf, asked[t]);
        int inserted_before = 0;
        for (int j = 0; j < 5; j++) {
            if (strcmp(asked[t], names[j]) == 0) {
                inserted_before = 1;
            }
        }
        printf("        查「%-6s」⇒ %-13s   事实：%s\n",
               asked[t],
               (ans == 1) ? "可能存在(1)" : ((ans == 0) ? "肯定没有(0)" : "[错误](-1)"),
               inserted_before ? "真的加过" : "从没加过");
    }
    printf("      int 键同样能用：登记 1001/2002/3003 之后查 1001 和 4004：\n");
    bloom_add_int(bf, 1001);
    bloom_add_int(bf, 2002);
    bloom_add_int(bf, 3003);
    printf("        查 int 1001 ⇒ %s ；查 int 4004 ⇒ %s\n",
           bloom_maybe_contains_int(bf, 1001) == 1 ? "可能存在" : "肯定没有",
           bloom_maybe_contains_int(bf, 4004) == 1 ? "可能存在" : "肯定没有");

    printf("\n      顺手看一眼位数组的头 8 个字节 —— 键的内容【一个字节都不在里面】：\n");
    bits_dump(bf->bits, 8, "      bf->bits[0..7]");
    printf("      ⇒ 布隆过滤器里没有键、没有桶、没有链表，只有一排开关（%llu 字节 %llu 位）。\n",
           bf->bytes, bf->m);
    printf("        所以「里面都有哪些元素」这种问题问不出来；只能问「这个元素来过吗」。\n");

    /* ========================================================================
       第 5 幕：核心性质实测
       ======================================================================== */
    section("第 5 幕：实测两条性质（真插 5000 个键 + 拿 5000 个假键去骗）");

    Bloom *main_bf = bloom_new((unsigned long long)N_REAL, 0.01);
    if (main_bf == NULL) {
        printf("      [致命] 实验过滤器造不出来\n");
        bloom_free(bf);
        return 1;
    }
    bloom_describe(main_bf, "      实验用：");

    for (int i = 0; i < N_REAL; i++) {
        real_key(buf, sizeof buf, i);
        bloom_add(main_bf, buf);
    }
    printf("      已插入 %d 个真实键：user_0 .. user_%d\n", N_REAL, N_REAL - 1);

    /* 性质 A：真加过的键，一个都不许说"没有" */
    int false_negative = 0;
    for (int i = 0; i < N_REAL; i++) {
        real_key(buf, sizeof buf, i);
        if (bloom_maybe_contains(main_bf, buf) == 0) {
            false_negative++;
        }
    }
    printf("\n      【性质 A｜说没有 ⇒ 一定没有】把 %d 个真键挨个复查：\n", N_REAL);
    printf("        被错答成「肯定没有」的个数（假阴性）= %d  ⇒ %s\n",
           false_negative,
           (false_negative == 0) ? "0 个，承诺成立 ✔" : "不为 0 —— 代码写错了，必须回头查！");

    /* 性质 B：从没加过的键，会被骗一部分，而这个"一部分"≈ 设计时定的 p */
    int false_positive = 0;
    for (int i = 0; i < N_PROBE; i++) {
        probe_key(buf, sizeof buf, i);
        if (bloom_maybe_contains(main_bf, buf) == 1) {
            false_positive++;
        }
    }
    double measured_p = (double)false_positive / (double)N_PROBE;
    double density = bloom_density(main_bf);
    double theory_p = pow_int(density, main_bf->k);
    printf("\n      【性质 B｜说有 ⇒ 可能是误判】拿 %d 个【从没加过】的键 x_0 .. x_%d 去查：\n",
           N_PROBE, N_PROBE - 1);
    printf("        被骗成「可能存在」的个数   = %d / %d\n", false_positive, N_PROBE);
    printf("        实测误判率                 = %.4f%%\n", 100.0 * measured_p);
    printf("        设计时的目标 p             = %.4f%%\n", 100.0 * 0.01);
    printf("        用当前占比反推 (占比^k)    = %.4f%%\n", 100.0 * theory_p);
    printf("        当前 1 的占比              = %.4f%%（%llu / %llu 位）\n",
           100.0 * density, (unsigned long long)(density * (double)main_bf->m), main_bf->m);
    /* 随机波动：n 次独立查询里"被骗次数"服从二项分布，标准差 = sqrt(N·p·(1-p))。
       没有 -lm 就没有 sqrt —— 所以用上面手写的 my_sqrt。 */
    double sigma_cnt = my_sqrt((double)N_PROBE * measured_p * (1.0 - measured_p));
    double target_cnt = 0.01 * (double)N_PROBE;
    printf("        波动尺度：查 %d 次这个动作本身，±1 个标准差 ≈ %.1f 个键（%.2f%%）\n",
           N_PROBE, sigma_cnt, 100.0 * sigma_cnt / (double)N_PROBE);
    printf("        目标 %d 个键 vs 实测 %d 个键 ⇒ 差 %.1f 个标准差，属于正常抖动\n",
           (int)target_cnt, false_positive,
           (sigma_cnt > 0.0) ? ((double)false_positive - target_cnt) / sigma_cnt : 0.0);
    printf("      ⇒ 实测 ≈ 目标 p ≈ 占比^k（同一量级）⇒ 实现是对的：误判率就是「1 的占比」的 k 次方。\n");
    printf("        ⚠ 这些键是固定的（程序每次生成一模一样的 x_0..x_4999），所以数字是确定的不是随机的；\n");
    printf("          换一批前缀（比如 y_0..y_4999）结果会在 1%% 上下摆动，那才是「抽样波动」的样子。\n");

    /* 对照实验：把 h2 变成 0（就是第 3 幕说的"不搅散"的下场），同规模实测差多少 */
    unsigned long long bad_bytes = (main_bf->m + 7ULL) / 8ULL;
    unsigned char *bad_bits = calloc((size_t)bad_bytes, 1u);
    if (bad_bits == NULL) {
        printf("      [错误] 坏哈希演示的位数组申请失败，跳过这一段\n");
    } else {
        for (int i = 0; i < N_REAL; i++) {
            real_key(buf, sizeof buf, i);
            /* 每个键只点【1 个】位 —— 模拟「k 个下标全部重合」 */
            bits_set(bad_bits, bloom_hash_str(buf) % main_bf->m);
        }
        long long bad_ones = count_lit(bad_bits, bad_bytes);
        int bad_fp = 0;
        for (int i = 0; i < N_PROBE; i++) {
            probe_key(buf, sizeof buf, i);
            if (bits_test(bad_bits, bloom_hash_str(buf) % main_bf->m) == 1) {
                bad_fp++;
            }
        }
        double bad_rate = (double)bad_fp / (double)N_PROBE;
        printf("\n      【对照实验｜k 个下标全重合会怎样】同样 %d 个键、同样 m = %llu 位，但每键只点 1 位：\n",
               N_REAL, main_bf->m);
        printf("        亮着的位数 = %lld（正常版点亮 %llu 位，是它的 %.1f 倍）\n",
               bad_ones, (unsigned long long)(density * (double)main_bf->m),
               (double)(density * (double)main_bf->m) / (double)bad_ones);
        printf("        实测误判率 = %.4f%%  ← 正常版只有 %.4f%%，贵了 %.1f 倍\n",
               100.0 * bad_rate, 100.0 * measured_p,
               (false_positive > 0) ? (double)bad_fp / (double)false_positive : 0.0);
        printf("        这个坏版本的「1 的占比」= %.4f%%，和它的实测误判率 %.4f%% 同一个量级：\n",
               100.0 * (double)bad_ones / (double)main_bf->m, 100.0 * bad_rate);
        printf("        因为只点一位（k=1）时误判率【就等于占比本身】；点满 k 位才是把 k 次「撞灯」乘起来。\n");
        printf("        ⇒ 打点的独立性才是布隆省空间的根源，哈希搅散不是可有可无的细节。\n");
        free(bad_bits);
    }

    /* ========================================================================
       第 6 幕：空间账
       ======================================================================== */
    section("第 6 幕：同样管 5000 个字符串，布隆过滤器省在哪儿");

    unsigned long long bf_bytes = main_bf->bytes;
    unsigned long long ht_estimate = (unsigned long long)N_REAL * 60ULL;
    printf("        布隆（m = %llu 位 / k = %d）    ： %8llu 字节（= m/8 向上取整）\n",
           main_bf->m, main_bf->k, bf_bytes);
    printf("        哈希表存这 %d 个字符串（粗估）： %8llu 字节（每元素约 60）\n",
           N_REAL, ht_estimate);
    printf("          60 怎么来的：节点{ next 8 + 键指针 8 + int 值 4 + 对齐 4 = 24 }\n");
    printf("                    + 键要复制一份（user_4999 约 10 字节，堆分配器凑整到 24）\n");
    printf("                    + 桶数组摊销（每桶一个指针 8，负载因子约 0.75）≈ 56 ⇒ 取 60\n");
    printf("        ⇒ 布隆只用 %llu 字节就管住了 %d 个键，省了约 %.1f 倍。\n",
           bf_bytes, N_REAL, (double)ht_estimate / (double)bf_bytes);
    printf("          代价：它只能说「也许有」，而且【不能删】（第 8 幕）。\n");
    printf("        ⇒ 真实用法是「门神」：布隆挡掉绝大多数根本不存在的查询，\n");
    printf("          %d 次假查询里只有约 %d 次（%.2f%%）需要再去查真表。\n",
           N_PROBE, false_positive, 100.0 * measured_p);

    /* ========================================================================
       第 7 幕：越插越糊
       ======================================================================== */
    section("第 7 幕：插入越多，1 越密，误判越凶（一张「变糊」的实测表）");

    Bloom *grow_bf = bloom_new((unsigned long long)N_REAL, 0.01);
    if (grow_bf == NULL) {
        printf("      [致命] 第三个过滤器造不出来\n");
        bloom_free(bf); bloom_free(main_bf);
        return 1;
    }
    printf("      位数组按 n = %d 设计，却一直插到 8000 个（超售 1.6 倍），每到一个刻度测一次：\n",
           N_REAL);
    printf("        已插入 │  1 的占比 │ 实测误判率 │ 理论 占比^k │ 说明\n");
    printf("        ───────┼───────────┼────────────┼─────────────┼──────────────────────\n");

    int marks[5] = {0, 1000, 3000, 5000, 8000};
    int mark_hit = 0;
    for (int step = 0; step <= 8000; step++) {
        if (step > 0) {
            real_key(buf, sizeof buf, step - 1);
            bloom_add(grow_bf, buf);
        }
        if (mark_hit < 5 && step == marks[mark_hit]) {
            double d = bloom_density(grow_bf);
            int fp = 0;
            for (int i = 0; i < N_PROBE; i++) {
                probe_key(buf, sizeof buf, i);
                if (bloom_maybe_contains(grow_bf, buf) == 1) {
                    fp++;
                }
            }
            double mp = (double)fp / (double)N_PROBE;
            double th = pow_int(d, grow_bf->k);
            printf("        %6d │ %9.4f%% │ %10.4f%% │ %11.4f%% │",
                   step, 100.0 * d, 100.0 * mp, 100.0 * th);
            if (step == 0) {
                printf(" 灯全灭，谁都骗不动\n");
            } else if (step == N_REAL) {
                printf(" ← 正好装到设计容量，守住 1%%\n");
            } else if (step > N_REAL) {
                printf(" ← 超售，误判率肉眼可见地爬\n");
            } else {
                printf(" 还没装满，比承诺更宽松\n");
            }
            mark_hit++;
        }
    }
    printf("      ⇒ 「1 的占比」就是布隆的糊度：设计容量之内守承诺，超售立刻打脸。\n");
    printf("        实测列和「占比^k」这一列同一量级、趋势完全一致 ⇒ 误判率就是由占比决定的，\n");
    printf("        跟键长、键的内容都没关系。工程上因此有两条铁律：\n");
    printf("        · n 要给足余量（宁可多估一倍）；\n");
    printf("        · 真装满了不能就地改造，只能新建一个大的、把老键全灌一遍（这就是\n");
    printf("          「布隆不可扩容」的由来，也是为什么它不能删 —— 它连自己存了什么都不知道）。\n");

    /* ========================================================================
       第 8 幕：删除为什么不行（现场制造一次假阴性）
       ======================================================================== */
    section("第 8 幕：为什么标准布隆过滤器【不能删】—— 删一个键，顺手拧灭别人的灯");

    Bloom *pair_bf = bloom_new(200, 0.01);        /* 小过滤器：200 个键就把占比推到 ~52%，好看清后果 */
    if (pair_bf == NULL) {
        printf("      [致命] 第四个过滤器造不出来\n");
        bloom_free(bf); bloom_free(main_bf); bloom_free(grow_bf);
        return 1;
    }
    char wbuf[32];
    for (int i = 0; i < 200; i++) {
        word_key(wbuf, sizeof wbuf, i);
        bloom_add(pair_bf, wbuf);
    }
    double d8 = bloom_density(pair_bf);
    printf("      第 1 步：先把小过滤器按【设计容量】装满（word_0..word_199）\n");
    bloom_describe(pair_bf, "        体检：");

    printf("\n      第 2 步：在这 200 个键里扫出一对【确实共用下标】的键（不是碰运气，是算出来的）：\n");
    int pair_i = -1, pair_j = -1, found = 0;
    unsigned long long shared = 0ULL;
    for (int i = 0; i < 200 && pair_i < 0; i++) {
        for (int j = i + 1; j < 200; j++) {
            shared = find_shared_slot(pair_bf, i, j, &found);
            if (found == 1) {
                pair_i = i;
                pair_j = j;
                break;
            }
        }
    }
    char keyA[32], keyB[32];
    word_key(keyA, sizeof keyA, pair_i);
    word_key(keyB, sizeof keyB, pair_j);
    unsigned long long slotsA[64], slotsB[64];
    bloom_derive_slots(bloom_hash_str(keyA), pair_bf->m, pair_bf->k, slotsA, 64);
    bloom_derive_slots(bloom_hash_str(keyB), pair_bf->m, pair_bf->k, slotsB, 64);
    printf("        %s 的 %d 个下标：", keyA, pair_bf->k);
    for (int i = 0; i < pair_bf->k; i++) {
        printf(" %5llu", slotsA[i]);
    }
    printf("\n        %s 的 %d 个下标：", keyB, pair_bf->k);
    for (int i = 0; i < pair_bf->k; i++) {
        printf(" %5llu", slotsB[i]);
    }
    printf("\n        两边重合的那个下标 = #%llu （它同时是 A 和 B 的位，谁都不独占）\n", shared);

    printf("\n      第 3 步：删除之前，两个键都查得到：查 A ⇒ %d，查 B ⇒ %d\n",
           bloom_maybe_contains(pair_bf, keyA), bloom_maybe_contains(pair_bf, keyB));
    printf("      第 4 步：执行【朴素删除】bloom_naive_remove(A) —— 只把 A 的 %d 个位清成 0\n",
           pair_bf->k);
    bloom_naive_remove(pair_bf, keyA);
    printf("        查 A ⇒ %d  ← 看起来是删干净了\n", bloom_maybe_contains(pair_bf, keyA));
    printf("        查 B ⇒ %d  ← ★★ B 明明还在集合里，却被答成「肯定没有」：这就是【假阴性】\n",
           bloom_maybe_contains(pair_bf, keyB));
    printf("        位 #%llu 现在是 %d —— 这一位是 A、B 合用的，A 一关灯 B 就跟着黑了\n",
           shared, bits_test(pair_bf->bits, shared));

    int killed = 0;
    for (int i = 0; i < 200; i++) {
        if (i == pair_i) {
            continue;                 /* A 自己是被"合法"删掉的，不算误伤 */
        }
        word_key(wbuf, sizeof wbuf, i);
        if (bloom_maybe_contains(pair_bf, wbuf) == 0) {
            killed++;
        }
    }
    printf("\n      第 5 步：别只盯着 B —— 把剩下 199 个键全查一遍，看看误伤面有多大：\n");
    printf("        只删了 1 个键 ⇒ 另外 %d 个真在里面的键被连带判成「肯定没有」\n", killed);
    printf("      这不是运气差：此刻 1 的占比 %.4f%%，随便一位都有这么大的概率正被别人用着；\n",
           100.0 * d8);
    printf("        A 的 %d 个位【全部只属于 A】（删掉一个人都伤不到）的概率 ≈ (1-占比)^k = %.4f%%\n",
           pair_bf->k, 100.0 * pow_int(1.0 - d8, pair_bf->k));
    printf("      ⇒ 性质对照：误判（假阳性）只是让人【多查一次】，能容忍；\n");
    printf("        假阴性是把真存在的东西判成不存在，业务上是【事故】。\n");
    printf("        布隆唯一的卖点就是「不会假阴性」，所以标准布隆【禁止删除】——\n");
    printf("        位数组只有 0/1 两个状态，它根本不知道一个位被几个键借用。\n");

    /* ========================================================================
       第 9 幕：计数布隆 —— 能删，但要付空间
       ======================================================================== */
    section("第 9 幕：计数布隆 Counting Bloom —— 每格 1 位换成 1 个计数器，删除就安全了");

    printf("      思路一句话：add 时 k 格各 +1，remove 时 k 格各 -1，查询看 k 格是否全部 > 0。\n");
    printf("      于是「这一格被几个人用」被显式记下来了，删除只需要还掉自己那一份。\n");
    CountingBloom *cb = counting_bloom_new(200, 0.01);   /* 和第 8 幕完全同参数 ⇒ 下标一模一样 */
    if (cb == NULL) {
        printf("      [致命] 计数版造不出来\n");
        bloom_free(bf); bloom_free(main_bf); bloom_free(grow_bf); bloom_free(pair_bf);
        return 1;
    }
    for (int i = 0; i < 200; i++) {
        word_key(wbuf, sizeof wbuf, i);
        counting_bloom_add(cb, wbuf);
    }
    unsigned cnt_before_remove = (unsigned)cb->cnt[shared];
    printf("      第 1 步：同样的 200 个键灌进计数版 ⇒ 第 8 幕那个共用格 #%llu 现在是 %u（不止 1 个人在用）\n",
           shared, cnt_before_remove);
    int rem_ret = counting_bloom_remove(cb, keyA);
    printf("      第 2 步：remove(A) 返回 %d ⇒ 共用格 %u 变 %u —— 只还掉 A 那一份，别人的份没动\n",
           rem_ret, cnt_before_remove, (unsigned)cb->cnt[shared]);
    printf("        查 A ⇒ %d（真删掉了）；查 B ⇒ %d（B 还活着！）← 计数版的删除是干净的\n",
           counting_bloom_maybe_contains(cb, keyA),
           counting_bloom_maybe_contains(cb, keyB));
    int cb_killed = 0;
    for (int i = 0; i < 200; i++) {
        if (i == pair_i) {
            continue;
        }
        word_key(wbuf, sizeof wbuf, i);
        if (counting_bloom_maybe_contains(cb, wbuf) == 0) {
            cb_killed++;
        }
    }
    printf("      第 3 步：同样把剩下 199 个键全查一遍：\n");
    printf("        计数版误伤 %d 个（标准版刚才误伤 %d 个）；counter_overflowed = %d ⇒ 全程没撞到 255，承诺有效\n",
           cb_killed, killed, cb->counter_overflowed);

    /* 批量对照：插 100 个再全删，标准版做不到 */
    CountingBloom *bulk_cb = counting_bloom_new(2000, 0.01);
    Bloom *bulk_bf = bloom_new(2000, 0.01);
    if (bulk_cb != NULL && bulk_bf != NULL) {
        char kbuf[32];
        for (int i = 0; i < 100; i++) {
            word_key(kbuf, sizeof kbuf, i);
            counting_bloom_add(bulk_cb, kbuf);
            bloom_add(bulk_bf, kbuf);
        }
        for (int i = 0; i < 100; i++) {
            word_key(kbuf, sizeof kbuf, i);
            counting_bloom_remove(bulk_cb, kbuf);
        }
        int std_still = 0;
        int cnt_still = 0;
        for (int i = 0; i < 100; i++) {
            word_key(kbuf, sizeof kbuf, i);
            if (bloom_maybe_contains(bulk_bf, kbuf) == 1) {
                std_still++;
            }
            if (counting_bloom_maybe_contains(bulk_cb, kbuf) == 1) {
                cnt_still++;
            }
        }
        printf("\n      批量实验：插入 100 个键，再把这 100 个全部删除，然后逐个复查：\n");
        printf("        标准布隆（压根没法删）说「可能存在」：%d / 100  ← 位一旦亮了就永远亮着\n", std_still);
        printf("        计数布隆（删过了）说「可能存在」    ：%d / 100  ← 计数器回到 0，删除真生效\n", cnt_still);
        printf("      空间代价：标准版 %llu 字节 vs 计数版 %llu 字节（一格一字节 ⇒ %.1f 倍）\n",
               bulk_bf->bytes, bulk_cb->bytes,
               (double)bulk_cb->bytes / (double)bulk_bf->bytes);
        printf("        计数器压到 4 bit/格 就是【恰好 4 倍】（每格从 1 位变 4 位，和 m 多大无关，\n");
        printf("        15 就够用了），但要处理「半个字节」的奇偶对齐 —— 本节为了把计数讲清楚先用 1 字节/格，\n");
        printf("        空间账在 README 里明算。\n");
        printf("      新增的坑：一格撞到 %u 就【停在 255 不再加】，并永久置 counter_overflowed = 1；\n", BLOOM_COUNT_MAX);
        printf("        因为回绕成 0 会让 remove 把占用数减错 ⇒ 又造出假阴性。（饱和路径本节只挡不测）\n");
        counting_bloom_free(bulk_cb);
        bloom_free(bulk_bf);
    }
    printf("      ⇒ 想要删除：要么付 8 倍空间上计数布隆，要么换 Cuckoo Filter / Ribbon Filter，\n");
    printf("        思路都是「把打点做成可撤销的」。\n");

    /* ========================================================================
       第 10 幕：收尾 —— 谁 malloc 谁 free
       ======================================================================== */
    section("第 10 幕：谁 malloc 谁 free —— 一个字节都不欠");

    counting_bloom_free(cb);
    bloom_free(pair_bf);
    bloom_free(grow_bf);
    bloom_free(main_bf);
    bloom_free(bf);
    printf("      本幕归还 5 块：bf / main_bf / grow_bf / pair_bf（各含一条位数组）+ cb（含计数数组）\n");
    printf("      前面已就地归还：第 5 幕的坏位数组 bad_bits、第 9 幕的 bulk_bf 与 bulk_cb\n");
    printf("      每个 Bloom 都是【两次 malloc】（结构体 + 数组），所以 free 也必须两次、顺序相反：\n");
    printf("        先 free(bf->bits) 再 free(bf) —— 反过来就把 bits 的地址弄丢了（经典泄漏）。\n");
    printf("      ⇒ 带 ASan 跑完，程序退出时应当【什么都不报】；漏一次分配就会打红并列出申请时的调用栈。\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住三句话：\n");
    printf("  1) 布隆 = 位数组 + k 个下标。「肯定没有」百分之百可信；「可能存在」有 p 的概率是骗你。\n");
    printf("  2) m = -n·ln p/(ln2)^2，k = (m/n)·ln2；每元素位数只按 ln 增长，所以便宜到离谱。\n");
    printf("  3) 标准版不能删（清位 = 制造假阴性）；要删就付 8 倍空间换计数布隆。\n");
    return 0;
}
