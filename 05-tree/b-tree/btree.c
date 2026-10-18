#include <stdio.h>
#include <stdlib.h>
#include "btree.h"

/*
 * ===================================================================
 * btree.c —— B 树实现（阶 m = BTREE_ORDER，demo 是 3）
 * ===================================================================
 * 阅读顺序就是"从小到大搭积木"的顺序：
 *   第 0 组：节点工厂 + 打印小工具（后面每个函数都要用它们）
 *   第 1 组：建一棵空树 / 销毁整棵树
 *   第 2 组：查找（最简单，先把"往哪个空隙钻"这条核心逻辑讲透）
 *   第 3 组：插入 + 分裂（本节最大考点）
 *   第 4 组：删除 + 借键 + 合并（本节第二大考点）
 *   第 5 组：三种打印视图 + 统计
 *
 * ⚠ 本文件里的 printf 是【故意留着当教学用的】。正式库里不该有打印，
 *   但这是教材："每一步状态打印出来给人看"是本仓库的硬要求。
 *   真把它当库用的话，把这些 printf 删掉即可，逻辑一行都不用改。
 */

/* ===================================================================
 * 第 0 组：节点工厂 + 打印小工具
 * =================================================================== */

/*
 * btree_make_node：造一个节点，is_leaf 决定它是叶子还是内部节点。
 *
 * 为什么所有节点都必须走这一个函数？
 *   1) 用 calloc 而不是 malloc：一次性把 key[] 和 child[] 全部清零。
 *      "新节点的 child[i] 忘了置 NULL" 是这个结构最容易犯的致命错误 ——
 *      垃圾值当指针一解引用就是段错误，而且往往几层之后才炸，极难定位。
 *      用 calloc 就把这类 bug 掐死在出生地。
 *   2) "申请失败返回 NULL" 只在这里判一次，不用抄五遍。
 *
 * m=3 时一个新节点在内存里长这样（calloc 之后全是 0/NULL）：
 *
 *   ┌──────────────────────────── BTreeNode ─────────────────────────────┐
 *   │ key[0] key[1] key[2]* │ child[0] child[1] child[2]* child[3]* │ num_keys │ is_leaf │
 *   │   0      0     0      │  NULL    NULL     NULL      NULL      │    0     │  1/0    │
 *   └────────────────────────────────────────────────────────────────────┘
 *                  ↑ ↑  ↑ 多开的"超载缓冲格"：分裂那一瞬间要用它
 *
 *   字节数（64 位平台）：3×4 + 4×8 + 4 + 4 = 48 → 对齐后 56 字节。
 *   真数据库里一个节点是 4KB/16KB（正好一页），这里为了看清逻辑才用这么小。
 */
static BTreeNode *btree_make_node(int is_leaf)
{
    BTreeNode *node = calloc(1, sizeof(BTreeNode));
    if (node == NULL) {
        fprintf(stderr, "      [致命] 申请 B 树节点失败：堆内存不够\n");
        return NULL;
    }
    node->num_keys = 0;
    node->is_leaf  = is_leaf;
    return node;
}

/* 把节点渲染成一行字符串：0 个键 → "[]"，2 个键 → "[10 | 20]"。
 * 为什么要先转字符串而不直接打印？因为很多提示语要把节点内容夹在句子中间，
 * 例如："分裂前 [10 | 20 | 30] → 左半 [10]，中键 20 上推，右半 [30]"。 */
#define BTREE_NODE_STR_LEN 96

static void btree_node_str(const BTreeNode *node, char *buf, size_t buf_size)
{
    if (node == NULL) {
        snprintf(buf, buf_size, "(空节点)");
        return;
    }

    /* 拼字符串用 snprintf + 偏移量：每一步都知道"还剩多少格子能写"，
     * 比 strcat/sprintf 安全，永远不可能写越界。 */
    int used = snprintf(buf, buf_size, "[");
    if (used < 0 || (size_t)used >= buf_size) {
        snprintf(buf, buf_size, "[?]");
        return;
    }
    for (int i = 0; i < node->num_keys; i++) {
        size_t left = buf_size - (size_t)used;
        int n;
        if (i == 0) {
            n = snprintf(buf + used, left, "%d", node->key[i]);
        } else {
            n = snprintf(buf + used, left, " | %d", node->key[i]);
        }
        if (n < 0 || (size_t)n >= left) {
            break;                    /* 格子不够了就停，绝不越界写 */
        }
        used += n;
    }
    snprintf(buf + used, buf_size - (size_t)used, "]");
}

/* 打印成 "[10 | 20]   (2 键 / 3 空隙 / 内部节点)"。
 * 之所以把"几键几空隙"一起打出来，是让你在输出里就能数出这条铁律：
 *   空隙数 = 键数 + 1。违反它，树一定写坏了。 */
static void btree_node_dump(const BTreeNode *node)
{
    char buf[BTREE_NODE_STR_LEN];
    btree_node_str(node, buf, sizeof buf);
    if (node == NULL) {
        printf("%s", buf);
        return;
    }
    printf("%-16s (%d 键 / %d 空隙 / %s)",
           buf, node->num_keys, node->num_keys + 1, node->is_leaf ? "叶子" : "内部节点");
}

/*
 * btree_key_pos：key 在本节点里的下标；不在就返回 -1。
 *
 * 节点里最多 m-1 个键（demo 只有 2 个），所以【线性扫描】比二分更直白，
 * 常数也更小。真数据库里一个页有 200~1000 个键，那里才值得二分查找
 * （README「常见的坑」#6 有讨论）。
 */
static int btree_key_pos(const BTreeNode *node, int key)
{
    for (int i = 0; i < node->num_keys; i++) {
        if (node->key[i] == key) {
            return i;
        }
    }
    return -1;
}

/*
 * btree_gap_pos：返回"该钻 child[i]"里的那个 i。全文件最核心的一个下标。
 *
 * 规则：从左往右找第一个【严格大于 key】的键，它的下标就是空隙号。
 *   节点 [10 | 20]（3 个空隙）：
 *     key = 5  → key[0]=10 > 5  → 返回 0 → 钻 child[0]（"全都 < 10"那条边）
 *     key = 15 → 10 不 > 15，20 > 15 → 返回 1 → 钻 child[1]（10 与 20 之间）
 *     key = 25 → 没有键大于它 → 返回 num_keys=2 → 钻 child[2]（最右边那条边）
 *
 * ⚠ 相等的时候也照样返回"那个键的下标"（因为 key[i] < key 不成立就停下）。
 *   到底算"命中"还是"钻下去"，由调用方先用 btree_key_pos 判过再决定。
 *   一个函数只干一件事，才写得短、才不容易错 —— 这是本节反复用的手法。
 */
static int btree_gap_pos(const BTreeNode *node, int key)
{
    int i = 0;
    while (i < node->num_keys && node->key[i] < key) {
        i++;
    }
    return i;
}

/* ===================================================================
 * 第 1 组：建与拆
 * =================================================================== */

/*
 * 空树怎么表示？—— 不返回 NULL，而是造一个"0 键的叶子"当根。
 *
 * 为什么？B 树的插入永远是"往下钻到一个叶子，往它数组里塞一个键"。
 * 如果空树用 NULL 表示，第一次插入就得特判"我要不要造根节点"，
 * 于是每个函数都得处理 root == NULL，凭空多出一堆分支。
 * 给一个空叶子当根，插入代码就是一条路走到底，第一次插入自然把根填上键。
 *
 *   btree_new() 之后：  root ──► []    (0 键 / 1 空隙 / 叶子)
 *   插 10 之后：        root ──► [10]  (1 键 / 2 空隙 / 叶子)
 */
BTreeNode *btree_new(void)
{
    return btree_make_node(1);   /* 只有一个节点的树，那个节点必然是叶子 */
}

/*
 * 销毁：后序遍历 —— 先把所有孩子拆干净，再拆自己。
 *
 * 顺序错了会怎样？
 *   若先 free(node) 再去递归 node->child[i]：node 已经还给堆管理器了，
 *   那块内存随时可能被别的 malloc 复用并改写，此时读 child[i] 就是
 *   "野指针 / use-after-free"，ASan 会当场报 heap-use-after-free。
 *   所以"树形结构 + 每个节点单独 malloc"，销毁一律【自底向上】。
 *   （链表可以"边存 next 边 free"，是因为它每格只有一个后继；
 *     节点有多个孩子指针，必须先把"通往孩子的纸条"用完整，才能放自己。）
 *
 *   递归拆的两个小细节：
 *   ① 叶子不递归 —— 叶子的 child[] 全是 NULL，递归进去也是立刻返回，
 *      但显式判 is_leaf 让"谁有孩子"这件事在代码里一眼可见。
 *   ② 调用方拿到 btree_free(root) 之后要自己把指针置 NULL（见 main.c）。
 */
void btree_free(BTreeNode *node)
{
    if (node == NULL) {
        return;                        /* 递归的自然收摊点；free(NULL) 也安全 */
    }
    if (!node->is_leaf) {
        for (int i = 0; i <= node->num_keys; i++) {
            btree_free(node->child[i]);    /* 先拆子树 */
        }
    }
    free(node);                            /* 再拆自己 */
}

/* ===================================================================
 * 第 2 组：查找
 * ===================================================================
 * 干什么：从根往下钻，每进一个节点比几个键；命中就成功，
 *         钻到叶子还没找到就失败。
 *
 * 复杂度：树高 h ≈ log_m(n)。每层做 ≤m 次整数比较，
 *         所以是 O(m · log_m n)；但 m 是由"一页能装几个键"决定的常数，
 *         真正的代价是【读了几次磁盘页】—— 这就是 probe_out 存在的意义。
 *
 *   在 m=3 的树里查 7：
 *     第 1 次读页 [6]        6 < 7 → 钻 child[1]
 *     第 2 次读页 [8 | 10]   8 > 7 → 钻 child[0]
 *     第 3 次读页 [7]        命中！probe = 3，也就是只读了 3 个页
 */
int btree_search(BTreeNode *root, int key, int *probe_out)
{
    int probes = 0;
    int found  = 0;
    BTreeNode *cur = root;

    while (cur != NULL) {
        probes++;                        /* 每进一个节点 = 磁盘上"读了一页" */

        char buf[BTREE_NODE_STR_LEN];
        btree_node_str(cur, buf, sizeof buf);
        printf("      第 %d 次读页: 节点 %-14s ", probes, buf);

        int pos = btree_key_pos(cur, key);
        if (pos >= 0) {
            printf("→ 命中！它就在本节点的 key[%d] 里\n", pos);
            found = 1;
            break;
        }
        if (cur->is_leaf) {
            printf("→ 这是个叶子，里面没有它，而叶子下面没有下一页了 → 树里没有 %d\n", key);
            break;
        }

        int gap = btree_gap_pos(cur, key);
        printf("→ 本节点没有；第一个比它大的键是 key[%d]，所以往空隙 child[%d] 钻\n", gap, gap);
        cur = cur->child[gap];
    }

    if (probe_out != NULL) {
        *probe_out = probes;             /* 不要这个数就传 NULL（pointer-basics 的 _out 流派）*/
    }
    return found;
}

/* ===================================================================
 * 第 3 组：插入 —— 本节最大考点
 * =================================================================== */

/*
 * btree_split：把一个"超载一格"的节点从中间劈成左右两半。
 *
 * ⚠ 前置条件（两条，都不满足就是调用方的 bug）：
 *   ① node->num_keys == BTREE_MAX_KEYS + 1（正好超载 1 个）
 *   ② right 必须是调用方【提前申请好】的空节点
 * 为什么把"申请右节点"放到调用方，而不是在这里 malloc？
 *   因为如果在这里 malloc 失败，节点已经改了一半，树就处于"半分裂"的
 *   非法状态，救不回来。先租好再动手，失败就能"原样退回"，
 *   这是 C 语言里做"看起来原子的操作"的通用招数：
 *   【先把所有资源备齐，再一口气改状态】。
 *
 * m=3（MAX_KEYS=2）时，node 里挤了 3 个键：
 *
 *      mid = num_keys / 2 = 3 / 2 = 1   （下标 1，也就是键 20）
 *
 *   ┌─ 分裂前 node ────────────────────────────────────────────────┐
 *   │ key:    10      20      30     │ child:   c0    c1    c2    c3 │
 *   └──────────────────────────────────────────────────────────────┘
 *              左半留   ↑中键上推   ↑右半搬走
 *
 *   左半（还是原来那个节点）：key=[10]，child=[c0, c1]
 *   中键 20 ──────────────► 交给父亲，塞进父亲里对应的那道空隙
 *   右半（新节点 right）    ：key=[30]，child=[c2, c3]
 *
 *   孩子为什么是"2 + 2"而不是"2 + 1"？
 *     因为"空隙数 = 键数 + 1"这条铁律：
 *     左半 1 个键 → 要 2 个空隙 = c0（<10）和 c1（10~20）；
 *     右半 1 个键 → 要 2 个空隙 = c2（20~30）和 c3（>30）。
 *     中键 20 上了父亲之后，它就是父亲眼里"c1 子树和 c2 子树之间那道缝的标签"。
 *
 *   ⚠ 搬完一定把原节点里"已不归它管"的格子写成 0 / NULL。
 *     不擦的话 num_keys 虽然变小了，但数组尾格还留着老键老指针：
 *       · 调试打印会冒出一个"幽灵键"，看得人一头雾水；
 *       · 更严重的是那个 child 指针：左右两半都留着同一个地址 →
 *         将来两边都 free 它就是 double free，堆管理器直接 abort。
 *     本节所有数组操作都遵守"用完擦干净"这条规矩。
 */
static void btree_split(BTreeNode *node, BTreeNode *right, int *mid_out)
{
    int mid = node->num_keys / 2;                   /* 中键下标 */
    int right_count = node->num_keys - mid - 1;     /* 右半搬走几个键 */

    char before[BTREE_NODE_STR_LEN];
    btree_node_str(node, before, sizeof before);

    /* ① 右半的键 */
    right->num_keys = right_count;
    for (int i = 0; i < right_count; i++) {
        right->key[i] = node->key[mid + 1 + i];
        node->key[mid + 1 + i] = 0;                  /* 擦干净 */
    }
    /* ② 右半的孩子：右半有 right_count 个键 → right_count + 1 个空隙 */
    for (int i = 0; i <= right_count; i++) {
        right->child[i] = node->child[mid + 1 + i];
        node->child[mid + 1 + i] = NULL;             /* 擦干净：防 double free */
    }
    /* ③ 中键取出来交给父亲，原格清零，左半收缩到 mid 个键 */
    int mid_key = node->key[mid];
    node->key[mid] = 0;
    node->num_keys = mid;

    *mid_out = mid_key;

    printf("      分裂！超载节点 %-16s → 左半 ", before);
    btree_node_dump(node);
    printf("，中键 %d 上推给父亲，右半新建 ", mid_key);
    btree_node_dump(right);
    printf("\n");
}

/*
 * insert_one：把 key 插进"以 node 为根的子树"，并且处理 node 可能因此分裂。
 *
 * 返回值约定（这套约定让"分裂往上传播"这件事写得特别顺）：
 *    0 = node 没分裂 → 父亲啥也不用做
 *    1 = node 分裂了！要上推的键写在 *mid_out，新兄弟写在 *right_out
 *   -1 = 内存不够，node 保持原样（资源没备齐时我们一行状态都没改）
 *
 * "到底有没有真的加进去一个键"用 *added_out 带出来，因为 return 那个口子
 * 已经被"有没有分裂"占了（pointer-basics 第 4 条：主结果 return，次要结果 _out）。
 * 不加这个参数行不行？行，但外面就分不清"插好了没分裂"和"重复键被忽略"，
 * btree_insert 只能瞎猜返回值 —— 教学代码不能靠猜。
 *
 * 递归的形状（全文件最需要看懂的一段）：
 *   叶子：塞键 → 可能分裂 → 返回 1
 *   内部：先递归钻下去 → 若孩子返回 1，就把中键接进本节点 → 本节点可能又分裂 → 返回 1
 *   根：  若返回 1，没人接了 → 造新根，树长高一层（这段在 btree_insert 里）
 * 也就是说：**分裂是从下往上一层层冒上来的**，跟"边下钻边预分裂"的写法方向
 * 相反，最终得到的树是同一类东西（差异见 README 坑 #3）。
 */
static int insert_one(BTreeNode *node, int key,
                      int *mid_out, BTreeNode **right_out, int *added_out)
{
    char buf[BTREE_NODE_STR_LEN];

    /* ---------------- 情况 A：叶子 → 直接往数组里插 ---------------- */
    if (node->is_leaf) {
        /* 先确定插在哪：从右往左看，只要前一个键比我要插的大，就继续往左退。
         * 停下来的位置就是"第一个比我大的键的位置"，插在它前面 → 天然保持有序。 */
        int pos = node->num_keys;
        while (pos > 0 && node->key[pos - 1] > key) {
            pos--;
        }
        if (pos > 0 && node->key[pos - 1] == key) {
            btree_node_str(node, buf, sizeof buf);
            printf("      叶子 %s 里已经有 %d → 重复键，本节不支持，忽略\n", buf, key);
            *added_out = 0;                        /* 明确告诉调用方：一个键都没加 */
            return 0;
        }

        /* 数组中间插入 = 把 [pos, num_keys) 整体右移一格，再在 pos 落子。
         * ⚠ 必须【从后往前】搬！
         *   从前往后写 key[i] = key[i-1]，会把 key[i] 的原值提前覆盖掉，
         *   下一轮要去抄它时它已经没了 → 结果 pos 右边所有键变成同一个值。
         *   从后往前搬时，每个目的地都是"刚被抄走的空格"或尾格那个空位，
         *   永远不会踩到还没抄走的源值。
         *   一句话记住：【搬数组之前，先确认目的地是空的】。 */
        for (int i = node->num_keys; i > pos; i--) {
            node->key[i] = node->key[i - 1];
        }
        node->key[pos] = key;
        node->num_keys++;                        /* 叶子的 child[] 全是 NULL，键数才是真实信息 */
        *added_out = 1;                          /* 到这里可以拍胸脯：确实多了一个键 */

        printf("      叶子塞入 %d（落在下标 %d）→ ", key, pos);
        btree_node_dump(node);
        printf("\n");

        if (node->num_keys <= BTREE_MAX_KEYS) {
            printf("      键数 ≤ 上限 %d，不用分裂，插入到此结束\n", BTREE_MAX_KEYS);
            return 0;
        }

        /* 超载（num_keys = MAX_KEYS + 1）→ 先把右兄弟租下来，租不到就干净退出 */
        BTreeNode *right = btree_make_node(1);   /* 分裂出来的右半同样是叶子 */
        if (right == NULL) {
            return -1;
        }
        btree_split(node, right, mid_out);
        *right_out = right;
        return 1;
    }

    /* ---------------- 情况 B：内部节点 → 先往下钻，回头接住上推的键 ---------------- */
    if (btree_key_pos(node, key) >= 0) {
        btree_node_str(node, buf, sizeof buf);
        printf("      内部节点 %s 里已经有 %d → 重复键，忽略\n", buf, key);
        *added_out = 0;
        return 0;
    }
    int gap = btree_gap_pos(node, key);
    btree_node_str(node, buf, sizeof buf);
    printf("      内部节点 %s：往空隙 child[%d] 继续下钻\n", buf, gap);

    int child_mid = 0;
    BTreeNode *child_right = NULL;
    int r = insert_one(node->child[gap], key, &child_mid, &child_right, added_out);
    if (r != 1) {
        return r;                                /* 0 = 平安插完；-1 = 失败：都不用自己动手 */
    }

    /* 孩子分裂了：中键进本节点下标 gap，右兄弟挂成 child[gap+1]。
     * 注意两个数组的"合法长度"不一样：键 num_keys 个（0..num_keys-1），
     * 空隙 num_keys+1 个（0..num_keys），所以右移循环的起点差 1 ——
     * 这是 B 树代码最容易写错的一格，写错表现为"某个子树凭空消失"。 */
    for (int i = node->num_keys; i > gap; i--) {
        node->key[i] = node->key[i - 1];         /* 键右移（从后往前）*/
    }
    node->key[gap] = child_mid;
    for (int i = node->num_keys + 1; i > gap + 1; i--) {
        node->child[i] = node->child[i - 1];      /* 空隙右移：从 num_keys+1 那个空格开始 */
    }
    node->child[gap + 1] = child_right;
    node->num_keys++;

    printf("      接住上推的键 %d，塞进本节点下标 %d，新兄弟挂到 child[%d] → ",
           child_mid, gap, gap + 1);
    btree_node_dump(node);
    printf("\n");

    if (node->num_keys <= BTREE_MAX_KEYS) {
        printf("      本节点还没超载 → 分裂到此为止，不用惊动爷爷\n");
        return 0;
    }

    BTreeNode *my_right = btree_make_node(node->is_leaf);   /* 内部节点分出来的还是内部节点 */
    if (my_right == NULL) {
        return -1;
    }
    btree_split(node, my_right, mid_out);
    *right_out = my_right;
    return 1;
}

/*
 * btree_insert：对外的插入入口。唯一比 insert_one 多干的一件事是【处理根分裂】。
 *
 * 根分裂没有父亲能接住中键，所以只能现造一个父亲：
 *
 *      插入前:  root ──► [10 | 20]        （根超载成 3 个键，劈开）
 *                                    ↓
 *      插入后:            [20]           ← 新根，1 个键，2 个孩子
 *                        /    \
 *                 [10]  ◄─    ─►  [30]   ← 老根（左半）和它的新右兄弟
 *
 *   · 树高 +1，而且所有叶子仍然同层（这正是 B 树"矮胖"的原因：
 *     它只在根那一层长高，绝不会出现一条腿特别长的情况）。
 *   · 要传 BTreeNode **root（二级指针）：因为新根的地址必须改到调用者身上。
 *     pointer-basics 那条铁律在这里第三次出场：想改外面的变量，就传它的地址。
 *
 * ⚠ 一个容易忽略的细节：新根【一进函数就先申请好】（下面叫 spare_root）。
 *   为什么？如果等 insert_one 报告"根分裂了"再去 malloc，万一这时申请失败，
 *   老根已经被劈成两半、右半悬在那里没人挂 —— 要么丢子树（错），要么漏内存（脏）。
 *   把资源提前备好，任何一步失败都能"原样退回"，整个插入要么是完整的、
 *   什么都没发生（0 或 -1 时树没被改过，这是本实现特意保住的性质）。
 *   这个"先把资源备齐再改状态"的套路，在数据库里就叫事务，只不过这里用 3 行代码实现。
 */
int btree_insert(BTreeNode **root, int key)
{
    if (root == NULL || *root == NULL) {
        printf("      [错误] 树还没建（root 是 NULL），先调 btree_new()\n");
        return -1;
    }
    printf("    ▶ 插入 %d\n", key);

    BTreeNode *spare_root = btree_make_node(0);   /* 备用新根：多半用不上，但必须先在手上 */
    if (spare_root == NULL) {
        printf("      [错误] 连备用根都申请不到，本次插入放弃（树一个字节都没改）\n");
        return -1;
    }

    int mid_key = 0;
    BTreeNode *right = NULL;
    int added = 0;                       /* 这次到底加没加成，让 insert_one 写进来 */
    int r = insert_one(*root, key, &mid_key, &right, &added);

    if (r == -1) {
        free(spare_root);                /* 每个 malloc 都要有一条路径走到 free，不然就是泄漏 */
        printf("      [错误] 深层节点申请失败，本次插入中止（树保持原样）\n");
        return -1;
    }
    if (!added) {
        free(spare_root);                /* 重复键：用不上备用根，还回去 */
        return 0;
    }

    if (r == 1) {
        spare_root->key[0]   = mid_key;
        spare_root->num_keys = 1;
        spare_root->child[0] = *root;                  /* 老根 = 分裂后的左半 */
        spare_root->child[1] = right;                  /* 新节点   = 分裂后的右半 */
        *root = spare_root;                            /* 二级指针在这里派上用场 */
        printf("      连根都分裂了，没人能接这个中键 → 新建根，树高 +1：");
        btree_node_dump(spare_root);
        printf("\n");
        return 1;
    }

    /* 走到这里 = 插成功了但根没分裂 → 备用根白租了，必须还，否则就是内存泄漏。
     * ⚠ 这条 free 是本文件最容易忘的一行：ASan 跑随机插入 300 个键，
     *   漏 1 千多个 56 字节的节点立刻现形。"每条 return 都要问一句：
     *   手上还攥着哪些没挂到树上的节点？" */
    free(spare_root);
    return 1;
}

/* ===================================================================
 * 第 4 组：删除 —— 借键与合并
 * ===================================================================
 * 难点在哪：插入永远只动"一个叶子 + 一路祖先的数组"，删的时候却可能让某个
 * 节点的键数少于 MIN_KEYS（欠键 underflow），而"少一个键"是破坏不变量的，
 * 必须就地补回来。补只有两个办法：
 *
 *   ① 借（rotate）：让亲兄弟多一个键给我，父亲的键当"中转站"横穿一次。
 *      兄弟的键数 > MIN_KEYS 才有的借（它自己得留够）。
 *   ② 合并（merge）：兄弟也只剩 MIN_KEYS 个，谁也帮不了谁 →
 *      把父亲的键当成桥拉下来，两个孩子焊成一个节点（键数 ≤ MAX_KEYS，见下）。
 *      代价是父亲少了一个键、少了一个孩子 → 父亲可能又欠键 → 继续往上处理。
 *
 *   所以删除最难受的地方是【连锁合并】：一次删除可能把整条路径上的节点全合并掉，
 *   最后根被掏空只剩 0 个键 → 树矮一层。这是 B 树唯一"变矮"的时机。
 *
 * 另一个难点：删内部节点的键会把整棵子树弄断（那个键左边/右边的子树怎么办）。
 *   解法：永远不真的删内部节点的键 —— 拿"左子树最大的键（前驱）"顶替它，
 *   然后到左子树里去删那个前驱。前驱在左子树的最右下角，必然是个叶子键！
 *   这样"真正动手删"永远发生在叶子上，代码只有一条主路。
 *
 *   节点 [10 | 20]，父亲里要删 20：
 *     20 的右子树（child[1]）和左子树（child[2]）原本被 20 分开；
 *     前驱 = child[1] 里最大的键（= 18），把 20 换成 18，
 *     "18 < x < 20"这个区间空了出来，序关系一条都没破；
 *     然后去 child[1] 里删 18 —— 它在叶子里，好删。
 *
 * m=3 时的键数账（说明"合并绝不会超载"）：
 *   MIN_KEYS=1，MAX_KEYS=2。欠键 = 0 个键，能走到"合并"这一步说明
 *   两个兄弟都只有 MIN_KEYS=1 个键（有得借早借了）。
 *   合并后键数 = 0 + 1（父亲的桥键）+ 1 = 2 = MAX_KEYS，正好不超。✓
 */

/*
 * 把本节点下标 pos 的键删掉：后面的键整体左移一格，尾格清零，键数 -1。
 * （删除不需要"从后往前"，因为左移的目的地永远是刚被读走的空格。）
 */
static void remove_key_at(BTreeNode *node, int pos)
{
    for (int i = pos; i + 1 < node->num_keys; i++) {
        node->key[i] = node->key[i + 1];
    }
    node->key[node->num_keys - 1] = 0;               /* 擦掉幽灵键 */
    node->num_keys--;
}

/*
 * 把本节点下标 pos 的孩子指针删掉：后面的空隙整体左移，尾格置 NULL。
 * ⚠ 调用时机很重要：必须在"键还没删"的时候调用，也就是此刻
 *    空隙数 = num_keys + 1 这条不变量还成立。函数内部靠它算边界。
 *    （所以 merge_children 里是先 remove_child_at 再 remove_key_at，顺序反了
 *      边界就会少搬一格，表现为"某个子树被复制了两份 / 丢了一份"。）
 */
static void remove_child_at(BTreeNode *node, int pos)
{
    for (int i = pos; i < node->num_keys; i++) {
        node->child[i] = node->child[i + 1];
    }
    node->child[node->num_keys] = NULL;              /* 擦掉幽灵指针，防 double free */
}

/* 一棵子树里最大的键：一直钻最右边那个空隙，直到叶子，取叶子最后一个键。 */
static int subtree_max_key(BTreeNode *node)
{
    while (!node->is_leaf) {
        node = node->child[node->num_keys];          /* 最后一个空隙 = "全都比本节点键大" */
    }
    return node->key[node->num_keys - 1];
}

/*
 * 向【左兄弟】借一个键（也叫"右旋"，键流的方向是 左兄弟 → 父亲 → 我）：
 *
 *      父亲:  [25]            child[i-1]=[10|20]  child[i]=[30]（欠键，只有 1 个够）
 *             ↑ 要借的是父亲左边的键 key[i-1]=25
 *
 *      第①步 child[i] 整体右移一格，腾出 key[0] / child[0]
 *      第②步 父亲的 key[i-1]=25 下移，成为 child[i] 的新 key[0]
 *            child[i-1] 最右边的空隙 移到 child[i] 最左边（它装的键全在 20~25 之间）
 *      第③步 左兄弟最右边的键 20 上移，成为父亲新的 key[i-1]
 *      第④步 左兄弟键数 -1
 *
 *      结果: 父亲 [20]   child[i-1]=[10]   child[i]=[25|30]
 *      序关系检查：[10] < 20 < [25|30]，父子之间大小规矩一点没破。✓
 *
 * 为什么"孩子最右那个空隙"要跟着走？因为键 25 从父亲下到了我这儿，
 * 原来"父亲的键 25"左边那一大片子树（含键 20~25 之间所有的东西）
 * 现在归我的"最左空隙"管。少搬这个指针，整片子树就丢了 —— 内部节点删除
 * 最容易丢数据的一格，就在这里。
 */
static void borrow_from_left(BTreeNode *node, int i)
{
    BTreeNode *child = node->child[i];
    BTreeNode *sib   = node->child[i - 1];

    char s_child[BTREE_NODE_STR_LEN], s_sib[BTREE_NODE_STR_LEN], s_par[BTREE_NODE_STR_LEN];
    btree_node_str(child, s_child, sizeof s_child);
    btree_node_str(sib,   s_sib,   sizeof s_sib);
    btree_node_str(node,  s_par,   sizeof s_par);

    /* ① 我先整体右移一格，腾出 0 号位 */
    for (int k = child->num_keys; k > 0; k--) {
        child->key[k] = child->key[k - 1];
    }
    for (int k = child->num_keys + 1; k > 0; k--) {   /* 空隙数 = 键数 + 1，边界别算错 */
        child->child[k] = child->child[k - 1];
    }
    /* ② 父亲的键下移给我；兄弟最右的空隙转给我当最左子树 */
    child->key[0]   = node->key[i - 1];
    child->child[0] = sib->child[sib->num_keys];
    child->num_keys++;
    /* ③ 兄弟最右的键上移给父亲（充当新的分隔标签） */
    node->key[i - 1] = sib->key[sib->num_keys - 1];
    /* ④ 兄弟收缩，并把搬走的格子擦干净 */
    sib->key[sib->num_keys - 1] = 0;
    sib->child[sib->num_keys]   = NULL;
    sib->num_keys--;

    printf("      向左兄弟借键（右旋）：父亲 %s 的 key[%d] 下移，兄弟最大键上移\n", s_par, i - 1);
    printf("        借之前: 父亲 %s | 我 %s | 左兄弟 %s\n", s_par, s_child, s_sib);
    printf("        借之后: 我 ");
    btree_node_dump(child);
    printf(" | 左兄弟 ");
    btree_node_dump(sib);
    printf("\n");
}

/*
 * 向【右兄弟】借一个键（"左旋"，键流方向 右兄弟 → 父亲 → 我）：
 *
 *      父亲 [10]，child[i]=[ ]（欠键），child[i+1]=[20|30]
 *      ① 父亲的 key[i]=10 下移，接到我的键数组【末尾】，我的键数 +1
 *      ② 右兄弟的【最左空隙】转给我当新的最右空隙（此时我的空隙数 = 键数 + 1 ✓）
 *      ③ 右兄弟最小的键 20 上移，成为父亲新的 key[i]
 *      ④ 右兄弟的键/空隙整体左移一格，键数 -1
 *      结果：父亲 [20]，我 [10]，右兄弟 [30]。 10 < 20 < 30 ✓
 *
 * 和 borrow_from_left 是完全对称的镜像，只是"下标 0 侧"和"尾侧"互换。
 * 写这两个函数时最需要注意的是②的时机：必须在 num_keys++ 之后取
 * child[num_keys]，否则写入位置差一格。
 */
static void borrow_from_right(BTreeNode *node, int i)
{
    BTreeNode *child = node->child[i];
    BTreeNode *sib   = node->child[i + 1];

    char s_child[BTREE_NODE_STR_LEN], s_sib[BTREE_NODE_STR_LEN], s_par[BTREE_NODE_STR_LEN];
    btree_node_str(child, s_child, sizeof s_child);
    btree_node_str(sib,   s_sib,   sizeof s_sib);
    btree_node_str(node,  s_par,   sizeof s_par);

    /* ① 父亲的键下移到我尾巴上 */
    child->key[child->num_keys] = node->key[i];
    child->num_keys++;
    /* ② 右兄弟最左的空隙转给我当最右空隙（注意此时我的空隙数已经是新键数+1） */
    child->child[child->num_keys] = sib->child[0];
    /* ③ 右兄弟最小的键上移给父亲 */
    node->key[i] = sib->key[0];
    /* ④ 右兄弟整体左移 + 收缩 + 擦尾 */
    for (int k = 0; k + 1 < sib->num_keys; k++) {
        sib->key[k] = sib->key[k + 1];
    }
    sib->key[sib->num_keys - 1] = 0;
    for (int k = 0; k + 1 <= sib->num_keys; k++) {
        sib->child[k] = sib->child[k + 1];
    }
    sib->child[sib->num_keys] = NULL;
    sib->num_keys--;

    printf("      向右兄弟借键（左旋）：父亲 %s 的 key[%d] 下移，兄弟最小键上移\n", s_par, i);
    printf("        借之前: 父亲 %s | 我 %s | 右兄弟 %s\n", s_par, s_child, s_sib);
    printf("        借之后: 我 ");
    btree_node_dump(child);
    printf(" | 右兄弟 ");
    btree_node_dump(sib);
    printf("\n");
}

/*
 * 合并孩子：把 child[j+1] 整个搬进 child[j]，父亲的 key[j] 拉下来当"桥"，
 *            然后父亲少一个键、少一个空隙，被掏空的 child[j+1] 当场 free。
 *
 *      合并前：  父亲 [40 | 70]
 *                child[1]=[ ]（欠键 0 个）    child[2]=[70 的右兄弟，只有 1 个键 55]
 *                （桥键 = key[1] = 40）
 *
 *      ┌──────── child[j] ────────┐┌──────── child[j+1] ────────┐
 *      │ 键: (空)      空隙: c0    ││ 键: 55      空隙: c1  c2    │
 *      └──────────────────────────┘└─────────────────────────────┘
 *                 ↓ 父亲的桥键 40 落进 child[j] 的尾部，
 *                   再把 child[j+1] 的所有键、所有空隙顺序接上去
 *      ┌──────── 合并后的 child[j] ─────────┐
 *      │ 键: 40 | 55      空隙: c0  c1  c2  │   ← 1 + 1 + 1 = 3 个空隙 = 2 键 + 1 ✓
 *      └────────────────────────────────────┘
 *      父亲 [70]（少了一个键、少了一个空隙）
 *
 * 为什么合并完一定不超载？（m=3 的账，前面算过：0 + 桥 + 1 = 2 = MAX_KEYS）
 *   一般情形：欠键孩子最多 MIN_KEYS-1 个键，能走到这一步说明兄弟 ≤ MIN_KEYS 个键
 *   → 合并后 ≤ (MIN_KEYS-1) + 1 + MIN_KEYS = 2·ceil(m/2)-1 ≤ m-1 = MAX_KEYS ✓
 *   （B 树定义里 MIN_KEYS = ceil(m/2)-1 正是为了让这个不等式成立而设计的。）
 *
 * 别忘了 free(right)：合并后那个空壳节点已经没有任何指针指向它了，
 * 不 free 就是内存泄漏（ASan 会报 leak，而且这类泄漏特别容易被忽略 ——
 * 它看起来"只是个空节点"，但删一万次就漏一万个）。
 */
static void merge_children(BTreeNode *node, int j)
{
    BTreeNode *left  = node->child[j];
    BTreeNode *right = node->child[j + 1];

    char s_left[BTREE_NODE_STR_LEN], s_right[BTREE_NODE_STR_LEN], s_par[BTREE_NODE_STR_LEN];
    btree_node_str(left,  s_left,  sizeof s_left);
    btree_node_str(right, s_right, sizeof s_right);
    btree_node_str(node,  s_par,   sizeof s_par);

    int lc = left->num_keys;                    /* left 原有几个键 */
    int rc = right->num_keys;                   /* right 要搬几个键 */

    /* ① 父亲的桥键落到 left 的末尾 */
    left->key[lc] = node->key[j];
    /* ② right 的键接在后面 */
    for (int k = 0; k < rc; k++) {
        left->key[lc + 1 + k] = right->key[k];
    }
    /* ③ right 的空隙接在后面：right 有 rc 个键 → rc+1 个空隙，一个都不能漏 */
    for (int k = 0; k <= rc; k++) {
        left->child[lc + 1 + k] = right->child[k];
    }
    left->num_keys = lc + 1 + rc;

    /* ④ right 擦干净，变成一个真正无牵连的空壳 */
    for (int k = 0; k < rc; k++) {
        right->key[k] = 0;
    }
    for (int k = 0; k <= rc; k++) {
        right->child[k] = NULL;
    }
    right->num_keys = 0;

    /* ⑤ 父亲少一个空隙、少一个键（顺序别反：见 remove_child_at 的注释） */
    remove_child_at(node, j + 1);
    remove_key_at(node, j);
    free(right);

    printf("      没得借 → 合并：父亲 %s 的 key[%d] 当桥，%s + %s 焊成一个 → 合并后 ",
           s_par, j, s_left, s_right);
    btree_node_dump(left);
    printf("\n");
    printf("        父亲少一个键少一个空隙，变成 ");
    btree_node_dump(node);
    printf("\n");
}

/*
 * fix_child：孩子 child[i] 欠键了，父亲负责把它救活。优先级 借左 → 借右 → 合并。
 *
 * 为什么先试左兄弟？纯粹是"随手写的固定顺序"，先左先右都不影响正确性，
 * 只影响最终的树长什么样（B 树本来就不是唯一的，README 有讨论）。
 *
 * 边界情况（这段的 if 条件是全节最绕的地方，逐条对上号）：
 *   · i > 0                    ：有左兄弟。左兄弟键数 > MIN_KEYS 才肯借（自己得留够）。
 *   · i < num_keys             ：有右兄弟。同样 > MIN_KEYS 才借。
 *                               （child[num_keys] 是最右孩子，它没有右兄弟。）
 *   · 都不肯借 → 合并：
 *       i > 0      → 和左兄弟合并（merge_children(node, i-1)：把 child[i] 并进 child[i-1]）
 *       i == 0     → 必然有右兄弟（此刻父亲 num_keys ≥ 1，孩子 ≥ 2 个），
 *                    merge_children(node, 0) 把 child[1] 并进 child[0]。
 *   注意 i == 0 时上一步是"和左边的自己合并右兄弟"，而不是"把左边并过来" ——
 *   实现上统一成"把 j+1 并进 j"，所以取 j = 0。写反了就会得到"键序倒过来"的树。
 */
static void fix_child(BTreeNode *node, int i)
{
    if (i > 0 && node->child[i - 1]->num_keys > BTREE_MIN_KEYS) {
        borrow_from_left(node, i);
        return;
    }
    if (i < node->num_keys && node->child[i + 1]->num_keys > BTREE_MIN_KEYS) {
        borrow_from_right(node, i);
        return;
    }
    if (i > 0) {
        merge_children(node, i - 1);
    } else {
        merge_children(node, 0);
    }
}

/*
 * remove_one：从"以 node 为根的子树"里删掉 key。
 *
 * 返回值约定（跟 insert_one 完全对称，但含义反过来了）：
 *   1 = 本节点【欠键】了，需要父亲救我（借或合并）
 *   0 = 本节点仍然合法（可能删了、也可能压根没找到）
 * "到底删掉没有"不占返回值，用 *found_out 带出来（C 语言只有一个 return 口子）。
 *
 * 三类节点的三种处理（这就是删除的全部难度）：
 *   A 叶子且命中        ：直接删。删完可能欠键 → 返回 1 让父亲救。
 *   B 内部节点且命中    ：不真删！用前驱顶替，然后到左子树里删前驱（保证落到叶子）。
 *   C 没命中            ：往对应空隙钻，回来若孩子报欠键就 fix_child。
 *
 * 情况 B 里"先顶替再递归删除"的顺序很关键：
 *   node->key[pos] = pred 必须【在】递归之前做。因为递归回来可能发生 fix_child，
 *   它会搬动本节点的键数组；如果那时候下标 pos 还留着旧值，
 *   搬完就把旧值又搬回来了，等于"删了个寂寞"——键还在树里。
 *   （另一种等价写法是用后继 = 右子树最小键，那样就去 child[pos+1] 里删、
 *     要救的孩子也是 pos+1。本节只用前驱，代码只有一条路，好读。）
 */
static int remove_one(BTreeNode *node, int key, int *found_out)
{
    char buf[BTREE_NODE_STR_LEN];

    /* ---------------- A：叶子，真正动手删的地方 ---------------- */
    if (node->is_leaf) {
        int pos = btree_key_pos(node, key);
        if (pos < 0) {
            btree_node_str(node, buf, sizeof buf);
            printf("      叶子 %s 里没有 %d → 什么都不删\n", buf, key);
            return 0;
        }
        btree_node_str(node, buf, sizeof buf);
        printf("      叶子删除前 %s，删掉 key[%d]=%d → ", buf, pos, key);
        remove_key_at(node, pos);
        *found_out = 1;
        btree_node_dump(node);
        printf("\n");
        return (node->num_keys < BTREE_MIN_KEYS) ? 1 : 0;   /* 1 = 我欠键了，爹救我 */
    }

    /* ---------------- B / C：内部节点 ---------------- */
    int pos = btree_key_pos(node, key);
    int child_index;                          /* 要动哪个孩子（也是回头要救的对象） */
    int child_underflow = 0;

    if (pos >= 0) {
        /* B：命中内部节点。前驱顶替 → 去左子树删前驱。 */
        int pred = subtree_max_key(node->child[pos]);
        btree_node_str(node, buf, sizeof buf);
        printf("      命中内部节点 %s 的 key[%d]=%d；它左边挂着 child[%d]、右边挂着 child[%d]，"
               "直接摘掉这个键两棵子树就断了\n", buf, pos, key, pos, pos + 1);
        printf("      → 取左子树最大键（前驱）%d 顶到 key[%d]，再去左子树里删掉 %d\n", pred, pos, pred);
        node->key[pos] = pred;                /* ⚠ 必须先顶替，理由见函数头 */
        child_index = pos;
        child_underflow = remove_one(node->child[pos], pred, found_out);
    } else {
        /* C：没命中，往对应空隙钻 */
        btree_node_str(node, buf, sizeof buf);
        child_index = btree_gap_pos(node, key);
        printf("      内部节点 %s 里没有 %d → 往空隙 child[%d] 钻\n", buf, key, child_index);
        child_underflow = remove_one(node->child[child_index], key, found_out);
    }

    /* 孩子在下面喊"我欠键了"，父亲必须在【本层】救，不能继续往下走 ——
     * 因为救的动作（借/合并）改的正是父亲的键数组和孩子数组。 */
    if (child_underflow) {
        printf("      孩子 child[%d] 欠键（现在只剩 %d 个键，下限是 %d）→ 父亲出手救它\n",
               child_index, node->child[child_index]->num_keys, BTREE_MIN_KEYS);
        fix_child(node, child_index);
    }

    /* 救完（或者压根没救）之后，检查【自己】是不是因为这次合并瘦下去了。
     * 是 → 返回 1，把欠键的事继续往上传，这就是"连锁合并"的推进方式。
     * 注意：只有 fix_child 走了"合并"分支，本节点的键数才会 -1；借键不改变本节点键数。 */
    return (node->num_keys < BTREE_MIN_KEYS) ? 1 : 0;
}

/*
 * btree_remove：对外的删除入口。比 remove_one 多干的只有一件事——【根被掏空】。
 *
 * 根是唯一"没人救"的节点（它没有父亲），所以它的欠键只能这么办：
 *   根 num_keys == 0 且是内部节点 → 它只剩 1 个孩子，让这个孩子直接当新根，
 *                                    老根 free 掉 → 树矮一层。
 *   根 num_keys == 0 且是叶子     → 树变成空树（仍留一个 0 键叶子当根，见 btree_new）。
 *   根 num_keys ≥ 1 但 < MIN_KEYS → 合法！B 树定义里根是唯一的例外，
 *                                    它只要求"内部节点至少 2 个孩子"，不要求 ≥ MIN_KEYS 个键。
 *
 * 为什么允许这个例外？因为不许的话，树就只有"满的"和"空的"两种状态了 ——
 * 删除时根会非常难处理（每次根瘦一点都要分裂式扩张补键，等于把树往回撑大）。
 *
 * 老根 free 之前一定先把 child[0] 置 NULL：
 *   那个孩子已经交给新根了，老根里留着它的地址就是"两个父亲"，
 *   将来 btree_free 会把它释放两次（double free）。摘干净再 free，
 *   是这类"把一个节点从结构里拆出来"操作的标准动作。
 */
int btree_remove(BTreeNode **root, int key)
{
    if (root == NULL || *root == NULL) {
        printf("      [错误] 树还没建（root 是 NULL），先调 btree_new()\n");
        return -1;
    }
    printf("    ▶ 删除 %d\n", key);

    int found = 0;
    int root_underflow = remove_one(*root, key, &found);

    if (root_underflow) {
        BTreeNode *r = *root;
        if (r->num_keys == 0) {
            if (r->is_leaf) {
                printf("      根叶子被删空 → 现在是一棵空树（仍然留着一个 0 键的叶子当根）\n");
            } else {
                BTreeNode *new_root = r->child[0];
                r->child[0] = NULL;                   /* 摘干净：孩子归新根了，别让老根带着它 */
                r->num_keys = 0;
                free(r);                              /* 老根只剩 0 个键 0 个孩子，可以放了 */
                *root = new_root;
                printf("      根被掏空（0 键）→ 根退化成它唯一的孩子，树高 -1。新根 ");
                btree_node_dump(new_root);
                printf("\n");
            }
        } else {
            printf("      根的键数少于 MIN_KEYS=%d，但它还有 %d 个键 %d 个空隙 → "
                   "根是唯一允许「瘦身」的节点，合法，不用救\n",
                   BTREE_MIN_KEYS, r->num_keys, r->num_keys + 1);
        }
    }
    if (!found) {
        printf("      结论：树里本来就没有 %d，一个字节都没动\n", key);
    }
    return found ? 1 : 0;
}

/* ===================================================================
 * 第 5 组：打印视图 + 统计
 * =================================================================== */

int btree_height(BTreeNode *root)
{
    if (root == NULL) {
        return 0;
    }
    if (root->is_leaf) {
        return 1;
    }
    return 1 + btree_height(root->child[0]);   /* 所有叶子同层，看最左边一条就够 */
}

int btree_count(BTreeNode *root)
{
    if (root == NULL) {
        return 0;
    }
    int total = root->num_keys;
    if (!root->is_leaf) {
        for (int i = 0; i <= root->num_keys; i++) {
            total += btree_count(root->child[i]);
        }
    }
    return total;
}

/* 递归到第 target 层，把那一层的节点全打出来（= 一层 BFS，但不用队列）。
 * 为什么不用队列？本目录必须自包含，不能去 include 04-queue；
 * 而"按层打印"用"递归深度 == 目标层"这个条件就能实现，一行数组都不用。
 * 代价是最坏情况要重复走几遍树 —— 教学 demo 完全无所谓。 */
static void print_one_level(BTreeNode *node, int target, int cur)
{
    if (node == NULL) {
        return;
    }
    if (cur == target) {
        btree_node_dump(node);
        printf("   ");
        return;                          /* 只打这一层，孩子留给下一个递归分支去数 */
    }
    if (node->is_leaf) {
        return;                          /* 叶子没有下一层，走过头了 */
    }
    for (int i = 0; i <= node->num_keys; i++) {
        print_one_level(node->child[i], target, cur + 1);
    }
}

void btree_print_levels(BTreeNode *root)
{
    int h = btree_height(root);
    printf("    按层看（一个方框 = 磁盘上的一个页）：\n");
    if (h == 0 || btree_count(root) == 0) {
        printf("      （空树：只有一个 0 键的叶子当根）\n");
        return;
    }
    for (int d = 0; d < h; d++) {
        printf("      第 %d 层  ", d + 1);
        print_one_level(root, d, 0);
        printf("\n");
    }
}

/* 缩进树形视图：把"键之间的空隙"显式标出来，这是理解 child[] 最直观的一张图。 */
#define BTREE_LINE_LEN 256

static void print_tree_rec(BTreeNode *node, const char *line_prefix,
                           const char *label, int i_am_last)
{
    /* 本行 = 父亲的续行缩进 + 我这行的连接符 + 方框 */
    printf("    %s%s", line_prefix, label);
    btree_node_dump(node);
    printf("\n");
    if (node->is_leaf) {
        return;
    }
    /* 给孩子们用的缩进前缀：我不是老小 → 下面还得画一条竖线把兄弟串下去；
     * 我是老小 → 这条分支到此结束，填 4 个空格。
     * ⚠ 注意区分：这个前缀是给【孩子的那一行】用的，不是给自己那行用的。
     *   一开始写成"给孩子行同时加竖线"，打出来就是 │ 跑到 ├─ 前面去了。 */
    char child_prefix[BTREE_LINE_LEN];
    snprintf(child_prefix, sizeof child_prefix, "%s%s", line_prefix, i_am_last ? "    " : "│   ");

    for (int i = 0; i <= node->num_keys; i++) {
        int last = (i == node->num_keys);            /* 老小用 └─，其余用 ├─ */
        char child_label[32];
        snprintf(child_label, sizeof child_label, "%s空隙%d → ", last ? "└─ " : "├─ ", i);
        print_tree_rec(node->child[i], child_prefix, child_label, last);
    }
}

void btree_print_tree(BTreeNode *root)
{
    printf("    树形视图（└─ 空隙k 就是 child[k]，它管的是两道键之间的那段区间）：\n");
    if (root == NULL) {
        printf("      （空树）\n");
        return;
    }
    if (btree_count(root) == 0) {
        printf("      （空树：只有一个 0 键的叶子当根）\n");
        return;
    }
    print_tree_rec(root, "", "", 1);                 /* 根没兄弟，按"老小"处理 */
}

/* 中序遍历：对 B 树就是"每个空隙前依次输出键"，读出来必须从小到大。
 * 这是最便宜的正确性体检：任何一次插入/删除之后打一遍，
 * 顺序不对就说明指针搬错了一格。 */
static void inorder_rec(BTreeNode *node)
{
    if (node == NULL) {
        return;
    }
    for (int i = 0; i < node->num_keys; i++) {
        if (!node->is_leaf) {
            inorder_rec(node->child[i]);       /* 先走第 i 个空隙（全都比 key[i] 小） */
        }
        printf("%d ", node->key[i]);
    }
    if (!node->is_leaf) {
        inorder_rec(node->child[node->num_keys]);   /* 最后一个空隙：比所有键都大 */
    }
}

void btree_print_inorder(BTreeNode *root)
{
    printf("    中序（必须从小到大，这是最省事的一次体检）：");
    if (root == NULL || btree_count(root) == 0) {
        printf("（没有键）\n");
        return;
    }
    inorder_rec(root);
    printf("\n");
}
