#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include "chaining.h"

/*
 * ===================================================================
 * 拉链法哈希表实现。
 *
 * 本节在内存管理上和 07-graph/adjacency-list 是**同一个形状**：
 *   两级结构 = 一个"指针数组"（一次 malloc，连续） + N 条"链表"（每个节点各一次 malloc）。
 * 所以 free 的纪律也一模一样：**从最里层往外剥**（链节点 → 数组 → 表结构）。
 * 字符串版还多一层：节点里的 key 自己也是 malloc 来的副本，必须单独 free。
 * ===================================================================
 */

/*
 * ===================================================================
 * 0. 先把最阴的那个坑处理干净：C 的 % 会给你负数
 * ===================================================================
 * 数学里的"取模"（mod）结果定义在 [0, m)，永远非负。
 * 但 C 语言的 % 是"跟着截断除法配套设计的余数"，它的**符号跟被除数**：
 *
 *     C99 起规定整数除法是"截断除法"（向 0 取整），并且必须满足这条等式：
 *         (a / b) * b + (a % b) == a
 *     a = -7, b = 4：
 *         -7 / 4  = -1      ← 向 0 取整，不是 -2（那是"向下取整"，数学里的 floor）
 *         所以 -7 % 4 = -7 - (-1)*4 = -7 + 4 = -3     ← 负数！
 *     （Python 用的是 floor 除法，-7 % 4 == 1，所以从 Python 过来的人必踩。）
 *
 * 为什么这事在哈希表里是致命的：
 *     int idx = h(key) % capacity;      // h(key) 可能是负数
 *     buckets[idx]                       // idx = -3 → 读写数组前面的内存
 * 编译器**不会**报警（它不知道 idx 的范围），运行时也不一定有症状 ——
 * 你可能往别的变量的内存里写了一个指针，程序跑十次都对，第十一次才崩。
 * ASan 会在第一次就越界打断你（`heap-buffer-overflow`）。
 *
 * 正确姿势只有一个：**把取余包进一个函数，保证返回值非负**（就是下面的 ht_mod）。
 * 本节所有下标换算都必须经过 ht_mod，没有例外。
 */
void ht_mod_pitfall(void)
{
    /* 故意挑几个负数键：-1 / -7 / -1000000007 / INT_MIN 各代表一种情况 */
    int keys[] = { -1, -7, -100, -1000000007, INT_MIN, 0, 7, 100 };
    int nkeys = (int)(sizeof(keys) / sizeof(keys[0]));
    int cap = 8;

    printf("  桶数 capacity = %d，合法下标只能是 0..%d\n\n", cap, cap - 1);
    printf("  %14s %10s %10s %10s\n", "键 key", "key%8", "ht_mod", "能用吗");
    printf("  ------------------------------------------------------\n");
    for (int i = 0; i < nkeys; i++) {
        int k = keys[i];
        int raw = k % cap;              /* ← 天真写法：可能是负数 */
        int good = ht_mod((long)k, cap);/* ← 唯一被允许的下标换算 */
        printf("  %14d %10d %10d   %s\n",
               k, raw, good, (raw < 0) ? "❌ 越界" : "✅");
    }
    printf("\n  逐条验算 C 的截断除法等式 (a/b)*b + a%%b == a：\n");
    printf("    -7 / 4 = %d,  -7 %% 4 = %d,  验算 %d*4 + (%d) = %d  ← 余数是负的\n",
           -7 / 4, -7 % 4, -7 / 4, -7 % 4, (-7 / 4) * 4 + (-7 % 4));
    printf("     7 / -4 = %d,  7 %% -4 = %d  ← 余数符号只跟【被除数】，跟除数无关\n",
           7 / (-4), 7 % (-4));
    printf("    注意：网上常见的说法是\"-7 %% 4 等于 -1\"，这是**记错了**：\n");
    printf("          -1 是商（-7/4 向 0 截断），余数是 -3。跑一遍上面那张表就不会再记混。\n");
    printf("\n  两个连带的坑：\n");
    printf("    1) 别用 abs(h) %% m 来\"修正\"：h == INT_MIN 时 -h 溢出，是【未定义行为】。\n");
    printf("       ht_mod 用 long 做除法再平移，%d %% %d = %d 全程定义良好。\n",
           INT_MIN, cap, ht_mod((long)INT_MIN, cap));
    printf("    2) 无符号哈希（djb2 用 unsigned long）配无符号取余（ht_mod_u）天然非负；\n");
    printf("       但如果为了\"统一\"把 h 塞进 int/long 再取余，就又可能负 —— 类型别偷懒。\n");
    printf("       第 7 幕会用一个真实长键把这件事跑出来看。\n");
}

/*
 * 数学取模：结果永远在 [0, m)。
 *
 * 为什么先转成 long？
 *   1) 哈希值本身可能大到 int 装不下（字符串哈希、乘法哈希都用 long/unsigned long），
 *      在 long 域里取余可以避免"先溢出再取余"丢掉信息；
 *   2) 万一传进来的是 INT_MIN，转 long 后取负/相减都不会溢出。
 * 为什么是 "+ m 再 % m" 而不是 "if (r < 0) r = -r"？
 *   取绝对值是错的：-3 和 3 在模 8 下是**不同的桶**（-3 ≡ 5），
 *   把 -3 变成 3 会让"同一个键两次落到不同桶"，直接毁掉哈希表的正确性。
 *   平移 m 才是同余的：-3 + 8 = 5，且 5 在 [0,8) 里。
 */
int ht_mod(long value, int m)
{
    if (m <= 0) {
        /* capacity 必须是正数。真发生在这里就是调用方逻辑错了，
         * 打印警告而不是静默返回 0 —— 静默返回会让所有键挤进桶 0，
         * 表还能"正确工作"（链很长而已），bug 就被藏到几个月后。 */
        printf("      [警告] ht_mod 的除数 m=%d 不合法，按 1 处理\n", m);
        m = 1;
    }
    long r = value % m;          /* C 的 %：符号跟 value */
    if (r < 0) {
        r += m;                  /* 平移进 [0, m)：同余，不改变"谁是谁的桶"关系 */
    }
    return (int)r;
}

/*
 * 无符号取模：一行，但它存在的意义是"整条链路上没有任何符号"。
 *   - 无符号 % 的结果一定在 [0, m)，不需要平移，也不可能有负下标；
 *   - 避免了 unsigned long → long 的收窄转换（值太大时是 implementation-defined）。
 * 字符串表的下标换算全走这里（ht_hash_string 返回 unsigned long）。
 * m == 0 会在这里炸（整数除零是未定义行为 + 运行时报 SIGFPE），
 * 所以调用方的 capacity 必须是正数 —— create 函数已经把住了这道门。
 */
unsigned long ht_mod_u(unsigned long value, unsigned long m)
{
    if (m == 0UL) {
        printf("      [警告] ht_mod_u 的除数 m=0，按 1 处理\n");
        m = 1UL;
    }
    return value % m;
}

/* =====================================================================
 * 1. int 键：哈希函数与下标换算
 * ===================================================================== */

/*
 * 本节的哈希函数 = 键本身（identity hash）。
 *
 * 为什么故意写得这么"傻"？
 *   教学要用真实数字：`1 %% 8 == 1`、`9 %% 8 == 1`、`17 %% 8 == 1` ——
 *   你可以**手算验证程序打印的每一个桶下标**。哈希函数一复杂，
 *   你就只能相信它，而这一节的重点恰恰是"别相信，自己算一遍"。
 *
 * 真实项目不能这么用，因为现实键往往带公共因子：
 *   比如键全是偶数（用户 ID 每次 +2），identity + 取余会让一半桶永远空着。
 *   常见修法（本节代码不用，但你要知道长什么样）：
 *     乘法哈希：h = (unsigned)key * 2654435761u;  然后取高位
 *     二次幂桶数 + 掩码：idx = h & (capacity - 1)   （等价于 % capacity，但更快）
 * 注意：这些修法都必须仍然经过 ht_mod 或等价的非负保证。
 */
static long ht_hash_int(int key)
{
    return (long)key;
}

/*
 * 键 → 桶下标。整节代码里唯一直接碰 buckets[] 索引的地方，只允许这一处换算。
 * ⚠ 为什么不再写 `int idx = key % t->capacity;` 而要多包一层函数？
 *   因为这一层里藏着 ht_mod 的非负保证。少包一层，键一负就越界。
 */
static int ht_bucket_of(const IntHT *t, int key)
{
    /* 三步走完哈希表的全部理论：哈希 → 取模 → 得下标 */
    long h = ht_hash_int(key);
    return ht_mod(h, t->capacity);
}

/* =====================================================================
 * 2. 就地重写的极简链表三招（第 02 章单链表的复用）
 * =====================================================================
 * 为什么不 #include "../../02-linked-list/singly/slist.h"？
 *   仓库的 Makefile 按目录独立编译（一次只吃本目录的 *.c），
 *   跨目录 include 会让这一节单独编译失败。**自包含 > 复用。**
 *   何况这三招每招 10 行，重写一遍才是真的复习。
 */

/* 造节点：堆上租一块，next 立刻置 NULL。 */
static HTNode *node_new(int key, int value)
{
    HTNode *nd = malloc(sizeof(HTNode));
    if (nd == NULL) {
        return NULL;
    }
    nd->key = key;
    nd->value = value;
    nd->next = NULL;   /* ★ 不置 NULL 就是野指针：malloc 不初始化内存 */
    return nd;
}

/*
 * 头插：O(1)。哈希表用头插而不是尾插，两个理由：
 *   1) 尾插要走到尾巴，链越长越贵；头插常数最小。
 *   2) 现代 CPU cache 视角：新插入的键最可能被马上读到，放链头最划算。
 * 副作用：**同桶内的顺序是"后插的在前"**，所以 ht_print 打出来的链
 *   和插入顺序相反。别把它当成 bug（本节 README 有说明）。
 */
static void node_push_front(HTNode **head, HTNode *fresh)
{
    fresh->next = *head;   /* ★ 顺序不能反：先让新节点抓住老头 */
    *head = fresh;         /*   再换头。反了的话老链整条失联 → 泄漏 */
}

/*
 * 按 key 删节点 —— 用 02 章教过的 dummy 哨兵统一"删头/删中/删尾"。
 * 返回 1 删掉了、0 没找到。
 *
 *   dummy ─►[A]─►[B]─►[C]─►NULL
 *     ▲      ▲
 *     └ prev ┘        ← prev 永远站在"要删的那个"前面一格
 * 于是永远只做一件事：prev->next = victim->next; free(victim);
 */
static int node_remove_key(HTNode **head, int key, long *probes_out)
{
    HTNode dummy;                 /* 栈上的假节点：不 malloc，所以不用 free */
    dummy.key = 0;
    dummy.next = *head;

    HTNode *prev = &dummy;
    while (prev->next != NULL) {
        if (probes_out != NULL) {
            (*probes_out)++;      /* 每看一个节点记一次数 */
        }
        if (prev->next->key == key) {
            HTNode *victim = prev->next;
            prev->next = victim->next;   /* ★ 先接好链 */
            free(victim);                  /* ★ 再 free —— 顺序反了就是 use-after-free */
            *head = dummy.next;            /* 统一写回：可能头被换掉了 */
            return 1;
        }
        prev = prev->next;
    }
    *head = dummy.next;   /* 没找到也写回（值没变，但两条出口路径行为完全一致） */
    return 0;
}

/* 数一条链的长度（ht_stats 用） */
static int chain_length(const HTNode *head)
{
    int cnt = 0;
    for (const HTNode *p = head; p != NULL; p = p->next) {
        cnt++;
    }
    return cnt;
}

/* =====================================================================
 * 3. 建表 / 拆表
 * =====================================================================
 * buckets 是"capacity 个 StrNode* / HTNode* 的数组"，一次 calloc 全置 0。
 * 指针的全 0 位模式就是 NULL，所以 calloc 一步到位，省掉一个 for 循环，
 * 也消灭"漏初始化某格"这类 bug（空桶必须是 NULL，不能是垃圾地址）。
 */
IntHT *ht_create(int capacity)
{
    if (capacity <= 0) {
        printf("      [错误] ht_create 的 capacity=%d 必须为正\n", capacity);
        return NULL;
    }

    IntHT *t = malloc(sizeof(IntHT));
    if (t == NULL) {
        return NULL;
    }

    t->buckets = calloc((size_t)capacity, sizeof(HTNode *));
    if (t->buckets == NULL) {
        free(t);          /* ★ 回滚：第二级分配失败时，第一级已经拿到了，必须还回去。
                           *   只 malloc 一次不会出事；一旦两处 malloc 忘了回滚，
                           *   就是"内存紧张路径上的泄漏"—— 偏偏内存紧张时最需要不泄漏。 */
        return NULL;
    }

    t->capacity = capacity;
    t->size = 0;
    t->probes = 0;
    return t;
}

/*
 * 拆表：三层从里往外剥。
 *   第 1 层：每条链上的每个节点（size 次 free）
 *   第 2 层：buckets 指针数组（1 次 free）
 *   第 3 层：IntHT 结构体本身（1 次 free）
 * 漏掉第 1 层是这一节最典型的泄漏 —— ASan 会报
 * "xxx bytes in N objects"，那个 N 就等于节点个数，直接给你线索。
 */
void ht_free(IntHT *t)
{
    if (t == NULL) {
        return;
    }
    for (int i = 0; i < t->capacity; i++) {
        HTNode *p = t->buckets[i];
        while (p != NULL) {
            HTNode *nxt = p->next;   /* ★ 先把 next 抄下来 */
            free(p);                 /*   再 free 当前 */
            p = nxt;                 /*   最后跳到抄下来的地址 */
        }
        t->buckets[i] = NULL;        /* 顺手置 NULL：防止有人接着遍历这张已拆的表 */
    }
    free(t->buckets);
    t->buckets = NULL;
    free(t);
}

/* =====================================================================
 * 4. rehash：扩容 = 全部重新投桶（本节的心脏手术）
 * =====================================================================
 * 为什么必须搬家而不是"在旁边再接一段数组"？
 *   桶下标 = h(key) % capacity，**capacity 一变，每个键的下标都变了**。
 *   老键继续待在老下标上，下次 get 就会去新下标找 → 找不到 → 数据凭空消失。
 *   所以 rehash 没有捷径：把每个节点从老链上摘下来，按新 capacity 重算桶，
 *   挂到新链上。整张表所有元素都要动一次。
 *
 * 既然代价是 O(n)，为什么还敢随手扩？
 *   因为**摊还**：capacity 每次翻倍，n 次插入里只有 log 次搬家，
 *   每次搬 n 个 → 平均每个元素只被搬常数几次，摊到每次插入仍是 O(1)。
 *   这就是"摊还复杂度"最直观的实例。
 *
 * ★ 关键优化（也是 Redis 增量 rehash 的立足点）：
 *   capacity 翻倍时，新下标只有两种可能：
 *       h % (2C) 要么等于 h % C，要么等于 h % C + C
 *   证明：h = q*C + r（r = h % C）。若 q 偶 → h = (q/2)*(2C) + r → 新下标 r；
 *         若 q 奇 → h = ((q-1)/2)*(2C) + (r + C) → 新下标 r + C。
 *   所以每个元素"要么不动，要么搬去 原下标 + 旧容量"，一个桶最多裂成两个桶 ——
 *   Java HashMap 的 split、Redis 的 dict 都是靠这条。ht_rehash_plan 会把这条打印出来。
 *
 * 实现细节：节点对象**一个都不重新 malloc**，只重接 next 指针；
 * 老数组整体 free。这样 rehash 不产生新的 malloc/free 责任，也不漏。
 */
static int ht_rehash(IntHT *t, int new_capacity)
{
    if (t == NULL || new_capacity <= 0) {
        return 0;
    }

    HTNode **fresh = calloc((size_t)new_capacity, sizeof(HTNode *));
    if (fresh == NULL) {
        printf("      [错误] rehash 申请新桶数组失败（要 %d 个指针），保持原表不动\n",
               new_capacity);
        return 0;   /* ★ 失败就整张表维持原样：负载因子高一点不会错，只是慢一点。
                     *   这种"扩容失败可降级"的设计是哈希表能长期跑的关键。 */
    }

    int old_capacity = t->capacity;
    int moved = 0;

    for (int i = 0; i < old_capacity; i++) {
        HTNode *p = t->buckets[i];
        while (p != NULL) {
            HTNode *nxt = p->next;        /* ★ 先抄 next：马上要改 p->next 了 */

            /* 用【新 capacity】重算下标：这就是"重新投桶"那一投 */
            int idx = ht_mod(ht_hash_int(p->key), new_capacity);

            node_push_front(&fresh[idx], p);   /* 挂到新桶（p 本身没动，只是换了条链） */
            if (idx != i) {
                moved++;
            }
            p = nxt;
        }
        t->buckets[i] = NULL;   /* 老链已整条搬空，置 NULL 保证后面 free 老数组不会重复释放 */
    }

    free(t->buckets);           /* 只 free 数组本身，节点已经全在新数组里了 */
    t->buckets = fresh;
    t->capacity = new_capacity;
    return moved;
}

/*
 * 预测打印：不真的搬，只按 new_capacity 算每个键会落到哪个桶，
 * 并且现场验算"翻倍扩容只有两种归宿"这条规律。
 *
 * 规律（只在 capacity 是 2 的幂、且哈希值非负时成立，本函数用非负键演示）：
 *   h = q*C + r，r = h % C = 旧下标
 *     q 偶 → 新下标 = r          （原地不动）
 *     q 奇 → 新下标 = r + C      （搬到"旧下标 + 旧容量"）
 * main.c 先用它打"预言"，再触发真扩容，两边数字必须完全一致 ——
 * 这是给学习者的一种验证：**你手算出来的桶 == 程序用的桶**。
 */
void ht_rehash_plan(const IntHT *t, int new_capacity)
{
    if (t == NULL || new_capacity <= 0) {
        return;
    }
    int old_cap = t->capacity;
    printf("  预测：capacity %d → %d 之后，每个键会落到哪个桶（只看非负键，规律才成立）\n",
           old_cap, new_capacity);
    printf("      key   旧桶   新桶   h/C 的商 q   q 奇偶   按规律推   吻合?\n");
    printf("  ------------------------------------------------------------------\n");
    for (int i = 0; i < t->capacity; i++) {
        for (const HTNode *p = t->buckets[i]; p != NULL; p = p->next) {
            long h = ht_hash_int(p->key);
            int neu = ht_mod(h, new_capacity);
            if (h < 0) {
                printf("  %6d %6d %6d %8s   （键为负，跳过规律验算）\n", p->key, i, neu, "-");
                continue;
            }
            long q = h / old_cap;
            int odd = (int)(q % 2 != 0);
            int predicted = i + (odd ? old_cap : 0);
            printf("  %6d %6d %6d %8ld %6s %10d   %s\n",
                   p->key, i, neu, q, odd ? "奇" : "偶", predicted,
                   (predicted == neu) ? "✅" : "❌");
        }
    }
    printf("  结论：新下标只有两种 —— 旧下标，或旧下标 + 旧容量(%d)。一个桶最多裂成两个桶。\n",
           old_cap);
}

/* =====================================================================
 * 5. put / get / contains / delete / len
 * ===================================================================== */

/*
 * put：有则覆盖值，无则插入。返回 1 成功、0 内存不够。
 *
 * 流程严格三步，顺序有讲究：
 *   (1) 先在旧桶里找 —— 找到就改值 return，size 不变（**覆盖不是插入**，
 *       否则反复 put 同一个键会把表撑爆）；
 *   (2) 找不到 → 判断"如果装上这第 size+1 个，负载因子会不会 > 0.75"，
 *       会就先扩容。**用整数乘法比较，不用浮点数**：
 *           (size+1)*4 > capacity*3   ⟺   (size+1)/capacity > 0.75
 *       浮点比较看着简单，但 double 有舍入，`0.75` 这种边界值今天恰好相等、
 *       明天换个 capacity 就判断错，属于"三年后回来查的 bug"。
 *   (3) 扩容会改变桶下标 → 必须【重新算一次 idx】再挂链。
 *
 * ★ 第 (3) 步是初学者最容易漏的一行：拿扩容前算好的 idx 去挂新数组，
 *   等于把元素挂进错误的桶，之后 get 再也找不到它（数据静默丢失，最难查）。
 */
int ht_put(IntHT *t, int key, int value)
{
    if (t == NULL) {
        return 0;
    }

    /* (1) 已存在？ */
    int idx = ht_bucket_of(t, key);
    for (HTNode *p = t->buckets[idx]; p != NULL; p = p->next) {
        t->probes++;
        if (p->key == key) {
            p->value = value;    /* 覆盖，size 不动 */
            return 1;
        }
    }

    /* (2) 要扩容？ */
    if ((t->size + 1) * 4 > t->capacity * 3) {
        int old_cap = t->capacity;
        printf("      [rehash] 装入第 %d 个键后负载因子将达 %.3f > 0.75，"
               "桶数 %d → %d\n",
               t->size + 1, (double)(t->size + 1) / (double)old_cap,
               old_cap, old_cap * 2);
        ht_rehash(t, old_cap * 2);   /* 失败也不影响正确性，表维持原样 */
    }

    /* (3) ★ 重新算下标：capacity 可能已经变了 */
    idx = ht_bucket_of(t, key);

    HTNode *fresh = node_new(key, value);
    if (fresh == NULL) {
        return 0;
    }
    node_push_front(&t->buckets[idx], fresh);
    t->size++;
    return 1;
}

/*
 * get：命中返回 1 并把值写进 value_out；value_out 传 NULL 表示只想知道有没有。
 *
 * 复杂度写 O(1) 的前提是"链很短"。真实代价 = 1 次数组寻址 + 链上比较次数，
 * 所以我们每次比较都 t->probes++，把抽象的 O(1) 变成能打印的数字。
 *
 * ⚠ 找到节点后必须再比一次 key：哈希相等 ≠ 键相等（这就是"冲突"的定义）。
 */
int ht_get(IntHT *t, int key, int *value_out)
{
    if (t == NULL) {
        return 0;
    }
    int idx = ht_bucket_of(t, key);
    for (HTNode *p = t->buckets[idx]; p != NULL; p = p->next) {
        t->probes++;
        if (p->key == key) {
            if (value_out != NULL) {
                *value_out = p->value;
            }
            return 1;
        }
    }
    return 0;   /* 整条链走完都没找到：键不存在（注意不是"桶空"才叫不存在） */
}

int ht_contains(IntHT *t, int key)
{
    return ht_get(t, key, NULL);   /* 复用：值不要，只要"在不在" */
}

int ht_delete(IntHT *t, int key)
{
    if (t == NULL) {
        return 0;
    }
    int idx = ht_bucket_of(t, key);
    int removed = node_remove_key(&t->buckets[idx], key, &t->probes);
    if (removed) {
        t->size--;     /* ★ 只减 size，不缩 capacity：
                        *   频繁"删一半又插满"时反复缩容会抖个不停（thrash）。
                        *   绝大多数库（含 Java HashMap）也不主动缩容，
                        *   真要缩必须等负载因子低到某个下限（如 0.1）且 size 不太小。 */
    }
    return removed;
}

int ht_len(const IntHT *t)
{
    return (t == NULL) ? 0 : t->size;   /* O(1)：因为我们一路维护着 size 字段。
                                         * 如果偷懒不维护，这里就得遍历全表 O(n)。 */
}

double ht_load_factor(const IntHT *t)
{
    if (t == NULL || t->capacity == 0) {
        return 0.0;
    }
    return (double)t->size / (double)t->capacity;
}

/* =====================================================================
 * 6. 看见它：把"桶 + 链"打成文字
 * ===================================================================== */

void ht_print(const IntHT *t)
{
    if (t == NULL) {
        return;
    }
    printf("  桶数组 buckets[%d]，元素 size = %d，负载因子 = %.3f\n",
           t->capacity, t->size, ht_load_factor(t));
    for (int i = 0; i < t->capacity; i++) {
        int len = chain_length(t->buckets[i]);
        printf("  buckets[%2d] (%2d 个)│ ", i, len);
        if (len == 0) {
            printf("NULL\n");        /* 空桶就一个 NULL：拉链法没有"废格子"概念 */
            continue;
        }
        for (const HTNode *p = t->buckets[i]; p != NULL; p = p->next) {
            printf("[k=%d v=%d] -> ", p->key, p->value);
        }
        printf("NULL\n");
    }
}

/*
 * 统计表：空桶数、最长链、平均链长。
 * "平均链长"其实就等于负载因子（size/capacity）——
 * 这一行是理解"为什么负载因子重要"的最短路径：
 *   负载因子 0.75 → 平均一个桶 0.75 个元素；
 *   负载因子 4.0  → 平均一条链 4 个节点，查找就得走 4 步比较。
 * 还额外统计"链长 > 1 的桶有几个"，那是冲突的直接证据。
 */
void ht_stats(const IntHT *t)
{
    if (t == NULL) {
        return;
    }
    int empty = 0;
    int longest = 0;
    int longest_bucket = -1;
    int collided_buckets = 0;

    for (int i = 0; i < t->capacity; i++) {
        int len = chain_length(t->buckets[i]);
        if (len == 0) {
            empty++;
        }
        if (len > 1) {
            collided_buckets++;
        }
        if (len > longest) {
            longest = len;
            longest_bucket = i;
        }
    }

    printf("  桶数 %d，元素 %d\n", t->capacity, t->size);
    printf("  空桶 %d 个（%.1f%%），非空桶 %d 个\n",
           empty, t->capacity ? 100.0 * empty / t->capacity : 0.0,
           t->capacity - empty);
    printf("  发生冲突的桶（链长≥2）%d 个\n", collided_buckets);
    printf("  最长链 = %d（在 buckets[%d]）\n", longest, longest_bucket);
    printf("  平均链长 = size/capacity = %.3f  ← 这就是负载因子本身\n", ht_load_factor(t));
    if (t->capacity - empty > 0) {
        printf("  非空桶里的平均链长 = %d/%d = %.3f\n",
               t->size, t->capacity - empty,
               (double)t->size / (double)(t->capacity - empty));
    }
}

/* =====================================================================
 * 7. 灌数据 & 数探针（实测"退化"的仪器）
 * ===================================================================== */

int ht_fill_arithmetic(IntHT *t, int n, int base, int step)
{
    int ok = 0;
    for (int i = 0; i < n; i++) {
        if (ht_put(t, base + i * step, 1000 + i)) {
            ok++;
        }
    }
    return ok;
}

/*
 * 清掉计数器 → 对 n 个键各 get 一次 → 返回总共比较过多少个链表节点。
 * 这就是"退化成本"的真实数字，比掐表计时稳定得多（计时会被 CPU 频率、
 * 缓存冷热、系统调度搅浑；比较次数是确定性的、可复现的）。
 */
long ht_probe_after_n_gets(IntHT *t, int n, int base, int step)
{
    if (t == NULL) {
        return 0;
    }
    t->probes = 0;
    int hit = 0;
    for (int i = 0; i < n; i++) {
        int v = 0;
        if (ht_get(t, base + i * step, &v)) {
            hit++;
        }
    }
    long p = t->probes;
    printf("  %d 次 get 全部命中 %d 个，累计比较节点数 = %ld，平均每次 %.2f\n",
           n, hit, p, (n > 0) ? (double)p / (double)n : 0.0);
    return p;
}

/* =====================================================================
 * 8. 字符串键版本（djb2 + strcmp）
 * =====================================================================
 * 和 IntHT 的区别只有三处，其余全是重复 —— 这正是"C 没有泛型"的样子：
 *   (a) 哈希：从 (long)key 换成 ht_hash_string(key)（把一串字符揉成一个整数）
 *   (b) 比较：从 p->key == key 换成 strcmp(p->key, key) == 0
 *   (c) 所有权：key 是 char *，表必须【自己复制一份】，free 时也要【单独 free】
 * 值仍然一律 int。
 */

/*
 * djb2：h = 5381; 每个字符 h = h * 33 + c。
 *
 * 为什么这个看起来随便的公式效果好？
 *   - 乘法把低位信息不断"搅"到高位，加法把新字符塞进低位：
 *     走 L 个字符后，最高几位几乎混入了所有字符的信息（雪崩效应的粗糙版）。
 *   - 33 = 2^5 + 1，h*33 可以写成 (h<<5) + h，一条移位一条加法，历史上很快。
 *   - 初值 5381 也是实测值（作者说它比 0 好：0 会让前几个字符影响不足）。
 * 为什么必须用 unsigned long？
 *   长字符串必然让这个乘法溢出。**无符号溢出 = 对 2^64 取模，C 标准明确定义**；
 *   有符号溢出 = 未定义行为，编译器可以"优化"出任何结果。
 *   顺带一个福利：无符号 `% capacity` 永远非负，连 ht_mod 的平移都不需要
 *   （但本节的 ht_index 仍旧统一走 ht_mod，不给自己留特例）。
 */
unsigned long ht_hash_string(const char *s)
{
    unsigned long h = 5381UL;
    if (s == NULL) {
        return 0;
    }
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        h = h * 33UL + (unsigned long)*p;   /* ★ 先转 unsigned char：
                                             *   char 在某些平台是 signed，中文/扩展 ASCII
                                             *   的字节会是负数，直接进表达式会被符号扩展污染。 */
    }
    return h;
}

/*
 * 自己写 strdup：strdup 是 POSIX 的，不在 ISO C11 里。
 * 编译选项 -std=c11（严格 ANSI）下 glibc 不会声明它，
 * 直接调用会得到 "implicit declaration of function 'strdup'" ——
 * C 语言对隐式声明的处理是"当作返回 int 的函数"，指针被截断成 int，
 * 64 位机上必崩。所以本节手写一份，顺便看清它到底做了什么。
 */
static char *dup_str(const char *s)
{
    if (s == NULL) {
        return NULL;
    }
    size_t len = strlen(s);
    char *p = malloc(len + 1);        /* ★ +1 是给结尾 '\0' 的 —— 少一个字节就少一个终止符 */
    if (p == NULL) {
        return NULL;
    }
    /* 连 '\0' 一起抄：循环上界写 <= len，就自然把终止符也搬过去了 */
    for (size_t i = 0; i <= len; i++) {
        p[i] = s[i];
    }
    return p;
}

/*
 * 字符串表的下标换算：和 int 表同构，只把 ht_hash_int 换成 ht_hash_string，
 * 并且走 ht_mod_u（无符号取余）而不是 ht_mod —— 理由见 ht_mod_u 的注释：
 * 整条链路都保持无符号，就不会有"收窄转换 + 负下标"这一对隐藏坑。
 */
static int sh_bucket_of(const StrHT *t, const char *key)
{
    return (int)ht_mod_u(ht_hash_string(key), (unsigned long)t->capacity);
}

static StrNode *sh_node_new(const char *key, int value)
{
    StrNode *nd = malloc(sizeof(StrNode));
    if (nd == NULL) {
        return NULL;
    }
    nd->key = dup_str(key);         /* ★ 复制，绝不存调用者字符串的地址 */
    if (nd->key == NULL) {
        free(nd);                    /* ★ 回滚：复制失败就把自己刚 malloc 的还掉 */
        return NULL;
    }
    nd->value = value;
    nd->next = NULL;
    return nd;
}

static void sh_push_front(StrNode **head, StrNode *fresh)
{
    fresh->next = *head;
    *head = fresh;
}

StrHT *sh_create(int capacity)
{
    if (capacity <= 0) {
        printf("      [错误] sh_create 的 capacity=%d 必须为正\n", capacity);
        return NULL;
    }
    StrHT *t = malloc(sizeof(StrHT));
    if (t == NULL) {
        return NULL;
    }
    t->buckets = calloc((size_t)capacity, sizeof(StrNode *));
    if (t->buckets == NULL) {
        free(t);
        return NULL;
    }
    t->capacity = capacity;
    t->size = 0;
    t->probes = 0;
    return t;
}

/*
 * 字符串表是**三层**所有权，比 int 表多一层，泄漏风险也多一层：
 *   StrHT 结构体 → buckets 数组 → 每个 StrNode → 每个 node->key 字符串
 * free 顺序必须倒着剥，而且 key 要在 node 之前读不到之前 free 掉：
 *   正确：先 free(p->key)，再 free(p)。（反了就是 use-after-free 的孪生兄弟
 *   "free 之后再也没法知道 key 是什么" —— 先 free(node) 后 node->key 就读不到了。）
 */
void sh_free(StrHT *t)
{
    if (t == NULL) {
        return;
    }
    for (int i = 0; i < t->capacity; i++) {
        StrNode *p = t->buckets[i];
        while (p != NULL) {
            StrNode *nxt = p->next;
            free(p->key);      /* ★ 字符串是独立 malloc 的，必须单独还 */
            free(p);
            p = nxt;
        }
        t->buckets[i] = NULL;
    }
    free(t->buckets);
    t->buckets = NULL;
    free(t);
}

static int sh_rehash(StrHT *t, int new_capacity)
{
    StrNode **fresh = calloc((size_t)new_capacity, sizeof(StrNode *));
    if (fresh == NULL) {
        printf("      [错误] 字符串表 rehash 失败，保持原表不动\n");
        return 0;
    }
    int old_capacity = t->capacity;
    for (int i = 0; i < old_capacity; i++) {
        StrNode *p = t->buckets[i];
        while (p != NULL) {
            StrNode *nxt = p->next;
            int idx = (int)ht_mod_u(ht_hash_string(p->key), (unsigned long)new_capacity);
            sh_push_front(&fresh[idx], p);
            p = nxt;
        }
        t->buckets[i] = NULL;
    }
    free(t->buckets);
    t->buckets = fresh;
    t->capacity = new_capacity;
    return 1;
}

int sh_put(StrHT *t, const char *key, int value)
{
    if (t == NULL || key == NULL) {
        return 0;
    }
    int idx = sh_bucket_of(t, key);
    for (StrNode *p = t->buckets[idx]; p != NULL; p = p->next) {
        t->probes++;
        if (strcmp(p->key, key) == 0) {   /* ★ 必须 strcmp：指针 == 只说明是同一块内存 */
            p->value = value;
            return 1;
        }
    }
    if ((t->size + 1) * 4 > t->capacity * 3) {
        sh_rehash(t, t->capacity * 2);
        idx = sh_bucket_of(t, key);        /* ★ 扩容后重算下标，理由同 ht_put 第 (3) 步 */
    }
    StrNode *fresh = sh_node_new(key, value);
    if (fresh == NULL) {
        return 0;
    }
    sh_push_front(&t->buckets[idx], fresh);
    t->size++;
    return 1;
}

int sh_get(StrHT *t, const char *key, int *value_out)
{
    if (t == NULL || key == NULL) {
        return 0;
    }
    int idx = sh_bucket_of(t, key);
    for (StrNode *p = t->buckets[idx]; p != NULL; p = p->next) {
        t->probes++;
        if (strcmp(p->key, key) == 0) {
            if (value_out != NULL) {
                *value_out = p->value;
            }
            return 1;
        }
    }
    return 0;
}

int sh_contains(StrHT *t, const char *key)
{
    return sh_get(t, key, NULL);
}

int sh_delete(StrHT *t, const char *key)
{
    if (t == NULL || key == NULL) {
        return 0;
    }
    int idx = sh_bucket_of(t, key);
    StrNode dummy;              /* 又是哨兵：删头/删中/删尾一套逻辑 */
    dummy.key = NULL;
    dummy.value = 0;
    dummy.next = t->buckets[idx];

    StrNode *prev = &dummy;
    while (prev->next != NULL) {
        t->probes++;
        if (strcmp(prev->next->key, key) == 0) {
            StrNode *victim = prev->next;
            prev->next = victim->next;
            free(victim->key);   /* ★ 先 free key：此时 victim 还是合法内存 */
            free(victim);        /*   再 free 节点。反了就读不到 victim->key 了 */
            t->buckets[idx] = dummy.next;
            t->size--;
            return 1;
        }
        prev = prev->next;
    }
    t->buckets[idx] = dummy.next;
    return 0;
}

int sh_len(const StrHT *t)
{
    return (t == NULL) ? 0 : t->size;
}

double sh_load_factor(const StrHT *t)
{
    if (t == NULL || t->capacity == 0) {
        return 0.0;
    }
    return (double)t->size / (double)t->capacity;
}

void sh_print(const StrHT *t)
{
    if (t == NULL) {
        return;
    }
    printf("  字符串桶数组 buckets[%d]，size = %d，负载因子 = %.3f\n",
           t->capacity, t->size, sh_load_factor(t));
    for (int i = 0; i < t->capacity; i++) {
        printf("  buckets[%2d]│ ", i);
        if (t->buckets[i] == NULL) {
            printf("NULL\n");
            continue;
        }
        for (const StrNode *p = t->buckets[i]; p != NULL; p = p->next) {
            printf("[%s:%d] -> ", p->key, p->value);
        }
        printf("NULL\n");
    }
}

/*
 * =====================================================================
 * 9. 词频统计：哈希表最常见的真实用途
 * =====================================================================
 * 切词规则（故意简单，但要讲清楚，因为工程里切词就是最容易出错的一层）：
 *   - 字母算进当前单词；
 *   - 非字母（空格、逗号、句号、数字、下划线、连字符）一律当分隔符；
 *   - 大写字母转小写，所以 "The" 和 "the" 记成同一个键
 *     （这一步叫归一化，做与不做决定统计结果完全不同，必须显式选择）。
 * 为什么要用 unsigned char 转 tolower？
 *   tolower 的参数要求"能用 unsigned char 表示或等于 EOF"，
 *   直接把可能是负数的 char 传进去是未定义行为。
 */
int sh_count_words(StrHT *t, const char *text)
{
    if (t == NULL || text == NULL) {
        return 0;
    }
    char word[64];
    int len = 0;
    int total = 0;      /* 一共切出多少个词（重复的也计数） */

    for (const char *p = text;; p++) {
        unsigned char c = (unsigned char)*p;
        int is_letter = isalpha(c) ? 1 : 0;

        if (is_letter) {
            if (len < (int)sizeof(word) - 1) {   /* ★ 留一个字节给 '\0' */
                word[len++] = (char)tolower(c);
            }
            /* 超长词直接截断（真实项目会换成动态缓冲，本节把它当边界情况讲清楚） */
        }
        if ((!is_letter || c == '\0') && len > 0) {
            word[len] = '\0';                    /* ★ 收尾终止符：漏了这一行，
                                                  *   上一个词的尾巴会粘到下一个词上 */
            int old = 0;
            if (sh_get(t, word, &old)) {
                sh_put(t, word, old + 1);        /* 已有 → 计数 +1（覆盖值，不新增键） */
            } else {
                sh_put(t, word, 1);              /* 第一次见 → 计数从 1 开始 */
            }
            total++;
            len = 0;                             /* ★ 缓冲区复位，否则下个词接着写 */
        }
        if (c == '\0') {
            break;   /* 循环条件写成 text;; 就是为了"看到终止符时先处理最后一个词再退出" */
        }
    }
    return total;
}

/*
 * 按"次数降序、同次数按字母升序"排序，输出前 max 个词。
 *
 * ⚠ 排序规则是【写死在函数里】的（因为本节禁用函数指针比较器）。
 *   真实库这里是 `qsort(entries, n, sizeof, cmp)`，比较器当参数传 ——
 *   代价是 cmp 里要用 void * 解引用，类型检查全靠你自己守住。
 *
 * 为什么输出必须排序？
 *   哈希表的桶顺序 = 哈希值顺序，**和插入顺序、字典序都无关**。
 *   所以"遍历哈希表打印"的顺序每次看着像随机 —— 想按词频看榜单，
 *   就必须显式排序（这就是"哈希表不保存顺序"这个特性的直接后果，
 *   见 README 常见的坑第 6 条）。
 */
void sh_print_top(const StrHT *t, int max)
{
    if (t == NULL || max <= 0) {
        return;
    }
    /* 先把所有节点指针收集到一个数组里，才能排序 */
    StrNode **arr = malloc((size_t)t->size * sizeof(StrNode *));
    if (arr == NULL) {
        printf("      [错误] sh_print_top 排序缓冲申请失败\n");
        return;
    }
    int n = 0;
    for (int i = 0; i < t->capacity; i++) {
        for (StrNode *p = t->buckets[i]; p != NULL; p = p->next) {
            arr[n++] = p;
        }
    }

    /* 插入排序：n 小时够快，且代码最短（本节 n ≈ 十几个词）。
     * 排序的是"指针"，不是节点本身 —— 表结构一点没动，arr 只是临时视图。 */
    for (int i = 1; i < n; i++) {
        StrNode *cur = arr[i];
        int j = i - 1;
        /* 比较规则：value 大的排前面；value 相同则 key 字典序小的排前面（保证输出可复现） */
        while (j >= 0 && (arr[j]->value < cur->value ||
               (arr[j]->value == cur->value && strcmp(arr[j]->key, cur->key) > 0))) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = cur;
    }

    int show = (n < max) ? n : max;
    printf("  共 %d 个不同的词，按词频降序（同频按字典序）前 %d 名：\n", n, show);
    for (int i = 0; i < show; i++) {
        printf("    %2d. %-12s %d 次\n", i + 1, arr[i]->key, arr[i]->value);
    }

    free(arr);   /* 临时数组用完就还：它只是"视图"，节点本身仍属于表 */
}
