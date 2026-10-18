#include <stdio.h>
#include <stdlib.h>   /* malloc / free */
#include "aqueue.h"

/*
 * ===================================================================
 * aqueue.c —— 朴素数组队列的实现（天生会假满的那一版）
 * ===================================================================
 * 先看一眼内存长什么样，后面每个函数都对着这张图解释：
 *
 *   capacity = 6，依次入队 10、20、30，然后出队两次之后：
 *
 *     下标:     0      1      2      3      4      5
 *            ┌──────┬──────┬──────┬──────┬──────┬──────┐
 *     data → │  10  │  20  │  30  │  ·   │  ·   │  ·   │   ← 一整块连续内存，在堆上
 *            └──────┴──────┴──────┴──────┴──────┴──────┘
 *              ↑                  ↑
 *             front=2            rear=3
 *              │                  └─ 下一个新人站这里（rear 是"开区间"右端）
 *              └─ 队头元素 30 在这儿；10、20 已经出队走了
 *
 *     活着的人群住在 [front, rear) = [2, 3) 这个左闭右开区间里，人数 = rear - front = 1。
 *     第 0、1 格里 10、20 的数值【还留在内存里】，只是 front 越过了它们 → 作废、不算数。
 *
 *   继续入队 40、50、60、70：rear 一路 3→4→5→6，而 capacity=6，
 *   于是 rear == capacity，再进人就要越界了 → 报"满"。
 *   可是第 0、1 格明明是空的！**这就是假满**：
 *     容器有空间，但"下标只会往右、不会回头"这个实现方式让它用不上。
 *   栈不会有这个问题：栈的 top 单向前进、弹栈就往回退，两端是同向的；
 *   队列把入口和出口开在了两端，出口那头（front）一路往前推，就把推过的格子全浪费了。
 *
 *   治法有两条：
 *     ① 笨办法 aqueue_compact：把活着的人整体往左搬回 data[0]，rear 归位 —— 出队变 O(n)。
 *     ② 正确做法（下一节 circular-queue）：下标到墙就绕回 0，即 (rear + 1) % capacity。
 */

/*
 * -------------------------------------------------------------------
 * 初始化
 * -------------------------------------------------------------------
 * 为什么不让 aqueue_init 返回一个 AQueue，而是传指针进来填？
 *   跟数组栈一样：结构体里有指针字段，返回一份拷贝容易让人误以为是两块独立内存，
 *   传指针「就地初始化」是 C 里最朴素的写法。
 *
 * 关键：front = rear = 0，两者重合就表示"队伍是空的"。
 *   这是半开区间 [front, rear) 的自然结果 —— 区间长度 0 时两个端点必然相等。
 * -------------------------------------------------------------------
 */
void aqueue_init(AQueue *q, int capacity)
{
    if (q == NULL) {
        return;                     /* 防御：传了空指针就什么都不做，绝不写内存 */
    }

    /* 先复位所有字段，避免后面提前 return 时留下"半初始化"的脏结构体 */
    q->data       = NULL;
    q->capacity   = 0;
    q->front      = 0;
    q->rear       = 0;
    q->total_push = 0;
    q->total_pop  = 0;
    q->reject_by_fake = 0;
    q->broken_by_design = true;      /* 本节实现天生会假满，这个标记永远是 true，用来提醒 */

    if (capacity <= 0) {
        printf("      [警告] aqueue_init 收到非法容量 %d，先建成空壳，enqueue 会一直失败\n",
               capacity);
        return;
    }

    q->data = malloc((size_t)capacity * sizeof(int));   /* 单位是字节：个数 × sizeof(int) */
    if (q->data == NULL) {
        printf("      [错误] malloc %d 个 int 失败，队列建成空壳（capacity 保持 0）\n", capacity);
        return;
    }

    q->capacity = capacity;
    /* malloc 的格子里是残留垃圾值，我们不读它就没事：
       规矩是"只有 [front, rear) 区间内的格子才算队列里的元素"。 */
}

/*
 * -------------------------------------------------------------------
 * 销毁
 * -------------------------------------------------------------------
 * free 之后把指针置 NULL、下标复位。
 * 这样即使 main 里手滑又调一次 destroy，free(NULL) 是合法空操作，不会 double free。
 * （`make asan` 能专门抓 double free，跑一遍就知道这条规矩多重要。）
 * -------------------------------------------------------------------
 */
void aqueue_destroy(AQueue *q)
{
    if (q == NULL) {
        return;
    }
    free(q->data);
    q->data     = NULL;    /* 撕掉纸条，防止拿旧地址去买菜（悬空指针） */
    q->capacity = 0;
    q->front    = 0;
    q->rear     = 0;
}

/*
 * -------------------------------------------------------------------
 * enqueue：入队（唯一能往队列里放东西的口子）
 * -------------------------------------------------------------------
 * 一步步：
 *   ① 检查有没有内存（data == NULL 直接失败，绝不能写）。
 *   ② 检查 rear 是否已经顶到右墙：rear == capacity。
 *      顶到了 → 打印原因；如果此时 front > 0，明确告诉你是【假满】并计数。
 *      ⚠ 判断顺序不能反：先判满再写，否则写完才发现越界，栈/队列的状态就永久坏了。
 *   ③ data[rear] = value；rear += 1。（先写在当前空位，再把"下一个空位"往右挪）
 *
 * 边界情况：
 *   · 空壳队列（capacity=0）：走 data == NULL 分支，安全失败。
 *   · 队列为空时 front == rear，入队写在那个重合点上，两者一起往右 → 完全正常。
 *   · 假满不是崩溃、不是越界：程序老老实实拒绝了这次写入，所以内存是安全的，
 *     只是"明明能装却说不行"——这是逻辑缺陷，不是内存缺陷。
 * -------------------------------------------------------------------
 */
bool aqueue_enqueue(AQueue *q, int value)
{
    if (q == NULL || q->data == NULL) {
        printf("      [失败] enqueue(%d)：这个队列没有可用内存（capacity=0）\n", value);
        return false;
    }

    if (q->rear >= q->capacity) {              /* 右边没有格子了 */
        if (q->front > 0) {
            q->reject_by_fake += 1;            /* 数一下被假满坑了几次 */
            printf("      [失败] enqueue(%d)：rear=%d 已经顶到墙（capacity=%d），"
                   "但 data[0..%d] 共 %d 个格子其实是空的 → ⚠ 这就是【假满】！"
                   "队列里只有 %d 个元素，远没满\n",
                   value, q->rear, q->capacity, q->front - 1, q->front, aqueue_size(q));
        } else {
            printf("      [失败] enqueue(%d)：真满了（size=%d == capacity=%d，front=0，"
                   "一个格子都没浪费）\n", value, aqueue_size(q), q->capacity);
        }
        return false;
    }

    q->data[q->rear] = value;   /* ③ 先放进 rear 指的这一格 */
    q->rear         += 1;       /*   再把 rear 指向新的空位（半开区间右端往右扩一格） */
    q->total_push   += 1;
    return true;
}

/*
 * -------------------------------------------------------------------
 * dequeue：出队（让队头那个人走掉）
 * -------------------------------------------------------------------
 * 一步步：
 *   ① 判空：front == rear 就是没人，返回 false 并且【不写脏】out_value。
 *   ② 读 data[front]，先存进局部变量 v。
 *   ③ front += 1 —— 注意：只是把"队头位置"往右挪一格，
 *      既不抹掉 data[front] 的值，也【绝不把后面的元素往前搬】。
 *   ④ 调用者要值就写进 out_value；传 NULL 表示不要。
 *
 * ★★ 本节的教学核心就在 ③ 这一行：
 *    正因为"出队只动 front、不搬数据"，front 才会一路往右把左边的格子作废，
 *    攒够次数就撞上 enqueue 的 rear >= capacity → 假满现场。
 *    如果这里改成"for 循环把所有元素往前挪一格"，就不会假满，
 *    但每次出队都是 O(size) —— 队列的灵魂（两头都 O(1)）就没了。
 *    所以正确解法不是搬数据，而是让下标绕圈 → 见 04-queue/circular-queue/。
 * -------------------------------------------------------------------
 */
bool aqueue_dequeue(AQueue *q, int *out_value)
{
    if (q == NULL || q->data == NULL) {
        printf("      [失败] dequeue：这个队列没有可用内存\n");
        return false;
    }

    if (q->front == q->rear) {         /* 空队列：两个指针重合 */
        printf("      [失败] dequeue：队列已经空了（front==rear==%d），没有人可出队\n", q->front);
        return false;
    }

    int v = q->data[q->front];   /* ② 先取值 */
    q->front += 1;               /* ③ 再挪指针；这一格从此作废，但里面的数值还留在内存里 */
    q->total_pop += 1;

    if (out_value != NULL) {     /* ④ 输出参数可以为 NULL —— C 里"一个函数带回两个值"的标准写法 */
        *out_value = v;
    }
    return true;
}

/*
 * -------------------------------------------------------------------
 * peek_front / peek_rear：只看，不让人走
 * -------------------------------------------------------------------
 * peek_front 读 data[front]（队头元素自己）。
 * peek_rear  读 data[rear - 1]（因为 rear 指的是"队尾【后面】的空位"，不是队尾本身）。
 *
 * ⚠ 这里的 rear - 1 是新手最容易踩的点：写成 data[rear] 读到的是垃圾值/作废格子。
 *   判空之后 rear 一定 > front >= 0，所以 rear - 1 至少是 0，不会越界到负数。
 *
 * 签名里的 const：承诺"我只读不改这个队列"，误写编译器立刻报错。
 * -------------------------------------------------------------------
 */
bool aqueue_peek_front(const AQueue *q, int *out_value)
{
    if (q == NULL || q->data == NULL || q->front == q->rear) {
        printf("      [失败] peek_front：队列是空的，没有队头可看\n");
        return false;
    }
    if (out_value != NULL) {
        *out_value = q->data[q->front];
    }
    return true;
}

bool aqueue_peek_rear(const AQueue *q, int *out_value)
{
    if (q == NULL || q->data == NULL || q->front == q->rear) {
        printf("      [失败] peek_rear：队列是空的，没有队尾可看\n");
        return false;
    }
    if (out_value != NULL) {
        *out_value = q->data[q->rear - 1];    /* 注意 -1：rear 是开区间右端 */
    }
    return true;
}

/* ---------- 下面几个查询函数都是一行，但每一行都值得解释 ---------- */

bool aqueue_is_empty(const AQueue *q)
{
    if (q == NULL) {
        return true;
    }
    return q->front == q->rear;   /* 半开区间 [front, rear) 长度为 0 */
}

bool aqueue_is_full(const AQueue *q)
{
    if (q == NULL) {
        return true;
    }
    return q->rear >= q->capacity;   /* 只看"右墙到了没" —— 这个判断【分不清真假满】，
                                        所以本节另外给了 is_fake_full 把它拆开讲 */
}

/*
 * 假满 = 右墙到了（rear == capacity），但左边被 front 走过的格子还能用（front > 0）。
 * 换句话说：队列里的人数根本没到 capacity，却已经无法入队了。
 * 这一行代码就是本节所有故事的起因。
 */
bool aqueue_is_fake_full(const AQueue *q)
{
    if (q == NULL || q->data == NULL) {
        return false;
    }
    return q->rear >= q->capacity && q->front > 0;
}

int aqueue_size(const AQueue *q)
{
    if (q == NULL || q->data == NULL) {
        return 0;
    }
    return q->rear - q->front;   /* 半开区间长度；空队列时正好是 0，不会出现负数 */
}

int aqueue_capacity(const AQueue *q)
{
    return (q == NULL) ? 0 : q->capacity;
}

/* 被作废的前缀格子数：front 走过的全都浪费了。size + wasted = rear，
   而真正还能用的是 capacity - rear 格 —— 这个等式一看就懂假满浪费了多少。 */
int aqueue_wasted_cells(const AQueue *q)
{
    if (q == NULL) {
        return 0;
    }
    return q->front;
}

/*
 * -------------------------------------------------------------------
 * compact：治假满的笨办法（整体左移），用来反衬"取模"有多优雅
 * -------------------------------------------------------------------
 * 一步步：
 *   ① 把 [front, rear) 这段活着的元素逐个搬到 data[0] 开始的位置；
 *   ② front = 0；rear = 搬过去的个数（也就是原来的 size）。
 * 为什么从前往后搬是安全的（源和目的区间会重叠）？
 *   因为 destination 永远 <= source（size 个元素往左移到 [0, size)），
 *   逐个"先读 data[i+front] 再写 data[i]"，读的时候那个格子还没被覆盖。
 *   反过来（从后往前搬 + 目的地在右边）才需要小心重叠。
 *
 * ⚠ 代价：一次调用搬 size 个元素，O(n)。
 *   如果你每次出队都顺手 compact 一次，队列就退化成"每次出队 O(n)"，
 *   等于把假满的钱用另一种方式付了回去。所以工程上的选择是下一节的取模绕圈。
 * -------------------------------------------------------------------
 */
int aqueue_compact(AQueue *q)
{
    if (q == NULL || q->data == NULL) {
        printf("      [跳过] compact：没有可用内存\n");
        return 0;
    }

    int moved = q->rear - q->front;    /* 活着的人数 */
    for (int i = 0; i < moved; i++) {
        q->data[i] = q->data[q->front + i];   /* 第 i 个活人搬到 data[i] */
    }

    int old_front = q->front;
    q->front = 0;
    q->rear  = moved;

    printf("      🔧 compact：把 %d 个元素整体左搬 %d 格，front %d → 0，rear %d → %d，"
           "右边腾出 %d 个空格（注意：这次搬了 %d 个元素 = O(n)，不是 O(1)）\n",
           moved, old_front, old_front, old_front + moved, moved, q->capacity - moved, moved);
    return moved;
}

/*
 * -------------------------------------------------------------------
 * 教学打印：把整排格子摊开给人看，并在下面一行标出 front / rear
 * -------------------------------------------------------------------
 * 真正的队列不提供这个操作（队列只准碰两头）。这里打印是为了让 FIFO 和假满看得见。
 *
 *   输出形如：
 *     [ 0: 10] [ 1: 20] [ 2: 30] [ 3:  ·] [ 4:  ·] [ 5:  ·]   ← 左=老，右=新
 *     [      ] [      ] [  F   ] [  R   ] [      ] [      ]   ← F=front R=rear
 *
 *   为什么第二行要用【完全相同的循环、完全相同的格子宽度】再生成一遍？
 *   因为"先打一串空格再把 ↑ 顶到第 i 格下面"这种写法，一旦前缀里有中文
 *   （中文在终端里占 2 列）就会算错列数、标记歪到隔壁格子去。
 *   用同一个循环打同一格宽的字符，宽度天然对齐，不用算列，这是最稳的办法。
 *   每格固定 9 列：` [%2d:%3s]`。
 *
 *   状态用三个记号区分：
 *     F  = front 所在格（队头元素）
 *     R  = rear  所在格（下一个入队位置，此时是空的）
 *     FR = front 和 rear 重合（空队列）
 * -------------------------------------------------------------------
 */
void aqueue_print(const AQueue *q, const char *label)
{
    if (q == NULL) {
        printf("  (空指针，没有队列可打印)\n");
        return;
    }

    printf("  %s  size=%d, front=%d, rear=%d, capacity=%d, 浪费格子=%d%s\n",
           label, aqueue_size(q), q->front, q->rear, q->capacity, aqueue_wasted_cells(q),
           aqueue_is_fake_full(q) ? "   ⚠ 假满中" : "");

    if (q->capacity <= 0) {
        printf("    （这个队列没有分到内存）\n");
        return;
    }

    /* 第一行：每个格子的「下标 : 值」；作废格子（下标 < front）打上 x 标记 */
    printf("    ");
    for (int i = 0; i < q->capacity; i++) {
        if (i < q->front) {
            printf(" [%2d:  x]", i);          /* x = 这个格子已被 front 走过，作废但值还在 */
        } else if (i < q->rear) {
            printf(" [%2d:%3d]", i, q->data[i]);  /* 活着的元素 */
        } else if (i == q->rear) {
            printf(" [%2d:  ·]", i);          /* · = 空位，rear 就停在这里等着放人 */
        } else {
            printf(" [%2d:  ·]", i);          /* 右边还没用过的空位 */
        }
    }
    printf("   ← 左=老，右=新\n");

    /* 第二行：同样的循环，只在 front / rear 那两个格子里写字母 */
    printf("    ");
    for (int i = 0; i < q->capacity; i++) {
        const char *mark = " ";
        bool is_f = (i == q->front);
        bool is_r = (i == q->rear);
        if (is_f && is_r) {
            mark = "FR";                      /* 两指针重合 = 空队列 */
        } else if (is_f) {
            mark = "F";
        } else if (is_r) {
            mark = "R";
        }
        printf(" [%2d:%3s]", i, mark);
    }
    /* rear 可能等于 capacity，也就是"指在墙外面"，这一格画不出来，得用文字补一句 */
    if (q->rear >= q->capacity) {
        printf("   ← F=front(队头元素)；rear=%d 已经在最后一格(%d)的右边 = 指向墙外，"
               "所以这一行画不出 R —— 正是「无处可放」的画面\n",
               q->rear, q->capacity - 1);
    } else {
        printf("   ← F=front(队头元素) R=rear(下一个入队位)\n");
    }
}

/*
 * -------------------------------------------------------------------
 * 统计打印：把"这一路折腾下来发生了什么"用一组数字复述一遍
 * -------------------------------------------------------------------
 */
void aqueue_stats(const AQueue *q)
{
    if (q == NULL) {
        return;
    }
    printf("    统计：累计入队 %d 次，累计出队 %d 次，当前 size=%d，"
           "其中被【假满】拒绝 %d 次（capacity=%d，front=%d 意味着 %d 个格子被白白作废）\n",
           q->total_push, q->total_pop, aqueue_size(q), q->reject_by_fake,
           q->capacity, q->front, aqueue_wasted_cells(q));
}
