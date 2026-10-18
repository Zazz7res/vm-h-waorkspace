#include <stdio.h>
#include "bst.h"

/*
 * main.c —— 二叉搜索树分幕演示
 *
 * 每一幕只做一件事，做完立刻打印【树形 + 中序】：
 *   树形给你看"结构变成什么样"，中序给你看"顺序还对不对（必须升序）"。
 *   这两样一起看，插入/删除的每一步就不再是抽象的指针操作了。
 *
 * 本节主线数据：{50, 30, 70, 20, 40, 60, 80, 35} —— 故意打乱顺序，
 * 这样建出来的树是矮胖的（高度 3），中序结果必然是 20..80 升序。
 */

static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/* 打印一行中序结果：BST 的"体检报告"，只要不是升序就说明树被写坏了 */
static void print_inorder(const char *label, const Node *root)
{
    int buf[BST_CAP];
    int n = 0;
    bst_inorder(root, buf, &n);

    printf("  %s", label);
    for (int i = 0; i < n; i++) {
        printf("%d ", buf[i]);
    }

    /* 顺手自己检查一遍是不是严格升序 —— 别用眼睛查，让程序说 */
    int first_break = -1;
    for (int i = 1; i < n; i++) {
        if (buf[i - 1] >= buf[i]) {
            first_break = i;
            break;
        }
    }
    if (n == 0) {
        printf("  → 空树\n");
    } else if (first_break < 0) {
        printf("  → 严格升序 ✔\n");
    } else {
        printf("  → 不是升序 ✘（第 %d 个位置 %d 后面接了个更小的 %d）\n",
               first_break + 1, buf[first_break - 1], buf[first_break]);
    }
}

/* 数一数树里有多少个节点：中序写进数组，数组长度就是节点数 */
static int count_nodes(const Node *root)
{
    int buf[BST_CAP];
    int n = 0;
    bst_inorder(root, buf, &n);
    return n;
}

/* 一句话概括当前结构：节点数 + 树高。退化时 高度 == 节点数 */
static void print_stats(const Node *root)
{
    printf("  节点数 = %d, 树高 = %d\n", count_nodes(root), bst_height(root));
}

int main(void)
{
    /* ============================================================
       第 1 幕：按打乱的顺序插入，每插一个看一次结构
       ============================================================ */
    section("第 1 幕：插入 8 个乱序值，每插一次打印【树形 + 中序】");

    const int data[] = {50, 30, 70, 20, 40, 60, 80, 35};
    int data_n = (int)(sizeof(data) / sizeof(data[0]));

    Node *tree = NULL;                 /* 空树：root 就是 NULL，一切从这里开始 */
    for (int i = 0; i < data_n; i++) {
        printf("\n  插入 %d：", data[i]);
        /* 新值从根开始比大小，一路钻到第一个 NULL 空位，长在那里 */
        tree = bst_insert(tree, data[i]);
        printf("\n");
        print_tree(tree);
        print_inorder("中序 =", tree);
    }
    printf("\n  看到了吗：不管插入顺序如何，【中序遍历永远是升序】——\n");
    printf("  有序性不是排出来的，是\"左 < 根 < 右\"这条铁律自动保证的。\n");
    printf("  想按序打印一棵 BST，只要中序遍历，一次排序都不用写。\n");

    /* ============================================================
       第 2 幕：插入的两个版本 + 重复值约定
       ============================================================ */
    section("第 2 幕：版本 A（二级指针）vs 版本 B（返回新根），结果完全一样");

    const int pair[] = {40, 20, 60, 10, 30};
    int pair_n = (int)(sizeof(pair) / sizeof(pair[0]));

    Node *tree_a = NULL;
    Node *tree_b = NULL;
    for (int i = 0; i < pair_n; i++) {
        bst_insert_pp(&tree_a, pair[i]);      /* 版本 A：传 tree_a 的地址（&tree_a）*/
        tree_b = bst_insert(tree_b, pair[i]); /* 版本 B：拿返回值更新自己         */
    }
    printf("  版本 A 用 void 返回 + Node ** 参数，调用要写 &tree_a：\n");
    print_tree(tree_a);
    printf("  版本 B 用 Node * 返回新根，调用要写 tree = bst_insert(tree, v)：\n");
    print_tree(tree_b);
    printf("  两棵树形状完全相同 ✔ —— 差别只在\"新根怎么交回给调用者\"。\n");
    printf("  版本 B 更常用：① 只用一级指针，读起来不绕；\n");
    printf("               ② 和删除、旋转天然一致（AVL/红黑树的旋转一定要返回新根）。\n");
    printf("  版本 A 的好处：① 不用写 root = ...，想不起来漏赋值；\n");
    printf("               ② 面试常考，能证明你真的懂\"改指针要传指针的地址\"。\n");

    printf("\n  重复值约定：再插一次 40 →\n");
    tree_b = bst_insert(tree_b, 40);          /* 会打印一行提示，树形不变 */
    printf("  版本 A 再插一次 40 →\n");
    bst_insert_pp(&tree_a, 40);

    free_tree(tree_a);
    tree_a = NULL;
    free_tree(tree_b);
    tree_b = NULL;
    printf("\n  （上面两棵小树已经 free_tree 释放，root 都置回 NULL 了）\n");

    /* ============================================================
       第 3 幕：查找 —— 每比一次就砍掉一半
       ============================================================ */
    section("第 3 幕：查找（递归 / 迭代 / 带比较步数）");

    const Node *hit = bst_find(tree, 35);
    printf("  bst_find(35)   ：%s\n", hit ? "找到 ✔" : "没找到");
    const Node *hit2 = bst_find_iter(tree, 60);
    printf("  bst_find_iter(60)：%s\n", hit2 ? "找到 ✔（迭代版，逻辑一模一样）" : "没找到");

    int steps = 0;
    const Node *miss = bst_find_counted(tree, 45, &steps);
    printf("  查找 45（不存在）：%s，比较 %d 次后走到 NULL\n",
           miss ? "找到" : "确认没有", steps);
    printf("  为什么只比 %d 次而不是 8 次？因为每比一次就【整片】排除掉一边：\n", steps);
    printf("    45 < 50 → 只看左子树（70/60/80 三个数连看都不看）\n");
    printf("    45 > 30 → 只看 30 的右子树\n");
    printf("    45 > 40 → 只看 40 的右子树 = NULL → 结束\n");
    const Node *found7 = bst_find_counted(tree, 35, &steps);
    printf("  查找 35：%s，比较 %d 次（它挂在第 %d 层）\n",
           found7 ? "找到" : "没找到", steps, steps);

    /* ============================================================
       第 4 幕：最小值 / 最大值 —— 一条道走到黑
       ============================================================ */
    section("第 4 幕：最小值 / 最大值");

    int min_val = 0, max_val = 0;
    if (bst_min(tree, &min_val)) {
        printf("  最小值 = %d   （从根一路往 left 走到底：%d → 30 → 20）\n", min_val, 50);
    }
    if (bst_max(tree, &max_val)) {
        printf("  最大值 = %d   （从根一路往 right 走到底：50 → 70 → 80）\n", max_val);
    }
    const Node *min_node = bst_min_node(tree);
    printf("  返回节点地址的用法：min 节点地址 = %p, val = %d\n",
           (void *)min_node, min_node ? min_node->val : -1);
    printf("  为什么删除时要它？因为\"右子树最小\"就是后继 —— 第 5 幕马上用到。\n");

    /* ============================================================
       第 5 幕：删除 —— 三种情况（本节最难）
       ============================================================ */
    section("第 5 幕：删除的三种情况，每删一次打印【树形 + 中序】");

    printf("  删除前的完整结构（下面三次删除都在它身上做）：\n");
    print_tree(tree);
    print_inorder("中序 =", tree);
    print_stats(tree);

    /* ---- 情况 1：叶子 ---- */
    printf("\n  【情况 1】删叶子 20：没孩子，父亲纸条改成 NULL 就完事\n");
    tree = bst_delete(tree, 20);
    print_tree(tree);
    print_inorder("中序 =", tree);

    /* ---- 情况 2：只有一个孩子 ---- */
    printf("\n  【情况 2】删只有一个孩子的 40（它只有左孩子 35）：孩子顶上来\n");
    tree = bst_delete(tree, 40);
    print_tree(tree);
    print_inorder("中序 =", tree);
    printf("  35 整支被\"提\"到了 40 原来的位置，跟链表里摘掉一个节点一模一样。\n");

    /* ---- 情况 3：左右孩子都有 ---- */
    printf("\n  【情况 3】删左右孩子都有的 50（它就是根！）：用后继 60 顶替\n");
    printf("    第 1 步：后继 = 右子树最小 = bst_min_node(50->right) = 60\n");
    printf("    第 2 步：把 60 这个【值】抄进 50 的格子（节点不动，只改 val）\n");
    printf("    第 3 步：去右子树里递归删掉那个重复的 60（它是叶子 → 退回情况 1）\n");
    tree = bst_delete(tree, 50);
    print_tree(tree);
    print_inorder("中序 =", tree);
    printf("  根换人了（50 → 60），但中序仍然严格升序 → 铁律守住了 ✔\n");
    printf("  ⚠ 关键：情况 3 里我们【没有真的摘掉】被删的那个节点，只是把它的值换掉，\n");
    printf("     真正 free 掉的是下面那个后继节点。这就是为什么删完指针关系还都对。\n");

    printf("\n  【附】删一个不存在的值 999：一路走到 NULL 就原样返回，不会误删、不会崩\n");
    tree = bst_delete(tree, 999);
    print_tree(tree);
    print_inorder("中序 =", tree);

    /* ============================================================
       第 6 幕：验证一棵树是不是合法 BST
       ============================================================ */
    section("第 6 幕：验证合法性 —— 正确方法 vs 会被骗的直觉方法");

    printf("  现在这棵树：");
    printf(bst_is_valid(tree) ? "合法 BST ✔\n" : "非法 ✘\n");

    /* 手工造一棵"局部全对、全局非法"的树：
     *          10
     *         /  \
     *        5    15
     *            /  \
     *           6    20     ← 6 在 10 的右子树里，却比 10 小 → 破坏铁律
     */
    Node *bad10 = node_new(10);
    Node *bad5 = node_new(5);
    Node *bad15 = node_new(15);
    Node *bad6 = node_new(6);
    Node *bad20 = node_new(20);
    if (bad10 != NULL && bad5 != NULL && bad15 != NULL && bad6 != NULL && bad20 != NULL) {
        bad10->left = bad5;
        bad10->right = bad15;
        bad15->left = bad6;      /* 6 < 15：跟亲爹比是合法的 */
        bad15->right = bad20;

        printf("\n  现场手工造一棵\"看着挺规矩\"的树：\n");
        print_tree(bad10);
        printf("  直觉方法（只比父子）判定：");
        printf(bst_is_valid_local_only(bad10) ? "合法 ← 被骗了 ✘\n" : "非法\n");
        printf("  正确方法（中序严格升序）判定：");
        printf(bst_is_valid(bad10) ? "合法" : "非法 ✔ 抓到了\n");
        printf("  中序一看就露馅：\n");
        print_inorder("  =", bad10);
        printf("  → 10 后面接了个 6，升序断了。6 挂在 10 的右子树里，却比 10 小。\n");
        printf("  教训：铁律管的是\"整棵左/右子树\"，不是\"亲生孩子\"。\n");
    }
    free_tree(bad10);
    bad10 = NULL;

    /* ============================================================
       第 7 幕：BST 的退化 —— 现场制造一条"链表"
       ============================================================ */
    section("第 7 幕：现场演示 BST 退化（这是下一节 AVL / 红黑树的唯一动机）");

    const int ascending[] = {1, 2, 3, 4, 5, 6, 7};
    int asc_n = (int)(sizeof(ascending) / sizeof(ascending[0]));

    Node *degenerated = bst_build_from_array(ascending, asc_n);
    printf("  按【升序】1,2,3,4,5,6,7 依次插入（每个新值都比所有已有值大 → 一直挂右边）：\n");
    print_tree(degenerated);
    print_stats(degenerated);
    printf("  看到没：它【看起来就是一条链表】—— 高度 7 == 节点数 7，每层只有 1 个节点。\n");

    int steps_bad = 0;
    bst_find_counted(degenerated, 7, &steps_bad);
    printf("  在这条\"链表\"里查找 7：比较 %d 次 → O(n)，剪枝完全没省下任何东西\n", steps_bad);
    printf("    （1→2→3→4→5→6→7，每步只排除 1 个候选，因为一边永远是空的）\n");

    printf("\n  同一批数据，用\"取中间当下根\"的办法建成平衡树：\n");
    Node *balanced = bst_build_balanced(ascending, asc_n);
    print_tree(balanced);
    print_stats(balanced);
    int steps_good = 0;
    bst_find_counted(balanced, 7, &steps_good);
    printf("  在平衡树里查找 7：只比较 %d 次 → O(log n)，同样 7 个数据差了一倍多\n", steps_good);
    printf("    （4→6→7，每步真的砍掉一半候选）\n");

    printf("\n  ★ 数据量再大一档感受更狠：n = 1,000,000\n");
    printf("    平衡：树高约 20 → 查找最多比 20 次\n");
    printf("    退化：树高 1,000,000 → 查找平均比 50 万次\n");
    printf("  所以：**从 BST 到 AVL 到红黑树，全都是在解决这一个\"退化\"问题** ——\n");
    printf("  插入删除之后想办法把树拽回平衡，让高度稳定在 O(log n)。\n");

    /* ============================================================
       第 8 幕：销毁
       ============================================================ */
    section("第 8 幕：销毁所有树（后序释放：先删左右子树，再 free 自己）");

    int n_main = count_nodes(tree);
    int n_chain = count_nodes(degenerated);
    int n_bal = count_nodes(balanced);
    printf("  一共 %d 个节点等着释放 = 主树 %d + 退化树 %d + 平衡树 %d\n",
           n_main + n_chain + n_bal, n_main, n_chain, n_bal);

    free_tree(tree);
    tree = NULL;
    free_tree(degenerated);
    degenerated = NULL;
    free_tree(balanced);
    balanced = NULL;
    printf("  三棵树全部 free 完毕，指针都置 NULL。ASan 跑完应当 0 泄漏。\n");

    /* ============================================================
       收尾
       ============================================================ */
    printf("\n=== 本节结束 ===\n");
    printf("记住五句话：\n");
    printf("  1) 铁律：左子树所有值 < 根 < 右子树所有值，对【每个】节点都成立。\n");
    printf("  2) 中序遍历必然升序 —— 有序性免费送来，也是校验合法性的唯一可靠办法。\n");
    printf("  3) 插入永远落在一个 NULL 空位上；代价是树形由插入顺序决定，会退化。\n");
    printf("  4) 删除三种情况：叶子直接摘、独苗提上来、两个孩子用后继顶替再递归删后继。\n");
    printf("  5) 查找 O(log n) 的前提是平衡；退化就变 O(n) —— AVL/红黑树存在的全部理由。\n");
    return 0;
}
