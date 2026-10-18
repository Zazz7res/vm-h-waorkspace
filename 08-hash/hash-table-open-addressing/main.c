#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "oa.h"

/*
 * main.c —— 开放寻址 + 线性探测，分幕演示
 *
 * 这一节的三个必看点：
 *   A. 删除为什么不能直接清零（第 3 幕；--bug 开关才跑错误现场，默认干净退出）
 *   B. 查找必须越过墓碑，只有撞到「真·空槽」才判定不存在（第 2、4 幕，把探测序列打出来）
 *   C. 缓存友好 + 装载率上限必须比拉链法保守（第 7、8 幕，全是实测数字）
 *
 * 所有格号都能手算：h(key) = key，home = key % capacity，撞了就 +1（到尾巴折回 0）。
 * 本目录【自包含】：只 include 标准库和自己的 oa.h，没有引用 hash-table-chaining 的任何文件。
 */

/* 本演示用的缓存线宽度。x86-64 实际就是 64 字节；写成常量方便换平台时改一处。 */
#define CACHE_LINE_BYTES 64

/* 小工具：打印分隔线（每个 main.c 自带一份，叶子目录互不引用） */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/*
 * 和 oa.c 里 oa_home_of 同一条规则，这里单独写一份**只为了打印**。
 * ⚠ 为什么要重复一遍？因为「教学用复算」必须独立：
 *   如果打印直接调实现里的函数，它算出什么我就只能信什么，手算复核的意义就没了。
 *   这份是给我自己当尺子用的。代价：改了 oa.c 的规则，这里要一起改。
 */
static int home_of(int key, int cap)
{
    int r = key % cap;
    if (r < 0) {
        r += cap;         /* 同余平移；绝不取绝对值（-3 和 3 是不同的格子） */
    }
    return r;
}

/* 命中时这个键真实住在哪一格？抄探测序列，最后一格就是（没命中返回 -1） */
static int slot_of_key(const OATable *t, int key)
{
    int path[512];
    const int maxs = (int)(sizeof(path) / sizeof(path[0]));
    int found = 0;
    int steps = oa_probe_path(t, key, path, maxs, &found);
    if (!found) {
        return -1;
    }
    return path[(steps < maxs) ? (steps - 1) : (maxs - 1)];
}

/*
 * 一段连续探测踩了几条缓存线：格号 ÷ 每线格数，再去重计数。
 * 这一步是「缓存友好」能不能量化的关键 —— 步数多不等于线多，
 * 因为连续的几个格号常常落在同一条线上。
 */
static int cache_lines_touched(const int *path, int steps, int slots_per_line)
{
    int lines[512];
    const int capn = (int)(sizeof(lines) / sizeof(lines[0]));
    int nlines = 0;

    for (int i = 0; i < steps; i++) {
        int line = path[i] / slots_per_line;
        int seen = 0;
        for (int j = 0; j < nlines; j++) {
            if (lines[j] == line) {
                seen = 1;
                break;
            }
        }
        if (!seen && nlines < capn) {
            lines[nlines++] = line;
        }
    }
    return nlines;
}

/* 数一遍全表「成功查找」的探测格数与踩到的缓存线数（用 const 探测：不改表、不记账） */
static void measure_full_table_lookups(const OATable *t, int slots_per_line,
                                      double *avg_probes_out, int *max_probes_out,
                                      double *avg_lines_out)
{
    long probes = 0, lines = 0;
    int maxp = 0, cnt = 0;
    int path[512];
    const int maxs = (int)(sizeof(path) / sizeof(path[0]));

    for (int i = 0; i < t->capacity; i++) {
        if (t->slots[i].state != SLOT_USED) {
            continue;
        }
        int found = 0;
        int steps = oa_probe_path(t, t->slots[i].key, path, maxs, &found);
        int shown = (steps < maxs) ? steps : maxs;
        probes += steps;
        lines += cache_lines_touched(path, shown, slots_per_line);
        if (steps > maxp) {
            maxp = steps;
        }
        cnt++;
    }
    if (avg_probes_out != NULL) {
        *avg_probes_out = (cnt > 0) ? (double)probes / (double)cnt : 0.0;
    }
    if (max_probes_out != NULL) {
        *max_probes_out = maxp;
    }
    if (avg_lines_out != NULL) {
        *avg_lines_out = (double)lines / (double)cnt;
    }
}

/*
 * 按「显示列数」补空格，然后打一个左对齐的单元格。
 * 为什么要有这个函数：printf 的 %-Ns 补的是【字节】数，而一个汉字是 3 字节 / 2 列、
 * 「·」是 2 字节 / 1 列、「†」是 3 字节 / 1 列 —— 直接 %-40s 打中文标签，
 * 后面的数字列会跟着字节数一起歪掉（第 8 幕那张表就是重灾区）。
 * 规则：ASCII 1 列；2 字节序列（含「·」U+00B7）1 列；3 字节序列里 U+2020「†」1 列、
 * 其余（CJK）2 列；4 字节（emoji 那些）按 2 列。够本文件用，不追求通用宽字符表。
 */
static void col(const char *s, int width)
{
    const unsigned char *p = (const unsigned char *)s;
    int disp = 0;

    for (; *p != '\0'; p++) {
        if (*p < 0x80u) {
            disp += 1;
        } else if ((*p & 0xF0u) == 0xE0u) {          /* 3 字节开头 */
            int is_dagger = (p[1] == 0x80u && p[2] == 0xA0u);  /* U+2020 † */
            disp += is_dagger ? 1 : 2;
            p += 2;
        } else if ((*p & 0xE0u) == 0xC0u) {          /* 2 字节开头（· 是 U+00B7） */
            disp += 1;
            p += 1;
        } else {                                     /* 4 字节开头 */
            disp += 2;
            p += 3;
        }
    }
    printf("%s", s);
    for (int i = disp; i < width; i++) {
        putchar(' ');
    }
}

/*
 * 拉链法的平均比较次数：头插规则下的【精确算式】（本目录不引用那边的代码，只按同规则算）
 *   桶 b 最终有 L 个元素；插入第 j 个时桶里已有 j-1 个 → 头插把它放到链头，
 *   等全插完，它排在第 (L-j+1) 个位置 → 查它要比较 L-j+1 次。
 *   Σ_{j=1..L} (L-j+1) = L(L+1)/2 → 全表平均 = Σ_b L(L+1)/2 ÷ n。
 * 为什么用精确式而不用教科书那条 1 + α/2：后者是「随机插桶」的期望，
 * 而这里桶的分布是本演示【确定】的那一种（键 0..n-1 均匀落在 m 个桶），
 * 既然能精确算，就不要拿期望值糊过去。
 * ⚠ 它是「按同一条规则独立复算的模型」，不是拉链法那份代码的实测输出 —— 引用别的
 *   目录会直接编译失败（仓库按目录独立编译），所以只能这样对照。
 */
static double chain_avg_comparisons(int n, int m)
{
    if (m <= 0 || n <= 0) {
        return 0.0;
    }
    double total = 0.0;
    for (int b = 0; b < m; b++) {
        int L = n / m + (b < n % m ? 1 : 0);
        total += ((double)L * L + (double)L) / 2.0;
    }
    return total / (double)n;
}

/* Knuth 对线性探测的经典结果（简单均匀哈希假设）：α = 装载率 */
static double theory_probe_success(double alpha)
{
    return 0.5 * (1.0 + 1.0 / (1.0 - alpha));
}
static double theory_probe_unsuccess(double alpha)
{
    return 0.5 * (1.0 + 1.0 / ((1.0 - alpha) * (1.0 - alpha)));
}

/* 清计数器 → 对 n 个键各 get 一次 → 返回平均探测格数（命中数写进 hit_out） */
static double measure_get_avg(OATable *t, const int *keys, int n, int *hit_out)
{
    t->probes = 0;
    int hit = 0;
    for (int i = 0; i < n; i++) {
        int v = 0;
        if (oa_get(t, keys[i], &v)) {
            hit++;
        }
    }
    if (hit_out != NULL) {
        *hit_out = hit;
    }
    return (n > 0) ? (double)t->probes / (double)n : 0.0;
}

/* LCG：固定种子 → 每次跑都给出同一批键（README 里的数字才谈得上「复算」） */
static unsigned g_seed = 20261003u;

static int lcg_residue(int cap)
{
    g_seed = g_seed * 1103515245u + 12345u;
    /* ★ 取高位：LCG 的低几位周期极短（低 1 位只有 2 个值、低 2 位只有 4 个），
     *   拿低位当桶号会把表打得比实际更糟。这里右移 16 位再取模。 */
    return (int)((g_seed >> 16) % (unsigned)cap);
}

/* 造一个「当前表里还没有」的键：余数随机，同余数时换 capacity 的倍数避开重复 */
static int make_fresh_key(OATable *t, int cap)
{
    for (int round = 0; round < 50; round++) {
        int r = lcg_residue(cap);
        for (int mult = 0; mult < 8; mult++) {
            int k = r + cap * mult;
            if (!oa_contains(t, k)) {
                return k;
            }
        }
    }
    return -1;   /* 表几乎满了才会到这里 */
}

/* 造 n 个确定不在表里的键（专门用来量「失败查找」的代价） */
static int make_absent_keys(OATable *t, int *keys, int n, int cap)
{
    int got = 0;
    for (int tries = 0; tries < n * 3 && got < n; tries++) {
        int k = make_fresh_key(t, cap);
        if (k >= 0) {
            keys[got++] = k;
        }
    }
    return got;
}

/*
 * =====================================================================
 * 第 1 幕：元素直接躺在数组里 —— 和拉链法最大的区别
 * =====================================================================
 */
static void act_layout(int show_bug)
{
    section("第 1 幕：所有元素都住在 slots[] 里（对照拉链法的「数组 + 链」）");

    OATable *t = oa_create(8);
    if (t == NULL) {
        printf("  建表失败\n");
        return;
    }
    printf("  删除策略 = %s ← 默认永远是这条正确的路%s\n",
           oa_mode_name(t->del_mode),
           show_bug ? "" : "（想看断链事故：加参数 --bug）");

    printf("\n  刚建好、一个元素都没有（calloc 之后全是 ·，这就是 SLOT_EMPTY 必须是 0 的原因）：\n");
    oa_print(t);

    int keys[] = { 12, 20, 28 };
    printf("\n  逐个 put，每个键的 home 都手算一遍（capacity=8，h(key)=key）：\n");
    for (int i = 0; i < 3; i++) {
        int before = oa_len(t);
        oa_put(t, keys[i], keys[i] * 10);
        printf("    put(%2d, %3d) → home = %2d %% 8 = %d，真实落点 = 槽 %d，size %d → %d\n",
               keys[i], keys[i] * 10, keys[i], home_of(keys[i], 8),
               slot_of_key(t, keys[i]), before, oa_len(t));
    }
    printf("\n  ⚠ 三个键的 home 全是 4，但槽 4/5/6 各住一个 —— 冲突被【水平】摊进数组；\n");
    printf("    拉链法在这里会让 buckets[4] 挂出三个节点。数组不会「装满」，只会「不好插」。\n");

    printf("\n  再 put 一次已存在的键 12（值改成 999）：\n");
    int before = oa_len(t);
    oa_put(t, 12, 999);
    printf("    size %d → %d ← 覆盖值，不是新增元素（漏了这一步，表会被同一个键撑爆）\n",
           before, oa_len(t));

    printf("\n");
    oa_print(t);
    printf("\n");
    oa_stats(t);

    printf("\n  第 4 个键：装载率会正好到 4/8 = 0.500 —— 判据是【严格大于】才动手，所以还塞得下：\n");
    oa_put(t, 4, 44);
    printf("    put(4,44) → home 4 → 槽 4/5/6 全被占 → 落点槽 %d；used=%d，装载率 %.3f\n",
           slot_of_key(t, 4), t->used, oa_load_factor(t));

    printf("\n  第 5 个键：(used+1)/capacity 越过 0.5 → 程序自己搬家（下面两行就是它打的）：\n");
    oa_put(t, 5, 55);
    printf("    put(5,55) → 新 capacity=%d，落点槽 %d；装载率被拉回 %.3f\n",
           t->capacity, slot_of_key(t, 5), oa_load_factor(t));

    printf("\n  搬家前键 12 住在槽 4（home=12%%8=4），现在呢？\n");
    printf("    home = 12 %% 16 = %d，真实槽号 = %d ← 槽号变了，但 get(12) 仍然对：\n",
           home_of(12, t->capacity), slot_of_key(t, 12));
    int v = 0;
    printf("    get(12) = %d（值就是覆盖后写的 999）│ contains(28) = %d │ len = %d │ 墓碑 = %d\n",
           oa_get(t, 12, &v) ? v : -1, oa_contains(t, 28), oa_len(t), oa_tomb_count(t));

    printf("\n  内存账（这才是和拉链法的分野）：\n");
    printf("    Slot = {int key, int value, int state} = %zu 字节；\n", sizeof(Slot));
    printf("    整张表 = OATable %zu 字节 + slots 数组 %d 格 × %zu = %zu 字节，一共 2 次分配。\n",
           sizeof(OATable), t->capacity, sizeof(Slot),
           (size_t)t->capacity * sizeof(Slot));
    printf("    拉链法存同样 5 个键 = 结构体 1 次 + 桶数组 1 次 + 每个节点各 1 次 = 7 次分配。\n");
    printf("    → 开放寻址：元素本体就在数组里，没有 per-element malloc，free 也只有两级。\n");

    oa_free(t);
    printf("\n  已 oa_free（先 free 数组、再 free 结构体）。ASan 会在退出时来查岗。\n");
}

/*
 * =====================================================================
 * 第 2 幕：冲突就是「往后挪一格」，把探测序列打印出来
 * =====================================================================
 */
static void act_probe_path(void)
{
    section("第 2 幕：一次查找走过哪些格子 —— 探测序列就是"
            "「开放寻址」这四个字的实体");

    OATable *t = oa_create(8);
    if (t == NULL) {
        return;
    }
    /* 29/37/45/53 全部 %8 = 5：故意让它们挤在槽 5，然后一格一格往后挪 */
    int keys[] = { 29, 37, 45, 53 };
    printf("  插入 29, 37, 45, 53 —— 四个键的 home 全等于 5：");
    for (int i = 0; i < 4; i++) {
        printf(" %d%%8=%d", keys[i], home_of(keys[i], 8));
    }
    printf("\n");
    for (int i = 0; i < 4; i++) {
        oa_put(t, keys[i], keys[i]);
    }
    printf("\n");
    oa_print(t);

    printf("\n  它们的落点（home 都是 5，却一个比一个靠后，最后一个【折回】数组头部）：\n");
    for (int i = 0; i < 4; i++) {
        printf("    key=%2d  home=%d  实际槽=%d\n", keys[i], home_of(keys[i], t->capacity),
               slot_of_key(t, keys[i]));
    }

    printf("\n  三次查找，把经过的下标逐个打出来：\n");
    oa_print_probe_path(t, 45);     /* 槽 5(别人的键) → 6 → 7 命中 */
    oa_print_probe_path(t, 53);     /* 槽 5 → 6 → 7 → 0（折回）命中 */
    oa_print_probe_path(t, 61);     /* home=5，键不存在：5,6,7,0 全是别人的，1 是 EMPTY → 停 */

    printf("\n  看清这三行的差别：\n");
    printf("    命中 = 撞到一个 USED 且 key 相等；\n");
    printf("    判负 = 撞到一个 EMPTY（真·空槽）。USED 不匹配、墓碑，都【不许】停。\n");
    printf("    get(61) 走了 5 格才敢说「没有」：前 4 格（5、6、7、0）全是别人的键，\n");
    printf("    第 5 格（槽 1）才是真·空槽 —— 这 5 格就是这条探测链的长度。\n");

    printf("\n  另一个对照：home 本来就是空槽，一步就判负：\n");
    oa_print_probe_path(t, 2);      /* home=2，槽 2 是 EMPTY → 1 步 */

    printf("\n  负数键也必须进得了合法格子（和第 08 章拉链法那条坑同源）：\n");
    printf("    -1 %% 8 在 C 里是 %d（负数！），平移后 home = %d\n", -1 % 8, home_of(-1, t->capacity));
    printf("    contains(-1) = %d，它的查找过程：\n", oa_contains(t, -1));
    oa_print_probe_path(t, -1);
    printf("    ⚠ 实现里要是裸写 `key %% capacity` 当地格号，这一步就在读 slots[-1]：\n");
    printf("      堆越界读写，编译器一声不吭，ASan 立刻打死你。\n");

    oa_free(t);
}

/*
 * =====================================================================
 * 第 3 幕：删除的灵魂 —— 墓碑 vs 直接清零
 * =====================================================================
 * 默认运行 = 墓碑（正确）。只有 --bug 才把删除策略切成 OA_DEL_ZERO，
 * 当场把「数据还在、程序说没有」的断链事故跑出来。
 * =====================================================================
 */
static void act_delete_tombstone(int show_bug)
{
    section("第 3 幕：删除那一格该写什么 —— 本节唯一真正的技术决定");

    /* 默认已经是墓碑了，这里【显式】再设一次：
     * 1) 证明 oa_set_delete_mode 这个开关在正常路径上也是可用的（验收要求每个声明都被跑到）；
     * 2) 让下面那行策略名来自表本身，而不是硬编码字符串 —— 打到屏幕上的必须是状态机的真实值。 */
    OATable *probe_only = oa_create(8);
    if (probe_only != NULL) {
        printf("  oa_mode_name(OA_DEL_TOMBSTONE) = %s\n", oa_mode_name(OA_DEL_TOMBSTONE));
        oa_set_delete_mode(probe_only, OA_DEL_TOMBSTONE);
        printf("  新建表自己记的 del_mode 字段 = %s ← 两行一致，说明「默认墓碑」是真的写进了表里，\n",
               oa_mode_name(probe_only->del_mode));
        printf("  不是靠打印时硬编码糊出来的。\n\n");
        oa_free(probe_only);
    }

    /* ---------- 正确路径：墓碑 ---------- */
    OATable *t = oa_create(8);
    if (t == NULL) {
        return;
    }
    oa_put(t, 12, 120);
    oa_put(t, 20, 200);
    oa_put(t, 28, 280);
    printf("  【正确做法：留墓碑】删掉 12 之前：\n");
    oa_print(t);
    printf("  三个键 home 都是 4，所以 20 住槽 5、28 住槽 6 —— 它们的探测路径【都要路过槽 4】。\n\n");

    printf("  oa_delete(12) = %d\n", oa_delete(t, 12));
    oa_print(t);
    printf("  size %d（真少了一个）│ 墓碑 %d │ used %d → 装载率 %.3f（真实密度才 %.3f）\n",
           t->size, oa_tomb_count(t), t->used, oa_load_factor(t), oa_real_load(t));
    printf("  ⚠ 注意装载率【没有下降】：墓碑不装数据，但它挡路，所以照样算进分子。\n\n");

    printf("  删掉 12 之后，20 还在不在？\n");
    int v = 0;
    printf("    get(20) 的值 = %d，命中 = %d\n",
           oa_get(t, 20, &v) ? v : -1, oa_contains(t, 20));
    printf("    它的路径：\n");
    oa_print_probe_path(t, 20);
    printf("    get(28) 的路径：\n");
    oa_print_probe_path(t, 28);
    printf("    get(12) 的路径（刚删掉的那个键，现在必须一路走到空槽才敢说没有）：\n");
    oa_print_probe_path(t, 12);
    printf("  → 墓碑让 20、28 全都活着；12 自己查不到，但查它要多走 3 格（代价就在这）。\n");

    printf("\n  墓碑能被【回收】：插入路径恰好经过它，就直接占住它，不用新格子：\n");
    int tb = oa_tomb_count(t);
    int sz = oa_len(t);
    oa_put(t, 4, 44);          /* home=4 → 槽 4 正是那个墓碑 */
    printf("    put(4,44) → 落点槽 %d，墓碑 %d → %d，size %d → %d，used 仍是 %d\n",
           slot_of_key(t, 4), tb, oa_tomb_count(t), sz, oa_len(t), t->used);
    oa_print(t);
    printf("  → 槽 4 的 † 被 U 4=44 顶掉了：挡路格子数守恒（used 不变），所以不用搬家。\n");
    printf("    但回收只发生在「插入路径经过墓碑」的那种情况 —— 第 5 幕专门演它有多不彻底。\n");

    /* 静态图：错误做法长什么样（默认运行只画不跑，保证正确版本干净退出） */
    printf("\n  要是那一格写成 EMPTY（这就是要避免的做法，此处只画不跑）：\n");
    printf("    ┌────┬────┬────┬────┬──────┬──────┬──────┬────┐\n");
    printf("    │  0 │  1 │  2 │  3 │   4  │   5  │   6  │  7 │\n");
    printf("    ├────┼────┼────┼────┼──────┼──────┼──────┼────┤\n");
    printf("    │ ·  │ ·  │ ·  │ ·  │  ·❌ │ U 20 │ U 28 │ ·  │\n");
    printf("    └────┴────┴────┴────┴──────┴──────┴──────┴────┘\n");
    printf("    get(20)：home=4 → 槽 4 是 EMPTY → 规则说「不存在」→ 当场停下；\n");
    printf("    可 20 就躺在槽 5，值一分没少 —— 程序却回答「没有」。**探测链被剪断。**\n");

    oa_free(t);

    /* ---------- 错误现场：只有 --bug 才真跑 ---------- */
    if (!show_bug) {
        printf("\n  ▶ 想让这个事故被真实跑出来（连「表里出现两个键 20」的后续一起看）：\n");
        printf("      编译后执行：  /tmp/v_oa --bug      （或 --show-bug，两个开关同义）\n");
        printf("    默认运行坚持走墓碑那条正确路，干净退出。\n");
        return;
    }

    printf("\n  ============ --bug 已开启：把删除改成「直接清零」，撞一次事故 ============\n");
    OATable *b = oa_create(8);
    if (b == NULL) {
        return;
    }
    oa_set_delete_mode(b, OA_DEL_ZERO);
    printf("  当前删除策略：%s ← 明知是错的，专门跑来撞一次\n", oa_mode_name(b->del_mode));
    oa_put(b, 12, 120);
    oa_put(b, 20, 200);
    oa_put(b, 28, 280);
    printf("\n  删除前：\n");
    oa_print(b);
    printf("\n  oa_delete(12) = %d（那一格被写成 EMPTY，连 key/value 都清了）\n", oa_delete(b, 12));
    oa_print(b);
    printf("  size %d │ 墓碑 %d │ used %d │ 装载率 %.3f ← 账面漂亮极了，这正是它阴人的地方\n",
           b->size, oa_tomb_count(b), b->used, oa_load_factor(b));

    printf("\n  ❌ 事故 1：数据还在数组里，程序说没有\n");
    printf("    槽 5 的内容：key=%d value=%d state=%d（USED，活得好好的）\n",
           b->slots[5].key, b->slots[5].value, b->slots[5].state);
    v = 0;
    printf("    get(20) 的值 = %d，命中 = %d\n",
           oa_get(b, 20, &v) ? v : -1, oa_contains(b, 20));
    printf("    它的路径：\n");
    oa_print_probe_path(b, 20);
    printf("    对照正确版本（墓碑）：同一次查找是「槽 4(†) → 槽 5(命中)」，2 步就命中。\n");

    printf("\n  ❌ 事故 2：你以为它丢了，于是重新 put 一次 → 表里出现【两个键 20】\n");
    int size_before = oa_len(b);
    oa_put(b, 20, 999);
    printf("    put(20,999) → size %d → %d（它没找到已存在的那个 20，因为路径被剪断了）\n",
           size_before, oa_len(b));
    oa_print(b);
    int v_new = 0;
    int v_ghost = b->slots[5].value;
    printf("    现在 get(20) 读到 %d ← 是新写的那个；槽 5 里那个 %d 成了永远读不到的幽灵\n",
           (oa_get(b, 20, &v_new), v_new), v_ghost);

    printf("\n  ❌ 事故 3：幽灵还占着格子，delete 也拿它没办法\n");
    printf("    delete(20) = %d（只删掉路径上碰到的第一个 20）\n", oa_delete(b, 20));
    int v_after = 0;
    printf("    再 get(20) = 命中 %d，值 %d ← 幽灵又冒出来了\n", oa_get(b, 20, &v_after), v_after);
    printf("    这类 bug 的共性：不崩溃、不报错、内存也不越界，只是【静默丢数据 + 留脏数据】。\n");

    oa_set_delete_mode(b, OA_DEL_TOMBSTONE);   /* 演示完立刻切回正确策略再收摊 */
    oa_free(b);
    printf("\n  演示结束，删除策略已切回 %s，表已 free。\n", oa_mode_name(OA_DEL_TOMBSTONE));
}

/*
 * =====================================================================
 * 第 4 幕：查找必须越过墓碑（长路径实测）
 * =====================================================================
 */
static void act_probe_over_tombstones(void)
{
    section("第 4 幕：查找必须越过墓碑继续探测 —— 撞到的必须是「真·空槽」");

    OATable *t = oa_create(1024);
    if (t == NULL) {
        return;
    }
    /* 五个键的 home 全是 7（7 + 1024*i），所以必定排成槽 7,8,9,10,11 的一串 */
    printf("  插入 5 个 home 全等于 7 的键（7, 1031, 2055, 3079, 4103）：\n");
    int keys[5];
    for (int i = 0; i < 5; i++) {
        keys[i] = 7 + 1024 * i;
        oa_put(t, keys[i], 100 + i);
        printf("    put(%5d) → home=%d，落点槽 %d\n",
               keys[i], home_of(keys[i], t->capacity), slot_of_key(t, keys[i]));
    }
    printf("\n  删掉住在槽 8 的那个 1031，让它变成墓碑：\n");
    printf("  oa_delete(1031) = %d\n", oa_delete(t, 1031));
    oa_print(t);

    printf("\n  ★ 核心演示：槽 8 现在是 †，而 2055 住在槽 9 —— 查找必须【走过去】\n");
    oa_print_probe_path(t, 2055);
    int v = 0;
    printf("    oa_get(2055) 的值 = %d，命中 = %d ← 值取回来了，墓碑没有骗过它\n",
           oa_get(t, 2055, &v) ? v : -1, oa_contains(t, 2055));
    oa_print_probe_path(t, 4103);

    printf("\n  ★ 另一半规则：不存在的那个键，也要一路走到 EMPTY 才敢判负\n");
    int absent = 7 + 1024 * 20;      /* home 也是 7，但从来没插过 */
    printf("    从没插过的 key=%d（home=%d，槽 7~11 全是挡路的）：\n",
           absent, home_of(absent, t->capacity));
    oa_print_probe_path(t, absent);
    printf("    → 它经过 USED 不匹配、经过墓碑，一直走到槽 12 那个 EMPTY 才停下，共 6 步。\n");
    printf("      如果槽 8 当初被清成 EMPTY，槽 8 之后的 2055/3079/4103 会【同时】失联，\n");
    printf("      而且判负看起来依然「正确」—— 错的答案永远长得像对的答案。\n");

    printf("\n  墓碑的真实代价（它不是免费的）：删除前后各把 5 个键查一遍：\n");
    OATable *t2 = oa_create(1024);
    if (t2 != NULL) {
        for (int i = 0; i < 5; i++) {
            oa_put(t2, keys[i], 100 + i);
        }
        int hit = 0;
        double avg_before = measure_get_avg(t2, keys, 5, &hit);
        oa_delete(t2, 1031);
        double avg_after = measure_get_avg(t2, keys, 5, &hit);
        printf("    删除前：5 次查找平均 %.2f 格；把槽 8 那个元素删掉后：平均 %.2f 格\n",
               avg_before, avg_after);
        printf("    → 步数一格没少：删掉的格子照样挡路。这就是「装载率分子必须含墓碑」的实测理由。\n");
        printf("      反过来说，墓碑并没有让查找变【快】，它只是让查找还【正确】。\n");
        oa_free(t2);
    }

    oa_free(t);
}

/*
 * =====================================================================
 * 第 5 幕：装载率的分子必须含墓碑 + 回收不彻底 → 同容量重排蒸发墓碑
 * =====================================================================
 */
static void act_tomb_accounting(void)
{
    section("第 5 幕：墓碑让装载率虚高，而「回收」并不彻底 → 需要同容量重排");

    /* ---- 小表：回收只发生在路径经过墓碑的插入上 ---- */
    OATable *t = oa_create(8);
    if (t == NULL) {
        return;
    }
    printf("  A) 墓碑什么时候被回收？只有插入路径【经过】它的时候。\n");
    oa_put(t, 12, 120);
    oa_put(t, 20, 200);
    oa_put(t, 28, 280);
    oa_delete(t, 12);                 /* 槽 4 变墓碑 */
    printf("    插 12/20/28（home 全 4）后删掉 12 → 槽 4 = †，used=%d，装载率 %.3f\n",
           t->used, oa_load_factor(t));
    oa_print(t);
    printf("    现在 put(5,55)：home = 5 %% 8 = 5 → 探测 5、6、7，路径压根不经过槽 4：\n");
    oa_put(t, 5, 55);                  /* 注意：它落的是 EMPTY 槽 7，不是那块墓碑 */
    printf("    put(5,55) 之后：落点槽 %d、墓碑 %d、size %d、used %d、装载率 %.3f\n",
           slot_of_key(t, 5), oa_tomb_count(t), t->size, t->used, oa_load_factor(t));
    printf("    → 「回收不彻底」现出原形：墓碑躲在探测路径之外，谁也不碰它，它就一直挡路。\n");
    printf("      这次插入用的是一个真·空槽，used 3 → %d，槽 4 那块 † 一格没少；\n", t->used);
    printf("      也就是说 size 涨了、密度涨了，可挡路的格子数【只增不减】。\n");

    printf("\n    再 put(3,33)：home=3 → 槽 3 是 EMPTY，一步就能落定 —— 可 used 已经到 %d，越线了：\n",
           t->used);
    oa_put(t, 3, 33);
    printf("    上面两行是程序自己打的：它选了【同容量重排】而不是翻倍，\n");
    printf("    因为真元素只有 %d 个，塞得下 —— 要消灭的只是挡路的墓碑。\n", t->size - 1);
    oa_print(t);
    printf("    现在：capacity=%d（没变）、size=%d、墓碑=%d、装载率 %.3f、真实密度 %.3f\n",
           t->capacity, t->size, oa_tomb_count(t), oa_load_factor(t), oa_real_load(t));
    printf("    12 早就被删了，可它留下的 † 一直在把 used 顶高；重排之后两个装载率终于相等。\n");
    printf("    重排顺带把 20/28/5 往前挪了（槽 4/5/6），键 3 落在自己的 home —— 一次搬家、两件事。\n");
    oa_free(t);

    /* ---- 极端：整张表全是墓碑 ---- */
    printf("\n  B) 反复「插入又删除」能把整张表填成墓碑。盯住这两个数字：\n");
    OATable *b = oa_create(16);
    if (b == NULL) {
        return;
    }
    int k[8];
    for (int i = 0; i < 8; i++) {
        k[i] = 4 + 16 * i;            /* home 全是 4 → 排成槽 4..11 */
        oa_put(b, k[i], i);
    }
    printf("    插入 8 个 home=4 的键 → 占满槽 4..11，装载率 %.3f\n", oa_load_factor(b));
    for (int i = 0; i < 8; i++) {
        oa_delete(b, k[i]);
    }
    printf("    再把这 8 个全删掉：\n");
    oa_print(b);
    printf("    size = %d（表里一个真元素都没有！）│ 墓碑 = %d │ used = %d\n",
           b->size, oa_tomb_count(b), b->used);
    printf("    装载率 used/capacity = %.3f ← 满表挡路；真实密度 size/capacity = %.3f\n",
           oa_load_factor(b), oa_real_load(b));
    printf("    ⚠ 分子要是写成 size，装载率就变成 %.3f —— 程序以为表空着。"
           "可它真找起来要走几格？\n", oa_real_load(b));
    int path[64];
    const int maxs = (int)(sizeof(path) / sizeof(path[0]));
    const int probe_key = 4 + 16 * 9;      /* home = 4，从没插过的键 */
    int found = 0;
    int steps_before = oa_probe_path(b, probe_key, path, maxs, &found);
    printf("    实测：查 key=%d（home=4）走了 %d 格，路径：\n", probe_key, steps_before);
    oa_print_probe_path(b, probe_key);

    printf("\n    下一次 put 逼出「蒸发墓碑」那一步 —— 故意挑一个路径【碰不到墓碑】的键：\n");
    printf("    put(60,999)：home = 60 %% 16 = 12，槽 12 是 EMPTY，一步就能落定；\n");
    printf("    可 used = %d 已经越线（(used+1)*2 = %d > capacity = %d），而真元素 0 个 —— "
           "接下来两行是程序自己打的：\n", b->used, (b->used + 1) * 2, b->capacity);
    int tombs_before = oa_tomb_count(b);
    oa_put(b, 60, 999);
    oa_print(b);
    int found2 = 0;
    int steps_after = oa_probe_path(b, 4 + 16 * 20, path, maxs, &found2);
    printf("    现在 size=%d 墓碑=%d used=%d 装载率 %.3f；同一种查找的步数从 %d 格降到 %d 格\n",
           b->size, oa_tomb_count(b), b->used, oa_load_factor(b), steps_before, steps_after);
    printf("    → 同容量重排（capacity 仍是 %d）一次把 %d 块墓碑清光，这是开放寻址特有的招式。\n",
           b->capacity, tombs_before);
    printf("    → 注意它【没有】把数组变大：翻倍解决的是「装不下」，重排解决的是「挡路」。\n");
    printf("    ⚠ 但搬家【不能只靠墓碑堆满才做】：阈值看的是 used，所以墓碑一到 0.5 就必须动手。\n");
    oa_free(b);

    /* ---- 为什么阈值 0.5 而不是 0.75 ---- */
    printf("\n  C) 为什么阈值定 0.5，而不是拉链法的 0.75？\n");
    printf("    拉链法看「平均链长 = α」：α=0.75 就是平均比较 1.375 个节点，很温和，\n");
    printf("    而且桶数不够时链还能继续长（α 可以 > 1），表永远不会「无路可走」。\n");
    printf("    开放寻址看的是【连跑长度】：挡路格子连成一片，新键落进串里就把串延长一格，\n");
    printf("    于是所有路过这里的键一起变长 —— α 越接近 1，曲线越陡（第 8 幕量出来）。\n");
    printf("    更要命的是 α=1 时数组里一个 EMPTY 都没有，查找连【停止条件】都失去意义。\n");
}

/*
 * =====================================================================
 * 第 6 幕：扩容 = 全部重新放置（先预测，再验算）
 * =====================================================================
 */
static void act_growth(void)
{
    section("第 6 幕：搬家就是把每个元素重新放一遍 —— 预测 vs 实际");

    OATable *t = oa_create(8);
    if (t == NULL) {
        return;
    }
    int keys[] = { 12, 20, 28, 4 };
    for (int i = 0; i < 4; i++) {
        oa_put(t, keys[i], keys[i] * 10);
    }
    printf("  搬家前（capacity=8，4 个元素，装载率 %.3f —— 已经贴着阈值）：\n", oa_load_factor(t));
    oa_print(t);

    printf("\n");
    oa_rehash_plan(t, 16);

    printf("\n  真搬一次：oa_rehash(t, 16) 返回搬走的元素数 = %d\n", oa_rehash(t, 16));
    printf("  搬家后：\n");
    oa_print(t);

    printf("\n  逐个核对（预测的 home vs 实际落点）：\n");
    printf("      键   预测home  实际槽  一致?\n");
    printf("  ------------------------------------\n");
    for (int i = 0; i < 4; i++) {
        int pred = home_of(keys[i], t->capacity);
        int real = slot_of_key(t, keys[i]);
        printf("    %4d %8d %8d    %s\n", keys[i], pred, real,
               (pred == real) ? "✅ 就住在 home" : "❌ 被同 home 的键挤走了");
    }
    printf("  → 12 和 28 的新 home 都是 12：先重放的占 12，后重放的只能去 13；\n");
    printf("    20 和 4 的新 home 都是 4，同理 4 落到 5。落点是「全表占用状态」的函数。\n");

    printf("\n  正确性检查：四个键全都还在，值也没变：\n");
    for (int i = 0; i < 4; i++) {
        int v = 0;
        printf("    get(%2d) = %3d（命中=%d）│ 住在槽 %d\n", keys[i],
               oa_get(t, keys[i], &v) ? v : -1, oa_contains(t, keys[i]), slot_of_key(t, keys[i]));
    }
    printf("  ⚠ 搬家时忘了用【新 capacity】重算 home，或者只搬一半 ——\n");
    printf("    后果和拉链法那条「rehash 后忘记重算下标」一模一样：静默丢数据。\n");

    oa_free(t);

    /* 摊还：数一数连插 64 个键要搬几趟 */
    printf("\n  摊还成本：从 capacity=8 开始连插 64 个键（下面的搬家记录是程序自己打的）\n");
    OATable *g = oa_create(8);
    if (g == NULL) {
        return;
    }
    int filled = oa_fill_arithmetic(g, 64, 0, 1);
    printf("  插入 %d 个键后：capacity=%d，size=%d，装载率 %.3f\n",
           filled, g->capacity, g->size, oa_load_factor(g));
    printf("  上面每行 [搬家] 的「重新放置 N 个元素」加起来就是复制总次数：\n");
    printf("    4 + 8 + 16 + 32 = %d 次复制 ÷ 64 次插入 = %.2f 次/插入 → 摊下来仍是 O(1)\n",
           4 + 8 + 16 + 32, (double)(4 + 8 + 16 + 32) / 64.0);
    printf("  为什么敢随手扩？因为 capacity 每次【翻倍】：表越大，搬家相对越少，\n");
    printf("    少数几次 O(n) 摊到大量 O(1) 插入上，就是摊还复杂度最干净的实例。\n");
    oa_stats(g);
    oa_free(g);
}

/*
 * =====================================================================
 * 第 7 幕：缓存友好（oa.h 里点名的就是第 7 幕）
 * =====================================================================
 */
static void act_cache(void)
{
    section("第 7 幕：为什么开放寻址对缓存友好 —— 把「探测格数」换算成「缓存线条数」");

    const int slot_sz = (int)sizeof(Slot);
    const int per_line = CACHE_LINE_BYTES / slot_sz;
    printf("  一格 = %d 字节（三个 int：key 4 + value 4 + state 4，没有指针，没有 padding）\n",
           slot_sz);
    printf("  一条缓存线 = %d 字节 → 装 %d 格（还余 %d 字节，凑不满第 %d 格）\n",
           CACHE_LINE_BYTES, per_line, CACHE_LINE_BYTES % slot_sz, per_line + 1);
    printf("  线性探测的下一步是 idx+1 → 连续几格往往就在同一条线上，\n");
    printf("  硬件预取器顺手把后面几格一起搬进 L1，等于免费。\n\n");

    /*
     * 两批键，两种方案各量一遍。为什么要两批：
     *   只给「一人一格」那批 = 自卖自夸；只给「一条长串」那批 = 自己吓自己。
     *   把两头都摆出来，才能看清开放寻址的优势到底来自哪 —— 来自【地址连续】，
     *   而不是来自「步数少」（步数它常常更多）。
     * cbuckets = 这批键在拉链法里会摊进几个桶：塌缩那批全同余 → 只有 1 个桶。
     */
    const int cap = 1024;
    const int n = 128;
    struct CacheCase {
        int base;
        int step;
        int cbuckets;
        const char *name;
    };
    struct CacheCase cs[2] = {
        { 0, 1024,    1, "塌缩：一条 128 长的串" },
        { 0,    8, 1024, "铺开：一人一格" },
    };
    double oa_probes[2] = { 0.0, 0.0 }, oa_lines[2] = { 0.0, 0.0 };
    int oa_max[2] = { 0, 0 };

    for (int b = 0; b < 2; b++) {
        OATable *t = oa_create(cap);
        if (t == NULL) {
            return;
        }
        int filled = oa_fill_arithmetic(t, n, cs[b].base, cs[b].step);

        if (b == 0) {
            /* oa_stats 打的是全表形态 + 连跑统计，两批都打太长，只给第一批 */
            printf("  批 1【%s】灌 %d 个键（键 = 0, 1024, 2048, …）：\n", cs[b].name, filled);
            oa_stats(t);
        }
        measure_full_table_lookups(t, per_line, &oa_probes[b], &oa_max[b], &oa_lines[b]);
        printf("  批 %d【%s】实测：平均探测 %.2f 格（最长 %d 格），平均踩 %.2f 条缓存线\n",
               b + 1, cs[b].name, oa_probes[b], oa_max[b], oa_lines[b]);
        oa_free(t);
    }

    printf("\n  换算的算术：%.2f 格 ÷ 每线 %d 格 ≈ %.2f 条，实测 %.2f 条略高一点 ——\n"
           "  因为探测的起点很少正好落在一条线的开头，头尾各多算一条。\n",
           oa_probes[0], per_line, oa_probes[0] / per_line, oa_lines[0]);

    printf("\n  同一批键、同一种分布，放进拉链法要踩几条线（比较次数按头插规则精确算，\n"
           "  见 chain_avg_comparisons 的注释；⚠ 那是【模型复算】，不是那边代码的实测）：\n");
    for (int b = 0; b < 2; b++) {
        double cmp = chain_avg_comparisons(n, cs[b].cbuckets);
        printf("    批 %d：%d 个键摊进 %d 个桶 → 平均比较 %.2f 个节点；"
               "节点各一次 malloc、地址不相邻 →\n",
               b + 1, n, cs[b].cbuckets, cmp);
        printf("      模型：桶数组 1 条线 + 每比一次 1 条线 = 平均 %.2f 条线"
               "（开放寻址同批 %.2f 条，差 %.1f 倍）\n",
               cmp + 1.0, oa_lines[b], (cmp + 1.0) / oa_lines[b]);
    }

    printf("\n  并排看（【访问次数两边几乎一样，差的是每次访问要不要换线】）：\n");
    printf("    ");
    col("批次", 22); col("方案", 16); col("平均访问次数", 16); col("平均缓存线", 14);
    col("线差", 8);
    printf("\n    ");
    for (int i = 0; i < 66; i++) {
        putchar('-');
    }
    printf("\n");
    for (int b = 0; b < 2; b++) {
        double cmp = chain_avg_comparisons(n, cs[b].cbuckets);
        double cline = cmp + 1.0;
        char c1[32], c2[32], c3[32], c4[32];

        printf("    ");
        col(cs[b].name, 22); col("开放寻址", 16);
        snprintf(c1, sizeof c1, "%.2f", oa_probes[b]);
        snprintf(c2, sizeof c2, "%.2f", oa_lines[b]);
        col(c1, 16); col(c2, 14); col("-", 8);
        printf("\n");
        printf("    ");
        col("", 22); col("拉链法(模型)", 16);
        snprintf(c3, sizeof c3, "%.2f", cmp);
        snprintf(c4, sizeof c4, "%.2f", cline);
        col(c3, 16); col(c4, 14);
        snprintf(c1, sizeof c1, "%.1fx", cline / oa_lines[b]);
        col(c1, 8);
        printf("\n");
    }

    printf("\n  内存账也顺便看一眼（两批用的都是 capacity=%d，一张表就是一个数组）：\n", cap);
    printf("    开放寻址：一格 %d 字节，全是有效数据；整张表 %d 格 = %zu 字节，一次 calloc。\n",
           slot_sz, cap, (size_t)cap * sizeof(Slot));
    printf("    拉链法：节点 = int key + int value + 指针 next = 16 字节，其中 %zu 字节"
           "纯粹是为了「挂链」；\n", sizeof(void *));
    printf("           外加堆管理器给每次 malloc 记的账（glibc 最少 8 字节头 + 16 字节对齐），\n");
    printf("           %d 个元素就是 %d 次 malloc。\n", n, n);
    printf("    元素紧凑 = 缓存和分配器两头都省，这是开放寻址的立身之本。\n");

    printf("\n  ⚠ 诚实边界（这三条别忘）：\n");
    printf("    1) 「平均 %.2f 条线」是【格号 → 缓存线】的算术换算，不是 perf 的 L1 miss 计数器\n"
           "       实测。要量真未命中得用 `perf stat -e L1-dcache-load-misses` 或 "
           "valgrind --tool=cachegrind。\n",
           oa_lines[0]);
    printf("    2) 拉链法的「一跳一线」是最坏假设：刚 malloc 出来的相邻节点地址常常很近，\n");
    printf("       实际差距会比 %.1fx 小 —— 但方向不会反过来，而且表跑久了（堆碎片化）"
           "就回到这个量级。\n",
           (chain_avg_comparisons(n, cs[0].cbuckets) + 1.0) / oa_lines[0]);
    printf("    3) 批 1 是【最坏聚集】那一头：平均 %.2f 格里只有 1 格真是你要的，\n",
           oa_probes[0]);
    printf("       其余 %.2f 格读的都是别人的键 —— 「连续探测」在这里就变成\n",
           oa_probes[0] - 1.0);
    printf("       「一整条线全读完、一个都不是」。它仍然赢在换线少，\n");
    printf("       可这赢面是「数组连续」给的，不是「步数少」给的（第 8 幕量步数）。\n");
}

/*
 * =====================================================================
 * 第 8 幕：primary clustering 实测 + 退化曲线
 * =====================================================================
 */
static void act_clustering(void)
{
    section("第 8 幕：冲突聚集实测 —— 同样 512 个键，只是分布不同，差多少");

    const int cap = 1024;
    const int n = 512;                 /* 512/1024 = 0.500，正好贴着阈值，不触发搬家 */

    /* 一批键：base/step 是插入序列，mbase/mstep 是「同 home 但没插过」的失败查找序列 */
    struct Batch {
        int base;
        int step;
        int mbase;
        int mstep;
        const char *name;
        const char *shape;
    };
    struct Batch bs[3] = {
        {   0,   1, 1024,   1, "顺序 step=1（键 0..511）",
          "▓▓▓▓▓▓▓▓...（一条长串占掉半张表）░░░░░░░░" },
        {   0,  33, 1024,  33, "打散 step=33（互质铺开）",
          "▓░▓░▓░▓░▓░...（每格之间都留空，谁都挡不到谁）" },
        {   7,1024,  7 + 1024 * 512, 1024, "塌缩 step=1024（全同余）",
          "░░░░░░░▓▓▓▓▓▓▓▓▓...（从槽 7 开始的一坨）" },
    };

    printf("  三张表都是 capacity=%d、装 %d 个键（装载率 %.3f），只有【键的分布】不同：\n",
           cap, n, (double)n / (double)cap);

    /* 先把「量」这个过程单独演示一遍。
     * 为什么只演示一次：oa_probe_after_n_gets 会自己往 stdout 打一行汇报，
     * 塞在表格循环里就把表格劈成三截了 —— 排版也是正确性的一部分。 */
    printf("\n  量法演示（批 1：顺序插 %d 个键，再逐个 get）：\n", n);
    OATable *demo = oa_create(cap);
    if (demo == NULL) {
        return;
    }
    int dfilled = oa_fill_arithmetic(demo, n, bs[0].base, bs[0].step);
    long dtotal = oa_probe_after_n_gets(demo, dfilled, bs[0].base, bs[0].step);
    printf("    ↑ 上面那行是 oa_probe_after_n_gets 自己打的：累计 %ld 格 ÷ %d 次 = %.2f 格/次\n",
           dtotal, dfilled, (double)dtotal / (double)dfilled);
    printf("    ⚠ 它只量了【命中】。命中可以很快（每个键都住在自己 home），\n");
    printf("      真正被聚集拖死的是【不命中】的查找 —— 所以下面的表把两列分开量。\n");
    oa_free(demo);

    /* 三批各量一遍，结果先存住，最后统一排版（顺序：插 → 成功 → 失败 → 连跑统计） */
    struct Res {
        double succ;
        double unsucc;
        int longest;
        int runs;
        int filled;
        int mn;
    } res[3];

    for (int b = 0; b < 3; b++) {
        OATable *t = oa_create(cap);
        if (t == NULL) {
            return;
        }
        int filled = oa_fill_arithmetic(t, n, bs[b].base, bs[b].step);

        /* 成功查找：把这批键原样再算一遍（算术序列，不存副本） */
        int keys[512];
        for (int i = 0; i < filled; i++) {
            keys[i] = bs[b].base + i * bs[b].step;
        }
        int hit = 0;
        res[b].succ = measure_get_avg(t, keys, filled, &hit);

        /* 失败查找：一批 home 落在挡路区间里、但确定没插过的键 */
        int miss[512];
        int mn = 0;
        for (int i = 0; i < n && mn < 512; i++) {
            int k = bs[b].mbase + i * bs[b].mstep;
            if (!oa_contains(t, k)) {
                miss[mn++] = k;
            }
        }
        res[b].unsucc = measure_get_avg(t, miss, mn, &hit);

        int longest = 0, cur = 0, runs = 0;
        for (int i = 0; i < t->capacity; i++) {
            if (t->slots[i].state == SLOT_EMPTY) {
                cur = 0;
            } else {
                cur++;
                if (cur > longest) {
                    longest = cur;
                }
                if (cur == 1) {
                    runs++;
                }
            }
        }
        res[b].longest = longest;
        res[b].runs = runs;
        res[b].filled = filled;
        res[b].mn = mn;
        oa_free(t);
    }

    printf("\n  汇总表（同一个装载率、同一个键数，只有分布不同）：\n");
    printf("    ");
    col("批次", 26); col("成功平均格", 14); col("失败平均格", 14);
    col("最长连跑", 12); col("挡路区间", 12);
    printf("\n    ");
    for (int i = 0; i < 76; i++) {
        putchar('-');
    }
    printf("\n");
    for (int b = 0; b < 3; b++) {
        printf("    ");
        col(bs[b].name, 26);
        printf("%-14.2f %-14.2f %-12d %-12d\n",
               res[b].succ, res[b].unsucc, res[b].longest, res[b].runs);
        printf("    ");
        col("形态：", 26);
        printf("%s\n", bs[b].shape);
    }

    printf("    （成功样本：每批 %d 次命中，查的就是刚插进去的那批键本身；\n", n);
    printf("      失败样本：%d / %d / %d 次，都是「home 落在挡路区间里、但确定没插过」的键 ——\n",
           res[0].mn, res[1].mn, res[2].mn);
    printf("      命中可以在串的中途就停下，不命中【必须】一路走到串尾后面的 EMPTY，\n");
    printf("      所以不命中的步数才是聚集真正的体温计。）\n");

    printf("\n  读这三行（数字就是上面那三行，不是我估的）：\n");
    printf("    · 第 1 行：键按顺序插，成功查找【看着】只要 %.2f 格 —— 每个键就住在自己 home。\n",
           res[0].succ);
    printf("      可失败查找平均 %.2f 格：它必须一路走到那条 %d 格长串的尾巴才敢判负。\n",
           res[0].unsucc, res[0].longest);
    printf("      「这就是 primary clustering」：连成一串的格子互相拖累 —— 新键只要落进串里\n");
    printf("      就自动把串延长一格，下一个键更慢，于是越聚越大。\n");
    printf("      同一批键在拉链法里是 %d 条各 1 个节点的链（home 各不相同），失败查找只看\n",
           res[0].filled);
    printf("      自己那条，平均 %.2f 个节点 —— 桶之间互不传染，这是拉链法唯一【赢】的地方。\n",
           chain_avg_comparisons(res[0].filled, cap));
    printf("    · 第 2 行：step 与 capacity 互质 → home 均匀铺满，%d 个键摊成 %d 段、最长 %d 格\n",
           res[1].filled, res[1].runs, res[1].longest);
    printf("      （摊成这样是【生日碰撞】的必然：α=0.5 时总有人撞到同一个格子，\n");
    printf("      但撞到之后不再互相拖累，串长停在 %d 而不是 %d）。成功 %.2f 格、失败 %.2f 格。\n",
           res[1].longest, res[0].longest, res[1].succ, res[1].unsucc);
    printf("      和第 1 行【同样的装载率、同样的键数】：失败平均 %.2f vs %.2f，差 %.1f 倍。\n",
           res[1].unsucc, res[0].unsucc, res[0].unsucc / res[1].unsucc);
    printf("    · 第 3 行：所有键同一个 home（等价于哈希函数彻底坏了 / 键全同余），\n");
    printf("      第 k 个插进去就要走 k 格，平均 (n+1)/2 = %.1f，实测 %.2f —— 字面意义上的单链表，\n",
           (double)(n + 1) / 2.0, res[2].succ);
    printf("      而且它是【一条数组里的链】：每格还是 %d 字节紧挨着 —— 按第 7 幕的换算，\n",
           (int)sizeof(Slot));
    printf("      %.2f 格 ≈ %.2f 条缓存线；同样 %.2f 次比较在拉链法里是散落的 %d 个节点\n",
           res[2].succ, res[2].succ / (double)(CACHE_LINE_BYTES / (int)sizeof(Slot)),
           res[2].succ, res[2].filled);
    printf("      ≈ %.2f 条线（同一个模型）。可见「聚集」和「缓存」是两个维度：\n",
           (double)res[2].succ + 1.0);
    printf("      访问次数两边【一模一样】(%.2f vs %.2f)，差的全是「跳一次换不换地址」。\n",
           res[2].succ, chain_avg_comparisons(res[2].filled, 1));
    printf("    → 结论：线性探测慢，不是因为「格子分散在内存里」，是因为【串会被别人加长】。\n");
    printf("      治它的路子不是换数组，是换探测序列（二次探测 / 双重哈希 / Robin Hood），\n");
    printf("      把「+1」变成「+i²」或「+h2(key)」，串就没法互相传染。\n");

    /* --- 退化曲线：装载率上升，步数怎么涨（键由固定 LCG 生成，可复现） --- */
    printf("\n  装载率与探测步数的关系（capacity=%d；键 = 随机 home + 同 home 时加 capacity 的倍数，\n", cap);
    printf("    所以每个键的 home 都能用「key mod capacity」手算，同时表里出现了真实的生日碰撞）：\n");
    printf("  ");
    col("n", 8); col("α", 8); col("成功实测", 14); col("成功理论", 14);
    col("失败实测", 14); col("失败理论", 14);
    printf("\n  ");
    for (int i = 0; i < 72; i++) {
        putchar('-');
    }
    printf("\n");

    OATable *t = oa_create(cap);
    if (t == NULL) {
        return;
    }
    int keys[512];
    int inserted = 0;
    const int marks[5] = { 102, 205, 307, 410, 512 };
    double up_first = 0.0, up_last = 0.0, sp_first = 0.0, sp_last = 0.0;
    double up_at04 = 0.0;            /* α=0.400 那一档的失败实测，最后一段要拿来对比 */
    int longest_last = 0;            /* 最后一档的最长连跑，实测聚集程度 */
    int miss_samples = 0;

    for (int m = 0; m < 5; m++) {
        while (inserted < marks[m]) {
            int k = make_fresh_key(t, cap);
            if (k < 0) {
                break;
            }
            oa_put(t, k, 1);
            keys[inserted++] = k;
        }
        int hit = 0;
        double sp = measure_get_avg(t, keys, inserted, &hit);
        int miss[256];
        int mn = make_absent_keys(t, miss, 256, cap);
        double up = measure_get_avg(t, miss, mn, &hit);
        double alpha = (double)t->used / (double)t->capacity;

        if (m == 3) {
            up_at04 = up;             /* 顺手记下 α=0.400 那档，末尾要对比 */
        }
        if (m == 4) {
            int cur = 0;
            longest_last = 0;
            for (int i = 0; i < t->capacity; i++) {
                if (t->slots[i].state == SLOT_EMPTY) {
                    cur = 0;
                } else if (++cur > longest_last) {
                    longest_last = cur;
                }
            }
        }
        printf("  %-8d %-8.3f %-14.2f %-14.2f %-14.2f %-14.2f\n",
               t->size, alpha, sp, theory_probe_success(alpha), up,
               theory_probe_unsuccess(alpha));
        if (m == 0) {
            up_first = up;
            sp_first = sp;
        }
        up_last = up;
        sp_last = sp;
        miss_samples = mn;
    }
    printf("    （失败样本：每档另取 %d 个「确定没插过」的键，home 由固定 LCG 生成，可复现）\n",
           miss_samples);
    printf("  理论式（线性探测 + 简单均匀哈希，Knuth）：成功 ½(1+1/(1-α))，"
           "失败 ½(1+1/(1-α)²)\n");
    printf("  对照：α 从 0.100 涨到 0.500，失败查找实测从 %.2f 格涨到 %.2f 格（理论 %.2f → %.2f），\n",
           up_first, up_last, theory_probe_unsuccess(0.100), theory_probe_unsuccess(0.500));
    printf("    成功查找从 %.2f 格涨到 %.2f 格 —— 看着还温和，因为【命中】在串的中段就停了，\n",
           sp_first, sp_last);
    printf("    只有【不命中】必须一路走到串尾的空槽，所以它的曲线陡得多。\n");
    printf("    可 α 再想往上：0.75 → %.1f 格，0.90 → %.1f 格，0.95 → %.1f 格 —— "
           "曲线右端是【墙】。\n",
           theory_probe_unsuccess(0.75), theory_probe_unsuccess(0.90),
           theory_probe_unsuccess(0.95));
    printf("    本节阈值 0.5 不是抄来的教条，是这条曲线的形状决定的：\n");
    printf("    实测列和理论列基本咬得住，说明「退化」就是理论里那条曲线，不是编的。\n");
    printf("  ⚠ 两个诚实的补充：\n");
    printf("    1) 本演示的表最多只跑到 α=0.500 —— 一旦越线程序就搬家了，所以 0.75/0.90 那几档\n");
    printf("       只能看公式。这恰好就是答案本身：开放寻址【不该】让 α 到 0.75。\n");
    printf("    2) 最后一档失败实测 %.2f 格、理论 %.2f 格，实测【高于】理论 —— 不是 bug：\n",
           up_last, theory_probe_unsuccess(0.500));
    printf("       那条公式的前提是「简单均匀哈希」（每次探测换个全新位置），而线性探测会聚集，\n");
    printf("       本档实测最长连跑 %d 格（%d 个键抢 %d 个 home，串与串会互相并起来）。\n",
           longest_last, t->size, cap);
    printf("       所以正确的读法是：公式给出【下限形状】，线性探测在它上面再加一层聚集税；\n");
    printf("       α=0.400 那档实测 %.2f vs 理论 %.2f 差得小，是因为那时候串还短、税还轻。\n",
           up_at04, theory_probe_unsuccess(0.400));
    oa_free(t);
}

/*
 * =====================================================================
 * 第 9 幕：形状总结 —— 和拉链法并排画一遍
 * =====================================================================
 */
static void act_summary(void)
{
    section("第 9 幕：把这一节的形状总结成一张图");

    printf("  同样存 12、20、28 三个键（capacity=8，home 全是 4）：\n\n");
    printf("  拉链法：桶数组里【只放指针】，元素住在堆上散落的节点里\n");
    printf("    ┌────┬────┬────┬────┬────┬────┬────┬────┐\n");
    printf("    │    │    │    │    │  ▲ │    │    │    │\n");
    printf("    └────┴────┴────┴────┴──┬─┴────┴────┴────┘\n");
    printf("                            └─►[28]─►[20]─►[12]─►NULL\n");
    printf("      3 次 malloc、3 个互不相邻的地址；删除 = 摘节点，干净利落\n\n");
    printf("  开放寻址：【就一个数组】，元素本体躺在格子里\n");
    printf("    ┌────┬────┬────┬────┬──────────┬──────────┬──────────┬────┐\n");
    printf("    │  0 │  1 │  2 │  3 │    4     │    5     │    6     │  7 │\n");
    printf("    ├────┼────┼────┼────┼──────────┼──────────┼──────────┼────┤\n");
    printf("    │ ·  │ ·  │ ·  │ ·  │ U 12=120 │ U 20=200 │ U 28=280 │ ·  │\n");
    printf("    └────┴────┴────┴────┴──────────┴──────────┴──────────┴────┘\n");
    printf("      1 次 calloc；冲突水平摊开；删除必须留 †，否则剪断后面那些键的路\n\n");
    printf("  删掉 12 之后两张表各自长什么样 —— 这就是两节的全部差别：\n\n");
    printf("    拉链法   buckets[4] ─►[28]─►[20]─►NULL   ← 20 还在原位，一跳就到，摘掉节点就完事\n");
    printf("    开放寻址 槽4=† 槽5=U 20 槽6=U 28          ← 20 还在原位，但找它【必须能跨过 †】\n\n");

    printf("  三个必须带走的结论：\n");
    printf("    1) EMPTY 是查找唯一的「停止标志」。删除写成 EMPTY = 剪断别人的探测链，\n");
    printf("       症状是「数据还在、程序说没有」，不崩不报错。所以默认必须是墓碑。\n");
    printf("    2) 装载率的分子是 used = USED + 墓碑，不是 size —— 墓碑挡路，所以它贵。\n");
    printf("       于是阈值比拉链法保守（0.5 vs 0.75），并且多一招「同容量重排蒸发墓碑」。\n");
    printf("    3) 元素在数组里 = 缓存友好 + 只分配两次；代价是冲突会传染（primary clustering），\n");
    printf("       α 一高步数就沿 ½(1+1/(1-α)) 起飞。想要不聚集得换探测策略：\n");
    printf("       二次探测 / 双重哈希把「+1」换成「+i²」或「+h2(key)」，聚集小得多，\n");
    printf("       但删除的墓碑问题一模一样 —— 墓碑是开放寻址的债，不是线性探测的债。\n");
    printf("  本幕不建表（纯图）。前八幕的表都在各自幕结尾 free 掉了，ASan 会来核对。\n");
}

int main(int argc, char **argv)
{
    int show_bug = 0;

    /* 命令行开关：默认【不】跑错误路径。
     * --bug 是本节头文件定名的开关；--show-bug 是同义别名，两个都收。 */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--bug") == 0 || strcmp(argv[i], "--show-bug") == 0) {
            show_bug = 1;
        } else {
            printf("  （忽略未知参数 \"%s\"；可用开关：--bug / --show-bug）\n", argv[i]);
        }
    }

    printf("开放寻址 + 线性探测（08-hash/hash-table-open-addressing）\n");
    printf("删除策略默认 = %s；%s\n", oa_mode_name(OA_DEL_TOMBSTONE),
           show_bug ? "本次带 --bug：第 3 幕会真实跑一遍「直接清零」的断链事故"
                    : "想看断链事故请加 --bug");

    act_layout(show_bug);
    act_probe_path();
    act_delete_tombstone(show_bug);
    act_probe_over_tombstones();
    act_tomb_accounting();
    act_growth();
    act_cache();
    act_clustering();
    act_summary();

    printf("\n=== 本节结束 ===\n");
    printf("记住三句话：\n");
    printf("  1) 只有【真·空槽 EMPTY】能让查找判负；USED 不匹配和墓碑都必须继续走。\n");
    printf("  2) 删除写墓碑，装载率分子含墓碑；墓碑躲在路径外不肯消失时，做一次同容量重排。\n");
    printf("  3) 元素在数组里 → 缓存友好、只分配两次；但冲突会传染，所以阈值只能到 0.5。\n");
    return 0;
}
