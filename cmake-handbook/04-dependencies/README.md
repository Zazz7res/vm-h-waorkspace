# 依赖：同一份 mathx，四条到货路线

> 这一章只回答一个问题：**别人写好的库，怎么进到我的构建里？**
> 我们准备了一个叫 `mathx` 的第三方库，把它造成**两种形状的上游**（一种只给 `.a` 和头，
> 一种还自带 CMake 配置文件），然后用四条完全不同的路线装进同一个 `consumer` 工程，
> 每一条都当场演"找到 / 找不到 / 版本对不对 / 到底谁应答的"。
> 下面**每一段输出都是这台机器上真实跑出来的**（cmake 4.2.3 + GNU Make 4.4.1 + gcc 15.2.0 + git 2.55.0），
> 跑法见文末「怎么跑这章的 demo」。
> 输出里的 `/tmp/cmake04-xoPsG7/...` 是 `mktemp -d` 每次随机生成的沙箱路径，你跑出来会是 `cmake04-<另外 6 位>`，
> 除此之外应当逐字节一致 —— 本章连 tar 包的 sha256 都是固定的（第 12 节自证）。
> 安全边界：**全程不联网**（FetchContent 只走 `file://`），`--prefix` 永不指向 `/usr/local`，
> 打包演示只用 `DESTDIR=` 暂存。

---

## 你会遇到的场景

先对上号，这些是不是你的现场：

1. 你 `find_package(OpenSSL)`，报 `Could not find a package configuration file`，
   你 `apt install` 了、库确实在 `/usr/lib`，它就是说没找到。
2. 教程里写 `find_package(XYZ REQUIRED)`，你加了 `CONFIG` 关键字，报错文案变了，你不知道该看哪个。
3. 同一个包，你机器上用的是系统那份，CI 上用的是 FetchContent 拉的那份，行为不一样，
   而你 `CMakeLists.txt` 里只有一行 `find_package`。
4. 你写 `find_package(mathx 2.0 REQUIRED)`，本地只有 1.3.0，configure 却**成功**了 ——
   直到运行时才炸。
5. 你 `find_package(Threads REQUIRED)`，链接行里连 `-pthread` 都没有，你以为这行没生效。
6. 你用 FetchContent 拉了个库，`rm -rf build` 之后 configure 突然变慢，或者内网机器直接卡死。
7. 你把上游源码 `add_subdirectory` 进来（vendoring），结果你 `install` 出来的包里
   混进了上游的 `.a` 和上游的 `xxxConfig.cmake`。
8. 你只想"手动指定一下路径"，试了 `<包名>_CONFIG`、`<包名>_DIR`，一个有效一个没用，你不知道为什么。

这 8 个现场的答案，全都在"两条模式 + 两条到货时机"这个框架里。

---

## 心智模型：`find_package` 是一次面试，有两种方言

`find_package(mathx)` 这一行其实同时在问两个人要答案：

```
        find_package(mathx [版本] [REQUIRED] [CONFIG|MODULE])
                     │
        ┌────────────┴────────────┐
        ▼                         ▼
 ┌──────────────┐          ┌────────────────┐
 │ Module 模式  │          │  Config 模式   │
 │ 调用方写的   │          │  上游自带的    │
 │ Find<包>.cmake│         │ <包>Config.cmake│
 │ 「我教你怎么找│          │ 「我自己来说   │
 │  这个包」    │          │  我在哪」      │
 └──────────────┘          └────────────────┘
   产物：MATHX_INCLUDE_DIR      产物：mathx::mathx 这类
        MATHX_LIBRARY            IMPORTED 目标 + 若干变量
        + 手造的 IMPORTED 目标
```

不写关键字时是 **AUTO**：先试 Module，试不到再试 Config —— 而且这个降级是**静默**的。
一句话记住本章：

> **`find_package` 的成功输出里，没有任何一行写着"我走的是哪条模式"。**
> 所以本章的核心技能是：**给两种模式各挂一块铭牌，然后看铭牌。**

这就是我们为什么要自己写 `Findmathx.cmake`（铭牌是 `[Findmathx.cmake] Module 模式应答`），
以及为什么上游的 `mathxConfig.cmake.in` 里要留一句 `message`（铭牌是 `[mathxConfig.cmake] Config 模式应答`）。

---

## 第 0 步：先造出"两种形状的上游"

`mathx/CMakeLists.txt` 里只有一个拨档：

```cmake
option(MX_EXPORT "装出去时是否附送 mathxConfig.cmake（ON=现代上游，OFF=上古只给 .a 和头）" ON)
```

装两次，分别落到两个沙箱前缀：

```
$ cmake -S mathx -B b-bare -DMX_EXPORT=OFF | grep -E "MX_EXPORT|Configuring done|Build files"
-- Configuring done (0.2s)
-- Build files have been written to: /tmp/cmake04-xoPsG7/proj/b-bare
[退出码 0]

$ cmake --install b-bare --prefix "/tmp/cmake04-xoPsG7/sysroot-bare" | tail -4
-- Install configuration: ""
-- Installing: /tmp/cmake04-xoPsG7/sysroot-bare/lib/libmathx.a
-- Installing: /tmp/cmake04-xoPsG7/sysroot-bare/include
-- Installing: /tmp/cmake04-xoPsG7/sysroot-bare/include/mathx.h
[退出码 0]

$ cmake --install b-cfg --prefix "/tmp/cmake04-xoPsG7/sysroot-cfg" | tail -6
-- Installing: /tmp/cmake04-xoPsG7/sysroot-cfg/include
-- Installing: /tmp/cmake04-xoPsG7/sysroot-cfg/include/mathx.h
-- Installing: /tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx/mathxConfig.cmake
-- Installing: /tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx/mathxConfigVersion.cmake
-- Installing: /tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx/mathxTargets.cmake
-- Installing: /tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx/mathxTargets-noconfig.cmake
[退出码 0]

$ find "/tmp/cmake04-xoPsG7/sysroot-bare" "/tmp/cmake04-xoPsG7/sysroot-cfg" -type f | sed "s|/tmp/cmake04-xoPsG7/||" | sort | grep -E "."
sysroot-bare/include/mathx.h
sysroot-bare/lib/libmathx.a
sysroot-cfg/include/mathx.h
sysroot-cfg/lib/cmake/mathx/mathxConfig.cmake
sysroot-cfg/lib/cmake/mathx/mathxConfigVersion.cmake
sysroot-cfg/lib/cmake/mathx/mathxTargets-noconfig.cmake
sysroot-cfg/lib/cmake/mathx/mathxTargets.cmake
sysroot-cfg/lib/libmathx.a
```

两棵安装树的差集，就是 Config 模式的**全部物质基础**：

```
    sysroot-bare/  lib/libmathx.a + include/mathx.h                （上古：只给二进制和头）
    sysroot-cfg/   还多 lib/cmake/mathx/{mathxConfig,mathxConfigVersion,mathxTargets*}.cmake
```

第 03 章我们花整章**生成**这三个 `.cmake`；这一章从另一侧看：**没有**它们时 `find_package` 靠什么活？
靠调用方自己写的 `Find<包>.cmake`。

顺带注意 `lib/cmake/<包名>/` 这个层级 —— 它是 Config 模式的固定住址，第 15 节的报错原文会亲口告诉你。

---

## 路线①：Config 模式（现代默认）—— 上游自带说明书

```
$ cmake -S consumer -B c-cfg -DCMAKE_PREFIX_PATH="/tmp/cmake04-xoPsG7/sysroot-cfg" -DUSE_OUR_MODULE=OFF       | grep -E '应答|mathx_DIR|头 =|库 =|Configuring done'
-- [mathxConfig.cmake] Config 模式应答：/tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx
-- mathx_DIR        = /tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx   (Config 模式由它指到配置文件目录)
-- 头 = 
-- 库 = 
-- Configuring done (0.1s)
[退出码 0]

$ ./c-cfg/consumer
[consumer] mathx 1.3.0 在场
[consumer] mx_clamp(999,0,10) = 10
[consumer] mx_gcd(48,180)     = 12
[consumer] mx_hash(a,5)       = 58503557
[退出码 0]
```

三个看点：

* 铭牌 `[mathxConfig.cmake] Config 模式应答` 是上游那份 `.cmake.in` 里的 `message` 打出来的 ——
  **不是 CMake 说的，是包自己说的**。
* `mathx_DIR` 指向 `lib/cmake/mathx`，它就是 `find_package` 实际加载配置文件的目录。
* **`头 =` 和 `库 =` 两行是空的。** Config 模式只造 `mathx::mathx` 这个目标，
  压根不填 `MATHX_INCLUDE_DIR` / `MATHX_LIBRARY` 这类旧式变量。

最后这条很关键：如果你照老教程写

```cmake
include_directories(${MATHX_INCLUDE_DIR})     # Config 模式下这里是空的
target_link_libraries(app ${MATHX_LIBRARY})
```

那你的工程**只能在 Module 模式下工作**。这就是"现代语法"和"老写法"在依赖层最直接的差别：
`target_link_libraries(consumer PRIVATE mathx::mathx)` 这一行，在两种模式下都能成立。

---

## 路线②：Module 模式 —— 上游什么都没给，我们的 Find 脚本上场

`cmake/modules/Findmathx.cmake` 是本章唯一的"新代码量"，它干四件事：
`find_path` 找头、`find_library` 找库、从 `mathx.h` 里**正则抠版本号**、造 `UNKNOWN IMPORTED` 目标。

```
$ cmake -S consumer -B c-bare -DCMAKE_PREFIX_PATH="/tmp/cmake04-xoPsG7/sysroot-bare"       | grep -E '应答|最低版本|mathx_VERSION|交付|头 =|库 ='
-- [Findmathx.cmake] Module 模式应答：这个文件就是我，正在被 include 执行
-- [Findmathx.cmake] 调用方要求的最低版本 = 1.0（CMake 自己不比较）
-- [Findmathx.cmake] 交付：mathx::mathx = /tmp/cmake04-xoPsG7/sysroot-bare/lib/libmathx.a（版本 1.3.0）
-- mathx_VERSION    = 1.3.0   (Module 模式由 Find 脚本填)
-- 头 = /tmp/cmake04-xoPsG7/sysroot-bare/include
-- 库 = /tmp/cmake04-xoPsG7/sysroot-bare/lib/libmathx.a
[退出码 0]

$ cat c-bare/CMakeFiles/consumer.dir/link.txt
/usr/bin/cc -Wl,--dependency-file=CMakeFiles/consumer.dir/link.d CMakeFiles/consumer.dir/main.c.o -o consumer  /tmp/cmake04-xoPsG7/sysroot-bare/lib/libmathx.a
[退出码 0]
```

对比上一节：**这次 `头 =` / `库 =` 有值了**，因为填它们的就是我们自己的模块。

同一个 `consumer/CMakeLists.txt` 一个字没改，只换了 `CMAKE_PREFIX_PATH`，因为：

* AUTO 模式（`find_package` 不写关键字）先找 `Findmathx.cmake`，找到就用它；
* 我们的模块在 bare 前缀下 `find_path`/`find_library` 命中，造出 IMPORTED 目标；
* 所以 `target_link_libraries(consumer PRIVATE mathx::mathx)` 这条写法**两种模式完全一样** ——
  这就是"用 target 而不是用变量"最大的好处：换到货方式不用改链接行。

版本号是怎么来的？不是 pkg-config，也不是 `--version`，是从头文件里 grep 出来的：

```cmake
file(STRINGS "${MATHX_INCLUDE_DIR}/mathx.h" _mx_ver REGEX "^#define MATHX_VERSION \"[^\"]*\"$")
```

这招不是我发明的 —— 第 10 节会看到官方 `FindZLIB.cmake` 用的就是同一行写法。

---

## 两种模式都能找到时，谁赢：搜索顺序现场裁决

把两个前缀一起给（`CMAKE_PREFIX_PATH` 用 `;` 分隔），模块也在：

```
$ cmake -S consumer -B c-both -DCMAKE_PREFIX_PATH="/tmp/cmake04-xoPsG7/sysroot-bare;/tmp/cmake04-xoPsG7/sysroot-cfg"       | grep -E '应答|库 ='
-- [Findmathx.cmake] Module 模式应答：这个文件就是我，正在被 include 执行
-- 库 = /tmp/cmake04-xoPsG7/sysroot-bare/lib/libmathx.a
[退出码 0]
```

**Module 赢** —— `CMAKE_MODULE_PATH` 命中就轮不到 Config。现在把我们的模块撤掉：

```
$ cmake -S consumer -B c-both2 -DCMAKE_PREFIX_PATH="/tmp/cmake04-xoPsG7/sysroot-bare;/tmp/cmake04-xoPsG7/sysroot-cfg" -DUSE_OUR_MODULE=OFF       | grep -E '应答|mathx_DIR|库 ='
-- [mathxConfig.cmake] Config 模式应答：/tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx
-- mathx_DIR        = /tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx   (Config 模式由它指到配置文件目录)
-- 库 = 
[退出码 0]
```

模块找不到 → **自动降级**去走 Config，`库 =` 立刻变空。

这个降级是 CMake 的善意（老包新包混在一个前缀里也能都工作），
但它同时制造了一类经典疑难杂症：**"我明明装了，为什么用的是这份？"**
判断方法本章教了两次 —— 看铭牌，以及看 `头 =`/`库 =` 填没填。
第 15 节给出正式的排查流程。

---

## 两个关键字各管一头：`CONFIG` / `MODULE` 强制走一条路

强制 CONFIG，但前缀里根本没有配置文件：

```
$ cmake -S consumer -B c-force1 -DCMAKE_PREFIX_PATH="/tmp/cmake04-xoPsG7/sysroot-bare" -DMODE=CONFIG
-- 拨档回执：MODE=CONFIG WANT_VERSION=1.0 USE_OUR_MODULE=ON
-- CMAKE_MODULE_PATH[0] = /tmp/cmake04-xoPsG7/proj/consumer/../cmake/modules
-- CMAKE_PREFIX_PATH    = /tmp/cmake04-xoPsG7/sysroot-bare
CMake Error at CMakeLists.txt:44 (find_package):
  Could not find a package configuration file provided by "mathx" (requested
  version 1.0) with any of the following names:

    mathxConfig.cmake
    mathx-config.cmake

  Add the installation prefix of "mathx" to CMAKE_PREFIX_PATH or set
  "mathx_DIR" to a directory containing one of the above files.  If "mathx"
  provides a separate development package or SDK, be sure it has been
  installed.


-- Configuring incomplete, errors occurred!
[退出码 1]
```

强制 MODULE，但 `CMAKE_MODULE_PATH` 里没有 `Findmathx.cmake`：

```
$ cmake -S consumer -B c-force2 -DCMAKE_PREFIX_PATH="/tmp/cmake04-xoPsG7/sysroot-cfg" -DMODE=MODULE -DUSE_OUR_MODULE=OFF       | grep -vE '^This warning|^$' | sed -n '1,14p'
CMake Error at CMakeLists.txt:42 (find_package):
  No "Findmathx.cmake" found in CMAKE_MODULE_PATH.


CMake Warning (dev) at CMakeLists.txt:42 (find_package):
  Findmathx.cmake must either be part of this project itself, in this case
  adjust CMAKE_MODULE_PATH so that it points to the correct location inside
  its source tree.

  Or it must be installed by a package which has already been found via
  find_package().  In this case make sure that package has indeed been found
  and adjust CMAKE_MODULE_PATH to contain the location where that package has
  installed Findmathx.cmake.  This must be a location provided by that
  package.  This error in general means that the buildsystem of this project
  is relying on a Find-module without ensuring that it is actually available.
```

两条错误**长得完全不一样**，因为它们问的是两个人：
`Could not find a package configuration file`（上游没给配置文件）
vs `No "Findmathx.cmake" found in CMAKE_MODULE_PATH`（我自己没写模块）。
看报错文案就能反推 CMake 当时在走哪条模式 —— 这是免费的信息。

**为什么有人愿意写 `CONFIG REQUIRED`？**
因为 `find_package(x CONFIG)` 明确"只信上游自带的配置文件"，
不会被某个第三方塞进 `CMAKE_MODULE_PATH` 的同名 Find 模块劫持。
现代工程里 `"CONFIG REQUIRED"` 越来越常见，就是这个原因。

---

## 本章签名结论：版本裁判是不对称的

同一个要求（`find_package(mathx 9.9 REQUIRED)`），面对同一份实际版本 1.3.0，
**两条模式的结局完全相反**。

### Config 模式：硬闸门

```
$ cmake -S consumer -B v-cfg99 -DCMAKE_PREFIX_PATH="/tmp/cmake04-xoPsG7/sysroot-cfg" -DUSE_OUR_MODULE=OFF -DWANT_VERSION=9.9       | sed -n '/CMake Error/,/installed/p'
CMake Error at CMakeLists.txt:40 (find_package):
  Could not find a configuration file for package "mathx" that is compatible
  with requested version "9.9".

  The following configuration files were considered but not accepted:

    /tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx/mathxConfig.cmake, version: 1.3.0
      The version found is not compatible with the version requested.



[退出码 1]
```

裁判是谁？是第 03 章 `write_basic_package_version_file(... SameMajorVersion)` 生成的那个文件：

```
$ sed -n '1,12p' "/tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx/mathxConfigVersion.cmake"
# This is a basic version file for the Config-mode of find_package().
# It is used by write_basic_package_version_file() as input file for configure_file()
# to create a version-file which can be installed along a config.cmake file.
#
# The created file sets PACKAGE_VERSION_EXACT if the current version string and
# the requested version string are exactly the same and it sets
# PACKAGE_VERSION_COMPATIBLE if the current version is >= requested version,
# but only if the requested major version is the same as the current one.
# The variable CVF_VERSION must be set before calling configure_file().


set(PACKAGE_VERSION "1.3.0")
[退出码 0]
```

注释里那句话就是规则本体：**主版本号必须相同，且装出去的版本 ≥ 要求的版本**。
所以 1.3.0 遇到要求 1.0 放行、遇到 9.9 拒绝：

```
$ cmake -S consumer -B v-cfg10 -DCMAKE_PREFIX_PATH="/tmp/cmake04-xoPsG7/sysroot-cfg" -DUSE_OUR_MODULE=OFF -DWANT_VERSION=1.0       | grep -E '应答|Configuring done'
-- [mathxConfig.cmake] Config 模式应答：/tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx
-- Configuring done (0.1s)
[退出码 0]
```

### Module 模式：良心活

模块写得**对**（带 `VERSION_VAR`）时，照样拦得住：

```
$ cmake -S consumer -B v-mod99 -DCMAKE_PREFIX_PATH="/tmp/cmake04-xoPsG7/sysroot-bare" -DMXM_STRICT_VERSION=ON -DWANT_VERSION=9.9       | sed -n '/CMake Error/,/unsuitable/p'
CMake Error at /usr/share/cmake-4.2/Modules/FindPackageHandleStandardArgs.cmake:290 (message):
  Could NOT find mathx: Found unsuitable version "1.3.0", but required is at
  least "9.9" (found /tmp/cmake04-xoPsG7/sysroot-bare/lib/libmathx.a)
Call Stack (most recent call first):
  /usr/share/cmake-4.2/Modules/FindPackageHandleStandardArgs.cmake:652 (_FPHSA_FAILURE_MESSAGE)
  /tmp/cmake04-xoPsG7/proj/cmake/modules/Findmathx.cmake:55 (find_package_handle_standard_args)
  CMakeLists.txt:40 (find_package)
```

现在把同一个模块里那一行的 `VERSION_VAR` 关掉（`MXM_STRICT_VERSION=OFF`）：

```
$ cmake -S consumer -B v-loose -DCMAKE_PREFIX_PATH="/tmp/cmake04-xoPsG7/sysroot-bare" -DMXM_STRICT_VERSION=OFF -DWANT_VERSION=9.9       | grep -E '最低版本|mathx_FOUND|库 =|Configuring done'
-- [Findmathx.cmake] 调用方要求的最低版本 = 9.9（CMake 自己不比较）
-- mathx_FOUND      = TRUE
-- 库 = /tmp/cmake04-xoPsG7/sysroot-bare/lib/libmathx.a
-- Configuring done (0.2s)
[退出码 0]

$ cmake --build v-loose | tail -1
[100%] Built target consumer
[退出码 0]

$ ./v-loose/consumer
[consumer] mathx 1.3.0 在场
[consumer] mx_clamp(999,0,10) = 10
[consumer] mx_gcd(48,180)     = 12
[consumer] mx_hash(a,5)       = 58503557
[退出码 0]
```

**要 9.9、实际 1.3.0，configure 成功了，程序还跑起来了。**

原因就在模块那一行：不写 `VERSION_VAR`，`find_package_handle_standard_args` 只判"文件找没找到"，
压根不看版本。而 CMake **已经**把 9.9 递到模块手里了（看那行"调用方要求的最低版本 = 9.9"）——
它只是不替你比较。

> **结论背下来：Module 模式下"版本号"是模块作者的良心活；Config 模式下它是 helper 生成的硬闸门。**
> 这也是现代上游都走 Config 的原因之一。
> 实用推论：你依赖一个用 Find 模块的包时，`find_package(foo 2.0 REQUIRED)` 这行**不构成任何保证**；
> 真要卡版本，自己在 CMakeLists 里判 `if(foo_VERSION VERSION_LESS 2.0)`。

---

## 读一个真实内置模块：`FindThreads` 在这台机器上给了个空目标

这一节教的是**读模块源码**这个动作本身。先看本机 `FindThreads.cmake` 的关键段：

```
$ sed -n "155,170p" /usr/share/cmake-4.2/Modules/FindThreads.cmake
# Internal helper macro.
# Do NOT even think about using it outside of this file!
macro(_threads_check_libc)
  if(NOT Threads_FOUND)
    if(CMAKE_C_COMPILER_LOADED)
      check_c_source_compiles("${PTHREAD_C_CXX_TEST_SOURCE}" CMAKE_HAVE_LIBC_PTHREAD)
    elseif(CMAKE_CXX_COMPILER_LOADED)
      check_cxx_source_compiles("${PTHREAD_C_CXX_TEST_SOURCE}" CMAKE_HAVE_LIBC_PTHREAD)
    endif()
    if(CMAKE_HAVE_LIBC_PTHREAD)
      set(CMAKE_THREAD_LIBS_INIT "")
      set(Threads_FOUND TRUE)
    endif()
  endif ()
endmacro()
```

逻辑是：**先编译一个真程序**，问"pthread 是不是已经在 libc 里了？"。
在这台机器（glibc 2.43）上答"是"，于是 `set(CMAKE_THREAD_LIBS_INIT "")` —— 什么都不加。跑一遍：

```
$ cmake -S . -B b | grep -E "Performing Test|Found Threads|THREADS_FOUND|CMAKE_THREAD_LIBS_INIT|接口"
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD
-- Performing Test CMAKE_HAVE_LIBC_PTHREAD - Success
-- Found Threads: TRUE
-- CMAKE_THREAD_LIBS_INIT=[] 探针结果 CMAKE_HAVE_LIBC_PTHREAD=1
-- Threads::Threads 的接口：compile=[_c-NOTFOUND] link=[_l-NOTFOUND]
[退出码 0]

$ cat b/CMakeFiles/p.dir/link.txt
/usr/bin/cc -Wl,--dependency-file=CMakeFiles/p.dir/link.d CMakeFiles/p.dir/p.c.o -o p
[退出码 0]

$ ./b/p
g=400000
[退出码 0]

$ cc -Wall p.c -o p_bare && ./p_bare
g=400000
[退出码 0]
```

看清楚了：这条 `link.txt` 里**没有 `-pthread`，也没有 `-lpthread`**，
`Threads::Threads` 的两个接口属性都是 `_c-NOTFOUND` / `_l-NOTFOUND` —— 它是个**空目标**。
而 `cc -Wall p.c` 完全不带任何 pthread 标志也照样跑出 `g=400000`（现代 glibc 把线程实现并进了 libc）。

那为什么还要写 `target_link_libraries(app PRIVATE Threads::Threads)`？

* 换到 macOS / 老 glibc / musl / 某些交叉工具链，那个探针会失败，
  模块转而去 `find_library(pthread)` 并把 `-lpthread` 填进接口 —— **同一行 CMake 自动变对**；
* 它是"我用了线程"这个事实的声明，别人读你的 `CMakeLists.txt` 就知道；
* 少了它在你这台机器上确实照样能跑（上面 `p_bare` 就是证据）。

> **别把"本机没坏"当成"写法对了"。** 这是本章要扎进脑子的第一条。
> 同理：`find_package(Threads)` 链接行里没有 `-pthread`，不代表你写错了，
> 也不代表换台机器还有这个运气。

## 内置模块的两副面孔：OpenMP 会真的把 flag 填进接口

```
$ cmake -S . -B b | grep -E "Performing Test|Found OpenMP|Configuring done"
-- Found OpenMP_C: -fopenmp (found version "4.5")
-- Found OpenMP: TRUE (found version "4.5") found components: C
-- Configuring done (0.3s)
[退出码 0]

$ grep -E 'C_FLAGS|C_INCLUDES' b/CMakeFiles/o.dir/flags.make
C_INCLUDES = 
C_FLAGS = -fopenmp
```

两个内置模块摆在一起，正好是 Module 模式的两副面孔：

* `FindThreads`：探针成功 → 接口是**空的**；
* `FindOpenMP`：探针成功 → 接口里**有 `-fopenmp`**（`flags.make` 的 `C_FLAGS` 那行就是证据）。

再看官方模块怎么处理版本 —— `FindZLIB.cmake`（本机装了模块但没装 zlib，
所以它只会"找不到"，但源码仍可当教材；下面两条是读文件，不是跑构建）：

```
$ grep -n -A2 "find_package_handle_standard_args(ZLIB" /usr/share/cmake-4.2/Modules/FindZLIB.cmake
242:find_package_handle_standard_args(ZLIB REQUIRED_VARS ZLIB_LIBRARY ZLIB_INCLUDE_DIR
243-                                       VERSION_VAR ZLIB_VERSION
244-                                       HANDLE_COMPONENTS)
[退出码 0]

$ grep -n "file(STRINGS" /usr/share/cmake-4.2/Modules/FindZLIB.cmake
128:cmake_policy(SET CMP0159 NEW) # file(STRINGS) with REGEX updates CMAKE_MATCH_<n>
221:  file(STRINGS "${ZLIB_INCLUDE_DIR}/zlib.h" ZLIB_H REGEX "^#define ZLIB_VERSION \"[^\"]*\"$")
[退出码 0]
```

`file(STRINGS ... REGEX)` 从头文件里抠版本 + `VERSION_VAR` 交给标准工具 ——
和我们第 4 节自己写的模块**一模一样**。
"版本比较是模块作者的活"这条规律，在官方模块里也是这么实现的；官方模块也**没写成硬闸门以外的任何东西**。

---

## 路线③：FetchContent —— configure 时现拉源码

本机不联网，所以下面所有 URI 都是 `file://`。先造一个本地"远程仓库"：

```
$ git clone -q --bare "/tmp/cmake04-xoPsG7/mathxsrc" "/tmp/cmake04-xoPsG7/mathx.git" && git --git-dir="/tmp/cmake04-xoPsG7/mathx.git" tag
v1.3.0
[退出码 0]

$ cmake -S fetch -B f-git -DROUTE=GIT "-DMX_URI=file:///tmp/cmake04-xoPsG7/mathx.git"       | grep -E 'FetchContent 走|BASE_DIR|源码目录|构建目录|Configuring done'
-- FetchContent 走 GIT：file:///tmp/cmake04-xoPsG7/mathx.git tag=v1.3.0
-- FETCHCONTENT_BASE_DIR = /tmp/cmake04-xoPsG7/proj/f-git/_deps
-- mathx 源码目录   = /tmp/cmake04-xoPsG7/proj/f-git/_deps/mathx-src
-- mathx 构建目录   = /tmp/cmake04-xoPsG7/proj/f-git/_deps/mathx-build
-- Configuring done (0.6s)
[退出码 0]

$ find f-git/_deps -maxdepth 1 | sort
f-git/_deps
f-git/_deps/mathx-build
f-git/_deps/mathx-src
f-git/_deps/mathx-subbuild

$ ./f-git/fetch_app
[fetch_app] 用 FetchContent 现拉的 mathx 1.3.0
[fetch_app] mx_clamp(-5,0,10) = 0，mx_hash(a,4) = 1167208513
[退出码 0]
```

`_deps` 下三件套，各管一件事：

```
    mathx-src/       拉下来的源码（真正的 .c/.h 在这儿）
    mathx-build/     它的构建产物（Make files、libmathx.a）—— 在你家构建目录里
    mathx-subbuild/  一个**独立的子 CMake 工程**，只干"把源码弄到手"这一件事
```

第三个最容易被忽略，也最能解释你看到的那些怪报错。把 `GIT_TAG` 写成本地仓库里不存在的 tag：

```
$ cmake -S fetch -B f-btag -DROUTE=GIT "-DMX_URI=file:///tmp/cmake04-xoPsG7/mathx.git" -DMX_TAG=nope       | grep -E 'Failed|Error|checkout' | head -6
[ 11%] Creating directories for 'mathx-populate'
[ 22%] Performing download step (git clone) for 'mathx-populate'
Cloning into 'mathx-src'...
fatal: invalid reference: nope
CMake Error at mathx-subbuild/mathx-populate-prefix/tmp/mathx-populate-gitclone.cmake:61 (message):
  Failed to checkout tag: 'nope'
```

报错点在 `mathx-subbuild/.../mathx-populate-gitclone.cmake` 这种**内部路径**里，
下面还挂着一串 `gmake: *** Error 2` 和 `FetchContent.cmake:1928` 的调用栈 ——
第一次见都以为是 CMake 坏了。**不是**：那是子工程 `mathx-populate` 目标在正常报错。

真实世界最常见的一条同类事故是 `GIT_TAG main` 或含糊的分支名：
今天能配好，明天上游一推新提交，你就构建出了不一样的东西。
**要固定就写 tag 或完整 commit hash。**（本 demo 用 `GIT_SHALLOW` + 固定 tag；
子模块用 `GIT_SUBMODULES ""` 明确不拉，否则它会连 `.gitmodules` 里的东西一起拽进来。）

## URL 路线：哈希写错的报错里，就写着正确答案

先做一个**确定性** tar 包（排序 + 固定 mtime + 固定 owner + `gzip -n`），
这样每次跑 demo 的 sha256 都一样，README 才敢把哈希写死：

```
$ mkdir -p tar && rm -rf tar/mathx-1.3.0 && cp -R mathx tar/mathx-1.3.0 \
      && tar --sort=name --mtime="UTC 2026-01-01" --owner=0 --group=0 --numeric-owner \
             -C tar -cf - mathx-1.3.0 | gzip -n > mathx-1.3.0.tar.gz \
      && sha256sum mathx-1.3.0.tar.gz
0d9d464e2cfcce56f721b3aebe79a7aed8f8284efce939f00729b9d84fde6ed7  mathx-1.3.0.tar.gz
[退出码 0]
[自证] 归档是确定性的：每次跑这份脚本，sha256 都应当是 0d9d464e2cfcce56f721b3aebe79a7aed8f8284efce939f00729b9d84fde6ed7
```

只给 URL、什么都不带 —— 撞策略警告：

```
$ cmake -S fetch -B f-url0 -DROUTE=URL "-DMX_URI=file:///tmp/cmake04-xoPsG7/proj/mathx-1.3.0.tar.gz"       | grep -E 'Warning|CMP0135|DOWNLOAD_EXTRACT|rebuilt' | head -8
CMake Warning (dev) at /usr/share/cmake-4.2/Modules/FetchContent.cmake:1383 (message):
  The DOWNLOAD_EXTRACT_TIMESTAMP option was not given and policy CMP0135 is
  not set.  The policy's OLD behavior will be used.  When using a URL
  download, the timestamps of extracted files should preferably be that of
  the time of extraction, otherwise code that depends on the extracted
  contents might not be rebuilt if the URL changes.  The OLD behavior
  preserves the timestamps from the archive instead, but this is usually not
  what you want.  Update your project to the NEW behavior or specify the
  DOWNLOAD_EXTRACT_TIMESTAMP option with a value of true to avoid this
  robustness issue.
```

这条警告担心的事非常具体，值得翻译成人话：
归档里带着**打包那天的老时间戳**，你换了 tar 包内容但文件 mtime 比已构建的目标旧，
`make` 就认为不用重编 —— **于是你构建出的还是上一版代码，而没有任何错误。**
修法是 `DOWNLOAD_EXTRACT_TIMESTAMP TRUE`（让解压时间变成现在），或者在工程顶部
`cmake_policy(SET CMP0135 NEW)`。

哈希写错：

```
$ cmake -S fetch -B f-badhash -DROUTE=URL "-DMX_URI=file:///tmp/cmake04-xoPsG7/proj/mathx-1.3.0.tar.gz"       -DMX_HASH=0000000000000000000000000000000000000000000000000000000000000000       | grep -E 'does not match|expected|actual' | head -5
-- verifying file...
     file='/tmp/cmake04-xoPsG7/proj/mathx-1.3.0.tar.gz'
CMake Error at mathx-subbuild/mathx-populate-prefix/src/mathx-populate-stamp/verify-mathx-populate.cmake:29 (message):
  error: SHA256 hash of

    /tmp/cmake04-xoPsG7/proj/mathx-1.3.0.tar.gz

  does not match expected value

    expected: '0000000000000000000000000000000000000000000000000000000000000000'
      actual: '0d9d464e2cfcce56f721b3aebe79a7aed8f8284efce939f00729b9d84fde6ed7'
```

**修法就是照抄 `actual` 那一串。** 这也是为什么官方推荐 URL 路线必配 `URL_HASH`：
它同时是完整性检查和"我到底下到了什么"的取证行。

配好之后：

```
$ cmake -S fetch -B f-url -DROUTE=URL "-DMX_URI=file:///tmp/cmake04-xoPsG7/proj/mathx-1.3.0.tar.gz"       "-DMX_HASH=0d9d464e2cfcce56f721b3aebe79a7aed8f8284efce939f00729b9d84fde6ed7" -DMX_ETS=TRUE | grep -E 'SHA256|Configuring done|Build files'
-- FetchContent 走 URL + SHA256=0d9d464e2cfcce56f721b3aebe79a7aed8f8284efce939f00729b9d84fde6ed7
-- Configuring done (0.5s)
-- Build files have been written to: /tmp/cmake04-xoPsG7/proj/f-url
[退出码 0]
```

## FetchContent 的代价，以及三味解药（其中一味实测无效）

代价就摆在磁盘上：

```
$ find f-url/_deps/mathx-src -type f | sed 's|f-url/||' | sort
_deps/mathx-src/CMakeLists.txt
_deps/mathx-src/cmake/mathxConfig.cmake.in
_deps/mathx-src/include/mathx.h
_deps/mathx-src/src/mathx.c

$ du -sh f-url/_deps/* | sort -k2
96K	f-url/_deps/mathx-build
16K	f-url/_deps/mathx-src
136K	f-url/_deps/mathx-subbuild

$ rm -rf f-url/_deps/mathx-build && cmake --build f-url 2>&1 | grep -E "Building|Linking" | head -4
[ 25%] Building C object _deps/mathx-build/CMakeFiles/mathx.dir/src/mathx.c.o
[ 50%] Linking C static library libmathx.a
[ 75%] Linking C executable fetch_app
```

三条税：

* `rm -rf build` 会把**拉下来的源码一起删掉**，下次 configure 重新拉一遍；
* 第三方每次跟着重编（上面 `mathx.c.o` 那行就是它在你家 CI 里烧的时间）；
* 别人 clone 你的仓库拿不到它，configure 时才去取 —— **内网/离线机器当场卡住**。

**解药一：`FETCHCONTENT_FULLY_DISCONNECTED=ON`**

```
$ cmake -S fetch -B f-off -DFETCHCONTENT_FULLY_DISCONNECTED=ON       | grep -E 'Configuring done|BASE_DIR'
-- FETCHCONTENT_BASE_DIR = /tmp/cmake04-xoPsG7/proj/f-off/_deps
-- Configuring done (0.0s)
[退出码 0]
```

源码已经躺在 `_deps` 里之后，加这个开关重配，下载/更新整步跳过（注意 `Configuring done (0.0s)`），
只把已有源码 `add_subdirectory` 进来。离线复现同事的构建、CI 里省时间都靠它 ——
**前提是第一次已经配好过。**

**解药二：`SYSTEM`（把警告算在谁头上）**

```
$ grep -E 'C_INCLUDES' f-url/CMakeFiles/fetch_app.dir/flags.make
C_INCLUDES = -I/tmp/cmake04-xoPsG7/proj/f-url/_deps/mathx-src/include

$ cmake -S fetch -B f-sys -DROUTE=URL "-DMX_URI=file:///tmp/cmake04-xoPsG7/proj/mathx-1.3.0.tar.gz"       "-DMX_HASH=0d9d464e2cfcce56f721b3aebe79a7aed8f8284efce939f00729b9d84fde6ed7" -DMX_ETS=TRUE -DMX_SYSTEM=ON | grep -E 'Configuring done'
-- Configuring done (0.5s)
[退出码 0]

$ grep -E 'C_INCLUDES' f-sys/CMakeFiles/fetch_app.dir/flags.make
C_INCLUDES = -isystem /tmp/cmake04-xoPsG7/proj/f-sys/_deps/mathx-src/include
```

同一个工程，只差 `-DMX_SYSTEM=ON` 一个档，`-I` 变成 `-isystem`。
`-I` 的潜台词是"这些头是我自己人的，警告算我的"；`-isystem` 才是"第三方系统头，别把它的警告报到我账上"。
第 03 章装出去的包天生 `-isystem`（Config 文件里写的是 `INTERFACE_SYSTEM_INCLUDE_DIRECTORIES`）；
现拉的源码要你**显式**说 `SYSTEM`（CMake 3.25+）。

**解药三：`EXCLUDE_FROM_ALL` —— 实测看不出差别**

```
$ cmake --build f-url --target help | grep -E "mathx|fetch_app"
... fetch_app
... mathx

$ cmake -S fetch -B f-excl -DROUTE=URL "-DMX_URI=file:///tmp/cmake04-xoPsG7/proj/mathx-1.3.0.tar.gz"       -DMX_ETS=TRUE -DMX_EXCLUDE=ON >/dev/null 2>&1 && cmake --build f-excl --target help | grep -E 'mathx|fetch_app'
... fetch_app
... mathx
```

**这里实测两份列表里 `mathx` 都在** —— 因为 `fetch_app` 链接了 `mathx`，它无论如何都得编。
`EXCLUDE_FROM_ALL` 只在"拉下来但没有任何 target 用它"（只要头文件、只要其中某个工具目标）时省时间。
我不替它编一个看不到的效果：这一档留着，等你哪天真的拉了整个 boost 再说。

---

## 路线④：vendoring —— 源码就在仓库里，`add_subdirectory` 一步到位

先把一个经典报错演出来（只给一个参数的树外源码）：

```
$ cmake -S noarg -B b-noarg | sed -n "/CMake Error/,+3p"
CMake Error at CMakeLists.txt:3 (add_subdirectory):
  add_subdirectory not given a binary directory but the given source
  directory "/tmp/cmake04-xoPsG7/proj/mathx" is not a subdirectory of
  "/tmp/cmake04-xoPsG7/proj/noarg".  When specifying an out-of-tree source a
  binary directory must be explicitly specified.
```

**树外源码必须给第二个参数（二进制目录）** —— CMake 没法替你猜一个源码在树外的工程该把产物放哪。
所以 `vendored/CMakeLists.txt` 里那句是写全的：

```
$ cmake -S vendored -B v-all | grep -E "树外|Configuring done"
-- 把树外源码 /tmp/cmake04-xoPsG7/proj/vendored/../mathx 用 add_subdirectory 拉进本树
-- Configuring done (0.1s)
[退出码 0]

$ ./v-all/vendor_app
[vendor_app] 用 vendored 源码编出来的 mathx 1.3.0
[vendor_app] mx_gcd(1071,462) = 21
[退出码 0]

$ find v-all/_mathx -maxdepth 2 -name "*.a" -o -maxdepth 2 -name "*.cmake" | sort
v-all/_mathx/CMakeFiles/CMakeDirectoryInformation.cmake
v-all/_mathx/cmake_install.cmake
v-all/_mathx/libmathx.a
v-all/_mathx/mathxConfig.cmake
v-all/_mathx/mathxConfigVersion.cmake
```

对比 FetchContent：**没有 `_deps`，也没有 subbuild**；产物进了你指定的 `_mathx/`。

### vendoring 的第三种税：你的包里混进了上游的交付物

```
$ DESTDIR="/tmp/cmake04-xoPsG7/stage" cmake --install v-all --prefix /usr/local | tail -6
-- Installing: /tmp/cmake04-xoPsG7/stage/usr/local/include
-- Installing: /tmp/cmake04-xoPsG7/stage/usr/local/include/mathx.h
-- Installing: /tmp/cmake04-xoPsG7/stage/usr/local/lib/cmake/mathx/mathxConfig.cmake
-- Installing: /tmp/cmake04-xoPsG7/stage/usr/local/lib/cmake/mathx/mathxConfigVersion.cmake
-- Installing: /tmp/cmake04-xoPsG7/stage/usr/local/lib/cmake/mathx/mathxTargets.cmake
-- Installing: /tmp/cmake04-xoPsG7/stage/usr/local/lib/cmake/mathx/mathxTargets-noconfig.cmake
[退出码 0]

$ find "/tmp/cmake04-xoPsG7/stage" -type f | sed "s|/tmp/cmake04-xoPsG7/stage/||" | sort
usr/local/include/mathx.h
usr/local/lib/cmake/mathx/mathxConfig.cmake
usr/local/lib/cmake/mathx/mathxConfigVersion.cmake
usr/local/lib/cmake/mathx/mathxTargets-noconfig.cmake
usr/local/lib/cmake/mathx/mathxTargets.cmake
usr/local/lib/libmathx.a
```

**你只 install 自己的工程**，结果 `libmathx.a`、`mathx.h`，连 `mathxConfig.cmake` 三件套**全被装走了** ——
因为 `add_subdirectory` 会**连同上游的 `install()` 规则一起继承**。

前两种税是"源码进你仓库""每次跟着重编"，这第三种最隐蔽：
用户会因此在系统里看到一份你没承诺过的 `libmathx.a`；
如果你的工程同时又 `find_package(mathx)` 过，两份同名 target / 同名文件打架就开始了。

（顺便：那次 `DESTDIR=` 运行**没有**碰 `/usr/local` 一个字节 —— 第 03 章的老规矩；
`cmake --install` 在 4.2.3 里没有 `--destdir` 选项，只能用环境变量。）

---

## 找不到时怎么办：三步定位，别改十行 CMakeLists

### 第一步：把报错本身读完，它就是说明书

```
$ cmake -S consumer -B c-dbg -DCMAKE_PREFIX_PATH="/tmp/cmake04-xoPsG7/sysroot-bare" -DUSE_OUR_MODULE=OFF       | sed -n '/CMake Error/,/installed/p'
CMake Error at CMakeLists.txt:40 (find_package):
  By not providing "Findmathx.cmake" in CMAKE_MODULE_PATH this project has
  asked CMake to find a package configuration file provided by "mathx", but
  CMake did not find one.

  Could not find a package configuration file provided by "mathx" (requested
  version 1.0) with any of the following names:

    mathxConfig.cmake
    mathx-config.cmake

  Add the installation prefix of "mathx" to CMAKE_PREFIX_PATH or set
  "mathx_DIR" to a directory containing one of the above files.  If "mathx"
  provides a separate development package or SDK, be sure it has been
  installed.
```

这一段说了**四件事**，一件都别跳过：

1. `By not providing "Findmathx.cmake" in CMAKE_MODULE_PATH` —— 我先试了模块模式，你没有；
2. 候选文件名只有 `mathxConfig.cmake` / `mathx-config.cmake` 这两个拼法，别自创；
3. 补救办法一：把安装前缀加进 `CMAKE_PREFIX_PATH`；
4. 补救办法二：或者直接设 `mathx_DIR`。

### 第二步：开搜索轨迹

```
$ cmake -S diag -B d1 2>&1 | sed -n "/CMake Debug Log/,+16p"
CMake Debug Log at CMakeLists.txt:4 (find_package):
  find_package considered the following paths for Findmathx.cmake:

    /usr/share/cmake-4.2/Modules/Findmathx.cmake

  The file was not found.

  The internally managed CMAKE_FIND_PACKAGE_REDIRECTS_DIR.

    /tmp/cmake04-xoPsG7/proj/d1/CMakeFiles/pkgRedirects

  <PackageName>_ROOT CMake variable [CMAKE_FIND_USE_PACKAGE_ROOT_PATH].

    none

  CMAKE_PREFIX_PATH variable [CMAKE_FIND_USE_CMAKE_PATH].
```

这是 `set(CMAKE_FIND_DEBUG_MODE ON)`（或命令行 `--debug-output`）打出来的轨迹。
它逐个列出试过哪条路径、为什么被否 —— `CMAKE_MODULE_PATH`、`pkgRedirects`、`<包名>_ROOT`、
`CMAKE_PREFIX_PATH`、环境变量版、`CMAKE_SYSTEM_PREFIX_PATH`……条数取决于你机器上装了多少东西：

```
$ cmake -S diag -B d2 2>&1 | grep -c "^    /"
39
[退出码 1]
```

**平时别开它**（刷屏），但查"我明明装了它为什么说没装"时它是唯一可靠的仪器。

### 第三步（救命稻草）：`<包名>_DIR` 直接点名

```
$ cmake -S consumer -B c-dir -DCMAKE_PREFIX_PATH=/nonexistent -DUSE_OUR_MODULE=OFF       "-Dmathx_DIR=/tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx" | grep -E '应答|mathx_DIR|Configuring done'
-- [mathxConfig.cmake] Config 模式应答：/tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx
-- mathx_DIR        = /tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx   (Config 模式由它指到配置文件目录)
-- Configuring done (0.1s)
[退出码 0]

$ cmake --build c-dir | tail -1
[100%] Built target consumer
[退出码 0]

$ ./c-dir/consumer
[consumer] mathx 1.3.0 在场
[consumer] mx_clamp(999,0,10) = 10
[consumer] mx_gcd(48,180)     = 12
[consumer] mx_hash(a,5)       = 58503557
[退出码 0]
```

`CMAKE_PREFIX_PATH` 是个**不存在的路径**，configure 却成功了，程序还跑起来了 ——
因为 `mathx_DIR` 直接点名，跳过了整套搜索。

它同时也是最精确的**诊断**：`_DIR` 能成、前缀不能成，就说明前缀下 `lib/cmake/mathx` 那个层级不对
（很多人栽在多一层/少一层目录上，比如装成了 `lib/mathx/cmake/`）。

### 两条反面实测：`_CONFIG` 无效，坏 `_DIR` 静默忽略

网上有人教你用 `<包名>_CONFIG` 直接指向配置文件**本身**：

```
$ cmake -S consumer -B c-cfgvar -DCMAKE_PREFIX_PATH=/nonexistent -DUSE_OUR_MODULE=OFF       "-Dmathx_CONFIG=/tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx/mathxConfig.cmake" | sed -n '/CMake Error/,+3p'
CMake Error at CMakeLists.txt:40 (find_package):
  By not providing "Findmathx.cmake" in CMAKE_MODULE_PATH this project has
  asked CMake to find a package configuration file provided by "mathx", but
  CMake did not find one.
```

**不生效。** 普通变量、CACHE、`CACHE FORCE` 三种写法都试过，都这条路。
要指目录就用 `mathx_DIR`，指文件是不存在的通路。
（也别指望在 Find 模块里 `set(mathx_CONFIG ...)` 然后 `return()` 把活儿转交给上游 Config —— 同样不生效，
这一点我在 `Findmathx.cmake` 第 0 段注释里留了实测记录。）

更阴的一条：

```
$ cmake -S consumer -B c-baddir -DCMAKE_PREFIX_PATH="/tmp/cmake04-xoPsG7/sysroot-cfg" -DUSE_OUR_MODULE=OFF -Dmathx_DIR=/nope       | grep -E '应答|Error|Configuring done'
-- [mathxConfig.cmake] Config 模式应答：/tmp/cmake04-xoPsG7/sysroot-cfg/lib/cmake/mathx
-- Configuring done (0.1s)
[退出码 0]
```

`mathx_DIR` 设成**不存在的目录**，CMake 不报"你给的目录里没有配置文件"，
而是当作没设、退回正常搜索 —— 于是这次**照样成功**。

后果是：你机器上好好的，CI 上换了个前缀就崩，而且报错完全不提 `_DIR`。
**凡是设过 `<包名>_DIR` 的工程，查问题第一件事就是 `grep mathx_DIR CMakeCache.txt`，
看它这次到底被填成了什么。**

---

## 四条路线怎么选

| 路线 | 上游给了什么 | 你要写什么 | 版本闸门 | 离线 | 适合 |
|---|---|---|---|---|---|
| ① Config | `lib/cmake/mathx/*` 三件套 | `find_package(mathx CONFIG)` | **硬**（ConfigVersion） | 要预装 | 现代上游、系统包、vcpkg/conan |
| ② Module | 只有 `.a` + 头 | 自己写 `Findmathx.cmake` | **良心**（除非你写 VERSION_VAR） | 要预装 | 上古库、系统库、没有 CMake 支持的库 |
| ③ FetchContent | 一个 git 仓库或 tar 包 | `FetchContent_Declare/MakeAvailable` | 由 tag/hash **锁死** | ❌ 第一次要能取到（可用 FULLY_DISCONNECTED 补） | 想锁死源码版本、CI 可复现 |
| ④ vendoring | 把上游源码放进你仓库 | `add_subdirectory(路径 二进制目录)` | 你自己控制 | ✅ | 内网、极小依赖、要改上游源码 |

三条实用判断：

* **能走 Config 就走 Config。** 版本闸门是白送的，target 接口是上游自己维护的。
* **要写 Find 模块就照官方模块写**（`find_path`/`find_library`/`file(STRINGS)`/`VERSION_VAR`/`UNKNOWN IMPORTED`），
  并且知道你的版本检查是"良心活"。
* **FetchContent 和 vendoring 都会把源码带进你的构建图**，区别只是"什么时候取"和"存不存在你仓库里"。
  两者都要注意 install 规则会传染（第 14 节那三种税）。

---

## 排错速查

| 现象 | 真实原因 | 怎么办 |
|---|---|---|
| `Could not find a package configuration file` | Config 模式：前缀下没有 `lib/cmake/<包>/` | 看报错里那两个候选文件名；给 `_DIR` |
| `No "FindX.cmake" found in CMAKE_MODULE_PATH` | 强制 MODULE 但没写模块 | 写模块，或去掉 `MODULE` 关键字 |
| 同一行 `find_package` 换机器换了实现 | AUTO 模式静默降级 Module→Config | 铭牌法；显式写 `CONFIG`/`MODULE` |
| `find_package(foo 2.0 REQUIRED)` 居然过了 | 对方是 Find 模块且没写 `VERSION_VAR` | 自己 `if(foo_VERSION VERSION_LESS 2.0)` |
| `Found unsuitable version "x", but required...` | 版本闸门正常工作 | 升级上游或降低要求，不是 CMake 坏 |
| FetchContent 报 `mathx-populate-gitclone.cmake:61` | 子工程取源码失败（tag/URI/离线） | 看上一行 `fatal:`；固定 tag/commit |
| FetchContent 每次 configure 都重新拉 | `rm -rf build` 把 `_deps` 一起删了 | `FETCHCONTENT_FULLY_DISCONNECTED=ON` 复用 |
| 换了 tar 包内容却没重编 | 归档老时间戳 + CMP0135 OLD | `DOWNLOAD_EXTRACT_TIMESTAMP TRUE` |
| 上游警告全算到我头上 | 现拉源码默认 `-I` | `SYSTEM`（3.25+），变 `-isystem` |
| 我的包里出现上游的 `.a` 和 Config | `add_subdirectory` 继承 `install()` | 用 `EXCLUDE_FROM_ALL` 不解决此问题；要么上游包好，要么别 install |
| `-Dmathx_DIR=/nope` 却构建成功 | 坏 `_DIR` 被**静默忽略**，退回搜索 | `grep mathx_DIR CMakeCache.txt` |
| `-D<包>_CONFIG=<文件>` 没反应 | 该通路不存在 | 只用 `<包>_DIR=<目录>` |
| 链接行里没有 `-pthread` | glibc 2.43 上探针成功 → 空接口 | 保留 `Threads::Threads`；别把本机没坏当写法对 |

---

## 怎么跑这章的 demo

```
cd cmake-handbook/04-dependencies
bash demo.sh                # 15 节全程跑完，自动删沙箱
bash demo.sh --keep         # 保留沙箱：两个 sysroot、_deps、stage 都能进去翻
bash demo.sh > out.txt      # 重定向自动关颜色
```

`demo.sh` 自己 `mktemp -d`，把所有安装前缀、`_deps`、`DESTDIR` 暂存全关在沙箱里。
安全边界（脚本第 15 节会自证）：

* **不联网**：`GIT` 路线用本地 bare 仓库，`URL` 路线用本地 tar 包，URI 全是 `file://`；
* `--prefix` 永不指向 `/usr/local`；打包只用 `DESTDIR=` 暂存到沙箱；
* 不写全局 CMake 配置，不碰仓库里那 56 节数据结构代码。

沙箱路径每次不同（`cmake04-` + 6 位随机），本文引用的那一轮是 `/tmp/cmake04-xoPsG7`。
`mathx-1.3.0.tar.gz` 的 sha256 是固定的 `0d9d464e...de6ed7` —— 因为打包命令带
`--sort=name --mtime --owner=0 --group=0 --numeric-owner` 和 `gzip -n`。

想亲手把两种模式对比着玩，最短路径是这三条：

```
cmake -S consumer -B x -DCMAKE_PREFIX_PATH="$SB/sysroot-cfg"  -DUSE_OUR_MODULE=OFF   # Config 应答
cmake -S consumer -B y -DCMAKE_PREFIX_PATH="$SB/sysroot-bare"                        # Module 应答
cmake -S consumer -B z -DCMAKE_PREFIX_PATH="$SB/sysroot-bare" -DMODE=CONFIG          # 强制 CONFIG → 报错
```

---

## 术语小抄

| 词 | 大白话 |
|---|---|
| Module 模式 | CMake 用**调用方写的** `Find<包>.cmake` 去找东西 |
| Config 模式 | CMake 用**上游装好的** `<包>Config.cmake` 直接问路 |
| `CMAKE_PREFIX_PATH` | "去这些前缀下面找"，多个用 `;` 分隔 |
| `<包名>_DIR` | 直接点名配置文件所在目录，跳过整套搜索 |
| `<包名>_ROOT` | 另一种点名方式（指前缀），优先级更高 |
| `CMAKE_FIND_DEBUG_MODE` | 把搜索轨迹逐条打出来的仪器，平时别开 |
| `ConfigVersion.cmake` | 版本裁判，`write_basic_package_version_file` 生成 |
| `VERSION_VAR` | Find 模块里"把版本交给标准工具判"的那个开关 |
| `_deps/<名>-{src,build,subbuild}` | FetchContent 三件套：源码 / 它的产物 / 取源码的子工程 |
| `mathx-populate` | 子工程里那个"只负责把源码弄到手"的内部目标 |
| vendoring | 把上游源码放进自己仓库，用 `add_subdirectory` 拉进构建图 |
| `DESTDIR=` | 把 install 结果先落到暂存目录，不碰真实系统路径 |

---

## 下一步

第 05 章《构建的四个旋钮》把这章"顺手敲的命令行"翻过来解剖：
`-G` 换生成器时那两条长得一样的错误各自的真实原因、`CMAKE_BUILD_TYPE` 默认为什么是空串、
`--config` 为什么会被静默吃掉、`-U` 为什么删不掉 `option()`、
以及 `-I` 的顺序怎么决定同名头谁赢 —— 全部用 `CMakeCache.txt` / `flags.make` / `.i` / `nm` 现场取证。
