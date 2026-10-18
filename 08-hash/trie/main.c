#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>     /* offsetof：亲眼看 child[26] 占了哪 208 字节 */
#include "trie.h"

/*
 * main.c —— 前缀树 Trie 分九幕演示
 *
 * 这一节有两个看点：
 *   A. 概念：pass_count（流量）和 end_count（站点）是两个不同的东西。
 *      有了它们，"以 ca 开头的词有几个"和"删除之后该 free 哪些节点"都不用遍历子树。
 *   B. 内存：每节点 216 字节、96% 的格子是 NULL —— 这笔账会打印成真实数字。
 *
 * 第 2~6 幕共用同一棵树，所以你看着它长大 → 被查 → 被删小；
 * 第 7~8 幕各建各的树（跑完就 free）；main 结尾把主树 trie_free 掉，ASan 零泄漏。
 */

/* 小工具：打印分隔线（每个 main.c 自带一份，叶子目录互不引用） */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/*
 * 小工具：把「一个串的每个前缀」的 pass_count / end_count 打成一横排。
 * 为什么坚持用 trie_count_prefix / trie_search 去读，而不是直接 node->pass_count？
 *   因为演示也必须走公开接口 —— 否则读者会以为"内部字段本来就该随便读"，
 *   而真实项目里字段是私有的，接口才是能依赖的东西。
 */
static void show_prefix_counters(const TrieNode *trie, const char *word)
{
    char buf[TRIE_WORD_MAX];
    size_t len = strlen(word);

    printf("      走过的每个前缀的 pass_count（有几个串经过它）：");
    for (size_t k = 1; k <= len; k++) {
        memcpy(buf, word, k);
        buf[k] = '\0';
        printf(" %s=%d", buf, trie_count_prefix(trie, buf));
    }
    printf("\n      走过的每个节点的 end_count（有几个串在这结束）：");
    for (size_t k = 1; k <= len; k++) {
        memcpy(buf, word, k);
        buf[k] = '\0';
        printf(" %s=%d", buf, trie_search(trie, buf));
    }
    printf("\n");
}

/*
 * =====================================================================
 * 第 1 幕：先看清"一个节点"到底多大 —— 后面所有关于费内存的话都有数字支撑
 * =====================================================================
 */
static void act_node_anatomy(void)
{
    section("第 1 幕：一个 TrieNode 有多大（sizeof / offsetof 实测）");

    printf("  sizeof(TrieNode)   = %zu 字节\n", sizeof(TrieNode));
    printf("  sizeof(TrieNode *) = %zu 字节   ← 一个指针的宽度，child[] 里有 %d 个它\n",
           sizeof(TrieNode *), TRIE_ALPHABET);
    printf("  拆开算：child[26] = %d × %zu = %zu 字节，end_count + pass_count = 2 × %zu = %zu 字节\n",
           TRIE_ALPHABET, sizeof(TrieNode *), sizeof(TrieNode *) * TRIE_ALPHABET,
           sizeof(int), sizeof(int) * 2);
    printf("  合计 %zu + %zu = %zu 字节，sizeof 也是 %zu → 中间【没有补齐空洞】（216 是 8 的倍数）\n",
           sizeof(TrieNode *) * TRIE_ALPHABET, sizeof(int) * 2,
           sizeof(TrieNode *) * TRIE_ALPHABET + sizeof(int) * 2, sizeof(TrieNode));

    /* offsetof 把"字段在内存里的偏移"打出来，验证上面那个算式不是嘴上说的 */
    printf("  字段偏移实测：offsetof(child)=%zu  offsetof(end_count)=%zu  offsetof(pass_count)=%zu\n",
           offsetof(TrieNode, child), offsetof(TrieNode, end_count),
           offsetof(TrieNode, pass_count));
    printf("  → end_count 前面正好是 %zu 字节 = 26 个指针，和算式对上。\n",
           offsetof(TrieNode, end_count));

    printf("\n  这就是「用空间换时间」换出去的那部分空间：\n");
    printf("    · 一个只挂 1 个孩子的节点，也要占 %zu 字节，其中 %zu 字节（%.1f%%）是 NULL。\n",
           sizeof(TrieNode), (sizeof(TrieNode *) * (TRIE_ALPHABET - 1)),
           100.0 * (double)(sizeof(TrieNode *) * (TRIE_ALPHABET - 1)) / (double)sizeof(TrieNode));
    printf("    · 想支持 Unicode（65536 个字符）就是 65536 × %zu = %zu 字节一个节点 —— 直接不可行。\n",
           sizeof(TrieNode *), sizeof(TrieNode *) * 65536UL);
    printf("    · 所以真实词库会把 26 格换成链表 / 位图 / 压缩 Trie，见 trie.c 第 11 节的讨论。\n");

    /* 一个孤零零的节点：证明 calloc 清零是真的（不清零就不敢直接读 child[]） */
    TrieNode *solo = trie_node_new();
    if (solo != NULL) {
        int nulls = 0;
        for (int i = 0; i < TRIE_ALPHABET; i++) {
            if (solo->child[i] == NULL) {
                nulls++;
            }
        }
        printf("\n  刚 calloc 出来的空节点：%d 个孩子格子全是 NULL（%d 个），pass=%d end=%d\n",
               nulls, nulls, solo->pass_count, solo->end_count);
        printf("  ⚠ 如果这里用的是 malloc（不清零），child[] 里会是随机地址，\n");
        printf("    下一次插入就会「钻仰空子」走进一块不属于树的内存 —— 这是本节最容易犯的错。\n");
        free(solo);
    }
}

/*
 * =====================================================================
 * 第 2 幕：插 5 个词，每插一个都看清"新建了几个节点、哪些计数器涨了"
 * =====================================================================
 */
static void act_insert(TrieNode **trie)
{
    section("第 2 幕：插入 cat / car / card / do / door，逐词看计数器");

    const char *words[] = { "cat", "car", "card", "do", "door" };
    int n = (int)(sizeof(words) / sizeof(words[0]));

    for (int k = 0; k < n; k++) {
        int before = trie_count_nodes(*trie);       /* 插入前全树几个节点（O(节点数) 的遍历） */
        int ok = trie_insert(trie, words[k]);
        int after = trie_count_nodes(*trie);
        size_t len = strlen(words[k]);
        /* 第一次插入会连根一起新建，所以"新建的字符节点"要把根剔出去数 */
        int root_new = (before == 0) ? 1 : 0;
        int letter_new = (after - before) - root_new;

        printf("\n  插 \"%s\"（%zu 个字符）：返回 %d，节点 %d → %d，新建 %d 个字符节点%s，复用 %d 个已有格子\n",
               words[k], len, ok, before, after, letter_new,
               root_new ? "＋1 个根" : "", (int)len - letter_new);
        show_prefix_counters(*trie, words[k]);
    }

    printf("\n  两个立刻能看到的规律：\n");
    printf("    1) 插 \"card\" 只新建 1 个节点：c、a、r 三格早就存在，直接顺着走 ——\n");
    printf("       「共用前缀」不是优化，是 Trie 的定义本身。这就是它能做前缀查询的原因。\n");
    printf("    2) 每个节点的 pass_count = 从根走到它这一步，一共被插过多少个串；\n");
    printf("       end_count 只在串的最后一个字符那里 > 0。\"ca\" 那格 pass=3 end=0 —— ca 不是词。\n");
    printf("  根节点也有 pass_count（=%d）：它代表空前缀，所有串都从它身上过。\n",
           (*trie)->pass_count);
}

/*
 * =====================================================================
 * 第 3 幕：把整棵树打印出来 —— 这一节的命根子
 * =====================================================================
 */
static void act_print_tree(const TrieNode *trie)
{
    section("第 3 幕：整棵树摊开看（缩进 = 深度，'*' = 这里有单词结束）");

    printf("      每行 = 一个节点：缩进深度 = 这个前缀有几个字符；'*' = 有单词在这结束；\n");
    printf("      pass = 有几个串经过这格，end = 有几个串在这格结束；行尾 = 从根走到这格的完整前缀。\n\n");
    trie_print_tree(trie);

    printf("\n      读法示范（对着上面那几行看）：\n");
    printf("        · 第三层那行 *'r'  pass=2 end=1，行尾写着 完整单词 \"car\"：\n");
    printf("          pass 的 2 = car 自己 + card（card 从 r 这格穿过）；end 的 1 = 只有 car 在这结束。\n");
    printf("          它既是别人的前缀，自己也是个词 —— 所以判断「是不是词」只能看 end_count，\n");
    printf("          看「有没有孩子」是错的（'r' 有孩子 'd'，car 照样是词）。\n");
    printf("        · 第二层那行 'a'  pass=3 end=0（没有星号）：路径是 c→a，也就是前缀 \"ca\"。\n");
    printf("          3 个串（car/card/cat）从它身上过，但它本身不是词 —— end=0 就是答案。\n");
    printf("          「以 ca 开头的词有几个」直接读这个 3，不用下去数星号。\n");
    printf("        · 第一层 'c' pass=3 和 'a' pass=3 相同：c 的孩子只有 a，所以经过 c 的都经过 a。\n");
    printf("          一旦某层数字开始变小，就说明上面分叉了（d:2 → o:2 → 再往下只剩 1）。\n");
    int stars = trie_count_words(trie);
    int nodes = trie_count_nodes(trie);
    printf("        · 星号 %d 个 = %d 个不同的词；节点共 %d 个（含根）→ 平均每个词占 %.1f 个节点。\n",
           stars, stars, nodes, (double)(nodes - 1) / (double)stars);
    printf("      · child[] 从左到右按 'a'→'z' 扫，所以这份输出【天生是字典序】，\n");
    printf("        哈希表要办到同一件事，得把键全捞出来再显式排一次序。\n");
}

/*
 * =====================================================================
 * 第 4 幕：两个计数器分别回答哪两类问题
 * =====================================================================
 */
static void act_two_counters(TrieNode **trie)
{
    section("第 4 幕：trie_search(end) vs trie_count_prefix(pass) vs trie_starts_with");

    const TrieNode *t = *trie;
    const char *probes[] = { "cat", "car", "card", "do", "door", "ca", "c", "d", "o", "zz" };
    int n = (int)(sizeof(probes) / sizeof(probes[0]));

    printf("  三种问法读的是不同的字段（word / search→end_count / starts→路通否 / count_prefix→pass_count）：\n");
    printf("  %-6s %6s %6s %12s   %s\n", "word", "search", "starts", "prefix", "说明");
    printf("  ------ ------ ------ ------------  ----\n");
    for (int i = 0; i < n; i++) {
        const char *s = probes[i];
        int end = trie_search(t, s);
        int path_ok = trie_starts_with(t, s);
        int pass = trie_count_prefix(t, s);
        const char *note = "";
        if (end > 0) {
            note = "是个完整单词";
        } else if (path_ok) {
            note = "只是半个词（前缀存在，但不是词）";
        } else {
            note = "路走不通，三种问法全是否";
        }
        printf("  %-6s %6d %6d %12d   %s\n", s, end, path_ok, pass, note);
    }

    printf("\n  现在把 \"cat\" 再插一遍，看两个计数器怎么分工：\n");
    int nodes_before = trie_count_nodes(*trie);
    trie_insert(trie, "cat");
    int nodes_after = trie_count_nodes(*trie);
    show_prefix_counters(*trie, "cat");
    printf("      节点数 %d → %d（一个都没新建：路已经在了）\n", nodes_before, nodes_after);
    printf("      search(\"cat\")=%d  ← 插了几遍就是这个数；\n", trie_search(*trie, "cat"));
    printf("      count_words=%d  ← 不同单词的个数没变（还是 5 个）；\n", trie_count_words(*trie));
    printf("      count_prefix(\"ca\")=%d ← 从 3 变 4：又有一个串经过 ca。\n",
           trie_count_prefix(*trie, "ca"));
    printf("  ⚠ 这里能看出 end_count 必须是 int 而不是 bool：重复插入只要把数加上去，\n");
    printf("    删除时减回来，减到 0 才真的不算词。用 bool 的话「插两遍删一遍」会把词误删。\n");

    printf("\n  「有多少串以 ca 开头」= ca.pass_count = %d，代价是走 2 步。\n",
           trie_count_prefix(*trie, "ca"));
    printf("  如果没有 pass_count，这个问题必须 DFS 整棵 ca 子树数星号 —— 子树越大越慢。\n");
    printf("  第 7 幕会用 676 个词的树证明：这一行的耗时和词数完全无关。\n");
}

/*
 * =====================================================================
 * 第 5 幕：自动补全（DFS + 回溯 + 就地路径缓冲区）
 * =====================================================================
 */
static void act_autocomplete(const TrieNode *trie)
{
    section("第 5 幕：自动补全 —— 给前缀，把所有完整单词倒出来");

    const char *prefixes[] = { "do", "ca", "c", "card", "z", "door" };
    int n = (int)(sizeof(prefixes) / sizeof(prefixes[0]));

    for (int i = 0; i < n; i++) {
        /* out 是【栈上的定长二维数组】：6 行 × TRIE_WORD_MAX 字节，一次 malloc 都没有。
         * 传进接口的是"你能装这么多，别超"，函数只负责不写爆它。 */
        char out[6][TRIE_WORD_MAX];
        int pass = trie_count_prefix(trie, prefixes[i]);
        int found = trie_autocomplete(trie, prefixes[i], out, 6);
        printf("  补全 \"%s\"（pass_count=%d，补出 %d 条）→", prefixes[i], pass, found);
        if (found == 0) {
            printf(" （空：前缀本身就不存在，walk 第一步就断了）\n");
        } else {
            for (int k = 0; k < found; k++) {
                printf(" \"%s\"", out[k]);
            }
            printf("\n");
        }
    }
    printf("  ⚠ 注意 \"ca\" 那行：pass_count=4 但只补出 3 条 ——\n");
    printf("    pass 数的是【串】（cat 插了两遍算 2 个），补全吐的是【不同的词】（节点只有一个）。\n");
    printf("    两个数都没错，问的是两个问题。读 Trie 的代码时先把这两个单位分清楚。\n");

    printf("\n  DFS 的路径缓冲区是怎么「写一下、退一步」的（前缀 \"do\"，缩进 = 递归深度）：\n");
    printf("      path=\"do\"    len=2  站在 do 节点：end=1 → 先抄走 \"do\"\n");
    printf("      path=\"doo\"   len=3  填 path[2]='o' 后进孩子\n");
    printf("      path=\"door\"  len=4  填 path[3]='r'，end=1 → 抄走 \"door\"\n");
    printf("      返回 len=3、再返回 len=2 —— 没有 memset、没有清零：\n");
    printf("      \"退回上一层\"= 参数 len 变小，下一次填 path[len] 直接把旧字符【覆盖】掉。\n");
    printf("      如果这个 path 是全局变量，两次 trie_autocomplete 调用就会互相踩数据，\n");
    printf("      而且谁也不负责清零 —— 这就是本节禁止用全局状态的原因。\n");

    printf("\n  顺手把整棵树的单词按字典序倒出来（空前缀 = 全部）：\n");
    char all[8][TRIE_WORD_MAX];
    int total = trie_dump_words(trie, all, 8);
    printf("    ");
    for (int k = 0; k < total; k++) {
        printf("%s ", all[k]);
    }
    printf("  ← 没跑一次比较、没排一次序；拉链法想把键按字典序打出来必须先显式排序。\n");
}

/*
 * =====================================================================
 * 第 6 幕：删除 —— pass_count 一路 -1，归零的节点真的 free 掉
 * =====================================================================
 */
static void act_delete(TrieNode **trie)
{
    section("第 6 幕：删除 —— 每一步都盯着节点总数有没有变小");

    const char *doomed[] = { "card", "cat", "cat", "car" };
    int n = (int)(sizeof(doomed) / sizeof(doomed[0]));

    for (int i = 0; i < n; i++) {
        int before = trie_count_nodes(*trie);
        int ok = trie_delete(trie, doomed[i]);
        int after = trie_count_nodes(*trie);
        printf("\n  删 \"%s\"：返回 %d，节点 %d → %d（free 掉 %d 个），"
               "search(\"%s\")=%d  count_prefix(\"%s\")=%d\n",
               doomed[i], ok, before, after, before - after,
               doomed[i], trie_search(*trie, doomed[i]),
               doomed[i], trie_count_prefix(*trie, doomed[i]));
        if (strcmp(doomed[i], "card") == 0) {
            printf("  → 只有末尾的 'd' 格子归零被 free；c、a、r 的 pass 各减 1 但都 > 0，全部留下。\n");
            printf("    car 还在不在？search(\"car\")=%d ✅（card 删了不影响 car —— 它们只是共用前缀）\n",
                   trie_search(*trie, "car"));
            trie_print_tree(*trie);
        }
        if (strcmp(doomed[i], "car") == 0) {
            printf("  ★ 看节点数掉了 %d 个：删 \"car\" 除了删掉 'r' 那一格，\n", before - after);
            printf("    还顺手把 'a' 和 'c' 一起 free 了 —— 因为那时已经【没有任何串】经过它们\n");
            printf("    （cat 删干净了、card 删干净了、car 自己刚删），pass 归零 + end 归零 = 该还给系统。\n");
            printf("    判据为什么不是只看 end_count？'a' 的 end_count 一直是 0（ca 不是词），\n");
            printf("    只看 end 就永远不会去 free 它 —— 内存白占着，这就是「Trie 只涨不缩」的成因。\n");
            trie_print_tree(*trie);
        }
    }

    printf("\n  现在树里只剩 \"door\"（一个串）。再试两件反直觉的事：\n");
    printf("  ① 删一个【不存在】的词：\n");
    int before = trie_count_nodes(*trie);
    int ok = trie_delete(trie, "cat");
    printf("     trie_delete(\"cat\") 返回 %d，节点 %d → %d：先 search 确认存在，\n",
           ok, before, trie_count_nodes(*trie));
    printf("     不然 pass_count 会被减成【负数】，节点永远归不了零 → 树只涨不缩，且不报错。\n");

    printf("  ② 删 \"do\"（它是 door 的前缀，本身也是个词）：\n");
    ok = trie_delete(trie, "do");
    printf("     返回 %d；search(\"do\")=%d 但 starts_with(\"do\")=%d、count_prefix(\"do\")=%d\n",
           ok, trie_search(*trie, "do"), trie_starts_with(*trie, "do"),
           trie_count_prefix(*trie, "do"));
    printf("     → \"do\" 这个词没了，但 'o' 那个节点【必须留着】：door 还从它身上过（pass=1）。\n");
    printf("       这就是 pass/end 双计数器在删除时的分工：end 决定\"词还在不在\"，\n");
    printf("       pass 决定\"节点还在不在\"。两件事完全独立，一个字段办不了。\n");
    trie_print_tree(*trie);

    printf("\n  删掉的节点去哪了？node_delete 里 free 完 return NULL，父亲那一格立刻接上 NULL：\n");
    printf("    父->child[idx] = node_delete(父->child[idx], ...)   ← 摘下来的地址不会留在树里\n");
    printf("    如果忘记 return NULL，父亲就攥着一个已 free 的地址（悬空指针），\n");
    printf("    下次插入走到那一格会\"以为孩子还在\"直接钻进去 → use-after-free。\n");
}

/*
 * 对照组：只用字符串数组 + strncmp 的"朴素词表"。
 * 它做的事就是哈希表唯一会做的事：把每个键拿出来比一遍 —— 除了键本身，
 * 哈希表手里没有任何能利用"前缀"这个结构的信息。
 * cmp_out 带回"比了多少次"。为什么数比较次数而不是掐秒表：
 *   计时有噪声（CPU 频率、缓存、别的进程都在抖），比较次数是可复现的整数，
 *   这是第 08 章一开始就定下的规矩（拉链法用 probes 干的是同一件事）。
 */
static int naive_count_prefix(const char *const *words, int n,
                             const char *prefix, long *cmp_out)
{
    size_t plen = strlen(prefix);
    int hits = 0;
    long cmp = 0;
    for (int i = 0; i < n; i++) {
        cmp++;                                   /* 每个键都必须看一眼，没有捷径 */
        if (strncmp(words[i], prefix, plen) == 0) {
            hits++;
        }
    }
    if (cmp_out != NULL) {
        *cmp_out = cmp;
    }
    return hits;
}

static int naive_exact(const char *const *words, int n, const char *key, long *cmp_out)
{
    long cmp = 0;
    for (int i = 0; i < n; i++) {
        cmp++;
        if (strcmp(words[i], key) == 0) {
            if (cmp_out != NULL) {
                *cmp_out = cmp;
            }
            return 1;
        }
    }
    if (cmp_out != NULL) {
        *cmp_out = cmp;
    }
    return 0;
}

/*
 * =====================================================================
 * 第 7 幕：规模对照 —— 复杂度是 O(串长 L)，不是 O(桶数 / 词数)
 * =====================================================================
 */
static void act_scale(void)
{
    section("第 7 幕：676 个两字母词 —— 前缀查询的代价还是「走 2 步」");

    /* 造 676 个词：aa ab ... az, ba ... zz（26 × 26），键全是合法小写字母 */
    enum { NWORDS = 26 * 26 };
    char words[NWORDS][3];
    const char *list[NWORDS];
    for (int a = 0; a < 26; a++) {
        for (int b = 0; b < 26; b++) {
            int k = a * 26 + b;
            words[k][0] = (char)('a' + a);
            words[k][1] = (char)('a' + b);
            words[k][2] = '\0';
            list[k] = words[k];
        }
    }

    TrieNode *trie = NULL;
    for (int k = 0; k < NWORDS; k++) {
        trie_insert(&trie, list[k]);
    }
    int nodes_total = trie_count_nodes(trie);
    printf("  插完 %d 个词：节点 %d 个（理论 = 1 个根 + 26 个首字母 + %d 个末端 = %d）\n",
           NWORDS, nodes_total, NWORDS, 1 + 26 + NWORDS);
    printf("  字符总数 = %d × 2 = %d，节点数 ≈ 字符数 + 1 —— Trie 的规模单位是【字符】，不是【词】。\n",
           NWORDS, NWORDS * 2);

    long cmp = 0;
    int naive_hits = naive_count_prefix(list, NWORDS, "c", &cmp);
    int steps = 0;
    trie_walk_counted(trie, "c", &steps);
    printf("\n  问「以 c 开头的词有几个」（答案应该是 ca cb cc ... cz 共 26 个）：\n");
    printf("    朴素扫描/哈希表：比了 %ld 个键才得出答案 = %d 个        ← O(n × L)\n", cmp, naive_hits);
    printf("    Trie：走 %d 步，读 c 节点的 pass_count = %d 个          ← O(L)，和 n 无关\n",
           steps, trie_count_prefix(trie, "c"));

    naive_count_prefix(list, NWORDS, "ca", &cmp);
    trie_walk_counted(trie, "ca", &steps);
    printf("    换个更长的前缀 \"ca\"：朴素还是比 %ld 次（一个都省不掉），Trie 走 %d 步 → %d 个。\n",
           cmp, steps, trie_count_prefix(trie, "ca"));

    cmp = 0;
    int zz_hit = naive_exact(list, NWORDS, "zz", &cmp);
    int steps2 = 0;
    trie_walk_counted(trie, "zz", &steps2);
    printf("  问「zz 在不在」（精确查，哈希表也擅长）：命中=%d。\n", zz_hit);
    printf("    朴素扫描：%ld 次 strcmp（zz 排在最后 → 比满整张表）；Trie：%d 步（= 串长）。\n", cmp, steps2);
    printf("    哈希表这一格也是 O(1) —— 所以【只做精确查就别上 Trie】，那笔内存白花了。\n");

    cmp = 0;
    int miss = naive_count_prefix(list, NWORDS, "zzz", &cmp);
    int steps3 = 0;
    int path3 = trie_walk_counted(trie, "zzz", &steps3);
    printf("    查一个【不存在】的前缀 \"zzz\"：朴素扫描比了 %ld 次才敢说 0 个；\n", cmp);
    printf("      Trie 走 %d 步就撞上空格子 → 走通了吗=%d，立即收工（%d 个命中）。\n", steps3, path3, miss);
    printf("      → 失败也是 O(L)。哈希表判断「不存在」同样便宜，但判断「没有这个前缀」\n");
    printf("        必须把整张表扫一遍，因为它不知道 z→z 这条路还能不能再往下长。\n");

    printf("\n  规模不变性验证：同一棵树里问 2 个字符和问 1 个字符的前缀，步数分别是：\n");
    for (int q = 0; q < 26; q += 25) {          /* 只测 'a' 和 'z' 两个首字母，省得刷屏 */
        char one[2] = { (char)('a' + q), '\0' };
        char two[3] = { (char)('a' + q), 'm', '\0' };
        int s1 = 0;
        int s2 = 0;
        trie_walk_counted(trie, one, &s1);
        trie_walk_counted(trie, two, &s2);
        printf("    前缀 \"%s\"：%d 步 → %2d 个串经过；前缀 \"%s\"：%d 步 → %2d 个串经过\n",
               one, s1, trie_count_prefix(trie, one), two, s2, trie_count_prefix(trie, two));
    }
    printf("    → 词表 676 个词、703 个节点，步数还是等于前缀长度。这就是 O(L) 的字面意思。\n");

    trie_print_stats(trie, "676 个两字母词");

    printf("\n  最后把 %d 个词【全删光】，看树会不会自己缩回去：\n", NWORDS);
    for (int k = 0; k < NWORDS; k++) {
        trie_delete(&trie, list[k]);
    }
    printf("    删完：trie_count_nodes = %d 个节点，root 指针 = %s\n",
           trie_count_nodes(trie), (trie == NULL) ? "NULL（树自己缩没了）" : "还指着东西");
    printf("    → 0 个节点、root 是 NULL：最后一个串删掉时，根的 pass 也归零，被 node_delete free。\n");
    printf("      这就是 trie_delete 为什么收 TrieNode **root —— 它要把调用者的根改成 NULL。\n");
    printf("      此刻再调 trie_free(trie) 就是对 NULL 的空操作（合法），不会 double free。\n");
    trie_free(trie);
    trie = NULL;
}

/*
 * =====================================================================
 * 第 8 幕：空间账单 + 非法输入（越界写的那道墙）
 * =====================================================================
 */
static void act_cost_and_walls(void)
{
    section("第 8 幕：这棵树到底值多少钱 / 什么输入会被拒");

    TrieNode *trie = NULL;
    const char *words[] = { "cat", "car", "card", "do", "door" };
    for (int i = 0; i < (int)(sizeof(words) / sizeof(words[0])); i++) {
        trie_insert(&trie, words[i]);
    }

    trie_print_stats(trie, "cat/car/card/do/door");
    printf("\n  对照上一节的拉链法哈希表（同样存这 5 个串）：\n");
    printf("    哈希表：5 个链表节点（各 ~24 字节）+ 桶数组 + 键副本 ≈ 几百字节量级，会做的只有【整串精确查】。\n");
    printf("    Trie  ：%d 个节点 × %zu 字节 = %zu 字节，贵好几倍，换来前缀查询 / 补全 / 字典序遍历。\n",
           trie_count_nodes(trie), sizeof(TrieNode),
           (size_t)trie_count_nodes(trie) * sizeof(TrieNode));
    printf("    → 值不值，取决于你到底要不要问「以 … 开头」。只要精确查就用哈希表；\n");
    printf("      要问前缀，哈希表唯一的办法是把每个键都拿出来比一遍（第 7 幕实测 676 次比较）。\n");

    printf("\n  现在故意喂非法输入（每一次都试图算出越界的 child 下标，被 word_is_valid 拦下）：\n");
    /* 造一个"全是合法小写字母、但太长"的串：只有长度检查会拦住它 */
    char big[TRIE_WORD_MAX + 8];
    memset(big, 'a', sizeof(big));
    big[(int)sizeof(big) - 1] = '\0';
    trie_insert(&trie, NULL);          /* 空指针 */
    trie_insert(&trie, "");            /* 空串：对应节点是根，语义混乱 */
    trie_insert(&trie, "Cat");         /* 'C'(67) - 'a'(97) = -30 → 负下标 */
    trie_insert(&trie, "cat!");        /* '!'(33) - 'a' = -64 → 负下标 */
    trie_insert(&trie, "cat7");        /* '7'(55) - 'a' = -42 → 负下标 */
    trie_insert(&trie, "{");           /* '{'(123) - 'a' = 26 → 正好越过 26 格的上界 */
    printf("  拒绝之后树里的词数还是 %d 个 —— 墙有效：写操作一个字节都没漏进去。\n",
           trie_count_words(trie));
    printf("  只读接口同样安全（走不通就说没有，不会去解引用非法格子）：\n");
    printf("    search(\"Cat\")=%d  starts_with(\"ca!\")=%d  count_prefix(\"do7\")=%d\n",
           trie_search(trie, "Cat"), trie_starts_with(trie, "ca!"),
           trie_count_prefix(trie, "do7"));

    printf("\n  一个「看着合法但其实很伤」的输入：超长串。合法上限是 %d 个字符，我造 %d 个：\n",
           TRIE_WORD_MAX - 1, (int)(sizeof(big) - 1));
    trie_insert(&trie, big);
    printf("  为什么连它也要拒：路径缓冲区是栈上的 char path[%d]，\n", TRIE_WORD_MAX);
    printf("  没有长度上限，DFS 往下钻就会写到缓冲区外面 —— 栈溢出/覆盖返回地址，比越界写数组更致命。\n");

    trie_free(trie);
    printf("  本幕的树已 trie_free 并把指针置 NULL（ASan 会来查岗）。\n");
    trie = NULL;
}

/*
 * =====================================================================
 * 第 9 幕：形状总结 + 收摊
 * =====================================================================
 */
static void act_wrapup(TrieNode **trie)
{
    section("第 9 幕：形状总结 + trie_free（后序递归）");

    printf("  三种结构解决同一个「按键找」问题，差别就在键被怎么处理：\n\n");
    printf("    数组     键 = 下标 0..n-1        → O(1)，但键必须是连号整数\n");
    printf("    哈希表   键 = 揉成一个大整数      → O(1) 精确查；顺序信息全丢，问不了前缀\n");
    printf("    Trie     键 = 拆成一位一位       → O(L) 精确查 + 前缀 + 补全 + 字典序；每节点 %zu 字节\n\n",
           sizeof(TrieNode));
    printf("    二叉树   键 = 比较大小一步步分左右 → O(log n) 精确查 + 范围查，前缀查不如 Trie\n\n");

    printf("  主树还剩这些节点（第 6 幕删到只剩 door）：\n");
    trie_print_tree(*trie);

    int nodes = trie_count_nodes(*trie);
    printf("\n  收摊：trie_free(root) 要 free %d 个节点。\n", nodes);
    printf("  顺序必须是【后序】—— 每层的动作是「先把 26 棵子树递归拔掉，再 free 自己」。\n");
    printf("  为什么不能先 free 自己：孩子的地址就存在我自己的 child[] 数组里，\n");
    printf("  free(node) 之后那块内存已经还给系统，再读 node->child[i] 就是 use-after-free；\n");
    printf("  更糟的是它「经常看起来没事」—— glibc 只是把这块地登记成空闲，老数据还躺在原地，\n");
    printf("  于是错误版本能跑一百次都对，某一次 malloc 复用了它，树就长出随机分支。\n");
    trie_free(*trie);
    *trie = NULL;      /* ★ 悬空指针当场掐掉：再误用会立刻崩，而不是悄悄算错 */
    printf("  已 trie_free 并置 NULL：free 次数 = 节点数 = %d，ASan 应该报 0 字节泄漏。\n", nodes);
}

int main(void)
{
    TrieNode *trie = NULL;   /* 第 2~6、9 幕共用的那棵树，唯一的入口指针 */

    printf("======== 08-hash / trie：前缀树（Trie）========\n");
    printf("键：char *（只许小写字母）    值：一律 int —— 就是那两个计数器\n");

    act_node_anatomy();
    act_insert(&trie);
    act_print_tree(trie);
    act_two_counters(&trie);
    act_autocomplete(trie);
    act_delete(&trie);
    act_scale();
    act_cost_and_walls();
    act_wrapup(&trie);

    printf("\n=== 本节结束 ===\n");
    printf("记住三句话：\n");
    printf("  1) pass_count 是「几个串经过我」，end_count 是「几个串在我这结束」；\n");
    printf("     前者答「以…开头的有几个」，后者答「它本身是不是一个词」。\n");
    printf("  2) 复杂度只看串长 L：插入/查询/删除/前缀计数都是 O(L)，和词表里有多少词无关；\n");
    printf("     代价是每个节点 26 个指针（%zu 字节），绝大多数格子是 NULL —— 空间换时间。\n", sizeof(TrieNode));
    printf("  3) 删除要一路把 pass_count 减 1，并把 pass 归零的节点真的 free 掉；\n");
    printf("     销毁整棵树必须后序递归 free —— 先删孩子，再删自己。\n");
    return 0;
}
