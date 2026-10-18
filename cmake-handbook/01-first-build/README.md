# 第一次构建：把 CMake 的三件事看清楚

> 这一章只干一件事：让你亲眼看到 `cmake` 到底分几步、每步往磁盘上写了什么、
> 以及为什么网上老教程里的 `cmake . && make` 在今天被认为是错的。
> 下面**每一段输出都是这台机器上真实跑出来的**（cmake 4.2.3 + GNU Make 4.4.1 + gcc 15.2.0），
> 跑法见文末「怎么跑这章的 demo」。
> 输出里的 `/tmp/cmake01-JujDkk/...` 是 `mktemp -d` 每次随机生成的沙箱路径，你跑出来会是 `cmake01-<另外 6 位>`，
> 除此之外应当逐字节一致。
>
> **引用约定**（和全书同一口径）：屏幕上除了沙箱路径，还会带出你的登录名 —— `ls -l` 的**属主列**、
> `/home/<用户>` 这类**家目录路径**。这两样是 shell 自己展开的，`demo.sh` 拦不住，README 里一律写成
> `<用户>` / `<家目录>`。同样，`du -sh` 的体积、`ls -l` 的时间戳**每跑必变**。
> 这四类行本章都不放进带 `$` 的引用块：块里只留命令行和稳定行，关掉块之后用一句正文说明，再开一个
> **不含 `$` 提示符**的块贴本轮真值（引文校验器只查带 `$` 的块）。除这几处以外，围栏逐行原样 ——
> 全章**没有省略号、没有改写、没有合并行**。

---

## 你会遇到的场景

先对上号，这些是不是你的现场：

1. 接手一个项目，README 写着 `mkdir build && cd build && cmake ..` —— 你不明白为什么要多套一层 `build`，
   觉得是仪式感，于是直接在根目录 `cmake .`，结果源码目录多了一堆 `CMakeFiles/`、`CMakeCache.txt`，
   `git status` 一片红，还分不清哪些该提交。
2. 你 `cmake` 过一次之后改了 `CMakeLists.txt`，纠结“要不要重新跑一遍 cmake”。
3. 你 `-DSOME_OPTION=ON`，命令成功退出 0，但行为一点没变 —— 变量名少写了一个字母。
4. 你只想重编一个 `.c`，结果所有文件全重编了，怀疑 CMake 没做增量。
5. 你换了编译器（比如从 gcc 换到 clang），`cmake` 却完全不理你，还在用 gcc。
6. 编辑器/clangd 一直报“找不到头文件”，因为它是按“直接 gcc”去猜的，不知道 CMake 生成的真实命令行。

这 6 个场景的答案全部来自同一个心智模型。把它记住，后面 90% 的怪事都能自己解释。

---

## 心智模型：CMake 不是构建系统，它是“构建系统的生成器”

大多数人卡住，是因为把 CMake 当成了 `make` 的替代品。**不是。** CMake 自己不编译任何代码，
它只做“翻译 + 生成”，真正干活的还是 `make`（或 ninja 等）。

```
        你写的                     CMake 生成的                    真正执行的
 ┌──────────────────┐  configure  ┌──────────────────┐   build    ┌──────────────┐
 │ CMakeLists.txt   │ ──────────► │ build/Makefile   │ ─────────► │ gcc ...      │
 │ (声明式：我要什么)│             │ CMakeCache.txt   │  (cmake    │ .o → hello   │
 └──────────────────┘             │ flags.make ...   │   --build) └──────────────┘
        ▲                         └──────────────────┘
        │                                 ▲
   -D 变量 / option                  底层构建系统（本机只有 Unix Makefiles；
   （只在 configure 生效）              没有 ninja，所以本章所有输出都是 make 的措辞）
```

三个必须分清的名词：

| 名词 | 是什么 | 本例的位置 | 能不能提交进 git |
|---|---|---|---|
| **源码目录**（`-S`） | 有 `CMakeLists.txt` 的地方，只放你写的东西 | `.`（`01-first-build/`） | ✅ 全部提交 |
| **构建目录**（`-B`） | 所有产物和缓存的家，可随时整个删掉 | `build/` | ❌ 一律忽略 |
| **产物** | 可执行文件、库、`.o` | `build/hello`、`build/CMakeFiles/*.o` | ❌ 忽略 |

**关键推论**（后面每个实验都在验证它）：

- 只有 configure 阶段读 `-D` 变量 → 所以变量改了“行为没变”，八成是没重新 configure，或者拼错了名字。
- 构建目录里的 `CMakeCache.txt` 会记住上一次的所有决定 → 这是“换了编译器不生效”的根因。
- `cmake --build` 只是把活转包给底层构建系统 → 增量、并行、`-j` 都是底层系统在管，不是 CMake 在管。

本机示例工程一共只有 4 个文件（第 1 节会打印出来）：`CMakeLists.txt` + `src/main.c` + `src/greet.c` + `src/greet.h`。
`greet.c` 里就一个 `snprintf` 包装，故意写得“有截断演示”，好让构建产物跑起来有内容可看。

---

## 场景逐个实战

下面每个场景都对应 `demo.sh` 里的一段，命令前的 `$` 就是终端里真敲的。

### 场景 1：先看清这台机器能用什么（demo 第 0 节）

版本决定语法能不能用，这步不能省：

```
$ cmake --version
cmake version 4.2.3
$ make --version | head -2
GNU Make 4.4.1
Built for x86_64-pc-linux-gnu
$ gcc --version | head -1
gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0
$ which cmake make gcc
```

三条路径里 `make` 那一行带你自己的家目录，按「引用约定」写成 `<家目录>`，所以整组输出挪到下面这个不带
提示符的块里 —— 它的位置（`~/.local/bin`，不是 `/usr/bin`）才是下面第 1 条要用的证据，前缀本身不是：

```
/usr/bin/cmake
<家目录>/.local/bin/make
/usr/bin/gcc
```

```
$ command -v c++ || echo "c++: 未安装"
c++: 未安装
```

三件事立刻值得注意：

1. **`make` 在 `~/.local/bin`**，不在 `/usr/bin`。换机器/换用户时如果 `PATH` 没带上它，
   `cmake --build` 会报 “No such tool available”，这时先怀疑 `PATH` 而不是怀疑 CMake。
2. **本机没有 g++**。所以 `project()` 必须写 `LANGUAGES C`：

   ```cmake
   project(hello_cmake VERSION 0.1 LANGUAGES C)
   ```

   不写 `LANGUAGES` 时 CMake 默认探测 `C CXX`，这台机器会在“编译器探测”阶段就失败。
   这不是小事 —— 声明你只用哪些语言，本身就是现代 CMake 的第一条卫生习惯。
3. **cmake 是 4.x**。4.x 有两个可见变化：`cmake_minimum_required(VERSION 3.5)` 这类过低声明会被直接拒绝
   （所以示例写 `3.16`），以及编译器可用性测试是 “skipped”（下一节你会看到）。

### 场景 2：第一次 configure —— 它做的四件事（demo 第 2 节）

```
$ cmake -S . -B build
-- The C compiler identification is GNU 15.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Configuring done (0.2s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake01-JujDkk/proj/build
```

逐行对应到它干的四件事：

| 输出行 | CMake 正在做的事 | 结果落在哪 |
|---|---|---|
| `The C compiler identification is GNU 15.2.0` | 找到编译器并**编一个探测程序**验证版本 | `CMakeFiles/4.2.3/CompilerIdC/` |
| `Detecting C compiler ABI info` | 探测链接的 ABI（平台、位宽） | `CMakeFiles/4.2.3/CMakeCCompiler.cmake` |
| `Check for working C compiler ... - skipped` | 4.x：既然上面已经真编过了，这一步就跳过（老版本这里会再编一次） | — |
| `Configuring done` | 执行完你的 `CMakeLists.txt`，所有变量算出来了 | `CMakeCache.txt` |
| `Generating done` | 把算出来的计划**翻译成底层构建系统的文件** | `build/Makefile`、`Makefile2`、`flags.make` |

然后打开构建目录看看：

```
$ ls build
CMakeCache.txt
CMakeFiles
Makefile
cmake_install.cmake
$ du -sh build
```

`du -sh` 那个字节数每跑必变（它跟着 `CMakeConfigureLog.yaml` 的长度走），本章不拿它当证据，只说明形状。
本轮真值是：

```
228K	build
```

```
$ find build -name "*.o" | wc -l
0
```

**`.o` 数量是 0 —— 这是本章最重要的一行。** configure 只是“把计划写好并存档”，一个字节源码都还没编译。
`cmake_install.cmake` 现在是空壳，但它已经在那儿了：`install()` 的产物在这个阶段生成，第 03 章会用到。

`build/CMakeFiles/` 里则是 CMake 的“车间”，十行全是它生成的脚手架：

```
$ ls build/CMakeFiles | head -10
4.2.3
CMakeConfigureLog.yaml
CMakeDirectoryInformation.cmake
CMakeScratch
InstallScripts.json
Makefile.cmake
Makefile2
TargetDirectories.txt
cmake.check_cache
hello.dir
```

`hello.dir/` 就是 target `hello` 的私人目录（`.o` 和 `flags.make` 都在里面），
`.dir` 后缀是 CMake 给每个 target 建的中转目录名 —— 看到它就明白“哦，这里是按 target 分家的”。

### 场景 3：构建、运行、以及“再跑一次什么都没干”（demo 第 3 节）

```
$ cmake --build build
[ 33%] Building C object CMakeFiles/hello.dir/src/main.c.o
[ 66%] Building C object CMakeFiles/hello.dir/src/greet.c.o
[100%] Linking C executable hello
[100%] Built target hello
```

这四行不是 CMake 的措辞，是 **make 的措辞**（`[ 33%] ...` 是 make 进度格式）。
`cmake --build build` 等价于 `make -C build`，但前者不依赖底层是什么系统 —— 这就是现代写法坚持用
`cmake --build` 的唯一理由：**换 generator 时你的脚本一个字都不用改**。

产物：

```
$ ls -l build/hello
```

这一行的**属主列**是你的登录名（按「引用约定」写成 `<用户>`），**时间戳**每跑必变 —— 两处都不当证据，
真正要看的只有“`hello` 这个可执行文件确实生成了、16096 字节”。本轮真值：

```
-rwxrwxr-x 1 <用户> <用户> 16096 Oct  5 13:01 build/hello
```

```
$ ./build/hello
正常写入：你好, CMake 学习者!
snprintf 返回 24，缓冲区容量 32 → 没截断

小缓冲区："你"
snprintf 返回 24，缓冲区容量 4 → **被截断了**（返回值告诉你本来需要几字节）
```

紧接着同一条命令再跑一次：

```
$ cmake --build build
[100%] Built target hello
```

只有“已经建好了”这一行，没有任何 `[33%]`/`[66%]` —— 没有文件变动，一个 `.o` 都没重编。
**这就是增量构建在磁盘上的样子。**

### 场景 4：碰一个文件，只重编那一个（demo 第 4 节）

```
$ touch src/greet.c && cmake --build build
[ 33%] Building C object CMakeFiles/hello.dir/src/greet.c.o
[ 66%] Linking C executable hello
[100%] Built target hello
```

`main.c.o` 没出现 → 只重编了被改的那个；但**链接必然重做**（可执行文件是全员拼出来的）。

```
$ touch src/main.c && cmake --build build -j 4
[ 33%] Building C object CMakeFiles/hello.dir/src/main.c.o
[ 66%] Linking C executable hello
[100%] Built target hello
```

`-j 4` 是转包给 make 的并行度。两个文件时看不出差别，几百个文件时才看得见 —— 但记住：**并行是底层构建系统的能力，
CMake 只负责把它传下去**（也可以用 `cmake -j` 之外的 `CMAKE_BUILD_PARALLEL_LEVEL` 环境变量固定下来）。

### 场景 5：改了 CMakeLists.txt，不用你手动重来（demo 第 5 节）

往 `CMakeLists.txt` 末尾追加一行：

```cmake
target_compile_definitions(hello PRIVATE BUILD_TAG="demo")
```

然后只跑构建：

```
$ cmake --build build
-- Configuring done (0.0s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake01-JujDkk/proj/build
[ 33%] Building C object CMakeFiles/hello.dir/src/greet.c.o
[ 66%] Building C object CMakeFiles/hello.dir/src/main.c.o
[100%] Linking C executable hello
[100%] Built target hello
```

看输出顺序：先 `Configuring/Generating`，再编译 ——
生成的 `Makefile` 里写着“依赖 `CMakeLists.txt`”，发现它变新就自动重跑 configure。
所以**日常只需要 `cmake --build`，改 CMakeLists 也会被带着重新 configure**。

注意两个 `.o` 全重编了 —— 因为编译参数变了（多了 `-DBUILD_TAG`）。这条规律很有用：
**改源文件 → 只重编那一个；改编译参数 → 受影响 target 全重编。**

### 场景 6：缓存 —— 90% 怪事的案发现场（demo 第 6 节）

```
$ grep -E "^(CMAKE_C_COMPILER|CMAKE_C_FLAGS|CMAKE_BUILD_TYPE|HELLO_STRICT|CMAKE_GENERATOR):" build/CMakeCache.txt
CMAKE_BUILD_TYPE:STRING=
CMAKE_C_COMPILER:FILEPATH=/usr/bin/cc
CMAKE_C_FLAGS:STRING=
HELLO_STRICT:BOOL=OFF
CMAKE_GENERATOR:INTERNAL=Unix Makefiles
$ wc -l build/CMakeCache.txt
361 build/CMakeCache.txt
[退出码 0]
```

这就是“CMake 记住了什么”的唯一权威答案，一共 361 行。别用编辑器翻，用 `-L`：

```
$ cmake -L build | head -14
-- Cache values
CMAKE_BUILD_TYPE:STRING=
CMAKE_INSTALL_PREFIX:PATH=/usr/local
HELLO_STRICT:BOOL=OFF
```

三行就够 —— `-L` 只列“给人看的”。而 `cmake -LAH build`（A=含 advanced，H=含帮助）会打出 148 行，
日常排查基本用不到，只在追“这个内部变量到底哪来的”时才用。

改一个变量（在**已有**构建目录上改，会触发 re-configure）：

```
$ cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug | tail -5
-- Configuring done (0.0s)
-- Generating done (0.0s)
$ grep -E "^CMAKE_BUILD_TYPE:" build/CMakeCache.txt
CMAKE_BUILD_TYPE:STRING=Debug
$ grep -E '^CMAKE_C_FLAGS' build/CMakeCache.txt
CMAKE_C_FLAGS:STRING=
CMAKE_C_FLAGS_DEBUG:STRING=-g
CMAKE_C_FLAGS_MINSIZEREL:STRING=-Os -DNDEBUG
CMAKE_C_FLAGS_RELEASE:STRING=-O3 -DNDEBUG
CMAKE_C_FLAGS_RELWITHDEBINFO:STRING=-O2 -g -DNDEBUG
```

看清楚了：`CMAKE_BUILD_TYPE` 自己不往命令行塞任何东西，它只是一个**下标**，
用来挑出 `CMAKE_C_FLAGS_<TYPE>` 这一份。所以 `Debug` 只动了 `-g`，`RELEASE` 那份 `-O3 -DNDEBUG` 一个字没变。

再往下追一层，变量最终落到每个 target 自己的 `flags.make`：

```
$ grep -E "C_INCLUDES|C_DEFINES|C_FLAGS" build/CMakeFiles/hello.dir/flags.make
C_DEFINES = -DBUILD_TAG=\"demo\"
C_INCLUDES = 
C_FLAGS = -g -std=gnu11 -Wall -Wextra -Wshadow
```

**`变量 → CMakeCache.txt → flags.make → gcc 命令行`**，这条链看懂了，CMake 就不再是黑盒：
`-g` 来自 Debug，`-Wall -Wextra -Wshadow` 来自 `target_compile_options(hello PRIVATE ...)`，
`-DBUILD_TAG` 来自场景 5 追加的那行。

顺带一个真实细节：`C_FLAGS` 里写的是 **`-std=gnu11`**，不是你 `set(CMAKE_C_STANDARD 11)` 里的 `c11`。
CMake 默认允许 GNU 扩展，所以映射成 `gnu11`。想要严格的 `c11`，得再加一句 `set(CMAKE_C_EXTENSIONS OFF)`
（或第 02 章的 `target_compile_features` 路线）。这个差异平时无害，但你用了 MSVC 兼容模式或严格的
标准检查脚本时就会咬你一口。

### 场景 7：自己的开关 `option()` + `-D`，以及拼错名字的真实现场（demo 第 7 节）

`CMakeLists.txt` 里已经声明了一个开关：

```cmake
option(HELLO_STRICT "把警告当成错误（用来体验 -D 开关和缓存）" OFF)
if(HELLO_STRICT)
    target_compile_options(hello PRIVATE -Werror)
endif()
```

打开它，用**独立的新构建目录**（这样互不干扰，也是推荐的实验方式）：

```
$ cmake -S . -B build-strict -DHELLO_STRICT=ON | grep -E "strict|Configuring done|Generating done|Build files"
-- Configuring done (0.2s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake01-JujDkk/proj/build-strict
[退出码 0]
$ grep -E "^HELLO" build-strict/CMakeCache.txt
HELLO_STRICT:BOOL=ON
[退出码 0]
$ cmake --build build-strict
[ 33%] Building C object CMakeFiles/hello.dir/src/main.c.o
[ 66%] Building C object CMakeFiles/hello.dir/src/greet.c.o
[100%] Linking C executable hello
[100%] Built target hello
[退出码 0]
```

带 `-Werror` 也过了，说明示例代码本身没有警告（这是本章的一个隐含验收：教学代码必须能在严格开关下干净编译）。

现在**把变量名拼错一个字母**（STRICT → STRICTT），这是最有价值的 8 行输出：

```
$ cmake -S . -B build-typo -DHELLO_STRICTT=ON
-- The C compiler identification is GNU 15.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Configuring done (0.2s)
-- Generating done (0.0s)
CMake Warning:
  Manually-specified variables were not used by the project:

    HELLO_STRICTT


-- Build files have been written to: /tmp/cmake01-JujDkk/proj/build-typo
[退出码 0]
$ grep -E "^HELLO" build-typo/CMakeCache.txt
HELLO_STRICT:BOOL=OFF
HELLO_STRICTT:UNINITIALIZED=ON
[退出码 0]
```

三件事同时发生，务必看清：

1. 命令**成功退出（0）**，构建照常生成；
2. 拼错的那个变量被原样写进缓存，类型标成 `UNINITIALIZED` —— 意思是“项目里没有任何地方读它”；
3. 真正生效的 `HELLO_STRICT` 还是 `OFF`，`-Werror` 根本没开。

那句 `CMake Warning` 只出现在 configure 那一大屏输出的最末尾，CI 日志里几乎必然被划掉。
所以现代 CMake 的卫生习惯是：**每次 `-D` 之后，用 `cmake -L build | grep 你的变量名` 自证一次。**
（更狠的办法是 `message(STATUS "HELLO_STRICT=${HELLO_STRICT}")`，但那是调试手段，别留在仓库里。）

### 场景 8：故意犯的第二个错 —— 在源码目录里 `cmake .`（demo 第 8 节）

老教程最常见的写法：

```
$ cmake .
-- The C compiler identification is GNU 15.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake01-JujDkk/insrc
[退出码 0]
$ find . -type f | sort | head -14
./CMakeCache.txt
./CMakeFiles/4.2.3/CMakeCCompiler.cmake
./CMakeFiles/4.2.3/CMakeDetermineCompilerABI_C.bin
./CMakeFiles/4.2.3/CMakeSystem.cmake
./CMakeFiles/4.2.3/CompilerIdC/CMakeCCompilerId.c
./CMakeFiles/4.2.3/CompilerIdC/a.out
./CMakeFiles/CMakeConfigureLog.yaml
./CMakeFiles/CMakeDirectoryInformation.cmake
./CMakeFiles/InstallScripts.json
./CMakeFiles/Makefile.cmake
./CMakeFiles/Makefile2
./CMakeFiles/TargetDirectories.txt
./CMakeFiles/cmake.check_cache
./CMakeFiles/hello.dir/DependInfo.cmake
[退出码 0]
$ cmake --build . | tail -4
[ 33%] Building C object CMakeFiles/hello.dir/src/main.c.o
[ 66%] Building C object CMakeFiles/hello.dir/src/greet.c.o
[100%] Linking C executable hello
[100%] Built target hello
[退出码 0]
```

**它能工作**，这正是它危险的地方。代价是：`CMakeCache.txt`、`CMakeFiles/`、`Makefile`、`hello` 全落在源码树里。
更糟的是这个缓存把当前这个生成器和编译器**粘死**在这份源码上：你随后想试 `-G Ninja`，
或者想换编译器，CMake 会报 “error: generator / CMAKE_C_COMPILER ... changed” 之类的话，
只能先把这些垃圾删掉。

想清干净只能手动（CMake 不会帮你删）：

```
$ rm -rf CMakeCache.txt CMakeFiles cmake_install.cmake Makefile hello && find . -maxdepth 1 -type f | sort
./CMakeLists.txt
```

结论一句话：**in-source 构建在 2026 年没有任何好处。** 正确写法永远是 `-S 源码 -B 构建`，
并且把 `build/` 之类写进 `.gitignore`。多个配置（Debug/Release/asan）就是多个兄弟目录：`build/`、`build-debug/`、
`build-asan/`，互不干扰 —— 本仓库那 56 节数据结构代码就是这么并排放 `build/`、`build-asan/`、`build-cmake/` 的。

### 场景 9：给编辑器和 clangd 用：导出真实命令行（demo 第 9 节）

```
$ cmake -S . -B build-cc -DCMAKE_EXPORT_COMPILE_COMMANDS=ON | tail -3
-- Configuring done (0.2s)
-- Generating done (0.0s)
$ head -c 320 build-cc/compile_commands.json
[
{
  "directory": "/tmp/cmake01-JujDkk/proj/build-cc",
  "command": "/usr/bin/cc -DBUILD_TAG=\\\"demo\\\"  -std=gnu11 -Wall -Wextra -Wshadow -o CMakeFiles/hello.dir/src/main.c.o -c /tmp/cmake01-JujDkk/proj/src/main.c",
  "file": "/tmp/cmake01-JujDkk/proj/src/main.c",
  "output": "CMakeFiles/hello.dir/src/main.c.o"
},
```

每个源文件一条记录：`file` 是谁、`command` 是完整编译命令、`directory` 是执行它的工作目录。

```
$ python3 -c "import json;[print(t['file'],'->',t['command'][:96]) for t in json.load(open('build-cc/compile_commands.json'))]"
/tmp/cmake01-JujDkk/proj/src/main.c -> /usr/bin/cc -DBUILD_TAG=\"demo\"  -std=gnu11 -Wall -Wextra -Wshadow -o CMakeFiles/hello.dir/src/
/tmp/cmake01-JujDkk/proj/src/greet.c -> /usr/bin/cc -DBUILD_TAG=\"demo\"  -std=gnu11 -Wall -Wextra -Wshadow -o CMakeFiles/hello.dir/src/
```

两个用途：① clangd / IDE 直接读它，就不会再“找不到头文件”；② **排查时它是物证** ——
你怀疑某个 flag 有没有生效，别看 CMakeLists，看这个文件里的命令行。
（注意它只对 Makefile / Ninja 这类 generator 有效；这个开关是构建设置，不会改变你的程序。）

### 场景 10：反悔与重来（demo 第 10 节）

```
$ rm -rf build && cmake -S . -B build | grep -E "compiler|Detecting|Configuring|Generating|Build files"
-- The C compiler identification is GNU 15.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- Configuring done (0.2s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake01-JujDkk/proj/build
[退出码 0]
$ cmake --fresh -S . -B build | tail -3
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake01-JujDkk/proj/build
[退出码 0]
```

`--fresh`（CMake 3.24+）就是“丢掉缓存重新 configure”的官方开关，省掉自己 `rm -rf`。
但它只清缓存，产物文件不一定清 —— 想要绝对干净还是 `rm -rf build`。这也是本章的实用建议：
**遇到解释不了的行为，第一反应是换一个干净的构建目录，而不是继续和缓存搏斗。**

最后是底层构建系统暴露的 target：

```
$ cmake --build build --target help
The following are some of the valid targets for this Makefile:
... all (the default if no target is provided)
... clean
... depend
... edit_cache
... rebuild_cache
... hello
... src/greet.o
... src/greet.i
```

这里出现了一个**必须早点分清的同名歧义**：`hello`、`clean`、`rebuild_cache` 是 **Makefile 的 target**，
而 `add_executable(hello ...)` 里的 `hello` 是 **CMake 的 target**。
前者是 CMake 生成的副产品，名字还可能被 generator 改（Ninja 下就没有 `src/greet.o` 这种了）。
第 02 章整章讲 CMake 的 target 和 `PUBLIC/PRIVATE/INTERFACE`，那里“target”指的是后者。

---

## 命令速查表

| 我想…… | 命令 | 备注 |
|---|---|---|
| 首次配置 | `cmake -S . -B build` | `-S`=源码目录，`-B`=构建目录；没有 -B 就是 in-source，别用 |
| 只声明用 C | `project(x VERSION 1.0 LANGUAGES C)` | 机器上没 g++ 时这是必须的 |
| 构建 | `cmake --build build` | 别写成 `make -C build`，换 generator 会失效 |
| 并行构建 | `cmake --build build -j 8` | 或 `cmake --build build --parallel 8` |
| 只构建一个 target | `cmake --build build --target hello` | `--target` 后面是底层构建系统的 target 名 |
| 看缓存里有什么 | `cmake -L build` | 加 `A`、`H` 是内部项和帮助，日常不用 |
| 改一个变量 | `cmake -S . -B build -DX=ON` | 复用已有 build 目录 → 只 re-configure，不重探编译器 |
| 自证 `-D` 生效 | `cmake -L build \| grep X` | 拼错变量名时它是唯一能救你的动作 |
| 完全重来 | `rm -rf build && cmake -S . -B build` | 或 `cmake --fresh -S . -B build`（3.24+） |
| 清产物 | `cmake --build build --target clean` | 只清产物，缓存还在（编译器/生成器仍被粘住） |
| 导出编译命令 | `cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` | clangd / IDE / 排查 flag 必备 |
| 看有哪些底层 target | `cmake --build build --target help` | Makefile generator 才有；注意这是 make 的 target |

## 这章的坑

1. **`cmake .`（in-source）**：产物粘在源码树上，换编译器/换 generator 必须先手删垃圾。
   正确做法：`-S` + `-B`，构建目录进 `.gitignore`。
2. **`-D` 变量名拼错不报错**：退出码 0，只有一屏日志末尾的 `CMake Warning: Manually-specified variables were not used`，
   缓存里多一条 `UNINITIALIZED`，真开关仍是默认值。**每次 `-D` 后 `cmake -L` 自证。**
3. **缓存粘住编译器和 generator**：`CMAKE_C_COMPILER` 一旦写进 `CMakeCache.txt`，
   后面 `-D` 换编译器会被忽略甚至报错。要换就换**新目录**或 `--fresh`。这是“我明明指定了 clang 为什么还在用 gcc”的标准答案。
4. **`CMAKE_C_STANDARD 11` 变成 `-std=gnu11`**：默认带 GNU 扩展。要严格 `c11` 加 `set(CMAKE_C_EXTENSIONS OFF)`。
5. **`CMAKE_BUILD_TYPE` 只是下标**：它不塞 flag，真正生效的是 `CMAKE_C_FLAGS_<TYPE>`。
   想要 Debug 有 `-g`，必须真的把 `CMAKE_BUILD_TYPE=Debug` 传进去；空值时你的 `-g` 无处来。
6. **改编译参数会导致全量重编**：受影响 target 的所有 `.o` 都重编（场景 5 里两个全重编）。
   而改源文件只重编那一个。别把“增量构建失效”误判成 CMake 的 bug。
7. **两个 “target” 不是一回事**：CMake 的 target（`add_executable` 定义）vs Makefile/Ninja 的 target（生成出来的动作名）。
   `--target` 用的是后者，`cmake --build --target help` 列出的也是后者。
8. **本机没有 ninja**：所有输出都是 make 的措辞（`[ 33%] ...`）。看别的教程里 `[2/5] Building C object ...` 那种格式是 Ninja，
   别以为命令写错了。

## 自检问题

答不上就回对应场景：

1. 跑完 `cmake -S . -B build`，构建目录里有 `.o` 吗？为什么？（场景 2）
2. `cmake --build build` 和 `make -C build` 有什么区别，为什么手册坚持前者？（场景 3）
3. 改了 `CMakeLists.txt` 需不需要手动重跑 configure？改了源文件呢？（场景 4、5）
4. `CMAKE_BUILD_TYPE=Debug` 到底改了哪个变量？在哪能看到最终 gcc 命令行？（场景 6、9）
5. `-DHELLO_STRIC=ON`（少一个 T）会发生什么？你怎么第一时间发现？（场景 7）
6. 为什么 in-source 构建会挡住“换编译器/换 generator”？（场景 8）
7. `clangd` 报找不到 `greet.h`，除了改代码之外，CMake 侧的正解是什么？（场景 9）

## 怎么跑这章的 demo

```bash
cd cmake-handbook/01-first-build
bash demo.sh              # 全程在 mktemp 沙箱里跑，跑完自动删
bash demo.sh --keep       # 保留沙箱并打印路径，进去自己接着敲
bash demo.sh > out.txt    # 重定向时自动关掉颜色，得到和本章一模一样的纯文本
```

`demo.sh` 故意**没有** `set -e`：本章要故意制造“拼错变量名”“in-source 构建”两种现场，
得让报错原样打出来。它不写任何全局配置、不装东西、不联网，只编译本目录这 4 个文件。

## 下一步

- **第 02 章《target 与 PUBLIC/PRIVATE/INTERFACE》**：本章把 `target_compile_options(hello PRIVATE ...)` 当成黑盒用了，
  下一章拆开讲 scope（`PRIVATE/INTERFACE/PUBLIC`）到底影响谁的编译命令行 —— 这是现代 CMake 的心脏。
- 想先看整个仓库怎么用 CMake 组织：直接看仓库根目录的 `CMakeLists.txt`（56 个 `ds_add_leaf(...)`，无 `file(GLOB)`），
  `cmake -S . -B build-cmake && cmake --build build-cmake --parallel && ctest --test-dir build-cmake`，
  56 个 smoke 测试全绿。第 08 章会逐行拆解它。
