#include <stdio.h>
#include <stdlib.h>
#include <string.h>          /* strcmp / strncmp：解析 --bug= 命令行开关 */
#include "skiplist.h"
#include "ordlist.h"

/*
 * 本目录完全自包含：只 include 本目录的 .h 和标准库。
 * （Makefile 按叶子目录独立编译，跨目录 include 会直接失败。）
 *
 * 默认跑 10 幕正常演示。反例演示挂在命令行开关后面，默认一步都不跑：
 *     ./main                正常 10 幕
 *     ./main --bug=flat     反例一：随机分层被写死 → 跳表退化成有序单链表
 *     ./main --bug=dup      反例二：insert 漏判重 → 重复 key 让删除和统计错乱
 */

/* 小工具：打印一幕的小标题（和 00-c-foundation/pointer-basics 同款）*/
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

/*
 * 整数版 log2，只用于"n 恰好是 2 的幂"的场合（第 1 幕的表就是 16/32/.../512）。
 * 为什么不调 math.h 的 log2()：验收命令是 `gcc ... *.c -o xxx`，没有 -lm。
 * 新 glibc 把 libm 并进了 libc 所以大概率也能过，但不该赌；自己写 6 行最稳，
 * 而且"2 翻几次到 n"本来就是 log2 的教科书定义。
 */
static int ilog2_of_pow2(int n)
{
    int k = 0;
    while ((1 << k) < n) {
        k++;
    }
    return k;
}

/*
 * Fisher-Yates 打乱。
 * 为什么要打乱：按 1..N 顺序插，第 0 层每次都插在尾巴上，虽然形状没错，
 * 但"随机写入序"才是真实场景，也免得有人怀疑数字是靠顺序挑出来的。
 * 调用前必须先 srand()，否则顺序不可复现，README 就对不上。
 */
static void shuffle_ints(int *a, int n)
{
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);           /* 在 0..i 里随机挑一个，和 a[i] 交换 */
        int t = a[i];
        a[i] = a[j];
        a[j] = t;
    }
}

/*
 * 数出某个节点到底有几层 —— 只能"逐层走一遍看它在不在"，不能"数到 NULL"。
 *
 * 为什么不能数到 NULL（本节的 ASan 闸门就是在这里逮到我写错的）：
 *   forward 数组长度【恰好】是 层数+1，末尾没有 NULL 终止符，
 *   一路读下去就是数组越界读 → heap-buffer-overflow。
 *   C 语言的指针数组不随身带长度，长度只能由外面的人保管。
 *
 * 这里的"外面的人"就是跳表本身：某一层的链表是完整可走的，
 * 所以逐层走一遍、看这个节点有没有出现在该层上，就绝对安全（而且绝对正确）。
 */
static int node_layer_count(const SkipList *s, const SkNode *target)
{
    int cnt = 0;
    for (int l = 0; l < s->level; l++) {
        SkNode *x = s->head->forward[l];
        while (x != s->nil) {
            if (x == target) {
                cnt++;
                break;
            }
            x = x->forward[l];
        }
    }
    return cnt;
}

/* ==================================================================
 * 反例一：把"抛硬币"钉死成 0 层 —— 跳表立刻退化成有序单链表
 * ================================================================== */
static void bug_demo_flat(void)
{
    section("[反例一] 随机分层被钉死成 0 层：跳表退化成有序单链表");

    printf("  正式写法：sk_insert() 内部调 sk_random_level() 决定层数。\n");
    printf("  反例写法：sk_insert_pinned(&sl, key, 0) —— 每个节点都只有第 0 层。\n");
    printf("  它模拟的是两种很真实的写法事故：\n");
    printf("    ① 有人嫌随机麻烦，把 sk_random_level() 直接改成 return 0（看起来一样能跑）；\n");
    printf("    ② 忘了调 sk_random_level()，新节点层数拿了个没初始化的值，永远是 0。\n\n");

    const int N = 2000;
    SkipList sl;
    sk_init(&sl, 20261003);

    int *keys = malloc(sizeof(int) * (size_t)N);
    if (keys == NULL) {
        fprintf(stderr, "内存不足\n");
        sk_destroy(&sl);
        return;
    }
    for (int i = 0; i < N; i++) {
        keys[i] = i + 1;
    }
    srand(4242);
    shuffle_ints(keys, N);
    for (int i = 0; i < N; i++) {
        sk_insert_pinned(&sl, keys[i], 0);      /* ★ 问题就出在最后这个 0 */
    }

    printf("  灌完 %d 个 key：size = %d，层数 = %d（只剩第 0 层）\n", N, sl.size, sl.level);
    printf("  → 层数 1 就意味着『只有一条链表』，dump 出来的第 0 层就是全部 2000 个 key，\n");
    printf("    和有序单链表【完全一样】，只是每个节点多背了一个指针数组的头。\n");

    int  probes = 200;
    sl.cmp_count = 0;
    for (int i = 0; i < probes; i++) {
        sk_search(&sl, 1 + (i * 10 + 7) % N);
    }
    double avg = (double)sl.cmp_count / (double)probes;

    printf("\n  实测 %d 次查找的平均比较次数：\n", probes);
    printf("    0 层索引（本反例）  ：%.2f 次\n", avg);
    printf("    理论 N/2（单链表）  ：%.2f 次   ← 两者几乎重合，说明它【就是】一条链表\n",
           (double)N / 2.0);
    printf("    正常跳表 2*log2(N)  ：约 22.0 次  ← 差了 %.0f 倍\n", avg / 22.0);
    printf("\n  → 结论：跳表的功劳全在『随机分层』上。索引一没，它就只是一条更占内存的链表。\n");

    sk_destroy(&sl);
    free(keys);
}

/* ==================================================================
 * 反例二：insert 忘了判"key 已存在"
 * ================================================================== */
static void bug_demo_dup(void)
{
    section("[反例二] insert 忘了判重：同一份数据被塞两次");

    printf("  业务里『同一条记录被推了两次』太常见了（重试、消息重复投递、用户连点）。\n");
    printf("  正确写法 sk_insert()：第二次插 5 会返回 false，链表一根线不动。\n");
    printf("  反例写法 sk_insert_dup()：不判重，直接塞。\n\n");

    SkipList sl;
    sk_init(&sl, 20261003);

    int seq[8] = {1, 3, 5, 5, 5, 7, 9, 9};
    for (int i = 0; i < 8; i++) {
        bool ok = sk_insert_dup(&sl, seq[i]);
        printf("    sk_insert_dup(%d) -> %s   size = %d\n",
               seq[i], ok ? "true（插了）" : "false", sl.size);
    }

    printf("\n  dump —— 第 0 层出现三个 5、两个 9，它们是各自独立 malloc 的节点：\n");
    sk_dump(&sl);

    printf("\n  业务说『这条记录撤销一次』，删 5：\n");
    sk_remove(&sl, 5);
    printf("  删一次之后 size = %d（表里还剩两个 5）\n", sl.size);
    sk_dump(&sl);

    printf("\n  再查 5 —— 依然返回存在，但业务已经『撤销』过了：\n");
    bool still = sk_search(&sl, 5);
    printf("    sk_search(5) = %s\n", still ? "true（还在！）" : "false");

    int out[16];
    int cnt = sk_range(&sl, 5, 5, out, 16);
    printf("    sk_range(5,5) 返回 %d 个 → 同一个值被报了好几遍\n", cnt);

    printf("\n  → 后果清单：\n");
    printf("     ① size 和『有几个不同的值』不一致，任何计数口径都错；\n");
    printf("     ② remove 一次只摘一个，『删掉了』和『查不到』变成两件事；\n");
    printf("     ③ 范围查询把同一个值报 N 遍；\n");
    printf("     ④ 想清楚了要『多值』语义，也该在结构里明说（比如加 count 字段），而不是靠沉默的重复。\n");
    printf("     正确做法：sk_insert() 里那句『第一个 >= key 的节点是不是正好等于 key』必须写。\n");

    sk_destroy(&sl);
}

static void run_bug_demo(const char *which)
{
    if (strcmp(which, "flat") == 0) {
        bug_demo_flat();
    } else if (strcmp(which, "dup") == 0) {
        bug_demo_dup();
    } else {
        printf("  未知的反例编号：%s（可选 flat / dup）\n", which);
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
            printf("用法：%s [--bug=flat|dup]\n", argv[0]);
            printf("  不带参数 = 10 幕正常演示（默认，不跑任何反例）\n");
            printf("  --bug=flat 看『忘了随机分层』的下场；--bug=dup 看『insert 漏判重』的下场\n");
            return 0;
        }
        printf("  没认出来的参数：%s（本节只认 --bug=flat / --bug=dup / --help）\n", argv[i]);
        return 0;
    }

    /* ============================================================
       第 1 幕：出发线 —— 有序单链表查找为什么必须 O(n)
       ============================================================ */
    section("1. 出发线：有序单链表查找，一格一格走的痛");

    printf("  数据【有序】了，查找能快一点吗？单链表的答案：几乎不能。\n");
    printf("  下面这条链表完全有序，找 13 只能这么走：\n\n");

    OrdList ol;
    ol_init(&ol);
    for (int i = 1; i <= 31; i += 2) {       /* 1 3 5 ... 31，共 16 个 */
        ol_append(&ol, i);
    }
    printf("    有序链表：");
    ol_print(&ol);
    ol.cmp_count = 0;
    bool hit = ol_search(&ol, 13);
    printf("    ol_search(13) = %s，比较次数 = %ld\n", hit ? "true（找到）" : "false", ol.cmp_count);
    printf("    （1 3 5 7 9 11 之后才轮到 13 —— 前面每一个都得看一眼，才知道不是它）\n");

    printf("\n  ★ 关键认知：二分查找靠的是 a[mid] 这种 O(1) 随机访问，链表给不了。\n");
    printf("    链表只有 next，站在节点上只能问『下一个是不是』，所以只能一格一格走。\n");
    printf("    有序能给的唯一好处是『提前刹车』（看到比目标大的就停），平均仍是 n/2 步。\n");

    printf("\n  把数据量翻倍，看代价怎么跟着翻倍（每种规模各查 32 个存在的 key 取平均）：\n");
    printf("         n     单链表平均比较     跳表平均比较     log2(n)\n");
    printf("        ----     ------------     ------------     --------\n");
    /* 下面这几对数字是给第 2 幕用的：所有结论都用实测值，不靠"我记得大概是 7 次" */
    double skip_at_16 = 0.0, skip_at_512 = 0.0;
    double list_at_32 = 0.0, skip_at_32 = 0.0, list_at_512 = 0.0;
    for (int n = 16; n <= 512; n *= 2) {
        OrdList t;
        ol_init(&t);
        for (int i = 1; i <= n; i++) {
            ol_append(&t, i);
        }
        t.cmp_count = 0;
        for (int i = 0; i < 32; i++) {
            ol_search(&t, 1 + (i * n) / 32);  /* 探测点摊到整条链上：1, 1+n/32, 1+2n/32, ... */
        }
        double list_avg = (double)t.cmp_count / 32.0;
        ol_destroy(&t);

        SkipList s;
        sk_init(&s, 20261003);               /* 固定种子 → 这一行数字每次运行都一模一样 */
        for (int i = 1; i <= n; i++) {
            sk_insert(&s, i);
        }
        s.cmp_count = 0;
        for (int i = 0; i < 32; i++) {
            sk_search(&s, 1 + (i * n) / 32);
        }
        double skip_avg = (double)s.cmp_count / 32.0;
        sk_destroy(&s);

        if (n == 16) {
            skip_at_16 = skip_avg;
        }
        if (n == 32) {
            list_at_32 = list_avg;
            skip_at_32 = skip_avg;
        }
        if (n == 512) {
            list_at_512 = list_avg;
            skip_at_512 = skip_avg;
        }
        printf("      %5d        %8.2f         %8.2f          %4d\n",
               n, list_avg, skip_avg, ilog2_of_pow2(n));
    }
    printf("        ↑ 左列：n 翻倍，比较次数就跟着翻倍 —— 这就是 O(n) 的现场证据。\n");
    printf("          中列：n 从 16 涨到 512（32 倍），比较次数只从 %.2f 涨到 %.2f —— 这才是 O(log n)。\n",
           skip_at_16, skip_at_512);
    printf("          理论值不是 log2(n) 而是 2*log2(n)：n=512 时 2*9 = 18.00，实测 %.2f，对得上。\n",
           skip_at_512);
    printf("          n 小的时候中列偏大，因为『从最高层一路沉到第 0 层』是固定开销，摊不平（第 2 幕现场演示）。\n");
    ol_destroy(&ol);

    /* ============================================================
       第 2 幕：治疗办法 —— 给链表加多层索引
       ============================================================ */
    section("2. 治疗办法：给有序链表加多层索引，让它【跳着走】");

    printf("  思路一句话：在【部分】节点上多挂几根指针，形成上层的『高速公路』。\n");
    printf("  查找时先在高层大步跨，跨不动了就下沉一层小步走，永远不回头。\n\n");
    printf("  下面是同一批数据 {1,3,5,...,31} 建成跳表后的样子（真实运行结果，不是我手画的）：\n\n");

    SkipList sl;
    sk_init(&sl, 20261003);                  /* ★ 固定种子：dump 的形状因此完全可复现 */
    for (int i = 1; i <= 31; i += 2) {
        sk_insert(&sl, i);
    }
    sk_dump(&sl);

    printf("\n  现在再找 13，把查找路径打印出来：\n");
    sl.trace = true;
    sl.cmp_count = 0;
    hit = sk_search(&sl, 13);
    long skip_cmp13 = sl.cmp_count;          /* 先把实测数字抓下来，trace 一关就没了 */
    sl.trace = false;

    OrdList odd;
    ol_init(&odd);
    for (int i = 1; i <= 31; i += 2) {
        ol_append(&odd, i);
    }
    odd.cmp_count = 0;
    bool ol_hit = ol_search(&odd, 13);
    long ol_cmp13 = odd.cmp_count;
    ol_destroy(&odd);

    printf("\n  两边【实测】并排看（同一批 16 个节点，同一个 key=13）：\n");
    printf("    有序单链表：结果 %s，%ld 次比较        跳表：结果 %s，%ld 次比较\n",
           ol_hit ? "找到" : "没找到", ol_cmp13, hit ? "找到" : "没找到", skip_cmp13);
    printf("  先别急着喊『跳表赢了』—— 16 个节点时它赢得很难看，因为：\n");
    printf("    ① 它必须从最高层（这里是第 %d 层）一层层沉到第 0 层，每层至少『看一眼下一个』，\n", sl.level - 1);
    printf("       这一笔固定开销约等于『层数』，节点少的时候根本摊不平；\n");
    printf("    ② 上层太稀，一眼就看完了，没省下几步。\n");
    printf("  所以结论只能靠规模说话（还是第 1 幕那张表，同一套探测口径）：\n");
    printf("    n =  32：单链表 %.2f 次  vs  跳表 %.2f 次  → 跳表只花 %.0f%% 的比较次数\n",
           list_at_32, skip_at_32, 100.0 * skip_at_32 / list_at_32);
    printf("    n = 512：单链表 %.2f 次  vs  跳表 %.2f 次  → 跳表只花 %.0f%% 的比较次数\n",
           list_at_512, skip_at_512, 100.0 * skip_at_512 / list_at_512);
    printf("  跳表的价值不在『小表更快』，而在『表越大越几乎不变快』—— 下一幕先把节点结构拆开。\n");

    /* ============================================================
       第 3 幕：解剖节点结构 —— forward 是"指针的数组"
       ============================================================ */
    section("3. 解剖 SkNode：forward 是『指针的数组』，不是『数组的指针』");

    SkNode *one = sl.head->forward[0];       /* 第 0 层的第一个真节点 */
    int one_layers = node_layer_count(&sl, one);
    printf("  节点定义：\n");
    printf("      typedef struct SkNode { int key; struct SkNode **forward; } SkNode;\n\n");
    printf("  sizeof 不会骗人：\n");
    printf("      sizeof(SkNode)         = %zu 字节   （int 4 + 补白 4 + 指针 8）\n", sizeof(SkNode));
    printf("      sizeof(one->forward)   = %zu 字节   ← forward 本身只是【一个指针】\n",
           sizeof(one->forward));
    printf("      sizeof(*one->forward)  = %zu 字节   ← 数组里【一个格子】是一个 SkNode*\n",
           sizeof(*one->forward));
    printf("      所以 forward + 1 只往前走 %zu 字节 = 一个格子；\n", sizeof(SkNode *));
    printf("      而 forward[l] 拿到的东西类型是 SkNode* —— 一句人话：\n");
    printf("      『第 l 层的下一个节点在哪』。\n");

    printf("\n  画出来（one 的 key = %d，它一共 %d 层）：\n", one->key, one_layers);
    printf("      one ──► ┌───────────┬──────────────┐\n");
    printf("              │ key = %3d │ forward ──────┼──┐   ← forward 只是一个指针\n", one->key);
    printf("              └───────────┴──────────────┘  │\n");
    printf("                                            ▼\n");
    printf("                    forward 指着的这块内存 = 【指针的数组】\n");
    printf("                    长度 = 这个节点的层数 = 最高层号 + 1（一格都不能多）\n");
    printf("                    ┌────────────┬────────────┬────────────┐\n");
    printf("                    │ forward[0] │ forward[1] │ forward[2] │ …\n");
    printf("                    └─────┬──────┴─────┬──────┴─────┬──────┘\n");
    printf("                          ▼            ▼            ▼\n");
    printf("                   第 0 层后继   第 1 层后继   第 2 层后继  （每个格子里装的是地址）\n");
    printf("                    实测值：");
    for (int l = 0; l < one_layers; l++) {   /* ★ 只读到【真实长度】为止，再多读一格就是越界 */
        if (one->forward[l] == sl.nil) {
            printf("forward[%d]=NIL  ", l);
        } else {
            printf("forward[%d]=%d  ", l, one->forward[l]->key);
        }
    }
    printf("\n");

    {
        SkNode *arr[4];
        SkNode *(*p)[4] = &arr;              /* p 才是『数组的指针』：指向一整块长 4 的数组 */

        printf("\n  【对照】什么叫『数组的指针』（本节【不】用这种写法）：\n");
        printf("      SkNode *arr[4];   SkNode *(*p)[4] = &arr;\n");
        printf("      sizeof(p)   = %zu 字节（它自己也是个指针）\n", sizeof(p));
        printf("      sizeof(*p)  = %zu 字节（它指向的是【一整块 4 个指针的数组】）\n", sizeof(*p));
        printf("      p + 1 往前走 %ld 字节 —— 一步跨过【整块数组】\n",
               (long)((char *)(p + 1) - (char *)p));
        printf("      one->forward + 1 往前走 %ld 字节 —— 一步只跨【一个格子】\n",
               (long)sizeof(SkNode *));
        printf("\n      ★ 两者取第 l 个元素都能写 forward[l] / (*p)[l]，所以极容易看漏；\n");
        printf("        差别全在 sizeof 和 +1 走多远上。看到 ** 先问一句：数组里装的到底是啥？\n");
    }

    printf("\n  ★ 为什么用『指针的数组』，而不是干脆定长 %d 格？\n", SK_MAX_LEVEL);
    printf("    因为层数是抛硬币随机决定的：多数节点 1 层，少数 2 层，极个别 5 层以上。\n");
    printf("    定长 = 每个节点都付 %d 字节的指针钱，而平均只用 %d 字节 → 白扔一倍内存。\n",
           SK_MAX_LEVEL * (int)sizeof(SkNode *), 2 * (int)sizeof(SkNode *));
    printf("    所以 forward 按实际层数现场 malloc。代价必须记住：\n");
    printf("    每个节点【两次 malloc】（本体 + 指针数组）→ sk_destroy 里就得【两次 free】，\n");
    printf("    少 free 任何一个，ASan 立刻报泄漏。这也是本节最容易写错的一行。\n");

    /* ============================================================
       第 4 幕：insert —— 查找路径顺手给出插入位置
       ============================================================ */
    section("4. insert：为什么『下降』顺便就把 update[] 给你了");

    printf("  插入需要知道的是：【每一层】新节点该插在谁后面 —— 这个数组就叫 update[]。\n");
    printf("  而查找在每层停住的位置，定义上就是『这一层最后一个比目标小的节点』= 前驱。\n");
    printf("  所以插入 = 一次查找 + 每层改两根线。看现场（trace 已打开）：\n");
    sl.trace = true;
    printf("\n  ▶ 往第 2 幕那张表里插 14（它不在表里）：\n");
    bool ins = sk_insert(&sl, 14);
    sl.trace = false;
    printf("      sk_insert(14) = %s，size = %d\n", ins ? "true" : "false", sl.size);
    printf("\n  插完之后的多层结构（注意 14 出现在它被抽到的那几层上）：\n");
    sk_dump(&sl);

    printf("\n  ▶ 再插一次 14（已存在）：\n");
    sl.trace = true;
    ins = sk_insert(&sl, 14);
    sl.trace = false;
    printf("      sk_insert(14) = %s ← 判重挡住了。漏了这一句就是反例二那堆破事。\n",
           ins ? "true" : "false（已存在，不重复插）");

    /* ============================================================
       第 5 幕：search 的查找路径 —— 亲眼看到"跳"
       ============================================================ */
    section("5. search：把『跳』的过程逐层打印出来");

    sl.trace = true;
    printf("  ▶ 查一个存在的 key = 21：\n");
    sl.cmp_count = 0;
    hit = sk_search(&sl, 21);
    printf("      结果 = %s，比较 %ld 次（表里 %d 个节点，换成有序链表最坏要 %d 次）\n\n",
           hit ? "找到" : "没找到", sl.cmp_count, sl.size, sl.size);

    printf("  ▶ 查一个不存在的 key = 22：\n");
    sl.cmp_count = 0;
    hit = sk_search(&sl, 22);
    printf("      结果 = %s，比较 %ld 次\n",
           hit ? "找到" : "没找到（第 0 层往前看一眼，不是它）", sl.cmp_count);
    sl.trace = false;

    printf("\n  看懂这三点才算真会了：\n");
    printf("    ① 每一层只有『往前走』，从不回头 —— 路径是单调的，这是正确性的根。\n");
    printf("    ② 下沉时我是【站在上一层停住的那个节点上】继续走，所以不会漏掉任何区间。\n");
    printf("    ③ 最终判定发生在第 0 层的『往前看一眼』，所以尾哨兵 NIL 必须在场兜底。\n");

    /* ============================================================
       第 6 幕：remove + 边界
       ============================================================ */
    section("6. remove：和 insert 完全对称，摘线 + 收缩层数");

    sl.trace = true;
    printf("  ▶ 删掉 14：\n");
    bool del = sk_remove(&sl, 14);
    printf("      sk_remove(14) = %s，size = %d\n\n", del ? "true" : "false", sl.size);

    printf("  ▶ 删一个不存在的 99：\n");
    del = sk_remove(&sl, 99);
    printf("      sk_remove(99) = %s，size = %d（一根线都没动）\n\n",
           del ? "true" : "false（表里没有）", sl.size);

    printf("  ▶ 删掉第 0 层上的第一个节点 1（哨兵在场 → 不用特判『删的是头』）：\n");
    del = sk_remove(&sl, 1);
    sl.trace = false;
    printf("      sk_remove(1) = %s\n", del ? "true" : "false");
    sk_dump(&sl);

    printf("\n  ▶ 把剩下的全删光，看层数怎么自己降回来：\n");
    for (int i = 1; i <= 31; i++) {
        sk_remove(&sl, i);
    }
    printf("      删空之后：size = %d，层数 = %d（最高层被删空就要收缩，否则查找每年空层白问一句）\n",
           sl.size, sl.level);
    sk_dump(&sl);

    printf("\n  ▶ 空表上继续查 / 删，必须都安全：\n");
    hit = sk_search(&sl, 1);
    del = sk_remove(&sl, 1);
    ins = sk_insert(&sl, 42);
    printf("      sk_search(1)=%s  sk_remove(1)=%s  然后 sk_insert(42)=%s，size=%d，层数=%d\n",
           hit ? "true" : "false", del ? "true" : "false", ins ? "true" : "false",
           sl.size, sl.level);
    sk_dump(&sl);
    sk_destroy(&sl);
    printf("      （sl 已 destroy，下面各幕另开新表，互不干扰）\n");

    /* ============================================================
       第 7 幕：抛硬币到底抛出个什么分布
       ============================================================ */
    section("7. 随机分层的真面目：层数直方图");

    printf("  sk_random_level() 每次 50%% 概率再升一层，理论上：\n");
    printf("    第 0 层有 n 个节点，第 1 层 n/2，第 2 层 n/4 …… 每上一层少一半。\n");
    printf("  连抽 4096 次数一数（用一个新表把随机流重新播种，保证可复现）：\n\n");

    SkipList rsl;
    sk_init(&rsl, 20261003);
    int  hist[SK_MAX_LEVEL] = {0};
    int  draws = 4096;
    int  max_draw = 0;
    for (int i = 0; i < draws; i++) {
        int lv = sk_random_level();
        hist[lv]++;
        if (lv > max_draw) {
            max_draw = lv;
        }
    }
    printf("      层号    实测次数    实测占比    理论占比 (1/2)^(l+1)\n");
    printf("      ------   ---------   ---------   ------------------\n");
    for (int l = 0; l <= max_draw; l++) {
        double got  = (double)hist[l] / (double)draws;
        double want = 1.0 / (double)(1 << (l + 1));
        printf("        %2d     %7d      %6.2f%%       %6.2f%%\n",
               l, hist[l], got * 100.0, want * 100.0);
    }
    printf("      本次抽样最高层号 = %d（上限 MAX_LEVEL-1 = %d，几乎碰不到）\n",
           max_draw, SK_MAX_LEVEL - 1);
    printf("      → 实测逐层减半，和理论 (1/2)^(l+1) 对上了。这就是『期望 O(log n)』的发动机。\n");
    sk_destroy(&rsl);

    /* ============================================================
       第 8 幕：N = 2000 复杂度实测大表（本节说服力来源）
       ============================================================ */
    section("8. 复杂度实测：N=2000，跳表 vs 有序单链表（真数字）");

    const int N = 2000;
    int *keys = malloc(sizeof(int) * (size_t)N);
    if (keys == NULL) {
        fprintf(stderr, "内存不足，第 8 幕跳过\n");
        return 1;
    }
    for (int i = 0; i < N; i++) {
        keys[i] = i + 1;
    }

    SkipList bsl;
    sk_init(&bsl, 20261003);                  /* 先建表（内部 srand），再换种子打乱 */
    srand(4242);
    shuffle_ints(keys, N);                    /* 打乱顺序插入 = 真实写入序 */

    long cmp_before_insert = bsl.cmp_count;
    for (int i = 0; i < N; i++) {
        sk_insert(&bsl, keys[i]);
    }
    double ins_avg = (double)(bsl.cmp_count - cmp_before_insert) / (double)N;

    OrdList big;
    ol_init(&big);
    for (int i = 1; i <= N; i++) {
        ol_append(&big, i);                   /* 有序链表按升序灌，内容完全相同 */
    }

    int probes = 200;
    bsl.cmp_count = 0;
    big.cmp_count = 0;
    int found_skip = 0, found_list = 0;
    for (int i = 0; i < probes; i++) {
        int k = 1 + (i * 10 + 7) % N;          /* 200 个【确实存在】的 key，均匀撒在 1..N */
        if (sk_search(&bsl, k)) {
            found_skip++;
        }
        if (ol_search(&big, k)) {
            found_list++;
        }
    }
    double skip_avg = (double)bsl.cmp_count / (double)probes;
    double list_avg = (double)big.cmp_count / (double)probes;

    long slots = sk_total_forward_slots(&bsl);
    double space_ratio = (double)slots / (double)bsl.size;

    printf("  两张表内容完全相同（1 .. %d 共 %d 个 key），查同样 %d 个存在的 key：\n\n", N, N, probes);
    printf("      指标                                有序单链表        跳表\n");
    printf("      --------------------------------   ------------   ------------\n");
    printf("      实测平均比较次数（%d 次取均）      %12.2f  %12.2f\n", probes, list_avg, skip_avg);
    printf("      命中个数                           %12d  %12d\n", found_list, found_skip);
    printf("      理论期望                           %12.1f  %12.1f\n", (double)N / 2.0, 22.0);
    printf("      加速比（链表比较数 / 跳表比较数）              %16.1f 倍\n", list_avg / skip_avg);
    printf("      当前层数                                     %12d  （log2(2000) 约 11）\n", bsl.level);
    printf("\n  理论对照 —— 为什么跳表那一格是 22 而不是 11：\n");
    printf("      2^10 = 1024 < 2000 <= 2048 = 2^11   →   log2(N) ≈ 11（这是【层数】）\n");
    printf("      跳表期望比较次数 = (1/p) * log2(N)，p = 1/2  →  2 * 11 ≈ 22 次\n");
    printf("      实测 %.2f 次，贴着 2*log2(N)：每层平均走 2 步 × 走 11 层。\n", skip_avg);
    printf("      （别看到 22 > 11 就说『比理论差』：log2(N) 只数了层数，每层的常数步数是 1/p。\n");
    printf("        常数因子不影响 O(log n) 的结论，但报数字时得报真实的。）\n");
    printf("\n  插入侧：打乱顺序灌 %d 个，每次插入也要走一遍下降 → 平均比较 %.2f 次\n", N, ins_avg);
    printf("  建表成本：跳表 O(N log N) 期望；有序链表用 tail 把手顺序灌是 O(N)（乱序插就是 O(N^2)）。\n");
    printf("  空间实测：指针格子总数 %ld / 节点数 %d = %.2f 个指针每节点（期望值 2.00）\n",
           slots, bsl.size, space_ratio);
    printf("      → 印证『跳表空间是链表的期望 2 倍』：第 l 层有 n/2^l 个节点，1 + 1/2 + 1/4 + … = 2。\n");
    printf("        有序链表每节点 1 根指针；跳表平均 2 根，而且每节点多一次 malloc。\n");

    /* ============================================================
       第 9 幕：范围查询 —— 跳表对平衡树的头号优势
       ============================================================ */
    section("9. 范围查询：定位一次，然后顺着第 0 层走");

    int out[64];
    int cnt = sk_range(&bsl, 400, 420, out, 64);
    printf("  sk_range(400, 420) 命中 %d 个：", cnt);
    for (int i = 0; i < cnt; i++) {
        printf("%d ", out[i]);
    }
    printf("\n");

    cnt = sk_range(&bsl, 1990, 5000, out, 64);
    printf("  sk_range(1990, 5000)（上界超出最大值）命中 %d 个：", cnt);
    for (int i = 0; i < cnt; i++) {
        printf("%d ", out[i]);
    }
    printf("\n");

    cnt = sk_range(&bsl, 100, 99, out, 64);
    printf("  sk_range(100, 99)（lo > hi，空区间）命中 %d 个 → 不崩，返回 0\n", cnt);

    cnt = sk_range(&bsl, 700, 700, out, 64);
    printf("  sk_range(700, 700)（单点区间）命中 %d 个：%d\n", cnt, cnt > 0 ? out[0] : -1);

    cnt = sk_range(&bsl, 1, 10000, out, 5);
    printf("  sk_range(1, 10000) 但缓冲区只有 5 格 → 命中 %d 个（截断，不越界写）\n", cnt);

    printf("\n  为什么这么便宜：定位 lo 花 O(log n)，之后沿【第 0 层】一路 next 到 hi 为止，\n");
    printf("  每步 O(1)，一共 k 步 → 总计 O(log n + k)。平衡树要中序遍历 + 递归栈回溯，\n");
    printf("  红黑树还得处理删除时的再平衡；跳表这里就是一句 while (cur->key <= hi)。\n");

    /* ============================================================
       第 10 幕：收摊
       ============================================================ */
    section("10. 收尾：几个 sk_init 就要几个 sk_destroy");

    printf("  本幕要还的：bsl（跳表，%d 个节点）、big（有序链表，%d 个节点）、keys 数组。\n",
           bsl.size, big.size);
    printf("  第 1、7 幕里那些临时表都在循环内/幕末各自 destroy 掉了，一个不漏。\n");
    sk_destroy(&bsl);
    printf("    bsl  已 destroy（每个节点 free 两次：指针数组 + 节点本体；两个哨兵也各两次）\n");
    ol_destroy(&big);
    printf("    big  已 destroy（连哨兵一起 free）\n");
    free(keys);
    keys = NULL;
    printf("    keys 数组已 free 并置 NULL\n");

    sk_destroy(&bsl);                          /* 重复销毁：判到 head == NULL 直接返回 */
    ol_destroy(&big);
    printf("    再 destroy 一次也没崩：head 已是 NULL，函数第一件事就是判它\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住五句话：\n");
    printf("  1) 有序 ≠ 能跳。单链表只有 next，二分要靠 O(1) 随机访问，所以只能 O(n) 一格一格走。\n");
    printf("  2) 跳表 = 在部分节点上多挂几层索引；每上一层少一半节点 → 层数 log2(n) → 期望 O(log n)。\n");
    printf("  3) forward 是【指针的数组】：forward[l] = 第 l 层后继。层数随机 → 长度随机 → 每节点两次 malloc，也必须两次 free。\n");
    printf("  4) 查找每层停住的位置正好是插入位置，所以 insert / search / remove 共用同一段『下降 + update[]』。\n");
    printf("  5) 代价：空间期望翻倍、随机化意味着最坏仍是 O(n)、以及『要复现就必须固定 srand 种子』。\n");
    return 0;
}
