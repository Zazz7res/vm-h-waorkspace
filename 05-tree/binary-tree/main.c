#include <stdio.h>
#include "btree.h"

/*
 * main.c —— 二叉树分幕演示
 *
 * 每一幕只做一件事，做完就把结果打印出来，让"看不见的指针操作"变成
 * 屏幕上看得见的一行字。建议一边跑一边对着 README 的图解看。
 *
 * 演示用的树（全仓库固定，所以输出是确定的）：
 *
 *              1
 *            /   \
 *           2     3
 *          / \     \
 *         4   5     6
 */

/* 小工具：打印一行分隔线，让输出好读（和 00-c-foundation 一样的排版习惯） */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/* 小工具：把遍历结果数组打印成一行；n 是数组里已填写的元素个数 */
static void print_seq(const char *label, const int arr[], int n)
{
    printf("  %-22s", label);        /* %-22s = 左对齐占 22 格，多行结果能对齐 */
    for (int i = 0; i < n; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

/* 小工具：判断数组前 n 个元素是否和期望值完全一致（用来自动核对遍历结果） */
static int seq_matches(const int got[], const int want[], int n)
{
    for (int i = 0; i < n; i++) {
        if (got[i] != want[i]) {
            return 0;
        }
    }
    return 1;
}

/* 小工具：把两段结果逐位比对，一样就打个勾 */
static void compare_two(const char *title, const int a[], const int b[], int n)
{
    printf("  %s：%s\n", title, seq_matches(a, b, n) ? "完全一致 ✔" : "不一样 ✘");
    print_seq("  递归版", a, n);
    print_seq("  迭代版", b, n);
}

int main(void)
{
    /* ============================================================
       第 1 幕：手工连指针建树
       ============================================================ */
    section("第 1 幕：手工连指针，把一棵固定的树接出来");

    Node *root = build_fixed_tree();
    if (root == NULL) {
        printf("  内存不够，建不了树，程序退出\n");
        return 1;
    }
    printf("  根节点地址 = %p, 里面存的值 = %d\n", (void *)root, root->val);
    printf("  只要拿到这一个地址，整棵树都能顺着 left/right 走完全程 ——\n");
    printf("  这就是为什么所有树的 API 都只传\"根节点地址\"，不传别的东西。\n");
    printf("\n  树形（print_tree 的输出，本章后面每一步都会印一次）：\n");
    print_tree(root);

    /* ============================================================
       第 2 幕：递归建树（带空标记的前序序列）
       ============================================================ */
    section("第 2 幕：同一棵树，用\"前序 + 空标记\"序列递归建出来");

    /* 读法：遇到一个数就造节点，遇到 -1 就"这个位置是空的"。
     * 顺序 = 根 → 左子树 → 右子树，和递归前序一模一样。 */
    const int marks[] = {
        1,
        2, 4, MARK_EMPTY, MARK_EMPTY,   /* 2 的左孩子 = 4，4 的两个孩子都是空 */
           5, MARK_EMPTY, MARK_EMPTY,   /* 2 的右孩子 = 5，5 的两个孩子都是空 */
        3,
           MARK_EMPTY,                  /* 3 的左孩子 = 空！这个 -1 不能省 */
           6, MARK_EMPTY, MARK_EMPTY,   /* 3 的右孩子 = 6 */
    };
    int marks_n = (int)(sizeof(marks) / sizeof(marks[0]));
    int empty_cnt = 0;                 /* 顺手数一下有几个空标记，别手写死数字 */
    for (int i = 0; i < marks_n; i++) {
        if (marks[i] == MARK_EMPTY) {
            empty_cnt++;
        }
    }
    printf("  序列（共 %d 个，其中 %d 个是空标记 -1）：\n  ", marks_n, empty_cnt);
    for (int i = 0; i < marks_n; i++) {
        printf("%d ", marks[i]);
    }
    printf("\n");

    Node *built = build_tree_from_marks(marks, marks_n);
    printf("  建出来的树：\n");
    print_tree(built);
    printf("  对比一下：两棵树的形状、值完全一样，后面所有操作的结果也一定一样。\n");
    printf("  手工连线适合\"写死的 demo\"；递归建树适合\"从文件/网络读一段描述还原树\"。\n");

    /* ============================================================
       第 3 幕：三种递归遍历 + 手算对照
       ============================================================ */
    section("第 3 幕：前序 / 中序 / 后序（递归版）");

    printf("  手算（对着 README 的图，用\"访问自己的时机\"推）：\n");
    printf("    前序 根→左→右 : 1 2 4 5 3 6\n");
    printf("    中序 左→根→右 : 4 2 5 1 3 6\n");
    printf("    后序 左→右→根 : 4 5 2 6 3 1\n\n");

    const int want_pre[]  = {1, 2, 4, 5, 3, 6};
    const int want_in[]   = {4, 2, 5, 1, 3, 6};
    const int want_post[] = {4, 5, 2, 6, 3, 1};

    int pre[64]; int n_pre = 0;
    int in[64];  int n_in = 0;
    int post[64]; int n_post = 0;

    preorder(root, pre, &n_pre);      /* 注意 &n_pre：函数要改这个计数器，必须传地址 */
    inorder(root, in, &n_in);
    postorder(root, post, &n_post);

    print_seq("  前序（递归）", pre, n_pre);
    print_seq("  中序（递归）", in, n_in);
    print_seq("  后序（递归）", post, n_post);
    printf("  自动核对：前序%s / 中序%s / 后序%s\n",
           seq_matches(pre, want_pre, n_pre) ? "✔" : "✘",
           seq_matches(in, want_in, n_in) ? "✔" : "✘",
           seq_matches(post, want_post, n_post) ? "✔" : "✘");
    printf("  只有前序和中序能唯一还原一棵树？不，\"前序+中序\"或\"后序+中序\"才行；\n");
    printf("  单看前序无法区分孩子归属，所以第 2 幕的序列要带空标记。\n");

    /* ============================================================
       第 4 幕：三种迭代遍历（自己写的显式栈）
       ============================================================ */
    section("第 4 幕：前序 / 中序 / 后序（迭代版，用本目录自带的显式栈）");

    int pre_i[64];  int n_pre_i = 0;
    int in_i[64];   int n_in_i = 0;
    int post_i[64]; int n_post_i = 0;

    preorder_iter(root, pre_i, &n_pre_i);
    inorder_iter(root, in_i, &n_in_i);
    postorder_iter(root, post_i, &n_post_i);

    compare_two("前序：递归 vs 迭代", pre, pre_i, n_pre);
    compare_two("中序：递归 vs 迭代", in, in_i, n_in);
    compare_two("后序：递归 vs 迭代", post, post_i, n_post);
    printf("  结论：递归版之所以能自动\"回头\"，是因为系统在帮你维护一个栈；\n");
    printf("  迭代版不过是把那个栈写成自己的数组。两者空间都是 O(n)。\n");
    printf("  递归更好读、但有栈溢出风险（树很高时）；迭代可控、但要自己想清楚存什么。\n");

    /* ============================================================
       第 5 幕：层序遍历（自己写的队列）
       ============================================================ */
    section("第 5 幕：层序遍历（BFS，一层一行，用本目录自带的队列）");

    printf("  手算：第1层 1 / 第2层 2 3 / 第3层 4 5 6\n\n");
    level_order(root);
    printf("  注意 3 的左边是空的，但 6 照样出现在第 3 层最右边 ——\n");
    printf("  层序遍历看的是\"层\"，不管中间缺了谁。\n");

    /* ============================================================
       第 6 幕：统计 —— 高度、节点总数、叶子数
       ============================================================ */
    section("第 6 幕：求高度 / 数节点 / 数叶子");

    printf("  树的高度      = %d   （空树算 0，固定树是 3 层）\n", tree_height(root));
    printf("  节点总数      = %d\n", tree_count(root));
    printf("  叶子节点数    = %d   （4、5、6 才是叶子；节点 3 只有一个孩子，不算）\n",
           tree_leaf_count(root));
    printf("  第 2 幕那棵递归建的树：高度 %d, 节点 %d, 叶子 %d —— 完全一致\n",
           tree_height(built), tree_count(built), tree_leaf_count(built));

    /* ============================================================
       第 7 幕：查找
       ============================================================ */
    section("第 7 幕：在无序二叉树里查找某个值");

    const Node *hit = tree_find(root, 5);
    if (hit != NULL) {
        printf("  查找 5：找到了，节点地址 = %p, 值 = %d\n", (void *)hit, hit->val);
    } else {
        printf("  查找 5：没找到\n");
    }
    const Node *miss = tree_find(root, 99);
    printf("  查找 99：%s\n", miss == NULL ? "没找到（全树扫了一遍才确认没有）" : "找到了");
    printf("  普通二叉树不能靠大小剪枝，所以最坏要把 n 个节点全问一遍 → O(n)。\n");
    printf("  想要 O(log n) 查找，就得给树加规矩：下一节 BST（左 < 根 < 右）。\n");

    /* ============================================================
       第 8 幕：是否平衡
       ============================================================ */
    section("第 8 幕：判断是否平衡（平衡 = 每个节点左右子树高度差 <= 1）");

    printf("  固定树（高度 %d, 节点 %d）：", tree_height(root), tree_count(root));
    printf(tree_is_balanced(root) ? "平衡 ✔\n" : "不平衡 ✘\n");

    /* 现场造一条"退化成链表"的右斜树：1 → 2 → 3 → 4 全挂右边 */
    Node *chain1 = node_new(1);
    Node *chain2 = node_new(2);
    Node *chain3 = node_new(3);
    Node *chain4 = node_new(4);
    if (chain1 != NULL && chain2 != NULL && chain3 != NULL && chain4 != NULL) {
        chain1->right = chain2;        /* 只有右孩子，长得就像链表 */
        chain2->right = chain3;
        chain3->right = chain4;

        printf("\n  再看一下这棵右斜树：\n");
        print_tree(chain1);
        printf("  高度 %d, 节点 %d → 高度 == 节点数，已经是一条链表了\n",
               tree_height(chain1), tree_count(chain1));
        printf("  判断平衡：");
        printf(tree_is_balanced(chain1) ? "平衡 ✔\n" : "不平衡 ✘（根 1 的左右高度差 = 3 - 0 = 3）\n");
    }
    free_tree(chain1);                 /* 用完就还，别留到最后 */
    chain1 = NULL;

    /* ============================================================
       第 9 幕：两棵树是否相同
       ============================================================ */
    section("第 9 幕：判断两棵树是否完全相同（形状 + 值都要一样）");

    printf("  手工树 vs 递归树      ：");
    printf(tree_is_same(root, built) ? "相同 ✔\n" : "不同 ✘\n");

    /* 只把 4 改成 9，形状一模一样 —— 值不同也算不同 */
    const int marks_diff[] = {
        1,
        2, 9, MARK_EMPTY, MARK_EMPTY,   /* 这里原来是 4，现在改成 9 */
           5, MARK_EMPTY, MARK_EMPTY,
        3,
           MARK_EMPTY,
           6, MARK_EMPTY, MARK_EMPTY,
    };
    int marks_diff_n = (int)(sizeof(marks_diff) / sizeof(marks_diff[0]));
    Node *other = build_tree_from_marks(marks_diff, marks_diff_n);
    printf("  手工树 vs 递归树(4→9) ：");
    printf(tree_is_same(root, other) ? "相同 ✔\n" : "不同 ✘（形状一样，但对应位置的值不同）\n");
    printf("  两棵空树 tree_is_same(NULL, NULL) = %d（约定为空树彼此相同）\n",
           tree_is_same(NULL, NULL));

    /* ============================================================
       第 10 幕：销毁整棵树
       ============================================================ */
    section("第 10 幕：销毁整棵树（后序释放：先删左右子树，再 free 自己）");

    int to_free = tree_count(root) + tree_count(built) + tree_count(other);
    printf("  待释放节点数：%d（手工树）+ %d（递归树）+ %d（改值树）= %d 个，\n",
           tree_count(root), tree_count(built), tree_count(other), to_free);
    printf("  malloc 了多少次就得 free 多少次，少一次就是内存泄漏。\n");
    free_tree(root);
    root = NULL;                       /* 关键习惯：free 完立刻置 NULL，防悬空指针 */
    free_tree(other);
    other = NULL;
    free_tree(built);
    built = NULL;
    printf("  已释放完毕，root/built/other 全部置 NULL。\n");
    printf("  为什么不能先 free 父亲再删孩子？\n");
    printf("    孩子的地址写在父亲身体里 —— 父亲都还给系统了，\n");
    printf("    再读 root->left 就是 use-after-free，ASan 会当场报警。\n");

    /* ============================================================
       收尾
       ============================================================ */
    printf("\n=== 本节结束 ===\n");
    printf("记住四句话：\n");
    printf("  1) 树是递归定义的，所以处理树就是「处理自己 + 递归左右子树」，出口永远是 NULL。\n");
    printf("  2) 前/中/后 = 「访问自己」排在两个孩子的前/中/后，左右顺序永远不换。\n");
    printf("  3) 层序要队列（先进先出才是一层一层），递归的实质就是系统在帮你用栈。\n");
    printf("  4) 销毁必须后序：先删孩子再删自己，否则就是读已归还的内存。\n");
    return 0;
}
