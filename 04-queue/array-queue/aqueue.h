#ifndef AQUEUE_H
#define AQUEUE_H

/*
 * ===================================================================
 * aqueue.h —— 朴素数组队列的「菜单」
 * ===================================================================
 * 这个文件只有声明，没有实现。把它当成饭店门口的菜单看：
 *   先知道「能点哪些菜」（有哪些操作），想知道「怎么炒的」再去翻 aqueue.c。
 *
 * 队列（queue）就一句话：**在队伍里排队，先来的人先被服务**。
 *   先进先出，英文缩写 FIFO（First In, First Out）。
 *   跟栈正好相反：栈是「最后放上去的盘子最先被拿走」。
 *
 * 本文件定义的结构体 AQueue 用「一段连续数组 + 两个下标」来实现队列：
 *   数组负责存人，front 记住「队头在第几格」，rear 记住「队尾后面的那个空位在第几格」。
 *
 * ★ 本节是**故意**写成"有毛病"的：出队只把 front 往前挪，绝不搬数据。
 *   于是 front 和 rear 只会一路向右、从不回头，
 *   用不了多久就会「后面没格子了，但前面明明空着一堆格子」——这就是**假满**。
 *   本节的全部教学目标就是让你在 main.c 的输出里亲眼看到它发生，
 *   然后带着"这毛病怎么治"去读下一节 04-queue/circular-queue/（答案：取模，让下标绕回 0）。
 *
 * ⚠ front / rear 的约定（本项目选的流派，全节自洽）：
 *   · front = 队头**元素自己**的下标（跟栈的"流派 A"一样，指针指在真实元素上）
 *   · rear  = 队尾**后面那个空位**的下标（也就是"下一个新人该站哪儿"，半开区间的右端）
 *   · 元素住在 [front, rear) 这个左闭右开区间里，个数 = rear - front
 *   · 判空 = (front == rear)，判满 = (rear == capacity)
 *   为什么 rear 用"开区间"而不像栈的 top 那样"闭区间"？
 *     因为队列空的时候 front 和 rear 要能重合在同一个位置（front == rear 就表示没人），
 *     如果 rear 也指在真实元素上，那"只有一个元素"和"空队列"就没法用两个下标区分了。
 *     下一节的循环队列、双端队列都沿用这套约定，这样五节能对照着看。
 */

#include <stdbool.h>   /* C11 标准库，提供 bool / true / false，纯粹为了让代码读起来像人话 */

/*
 * 队列的全部家当。前 4 个字段是"真家伙"，后 4 个是为了把假满现场数给你看而加的计数器。
 * 数组在堆上（malloc 来的），结构体本身放哪由调用者决定（demo 里放栈上）。
 */
typedef struct {
    int *data;             /* 指向那排格子所在的连续内存；data[0] 是最早分配到的位置 */
    int  capacity;         /* 当前最多有几个格子 = data 数组的长度 */
    int  front;            /* 队头元素的下标；出队就从这里走，front++ */
    int  rear;             /* 队尾后面那个空位（= 下一个入队位置）；入队就写这里，rear++ */

    /* ---- 下面 4 个只是统计量，用来在演示里把"浪费了多少格子"讲清楚，不影响队列内容 ---- */
    int  total_push;       /* 一共入队过多少次 */
    int  total_pop;        /* 一共出队过多少次 */
    int  reject_by_fake;   /* 有多少次是"假满"被拒的：明明有空位，却因为 rear 顶到墙了 */
    bool broken_by_design; /* 就是个提醒标记：true = 这个实现天生会假满（本节永远是 true）*/
} AQueue;

/* ---------- 生命周期 ---------- */

/*
 * 初始化一个空队列：malloc capacity 个 int，front 和 rear 都置 0（队伍空、两指针重合在最左边）。
 * 申请失败时把 capacity 置 0、data 置 NULL，后续所有操作都会安全失败，不会解引用空指针。
 */
void aqueue_init(AQueue *q, int capacity);

/*
 * 销毁队列，把 malloc 来的数组还回去，并把结构体复位成"空的、没内存"的状态。
 * 谁 malloc 谁 free：这里 free 掉 aqueue_init 申请的那块内存，避免内存泄漏。
 */
void aqueue_destroy(AQueue *q);

/* ---------- 核心操作 ---------- */

/*
 * enqueue / push_back：入队，站到队尾。
 * 成功返回 true；rear 已经顶到数组右墙时返回 false 并打印原因 ——
 * ⚠ 这个 false 很可能就是"假满"（前面明明有空位），函数会顺手把这种情况指出来。
 */
bool aqueue_enqueue(AQueue *q, int value);

/*
 * dequeue / pop_front：出队，让队头那个人走掉。
 *   元素值通过 out_value 这个"输出参数"带回去（C 只能 return 一个值）。
 *   队空时返回 false，并且不写 out_value。
 *   out_value 允许传 NULL，表示"我只想让他走，不关心他是几"。
 * ⚠ 注意这里只 front++，**不把后面的元素往前搬** —— 这正是本节刻意的写法，
 *    搬数据就退化成"每次出队 O(n)"，那不叫队列了；不搬数据才有假满，才有下一节的取模。
 */
bool aqueue_dequeue(AQueue *q, int *out_value);

/*
 * 看一眼队头但**不**让他走。队空返回 false。
 * 为什么单独需要它：很多场景只想知道"现在该服务谁"，还不能把他弹出（BFS 判重、调度器预览）。
 */
bool aqueue_peek_front(const AQueue *q, int *out_value);

/*
 * 看一眼队尾（最新来的那个）。队空返回 false。
 * 提醒：本节的 rear 是"队尾后面的空位"，所以队尾元素在 data[rear - 1]，别写成 data[rear]。
 */
bool aqueue_peek_rear(const AQueue *q, int *out_value);

/* ---------- 查询 ---------- */

bool aqueue_is_empty(const AQueue *q);   /* front == rear，一个人都没有 */

/*
 * "真满"还是"假满"？
 *   · rear == capacity        → 报"满"（本节就是靠这个分支制造假满）
 *   · 但同时 front > 0        → 这就是【假满】：前面有 front 个格子空着，其实塞得下
 * 这两个函数分开给你用，方便把"容器真的没空间了"和"下标到墙了"这两件事区分开。
 */
bool aqueue_is_full(const AQueue *q);        /* 只看 rear 是不是顶到右墙 */
bool aqueue_is_fake_full(const AQueue *q);   /* 顶到右墙【而且】前面还有空位 = 假满 */

int aqueue_size(const AQueue *q);        /* 当前几个人 = rear - front */
int aqueue_capacity(const AQueue *q);    /* 一共几格 */
int aqueue_wasted_cells(const AQueue *q);/* 被"走过就废弃"的前缀占掉的格子数 = front */

/* ---------- 演示辅助 ---------- */

/*
 * 把整排格子一行打印出来：格子内容 + 下面一行标出 front / rear 落在哪。
 * 这不是队列的正式操作 —— 真正的队列只准碰两头，这里是为了教学才全摊开，
 * 让你在输出里亲眼看到 front 和 rear 一起往右爬、左边留出一串废弃格子。
 */
void aqueue_print(const AQueue *q, const char *label);

/*
 * 把"走过的路"复述一遍：入队 N 次、出队 M 次、当前 size、被假满拒绝了 K 次。
 * 纯打印辅助，方便 main.c 每一幕结束时给个小结。
 */
void aqueue_stats(const AQueue *q);

/*
 * 治假满的"笨办法"：把所有还活着的元素整体往 data[0] 挪一遍，于是 rear 又能往右走了。
 * 本节会调用它一次，让你看清两件事：
 *   ① 它确实能救急（假满立刻消失）；
 *   ② 它每次都要搬 size 个元素，出队本来的 O(1) 被拖成了 O(n) ——
 *      所以工程上没人这么干，正确答案是下一节的取模绕圈。
 * 返回搬动了多少个元素。
 */
int aqueue_compact(AQueue *q);

#endif /* AQUEUE_H */
