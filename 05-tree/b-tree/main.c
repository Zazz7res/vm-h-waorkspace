#include <stdio.h>
#include <stdlib.h>
#include "btree.h"

/*
 * ===================================================================
 * main.c —— B 树分幕演示
 * ===================================================================
 * 六幕，每幕只证明一件事：
 *   第 1 幕：节点塞满 → 分裂 → 中键上推 → 根分裂 → 树长高（核心中的核心）
 *   第 2 幕：连续插入 12 个键，三种视图看清整棵树 + 中序有序性体检
 *   第 3 幕：查找，把"读了几次页"数出来（这个数字就是 B 树存在的全部理由）
 *   第 4 幕：删除 —— 借键、合并、连锁合并、树变矮、删不存在的键
 *   第 5 幕：把 README 里那张"100 万键，B 树 vs 红黑树"的账现场算一遍
 *   第 6 幕：销毁，谁 malloc 谁 free
 *
 * 编译运行（在项目根目录）：
 *   make one DIR=05-tree/b-tree
 */

/* 小工具：打印分幕标题，和 00-c-foundation/pointer-basics 里一个套路 */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/* 小工具：插入并把返回值说人话讲出来（返回值不解释给读者看，就等于没写文档）*/
static void insert_and_report(BTreeNode **root, int key)
{
    int r = btree_insert(root, key);
    if (r == 0) {
        printf("    结果：没插（键重复）\n");
    } else if (r < 0) {
        printf("    结果：失败（内存不够）\n");
    } else {
        printf("    结果：插好了。树高 %d 层，共 %d 个键\n",
               btree_height(*root), btree_count(*root));
    }
    btree_print_inorder(*root);
}

/* 小工具：删除并报告 */
static void remove_and_report(BTreeNode **root, int key)
{
    int r = btree_remove(root, key);
    printf("    结果：%s。树高 %d 层，共 %d 个键\n",
           r == 1 ? "删掉了" : (r == 0 ? "没找到，树保持原样" : "内部错误"),
           btree_height(*root), btree_count(*root));
    btree_print_inorder(*root);
    btree_print_levels(*root);
}

int main(void)
{
    /* ============================================================
       第 1 幕：分裂，从头看到尾
       ============================================================ */
    section("第 1 幕 · 插 4 个键，看尽『塞满 → 分裂 → 中键上推 → 树长高』");
    printf("  本节阶 m = %d：一个节点最多 %d 个键、最多 %d 个孩子，最少 %d 个键\n\n",
           BTREE_ORDER, BTREE_MAX_KEYS, BTREE_MAX_CHILDREN, BTREE_MIN_KEYS);

    BTreeNode *root = btree_new();
    printf("  刚建好的空树（空树也是有一个节点的！）：\n");
    btree_print_levels(root);

    printf("\n  --- 插 10：根是叶子，直接塞 ---\n");
    insert_and_report(&root, 10);
    printf("\n  --- 插 20：根有 2 个键了，刚好填满 ---\n");
    insert_and_report(&root, 20);
    printf("\n  --- 插 30：根要塞第 3 个键，超载 → 分裂，中键上推 → 没父亲 → 造新根 ---\n");
    insert_and_report(&root, 30);
    printf("\n  --- 插 40：往下钻到叶子，叶子分裂，中键上推给现成的父亲 ---\n");
    insert_and_report(&root, 40);

    printf("\n  现在这棵树的形状（空隙号就是 child[] 的下标）：\n");
    btree_print_tree(root);

    /* ============================================================
       第 2 幕：连插一批，三种视图 + 有序性体检
       ============================================================ */
    section("第 2 幕 · 再插 1~9、11、12，然后三种视图各看一遍");

    int batch[] = {5, 1, 9, 7, 3, 11, 12, 2, 6, 8, 4};
    for (size_t i = 0; i < sizeof batch / sizeof batch[0]; i++) {
        printf("\n  --- 插 %d ---\n", batch[i]);
        insert_and_report(&root, batch[i]);
    }

    printf("\n  插一个已经存在的键 10，看看重复键怎么处理：\n");
    insert_and_report(&root, 10);

    printf("\n  整棵树 —— 按层视图（一个方框 = 一页磁盘）：\n");
    btree_print_levels(root);
    printf("\n  整棵树 —— 树形视图（能看清每个节点挂在父亲哪道空隙上）：\n");
    btree_print_tree(root);
    printf("\n  最后做一次有序性体检：\n");
    btree_print_inorder(root);
    printf("  （上面必须是 1 2 3 ... 12 严格递增，只要有一个数顺序错了，\n");
    printf("    就说明某次分裂/借键把指针搬错了一格）\n");

    /* ============================================================
       第 3 幕：查找 —— 数字就是主角
       ============================================================ */
    section("第 3 幕 · 查找：真正花的钱是『读了几次页』");

    int probe = 0;
    printf("\n  查 11（在树里）：\n");
    int hit = btree_search(root, 11, &probe);
    printf("    => %d：%s，一共读了 %d 次页（树高 %d 层，最多也就 %d 次）\n",
           11, hit ? "找到" : "没找到", probe, btree_height(root), btree_height(root));

    printf("\n  查 99（不在树里）：\n");
    hit = btree_search(root, 99, &probe);
    printf("    => %d：%s，同样只读了 %d 次页就确定它不存在\n",
           99, hit ? "找到" : "没找到", probe);

    printf("\n  ⚠ 划重点：B 树查『没有的东西』和查『有的东西』一样快（都 ≤ 树高 次），\n");
    printf("     因为所有叶子都在同一层，不存在『往下多钻几层才发现没有』。\n");

    /* ============================================================
       第 4 幕：删除 —— 借键、合并、树变矮
       ============================================================ */
    section("第 4 幕 · 删除：先借，借不到就合并，根被掏空就矮一层");

    printf("  当前树：\n");
    btree_print_levels(root);

    printf("\n  --- 删 1：叶子直接少一个键；它的兄弟有得借 → 看借键 ---\n");
    remove_and_report(&root, 1);

    printf("\n  --- 删 20：命中【内部节点】的键，不能真删，要用前驱顶替 ---\n");
    remove_and_report(&root, 20);

    printf("\n  --- 删 666：树里根本没有这个键 ---\n");
    remove_and_report(&root, 666);

    /* 造一棵"一删就连锁合并"的最小树：7 个键刚好把三层填满（每层 1/3/9 个槽位用掉 7 个）。
     * 为什么不用上面那棵 15 键的树？因为键越多、删除时能借到的兄弟就越多，
     * 合并反而不容易发生 —— 想看清合并，就得把树搭成"每个兄弟都只有 1 个键"的紧巴巴状态。 */
    printf("\n  重造一棵 7 个键的紧巴巴树，专门演示『合并 → 连锁合并 → 树矮一层 → 删空』：\n");
    btree_free(root);
    root = btree_new();
    int seq[] = {10, 20, 30, 40, 50, 60, 70};
    for (size_t i = 0; i < sizeof seq / sizeof seq[0]; i++) {
        btree_insert(&root, seq[i]);
    }
    printf("\n  插入完毕，这棵树每个内部节点都只有 1 个键、每个叶子都只有 1 个键：\n");
    btree_print_levels(root);
    btree_print_tree(root);

    printf("\n  --- 删 10：三层一起塌（合并 → 惊动爷爷 → 再合并 → 根被掏空）---\n");
    remove_and_report(&root, 10);

    printf("\n  --- 删 70：最右边同样走合并，但这次父亲还剩 1 个键，不用往上塌 ---\n");
    remove_and_report(&root, 70);

    printf("\n  --- 剩下 5 个键挨个删光，看根怎么退化成叶子、树怎么变空 ---\n");
    int to_kill[] = {50, 30, 40, 20, 60};
    for (size_t i = 0; i < sizeof to_kill / sizeof to_kill[0]; i++) {
        printf("\n  删 %d：\n", to_kill[i]);
        remove_and_report(&root, to_kill[i]);
    }

    printf("\n  --- 空树里再删一个 1 ---\n");
    remove_and_report(&root, 1);

    /* 删空之后重新插几个键，顺便证明"空树还能继续用"（根那个空叶子没被 free 掉）*/
    printf("\n  --- 空树继续插 5、6、7，证明它没有被删坏 ---\n");
    int revive[] = {5, 6, 7};
    for (size_t i = 0; i < sizeof revive / sizeof revive[0]; i++) {
        insert_and_report(&root, revive[i]);
    }

    /* ============================================================
       第 5 幕：把「为什么数据库用 B 树」这笔账现场算出来
       ============================================================ */
    section("第 5 幕 · 算账：100 万个键，B 树几层、红黑树几层、各读几次盘");

    printf("  约定：磁盘一次必须读整整一页 = 4096 字节，读 1 字节和读 4096 字节一样贵。\n\n");

    /* B 树：节点大小 ≈ (m-1) 个键×4 字节 + m 个指针×8 字节 + 16 字节头信息 */
    printf("  ① B 树一个页能塞几个键？（4KB 页）\n");
    printf("     节点字节 = (m-1)×4 + m×8 + 16 = 12m + 12 ≤ 4096  →  m ≤ %d\n", (4096 - 12) / 12);
    int m_real = 340;                 /* 取个稳妥的整数值往下算 */
    printf("     取 m = %d（真实数据库还会考虑键长度、记录指针，量级就是这样）\n", m_real);

    /* 一棵高 h 层、阶 m 的 B 树最多能装多少键：叶子最多 m^(h-1) 个，每个叶子最多 m-1 个键 */
    long n_target = 1000000;
    printf("\n  ② m=%d 时装下 %ld 个键要几层？（每层最多键数 = (m-1)×m^(h-1)）\n",
           m_real, n_target);
    long cap = 1;
    int  h = 0;
    while (cap < n_target) {
        h++;
        if (h == 1) {
            cap = (long)(m_real - 1);
        } else {
            cap *= m_real;            /* 每多一层，容量乘以 m：这就是"扇出"的威力 */
        }
        printf("     第 %d 层最多装 %ld 个键%s\n", h, cap, cap >= n_target ? "  ← 够了！" : "");
    }
    printf("     => 树高 %d 层，一次查找最多读 %d 个页 = %d × 4KB = %d KB\n", h, h, h, h * 4);

    printf("\n  ③ 本节 demo 的 m=3 呢？（对比就知道「阶」有多重要）\n");
    long cap3 = 1;
    int  h3 = 0;
    while (cap3 < n_target && h3 < 20) {
        h3++;
        if (h3 == 1) {
            cap3 = (long)(BTREE_MAX_KEYS);
        } else {
            cap3 *= BTREE_ORDER;
        }
    }
    printf("     m=3：一个节点最多 %d 个键，装下 %ld 个键要 %d 层"
           "（对照 demo：现在树里 %d 个键就已经 %d 层了）\n",
           BTREE_MAX_KEYS, n_target, h3, btree_count(root), btree_height(root));
    printf("     结论：阶越小 → 树越高 → 读盘越多次。教学用 m=3 是为了看清分裂，\n");
    printf("           工程上 m 是由『一页装几个键』定出来的几百到几千。\n");

    printf("\n  ④ 红黑树（BST 家族）同样 100 万个键：\n");
    printf("     节点 = 键 4B + 左右父 3×8B + 颜色 1B + 对齐 ≈ 32B\n");
    printf("     树高 ≈ log2(1000000) = 20 层（最坏 2×log2(n) = 40 层）\n");
    printf("     一次查找 = 从上到下 = 20 次随机 IO，虽然 32B 塞不满一页，\n");
    printf("     但每次 IO 都得付一遍寻道/映射的钱 → 20 × 4KB = 80KB 读进来，只用 640B。\n");

    printf("\n  ┌────────────┬──────────────┬────────────┬──────────────┐\n");
    printf("  │            │ 节点多大     │ 树多高     │ 一次查找读几次页│\n");
    printf("  ├────────────┼──────────────┼────────────┼──────────────┤\n");
    printf("  │ 红黑树      │ ~32 字节      │ ~20 层     │ ~20 次随机 IO  │\n");
    printf("  │ B 树 m=%d   │ 4096 字节(一页)│ %d 层      │ %d 次 IO        │\n", m_real, h, h);
    printf("  │ B 树 m=3    │ 教学用的迷你版│ %d 层(1e6) │ %d 次          │\n", h3, h3);
    printf("  └────────────┴──────────────┴────────────┴──────────────┘\n");
    printf("  （内存里的查找红黑树更快、更省；一旦数据装不进内存，B 树家族完胜。）\n");

    /* ============================================================
       第 6 幕：收摊
       ============================================================ */
    section("第 6 幕 · 销毁：谁 malloc 谁 free，自底向上");

    printf("  树里还剩 %d 个键，%d 层 → 开始释放\n", btree_count(root), btree_height(root));
    btree_free(root);
    root = NULL;                        /* 铁律：free 完立刻置 NULL，误用会当场崩而不是潜伏 */
    printf("  已 free 并把 root 置 NULL。程序到这里如果没报 leak，说明每个节点都还干净了。\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住四句话：\n");
    printf("  1) B 树 = 把二叉搜索树的『一个节点』变宽成一页，键数上限 m-1、孩子数上限 m。\n");
    printf("  2) 插入的核心：塞满就劈三块（左留、右新建、中键上推），没人接就造新根，树长高。\n");
    printf("  3) 删除的核心：删内部键先用前驱顶替；孩子欠键先借后合并；根空了树矮一层。\n");
    printf("  4) 它存在的理由是磁盘：一次读一页，所以要把一页装满键，把树压到 3 层。\n");
    return 0;
}
