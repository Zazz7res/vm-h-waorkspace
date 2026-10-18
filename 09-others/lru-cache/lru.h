#ifndef LRU_H
#define LRU_H

/*
 * ============================================================================
 * LRU 缓存 lru-cache（Least Recently Used，最近最少使用）—— 本文件的"菜单"
 * ============================================================================
 *
 * 【它要解决的问题：容量有限的缓存，满了该扔谁？】
 *   缓存装不下那么多东西，满了就必须扔一个。扔谁？
 *   LRU 的答案：**扔"最久没被用过"的那个**。依据是程序访问的局部性：
 *   刚被访问过的数据，很可能马上又被访问；好久没碰的，短期多半也不会再碰。
 *   （操作系统换页、CPU 缓存、Redis 的 maxmemory-policy、浏览器图片缓存、
 *     数据库 buffer pool、CDN……都在用它或它的变体。LeetCode 146 就是让你手写一遍。）
 *
 * 【硬性要求：get 和 put 都必须 O(1)】
 *   缓存是给"每秒几十万次访问"用的，任何一步退化成 O(n) 都等于自杀。
 *
 * 【为什么一个结构要"哈希表 + 双向链表"两件套 —— 本节的重点】
 *   单独任何一个都不行：
 *
 *   只用哈希表：key → value 查得快 O(1)。但"谁最久没被用过"它答不了 —— 哈希表天生无序，
 *     想答只能**扫一遍全表**比较时间戳 ⇒ O(n)。而且一 get 某个 key 它的"新旧"就变了，
 *     这个扫表连"提前算好缓存起来"都做不到。
 *
 *   只用双向链表：谁最旧一眼看到（尾巴），摘下来 / 插到头上都是 O(1)。
 *     但 get(k) 要先**找到** k 在哪个节点 ⇒ 从头一个个比 ⇒ O(n)。
 *
 *   两个一起用，各干自己最擅长的一半：
 *     哈希表负责"k 在哪个节点"（O(1) 定位到链表节点）；
 *     双向链表负责"谁最新、谁最旧"（O(1) 摘除 + O(1) 插到头部）。
 *     哈希表里存的是**链表节点的指针**：一份数据、两个视图，没有第二份拷贝，
 *     所以链表一动哈希表不用同步（它们指的是同一个节点）。
 *
 *   为什么必须是**双向**？"把中间某个节点摘下来提到头上"要改它前一个节点的 next。
 *   单链表只有 next，想找回"前一个"又得 O(n) 扫；有 prev 就是两行指针，永远 O(1)。
 *
 * 【head / tail 两个哨兵（sentinel）—— 干掉所有特判】
 *   head、tail 是**不存数据**的假节点，一头一尾永远站着：
 *       head ⇄ 最新的真节点 ⇄ … ⇄ 最旧的真节点 ⇄ tail
 *   有了它们：
 *     · "插到最前面"永远是 `insert_after(&head, p)`，不用管链表是不是空的；
 *     · "淘汰最旧"永远是 `p = tail.prev`，不用管是不是只剩一个节点；
 *     · 摘任意节点永远不用写 `if (p->prev != NULL)`，因为 head/tail 永远在那儿当爹。
 *   哨兵不省内存（反而多两个节点），省的是**分支和 bug** —— LRU 写错九成错在边界特判。
 *
 * 【本文件里的第三个字段：stamp（第几刻被用过）】
 *   纯粹为了"能自我验证"。双向链表说"尾巴就是最旧的"，凭什么信它？
 *   给每个节点记一个单调递增的 stamp（每次 get 命中 / put 都更新），
 *   那么"链表从头到尾 = stamp 从大到小"这条不变式必须永远成立。
 *   `lru_check_order()` 每一步都验一遍；`lru_naive_find_oldest()` 则**不走链表**、
 *   硬扫哈希表找 stamp 最小的节点，两边给出的"该淘汰谁"必须一致 —— 这就是本节的对拍。
 *
 * 【约定】
 *   key、value 都是 int（不做 void* 泛型、不传比较器函数指针，和全仓库一致）。
 *   get 命中返回 value，未命中返回 -1（题目规定的未命中记号）。
 *   accesses（命中过几次）和打印函数都只为把过程摊开给人看，真实项目里不需要。
 */

typedef struct LRUNode {
    int key;
    int value;
    int accesses;             /* 被 get 命中过几次（0 = 放进去后还没被查过）。教学用 */
    int stamp;                /* 最后一次"被使用"是第几刻（越大越新）。给自洽检查用 */
    struct LRUNode *prev;     /* ← 双向链表左边（往 head / 最新 方向） */
    struct LRUNode *next;     /* → 双向链表右边（往 tail / 最旧 方向） */
    struct LRUNode *hnext;    /* 哈希桶内的拉链（同桶冲突的节点串成一条单链） */
} LRUNode;

typedef struct {
    LRUNode **buckets;        /* 哈希表本体：一格一个"链头指针"。只存指针，数据在链表里 */
    int bucket_count;         /* 桶数。必须明显 ≥ 容量，否则桶里挤成一长串，O(1) 退化成 O(n) */
    int capacity;             /* 最多装几个条目，满了就淘汰 tail.prev */
    int size;                 /* 当前装了几个 */

    /* 两个哨兵是**结构体成员**（不是 malloc 出来的），所以永远存在、地址永远不变。
     * 清空后 head.next = &tail、tail.prev = &head 就是空表。 */
    LRUNode head;
    LRUNode tail;

    long long clock_ticks;    /* 全局"第几刻"计数器，喂给 stamp */
    int promote_on_get;       /* 1 = get 命中后提到最前（真 LRU）；0 = 关掉 ⇒ 退化成 FIFO。
                               * 第 5 幕用它现场演示"少这一行动作，策略整个变味" */
    int last_evicted_key;     /* 上一次 lru_put 淘汰了哪个 key（配合 last_evicted 看） */
    int last_evicted;         /* 1 = 上一次 put 确实淘汰了一个条目 */

    long long hits;           /* get 命中次数 */
    long long misses;         /* get 未命中次数 */
    long long evictions;      /* 一共淘汰了多少个条目 */
    long long bucket_steps;   /* 最近一次查找在桶里顺着拉链比了几个节点 */
    long long scan_steps;     /* 最近一次"硬扫全表找最旧"检查了几个条目（第 1 幕的痛） */
} LRUCache;

/* ---------- 生命周期 ---------- */

/* 建一个容量 capacity 的缓存；桶数内部按"容量 ×2 + 1，再取奇数"决定。
 * 返回 0 成功；-1 失败（capacity 非法或内存申请失败）。 */
int  lru_create(LRUCache *c, int capacity);

/* 释放所有真节点 + 桶数组，并把结构体清成"没用过"。
 * ⚠ 忘了调 = 内存泄漏；调完还能再调一次（幂等）。第 6 幕会数清楚"申请了几次、释放了几次"。 */
void lru_destroy(LRUCache *c);

/* ---------- 两个核心操作，都是 O(1) ---------- */

/* 查：命中返回 value 并把该条目提到"最新"；未命中返回 -1。 */
int  lru_get(LRUCache *c, int key);

/* 存：key 已存在 ⇒ 改 value + 提到最新（不算新增、不淘汰）；
 *     key 不存在且已装满 ⇒ **先**淘汰最旧的（tail.prev）**再**插到最新位置。
 *     （顺序不能颠倒：capacity=1 时先插后淘汰会把刚放进去的自己淘汰掉）
 * 被淘汰的 key 记在 c->last_evicted_key / c->last_evicted 里，方便外面打印。 */
void lru_put(LRUCache *c, int key, int value);

/* ---------- 观察 / 自验证用（不属于 LRU 本体） ---------- */

/* 从"最新"往"最旧"打印整条链表 + 每个条目的命中次数 + 谁下一个被淘汰 + 命中统计。 */
void lru_print(const LRUCache *c, const char *tag);

/* 只查不提升（"偷看一眼"）：对拍和演示时用，避免为了看一眼而改变新旧顺序。
 * 它会把"这次在桶里比了几个节点"记进 bucket_steps，方便把哈希表的代价打出来看。 */
int  lru_peek(const LRUCache *c, int key);

/* 校验不变式"链表从头到尾 stamp 严格递减"。一致返回 1，不一致返回 0 并打印在哪断的。 */
int  lru_check_order(const LRUCache *c);

/* 完全不走双向链表：硬扫哈希表所有桶，找 stamp 最小（= 最久未用）的条目，返回它的 key。
 * *checks_out 带回"检查了几个条目"。缓存为空时返回 -2。
 * 它和 tail.prev->key 必须永远相同 —— 两个实现互相盯着。 */
int  lru_naive_find_oldest(LRUCache *c, long long *checks_out);

/* 打印每个桶里有几个 key、最长链多长 —— 看哈希表有没有退化成一条链。 */
void lru_print_buckets(const LRUCache *c);

/* 反悔开关：1 = 正常 LRU，0 = 关掉"访问即变最新" ⇒ 策略退化成 FIFO（先进先出）。 */
void lru_set_promote_on_get(LRUCache *c, int on);

/* 把 key 映射到第几个桶（暴露出来是为了打印/演示；内部用的就是它）。 */
int  lru_hash_bucket(int key, int bucket_count);

/* 全进程累计：一共 malloc 过几个真节点、free 过几个。收尾时两个数必须相等 ——
 * 这是"我自己先查一边"，ASan 是"再查一遍"，两道关卡都过才算真没泄漏。 */
void lru_alloc_stats(long long *alloc_out, long long *freed_out);

/* 取全局"申请过几个节点 / 释放过几个节点"计数（本文件内部统计）。
 * 收尾时两个数必须相等，否则就是漏 free 了 —— ASan 也会同时点名。 */
void lru_alloc_stats(long long *alloc_out, long long *freed_out);

#endif /* LRU_H */
