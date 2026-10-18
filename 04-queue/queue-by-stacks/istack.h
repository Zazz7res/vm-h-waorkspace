#ifndef ISTK_H
#define ISTK_H

/*
 * ===================================================================
 * istack.h —— 本目录自带的极简数组 int 栈（"几行就够"的那个栈）
 * ===================================================================
 * 为什么这里要**再写一份**栈，而不去 include 03-stack/array-stack/ 的头文件？
 *
 *   因为本仓库有一条硬规矩：**每个叶子目录自包含**，只能 include 本目录的 .h 和标准库。
 *   跨目录 include 在这台机器上会直接编译失败（gcc 找不到那个路径，Makefile 也不给 -I）。
 *   更重要的是教学上的理由：这一节要证明的是
 *   「队列的 FIFO 行为，**只靠**栈的 push / pop / top / empty 这四个动作拼出来」。
 *   如果栈是别的目录借来的，读者得先跳去另一个文件才知道栈能干什么；
 *   现在栈就 40 行、就在同一个目录里，往下翻两页就能看完 —— 边界清清楚楚。
 *
 * ★ 对外的合同（qsq.c 里**只用**这几个动作，绝不多拿）：
 *     istk_push(s, v)     把 v 压到栈顶
 *     istk_pop(s, &out)   把栈顶元素取走，写进 out
 *     istk_top(s, &out)   只看栈顶，不取走
 *     istk_empty(s)       是不是空
 *     istk_size(s)        有几个
 *     istk_at(s, i, &out) 只读第 i 格 —— 纯粹为"把栈画出来"开的口子
 *   qsq.c 里如果出现 `s->data[i] = ...` 这种直接改格子内容的代码，就说明边界破了 ——
 *   那样"队列行为只来自两个栈的组合"这句话就废了。
 *
 * ⚠ 下标流派（和 03-stack/array-stack 一致，全仓库统一）：
 *   · top = 栈顶元素【自己的】下标（"流派 A"：指针指在真实元素上，不是指在空位上）
 *   · 空栈 = top == -1，有 n 个元素时 top == n - 1
 *   · 元素住在 data[0 .. top]，data[0] 是栈底（最早压进来的那个）
 *   为什么空栈用 -1 而不是 0：用 0 的话"栈里第 0 格有元素"和"空栈"就没法区分了。
 */

#include <stdbool.h>

/*
 * 容量固定 64，而且**故意**不做大：
 *   1) 这一节所有演示的元素数都在个位数，压测那一幕同时在队里也不超过几十；
 *   2) 固定小数组让"栈里现在有几个、谁是栈顶"一眼可见，不用先理解 realloc；
 *   3) 满了怎么办？—— istk_push 返回 false，让调用方看见失败。
 *      工程版可以给 in 栈做扩容，或者干脆换成链表栈（03-stack/linked-stack 那种）。
 *      本节把"容量不足"变成一个**明确可测的坑**（main.c 第 8 幕现场撞墙），
 *      比偷偷 realloc 更有讲头。
 */
#define ISTK_CAP 64

typedef struct {
    int  data[ISTK_CAP];  /* 那些格子。data[0] = 栈底，data[top] = 栈顶 */
    int  top;             /* 栈顶元素的下标；空栈 = -1（见文件头那段）*/
    long n_push;          /* 计数器：push 成功了多少次（均摊那一幕要拿它对账）*/
    long n_pop;           /* 计数器：成功 pop 了多少次 */
} IntStack;

/* ---------- 生命周期 ---------- */

/* 清空并把 top 摆回 -1。数组在结构体里，没有 malloc，所以 istk_destroy 也不 free ——
 * 但仍然给一个：把计数器复位、把格子涂成哨兵值，方便"同一个结构体复用两次"时不留残留。 */
void istk_init(IntStack *s);
void istk_destroy(IntStack *s);

/* ---------- 核心动作 ---------- */

/* 压入。满了返回 false 并且**一个字节都不写**（宁可失败，也不写越界）。 */
bool istk_push(IntStack *s, int value);

/* 弹出。空栈返回 false，此时 *out 保持原值不动（"要么改值要么改标志，不能两样都不清"）。 */
bool istk_pop(IntStack *s, int *out);

/* 只看不拿。空栈同样返回 false。 */
bool istk_top(const IntStack *s, int *out);

bool istk_empty(const IntStack *s);
bool istk_full(const IntStack *s);

/* 现在栈里有几个元素。这就是 top + 1，写成函数只为了让调用方别自己算错。 */
int istk_size(const IntStack *s);

/*
 * 读"从栈底数第 idx 格"的元素（idx 从 0 开始，idx == size-1 就是栈顶）。
 * ★ 这个口子是**专门为可视化开的**：画栈的时候想从栈底往上遍历，可 pop 会把数据拿走。
 *   为什么不让 qsq.c 直接读 s->data[i]？因为"队列的行为只来自两个栈的组合"
 *   这句话要求 qsq.c 只能通过栈的接口看栈。所以：开一个**只读**访问器，并注明只准用于打印。
 */
bool istk_at(const IntStack *s, int idx, int *out);

#endif /* ISTK_H */
