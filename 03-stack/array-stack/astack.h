#ifndef ASTACK_H
#define ASTACK_H

/*
 * ===================================================================
 * astack.h —— 数组栈的「菜单」
 * ===================================================================
 * 这个文件只有声明，没有实现。把它当成饭店门口的菜单看：
 *   先知道「能点哪些菜」（有哪些操作），想知道「怎么炒的」再去翻 astack.c。
 *
 * 栈（stack）就一句话：**只能从同一头进出的一摞盘子**。
 *   最后放上去的那个盘子，必须最先被拿走 —— 后进先出，英文缩写 LIFO
 *   （Last In, First Out）。
 *
 * 本文件定义的结构体 AStack 用「一段连续数组 + 一个下标」来实现栈：
 *   数组负责存盘子，下标 top 负责记住「最上面那个盘子在第几格」。
 *
 * ⚠ 关于 top 的两种流派（本项目选第一种，全项目自洽）：
 *   流派 A（本项目用的）：top = **栈顶元素自己的下标**，空栈时 top = -1。
 *       · push：先 top++ 再写数据         → data[++top] = v;
 *       · pop：先读 data[top] 再 top--    → v = data[top--];
 *       · 元素个数 = top + 1
 *       · 判空 = (top == -1)，判满 = (top == capacity - 1)
 *   流派 B：top = **下一个要写入的空位下标**，空栈时 top = 0。
 *       · push：先写数据再 top++          → data[top++] = v;
 *       · pop：先 top-- 再读             → v = data[--top];
 *       · 元素个数 = top
 *       · 判空 = (top == 0)，判满 = (top == capacity)
 *   两种都能写对，**关键是别混着用**：混了就会少存一个或多存一个，
 *   表现为「明明没满却报栈满」「peek 读到刚 pop 掉的旧值」。
 *   本仓库教材风格的书（如《数据结构（C 语言版）》）多用流派 A，
 *   所以这里跟着用 A，判空写 top == -1，一眼就知道是「没有元素」。
 */

#include <stdbool.h>   /* C11 标准库，提供 bool / true / false，纯粹为了让代码读起来像人话 */

/*
 * 栈的全部家当就这 5 个字段。
 * 数组在堆上（malloc 来的），结构体本身放哪由调用者决定（demo 里放栈上）。
 */
typedef struct {
    int  *data;        /* 指向那摞盘子所在的连续内存；data[0] 是最先入栈、垫在最底下的 */
    int   capacity;    /* 当前最多能装几个元素 = data 数组的长度 */
    int   top;         /* 栈顶元素的【下标】；空栈 = -1（流派 A，见文件头注释） */
    bool  auto_grow;   /* 扩容开关：true = 装满了自动把 capacity 翻倍；false = 老老实实报「栈满」 */
    int   grow_count;  /* 一共扩容过几次，用来在演示里把「摊销成本」这件事数给你看 */
} AStack;

/* ---------- 生命周期 ---------- */

/*
 * 初始化一个栈。
 *   init_capacity：一开始给多少格子（会立刻 malloc）
 *   auto_grow    ：true 表示允许满了自动翻倍
 * 申请失败时把 capacity 置 0、data 置 NULL，后续所有操作都会安全地失败，
 * 不会去解引用空指针。
 */
void astack_init(AStack *s, int init_capacity, bool auto_grow);

/*
 * 销毁栈，把 malloc 来的数组还回去，并把结构体复位成「空的、没内存」的状态。
 * 谁 malloc 谁 free：这里 free 掉 astack_init 申请的那块内存，避免内存泄漏。
 */
void astack_destroy(AStack *s);

/* ---------- 核心操作 ---------- */

/* push：压栈。成功返回 true；栈满且没开扩容开关时返回 false（并在屏幕上说明原因） */
bool astack_push(AStack *s, int value);

/*
 * pop：弹栈。
 *   元素值通过 out_value 这个「输出参数」带回去（C 只能 return 一个值）。
 *   栈空时返回 false，并且不写 out_value。
 *   out_value 允许传 NULL，表示「我只想把栈顶扔掉，不关心它是几」。
 */
bool astack_pop(AStack *s, int *out_value);

/*
 * peek / top：看一眼栈顶但**不**拿走。
 *   栈空返回 false。这就是栈和「直接读数组」的区别：只允许看第 top 格，
 *   别的格子按规矩摸不到（虽然 C 语言拦不住你，代码里我们不这么干）。
 */
bool astack_peek(const AStack *s, int *out_value);

/* ---------- 查询 ---------- */

bool astack_is_empty(const AStack *s);   /* 栈里一个元素都没有 */
bool astack_is_full(const AStack *s);    /* 只有没开 auto_grow 时才有意义 */
int  astack_size(const AStack *s);       /* 当前有几个元素 = top + 1 */
int  astack_capacity(const AStack *s);   /* 当前能装几个元素 */

/* ---------- 演示辅助 ---------- */

/*
 * 把栈里所有元素一行打印出来（栈底在左、栈顶在右），并在栈顶那一格下面画个箭头。
 * 这不是栈的正式操作 —— 真正的栈应该「只准看栈顶」，这里是为了教学才全倒出来，
 * 让你在输出里亲眼看到「后进先出」到底是怎么个先后顺序。
 */
void astack_print(const AStack *s, const char *label);

#endif /* ASTACK_H */
