# 链表栈 linked-stack

> 用「一摞自己 malloc 的节点 + next 指针」实现栈：不预估容量、不搬数据、每一次操作都是严格 O(1)。

---

## 一句话是什么

**把栈顶放在链表的头部。**
每个节点是一小块堆内存，装着「我的值」和「下一个节点的地址」。
`push` = 造一个新节点，让它指向老 head，再把 head 换成它（改 2 个指针）；
`pop` = 让 head 跳到 `head->next`，把摘下来的节点 `free` 掉。
两个操作都跟「栈里现在有几个元素」完全无关 → 永远 O(1)，而且永远不可能「栈满」。

同时这一节还要解决一个纯 C 语言问题：**在头部操作会改掉 `head` 这个指针本身**，
所以函数要么传 `LNode **`（指针的指针），要么把 `head` 装进结构体里传 `LStack *`。**本节选结构体包 head**，理由见下面。

---

## 为什么需要它

数组栈（上一节）已经很快了，链表栈存在的理由有三个，每一个都能在真实项目里遇到：

| 痛点 | 数组栈的表现 | 链表栈的表现 |
|---|---|---|
| **容量估不准 / 根本没法估** | 固定容量版会 push 失败；扩容版要多浪费最多一倍空间，还得写 realloc | 一个元素一次 malloc，用到多少占多少，不会「栈满」 |
| **不能容忍任何一次 O(n)** | 扩容那一次要搬 n 个元素。摊还虽是 O(1)，但「最坏的那一下」是真的慢 | 每一次 push 都是 1 次 malloc + 2 行赋值，**严格 O(1)** |
| **同时存在很多个栈、每个都很小** | 每个栈都要预开一整块（哪怕只用 2 格也要占 capacity 格） | 每个节点按需申请，2 个元素就只占 2 个节点 |

反过来，**不用「栈」这个抽象**的痛还有一条（和上一节一样，但值得再强调）：
很多算法要求「最晚出现的最先处理」（括号匹配、撤销、DFS、表达式求值），
如果用数组裸写，每次都得自己维护「当前最新位置」并且防止它变成负数 ——
栈做的就是把这件事变成一个变量 `head` 加四个函数。

还有一个**纯学习**的理由：链表栈是全仓库第一个「每个操作都动指针」的结构，
它是 `02-linked-list` 和后面二叉树、图的预热。`push` 那两行顺序一反就自环，
这种 bug 编译器完全不会报，只能靠画内存图想清楚。

---

## 内存布局图解

⚠ 三个关键点：①节点在堆上是**散着**的，靠 `next` 串成顺序；②**左边是栈顶**（最新）；③`head` 只是一个「装着地址的变量」，它可以被换掉。

```
  lstack_init(&s) ;  push(10) ; push(20) ; push(30)

   s（结构体，在调用者的栈帧里）
   ┌──────────┬───────┐
   │ head     │ size=3│
   └────┬─────┴───────┘
        │ 存的是一个地址
        ↓
   ┌──────────────┐        ┌──────────────┐        ┌──────────────┐
   │ val │  30    │        │ val │  20    │        │ val │  10    │
   │ next│ 0x7f1b ├──────→ │ next│ 0x7f22 ├──────→ │ next│  NULL  │
   └──────────────┘        └──────────────┘        └──────────────┘
      ↑ 栈顶                                          ↑ 栈底（最先压入）
      新节点永远插在左边                                 它被挤到最右边


  push(40)：三步，一个老节点都不用搬
     ① new_node = malloc(sizeof(LNode))        →  ┌──────────────┐
     ② new_node->next = s->head                →  │ val │  40    │
     ③ s->head = new_node                      →  │ next│ ?????  │
                                                   └──────────────┘
   改完后：
   s->head ──→ [40] ──→ [30] ──→ [20] ──→ [10] ──→ NULL
                栈顶                            栈底

   ★ ②③ 顺序反了会怎样？
     先 `s->head = new_node`，此时 s->head 已经指向 new_node 了，
     再执行 `new_node->next = s->head` 就等价于 `new_node->next = new_node`：
     链表变成 1 个节点的【自环】，lstack_print 无限循环、lstack_destroy 无限 free。
     这是本节最经典的一个「看起来完全正常」的错误。


  pop()：摘 head 并 free
     ① old = s->head                （先记住要还的那块内存）
     ② s->head = old->next          （★ head 必须先跳走！）
     ③ v = old->val                 （先取值）
     ④ free(old)                    （数组栈没有这步，链表栈的旧值是【真的消失】）

   s->head ──→ [40] ──→ [30] ──→ [20] ──→ [10] ──→ NULL
       ①②        ↑
                  old ──→ free 掉，这块堆内存还给系统
   写成 `free(old); s->head = old->next;` 就是 use-after-free：
   内存已经可能被人复用，读到的 next 是垃圾，链表当场断成奇怪的样子。
```

### 「为什么必须在头部 push/pop」—— 尾部的代价

```
  在头部操作（O(1)）：head 就是栈顶，一步到位
     head ──→ [top] ──→ … ──→ NULL
                ↑ push/pop 都在这里，只改 2 个指针

  在尾部操作（O(n)）：单链表【只能从 head 往后走】，想碰尾巴就得走完全程
     head ──→ (…) ──→ (…) ──→ (…) ──→ [tail] ──→ NULL
       └──────── 要走 k 步才能找到尾巴（k = 当前元素个数）───────┘

     · 尾部 push：得先找到 tail 才能写 tail->next           → O(n)
     · 尾部 pop：不仅要找 tail，还要找 tail 的【前一个】节点，
                 单链表不能倒着走，只能一路记 prev           → O(n)
     → 每次操作都要遍历，栈就退化成了「一个特别慢的数组」。
     （真想两端都快：用双向链表，或者干脆做成队列 04-queue/linked-queue 的「头出尾进」。）
```

main.c 第 2 幕把这件事**用步数实测**出来了：尾部压栈第 1~5 次分别访问 0、1、2、3、4 个节点。

### 改到 `head` 本身的两条路

```
  路线 1（本节采用）：把 head 装进结构体 —— 传结构体的地址
     调用者:  LStack s;  lstack_push(&s, 40);
     函数里:  s->head = new_node;      ← 改的是【外面那个盒子】里的字段，真的生效
     好处：签名干净（一个 LStack*）、能顺手带 size、以后加字段不用改所有函数签名

  路线 2（算法题常用）：传指针的指针
     调用者:  LNode *head = NULL;  push(&head, 40);
                        │
                        └→ 传的是「head 这个变量自己的地址」
     函数里:  *head = new_node;        ← 一个 * 就是「head 变量本身」
     代价：每处都要写 *head、(*size)++，括号容易漏；size 还得单独带
```

---

## 核心操作逐个拆解

### 1. `lstack_make_node(int value)` —— 造一个孤立节点

**干什么**：申请一个 `LNode`，填值，`next = NULL`。
**一步步**：`malloc(sizeof(LNode))` → 判 NULL → `node->val = value` → `node->next = NULL`。
**边界/坑**：`next` 必须显式置 NULL。`malloc` 不清零，`next` 里是脏地址；万一某条路径忘了接，`lstack_print` 就会顺着脏地址走飞。
**为什么单独暴露**：main.c 演示「失败写法」时需要一个「调用者持有的节点」，好由调用者负责 free（见第 3 幕）。
**对应代码**：`lstack.c` 顶部。

---

### 2. `lstack_init(LStack *s)` / `lstack_destroy(LStack *s)`

**init 干什么**：`head = NULL; size = 0;`。链表栈不预申请内存，所以 init 不可能失败 —— 这是它和数组栈的明显区别（数组栈 init 里有 malloc，会失败）。
**空栈的定义**：以 `head == NULL` 为准，而不是 `size == 0`。`size` 是记账用的，记错了结构还在；`head` 是结构本身。

**destroy 干什么**：一路 free 掉所有还没弹走的节点。
**一步步**：`cur = head` → 循环 { `next_node = cur->next`; `free(cur)`; `cur = next_node` } → `head = NULL; size = 0`。
**边界**：空栈 destroy 完全安全（循环不进，`free(NULL)` 也不执行）；打印了「free 掉 N 个」，方便你核对没漏。
**死规矩**：先备份 `next` 再 `free`。顺序反了 = use-after-free，ASan 直接报。
**对应代码**：`lstack_destroy`。

---

### 3. `lstack_push(LStack *s, int value)` —— 头部插节点

**干什么**：把新节点插到链表最前面，成为新栈顶。
**一步步**：
1. 造节点，失败返回 `false`；
2. `new_node->next = s->head;`（空栈时 `s->head` 是 NULL，正好成为新链尾，不用特判）；
3. `s->head = new_node;`（改盒子里的字段，所以 `LStack *` 就够，不需要二级指针）；
4. `s->size++`。

**边界情况**：空栈 push（第 2 步自动处理）；malloc 失败（栈保持原样）。
**最容易犯的错**：2、3 顺序写反 → 自环（上面图解里有）。
**对应代码**：`lstack_push`。

---

### 4. `lstack_pop(LStack *s, int *out_value)` —— 摘头节点并 free

**干什么**：栈顶节点出栈、内存归还系统，值通过输出参数带回。
**一步步**：判 `head == NULL` → `old = head` → `head = old->next` → 读 `old->val` → 写 `*out_value`（若不为 NULL）→ `free(old)` → `size--`。
**边界情况**：空栈 pop 返回 false 且**不碰 `out_value`**（保证失败不写脏调用者的变量）；`out_value` 传 NULL 表示「只丢不读」。
**注意 free 时机**：值必须在 `free` **之前**取出来，否则读的是已释放内存。
**对应代码**：`lstack_pop`。

---

### 5. `lstack_peek(const LStack *s, int *out_value)` —— 只看栈顶

**干什么**：读 `s->head->val`，链表一动不动。
**边界**：`head == NULL` 时必须先挡住，否则解引用空指针；签名带 `const`，误写 `s->head = ...` 编译器直接报错。
**对应代码**：`lstack_peek`。

---

### 6. `lstack_is_empty` / `lstack_size`

`is_empty` 看 `head == NULL`（O(1)）；`size` 直接读 `s->size`（O(1)）。
**为什么要专门维护 size**：如果不存 size，「这个栈几个元素」就得遍历一遍 → O(n)，而且遍历一次就破坏了「栈操作都是 O(1)」的说法。多一个 int 字段换 O(1)，非常划算。

---

### 7. `lstack_print` —— 教学打印

**干什么**：`head → [30] → [20] → [10] → NULL`，左边是栈顶。
**为什么正式 API 里没有它**：栈只该碰栈顶。这里是为了让 LIFO 看得见。
**注意**：打印本身是 O(n)，别把它当栈操作算进复杂度。

---

### 8. `lstack_push_broken(LNode *head, LNode *new_node)` —— 反例：一级指针改不到 head

**干什么**：演示「函数里改了 head，外面毫无变化」。
**一步步**：`new_node->next = head;`（有效，改的是节点内部）→ `head = new_node;`（❌ 只改参数拷贝）。
**为什么它不 malloc**：真实 bug 版本会在函数内部 malloc，于是那个节点除了函数里的拷贝以外没人记得，变成彻底泄漏。本仓库要求 ASan 0 leak，所以让调用者造节点、用完自己 free，演示同一个指针问题而不引入泄漏。
**对应代码**：`lstack_push_broken` + main.c 第 3 幕。

---

### 9. `lstack_push_double_pointer(LNode **head, int value, int *size)` —— 正统写法 B

**干什么**：不包结构体，靠二级指针改到外面的 `head`。
**一步步**：`new_node->next = *head;` → `*head = new_node;`（★ 一个 `*` 就是「外面那个变量」）→ `(*size)++;` → `return new_node;`。
**边界**：`head == NULL`（连地址都没给）直接返回；`size` 允许 NULL。
**为什么 `(*size)++` 的括号必须有**：`*size + 1` 会被解析成「size 指向的值 + 1」，而 `*size++` 是「先解引用再把 size 指针本身后移」，都不是你要的。
**对应代码**：`lstack_push_double_pointer`。

---

### 10. `lstack_push_at_tail_count_steps` —— 反例之二：尾部压栈并数步数

**干什么**：故意在尾巴插节点，返回「访问了多少个节点」，把 O(n) 变成看得见的数字。
**一步步**：造节点 → 空栈就 `head = new` 返回 0 → 否则 `cur = head`，`while (cur->next != NULL) { cur = cur->next; steps++; }` → `cur->next = new`。
**边界**：空栈走 0 步；单节点走 1 步（起点也算访问过，所以 `return steps + 1`）。
**对应代码**：`lstack_push_at_tail_count_steps`，运行结果第 2 幕就是它的输出。

---

## 复杂度表

横向对比：链表栈 vs 数组栈 vs 链表队列（下一节的 `linked-queue`）。

| 操作 | 链表栈（本节） | 数组栈 | 链表队列（头出尾进） |
|---|---|---|---|
| push / 入队 | **O(1) 严格**（无搬数据、无扩容） | O(1)（扩容那次 O(n)，摊还 O(1)） | O(1)（要维护 tail） |
| pop / 出队 | **O(1) 严格** | O(1) | O(1) |
| peek / 看队头 | O(1) | O(1) | O(1) |
| 判空 / 报 size | O(1)（靠 `head`/`size` 字段） | O(1)（靠 `top`） | O(1)（靠 head/tail 或 size） |
| 在「另一端」操作 | **O(n)**（本节实测给你看了） | O(n)（要整体挪） | O(1)（因为本来就是双端） |
| 随机访问第 k 个 | O(n) | O(1)（但违反栈语义） | O(n) |
| 单元素内存 | int(4) + 指针(8)，还要对齐 + malloc 管理头 → 实际 24~32 字节 | 4 字节 | 同左 |
| 会不会「栈满」 | **不会**（堆够用就行） | 固定容量版会；扩容版基本不会 | 不会 |
| cache 友好 | 差（节点散落，每跳一次 next 可能 cache miss） | **好**（连续） | 差 |
| malloc/free 次数 | 每个元素 1+1 次 | 1 次（或每次搬一次） | 每个元素 1+1 次 |
| 实现难度 | 关键是 push 两行顺序 + free 前存 next | 关键是 top 流派别混 | 关键是空队列时 tail 同步 |

一句话选择：**元素数量完全不可预测 / 不能容忍扩容那一跳 → 链表栈；能估算数量、追求速度和 cache → 数组栈。**

---

## 代码导读

| 文件 | 作用 |
|---|---|
| `lstack.h` | 菜单：`LNode`（`val` + `struct LNode *next`）与 `LStack`（`head` + `size`）两个类型；文件头注释直接写明「栈顶必须放头部」和「`LNode**` vs 结构体包 head」的取舍 |
| `lstack.c` | 实现。顶部有完整内存图；每个函数上方讲「为什么这么写 + 顺序反了会怎样」；额外 3 个反例/对比函数（`lstack_push_broken`、`lstack_push_double_pointer`、`lstack_push_at_tail_count_steps`） |
| `main.c` | 四幕：头部压栈的链变化 → 尾部压栈实测 O(n) → 一级指针失败 vs 二级指针 vs 结构体 → 边界与 destroy |

**本目录完全自包含**：只 `#include <stdio.h> <stdlib.h> "lstack.h"`，没有引用 `03-stack/array-stack/` 的任何文件（Makefile 按目录独立编译，跨目录 include 必然失败）。

---

## 编译与运行

在项目根目录执行：

```bash
make one DIR=03-stack/linked-stack

# 内存安全检查（链表栈是最容易泄漏/use-after-free 的一节，必跑）
gcc -Wall -Wextra -Wshadow -std=c11 -g -fsanitize=address -static-libasan \
    03-stack/linked-stack/*.c -o /tmp/asan_a3 && /tmp/asan_a3
```

`-Wall -Wextra -Wshadow -std=c11`：0 警告 0 错误；ASan 含泄漏检查：无任何报告（每个 `lstack_make_node`/`lstack_push` 分配的节点都被 `lstack_pop` 或 `lstack_destroy` 还掉了，包括第 3 幕反例里调用者自己 `malloc` 的那个节点）。

---

## 运行结果

（下面输出里的 `0x…` 地址每次运行都会变 —— 系统开了地址随机化，那是正常的；关键看**同一行调用前后地址是否相同**。）

```text

========================================
第 1 幕：链表栈长什么样（栈顶 = 链表头）
========================================
  刚初始化：  size=0：    head → NULL   （空栈：一个节点都没有）
    head = (nil)（NULL 就是空栈），size=0

  push(10)
  现在栈里：  size=1：    head → [10] → NULL   （左=栈顶/最新，右=栈底/最老）

  push(20)
  现在栈里：  size=2：    head → [20] → [10] → NULL   （左=栈顶/最新，右=栈底/最老）

  push(30)
  现在栈里：  size=3：    head → [30] → [20] → [10] → NULL   （左=栈顶/最新，右=栈底/最老）

  看清楚了吗：10 明明最先压入，现在却被挤到最右边（栈底）。
  新节点永远插在【左边】，head 每次都被换成刚建的那个节点 —— 这就是「头部压栈」。

  peek() 只读 head 那个节点：栈顶 = 30，链表一根指针都没动
  peek 之后：  size=3：    head → [30] → [20] → [10] → NULL   （左=栈顶/最新，右=栈底/最老）

  pop()：摘掉 head 节点，free 掉它，head 跳到 next
    弹出 30（顺便把它 malloc 的那块内存 free 了）
  现在栈里：  size=2：    head → [20] → [10] → NULL   （左=栈顶/最新，右=栈底/最老）

  把所有元素弹光，看弹出顺序：20 10 ← 后压入的先弹出（LIFO）
  现在栈里：  size=0：    head → NULL   （空栈：一个节点都没有）

========================================
第 2 幕：头部 push 是 O(1)，尾部 push 是 O(n) —— 用步数实测
========================================

  【头部压栈】（正式做法）：只改 2 个指针，跟栈里已有几个元素完全无关
    压 5 个之后：  size=5：    head → [5] → [4] → [3] → [2] → [1] → NULL   （左=栈顶/最新，右=栈底/最老）
  每次 push 的代价恒定 = 1 次 malloc + 2 行指针赋值 → O(1)

  【尾部压栈】（反面做法）：必须先从头走到尾找最后一个节点，走几步打印几步
  第 1 次尾部 push → 访问了 0 个节点才找到尾巴，size=1
  第 2 次尾部 push → 访问了 1 个节点才找到尾巴，size=2
  第 3 次尾部 push → 访问了 2 个节点才找到尾巴，size=3
  第 4 次尾部 push → 访问了 3 个节点才找到尾巴，size=4
  第 5 次尾部 push → 访问了 4 个节点才找到尾巴，size=5
  步数 0、1、2、3、4 一路涨：压 n 个元素总共 0+1+2+...+(n-1) ≈ n²/2 步 → O(n²)
  尾部 pop 更糟：单链表不能倒退，删尾巴还得先找「倒数第二个」，照样 O(n)。
  所以栈顶只能放在【头部】。（真想两端都快 → 双向链表，或 04-queue/linked-queue 的头出尾进）
    destroy：一路 free 掉 5 个还没弹走的节点（谁 malloc 谁 free）
    destroy：一路 free 掉 5 个还没弹走的节点（谁 malloc 谁 free）

========================================
第 3 幕：想改到 head 这个指针本身，该怎么办（本节的 C 语言重点）
========================================

  --- 反例：void f(LNode *head, LNode *new_node) ---
  外面原本的栈：  size=1：    head → [100] → NULL   （左=栈顶/最新，右=栈底/最老）
  调用前：外面 box.head = 0x5b10f20eb440
      [函数内部] 我把 head 指到新节点了：head 现在 = 0x5b10f20eb420（值=999）
  调用后：外面 box.head = 0x5b10f20eb440   ← 一模一样，新节点根本没接上
  原因：C 传参永远是拷贝。函数里的 head 是另一变量，它指向哪、被改成谁，
        都影响不到 box.head。想改「指针本身」，就得传「指针的地址」。
  外面的栈还是：  size=1：    head → [100] → NULL   （左=栈顶/最新，右=栈底/最老）
    destroy：一路 free 掉 1 个还没弹走的节点（谁 malloc 谁 free）

  --- 正统写法 A：把 head 装进 LStack 结构体，传 LStack *（本项目采用）---
  box 本身在栈上的地址 = 0x7ffdc66f8120，里面的 head 字段地址 = 0x7ffdc66f8120（两者其实同一个位置）
  函数收到的是 LStack *s，`s->head = new_node;` 就是「往那个字段里写」→ 真能改到外面
  结构体版压栈后：  size=2：    head → [2] → [1] → NULL   （左=栈顶/最新，右=栈底/最老）

  --- 正统写法 B：传二级指针 LNode **head（写算法题/函数式风格时常见）---
  调用前 bare_head = (nil)
  两次 push 之后 bare_head = 0x5b10f20eb3e0, bare_size = 2   ← 变量真的被改到了
  链：head → [8] → [7] → NULL
  两种写法对比：
    A 结构体包 head：签名干净（LStack *），还能顺手放 size，本节选它；
    B 二级指针 LNode **：不用结构体，但要自己带 size、每处都要写 *head，容易漏括号。
  手工 free 掉 2 个节点，bare_head 已置 (nil)

========================================
第 4 幕：边界情况
========================================
  good 现在：  size=2：    head → [2] → [1] → NULL   （左=栈顶/最新，右=栈底/最老）

  pop 掉剩下的元素：
      弹出后：  size=1：    head → [1] → NULL   （左=栈顶/最新，右=栈底/最老）
      弹出后：  size=0：    head → NULL   （空栈：一个节点都没有）
      [失败] pop：栈是空的（head == NULL，size=0）
  空栈上再 pop 一次（上面 [失败] 就是它的输出，程序不会崩）

  空栈 peek：
      [失败] peek：栈是空的，没有栈顶可看
    → false，dummy 仍是 0，没被写脏

  空栈上再 push(42)，栈可以无限复活（不像数组栈有容量天花板）：

  push(42)
  现在栈里：  size=1：    head → [42] → NULL   （左=栈顶/最新，右=栈底/最老）

========================================
收尾：destroy 会把还没弹走的节点全部 free
========================================
  残留着 3 个节点的栈：  size=3：    head → [44] → [43] → [42] → NULL   （左=栈顶/最新，右=栈底/最老）
    destroy：一路 free 掉 3 个还没弹走的节点（谁 malloc 谁 free）
  destroy 之后：  size=0：    head → NULL   （空栈：一个节点都没有）
    destroy：一路 free 掉 0 个还没弹走的节点（谁 malloc 谁 free）
  第 1 幕的 s 也 destroy 了一遍（当时已空，free 掉 0 个节点）

=== 本节结束 ===
记住四句话：
  1) 栈顶放【链表头部】：push/pop 都是 O(1)；放尾部单链表就是 O(n)。
  2) push 两行顺序不能反：先 new->next = head; 再 head = new;（反了就自环）。
  3) pop/destroy 必须先存好 next，再 free 当前（free 后读 next = use-after-free）。
  4) 改 head 本身要么传 LNode **，要么把 head 装进结构体传 LStack *（本节选后者）。
```

---

## 常见的坑

1. **`push` 两行顺序写反**（`s->head = new; new->next = s->head;`）→ 新节点指向自己，形成 1 节点自环。打印时 `for (cur = cur->next; …)` 无限循环，`destroy` 无限 free 直到崩。编译器一个警告都不会给，只能靠画图。

2. **`free` 之后才读 `cur->next`**：`free(cur); cur = cur->next;` 是 use-after-free。正确写法永远是「先备份 next，再 free」。`make asan` 能稳定抓到这类 bug。

3. **忘了 free 摘下来的节点**：数组栈 pop 只是 `top--`，链表栈 pop 必须真的 `free`。只把 `head` 往后挪不 free，压 10 万个再 destroy 只能救回最后剩下的那些 —— 已 pop 掉的那些永久泄漏。

4. **想当然在尾部 push/pop**：单链表尾部操作是 O(n)，「第 k 次要走 k 步」，压 n 个元素变成 O(n²)。要两端都快就用双向链表，或者干脆按队列的「头出尾进」设计（`04-queue/linked-queue`）。

5. **只传 `LNode *head` 却想改 head**：C 传参永远是拷贝，改不到外面。要么传 `LNode **`，要么把 head 装进结构体传 `LStack *`。判断口诀：**要改谁，就把「谁的地址」传进去。** 改节点内容 → 传节点指针；改指针本身 → 传指针的指针。

6. **不维护 `size`，判空靠遍历**：那 `lstack_size` 就变成 O(n)，「栈所有操作 O(1)」的结论直接被推翻。一个 int 字段的成本极低，一定要维护，而且 `push`/`pop` 两边都要改（只改一边就会漂移，`size` 从此不可信）。

7. **`new_node->next` 忘了置 NULL**：`malloc` 不清零。新尾巴的 `next` 是脏地址，遍历会走飞。`lstack_make_node` 里那句 `node->next = NULL;` 不是可有可无的。

8. **`(*size)++` 的括号漏了**：`*size++` 意思完全不同（指针自增）。二级指针对象上括号就是「语法正确性」的一部分。

---

## 什么时候用它 / 什么时候别用

**适合：**
- 元素数量完全估不出来（比如「读输入直到 EOF，压所有 token」）。
- 不接受任何一次 O(n) 抖动：实时链路、不能超时的处理路径，扩容型数组栈的那一下搬家是隐患。
- 同时存在很多个「都很小」的栈（每个只放几层），预开一整块内存太浪费。
- 教学/练习指针：本节是「谁申请谁 free」和「传谁的地址才能改谁」最好的练兵场。

**不适合：**
- 追求速度：连续内存的数组栈在 cache 上完胜，节点数一多（几十万）差距非常直观。
- 需要「偶尔看一眼整个栈」或「按下标访问」：数组栈一遍 for 就够；链表栈每跳一次指针都可能 cache miss。
- 内存紧张且元素是小整数（比如 int）：每个节点要额外背 8 字节指针 + malloc 管理头，实际能到数组栈的 4~8 倍占用。
- 需要「两端都能 O(1) 进出」：那已经是双端队列的活，见 `04-queue/deque`。

---

## 自检问题

1. **为什么栈顶要放在链表头部而不是尾部？**
   答：单链表只有「从 head 往后走」的能力。头部插/删只改 2 个指针 → O(1)；尾部插要先走到最后一个节点、尾部删还要找倒数第二个节点（不能倒退）→ O(n)。第 2 幕实测：第 k 次尾部 push 要访问 k 个节点，压 n 个总开销 O(n²)。

2. **`lstack_push` 里 `new_node->next = s->head;` 和 `s->head = new_node;` 能换顺序吗？为什么？**
   答：不能。先执行 `s->head = new_node` 的话，`s->head` 已经是 `new_node`，再执行 `new_node->next = s->head` 就是让它指向自己 → 自环，遍历和销毁都会失控。

3. **`void f(LNode *head)` 里 `head = malloc(...)` 或 `head = new_node;` 为什么改不到外面？该怎么写？**
   答：C 语言传参永远是拷贝，函数里那个 `head` 是另一个变量，它的值和外面无关。要改「指针本身」就把它的地址传进去：`void f(LNode **head)` 然后 `*head = new_node;`；或者把 `head` 装进结构体，传 `LStack *s` 再 `s->head = new_node;`（本节采用后者，签名更干净，还能顺手带 `size`）。

4. **`pop` 为什么必须 `free` 被摘掉的节点？`destroy` 又为什么必须「先存 next 再 free」？**
   答：数组栈 pop 只是把 `top` 下移，格子还在；链表栈的节点是自己 `malloc` 的堆内存，不 `free` 就是永久泄漏。`destroy` 里若写成 `free(cur); cur = cur->next;`，读 `cur->next` 时那块内存已归还，属 use-after-free，读到的可能是垃圾，链表断裂。

5. **结构体里为什么要多存一个 `size`？不存会怎样？**
   答：为了 O(1) 报元素个数 / 判空。不存就得每次遍历数节点，把「所有操作 O(1)」变成「查询 O(n)」。代价只是一个 int。

6. **链表栈会不会出现「栈满」？**
   答：不会出现数组那种「容量到顶」的满；只有 `malloc` 返回 NULL（堆真的不够）才失败，这时 `lstack_push` 返回 false 并且栈保持原样不被破坏。
