#include <stdio.h>
#include "segtree.h"

static void section(const char *title)
{
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n");
}

/* 和树状数组那一节同款的极小确定性伪随机（LCG）。
 * 不用 rand()：它的序列由各 C 库决定，换平台就变，README 里的输出就没法复核。 */
static unsigned int g_seed = 20261003u;

static unsigned int next_u32(void)
{
    g_seed = g_seed * 1103515245u + 12345u;
    return g_seed >> 8;
}

int main(void)
{
    const int n = 8;
    const int data[8] = {5, 3, 8, 1, 4, 2, 7, 6};
    /* 手算好这些，后面全靠它验收：
     *   总和 = 5+3+8+1+4+2+7+6 = 36
     *   [1,4] = 17    [5,8] = 19    [1,2] = 8    [3,4] = 9
     *   [5,6] = 6     [7,8] = 13    [2,5] = 3+8+1+4 = 16                                     */

    SegTree tree;
    st_init(&tree, data, n);

    /* ============================================================
       第 1 幕：动机 —— 树状数组哪里干不动了
       ============================================================ */
    section("第 1 幕：BIT 擅长「单点改 + 前缀查」，一旦「改的也是区间」它就别扭了");

    printf("      需求：a[2..5] 每个数都 +3，之后再问 a[1..8] 的和。\n");
    printf("      · 用树状数组：只能 a[2]+=3、a[3]+=3、a[4]+=3、a[5]+=3 —— 4 次单点改（每次 O(log n)），\n");
    printf("        区间长度就是次数，区间一长等于没优化。硬要优化得上「差分树状数组」，两层套娃极难写对。\n");
    printf("      · 用线段树：找到 [%d,%d] 这类「整段被盖住」的节点，改它一格 + 记一笔欠账就完事。\n",
           3, 4);
    printf("      下面实测这一笔区间加在线段树里到底动了几个节点：\n");
    tree.trace = 1;
    st_range_add(&tree, 2, 5, 3);
    long long cells = tree.visits;
    tree.trace = 0;
    printf("      ⇒ 一共访问了 %lld 个节点（n=8）。拆开看这 11 个：\n", cells);
    printf("        · 3 个「整段被盖住」的直接改 sum（[2,2]、[3,4]、[5,5]），其中 [3,4] 还顺手记了 lazy\n");
    printf("        · 5 个祖先把孩子的值加起来重算（[1,2]、[1,4]、[5,6]、[5,8]、[1,8]）\n");
    printf("        · 3 个路过一看「不相交」立刻返回（[1,1]、[6,6]、[7,8]），它们只花了一次比较\n");
    printf("      朴素 for 循环要改 4 格，看着还少；但它是 O(区间长度)，n=10 万、区间长 5 万时\n");
    printf("      就是 5 万次写，而线段树永远只碰十几个节点 —— 差的是数量级。\n");
    printf("      再把这次修改撤回，从干净的树开始讲：\n");
    st_range_add(&tree, 2, 5, -3);
    printf("      撤回后 a[1..8] 的总和 = %d（应当等于手算的 36）\n", st_range_sum(&tree, 1, 8));

    /* ============================================================
       第 2 幕：建树 —— 区间是怎么被一段一段切开的
       ============================================================ */
    section("第 2 幕：建树。父亲 = 两个孩子相加，一眼就能核对");

    st_print_tree(&tree);
    printf("      手算核对：[1,4] = 5+3+8+1 = 17 ✔   [5,8] = 4+2+7+6 = 19 ✔   根 = 36 ✔\n");
    printf("      看两件事：\n");
    printf("        · 每一层的区间长度减半 ⇒ 树一共 log2(n)+1 层，高只有 4 层（n=8）\n");
    printf("        · 节点编号 1、2、3、4…15 是按「完全二叉树」排的：node 的孩子就是 2i 和 2i+1\n");

    /* ============================================================
       第 3 幕：为什么数组要开 4n 而不是 2n
       ============================================================ */
    section("第 3 幕：为什么开 4n 而不是 2n —— 实测最大编号");

    printf("       n    实际最大编号    2n    4n     结论\n");
    printf("      ------------------------------------------------\n");
    for (int k = 1; k <= 12; k++) {
        int need = st_max_node_index(k);
        printf("      %3d %12d %7d %6d   %s\n", k, need, 2 * k, 4 * k,
               need <= 2 * k ? "2n 够用" : "★2n 不够，必须更多");
    }
    printf("      ------------------------------------------------\n");
    printf("      为什么 2 的幂（1、2、4、8）刚好 2n 够用？因为那时树是**满的**，\n");
    printf("      最后一层正好从 n+1 排到 2n。n=3 时树不满，但编号照 2i/2i+1 往两边推，\n");
    printf("      最靠右那条分支每次乘 2 再加 1 ⇒ 编号窜得比元素数快得多。\n");
    printf("      看上面这张表：n=6 已经要 %d 格（比 2n=12 多 1 格），n=10 要 %d 格（2n 只给 20）——\n",
           st_max_node_index(6), st_max_node_index(10));
    printf("      只要有一次写到 2n+1 这个下标，就越界踩坏了别人的内存 —— ASAN 第一个抓它。\n");
    printf("      结论：**开 4n+5**。多花的是一点内存，省掉的是「数组越界」这种查三小时的神秘 bug。\n");
    printf("      那 4n 会不会也夸张了？把 n=1..2000 全扫一遍，看最大编号占 4n 的比例最高的那个：\n");
    int worst_n = 1, worst_need = st_max_node_index(1), worst_permille = 0;
    for (int k = 1; k <= 2000; k++) {
        int need = st_max_node_index(k);
        int permille = need * 1000 / (4 * k);          /* 用千分比避免打印小数 */
        if (permille > worst_permille) {
            worst_permille = permille;
            worst_n = k;
            worst_need = need;
        }
    }
    printf("        最凶的是 n=%d：最大编号 %d，占 4n=%d 的 %d.%d%% ⇒\n",
           worst_n, worst_need, 4 * worst_n, worst_permille / 10, worst_permille % 10);
    printf("        4n 是**几乎卡着上界**的安全值：连最凶的例子也只用到 95.4%%，没浪费多少；\n");
    printf("        而 2n 在 n=6、10、11、12… 上直接不够 —— 差的那几格就是越界写，会踩坏别人的内存。\n");

    /* ============================================================
       第 4 幕：区间查询 —— 一次查询只走 O(log n) 个节点
       ============================================================ */
    section("第 4 幕：区间查询全过程（看它怎么「整段取走、不往下钻」）");

    printf("      查 a[2..5]：\n");
    tree.trace = 1;
    int q25 = st_range_sum(&tree, 2, 5);
    long long v25 = tree.visits;
    tree.trace = 0;
    printf("      ⇒ 结果 %d，只访问了 %lld 个节点（手算 3+8+1+4 = 16 ✔）\n", q25, v25);

    printf("\n      查 a[1..4]：正好等于树上某个节点 ⇒ 一次就取走\n");
    tree.trace = 1;
    int q14 = st_range_sum(&tree, 1, 4);
    tree.trace = 0;
    printf("      ⇒ 结果 %d（手算 5+3+8+1 = 17 ✔）。这就是线段树快的原因：区间能整块拿就不拆\n", q14);

    /* ============================================================
       第 5 幕：单点更新
       ============================================================ */
    section("第 5 幕：单点更新 = 把「区间」缩成一个点，只有树高这条路要重算");

    printf("      a[3] += 10（原来 8，变 18）：\n");
    tree.trace = 1;
    st_point_add(&tree, 3, 10);
    tree.trace = 0;
    printf("      ⇒ 真正被改的只有 [3,3]→[3,4]→[1,4]→[1,8] 这一条链（4 个节点）；\n");
    printf("        本次递归一共走了 %lld 个节点，多出来的是「路过一看：不相交，立刻返回」的兄弟，\n",
           tree.visits);
    printf("        量级就是树高 + 1 ⇒ O(log n)，和 n 的大小几乎无关。\n");
    printf("      现在查 a[1..4] = %d（= 17+10 = 27 ✔），再撤回：\n", st_range_sum(&tree, 1, 4));
    st_point_add(&tree, 3, -10);

    /* ============================================================
       第 6 幕：懒标记 —— 全章最难，慢慢看
       ============================================================ */
    section("第 6 幕：懒标记 lazy + pushdown（把「欠账挂在哪、什么时候还」看清楚）");

    printf("      【第 1 步】区间加 a[2..5] += 3。注意看谁被「整段盖住」：\n");
    tree.trace = 1;
    st_range_add(&tree, 2, 5, 3);
    tree.trace = 0;
    printf("\n      改完之后的树：\n");
    st_print_tree(&tree);
    printf("      谁挂着欠账：\n");
    st_print_lazy(&tree);
    printf("      ⚠ 关键观察：被 [3,4] 这个整段盖住的两个叶子 a[3]、a[4] 的格子里**还是旧值**（8 和 1），\n");
    printf("        而 a[2]、a[5] 因为在 [2,2]、[5,5] 上被单独盖住，已经写成新值（6 和 7）。\n");
    printf("        同一个区间改完，树里有的格子新、有的格子旧 —— 全靠 lazy 记着谁欠了多少。\n");
    printf("        这就是「懒」——不需要的就不做。所以正确性依赖一条铁律：**往下走之前必须 pushdown**。\n");

    printf("\n      【第 2 步】现在单查一个点 a[3]，这一查必须一路钻到叶子 ⇒ 沿途要还账：\n");
    tree.trace = 1;
    int q3 = st_range_sum(&tree, 3, 3);
    tree.trace = 0;
    printf("      ⇒ a[3] = %d（原来 8，加了 3 ⇒ 11 ✔）。看上面的 pushdown 日志：账是一层一层还下去的\n", q3);
    printf("      还完账之后的树（欠账位置变了/没了）：\n");
    st_print_tree(&tree);

    printf("\n      【第 3 步】再查整个区间 [1,4]：这次 [1,4] 这个节点自己就能给出答案，\n");
    printf("        于是**完全不往下钻、也就不需要还账** —— 省时间就是这么来的。\n");
    tree.trace = 1;
    int q14b = st_range_sum(&tree, 1, 4);
    tree.trace = 0;
    printf("      ⇒ a[1..4] = %d（手算 5+6+11+4 = 26 ✔）\n", q14b);

    printf("\n      【第 4 步】把这一节的修改全部撤回，保持数据干净：a[2..5] 每个 -3\n");
    printf("        注意：撤回走的也是同一套「懒」流程，所以它同样会在某个节点上挂一笔新欠账。\n");
    st_range_add(&tree, 2, 5, -3);
    printf("      撤回后 a[1..8] 的总和 = %d（应为 36 ✔）—— 根节点的账永远是对的\n",
           st_range_sum(&tree, 1, 8));
    printf("      但「谁还挂着欠账」是这样的：\n");
    st_print_lazy(&tree);
    printf("      看下面的树：节点5 [3,4] 的和=9 是对的（a[3]+a[4] = 8+1 = 9 ✔），\n");
    printf("        可它两个孩子还写着 11 和 4（真值应是 8 和 1）—— 这就是「脏后代」。\n");
    printf("        完全不影响正确性：以后谁要用这两个孩子，pushdown 会先把 -3 补给它们。\n");
    st_print_tree(&tree);
    printf("      （注意：这一幕结束后树里仍挂着 lazy=-3，而且下一幕不重建树 —— 带着欠账继续干活，\n");
    printf("        正是线段树的常态；第 7 幕的对拍会连这个状态一起验证，看它会不会算错）\n");

    /* ============================================================
       第 7 幕：和朴素 O(n) 对拍
       ============================================================ */
    section("第 7 幕：20 步混合操作，每一步都和朴素 for 循环对拍");

    printf("      朴素版（只动 a[]）和线段树（只动 sum[]/lazy[]）是两套互不知情的实现，\n");
    printf("      同一串操作喂给两边，结果必须永远相等 —— 不相等就说明有一份写错了。\n\n");

    int op_kind[20], op_l[20], op_r[20], op_v[20];
    for (int k = 0; k < 20; k++) {
        int x = 1 + (int)(next_u32() % (unsigned int)n);
        int y = 1 + (int)(next_u32() % (unsigned int)n);
        op_l[k] = x < y ? x : y;
        op_r[k] = x < y ? y : x;                       /* 保证 L <= R */
        if ((next_u32() & 1u) == 0u) {
            op_kind[k] = 1;                            /* 区间加 */
            op_v[k] = (int)(next_u32() % 9u) - 4;      /* -4..+4 */
            if (op_v[k] == 0) {
                op_v[k] = 2;
            }
        } else {
            op_kind[k] = 2;                            /* 区间查询 */
            op_v[k] = 0;
        }
    }
    printf("      操作序列：\n");
    for (int k = 0; k < 20; k++) {
        if (op_kind[k] == 1) {
            printf("        %2d: range_add(%d,%d,%+d)\n", k + 1, op_l[k], op_r[k], op_v[k]);
        } else {
            printf("        %2d: range_sum(%d,%d)\n", k + 1, op_l[k], op_r[k]);
        }
    }

    int mismatches = 0;
    printf("\n       步  操作                    线段树    朴素    判定      工作量\n");
    printf("      ------------------------------------------------------------\n");
    for (int k = 0; k < 20; k++) {
        int seg_ans = 0;
        int naive_ans = 0;
        int adds = 0;
        long long visits = 0;
        char desc[32];

        if (op_kind[k] == 1) {
            snprintf(desc, sizeof(desc), "range_add(%d,%d,%+d)", op_l[k], op_r[k], op_v[k]);
            st_range_add(&tree, op_l[k], op_r[k], op_v[k]);      /* 线段树这套 */
            visits = tree.visits;
            st_naive_range_add(&tree, op_l[k], op_r[k], op_v[k]);/* 朴素那套，只动 a[] */

            /* 改完当场整体核对一遍：两边眼里的「全数组之和」必须相等 */
            seg_ans = st_range_sum(&tree, 1, n);
            naive_ans = st_naive_range_sum(&tree, 1, n, &adds);
        } else {
            snprintf(desc, sizeof(desc), "range_sum(%d,%d)", op_l[k], op_r[k]);
            seg_ans = st_range_sum(&tree, op_l[k], op_r[k]);
            visits = tree.visits;
            naive_ans = st_naive_range_sum(&tree, op_l[k], op_r[k], &adds);
        }

        printf("      %3d  %-21s %8d %8d    %s   线段树 %lld 节点 / 朴素 %d 格\n",
               k + 1, desc, seg_ans, naive_ans,
               seg_ans == naive_ans ? "✔ 一致" : "✘ 不一致", visits, adds);
        if (seg_ans != naive_ans) {
            mismatches++;
        }
    }
    printf("      ------------------------------------------------------------\n");
    printf("      对拍结论：20 步操作，不一致 = %d ⇒ %s\n", mismatches,
           mismatches == 0 ? "带懒标记的线段树实现正确 ✔" : "写错了，回去查 pushdown！");

    printf("\n      对拍结束后，把 a[] 和树里看到的值逐个再比一遍（这是最强的一次交叉验证）：\n");
    int point_bad = 0;
    for (int i = 1; i <= n; i++) {
        int from_tree = st_range_sum(&tree, i, i);
        int from_naive = st_naive_range_sum(&tree, i, i, NULL);
        printf("        a[%d]：线段树 %3d   朴素 %3d   %s\n", i, from_tree, from_naive,
               from_tree == from_naive ? "✔" : "✘");
        if (from_tree != from_naive) {
            point_bad++;
        }
    }
    printf("      ⇒ 单点全部一致：%s\n", point_bad == 0 ? "8 个点对上 ✔" : "有点对不上 ✘");

    printf("\n      最后的树（注意有些叶子还没被「叫醒」，欠账挂在祖先是完全正常的状态）：\n");
    st_print_tree(&tree);
    st_print_lazy(&tree);

    /* ============================================================
       收尾
       ============================================================ */
    printf("\n=== 本节结束 ===\n");
    printf("记住四句话：\n");
    printf("  1) 线段树把「区间」当节点：叶子管 1 个数，父亲 = 两个孩子相加，任何区间能拆成 O(log n) 个节点。\n");
    printf("  2) 数组开 4n+5：节点 i 的孩子是 2i、2i+1，n 不是 2 的幂时最大编号会远超 2n（本节实测过）。\n");
    printf("  3) 懒标记 lazy[node] = 「我自己改好了，孩子还不知道」。它自己的 sum 是对的，孩子的是脏的。\n");
    printf("  4) 所以只要打算往孩子走（查询或修改都一样），必须先 pushdown，否则读到的就是脏值。\n");

    st_free(&tree);
    return 0;
}
