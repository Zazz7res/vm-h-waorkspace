# 第 08 章 · 逐行拆解仓库根那份 249 行的 `CMakeLists.txt`

> 前面几章一直在教你**怎么读**：第 01 章教你看产物，第 02 章教你认 target 和作用域，
> 第 03 章讲库与安装，第 04 章讲依赖，第 05 章教你翻 `flags.make` / `CMakeCache.txt` / `link.txt` 这三本账，
> 第 06 章讲 ctest、sanitizer、preset。
> 这一章是**第一次把它们全用在一份真的、正在跑的、249 行的文件上** —— 仓库根那份 `CMakeLists.txt`。
>
> 这一章的性质要提前说清楚：**它不是纠错章。** 那份文件是现代化的（全程 target-based、没有
> `include_directories`、没有 `file(GLOB)`、没有 `link_directories`），不需要你改。
> 我们要做的是另一件事：**每一行它在干什么、为什么要这么写、少写这一行或者换成别的写法会怎样。**
> 顺手的结果是：拆到第 7、11、16 节的时候，我们发现**这份文件里有三条注释在本机不成立** ——
> 代码是对的，说明是过时的。这是"逐行拆"比"读一遍就点头"值钱的地方。
>
> 下面**每一段输出都是这台机器上真实跑出来的**（cmake 4.2.3 + GNU Make 4.4.1 + gcc 15.2.0 + ld.bfd），
> 由 `bash demo.sh` 一次生成，共 24 节，跑法见文末「怎么跑这章的 demo」。
> 输出里的 `/tmp/cmake08-aiyAIw/...` 是 `mktemp -d` 随机生成的沙箱路径，你跑出来会是 `cmake08-<另外 6 位>`。
> 会变的不止路径：**ASan 报告里的 PID、地址、`(BuildId: ...)`、`Total Test time (real)` 这类耗时、
> `du -sh` 的体积**每次都变；它们的**形状**稳定，值不稳定。

本章的取舍：**我不重讲"怎么写一份 CMakeLists"，我只回答"这一行为什么在这里"。**
所以每一节都是同一个动作的重复：`读那一行 → 找它留下的账 → 在沙箱里把这行拨掉/换掉 → 看屏幕上多了什么少了什么`。
读法比结论重要：这一章真正想交付的是**一套可以搬到任何一份陌生 `CMakeLists.txt` 上的拆法**。

---

## 你会遇到的场景

先对上号，这些是不是你的现场：

1. 你接手一个仓库，根目录一份 249 行的 `CMakeLists.txt`，从上往下读了三遍还是不知道它干了什么。
2. 你看见 `cmake_minimum_required(VERSION 3.16)`，以为改成 `3.1` 只是"兼容性差一点"，最多一个警告。
3. 你把 `project(ds_in_c VERSION 1.0 LANGUAGES C)` 里 `LANGUAGES C` 删了，觉得"反正只有 C 代码"，然后 configure 炸了，你以为编译器坏了。
4. 编译命令行里有 `-std=gnu11`，你在文件里搜 `gnu11` 一个字符都搜不到，于是你以为它来自编译器默认值。
5. 你 `cmake --build` 完，产物散在构建目录根上，和 `CMakeCache.txt`、`Makefile` 挤在一起，你想收拾但不知道是哪一行管这件事。
6. 你敲 `-DDS_WITH_ASAN_ON=ON`（多打了一个下划线），命令成功、构建成功，你以为 sanitizer 已经进去了。
7. 你读 `add_executable(${_target} ${ARGN})`，不知道 `ARGN` 是什么，也不知道"少传一个源文件"屏幕上会说什么。
8. 工程里写着"每个叶子目录自包含"，你以为这条约定靠人自觉 —— 其实它有一行代码在强制，而且那一行删掉**照样能编过**。
9. 你看见注释写「本机系统库不带共享版 asan，必须静态链接」，就把它抄进自己工程；换台机器发现产物大了 70 倍。
10. 你把清单换成 `file(GLOB ...)` 图省事，新加了一个 `.c`，构建说 `[100%] Built target app`，什么都没干，也不报错。
11. 你的 ctest 全绿，但程序崩了的时候栈里没有行号 —— 因为 `CMAKE_BUILD_TYPE` 是空的，一个 `-g` 都没有。
12. 收尾那句 `message(STATUS "可执行 target 数量：${_n_targets}")` 里的数字，你以为它数的是可执行文件。

这 12 个现场分别落在本章的第 1 节、4 节、5 节、6 节、8 节、9 节、11 节、12+13 节、16 节、20 节、21 节、22 节。

---

## 一句话版本

* **读一份 `CMakeLists.txt` 不要从上往下读，要先读它留下的账**：`bin/` 里的产物、`flags.make`、`link.txt`、`CTestTestfile.cmake`、`CMakeCache.txt`。账对上了，行才有意义（第 1 节）。
* **同一份仓库可以有两条构建路线并存**（根 `Makefile` 和根 `CMakeLists.txt`）。它们能不打架，靠的是产物目录不同；它们"口径一致"这句话只覆盖了**警告集**，没覆盖 `-std` 和 `-g`（第 2 节）。
* **大部分"少写一行"的代价是静默**：拼错开关名退 0、`GLOB` 漏文件退 0、忘 `enable_testing()` 退 0、没 `-g` 退 0。会当场响的反而只有几行（第 4、5、11、15 节）。
* **`target_*` 命令的价值是"名单"，不是"能不能编过"**：第 75 行整行删掉照样编译成功，它管的是这个 target 的头搜索名单**恰好一条**（第 13 节）。
* **注释是给人看的承诺，不是机器验过的结论**：这份文件三条注释本机不成立（第 7、11、16 节），代码本身没问题。逐行拆 = 逐行对，不对不采信。
* **`-g` 在这份文件里只有两个来源**：`CMAKE_BUILD_TYPE`（文件没设默认值，所以平时是空的）或 `DS_WITH_ASAN=ON`。所以"平时没调试信息、一开 ASan 就有行号"是设计出来的（第 17、21 节）。

---

## 引用约定（这一章怎么读）

1. **围栏里的都是真输出**，一行没改；我给的解释放在围栏外面，用 ★ 或"注意"起头。
2. `$ ` 开头的是**敲的命令**，它的 stdout+stderr 原样跟在下面。`demo.sh` 每条命令后面都会打一行 `[退出码 N]`，那是脚本加的，不是 CMake 打的 —— 本章判断"这条命令到底成没成"全靠它。
3. 命令里出现 `| grep`、`| tail`、`| head` 的，是为了**屏幕干净**；脚本统一 `set -o pipefail`，所以 `[退出码]` 报的是整条管道的真实成败（谁挂了报谁）。
4. `〔在 9a503fd8 里〕` 是脚本记号：命令在**仓库根**那个目录里跑，但屏幕上不印它的绝对路径。
   `〔在 08-repo-cmake-walkthrough 里〕` 同理，是本章自己的目录。
5. 输出里的 `<仓库根>` **不是 CMake 打的**，是我在命令里用 `sed "s|$PWD|<仓库根>|g"` 现场换掉的 ——
   `CMakeCache.txt`、`CTestTestfile.cmake`、`C_INCLUDES` 都会把源码树绝对路径原样记下来，里面有本机登录名。
   **事后编辑捕获不算达标**，所以打码发生在命令串里（第 1、12、18 节各有一个例子）。
6. 本章**没有**对仓库根做过任何 configure / build / ctest / make（那是唯一一份物证，改了账就没了）。
   所有"拨开关看后果"的实验都在沙箱里跑本目录那份 **4 节迷你复刻**，它的 target 一律带 `mini-` 前缀，
   就是为了和真仓库那 56 个名字区分开。

---

## 心智模型：一份 `CMakeLists.txt` 的四种落点

那份 249 行的文件看起来是"一堆命令"，其实它只往**四个地方**写东西。想读懂它，先记住这张图：

```
   文件里那一行                        落到哪本账                       谁读它、你怎么验
 ┌────────────────────────┐   ┌──────────────────────────────┐   ┌────────────────────────┐
 │ cmake_minimum_required │   │ 不落账，改的是**策略默认值** │   │ configure 成不成功      │
 │ project(... LANGUAGES) │──►│ （同一句代码行为可能不同）    │──►│ 以及 CMakeCache 里那一  │
 │ set(CMAKE_C_STANDARD)  │   │  失败时是硬错误（CMake 4.x） │   │ 整套 CMAKE_C_* 工具链   │
 └────────────────────────┘   └──────────────────────────────┘   └────────────────────────┘
 ┌────────────────────────┐   ┌──────────────────────────────┐   ┌────────────────────────┐
 │ target_include_        │   │ CMakeFiles/<t>.dir/flags.make│   │ make → gcc：-I 和 -W   │
 │ directories /          │──►│   C_INCLUDES = -I...         │──►│ 少一条 -I 会不会炸、    │
 │ target_compile_options │   │   C_FLAGS = -std=gnu11 ...   │   │ 多了谁的 -I            │
 └────────────────────────┘   └──────────────────────────────┘   └────────────────────────┘
 ┌────────────────────────┐   ┌──────────────────────────────┐   ┌────────────────────────┐
 │ target_link_options    │   │ <t>.dir/link.txt             │   │ 真正的链接命令原文；    │
 │ （ASan 那半）           │──►│ /usr/bin/cc -fsanitize=...   │──►│ asan 进没进第二个一半   │
 └────────────────────────┘   └──────────────────────────────┘   └────────────────────────┘
 ┌────────────────────────┐   ┌──────────────────────────────┐   ┌────────────────────────┐
 │ add_test /             │   │ CTestTestfile.cmake          │   │ ctest：名单 + 每条用例  │
 │ enable_testing()       │──►│ 与产物目录 bin/              │──►│ 是哪一行 add 出来的     │
 └────────────────────────┘   └──────────────────────────────┘   └────────────────────────┘
```

三句话背下来：

* **配置期写的是"名单"，不是"结果"。** `-I`、`-W`、`-fsanitize` 都要到 `flags.make` / `link.txt` 里才算数（第 05 章的老规矩，本章第 12、15 节复用）。
* **`option` / 缓存变量落在 `CMakeCache.txt`**，所以"开关到底开没开"是**读一个键**就能定案的事（第 9 节）。
* **`add_test` 落在 `CTestTestfile.cmake`，而且顺手记下**"你是在第几行调用的函数"（第 18 节那个 `_BACKTRACE_TRIPLES`）。

### 读这份文件先认「谁在说话」

249 行的文件，报错来自 6 个不同的说话人。它们都印在同一个终端里：

| 屏幕上是什么样 | 说话人 | 本章哪节 | 真实退出码 |
|---|---|---|---|
| `The target name "01-array/dynamic-array" is reserved or not valid` | 配置期的 CMake | 10 | 1 |
| `No SOURCES given to target: ... ` + `Call Stack (most recent call first)` | 配置期的 CMake（栈里带函数名） | 11 | 1 |
| `Cannot find source file: a.c` + `Tried extensions .c .C ...` | 配置期的 CMake（源清单） | 11 | 1 |
| `Cannot specify compile options for target "early"` | 配置期的 CMake（顺序错） | 14 | 1 |
| `fatal error: slist-num.h: No such file or directory` | gcc 的预处理 | 12 | 2 |
| `error: implicit declaration of function 'strdup'` | gcc 15（新默认把隐式声明当错误） | 7 | 1 |
| `warning: declaration of 'total' shadows a previous local [-Wshadow]` | gcc（只是警告） | 14 | **0** |
| `gmake: *** No rule to make target '01-array-static-array'.  Stop.` | 底层 GNU Make | 23 | 2 |
| `ERROR: LeakSanitizer: detected memory leaks` | 程序里的 asan 运行时 | 15、17 | 1（ctest 汇总给 8） |
| `No tests were found!!!` | ctest | 19 | **0** |

最后两行是本章最疼的：**警告不改变退出码**（第 14 节），**ctest 说"我什么都没测"的时候退的是 0**（第 19 节）。

---

## 第 0 部分 · 先把地基看清楚（第 1~3 节）

这三节不下任何结论，只做一件事：**把"这份文件留下了什么"和"本章要用的实验台"摆出来**。
后面 21 节全部建立在这三节之上。先把工具版本钉在屏幕上（本章所有结论都是这一套版本下的结论）：

```
$ cmake --version | head -1 && gcc --version | head -1 && make --version | head -1
cmake version 4.2.3
gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
GNU Make 4.4.1
[退出码 0]
```


### 第 1 节 这份 249 行的文件在这台机器上留下了什么

先看文件本身和它的产物树（仓库根，**全程只读**）：

```
$ 〔在 9a503fd8 里〕 wc -l CMakeLists.txt
249 CMakeLists.txt
[退出码 0]
```

```
$ 〔在 9a503fd8 里〕 ls -1 build-cmake
CMakeCache.txt
CMakeFiles
CTestTestfile.cmake
Makefile
Testing
bin
cmake_install.cmake
[退出码 0]
```

注意 `bin` 那一行 —— 它是第 8 节（文件第 55 行）的证据。现在把三处"数量"对上：

```
$ 〔在 9a503fd8 里〕 ls -1 build-cmake/bin | wc -l && ls -1 build-cmake/bin | head -4
56
00-c-foundation-dynamic-memory
00-c-foundation-pointer-basics
00-c-foundation-struct-and-typedef
01-array-dynamic-array
[退出码 0]
```

```
$ 〔在 9a503fd8 里〕 ls -d build-cmake/CMakeFiles/*.dir | wc -l
56
[退出码 0]
```

```
$ 〔在 9a503fd8 里〕 grep -c "add_test(" build-cmake/CTestTestfile.cmake
56
[退出码 0]
```

★ **三处 56**。`bin/` 里 56 个可执行文件、`CMakeFiles/` 下 56 个 `.dir`、名单文件里 56 条 `add_test`。
这三个数分别是文件第 55、71、87 行留下的账 —— **它们能互相对质**，这就是本章所有结论的出发点。
一份 `CMakeLists.txt` 你不用读完就能知道它"大概做了什么"：数它的产物。

再看这份文件在这台机器上**实际用的是什么配置**（四行缓存）：

```
$ 〔在 9a503fd8 里〕 grep -E "^(CMAKE_BUILD_TYPE|DS_WITH_ASAN|CMAKE_C_COMPILER|CMAKE_GENERATOR):" build-cmake/CMakeCache.txt
CMAKE_BUILD_TYPE:STRING=
CMAKE_C_COMPILER:FILEPATH=/usr/bin/cc
DS_WITH_ASAN:BOOL=OFF
CMAKE_GENERATOR:INTERNAL=Unix Makefiles
[退出码 0]
```

★ 这四行是后面几节的舞台：`CMAKE_BUILD_TYPE:STRING=` 是**空的**（第 21 节，它直接决定"一个 `-g` 都没有"）；
`DS_WITH_ASAN:BOOL=OFF`（第 9、15 节，开关关着）；`CMAKE_C_COMPILER:FILEPATH=/usr/bin/cc`（第 5 节）；
`CMAKE_GENERATOR:INTERNAL=Unix Makefiles`（第 05 章旋钮一讲过，它决定 `--build --target` 能不能点名 target）。

最后一件事，也是这种"读磁盘物证"的写法唯一要小心的地方：

```
$ 〔在 9a503fd8 里〕 grep -E "^(CMAKE_CACHEFILE_DIR|CMAKE_HOME_DIRECTORY)" build-cmake/CMakeCache.txt | sed "s|$PWD|<仓库根>|g"
CMAKE_CACHEFILE_DIR:INTERNAL=<仓库根>/build-cmake
CMAKE_HOME_DIRECTORY:INTERNAL=<仓库根>
[退出码 0]
```

★ 这两行存的是**源码树和构建树的绝对路径**，里面天然带着本机登录名：谁把 `CMakeCache.txt` 原样贴进
文档 / issue / CI 日志，就顺手把身份贴出去了。本章的规矩是**在命令里**换掉（屏幕上看到的是字面 `$PWD`），
不是跑完再编辑输出。

### 第 2 节 两条构建路线并存：根 `Makefile` 与根 `CMakeLists` 的口径对质

这个仓库有**两份构建入口**，都在根目录：`Makefile`（你日常手搓单节用的）和 `CMakeLists.txt`。
先读 `Makefile` 前 8 行，它是这条路线的全部规则：

```
$ 〔在 9a503fd8 里〕 sed -n "1,8p" Makefile
CC      ?= gcc
CFLAGS  := -Wall -Wextra -Wshadow -std=c11 -g
BIN     := build

# 自动发现所有叶子目录：形如 01-array/dynamic-array/main.c
RAW     := $(shell find . -mindepth 3 -maxdepth 3 -name main.c -not -path './$(BIN)*' 2>/dev/null)
LEAVES  := $(sort $(patsubst %/,%,$(patsubst ./%,%,$(dir $(RAW)))))
TARGETS := $(addprefix $(BIN)/,$(LEAVES:%=%/main))
[退出码 0]
```

现在把两条路线的"警告尺子"放在一起：

```
$ 〔在 9a503fd8 里〕 grep -n "set(DS_WARNINGS" CMakeLists.txt
61:set(DS_WARNINGS -Wall -Wextra -Wshadow)
[退出码 0]
```

★ 对照屏幕上这两行：`Makefile` 是 `-Wall -Wextra -Wshadow -std=c11 -g`，`DS_WARNINGS` 只取**前三把尺子**。
第 60 行那句「和 Makefile 里 CFLAGS 的警告集完全一致」——**那三个 `-W` 确实一致**；剩下两样走了别的路：
`-std` 由 `CMAKE_C_STANDARD` 生成（第 6、7 节，而且是 `-std=gnu11`，不是 `-std=c11`），
`-g` 在 CMake 这条路线上**默认根本没有**（第 21 节）。"完全一致"这四个字的边界要自己读出来，别整个接受。

真仓库那份 `flags.make` 长这样（同样是命令里打码）：

```
$ 〔在 9a503fd8 里〕 grep -E "^(C_INCLUDES|C_FLAGS)" build-cmake/CMakeFiles/01-array-dynamic-array.dir/flags.make | sed "s|$PWD|<仓库根>|g"
C_INCLUDES = -I<仓库根>/01-array/dynamic-array
C_FLAGS = -std=gnu11 -Wall -Wextra -Wshadow
[退出码 0]
```

`C_INCLUDES` 恰好一条 `-I`、`C_FLAGS` 恰好那三把尺子 —— 这两行是第 12 节和第 14 节的对照组。

最后看两条路线的**目录形状**，它是判断"这个 `build*` 目录是谁生的"的指纹：

```
$ 〔在 9a503fd8 里〕 ls -1 build | head -4
00-c-foundation
01-array
02-linked-list
03-stack
[退出码 0]
```

```
$ 〔在 9a503fd8 里〕 ls -1 build/01-array/dynamic-array
main
[退出码 0]
```

```
$ 〔在 9a503fd8 里〕 ls -1 build-cmake/bin | grep dynamic-array
01-array-dynamic-array
[退出码 0]
```

★ 同一个 `01-array/dynamic-array`，两条路线两个名字：`Makefile` 交 `build/01-array/dynamic-array/main`，
CMake 交 `build-cmake/bin/01-array-dynamic-array`。**目录树形状就是路线的指纹**：
带 `CMakeCache.txt` / `CMakeFiles/` 的是 CMake 树；按 `<章>/<节>/main` 分层的是根 `Makefile` 树
（本仓库里 `build/`、`build-asan/`、`build-check/` 三棵都是后者 —— 本章手查过：
这三棵里**一个 `CMakeCache.txt` 都没有**，它们是 `make` / `make asan` / `make check` 各自的产物；
那三条 recipe 本章**没有跑过**，只读了 `Makefile` 里 `CFLAGS` 和 `find` 那两行）。

两条路线为什么不打架？三个理由，都在屏幕上能指出来：
① **产物目录不同**（`build*` vs `build-cmake*`），谁也不覆盖谁；
② **发现方式不同**（`Makefile` 用 `find` 现场扫，CMake 用手工清单），所以一个新增目录不会自动进另一个的账；
③ 文件第 248 行专门写了一句「根 Makefile 与本文件互不影响，`make one DIR=...` 照常可用」（第 23 节读到它）。

### 第 3 节 章工程登场：迷你复刻先跑一遍

本章不能动仓库根那棵树，所以**所有拨开关的实验都在一份 4 节的迷你复刻上做**，它就在本章节目录里：

* `CMakeLists.txt`：段落顺序、函数名 `ds_add_leaf`、开关名 `DS_WITH_ASAN`、警告变量名 `DS_WARNINGS`
  全部和真文件一致；只有目录前缀换成 `mini/`、项目名换成 `ds_ch08`。
* `mini/`：四节道具 —— `00-c-foundation/pointer-basics`、`mini/01-array/static-array`、
  `mini/02-linked-list/slist`（**故意漏 12 字节**，第 15 节的靶子）、`mini/03-stack/array-stack`。
* `src/`：五个道具（跨目录 include、遮蔽变量、POSIX `strdup`、GLOB 探针、一个静态库），第 12/14/7/20/22 节各用一件。
* 全章**没有任何一个文件叫 `main.c`** —— 根 `Makefile` 会按 `<章>/<节>/main.c` 自动发现章节，
  本章不能给它添一个假章节（第 20 节会看到那条 `find`）：

跑一遍，确立"正常状态"长什么样：

```
$ cmake -S /tmp/cmake08-aiyAIw/proj -B /tmp/cmake08-aiyAIw/b/main 2>&1 | tail -8
--   可执行 target 数量：4（一节一个）
--   警告集：-Wall;-Wextra;-Wshadow
--   ASan：OFF （想开就 -DDS_WITH_ASAN=ON 重新 configure）
--   产物目录：/tmp/cmake08-aiyAIw/b/main/bin
-- --------------------------------------------------
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake08-aiyAIw/b/main
[退出码 0]
```

```
$ cmake --build /tmp/cmake08-aiyAIw/b/main -j4 2>&1 | tail -4
[100%] Built target mini-02-linked-list-slist
[100%] Built target mini-01-array-static-array
[100%] Built target mini-00-c-foundation-pointer-basics
[100%] Built target mini-03-stack-array-stack
[退出码 0]
```

```
$ ls -1 /tmp/cmake08-aiyAIw/b/main/bin
mini-00-c-foundation-pointer-basics
mini-01-array-static-array
mini-02-linked-list-slist
mini-03-stack-array-stack
[退出码 0]
```

```
$ ctest --test-dir /tmp/cmake08-aiyAIw/b/main 2>&1 | tail -3
100% tests passed, 0 tests failed out of 4

Total Test time (real) =   0.01 sec
[退出码 0]
```

★ 四节全绿，**包括那节一直在漏 12 字节的链表**。这是故意的对照组：不开 `DS_WITH_ASAN`，
"程序自己没喊疼"就等于"通过"。第 15 节把开关拨上去，同一条用例会变红。

先把这一章的道具全列出来，顺便自证"没有一个 `main.c`"：

```
$ find /tmp/cmake08-aiyAIw/proj -name '*.c' | sort
/tmp/cmake08-aiyAIw/proj/mini/00-c-foundation/pointer-basics/leaf.c
/tmp/cmake08-aiyAIw/proj/mini/01-array/static-array/leaf.c
/tmp/cmake08-aiyAIw/proj/mini/02-linked-list/slist/leaf.c
/tmp/cmake08-aiyAIw/proj/mini/03-stack/array-stack/leaf.c
/tmp/cmake08-aiyAIw/proj/src/cross/crossleaf.c
/tmp/cmake08-aiyAIw/proj/src/glob/ctor.c
/tmp/cmake08-aiyAIw/proj/src/glob/mainx.c
/tmp/cmake08-aiyAIw/proj/src/lib/numops.c
/tmp/cmake08-aiyAIw/proj/src/std/strdup-leaf.c
/tmp/cmake08-aiyAIw/proj/src/warn/leaf.c
[退出码 0]
```

★ 十份 `.c`：四份在 `mini/`（每节一份），六份在 `src/` 当道具，**没有一个叫 `main.c`** ——
第 2 节那条 `find . -mindepth 3 -maxdepth 3 -name main.c` 扫不到本章里的任何一份文件。

复刻的 `flags.make` 和 `link.txt`：

```
$ grep -E '^(C_INCLUDES|C_FLAGS)' /tmp/cmake08-aiyAIw/b/main/CMakeFiles/mini-01-array-static-array.dir/flags.make
C_INCLUDES = -I/tmp/cmake08-aiyAIw/proj/mini/01-array/static-array
C_FLAGS = -std=gnu11 -Wall -Wextra -Wshadow
[退出码 0]
```

```
$ cat /tmp/cmake08-aiyAIw/b/main/CMakeFiles/mini-01-array-static-array.dir/link.txt
/usr/bin/cc -Wl,--dependency-file=CMakeFiles/mini-01-array-static-array.dir/link.d "CMakeFiles/mini-01-array-static-array.dir/mini/01-array/static-array/leaf.c.o" -o bin/mini-01-array-static-array
[退出码 0]
```

★ 复刻和真仓库同一种形状：一条 `-I`、那三把尺子、链接行里既没有 `-O` 也没有 `-g`。
**形状一样就够了** —— 本章后面每个实验都在这个形状上做加减法，改哪一行就多/少哪个片段。

---

## 第 1 部分 · 文件开头那十几行（43~55）：项目身份、语言标准、产物去哪（第 4~8 节）

真文件第 43~55 行一共 13 行，管着四件事：**允许哪版 CMake、这是什么项目、按哪个 C 标准编、产物放哪**。
四件事各自留下一本不同的账，所以本章把它们拆成四节。

### 第 4 节 第 43 行 `cmake_minimum_required(VERSION 3.16)`：下界管的是策略

第 43 行是全文件第一句真代码。先把它拨低：

```
$ cmake -S /tmp/cmake08-aiyAIw/p/min31 -B /tmp/cmake08-aiyAIw/b/min31 2>&1 | tail -9

  Update the VERSION argument <min> value.  Or, use the <min>...<max> syntax
  to tell CMake that the project requires at least <min> but has been updated
  to work with policies introduced by <max> or earlier.

  Or, add -DCMAKE_POLICY_VERSION_MINIMUM=3.5 to try configuring anyway.


-- Configuring incomplete, errors occurred!
[退出码 1]
```

★ 本机 cmake 4.2.3 把 3.5 以下的兼容代码**整个删掉了**，所以 `3.1` 不是警告，是 configure 失败（退出码 1）。
真文件写 `3.16` 就躲过了这道闸。注意屏幕上的第二选择：`-DCMAKE_POLICY_VERSION_MINIMUM=3.5` ——
那是逃生门，不是"你的文件没问题"的证据。

再看两个能通过的下界：

```
$ cmake -S /tmp/cmake08-aiyAIw/p/min310 -B /tmp/cmake08-aiyAIw/b/min310 2>&1 | tail -3
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake08-aiyAIw/b/min310
[退出码 0]
```

```
$ cmake -S /tmp/cmake08-aiyAIw/p/min316 -B /tmp/cmake08-aiyAIw/b/min316 2>&1 | tail -3
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake08-aiyAIw/b/min316
[退出码 0]
```

★ 3.10 和 3.16 在本机都安静通过。**也就是说"下界写多少"在屏幕上看不出来** —— 它改的是**策略默认值**：
同一句 CMake 代码，在不同下界下行为可能不同（例如某些命令的参数展开、某些 CMP 的 NEW/OLD）。
这就是为什么这一行既不能删（删了 CMake 4 直接报错"没设最小版本"）、又不能随手写小。

它自己的官方说法：

```
$ cmake --help-command cmake_minimum_required | sed -n '1,10p'
cmake_minimum_required
----------------------

Require a minimum version of cmake.

 cmake_minimum_required(VERSION <min>[...<policy_max>] [FATAL_ERROR])

.. versionadded:: 3.12
  The optional ``<policy_max>`` version behavior; ignored in older CMake.

[退出码 0]
```

```
$ cmake -S /tmp/cmake08-aiyAIw/p/minrange -B /tmp/cmake08-aiyAIw/b/minrange 2>&1 | tail -3
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake08-aiyAIw/b/minrange
[退出码 0]
```

★ `VERSION <min>[...<policy_max>]` 的意思是"我按 `min` 的规矩写，但 `policy_max` 之前的新策略我也认"。
本仓库只写了裸下界 —— 对一份只用基础命令的教学工程这是对的取舍；真要跨大版本，`...<max>` 才是
不让自己被新策略偷袭的那一半。

### 第 5 节 第 45~48 行 `project(... LANGUAGES C)`：少写 `LANGUAGES` 在本机会炸

真文件这四行是：`project(ds_in_c VERSION 1.0 DESCRIPTION "..." LANGUAGES C)`。
把 `LANGUAGES C` 删掉，其他一个字不动：

```
$ cmake -S /tmp/cmake08-aiyAIw/p/nolangs -B /tmp/cmake08-aiyAIw/b/nolangs 2>&1 | grep -E 'compiler identification|CMake Error|Configuring incomplete' | head -6
-- The C compiler identification is GNU 15.2.0
-- The CXX compiler identification is unknown
CMake Error at CMakeLists.txt:2 (project):
-- Configuring incomplete, errors occurred!
[退出码 1]
```

★ 第二行是重点：`The CXX compiler identification is unknown`。
`project()` 不写 `LANGUAGES` 时**默认同时启用 C 和 CXX**，本机没有 g++，于是当场炸。
看完整报错：

```
$ cmake -S /tmp/cmake08-aiyAIw/p/cxx -B /tmp/cmake08-aiyAIw/b/cxx 2>&1 | tail -8
  No CMAKE_CXX_COMPILER could be found.

  Tell CMake where to find the compiler by setting either the environment
  variable "CXX" or the CMake cache entry CMAKE_CXX_COMPILER to the full path
  to the compiler, or to the compiler name if it is in the PATH.


-- Configuring incomplete, errors occurred!
[退出码 1]
```

★ `No CMAKE_CXX_COMPILER could be found` —— 这就是第 45 行那句 `LANGUAGES C` 在本机的价值：
**它不是排版，是"能不能 configure"的分界**。换句话说，这份文件"看起来随大流的一行"，
换一台没装 C++ 编译器的机器上就是生死线；反过来说，如果你把 `LANGUAGES C` 删了，
在一台装了 g++ 的机器上**什么都不会发生** —— 所以这个坑是**机器相关**的，读不出它就只能靠试。

`project()` 落下的账（复刻工程的缓存）：

```
$ grep -E '^(CMAKE_PROJECT_NAME|CMAKE_C_COMPILER|ds_ch08_SOURCE_DIR|ds_ch08_BINARY_DIR)' /tmp/cmake08-aiyAIw/b/main/CMakeCache.txt
CMAKE_C_COMPILER:FILEPATH=/usr/bin/cc
CMAKE_C_COMPILER_AR:FILEPATH=/usr/bin/gcc-ar-15
CMAKE_C_COMPILER_RANLIB:FILEPATH=/usr/bin/gcc-ranlib-15
CMAKE_PROJECT_NAME:STATIC=ds_ch08
ds_ch08_BINARY_DIR:STATIC=/tmp/cmake08-aiyAIw/b/main
ds_ch08_SOURCE_DIR:STATIC=/tmp/cmake08-aiyAIw/proj
CMAKE_C_COMPILER-ADVANCED:INTERNAL=1
CMAKE_C_COMPILER_AR-ADVANCED:INTERNAL=1
CMAKE_C_COMPILER_RANLIB-ADVANCED:INTERNAL=1
[退出码 0]
```

★ 项目名 `ds_ch08`、两个目录变量 `<name>_SOURCE_DIR` / `<name>_BINARY_DIR`、一整套 `CMAKE_C_*` 工具链路径。
第 1 节那份真仓库缓存里 `CMAKE_C_COMPILER` 同样是 `/usr/bin/cc` —— **不是 gcc 也不是 clang，是 `cc`**，
它是系统包管的软链（第 05 章旋钮三讲过：写死编译器绝对路径的工程换机器会炸，这份文件没有）。

`VERSION 1.0` 那半句只干一件事：给结尾的 `message` 用（第 22 节）。它没变成任何库的 soname ——
因为这份文件不装东西。

### 第 6 节 第 51~53 行：命令行里那个 `-std=gnu11` 到底是谁写的

你在 `CMakeLists.txt` 里搜 `gnu11` 一个字符都搜不到，但 `flags.make` 里它就是存在。它来自这三行：

```
set(CMAKE_C_STANDARD 11)              # 对应 Makefile 里的 -std=c11
set(CMAKE_C_STANDARD_REQUIRED ON)     # 编译器不支持 C11 就直接报错，而不是偷偷降级
set(CMAKE_C_EXTENSIONS ON)            # 保留 GNU 扩展（我们没刻意用，但别因 strict 模式添乱）
```

四档实验（每档一个独立小工程，只看 `C_FLAGS` 那一行）：

```
$ grep '^C_FLAGS' /tmp/cmake08-aiyAIw/b/std-ext_on/CMakeFiles/app.dir/flags.make
C_FLAGS = -std=gnu11
[退出码 0]
```

```
$ grep '^C_FLAGS' /tmp/cmake08-aiyAIw/b/std-ext_off/CMakeFiles/app.dir/flags.make
C_FLAGS = -std=c11
[退出码 0]
```

```
$ grep '^C_FLAGS' /tmp/cmake08-aiyAIw/b/std-req_off/CMakeFiles/app.dir/flags.make
C_FLAGS = -std=gnu11
[退出码 0]
```

```
$ grep '^C_FLAGS' /tmp/cmake08-aiyAIw/b/std-nostd/CMakeFiles/app.dir/flags.make
C_FLAGS = 
[退出码 0]
```

★ 四档对质，结论要说得准：`-std=gnu11` ← 第 51 行 + 第 53 行**一起**给的结果
（`EXTENSIONS ON` 就是那个 `gnu` 前缀）；`EXTENSIONS OFF` → `-std=c11`；
`REQUIRED` 拨成 OFF → **本机屏幕上看不出任何差别**（gcc 15 本来就支持 C11，它只在"编译器不支持"时才有戏）；
三行全删 → `C_FLAGS = ` 空 → 编译器自己的标准接管。

最后一档值得单独量一下，因为它太安静了：

```
$ gcc -dM -E -x c /dev/null | grep __STDC_VERSION__
#define __STDC_VERSION__ 202311L
[退出码 0]
```

```
$ gcc -dM -E -x c -std=gnu11 /dev/null | grep __STDC_VERSION__
#define __STDC_VERSION__ 201112L
[退出码 0]
```

★ **本机 gcc 15 不带 `-std` 时是 C23（`__STDC_VERSION__ 202311L`）**。
所以第 51 行不是装饰：删掉它，这 56 节代码就从 C11 世界挪到了 C23 世界，
而屏幕上**一个字的提示都没有**。这类"配置变了但没人说话"的坑，只能靠读 `flags.make` 抓（第 05 章的取证法）。

### 第 7 节 `c11` 与 `gnu11` 的真实分界：那句「对应 Makefile 里的 `-std=c11`」其实不成立

第 51 行那句注释是本文件的**第一条在本机不成立的说明**。要证它，得找一个"只有 GNU 扩展才端得出声明"的调用：

```
$ cp /tmp/cmake08-aiyAIw/proj/src/std/strdup-leaf.c /tmp/cmake08-aiyAIw/ && gcc -std=c11 -Wall -Wextra -Wshadow /tmp/cmake08-aiyAIw/strdup-leaf.c -o /tmp/cmake08-aiyAIw/sd-c11 2>&1 | head -7
/tmp/cmake08-aiyAIw/strdup-leaf.c: In function ‘main’:
/tmp/cmake08-aiyAIw/strdup-leaf.c:12:18: error: implicit declaration of function ‘strdup’; did you mean ‘strcmp’? [-Wimplicit-function-declaration]
   12 |     char *name = strdup("gnu11-or-c11");
      |                  ^~~~~~
      |                  strcmp
/tmp/cmake08-aiyAIw/strdup-leaf.c:12:18: error: initialization of ‘char *’ from ‘int’ makes pointer from integer without a cast [-Wint-conversion]
[退出码 1]
```

```
$ gcc -std=gnu11 -Wall -Wextra -Wshadow /tmp/cmake08-aiyAIw/strdup-leaf.c -o /tmp/cmake08-aiyAIw/sd-gnu11 2>&1 | head -7 && /tmp/cmake08-aiyAIw/sd-gnu11
strdup ok: gnu11-or-c11
[退出码 0]
```

★ 同一份 `strdup-leaf.c`，两个标准，两种命运：
`-std=c11`（根 `Makefile` 那条路线）里 `strdup` 的声明没端出来，**gcc 15 把隐式声明直接判成 error**（退出码 1）；
`-std=gnu11`（CMake 这条路线）安静通过、跑得对。
注意这条分界不是"能不能用 POSIX 函数"这种理论问题，而是**这台机器的编译器现在会把这件事报成什么**。

回到文件原文核对：

```
$ 〔在 9a503fd8 里〕 grep -n "std=c11\|C_EXTENSIONS\|CMAKE_C_STANDARD" Makefile CMakeLists.txt
Makefile:2:CFLAGS  := -Wall -Wextra -Wshadow -std=c11 -g
CMakeLists.txt:51:set(CMAKE_C_STANDARD 11)              # 对应 Makefile 里的 -std=c11
CMakeLists.txt:52:set(CMAKE_C_STANDARD_REQUIRED ON)     # 编译器不支持 C11 就直接报错，而不是偷偷降级
CMakeLists.txt:53:set(CMAKE_C_EXTENSIONS ON)            # 保留 GNU 扩展（我们没刻意用，但别因 strict 模式添乱）
[退出码 0]
```

★ 结论照着屏幕说：第 53 行「我们没刻意用 GNU 扩展」**成立**（现在确实用不到，开着只是不给自己添乱）；
第 51 行「对应 Makefile 里的 `-std=c11`」**不成立** —— 它对应的是 `gnu11`，两条路线在
"POSIX 声明要不要端出来"上不是同一个标准。这条差异现在没伤害任何人（那 56 节没用 POSIX 扩展），
但它是**将来会咬人**的那种不一致：同一份 `.c`，`make` 编不过、`cmake` 编得过。

### 第 8 节 第 55 行 `CMAKE_RUNTIME_OUTPUT_DIRECTORY`：56 个产物为什么都躺在 `bin/`

真文件第 55 行只有一句：`set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")`。
先看它的账：

```
$ 〔在 9a503fd8 里〕 ls -1 build-cmake/bin | wc -l && ls -1 build-cmake/bin | head -3
56
00-c-foundation-dynamic-memory
00-c-foundation-pointer-basics
00-c-foundation-struct-and-typedef
[退出码 0]
```

现在把这行换成一句注释（工程其他部分不动）：

```
$ grep -n 'CMAKE_RUNTIME_OUTPUT_DIRECTORY' /tmp/cmake08-aiyAIw/p/nobin/CMakeLists.txt
35:# 实验：CMAKE_RUNTIME_OUTPUT_DIRECTORY 那一行被删掉
102:message(STATUS "  产物目录：${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
[退出码 0]
```

```
$ cmake -S /tmp/cmake08-aiyAIw/p/nobin -B /tmp/cmake08-aiyAIw/b/nobin >/dev/null 2>&1 && cmake --build /tmp/cmake08-aiyAIw/b/nobin -j4 >/dev/null 2>&1 && ls -1 /tmp/cmake08-aiyAIw/b/nobin
CMakeCache.txt
CMakeFiles
CTestTestfile.cmake
Makefile
cmake_install.cmake
mini-00-c-foundation-pointer-basics
mini-01-array-static-array
mini-02-linked-list-slist
mini-03-stack-array-stack
[退出码 0]
```

```
$ cat /tmp/cmake08-aiyAIw/b/nobin/CMakeFiles/mini-01-array-static-array.dir/link.txt
/usr/bin/cc -Wl,--dependency-file=CMakeFiles/mini-01-array-static-array.dir/link.d "CMakeFiles/mini-01-array-static-array.dir/mini/01-array/static-array/leaf.c.o" -o mini-01-array-static-array
[退出码 0]
```

★ 两个证据同时到位：产物**散在构建目录根上**，和 `CMakeCache.txt`、`Makefile` 挤在一起；
`link.txt` 末尾的 `-o` 也跟着少了 `bin/` 那一段。
有那一行，56 个可执行文件挤在一个 `bin/` 里（第 1 节那条 `ls | wc -l` 就是它的账）。

为什么这行**该写在函数外面**：

★ 它是**项目级/目录级**的变量（名字就叫 `CMAKE_*`），不是 per-target 属性。写进 `ds_add_leaf()` 里也生效
（函数体属于同一个目录作用域），但那是把项目级的事伪装成 target 级的事；真要按 target 控制产物位置，
用的是 `RUNTIME_OUTPUT_DIRECTORY` 这个 **target 属性**（本章没演，见文末「没演」）。

顺带一条和第 01 章呼应的：这一行只影响**可执行文件**（`RUNTIME_`），库另有 `LIBRARY_` / `ARCHIVE_` 两个变量。
这份文件不产库，所以只写了一个 —— 这个"看起来不完整"的写法其实是**精确匹配需求**的好例子。


---

## 第 2 部分 · 第 58~88 行：一个开关、一份警告集、和一个把 56 次样板收成一份的函数

第 58~61 行是两行全局设置，第 67~88 行是那份文件的心脏：`ds_add_leaf()`。
真文件里它长这样（**这是抄原文，不是输出**）：

```cmake
function(ds_add_leaf leaf_dir)
    string(REPLACE "/" "-" _target "${leaf_dir}")
    add_executable(${_target} ${ARGN})
    target_include_directories(${_target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${leaf_dir}")
    target_compile_options(${_target} PRIVATE ${DS_WARNINGS})
    if(DS_WITH_ASAN) ... endif()
    add_test(NAME smoke_${_target} COMMAND ${_target})
endfunction()
```

七行代码，六个实验点（第 10~18 节）。这一部分是全章密度最高的地方。

### 第 9 节 第 58 行 `option(DS_WITH_ASAN ... OFF)`：开关在缓存里的样子

`option()` 干的事就一件：**给缓存里放一个 BOOL，并给个默认值**。
先看它在缓存里的样子（复刻工程；真仓库那份见第 1 节，同一个键同一个值）：

```
$ grep -E '^DS_WITH_ASAN' /tmp/cmake08-aiyAIw/b/main/CMakeCache.txt
DS_WITH_ASAN:BOOL=OFF
[退出码 0]
```

★ 同一个键、同一个值。`grep -E '^DS_WITH_ASAN' CMakeCache.txt` 是"这个项目到底开没开 ASan"的**最快定案法**，
比翻 `CMakeLists.txt` 快，因为它读的是**这次配置实际用的值**，不是"文件里写了什么"。

另一个读法：

```
$ cmake -L /tmp/cmake08-aiyAIw/b/main 2>&1 | grep -E 'DS_WITH_ASAN'
--   ASan：OFF （想开就 -DDS_WITH_ASAN=ON 重新 configure）
DS_WITH_ASAN:BOOL=OFF
[退出码 0]
```

★ 屏幕上先冒出回显行，是因为 `cmake -L` 会**顺手重跑一遍 configure**（第 05 章旋钮三演过）。
`-L` 只列非内部缓存变量，`-LA` 连内部变量一起列。

现在演"拼错开关名"。这一条是本节的主角：

```
$ cmake -S /tmp/cmake08-aiyAIw/proj -B /tmp/cmake08-aiyAIw/b/typo -DDS_WITH_ASAN_ON=ON 2>&1 | tail -5

    DS_WITH_ASAN_ON


-- Build files have been written to: /tmp/cmake08-aiyAIw/b/typo
[退出码 0]
```

★ 这段输出里有两件事同时成立：
① CMake **确实说了**有个变量没人用（它把 `DS_WITH_ASAN_ON` 列在"未使用变量"里，屏幕被 `tail -5` 截掉了前半句）；
② 它**照退 0**，构建目录照样生成。
也就是说：拼错的代价不是一声报错，是"你以为开了、其实没开"。

验证到底开没开 —— 两份 `flags.make`：

```
$ grep -c fsanitize /tmp/cmake08-aiyAIw/b/main/CMakeFiles/mini-02-linked-list-slist.dir/flags.make
0
[退出码 1]
```

```
$ grep -c fsanitize /tmp/cmake08-aiyAIw/b/typo/CMakeFiles/mini-02-linked-list-slist.dir/flags.make
0
[退出码 1]
```

★ 两个 0。第二个 0 就是拼错名字的全部代价：**命令成功、目录正常、ASan 一个字节都没挂上**。
这两条命令的退出码都是 1，那是 `grep -c` 没匹配到东西，不是命令坏了 —— 别把"grep 没找到"读成"操作失败"。
正确的验收顺序永远是：`-D` 完 → 读缓存键 → 读 `flags.make`。第 06 章给过这条链，本章第 15 节再走一遍。

### 第 10 节 第 68 行 `string(REPLACE "/" "-" _target "${leaf_dir}")`：target 名与 `bin/` 里那 56 个文件

先看真文件的两行清单和对应的产物：

```
$ 〔在 9a503fd8 里〕 grep -m2 "^ds_add_leaf(01-array" CMakeLists.txt
ds_add_leaf(01-array/static-array             01-array/static-array/static-array.c
ds_add_leaf(01-array/dynamic-array            01-array/dynamic-array/dynamic-array.c
[退出码 0]
```

```
$ 〔在 9a503fd8 里〕 ls -1 build-cmake/bin/01-array-dynamic-array
build-cmake/bin/01-array-dynamic-array
[退出码 0]
```

★ 目录 `01-array/dynamic-array` → target `01-array-dynamic-array`。
一行代码，磁盘上两份物证（清单那两行 + `bin/` 里那个文件）说的是同一件事 ——
**这就是"逐行拆"最理想的形状：每一行都能指到一件东西上。**

复刻工程的版本：

```
$ ls -1 /tmp/cmake08-aiyAIw/b/main/bin
mini-00-c-foundation-pointer-basics
mini-01-array-static-array
mini-02-linked-list-slist
mini-03-stack-array-stack
[退出码 0]
```

★ 本章节目录多了一层 `mini/`，所以同一行 REPLACE 产出 `mini-01-array-static-array`。
这正好当场证明那行干的就是"把斜杠换成横线"，**段数它不管**。
所以你要在 `bin/` 里找某节，就把路径里的 `/` 全部换成 `-` 就对了 —— 反过来读别人的构建目录也一样。

那如果不换会怎样：

```
$ cmake -S /tmp/cmake08-aiyAIw/p/slashname -B /tmp/cmake08-aiyAIw/b/slashname 2>&1 | tail -7
CMake Error at CMakeLists.txt:3 (add_executable):
  The target name "01-array/dynamic-array" is reserved or not valid for
  certain CMake features, such as generator expressions, and may result in
  undefined behavior.


-- Configuring incomplete, errors occurred!
[退出码 1]
```

★ 这就是真文件第 40~41 行那句「唯一的小麻烦」的原文报错：
`The target name "01-array/dynamic-array" is reserved or not valid for certain CMake features`。
**它不是风格问题，是 CMake 不让** —— `/` 在 target 名里有含义（分层命名、生成器表达式），留着会撞车。
顺带一条好读的：target 名**以数字开头是合法的**，所以 `bin/` 里那 56 个文件天然按章节排序，
`ls` 出来就是教材目录 —— 教学工程里这不是小事，产物名单和讲义目录能一眼对齐。

### 第 11 节 第 71 行 `add_executable(${_target} ${ARGN})`：`ARGN` 是什么，少传一个会怎样

第 70 行的注释写的是 `ARGVN`。**`ARGVN` 这个变量不存在**，实际用的是 `ARGN`。
这是本文件的**第二条说不准的注释**（笔误，无害，但说明它没被逐行验过）。把两个都打印出来看：

```
$ cmake -S /tmp/cmake08-aiyAIw/p/argn -B /tmp/cmake08-aiyAIw/b/argn 2>&1 | grep -E 'argv0|ARGN' | head -6
-- argv0(第一个参数)=01-array/dynamic-array  ARGN 项数=2  总参数个数(ARGC)=3
--   ARGN 里的一项 => 01-array/dynamic-array/dynamic-array.c
--   ARGN 里的一项 => 01-array/dynamic-array/main.c
[退出码 0]
```

★ `ARGN` = "除第一个之外的全部参数"。真文件里每个调用点的第一个参数是**目录**，剩下的是**该目录的所有 `.c`**：

```cmake
ds_add_leaf(01-array/static-array  01-array/static-array/static-array.c)
   │            │                       │
   │            └ ARGN[0]               └ 这一项进 add_executable 的源清单
   └ leaf_dir → 换成 - → target 名
```

`foreach` 那条把 `ARGN` 的每一项原样端出来 —— 所以**函数并不检查你传的是不是真文件、在不在这个目录里**。
那层检查在配置期由 `add_executable` 做，看下面两个现场。

**现场一：只报目录名、忘了列 `.c`。**

```
$ cmake -S /tmp/cmake08-aiyAIw/p/nosrc -B /tmp/cmake08-aiyAIw/b/nosrc 2>&1 | tail -8
-- Configuring done (0.1s)
CMake Error at CMakeLists.txt:5 (add_executable):
  No SOURCES given to target: 01-array-static-array
Call Stack (most recent call first):
  CMakeLists.txt:7 (ds_add_leaf)


CMake Generate step failed.  Build files cannot be regenerated correctly.
[退出码 1]
```

★ `No SOURCES given to target: 01-array-static-array`，而且 `Call Stack (most recent call first)` 一路指回函数外面
（`CMakeLists.txt:7 (ds_add_leaf)`）。读法要注意：**括号里是函数名，上面那个行号是函数被调用的地方** ——
调用栈会把 56 个调用点里到底是哪一个交出来。这就是"样板收进函数"的隐性代价（报错必须看栈）
和它的回报（少写一个参数也能被定位）。

**现场二：源文件写成了裸文件名，而文件在子目录里。**

```
$ cmake -S /tmp/cmake08-aiyAIw/p/barename -B /tmp/cmake08-aiyAIw/b/barename 2>&1 | grep -m1 -A6 'Cannot find source file'
  Cannot find source file:

    a.c

  Tried extensions .c .C .c++ .cc .cpp .cxx .cu .mpp .m .M .mm .ixx .cppm
  .ccm .cxxm .c++m .h .hh .h++ .hm .hpp .hxx .in .txx .f .F .for .f77 .f90
  .f95 .f03 .hip .ispc
[退出码 1]
```

★ `Cannot find source file: a.c` + 一长串 `Tried extensions`。
第 71 行那些路径是相对**CMakeLists 所在目录**展开的（不是相对 `leaf_dir`！这一点最容易看漏）。
真文件把每份 `.c` 都写成 `01-array/dynamic-array/dynamic-array.c` 这种"章/节/文件"全长路径，
看着啰嗦，其实是把"相对谁"写死在纸面上 ——
于是**同一个目录里的源文件绝不可能被漏在清单外面**：漏了就是 `Cannot find source file`，当场响。
这条"当场响"正是第 20 节 `file(GLOB)` 反面实验的对照面。

### 第 12 节 第 75 行 `target_include_directories(... PRIVATE)`：叶子自包含的强制点

真文件第 73~74 行的注释是：「本节叶子目录自包含：只 include 本目录的 `.h` 和标准库。
这一行就是把那条约定固化进构建系统 —— 跨目录 include 会直接编译不过。」
这条注释**成立**。证法：给复刻加第五节，它的 `leaf.c` 只 include 邻居的头。

```
$ grep -n '^ds_add_leaf' /tmp/cmake08-aiyAIw/p/cross/CMakeLists.txt
79:ds_add_leaf(mini/00-c-foundation/pointer-basics  mini/00-c-foundation/pointer-basics/leaf.c)
82:ds_add_leaf(mini/01-array/static-array           mini/01-array/static-array/leaf.c)
86:ds_add_leaf(mini/02-linked-list/slist            mini/02-linked-list/slist/leaf.c)
89:ds_add_leaf(mini/03-stack/array-stack            mini/03-stack/array-stack/leaf.c)
90:ds_add_leaf(mini/04-cross mini/04-cross/leaf.c)
[退出码 0]
```

```
$ cmake -S /tmp/cmake08-aiyAIw/p/cross -B /tmp/cmake08-aiyAIw/b/cross >/dev/null 2>&1 && cmake --build /tmp/cmake08-aiyAIw/b/cross 2>&1 | grep -E 'fatal error|Built target mini-00|slist-num' | head -6
[ 20%] Built target mini-00-c-foundation-pointer-basics
/tmp/cmake08-aiyAIw/p/cross/mini/04-cross/leaf.c:2:10: fatal error: slist-num.h: No such file or directory
    2 | #include "slist-num.h"          /* ★ 这份头不在本 leaf 目录里，它在 02-linked-list/slist/ */
[退出码 2]
```

```
$ grep '^C_INCLUDES' /tmp/cmake08-aiyAIw/b/cross/CMakeFiles/mini-04-cross.dir/flags.make
C_INCLUDES = -I/tmp/cmake08-aiyAIw/p/cross/mini/04-cross
[退出码 0]
```

★ 三屏连读：`C_INCLUDES` 恰好一条 `-I`（还是这个 target **自己那一节**的目录），
于是跨目录 include 死在编译期，屏幕上原话是 `slist-num.h: No such file or directory`，退出码 2（底层 make 挂）。
**这就是"约定"变成"物理"的那一行**：不是靠人自觉，是靠这一行 + `PRIVATE`。

再看反面教材工程长什么样（这是**抄函数体**，为了让你看清"老写法"整份文件的样子）：

```
$ sed -n '/^function(ds_add_leaf/,/endfunction/p' /tmp/cmake08-aiyAIw/p/crossold/CMakeLists.txt
function(ds_add_leaf leaf_dir)
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/mini/02-linked-list/slist")
    string(REPLACE "/" "-" _target "${leaf_dir}")

    # ARGVN 是 CMake 的"剩余参数"清单；这里就是该目录下所有 .c
    add_executable(${_target} ${ARGN})

    # 本节叶子目录**自包含**：只 include 本目录的 .h 和标准库。
    # 这一行就是把那条约定固化进构建系统 —— 跨目录 include 会直接编译不过。
    # target_include_directories（实验：这一行被注释掉）(${_target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${leaf_dir}")

    target_compile_options(${_target} PRIVATE ${DS_WARNINGS})

    if(DS_WITH_ASAN)
        # -g 让 ASan 报到具体行号；-fno-omit-frame-pointer 让栈回溯可读
        target_compile_options(${_target} PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
        # 本机系统库不带共享版 asan，必须静态链接，否则运行时找不到 libasan.so
        target_link_options(${_target} PRIVATE -fsanitize=address -static-libasan)
    endif()

    # 每条 smoke 用例就是"把这节跑一遍，退出码必须是 0"
    add_test(NAME smoke_${_target} COMMAND ${_target})
endfunction()
[退出码 0]
```

★ 两处改动：`target_include_directories` 那行被注释掉，函数里塞进一行目录级 `include_directories`，
指的就是邻居 `slist` 的目录 —— 也就是"我让所有节都能看见 slist 的头"。跑跑看：

```
$ cmake -S /tmp/cmake08-aiyAIw/p/crossold -B /tmp/cmake08-aiyAIw/b/crossold >/dev/null 2>&1 && cmake --build /tmp/cmake08-aiyAIw/b/crossold 2>&1 | tail -3
[ 90%] Building C object CMakeFiles/mini-04-cross.dir/mini/04-cross/leaf.c.o
[100%] Linking C executable bin/mini-04-cross
[100%] Built target mini-04-cross
[退出码 0]
```

```
$ grep '^C_INCLUDES' /tmp/cmake08-aiyAIw/b/crossold/CMakeFiles/mini-00-c-foundation-pointer-basics.dir/flags.make
C_INCLUDES = -I/tmp/cmake08-aiyAIw/p/crossold/mini/02-linked-list/slist
[退出码 0]
```

★ 老写法一路绿灯：抄邻居东西的那节建出来了，
**而完全不相干的第一节（`mini-00-...-pointer-basics`）也吃到了那条 `-I`**。
这就是真文件第 26~30 行拒绝目录级命令的理由，现在它在屏幕上：
**目录级命令污染的是"它之后定义的所有 target"**，而且污染完之后构建更顺利（不会有人抱怨）。
（老写法 → 新写法的完整迁移是规划中的第 07 章，**该章在本仓库尚未收口**，本章只演到"污染可见"这一步。）

### 第 13 节 把第 75 行整行删掉，居然照样能编：它的价值是名单，不是能用

上一节证明"这一行挡得住跨目录 include"。这一节做**更狠的一步**：整行删掉（不加任何别的东西）。

```
$ sed -n '/^function(ds_add_leaf/,/endfunction/p' /tmp/cmake08-aiyAIw/p/noin/CMakeLists.txt
function(ds_add_leaf leaf_dir)
    string(REPLACE "/" "-" _target "${leaf_dir}")

    # ARGVN 是 CMake 的"剩余参数"清单；这里就是该目录下所有 .c
    add_executable(${_target} ${ARGN})

    # 本节叶子目录**自包含**：只 include 本目录的 .h 和标准库。
    # 这一行就是把那条约定固化进构建系统 —— 跨目录 include 会直接编译不过。

    target_compile_options(${_target} PRIVATE ${DS_WARNINGS})

    if(DS_WITH_ASAN)
        # -g 让 ASan 报到具体行号；-fno-omit-frame-pointer 让栈回溯可读
        target_compile_options(${_target} PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
        # 本机系统库不带共享版 asan，必须静态链接，否则运行时找不到 libasan.so
        target_link_options(${_target} PRIVATE -fsanitize=address -static-libasan)
    endif()

    # 每条 smoke 用例就是"把这节跑一遍，退出码必须是 0"
    add_test(NAME smoke_${_target} COMMAND ${_target})
endfunction()
[退出码 0]
```

★ 这回是真删：函数体里那一行没了，只剩两行注释在原地。

```
$ cmake -S /tmp/cmake08-aiyAIw/p/noin -B /tmp/cmake08-aiyAIw/b/noin >/dev/null 2>&1 && cmake --build /tmp/cmake08-aiyAIw/b/noin -j4 2>&1 | tail -2
[100%] Built target mini-02-linked-list-slist
[100%] Built target mini-00-c-foundation-pointer-basics
[退出码 0]
```

```
$ grep '^C_INCLUDES' /tmp/cmake08-aiyAIw/b/noin/CMakeFiles/mini-01-array-static-array.dir/flags.make
C_INCLUDES = 
[退出码 0]
```

★ **四节全过、`C_INCLUDES` 一个字都没有。** 为什么？因为 `#include "sarr.h"` 找得到 ——
双引号形式会先搜**包含它的那个源文件自己的目录**（第 05 章旋钮四演过这条编译器规则）。
也就是说 CMake 不给 `-I`，gcc 也照样能在同目录里找到头。

那这一行到底在管什么？和"有它"的状态对一下：

```
$ grep '^C_INCLUDES' /tmp/cmake08-aiyAIw/b/main/CMakeFiles/mini-01-array-static-array.dir/flags.make
C_INCLUDES = -I/tmp/cmake08-aiyAIw/proj/mini/01-array/static-array
[退出码 0]
```

★ 三节连起来读就清楚了：
**有第 75 行** → 这个 target 的头搜索名单是"本节目录"，恰好一条；
**删掉第 75 行** → 名单是空的，同目录的引号 include 靠编译器规则蒙混过关；
**换成目录级老写法** → 名单变成"别人的目录"（第 12 节最后一屏）。
所以这一行不是为了让代码能编，是为了让**名单本身可控、可读、可 grep**：
`clangd` / IDE 从编译数据库里看到的名单要和构建一致（第 01 章场景 9）；
而且当有人把 `#include "sarr.h"` 写成 `<sarr.h>`、或者把文件搬出这一节时，
**这一行决定他是当场报错，还是悄悄靠某条别的路径蒙过去。**

这一节是全章最想让你记住的一节：**"删了还能编"不等于"这行没用"**。
CMake 里很多行的价值不在编译成败，在**约束的形状**。

### 第 14 节 第 77 行 `target_compile_options(... PRIVATE ${DS_WARNINGS})`

`DS_WARNINGS` 是第 61 行那个三把尺子。先造一个只有 `-Wshadow` 看得见的道具：
`src/warn/leaf.c` 里外层有个 `int total`，内层又写了一个同名 `total`。三档实验：

```
$ cmake -S /tmp/cmake08-aiyAIw/p/warnfull -B /tmp/cmake08-aiyAIw/b/warnfull >/dev/null 2>&1 && cmake --build /tmp/cmake08-aiyAIw/b/warnfull 2>&1 | grep -E 'warning|shadow' | head -4
/tmp/cmake08-aiyAIw/p/warnfull/mini/05-warn/leaf.c:11:13: warning: declaration of ‘total’ shadows a previous local [-Wshadow]
   11 |         int total = 32;                 /* ← -Wshadow 在这里响 */
/tmp/cmake08-aiyAIw/p/warnfull/mini/05-warn/leaf.c:8:9: note: shadowed declaration is here
[退出码 0]
```

```
$ grep '^C_FLAGS' /tmp/cmake08-aiyAIw/b/warnfull/CMakeFiles/mini-05-warn.dir/flags.make
C_FLAGS = -std=gnu11 -Wall -Wextra -Wshadow
[退出码 0]
```

★ 全套三把尺子 → 报 `declaration of 'total' shadows a previous local [-Wshadow]`，
并且 `C_FLAGS` 里那三个 `-W` 齐全。

```
$ cmake -S /tmp/cmake08-aiyAIw/p/wallonly -B /tmp/cmake08-aiyAIw/b/wallonly >/dev/null 2>&1 && cmake --build /tmp/cmake08-aiyAIw/b/wallonly 2>&1 | grep -E 'warning' | grep -c .
0
[退出码 1]
```

```
$ cmake -S /tmp/cmake08-aiyAIw/p/nowarn -B /tmp/cmake08-aiyAIw/b/nowarn >/dev/null 2>&1 && cmake --build /tmp/cmake08-aiyAIw/b/nowarn 2>&1 | grep -E 'warning' | grep -c .
0
[退出码 1]
```

★ 三档对质：全套 → 有 warning；只留 `-Wall` → **0 条**；第 77 行整个删掉 → **0 条**。
后两个 0 的退出码是 1，那是 `grep -c` 没匹配，管道本身跑成功了 ——
**"屏幕上没有 warning"这件事不能用退出码判断**（`gcc` 只报 warning 时退 0，`grep -c` 反而退 1，很容易读反）。

第 77 行删掉之后，那份 `flags.make`：

```
$ grep -E '^(C_FLAGS|C_INCLUDES)' /tmp/cmake08-aiyAIw/b/nowarn/CMakeFiles/mini-05-warn.dir/flags.make
C_INCLUDES = -I/tmp/cmake08-aiyAIw/p/nowarn/mini/05-warn
C_FLAGS = -std=gnu11
[退出码 0]
```

★ 警告没了，`-std=gnu11` **还在**。
这一屏是本章作用域课的实物版：第 51~53 行是**项目级**的（所有 target 共享），
第 77 行是 **per-target** 的（只有这个 target 的三把尺子）。两件事、两条路、两个落点。

再补一条位置规矩 —— `target_*` 命令必须在 target 存在之后：

```
$ cmake -S /tmp/cmake08-aiyAIw/p/optbefore -B /tmp/cmake08-aiyAIw/b/optbefore 2>&1 | tail -6
CMake Error at CMakeLists.txt:3 (target_compile_options):
  Cannot specify compile options for target "early" which is not built by
  this project.


-- Configuring incomplete, errors occurred!
[退出码 1]
```

★ `Cannot specify compile options for target "early" which is not built by this project.`
**target 版命令的失败是当场响的**（退出码 1），这是它比目录级命令好的地方之一：
目录级的 `add_compile_options` 写在 `add_executable` **之后**是"静默无效"（第 06 章第 15 节演过），
target 级写在之前是"直接报错"。真文件把规则收在函数里、又总是先 `add_executable` 再 `target_*`，
两个坑都绕开了 —— 这个顺序不是巧合，是唯一能工作的顺序。

### 第 15 节 第 79~84 行 ASan 那四行：编译期和链接期必须一起变

拨开关（`option` 那一行不动，只给 `-D`）：

```
$ cmake -S /tmp/cmake08-aiyAIw/proj -B /tmp/cmake08-aiyAIw/b/asan -DDS_WITH_ASAN=ON 2>&1 | tail -7
--   警告集：-Wall;-Wextra;-Wshadow
--   ASan：ON （想开就 -DDS_WITH_ASAN=ON 重新 configure）
--   产物目录：/tmp/cmake08-aiyAIw/b/asan/bin
-- --------------------------------------------------
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake08-aiyAIw/b/asan
[退出码 0]
```

```
$ grep '^C_FLAGS' /tmp/cmake08-aiyAIw/b/asan/CMakeFiles/mini-02-linked-list-slist.dir/flags.make
C_FLAGS = -std=gnu11 -Wall -Wextra -Wshadow -fsanitize=address -g -fno-omit-frame-pointer
[退出码 0]
```

```
$ cat /tmp/cmake08-aiyAIw/b/asan/CMakeFiles/mini-02-linked-list-slist.dir/link.txt
/usr/bin/cc -fsanitize=address -static-libasan -Wl,--dependency-file=CMakeFiles/mini-02-linked-list-slist.dir/link.d "CMakeFiles/mini-02-linked-list-slist.dir/mini/02-linked-list/slist/leaf.c.o" -o bin/mini-02-linked-list-slist
[退出码 0]
```

★ 同一份源码、同一个 target，两处**各多了一段**：`flags.make` 里多
`-fsanitize=address -g -fno-omit-frame-pointer`（编译期插检查），`link.txt` 里多
`-fsanitize=address -static-libasan`（链接期带运行时）。这就是第 06 章第 16 节"两半成对"在真文件形状上的样子；
和第 3 节没开开关的那份对比，差别只有 `if(DS_WITH_ASAN)` 那 4 行，其余一个字没动。

跑测试看结论变了没：

```
$ cmake --build /tmp/cmake08-aiyAIw/b/asan -j4 >/dev/null 2>&1 && ctest --test-dir /tmp/cmake08-aiyAIw/b/asan --output-on-failure 2>&1 | tail -14
    #4 0x5889d2785684 in _start (/tmp/cmake08-aiyAIw/b/asan/bin/mini-02-linked-list-slist+0x9684) (BuildId: 7b54f3f3e7b600e81e02bd2bf321bead24c8c93d)

SUMMARY: AddressSanitizer: 12 byte(s) leaked in 3 allocation(s).

    Start 4: smoke_mini-03-stack-array-stack
4/4 Test #4: smoke_mini-03-stack-array-stack .............   Passed    0.01 sec

75% tests passed, 1 tests failed out of 4

Total Test time (real) =   0.10 sec

The following tests FAILED:
	  3 - smoke_mini-02-linked-list-slist (Failed)
Errors while running CTest
[退出码 8]
```

★ 名单还是 4 条（第 18 节单看），变的是**"通过"的定义**：那节漏 12 字节的链表当场红，`ctest` 退 **8**。
这是本章对"这份文件在干什么"最有力的一次回答：第 87 行那 56 条 `add_test` 只是"把这节跑一遍，退出码必须是 0"，
而第 79~84 行决定了**"退出码 0"这句话有多重**。
（`tail -14` 截掉了栈帧上半部分，完整报告在第 17 节；PID / 地址 / `BuildId` 每次都变。）

### 第 16 节 第 82~83 行那句注释（本机不带共享版 asan）在本机**不成立**

第 83 行的 `-static-libasan` 头上写着理由（第 82 行）。这是本文件的**第三条、也是最有价值的一条待验说明**：

```
$ 〔在 9a503fd8 里〕 grep -n "libasan\|共享版" CMakeLists.txt
82:        # 本机系统库不带共享版 asan，必须静态链接，否则运行时找不到 libasan.so
83:        target_link_options(${_target} PRIVATE -fsanitize=address -static-libasan)
[退出码 0]
```

系统目录里到底有没有共享版 asan：

```
$ ls -1 /usr/lib/x86_64-linux-gnu/libasan.so*
/usr/lib/x86_64-linux-gnu/libasan.so.8
/usr/lib/x86_64-linux-gnu/libasan.so.8.0.0
[退出码 0]
```

★ **`libasan.so.8` 就在这台机器的系统目录里。** 所以"必须静态链接"这个前提是**过时的**（或者属于另一台机器）。
把那一行的 `-static-libasan` 删掉，其他不动：

```
$ grep -n 'fsanitize=address' /tmp/cmake08-aiyAIw/p/asanshared/CMakeLists.txt
62:        target_compile_options(${_target} PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
64:        target_link_options(${_target} PRIVATE -fsanitize=address)
[退出码 0]
```

```
$ cmake -S /tmp/cmake08-aiyAIw/p/asanshared -B /tmp/cmake08-aiyAIw/b/shared -DDS_WITH_ASAN=ON >/dev/null 2>&1 && cmake --build /tmp/cmake08-aiyAIw/b/shared -j4 >/dev/null 2>&1 && ldd /tmp/cmake08-aiyAIw/b/shared/bin/mini-01-array-static-array | grep -E 'asan'
	libasan.so.8 => /usr/lib/x86_64-linux-gnu/libasan.so.8 (0x00007c23db600000)
[退出码 0]
```

```
$ ldd /tmp/cmake08-aiyAIw/b/asan/bin/mini-01-array-static-array | grep -c asan
0
[退出码 1]
```

```
$ /tmp/cmake08-aiyAIw/b/shared/bin/mini-01-array-static-array
static-array sum=14
[退出码 0]
```

★ 对照得很干净：**不加** → `ldd` 里明明白白一行 `libasan.so.8 => /usr/lib/x86_64-linux-gnu/...`，
程序照跑（`static-array sum=14`）；**加** → `ldd` 里 asan 出现 **0 次**（退 1 是 `grep -c` 没匹配），
运行时库整个塞进可执行文件里。

代价是多少：

```
$ stat -c '%s 字节  %n' /tmp/cmake08-aiyAIw/b/asan/bin/mini-01-array-static-array /tmp/cmake08-aiyAIw/b/shared/bin/mini-01-array-static-array
1987784 字节  /tmp/cmake08-aiyAIw/b/asan/bin/mini-01-array-static-array
27032 字节  /tmp/cmake08-aiyAIw/b/shared/bin/mini-01-array-static-array
[退出码 0]
```

★ **同一个 target，静态版是动态版的 70 多倍**（1987784 字节 vs 27032 字节）。
真仓库是 56 个 target，开一次 ASan 就是 56 份这个差额 ——
注意这一句是**从上面两个数推出来的**，不是本机实测 56 份的总和（本章不会去跑真仓库，见文末「没演」）。

那这条注释**什么时候成立**？想清楚适用场景比知道它本机错了更有用：
① 跑构建的机器和跑产物的机器**不是同一台**，而目标机器没有对应版本的 `libasan.so.8`
（gcc 大版本一换，so 名字就变：`libasan.so.8` → `libasan.so.9`）；② CI 容器没装 `libasan` 那个 runtime 包
（很多发行版把它单独分包）；③ 你要把"能复现问题的产物"发给别人。
老实的说法是：**第 83 行不是"必须"，是"把 asan 运行时打进产物，换机器少一个依赖"**。
这一处最值钱，因为它是典型的"代码没错、说明错了"：信了注释的人会以为 `-static-libasan` 是 sanitizer
生效的前提，转而去查"为什么找不到 libasan.so"；而真相是这台机器根本不缺。

### 第 17 节 第 80~81 行那个 `-g`：报告里 `leaf.c:17` 那行字是它给的

第 80 行的注释是「`-g` 让 ASan 报到具体行号」。这一句**本机实测成立**，证法是对照：

```
$ cd /tmp/cmake08-aiyAIw/b/asan && ./bin/mini-02-linked-list-slist 2>&1 | head -12

=================================================================
==52910==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 12 byte(s) in 3 object(s) allocated from:
    #0 0x5bae9819c77b in malloc (/tmp/cmake08-aiyAIw/b/asan/bin/mini-02-linked-list-slist+0xf077b) (BuildId: 7b54f3f3e7b600e81e02bd2bf321bead24c8c93d)
    #1 0x5bae981ffaeb in main /tmp/cmake08-aiyAIw/proj/mini/02-linked-list/slist/leaf.c:17
    #2 0x7947fc22a600 in __libc_start_call_main ../sysdeps/nptl/libc_start_call_main.h:59
    #3 0x7947fc22a717 in __libc_start_main_impl ../csu/libc-start.c:360
    #4 0x5bae980b5684 in _start (/tmp/cmake08-aiyAIw/b/asan/bin/mini-02-linked-list-slist+0x9684) (BuildId: 7b54f3f3e7b600e81e02bd2bf321bead24c8c93d)

SUMMARY: AddressSanitizer: 12 byte(s) leaked in 3 allocation(s).
[退出码 1]
```

★ 有 `-g`：`Direct leak of 12 byte(s)` 的栈帧里第 1 帧是
`in main /tmp/.../mini/02-linked-list/slist/leaf.c:17` —— 源文件 + 行号，直接点到那三行 `malloc`。

把第 81 行里的 `-g` 摘掉（asan 两半都还在，帧指针也还在）：

```
$ grep -n 'fsanitize=address' /tmp/cmake08-aiyAIw/p/asannog/CMakeLists.txt
62:        target_compile_options(${_target} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
64:        target_link_options(${_target} PRIVATE -fsanitize=address -static-libasan)
[退出码 0]
```

```
$ cmake -S /tmp/cmake08-aiyAIw/p/asannog -B /tmp/cmake08-aiyAIw/b/asannog -DDS_WITH_ASAN=ON >/dev/null 2>&1 && grep '^C_FLAGS' /tmp/cmake08-aiyAIw/b/asannog/CMakeFiles/mini-02-linked-list-slist.dir/flags.make
C_FLAGS = -std=gnu11 -Wall -Wextra -Wshadow -fsanitize=address -fno-omit-frame-pointer
[退出码 0]
```

```
$ cd /tmp/cmake08-aiyAIw/b/asannog && cmake --build . -j4 >/dev/null 2>&1 && ./bin/mini-02-linked-list-slist 2>&1 | head -12

=================================================================
==53023==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 12 byte(s) in 3 object(s) allocated from:
    #0 0x5d311371377b in malloc (/tmp/cmake08-aiyAIw/b/asannog/bin/mini-02-linked-list-slist+0xf077b) (BuildId: b45a32bd4f250fb38d7951457c27c131a9336317)
    #1 0x5d3113776aeb in main (/tmp/cmake08-aiyAIw/b/asannog/bin/mini-02-linked-list-slist+0x153aeb) (BuildId: b45a32bd4f250fb38d7951457c27c131a9336317)
    #2 0x760e8542a600 in __libc_start_call_main ../sysdeps/nptl/libc_start_call_main.h:59
    #3 0x760e8542a717 in __libc_start_main_impl ../csu/libc-start.c:360
    #4 0x5d311362c684 in _start (/tmp/cmake08-aiyAIw/b/asannog/bin/mini-02-linked-list-slist+0x9684) (BuildId: b45a32bd4f250fb38d7951457c27c131a9336317)

SUMMARY: AddressSanitizer: 12 byte(s) leaked in 3 allocation(s).
[退出码 1]
```

★ 只把 `-g` 摘掉：asan 照样进编译和链接两半、照样报出 12 字节泄漏，但那一帧变成
`in main (/tmp/.../bin/mini-02-linked-list-slist+0x153aeb) (BuildId: b45a...)` ——
**行号没了，只剩可执行文件 + 偏移**。抓到了，指不出在哪行。
两屏的数字（PID、地址、BuildId）全都不一样，但**形状**的差别只有一处，而那一处正是第 81 行那个 `-g` 给的。

现在把本文件的三条注释放一起结算：
★ 第 51 行"对应 Makefile 的 `-std=c11`"——**不成立**（第 7 节）；
第 82 行"本机必须静态链接"——**不成立**（第 16 节）；
第 80 行"`-g` 让 ASan 报到具体行号"——**成立**（就是上面两屏对照）。
注释是给人看的承诺，不是机器验过的结论。**逐行拆的意思就是逐行对，不对不采信。**

这一节还顺手抓到一个真工程里很坑的现象：

```
$ /tmp/cmake08-aiyAIw/b/asan/bin/mini-02-linked-list-slist > /tmp/cmake08-aiyAIw/o1.txt 2> /tmp/cmake08-aiyAIw/e1.txt ; wc -c /tmp/cmake08-aiyAIw/o1.txt /tmp/cmake08-aiyAIw/e1.txt
  0 /tmp/cmake08-aiyAIw/o1.txt
798 /tmp/cmake08-aiyAIw/e1.txt
798 total
[退出码 0]
```

★ 这一节程序的 **stdout 是 0 字节**。不是它没跑，是 LSan 报完泄漏就用非正常路径退出，
stdio 缓冲区里那句 `slist total=6` 没来得及落地（stderr 798 字节，报告全在）。

```
$ ASAN_OPTIONS=detect_leaks=0 /tmp/cmake08-aiyAIw/b/asan/bin/mini-02-linked-list-slist
slist total=6
[退出码 0]
```

★ 把漏检关掉（asan 还在），那行 stdout 立刻回来了，退出码 0。
所以那 0 字节是**"报泄漏的退出方式"的副作用**，不是程序没执行。
读 ASan 的 `ctest` 日志时，**别把"看不到程序输出"当成"程序没执行到"** ——
这个误判会让你去查一个根本不存在的路径问题。

最后回到真仓库，看一眼不开 ASan 时它的编译标志：

```
$ 〔在 9a503fd8 里〕 grep -h "^C_FLAGS" build-cmake/CMakeFiles/01-array-dynamic-array.dir/flags.make
C_FLAGS = -std=gnu11 -Wall -Wextra -Wshadow
[退出码 0]
```

★ 一个 `-g` 都没有。这条链在第 21 节合上。


---

## 第 3 部分 · 第 87~233 行：接线、总闸、和那 56 个调用点（第 18~21 节）

函数定义了规则，但规则要**被执行 56 次**才成为这个工程。这部分是本文件"体量"的来源，
也是它最容易被改坏的一段（`file(GLOB)` 一替换，249 行变 5 行，看起来还更整洁）。

### 第 18 节 第 87 行 `add_test(NAME smoke_${_target} COMMAND ${_target})`：名单长什么样

先看复刻的名单文件（`grep` 把两种行都捞出来了）：

```
$ grep -E 'add_test|BACKTRACE' /tmp/cmake08-aiyAIw/b/main/CTestTestfile.cmake | head -4
add_test(smoke_mini-00-c-foundation-pointer-basics "/tmp/cmake08-aiyAIw/b/main/bin/mini-00-c-foundation-pointer-basics")
set_tests_properties(smoke_mini-00-c-foundation-pointer-basics PROPERTIES  _BACKTRACE_TRIPLES "/tmp/cmake08-aiyAIw/proj/CMakeLists.txt;68;add_test;/tmp/cmake08-aiyAIw/proj/CMakeLists.txt;79;ds_add_leaf;/tmp/cmake08-aiyAIw/proj/CMakeLists.txt;0;")
add_test(smoke_mini-01-array-static-array "/tmp/cmake08-aiyAIw/b/main/bin/mini-01-array-static-array")
set_tests_properties(smoke_mini-01-array-static-array PROPERTIES  _BACKTRACE_TRIPLES "/tmp/cmake08-aiyAIw/proj/CMakeLists.txt;68;add_test;/tmp/cmake08-aiyAIw/proj/CMakeLists.txt;82;ds_add_leaf;/tmp/cmake08-aiyAIw/proj/CMakeLists.txt;0;")
[退出码 0]
```

★ 两件东西同时在这里：
① `add_test(smoke_mini-00-... "/绝对路径/bin/mini-00-...")` —— **可执行文件的绝对路径是配置期展开的**，
所以裸 target 名和 `$<TARGET_FILE:...>` 到这里已经是同一个东西（第 06 章第 4、12 节的账）；
② `set_tests_properties(... _BACKTRACE_TRIPLES "...CMakeLists.txt;68;add_test;...;79;ds_add_leaf;...")` ——
**名单文件把"哪一行 add_test、哪一行调的函数"都记下来了**。
在本文件里：`68` 是复刻里 `add_test` 那一行，`79` 是第一个调用点。

`ctest -N` 读同一本账：

```
$ ctest --test-dir /tmp/cmake08-aiyAIw/b/main -N 2>&1 | tail -7
Test project /tmp/cmake08-aiyAIw/b/main
  Test #1: smoke_mini-00-c-foundation-pointer-basics
  Test #2: smoke_mini-01-array-static-array
  Test #3: smoke_mini-02-linked-list-slist
  Test #4: smoke_mini-03-stack-array-stack

Total Tests: 4
[退出码 0]
```

真仓库那 56 条：

```
$ 〔在 9a503fd8 里〕 grep -c "add_test(" build-cmake/CTestTestfile.cmake && grep -c add_test build-cmake/CTestTestfile.cmake
56
112
[退出码 0]
```

★ **56 和 112**。这是第 06 章坑清单第 12 条在真仓库上的复刻：`grep -c add_test`（不带括号）把
`_BACKTRACE_TRIPLES` 里那个 `add_test` 字样也数进去了。**带括号的 56 才是用例数。**
取证要挑对模式，别只认 `grep -c` 一个数。

真仓库的两行原文（同样是**命令里**打码，屏幕上带 `$PWD` 是字面量）：

```
$ 〔在 9a503fd8 里〕 grep -m1 "smoke_01-array-static-array" build-cmake/CTestTestfile.cmake | sed "s|$PWD|<仓库根>|g"
add_test(smoke_01-array-static-array "<仓库根>/build-cmake/bin/01-array-static-array")
[退出码 0]
```

```
$ 〔在 9a503fd8 里〕 grep -m1 "_BACKTRACE_TRIPLES" build-cmake/CTestTestfile.cmake | sed "s|$PWD|<仓库根>|g"
set_tests_properties(smoke_00-c-foundation-pointer-basics PROPERTIES  _BACKTRACE_TRIPLES "<仓库根>/CMakeLists.txt;87;add_test;<仓库根>/CMakeLists.txt;97;ds_add_leaf;<仓库根>/CMakeLists.txt;0;")
[退出码 0]
```

★ 这两行把第 1 节那句"三个口径互相对质"补齐了：
`add_test(smoke_01-array-static-array "<仓库根>/build-cmake/bin/01-array-static-array")`
一行里串着第 68 行的名字规则、第 55 行的产物目录、第 87 行的用例名；
`_BACKTRACE_TRIPLES` 里那两个数字（`87` = 函数里那行 `add_test`，`97` = 清单里第一个调用点）意味着
`ctest` 报某条用例出事时，你能一路回指到 `CMakeLists.txt` 的具体行。
用例名是 `smoke_` + target 名，和 target 名**不完全一样** —— 第 06 章第 4 节那两种命名写法之一。

### 第 19 节 第 90 行 `enable_testing()` 的位置：致命的是**缺失**，不是先后

真文件里它在第 90 行 —— 函数之后、56 个调用点之前。先确认：

```
$ 〔在 9a503fd8 里〕 grep -n "^enable_testing" CMakeLists.txt
90:enable_testing()
[退出码 0]
```

现在把它挪到**文件末尾**（所有 `add_test` 之后）：

```
$ grep -n 'enable_testing' /tmp/cmake08-aiyAIw/p/late/CMakeLists.txt
104:enable_testing()
[退出码 0]
```

```
$ cmake -S /tmp/cmake08-aiyAIw/p/late -B /tmp/cmake08-aiyAIw/b/late >/dev/null 2>&1 && ls -1 /tmp/cmake08-aiyAIw/b/late/CTestTestfile.cmake && ctest --test-dir /tmp/cmake08-aiyAIw/b/late -N 2>&1 | tail -2
/tmp/cmake08-aiyAIw/b/late/CTestTestfile.cmake

Total Tests: 4
[退出码 0]
```

★ 名单照样有 4 条。**同一份 `CMakeLists.txt` 目录里，先后顺序不影响结果。**
所以第 06 章第 2 节那块"哑屏幕"的开关是**这一行在不在**，不是它在第几行。

那删掉呢：

```
$ grep -c 'enable_testing' /tmp/cmake08-aiyAIw/p/nonetest/CMakeLists.txt
0
[退出码 1]
```

```
$ cmake -S /tmp/cmake08-aiyAIw/p/nonetest -B /tmp/cmake08-aiyAIw/b/nonetest >/dev/null 2>&1 && ls -1 /tmp/cmake08-aiyAIw/b/nonetest/CTestTestfile.cmake ; ctest --test-dir /tmp/cmake08-aiyAIw/b/nonetest 2>&1 | tail -2
ls: cannot access '/tmp/cmake08-aiyAIw/b/nonetest/CTestTestfile.cmake': No such file or directory
Test project /tmp/cmake08-aiyAIw/b/nonetest
No tests were found!!!
[退出码 0]
```

★ 整行删掉才是真事故，三件事同时发生：`CTestTestfile.cmake` **根本不生成**（`ls` 报 No such file）、
`ctest` 打 `No tests were found!!!`、**退出码 0**。
`add_test` 一条没少写，构建一声不吭，验收闸门整个变成空气。

那真文件为什么还要管位置？两个理由都不在"能不能跑"这一层：
① **给人读**：写在函数之后、清单之前，读起来是"先立总闸再接线"；
② **将来分目录时它变成硬约束**：`enable_testing()` 是目录属性，`add_subdirectory` 引入的子目录
只有在它**之后**才继承得到名单（本章没演多目录，见「没演」）。
所以第 90 行的正确评价是：**在这一份文件里不是必需的，但它是"将来不会变坏"的那种写法。**

### 第 20 节 第 92~233 行那 56 个调用点：手工清单 vs `file(GLOB)`

先数清单：

```
$ 〔在 9a503fd8 里〕 grep -c "^ds_add_leaf(" CMakeLists.txt && grep -c "ds_add_leaf(" CMakeLists.txt
56
58
[退出码 0]
```

★ 56 和 58。第二个数把第 37、64 行两条**注释**里出现的 `ds_add_leaf(` 也算进去了。
（第 37 行是「重复 56 次的样板抽进一个 `ds_add_leaf()` 函数」，第 64 行是函数说明块的标题。）
这条对质顺带证明：**清单确实是 56 行**，不是"大概几十个"。

再看同一台机器上另一条路线怎么数：

```
$ 〔在 9a503fd8 里〕 find . -mindepth 3 -maxdepth 3 -name main.c -not -path "./build*" | wc -l
56
[退出码 0]
```

```
$ 〔在 9a503fd8 里〕 grep -n "mindepth" Makefile
6:RAW     := $(shell find . -mindepth 3 -maxdepth 3 -name main.c -not -path './$(BIN)*' 2>/dev/null)
[退出码 0]
```

★ `Makefile` 现场 `find` 到 56 个 `main.c`，CMake 清单里也正好 56 个调用点。
**两个 56 相等不是巧合，是有人在对账**（这份文件第 35~36 行那两条注释讲的就是这个时机差别：
`Make` 的 recipe 每次执行都重跑 `find`，`CMake` 的清单只在生成阶段跑一次）。
这也是本章认为这份文件"是范例"的主要理由：**它把两种发现方式的产出绑成了同一个数**。

现在做那个"省事版"实验 —— 把清单换成 `file(GLOB)`，然后用"新加一个 `.c`"来验它会不会漏：

```
$ cmake -S /tmp/cmake08-aiyAIw/p/glob -B /tmp/cmake08-aiyAIw/b/glob >/dev/null 2>&1 && cmake --build /tmp/cmake08-aiyAIw/b/glob 2>&1 | tail -2 && /tmp/cmake08-aiyAIw/b/glob/app
[100%] Linking C executable app
[100%] Built target app
glob-probe main only
[退出码 0]
```

第一步正常：链接了、跑起来了，屏幕上只有 `glob-probe main only`。现在**只往目录里扔一个新文件**（`ctor.c`，
它带 `__attribute__((constructor))`，只要参与编译就一定会在程序开头打一行字），**不重新 configure**：

```
$ cp /tmp/cmake08-aiyAIw/proj/src/glob/ctor.c /tmp/cmake08-aiyAIw/p/glob/leaf/ && cmake --build /tmp/cmake08-aiyAIw/b/glob 2>&1 | tail -2
[100%] Built target app
[退出码 0]
```

```
$ grep -c 'ctor.c.o' /tmp/cmake08-aiyAIw/b/glob/CMakeFiles/app.dir/link.txt
0
[退出码 1]
```

```
$ /tmp/cmake08-aiyAIw/b/glob/app
glob-probe main only
[退出码 0]
```

★ 三重证据同时说"它没参与"：构建输出说 `[100%] Built target app`（**什么都没干**，退 0）；
`link.txt` 里搜 `ctor.c.o` 得到 0；跑起来屏幕上没有 `[ctor.c 参与了]` 那行。
文件**真的躺在目录里**，构建系统却说它不存在 —— 第 31~34 行那句"玄学问题"现在有屏幕证据了。

补救办法当然是重新 configure：

```
$ cmake -S /tmp/cmake08-aiyAIw/p/glob -B /tmp/cmake08-aiyAIw/b/glob >/dev/null 2>&1 && cmake --build /tmp/cmake08-aiyAIw/b/glob 2>&1 | tail -2
[ 66%] Linking C executable app
[100%] Built target app
[退出码 0]
```

```
$ /tmp/cmake08-aiyAIw/b/glob/app
[ctor.c 参与了]
glob-probe main only
[退出码 0]
```

★ 一次 `cmake -S ... -B ...` 就进去了（这次屏幕上是 `[ 66%] Linking`，真的重链了），`[ctor.c 参与了]` 出来了。
**所以差别不在"能不能"，在"谁被迫重跑清单"。**
`GLOB` 的问题不是它扫目录，是**没有任何东西因为目录里多了一个文件而失效**。
`make` 会不会重跑 configure？不会 —— 它只看 `.o` 比 `.c` 新不新，而那个新文件从来没进过依赖表。

手工清单的代价是啰嗦（249 行里 142 行是调用点），回报是：**新增一个 `.c` 没写进清单时报
`Cannot find source file`（第 11 节），当场响。** 教学工程最怕的不是啰嗦，是"学生挪了目录，
构建悄悄少编一个文件，于是他的程序行为和讲义不一致"。
（`GLOB` 的两种合法用法和 `CONFIGURE_DEPENDS` 的可靠性是第 07 章的范围，**该章在本仓库尚未收口**，本章不下结论。）

### 第 21 节 `CMAKE_BUILD_TYPE` 是空的：这一件事在真工程里的后果

先把第 1 节那行空缓存的后果量化。真仓库 56 份 `flags.make`：

```
$ 〔在 9a503fd8 里〕 grep -h "^C_FLAGS" build-cmake/CMakeFiles/*.dir/flags.make | wc -l
56
[退出码 0]
```

```
$ 〔在 9a503fd8 里〕 grep -h "^C_FLAGS" build-cmake/CMakeFiles/*.dir/flags.make | sort -u
C_FLAGS = -std=gnu11 -Wall -Wextra -Wshadow
[退出码 0]
```

★ **56 份，去掉路径之后只有一种 `C_FLAGS`**：`-std=gnu11 -Wall -Wextra -Wshadow`。
这一屏同时是第 14 节那条"per-target 警告集"的证据：56 个 target 用的是同一份 `DS_WARNINGS`，
而它是通过 `target_compile_options(... PRIVATE ...)` 给的 —— 不是通过目录级 `add_compile_options` 蒙出来的。

```
$ 〔在 9a503fd8 里〕 grep -h "^C_FLAGS" build-cmake/CMakeFiles/*.dir/flags.make | grep -c -- "-O"
0
[退出码 1]
```

```
$ 〔在 9a503fd8 里〕 grep -h "^C_FLAGS" build-cmake/CMakeFiles/*.dir/flags.make | grep -c -- "-g"
0
[退出码 1]
```

★ 两个 0：**既没有 `-O` 也没有 `-g`**（退出码 1 是 `grep` 没匹配，不是命令坏了）。
原因就在缓存里那句 `CMAKE_BUILD_TYPE:STRING=`（第 05 章旋钮二讲过：CMake 的默认**不是** Debug）。
也就是说这台机器上那 56 个可执行文件**既不优化、也不带调试信息**。对教学工程这甚至更好
（行为和源码顺序一致），但它有三个真实后果，值得你自己判断要不要接受：

1. 没有 `-g`：崩溃或 sanitizer 报栈时行号得靠别处补（第 17 节）。
2. 没有 `-O`：性能演示（比如动态数组扩容那节）测出来的数字不能拿去比较。
3. 没有 `-DNDEBUG`：`assert()` 是开着的 —— 这一条**对教学是优点**，讲义靠 `assert` 表达不变式。

三个旋钮在复刻上拨一遍，把"谁给什么"钉死：

```
$ cmake -S /tmp/cmake08-aiyAIw/proj -B /tmp/cmake08-aiyAIw/b/dbg -DCMAKE_BUILD_TYPE=Debug >/dev/null 2>&1 && grep '^C_FLAGS' /tmp/cmake08-aiyAIw/b/dbg/CMakeFiles/mini-01-array-static-array.dir/flags.make
C_FLAGS = -g -std=gnu11 -Wall -Wextra -Wshadow
[退出码 0]
```

```
$ cmake -S /tmp/cmake08-aiyAIw/proj -B /tmp/cmake08-aiyAIw/b/rel -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1 && grep '^C_FLAGS' /tmp/cmake08-aiyAIw/b/rel/CMakeFiles/mini-01-array-static-array.dir/flags.make
C_FLAGS = -O3 -DNDEBUG -std=gnu11 -Wall -Wextra -Wshadow
[退出码 0]
```

```
$ cmake -S /tmp/cmake08-aiyAIw/proj -B /tmp/cmake08-aiyAIw/b/dbasan -DCMAKE_BUILD_TYPE=Debug -DDS_WITH_ASAN=ON >/dev/null 2>&1 && grep '^C_FLAGS' /tmp/cmake08-aiyAIw/b/dbasan/CMakeFiles/mini-02-linked-list-slist.dir/flags.make
C_FLAGS = -g -std=gnu11 -Wall -Wextra -Wshadow -fsanitize=address -g -fno-omit-frame-pointer
[退出码 0]
```

★ 看清楚这三行：`Debug` 给 `-g`、`Release` 给 `-O3 -DNDEBUG`、什么都不设就**什么都不给**。
第三行是 `-g` 出现了**两次**（`-g -std=gnu11 ... -fsanitize=address -g -fno-omit-frame-pointer`）——
一次来自 `CMAKE_C_FLAGS_DEBUG`，一次来自真文件第 81 行。重复的 `-g` 无害，
但它说明一件事：**这份文件在 ASan 档里自己补了 `-g`，不指望 `BUILD_TYPE`。**

★ **这条链在这里合上了**：本文件里 `-g` 只有两个来源 —— `CMAKE_BUILD_TYPE`（文件根本没设默认值）
或 `DS_WITH_ASAN=ON`（函数里补一个）。所以"平时没调试信息、一开 ASan 就有行号"不是巧合，
是第 80~81 行那一个 `-g` 顶上来的。这是一个**设计**，只是没写在注释里。

顺手把 ASan 档当验收闸门（这一条是**沙箱实测**）：

```
$ cmake -S /tmp/cmake08-aiyAIw/proj -B /tmp/cmake08-aiyAIw/b/asan2 -DDS_WITH_ASAN=ON >/dev/null 2>&1 && cmake --build /tmp/cmake08-aiyAIw/b/asan2 -j4 >/dev/null 2>&1 && cd /tmp/cmake08-aiyAIw/b/asan2 && ctest 2>&1 | tail -5
The following tests FAILED:
	  3 - smoke_mini-02-linked-list-slist (Failed)
Errors while running CTest
Output from these tests are in: /tmp/cmake08-aiyAIw/b/asan2/Testing/Temporary/LastTest.log
Use "--rerun-failed --output-on-failure" to re-run the failed cases verbosely.
[退出码 8]
```

★ 4 条用例、1 条红、退出码 **8**，并且它直接告诉你日志文件在哪。这就是 CI 能拿的结论形状。

把这两个旋钮固化成 preset（同样是**沙箱实测**，工程是复刻）：

```
$ cd /tmp/cmake08-aiyAIw/p/preset && cmake --preset ci-asan 2>&1 | tail -4
-- --------------------------------------------------
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake08-aiyAIw/p/preset/out/ci-asan
[退出码 0]
```

```
$ cd /tmp/cmake08-aiyAIw/p/preset && cmake --build --preset ci-asan >/dev/null 2>&1 && ctest --preset ci-asan 2>&1 | tail -5
Total Test time (real) =   0.10 sec

The following tests FAILED:
	  3 - smoke_mini-02-linked-list-slist (Failed)
Errors while running CTest
[退出码 8]
```

```
$ grep '^C_FLAGS' /tmp/cmake08-aiyAIw/p/preset/out/ci-asan/CMakeFiles/mini-01-array-static-array.dir/flags.make
C_FLAGS = -g -std=gnu11 -Wall -Wextra -Wshadow -fsanitize=address -g -fno-omit-frame-pointer
[退出码 0]
```

★ preset 里一个 `hidden` 底座（给 `CMAKE_BUILD_TYPE=Debug`）+ 一个 `ci-asan`（继承它、再开 `DS_WITH_ASAN=ON`），
三条 `--preset` 命令就跑出上面那套闸门，最后一行 `C_FLAGS` 证明两半都到位。
**这一段是"沙箱里实测"的形状，不是对真仓库的落地建议** —— 真仓库现在**没有** `CMakePresets.json`
（第 24 节核对），要不要加、加哪几档是给作者的决定。
同样的闸门在真仓库上的等价命令是文件第 15~18 行注释里那三行（换成
`-DCMAKE_BUILD_TYPE=Debug -DDS_WITH_ASAN=ON`）：**本章没有对真仓库跑过它们**（红线：那会改动物证树），
所以这一句请当**本机推演**读，别当实测。

---

## 第 4 部分 · 收尾的两段（235~249）和"只编某一节"（第 22~24 节）

### 第 22 节 第 235~249 行那段 `message`：那个数字其实会把库也数进去

先看原文是哪几行：

```
$ 〔在 9a503fd8 里〕 grep -n "BUILDSYSTEM_TARGETS\|list(LENGTH\|_n_targets" CMakeLists.txt
238:get_property(_all_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
239:list(LENGTH _all_targets _n_targets)
243:message(STATUS "  可执行 target 数量：${_n_targets}（一节一个）")
[退出码 0]
```

★ 真文件里"可执行 target 数量"那句之所以可信，是因为**这份文件只造可执行文件**
（清单 56 行 = 产物 56 个，第 1 节那两个数早就对上了）。

但那句用的是 `BUILDSYSTEM_TARGETS`，它数的是**本目录里所有构建期 target**。加一个静态库试试：

```
$ grep -n 'add_library\|^add_test' /tmp/cmake08-aiyAIw/p/lib/CMakeLists.txt
90:add_library(numops STATIC src/lib/numops.c)
[退出码 0]
```

```
$ cmake -S /tmp/cmake08-aiyAIw/p/lib -B /tmp/cmake08-aiyAIw/b/lib 2>&1 | grep -E 'target 数量|警告集|ASan|产物目录'
--   可执行 target 数量：5（一节一个）
--   警告集：-Wall;-Wextra;-Wshadow
--   ASan：OFF （想开就 -DDS_WITH_ASAN=ON 重新 configure）
--   产物目录：/tmp/cmake08-aiyAIw/b/lib/bin
[退出码 0]
```

```
$ ctest --test-dir /tmp/cmake08-aiyAIw/b/lib -N 2>&1 | tail -2

Total Tests: 4
[退出码 0]
```

★ 加了一个 `add_library(numops STATIC ...)` 之后：回显说 **5**，用例还是 **4**、产物还是 4 个可执行文件。
`BUILDSYSTEM_TARGETS` 里可执行 / 静态库 / 共享库 / INTERFACE 库**不分家**。
所以第 243 行那句 message 的正确读法是：
**"这个数字只在这份文件不造库的时候等于可执行文件数"**。
今天成立，明天有人把通用工具抽成第一个公共库，它就悄悄假了。
这类"目前正确但依赖前提"的输出值得把前提写进注释 —— 本章挑出的第四条软肋，只不过它在代码里不在注释里。

回显剩下三行的用处：

```
$ cmake -S /tmp/cmake08-aiyAIw/proj -B /tmp/cmake08-aiyAIw/b/echo 2>&1 | tail -7
--   警告集：-Wall;-Wextra;-Wshadow
--   ASan：OFF （想开就 -DDS_WITH_ASAN=ON 重新 configure）
--   产物目录：/tmp/cmake08-aiyAIw/b/echo/bin
-- --------------------------------------------------
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake08-aiyAIw/b/echo
[退出码 0]
```

```
$ cmake -S /tmp/cmake08-aiyAIw/proj -B /tmp/cmake08-aiyAIw/b/echo2 -DDS_WITH_ASAN=ON 2>&1 | grep -E 'ASan|产物目录'
--   ASan：ON （想开就 -DDS_WITH_ASAN=ON 重新 configure）
--   产物目录：/tmp/cmake08-aiyAIw/b/echo2/bin
[退出码 0]
```

★ 这四行回显不是装饰，它们是**本次 configure 的回执**，和第 1 节那四行缓存、第 15 节那份 `flags.make`
是同一条信息（三处能对上，这份文件就没自相矛盾）。两个小读法：`警告集：-Wall;-Wextra;-Wshadow`
用**分号**连，因为 `DS_WARNINGS` 是列表变量；而 ON 的那一屏里"想开就 -DDS_WITH_ASAN=ON 重新 configure"
**文案没变** —— 那句话是写死的字符串，只有前面的变量在变。**回执里的提示语也会过期**，很便宜的一课。

### 第 23 节 第 20~21 行那句"只编某一节"：`--target` 与 `Makefile` 路线的 `one DIR=`

文件开头的用法注释说：`cmake --build build-cmake --target 01-array-dynamic-array`。
在复刻上验一遍（名字要带本目录的 `mini-` 前缀）：

```
$ rm -rf /tmp/cmake08-aiyAIw/b/one && cmake -S /tmp/cmake08-aiyAIw/proj -B /tmp/cmake08-aiyAIw/b/one >/dev/null 2>&1 && cmake --build /tmp/cmake08-aiyAIw/b/one --target mini-01-array-static-array 2>&1 | tail -3 && ls -1 /tmp/cmake08-aiyAIw/b/one/bin
[ 50%] Building C object CMakeFiles/mini-01-array-static-array.dir/mini/01-array/static-array/leaf.c.o
[100%] Linking C executable bin/mini-01-array-static-array
[100%] Built target mini-01-array-static-array
mini-01-array-static-array
[退出码 0]
```

★ `bin/` 里只有一个文件 —— `--target` 真的只编那一个。它可用依赖两件事：
生成器是 `Unix Makefiles`（单配置，target 名就是 make 的目标名），以及**你知道名字怎么变的**（第 10 节）。

名字错了会怎样：

```
$ cmake --build /tmp/cmake08-aiyAIw/b/one --target 01-array-static-array 2>&1 | tail -3
gmake: *** No rule to make target '01-array-static-array'.  Stop.
[退出码 2]
```

★ 第二个名字是"少写了 `mini-` 前缀"的样子（在真仓库里 `01-array-dynamic-array` 就是对的）。
报错来自底层 make：`No rule to make target '01-array-static-array'`，退出码 2 ——
**CMake 不猜你想编哪个** —— `--target` 的名字它只转交给生成器，不做模糊匹配。
打错名字的代价是"多读一句 make 报错"；更大的坑是"你以为编了、其实在别的构建目录编过" ——
所以永远回头看 `bin/` 里实际有什么。

两条路线的"只做一个"对照，读原文：

```
$ 〔在 9a503fd8 里〕 sed -n "20,21p" CMakeLists.txt
#   只编某一节：
#     cmake --build build-cmake --target 01-array-dynamic-array
[退出码 0]
```

```
$ 〔在 9a503fd8 里〕 grep -n "make one DIR" CMakeLists.txt Makefile | head -3
CMakeLists.txt:6:#   · Makefile   —— 你日常手搓单节用的（`make one DIR=...`、`make check`、`make status`）
CMakeLists.txt:248:message(STATUS "  注意：根 Makefile 与本文件互不影响，`make one DIR=...` 照常可用")
Makefile:39:	@test -n "$(DIR)" || { echo "用法: make one DIR=01-array/dynamic-array"; exit 1; }
[退出码 0]
```

★ 两条路线各有一句"只做一个"：CMake 用 **target 名**（`--target 01-array-dynamic-array`），
`Makefile` 用**目录名**（`make one DIR=01-array/dynamic-array`）。
第 2 节那两个名字在这里对上号 —— 这是本章认为"这两条路线是被认真并列维护"的第二处证据
（第一处是两个 56）。注意 `Makefile` 那句用法里带 `exit 1`：**它自己会响**，
而 CMake 那条要靠 make 报错 —— 同一个"参数没给对"的问题，两条路线的反馈强度不一样。

### 第 24 节 一页纸：这份文件的构造清单

`demo.sh` 最后自己打了一张表（**下面是脚本里写死的小抄，不是命令输出**）：

```text
  行段            在做什么                        少写/换写会怎样（本章哪节演过）
  -------------  ------------------------------  ------------------------------------
  43             cmake_minimum_required 3.16     写 3.1 → configure 失败（S4）
  45-48          project(... LANGUAGES C)        省掉 LANGUAGES → 本机找 CXX 炸（S5）
  51-53          C_STANDARD / REQUIRED / EXTENS  删三行 → 无 -std → 本机落到 C23（S6）
                 11                              EXTENSIONS OFF → -std=c11（S6、S7）
  55             RUNTIME_OUTPUT_DIRECTORY/bin    删掉 → 56 个产物散在构建根（S8）
  58             option(DS_WITH_ASAN OFF)        名字拼错 → 静默什么也没开（S9）
  61             set(DS_WARNINGS 三把尺子)        删掉 → -Wshadow 那类问题全绿（S14）
  68             string(REPLACE "/" "-")         不换 → target 名非法（S10）
  71             add_executable(${t} ${ARGN})    ARGN 空 → No SOURCES（S11）
  75             target_include_directories      跨节 include 的强制点（S12）；
                 (PRIVATE 本节目录)               删掉却照样能编（S13）
  77             target_compile_options PRIVATE  写在 add_executable 前 → 报错（S14）
  79-81          ASan 编译期 + -g + 帧指针        没 -g → 报告没有行号（S17）
  83             ASan 链接期 + static-libasan     两半缺一半停在链接（S15）；
                                                  本机不需要 static（S16）
  87             add_test(smoke_ + target)       名单文件里那 56 条（S18）
  90             enable_testing()                整行缺失 → 哑屏幕退 0（S19）
  92-233         56 个手工调用点                  换成 GLOB → 新文件静默不参与（S20）
  238-249        回显：target 数 / 警告集 / ASan   数字含库，不等于可执行数（S22）
```

最后三条自证。第一个是"三个口径互相对质"：

```
$ 〔在 9a503fd8 里〕 ls -1 build-cmake/bin | wc -l && grep -c "^ds_add_leaf(" CMakeLists.txt && grep -c "add_test(" build-cmake/CTestTestfile.cmake
56
56
56
[退出码 0]
```

★ 56 / 56 / 56：产物数、清单行数、名单里的 `add_test` 条数。
**仓库根那棵树本章只读了它，没动它** —— 这三个数和第 1 节开始时一模一样。

第二个是第 21 节那句话的证据：

```
$ 〔在 9a503fd8 里〕 ls -1 CMakePresets.json 2>&1
ls: cannot access 'CMakePresets.json': No such file or directory
[退出码 2]
```

★ 「真仓库现在没有 `CMakePresets.json`」——这就是它的原文，退出码 2 是 `ls` 找不到文件。

第三个是安全边界：

```
$ 〔在 08-repo-cmake-walkthrough 里〕 find . -name CMakeCache.txt -o -name '*.o' -o -name CTestTestfile.cmake -o -type d -name bin -o -type d -name out | wc -l
0
[退出码 0]
```

```
$ du -sh /tmp/cmake08-aiyAIw
45M	/tmp/cmake08-aiyAIw
[退出码 0]
```

★ **0** —— 章节目录里一个构建产物都没有（`CMakeCache.txt`、`.o`、`CTestTestfile.cmake`、`bin/`、`out/` 全查）。
本章 24 节写出来的东西全在沙箱里，跑完自动删；沙箱一共 45M，其中一大半是第 16 节那两份 asan 产物。

---

## 对照表：一份文件的三本账 × 本章的取证命令

| 想知道的事 | 读哪本账 | 本章用的命令（都是只读） | 哪节 |
|---|---|---|---|
| 这份文件到底造了多少东西 | `bin/` + `CMakeFiles/*.dir` | `ls ... \| wc -l`、`ls -d *.dir \| wc -l` | 1、8、24 |
| 现在用的是哪台编译器 / 哪个生成器 | `CMakeCache.txt` | `grep -E "^(CMAKE_C_COMPILER\|CMAKE_GENERATOR):"` | 1、5 |
| 开关到底开没开 | `CMakeCache.txt` | `grep -E '^DS_WITH_ASAN'` / `cmake -L` | 9 |
| 有没有 `-std`、有没有警告、有没有 `-g` | `flags.make` 的 `C_FLAGS` | `grep '^C_FLAGS'` | 3、6、14、21 |
| 这个 target 能看见哪些头目录 | `flags.make` 的 `C_INCLUDES` | `grep '^C_INCLUDES'` | 12、13 |
| asan/链接选项进没进 | `link.txt` | `cat .../link.txt` | 3、8、15 |
| 有哪些用例、分别是哪一行注册的 | `CTestTestfile.cmake` | `grep -c "add_test("`、`ctest -N` | 18 |
| 产物有没有真的链上共享 asan | 产物本身 | `ldd <产物> \| grep asan`、`stat -c %s` | 16 |
| 程序里有没有行号 | 报告文本 | 跑一次看栈帧有没有 `文件:行号` | 17 |

---

## 本章的坑清单（按代价从大到小）

1. **`ctest` 全绿 ≠ ASan 开过。** `DS_WITH_ASAN=OFF` 时那 56 条用例只检查"程序自己退 0"。
   定案只看一处：`grep '^C_FLAGS' .../flags.make` 里有没有 `-fsanitize=address`（第 9、15 节）。
2. **忘 `enable_testing()` 是哑事故**：名单文件根本不生成、`No tests were found!!!`、**退出码 0**（第 19 节）。
3. **`file(GLOB)` 新增文件静默不参与**：构建说 `Built target`、`link.txt` 里没有它的 `.o`、
   跑起来也没有它的输出 —— 三重证据都得会看（第 20 节）。
4. **`CMAKE_BUILD_TYPE` 是空的这件事不报错**：56 份 `flags.make` 里既没 `-O` 也没 `-g`，
   直到你需要调试信息或比较性能那天才发现（第 21 节）。
5. **注释里的"本机事实"会过时**：`-static-libasan` 那句在本机不成立（第 16 节）；
   `-std=c11` 那句也不成立（第 7 节）。**照抄注释 = 把别人的机器当成你的机器。**
6. **`-D` 变量名拼错不会失败**：CMake 会提示"没人用这个变量"，但**退 0**、构建目录照生成（第 9 节）。
7. **`target_include_directories` 删掉照样能编**（同目录引号 include 靠编译器规则），
   所以"能编"不能证明"这行没用"，也不能证明"这行有用"（第 13 节）。
8. **目录级 `include_directories` 污染的是"之后所有 target"**：不相干的节也会吃到邻居的 `-I`，
   而构建只会更顺，不会有人抱怨（第 12 节）。
9. **`target_*` 写在 `add_executable` 之前是报错，目录级 `add_compile_options` 写在之后是静默无效** ——
   两类顺序错误代价不同，前者会响（第 14 节）。
10. **`BUILDSYSTEM_TARGETS` 把库也数进去**：那句"可执行 target 数量"有隐含前提（第 22 节）。
11. **`grep -c add_test` 会数多一倍**：backtrace 里也有这个字符串。数用例用 `ctest -N`（第 18 节）。
12. **警告不改变退出码**：`gcc` 报 warning 退 0，`grep -c warning` 反而退 1。
    "屏幕上有没有 warning"这件事不能用管道退出码判断（第 14 节）。
13. **报泄漏的 ASan 退出会把 stdout 吞掉 0 字节**：别把"看不到程序输出"当成"程序没执行到"（第 17 节）。
14. **`cmake_minimum_required` 写低了在 CMake 4.x 是硬错误**，写多高在屏幕上看不出来 —— 它改策略（第 4 节）。
15. **`project()` 不写 `LANGUAGES` 会连 CXX 一起找**：这台机器没 g++，configure 当场炸（第 5 节）。
16. **`-std` 不在文件里而在 `flags.make` 里**：`set(CMAKE_C_STANDARD 11)` + `EXTENSIONS ON`
    拼出来的字符串你在源码里搜不到（第 6 节）。
17. **不带 `-std` 时本机 gcc 15 是 C23**：删那三行不会报错，只会换语言（第 6 节）。
18. **target 名不能含 `/`**：`string(REPLACE)` 那行是必需品不是美化；但数字开头合法（第 10 节）。
19. **`--target` 打错名字 CMake 不猜**：报错来自 make，`No rule to make target`（第 23 节）。
20. **`CMakeCache.txt` / `CTestTestfile.cmake` 存绝对路径**：贴进文档就带出登录名。
    打码要发生在**命令里**，事后编辑捕获不算（第 1、18 节）。
21. **`option` 的回执文案是写死的字符串**：`ASan：ON （想开就 -DDS_WITH_ASAN=ON…）` 这种自相矛盾
    会出现在任何"变量 + 固定提示"拼起来的输出里（第 22 节）。
22. **两条路线的"口径一致"只覆盖警告集**：`-std` 和 `-g` 各有各的来源，别把注释整句接受（第 2 节）。

---

## 自检题（能答上就算这章读完了）

**1. 给你一份 300 行的陌生 `CMakeLists.txt`，你第一件事做什么？**
先读它留下的账：`ls` 构建目录的产物、`grep '^C_FLAGS' .../flags.make`、`cat .../link.txt`、
`grep -E '^(CMAKE_BUILD_TYPE|...)' CMakeCache.txt`、`ctest -N`。
第 1 节那三个 56 就是这么来的 —— 数完再回去读行，每行都能指到一件东西上；反过来（先读行）就只能猜。

**2. 屏幕上出现 `-std=gnu11`，文件里搜不到。它由哪几行决定？删哪一行会变成 `-std=c11`？哪一行在本机看不出差？**
`set(CMAKE_C_STANDARD 11)` 给"11"，`CMAKE_C_EXTENSIONS ON` 给"gnu"前缀 → `gnu11`；
把 `EXTENSIONS` 拨成 `OFF` 就变 `-std=c11`；`CMAKE_C_STANDARD_REQUIRED` 在 gcc 15 上**看不出差别**
（它只在编译器不支持 C11 时才有戏，第 6 节）。

**3. 同事说"我把 ASan 打开了"。你一条命令怎么验？**
`grep '^C_FLAGS' <构建目录>/CMakeFiles/<target>.dir/flags.make | grep fsanitize`（第 9 节）。
更完整要两条：`flags.make` 有编译期那半、`link.txt` 有链接期那半（第 15 节）。只看缓存里的 `DS_WITH_ASAN:BOOL=ON`
只能证明"开关拨了"，不能证明"两半都在"。

**4. `ds_add_leaf(01-array/static-array  01-array/static-array/static-array.c)` 这一行，
函数里的 `ARGN` 拿到的是什么？少写第二个参数会在哪一步报错、报什么？**
`ARGN` = 除第一个之外的全部参数 = 那一份（或多份）`.c` 的路径，原样进 `add_executable`。
少写第二个参数 → 配置期报 `No SOURCES given to target: 01-array-static-array`，
并且带 `Call Stack (most recent call first)` 指回调用点（第 11 节）。

**5. 「叶子目录自包含」这条约定靠什么生效？把那行删掉会立刻炸吗？**
靠第 75 行 `target_include_directories(... PRIVATE 本节目录)`：跨节 include 会死在编译期（第 12 节实测
`fatal error: slist-num.h: No such file or directory`）。
但**整行删掉不会炸** —— 同目录的引号 include 由编译器规则兜住了（第 13 节）。
所以它的价值是"名单恰好一条"，是可控性和工具可见性，不是"能不能编"。

**6. 第 82 行说本机必须 `-static-libasan`。你在这台机器上怎么证伪？代价数据是什么？**
`ls /usr/lib/x86_64-linux-gnu/libasan.so*`（有 `libasan.so.8`）+ 去掉那半句重新构建 +
`ldd <产物> | grep asan`（不加时出现一行、加了是 0 次）+ `stat -c %s` 两个产物对比：
1987784 字节 vs 27032 字节（第 16 节）。它什么时候成立：目标机器没有对应版本 `libasan.so`（跨机器发产物）。

**7. 为什么"平时没调试符号、一开 ASan 就有行号"不是巧合？**
`CMAKE_BUILD_TYPE` 是空的（缓存里 `STRING=` 后面什么都没有），所以 56 份 `flags.make` 里既无 `-O` 也无 `-g`；
而第 81 行在 ASan 分支里额外给了一个 `-g`（第 21 节三屏对质：Debug 给 `-g`、Release 给 `-O3 -DNDEBUG`、
两个都不设就什么都不给）。

**8. `ctest` 报 `No tests were found!!!` 和 `gmake: *** No rule to make target` —— 哪个更危险？**
前者。它**退 0**，CI 会放行；后者退 2，一定有人来问。
`No tests were found!!!` 在本章的成因是"删掉了 `enable_testing()`"（第 19 节），
和第 06 章那三四种"绿灯不可信"的成因是同一族。

**9. ASan 报告里 `in main /tmp/.../leaf.c:17` 这一行每个部分都稳定吗？**
不。路径前缀是 `mktemp` 沙箱名，`==52910==`（PID）、`0x5bae...`（地址）、`BuildId` 每次都变。
稳定的是语义：`Direct leak of N byte(s) in M object(s)`、`SUMMARY:` 那行，以及
"**有 `文件:行号` 还是只有 `可执行文件+偏移`**"这个形状差别（第 17 节）。

**10. 那 56 行为什么不肯换成 `file(GLOB)`？换成 GLOB 之后你会在屏幕上看到什么？**
因为 GLOB 只在 configure 时扫目录，新增文件不会让任何东西失效。第 20 节三重证据：
`[100%] Built target app`（没干活）、`link.txt` 里 0 个 `ctor.c.o`、跑起来没有 ctor 的输出。
手工清单的对应代价是：漏写就报 `Cannot find source file`（当场响）。

---

## 本章没演 / 没验证（请当它们不存在，别照抄）

手册最容易骗人的地方是"顺手多写一句看起来合理的结论"。下面这些我**没有**在本机跑出来：

* **仓库根那份真文件没有被本章 configure / build / ctest / make 过**（硬红线：`build-cmake/` 是唯一物证）。
  所以第 21 节末尾那三行 CI 命令、以及"56 份静态 asan 产物的总体积差额"都属于**推演**，不是实测。
* `install()` / `export()` / 链接库顺序：这份文件一个都没有；第 22 节临时加的那个静态库只用来打脸
  那句"可执行 target 数量"，**没有**演它怎么装、怎么被链接。`target_link_libraries`、
  `target_compile_definitions`、`ALIAS`、`INTERFACE` 库是第 04 章的范围，本章没碰。
* `enable_testing()` 与 `add_subdirectory` 的**跨目录**次序（"必须在前"那条硬约束）：
  本章只演了单目录内的顺序无关，多目录场景没搭。
* `RUNTIME_OUTPUT_DIRECTORY` 的 **target 属性**版本、以及生成器表达式形式
  `$<0:>` / 多配置下的 `<cfg>/` 子目录形状：本机是单配置 `Unix Makefiles`，构造不出来。
* 多配置生成器（Xcode / Visual Studio）与 ninja（本机不存在，第 05 章验证过）：本章所有结论只适用于单配置。
* `CMAKE_C_STANDARD_REQUIRED` 拨成 OFF 后"偷偷降级"的真实场景：需要一台不支持 C11 的编译器，本机造不出。
* `-Werror`（`Makefile` 的 `check` 目标里有，第 2 节只读了那两行 recipe）：**没有**在 CMake 路线上试过，
  也没跑过 `make check` / `make asan` / `make status` 任何一条（红线）。
* `compile_commands.json` / clangd 的实际行为：第 13 节只说"名单要和构建一致"，**没有**打开
  `CMAKE_EXPORT_COMPILE_COMMANDS` 去验证编辑器看到什么。
* `file(GLOB ... CONFIGURE_DEPENDS)` 的可承诺程度、`GLOB` 的两种合法用法：那是第 07 章的范围，
  **该章在本仓库尚未收口**，本章不下结论。
* preset 的其他面：`--list-presets` 三类名单、`condition`、`CMakeUserPresets.json`、`environment` / `$penv{}`
  —— 第 06 章演过，本章只用了"hidden 底座 + inherits + 三条 `--preset`"这一小块。
* `PASS_REGULAR_EXPRESSION`、`WILL_FAIL`、`TIMEOUT` 这些裁判属性：第 06 章的事，本章没重复。
* 那 56 节**源码本身**（指针、数组、链表、栈的讲法）不在本章范围；本章只借复刻里那节漏 12 字节的链表当靶子。
* `CTestTestfile.cmake` 里 `_BACKTRACE_TRIPLES` 的**官方保证**（它是否永远只记三层）：
  我只报告了本机这两份文件里它长什么样。

---

## 怎么跑这章的 demo

```bash
cd cmake-handbook/08-repo-cmake-walkthrough
bash demo.sh                     # 24 节全跑，跑完自动删沙箱
bash demo.sh --keep              # 保留沙箱并打印路径，进去自己接着拨开关
bash demo.sh > /tmp/c08.txt 2>&1 # 重定向时自动关颜色，拿纯文本
```

章内文件一览：

```text
08-repo-cmake-walkthrough/
├── CMakeLists.txt              迷你复刻：ds_add_leaf / DS_WITH_ASAN / DS_WARNINGS 同名同形
├── mini/                       四节道具（00 pointer-basics / 01 static-array /
│                               02 slist【故意漏 12 字节】/ 03 array-stack）
├── src/                        五个实验道具
│   ├── cross/crossleaf.c       第 12 节：跨目录 include
│   ├── warn/leaf.c             第 14 节：只有 -Wshadow 看得见的遮蔽
│   ├── std/strdup-leaf.c       第 7 节：POSIX 声明在 c11 / gnu11 下的分界
│   ├── glob/{mainx.c,ctor.c}   第 20 节：constructor 探针，验证文件参没参与
│   └── lib/numops.c            第 22 节：塞一个静态库进去打脸那个数字
└── demo.sh                     本章全部证据的唯一来源
```

安全边界（这几条是硬规矩，不是客套）：

* 所有 configure/build/ctest **都在 `mktemp -d` 的沙箱里**，工程是本章节目录的副本；
  章节目录里连一个 `.o` 都没有（第 24 节 `find` 报 0）。
* 仓库根的 `CMakeLists.txt` / `Makefile` / `README.md` / 那 56 个 `00-*`…`11-*` 目录 / 所有 `build*/`
  一律**只读**：只用 `ls` / `grep` / `sed -n` / `wc` 读，一个字节都不写。
  `ctest` 一次都没在 `build-cmake/` 上跑过（它会往 `Testing/` 下落盘，那也算写）。
* **不联网**、不装东西、不写任何全局配置（`~/.cmake` 之类一个都不碰）、不做任何 git 写操作、
  `--prefix` 只指沙箱。
* 输出里不出现本机登录名：读仓库根文件时用 `runc_at`（屏幕上只留目录名 + 相对路径），
  含绝对路径的行（`CMAKE_CACHEFILE_DIR` / `C_INCLUDES` / `CTestTestfile.cmake` 的 `add_test`）
  一律**在命令里**用 `sed "s|$PWD|<仓库根>|g"` 换掉再落地。
* 章工程里**没有任何一个文件叫 `main.c`**，源文件都在第 5 层以下 ——
  根 `Makefile` 的 `find . -mindepth 3 -maxdepth 3 -name main.c` 扫不到本章，
  所以本章不会变成教材的第 57 个章节（第 2、20 节那条 `find` 就是它的账）。

想把你自己的工程按这一章的方法过一遍，建议顺序：

1. 先数产物：`ls <构建目录>/bin | wc -l`（或 `ls -d *.dir | wc -l`），和清单行数对质（第 1 节）。
2. 再读四行缓存：`CMAKE_BUILD_TYPE` / `CMAKE_<LANG>_COMPILER` / `CMAKE_GENERATOR` / 你的开关（第 1、9 节）。
3. 然后抽一个 target，`grep '^(C_FLAGS|C_INCLUDES)' flags.make` + `cat link.txt`（第 3、6、12、15 节）。
4. 把文件里**每一条带事实断言的注释**拿去跑一遍（第 7、16、17 节）——这一步最便宜、收益最大。
5. 最后逐个"拨开关看后果"，但**在副本上做**，不要在唯一物证树上做（第 8、14、19、20 节）。

---

## 术语小抄

| 词 | 大白话 |
|---|---|
| 叶子目录 / leaf | 一个 `.c` + 它的 `.h` 自己待着、不碰邻居的那种目录；本工程里就是 `章/节/` |
| `ds_add_leaf()` | 把"一节怎么变成一个可执行文件 + 一条用例"的规则抽成一个函数，56 次只报目录和文件 |
| `ARGN` | 函数里"除第一个之外的全部参数"清单（本文件的注释误写成 `ARGVN`） |
| 产物目录 | `CMAKE_RUNTIME_OUTPUT_DIRECTORY` 指的那个 `bin/`，让可执行文件别散在构建根 |
| `DS_WITH_ASAN` | 这份文件唯一的开关；`option` 给的 BOOL，默认 `OFF`，落在缓存里 |
| `DS_WARNINGS` | 三把尺子的列表变量：`-Wall -Wextra -Wshadow`，靠 `target_compile_options PRIVATE` 发出去 |
| `flags.make` | 一个 target 的编译参数清单，`-std` / `-I` / `-W` / `-fsanitize` 的最终落点 |
| `link.txt` | 链接命令原文；asan 的"另一半"、`-o` 落点（有没有 `bin/`）都在这一行 |
| `CTestTestfile.cmake` | ctest 的名单文件，配置期生成；`_BACKTRACE_TRIPLES` 还记着"哪一行注册的" |
| `-static-libasan` | 把 asan 运行时打进产物；本机有共享版，所以这不是必需，只是换机器少一个依赖 |
| `-fno-omit-frame-pointer` | 别把帧指针优化掉，栈回溯才认得出每一层 |
| `BUILDSYSTEM_TARGETS` | "本目录里所有构建期 target"，可执行和库不分家 —— 拿它数可执行文件要当心 |
| `file(GLOB)` | 配置期扫目录收源文件；新加文件不会让任何依赖失效，所以会静默漏编 |
| 单配置 / `--target` | `Unix Makefiles` 下一次只有一套编译参数，`cmake --build --target X` 里的名字就是 make 的目标名 |

---

## 本章之后

| 计划章 | 讲什么 | 现在有没有 |
|---|---|---|
| 07 | 老写法 → 新写法迁移：`include_directories` / `link_directories` / 全局变量那一套怎么改，`file(GLOB)` 的合法用法与 `CONFIGURE_DEPENDS` | **未收口**（目录已在写，本章只引用它的题目） |
| 09（暂定） | 把这份 249 行改成"能装、能被别人 `find_package` 用到"的工程：`add_library` / `install` / `export` 那一整套 | 未写 |

本章给第 07 章留下三个现场，它接着讲才顺：
① 目录级 `include_directories` 的污染在屏幕上长什么样（第 12 节最后一屏）；
② `file(GLOB)` 静默漏编的三重证据（第 20 节）；
③ 那句"可执行 target 数量"为什么会虚高（第 22 节）。

本章也把第 06 章的两件事收口了：`target_link_options` 那两行为什么必须成对（第 15 节在真文件形状上验证），
以及 `-static-libasan` 背后的部署假设（第 16 节在本机证伪，并说清它什么时候成立）。
