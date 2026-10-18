#ifndef DEQUE_H
#define DEQUE_H

/*
 * ===================================================================
 * deque.h —— 双端队列的「菜单」
 * ===================================================================
 * 双端队列（deque = double-ended queue，读作 "deck"）：
 *   **两头都能进、两头都能出**。
 *   栈 = 只开一个口；队列 = 开两个口但各管一件事（一头进一头出）；
 *   双端队列 = 两个口都随便用 → 它是栈和队列的"共同爹"：
 *     · 只用 push_back + pop_back      → 它就是【栈】
 *     · 只用 push_back + pop_front     → 它就是【队列】
 *     · 四种混着用                    → 单调队列、滑动窗口、撤销+重做、任务窃取
 *
 * 本节用【循环数组】实现，也就是沿用 04-queue/circular-queue 的那套技巧：
 *   一整块连续内存 + front/rear 两个下标 + size 计数，所有移动都取模。
 *   这样四端操作全是 O(1)，而且内存紧凑、cache 友好。
 *
 * ★★ 本节最要紧的一个技术点：push_front 时 front 要【倒退】，
 *    倒退的写法是 (front - 1 + capacity) % capacity，必须先加 capacity 再取模。★★
 * 为什么不能直接写 (front - 1) % capacity？
 *   因为 C 语言的取模是【向零截断】，不是数学上的同余：
 *       front = 0, capacity = 4  →  (0 - 1) % 4 = -1   ← 负数！
 *       于是 data[-1] = v 就是往数组前面越界写（ASan 立刻报 heap-buffer-overflow）。
 *   加一个 capacity 把被减数抬到非负，再取模就永远落在 0..capacity-1：
 *       (0 - 1 + 4) % 4 = 3 % 4 = 3  ✓（回到环的最后那一格）
 *   pop_back 里的 rear 倒退、peek_back 里的"队尾前一格"，全都是同一个套路。
 *
 * front / rear 约定（和数组队列、循环队列完全一致，方便五节对照）：
 *   · front = 队头元素的下标
 *   · rear  = 「下一次 push_back 要写的位置」（半开区间 [front, rear) 的右端）
 *   · 元素住在环上从 front 出发、走 size 步的那段弧里；size == 0 就是空
 *   · 判满 = size == capacity（用 size 计数，不留路标格，容量不打折）
 */

#include <stdbool.h>

typedef struct {
    int *data;       /* 环：一整块连续内存，被当成首尾相接的圈来用 */
    int  capacity;   /* 环上一共几个格子（= 数组长度） */
    int  front;      /* 队头元素下标；push_front 会把它倒退，pop_front 会把它前进 */
    int  rear;       /* push_back 的写入位置；pop_back 会把它倒退，push_back 会把它前进 */
    int  size;       /* 当前元素个数：判空判满都靠它，一格不浪费 */

    int  push_front_count;  /* 统计：往头端塞了几次（就是"倒退 front"发生了多少次）*/
    int  push_back_count;   /* 统计：往尾端塞了几次 */
    int  pop_front_count;   /* 统计：从头端摘了几次 */
    int  pop_back_count;    /* 统计：从尾端摘了几次 —— 用栈模式时这个数会很大 */
} Deque;

/* ---------- 生命周期 ---------- */

/* 初始化：malloc capacity 个格子，front=rear=size=0。失败则建空壳，后续操作安全失败 */
void deque_init(Deque *d, int capacity);

/* 销毁：free 数组 + 指针置 NULL + 字段复位（谁 malloc 谁 free，demo 结尾必须调） */
void deque_destroy(Deque *d);

/* ---------- 四端操作（全部 O(1)）---------- */

/*
 * 尾部入队：data[rear] = v; rear = (rear + 1) % capacity; size++
 * 和循环队列的 enqueue 一模一样，没有任何新东西。
 */
bool deque_push_back(Deque *d, int value);

/*
 * 头部入队：front = (front - 1 + capacity) % capacity; data[front] = v; size++
 * ⚠★ 本节的主角：front 要【逆时针退一格】，所以必须先 +capacity 再取模。
 *    顺序也很关键：必须【先退再写】—— 因为 front 指的就是队头元素自己，
 *    要插到它前面，得先把指针挪到那个空位上，再写值。
 */
bool deque_push_front(Deque *d, int value);

/* 头部出队：v = data[front]; front = (front + 1) % capacity; size--（队头是真实元素，先读后挪）*/
bool deque_pop_front(Deque *d, int *out_value);

/*
 * 尾部出队：rear = (rear - 1 + capacity) % capacity; v = data[rear]; size--
 * ⚠ rear 是「开区间右端」，指向的是空位，所以必须【先退再读】，
 *    直接读 data[rear] 会读到一个不属于队列的格子。这里又有一次 +capacity。
 */
bool deque_pop_back(Deque *d, int *out_value);

/* 看两头但不删：队头 data[front]、队尾 data[(rear-1+capacity)%capacity] */
bool deque_peek_front(const Deque *d, int *out_value);
bool deque_peek_back(const Deque *d, int *out_value);

/* ---------- 查询 ---------- */

bool deque_is_empty(const Deque *d);   /* size == 0 */
bool deque_is_full(const Deque *d);    /* size == capacity */
int  deque_size(const Deque *d);       /* O(1)，不用遍历 */
int  deque_capacity(const Deque *d);

/* ---------- 教学辅助 ---------- */

/* 把环摊平打印：每格内容 + 下面一行标 F/R + 第三行给出「从头到尾」的逻辑顺序 */
void deque_print(const Deque *d, const char *label);

/*
 * 一句话把四种用法点出来（纯打印辅助，不动数据）：
 * 让 demo 能在每种模式下提示"你现在用的是栈/队列/双端"。
 */
void deque_usage_hint(const char *mode);

#endif /* DEQUE_H */
