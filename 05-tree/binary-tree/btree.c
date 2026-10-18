#include <stdio.h>
#include <stdlib.h>
#include "btree.h"

/*
 * ===================================================================
 * btree.c —— 普通二叉树的实现
 *
 * 阅读顺序建议：
 *   node_new → build_fixed_tree（手工连线）→ build_tree_from_marks（递归建树）
 *   → 三种递归遍历 → 三种迭代遍历 → 层序 → 统计查询 → print_tree → free_tree
 *
 * 贯穿全文件的一句话：
 *   **树是递归定义的结构，所以处理树几乎总是在写递归。**
 *   节点里有 left/right 两个指针，而 left/right 指向的东西又是一个节点 ——
 *   于是"处理一棵树"= "处理自己 + 处理左子树 + 处理右子树"，
 *   递归的终止条件永远是"手上这个指针是 NULL"（空树什么都不用做）。
 * ===================================================================
 */

/* ===================================================================
 * 自带的小工具：一个只存 const Node * 的极小栈
 * -------------------------------------------------------------------
 * 为什么在这里手写而不去 #include ../03-stack 的代码？
 *   因为 Makefile 的规则是把"本目录下所有 .c 一起编译" —— 每个叶子目录独立编译，
 *   跨目录 include 的头文件不在编译列表里，会直接编译失败。
 *   所以"层序要队列、迭代遍历要栈"，就得在**本目录**里各配一个最小版本。
 *
 * 它就是一个数组 + 一个"下一个空位在几号"的下标 top：
 *
 *   stack[]  ┌────┬────┬────┬────┬─ ─ ┐
 *            │ A  │ B  │ C  │    │    │      top = 3（指向下一个空位）
 *            └────┴────┴────┴────┴─ ─ ┘
 *             0    1    2    3
 *            └── 已用 ──┘
 *   push: stack[top] = x; top++;
 *   pop : top--;        return stack[top];
 *  后进先出，这就是"函数调用栈"的机制本体 —— 迭代版遍历只是把这个
 *  系统偷偷帮你用的栈拿到明面上写出来而已。
 * =================================================================== */

/* 入栈：成功返回 1，栈满返回 0（本文件所有调用者都会检查并把错误打出来） */
static int st_push(const Node *stack[], int *top_in_out, const Node *node)
{
    if (*top_in_out >= TREE_HELPER_CAP) {
        return 0;                    /* 满了：绝不能再写，写了就是越界踩内存 */
    }
    stack[*top_in_out] = node;       /* 先放在 top 指的格子里 */
    *top_in_out += 1;                /* 再把 top 往上挪一格：它永远指向"下一个空位" */
    return 1;
}

/* 出栈：栈空时返回 NULL。
 * 注意本文件往栈里放的节点地址全都不是 NULL（入栈前都判过空），
 * 所以这里用 NULL 当"栈空了"的信号不会产生歧义。 */
static const Node *st_pop(const Node *stack[], int *top_in_out)
{
    if (*top_in_out <= 0) {
        return NULL;
    }
    *top_in_out -= 1;                /* 先把 top 退回来，退回来的那格才是栈顶 */
    return stack[*top_in_out];
}

/* ===================================================================
 * 自带的小工具：一个只存 const Node * 的极小队列
 * -------------------------------------------------------------------
 * 和栈的区别就在"从哪头拿"：
 *   栈  = 从最后放进来的那个拿（后进先出）
 *   队列 = 从最早放进来的那个拿（先进先出）
 *
 * 实现用 head/tail 两个下标，**只往前不回头**（不做取模回绕）：
 *
 *   queue[]  ┌────┬────┬────┬────┬────┬─ ─ ┐
 *            │ 1  │ 2  │ 3  │ 4  │    │    │
 *            └────┴────┴────┴────┴────┴─ ─ ┘
 *              ▲head            ▲tail
 *              └─ 已出队 ─┘└─ 还在队里 ─┘
 *   入队：queue[tail] = x; tail++;
 *   出队：x = queue[head]; head++;
 *   队里元素个数 = tail - head
 *
 * 为什么不回绕？因为一棵 n 个节点的树，层序遍历最多只会入队 n 次
 * （每个节点恰好进队一次），所以只要容量 >= 节点数就绝对不会满。
 * 回绕（循环队列）是为了反复复用同一段空间，那是 04-queue 的主题；
 * 这里只是"用完就扔"的一次性工具，越简单越好。
 * =================================================================== */

/* 入队：成功 1，队满 0 */
static int qu_push(const Node *queue[], int *tail_in_out, const Node *node)
{
    if (*tail_in_out >= TREE_HELPER_CAP) {
        return 0;
    }
    queue[*tail_in_out] = node;
    *tail_in_out += 1;
    return 1;
}

/* 出队：空队列返回 NULL（同上：队里不会存 NULL，所以 NULL == 队空） */
static const Node *qu_pop(const Node *queue[], int *head_in_out)
{
    /* head 和 tail 的差值由调用者维护：head < tail 才说明有货 */
    return queue[(*head_in_out)++];  /* 先拿 head 这格，再把 head 往后挪 */
}

/* ===================================================================
 * 遍历的公共出口：把访问到的值追加到调用者给的数组里
 * -------------------------------------------------------------------
 * 为什么不在遍历函数里直接 printf？
 *   因为我们要把"递归版"和"迭代版"的结果摆在一起逐位比对，
 *   打印出来就收不回来了，写进数组才能用 for 循环对比。
 *   顺带把 00-c-foundation 学的"输出参数"又练一遍：
 *   *n_out 是一个"我带着走的计数器"，函数内部想改它必须拿到它的地址。
 * =================================================================== */
static void visit(const Node *node, int out[], int *n_out)
{
    if (*n_out >= TREE_HELPER_CAP) {
        return;                      /* 保护：结果数组按 TREE_HELPER_CAP 上限算，超了就丢 */
    }
    out[*n_out] = node->val;         /* 写在"下一个空位"上 */
    *n_out += 1;
}

/* ===================================================================
 * 1. node_new：造一个孤立节点
 * ===================================================================
 * 内存图（刚 malloc 完、还没接任何孩子）：
 *
 *      堆上：       ┌──────────┬────────┬────────┐
 *                  │ val = 7  │ NULL   │ NULL   │
 *                  └──────────┴────────┴────────┘
 *                        ▲
 *   返回的地址 ───────────┘
 *
 * 为什么左右一定要显式置 NULL？
 *   malloc 回来的内存里是垃圾值，不写 NULL 的话它"看起来有个左孩子"，
 *   下次遍历就会顺着垃圾地址跳进野地里 —— 典型的段错误来源。
 *   （用 calloc 也能顺带清零，但显式写 NULL 让"这个节点是空的"这件事
 *     在读代码时一目了然，教学场景我选看得懂。）
 */
Node *node_new(int val)
{
    Node *node = malloc(sizeof *node);   /* sizeof *node 比 sizeof(Node) 好：
                                          * 以后改结构体类型也不用改这里 */
    if (node == NULL) {
        printf("  [错误] malloc 失败，造不出值为 %d 的节点\n", val);
        return NULL;                     /* 教学程序不 exit，把判空的责任交给调用者 */
    }
    node->val = val;
    node->left = NULL;                   /* 新节点是"光杆"：没有左孩子 */
    node->right = NULL;                  /* 也没有右孩子 */
    return node;
}

/* ===================================================================
 * 2-A. build_fixed_tree：手工连指针，把一棵写死的树接出来
 * ===================================================================
 *
 * 目标树（本节所有输出都基于它，所以 README 里的结果是确定的）：
 *
 *              1
 *            /   \
 *           2     3
 *          / \     \
 *         4   5     6
 *
 * 节点 3 只有右孩子 —— 这是故意的：
 *   "某个孩子为空"在图上是画不出来的，必须靠代码里的 NULL 表达；
 *   留着这个不对称的结构，层序遍历和 print_tree 的输出才有区分度。
 *
 * 手工连线在内存里发生了什么（地址用 0xA~0xF 假装，真机每次运行都不同）：
 *
 *   0xA: [1 | ─────────┼ ─────────┐        root
 *        └─────────────┴──────────┼───► 0xC
 *   0xB: [2 | ─────────┬ ─────────┤
 *        └─────────────┼─┴────────┘
 *   0xC: [3 |  NULL    │ ─────────┐   ← 3 的 left 是 NULL，所以它没有左孩子
 *        └─────────────┴──┬───────┼───► 0xF
 *   0xD: [4 |  NULL    |  NULL   ]   ← 叶子：两张纸条都是空的
 *   0xE: [5 |  NULL    |  NULL   ]
 *   0xF: [6 |  NULL    |  NULL   ]
 *
 *   root ──► 0xA ─┬─ left  ──► 0xB ─┬─ left  ──► 0xD (4)
 *                 │                  └─ right ──► 0xE (5)
 *                 └─ right ──► 0xC ── right ──► 0xF (6)
 *
 * 手工连线的适用场景：写测试用例、写教学 demo、结构很小很固定。
 * 真实项目里不会这么干，一般用"递归建树"（下一个函数）或者从数据插入。
 */
Node *build_fixed_tree(void)
{
    /* 先造 6 个孤立节点：这一步之后每个节点的 left/right 都是 NULL */
    Node *n1 = node_new(1);
    Node *n2 = node_new(2);
    Node *n3 = node_new(3);
    Node *n4 = node_new(4);
    Node *n5 = node_new(5);
    Node *n6 = node_new(6);

    /* 万一 malloc 失败了，n1 会是 NULL，后面 n1->left 直接段错误。
     * 正式项目里这一步该 exit(1)，这里用 NULL 让 main.c 自己去判断。 */
    if (n1 == NULL || n2 == NULL || n3 == NULL ||
        n4 == NULL || n5 == NULL || n6 == NULL) {
        return n1;
    }

    /* 再接线：一行一句人话 */
    n1->left = n2;     /* "1 的左孩子是 2" = 把 2 的地址写进 1 的 left 纸条 */
    n1->right = n3;    /* "1 的右孩子是 3"                                  */
    n2->left = n4;     /* "2 的左孩子是 4"                                  */
    n2->right = n5;    /* "2 的右孩子是 5"                                  */
    n3->left = NULL;   /* "3 没有左孩子" —— 写出来是为了让人看到这件事      */
    n3->right = n6;    /* "3 的右孩子是 6"                                  */
    /* 4、5、6 三个叶子的 left/right 保持 NULL，什么都不用做 */

    return n1;         /* 返回根：整棵树的入口地址。只要拿到 0xA，全树都能走到 */
}

/* ===================================================================
 * 2-B. 递归建树的私有零件：一边读数组一边"往下钻"
 * ===================================================================
 * 序列写法叫"带空标记的前序序列"（DFS 前序 + 用 MARK_EMPTY 表示空孩子）：
 *   1, 2, 4, -1, -1, 5, -1, -1, 3, -1, 6, -1, -1
 *
 * 为什么必须带 -1 才能唯一还原树形？
 *   只给 1,2,4,5,3,6 这种前序序列，"4 的右孩子到底是谁"根本没法知道。
 *   把"这里没孩子"也显式写成 -1，序列就不再有任何歧义了 ——
 *   所以本函数读到的每个 -1 都会让某一层递归立刻 return NULL。
 *
 * pos_in_out 是"读到第几个了"，必须传地址：
 *   这跟 00-c-foundation 里"想改外面的变量就得传地址"是同一个道理。
 *   如果写成 int pos，每层递归各拿一份拷贝，
 *   左子树读完之后父层根本不知道"读到哪了"，右子树就会读到重复的数据。
 */
static Node *build_rec(const int marks[], int n, int *pos_in_out)
{
    if (*pos_in_out >= n) {
        return NULL;                 /* 数组读完了：当成空孩子，递归自然收敛 */
    }

    int value = marks[*pos_in_out];  /* 读当前这个数 */
    *pos_in_out += 1;                /* 消耗掉它：读过的数不再看第二次 */

    if (value == MARK_EMPTY) {
        return NULL;                 /* 空标记 → 这个位置没有节点 */
    }

    Node *node = node_new(value);
    if (node == NULL) {
        return NULL;                 /* 内存不够：这一子树建不出来 */
    }

    /* 关键的两行：先给左孩子递归，再给右孩子递归。
     * 递归进去以后它会自己接着往下读同一个数组（pos 是共享的），
     * 所以"左子树读走了几个数"完全由左子树的形状决定，我们不用管。 */
    node->left = build_rec(marks, n, pos_in_out);
    node->right = build_rec(marks, n, pos_in_out);
    return node;
}

/* 对外入口：给个整数数组（含 -1 空标记），造出一棵树 */
Node *build_tree_from_marks(const int marks[], int n)
{
    int pos = 0;                     /* 从下标 0 开始读 */
    return build_rec(marks, n, &pos);/* 传地址：让递归深层能推进 pos */
}

/* ===================================================================
 * 3-A. 递归前序：根 → 左 → 右
 * ===================================================================
 * 三个递归遍历只差"处理自己那一行写在第几个位置"，所以一起讲：
 *
 *   前序 preorder  : 先访问自己，再左，再右
 *   中序 inorder   : 先左，再访问自己，再右
 *   后序 postorder : 先左，再右，最后访问自己
 *
 * "前/中/后"三个字指的就是【访问自己这个节点】的时机排在两个孩子前面、
 * 中间还是后面 —— 左右子树的相对顺序永远是"左在前、右在后"，从不交换。
 *
 * 用固定树手算（README 里会再算一遍）：
 *
 *              1
 *            /   \
 *           2     3
 *          / \     \
 *         4   5     6
 *
 *   前序 = 1 2 4 5 3 6      ← 每到一个节点先喊它的名字，然后一头扎进左边
 *   中序 = 4 2 5 1 3 6      ← 一直往左下钻到底，回头喊父，再去右
 *   后序 = 4 5 2 6 3 1      ← 把孩子全喊完才轮到自己，所以最后一个是根 1
 *
 * 递归的"终止条件"就是 root == NULL：空树啥也没有，直接返回。
 * 有了它，叶子节点的两层递归（左、右）都立刻返回，逻辑才闭合。
 */
void preorder(const Node *root, int out[], int *n_out)
{
    if (root == NULL) {
        return;                      /* 递归出口：空树不产生任何输出 */
    }
    visit(root, out, n_out);         /* ① 先处理自己 —— 这一行在第 1 位 = "前"序 */
    preorder(root->left, out, n_out);/* ② 再把左子树整个交给自己（同一套代码） */
    preorder(root->right, out, n_out);/* ③ 最后右子树 */
}

/* 中序：左 → 自己 → 右。把 visit 从第一行挪到中间，仅此而已 */
void inorder(const Node *root, int out[], int *n_out)
{
    if (root == NULL) {
        return;
    }
    inorder(root->left, out, n_out); /* ① 左子树整棵先走完 */
    visit(root, out, n_out);         /* ② 左子树都处理完了，才轮到喊自己 —— "中"序 */
    inorder(root->right, out, n_out);/* ③ 右子树 */
}

/* 后序：左 → 右 → 自己。销毁整棵树时用的就是这个顺序，见 free_tree */
void postorder(const Node *root, int out[], int *n_out)
{
    if (root == NULL) {
        return;
    }
    postorder(root->left, out, n_out);
    postorder(root->right, out, n_out);
    visit(root, out, n_out);         /* ③ 两个孩子都完事了才写自己 —— "后"序 */
}

/* ===================================================================
 * 3-B. 迭代前序：把系统帮你用的栈拿到明面上
 * ===================================================================
 * 先看递归版真正发生了什么。preorder(1) 的调用栈是这样一层层压上去的：
 *
 *   压栈方向 ↓
 *   ┌────────────────────────┐
 *   │ preorder(3)  准备走右   │  ← 栈顶
 *   ├────────────────────────┤
 *   │ preorder(2)  准备走右   │
 *   ├────────────────────────┤
 *   │ preorder(1)  准备走右   │  ← main 的下面
 *   └────────────────────────┘
 *   系统必须记住"我欠谁还没走右子树"，这个"记住"靠的就是函数调用栈。
 *
 * 所以自己写迭代版 = 自己准备一个栈，把"欠着的右子树"存进去。
 * 前序的做法：弹出一个就访问它，然后**先压右、再压左**，
 * 因为栈后进先出 —— 最后压的左孩子会先弹出来，正好符合"先左后右"。
 */
void preorder_iter(const Node *root, int out[], int *n_out)
{
    const Node *stack[TREE_HELPER_CAP];
    int top = 0;                     /* top 永远指向"下一个空位"，也等于已用个数 */

    if (root != NULL) {
        if (!st_push(stack, &top, root)) {
            printf("  [错误] 迭代前序：栈溢出（树太大了）\n");
            return;
        }
    }

    while (top > 0) {                /* 栈空 = 没有欠着的节点了 = 遍历结束 */
        const Node *cur = st_pop(stack, &top);
        if (cur == NULL) {
            break;                   /* 只有栈空才会返回 NULL，这里当退出信号 */
        }
        visit(cur, out, n_out);      /* 弹出的瞬间就访问：根最先被访问 → 前序 */

        /* 压栈顺序是"右先左后"，这样左孩子才在栈顶，先被弹出 */
        if (cur->right != NULL) {
            if (!st_push(stack, &top, cur->right)) {
                printf("  [错误] 迭代前序：栈溢出\n");
                return;
            }
        }
        if (cur->left != NULL) {
            if (!st_push(stack, &top, cur->left)) {
                printf("  [错误] 迭代前序：栈溢出\n");
                return;
            }
        }
    }
}

/* ===================================================================
 * 3-C. 迭代中序：一条"钻到底"的循环
 * ===================================================================
 * 中序的递归是"先把整棵左子树走完才轮到自己"，翻译成迭代就是：
 *   一直往左下钻，沿途经过的节点全塞进栈（因为它们都是"我还没访问的父"），
 *   钻到 NULL 了，说明这一支的"最左"找到了 —— 弹栈顶、访问它、
 *   然后把 cur 指向它的右孩子，下一轮再去钻这条右子树的最左。
 *
 * 固定树走一遍（左边是栈，bottom→top）：
 *   cur=1  钻 1,2,4      栈=[1,2,4]   访问不出
 *   钻到 NULL → 弹 4 访问  栈=[1,2]     输出 4
 *   4 没右孩子 → 弹 2 访问  栈=[1]       输出 4 2
 *   2 的右孩子 5 → 钻 5     栈=[1,5]     ——
 *   弹 5 访问              栈=[1]       输出 4 2 5
 *   弹 1 访问              栈=[]        输出 4 2 5 1
 *   钻 3, 没有左 → 弹 3     栈=[]        输出 4 2 5 1 3
 *   3 的右孩子 6 → 钻 6 → 弹 6 访问      输出 4 2 5 1 3 6   ✅ 和手算一致
 */
void inorder_iter(const Node *root, int out[], int *n_out)
{
    const Node *stack[TREE_HELPER_CAP];
    int top = 0;
    const Node *cur = root;          /* cur = "我现在站在哪个节点上" */

    while (cur != NULL || top > 0) { /* 两个条件都要：cur 空了但栈里还有欠着的父 */
        while (cur != NULL) {        /* 一路向左钻到底，沿途节点全部入栈 */
            if (!st_push(stack, &top, cur)) {
                printf("  [错误] 迭代中序：栈溢出\n");
                return;
            }
            cur = cur->left;
        }
        cur = st_pop(stack, &top);   /* 钻到 NULL 了 → 栈顶就是"最左没访问的那个" */
        if (cur == NULL) {
            break;                   /* 正常路径走不到这里，纯粹是防越界的保险 */
        }
        visit(cur, out, n_out);      /* 左子树已经全出去了，现在轮到根 */
        cur = cur->right;            /* 转向右子树，让下一轮去钻它的最左 */
    }
}

/* ===================================================================
 * 3-D. 迭代后序：双栈法（最容易讲清楚的一种）
 * ===================================================================
 * 后序"左 右 根"最难迭代，因为访问一个节点的前提是**两个孩子都访问完了**，
 * 光靠一个栈你无法判断"栈顶的右子树到底走完了没"。
 *
 * 技巧：**反过来想**。后序 "左 右 根" 很难，但它的镜像
 *   "根 右 左" 几乎就是前序（前序 = 根 左 右，把压栈的左右顺序对调即可），
 *   非常好写。而 "根 右 左" 整个倒过来写，正好就是 "左 右 根"！
 * 所以：用一个"收集栈 collect"把 "根 右 左" 的顺序存下来，
 *   最后从 collect 里往外弹 —— 栈天生就把顺序反过来了，直接就是后序。
 *
 * 固定树完整跑一遍（work 是工作栈，collect 是收集栈，都按 bottom→top 写）：
 *   work=[1]        弹 1 → collect=[1]        压左2、右3
 *   work=[2,3]      弹 3 → collect=[1,3]      3 无左，压右6
 *   work=[2,6]      弹 6 → collect=[1,3,6]    6 无孩子
 *   work=[2]        弹 2 → collect=[1,3,6,2]  压左4、右5
 *   work=[4,5]      弹 5 → collect=[1,3,6,2,5]
 *   work=[4]        弹 4 → collect=[1,3,6,2,5,4]
 *   work=[] 结束
 *   collect 出栈顺序（后入先出）= 4, 5, 2, 6, 3, 1  ✅ 与手算后序完全一致
 *
 * 代价：多用一个栈，空间 O(n)。想 O(1) 额外空间的莫里斯遍历不在本节范围。
 */
void postorder_iter(const Node *root, int out[], int *n_out)
{
    const Node *work[TREE_HELPER_CAP];    /* s1：干活的栈 */
    const Node *collect[TREE_HELPER_CAP]; /* s2：收集的栈，最后倒出来 */
    int work_top = 0;
    int collect_top = 0;

    if (root == NULL) {
        return;
    }
    if (!st_push(work, &work_top, root)) {
        printf("  [错误] 迭代后序：栈溢出\n");
        return;
    }

    while (work_top > 0) {
        const Node *cur = st_pop(work, &work_top);
        if (cur == NULL) {
            break;
        }
        /* 这一步产生的是"根 → 右 → 左"序列，直接写进最终栈会正序；
         * 我们把它压进 collect，最后倒着弹出来就变成"左 → 右 → 根" */
        if (!st_push(collect, &collect_top, cur)) {
            printf("  [错误] 迭代后序：收集栈溢出\n");
            return;
        }
        /* 注意这里和前序相反：先压左、再压右，右孩子在栈顶先出来 */
        if (cur->left != NULL) {
            if (!st_push(work, &work_top, cur->left)) {
                printf("  [错误] 迭代后序：栈溢出\n");
                return;
            }
        }
        if (cur->right != NULL) {
            if (!st_push(work, &work_top, cur->right)) {
                printf("  [错误] 迭代后序：栈溢出\n");
                return;
            }
        }
    }

    while (collect_top > 0) {
        const Node *cur = st_pop(collect, &collect_top);
        if (cur == NULL) {
            break;
        }
        visit(cur, out, n_out);
    }
}

/* ===================================================================
 * 4. level_order：层序遍历（BFS），一层打印一行
 * ===================================================================
 * 前中后序都是"一条路走到黑再回头"（DFS），层序是"一层一层剥开"（BFS）：
 * 先把第 1 层（根）看完，再从左到右看第 2 层，再看第 3 层……
 * 这种"按发现的先后顺序处理"就要求**先进先出** —— 必须是队列，用栈立刻变形。
 *
 * 为什么会先进先出？因为第 k 层节点的孩子们，正好就是第 k+1 层，
 * 而且从左到右的顺序和它们父母入队的顺序一致。
 *
 *   固定树跑一遍（队列里的内容按 bottom→top 写，队头在左）：
 *   开始:   [1]                       打印第1层: 1
 *   1 出队, 孩子 2,3 入队:  [2,3]      打印第2层: 2 3
 *   2 出队 → 4,5 入队:      [3,4,5]
 *   3 出队 → 6  入队:       [4,5,6]
 *   4,5,6 出队（都没孩子）: []          打印第3层: 4 5 6   ✅ 一层一行
 *
 * "怎么知道一层结束了"：进入循环时先记下 tail - head，
 * 这就是当前层的节点个数 level_size；for 循环精确处理这么多个，
 * 处理完正好下一层全在队里 —— 这是层序遍历的标准写法，背下来。
 */
void level_order(const Node *root)
{
    if (root == NULL) {
        printf("  （空树，层序遍历什么都没有）\n");
        return;
    }

    const Node *queue[TREE_HELPER_CAP];
    int head = 0;                    /* 队头：下一个要出队的位置 */
    int tail = 0;                    /* 队尾：下一个空位 */
    int level = 1;

    if (!qu_push(queue, &tail, root)) {
        printf("  [错误] 层序遍历：队列溢出\n");
        return;
    }

    while (head < tail) {            /* head == tail 说明队列空了 */
        int level_size = tail - head;/* 此刻队里有多少个，就是这一层有多少个 */
        printf("  第 %d 层（%d 个）: ", level, level_size);

        for (int i = 0; i < level_size; i++) {
            const Node *cur = qu_pop(queue, &head);
            printf("%d ", cur->val);

            /* 两个孩子按"左先右后"排队尾，保证下一层也是从左往右打印 */
            if (cur->left != NULL) {
                if (!qu_push(queue, &tail, cur->left)) {
                    printf("\n  [错误] 层序遍历：队列溢出\n");
                    return;
                }
            }
            if (cur->right != NULL) {
                if (!qu_push(queue, &tail, cur->right)) {
                    printf("\n  [错误] 层序遍历：队列溢出\n");
                    return;
                }
            }
        }
        printf("\n");
        level += 1;
    }
}

/* ===================================================================
 * 5-A. tree_height：树的高度
 * ===================================================================
 * 定义要说清（不同教材不一样，这里用"节点数"口径）：
 *   空树高度 = 0；只有一个根节点 = 1；
 *   固定树高度 = 3（第 3 层有 4/5/6）。
 *
 * 代码只有一行公式：height = 1 + max(height(左), height(右))
 * 为什么能这么写？这就是递归的"信仰之跃"：
 *   假设我已经能从左右孩子那儿各拿到一个数（它们比我矮一层，天然可解），
 *   那我这一层就是"较高那边的数 + 我自己这一层"。
 *   终止条件 root == NULL → 0，保证最底层的孩子也算得出来。
 *
 * 复杂度：每个节点恰好被访问一次 → O(n)。
 */
int tree_height(const Node *root)
{
    if (root == NULL) {
        return 0;                    /* 空树高度 0：递归的地板 */
    }
    int left_h = tree_height(root->left);
    int right_h = tree_height(root->right);
    /* 谁高用谁，再加 1 是把"自己这一层"算进去 */
    return 1 + (left_h > right_h ? left_h : right_h);
}

/* ===================================================================
 * 5-B. tree_count / tree_leaf_count：节点总数、叶子数
 * ===================================================================
 * 总数：空树 0，否则 1 + 左边的数量 + 右边的数量 —— 跟高度一个模子。
 * 叶子：左右都是 NULL 才算叶子（"两张纸条都没写字"）。
 *       只有一个孩子的节点**不算**叶子，这是考试最爱扣分的地方。
 *       固定树里 4、5、6 是叶子 → 3 片。
 */
int tree_count(const Node *root)
{
    if (root == NULL) {
        return 0;
    }
    return 1 + tree_count(root->left) + tree_count(root->right);
}

int tree_leaf_count(const Node *root)
{
    if (root == NULL) {
        return 0;                    /* 空树没有叶子 */
    }
    if (root->left == NULL && root->right == NULL) {
        return 1;                    /* 自己就是叶子：算 1，不用再往下递归 */
    }
    /* 不是叶子：把左右两边各自贡献的叶子数加起来 */
    return tree_leaf_count(root->left) + tree_leaf_count(root->right);
}

/* ===================================================================
 * 5-C. tree_find：在无序树里找值
 * ===================================================================
 * 普通二叉树没有顺序，所以"值比根小就往左走"那种剪枝不存在，
 * 只能把每个节点都问一遍（最坏 O(n)）。
 * 这正是下一节 BST 要解决的问题：给树加上"左 < 根 < 右"的规矩，
 * 查找就能每层砍掉一半 → O(log n)。
 *
 * 返回节点地址而不是 1/0：这样调用者拿到节点还能读它、改它的 val。
 * 找不到返回 NULL，语义和标准库 strchr 之类一致。
 */
const Node *tree_find(const Node *root, int val)
{
    if (root == NULL) {
        return NULL;                 /* 走到空位了：全树没有（这一支没有） */
    }
    if (root->val == val) {
        return root;                 /* 命中：把自己交出去 */
    }
    const Node *found = tree_find(root->left, val);
    if (found != NULL) {
        return found;                /* 左边找到了就别再去右边翻 */
    }
    return tree_find(root->right, val);
}

/* ===================================================================
 * 5-D. tree_is_balanced：判断是否平衡
 * ===================================================================
 * 平衡的定义（每个节点都要满足，不只是根）：
 *   |height(左子树) - height(右子树)| <= 1
 *
 * 为什么要平衡？因为高度直接决定查找要走几步：
 *   n 个节点的平衡树   高度 ≈ log2(n)      → 查找 O(log n)
 *   n 个节点退化成链表 高度 = n             → 查找 O(n)
 *
 * 本实现是"朴素的 O(n log n)"写法：每判一个节点都要重算两遍高度，
 * 会有重复计算。教学场景（n ≤ 100）完全够，写起来最贴近定义。
 * 想优化成 O(n)，做法是让递归"顺便"把高度带回来看见 tree_height 的公式 +
 * 一个 balanced_out 输出参数 —— AVL 那一节会正式写。
 */
int tree_is_balanced(const Node *root)
{
    if (root == NULL) {
        return 1;                    /* 空树算平衡 */
    }

    int left_h = tree_height(root->left);
    int right_h = tree_height(root->right);
    int diff = left_h - right_h;
    if (diff < 0) {
        diff = -diff;                /* 取绝对值，避免 <stdlib.h> 里 abs 的类型纠结 */
    }
    if (diff > 1) {
        return 0;                    /* 这一个节点就已经歪了，不用再往下查 */
    }

    /* 自己这一层过了，还得左右子树内部各自也全都合格 —— && 短路求值 */
    return tree_is_balanced(root->left) && tree_is_balanced(root->right);
}

/* ===================================================================
 * 5-E. tree_is_same：两棵树是否完全相同
 * ===================================================================
 * "相同" = 形状一样 **且** 每个对应位置的值也一样。
 * 只判值不判形状是错的：1(左=2) 和 1(右=2) 值集合相同但形状不同。
 *
 * 四种入口情况的真值表：
 *   a NULL, b NULL → 1（两棵空树算相同）
 *   a NULL, b 有   → 0
 *   a 有,   b NULL → 0
 *   都有           → 值相等 且 左同 且 右同
 */
int tree_is_same(const Node *a, const Node *b)
{
    if (a == NULL && b == NULL) {
        return 1;
    }
    if (a == NULL || b == NULL) {
        return 0;                    /* 一个空一个不空：形状都不一样 */
    }
    if (a->val != b->val) {
        return 0;
    }
    return tree_is_same(a->left, b->left) && tree_is_same(a->right, b->right);
}

/* ===================================================================
 * 6. print_tree：把树横着打印出来（本章的教学支柱）
 * ===================================================================
 * 为什么横着打印？因为终端是"一行一行往下"的，树是"一层一层往宽"的，
 * 直接画正立的树要处理列对齐，代码会长到没法读。
 * 把树顺时针转 90°，就变成"根在最上、越往下越靠右缩进"的形式，
 * 一个递归 + 一个前缀字符串就能搞定：
 *
 *   想要的效果（固定树）：
 *   1                      ← 根，最左
 *   ├─2                    ← 2 是 1 的左孩子，1 下面还有兄弟，所以用 ├
 *   │ ├─4                 ← "│" 表示"上面那个分支还没画完，竖线要继续"
 *   │ └─5                 ← 5 是 2 的最后一个孩子，所以用 └，之后竖线消失
 *   └─3                    ← 3 是 1 的最后一个孩子 → └
 *     └─6                  ← 3 下面已经没有兄弟了，所以前缀是空格不是竖线
 *
 * 规则只有三条：
 *   ① 这个节点是父亲的最后一个孩子吗？是 → └─，否 → ├─
 *   ② 给"我的孩子"准备的前缀 = 我的前缀 + (我是最后一个 ? 两个空格 : "│ ")
 *   ③ 左孩子是不是最后一个，取决于"有没有右孩子"；右孩子永远是最后一个。
 *
 * 只有 const Node * 和 snprintf，不 malloc，所以这个函数不需要配套释放。
 */
static void print_tree_rec(const Node *node, const char *prefix, int is_last, int is_root)
{
    if (node == NULL) {
        return;                      /* 空孩子不占一行 */
    }

    char next_prefix[TREE_HELPER_CAP];

    if (is_root) {
        /* 根节点不带连接符，直接印在最左边 */
        printf("%d\n", node->val);
        snprintf(next_prefix, sizeof next_prefix, "%s", prefix);
    } else {
        printf("%s%s%d\n", prefix, is_last ? "└─" : "├─", node->val);
        snprintf(next_prefix, sizeof next_prefix, "%s%s",
                 prefix, is_last ? "  " : "│ ");
    }

    int left_is_last = (node->right == NULL) ? 1 : 0;  /* 没有右孩子，左就是最后一个 */
    print_tree_rec(node->left, next_prefix, left_is_last, 0);
    print_tree_rec(node->right, next_prefix, 1, 0);     /* 右孩子永远是最后一个 */
}

void print_tree(const Node *root)
{
    if (root == NULL) {
        printf("  （空树）\n");
        return;
    }
    print_tree_rec(root, "", 1, 1);
}

/* ===================================================================
 * 7. free_tree：后序释放整棵树（本节最重要的安全性知识）
 * ===================================================================
 * 规矩：谁 malloc 谁 free。一棵树 n 个节点就是 n 次 malloc，
 *       想干净退出（ASan 报 0 leak）就得 n 次 free，一个都不能少。
 *
 * 必须"先删孩子再删自己"，也就是【后序】。为什么？看内存：
 *
 *   节点 2 长这样，它的左右孩子地址存在它**自己身体里**：
 *   ┌────────┬───────────┬───────────┐
 *   │ val: 2 │ left: 0xD │ right: 0xE│
 *   └────────┴───────────┴───────────┘
 *                  ▲           ▲
 *                  │           │
 *              节点 4       节点 5
 *
 *   ❌ 如果按前序释放：
 *      free(2);                 ← 2 这块内存已经还给系统了
 *      free_tree(2->left);      ← 这一步要**读** 2->left 才能拿到 0xD
 *                                读一个已经归还的地址 = use-after-free，
 *                                值随时可能被覆盖成垃圾（甚至被别的 malloc 复用），
 *                                于是要么段错误，要么把 4、5 永久漏掉。
 *                                ASan 会立刻打印 "heap-use-after-free"。
 *
 *   ✅ 后序：
 *      free_tree(2->left);   ← 趁 2 还活着，先把 0xD 读出来，把左子树全删干净
 *      free_tree(2->right);  ← 再把右子树删干净
 *      free(2);              ← 最后才拆掉写着孩子地址的这张"纸条"
 *
 *   一句话记：**纸条上写着储物柜钥匙，你要先把柜子清空，才能扔纸条。**
 *
 * 递归版实现（注意它是"自底向上"归还，最深处先 free）：
 *   free_tree(1)
 *     free_tree(2)
 *       free_tree(4) → free(4) 已归还
 *       free_tree(5) → free(5) 已归还
 *       free(2)      已归还
 *     free_tree(3)
 *       free_tree(NULL) 什么都不做
 *       free_tree(6) → free(6)
 *       free(3)
 *     free(1)   ← 最后只剩根
 */
void free_tree(Node *root)
{
    if (root == NULL) {
        return;                      /* 空树：free(NULL) 也是安全的，这里直接返回更清楚 */
    }
    free_tree(root->left);           /* ① 先把左子树全部归还 */
    free_tree(root->right);          /* ② 再把右子树全部归还 */
    free(root);                      /* ③ 最后归还自己：此时没人再靠 root 找孩子了 */
    /* 小提醒：调用者手里那个指向根的指针（比如 main 里的 root）现在成了悬空指针，
     * 用完记得 root = NULL，别再拿它去 print_tree。 */
}
