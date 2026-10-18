#include <stdio.h>
#include <stdlib.h>
#include <string.h>   /* strlen */
#include "pqueue.h"

/*
 * ===================================================================
 * 本文件分两层：
 *   第 1 层（下面这 60 行左右）= 一个【极简二叉堆】，元素类型写死成 Task。
 *   第 2 层 = 优先队列的 4 个对外接口，只是把堆的操作换了个名字。
 *
 * 为什么要在自己目录里重写一遍堆？
 *   本项目的硬约定是"叶子目录必须自包含"，Makefile 按目录独立编译，
 *   所以这里不能去 #include 隔壁 binary-heap 的东西。
 *   换个角度想这其实是好事：堆的骨架其实就只有【上浮】和【下沉】两件事，
 *   能默写出来才算真会 —— 你刚刚在 binary-heap 看过一遍，现在再写一遍。
 *
 * 堆的三条下标公式（0 起下标）：
 *     父(i) = (i-1)/2    左(i) = 2i+1    右(i) = 2i+2
 * 一句话记法：堆就是一棵被压扁成数组的完全二叉树。
 * ===================================================================
 */

/* ---- 下标公式：和 binary-heap 里一模一样的三条 ---- */
static int parent_of(int i) { return (i - 1) / 2; }   /* i >= 1 才有父亲；i==0 是根 */
static int left_of(int i)   { return 2 * i + 1; }
static int right_of(int i)  { return 2 * i + 2; }

/*
 * ===================================================================
 * 唯一的比较点：a 比 b 更该排在前面吗？
 * ===================================================================
 * 优先队列语义：priority 数字越【小】越紧急 → 小顶堆。
 *
 * ⚠ 注意这里【只比 priority】，完全不看 id。
 *   这一行就是"优先级相同的任务谁先走没保证"的根源 ——
 *   两个 priority 相等的任务，在这里 a<b 和 b<a 都是 0（谁也不比谁优先），
 *   于是堆在它们之间不做任何交换，最终顺序完全由"数组里谁被搬到哪"决定，
 *   而那个搬运规则（末尾补顶 + 下沉）跟到达顺序毫无关系。
 *   main.c 第 5 幕会把这条坑跑给你看。
 *
 * 如果要稳定（先来先服务），这里改成双键即可（本项目【不】实现，只给写法）：
 *     if (a.priority != b.priority) return a.priority < b.priority;
 *     return a.seq < b.seq;            // seq 是入队时发的自增序号
 * 再加一个"入队时 q->enqueued++ 写进 Task.seq"，两行代码，稳定性立刻到手。
 */
static int wins(const Task *a, const Task *b)
{
    return a->priority < b->priority;
}

/* ---- 打印：把队列当前的数组状态打出来（学习期每一步都要看得见） ---- */
static void dump(const PQueue *q, const char *note)
{
    if (!q->verbose) {
        return;
    }
    printf("        %s: [", note);
    for (int k = 0; k < q->size; k++) {
        printf(" p%d(#%d)", q->data[k].priority, q->data[k].id);
    }
    printf(" ]\n");
}

/* ---- 交换：堆里所有的"结构变化"其实只有这一种动作 ---- */
static void swap_slots(PQueue *q, int a, int b, const char *why)
{
    Task tmp = q->data[a];
    q->data[a] = q->data[b];
    q->data[b] = tmp;
    q->swaps++;                       /* 让"O(log n)"变成一个能看见的数字 */

    if (q->verbose) {
        printf("      %s: 下标 %d <-> %d（p%d(#%d) <-> p%d(#%d)）\n", why, a, b,
               q->data[a].priority, q->data[a].id,
               q->data[b].priority, q->data[b].id);
        dump(q, "交换后");
    }
}

/*
 * ===================================================================
 * 上浮 sift_up：入队后用。新元素在数组末尾，往爬到自己的位置。
 * ===================================================================
 * 每轮和父亲比，比父亲更紧急就上，否则停。
 * i 每轮至少减半（(i-1)/2 <= i/2）→ 最多 log2(n) 轮。
 */
static void sift_up(PQueue *q, int i)
{
    while (i > 0) {
        int p = parent_of(i);
        if (!wins(&q->data[i], &q->data[p])) {
            if (q->verbose) {
                printf("      停：p%d(#%d) 不比父亲 p%d(#%d) 更紧急\n",
                       q->data[i].priority, q->data[i].id,
                       q->data[p].priority, q->data[p].id);
            }
            return;
        }
        swap_slots(q, i, p, "上浮");
        i = p;
    }
    if (q->verbose) {
        printf("      停：已经爬到队首（下标 0）\n");
    }
}

/*
 * ===================================================================
 * 下沉 sift_down：出队后用。堆顶那个大概率不紧急，往下沉。
 * ===================================================================
 * 关键：和【两个孩子里更紧急的那个】换，不能只跟左孩子换。
 *   反例：父 p5，左 p1，右 p2。跟右(p2)换 → 队首变 p2、左边还留着 p1 → p2 > p1 违规 ❌
 *        跟左(p1)换 → 队首 p1、左右变 p5/p2 → 这一层合法 ✅
 *   道理：换上来的孩子必须 <= 另一个孩子，挑最小的那个自动满足。
 */
static void sift_down(PQueue *q, int i)
{
    for (;;) {
        int l = left_of(i);
        int r = right_of(i);
        if (l >= q->size) {
            return;                   /* 叶子，没有孩子可换 */
        }

        int best = l;                 /* 先假设左孩子最紧急 */
        if (r < q->size && wins(&q->data[r], &q->data[l])) {
            best = r;                 /* 右孩子存在且更紧急 → 换成右 */
        }
        if (!wins(&q->data[best], &q->data[i])) {
            if (q->verbose) {
                printf("      停：p%d(#%d) 已经是自己和孩子里最紧急的\n",
                       q->data[i].priority, q->data[i].id);
            }
            return;
        }
        swap_slots(q, i, best, "下沉");
        i = best;
    }
}

/*
 * ===================================================================
 * 生 / 死
 * ===================================================================
 * PQueue 里只有 data 一块数组要还，所以 pq_free 里两次 free 就干净了。
 * 约定：pq_free(q) 之后调用者要把 q 置 NULL（这里改不到外面的指针）。
 */
PQueue *pq_new(int initial_capacity)
{
    if (initial_capacity < 1) {
        initial_capacity = 1;
    }
    PQueue *q = malloc(sizeof(PQueue));
    if (q == NULL) {
        return NULL;
    }
    q->data = malloc((size_t)initial_capacity * sizeof(Task));
    if (q->data == NULL) {
        free(q);                      /* 第二次 malloc 失败，第一次要还掉 */
        return NULL;
    }
    q->size = 0;
    q->capacity = initial_capacity;
    q->verbose = 1;
    q->enqueued = 0;
    q->swaps = 0;
    return q;
}

void pq_free(PQueue *q)
{
    if (q == NULL) {
        return;
    }
    free(q->data);
    free(q);
}

/* 扩容：翻倍。为什么翻倍？摊还分析 —— n 次入队总共搬 1+2+4+…+n < 2n 个，
 * 平均每次 O(1)；每次只加 1 格就退化成整体 O(n²)。
 * ⚠ realloc 的返回值必须用变量接住再赋回 q->data，否则内存搬家后就是悬空指针。 */
static int grow(PQueue *q)
{
    int new_cap = q->capacity * 2;
    Task *fresh = realloc(q->data, (size_t)new_cap * sizeof(Task));
    if (fresh == NULL) {
        return -1;
    }
    q->data = fresh;
    q->capacity = new_cap;
    if (q->verbose) {
        printf("      [扩容] 数组 2 倍扩到能装 %d 个任务\n", new_cap);
    }
    return 0;
}

/*
 * ===================================================================
 * 对外接口 1：pq_enqueue —— 入队
 * ===================================================================
 * 就两步：接到尾部（保住完全二叉树的形状）+ 上浮（修唯一那处违例）。
 * 顺序不能反：先 size++ 再 sift_up，否则 sift_up 的边界判断看不见新元素。
 */
int pq_enqueue(PQueue *q, int priority, int id)
{
    if (q == NULL) {
        return -1;
    }
    if (q->size == q->capacity && grow(q) != 0) {
        return -1;
    }

    Task t;                    /* 先在栈上凑一个任务，再整个塞进数组 */
    t.priority = priority;
    t.id = id;

    int at = q->size;
    q->data[at] = t;
    q->size++;
    q->enqueued++;

    if (q->verbose) {
        printf("  enqueue(优先级 %d, 任务 #%d) → 先站到队尾（下标 %d）\n", priority, id, at);
        dump(q, "入队后");
    }
    sift_up(q, at);
    if (q->verbose) {
        dump(q, "上浮结束");
    }
    return 0;
}

/*
 * ===================================================================
 * 对外接口 2：pq_dequeue —— 出队最优先
 * ===================================================================
 * 三步，和堆的 pop 完全一样：
 *   ① 抄下答案 data[0]
 *   ② 和【最后一个】交换 → size--   （为什么必须末尾：只有末尾是叶子，
 *      搬走它不会有节点失去父亲，数组下标仍然连续，形状不坏，且只花 O(1)。
 *      "删队首再把后面整体前移"结构其实没坏，但每次 O(n)，队列操作全退化）
 *   ③ 新队首下沉
 */
int pq_dequeue(PQueue *q, int *priority_out, int *id_out)
{
    if (q == NULL || q->size == 0) {
        if (q != NULL && q->verbose) {
            printf("  dequeue 失败：队列已空\n");
        }
        return -1;
    }

    Task top = q->data[0];          /* ① 先抄答案，否则 ② 会把它盖掉 */
    int last = q->size - 1;

    if (q->verbose) {
        printf("  dequeue → 队首 p%d(#%d) 被服务掉。", top.priority, top.id);
        if (last > 0) {
            printf("它要和队尾下标 %d 的 p%d(#%d) 交换\n",
                   last, q->data[last].priority, q->data[last].id);
        } else {
            printf("队列里只剩它一个，直接清空\n");
        }
    }

    if (last > 0) {
        swap_slots(q, 0, last, "出队第一步");
    }
    q->size--;
    if (q->verbose) {
        dump(q, "缩容后");
    }

    if (q->size > 0) {
        sift_down(q, 0);
    }
    if (q->verbose) {
        dump(q, "下沉结束");
    }

    if (priority_out != NULL) {
        *priority_out = top.priority;
    }
    if (id_out != NULL) {
        *id_out = top.id;
    }
    return 0;
}

/*
 * ===================================================================
 * 对外接口 3 / 4：peek 与 size
 * ===================================================================
 * peek 是唯一"不改结构"的操作，O(1)。这也是优先队列对外的全部信息量：
 * 它只知道"谁最优先"，不知道"谁第二优先"。
 */
int pq_peek(const PQueue *q, int *priority_out, int *id_out)
{
    if (q == NULL || q->size == 0) {
        return -1;
    }
    if (priority_out != NULL) {
        *priority_out = q->data[0].priority;
    }
    if (id_out != NULL) {
        *id_out = q->data[0].id;
    }
    return 0;
}

int pq_size(const PQueue *q)  { return q == NULL ? 0 : q->size; }
int pq_empty(const PQueue *q) { return (q == NULL || q->size == 0) ? 1 : 0; }

/* 累计交换次数：用来把"O(log n)"变成一个看得见的数字（main.c 第 6 幕要用）。 */
long pq_swaps(const PQueue *q)
{
    return q == NULL ? 0 : q->swaps;
}

void pq_reset_swaps(PQueue *q)
{
    if (q != NULL) {
        q->swaps = 0;
    }
}

/*
 * ===================================================================
 * pq_set_verbose：学习期开着，跑大数据时关掉
 * ===================================================================
 */
void pq_set_verbose(PQueue *q, int on)
{
    if (q != NULL) {
        q->verbose = on ? 1 : 0;
    }
}

/*
 * ===================================================================
 * pq_print：把队列画成一棵树，并同时给出数组形式
 * ===================================================================
 * 元素写成 p<优先级>(#<任务号>)。看这棵树只要记住一件事：
 *   父亲一定不比孩子"更晚被服务"。兄弟之间谁先谁后，树不管。
 */
void pq_print(const PQueue *q)
{
    if (q == NULL) {
        printf("  (队列指针 NULL)\n");
        return;
    }
    if (q->size == 0) {
        printf("  (空队列，size=0)\n");
        return;
    }

    int total = q->size;
    int levels = 0;
    while ((1 << levels) <= total) {
        levels++;
    }

    if (total > 31) {
        printf("  人数 %d，树太宽就不画了，给前 20 个：\n    ", total);
        int show = total < 20 ? total : 20;
        for (int k = 0; k < show; k++) {
            printf(" p%d(#%d)", q->data[k].priority, q->data[k].id);
        }
        printf("%s\n", show < total ? " ..." : "");
        return;
    }

    printf("  树形（父不比子更晚服务；括号里是数组下标）\n");
    for (int d = 0; d < levels; d++) {
        int start = (1 << d) - 1;
        int end = (1 << (d + 1)) - 2;
        if (end > total - 1) {
            end = total - 1;
        }
        char gutter[48];
        snprintf(gutter, sizeof gutter, "  第%d层 [%d..%d] |", d, start, end);
        printf("%-20s", gutter);

        int half = 6 * (1 << (levels - 1 - d));
        int cur_col = 0;
        for (int idx = start; idx <= end; idx++) {
            char label[48];
            snprintf(label, sizeof label, "p%d(#%d)[%d]",
                     q->data[idx].priority, q->data[idx].id, idx);
            int len = (int)strlen(label);
            int target = (2 * (idx - start) + 1) * half - len / 2;
            if (target < cur_col) {
                target = cur_col;
            }
            for (int s = cur_col; s < target; s++) {
                putchar(' ');
            }
            fputs(label, stdout);
            cur_col = target + len;
        }
        printf("\n");
    }

    printf("  数组形式:");
    for (int k = 0; k < total; k++) {
        printf(" %d", q->data[k].priority);
    }
    printf("   （优先级序列；下标 0 就是下一个被服务的）\n");
}
