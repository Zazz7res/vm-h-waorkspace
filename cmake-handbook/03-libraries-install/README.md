# 第 03 章《库、install 与 export：让别人 find_package 得到你的库》

> 前置：第 01 章（configure/build 两阶段、缓存、`-S/-B`）、第 02 章（PUBLIC/PRIVATE/INTERFACE、
> `flags.make` 物证法）。本章会把第 02 章的作用域语义**再往上抬一层**：
> 在同一条构建树里，作用域搞错最多是编译不过；**交付出去之后，作用域搞错会让别人链接不上。**
>
> 本章所有终端输出都来自一次真实运行：`bash demo.sh`（608 行）。
> 沙箱前缀 `/tmp/cmake03-hAJIJq` 是随机的、`Configuring done (0.2s)` 里的耗时会浮动，
> 这两处不必对齐；其余每一行都应当一字不差。

---

## 0. 这一章解决什么问题

第 02 章的世界裡只有一个工程：`add_subdirectory` 把大家拉进同一条构建树，
target 之间用 PUBLIC/PRIVATE 互相传话，传错了当场编译失败，好查。

真实世界是两半：

```
   你的源码树 src/            你的构建树 build/            别人的机器
   ┌──────────────┐  configure ┌──────────────┐  install  ┌──────────────┐
   │ lib/vector.c │ ──────────► │ libvector.a  │ ────────► │ <前缀>/lib   │
   │ lib/...      │             │ app          │           │ <前缀>/include│
   └──────────────┘             └──────────────┘           │ <前缀>/lib/  │
                                                            │  cmake/Vector│
                                                            └──────┬───────┘
                                                                   │ find_package(Vector)
                                                            ┌──────▼───────┐
                                                            │ consumer 工程│
                                                            └──────────────┘
```

**三张地图，三套路径基准。** 前两组由你控制（写 CMakeLists 的人是你），
最后一组由**你的用户**控制：他可能在一台完全不同的机器上、把包解压到任何目录、
只拿着你 `install` 出去的那些文件。

所以本章只讲一件事：**`install` 之后，一个 target 身上哪些东西会被带走、哪些会当场失效。**
一句话总结全章：

> **装出去的东西才是契约。** 没装出去的头等于不存在，没导出的依赖等于链接不上，
> 没写对的 RPATH 等于程序跑不起来 —— 而这三种坏法，前两种在配置期报错，最后一种只在用户机器上报错。

本章的实验装置有 5 个 CACHE 开关（**真实项目里一个都不要写**，它们只用来把同一份源码拨成 5 种状态）：

| 开关 | 默认 | 拨动它会发生什么 |
|---|---|---|
| `BUILD_SHARED_LIBS` | OFF | 库从 `.a` 变 `.so`，依赖被物理吸进去，运行期多一个「去哪找」的问题 |
| `VECTOR_DEP_SCOPE` | PRIVATE | 内部依赖链接作用域 → 决定导出文件里是 `$<LINK_ONLY:Vector::util>` 还是裸 `Vector::util` |
| `VECTOR_EXPORT_UTIL` | ON | OFF 就复现本章最硬的那条导出报错（第 11 节） |
| `VECTOR_UTIL_AS_OBJECT` | OFF | 内部依赖改成 OBJECT 库：代码物理并进 `libvector.a` |
| `VECTOR_PLAIN_INCLUDE` | OFF | 公开头目录用裸绝对路径 → 同树没事、一导出就炸（第 14 节） |

---

## 1. 工程结构：一个库、一个内部依赖、两种消费者

```
$ find . -type f | sort
./CMakeLists.txt
./app/CMakeLists.txt
./app/main.c
./cmake/VectorConfig.cmake.in
./consumer/CMakeLists.txt
./consumer/main.c
./lib/CMakeLists.txt
./lib/include/vector.h
./lib/src/vector.c
./lib/src/vector_internal.h
./util/CMakeLists.txt
./util/include/util.h
./util/src/util.c
```

目录布局是业界最常见的那一种，`lib/` 下面分成两半是**本章全部故事的物理基础**：

```
lib/include/vector.h        对外契约   → 会装到 <前缀>/include/vector.h
lib/src/vector.c            实现
lib/src/vector_internal.h   内部实现   → 不装，消费者永远看不见
util/include/util.h         内部依赖的契约 → 装到 <前缀>/include/vector/util.h（多包一层）
```

两个消费者的区别是主线：

| | `app/` | `consumer/` |
|---|---|---|
| 和库的关系 | 同一条构建树（顶层 `add_subdirectory`） | **完全独立**的工程 |
| 怎么拿到库 | `target_link_libraries(app PRIVATE Vector::vector)` | `find_package(Vector)` + 同名 target |
| 能看见什么 | 源码树里的一切路径 | **只有安装树里的那几个文件** |
| 什么时候坏 | 配置期 | 装完之后（可能是三个月后、别人机器上） |

`consumer/` 故意**没有**被顶层 `add_subdirectory` 加进来。加了它就等于作弊：
它能直接看见源码路径，于是「装漏了什么」「头有没有导出」这类问题全都暴露不出来。

---

## 2. 默认档 configure：那几行 `message` 是「当前拨到哪一档」的回执

```
$ cmake -S . -B build
-- The C compiler identification is GNU 15.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- 工程 Vector 1.0.2，库类型 = OFF（OFF=静态 .a，ON=共享 .so）
--   VECTOR_DEP_SCOPE      = PRIVATE
--   VECTOR_EXPORT_UTIL    = ON
--   VECTOR_UTIL_AS_OBJECT = OFF
--   VECTOR_PLAIN_INCLUDE  = OFF
--   CMAKE_INSTALL_PREFIX  = /usr/local
-- Configuring done (0.2s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake03-hAJIJq/proj/build
```

默认档就是**推荐写法**：静态库、依赖 PRIVATE、内部依赖一起导出、头目录用 generator expression。
`CMAKE_INSTALL_PREFIX` 现在是默认的 `/usr/local`，第 4 节会把它指到沙箱里 —— 本章绝不往真系统装东西。

顶层还有一段自己的校验。因为 `CACHE STRING` 的取值 CMake **一律不检查**（第 02 章的教训），
拼错 `PRIVATE` 试试：

```
$ cmake -S . -B build-typo -DVECTOR_DEP_SCOPE=PROVITE | grep 'VECTOR_DEP_SCOPE|非法|Configuring incomplete'
  VECTOR_DEP_SCOPE='PROVITE' 非法，只接受 PRIVATE 或 PUBLIC
-- Configuring incomplete, errors occurred!
```

对照第 02 章那次「拼错变量名 → 静默失效」：这里因为写了

```cmake
if(NOT VECTOR_DEP_SCOPE MATCHES "^(PRIVATE|PUBLIC)$")
    message(FATAL_ERROR "VECTOR_DEP_SCOPE='${VECTOR_DEP_SCOPE}' 非法，只接受 PRIVATE 或 PUBLIC")
endif()
```

就变成当场报错。顺带一个高频误解：

```cmake
set_property(CACHE VECTOR_DEP_SCOPE PROPERTY STRINGS PRIVATE PUBLIC)
```

这行**只**让 `cmake-gui`/`ccmake` 显示成下拉框，对命令行 `-D` 一点约束力都没有。
取值校验必须自己写 `MATCHES`。

---

## 3. 构建：两个纯路径坑，先在这里踩掉

```
$ cmake --build build
[ 16%] Building C object util/CMakeFiles/util.dir/src/util.c.o
[ 33%] Linking C static library libutil.a
[ 33%] Built target util
[ 50%] Building C object lib/CMakeFiles/vector.dir/src/vector.c.o
[ 66%] Linking C static library libvector.a
[ 66%] Built target vector
[ 83%] Building C object app/CMakeFiles/app.dir/main.c.o
[100%] Linking C executable app
[100%] Built target app
```

```
$ find build \( -name '*.a' -o -name '*.so*' \) | sort
build/lib/libvector.a
build/util/libutil.a

$ ls -ld build/app; ls -l build/app/app
drwxrwxr-x 3 <用户> <用户> 120 <日期> build/app
-rwxrwxr-x 1 <用户> <用户> 16416 <日期> build/app/app

$ ./build/app/app
表里有 6 个元素：3 6 9 12 15 18
总和 = 63
```

两个占位符按全书约定折掉了真值：`<用户>` 是本机登录名，`<日期>` 是 `ls` 那一列 mtime ——
它每重新构建一次就变一次，所以本章不拿它对账，只留字段形状（`权限 链接数 属主 属组 字节数 时间 路径`）。

两个坑，都是「命令没错、路径写错」型：

1. **可执行文件是 `build/app/app`**，不是 `build/app`。因为 `app/` 是个 `add_subdirectory` 子目录，
   CMake 会在构建树里镜像源码树的目录结构，于是「目录叫 app、里面的可执行文件也叫 app」。
   写成 `./build/app` 会得到 `bash: Is a directory`（退出码 126）。
2. **`find` 里的 `\(` `\)`**。写成 `find build -name '*.a' -o -name app` 时，
   `-o` 右侧的规则自带一个隐式 `-print`，左侧的没有 —— 结果只打印后半边（或者混进目录）。
   只要用了 `-o`，就把「匹配条件」用括号包起来，让隐式 `-print` 作用于整体。

---

## 4. `install` 到底干了什么：四类 `install()` 对应安装树的四块

```
$ cmake --install build --prefix "/tmp/cmake03-hAJIJq/prefix"
-- Install configuration: ""
-- Installing: /tmp/cmake03-hAJIJq/prefix/lib/libutil.a
-- Installing: /tmp/cmake03-hAJIJq/prefix/include/vector
-- Installing: /tmp/cmake03-hAJIJq/prefix/include/vector/util.h
-- Installing: /tmp/cmake03-hAJIJq/prefix/lib/libvector.a
-- Up-to-date: /tmp/cmake03-hAJIJq/prefix/include
-- Installing: /tmp/cmake03-hAJIJq/prefix/include/vector.h
-- Installing: /tmp/cmake03-hAJIJq/prefix/lib/cmake/Vector/VectorTargets.cmake
-- Installing: /tmp/cmake03-hAJIJq/prefix/lib/cmake/Vector/VectorTargets-noconfig.cmake
-- Installing: /tmp/cmake03-hAJIJq/prefix/lib/cmake/Vector/VectorConfig.cmake
-- Installing: /tmp/cmake03-hAJIJq/prefix/lib/cmake/Vector/VectorConfigVersion.cmake
-- Installing: /tmp/cmake03-hAJIJq/prefix/bin/app
```

```
$ find "/tmp/cmake03-hAJIJq/prefix" -type f | sort
<沙箱>/prefix/bin/app
<沙箱>/prefix/include/vector.h
<沙箱>/prefix/include/vector/util.h
<沙箱>/prefix/lib/cmake/Vector/VectorConfig.cmake
<沙箱>/prefix/lib/cmake/Vector/VectorConfigVersion.cmake
<沙箱>/prefix/lib/cmake/Vector/VectorTargets-noconfig.cmake
<沙箱>/prefix/lib/cmake/Vector/VectorTargets.cmake
<沙箱>/prefix/lib/libutil.a
<沙箱>/prefix/lib/libvector.a
```

安装树的每一块都能对应回一条 `install()`：

| 安装树里的位置 | 由哪条命令决定 | 关键细节 |
|---|---|---|
| `bin/app` | `install(TARGETS app RUNTIME DESTINATION bin)` | 可执行文件走 `RUNTIME` |
| `lib/libvector.a` | `install(TARGETS vector ARCHIVE …)` | `.a` 走 `ARCHIVE`，`.so` 走 `LIBRARY`（第 13 节会看到切换） |
| `include/vector.h` | `install(DIRECTORY include/ DESTINATION include)` | **`include/` 结尾那个斜杠是关键** |
| `include/vector/util.h` | `install(DIRECTORY include/ DESTINATION include/vector)` | 内部依赖的头，多包一层目录 |
| `lib/cmake/Vector/*.cmake` | `install(EXPORT …)` + `install(FILES …)` | `find_package` 的全部入口 |

那个斜杠值得单独记：

```cmake
install(DIRECTORY include/ DESTINATION include)   # → include/vector.h          ✅
install(DIRECTORY include  DESTINATION include)   # → include/include/vector.h  ❌
```

以及为什么路径写成 `${CMAKE_INSTALL_LIBDIR}` 而不是 `"lib"`：
`include(GNUInstallDirs)` 会在 64 位发行版上把 `CMAKE_INSTALL_LIBDIR` 给成 `lib64`，
在 Debian 系给成 `lib`。写死 `"lib"` 的包在某些机器上装完 `find_package` 就找不到。
注意 `include(GNUInstallDirs)` 写在**顶层**就够了，子目录自动继承 —— 变量是普通变量，
向下可见，向上不可见。

`vector_internal.h` 没出现在安装树里。它从来没被 `install` —— 这正是第 9 节的靶子。

---

## 5. 先看它怎么「找不到」：装了 ≠ 能被找到

```
$ cmake -S consumer -B build-consumer 2>&1 | tail -18
CMake Error at CMakeLists.txt:28 (find_package):
  By not providing "FindVector.cmake" in CMAKE_MODULE_PATH this project has
  asked CMake to find a package configuration file provided by "Vector", but
  CMake did not find one.

  Could not find a package configuration file provided by "Vector" (requested
  version 1.0) with any of the following names:

    VectorConfig.cmake
    vector-config.cmake

  Add the installation prefix of "Vector" to CMAKE_PREFIX_PATH or set
  "Vector_DIR" to a directory containing one of the above files.  If "Vector"
  provides a separate development package or SDK, be sure it has been
  installed.


-- Configuring incomplete, errors occurred!
```

这条报错是本章最重要的一条：**库确实装好了，但没有任何人告诉 CMake 前缀在哪。**
`find_package` 的查找路径是有限集合（`CMAKE_PREFIX_PATH`、`<包名>_ROOT`、`<包名>_DIR`、
系统默认前缀如 `/usr/local`、`CMAKE_SYSTEM_PREFIX_PATH`），你的沙箱前缀不在里面。

报错里那句 `Add the installation prefix … to CMAKE_PREFIX_PATH or set "Vector_DIR"`
就是解决办法 —— CMake 少见地把答案写在自己脸上，看到这条就照着填。

还有两种「同一种病的不同说法」要认得：

* 文件名候选只有 `VectorConfig.cmake` 和 `vector-config.cmake` 两种。
  你的模板文件名叫 `VectorConfig.cmake.in` 不是随便起的：包名大写 + `Config.cmake`。
* `(requested version 1.0)` 这一句说明**版本参数已经传进去了**（来自
  `find_package(Vector ${CONSUMER_FIND_VERSION} REQUIRED)`）。
  如果连文件都找不到，报错里就根本不会出现版本号 —— 这是区分「没找到」和「版本不兼容」的第一线索。

---

## 6. 用 `CMAKE_PREFIX_PATH` 接通

```
$ cmake -S consumer -B build-consumer -DCMAKE_PREFIX_PATH="/tmp/cmake03-hAJIJq/prefix"
-- consumer 找到 Vector 的配置文件目录：/tmp/cmake03-hAJIJq/prefix/lib/cmake/Vector
-- consumer 眼里 Vector::vector 的头目录  = /tmp/cmake03-hAJIJq/prefix/include
-- consumer 眼里 Vector::vector 的链接要求 = $<LINK_ONLY:Vector::util>
-- Configuring done (0.0s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake03-hAJIJq/proj/build-consumer

$ cmake --build build-consumer
[ 50%] Building C object CMakeFiles/consumer.dir/main.c.o
[100%] Linking C executable consumer
[100%] Built target consumer

$ ./build-consumer/consumer
[consumer] 元素数 = 5，总和 = 55
```

中间那三行 `message` 是 consumer 自己去**读导入目标身上的属性**读出来的：

```cmake
find_package(Vector ${CONSUMER_FIND_VERSION} REQUIRED)
get_target_property(_vec_inc Vector::vector INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(_vec_lnk Vector::vector INTERFACE_LINK_LIBRARIES)
```

这招是排查 `find_package` 问题的第一工具：**别猜 CMake 找到了什么，把它读出来打印。**
三个属性各自的意义：

* `Vector_DIR` = `find_package` 实际加载 `VectorConfig.cmake` 的目录。
  它是 CMake 自己设的变量，指向的就是那个包配置文件所在处。
* 头目录 = `<前缀>/include` —— 来自 `$<INSTALL_INTERFACE:include>` 那条相对路径 + 前缀拼接。
* 链接要求 = `$<LINK_ONLY:Vector::util>` —— 这个写法是第 7 节的主角，也是第 8 节 A/B 的对照组。

顺带一条容易忽略的语法：`find_package(Vector 1.0 REQUIRED)` 里的 `REQUIRED`
意味着找不到就配置失败。不写 `REQUIRED` 时 CMake 会把 `Vector_FOUND` 设成 `FALSE`
然后**继续往下跑**，于是你在十几行之后撞上
`Target links to Vector::vector which was not found` —— 症状离病因隔了几十行。
库依赖是硬依赖，一律写 `REQUIRED`。

---

## 7. 物证：导出的 `VectorTargets.cmake` 里 target 是怎么「复活」的

第 02 章用 `flags.make` 当物证，本章的物证换成 `VectorTargets.cmake` ——
它就是「装出去的那份 target 说明书」。

```
$ grep -nE "add_library|LINK_LIBRARIES|INCLUDE_DIRECTORIES" /tmp/cmake03-hAJIJq/prefix/lib/cmake/Vector/VectorTargets.cmake
59:add_library(Vector::util STATIC IMPORTED)
62:  INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include/vector"
66:add_library(Vector::vector STATIC IMPORTED)
70:  INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include"
71:  INTERFACE_LINK_LIBRARIES "\$<LINK_ONLY:Vector::util>"
```

```
$ grep -h 'C_INCLUDES' build-consumer/CMakeFiles/consumer.dir/flags.make | sed "s|/tmp/cmake03-hAJIJq|<沙箱>|g"
C_INCLUDES = -isystem <沙箱>/prefix/include
```

四件事值得停下来：

1. **`add_library(Vector::vector STATIC IMPORTED)`**
   消费者拿到的是一个 IMPORTED 目标：它不编译你的源码，只是登记
   「头在哪、库文件在哪、要顺带链接谁」。所以 `Vector::vector` 这个名字在
   你的工程里是 ALIAS、在消费者那里是 IMPORTED —— **同一个名字，两种来源**，
   这正是 ALIAS 的价值：库内部怎么改，消费者代码一个字不动。
2. **`${_IMPORT_PREFIX}` 是算出来的，不是写死的**
   所以整个前缀目录可以搬到任何机器、任何位置，解压即用。
   这就是 `$<INSTALL_INTERFACE:include>`（相对路径）和裸绝对路径的分水岭（第 14 节会用报错证明）。
3. **消费者拿到的是 `-isystem` 而不是 `-I`**
   CMake 默认把导入目标的头目录标成 system 目录，好处是**库头文件里的警告不会刷屏你的构建输出**
   （GCC 对 system 头目录里的代码默认不报警告）。副作用是：如果你**故意**想在自己的代码里
   靠 `-Wall -Werror` 检查一切，库头的警告会被静默掉。
4. **`$<LINK_ONLY:Vector::util>` = 「util 只参与链接，不参与编译」**
   这就是 `PRIVATE` 在导出文件里的长相。第 02 章的语义换了身衣服还在，
   而且**衣服换得更有欺骗性** —— 见下一节。

---

## 8. A/B 实验：只把作用域换成 PUBLIC，导出文件、`-I`、可见的头全变

这是本章的核心实验。**只改一个 `-D`**，其余一个字不动：
`-DVECTOR_DEP_SCOPE=PUBLIC`，重新构建、装到另一个前缀，然后拿同一个 consumer 去撞。

```
$ cmake -S . -B build-pub -DVECTOR_DEP_SCOPE=PUBLIC | grep 'VECTOR_DEP_SCOPE|Configuring done|Build files'
--   VECTOR_DEP_SCOPE      = PUBLIC
-- Configuring done (0.2s)
-- Build files have been written to: /tmp/cmake03-hAJIJq/proj/build-pub

$ cmake --install build-pub --prefix "/tmp/cmake03-hAJIJq/prefix-pub" | grep 'libutil|libvector|util.h'
-- Installing: /tmp/cmake03-hAJIJq/prefix-pub/lib/libutil.a
-- Installing: /tmp/cmake03-hAJIJq/prefix-pub/include/vector/util.h
-- Installing: /tmp/cmake03-hAJIJq/prefix-pub/lib/libvector.a
```

导出文件里唯一的区别就是那一行：

```
$ grep -nE "add_library|LINK_LIBRARIES|INCLUDE_DIRECTORIES" /tmp/cmake03-hAJIJq/prefix-pub/lib/cmake/Vector/VectorTargets.cmake
59:add_library(Vector::util STATIC IMPORTED)
62:  INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include/vector"
66:add_library(Vector::vector STATIC IMPORTED)
70:  INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include"
71:  INTERFACE_LINK_LIBRARIES "Vector::util"
```

```
$ cmake -S consumer -B bpub -DCMAKE_PREFIX_PATH="/tmp/cmake03-hAJIJq/prefix-pub" | grep '链接要求|头目录'
-- consumer 眼里 Vector::vector 的头目录  = /tmp/cmake03-hAJIJq/prefix-pub/include
-- consumer 眼里 Vector::vector 的链接要求 = Vector::util

$ grep -h C_INCLUDES bpub/CMakeFiles/consumer.dir/flags.make | sed "s|/tmp/cmake03-hAJIJq|<沙箱>|g"
C_INCLUDES = -isystem <沙箱>/prefix-pub/include -isystem <沙箱>/prefix-pub/include/vector
```

然后让 consumer 去 `#include "util.h"` —— 同一个源文件，两个档各撞一次：

```
$ cmake -S consumer -B bpub2 -DCMAKE_PREFIX_PATH="/tmp/cmake03-hAJIJq/prefix-pub" -DCONSUMER_PROBES_UTIL_H=ON >/dev/null && cmake --build bpub2 2>&1 | tail -2 && ./bpub2/consumer
[100%] Linking C executable consumer
[100%] Built target consumer
[consumer] 元素数 = 5，总和 = 55
[consumer] 用到了内部依赖 u_clamp(999,0,10) = 10

$ cmake -S consumer -B bpriv -DCMAKE_PREFIX_PATH="/tmp/cmake03-hAJIJq/prefix" -DCONSUMER_PROBES_UTIL_H=ON >/dev/null && cmake --build bpriv 2>&1 | grep -E 'fatal error|Error 1' | head -2
/tmp/cmake03-hAJIJq/proj/consumer/main.c:21:10: fatal error: util.h: No such file or directory
gmake[2]: *** [CMakeFiles/consumer.dir/build.make:79: CMakeFiles/consumer.dir/main.c.o] Error 1
```

汇总成一张表 —— 这张表就是第 02 章作用域表的「交付版」：

| | `PRIVATE util`（默认档） | `PUBLIC util` |
|---|---|---|
| 导出文件里那一行 | `"$<LINK_ONLY:Vector::util>"` | `"Vector::util"` |
| consumer 的 `-isystem` | 只有 `<前缀>/include` | 多了 `<前缀>/include/vector` |
| consumer 能 `#include "util.h"` 吗 | **不能** | **能** |
| `libutil.a` 装了吗 | 装了 | 装了 |
| consumer 链接时用不用它 | **用** | 用 |
| 消费者链接命令长度 | 两条 `.a` | 两条 `.a` |

最后两行是本章最容易搞反的地方，用一句话钉死：

> **`PRIVATE` 切断的是「编译期传染」（头目录、宏、编译选项），从来没有切断「链接期传染」。**
> 「不传染」≠「不交付」。这句话是第 11 节那条报错的伏笔。

反过来选 `PUBLIC` 也不是免费的：util.h 必须跟着装出去（否则 `-isystem` 指向空目录），
于是你的内部依赖的头**变成了公开 API 的一部分** ——
以后改一个函数签名就是破坏兼容。所以判断标准不是
「我想不想让消费者看见」，而是**「消费者的代码需不需要 include 它」**：
不需要就 `PRIVATE`（并且把它的头藏到 `include/vector/` 这种深层目录里），需要就 `PUBLIC`（并且当成契约维护）。

---

## 9. 两种「看不见」，病因完全不同

```
$ cmake -S consumer -B build-leak -DCMAKE_PREFIX_PATH="/tmp/cmake03-hAJIJq/prefix" -DCONSUMER_PROBES_INTERNAL=ON >/dev/null && cmake --build build-leak 2>&1 | grep -E 'fatal error|Error 1' | head -2
/tmp/cmake03-hAJIJq/proj/consumer/main.c:14:10: fatal error: vector_internal.h: No such file or directory
gmake[2]: *** [CMakeFiles/consumer.dir/build.make:79: CMakeFiles/consumer.dir/main.c.o] Error 1
```

同样是 `No such file or directory`，但 `util.h` 和 `vector_internal.h` 是两种病：

| | `util.h`（库的私有依赖的头） | `vector_internal.h`（库自己的内部头） |
|---|---|---|
| 装出去了吗 | **装了**（`include/vector/util.h`） | **没装**（安装树里根本没有） |
| 为什么看不见 | 没有 `-I`：作用域是 `PRIVATE` | 没有文件：交付清单里没有 |
| 换个作用域能看见吗 | 能（`PUBLIC` 就看见了） | **永远不能**（装都没装） |
| 属于哪类决定 | `target_link_libraries` 的关键词 | `install()` 的清单 |

第二条推论很重要：`vector_internal.h` 之所以安全，是因为它**放在 `lib/src/` 而不是 `lib/include/`**，
而 `install(DIRECTORY include/ …)` 只搬 `include/`。
也就是说「公开 / 内部」的分界在**目录结构**上做，不在 CMakeLists 的某一行上做 ——
目录分界是物理的，`PRIVATE` 是逻辑的；物理分界不会因为别人改一行 CMakeLists 就失效。

---

## 10. 版本：`find_package` 报「找到了但不兼容」和「根本没找到」是两种病

```
$ cmake -S consumer -B build-ver -DCMAKE_PREFIX_PATH="/tmp/cmake03-hAJIJq/prefix" -DCONSUMER_FIND_VERSION=2.0 2>&1 | tail -12
CMake Error at CMakeLists.txt:28 (find_package):
  Could not find a configuration file for package "Vector" that is compatible
  with requested version "2.0".

  The following configuration files were considered but not accepted:

    /tmp/cmake03-hAJIJq/prefix/lib/cmake/Vector/VectorConfig.cmake, version: 1.0.2
      The version found is not compatible with the version requested.



-- Configuring incomplete, errors occurred!
```

```
$ cmake -S consumer -B build-ver1 -DCMAKE_PREFIX_PATH="/tmp/cmake03-hAJIJq/prefix" -DCONSUMER_FIND_VERSION=1.0 | grep '找到 Vector 的配置|Configuring done'
-- consumer 找到 Vector 的配置文件目录：/tmp/cmake03-hAJIJq/prefix/lib/cmake/Vector
-- Configuring done (0.2s)
```

「考虑过但没接受」这句是关键字：它证明**文件找到了**，问题出在版本比对上。
比对由 `VectorConfigVersion.cmake` 执行，它是这一句生成的：

```cmake
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/VectorConfigVersion.cmake
    VERSION ${PROJECT_VERSION}          # 1.0.2，来自 project(Vector VERSION 1.0.2)
    COMPATIBILITY SameMajorVersion)
```

`COMPATIBILITY` 常用四值：

| 值 | 语义 | 谁该用 |
|---|---|---|
| `SameMajorVersion` | `1.0.2` 满足 `1.0`、`1.5` 的要求；不满足 `2.0` | 遵守语义化版本的库（本章用的） |
| `SameMinorVersion` | 只接受同一次版本号（`1.0.x` 只满足 `1.0`） | ABI 小版本敏感的库 |
| `ExactVersion` | 一字不差 | 插件、和某个构建强绑定的组件 |
| `AnyNewerVersion` | 不比它旧就行 | 纯头文件、向后兼容极稳的库 |

排查顺序建议固定成三步：

1. 报错里有版本号吗？没有 → 前缀问题（第 5 节）；有 → 版本问题。
2. `message(STATUS "${Vector_DIR}")` 打印出实际加载的目录，确认加载的是你以为的那份安装树。
3. 打开那个目录里的 `VectorConfigVersion.cmake`，看 `PACKAGE_VERSION_COMPATIBILITY` 是哪一种。

---

## 11. 本章最硬的一条报错：内部依赖没进导出集合 —— `PRIVATE` 也照样逃不掉

把 `VECTOR_EXPORT_UTIL` 关掉（等于所有教程里「内部依赖就不装了呗」的写法）：

```
$ cmake -S . -B build-noexport -DVECTOR_EXPORT_UTIL=OFF 2>&1 | grep -E "CMake Error|not in any export|Generating step failed" -A2 | head -10
CMake Error in lib/CMakeLists.txt:
  install(EXPORT "VectorTargets" ...) includes target "vector" which requires
  target "util" that is not in any export set.
```

**请把这条报错和「作用域」这个词联系到一起，再划掉「作用域」。**
它不区分 `PRIVATE` 和 `PUBLIC`（第 02 章那种「PRIVATE 就干净了」的直觉在这里失效）：
只要你 `target_link_libraries` 了，CMake 就要求你交代清楚那个 target 归谁交付。

真正的坏消息在后面三步 —— 这条报错**不会拦住你交付一个坏包**：

```
$ cmake --build build-noexport 2>&1 | tail -2
[100%] Linking C executable app
[100%] Built target app

$ cmake --install build-noexport --prefix "/tmp/cmake03-hAJIJq/prefix-noexport" 2>&1 | tail -3
-- Installing: /tmp/cmake03-hAJIJq/prefix-noexport/lib/cmake/Vector/VectorConfig.cmake
-- Installing: /tmp/cmake03-hAJIJq/prefix-noexport/lib/cmake/Vector/VectorConfigVersion.cmake
-- Installing: /tmp/cmake03-hAJIJq/prefix-noexport/bin/app
```

```
$ find "/tmp/cmake03-hAJIJq/prefix-noexport" -type f | sort
<沙箱>/prefix-noexport/bin/app
<沙箱>/prefix-noexport/include/vector.h
<沙箱>/prefix-noexport/lib/cmake/Vector/VectorConfig.cmake
<沙箱>/prefix-noexport/lib/cmake/Vector/VectorConfigVersion.cmake
<沙箱>/prefix-noexport/lib/cmake/Vector/VectorTargets-noconfig.cmake
<沙箱>/prefix-noexport/lib/cmake/Vector/VectorTargets.cmake
<沙箱>/prefix-noexport/lib/libvector.a
```

注意：`libutil.a` 不在了，但 `VectorTargets.cmake` **照样装出去了**，里面写着一条悬空引用：

```
$ grep -nE "add_library|LINK_LIBRARIES|INCLUDE_DIRECTORIES" /tmp/cmake03-hAJIJq/prefix-noexport/lib/cmake/Vector/VectorTargets.cmake
59:add_library(Vector::vector STATIC IMPORTED)
63:  INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include"
64:  INTERFACE_LINK_LIBRARIES "\$<LINK_ONLY:util>"
```

裸 `util`，没有 `Vector::util` 这个导入目标 —— 谁也不知道它指向哪。
然后 consumer 那边一路绿灯，直到链接：

```
$ cmake -S consumer -B bnoex -DCMAKE_PREFIX_PATH="/tmp/cmake03-hAJIJq/prefix-noexport" | grep '找到 Vector 的配置|Configuring done'
-- consumer 找到 Vector 的配置文件目录：/tmp/cmake03-hAJIJq/prefix-noexport/lib/cmake/Vector
-- Configuring done (0.2s)

$ cmake --build bnoex 2>&1 | grep -E 'undefined reference|ld returned|Error 1' | head -4
vector.c:(.text+0xb8): undefined reference to `u_clamp'
collect2: error: ld returned 1 exit status
gmake[2]: *** [CMakeFiles/consumer.dir/build.make:102: consumer] Error 1
```

把这三步连起来读，就是真实项目里最难查的一类事故：

1. **配置期**：报错明确、原因明确（`not in any export set`）。此时你**应该立刻修**，
   因为再过两步原因就查不出来了。
2. **构建/安装期**：因为报错发生在 generate 阶段，而 configure 已经成功、
   构建脚本大部分已落盘 —— 于是 `cmake --build` 和 `cmake --install` **都还是成功的**
   （CMake 只是宣布「Build files cannot be regenerated correctly」，并没有把你的旧脚本删掉）。
   你装出去一个半残包。
3. **使用期**：`find_package` 正常、编译正常，链接才炸，报的是
   `undefined reference to u_clamp` —— 报错里一个 `export`/`install` 字样都没有，
   症状和病因隔了两层楼。

> **硬规矩：导出集合必须覆盖整条依赖链。** 只要 link 过的 target，一个都不能漏。
> 如果你的内部依赖确实不该被导出，唯一干净的做法是让依赖边**不存在** —— 下一节。

---

## 12. 内部依赖的第三条路：OBJECT 库 + 不导出 = 彻底不外露

```
$ cmake -S . -B build-hide -DVECTOR_UTIL_AS_OBJECT=ON -DVECTOR_EXPORT_UTIL=OFF 2>&1 | grep -E "CMake Error|not in any export|Configuring done|Build files" | head -6
-- Configuring done (0.2s)
-- Build files have been written to: /tmp/cmake03-hAJIJq/proj/build-hide
```

配置期干干净净。对应的 CMakeLists 只有这几行：

```cmake
add_library(util OBJECT src/util.c)                      # 只产 .o，不产 .a
add_library(vector src/vector.c)
target_sources(vector PRIVATE $<TARGET_OBJECTS:util>)    # 把 .o 直接吸进来
target_include_directories(vector PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/util/include>)
```

物证：util 的代码**物理上就长在 `libvector.a` 里面**。

```
$ ar t build-hide/lib/libvector.a
vector.c.o
util.c.o

$ nm --defined-only build-hide/lib/libvector.a | grep u_clamp
0000000000000000 T u_clamp
```

```
$ cmake --install build-hide --prefix "/tmp/cmake03-hAJIJq/prefix-hide" | grep 'Installing'
-- Installing: /tmp/cmake03-hAJIJq/prefix-hide/lib/libvector.a
-- Installing: /tmp/cmake03-hAJIJq/prefix-hide/include
-- Installing: /tmp/cmake03-hAJIJq/prefix-hide/include/vector.h
-- Installing: /tmp/cmake03-hAJIJq/prefix-hide/lib/cmake/Vector/VectorTargets.cmake
-- Installing: /tmp/cmake03-hAJIJq/prefix-hide/lib/cmake/Vector/VectorTargets-noconfig.cmake
-- Installing: /tmp/cmake03-hAJIJq/prefix-hide/lib/cmake/Vector/VectorConfig.cmake
-- Installing: /tmp/cmake03-hAJIJq/prefix-hide/lib/cmake/Vector/VectorConfigVersion.cmake
-- Installing: /tmp/cmake03-hAJIJq/prefix-hide/bin/app

$ grep -nE "add_library|LINK_LIBRARIES|INCLUDE_DIRECTORIES" /tmp/cmake03-hAJIJq/prefix-hide/lib/cmake/Vector/VectorTargets.cmake
59:add_library(Vector::vector STATIC IMPORTED)
63:  INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include"

$ cmake -S consumer -B bhide -DCMAKE_PREFIX_PATH="/tmp/cmake03-hAJIJq/prefix-hide" >/dev/null && cmake --build bhide 2>&1 | tail -1 && ./bhide/consumer
[100%] Built target consumer
[consumer] 元素数 = 5，总和 = 55
```

安装树里没有 `libutil.a`、没有 `include/vector/util.h`；
导出文件里**连 `INTERFACE_LINK_LIBRARIES` 这一行都没有**；
consumer 配置、链接、运行全部正常。这才是「内部依赖真的不外露」。

三条路放一起对比，选型就是一道清楚的取舍题：

| 做法 | 配置期 | 安装树里 | consumer 能 include `util.h` | consumer 需要 `libutil.a` | 代价 |
|---|---|---|---|---|---|
| `STATIC` + `PRIVATE` + 导出（默认档） | ✅ | `libvector.a` + `libutil.a` + `include/vector/util.h` | ❌（只给链接） | **需要** | 多交付一个 `.a` 和一个头 |
| `STATIC` + 不导出 | ❌ **generate 报错** | 半残包 | — | — | 见第 11 节事故链 |
| `OBJECT` + 吸进 `.o` + 不导出 | ✅ | 只有 `libvector.a` + `vector.h` | ❌（头没装） | **不需要** | 见下面三条 |

`OBJECT` 那条路的代价必须写清楚，否则你会在别处踩坑：

1. **`-I` 没人替你传了。** `vector.c` 里那句 `#include "util.h"` 需要自己补
   `target_include_directories(vector PRIVATE …)`，忘了就是 `fatal error`。
2. **代码会被复制。** `.o` 是被搬进别人的产物里的。如果 `util` 同时被两个库用 `$<TARGET_OBJECTS:>` 吸走，
   同一份符号会出现两次；将来链接到一个可执行文件里就是 `multiple definition`。
3. **它骗不过 CMake 的依赖检查，只有「不 link」才骗得过。**
   验证过：`util` 编成 `OBJECT` 库但用 `target_link_libraries(vector PRIVATE util)`，
   照样报第 11 节那条 `not in any export set`。
   区别只在于报错之后 consumer 居然还能链接成功（因为符号确实已经在 `.a` 里了）——
   一个「坏包恰好能用」的状态，比「好包」更糟，因为它让你以为可以不导出依赖。

---

## 13. 换成共享库：产物、链接、加载三件事一起变

`BUILD_SHARED_LIBS=ON` 是 CMake 自带的标准开关，`add_library(vector …)` 一行都不用改。

```
$ cmake -S . -B build -DBUILD_SHARED_LIBS=ON | grep '库类型|Configuring done|Build files'
-- 工程 Vector 1.0.2，库类型 = ON（OFF=静态 .a，ON=共享 .so）
-- Configuring done (0.2s)
-- Build files have been written to: /tmp/cmake03-hAJIJq/shared/build

$ cmake --build build 2>&1 | tail -6
[ 50%] Building C object lib/CMakeFiles/vector.dir/src/vector.c.o
[ 66%] Linking C shared library libvector.so
[ 66%] Built target vector
[ 83%] Building C object app/CMakeFiles/app.dir/main.c.o
[100%] Linking C executable app
[100%] Built target app

$ find build \( -name '*.a' -o -name '*.so*' \) | sort
build/lib/libvector.so
build/util/libutil.a
```

导出文件发生了质变：

```
$ grep -nE "add_library|LINK_LIBRARIES|INCLUDE_DIRECTORIES" /tmp/cmake03-hAJIJq/prefix-shared/lib/cmake/Vector/VectorTargets.cmake
59:add_library(Vector::util STATIC IMPORTED)
62:  INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include/vector"
66:add_library(Vector::vector SHARED IMPORTED)
70:  INTERFACE_INCLUDE_DIRECTORIES "${_IMPORT_PREFIX}/include"
```

```
$ nm -D --defined-only "/tmp/cmake03-hAJIJq/prefix-shared/lib/libvector.so" | grep u_clamp
00000000000013a4 T u_clamp

$ cmake -S consumer -B bsh -DCMAKE_PREFIX_PATH="/tmp/cmake03-hAJIJq/prefix-shared" >/dev/null && cmake --build bsh 2>&1 | tail -1 && ./bsh/consumer
[100%] Built target consumer
[consumer] 元素数 = 5，总和 = 55

$ ldd ./bsh/consumer | grep -E "vector|not found" | sed "s|0x.*||"
	libvector.so => /tmp/cmake03-hAJIJq/prefix-shared/lib/libvector.so (
```

三个要点：

* `Vector::vector` 那一行**没有 `INTERFACE_LINK_LIBRARIES`**。
  共享库在链接那一刻已经把 `util` 的代码吸进 `.so` 里了（`nm -D` 显示 `u_clamp` 是它自己**定义**的符号），
  所以消费者链接期不再需要 `libutil.a`。这是共享库和静态库在「交付」上最大的区别：
  **静态库交付的是「引用清单」，共享库交付的是「成品」。**
* 但 `libutil.a` 依然被装了 —— 因为 `VECTOR_EXPORT_UTIL` 还是 `ON`。
  CMake 不知道你物理上已经自洽，它只按依赖图检查交代；这条边还在，就得还在导出集合里。
* `./bsh/consumer` 能跑，是因为它**在构建树里**，CMake 给它写了构建期 RPATH，
  `ldd` 那一行解析到的正是 `<沙箱>/prefix-shared/lib/libvector.so`。

那装出去的 `app` 呢？它已经不在构建目录里了：

```
$ ldd "/tmp/cmake03-hAJIJq/prefix-shared/bin/app" | grep -E 'vector|not found' | sed "s|0x.*||"
	libvector.so => not found

$ { "/tmp/cmake03-hAJIJq/prefix-shared/bin/app"; echo "退出码=$?"; } 2>&1 | head -2
/tmp/cmake03-hAJIJq/prefix-shared/bin/app: error while loading shared libraries: libvector.so: cannot open shared object file: No such file or directory
退出码=127

$ readelf -d "/tmp/cmake03-hAJIJq/prefix-shared/bin/app" | grep -E 'RPATH|RUNPATH' || echo '（这个二进制里一个 RPATH/RUNPATH 字段都没有）'
（这个二进制里一个 RPATH/RUNPATH 字段都没有）
```

（第二条里 `退出码=127` 是被测程序自己的返回码，外面的 `[退出码 0]` 是整条管道的 —— 花括号+echo 这一招就是为了让两者都显示出来。）

事故是**安装这一步造成的**。回看 `cmake --install` 的输出，最后一句：

```
-- Set non-toolchain portion of runtime path of "/tmp/cmake03-hAJIJq/prefix-shared/bin/app" to ""
```

意思是 CMake 故意把构建期的 RPATH 擦掉了（构建期它指向源码树旁边那个 `.so`）。
擦掉之后没人告诉动态链接器 `libvector.so` 在哪，于是 `ldd` 报 `not found`，
执行时 `error while loading shared libraries`，连 `main` 都没进去，退出码 127。
**这种失败没有任何 CMake 报错**：它发生在运行期，第一个遇到它的人是你的用户。

正规解法有三种，本章现场演示第一种：

| 解法 | 怎么做 | 适用 |
|---|---|---|
| ① 安装时写对 RPATH | `set_target_properties(app PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")` | 整个前缀可搬迁的绿色打包（`$ORIGIN` = 该可执行文件自己所在目录） |
| ② 交给系统 | 库目录写进 `/etc/ld.so.conf.d/` 再 `ldconfig` | 正式装进系统的发行版包，需要 root |
| ③ 临时救场 | `LD_LIBRARY_PATH=<前缀>/lib ./bin/app` | 调试。**别写进服务启动脚本**：它会污染所有子进程的解析 |

注意 `RPATH`/`RUNPATH` 是**链接器**的语法，不是 shell 变量，`$` 不该被 CMake 展开 ——
所以在 CMakeLists 里要写成 `"$ORIGIN/../lib"`，而 CMake 会原样传给链接器。

现场修复（往沙箱里那份 `app/CMakeLists.txt` 追加一行）：

```
$ printf '%s\n' 'set_target_properties(app PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")' >> app/CMakeLists.txt && tail -1 app/CMakeLists.txt
set_target_properties(app PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")

$ cmake --build build 2>&1 | grep -E 'Configuring done|Linking C executable|Built target app'
-- Configuring done (0.0s)
[100%] Built target app

$ cmake --install build --prefix "/tmp/cmake03-hAJIJq/prefix-shared" 2>&1 | tail -2
-- Installing: /tmp/cmake03-hAJIJq/prefix-shared/bin/app
-- Set non-toolchain portion of runtime path of "/tmp/cmake03-hAJIJq/prefix-shared/bin/app" to "$ORIGIN/../lib"

$ readelf -d "/tmp/cmake03-hAJIJq/prefix-shared/bin/app" | grep -E 'RPATH|RUNPATH'
 0x000000000000001d (RUNPATH)            Library runpath: [$ORIGIN/../lib]

$ { "/tmp/cmake03-hAJIJq/prefix-shared/bin/app"; echo "退出码=$?"; } 2>&1 | head -3
表里有 6 个元素：3 6 9 12 15 18
总和 = 63
退出码=0
```

这里有一个非常值得记住的机制细节：**`cmake --build` 那一屏里没有出现 `Linking C executable`，
程序却变了。** RPATH 是 `cmake --install` 在拷贝文件之后**改写 ELF 头部**得到的
（内部走 `file(RPATH_CHANGE …)`），不需要重新链接。
所以「改 RPATH → 必须重编」是错的直觉；真正的因果在 install 那一步。
这也是为什么 RPATH 问题在本机上很难复现：只 `cmake --build` 不 `cmake --install`，永远看不到。

顶层那句 `set(CMAKE_POSITION_INDEPENDENT_CODE ON)` 是这一节能链接成功的前提：
`libutil.a` 的机器码要被吸进 `.so`，就必须是位置无关的。
少了它，`BUILD_SHARED_LIBS=ON` 会在链接 `.so` 时报
`relocation R_X86_64_32S … can not be used when making a shared object`。

---

## 14. 裸绝对路径的公开头目录：同树一个字不报错，一导出就炸

第 02 章结尾埋的那颗雷，在这里拆（下面的源码是第 13 节复制出来的那份 `shared/`）：

```
$ cmake -S . -B build-plain -DVECTOR_PLAIN_INCLUDE=ON 2>&1 | grep -E "CMake Error" -A5 | head -12
CMake Error in lib/CMakeLists.txt:
  Target "vector" INTERFACE_INCLUDE_DIRECTORIES property contains path:

    "/tmp/cmake03-hAJIJq/shared/lib/include"

  which is prefixed in the source directory.

$ cmake --build build-plain 2>&1 | tail -2
[100%] Linking C executable app
[100%] Built target app
```

报错说的是：要导出的 target 身上有个 `INTERFACE_INCLUDE_DIRECTORIES` 指向**源码树里的路径**，
而 `install(EXPORT)` 拒绝把它写进交付给别人的文件 —— 那个路径只存在于你这台机器上。

同树编译阶段一个字错都没有（上面照样 `Built target app`）：`app` 只需要
「这个目录里有个 `vector.h`」，它确实有。区别只在于 `app` 站在源码树里，而 consumer 站在安装树里。

所以这一条现代写法不是为了好看：

```cmake
target_include_directories(vector PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>   # 构建树里的人
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)        # 装完之后的人
```

| 写法 | 同树（app） | 交付（consumer） |
|---|---|---|
| 裸绝对路径 | ✅ 能用 | ❌ generate 阶段报错 |
| 只有 `$<BUILD_INTERFACE:…>` | ✅ 能用 | ⚠️ 不报错，但导出文件里没有头目录 → 消费者 `#include "vector.h"` 失败 |
| BUILD + INSTALL 都写（推荐） | ✅ | ✅ |

中间那一档比裸路径更阴险：**它不报错**，症状和第 5 节一样是「找不到头」，
而你要翻到消费者的 `flags.make` 才发现 `-I` 那一栏是空的。

---

## 15. `DESTDIR`：打包脚本为什么能把库「装」进暂存目录

先看一个**不存在**的选项：

```
$ cmake --install build --prefix /usr/local --destdir "/tmp/cmake03-hAJIJq/stage" 2>&1 | head -6
Unknown argument --destdir
Usage: cmake --install <dir> [options]
Options:
  <dir>              = Project binary directory to install.
  --config <cfg>     = For multi-configuration tools, choose <cfg>.
  --component <comp> = Component-based install. Only install <comp>.
```

这台机器的 CMake 4.2.3 的 `cmake --install` **没有** `--destdir`（用法列表里搜不到它）。
暂存目录要靠环境变量 `DESTDIR`：

```
$ DESTDIR="/tmp/cmake03-hAJIJq/stage" cmake --install build --prefix /usr/local 2>&1 | tail -4
-- Installing: /tmp/cmake03-hAJIJq/stage/usr/local/lib/cmake/Vector/VectorConfig.cmake
-- Installing: /tmp/cmake03-hAJIJq/stage/usr/local/lib/cmake/Vector/VectorConfigVersion.cmake
-- Installing: /tmp/cmake03-hAJIJq/stage/usr/local/bin/app
-- Set non-toolchain portion of runtime path of "/tmp/cmake03-hAJIJq/stage/usr/local/bin/app" to "$ORIGIN/../lib"

$ find "/tmp/cmake03-hAJIJq/stage" -type f | sort | sed "s|/tmp/cmake03-hAJIJq|<沙箱>|g" | head -10
<沙箱>/stage/usr/local/bin/app
<沙箱>/stage/usr/local/include/vector.h
<沙箱>/stage/usr/local/include/vector/util.h
<沙箱>/stage/usr/local/lib/cmake/Vector/VectorConfig.cmake
<沙箱>/stage/usr/local/lib/cmake/Vector/VectorConfigVersion.cmake
<沙箱>/stage/usr/local/lib/cmake/Vector/VectorTargets-noconfig.cmake
<沙箱>/stage/usr/local/lib/cmake/Vector/VectorTargets.cmake
<沙箱>/stage/usr/local/lib/libutil.a
<沙箱>/stage/usr/local/lib/libvector.so

$ ls /usr/local/lib /usr/local/include 2>/dev/null | grep -iE 'vector|libutil' || echo '（/usr/local 里搜不到任何 vector 相关的东西 —— 确实一个字都没写进去）'
（/usr/local 里搜不到任何 vector 相关的东西 —— 确实一个字都没写进去）
```

机制：`--prefix` 声明「最终会落在 `/usr/local`」，`DESTDIR` 把每一层路径**前面再加一个暂存根目录**。
打 `.deb`/`.rpm`/tar 包就是这么干的：先 `DESTDIR` 到 staging，检查内容清单，
再由打包工具搬进真系统。好处是「装到哪里」这件事在打包时就能被完整验证，
而不需要真的去动系统目录。
（这一节用的是第 13 节那份 `shared/` 构建目录，所以 stage 里是 `libvector.so`，
而且它已经带着上一节刚加上的 `RUNPATH=$ORIGIN/../lib` —— 两节课的内容在这份清单上会合了。）

---

## 16. 十条坑（前八条本章现场复现过，后两条是配套的）

1. **`PRIVATE` 也要求依赖被导出。** `install(EXPORT)` 不看作用域，只看依赖边存不存在（第 11 节）。
2. **generate 失败不等于构建失败。** configure 已经成功、构建脚本已落盘时，
   `cmake --build`/`cmake --install` 可能照样成功，交付出一个半残包（第 11 节第 ② 步）。
   所以 CI 里必须检查 `configure` 的退出码，而不是只看 `build` 和 `install`。
3. **导出集合里的 target 名字必须能被解析。** 半残包里那行 `$<LINK_ONLY:util>` 就是反面教材：
   既没有 `Vector::util`，也没有 `util` 这个 target，报错要等到链接期才出现。
4. **裸绝对路径的头目录 = 交付毒药。** 同树永远正常，`install(EXPORT)` 才炸（第 14 节）。
5. **只写 `$<BUILD_INTERFACE>` 更阴险**：不报错，交付后消费者的 `-I` 是空的。
6. **`install(DIRECTORY include/ …)` 结尾那个斜杠**。写了装「目录里的内容」，
   不写连目录名一起装，你会得到 `include/include/vector.h`（第 4 节）。
7. **`--prefix` 指真系统目录要小心。** 本章全部 `install` 都在沙箱前缀里；
   想验证「正式装到 `/usr/local` 会怎样」用 `DESTDIR` 而不是真装（第 15 节）。
8. **共享库装完跑不起来，八成是 RPATH。** `cmake --install` 会主动擦掉构建期 RPATH，
   没设 `INSTALL_RPATH` 就等着 `error while loading shared libraries`（第 13 节）。
9. **`find_package` 报「找不到」和报「版本不兼容」是两种病**，
   前者改前缀，后者改版本号或 `COMPATIBILITY`。别拿后者去改版本号（第 10 节）。
10. **`add_library(x INTERFACE src.c)` 静默什么都不做**（第 02 章验证过）：
    `INTERFACE` 库按定义「没有自己的构建产物」，源码会被无声忽略。
    给 INTERFACE 库加东西只能加 usage requirements（头目录、宏、编译特性、`INTERFACE_*` 属性）。

还有一条不在本章主线里但顺手记住：**`cmake --install` 没有 `--destdir`**（第 15 节），
`DESTDIR` 是环境变量。

---

## 17. 自检：能答上这 8 题就算过了

1. `install(TARGETS vector ARCHIVE/LIBRARY/RUNTIME …)` 三行里，Linux 静态库命中哪一行？Windows 的 `.dll` 命中哪一行？
   <sub>答：`.a` 命中 `ARCHIVE`，`.so` 命中 `LIBRARY`，`.dll` 命中 `RUNTIME`（Windows 把 dll 当「可执行运行时」）。三行都写，同一份文件跨平台不用改。</sub>
2. 为什么 consumer 链接时需要 `libutil.a`，即使 `vector` 是用 `PRIVATE util` 链接的？
   <sub>答：静态库 `.a` 只是被引用，`u_clamp` 的代码不在 `libvector.a` 里；`PRIVATE` 只切断编译期传染，链接期要求被写成 `$<LINK_ONLY:Vector::util>` 照样传染。</sub>
3. `find_package(Vector REQUIRED)` 和 `find_package(Vector)`，你会选哪个？为什么？
   <sub>答：硬依赖一律 `REQUIRED`。不写时 `Vector_FOUND=FALSE` 但配置继续，报错会推迟到 `target_link_libraries` 那行，离病因几十行。</sub>
4. 消费者报 `fatal error: util.h: No such file or directory`，而 `find` 证明安装树里**有** `include/vector/util.h`。两句话怎么同时成立？
   <sub>答：文件存在 ≠ 路径在编译命令行里。`PRIVATE` 时那个目录没被传染成 `-I`，编译器根本不去那里找。</sub>
5. 你只改了 `INSTALL_RPATH`，`cmake --build` 没有重新链接，可执行文件却修好了。为什么？
   <sub>答：RPATH 是 `cmake --install` 用 `file(RPATH_CHANGE)` 改写 ELF 头部得到的，与链接无关。</sub>
6. `install(EXPORT)` 报错之后仍然安装成功了，这说明什么？你该怎么防？
   <sub>答：报错在 generate 阶段，configure 已成功、脚本已落盘。CI 里要判断 `cmake -S -B` 的退出码，且把 `--debug-trycompile`/`--fresh` 之类的干净重建纳入流程。</sub>
7. 想把内部依赖彻底藏起来，`OBJECT` 库那条路的三条代价是什么？
   <sub>答：`-I` 要自己补；同一份代码可能被复制进多个库（重复符号风险）；且必须「不 link」才有效 —— 只是把 `util` 编成 OBJECT 但仍然 `target_link_libraries` 的话，导出检查照样报错。</sub>
8. 一个 `.so` 交付出去了，`nm -D` 显示它自己定义了 `u_clamp`，导出文件里也没有 `INTERFACE_LINK_LIBRARIES`。这两件事之间有因果关系吗？
   <sub>答：有。`PRIVATE` 依赖在链接 `.so` 时被真的吸进产物，因此对外不再需要声明依赖；静态库做不到这点，所以它必须把依赖交代给导出文件。</sub>

---

## 18. 下一章预告

本章把「一个库自己怎么交付」讲完了。但真实工程里 `find_package` 找的不只是**你自己写的**库：
`OpenSSL`、`ZLIB`、`Threads`、`fmt`、`googletest`……
第 04 章《依赖：`find_package`、`FetchContent` 和找不到时怎么办》会回答三个问题：

* Module 模式（`FindXxx.cmake`）和 Config 模式（`XxxConfig.cmake`）到底差在哪，
  本章用的是哪一种，为什么；
* `find_package(Threads REQUIRED)` 为什么是 CMake 里最像「魔法」的一句；
* 依赖没装、装错版本、装了两份的时候，`FetchContent` 与 vendoring 各自的取舍。

同时本章第 11 节那条「报错在前、症状在后」的链条，会在第 06 章被 CI 闸门彻底封死
（`ctest` + Sanitizer + `CMakePresets`）。
