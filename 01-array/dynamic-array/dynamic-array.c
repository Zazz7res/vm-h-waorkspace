#include <stdio.h>
#include <stdint.h>    /* uintptr_t：把地址当纯整数存下来，才能安全地"事后比较新旧地址" */
#include <stdlib.h>    /* malloc / realloc / free 全家都在这里 */
#include <string.h>    /* memmove：插入删除挪格子 */
#include "dynamic-array.h"

/*
 * ===================================================================
 * 0. 内部工具：给"还没被元素占用"的格子涂可辨识的垃圾
 * ===================================================================
 * 工程代码不需要这一步（真正的 vector 要么不管，要么默认构造 0）。
 * 这里特意涂，是因为这一节的核心图景就是「元素区 / 空位区」的边界：
 *   空位里如果躺着上一轮的残留值，你不看账面就会误以为它还是元素。
 *   涂成 -(2000+i) 这种一看就不像正常数据的数，边界立刻肉眼可辨。
 * static：只在本文件可见，等于"私有函数"，不污染菜单。
 */
static void poison_spare_cells(int *cell, int count)
{
    for (int i = 0; i < count; i++) {
        cell[i] = -(2000 + i);
    }
}

/*
 * ===================================================================
 * 1. 扩容一步：一次 realloc，容量按公式变大
 * ===================================================================
 * 新容量 = 旧容量 * grow_multiplier + grow_increment
 *   默认 2 / 0  → 翻倍（1 → 2 → 4 → 8 → ...）
 *   对照 1 / 1  → 每次只多一格（1 → 2 → 3 → 4 → ...），用来把"坏策略"跑出真实数字
 * 旧容量为 0（懒初始化：init 时传了 0 或负数）时，直接给 DYNARRAY_MIN_CAPACITY 格。
 *
 * 【为什么必须 `p = realloc(p, n)` 用返回值覆盖自己？】
 *   realloc 有两种成功结局：
 *     A 原地扩大：后面正好有空地，地址不变 —— 一个字节都不用抄；
 *     B 整块搬家：换到新地址，老数据由 realloc 帮你抄过去，旧块当场被 free。
 *   所以旧指针在结局 B 之后就是悬空指针，只有返回值才是"现在合法的地址"。
 *   这里把旧地址先存成 uintptr_t（纯整数）再比较，是为了避开
 *   "拿已释放的指针去比较/打印"这个警告（我们只比数值，从不解引用它）。
 *
 * 【失败（结局 C）怎么办？】
 *   realloc 返回 NULL，而【原内存原封不动、依然有效】，容量还是老值。
 *   这时绝不能把 NULL 写回 a->data —— 那样老块就再也找不回来，直接泄漏。
 *   本函数的做法：一行账面都不改，返回 false，让调用者继续使用旧数据。
 */
static bool dynarray_grow_one_step(DynArray *a)
{
    uintptr_t old_addr = (uintptr_t)a->data;      /* 先记下旧地址的数值 */
    int old_capacity = a->capacity;
    int new_capacity = (old_capacity <= 0)
                     ? DYNARRAY_MIN_CAPACITY
                     : old_capacity * a->grow_multiplier + a->grow_increment;

    /* 防呆：万一策略字段被写成 0 * capacity + 0 这种退化组合，至少也要多出一格，
     * 否则 reserve 的循环会原地打转，程序卡死 —— 防御性编程就是要防"自己算的公式" */
    if (new_capacity <= old_capacity) {
        new_capacity = old_capacity + 1;
    }

    int *moved = realloc(a->data, (size_t)new_capacity * sizeof(int));
    if (moved == NULL) {
        /* 结局 C：申请失败。a->data 仍然有效，账面一个字都不改 —— 数据一个没丢 */
        printf("      [扩容失败] 想要 %d 格，realloc 返回 NULL；"
               "原内存 0x%lx（%d 格）依然可用，容量保持不变，没有泄漏\n",
               new_capacity, (unsigned long)old_addr, old_capacity);
        return false;
    }

    bool relocated = ((uintptr_t)moved != old_addr);
    a->data = moved;                 /* 从此以后只有这个是合法指针，旧地址作废 */
    a->capacity = new_capacity;
    a->grow_count++;
    /* 最坏情况账本：当成"每次都搬家"来记，这是摊还分析用的上界 */
    a->copy_total += (unsigned long long)old_capacity;
    if (relocated) {
        a->grow_moved_count++;
        a->copy_moved_total += (unsigned long long)old_capacity;   /* 实测账本：真搬家才记 */
    }

    /*
     * 新扩出来的格子是未初始化内存，读它是未定义行为 —— 这里立刻涂成垃圾值，
     * 之后打印地图时看到的就是"确定的垃圾"，不是"随机的运气"。
     */
    poison_spare_cells(&a->data[old_capacity], new_capacity - old_capacity);

    if (a->log_enabled) {
        /* log_enabled = 0 时这句跳过：做"+1 扩容"那种对照实验时，
         * 一次 push 就打一行会刷屏，我们只要最后的统计数字 */
        printf("      扩容第 %d 次：%d 格 → %d 格，realloc 旧地址 0x%lx → 新地址 0x%lx  %s"
               "（最坏抄 %d 个 / 实测累计抄 %llu 个）\n",
               a->grow_count, old_capacity, new_capacity,
               (unsigned long)old_addr, (unsigned long)moved,
               relocated ? "【整块搬家】" : "【原地扩大，地址没变】",
               old_capacity, a->copy_moved_total);
    }
    return true;
}
/*
 * ===================================================================
 * 2. reserve：保证"还能再放 extra 个元素"，不够就一直扩到够
 * ===================================================================
 * 关键的第一行：容量够就【什么都不做】—— 这就是"空位"存在的全部意义：
 * 让绝大多数 push_back 只是一次赋值，不碰 realloc、不抄数据。
 * while 循环的意义：+1 策略一次扩一档，要扩很多次才够；
 * 翻倍策略通常一次就够。循环次数本身就是两种策略的差距，main 里会跑给你看。
 */
static bool dynarray_reserve(DynArray *a, int extra)
{
    while (a->capacity < a->size + extra) {
        if (!dynarray_grow_one_step(a)) {
            return false;      /* 扩不动了：保持原状，调用者会返回 false */
        }
    }
    return true;
}

/*
 * ===================================================================
 * 3. 缩容判断 + 执行
 * ===================================================================
 * 判定式（整数乘法，避免除法取整的边界歧义；「压线」也算触线）：
 *      size * shrink_denominator <= capacity * shrink_numerator
 * 默认 1/4：元素数掉到容量的四分之一（含正好四分之一），才把容量砍一半。
 *
 * 为什么用 <= 而不是 <? 因为 "<" 会把阈值线推成一个不整数：
 *   用 < 时 size=capacity/4 这一格不砍，size=capacity/4-1 才砍，砍完的新容量
 *   和 size 之间多出一格的缝隙，讲"缓冲带"时就得反复解释这一格。
 *   用 <= 时「等于 1/4 就砍」一句话讲完，而且第 6 幕的抖动对照实验要的就是压线触发。
 *
 * 【为什么不是 1/2 就砍？—— 抖动（thrashing）】
 *   阈值定在 1/2 时：size 掉到 capacity 的一半（压线）→ 砍成 capacity/2 →
 *   现在 size == 新 capacity，正好满 → 再 push 一个又要扩容（翻倍回原来的 capacity）→
 *   扩容完再 pop 一个，size 又掉回新 capacity 的一半 → 又触发缩容……
 *   「pop 一下 push 一下」就会【每一步都搬家一次】，每次 O(n)。
 *   定在 1/4 就没这个问题：缩容后 size 最多是新容量的 1/2，
 *   要连续 push 回一倍的量才碰得到扩容线；中间这 n/2 步全是 O(1) 的空位赋值。
 *   一句话：扩容和缩容的线【必须隔开一段距离】，不然就会来回摩擦。
 *   main 第 6 幕用两种阈值各跑 20 轮 pop/push，把 realloc 次数打出来对比。
 */
static bool should_shrink(const DynArray *a)
{
    if (a->capacity <= a->base_capacity) {
        return false;   /* 已经是最小租约，再砍就等于全还回去，空表也要留几格 */
    }
    long long left  = (long long)a->size * a->shrink_denominator;
    long long right = (long long)a->capacity * a->shrink_numerator;
    return left <= right;
}

static void dynarray_maybe_shrink(DynArray *a)
{
    if (!should_shrink(a)) {
        return;
    }
    int old_capacity = a->capacity;
    int new_capacity = old_capacity / 2;

    /* 三道地板，一道都不能少：
     *   1) 不低于初始容量（否则空表来回 malloc/free）
     *   2) 必须装得下现有元素（砍完放不下就是把数据挤没了，绝对错误） */
    if (new_capacity < a->base_capacity) {
        new_capacity = a->base_capacity;
    }
    if (new_capacity < a->size) {
        new_capacity = a->size;
    }
    if (new_capacity >= old_capacity) {
        return;   /* 砍无可砍（比如 base 已经等于当前容量） */
    }

    uintptr_t old_addr = (uintptr_t)a->data;
    int *moved = realloc(a->data, (size_t)new_capacity * sizeof(int));
    if (moved == NULL) {
        /* 缩小一般不会失败；真失败了也什么都不改，多占点内存而已，数据毫发无损 */
        printf("      [缩容跳过] realloc 到 %d 格失败，继续用 %d 格，数据没丢\n",
               new_capacity, old_capacity);
        return;
    }
    a->data = moved;
    a->capacity = new_capacity;
    a->shrink_count++;
    a->copy_total += (unsigned long long)new_capacity;   /* 搬家最坏要抄这么多 */
    if ((uintptr_t)moved != old_addr) {
        a->shrink_moved_count++;                    /* 缩容的搬家单独记账，别混进扩容那本 */
        a->copy_moved_total += (unsigned long long)new_capacity;
    }
    if (a->log_enabled) {
        printf("      缩容第 %d 次：size=%d 掉到 capacity=%d 的阈值线上（含压线）→ 容量砍到 %d，"
               "realloc 0x%lx → 0x%lx，归还 %d 格 = %zu 字节\n",
               a->shrink_count, a->size, old_capacity, new_capacity,
               (unsigned long)old_addr, (unsigned long)moved,
               old_capacity - new_capacity,
               (size_t)(old_capacity - new_capacity) * sizeof(int));
    }

    /* 容量变小后，尾部可用格子也变了：这里不需要涂垃圾，因为新容量里没有空位了
     * （new_capacity >= size），elements 之外的格子根本不存在了 */
}

/*
 * ===================================================================
 * 4. init / destroy
 * ===================================================================
 * init 传 0 或负数 = "懒启动"：先不占堆内存（data=NULL, capacity=0），
 * 第一次 push_back 时再现场租 MIN_CAPACITY 格。空 vector 就该是零成本。
 *
 * 结构体本身在哪由调用者决定（main 里就是栈上的局部变量）；
 * 只有 data 指向的那排格子在堆上 —— 这就是"半静态半动态"：
 *   记账的小盒子在栈上，装东西的大仓库在堆上。
 */
bool dynarray_init(DynArray *a, int init_capacity)
{
    if (a == NULL) {
        return false;
    }
    a->data = NULL;
    a->size = 0;
    a->capacity = 0;
    a->base_capacity = (init_capacity > 0) ? init_capacity : 0;  /* 下面还会兜底抬到 MIN */
    a->grow_multiplier = 2;                   /* 默认翻倍 */
    a->grow_increment = 0;
    a->shrink_numerator = 1;                  /* 默认 1/4 才缩 */
    a->shrink_denominator = DYNARRAY_DEFAULT_SHRINK_DENOMINATOR;
    a->grow_count = 0;
    a->grow_moved_count = 0;
    a->shrink_count = 0;
    a->shrink_moved_count = 0;
    a->log_enabled = 1;                   /* 默认把扩缩容现场打出来，看得见才信得过 */
    a->copy_total = 0;
    a->copy_moved_total = 0;

    if (a->base_capacity < DYNARRAY_MIN_CAPACITY) {
        /* 无论传了什么，缩容的下限至少留 MIN 格：省得空表反复 malloc 1 格 */
        a->base_capacity = DYNARRAY_MIN_CAPACITY;
    }

    if (init_capacity > 0) {
        int want = (init_capacity < DYNARRAY_MIN_CAPACITY) ? DYNARRAY_MIN_CAPACITY : init_capacity;
        a->data = malloc((size_t)want * sizeof(int));
        if (a->data == NULL) {
            printf("      [错误] malloc %d 格失败，结构体保持空表状态（安全，可以照常 destroy）\n", want);
            a->capacity = 0;
            return false;
        }
        a->capacity = want;
        poison_spare_cells(a->data, want);
        printf("      dynarray_init(%d)：malloc %d 格 = %zu 字节，入口地址 %p；size=0\n",
               init_capacity, want, (size_t)want * sizeof(int), (void *)a->data);
    } else {
        printf("      dynarray_init(%d)：懒启动，一块内存都不占（data=NULL, capacity=0），"
               "第一次 push_back 时再租 %d 格\n", init_capacity, DYNARRAY_MIN_CAPACITY);
    }
    return true;
}

/*
 * ===================================================================
 * 4b. destroy —— free 之后【账面必须归零】
 * ===================================================================
 * 三件事一件都不能少，顺序也不能反：
 *   1) free(a->data) —— 归还堆内存；
 *   2) a->data = NULL —— 撕掉写旧地址的纸条。不置 NULL 的话，结构体里留着
 *      一个已经作废的地址 + size/capacity 两个"看起来很正常"的数字，
 *      下次误用就是"合法的调用 + 悬空指针"，ASan 报 heap-use-after-free，
 *      更可怕的是在没有 ASan 的生产环境里它会先跑对三天再诡异崩掉；
 *   3) size = capacity = 0 —— 从此任何操作都会先撞上"空表"的判断，当场暴露，
 *      而不是悄悄往别人的内存里写。
 * 置 NULL 还顺带让 destroy 变成幂等：再 destroy 一次就是 free(NULL)，合法空操作。
 */
void dynarray_destroy(DynArray *a)
{
    if (a == NULL) {
        return;
    }
    printf("      dynarray_destroy：准备 free(%p)（归还 %d 格 = %zu 字节，当前 size=%d）\n",
           (void *)a->data, a->capacity, (size_t)a->capacity * sizeof(int), a->size);
    free(a->data);
    a->data = NULL;
    a->size = 0;
    a->capacity = 0;
    printf("      free 之后立刻置空：data=NULL，size=0，capacity=0（账面一起归零）。\n");
    printf("      不置空会怎样：旧的 data 地址还留在结构体里，数字看着一切正常 ——\n");
    printf("      那就是悬空指针，下次读到它是 ASan 里的 heap-use-after-free，\n");
    printf("      没有 ASan 时就是「今天跑对、三天后诡异崩溃」。\n");
    printf("      置空之后再【读】它：is_empty=true、size=0、at 一律返回 false —— 安全失败；\n");
    printf("      要是绕过接口直接用 dynarray_get 拿 data[0]：data 是 NULL → 当场段错误。\n");
    printf("      立刻爆炸 >> 悄悄算错，这是 C 里处理「用过的指针」的唯一正道。\n");
    printf("      ⚠ 一个诚实的细节：destroy 之后再 push_back，因为 realloc(NULL, n) 等价于 malloc(n)，\n");
    printf("         这张表会「复活」成一张新表 —— 语法上合法，但几乎一定是逻辑错误。\n");
    printf("         所以 destroy 之后只有两条路：重新 init，或者让它的变量作用域结束。\n");
    printf("      累计账本：扩容 %d 次（真搬家 %d 次）、缩容 %d 次（真搬家 %d 次）、"
           "最坏抄 %llu 个元素、实测抄 %llu 个元素\n",
           a->grow_count, a->grow_moved_count, a->shrink_count, a->shrink_moved_count,
           a->copy_total, a->copy_moved_total);
}

/* ---------- 5. 只读操作 ---------- */

int dynarray_size(const DynArray *a)
{
    return (a != NULL) ? a->size : 0;
}

int dynarray_capacity(const DynArray *a)
{
    return (a != NULL) ? a->capacity : 0;
}

bool dynarray_is_empty(const DynArray *a)
{
    return (a == NULL) ? true : (a->size == 0);
}

/*
 * at：带检查的版本。
 * 为什么先判 data 再判 index？因为 capacity==0 的懒启动空表 data 是 NULL，
 * 哪怕 index 是 0 也绝不能去解引用；两个条件任何一个不满足就返回 false。
 */
bool dynarray_at(const DynArray *a, int index, int *value_out)
{
    if (a == NULL || value_out == NULL || a->data == NULL) {
        return false;
    }
    if (index < 0 || index >= a->size) {
        printf("      [at 拒绝] index=%d 不在 [0, size=%d) 内，返回 false，out 保持原值\n",
               index, a->size);
        return false;
    }
    *value_out = a->data[index];
    return true;
}

/*
 * get：不设防的版本 —— 就是把原生 a->data[index] 包了一层皮。
 * 它【快】，代价是越界直接未定义行为（ASan 会报 heap-buffer-overflow）。
 * 存在意义见头文件：让你亲手摸到"检查"和"不检查"这两档的界线在哪。
 */
int dynarray_get(const DynArray *a, int index)
{
    /* 这里刻意一个 if 都不写：内层循环里下标由程序自己保证时，省下每次比较 */
    return a->data[index];
}

int dynarray_find(const DynArray *a, int value)
{
    if (a == NULL) {
        return -1;
    }
    for (int i = 0; i < a->size; i++) {
        if (a->data[i] == value) {
            return i;
        }
    }
    return -1;
}

/* ---------- 6. 尾部增删：动态数组最划算的两个操作 ---------- */

/*
 * push_back 的两条路径：
 *   路径 A（绝大多数时候）：capacity 还有空位 → 写一个值 + size++，O(1)，一个字节都不搬。
 *   路径 B（偶尔）：空位用完了 → 先扩容（可能 realloc 搬家，O(n)），再走路径 A。
 * 把 B 的成本摊到 A 上就是"摊还 O(1)"：翻倍策略下 n 次 push 的搬运总量 < 2n。
 */
bool dynarray_push_back(DynArray *a, int value)
{
    if (a == NULL) {
        return false;
    }
    if (!dynarray_reserve(a, 1)) {
        printf("      [push_back 失败] 扩不出内存，元素没收，表保持原样（size=%d）\n", a->size);
        return false;
    }
    a->data[a->size] = value;   /* 写在"下一个空位"，这个位置之前一定没人用 */
    a->size++;                  /* 然后才承认它是元素 —— 顺序反了会读到一个没赋值的格子 */
    return true;
}

/*
 * pop_back：尾部删除唯一不用挪格子的删除，O(1)。
 * 注意：pop 之后那格里的值【还在】，只是不再被算作元素（size 变小）。
 * 这就是为什么"元素个数"必须单独记账，不能靠格子内容猜。
 * 顺手做一次缩容检查 —— 缩容只在删除类操作后考虑，插入后再缩毫无意义。
 */
bool dynarray_pop_back(DynArray *a, int *value_out)
{
    if (a == NULL || a->size == 0) {
        printf("      [pop_back 失败] 空表（size=0），没东西可弹\n");
        return false;
    }
    int gone = a->data[a->size - 1];
    a->size--;                                  /* 先把元素数降下来，格子内容原地不动 */
    a->data[a->size] = -(2099);                 /* 涂个残留标记，让"它已不是元素"肉眼可见 */
    if (value_out != NULL) {
        *value_out = gone;                      /* 输出参数：值从指针带出去 */
    }
    dynarray_maybe_shrink(a);
    return true;
}

/* ---------- 7. 中间插入 / 删除：动态数组也救不了的 O(n) ---------- */

/*
 * insert_at 的完整代价 = 可能的扩容（O(n)） + 后半排右移（O(n)） = O(n)。
 * 想清楚这一点很重要：动态数组解决的是"装不下"，不是"挪格子"。
 * 需要频繁在中间/头部增删，就该用链表（02-linked-list/），
 * 或者干脆改用"逻辑顺序"（堆 06-heap/、队列 04-queue/）。
 */
bool dynarray_insert_at(DynArray *a, int index, int value)
{
    if (a == NULL) {
        return false;
    }
    if (index < 0 || index > a->size) {
        printf("      [insert_at 失败] index=%d 不在 [0, size=%d] 内\n", index, a->size);
        return false;
    }
    if (!dynarray_reserve(a, 1)) {
        printf("      [insert_at 失败] 扩不出内存，表保持原样\n");
        return false;
    }
    int tail_cells = a->size - index;
    /* 又是 memmove：源 [index, size) 与目标 [index+1, size+1) 必然重叠，memcpy 会抄坏 */
    memmove(&a->data[index + 1], &a->data[index], (size_t)tail_cells * sizeof(int));
    a->data[index] = value;
    a->size++;
    return true;
}

bool dynarray_remove_at(DynArray *a, int index, int *value_out)
{
    if (a == NULL || a->size == 0) {
        printf("      [remove_at 失败] 结构体为空或表已空\n");
        return false;
    }
    if (index < 0 || index >= a->size) {
        printf("      [remove_at 失败] index=%d 不在 [0, size=%d) 内\n", index, a->size);
        return false;
    }
    int gone = a->data[index];
    int tail_cells = a->size - index - 1;
    memmove(&a->data[index], &a->data[index + 1], (size_t)tail_cells * sizeof(int));
    a->size--;
    a->data[a->size] = -(2099);
    if (value_out != NULL) {
        *value_out = gone;
    }
    dynarray_maybe_shrink(a);
    return true;
}

/* ---------- 8. 教学专用：改策略 ---------- */

void dynarray_set_growth_policy(DynArray *a, int multiplier, int increment)
{
    if (a == NULL) {
        return;
    }
    a->grow_multiplier = multiplier;
    a->grow_increment = increment;
    printf("      [策略] 扩容公式改成：新容量 = 旧容量 * %d + %d\n", multiplier, increment);
}

void dynarray_set_shrink_policy(DynArray *a, int numerator, int denominator)
{
    if (a == NULL || denominator <= 0) {
        return;
    }
    a->shrink_numerator = numerator;
    a->shrink_denominator = denominator;
    printf("      [策略] 缩容阈值改成：size * %d <= capacity * %d 时才砍（即 size <= capacity 的 1/%d，压线就砍）\n",
           denominator, numerator, denominator);
}

/* ---------- 9. 打印 ---------- */

/*
 * 值 + 全套账面。注意末尾同时报 size 和 capacity 两个数字：
 * 这一行就是这个结构的"体检报告"，容量利用率（size/capacity）看着它就能口算。
 */
void dynarray_print(const DynArray *a, const char *tag)
{
    if (a == NULL) {
        printf("%s: (NULL)\n", tag);
        return;
    }
    printf("%s: [", tag);
    for (int i = 0; i < a->size; i++) {
        printf("%s%d", (i > 0) ? "|" : "", a->data[i]);
    }
    if (a->size == 0) {
        printf("(空表)");
    }
    printf("]  size=%d / capacity=%d  data=%p  扩容%d次(搬家%d) 缩容%d次(搬家%d) 抄动:最坏%llu/实测%llu\n",
           a->size, a->capacity, (void *)a->data,
           a->grow_count, a->grow_moved_count, a->shrink_count, a->shrink_moved_count,
           a->copy_total, a->copy_moved_total);
}

/*
 * 内存地图：下标 / 地址 / 数值 / 身份（元素 or 空位）。
 * 只画前 max_cells 格（capacity 会涨到几百几千，全画没法看），
 * 但账面里的 size / capacity 报的是真数 —— 省的是打印，不省事实。
 */
void dynarray_print_memory_map(const DynArray *a, const char *tag, int max_cells)
{
    if (a == NULL) {
        printf("%s: (NULL)\n", tag);
        return;
    }
    if (a->data == NULL) {
        printf("  %s: data = NULL，一格都没租（懒启动的空表，零堆成本）\n", tag);
        return;
    }
    int shown = (a->capacity < max_cells) ? a->capacity : max_cells;
    printf("  %s  data 入口 = %p，每格 %zu 字节，共 %d 格（只显示前 %d 格），元素 %d 个\n",
           tag, (void *)a->data, sizeof(int), a->capacity, shown, a->size);

    printf("        下标: ");
    for (int i = 0; i < shown; i++) {
        printf("%-15d", i);
    }
    printf("\n        地址: ");
    for (int i = 0; i < shown; i++) {
        printf("%-15p", (void *)&a->data[i]);
    }
    printf("\n        数值: ");
    for (int i = 0; i < shown; i++) {
        printf("%-15d", a->data[i]);
    }
    printf("\n        身份: ");
    for (int i = 0; i < shown; i++) {
        /* 「元素」和「空位」都是 6 字节但只占 4 个显示列，所以宽度要给 17（= 15 + 2）
         * 才能和上面 %-15 的 数值/地址 行对齐；少给的 2 就是两个汉字多出来的字节 */
        printf("%-17s", (i < a->size) ? "元素" : "空位");
    }
    printf("\n        （图例：元素 = 真实数据；空位 = 已租但未使用，里面是残留的垃圾值）\n");
    if (a->capacity > shown) {
        printf("        （还有 %d 格没显示，下标 %d..%d；完整账面看 dynarray_print 那一行）\n",
               a->capacity - shown, shown, a->capacity - 1);
    }
    printf("        边界: 第 %d 格起就是空位 —— size=%d 是分界线，不是 capacity\n",
           a->size, a->size);
}

/*
 * 扩容战报：把"这一路 push 到底搬了几回家、抄了多少元素"总结成一小段。
 * 这是把"摊还 O(1)"从口号变成账单的地方 —— 数字全来自真实执行，不是我编的。
 */
void dynarray_print_grow_report(const DynArray *a, const char *tag)
{
    if (a == NULL) {
        return;
    }
    printf("  %s：push 了 %d 个元素 → 扩容 %d 次（其中真搬家 %d 次）；"
           "搬运账本：最坏 %llu 个 / 实测 %llu 个\n",
           tag, a->size, a->grow_count, a->grow_moved_count,
           a->copy_total, a->copy_moved_total);
    if (a->size > 0 && a->copy_total > 0) {
        printf("    平均每个元素分摊到 %.2f 次抄动（最坏口径）—— 这就是「摊还 O(1)」的字面意思\n",
               (double)a->copy_total / (double)a->size);
    }
}
