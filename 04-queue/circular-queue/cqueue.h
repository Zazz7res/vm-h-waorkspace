#ifndef CQUEUE_H
#define CQUEUE_H

/*
 * ===================================================================
 * cqueue.h —— 循环队列的「菜单」
 * ===================================================================
 * 上一节（04-queue/array-queue）留了个烂摊子：front 和 rear 只会往右走、从不回头，
 * 于是 rear 顶到墙时前面明明空着一堆格子，程序却报"满" —— 这叫**假满**。
 *
 * 本节的解法只有一行代码，但它是整章最重要的一个技巧：**取模（求余数）**。
 *
 *   rear = (rear + 1) % capacity;      ← 到墙了不报错，而是绕回第 0 格
 *   front = (front + 1) % capacity;    ← 队头也一样绕圈
 *
 * 为什么取模能绕圈？把下标想象成**时钟的指针**：
 *   capacity = 6 就像一个只有 6 个刻度的钟：0 → 1 → 2 → 3 → 4 → 5 → 又回到 0。
 *   普通数组下标走到 6 就出界了，但 `6 % 6 == 0`、`7 % 6 == 1`、`8 % 6 == 2`……
 *   取模天生就是"绕回开头"的操作，所以下标永远不会越界。
 *   这样 front 和 rear 沿同一个圆周跑，rear 追着 front，只要两者之间"弧长"没满一圈就能塞人。
 *   数组还是那块连续内存，只是把它**当成一个环来看**。
 *
 * ★ 环带来一个新麻烦：**空和满看起来一模一样**（都是 front == rear）。
 *   所以必须再引入一个信息来区分，方案有两条，本节【两条都实现】：
 *
 *   ┌─ 方案 A（本文件主推，推荐日常用）：多带一个 size 计数器
 *   │    · 判空 size == 0，判满 size == capacity
 *   │    · 格子一个不浪费，容量 6 就是真能装 6 个
 *   │    · 代价：多一个 int 字段，且每次进出都要 size++ / size--（两处要成对写，别漏）
 *   │    · 对应类型：CQueue，函数 cqueue_*
 *   └─ 方案 B（老教材经典写法）：约定「永远少用一个格子」
 *        · 判空 front == rear；判满 (rear + 1) % capacity == front
 *          （意思是：rear 再往前迈一步就撞到 front 了 —— 那一步的位置故意不让站人）
 *        · 好处：不用 size 字段，纯靠两个下标就能区分空/满
 *        · 代价：**capacity 个格子只能装 capacity - 1 个人**，永久浪费一格；
 *                而且判满表达式一旦忘了 +1 或忘了取模就是灾难。
 *                为什么非浪费一格不可？因为如果不留这格，
 *                装满时 rear 绕一圈正好又和 front 重合，跟"空队列"的状态完全一样，
 *                两个下标不足以区分这两种情况 —— 必须留出"错开一格"的空间。
 *        · 对应类型：CQueueWaste，函数 cwaste_*（demo 里现场撞给你看）
 *
 * front / rear 的约定沿用上一节：
 *   · front = 队头元素的下标（指在真实元素上）
 *   · rear  = 下一个入队位置（半开区间的右端，环上的"弧的终点"）
 *   · 元素住在环上从 front 出发、到 rear 之前的那段弧里，个数 = size（方案 A）
 */

#include <stdbool.h>

/* ==================================================================
   方案 A：size 计数器版（推荐）
   ================================================================== */
typedef struct {
    int *data;         /* 环：一整块连续内存，被当成首尾相接的圈来用 */
    int  capacity;     /* 环上一共几个格子（= 数组长度），也就是最多能装几个人 */
    int  front;        /* 队头元素下标，出队后 front = (front+1) % capacity */
    int  rear;         /* 下一个入队位置，入队后 rear  = (rear+1) % capacity */
    int  size;         /* ★ 当前人数 —— 有了它，"空"和"满"就分得开了 */

    /* ---- 下面两个只是统计量，专给 demo 用，不影响队列内容 ---- */
    int  wrap_count;   /* front/rear 一共"绕回 0"了几次 —— 就是取模真正救了场的次数 */
    int  reject_full;  /* 因为真满（size == capacity）被拒绝的入队次数 */
} CQueue;

/* 初始化：malloc capacity 个格子，front=rear=size=0。失败则建成空壳，后续安全失败 */
void cqueue_init(CQueue *q, int capacity);

/* 销毁：free 数组并把指针置 NULL、下标与计数复位（谁 malloc 谁 free） */
void cqueue_destroy(CQueue *q);

/* 入队：真满（size == capacity）才失败 —— 注意判的是 size 而不是 rear 顶没顶墙，
 *        这正是它和上一节朴素队列的本质区别：假满被彻底消灭了 */
bool cqueue_enqueue(CQueue *q, int value);

/* 出队：值通过 out_value 带回（可传 NULL 表示不要值）；size == 0 时失败且不写脏 */
bool cqueue_dequeue(CQueue *q, int *out_value);

/* 看队头（data[front]）/ 看队尾（data[(rear-1+capacity)%capacity]）但不改状态 */
bool cqueue_peek_front(const CQueue *q, int *out_value);
bool cqueue_peek_rear(const CQueue *q, int *out_value);

bool cqueue_is_empty(const CQueue *q);   /* size == 0 */
bool cqueue_is_full(const CQueue *q);    /* size == capacity，唯一的"满"的判据 */
int  cqueue_size(const CQueue *q);       /* 直接返回 size 字段，O(1) */
int  cqueue_capacity(const CQueue *q);

/* 教学打印：把环摊平成一条数组来看，标出 F/R，并说明"下标是绕着走的" */
void cqueue_print(const CQueue *q, const char *label);

/* 只画环的形状（ASCII 圆圈），把 front/rear 在圈上的位置标出来 */
void cqueue_ring(const CQueue *q);

/* ==================================================================
   方案 B：少用一个格子版（老教材写法，对照用）
   ==================================================================
 * 没有 size 字段，全靠 front / rear 两个下标的相对位置判断空满：
 *   空：front == rear
 *   满：(rear + 1) % capacity == front     ← 差一格就撞上，所以那一格永远不许站人
 * 能用容量 = capacity - 1。
 */
typedef struct {
    int *data;         /* 环 */
    int  capacity;     /* 格子总数（比"能装几个人"多 1） */
    int  front;        /* 队头元素下标 */
    int  rear;         /* 下一个入队位置 */
    int  wasted_slot;  /* 恒为 1：这个实现永久占用却永不可用的那一格（写出来给 demo 看） */
} CQueueWaste;

void cqueue_w_init(CQueueWaste *q, int capacity);
void cqueue_w_destroy(CQueueWaste *q);
bool cqueue_w_enqueue(CQueueWaste *q, int value);
bool cqueue_w_dequeue(CQueueWaste *q, int *out_value);
bool cqueue_w_is_empty(const CQueueWaste *q);   /* front == rear */
bool cqueue_w_is_full(const CQueueWaste *q);    /* (rear + 1) % capacity == front */
int  cqueue_w_size(const CQueueWaste *q);       /* 用两个下标现算：(rear - front + capacity) % capacity */
void cqueue_w_print(const CQueueWaste *q, const char *label);

#endif /* CQUEUE_H */
