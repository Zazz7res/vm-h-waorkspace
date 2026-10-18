#ifndef DYNAMIC_ARRAY_H
#define DYNAMIC_ARRAY_H

/*
 * ===================================================================
 * dynamic-array.h —— 动态数组（vector）的「菜单」
 * ===================================================================
 * 上一节的静态数组撞了墙：长度编译期定死，装满就只能失败。
 * 动态数组就是把那堵墙拆了 —— 用 malloc/realloc 在堆上租格子，
 * 不够了就换一块更大的，容量可以一直长（直到内存不够）。
 *
 * 【本节最重要的一句话：size 和 capacity 是两个数字，绝不是一回事】
 *     capacity = 现在【租了】几格（房子有几间）
 *     size     = 现在【住了】几个人（有几个真实元素）
 *   永远满足 size <= capacity。
 *   静态数组里这两个数字被迫相等（所以只需要 length 一个字段）；
 *   动态数组故意让 capacity 比 size 大一点，多出来的空位就是
 *   "下次插入不用再搬家"的预付定金 —— 整个 vector 的性能秘密全在这个差值里。
 *
 * 【为什么扩容要翻倍，而不是 +1？】
 *   push_back 绝大多数时候只是往空位写一个值（O(1)）；
 *   只有 capacity 用完的那一次要 realloc + 把老元素整排抄过去（O(n)）。
 *   翻倍的话，第 k 次扩容后要再走 2^(k-1) 步才轮到下一次扩容，
 *   n 次 push 的抄动总量是 1+2+4+...+n < 2n，摊到每次 push 还是 O(1) —— 这叫「摊还 O(1)」。
 *   如果每次只 +1，那每 push 一个元素都要抄一遍全部老元素，
 *   总量变成 1+2+...+n = n²/2，退化成 O(n²)。
 *   本文件把扩容策略做成了【两个 int 字段】（grow_multiplier / grow_increment），
 *   不是搞泛型，只是为了让 main 能【真跑一遍】+1 策略，把两个数字摆在一起对比，
 *   而不是让你相信一句口头承诺。
 *
 * 【缩容为什么要等到 1/4 才砍一半，而不是 1/2 就砍？】
 *   如果 size 掉到 capacity 的 1/2 就缩容，那么在最坏情况下
 *   「pop 一下 push 一下」会每次都踩在阈值上：缩容 → 立刻又扩容 → 又缩容……
 *   每次都要整块搬家，这叫【抖动 / thrashing】，代价可以是无限次的 O(n)。
 *   留出 1/4 这条"缓冲带"之后，缩容完 size 最多只有新容量的 1/2，
 *   要再加回一倍才会碰到扩容线 —— 一缩一扩之间隔了 n/2 步，成本被摊平了。
 *   main 的第 6 幕会把两种阈值各跑一遍，把 realloc 次数打印出来对比。
 *
 * 【free 之后必须把账面归零】
 *   destroy 里要做的三件事：free(data) → data = NULL → size = capacity = 0。
 *   不置零的话，结构体里留着一个已经作废的地址和一套"看起来很正常"的数字，
 *   下一次误用就是悬空指针；置零之后任何操作都会立刻撞上"空"的判断，当场暴露。
 */

#include <stdbool.h>   /* bool / true / false：让"这次操作合法吗"读起来像人话 */

/*
 * 起始容量下限：init 传 0 或负数时，表示"先不租房子"（capacity=0, data=NULL），
 * 等第一次 push 再现场租 —— 这也是合法用法，空 vector 不占堆内存。
 * 而一旦开始租，最小的租约就是 DYNARRAY_MIN_CAPACITY 格，别 1 格 1 格地租。
 */
#define DYNARRAY_MIN_CAPACITY 4

/*
 * 默认缩容阈值的分母 = 4，即"元素数掉到容量的 1/4（含压线）才砍一半"。
 * 判定式统一写成不带除法、不带小数的整数比较：
 *      size * shrink_denominator <= capacity * shrink_numerator
 * 也就是 size * 4 <= capacity * 1  →  size <= capacity/4。
 * 为什么这样写？因为 `size <= capacity / 4` 里的整数除法会向下取整，
 * 边界上差一格就抖一次；乘法形式没有取整歧义，是工程里更稳的写法。
 */
#define DYNARRAY_DEFAULT_SHRINK_DENOMINATOR 4

typedef struct {
    int *data;                 /* 堆上那排连续格子的入口；没租的时候是 NULL */
    int size;                  /* 元素个数：data[0 .. size-1] 是真实数据 */
    int capacity;              /* 已分配格子数：data[size .. capacity-1] 是空位（残留值） */

    /* ---- 下面全是"记账 + 可演示"的字段，工程代码里不会留这么多 ---- */
    int base_capacity;         /* 初始容量：缩容绝不砍到它以下（至少 DYNARRAY_MIN_CAPACITY） */
    int grow_multiplier;       /* 扩容公式：新容量 = 旧容量 * grow_multiplier + grow_increment */
    int grow_increment;        /* 默认 2 / 0 = 翻倍；改成 1 / 1 就是"每次 +1"的对照组 */
    int shrink_numerator;      /* 缩容条件：size * shrink_denominator <= capacity * shrink_numerator */
    int shrink_denominator;    /* 默认 1 / 4；改成 1 / 2 就是会抖动的那个反面教材 */
    int grow_count;            /* 扩容执行了多少次（= realloc 被调用的次数） */
    int grow_moved_count;      /* 其中有多少次 realloc 真的搬了家（返回的地址变了） */
    int shrink_count;          /* 缩容执行了多少次 */
    int shrink_moved_count;    /* 其中多少次缩容的 realloc 真的搬了家（单独记，别和扩容的账混） */
    int log_enabled;           /* 1 = 每次扩缩容都打印 realloc 现场；0 = 安静记账（做对照实验时用） */
    unsigned long long copy_total;
    /* ↑ 最坏情况账本：每次扩容都算"把当时的旧容量整排抄一遍"。
         摊还分析用的就是这本账，它给的是上界，和平台 realloc 聪不聪明无关。 */
    unsigned long long copy_moved_total;
    /* ↑ 实测账本：只有 realloc 真的搬家了才累加。地址没变（原地扩大）时一个字节都不用抄。
         两本账都摆出来，你才知道"翻倍为什么便宜"是上界结论，而不是运气。 */
} DynArray;

/* ---------- 生命周期 ---------- */

/*
 * 建表。init_capacity 是给定的起始格子数（<=0 表示先不占内存，等第一次 push 再说）。
 * 返回 false 只在 malloc 失败时发生，此时结构体保持"全 0 + data=NULL"的安全状态。
 */
bool dynarray_init(DynArray *a, int init_capacity);

/*
 * 销毁：free + 指针置 NULL + size/capacity 归零，三件事一件都不能少。
 * 调用之后这个结构体可以安全地再 init 一次，也可以再 destroy 一次（幂等），
 * 因为 free(NULL) 是合法的空操作 —— 这就是"置 NULL"的第二个好处。
 */
void dynarray_destroy(DynArray *a);

/* ---------- 只有读：一个快、一个安全 ---------- */

int  dynarray_size(const DynArray *a);       /* 元素个数 */
int  dynarray_capacity(const DynArray *a);   /* 已分配格子数 */
bool dynarray_is_empty(const DynArray *a);

/*
 * at：安全检查版。越界返回 false，*value_out 保持原值不动。
 * 正式代码里默认走这个 —— 边界检查的成本远小于一次诡异 bug 的成本。
 */
bool dynarray_at(const DynArray *a, int index, int *value_out);

/*
 * get：不设防版，等价于原生 a->data[index]，越界就是未定义行为（ASan 会报 heap-buffer-overflow）。
 * 为什么还要留它？两个原因：
 *   1) 让你看清"语言层不检查"和"接口层检查"是可以分开的两档，
 *      C++ 的 vector 就是这么设计 operator[] 和 at() 的；
 *   2) 内层循环里下标已经由程序自己保证合法时，省掉一次判断。
 * ⚠ 用它的唯一前提：index 一定在 [0, size) 里。main 里只在确认合法后才用它。
 */
int  dynarray_get(const DynArray *a, int index);

/* 按值找第一个匹配的下标，找不到 -1：数组不排序就只能线性扫，O(n)。 */
int  dynarray_find(const DynArray *a, int value);

/* ---------- 增删：注意哪一步会触发扩容/缩容 ---------- */

/*
 * 尾部追加。空位够用就只写一个值（O(1)）；
 * 空位用完就按公式扩 capacity（一般翻倍），必要时 realloc 搬家（O(n)）。
 * 扩容现场会打印：旧地址 → 新地址、搬了多少个元素 —— 摊还 O(1) 靠看得见才信得过。
 */
bool dynarray_push_back(DynArray *a, int value);

/* 弹出尾部：唯一不用挪格子的删除，O(1)；顺手做缩容检查。 */
bool dynarray_pop_back(DynArray *a, int *value_out);

/*
 * 在 index 处插入（0 <= index <= size）。两步代价：
 *   1) 空位不够 → 先扩容（可能搬家）
 *   2) 把 [index, size) 右移一格（memmove，区间重叠所以不能用 memcpy）
 * 所以中间插入是 O(n) —— 和静态数组一模一样，动态数组只解决了"装不下"，
 * 没解决"挪格子"。想要不挪格子的增删，去看 02-linked-list/。
 */
bool dynarray_insert_at(DynArray *a, int index, int value);

/* 删除第 index 格（0 <= index < size）：后半排左移一格，O(n)；顺手做缩容检查。 */
bool dynarray_remove_at(DynArray *a, int index, int *value_out);

/* ---------- 教学专用：把扩容/缩容策略临时改成别的档位 ---------- */

/*
 * ⚠ 正式代码里绝对不需要这两个函数 —— 它们存在的唯一理由是让 main 能
 *   【真的跑一遍】"+1 扩容"和"1/2 就缩容"两种坏策略，把 realloc 次数和
 *   抄动元素数打印出来，用实测数字证明"翻倍 / 1/4 阈值"为什么更好，
 *   而不是让你信一句口头结论。
 * 参数都是普通 int，没有函数指针、没有泛型 —— 本项目一律如此。
 */
void dynarray_set_growth_policy(DynArray *a, int multiplier, int increment);
void dynarray_set_shrink_policy(DynArray *a, int numerator, int denominator);

/* ---------- 打印 ---------- */

/* 一行值 + 全部账面（size / capacity / data 地址 / 扩容次数 / 搬家次数 / 两本搬运账） */
void dynarray_print(const DynArray *a, const char *tag);

/*
 * 内存地图：把 capacity 的前 max_cells 格画出来（下标 / 地址 / 数值 / 身份）。
 * 为什么要限制打印数量？capacity 会涨到几百几千格，全打出来没法看。
 * "‖ 左边是元素，右边是空位"这条边界，是这一节最想让你形成的直觉。
 */
void dynarray_print_memory_map(const DynArray *a, const char *tag, int max_cells);

/* 把"这一节到底搬了多少家"总结成一张小表打在输出里 */
void dynarray_print_grow_report(const DynArray *a, const char *tag);

#endif /* DYNAMIC_ARRAY_H */
