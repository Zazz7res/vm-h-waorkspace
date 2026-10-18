#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "avl.h"
#include "mini_bst.h"

/*
 * ===================================================================
 * AVL 演示：一幕一个主题，每一步都把树打出来给人看
 * ===================================================================
 * 第 1 幕  退化对比：同样 1..8，朴素 BST 变成链表，AVL 还是矮胖子
 * 第 2 幕  四种旋转的手术台：LL / RR / LR / RL 各搭一个失衡形状，看前后
 * 第 3 幕  插入自动修复：四段数据分别触发四种情况（打开旋转现场打印）
 * 第 4 幕  中序遍历 + 查找：证明旋转没把顺序搞坏
 * 第 5 幕  删除：三种情况各删一次，删完仍然平衡
 * 第 6 幕  批量灌 32 个数：验证高度确实贴着 log n
 * 第 7 幕  销毁整棵树
 * ===================================================================
 */

static void section(const char *title)
{
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n");
}

/* 打一行树 + 附带"高度 / 节点数 / 是否仍然合法"的体检报告 */
static void report(const AVLNode *root)
{
    avl_print_tree(root);
    printf("  体检：节点数=%d  树高=%d  合法性=%s\n",
           avl_count(root), avl_height(root),
           avl_is_valid(root) ? "通过（所有 |bf|<=1 且 height 都对）" : "★有问题★");
}

/*
 * 手工搭一棵"故意失衡"的三节点树，专门给旋转当标本。
 * 三个值按 BST 规矩挂：root_v 是根，child_v 是它的孩子，gc_v 是孙子。
 * first_right / second_right：0 = 走左边，1 = 走右边。
 * 关键点：搭完只更新高度，【不做任何旋转】——于是 bf 就停在 ±2，
 * 可以清清楚楚看到"塌了"的样子，然后再手动叫旋转上场。
 */
static AVLNode *build_shape(int root_v, int child_v, int gc_v,
                            int first_right, int second_right)
{
    AVLNode *gc = avl_new(gc_v);
    AVLNode *child = avl_new(child_v);
    AVLNode *root = avl_new(root_v);
    if (gc == NULL || child == NULL || root == NULL) {
        /* 内存没申请到就收拾干净走人，别留下半棵树 */
        free(gc);
        free(child);
        free(root);
        return NULL;
    }

    if (first_right) {
        root->right = child;
    } else {
        root->left = child;
    }
    if (second_right) {
        child->right = gc;
    } else {
        child->left = gc;
    }

    avl_update_height(child);   /* 先下后上，和真实插入的回溯顺序一致 */
    avl_update_height(root);
    return root;
}

/* 旋转标本演示：kind 0=LL右旋 1=RR左旋 2=LR双旋 3=RL双旋 */
static void rotate_demo(const char *title, const char *explain,
                        int root_v, int child_v, int gc_v,
                        int first_right, int second_right, int kind)
{
    printf("\n--- %s ---\n%s\n", title, explain);

    AVLNode *tree = build_shape(root_v, child_v, gc_v, first_right, second_right);
    if (tree == NULL) {
        printf("  搭不起来（内存不够）\n");
        return;
    }

    printf("  旋转前（bf 已经是 ±2，就是这里塌了）：\n");
    avl_print_tree(tree);

    AVLNode *fixed;
    switch (kind) {
        case 0:  fixed = avl_rotate_right(tree); break;   /* LL */
        case 1:  fixed = avl_rotate_left(tree);  break;   /* RR */
        case 2:  fixed = avl_rotate_lr(tree);    break;   /* LR */
        default: fixed = avl_rotate_rl(tree);    break;   /* RL */
    }

    printf("  旋转后（子树新根换成 %d）：\n", fixed->value);
    avl_print_tree(fixed);
    printf("  校验：%s\n", avl_is_valid(fixed) ? "平衡恢复，|bf|<=1" : "★仍有问题★");

    /* 注意：旋转后原来的 tree 变量已经不再是根了，只销毁返回的新根即可，
     * 三个节点都还在这棵子树里，一次销毁全部释放干净。 */
    avl_destroy(fixed);
}

/* 往 AVL 里插一个值，并当场把树打出来 */
static void insert_and_show(AVLNode **root, int value)
{
    AVLNode *old_root = *root;                  /* 先记住旧根，才能对比"有没有换人" */
    printf("  插入 %d\n", value);
    AVLNode *new_root = avl_insert(*root, value);   /* 返回值必须接回去！ */
    *root = new_root;

    if (old_root == NULL) {
        printf("  结果：这是第一个节点，它自己当根\n");
    } else if (new_root != old_root) {
        printf("  结果：子树换了根，%d → %d\n", old_root->value, new_root->value);
    } else {
        printf("  结果：根还是 %d（没旋转，或旋转发生在更下面）\n", new_root->value);
    }
    report(*root);
}

int main(void)
{
    /* ================================================================
       第 1 幕：AVL 存在的全部理由 —— 和朴素 BST 并排看
       ================================================================ */
    section("第 1 幕 · 同一批顺序数据 1..8：BST 变链表 vs AVL 矮胖子");

    MiniBSTNode *plain = NULL;

    for (int v = 1; v <= 8; v++) {
        plain = mini_bst_insert(plain, v);        /* 插完什么都不检查 */
    }

    printf("\n  【朴素 BST：不检查、不旋转】\n");
    mini_bst_print_tree(plain);
    printf("  节点数=%d  高度=%d（每次现算，递归 O(n)）\n",
           mini_bst_count(plain), mini_bst_height(plain));
    printf("  找最大值 8 需要比较 %d 次 → 这就是 O(n)，跟遍历链表一样\n",
           mini_bst_compare_count(plain, 8));

    printf("\n  【AVL：每次插入后自动修正，喂的是同一批 1..8】\n");
    AVLNode *balanced = NULL;
    for (int v = 1; v <= 8; v++) {
        balanced = avl_insert(balanced, v);       /* 插完检查 bf，塌了就转 */
    }
    avl_print_tree(balanced);
    printf("  节点数=%d  高度=%d（直接读根节点字段，O(1)）\n",
           avl_count(balanced), balanced->height);
    printf("  合法性：%s\n", avl_is_valid(balanced) ? "通过" : "★有问题★");

    printf("\n  结论：8 个节点，BST 高 8（一条链），AVL 高 4（每层塞得满满的）。\n");
    printf("        节点越多差距越夸张：100 万个顺序数据，BST 要走 100 万步，AVL 只要 20 步。\n");
    printf("        这就是 AVL 存在的全部理由：花 4 字节存高度，换回 O(log n) 的保证。\n");

    avl_destroy(balanced);
    balanced = NULL;
    mini_bst_destroy(plain);
    plain = NULL;

    /* ================================================================
       第 2 幕：四种旋转，各搭一个失衡标本单独开刀
       ================================================================ */
    section("第 2 幕 · 四种旋转的手术台（旋转前后树形对照）");

    rotate_demo("LL 型 → 一次右旋",
                "30 的左孩子 20 又有了左孩子 10，左边被压高 2 层。\n"
                "右旋：20 上位当根，30 降级成 20 的右孩子。",
                30, 20, 10, 0, 0, 0);

    rotate_demo("RR 型 → 一次左旋",
                "10 的右孩子 20 又有了右孩子 30，右边被压高 2 层。\n"
                "左旋：20 上位当根，10 降级成 20 的左孩子。（LL 的镜像）",
                10, 20, 30, 1, 1, 1);

    rotate_demo("LR 型 → 先左旋左孩子，再右旋",
                "30 的左孩子 10 却长了个【右】孩子 20，中间那个节点是「拐」着的。\n"
                "直接右旋转不动，先把左子树左旋成标准 LL，再右旋：20 上位当根。",
                30, 10, 20, 0, 1, 2);

    rotate_demo("RL 型 → 先右旋右孩子，再左旋",
                "10 的右孩子 30 却长了个【左】孩子 20。先右旋右子树成标准 RR，再左旋。\n"
                "结果 20 上位当根，10 和 30 变成它的左右孩子。（LR 的镜像）",
                10, 30, 20, 1, 0, 3);

    /* ================================================================
       第 3 幕：插入时自动触发这四种情况
       ================================================================ */
    section("第 3 幕 · 插入自动修复：四段数据分别命中 LL / RR / LR / RL");

    printf("\n  【把旋转现场打印打开】每次真的发生旋转会列出旋转前后的子树。\n");
    avl_set_verbose(1);

    struct { const char *name; int data[3]; } cases[] = {
        { "LL 型：依次插入 30 → 20 → 10", { 30, 20, 10 } },
        { "RR 型：依次插入 10 → 20 → 30", { 10, 20, 30 } },
        { "LR 型：依次插入 30 → 10 → 20", { 30, 10, 20 } },
        { "RL 型：依次插入 10 → 30 → 20", { 10, 30, 20 } },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        printf("\n  ===== %s =====\n", cases[i].name);
        AVLNode *t = NULL;
        for (int j = 0; j < 3; j++) {
            insert_and_show(&t, cases[i].data[j]);
        }
        printf("  最终根 = %d，高度 = %d\n", t->value, t->height);
        avl_destroy(t);
    }

    avl_set_verbose(0);

    /* ================================================================
       第 4 幕：中序遍历 + 查找
       ================================================================ */
    section("第 4 幕 · 中序遍历证明有序，查找证明真的 O(log n)");

    static const int seed_data[] = { 50, 30, 70, 20, 40, 60, 80, 10, 35, 45, 90 };
    AVLNode *tree = NULL;
    printf("\n  依次插入：");
    for (size_t i = 0; i < sizeof(seed_data) / sizeof(seed_data[0]); i++) {
        printf("%d ", seed_data[i]);
        tree = avl_insert(tree, seed_data[i]);
    }
    printf("\n\n  建出来的树：\n");
    report(tree);

    printf("\n  中序遍历（左→根→右）：");
    avl_print_inorder(tree);
    printf("\n  ↑ 严格升序 = 旋转只改形状，从没破坏\"左 < 根 < 右\"\n");

    printf("\n  重复值再插一次 50：");
    tree = avl_insert(tree, 50);
    printf("节点数=%d（值已存在 → 忽略，AVL 里不放重复元素）\n", avl_count(tree));

    printf("\n  查找结果：\n");
    const int probes[] = { 45, 90, 99 };
    for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
        const AVLNode *hit = avl_search(tree, probes[i]);
        if (hit != NULL) {
            printf("    %2d 找到了，节点高度 h=%d, bf=%d\n", hit->value, hit->height,
                   avl_balance(hit));
        } else {
            printf("    %2d 不在树里（返回 NULL）\n", probes[i]);
        }
    }

    /* ================================================================
       第 5 幕：删除三种情况
       ================================================================ */
    section("第 5 幕 · 删除：叶子 / 单孩子 / 双孩子，删完照样平衡");

    printf("\n  当前这棵树（接着第 4 幕，节点数 %d）：\n", avl_count(tree));
    report(tree);
    printf("\n  删除同样把旋转现场打开，看看删除是怎么把祖先压塌的。\n");
    avl_set_verbose(1);

    printf("\n  (a) 删【叶子节点】35：没有孩子，free 掉，爸爸 40 的左腿变 NULL\n");
    printf("      这是最简单的情况：树最多矮一层，祖先的 bf 还在 ±1 内\n");
    tree = avl_remove(tree, 35);
    report(tree);

    printf("\n  (b) 删【只有一个右孩子】的节点 80：它只有右孩子 90\n");
    printf("      80 的整棵子树（就是 90）都比 80 大，直接提上来顶替 80 的位置\n");
    printf("      70 的右腿从 80(h=2) 变成 90(h=1)，bf 从 -1 变回 0，反而更平衡了\n");
    tree = avl_remove(tree, 80);
    report(tree);

    printf("\n  (c) 删【有两个孩子】的根节点 50：不能真删自己（两棵子树没地方放）\n");
    printf("      去右子树找最小节点 = 后继 = 60，把它的值抄进我这边，\n");
    printf("      再去右子树里删掉那个值为 60 的节点（它正好是叶子，落到情况 a）\n");
    printf("      所以根的【值】会从 50 变成 60 —— 节点没换，换的是里面装的数\n");
    tree = avl_remove(tree, 50);
    report(tree);

    printf("\n  (d) 再删根节点（现在值已经是 60，依然有两个孩子）：后继是 70，而 70 只有右孩子 90\n");
    printf("      抄完值再去删那个 70 → 落到情况 (b)。右支高度从 2 掉到 1，\n");
    printf("      根的 bf 变成 +2 → 这一次必须旋转才救得回来\n");
    printf("      ★ 插入最多修一处，删除要把一路上每个祖先都检查一遍 ★\n");
    tree = avl_remove(tree, 60);
    report(tree);

    printf("\n  中序再走一遍，看「抄值 + 旋转」有没有把顺序搞乱：");
    avl_print_inorder(tree);
    printf("\n");

    printf("\n  (e) 删一个不存在的值 999：递归走到 NULL 就原路返回，绝不误删\n");
    tree = avl_remove(tree, 999);
    report(tree);

    avl_set_verbose(0);

    printf("\n  (f) 把剩下的全删光，最后应该得到空树：\n");
    /* ⚠ sizeof(seed_data) 是【字节数】44，不是元素个数 11！
     *   这里必须除以 sizeof(seed_data[0])，否则越界读 —— ASan 会当场抓到。 */
    int how_many = (int)(sizeof(seed_data) / sizeof(seed_data[0]));
    for (int i = 0; i < how_many; i++) {
        tree = avl_remove(tree, seed_data[i]);
    }
    report(tree);
    printf("  节点数=%d → 树彻底空了\n", avl_count(tree));

    /* ================================================================
       第 6 幕：批量灌数据，看高度是不是真的贴着 log n
       ================================================================ */
    section("第 6 幕 · 32 个乱序数据：高度到底长成什么样");

    int shuffled[32];
    for (int i = 0; i < 32; i++) {
        shuffled[i] = i + 1;                     /* 先放 1..32 */
    }
    srand(20260103u);                            /* 固定种子，输出可复现 */
    for (int i = 31; i > 0; i--) {               /* Fisher-Yates 洗牌 */
        int j = rand() % (i + 1);
        int tmp = shuffled[i];
        shuffled[i] = shuffled[j];
        shuffled[j] = tmp;
    }

    AVLNode *big = NULL;
    printf("\n  乱序插入序列：");
    for (int i = 0; i < 32; i++) {
        printf("%d ", shuffled[i]);
        big = avl_insert(big, shuffled[i]);
    }
    printf("\n\n  建出来的树：\n");
    avl_print_tree(big);

    printf("\n  节点数 = %d\n", avl_count(big));
    printf("  实际高度 = %d\n", big->height);
    printf("  理论上限 1.44 * log2(n+2) ≈ 7.7 → AVL 保证不会超过它\n");
    printf("  完全平衡二叉树的高度 log2(32) = 5，AVL 允许比它「胖」一点，但绝不变链\n");
    printf("  合法性：%s\n", avl_is_valid(big) ? "通过" : "★有问题★");
    printf("  找最大元素 32 只要往下走 %d 步（同样数据喂朴素 BST 要走 32 步）\n",
           big->height);

    printf("\n  最坏情况实测：把 1..32 顺序喂给 AVL（这就是能让 BST 变链的那批数据）\n");
    AVLNode *worst = NULL;
    for (int v = 1; v <= 32; v++) {
        worst = avl_insert(worst, v);
    }
    printf("  顺序数据的 AVL 高度 = %d，合法性 = %s\n", worst->height,
           avl_is_valid(worst) ? "通过" : "★有问题★");
    printf("  同样数据喂朴素 BST：高度 = ");
    MiniBSTNode *worst_plain = NULL;
    for (int v = 1; v <= 32; v++) {
        worst_plain = mini_bst_insert(worst_plain, v);
    }
    printf("%d（彻底一条链）\n", mini_bst_height(worst_plain));
    printf("  把顺序数据树打出来给你看形状：\n");
    avl_print_tree(worst);

    /* ================================================================
       第 7 幕：销毁
       ================================================================ */
    section("第 7 幕 · 销毁：谁 malloc 谁 free，一个都不能漏");

    avl_destroy(tree);      /* tree 在这之前已经是空树（第 5 幕删光了） */
    avl_destroy(big);
    avl_destroy(worst);
    mini_bst_destroy(worst_plain);
    tree = NULL;
    big = NULL;
    worst = NULL;
    worst_plain = NULL;
    printf("  四棵树全部销毁，指针都置成 NULL（make asan 会确认 0 泄漏）\n");

    printf("\n=== AVL 演示结束 ===\n");
    printf("记住四句话：\n");
    printf("  1) bf = 左高 - 右高，只允许 -1/0/+1，出现 ±2 就得旋转。\n");
    printf("  2) 四种情况四种转法：LL 右旋、RR 左旋、LR 左旋子树再右旋、RL 右旋子树再左旋。\n");
    printf("  3) 旋转函数返回的是【新根】，永远写 root = avl_rotate_xxx(root)。\n");
    printf("  4) 高度存在节点里，更新只在递归回头那一行，先孩子后自己。\n");
    return 0;
}
