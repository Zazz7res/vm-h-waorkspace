# 动态内存 dynamic-memory —— malloc / calloc / realloc / free 的实弹演习

> 上一节 `pointer-basics` 讲了「堆是租来的仓库」，这一节真的去租：
> 三种要内存的方式、一次可能搬家的扩容、一次送出去的指针交接、一次 use-after-free 的安全围观，
> 外加一场**故意泄漏**给 ASan 抓。每条结论都有真实输出可核对。

---

## 一句话是什么

栈上的内存归编译器管，**堆上的内存归你管**：`malloc` 按字节租、`calloc` 租完还帮你打扫、
`realloc` 换大小（可能搬家）、`free` 还。谁申请、谁负责还；不还就是泄漏；还完再用就是悬空指针。
还有一条铁律贯穿全仓库：函数想改调用者的指针，参数就得是**指针的指针**（`Node **`）。

## 为什么需要它（不用它会怎样）

栈解决不了两件事：

1. **大小编译时不知道。** `int data[n]` 里 n 是运行时才读进来的；链表今天 3 个节点、明天 4000 个。
   栈上的数组要么写死上限、要么（VLA）一超载就爆栈——堆上想租多大租多大（直到内存不够，那时返回 NULL）。
2. **活不过函数返回。** `pointer-basics` 里的反面教材 `int *wrong() { int local[10]; return local; }`：
   函数一返回栈帧就回收，交出去的是「已被保洁收走的工位」；堆内存**不 free 就一直在**，
   所以链表节点、动态数组、树的每个结点全都 malloc 在堆上，才能跨函数、跨 return 继续活着。

不用它会怎样——具体到本节：**没有 malloc 就没有链表**。每个节点要单独存在、单独摘除，
数组那种「一块连续内存 + 下标」根本表达不了「插入/删除时邻居关系会变」。
而且不还内存的程序会越吃越肥；`realloc` 不接收返回值会丢整块数据——这些坑本节全都有实测。

## 图解（ASCII 图 + 内存/结构布局）

第 1 幕跑完后，堆上真实长这样（本机实测地址，三块紧挨着，每块 20 字节）：

```
  0x59e4043ff020  ┌────┬────┬────┬────┬────┐   m1 = arr_new_malloc(5, 42)   ← malloc 不清零，42 是自己填的
  0x59e4043ff040  ├────┼────┼────┼────┼────┤   c1 = arr_new_calloc(5)       ← 全 0
  0x59e4043ff060  ├────┼────┼────┼────┼────┤   ms1 = malloc + memset(0)     ← 全 0
                  └────┴────┴────┴────┴────┘
  free(m1); free(c1); free(ms1);   ← 归还。指针变量本身还在栈上，里面的数字也还在，但已不作数
```

「malloc 不清零」实测（第 2 幕）：6 格数组涂满 777、free，再 malloc 同样大小——堆管理器只管发还、不管擦洗，大概率把刚收回的那块原样再发给你：

```
   dirty_block_setup:  malloc → 写满 777 → free        arr_new_malloc_untouched: 再 malloc(6)
   ┌──────────────┐                                  ┌─────────────────────────────┐
   │777 777 777 …│ ─── 归还（内容原样留着）──→        │ 脏值/别人用过的字节，随机混合 │
   └──────────────┘                                  └─────────────────────────────┘
   本机这次打印：[-1702906945 23009 0 0 777 777]  ← 两个 777 是上一任主人留下的
```

`realloc` 的三种结局（第 3 幕都会遇到）：

```
结局 A 原地扩大：后面正好有空地        结局 B 搬家：后面被占了               结局 C 失败：找不齐
 [11 12 13 14 ▁▁ ▁▁ ▁▁ ▁▁]            [11 12 13 14] ←当场作废(free)        返回 NULL
 ↑ 地址不变，皆大欢喜                   [11 12 13 14 104 105 106 107] ←新地址  ↓
                                     老数据被整块抄过去，旧指针读=悬空        原 arr 原封不动，仍然有效
```

地址的「值」和地址背后的「所有权」是两回事——这就是 `array_resize` 里 `uintptr_t` 的意义：

```
   uintptr_t old_addr = (uintptr_t)arr;   ← 存的是一串十六进制**数字**，不是指针
   打印它 ✓  拿它和 moved 比 ✓  解引用它 ✗（压根没打算）
   若存成 void*：realloc 搬家时旧块已被释放，再打印/比较会吃 -Wuse-after-free 警告；
   存成整数，编译器的「指针来源追踪」就无从指控——因为我们从头到尾没碰过那块内存。
```

指针交接（第 4 幕，两种写法的岔路）：

```
 main:  head 住在 0x7ff...a88，里面存 0x...040 ──→ [10] → [11] → [12] → 尾
 push_front_broken(Node *head)：  参数 head 是**拷贝**（住在别的格子）
     fresh->next = head;  head = fresh;   ← 只改了拷贝那份；返回后蒸发，main 的头纹丝不动
 push_front_ok(Node **head)：     参数 head 里装的是 0x7ff...a88 这个**地址**
     fresh->next = *head;   ← 第 1 步：新节点先接上老链（先接）
     *head = fresh;         ← 第 2 步：往 main 的 head 格子里写新地址（后断）
     口诀「先接后断」：反过来的话老链的头先被覆盖，fresh->next 就没值可拿了
```

谁申请谁负责的完整链条（本节的「责任交接」）：

```
  build_list() ──malloc×3──→ 造好链，把 head **交出去**（return）
  main         ──持有──→ 用（push/print/pop）──→ 逐个 free（pop_front_ok）或整条 free_list 还干净
  push_front_broken 的 free(fresh)：函数造的节点送不出去，只能返回前自行销毁——
  这一 free 就是「白干一场」的自供状，不 free 它就是泄漏清单上多一条。
```

## 逐个操作拆解（对应 dynamic-memory.c 里每个函数）

### `print_int_array(tag, arr, n)` —— 看内存里到底装了啥

n 必须单独传：数组一退化成指针，长度信息就没了（`pointer-basics` 讲过）。顺手打出 `地址=%p`，
本节所有「搬家/回收/悬空」的证据都靠它。NULL 进来不崩，打一行「内存没申请到」。

### `arr_new_malloc(n, fill)` —— 按字节租，内容自理

三个细节都在代码里：参数单位是**字节**，所以写 `(size_t)n * sizeof(int)`——先转 size_t 再乘，
防 n 很大时 int 乘法溢出；失败返回 NULL **必须判空**；malloc 不清零，内容自己填。
O(n)（填值），申请本身摊销 O(1)。

### `arr_new_calloc(n)` —— 两个参数 + 自动清零

`calloc(count, size)` 把个数和每个大小**分开传**，顺手帮你检查乘法溢出，然后把整块填 0。
什么时候必须它：计数表、哈希桶、并查集父数组这类「初值 0 就是算法一部分」的场合。
代价：清零 O(n)。很多平台上 calloc 就是 malloc+memset，本节用第三种写法验证「清零没有魔法」。

### `arr_new_malloc_memset(n)` —— 手工清零，效果等同 calloc

`memset(arr, 0, bytes)` 第二个参数是**字节值**：只能安全清零（0 按字节铺还是 0），
想全填 1 用 `memset(arr, 1, …)` 得到的是每字节 0x01 → `0x01010101` = 16843009，不是 1。

### `dirty_block_setup()` + `arr_new_malloc_untouched()` —— 亲眼看不清零

涂脏 → free → 再 malloc 同样大小，大概率捡到同一块脏内存。代码注释里说得诚实：
**这个现象依赖实现**，要记住的结论不是「一定看到脏值」，而是
「你绝对不能依赖 malloc 给你的初值，读到什么都有可能」。

### `array_resize(arr, new_n)` —— realloc 的正确包装

三种结局的处理全在这个函数里：原地扩大打【原地扩大，地址没变】；搬家打【整块搬走了！旧指针当场作废】；
失败（NULL）**返回原指针**——原内存依然有效、没泄漏，调用者照样接收返回值就行。
正因为失败时它还返回 arr，main 里 `grown = array_resize(base, 8)` 这种「自赋值」写法才是安全的；
裸写 `arr = realloc(arr, bytes);` 且不判空，一旦结局 C，arr 被 NULL 覆盖，老块再也找不回——泄漏。
复杂度：结局 A 摊还 O(1)；结局 B 要整块抄数据 O(n)；结局 C 失败返回。

### `push_front_broken(Node *head, int val)` —— 反面教材

malloc 新节点、接上老链、把**拷贝**的头指针改成新节点——函数里打印真的多了一个节点，
可返回后 main 的头纹丝不动；fresh 外面谁也拿不到，注定泄漏，所以返回前 `free(fresh)`。
注释特意说明：free 它不伤老链——老链由 main 自己的 head 持有（这就是「谁的内存谁 free」）。

### `push_front_ok(Node **head, int val)` —— 二级指针正解

`fresh->next = *head; *head = fresh;` 两句话，第二句的星号 = 直接往 main 的 head 格子里写。
为什么必须 `Node **`：想改 main 的 head 就要拿到 head 的地址，而 head 本身是指针，它的地址就是指针的指针——链表/树的插入删除接口全是 `Node **`，根子就在这。O(1)。

### `pop_front_ok(Node **head, int *value_out)` —— 「头指针能变 NULL」的铁证

三步：记下 gone → 头指针挪到下一个（链里只剩一个节点时这里就写成 NULL）→ free gone。
一级指针版本连「置 NULL」都做不到，main 会留一个悬空头指针。边界：`head == NULL` 或 `*head == NULL` 都返回 -1，
**绝不 free 任何东西**；`value_out` 允许 NULL（输出参数约定，和 pointer-basics 同款）。O(1)。

### `build_list / list_length / print_list / free_list` —— 标准四件套

build_list 用尾插（「记住尾巴往后接」），让打印顺序和数字顺序一致；中途申请失败会
`free_list(head)` 把手上的半成品全还回去再返回 NULL——**失败路径也要还内存**。
list_length/print_list 都让 cur 去走、head 原地不动。print_list 给每个节点打 `[值|next地址]`，
尾节点打 `[值|尾]`。free_list 的顺序是唯一的正确姿势：

```
   Node *next_to_keep = cur->next;   ← ① 先抄下退路
   free(cur);                        ← ② 再拆当前
   cur = next_to_keep;               ← ③ 前进（读的是抄下来的，不是已归还的）
```

写成 `free(cur); cur = cur->next;` 就是 use-after-free（读刚归还单元里的 next）。四个函数都是 O(n)。

### `leak_demo()` —— 故意漏两块，藏在命令行参数后面

malloc 100 个 int（400 字节）和一个 Node（16 字节），都不 free，共 416 字节。普通编译跑：正常退出、零提示——泄漏就是这么阴；
ASan 编译跑：退出时红字报泄漏 + 完整申请调用栈。它只在 `./二进制 leak` 时才执行，因为本仓库的硬规矩是**每个 demo 默认必须干净通过**。

## 复杂度一览

| 操作 | 时间 | 备注 |
|---|---|---|
| `malloc` / `free` | 摊销 O(1)（实现相关） | 系统级分配（brk/mmap）会偶发变慢 |
| `calloc` / `memset` 清零 | O(n) | 省不掉的就是「打扫」这遍 |
| `realloc` 原地 / 搬家 / 失败 | O(1) / O(n) / O(1) | 哪种结局**不由你选** |
| `push_front_ok` / `pop_front_ok` | O(1) | 头部操作是链表的看家本领 |
| `build_list` / `free_list` | O(n) | 每节点一次 malloc/free；length/print 也是 O(n) |

## 运行结果

复现（项目根目录）：

```bash
make one DIR=00-c-foundation/dynamic-memory
```

节选真实输出（共 110 行）——第 2、3、4 幕的关键段落：

```text
      先申请 6 格涂满 777，地址 0x59e4043ff060，然后 free 掉（归还时不擦洗）
      再次 malloc(6 格)，拿到 0x59e4043ff060 —— 里面的值见下方打印，全是垃圾
    第二次 malloc 的内容（未写过一个字节） 地址=0x59e4043ff060: [-1702906945 23009 0 0 777 777]  6 格
      malloc(16 字节) 拿到 0x59e4043ff060，然后手工填成 0
    扩容前         地址=0x59e4043ff060: [11 12 13 14]  4 格

  第 3a 步：从 4 格扩到 8 格（只多 16 字节）
      realloc: 旧地址 0x59e4043ff060  ->  新地址 0x59e4043ff380   【整块搬走了！旧指针当场作废】   (8 格 = 32 字节)
    扩到 8 格      地址=0x59e4043ff380: [11 12 13 14 104 105 106 107]  8 格

  第 3b 步：再从 8 格扩到 20000 格（要多 80000 字节）—— 后面被占了，只能搬家
      realloc: 旧地址 0x59e4043ff380  ->  新地址 0x59e4043ff380   【原地扩大，地址没变】   (20000 格 = 80000 字节)
  搬家后前 8 格的内容（realloc 已经帮你把老数据整块抄过来了）：
    扩到 20000 格   地址=0x59e4043ff380: [11 12 13 14 104 105 106 107]  8 格

  这次运气好，原地扩大了，地址没变（不能指望运气，下面按搬家讲）
  调用前 main 里 head = 0x59e4043ff040
      [函数内部] 我以为链表变成: [99|0x59e4043ff040] -> [10|0x59e4043ff020] -> [11|0x59e4043ff060] -> [12|尾]   共 4 个节点
  调用后 main 里 head = 0x59e4043ff040   ← 没变，新节点根本没进来（函数里的成功是拷贝上的成功）
  调用后 main 里 head = 0x59e4043ff380   ← 变了！新节点真的成了头
  弹出 99，此刻 main 里 head = 0x59e4043ff040
  弹出 10，此刻 main 里 head = 0x59e4043ff020
  弹出 11，此刻 main 里 head = 0x59e4043ff060
  弹出 12，此刻 main 里 head = (nil)
      [空链] 没有节点可弹
  head == NULL ? 是（链已空，所有节点都已归还）  ← 一级指针版本永远做不到这一步，main 会留一个悬空头指针
```

逐段读：**第一段**——free 后马上再 malloc 同样大小，捡到的正是刚还的 0x...060；六个格子里
既有 `-1702906945`、`23009` 这种别的用途残留，也有原样躺着的两个 `777`。看到什么全凭堆管理器心情，
所以结论只有一条：初值不可信。**第二段**——最有教学价值的一幕：3a「只多 16 字节」的小扩容
**反而搬家了**（060→380），3b「多 80000 字节」的大扩容**反而原地扩大**（380→380）。
和直觉相反——3b 那行「后面被占了，只能搬家」是提前写死的预期，被 reality 打脸，main.c 只好自认「这次运气好」。
这恰恰证明：搬家与否**完全由实现决定，不可预设**，「永远接收 realloc 返回值」是唯一站得住的写法；
3a 搬家后 `扩到 8 格` 的前四格 `[11 12 13 14 …]` 就是老数据被整块抄过去的证据。
**第三段**——broken 版函数内部看着 4 个节点，main 的 head 调用前后都是 0x...040，纹丝不动；
ok 版之后 head 变成新节点的地址。注意 4b 的新节点拿到的正是 0x...380——第 3 幕 free 掉的大块
被捡走了，堆的「回收再发」又一次现场出镜。最后弹出四连，head 一格一格退到 `(nil)`。

泄漏演示（默认跳过；上面第 6 幕会打印这条命令）：

```bash
gcc -Wall -Wextra -Wshadow -std=c11 -g -fsanitize=address -static-libasan 00-c-foundation/dynamic-memory/*.c -o /tmp/dm && /tmp/dm leak
```

ASan 实测把「一共故意漏掉 416 字节 / 2 次分配。」抓成了这样（节选，程序退出码变为 1）：

```text
==33604==ERROR: LeakSanitizer: detected memory leaks
Direct leak of 400 byte(s) in 1 object(s) allocated from:
Direct leak of 16 byte(s) in 1 object(s) allocated from:
    #1 0x6502d0fca7f9 in leak_demo 00-c-foundation/dynamic-memory/dynamic-memory.c:417
SUMMARY: AddressSanitizer: 416 byte(s) leaked in 2 allocation(s).
```

400 + 16 = 416，和程序自己报的字节数严丝合缝；完整版报告里每条泄漏都带申请时的调用栈
（哪个函数、哪一行 malloc 的）。其余输出：第 1 幕三兄弟对比表、第 5 幕 free 三铁律 +
`free(NULL)` 实测无事、结尾「记住五句话」。

## 常见的坑

1. **`malloc(n)` 当成 n 个元素**。
   错法：`int *a = malloc(n);`——n 字节只够 n/4 个 int。
   对法：`malloc((size_t)n * sizeof(int))`，先转 size_t 再乘防溢出。
2. **申请后不判空**。
   错法：拿到 NULL 直接 `a[0] = 1`，解引用空指针当场崩。
   对法：`if (a == NULL) return/handle;`——大申请真会失败，本节每个函数都这么写。
3. **裸 `p = realloc(p, n)` 且不接失败分支**。
   错法：结局 C 时 NULL 覆盖 p，原块地址永久丢失（块还活着，但没人能找到它）= 泄漏。
   对法：临时指针接，失败时原 p 照常可用；或像本节把它包进 `array_resize`，失败返回原指针。
4. **realloc 搬家后继续用旧指针**。
   错法：取消 main.c 里注释掉的那行 `printf("%d", before_move[0]);`——读已归还内存（ASan 报 heap-use-after-free），
   或 `free(before_move)`（非法释放，当场 abort）。对法：只认返回值；想留旧地址做对比，存成 `uintptr_t` 纯数值，只看不碰。
5. **free 之后接着用**。
   错法：`free(a); a[0] = 1;` 或 `free(cur); cur = cur->next;`——悬空指针，可能当场崩、可能「看起来没事」某天诡异出错。
   对法：free 完立刻置 NULL；删链先抄 next 再 free。
6. **double free**。
   错法：`free(p); free(p);` 破坏堆管理器。对法：free 后置 NULL，`free(NULL)` 是合法空操作
   （第 5 幕实测），置 NULL 一次挡住误用和重删两个坑。
7. **memset 清零以外的东西**。
   错法：`memset(a, 1, n*sizeof(int));` 想全填 1，得到 16843009。
   对法：填 0 用 memset/calloc，填别的值就老老实实循环。
8. **依赖 malloc 的初值**。
   错法：看到本机 malloc 回来是全 0 就当约定。对法：垃圾值合法存在（本节实测捡到 777 和
   -1702906945），要么自己填、要么 calloc。

## 自检问题

1. `calloc(10, sizeof(int))` 和 `malloc(10 * sizeof(int))` 差在哪两个具体行为上？
   **答：** calloc 两参分开传 + 顺手检查乘法溢出 + 自动清零；代价就是清零那遍 O(n)。
2. `realloc` 失败时，原来的内存还在吗？此时该 free 它吗？
   **答：** 在且完全有效；不该 free 也不该丢——本节 array_resize 正是此时把原指针原样还回去。
3. 为什么 `array_resize` 里存旧地址用 `uintptr_t` 而不是 `void *`？
   **答：** 搬家成功时旧块已被释放，拿旧 void* 打印/比较会触发 -Wuse-after-free；整数只是地址数值，从不解引用。
4. `push_front_broken` 里那个 `free(fresh)` 为什么不会把老链拆散？
   **答：** 老链由 main 自己的 head 持有，fresh 是「没接进任何人世界」的新节点，只属于本函数。
5. 为什么删头必须写成 `Node **`？一级指针版最坏的下场是什么？
   **答：** 头指针本身（包括置 NULL）要被改；一级版改不到外面，main 留一个悬空头指针继续被遍历。
6. 「先接后断」在本节出现在哪几处？
   **答：** push_front_ok 先接 next 再改头；pop_front_ok 先挪头最后 free；free_list 先抄 next 再拆当前。
7. `int local[1000000];` 在函数里为什么危险？malloc 回来就没事了吗？
   **答：** 栈小且函数返回即销毁，百万级容易爆栈；堆上能拿到，但必须判空 + 负责 free。
8. 为什么 `leak_demo` 要藏在命令行参数后面？
   **答：** 仓库规矩是默认运行必须干净通过；泄漏是设计好的演示，只在带参数 + ASan 时登场。

## 下一步

- 前置：`00-c-foundation/pointer-basics`（值拷贝 vs 传地址、输出参数约定）、
  `00-c-foundation/struct-and-typedef`（本节 `Node` 的形状就是它教的）。
- 接下来 `01-array/dynamic-array`：把本节 `array_resize` 升级成真正的动态数组，
  顺便算清「每次 ×2 扩容」的摊还账（那时 calloc 式全量拷贝的成本要开始心疼了）。
- 再往后 `02-linked-list`：`Node **` 接口、逐节点 malloc、free_list 会天天见面；
  `05-tree`、`07-graph` 建点建表同样建立在这四件套上——这一节的规矩不熟，后面每一步都在裸奔。
- 工具习惯：写完任何数据结构跑一遍 `make asan`（或 `make check`，零告警 + ASan 双保险）；
  泄漏报告里的行号会直接告诉你哪次 malloc 没还。
