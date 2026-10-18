#include <stdio.h>
#include <stdlib.h>
#include "mini_bst.h"

/*
 * ===================================================================
 * mini_bst.c —— 故意写得"什么都不做"的 BST
 * ===================================================================
 * 这个文件的全部意义就是：让读者看到"不加平衡检查"时，代码有多短，
 * 而树坏掉有多彻底。函数都只有几行，正因为如此，退化才来得悄无声息。
 */

MiniBSTNode *mini_bst_new(int value)
{
    MiniBSTNode *node = malloc(sizeof(MiniBSTNode));
    if (node == NULL) {
        printf("      [错误] mini_bst_new 内存申请失败\n");
        return NULL;
    }
    node->value = value;
    node->left = NULL;
    node->right = NULL;
    return node;
}

/*
 * 插入：比当前节点小往左走，大往右走，走到 NULL 就挂上新节点。
 * 没有高度更新、没有平衡检查、没有旋转 —— 三行逻辑搞定。
 *
 *   插 1,2,3,4 的过程（每次都只能往右走，因为后面都比前面大）：
 *
 *   插 1:      1
 *
 *   插 2:      1
 *                \
 *                 2
 *
 *   插 3:      1
 *                \
 *                 2
 *                   \
 *                    3
 *
 *   它不会"自己扶正"，于是最后是一条链。
 */
MiniBSTNode *mini_bst_insert(MiniBSTNode *root, int value)
{
    if (root == NULL) {
        return mini_bst_new(value);      /* 走到空位了，就地造节点 */
    }
    if (value < root->value) {
        root->left = mini_bst_insert(root->left, value);
    } else if (value > root->value) {
        root->right = mini_bst_insert(root->right, value);
    }
    /* 相等的值直接忽略：教学版本保证树里没有重复元素 */
    return root;                          /* 根没换人，所以返回原根 */
}

/*
 * 现算高度：因为节点里没存 height，只能每次从根递归到底。
 * 代价 O(n)。这正是 AVL 用 height 字段换掉的东西。
 */
int mini_bst_height(const MiniBSTNode *root)
{
    if (root == NULL) {
        return 0;                        /* 约定：空树高度 0 */
    }
    int hl = mini_bst_height(root->left);
    int hr = mini_bst_height(root->right);
    return 1 + (hl > hr ? hl : hr);      /* 自己这一层 + 更高的那侧 */
}

int mini_bst_count(const MiniBSTNode *root)
{
    if (root == NULL) {
        return 0;
    }
    return 1 + mini_bst_count(root->left) + mini_bst_count(root->right);
}

/*
 * 查找 value 要比较几次：从根开始，每经过一个节点算一次比较。
 * 这个次数就是"这条路径的深度 + 1"，形状坏掉时它等于节点总数。
 */
int mini_bst_compare_count(const MiniBSTNode *root, int value)
{
    int steps = 0;
    const MiniBSTNode *cur = root;
    while (cur != NULL) {
        steps++;
        if (value == cur->value) {
            return steps;                 /* 命中，比较次数到手 */
        }
        cur = (value < cur->value) ? cur->left : cur->right;
    }
    return -1;                            /* 值不在树里 */
}

void mini_bst_print_inorder(const MiniBSTNode *root)
{
    if (root == NULL) {
        return;
    }
    mini_bst_print_inorder(root->left);
    printf("%d ", root->value);
    mini_bst_print_inorder(root->right);
}

/*
 * 横向树形打印，和 avl.c 里那套缩进规则完全一致，方便并排对比：
 *   根在最上面，L 支在中间，R 支往下；孩子缩进一层。
 * 这里没有 height/bf 可标，所以只标节点值 —— 但"形状"本身就说明了问题。
 */
static void mini_print_rec(const MiniBSTNode *node, const char *prefix,
                           const char *edge, int is_last_child)
{
    if (node == NULL) {
        return;
    }
    printf("%s%s%d\n", prefix, edge, node->value);

    char child_prefix[160];
    snprintf(child_prefix, sizeof(child_prefix), "%s%s",
             prefix, is_last_child ? "   " : "│  ");

    int has_left = (node->left != NULL);
    int has_right = (node->right != NULL);
    if (has_left) {
        mini_print_rec(node->left, child_prefix, has_right ? "├─L " : "└─L ", !has_right);
    }
    if (has_right) {
        mini_print_rec(node->right, child_prefix, "└─R ", 1);
    }
}

void mini_bst_print_tree(const MiniBSTNode *root)
{
    if (root == NULL) {
        printf("  (空树)\n");
        return;
    }
    mini_print_rec(root, "", "", 1);
}

/*
 * 销毁：后序 —— 先 free 左右孩子，再 free 自己。
 * 如果先 free 自己，node->left 就成了悬空指针，接下来读它就是未定义行为。
 */
void mini_bst_destroy(MiniBSTNode *root)
{
    if (root == NULL) {
        return;
    }
    mini_bst_destroy(root->left);
    mini_bst_destroy(root->right);
    free(root);
}
