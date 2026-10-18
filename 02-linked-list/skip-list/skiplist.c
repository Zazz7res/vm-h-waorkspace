#include <stdio.h>
#include <stdlib.h>
#include <limits.h>      /* INT_MIN / INT_MAX，两个哨兵的 key 就从这里借 */
#include "skiplist.h"

/*
 * ===================================================================
 * 第 0 部分：内部小工具
 * ===================================================================
 */

/*
 * sk_make_node —— 租一个节点
 *
 * ★★ 这里是本节第一个"这行为什么存在"：为什么是两次 malloc？★★
 *   ① malloc(sizeof(SkNode))            —— 节点本体：一个 int + 一个指针
 *   ② malloc(sizeof(SkNode*) * (level+1)) —— 那个"指针的数组"本体
 *
 * 为什么不一次 malloc 搞定（比如 struct { int key; SkNode *forward[16]; }）？
 *   那样每个节点都要付 16 个格子的钱，而**绝大多数节点只有 1 层**。
 *   实测：1000 个节点的跳表里，只有约 500 个有第 1 层、250 个有第 2 层……
 *   按上限定长 = 白花 8 倍内存。层数是随机的，所以数组长度也必须随机 → 单独 malloc。
 *   （C99 的柔性数组成员可以一次 malloc 搞定，属于工程优化，见 README「常见的坑」第 5 条。）
 *
 * 反过来，两次 malloc 就必须两次 free —— sk_destroy 里一一对应，少一次就是泄漏。
 */
static SkNode *sk_make_node(int level, int key)
{
    if (level < 0 || level >= SK_MAX_LEVEL) {
        return NULL;                      /* 层号越界直接拒，别把数组长度写成负数 */
    }

    SkNode *n = malloc(sizeof(SkNode));
    if (n == NULL) {
        return NULL;
    }

    /* 数组里有 (level + 1) 个格子：第 0 层到第 level 层 */
    n->forward = malloc(sizeof(SkNode *) * (size_t)(level + 1));
    if (n->forward == NULL) {
        free(n);                          /* 第二笔没租到，第一笔必须退掉，否则节点本体泄漏 */
        return NULL;
    }

    n->key = key;
    for (int l = 0; l <= level; l++) {
        n->forward[l] = NULL;
        /* 为什么要全部置空：万一某条路径忘了 splice 进链表，forward[l] 里就是 NULL，
         * 误用会当场崩，比"读到随机垃圾地址然后一路走飞"好查得多。
         * ⚠ 但别把它当"数组的结束标记"用：数组长度【恰好】是 level+1，一格 NULL 都不多，
         *   所以"从 0 数到 NULL"这种写法必然越界读 —— 见 sk_total_forward_slots 的注释。*/
    }
    return n;
}

/*
 * sk_label —— 把一个节点翻译成"人能看懂的名字"
 * 打印路径时，光有数字不够：必须能区分"这是头哨兵""这是尾哨兵""这是真节点"。
 */
static void sk_label(const SkipList *s, const SkNode *n, char *buf, size_t buf_sz)
{
    if (n == NULL) {
        snprintf(buf, buf_sz, "NULL");
    } else if (n == s->head) {
        snprintf(buf, buf_sz, "HEAD");
    } else if (n == s->nil) {
        snprintf(buf, buf_sz, "NIL");
    } else {
        snprintf(buf, buf_sz, "%d", n->key);
    }
}

/*
 * ===================================================================
 * 第 1 部分：生命周期
 * ===================================================================
 */

/*
 * sk_random_level —— 抛硬币决定层数
 *
 * 规矩：从第 0 层开始，每次 50% 概率"再升一层"，直到抛输或者顶到上限。
 *
 *   硬币结果              层号    出现概率     n=2000 时的期望节点数
 *   正                    0        1/2          1000
 *   反正                  1        1/4           500
 *   反反正                2        1/8           250
 *   反反反正              3        1/16          125
 *   …                    …        …             …
 *   一句话：第 l 层只有大约 n / 2^l 个节点，所以层数自然收敛到 log2(n)。
 *   这也解释了"空间期望 2n"：每个节点平均付 1 + 1/2 + 1/4 + … = 2 个指针格子。
 *
 * ⚠ 为什么这里用 rand() 而不是真随机？为什么又必须 srand(固定种子)？
 *   · 教材：README 里要贴真实运行输出、要画 dump 图。如果每次运行层数都不一样，
 *     图和输出下一秒就作废了。所以 sk_init 里写死一个种子，让整节可复现。
 *   · 真实系统：反而**不该**写死种子。两个理由：
 *       ① 层数是"数据的形状"，写死种子 + 固定插入顺序 = 每次跑同一条路径，
 *          线上若被攻击者猜到种子和插入序，可以专门构造出"所有节点都只有一层"
 *          的最坏输入，把跳表打成普通链表（O(n)）。
 *       ② 多机/多线程各自 srand(时间戳) 互不影响，不需要跨进程一致。
 *   跳表的 O(log n) 是**期望**复杂度而不是最坏保证，这个"随机"就是它的发动机。
 */
int sk_random_level(void)
{
    int lvl = 0;

    /* (rand() & 1) == 1：把 rand() 的最低位当硬币。
     * glibc 的 rand() 低位质量不错；换成弱 LCG 的平台这里会偏，见 README「常见的坑」第 6 条。 */
    while ((rand() & 1) == 1) {
        lvl++;
        if (lvl >= SK_MAX_LEVEL - 1) {
            break;                        /* 上限兜底：抛硬币可以一直赢，但层数不能上天 */
        }
    }
    return lvl;
}

/*
 * sk_init —— 建一张空跳表
 *
 * 空表长这样：两个哨兵面对面，所有层都是"空的"。
 *
 *   第 3 层  HEAD ───────────────────────────────► NIL
 *   第 2 层  HEAD ───────────────────────────────► NIL        层数=1，
 *   第 1 层  HEAD ───────────────────────────────► NIL        所以对外只看第 0 层
 *   第 0 层  HEAD ───────────────────────────────► NIL        （sk_dump 会把空层都打出来）
 *
 * 为什么两个哨兵的数组长度都是 SK_MAX_LEVEL？
 *   因为插入使 level 变大时，要能立刻在高层把 head 接出去；如果 head 只有 1 层，
 *   每次升层都得给 head 换一块更大的数组 —— 那才是真麻烦。给头哨兵一次配满最省心。
 *   代价只有两个节点，可以忽略。
 */
void sk_init(SkipList *s, unsigned int seed)
{
    if (s == NULL) {
        return;
    }

    /* ⚠ 这一行是"README 里的输出能对上"的全部原因：固定种子 → 固定层数 → 固定形状。
     *   正式代码里请把 seed 换成时间或随机设备，理由见上面 sk_random_level 的注释。*/
    srand(seed);

    s->level          = 1;                /* 至少第 0 层永远存在（它就是那条原始链表）*/
    s->size           = 0;
    s->cmp_count      = 0;
    s->pointer_slots  = 0;                /* 哨兵的格子不计入：那是固定设施，不随 n 增长 */
    s->trace          = false;            /* 默认不啰嗦，想看路径在 main.c 里打开 */

    s->head = sk_make_node(SK_MAX_LEVEL - 1, INT_MIN);
    s->nil  = sk_make_node(SK_MAX_LEVEL - 1, INT_MAX);

    if (s->head == NULL || s->nil == NULL) {
        /* 内存不够：把已租到的退掉，两个指针留 NULL。
         * 后面所有函数第一件事都是判 head == NULL，宁可什么都不做也不崩。 */
        if (s->head != NULL) { free(s->head->forward); free(s->head); }
        if (s->nil  != NULL) { free(s->nil->forward);  free(s->nil);  }
        s->head = NULL;
        s->nil  = NULL;
        /* 注意：这里走 stderr，而且正常运行永远到不了这一行（main.c 的 stderr 始终是空的）。
         * 正式项目里"申请内存失败"属于必须让运维看见的事件，不该混在业务 stdout 里。 */
        fprintf(stderr, "sk_init: 内存不足，跳表没能建起来\n");
        return;
    }

    for (int l = 0; l < SK_MAX_LEVEL; l++) {
        s->head->forward[l] = s->nil;     /* 每一层：头直接连到尾，中间没人 */
        s->nil->forward[l]  = s->nil;     /* 尾哨兵自环：哪怕有人误走 NIL，也走不出去、不会越界 */
    }
}

/*
 * sk_destroy —— 全部还回去
 *
 * ⚠ 顺序死规矩（和单链表 free 同一条铁律）：先把 forward[0] 抄到 nxt 里，
 *   再 free 这个节点。反过来写就是 free 之后读已释放内存 —— ASan 立刻举报。
 * ⚠ 数量规矩：每个真节点两次 free（forward 数组 + 节点本体），两个哨兵也各两次。
 *   少 free 任何一个 forward 数组，ASan 都会报 "xx byte(s) leaked"。
 */
void sk_destroy(SkipList *s)
{
    if (s == NULL || s->head == NULL) {
        return;                           /* 没建起来 / 已经收过摊，重复调用安全（不 double free）*/
    }

    SkNode *cur = s->head->forward[0];    /* 第 0 层就是那条完整的有序链表，沿它走一遍不漏任何人 */
    while (cur != NULL && cur != s->nil) {
        SkNode *nxt = cur->forward[0];    /* ① 先抄住下一个 */
        free(cur->forward);               /* ② 还它的指针数组（第二次 malloc）*/
        free(cur);                        /* ③ 再还节点本体（第一次 malloc）*/
        cur = nxt;                        /* ④ 走到抄下来的地址 */
    }

    free(s->head->forward);
    free(s->head);
    s->head = NULL;                       /* 置空：误用会当场崩，比"悄悄读到垃圾"好查一万倍 */

    free(s->nil->forward);
    free(s->nil);
    s->nil  = NULL;

    s->size           = 0;
    s->level          = 1;
    s->cmp_count      = 0;
    s->pointer_slots  = 0;
}

/*
 * ===================================================================
 * 第 2 部分：insert —— 本节的心脏
 * ===================================================================
 * ★ 核心问题：为什么"查找路径"顺手就把"插入位置"给出了？
 * ★
 * 插入需要知道的东西是：**每一层，新节点该插在谁后面**（记在 update[l] 里）。
 * 而查找在做的事是：从最高层出发，每层一路往右走到"下一个就 ≥ key"为止。
 * 停下来的那个位置，用大白话讲就是——
 *
 *     "这一层里，最后一个还比 key 小的节点"
 *
 * 而"新节点应该插在哪两个节点中间"的答案，正好就是
 * "最后一个比它小的"和"第一个比它大的"。也就是说：
 *   update[l]        = 第 l 层里 key 的前驱（我们停住的地方）
 *   update[l]->forward[l] = 第 l 层里 key 的后继（我们撞墙看到的下一个）
 * 查找走一趟，把这两件事**同时**告诉你了。所以插入 = 一趟查找 + 每层改两根线。
 *
 * 图解（往下面这张表里插 17，新节点抽到 2 层）：
 *
 *   查找下降（同时填 update[]）
 *     第 2 层: HEAD ──► 13 ── 下一个是 21 ≥ 17，停     update[2] = 13
 *     第 1 层: 13   ── 下一个是 19 ≥ 17，停            update[1] = 13
 *     第 0 层: 13   ──► 15 ── 下一个是 19 ≥ 17，停      update[0] = 15
 *
 *   改线（每层两步，顺序绝不能反 —— 单链表"先接右边再接左边"的老规矩）
 *     l=0: n->forward[0] = update[0]->forward[0] (=19);  update[0]->forward[0] = n;
 *     l=1: n->forward[1] = update[1]->forward[1] (=19);  update[1]->forward[1] = n;
 *     l=2: n->forward[2] = update[2]->forward[2] (=21);  update[2]->forward[2] = n;
 *
 *   插完：
 *     第 2 层  HEAD ──► 13 ──────────────────► [17] ──► 21 ──► NIL
 *     第 1 层  HEAD ──► ... ──► 13 ─────────► [17] ──► 19 ──► NIL
 *     第 0 层  HEAD ─► 13 ─► 15 ────────────► [17] ──► 19 ──► NIL
 */
static bool sk_insert_core(SkipList *s, int key, int level, bool allow_dup)
{
    if (s == NULL || s->head == NULL) {
        return false;
    }
    if (level < 0 || level >= SK_MAX_LEVEL) {
        return false;                     /* 层号非法：不碰链表，一根线都不动 */
    }

    /*
     * update[l] = 第 l 层的前驱。长度按上限开在**栈上**（16 个指针 = 128 字节），
     * 不 malloc，所以也不用 free —— 这是跳表实现里最常见的一个小舒服。
     */
    SkNode *update[SK_MAX_LEVEL];
    SkNode *x = s->head;

    if (s->trace) {
        printf("      ▶ 插入 key=%d（新节点最高层号 = %d，所以占 %d 格 forward）。先做一次下降，顺便把每层前驱记下来：\n",
               key, level, level + 1);
    }

    /* ---- 阶段一：逐层下降，填 update[] ---- */
    for (int l = s->level - 1; l >= 0; l--) {
        char buf[16];
        char entry[16];
        char stop[16];
        bool moved = false;

        sk_label(s, x, entry, sizeof entry);
        if (s->trace) {
            printf("        第 %2d 层: 从 %s 出发", l, entry);
        }

        for (;;) {
            s->cmp_count++;               /* 每"看一眼某节点的 key"记一次，这是复杂度的计量单位 */
            if (x->forward[l]->key >= key) {
                break;                    /* 撞墙：下一个已经不小于 key，本层不能往前了 */
            }
            x = x->forward[l];
            moved = true;
            if (s->trace) {
                printf(" ──► %d", x->key);
            }
        }
        update[l] = x;                    /* ★ 停住的位置 = 本层最后一个 < key 的节点 = 前驱 */

        if (s->trace) {
            sk_label(s, x->forward[l], buf, sizeof buf);
            sk_label(s, x, stop, sizeof stop);
            printf("   ── 下一个是 %s（不小于 %d），本层停 → update[%d] = %s",
                   buf, key, l, stop);
            if (!moved) {
                printf(" ｜ 本层一步没跨出去");
            }
            printf("%s\n", (l == 0) ? "（已经是最底层，不用再沉）" : "，下沉 ↓");
        }
    }

    /* ---- 阶段二：判重（allow_dup = false 时才判）---- */
    SkNode *first_ge = x->forward[0];     /* 第 0 层"下一个"，就是表里第一个 >= key 的节点 */
    s->cmp_count++;
    if (!allow_dup && first_ge != s->nil && first_ge->key == key) {
        if (s->trace) {
            printf("      ⚠ key=%d 已经存在（有序跳表里同一个值只留一份）→ 本次插入放弃\n", key);
        }
        return false;
    }

    /* ---- 阶段三：先造节点，再造结构变化 —— 顺序反了就会留下"层数升了但没人住"的半吊子状态 ---- */
    SkNode *n = sk_make_node(level, key);
    if (n == NULL) {
        if (s->trace) {
            printf("      [失败] malloc 不出节点，链表一根线都没动（层数也保持原样）\n");
        }
        return false;
    }

    /* ---- 阶段四：如果新节点比现有所有层都高，把层数抬上去 ---- */
    if (level > s->level - 1) {
        if (s->trace) {
            printf("      ★ 抽到 %d 层，比当前最高层 %d 还高 → 层数 %d → %d\n"
                   "        多出来的那些层里，前驱只能是 HEAD（表还没有比 %d 小的节点站在上面）\n",
                   level, s->level - 1, s->level, level + 1, key);
        }
        for (int l = s->level; l <= level; l++) {
            update[l] = s->head;          /* ⚠ 必须补这一句。忘补 = update[l] 是栈上的垃圾值 → 写飞 */
        }
        s->level = level + 1;             /* 对外层数 = 最高层号 + 1 */
    }

    /* ---- 阶段五：逐层改线 ---- */
    for (int l = 0; l <= level; l++) {
        n->forward[l] = update[l]->forward[l];   /* ① 新节点先牵住右边的后继 */
        update[l]->forward[l] = n;               /* ② 前驱再改口接新节点 */
    }
    s->size++;
    s->pointer_slots += (long)(level + 1);    /* 记账：这个节点占了几格 forward，等下 remove 要按同一口径减回去 */

    if (s->trace) {
        char a[16], b[16];
        for (int l = 0; l <= level; l++) {
            sk_label(s, update[l], a, sizeof a);
            sk_label(s, n->forward[l], b, sizeof b);
            printf("        改线 第 %2d 层: %s ──► [%d] ──► %s\n", l, a, key, b);
        }
    }
    return true;
}

/*
 * 正式入口：层数由抛硬币决定。
 * 这是唯一"正确"的插法 —— 层数一旦人为固定（见 sk_insert_pinned），跳表就不是跳表了。
 */
bool sk_insert(SkipList *s, int key)
{
    return sk_insert_core(s, key, sk_random_level(), false);
}

/*
 * 反例一专用：把层数钉死（永远传 0 就等于"不建索引"）。
 * 留在这个文件里而不是写在 main.c 里，是因为它和 sk_insert 只差一个参数，
 * 对照着读最能说明问题：**随机分层就是跳表的命根子**，抽不到层就不是跳表。
 */
bool sk_insert_pinned(SkipList *s, int key, int level)
{
    return sk_insert_core(s, key, level, false);
}

/*
 * 反例二专用：跳过判重，允许重复 key。
 * 真实 bug 场景：写 insert 时忘了"key 已存在就返回"，于是同一份数据被塞进去两次；
 * 之后 remove(key) 一次只摘掉一个，size 和"有几个不同的数"就对不上了。
 */
bool sk_insert_dup(SkipList *s, int key)
{
    return sk_insert_core(s, key, sk_random_level(), true);
}

/*
 * ===================================================================
 * 第 3 部分：search —— 让你亲眼看到"跳"
 * ===================================================================
 * 算法只有两句话：
 *   ① 在当前层，只要"下一个节点的 key 还 < 目标"就往前跳（一直跳到底）。
 *   ② 跳不动了（下一个 ≥ 目标，或者撞到 NIL）就下沉一层，重复 ①。
 *   ③ 沉到第 0 层再往前看一格：等于目标就命中，否则不存在。
 *
 * 为什么这样一定找得到（如果它在的话）？
 *   因为每一层都是"下一层的子序列"，而且各层都保持有序。
 *   在第 l 层停在 x（x->key < key < x->forward[l]->key）时，
 *   说明 key 如果存在，必然落在 x 和 x->forward[l] 之间那一段里 ——
 *   下沉一层以后，那一段被拆得更细，我们只要在 x 后面继续找就行，
 *   **永远不需要回头**。这就是"单调下降"能保证正确性的原因。
 *
 * 期望代价：每层平均走 1/p = 2 步（p = 1/2），层数 log2(n)，
 *   所以期望比较次数 ≈ 2·log2(n)。n=2000 时 ≈ 22 次，而不是 1000 次 ——
 *   main.c 第 7 幕会把这个数真的量出来。
 */
bool sk_search(SkipList *s, int key)
{
    if (s == NULL || s->head == NULL) {
        return false;
    }

    SkNode *x = s->head;

    if (s->trace) {
        printf("      ▶ 查找 key=%d：从第 %d 层（最高层）出发，能跳就跳，跳不动就下沉 ↓\n",
               key, s->level - 1);
    }

    for (int l = s->level - 1; l >= 0; l--) {
        char buf[16];
        char entry[16];
        int  steps = 0;

        /* ★ 这一行的起点是"上一层停住的那个节点"，不一定是 HEAD：
         *   只有最高层那次才真的从 HEAD 出发。打日志必须打真实身份，否则读者以为
         *   每层都从头再走一遍，那就完全看不懂"跳"了。 */
        sk_label(s, x, entry, sizeof entry);
        if (s->trace) {
            printf("        第 %2d 层: 从 %s 出发", l, entry);
        }

        for (;;) {
            s->cmp_count++;
            if (x->forward[l]->key >= key) {
                break;                    /* 本层到头：下一个已经不小于目标了 */
            }
            x = x->forward[l];
            steps++;
            if (s->trace) {
                printf(" ──► %d", x->key);
            }
        }

        if (s->trace) {
            sk_label(s, x->forward[l], buf, sizeof buf);
            if (l == 0) {
                printf("   ── 下一个是 %s，第 0 层不能再沉了", buf);
            } else if (steps > 0) {
                printf("   ── 下一个是 %s ≥ %d，本层停，下沉 ↓（本层跨了 %d 格）",
                       buf, key, steps);
            } else if (l == s->level - 1) {
                /* 最高层 0 跨很常见：HEAD 的下一个（= 全表最高的那几个节点之一）就已经不小于目标 */
                printf("   ── 下一个是 %s ≥ %d，本层停，下沉 ↓（最高层 0 跨：HEAD 后面第一个就太大了）",
                       buf, key);
            } else {
                /* 非最高层的 0 跨不是浪费：上一层停的位置已经把它夹住了，这一层只是再确认一次 */
                printf("   ── 下一个是 %s ≥ %d，本层停，下沉 ↓（本层 0 跨：上一层已经把我送到位置了）",
                       buf, key);
            }
            printf("\n");
        }
    }

    /* 第 0 层的"下一格"就是唯一可能的答案 */
    SkNode *cand = x->forward[0];
    s->cmp_count++;
    bool hit = (cand != s->nil && cand->key == key);

    if (s->trace) {
        char buf[16];
        sk_label(s, cand, buf, sizeof buf);
        printf("      %s 第 0 层往前看一眼 = %s → %s\n",
               (hit ? "✔" : "✘"), buf, hit ? "命中" : "不存在");
    }
    return hit;
}

/*
 * ===================================================================
 * 第 4 部分：remove —— 和 insert 完全对称
 * ===================================================================
 * 同样是"下降 + 记 update[]"，差别只在收尾：
 *   ① 先确认 cand 真的是那个 key（否则什么都没发生，返回 false）；
 *   ② 对每一层问一句："这一层的前驱的后继**是不是**它？"
 *      是 → 跨过它（update[l]->forward[l] = cand->forward[l]）
 *      不是 → 说明这个节点根本没有第 l 层，跳过。
 *   为什么要逐层"问"而不直接用节点自己的层数？
 *      因为我们没有给节点存层数字段（forward 数组是定长的才存得下），
 *      而"问前驱"这个动作既简单又绝对不会错。
 *   ③ free 两次（指针数组 + 本体），size--；
 *   ④ 如果最高层被删空了，把 s->level 降下来 —— 不降的话层数只增不减，
 *      查找每次都要在空层上白问一句，长期跑下来就慢了。
 */
bool sk_remove(SkipList *s, int key)
{
    if (s == NULL || s->head == NULL) {
        return false;
    }

    SkNode *update[SK_MAX_LEVEL];
    SkNode *x = s->head;

    if (s->trace) {
        printf("      ▶ 删除 key=%d：先按查找的路径走一遍，把每层前驱存进 update[]\n", key);
    }

    for (int l = s->level - 1; l >= 0; l--) {
        for (;;) {
            s->cmp_count++;
            if (x->forward[l]->key >= key) {
                break;
            }
            x = x->forward[l];
        }
        update[l] = x;
        if (s->trace) {
            char buf[16];
            char stop[16];
            sk_label(s, x->forward[l], buf, sizeof buf);
            sk_label(s, x, stop, sizeof stop);
            printf("        第 %2d 层: 停在 %s，下一个是 %s%s\n",
                   l, stop, buf, l == 0 ? "（最底层）" : "，下沉 ↓");
        }
    }

    SkNode *victim = x->forward[0];
    s->cmp_count++;
    if (victim == s->nil || victim->key != key) {
        if (s->trace) {
            printf("      ✘ 表里没有 %d，一根线都没动\n", key);
        }
        return false;
    }

    /* 逐层摘线：注意此时还没 free，victim->forward[l] 仍然可以安全读 */
    int unlinked = 0;
    for (int l = 0; l <= s->level - 1; l++) {
        if (update[l]->forward[l] == victim) {
            update[l]->forward[l] = victim->forward[l];
            unlinked++;
            if (s->trace) {
                char a[16], b[16];
                sk_label(s, update[l], a, sizeof a);
                sk_label(s, victim->forward[l], b, sizeof b);
                printf("        第 %2d 层有它：%s ──✂── [%d] ──► 改成 %s ──► %s\n",
                       l, a, key, a, b);
            }
        } else if (s->trace) {
            printf("        第 %2d 层没有它（这个节点压根没长到这一层）→ 跳过\n", l);
        }
    }
    if (s->trace) {
        printf("      ✔ 一共在 %d 层上把 [%d] 摘掉了\n", unlinked, key);
    }

    free(victim->forward);                /* 第二次 malloc 的那块 */
    free(victim);                         /* 第一次 malloc 的那块 */
    victim = NULL;                        /* 顺手置空，防误用 */
    s->size--;
    s->pointer_slots -= (long)unlinked;   /* 记账回收：unlinked 正好等于该节点的层数 */

    /* 收缩层数：从最高层往下，哪层空了就降 */
    int old_level = s->level;
    while (s->level > 1 && s->head->forward[s->level - 1] == s->nil) {
        s->level--;
    }
    if (s->trace && s->level != old_level) {
        printf("      ★ 最高层 %d 被删空 → 层数 %d → %d（不收缩的话，以后每次查找都要在空层白问一句）\n",
               old_level - 1, old_level, s->level);
    }

    return true;
}

/*
 * ===================================================================
 * 第 5 部分：dump —— 本节最重要的可视化
 * ===================================================================
 * 把多层结构一层一行摊开打印，从最高层到第 0 层，**空层也照打**。
 * 为什么空层也要打？
 *   ① 让"高层比低层稀疏"这件事看得见摸得着（这才是跳表的本体）；
 *   ② 插入时把 s->level 抬上去、删除时又没及时收回来，这类 bug 只在
 *      "打出一层空的最高层"时才暴露。
 */
void sk_dump(const SkipList *s)
{
    if (s == NULL || s->head == NULL) {
        printf("  （这张表还没建起来 / 已经销毁）\n");
        return;
    }

    printf("  跳表快照：size=%d，层数=%d（有效层号 0 ~ %d）\n",
           s->size, s->level, s->level - 1);

    for (int l = s->level - 1; l >= 0; l--) {
        printf("    第 %2d 层 | HEAD", l);

        int      cnt = 0;
        SkNode  *x   = s->head->forward[l];
        while (x != s->nil) {
            printf(" ──► %d", x->key);
            x = x->forward[l];
            cnt++;
        }

        if (cnt == 0) {
            printf(" ──► (空)");          /* 空层也要打印：见上面函数头注释 */
        }
        printf(" ──► NIL   （本层 %d 个节点）\n", cnt);
    }

    printf("    怎么读这张图：每一行是**一条完整的有序链表**（同一个 head 到同一个 nil），\n");
    printf("                  行与行之间不是不同的节点，而是同一批节点的【不同层指针】；\n");
    printf("                  第 0 行永远是全部 %d 个成员，越往上越稀疏，跨度越大。\n", s->size);
}

/*
 * ===================================================================
 * 第 6 部分：range —— 支撑"跳表天然适合范围查询"这句话
 * ===================================================================
 * 两步：① 用下降定位"第一个 >= lo 的节点"（O(log n)）
 *       ② 然后沿第 0 层一路 next，直到超过 hi（O(k)，k = 命中个数）
 * 对比平衡树：BST 做范围查询要中序遍历，回溯 + 递归栈；跳表就是"顺着链表走"，
 * 代码三行，而且缓存局部性也比树的递归访问好。
 */
int sk_range(const SkipList *s, int lo, int hi, int *out, int out_cap)
{
    if (s == NULL || s->head == NULL || out == NULL || out_cap <= 0) {
        return 0;
    }

    SkNode *x = s->head;
    for (int l = s->level - 1; l >= 0; l--) {
        while (x->forward[l]->key < lo) {
            x = x->forward[l];            /* 只要还全都小于 lo 就往右冲 */
        }
    }

    /* 下沉到第 0 层后，x 是最后一个 < lo 的节点 → x->forward[0] 就是候选起点 */
    int       n = 0;
    SkNode *cur = x->forward[0];
    while (cur != s->nil && cur->key <= hi) {
        if (n < out_cap) {
            out[n++] = cur->key;
        } else {
            break;                        /* 缓冲区满了就停；真要全部拿到就把 out_cap 开大 */
        }
        cur = cur->forward[0];
    }
    return n;
}

/*
 * ===================================================================
 * 第 7 部分：空间实测 —— "跳表空间是链表的期望 2 倍"不是嘴上说说的
 * ===================================================================
 * ⚠ 这里曾经踩过一个真实的坑，值得写进注释里（README「常见的坑」第 5 条就是它）：
 *   一开始的实现是"沿第 0 层走一遍，每个节点从 forward[0] 数到 NULL 为止"，
 *   理由是"造节点时都置了 NULL"。听着很顺，其实**错**：
 *   malloc 的格子数【恰好】是 level+1，后面根本没有多余的 NULL 做终止符，
 *   数到 NULL 就是往数组外面读 —— ASan 当场报 heap-buffer-overflow（本节的闸门就是这么逮到它的）。
 *
 * 正解：数组的长度信息只有"造它的人"知道，所以由 SkipList 记账：
 *   插入时 += (level + 1)；删除时 -= "这个节点在几层上被摘掉"（= 它自己的层数，天然相等）。
 * 一句话教训：**C 语言里，指针数组不随身带长度。长度必须另存。**
 *
 * 期望值 = 2（几何级数 1 + 1/2 + 1/4 + … → 2），所以上千节点的表里
 * 格子总数 ≈ 2n。main.c 第 8 幕把这个比值真的打出来。
 */
long sk_total_forward_slots(const SkipList *s)
{
    if (s == NULL || s->head == NULL) {
        return 0;
    }
    return s->pointer_slots;
}
