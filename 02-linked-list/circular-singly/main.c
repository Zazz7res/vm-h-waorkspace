/*
 * ⚠ 这一行必须在【所有 #include 之前】，而且必须在文件最顶部：
 *   -std=c11 是"严格 ISO C"模式，glibc 会因此藏掉 fork / waitpid 这些 POSIX 声明，
 *   不打开关的话编译会报 "implicit declaration of function 'fork'"（警告 = 我们的闸门直接不合格）。
 *   告诉 glibc"我要 POSIX.1-2008 的这些函数"，声明才会露出来。
 *   只有 --bug 那一幕用到 fork，所以只有这个文件需要它。
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>      /* SIGSEGV */
#include <sys/wait.h>    /* waitpid / WIFSIGNALED / WTERMSIG */
#include <unistd.h>      /* fork / _exit */

#include "clist.h"

/* 小工具：分幕标题（和 pointer-basics / singly / doubly 同款，保持手感一致） */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/* 小工具：一行看清"链表内容 + 账本对不对"。
 * 为什么每次都要对账？因为 size 是抄下来的账，漏记一次就永久撒谎，
 * 而程序不会崩，只是长度不对 —— 这种 bug 最阴。 */
static void show(const char *label, CList *l)
{
    printf("  %s: ", label);
    clist_print(l);
}

static void check_size(CList *l)
{
    int book = clist_size(l);
    int walk = clist_count_by_walk(l);
    printf("      对账 → size 账本 = %d, 绕圈现数 = %d  %s\n",
           book, walk, (book == walk) ? "✓ 一致" : "✗ 不一致，有地方漏记账了！");
}

/* ---------- 约瑟夫环的"对拍"工具 ----------
 * 光看链表版自己打印自己，等于自己给自己打分。这里再写一份【完全独立】的数组版：
 * 它用 alive[] 标记谁还在圈里，靠 (pos + 1) % n 绕回开头 —— 和链表的"走一根 next"是两种世界观。
 * 两份代码思路毫无关系，答案却必须一模一样；不一致就有一边写错了。
 * 这是验证数据结构实现最省钱也最狠的一招，以后每写一个结构都该这么干。
 */
#define JOSEPHUS_MAX 64

static void print_order(const int *a, int n)
{
    for (int i = 0; i < n; i++) {
        printf("%d%s", a[i], (i == n - 1) ? "" : " ");
    }
    printf("（共 %d 人）\n", n);
}

static int josephus_array(int n, int m, int out[], int cap)
{
    if (n <= 0 || m <= 0 || n > JOSEPHUS_MAX) {
        return 0;
    }

    int alive[JOSEPHUS_MAX];
    for (int i = 0; i < n; i++) {
        alive[i] = 1;                          /* 1 = 还在圈里 */
    }

    int pos  = -1;                             /* 上一次报数停在哪儿；-1 表示"还没开始，下一个就是 0 号" */
    int left = n;
    int cnt  = 0;
    while (left > 0) {
        int spoken = 0;
        while (spoken < m) {
            pos = (pos + 1) % n;               /* ★ 数组版必须取模才能绕回去；链表版不用 */
            if (alive[pos]) {
                spoken++;                      /* 只有还在圈里的人才占一个报数名额 */
            }
        }
        alive[pos] = 0;
        left--;
        if (cnt < cap) {
            out[cnt] = pos + 1;                /* 下标 → 编号（编号从 1 数） */
        }
        cnt++;
    }
    return cnt;
}

/* 跑一个约瑟夫用例：链表版（会打印每一轮）+ 数组版（静默）+ 对拍结论 */
static void josephus_case(int n, int m)
{
    int linked[JOSEPHUS_MAX];
    int arr[JOSEPHUS_MAX];

    int cl = clist_josephus(n, m, linked, JOSEPHUS_MAX);
    int ca = josephus_array(n, m, arr, JOSEPHUS_MAX);

    printf("    链表版出圈顺序: ");
    print_order(linked, cl);
    printf("    数组版出圈顺序: ");
    print_order(arr, ca);

    int same = (cl == ca) ? 1 : 0;
    for (int i = 0; i < cl && i < ca; i++) {
        if (linked[i] != arr[i]) {
            same = 0;
        }
    }
    printf("    对拍结果: %s ← 两套思路完全不同的实现给出同一个答案，才算真的写对了\n",
           same ? "✓ 一致" : "✗ 不一致，有一边是错的");
}

/*
 * 受控演示 A：死循环。
 * 把单链表的 `p != NULL` 原样搬到循环表上，会永远碰不到 NULL。
 * clist_count_broken_null() 里塞了一个"最多走这么多步"的刹车，所以本进程安全。
 */
static void bug_demo_infinite_loop(void)
{
    CList l;
    clist_init(&l);
    clist_push_back(&l, 10);
    clist_push_back(&l, 20);
    clist_push_back(&l, 30);
    show("环长 3 的表", &l);

    int   hit_null = 0;
    int   steps    = clist_count_broken_null(&l, 12, &hit_null);
    printf("  用『p != NULL』当终止条件去数长度：走了 %d 步就被强行刹车（碰到 NULL 了吗？%s）\n",
           steps, hit_null ? "碰到了" : "没碰到，永远也碰不到");
    printf("  ↑ 真实程序里没有这个刹车：三个节点的环它也要数到程序被杀死。\n");
    printf("    表现就是 CPU 100%%、程序卡死、日志一行不刷 —— 线上事故的经典主角。\n");
    clist_free(&l);
}

/*
 * 受控演示 B：当场段错误。
 * 少写一次判空 → tail 是 NULL 还去读 tail->next → 解引用空指针。
 * ⚠ 真的会崩，所以 fork 一个子进程去崩，父进程站旁边收尸：
 *   整个演示跑完，进程退出码还是 0，日志还是干净的。
 */
static void bug_demo_real_crash(void)
{
    /* ★ fork 会把【已经攒在 stdio 缓冲区里、还没写出去的文字】一起拷贝给子进程。
     *   终端下 stdout 是行缓冲，看不出问题；一旦输出重定向到文件就变成全缓冲，
     *   不 fflush 的话子进程会那半截内容再打一遍，屏幕上出现重复段落。 */
    fflush(stdout);

    pid_t kid = fork();
    if (kid < 0) {
        printf("  fork 失败，跳过这个演示（不影响后面的内容）\n");
        return;
    }

    if (kid == 0) {
        /* 子进程：把 stderr 接到 /dev/null，ASan 那份几十行的报告就不糊日志了；
         * 然后用 _exit 直接消失（不跑 atexit，免得子进程也被做一遍泄漏检查）。 */
        freopen("/dev/null", "w", stderr);
        CList empty;
        clist_init(&empty);
        printf("      [子进程] 我要在空表上执行 tail->next->val 了，看崩不崩...\n");
        fflush(stdout);
        int v = clist_head_val_unguarded(&empty);        /* ❌ 这一行就是事故现场 */
        printf("      [子进程] 居然没崩？v = %d\n", v);
        _exit(0);
    }

    int status = 0;
    waitpid(kid, &status, 0);                            /* 等子进程死透，回收它，不留僵尸 */
    if (WIFSIGNALED(status)) {
        printf("      [父进程] 子进程被信号 %d 打死（SIGSEGV = %d）→ 确确实实是段错误\n",
               WTERMSIG(status), SIGSEGV);
    } else if (WEXITSTATUS(status) != 0) {
        printf("      [父进程] 子进程异常退出，退出码 %d → ASan 抢先接管了这次空指针解引用并终止了它\n",
               WEXITSTATUS(status));
    } else {
        printf("      [父进程] 子进程竟然全身而退（编译器把这次访问优化掉了，不该发生）\n");
    }
    printf("  ↑ 同一个位置，走正门的 clist_head() 会先判 tail == NULL，空表返回 NULL，一点事没有。\n");
    printf("    教训：循环表里【没有任何 NULL 帮你兜底】，判空这件事只能自己写，写漏就崩。\n");
}

int main(int argc, char **argv)
{
    int show_bug = 0;
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && argv[i][1] == '-') {
            show_bug = 1;                    /* 任何 --xxx 都算开启 bug 演示，简单够用 */
        }
    }

    CList l;                                 /* 栈上的"把手"：里面只有 tail 和 size 两个字段 */
    clist_init(&l);

    /* ============================================================
       第 1 幕：尾巴指回头 —— 循环链表就多了这一根箭头
       ============================================================ */
    section("1. 空表 / 单节点 / 多节点：尾巴的 next 到底是谁");

    printf("  空表：tail = %s, size = %d\n",
           l.tail == NULL ? "NULL（连环都还没有）" : "非 NULL", clist_size(&l));
    printf("  空表问一句 head 是谁: %s\n",
           clist_head(&l) == NULL ? "NULL —— 因为压根没有节点" : "有值");
    printf("  空表是空的吗？is_empty = %d\n", clist_is_empty(&l));

    clist_push_back(&l, 42);
    printf("\n  尾插 42（第一个节点）：★ 它的 next 指向【自己】，这就是环的最小形态\n");
    show("  现在的环", &l);
    printf("      tail->val = %d, tail->next->val = %d\n", l.tail->val, l.tail->next->val);
    printf("      tail->next == tail ? %s\n",
           l.tail->next == l.tail ? "成立（自己牵自己，一根箭头闭环）" : "不成立");
    printf("      单链表在这里只能写 next = NULL；就是这一格的区别，逼出了整套新规则\n");

    clist_push_back(&l, 7);
    clist_push_back(&l, 99);
    printf("\n  再尾插 7、99：\n");
    show("现在的环", &l);
    Node *h = clist_head(&l);
    printf("      tail 停在 val = %d；tail->next 的 val = %d\n", l.tail->val, h->val);
    printf("      clist_head(&l) == tail->next ? %s —— head 没存在结构体里，是当场算的\n",
           (h == l.tail->next) ? "成立（同一个地址）" : "不成立");
    printf("      画出来（★ 就是最后那根箭头，单链表在那个位置写的是 NULL）：\n");
    printf("        head = tail->next（零步白捡）\n");
    printf("          │\n");
    printf("          ▼\n");
    printf("        [%d|●] ──► [%d|●] ──► [%d|●] ──┐\n", h->val, h->next->val, l.tail->val);
    printf("          ▲                             │\n");
    printf("          └─────────────────────────────┘  ← tail 停在 %d，它的 next 指回 %d\n",
           l.tail->val, h->val);

    /* ============================================================
       第 2 幕：只维护 tail，头插尾插都是 O(1)
       ============================================================ */
    section("2. 头插 O(1) + 尾插 O(1)：秘诀是【只存 tail，head 白捡】");

    clist_free(&l);
    clist_init(&l);
    printf("  从空表开始，依次尾插 1、2、3（顺序保住，每步恒定几行代码）：\n");
    clist_push_back(&l, 1);
    show("    尾插 1", &l);
    clist_push_back(&l, 2);
    show("    尾插 2", &l);
    clist_push_back(&l, 3);
    show("    尾插 3", &l);
    printf("  → 单链表尾插每次都要从头走到尾（0+1+2 步）；这里 tail 就在手里，一步到位。\n");

    printf("\n  现在头插 0、-1（注意 tail 完全没动）：\n");
    Node *tail_before = l.tail;
    clist_push_front(&l, 0);
    show("    头插 0", &l);
    clist_push_front(&l, -1);
    show("    头插 -1", &l);
    printf("  tail 把手动过吗？%s（头插只改 tail->next 这一根线）\n",
           tail_before == l.tail ? "没动，还是那条尾巴" : "动了");
    printf("  只存 head 做不到的：那尾插就得走一整趟。只存 tail 两头通吃 —— 这是本节最值钱的一句。\n");
    printf("  ⚠ 反过来说：在循环表里【存 head 是浪费】，head 永远能 tail->next 一步拿到，\n");
    printf("    多存一个字段就多一份要维护的账（head 和 tail 会不一致，历史上真出过这种 bug）。\n");
    check_size(&l);

    /* ============================================================
       第 3 幕：绕圈打印 —— 把"没有终点"演给你看
       ============================================================ */
    section("3. 从 tail 出发走 2×n 步：亲眼看到它绕回来");

    int   n = clist_size(&l);
    printf("  环里有 %d 个节点，故意走 %d 步：\n", n, 2 * n);
    clist_walk_ring(&l, 2 * n);
    printf("  → 第 %d 步之后就进入第二圈了：val 序列一模一样地重播。\n", n);
    printf("    走一百步也等不到一个 NULL —— 所以说『NULL 不再是终点』。\n");

    printf("\n  边界：只有一个节点时绕圈\n");
    CList solo;
    clist_init(&solo);
    clist_push_back(&solo, 555);
    clist_walk_ring(&solo, 3);
    printf("  → 自己牵自己的环，走多少步都是同一个节点，也永远停不下来。\n");
    clist_free(&solo);

    /* ============================================================
       第 4 幕：遍历终止条件 + 按值查找 + 长度
       ============================================================ */
    section("4. 遍历终止条件：p != NULL 改成 p != head（找 / 数都靠它）");

    show("当前环", &l);
    printf("  正确写法（clist_find 内部）：\n");
    printf("      Node *p = head;  do { 用 p;  p = p->next; } while (p != head);\n");
    printf("  三个必须记住的点：\n");
    printf("      ① 终止条件比的是【地址】，不是值 —— 走回起点说明全看过了\n");
    printf("      ② 必须 do-while：写成 while (p != head) 一次都不会进，第一个节点直接漏掉\n");
    printf("      ③ 先干活再挪指针，顺序反了会把起点少看一次、把 NULL 位置多看一次\n");

    Node *hit  = clist_find(&l, 2);
    Node *miss = clist_find(&l, 200);
    printf("  find(2)  : %s\n", hit  != NULL ? "命中，返回节点地址（想删它就直接拿这个地址）" : "未命中");
    printf("  find(200): %s ← 绕完整整一圈才敢下结论，所以是 O(n)\n",
           miss != NULL ? "命中" : "未命中，返回 NULL（这个 NULL 是我们造的信号，链表里没有）");
    check_size(&l);

    /* ============================================================
       第 5 幕：按值删除 —— 头 / 中 / 尾 / 唯一 / 不存在，五种局面的账都要对
       ============================================================ */
    section("5. 按值删除：要改的是【前驱】那根线（循环表的老规矩，一点没变轻松）");

    printf("  ★ 单链表的老毛病一条都没少：节点只写着下一个是谁 → 删它得改【前驱】的 next →\n");
    printf("    所以必须 prev/cur 两个指针手拉手往前走。循环表只帮上一个小忙：\n");
    printf("    『head 的前驱』白捡（就是 tail），于是能从 tail 起步，不用特判『头没有前驱』。\n");

    clist_free(&l);
    clist_init(&l);
    for (int i = 1; i <= 5; i++) {
        clist_push_back(&l, i * 10);            /* 10 20 30 40 50 */
    }
    show("起始", &l);
    printf("      （tail 停在 50；50 的下一个是 10）\n");

    clist_remove(&l, 10);
    show("删 10（头节点：只改 tail->next，tail 不动）", &l);
    check_size(&l);

    clist_remove(&l, 30);
    show("删 30（中间节点：prev->next = cur->next）", &l);
    check_size(&l);

    printf("      此刻 tail 停在 val = %d\n", l.tail->val);
    clist_remove(&l, 50);
    show("删 50（尾巴节点：tail 必须挪到前驱 40）", &l);
    printf("      此刻 tail 停在 val = %d ← 漏掉那句『tail = prev』，把手就指向已释放内存了\n", l.tail->val);
    check_size(&l);

    int nothing = clist_remove(&l, 999);
    printf("  删 999（不存在）：返回值 = %d，表原封不动: ", nothing);
    clist_print(&l);
    check_size(&l);

    clist_remove(&l, 40);
    show("删 40（倒数第二个：剩下 20 自己牵自己）", &l);
    printf("      tail 现在停在 val = %d，tail->next == tail ? %s\n",
           l.tail->val, l.tail->next == l.tail ? "成立" : "不成立");
    clist_remove(&l, 20);
    show("删 20（最后一个：环被彻底拆掉）", &l);
    printf("      tail = %s, size = %d, is_empty = %d ← 三者必须同时成立\n",
           l.tail == NULL ? "NULL" : "非 NULL", clist_size(&l), clist_is_empty(&l));

    int ghost = clist_remove(&l, 1);
    printf("  空表上继续删：返回值 = %d（安全拒绝，没崩）\n", ghost);

    clist_push_back(&l, 7);
    clist_push_back(&l, 7);
    clist_push_back(&l, 7);
    printf("\n  有重复值时删一次（只删最靠前那个）：\n");
    show("删之前", &l);
    clist_remove(&l, 7);
    show("删之后", &l);
    check_size(&l);
    clist_free(&l);
    clist_init(&l);

    /* ============================================================
       第 6 幕：约瑟夫环实战 ① n=7, m=3
       ============================================================ */
    section("6. 约瑟夫环实战 ①：n=7 人围圈，从 1 报数，数到 m=3 出圈");

    printf("  ★ 为什么这题是循环链表的『原生场景』：\n");
    printf("    数组版：绕回开头要写 (i + 1) %% n，出圈要把后面元素整体往前搬一格（O(n)）\n");
    printf("    循环链表：绕回开头是结构自带的（走一根 next 而已），出圈 = 改一根线（O(1)）\n\n");

    josephus_case(7, 3);
    printf("  ★ 盯住『剩下』那一行的开头：下一轮永远从【出圈者的下一个人】开始报数。\n");
    printf("    代码里只有一句 ring.tail = prev（把手挪到出圈者的前驱），起点就自动跟着转过去了 ——\n");
    printf("    循环表里『谁是头』纯粹是约定，转把手 = 转圈，一个节点都不用动。\n");

    /* ============================================================
       第 7 幕：约瑟夫环实战 ② n=10, m=4 + m 比剩余人数大的情况
       ============================================================ */
    section("7. 约瑟夫环实战 ②：n=10, m=4；再看看 m 比环还长会怎样");

    josephus_case(10, 4);

    printf("\n  m > 剩下人数（n=5, m=7）：环上多绕两圈半，一行特判都不用写\n");
    josephus_case(5, 7);
    printf("  ↑ 数组版遇到 m 比剩余人数大，得老实算 (pos + m - 1) %% size 才知道停在哪一格；\n");
    printf("    链表版只管 cur = cur->next 一路走，走过头自然绕回来 —— 环自己就是那个取模。\n");

    printf("\n  边界：n=1（一个人也是环，报数会一直报到自己头上）\n");
    josephus_case(1, 3);

    printf("\n  边界：参数非法 n=0\n");
    int refused = clist_josephus(0, 3, NULL, 0);
    printf("    返回 %d（拒绝执行，也没有 malloc 过任何东西）\n", refused);

    /* ============================================================
       第 8 幕：轮转发牌 —— RR 调度同款，"推进 = 走一根 next"
       ============================================================ */
    section("8. 轮转发牌（时间片轮转调度同款）：3 个玩家分一副牌");

    int deck[] = { 3, 5, 8, 2, 7, 4, 9, 10, 6, 1, 11, 13 };
    int ndeck  = (int)(sizeof(deck) / sizeof(deck[0]));
    printf("  一副 %d 张牌（牌面就是数字），发给 3 个玩家，轮流一张一张来：\n", ndeck);
    clist_deal(3, deck, ndeck);
    printf("  → 注意『轮到下一个人』在代码里就是 cur = cur->next; 一句。\n");
    printf("    操作系统给进程轮转分配时间片、音乐播放器循环播放下一首，逻辑完全一样：\n");
    printf("    队列排成环 + 一个游标，永远不用判断『是不是到队尾了』。\n");
    printf("  玩家数超过上限 8 会被拒绝（本节的静态数组撑不住），说明上限存在的意义：\n");
    clist_deal(99, deck, 2);

    /* ============================================================
       第 9 幕（可选）：用错终止条件的现场
       ============================================================ */
    if (show_bug) {
        section("9. --bug：用错终止条件的两种死法（默认运行不跑这一幕）");
        bug_demo_infinite_loop();
        printf("\n");
        bug_demo_real_crash();
        printf("  ★ 父进程活得好好的，退出码还是 0 —— 崩的是被 fork 出去的那个替死鬼。\n");
    } else {
        printf("\n  （想看『用错终止条件会死循环 / 当场段错误』的受控现场，加 --bug 再跑一次）\n");
    }

    /* ============================================================
       收尾：谁 malloc 谁 free
       ============================================================ */
    clist_free(&l);
    printf("\n  clist_free 之后：");
    clist_print(&l);
    printf("  tail 已被置 NULL（%s），再 free 一次也安全（空表直接 return）\n",
           l.tail == NULL ? "不留悬空把手" : "居然还指着东西");
    check_size(&l);

    printf("\n=== 本节结束 ===\n");
    printf("记住五句话：\n");
    printf("  1) 循环单链表 = 单链表 + 一根『尾巴牵回头』的箭头，节点定义一个字都没改。\n");
    printf("  2) 环里没有 NULL：遍历终止条件必须从 p != NULL 换成 p != head，且用 do-while。\n");
    printf("  3) 只存 tail 不存 head：head = tail->next 白捡，于是头插尾插都是 O(1)。\n");
    printf("  4) 删节点仍然要前驱（prev/cur 手拉手），但 tail 天生就是 head 的前驱，省掉一个特判。\n");
    printf("  5) 『天生要绕圈』的问题（约瑟夫环、RR 调度、循环播放）用它写最省心。\n");
    return 0;
}
