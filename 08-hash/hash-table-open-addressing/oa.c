#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "oa.h"

/*
 * ===================================================================
 * 开放寻址 + 线性探测的实现。
 *
 * 内存形状和拉链法最大的不同：**只有一块数组**。
 *   拉链法   = 1 次 calloc（指针数组） + size 次 malloc（节点，散落在堆上）
 *   开放寻址 = 1 次 calloc（Slot 数组，元素本体就住在这里）
 * 所以这里 free 只有两级，比拉链法好收摊；代价是"删除"这件事变复杂了 ——
 * 链表摘掉一个节点就完事，数组里腾出一格就等于在探测路上挖了个坑。
 * 本文件的全部设计都绕着这个坑转。
 * ===================================================================
 */

/* 一格的显示宽度（列）与每行画的格子数：整张表的排版全靠这两个数。 */
#define OA_CELL_W 12
#define OA_COLS   8

/*
 * ===================================================================
 * 0. 哈希函数与下标换算（和第 08 章拉链法同一套纪律）
 * ===================================================================
 * 仓库的规矩：键 int、值 int、不许 void *、不许函数指针比较器。
 * 于是哈希函数就是最"傻"的 identity：h(key) = key。
 *
 * 为什么故意写得这么傻？因为开放寻址的全部乐趣在于
 * **"某个键落在第几格"是可以手算的**：
 *     home = key % capacity，然后 4 → 5 → 6 一格一格往后挪。
 * 哈希函数一复杂，你就只能相信程序；而这一节要求你**自己算一遍再对答案**。
 *
 * ⚠ 下标换算必须经过非负保证：C 的 % 是截断除法的余数，符号跟被除数，
 *   `-1 % 8` 在 C 里是 `-1`，拿去访问 slots[-1] 就是堆越界读写 ——
 *   编译器不拦，ASan 拦。修法只许"同余平移 r += m"，不许"取绝对值"：
 *   -3 和 3 在模 8 下是**不同的格子**（-3 ≡ 5），取绝对值会让同一个键
 *   两次算出两个 home，探测链当场乱掉，正确性直接归零。
 */
static long oa_hash_int(int key)
{
    return (long)key;
}

static int oa_mod(long value, int m)
{
    if (m <= 0) {
        /* capacity 必须是正数。真到这里就是调用方逻辑错了。
         * 打印警告而不是静默返回 0：静默会让所有键挤进槽 0，
         * 表还能"正确工作"（探测步数爆炸而已），bug 就被藏到几个月后。 */
        printf("      [错误] oa_mod 的除数 m=%d 不合法，按 1 处理\n", m);
        m = 1;
    }
    long r = value % m;      /* C 的 %：符号跟 value */
    if (r < 0) {
        r += m;              /* 平移进 [0, m)：同余，不改变"谁和谁撞"的关系 */
    }
    return (int)r;
}

/*
 * 键的 home（第一选择格）。全文件**唯一**做"键 → 格子"换算的地方，
 * 所有探测都从它出发，所以 capacity 一变（搬家）home 就全变 ——
 * 这就是"搬家必须把所有元素重放一遍"的根源。
 */
static int oa_home_of(int key, int capacity)
{
    return oa_mod(oa_hash_int(key), capacity);
}

/*
 * 环上走一步：线性探测的下一步是 idx+1，走到数组尾巴要折回 0。
 * 为什么写成"加法 + 判断"而不是 `(idx + 1) % capacity`？
 *   ① % 是除法指令，探测循环里每步一次，值得省掉；
 *   ② 把"环形"这件事集中到一处，别处只看见"下一个格子"。
 *   ③ 而且这里入参一定是合法下标，不需要 oa_mod 的非负保证 —— 别把两个职责混一层。
 */
static int oa_next_idx(int idx, int capacity)
{
    int n = idx + 1;
    if (n >= capacity) {
        n -= capacity;      /* 折回数组头：环形数组 */
    }
    return n;
}

/* =====================================================================
 * 1. 建表 / 拆表
 * ===================================================================== */

/*
 * 建表：结构体一次 malloc + 格子数组一次 calloc，总共就这两块内存。
 *
 * 为什么用 calloc 而不是 malloc + 手写循环置 EMPTY？
 *   头文件定死了一条：**SLOT_EMPTY 必须是 0**。
 *   calloc 把内存全填 0 位模式，于是每一格天然就是 EMPTY，
 *   顺便消灭"漏初始化某一格 → state 是垃圾值 → 被当成 USED"这类 bug
 *   （那种情况下 get 会读到随机 key，探测还会误判"占用不匹配"一路走过去，
 *     症状是"偶尔查不到明明存过的键" —— 最难查的一类）。
 *
 * 为什么 capacity 不强制是 2 的幂？
 *   这里**不需要**：我们用 `% capacity` 而不是 `& (capacity-1)`，任何正整数都合法。
 *   本节全程用 2 的幂，只是为了让"扩容翻倍 → home 怎么变"那条规律好算好看。
 */
OATable *oa_create(int capacity)
{
    if (capacity <= 0) {
        printf("      [错误] oa_create 的 capacity=%d 必须为正\n", capacity);
        return NULL;
    }

    OATable *t = malloc(sizeof(OATable));
    if (t == NULL) {
        return NULL;
    }

    t->slots = calloc((size_t)capacity, sizeof(Slot));
    if (t->slots == NULL) {
        free(t);          /* ★ 两级分配就要两级回滚：漏这行是"内存紧张路径上的泄漏" */
        return NULL;
    }

    t->capacity = capacity;
    t->size = 0;
    t->tombs = 0;
    t->used = 0;                        /* used = size + tombs，是派生值 */
    t->probes = 0;
    t->del_mode = OA_DEL_TOMBSTONE;     /* ★ 默认必须是正确的那条路（仓库规矩） */
    return t;
}

/*
 * 拆表：两级，从里往外。
 *   第 1 层 slots 数组（1 次 free，里面所有元素跟着一起还）
 *   第 2 层 OATable 结构体（1 次 free）
 * 对比拉链法的三层（结构体 → 桶数组 → 每个节点）：开放寻址**没有 per-element malloc**，
 * 所以不存在"漏 free 某个节点"这种泄漏 —— 这是它除了缓存友好之外第二个实打实的好处。
 * ⚠ 前提是没有键级所有权：int 键不含内存，所以这里两级就干净了。
 *   要做字符串版，Slot 里的 key 就是第 3 层，free 必须一格一格剥。
 */
void oa_free(OATable *t)
{
    if (t == NULL) {
        return;
    }
    free(t->slots);
    t->slots = NULL;      /* 顺手置 NULL：防止有人接着遍历这张已拆的表 */
    free(t);
}

/* =====================================================================
 * 2. 探测：本文件的心脏（规则只写一遍）
 * =====================================================================
 * 查找规则（头文件第三节讲的就是这四条）：
 *   从 home 出发，一格一格往后看
 *     USED 且 key 相等 → 命中，停
 *     USED 但 key 不同 → 别人的东西，不停，继续
 *     TOMB            → 这里住过、被删了，不停，继续   ← ★ 本节的灵魂
 *     EMPTY           → 判定"键不存在"，停
 *
 * ⚠ EMPTY 是**唯一**的停止标志。谁把删除的格子写成 EMPTY，
 *   就等于在别人的探测路径上挖了个坑：后面那串元素再也走不到（断链）。
 *   这就是 --bug 要现场演示的事故。
 *
 * 循环上界为什么是 capacity 而不是"直到遇到 EMPTY"？
 *   因为存在"一个 EMPTY 都没有"的状态（used == capacity：全是 USED 和墓碑）。
 *   写 `while (state != EMPTY)` 就是**死循环** —— 程序不崩、不报错，只是不动。
 *   有界循环把这种状态变成"探测 capacity 格后回答不存在"，
 *   并且给 oa_put 留出"表满了，必须先搬家"的判断口。
 *
 * 为什么 get / contains / delete / put / probe_path 全共用这一个 core？
 *   因为探测规则一旦抄成第二份，两份就会开始"各说各话"：
 *   比如 put 忘了墓碑能回收、delete 多认了一次 EMPTY 就少删一个键。
 *   差异全部用"可选的输出参数"表达（传 NULL 就是不要那个信息）。
 */
static int oa_probe_core(const OATable *t, int key,
                         int *path_out, int path_max,
                         int *found_out, int *slot_out, int *tomb_out)
{
    if (found_out != NULL) {
        *found_out = 0;
    }
    if (slot_out != NULL) {
        *slot_out = -1;
    }
    if (tomb_out != NULL) {
        *tomb_out = -1;
    }
    if (t == NULL || t->capacity <= 0 || t->slots == NULL) {
        return 0;
    }

    const int cap = t->capacity;
    int idx = oa_home_of(key, cap);
    int steps = 0;

    for (steps = 0; steps < cap; steps++) {
        if (path_out != NULL && steps < path_max) {
            path_out[steps] = idx;      /* 调用方要的就是"经过哪些下标"这件事本身 */
        }
        const Slot *s = &t->slots[idx];

        if (s->state == SLOT_EMPTY) {
            /* 真·空槽：停在这里判定不存在。这是唯一合法的"没有"证据。 */
            if (slot_out != NULL) {
                *slot_out = idx;        /* 空槽本身也报出去：put 要拿它当插入点 */
            }
            return steps + 1;           /* 步数算上这一格：我们确实看了它 */
        }
        if (s->state == SLOT_USED) {
            if (s->key == key) {
                if (found_out != NULL) {
                    *found_out = 1;
                }
                if (slot_out != NULL) {
                    *slot_out = idx;
                }
                return steps + 1;
            }
            /* USED 但不是这个键：别人的家，继续走 */
        } else { /* SLOT_TOMB */
            if (tomb_out != NULL && *tomb_out < 0) {
                *tomb_out = idx;        /* 只记第一个：越靠前的墓碑离 home 越近，回收它最划算 */
            }
        }
        idx = oa_next_idx(idx, cap);
    }

    /* 一整圈都没撞到 EMPTY：表里没有空格，无法断定"不存在"。
     * 0.5 阈值保证至少一半是空的，所以正常路径走不到这里；
     * 走到这里说明状态被外部改坏了 —— 返回 cap，让调用方自己决定搬家。 */
    return cap;
}

/*
 * 公开版：只抄探测序列 + 回答在不在，别的都不给（所以签名里有 NULL 占位）。
 * 它是 const：观测函数不该有副作用，不能污染 t->probes。
 */
int oa_probe_path(const OATable *t, int key, int *path_out, int path_max, int *found_out)
{
    return oa_probe_core(t, key, path_out, path_max, found_out, NULL, NULL);
}

/*
 * get：命中返回 1 并把值写进 value_out；value_out 传 NULL 表示只想知道有没有。
 *
 * 为什么这里要动 t->probes，而上面的 oa_probe_path 是 const？
 *   "抄探测序列"是**观测**，看别人的表也不该改别人；
 *   "这张表累计探测了多少格"是**使用**行为，只有真在用的 get/put/delete 才该记账。
 *   两个职责分成两个函数，main.c 就能既打印又统计，互不污染。
 *   代价：oa_get 不能声明成 const OATable *（为了可观测性牺牲接口纯度）。
 */
int oa_get(OATable *t, int key, int *value_out)
{
    if (t == NULL) {
        return 0;
    }
    int found = 0;
    int slot = -1;
    int steps = oa_probe_core(t, key, NULL, 0, &found, &slot, NULL);
    t->probes += steps;
    if (!found) {
        return 0;
    }
    if (value_out != NULL) {
        *value_out = t->slots[slot].value;
    }
    return 1;
}

int oa_contains(OATable *t, int key)
{
    return oa_get(t, key, NULL);   /* 复用：值不要，只要"在不在" */
}

/* =====================================================================
 * 3. 删除：本节的主角
 * =====================================================================
 * 命中之后那一格写什么，是本节唯一真正的技术决定：
 *
 *   OA_DEL_TOMBSTONE（默认，✅ 正确）
 *     state = SLOT_TOMB。语义："这里曾经有东西，别在这里停下。"
 *     size-- 而 tombs++，所以 used = size + tombs **不变**：格子仍然挡路，
 *     装载率的分子仍然要算它。这不是记账洁癖，而是它**真的还在拖慢查找**。
 *
 *   OA_DEL_ZERO（❌ 错误演示，只在 --bug 时打开）
 *     state = SLOT_EMPTY。当场剪断所有"路过这里"的键的探测链：
 *     数据还在数组里，程序却说"不存在"；而且 size 和 used 都减了 ——
 *     记账看起来完全干净，装载率还挺好看，没有任何警报。
 *     **这就是最难查的 bug 类型：不崩溃、不报错，只是静默丢数据。**
 *
 * ⚠ 线性探测下删除只能"原地写状态"，绝不能"把后面的元素往前挪一格"。
 *   拉链法可以顺手重排链里的节点（桶内顺序无所谓），线性探测不行：
 *   要把 5 挪到 4，必须先确认"键 5 的 home 就是 4"（挪完还得算回 4 才行），
 *   不满足就得连锁往后挪，一路传到那个 EMPTY 为止 —— 那是 Robin Hood 表干的事。
 */
int oa_delete(OATable *t, int key)
{
    if (t == NULL) {
        return 0;
    }
    int found = 0;
    int slot = -1;
    int steps = oa_probe_core(t, key, NULL, 0, &found, &slot, NULL);
    t->probes += steps;
    if (!found) {
        return 0;            /* 本来就没有：删除幂等，绝不会二次修改状态 */
    }

    Slot *s = &t->slots[slot];
    if (t->del_mode == OA_DEL_ZERO) {
        s->state = SLOT_EMPTY;      /* ❌ 在探测路径上挖坑 */
        s->key = 0;
        s->value = 0;               /* 顺手清内容：让"数据明明还在那儿"这件事看不出来，
                                     *   这才是真实世界这类 bug 最难查的地方 */
    } else {
        s->state = SLOT_TOMB;       /* ✅ 立碑：这一格仍然挡路，但挡的是合法的路 */
        t->tombs++;
        /* key/value 故意留着：墓碑不需要身份，留着旧值方便你盯着数组看历史。
         * 真实实现会清掉（键若是指针，留着就是悬空引用）。本节键是 int，留着无害。 */
    }
    t->size--;
    t->used = t->size + t->tombs;   /* ★ used 是派生值：改完 size/tombs 立刻重算，
                                     *   绝不让三个计数器各说各话（手写 ++/-- 迟早漏一条路径） */
    return 1;
}

/* =====================================================================
 * 4. put：插入或覆盖（含"要不要搬家"的决策）
 * =====================================================================
 * 比拉链法的 put 多出三个开放寻址特有的难点：
 *   ① 探测途中要**顺手记住第一个墓碑**，键确实不存在时优先回收它
 *      —— 否则墓碑只增不减，装载率一路涨到不得不搬家。
 *   ② 装载率的分子是 used（USED + 墓碑），不是 size。
 *      墓碑不装数据，但它挡路；挡路就要算账。
 *   ③ 搬家（扩容或同容量重排）之后，home、墓碑位置、空槽位置全部作废 →
 *      **必须从头再走一遍**。拉链法强调"重算下标"，这里是"整个操作重跑"，
 *      写法上反而更不容易漏：循环在顶上，重跑就是 continue。
 *
 * 为什么"回收墓碑"不需要看装载率，"占新空槽"才需要？
 *   回收墓碑：墓碑 → USED，挡路格子数守恒 → used 不变 → 不必搬家。
 *   占空槽：EMPTY → USED，used +1 → 必须检查会不会越过阈值。
 *   这一条区别就是"分子是 used 而不是 size"的最直接用处。
 */
int oa_put(OATable *t, int key, int value)
{
    if (t == NULL || t->capacity <= 0) {
        return 0;
    }

    for (;;) {
        const int cap = t->capacity;
        int found = 0;
        int slot = -1;        /* 命中格 or 撞到的那个 EMPTY */
        int tomb = -1;        /* 探测路上第一个墓碑 */
        int steps = oa_probe_core(t, key, NULL, 0, &found, &slot, &tomb);
        t->probes += steps;   /* ★ 计的是 core 真正走过的格数，别自己瞎写 +1 */

        /* (1) 键已存在 → 覆盖值，size 不动 ★（"覆盖"不是"插入"，
         *      漏了这一步，反复 put 同一个键会把表撑爆） */
        if (found) {
            t->slots[slot].value = value;
            return 1;
        }

        /* slot 到这里 = 撞到的 EMPTY（-1 表示绕了一圈没找到 EMPTY） */

        /* (2) 定落点：优先回收墓碑，其次用空槽 */
        int target = (tomb >= 0) ? tomb : slot;

        /* (3) 需要搬家吗？只有"要占一个新空槽"才增加 used */
        if (target < 0) {
            /* 既没墓碑也没空槽：used == capacity，表被塞满。
             * 0.5 阈值下正常走不到这里，但绝不留死循环 —— 强制翻倍。
             * 写死这一条是"最坏情况也要有一条出路"，
             * 而不是给三个月后的自己留一个静默卡死的现场。 */
            printf("      [兜底] 表中无任何空槽/墓碑（used=%d, capacity=%d），强制翻倍\n",
                   t->used, cap);
            oa_rehash(t, cap * 2);
            continue;                 /* ★ 数组换了，上面的结论全部作废 */
        }
        if (t->slots[target].state == SLOT_EMPTY && (t->used + 1) * 2 > cap) {
            /* 整数乘法比较，不写浮点 `> 0.5`：double 有舍入，
             * "恰好一半"这种边界值今天相等、明天换个 capacity 就判错。
             * 这里是严格大于，所以 used 刚好 == cap/2 时不动：
             * 装载率能升到正好 0.500（第 1 幕会让你亲眼看到 4/8 = 0.500）。 */
            if (t->tombs > 0 && (t->size + 1) * 2 <= cap) {
                /* 挡路的格子里有一堆是墓碑，而真元素还塞得下 → 同容量重排就够，不翻倍。
                 * 这一招是开放寻址独有的：拉链法没有"挡路"这回事，用不着。 */
                printf("      [整理] 装载率 (%d+1)/%d 越过 0.5 阈值，挡路的 %d 格里有 %d 个是墓碑"
                       " → 同容量(%d)重排，把墓碑蒸发掉\n",
                       t->used, cap, t->used, t->tombs, cap);
                oa_rehash(t, cap);
            } else {
                printf("      [扩容] 装载率 (%d+1)/%d 越过 0.5 阈值（真元素 %d、墓碑 %d）"
                       " → 格子数组 %d → %d\n",
                       t->used, cap, t->size, t->tombs, cap, cap * 2);
                oa_rehash(t, cap * 2);
            }
            continue;                 /* ★ 搬家后重新探测：home 和落点都变了 */
        }

        /* (4) 落子 */
        Slot *d = &t->slots[target];
        if (d->state == SLOT_TOMB) {
            t->tombs--;               /* 墓碑被回收：挡路格子没变，真元素多了一个 */
        }
        d->key = key;
        d->value = value;
        d->state = SLOT_USED;
        t->size++;
        t->used = t->size + t->tombs; /* 派生字段立刻重算 */
        return 1;
    }
}

/* =====================================================================
 * 5. 只读小查询 / 删除模式开关
 * ===================================================================== */

int oa_len(const OATable *t)
{
    return (t == NULL) ? 0 : t->size;    /* O(1)：一路维护着 size。注意：不含墓碑！ */
}

int oa_tomb_count(const OATable *t)
{
    return (t == NULL) ? 0 : t->tombs;
}

/*
 * 装载率：**分子是 used（USED + 墓碑）**。本节最重要的一根弦。
 * 拉链法的分子就是 size，因为空桶不挡路 —— 每个桶是独立的一条链，
 * 别的桶再长也不影响我这一跳。
 * 开放寻址的墓碑挡路（查找必须走过去），所以它和真元素一样贵。
 * 这就是同一个"表该多大"的问题，两种做法给出不同分母/分子的原因。
 */
double oa_load_factor(const OATable *t)
{
    if (t == NULL || t->capacity <= 0) {
        return 0.0;
    }
    return (double)t->used / (double)t->capacity;
}

/* 真实数据密度 size/capacity。和上面那个数并排一放，墓碑的代价就看得见。 */
double oa_real_load(const OATable *t)
{
    if (t == NULL || t->capacity <= 0) {
        return 0.0;
    }
    return (double)t->size / (double)t->capacity;
}

/*
 * 切换删除策略。默认是 OA_DEL_TOMBSTONE（正确），
 * 本函数存在的唯一意义是让 main.c 能在 --bug 开关后打开那条**错误**的路，
 * 把"断链"跑出真实结果给你看 —— 然后立刻切回来。
 */
void oa_set_delete_mode(OATable *t, int mode)
{
    if (t == NULL) {
        return;
    }
    t->del_mode = mode;
}

const char *oa_mode_name(int mode)
{
    switch (mode) {
    case OA_DEL_TOMBSTONE:
        return "墓碑（正确）";
    case OA_DEL_ZERO:
        return "直接清零（错误演示）";
    default:
        return "未知模式";
    }
}

/* =====================================================================
 * 6. 看见它：整张格子数组 + 一次查找的探测路径
 * =====================================================================
 * 这一节的核心可视化就是"数组 + 挪格子"，所以打印函数不是附属品，
 * 它就是教学内容本身：你得一眼看出"4、5、6 连成一串"、"4 是个 †"。
 */

/*
 * 补空格到固定显示宽度。
 * ⚠ 为什么不直接 printf("%-12s", body)？
 *   · 是 2 字节、† 是 3 字节的 UTF-8，屏幕上却只占 **1 列**；
 *   printf 的宽度按**字节**补齐，直接用会让这两种格子比旁边的短 1~2 格，
 *   整张表歪歪扭扭 —— 而这张表的全部价值就在"对齐"。
 *   所以宽度由调用方把"显示列数"算好传进来（disp），这里只负责补空格。
 */
static void pad_cell(char *out, size_t outsz, const char *body, int disp)
{
    int pad = OA_CELL_W - disp;
    if (pad < 0) {
        pad = 0;
    }
    snprintf(out, outsz, "%s%*s", body, pad, "");
}

/* 一格排成固定宽度的文本：空槽 ·、墓碑 †、占用 U key=value */
static void slot_cell(const Slot *s, char *out, size_t outsz)
{
    char body[48];
    int sym_bytes = 1;

    if (s->state == SLOT_USED) {
        snprintf(body, sizeof body, "U %d=%d", s->key, s->value);
    } else if (s->state == SLOT_TOMB) {
        snprintf(body, sizeof body, "†");
        sym_bytes = (int)strlen(body);        /* 3 字节 */
    } else {
        snprintf(body, sizeof body, "·");
        sym_bytes = (int)strlen(body);        /* 2 字节 */
    }
    /* 显示列数 = 字节数 -（符号占的字节）+（符号只占 1 列） */
    pad_cell(out, outsz, body, (int)strlen(body) - sym_bytes + 1);
}

/* 画一条横线（┌────┬────┐ 那种），cols = 这一行几个格子 */
static void grid_rule(const char *left, const char *mid, const char *right, int cols)
{
    printf("  %s", left);
    for (int c = 0; c < cols; c++) {
        printf("────────────");
        printf("%s", (c < cols - 1) ? mid : right);
    }
    printf("\n");
}

/* 一行的单元格外壳：│cell│cell│……│cell */
static void grid_cells(const OATable *t, int row0, int cols, int show_numbers)
{
    char cell[OA_CELL_W * 3];
    char body[OA_CELL_W * 3];

    printf("  │");
    for (int c = 0; c < cols; c++) {
        int i = row0 + c;
        if (show_numbers) {
            snprintf(body, sizeof body, "slot %d", i);
            pad_cell(cell, sizeof cell, body, (int)strlen(body));
        } else {
            slot_cell(&t->slots[i], cell, sizeof cell);
        }
        printf("%s│", cell);
    }
    printf("\n");
}

void oa_print(const OATable *t)
{
    if (t == NULL || t->capacity <= 0) {
        printf("  （空表，没东西可画）\n");
        return;
    }

    printf("  slots[%d] │ USED %d · 墓碑 %d · 空槽 %d │ 装载率 used/cap = %.3f"
           " │ 真实密度 size/cap = %.3f\n",
           t->capacity, t->size, t->tombs, t->capacity - t->used,
           oa_load_factor(t), oa_real_load(t));

    /* 大图只画前面一段：1024 格全画会把屏幕埋掉；
     * "整张表的形态"交给 oa_stats 的连跑统计和那条 ▓▒░ 缩略图。 */
    const int show = (t->capacity < 24) ? t->capacity : 24;

    for (int row0 = 0; row0 < show; row0 += OA_COLS) {
        int cols = (show - row0 < OA_COLS) ? (show - row0) : OA_COLS;
        grid_rule("┌", "┬", "┐", cols);
        grid_cells(t, row0, cols, 1);
        grid_rule("├", "┼", "┤", cols);
        grid_cells(t, row0, cols, 0);
        grid_rule("└", "┴", "┘", cols);
    }

    if (t->capacity > show) {
        printf("  （只画前 %d / %d 格，其余 %d 格全在后面；整表形态见下面的 oa_stats）\n",
               show, t->capacity, t->capacity - show);
    }
    printf("  图例：· 空槽（查找走到这里就【停】） † 墓碑（不停，继续走） U key=value 占用\n");
}

/*
 * 打印一次查找经过的探测序列，例如：
 *   查找 key=20 │ home=4 │ 经过：槽 4(† 墓碑→继续) → 槽 5(U 命中→停) │ 2 步，命中 ✅
 * 这一行字就是"开放寻址"四个字的实体 —— 拉链法对应的说法是"走了几个节点"。
 * ⚠ 它用 const 的 oa_probe_path 而不是 oa_get：打印不该改表，也不该污染计数器。
 */
void oa_print_probe_path(const OATable *t, int key)
{
    if (t == NULL || t->capacity <= 0) {
        return;
    }
    int path[64];
    int found = 0;
    int steps = oa_probe_path(t, key, path, 64, &found);
    int shown = (steps < 64) ? steps : 64;

    printf("  查找 key=%d │ home=%d │ 经过：", key, oa_home_of(key, t->capacity));
    for (int i = 0; i < shown; i++) {
        const Slot *s = &t->slots[path[i]];
        const char *tag;
        if (s->state == SLOT_EMPTY) {
            tag = "· 空槽→停";
        } else if (s->state == SLOT_TOMB) {
            tag = "† 墓碑→不停，继续";
        } else if (s->key == key) {
            tag = "U 命中→停";
        } else {
            tag = "U 别人的键→不停";
        }
        printf("%s槽 %d(%s)", (i > 0) ? " → " : "", path[i], tag);
    }
    if (found) {
        printf(" │ %d 步，命中 ✅\n", steps);
    } else {
        printf(" │ %d 步，判定【不存在】❌（只有撞到真·空槽才敢这么说）\n", steps);
    }
    if (steps > 64) {
        printf("      （路径超过 64 格，只画前 64 格；总步数仍是 %d）\n", steps);
    }
}

/*
 * 表的整体形态。除了记账数字，这里多做两件"只有打印函数会做"的事：
 *   1) 最长连跑（longest run）：primary clustering 的直接度量。
 *      拉链法没有这个概念 —— 每个桶独立，一条链再长也不影响别的桶；
 *      开放寻址的冲突会**互相把对方的探测路径拉长**，连跑就是这种传染的产物。
 *      注意切断连跑的是 EMPTY，墓碑【不】切断：墓碑照样挡路。
 *   2) 对表里每个现存键跑一次 const 探测，量出"当前形态下的平均/最长步数"。
 *      这里必须用 oa_probe_path（const、无副作用）而不是 oa_get：
 *      统计函数不该污染 t->probes 这个业务计数器，否则"累计探测格数"就没法解释了。
 */
void oa_stats(const OATable *t)
{
    if (t == NULL || t->capacity <= 0) {
        return;
    }
    const int cap = t->capacity;

    int empty = 0, longest = 0, cur = 0, runs = 0;
    for (int i = 0; i < cap; i++) {
        if (t->slots[i].state == SLOT_EMPTY) {
            empty++;
            cur = 0;
        } else {
            cur++;
            if (cur > longest) {
                longest = cur;
            }
            if (cur == 1) {
                runs++;                  /* 一段挡路区间的开始 */
            }
        }
    }

    long total = 0;
    int maxstep = 0;
    for (int i = 0; i < cap; i++) {
        if (t->slots[i].state != SLOT_USED) {
            continue;
        }
        int found = 0;
        int steps = oa_probe_path(t, t->slots[i].key, NULL, 0, &found);
        total += steps;
        if (steps > maxstep) {
            maxstep = steps;
        }
    }

    printf("  格子数组 capacity=%d │ USED=%d │ 墓碑=%d │ 空槽=%d\n",
           cap, t->size, t->tombs, empty);
    printf("  used = size + tombs = %d + %d = %d\n", t->size, t->tombs, t->used);
    printf("  装载率 used/capacity = %.3f  ← 触发搬家的判据（本节阈值 0.5）\n", oa_load_factor(t));
    printf("  真实密度 size/capacity = %.3f  ← 两个数的差 %.3f 就是"
           "白白挡路的墓碑\n",
           oa_real_load(t), oa_load_factor(t) - oa_real_load(t));
    printf("  挡路区间（连跑）%d 段，最长 %d 格 ← primary clustering 的度量\n", runs, longest);
    if (t->size > 0) {
        printf("  把现存 %d 个键逐个查一遍：平均 %.2f 步，最长 %d 步\n",
               t->size, (double)total / (double)t->size, maxstep);
    } else {
        printf("  表里没有真元素，"
               "逐个查找那一项跳过\n");
    }
    printf("  累计探测格数 t->probes = %ld（put/get/contains/delete 都往里加）\n", t->probes);

    /* 缩略图：一格一个字符，长串一眼看出来 */
    const int bar = (cap < 64) ? cap : 64;
    printf("  形态：");
    for (int i = 0; i < bar; i++) {
        printf("%s", (t->slots[i].state == SLOT_USED) ? "▓"
               : (t->slots[i].state == SLOT_TOMB) ? "▒" : "░");
    }
    if (cap > bar) {
        printf(" …（只画前 %d 格）", bar);
    }
    printf("  （▓=占用 ▒=墓碑 ░=空）\n");
}

/* =====================================================================
 * 7. 搬家：扩容 / 同容量重排（本节第二颗心脏）
 * =====================================================================
 * 为什么必须搬家而不是"在旁边再接一段数组"？
 *   home = h(key) % capacity，**capacity 一变，每个键的 home 都变了**。
 *   老键留在老下标，下次 get 按新 capacity 算 home → 走不到那一格 → 数据凭空消失。
 *
 * 为什么这里比拉链法更痛？
 *   拉链法搬家只重接 next 指针，节点本体一个字节都不动。
 *   开放寻址必须把 key/value **逐字段复制进新数组**（这里恰好 12 字节，很便宜），
 *   更要紧的是：复制之后**真实落点由探测决定**，不再是 h % cap 一个公式能算准的。
 *   所以下面 home 只是"起点"，真实落点是 while 循环挪出来的。
 *
 * new_capacity == 旧 capacity 时是"同容量重排"：元素没换桶数，
 *   但被重新摊平，墓碑全部留在旧数组里、随 free 一起消失 —— 这就是"蒸发墓碑"。
 *   拉链法没有这个需求（它没有"挡路"这回事）。
 */
int oa_rehash(OATable *t, int new_capacity)
{
    if (t == NULL || new_capacity <= 0) {
        return 0;
    }

    Slot *fresh = calloc((size_t)new_capacity, sizeof(Slot));
    if (fresh == NULL) {
        /* ★ 失败就整张表维持原样：装载率高一点只是慢，不是错。
         *   "宁可慢也不要崩"是哈希表能长期跑的关键（Redis 增量 rehash 同动机）。 */
        printf("      [错误] 搬家申请新数组失败（要 %d 格 × %zu 字节），原表不动\n",
               new_capacity, sizeof(Slot));
        return 0;
    }

    const int old_cap = t->capacity;
    int old_tombs = t->tombs;
    int placed = 0;      /* 被复制进新数组的元素数 */
    int changed = 0;     /* 其中槽号变了的 */

    for (int i = 0; i < old_cap; i++) {
        const Slot *s = &t->slots[i];
        if (s->state != SLOT_USED) {
            continue;    /* 墓碑被丢在旧数组里：free 那一刻就蒸发了 */
        }
        int j = oa_home_of(s->key, new_capacity);
        /* 新数组里线性探测到空位 —— 这一步才是"真实落点" */
        while (fresh[j].state != SLOT_EMPTY) {
            j = oa_next_idx(j, new_capacity);
        }
        fresh[j].key = s->key;        /* 复制本体（拉链法只需重接指针） */
        fresh[j].value = s->value;
        fresh[j].state = SLOT_USED;
        placed++;
        if (j != i) {
            changed++;
        }
    }

    free(t->slots);                   /* 旧数组整体还掉：元素没有 per-element malloc，好收摊 */
    t->slots = fresh;
    t->capacity = new_capacity;
    t->tombs = 0;                     /* 搬家顺手把墓碑清零 */
    t->used = t->size + t->tombs;     /* == size：这就是"墓碑蒸发"的账面效果 */
    printf("      [搬家] 重新放置 %d 个元素到 slots[%d]，其中 %d 个槽号变了；"
           "墓碑 %d → 0（全部蒸发）\n", placed, new_capacity, changed, old_tombs);
    return placed;
}

/*
 * 打印"预测"：每个键在新 capacity 下的 home 是几，和旧 home 对比，
 * 顺便现场验算"翻倍扩容只动一位"那条规律。
 *
 * ⚠ 头文件特别强调：这里给的只是 **home（第一选择格）**，不是真实落点。
 *   开放寻址里"键 K 落在哪一格"永远取决于**当时整个数组的占用情况**，
 *   一个公式算不出来 —— 这是它和拉链法最本质的差别：
 *     拉链法：key → 桶，一一对应，桶里再挂链，别的桶跟我不相干。
 *     开放寻址：key → home → 探测 → 落点，落点是"全表状态"的函数。
 *   所以预测只能对 home 负责，真实落点留给调用方搬家后逐个量（main.c 第 6 幕就这么做）。
 *
 * 2 的幂规律（h = q*C + r，r = 旧 home）：
 *     q 偶 → 新 home = r（不动）
 *     q 奇 → 新 home = r + C（搬走）
 *   证明两行：q 偶时 h = (q/2)*(2C) + r；q 奇时 h = ((q-1)/2)*(2C) + (r + C)。
 *   拉链法靠这条让"一个旧桶最多裂成两个新桶"；开放寻址同样成立，
 *   但它只保证 home，落点还得挤一挤。
 */
void oa_rehash_plan(const OATable *t, int new_capacity)
{
    if (t == NULL || new_capacity <= 0) {
        return;
    }
    printf("  预测：capacity %d → %d 之后每个键的 home 变成几（只看非负键，翻倍规律才好验）\n",
           t->capacity, new_capacity);
    printf("   旧槽号      键   旧home  新home  h/C 的商 q  q奇偶  按规律推  home吻合?\n");
    printf("  -------------------------------------------------------------------------\n");

    int elements = 0;
    for (int i = 0; i < t->capacity; i++) {
        const Slot *s = &t->slots[i];
        if (s->state != SLOT_USED) {
            continue;
        }
        elements++;
        long h = oa_hash_int(s->key);
        int new_home = oa_home_of(s->key, new_capacity);
        if (h < 0) {
            printf("  %7d %8d %7d %7d %8s   （键为负，跳过规律验算：C 的 %% 会给负余数）\n",
                   i, s->key, oa_home_of(s->key, t->capacity), new_home, "-");
            continue;
        }
        int old_home = (int)(h % t->capacity);
        long q = h / t->capacity;
        int odd = (q % 2 != 0) ? 1 : 0;
        int predicted = old_home + (odd ? t->capacity : 0);
        printf("  %7d %8d %7d %7d %8ld %6s %9d %8s\n",
               i, s->key, old_home, new_home, q, odd ? "奇" : "偶", predicted,
               (predicted == new_home) ? "✅" : "❌");
    }
    if (elements == 0) {
        printf("  （表里没有 USED 格子，无家可搬）\n");
    }
    printf("  注意：以上是 home，不是真实落点。真搬家时同一个 home 的元素要互相让位、往后挪。\n");
}

/* =====================================================================
 * 8. 灌数据 & 数探针（实测"聚集"与"退化"的仪器）
 * ===================================================================== */

/*
 * 灌 n 个键：base, base+step, base+2*step, ...
 * step = 1          → 键连着、home 也连着 → 整张表挤成一长串（顺序插入的最坏情形）
 * step = capacity   → 所有键同一个 home  → 单点塌缩，探测步数 = 1,2,3,...,n
 * step 与 capacity 互质（如 33 对 1024）→ home 均匀铺满全表，几乎不撞
 * 返回成功插入的个数。
 */
int oa_fill_arithmetic(OATable *t, int n, int base, int step)
{
    int ok = 0;
    for (int i = 0; i < n; i++) {
        if (oa_put(t, base + i * step, 1000 + i)) {
            ok++;
        }
    }
    return ok;
}

/*
 * 清零计数器 → 把 n 个键各 get 一次 → 返回这一轮总共探测了多少格。
 * 为什么数格子而不是掐表计时？
 *   计时会被 CPU 频率、缓存冷热、系统调度搅浑；
 *   探测格数是**确定性的**：同样一批键、同一个 capacity，重跑一百遍数字一样。
 *   教学要的就是"能印在 README 上、你自己能复算"的数字。
 */
long oa_probe_after_n_gets(OATable *t, int n, int base, int step)
{
    if (t == NULL) {
        return 0;
    }
    t->probes = 0;
    int hit = 0;
    for (int i = 0; i < n; i++) {
        int v = 0;
        if (oa_get(t, base + i * step, &v)) {
            hit++;
        }
    }
    long p = t->probes;
    printf("  %d 次 get 命中 %d 个，累计探测 %ld 格，平均每次 %.2f 格\n",
           n, hit, p, (n > 0) ? (double)p / (double)n : 0.0);
    return p;
}
