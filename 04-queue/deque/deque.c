#include <stdio.h>
#include <stdlib.h>   /* malloc / free */
#include "deque.h"

/*
 * ===================================================================
 * deque.c —— 双端队列（循环数组版）：四端都 O(1)
 * ===================================================================
 * 先看内存长什么样。capacity = 6，依次做了：
 *   push_back(20)、push_back(30)、push_front(10)、push_back(40)
 * 结果（元素逻辑顺序 10 → 20 → 30 → 40）：
 *
 *     下标:    0      1      2      3      4      5
 *            ┌──────┬──────┬──────┬──────┬──────┬──────┐
 *     data → │  20  │  30  │  40  │  ·   │  ·   │  10  │
 *            └──────┴──────┴──────┴──────┴──────┴──────┘
 *                                             ▲         ▲
 *                                            rear      front
 *                                          (下一格    (10 是从头插进来的，
 *                                           写 40 后   push_front 让 front 从 0
 *                                           它到了 2)   倒退到 5：(0-1+6)%6 = 5)
 *
 *   逻辑顺序（头→尾）沿环从 front 走 size 步：
 *        data[5]=10 → data[0]=20 → data[1]=30 → data[2]=40
 *   ⚠ 下标顺序完全靠后，别拿 data[0] 当"第一个元素"，也别比大小判断先后。
 *
 *   两个指针各自会走四个方向：
 *        push_front ：front 逆时针退 1 → (front - 1 + cap) % cap   ← 本节最难的一行
 *        pop_front  ：front 顺时针进 1 → (front + 1) % cap
 *        push_back  ：rear  顺时针进 1 → (rear + 1) % cap
 *        pop_back   ：rear  逆时针退 1 → (rear - 1 + cap) % cap
 *   合起来就是：front 管"头"，rear 管"尾"，各自沿环正反挪一格，
 *   每步都只改一个下标 + 写/读一格 → 四端操作都是严格的 O(1)。
 *
 *   ★ 为什么倒退一定要 "+ capacity"？因为 C 的 % 是【向零截断】：
 *        (-1) % 6 = -1（数学上期望的是 5）；(-7) % 6 = -1（不是 5）。
 *        所以 front==0 时 (front-1)%cap 得到 -1，接着 data[-1] = v 就是数组前面越界写，
 *        ASan 报 heap-buffer-overflow（而且往往当场不崩，隔几步才炸，最难查）。
 *        抬正的做法：(x - 1 + capacity) % capacity，一次 +cap 就够，因为
 *        x ∈ [0, cap-1] ⇒ x-1+cap ∈ [cap-1, 2cap-2]，必然非负且 < 2cap。
 *        （如果一次要退 k 步、k 可能大于 cap，就得写成 ((x % cap) + cap) % cap 那种通用式。）
 */

/*
 * -------------------------------------------------------------------
 * 初始化
 * -------------------------------------------------------------------
 * front = rear = size = 0：两个指针重合并都指"下一个要写的位置"，
 * size=0 让"空"这件事有一个明确的、不会和"满"混淆的判据。
 * malloc 失败就建空壳：后续所有操作先查 data == NULL，安全失败，绝不解引用空指针。
 * -------------------------------------------------------------------
 */
void deque_init(Deque *d, int capacity)
{
    if (d == NULL) {
        return;
    }

    d->data             = NULL;
    d->capacity         = 0;
    d->front            = 0;
    d->rear             = 0;
    d->size             = 0;
    d->push_front_count = 0;
    d->push_back_count  = 0;
    d->pop_front_count  = 0;
    d->pop_back_count   = 0;

    if (capacity <= 0) {
        printf("      [警告] deque_init 收到非法容量 %d，建成空壳，四端操作都会失败\n", capacity);
        return;
    }

    d->data = malloc((size_t)capacity * sizeof(int));   /* 单位是字节：个数 × sizeof(int) */
    if (d->data == NULL) {
        printf("      [错误] malloc %d 个 int 失败，双端队列建成空壳\n", capacity);
        return;
    }

    d->capacity = capacity;
    /* 环上的格子初始是残留垃圾，规矩不变：只有从 front 出发走 size 步经过的格子才算数。 */
}

/* 销毁：free 一次（只有这一整块内存），然后把所有字段复位。重复 destroy 时 free(NULL) 合法 */
void deque_destroy(Deque *d)
{
    if (d == NULL) {
        return;
    }
    free(d->data);
    d->data     = NULL;   /* 撕掉纸条：再误用会当场崩，而不是悄悄读到垃圾 */
    d->capacity = 0;
    d->front    = 0;
    d->rear     = 0;
    d->size     = 0;
}

/*
 * -------------------------------------------------------------------
 * 内部小工具：环上某个下标此刻是不是"还在队里"
 * -------------------------------------------------------------------
 * 判据：(i - front + capacity) % capacity < size
 *   意思是"从 front 沿环往前数，i 排在第 (i-front+cap)%cap 个位置，
 *   只要这个位置编号小于 size，它就还活着"。
 * 为什么不能用 front <= i < rear？绕圈之后 rear 会比 front 小，判断直接错。
 * （这个式子里又一次出现 +capacity 抬正 —— 全节都在用同一个技巧。）
 * -------------------------------------------------------------------
 */
static bool deque_alive(const Deque *d, int i)
{
    if (d == NULL || d->data == NULL || d->capacity <= 0) {
        return false;
    }
    return (i - d->front + d->capacity) % d->capacity < d->size;
}

/*
 * -------------------------------------------------------------------
 * push_back：尾部入队（= 队列的 enqueue / 和循环队列完全一样）
 * -------------------------------------------------------------------
 * ① 判满：size == capacity（跟 rear 在不在最后一格【无关】）
 * ② data[rear] = v      ← rear 指的正是那个空位
 * ③ rear = (rear + 1) % capacity
 * ④ size++
 * -------------------------------------------------------------------
 */
bool deque_push_back(Deque *d, int value)
{
    if (d == NULL || d->data == NULL) {
        printf("      [失败] push_back(%d)：没有可用内存\n", value);
        return false;
    }
    if (d->size == d->capacity) {
        printf("      [失败] push_back(%d)：【真满】size=%d == capacity=%d，"
               "环上一个空位都没有（判的是 size，不是 rear 顶没顶墙）\n",
               value, d->size, d->capacity);
        return false;
    }

    d->data[d->rear] = value;                       /* ② 写在"下一个位置"上 */
    d->rear = (d->rear + 1) % d->capacity;          /* ③ 前进一格：加法不用 +capacity 抬正 */
    d->size += 1;
    d->push_back_count += 1;
    if (d->rear == 0) {
        printf("      ↻ rear 走完最后一格，取模绕回 0（( %d + 1 ) %% %d = 0）\n",
               d->capacity - 1, d->capacity);
    }
    return true;
}

/*
 * -------------------------------------------------------------------
 * push_front：头部入队 —— ★★ 本节的主角 ★★
 * -------------------------------------------------------------------
 * 一步步：
 *   ① 判满（同上，size == capacity 就拒绝）
 *   ② front = (front - 1 + capacity) % capacity   ← 先让队头指针【逆时针退一格】
 *      为什么要先退？因为 front 指的就是队头元素自己，
 *      新元素要站在它【前面】，所以那个空位是 front 的上一格。
 *      ⚠ 这里绝不能写成 (front - 1) % capacity：front==0 时它等于 -1，
 *        下一步 data[-1] = value 就是往数组左边越界写内存。
 *   ③ data[front] = value
 *   ④ size++
 *
 * 现场演算（capacity = 6）：
 *   front=0 → (0-1+6)%6 = 5%6 = 5   ✓ 绕到最后一格
 *   front=5 → (5-1+6)%6 = 10%6 = 4  ✓ 正常倒退
 *   front=1 → (1-1+6)%6 = 6%6   = 0 ✓ 回到开头
 * 顺序死规矩：② 和 ③ 不能颠倒。先写后挪，就会把老队头覆盖掉，整个队少一个人。
 * -------------------------------------------------------------------
 */
bool deque_push_front(Deque *d, int value)
{
    if (d == NULL || d->data == NULL) {
        printf("      [失败] push_front(%d)：没有可用内存\n", value);
        return false;
    }
    if (d->size == d->capacity) {
        printf("      [失败] push_front(%d)：【真满】size=%d == capacity=%d\n",
               value, d->size, d->capacity);
        return false;
    }

    int old_front = d->front;
    d->front = (d->front - 1 + d->capacity) % d->capacity;   /* ② ★ 先 +capacity 再取模 ★ */
    if (old_front == 0) {
        printf("      ↺ 倒退绕圈：front 本来是 0，( 0 - 1 + %d ) %% %d = %d → 落到最后一格。"
               "要是写 ( 0 - 1 ) %% %d 得到 %d，data[%d] 就是越界写（ASan 会当场抓住）\n",
               d->capacity, d->capacity, d->front, d->capacity,
               (0 - 1) % d->capacity, (0 - 1) % d->capacity);
    }

    d->data[d->front] = value;      /* ③ 再往腾出来的那一格写值 */
    d->size += 1;
    d->push_front_count += 1;
    return true;
}

/*
 * -------------------------------------------------------------------
 * pop_front：头部出队（= 队列的 dequeue）
 * -------------------------------------------------------------------
 * ① 判空（size == 0）
 * ② v = data[front]         ← front 指着真实元素，所以【先读】
 * ③ front = (front + 1) % capacity   ← 加法不会出负数，直接取模就安全
 * ④ size--
 * -------------------------------------------------------------------
 */
bool deque_pop_front(Deque *d, int *out_value)
{
    if (d == NULL || d->data == NULL) {
        printf("      [失败] pop_front：没有可用内存\n");
        return false;
    }
    if (d->size == 0) {
        printf("      [失败] pop_front：队列是空的（front=%d, rear=%d 重合）\n",
               d->front, d->rear);
        return false;
    }

    int v = d->data[d->front];                    /* ② 先读 */
    d->front = (d->front + 1) % d->capacity;      /* ③ 前进一格 */
    if (d->front == 0) {
        printf("      ↻ front 走完最后一格，取模绕回 0\n");
    }
    d->size -= 1;
    d->pop_front_count += 1;

    if (out_value != NULL) {
        *out_value = v;
    }
    return true;
}

/*
 * -------------------------------------------------------------------
 * pop_back：尾部出队（= 栈的 pop！）
 * -------------------------------------------------------------------
 * ① 判空
 * ② rear = (rear - 1 + capacity) % capacity   ← 先让 rear【倒退一格】
 * ③ v = data[rear]                             ← 再读，读到的才是队尾元素
 * ④ size--
 *
 * ⚠ 顺序和 pop_front 正好相反：
 *     pop_front 先读后挪，因为 front 指在真实元素上；
 *     pop_back  先挪后读，因为 rear 指的是"开区间右端"的空位，
 *                 队尾元素在它的【前一格】。
 *   这两个"谁先谁后"搞混，是循环数组实现里最常见的 off-by-one bug，
 *   症状通常是"弹出的值比预期慢一个"或者"第一个/最后一个元素永远弹不出来"。
 * -------------------------------------------------------------------
 */
bool deque_pop_back(Deque *d, int *out_value)
{
    if (d == NULL || d->data == NULL) {
        printf("      [失败] pop_back：没有可用内存\n");
        return false;
    }
    if (d->size == 0) {
        printf("      [失败] pop_back：队列是空的（front=%d, rear=%d 重合）\n",
               d->front, d->rear);
        return false;
    }

    int old_rear = d->rear;
    d->rear = (d->rear - 1 + d->capacity) % d->capacity;   /* ② ★ 又一次 +capacity 抬正 ★ */
    if (old_rear == 0) {
        printf("      ↺ 倒退绕圈：rear 本来是 0，( 0 - 1 + %d ) %% %d = %d → 落到最后一格\n",
               d->capacity, d->capacity, d->rear);
    }

    int v = d->data[d->rear];      /* ③ 先挪后读 */
    d->size -= 1;
    d->pop_back_count += 1;

    if (out_value != NULL) {
        *out_value = v;
    }
    return true;
}

/*
 * -------------------------------------------------------------------
 * peek_front / peek_back：只看一头，不删
 * -------------------------------------------------------------------
 * 和各自的 pop 用完全相同的位置计算，只是不挪指针、不改 size，所以可以反复调。
 * peek_back 又要算"rear 的前一格"，于是再抄一遍 +capacity 的式子。
 * 参数里的 const：承诺只读，误写编译器立刻报错。
 * -------------------------------------------------------------------
 */
bool deque_peek_front(const Deque *d, int *out_value)
{
    if (d == NULL || d->data == NULL || d->size == 0) {
        printf("      [失败] peek_front：空的，没有队头可看\n");
        return false;
    }
    if (out_value != NULL) {
        *out_value = d->data[d->front];
    }
    return true;
}

bool deque_peek_back(const Deque *d, int *out_value)
{
    if (d == NULL || d->data == NULL || d->size == 0) {
        printf("      [失败] peek_back：空的，没有队尾可看\n");
        return false;
    }
    int tail = (d->rear - 1 + d->capacity) % d->capacity;
    if (out_value != NULL) {
        *out_value = d->data[tail];
    }
    return true;
}

/* ---------- 查询：全 O(1)，一个都不许遍历 ---------- */

bool deque_is_empty(const Deque *d)
{
    if (d == NULL) {
        return true;
    }
    return d->size == 0;    /* 千万别写 front == rear：绕圈装满时它也成立（那是满，不是空）*/
}

bool deque_is_full(const Deque *d)
{
    if (d == NULL) {
        return true;
    }
    return d->size == d->capacity;
}

int deque_size(const Deque *d)
{
    return (d == NULL) ? 0 : d->size;
}

int deque_capacity(const Deque *d)
{
    return (d == NULL) ? 0 : d->capacity;
}

/*
 * -------------------------------------------------------------------
 * 教学打印：把环摊平成一条数组
 * -------------------------------------------------------------------
 * 输出三行：
 *   第 1 行：每个格子的「下标 : 值」，不在队里的格子打 ·
 *   第 2 行：同样的循环，只在 front/rear 那两格写字母 F/R（重合时写 FR）
 *   第 3 行：沿环走出来的【逻辑顺序】，这是最重要的一行 ——
 *            它告诉你"从头到尾"到底是哪几个格子，跟下标大小没关系。
 * 用同一个循环、同一格宽（` [%2d:%3s]`，纯 ASCII）生成，箭头标记就不会被中文宽度顶歪。
 * -------------------------------------------------------------------
 */
void deque_print(const Deque *d, const char *label)
{
    if (d == NULL) {
        printf("  (空指针，没有双端队列可打印)\n");
        return;
    }

    printf("  %s  size=%d, front=%d, rear=%d, capacity=%d | 统计 pushF=%d pushB=%d popF=%d popB=%d\n",
           label, d->size, d->front, d->rear, d->capacity,
           d->push_front_count, d->push_back_count, d->pop_front_count, d->pop_back_count);

    if (d->capacity <= 0) {
        printf("    （这个双端队列没有分到内存）\n");
        return;
    }

    printf("    ");
    for (int i = 0; i < d->capacity; i++) {
        if (deque_alive(d, i)) {
            printf(" [%2d:%3d]", i, d->data[i]);
        } else {
            printf(" [%2d:  ·]", i);
        }
    }
    printf("   ← 下标 0..%d 当成一个圈（%d 的右边接回 0）\n",
           d->capacity - 1, d->capacity - 1);

    printf("    ");
    for (int i = 0; i < d->capacity; i++) {
        const char *mark = " ";
        bool is_f = (i == d->front);
        bool is_r = (i == d->rear);
        if (is_f && is_r) {
            mark = "FR";
        } else if (is_f) {
            mark = "F";
        } else if (is_r) {
            mark = "R";
        }
        printf(" [%2d:%3s]", i, mark);
    }
    printf("   ← F=front(队头元素) R=rear(push_back 的写入位)");
    if (d->size == d->capacity) {
        printf("   ⚠ F 和 R 重合，但这是【满】不是空（size=%d）—— 全靠 size 字段区分",
               d->size);
    } else if (d->size == 0) {
        printf("   F 和 R 重合 = 空（size=0）");
    }
    printf("\n");

    /* 第三行：逻辑顺序。从左到右 = 从队头到队尾 */
    printf("    逻辑顺序（头→尾）：");
    if (d->size == 0) {
        printf("（空，front==rear==%d，弧长 0）", d->front);
    } else {
        for (int k = 0; k < d->size; k++) {
            int idx = (d->front + k) % d->capacity;
            printf("data[%d]=%d", idx, d->data[idx]);
            if (k + 1 < d->size) {
                printf(" → ");
            }
        }
    }
    printf("\n");
}

/* 纯打印辅助：告诉看 demo 的人"你现在这串操作相当于在用哪种结构" */
void deque_usage_hint(const char *mode)
{
    printf("  【%s】\n", mode);
}
