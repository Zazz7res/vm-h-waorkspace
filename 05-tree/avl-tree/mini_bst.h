#ifndef MINI_BST_H
#define MINI_BST_H

/*
 * ===================================================================
 * mini_bst —— 一棵"裸奔"的二叉搜索树，专门用来给 AVL 当反面教材
 * ===================================================================
 *
 * 【为什么本目录要自己写一份 BST】
 *   本仓库每个叶子目录必须完全自包含（Makefile 是按目录独立编译的，
 *   跨目录 include 根本编译不过）。所以这里不引用 05-tree/bst/，
 *   而是在自己目录里写一个最小版本：只有插入、高度、中序、打印、销毁。
 *
 * 【它和 AVL 的唯一区别】
 *   节点里没有 height 字段，插完也【不做任何检查、不旋转】。
 *   就这样一个小改动，后果是：喂给它 1..8 这种顺序数据，
 *   它会变成一条"只有右孩子的链表"，高度 = 节点数 = 8。
 *   同样这批数据喂给 AVL，高度只有 4。
 *   把两棵树并排打出来，就是 AVL 存在的全部理由。
 *
 * 【查找代价怎么算】
 *   朴素 BST 找最大值要沿着右指针一路走到底，比较次数 = 树高。
 *   链表形状 = O(n)；平衡形状 = O(log n)。
 * ===================================================================
 */

typedef struct MiniBSTNode {
    int                 value;
    struct MiniBSTNode *left;
    struct MiniBSTNode *right;
    /* 注意：这里【没有】 height。算高度只能每次现递归，见 mini_bst_height() */
} MiniBSTNode;

/* 造节点。失败返回 NULL。 */
MiniBSTNode *mini_bst_new(int value);

/* 朴素插入：找到位置挂上去，插完什么都不检查。返回子树根（还是原来那个）。 */
MiniBSTNode *mini_bst_insert(MiniBSTNode *root, int value);

/* 现算高度（因为没有 height 字段，只能递归下去数）：空 = 0，叶子 = 1。 */
int mini_bst_height(const MiniBSTNode *root);

/* 节点个数。 */
int mini_bst_count(const MiniBSTNode *root);

/* 查找 value 需要比较几次（沿路数一下）。找不到返回 -1，值不存在时用于说明"最坏也得走到底"。 */
int mini_bst_compare_count(const MiniBSTNode *root, int value);

/* 中序遍历打印，证明它确实是有序的。 */
void mini_bst_print_inorder(const MiniBSTNode *root);

/* 横着打印树形（没有高度可标，就只标值 + 一个提示：形状斜成什么样）。 */
void mini_bst_print_tree(const MiniBSTNode *root);

/* 销毁：先拆孩子再拆自己。 */
void mini_bst_destroy(MiniBSTNode *root);

#endif /* MINI_BST_H */
