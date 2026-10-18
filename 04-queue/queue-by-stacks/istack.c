/*
 * ===================================================================
 * istack.c —— 极简数组 int 栈：push / pop / top / empty
 * ===================================================================
 * 四个函数加起来不到 40 行，是**故意的**：这一节的主角是"两个栈怎么拼出队列"，
 * 栈本身绝不能抢戏。所以这里没有扩容、没有泛型、没有函数指针，只有 int 和一个下标。
 *
 * 唯一的流派选择（全仓库统一，见 istack.h 文件头）：
 *   top = 栈顶元素【自己的】下标；空栈 top == -1。
 *   于是：
 *     push：先 top++，再写 data[top]     ← 顺序反了会把当前栈顶覆盖掉
 *     pop ：先读 data[top]，再 top--     ← 顺序反了会拿到下面那个元素
 *   这两个"先动指针还是先动格子"的顺序，就是本节最容易写错的两行。
 */

#include "istack.h"

#include <stddef.h>   /* NULL */

/*
 * 初始化 / 销毁
 *
 * 为什么数组栈也要给 destroy？它一个 malloc 都没有。
 *   ① 仓库规矩：有 init 就要有 destroy，读代码的人不用先判断"这个类型有没有堆内存"；
 *   ② 这里做的是"把状态摆回初值"：top 回 -1、计数器归零、格子涂成哨兵值。
 *      涂哨兵是为了**复用安全**：如果只把 top 摆回 -1 就接着用，旧数据还躺在格子里，
 *      任何"忘了判空就摸 data[i]"的 bug 都会读到上一次的残留值，比读到垃圾更难查。
 */
void istk_init(IntStack *s)
{
    if (s == NULL) {
        return;
    }
    for (int i = 0; i < ISTK_CAP; i++) {
        s->data[i] = 0;
    }
    s->top    = -1;                 /* ★ 空栈的唯一判据 */
    s->n_push = 0;
    s->n_pop  = 0;
}

void istk_destroy(IntStack *s)
{
    if (s == NULL) {
        return;                     /* 幂等：销毁两次不炸 */
    }
    for (int i = 0; i < ISTK_CAP; i++) {
        s->data[i] = -1;            /* 涂哨兵：残留值一眼可辨 */
    }
    s->top    = -1;
    s->n_push = 0;
    s->n_pop  = 0;
}

/*
 * push：压到栈顶
 *
 * 为什么先判满再 top++：满了还往前写就是越界写（ISTK_CAP 格之后的内存是别人的）。
 * 本函数的合同是"失败时一个字节都不写"，所以判满必须写在最前面。
 */
bool istk_push(IntStack *s, int value)
{
    if (s == NULL) {
        return false;
    }
    if (s->top + 1 >= ISTK_CAP) {   /* 等价于 istk_full(s)；写成这样是为了把边界讲明白 */
        return false;
    }
    s->top++;                       /* ① 先把手伸到新的空位 */
    s->data[s->top] = value;        /* ② 再把值写进去 */
    s->n_push++;
    return true;
}

/*
 * pop：把栈顶取走
 *
 * out 参数是本仓库的统一写法（"流派：返回值表成败，结果走 _out"）。
 * 为什么不让它直接返回 int？因为"空栈"没有合法的 int 可返回 ——
 * 用 -1 或 INT_MIN 当哨兵都会撞到真实数据（本仓库的元素类型就是 int）。
 *
 * 空栈时 *out 一个字都不改，调用方拿到的还是它自己放进去的值 —— 这样"忘了判返回值"
 * 的 bug 至少不会引入新的垃圾数据，行为更容易解释。
 */
bool istk_pop(IntStack *s, int *out)
{
    if (s == NULL || out == NULL) {
        return false;
    }
    if (s->top < 0) {
        return false;               /* 空栈：不动 *out，直接失败 */
    }
    *out = s->data[s->top];         /* ① 先把栈顶的值抄出去 */
    s->top--;                       /* ② 再把手缩回来（格子内容不用擦：top 之后的都算不存在）*/
    s->n_pop++;
    return true;
}

/* top：只看不动。注意它不增加 n_pop —— 看和拿是两件事，账要分开记。 */
bool istk_top(const IntStack *s, int *out)
{
    if (s == NULL || out == NULL) {
        return false;
    }
    if (s->top < 0) {
        return false;
    }
    *out = s->data[s->top];
    return true;
}

bool istk_empty(const IntStack *s)
{
    return (s == NULL) ? true : (s->top < 0);
}

bool istk_full(const IntStack *s)
{
    return (s == NULL) ? true : (s->top + 1 >= ISTK_CAP);
}

/*
 * 只读"从栈底数第 idx 格"。越界一律返回 false 并且不碰 *out。
 * 存在的唯一理由：q_dump 要把整条栈画出来（需要从头到尾遍历），
 * 而 push / pop / top / empty 四个动作里没有任何一个能"遍历"。
 * 有了它，qsq.c 就不必知道 data 这个数组长什么样 —— 边界保住了。
 */
bool istk_at(const IntStack *s, int idx, int *out)
{
    if (s == NULL || out == NULL) {
        return false;
    }
    if (idx < 0 || idx > s->top) {  /* 空栈 top == -1 → 任何 idx 都算越界 */
        return false;
    }
    *out = s->data[idx];
    return true;
}

int istk_size(const IntStack *s)
{
    if (s == NULL) {
        return 0;
    }
    return s->top + 1;              /* top = -1 → 0 个；top = 0 → 1 个。这就是"流派 A"的红利 */
}
