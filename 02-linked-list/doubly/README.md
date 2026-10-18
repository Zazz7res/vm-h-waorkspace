# 双向链表 doubly linked list（dlist）—— 多一根 prev，把"删已知节点"变成 O(1)

> 前置：`02-linked-list/singly/`（单链表）与 `00-c-foundation/pointer-basics/`（想改外面就传地址）。
> 这一节只比单链表多一个字段，却治好了单链表两个最疼的病：**尾插要跑一整趟** 和 **删节点必须先找前驱**。

---

## 一句话是什么

双向链表就是**每个节点多揣一根 `prev` 纸条，指回上一个节点**。

```c
typedef struct Node {
    int val;
    struct Node *prev;   /* 指向上一个节点；我是头时它是 NULL */
    struct Node *next;   /* 指向下一个节点；我是尾时它是 NULL */
} Node;
```

就多了这一根线，换来三件事：往前往后**都能走**；手里已经有节点地址时**删除是 O(1)**（不用从 head 走一趟找前驱）；
再加上把 `head / tail / size` 打包进 `DList` 把手，**尾插也变成 O(1)**。
代价是每节点多一个指针、每次接线都要**成对维护**（改一根不改另一根，链表就会裂开）。

---

## 为什么需要它

| 需求 | 单链表 | 双向链表 |
|---|---|---|
| 尾插 | O(n)：每次从头走到尾 | **O(1)**：`tail` 一直攥在手里 |
| 删「已经拿到地址」的节点 | O(n)：必须遍历找前驱 | **O(1)**：`node->prev` 就写在节点身上 |
| 倒着遍历（反向打印/反向查找） | 做不到，只能重走一遍或存进数组 | **O(n) 直接倒着走** |
| 实现deque / LRU 缓存 / 浏览器前进后退 | 别扭 | 天生对口 |
| 内存 | 节点 16 字节 | 节点 24 字节（`.c` 注释：8+8+4=20，对齐后 24） |

典型应用（`.c` 注释里点名的）：LRU 缓存里"把一个已知节点提到队头"、Linux 内核的 task list。

---

## 图解

### 1. 两两互指：每根 `next` 都有一根反向的 `prev`

```
     head                                          tail
      │                                               │
      ▼                                               │
  ┌───────┐   next    ┌───────┐   next    ┌───────┐  │
  │   5   │──────────►│  10   │──────────►│  20   │──┘
  └───────┘           └───────┘           └───────┘
      ▲                   ▲   │               ▲   │
      │        prev ◄─────┘   │     prev ◄────┘   │
      │                       │                   │
    NULL                    NULL                 NULL
  head->prev == NULL                     tail->next == NULL
  （这两个 NULL 就是本节全部的"边界信号"，后面删除全靠它们当哨兵）
```

### 2. 头插三步（`dlist_push_front`）：先接新线，再挪把手

```
  ① n->next = head          [9] ──► [5] ──► [10] ──► [20] ──► NULL
                               ▲       ▲
                              new     head（还指着老的头）

  ② head->prev = n            [9] ◄──► [5] ──► [10] ──► [20] ──► NULL
     ⚠ 空表时没有"老的头"可指 → 走 else：tail = n（新节点同时是头和尾）

  ③ head = n                head [9] ◄──► [5] ◄──► [10] ◄──► [20] ──► NULL
                            （tail 一步都不用动！）        ④ size++
```

尾插 `dlist_push_back` 是它的镜像：`n->prev = tail;` → `tail->next = n;` → `tail = n;` → `size++`，
空表时 else 分支改成 `head = n`。

### 3. 摘掉一个节点只要两行接线（`dlist_remove`）

```
  之前：  NULL ◄── [5] ◄──► [10] ◄──► [20] ──► NULL
                       ▲        │        ▲
                       └────────┴────────┘
     node->prev->next = node->next;    /* 前驱跨过我要删的节点，直接牵住后继 */
     node->next->prev = node->prev;    /* 后继回头牵住前驱 */
  之后：  NULL ◄── [5] ◄──────► [20] ──► NULL              然后 free(node); size--;
```

而"我是头 / 我是尾"这两种特殊情况**不需要造 dummy 节点**，`NULL` 本身就是哨兵：

```
   node->prev == NULL  →  我是头：该改的是 list->head（不是某个前驱的 next）
   node->next == NULL  →  我是尾：该改的是 list->tail
   两个 else 同时命中  →  我是唯一节点：head = tail = NULL，链表正好变空
   两个 if  都成立     →  普通节点：前接后、后接前
   ── 四种情况被两支 if/else 全部包干，这就是双向链表删除写得比单链表短的原因。
```

---

## 逐个操作拆解（对应 `dlist.c` 里每个函数）

### 0. `make_node()` —— 两根纸条都必须显式置 NULL

```c
    n->val  = val;
    n->prev = NULL;
    n->next = NULL;
```

`malloc` 给的是**未初始化**的堆内存，不置 NULL 就拿到垃圾地址；这条链上一个野指针就够让遍历当场崩。
它是 `static`，只在本文件用，调用者拿不到"半成品节点"。

### 1. `dlist_init()` / `dlist_free()` —— 空表的定义必须三个字段同时成立

空表 = `head == NULL && tail == NULL && size == 0`。少设一个，后面每个函数的边界判断都会失真。
`dlist_free` 还是那条铁律：**先抄 `next` 再 free**：

```c
        Node *nxt = cur->next;      /* ① 先抄住下一个 */
        free(cur);                  /* ② 再还掉当前 */
        cur = nxt;                  /* ③ 走到抄下来的地址 */
```

结尾把 `head/tail/size` 一起复位，所以再调一次 `dlist_free` 只是空转（`free(NULL)` 合法）——
第 6 幕末尾就是专门演示这一句的。
⚠ 反过来说：对**还挂着节点**的链表再调一次 `dlist_init` 是灾难 —— 见「常见的坑」第 5 条。

### 2. `dlist_push_front()` —— 三步接线 + 空表特判

```c
    n->next = list->head;             /* ① 新节点往后牵住老的第一节点（空表时这里是 NULL，正好） */

    if (list->head != NULL) {         /* ② 老的第一节点要回头指我 —— 但空表时没有"老的第一节点" */
        list->head->prev = n;
    } else {
        list->tail = n;               /* 空表：新节点既是头也是尾，tail 必须一起设 */
    }

    list->head = n;                   /* ③ 头把手挪到新节点 */
    list->size++;                     /* ④ 记账 */
```

顺序很关键：**先接好新线，再挪把手**。`head = n` 一旦执行，老头的地址就只能从 `n->next` 拿了。
那个 `else` 分支不是装饰：忘了设 `tail`，之后的尾插就会去改一个 `NULL` 的 `->next`，当场崩。

### 3. `dlist_push_back()` —— 单链表的痛在这里被治好

结构和 `push_front` 完全镜像（`n->prev = tail;` / `tail->next = n;` / `tail = n;` / `size++`），
空表时 `else` 里补 `list->head = n;`。为什么它是 O(1) 而单链表是 O(n)：
单链表每次尾插都要 `while (cur->next != NULL)` 从头走到尾，而这里 `tail` 一直攥在手里。

### 4. `dlist_insert_after()` / `dlist_insert_before()` —— 五根线，顺序不能乱

```c
    n->prev = pos;                    /* ① */
    n->next = pos->next;              /* ② 先抄住 pos 后面的老节点（★pos->next 后面会被覆盖） */

    if (pos->next != NULL) {
        pos->next->prev = n;          /* ③ 老后继回头改指新节点 */
    } else {
        list->tail = n;               /* pos 本来是尾巴 → 新节点变成尾巴 */
    }

    pos->next = n;                    /* ④ 最后才改 pos->next（旧值已在②抄走了） */
```

`insert_before` 是镜像版：`n->next = pos;` → `n->prev = pos->prev;` → 改老前驱的 `next`（没有老前驱就 `head = n`）
→ 最后 `pos->prev = n;`。两处 **`else` 分支就是"插到了端点"**，把手必须跟着挪。
`pos == NULL` 时本节选择**明确拒绝**并打印 `[错误] insert_after：pos 是 NULL，没法在它后面插`：
要么当头插要么报错，"猜调用者的意思"是 bug 的温床。

### 5. `dlist_remove()` —— 本节最大卖点

```c
    if (node->prev != NULL) {
        node->prev->next = node->next;   /* 前驱跨过我要删的节点，直接牵住后继 */
    } else {
        list->head = node->next;         /* 我是头：新的头 = 我的后继（可能为 NULL） */
    }

    if (node->next != NULL) {
        node->next->prev = node->prev;   /* 后继回头牵住前驱 */
    } else {
        list->tail = node->prev;         /* 我是尾：新的尾 = 我的前驱（可能为 NULL） */
    }

    free(node);                          /* 内存一定要还；注意这行之后 node 已经是野指针 */
    list->size--;
```

为什么单链表删节点非 O(n) 不可：要摘掉 X 必须改 **X 前驱** 的 `next`，而节点不知道前驱是谁，只能从 head 走一趟。
双向链表 `X->prev` 明晃晃写在节点里，前驱 O(1) 到手，改四根线就完事。
`main` 第 4 幕把「普通 / 头 / 尾 / 唯一」四种都跑了一遍，输出见下面。

### 6. `dlist_remove_by_val()` —— 慢在【找】，不慢在【删】

```c
    Node *cur = dlist_find(list, val);   /* 找：O(n) —— 这一步单双向都躲不掉 */
    if (cur == NULL) { ... return 0; }
    dlist_remove(list, cur);             /* 删：O(1) —— 双向链表的真正优势 */
```

这个对比很关键：双向链表**并没有让查找变快**，它只让"我已经知道是哪个节点了，动手"这件事变快。
语义上有两条约定：多个匹配只删最靠前那个（第 5 幕用重复值 2 演示）；找不到就打印提示并原样返回。

### 7. `dlist_pop_front()` / `dlist_pop_back()` —— 复用 `remove`，值要抢在 free 前抄

```c
    Node *old = list->head;
    if (val_out != NULL) {
        *val_out = old->val;             /* ★ free 之前先把值抄出来 */
    }
    dlist_remove(list, old);             /* 复用上面的 O(1) 删除：head 会被自动更新 */
```

`val_out` 是输出参数（主结果走 `return`，值写进调用者给的地址，传 `NULL` 表示"值我不要"）。
空表直接返回 0，**绝不读 `head->val`**；第 1 幕演示了这点：`ghost` 保持原值 -1。

### 8. `dlist_find()` / `dlist_find_from_tail()` —— 双向的"双向"就在这里

```c
    for (Node *cur = list->head; cur != NULL; cur = cur->next) {   /* 往后走 */
    for (Node *cur = list->tail; cur != NULL; cur = cur->prev) {   /* 往前走 */
```

起始点和步进**同时换掉**，就是这两行差别。什么时候反向更快：你大致知道目标在**后半段**
（例如 LRU 缓存，新东西都堆在尾巴上）。空表时 `head/tail` 就是 NULL，循环条件第一次就为假 —— 天然安全。

### 9. `dlist_size()` / `dlist_print_forward()` / `dlist_print_backward()`

`dlist_size` 是 `O(1)`：那是每次增删**维护出来的账**，不是现场数出来的（所以漏一次 `size++` 就永久错）。
正向打印用 `printf(cur->next != NULL ? " <=>" : " =>");` 把"最后一格后面是 `tail->next == NULL`"画出来；
空表单独印一行 `NULL <=> NULL   (空表)`，因为循环一次不进时只印 "NULL" 分不清是"空"还是"忘了判边界"。
反向打印从 `tail` 一路 `cur = cur->prev`，**印出来是倒序的** —— 那就是 prev 真接好了的铁证。
两个函数的参数都是 `const DList *list`：承诺只读不改，写错了编译器立刻提醒。

---

## 复杂度

`n` = 节点数。括号里是单链表同操作的代价，方便对比。

| 操作 | 双向链表 | 单链表 | 为什么 |
|---|---|---|---|
| `push_front` | **O(1)** | O(1) | 都只动 head |
| `push_back` | **O(1)** | O(n) | 有 `tail` 把手，不用走到尾 |
| `insert_after/before` | **O(1)**（拿到 pos 之后） | O(n) | 五根线，端点时挪把手 |
| `remove`（节点地址已知） | **O(1)** | O(n) | `node->prev` 直接给前驱 |
| `remove_by_val` | O(n) | O(n) | 慢在【找】，删本身 O(1) |
| `pop_front` / `pop_back` | **O(1)** | O(1) / O(n) | 单链表 pop_back 要找倒数第二个 |
| `find` / `find_from_tail` | O(n) | O(n) | 双向省不了比较次数，只省方向 |
| `size` | **O(1)** | O(n) | 记账字段 |
| `print_forward/backward` | O(n) | O(n) / 做不到 | 反向走是双向独有 |
| `free` | O(n) | O(n) | 必须逐个还 |
| 每节点空间 | 24 字节 | 16 字节 | 多一个 `prev` 指针 |

---

## 真实运行输出

下面是 `make one DIR=02-linked-list/doubly` 这次跑出的**原文节选**（全量 110 行，挑了三段，逐字未改）。

第 2 幕 —— 两头插 + 反向遍历：

```text
  尾插 20（空表→单节点）: NULL <= [20] => NULL
    此时 head 和 tail 指的是同一个节点
  尾插 30:                 NULL <= [20] <=> [30] => NULL
  头插 10:                 NULL <= [10] <=> [20] <=> [30] => NULL
  头插 5:                  NULL <= [5] <=> [10] <=> [20] <=> [30] => NULL
  尾插 99:                 NULL <= [5] <=> [10] <=> [20] <=> [30] <=> [99] => NULL
  size = 5（是每次增删维护出来的账，不用现场遍历）

  ★ 反向遍历（从 tail 出发一路 prev 往回走）：
  [99] <- [30] <- [20] <- [10] <- [5]  <- NULL   (再往 prev 走就出表了)
  → 打印出来是倒序的，这就是 prev 那根线真的接好了的铁证。
```

第 4 幕 —— 中间 / 头 / 尾各删一次，把手自动更新，然后 `node = NULL` 被拒绝：

```text
  删【中间节点 30】(dlist_remove，找它才是 O(n)，动手 O(1)):
  结果:                  NULL <= [1] <=> [5] <=> [10] <=> [20] <=> [25] <=> [35] <=> [99] <=> [200] => NULL
  删【头节点】(head 自动更新):
  结果:                  NULL <= [5] <=> [10] <=> [20] <=> [25] <=> [35] <=> [99] <=> [200] => NULL
  删【尾节点】(tail 自动更新):
  结果:                  NULL <= [5] <=> [10] <=> [20] <=> [25] <=> [35] <=> [99] => NULL
  现在 head->val = 5, tail->val = 99, size = 6

  故意传 node = NULL：
      [错误] remove：链表或节点是 NULL，什么都不做
```

第 5 幕 —— 按值删（只删最靠前那个）与两头弹出，一直弹到空表：

```text
  pop_front 弹出 1:     NULL <= [3] <=> [2] <=> [4] => NULL
  pop_back  弹出 4:     NULL <= [3] <=> [2] => NULL
  pop_back 弹出不取值:   NULL <= [3] => NULL
  size = 1

  一直弹到空：
    弹出 3 → NULL <=> NULL   (空表)
  空表时 head = NULL, tail = NULL, size = 0
```

同一次运行里另外几条（第 1 幕边界、第 3 幕把手挪动、第 6 幕收尾）：
`pop_front 返回 0（0=空表弹不出），ghost 仍是 -1`、
`  验证 tail 确实跟着挪了：tail->val = 200，head->val = 1`、
`      [错误] insert_after：pos 是 NULL，没法在它后面插`、
`  → 两个函数返回的是【同一个节点】：指针相等，正确`、
`  再 free 一次（head 已是 NULL，free(NULL) 合法）：安全，不会 double free`。

---

## 常见的坑

1. **接线顺序写反**：`insert_after` 里如果把 `pos->next = n;` 提前，`n->next = pos->next;` 就会抄到 `n` 自己
   → 新节点自指成环，之后任何遍历都是死循环。规矩是**先抄旧值、再覆盖，最后才挪把手**。
2. **忘了空表特判**：`push_front` 少了 `if (list->head != NULL)` 就是在 `NULL` 上写 `->prev`，当场段错误；
   少了 `else { list->tail = n; }` 则更阴 —— 第一个节点插进去时 `tail` 还是 NULL，之后尾插就改到空指针上。
   `push_back` 的 `else { list->head = n; }` 同理：漏了它，`find`/正向打印从 head 出发什么也找不到。
3. **端点插入时忘了挪把手**：`insert_after` 插到尾巴后面（`pos->next == NULL`）必须 `tail = n`，
   `insert_before` 插到头前面必须 `head = n`。忘了不会立刻崩，但 `pop_back`/反向打印会从**错误的端点**出发，
   链表里就有一段再也走不到 —— 而且 ASan 查不出这种**语义**错误。
4. **`remove` 之后继续用那个指针**：函数内部已经 `free(node)`，外面那个变量还留着旧地址 = 悬空指针，
   再用就是 UAF，再 free 就是 double free。`main` 里那行 `mid = NULL;` 就是标准动作（注释：地址必须作废）。
5. **对还挂着节点的链表直接再 `dlist_init`**：`head/tail` 被抹成 NULL，老节点瞬间变成"没有任何指针指向"的孤儿，
   既走不到也 free 不掉 → 泄漏（ASan 报成 **Indirect leak**）。正确姿势是 `main.c` 里的 `reset()`：
   **先 `dlist_free` 再 `dlist_init`**。这是链表最隐蔽的一类泄漏来源。
6. **`free` 顺序错**：`free(cur); cur = cur->next;` 读的是已释放内存。必须 `nxt = cur->next;` 先抄住。
7. **`size` 忘记维护**：它不现场数，漏一次 `++`/`--` 以后每次 `dlist_size` 给出的都是错数，
   而错误还会随增删累积 —— 三个字段（head/tail/size）必须**同步**改，这是"把手"型结构的全部纪律。
8. **递进一个不属于本链表的地址**：`insert_after` 的注释点名了这条 —— 从别的链表（或已 free 的节点）拿来的
   `pos` 会把两条链接错，而且完全静默。工程做法是**只暴露内部 `find` 出来的节点**，别让人随便递地址进来。
9. **想当然以为双向让查找变快**：`find` 还是 O(n)。省掉的只是"找前驱"，不是"找节点"。

---

## 这一节实现到什么程度（诚实说明）

- 节点地址直接由 `find` 暴露给调用者，所以坑 8 是**接口层面固有**的风险，本节只在注释里警告，
  没有做"句柄/index 化"的防护（那需要 index→node 映射，超出本节范围）。
- 没做：反转（单链表那节做了）、去重排序插入、`splice`/整段搬移、带哨兵节点的版本、循环双向
  （那是 `02-linked-list/circular-doubly/`）、`sizeof` 的实测打印（24 字节是 `.c` 注释里的推算，本 main 没印）。
- 没有 `dlist_remove_all(值)`：`remove_by_val` 按约定只删第一个匹配。

---

## 自检问题

1. 为什么双向链表删一个**已知地址**的节点是 O(1)，单链表却必须 O(n)？（「图解 3」+ 拆解第 5 小节）
2. 空表 `push_front` 为什么要走 `else` 分支去设 `tail`？不设会怎样？（拆解第 2 小节）
3. `insert_after` 里 `n->next = pos->next;` 为什么必须在 `pos->next = n;` 之前？（拆解第 4 小节 + 坑 1）
4. 删除的四种情况是怎么被两支 `if/else` 全部覆盖的？为什么不用造 dummy？（「图解 3」末段）
5. `pop_front` 为什么要在 `dlist_remove` 之前把值抄进 `val_out`？（拆解第 7 小节）
6. 对一条非空链表只调 `dlist_init` 会造成什么后果？正确写法是什么？（坑 5）
7. `find_from_tail` 什么时候比 `find` 快？快多少是复杂度级别的吗？（拆解第 8 小节）

---

## 复跑与验证

```bash
# 在项目根目录
make one DIR=02-linked-list/doubly     # 编译 + 跑六幕（本文所有输出行都来自这里）
make asan                              # ASan：野指针 / 泄漏 / double free 全能抓（含 Indirect leak）
```

下一步：`02-linked-list/circular-doubly/`（把两端的 NULL 换成互相指，得到环形）、
`03-stack/linked-stack/` 和 `04-queue/deque/`（`pop_front`/`pop_back` 都 O(1) 之后，两头队列才做得起来）。
