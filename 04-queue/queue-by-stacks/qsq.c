/*
 * ===================================================================
 * qsq.c —— 两个栈拼队列：全部实现
 * ===================================================================
 * 整个文件的核心就是 q_dequeue 里那几行：out 空了就倒，倒完再弹。
 * 其余都是记账、可视化和边界保护。
 *
 * ★ 本文件对两个栈只使用 istk_push / istk_pop / istk_top / istk_empty /
 *   istk_size / istk_at（最后一个是只读访问器，只给画图用）。
 *   如果哪天这里出现 `q->in.data[i] = ...`，那"队列行为只来自两个栈"这句话就名存实亡了。
 */

#include "qsq.h"

#include <stdio.h>
#include <stddef.h>

/* ==================================================================
 * 生命周期
 * ================================================================== */
void q_init(TwoStackQueue *q)
{
    if (q == NULL) {
        return;
    }
    istk_init(&q->in);
    istk_init(&q->out);
    q->enqueue_ops  = 0;
    q->dequeue_ops  = 0;
    q->dequeue_fail = 0;
    q->pour_ops     = 0;
    q->move_count   = 0;
    q->max_one_pour = 0;
    q->max_one_deq  = 0;
}

void q_destroy(TwoStackQueue *q)
{
    if (q == NULL) {
        return;                     /* 幂等：destroy 两次不炸 */
    }
    istk_destroy(&q->in);
    istk_destroy(&q->out);
    q->enqueue_ops  = 0;
    q->dequeue_ops  = 0;
    q->dequeue_fail = 0;
    q->pour_ops     = 0;
    q->move_count   = 0;
    q->max_one_pour = 0;
    q->max_one_deq  = 0;
}

void q_reset_counters(TwoStackQueue *q)
{
    if (q == NULL) {
        return;
    }
    /* 只清队列这一层的账本；两个栈自己的 n_push / n_pop 保留（它们是"这个栈被用过多少次"
     * 的历史记录，对比实验里不想伪造它）。要连内容一起清就 q_destroy + q_init。 */
    q->enqueue_ops  = 0;
    q->dequeue_ops  = 0;
    q->dequeue_fail = 0;
    q->pour_ops     = 0;
    q->move_count   = 0;
    q->max_one_pour = 0;
    q->max_one_deq  = 0;
}

/* ==================================================================
 * 内部工具：整栈倒过去
 * ================================================================== */

/*
 * 把 from 栈**全部**弹出来压进 to 栈 —— 这就是"倒"。
 *
 * 顺序为什么反过来：栈只能从顶上拿，所以弹出的顺序是"新 → 旧"，
 * 压到对面去之后"新"先落底、"旧"后落，于是"旧"停在 to 栈的栈顶。
 * 一次搬运 = 一次 pop + 一次 push，记 move_count += 1。
 *
 * 返回真正搬了多少个。中途 to 栈装满就停（宁可少搬，也不能越界写）。
 */
static int pour_stack(IntStack *from, IntStack *to, TwoStackQueue *q)
{
    int moved = 0;

    while (!istk_empty(from)) {
        int v;
        if (!istk_pop(from, &v)) {
            break;                  /* 理论上到不了这里：!empty 之后 pop 一定成功 */
        }
        if (!istk_push(to, v)) {
            /*
             * 目标栈放不下 —— 出事了：元素已经从 from 弹出，却没能塞进 to。
             * 直接 break 会**把这个元素丢掉**（它已经不在任何一个栈里了），
             * 所以必须原样塞回 from 再走，让调用方从"返回值 < 原来的个数"看出没倒干净。
             * 这手"半途失败要还原"是固定容量数组栈必须付的税
             * （工程版用链表栈就没这个分支，见 README 的「常见的坑」第 6 条）。
             */
            (void) istk_push(from, v);
            break;
        }
        moved++;
        if (q != NULL) {
            q->move_count++;        /* ★ 均摊分析要数的就是这个 1 */
        }
    }
    return moved;
}

/* 记一次倒栈的账（次数 + 一趟搬家的峰值），三个调用点都要，抽出来免得写漏。 */
static void note_pour(TwoStackQueue *q, int moved)
{
    if (moved <= 0) {
        return;
    }
    q->pour_ops++;
    if ((long) moved > q->max_one_pour) {
        q->max_one_pour = moved;
    }
}

/*
 * 记一次【出队】的真实代价：这次调用从头到尾一共搬了多少个。
 * before 是进函数时的 move_count。
 * ★ 为什么需要它而不直接用 max_one_pour：反面版一次出队倒两趟
 *   （过去 k 个、回来 k-1 个），max_one_pour 只会报其中较大的一趟 = k，
 *   而真实代价是 2k-1。均摊论证要的是后者。
 */
static void note_deq_cost(TwoStackQueue *q, long before)
{
    long moved = q->move_count - before;
    if (moved > q->max_one_deq) {
        q->max_one_deq = moved;     /* 最坏一次出队的真实代价，反例对比表用它说话 */
    }
}

/* ==================================================================
 * 队列的四个动作
 * ================================================================== */
bool q_enqueue(TwoStackQueue *q, int value)
{
    if (q == NULL) {
        return false;
    }
    q->enqueue_ops++;

    /*
     * 进队**永远**只压 in 栈：一次 push，最坏情况也是 O(1)（注意，这里不是均摊，是每次）。
     * ★ 绝对不要顺手压进 out 栈：out 栈里躺着的是"比 in 更早"的那批（已经反过序了），
     *   新元素压到它们上面，就会比它们先出队 —— 顺序当场乱掉。
     */
    return istk_push(&q->in, value);
}

bool q_dequeue(TwoStackQueue *q, int *out)
{
    if (q == NULL || out == NULL) {
        return false;
    }
    q->dequeue_ops++;
    long before = q->move_count;        /* 本次出队的代价从这一刻开始算 */

    /*
     * ★★ 本节的心脏，就这两句：
     *   out 空 → 把 in 整个倒过来 → out 的栈顶就是最早进队的那个。
     * 判空的对象必须是 out，不是"整个队列"：
     *   out 非空时，哪怕 in 里有一万个元素也**不能倒**（见 qsq.h 那条纪律）。
     */
    if (istk_empty(&q->out)) {
        if (istk_empty(&q->in)) {
            q->dequeue_fail++;      /* 两个栈都空 = 队列真的空了 */
            return false;           /* ★ 此时 *out 一个字都不改（本次搬运 0，不用记账）*/
        }
        note_pour(q, pour_stack(&q->in, &q->out, q));
    }

    {
        bool ok = istk_pop(&q->out, out);
        note_deq_cost(q, before);   /* 这一次出队到底搬了几个，实测进账本 */
        return ok;
    }
}

bool q_peek(TwoStackQueue *q, int *out)
{
    if (q == NULL || out == NULL) {
        return false;
    }
    /*
     * peek 的麻烦在于：队头可能在 in 栈的**栈底**，而栈只能看栈顶。
     * 唯一的正规解法就是"先倒过来再看" —— 所以 peek 是有副作用的，
     * 这就是本函数签名不带 const 的原因（见 qsq.h，README「常见的坑」第 5 条）。
     * 倒完之后队头一定在 out 栈顶，直接 top 即可；
     * 千万别"pop 出来看完再压回去" —— 那是白给两次搬运。
     */
    if (istk_empty(&q->out)) {
        if (istk_empty(&q->in)) {
            return false;
        }
        note_pour(q, pour_stack(&q->in, &q->out, q));
    }
    return istk_top(&q->out, out);
}

bool q_empty(const TwoStackQueue *q)
{
    if (q == NULL) {
        return true;
    }
    /* ★ 两个栈都要判。只判 in 会漏掉"已经倒过去但还没出队"的那批 —— 这是最常见的写法错误。 */
    return istk_empty(&q->in) && istk_empty(&q->out);
}

int q_size(const TwoStackQueue *q)
{
    if (q == NULL) {
        return 0;
    }
    return istk_size(&q->in) + istk_size(&q->out);
}

/* ==================================================================
 * 反面教材：每次出队都倒过去、再倒回来
 * ================================================================== */
bool q_dequeue_naive(TwoStackQueue *q, int *out)
{
    if (q == NULL || out == NULL) {
        return false;
    }
    q->dequeue_ops++;
    long before = q->move_count;        /* ★ 两趟搬家都要记进同一次出队的账 */

    /* ① 把 in 整栈倒进 out：搬 k 次 */
    note_pour(q, pour_stack(&q->in, &q->out, q));

    int head;
    if (!istk_pop(&q->out, &head)) {
        q->dequeue_fail++;
        note_deq_cost(q, before);
        return false;               /* 两个栈本来就都空 */
    }

    /* ② ★ 灾难就在这一步：out 里剩下的又全部倒回 in，再搬 k-1 次。
     *    倒过去反转一次、倒回来再反转一次 → in 里的相对顺序回到原样，
     *    所以**结果完全正确**，只是单次出队付了 2k-1 次搬运 = O(n)。
     *    这正是要让它跑在命令行开关后面的原因：它对输出结果一点错都没有，
     *    只看单元测试永远发现不了这种代码。 */
    note_pour(q, pour_stack(&q->out, &q->in, q));

    note_deq_cost(q, before);       /* 这里记到的是 2k-1，而 max_one_pour 只记到 k */
    *out = head;
    return true;
}

/* ==================================================================
 * 可视化：把两个栈并排竖着画出来
 * ================================================================== */

/*
 * 把某一格的内容排成 8 个字符宽。
 * ★ 格子里只放 ASCII（数字或 --），中文一律留到行尾：
 *   中英混排时一个汉字占两列，把汉字放进框里就会把右边的框挤歪 —— 这是本项目画图的第一纪律。
 * row = 0 → 栈顶，往下每行 +1。返回 true 表示这一格真的住着元素。
 */
static bool cell_of(const IntStack *st, int row, char *buf, size_t sz)
{
    int size = istk_size(st);
    int idx  = size - 1 - row;                  /* 从栈顶往下数第 row 格 = 下标 size-1-row */

    if (idx >= 0) {
        int v = 0;
        if (istk_at(st, idx, &v)) {
            snprintf(buf, sz, "%8d", v);
            return true;
        }
    }
    /* 空栈的那一行画 --；比对面矮的那几行画空白（但格子还画出来，让两个框一样高）*/
    snprintf(buf, sz, "%8s", (row == 0) ? "--" : "");
    return false;
}

void q_dump(const TwoStackQueue *q, const char *op)
{
    if (q == NULL) {
        return;
    }

    int n_in  = istk_size(&q->in);
    int n_out = istk_size(&q->out);
    int hi    = (n_in > n_out) ? n_in : n_out;
    if (hi == 0) {
        hi = 1;                                 /* 两个都空也要画一行 -- */
    }

    if (op != NULL) {
        printf("  %s\n", op);
    }
    printf("        in(进队侧)          out(出队侧)\n");
    printf("        ┌────────┐          ┌────────┐\n");

    for (int row = 0; row < hi; row++) {
        char l[10];
        char r[10];
        bool hl = cell_of(&q->in,  row, l, sizeof l);
        bool hr = cell_of(&q->out, row, r, sizeof r);

        printf("        │%s│          │%s│", l, r);

        /* ---- 行尾注释：只在关键行出现，多了就成噪音 ----
         * hl / hr 是这一行左右两格"到底住着元素没有"，
         * 用它俩判断比用长度判断更直接：空白格和元素格一眼分开。 */
        if (n_in == 0 && n_out == 0) {
            printf("   ← 两个栈都空：这就是空队列");
        } else if (row == 0) {
            printf("   ← 这一行是两个栈各自的【栈顶】");
        } else if (!hl && hr) {
            printf("   ← in 到上一行就见底了，这一格是空白；out 这里还有元素");
        } else if (hl && !hr) {
            printf("   ← out 到上一行就见底了，这一格是空白；in 这里还有元素");
        } else if (row == n_in - 1 && row == n_out - 1) {
            printf("   ← 两列同时到底：这一行是各自的【栈底】");
        } else if (row == n_in - 1) {
            printf("   ← in 栈底 = 这批里最早进队的那个");
        } else if (row == n_out - 1) {
            printf("   ← out 栈底 = 倒进来时最后落下的那个");
        }
        printf("\n");

        if (row + 1 < hi) {
            printf("        ├────────┤          ├────────┤\n");
        }
    }
    printf("        └────────┘          └────────┘\n");
    printf("        in 有 %d 个，out 有 %d 个\n", n_in, n_out);

    /* 再补一行"逻辑队列视角"：队头 → 队尾。这是检验图画对没有的最快办法：
     * 队头 = out 栈顶往下依次 + in 栈底往上依次。 */
    printf("        队列里（队头→队尾）：");
    int shown = 0;
    for (int row = 0; row < n_out; row++) {
        char b[10];
        (void) cell_of(&q->out, row, b, sizeof b);
        printf("%s", b);
        shown++;
    }
    for (int idx = 0; idx < n_in; idx++) {
        char b[10];
        (void) cell_of(&q->in, n_in - 1 - idx, b, sizeof b);   /* 栈底 → 栈顶 = 旧 → 新 */
        printf("%s", b);
        shown++;
    }
    if (shown == 0) {
        printf("（空队列）");
    }
    printf("\n");
    printf("        计数：入队 %ld 次，出队 %ld 次，倒栈 %ld 次，累计搬运 %ld 次\n\n",
           q->enqueue_ops, q->dequeue_ops, q->pour_ops, q->move_count);
}
