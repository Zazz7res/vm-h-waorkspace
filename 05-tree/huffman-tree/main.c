#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "huffman.h"

/*
 * ===================================================================
 * main.c —— 哈夫曼树 / 哈夫曼编码：一个真的能压缩再解压的小闭环
 * ===================================================================
 * 九幕：
 *   第 1 幕 变长编码为什么会解不出来 —— 前缀冲突（全章最重要的一段）
 *   第 2 幕 统计频次 + 用最小堆建树（每一步的堆状态都打印）
 *   第 3 幕 打印生成的哈夫曼树
 *   第 4 幕 编码表：字符 | 频次 | 编码 | 码长，并做前缀码自检
 *   第 5 幕 WPL 怎么算（定义法 + 合并权重法，两者必须相等）
 *   第 6 幕 压缩：ASCII 定长 / 字符集定长 / 哈夫曼 三种比特数对比 + 压缩率
 *   第 7 幕 解压：比特流走树还原，和原文 strcmp 对比 —— 证明真的可逆
 *   第 8 幕 边界情况：整段文本只有一种字符（建树只剩一个根，必须特判）
 *   第 9 幕 销毁
 *
 * 编译运行（在项目根目录）：
 *   make one DIR=05-tree/huffman-tree
 */

static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/*
 * 把"一个字符"打印成人能看懂的样子：空格写成「空格」，不可打印的写成 0xNN。
 * huffman.c 里有一个一模一样的静态小工具，但它是 static 的（不外泄，
 * 因为本目录要求自包含 + 头文件只放"菜单"），所以 main.c 这边自己再写一份。
 * buf 由调用者给，函数返回 buf，这样可以直接塞进 %s。
 */
static const char *char_repr_for_report(unsigned char c, char *buf, size_t buf_size)
{
    if (c == ' ') {
        snprintf(buf, buf_size, "空格");
    } else if (c == '\n') {
        snprintf(buf, buf_size, "\\n");
    } else if (c == '\t') {
        snprintf(buf, buf_size, "\\t");
    } else if (c >= 0x21 && c <= 0x7e) {
        snprintf(buf, buf_size, "%c", c);
    } else {
        snprintf(buf, buf_size, "0x%02X", c);
    }
    return buf;
}

/*
 * 第 1 幕用的一张"坏编码表"：A=0，B=01，C=10。
 * 它不是前缀码，因为 A 的 "0" 正好是 B 的 "01" 的开头。
 *
 * 这里用一个手写的迷你解码器，把 "010" 的两种切法都试出来给人看：
 * 它【故意】在所有能匹配的编码里都试一遍（穷举），所以才叫"歧义"；
 * 真正的哈夫曼解码器（huff_decode）只要一路走树，永远只有一条路。
 * 两边对照着读，前缀码的价值就一目了然。
 */
static void try_decode_bad(const char *bits, int pos, const char *codes[3],
                           const char chs[3], char *trail, int trail_len,
                           char *cuts, int cuts_len, int *solutions)
{
    if (bits[pos] == '\0') {
        trail[trail_len] = '\0';
        cuts[cuts_len]   = '\0';            /* 两条 buffer 都在这一格收尾 */
        (*solutions)++;
        printf("        解法 %d：%-4s   ← 切法：%s\n", *solutions, trail, cuts);
        return;
    }
    for (int k = 0; k < 3; k++) {
        int clen = (int)strlen(codes[k]);
        if (strncmp(bits + pos, codes[k], (size_t)clen) == 0) {
            trail[trail_len] = chs[k];
            /* 拼一份"0|10"这种带竖线的切法，肉眼一看就知道边界在哪 */
            int next = cuts_len;
            if (next > 0) {
                cuts[next++] = '|';
            }
            for (int j = 0; j < clen; j++) {
                cuts[next++] = codes[k][j];
            }
            try_decode_bad(bits, pos + clen, codes, chs, trail, trail_len + 1,
                           cuts, next, solutions);
        }
    }
}

int main(void)
{
    /* ============================================================
       第 1 幕：前缀冲突 —— 为什么"随便变长"是不行的
       ============================================================ */
    section("第 1 幕 · 定长太浪费，变长会歧义：前缀冲突长什么样");

    printf("  场景：一段文本里只有 3 种字符 A / B / C。\n");
    printf("  定长要 ceil(log2(3)) = 2 位，哪怕 A 出现了 1 万次，也得付 2 位。\n");
    printf("  那我把常见的 A 编成 1 位省下来 —— 看会出什么事：\n\n");

    const char *bad_codes[3] = { "0", "01", "10" };
    const char bad_chs[3]    = { 'A', 'B', 'C' };
    printf("  坏编码表：A=0, B=01, C=10   （注意 A 的 0 是 B 的 01 的开头 → 前缀冲突）\n");
    printf("  现在收到比特流 \"010\"，它可能是：\n");

    char trail[32];
    char cuts[64];
    int solutions = 0;
    try_decode_bad("010", 0, bad_codes, bad_chs, trail, 0, cuts, 0, &solutions);
    printf("  ⇒ 同一段比特解出 %d 种不同的结果 → 信息丢了，压缩彻底失败。\n", solutions);
    printf("    问题出在读到第一个 0 的时候：解码器不知道【该不该停】——\n");
    printf("    它可能是完整的 A，也可能是 B 的前一半。要么加分隔符（更费比特），要么完蛋。\n\n");

    printf("  换成一张前缀码表：A=0, B=10, C=11（谁的编码都不是别人的开头）\n");
    printf("  同样读 \"010\"：0→A，剩下 10→B，唯一解 = \"AB\"，不需要分隔符、不需要前瞻。\n");
    printf("  ★ 哈夫曼编码干的就是这件事：自动造出一张【又短又是前缀码】的表。\n");
    printf("    办法是把所有字符都放在树的【叶子】上，编码 = 从根到叶子的 0/1 路径。\n");
    printf("    叶子在半路不会遇到别的叶子，所以前缀冲突在结构上就不可能存在。\n");

    /* ============================================================
       第 2 幕：统计频次 + 建树（全过程打印）
       ============================================================ */
    section("第 2 幕 · 建树：统计频次 → 全塞最小堆 → 每次取两个最小的合并");

    const char *text = "that is the test of huffman coding";
    printf("  示例文本（%zu 个字符）：\"%s\"\n\n", strlen(text), text);

    HuffCodeEntry table[HUFF_CHAR_SET];
    int distinct = 0;
    long merge_sum = 0;
    HuffNode *root = huff_build(text, table, &distinct, &merge_sum);
    if (root == NULL) {
        printf("  建树失败\n");
        return 1;
    }

    /* ============================================================
       第 3 幕：把树画出来
       ============================================================ */
    section("第 3 幕 · 生成的哈夫曼树长这样");
    huff_print_tree(root);
    printf("  读法：从根出发，每往左拐写一个 0、往右拐写一个 1，走到叶子就是那个字符的编码。\n");
    printf("  频次大的字符【离根近】（路径短），频次小的【离根远】（路径长）——\n");
    printf("  这正是「越常用越短」的贪心结果。\n");

    /* ============================================================
       第 4 幕：编码表 + 前缀码自检
       ============================================================ */
    section("第 4 幕 · 编码表：字符 → 频次 → 编码 → 码长");
    huff_print_table(table);

    printf("  前缀码自检（把表里任意两条编码两两对比，看有没有谁是谁的开头）：\n");
    int pf = huff_check_prefix_free(table);
    printf("    => %s\n", pf ? "全部互不为前缀，是合法前缀码 ✓ 所以不需要分隔符就能唯一解码"
                            : "存在前缀冲突 ✗ 这张表解不出原文");

    /* ============================================================
       第 5 幕：WPL
       ============================================================ */
    section("第 5 幕 · 带权路径长度 WPL：这棵树到底值多少比特");

    printf("  定义：WPL = Σ(每个叶子的权重 × 它到根的路径长度)\n");
    printf("        叶子权重 = 该字符出现次数，路径长度 = 该字符的码长\n");
    printf("        所以 WPL 的物理意义就是【整段文本编码后的总比特数】。\n");
    int wpl = huff_wpl(root);
    printf("  ① 定义法（递归遍历叶子累加 weight×depth）算出 WPL = %d 比特\n", wpl);
    printf("  ② 合并法（每次合并产生的新节点权重全部加起来）= %ld 比特\n", merge_sum);
    printf("     %s  ← 这是哈夫曼 coding 的经典结论，两种算法必须相等\n",
           (long)wpl == merge_sum ? "两者相等 ✓" : "两者不等 ✗ 代码有 bug");

    printf("\n  逐叶子的账（码长 × 频次 = 这个字符花掉的比特）：\n");
    int total_bits_check = 0;
    for (int i = 0; i < HUFF_CHAR_SET; i++) {
        if (table[i].freq <= 0) {
            continue;
        }
        char rb[8];
        printf("        %-6s 频次 %2d × 码长 %d = %3d 比特\n",
               char_repr_for_report((unsigned char)i, rb, sizeof rb),
               table[i].freq, table[i].code_len, table[i].freq * table[i].code_len);
        total_bits_check += table[i].freq * table[i].code_len;
    }
    printf("     合计 %d 比特（应当 == WPL）\n", total_bits_check);

    /* ============================================================
       第 6 幕：真的压缩一遍
       ============================================================ */
    section("第 6 幕 · 压缩：三种编码方案各要多少比特");

    size_t text_len = strlen(text);

    char bits[HUFF_MAX_BITS];
    int bit_count = 0;
    int ok = huff_encode(text, table, bits, HUFF_MAX_BITS, &bit_count);
    if (!ok) {
        printf("  编码失败\n");
        return 1;
    }

    /* 定长编码需要几位：能表示 distinct 种字符的最短位数 = ceil(log2(distinct)) */
    int fixed_bits_per_char = 1;
    int room = 2;
    while (room < distinct) {
        fixed_bits_per_char++;
        room *= 2;                     /* room = 2^bits，够用就停 */
    }
    long fixed_total = (long)text_len * fixed_bits_per_char;
    long ascii_total = (long)text_len * 8;

    printf("  ① 最土的定长：直接用 ASCII，每个字符 8 比特 → %ld 比特（%ld 字节）\n",
           ascii_total, (ascii_total + 7) / 8);
    printf("  ② 聪明的定长：文本里只有 %d 种字符，够用的最少位数 = ceil(log2(%d)) = %d 比特/字符\n",
           distinct, distinct, fixed_bits_per_char);
    printf("     → %zu 字符 × %d = %ld 比特（%ld 字节）\n",
           text_len, fixed_bits_per_char, fixed_total, (fixed_total + 7) / 8);
    printf("     ⚠ 定长的浪费就在这：'空格' 和 'y' 频次差 5 倍，却付一样多的比特。\n");
    printf("  ③ 哈夫曼编码：%d 比特（%ld 字节）\n", bit_count, (long)((bit_count + 7) / 8));

    printf("\n  压缩效果：\n");
    printf("    对比 ASCII  ：%ld → %d 比特，压到 %.1f%%，省 %.1f%%\n",
           ascii_total, bit_count, 100.0 * bit_count / (double)ascii_total,
           100.0 - 100.0 * bit_count / (double)ascii_total);
    printf("    对比定长编码：%ld → %d 比特，压到 %.1f%%，省 %.1f%%\n",
           fixed_total, bit_count, 100.0 * bit_count / (double)fixed_total,
           100.0 - 100.0 * bit_count / (double)fixed_total);
    printf("    平均每字符 %.2f 比特（定长要 %d 比特）\n",
           bit_count / (double)text_len, fixed_bits_per_char);

    printf("\n  编码后的比特流（共 %d 位，这里只显示前 %d 位）：\n",
           bit_count, bit_count < 160 ? bit_count : 160);
    for (int i = 0; i < bit_count && i < 160; i++) {
        putchar(bits[i]);
        if (i % 10 == 9) {
            putchar(' ');                     /* 每 10 位一个空格，好看清 */
        }
    }
    if (bit_count > 160) {
        printf(" ...（后面还有 %d 位）", bit_count - 160);
    }
    printf("\n");

    printf("\n  一个字符一个字符地看它变成了什么（前 12 个）：\n");
    for (size_t i = 0; i < text_len && i < 12; i++) {
        char rb[8];
        const char *nm = char_repr_for_report((unsigned char)text[i], rb, sizeof rb);
        printf("        第 %2zu 个字符 %-6s → %s（%d 位）\n",
               i + 1, nm, table[(unsigned char)text[i]].code,
               table[(unsigned char)text[i]].code_len);
    }

    /* ============================================================
       第 7 幕：真的解压回来
       ============================================================ */
    section("第 7 幕 · 解压：比特流 → 走树 → 和原文逐字节对比");

    char back[512];
    int got = huff_decode(bits, bit_count, root, back, (int)sizeof back);
    if (got < 0) {
        printf("  解码失败\n");
        return 1;
    }
    printf("  解码得到 %d 个字符：%s\n", got, back);
    printf("  原文    %zu 个字符：%s\n", text_len, text);
    if (got == (int)text_len && strcmp(back, text) == 0) {
        printf("  ✓ 一模一样 —— 压缩 + 解压闭环成功，而且中间【没有任何分隔符】，\n");
        printf("    全靠哈夫曼编码是前缀码：走到叶子就必然是一个字符的结束。\n");
    } else {
        printf("  ✗ 对不上，代码有 bug\n");
    }

    /* ============================================================
       第 8 幕：边界情况
       ============================================================ */
    section("第 8 幕 · 两个必须知道的边界情况");

    printf("  情况一：整段文本只有一种字符 \"aaaa\"\n");
    HuffCodeEntry t2[HUFF_CHAR_SET];
    int d2 = 0;
    long ms2 = 0;
    HuffNode *r2 = huff_build("aaaa", t2, &d2, &ms2);
    printf("      不同字符 %d 个 → 堆里一上来就只有一个节点，根本不用合并（合并权重和 = %ld）\n",
           d2, ms2);
    printf("      如果按\"根的深度 0\"给编码，编码就是空串，1 比特都不占 ——\n");
    printf("      可解码时怎么知道有几个 a？所以本实现特判成 1 位编码 \"0\"：\n");
    huff_print_table(t2);
    printf("      WPL(定义法) = %d，而合并法 = %ld —— 这个极端情况下两者不再相等，\n",
           huff_wpl(r2), ms2);
    printf("      因为\"合并法\"依赖至少发生一次合并。这是那个经典结论的适用范围。\n");
    huff_free_tree(r2);
    r2 = NULL;

    printf("\n  情况二：比特流末尾被截断（比如文件少写了几位）\n");
    /* ⚠ 缓冲区必须按 HUFF_MAX_BITS 开：截断只是"少给几位"，不是"缓冲区也能变小"。
       这里要是写成 char cut[64]，下面 memcpy 124 字节就直接把栈踩穿了 —— 
       这种坑 ASan 一跑就现形，也正是它存在的意义。 */
    char cut[HUFF_MAX_BITS];
    int cut_bits = bit_count - 2;            /* 故意剪掉最后 2 位 */
    memcpy(cut, bits, (size_t)cut_bits);
    cut[cut_bits] = '\0';
    printf("      把 %d 位的比特流剪成 %d 位，再喂给解码器：\n", bit_count, cut_bits);
    char back2[512];
    int got2 = huff_decode(cut, cut_bits, root, back2, (int)sizeof back2);
    printf("      结果：%d 个字符 → \"%s\"\n", got2, back2);
    printf("      少了 %zu 个字符，而且函数会给出警告（停在内部节点 = 半截码）。\n",
           text_len - (size_t)(got2 > 0 ? got2 : 0));
    printf("      真实压缩文件为什么要在文件头存\"原文长度\"？就是为了知道该在哪一比特停。\n");

    /* ============================================================
       第 9 幕：收摊
       ============================================================ */
    section("第 9 幕 · 销毁");
    huff_free_tree(root);
    root = NULL;
    printf("  所有节点（%d 个叶子 + %d 个内部节点）都已释放。\n",
           distinct, distinct - 1 > 0 ? distinct - 1 : 0);
    printf("  如果 ASan 没报 leak，说明每个 malloc 过的节点都还挂在树上、都被走到过。\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住五句话：\n");
    printf("  1) 定长编码浪费：每个字符都按\"最大种类数\"付比特，罕见字符白白多付。\n");
    printf("  2) 变长编码的杀手是前缀冲突：\"010\" 能切成 0|10 也能切成 01|0，解不出来。\n");
    printf("  3) 哈夫曼把字符全放叶子上，编码=根到叶子的路径 → 天生前缀码，不需要分隔符。\n");
    printf("  4) 建树用最小堆：每次取两个最小的合并放回，直到剩一个（就是根）。O(n log n)。\n");
    printf("  5) WPL = Σ权重×深度 = 总比特数 = 所有合并权重之和，三种算法互相验证。\n");
    return 0;
}
