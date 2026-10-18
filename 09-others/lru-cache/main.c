#include <stdio.h>
#include "lru.h"

/*
 * ============================================================================
 * LRU 缓存演示：分幕把每一步状态打印出来。
 *
 *   第 1 幕  为什么"哈希表 + 双向链表"两个都少不了（把两种痛各测一遍）
 *   第 2 幕  看一眼真实内存布局：桶数组 + 一条双向链表 + 两个哨兵
 *   第 3 幕  LeetCode 146 官方示例序列，capacity=2，逐步打印链表和淘汰者
 *   第 4 幕  桶数开小了会怎样：哈希表退化成一条链，O(1) 变 O(n)
 *   第 5 幕  LRU 和 FIFO 差在哪：就差"get 之后要不要提到最前"这一行
 *   第 6 幕  边界情况：capacity=1、put 更新老 key、key=0 和负数 key、未命中
 *   收尾     申请/释放次数配不配平（ASan 会再查一遍）
 *
 * 所有数据都是**写死的**，输出可以一字不差地复核。
 * ============================================================================
 */

static void section(const char *title)
{
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n");
}

/* 只走链表、不用哈希表：找 key 要比几个节点（这就是"只有链表"的代价） */
static int find_by_list_only(const LRUCache *c, int key, long long *steps_out)
{
    long long steps = 0;
    for (const LRUNode *p = c->head.next; p != &c->tail; p = p->next) {
        steps++;
        if (p->key == key) {
            if (steps_out != NULL) {
                *steps_out = steps;
            }
            return p->value;
        }
    }
    if (steps_out != NULL) {
        *steps_out = steps;
    }
    return -1;
}

int main(void)
{
    /* ============================================================
       第 1 幕：两种"只用一种结构"的痛，各测一次
       ============================================================ */
    section("第 1 幕：为什么必须两件套 —— 只用哈希表 / 只用链表各是什么下场");

    LRUCache c1;
    lru_create(&c1, 6);
    for (int i = 0; i < 6; i++) {
        lru_put(&c1, 101 + i, (101 + i) * 10);      /* key = 101..106 */
    }
    /* 让 101 变成"最久没被用"：其它五个都在后面被摸过 */
    for (int i = 1; i < 6; i++) {
        (void)lru_get(&c1, 101 + i);
    }
    lru_print(&c1, "装好 6 个条目、然后把 102~106 各查一遍之后的状态：");

    printf("\n      现在要淘汰人了，问「谁最久没被用过」：\n");
    long long checks = 0;
    int naive = lru_naive_find_oldest(&c1, &checks);
    printf("      · 只有哈希表（没有链表）：只能把所有桶全扫一遍比 stamp ⇒ 扫了 %lld 个条目，"
           "答案是 key=%d\n", checks, naive);
    printf("        这就是 O(n)：缓存越大越慢，而缓存恰恰就是为了大而存在的\n");
    printf("      · 有双向链表：直接看 tail.prev ⇒ 0 次比较，答案是 key=%d\n", c1.tail.prev->key);
    printf("      · 两个答案 %s ⇒ 链表维护的顺序是真的「按新旧排」的，没写错\n",
           (naive == c1.tail.prev->key) ? "一致 ✔" : "不一致 ✘");

    printf("\n      反过来，问「key=101 的值是多少」：\n");
    long long list_steps = 0;
    long long hash_steps = 0;
    int by_list = find_by_list_only(&c1, 101, &list_steps);
    int by_hash = lru_peek(&c1, 101);
    hash_steps = c1.bucket_steps;
    printf("      · 只有链表（没有哈希表）：从头上一个个比 ⇒ 比了 %lld 个节点才找到（值 %d）\n",
           list_steps, by_list);
    printf("      · 有哈希表：算一次桶号 + 顺着那条链 ⇒ 只比了 %lld 个（值 %d）⇒ O(1)\n",
           hash_steps, by_hash);
    printf("      ⇒ 结论：**哈希表管「在哪」（回答定位问题），双向链表管「新旧」（回答该淘汰谁）**，\n");
    printf("        两边指向的是同一个节点，所以没有第二份数据要同步。\n");

    /* ============================================================
       第 2 幕：真实布局
       ============================================================ */
    section("第 2 幕：这个结构在内存里到底长什么样（桶数组 + 一条双向链表 + 两个哨兵）");

    printf("      c1 的桶表（桶号就是 key 经过 hash 取模的结果）：\n");
    lru_print_buckets(&c1);
    printf("\n      画成图（key=101..106 分布在哪些桶、链表按什么顺序串着它们）：\n");
    printf("        buckets[%d 个]                双向链表（head、tail 是哨兵，不存数据）\n",
           c1.bucket_count);
    printf("        ┌────┬────┬────┬…┐          head ⇄ [106] ⇄ [105] ⇄ … ⇄ [101] ⇄ tail\n");
    printf("        │ ·  │103 │ ·  │ │           ↑最新             ↑最旧 = 下一个淘汰\n");
    printf("        └────┴─┬──┴────┴─┘           每次 get/put 命中：把这个节点摘下来挂到 head 后面\n");
    printf("               │                     容量满了：tail.prev 直接就是答案，不用扫\n");
    printf("               └→ 指向链表节点本体（不是拷贝！）\n");
    lru_check_order(&c1);

    /* ============================================================
       第 3 幕：LeetCode 146 官方序列
       ============================================================ */
    section("第 3 幕：LeetCode 146 官方示例，capacity = 2（标准答案逐个对）");

    LRUCache cache;
    lru_create(&cache, 2);
    printf("      官方序列：put(1,1) put(2,2) get(1)=1 put(3,3)[淘汰2] get(2)=-1\n");
    printf("                put(4,4)[淘汰1] get(1)=-1 get(3)=3 get(4)=4\n\n");

    struct {
        int is_get;
        int key;
        int value;        /* put 时用；get 时忽略 */
        int expect;       /* get 的官方期望值；put 用 -2 表示不校验 */
    } ops[] = {
        {0, 1, 1, -2},
        {0, 2, 2, -2},
        {1, 1, 0,   1},   /* 官方：返回 1 */
        {0, 3, 3, -2},    /* 官方：淘汰 key 2 */
        {1, 2, 0,  -1},   /* 官方：返回 -1（已被淘汰） */
        {0, 4, 4, -2},    /* 官方：淘汰 key 1 */
        {1, 1, 0,  -1},   /* 官方：返回 -1 */
        {1, 3, 0,   3},   /* 官方：返回 3 */
        {1, 4, 0,   4},   /* 官方：返回 4 */
    };
    const int nops = (int)(sizeof(ops) / sizeof(ops[0]));

    int wrong = 0;
    for (int i = 0; i < nops; i++) {
        char tag[96];
        if (ops[i].is_get) {
            snprintf(tag, sizeof(tag), "第 %d 步  get(%d)", i + 1, ops[i].key);
            lru_print(&cache, tag);
            int got = lru_get(&cache, ops[i].key);
            const char *mark = (got == ops[i].expect) ? "✔ 与官方一致" : "✘ 与官方不符！";
            if (got != ops[i].expect) {
                wrong++;
            }
            printf("        ⇒ 返回 %d（官方期望 %d）%s\n", got, ops[i].expect, mark);
        } else {
            snprintf(tag, sizeof(tag), "第 %d 步  put(%d, %d)", i + 1, ops[i].key, ops[i].value);
            lru_print(&cache, tag);
            lru_put(&cache, ops[i].key, ops[i].value);
            if (cache.last_evicted) {
                printf("        ⇒ 存进去了；容量已满 ⇒ **淘汰了 key=%d**（它当时正是 tail.prev）\n",
                       cache.last_evicted_key);
            } else {
                printf("        ⇒ 存进去了，还没满，谁也没淘汰\n");
            }
        }
        lru_print(&cache, "     这一步做完：");
        lru_check_order(&cache);
        long long chk = 0;
        int naive_old = lru_naive_find_oldest(&cache, &chk);
        printf("        对照：扫全表找最旧 = key=%d（扫 %lld 个）｜ 链表 tail.prev = key=%d ⇒ %s\n",
               naive_old, chk, cache.tail.prev->key,
               naive_old == cache.tail.prev->key ? "一致 ✔" : "不一致 ✘");
        printf("\n");
    }
    printf("      LeetCode 146 九步全部核对：不一致 %d 步 ⇒ %s\n", wrong,
           wrong == 0 ? "和官方输出完全一致 ✔" : "对不上，回去查代码！");

    /* ============================================================
       第 4 幕：哈希表退化
       ============================================================ */
    section("第 4 幕：桶数开太小会怎样 —— 哈希表退化成一条链，O(1) 变 O(n)");

    static const int wk[6] = {7, 14, 21, 28, 35, 42};

    LRUCache good;
    lru_create(&good, 6);                       /* 正常：桶数 = 2*6+1 = 13 */
    for (int i = 0; i < 6; i++) {
        lru_put(&good, wk[i], i + 1);
    }
    printf("      正常配置（容量 6 ⇒ 13 个桶）：\n");
    lru_print_buckets(&good);
    long long best_probe = 0;
    for (int i = 0; i < 6; i++) {
        (void)lru_peek(&good, wk[i]);
        if (good.bucket_steps > best_probe) {
            best_probe = good.bucket_steps;
        }
    }
    printf("      ⇒ 逐个查这 6 个 key，最坏的一次比了 %lld 个节点\n\n", best_probe);

    LRUCache bad;
    lru_create(&bad, 6);
    bad.bucket_count = 3;                       /* ⚠ 教学用：正常代码别硬改字段 */
    printf("      故意把桶数改成 3（比条目数还少）：\n");
    for (int i = 0; i < 6; i++) {
        lru_put(&bad, wk[i], i + 1);
    }
    lru_print_buckets(&bad);
    long long worst_probe = 0;
    for (int i = 0; i < 6; i++) {
        (void)lru_peek(&bad, wk[i]);
        if (bad.bucket_steps > worst_probe) {
            worst_probe = bad.bucket_steps;
        }
    }
    printf("      ⇒ 同样 6 个 key，最坏的一次要比 %lld 个节点\n", worst_probe);
    printf("      ⇒ 结论：桶要开得比条目多（本实现默认 2×容量+1）。桶数 ↓ ⇒ 拉链 ↑ ⇒ 查找变慢；\n");
    printf("        极端情况（所有 key 撞同一桶）就是「哈希表退化成一维数组」，\n");
    printf("        LRU 名义上 O(1)、实际上 O(n)。这就是哈希表负载因子要 < 1 的原因。\n");

    /* ============================================================
       第 5 幕：LRU vs FIFO
       ============================================================ */
    section("第 5 幕：LRU 和 FIFO 就差一行代码 —— get 之后提不提到最前");

    printf("      同一串操作分别喂给两个缓存（容量都是 3）：\n");
    printf("        put(1) put(2) put(3) get(1) get(2) get(3) 然后 put(4) —— 谁会被害？\n");
    printf("      最后三句 get 说明 1、2、3 刚刚全被用过，按 LRU 的「道理」谁都不该被扔；\n");
    printf("      但 LRU 只能扔一个，扔的是「最久没被用」的那个 = 最后一次 get 最早的人。\n\n");

    LRUCache lru_side;
    LRUCache fifo_side;
    lru_create(&lru_side, 3);
    lru_create(&fifo_side, 3);
    lru_set_promote_on_get(&fifo_side, 0);      /* 关掉提升 ⇒ 退化成 FIFO */

    static const int seq_get[3] = {1, 2, 3};
    lru_put(&lru_side, 1, 10);
    lru_put(&fifo_side, 1, 10);
    lru_put(&lru_side, 2, 20);
    lru_put(&fifo_side, 2, 20);
    lru_put(&lru_side, 3, 30);
    lru_put(&fifo_side, 3, 30);
    for (int i = 0; i < 3; i++) {
        (void)lru_get(&lru_side, seq_get[i]);
        (void)lru_get(&fifo_side, seq_get[i]);
    }
    printf("      三个都查过之后：\n");
    lru_print(&lru_side, "  LRU（get 会提到最前）：");
    lru_print(&fifo_side, "  FIFO（get 不提，顺序还是插入顺序）：");
    printf("\n      现在 put(4, 40)，容量只有 3 ⇒ 必须淘汰一个：\n");
    lru_put(&lru_side, 4, 40);
    lru_put(&fifo_side, 4, 40);
    lru_print(&lru_side, "  LRU：");
    lru_print(&fifo_side, "  FIFO：");
    printf("      ⇒ LRU 淘汰了 key=%d（它最后被用到的时刻最早），FIFO 淘汰了 key=%d（它最早插进来）\n",
           lru_side.last_evicted_key, fifo_side.last_evicted_key);
    printf("      ⇒ 看 FIFO 那行的自洽检查为什么「跳过」：它的位置已经不是按新旧排的了。\n");
    printf("        也就是说：**没有「访问即变最新」这一行，你就不是 LRU**，\n");
    printf("        而链表也失去了「tail.prev 就是最久未用」这条性质 ⇒ 只能回去扫全表。\n");
    printf("\n      再验证一次：FIFO 模式下「扫表找最旧」和「链表尾巴」答案不一样：\n");
    long long f_chk = 0;
    int f_naive = lru_naive_find_oldest(&fifo_side, &f_chk);
    printf("        扫全表（真·最久未用）= key=%d ｜ 链表 tail.prev（最早插入）= key=%d ⇒ %s\n",
           f_naive, fifo_side.tail.prev->key,
           f_naive == fifo_side.tail.prev->key ? "居然一致（说明这串操作碰巧同序）"
                                               : "不一致 ✔ 正说明 FIFO ≠ LRU");

    /* ============================================================
       第 6 幕：边界情况
       ============================================================ */
    section("第 6 幕：边界情况实测（capacity=1、更新老 key、key=0 与负数 key、未命中）");

    LRUCache one;
    lru_create(&one, 1);
    printf("      capacity=1：先淘汰再插入（顺序反了会把刚放进来的自己淘汰掉）\n");
    lru_put(&one, 42, 100);
    lru_print(&one, "      put(42,100) 之后：");
    lru_put(&one, 43, 200);
    lru_print(&one, "      put(43,200) 之后：");
    printf("        ⇒ 淘汰了 key=%d，表里 %d 个条目（还是满的）；如果先插后淘汰，这里会是空表\n",
           one.last_evicted_key, one.size);

    printf("\n      put 一个已经存在的 key：只改值 + 提到最前，不新增、不淘汰\n");
    lru_put(&one, 43, 999);
    lru_print(&one, "      put(43,999) 之后：");
    printf("        ⇒ size 还是 %d，last_evicted=%d（没淘汰任何人）\n", one.size, one.last_evicted);

    printf("\n      key 可以是 0、也可以是负数（这就是「先转 unsigned 再取模」的意义）：\n");
    int probe_keys[5] = {0, -1, -7, 7, 2147483647};
    LRUCache neg;
    lru_create(&neg, 8);
    for (int i = 0; i < 5; i++) {
        lru_put(&neg, probe_keys[i], i + 1);
        printf("        key=%-11d → 桶号 %2d（一定在 0..%d 内）｜ 存进去再偷看 = %d\n",
               probe_keys[i], lru_hash_bucket(probe_keys[i], neg.bucket_count),
               neg.bucket_count - 1, lru_peek(&neg, probe_keys[i]));
    }
    printf("        ⇒ 如果写成 `key %% 桶数`（不转 unsigned），key=-1 会得到 -1 号桶 ⇒ 数组越界，\n");
    printf("          而内存越界写这种 bug 现场往往崩在八竿子打不着的地方。\n");

    printf("\n      未命中：返回 -1，而且**不会**改动链表（不存在的东西没法提）\n");
    lru_print(&neg, "      查一个没存过的 key=999 之前：");
    int miss = lru_get(&neg, 999);
    printf("        lru_get(999) = %d（未命中），misses = %lld\n", miss, neg.misses);
    lru_print(&neg, "      查完之后（顺序一模一样）：");
    lru_check_order(&neg);

    /* ============================================================
       收尾：释放
       ============================================================ */
    section("收尾：全部释放（ASan 会再查一遍有没有漏）");

    lru_destroy(&c1);
    lru_destroy(&cache);
    lru_destroy(&good);
    lru_destroy(&bad);
    lru_destroy(&lru_side);
    lru_destroy(&fifo_side);
    lru_destroy(&one);
    lru_destroy(&neg);
    lru_destroy(&cache);        /* 再调一次：确认 destroy 是幂等的，不会二次释放 */

    long long alloc_n = 0;
    long long freed_n = 0;
    lru_alloc_stats(&alloc_n, &freed_n);
    printf("      本程序一共 malloc 了 %lld 个真节点、free 了 %lld 个 ⇒ %s\n",
           alloc_n, freed_n, alloc_n == freed_n ? "配平 ✔" : "有泄漏 ✘");
    printf("      （哨兵 head/tail 是结构体成员，从来就没 malloc 过，所以也不该 free）\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住四句话：\n");
    printf("  1) 哈希表回答「key 在哪个节点」，双向链表回答「谁最新谁最旧」，缺一个就必有 O(n)。\n");
    printf("  2) 双向 = 能从中间 O(1) 摘走；哨兵 = 摘/插一行特判都不用。\n");
    printf("  3) get 命中必须「提到最前」，这一步就是 LRU 的全部灵魂；漏了它就退化成 FIFO。\n");
    printf("  4) 淘汰要连着做四件事：哈希摘掉、链表摘掉、free、size--；少任何一件都是 bug。\n");
    return 0;
}
