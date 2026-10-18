# 链表队列 linked-queue

> 前两节都在跟「格子用完怎么办」死磕：线性数组队列会假满，循环队列要靠取模绕圈、还要区分空和满。
> 这一节直接把那整个问题**掀掉** —— 不预分配格子，来一个人就 `malloc` 一个节点。于是永不满、不需要取模。
> 但链表带来两类新问题：**方向**（单链表只能往前走）和**指针维护**（`head`、`tail` 得同时管对，还要 `free`）。

## 一句话是什么

链表队列 = 每个元素是一个独立 `malloc` 的节点，用 `next` 串成一条队；外面挂两个指针：
**`head` 指着队头（下一个出队的人），`tail` 指着队尾（最新来的人）**，再加一个 `size` 计人数。

```c
typedef struct LQNode { int val; struct LQNode *next; } LQNode;   /* 一个节点 = 号码牌 + 一张纸条 */
typedef struct { LQNode *head, *tail; int size; } LQueue;         /* 盒子：两个指针 + 一个计数 */
```

规矩只有一条，但它是本节的全部结论：**头部出队、尾部入队**。反过来（头进尾出）会退化到 `O(n)`，
本节把反例也实现了，并且把「到底走了几步」数给你看。

## 为什么需要它（不用它会怎样）

**不用链表、继续用数组会怎样。** 你得回答三个问题：容量预估多少（猜小了要扩容搬数据）、满了怎么办（拒绝？阻塞？）、
下标到墙怎么办（取模绕圈，还要 `size` 或路标格来区分空满）。链表队列把这三个问题一次全消掉：**不预估、永不满、
不需要取模** —— 堆有多少内存就能排多少人，`enqueue` 就是造个节点接上去，`dequeue` 就是摘个节点还回去。

**代价是什么。** 每个元素多背一个 8 字节的 `next` 指针，再加上分配器的管理头（实际占用常常 32 字节起）；
节点在堆上东一块西一块，一路 `next` 跳来跳去，CPU cache 命中率差。所以「数组 vs 链表」不是谁高级，
是**空间局部性**和**容量弹性**之间的取舍。

**为什么必须「头出尾进」—— 本节最要紧的一条。** 单链表只有一种能力：**顺着 `next` 往前走**。
- 头出：摘掉第一个节点只需改 `head` 一个指针 → `O(1)` ✓；尾进：`tail` 就挂在最后一个节点上，接上新人、换过 `tail` → `O(1)` ✓
- 反过来若「尾出」：要摘最后一个节点就得先知道它的**前驱**，可单链表没有 `prev` 指针，只能从头走到倒数第二个 → `O(n)` ✗

于是「先进先出」的两个不变量（入队 O(1) **且** 出队 O(1)）只剩这一种摆法能同时满足。本节用
`lqueue_pop_from_tail_count_steps` 把反例实现出来，摘 5 次的步数依次是 4、3、2、1、1 —— O(n) 变成看得见的数字。

**为什么要同时维护 `tail`。** 只有 `head` 的链表想知道队尾就得一路 `next` 走到底，`enqueue` 立刻变 `O(n)`。
`tail` 花的正是「省掉那 n 步」的钱 —— 这是队列比栈多维护一个指针的全部意义。

## 图解：三次 malloc 串成的一条队

### 整体形状（队里有 10、20、30，10 最早来）

```
        head                          tail
          │                              │
          ▼                              ▼
        ┌───────┐      ┌───────┐      ┌───────┐
   q →  │  10   │ ───→ │  20   │ ───→ │  30   │ ───→ NULL
        └───────┘      └───────┘      └───────┘
        ▲ 出队从这里走              入队从这里加 ▲
        摘头节点：改 1 个指针 = O(1)   接新节点：改 2 个指针 = O(1)
```

每个方框是一次**独立的 `malloc`**，地址在堆上东一块西一块，靠 `next` 串起来才叫「一条队」——
跟数组队列那种「一整块连续内存」完全不同。`q`（`LQueue` 盒子）本身放哪由调用者决定，demo 里放在 `main` 的栈上。

### 一进一出各自改哪几个指针

```
  入队 enqueue(40)：  tail->next = 新节点;  tail = 新节点;     ← 顺序不能反！
    [10]→[20]→[30]→NULL   变成   [10]→[20]→[30]→[40]→NULL      ▲tail 从 30 换到 40
    反过来写（先挪 tail 再接链）老尾巴就找不到新人了。空队列（tail==NULL）走另一条分支：head 和 tail 【同时】指向新节点。

  出队 dequeue()：  老 = head;  head = head->next;  free(老);   ← → [20]→[30]→[40]→NULL
    为什么必须 free：那个节点是 enqueue 时 malloc 来的，队列是唯一认识它的地方；只挪 head 不还内存就是纯泄漏。
    为什么值要在 free 之前取：free 之后连 old->val 都不能读了（悬空指针）。
```

### 本节最大的坑的现场图：摘掉「最后一个」节点时

```
   出队前：  head 和 tail 指着【同一个】节点
              ┌───────┐
              │  30   │ → NULL        ▲head ▲tail
              └───────┘
   只写 `head = head->next;`（变 NULL）然后 `free(old)`：
              ┌─ ─ ─ ─┐
              │ 已释放 │        head = NULL ✓   tail = ？✗ —— 它还指着这块已经还回去的内存！
              └─ ─ ─ ─┘                        下一次 enqueue 读/写 tail->next = 往已释放内存里写
   对法：free 之前先判断 `old->next == NULL`（我摘的是不是最后一个人），是就补一句 `q->tail = NULL;`
```

这也是「结构体包两个指针」的价值所在：`head` 和 `tail` 在同一处维护，才容易一起改对。
想不包结构体也行，那就得传二级指针（`LQNode **head, LQNode **tail`，注意 `(*tail)->next` 的括号不能省）；
传一级指针等于白改 —— 这条反例本节实现成 `lqueue_enqueue_broken`，demo 里当场撞。

## 逐个操作拆解

### `lqueue_make_node(int value)` —— 造一个孤立节点

`malloc(sizeof(LQNode))`，填 `val`，**`next` 置 `NULL`**，失败返回 `NULL`。`next` 必须置 `NULL`：这个节点此刻是队尾，
留着垃圾值的话一路 `next` 打印会跑到野内存里去。它是给 demo 演示「写法二」和「失败写法」用的
（让调用者拿到地址、从而能负责 `free`），正式操作里 `enqueue` 内部自己造。

### `lqueue_init` / `lqueue_destroy` —— 生命周期

`init` 什么都不用申请：`head = tail = NULL`、`size = 0` 就是合法空队列。⚠ 两个指针必须**同时**为 `NULL` ——
只清一个就是脏状态，下一次 `enqueue` 会走「接到老尾巴上」的分支，往野指针里写数据。
`destroy` 把可能残留的整条队全部 `free`：

```c
while (node != NULL) { next = node->next; free(node); node = next; }  /* ① 先抄下一个 ② 再 free ③ 往前走 */
q->head = q->tail = NULL; q->size = 0;
```

顺序是死规矩：写成 `free(node)` 之后再取 `node->next` 就是**读已释放内存**（典型悬空访问，ASan 会报警）。
最后两个指针一起清干净、结构体回到「合法空队列」，所以重复 `destroy` 也安全（demo 里故意调了两次）。

### `lqueue_enqueue(LQueue *q, int value)` —— 尾部入队，O(1)

三步：造节点 → 空队列就 `head = tail = node`，否则 `tail->next = node; tail = node;` → `size++`。
- **先造节点再动队列**：`malloc` 失败就直接返回 `false`，队列一个字节都没被改动，还是完好状态。
- 空队列那句 `head = node` **不能省**：漏了 `head`，出队会以为队列是空的，那个节点永远 `free` 不掉（泄漏）。
- 为什么放尾巴是 O(1)：因为 `tail` 字段帮我们记住了最后一个节点在哪，省掉「从头走到尾」的 n 步。

### `lqueue_dequeue(LQueue *q, int *out_value)` —— 头部出队，O(1)

```c
if (q->head == NULL) { /* 空队列：返回 false，绝不 free、绝不写脏 out_value */ return false; }
LQNode *old = q->head;                 /* ② 先记住要扔掉的人 */
int v = old->val;                      /*    值必须在 free 之前取出 */
bool was_last = (old->next == NULL);   /* ④ 提前判断：他是不是队伍里最后一个？ */
q->head = old->next;                   /* ③ 头指针往后挪一格 */
if (was_last) { q->tail = NULL; }      /* ④ ★★ 本节最容易漏的一行 ★★ */
old->next = NULL;                      /* 小习惯：摘下来先抹掉 next，误用读到 NULL 比读到垃圾安全 */
free(old);  q->size -= 1;              /* ⑤⑥ 谁 malloc 谁 free；人数 -1 */
if (out_value != NULL) { *out_value = v; }
```

上面是从 `lqueue.c` 摘的骨架，省掉了 `q == NULL` 的守卫和两处 `printf` 提示，顺序与源码一致。
`out_value` 是**输出参数**：C 只有一个 `return` 口子，成功与否走返回值，「谁走了」写进调用者给的地址；
传 `NULL` 表示只要人走、不关心他是几。判空用 `head == NULL`（不是 `size == 0`，但两者必须永远等价）。
为什么出队选头部：摘头只需要改 `head` 一个指针、不需要前驱信息；摘尾需要前驱，单链表没有 `prev`，那就得遍历。
**一进一出各挑最省事的那端**，这就是「头出尾进」四个字背后的全部理由。

### `lqueue_peek_front` / `lqueue_peek_rear` / `is_empty` / `size` —— 全 O(1)

`peek_front` 读 `head->val`（下一个要服务的），`peek_rear` 读 `tail->val`（最新来的）；
后者能 O(1) **全靠 `tail` 字段**。判空分别看 `head == NULL` / `tail == NULL`，空队列返回 `false` 且不写 `out_value`。
`is_empty` 返回 `head == NULL`，`size` 直接读字段。判空只看 `head`，但 `size == 0` 必须和它等价 ——
不等价就说明某处 `enqueue`/`dequeue` 忘了 `size++`/`size--`，这是本结构最常见的 bug 之一，所以 `lqueue_print` 顺手验证。
签名里的 `const` 是承诺「我只读不改」，误写编译器立刻报错。

### `lqueue_print(const LQueue *q, const char *label)` —— 教学打印

打 `head → [10] → [20] → [30] ▲tail → NULL`，左边是队头（先走的人），`▲tail` 挂在尾巴节点上。
真正的队列**不需要**把整条队倒出来（抽象上只准碰两头），打印是为了看见 `head`/`tail` 各站在哪一端。
它顺带走一遍 `next` 数节点数、和 `size` 字段比对，不一致就打警告 —— 注意这个「走一遍」本身是 `O(n)`，
正式代码判断大小只用 `size` 字段，**绝不遍历**。

### `lqueue_pop_from_tail_count_steps(...)` —— 反例：坚持尾部出队，并返回访问过的节点数

走到**倒数第二个**节点（`while (prev->next != q->tail)`，每走一步 `steps++`），再 `prev->next = NULL`、`tail = prev`、
`free(victim)`；只有一个节点时走特殊分支（`head`、`tail` 一起归 `NULL`，步数 1）。它仍然是**正确**的出队
（值拿到了、链也断干净了），只是慢：n 个人要走 n-1 步，人数翻倍步数也翻倍 —— 这就是 O(n) 的现场证据。
要两头都能 O(1)，得用双向链表（`02-linked-list/`）或者干脆用循环数组（`04-queue/deque`）。

### `lqueue_enqueue_double` 与 `lqueue_enqueue_broken` —— 写法二 vs 失败写法

`double` 版是不包结构体的正统写法：`enqueue` 要改 `tail` 本身，所以传「地址的地址」；`head` 也要传，
因为空队列时第一个节点要同时赋给 `head`。签名变成 `LQNode **, LQNode **, int, int *` —— 能用但很累：
调用处全是 `&`，每个字段都要写 `*`，而且 `(*tail)->next` 的括号漏了就变成 `*(tail->next)`，逻辑彻底错。

```c
tail->next = new_node;   /* broken 版：这一句【是有效的】—— 改的是别人的节点内部 */
tail       = new_node;   /* ⚠ 这一句【毫无用处】—— 只改了形参这份拷贝，出了函数就作废 */
```

这是 `swap_by_value` 的翻版，只不过对象换成了指针。为了不把内存搞漏，节点由**调用者** `malloc`、调用者负责能 `free` 到它
（如果 `malloc` 写在函数内部，那块内存除了函数里那份拷贝没人认识 → 彻底泄漏；本仓库要求 ASan 0 leak，所以换个方式演示）。

## 复杂度一览

| 操作 | 时间 | 空间 | 备注 |
|---|---|---|---|
| `init` | O(1) | O(1) | 不预申请内存，所以不会失败 |
| `enqueue`（尾进） | **O(1)** | 一节点 | 靠 `tail` 省掉走到尾的 n 步 |
| `dequeue`（头出） | **O(1)** | — | 改 1 个指针 + 1 次 free |
| 尾部出队（反例） | **O(n)** | — | 要找前驱，步数 n-1 |
| `peek_front` / `peek_rear` | O(1) | — | 后者靠 `tail` |
| `is_empty` / `size` | O(1) | — | `size` 是字段，不是数出来的 |
| `destroy` / `print` | O(n) | — | 逐个 free / 教学用 |
| 稳态占用 | — | 每人 (int + 指针 + 分配器开销) | 用多少长多少，没有预留浪费 |

对照前两节：**永不满、不需要取模、不需要判满**，两头严格 O(1)；代价是每元素多一个指针 + 每次操作一次
`malloc`/`free`，以及 cache 不友好。「先进先出」要值钱，必须**入队 O(1) 和出队 O(1) 同时成立** ——
本节是三种实现里唯一不用任何补偿技巧就达标的那个，前提是把进出两端选对了。

## 运行结果（真实输出节选）

复现（项目根目录）：`make one DIR=04-queue/linked-queue`

**① 第 1 幕：FIFO，以及摘掉最后一个节点时的那句警告**

```text
  enqueue(30)
  现在队列：  size=3, head=→节点, tail=→节点
    head → [10] → [20] → [30] ▲tail → NULL
  dequeue()
      ⚠ 摘掉的是【最后一个】节点：head 变 NULL，tail 也必须一起置 NULL！（漏了这行，下次 enqueue 就会往已释放的内存里写 → ASan 报 use-after-free）
    出队的是 30（队头，最早进来的那个）
  现在队列：  size=0, head=NULL, tail=NULL
    head → NULL   （空队列，尾巴也是 NULL）
```

**② 第 2 幕：`head` 和 `tail` 相等 = 队里只剩一个节点**

```text
  现在 head 和 tail 指向【同一个】节点：
    head=0x5c57c6c0b040, tail=0x5c57c6c0b040  ← 一样，说明队里只剩 1 个节点
```

**③ 第 3 幕：反例的步数（尾部出队）**

```text
    第 1 次尾部出队：拿走 5，访问了 4 个节点 → 队伍越长越慢 = O(n)
    第 2 次尾部出队：拿走 4，访问了 3 个节点 → 队伍越长越慢 = O(n)
    第 3 次尾部出队：拿走 3，访问了 2 个节点 → 队伍越长越慢 = O(n)
    第 4 次尾部出队：拿走 2，访问了 1 个节点 → 队伍越长越慢 = O(n)
      （队列只有 1 个节点，访问 1 个就够了）
    第 5 次尾部出队：拿走 1，访问了 1 个节点 → 队伍越长越慢 = O(n)
```

**④ 第 4 幕：一级指针的失败现场**

```text
    结果：tail->next = 新节点【生效了】（改的是别人节点里的字段），
          但 `tail = new_node;` 只是改拷贝 → q.tail 还指着老节点：
      改动后：  size=1, head=→节点, tail=→节点
    head → [300] ▲tail → [999] → NULL
    ⚠ 警告：一路 next 数出来是 2 个，但 size 字段写着 1 —— 两者必须一致！
          但 tail 没换，所以队尾读出来还是老节点：
          peek_rear → 300（本该是 999）
```

**⑤ 第 5 幕：空队列上的操作都安全**

```text
  dequeue()
      [失败] dequeue：队列是空的（head==NULL, size=0）
    → 这次 dequeue 没成功（队列空），见上面的 [失败]
      [失败] peek_front：队列是空的，没有队头可看
```

逐段解释：
- **①** 入队 10→20→30，`▲tail` 始终挂在最新的 30 上；出队还是 10→20→30，FIFO 成立。最后一个人走掉时打印那句警告，就是「`tail` 必须跟着归 NULL」的现场；之后 `head`、`tail` 双双 `NULL`，状态合法。
- **②** 只剩一个节点时 `head` 和 `tail` 数值相同 —— 这正是「下一个被摘掉的就是最后一个」的信号，`dequeue` 里靠 `old->next == NULL` 抓这个瞬间（地址每次运行都变，那是系统的地址随机化）。
- **③** 5 个人的队伍，从尾巴摘第一刀走了 **4** 个节点，之后 3、2、1 递减；只剩 1 个时走特殊分支、步数 1。步数随队伍长度增长 = O(n)，和正常「头出」的恒 O(1) 形成对照。
- **④** 这一屏最有信息量：`tail->next = new_node` 生效了，所以沿 `next` 确实能走到 999；但 `tail` 本身没换，于是 `▲tail` 还挂在 300 上、`peek_rear` 读出 300、`size` 也和实际节点数对不上（2 对 1）—— 一处「以为改了其实没改」会同时坏掉三个地方。
- **⑤** 空队列 `dequeue`/`peek` 只返回 `false` 并打印原因，不 `free`、不写脏 `out_value`；随后 `is_empty=true, size=0` 两个判据一致，再入队出队一轮证明没有残留脏指针。

剩下共 195 行输出，一句话带过：第 3 幕的队列 `destroy` 掉了；第 4 幕的「写法二」用二级指针建了三个节点并手工
`free` 干净（打印出 `head=0x…、tail=0x…、size=1/2/3`），末尾手工补状态后打印 `修好之后`；收尾对 `q` 连续 `destroy` 两次不崩（`head=(nil), tail=(nil), size=0`）。

## 常见的坑

**坑 1：摘掉最后一个节点时忘了 `tail = NULL`（本节头号事故）。** 错法：`head = head->next; free(old);` 就完事 ——
队空了，但 `tail` 还指着刚 `free` 的那块内存，下一次 `enqueue` 就往已释放内存里写 → `heap-use-after-free`，
表现为「看起来塞进去了但打不出来」或者直接崩。对法：`free` 之前先 `bool was_last = (old->next == NULL);`，
是最后一个就把 `tail` 一起置 `NULL`。

**坑 2：方向选反，改成「头进尾出」。** 错法：头部插节点（O(1) 没问题）+ 尾部摘节点（要找前驱，O(n)），队列变成「每次出队遍历一遍」。
对法：单链表当队列只能**头出尾进**；真需要两头都 O(1) 就换双向链表或循环数组（`04-queue/deque`）。

**坑 3：传一级指针想改 `tail`/`head` 本身。** 错法：`void enqueue(LQNode *tail, LQNode *node) { tail->next = node; tail = node; }`
—— 第二行改的是拷贝，外面 `q->tail` 纹丝不动，于是队尾指针越落越远、`peek_rear` 读到旧人、`size` 也对不上。
对法：传 `LQNode **tail`（`(*tail)->next` 的括号不能省），或把 `head`/`tail` 装进结构体传 `LQueue *`。

**坑 4：出队不 `free`，或 `free` 之后还读这个节点。** 错法：`head = head->next;` 就结束（每次出队泄漏一个节点）；
或者 `free(old); printf("%d", old->val);`（悬空读）。对法：值先存进局部变量 `v` 再 `free(old)`；`destroy` 里同理 —— 先抄 `next` 再 `free` 当前。

**坑 5：新节点的 `next` 没置 `NULL`。** 错法：`LQNode *n = malloc(sizeof(LQNode)); n->val = v;` 就接上去 ——
`next` 是残留垃圾值，而它现在站在队尾，于是 `head → ... → 垃圾地址 → ？`，打印或 `destroy` 直接跑到野内存。
对法：造节点时 `node->next = NULL`；摘下来准备 `free` 时也顺手 `old->next = NULL`。

**坑 6：`size` 字段和 `head == NULL` 各说各话。** 错法：某条分支漏了 `size++`（比如空队列分支里忘加），
或在 `dequeue` 的失败路径上也 `size--`。后果是 `is_empty`（看 `head`）说有人、`size` 说 0，判据互相打架。
对法：两个判据必须永远等价（demo 第 5 幕专门验证一次），`lqueue_print` 里那次「走一遍数节点」就是在替你查这笔账。

**坑 7：`enqueue` 先改队列再 `malloc`。** 错法：已经 `tail = node` 之后才发现 `malloc` 返回 `NULL`，尾巴指针就成了野值。
对法：先造节点，失败就直接 `return false` —— 本节的 `enqueue` 保证失败时「队列一个字节都没被改动，还是完好状态」。

## 自检问题

1. 为什么链表队列不需要判「满」？—— 它不预分配：每次入队 `malloc` 一个节点，只要堆还有内存就能继续，满的唯一界限是 `malloc` 失败。
2. `tail` 字段省掉了什么？—— 省掉「从头走到尾」的 n 步。只有 `head` 的链表要入队到尾部就是 O(n)，那就不叫队列了。
3. 出队时程序怎么知道「他正好是最后一个」？—— 看 `old->next == NULL`（此时 `head` 和 `tail` 指同一节点）。此时 `head` 变 `NULL`，`tail` 必须一起置 `NULL`。
4. 为什么 `dequeue` 必须 `free`，而 `peek_front` 不能？—— 节点是 `enqueue` 时 `malloc` 的，队列是唯一认识它的地方：出队后没人引用它，不还就是泄漏；`peek` 之后他还在队里，`free` 就等于把人删了。
5. 空队列时 `head == NULL`，那 `tail` 和 `size` 该是什么？—— 必须是 `NULL` / `0`。三者等价，任何不一致都是 bug。
6. 「头进尾出」为什么不行，「头出尾进」为什么行？—— 尾出要改的是尾巴节点的**前驱**的 `next`，单链表只能往前走、没有 `prev`，必须遍历；头出不需要前驱，改 `head` 就够。
7. `lqueue_enqueue_double` 里 `(*tail)->next` 的括号为什么不能省？—— `tail` 是「尾巴指针的地址」，`*tail->next` 会被解析成 `*(tail->next)`，优先级完全不同，逻辑彻底错。
8. 三种队列实现各自的「满」是怎样的？—— 线性数组：假满（有空位却报满）；循环数组：`size == capacity` 才是真满（或方案 B 少用一格）；链表：不存在满，只有 `malloc` 失败。

## 下一步

- **`04-queue/deque/`**：双端队列。本节说「单链表尾出是 O(n)，要两头都快得用双向链表或循环数组」，`deque` 就是循环数组那条路的答案，`(x - 1 + capacity) % capacity` 在那里天天用。
- **`04-queue/queue-by-stacks/`**：只有栈（后进先出）怎么凑出队列（先进先出），顺便见识「均摊 O(1)」。
- 回头对照：`04-queue/array-queue/`（假满从哪来）、`04-queue/circular-queue/`（取模怎么治）—— 三节连读才能体会「本节为什么可以不管满」。
- 前置：`02-linked-list/singly/`（节点与 `next`）、`00-c-foundation/dynamic-memory/`（谁 `malloc` 谁 `free`）。
