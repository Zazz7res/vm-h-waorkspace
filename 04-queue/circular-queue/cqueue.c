#include <stdio.h>
#include <stdlib.h>   /* malloc / free */
#include "cqueue.h"

/*
 * ===================================================================
 * cqueue.c —— 循环队列的实现：用取模彻底消灭「假满」
 * ===================================================================
 * 先看一眼"环"长什么样。capacity = 4，方案 A（带 size 计数）：
 *
 *        ┌──────┬──────┬──────┬──────┐
 *   下标 │  0   │  1   │  2   │  3   │
 *        ├──────┼──────┼──────┼──────┤
 *   内容 │  30  │  40  │  ·   │  10  │      ← 数组在内存里仍然是【一条直线】，
 *        └──────┴──────┴──────┴──────┘        是我们把它【当成圈】来走
 *          ▲                        ▲
 *         （空位）                这里绕回来接上 0
 *
 *   front = 3（队头是 10），rear = 2（下一个新人站 data[2]），size = 3。
 *   逻辑顺序（谁先谁后）不是按下标从小到大，而是沿着环走：
 *        data[3]=10 → data[0]=30 → data[1]=40 →（data[2] 是空的，该站新人了）
 *   ★ 记住这句话：环上「先后」= 从 front 出发顺时针走到 rear，跟下标大小无关。
 *
 *   两个指针怎么动（每一步都取模）：
 *        入队：data[rear] = v;  rear = (rear + 1) % capacity;  size++;
 *        出队：v = data[front]; front = (front + 1) % capacity; size--;
 *
 *   取模为什么就是"绕圈"？（capacity = 4 的例子）
 *        下标 0 → 1 → 2 → 3 → 4%4=0 → 1 → 2 → 3 → 5%4… 永远在 0..3 里循环
 *        3 + 1 = 4，4 % 4 = 0 → 从最后一格直接回到第 0 格，这就是"绕回开头"。
 *   于是上一节的假满彻底没了：rear 顶到 3 之后不会报错，而是绕到 0 继续用空位。
 *
 *   为什么【必须】有一个额外信息（size）或者一个约定（少用一格）？
 *        空队列时：front == rear（两个指针重合，弧长 0）
 *        装满时：rear 往前走 capacity 步，正好又回到 front → 还是 front == rear！
 *        光看 front/rear 完全分不开这两种情况 —— 所以：
 *          方案 A：多记一个 size，空 = size==0，满 = size==capacity（本节主推）
 *          方案 B：故意让环上永远空着一格，
 *                  满 = (rear+1)%capacity == front，空 = front == rear（老教材写法）
 */

/*
 * -------------------------------------------------------------------
 * 内部小工具：某个下标此刻是不是「环上还活着」的格子
 * -------------------------------------------------------------------
 * 判据：从 front 出发沿环走 size 步，路上经过的下标全都活着。
 * 为什么要绕这么一圈而不写 `front <= i && i < rear`？
 *   因为绕圈之后 rear 会【小于】front（比如 front=3、rear=1），
 *   朴素的大小比较直接把整段弧切错了。这是循环队列第二个最容易踩的坑，
 *   所以这里给出一个不会出错的写法：老老实实从 front 走 size 步。
 * （工程上想省掉这层循环，可以用 (i - front + capacity) % capacity < size，
 *   一个式子搞定 —— 注释里写清含义，demo 里也演示一次。）
 * -------------------------------------------------------------------
 */
static bool cq_alive(int front, int size, int capacity, int i)
{
    if (capacity <= 0) {
        return false;
    }
    return (i - front + capacity) % capacity < size;
}

/*
 * -------------------------------------------------------------------
 * 初始化（方案 A）
 * -------------------------------------------------------------------
 * capacity 个格子 + front=rear=size=0。
 * 三个字段一起归零很关键：
 *   · front==rear 让"空"这个状态成立；
 *   · size=0 让"没绕圈时"的弧长也正好是 0（两套判据要一致，否则会出现
 *     size=0 但 front != rear 的鬼状态 —— 那是 bug 的温床）。
 * malloc 失败就建空壳，后续所有操作安全失败，不解引用空指针。
 * -------------------------------------------------------------------
 */
void cqueue_init(CQueue *q, int capacity)
{
    if (q == NULL) {
        return;
    }

    q->data        = NULL;
    q->capacity    = 0;
    q->front       = 0;
    q->rear        = 0;
    q->size        = 0;
    q->wrap_count  = 0;
    q->reject_full = 0;

    if (capacity <= 0) {
        printf("      [警告] cqueue_init 收到非法容量 %d，建成空壳，enqueue 会一直失败\n",
               capacity);
        return;
    }

    q->data = malloc((size_t)capacity * sizeof(int));
    if (q->data == NULL) {
        printf("      [错误] malloc %d 个 int 失败，队列建成空壳\n", capacity);
        return;
    }

    q->capacity = capacity;
    /* 注意：方案 A 的可用容量 = capacity（一格都不浪费），
       对比方案 B（下面 CQueueWaste）的可用容量 = capacity - 1。 */
}

/* 销毁：free + 置 NULL + 全部复位（重复 destroy 时 free(NULL) 是合法空操作） */
void cqueue_destroy(CQueue *q)
{
    if (q == NULL) {
        return;
    }
    free(q->data);
    q->data     = NULL;   /* 撕掉纸条：再误用会当场崩，而不是悄悄读到垃圾 */
    q->capacity = 0;
    q->front    = 0;
    q->rear     = 0;
    q->size     = 0;
}

/*
 * -------------------------------------------------------------------
 * 入队（方案 A）
 * -------------------------------------------------------------------
 * 一步步：
 *   ① 有内存吗（data == NULL → 失败）
 *   ② 真满了吗：size == capacity —— ⚠ 判的是【人数】，不是"rear 有没有顶到墙"。
 *      就这一句改动，把上一节的假满彻底消灭了：rear 顶到最后一格不算满，
 *      因为它下一步就绕回 0 去了。
 *   ③ 写 data[rear] = value
 *   ④ rear = (rear + 1) % capacity   ← ★★ 本节的主角：取模绕圈 ★★
 *   ⑤ size++
 *
 * 边界情况：
 *   · size == capacity：真满，返回 false，不动任何状态。
 *   · rear == capacity - 1 时入队：写最后一格，然后 rear 变成 0（绕回），
 *     这正是 demo 里要重点打印的瞬间，wrap_count 就是数这个的。
 *   · 空队列（front==rear==0）：写 data[0]，rear=1，size=1 —— 一切正常。
 * -------------------------------------------------------------------
 */
bool cqueue_enqueue(CQueue *q, int value)
{
    if (q == NULL || q->data == NULL) {
        printf("      [失败] enqueue(%d)：这个队列没有可用内存\n", value);
        return false;
    }

    if (q->size == q->capacity) {            /* 唯一的判满依据：人数 == 格子数 */
        q->reject_full += 1;
        printf("      [失败] enqueue(%d)：【真满】size=%d == capacity=%d，"
               "环上确实一个空位都没有了（注意：跟 rear 在不在最后无关）\n",
               value, q->size, q->capacity);
        return false;
    }

    q->data[q->rear] = value;                /* ③ 先放进 rear 这一格 */

    int old_rear = q->rear;
    q->rear = (q->rear + 1) % q->capacity;   /* ④ ★ 到墙就绕回 0；3+1=4，4%4=0 ★ */
    if (q->rear < old_rear) {                /* rear 变小了 = 刚刚发生了一次"绕圈" */
        q->wrap_count += 1;
        printf("      ↻ 绕圈了！rear 从最后一格 %d 迈过墙 → ( %d + 1 ) %% %d = %d，"
               "回到开头继续用空位（累计 %d 次；上一节这种时候会报假满）\n",
               old_rear, old_rear, q->capacity, q->rear, q->wrap_count);
    }
    q->size += 1;                            /* ⑤ 人数 +1（方案 A 的命根子，别漏写） */
    return true;
}

/*
 * -------------------------------------------------------------------
 * 出队（方案 A）
 * -------------------------------------------------------------------
 * 一步步：
 *   ① 判空：size == 0 → false，且不写脏 out_value
 *   ② 读 data[front] 存进局部变量
 *   ③ front = (front + 1) % capacity  ← 队头也绕圈
 *   ④ size--
 *   ⑤ 需要就把值带回（out_value 可以是 NULL）
 *
 * 对比上一节：这里 front 也是往前挪、不搬数据，但它是**挪到环上的下一格**，
 * 而不是"往右一格直到撞墙"。同一个动作，加了取模就不再产生作废格子 ——
 * 因为被 front 走过的那些格子，rear 绕回来之后还能继续用！
 * -------------------------------------------------------------------
 */
bool cqueue_dequeue(CQueue *q, int *out_value)
{
    if (q == NULL || q->data == NULL) {
        printf("      [失败] dequeue：这个队列没有可用内存\n");
        return false;
    }

    if (q->size == 0) {
        printf("      [失败] dequeue：队列是空的（size=0, front=%d, rear=%d）\n",
               q->front, q->rear);
        return false;
    }

    int v = q->data[q->front];               /* ② 先取值（挪指针之前） */

    int old_front = q->front;
    q->front = (q->front + 1) % q->capacity; /* ③ ★ 队头也绕圈 ★ */
    if (q->front < old_front) {
        q->wrap_count += 1;
        printf("      ↻ 绕圈了！front 从最后一格 %d → ( %d + 1 ) %% %d = %d，"
               "累计 %d 次\n", old_front, old_front, q->capacity, q->front, q->wrap_count);
    }
    q->size -= 1;                            /* ④ 人数 -1（和 size++ 必须成对，漏一个就全乱）*/

    if (out_value != NULL) {
        *out_value = v;
    }
    return true;
}

/*
 * -------------------------------------------------------------------
 * peek_front / peek_rear
 * -------------------------------------------------------------------
 * 队头在 data[front]，很简单。
 * 队尾在 rear 的【前一格】。这里有个 C 语言的经典坑：
 *     当 rear == 0 时，rear - 1 == -1，data[-1] 就是数组越界（ASan 立刻报警）！
 * 正确算法是先加一个 capacity 再取模，把负数变正：
 *     int tail = (q->rear - 1 + q->capacity) % q->capacity;
 *   · rear=0, capacity=4 → (0-1+4)%4 = 3%4 = 3 ✓（队尾在最后一格）
 *   · rear=2, capacity=4 → (2-1+4)%4 = 5%4   = 1 ✓
 * 为什么要"+capacity"而不是直接判断 if (rear == 0) 特殊处理？
 *   因为加一次再取模对【所有】情况都成立，一个分支都不用写；
 *   这个技巧在双端队列（04-queue/deque）里更是每天都要用，务必记牢。
 * -------------------------------------------------------------------
 */
bool cqueue_peek_front(const CQueue *q, int *out_value)
{
    if (q == NULL || q->data == NULL || q->size == 0) {
        printf("      [失败] peek_front：队列是空的，没有队头可看\n");
        return false;
    }
    if (out_value != NULL) {
        *out_value = q->data[q->front];
    }
    return true;
}

bool cqueue_peek_rear(const CQueue *q, int *out_value)
{
    if (q == NULL || q->data == NULL || q->size == 0) {
        printf("      [失败] peek_rear：队列是空的，没有队尾可看\n");
        return false;
    }
    int tail = (q->rear - 1 + q->capacity) % q->capacity;   /* 先 +capacity 防负数取模 */
    if (out_value != NULL) {
        *out_value = q->data[tail];
    }
    return true;
}

/* ---------- 查询：方案 A 里这些都只是读字段，一眼看穿 ---------- */

bool cqueue_is_empty(const CQueue *q)
{
    return (q == NULL) ? true : (q->size == 0);
}

bool cqueue_is_full(const CQueue *q)
{
    if (q == NULL) {
        return true;
    }
    return q->size == q->capacity;   /* ⚠ 千万不要写成 rear == capacity 或 front == rear */
}

int cqueue_size(const CQueue *q)
{
    return (q == NULL) ? 0 : q->size;
}

int cqueue_capacity(const CQueue *q)
{
    return (q == NULL) ? 0 : q->capacity;
}

/*
 * -------------------------------------------------------------------
 * 教学打印（方案 A）
 * -------------------------------------------------------------------
 * 把环摊平成一条数组打出来，同时给出「沿环走的逻辑顺序」，
 * 这样就能看清两件反直觉的事：
 *   ① 下标顺序和入队顺序【不一样】（绕圈之后前面的格子反而是新元素）；
 *   ② 左边那些"空位"不会再被浪费 —— rear 绕回来就会用到它们。
 *
 * 标记约定（第二行）：
 *   F  = front 所在格（队头元素）
 *   R  = rear  所在格（下一个入队位置，此刻是空的）
 *   FR = 两者重合（空队列时 front == rear）
 * 用的是「同一个循环、同一格宽」的写法，这样中文前缀不会把标记顶歪。
 * -------------------------------------------------------------------
 */
void cqueue_print(const CQueue *q, const char *label)
{
    if (q == NULL) {
        printf("  (空指针，没有队列可打印)\n");
        return;
    }

    printf("  %s  size=%d, front=%d, rear=%d, capacity=%d, 绕圈累计=%d 次\n",
           label, q->size, q->front, q->rear, q->capacity, q->wrap_count);

    if (q->capacity <= 0) {
        printf("    （这个队列没有分到内存）\n");
        return;
    }

    /* 第一行：所有格子的「下标 : 值」；不在队列里的格子打 ·（环上表示"空位"） */
    printf("    ");
    for (int i = 0; i < q->capacity; i++) {
        if (cq_alive(q->front, q->size, q->capacity, i)) {
            printf(" [%2d:%3d]", i, q->data[i]);
        } else {
            printf(" [%2d:  ·]", i);
        }
    }
    printf("   ← 下标 0..%d（把它想成一个圈，%d 的右边接回 0）\n",
           q->capacity - 1, q->capacity - 1);

    /* 第二行：标 F / R */
    printf("    ");
    for (int i = 0; i < q->capacity; i++) {
        const char *mark = " ";
        bool is_f = (i == q->front);
        bool is_r = (i == q->rear);
        if (is_f && is_r) {
            mark = "FR";
        } else if (is_f) {
            mark = "F";
        } else if (is_r) {
            mark = "R";
        }
        printf(" [%2d:%3s]", i, mark);
    }
    printf("   ← F=front(队头元素) R=rear(下一个入队位)\n");

    /* 第三行：沿环走的真实逻辑顺序，这行最能治"下标顺序 = 排队顺序"的误解 */
    printf("    环上顺序（队头→队尾）：");
    if (q->size == 0) {
        printf("（空队列，front==rear==%d，弧长 0）", q->front);
    } else {
        for (int k = 0; k < q->size; k++) {
            int idx = (q->front + k) % q->capacity;
            printf("data[%d]=%d", idx, q->data[idx]);
            if (k + 1 < q->size) {
                printf(" → ");
            }
        }
    }
    printf("\n");
}

/*
 * -------------------------------------------------------------------
 * 只画"环"的形状：一条数组 + 一根从尾回到头的箭头
 * -------------------------------------------------------------------
 * 为什么用"直线 + 回指箭头"而不是画个真圆圈？
 *   ① 内存本来就是连续的直线，圆圈只是我们脑子里的模型；把两者同时展示最诚实。
 *   ② 真圆圈要按 capacity 排版，容量一大就画不下；这种写法任意容量都能画。
 * 排版要点：每格固定 9 列（` [%2d:%3s]`，纯 ASCII 符号），对齐行用同一个循环生成，
 *          这样箭头一定落在对应格子的正下方，不会因中文字符宽度而歪掉。
 * -------------------------------------------------------------------
 */
void cqueue_ring(const CQueue *q)
{
    if (q == NULL || q->capacity <= 0) {
        printf("    （没有内存，画不出环）\n");
        return;
    }

    int cap = q->capacity;

    printf("    第 1 行是普通的连续数组，第 2 行那根箭头才是「环」的真正含义：\n");
    printf("    ");
    for (int i = 0; i < cap; i++) {
        if (cq_alive(q->front, q->size, cap, i)) {
            printf(" [%2d:%3d]", i, q->data[i]);
        } else {
            printf(" [%2d:  ·]", i);
        }
    }
    printf("   ← 内存里是一条直线（· 是空位，可能是「还没用过」也可能是「已被 front 绕过、"
           "但 rear 绕回来就能再用」）\n");

    /* 对齐行：最后一格下方画 │（出发口），第 0 格下方画 v（绕回来的落点） */
    printf("    ");
    for (int i = 0; i < cap; i++) {
        if (i == 0) {
            printf("    v    ");               /* 9 列，落在第 0 格正下方 */
        } else if (i == cap - 1) {
            printf("    |    ");               /* 9 列，落在最后一格正下方 */
        } else {
            printf("         ");               /* 9 个空格，保持同宽 */
        }
    }
    printf("   ← | 走到头，+1 之后被 %d 一取模，就落到 v 这里（回到开头）\n", cap);

    printf("    关键三行代码（本节的灵魂）：\n");
    printf("      rear = (rear + 1) %% %d;   // 例：rear=%d → ( %d + 1 ) %% %d = %d（绕回）\n",
           cap, cap - 1, cap - 1, cap, ((cap - 1) + 1) % cap);
    printf("      front = (front + 1) %% %d;  // 同理，front 永远不会越界\n", cap);
    printf("      判满 = (size == %d) 或 (rear + 1) %% %d == front（两套方案见 README）\n",
           cap, cap);
}

/* ==================================================================
   方案 B：少用一个格子版（下面这套函数完全不用 size 字段）
   ================================================================== */

/*
 * -------------------------------------------------------------------
 * 初始化（方案 B）
 * -------------------------------------------------------------------
 * capacity <= 1 直接拒绝：如果只有 1 个格子，"满"和"空"两个状态会彻底重合
 * （front == rear 且 (rear+1)%capacity == front 同时成立），方案 B 就不成立了。
 * 这也是"为什么必须浪费一格"的最硬证据 —— 浪费的那一格就是这个方案的边界。
 * -------------------------------------------------------------------
 */
void cqueue_w_init(CQueueWaste *q, int capacity)
{
    if (q == NULL) {
        return;
    }

    q->data        = NULL;
    q->capacity    = 0;
    q->front       = 0;
    q->rear        = 0;
    q->wasted_slot = 1;

    if (capacity <= 1) {
        printf("      [警告] cqueue_w_init 需要 capacity >= 2（少了 1 格约定要求"
               "至少留一个空位），收到 %d，建成空壳\n", capacity);
        return;
    }

    q->data = malloc((size_t)capacity * sizeof(int));
    if (q->data == NULL) {
        printf("      [错误] malloc %d 个 int 失败\n", capacity);
        return;
    }
    q->capacity = capacity;
    /* 可用容量是 capacity - 1 —— 记住这一点，demo 里会专门撞一次墙 */
}

void cqueue_w_destroy(CQueueWaste *q)
{
    if (q == NULL) {
        return;
    }
    free(q->data);
    q->data     = NULL;
    q->capacity = 0;
    q->front    = 0;
    q->rear     = 0;
}

/*
 * -------------------------------------------------------------------
 * 判空 / 判满 / 算人数（方案 B 的全部精髓就在这三行）
 * -------------------------------------------------------------------
 *   空：front == rear
 *   满：(rear + 1) % capacity == front      ← "rear 再走一步就撞上 front"
 *   人数：(rear - front + capacity) % capacity
 *
 * 为什么"满"不能写成 front == rear？
 *   因为装满的时候 rear 沿着环正好走了一整圈，又回到 front，也是 front == rear。
 *   两种状态在两个下标上完全重合，程序分不出来 → 必然出 bug。
 *   留一格不许站人，装满时 rear 就只能停在 front 前面一格，两个状态就分开了：
 *
 *     capacity = 4（可用只有 3 格）
 *       空：front == rear == 0
 *            [  ·  ][  ·  ][  ·  ][  ·  ]      F 和 R 重在第 0 格，弧长 0
 *       装 3 个之后：front=0, rear=3
 *            [ 10  ][ 20  ][ 30  ][  ·  ]      第 3 格（R 站的那格）就是"路标"，不许放人
 *            判满：( rear + 1 ) % 4 = (3+1)%4 = 0 == front ✓ → 到此为止
 *       如果硬要放第 4 个：写 data[3]=40，rear 变成 (3+1)%4 = 0 == front
 *            → 状态和"空队列"一模一样，程序会以为队列是空的，整环数据全丢。
 *            这就是「必须浪费一格」的全部原因 —— 不是抠门，是两个下标的信息量不够。
 * -------------------------------------------------------------------
 */
bool cqueue_w_is_empty(const CQueueWaste *q)
{
    if (q == NULL || q->data == NULL) {
        return true;
    }
    return q->front == q->rear;         /* 弧长为 0 */
}

bool cqueue_w_is_full(const CQueueWaste *q)
{
    if (q == NULL || q->data == NULL) {
        return true;
    }
    return (q->rear + 1) % q->capacity == q->front;   /* ★ 经典判满式 ★ */
}

/*
 * 人数用下标现算，这行同样有坑：rear 绕到 front 左边时 rear - front 会是负数。
 * C 语言里 -1 % 4 的结果是 -1（**向零取整**，不是数学上的同余），
 * 所以要先把被除数抬正：(rear - front + capacity) % capacity。
 *   · rear=3, front=0 → (3-0+4)%4 = 3
 *   · rear=1, front=3 → (1-3+4)%4 = 2  ✓（环上活着的确实是 data[3]、data[0]）
 */
int cqueue_w_size(const CQueueWaste *q)
{
    if (q == NULL || q->data == NULL) {
        return 0;
    }
    return (q->rear - q->front + q->capacity) % q->capacity;
}

/*
 * -------------------------------------------------------------------
 * 入队 / 出队（方案 B）
 * -------------------------------------------------------------------
 * 操作和方案 A 一模一样（写 data[rear]、rear 取模；读 data[front]、front 取模），
 * 唯一区别是**不维护 size**，靠判满式子挡人。
 * -------------------------------------------------------------------
 */
bool cqueue_w_enqueue(CQueueWaste *q, int value)
{
    if (q == NULL || q->data == NULL) {
        printf("      [失败] enqueue(%d)：没有可用内存\n", value);
        return false;
    }

    if (cqueue_w_is_full(q)) {
        printf("      [失败] enqueue(%d)：( rear+1 ) %% capacity = ( %d+1 ) %% %d = %d == front=%d"
               " → 方案 B 判满成立。capacity=%d 格但可用只有 %d 格：rear 站的第 %d 格"
               "是「路标」，永远不许放人，这就是少用一个格子的代价\n",
               value, q->rear, q->capacity, (q->rear + 1) % q->capacity, q->front,
               q->capacity, q->capacity - q->wasted_slot, q->rear);
        return false;
    }

    q->data[q->rear] = value;
    int old_rear = q->rear;
    q->rear = (q->rear + 1) % q->capacity;
    if (q->rear < old_rear) {
        printf("      ↻ 绕圈：rear %d → 0\n", old_rear);
    }
    return true;
}

bool cqueue_w_dequeue(CQueueWaste *q, int *out_value)
{
    if (q == NULL || q->data == NULL) {
        printf("      [失败] dequeue：没有可用内存\n");
        return false;
    }
    if (cqueue_w_is_empty(q)) {
        printf("      [失败] dequeue：front == rear == %d → 空队列（注意这个式子"
               "和方案 A 的 size==0 是两码事）\n", q->front);
        return false;
    }

    int v = q->data[q->front];
    int old_front = q->front;
    q->front = (q->front + 1) % q->capacity;
    if (q->front < old_front) {
        printf("      ↻ 绕圈：front %d → 0\n", old_front);
    }

    if (out_value != NULL) {
        *out_value = v;
    }
    return true;
}

/* 方案 B 的打印：额外把"那个永远不能站人的格子"用 ! 标出来 */
void cqueue_w_print(const CQueueWaste *q, const char *label)
{
    if (q == NULL) {
        printf("  (空指针)\n");
        return;
    }
    int size = cqueue_w_size(q);
    printf("  %s  size=%d（现算出来的）, front=%d, rear=%d, capacity=%d, 可用=%d（浪费 %d 格）,"
           "空=%s, 满=%s\n",
           label, size, q->front, q->rear, q->capacity,
           q->capacity > 0 ? q->capacity - q->wasted_slot : 0, q->wasted_slot,
           cqueue_w_is_empty(q) ? "true" : "false",
           cqueue_w_is_full(q) ? "true" : "false");

    if (q->capacity <= 0) {
        return;
    }

    bool full = cqueue_w_is_full(q);
    printf("    ");
    for (int i = 0; i < q->capacity; i++) {
        bool alive = cq_alive(q->front, size, q->capacity, i);
        if (alive) {
            printf(" [%2d:%3d]", i, q->data[i]);
        } else if (full && i == q->rear) {
            printf(" [%2d:  !]", i);        /* ! = 判满时空着却不许放人的那一格（路标） */
        } else {
            printf(" [%2d:  ·]", i);
        }
    }
    if (full) {
        printf("   ← ! = rear 站的这一格：方案 B 判满成立，它必须空着当路标，谁也不许用\n");
    } else {
        printf("   ← · = 空位（还没满，这些格子都可以放人）\n");
    }

    printf("    ");
    for (int i = 0; i < q->capacity; i++) {
        const char *mark = " ";
        bool is_f = (i == q->front);
        bool is_r = (i == q->rear);
        if (is_f && is_r) {
            mark = "FR";
        } else if (is_f) {
            mark = "F";
        } else if (is_r) {
            mark = "R";
        }
        printf(" [%2d:%3s]", i, mark);
    }
    printf("   ← F=front R=rear\n");
}
