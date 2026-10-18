# 数据结构与算法 · C 语言手写实现

> 目的：**用最小的、能跑通的代码，把数据结构每一章亲手写一遍**，同时把 C 的指针、内存、结构体练熟。
> 不是给你看的算法库，是给你**边跑边学**的教材。每一节都可以单独编译运行，输出把每一步都打印出来。

---

## 三条硬约定（本项目为什么长这样）

| 约定 | 原因 |
|---|---|
| **元素类型全部用 `int`** | 不搞 `void*` 泛型、不搞函数指针比较器。你的注意力应该全在"指针怎么指、结构怎么连"上，而不是类型系统 |
| **注释和文档全中文、不怕详细** | 每个函数上方先讲"这行代码存在的原因"，再讲写法 |
| **每个 demo 必须 `make asan` 干净通过** | 没有内存泄漏、没有越界、没有悬空指针 —— 数据结构最大的 bug 来源 |

---

## 怎么用

```bash
make              # 编译所有章节（自动发现）
make run          # 编译 + 按顺序全部跑一遍
make list         # 列出所有章节路径
make one DIR=02-linked-list/singly        # 只看某一节：编译并运行
make asan         # 用 AddressSanitizer 重编译并运行，专抓内存错误
make status       # 体检：哪一节缺 main.c / 缺 README.md，一眼看清缺口
make check        # 验收闸门：每节 -Werror 编译 + 运行 + ASan，有不干净就退出非 0
make dscheck      # 引文闸门：把每节 README 的「运行结果」围栏拿 build-check 里的二进制当场重跑逐行比
make clean        # 清理编译产物
```

只需要 `gcc` 和 `make`，没有任何第三方依赖。

`make dscheck`（要先 `make check` 建好 `build-check/`）比的是**屏幕文本**，不是行为对错，
它自己的口径写在工具头部注释里，一句话版本：**逐字对上的行数单独报出来**，
折掉的只有四类、每类都单独计数不混进「对上」——行内数字与耗时、行首缩进、表格里的连续空白、
以及三类天然不在 stdout 里的行（怎么跑的命令、作者写的「此处省略」注解、ASan 那一趟的报告）。
另有一张登记簿（`~/.cache/handbook-captures/ds-varying-lines.txt`）收「每跑必变但确实是真跑出来的」行，
规矩是**登记了却在本机命中 0 次 = 工具直接判失败**，所以它不能只写不核。
当前账：56 节、输出围栏 10688 行、对不上 0 行、登记簿 4 条全部命中。

### 想用 CMake？同一批代码，两条构建路线

`CMakeLists.txt` 是**额外**给的一条路，和上面的 `Makefile` 互不影响、可并存：
`Makefile` 是你日常手搓单节用的，CMake 那份则顺便当作**现代 CMake 的可运行范例**
（target-based、零 `file(GLOB)`、56 个 target 各注册一条 ctest 用例）。

```bash
cmake -S . -B build-cmake                    # 配置（产物全在 build-cmake/ 里，不脏源码目录）
cmake --build build-cmake --parallel 8       # 编译全部 56 节
ctest --test-dir build-cmake --output-on-failure   # 全部跑一遍，等价于 make check
cmake -S . -B build-cmake -DDS_WITH_ASAN=ON  # 想挂 ASan 就重配置时打开这个开关
cmake --build build-cmake --target 01-array-dynamic-array   # 只编某一节
```

**推荐学习动作**：先读该节 `README.md` 的「一句话是什么 + 图解」→ 再读 `.h` 看有哪些操作 → 然后对着 `.c` 逐行看注释 → 最后 `make one DIR=...` 跑起来，对照 README 末尾的真实输出。改几个数字自己试一遍，才算真会。

---

## 目录结构

每一节（叶子目录）都是同一套布局：

```
02-linked-list/              ← 一个大类
├── README.md                ← 本类总览 + 几种实现的对比
├── singly/                  ← 一个具体结构，自己独立成目录
│   ├── README.md            ← 详解：图解 / 逐操作拆解 / 复杂度 / 坑 / 真实输出
│   ├── slist.h              ← 只有声明（菜单）
│   ├── slist.c              ← 实现，注释全是大白话
│   └── main.c               ← 演示：把每一步打印出来
├── doubly/
└── ...
```

---

## 学习地图

按顺序学，箭头就是前置依赖。

### 00 · C 语言地基（先看这个，别跳过）
| 目录 | 内容 | 为什么必须先学 |
|---|---|---|
| `00-c-foundation/pointer-basics/` | 值传递 vs 指针传递、`&` 与 `*`、数组退化、`malloc/free` | 后面每一个结构的插入删除都是指针操作 |
| `00-c-foundation/struct-and-typedef/` | `struct`、`typedef`、`->`、嵌套结构、传结构体 | 链表节点、树节点就是「结构体 + 指针」 |
| `00-c-foundation/dynamic-memory/` | `malloc/calloc/realloc/free`、内存泄漏实测、二级指针 | 动态数组扩容、树图建点的核心 |

### 01 · 数组 array
| 目录 | 内容 |
|---|---|
| `01-array/static-array/` | 静态数组：固定长度、O(1) 随机访问、插入删除为什么慢 |
| `01-array/dynamic-array/` | 动态数组 / vector：容量翻倍扩容、摊还 O(1) 尾插 |
| `01-array/two-dimensional/` | 二维数组在内存里到底怎么摆的 |

### 02 · 链表 linked-list
| 目录 | 内容 |
|---|---|
| `02-linked-list/singly/` | 单链表：头插、尾插、按值删、反转（递归+迭代） |
| `02-linked-list/doubly/` | 双向链表：删节点不用找前驱 |
| `02-linked-list/circular-singly/` | 循环单链表：约瑟夫环 |
| `02-linked-list/circular-doubly/` | 循环双向链表 |
| `02-linked-list/skip-list/` | 跳表：链表怎么做到 O(log n) 查找 |

### 03 · 栈 stack
| 目录 | 内容 |
|---|---|
| `03-stack/array-stack/` | 数组实现的栈 |
| `03-stack/linked-stack/` | 链表实现的栈 |
| `03-stack/monotonic-stack/` | 单调栈：下一个更大元素 |
| `03-stack/expression-eval/` | 括号匹配 + 后缀表达式求值（栈的经典应用） |

### 04 · 队列 queue
| 目录 | 内容 |
|---|---|
| `04-queue/array-queue/` | 普通数组队列（会"假满"） |
| `04-queue/circular-queue/` | 循环队列：取模解决假满 |
| `04-queue/linked-queue/` | 链表队列 |
| `04-queue/deque/` | 双端队列 |
| `04-queue/queue-by-stacks/` | 用两个栈实现队列（经典面试题） |

### 05 · 树 tree
| 目录 | 内容 |
|---|---|
| `05-tree/binary-tree/` | 二叉树：前/中/后序（递归+迭代）、层序遍历 |
| `05-tree/bst/` | 二叉搜索树：查找、插入、删除（三种情况） |
| `05-tree/avl-tree/` | AVL 树：四种旋转自动平衡 |
| `05-tree/red-black-tree/` | 红黑树：五条性质 + 插入修复 |
| `05-tree/b-tree/` | B 树：数据库索引为什么用它 |
| `05-tree/huffman-tree/` | 哈夫曼树 + 哈夫曼编码：前缀码压缩 |

### 06 · 堆与优先队列 heap
| 目录 | 内容 |
|---|---|
| `06-heap/binary-heap/` | 二叉堆：上浮 / 下沉，数组下标即父子关系 |
| `06-heap/priority-queue/` | 优先队列 |
| `06-heap/top-k/` | Top-K 问题：堆为什么是标准答案 |

### 07 · 图 graph
| 目录 | 内容 |
|---|---|
| `07-graph/adjacency-matrix/` | 邻接矩阵：判边 O(1)，稀疏图浪费 |
| `07-graph/adjacency-list/` | 邻接表：省空间，遍历邻居快 |
| `07-graph/bfs/` | 广度优先 + 无权最短路 |
| `07-graph/dfs/` | 深度优先：递归 + 显式栈两种 |
| `07-graph/connected-components/` | 连通分量、判环 |
| `07-graph/topological-sort/` | 拓扑排序：Kahn 入度法 + DFS 法 |
| `07-graph/dijkstra/` | 单源最短路（非负权） |
| `07-graph/bellman-ford/` | 能处理负权、能判负环 |
| `07-graph/floyd/` | 多源最短路：三重循环 |
| `07-graph/prim-mst/` | 最小生成树 · Prim |
| `07-graph/kruskal-mst/` | 最小生成树 · Kruskal（配并查集） |

### 08 · 散列与字符串 hash
| 目录 | 内容 |
|---|---|
| `08-hash/hash-table-chaining/` | 拉链法哈希表 + 自动扩容 |
| `08-hash/hash-table-open-addressing/` | 开放寻址 + 线性探测 + 删除的墓碑 |
| `08-hash/trie/` | 前缀树：字符串检索 |

### 09 · 其他重要结构 others
| 目录 | 内容 |
|---|---|
| `09-others/union-find/` | 并查集：路径压缩 + 按秩合并 |
| `09-others/binary-indexed-tree/` | 树状数组 BIT：前缀和 O(log n) |
| `09-others/segment-tree/` | 线段树：区间查询 + 懒标记 |
| `09-others/lru-cache/` | LRU 缓存：哈希表 + 双向链表（综合大题） |
| `09-others/bloom-filter/` | 布隆过滤器：位数组 + 多哈希 |

### 10 · 排序 sorting
| 目录 | 内容 |
|---|---|
| `10-sorting/comparison-basic/` | 冒泡、选择、插入：O(n²) 三兄弟，讲清各自适用场景 |
| `10-sorting/merge-sort/` | 归并：分治 + 稳定 |
| `10-sorting/quick-sort/` | 快排：分区 + 递归 |
| `10-sorting/heap-sort/` | 堆排序 |
| `10-sorting/non-comparison/` | 计数、基数、桶排序：什么时候能突破 O(n log n) |

### 11 · 查找 searching
| 目录 | 内容 |
|---|---|
| `11-searching/binary-search/` | 二分查找 4 种边界写法一次讲透 |
| `11-searching/rotated-array/` | 旋转数组二分 |
| `11-searching/kth-smallest/` | 第 K 小：二分答案 + 堆 + 快排三种思路 |

---

## 复杂度速查表

### 各结构操作复杂度（平均 / 最坏）

| 结构 | 按下标访问 | 查找 | 插入 | 删除 | 额外空间 |
|---|---|---|---|---|---|
| 静态数组 | O(1) | O(n) | — | — | O(1) |
| 动态数组 | O(1) | O(n) | 尾 O(1) 摊还 | O(n) | O(n) |
| 单链表 | O(n) | O(n) | 头 O(1) / 中 O(n) | O(n) | O(n) |
| 双向链表 | O(n) | O(n) | O(1)（已知节点时） | O(1)（已知节点时） | O(n) |
| 栈 / 队列 | — | — | O(1) | O(1) | O(n) |
| 循环队列 | — | — | O(1) | O(1) | O(n) |
| 哈希表（拉链） | — | O(1) / O(n) | O(1) / O(n) | O(1) / O(n) | O(n) |
| 二叉搜索树 | — | O(log n) / O(n) | O(log n) / O(n) | O(log n) / O(n) | O(n) |
| AVL / 红黑树 | — | O(log n) | O(log n) | O(log n) | O(n) |
| B 树（阶 m） | — | O(log_m n) | O(log_m n) | O(log_m n) | O(n) |
| 跳表 | — | O(log n) | O(log n) | O(log n) | O(n log n) |
| 二叉堆 | 只看堆顶 O(1) | O(n) | O(log n) | 堆顶 O(log n) | O(1) 原地 |
| 并查集 | — | — | — | — | O(n)，操作近似 O(1) |
| 树状数组 | — | 前缀和 O(log n) | O(log n) | — | O(n) |
| 线段树 | — | 区间 O(log n) | O(log n) | O(log n) | O(n) |
| Trie | — | O(串长 L) | O(L) | O(L) | O(总字符数 × 26) |

### 排序算法对比

| 算法 | 最好 | 平均 | 最坏 | 空间 | 稳定 |
|---|---|---|---|---|---|
| 冒泡 | O(n) | O(n²) | O(n²) | O(1) | 稳定 |
| 选择 | O(n²) | O(n²) | O(n²) | O(1) | 不稳定 |
| 插入 | O(n) | O(n²) | O(n²) | O(1) | 稳定 |
| 归并 | O(n log n) | O(n log n) | O(n log n) | O(n) | 稳定 |
| 快排 | O(n log n) | O(n log n) | O(n²) | O(log n) | 不稳定 |
| 堆排 | O(n log n) | O(n log n) | O(n log n) | O(1) | 不稳定 |
| 计数 | O(n+k) | O(n+k) | O(n+k) | O(k) | 稳定 |
| 基数 | O(d·n) | O(d·n) | O(d·n) | O(n+k) | 稳定 |

---

## 本项目刻意不做什么

- **不做泛型**：不出现 `void *`、不出现函数指针比较器。想看通用实现，学完本仓库再去看 `qsort`。
- **不做线程安全**：不加锁，不讨论并发。
- **不做错误处理大全**：只在 `malloc` 失败、除数为 0、下标越界这些真实危险点做检查。
- **不追求最优工程写法**：优先保证「一眼能看懂它在干什么」。

## 许可证

学习用途，随便用。

---

## 同一个仓库里的另外 5 本手册

数据结构写完之后，光会「怎么实现」还不够 —— 你得能把这些代码**编译出来、管住版本、连上机器、
用脚本串起来**。所以这个仓库里还放了 5 本同一套写法的教材：**每一句结论都是这台机器真跑出来的，
围栏里的输出逐行可在捕获里核对**。

| 手册 | 目录 | 讲什么 | 怎么跑 |
|---|---|---|---|
| **Git** | `git-handbook/` | 10 章：东西在三个地方之间移动（工作区 / 暂存区 / 仓库）→ 日常提交 → 撤销 → 分支合并 → rebase → stash/ignore → 远程协作 → 排查 → submodule/worktree → 疑难杂症。所有实验只在 `mktemp` 沙箱里做，绝不碰你现有仓库 | `cd git-handbook/01-mental-model && bash demo.sh` |
| **SSH** | `ssh-handbook/` | 6 章：一条 `ssh` 经过的三段路 → 密钥 → 配置文件 → agent 与连接复用 → 端口转发 → 排错。**全程只连 `127.0.0.1`、每个连接都故意失败**，不读写 `~/.ssh`、不动 `sshd` | `cd ssh-handbook/01-how-it-works && bash demo.sh` |
| **现代 CMake** | `cmake-handbook/` | 8 章：第一次构建 → target 与作用域 → 库与安装 → 找依赖 → 构建内部（`flags.make`、`compile_commands.json`）→ ctest 与 sanitizer 与 presets → 老写法迁移 → **逐行拆解本仓库根的 `CMakeLists.txt`** | `cd cmake-handbook/01-first-build && bash demo.sh` |
| **Python** | `python-handbook/` | 12 章：解释器到底做了什么 → 类型与变量 → 字符串与文本 → 容器 → 函数的坑 → 类与 dataclass → 异常与 `with` → 文件与 `pathlib` → 模块与 venv → 类型注解与工具 → 并发 → 速查。只用标准库，不装包 | `cd python-handbook/01-whats-really-happening && bash demo.sh` |
| **Bash** | `bash-handbook/` | 11 章：交互 shell vs 脚本 → 词与展开 → 判断与退出码 → 管道与重定向 → 循环 → 函数 → grep/sed/awk → 健壮脚本 → 出事怎么查 → 实战配方 → 速查 | `cd bash-handbook/01-shell-vs-script && bash demo.sh` |

**交付进度（2026-10-17，以磁盘为准）**：六本手册**全部交付完** —— Git 10 章 ✅、SSH 6 章 ✅、
现代 CMake 8 章 ✅、Bash 11 章 ✅、Python 12 章 ✅（再加数据结构本体那 56 节）。
最后三章各挑一句：
**Bash 第 11 章** `bash-handbook/11-cheatsheet/`（451 行 / 10 张表 / 一页纸 22 条）自己不产新证据，
demo 把表里的 8 条硬事实当场重跑一遍；**Python 第 11 章** `python-handbook/11-concurrency/`
（2237 行 / 15 节 / 7 支取证脚本）把「并发」拆成四本**互相看不见**的账 —— GIL 只管「一次让谁跑字节码」、
`threading.active_count()` 只是它自己的名册、`forkserver` 会把你的主模块整份重新 import、
协程不 await 就一行都不执行，收口表上**一个秒数、一个 pid 都没有**；
**Python 第 12 章** `python-handbook/12-cheatsheet/`（887 行 / 11 张表 × 8 行 / 四段骨架 / 一页纸 20 条）
同样只核对不产证据：160 次出处当场解析到真标题（认四种标题形状，断链 0、覆盖 11 章）、页面结构五样当场数、
四段骨架在内存里过 `ast.parse` + `compile()`、51 条硬事实用同一个解释器当场重判、隐私五类全 0。

各章「本机没有什么」也一并钉死：Python 手册**一个静态检查器输出都没有**（mypy / pyright / ruff / black /
flake8 / pylint / pytest 连 `pip` 一起缺席，改用 `typeguard` 的插桩边界 + 自己拿 `ast` 搓一把尺自证），
Bash 手册**一个 shellcheck 输出都没有**（改用 bash 自带的 `-x` / `trap DEBUG` / `-n` 三件套）。
Bash 那本的两条安全口径也留在原地：第 10 章（2634 行 / 13 节）把 `crontab` 与 `systemctl` 摊开讲但
**一次都不调用**，第 09 章的 `strace` 只看自己的进程、**永不 `-p` 别人的**。
★ 一处诚实订正：Python 第 10 章原写过「唯一在场的第三方库是 `typeguard`」，那句话只成立于它当场探的那几个名字 ——
第 12 章把 18 个名字的候选名单一次切成两半，`dist-packages` 里其实躺着 `requests`/`rich`/`setuptools`/
`typeguard`/`yaml` 五个。被否掉的始终是「装东西」这条路（没有 pip），不是「本机一个第三方包都没有」。

**和本仓库的耦合点只有一个**：CMake 手册第 08 章、Bash/Python 手册的若干示例都直接拿
**这个仓库的真实文件**当解剖对象（根 `CMakeLists.txt`、根 `Makefile`、那 56 节 C 代码）。
所以你在那边学到的东西，回头就能在自己写的代码上验证。

**建议顺序**：先把 `00-c-foundation` 和 `01-array` 跑完（指针和内存手感建立起来），
然后 CMake 手册 01~02 + Git 手册 01~03 —— 这三本组合起来解决「我写的 C 代码怎么建、怎么管」。
其余按需查。

**每本手册的验收闸门（四档，一把比一把紧，各管一层别混用）**：

```console
$ python3 ~/.local/bin/verify_quotes.py <章 README> <那次 demo 的捕获> <沙箱前缀正则>
$ bash ~/.local/bin/chapter_gate.sh <章 README> <捕获> <沙箱正则>
$ python3 ~/.local/bin/fence_capture_check.py <手册根> --prefix bash
$ bash ~/.local/bin/audit_all.sh
```

第一条只管**带 `$ ` 提示符的引用行**：未匹配必须为 0（唯一例外是脱敏替换行，各手册根 README 的「安全红线」表里写明了）。
第二条是单章交付闸门：占位符残留、围栏成对、引文、隐私、收尾节齐不齐，一条命令十几行判定。
第三条补的是第一条留下的口子 —— demo **原样打印、不带提示符**的那九千多行围栏，以前没人判过；
它按严格档逐行回冻结捕获里对，所以每跑必变的东西（提交号、端口、时刻、随机路径）会被它点名。
第四条是六本全扫的现量账。

`fence_capture_check.py` 把对不上的行分四类报数：**图**（人画的示意图占着现场围栏的位置，该改标 `text`）、
**数 / 变**（pid、端口、时刻、提交号、随机路径 —— 严格档注定对不上，是口径差不是编造）、
**他**（既不图也没有可变数字 —— 这一类必须逐行读）。只有「他」为 0、且没有一章因找不到捕获而一行没核，
才算那一本在严格档下干净。现状账连同解释写在 `~/.cache/handbook-captures/fidelity-ledger.txt`。

---

## 本机环境（6 本手册共用的一张底牌）

所有手册的每一句结论都只在这台机器上验过。换一台机器，**先跑这几条命令对照一遍**，
对不上的地方去看该章的「本章没演 / 没验证」清单，别怀疑自己。

```console
$ grep PRETTY_NAME /etc/os-release; uname -r
PRETTY_NAME="Ubuntu 26.04 LTS"
7.0.0-34-generic

$ bash --version | head -1; ls -l /bin/sh
GNU bash, version 5.3.9(1)-release (x86_64-pc-linux-gnu)
lrwxrwxrwx 1 root root 4 Feb  3  2026 /bin/sh -> dash

$ gcc --version | head -1; cmake --version | head -1; make --version | head -1
gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
cmake version 4.2.3
GNU Make 4.4.1

$ python3 -V; git --version; ssh -V
Python 3.14.4
git version 2.55.0
OpenSSH_10.2p1 Ubuntu-2ubuntu3.6, OpenSSL 3.5.5 27 Jan 2026

$ ls --version | head -1; grep --version | head -1; sed --version | head -1; awk -W version 2>&1 | head -1
ls (uutils coreutils) 0.8.0
grep (GNU grep) 3.12
sed (GNU sed) 4.9
mawk 1.3.4 20260129

$ for c in ninja g++ shellcheck python pip; do command -v $c || echo "$c 未安装"; done
ninja 未安装
g++ 未安装
shellcheck 未安装
python 未安装
pip 未安装

$ echo "LANG=$LANG"; command -v make
LANG=zh_CN.UTF-8
<家目录>/.local/bin/make
```

（这段的取证脚本是 `~/.cache/handbook-captures/root-env.sh`，原文捕获在同目录 `root-env.txt`。
只有最后一行的家目录被换成了 `<家目录>` —— 那是 `command -v make` 自己展开出来的真实路径，
按全书脱敏约定处理。注意 `LANG` 那行：脚本里 `export LC_ALL=C.UTF-8` 之后 `$LANG` **仍然是**
`zh_CN.UTF-8`，因为 `LC_ALL` 只是**压过**它、不修改它 —— 这就是为什么每个 demo 都要显式设 `LC_ALL`。）

三个会咬人的差异，先记住：

1. **别拿「GNU 还是非 GNU」当判据，本机是混的。** `ls`/`date`/`wc`/`head`/`tail`/`tr`/`sort`/`seq`/`uniq`/
   `cut`/`paste`/`sleep`/`env`/`timeout`/`stdbuf` 是 Rust 写的 **uutils coreutils 0.8.0**；
   而 `find` 和 `xargs` 是 **GNU findutils 4.10.0**，`grep` 是 GNU grep 3.12，`sed` 是 GNU sed 4.9。
   本机当场实测：`date -d '2026-01-01' +%Y`、`find -printf '%f\n'`、`sort -R --random-source` 这三个
   「听起来很 GNU」的写法**都能跑**（早期版本这里写过「`date -d`、`find -printf` 不一定支持」，是错的，
   `bash-handbook/04` 早就实测过 `find` 的来路）。真会咬人的是另外两条：`wc -l` 数的是**换行符个数**
   （末行没换行就不算一行），`tr` 按**字节**处理多字节中文（会把一个字切成半个）。
   规矩只有一条：**用之前在这台机器上现场试一次。**
2. **`awk` 是 mawk，不是 gawk。** `--include`、`asort()`、`gensub()` 这类 gawk 特性没有。
3. **`LANG=zh_CN.UTF-8`** —— 工具报错会被翻译成中文。手册里引用的都是英文原文，
   所以每个 `demo.sh` 开头都 `export LC_ALL=C.UTF-8`；你自己复现时不加这一句，
   对不上是正常的，不是 bug。

另外：`make` 不在系统路径里，是装在 `<家目录>/.local/bin` 的（`command -v make` 就能看到）。
