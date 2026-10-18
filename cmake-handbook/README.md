# CMake 大白话手册

> 一本「先跑起来、再懂为什么」的 CMake 手册。每一章的做法都一样：**把同一个关键字做成开关，
> 让构建从成功变成失败，再把磁盘上的物证摊给你看**。
> 所有输出都出自这台机器（cmake 4.2.3 + GNU Make 4.4.1 + gcc 15.2.0，只有 C，没有 C++）。

---

## 这本书是什么、写给谁

写给正在学 C 语言和数据结构、已经受够"抄一段 `CMakeLists.txt` 结果它居然能跑，但一改就炸"的人。
CMake 的资料有个特殊困难：网上同时流传**两代写法**（`include_directories` 那一代和
target 这一代），它们混在同一份文件里能同时生效，于是没人报错、所有人都学会了错的。
这本手册的对策只有一条：**不看你信什么，看命令行里到底出现了什么。**

于是全书反复用三样物证：

- `build/compile_commands.json` 和 `build/**/*.flags.make` —— 某个 target 真正的编译参数；
- `build/CMakeCache.txt` —— 上一次 configure 的全部决定（缓存粘住东西这件事的案发地）；
- 安装树 `ls -R <prefix>` —— 你交付出去到底交付了什么。

一句话前提：**全书只看一个仓库不够，得跑。** CMake 尤其如此 —— 它的结论全是
"某条命令行里有没有那个 `-I`"，光读段落你会以为你懂了，一拨开关就懵。

---

## 全书主线：三张地图（源码树 / 构建树 / 安装树）

这是全书的骨架，每一章都会回到它。它长这样：

```text
  地图 1：源码树 -S          地图 2：构建树 -B                 地图 3：安装树 <prefix>
  ┌────────────────┐        ┌───────────────────┐            ┌──────────────────┐
  │ CMakeLists.txt │ 配置+  │ Makefile          │  install   │ lib/libvector.a  │
  │ src/*.c *.h    │ ─────► │ CMakeCache.txt    │ ─────────► │ include/...      │
  │ （只放你写的） │  生成   │ **/*.flags.make   │            │ lib/cmake/模块   │
  └────────────────┘        │ 产物 .o / .a / .so│            └────────┬─────────┘
                            └─────────┬─────────┘                     │
                                      │ cmake --build                 │ find_package
                                      ▼                               ▼
                                    gcc 真的在编译            别人机器上的 consumer 工程
```

三张地图 = **三套路径基准**，这是全书最容易搞错的一件事：
同一条 `include` 路径，在地图 1 上成立，导出到地图 3 之后可能一个字都不剩。

第 01 章先立起前两张（源码目录 `-S` / 构建目录 `-B`，外加"CMake 不编译，`make` 才编译"），
第 03 章补上第三张（安装树 + 别人的机器），第 04 章则是在问：**第四种东西 —— 别人的库 ——
是怎么进到地图 1 里来的**。四章合起来正好是一条完整的路。

第 05、06 章不换地图，换的是**看地图的方式**：05 教你在构建树里取证（`CMakeCache.txt` /
`flags.make` / 预处理的 `.i` / `nm` / `size`），06 教你把取证出来的结论固化成一条命令
（CTest 的名单、Sanitizer 的两半、`CMakePresets.json`）。所以这两章可以晚点读，但**别跳过 05**——
后面任何一章的排错都靠它那一套手法。

---

## 章节表

| 章 | 目录 | 一句话讲什么 | `demo.sh` | 章 README |
|---|---|---|---|---|
| 01 | `01-first-build/` | 第一次构建：`配置 / 生成 / 构建` 三件事分开看、谁真的在编译、`build/` 为什么必须存在、缓存粘住编译器与生成器、`-D` 拼错名字的真实现场、导出 `compile_commands.json` 给 clangd | 187 | 535 |
| 02 | `02-targets-and-scopes/` | target 与作用域：`PUBLIC` / `PRIVATE` / `INTERFACE` 到底改了**谁**的命令行 —— 把关键字当开关，让编译从成功变失败，再用 `flags.make` 对质 | 222 | 636 |
| 03 | `03-libraries-install/` | 库、install 与 export：四类 `install()`、`CMAKE_PREFIX_PATH` 接通、导出文件里 target 怎么复活、版本、`RPATH` 事故现场、`DESTDIR` 打包 | 376 | 956 |
| 04 | `04-dependencies/` | 依赖：同一份 `mathx` 的四条到货路线（Config / Module / FetchContent / vendoring）、谁应答了 `find_package`、版本裁判是谁、内置模块 `FindThreads` 与 `FindOpenMP` 的真身 | 423 | 991 |
| 05 | `05-build-internals/` | 构建的四个旋钮（取证章）：`-G` 生成器与"换生成器报的是不匹配不是没装"、`CMAKE_BUILD_TYPE` 默认是空不是 Debug、`--config` 在单配置生成器上静默无效、缓存 `-D/-L/-LA/-U` 各自动了什么、找错头不报错怎么用 `.i` 抓、`flags.make` / `link.txt` / `nm` / `size` 四处互相印证 | 372 | 911 |
| 06 | `06-ctest-sanitizers-presets/` | CTest + Sanitizer + `CMakePresets.json`：`enable_testing()` 缺失与零用例那两块**一样的哑屏幕**、`CTestTestfile.cmake` 的解剖、四种裁判（退出码 / `PASS_REGULAR_EXPRESSION` / `WILL_FAIL` / `TIMEOUT`）各自的盲区、Sanitizer 三种错挂法与正确接法、preset 的覆盖顺序 / `hidden` / `condition` / `$env{}` 七种报错原文 | 517 | 1686 |
| 07 | `07-legacy-migration/` | 老写法 → 新写法的迁移取证（30 节）：`include_directories` 给的不是"能不能编"而是"**给了谁**"、`link_directories` 的赢家**不写在文件里**、`add_definitions` 一次配置所有 target 一起变、`file(GLOB)` 是配置期拍的一张快照（含 `CONFIGURE_DEPENDS` 的真实代价）、无关键字 `target_link_libraries`、`COMPILE_FLAGS` 与那串全局变量清单，最后一步一验走完整个迁移 | 645 | 3123 |
| 08 | `08-repo-cmake-walkthrough/` | 逐行拆解**本仓库根那份 249 行的 `CMakeLists.txt`**：`cmake_minimum_required` 到 `install` 的每一段、`ds_add_leaf` 怎么把 56 次样板收成一份、`DS_WITH_ASAN` 开关、警告集、输出落点，以及那 56 个调用点的接线；旁边复刻了一个四节的 mini 工程做对照 | 631 | 2119 |

各章工程旁边还留了搭场景用的素材，跑的时候会被复制进沙箱：04 的 `cmake/`（自己写的 Find 脚本）、
`mathx/`（vendored 源码）、`fetch/`、`consumer/`；05 的 `alt/`（那份"同名头"的冒牌货，用来演找错头）；
06 的 `variants/`（六份故意写坏的工程）+ `variants/presets/`（六份故意写坏的 JSON）+ `src/`（八支探针程序）；
07 的 `legacy/` 与 `modern/`（同一个工程的两种写法）+ `variants/`（九份专治一个症状的对照工程）+
`include/`、`src/`、`tp/`（被反复引用的头、源文件，和那份"装在两个前缀里的同名 libfoo"）；
08 的 `mini/`（仓库根那份文件的四节缩小版）+ `src/`（五组小样本）。

### 关于「第 09 章」：已归并进 03，不再单开

原先排期的「09（暂定）：把自己做的工程交给别人用 —— `install(TARGETS ... EXPORT ...)` →
`CMakePackageConfig` → 别人 `find_package` 拿到带名的 target」**不是没写，是已经被 03 章走完了**：
`lib/CMakeLists.txt` 里 `install(EXPORT ... NAMESPACE Vector::)`、
`write_basic_package_version_file(...)`（`CMakePackageConfigHelpers` 那两个函数之一）、
装出来的 `prefix/lib/cmake/Vector/VectorConfig.cmake`，以及旁边那个 consumer 工程
用 `find_package(Vector)` 拿到 `Vector::vector` —— 一整条链在 03 章是**真跑真装真消费**过的。
再开一章只会把同一条链重讲一遍，所以砍掉。

03 章之后**确实还没演**的三件事，别以为书里讲了：

| 没覆盖的 | 为什么没演 |
|---|---|
| `CPack` 打成 `.deb` / `.tar.gz` 分发 | 装包要 root，越了本手册「只写沙箱」的红线 |
| 多配置生成器（Xcode / Ninja Multi-Config）下的导出目录布局 | 本机只有单配置 Makefile 生成器，演了就是编 |
| `install(SCRIPT)`、自定义 install 阶段、`EXPORT_NAME` 改名 | 用得少，且每个都能自己 `cmake --help-package-props` 查到，不值得占篇幅 |

07 章里那些「导出那天才会炸」的症状证明的是**会炸**；炸法的完整现场在 03 章第 11 节
（把 `VECTOR_EXPORT_UTIL` 拨成 OFF，`install(EXPORT)` 当场把缺依赖的原文吐给你）。

---

## 怎么跑

每章一个目录，里面是 **一份 `demo.sh` + 章自己的小工程 + 一份 `README.md`**。

```bash
cd cmake-handbook/02-targets-and-scopes
bash demo.sh                     # 按 README 小节顺序跑完全章证据，跑完自动删沙箱
bash demo.sh --keep              # 保留沙箱并打印路径，进去自己拨开关
bash demo.sh > /tmp/c02.txt 2>&1 # 重定向时自动关颜色，拿到纯文本
```

在场的清单（在 `cmake-handbook/` 目录下敲，真输出如下）：

```text
$ ls */demo.sh
01-first-build/demo.sh
02-targets-and-scopes/demo.sh
03-libraries-install/demo.sh
04-dependencies/demo.sh
05-build-internals/demo.sh
06-ctest-sanitizers-presets/demo.sh
07-legacy-migration/demo.sh
08-repo-cmake-walkthrough/demo.sh
```

几点跑之前该知道的：

- 每章 demo 只做一件事：**把章 README 里引用的终端输出原样再生产一遍**。
  沙箱前缀（`/tmp/cmake01-Hn2Yac` 这种）每次随机，耗时那一栏也会飘，其余应当一字不差。
- 02 和 03 章的 demo 会**故意让编译失败好几次**。看到红色不是脚本坏了，
  那就是本章的证据本体 —— 每个失败段落后面都跟着 `[退出码 N]`，用它分辨"故意的失败"和"脚本自己崩了"。
- 06 章的 demo 更绕不开这一条：它的证据里有 **ctest 的红色**（`***Failed` / `***Timeout`，退出码 8）、
  **配置的红色**（`Can not find test to add properties to`，退出码 1）、还有 **ASan 的红色**
  （`heap-buffer-overflow`，退出码 8）。判断脚本自己有没有崩，只看每段末尾那个 `[退出码 N]`，
  以及最后 `find . -name CMakeCache.txt | wc -l` 那类自证行 —— 章 README 里引用的都是同一份产物。
- 03 章的 demo 里那 5 个 `CACHE` 开关（`VECTOR_DEP_SCOPE` 之类）是**实验装置，不是写法示范**，
  真实工程里一个都不要写。README 里也这么写着。

---

## 全书约定与安全红线

**实验全在 `mktemp -d` 的沙箱里做，读者可以照抄。** 同系列的 git / ssh 手册共用这套规矩（python / bash 两本按同一套写）。

| 红线 | 具体做法 | 为什么要这样 |
|---|---|---|
| **一切副作用进沙箱** | 源码副本、构建树、安装树、`FetchContent` 下载目录全在 `/tmp/cmakeNN-XXXXXX` 里，退出 `trap` 只删自己刚建那一个 | 手册不该在你的源码树里留 `CMakeCache.txt` |
| **`--prefix` 永远指向沙箱** | **绝不往 `/usr/local` 装东西**，一次都不 | 系统目录里的库会污染后面所有构建，而且很难看干净它是怎么进去的 |
| **打包演示用环境变量 `DESTDIR`** | 想演示"真装到 `/usr/local` 会怎样"，写 `DESTDIR=<沙箱>/stage cmake --install build --prefix /usr/local`：路径最终声明为 `/usr/local`，实际落点在暂存根下 | 这一条有现场：`cmake --install` 后面直接跟 `--destdir` 会得到 `Unknown argument --destdir` —— **本机 CMake 4.2.3 的 `cmake --install` 没有 `--destdir` 选项**（03 章第 15 节实测，用法列表里也搜不到它）。`DESTDIR` 是环境变量，不是命令行选项 |
| **不联网** | `FetchContent` 的地址**全部是 `file://`**：本地 bare git 仓库 + 本地 `tar.gz`。04 章 demo 会 `grep` 自己的 `CMakeLists.txt` 自证没有一个 `http(s)` | 一是复现不该依赖网络，二是这本手册要在任何机器上"照着敲就能跑" |
| **不写全局 CMake 配置** | 没有 `~/.cmake`、没有 `CMAKE_PREFIX_PATH` 环境变量注入、没有 `site.guess`；所有变量都在命令行 `-D` 或沙箱里的文件上。06 章演示 `CMakeUserPresets.json` 时也只在**沙箱的源码目录**里临时造一份，跑完随沙箱一起消失 | 全局配置是"我这能跑他那不能"的头号来源，也是手册最不该替你改的东西 |
| **屏幕上不出现真实身份** | demo 的取证命令一律用 `mktemp` 沙箱路径，需要"在某目录里跑"时只打印目录名（`basename`）+ 相对路径；章工程里读的环境变量全是自造的 `CH06_*` / `DS_*`，不碰你机器上任何真实变量。补一刀：`ls -l` 的**属主列**、`/home/<用户>` 这类**家目录路径**、`uname -a` 里的**主机名**，在 README 里统一写成 `<用户>` / `<家目录>` / `<主机名>` —— 这三样是 shell 自己展开出来的，demo 拦不住，只能事后替换 | ASan 报告、`CMakeCache.txt`、`ctest` 的名单都会把绝对路径整段吐到屏幕上；一本公开的手册不该带着你的登录名 |

> **这三处替换是全书唯一对真输出的改动**，其余围栏逐行原样。代价：引文校验器会把这些行各算 1 行"未匹配"
> （例如第 05 章稳定报 2 行），这是**预期内**的，不是编造 —— 校验时看到 `<用户>` / `<家目录>` 开头的未匹配行，
> 可以直接放过；除此之外任何未匹配行都得修。
| **跑被污染的程序也在沙箱里** | 06 章那八支故意写坏（越界、泄漏、UB）的探针只在沙箱构建、只在沙箱执行，全部是几十行的小程序，退出码和 stderr 就是证据 | 这些程序的行为按设计是"未定义"，把它们链到你的真实工程上没有意义，也不该让它们在你能碰到的目录里跑 |
| **只讲 C，产物只出自 Unix Makefiles** | 本机**没有 `g++`、没有 `ninja` 可执行文件**；只有 gcc 15.2.0 + GNU Make 4.4.1（`make` 在 `~/.local/bin`）。所以全书 `project(... C)`，**真正构建出东西的目录一律是默认的 `Unix Makefiles`**。唯一的例外是 05 章：它把 `-GNinja` 当反面教材**故意跑了好几次**（撞墙①②、`-DCMAKE_GENERATOR=Ninja`、只删缓存再来一次），因为那一章要讲的正是"生成器名字是 CMake 带的、工具要自己装" | 示例跑不出来就不该写；但"跑不出来时屏幕上那几行原话"是有价值的证据，05 章把它当证据跑 |
| **不碰本仓库的数据结构代码** | 仓库根那 56 节代码、根目录的 `Makefile` 和 `CMakeLists.txt`，手册一概不改不构建到它们头上 | 手册是旁路教材，不是第二个构建系统 |

---

## 我遇到 X，去哪一章

| 我的现场 | 去 |
|---|---|
| README 让我 `mkdir build && cd build && cmake ..`，我不明白为什么要多套一层 | 01（场景 8：在源码目录里 `cmake .` 的完整代价） |
| 改了 `CMakeLists.txt`，要不要重新跑一遍 `cmake` | 01（场景 5：生成的 Makefile 自己会重跑） |
| `-DSOME_OPTION=ON` 命令成功退出 0，但行为一点没变 | 01（场景 7 拼错名字的现场 + 场景 6 缓存） |
| 换了编译器，CMake 完全不理你，还在用 gcc | 01（场景 6、10：缓存把编译器和生成器粘死了） |
| 只想重编一个 `.c`，结果所有文件全重编 | 01（场景 4 + 场景 10 反悔与重来） |
| 编辑器 / clangd 一直报"找不到头文件" | 01（场景 9：把真实命令行导出成 `compile_commands.json`） |
| 明明 `target_link_libraries` 了，`#include "mylib.h"` 还是 No such file，只能自己补 `-I` | 02（场景 1、5：头目录的作用域降级了） |
| 老教程不写第三个关键字，我随手写了 `PRIVATE`，项目变大后开始出诡异问题 | 02（场景 2、3、8，含"编译全绿但程序行为变了"那种最阴的失败） |
| 我的库内部依赖了另一个库，消费者莫名 `undefined reference` | 02（场景 7）→ 03（第 11 节：导出时它换了个报错等你） |
| `-Wall` 加在库里，结果消费者一条警告都没吃到 | 02（作用域总表：`PRIVATE` 只砍谁） |
| 写了 `target_compile_features(hello PUBLIC c_std_11)`，命令行里却搜不到 `-std` | 02（场景 4：不是 CMake 没干活） |
| 别人写的 `INTERFACE` 库里列了 `.c`，构建"成功"了但什么都没产出 | 02（场景 10：不报错也不干活） |
| 别人 `find_package(Vector)` 找不到我已经装好的库 | 03（第 5、6 节：装了 ≠ 能被找到，`CMAKE_PREFIX_PATH` 接通） |
| `find_package` 报"找到了但不兼容"，和"根本没找到"是一回事吗 | 03（第 10 节：两种病，两套治法） |
| 导出时报错：内部依赖没在导出集合里 | 03（第 11 节，本章最硬的一条；不想外露看第 12 节 OBJECT 库那条路） |
| 库装成 `.so` 之后，程序跑不起来说找不到共享库 | 03（第 13 节：产物、链接、加载三件事一起变，`RPATH` 事故现场） |
| 公开头目录写了裸绝对路径，同一棵构建树里一个字不报，一导出就炸 | 03（第 14 节） |
| 想打 `.deb` / tar 包，但绝不希望构建脚本碰真系统目录 | 03（第 15 节：`DESTDIR` 的机制与那两次失败尝试） |
| `find_package(mathx)` 到底是谁应答的，我写的 Find 还是上游自带的 Config | 04（第 3~6 节：搜索顺序现场裁决 + `CONFIG` / `MODULE` 强制走一条） |
| 上游只给了 `.a` 和头，什么都没配置，怎么让它可被 `find_package` | 04（第 4 节：Module 模式，自己写 `Findmathx.cmake`） |
| "版本对不对"是谁判的，为什么 Module 模式下答案不一样 | 04（第 7、8 节：`ConfigVersion.cmake` 说了算 vs 交给你自己比） |
| `FetchContent` 会不会偷偷联网，公司机器上直接卡死 | 04（第 11~13 节：`file://` 演 GIT 与 URL 两条路 + 它的代价和三味解药） |
| 依赖找不到，我不想再乱改十行 `CMakeLists.txt` | 04（第 15 节：三步定位） |
| 照教程敲 `cmake -G Ninja build` 一堆红字，里面有 `CMAKE_C_COMPILER not set`，我以为是编译器没配好 | 05（旋钮一：本机没装 ninja 的完整报错原文） |
| 同目录换生成器报 `Does not match the generator used previously`，删了 `CMakeCache.txt` 照旧 | 05（旋钮一："只删缓存够不够"） |
| `cmake --build build --config Release` 退出 0，产物里一个 `-O` 都没有 | 05（旋钮二：`--config` 在单配置生成器上静默无效，`size` / `nm` 作证） |
| `-DSOME_OPTION=1` 少拼一个字母，构建成功但行为一点没变；`cmake -L` 想看缓存却噼里啪啦重跑配置 | 05（旋钮三：`-D` / `-L` / `-LA` / `-U` 各自动了哪本账） |
| 换了编译器，CMake 冒一句 `You have changed variables that require your cache to be deleted.`，我想知道它顺手擦了谁 | 05（旋钮三） |
| 项目里有两份同名头，程序链接正常、跑得不对，grep 半天找不到用的哪份 | 05（旋钮四：预处理 `.i` 的文件标记行 + 老写法 `BEFORE` 的顺序反转陷阱） |
| `ctest --test-dir build` 说 `No tests were found!!!` 却退 0 | 06（第 2、3 节：缺 `enable_testing()` 和零用例是同一块哑屏幕，只能靠 `CTestTestfile.cmake` 是否存在来分辨） |
| 输出明明是错的（43 不是 42），测试却绿 | 06（第 6~10 节：四种裁判各自的盲区） |
| 我给用例标了 `WILL_FAIL`，结果它把本来绿的判成红 | 06（第 9 节：`WILL_FAIL` 只认退出码，不认输出） |
| `-fsanitize=address` 写进 target 属性，构建照过，什么也没挂上 | 06（第 13 节：未知属性名静默照单全收） |
| `undefined reference to '__asan_init'`；或者 ASan 加了、越界程序却安静退 0 | 06（第 14 节 编译期/链接期只补了一半，第 15 节 `add_compile_options` 位置写错） |
| UBSan 打了 `runtime error`，ctest 还是 `Passed` | 06（第 19 节：默认 recover，要 `-fno-sanitize-recover=all` 才改退出码） |
| 手敲那一长串 `-D` 太累，想固化成一条命令 | 06（第 21~23 节：`--preset` 三条对齐 + 命令行 `-D` / `-B` 的覆盖顺序） |
| `--list-presets` 里没有我要的那个，可文件里明明写了 | 06（第 24 节：`hidden` 在任何名单里都不出现、`condition` 不满足会被整条跳过） |
| preset 报错看不懂 | 06（第 28 节：七种报错原文当字典用，含 `No such preset`、`Cyclic preset inheritance`、`Invalid extra field`；`Cannot use hidden preset` 那条在第 24 节） |

---

## 建议阅读顺序

**01 → 02 → 03 → 04 → 05 → 06**，前四章基本不可换，后两章可以插队：

- 01 立的是"三张地图 + 去看命令行"这个方法，后面每一章都拿它当显微镜；
- 02 的三个关键字必须在同一棵构建树里看懂，才有资格进 03 —— 03 章开头就把 02 的语义抬一层：
  同树搞错最多编译不过，**交付出去搞错是别人链接不上**；
- 04 是"别人已经有了"的世界，用的是 03 章装出去的那个 mathx 当靶子，所以不能提前读；
- 05 不换地图，换的是取证手法。它假定你已经会在 `-B` 目录里翻 `CMakeCache.txt` / `flags.make`（01 教的），
  除此之外它自己就是全书的"排错附录"，**卡住的时候直接翻它的排错速查和构建目录解剖表**；
- 06 需要 05 的旋钮心智模型（配置写规则 / 构建执行规则），因为它把那两个旋钮固化进
  `CMakePresets.json`。如果你已经在用别人写好的 preset，可以只读 06 的第 21~23 节和第 30 节小抄。

**可以跳 / 可以晚点的：**

- 只写自己一个人玩的小工程：03 的第 10 节之后、以及整个 04 章都可以先放着，
  但 01 + 02 不能跳 —— 那是所有 CMake 报错的解读器。
- 只想看懂别人工程写的 `CMakeLists.txt`：01 + 02 + 04 的第 3~8 节 + 06 的第 21 节（`--list-presets` 念名单），
  够你读懂 80%。
- 03 章那 5 个实验开关的细节（第 11、12、14 节）第一次读可以只记结论，回头再拨。
- 06 的三个部分相对独立：第 1 部分（CTest）、第 2 部分（Sanitizer）、第 3 部分（preset）
  可以按你需要挑一块，只要第 0 部分那 5 节（名单落在哪个文件、哑屏幕）先看完。

---

## 这本书没覆盖什么

- **C++**：机器上没有 `g++`，全书零 C++ 内容（`cxx_std_17`、`INTERFACE` 库的语言标准、
  C++ 项目布局、vcpkg / Conan 全都没有）。
- **其他生成器的实际行为**：05 章讲清了"生成器是谁、它在哪个文件里记账、换生成器为什么报不匹配"，
  但本机**只有 GNU Make，没有 Ninja**，也拿不到 Visual Studio / Xcode，所以
  **多配置生成器的现场一个都没演**：`CMAKE_CONFIGURATION_TYPES` 到底长什么样、
  `--config` 在多配置生成器上怎么生效、产物为什么会带 `Debug/` `Release/` 子目录 ——
  05 章里这些只以"候选路径列表"的样子出现过一次，没有实证。
- **CTest 的另一半**：06 章演的是"给已构建好的可执行文件当裁判"。
  `ctest -S`（用 CTest 脚本驱动整个 配置→构建→测试→提交 流程）、`Coverage` / `MemoryCheck` 两个
  `ctest_test()` 后端、`-j` / `PROCESSORS` / `RUN_SERIAL` / `FIXTURES_*`、`WORKING_DIRECTORY`、
  `MISSING_REGULAR_EXPRESSION` / `FAIL_REGULAR_EXPRESSION` 这两把没演的尺子、`ctest --show-only=json-v1`
  都不在书里（06 章末尾「本章没演 / 没验证」逐条列了，那一条清单也是这章的诚实边界）。
- **Sanitizer 的代价与边界**：挂 ASan 之后慢多少、内存多用多少，本机没测；
  TSan / MSan / HWASan 以及 `ASAN_OPTIONS` 调参只在 06 章末尾列了名字，一条都没实跑。
- **preset 的深水区**：`toolchainFile` / `pkgConfig` / `vendorPaths` / `installDir` / `testSpec`
  这些字段 06 章一个都没用；`testPreset` 只用过 `output.outputOnFailure` 和 `filter.include`，
  报告格式（`xml` / `junit`）与成功率阈值那类字段没碰；`packagePreset` 和 `workflowPreset`
  （`ctest --workflow`）命令行里能看到选项，但没真跑过，所以不给你结论。
  `CMakeUserPresets.json` 只在沙箱里演示了一次，**与公共文件同名 preset 的覆盖先后顺序本机没验证**。
- **跨工程交付的完整图景**：包管理器、`CPack`、toolchain 文件、交叉编译、`find_root_path` 行为、
  exported target 名字空间治理，03 只讲到"能被 `find_package` 找到 + `DESTDIR` 打包"为止。
- **老写法迁移**：`include_directories` / `link_directories` / `add_definitions` / `file(GLOB)` /
  无关键字签名 / `COMPILE_FLAGS` 这一整套怎么一步步改过来 —— **第 07 章**（30 节，每种坏写法都先让它
  在本机炸一次、给出对策再验证）。07 章没演的仍然没演：它把"导出那天才会炸"的那类症状留给了 09 章。
- **本仓库那份 `CMakeLists.txt` 的逐行读法** —— **第 08 章**（对着 249 行原文一段一段讲，
  旁边有四节缩小版的 `mini/` 做可运行对照）。08 章只读不写：仓库根那份文件一个字节都没被本手册改过。
- 宏、`Generator Expression` 全表、`CMakeScript`（`cmake -P`）、`FetchContent` 与
  `ExternalProject` 的差别，都不在书里。

---

## 下一步

从 [`01-first-build/`](01-first-build/README.md) 开始：它一章把"配置 / 生成 / 构建"拆干净，
并交给你全书唯一需要的方法 —— **去 `flags.make` 和 `compile_commands.json` 里看真实命令行**。
跑法永远是同一条：`cd <章节目录> && bash demo.sh`。
