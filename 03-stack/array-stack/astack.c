#include <stdio.h>
#include <stdlib.h>   /* malloc / realloc / free */
#include "astack.h"

/*
 * ===================================================================
 * astack.c —— 数组栈的实现
 * ===================================================================
 * 先看一眼内存长什么样，后面每个函数都对着这张图解释：
 *
 *   auto_grow = false, capacity = 4, 已压入 10、20、30 又弹掉一次的情况：
 *
 *     下标:      0       1       2       3
 *            ┌───────┬───────┬───────┬───────┐
 *     data → │  10   │  20   │  30   │ (垃圾) │   ← 一整块连续内存，在堆上
 *            └───────┴───────┴───────┴───────┘
 *              ▲                        ▲
 *              栈底（最先入栈，最后才被拿走）    栈顶方向
 *                          top = 2 指向「30」这一格
 *     第 3 格还没写过，里面是残留的垃圾值 —— 这不是 bug，
 *     因为规矩是「只有下标 <= top 的格子才算栈里的元素」，别的格子不许看。
 *
 *   push(40)：top 先 2→3，再把 40 写进 data[3]
 *   pop()    ：先读 data[3]（=40）拿走，top 再 3→2。
 *              ⚠ data[3] 里的 40 并没有被抹掉，只是 top 不再包括它，
 *              下次 push 会直接把它盖掉 —— 「弹出 = 挪指针」而不是「清空格子」，
 *              这正是栈操作能 O(1) 的原因。
 */

/*
 * -------------------------------------------------------------------
 * 内部小工具：真正装满了，要不要扩容？
 * -------------------------------------------------------------------
 * 只有 auto_grow = true 才会走到这里。
 *
 * 为什么是「翻倍」而不是「每次多要 1 个格子」？
 *   每次 +1 的话，压 n 个元素就要搬 1+2+...+(n-1) ≈ n²/2 次数据，O(n²)，慢得离谱。
 *   翻倍的话，第 1、2、4、8、16… 次扩容分别搬 1、2、4、8… 个元素，
 *   总搬运次数 ≈ 2n，均摊到每个 push 上就是常数 —— 即「摊还 O(1)」。
 *   代价是最多多浪费一倍空间（capacity 最多是 size 的 2 倍），换速度，很划算。
 *
 * 关键 C 语言知识点：realloc 有两种结果
 *   ① 原地就变大了：地址不变，省掉搬运；
 *   ② 原地放不下：申请一块新的、把老数据拷过去、把老的 free 掉，于是**地址变了**。
 *   所以 realloc 之后原来那个指针就成悬空指针了，必须换成新地址再用。
 *   另外一定写成 `int *nd = realloc(...); if (nd != NULL) s->data = nd;`
 *   —— 不能写成 `s->data = realloc(s->data, ...)`：万一失败返回 NULL，
 *      s->data 直接被覆盖成 NULL，原来那块内存就再也找不回来，永久泄漏。
 * -------------------------------------------------------------------
 */
static bool astack_try_grow(AStack *s)
{
    int new_capacity = s->capacity * 2;

    /* 溢出保护：capacity 大到乘 2 变成负数时，说明已经荒唐地大了，直接放弃 */
    if (new_capacity <= s->capacity) {
        printf("      [失败] 容量溢出，无法继续扩容（当前 capacity=%d）\n", s->capacity);
        return false;
    }

    int *new_data = realloc(s->data, (size_t)new_capacity * sizeof(int));
    if (new_data == NULL) {
        /* 扩不动也不能把栈搞坏：老数组还在、top 没动，调用方按 push 失败处理 */
        printf("      [失败] 想扩容到 capacity=%d，但系统内存不够，栈维持原样\n", new_capacity);
        return false;
    }

    /* 从这一刻起：以后一律用 new_data（也就是 s->data）访问，老地址已经作废 */
    printf("    ♻ 栈满了，自动扩容：capacity %d → %d（第 %d 次扩容，"
           "搬了 %d 个元素，多花了 %d 个格子的空间）\n",
           s->capacity, new_capacity, s->grow_count + 1, s->top + 1, new_capacity - s->capacity);

    s->data      = new_data;   /* 换成新地址，旧的悬空指针不能再碰 */
    s->capacity  = new_capacity;
    s->grow_count += 1;
    return true;
}

/*
 * -------------------------------------------------------------------
 * 初始化
 * -------------------------------------------------------------------
 * 为什么不让 astack_init 返回一个 AStack，而是传指针进来填？
 *   结构体里有指针字段，返回一份拷贝容易让人误以为是独立的两份内存，
 *   传指针「就地初始化」是 C 里最朴素的写法，也方便以后想改成别处放。
 *
 * init_capacity 非法（<=0）时不申请内存，把栈标成「空壳」；
 * 之后 push 会因为 data==NULL 直接失败并提示，不会崩。
 * -------------------------------------------------------------------
 */
void astack_init(AStack *s, int init_capacity, bool auto_grow)
{
    if (s == NULL) {
        return;
    }

    if (init_capacity <= 0) {
        printf("      [警告] astack_init 收到非法容量 %d，先建成空壳，push 会一直失败\n",
               init_capacity);
        s->data       = NULL;
        s->capacity   = 0;
        s->top        = -1;      /* 流派 A：空栈就是 -1 */
        s->auto_grow  = auto_grow;
        s->grow_count = 0;
        return;
    }

    s->data = malloc((size_t)init_capacity * sizeof(int));
    if (s->data == NULL) {
        printf("      [错误] malloc %d 个 int 失败\n", init_capacity);
        s->capacity  = 0;        /* 用 capacity=0 当「没内存」的标记 */
        s->top       = -1;
        s->auto_grow = auto_grow;
        s->grow_count = 0;
        return;
    }

    /* malloc 的内容是脏的（残留数据），我们不读它就没事；
       想干净可以用 calloc 或手动 memset，这里为了强调「只读 top 以内的格子」就不清了。 */
    s->capacity   = init_capacity;
    s->top        = -1;          /* ⚠ 关键：空栈是 top=-1，不是 top=0。
                                     写成 0 的话 data[0] 会被当成一个「值为垃圾的栈顶元素」。*/
    s->auto_grow  = auto_grow;
    s->grow_count = 0;
}

/*
 * -------------------------------------------------------------------
 * 销毁
 * -------------------------------------------------------------------
 * free 之后把指针置 NULL，再把 top 复位。
 * 这样即使 main 里手滑又调了一次 destroy，free(NULL) 是合法空操作，
 * 不会 double free。（`make asan` 专门能抓 double free，跑一遍就知道重要。）
 * -------------------------------------------------------------------
 */
void astack_destroy(AStack *s)
{
    if (s == NULL) {
        return;
    }
    free(s->data);
    s->data       = NULL;   /* 撕掉纸条，防止拿旧地址去买菜 */
    s->capacity   = 0;
    s->top        = -1;
    s->auto_grow  = false;
    s->grow_count = 0;
}

/*
 * -------------------------------------------------------------------
 * push：压栈（唯一能往栈里放东西的口子）
 * -------------------------------------------------------------------
 * 一步步：
 *   ① 先判「装不装得下」。装不下分两种：开了开关 → 尝试翻倍扩容；
 *      没开开关 → 老老实实返回 false（这就是「固定容量栈满了怎么办」的答案：
 *      只能告诉调用者失败，让他自己决定是重试、丢旧数据还是换实现）。
 *   ② top 先加 1（流派 A：先把指针往上挪一格）
 *   ③ 把值写进新的栈顶格子
 *
 * ⚠ 边界：
 *   · data == NULL（初始化失败）：直接 false，绝不能写内存。
 *   · 判断顺序不能反：先判满再 top++，否则 top 越界之后才发现问题，
 *     栈就永久坏了（top 已经是脏的）。
 * -------------------------------------------------------------------
 */
bool astack_push(AStack *s, int value)
{
    if (s == NULL || s->data == NULL) {
        printf("      [失败] push(%d)：这个栈没有可用内存（capacity=0）\n", value);
        return false;
    }

    if (s->top + 1 >= s->capacity) {          /* 等价于流派 A 的 top == capacity-1 */
        if (!s->auto_grow) {
            printf("      [失败] push(%d)：栈已满（capacity=%d，top=%d），"
                   "且没开自动扩容 → 只能报错\n", value, s->capacity, s->top);
            return false;
        }
        if (!astack_try_grow(s)) {
            return false;                      /* 扩容也失败了，保持原状退出 */
        }
    }

    s->top += 1;                /* ② 指针先往上挪 */
    s->data[s->top] = value;    /* ③ 再写数据；顺序是流派 A 的死规矩，别和流派 B 混 */
    return true;
}

/*
 * -------------------------------------------------------------------
 * pop：弹栈（拿走栈顶）
 * -------------------------------------------------------------------
 * 一步步：
 *   ① 判空：top == -1 就是没盘子，返回 false
 *   ② 读 data[top]，先存进局部变量 v（因为 top 一减，那格就不算数了）
 *   ③ top -= 1
 *   ④ 调用者要值就写进 out_value；传 NULL 表示不要
 *
 * ⚠ 边界与坑：
 *   · 空栈 pop 是栈最常见的事故（对应现实里「拿浏览器返回键一路退到关掉程序」），
 *     必须判空。这里返回 false 而不是崩溃，是为了让 demo 能演示这个分支。
 *   · pop 完不清 data[top]：值还留在内存里。安全性要求高的场合（密码等）
 *     要主动抹掉；算法题无所谓，反正再 push 就盖掉了。
 * -------------------------------------------------------------------
 */
bool astack_pop(AStack *s, int *out_value)
{
    if (s == NULL || s->data == NULL) {
        printf("      [失败] pop：这个栈没有可用内存\n");
        return false;
    }

    if (s->top < 0) {              /* 空栈：top == -1 */
        printf("      [失败] pop：栈已经空了（top=-1），没有元素可弹\n");
        return false;
    }

    int v = s->data[s->top];       /* ② 先取值 */
    s->top -= 1;                   /* ③ 再挪指针；注意这里只是「作废」，格子里的值没清 */

    if (out_value != NULL) {       /* ④ 输出参数可以为 NULL —— 一个函数想带回两个值的标准写法 */
        *out_value = v;
    }
    return true;
}

/*
 * -------------------------------------------------------------------
 * peek：只看栈顶，不拿走
 * -------------------------------------------------------------------
 * 为什么单独需要它？很多场景只想知道「当前最要紧的是谁」，还不能把它弹出。
 *   · 括号匹配：看一眼栈顶的开括号是不是跟当前闭括号配对
 *   · 单调栈：比较 nums[i] 和栈顶元素大小，决定是否弹
 *   · 撤销操作：提示用户「即将撤销的是哪一步」
 * 注意函数签名里的 const：承诺「我只读不改这个栈」，误写编译器立刻报错。
 * -------------------------------------------------------------------
 */
bool astack_peek(const AStack *s, int *out_value)
{
    if (s == NULL || s->data == NULL || s->top < 0) {
        printf("      [失败] peek：栈是空的，没有栈顶可看\n");
        return false;
    }

    if (out_value != NULL) {
        *out_value = s->data[s->top];   /* 和 pop 一样的位置，只是 top 不减 */
    }
    return true;
}

/* ---------- 下面几个查询函数都是一行，但每一行都值得解释 ---------- */

bool astack_is_empty(const AStack *s)
{
    return s == NULL || s->top < 0;   /* 流派 A：只有 top==-1 才叫空 */
}

bool astack_is_full(const AStack *s)
{
    if (s == NULL) {
        return true;
    }
    /* 开了扩容开关，「满」就不是终点了：还能翻倍，所以直接说没满 */
    if (s->auto_grow) {
        return s->capacity > 0 ? false : true;
    }
    return s->top == s->capacity - 1;   /* 最后一格也被占了 */
}

int astack_size(const AStack *s)
{
    if (s == NULL || s->top < 0) {
        return 0;                        /* top=-1 → 0 个元素；top+1 就是个数 */
    }
    return s->top + 1;
}

int astack_capacity(const AStack *s)
{
    return (s == NULL) ? 0 : s->capacity;
}

/*
 * -------------------------------------------------------------------
 * 教学用打印：把整摞盘子一行摊开给人看
 * -------------------------------------------------------------------
 * 真正的栈不提供这个操作（栈只允许碰栈顶）。这里打印是为了让「后进先出」看得见。
 *
 *   输出形如（左边是栈底、右边是栈顶方向，^ 标出 top 落在哪一格）：
 *     [ 0: 10] [ 1: 20] [ 2: 30] [ 3:  ·]   ← 左=栈底，右=栈顶方向
 *     [ 0:   ] [ 1:   ] [ 2:  ^] [ 3:   ]   ← ^ 就是 top=2
 *
 *   为什么第二行要用【完全相同的循环、完全相同的格子宽度】再生成一遍？
 *   因为「先打一串空格再把 ↑ 顶到第 i 格下面」这种写法，一旦前缀里有中文
 *   （中文在终端里占 2 列）就会算错列数、箭头歪到隔壁格子去。
 *   用同一个循环打同一格宽的字符，宽度天然对齐，不用算列，这是最稳的办法。
 *   每格固定 9 列：` [%2d:%3d]`（1 空格 + [ + 2 列下标 + : + 3 列内容 + ]）。
 * -------------------------------------------------------------------
 */
void astack_print(const AStack *s, const char *label)
{
    if (s == NULL) {
        printf("  (空指针，没有栈可打印)\n");
        return;
    }

    printf("  %s  size=%d, top=%d, capacity=%d, 已扩容 %d 次\n",
           label, astack_size(s), s->top, s->capacity, s->grow_count);

    if (s->capacity <= 0) {
        printf("    （这个栈没有分到内存）\n");
        return;
    }

    /* 第一行：每个格子的「下标 : 值」 */
    printf("    ");
    for (int i = 0; i < s->capacity; i++) {
        if (i <= s->top) {
            printf(" [%2d:%3d]", i, s->data[i]);   /* 已占用：显示真值 */
        } else {
            printf(" [%2d:  ·]", i);               /* 空格子：· 表示还没算进栈里 */
        }
    }
    printf("   ← 左=栈底，右=栈顶方向\n");

    /* 第二行：同样的循环，只在 top 那一格里画个 ^，别的格子留空 */
    printf("    ");
    for (int i = 0; i < s->capacity; i++) {
        if (i == s->top) {
            printf(" [%2d:%3s]", i, "^");
        } else {
            printf(" [%2d:%3s]", i, " ");
        }
    }
    if (s->top >= 0) {
        printf("   ← ^ 就是 top=%d 指的格子\n", s->top);
    } else {
        printf("   ← 没有任何格子被标出：空栈时 top=-1，一格都不算\n");
    }
}
