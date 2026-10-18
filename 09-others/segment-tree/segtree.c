#include <stdio.h>
#include <stdlib.h>
#include "segtree.h"

/*
 * ============================================================================
 * 线段树实现。三个动作全靠"节点编号 = 2i / 2i+1"这一件事撑着：
 *     建树：自底向上，父亲 = 两个孩子相加
 *     查询：自顶向下，遇到"整段被覆盖"就停下取值
 *     修改：同上，遇到"整段被覆盖"就改自己 + 记欠账（lazy），不再往下
 * ============================================================================
 *
 * 先约定好每个数组下标的含义，后面全部照这个来：
 *
 *     node 是数组下标（1 起）；它管的区间记作 [l, r]；中点 mid = (l+r)/2
 *     左孩子 = node*2   管 [l, mid]
 *     右孩子 = node*2+1 管 [mid+1, r]
 *
 *   n = 8 时的完整编号（这就是"数组当树"的用法，一个指针都不用）：
 *
 *                      1  [1,8]
 *                 ┌────┴────┐
 *                 2  [1,4]   3  [5,8]
 *              ┌───┴───┐   ┌───┴───┐
 *              4 [1,2] 5 [3,4] 6 [5,6] 7 [7,8]
 *             ┌┴┐    ┌┴┐    ┌┴┐     ┌┴┐
 *             8 9    10 11  12 13   14 15    ← 叶子，各管一个元素
 *
 *   叶子编号最大是 15 = 2*8-1 ⇒ n 是 2 的幂时 2n 够用。
 *   n = 6 时就乱了：树不满，但编号照 2i/2i+1 往两边推，最大编号冲到 13 > 2*6=12 ⇒ 所以开 4n+5。
 */

/* ---------- 内部小工具：把一笔账"记在某个节点身上" ---------- */

/*
 * "这个节点的区间 [l,r] 里每个数都要 +v"，那么区间和要加 v * 区间长度。
 * 另外：只有"有孩子"的节点才需要记欠账 —— 叶子没孩子，欠给谁？
 * 记在叶子的 lazy 上不但没用，还会让打印出来的树看着像还有事没做完，所以用 l != r 挡掉。
 */
static void st_apply(SegTree *t, int node, int l, int r, int v)
{
    t->sum[node] += v * (r - l + 1);      /* ★区间和 = 每人加 v × 有几个人 */
    if (l != r) {
        t->lazy[node] += v;               /* ★欠账记在这里，先不往下传 */
    }
}

/*
 * pushdown：把挂在自己身上的欠账转给两个孩子，然后清零。
 *
 *   场景：原来 [1,4] 的和是 17（孩子 8 + 9），有一次"整个 [1,4] 每人 +3"被 st_apply
 *   记在了 [1,4] 这一格上，孩子还没动：
 *
 *   pushdown 之前                    pushdown 之后
 *   [1,4] 和=29 lazy=+3              [1,4] 和=29 lazy=0      ← 父亲的和一个字都不用改！
 *      ├ [1,2] 和=8   （脏）            ├ [1,2] 和=14 lazy=+3  ← 8 + 2人×3 = 14
 *      └ [3,4] 和=9   （脏）            └ [3,4] 和=15 lazy=+3  ← 9 + 2人×3 = 15
 *
 * 记住这条不变式，整棵树每一步都成立：
 *     sum[父] = sum[左] + sum[右] + lazy[父] × 父区间长度
 *   还账前：29 = 8 + 9 + 3×4 = 29 ✔；还账后 lazy[父]=0 ⇒ 29 = 14 + 15 + 0 ✔
 *
 * 为什么父亲的 sum 不用改？因为父亲的 sum 在这次修改发生时就加上去了（见 st_apply），
 * 欠的只是"孩子还不知道"。pushdown 是把消息传下去，不是把账重算一遍。
 */
static void st_pushdown(SegTree *t, int node, int l, int r)
{
    int z = t->lazy[node];
    if (z == 0) {
        return;                       /* 最常见的分支：没欠账，直接走 */
    }
    if (l == r) {
        t->lazy[node] = 0;            /* 叶子没有孩子，欠账就地抹掉（值已经加在自己身上了） */
        return;
    }

    int mid = l + (r - l) / 2;        /* 这样写而不是 (l+r)/2：n 很大时 l+r 会溢出，这是好习惯 */
    st_apply(t, node * 2, l, mid, z);
    st_apply(t, node * 2 + 1, mid + 1, r, z);
    t->lazy[node] = 0;

    if (t->trace) {
        printf("            ↡ pushdown：节点 [%d,%d] 把欠账 %+d 转给 [%d,%d] 和 [%d,%d]，自己清零\n",
               l, r, z, l, mid, mid + 1, r);
    }
}

/* ---------- 建树 ---------- */

/*
 * 递归建树：叶子直接取原值，内部节点 = 两个孩子相加。
 * 后序（先建孩子再算父亲），一趟 O(n)。
 */
static void st_build_rec(SegTree *t, int node, int l, int r)
{
    if (l == r) {
        t->sum[node] = t->a[l];       /* 叶子：管的区间只有一个人，和就是它自己 */
        t->lazy[node] = 0;
        return;
    }
    int mid = l + (r - l) / 2;
    st_build_rec(t, node * 2, l, mid);
    st_build_rec(t, node * 2 + 1, mid + 1, r);
    t->sum[node] = t->sum[node * 2] + t->sum[node * 2 + 1];
    t->lazy[node] = 0;                /* 新树干净，谁也不欠谁 */
}

/* ---------- 区间修改（递归主体） ---------- */

/*
 * 三种情况，顺序绝对不能颠倒：
 *   1) [l,r] 和 [L,R] 完全不相交 ⇒ 转身就走，这个节点一点关系没有
 *   2) [l,r] 被 [L,R] 整个包住 ⇒ ★就改这个节点，欠账记在这里，**不再往下**（这就是懒）
 *   3) 只有一部分相交 ⇒ 必须钻进孩子。钻之前先 pushdown（否则孩子身上缺账）
 *
 * 为什么第 2 种可以停下来不改孩子？因为以后所有查询只要经过这里，
 * 要么整段取（取父亲的 sum，已经是对的），要么往下走（会先 pushdown 把账补齐）。
 * 换句话说：**欠账只可能在"往下走"的时候暴露，那我们就在往下走之前还。**
 */
static void st_add_rec(SegTree *t, int node, int l, int r, int L, int R, int v)
{
    t->visits++;
    if (R < l || r < L) {
        if (t->trace) {
            printf("        [%d,%d] 与目标 [%d,%d] 不相交 ⇒ 直接返回\n", l, r, L, R);
        }
        return;
    }
    if (L <= l && r <= R) {
        st_apply(t, node, l, r, v);
        if (t->trace) {
            printf("        [%d,%d] 被 [%d,%d] 整个盖住 ⇒ 只改这里：sum += %d×%d ⇒ %d，",
                   l, r, L, R, v, r - l + 1, t->sum[node]);
            if (l == r) {
                printf("叶子没孩子，所以 lazy 仍是 %d\n", t->lazy[node]);
            } else {
                printf("欠账 lazy=%+d 挂在本节点，孩子不动，不再往下\n", t->lazy[node]);
            }
        }
        return;
    }

    if (t->trace) {
        printf("        [%d,%d] 只和 [%d,%d] 部分相交 ⇒ 先 pushdown，再钻进两个孩子\n", l, r, L, R);
    }
    st_pushdown(t, node, l, r);        /* ★更新路径上也要 pushdown，见 README 的图 */

    int mid = l + (r - l) / 2;
    st_add_rec(t, node * 2, l, mid, L, R, v);
    st_add_rec(t, node * 2 + 1, mid + 1, r, L, R, v);

    /* 孩子都改完了，父亲的区间和重新由孩子汇总（此时 lazy[node] 必为 0） */
    t->sum[node] = t->sum[node * 2] + t->sum[node * 2 + 1];
    if (t->trace) {
        printf("        [%d,%d] 汇总孩子 ⇒ sum = %d + %d = %d\n",
               l, r, t->sum[node * 2], t->sum[node * 2 + 1], t->sum[node]);
    }
}

/* ---------- 区间查询（递归主体） ---------- */

/*
 * 结构和修改一模一样，只是"整段被盖住"时返回 sum 而不是修改：
 *   1) 不相交 ⇒ 贡献 0（注意：返回 0 是加法的单位元，所以能直接相加）
 *   2) 整段被盖住 ⇒ 直接返回这个节点的 sum，不往下（这才省下 O(n)）
 *   3) 部分相交 ⇒ pushdown 后分别问左右，回来相加
 */
static int st_sum_rec(SegTree *t, int node, int l, int r, int L, int R)
{
    t->visits++;
    if (R < l || r < L) {
        if (t->trace) {
            printf("        [%d,%d] 不在 [%d,%d] 里 ⇒ 贡献 0\n", l, r, L, R);
        }
        return 0;
    }
    if (L <= l && r <= R) {
        if (t->trace) {
            printf("        [%d,%d] 完整落在 [%d,%d] 里 ⇒ 整段取走 sum = %d（不再往下）\n",
                   l, r, L, R, t->sum[node]);
        }
        return t->sum[node];
    }

    if (t->trace) {
        printf("        [%d,%d] 和 [%d,%d] 部分相交 ⇒ 先 pushdown，再问左右\n", l, r, L, R);
    }
    st_pushdown(t, node, l, r);        /* ★查询路径上也要 pushdown！不然会读到脏孩子 */

    int mid = l + (r - l) / 2;
    int left_sum = st_sum_rec(t, node * 2, l, mid, L, R);
    int right_sum = st_sum_rec(t, node * 2 + 1, mid + 1, r, L, R);
    return left_sum + right_sum;
}

/* ---------- 生命周期 ---------- */

int st_init(SegTree *t, const int *src, int n)
{
    if (t == NULL || n <= 0) {
        printf("      [错误] st_init 参数不对：n=%d\n", n);
        return -1;
    }

    /* ★开 4n+5 而不是 2n：见文件头那段说明 + main.c 第 3 幕的实测表。
     * 多要 5 个是为了让 node*2+1 在最坏情况下也落在申请范围内，图个安心。 */
    int cap = 4 * n + 5;
    t->sum  = calloc((size_t)cap, sizeof(int));
    t->lazy = calloc((size_t)cap, sizeof(int));
    t->a    = calloc((size_t)n + 1, sizeof(int));
    if (t->sum == NULL || t->lazy == NULL || t->a == NULL) {
        free(t->sum);
        free(t->lazy);
        free(t->a);
        t->sum = NULL;
        t->lazy = NULL;
        t->a = NULL;
        printf("      [错误] 内存申请失败，n=%d（要 %d 个格子）\n", n, cap);
        return -1;
    }

    t->n = n;
    t->trace = 0;
    t->visits = 0;

    for (int i = 1; i <= n; i++) {
        t->a[i] = (src != NULL) ? src[i - 1] : 0;   /* src 是 0 起点，这里存成 1 起点 */
    }
    st_build_rec(t, 1, 1, n);                       /* 从 1 号节点、区间 [1,n] 开始建 */
    return 0;
}

void st_free(SegTree *t)
{
    if (t == NULL) {
        return;
    }
    free(t->sum);
    free(t->lazy);
    free(t->a);
    t->sum = NULL;     /* 三个都置空：漏掉任何一个，ASan 就会点名内存泄漏 */
    t->lazy = NULL;
    t->a = NULL;
    t->n = 0;
}

/* ---------- 对外接口 ---------- */

void st_range_add(SegTree *t, int L, int R, int v)
{
    if (t->sum == NULL) {
        printf("      [错误] st_range_add：树还没建\n");
        return;
    }
    if (L < 1 || R > t->n || L > R) {
        printf("      [错误] st_range_add 区间非法：[%d,%d]（合法 1..%d 且 L<=R）\n",
               L, R, t->n);
        return;
    }
    t->visits = 0;
    st_add_rec(t, 1, 1, t->n, L, R, v);
}

int st_range_sum(SegTree *t, int L, int R)
{
    if (t->sum == NULL) {
        printf("      [错误] st_range_sum：树还没建\n");
        return 0;
    }
    if (L < 1 || R > t->n || L > R) {
        printf("      [错误] st_range_sum 区间非法：[%d,%d]（合法 1..%d 且 L<=R）\n",
               L, R, t->n);
        return 0;
    }
    t->visits = 0;
    return st_sum_rec(t, 1, 1, t->n, L, R);
}

/* 单点加：把区间缩成 [i,i] 就是单点。树高 O(log n)，所以也是 O(log n)。 */
void st_point_add(SegTree *t, int i, int delta)
{
    st_range_add(t, i, i, delta);
}

/* ---------- 朴素版：两套互不知情的实现，专门用来互相挑错 ---------- */

int st_naive_range_sum(const SegTree *t, int L, int R, int *adds_out)
{
    int sum = 0;
    int adds = 0;
    for (int i = L; i <= R; i++) {
        sum += t->a[i];
        adds++;
    }
    if (adds_out != NULL) {
        *adds_out = adds;
    }
    return sum;
}

/* 只动 a[]，绝不动 sum[]/lazy[] —— 这样它才配当"标准答案" */
void st_naive_range_add(SegTree *t, int L, int R, int v)
{
    for (int i = L; i <= R; i++) {
        t->a[i] += v;
    }
}

/* ---------- 打印 ---------- */

/* 递归打印：每个节点一行，缩进表示层级，把"区间 / 和 / 欠账"三件事全摊开 */
static void st_print_rec(const SegTree *t, int node, int l, int r, int depth)
{
    printf("      ");                   /* 和上面 ┌── 那行对齐 */
    for (int d = 0; d < depth; d++) {
        printf("      ");          /* 每深一层再缩进 6 格 */
    }
    printf("└─");
    printf(" 节点%-3d [%d,%d]  和=%-4d", node, l, r, t->sum[node]);

    if (t->lazy[node] != 0) {
        printf("  lazy=%+d ←【欠账挂在这里，孩子还不知道】", t->lazy[node]);
    }
    if (l == r) {
        printf("  (叶子，对应 a[%d])", l);
    }
    printf("\n");

    if (l != r) {
        int mid = l + (r - l) / 2;
        st_print_rec(t, node * 2, l, mid, depth + 1);
        st_print_rec(t, node * 2 + 1, mid + 1, r, depth + 1);
    }
}

void st_print_tree(const SegTree *t)
{
    printf("      ┌── 线段树（每行一个节点：编号 / 管的区间 / 区间和 / 欠账）──\n");
    st_print_rec(t, 1, 1, t->n, 0);
    printf("      └── 共 %d 个元素；数组申请了 %d 格（4n+5），空白格子都是没用上的\n",
           t->n, 4 * t->n + 5);
}

/* 递归找挂着欠账的节点：深度只有树高，递归完全安全 */
static void st_find_lazy(SegTree *t, int node, int l, int r, int *count)
{
    if (t->lazy[node] != 0) {
        (*count)++;
        printf("        节点%-3d [%d,%d]  lazy=%+d  ⇒ 这一段里每个数都欠 %+d，孩子们还是老账\n",
               node, l, r, t->lazy[node], t->lazy[node]);
    }
    if (l == r) {
        return;
    }
    int mid = l + (r - l) / 2;
    st_find_lazy(t, node * 2, l, mid, count);
    st_find_lazy(t, node * 2 + 1, mid + 1, r, count);
}

/* 只把挂着欠账的节点挑出来，方便"现在到底谁还没还账"一眼看清 */
void st_print_lazy(const SegTree *t)
{
    /* const 只是"我承诺不改树"，递归里也确实只读；这里脱一次 const 好让签名统一 */
    int count = 0;
    SegTree *m = (SegTree *)t;

    st_find_lazy(m, 1, 1, m->n, &count);
    if (count == 0) {
        printf("      现在没有任何节点挂着 lazy ⇒ 所有欠账都已经推到孩子身上了\n");
    } else {
        printf("      ⇒ 一共 %d 个节点挂着欠账。它们自己的 sum 是对的，但**子树里的值还是旧的**\n",
               count);
    }
}

/* ---------- 4n 的实证 ---------- */

/* 递归算"建 n 个元素的树，最大会用到几号格子"。只看下标递推，不申请内存。 */
static int st_probe_max_index(int node, int l, int r)
{
    if (l == r) {
        return node;                       /* 父亲的编号一定比孩子小，叶子就是最深的那格 */
    }
    int mid = l + (r - l) / 2;
    int left_max = st_probe_max_index(node * 2, l, mid);
    int right_max = st_probe_max_index(node * 2 + 1, mid + 1, r);
    return left_max > right_max ? left_max : right_max;
}

int st_max_node_index(int n)
{
    if (n <= 0) {
        return 0;
    }
    return st_probe_max_index(1, 1, n);
}
