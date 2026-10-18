#include <stdio.h>
#include <stdlib.h>
#include <string.h>   /* strlen / strcmp：本节只用来搬运字符，不用来比大小 */
#include "huffman.h"

/*
 * ===================================================================
 * huffman.c —— 哈夫曼树 + 哈夫曼编码
 * ===================================================================
 * 分组：
 *   第 0 组：本目录自带的极简最小堆（不去 include 06-heap，目录必须自包含）
 *   第 1 组：小工具（频次统计、字符怎么显示）
 *   第 2 组：建树（统计 → 入堆 → 两两合并 → 填编码表）
 *   第 3 组：查看产物（编码表 / 树形 / WPL / 前缀码自检）
 *   第 4 组：编码、解码 —— 闭环
 *   第 5 组：销毁
 */

/* ===================================================================
 * 第 0 组：最小堆
 * ===================================================================
 * 为什么这里要写一个堆？
 *   哈夫曼建树每一轮都要"取出权重最小的两个节点"。
 *   · 全表扫描找最小：一轮 O(n)，总共 O(n²)
 *   · 最小堆：一轮 O(log n)，总共 O(n log n)
 *   而且"每次都要最小的那个"这种需求，堆就是它的标准答案。
 *
 * 堆的三条铁律（对照 06-heap 的写法，这里只留够用的一半）：
 *   1) 形状：用数组表示完全二叉树。下标 i 的父亲是 (i-1)/2，
 *      孩子是 2i+1 和 2i+2。不用任何指针，纯下标算术。
 *   2) 顺序：父节点权重 ≤ 两个孩子权重。注意【只保证父子关系】，
 *      同层之间、跨层之间都不保证有序 —— 所以"打印堆"不等于"打印排序结果"，
 *      下面 huff_heap_dump 特意把这个区别标注出来，很多人第一次看会以为写错了。
 *   3) 只有堆顶（item[0]）是全局最小 —— 而哈夫曼恰好只反复要堆顶，完美匹配。
 *
 * ⚠ 本仓库硬约定：不写函数指针比较器。所以这里比大小就是 `a->weight < b->weight`
 *   写死在代码里 —— 想改成最大堆，就把 < 换成 >（两处：上浮、下沉）。
 */

void huff_heap_init(HuffMinHeap *h)
{
    h->size = 0;
    for (int i = 0; i < HUFF_HEAP_CAP; i++) {
        h->item[i] = NULL;   /* 全部置空：将来 pop 出来的必然是真节点，不会读到垃圾 */
    }
}

/*
 * push：新节点放到数组末尾，然后【上浮】—— 只要比父亲小就和父亲换位置。
 *
 *        上浮前（要放 1）             上浮中                    上浮后
 *              2                        2                        4
 *             / \                      / \                      / \
 *            3   5                    3   5        1<3 换!      1   5
 *           /                        /                        / \
 *          (空)←1 放这里            1                          3   ...
 *
 * 为什么上浮最多 log n 次？数组是一棵完全二叉树，末尾元素的深度就是树高。
 */
int huff_heap_push(HuffMinHeap *h, HuffNode *node)
{
    if (node == NULL) {
        return 0;
    }
    if (h->size >= HUFF_HEAP_CAP) {
        fprintf(stderr, "      [错误] 堆满了（容量 %d）\n", HUFF_HEAP_CAP);
        return 0;
    }

    int pos = h->size;                 /* 先放在数组末尾 = 完全二叉树最左下的空位 */
    h->item[pos] = node;
    h->size++;

    while (pos > 0) {
        int parent = (pos - 1) / 2;
        if (h->item[parent]->weight <= h->item[pos]->weight) {
            break;                     /* 父 ≤ 子，堆性质已经满足，收工 */
        }
        /* 交换一对指针即可：节点本身不用搬，只换"纸条上的地址" */
        HuffNode *tmp = h->item[parent];
        h->item[parent] = h->item[pos];
        h->item[pos] = tmp;
        pos = parent;                  /* 继续往上比 */
    }
    return 1;
}

/*
 * pop：拿走堆顶（最小），把【数组最后一个元素】填到堆顶，然后【下沉】。
 *
 *   为什么填"最后一个"？因为它是唯一能保证"拿走之后形状还是完全二叉树"的元素。
 *   下沉：拿它和【较小的那个孩子】比，比孩子大就换下去，一路到不再违规为止。
 *
 *          堆顶 1 要拿走，尾部 7 补到堆顶
 *                1                       7
 *               / \                     / \
 *              3   5       ──►         3   5     ← 7 比两个孩子都大，和孩子里最小的 3 换
 *             / \                     / \
 *            7   4                   1?  4       → 换成 3 / 7 之后下面还有孩子就继续比
 *
 * 空堆返回 NULL —— 调用方必须判空（本文件里由 huff_build 保证：只在 size≥2 时 pop）。
 */
HuffNode *huff_heap_pop(HuffMinHeap *h)
{
    if (h->size <= 0) {
        return NULL;
    }

    HuffNode *min = h->item[0];
    h->size--;
    h->item[0] = h->item[h->size];     /* 尾部元素搬到堆顶 */
    h->item[h->size] = NULL;           /* 擦干净：别让已经出堆的地址继续留在数组里 */

    int pos = 0;
    for (;;) {
        int left  = 2 * pos + 1;
        int right = 2 * pos + 2;
        int small = left;              /* 先假设左孩子较小 */

        if (left >= h->size) {
            break;                     /* 没有左孩子 = 已是叶子位置，下沉结束 */
        }
        if (right < h->size && h->item[right]->weight < h->item[left]->weight) {
            small = right;             /* 右孩子存在且更小 → 跟右孩子换 */
        }
        if (h->item[pos]->weight <= h->item[small]->weight) {
            break;                     /* 已经 ≤ 最小的那个孩子，性质恢复 */
        }
        HuffNode *tmp = h->item[pos];
        h->item[pos]   = h->item[small];
        h->item[small] = tmp;
        pos = small;
    }
    return min;
}

/*
 * 把堆"按层"打印出来，让人一眼看出它是一棵树，而不是一排数字。
 * 数组下标的层级分布：第 0 层 [0]，第 1 层 [1..2]，第 2 层 [3..6]，第 3 层 [7..14]…
 * 每层的下标范围就是 [2^k - 1, 2^(k+1) - 2]，所以循环里每轮把边界乘 2。
 */
void huff_heap_dump(const HuffMinHeap *h)
{
    if (h->size == 0) {
        printf("        堆：空\n");
        return;
    }
    printf("        堆里 %d 个节点（按层看，父 ≤ 子；同层之间【不保证】有序）：\n", h->size);
    int level_start = 0;
    int level_count = 1;                      /* 本层最多几个格子 */
    int level = 0;
    while (level_start < h->size) {
        int level_end = level_start + level_count;
        if (level_end > h->size) {
            level_end = h->size;              /* 最后一层没填满 */
        }
        printf("          第 %d 层: ", level + 1);
        for (int i = level_start; i < level_end; i++) {
            const HuffNode *n = h->item[i];
            printf("[%d:权重%d]", i, n->weight);
        }
        printf("\n");
        level_start = level_end;
        level_count *= 2;
        level++;
    }
    printf("          堆顶 item[0] = 权重 %d ← 这就是当前全局最小，pop 就是拿它\n",
           h->item[0]->weight);
}

/* ===================================================================
 * 第 1 组：小工具
 * =================================================================== */

/*
 * 造一个叶子。权重就是"这个字符出现了几次"。
 * 用 calloc 是为了 left/right 一定是 NULL —— 忘置 NULL 是本节第二个致命 bug：
 * huff_print_tree 一路往 left 递归时会撞上垃圾指针段错误。
 */
static HuffNode *make_leaf(unsigned char ch, int weight)
{
    HuffNode *n = calloc(1, sizeof(HuffNode));
    if (n == NULL) {
        fprintf(stderr, "      [致命] 造叶子失败，堆内存不够\n");
        return NULL;
    }
    n->ch      = ch;
    n->weight  = weight;
    n->is_leaf = 1;
    n->left    = NULL;
    n->right   = NULL;
    return n;
}

/* 造一个内部节点（合并的结果）：它不代表任何字符，只带"两个孩子的权重和"。 */
static HuffNode *make_inner(HuffNode *a, HuffNode *b)
{
    HuffNode *n = calloc(1, sizeof(HuffNode));
    if (n == NULL) {
        fprintf(stderr, "      [致命] 合并时造内部节点失败\n");
        return NULL;
    }
    n->weight  = a->weight + b->weight;   /* 合并的定义就是权重相加 */
    n->ch      = 0;                       /* 内部节点没有字符（也避免和 '\0' 这个字符混淆，见坑）*/
    n->is_leaf = 0;
    n->left    = a;
    n->right   = b;
    return n;
}

/*
 * 把字符显示成人能读的样子：
 *   空格 → "空格"，换行 → "\\n"，不可打印字节 → "0x01"，其余原样。
 * 为什么要专门写这个？因为哈夫曼 demo 的文本里【空格是最常见的字符】，
 * 直接 printf("%c") 打出来就是一个看不见的洞，表格会对不上。
 */
static const char *char_repr(unsigned char c, char *buf, size_t buf_size)
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
 * 频次统计：freq_out[256] 全清零，然后扫一遍文本，每见一个字符就 ++。
 * 返回"不同字符的个数"，后面算定长编码位数要用它。
 *
 * ⚠ 下标必须写 (unsigned char)c：char 可能是 signed 的，
 *   一个 ≥0x80 的字节会变成负数，拿它当下标就是【数组越界】。
 *   这是 C 语言处理"字节"最经典的坑，本文件所有用字符当下标的地方都加了转换。
 */
int huff_count_chars(const char *text, int freq_out[HUFF_CHAR_SET])
{
    for (int i = 0; i < HUFF_CHAR_SET; i++) {
        freq_out[i] = 0;
    }
    if (text == NULL) {
        return 0;
    }
    for (const char *p = text; *p != '\0'; p++) {
        freq_out[(unsigned char)*p]++;
    }

    int distinct = 0;
    for (int i = 0; i < HUFF_CHAR_SET; i++) {
        if (freq_out[i] > 0) {
            distinct++;
        }
    }
    return distinct;
}

/* ===================================================================
 * 第 2 组：建树
 * =================================================================== */

/*
 * assign_codes：从根往下 DFS，左拐记 '0'、右拐记 '1'，走到叶子就把整条路径登记进表。
 *
 * 为什么"哈夫曼编码一定是前缀码"？—— 这段递归本身就是答案：
 *   我们【只在叶子处登记编码】，而"从根到叶子的路径"途中不可能再经过另一个叶子
 *   （叶子没有孩子）。所以任何一条编码都不可能是另一条的开头。
 *   反过来想：如果把某个字符放在内部节点上，它的路径就会是它子孙路径的前缀 —— 冲突。
 *   这就是"所有字符必须放叶子上"这条规矩的真正理由。
 *
 * 边界情况：整段文本只有一个种类的字符（比如 "aaaa"）时，根本身就是叶子，
 *   depth = 0，会得到长度 0 的编码 —— 那意味着"这个字符不占任何比特"，
 *   解码时根本没法知道它出现了几次。所以这里特判成 1 位的 "0"。
 */
static void assign_codes(HuffNode *node, char *path, int depth, HuffCodeEntry table[])
{
    if (node == NULL) {
        return;
    }
    if (node->is_leaf) {
        if (depth == 0) {
            /* 单字符文本的极端情况，见函数头 */
            table[node->ch].code_len = 1;
            snprintf(table[node->ch].code, HUFF_MAX_CODE_LEN, "0");
        } else {
            path[depth] = '\0';                    /* 把路径当字符串截断 */
            table[node->ch].code_len = depth;
            snprintf(table[node->ch].code, HUFF_MAX_CODE_LEN, "%s", path);
        }
        return;
    }
    if (depth >= HUFF_MAX_CODE_LEN - 1) {
        printf("      [警告] 树太深，编码长度超过 %d 位，后面的字符没登记上\n",
               HUFF_MAX_CODE_LEN - 1);
        return;
    }
    path[depth] = '0';
    assign_codes(node->left,  path, depth + 1, table);
    path[depth] = '1';                             /* 注意：同一个 buffer 复用，走右边时覆盖这一格 */
    assign_codes(node->right, path, depth + 1, table);
}

/*
 * huff_build：统计 → 全入堆 → 两两合并 → 剩一个就是根 → 填编码表。
 *
 * 三步走，每一步都打印，因为"建树过程"就是本节要看的东西：
 *
 *  第 1 步 频次统计。假设文本 "aabcd"：
 *          a:2  b:1  c:1  d:1   →  4 个叶子丢进最小堆
 *
 *  第 2 步 合并（每次取【两个】最小的）：
 *          堆: [b1 c1 d1 a2]  → 取 b1、c1 → 新节点 2 → 堆: [d1 a2 (bc)2]
 *          堆: [d1 a2 (bc)2]  → 取 d1、a2 → 新节点 3 → 堆: [(bc)2 (da)3]
 *          堆: [(bc)2 (da)3]  → 取 (bc)2、(da)3 → 新节点 5 → 堆: [(5)]  ← 只剩一个，它是根
 *          合并产生的权重：2 + 3 + 5 = 10 = WPL（考点里那条等价结论，这里边算边验证）
 *
 *  第 3 步 走树填表：b=00 c=01 d=10 a=11（左 0 右 1）
 *
 * ⚠ 关键细节：第 2 步里 pop 两次得到 a、b，谁当左孩子？本实现"先 pop 的当左"。
 *   这会导致不同的树形状（同权重交换、左右互换都能得到另一棵合法的哈夫曼树），
 *   WPL 完全不变，编码串却不同。README「常见的坑」#4 讲了这点，
 *   考试判卷看的是 WPL，不是编码串一模一样。
 */
HuffNode *huff_build(const char *text, HuffCodeEntry table[],
                     int *distinct_out, long *merge_sum_out)
{
    int freq[HUFF_CHAR_SET];
    int distinct = huff_count_chars(text, freq);

    for (int i = 0; i < HUFF_CHAR_SET; i++) {   /* 先把整张编码表清空 */
        table[i].freq = 0;
        table[i].code_len = 0;
        table[i].code[0] = '\0';
    }
    if (distinct_out != NULL) {
        *distinct_out = distinct;
    }
    if (merge_sum_out != NULL) {
        *merge_sum_out = 0;
    }

    printf("    第 1 步 统计频次：文本共 %zu 个字符，出现了 %d 种不同字符\n",
           text == NULL ? (size_t)0 : strlen(text), distinct);
    if (distinct == 0) {
        printf("      空文本，不建树，返回 NULL\n");
        return NULL;
    }
    printf("      字符 → 频次： ");
    for (int i = 0; i < HUFF_CHAR_SET; i++) {
        if (freq[i] > 0) {
            char rb[8];
            printf("%s:%d  ", char_repr((unsigned char)i, rb, sizeof rb), freq[i]);
        }
    }
    printf("\n");

    /* ---- 第 2 步：全部入堆 ---- */
    HuffMinHeap heap;
    huff_heap_init(&heap);
    for (int i = 0; i < HUFF_CHAR_SET; i++) {
        if (freq[i] == 0) {
            continue;
        }
        HuffNode *leaf = make_leaf((unsigned char)i, freq[i]);
        if (leaf == NULL) {
            return NULL;                 /* 造不出叶子就没戏，前面没有需要回滚的资源 */
        }
        if (!huff_heap_push(&heap, leaf)) {
            free(leaf);                  /* 堆装不下：这个叶子还没挂到任何地方，谁申请谁负责 */
            return NULL;
        }
    }
    printf("    第 2 步 造 %d 个叶子，全部塞进最小堆：\n", distinct);
    huff_heap_dump(&heap);

    /* ---- 第 2 步（续）：反复合并，直到只剩一个节点 ---- */
    long merge_sum = 0;
    int round = 0;
    while (heap.size > 1) {
        round++;
        printf("      ── 第 %d 轮：堆里还有 %d 个节点，取走最小的两个 ──\n", round, heap.size);

        HuffNode *a = huff_heap_pop(&heap);        /* 全局最小 */
        HuffNode *b = huff_heap_pop(&heap);        /* 次小（a 拿走之后新的堆顶）*/
        if (a == NULL || b == NULL) {
            printf("      [错误] 堆里节点不够了，逻辑不该走到这里\n");
            return NULL;
        }
        HuffNode *merged = make_inner(a, b);
        if (merged == NULL) {
            /* 把两个节点塞回去，保持堆和树的ownership一致，再退出 */
            huff_heap_push(&heap, a);
            huff_heap_push(&heap, b);
            return NULL;
        }
        merge_sum += merged->weight;               /* 考点：WPL == 所有合并权重之和 */

        char ra[8], rb2[8];
        printf("        取出 权重%d(%s) 和 权重%d(%s) → 合并成新内部节点 权重%d\n",
               a->weight, a->is_leaf ? char_repr(a->ch, ra, sizeof ra) : "子树",
               b->weight, b->is_leaf ? char_repr(b->ch, rb2, sizeof rb2) : "子树",
               merged->weight);
        if (!huff_heap_push(&heap, merged)) {
            free(merged);
            return NULL;
        }
        huff_heap_dump(&heap);
    }

    HuffNode *root = huff_heap_pop(&heap);         /* 只剩一个，它就是根 */
    printf("      堆里只剩 1 个节点 → 它就是根，权重 %d（= 文本总字符数）\n",
           root->weight);
    if (merge_sum_out != NULL) {
        *merge_sum_out = merge_sum;
    }
    printf("      各轮合并权重之和 = %ld（下面会和 WPL 对一下，两者必须相等）\n", merge_sum);

    /* ---- 第 3 步：走树，把编码填进表 ---- */
    for (int i = 0; i < HUFF_CHAR_SET; i++) {
        table[i].freq = freq[i];                   /* 频次先写进表，打印表格要用 */
    }
    char path[HUFF_MAX_CODE_LEN];
    assign_codes(root, path, 0, table);
    printf("    第 3 步 从根往下走，左拐记 0、右拐记 1，走到叶子就登记编码\n");

    return root;
}

/* ===================================================================
 * 第 3 组：查看产物
 * =================================================================== */

/* 打印"字符 | 频次 | 编码 | 码长"表格，只打出现过的字符。 */
void huff_print_table(const HuffCodeEntry table[])
{
    printf("    ┌──────────┬────────┬──────────────────────────┬────────┐\n");
    printf("    │ 字符     │ 频次   │ 编码                     │ 码长   │\n");
    printf("    ├──────────┼────────┼──────────────────────────┼────────┤\n");
    for (int i = 0; i < HUFF_CHAR_SET; i++) {
        if (table[i].freq <= 0) {
            continue;
        }
        char rb[8];
        printf("    │ %-8s │ %6d │ %-24s │ %4d │\n",
               char_repr((unsigned char)i, rb, sizeof rb),
               table[i].freq, table[i].code, table[i].code_len);
    }
    printf("    └──────────┴────────┴──────────────────────────┴────────┘\n");
}

/*
 * 树形视图：和 05-tree/b-tree 一个画法 —— 缩进 + 连接符，
 * 但每条边标上 0 / 1，这样"某个字符的编码"就是沿着标签从上往下念一遍。
 *
 *   [内部 权重34]
 *   ├─ 0 → [内部 权重15]
 *   │        ├─ 0 → [叶 'e' 4]        ← 'e' 的编码 = 00
 *   │        └─ 1 → ...
 *   └─ 1 → ...
 */
#define HUFF_LINE_LEN 256

static void print_tree_rec(HuffNode *node, const char *line_prefix,
                           const char *label, int i_am_last)
{
    char rb[8];
    printf("    %s%s", line_prefix, label);
    if (node->is_leaf) {
        printf("[叶 '%s' 权重%d]", char_repr(node->ch, rb, sizeof rb), node->weight);
    } else {
        printf("[内部 权重%d]", node->weight);
    }
    printf("\n");

    if (node->is_leaf) {
        return;
    }
    /* 孩子们那一行用的缩进：我不是老小 → 下面要续一条竖线 */
    char child_prefix[HUFF_LINE_LEN];
    snprintf(child_prefix, sizeof child_prefix, "%s%s", line_prefix, i_am_last ? "    " : "│   ");

    int has_right = (node->right != NULL);
    if (node->left != NULL) {
        char lab[16];
        snprintf(lab, sizeof lab, "%s0 → ", has_right ? "├─ " : "└─ ");
        print_tree_rec(node->left, child_prefix, lab, !has_right);
    }
    if (node->right != NULL) {
        char lab[16];
        snprintf(lab, sizeof lab, "└─ 1 → ");
        print_tree_rec(node->right, child_prefix, lab, 1);
    }
}

void huff_print_tree(HuffNode *root)
{
    if (root == NULL) {
        printf("    （没有树：空文本）\n");
        return;
    }
    printf("    哈夫曼树（边上的 0/1 就是编码的每一位，字符全在叶子上）：\n");
    print_tree_rec(root, "", "", 1);
}

/*
 * WPL = Σ(叶子权重 × 深度)。深度从 0 开始数"走了几条边"。
 *
 * 为什么 WPL 就是编码后的总比特数？
 *   叶子深度 = 该字符的码长；权重 = 该字符出现次数；
 *   乘起来 = 这个字符在整段文本里占了多少比特；所有字符加起来 = 总比特数。
 *   下面 main.c 第 6 幕会把 huff_encode 数出来的比特数和 WPL 对比，必须相等。
 *
 * 单字符特判：只有一个叶子且它就是根时，深度是 0，但 assign_codes 给了 1 位编码，
 *   所以这里也用 1，保证 WPL 和实际比特数仍然一致。
 */
static int wpl_rec(HuffNode *node, int depth)
{
    if (node == NULL) {
        return 0;
    }
    if (node->is_leaf) {
        return node->weight * (depth == 0 ? 1 : depth);
    }
    return wpl_rec(node->left, depth + 1) + wpl_rec(node->right, depth + 1);
}

int huff_wpl(HuffNode *root)
{
    return wpl_rec(root, 0);
}

/*
 * 前缀码自检：把表里所有【出现过】的编码两两比较，
 * 只要有一条是另一条的开头，就返回 0。
 *
 * 为什么要专门写这个检查？（这是"自己证明给自己看"的那类代码，不是装饰）
 *   哈夫曼正确性全靠"前缀码"这一条性质；一旦建树时把字符挂到了内部节点上，
 *   或者编码填错了一格，压缩仍然"能跑"，只有解码会神不知鬼不觉地错。
 *   把这个性质写成可执行的检查，跑一遍就死心，比读十遍代码有用。
 *   main.c 里还顺手用同一套逻辑证明了那个"坏编码表"确实不是前缀码 —— 对照着看，
 *   就知道前缀冲突不是玄学，是一个能检测的机械性质。
 */
int huff_check_prefix_free(const HuffCodeEntry table[])
{
    const char *codes[HUFF_CHAR_SET];
    int lens[HUFF_CHAR_SET];
    int n = 0;

    for (int i = 0; i < HUFF_CHAR_SET; i++) {
        if (table[i].freq > 0 && table[i].code_len > 0) {
            codes[n] = table[i].code;
            lens[n]  = table[i].code_len;
            n++;
        }
    }
    for (int a = 0; a < n; a++) {
        for (int b = 0; b < n; b++) {
            if (a == b) {
                continue;
            }
            /* codes[a] 是不是 codes[b] 的开头？只要 b 更长且前 len(a) 位相同，就是冲突 */
            if (lens[b] > lens[a] && strncmp(codes[b], codes[a], (size_t)lens[a]) == 0) {
                printf("      [不是前缀码] '%s' 是 '%s' 的开头\n", codes[a], codes[b]);
                return 0;
            }
        }
    }
    return 1;
}

/* ===================================================================
 * 第 4 组：编码、解码
 * =================================================================== */

/*
 * huff_encode：查表把每个字符换成 0/1 串，首尾相接成一整条比特流。
 *
 * 注意"首尾相接、中间不加分隔符" —— 这正是前缀码存在的意义：
 * 没有分隔符也一定能唯一切开。定长编码也不需要分隔符（靠长度切），
 * 但它要为不常见的字符付多余比特。
 *
 * bits_out 里存的是字符 '0' 和 '1'（不是真的比特位）。
 * 为什么不用 char 的某一位存真比特？—— 为了让你能把比特流打印出来看。
 * 真压缩文件当然要用位操作（1 字节省 8 倍空间），本节的重点是"编码为什么可解"，
 * 不是"怎么把 8 个比特塞进 1 个字节"，所以这里故意浪费。README 坑 #6 有说明。
 */
int huff_encode(const char *text, const HuffCodeEntry table[],
                char *bits_out, int cap, int *bit_count_out)
{
    int used = 0;
    for (const char *p = text; *p != '\0'; p++) {
        const HuffCodeEntry *e = &table[(unsigned char)*p];
        if (e->freq <= 0) {
            char rb[8];
            printf("      [错误] 字符 '%s' 没在编码表里（建树用的文本和要编码的文本不是同一份）\n",
                   char_repr((unsigned char)*p, rb, sizeof rb));
            if (bit_count_out != NULL) {
                *bit_count_out = used;
            }
            return 0;
        }
        for (int i = 0; i < e->code_len; i++) {
            if (used + 1 >= cap) {         /* 留一格放 '\0'，所以是 used+1 >= cap */
                printf("      [错误] 比特流缓冲区只有 %d 格，装不下了\n", cap);
                if (bit_count_out != NULL) {
                    *bit_count_out = used;
                }
                return 0;
            }
            bits_out[used++] = e->code[i];
        }
    }
    bits_out[used] = '\0';                 /* 当字符串打印时方便；真正解码只看 bit_count */
    if (bit_count_out != NULL) {
        *bit_count_out = used;
    }
    return 1;
}

/*
 * huff_decode：拿比特流 + 哈夫曼树，从根开始走。
 *   读一个 '0' → 往左；读一个 '1' → 往右；走到叶子 → 吐出这个字符，回到根，继续读。
 *
 * 为什么"走到叶子就一定能回根、不会读错"？
 *   因为前缀码保证了：从根出发的任何一条路径，中途不会经过别的叶子；
 *   而且下一个比特的含义（继续往下 or 换字符）完全由当前所在节点决定，
 *   不需要回头看后面的比特 —— 这叫"即时解码"（唯一可解且无需前瞻）。
 *   对照 main.c 第 1 幕那个坏编码表："010" 读到 "0" 时无法决定停不停，
 *   因为 '0' 既是一个完整编码、又是 '01' 的开头。
 *
 * 边界情况：
 *   ① 只有一个字符种类时，根就是叶子：每读 1 个比特就吐一个字符（特判在下面）。
 *   ② 比特流末尾不完整（截断了）→ 走到中间就没了，最后停在内部节点上。
 *      本函数把它当作输入错误报告出来，而不是硬凑一个字符。真实工程里要在
 *      文件头存"原文字符数"，靠它判断该在哪停 —— 见 README 坑 #5。
 */
int huff_decode(const char *bits, int bit_count, HuffNode *root, char *out, int cap)
{
    if (root == NULL || bit_count < 0) {
        return 0;
    }
    int n = 0;
    HuffNode *cur = root;

    if (root->is_leaf) {
        /* 极端情况：整棵树就一个叶子（"aaaa"），规定每 1 比特 = 1 个该字符 */
        for (int i = 0; i < bit_count; i++) {
            if (n + 1 >= cap) {
                printf("      [错误] 还原缓冲区只有 %d 格，装不下\n", cap);
                return -1;
            }
            out[n++] = (char)root->ch;
        }
        out[n] = '\0';
        return n;
    }

    for (int i = 0; i < bit_count; i++) {
        if (bits[i] == '0') {
            cur = cur->left;
        } else if (bits[i] == '1') {
            cur = cur->right;
        } else {
            printf("      [错误] 比特流里出现了 '%c'，只该有 0 和 1\n", bits[i]);
            return -1;
        }
        if (cur == NULL) {
            printf("      [错误] 比特流走到头没有孩子了（比特流比树长，输入被写坏了）\n");
            return -1;
        }
        if (cur->is_leaf) {
            if (n + 1 >= cap) {
                printf("      [错误] 还原缓冲区只有 %d 格，装不下\n", cap);
                return -1;
            }
            out[n++] = (char)cur->ch;
            cur = root;                    /* 关键一步：吐完字符立刻回根，后面的比特是新字符 */
        }
    }
    if (cur != root) {
        printf("      [警告] 比特流末尾只剩半截（停在内部节点），最后那几个比特没解出字符\n");
    }
    out[n] = '\0';
    return n;
}

/* ===================================================================
 * 第 5 组：销毁
 * =================================================================== */

/*
 * 后序释放：先放左右子树，再放自己。
 * 和 btree_free 一个道理（见 05-tree/b-tree）：
 *   一旦 free(node)，node->left / node->right 就成了"已经被回收的纸条"，
 *   再去读就是 use-after-free。所以必须自底向上。
 * 所有节点都挂在 root 下面（叶子在建树时被 pop 出来又挂回树里），
 * 所以放完整棵树，一个都不会漏 —— ASan 报 leak 就说明有节点失联了。
 */
void huff_free_tree(HuffNode *root)
{
    if (root == NULL) {
        return;
    }
    huff_free_tree(root->left);
    huff_free_tree(root->right);
    free(root);
}
