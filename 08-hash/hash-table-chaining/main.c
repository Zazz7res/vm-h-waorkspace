#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chaining.h"

/*
 * main.c —— 拉链法哈希表分幕演示
 *
 * 这一节有两个看点：
 *   A. 概念：哈希表 = 哈希函数 + 桶数组 + 冲突处理。三件事每一件都能打印出来。
 *   B. 内存：两级（int 键）/ 三级（字符串键）分配，free 必须从里往外剥，0 泄漏。
 *
 * 所有桶下标都用 identity 哈希（h(key) = key）算，所以**每个数字你都能手算复核**。
 */

/* 小工具：打印分隔线（每个 main.c 自带一份，叶子目录互不引用） */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/*
 * =====================================================================
 * 第 1 幕：先把最阴的那个坑钉死 —— C 的 % 会返回负数
 * =====================================================================
 */
static void act_mod_pitfall(void)
{
    section("第 1 幕：hash(key) % capacity —— 负数取余的坑（实测）");
    ht_mod_pitfall();
    printf("\n  一句话记住：**下标换算只许写 ht_mod(...)，不许裸写 key %% capacity。**\n");
}

/*
 * =====================================================================
 * 第 2 幕：哈希函数把键投进桶 —— 每个下标都手算复核
 * =====================================================================
 */
static void act_first_table(void)
{
    section("第 2 幕：建一张 8 个桶的表，每投一个键就手算一次");

    IntHT *t = ht_create(8);
    if (t == NULL) {
        printf("  建表失败\n");
        return;
    }

    /* 故意挑这些键：3/11/27 都 % 8 == 3（要撞在一起），20/100 都 % 8 == 4，8 % 8 == 0 */
    int keys[] = { 3, 11, 20, 27, 8, 100 };
    int n = (int)(sizeof(keys) / sizeof(keys[0]));

    printf("  逐个 put（哈希函数 = 键本身，桶下标 = ht_mod(key, 8) = key %% 8）：\n");
    for (int i = 0; i < n; i++) {
        int before = ht_len(t);
        ht_put(t, keys[i], keys[i] * 10);   /* 值随手设成 key*10，方便核对 */
        int after = ht_len(t);
        printf("    put(%3d, %4d) → 手算桶 = %d，size %d → %d\n",
               keys[i], keys[i] * 10, ht_mod(keys[i], 8), before, after);
    }
    /* 再 put 一次已存在的键，演示「覆盖不新增」 */
    int before = ht_len(t);
    ht_put(t, 3, -777);
    printf("    put(  3, -777) → 键已存在：size %d → %d（覆盖值，不新增元素）\n",
           before, ht_len(t));

    printf("\n");
    ht_print(t);
    printf("\n");
    ht_stats(t);

    printf("\n  ⚠ 注意 buckets[3] 那条链是 [k=27] -> [k=11] -> [k=3]，和插入顺序【相反】：\n");
    printf("    因为拉链法用头插（O(1)）。哈希表不保证任何顺序，这是它的本性，不是 bug。\n");

    /* 查三种情况：链尾命中、链头命中、桶非空但键不在 */
    int v = -1;
    t->probes = 0;
    int found = ht_get(t, 3, &v);
    printf("\n  get(3)  : 命中=%d 值=%d 走了 %ld 个节点（桶 3 链长 3，它在链尾）\n",
           found, v, t->probes);
    t->probes = 0;
    found = ht_get(t, 27, &v);
    printf("  get(27) : 命中=%d 值=%d 走了 %ld 个节点（它在链头，一次就中）\n",
           found, v, t->probes);
    t->probes = 0;
    found = ht_get(t, 4, &v);
    printf("  get(4)  : 命中=%d       走了 %ld 个节点（桶 4 有 2 个节点，都不是 4 → 判定不存在）\n",
           found, t->probes);
    printf("  get(-1) : 命中=%d  桶下标 = ht_mod(-1,8) = %d ← 负数键也进得了合法桶\n",
           ht_contains(t, -1), ht_mod(-1, 8));

    printf("\n  内存账：IntHT %zu 字节 + buckets 数组 %d 个指针 × %zu = %zu 字节 + "
           "%d 个节点 × %zu = %zu 字节\n",
           sizeof(IntHT), t->capacity, sizeof(HTNode *),
           (size_t)t->capacity * sizeof(HTNode *),
           t->size, sizeof(HTNode), (size_t)t->size * sizeof(HTNode));
    printf("  （「两级内存」：一次连续的指针数组 + 若干散落在堆上的小方块）\n");

    ht_free(t);
    printf("  已 ht_free：逐桶逐节点 free → free 数组 → free 表结构，ASan 会来查岗\n");
}

/*
 * =====================================================================
 * 第 3 幕：冲突可视化 —— 三个键故意挤同一个桶
 * =====================================================================
 */
static void act_collision(void)
{
    section("第 3 幕：故意构造冲突 —— 1、9、17 全撞在 buckets[1]");

    IntHT *t = ht_create(8);
    if (t == NULL) {
        return;
    }
    printf("  capacity = 8 时：1 %% 8 = 1，9 %% 8 = 1，17 %% 8 = 1 → 三个键算出同一个下标。\n");
    printf("  这就是【冲突】：不是 bug，是鸽笼原理的必然结果（键无穷，桶有限）。\n\n");

    ht_put(t, 1, 111);
    ht_put(t, 9, 999);
    ht_put(t, 17, 1717);
    ht_print(t);

    printf("\n  buckets[1] 那条链摊开画（三个节点各是一次 malloc，散落在堆上）：\n");
    printf("    头指针 t->buckets[1]\n");
    printf("      │\n");
    printf("      ▼\n");
    printf("    ┌────────┬───────┬──────┐     ┌───────┬───────┬──────┐     ┌───────┬───────┬──────┐\n");
    printf("    │key  17 │val1717│ next ├───► │key  9 │val 999│ next ├───► │key  1 │val 111│ next ├───► NULL\n");
    printf("    └────────┴───────┴──────┘     └───────┴───────┴──────┘     └───────┴───────┴──────┘\n");
    printf("    （17 最后插，头插把它顶到了链头 → 链里顺序 = 插入顺序的倒序）\n");

    int v = 0;
    t->probes = 0;
    int hit = ht_get(t, 1, &v);
    printf("\n  插入后 get(1) 还正确吗？ 命中=%d 值=%d，比较了 %ld 个节点\n", hit, v, t->probes);
    printf("    → 正确。代价是：它必须顺链走到第 3 个节点才比中 key==1。\n");
    printf("      哈希函数只决定「进哪个桶」，**「到底是哪个键」靠链表上逐个比 key 相等**，\n");
    printf("      所以节点里必须把 key 也存一份（只存 value 就没法确认命中）。\n");
    printf("      同理：桶下标相同 ≠ 键相同，所以哈希函数允许「乱撞」，正确性由比 key 兜底。\n");

    t->probes = 0;
    hit = ht_get(t, 2, &v);
    printf("  get(2)  ：命中=%d，比较了 %ld 个节点 ← 桶 2 是空的，0 步就判定不存在\n",
           hit, t->probes);

    ht_stats(t);
    printf("\n  负载因子 %.3f（size 3 / capacity 8）：整体很空，但桶 1 已经挂了 3 个节点 ——\n",
           ht_load_factor(t));
    printf("    说明**负载因子是个平均值，掩盖不了局部长链**；局部由哈希函数的质量决定。\n");

    ht_free(t);
}

/*
 * =====================================================================
 * 第 4 幕：负载因子 0.75 触发 rehash —— 先预测，再验证
 * =====================================================================
 */
static void act_rehash(void)
{
    section("第 4 幕：负载因子超 0.75 → 桶数翻倍 + 全部重新投桶");

    IntHT *t = ht_create(4);
    if (t == NULL) {
        return;
    }
    /* 键 1、5、9：% 4 全等于 1 → 三个挤在一个桶；翻倍后有的搬家有的不动，正好看清规律 */
    ht_put(t, 1, 10);
    ht_put(t, 5, 50);
    ht_put(t, 9, 90);
    printf("  先装 3 个键（1、5、9），此时：\n");
    ht_print(t);
    printf("  负载因子 = %d/%d = %.3f —— **正好 0.75，还没超，所以不扩**（规矩是严格大于）\n",
           t->size, t->capacity, ht_load_factor(t));
    printf("  判据写成整数乘法：(size+1)*4 > capacity*3 ? 装入第 4 个时 16 > 12 成立 → 扩\n");

    printf("\n");
    ht_rehash_plan(t, 8);

    printf("\n  现在 put 第 4 个键 13 —— 程序自己判断该扩容了：\n");
    ht_put(t, 13, 130);
    printf("\n  真实结果（和上面的预测逐行对照）：\n");
    ht_print(t);
    printf("  验证：1→桶1（原地，q=0 偶），5→桶5（搬家，q=1 奇，1+4=5），"
           "9→桶1（原地，q=2 偶），13→桶5。\n");
    printf("        1 %% 8 = %d, 5 %% 8 = %d, 9 %% 8 = %d, 13 %% 8 = %d —— 全部对上 ✅\n",
           ht_mod(1, 8), ht_mod(5, 8), ht_mod(9, 8), ht_mod(13, 8));

    printf("\n  扩容后 size 不变（还是 %d），capacity 变成 %d，负载因子降到 %.3f：\n",
           t->size, t->capacity, ht_load_factor(t));
    printf("    扩容的意义不是「多塞几个」，而是**把长链剪短**：原来一个桶挂 4 个，"
           "现在摊成两个桶。\n");
    printf("    代价是 O(n) 全部重投 —— 但因为每次翻倍，摊到每次插入仍是 O(1)（摊还复杂度）。\n");
    printf("    ⚠ ht_put 里 rehash 之后必须【重新算一次桶下标】再挂链；"
           "拿旧下标挂新数组 = 元素进错桶 = 数据静默丢失。\n");

    int v = 0;
    printf("  扩容后老键还能取到吗？ get(1)=%d get(5)=%d get(9)=%d ← 三个都靠重投找回原位\n",
           ht_get(t, 1, &v) ? v : -1,
           ht_get(t, 5, &v) ? v : -1,
           ht_get(t, 9, &v) ? v : -1);

    ht_free(t);
}

/*
 * =====================================================================
 * 第 5 幕：delete / contains / len —— 拉链法删除有多干净
 * =====================================================================
 */
static void act_delete(void)
{
    section("第 5 幕：删除 —— 就是「删一个链表节点」，不欠任何债");

    IntHT *t = ht_create(8);
    if (t == NULL) {
        return;
    }
    int keys[] = { 1, 9, 17, 2, 10 };
    for (int i = 0; i < 5; i++) {
        ht_put(t, keys[i], keys[i]);
    }
    printf("  删之前：\n");
    ht_print(t);

    printf("\n  ht_delete(9) → 删掉桶 1 链中间那个节点，返回 %d\n", ht_delete(t, 9));
    ht_print(t);
    printf("  contains(9) = %d, contains(1) = %d, contains(17) = %d ← 邻居没被牵连\n",
           ht_contains(t, 9), ht_contains(t, 1), ht_contains(t, 17));

    printf("\n  ht_delete(9) 再来一次 → 返回 %d（本来就没有，删除幂等，不会二次 free）\n",
           ht_delete(t, 9));
    printf("  ht_delete(404) → 返回 %d（桶 4 是空的，哨兵循环一圈就出来）\n", ht_delete(t, 404));

    printf("\n  ht_delete(17) → 删桶 1 的【链头】，返回 %d\n", ht_delete(t, 17));
    printf("    这一步就是第 02 章「删头节点」的难点：要改的是 buckets[1] 这个变量本身，\n");
    printf("    所以 node_remove_key 的形参写成 HTNode **head，调用时传 &t->buckets[idx]。\n");
    ht_print(t);

    printf("\n  size = %d，capacity = %d，负载因子 = %.3f\n",
           t->size, t->capacity, ht_load_factor(t));
    printf("  ⚠ 删了 2 个键，capacity 仍是 8：**拉链法不自动缩容**。\n");
    printf("    因为「删一半→再插满」会来回搬家（thrash），绝大多数库只扩容不缩容，\n");
    printf("    真要缩必须等负载因子跌破一个很低的下限（比如 0.1）且表足够大。\n");

    ht_free(t);
}

/*
 * =====================================================================
 * 第 6 幕：实测「退化成链表」—— 用比较次数说话
 * =====================================================================
 */
static void act_degeneration(void)
{
    section("第 6 幕：健康表 vs 病表 —— O(1) 是真的还是嘴上说说（n = 1000）");

    const int n = 1000;

    /* 健康：键 0..999，均匀铺开 */
    IntHT *good = ht_create(8);
    if (good == NULL) {
        return;
    }
    int ok = ht_fill_arithmetic(good, n, 0, 1);
    printf("  【健康表】插入 %d 个（键 = 0,1,2,...,%d，step=1）\n", ok, n - 1);
    ht_stats(good);
    long p_good = ht_probe_after_n_gets(good, n, 0, 1);

    /* 病：键全是 4096 的倍数 → 桶数翻倍到 2048 也没用，全在桶 0 */
    IntHT *bad = ht_create(8);
    if (bad == NULL) {
        ht_free(good);
        return;
    }
    ok = ht_fill_arithmetic(bad, n, 0, 4096);
    printf("\n  【病表】插入 %d 个（键 = 0, 4096, 8192, ...，step = 4096 = 2^12）\n", ok);
    printf("    0 %% 任何 2 的幂都 = 0，所以扩容到 %d 个桶也没用，全挤在 buckets[0]。\n",
           bad->capacity);
    ht_stats(bad);
    long p_bad = ht_probe_after_n_gets(bad, n, 0, 4096);

    printf("\n  对比（这就是「退化」的真实数字）：\n");
    printf("    健康表 %d 次 get 比较节点数 = %ld（平均 %.2f，约 O(1)）\n",
           n, p_good, (double)p_good / n);
    printf("    病表   %d 次 get 比较节点数 = %ld（平均 %.2f，约 n/2 → O(n)）\n",
           n, p_bad, (double)p_bad / n);
    printf("    相差 %.0f 倍。病表的 %ld 正好等于 1+2+...+n = n(n+1)/2 = %d ——\n",
           (double)p_bad / (double)p_good, p_bad, n * (n + 1) / 2);
    printf("    链头到链尾的距离就是比较次数，和单链表查找一模一样。\n");
    printf("  结论：**哈希表的 O(1) 是「平均/期望」，不是「保证」。**\n");
    printf("    病从两处来：① 哈希函数烂（真实键有公共因子）；② 负载因子失控。\n");
    printf("    本节用 step=4096 人为制造 ①，用 0.75 阈值挡住 ②。\n");
    printf("    （为什么用比较次数而不是掐表计时：计时会被 CPU 频率、缓存冷热、系统调度搅浑；\n");
    printf("      比较次数是确定性的，这两行数字你重跑一百遍也一样。）\n");

    ht_free(good);
    ht_free(bad);
}

/*
 * =====================================================================
 * 第 7 幕：字符串键 —— djb2 把一串字符揉成一个整数
 * =====================================================================
 */
static void act_string_hash(void)
{
    section("第 7 幕：字符串键 djb2 哈希：h = h*33 + c，值仍是 int");

    /* 前 6 个是短键（哈希值小，塞进 long 不会变负）；
     * 后 3 个是故意挑出来的长键，实测会把 64 位最高位置成 1 → 收窄成 long 就变负数 */
    const char *words[] = {
        "a", "b", "app", "appl", "apple", "ppa",
        "cat", "electrocardiograph", "photomechanically"
    };
    int nw = (int)(sizeof(words) / sizeof(words[0]));

    printf("  djb2：h = 5381，逐字符 h = h * 33 + c（用 unsigned long，溢出 = 回绕，定义良好）\n");
    printf("  %-22s %4s %22s %22s %7s %8s %8s\n",
           "键", "长度", "h（无符号）", "同一串位存进 long", "裸%8", "ht_mod", "ht_mod_u");
    printf("  ------------------------------------------------------------------------"
           "-------------------\n");
    for (int i = 0; i < nw; i++) {
        unsigned long h = ht_hash_string(words[i]);
        long as_long = (long)h;        /* ★ 危险动作：无符号 → 有符号的收窄转换 */
        printf("  %-22s %4zu %22lu %22ld %7d %8d %8lu%s\n",
               words[i], strlen(words[i]), h, as_long,
               (int)(as_long % 8), ht_mod(as_long, 8), ht_mod_u(h, 8UL),
               (as_long < 0) ? "   ← 最高位是 1，存进 long 变负：裸 %8 就是越界下标" : "");
    }

    printf("\n  读这张表的三个要点：\n");
    printf("    1) 「app / appl / apple」只差一两个字符，h 却差好几个数量级 ——\n");
    printf("       乘法把每个字符都搅进了高位，这就是散列均匀的来源。\n");
    printf("       对照：若用「字符相加」当哈希，\"app\" 和 \"ppa\" 必然撞（加法不管顺序）。\n");
    printf("    2) 长键那几行：h 存进 long 会变成**负数**（值超出 long 能表示的范围，\n");
    printf("       C 标准把这种转换列为 implementation-defined，gcc 按补码回绕）。\n");
    printf("       于是裸写 (int)(h %% 8) 得到负下标 —— 第 1 幕那个坑换了个入口又出现一次。\n");
    printf("    3) 最后两列（ht_mod 平移 vs ht_mod_u 无符号）在这里都是对的：\n");
    printf("       capacity = 8 是 2 的幂，2^64 能被 8 整除，所以两种修法数值恰好相同。\n");
    printf("       桶数不是 2 的幂时就不一定，比如同一批键按 %% 3 算：\n");
    for (int i = 6; i < nw; i++) {
        unsigned long h = ht_hash_string(words[i]);
        printf("         %-22s ht_mod((long)h, 3) = %d   ht_mod_u(h, 3) = %lu   %s\n",
               words[i], ht_mod((long)h, 3), ht_mod_u(h, 3UL),
               (ht_mod((long)h, 3) == (int)ht_mod_u(h, 3UL)) ? "相同" : "★ 不同");
    }
    printf("       → 结论：**字符串哈希一路用无符号（ht_mod_u），中途不要塞回有符号类型。**\n");

    StrHT *s = sh_create(8);
    if (s == NULL) {
        return;
    }
    sh_put(s, "cat", 1);
    sh_put(s, "car", 2);
    sh_put(s, "dog", 3);
    sh_put(s, "cat", 9);        /* 覆盖：键已存在，只改值 */
    printf("\n  put cat=1, car=2, dog=3，再 put cat=9（覆盖）：\n");
    printf("    手算复核桶位：ht_mod_u(h(\"cat\"),8) = %lu, ht_mod_u(h(\"car\"),8) = %lu,"
           " ht_mod_u(h(\"dog\"),8) = %lu\n",
           ht_mod_u(ht_hash_string("cat"), 8UL),
           ht_mod_u(ht_hash_string("car"), 8UL),
           ht_mod_u(ht_hash_string("dog"), 8UL));
    sh_print(s);
    int v = 0;
    printf("  get(cat) = %d ← 覆盖生效（不是插了两条）；size = %d\n",
           sh_get(s, "cat", &v) ? v : -1, sh_len(s));
    printf("  contains(\"cat\") = %d, contains(\"CAT\") = %d ← strcmp 区分大小写\n",
           sh_contains(s, "cat"), sh_contains(s, "CAT"));

    /* 演示「表自己复制了键」：调用者的缓冲区改了，表里的键纹丝不动 */
    char buf[8];
    buf[0] = 'c'; buf[1] = 'a'; buf[2] = 't'; buf[3] = '\0';
    sh_get(s, buf, &v);
    buf[0] = 'x';                /* 把调用者的字符串改成 "xat" */
    printf("  调用者的缓冲区改成 \"xat\" 之后，表里仍然：contains(\"cat\")=%d, contains(\"xat\")=%d\n",
           sh_contains(s, "cat"), sh_contains(s, "xat"));
    printf("  → 因为 sh_put 里 dup_str 复制了一份。存别人的指针 = 迟早悬空（第 00 章的债）。\n");
    printf("  ⚠ 顺带一条 C 语言规矩：strdup 属于 POSIX，不在 ISO C11 里；\n");
    printf("    用 -std=c11 编译时它**没有声明**，隐式声明被当成返回 int → 指针被截断，\n");
    printf("    64 位机上必崩。所以本节自己写了一份 dup_str（malloc + 逐字节抄，连终止符一起）。\n");

    printf("  delete(cat) = %d；sh_free 会连带 free 每个 node->key —— 三层所有权。\n",
           sh_delete(s, "cat"));
    sh_free(s);
}

/*
 * =====================================================================
 * 第 8 幕：真实用途 —— 词频统计
 * =====================================================================
 */
static void act_word_freq(void)
{
    section("第 8 幕：词频统计 —— 键是 char *，值是 int 计数器");

    const char *text =
        "The quick brown fox jumps over the lazy dog. "
        "The dog barked at the fox, and the fox ran away; "
        "the quick dog chased the fox again!";

    printf("  原文：\n    \"%s\"\n\n", text);

    StrHT *t = sh_create(8);
    if (t == NULL) {
        return;
    }
    int tokens = sh_count_words(t, text);
    printf("  切出 %d 个词（含重复），不同的键 %d 个\n\n", tokens, sh_len(t));

    printf("  按桶顺序打印（既不是插入顺序，也不是字典序，而是「哈希 %% 桶数」的顺序）：\n");
    sh_print(t);

    printf("\n");
    sh_print_top(t, 6);

    printf("\n  除了榜单，还能随机问任意词（这才是哈希表的强项：不问顺序，只问有没有）：\n");
    int v = 0;
    printf("    get(\"fox\")  = %d\n", sh_get(t, "fox", &v) ? v : -1);
    printf("    get(\"the\")  = %d ← 原文里的 \"The\" 被 tolower 归一化后并进了同一个键\n",
           sh_get(t, "the", &v) ? v : -1);
    printf("    get(\"lazy\") = %d\n", sh_get(t, "lazy", &v) ? v : -1);
    printf("    get(\"owl\")  = %d（不存在时打印 -1，而不是 0 —— 0 是合法计数值，不能当哨兵）\n",
           sh_get(t, "owl", &v) ? v : -1);
    printf("    标点 . , ; ! 都被当分隔符丢掉，所以 \"dog.\" 记成 \"dog\"。\n");
    printf("    如果原文有 \"don't\"，会被切成 don 和 t 两个词 —— 切词规则必须自己显式选，\n");
    printf("    这叫**键的规范化**，工程里它比哈希函数更容易出错，也和哈希表本身无关。\n");

    printf("\n  负载因子 %.3f，桶数 %d，节点数 %d：\n",
           sh_load_factor(t), t->capacity, sh_len(t));
    printf("    每个词 = 一次 malloc 节点(%zu 字节) + 一次 malloc 键字符串，\n",
           sizeof(StrNode));
    printf("    固定开销合计 %zu 字节（结构体 + 桶数组 + 节点），字符串另算：\n",
           sizeof(StrHT) + (size_t)t->capacity * sizeof(StrNode *)
               + (size_t)sh_len(t) * sizeof(StrNode));
    for (int i = 0; i < t->capacity; i++) {
        for (const StrNode *p = t->buckets[i]; p != NULL; p = p->next) {
            printf("      %-8s 节点 %zu + 字符串 %zu = %zu 字节\n",
                   p->key, sizeof(StrNode), strlen(p->key) + 1,
                   sizeof(StrNode) + strlen(p->key) + 1);
        }
    }

    sh_free(t);
    printf("  sh_free 已把「结构体 → 桶数组 → 节点 → 每个 node->key」四层从里往外剥干净。\n");
}

/*
 * =====================================================================
 * 第 9 幕：把这一节的形状总结成一张图
 * =====================================================================
 */
static void act_summary(void)
{
    section("第 9 幕：把这一节的形状总结成一张图");

    printf("  IntHT 在内存里到底长什么样（capacity = 8，存了第 2 幕那 6 个键）：\n\n");
    printf("    t->buckets ──►┌────────┬────────┬────────┬────────┬────────┬────────┬────────┬────────┐\n");
    printf("                  │  [0]   │  [1]   │  [2]   │  [3]   │  [4]   │  [5]   │  [6]   │  [7]   │\n");
    printf("                  └───┬────┴───┬────┴───┬────┴───┬────┴───┬────┴───┬────┴───┬────┴───┬────┘\n");
    printf("                      │        │        │        │        │        │        │        │\n");
    printf("                      ▼        ▼        ▼        ▼        ▼        ▼        ▼        ▼\n");
    printf("                    [k=8]    NULL     NULL     [k=27]   [k=100]  NULL     NULL     NULL\n");
    printf("                      │                          │        │\n");
    printf("                    NULL                       [k=11]   [k=20]\n");
    printf("                                                 │        │\n");
    printf("                                               [k=3]    NULL\n");
    printf("                                                 │\n");
    printf("                                               NULL\n");
    printf("    上面 8 个格子 = 一次 calloc：%d 个指针 × %zu 字节 = %d 字节，连续内存。\n",
           8, sizeof(HTNode *), 8 * (int)sizeof(HTNode *));
    printf("    下面每个方框 = 各一次 malloc 的 HTNode（%zu 字节），散落在堆上，靠 next 串起来。\n",
           sizeof(HTNode));
    printf("    桶 0：8 %% 8 = 0 → 1 个节点。  桶 3：3/11/27 都 %% 8 = 3 → 3 个节点一条链。\n");
    printf("    桶 4：20/100 都 %% 8 = 4 → 2 个节点。其余 5 桶是 NULL —— 空桶只花一个指针，\n");
    printf("           这就是拉链法永远「装得下」的原因：桶数组本身从不装元素。\n\n");
    printf("  三句话总结：\n");
    printf("    1) 找键 = 算下标（O(1)）+ 走链（= 链长）。所以控链长就是控性能。\n");
    printf("    2) 链长由两件事决定：哈希函数均不均匀、负载因子高不高（我们用 0.75 卡住）。\n");
    printf("    3) 桶数组管「跳到哪里」，链表管「撞了怎么办」—— 拉链法就是把前两章的\n");
    printf("       动态数组和单链表拼起来，各干自己最擅长的那一半。\n");
    printf("  所有表都已 free。跑一遍 ASan 版（README 里有命令），stderr 应该是 0 字节。\n");
}

int main(void)
{
    act_mod_pitfall();
    act_first_table();
    act_collision();
    act_rehash();
    act_delete();
    act_degeneration();
    act_string_hash();
    act_word_freq();
    act_summary();

    printf("\n=== 本节结束 ===\n");
    printf("记住三句话：\n");
    printf("  1) 哈希表 = 哈希函数 + 桶数组 + 冲突处理，三者缺一块都不叫哈希表。\n");
    printf("  2) 下标换算必须保证非负（ht_mod）；键必须存在节点里，命中要逐个比 key。\n");
    printf("  3) O(1) 建立在「链很短」这个假设上，靠 0.75 负载因子 + rehash 维持。\n");
    return 0;
}
