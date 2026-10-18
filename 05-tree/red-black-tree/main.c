#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "rbtree.h"

/*
 * ===================================================================
 * 红黑树演示：八幕，每一幕只做一件事
 * ===================================================================
 * 第 1 幕  先认识那个"共享哨兵"：空树长什么样、叶子是不是同一个节点
 * 第 2 幕  左旋 / 右旋单独开刀：形状怎么变、parent 怎么改、为什么必须配变色
 * 第 3 幕  插入修复的三种情况：情况1 叔叔红变色上移 / 情况2 内侧掰外侧 / 情况3 终杀
 * 第 4 幕  依次插入 1..7，每插一个就 rbtree_check() 验五条性质
 * 第 5 幕  中序遍历 + 查找：颜色怎么折腾，顺序不会坏
 * 第 6 幕  删除：红节点直接删 / 黑叶子删要补黑 / 双孩子摘后继
 * 第 7 幕  压力测试：随机增删 300 次，每步都校验（这是本文件的"单元测试"）
 * 第 8 幕  销毁
 * ===================================================================
 */

static void section(const char *title)
{
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n");
}

/* 第 4 幕用：插一个值 → 打印树 → 立刻校验五条性质 */
static void insert_check(RBTree *t, int value)
{
    printf("\n  ---- 插入 %d ----\n", value);
    rbtree_insert(t, value);
    rbtree_print_tree(t);
    rbtree_check(t);
}

int main(void)
{
    /* ================================================================
       第 1 幕：认识哨兵
       ================================================================ */
    section("第 1 幕 · 共享哨兵 nil：为什么红黑树不到处判 NULL");

    RBTree t;
    rbtree_init(&t);
    printf("  刚 init 的空树：");
    rbtree_print_tree(&t);
    printf("  t.root 和 t.nil 是同一个地址 = %p ← 空树的\"根\"就是哨兵本身\n",
           (void *)t.root);
    printf("  哨兵的颜色 = %s（性质 3 规定所有叶子是黑的，就靠这一个节点保证）\n",
           rb_color_name_of(t.nil->color));

    rbtree_insert(&t, 40);
    rbtree_insert(&t, 20);
    rbtree_insert(&t, 60);
    printf("\n  插了 40 / 20 / 60 之后的树：\n");
    rbtree_print_tree(&t);

    const RBNode *n20 = rbtree_search(&t, 20);
    const RBNode *n60 = rbtree_search(&t, 60);
    printf("\n  看叶子：节点 20 的左腿 = %p，右腿 = %p\n",
           (void *)n20->left, (void *)n20->right);
    printf("        节点 60 的左腿 = %p，右腿 = %p\n",
           (void *)n60->left, (void *)n60->right);
    printf("        哨兵地址        = %p  ← 四个空位是【同一个】节点\n", (void *)t.nil);
    printf("  ★ 这就是用哨兵的理由：`uncle->color` 这种代码可以直接写，读出来就是\"黑\"。\n");
    printf("    不用哨兵的话，每读一个颜色都要先写 if (x != NULL)，判断数量直接翻倍，\n");
    printf("    而且删除时\"双黑往上冒泡\"会在根那里断掉。Linux 内核的 rb_tree 也这么干。\n");
    rbtree_check(&t);

    /* 这一幕的树用完就地销毁：谁 malloc 谁 free，别拖到最后（ASan 会盯着每一棵）。
     * 销毁之后 t.root / t.nil 都已经是 NULL，这一棵不能再用了。 */
    rbtree_destroy(&t);

    /* ================================================================
       第 2 幕：左旋 / 右旋
       ================================================================ */
    section("第 2 幕 · 左旋 / 右旋：形状怎么变，为什么必须和变色配对");

    RBTree r;
    rbtree_init(&r);
    rbtree_insert(&r, 10);
    rbtree_insert(&r, 5);
    rbtree_insert(&r, 20);
    rbtree_insert(&r, 15);
    rbtree_insert(&r, 25);
    printf("\n  旋转前的树（这是一棵合法红黑树）：\n");
    rbtree_print_tree(&r);
    rbtree_check(&r);

    RBNode *root10 = r.root;             /* 现在根是 10 */
    printf("\n  【对根 10 做一次左旋】只改 6 个指针，【一个颜色都不动】：\n");
    printf("     10 的右腿 ← 15（原来挂在 20 的左腿，现在改嫁）\n");
    printf("     15 的 parent ← 10\n");
    printf("     20 接班当根（parent ← 哨兵），20 的左腿 ← 10，10 的 parent ← 20\n");
    rbtree_rotate_left(&r, root10);
    rbtree_print_tree(&r);
    printf("  然后校验：\n");
    int broken = rbtree_check(&r);
    printf("  ★ 看到了吗：随便转一次，性质 5 就破了（黑高左 2 右 0 不相等）。\n");
    printf("    旋转本身【不保平衡】，它只是搬形状；黑高靠的是配套的【变色】。\n");
    printf("    所以第 3 幕里每一次旋转都紧跟着一句\"谁染黑谁染红\"，两者成对出现。\n");
    if (broken == 0) {
        printf("  （咦，这次居然没过？说明形状恰好仍然合法，属于巧合）\n");
    }

    printf("\n  【再右旋回去】旋转是可逆的，转出去再转回来，树完好如初：\n");
    rbtree_rotate_right(&r, r.root);     /* 现在根是 20，右旋还原成 10 当根 */
    rbtree_print_tree(&r);
    rbtree_check(&r);
    rbtree_destroy(&r);

    /* ================================================================
       第 3 幕：插入修复的三种情况
       ================================================================ */
    section("第 3 幕 · 插入修复三种情况（左/右镜像都演示一遍）");

    printf("\n  规则：新节点一律【染红】挂上去（性质 5 不受影响，最坏只坏性质 4），\n");
    printf("       然后看\"叔叔\"是什么颜色，决定走哪条路：\n");
    printf("       情况1 叔叔红 → 只变色：爸黑叔黑爷红，把冲突【上移一层】，不旋转\n");
    printf("       情况2 叔叔黑 + 我在内侧 → 对爸爸转一次，把形状【掰成外侧】\n");
    printf("       情况3 叔叔黑 + 我在外侧 → 爸黑爷红 + 对爷爷转一次，【当场终结】\n");

    struct {
        const char *title;
        const char *pre;
        int data[4];
        int n;
    } cases[] = {
        { "情况 1：叔叔是红的 → 变色上移，零旋转",
          "先插 10 / 5 / 15（10 黑根，两个红孩子），再插 3：爸 5 红、叔 15 也红 → 走情况 1",
          { 10, 5, 15, 3 }, 4 },
        { "情况 3（外侧，非镜像）：爸左我也左 → 爸黑爷红 + 对爷爷右旋",
          "插 10 / 5 之后爸爸是黑的，没事；再插 3 → 爸 5 红、叔是黑哨兵、我在外侧 → 直接情况 3",
          { 10, 5, 3 }, 3 },
        { "情况 2 + 情况 3（镜像）：爸右我左 → 先右旋掰直，再左旋终结",
          "插 10 / 20，再插 15 → 爸 20 红、叔是黑哨兵、我在【内侧】→ 情况 2 掰成外侧，紧接情况 3",
          { 10, 20, 15 }, 3 },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        printf("\n  ===== %s =====\n", cases[i].title);
        printf("  %s\n", cases[i].pre);
        RBTree c;
        rbtree_init(&c);
        rbtree_set_verbose(&c, 1);
        for (int j = 0; j < cases[i].n; j++) {
            printf("\n  · 第 %d 次插入 %d：\n", j + 1, cases[i].data[j]);
            rbtree_insert(&c, cases[i].data[j]);
            rbtree_print_tree(&c);
        }
        printf("\n  最后校验：\n");
        rbtree_check(&c);
        rbtree_destroy(&c);
    }

    /* ================================================================
       第 4 幕：1..7 依次插入，每步校验
       ================================================================ */
    section("第 4 幕 · 顺序插入 1..7：每插一个就验一次五条性质");

    printf("\n  这批数据喂给朴素 BST 会变成 7 层链表；红黑树不会 —— 看下面的 bh 和树高。\n");
    RBTree s;
    rbtree_init(&s);
    for (int v = 1; v <= 7; v++) {
        insert_check(&s, v);
    }
    printf("\n  插完 7 个：树高 %d 层（链表会是 7 层），黑高 bh = 2\n", rbtree_height(&s));
    printf("  红黑树保证树高 <= 2·log2(n+1) = 2·log2(8) = 6，这里只用 %d 层\n",
           rbtree_height(&s));

    /* ================================================================
       第 5 幕：中序 + 查找
       ================================================================ */
    section("第 5 幕 · 中序遍历与查找：颜色怎么折腾，顺序都不会坏");

    printf("\n  中序遍历（左→根→右）：");
    rbtree_print_inorder(&s);
    printf("\n  ↑ 升序 = 旋转和变色只改形状与颜色，从没破坏\"左 < 根 < 右\"\n");

    printf("\n  查找：\n");
    const int probes[] = { 1, 4, 7, 99 };
    for (size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
        const RBNode *hit = rbtree_search(&s, probes[i]);
        if (hit != NULL) {
            printf("    %2d 命中：颜色=%s，爸爸=%d\n", hit->value,
                   rb_color_name_of(hit->color),
                   (hit->parent == s.nil) ? -1 : hit->parent->value);
        } else {
            printf("    %2d 不在树里（撞上哨兵 → 返回 NULL）\n", probes[i]);
        }
    }

    /* ================================================================
       第 6 幕：删除
       ================================================================ */
    section("第 6 幕 · 删除：红节点最便宜，黑节点要补账");

    /* 用一棵大一点的树，才看得见删除引发的各种情况 */
    RBTree d;
    rbtree_init(&d);
    rbtree_set_verbose(&d, 1);
    static const int build_data[] = { 50, 20, 70, 10, 30, 60, 80, 5, 15, 25, 35, 55, 65, 75, 90 };
    int n_build = (int)(sizeof(build_data) / sizeof(build_data[0]));
    printf("\n  先建一棵 15 个节点的树：");
    for (int i = 0; i < n_build; i++) {
        printf("%d ", build_data[i]);
        rbtree_insert(&d, build_data[i]);
    }
    printf("\n");
    rbtree_print_tree(&d);
    rbtree_check(&d);

    printf("\n  ---- (a) 删一个【红】节点 ----\n");
    printf("  红节点被拿走不改变任何路径的黑节点数 → 记账发现不用修，最便宜的情况\n");
    rbtree_remove(&d, 5);
    rbtree_print_tree(&d);
    rbtree_check(&d);

    printf("\n  ---- (b) 删一个【黑】节点，而且它只有一条右腿 ----\n");
    printf("  黑节点离场 → 它那条路少了一个黑，这笔亏空要有人认：\n");
    printf("    · 顶上来的是【红】孩子 → 把它染黑就补回来了，一步都不用冒泡（本次就是这种）\n");
    printf("    · 顶上来的是黑孩子/黑哨兵 → 它背上【双黑】，往上靠兄弟那棵子树\"借黑\"：\n");
    printf("      CASE 1 换一个黑兄弟 / CASE 2 兄弟染红、亏空上移 / CASE 3 掰形状 / CASE 4 一次补平\n");
    rbtree_remove(&d, 10);
    rbtree_print_tree(&d);
    rbtree_check(&d);

    printf("\n  ---- (c) 删一个【有两条腿】的节点 ----\n");
    printf("  摘的是后继（右子树最小），让它顶到 50 的位置并【继承 50 的颜色】，\n");
    printf("  所以\"50 这个位置\"的黑数没变；真正腾出原位置的是后继 55 —— 记账按 55 原来的颜色算\n");
    rbtree_remove(&d, 50);
    rbtree_print_tree(&d);
    rbtree_check(&d);

    printf("\n  ---- (d) 删一个不存在的值 ----\n");
    rbtree_remove(&d, 999);
    rbtree_check(&d);

    printf("\n  中序检查顺序还在：");
    rbtree_print_inorder(&d);
    printf("\n");

    printf("\n  ---- (e) 全部删光，每删一个都校验 ----\n");
    printf("  注意：50/20/70 这些值可能已经被\"后继顶位\"换掉了，所以这里会看到\"不在树里\"，\n");
    printf("  这正是删除该老实的地方 —— 值不存在就一个节点都不动。\n");
    for (int i = 0; i < n_build; i++) {
        rbtree_remove(&d, build_data[i]);
        printf("  对 %d 执行删除之后：节点 %d 个，树高 %d，", build_data[i],
               rbtree_count(&d), rbtree_height(&d));
        rbtree_check(&d);
    }
    rbtree_set_verbose(&d, 0);
    rbtree_destroy(&d);

    /* ================================================================
       第 7 幕：压力测试
       ================================================================ */
    section("第 7 幕 · 随机增删 300 次，每一步都跑 rbtree_check()");

    RBTree p;
    rbtree_init(&p);
    srand(20260103u);                   /* 固定种子：谁跑都是同一串数据，便于对答案 */

    int fail_count = 0;
    int ops = 0;
    int max_height = 0;
    int removed = 0;
    int inserted = 0;
    for (int round = 0; round < 300; round++) {
        int v = rand() % 100;            /* 值域 0..99，制造大量重复/删除交织 */
        if (rbtree_search(&p, v) != NULL) {
            if (rbtree_remove(&p, v) == 1) {
                removed++;
            }
        } else {
            rbtree_insert(&p, v);
            inserted++;
        }
        ops++;
        /* 每一步都校验，但用"安静版"：通过时一个字都不打，破了才打印是第几条/哪个节点。
         * 这样 300 步不会刷屏，而任何一次破环都绝对漏不掉。 */
        if (rbtree_check_quiet(&p) != 0) {
            printf("  >>> 第 %d 步（值 %d）之后校验失败，见上面的 ✗ 行\n", ops, v);
            fail_count++;
        }
        if (rbtree_height(&p) > max_height) {
            max_height = rbtree_height(&p);
        }
        /* 只在几个里程碑打完整报告，别让 300 棵树刷屏 */
        if (ops == 50 || ops == 150 || ops == 300) {
            printf("\n  ---- 第 %d 步的完整快照 ----\n", ops);
            rbtree_print_tree(&p);
            rbtree_check(&p);
        }
    }
    printf("\n  压力测试小结：\n");
    printf("    操作次数 = %d（成功插入 %d / 成功删除 %d）\n", ops, inserted, removed);
    printf("    校验失败次数 = %d %s\n", fail_count,
           fail_count == 0 ? "← 五条性质一次都没破" : "← 有问题，去看上面打印");
    printf("    全程最大树高 = %d\n", max_height);
    /* 2*log2(n+1) 上限：不用 math.h，手算"2 的几次方 >= n+1"就够了 */
    int n_now = rbtree_count(&p);
    int k = 0;
    int pow2 = 1;                        /* pow2 = 2^k */
    while (pow2 < n_now + 1) {
        pow2 *= 2;
        k++;
    }
    printf("    现在 n=%d，红黑树理论上限 2*log2(n+1) = %d，实际树高 %d ← 没超\n",
           n_now, 2 * k, rbtree_height(&p));
    printf("    AVL 的上限是 1.44*log2(n) ≈ 更矮，但旋转次数比红黑多，这就是取舍\n");
    rbtree_destroy(&p);

    /* ================================================================
       第 8 幕：销毁
       ================================================================ */
    section("第 8 幕 · 销毁：真实节点逐个 free，唯一的哨兵最后 free 一次");

    rbtree_check(&s);                    /* 销毁前再确认一次这棵树还是好的 */
    rbtree_destroy(&s);
    printf("  本文件一共建过 6 棵树，现在全都销毁了：\n");
    printf("    t(第1幕 就地销毁) r(第2幕) c(第3幕，三种情况各一棵)\n");
    printf("    s(第8幕，就是刚销毁的这棵) d(第6幕) p(第7幕压力测试)\n");
    printf("  销毁顺序：先递归拆【真实节点】，最后 free【唯一的哨兵】—— 哨兵 free 两次就是 double free。\n");
    printf("  想确认 0 泄漏就跑 ASan（README 的编译运行小节里有整条命令）。\n");

    printf("\n=== AVL 还是红黑？三分钟决策表 ===\n");
    printf("  查远多于增删、想卡死最坏树高（只读大字典、内存索引）→ AVL：树高 <= 1.44·log2(n+2)，更矮\n");
    printf("  增删频繁、写入型负载（调度器、map/TreeMap、LRU）      → 红黑：插入 <=2 次、删除 <=3 次旋转\n");
    printf("  要\"有序 + 范围查询\"的标准库容器                        → 红黑（libstdc++ 的 map、Java TreeMap 都是它）\n");
    printf("  纯按 key 取值、完全不在乎顺序                          → 两个都别用，上哈希表 O(1)\n");
    printf("  数据在硬盘上、一页能塞几十个键                          → 两个都别用，上 B 树（05-tree/b-tree/）\n");
    printf("  不想手写旋转、能接受随机化                              → 跳表（02-linked-list/skip-list/）\n");

    printf("\n=== 红黑树演示结束 ===\n");
    printf("记住五句话：\n");
    printf("  1) 五条性质里真正\"干活\"的是性质 4 + 性质 5：最长路 <= 2 × 最短路 → 树高 <= 2log(n+1)。\n");
    printf("  2) 新节点染红：把破坏限制在性质 4（局部），别去碰性质 5（全局）。\n");
    printf("  3) 看叔叔决定怎么修：叔叔红就变色上移（旋转少），叔叔黑才旋转。\n");
    printf("  4) 哨兵把\"NULL 也是黑叶子\"这条约定变成了真实节点，判空少一半。\n");
    printf("  5) 删除的亏空记在顶替者头上叫\"双黑\"，靠兄弟借黑，最多冒泡 O(log n) 层。\n");
    return 0;
}
