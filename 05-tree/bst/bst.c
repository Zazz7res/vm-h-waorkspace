#include <stdio.h>
#include <stdlib.h>
#include "bst.h"

/*
 * ===================================================================
 * bst.c —— 二叉搜索树实现
 *
 * 全程只守一条铁律：对【每个】节点都成立
 *     左子树所有值 < 该节点值 < 右子树所有值
 *
 * 一旦某次操作可能破坏它（尤其是删除），我们就得想办法把它救回来。
 * 插入 / 查找 / 删除 的代码之所以能这么短，全是这条铁律给的：
 *   每一步只看一眼当前节点，就能决定"往左走还是往右走"，另一边走都不用看。
 * ===================================================================
 */

/* ===================================================================
 * 1. node_new：造一个孤立节点
 * ===================================================================
 *   ┌──────────┬────────┬────────┐
 *   │ val = v  │  NULL  │  NULL  │   ← 两张纸条都空白，等会儿有人来写地址
 *   └──────────┴────────┴────────┘
 * left/right 必须显式置 NULL：malloc 回来的内存是垃圾值，
 * 不写 NULL 就等于"这张纸条上刚好写着某个随机地址"，遍历必崩。
 */
Node *node_new(int val)
{
    Node *node = malloc(sizeof *node);
    if (node == NULL) {
        printf("  [错误] malloc 失败，造不出值为 %d 的节点\n", val);
        return NULL;
    }
    node->val = val;
    node->left = NULL;
    node->right = NULL;
    return node;
}

/* ===================================================================
 * 2-A. bst_insert（版本 B：返回新根，最常用）
 * ===================================================================
 * 【为什么插入一定落在一片"叶子空位"上？】
 *   插入的过程就是"拿着新值从根往下走一遍查找"：
 *   每到一个节点就比一次大小，决定钻左还是钻右；
 *   一直走到"纸条是 NULL"为止 —— 而 NULL 只可能出现在叶子（或只有一个
 *   孩子的节点）的下面。所以新节点永远是【挂在某个空指针位上】，
 *   绝不会插到两个已有节点中间，也不需要像数组那样挪动别人。
 *
 *   以插入 35 进这棵树为例（根 50）：
 *
 *        50                     35 < 50  → 往左
 *       /  \                   35 > 30  → 往右
 *     30    70                 35 < 40  → 往左，发现是 NULL ← 就插在这！
 *       \
 *       40   ← 40 的 left 纸条本来是空的
 *
 *   内存上只发生了一件事：`40->left = 新节点地址`（写一张纸条，不复制数据）。
 *   所以 BST 插入的耗时 == 从根走到那个空位的步数 == 树高 O(h)。
 *   h ≈ log n（平衡时）就快；h == n（退化）就慢 —— 见文件末尾的退化演示。
 *
 * 【为什么这个版本用"返回值"而不是"二级指针"？】
 *   函数把"我这层处理完之后，我这棵子树的根是谁"回给上一层，
 *   上一层写 `node->left = bst_insert(node->left, v);` 就自动完成了挂接；
 *   空树时传进来的 root 是 NULL，返回新节点，调用方 `root = bst_insert(root, v)`
 *   也就自然把根挂上了。全程只用一级指针，读起来不用绕。
 *   下一节 AVL 必须靠这个写法（旋转会换根），所以现在先习惯它。
 */
Node *bst_insert(Node *root, int val)
{
    if (root == NULL) {
        /* 走到空位了：新节点就生在这里，并把它交回给上一层去接 */
        return node_new(val);
    }
    if (val < root->val) {
        /* 该往左走。用返回值覆盖 root->left：
         * 下面若真的新建了节点，这里就等于"把新节点接到左边"；
         * 若只是一次空插入（值重复），赋回原值也无害。 */
        root->left = bst_insert(root->left, val);
    } else if (val > root->val) {
        root->right = bst_insert(root->right, val);
    } else {
        /* val == root->val：本节约定不放重复值，直接忽略。
         * （真需要存重复值时，常见做法是节点里加一个 count 字段，
         *   或者一律允许重复值往右边走 —— 但那会让"严格升序"变成"非降序"。） */
        printf("  [提示] 值 %d 已存在，本节约定不插入重复值\n", val);
    }
    return root;                        /* 根还是原来那个：把这棵子树原样交回去 */
}

/* ===================================================================
 * 2-B. bst_insert_pp（版本 A：二级指针）
 * ===================================================================
 * 和 00-c-foundation/pointer-basics 里那句"想改外面的变量就得传地址"完全同理：
 *
 *   要改"根指针本身"（空树时得让 root 从 NULL 变成新节点地址），
 *   那就必须拿到 root 这个指针变量的地址 —— 也就是 Node **。
 *
 *   main 里:      root（一个指针变量，地址假设 0x100）
 *                    │
 *   调用时传 &root ─┘  → 函数收到 root_in_out = 0x100
 *                       *root_in_out 就是 root 本身！
 *                       写 *root_in_out = 新节点;  ← 直接改到外面
 *
 *   两个版本效果一模一样，只是"把新根交回去"的方式不同：
 *     版本 A 走【写进caller给的地址】，版本 B 走【return】。
 *   教材/工程里版本 B 更常用（见上面注释），但面试和很多老教材写的是版本 A，
 *   而且它把"二级指针"这件事演得很直白，所以两版都实现、main.c 里都跑一遍。
 */
void bst_insert_pp(Node **root_in_out, int val)
{
    if (root_in_out == NULL) {
        printf("  [错误] bst_insert_pp 收到空的二级指针，无法挂接节点\n");
        return;
    }

    Node *cur = *root_in_out;           /* 读出"这棵子树当前的根" */

    if (cur == NULL) {                  /* 空位到了：新节点诞生在这里 */
        *root_in_out = node_new(val);   /* 星号 = 往调用者的指针变量里写地址 */
        return;
    }
    if (val < cur->val) {
        bst_insert_pp(&cur->left, val); /* 传"左孩子这个字段"的地址：还是二级指针 */
    } else if (val > cur->val) {
        bst_insert_pp(&cur->right, val);
    } else {
        printf("  [提示] 值 %d 已存在，本节约定不插入重复值\n", val);
    }
}

/* ===================================================================
 * 3-A. bst_build_from_array：按顺序逐个插入
 * ===================================================================
 * 提醒：数组的【顺序】直接决定树形！
 *   {50,30,70,20,40}   → 一棵矮胖的树
 *   {20,30,40,50,70}   → 一条右斜的链表
 * 同一批数据，不同顺序 = 不同的树。这就是 BST 会退化的根本原因。
 */
Node *bst_build_from_array(const int arr[], int n)
{
    Node *root = NULL;                  /* 一开始是空树 */
    for (int i = 0; i < n; i++) {
        root = bst_insert(root, arr[i]);/* 版本 B 的标准用法：拿返回值更新根 */
    }
    return root;
}

/* ===================================================================
 * 3-B. bst_build_balanced：从"已升序"数组造一棵矮树
 * ===================================================================
 * 思路：数组中间那个数当根 —— 它左边一半都比它小、右边一半都比它大，
 * 铁律天生成立；然后左右两半各自递归地取中间，树高自然 ≈ log2(n)。
 *
 *   sorted = [1,2,3,4,5,6,7]
 *              └────┘ │ └────┘
 *             左子树  4  右子树       → 4 当根，两边各 3 个，完美平衡
 *
 * 造出来的树查任何值最多走 3 步；而 1..7 顺序插入的树查 7 要走 7 步。
 * 这个对比就是"平衡"二字的价值，也是 AVL/红黑树每天在干的活。
 */
static Node *build_balanced_rec(const int sorted[], int lo, int hi)
{
    if (lo > hi) {
        return NULL;                    /* 区间空了：这棵子树是空的 */
    }
    /* 这么写中点而不是 (lo + hi) / 2：防止 lo+hi 溢出 int 范围 */
    int mid = lo + (hi - lo) / 2;

    Node *node = node_new(sorted[mid]);
    if (node == NULL) {
        return NULL;
    }
    node->left = build_balanced_rec(sorted, lo, mid - 1);
    node->right = build_balanced_rec(sorted, mid + 1, hi);
    return node;
}

Node *bst_build_balanced(const int sorted[], int n)
{
    if (n <= 0) {
        return NULL;
    }
    return build_balanced_rec(sorted, 0, n - 1);
}

/* ===================================================================
 * 4. 查找：递归版 / 迭代版 / 带步数版
 * ===================================================================
 * 三个版本是同一套逻辑："跟当前节点比一下 → 相等就中、小了往左、大了往右、
 * 走到 NULL 就说明没有"。
 *
 * 和查找有关的两个复杂度必须分清：
 *   平衡时 h ≈ log2(n) → O(log n)      （128 个节点最多比 8 次）
 *   退化时 h = n       → O(n)          （就是链表）
 * 铁律给的是"每步能砍一半"，但"能不能真砍一半"取决于树形平不平衡。
 */
const Node *bst_find(const Node *root, int val)
{
    if (root == NULL) {
        return NULL;                    /* 走到空位 = 树里没有这个值 */
    }
    if (val == root->val) {
        return root;                    /* 命中 */
    }
    /* 比根小就只看左边，比根大就只看右边 —— 另一边连看都不看，这就是剪枝 */
    if (val < root->val) {
        return bst_find(root->left, val);
    }
    return bst_find(root->right, val);
}

const Node *bst_find_iter(const Node *root, int val)
{
    const Node *cur = root;
    while (cur != NULL) {
        if (val == cur->val) {
            return cur;
        }
        cur = (val < cur->val) ? cur->left : cur->right;  /* 一行决定往哪走 */
    }
    return NULL;                        /* 走到 NULL：没有 */
}

const Node *bst_find_counted(const Node *root, int val, int *steps_out)
{
    const Node *cur = root;
    int steps = 0;

    while (cur != NULL) {
        steps += 1;                     /* 每比较一次记一步 */
        if (val == cur->val) {
            break;                      /* 命中，步数就是它所在的层数 */
        }
        cur = (val < cur->val) ? cur->left : cur->right;
    }
    if (steps_out != NULL) {
        *steps_out = steps;             /* 输出参数：约定后缀 _out 表示"我给你写的" */
    }
    return cur;
}

/* ===================================================================
 * 5. 最小 / 最大：一条道走到黑
 * ===================================================================
 * 铁律说"左子树都比根小"，所以整棵树最小的值 = 从根一路向左走到的那个点。
 * 一路向左能走多远，取决于树往左歪多少 —— 退化时也要 O(n) 步。
 *
 *   找最小   while (cur->left != NULL) cur = cur->left;
 *
 * 为什么删除时要用它？因为"右子树里最小的值"就是这个被删节点的【后继】
 * （中序遍历里紧挨在它后面的那个值），它是唯一能合法顶到被删者位置上的值。
 */
const Node *bst_min_node(const Node *root)
{
    const Node *cur = root;
    while (cur != NULL && cur->left != NULL) {
        cur = cur->left;
    }
    return cur;                         /* 空树返回 NULL */
}

const Node *bst_max_node(const Node *root)
{
    const Node *cur = root;
    while (cur != NULL && cur->right != NULL) {
        cur = cur->right;
    }
    return cur;
}

int bst_min(const Node *root, int *out)
{
    const Node *node = bst_min_node(root);
    if (node == NULL) {
        return 0;                       /* 空树：没有最小值 */
    }
    if (out != NULL) {
        *out = node->val;
    }
    return 1;
}

int bst_max(const Node *root, int *out)
{
    const Node *node = bst_max_node(root);
    if (node == NULL) {
        return 0;
    }
    if (out != NULL) {
        *out = node->val;
    }
    return 1;
}

/* ===================================================================
 * 6. bst_delete：全章最难点 —— 三种情况
 * ===================================================================
 * 签名 `Node *bst_delete(Node *root, int val)`：仍然是"返回新根"的写法。
 * 删除会改变子树的根（删掉的正是子树根的时候），所以必须把新根交回去。
 *
 * 先用铁律找到目标（比它小往左、比它大往右），找到了才分三种情况处理：
 *
 * ────────────────────────────────────────────────────────────────
 * 情况 1：要删的是叶子（两个孩子都是 NULL）
 *
 *      30                    30
 *       \                     \
 *      [20]      →            NULL      ← 父亲左边的纸条变成 NULL
 *
 *   怎么实现：删完之后"这棵子树的根"就是 NULL，函数直接返回 NULL，
 *   父亲那句 `node->left = bst_delete(node->left, val);` 自动把纸条写成 NULL。
 *   一行特判都不用写 —— 这就是"返回新根"写法的优雅之处。
 *
 * 情况 2：要删的只有一个孩子
 *
 *      30                    30
 *       \                     \
 *      [40]     →             35        ← 孩子"顶替"父亲的位置
 *       /                      （和链表里摘掉一个节点完全一样）
 *      35
 *
 *   怎么实现：返回它唯一的那个孩子，让父亲去接住它。
 *
 * 情况 3：左右孩子都有 —— 最麻烦，也是本节存在的意义
 *
 *   不能直接删：删了以后拿"谁"去同时管住左边一堆小值和右边一堆大值？
 *   办法：找一个【合法继任者】把它的值抄过来，再去删掉那个继任者本身。
 *   继任者 = 右子树里最小的那个 = 中序遍历里紧挨在被删者后面的那个值。
 *   妙处：继任者在右子树的最靠左位置，它【不可能有左孩子】，
 *         所以它自己必然落在情况 1 或情况 2 —— 递归一下就搞定了。
 * ────────────────────────────────────────────────────────────────
 *
 * 情况 3 的完整 ASCII 图（这就是 main.c 第 5 幕删根节点 50 的过程）：
 *
 *   删除前：                       第 1 步：定位继任者 = 右子树最小
 *          50                            往右一步到 70，再一路向左 → 60
 *         /  \
 *       30    70                   60 是 70 的左孩子，且 60 自己没有左孩子
 *        \    / \
 *        35 60  80
 *
 *   第 2 步：把 60 的值【抄】到 50 的格子里（节点本身不动，只改 val）
 *          60            ← 值换了，形状没换
 *         /  \
 *       30    70
 *        \    / \
 *        35 60  80       ← 现在 60 出现了两次，必须把下面那个真的删掉
 *
 *   第 3 步：递归 `root->right = bst_delete(root->right, 60)`
 *           在右子树 70(左=60, 右=80) 里删 60 → 60 是叶子 → 情况 1
 *          60
 *         /  \
 *       30    70          ← 70 的左纸条变 NULL
 *        \      \
 *        35     80
 *
 *   校验：中序 = 30 35 60 70 80，仍然严格升序 → 铁律守住了 ✔
 *
 *   ⚠ 为什么抄"右子树最小（后继）"，而不是随便抓一个值？
 *     继任者必须同时满足：比左子树所有值都大（它在右子树里 → 天然成立）、
 *     比右子树剩下所有值都小（取右子树最小 → 成立）。只有它俩个条件都不破坏。
 *     （取"左子树最大 = 前驱"同样合法，两种对称写法，实现一本道，本节约用后继。）
 */
Node *bst_delete(Node *root, int val)
{
    if (root == NULL) {
        /* 走到底也没找到这个值：树不变，原样返回 NULL。
         * 所以"删一个不存在的值"是安全的，不会崩、不会误删别人。 */
        return NULL;
    }

    if (val < root->val) {
        /* 目标在左边：把"左子树删完之后的新根"接回左纸条 */
        root->left = bst_delete(root->left, val);
        return root;
    }
    if (val > root->val) {
        root->right = bst_delete(root->right, val);
        return root;
    }

    /* ------------------- 走到这里：要删的就是我 ------------------- */

    if (root->left == NULL) {
        /* 情况 1（右也是 NULL → 叶子）和 情况 2（只有右孩子）合并处理：
         * "没有左孩子"时，把右孩子整体提上来就行（右孩子可能是 NULL）。
         * 注意顺序：先把要留着的地址抄到局部变量里，再 free 自己 ——
         * free 之后再读 root->right 就是 use-after-free。 */
        Node *replacement = root->right;
        free(root);
        return replacement;
    }
    if (root->right == NULL) {
        /* 情况 2 的另一半：只有左孩子 */
        Node *replacement = root->left;
        free(root);
        return replacement;
    }

    /* 情况 3：左右孩子都在 —— 用后继顶替，然后去右子树删掉后继 */
    const Node *successor = bst_min_node(root->right);   /* 右子树最小 = 后继 */
    root->val = successor->val;                          /* 第 2 步：只抄值，不动指针 */
    root->right = bst_delete(root->right, successor->val);/* 第 3 步：递归删那个重复的后继 */
    return root;
}

/* ===================================================================
 * 7. bst_inorder：中序遍历 = 升序输出
 * ===================================================================
 * 为什么 BST 的中序一定是升序？对每个节点用一次铁律就懂了：
 *   中序 = 左子树全部 → 自己 → 右子树全部
 *   而"左子树所有值 < 自己 < 右子树所有值" —— 递归地套下去就是整树升序。
 *
 * 这条性质重要到：BST 的合法性校验（下一个函数）干脆就是"中序是否严格升序"。
 * 也常被用来"把 BST 转成有序数组"（O(n)）。
 */
void bst_inorder(const Node *root, int out[], int *n_out)
{
    if (root == NULL) {
        return;
    }
    bst_inorder(root->left, out, n_out);
    if (*n_out < BST_CAP) {            /* 防止写爆调用者的数组 */
        out[*n_out] = root->val;
        *n_out += 1;
    }
    bst_inorder(root->right, out, n_out);
}

/* ===================================================================
 * 8-A. bst_is_valid：正确的校验方法
 * ===================================================================
 * 铁律说的是"左子树【所有】值 < 根 < 右子树【所有】值"，
 * 注意是"所有"！所以只能靠中序遍历（它按从小到大把每个值吐出来）来验：
 *   中序严格升序 ⟺ 合法 BST。
 *
 * 实现上不必真开一个数组存结果：只记住"上一个访问到的节点" prev，
 * 每次访问新节点时比一下 prev->val < node->val 即可。
 *
 * 但 prev 得在递归层与层之间【共享并更新】，所以传它的地址（`const Node **`）：
 * 又是 00-c-foundation 那句老话 —— 想改外面的变量，就得把地址传进去。
 * 这里"外面的变量"其实是上一层递归的局部变量，一个道理。
 */
static int inorder_check(const Node *node, const Node **prev_in_out)
{
    if (node == NULL) {
        return 1;                      /* 空树合法 */
    }
    /* ① 先把左子树整个查一遍，同时把 prev 推到"左子树最后一个（=最大）" */
    if (!inorder_check(node->left, prev_in_out)) {
        return 0;
    }
    /* ② 轮到自己：现在 prev 就是中序里紧邻我前面那个节点 */
    if (*prev_in_out != NULL && (*prev_in_out)->val >= node->val) {
        return 0;                      /* 不是严格升序 → 铁律被破坏了 */
    }
    *prev_in_out = node;               /* 更新"上一个访问过的"，交给右子树去比 */
    /* ③ 最后查右子树 */
    return inorder_check(node->right, prev_in_out);
}

int bst_is_valid(const Node *root)
{
    const Node *prev = NULL;           /* 中序刚开始，前面还没有任何节点 */
    return inorder_check(root, &prev);
}

/* ===================================================================
 * 8-B. bst_is_valid_local_only：【错误示范】，专门留着被打脸
 * ===================================================================
 * 很多人（包括很多手写代码）会"直觉地"这么查：
 *   只检查 node->left->val < node->val < node->right->val，然后递归。
 * 听起来很合理，但它只保证了"和亲孩子的关系"，没保证"和曾曾曾祖父的关系"。
 *
 * 反例（main.c 第 6 幕会把它打印出来）：
 *
 *          10
 *         /  \
 *        5    15
 *            /  \
 *           6    20      ← 6 挂在 15 的左边：6 < 15 ✔ 局部看完全合法
 *
 *   但 6 在【10 的右子树】里，按铁律必须 > 10 —— 它却比 10 小，整棵树是非法的！
 *   局部检查会一路放行（10/5、10/15、15/6、15/20 关系都"对"），
 *   而中序遍历吐出来是 5 10 6 15 20 —— 10 后面接了个 6，升序断了 → 正确版抓到了。
 *
 * 所以：判合法要么用"中序严格升序"，要么用"给每个节点传一个 (min, max) 允许区间"。
 */
int bst_is_valid_local_only(const Node *root)
{
    if (root == NULL) {
        return 1;
    }
    if (root->left != NULL && root->left->val >= root->val) {
        return 0;                      /* 只看左亲孩子：错就错在"只" */
    }
    if (root->right != NULL && root->right->val <= root->val) {
        return 0;
    }
    return bst_is_valid_local_only(root->left) && bst_is_valid_local_only(root->right);
}

/* ===================================================================
 * 9. bst_height：树高 —— 快慢的晴雨表
 * ===================================================================
 * 空树 0，单根 1；height = 1 + max(h(左), h(右))。
 * 查找/插入/删除的步数都 == 树高，所以这一行代码就是性能指标本身：
 *   n = 7 时：平衡树高 3；退化成链表高 7。差得不算多；
 *   n = 100 万时：平衡树高 20，退化树高 100 万 —— 天壤之别。
 */
int bst_height(const Node *root)
{
    if (root == NULL) {
        return 0;
    }
    int left_h = bst_height(root->left);
    int right_h = bst_height(root->right);
    return 1 + (left_h > right_h ? left_h : right_h);
}

/* ===================================================================
 * 10. print_tree：横着把树画出来
 * ===================================================================
 * 规则三条（详细推导见本仓库 05-tree/binary-tree 那一节的 README）：
 *   ① 我是父亲的最后一个孩子吗？是 → "└─"，否 → "├─"
 *   ② 给我的孩子用的前缀 = 我的前缀 + (我是最后一个 ? 两个空格 : "│ ")
 *   ③ 左孩子是不是"最后一个"，取决于"有没有右孩子"；右孩子永远是最后一个
 *
 * 对 BST 来说这个函数格外重要：退化成链表时它画出来就是一条竖着挂下来的链，
 * 一眼就能"看见"退化（第 7 幕）；删除接错线时也能立刻在图上发现。
 */
static void print_tree_rec(const Node *node, const char *prefix, int is_last, int is_root)
{
    if (node == NULL) {
        return;
    }

    char next_prefix[BST_CAP];
    if (is_root) {
        printf("%d\n", node->val);
        snprintf(next_prefix, sizeof next_prefix, "%s", prefix);
    } else {
        printf("%s%s%d\n", prefix, is_last ? "└─" : "├─", node->val);
        snprintf(next_prefix, sizeof next_prefix, "%s%s",
                 prefix, is_last ? "  " : "│ ");
    }

    int left_is_last = (node->right == NULL) ? 1 : 0;
    print_tree_rec(node->left, next_prefix, left_is_last, 0);
    print_tree_rec(node->right, next_prefix, 1, 0);
}

void print_tree(const Node *root)
{
    if (root == NULL) {
        printf("  （空树）\n");
        return;
    }
    print_tree_rec(root, "", 1, 1);
}

/* ===================================================================
 * 11. free_tree：后序释放
 * ===================================================================
 * 必须【先删孩子，再删自己】，因为孩子的地址存在父亲身体里：
 *
 *   ❌ free(root); free_tree(root->left);   ← 读已归还的内存 = use-after-free
 *   ✅ free_tree(root->left);
 *      free_tree(root->right);
 *      free(root);
 *
 * 一句话记：纸条上写着储物柜钥匙，先把柜子清空，再扔纸条。
 *
 * ⚠ 特别注意删除函数：bst_delete 里 free 掉的那个节点，是"抄完值之后"才 free 的，
 *   而且先把孩子地址存进局部变量 —— 和本函数同一个道理。
 */
void free_tree(Node *root)
{
    if (root == NULL) {
        return;
    }
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}
