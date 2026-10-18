#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lru.h"

/*
 * ============================================================================
 * LRU 缓存实现。
 *
 * 一句话：**哈希表负责「在哪」，双向链表负责「新旧」，两边指的是同一个节点。**
 *
 *        哈希表 buckets[]                双向链表（head/tail 是哨兵，不存数据）
 *      ┌──────┬──────┬──────┐          head ⇄ [3] ⇄ [1] ⇄ [2] ⇄ tail
 *      │ 0    │ 1    │ 2    │             ↑最新            ↑最旧（下一个淘汰）
 *      ├──────┼──────┼──────┤
 *      │  ·   │[1]·  │[2]·  │   桶里存的不是拷贝，而是**链表节点的指针**
 *      ├──────┼──────┼──────┤
 *      │[3]·  │  ·   │  ·   │   ⇒ get(k)：哈希定位 O(1) → 链表提到头上 O(1)
 *      └──────┴──────┴──────┘   ⇒ 满了：tail.prev 就是答案 O(1) → 摘掉 + free
 *
 * 所有"摘下来 / 插进去"的代码都不用判空，因为 head、tail 两个哨兵永远站在两头。
 * ============================================================================
 */

/* ---------- 内部：全局小账本（证明"申请过几个、释放过几个"） ---------- */

static long long g_nodes_allocated = 0;   /* 成功 malloc 出真节点的次数 */
static long long g_nodes_freed = 0;       /* 成功 free 掉真节点的次数 */

void lru_alloc_stats(long long *alloc_out, long long *freed_out)
{
    if (alloc_out != NULL) {
        *alloc_out = g_nodes_allocated;
    }
    if (freed_out != NULL) {
        *freed_out = g_nodes_freed;
    }
}

/* ---------- 内部：哈希 ---------- */

/*
 * key → 桶号。两个细节：
 *   1) 先转 unsigned：负数直接取模会得到负数（C 的 % 随左操作数符号），拿去当下标就数组越界。
 *      转成 unsigned 再取模，结果一定落在 0..bucket_count-1。
 *   2) 乘一个"黄金分割"常数 2654435761（= 2^32 / 黄金比例）再取模 —— Knuth 乘法散列。
 *      为什么要乘：连续的 key（1、2、3…）如果直接取模，桶号和 key 号一模一样，
 *      看着均匀但碰上"步长正好等于桶数"的数据就全挤一桶；乘上大奇数后高位被打散，
 *      取模得到的桶号就乱序了。乘法的另一个好处：一个整数运算都不用除（取模除外），极快。
 */
int lru_hash_bucket(int key, int bucket_count)
{
    unsigned int k = (unsigned int)key;
    k *= 2654435761u;
    return (int)(k % (unsigned int)bucket_count);
}

/*
 * 在桶里顺着拉链找 key。找到返回节点指针，找不到返回 NULL。
 * *steps_out 带回"比了几个节点" —— 桶数开得太小时这个数会变大，第 4 幕就拿它演示退化。
 */
static LRUNode *lru_find_node(const LRUCache *c, int key, long long *steps_out)
{
    long long steps = 0;
    int b = lru_hash_bucket(key, c->bucket_count);
    LRUNode *p = c->buckets[b];

    while (p != NULL) {
        steps++;
        if (p->key == key) {
            break;
        }
        p = p->hnext;
    }
    if (steps_out != NULL) {
        *steps_out = steps;
    }
    return p;
}

/* ---------- 内部：双向链表三个动作 ---------- */

/*
 * 把节点 p 从链表里摘出来（不动哈希表）。
 *
 *   摘之前：  A ⇄ [p] ⇄ B
 *   摘之后：  A ⇄ B          p->prev、p->next 置 NULL，表示"它现在不在任何链上"
 *
 * 两行指针，一个 if 都没有 —— 全靠 p->prev、p->next 一定不是 NULL（哨兵顶着）。
 * 置 NULL 不是多余的：万一以后哪条路径忘了插回去，下次解引用立刻段错误/ASan 报警，
 * 比"悄悄挂在旧链上"好抓一百倍。
 */
static void lru_unlink(LRUNode *p)
{
    p->prev->next = p->next;
    p->next->prev = p->prev;
    p->prev = NULL;
    p->next = NULL;
}

/* 插到"最新"位置 = 紧跟在 head 后面。四行，顺序不能乱：先接自己的两边，再改邻居。 */
static void lru_push_front(LRUCache *c, LRUNode *p)
{
    p->prev = &c->head;
    p->next = c->head.next;
    c->head.next->prev = p;
    c->head.next = p;
}

/* 提到最前 = 摘下来 + 插到头上 ⇒ 还是 O(1)。它就是这个数据结构的心脏：
 * 所谓"最近使用"，实现上就是"把它挪到链表头"。 */
static void lru_move_front(LRUCache *c, LRUNode *p)
{
    if (c->head.next == p) {
        return;                     /* 已经在最前面了，省两次指针改写 */
    }
    lru_unlink(p);
    lru_push_front(c, p);
}

/* ---------- 内部：哈希表的增删 ---------- */

/* 插到桶链头部（不查重复 —— 调用者保证 key 是新的） */
static void lru_hash_insert(LRUCache *c, LRUNode *p)
{
    int b = lru_hash_bucket(p->key, c->bucket_count);
    p->hnext = c->buckets[b];
    c->buckets[b] = p;
}

/*
 * 从桶链里摘掉 key。用**二级指针**：pp 永远指向"存着 p 的那个格子"
 * （可能是桶头 buckets[b]，也可能是前一个节点的 hnext）。
 * 这样"删链头"和"删中间"是同一行代码 `*pp = (*pp)->hnext;`，不用特判、不用 prev。
 */
static void lru_hash_remove(LRUCache *c, int key)
{
    int b = lru_hash_bucket(key, c->bucket_count);
    LRUNode **pp = &c->buckets[b];

    while (*pp != NULL) {
        if ((*pp)->key == key) {
            *pp = (*pp)->hnext;
            return;
        }
        pp = &(*pp)->hnext;
    }
}

/* 淘汰一个节点要做完的**四件事**，少做任何一件都是 bug：
 *   1) 从哈希表摘掉 —— 漏了 ⇒ key 还在表里，下次 get 摸到一块已释放的内存（use-after-free）
 *   2) 从链表摘掉   —— 漏了 ⇒ 链断了，遍历直接崩
 *   3) free         —— 漏了 ⇒ 内存泄漏（LRU 是长期运行的结构，漏一个就永远回不来）
 *   4) size--       —— 漏了 ⇒ 永远装不满 / 永远满，淘汰策略全乱
 */
static void lru_evict_node(LRUCache *c, LRUNode *victim)
{
    c->last_evicted_key = victim->key;
    c->last_evicted = 1;
    c->evictions++;

    lru_hash_remove(c, victim->key);
    lru_unlink(victim);
    free(victim);
    g_nodes_freed++;
    c->size--;
}

/* ---------- 生命周期 ---------- */

/*
 * 桶数怎么定：**至少是容量的 2 倍**（负载因子 ≤ 0.5），再往上找一个奇数。
 * 为什么不能等于容量：哈希会冲突，桶越满、拉链越长，get 的 O(1) 就越接近 O(n)。
 * 第 4 幕故意开成"桶数 = 容量"，让你看见最长链变多长、比几步。
 */
static int lru_pick_buckets(int capacity)
{
    int b = capacity * 2 + 1;
    if (b < 7) {
        b = 7;                      /* 小缓存也别开太少，7 是个顺手的下限 */
    }
    if (b % 2 == 0) {
        b++;                        /* 奇数：和乘法散列的 2 的幂因子不共因子，散得更开 */
    }
    return b;
}

int lru_create(LRUCache *c, int capacity)
{
    if (c == NULL || capacity <= 0) {
        printf("      [错误] lru_create：capacity=%d 必须是正整数\n", capacity);
        return -1;
    }

    memset(c, 0, sizeof(*c));       /* 所有计数、指针先清 0，省得漏初始化某个字段 */
    c->capacity = capacity;
    c->bucket_count = lru_pick_buckets(capacity);
    c->promote_on_get = 1;          /* 默认是真 LRU */

    c->buckets = calloc((size_t)c->bucket_count, sizeof(LRUNode *));
    if (c->buckets == NULL) {
        printf("      [错误] lru_create：申请 %d 个桶失败\n", c->bucket_count);
        return -1;
    }

    /* 哨兵：它们是结构体成员，不是 malloc 来的，所以"永远存在"，链表永远非空。
     * 空表就是 head 和 tail 互相指着：head.next == &tail && tail.prev == &head。 */
    c->head.prev = NULL;
    c->head.next = &c->tail;
    c->head.key = -1;               /* -1 表示"哨兵不存 key"（这里不用 0，免得和真 key=0 混） */
    c->tail.prev = &c->head;
    c->tail.next = NULL;
    c->tail.key = -1;
    c->size = 0;
    return 0;
}

void lru_destroy(LRUCache *c)
{
    if (c == NULL || c->buckets == NULL) {
        return;                     /* 幂等：没建过 / 已经拆过，都不炸 */
    }

    /* 只 free 真节点：从 head.next 走到 tail 为止，tail 本身不能 free（它是栈上的成员） */
    LRUNode *p = c->head.next;
    int freed = 0;
    while (p != &c->tail) {
        LRUNode *next = p->next;
        free(p);
        g_nodes_freed++;
        freed++;
        p = next;
    }
    printf("      [destroy] 释放了 %d 个真节点（另有 2 个哨兵是结构体成员，不该 free）+ %d 个桶\n",
           freed, c->bucket_count);

    free(c->buckets);
    c->buckets = NULL;              /* 置空：再调一次 destroy 也安全，用了它则是干净的段错误 */
    c->size = 0;
    c->capacity = 0;
    c->head.next = NULL;
    c->tail.prev = NULL;
}

/* ---------- 两个核心操作 ---------- */

int lru_get(LRUCache *c, int key)
{
    if (c == NULL || c->buckets == NULL) {
        printf("      [错误] lru_get：缓存还没建\n");
        return -1;
    }

    long long steps = 0;
    LRUNode *p = lru_find_node(c, key, &steps);
    c->bucket_steps = steps;

    if (p == NULL) {
        c->misses++;
        return -1;                  /* 题目规定的未命中记号 */
    }

    c->hits++;
    p->accesses++;
    p->stamp = ++c->clock_ticks;    /* 记时刻：给 lru_check_order / lru_naive_find_oldest 用 */
    if (c->promote_on_get) {
        lru_move_front(c, p);       /* ★LRU 的全部灵魂就在这一行 */
    }
    return p->value;
}

void lru_put(LRUCache *c, int key, int value)
{
    if (c == NULL || c->buckets == NULL) {
        printf("      [错误] lru_put：缓存还没建\n");
        return;
    }

    c->last_evicted = 0;
    c->last_evicted_key = 0;

    long long steps = 0;
    LRUNode *p = lru_find_node(c, key, &steps);
    c->bucket_steps = steps;

    if (p != NULL) {
        /* 老 key：只改值 + 提到最新，**不新增条目、不淘汰任何人** */
        p->value = value;
        p->stamp = ++c->clock_ticks;
        lru_move_front(c, p);
        return;
    }

    /* 新 key 且已装满 ⇒ 先淘汰，再插入。
     * 顺序反了会怎样？capacity=1 时：先插入的话它自己就是最旧的，紧接着被淘汰的是它自己
     * ⇒ 缓存永远为空，永远 miss。这种"看着只差一行顺序"的错最难查。 */
    if (c->size >= c->capacity) {
        lru_evict_node(c, c->tail.prev);   /* tail.prev 恒等于最旧节点，哨兵保证它永远存在 */
    }

    LRUNode *n = malloc(sizeof(*n));
    if (n == NULL) {
        printf("      [错误] lru_put：申请节点失败（key=%d）\n", key);
        return;
    }
    n->key = key;
    n->value = value;
    n->accesses = 0;
    n->stamp = ++c->clock_ticks;
    n->prev = NULL;
    n->next = NULL;
    n->hnext = NULL;
    g_nodes_allocated++;

    lru_push_front(c, n);
    lru_hash_insert(c, n);
    c->size++;
}

/* ---------- 观察 / 自验证 ---------- */

int lru_peek(const LRUCache *c, int key)
{
    if (c == NULL || c->buckets == NULL) {
        printf("      [错误] lru_peek：缓存还没建\n");
        return -1;
    }
    long long steps = 0;
    LRUNode *p = lru_find_node(c, key, &steps);
    return (p != NULL) ? p->value : -1;   /* 什么都不改：偷看不算使用 */
}

void lru_set_promote_on_get(LRUCache *c, int on)
{
    c->promote_on_get = on ? 1 : 0;
}

/*
 * 从最新往最旧打印。尾巴那个标注"下一个淘汰"。
 * 中括号里是 key，等号后是 value，#号后是这个条目被命中过几次。
 */
void lru_print(const LRUCache *c, const char *tag)
{
    if (c == NULL || c->buckets == NULL) {
        printf("      [错误] lru_print：缓存还没建\n");
        return;
    }
    if (tag != NULL && tag[0] != '\0') {
        printf("      %s\n", tag);
    }

    printf("      顺序 最新→最旧：");
    const LRUNode *p = c->head.next;
    if (p == &c->tail) {
        printf("（空）");
    }
    while (p != &c->tail) {
        printf("[key=%d val=%d #%d]", p->key, p->value, p->accesses);
        if (p->next != &c->tail) {
            printf(" → ");
        }
        p = p->next;
    }
    if (c->size > 0) {
        printf("   ← 最旧的是 key=%d，它下一个被淘汰", c->tail.prev->key);
    }
    printf("\n");
    printf("      容量 %d/%d ｜ 命中 %lld ｜ 未命中 %lld ｜ 淘汰 %lld ｜ 本次在桶里比了 %lld 个\n",
           c->size, c->capacity, c->hits, c->misses, c->evictions, c->bucket_steps);
}

/* 校验不变式：链表从头到尾 stamp 必须严格递减（真 LRU 模式下） */
int lru_check_order(const LRUCache *c)
{
    if (c == NULL || c->buckets == NULL) {
        printf("      [检查] 缓存还没建，跳过\n");
        return 1;
    }
    if (!c->promote_on_get) {
        printf("      [自洽检查] promote_on_get=0（FIFO 模式）：位置只由插入顺序决定，\n");
        printf("                 本来就不该等于「按使用时间排序」⇒ 这一条检查跳过，\n");
        printf("                 而下一条（谁最旧）就会和链表给的答案不一致 —— 那正是重点。\n");
        return 1;
    }

    int count = 0;
    int prev_stamp = 0;
    int first = 1;
    for (const LRUNode *p = c->head.next; p != &c->tail; p = p->next) {
        if (!first && p->stamp >= prev_stamp) {
            printf("      [自洽检查] ✘ 链表顺序不对：key=%d 的 stamp=%d 竟然不比前一个(%d)更小\n",
                   p->key, p->stamp, prev_stamp);
            return 0;
        }
        prev_stamp = p->stamp;
        first = 0;
        count++;
    }
    if (count != c->size) {
        printf("      [自洽检查] ✘ 链表上数到 %d 个，size 记的是 %d ⇒ 有一边的指针断了\n",
               count, c->size);
        return 0;
    }
    printf("      [自洽检查] ✔ 链表长度 = size = %d，且 stamp 从新到旧严格递减\n", count);
    return 1;
}

/*
 * **不走链表**，硬扫全部桶，找 stamp 最小（= 最久没被用过）的条目。
 * 这就是"只有哈希表、没有双向链表"时你唯一的办法：每次要淘汰人，都得先 O(n) 扫一遍。
 * 返回它的 key（空表返回 -2），*checks_out 带回检查了几个条目。
 */
int lru_naive_find_oldest(LRUCache *c, long long *checks_out)
{
    if (c == NULL || c->buckets == NULL || c->size == 0) {
        c->scan_steps = 0;
        return -2;
    }

    long long checks = 0;
    int best_key = 0;
    int best_stamp = 0;
    int have = 0;

    for (int b = 0; b < c->bucket_count; b++) {
        for (LRUNode *p = c->buckets[b]; p != NULL; p = p->hnext) {
            checks++;
            if (!have || p->stamp < best_stamp) {
                have = 1;
                best_stamp = p->stamp;
                best_key = p->key;
            }
        }
    }
    c->scan_steps = checks;
    if (checks_out != NULL) {
        *checks_out = checks;
    }
    return best_key;
}

void lru_print_buckets(const LRUCache *c)
{
    if (c == NULL || c->buckets == NULL) {
        printf("      [错误] lru_print_buckets：缓存还没建\n");
        return;
    }
    int longest = 0;
    int longest_bucket = 0;
    int used = 0;

    printf("      桶号（下标）→ 这条拉链上的 key：\n");
    for (int b = 0; b < c->bucket_count; b++) {
        int len = 0;
        printf("        %3d :", b);
        for (const LRUNode *p = c->buckets[b]; p != NULL; p = p->hnext) {
            printf(" %d", p->key);
            len++;
        }
        if (len == 0) {
            printf(" -");
        } else {
            used++;
            if (len > longest) {
                longest = len;
                longest_bucket = b;
            }
        }
        printf("\n");
    }
    printf("      桶数 %d ｜ 装了 %d 个 ｜ 非空桶 %d 个 ｜ 最长链 %d（在桶 %d）\n",
           c->bucket_count, c->size, used, longest, longest_bucket);
    printf("      ⇒ 平均每次查找比 %.1f 个；最长链就是最坏情况。桶开小了这条链就会变长 ⇒ O(1) 变 O(n)\n",
           c->bucket_count > 0 ? (double)c->size / (double)c->bucket_count : 0.0);
}
