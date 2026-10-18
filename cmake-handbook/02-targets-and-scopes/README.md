# target 与作用域：PUBLIC / PRIVATE / INTERFACE 到底改变了谁的命令行

> 第 01 章学会了「去看 `flags.make` 里的真实命令行」，这一章就把这个方法当成显微镜，
> 把现代 CMake 最抽象、也最容易写错的一件事看清楚：**三个作用域关键字**。
>
> 本章的做法是**把关键字做成开关**：同一份源码，只把 `PUBLIC` 拨成 `PRIVATE` 或 `INTERFACE`，
> 让编译**从成功变成失败**，让你亲眼看见每个关键字砍掉了什么。
> 下面每段输出都来自这台机器的一次真实运行（cmake 4.2.3 / GNU Make 4.4.1 / gcc 15.2.0）。
> 输出里的 `/tmp/cmake02-PyA0uC/...` 是 `mktemp` 每次随机生成的沙箱路径，你跑出来会是另一个后缀。

---

## 你会遇到的场景

先对号，这几个是不是你的现场：

1. 库写好了，可执行文件里 `#include "mylib.h"` 报 **No such file or directory**，
   你手动补了一句 `include_directories(...)` 就好了 —— 但你说不清为什么“链接了库还要自己补 -I”。
2. 老教程里 `target_link_libraries(app vector)` 不写第三个关键字，CMake 立刻报错要求你写
   `PUBLIC/PRIVATE/INTERFACE` —— 你随手写了 `PRIVATE`，项目变大了以后开始出诡异问题。
3. 你的库内部依赖了另一个库，消费者莫名开始 `undefined reference`，或者反过来：消费者被迫手写一堆它根本不该知道的依赖。
4. 编译选项 `-Wall` 加在库里，结果只有库自己受警告约束，消费者的代码一条警告都没有。
5. 明明写了 `target_compile_features(hello PUBLIC c_std_11)`，编译命令行里却搜不到 `-std` —— 是 CMake 没干活吗？
6. 别人写的 `INTERFACE` 库里列了 `.c` 文件，构建“成功”了，但那个库什么都没产出。

**这 6 个场景的答案是同一条规则**，而且全部可以在磁盘上验证 —— 本章就是把这个验证方法交给你。

---

## 心智模型

### 图 1：作用域只有三个方向，背这一张就够

把每个 target 想成一个人，「使用要求」（`-I` 头目录、`-D` 宏、`-std`、`-W` 选项、要链接的库）是想发给别人的东西：

```
                        消 费 者（链接了这个 target 的人）
                                     ▲
                                     │
             PUBLIC     ─────────────┼─────────────  INTERFACE
          自己 + 别人都拿到            │          只有别人拿到
                                     │
                                 自 己
                                  PRIVATE
                             只有自己拿到

  · PUBLIC    = “这是我的对外契约”：我编译时要它，凡用我的人也必须要有它
  · PRIVATE   = “这是我的实现细节”：只有我编译时看得见，不许传染
  · INTERFACE = “我自己不产出东西，纯粹是给别人用的”：
                典型就是 add_library(x INTERFACE) 这种没有源码的 target
```

一句判断口诀：**「消费者需要知道这个信息才能编译成功吗？」**

- 需要 → `PUBLIC`（最典型：公开头文件所在的目录）。
- 不需要，是我自己的事 → `PRIVATE`（典型：只在 `.c` 里用的宏、内部工具库的路径）。
- 我根本没有“自己”（纯设置/纯头文件库）→ `INTERFACE`。

### 图 2：别信文字，信物证 —— 每个 target 的命令行都存在两个文件里

```
build/CMakeFiles/<target>.dir/flags.make      ← 这个 target 每个源文件用什么命令编（-I / -D / -W / -std）
build/CMakeFiles/<target>.dir/link.txt        ← 这个 target 用什么命令链接（谁被带了进来）
```

现代 CMake 的“作用域”不是一个抽象概念，它就是**这两个文件里的字节**。
第 01 章教你看过一次 `flags.make`，本章全程用它当显微镜：
拨一下开关 → 重新 configure → 立刻 `grep flags.make` → 看某个 `-I` 是出现了还是消失了。
这个方法比任何教程都可靠，因为它读的是 CMake 自己的最终结论。

### 图 3：本章的靶场工程

```
    app ──┐
          ├──► vector ──► util          strict_flags（纯 INTERFACE，只有编译选项）
    vstats┘                    ▲                              │
                               └──── vector 也链接了它 ────────┘
```

四个 target，职责刻意做得很小：

| target | 类型 | 暴露什么 | 谁依赖它 |
|---|---|---|---|
| `util` | STATIC | `util/util.h`，外加一个编译期宏 `UTIL_TAG` | `vector` |
| `strict_flags` | INTERFACE | 只有 `-Wall -Wextra -Wshadow`，没有源码 | `vector` |
| `vector` | STATIC | `lib/vector.h`，实现里 `#include "util.h"` 并调用 `u_clamp` | `app`、`vstats` |
| `app` | 可执行 | 只 `#include "vector.h"` | — |
| `vstats` | 可执行 | 两个头都 include，还用 `#ifdef` 探测 `UTIL_TAG` | — |

`CMakeLists.txt` 里两个开关（**注意：把关键字做成变量是本章的实验装置，真实项目别这么写**）：

```cmake
set(VEC_INCLUDE_SCOPE "PUBLIC" CACHE STRING "vector 自己的头目录用什么作用域（PUBLIC/PRIVATE）")
set(VEC_DEP_SCOPE     "PUBLIC" CACHE STRING "vector 链接 util 时用什么作用域（PUBLIC/PRIVATE/INTERFACE）")
...
target_include_directories(vector ${VEC_INCLUDE_SCOPE} ${CMAKE_CURRENT_SOURCE_DIR}/lib)
target_link_libraries(vector ${VEC_DEP_SCOPE} util)
```

开关取值必须自己校验，否则你会把 `POBLIC` 一路带到一条毫不相干的报错里去：

```cmake
if(NOT VEC_DEP_SCOPE MATCHES "^(PUBLIC|PRIVATE|INTERFACE)$")
    message(FATAL_ERROR "VEC_DEP_SCOPE='${VEC_DEP_SCOPE}' 非法，只接受 PUBLIC / PRIVATE / INTERFACE")
endif()
```

---

## 场景逐个实战

### 场景 1：configure 先把「有哪些 target」打给你看（demo 第 1~2 节）

工程只有 7 个文件：

```
$ find . -type f | sort
./CMakeLists.txt
./app/main.c
./lib/vector.c
./lib/vector.h
./tools/vstats.c
./util/util.c
./util/util.h
```

configure 的输出比第 01 章多了几行 `-- CMake target:`：

```
$ cmake -S . -B build
-- The C compiler identification is GNU 15.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- 实验开关：VEC_INCLUDE_SCOPE=PUBLIC  VEC_DEP_SCOPE=PUBLIC
-- CMake target: util
-- CMake target: strict_flags
-- CMake target: vector
-- CMake target: app
-- CMake target: vstats
-- Configuring done (0.2s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake02-PyA0uC/proj/build
```

那 5 行是这段代码遍历出来的：

```cmake
get_property(_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(_t IN LISTS _targets)
    message(STATUS "CMake target: ${_t}")
endforeach()
```

两个值得记的点：

- **属性名是 `BUILDSYSTEM_TARGETS`，不是 `TARGETS`。** 我第一次写的是 `get_property(_t DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} TARGETS)`，
  当场一条 `CMake Error: get_property given invalid argument "TARGETS"`，而且因为它在文件末尾，
  前面所有 target 都已经定义完了才报错 —— configure 直接判定失败，一个构建文件都没生成。
  （这就是本章的一个通用经验：**CMake 报错先看它停在第几行**，前面的语句其实已经执行过了。）
- `util`、`vector`、`strict_flags` 也在清单里，虽然它们不生成可执行文件。**在 CMake 眼里，“库”和“程序”是同一种东西：target。**
  这是现代 CMake 的地基：一切设置都挂在 target 上，而不是挂在全局变量上。

### 场景 2：全 PUBLIC 的基线 —— 先把“正确姿势”跑出来（demo 第 3 节）

```
$ cmake --build build
[ 12%] Building C object CMakeFiles/util.dir/util/util.c.o
[ 25%] Linking C static library libutil.a
[ 25%] Built target util
[ 37%] Building C object CMakeFiles/vector.dir/lib/vector.c.o
[ 50%] Linking C static library libvector.a
[ 50%] Built target vector
[ 62%] Building C object CMakeFiles/app.dir/app/main.c.o
[ 75%] Linking C executable app
[ 75%] Built target app
[ 87%] Building C object CMakeFiles/vstats.dir/tools/vstats.c.o
[100%] Linking C executable vstats
[100%] Built target vstats
```

依赖顺序是 CMake 自己排出来的：先 util，再 vector，最后两个可执行文件。
**你一个字都没写“谁先谁后”** —— 这就是 `target_link_libraries` 的第二个作用：声明依赖边，构建图自动拓扑排序。
（对照第 07 章数据结构里的拓扑排序：这里就是一次真实的拓扑序。）

两个程序跑起来：

```
$ ./build/app
共 10 个元素：1 4 9 16 25 36 49 64 81 100 
越界取值 vec_get(&v, 99) = -1

$ ./build/vstats
元素个数 = 6，总和 = 36，夹到 [0,10] 之后的最大值 = 10
库标签（来自 util 的编译期定义）：util-v1
```

**把最后这行记住**：`库标签：util-v1`。场景 6 会把它悄悄弄丢，而且编译一个错都不报。

### 场景 3：物证 —— 传染路径在 flags.make 里一览无余（demo 第 4 节）

```
--- build/util ---
C_DEFINES = -DUTIL_TAG=\"util-v1\"
C_INCLUDES = -I/tmp/cmake02-PyA0uC/proj/util
C_FLAGS = 

--- build/vector ---
C_DEFINES = -DBUILDING_VECTOR=1 -DUTIL_TAG=\"util-v1\"
C_INCLUDES = -I/tmp/cmake02-PyA0uC/proj/lib -I/tmp/cmake02-PyA0uC/proj/util
C_FLAGS = -Wall -Wextra -Wshadow

--- build/app ---
C_DEFINES = -DUTIL_TAG=\"util-v1\"
C_INCLUDES = -I/tmp/cmake02-PyA0uC/proj/lib -I/tmp/cmake02-PyA0uC/proj/util
C_FLAGS = -Wall -Wextra -Wshadow

--- build/vstats ---
C_DEFINES = -DUTIL_TAG=\"util-v1\"
C_INCLUDES = -I/tmp/cmake02-PyA0uC/proj/lib -I/tmp/cmake02-PyA0uC/proj/util
C_FLAGS = -Wall -Wextra -Wshadow
```

四张物证读出来的结论，就是作用域规则本身：

| 观察 | 说明的规则 |
|---|---|
| `-I.../util` 和 `-DUTIL_TAG` 从 util 出现在 vector/app/vstats 里 | `PUBLIC` **顺着依赖链往下传染**（app 没链接 util，但 app→vector→util 传到了） |
| `-DBUILDING_VECTOR=1` **只在 vector.dir 里** | `PRIVATE` 到我自己为止，不许传染 |
| `-Wall -Wextra -Wshadow` 在 vector/app/vstats 里都有，但 util 里**没有** | `strict_flags` 是 INTERFACE 库，选项写在 `INTERFACE` 里 → 只发给消费者；而 util 谁都没链接，所以 C_FLAGS 是空的 |
| `app` 里出现了 `-I.../lib`，但 `add_executable(app ...)` 那几行从没写过 lib 目录 | 这就是「链接了库，为什么 -I 自动有了」的答案：`target_include_directories(vector PUBLIC lib)` |

再看链接的物证：

```
$ cat build/CMakeFiles/vstats.dir/link.txt
/usr/bin/cc -Wl,--dependency-file=CMakeFiles/vstats.dir/link.d CMakeFiles/vstats.dir/tools/vstats.c.o -o vstats  libvector.a libutil.a
```

`libutil.a` 也在这里 —— vstats 只 `target_link_libraries(vstats PRIVATE vector)`，util 是被“传染”进来的。
**这就是现代 CMake 相比老式 `link_directories + target_link_libraries(app vector util m)` 的核心收益：
依赖闭包由库自己声明，消费者不需要知道库内部用了什么。**

### 场景 4：一个反直觉的发现 —— `-std` 去哪了（demo 第 4 节末）

CMakeLists 明明写了：

```cmake
target_compile_features(vector PUBLIC c_std_11)
```

但把四个 target 的 `C_FLAGS` 全列出来，里面**一个 `-std` 都没有**：

```
$ grep -h '^C_FLAGS' build/CMakeFiles/*.dir/flags.make | sort -u
C_FLAGS = 
C_FLAGS = -Wall -Wextra -Wshadow

$ gcc -dM -E -x c /dev/null | grep __STDC_VERSION__
#define __STDC_VERSION__ 202311L
```

第二行是解释：本机 gcc 15 的**默认方言已经是 C23**（`202311L`）。
而 `target_compile_features(... c_std_11)` 的语义是「**我至少需要 C11**」——
一个最低要求。既然默认就满足，CMake 就不发任何 flag。

做一组对照实验（在另一份拷贝里注入第 01 章那种硬指定）：

```
$ sed -i '/^project(scopes_demo/a set(CMAKE_C_STANDARD 11)' CMakeLists.txt && grep -n -A1 '^project' CMakeLists.txt
13:project(scopes_demo VERSION 0.1 LANGUAGES C)   # 本机没有 g++，只声明 C
14-set(CMAKE_C_STANDARD 11)

$ grep -h '^C_FLAGS' build/CMakeFiles/*.dir/flags.make | sort -u
C_FLAGS = -std=gnu11
C_FLAGS = -std=gnu11 -Wall -Wextra -Wshadow
```

结论要背下来，因为很多人以为“没看到 -std = CMake 没干活”：

| 写法 | 语义 | 会不会产生 `-std` |
|---|---|---|
| `set(CMAKE_C_STANDARD 11)` | **硬指定**方言 | 一定会（`-std=gnu11`） |
| `target_compile_features(t PUBLIC c_std_11)` | **最低要求** | 只在默认不满足时才加；够用就不加 |

所以库作者推荐后者：你只声明“我需要 C11 的语法”，把“用什么 flag 满足它”交给 CMake 和编译器。

### 场景 5：实验①，把头目录降级成 PRIVATE —— 消费者当场崩（demo 第 6 节）

```
$ cmake -S . -B build-p -DVEC_INCLUDE_SCOPE=PRIVATE
-- The C compiler identification is GNU 15.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- 实验开关：VEC_INCLUDE_SCOPE=PRIVATE  VEC_DEP_SCOPE=PUBLIC
-- CMake target: util
-- CMake target: strict_flags
-- CMake target: vector
-- CMake target: app
-- CMake target: vstats
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake02-PyA0uC/proj/build-p
[退出码 0]

$ cmake --build build-p
[ 12%] Building C object CMakeFiles/util.dir/util/util.c.o
[ 25%] Linking C static library libutil.a
[ 25%] Built target util
[ 37%] Building C object CMakeFiles/vector.dir/lib/vector.c.o
[ 50%] Linking C static library libvector.a
[ 50%] Built target vector
[ 62%] Building C object CMakeFiles/app.dir/app/main.c.o
/tmp/cmake02-PyA0uC/proj/app/main.c:3:10: fatal error: vector.h: No such file or directory
    3 | #include "vector.h"
      |          ^~~~~~~~~~
compilation terminated.
gmake[2]: *** [CMakeFiles/app.dir/build.make:79: CMakeFiles/app.dir/app/main.c.o] Error 1
gmake[1]: *** [CMakeFiles/Makefile2:161: CMakeFiles/app.dir/all] Error 2
gmake: *** [Makefile:91: all] Error 2
[退出码 2]
```

这里有两个层次，务必分开看：

1. **`vector` 自己编成功了。** 因为 `lib/vector.c` 里写的是 `#include "vector.h"`，
   引号包含会先在**当前源文件所在目录**找 —— 同目录，不需要 `-I` 也能找到。
   所以头目录降级成 PRIVATE，库自己毫无感觉。
2. **崩的是消费者。** `app/main.c` 在另一个目录，它只能靠传染过来的 `-I` 才看见 `vector.h`。

物证确认了这两点：

```
--- build-p/vector ---
C_DEFINES = -DBUILDING_VECTOR=1 -DUTIL_TAG=\"util-v1\"
C_INCLUDES = -I/tmp/cmake02-PyA0uC/proj/lib -I/tmp/cmake02-PyA0uC/proj/util
C_FLAGS = -Wall -Wextra -Wshadow

--- build-p/app ---
C_DEFINES = -DUTIL_TAG=\"util-v1\"
C_INCLUDES = -I/tmp/cmake02-PyA0uC/proj/util
C_FLAGS = -Wall -Wextra -Wshadow
```

`-I.../lib` 在 vector.dir 里还在，在 app.dir 里消失了 —— 一个字都不差地对应「PRIVATE = 只给我自己」。

> **这一节就是「库的公开头目录必须写 PUBLIC」的全部理由。**
> 而且它给了一个隐蔽警告：如果你的 `.c` 和 `.h` 放在同一个目录，写错 PRIVATE/PUBLIC **可能一开始不报错**
> （同目录引号包含救了你），等哪天有人把库装到系统目录、或者换了个引用方式，才在消费者那边炸掉。
> 别靠“恰好能编过”来验证作用域，要靠 `flags.make` 验。

### 场景 6：实验②，把依赖改成 INTERFACE —— 这次崩的是库自己（demo 第 7 节）

```
$ cmake -S . -B build-i -DVEC_DEP_SCOPE=INTERFACE | grep -E "实验开关|Configuring done|Build files"
-- 实验开关：VEC_INCLUDE_SCOPE=PUBLIC  VEC_DEP_SCOPE=INTERFACE
-- Configuring done (0.2s)
-- Build files have been written to: /tmp/cmake02-PyA0uC/proj/build-i

$ cmake --build build-i
[ 12%] Building C object CMakeFiles/util.dir/util/util.c.o
[ 25%] Linking C static library libutil.a
[ 25%] Built target util
[ 37%] Building C object CMakeFiles/vector.dir/lib/vector.c.o
/tmp/cmake02-PyA0uC/proj/lib/vector.c:5:10: fatal error: util.h: No such file or directory
    5 | #include "util.h"      /* ← 整个本章的胜负手：这一行能不能成立，取决于 CMake 有没有给到 -I */
      |          ^~~~~~~~
```

方向和直觉正好相反：**`INTERFACE` 只给消费者，不给正在编译的自己。**
所以 `vector.c` 里那句 `#include "util.h"` 拿不到 `-I`，最先崩的是 `vector` 本身，而不是 app/vstats。

一条实用推论：**`INTERFACE` 关键字只应该出现在 `add_library(x INTERFACE)` 这种「自己不产出任何东西」的 target 上。**
给一个 STATIC 库的依赖写 `INTERFACE`，等于宣称“我不需要它，但用我的人需要”——
对 C 库来说几乎总是假的（你的 `.c` 就是要 include 它）。
真正合法的用法是纯头文件库：`add_library(foo INTERFACE)` + `target_include_directories(foo INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)`，
此时“自己”根本不存在，没有源码要编。

### 场景 7：实验③，PRIVATE 依赖 —— 崩在编译期，但链接行照带不误（demo 第 8 节）

```
$ cmake -S . -B build-dp -DVEC_DEP_SCOPE=PRIVATE | grep -E "实验开关|Configuring done|Build files"
-- 实验开关：VEC_INCLUDE_SCOPE=PUBLIC  VEC_DEP_SCOPE=PRIVATE

$ cmake --build build-dp
[ 37%] Building C object CMakeFiles/vector.dir/lib/vector.c.o
[ 50%] Linking C static library libvector.a
[ 50%] Built target vector
[ 62%] Building C object CMakeFiles/app.dir/app/main.c.o
[ 75%] Linking C executable app
[ 75%] Built target app
[ 87%] Building C object CMakeFiles/vstats.dir/tools/vstats.c.o
/tmp/cmake02-PyA0uC/proj/tools/vstats.c:9:10: fatal error: util.h: No such file or directory
    9 | #include "util.h"
      |          ^~~~~~~~
[退出码 2]
```

注意这次的分布和场景 5/6 都不同，三点都值得记：

1. **`vector` 自己编过了** —— `PRIVATE` 仍然给“自己”使用要求，所以 `vector.c` 看得见 `util.h`。
2. **`app` 也过了** —— 它只 include `vector.h`，压根不需要 util。
3. **只有 `vstats` 崩** —— 它多 include 了一个 `util.h`，而 util 的 `-I` 被 `PRIVATE` 拦在了 vector 那一层。

物证 + 链接行：

```
--- build-dp/vector ---
C_DEFINES = -DBUILDING_VECTOR=1 -DUTIL_TAG=\"util-v1\"
C_INCLUDES = -I/tmp/cmake02-PyA0uC/proj/lib -I/tmp/cmake02-PyA0uC/proj/util
C_FLAGS = -Wall -Wextra -Wshadow

--- build-dp/vstats ---
C_DEFINES = 
C_INCLUDES = -I/tmp/cmake02-PyA0uC/proj/lib
C_FLAGS = -Wall -Wextra -Wshadow

$ cat build-dp/CMakeFiles/vstats.dir/link.txt
/usr/bin/cc -Wl,--dependency-file=CMakeFiles/vstats.dir/link.d CMakeFiles/vstats.dir/tools/vstats.c.o -o vstats  libvector.a libutil.a
```

最后一行是本章最容易被误解的一点：

> **`PRIVATE` 砍的是「使用要求」（`-I`、`-D`、`-std`、`-W`），不是「链接」本身。**
> `vstats` 的链接命令里 `libutil.a` 依然在 —— 静态库的符号必须闭合，这件事 CMake 会自动兜住。
> 所以“内部依赖写成 PRIVATE”不会让链接失败，只会让**消费者看不见那个库的头文件和宏**。
> 反过来说：`PRIVATE` 是你**表达“util 是实现细节，不是我的 API 的一部分”**的正确方式 ——
> 只要 vstats 不该 include `util.h`，那 `PRIVATE` 就是对的，报错的是 vstats 的越界依赖。

这一层理解到位之后，选择规则就变得很自然：

| 情况 | 该用 |
|---|---|
| 我的公开头文件里 include 了它（消费者必须也看得见） | `PUBLIC` |
| 只有我的 `.c` 用它，消费者不该知道 | `PRIVATE` |
| 我自己不用，但消费者必须链接/看见（常见于 INTERFACE 库转发） | `INTERFACE` |

### 场景 8：最阴的一种失败 —— 作用域写错，编译全绿，程序行为变了（demo 第 9 节）

这次只降级**一个编译定义**，头目录仍保持 PUBLIC，于是 `#include` 不再报错：

```
$ sed -i 's/^target_compile_definitions(util PUBLIC UTIL_TAG/target_compile_definitions(util PRIVATE UTIL_TAG/' CMakeLists.txt && grep -n 'UTIL_TAG' CMakeLists.txt
43:# 同样是 PUBLIC：-DUTIL_TAG=... 会传染给消费者。
45:target_compile_definitions(util PRIVATE UTIL_TAG="util-v1")
81:add_executable(vstats tools/vstats.c)          # 同时碰 vector.h 和 util.h + UTIL_TAG

$ cmake --build build | tail -3
[ 87%] Building C object CMakeFiles/vstats.dir/tools/vstats.c.o
[100%] Linking C executable vstats
[100%] Built target vstats

$ ./build/vstats
元素个数 = 6，总和 = 36，夹到 [0,10] 之后的最大值 = 10
库标签（来自 util 的编译期定义）：<没拿到 UTIL_TAG>
```

对照场景 2 的基线，那一行本来打印 `util-v1`，现在变成 `<没拿到 UTIL_TAG>`：

- 退出码 0，**没有任何 warning**，构建产物齐全；
- 只有运行结果变了。

这就是「`PUBLIC`/`PRIVATE` 写错」在生产里真正的样子 —— 它不总是编译错误，
更多时候是**行为差异**：功能开关宏、版本号宏、`NDEBUG`、导出符号宏（`MYLIB_API`）一旦被 `PRIVATE` 挡住，
程序就会带着另一套语义编出来。

两个可操作的结论：

1. **不要用 `#ifdef` 宽容地探测关键的编译定义**（`vstats.c` 里那段 `#ifdef UTIL_TAG` 只是为了让演示能跑完才这么写）。
   真实项目里宁可让它编译失败：`#ifndef REQUIRED_TAG` → `#error "忘了链接 util？"`。
2. **符号导出宏必须是 PUBLIC。** Windows/`-fvisibility=hidden` 场景下，
   `MYLIB_EXPORT` 这类宏如果没传染给消费者，链接期就是一堆 undefined symbol —— 而它的根因就在这一节。

### 场景 9：开关值打错一个字母（demo 第 10 节）

```
$ cmake -S . -B build-bad -DVEC_DEP_SCOPE=POBLIC
-- The C compiler identification is GNU 15.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
CMake Error at CMakeLists.txt:29 (message):
  VEC_DEP_SCOPE='POBLIC' 非法，只接受 PUBLIC / PRIVATE / INTERFACE


-- Configuring incomplete, errors occurred!
[退出码 1]
```

这条报错是**我们自己写的 `message(FATAL_ERROR)`** 兜住的。
如果没写校验，`target_link_libraries(vector POBLIC util)` 会怎么表现？
`POBLIC` 会被当成一个**库的名字**（因为缺了关键字时 CMake 走兼容模式），
你会得到一条和“你打错字”毫无关系的错，例如“找不到库 POBLIC”或者更晚才炸的链接错误 ——
这类 bug 的定位成本极高。所以：**任何 CACHE STRING 的取值，自己写 MATCHES 校验。**

顺带看一眼失败之后留下了什么：

```
$ ls build-bad 2>/dev/null || echo "（没有：FATAL_ERROR 让它在生成构建文件之前就停了）"
CMakeCache.txt
CMakeFiles
```

实情是：**留下了半截目录**。`CMakeCache.txt` 和 `CMakeFiles/` 在报错之前就已经写下去了，只是没有 `Makefile`。
所以 `FATAL_ERROR` 不能帮你保持目录干净；CI 里失败之后要么整个删掉构建目录，要么每次用新目录
（第 01 章的 `--fresh` 就是干这个的）。

### 场景 10：INTERFACE 库能不能带源码？—— 一个「不报错但也不干活」的坑（demo 第 11 节）

新手以为 `INTERFACE` 就是“普通库少写点东西”，于是把 `.c` 也列进去：

```
$ printf 'add_library(bad_iface INTERFACE lib/vector.c)\n' >> CMakeLists.txt && cmake -S . -B build-iface-err | tail -4
-- CMake target: vstats
-- Configuring done (0.2s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake02-PyA0uC/soft/build-iface-err

$ cmake --build build-iface-err 2>&1 | grep -E "bad_iface|Built target"
[ 25%] Built target util
[ 50%] Built target vector
[ 75%] Built target app
[100%] Built target vstats
[100%] Built target bad_iface

$ find build-iface-err -name "*bad*"
build-iface-err/CMakeFiles/bad_iface.dir

$ ls -A build-iface-err/CMakeFiles/bad_iface.dir; echo "（上面全是 CMake 自己的记账文件，没有一个是 .o）"
DependInfo.cmake
build.make
cmake_clean.cmake
compiler_depend.make
compiler_depend.ts
progress.make
（上面全是 CMake 自己的记账文件，没有一个是 .o）

$ find build-iface-err -name "vector.c.o"
build-iface-err/CMakeFiles/vector.dir/lib/vector.c.o
```

结果比“报错”危险得多：

1. configure 成功；
2. make 还煞有介事地报告 `[100%] Built target bad_iface`；
3. 但 `bad_iface.dir` 里除了记账文件**没有任何编译产物**，
   列进去的 `lib/vector.c` 也没被它编第二次（`find` 只找到 `vector.dir` 那一份 `vector.c.o`，那是 vector 自己编的）。

**它假装成功，其实什么都没干。** INTERFACE 库不产生物理文件，这是它的定义，不是 bug。
它的正当用途只有三种：纯头文件库、一组编译选项/定义的打包（本章的 `strict_flags` 就是）、一组依赖名的打包。
想让源码参与构建，就该用 `STATIC`（或 `OBJECT`），别用 `INTERFACE`。

---

## 作用域总表

`target_link_libraries(A <scope> B)` 里 A 是“正在写的库”，B 是“被依赖的库”：

| scope | A 自己编译时拿到 B 的 -I/-D？ | A 自己链接 B？ | 连到 A 的消费者拿到 B 的 -I/-D？ | 消费者链接 B？ | 什么时候用 |
|---|---|---|---|---|---|
| `PUBLIC` | ✅ | ✅ | ✅ | ✅ | B 出现在 A 的**公开头文件**里 |
| `PRIVATE` | ✅ | ✅ | ❌ | ✅（静态库符号要闭合，见场景 7） | B 只是 A 的**实现细节** |
| `INTERFACE` | ❌ | ❌ | ✅ | ✅ | A 不编译任何东西（纯头文件/纯设置），或明确“我不需要但用户需要” |

`target_include_directories` / `target_compile_definitions` / `target_compile_options` 同理，只是没有“链接”那一列：

| scope | 自己 | 消费者 |
|---|---|---|
| `PUBLIC` | ✅ | ✅ |
| `PRIVATE` | ✅ | ❌ |
| `INTERFACE` | ❌ | ✅ |

再加两条本章实测到的规则：

- **传染是多级的**：`app` 只链接了 `vector`，却拿到了 `util` 的 `-I`/`-D`（app→vector→util 一路 PUBLIC 传下来）。
- **没链接就没有传染**：`util` 的 `C_FLAGS` 是空的，因为它谁都没链接，`strict_flags` 的警告到不了它。

## 命令速查表

| 我想…… | 命令 |
|---|---|
| 看这个 target 真正的编译设置 | `grep -E "C_INCLUDES\|C_DEFINES\|C_FLAGS" build/CMakeFiles/<t>.dir/flags.make` |
| 看它链接时被带了哪些库 | `cat build/CMakeFiles/<t>.dir/link.txt` |
| 看 CMake 侧有哪些 target | configure 时 `message(STATUS ...)` + `get_property(_t DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)` |
| 看底层构建系统有哪些 target | `cmake --build build --target help`（名字和上面不是一回事） |
| 换个作用域做实验，且不污染旧缓存 | **换一个新的 -B 目录**：`cmake -S . -B build-p -DVEC_DEP_SCOPE=PRIVATE` |
| 确认编译器默认语言级别 | `gcc -dM -E -x c /dev/null \| grep __STDC_VERSION__`（本机 `202311L` = C23） |
| 校验开关取值合法 | `if(NOT VAR MATCHES "^(A\|B)$") message(FATAL_ERROR ...) endif()` |

## 这章的坑

1. **库的公开头目录写成 `PRIVATE`**：消费者直接 “No such file or directory”。而且如果 `.c` 和 `.h` 同目录，
   库自己**不会**报错（引号包含救它），只有消费者炸 —— 别靠“本地能编过”验证作用域。
2. **`INTERFACE` 用在了有源码的 target 上**：立刻崩在库自己头上（场景 6）。`INTERFACE` 方向是“只给别人”。
3. **`PRIVATE` 不影响链接，只影响使用要求**：静态库场景下你不会得到链接错误，只会得到“头找不到/宏没定义”。
   反过来别把 PRIVATE 当“不链接”用。
4. **编译定义被 `PRIVATE` 挡住是静默失败**：全绿、无警告、行为变了（场景 8）。关键宏宁可 `#error` 也不要 `#ifdef` 宽容。
5. **`target_compile_features` 是最低要求，不是硬指定**：默认方言已满足时**不会**出现 `-std`（场景 4）。
   想强制某方言用 `set(CMAKE_C_STANDARD)`/`target_compile_options(-std=...)`，但一般不该这么做。
6. **CACHE STRING 不校验取值**：`-DVEC_DEP_SCOPE=POBLIC` 会一路带下去，报一条毫不相干的错。自己写 `MATCHES` + `FATAL_ERROR`。
7. **`FATAL_ERROR` 之后构建目录不干净**：`CMakeCache.txt`/`CMakeFiles/` 已经写了。失败后要删目录或 `--fresh`。
8. **`add_library(x INTERFACE 源码)` 不报错但什么都不做**：make 还会报 `Built target x`（场景 10）。
9. **`get_property(... TARGETS)` 是错的属性名**：正确写法是 `get_property(_t DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)`。
   我自己在本章第一版里就踩了，报错停在文件末尾，前面语句都已执行完。
10. **两个“target”仍然不是一回事**（第 01 章说过，本章再验一次）：
    `--target help` 列出的是 make 的 target（`all`/`clean`/`util/util.o`），CMake 侧只有那 5 个名字。

## 自检问题

1. `app` 从没写过 `-I.../lib`，为什么它 `#include "vector.h"` 能成功？（场景 3）
2. 把头目录降级成 `PRIVATE` 之后，为什么 `vector` 自己还能编过、只有 `app` 崩？（场景 5）
3. `PUBLIC` / `PRIVATE` / `INTERFACE` 三种依赖，分别“给谁”“不给谁”？（场景 5/6/7 + 总表）
4. 为什么 `PRIVATE` 依赖下 `link.txt` 里仍有 `libutil.a`？这说明 `PRIVATE` 到底管什么、不管什么？（场景 7）
5. 本章里哪一种失败是**没有任何报错**的？怎么防？（场景 8）
6. `C_FLAGS` 里搜不到 `-std`，是 CMake 的 bug 吗？怎么证明？（场景 4）
7. 我要做一个只暴露头文件的库，`add_library` 该写哪个关键字？能不能给它列 `.c`？（场景 10）
8. 一个 `CACHE STRING` 开关应该配什么代码才不会被打错字坑到？（场景 9）

## 怎么跑这章的 demo

```bash
cd cmake-handbook/02-targets-and-scopes
bash demo.sh              # 全程 mktemp 沙箱，跑完自动删
bash demo.sh --keep       # 保留沙箱，进去自己拨开关接着试
bash demo.sh > out.txt    # 重定向自动关颜色，得到和本章一致的纯文本
```

demo 里**故意制造了三次编译失败和一次静默行为变化**，所以它没有 `set -e`，每条命令后面都打 `[退出码 N]`。
看到 `fatal error: ... No such file or directory` 是设计好的，不是脚本坏了。

**建议的自学动作**（比读十遍有用）：跑完 `--keep`，自己把
`target_compile_definitions(util PUBLIC UTIL_TAG=...)` 改回 `PRIVATE`，
或者把 `target_link_libraries(app PRIVATE vector)` 改成 `PUBLIC`，
然后 `grep flags.make` 猜一遍哪个 `-I`/`-D` 会消失，再验证你猜对没有。

## 下一步

- **第 03 章《库、install 与 export》**：本章的 `PUBLIC` 头目录用的是构建树里的绝对路径
  （`-I/tmp/cmake02-.../lib`）—— 一旦 `cmake --install` 到 `/usr/local`，这个路径就失效了。
  下一章讲 `$<BUILD_INTERFACE:...>` / `$<INSTALL_INTERFACE:...>` 这对搭档，以及
  `install(TARGETS)` + `export` 怎么让别的工程能用 `find_package` 找到你的库。
- 想先横向看构建方式和诊断：第 05 章（generator、`CMAKE_BUILD_TYPE`、`compile_commands.json`）、
  第 06 章（CTest + Sanitizer：把本章这些实验固化成会自动失败的测试）。
- 本仓库那 56 节数据结构代码的 `CMakeLists.txt` 里，`ds_add_leaf()` 用的正是本章的规则
  （`target_include_directories(... PRIVATE ...)`，因为每个叶子目录都自包含、**没有对外头文件**）——
  第 08 章逐行拆解时你会认出这个选择。
