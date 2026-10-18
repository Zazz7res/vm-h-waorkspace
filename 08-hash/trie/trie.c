#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trie.h"

/*
 * ===================================================================
 * 前缀树 Trie 实现。
 *
 * 内存形状：只有【一种】节点，一次 malloc 一个节点，节点里 26 个槽位往下指。
 *   和 07-graph 的邻接表像（指针数组负责跳转），和第 05 章的二叉树更像
 *   （每个节点从"2 个孩子"变成"26 个孩子"）。
 *
 * 所以释放纪律也完全继承二叉树那一套：**从最深的孩子开始往回收（后序）**。
 * 这一节最容易漏的就是 trie_free 的递归顺序，见第 10 节的图解。
 *
 * 全文件的阅读顺序：
 *   1 节点工厂   2 输入校验（防越界）   3 插入（含失败回滚）
 *   4 三个查询（同一个 walk + 读不同字段） 5 走多少步（把 O(L) 跑成数字）
 *   6 删除（pass_count 一路减 1） 7 自动补全（DFS + 回溯 + 就地缓冲区）
 *   8 打印整棵树  9 空间账单  10 递归释放（后序） 11 设计讨论
 * ===================================================================
 */

/*
 * ===================================================================
 * 1. 节点工厂
 * ===================================================================
 * 用 calloc 而不是 malloc，理由是"清零"属于正确性，不是礼貌：
 *   - child[] 里残留一个随机地址 → 插入时以为"这个孩子已经有了"就钻进去 → 段错误；
 *   - end_count/pass_count 是垃圾值 → 查询直接返回莫名奇妙的数字。
 * calloc 把 26 个指针和两个 int 一起抹成 0，一行代码堵死这两类 bug。
 *
 * 大小写成 sizeof(TrieNode) 而不是手写 216：换平台（指针 4 字节 / int 8 字节）
 * 自动跟着变，这也是 00-c-foundation 就立下的规矩。
 */
TrieNode *trie_node_new(void)
{
    return (TrieNode *)calloc(1, sizeof(TrieNode));
}

/*
 * ===================================================================
 * 2. 输入校验：这堵墙为什么必须存在
 * ===================================================================
 * child 数组只有 26 格，下标是 `*p - 'a'`。这个表达式对输入【毫无抵抗力】：
 *
 *     'a' → 0     ✅        'z'(122) → 25   ✅
 *     'A'(65) → -32   ❌ 负下标：读写 node 前面的内存
 *     '0'(48) → -49   ❌
 *     '{'(123) → 26   ❌ 正好越过数组尾，写到 end_count/pass_count 之后的内存
 *     ' '(32) → -65   ❌ 空格也是负数
 *
 * 编译器不会拦（它不知道 idx 的范围），程序可能连跑十次都对，第十一次才崩 ——
 * 因为可能只是把隔壁字段的值改成了一个"看起来无害"的数。ASan 第一次就越界打断你。
 * 所以插入/删除这种【会写】的操作必须先过这堵墙；查询这种只读的靠 walk 里的
 * 同样的判断返回"查不到"。
 *
 * 长度上限同理：路径缓冲区是栈上的定长数组，没有上界就没有边界检查。
 */
static int word_is_valid(const char *word)
{
    if (word == NULL) {
        printf("      [警告] 键是 NULL 指针，已拒绝\n");
        return 0;
    }
    if (word[0] == '\0') {
        /* 空串在 Trie 里代表"根自己"，插进去就是把根的 end_count 当词频用，
         * 语义一团糟（"以空串开头的词"永远等于全部词）。本节直接拒绝，
         * 并在 README 的坑里告诉你真实库是怎么处理的（给根单独存一个 bool）。 */
        printf("      [警告] 空串 \"\" 不插：它对应的节点是根本身，语义会混乱\n");
        return 0;
    }
    size_t len = strlen(word);
    if (len >= (size_t)TRIE_WORD_MAX) {
        printf("      [警告] 串长 %zu ≥ 上限 %d，已拒绝（路径缓冲区只有 %d 格）\n",
               len, TRIE_WORD_MAX - 1, TRIE_WORD_MAX);
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        if (word[i] < 'a' || word[i] > 'z') {
            printf("      [警告] 第 %zu 个字符 '%c' 不是小写字母：idx = '%c' - 'a' = %d，"
                   "child[] 只有 26 格，会越界 → 整个串已拒绝\n",
                   i, word[i], word[i], (int)word[i] - 'a');
            return 0;
        }
    }
    return 1;
}

/*
 * ===================================================================
 * 3. 插入：走一条路，路过谁就给谁 +1
 * ===================================================================
 * 计数器语义在这里最容易搞混，所以把两件事分开写清楚：
 *     pass_count++  —— 每【经过】一个节点就加 1（含根，含终点）
 *     end_count++   —— 只在【最后一个字符对应的节点】加 1
 * 于是插 "card"：c、a、r、d 四个节点的 pass 都 +1，只有 d 的 end +1。
 *
 * 重复插入完全不用特殊处理：路已经存在 → 不新建节点 → 计数器继续累加。
 * 这就是 end_count 是 int 而不是 bool 的收益（Trie 天生支持词频/多重集合）。
 *
 * 为什么要 TrieNode **root：空树时根由本函数创建，要改到调用者手里的指针。
 * 为什么 path[] 是栈上数组：串长已被 word_is_valid 卡住 ≤ 31，
 *   所以"走过的节点"最多 32 个（含根），定长数组一定装得下，不用 malloc。
 *   它存在的唯一理由是【回滚】：中途 calloc 失败时，前面已经 +1 的 pass_count
 *   必须减回去，否则树会记住一个从没插进去的串（delete 时就可能把节点错删）。
 */
int trie_insert(TrieNode **root, const char *word)
{
    if (root == NULL) {
        printf("      [警告] trie_insert: root 是 NULL，连\"往哪儿挂\"都不知道\n");
        return 0;
    }
    if (!word_is_valid(word)) {
        return 0;
    }

    if (*root == NULL) {
        *root = trie_node_new();
        if (*root == NULL) {
            printf("      [错误] 根节点申请失败（内存不够）\n");
            return 0;
        }
        printf("      [新建根] 空树的第一次插入要先把根造出来，并写回调用者的指针\n");
    }

    TrieNode *path[TRIE_WORD_MAX];    /* 走过的节点：深度 0..31 最多 32 个 */
    int depth = 0;

    TrieNode *cur = *root;
    path[depth++] = cur;
    cur->pass_count++;                /* 根代表空前缀：每个串都从它身上过 */

    for (const char *p = word; *p != '\0'; p++) {
        int idx = *p - 'a';           /* 校验过一定是 0..25，这里可以放心算 */
        if (cur->child[idx] == NULL) {
            cur->child[idx] = trie_node_new();
            if (cur->child[idx] == NULL) {
                /* ★ 回滚：把这次插入已经加过的 pass_count 全部减回去 */
                for (int i = 0; i < depth; i++) {
                    path[i]->pass_count--;
                }
                printf("      [错误] 建 '%c' 节点时内存不足，已回滚 %d 个节点的 pass_count\n",
                       *p, depth);
                return 0;
            }
        }
        cur = cur->child[idx];
        path[depth++] = cur;
        cur->pass_count++;            /* 经过 = +1 */
    }

    cur->end_count++;                 /* 只有终点：这个串在我这里结束 */
    return 1;
}

/*
 * ===================================================================
 * 4. 查询三兄弟：同一个 walk，读不同字段
 * ===================================================================
 * walk 是本节唯一的"下标换算 + 跳转"入口：字符 → 格子编号 → 跳过去。
 * 走通了就返回末端节点，走不通一律返回 NULL（绝不返回"走到一半"的节点，
 * 否则调用方必须自己判断到底走完了没 —— 那种接口迟早出错）。
 */
static const TrieNode *walk(const TrieNode *node, const char *s, int *steps_out)
{
    int steps = 0;
    if (node == NULL || s == NULL) {
        if (steps_out != NULL) {
            *steps_out = 0;
        }
        return NULL;
    }
    for (; *s != '\0'; s++) {
        int idx = *s - 'a';
        if (idx < 0 || idx >= TRIE_ALPHABET) {
            break;                       /* 非法字符 = 这条前缀不可能存在 */
        }
        if (node->child[idx] == NULL) {
            break;                       /* 路断了 */
        }
        node = node->child[idx];
        steps++;
    }
    if (steps_out != NULL) {
        *steps_out = steps;
    }
    if (*s != '\0') {
        return NULL;                     /* 没走完 → 失败，别把半个结果漏出去 */
    }
    return node;
}

/* 整串精确查：走完 word，读 end_count。 */
int trie_search(const TrieNode *root, const char *word)
{
    const TrieNode *node = walk(root, word, NULL);
    if (node == NULL) {
        return 0;
    }
    return node->end_count;
}

/* 前缀查询：走完 prefix 就说明"有串以它开头"，一个计数器都不用读。 */
int trie_starts_with(const TrieNode *root, const char *prefix)
{
    return (walk(root, prefix, NULL) != NULL) ? 1 : 0;
}

/*
 * 前缀计数：走完 prefix，直接读 pass_count —— 一行，但它是整节的重点。
 * 复杂度是 O(prefix 长度)，不是 O(子树大小)：
 * 子树里有 3 个词还是 300 万个词，这一行的耗时完全相同。
 * （没有 pass_count 就必须 DFS 数 end_count，那才是真的按规模付费。）
 */
int trie_count_prefix(const TrieNode *root, const char *prefix)
{
    const TrieNode *node = walk(root, prefix, NULL);
    if (node == NULL) {
        return 0;
    }
    return node->pass_count;
}

/* 把"只走 len 步"这件事跑成可打印的数字。 */
int trie_walk_counted(const TrieNode *root, const char *s, int *steps_out)
{
    int steps = 0;
    const TrieNode *node = walk(root, s, &steps);
    if (steps_out != NULL) {
        *steps_out = steps;
    }
    return (node != NULL) ? 1 : 0;
}

/*
 * ===================================================================
 * 5. 删除：先减计数器，再决定"我这个节点还要不要留"
 * ===================================================================
 * 递归函数 node_delete 的返回值是"处理完之后我还剩不剩"：
 *     返回 NULL  → 我已经被 free 了，请父亲把孩子格子置空
 *     返回 node  → 我还留着
 * 这就是为什么插入要用二级指针、删除的【递归】却只要一级指针再返回节点：
 * 递归里"父亲那一格"就在调用者手上（`node->child[idx] = node_delete(...)`），
 * 只有最外层需要改的是【调用者的 root】，所以公开接口用 TrieNode **root。
 *
 * 三条铁律，对应代码里三个标 ★ 的地方：
 *   ★1 pass_count 在【每一层】都要 -1，包括终点自己 —— 因为 pass 统计的是
 *      "经过我的串"，被删的这个串以前经过我，现在不经过了。
 *      少减一层（比如忘了减终点），那个节点就永远 free 不掉，是内存泄漏的温床。
 *   ★2 判据是 pass_count == 0 【且】 end_count == 0。
 *      只看 end_count 会把 "car" 删掉时顺手删掉 card 还在用的那个节点；
 *      只看 pass_count 在 pass==0 时其实蕴含 end==0，但写全两条是给自己看的文档。
 *   ★3 free 完必须 return NULL，让父亲把格子置空。否则父亲手里攥着一个
 *      已经还给系统的地址（悬空指针），下次插入就钻进去 → use-after-free。
 */
static TrieNode *node_delete(TrieNode *node, const char *word)
{
    if (node == NULL) {
        return NULL;            /* 保险：路断了（公开接口已确认串存在，正常走不到这里） */
    }

    node->pass_count--;         /* ★1 经过我的少了一个 */

    if (*word == '\0') {
        if (node->end_count > 0) {
            node->end_count--;  /* 插了两遍的词，删一遍只是把计数退一格 */
        }
    } else {
        int idx = *word - 'a';
        /* 先处理孩子 —— 这一步返回 NULL 就说明孙子被 free 了，格子立刻跟上置空 */
        node->child[idx] = node_delete(node->child[idx], word + 1);
    }

    if (node->pass_count == 0 && node->end_count == 0) {   /* ★2 */
        free(node);
        return NULL;            /* ★3 我没了 */
    }
    return node;
}

int trie_delete(TrieNode **root, const char *word)
{
    if (root == NULL || *root == NULL) {
        return 0;
    }
    if (!word_is_valid(word)) {
        return 0;
    }
    /*
     * ★ 动手之前先确认"这个词真的在"。和 ht_put 的"先查再插"是同一条纪律：
     *   不在还硬删，pass_count 会被减成负数，节点永远不会归零 →
     *   整棵树只涨不缩，而且不报错，几个月后才发现内存被吃光。
     */
    if (trie_search(*root, word) == 0) {
        printf("      [提示] \"%s\" 本来就不在树里，一个计数器都不动\n", word);
        return 0;
    }
    *root = node_delete(*root, word);
    return 1;
}

/*
 * ===================================================================
 * 6. 自动补全：DFS + 回溯 + 一个【就地写的定长缓冲区】
 * ===================================================================
 * 目标：把以 prefix 开头的所有完整单词找出来。做法是"从 prefix 末端节点开始，
 * 逢岔路就每条都走一遍"，这就是深度优先搜索：
 *
 *     前缀 "do" 站在 o 节点上，往下只有 1 条路（另一个 o）→ 再往下只有 r →
 *     沿途把走过的字符一格一格填进 path[]：
 *
 *       进入时 path = "do"           len=2
 *       填 path[2]='o'  → "doo"      len=3   （递归进去）
 *       填 path[3]='r'  → "door"     len=4   end_count=1 → 抄走 "door"
 *       回到 len=3，回到 len=2，再试 'do' 自己：end_count=1 → 抄走 "do"
 *
 *     顺序是"先站点后流量"：一个节点先报自己的 end_count（"do"），
 *     再往下钻（"door"）—— 于是短词排在长词前面，正好是字典序。
 *
 * 为什么用递归而不是循环：树的深度【事先不知道】（最短 1 层，最长 31 层）。
 * 用循环就得自己维护一个栈存"待处理节点 + 当前路径长度"，等于手写一遍调用栈，
 * 代码量翻倍还容易漏。递归把"回到上一层"这件事交给了栈帧，代价是每层一个栈帧
 * （本节深度上限 32，绝对安全；真实库里几百层的树要考虑改成显式栈）。
 *
 * 为什么缓冲区必须就地写死、不许全局：
 *     全局缓冲区 = 同一个函数两次调用互相踩数据（不可重入），线程一并发就错，
 *     而且"谁负责清零"变成隐式约定 —— 这是 C 里最常见的 bug 来源之一。
 *     本节的 path[] 是【trie_autocomplete 栈帧里的局部数组】，函数一返回就消失，
 *     递归各层共用同一块内存（靠参数把地址传下去），互不干扰的原因是
 *     每一层只读写 path[len] 这一格，并且离开时不需要清零。
 *
 * ★ 回溯到底是什么：填 path[len] → 递归进去（它只用 path[0..len]）→ 出来。
 *   什么都不用删：下一个孩子来填 path[len] 时，【覆盖】就是清理。
 *   长度 len 是参数（值传递），返回后自动回到本层的值 —— 这就是"退一步"。
 */
static int collect_words(const TrieNode *node, char *path, int len,
                         char out[][TRIE_WORD_MAX], int max_words, int found)
{
    if (node == NULL || found >= max_words) {
        return found;
    }

    if (node->end_count > 0) {
        path[len] = '\0';                       /* 缓冲区到这里就是一个完整单词 */
        memcpy(out[found], path, (size_t)len + 1);  /* +1：把 '\0' 一起抄过去 */
        found++;
    }

    for (int i = 0; i < TRIE_ALPHABET; i++) {   /* i 从 0 到 25 = 'a' 到 'z'：天然字典序 */
        if (found >= max_words) {
            break;                              /* 装满了，整棵子树不用再走 */
        }
        if (node->child[i] == NULL) {
            continue;                           /* 空格子直接跳过 —— 26 格里绝大多数走不到 */
        }
        if (len + 1 >= TRIE_WORD_MAX) {
            printf("      [警告] 路径超过 %d 字符，补全结果被截断\n", TRIE_WORD_MAX - 1);
            break;
        }
        path[len] = (char)('a' + i);            /* ★ 就地写第 len 格 */
        found = collect_words(node->child[i], path, len + 1, out, max_words, found);
        /* ★ 回溯：这里故意什么都不写。下一个孩子会覆盖 path[len]。 */
    }
    return found;
}

/*
 * 公开接口：先在【调用者看不见的地方】放好缓冲区 path[]，再把前缀抄进它开头。
 * 前缀本身就是答案的一部分（补全 "do" 得到 "door"，"do" 两个字符已经就位），
 * 所以缓冲区前半截预填前缀，DFS 只负责往后接字符 —— 省掉一次拼接。
 */
int trie_autocomplete(const TrieNode *root, const char *prefix,
                      char out[][TRIE_WORD_MAX], int max_words)
{
    if (root == NULL || out == NULL || max_words <= 0) {
        return 0;
    }
    if (prefix == NULL) {
        printf("      [警告] 前缀是 NULL 指针\n");
        return 0;
    }
    int len = (int)strlen(prefix);
    if (len >= TRIE_WORD_MAX) {
        printf("      [警告] 前缀太长（%d ≥ %d），不补全\n", len, TRIE_WORD_MAX);
        return 0;
    }

    const TrieNode *start = walk(root, prefix, NULL);
    if (start == NULL) {
        return 0;               /* 前缀本身都不存在 → 一个补全也不可能有 */
    }

    char path[TRIE_WORD_MAX];   /* ← 全部状态就在这 32 字节的栈空间里，没有全局变量 */
    memcpy(path, prefix, (size_t)len);
    return collect_words(start, path, len, out, max_words, 0);
}

/* 从根开始 DFS：空前缀 = 所有单词，出来的顺序就是字典序（哈希表做不到的那件事）。 */
int trie_dump_words(const TrieNode *root, char out[][TRIE_WORD_MAX], int max_words)
{
    if (root == NULL || out == NULL || max_words <= 0) {
        return 0;
    }
    char path[TRIE_WORD_MAX];
    return collect_words(root, path, 0, out, max_words, 0);
}

/*
 * ===================================================================
 * 7. 把树打印出来 —— 不打印，前面所有解释都只是口号
 * ===================================================================
 * 一行 = 一个节点，缩进 2 × 深度，所以"同一列起手的行"就是同一层。
 * 行首的 '*' = end_count > 0，也就是"这是一个完整单词的终点"。
 * 行尾直接把这个节点代表的完整单词（= 从根走过的路径）打出来，
 * 于是"car 有孩子 d 但 car 自己也是单词"这件事不用解释，眼睛就能看到。
 * 路径缓冲区和回溯的手法与 collect_words 完全一样（同一个道理用第二遍）。
 */
static void print_subtree(const TrieNode *node, int depth, char *path, int len)
{
    char label[16];
    if (depth == 0) {
        snprintf(label, sizeof(label), "%s", "(root)");
    } else {
        snprintf(label, sizeof(label), "'%c'", path[len - 1]);
    }

    printf("      %*s%s%-9s pass=%-4d end=%-4d",
           depth * 2, "", (node->end_count > 0) ? "*" : " ",
           label, node->pass_count, node->end_count);
    if (node->end_count > 0) {
        path[len] = '\0';
        printf("  ← 完整单词 \"%s\"%s", path,
               (node->end_count > 1) ? "（插了这么多遍）" : "");
    }
    printf("\n");

    for (int i = 0; i < TRIE_ALPHABET; i++) {
        if (node->child[i] != NULL) {
            path[len] = (char)('a' + i);
            print_subtree(node->child[i], depth + 1, path, len + 1);
            /* 回溯：同上，靠下一次覆盖清理，不需要显式抹掉 */
        }
    }
}

/*
 * 数每一层有几个节点。累加器 per_level[] 是【调用者的局部数组，用指针传进来】，
 * 不是全局变量 —— 这一点对比第 6 节缓冲区那段注释：想带状态出函数，
 * 就明明白白写进参数里，别偷偷藏在文件作用域。
 */
static void count_levels(const TrieNode *node, int depth, int *per_level, int *max_depth)
{
    if (depth > TRIE_WORD_MAX) {
        return;                 /* 理论到不了（插入已限制串长），纯粹是边界保险 */
    }
    per_level[depth]++;
    if (depth > *max_depth) {
        *max_depth = depth;
    }
    for (int i = 0; i < TRIE_ALPHABET; i++) {
        if (node->child[i] != NULL) {
            count_levels(node->child[i], depth + 1, per_level, max_depth);
        }
    }
}

void trie_print_tree(const TrieNode *root)
{
    if (root == NULL) {
        printf("      (空树：连根都没有)\n");
        return;
    }
    char path[TRIE_WORD_MAX];
    print_subtree(root, 0, path, 0);

    int per_level[TRIE_WORD_MAX + 1] = { 0 };
    int max_depth = 0;
    count_levels(root, 0, per_level, &max_depth);
    printf("      ── 每层节点数：");
    for (int d = 0; d <= max_depth; d++) {
        printf("第%d层=%d ", d, per_level[d]);
    }
    printf("（一共 %d 层：第 0 层是根，最深层 %d = 最长单词 %d 个字符）\n",
           max_depth + 1, max_depth, max_depth);
}

/*
 * ===================================================================
 * 8. 空间账单：把"费内存"变成一个能读的数字
 * ===================================================================
 * 一趟遍历同时统计：节点数、真正连出去的岔路数（= 用掉的 child 格子）、
 * 不同单词数、这些单词的字符总数。一次遍历拿全，别写四个递归函数。
 */
typedef struct {
    int nodes;      /* 节点总数（含根） */
    int edges;      /* 非 NULL 的 child 格子数 = 树里的边数 */
    int words;      /* end_count > 0 的节点数 = 不同单词数 */
    int letters;    /* 所有不同单词的字符总数 = 真正"有用"的字符数 */
} TrieStat;

static void gather_stat(const TrieNode *node, int depth, TrieStat *s)
{
    s->nodes++;
    if (node->end_count > 0) {
        s->words++;
        s->letters += depth;    /* depth 就是这个节点代表的单词长度 */
    }
    for (int i = 0; i < TRIE_ALPHABET; i++) {
        if (node->child[i] != NULL) {
            s->edges++;
            gather_stat(node->child[i], depth + 1, s);
        }
    }
}

void trie_print_stats(const TrieNode *root, const char *label)
{
    if (root == NULL) {
        printf("      （空树，没有账可算）\n");
        return;
    }
    TrieStat s = { 0, 0, 0, 0 };
    gather_stat(root, 0, &s);

    size_t slots = (size_t)s.nodes * TRIE_ALPHABET;
    size_t total = (size_t)s.nodes * sizeof(TrieNode);
    size_t wasted = (slots - (size_t)s.edges) * sizeof(TrieNode *);

    printf("      %s：节点 %d 个 × sizeof(TrieNode)=%zu 字节 = %zu 字节（%.1f KB）\n",
           label, s.nodes, sizeof(TrieNode), total, (double)total / 1024.0);
    printf("        其中每个节点：child[26] = 26 × %zu = %zu 字节，end_count+pass_count = %zu 字节，合计 %zu 字节\n",
           sizeof(TrieNode *), sizeof(TrieNode *) * TRIE_ALPHABET,
           sizeof(int) * 2, sizeof(TrieNode));
    printf("        child 槽位：共 %zu 格，只用掉 %d 格（边数 = 节点数 - 1 = %d），空 %zu 格 = %.1f%%\n",
           slots, s.edges, s.nodes - 1, slots - (size_t)s.edges,
           100.0 * (double)(slots - (size_t)s.edges) / (double)slots);
    printf("        光那些 NULL 格子就占着 %zu 字节（%.1f KB）—— 这就是\"费\"的字面意思\n",
           wasted, (double)wasted / 1024.0);
    printf("        存了几个词：%d 个不同单词，字符总数 %d → 摊下来每个字符 %zu 字节\n",
           s.words, s.letters, s.letters > 0 ? total / (size_t)s.letters : 0);
}

/*
 * ===================================================================
 * 9. 节点计数（给"插入前后节点数差了几个"用）
 * ===================================================================
 * 注意它是 O(节点数) 的全树遍历，不是 O(1)。
 * 哈希表里 size 是一个字段，读它 O(1)，因为 put/delete 一路维护；
 * Trie 想 O(1) 拿节点数也得在插入/删除里维护计数器 —— 本节选择不维护，
 * 让你分清"什么数据天生就是一次遍历的代价"。
 */
int trie_count_nodes(const TrieNode *root)
{
    if (root == NULL) {
        return 0;
    }
    int total = 1;
    for (int i = 0; i < TRIE_ALPHABET; i++) {
        total += trie_count_nodes(root->child[i]);   /* child[i] 是 NULL 时上面第一行就返回 0 */
    }
    return total;
}

/* 不同单词数：只认 end_count > 0 的节点，和孩子有多少无关。 */
int trie_count_words(const TrieNode *root)
{
    if (root == NULL) {
        return 0;
    }
    int total = (root->end_count > 0) ? 1 : 0;
    for (int i = 0; i < TRIE_ALPHABET; i++) {
        total += trie_count_words(root->child[i]);
    }
    return total;
}

/*
 * ===================================================================
 * 10. trie_free —— 为什么【必须】后序（先删孩子，再删自己）
 * ===================================================================
 * 树是多块独立内存（每个节点一次 calloc），销毁必须一个不落。而节点的
 * "儿子的地址"存在【我自己的 child[] 数组里】，于是顺序就成了生死问题：
 *
 *      ✅ 后序（本函数的写法）              ❌ 先序（先 free 自己）
 *      ┌────────────────┐                   ┌────────────────┐
 *      │ node 还完整     │                   │ node 已还给系统 │
 *      │  child[a] ─────┼──► 先递归删掉      │  child[a] ???  │← 这块内存
 *      │  child[t] ─────┼──► 先递归删掉      │  child[t] ???  │  已经不属于你了
 *      └────────────────┘                   └────────────────┘
 *        孩子全没了 → free(node)               再去读 child[] 就是
 *                                              use-after-free（未定义行为）
 *
 * 为什么读已 free 的内存"可能看起来没事"：glibc 只是把那块地登记回空闲链表，
 * 数据还在原地。于是错误版本可能跑一百次都对，某一次 malloc 恰好复用了它 ——
 * 你就得到一个"树会随机长出新分支"的诡异 bug。
 * 本节实测过这个错误版本（先 free 父亲、再读父亲->child[i]）：
 *   直白写法连 gcc -Wall 都会喊出来（warning: pointer used after 'free'）；
 *   藏在递归里 gcc 看不出来，但 ASan 一定报 heap-use-after-free 并 abort。
 * 这才是本节坚持加 -fsanitize=address 的原因。
 *
 * 和第 02 章链表那条纪律是【同一条】：free 之前先把还要用的东西抄下来。
 *   链表：抄 next → 接链 → free 老节点。
 *   树：  递归 free 全部孩子 → free 自己。
 * 为什么这里不需要二级指针（对比 trie_insert）：free 不需要改任何指针——
 * 整棵树都要没了，把格子置空毫无意义；只有"从树上摘掉某一个节点"才需要，
 * 那是 trie_delete 的活（它用返回值把 NULL 交给父亲）。
 *
 * 递归调用的次数 = 节点数（每个节点被访问一次，NULL 的孩子也进函数但立刻返回，
 * 共 nodes × 26 次递归调用）。⚠ 调用完之后 root 指针本身还在，它是悬空指针，
 * 调用方必须立刻 root = NULL（00-c-foundation 第 5 课），否则再 trie_free 一次
 * 就是 double free，ASan 直接 abort。
 */
void trie_free(TrieNode *node)
{
    if (node == NULL) {
        return;                     /* 空格子：合法的空操作，所以调用方不用判空 */
    }
    for (int i = 0; i < TRIE_ALPHABET; i++) {
        trie_free(node->child[i]);  /* ★ 后序：先把 26 棵子树连根拔掉 */
    }
    free(node);                     /* ★ 现在才轮到自己 —— 我的内存即将还给系统 */
}

/*
 * ===================================================================
 * 11. 设计讨论（只讨论，不实现第二套）：如果不用 26 个格子会怎样？
 * ===================================================================
 * 第 8 节的账单已经打出来了：一个 2 个字母的词表（676 个词）要 703 个节点、
 * 18278 个 child 格子，其中 96% 是 NULL。"每节点固定 26 个指针"当然可以换掉：
 *
 *  A) 孩子用链表存：  struct Child { char ch; struct TrieNode *next; struct Child *sibling; }
 *      省：内存变成"实际孩子数"，'ca' 只有 2 个孩子就只花 2 格，通常省 5~20 倍。
 *      费：取一个字符不再是 O(1)，而是【在这几个孩子里线性找】，
 *          查询复杂度从 O(L) 变成 O(L × 平均分支数)。单词表分支少（1~3），
 *          常数很小，所以真实词库大量用这一招（这叫"左孩子右兄弟"表示法）。
 *  B) 孩子排序 + 二分：分支多的节点（比如 'a' 后面什么字母都有）用二分，
 *      变成 O(L × log 分支)。实现难度直接上一个台阶，还要处理"要不要重排"。
 *  C) 位图 + 紧凑数组：uint32_t bitmap 标记哪 26 格有孩子，孩子紧跟数组存放。
 *      取孩子 = popcount 算偏移，O(1) 又省空间，但【插入要搬动数组】，
 *      于是变成"读快写慢"，只适合一次性建好、之后只查的词库。
 *  D) 压缩 Trie / radix tree：把"只有一个孩子的链"合并成一条边（存整段字符串）。
 *      节点数暴跌，"card" 和 "car" 共用 "car" 前缀时直接压成一条边。
 *      Linux 内核路由表（fib_trie）就用到这个思路。代价：删除/插入要做字符串切分，
 *      代码复杂度翻倍，而且 end_count/pass_count 的语义要重讲一遍。
 *
 * 结论就一句话：**固定 26 格是把"分支查找"这件事彻底外包给了数组下标 ——
 * 用空间换掉了 O(分支数) 的常数，所以本节才能把复杂度写成干净的 O(L)。**
 * 什么时候必须换掉它？看第 8 节那个空槽率：词表越大、越稀疏（比如中文、
 * Unicode、URL 路径），26（或 65536）个格子的浪费越夸张 —— 那时候 A/D 是标准答案。
 */
