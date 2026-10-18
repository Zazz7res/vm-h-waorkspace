/*
 * ===================================================================
 * main.c —— 用两个栈实现队列：十幕演示
 * ===================================================================
 * 每一幕只讲一件事，顺序就是学习顺序：
 *   1  栈和队列规矩相反（先把 LIFO 摆出来）
 *   2  本节自带的极简数组栈：四个动作 + 一次容量撞墙
 *   3  核心动作『倒』：为什么倒一次顺序就反过来了
 *   4  完整场景：每一步都把两个栈画出来（本节主菜）
 *   5  peek / empty / size：peek 是有副作用的
 *   6  均摊分析：用计数器实测『搬运次数 = 出队次数』
 *   7  反面教材的预告（真正的那两段代码挂在 --bug 开关后面）
 *   8  边界：空队列、容量满、destroy 之后再用、元素值域
 *   9  反过来：用两个队列实现栈（思路 + 对照，代码在 README）
 *   10 收尾：几个 init 几个 destroy
 *
 * 编译（本目录自包含，三个 .c 一起编就行）：
 *   gcc -Wall -Wextra -Wshadow -std=c11 -g -fsanitize=address -static-libasan \
 *       ./istack.c ./qsq.c ./main.c -o /tmp/q_by_stacks
 * 运行：
 *   /tmp/q_by_stacks                默认：10 幕正常演示（不跑任何反例）
 *   /tmp/q_by_stacks --bug=naive    反面教材一：每次出队都倒过去再倒回来
 *   /tmp/q_by_stacks --bug=mix      反面教材二：out 没空就把 in 倒进来（顺序错乱）
 */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "istack.h"
#include "qsq.h"

static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/*
 * 出队一步 + 把两个栈画出来，旁白由【实测】决定，不靠人肉猜。
 * 返回值：这次真的取到的那个数（失败时返回 -1，仅用于攒序列）。
 *
 * 为什么要专门实测『这一次搬了几个』：
 *   旁白如果写成 if (i == 0) 就说『这里倒了一次栈』，那么它必须和真实行为
 *   严格对齐；一旦对齐错一格，读者看到的解释就和图上显示的数字互相打脸
 *   ——而这种『讲解比代码错后半拍』是演示类代码里最难发现的 bug，因为它读起来很顺。
 *   搬运计数器（move_count）的差值就是铁证：搬了 0 个 → out 当时非空，直接弹；
 *   搬了 k>0 个 → out 当时是空的，先把 in 倒了 k 个进来。
 */
static int deq_step(TwoStackQueue *q, const char *tag)
{
    long before = q->move_count;        /* 进函数前的账本 */
    int  got = -1;
    bool ok = q_dequeue(q, &got);
    long moved = q->move_count - before;

    char label[200];
    if (!ok) {
        snprintf(label, sizeof label,
                 "%s q_dequeue() → false（两个栈都空）· 本次搬运 %ld 个", tag, moved);
    } else if (moved > 0) {
        snprintf(label, sizeof label,
                 "%s q_dequeue() → true，取到 %d ← out 本来是空的："
                 "先把 in 整个倒过来（本次搬 %ld 个），再弹 out 栈顶", tag, got, moved);
    } else {
        snprintf(label, sizeof label,
                 "%s q_dequeue() → true，取到 %d ← out 本来就有货："
                 "直接弹栈顶，本次一个都不搬", tag, got);
    }
    q_dump(q, label);
    return got;
}

/* ==================================================================
 * 反例一：每次出队都『倒过去 + 倒回来』（结果正确，复杂度灾难）
 * 挂在 --bug=naive 后面，默认一次都不跑。
 * ================================================================== */
static void bug_demo_naive(void)
{
    section("[反例一] 每次出队都倒过去再倒回来：结果全对，代价 O(n)/次");

    printf("  两个队列跑【完全相同】的一串操作，只有出队实现不同：\n");
    printf("    正确版 q_dequeue()       ：out 空了才倒一次，每个元素一辈子只搬一次\n");
    printf("    反例版 q_dequeue_naive() ：每次出队先把 in 倒进 out，取一个，再把剩下的倒回 in\n\n");

    TwoStackQueue good, bad;
    int got_good[32], got_bad[32];
    int ng = 0, nb = 0;

    q_init(&good);
    q_init(&bad);

    /* 一段『人话脚本』：正数 = 入队这个值，-1 = 出队一次。
     * 队列只有 9 个人，脚本里却有 12 次出队 → 最后三次必然失败，
     * 失败正好用来验证反例版的空队列判断（它的 *out 同样不被污染）。 */
    static const int script[18] = {
        1, 2, 3, 4, 5, 6, -1, 7, -1, -1, 8, 9, -1, -1, -1, -1, -1, -1
    };

    for (int i = 0; i < 18; i++) {
        int v = script[i];
        int tmp = 0;
        if (v > 0) {
            (void) q_enqueue(&good, v);
            (void) q_enqueue(&bad, v);
        } else {
            if (q_dequeue(&good, &tmp)) {
                got_good[ng++] = tmp;
            }
            if (q_dequeue_naive(&bad, &tmp)) {
                got_bad[nb++] = tmp;
            }
        }
    }

    bool same = (ng == nb);
    for (int i = 0; same && i < ng; i++) {
        same = (got_good[i] == got_bad[i]);
    }
    printf("  ① 两边的【出队序列】一致吗？ %s\n",
           same ? "完全一致 ← 这就是它难发现的原因" : "不一致");
    if (ng == 0 || nb == 0) {
        printf("     （一次都没出队成功，序列无从对比 —— 脚本改坏了）\n");
    } else {
        printf("     正确版：%d", got_good[0]);
        for (int i = 1; i < ng; i++) {
            printf(" %d", got_good[i]);
        }
        printf("\n     反例版：%d", got_bad[0]);
        for (int i = 1; i < nb; i++) {
            printf(" %d", got_bad[i]);
        }
        printf("\n     两边各成功出队 %d / %d 次，失败 %ld / %ld 次（失败的就是空队列那几次），\n",
               ng, nb, good.dequeue_fail, bad.dequeue_fail);
        printf("     顺序完全一样 → 这种性能 bug 单元测试永远查不出来。\n");
    }

    /* ★ 表头/数据行对齐的唯一办法：把 CJK 的显示宽度当成 2 列来手工配。
     *   "正确版" = 3 个汉字 = 6 列，所以表头 "实现" 后面补 3 个空格，两列才对得上。
     *   【单次最大搬运】这一列用的是 max_one_deq（一次出队的总搬运），
     *   不是 max_one_pour（一趟倒栈的峰值）：反例版一次出队倒两趟，
     *   用后者会说出 64 而真实代价是 127 —— 数字错了，整段论证就废了。 */
    printf("\n  ② 但【搬运次数】是两回事：\n");
    printf("     实现   出队次数   搬运次数  单次最大搬运     均摊(搬运/出队)\n");
    printf("     %s %-9ld  %-9ld  %-12ld %.2f\n", "正确版",
           good.dequeue_ops, good.move_count, good.max_one_deq,
           good.dequeue_ops ? (double) good.move_count / (double) good.dequeue_ops : 0.0);
    printf("     %s %-9ld  %-9ld  %-12ld %.2f\n", "反例版",
           bad.dequeue_ops, bad.move_count, bad.max_one_deq,
           bad.dequeue_ops ? (double) bad.move_count / (double) bad.dequeue_ops : 0.0);
    printf("     对照：一趟倒栈的峰值 max_one_pour = 正确版 %ld / 反例版 %ld（反例版一次出队要倒两趟）\n",
           good.max_one_pour, bad.max_one_pour);
    printf("     搬运次数之比 = %.1f 倍（同一串操作、同一个结果）\n",
           good.move_count ? (double) bad.move_count / (double) good.move_count : 0.0);

    printf("\n  ③ 规模上去再看：狂灌 %d 个，再一次性出队 %d 个\n", ISTK_CAP, ISTK_CAP);
    TwoStackQueue big_good, big_bad;
    q_init(&big_good);
    q_init(&big_bad);
    /* 先把两个队列都灌满 ISTK_CAP 个，再开始出队 —— 不灌就没有东西可搬，
     * 下面那几句"搬 0 次 / 64^2"会全部变成空话（这里曾经就漏过这一轮）。*/
    for (int i = 0; i < ISTK_CAP; i++) {
        (void) q_enqueue(&big_good, i);
        (void) q_enqueue(&big_bad, i);
    }
    long first_g = 0, first_b = 0;
    long zero_g = 0, zero_b = 0;          /* 本次搬运 0 个的出队有多少次 */
    long max_g = 0, max_b = 0;            /* 单次出队的最大搬运数（实测，不查表）*/
    for (int i = 0; i < ISTK_CAP; i++) {
        int tmp = 0;
        long before_g = big_good.move_count;
        long before_b = big_bad.move_count;
        (void) q_dequeue(&big_good, &tmp);
        (void) q_dequeue_naive(&big_bad, &tmp);
        long mv_g = big_good.move_count - before_g;
        long mv_b = big_bad.move_count - before_b;
        if (i == 0) {
            first_g = mv_g;
            first_b = mv_b;
        }
        if (mv_g == 0) zero_g++;
        if (mv_b == 0) zero_b++;
        if (mv_g > max_g) max_g = mv_g;
        if (mv_b > max_b) max_b = mv_b;
    }
    printf("     %s 总搬运 %4ld 次｜第一次出队搬 %3ld 个｜搬 0 个的出队 %2ld 次｜单次最大 %3ld 个\n",
           "正确版", big_good.move_count, first_g, zero_g, max_g);
    printf("     %s 总搬运 %4ld 次｜第一次出队搬 %3ld 个｜搬 0 个的出队 %2ld 次｜单次最大 %3ld 个\n",
           "反例版", big_bad.move_count, first_b, zero_b, max_b);
    printf("     同样是 %d 次出队：正确版搬 %ld 次（= 出队次数 → 均摊 O(1)）；\n",
           ISTK_CAP, big_good.move_count);
    printf("                    反例版搬 %ld 次 = %d^2 → 单次 O(n)，n 次就是 O(n^2)\n",
           big_bad.move_count, ISTK_CAP);
    printf("     → 结论：两栈队列的全部价值就在那句『out 空了才倒』上。\n");
    printf("       少了这个判空，它就退化成『每次出队都把全队重排一遍』的昂贵把戏。\n");

    q_destroy(&big_good);
    q_destroy(&big_bad);
    q_destroy(&good);
    q_destroy(&bad);
}

/* ==================================================================
 * 反例二：out 栈还没空就把 in 倒进来（正确性 bug，不是性能 bug）
 * 挂在 --bug=mix 后面。
 * ================================================================== */
static void bug_demo_mix(void)
{
    section("[反例二] out 没空就把 in 倒进来：不崩、不报错，只是顺序错了");

    printf("  这种写法在真实代码里长这样：作者觉得『每次出队前保证 out 是满的，性能一定更好』。\n");
    printf("  下面照着那个思路手动倒一次（正式代码里那句判空被删掉了）：\n\n");

    TwoStackQueue q;
    q_init(&q);
    int out = 0;

    (void) q_enqueue(&q, 1);
    (void) q_enqueue(&q, 2);
    (void) q_enqueue(&q, 3);
    (void) q_dequeue(&q, &out);
    printf("  入队 1、2、3，正常出队一次 → 取到 %d。此时 out 栈里还留着 2（更早的那批）\n", out);
    (void) q_enqueue(&q, 4);
    (void) q_enqueue(&q, 5);
    q_dump(&q, "  又入队 4、5 之后（规矩状态：in 有新料、out 有旧料）：");

    printf("  ★ 现在【故意违反纪律】：不看 out 空不空，把 in 整个倒进 out。\n");
    int v;
    while (istk_pop(&q.in, &v)) {
        (void) istk_push(&q.out, v);              /* ✘ 就是这一行：out 里还躺着 2 */
    }
    q_dump(&q, "  违规倒完之后：");

    printf("  接下来连着出队三次，看顺序：");
    for (int i = 0; i < 3; i++) {
        if (q_dequeue(&q, &out)) {
            printf("%d ", out);
        }
    }
    printf(" ← 规矩版这三步给的是 2 3 4\n");
    printf("  为什么会这样：倒进去的 4、5 被压到了 2 的【上面】，于是晚来的 5 先出队。\n");
    printf("  最难查的是它【只在特定交错下出错】：只要 out 每次都被取空了才入队，这行代码永远不出事。\n");
    printf("  → 所以 q_dequeue 里那句 if (istk_empty(&q->out)) 是正确性要求，不是优化。\n");

    q_destroy(&q);
}

static void run_bug_demo(const char *which)
{
    if (strcmp(which, "naive") == 0) {
        bug_demo_naive();
    } else if (strcmp(which, "mix") == 0) {
        bug_demo_mix();
    } else {
        printf("  未知的反例编号：%s（可选 naive / mix）\n", which);
    }
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--bug=", 6) == 0) {
            run_bug_demo(argv[i] + 6);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0) {
            printf("用法：%s [--bug=naive|mix]\n", argv[0]);
            printf("  不带参数 = 10 幕正常演示（默认，不跑任何反例）\n");
            printf("  --bug=naive 看『每次倒过去再倒回来』的代价；--bug=mix 看违反纪律的错序\n");
            return 0;
        }
        printf("  没认出来的参数：%s（本节只认 --bug=naive / --bug=mix / --help）\n", argv[i]);
        return 0;
    }

    /* ============================================================
       第 1 幕：出发线 —— 栈和队列的规矩正好相反
       ============================================================ */
    section("1. 出发线：栈 LIFO、队列 FIFO，方向正好相反");

    printf("  队列要什么：先来的人先服务（打印任务、消息缓冲、BFS 一层层扩、抢号排队）。\n");
    printf("  栈给什么：最后放上去的盘子最先被拿走。\n");
    printf("  先把一个栈的真实行为跑出来，看清它为什么不够用：\n\n");

    IntStack one;
    istk_init(&one);
    for (int i = 1; i <= 3; i++) {
        (void) istk_push(&one, i);
        int t = 0;
        (void) istk_top(&one, &t);
        printf("    push %d 之后，栈顶 = %d\n", i, t);
    }
    printf("    现在开始弹：");
    int e = 0;
    while (istk_pop(&one, &e)) {
        printf("%d ", e);
    }
    printf("  ← 3 2 1，最新的最先出来\n");
    printf("  ✘ 想要 1 2 3 的话，一个栈做不到：1 被压在【栈底】，而栈只能动栈顶。\n");
    printf("    要把 1 拿出来，就得先把 3、2 搬走 —— 那搬走的东西放哪儿？\n");
    printf("  ✔ 所以需要第二个栈：接住搬走的东西。而且这一接一放，顺序正好被翻回来。\n");
    istk_destroy(&one);

    /* ============================================================
       第 2 幕：原料 —— 本目录自带的极简数组栈
       ============================================================ */
    section("2. 原料：本目录自带的极简 int 栈（不许跨目录借）");

    printf("  仓库规矩：每个叶子目录自包含，只能 include 本目录的 .h 和标准库，\n");
    printf("  所以 03-stack/ 里的栈【不能】include 过来 —— 这里就地写一个 40 行的。\n");
    printf("  对外只有：push / pop / top / empty（+ size、at 两个查询）。\n\n");
    printf("  ★ 下标流派（和 03-stack/array-stack 一致）：top = 栈顶元素【自己的】下标，空栈 top = -1。\n");

    IntStack s;
    istk_init(&s);
    printf("    刚 init：size = %d，top = %d，empty = %s（判空只看这一个依据）\n",
           istk_size(&s), s.top, istk_empty(&s) ? "true" : "false");

    for (int i = 1; i <= 3; i++) {
        (void) istk_push(&s, i);
    }
    printf("    连压 1、2、3：size = %d，top = %d\n", istk_size(&s), s.top);
    printf("    从栈底读到栈顶（istk_at 是给画图用的只读口子）：");
    for (int i = 0; i < istk_size(&s); i++) {
        int t = 0;
        (void) istk_at(&s, i, &t);
        printf("%d ", t);
    }
    printf("  ← 栈底 = 最早，栈顶 = 最新\n");

    int p = 0;
    (void) istk_pop(&s, &p);
    printf("    pop 一次拿走 %d；再看一眼新的栈顶：", p);
    (void) istk_top(&s, &p);
    printf("%d（现在轮到它出栈）\n", p);

    istk_destroy(&s);                              /* top 回 -1，格子涂成哨兵 */
    int untouched = 999;
    bool empty_pop = istk_pop(&s, &untouched);
    printf("    空栈上 pop：返回 %s，*out 保持调用方原来的 %d（没被污染）\n",
           empty_pop ? "true" : "false", untouched);

    printf("\n  再看一件事：固定容量会撞墙。\n");
    IntStack tight;
    istk_init(&tight);
    int ok_cnt = 0;
    int bad_cnt = 0;
    for (int i = 0; i < ISTK_CAP + 10; i++) {
        if (istk_push(&tight, i)) {
            ok_cnt++;
        } else {
            bad_cnt++;
        }
    }
    printf("    ISTK_CAP = %d：硬压 %d 次 → 成功 %d 次、失败 %d 次，size = %d（一个字节都没越界写）\n",
           ISTK_CAP, ISTK_CAP + 10, ok_cnt, bad_cnt, istk_size(&tight));
    printf("    ★ 这就是本节第一个真实代价：队列里同时最多只能放 %d 个元素。\n", ISTK_CAP);
    printf("      工程版要么给栈扩容，要么用链表栈 —— 本节把它留成『看得见的失败』而不是偷偷搬家。\n");
    istk_destroy(&tight);

    /* ============================================================
       第 3 幕：核心动作 —— 倒
       ============================================================ */
    section("3. 一个字：倒。in 整个翻进 out，顺序就反过来了");

    printf("  两个栈的分工写死：\n");
    printf("     in  栈：只负责【进队】（enqueue 永远压这里）\n");
    printf("     out 栈：只负责【出队】（dequeue 永远弹这里）\n");
    printf("  要出队而 out 空了 → 把 in 整个倒进 out。现场来一次：\n\n");

    TwoStackQueue q;
    q_init(&q);
    for (int i = 1; i <= 3; i++) {
        (void) q_enqueue(&q, i);
    }
    q_dump(&q, "入队 1、2、3 之后（三个都堆在 in 栈，1 在最底下）：");
    printf("  此刻想出队：1 在 in 的【栈底】，栈只能动栈顶 → 只能把 3、2 先搬走，搬到 out 去。\n\n");

    long mv0 = q.move_count;
    int v = 0;
    while (istk_pop(&q.in, &v)) {
        (void) istk_push(&q.out, v);
        q.move_count++;
        printf("    搬 %d：in 弹出一个 → out 压上一个\n", v);
    }
    q.pour_ops++;
    q_dump(&q, "倒完之后（看右列：out 从上往下读就是 1 2 3）：");
    printf("  ★ 为什么顺序反过来了：弹的时候是【新 → 旧】，\n");
    printf("    压到对面去就变成【新的先落底、旧的后来】→ 旧的停在栈顶。\n");
    printf("    这一趟一共搬了 %ld 个（两个栈各自的 push/pop 计数器：in %ld 次 pop、out %ld 次 push）\n",
           q.move_count - mv0, q.in.n_pop, q.out.n_push);
    int head = 0;
    (void) istk_pop(&q.out, &head);
    printf("  于是 pop 一下 out 栈顶 → 拿到 %d = 最早进队的那个 = FIFO ✔\n\n", head);
    printf("  整节的灵魂就这一句：\n");
    printf("     如果 out 空了，就把 in 整个倒进 out；然后弹 out 的栈顶。\n");

    /* ============================================================
       第 4 幕：完整场景 —— 每一步都画两个栈
       ============================================================ */
    section("4. 完整场景（本节主菜）：enq 1,2,3 → deq → deq → enq 4,5 → deq x3 → deq(空)");

    printf("  看的时候只问自己一句话：『现在 out 栈顶是谁？它是不是最早进队的那个？』\n");
    printf("  画法是竖着的：最上面那一行是【栈顶】，越往下越早，-- 表示这一格没有东西。\n\n");

    TwoStackQueue demo;
    q_init(&demo);
    char label[160];
    int  enq_seq[8];                  /* 实际按顺序入队的值 */
    int  deq_seq[8];                  /* 实际按顺序出队的值 */
    int  n_enq = 0, n_deq = 0;

    for (int i = 1; i <= 3; i++) {
        bool ok = q_enqueue(&demo, i);
        enq_seq[n_enq++] = i;
        snprintf(label, sizeof label, "① q_enqueue(%d) → %s（只是压 in 栈，一次就完）",
                 i, ok ? "true" : "false");
        q_dump(&demo, label);
    }

    deq_seq[n_deq++] = deq_step(&demo, "②");
    deq_seq[n_deq++] = deq_step(&demo, "②");

    for (int i = 4; i <= 5; i++) {
        (void) q_enqueue(&demo, i);
        enq_seq[n_enq++] = i;
        snprintf(label, sizeof label, "③ q_enqueue(%d) → 压进 in 栈（注意：没有去动 out 栈）", i);
        q_dump(&demo, label);
    }

    deq_seq[n_deq++] = deq_step(&demo, "④");
    deq_seq[n_deq++] = deq_step(&demo, "④");
    deq_seq[n_deq++] = deq_step(&demo, "④");

    int sentinel = 12345;                          /* 显眼值：用来验证『失败时不写 *out』 */
    bool last = q_dequeue(&demo, &sentinel);
    snprintf(label, sizeof label,
             "⑤ q_dequeue() → %s（两个栈都空了 = 队列真的空），*out 仍然是原来的 %d",
             last ? "true" : "false", sentinel);
    q_dump(&demo, label);

    printf("  这一路的账本：入队 %ld 次，出队 %ld 次（其中失败 %ld 次），倒栈 %ld 次，累计搬运 %ld 次\n",
           demo.enqueue_ops, demo.dequeue_ops, demo.dequeue_fail, demo.pour_ops, demo.move_count);

    /* 出队序列不是我们『说它是 1 2 3 4 5』，是把 deq_step 实际返回的值挨个攒起来打出来，
     * 再和入队序列逐个比一遍 —— 这样这段旁白不可能骗人。 */
    printf("  入队序列（实测）：");
    for (int i = 0; i < n_enq; i++) {
        printf("%d ", enq_seq[i]);
    }
    printf("\n  出队序列（实测）：");
    for (int i = 0; i < n_deq; i++) {
        printf("%d ", deq_seq[i]);
    }
    bool fifo_ok = (n_enq == n_deq);
    for (int i = 0; i < n_deq && i < n_enq; i++) {
        if (deq_seq[i] != enq_seq[i]) {
            fifo_ok = false;
        }
    }
    printf("→ 两个序列%s，%s FIFO ✔\n",
           fifo_ok ? "逐位相同" : "不一样（那就是 bug！）",
           fifo_ok ? "完全" : "不");
    printf("  搬运 %ld 次 vs %d 个元素 → 平均每人 %.2f 次，也就是『一辈子只被搬一次』。\n",
           demo.move_count, n_enq, (double) demo.move_count / (double) n_enq);
    printf("  注意『倒栈只发生了 %ld 次，而操作一共 %ld 次』—— 大多数操作根本不碰搬运，\n",
           demo.pour_ops, demo.enqueue_ops + demo.dequeue_ops);
    printf("  这就是下一幕均摊分析的入口。\n");
    q_destroy(&demo);

    /* ============================================================
       第 5 幕：peek / empty / size
       ============================================================ */
    section("5. peek / empty / size：peek 是唯一『看着只读、其实会动手』的函数");

    TwoStackQueue pk;
    q_init(&pk);
    (void) q_enqueue(&pk, 10);
    (void) q_enqueue(&pk, 20);
    (void) q_enqueue(&pk, 30);

    printf("  队列里堆着 10、20、30（全在 in 栈）。现在调 peek：\n");
    printf("    队头 10 在 in 栈的【栈底】，而栈只能看栈顶 → 唯一的正规解法：真的倒一次。\n");
    long mv_before = pk.move_count;
    int head_v = 0;
    bool pk_ok = q_peek(&pk, &head_v);
    printf("  q_peek() → %s，队头 = %d，本次搬运 %ld 次\n",
           pk_ok ? "true" : "false", head_v, pk.move_count - mv_before);
    printf("  ★ 所以 peek 不是只读操作：q_peek 的签名里【没有 const】，就是这个原因。\n");
    printf("    写成 const 只有两条路：要么撒谎（偷偷改结构），要么绕开栈的抽象去直接摸\n");
    printf("    in 栈的栈底 —— 后者确实能省掉这趟搬运，但『队列只靠栈的四个动作拼出来』\n");
    printf("    这句话当场作废。本节选老实做法，把代价说明白。\n");

    mv_before = pk.move_count;
    (void) q_peek(&pk, &head_v);
    printf("  再 peek 一次 → 队头 %d，本次搬运 %ld 次（out 非空 → 直接看栈顶，一分钱不花）\n",
           head_v, pk.move_count - mv_before);
    printf("  → peek 的代价和 dequeue 一样：均摊 O(1)，最坏一次 O(n)。\n\n");

    int dv = 0;
    (void) q_dequeue(&pk, &dv);
    printf("  empty / size 必须同时看两个栈：\n");
    printf("    现在 in = %d 个、out = %d 个 → q_size = %d，q_empty = %s\n",
           istk_size(&pk.in), istk_size(&pk.out), q_size(&pk), q_empty(&pk) ? "true" : "false");
    printf("    ★ 只判 in 会漏掉『已经倒过去但还没出队』的那批 —— 这是本节第一大笔误。\n");
    int x = 0;
    while (q_dequeue(&pk, &x)) { /* 掏空 */ }
    printf("    全出队之后：in = %d、out = %d → q_size = %d，q_empty = %s\n",
           istk_size(&pk.in), istk_size(&pk.out), q_size(&pk), q_empty(&pk) ? "true" : "false");
    int keep = 555;
    printf("    空队列上 q_peek → %s（*out 仍是 %d）\n",
           q_peek(&pk, &keep) ? "true" : "false", keep);
    q_destroy(&pk);

    /* ============================================================
       第 6 幕：均摊分析 —— 计数器实测
       ============================================================ */
    section("6. 均摊：单次最坏 O(n)，但每个元素一辈子只搬一次 → 均摊 O(1)");

    printf("  先把用词说准：本节【不依赖随机数】，所以这不是『平均复杂度』，\n");
    printf("  而是『均摊（amortized）』：就算对手故意构造最坏的操作序列，总代价也超不过界限。\n");
    printf("  理由只有一句：元素进 in 栈 1 次 → 被倒进 out 栈最多 1 次 → 从 out 弹出 1 次。\n");
    printf("     为什么不会有第二次搬运？因为它一旦进了 out，就必须先出队才能再回 in ——\n");
    printf("     而 out 非空时禁止倒栈（那是反例二），所以一辈子只搬这一次。\n\n");

    printf("  ① 常规混合：6 步一循环（enq enq enq deq deq deq），跑 100 轮 = 600 次操作\n");
    TwoStackQueue m;
    q_init(&m);
    long hist[8];
    for (int i = 0; i < 8; i++) {
        hist[i] = 0;
    }
    for (int round = 0; round < 100; round++) {
        for (int k = 0; k < 3; k++) {
            (void) q_enqueue(&m, round * 10 + k);
        }
        for (int k = 0; k < 3; k++) {
            long before = m.move_count;
            int got = 0;
            (void) q_dequeue(&m, &got);
            long used = m.move_count - before;
            if (used < 8) {
                hist[used]++;
            }
        }
    }
    printf("     入队 %ld 次，出队 %ld 次，倒栈 %ld 次，总搬运 %ld 次，单次出队最大搬运 %ld 个\n",
           m.enqueue_ops, m.dequeue_ops, m.pour_ops, m.move_count, m.max_one_deq);
    printf("     每次出队搬了几个（分布）：");
    for (int i = 0; i < 8; i++) {
        if (hist[i] > 0) {
            printf("搬 %d 个 → %ld 次   ", i, hist[i]);
        }
    }
    printf("\n");
    printf("     平均 %.2f 个/次 → 均摊 O(1) 的实测含义就是这个数【不随规模增长】\n",
           (double) m.move_count / (double) m.dequeue_ops);
    printf("     ★ 大部分出队搬 0 个（直接弹 out），少数出队一口气搬一坨 —— 这就是均摊的样子。\n");
    q_destroy(&m);

    printf("\n  ② 最坏形状：狂灌 %d 个（in 栈装满），再一次性出队 %d 个\n", ISTK_CAP, ISTK_CAP);
    TwoStackQueue worst;
    q_init(&worst);
    int *bulk = malloc(sizeof(int) * (size_t) ISTK_CAP);   /* 本目录唯一一块堆内存 */
    if (bulk == NULL) {
        fprintf(stderr, "内存不足，最坏形状那一半演示被跳过了\n");
    } else {
        for (int i = 0; i < ISTK_CAP; i++) {
            bulk[i] = i + 1;
        }
        int put = 0;
        for (int i = 0; i < ISTK_CAP; i++) {
            if (q_enqueue(&worst, bulk[i])) {
                put++;
            }
        }
        printf("     灌进去 %d 个（in 栈 size = %d，out 栈 size = %d）\n",
               put, istk_size(&worst.in), istk_size(&worst.out));

        long per_deq[ISTK_CAP];
        int nd = 0;
        for (int i = 0; i < put; i++) {
            long before = worst.move_count;
            int got = 0;
            (void) q_dequeue(&worst, &got);
            per_deq[nd++] = worst.move_count - before;
        }
        long sum = 0;
        long zeros = 0;
        for (int i = 0; i < nd; i++) {
            sum += per_deq[i];
            if (per_deq[i] == 0) {
                zeros++;
            }
        }
        printf("     第一次出队搬 %ld 个（全队一起搬家）→ 这一次真的是 O(n)\n", per_deq[0]);
        printf("     其余 %ld 次出队全部搬 0 个（out 里已经排好队了）\n", zeros);
        printf("     总搬运 %ld 次 / 出队 %d 次 = 平均 %.2f\n",
               sum, nd, nd ? (double) sum / (double) nd : 0.0);
        printf("     ★ 等式成立：搬运次数 %ld == 成功出队次数 %d == 元素个数 %d\n", sum, nd, nd);
        printf("       更一般的形式是：搬运次数 = 成功出队次数 + out 栈里还剩下的个数\n");
        printf("       （此刻还剩 %d 个，成功出队 %ld 次 → %ld + %d = %ld，和账本 %ld 对得上）\n",
               istk_size(&worst.out), worst.dequeue_ops - worst.dequeue_fail,
               worst.dequeue_ops - worst.dequeue_fail, istk_size(&worst.out),
               (worst.dequeue_ops - worst.dequeue_fail) + istk_size(&worst.out),
               worst.move_count);
        free(bulk);
        bulk = NULL;
    }
    q_destroy(&worst);

    printf("\n  ③ 一句话记住均摊：\n");
    printf("     单次贵得要命（O(n)）没关系，只要『贵』这件事在【同一个元素身上只发生一次】，\n");
    printf("     n 次操作的总账就是 O(n)，摊到每次就是 O(1)。\n");
    printf("     反例版（--bug=naive）慢在哪？它让同一个元素反复付这笔钱 → 总账变 O(n^2)。\n");

    /* ============================================================
       第 7 幕：反面教材预告
       ============================================================ */
    section("7. 反面教材：两段『正确但有害』的代码（都挂在命令行开关后面，默认不跑）");

    printf("    --bug=naive  q_dequeue_naive()：每次出队都倒过去、再倒回来\n");
    printf("                 → 对外行为【完全正确】，但单次 O(n)、n 次 O(n^2)；只有计数器看得出差距\n");
    printf("    --bug=mix    手动违反纪律：out 非空时又把 in 倒进来\n");
    printf("                 → 这个更狠：不崩、不报内存错误，只是【顺序错】，而且只在特定交错下错\n");
    printf("  为什么值得各跑一遍：\n");
    printf("    naive 版教你看懂『均摊』这件事本身 —— 它和正确版的输出序列一模一样；\n");
    printf("    mix 版教你看懂『那句判空不是优化』—— 少了它，程序只是偶尔给你换个顺序。\n");
    printf("  这两段的实测数字在 README 的『运行结果』里都贴了，跑一下就复现。\n");

    /* ============================================================
       第 8 幕：边界
       ============================================================ */
    section("8. 边界与坑：空队列、容量满、destroy 之后、元素值域");

    printf("  ① 空队列上 dequeue / peek：返回 false 并且不写 *out\n");
    TwoStackQueue emp;
    q_init(&emp);
    int keep2 = 4242;
    printf("     q_dequeue(空) → %s，*out 还是 %d\n",
           q_dequeue(&emp, &keep2) ? "true" : "false", keep2);
    printf("     q_peek(空)    → %s，*out 还是 %d\n",
           q_peek(&emp, &keep2) ? "true" : "false", keep2);
    printf("     q_empty(空)   → %s，q_size = %d\n",
           q_empty(&emp) ? "true" : "false", q_size(&emp));

    printf("  ② in 栈满时入队会失败，但已入队的元素照旧能出队\n");
    int filled = 0;
    for (int i = 0; i < ISTK_CAP + 5; i++) {
        if (q_enqueue(&emp, i)) {
            filled++;
        }
    }
    printf("     尝试 %d 次入队 → 收下 %d 个，拒绝 %d 个；q_size = %d\n",
           ISTK_CAP + 5, filled, ISTK_CAP + 5 - filled, q_size(&emp));
    int got2 = 0;
    (void) q_dequeue(&emp, &got2);
    printf("     拒绝入队不影响出队：第一个取到的是 %d（就是最早那个）\n", got2);
    q_destroy(&emp);

    printf("  ③ destroy 之后再用：必须安全失败\n");
    TwoStackQueue dead;
    q_init(&dead);
    (void) q_enqueue(&dead, 99);
    q_destroy(&dead);
    int dv2 = -1;
    printf("     q_empty = %s，q_size = %d，q_dequeue = %s（*out 还是 %d，没读到旧数据）\n",
           q_empty(&dead) ? "true" : "false", q_size(&dead),
           q_dequeue(&dead, &dv2) ? "true" : "false", dv2);
    q_destroy(&dead);                              /* 幂等：再来一次也不炸 */
    printf("     再 destroy 一次也没崩（destroy 只复位状态，而且第一件事就是判 NULL）\n");

    printf("  ④ 元素值域：栈里存 int，任何 int 都能进（负数、0、重复值、INT_MAX）\n");
    TwoStackQueue val;
    q_init(&val);
    int seq_v[4] = { -7, 0, 2147483647, -7 };
    for (int i = 0; i < 4; i++) {
        (void) q_enqueue(&val, seq_v[i]);
    }
    printf("     灌入 -7 0 2147483647 -7（含 INT_MAX 和两个相同的 -7），出队序列：");
    int vv = 0;
    while (q_dequeue(&val, &vv)) {
        printf("%d ", vv);
    }
    printf("\n     → 结构里没有哨兵值占用真实数据空间（对比跳表那节：head/nil 吃掉了 INT_MIN/INT_MAX）\n");
    q_destroy(&val);

    /* ============================================================
       第 9 幕：反过来想
       ============================================================ */
    section("9. 反过来：用两个【队列】实现【栈】（思路 + 关键代码在 README）");

    printf("  刚才：两个 LIFO 拼出 FIFO —— 靠『整个倒一次』把顺序翻回来，每人只翻一次。\n");
    printf("  现在：两个 FIFO 拼出 LIFO —— 靠『留最后一个』把栈顶顶出来。\n\n");
    printf("  思路（push 便宜版）：\n");
    printf("    push(x)   ：x 进主队列；\n");
    printf("    pop / top ：把主队列前 n-1 个人依次搬到副队列，剩下那个就是栈顶；\n");
    printf("              取走它，然后把主、副队列【名字互换】（不搬数据）。\n");
    printf("  关键代价：每次 pop 都要搬 n-1 个，而且同一个元素每次 pop 都要被搬一遍\n");
    printf("            （它每次都得给后来者让路）→ 真 O(n)，均摊也救不回来。\n\n");
    printf("  ★ 这道题真正的收获是对照『为什么两栈队列能均摊 O(1)、两队列栈不能』：\n");
    printf("     两栈队列：一个元素被搬，是因为它【该出队了】—— 这种机会一辈子只有一次；\n");
    printf("     两队列栈：一个元素被搬，是因为【别人比它新】—— 每来一个新人都要它让一次路。\n");
    printf("     均摊分析的全部功夫，就在于数清楚『同一个元素付了几次钱』。\n");

    /* ============================================================
       第 10 幕：收尾
       ============================================================ */
    section("10. 收尾：几个 q_init 就要几个 q_destroy");

    printf("  本节全程只有一块堆内存（第 6 幕的 bulk 数组），它已经在幕内 free 并置 NULL。\n");
    printf("  其余结构体都在栈上：one、tight 是数组栈，q 是第 3 幕建的队列，现在还掉：\n");
    q_destroy(&q);
    printf("    q 已 destroy（两个栈各自复位；再 destroy 一次也不会炸）\n");
    printf("  ★ 数组栈一个 malloc 都没有，为什么还要写 destroy？\n");
    printf("    ① 仓库规矩：有 init 就要有 destroy，读代码的人不用先判断这个类型有没有堆内存；\n");
    printf("    ② destroy 会把格子涂成哨兵值并复位 top，防止结构体复用两次时读到上一次的残留。\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住五句话：\n");
    printf("  1) 分工写死：in 只管进队，out 只管出队；谁也不能越界干对方的事。\n");
    printf("  2) 灵魂一句：out 空了才把 in 整个倒过来 —— 倒完 out 栈顶就是最早进队的那个。\n");
    printf("  3) 那个判空不是优化，是正确性要求：out 非空时倒栈 = 顺序永久错乱（--bug=mix）。\n");
    printf("  4) 单次出队最坏 O(n)，但每个元素一辈子只搬一次 → n 次操作总搬运 <= n → 均摊 O(1)；\n");
    printf("     反面教材（--bug=naive）结果全对、每次出队却搬 2n-1 次：正确 不等于 快。\n");
    printf("  5) size 是两栈之和，empty 是两栈都空，peek 会真的动手搬 —— 元素住在两个栈里。\n");
    return 0;
}
