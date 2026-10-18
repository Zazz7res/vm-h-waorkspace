#include <stdio.h>
#include <stdlib.h>
#include "rbtree.h"

/*
 * ===================================================================
 * 红黑树 · 实现文件
 * ===================================================================
 * 阅读顺序建议：
 *   1) rbtree_init（先搞懂"哨兵"和"空树长什么样"）
 *   2) rbtree_rotate_left / rbtree_rotate_right（会转）
 *   3) rbtree_insert（内含三种修复情况：情况 1 变色上移 / 情况 2 内侧掰外侧 / 情况 3 终杀）
 *   4) rbtree_check（先学会验货，再往下读心里有底）
 *   5) rbtree_remove + rb_delete_fixup（双黑冒泡，CASE 1~4 + 镜像）
 *
 * 【贯穿全文的约定】
 *   树里【永远不出现裸 NULL 孩子】：所有空位都指向唯一的黑哨兵 t->nil。
 *   于是 `uncle->color` 这种代码可以直接写，读出来就是"黑"，
 *   正好等于教材里"NULL 视为黑叶子"那条性质 —— 把约定变成了数据结构。
 *   ⚠ 唯一不要碰的：哨兵的 left / right / parent 字段。它不往外指任何东西，
 *     只借用它的 color 和"存在性"。
 * ===================================================================
 */

/* 打印日志时给人看的颜色名 */
static const char *rb_color_cn(RBColor color)
{
    return (color == RB_RED) ? "红" : "黑";
}

/* 树形打印时用的单字母标记：R / B（宽度固定，排版整齐） */
static const char *rb_color_mark(RBColor color)
{
    return (color == RB_RED) ? "R" : "B";
}

/* ===================================================================
 * 第 1 组：内部小工具
 * =================================================================== */

/*
 * rb_transplant(t, u, v)：把 u 从树上摘掉，让 v 顶到 u 原来的位置。
 * 三件事：① 父方（或根指针）改指 v  ② v 的 parent 认新爸  ③ 完事，u 成孤儿
 * 抽成函数是因为删除里要用它 4 次，每次三个指针都不能写漏。
 * ⚠ 它【只搬位置，不搬子树内容】：u 自己的孩子还挂在 u 身上，
 *   所以调用前必须保证 u 除了 v 之外没有别的孩子需要保留。
 */
static void rb_transplant(RBTree *t, RBNode *u, RBNode *v)
{
    if (u->parent == t->nil) {
        t->root = v;                 /* u 本来是根 */
    } else if (u == u->parent->left) {
        u->parent->left = v;         /* u 是左孩子 */
    } else {
        u->parent->right = v;        /* u 是右孩子 */
    }
    v->parent = u->parent;           /* v 认新爸爸。v 是哨兵时这行也照写：
                                      * 哨兵的 parent 字段我们从不读，写了也不影响正确性，
                                      * 换来的是少一个 if —— 工程里就是这么取舍的 */
}

/* 子树里最小的节点：一路往左走到底。 */
static RBNode *rb_minimum(RBTree *t, RBNode *node)
{
    while (node->left != t->nil) {
        node = node->left;
    }
    return node;
}

/* 后序销毁真实节点；遇到哨兵就返回（哨兵全树只有一个，由 rbtree_destroy 释放）。 */
static void rb_destroy_rec(RBTree *t, RBNode *node)
{
    if (node == t->nil) {
        return;
    }
    rb_destroy_rec(t, node->left);
    rb_destroy_rec(t, node->right);
    free(node);
}

/* ===================================================================
 * 第 2 组：建树 / 拆树
 * ===================================================================
 * 空树不是 root == NULL，而是 root == nil。
 * 一旦接受这个设定，"这棵树空不空"就统一写成 t->root == t->nil，
 * 而且任何节点读自己孩子颜色时都永远不会解引用 NULL。
 */
void rbtree_init(RBTree *t)
{
    RBNode *sentinel = malloc(sizeof(RBNode));
    if (sentinel == NULL) {
        printf("    [错误] 连哨兵都申请不到，红黑树建不起来\n");
        t->root = NULL;
        t->nil = NULL;
        t->verbose = 0;
        return;
    }

    sentinel->value = 0;
    sentinel->color = RB_BLACK;      /* ★ 性质 3：叶子（空位）是黑的，就靠它保证 */
    sentinel->parent = NULL;         /* 这三个字段【故意】留 NULL，谁也不许读 */
    sentinel->left = NULL;
    sentinel->right = NULL;

    t->nil = sentinel;
    t->root = sentinel;              /* 空树：根就是哨兵 */
    t->verbose = 0;
}

/*
 * 销毁：先按后序把所有真实节点 free 掉，最后 free 唯一的哨兵，
 * 然后把两个指针都置 NULL，防止调用者误用（rbtree_check 会挡这种树）。
 */
void rbtree_destroy(RBTree *t)
{
    if (t == NULL || t->nil == NULL) {
        return;
    }
    rb_destroy_rec(t, t->root);
    free(t->nil);
    t->nil = NULL;
    t->root = NULL;
    t->verbose = 0;
}

void rbtree_set_verbose(RBTree *t, int on)
{
    t->verbose = on ? 1 : 0;
}

/* ===================================================================
 * 第 3 组：左旋 / 右旋
 * ===================================================================
 * 形状变化和 AVL 完全一样，区别是：这里不用维护 height，但要维护 parent。
 * 一共要改 6 个指针，漏一个，删除时"往爸爸走"就走到别人家去了。
 *
 * 左旋：x 的右孩子 y 上位
 *        x                                  y
 *       / \        左旋 x           →      /   \
 *      A   y                             A     y
 *         / \                                  / \
 *        B   C                                     B   C
 *
 * 右旋：y 的左孩子 x 上位（完全镜像）
 *            y                          x
 *           / \      右旋 y      →      / \
 *          x   C                       y   C
 *         / \                         / \
 *        A   B                       A   B
 *
 * 旋转【只改形状，不改颜色】。改颜色是修复函数的事，职责分得很干净。
 * =================================================================== */
void rbtree_rotate_left(RBTree *t, RBNode *x)
{
    RBNode *y = x->right;                  /* y 要上位 */

    x->right = y->left;                    /* B 过继给 x 当右腿 */
    if (y->left != t->nil) {
        y->left->parent = x;               /* 真孩子才改认爸爸；B 是哨兵则不动 */
    }

    y->parent = x->parent;                 /* y 接班：认 x 的爸爸当爸爸 */
    if (x->parent == t->nil) {
        t->root = y;                       /* x 本来是根 → y 成新根 */
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }

    y->left = x;                           /* 最后：x 降级成 y 的左孩子 */
    x->parent = y;
}

void rbtree_rotate_right(RBTree *t, RBNode *y)
{
    RBNode *x = y->left;

    y->left = x->right;                    /* B 过继给 y 当左腿 */
    if (x->right != t->nil) {
        x->right->parent = y;
    }

    x->parent = y->parent;
    if (y->parent == t->nil) {
        t->root = x;
    } else if (y == y->parent->left) {
        y->parent->left = x;
    } else {
        y->parent->right = x;
    }

    x->right = y;
    y->parent = x;
}

/* ===================================================================
 * 第 4 组：插入 = BST 插入 + 修复
 * ===================================================================
 * 【为什么新节点一律染红】
 *   插成黑色 → 那条路径黑节点数 +1 → 性质 5（黑高处处相等）立刻破，
 *             破坏范围是"这个节点到所有叶子"，基本只能大动干戈。
 *   插成红色 → 黑高一点没变，性质 5 完好，最坏只是和紅爸爸"红红相连"（性质 4）。
 *             而性质 4 是【局部】问题，改 2~3 个颜色 + 最多 2 次旋转就能修好。
 *   一句话：把破坏限制在最小范围，是红黑树所有设计选择的出发点。
 *
 * 【修复为什么一定能停下来】
 *   循环条件是"爸爸是红的"。情况 1 把冲突往上推一层（z = 爷爷），
 *   爷爷的爸爸如果是红的再推一层；往上走总有到根的时候，
 *   而根永远是黑的（循环外最后一行强制染黑），所以必然终止。
 *   情况 2 只是把"内侧"掰成"外侧"，紧接着必然执行情况 3 并结束。
 *
 * 【旋转次数】插入全程最多 2 次旋转（情况 2+3 各一次，或者只走情况 3 一次）；
 *   变色次数却可能到 O(log n)。AVL 是反过来：最多 2 次旋转且从不"上移变色"。
 */
RBNode *rbtree_insert(RBTree *t, int value)
{
    if (t->nil == NULL) {
        printf("    [错误] 树没初始化好（连哨兵都没了），不插了\n");
        return NULL;
    }

    /* --- 阶段 A：完全按 BST 找位置，走到哨兵就是找到了空位 --- */
    RBNode *parent = t->nil;
    RBNode *cur = t->root;
    while (cur != t->nil) {
        parent = cur;
        if (value < cur->value) {
            cur = cur->left;
        } else if (value > cur->value) {
            cur = cur->right;
        } else {
            if (t->verbose) {
                printf("    [插入] %d 已经在树里 → 本教学版本不收重复值，忽略\n", value);
            }
            return NULL;
        }
    }

    RBNode *z = malloc(sizeof(RBNode));
    if (z == NULL) {
        printf("    [错误] 插入 %d 时内存申请失败\n", value);
        return NULL;
    }
    z->value = value;
    z->color = RB_RED;                     /* ★ 一律红色，理由见函数头 */
    z->left = t->nil;                      /* 两条腿都指向哨兵，绝不裸 NULL */
    z->right = t->nil;
    z->parent = parent;

    if (parent == t->nil) {
        t->root = z;                       /* 插进空树，自己当根 */
    } else if (value < parent->value) {
        parent->left = z;
    } else {
        parent->right = z;
    }

    if (t->verbose) {
        if (parent == t->nil) {
            printf("    [插入] %d 插进空树 → 它就是根（根随后会被强制染黑）\n", value);
        } else {
            printf("    [插入] %d 按 BST 挂在 %d 的%s腿，先染成【红】（爸爸 %d 是%s的）\n",
                   value, parent->value, (value < parent->value) ? "左" : "右",
                   parent->value, rb_color_cn(parent->color));
        }
    }

    /* --- 阶段 B：修复性质 4 --- */
    /* 若爸爸是黑的（或自己就是根），循环一次都不进，直接结束：这就是多数情况 */
    while (z->parent->color == RB_RED) {
        RBNode *p = z->parent;             /* 爸爸 */
        RBNode *g = p->parent;             /* 爷爷：爸爸红 ⇒ 根必黑 ⇒ 爷爷一定存在 */

        if (p == g->left) {
            RBNode *u = g->right;          /* 叔叔：可能是哨兵，但哨兵就是黑的，直接读颜色 */

            if (u->color == RB_RED) {
                /* ---------- 情况 1：叔叔红 → 只变色，冲突上移，不旋转 ---------- */
                if (t->verbose) {
                    printf("    [修复·情况1] 叔叔 %d 是红的 → 爸 %d 变黑、叔 %d 变黑、爷 %d 变红；"
                           "黑高没变（这条路上还是 1 个黑），冲突推到爷爷身上，不旋转\n",
                           u->value, p->value, u->value, g->value);
                }
                p->color = RB_BLACK;
                u->color = RB_BLACK;
                g->color = RB_RED;
                z = g;                     /* 往上跳一层继续检查 */
                continue;
            }

            if (z == p->right) {
                /* ---------- 情况 2：叔叔黑 + 我在内侧（爸左我右）→ 先把形状掰直 ---------- */
                if (t->verbose) {
                    printf("    [修复·情况2] 叔叔是黑哨兵，而 %d 挂在爸 %d 的【右】侧（内侧）"
                           " → 对爸爸做一次【左旋】把 %d 转到外侧（这一步不动颜色）\n",
                           z->value, p->value, z->value);
                }
                z = p;                     /* 旋转后"当爸的"变成当初那个内侧孙子 */
                rbtree_rotate_left(t, z);
                p = z->parent;             /* 重新取 p / g，情况 3 要用新位置 */
                g = p->parent;
            }

            /* ---------- 情况 3：叔叔黑 + 我在外侧（爸左我也左）→ 变色 + 旋转，收工 ---------- */
            if (t->verbose) {
                printf("    [修复·情况3] %d 在爸 %d 的【左】侧（外侧）→ 爸 %d 染黑、爷 %d 染红、"
                       "对爷爷做【右旋】。红红相连消除，且这条路径黑数不变，祖先无需再动\n",
                       z->value, p->value, p->value, g->value);
            }
            p->color = RB_BLACK;
            g->color = RB_RED;
            rbtree_rotate_right(t, g);
            /* 到这里这一支的黑高和"插入前"一样，循环条件必然为假，退出 */
        } else {
            /* ---------- 完全镜像：爸在爷的右边 ---------- */
            RBNode *u = g->left;

            if (u->color == RB_RED) {
                if (t->verbose) {
                    printf("    [修复·情况1·镜像] 叔叔 %d 是红的 → 爸 %d 变黑、叔 %d 变黑、爷 %d 变红；"
                           "黑高没变（这条路上还是 1 个黑），冲突推到爷爷身上，不旋转\n",
                           u->value, p->value, u->value, g->value);
                }
                p->color = RB_BLACK;
                u->color = RB_BLACK;
                g->color = RB_RED;
                z = g;
                continue;
            }

            if (z == p->left) {
                if (t->verbose) {
                    printf("    [修复·情况2·镜像] 叔叔是黑哨兵，而 %d 挂在爸 %d 的【左】侧（内侧）"
                           " → 对爸爸做一次【右旋】把 %d 转到外侧（这一步不动颜色）\n",
                           z->value, p->value, z->value);
                }
                z = p;
                rbtree_rotate_right(t, z);
                p = z->parent;
                g = p->parent;
            }

            if (t->verbose) {
                printf("    [修复·情况3·镜像] %d 在爸 %d 的【右】侧（外侧）→ 爸 %d 染黑、爷 %d 染红、"
                       "对爷爷做【左旋】。红红相连消除，且这条路径黑数不变，祖先无需再动\n",
                       z->value, p->value, p->value, g->value);
            }
            p->color = RB_BLACK;
            g->color = RB_RED;
            rbtree_rotate_left(t, g);
        }
    }

    /* 收尾：性质 2 —— 根永远染回黑色。
     * 为什么染黑根不会破坏性质 5？根是所有路径的公共起点，
     * 它变黑等于【每条路径同时 +1 个黑】，相等关系仍然成立。 */
    t->root->color = RB_BLACK;
    return z;
}

/* 给"背着双黑这笔账的那个位置"起一个人类看得懂的名字。
 * x 经常就是哨兵本身（删的是黑叶子时顶上来的是空位），
 * 直接打 x->value 会冒出一个莫名其妙的 0，所以专门包一层。 */
static void rb_hole_name(const RBTree *t, const RBNode *x, char *buf, size_t len)
{
    if (x == t->nil) {
        snprintf(buf, len, "空位(黑哨兵)");
    } else {
        snprintf(buf, len, "节点 %d", x->value);
    }
}

/* ===================================================================
 * 第 5 组：删除
 * ===================================================================
 * 全树最啰嗦的一段，但骨架就三步：
 *
 * 第一步：按 BST 的规矩决定"真正离开树的是哪个节点"（y）
 *   - z 没有左孩子 → 右孩子（或哨兵）顶上来
 *   - z 没有右孩子 → 左孩子顶上来
 *   - 两个孩子都有 → 摘右子树最小节点 y（后继），把 y 搬到 z 的位子。
 *     后继一定没有左孩子，所以它最多只有一条右腿，搬动很安全。
 *
 * 第二步：记账 —— 记住 y【离开前】的颜色。
 *   红节点被拿走：黑高没变，性质 5 完好，直接结束（很多删除就这么便宜）。
 *   黑节点被拿走：那条路径少了一个黑 → 破坏性质 5 → 进入第三步。
 *
 * 第三步：把亏空记在顶替者 x 身上，这叫【双重黑】(double black)。
 *   ⚠ 双黑不是真有一个"黑黑"的节点，而是一种记账状态："这条路径还欠一个黑"。
 *   x 带着这笔账往上冒泡，交给 rb_delete_fixup 去借黑。
 *
 * 借黑的四个情况（x 是左孩子；x 是右孩子完全镜像）。w = x 的兄弟：
 *   CASE 1  兄弟红 → 兄弟染黑、爸染红、对爸左旋 ⇒ 换一个黑兄弟进来（只为进 CASE 2/3/4）
 *   CASE 2  兄弟黑 + 兄弟俩孩子都黑 → 没得借：把兄弟染红，亏空推给爸爸，x 上移一层
 *           （这就是删除最坏要走 O(log n) 层的原因，和插入情况 1 同一个套路）
 *   CASE 3  兄弟黑 + 远侄子黑（近侄子红）→ 近侄子染黑、兄弟染红、对兄弟右旋 ⇒ 掰成 CASE 4
 *   CASE 4  兄弟黑 + 远侄子红 → 兄弟继承爸爸的颜色、爸爸染黑、远侄子染黑、对爸爸左旋
 *           ⇒ x 这边路径正好补回一个黑，账平了，x = root 直接退出
 *   退出循环后 x->color = RB_BLACK：注销双黑标记。
 *
 * 【实现程度，诚实说明】上面四步 + CASE 1~4 及左右镜像全部实现了，不是半成品。
 *   没实现的是：迭代版遍历、节点内存池、以及"删除后不回收节点只打墓碑"这种工程变体。
 *   正确性靠 rbtree_check()（五条性质全查）+ main.c 里的随机增删压力测试兜底。
 * =================================================================== */
static void rb_delete_fixup(RBTree *t, RBNode *x)
{
    /* 双黑是个【记账状态】，不是一个真节点：x 常常就是那个哨兵（它没值可打），
     * 所以这里给它起个名字，避免打印出莫名其妙的 "0"。 */
    char hole[32];
    rb_hole_name(t, x, hole, sizeof hole);

    /* 进来的第一眼就能销账的情况：顶替者本身是红的 → 染黑即补回一个黑，不用冒泡 */
    if (t->verbose && x != t->root && x->color == RB_RED) {
        printf("    [双黑·当场销账] 顶上来的%s 本来就是【红】的 → 直接染黑，"
               "这条路径的黑数正好补回来，一步都不用冒泡\n", hole);
    }

    /* 循环条件：x 不是根，而且 x 还带着双黑（读它的 color 就是"黑"） */
    while (x != t->root && x->color == RB_BLACK) {
        /* 每转一层重新报一次名：亏空的"户口"已经换到爸爸头上了 */
        rb_hole_name(t, x, hole, sizeof hole);

        if (x == x->parent->left) {
            RBNode *w = x->parent->right;

            /* ---------- CASE 1：兄弟红 → 转成黑兄弟 ---------- */
            if (w->color == RB_RED) {
                if (t->verbose) {
                    printf("    [双黑·CASE1] %s 这边缺一个黑，兄弟 %d 是红的 → 兄弟染黑、爸爸染红、"
                           "对爸爸【左旋】：先给自己换一个黑兄弟\n", hole, w->value);
                }
                w->color = RB_BLACK;
                x->parent->color = RB_RED;
                rbtree_rotate_left(t, x->parent);
                w = x->parent->right;      /* ★ 旋转后兄弟换人了，必须重取，否则后面全错 */
            }

            /* ---------- CASE 2：兄弟一家全黑 → 兄弟染红，亏空上移 ---------- */
            if (w->left->color == RB_BLACK && w->right->color == RB_BLACK) {
                if (t->verbose) {
                    printf("    [双黑·CASE2] 黑兄弟 %d 的两个孩子也都是黑的 → 没处借黑，"
                           "把兄弟染红（它那边路径黑数 -1，跟这边一样浅了），亏空推给爸爸\n",
                           w->value);
                }
                w->color = RB_RED;
                x = x->parent;             /* 爸爸接管这笔双黑账，往上冒泡 */
                continue;
            }

            /* ---------- CASE 3：远侄子黑 → 掰成 CASE 4 ---------- */
            if (w->right->color == RB_BLACK) {
                if (t->verbose) {
                    printf("    [双黑·CASE3] 兄弟 %d 黑，但它的【远】侄子(右)是黑哨兵、"
                           "近侄子 %d 是红的 → 近侄子染黑、兄弟染红、对兄弟【右旋】，换成 CASE 4\n",
                           w->value, w->left->value);
                }
                w->left->color = RB_BLACK;
                w->color = RB_RED;
                rbtree_rotate_right(t, w);
                w = x->parent->right;      /* 兄弟又换人了 */
            }

            /* ---------- CASE 4：远侄子红 → 一次解决 ---------- */
            if (t->verbose) {
                printf("    [双黑·CASE4] 兄弟 %d 黑、远侄子 %d 红 → 兄弟继承爸爸的颜色、"
                       "爸爸染黑、远侄子染黑、对爸爸【左旋】：这边路径补回一个黑，账平了\n",
                       w->value, w->right->value);
            }
            w->color = x->parent->color;
            x->parent->color = RB_BLACK;
            w->right->color = RB_BLACK;
            rbtree_rotate_left(t, x->parent);
            x = t->root;                   /* 账已平，直接把 x 提到根以退出循环 */
        } else {
            /* ---------- x 是右孩子：完全镜像（兄弟在左边，远侄子在左） ---------- */
            RBNode *w = x->parent->left;

            if (w->color == RB_RED) {
                if (t->verbose) {
                    printf("    [双黑·CASE1·镜像] %s 这边缺一个黑，兄弟 %d 是红的 → 兄弟染黑、爸爸染红、"
                           "对爸爸【右旋】：先给自己换一个黑兄弟\n", hole, w->value);
                }
                w->color = RB_BLACK;
                x->parent->color = RB_RED;
                rbtree_rotate_right(t, x->parent);
                w = x->parent->left;
            }

            if (w->left->color == RB_BLACK && w->right->color == RB_BLACK) {
                if (t->verbose) {
                    printf("    [双黑·CASE2·镜像] 黑兄弟 %d 一家全黑 → 兄弟染红，亏空推给爸爸\n",
                           w->value);
                }
                w->color = RB_RED;
                x = x->parent;
                continue;
            }

            if (w->left->color == RB_BLACK) {
                if (t->verbose) {
                    printf("    [双黑·CASE3·镜像] 兄弟 %d 的【远】侄子(左)是黑哨兵、"
                           "近侄子 %d 红 → 近侄子染黑、兄弟染红、对兄弟【左旋】\n",
                           w->value, w->right->value);
                }
                w->right->color = RB_BLACK;
                w->color = RB_RED;
                rbtree_rotate_left(t, w);
                w = x->parent->left;
            }

            if (t->verbose) {
                printf("    [双黑·CASE4·镜像] 兄弟 %d 黑、远侄子 %d 红 → 兄弟继承爸色、爸染黑、"
                       "远侄子染黑、对爸【右旋】，收工\n", w->value, w->left->value);
            }
            w->color = x->parent->color;
            x->parent->color = RB_BLACK;
            w->left->color = RB_BLACK;
            rbtree_rotate_right(t, x->parent);
            x = t->root;
        }
    }

    /* 注销双黑：走到这里要么 x 本来就是红的（染黑即补上亏空），
     * 要么账已经在 CASE 4 里被抹平（x 是根）。两种情况都只可能是"变黑"收尾。 */
    x->color = RB_BLACK;
}

int rbtree_remove(RBTree *t, int value)
{
    if (t->nil == NULL) {
        return 0;
    }

    /* 先按 BST 找到值为 value 的节点 z */
    RBNode *z = t->root;
    while (z != t->nil && z->value != value) {
        z = (value < z->value) ? z->left : z->right;
    }
    if (z == t->nil) {
        printf("    [删除] %d 不在树里 → 什么都不做，绝不「顺手」删掉别的节点\n", value);
        return 0;
    }

    RBNode *y = z;                       /* y = 真正【腾出原位置】的那个节点 */
    RBColor y_color_before = y->color;   /* ★ 它走之前是什么颜色 —— 决定要不要修复 */
    int     y_value_before = y->value;   /* 记账要用；z 下面就被 free 了，值先抄下来 */
    RBNode *x;                           /* x = 顶到 y 原位的那个节点（可能带双黑） */

    if (z->left == t->nil) {
        /* (a)/(b) 只有右腿（或光杆一条）：右腿直接顶上来 */
        x = z->right;
        rb_transplant(t, z, x);
        if (t->verbose) {
            printf("    [删除] %d 是%s → 直接把它的位置让给%s\n",
                   z->value,
                   (z->right == t->nil) ? "叶子" : "只有一条右腿的单孩子节点",
                   (x == t->nil) ? "黑哨兵（空位）" : "它的右孩子");
        }
    } else if (z->right == t->nil) {
        x = z->left;
        rb_transplant(t, z, x);
        if (t->verbose) {
            printf("    [删除] %d 只有一条左腿 → 让左孩子顶上来\n", z->value);
        }
    } else {
        /* (c) 两条腿都有：把后继（右子树最小）y 摘出来，搬到 z 的位置上 */
        y = rb_minimum(t, z->right);
        y_color_before = y->color;
        y_value_before = y->value;
        x = y->right;                    /* 后继没有左腿，最多一条右腿 */
        if (y->parent == z) {
            /* y 恰好就是 z 的右孩子：不能 transplant(y)，否则 y 会被搬两次、指针全乱。
             * 只要保证 x 认 y 当爸（x 是哨兵时这行也安全，见 rb_transplant 注释）。 */
            x->parent = y;
        } else {
            rb_transplant(t, y, x);      /* 先让 y 的右腿补上 y 的坑 */
            y->right = z->right;         /* y 接管 z 的整棵右子树 */
            y->right->parent = y;
        }
        rb_transplant(t, z, y);          /* y 整体搬到 z 的位置 */
        y->left = z->left;               /* 再把 z 的左子树交给 y */
        y->left->parent = y;
        y->color = z->color;             /* ★ 继承 z 的颜色：黑高不会因为"换了个人站这"而变化 */
        if (t->verbose) {
            printf("    [删除] %d 有两条腿 → 摘后继 %d 顶到它的位置，%d 再【继承 %d 原本的颜色(%s)】，"
                   "所以这个位置的黑数没变；真正腾出原位置、需要记账的是后继 %d（它原本是%s）\n",
                   z->value, y->value, y->value, z->value, rb_color_cn(y->color),
                   y_value_before, rb_color_cn(y_color_before));
        }
    }

    free(z);                             /* z 已经彻底不在树里了（后继情况里 y 已接管其位置） */

    if (y_color_before == RB_BLACK) {
        /* 只有"黑色离场"才欠账 —— 这是红黑树删除快慢的分水岭 */
        if (t->verbose) {
            printf("    [删除] 腾出原位置的 %d 原本是【黑】的 → 经过它的那条路少了一个黑，"
                   "这笔亏空记在顶替者头上（叫【双重黑】），开始往上借黑\n", y_value_before);
        }
        rb_delete_fixup(t, x);
    } else if (t->verbose) {
        printf("    [删除] 腾出原位置的 %d 原本是【红】的 → 每条路的黑数一点没动，不用修复（最便宜）\n",
               y_value_before);
    }
    return 1;
}

/* ===================================================================
 * 第 6 组：查找 / 中序遍历
 * =================================================================== */

const RBNode *rbtree_search(const RBTree *t, int value)
{
    if (t->nil == NULL) {
        return NULL;
    }
    const RBNode *cur = t->root;
    while (cur != t->nil) {              /* 撞上哨兵 = 这条路走完了没找到 */
        if (value == cur->value) {
            return cur;
        }
        cur = (value < cur->value) ? cur->left : cur->right;
    }
    return NULL;
}

/* 中序递归：左 → 自己 → 右。哨兵不是真节点，不打印。 */
static void rb_inorder_rec(const RBTree *t, const RBNode *node)
{
    if (node == t->nil) {
        return;
    }
    rb_inorder_rec(t, node->left);
    printf("%d ", node->value);
    rb_inorder_rec(t, node->right);
}

void rbtree_print_inorder(const RBTree *t)
{
    if (t->nil == NULL) {
        return;
    }
    rb_inorder_rec(t, t->root);
}

/* ===================================================================
 * 第 7 组：树形打印（颜色 + 黑色高度）
 * ===================================================================
 * 每行一个节点，写成  13[R] bh=2 ：
 *   [R]/[B] = 红 / 黑
 *   bh      = 这个节点子树的黑色高度：从它往下走到哨兵，路上黑节点个数
 *             （含它自己如果是黑的，不含哨兵）
 * 看图时可以顺手验证性质 5：同一个节点左右两个孩子标的 bh 应该相等。
 * =================================================================== */
static int rb_subtree_bh(const RBTree *t, const RBNode *node)
{
    if (node == t->nil) {
        return 0;
    }
    int bl = rb_subtree_bh(t, node->left);
    int br = rb_subtree_bh(t, node->right);
    int mine = (node->color == RB_BLACK) ? 1 : 0;
    return mine + (bl > br ? bl : br);
}

static void rb_print_rec(const RBTree *t, const RBNode *node, const char *prefix,
                         const char *edge, int is_last_child)
{
    if (node == t->nil) {
        return;
    }
    printf("%s%s%d[%s] bh=%d\n", prefix, edge, node->value,
           rb_color_mark(node->color), rb_subtree_bh(t, node));

    char child_prefix[160];
    snprintf(child_prefix, sizeof(child_prefix), "%s%s",
             prefix, is_last_child ? "   " : "│  ");

    int has_left = (node->left != t->nil);
    int has_right = (node->right != t->nil);
    if (has_left) {
        rb_print_rec(t, node->left, child_prefix, has_right ? "├─L " : "└─L ", !has_right);
    }
    if (has_right) {
        rb_print_rec(t, node->right, child_prefix, "└─R ", 1);
    }
}

void rbtree_print_tree(const RBTree *t)
{
    if (t->nil == NULL) {
        printf("  (这棵树没建好或者已经销毁了)\n");
        return;
    }
    if (t->root == t->nil) {
        printf("  (空树：根就是那个唯一的黑色哨兵 nil)\n");
        return;
    }
    rb_print_rec(t, t->root, "", "", 1);
    printf("  图例：[R]=红 [B]=黑 bh=该子树黑色高度（不含哨兵）。"
           "同一节点左右 bh 相等 = 性质5成立\n");
}

/* ===================================================================
 * 第 8 组：rbtree_check —— 五条性质全部验一遍
 * ===================================================================
 * 这是"让学习者信服"的核心设施：每次插入 / 删除之后调一次，
 * 有任何破坏都会打印【第几条性质 + 坏在哪个节点 + 当时颜色】。
 *
 * 检查项：
 *   性质 1  颜色只能是 RB_RED / RB_BLACK 两种（防写脏）
 *   性质 2  根是黑的
 *   性质 3  所有空位都指向【那一个】共享哨兵，且哨兵是黑的；
 *           树里不允许出现裸 NULL 的腿（真出现就说明哨兵没铺到位）
 *   性质 4  红节点两个孩子必须都是黑的（不许红红相连）
 *   性质 5  每个节点左右子树的黑色高度必须相等
 *   附加    child->parent 必须指回父亲（父子互相指认）。
 *           这条不属于五条性质，但用 parent 指针的实现最容易在这写错。
 *           ⚠ 哨兵例外：它的 parent 字段是随手写的垃圾值，所以跳过不查。
 * 返回 0 = 五条全通过；返回 n = 发现 n 处破坏。
 */
static int rb_check_walk(const RBTree *t, const RBNode *node, int *bad)
{
    if (node == t->nil) {
        return 0;                        /* 走到哨兵：黑高 0（哨兵本身不计入 bh） */
    }

    if (node->color != RB_RED && node->color != RB_BLACK) {
        printf("    ✗ 性质1被破坏：节点 %d 的颜色既不是红也不是黑（原始值 %d）\n",
               node->value, (int)node->color);
        (*bad)++;
    }

    if (node->left == NULL || node->right == NULL) {
        printf("    ✗ 性质3被破坏：节点 %d 有一条腿是裸 NULL，没指向共享哨兵\n", node->value);
        (*bad)++;
        return -1;                       /* 已经走不通了，别再往下报假错 */
    }

    if (node->left != t->nil && node->left->parent != node) {
        printf("    ✗ 附加检查失败：节点 %d 的左孩子 %d 不认它当爸爸（parent 写歪了）\n",
               node->value, node->left->value);
        (*bad)++;
    }
    if (node->right != t->nil && node->right->parent != node) {
        printf("    ✗ 附加检查失败：节点 %d 的右孩子 %d 不认它当爸爸（parent 写歪了）\n",
               node->value, node->right->value);
        (*bad)++;
    }

    if (node->color == RB_RED &&
        (node->left->color == RB_RED || node->right->color == RB_RED)) {
        printf("    ✗ 性质4被破坏：红节点 %d 的左孩子 %d[%s]、右孩子 %d[%s] 里有红的（红红相连）\n",
               node->value,
               node->left->value, rb_color_mark(node->left->color),
               node->right->value, rb_color_mark(node->right->color));
        (*bad)++;
    }

    int bl = rb_check_walk(t, node->left, bad);
    int br = rb_check_walk(t, node->right, bad);
    if (bl < 0 || br < 0) {
        return -1;
    }

    if (bl != br) {
        printf("    ✗ 性质5被破坏：节点 %d[%s] 的左子树黑高 %d，右子树黑高 %d，不相等\n",
               node->value, rb_color_mark(node->color), bl, br);
        (*bad)++;
    }

    int mine = (node->color == RB_BLACK) ? 1 : 0;
    return mine + (bl > br ? bl : br);
}

/*
 * 真正的检查主体。loud = 1 时通过也打印一份"体检报告"，
 * loud = 0 时只在真破环才打印 —— 压力测试每一步都查，不刷屏。
 * 返回破坏处数，0 = 五条性质 + parent 互指全部成立。
 */
static int rb_check_impl(const RBTree *t, int loud)
{
    if (t->nil == NULL) {
        printf("  校验 ✗ 树根本没初始化（或已销毁），没什么可检查的\n");
        return 1;
    }

    int bad = 0;

    if (t->nil->color != RB_BLACK) {
        printf("    ✗ 性质3被破坏：共享哨兵 nil 竟然被染成红色了\n");
        bad++;
    }
    if (t->root != t->nil && t->root->color != RB_BLACK) {
        printf("    ✗ 性质2被破坏：根节点 %d 是红色的\n", t->root->value);
        bad++;
    }

    int bh = rb_check_walk(t, t->root, &bad);

    if (bad == 0) {
        if (loud) {
            printf("  校验 ✓ 五条性质全部成立：1 颜色合法 / 2 根黑 / 3 空位全是同一个黑哨兵 / "
                   "4 无红红相连 / 5 黑高统一 bh=%d\n", bh);
            printf("         （另：父子指针全部互相指认）节点 %d 个，树高 %d 层；"
                   "红黑树允许高到 2·log2(n+1)\n", rbtree_count(t), rbtree_height(t));
        }
    } else {
        printf("  校验 ✗ 一共发现 %d 处破坏（这棵树已经不再是红黑树了）\n", bad);
    }
    return bad;
}

int rbtree_check(const RBTree *t)
{
    return rb_check_impl(t, 1);
}

/* 压力测试用：通过时不打印，只有真破环才打印详情 */
int rbtree_check_quiet(const RBTree *t)
{
    return rb_check_impl(t, 0);
}

/* ===================================================================
 * 第 9 组：统计
 * =================================================================== */
static int rb_count_rec(const RBTree *t, const RBNode *node)
{
    if (node == t->nil) {
        return 0;
    }
    return 1 + rb_count_rec(t, node->left) + rb_count_rec(t, node->right);
}

int rbtree_count(const RBTree *t)
{
    if (t->nil == NULL) {
        return 0;
    }
    return rb_count_rec(t, t->root);
}

static int rb_height_rec(const RBTree *t, const RBNode *node)
{
    if (node == t->nil) {
        return 0;
    }
    int hl = rb_height_rec(t, node->left);
    int hr = rb_height_rec(t, node->right);
    return 1 + (hl > hr ? hl : hr);
}

int rbtree_height(const RBTree *t)
{
    if (t->nil == NULL) {
        return 0;
    }
    return rb_height_rec(t, t->root);
}

/* 给头文件里"想用中文颜色名"的调用者（main.c 打印日志用） */
const char *rb_color_name_of(RBColor color)
{
    return rb_color_cn(color);
}
