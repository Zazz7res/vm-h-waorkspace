#include <stdio.h>
#include <stdlib.h>
#include "avl.h"

/*
 * ===================================================================
 * AVL 树 · 实现文件
 * ===================================================================
 * 阅读顺序建议：
 *   1) 先看 4 个旋转函数（本章核心，把指针怎么换看清楚）
 *   2) 再看 avl_rebalance（判断是哪种情况、调用哪个旋转）
 *   3) 最后看 avl_insert / avl_remove（递归下去、回头修正）
 * ===================================================================
 */

/* 全局开关：非 0 时，每次真正发生旋转都会把"旋转前 / 旋转后"的子树打出来。
 * 教学代码打印是好事，正式项目里这种开关应该走日志系统，不该 printf。 */
static int g_avl_verbose = 0;

void avl_set_verbose(int on)
{
    g_avl_verbose = on ? 1 : 0;
}

/* ===================================================================
 * 第 1 组：小工具
 * =================================================================== */

/*
 * -------------------------------------------------------------------
 * avl_height：读一个节点的高度，NULL 当作 0
 * -------------------------------------------------------------------
 * 为什么要单独包一层函数，而不直接写 node->height？
 *   因为平衡因子是"左高 - 右高"，而孩子经常是 NULL。
 *   如果每次都要写 `(node->left ? node->left->height : 0)`，代码会脏得不行。
 *   把这个"空 = 0"的约定收进一个函数，全项目只写一处判空。
 *   这也是"空指针高度为 0"这条约定的唯一定义处。
 */
int avl_height(const AVLNode *node)
{
    if (node == NULL) {
        return 0;
    }
    return node->height;
}

/*
 * -------------------------------------------------------------------
 * avl_balance：平衡因子 bf = 左子树高度 - 右子树高度
 * -------------------------------------------------------------------
 *   bf > 0  → 左边高（左重）
 *   bf = 0  → 一样高
 *   bf < 0  → 右边高（右重）
 * 合法范围只有 -1 / 0 / +1。一旦出现 ±2，这个节点就"塌了"。
 * 注意：这里返回的是【高度差】，不是绝对值。正负号直接告诉我们要往哪边转，
 *       所以调用方靠符号就能选对旋转方向，不用再问"到底哪边高"。
 */
int avl_balance(const AVLNode *node)
{
    if (node == NULL) {
        return 0;
    }
    return avl_height(node->left) - avl_height(node->right);
}

/*
 * -------------------------------------------------------------------
 * avl_update_height：按"1 + max(左高, 右高)"重算自己的高度
 * -------------------------------------------------------------------
 * 只有【孩子的高度已经是对的】时候才能调它 —— 所以调用时机永远是
 * "递归回头的那一行"，自底向上，一路把高度刷上去。
 * 边界：node 为 NULL 直接不管（没人能更新空气）。
 */
void avl_update_height(AVLNode *node)
{
    if (node == NULL) {
        return;
    }
    int hl = avl_height(node->left);
    int hr = avl_height(node->right);
    node->height = 1 + (hl > hr ? hl : hr);
}

/* 数节点个数：空树 0 个，否则 = 我自己 1 个 + 左子树 + 右子树。 */
int avl_count(const AVLNode *node)
{
    if (node == NULL) {
        return 0;
    }
    return 1 + avl_count(node->left) + avl_count(node->right);
}

/* ===================================================================
 * 第 2 组：树形打印（内部实现 + 对外入口）
 *
 * 打印方式是"横着的树"：根写在最上面，左孩子用 ├─L 挂在下面一行，
 * 右孩子用 └─R。这样一行一个节点，长树也放得进终端，
 * 而且每个节点的 (h=,bf=) 直接跟在值后面，形状和平衡状态一起看见。
 *
 * 缩进规则（和 mini_bst.c 完全一致，方便两棵树并排看）：
 *   我给孩子用的缩进 = 我这一行的缩进 + (我是家中最后一个孩子 ? "   " : "│  ")
 *   "最后一个孩子"指的是【右孩子】；右孩子永远排在最后，用 └─。
 * =================================================================== */

static void avl_print_tree_rec(const AVLNode *node, const char *prefix,
                               const char *edge, int is_last_child)
{
    if (node == NULL) {
        return;
    }

    /* 一行一个节点： 8(h=4,bf=1) —— 值、存的高度、算出来的平衡因子 */
    printf("%s%s%d(h=%d,bf=%d)\n", prefix, edge, node->value,
           node->height, avl_balance(node));

    char child_prefix[160];
    snprintf(child_prefix, sizeof(child_prefix), "%s%s",
             prefix, is_last_child ? "   " : "│  ");

    int has_left = (node->left != NULL);
    int has_right = (node->right != NULL);

    if (has_left) {
        /* 左孩子先来：如果右边还有兄弟，它就"不是最后一个"（用 ├─，要画竖线继续连） */
        avl_print_tree_rec(node->left, child_prefix, has_right ? "├─L " : "└─L ", !has_right);
    }
    if (has_right) {
        /* 右孩子永远是最后一个（用 └─，之后不再画竖线） */
        avl_print_tree_rec(node->right, child_prefix, "└─R ", 1);
    }
}

void avl_print_tree(const AVLNode *root)
{
    if (root == NULL) {
        printf("  (空树)\n");
        return;
    }
    avl_print_tree_rec(root, "", "", 1);
}

/* 内部用：旋转现场只打印"这一小块子树"，前面统一缩 6 格，好看 */
static void avl_print_subtree(const AVLNode *node)
{
    avl_print_tree_rec(node, "      ", "", 1);
}

/* ===================================================================
 * 第 3 组：造节点
 * ===================================================================
 * 新节点是"光杆司令"：height = 1（自己这一层，下面没孩子），
 * 两个孩子先置 NULL。任何分支都必须等它挂到树上之后再去更新高度。
 */
AVLNode *avl_new(int value)
{
    AVLNode *node = malloc(sizeof(AVLNode));
    if (node == NULL) {
        printf("    [错误] avl_new 内存申请失败（想要 %zu 字节）\n", sizeof(AVLNode));
        return NULL;
    }
    node->value = value;
    node->left = NULL;
    node->right = NULL;
    node->height = 1;        /* ⚠ 不是 0：空树才是 0，一个节点的高度是 1 */
    return node;
}

/* ===================================================================
 * 第 4 组：四种旋转 —— 全章核心
 *
 * 【旋转到底在保护什么】
 *   二叉搜索树的规矩：左 < 根 < 右。旋转前后这个规矩一点都不能破。
 *   下面每一张图里，A/B/C 代表"某棵子树"，它们内部的结构完全不用管，
 *   旋转只动三个节点（根、孩子、孙子）之间的指针。
 *
 * 【为什么旋转一定要"返回新根"】
 *   右旋之后，x 变成了子树的根，y 降级成 x 的孩子。父节点（或更上面的祖先）
 *   手里攥着的还是 y 的地址，如果不把返回值接回去，整棵树就跟 x 断连了。
 *   所以调用写法永远是： root = avl_rotate_right(root);
 * =================================================================== */

/*
 * -------------------------------------------------------------------
 * 右旋（处理 LL 型）
 * -------------------------------------------------------------------
 * 什么时候用它：某个节点 y 的【左孩子的左边】变高了，bf = +2。
 *
 *        y                             x
 *       / \                           /   \
 *      x   C     右旋 y        →     A     y
 *     / \                                / \
 *    A   B                              B   C
 *
 *   x 上位当根，y 降为 x 的右孩子；x 原来的右孩子 B"改嫁"给 y 当左孩子。
 *   为什么 B 要改嫁给 y？因为按大小关系 A < x < B < y < C，
 *   B 比 y 小、比 x 大，挂在 x 的右边或 y 的左边都合法；
 *   旋转后 x 的右边给了 y，所以 B 只能去 y 的左边。
 *
 * 关键三行（顺序有讲究，先保存 B，否则会丢）：
 *   x->right = y;   // 让 x 的右手指向老上级 y
 *   y->left  = B;   // y 的左手指向改嫁过来的 B
 *   此时 y->left 已经不需要 x 了 —— 但如果先写这一行就会把 x 的地址覆盖掉，
 *   所以必须【先把 B 存进局部变量】再动手。
 */
AVLNode *avl_rotate_right(AVLNode *y)
{
    if (y == NULL || y->left == NULL) {
        /* 正常绝不可能走到这里：bf=+2 时左孩子必然存在。
         * 走到就说明调用方逻辑错了，宁可打印也别硬转（否则解引用 NULL 直接崩）。 */
        printf("    [内部错误] rotate_right 被喂了一棵没有左孩子的树\n");
        return y;
    }

    AVLNode *x = y->left;      /* 要上位的左孩子 */
    AVLNode *b = x->right;     /* 唯一需要搬家的那棵子树，先存住 */

    x->right = y;              /* 上位：x 的右边挂上老上级 y */
    y->left = b;               /* 改嫁：B 变成 y 的左子树（比 y 小，合法） */

    /* 高度必须重算，而且顺序是【先下后上】：
     * y 现在少了左腿（B 顶上去了），它的高度先算出来，x 才能用对。 */
    avl_update_height(y);
    avl_update_height(x);
    return x;                  /* 新根交给调用者接回去 */
}

/*
 * -------------------------------------------------------------------
 * 左旋（处理 RR 型）—— 右旋的镜像
 * -------------------------------------------------------------------
 * 什么时候用它：某个节点 x 的【右孩子的右边】变高了，bf = -2。
 *
 *      x                                  y
 *     / \        左旋 x            →     /   \
 *    A   y                              x     C
 *       / \                            / \
 *      B   C                          A   B
 *
 *   y 上位当根，x 降为 y 的左孩子，y 原来的左孩子 B 改嫁给 x 当右孩子。
 */
AVLNode *avl_rotate_left(AVLNode *x)
{
    if (x == NULL || x->right == NULL) {
        printf("    [内部错误] rotate_left 被喂了一棵没有右孩子的树\n");
        return x;
    }

    AVLNode *y = x->right;     /* 要上位的右孩子 */
    AVLNode *b = y->left;      /* 需要搬家的那棵子树 */

    y->left = x;               /* 上位 */
    x->right = b;              /* 改嫁：B 比 x 大，挂在 x 右边合法 */

    avl_update_height(x);      /* 先下后上 */
    avl_update_height(y);
    return y;
}

/*
 * -------------------------------------------------------------------
 * LR 双旋（处理 LR 型）
 * -------------------------------------------------------------------
 * 什么时候用它：失衡点 y 的 bf = +2，但压高 y 的不是左孩子的【左边】，
 * 而是左孩子的【右边】—— 也就是"先往左走、再往右走"那条路径插了新节点。
 * 这种情况下直接右旋是转不动的：
 *
 *        y                            y
 *       / \        直接右旋 y    →   ┌─┐\
 *      x   C                          x C     ← 形状更难看，x 还多了一条右腿
 *       \                               \
 *        B                               B
 *
 *   所以标准做法分两步：先给左孩子 x 来一次【左旋】，把形状掰成标准 LL，
 *   再对 y 来一次【右旋】。两次旋转，根从 y 变成 B。
 *
 *      y                          y                         B
 *     / \     先左旋 x            / \      再右旋 y         / \
 *    x   C            →         B   C          →         x   y
 *     \                         /                          \
 *      B                       x                            C
 *
 *   （B 可能带自己的左右子树，图里省略；它们跟着 B 一起移动，大小关系不变。）
 */
AVLNode *avl_rotate_lr(AVLNode *y)
{
    if (y == NULL || y->left == NULL) {
        printf("    [内部错误] rotate_lr 的失衡点没有左孩子\n");
        return y;
    }
    y->left = avl_rotate_left(y->left);   /* 第一步：把左子树掰成 LL 形状 */
    return avl_rotate_right(y);           /* 第二步：整体右旋，一次搞定 */
}

/*
 * -------------------------------------------------------------------
 * RL 双旋（处理 RL 型）—— LR 的镜像
 * -------------------------------------------------------------------
 * 什么时候用它：失衡点 x 的 bf = -2，但压高它的是右孩子的【左边】。
 * 先给右孩子来一次【右旋】变成标准 RR，再对 x 来一次【左旋】。
 *
 *    x                             x                          B
 *   / \   先右旋 y       →         / \     再左旋 x     →    / \
 *  A   y                         A   B                       y
 *      /                             \                       / \
 *     B                               y                     A   y
 *                                                    (B 的左子树归 y)
 */
AVLNode *avl_rotate_rl(AVLNode *x)
{
    if (x == NULL || x->right == NULL) {
        printf("    [内部错误] rotate_rl 的失衡点没有右孩子\n");
        return x;
    }
    x->right = avl_rotate_right(x->right); /* 第一步：把右子树掰成 RR 形状 */
    return avl_rotate_left(x);             /* 第二步：整体左旋 */
}

/* ===================================================================
 * 第 5 组：rebalance —— "该用哪种旋转"的裁判
 * ===================================================================
 * 输入：一个高度已经更新过的子树根
 * 输出：修好之后的子树根（可能还是原来那个节点，也可能换了人）
 *
 * 判断只看两个数字：失衡点自己的 bf，和"高那一侧孩子"的 bf。
 * 为什么不用"刚插入的值 v 和谁比"来判断？
 *   因为删除的时候根本没有 v 可以参考；而且值相等被忽略时也会误判。
 *   用孩子的 bf 符号判断，插入和删除能共用同一段代码，这才是稳的。
 *
 *   bf = +2（左重）：孩子的 bf >= 0 → LL，孩子 bf < 0 → LR
 *   bf = -2（右重）：孩子的 bf <= 0 → RR，孩子 bf > 0 → RL
 *   孩子 bf == 0 只会出现在删除时，此时归到单旋那一档就够。
 * =================================================================== */
static AVLNode *avl_rebalance(AVLNode *root)
{
    if (root == NULL) {
        return NULL;
    }

    int bf = avl_balance(root);
    if (bf >= -1 && bf <= 1) {
        return root;                 /* 还没塌，什么都不用做（最常见） */
    }

    if (bf > 1) {                    /* 左重：LL 或 LR */
        int child_bf = avl_balance(root->left);

        if (child_bf >= 0) {         /* 左孩子的左边高 → LL → 一次右旋 */
            if (g_avl_verbose) {
                printf("    [失衡] 节点 %d bf=%d，左重且左孩子 bf=%d → LL 型 → 执行【右旋】\n",
                       root->value, bf, child_bf);
                printf("    旋转前：\n");
                avl_print_subtree(root);
            }
            AVLNode *new_root = avl_rotate_right(root);
            if (g_avl_verbose) {
                printf("    旋转后：\n");
                avl_print_subtree(new_root);
            }
            return new_root;
        }
        /* 左孩子的右边高 → LR → 先左旋左孩子，再右旋 */
        if (g_avl_verbose) {
            printf("    [失衡] 节点 %d bf=%d，左重但左孩子 bf=%d → LR 型 → 执行【左旋子树 + 右旋】\n",
                   root->value, bf, child_bf);
            printf("    旋转前：\n");
            avl_print_subtree(root);
        }
        AVLNode *new_root = avl_rotate_lr(root);
        if (g_avl_verbose) {
            printf("    旋转后：\n");
            avl_print_subtree(new_root);
        }
        return new_root;
    }

    /* bf < -1：右重，RR 或 RL，和上面完全镜像 */
    int child_bf = avl_balance(root->right);
    if (child_bf <= 0) {             /* RR → 一次左旋 */
        if (g_avl_verbose) {
            printf("    [失衡] 节点 %d bf=%d，右重且右孩子 bf=%d → RR 型 → 执行【左旋】\n",
                   root->value, bf, child_bf);
            printf("    旋转前：\n");
            avl_print_subtree(root);
        }
        AVLNode *new_root = avl_rotate_left(root);
        if (g_avl_verbose) {
            printf("    旋转后：\n");
            avl_print_subtree(new_root);
        }
        return new_root;
    }
    /* RL → 先右旋右孩子，再左旋 */
    if (g_avl_verbose) {
        printf("    [失衡] 节点 %d bf=%d，右重但右孩子 bf=%d → RL 型 → 执行【右旋子树 + 左旋】\n",
               root->value, bf, child_bf);
        printf("    旋转前：\n");
        avl_print_subtree(root);
    }
    AVLNode *new_root = avl_rotate_rl(root);
    if (g_avl_verbose) {
        printf("    旋转后：\n");
        avl_print_subtree(new_root);
    }
    return new_root;
}

/* ===================================================================
 * 第 6 组：插入
 * ===================================================================
 * 三步走，全在递归的"回头路"上做：
 *   ① 按 BST 规矩递归找到空位挂新节点
 *   ② 更新我的高度（孩子的高度已经是对的）
 *   ③ 算我的 bf，塌了就调 avl_rebalance，并把新根返回给上一层
 *
 *   插 20 到  10 ← 30（30 是根，bf=+2）  这一路的执行顺序：
 *
 *   insert(30)
 *     ├ 30 > 20 → 去左边： insert(10)
 *     │                     ├ 10 < 20 → 去右边： insert(NULL) → 造节点 20
 *     │                     ├ 更新 10 的高度 → h=2
 *     │                     └ 10 的 bf = 0-1 = -1，不塌，返回 10
 *     ├ root->left = 10                     ← 返回值必须接回去！
 *     ├ 更新 30 的高度 → h=3
 *     └ 30 的 bf = 2-0 = +2 → LR？看左孩子 bf=-1 → LR → 双旋 → 新根是 20
 *
 * 为什么"每层递归只检查这一个节点"就足够？
 *   因为插入只让一条从叶子到根的路径变高。其他分支高度没变，本来合法的
 *   节点不会突然失衡 —— 所以整棵树里【最多只有一个节点】需要旋转，
 *   而且转完这一支的高度就恢复成插入前的高度了，往上无需再动。
 *   （删除不一样：删除可能让好几个祖先都失衡，需要一路修到根。）
 *
 * 边界情况：
 *   - 值已存在：忽略，直接返回原根（保证树里没有重复元素，删除逻辑也更简单）。
 *   - malloc 失败：新节点为 NULL，一路返回 NULL，不会崩，但树会少一个节点。
 *   - 往空树插：第一次递归就命中 root == NULL，直接返回新节点。
 */
AVLNode *avl_insert(AVLNode *root, int value)
{
    /* ① BST 式递归下降 */
    if (root == NULL) {
        return avl_new(value);            /* 找到空位，新节点 height 已经是 1 */
    }
    if (value < root->value) {
        root->left = avl_insert(root->left, value);
    } else if (value > root->value) {
        root->right = avl_insert(root->right, value);
    } else {
        return root;                      /* 重复值：忽略 */
    }

    /* ② 回头路上先刷高度 */
    avl_update_height(root);

    /* ③ 再检查平衡，把（可能换了人的）新根交出去 */
    return avl_rebalance(root);
}

/* ===================================================================
 * 第 7 组：删除
 * ===================================================================
 * 先按 BST 的规矩删，再自底向上重新平衡。
 *
 * BST 删除的三种情况（这里必须讲清，因为 AVL 的删除比插入麻烦）：
 *   (a) 叶子（没孩子）：直接 free，把自己的位置让给 NULL。
 *   (b) 只有一个孩子：把孩子提上来顶替自己，free 自己。
 *       为什么能直接顶替？因为那个孩子整棵子树都比我大（或都比我小），
 *       提到父亲的位置后，大小关系完全成立。
 *   (c) 两个孩子：不能删自己（否则一边子树没地方放）。
 *       标准做法是找【后继】= 右子树里最小的那个节点，
 *       把它的【值】抄进我这边，然后去右子树里删掉那个后继节点。
 *       后继一定没有左孩子（它已经是右子树最小），所以递归下去必然落在 (a) 或 (b)，
 *       递归深度最多两层就终止，不会卡住。
 *
 *        10
 *       /  \       删 10（双孩子）：右子树最小的是 15 → 后继
 *      5    15     把 15 的值抄到根，再去右子树删掉那个"值为 15 的节点"
 *           / \              ↓
 *          12  20            15（原 15 的位子被后继的右孩子 20 顶掉）
 *                            /  \
 *                           12   20
 *
 * 删除后的修正：和插入一样，每层递归都更新高度 + rebalance。
 * 区别是：删掉一个节点会让路径变矮，"变矮"同样能造成 |bf| = 2，
 * 而且一次旋转之后这支的高度可能比原来更矮，所以【每个祖先都要检查一遍】，
 * 最坏 O(log n) 次旋转（插入最多 2 次）。
 */
static AVLNode *avl_min_node(AVLNode *node)
{
    /* 一路往左走到底，就是这棵子树的最小值节点 */
    while (node != NULL && node->left != NULL) {
        node = node->left;
    }
    return node;
}

AVLNode *avl_remove(AVLNode *root, int value)
{
    /* ① 递归找到要删的位置；走空了说明值不在树里，安静返回，不误删任何东西 */
    if (root == NULL) {
        return NULL;
    }

    if (value < root->value) {
        root->left = avl_remove(root->left, value);
    } else if (value > root->value) {
        root->right = avl_remove(root->right, value);
    } else {
        /* ② 命中：分三种情况处理 */
        if (root->left == NULL && root->right == NULL) {
            /* (a) 叶子：直接拆掉，爸爸的位置变成 NULL */
            free(root);
            return NULL;
        }

        if (root->left == NULL || root->right == NULL) {
            /* (b) 单孩子：孩子顶替我。
             * 顶替前先把被弃用的那条腿置 NULL，这样即使以后代码写错重复 free，
             * 也不会变成 double free（free(NULL) 是合法空操作）。 */
            AVLNode *child = (root->left != NULL) ? root->left : root->right;
            root->left = NULL;
            root->right = NULL;
            free(root);
            return child;
        }

        /* (c) 双孩子：拿后继的值顶替，再去右子树里删掉那个后继 */
        AVLNode *succ = avl_min_node(root->right);
        root->value = succ->value;
        root->right = avl_remove(root->right, succ->value);
    }

    /* ③ 回头路：更新高度 → 检查平衡 */
    avl_update_height(root);
    return avl_rebalance(root);
}

/* ===================================================================
 * 第 8 组：查找 / 中序遍历 / 校验 / 销毁
 * =================================================================== */

/*
 * 查找：利用"左小右大"，每次比较扔掉半棵树 → O(log n)。
 * 返回节点地址（只读），找不到返回 NULL。
 */
const AVLNode *avl_search(const AVLNode *root, int value)
{
    const AVLNode *cur = root;
    while (cur != NULL) {
        if (value == cur->value) {
            return cur;
        }
        cur = (value < cur->value) ? cur->left : cur->right;
    }
    return NULL;
}

/*
 * 中序遍历（左 → 根 → 右）：在 BST 上它必然打出升序。
 * 这是"树的结构没被旋转搞坏"的最强证据 —— 旋转改变形状，但绝不能改变顺序。
 */
void avl_print_inorder(const AVLNode *root)
{
    if (root == NULL) {
        return;
    }
    avl_print_inorder(root->left);
    printf("%d ", root->value);
    avl_print_inorder(root->right);
}

/*
 * 事后校验：递归检查两件事
 *   1) 每个节点的 |bf| <= 1（AVL 的定义）
 *   2) 存的 height 和"按孩子重算出来的高度"一致（height 字段有没有被更新漏）
 * 有问题就打印在哪一个节点，返回 0；全程没问题返回 1。
 * 每次操作之后调它一次，等于给自己一个"作弊检测器"。
 */
int avl_is_valid(const AVLNode *root)
{
    if (root == NULL) {
        return 1;
    }

    int ok = 1;

    int bf = avl_balance(root);
    if (bf < -1 || bf > 1) {
        printf("    ✗ 节点 %d 不平衡：bf=%d\n", root->value, bf);
        ok = 0;
    }

    int real_h = 1 + (avl_height(root->left) > avl_height(root->right)
                        ? avl_height(root->left) : avl_height(root->right));
    if (root->height != real_h) {
        printf("    ✗ 节点 %d 存的 height=%d，实际应该是 %d（高度更新漏了）\n",
               root->value, root->height, real_h);
        ok = 0;
    }

    if (!avl_is_valid(root->left)) {
        ok = 0;
    }
    if (!avl_is_valid(root->right)) {
        ok = 0;
    }
    return ok;
}

/*
 * 销毁：后序 —— 先拆左子树、再拆右子树、最后 free 自己。
 * 顺序错了就是悬空指针：如果先 free(root)，后面 root->left 读的是已释放内存。
 * free 之后树本身已经没意义了，但调用者一定要记得把自己的 root 置 NULL，
 * 否则 main 里那个指针就成了野指针（本项目在 main 里都写了 root = NULL）。
 */
void avl_destroy(AVLNode *root)
{
    if (root == NULL) {
        return;
    }
    avl_destroy(root->left);
    avl_destroy(root->right);
    free(root);
}
