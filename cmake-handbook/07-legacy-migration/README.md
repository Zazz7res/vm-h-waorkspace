# 第 07 章 · 老写法 → 新写法迁移：改完之后，怎么证明自己没改坏

> 前面六章教的都是**新写法**该怎么写。这一章处理另一件更麻烦的事：
> 你手上已经有一份 `CMakeLists.txt`，它是从教程 / 老同事 / 公司仓库里拷来的，
> 里面全是 `include_directories`、`link_directories`、`add_definitions`、`file(GLOB)`、
> 无关键字的 `target_link_libraries`、`set_target_properties(... COMPILE_FLAGS)` ——
> **而且它能跑。** 于是"要不要改"没人能拍板，"改了会不会坏"更没人敢答。
>
> 本章给的不是标准答案式的文件，而是**一把尺子和一套走查**：
> 改写安不安全，不看能不能跑，看**改写前后传给编译器的字符是否一致**。
>
> 下面每一段输出都是这台机器上真实跑出来的（cmake 4.2.3 + GNU Make 4.4.1 + gcc 15.2.0，只有 C），
> 由 `bash demo.sh` 一次生成，共 30 节，跑法见文末「怎么跑这章的 demo」。
> 输出里的 `/tmp/cmake07-AqiaHO/...` 是 `mktemp -d` 随机生成的沙箱路径，你跑出来会是 `cmake07-<另外 6 位>`；
> 会变的不止路径：**`Configuring done (0.1s)` 的秒数、`Total Test time (real)` 和 `time` 那三行耗时每次都飘**，
> 它们的形状稳定、值不稳定（第 21 节那两次计时尤其明显：同两条命令，我这边两遍跑出 1 毫秒和 10 毫秒两种差值）。

本章的取舍：**我不但演"老写法哪里坏"，还专门演两件事 ——**
一是**老写法到底有没有坏**（它常常没坏，这才是它能活十年的原因），
二是**"用新写法改写"这件事本身的边界**：本章那把尺子（命令行 diff）有一处抓不到，
第 23 节当场把它抓不出来给你看。

---

## 你会遇到的场景

先对上号，这些是不是你的现场：

1. 网上抄来的 `include_directories("${CMAKE_SOURCE_DIR}/include")` 一开始就在文件最上面。后来你只想给**某一个** target 加个头目录，发现做不到 —— 它全给了。
2. 有人为了"整理风格"把 `include_directories` 从文件中部挪到最前面（或反过来挪到 target 之后），构建照过，但你说不清这行到底动了什么。
3. 工程里有**两份同名头文件**（自己一份、第三方拷进来一份），程序跑得不对，grep 半天找不到用的哪份。
4. 有**两份同名 `libfoo.a`**，`link_directories` 给了两个目录，换台机器就链到另一份 —— 屏幕上没有任何一行写着"我选了哪一份"。
5. 你 `add_definitions(-DDEBUG_TRACE)` 只是想打开自己主程序的日志，结果 `add_subdirectory` 进来的第三方目录也开始打日志。
6. 新增一个 `.c`，`cmake --build` 成功、`ctest` 全绿、退出码 0 —— 那个文件从头到尾没被编译过。
7. 有人说 `file(GLOB)` 加个 `CONFIGURE_DEPENDS` 就好了；你想知道**在你这台机器、这个生成器上**它到底会不会自动重跑，代价是什么。
8. 老教程的 `target_link_libraries(app calc util)` 不写第三个关键字。你"顺手补上 `PRIVATE`"之后，某个下游突然编不过了；或者反过来，你不补，导出时 `CMake Error` 等你。
9. `set_target_properties(x PROPERTIES COMPILE_FLAGS ...)` 写第二次把第一次冲掉了；或者里面写了 `$<CONFIG>`，屏幕上出现一条你从未见过的编译命令。
10. 你终于决定动手改那份 200 行的老 `CMakeLists.txt`，怕改坏了没法交代 —— 需要一个**一步一验**的流程和一个"我改完了"的物证清单。

这 10 个现场，答案分别落在本章的第 4/6 节、第 3 节 + 第 10 节、第 7 节、第 8~9 节 + 第 11 节、
第 5/13/15 节、第 17~19 节、第 20~21 节、第 23 节、第 25~26 节、第 27~30 节。

---

## 一句话版本

* **老写法的失败模式永远是"编译照过、行为变了"，不是编译不过。** 所以它的检查也不能靠"能跑"。
* **唯一的尺子是改写前后的命令行**：`CMakeFiles/<t>.dir/flags.make`（`-I` / `-D` / 编译标志）和 `<t>.dir/link.txt`（`-L` / 库路径 / `-l`），
  抽成一行一个再 diff。程序输出是第二把尺子（行为等价用它对，不用退出码对）。
* **三种目录级指令不是一种东西**：`include_directories` / `add_definitions` 是**生成期读目录属性**——
  同一个目录里不分先后全都吃到，只有子目录分先后；`link_directories` 才是**真快照**——写在 target 之后等于没写。
* **-I 和 -L 的"顺序"也是命令行的一部分**：同名头 / 同名库的赢家由先后决定，两次都构建成功（第 7、9 节实测换赢家）。
* **`file(GLOB)` 的病不是"不刷新"，是"刷新这件事没有触发器"**；`CONFIGURE_DEPENDS` 在本机确实会自动重跑 configure（第 20 节实测），
  但它会留下旧产物，而且换生成器就不保证了。
* **本章的尺子有一个盲区**：无关键字 `target_link_libraries` 和 `PRIVATE` 版的 `link.txt` **逐字符相同**（第 23 节实测），
  那一类只能看属性和"改到一半会硬报错"。

---

## 引用约定（这一章怎么读）

1. **围栏里的都是真输出**，一行没改；我给的解释放在围栏外面，用 ★ 或"注意"起头。
   围栏里出现的 `  ↑ …`、`  ★ …` 这两类缩进行是 `demo.sh` 自己打印的注解，不是我后加的。
2. `$ ` 开头的是**敲的命令**，它的 stdout+stderr 原样跟在下面。`demo.sh` 每条命令后面都会打一行 `[退出码 N]`，
   那是脚本加的，不是 CMake 打的 —— 本章凡是"grep 什么都没找到"这类结论都靠它（`退出码 1` + `0` 才是"真的没有"）。
3. 命令里出现 `| grep`、`| tail` 的是**为了屏幕干净**；脚本统一 `set -o pipefail`，所以 `[退出码]` 报的是整条管道的真实成败（谁挂了报谁）。
4. `〔在 inc-order 里〕`、`〔在 9a503fd8 里〕`、`〔在 07-legacy-migration 里〕` 是脚本的记号：命令在那个目录里跑，
   但屏幕上**不印它的绝对路径**（免得把本机登录名带进文档）。第 22 节读仓库根那份真实文件用的就是它。
5. 围栏外的正文里我用 `<沙箱>` 代指 `/tmp/cmake07-AqiaHO`，用 `b/base`、`b/legacyall` 代指构建目录；
   围栏里一律是当时的原始绝对路径。
6. **本章两个构建目录名会反复出现，先记住它们**：
   `b/base` = 实验室工程七个 `CH07_LEG_*` 开关全 OFF（＝现代写法那一半，第 2 节立起来）；
   `b/legacyall` = 同一个工程、把老开关拨成 ON（＝老写法那一半）。同一份源码、同一个 ROOT，
   所以两边的 `flags.make` 可以直接 diff —— 这是全章对质的基准。
7. 本章 target / 宏 / 开关名一律带 `CH07_` 或 `ch07` 前缀，为的是和仓库根那份 56 节的 `CMakeLists.txt` 区分；
   **本章不动它一个字节**（第 22 节只读它，第 30 节末尾 `find` 自证）。

---

## 心智模型：一张图，三种"记账时机"

老写法的坑几乎全部来自一件事：**你以为你在给一个 target 东西，其实你在改一个属性，
而那个属性什么时候被读、被谁读，是三条不同的规则。**

```text
   你写的指令                  它落到哪本账                谁在什么时候读它
 ┌───────────────────────┐   ┌────────────────────────┐   ┌──────────────────────────┐
 │ include_directories() │   │ 目录属性               │   │ **生成期**读本目录属性   │
 │ add_definitions()     │──►│ INCLUDE_DIRECTORIES    │──►│ 同目录的 target 不分先后 │
 │                       │   │ COMPILE_DEFINITIONS    │   │ 都吃到；子目录只看       │
 │                       │   │ （目录级）             │   │ add_subdirectory 那一刻  │
 └───────────────────────┘   └────────────────────────┘   └──────────────────────────┘
 ┌───────────────────────┐   ┌────────────────────────┐   ┌──────────────────────────┐
 │ link_directories()    │   │ 目录属性，但**读取即清**│   │ 只影响**那一刻之后**创建 │
 │                       │──►│ LINK_DIRECTORIES       │──►│ 的 target；写在          │
 │                       │   │ （快照语义）           │   │ add_executable 之后＝没写│
 └───────────────────────┘   └────────────────────────┘   └──────────────────────────┘
 ┌───────────────────────┐   ┌────────────────────────┐   ┌──────────────────────────┐
 │ file(GLOB)            │   │ 一个**普通变量的值**   │   │ 配置期算一次，算完就和   │
 │                       │──►│ ${LEAF} = 文件名列表   │──►│ 磁盘脱钩：磁盘上加/删    │
 │                       │   │ （不是一张"会更新的账"）│   │ 文件没人知道             │
 └───────────────────────┘   └────────────────────────┘   └──────────────────────────┘
                    三本账最后都汇到同一处，那就是本章的取证现场：
                    <构建目录>/CMakeFiles/<target>.dir/flags.make   ← -I / -D / 编译标志
                    <构建目录>/CMakeFiles/<target>.dir/link.txt     ← -L / 库路径 / -l
                    <构建目录>/CMakeFiles/<target>.dir/build.make   ← 这个 target 到底编哪些文件
```

三句话背下来：

* **命令行是唯一的事实。** `CMakeLists.txt` 里写了什么不算，`flags.make` / `link.txt` 里出现了才算。
* **"改写了"和"改坏了"只能靠对比判定**：同一份源码配置两次（改前 / 改后），比那三个文件的字符。
* **三种时机是三件事**：目录属性（`include_directories`）、快照（`link_directories`）、变量值（`file(GLOB)`）。
  流传最广的那句"老写法必须写在最前面"，只对第三种里的一条成立，对第一种反而是**写在前面污染面更大**。

### 读报错先认「谁在说话」

这一章的屏幕上有 5 个说话人。老写法的坑之所以难查，就是因为其中**两个说话人根本不出声**：

| 屏幕上是什么样 | 说话人 | 本章哪节 | 真实退出码 |
|---|---|---|---|
| `CMake Error at CMakeLists.txt:6 (target_link_libraries): The INTERFACE, PUBLIC or PRIVATE option must appear...` | 配置期的 CMake | 23、27 | 1 |
| `compilation terminated.`（源文件已经不在磁盘上了，清单里那条规则还在指向空气；第 21 节用 `tail -4` 只留下最后四行） | 编译期的 cc | 21 | 2 |
| `collect2: error: ld returned 1 exit status`（`-L` 一个都没有，找不到 `-lfoo`） | 链接期的 ld/collect2 | 10 | 2 |
| `gmake[2]: *** [CMakeFiles/.../build.make:101: xxx] Error 1` | make（它在执行规则） | 10、25、26 | 2 |
| `-- GLOB mismatch! / The following files were added:` | CMake 在构建中被重新叫醒 | 20 | 0 |
| **什么都没说，但 `[greet]` 多打了一行、`foo_tag()` 换了个数** | 程序自己 | 5、7、9、15 | **0** |

★ 最后那一行才是本章的主角：**老写法出事时屏幕上没有字。**
`grep -c` 报 `0` + 退出码 1 那种"我确认它不存在"反而是好消息（第 18 节用它取证）；
真正贵的是全绿的屏幕加上一个变了的行为。

---

## 第 0 部分 · 现场、取证工具，和"三种时机"的真相

### 第 1 节 本章的三套工程布局，和那份「能跑的老写法」

先把机器和素材交代清楚。本章的工程全部在沙箱里，`legacy/` 和 `modern/` 只是同一棵源码树的两个"机制"入口：

```
$ cmake --version | head -1 && gcc --version | head -1 && make --version | head -1
cmake version 4.2.3
gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
GNU Make 4.4.1
[退出码 0]
```

```
$ ls -1 /tmp/cmake07-AqiaHO/ch07
CMakeLists.txt
include
legacy
modern
src
tp
variants
[退出码 0]
  上面这一份就是本章全部素材：CMakeLists.txt（实验室）+ legacy/（老写法整份）+ modern/（改写后整份）
  + include/ src/ tp/（三套工程共用的源码）+ variants/（每节一份故意写坏的工程）。
  ★ 源码只有一份。legacy/ 和 modern/ 算出来的 ROOT 是同一个目录，
    所以它们的 flags.make 可以直接 diff —— 改的只有机制，没有意图。
```

九个故意写坏的靶子在 `variants/` 里，一个一份、每份只讲一件事：`order`、`inc-order`、`link-order`、
`glob`、`glob-depends`、`plain-link`、`cache-var`、`comp-flags`、`min-version`
（文末「怎么跑这章的 demo」那份文件清单里，每个靶子标着它在哪一节出场）。
三份 `CMakeLists.txt` 的体量：`wc -l` 数出 `legacy` 67 行、`modern` 68 行、实验室那份 176 行。
（67 / 68 那两份几乎一样长 —— 它们是"同一批意图、两种机制"；176 那份最啰嗦，因为它同时装着两套，
靠七个开关决定这次构建用哪一套。这也是本章能对质的前提。）

**为什么要有两份同名的 `libfoo.a`？** 这是全章唯一能让"-L 的顺序"变成屏幕上可见事实的装置：
同一份 `src/foo/foo.c`，只用两条不同的 `-D` 编两遍，`ar` 成两份**名字完全一样**的库：

```
$ gcc -c -I/tmp/cmake07-AqiaHO/ch07/include -DCH07_FOO_V=1 /tmp/cmake07-AqiaHO/ch07/src/foo/foo.c -o /tmp/cmake07-AqiaHO/libs/v1/foo.o && ar rcs /tmp/cmake07-AqiaHO/libs/v1/libfoo.a /tmp/cmake07-AqiaHO/libs/v1/foo.o && gcc -c -I/tmp/cmake07-AqiaHO/ch07/include -DCH07_FOO_V=2 /tmp/cmake07-AqiaHO/ch07/src/foo/foo.c -o /tmp/cmake07-AqiaHO/libs/v2/foo.o && ar rcs /tmp/cmake07-AqiaHO/libs/v2/libfoo.a /tmp/cmake07-AqiaHO/libs/v2/foo.o && find /tmp/cmake07-AqiaHO/libs -name libfoo.a
/tmp/cmake07-AqiaHO/libs/v2/libfoo.a
/tmp/cmake07-AqiaHO/libs/v1/libfoo.a
[退出码 0]
  两份 libfoo.a：名字一模一样，内容不同（一份 foo_tag() 返回 1、一份返回 2），
  区别只在造它们时那两条 -DCH07_FOO_V。**只有两份同名的库才能把 -L 顺序这件事照出来**
  —— 只有一份库的时候，顺序怎么乱都能跑。
```

注意这两条 `gcc`/`ar` **没有经过 CMake** —— 它们是给 CMake 准备的"外部已存在的库"，
这正是 `link_directories` 这套写法的典型使用场景（库不是你建的）。

现在跑那份老写法。`legacy/CMakeLists.txt` 就是把六个病灶一次全上的"2013 年典型文件"：

```cmake
include_directories("${ROOT}/include")
add_definitions(-DCH07_MODE_${MODE}=1 -DCH07_STAMP=${STAMP} -DCH07_DEBUG_TRACE=1)
link_directories("${FOO_DIR1}" "${FOO_DIR2}")
...
set(ALL_DEPS calc util)
file(GLOB LEAF_SRC "${ROOT}/src/leaf/*.c")
target_link_libraries(app ${ALL_DEPS})
target_link_libraries(greet util)
target_link_libraries(fooprog foo)          # ← 只给名字：链到哪一份由上面的 -L 顺序决定
```

（我只引了它的骨头；六处病灶各自对应第 4、5、8、13、17、23 节，完整文件在 `legacy/CMakeLists.txt`。）

```
$ cmake -S /tmp/cmake07-AqiaHO/ch07/legacy -B /tmp/cmake07-AqiaHO/b/legacyproj -DMODE=PLUS -DSTAMP=7 -DFOO_DIR1=/tmp/cmake07-AqiaHO/libs/v1 -DFOO_DIR2=/tmp/cmake07-AqiaHO/libs/v2 2>&1 | tail -3
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake07-AqiaHO/b/legacyproj
[退出码 0]
```

```
$ cmake --build /tmp/cmake07-AqiaHO/b/legacyproj -j4 2>&1 | tail -2
[100%] Built target notepad
[100%] Built target app
[退出码 0]
```

```
$ /tmp/cmake07-AqiaHO/b/legacyproj/app && echo --- && /tmp/cmake07-AqiaHO/b/legacyproj/greet && echo --- && /tmp/cmake07-AqiaHO/b/legacyproj/fooprog
[app] trace: 进 main 了，参数准备给 2 和 3
[util] 编译器给我的 CH07_STAMP = 7
[calc] 走的是 PLUS 分支
[app] calc_add(2,3) = 5
---
[greet] 注意：有人给全工程打开了 CH07_DEBUG_TRACE，我也被打了 trace
[util] 编译器给我的 CH07_STAMP = 7
[greet] 你好，我只是来问个好
---
[fooprog] foo_tag() = 1
[退出码 0]
  配置成功、构建成功、三个程序都跑出「正确结果」，屏幕上一个警告都没有。
  ★ 这就是本章最难的一点：**老写法不是坏的。** 它没有一条报错，所以没有任何机制逼着人改；
    而它已经在偷偷影响别人了 —— 下面几节逐个取证。
```

★ 请盯住上面那块屏幕的**最后一行**：`foo_tag() = 1`。
这个数字现在还是"两个 `-L` 目录的先后顺序"决定的（第 8~9 节会当场把它翻成 2）。
而 `[greet]` 那行 trace 已经在提醒另一件事了 —— 第 5 节会去第三方目录里看它有没有把别人也点上。

### 第 2 节 取证现场：一个 target 真正吃到什么，只看三个文件

这一节立起全章的基准构建树 `b/base`：实验室工程**七个老开关全 OFF**（就是现代写法那一半）。

```
$ cmake -S /tmp/cmake07-AqiaHO/ch07 -B /tmp/cmake07-AqiaHO/b/base -DCH07_FOO_DIR1=/tmp/cmake07-AqiaHO/libs/v1 -DCH07_FOO_DIR2=/tmp/cmake07-AqiaHO/libs/v2 -DCH07_STAMP=7 -DCH07_TRACE=ON 2>&1 | tail -5
--             GLOB=OFF FLAGS=OFF PLAIN=OFF CACHEVAR=OFF
--   leaf 清单里有 /tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_a.c;/tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_b.c 这几个文件
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake07-AqiaHO/b/base
[退出码 0]
```

```
$ cmake --build /tmp/cmake07-AqiaHO/b/base -j4 2>&1 | tail -3
[100%] Built target notepad
[100%] Built target greet
[100%] Built target app
[退出码 0]
```

（`tail -5`、`tail -3` 是屏幕管理：`-- Found ...`、每一个 `[xx%] Building` 都不重要，
重要的是那三行开关回执和"构建到 100%"。全章的 `[退出码 N]` 报的都是**整条管道的真实成败**。
最后提醒一句：`-j4` 下 `Built target` 这几行的**先后顺序本身是不稳定的**。这一章我就把同一份 demo
整跑过两遍：上一遍这两屏是 `greet / app` 和 `leaf_b / app / greet`，这一遍变成上面看到的
`notepad / app` 和 `notepad / greet / app` —— 两遍的退出码、行集、行内字符全一致，只有先后不同。
所以本章从来不拿"哪几行的顺序"当证据，只拿"哪几行**在不在**"和"行里的字符"当证据。）

`find <构建目录> -name flags.make | sort` 在 `b/base` 里数出**八份**：
`app` `calc` `fooprog` `greet` `leaf_a` `leaf_b` `util` 七份在顶层 `CMakeFiles/` 下，
第八份在 `tp/CMakeFiles/notepad.dir/` 里（退出码 0）。

★ flags.make **一个 target 一份**；它在生成阶段写出来，构建阶段被生成的 Makefile 读。
下面把 `app` 的那份整个念出来，后面每一节比的都是它里面的行。

**为什么是"一个 target 一份"而不是"一个目录一份"？** 因为 CMake 派发完目录属性和 target 属性之后，
要把结果**逐个 target 落地**给 make 用。这个落地动作就是本章可信度的来源：
它是产物，不是声明，产物不会替你辩护。

```
$ cat /tmp/cmake07-AqiaHO/b/base/CMakeFiles/app.dir/flags.make
# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 4.2

# compile C with /usr/bin/cc
C_DEFINES = -DCH07_DEBUG_TRACE=1

C_INCLUDES = -I/tmp/cmake07-AqiaHO/ch07/include

C_FLAGS = 

[退出码 0]
```

三行是本章的取证对象：`C_DEFINES`（`-D`）、`C_INCLUDES`（`-I`）、`C_FLAGS`（编译标志）。
它们**不是** `CMakeLists.txt` 里那些行的拷贝，是 CMake 把目录属性和 target 属性派发完之后的产物 —— 所以只有它可信。
（`# compile C with /usr/bin/cc` 那行说明本机走的编译器；本章全用默认的 `cc`，没有碰过 `CMAKE_C_COMPILER`。）

```
$ cat /tmp/cmake07-AqiaHO/b/base/CMakeFiles/app.dir/link.txt
/usr/bin/cc -Wl,--dependency-file=CMakeFiles/app.dir/link.d CMakeFiles/app.dir/src/app.c.o -o app  libcalc.a libutil.a
[退出码 0]
```

```
$ cat /tmp/cmake07-AqiaHO/b/base/CMakeFiles/fooprog.dir/link.txt
/usr/bin/cc -Wl,--dependency-file=CMakeFiles/fooprog.dir/link.d CMakeFiles/fooprog.dir/src/fooprog.c.o -o fooprog  /tmp/cmake07-AqiaHO/libs/v1/libfoo.a
[退出码 0]
  link.txt 里是链接命令本体：-L、库路径、-l 都在这里。注意 fooprog 那行**没有 -L**，
  它拿到的是一个 .a 的绝对路径 —— 这就是第 2 部分要对照的东西。
```

```
$ grep -c src/app.c /tmp/cmake07-AqiaHO/b/base/CMakeFiles/app.dir/build.make
13
[退出码 0]
  ↑ 第三个文件 build.make 里才是一张真正的源文件清单（第 4 部分讲 file(GLOB) 时盯的就是它和用例名单）。
```

★ 三个文件的分工，本章全程照这张表取证：

| 文件 | 里面是什么 | 本章用它回答 |
|---|---|---|
| `CMakeFiles/<t>.dir/flags.make` | `C_DEFINES` / `C_INCLUDES` / `C_FLAGS` 三行 | "这个宏 / 这个 `-I` 给了谁"（第 4~7、13~16 节） |
| `CMakeFiles/<t>.dir/link.txt` | 链接命令本体 | "`-L` 在哪、链的是哪一份库"（第 8~12 节） |
| `CMakeFiles/<t>.dir/build.make` | 编译规则 + 每个源文件的落点 | "这个 `.c` 到底参没参与构建"（第 17~21 节） |

然后是取证工具。**本章不靠 shell 函数，靠三个落成文件的小脚本** ——
原因很实际：`runc` 是 `bash -c` 起子 shell 执行的，父脚本里的函数它看不见
（这个坑我在第 06 章第 15 节当场撞过一次）。工具本身也是证据，所以直接把文件 `cat` 出来：

```
$ cat /tmp/cmake07-AqiaHO/seq.sh
#!/usr/bin/env bash
# 取证小工具（一）：把一类标志**按出现顺序**一行一个抽出来，方便 diff。
#   用法: seq.sh <flags.make 或 link.txt> <-I | -D | -L | -W>
# flags.make 把同类标志全挤在一行里，人眼比不了顺序、diff 也说不清改了哪一项；
# link.txt 里没有 C_INCLUDES 这种行名，所以按文件名分两条路走。
F=$1
K=$2
case "$(basename "$F")" in
    link.txt)
        grep -oE -- "$K[^ ]*" "$F"
```

（这一屏砍掉了那个脚本后面 20 行 —— 同一个 `case` 的其余分支：`flags.make` 那一支先按行名取出
`C_INCLUDES` / `C_DEFINES` / `C_FLAGS`，再交给同一条 `grep -oE -- "$K[^ ]*"`；认不出的文件名报错退出 2。
原文在 `demo.sh` 里写着的，本章不重复念。这个工具存在的唯一理由：`flags.make` 把同类标志全挤在一行里，
人眼比不了顺序；抽成一行一个之后 `diff` 才有意义。）

用起来是这样（同一份 `flags.make`，抽两种标志）：

```
$ /tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/base/CMakeFiles/app.dir/flags.make -I
-I/tmp/cmake07-AqiaHO/ch07/include
[退出码 0]

$ /tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/base/CMakeFiles/app.dir/flags.make -D
-DCH07_DEBUG_TRACE=1
[退出码 0]
```

第二份工具解决"覆盖范围"这一类问题 —— 谁有、谁没有、谁多了一条，一眼看完：

`dump.sh <构建目录> <C_INCLUDES | C_DEFINES | C_FLAGS>` 的实现就一个循环：
`find` 出所有 `flags.make`，从路径里剥出 target 名，`grep -m1 "^$KEY = "` 取出那一行再去掉行名。
全章"覆盖范围"类的问题都靠它一眼看完。用法和它的第一份输出：

```
$ /tmp/cmake07-AqiaHO/dump.sh /tmp/cmake07-AqiaHO/b/base C_INCLUDES
app       -I/tmp/cmake07-AqiaHO/ch07/include
calc      -I/tmp/cmake07-AqiaHO/ch07/include
fooprog   -I/tmp/cmake07-AqiaHO/ch07/include
greet     -I/tmp/cmake07-AqiaHO/ch07/include
leaf_a    
leaf_b    
util      -I/tmp/cmake07-AqiaHO/ch07/include
notepad   
[退出码 0]
  ★ 这份输出后面反复要用：现代写法里 -I 只出现在真正 include 了本工程头文件的 target 上，
    leaf_a、leaf_b、notepad 三行是空的 —— 它们一个本工程的头都不需要。
```

（第三份工具 `snap.sh` 要等第 27 节抓基线时才用，它把整个构建目录的 `flags.make` + `link.txt`
原样钉进一个文件，内容我放在第 27 节里念。）

★ 这里有一个"看起来该有、其实没有"的东西：**`-I` 的行尾没有冒号、`C_FLAGS = ` 后面是空的。**
`flags.make` 里没有标志就是真的没有，不会写个 `# 无` 给你。所以本章凡是"证明不存在"的命令
都靠 `grep -c`（0 + 退出码 1）或 `wc -l`（0），绝不靠"我看了一眼觉得没有"。

### 第 3 节 include_directories 到底什么时候算：三个靶子站三个位置

流传最广的一句话是"全局指令必须写在 `add_executable` **前面**，否则吃不到"。
这一节把它当场判掉。`variants/order/CMakeLists.txt` 里有五个靶子，位置各不相同：

```cmake
add_executable(early "${SRC}")          # ① 在 include_directories 之前创建
add_subdirectory(sub_before)            # ③ 子目录在它之前

include_directories("${ROOT}/include")  # ← 被试的那一行
add_definitions(-DORDER_MARK=1)

get_target_property(P2 early INCLUDE_DIRECTORIES)   # ② 之后回读 early
add_subdirectory(sub_after)             # ④ 子目录在它之后
add_executable(late "${SRC}")           # ⑤ 在它之后创建
```

配置期直接把属性 `message` 出来（属性读的是"那一刻"的值）：

```
$ cmake -S /tmp/cmake07-AqiaHO/ch07/variants/order -B /tmp/cmake07-AqiaHO/b/order 2>&1 | grep -E 'early|late|before_sub|after_sub'
-- ① 刚 add_executable 完：early 的 INCLUDE_DIRECTORIES = [P1-NOTFOUND]
--   before_sub（子目录，先加进来）的 INCLUDE_DIRECTORIES = [PBS-NOTFOUND]
-- ② include_directories 之后：early 的 INCLUDE_DIRECTORIES = [/tmp/cmake07-AqiaHO/ch07/include]
--   after_sub（子目录，后加进来）的 INCLUDE_DIRECTORIES = [/tmp/cmake07-AqiaHO/ch07/include]
-- ③ 在它之后才创建的 late 的 INCLUDE_DIRECTORIES = [/tmp/cmake07-AqiaHO/ch07/include]
[退出码 0]
  五个靶子的位置：early 在 include_directories **之前**创建；late 在**之后**；
  before_sub 在**之前**被 add_subdirectory；after_sub 在**之后**；三个 message 直接读属性。
```

`P1-NOTFOUND` / `PBS-NOTFOUND` 是 `get_target_property` 对"属性没设过"的返回值
（CMake 把 `<PROP>-NOTFOUND` 当成未设置的哨兵字符串），所以第①行说的不是"early 少了什么"，
而是"这一刻属性还没被设过"。

**但属性读出来还不算数，要看最终命令行。** 这才是关键的一步：

```
$ for t in early late; do printf '%-6s ' $t; /tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/order/CMakeFiles/$t.dir/flags.make -I; done
early  -I/tmp/cmake07-AqiaHO/ch07/include
late   -I/tmp/cmake07-AqiaHO/ch07/include
[退出码 0]

$ for p in CMakeFiles/early.dir sub_before/CMakeFiles/before_sub.dir sub_after/CMakeFiles/after_sub.dir; do printf '%-40s ' $p; grep -h '^C_INCLUDES' /tmp/cmake07-AqiaHO/b/order/$p/flags.make; done
CMakeFiles/early.dir                     C_INCLUDES = -I/tmp/cmake07-AqiaHO/ch07/include
sub_before/CMakeFiles/before_sub.dir     C_INCLUDES = 
sub_after/CMakeFiles/after_sub.dir       C_INCLUDES = -I/tmp/cmake07-AqiaHO/ch07/include
[退出码 0]
  ★ 屏幕上真正的一条线：**同一个目录里不分先后，全都吃到**（early 在那条指令之前就创建了，照样有 -I）；
    真正分先后的是**子目录** —— 它吃不吃得到，只看 add_subdirectory 那一刻目录属性里有没有东西。
```

`add_definitions` 完全同一套规则：

```
$ for p in CMakeFiles/early.dir sub_before/CMakeFiles/before_sub.dir sub_after/CMakeFiles/after_sub.dir; do printf '%-40s ' $p; grep -h '^C_DEFINES' /tmp/cmake07-AqiaHO/b/order/$p/flags.make; done
CMakeFiles/early.dir                     C_DEFINES = -DORDER_MARK=1
sub_before/CMakeFiles/before_sub.dir     C_DEFINES = 
sub_after/CMakeFiles/after_sub.dir       C_DEFINES = -DORDER_MARK=1
[退出码 0]
```

★ 于是那句流传最广的话被拆成两半：

* **"必须写在 target 前面"对 `include_directories` / `add_definitions` 是假的。**
  它们是目录属性，生成期才读，同一目录里的 target 不分先后全都能吃到。
  对它们来说，写在前面不是"为了生效"，而是**为了扩大污染面**（第 4 节）。
* **对 `link_directories` 是真的**，而且真得很安静 —— 写在后面是一个 `-L` 都不给，第 10 节当场演。
* 顺带一条：这也是为什么本章反复强调"别看 `CMakeLists.txt`，看 `flags.make`"：
  属性读出来是 `[P1-NOTFOUND]`，命令行里却有 `-I`。只看前一种会得出完全相反的结论。


---

## 第 1 部分 · 真病：`include_directories` 给的不是"能不能编"，是"给了谁"（第 4~7 节）

第 3 节把"老指令必须写在最前面"判掉了：目录级命令**在同一目录里不分先后**。
剩下的问题才要命：既然不分先后都能吃到，那**到底多少人吃到了**？
第 4、5 节量覆盖面，第 6 节给验收动作，第 7 节证明"顺序"这一项也得一起对。

### 第 4 节 真病一：一个目录里所有 target 都吃到，包括你不想给的那个

把 `CH07_LEG_INCLUDE`、`CH07_LEG_DEFS` 拨到 `ON`，其余五个开关保持 `OFF`：
只有"头目录怎么给"这一处换回老写法，源码、ROOT、开关值全和 `b/base` 一样。
配置那一步照打 `-- Configuring done` + `-- Build files have been written to: <沙箱>/b/legacyall`（退出码 0），
构建也是退出码 0、`[100%] Built target app`、`[100%] Built target greet`，屏幕上一个警告都没有。
**污染从不表现为构建失败。** 把两棵树的覆盖面各扫一遍
（`dump.sh` 是第 2 节写进沙箱的工具，第二个参数是要看的属性名，逐 target 打一行）：

```
$ /tmp/cmake07-AqiaHO/dump.sh /tmp/cmake07-AqiaHO/b/base C_INCLUDES
app       -I/tmp/cmake07-AqiaHO/ch07/include
calc      -I/tmp/cmake07-AqiaHO/ch07/include
fooprog   -I/tmp/cmake07-AqiaHO/ch07/include
greet     -I/tmp/cmake07-AqiaHO/ch07/include
leaf_a    
leaf_b    
util      -I/tmp/cmake07-AqiaHO/ch07/include
notepad   
[退出码 0]
```


```
$ /tmp/cmake07-AqiaHO/dump.sh /tmp/cmake07-AqiaHO/b/legacyall C_INCLUDES
app       -I/tmp/cmake07-AqiaHO/ch07/include
calc      -I/tmp/cmake07-AqiaHO/ch07/include
fooprog   -I/tmp/cmake07-AqiaHO/ch07/include
greet     -I/tmp/cmake07-AqiaHO/ch07/include
leaf_a    -I/tmp/cmake07-AqiaHO/ch07/include
leaf_b    -I/tmp/cmake07-AqiaHO/ch07/include
util      -I/tmp/cmake07-AqiaHO/ch07/include
notepad   -I/tmp/cmake07-AqiaHO/ch07/include
[退出码 0]
```


上面那份 `leaf_a`、`leaf_b`、`notepad` 三行等号后面是空的，下面这份这三行各多一条 `-I`；
其余五个 target 两份完全相同 —— 它们本来就要那个头目录。为什么偏偏是这三个：
`src/leaf/leaf_a.c` 和 `tp/notepad.c` 都只 `#include <stdio.h>`，一个本工程头文件都不引，
所以现代写法里 `target_include_directories` 没点它们的名字，`flags.make` 自然空着。
而 `include_directories` 是**目录级**的：作用域是"这个目录里此刻和之后所有 target"，
只看"你在不在"，不看"你要不要"。

★ 这才是 `include_directories` 的真病：**不是"忘了写就吃不到"，而是"你想给一个、结果全给了"**。
危害不是编译不过（它永远编译得过），是另外两件事：① 同名头文件的赢家会变（第 7 节当场翻出来）；
② 导出的库会把这条 `-I` 一起交付出去，变成别人机器上的坑（第 03 章讲 `install` / `export` 时演的正是它）。
顺带一句：**多给一条 `-I` 从不当场报错，所以它从不当场被发现。**

### 第 5 节 真病二：污染会顺着 `add_subdirectory` 走进第三方目录

两棵树里都有第三方 target `notepad`，它来自 `add_subdirectory(tp)`，
`tp/CMakeLists.txt` 是"别人写的工程"，源码只 `#include <stdio.h>`。把它的 `C_INCLUDES` 抓出来：

```
$ for d in base legacyall; do printf '%-10s ' $d; grep -h '^C_INCLUDES' /tmp/cmake07-AqiaHO/b/$d/tp/CMakeFiles/notepad.dir/flags.make; done
base       C_INCLUDES = 
legacyall  C_INCLUDES = -I/tmp/cmake07-AqiaHO/ch07/include
[退出码 0]
```


现代那行 `C_INCLUDES = ` 等号后面什么都没有，老写法那行多一条 `-I`。再跑起来（现代那份只打印
`[notepad] 我只是想记一行笔记，第 1 行`，也就是下面围栏的第二行）：

```
$ /tmp/cmake07-AqiaHO/b/legacyall/tp/notepad
[notepad] 有人给全工程打开了 trace，我被迫打印这行
[notepad] 我只是想记一行笔记，第 1 行
[退出码 0]
```


多出来那行 trace 不是 notepad 想打的：它源码里有一块 `#ifdef CH07_DEBUG_TRACE` 的"被迫打开"分支，
被老写法的 `add_definitions` 全工程点亮了（本节只拨了 `INCLUDE`/`DEFS`，动这只手的是后一条）。
**`-I` 只让它"能"看见那个头，`-D` 才让它"改口"。**

★ 一个只 `#include <stdio.h>` 的第三方目录，现代写法给它空的 `C_INCLUDES`、它只打一行；
老写法给它 `-I` 并把它改成口。**第三方子目录最容易中招**：它通常只有一个 `#ifdef`，没人会去读，
一旦被全工程点亮，输出的就是**别人家的日志**。这就是 `target_*` 系列要求逐个点名的理由 ——
点名的代价是啰嗦，回报是"没点到的真的没有"。

### 第 6 节 改写对质：同一份源码，两种写法，`-I` 序列逐 target diff

第 4 节看的是整张表，迁移时要的是**一条能跑的命令**。把两边 `flags.make` 里的 `-I`
抽成一行一个（`seq.sh`，第 2 节写进沙箱），逐个 target diff：

```
$ for t in app greet calc util fooprog; do printf '%-8s ' $t; diff <(/tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/base/CMakeFiles/$t.dir/flags.make -I) <(/tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/legacyall/CMakeFiles/$t.dir/flags.make -I) >/dev/null && echo 完全一致 || echo 有差别; done
app      完全一致
greet    完全一致
calc     完全一致
util     完全一致
fooprog  完全一致
[退出码 0]
```


五行"完全一致"的都是本来就该有 `-I` 的 target —— 这正是想要的结果。
反过来，`leaf_a`/`leaf_b`/`notepad` 三个是"现代 0 条、老写 1 条"
（同一批 diff 换成数条数的写法，六行输出依次是 `0 1 0 1 0 1`）。两类差别必须**分开记账**：
前者"必须为 0，否则是你改坏了"，后者"预期不为 0，而且方向必须是少一条、不是多一条"。

★ 这一项的验收动作就是这条 diff：`-I` 序列的差异**只允许出现在事先点名的那几个 target 上**，
其余逐字符相同。上面那两条循环就是它的可跑版本：`seq.sh` 负责把一行拆成"一行一个 `-I`"，
`diff` 负责连顺序一起比，`退出码 0` 才算过。第 6 部分走查每一步用的都是它的某一版。

### 第 7 节 `-I` 的"顺序"也是命令行的一部分：`BEFORE` 那一手当场测

前面的 diff 都在比"有没有"，这一节比"谁在前"。靶子是两个同名头文件，
`std/ch07/hdrwin.h` 里是 `#define CH07_WHO "std 目录赢了"`，`alt/ch07/hdrwin.h` 里是
`#define CH07_WHO "alt 目录赢了"`。`variants/inc-order/CMakeLists.txt` 把这两个目录给
`include_directories`，只有一个 `WAY` 开关：`plain` 原样给，`before` 在前面加一个 `BEFORE`。
两次配置打印的**目录清单顺序一模一样**（`两个目录 = [.../alt] [.../std]`）。看命令行和结果：

```
$ /tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/incplain/CMakeFiles/hdrwin.dir/flags.make -I | sed 's|-I/tmp/cmake07-AqiaHO/ch07/variants/inc-order/|-I|' && /tmp/cmake07-AqiaHO/b/incplain/hdrwin
-Ialt
-Istd
[hdrwin] alt 目录赢了
[退出码 0]
```


```
$ /tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/incbefore/CMakeFiles/hdrwin.dir/flags.make -I | sed 's|-I/tmp/cmake07-AqiaHO/ch07/variants/inc-order/|-I|' && /tmp/cmake07-AqiaHO/b/incbefore/hdrwin
-Istd
-Ialt
[hdrwin] std 目录赢了
[退出码 0]
```


同一个 `hdrwin`、同一行 `#include <ch07/hdrwin.h>`，一个打印 `alt 目录赢了`、一个打印 `std 目录赢了`。
再数两边各有几条 `-I`：

```
$ /tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/incplain/CMakeFiles/hdrwin.dir/flags.make -I | wc -l && /tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/incbefore/CMakeFiles/hdrwin.dir/flags.make -I | wc -l
2
2
[退出码 0]
```


两条都是 `2` —— **集合完全相同，只是顺序换了**。所以"比条数"这种偷懒写法在这里会漏判：
条数一致、内容一致，赢家却换了。要抓它只能整条序列按顺序比（`diff`，不是 `wc -l`）。

★ 本机实测：`include_directories(BEFORE 目录1 目录2)` 把**整批反着**插到最前面 ——
`WAY=before` 是 `-Istd -Ialt`，`WAY=plain` 是 `-Ialt -Istd`。两条结论：
① `-I` 的**顺序**和**条数**是两个独立的量，验收都得看；
② `BEFORE` 是老写法里少见的"看起来无害、其实重排全局"的参数。新写法靠"给单个 target 只挂一个目录"
来消除歧义（`target_include_directories` 也接受 `BEFORE`，但那只动这一个 target 的那一串）。

---

## 第 2 部分 · `link_directories` 与 `-L`：赢家不写在文件里（第 8~12 节）

`-I` 那一族的坑是"多给了"，`-L` 这一族的坑是"选哪个根本不由 `CMakeLists.txt` 决定"。
第 8 节看它落到命令行上长什么样，第 9、10 节一次当场翻转、一次当场清零，
第 11 节给新写法，第 12 节给这一项能用的验收。

### 第 8 节 `link_directories` 落到命令行上是什么样子：`-L` 之外还多送一个 `-rpath`

把 `b/legacyall` 再配置一次，把后面四个老开关一次拨满（`LINK`/`GLOB`/`FLAGS`/`PLAIN`/`CACHEVAR`）。
这条命令里**没有**再给 `INCLUDE`/`DEFS`，但第 4 节拨的那两个仍然生效 —— `-D` 写进的是 cache，
同一个 `-B` 目录复用就不会掉（证据在第 13 节那张表里：八个 target 每行还带着三条 `-D`）。
链这件事只看一个 target：`fooprog`，唯一一个要链 `libfoo.a` 的。

```
$ cat /tmp/cmake07-AqiaHO/b/legacyall/CMakeFiles/fooprog.dir/link.txt
/usr/bin/cc -Wl,--dependency-file=CMakeFiles/fooprog.dir/link.d CMakeFiles/fooprog.dir/src/fooprog.c.o -o fooprog   -L/tmp/cmake07-AqiaHO/libs/v1  -L/tmp/cmake07-AqiaHO/libs/v2  -Wl,-rpath,/tmp/cmake07-AqiaHO/libs/v1:/tmp/cmake07-AqiaHO/libs/v2 -lfoo
[退出码 0]
```


`seq.sh <沙箱>/b/legacyall/CMakeFiles/fooprog.dir/link.txt -L` 抽出两行：
`-L<沙箱>/libs/v1`、`-L<沙箱>/libs/v2`。注意上面那行原文里还多一件事：
`-Wl,-rpath,<沙箱>/libs/v1:<沙箱>/libs/v2` —— `link_directories` 不只给 `-L`，
**它把每个目录同时塞进 rpath**。于是这个可执行文件运行时会再去这两条路径找一次动态库，
属于"构建期不出事、搬走才出事"那一类。

★ 老写法那一行 `link_directories(${A} ${B})` 落到命令行是三种东西：两条 `-L`、一条 `-rpath`、
外加后面 `-lfoo` 的查找顺序。源文件里那行字完全看不出这些 ——
这就是"别看 `CMakeLists.txt`，看 `link.txt`"在链接这一项上最具体的理由。

### 第 9 节 `-L` 的顺序决定谁赢：两份同名库，屏幕上是两个不同的数字

`variants/link-order` 只有两个变量：`FOO_A`/`FOO_B` 指到 `libs/v1`、`libs/v2`
（两份 `libfoo.a` 的 `foo_tag()` 分别返回 1 和 2），`ORDER` 决定两个目录的先后。`ORDER=ab`：

```
$ cmake -S /tmp/cmake07-AqiaHO/ch07/variants/link-order -B /tmp/cmake07-AqiaHO/b/lo12 -DFOO_A=/tmp/cmake07-AqiaHO/libs/v1 -DFOO_B=/tmp/cmake07-AqiaHO/libs/v2 -DORDER=ab -DWHEN=before >/dev/null && cmake --build /tmp/cmake07-AqiaHO/b/lo12 >/dev/null && /tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/lo12/CMakeFiles/orderapp.dir/link.txt -L && /tmp/cmake07-AqiaHO/b/lo12/orderapp
-L/tmp/cmake07-AqiaHO/libs/v1
-L/tmp/cmake07-AqiaHO/libs/v2
[fooprog] foo_tag() = 1
[退出码 0]
```


`ORDER=ba` —— `CMakeLists.txt` 一个字节没动，只是命令行换了两个变量的先后：

```
$ cmake -S /tmp/cmake07-AqiaHO/ch07/variants/link-order -B /tmp/cmake07-AqiaHO/b/lo21 -DFOO_A=/tmp/cmake07-AqiaHO/libs/v1 -DFOO_B=/tmp/cmake07-AqiaHO/libs/v2 -DORDER=ba -DWHEN=before >/dev/null && cmake --build /tmp/cmake07-AqiaHO/b/lo21 >/dev/null && /tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/lo21/CMakeFiles/orderapp.dir/link.txt -L && /tmp/cmake07-AqiaHO/b/lo21/orderapp
-L/tmp/cmake07-AqiaHO/libs/v2
-L/tmp/cmake07-AqiaHO/libs/v1
[fooprog] foo_tag() = 2
[退出码 0]
```


`-L` 两行整批反了顺序，跑出来从 `1` 变成 `2`。把两份 `link.txt` 的 `-L` 序列 diff，
只有两行位置交换（`1d0` 加 `2a2`），退出码 1。谁在前谁赢，这是链接器规则不是 CMake 规则，
CMake 只负责把清单原样抄进命令行。

★ 这就是 `link_directories` 的全部危险性：**赢家写在你的手指头上，不写在文件里。**
`git diff` 看不出这次改动，审查看不出，构建也不报错；唯一的痕迹是 `-L` 的**顺序**，
而这顺序由两个变量在命令行上谁先出现决定。同一份 `CMakeLists.txt` 在 CI 和本机之间跑出不同数字，
最常见的成因就在这里。新写法把"选哪个"变成写在文件里的一个字（第 11 节）。

### 第 10 节 `link_directories` 是快照：写在 `add_executable` 之后，一个 `-L` 都没有

还是 `variants/link-order`，把 `WHEN` 从 `before` 换成 `after` —— 意思是 `link_directories`
那一行被挪到 `add_executable(orderapp ...)` **后面**。第 3 节那句"目录级命令不分先后"不包括它。
配置这一步照样成功（`-- Configuring done` 照打，退出码 0）。它的 `link.txt`：

```
$ cat /tmp/cmake07-AqiaHO/b/loafter/CMakeFiles/orderapp.dir/link.txt
/usr/bin/cc -Wl,--dependency-file=CMakeFiles/orderapp.dir/link.d "CMakeFiles/orderapp.dir/tmp/cmake07-AqiaHO/ch07/src/fooprog.c.o" -o orderapp  -lfoo
[退出码 0]
```


`-L` 呢？数一下：

```
$ /tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/loafter/CMakeFiles/orderapp.dir/link.txt -L | wc -l
0
[退出码 1]
```


`0` 条，而且这条管道退出码是 `1` —— `seq.sh` 里的 `grep` 什么都没匹配到。
**0 条不是"没测出来"，是真的没有**，这就是引用约定第 2 条说的"退出码 1 加 `0` 才算确认不存在"。
后果当场出现：

```
$ cmake --build /tmp/cmake07-AqiaHO/b/loafter 2>&1 | tail -4
collect2: error: ld returned 1 exit status
gmake[2]: *** [CMakeFiles/orderapp.dir/build.make:101: orderapp] Error 1
gmake[1]: *** [CMakeFiles/Makefile2:87: CMakeFiles/orderapp.dir/all] Error 2
gmake: *** [Makefile:91: all] Error 2
[退出码 2]
```


`collect2: error: ld returned 1 exit status` 是链接器在说话，`gmake[2]: *** [...] Error 1`
是 make 在转述，退出码 2。

★ 三种目录级全局指令，这里是**第三种行为**：`link_directories` 只看 target 创建那一刻
（它被读进一个待用清单，`add_executable` 取走之后目录里就空了）。
于是那句"必须写在前面"对它是**真的**，而且错得很难看：写在后面没有任何警告，
一路走到链接期才炸。第 1 节那句"同一目录不分先后"要加上这条才完整 ——
`include_directories` / `add_definitions` 是生成期读（不分先后），`link_directories` 是创建期取（必须在前）。

### 第 11 节 新写法不需要 `-L`：给 target 名字，或者给 `.a` 的绝对路径

回到现代基线 `b/base`，同一个 `fooprog` 的 `link.txt`：

```
$ cat /tmp/cmake07-AqiaHO/b/base/CMakeFiles/fooprog.dir/link.txt
/usr/bin/cc -Wl,--dependency-file=CMakeFiles/fooprog.dir/link.d CMakeFiles/fooprog.dir/src/fooprog.c.o -o fooprog  /tmp/cmake07-AqiaHO/libs/v1/libfoo.a
[退出码 0]
```


整行里**没有 `-L`、没有 `-lfoo`、也没有 `-rpath`**，只有一个绝对路径
`<沙箱>/libs/v1/libfoo.a`，跑出来是 `[fooprog] foo_tag() = 1`。它是 `modern/CMakeLists.txt` 里
`add_library(Foo::foo STATIC IMPORTED)` + `set_property(... IMPORTED_LOCATION ...)` +
`target_link_libraries(fooprog PRIVATE Foo::foo)` 三行合起来的效果；
`find_package` 带 imported target 时走的是同一套（第 04 章）。
"选哪个库"现在写在配置命令的一个变量里，并且**直接印在 `link.txt` 上**：

```
$ cmake -S /tmp/cmake07-AqiaHO/ch07 -B /tmp/cmake07-AqiaHO/b/pick2 -DCH07_FOO_PICK=2 -DCH07_FOO_DIR1=/tmp/cmake07-AqiaHO/libs/v1 -DCH07_FOO_DIR2=/tmp/cmake07-AqiaHO/libs/v2 2>&1 | grep -E '回滚开关|Configuring done'
--   回滚开关：INCLUDE=OFF DEFS=OFF LINK=OFF
-- Configuring done (0.1s)
[退出码 0]
```


```
$ cmake --build /tmp/cmake07-AqiaHO/b/pick2 --target fooprog >/dev/null && cat /tmp/cmake07-AqiaHO/b/pick2/CMakeFiles/fooprog.dir/link.txt && /tmp/cmake07-AqiaHO/b/pick2/fooprog
/usr/bin/cc -Wl,--dependency-file=CMakeFiles/fooprog.dir/link.d CMakeFiles/fooprog.dir/src/fooprog.c.o -o fooprog  /tmp/cmake07-AqiaHO/libs/v2/libfoo.a
[fooprog] foo_tag() = 2
[退出码 0]
```


`-DCH07_FOO_PICK=2` 一换，那个绝对路径就换成 `libs/v2/libfoo.a`，跑出来 `foo_tag() = 2`。

★ 新旧写法在这一项上的真正差别不是"新写法更简洁"，是**决策位置换了**：
老写法要三个信息（目录清单 + `-lfoo` 的名字 + 谁在前的顺序）才凑出一个赢家，其中两个不在屏幕上；
新写法只要一个信息（这个 `.a` 的完整路径），它就印在 `link.txt` 里。
审查时的问句从"这两个目录谁先传？"变成"这一行写的是哪个文件？" —— 后者 `git diff` 就能答。

### 第 12 节 链接这一项的迁移验收：`link.txt` 规范化 diff

机制换了，`link.txt` 必然不等：老写法两条 `-L` 加 `-lfoo`，新写法一个绝对路径。
所以验收不能要求"diff 为 0"，要要求"**差异只有这一种形状**"。整条 `-L` 序列的 diff：

```
$ diff <(/tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/legacyall/CMakeFiles/fooprog.dir/link.txt -L) <(/tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/base/CMakeFiles/fooprog.dir/link.txt -L); echo "[-L diff 退出码 $?]"
1,2d0
< -L/tmp/cmake07-AqiaHO/libs/v1
< -L/tmp/cmake07-AqiaHO/libs/v2
[-L diff 退出码 1]
[退出码 0]
```


`1,2d0`：新写法里少的正是那两行 `-L`（现代那棵树用 `seq.sh` 数 `-L` 是空输出加退出码 1，
即"确实一条都没有"）。库的点名方式也换了 ——
`grep -oE -- '-lfoo'` 在老写法 `link.txt` 里抓到 `-lfoo`（名字 + 靠 `-L` 找），
`grep -oE '[^ ]*libfoo.a'` 在现代 `link.txt` 里抓到 `<沙箱>/libs/v1/libfoo.a`（直接就是那个文件）。
最后量覆盖面：`-L` 是只给了 `fooprog`，还是像 `-I` 那样全给？`grep -l -- -L` 在老写法那棵树里
点名了 **6 个** `link.txt`：`app`、`fooprog`、`greet`、`leaf_a`、`leaf_b`，连第三方 `notepad` 都在。
现代基线那一侧同样一条命令只数出 **0 个**：

```
$ grep -l -- -L /tmp/cmake07-AqiaHO/b/base/CMakeFiles/*.dir/link.txt /tmp/cmake07-AqiaHO/b/base/tp/CMakeFiles/*.dir/link.txt 2>/dev/null | wc -l
0
[退出码 1]
```


（`wc -l` 报 `0`、`grep -l` 退出码 `1`，两件事同时成立才算数）。

★ 这一节是"机制换了怎么办"的样板：diff 不为 0 时**不要试图让它变成 0**，
而是把允许的差别写死成一句话 —— 这里是"`-L` 序列整批消失，库从 `-lfoo` 变成一个 `.a` 绝对路径，
且带 `-L` 的文件数从 6 变 0"。超出这句话的每一行都要当事故查。第 28 节走查用的就是这条判据。

---

## 第 3 部分 · `add_definitions` 与宏：一次配置，所有 target 一起变（第 13~16 节）

宏这一族的坑和第 4 节同构，但后果更贵：`-I` 多给一条通常没人察觉，`-D` 多给一条会
**直接改程序运行时的行为**。第 13 节量覆盖面，第 14 节换一个完全独立的取证口子
（不问 CMake，问预处理器），第 15 节演最贵的那种失败，第 16 节给这一项的验收。

### 第 13 节 `add_definitions` 的覆盖范围：一次配置，所有 target 一起变

老写法树 `b/legacyall`（第 4 节把 `CH07_LEG_DEFS` 拨在 `ON`，此后一直没关）的八行 `C_DEFINES`：

```
$ /tmp/cmake07-AqiaHO/dump.sh /tmp/cmake07-AqiaHO/b/legacyall C_DEFINES
app       -DCH07_DEBUG_TRACE=1 -DCH07_MODE_PLUS=1 -DCH07_STAMP=7
calc      -DCH07_DEBUG_TRACE=1 -DCH07_MODE_PLUS=1 -DCH07_STAMP=7
fooprog   -DCH07_DEBUG_TRACE=1 -DCH07_MODE_PLUS=1 -DCH07_STAMP=7
greet     -DCH07_DEBUG_TRACE=1 -DCH07_MODE_PLUS=1 -DCH07_STAMP=7
leaf_a    -DCH07_DEBUG_TRACE=1 -DCH07_MODE_PLUS=1 -DCH07_STAMP=7
leaf_b    -DCH07_DEBUG_TRACE=1 -DCH07_MODE_PLUS=1 -DCH07_STAMP=7
util      -DCH07_DEBUG_TRACE=1 -DCH07_MODE_PLUS=1 -DCH07_STAMP=7
notepad   -DCH07_DEBUG_TRACE=1 -DCH07_MODE_PLUS=1 -DCH07_STAMP=7
[退出码 0]
```


八行**一模一样**，每行三条 `-D`。现代基线里挑三个看（它们各只需一条）：

```
$ for t in util calc app; do printf '%-6s ' $t; grep -h '^C_DEFINES' /tmp/cmake07-AqiaHO/b/base/CMakeFiles/$t.dir/flags.make; done
util   C_DEFINES = -DCH07_STAMP=7
calc   C_DEFINES = -DCH07_MODE_PLUS=1
app    C_DEFINES = -DCH07_DEBUG_TRACE=1
[退出码 0]
```


`util` 要 `CH07_STAMP`（它打印这个数），`calc` 要 `CH07_MODE_PLUS`（它选加还是乘），
`app` 要 `CH07_DEBUG_TRACE`（它自己那行 trace）。三条不同的宏，现代写法用三行
`target_compile_definitions(... PRIVATE ...)` 精确给到；老写法一行
`add_definitions(-DA -DB -DC)` 全给。它和 `include_directories` 同族（目录属性、生成期读），
所以第 3 节"同一目录不分先后"对它同样成立。

★ 老写法那一半：**每一个 target 都拿到一模一样的三个 `-D`**，包括第三方 `notepad`、
包括只 `#include <stdio.h>` 的 `leaf_a`/`leaf_b`。现代那一半里 `PRIVATE` 的意义看得见摸得着：
宏只在"编译这个 `.c` 的那一刻"存在，不外传。

### 第 14 节 不问 CMake，直接问预处理器：`-dM -E`

前面所有结论都经由 `flags.make`。这一节走另一条口子：让 gcc 自己把"此刻有哪些宏、值多少"打出来。
先只给 `-I`、不给 `-D`（模拟"手工 gcc 一下"）：`gcc -dM -E -I<沙箱>/ch07/include <沙箱>/ch07/src/util/util.c`
再 `grep -c CH07_STAMP`，数出 `0`、退出码 `1` —— 不给 `-D` 时 `CH07_STAMP` 确实不存在。
再把 `flags.make` 里的 `C_DEFINES` 和 `C_INCLUDES` **原样**抠出来喂回 gcc（问 `util`）：

```
$ gcc -dM -E $(grep -m1 '^C_DEFINES' /tmp/cmake07-AqiaHO/b/base/CMakeFiles/util.dir/flags.make | sed 's/^C_DEFINES = //') $(grep -m1 '^C_INCLUDES' /tmp/cmake07-AqiaHO/b/base/CMakeFiles/util.dir/flags.make | sed 's/^C_INCLUDES = //') /tmp/cmake07-AqiaHO/ch07/src/util/util.c | grep CH07
#define CH07_STAMP 7
#define CH07_UTIL_H 
[退出码 0]
```


`#define CH07_STAMP 7` 来自命令行，`#define CH07_UTIL_H` 来自头文件自己。
同理问 `greet`（它的 `C_DEFINES` 是空的）只剩 `CH07_UTIL_H` 一行。关键在第三问 —— 老写法树里第三方 `notepad`：

```
$ gcc -dM -E $(grep -m1 '^C_DEFINES' /tmp/cmake07-AqiaHO/b/legacyall/tp/CMakeFiles/notepad.dir/flags.make | sed 's/^C_DEFINES = //') /tmp/cmake07-AqiaHO/ch07/tp/notepad.c | grep CH07
#define CH07_MODE_PLUS 1
#define CH07_STAMP 7
#define CH07_DEBUG_TRACE 1
[退出码 0]
```


三行全在，而 `notepad.c` 的作者从来没要求过这三个宏中的任何一个。

★ 中间那条是本章最喜欢的一招：**把 `flags.make` 里的 `C_DEFINES` / `C_INCLUDES` 原样喂回
`gcc -dM -E`**，于是"这个 target 编译时到底有哪些宏"不再依赖 CMake 的任何解释，由预处理器自己签字。
查"这个宏是谁给的"时，这条命令比读三遍 `CMakeLists.txt` 快。

### 第 15 节 最贵的一种失败：构建全绿，程序行为变了

同一份源码，`b/legacyall` 里跑三个程序（`app`、`greet`、第三方 `notepad`）：

```
$ /tmp/cmake07-AqiaHO/b/legacyall/app && echo --- && /tmp/cmake07-AqiaHO/b/legacyall/greet && echo --- && /tmp/cmake07-AqiaHO/b/legacyall/tp/notepad
[app] trace: 进 main 了，参数准备给 2 和 3
[util] 编译器给我的 CH07_STAMP = 7
[calc] 走的是 PLUS 分支
[app] calc_add(2,3) = 5
---
[greet] 注意：有人给全工程打开了 CH07_DEBUG_TRACE，我也被打了 trace
[util] 编译器给我的 CH07_STAMP = 7
[greet] 你好，我只是来问个好
---
[notepad] 有人给全工程打开了 trace，我被迫打印这行
[notepad] 我只是想记一行笔记，第 1 行
[退出码 0]
```


把这段和现代基线 `b/base` 的三个程序逐个对齐，**只有两行是多出来的**：
`[greet] 注意：有人给全工程打开了 CH07_DEBUG_TRACE，我也被打了 trace` 和
`[notepad] 有人给全工程打开了 trace，我被迫打印这行`；其余九行逐字符相同，
包括 `[calc] 走的是 PLUS 分支` 和 `[app] calc_add(2,3) = 5`。而测试那边两边**同一行结果**：
`100% tests passed, 0 tests failed out of 2`，退出码 0（`b/base` 与 `b/legacyall` 各跑一次，输出完全一样）。
为什么测不出来：那两条用例断言的是 `calc_add` 的返回值，没有任何一条断言"输出几行"。

★ 这就是老写法的失败模式：**编译照过、测试照绿、行为变了。**
`-D` 污染贵在这里 —— 它改的是运行时可见的行为，而它的传播路径（目录属性）在 `CMakeLists.txt` 里
长得和"我只给这个 target 加个开关"一模一样。仓库里只要有那种"打开某个宏就多打十行日志"的调试分支，
`add_definitions` 就是那根随时可能被别人碰到的线。

### 第 16 节 `target_compile_definitions` 的 `-D` 序列 diff

这一项的验收动作和第 6 节一样是 diff，只是把 `-I` 换成 `-D`。四个 target 两边各打一行：

```
$ for t in calc util app greet; do printf '%-6s 现代: ' $t; /tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/base/CMakeFiles/$t.dir/flags.make -D | tr '\n' ' '; echo; printf '%-6s 老式: ' $t; /tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/legacyall/CMakeFiles/$t.dir/flags.make -D | tr '\n' ' '; echo; done
calc   现代: -DCH07_MODE_PLUS=1 
calc   老式: -DCH07_DEBUG_TRACE=1 -DCH07_MODE_PLUS=1 -DCH07_STAMP=7 
util   现代: -DCH07_STAMP=7 
util   老式: -DCH07_DEBUG_TRACE=1 -DCH07_MODE_PLUS=1 -DCH07_STAMP=7 
app    现代: -DCH07_DEBUG_TRACE=1 
app    老式: -DCH07_DEBUG_TRACE=1 -DCH07_MODE_PLUS=1 -DCH07_STAMP=7 
greet  现代: 
greet  老式: -DCH07_DEBUG_TRACE=1 -DCH07_MODE_PLUS=1 -DCH07_STAMP=7 
[退出码 0]
```


差别分两种，必须分开记账：`calc` 现代 1 条、老式 3 条 —— 需要的那条 `-DCH07_MODE_PLUS=1`
**两边都在**，老式多给两条；`greet` 现代 0 条、老式 3 条 —— 它一条都不需要。
对 `calc`、`util` 单独跑归一化 diff 的话，两个都输出"有差别"、退出码 1。

★ 于是"等价判定"的正确姿势是：**对确实需要这个宏的 target，盯的是"那条需要的宏两边同值"，
而不是整行 diff 为 0**（老式一定多给，第 13 节量过）；对不需要的 target（`greet` 现代空、老式三条），
这一项**不要求 diff 为 0**，它要求的是"迁移后必须变空"。第 28 节步骤 3 就是把这条判据落成一条命令。

---

## 第 4 部分 · `file(GLOB)`：清单是配置期拍的一张快照（第 17~22 节）

前面两族的账本至少还在构建目录里（`flags.make` / `link.txt`）。`file(GLOB)` 的账本是
**一个普通变量的值**：算完就和磁盘脱钩。第 17 节立基线，第 18 节演"文件在、构建绿、它没参与"，
第 19 节给唯一的补救，第 20、21 节把 `CONFIGURE_DEPENDS` 的两半（加文件 / 删文件）和代价量一遍，
第 22 节接回本仓库那份真实的 `CMakeLists.txt`。

### 第 17 节 `file(GLOB)` 的清单落在哪：只在配置期算一次

`variants/glob` 用 `file(GLOB ...)` 收集 `src/leaf/*.c`，每个 `.c` 变成一个 target 加一条用例。
配置期它会打印自己扫到了什么：

```
$ cmake -S /tmp/cmake07-AqiaHO/ch07/variants/glob -B /tmp/cmake07-AqiaHO/b/glob 2>&1 | grep -E 'GLOB|Configuring done'
-- GLOB 在配置期扫到 /tmp/cmake07-AqiaHO/ch07/variants/glob 下这几个 leaf：/tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_a.c;/tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_b.c
-- Configuring done (0.1s)
[退出码 0]
```


清单两行，而磁盘上此时也只有 `leaf_a.c`、`leaf_b.c` 两个文件（`ls -1` 两条，退出码 0），
一一对应。**这是基线。** 构建完看用例名单：

```
$ cmake --build /tmp/cmake07-AqiaHO/b/glob -j4 >/dev/null && ctest --test-dir /tmp/cmake07-AqiaHO/b/glob -N 2>&1 | tail -4
  Test #1: smoke_leaf_a
  Test #2: smoke_leaf_b

Total Tests: 2
[退出码 0]
```


`Total Tests: 2`。此刻"清单 = 磁盘 = 用例"三者对齐，`file(GLOB)` 看起来完全没问题 ——
它确实没问题，直到有人往那个目录里加一个文件。

### 第 18 节 真病：新增一个 `.c`，`cmake --build` 根本不知道它存在

只做一件事：往 `src/leaf/` 放一个 `leaf_c.c`（脚本直接建文件，不经过 `runc`，
所以屏幕上没有对应的 `$ ` 行）。**`CMakeLists.txt` 一个字节没改，configure 也没重跑**：

```
$ ls -1 /tmp/cmake07-AqiaHO/ch07/src/leaf
leaf_a.c
leaf_b.c
leaf_c.c
[退出码 0]
```


三个文件了。直接构建：退出码 0，屏幕最后两行只有 `[ 50%] Built target leaf_a`、
`[100%] Built target leaf_b`，`leaf_c` 一个字都没提。它到底知不知道 `leaf_c` 的存在？问生成的 Makefile：

```
$ grep -c leaf_c /tmp/cmake07-AqiaHO/b/glob/CMakeFiles/Makefile2
0
[退出码 1]
```


`Makefile2` 里 `leaf_c` 出现 `0` 次（`grep -c` 退出码 1 才是"真的没有"）。
整棵构建树里连一个叫 `leaf_c*` 的文件都找不出来（`find | wc -l` 也是 `0`，退出码 0）。
用例名单和实跑：

```
$ ctest --test-dir /tmp/cmake07-AqiaHO/b/glob 2>&1 | grep -E 'tests passed'
100% tests passed, 0 tests failed out of 2
[退出码 0]
```


`100% tests passed, 0 tests failed out of 2`。

★ 这一节是本章**最安静的一次事故**：文件在磁盘上、构建成功、测试全绿、退出码 0。
`file(GLOB)` 的结果是**配置期拍的一张快照**，拍完就和磁盘没关系了；新增文件不会让构建系统察觉，
于是出现"明明有这个文件，却怎么都不参与编译"。它难查是因为屏幕上没有任何一处说"少了一个"——
只有你以为它在了。

### 第 19 节 唯一的补救（不带 `CONFIGURE_DEPENDS` 时）：重跑 configure

补救动作只有一个字：再配置一次。**源码、`CMakeLists.txt` 都没动**：

```
$ cmake -S /tmp/cmake07-AqiaHO/ch07/variants/glob -B /tmp/cmake07-AqiaHO/b/glob 2>&1 | grep -E 'GLOB|Build files'
-- GLOB 在配置期扫到 /tmp/cmake07-AqiaHO/ch07/variants/glob 下这几个 leaf：/tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_a.c;/tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_b.c;/tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_c.c
-- Build files have been written to: /tmp/cmake07-AqiaHO/b/glob
[退出码 0]
```


清单变成三行了。构建：

```
$ cmake --build /tmp/cmake07-AqiaHO/b/glob 2>&1 | grep -E 'leaf_c|Built target'
[ 33%] Built target leaf_a
[ 66%] Built target leaf_b
[ 83%] Building C object CMakeFiles/leaf_c.dir/tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_c.c.o
[100%] Linking C executable leaf_c
[100%] Built target leaf_c
[退出码 0]
```


`[ 83%] Building C object CMakeFiles/leaf_c.dir/.../leaf_c.c.o` 这一行第一次出现，
用例名单随之从 2 条变 3 条（`Test #3: smoke_leaf_c` / `Total Tests: 3`，退出码 0）。

★ 从第 18 节到第 19 节，**全部差别就是重跑了一次 configure**，源码一个字没动。
这就是"GLOB 的清单只在配置期算"的可操作推论：要么每次加文件都手动重配置，
要么让构建系统在构建时发现清单不对 —— 后者就是第 20 节。
顺带一条 CI 常识：CI 里"clean checkout + 全新 `-B` 目录"每次都重新 configure，
所以这类坑**在 CI 上几乎从不出错**，出错的永远是本机那个复用了两周的构建目录。

### 第 20 节 `CONFIGURE_DEPENDS`：本机（Unix Makefiles + CMake 4.2.3）到底会不会自动重跑

`variants/glob-depends` 和上一节那个工程只差一个 `CONFIGURE_DEPENDS` 参数。
脚本先把 `leaf_c.c` 从磁盘删掉（`rm -f`，退出码 0），再配置一棵全新的 `b/globdep`：

```
$ cmake -S /tmp/cmake07-AqiaHO/ch07/variants/glob-depends -B /tmp/cmake07-AqiaHO/b/globdep 2>&1 | grep -E 'CONFIGURE_DEPENDS|Configuring done'
-- CONFIGURE_DEPENDS 版：扫到 /tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_a.c;/tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_b.c
-- Configuring done (0.1s)
[退出码 0]
```


清单两行，构建完用例也是两条（`Total Tests: 2`，退出码 0）—— **这就是基线**。
接着脚本用 heredoc 把 `leaf_c.c` 再写回磁盘（同样没有 `$ ` 行），然后只跑构建，一条命令都不多敲：

```
$ cmake --build /tmp/cmake07-AqiaHO/b/globdep 2>&1 | tail -16
-- GLOB mismatch!
The following files were added:
  +/tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_c.c
-- CONFIGURE_DEPENDS 版：扫到 /tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_a.c;/tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_b.c;/tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_c.c
-- Configuring done (0.0s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake07-AqiaHO/b/globdep
[ 33%] Built target leaf_a
[ 66%] Built target leaf_b
[ 83%] Building C object CMakeFiles/leaf_c.dir/tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_c.c.o
[100%] Linking C executable leaf_c
[100%] Built target leaf_c
[退出码 0]
```


`-- GLOB mismatch!` / `The following files were added:` / `+<沙箱>/ch07/src/leaf/leaf_c.c` 这三行
是 **CMake 在 `cmake --build` 的过程中自己打出来的**：构建系统发现清单和磁盘不一致，
回头重跑了 configure（后面紧跟着 `-- Configuring done` / `-- Generating done`），然后才继续编译链接。
退出码是 **0** —— 它没把这次重跑当成错误。用例名单跟着变成 3 条（`Total Tests: 3`）。

★ 本机实测：**会**。`cmake --build` 自己回头重跑了 configure，新文件当场进了清单。
但请把这条结论的边界一起记住：`CONFIGURE_DEPENDS` 是**和生成器有关**的行为 ——
CMake 自己就在报错文案里说过它"依生成器而定"，本机的组合是 Unix Makefiles + CMake 4.2.3，
测出来是"可用"；换 Ninja、换旧版 CMake，或者构建树是从别人机器上拷来的，都不能假设同样结果。
这一节测的是"我这台机器上现在如何"，不是"所有机器上都如何"。

### 第 21 节 `CONFIGURE_DEPENDS` 的另一半：删文件、以及它没扫干净的东西

把 `leaf_c.c` 从磁盘删掉，然后**只跑构建**：

```
$ rm -f /tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_c.c && cmake --build /tmp/cmake07-AqiaHO/b/globdep 2>&1 | tail -14
-- GLOB mismatch!
The following files were removed:
  -/tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_c.c
-- CONFIGURE_DEPENDS 版：扫到 /tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_a.c;/tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_b.c
-- Configuring done (0.0s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake07-AqiaHO/b/globdep
[ 50%] Built target leaf_a
[100%] Built target leaf_b
[退出码 0]
```


`The following files were removed:` 出现了，清单回到两行，`Built target` 只剩 `leaf_a`/`leaf_b`，
用例名单也回到 `Total Tests: 2`（退出码 0）。但构建目录里有个东西没跟着走：

```
$ find /tmp/cmake07-AqiaHO/b/globdep -maxdepth 1 -type f -perm -u+x | sort
/tmp/cmake07-AqiaHO/b/globdep/leaf_a
/tmp/cmake07-AqiaHO/b/globdep/leaf_b
/tmp/cmake07-AqiaHO/b/globdep/leaf_c
[退出码 0]
```


**`leaf_c` 这个可执行文件还在。** 生成器把清单修对了、把 Makefile 重写了，却不会去删上一次的产物。
这就是"GLOB mismatch 修的是账，不是货"。对照另一半：不带 `CONFIGURE_DEPENDS` 的那棵树
（第 18、19 节的状态）在同样删文件之后跑构建，屏幕最后四行是
`compilation terminated.` 加三行 `gmake: *** Error`，退出码 2 —— 清单里还留着一条指向空气的规则。
两边各有一种失败方式，这个对照很值钱：

| 情形 | 加文件 | 删文件 |
|---|---|---|
| `file(GLOB)` 不带 `CONFIGURE_DEPENDS` | 静默忽略（第 18 节，退出码 0） | 构建失败（退出码 2） |
| `file(GLOB ... CONFIGURE_DEPENDS)` | 当场重配置并编上（第 20 节） | 重配置、清单修对，但旧产物留在目录里 |

最后量"每次构建都重扫目录"的代价：同一个两三个文件的工程各跑一次 `time cmake --build`，
带 `CONFIGURE_DEPENDS` 的是 `real 0m0.072s`，不带的是 `real 0m0.062s`（差 10 毫秒）。

★ 删文件这一半本机也测了：`GLOB mismatch` 照样报、名单回到两条 —— **但它没碰磁盘**，
`find -maxdepth 1 -type f -perm -u+x` 数出三个可执行文件，多出来的那个就是 `leaf_c` 的尸体。
代价这一项本机量不出稳定的数：这一遍是 `0.072s` vs `0.062s`（差 10 毫秒），
上一遍同样的两条命令是 `0.082s` vs `0.081s`（差 1 毫秒）—— 差值本身比两遍之间的抖动还小。
所以正确的结论是两句：① 不要靠"感觉差不多"决定要不要 `CONFIGURE_DEPENDS`，
要靠第 18 节那种"清单 vs 磁盘"的对照；② 几千个文件的目录上会不会量出代价，**本章没测**（见文末清单）。

### 第 22 节 接回本仓库：那份 56 节的 `CMakeLists` 为什么写着「不写 `file(GLOB)`」

前面都是实验室工程。这一节读的是**你手上这份真实文件**（仓库根 `CMakeLists.txt`，本章只读不写）：

```
$ 〔在 9a503fd8 里〕 sed -n '31,36p' CMakeLists.txt
# ② **不写 `file(GLOB ...)` 自动收集源文件**。56 个目录的源文件是一个一个列出来的。
#    理由：GLOB 在 configure 时才扫目录 —— 新增一个 .c 不会让构建系统察觉，
#    于是出现"明明有这个文件却怎么都不参与编译"的玄学问题。显式清单看着啰嗦，
#    但每一次增删文件都会被迫重新 configure，错误当场暴露。
#    （本仓库另有 Makefile 用 `find` 自动发现目录，那是因为 Make 的 recipe 每次执行都重跑；
#     CMake 的清单只在生成阶段跑一次，两者的时机差别正是这条规矩的根。）
[退出码 0]
```


`grep -n 'file(GLOB' CMakeLists.txt` 在这份文件里只有一个命中，而命中就在第 31 行 ——
那一行是**注释**：这份文件里没有一处真的在用 GLOB，它是在解释为什么不用。
注释里"新增一个 .c 不会让构建系统察觉，于是出现『明明有这个文件却怎么都不参与编译』的玄学问题"
就是第 18 节当场演出来的那件事；它下面那句"Make 的 recipe 每次执行都重跑；
CMake 的清单只在生成阶段跑一次"是同一件事的成因，本章用 `find` / `grep -c` 把它证到了行号级别。

★ 这一节要接出去的结论就一句：本仓库对 GLOB 的态度不是风格偏好，
而是**被第 18 节那种"全绿屏幕 + 少一个用例"教育过之后的选择**。显式清单看着啰嗦，
但每一次增删文件都会被迫重新 configure，错误当场暴露。
读到这里你应该能替那条注释补一句更硬的理由：`CONFIGURE_DEPENDS` 能救加文件那一半，
但它是依生成器而定的（第 20 节），而"写死清单"不依赖任何生成器。

---

## 第 5 部分 · 剩下三种老写法：无关键字签名、全局变量清单、`COMPILE_FLAGS`（第 23~26 节）

前四部分那三族都能落到 `flags.make` / `link.txt` 上被抓。这一部分的三种是**尺子的盲区或半盲区**，
所以每节的重点都在"换哪个口子取证"：第 23 节看属性和硬错误，第 24 节看清单能不能被命令行改写，
第 25、26 节看字符串属性和列表属性的两条不同下场。

### 第 23 节 老写法四：无关键字 `target_link_libraries` —— 命令行一模一样，差别全在属性里

`variants/plain-link` 里有三个几乎同样的库：`calc_plain` 用**没有关键字**的
`target_link_libraries(calc_plain util)`，`calc_priv` 用 `PRIVATE`，`calc_pub` 用 `PUBLIC`。
配置期把各自的 `INTERFACE_LINK_LIBRARIES` 打印出来：

```
$ cmake -S /tmp/cmake07-AqiaHO/ch07/variants/plain-link -B /tmp/cmake07-AqiaHO/b/plain 2>&1 | grep -E 'INTERFACE_LINK'
-- plain   的 INTERFACE_LINK_LIBRARIES = [util]
-- PRIVATE 的 INTERFACE_LINK_LIBRARIES = [$<LINK_ONLY:util>]
-- PUBLIC  的 INTERFACE_LINK_LIBRARIES = [util]
[退出码 0]
```


同一场配置里 `grep -ciE 'warning|deprecat'` 数出 **0**（退出码 1）：**CMake 4.2.3 上没有一个字节的警告。**
构建完把三个使用方的 `link.txt` 做归一化 diff（`use_plain` vs `use_priv`，只把 target 名换成占位符）：

```
$ diff <(sed -e s/use_plain/USEX/g -e s/calc_plain/CALCX/g /tmp/cmake07-AqiaHO/b/plain/CMakeFiles/use_plain.dir/link.txt) <(sed -e s/use_priv/USEX/g -e s/calc_priv/CALCX/g /tmp/cmake07-AqiaHO/b/plain/CMakeFiles/use_priv.dir/link.txt); echo "[无关键字 vs PRIVATE：link.txt diff 退出码 $?]"
[无关键字 vs PRIVATE：link.txt diff 退出码 0]
[退出码 0]
```


**退出码 0 —— 逐字符相同。** 这就是本章那条"尺子的盲区"：命令行完全抓不到这一处，
唯一露馅的是上面那张属性表 —— 无关键字那行是 `[util]`，和 `PUBLIC` 同值；`PRIVATE` 那行是
`[$<LINK_ONLY:util>]`。也就是说在属性层面，**"不写关键字"选到的是 `PUBLIC` 那一族的行为，不是 `PRIVATE`。**
那它什么时候真的坏？看同一个调用里混着写裸名字和关键字的靶子 `mix/`：

```
$ cmake -S /tmp/cmake07-AqiaHO/mix -B /tmp/cmake07-AqiaHO/b/mix -DMIXROOT=/tmp/cmake07-AqiaHO/ch07/src 2>&1 | tail -6
CMake Error at CMakeLists.txt:6 (target_link_libraries):
  The INTERFACE, PUBLIC or PRIVATE option must appear as the second argument,
  just after the target name.


-- Configuring incomplete, errors occurred!
[退出码 1]
```


`The INTERFACE, PUBLIC or PRIVATE option must appear as the second argument, just after the target name.`
—— **硬错误**，退出码 1，不是警告。老仓库里"加一个依赖就顺手补一个裸名字"迟早把配置弄崩。

★ 结论（本机 4.2.3）：无关键字签名**照工作**，和 `PRIVATE` 版的 `link.txt` 逐字符相同，零警告。
同一棵构建树里两者看不出差别，差别在属性值上，**要到"把 target 导出给别人"那天才炸** ——
而"导出之后头文件/库怎么传出去"本章没演（见文末清单，那是第 03 章 `install` / `export` 的地盘）。
这一类的取证口子只有两个：属性打印，和"改到一半会硬报错"。

### 第 24 节 老写法五：全局变量 / CACHE 变量传依赖清单 → 一个 INTERFACE 库

老写法爱这么写：父目录 `set(ALL_DEPS calc util)`，子目录 `target_link_libraries(x ${ALL_DEPS})`。
靶子 `variants/cache-var` 把它做成三个 target：`cv_var`（普通变量清单）、`cv_cache`（`CACHE` 变量清单）、
`cv_iface`（一个 INTERFACE 库 `ch07_deps`）。配置期回显两行：
`子目录看到的 LATE_DEPS = []   ALL_DEPS = [calc;util]`，以及父目录那行
`cache-var：父目录 ALL_DEPS=[calc;util]  CACHE_DEPS=[calc;util]`。
`ALL_DEPS` 子目录看得到，`LATE_DEPS` 却是空 —— 它在 `add_subdirectory` **之后**才被 `set`，
子目录那一刻拿到的就是空。这是第 3 节"目录属性 vs 变量值"那条区别的第三次现身：
变量清单和 `link_directories` 一样是**那一刻**的快照。三个 target 的 `link.txt` 归一化后 diff 退出码 0，
三行内容依次是 `-o cv_var ../libcalc.a ../libutil.a`、`-o cv_cache ...`、`-o cv_iface ...` ——
清单换成 INTERFACE 库，命令行上**零变化**，换来的是"这件事只能在一处改"。看反例：

```
$ cat /tmp/cmake07-AqiaHO/b/cvlate/sub/CMakeFiles/cv_cache.dir/link.txt
/usr/bin/cc -Wl,--dependency-file=CMakeFiles/cv_cache.dir/link.d "CMakeFiles/cv_cache.dir/tmp/cmake07-AqiaHO/ch07/src/app.c.o" -o cv_cache  -lnope
[退出码 0]
```


`-lnope`。这一行是配置时写了 `-DCACHE_DEPS=nope` 的结果（那次配置退出码 0、`-- Configuring done` 照打）。
**没人校验过 `nope` 是不是一个真的库。** 本章到这里就停：`b/cvlate` 没有构建，
所以"它会不会炸、什么时候炸"不在本章的证据里。

★ 两条结论：`CACHE` 变量能被命令行改写（这里改成了根本不存在的 `nope`），而改写之后
**配置期一个字的抱怨都没有**；INTERFACE 库那一版没有可被命令行改写的字符串，
代价是多两行 `add_library(ch07_deps INTERFACE)` + `target_link_libraries(ch07_deps INTERFACE calc util)`。
清单类老写法的病不是"跑不通"，是"任何人都能在你看不见的那一步把它换掉"。

### 第 25 节 `COMPILE_FLAGS` vs `COMPILE_OPTIONS`：覆盖 / 追加 / 带空格的那个参数

`variants/comp-flags` 做了六个 target，两两一对。第一对测"覆盖还是追加"：

```
$ for t in cf_over cf_append; do printf '%-10s ' $t; grep -m1 '^C_FLAGS' /tmp/cmake07-AqiaHO/b/cf/CMakeFiles/$t.dir/flags.make | sed 's|^C_FLAGS = ||'; done
cf_over    -DCF_B=2
cf_append  -DCF_A=1 -DCF_B=2
[退出码 0]
```


`cf_over` 的 `C_FLAGS` 只剩 `-DCF_B=2`：字符串属性是**赋值**，后写那次把前一次整个盖掉；
`cf_append` 两条都在：列表属性是**追加**。第二对测带空格的参数怎么落到命令行：

```
$ for t in cf_space cf_list; do printf '%-9s ' $t; grep -m1 '^C_FLAGS' /tmp/cmake07-AqiaHO/b/cf/CMakeFiles/$t.dir/flags.make | sed 's|^C_FLAGS = ||'; done
cf_space  -DCF_MSG=a b
cf_list   "-DCF_MSG=a b"
[退出码 0]
```


`cf_space` 拿到裸的 `-DCF_MSG=a b` —— 中间那个空格被当成分隔符，`b` 掉出来变成"一个文件名"；
`cf_list` 拿到 `"-DCF_MSG=a b"`，引号原样进了 `flags.make`，整条是一个参数。后果当场可见：

```
$ cmake --build /tmp/cmake07-AqiaHO/b/cf --target cf_space 2>&1 | tail -4
gmake[3]: *** Deleting file 'CMakeFiles/cf_space.dir/tmp/cmake07-AqiaHO/ch07/src/leaf/leaf_a.c.o'
gmake[2]: *** [CMakeFiles/Makefile2:166: CMakeFiles/cf_space.dir/all] Error 2
gmake[1]: *** [CMakeFiles/Makefile2:173: CMakeFiles/cf_space.dir/rule] Error 2
gmake: *** [Makefile:150: cf_space] Error 2
[退出码 2]
```


退出码 2，三层 `gmake` 转述加一行 `Deleting file`（make 把失败的产物删掉了）。
同一批参数用列表属性给就正常：`cf_list` 跑到 `[100%] Built target cf_list`，退出码 0。

★ 两处差别性质完全不同：`COMPILE_FLAGS` 是**字符串属性**，赋值语义（覆盖）且不给你列表该有的引号保护；
`COMPILE_OPTIONS` 是**列表属性**，追加语义、逐项处理。迁移时一律用
`target_compile_options(... PRIVATE ...)`，并把需要空格的参数**当成一个列表元素**写。
第 26 节接着看它另一条更要紧的差别。

### 第 26 节 `COMPILE_FLAGS` 最贵的一条：生成器表达式原样漏进 Makefile，被 make 自己展开

第三对 target 各带一个 `$<CONFIG>`，一个走 `COMPILE_FLAGS`、一个走 `COMPILE_OPTIONS`：

```
$ for t in cf_genex cf_list_genex; do printf '%-13s ' $t; grep -m1 '^C_FLAGS' /tmp/cmake07-AqiaHO/b/cf/CMakeFiles/$t.dir/flags.make | sed 's|^C_FLAGS = ||'; done
cf_genex      -DCF_CFG=$<CONFIG>
cf_list_genex -DCF_CFG=
[退出码 0]
```


左边那行是**字面量** `$<CONFIG>`，右边那行已经变成空（本机没设 `CMAKE_BUILD_TYPE`，求值结果是空串）。
也就是说 CMake 对字符串属性里的生成器表达式**不做求值**，它把原文照抄进了 `flags.make`，
真正吃下这行的是 make。`cf_genex` 构建失败（退出码 2，屏幕上只有 make 的三行转述，看不见原因）。
把 `--verbose` 打开，只把那条 `-D` 抠出来：

```
$ cmake --build /tmp/cmake07-AqiaHO/b/cf --target cf_genex --verbose 2>&1 | grep -oE -- '-DCF_CFG=[^ ]*'
-DCF_CFG=CMakeFiles/cf_genex.dir/compiler_depend.tsCONFIG>
[退出码 2]
```


`-DCF_CFG=CMakeFiles/cf_genex.dir/compiler_depend.tsCONFIG>`。`$<` 在 make 语法里是
**第一条前置依赖**，于是 make 把它展开成 `CMakeFiles/cf_genex.dir/compiler_depend.ts`，
后面剩下的 `CONFIG>` 原样接上去。这一行后面挂的 `[退出码 2]` 是 `set -o pipefail` 的功劳 ——
它让失败的构建成为整条管道的结论，否则这里会显示 `grep` 的成功。

★ 这一行是**本章最硬的一张证据**，它同时说明两件事：① 字符串属性不展开生成器表达式，
`$<...>` 会以原文进入 Makefile，然后被另一套语言（make）解释成一个完全不同的东西；
② 这种事故在 `CMakeLists.txt` 里**长得完全正常**，只有 `flags.make` 和 `--verbose` 能看见。
`target_compile_options` 那版是 CMake 求值后的结果（`-DCF_CFG=`，`cf_list_genex` 构建退出码 0），那才是你要的语义。

---

## 第 6 部分 · 完整迁移走查：一步一验，和"一次改太多"的反面（第 27~30 节）

六种老写法各有一把尺子了。这一部分把它们串成**可以照着走的流程**：每步只做一件事、
只答一个问题，第 29 节再故意演示"一次改太多"的下场。文末「对照表」是这条流程的压缩版。

### 第 27 节 步骤 0：抓基线，以及一定绕不过的那条版本行

整套流程的地基是"改之前先有东西可比"。把 `b/legacyall` 全部 target 的 `flags.make` / `link.txt`
钉进一个文件（`snap.sh` 也是脚本写进沙箱的），再把四个程序的输出钉进另一个：

```
$ mkdir -p /tmp/cmake07-AqiaHO/baseline && /tmp/cmake07-AqiaHO/snap.sh /tmp/cmake07-AqiaHO/b/legacyall /tmp/cmake07-AqiaHO/baseline/cmdline.txt
106 /tmp/cmake07-AqiaHO/baseline/cmdline.txt
[退出码 0]
```


`106` 行，里面 16 个 `== ` 段（每个 target 一段）、8 个 `link.txt`（八条链接命令行）。
配套那份"改之前的程序输出"是后面每一步 diff 的左边那份，它有三个特征要记住：
`[greet]` 段有一行被迫的 trace、`[notepad]` 段也有一行、最后一行是 `[fooprog] foo_tag() = 1`：

```
$ { /tmp/cmake07-AqiaHO/b/legacyall/app; echo ---; /tmp/cmake07-AqiaHO/b/legacyall/greet; echo ---; /tmp/cmake07-AqiaHO/b/legacyall/tp/notepad; echo ---; /tmp/cmake07-AqiaHO/b/legacyall/fooprog; } > /tmp/cmake07-AqiaHO/baseline/output.txt && cat /tmp/cmake07-AqiaHO/baseline/output.txt
[app] trace: 进 main 了，参数准备给 2 和 3
[util] 编译器给我的 CH07_STAMP = 7
[calc] 走的是 PLUS 分支
[app] calc_add(2,3) = 5
---
[greet] 注意：有人给全工程打开了 CH07_DEBUG_TRACE，我也被打了 trace
[util] 编译器给我的 CH07_STAMP = 7
[greet] 你好，我只是来问个好
---
[notepad] 有人给全工程打开了 trace，我被迫打印这行
[notepad] 我只是想记一行笔记，第 1 行
---
[fooprog] foo_tag() = 1
[退出码 0]
```


然后是老仓库第 0 步一定撞上的那堵墙。靶子 `variants/min-version` 第一行是
`cmake_minimum_required(VERSION 2.8.12)` —— 就这一行，配置直接死：

```
$ cmake -S /tmp/cmake07-AqiaHO/ch07/variants/min-version -B /tmp/cmake07-AqiaHO/b/minver 2>&1 | tail -10
  Compatibility with CMake < 3.5 has been removed from CMake.

  Update the VERSION argument <min> value.  Or, use the <min>...<max> syntax
  to tell CMake that the project requires at least <min> but has been updated
  to work with policies introduced by <max> or earlier.

  Or, add -DCMAKE_POLICY_VERSION_MINIMUM=3.5 to try configuring anyway.


-- Configuring incomplete, errors occurred!
[退出码 1]
```


`Compatibility with CMake < 3.5 has been removed from CMake.` 退出码 1。
报错自己给了三条出路：改 `<min>`、用 `<min>...<max>` 语法、或者加
`-DCMAKE_POLICY_VERSION_MINIMUM=3.5` "试着照样配置"。

★ 这一步是整套配方的地基：**先把改写前的命令行存盘。** 没有 `baseline/cmdline.txt`，
后面五步的每个 diff 都没有左边那份；没有 `baseline/output.txt`，第 15 节那种"屏幕全绿、行为变了"
根本不会被发现。而老仓库第一件事就撞版本行：CMake 4.x 移除了对 `< 3.5` 的策略兼容，这是**硬错误**。
第三条那个 `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` 是很多人 CI 里的第一反应，本章的态度是
**别把它当成"改完了"**：它只让配置过关，那行策略语义仍然停在 2.8 时代（改完之后重跑配置，本章没做，见文末清单）。

### 第 28 节 步骤 1~5：一次只关一个开关，每步一条 diff

`ch07/CMakeLists.txt` 里那七个 `CH07_LEG_*` 开关，就是把六种老写法各做成一个可以拨的分支。
**每步只把一个开关拨回 `OFF`**，所以每步的 diff 只解释一件事。

步骤 1 = `link_directories` → `.a` 绝对路径（注意这条配置命令里没有 `CH07_LEG_LINK`，其余五个仍是 `ON`）：

```
$ cmake -S /tmp/cmake07-AqiaHO/ch07 -B /tmp/cmake07-AqiaHO/b/s1 -DCH07_LEG_INCLUDE=ON -DCH07_LEG_DEFS=ON -DCH07_LEG_GLOB=ON -DCH07_LEG_FLAGS=ON -DCH07_LEG_PLAIN=ON -DCH07_LEG_CACHEVAR=ON -DCH07_FOO_DIR1=/tmp/cmake07-AqiaHO/libs/v1 -DCH07_FOO_DIR2=/tmp/cmake07-AqiaHO/libs/v2 -DCH07_STAMP=7 -DCH07_TRACE=ON >/dev/null && cmake --build /tmp/cmake07-AqiaHO/b/s1 -j4 >/dev/null && echo 步骤1_构建成功
步骤1_构建成功
[退出码 0]
```


```
$ diff <(/tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/legacyall/CMakeFiles/fooprog.dir/link.txt -L) <(/tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/s1/CMakeFiles/fooprog.dir/link.txt -L); echo "[步骤1 的 -L diff 退出码 $?]"
1,2d0
< -L/tmp/cmake07-AqiaHO/libs/v1
< -L/tmp/cmake07-AqiaHO/libs/v2
[步骤1 的 -L diff 退出码 1]
[退出码 0]
```


两条验收：`-L` 序列 diff 退出码 1、少的正是那两行（`1,2d0`）；四个程序串起来对基线
`baseline/output.txt` 跑 diff，**退出码 0**（一行差异都没有）；
`grep -oE '[^ ]*libfoo.a'` 在 `b/s1/CMakeFiles/fooprog.dir/link.txt` 里抓到
`<沙箱>/libs/v1/libfoo.a` —— **从这一步起 `foo_tag()` 是 1 还是 2 由文件里的一行决定**（第 9 节）。
另外四个 target（`app`/`greet`/`calc`/`util`）的 `-I` 序列 diff 全部打印"命令行一致"。

步骤 2 = `include_directories` → `target_include_directories`：

```
$ for t in app greet calc util fooprog leaf_a leaf_b; do printf '%-8s ' $t; if diff <(/tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/s1/CMakeFiles/$t.dir/flags.make -I) <(/tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/s2/CMakeFiles/$t.dir/flags.make -I) >/dev/null; then echo 一致; else echo 有差别; fi; done
app      一致
greet    一致
calc     一致
util     一致
fooprog  一致
leaf_a   有差别
leaf_b   有差别
[退出码 0]
```


`app greet calc util fooprog` 五行"一致"，`leaf_a leaf_b` 两行"有差别"，
第三方 `notepad` 单独再比一次也是有差别。这三行是**预期会差**的（它们本来就不该有那条 `-I`，第 4、5 节）。
这一步之后程序输出对基线 diff 退出码 0 —— 少了 `-I` 却没改变行为，因为那三个文件不引任何本工程头。

步骤 3 = `add_definitions` → `target_compile_definitions`。先看这一步之前（`b/s2`）的八行 `C_DEFINES`：

```
$ /tmp/cmake07-AqiaHO/dump.sh /tmp/cmake07-AqiaHO/b/s3 C_DEFINES
app       -DCH07_DEBUG_TRACE=1
calc      -DCH07_MODE_PLUS=1
fooprog   
greet     
leaf_a    
leaf_b    
util      -DCH07_STAMP=7
notepad   
[退出码 0]
```


八行变成"各拿各的"了。这是本章唯一一个**行为一定会变**的步骤：

```
$ { /tmp/cmake07-AqiaHO/b/s3/app; echo ---; /tmp/cmake07-AqiaHO/b/s3/greet; echo ---; /tmp/cmake07-AqiaHO/b/s3/tp/notepad; echo ---; /tmp/cmake07-AqiaHO/b/s3/fooprog; } | diff /tmp/cmake07-AqiaHO/baseline/output.txt -; echo "[步骤3 程序输出 diff 退出码 $?]"
6d5
< [greet] 注意：有人给全工程打开了 CH07_DEBUG_TRACE，我也被打了 trace
10d8
< [notepad] 有人给全工程打开了 trace，我被迫打印这行
[步骤3 程序输出 diff 退出码 1]
[退出码 0]
```


diff 不为 0，而且**只少两行**：`6d5` 是 `[greet]` 那行 trace，`10d8` 是 `[notepad]` 那行 trace。
这正是第 15 节那条判据的落地：不为 0 是预期的，但必须能**逐行说出少掉的是哪两行**。

步骤 4 = `file(GLOB)` → 写死清单。GLOB 不改任何已有 target 的命令行，它改的是"会不会悄悄少一个"，
所以这一步的验收是清单和用例名单。配置命令里 `-DCH07_LEG_GLOB` 已经不出现了，父目录回显的是
**写死的那份清单**：`leaf 清单里有 <沙箱>/ch07/src/leaf/leaf_a.c;/tmp/.../leaf_b.c 这几个文件`。
构建后 `ctest -N` 给 `Total Tests: 2`，和步骤 3 的 `b/s3` 完全一致 —— 名单没变，这一步才算过。

步骤 5 = `COMPILE_FLAGS` / 无关键字 / 全局变量清单 三处一起收：

```
$ for t in calc app greet; do printf '%-6s 老: ' $t; grep -m1 '^C_FLAGS' /tmp/cmake07-AqiaHO/b/legacyall/CMakeFiles/$t.dir/flags.make | sed 's|^C_FLAGS = ||'; printf '%-6s 新: ' $t; grep -m1 '^C_FLAGS' /tmp/cmake07-AqiaHO/b/s5/CMakeFiles/$t.dir/flags.make | sed 's|^C_FLAGS = ||'; done
calc   老: -Wall -Wextra -DLEG_FLAGS=1
calc   新: -Wall -Wextra
app    老: 
app    新: 
greet  老: 
greet  新: 
[退出码 0]
```


```
$ diff <(/tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/s5/CMakeFiles/calc.dir/flags.make -I; /tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/s5/CMakeFiles/calc.dir/flags.make -D) <(/tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/base/CMakeFiles/calc.dir/flags.make -I; /tmp/cmake07-AqiaHO/seq.sh /tmp/cmake07-AqiaHO/b/base/CMakeFiles/calc.dir/flags.make -D); echo "[步骤5 之后 vs 第 02 节现代基线 diff 退出码 $?]"
[步骤5 之后 vs 第 02 节现代基线 diff 退出码 0]
[退出码 0]
```


`calc 老: -Wall -Wextra -DLEG_FLAGS=1` vs `calc 新: -Wall -Wextra` —— 唯一要记账的是老版多带的
`-DLEG_FLAGS=1`（要么保留，要么单独写一行"它为什么可以删"）；`app`、`greet` 两对逐行相同。
无关键字和全局清单那两处（`app`、`greet` 的 `link.txt` 各归一化 diff 一次）**退出码都是 0** ——
这正是第 23、24 节说的"尺子抓不到"，那两处只能靠属性打印和"改到一半会硬报错"验收。

★ 最后一条 diff 是全章的收口：`b/s5` 的 `calc` 和 `b/base`（第 2 节那份纯现代基线）的 `calc`，
`-I` 加 `-D` 两条序列合起来 diff **退出码 0**，围栏里除了命令和那行 `[退出码 0]` 什么都没有。
**五个步骤走完，老写法树和现代树在同一个 target 上的命令行完全重合了。**

### 第 29 节 反面：一次改太多，编译照过、测试照绿，只有命令行说了真话

现在演"一个人图省事"的版本：一步到位改成现代写法，同时**顺手**把库路径挑成 `v2`、
把 `MODE` 统一成 `DOUBLE`。配置和构建都干净（连起来跑，屏幕只回一行 `改完之后_配置0_构建0`，退出码 0），
测试也干净（`100% tests passed, 0 tests failed out of 2`，退出码 0）。看输出对基线的 diff：

```
$ { /tmp/cmake07-AqiaHO/b/after/app; echo ---; /tmp/cmake07-AqiaHO/b/after/greet; echo ---; /tmp/cmake07-AqiaHO/b/after/tp/notepad; echo ---; /tmp/cmake07-AqiaHO/b/after/fooprog; } | diff /tmp/cmake07-AqiaHO/baseline/output.txt -; echo "[程序输出 diff 退出码 $?]"
3,4c3,4
< [calc] 走的是 PLUS 分支
< [app] calc_add(2,3) = 5
---
> [calc] 走的是 DOUBLE 分支
> [app] calc_add(2,3) = 10
6d5
< [greet] 注意：有人给全工程打开了 CH07_DEBUG_TRACE，我也被打了 trace
10d8
< [notepad] 有人给全工程打开了 trace，我被迫打印这行
13c11
< [fooprog] foo_tag() = 1
---
> [fooprog] foo_tag() = 2
[程序输出 diff 退出码 1]
[退出码 0]
```


三处不同：`[calc]` 走 DOUBLE 分支、`calc_add(2,3) = 10`；`[fooprog] foo_tag() = 2`；外加少掉的两行 trace。
前两处是这次**不该有**的变化。抓它们靠的两条：`calc` 的 `-D` 序列老的是
`-DCH07_DEBUG_TRACE=1 -DCH07_MODE_PLUS=1 -DCH07_STAMP=7`、新的是 `-DCH07_MODE_DOUBLE=1`；
`fooprog` 的 `link.txt` 里那个唯一路径变成 `<沙箱>/libs/v2/libfoo.a`。再看在把两处改回去之后：

```
$ cmake -S /tmp/cmake07-AqiaHO/ch07 -B /tmp/cmake07-AqiaHO/b/fixed -DCH07_FOO_PICK=1 -DCH07_MODE=PLUS -DCH07_STAMP=7 -DCH07_TRACE=ON -DCH07_FOO_DIR1=/tmp/cmake07-AqiaHO/libs/v1 -DCH07_FOO_DIR2=/tmp/cmake07-AqiaHO/libs/v2 >/dev/null && cmake --build /tmp/cmake07-AqiaHO/b/fixed -j4 >/dev/null && { /tmp/cmake07-AqiaHO/b/fixed/app; echo ---; /tmp/cmake07-AqiaHO/b/fixed/greet; echo ---; /tmp/cmake07-AqiaHO/b/fixed/tp/notepad; echo ---; /tmp/cmake07-AqiaHO/b/fixed/fooprog; } | diff /tmp/cmake07-AqiaHO/baseline/output.txt -; echo "[把两个顺手改回去之后：输出 diff 退出码 $?]"
6d5
< [greet] 注意：有人给全工程打开了 CH07_DEBUG_TRACE，我也被打了 trace
10d8
< [notepad] 有人给全工程打开了 trace，我被迫打印这行
[把两个顺手改回去之后：输出 diff 退出码 1]
[退出码 0]
```


★ 两处"顺手"各被抓到一次，用的都是本章的尺子：`-DCH07_MODE_DOUBLE=1` 出现在 `calc` 的 `-D` 序列里
（第 13 节那张表的读法），`libs/v2/libfoo.a` 出现在 `fooprog` 的 `link.txt` 里（第 11 节那条"决策位置"）。
第二块围栏是**把两个顺手改回去之后**（`-DCH07_FOO_PICK=1 -DCH07_MODE=PLUS`）的 diff：它**仍然不为 0**，
少的正是 `greet` / `notepad` 那两行 trace —— 那是步骤 3 的**预期变化**。
能被逐行说出原因的差异才叫差异，说不出来的就是事故 —— 这就是"迁移必须一次一步"的全部理由。

### 第 30 节 本章之后：一页配方，和"一个产物都没留下"的自证

本章的副作用全部落在 `mktemp -d` 的沙箱里，章节目录只留源码和 `CMakeLists.txt`。自证一条：

```
$ 〔在 07-legacy-migration 里〕 find . -name CMakeCache.txt -o -name '*.o' -o -name '*.a' -o -name flags.make -o -name link.txt -o -name leaf_c.c -o -type d -name out | wc -l
0
[退出码 0]
```


`0`。`find` 的判据是本章会产出的全部东西：`CMakeCache.txt`、`*.o`、`*.a`、`flags.make`、`link.txt`、
`leaf_c.c`（demo 在沙箱里造的那个新文件）、以及任何 `out` 目录，一起数。
章节目录里剩下的只有 `demo.sh`、`README.md` 和 `legacy/ modern/ variants/` 三堆"素材"
（每个目录里只有 `CMakeLists.txt` 和 `.c`）。沙箱本体在 demo 结束时删掉，它生前是 13M，全是构建树。

★ 这一章的配方压缩成一句：**把六种老写法各做成一个能拨的开关，一次只关一个，
每步只用一条 diff 回答一个问题，并且把"预期会差的那几行"提前写下来。**
流程表和它对应的验收命令在下面的「对照表」；那条流程走不通的地方（导出、多配置生成器、
`FetchContent` 的迁移）都在「本章没演 / 没验证」里，别照着抄。

---

## 对照表：老写法 → 新写法 → 不迁的代价 → 本机证据

左边那一列每一条都**能跑**，这才是它们活十年的原因。"不迁的代价"写的是本章真跑出来的那个后果，
不是"不够现代"。

| 老写法 | 新写法 | 不迁的代价（本机实测的那件事） | 证据 |
|---|---|---|---|
| `include_directories(d)` | `target_include_directories(t PUBLIC/PRIVATE d)` | 一个目录里所有 target 全吃到：`leaf_a` `leaf_b` `notepad` 各多一条 `-I`，它们一个本工程头都不引 | 4、5、6 |
| `include_directories(BEFORE a b)` | 通常**直接消失**（只给需要的那个 target 一条） | 同名头赢家换人：`-I` 从 `alt,std` 变 `std,alt`，屏幕从"alt 赢了"变"std 赢了"，两边全绿 | 7 |
| `link_directories(d1 d2)` + `-lfoo` | `add_library(X STATIC IMPORTED)` + `IMPORTED_LOCATION` 给 `.a` 绝对路径 | 两份同名库谁赢**由 `-L` 先后决定**：程序一个打 1、一个打 2，源码一字未改 | 8、9 |
| `link_directories` 写在 `add_executable` 之后 | 同上（不存在时机问题） | **0 个 `-L`**：配置干净退出，只有链接报 `collect2: error: ld returned 1` | 10 |
| `add_definitions(-DX=1)` | `target_compile_definitions(t PRIVATE X=1)` | 全工程点亮别人的 `#ifdef`：`greet` / `notepad` 各多打一行 trace，构建与 ctest 全绿 | 13、15 |
| `file(GLOB src/*.c)` | 写死源文件清单 | 新增 `.c` **静默不参与**：构建 `[100%]`、`ctest` 100%、`grep -c leaf_c Makefile2` = 0 | 17、18 |
| `file(GLOB ... CONFIGURE_DEPENDS)` | 仍优先写死清单；要用就先验生成器 | 本机（Unix Makefiles + 4.2.3）**会自动重跑 configure**；但删文件后构建目录里留一具旧产物尸体 | 20、21 |
| `target_link_libraries(t a b)`（无关键字） | `target_link_libraries(t PRIVATE a b)` | **命令行 diff 抓不到**：与 `PRIVATE` 版 `link.txt` 逐字符相同、零警告；差别只在 `$<LINK_ONLY:>` | 23 |
| 同一次调用混用裸名 + 关键字 | —— | 本机**硬错误**（`The INTERFACE, PUBLIC or PRIVATE option must appear as the second argument`）—— 改到一半会停住 | 23 |
| `set(DEPS a b)` / `set(DEPS a b CACHE ...)` 传清单 | 一个 `add_library(deps INTERFACE)` | CACHE 版能被 `-DDEPS=nope` 改写且**配置照样成功**，`link.txt` 里安静多一个 `-lnope` | 24 |
| `set_target_properties(t PROPERTIES COMPILE_FLAGS "...")` | `target_compile_options(t PRIVATE ...)` | ① 属性**覆盖**、命令**追加**；② 带空格参数被重新分词（`cf_space` 编不过）；③ `$<CONFIG>` **不展开**，漏进 Makefile 被 make 拼成 `compiler_depend.tsCONFIG>` | 25、26 |
| `cmake_minimum_required(VERSION 2.8.12)` | 抬 `<min>`，或写 `<min>...<max>` | CMake 4.x 上是**硬错误**、配置直接失败（三条出路见第 27 节表） | 27 |
| 一次改完所有上面这些 | 一步一开关 | "预期变化"和"顺手改的"混在同一个 diff 里分不开：本章那两处顺手让 `calc_add` 从 5 变 10、`foo_tag` 从 1 变 2，而配置 0 / 构建 0 / 测试 100% | 29 |

还有两条**不是改写、而是读法**的对照：

| 想搞清楚的事 | 别看这个 | 要看这个 |
|---|---|---|
| 这个 target 吃到哪些头 / 宏 / 标志 | `CMakeLists.txt` 里写了什么 | `<t>.dir/flags.make` 的 `C_INCLUDES` / `C_DEFINES` / `C_FLAGS` |
| 链的是哪一份库 | `target_link_libraries` 里那个名字 | `<t>.dir/link.txt` 里那个**绝对路径** |
| 这个 `.c` 到底参没参与构建 | 磁盘上有没有这个文件 | `<t>.dir/build.make` + `ctest -N` 的名单条数 |
| 宏到底进没进编译 | CMake 的任何说法 | `gcc -dM -E <flags> file.c`（第 14 节） |

---

## 本章的坑清单（按代价从大到小）

排在前面的都是**屏幕上没有字**的；会响的都在后面。

1. **构建全绿 + 测试全绿，程序行为却变了。** `add_definitions` 把别人的 `#ifdef` 点亮，
   `greet` / `notepad` 各多打一行；`ctest` 还是 `100% tests passed`，因为那俩根本不在名单里（第 15 节）。
2. **两份同名库，链到哪一份由 `-L` 先后决定**，而这个决定**不存在于任何被版本控制的文件里**。
   换机器 / 换 runner 就换行为，日志全绿（第 9 节）。
3. **`file(GLOB)` 新增文件静默漏编**：`[100%] Built target`、`grep -c` 命中 0、`ctest` 少一条、退出码 0（第 18 节）。
   代价是"少一个 target"这种**根本不在账本上**的缺失。
4. **同名头文件的赢家由 `-I` 顺序决定**，`BEFORE` 一改就换人；条数和集合都不变，
   所以**只数条数的验收会漏诊**（第 7 节）。
5. **`link_directories` 往文件后面挪一行 = 完全失效**，而配置照报 `Configuring done` ——
   "顺手整理顺序"就能造成这件事（第 10 节）。
6. **一次改太多 → diff 里"预期变化"和"顺手改的"混在一起**，从此无法归因（第 29 节两处顺手）。
7. **命令行 diff 有盲区**：无关键字签名和 `PRIVATE` 版的 `link.txt` **逐字符相同**、本机零警告（第 23 节）。
   这一类只能看属性 + 靠"改到一半会硬报错"。全局变量清单 → INTERFACE 库同样 diff 为 0（第 24 节）。
8. **CACHE 变量当依赖清单，`-D` 就能改**：本章改成 `-lnope` 之后配置**照样成功**（第 24 节）。
   拼错的清单值不进报错，只进命令行。
9. **`COMPILE_FLAGS` 里的生成器表达式不展开**，字面量漏进 Makefile 后被 `$<`（第一个依赖）拼走，
   报出一个你从未见过的宏名（第 26 节）。
10. **`CONFIGURE_DEPENDS` 的名单对、目录里有尸体**：删文件后用例回到 2 条，
    构建目录里那个旧可执行文件还在（3 个可执行 / 2 条用例）。**别用"目录里有啥"当验收**（第 21 节）。
11. **`include_directories` 污染会走进 `add_subdirectory` 的第三方目录**，
    把别人源码里那些"我这辈子不会定义"的宏打开（第 5 节）。
12. **`COMPILE_FLAGS` 是覆盖不是追加**：第二次 `set_target_properties` 把第一次整个换掉，
    `C_FLAGS` 里只剩 `-DCF_B=2`（第 25 节）。
13. **带空格的 flag 字符串会被重新分词**：`-DCF_MSG=a b` 变成两个参数，
    那个裸的 `b` 被当成**第二个源文件**，所以报"找不到文件"而不是"宏不对"（第 25 节）—— 报错位置离病灶隔一层。
14. **属性没设过时 `get_target_property` 返回 `<PROP>-NOTFOUND`**，而同一时刻最终命令行里可能有 `-I`。
    只看属性会得出完全相反的结论（第 3 节）。
15. **`link_directories` 顺手多送一条 `-Wl,-rpath,...`**；迁到新写法时它一起消失，
    如果有人运行时依赖它，症状要到"跑起来找不到 .so"才出现（第 8 节）。
16. **同一目录里 target 创建先后不影响 `include_directories` / `add_definitions`** ——
    很多人以为"写在后面就吃不到"，实测 `early` 那条照样有 `-I`（第 3 节）。想反了会去"修"一个不存在的 bug。
17. **`ctest -N` 的 `Total Tests: N` 要用条数对**，不要看 `-N` 的输出行数 —— `tail -3` 会把它剪成
    "一条用例 + 空行 + Total"（第 17、18 节）。
18. **`grep -c` / `grep -l` 报 0 时退出码是 1**，这是"确认不存在"的正确形状，不是失败。
    `set -o pipefail` 下管道的退出码报的是**真实成败**（第 10、12、18 节）。
19. **`-j4` 下 `Built target` 的先后和百分比都不稳定**（同一份 demo 连跑几次顺序就变过）。
    **别拿输出行的顺序当证据**，只拿"在不在"和"行里的字符"当证据。
20. **`cmake_minimum_required` 抬版本不是改一个数**：它同时换整套策略默认值，
    抬完要把全部验收重跑一遍（第 27 节）。
21. **`-DCMAKE_POLICY_VERSION_MINIMUM=3.5` 不是"改完了"**：它一行源码都没改，
    只是把版本兼容问题搬到命令行里。要用就单独记账（第 27 节）。
22. **`CONFIGURE_DEPENDS` 会不会自动重跑是生成器相关的**。本章只担保 Unix Makefiles + CMake 4.2.3
    这一条（第 20 节实测）。换生成器要**重新验**，别照抄本章结论。

---

## 自检题（能答上就算这章读完了）

**1. 有人说"`include_directories` 必须写在 `add_executable` 前面，不然吃不到"。这句话对不对？**
对 `include_directories` / `add_definitions` **不对**：它们是目录属性、生成期才读，
同一目录里的 target 不分先后全都吃到（第 3 节实测：`early` 在那条指令之前就创建了，
属性读出来是 `[P1-NOTFOUND]`，而 `flags.make` 里有 `-I`）。
对 `link_directories` **对**：它是 target 创建那一刻的快照，写在后面一个 `-L` 都不给（第 10 节，链接期才响）。
所以这一句话把三种不同机制说成了一种。

**2. 给你一份能跑的老 `CMakeLists.txt`，你要怎么知道自己改写时没改坏？**
先抓基线：把 `flags.make` + `link.txt` + **程序输出**三份存盘（第 27 节 `snap.sh`）。
然后一次只改一处，改完对**指定的那本账**：`-I` 要求需要的 target diff 为 0、
`-L` 要求路径唯一且程序输出一致、`-D` 要求需要的宏同值、GLOB 要求用例条数一致。
"能跑"不是证据 —— 本章所有事故都在能跑的前提下发生。

**3. 工程里有 `a/libfoo.h` 和 `vendor/libfoo.h`，你怎么确认真用了哪一份？只看 `CMakeLists.txt` 够吗？**
不够，两个目录都写了、都合法。两条路：
① 把 `-I` 抽成一行一个看顺序（`seq.sh ... -I`），第 7 节实测 `BEFORE` 一改赢家就换人；
② 让程序自己说：本章用两份同名头里各放一个 `#define CH07_WHO`，屏幕上直接打出赢家。

**4. `link_directories(dirA dirB)` 和 `link_directories(dirB dirA)` 在 `link.txt` 上差在哪？怎么验收这两种写法迁移成功？**
只差 `-L` 的先后（第 9 节 diff 形状 `1d0` + `2a2`），但屏幕上是两个不同数字（1 / 2）。
验收不能要求 diff 为 0（机制换了必然变），要换成三条：库路径**唯一确定的绝对路径**、
**程序输出一字不差**、且没有别的 target 还依赖那个 `-L`（`grep -l -- -L */link.txt | wc -l` → 0）。

**5. 为什么"加了 `.c` 但没参与编译"这种事 `ctest` 全绿发现不了？你用什么三条证据查？**
因为少了一个 target 就是少一个 target，名单只数它有的。第 18 节那三条：
① `grep -c leaf_c <b>/CMakeFiles/Makefile2` → 0（+ 退出码 1）；
② `find <b> -name 'leaf_c*'` → 0 个产物；③ `ctest -N` 的 `Total Tests:` 条数。
**三条一起看才叫证据**，任何一条单独看都会骗人。

**6. `CONFIGURE_DEPENDS` 在本机到底做了什么？它没做什么？**
做的：`cmake --build` 时自己重跑 configure —— 屏幕上 `-- GLOB mismatch!` +
`The following files were added:` 然后重新 Generating，加文件被编译进名单（第 20 节），删文件同样会报
（`were removed`，第 21 节）。
没做的：**不碰磁盘** —— 构建目录里那个旧的 `leaf_c` 可执行文件还在（3 个可执行 / 2 条用例）。
而且这套行为**和生成器有关**，本机只担保 Unix Makefiles + CMake 4.2.3。

**7. 你怎么用一条命令证明"这个宏确实进了编译"，而不经过 CMake 的任何说法？**
`gcc -dM -E $(grep '^C_DEFINES' .../flags.make | sed 's/^C_DEFINES = //') ... file.c | grep CH07` ——
把 `flags.make` 里的 `C_DEFINES` 原样喂回 gcc，让预处理器报出宏的最终清单（第 14 节）。
注意读输出要认 include guard（`CH07_UTIL_H` 那种是头文件被引过的痕迹，不是谁给的宏）。

**8. 无关键字 `target_link_libraries(t a b)` 在 CMake 4.2.3 上会警告吗？`link.txt` 能 diff 出差别吗？那靠什么查？**
都不：本机**一条警告都不说**（`grep -ciE 'warning|deprecat'` → 0 + 退出码 1），
而且和 `PRIVATE` 版的 `link.txt` **逐字符相同**（diff 退出码 0）。第 23 节。
只能看属性：`INTERFACE_LINK_LIBRARIES` 里只有 `PRIVATE` 版包着 `$<LINK_ONLY:util>`，
无关键字和 `PUBLIC` 都是裸名字。改到一半混用两种签名时本机给**硬错误**（退出码 1）。

**9. 为什么迁移 `COMPILE_FLAGS` 时必须真跑一次构建，光读 `flags.make` 不够？**
两种情况光读文件看不全：① 带空格的参数在 `flags.make` 里只是"少了那对引号"，
要到编译器把它当成两个参数（那个裸 `b` 被当第二个源文件）才炸出来（第 25 节）；
② `$<CONFIG>` 在 `flags.make` 里只看得见一个字面量，
它被 **make** 解释成"第一个依赖"是构建时的事 —— `--verbose` 里那句
`-DCF_CFG=CMakeFiles/cf_genex.dir/compiler_depend.tsCONFIG>` 才是真相（第 26 节）。

**10. 一次改完五种老写法，配置 0、构建 0、ctest 100%，diff 里出现五行变化。这正常吗？**
不正常，而且**没法判断正不正常** —— 因为"预期变化"和"顺手改的"混在同一个 diff 里。
本章那处反例（第 29 节）里五行有两处是顺手造成的：`calc_add` 从 5 变 10、`foo_tag` 从 1 变 2。
正确做法是一次一步（第 28 节：每步 diff 只有唯一解释），
并且**能逐行说出每一处差异为什么在那里** —— 说不出来的就是事故。

---

## 本章没演 / 没验证（请当它们不存在，别照抄）

手册最容易骗人的地方是"顺手多写一句看起来合理的结论"。下面这些我**没有**在本机跑出来：

* **`CONFIGURE_DEPENDS` 在其他生成器上的行为**：本机只有 `Unix Makefiles`（**这台机器没装 ninja**，
  `-GNinja` 在本章只能当"试不了"的证据）。Xcode / Visual Studio 等**多配置**生成器一个都没验。
* **`CONFIGURE_DEPENDS` 在真实大工程上的代价**：第 21 节那两次计时（这一遍 `0.072s` vs `0.062s`，
  上一遍 `0.082s` vs `0.081s`）是两文件小工程，差值本身不如抖动大，量不出有意义的数。
  本章只担保它**不为零**，不担保"能忽略"。
* **GLOB 的两种合法用法**（生成型源文件、非编译类资源清单）：本章一概没碰，
  只演了"源码清单用 GLOB"这一种。
* **`-rpath` 的实际运行时后果**：第 8 节只证明了 `link_directories` 会顺手带出
  `-Wl,-rpath,...` 这一段。删掉它会不会让某个程序找不到 `.so` —— **没有构造这个场景**（RPATH 是第 03 章）。
* **`install(EXPORT)` / `find_package` 下无关键字签名的那个报错**：第 23 节只说"要到导出那天才炸"，
  **没有真的导出**。那个报错原文在第 03 章。
* **`target_link_libraries` 的链接顺序语义**（`--start-group`、循环依赖、`.a` 的先后导致
  `undefined reference`）：本章的 `link.txt` 差异全在"选哪份库"和"关键字落到哪个属性"上，
  顺序本身没演。
* **`add_compile_options` / `add_compile_definitions` / `add_link_options`** 这三个
  "现代但仍是目录级"的命令：本章只对照了 target 级，**没有**演它们和 `include_directories`
  在作用域上的异同。
* **`CMP00xx` 策略变量**（`CMP0079`、`CMP0022`、`CMP0074` 等）对迁移的影响：
  第 27 节只报了 `cmake_minimum_required` 的硬错误原文，抬版本之后哪些策略会变、
  变完命令行差在哪 —— **一个都没 diff**。
* **`cmake -P` 脚本模式、`CMAKE_POLICY_VERSION_MINIMUM` 真加上去能不能配置通过**：
  第 27 节只是**读到**报错里那句话，没跑过第三条出路。
* **多目录真第三方**（`FetchContent` / `find_package` 拉真库）：本章的"第三方"只有一个
  手写 `tp/CMakeLists.txt` + 一个 `notepad.c`。真实第三方 target 多、属性多，污染路径比本章复杂得多。
* **本章没有跑过任何 sanitizer / preset / install**；没有动仓库根的 `CMakeLists.txt`、`Makefile`
  和任何 `build*/`（第 22 节只 `sed -n` / `grep` 读那三行，第 30 节 `find` 自证章节目录 0 产物）。
* **"老写法在 CI 上换机器就换行为"这个推断**：第 9 节在本机用两个 `-DFOO_A/-DFOO_B` 目录顺序
  造出了两个不同二进制，但**没有**换机器、换 runner、换镜像重跑。跨机器那一步是推演。
* **`add_definitions` 里 `$<...>` 生成器表达式的行为**：本章只测了 `COMPILE_FLAGS` 属性里不展开
  （第 26 节），`add_definitions` 那条路**没测**。
* **`link_libraries()` 这条目录级命令**：它和 `include_directories` 同属"目录作用域"，
  本章**一次都没跑过它**（`link_directories` 跑了，`link_libraries` 没有）。
  同理 `install()` / `export()` 那一类也没演。第 3 节那张三分表（目录属性 / 快照 / 变量值）里
  `link_libraries` 落在哪一格，本章**不下结论**。
* **`include_directories(BEFORE)` 的完整顺序规则**（多目录、和 `AFTER` / `SYSTEM` 混合时）：
  第 7 节只演了"同一批两个目录 + `BEFORE`"这一种形状，其余形状没测。

---

## 怎么跑这章的 demo

```bash
cd cmake-handbook/07-legacy-migration
bash demo.sh                     # 30 节全跑，跑完自动删沙箱
bash demo.sh --keep              # 保留沙箱并打印路径，进去自己接着拨开关
bash demo.sh > /tmp/c07.txt 2>&1 # 重定向时自动关颜色，拿纯文本
```

章内文件一览：

```text
07-legacy-migration/
├── CMakeLists.txt          实验室（176 行）：同时装两套写法，七个 CH07_LEG_* 开关决定用哪套
├── legacy/CMakeLists.txt   一份"典型的 2013 年文件"（67 行）：六个病灶一次全上，而且它能跑
├── modern/CMakeLists.txt   改写结果（68 行）：这才是真实工程里该交出去的写法
├── include/ch07/*.h        calc.h / util.h / foo.h（foo.h 那句"谁赢由 -L 顺序决定"是本章题眼）
├── src/                    三套工程共用的**唯一一份**源码
│   ├── app.c greet.c       CH07_DEBUG_TRACE 探针（"被迫打开的 #ifdef"）
│   ├── calc/ util/         CH07_MODE_* / CH07_STAMP 探针（宏没进来就返回 0 / 打 0）
│   ├── foo/foo.c           一份源码编成两份同名 libfoo.a（-DCH07_FOO_V=1 / =2）
│   ├── fooprog.c           全章唯一的 libfoo 消费者：链到哪份就打几
│   ├── hdrwin.c            头文件赢家探测器（两份同名 ch07/hdrwin.h）
│   └── leaf/leaf_a.c b.c   自包含小程序；第 18~21 节临时造一个 leaf_c.c 当 GLOB 靶子
├── tp/                     伪装成第三方的目录：一行 add_executable，全章的污染检测器
├── variants/               九个故意写坏的靶子工程，一个只讲一件事
│   ├── order/ (+sub_before, sub_after)   第 3 节：include_directories 到底什么时候算
│   ├── inc-order/ (std/ alt/)            第 7 节：-I 顺序 = 同名头赢家
│   ├── link-order/                       第 9、10 节：-L 顺序决定谁赢 / 它是快照
│   ├── glob/  glob-depends/              第 17~21 节：GLOB 与 CONFIGURE_DEPENDS 实测
│   ├── plain-link/                       第 23 节：无关键字签名（尺子的盲区）
│   ├── cache-var/ (+sub/)                第 24 节：全局 / CACHE 变量传清单
│   ├── comp-flags/                       第 25、26 节：COMPILE_FLAGS 三处差别
│   └── min-version/                      第 27 节：CMake 4.x 上的版本行硬错误
└── demo.sh                 本章全部证据的唯一来源（645 行、30 个 banner）
```

安全边界（这几条是硬规矩，不是客套）：

* 所有 configure / build / ctest **都在 `mktemp -d "${TMPDIR:-/tmp}/cmake07-XXXXXX"` 的沙箱里**，
  工程是本章节目录的副本；跑完 `trap cleanup EXIT` 只删它自己建的那个目录（章节目录 0 产物，第 30 节 `find` 自证）。
* 仓库根的 `CMakeLists.txt` / `Makefile` / 那 56 个 `00-*`…`11-*` 目录 / 所有 `build*/`
  一律**只读**：第 22 节只 `sed -n` 和 `grep` 读几行，没在它旁边 configure、没 `make`、没 `ctest`。
* **不联网**、不装东西、不 sudo、不写任何全局配置（`~/.cmake` 一个字节都不碰）；
  `--prefix` 之类的外部落点一个都没用，两份 `libfoo.a` 也是沙箱里现造的。
* 输出里不出现本机身份：读仓库根那份真实文件时用 `runc_at`（屏幕上只留目录名 + 相对命令），
  而且全程不打印 `make` 在哪（不 `which make`、不 `cmake -LA`）。
* 只讲 C（每个 `project(... LANGUAGES C)`），生成器一律用默认的 Unix Makefiles。

想把这一章接回你自己的工程，顺序应该是：

1. **先抓基线，别先改。** 配置一份现成的，把 `flags.make` + `link.txt` + 程序输出三份存盘（第 27 节）。
2. 把 `CMakeLists.txt` 里每一条目录级指令（`include_directories` / `link_directories` /
   `add_definitions` / `file(GLOB)` / `link_libraries`）列成一张清单，标上作用域（第 1、3、10 节）。
3. 按第 28 节那五步走，**一次一步**，每步只 diff 指定的那本账，并在 PR 里记一行结论。
4. 步骤 5 那一类（无关键字、全局变量清单）**别忘了命令行看不到**，改用属性 + "改到一半会硬报错"来验（第 23、24 节）。
5. 最后把 `CONFIGURE_DEPENDS` 这类"和生成器有关"的结论，在**你自己的生成器上**重跑一遍再采信（第 20 节）。

---

## 术语小抄

| 词 | 大白话 |
|---|---|
| 目录作用域命令 | `include_directories` / `link_directories` / `add_definitions` / `link_libraries`：给的是"这个目录里所有 target"，不看你要不要 |
| target 作用域命令 | `target_include_directories` / `target_compile_definitions` / `target_compile_options` / `target_link_libraries`：点名给，一个 target 一份 |
| `PUBLIC` / `PRIVATE` / `INTERFACE` | 这条要求给谁：只给自己 / 自己也链接它的人也要 / 只给链接它的人（自己不用） |
| `flags.make` | 一个 target 的编译参数最终落点：`C_DEFINES`（`-D`）/ `C_INCLUDES`（`-I`）/ `C_FLAGS`。本章的主证据 |
| `link.txt` | 一个 target 的链接命令本体：`-L`、库路径、`-l`、`-rpath` 都在这行 |
| `build.make` | 一个 target 到底编哪些文件、每条规则的产物在哪。GLOB 那几节盯它 |
| `<PROP>-NOTFOUND` | `get_target_property` 对"这属性没设过"的返回值。它不等于"最终命令行里没有" |
| 快照语义 | "target 创建那一刻的属性值"才算数：`link_directories` 是（第 10 节）；`include_directories` **不是**（第 3 节） |
| `-L` / `-lfoo` | `-L` 是"去这些目录找库"，`-lfoo` 是"找一个叫 foo 的库"。谁赢由 `-L` 先后决定，文件里不留记录 |
| `IMPORTED` target | 告诉 CMake"外面已经有一个库了，路径是它"。好处：链接行里出现的是**绝对路径**而不是名字 |
| `INTERFACE` 库 | 不编译任何东西、只当"一组要求的名字"用的库。全局变量清单的现代替身 |
| `file(GLOB)` | 配置期扫一次目录，把结果**当成一个普通变量的值**。磁盘后来变了没人知道 |
| `CONFIGURE_DEPENDS` | GLOB 的一个关键词：构建前重扫，不一致就自动重跑 configure。行为和生成器有关（本机：会） |
| `-dM -E` | gcc："只做预处理，把定义的所有宏列出来"。绕开 CMake 直接和编译器对质 |
| `-rpath` | 写进二进制的运行时库搜索路径。`link_directories` 会顺手给你加一条 |
| 生成器表达式 | `$<CONFIG>` 那一类，由 **CMake** 展开。放进 `COMPILE_FLAGS` 就不展开，会被 **make** 的 `$<`（第一个依赖）抢走 |
| 基线 | 改写**之前**存盘的命令行 + 程序输出。没有它，"改坏了"只是一种感觉 |
| 等价改写 | 需要的 target diff 为 0。本章判定"这步可以放行"的唯一标准 |

---

## 本章之后

| 章 | 讲什么 | 现在有没有 |
|---|---|---|
| 08 | 逐行拆解仓库根那份 249 行的 `CMakeLists.txt`：`ds_add_leaf` 那个函数、`DS_WITH_ASAN` 开关、56 个手工清单调用点 | **已交付**（`08-repo-cmake-walkthrough/`） |
| 09（暂定） | 把工程改成"能装、能被别人 `find_package` 用到"：`install` / `export` / `CONFIG.cmake` 那一整套 | 未写 |

本章和第 08 章接在同一个地方：第 22 节那句"为什么写着不写 `file(GLOB)`"，
读的就是仓库根那份文件的注释原文；第 08 章逐行拆它时，
把 56 个源文件一个一个列出来、`target_include_directories` 写成 `PRIVATE` 让叶子自包含 ——
那正是本章配方走完之后应有的形状。

本章也给第 09 章留了三件没做完的事：① 无关键字签名"导出那天才炸"的那个报错原文（第 23 节只说会炸）；
② `-rpath` 那段到底该不该留（第 8 节只证明了它被顺手加上）；
③ `PUBLIC` 那条 `-I` 怎么变成别人机器上的坑（第 4 节说的"跟着库一起交付"）。
