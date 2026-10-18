# 构建的四个旋钮：把 configure/build 这件事看到字符级

> 这一章不讲新概念，只教你**取证**。前面四章讲的是"怎么写 CMakeLists"，
> 这一章讲的是"写完以后，命令行上到底发生了什么、发生在哪几个文件里、怎么把它抓出来"。
> 下面**每一段输出都是这台机器上真实跑出来的**（cmake 4.2.3 + GNU Make 4.4.1 + gcc 15.2.0 + binutils size/nm），
> 跑法见文末「怎么跑这章的 demo」。
> 输出里的 `/tmp/cmake05-Y4fEmM/...` 是 `mktemp -d` 每次随机生成的沙箱路径，你跑出来会是 `cmake05-<另外 6 位>`，
> 除此之外应当逐字节一致（会变的那几处我都标了）。

这一章的取舍很重要：**我不给你"建议怎么写"，我给你"怎么自己查出该怎么写"**。
因为构建这一层的规定版本之间会变，但"CMakeCache.txt / flags.make / .i / nm"这四个取证现场从来没变过。

---

## 你会遇到的场景

先对上号，这些是不是你的现场：

1. 你照教程敲 `cmake -G Ninja build`，报了一堆红字，其中两句是 `Ninja` 和 `CMAKE_C_COMPILER not set`，
   你以为是编译器没配好，装了三个 gcc 版本还是不行。
2. 你的目录已经用 Makefile 配过，改成 `-GNinja` 后报 `Does not match the generator used previously`，
   你删了 `CMakeCache.txt` 想重配，报错照旧，于是你以为"报错在骗人"。
3. 你 `cmake --build build --config Release`，命令成功退出 0，你以为发出去的包是优化过的，
   实际上一个 `-O` 都没有。
4. 你 `-DCMAKE_BUILD_TYPE=Debug`，`cmake` 打印了 `Configuring done`，但下次不带参数的 `cmake` 又把它变回空。
5. 你 `-DSOME_OPTION=1`（少写了一个字母），构建完全成功，但行为一点没变。
6. 你 `cmake -L build` 想看缓存，结果它噼里啪啦跑了一堆配置输出，你以为缓存文件被改坏了。
7. 项目里有两份同名头（自己一份、第三方 vendored 一份），程序链接正常、跑得不对，
   你 grep 半天找不到"到底用的哪份"。
8. 你换了编译器（gcc 换 clang），configure 冒出一句 `You have changed variables that require your cache to be deleted.`，
   你不知道它是错误还是提示，更不知道它顺手把你别的手工变量擦了。

这 8 个现场，答案分别落在本章的四节里。

---

## 心智模型：三个东西各有各的"记账本"

大多数人把这三样混成一个"构建目录"，所以查问题时无从下手。它们其实是三本独立的账：

```
   旋钮              记账在哪                        什么时候被读
 ┌────────────┐   ┌──────────────────────┐   ┌───────────────────┐
 │ -G 生成器  │   │ CMakeCache.txt       │   │ 每次 configure    │
 │ 构建类型   │──►│ + CMakeFiles/ 里的   │──►│ 读缓存 → 生成规则 │
 │ -D 变量    │   │   生成器痕迹         │   │                   │
 └────────────┘   └──────────────────────┘   └─────────┬─────────┘
                                                      │ 生成
                                                      ▼
 ┌────────────┐   ┌──────────────────────┐   ┌───────────────────┐
 │ 优化/头路径│   │ CMakeFiles/<t>.dir/  │   │ make / ninja      │
 │ 单文件标志 │──►│   flags.make         │──►│ 读这些文件去真的  │
 │            │   │   build.make link.txt│   │ 调 gcc            │
 └────────────┘   └──────────────────────┘   └───────────────────┘
```

三句话背下来：

* **configure 写规则，build 执行规则。** 规则一旦写出来，`make` 只认规则，不认你的命令行历史。
* **`CMakeCache.txt` 是 configure 的记忆，不是构建的配置。** 你想改的东西得先"进记忆"，才会"进规则"。
* **想知道真相，别看 CMake 打印了什么，看它写进磁盘的字符。** 本章后面每一节都是这么做的。

---

## 旋钮一：生成器（`-G`）——CMake 不编译代码，它只是翻译

先看最基本的：这台机器上 CMake 配出来的东西是谁在执行。

```
$ cmake -S /tmp/cmake05-Y4fEmM/proj -B /tmp/cmake05-Y4fEmM/build/one
-- The C compiler identification is GNU 15.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- 拨档回执：ALT_FIRST=OFF USE_GLOBAL_INCLUDE_DIR=OFF
-- 当前生成器 = Unix Makefiles   构建类型 = []
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake05-Y4fEmM/build/one
[退出码 0]
```

`-- 当前生成器 = Unix Makefiles   构建类型 = []` 这行是本章那个 8 行小工程自己打印的
（`message(STATUS "当前生成器 = ${CMAKE_GENERATOR}   构建类型 = [${CMAKE_BUILD_TYPE}]")`）。
**构建类型是空串** —— 这个空串是本章第二节的主角，先记住它在这儿。

生成器名是**写进缓存**的：

```
$ grep -nE '^CMAKE_GENERATOR' /tmp/cmake05-Y4fEmM/build/one/CMakeCache.txt
288:CMAKE_GENERATOR:INTERNAL=Unix Makefiles
290:CMAKE_GENERATOR_INSTANCE:INTERNAL=
292:CMAKE_GENERATOR_PLATFORM:INTERNAL=
294:CMAKE_GENERATOR_TOOLSET:INTERNAL=
```

后三个空的是 Windows 用的（VS 实例 / 平台 / 工具集），Linux 上永远是空 —— 看到它们不用慌。

真正的执行者是谁？看构建目录顶层那个 `Makefile`：

```
$ head -20 /tmp/cmake05-Y4fEmM/build/one/Makefile | grep -E "^[a-zA-Z_.-]+:"
default_target: all
.NOTPARALLEL:
.SUFFIXES:

$ grep -nE 'cmake_check_build_system|CMakeFiles/Makefile2' /tmp/cmake05-Y4fEmM/build/one/Makefile | head -8
89:all: cmake_check_build_system
91:	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 all
97:	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 clean
123:app: cmake_check_build_system
124:	$(MAKE) $(MAKESILENT) -f CMakeFiles/Makefile2 app
205:cmake_check_build_system:
```

**顶层 Makefile 里一个 `gcc` 都没有。** 它只做两件事：回调 `cmake` 检查规则是否需要重新生成
（`cmake_check_build_system`），然后把活转给 `CMakeFiles/Makefile2`。真正的编译命令在更深处（第 17 节抓它）。

顺带澄清一个高频误会 —— "生成器列表里有 Ninja"不等于"我的机器能使"：

```
$ cmake -G "不存在的生成器" -S /tmp/cmake05-Y4fEmM/proj -B /tmp/cmake05-Y4fEmM/build/none | grep -E "Unix Makefiles|Ninja|^\*"
* Unix Makefiles               = Generates standard UNIX makefiles.
  Ninja                        = Generates build.ninja files.
  Ninja Multi-Config           = Generates build-<Config>.ninja files.

$ command -v ninja; command -v gmake make cmake gcc
/usr/bin/gmake
<家目录>/.local/bin/make
/usr/bin/cmake
/usr/bin/gcc
```

`command -v ninja` 那一行**什么都没打印**（机器上没装 ninja），但上面的列表里 `Ninja` 赫然在列。
生成器名字是 CMake 自带的知识，执行工具要你自己装。行首的 `*` 标的是"不写 `-G` 时选它"。

### 同一个目录换生成器：报的是"不匹配"，不是"没装"

```
$ cmake -S /tmp/cmake05-Y4fEmM/proj -B /tmp/cmake05-Y4fEmM/build/one -GNinja
CMake Error: Error: generator : Ninja
Does not match the generator used previously: Unix Makefiles
Either remove the CMakeCache.txt file and CMakeFiles directory or choose a different binary directory.
[退出码 1]
```

这条错误**本机没装 ninja 也照样出现** —— 它压根还没走到找工具那一步。
它的意思是：这个二进制目录以前是别的生成器生成的，CMake 拒绝在同一目录里换。

而全新目录 `-GNinja` 报的是完全不同的东西：

```
$ cmake -S /tmp/cmake05-Y4fEmM/proj -B /tmp/cmake05-Y4fEmM/build/ninja1 -GNinja
CMake Error: CMake was unable to find a build program corresponding to "Ninja".  CMAKE_MAKE_PROGRAM is not set.  You probably need to select a different build tool.
CMake Error: CMAKE_C_COMPILER not set, after EnableLanguage
-- Configuring incomplete, errors occurred!
[退出码 1]
```

**两句错误要分开读**：第一句是真问题（没有 ninja 这个可执行文件）；
第二句 `CMAKE_C_COMPILER not set` 是第一句的**连锁**（生成器没定下来 → 语言没启用 → 编译器自然没检测），
不是第二个毛病。看到两句就装两个东西，是这里最常见的浪费。

`-DCMAKE_GENERATOR=Ninja` 撞的是同一堵墙，说明 `-G` 干的事就是把值写进这个缓存项：

```
$ cmake -S /tmp/cmake05-Y4fEmM/proj -B /tmp/cmake05-Y4fEmM/build/ninja2 -DCMAKE_GENERATOR=Ninja
CMake Error: CMake was unable to find a build program corresponding to "Ninja".  CMAKE_MAKE_PROGRAM is not set.  You probably need to select a different build tool.
CMake Error: CMAKE_C_COMPILER not set, after EnableLanguage
-- Configuring incomplete, errors occurred!
[退出码 1]
```

### 只删 `CMakeCache.txt` 够不够

不够。上面那句错误里 CMake 自己说了 `Either remove the CMakeCache.txt file and CMakeFiles directory`，
它说"和"，不是"或"：

```
$ rm -f /tmp/cmake05-Y4fEmM/build/ninja1/CMakeCache.txt && cmake -S /tmp/cmake05-Y4fEmM/proj -B /tmp/cmake05-Y4fEmM/build/ninja1 -GNinja
CMake Error: CMake was unable to find a build program corresponding to "Ninja".  CMAKE_MAKE_PROGRAM is not set.  You probably need to select a different build tool.
CMake Error: CMAKE_C_COMPILER not set, after EnableLanguage
-- Configuring incomplete, errors occurred!
[退出码 1]
```

注意错误已经**换了一种**：从"不匹配"变成"找不到工具"，说明换生成器这一步其实过了。
但本机确实没有 ninja，所以还是失败 —— 这个细节值得练一遍：
**判断"我到底解决了哪个问题"，办法是看报错文本变没变，而不是看它还是红的。**

正规做法是 `--fresh`（缓存和 `CMakeFiles/` 一起清掉再配置）：

```
$ cmake -S /tmp/cmake05-Y4fEmM/proj -B /tmp/cmake05-Y4fEmM/build/ninja1 --fresh -GNinja
CMake Error: CMake was unable to find a build program corresponding to "Ninja".  CMAKE_MAKE_PROGRAM is not set.  You probably need to select a different build tool.
CMake Error: CMAKE_C_COMPILER not set, after EnableLanguage
-- Configuring incomplete, errors occurred!
[退出码 1]
```

**实际用法记住三条就够：**
1. 换生成器 = 换目录。`build-make/`、`build-ninja/` 各一个，比在同一个目录里搏斗省时间。
2. 同一目录想重来 = `cmake --fresh`，别手动 `rm CMakeCache.txt`（不完整）。
3. 看到 `CMAKE_C_COMPILER not set` 先往上看一行，那里才有真原因。

---

## 旋钮二：构建类型 —— 默认是"空"，不是 Debug

这是本章最值得单独拎出来的一条，因为它的失败是**无声的**。

```
$ grep -nE "CMAKE_BUILD_TYPE:STRING" /tmp/cmake05-Y4fEmM/build/bt-empty/CMakeCache.txt
37:CMAKE_BUILD_TYPE:STRING=

$ grep -nE '^C_FLAGS|^C_INCLUDES' /tmp/cmake05-Y4fEmM/build/bt-empty/CMakeFiles/app.dir/flags.make
7:C_INCLUDES = -I/tmp/cmake05-Y4fEmM/proj/include -I/tmp/cmake05-Y4fEmM/proj/alt/include
9:C_FLAGS = 
```

`C_FLAGS = ` 后面**什么都没有**。不写 `CMAKE_BUILD_TYPE` 时，CMake 一个优化/调试标志都不加，
等于 gcc 自己的默认（`-O0`，而且不带 `-g`）。
所以"我以为默认是 Debug"的直接后果是：你发布了一个没优化的包，或者调试时 `gdb` 里没有行号 —— 两种都是无声的。

四档的真实字符，一台机器上跑出来的原样：

```
C_FLAGS(Debug         ) = [-g]
C_FLAGS(Release       ) = [-O3 -DNDEBUG]
C_FLAGS(RelWithDebInfo) = [-O2 -g -DNDEBUG]
C_FLAGS(MinSizeRel    ) = [-Os -DNDEBUG]
```

这四串是 CMake 给 GNU 编译器的默认值，configure 时写进缓存，所以缓存里可以直接读到键值：

```
$ grep -nE 'CMAKE_C_FLAGS(_DEBUG|_RELEASE|_RELWITHDEBINFO|_MINSIZEREL)?:STRING' /tmp/cmake05-Y4fEmM/build/bt-Debug/CMakeCache.txt
54:CMAKE_C_FLAGS:STRING=
57:CMAKE_C_FLAGS_DEBUG:STRING=-g
60:CMAKE_C_FLAGS_MINSIZEREL:STRING=-Os -DNDEBUG
63:CMAKE_C_FLAGS_RELEASE:STRING=-O3 -DNDEBUG
66:CMAKE_C_FLAGS_RELWITHDEBINFO:STRING=-O2 -g -DNDEBUG
```

**为什么很多人把 `-DNDEBUG` 当成"这是 Release"**：因为它只在 Release/RelWithDebInfo/MinSizeRel 三档里出现，
`Debug` 没有。`NDEBUG` 的效果是让 `assert()` 变成空操作 —— 所以"Release 里 assert 全部消失"不是玄学，是这一串字符。
你要是在 Debug 里靠 `assert` 抓到了 bug，换 Release 复现不了，原因就在这行缓存里。

### 落到二进制上：`size` 和 `nm` 作证

`flags.make` 是"打算怎么编"，`size`/`nm` 是"编出来是什么"。两个都要看。

```
$ for d in bt-empty bt-Debug bt-Release bt-RelWithDebInfo bt-MinSizeRel; do printf '%-16s ' $d; size -t /tmp/cmake05-Y4fEmM/build/$d/app | tail -1; done
bt-empty            2135	    600	      8	   2743	    ab7	(TOTALS)
bt-Debug            2135	    600	      8	   2743	    ab7	(TOTALS)
bt-Release          2014	    600	      8	   2622	    a3e	(TOTALS)
bt-RelWithDebInfo    2014	    600	      8	   2622	    a3e	(TOTALS)
bt-MinSizeRel       1963	    600	      8	   2571	    a0b	(TOTALS)
```

两个反直觉点，都是这台机器的真实数字：

* `bt-empty` 和 `bt-Debug` 的 `text` **一模一样（2135）**。因为 `-g` 只往别处加调试信息，不改代码本身；
  只有 `-O` 系列才会动 `text`。所以"加了 `-g` 让程序变大"这个印象在这列里看不到。
* `Release`(`-O3`) 和 `RelWithDebInfo`(`-O2`) 的 `text` **也一样（2014）** —— 这个小程序上 `-O2` 和 `-O3` 没差出代码量。
  别拿"我的 Release 应该比 RelWithDebInfo 小"当预期，要看自己项目的数字。

符号层面更能说明问题。`src/main.c` 里有个故意写的 `static int burn()`：

```
$ for d in bt-empty bt-Debug bt-Release bt-MinSizeRel; do printf '%-14s burn=%s calc_add=%s\n' $d "$(nm /tmp/cmake05-Y4fEmM/build/$d/app | grep -c ' burn')" "$(nm /tmp/cmake05-Y4fEmM/build/$d/app | grep -c 'calc_add')"; done
bt-empty       burn=1 calc_add=1
bt-Debug       burn=1 calc_add=1
bt-Release     burn=0 calc_add=1
bt-MinSizeRel  burn=0 calc_add=1
```

`burn` 在 Debug 里是个真函数（符号数 1），在 Release 里被**内联掉了**（符号数 0）。
这就是"优化级别"唯一诚实的验证方式：**别看 CMake 打印了什么，看符号表里还剩什么。**
（`calc_add` 一直是 1，因为它是跨文件调用的外部函数，内联不掉 —— 这个对照是故意留的。）

### 取证工具自己有个坑

上面的 `for` 循环我前面专门加了一条 `ls -l`，就是为了让"文件存在"这件事先钉死：

```
$ ls -l /tmp/cmake05-Y4fEmM/build/bt-Debug/app /tmp/cmake05-Y4fEmM/build/bt-Release/app | awk '{print $5, $9}'
18208 /tmp/cmake05-Y4fEmM/build/bt-Debug/app
16056 /tmp/cmake05-Y4fEmM/build/bt-Release/app
```

因为 `size` 在文件不存在时**不吭声**，照样给你一行全 0：

```
$ size -t /tmp/cmake05-Y4fEmM/build/这个目录根本不存在/app 2>/dev/null | tail -1
      0	      0	      0	      0	      0	(TOTALS)
[退出码 1]
```

如果你没看退出码，这行全 0 会被读成"优化生效了，代码变小到 0"。
它唯一的破绽是退出码 1，而**在管道里这个 1 会被后面的命令吃掉**（demo 里的 `runc` 加了 `set -o pipefail` 才露出来）。
教训通用：**任何"用工具数字作证"的场合，先确认被看的对象存在，再看退出码。**

### `--config` 在单配置生成器上是静默无效

这条是本章最阴的，因为它的表现是"成功"：

```
$ cmake --build /tmp/cmake05-Y4fEmM/build/bt-empty --config Release
[ 33%] Building C object CMakeFiles/app.dir/src/main.c.o
[ 66%] Building C object CMakeFiles/app.dir/src/calc.c.o
[100%] Linking C executable app
[100%] Built target app
[退出码 0]

$ grep -nE 'C_FLAGS' /tmp/cmake05-Y4fEmM/build/bt-empty/CMakeFiles/app.dir/flags.make
9:C_FLAGS = 

$ grep -nE 'CMAKE_BUILD_TYPE:STRING' /tmp/cmake05-Y4fEmM/build/bt-empty/CMakeCache.txt
37:CMAKE_BUILD_TYPE:STRING=
```

退出码 0，构建完成，`--config Release` 一个字都没多说什么 —— 而 flags 还是空的、缓存还是空串。

原因写在 `cmake --build` 自己的 usage 里：

```
  --config <cfg> = For multi-configuration tools, choose <cfg>.
```

`For multi-configuration tools`。Unix Makefiles 和 Ninja 是**单配置**生成器：
构建类型在 configure 那一刻就定死在规则文件里了，build 阶段没有任何地方能接收"换个类型"这个意图。
只有 `Ninja Multi-Config`、Xcode、Visual Studio 这类才在 build 时选配置
（**本机只有 Unix Makefiles，多配置那一侧是 CMake 文档规定，未在本章实测**）。

**正确写法**：`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`（configure 时给），
然后 `cmake --build build`。`--config` 留着给 Windows/IDE 用户。

---

## 旋钮三：缓存 —— 你的 `-D` 到底进了哪本账

### 换编译器时，CMake 会自己删缓存重跑

先用一个假编译器证明"它只负责调用"：`mycc.sh` 是个三行 shell 脚本，里面 `exec /usr/bin/gcc "$@"`。

```
$ cmake -S /tmp/cmake05-Y4fEmM/proj -B /tmp/cmake05-Y4fEmM/build/cc -DCMAKE_C_COMPILER=/tmp/cmake05-Y4fEmM/mycc.sh
-- The C compiler identification is GNU 15.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /tmp/cmake05-Y4fEmM/mycc.sh - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- 拨档回执：ALT_FIRST=OFF USE_GLOBAL_INCLUDE_DIR=OFF
-- 当前生成器 = Unix Makefiles   构建类型 = []
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake05-Y4fEmM/build/cc
[退出码 0]
```

CMake 照样识别成 `GNU 15.2.0` —— 识别是"问编译器自己"问出来的，所以包一层脚本骗不过识别，但也不需要骗。

然后在同一个目录里把编译器换回 `/usr/bin/cc`，**同时故意带一个 `-DALT_FIRST=ON`**：

```
$ cmake -S /tmp/cmake05-Y4fEmM/proj -B /tmp/cmake05-Y4fEmM/build/cc -DALT_FIRST=ON -DCMAKE_C_COMPILER=/usr/bin/cc
-- 拨档回执：ALT_FIRST=ON USE_GLOBAL_INCLUDE_DIR=OFF
-- 当前生成器 = Unix Makefiles   构建类型 = []
-- Configuring done (0.0s)
You have changed variables that require your cache to be deleted.
Configure will be re-run and you may have to reset some variables.
The following variables have changed:
CMAKE_C_COMPILER= /usr/bin/cc

-- The C compiler identification is GNU 15.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- 拨档回执：ALT_FIRST=OFF USE_GLOBAL_INCLUDE_DIR=OFF
-- 当前生成器 = Unix Makefiles   构建类型 = []
-- Configuring done (0.1s)
```

**看点全在那两行"拨档回执"**：第一次是 `ALT_FIRST=ON`，重跑之后变成 `ALT_FIRST=OFF`。
这段提示不是 FATAL（它甚至没写 Error），CMake 当场删缓存、自己重跑了一遍 configure；
而重跑时**没有再带上你这一轮的 `-D`** —— 于是所有"工程里没兜底、纯靠你手敲"的变量全回到默认。

```
$ grep -nE 'ALT_FIRST' /tmp/cmake05-Y4fEmM/build/cc/CMakeCache.txt
18:ALT_FIRST:BOOL=OFF
```

这就是"我换了个编译器，怎么别的选项全乱了"的机制。
**防御办法**：把你需要的 `-D` 固定成脚本或 preset（第 06 章），别靠记忆手敲；
或者在 `CMakeLists.txt` 里给关键项写默认值（`option(... OFF)` / `set(... CACHE ...)`），让它有兜底。

### `-D / -L / -LA / -U` 各自动了什么

一次塞两个变量，一个工程认识（`ALT_FIRST`），一个工程完全不认识（`MY_TMP`）：

```
$ cmake -S /tmp/cmake05-Y4fEmM/proj -B /tmp/cmake05-Y4fEmM/build/cache -DALT_FIRST=ON -DMY_TMP='随手塞的一个变量'
-- The C compiler identification is GNU 15.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- 拨档回执：ALT_FIRST=ON USE_GLOBAL_INCLUDE_DIR=OFF
-- 当前生成器 = Unix Makefiles   构建类型 = []
-- Configuring done (0.1s)
-- Generating done (0.0s)
CMake Warning:
  Manually-specified variables were not used by the project:

    MY_TMP


-- Build files have been written to: /tmp/cmake05-Y4fEmM/build/cache
[退出码 0]
```

`Manually-specified variables were not used by the project` —— **这就是拼错变量名时唯一的那次提醒**。
警告归警告，构建照样成功，所以你只有配置时盯着屏幕才能看见它。
把它当错误处理（CI 里 grep 这行）是本章最值得搬走的一条习惯。

它确实进了缓存，类型是 `UNINITIALIZED`（意思是"没人用过它"）：

```
$ grep -nE 'ALT_FIRST|MY_TMP|CMAKE_BUILD_TYPE' /tmp/cmake05-Y4fEmM/build/cache/CMakeCache.txt
18:ALT_FIRST:BOOL=ON
37:CMAKE_BUILD_TYPE:STRING=
226:MY_TMP:UNINITIALIZED=随手塞的一个变量
```

然后 `-L`：

```
$ cmake -L /tmp/cmake05-Y4fEmM/build/cache
-- 拨档回执：ALT_FIRST=ON USE_GLOBAL_INCLUDE_DIR=OFF
-- 当前生成器 = Unix Makefiles   构建类型 = []
-- Configuring done (0.0s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake05-Y4fEmM/build/cache
-- Cache values
ALT_FIRST:BOOL=ON
CMAKE_BUILD_TYPE:STRING=
CMAKE_INSTALL_PREFIX:PATH=/usr/local
USE_GLOBAL_INCLUDE_DIR:BOOL=OFF
[退出码 0]
```

两个看点：

* **`cmake -L` 会重新配置一次**（前面那五行 STATUS 就是 configure 又跑了一遍）。
  所以它不是"只读的查看命令"。想看又不动它 —— 直接 `grep` 那个 `CMakeCache.txt` 文件。
* **列表里没有 `MY_TMP`**，尽管它明明在缓存文件里。

有人于是想起 `-LA`（把 advanced 项也放出来）：

```
$ cmake -LA /tmp/cmake05-Y4fEmM/build/cache | grep -E "MY_TMP|ALT_FIRST|Cache values"
-- 拨档回执：ALT_FIRST=ON USE_GLOBAL_INCLUDE_DIR=OFF
-- Cache values
ALT_FIRST:BOOL=ON
```

**`-LA` 里也没有。** 所以网上"`-LA` 就能看到全部"这句话不准确：
`-L` 系列只列**带类型的**缓存项（`BOOL`/`STRING`/`PATH`/`FILEPATH`/`INTERNAL`），
`UNINITIALIZED` 的一律不列。要查你手敲进去的野变量，只能 `grep CMakeCache.txt`。

`-U` 是删除缓存项。拿 `option()` 声明的变量试：

```
$ cmake -S /tmp/cmake05-Y4fEmM/proj -B /tmp/cmake05-Y4fEmM/build/cache -UALT_FIRST
-- 拨档回执：ALT_FIRST=OFF USE_GLOBAL_INCLUDE_DIR=OFF
-- 当前生成器 = Unix Makefiles   构建类型 = []
-- Configuring done (0.0s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake05-Y4fEmM/build/cache
[退出码 0]

$ grep -nE 'ALT_FIRST' /tmp/cmake05-Y4fEmM/build/cache/CMakeCache.txt
18:ALT_FIRST:BOOL=OFF
```

`-U` 确实删了，但同一次 configure 里 `option()` 又把它**加回来**，值回到默认 `OFF`。
所以：`-U` 对 `option()` / `set(... CACHE ...)` 声明的变量只能删一时，不能删一世；
想改值就老老实实 `-D`。`-U` 真正有用的场景是删掉那些"工程里已经不认了、但缓存还留着"的旧项。

`--fresh` 是清空重来，两个变量的命运差在这儿看得最清楚：

```
$ cmake -S /tmp/cmake05-Y4fEmM/proj -B /tmp/cmake05-Y4fEmM/build/cache --fresh
-- The C compiler identification is GNU 15.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- 拨档回执：ALT_FIRST=OFF USE_GLOBAL_INCLUDE_DIR=OFF
-- 当前生成器 = Unix Makefiles   构建类型 = []
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake05-Y4fEmM/build/cache
[退出码 0]

$ grep -nE 'ALT_FIRST|MY_TMP' /tmp/cmake05-Y4fEmM/build/cache/CMakeCache.txt
18:ALT_FIRST:BOOL=OFF
```

`MY_TMP` 真没了（缓存一删它就没有归宿），`ALT_FIRST` 复活成默认 `OFF`（`option()` 兜底）。

### 手改 `CMakeCache.txt` 也生效，但目录名会开始骗人

```
$ grep -nE '^CMAKE_BUILD_TYPE:STRING' /tmp/cmake05-Y4fEmM/build/bt-Debug/CMakeCache.txt
37:CMAKE_BUILD_TYPE:STRING=Debug

$ sed -i 's/CMAKE_BUILD_TYPE:STRING=Debug/CMAKE_BUILD_TYPE:STRING=Release/' /tmp/cmake05-Y4fEmM/build/bt-Debug/CMakeCache.txt
[退出码 0]

$ cmake -S /tmp/cmake05-Y4fEmM/proj -B /tmp/cmake05-Y4fEmM/build/bt-Debug >/dev/null && grep -nE '^C_FLAGS' /tmp/cmake05-Y4fEmM/build/bt-Debug/CMakeFiles/app.dir/flags.make
9:C_FLAGS = -O3 -DNDEBUG
```

`CMakeCache.txt` 就是个"下次 configure 读回来"的键值文件，所以手改确实生效。
**但注意**：这个目录现在名字叫 `bt-Debug`，里面装的却是 `-O3 -DNDEBUG` 的产物：

```
$ grep -nE '^CMAKE_BUILD_TYPE:STRING' /tmp/cmake05-Y4fEmM/build/bt-Debug/CMakeCache.txt
37:CMAKE_BUILD_TYPE:STRING=Release
```

"为什么我的 Debug 是优化过的、assert 全消失了" —— 就是这种现场。
结论：手改缓存是能用的调试手段，但**换构建类型请用 `-DCMAKE_BUILD_TYPE=` 或直接开个新目录**，
别让目录名和产物不一致。

---

## 旋钮四：头路径与单文件标志 —— 找错头不报错

这个工程故意放了**两份同名 `calc.h`**，`CALC_FLAVOR`/`CALC_WHICH` 的值不一样，
而 `CMakeLists.txt` 把两个目录**都**挂到 `-I` 上，只是顺序随 `ALT_FIRST` 翻转。

```
$ grep -nE 'C_INCLUDES' /tmp/cmake05-Y4fEmM/build/inc-off/CMakeFiles/app.dir/flags.make
7:C_INCLUDES = -I/tmp/cmake05-Y4fEmM/proj/include -I/tmp/cmake05-Y4fEmM/proj/alt/include

$ /tmp/cmake05-Y4fEmM/build/inc-off/app
[app] calc.h 的赢家 = include/calc.h
[app] CALC_FLAVOR   = 正牌（include/ 里那份）
[app] 双引号 "local.h" → src/local.h（同目录优先，-I 抢不过）
[app] 尖括号 <local.h> → include/local.h（同目录那一步被跳过）
[app] calc_add(2,3)=5 calc_mul(4,5)=20 burn=1159059266
[退出码 0]
```

把开关拨一下，`-I` 顺序换边，赢家就换边：

```
$ grep -nE 'C_INCLUDES' /tmp/cmake05-Y4fEmM/build/inc-on/CMakeFiles/app.dir/flags.make
7:C_INCLUDES = -I/tmp/cmake05-Y4fEmM/proj/alt/include -I/tmp/cmake05-Y4fEmM/proj/include

$ /tmp/cmake05-Y4fEmM/build/inc-on/app
[app] calc.h 的赢家 = alt/include/calc.h
[app] CALC_FLAVOR   = 山寨（alt/include/ 里那份）
[app] 双引号 "local.h" → src/local.h（同目录优先，-I 抢不过）
[app] 尖括号 <local.h> → include/local.h（同目录那一步被跳过）
[app] calc_add(2,3)=5 calc_mul(4,5)=20 burn=1159059266
[退出码 0]
```

**整条链路一个字错误都没有。** 头*找不到*才报错，头*找错*不报错 ——
这就是 vendored 依赖和项目自己头文件撞名时最难查的那类 bug：编译过、链接过、跑得不对。

### 不运行程序也能看到头是哪份：预处理器的文件标记行

`--target src/calc.i` 是"只预处理、不编译"的派生目标（清单见第 20 节 `--target help`）：

```
$ cmake --build /tmp/cmake05-Y4fEmM/build/inc-off --target src/calc.i
Preprocessing C source to CMakeFiles/app.dir/src/calc.c.i
[退出码 0]

$ grep -aE '^# [0-9]+ "' /tmp/cmake05-Y4fEmM/build/inc-off/CMakeFiles/app.dir/src/calc.c.i | grep -a calc.h
# 1 "/tmp/cmake05-Y4fEmM/proj/include/calc.h" 1

$ grep -aE '^# [0-9]+ "' /tmp/cmake05-Y4fEmM/build/inc-on/CMakeFiles/app.dir/src/calc.c.i | grep -a calc.h
# 1 "/tmp/cmake05-Y4fEmM/proj/alt/include/calc.h" 1
```

`.i` 是只跑预处理的结果。预处理器每打开一个文件，就在输出里留一行
「`#` 行号 `"路径"`」的标记 —— 这是**最硬**的头解析证据，比程序跑出来的打印更接近编译器本意。

顺带一个细节：**宏定义本身在 `.i` 里看不到**（`#define` 被预处理器消费掉了），能看到的是这个文件标记行。
所以"我想确认宏是什么值"得靠程序打印，"我想确认打开的是哪个文件"靠 `.i`。

### 双引号和尖括号不是风格问题

`main.c` 里三行 include 分别指向不同的解析路径，`.i` 的标记行了原样：

```
$ grep -aE '^# [0-9]+ "' /tmp/cmake05-Y4fEmM/build/inc-off/CMakeFiles/app.dir/src/main.c.i | grep -aE 'local.h|calc.h'
# 1 "/tmp/cmake05-Y4fEmM/proj/include/calc.h" 1
# 8 "/tmp/cmake05-Y4fEmM/proj/include/calc.h"
# 1 "/tmp/cmake05-Y4fEmM/proj/src/local.h" 1
# 1 "/tmp/cmake05-Y4fEmM/proj/include/local.h" 1
```

三条规则一次看全：

| 写法 | 解析顺序 | 本例结果 |
|---|---|---|
| `#include "calc.h"` | 先搜**包含者所在目录**（`src/`，没有）→ 再按 `-I` 顺序 | `include/calc.h` |
| `#include "local.h"` | 先搜包含者所在目录（`src/`，**有**）→ 后面不看了 | `src/local.h`（`-I` 完全插不上手） |
| `#include <local.h>` | **跳过**包含者所在目录那一步 → 直接从 `-I` 找 | `include/local.h` |

这解释了一个很多人踩过的现象：你把 `-I` 指到新版头目录，程序里 `#include "同一目录那份"` 却还编旧版 ——
因为旧版和 `.c` 在同一目录，双引号的第一步就命中了，`-I` 根本还没轮到。

工程里两份 `local.h` 的**包含宏故意不同名**（`SRC_LOCAL_H` / `INC_LOCAL_H`）。
如果同名，后 include 的那份会被防重复包含挡掉，`.i` 里只剩一行标记，这个对比就演不出来了 ——
这是写演示时容易踩的坑，写真实项目时正好相反（撞名才说明你在同一个头）。

### 老写法的 `BEFORE` 陷阱：传两个目录，顺序整个反过来

同一个 `CMakeLists.txt`，只把给头目录的命令换成全局 `include_directories(BEFORE ...)`：

```
$ grep -nE 'C_INCLUDES' /tmp/cmake05-Y4fEmM/build/legacy/CMakeFiles/app.dir/flags.make
7:C_INCLUDES = -I/tmp/cmake05-Y4fEmM/proj/alt/include -I/tmp/cmake05-Y4fEmM/proj/include

$ /tmp/cmake05-Y4fEmM/build/legacy/app | head -2
[app] calc.h 的赢家 = alt/include/calc.h
[app] CALC_FLAVOR   = 山寨（alt/include/ 里那份）
[退出码 0]
```

`ALT_FIRST` 明明是 `OFF`（`CMakeLists.txt` 里给的顺序是 `include → alt/include`），
但 `include_directories(BEFORE 目录1 目录2)` 是**把整批反着**插到最前面，结果 `alt/include` 赢了。

**同一份代码，只换了给头目录的"命令"，程序就跑成了另一份头的行为，还是不报错。**
这就是为什么第 02 章反复要求用 `target_include_directories`：
它的顺序就是你写的顺序，而且作用范围只在这个 target 上。

想抄近路用 `cmake -P` 脚本验证？不行：

```
$ printf 'include_directories(BEFORE /tmp/A /tmp/B)\n' > /tmp/cmake05-Y4fEmM/before.cmake && cmake -P /tmp/cmake05-Y4fEmM/before.cmake
CMake Error at /tmp/cmake05-Y4fEmM/before.cmake:1 (include_directories):
  include_directories command is not scriptable


[退出码 1]
```

`-P` 脚本模式没有"目录"的概念，所以目录属性类命令全不可用。
这类"只能在真工程里验"的边界，本身就是知识点。

### 单个源文件的标志落在哪：三处互相印证

`CMakeLists.txt` 里有 `set_source_files_properties(src/calc.c PROPERTIES COMPILE_FLAGS "-DFROM_SOURCE_PROPERTY=1")`。
它去哪了？

```
$ grep -nE 'FROM_SOURCE_PROPERTY' /tmp/cmake05-Y4fEmM/build/inc-off/CMakeFiles/app.dir/flags.make
11:# Custom flags: CMakeFiles/app.dir/src/calc.c.o_FLAGS = -DFROM_SOURCE_PROPERTY=1

$ grep -nE 'FROM_SOURCE_PROPERTY' /tmp/cmake05-Y4fEmM/build/inc-off/CMakeFiles/app.dir/build.make | head -3
93:	/usr/bin/cc $(C_DEFINES) $(C_INCLUDES) $(C_FLAGS) -DFROM_SOURCE_PROPERTY=1 -MD -MT CMakeFiles/app.dir/src/calc.c.o -MF CMakeFiles/app.dir/src/calc.c.o.d -o CMakeFiles/app.dir/src/calc.c.o -c /tmp/cmake05-Y4fEmM/proj/src/calc.c
```

`flags.make` 里只是一行**注释**（`# Custom flags: ...`），真正的串拼在 `build.make` 的那条命令尾巴上。
再看展开后的真实命令行（要先 `touch` 一下，否则没活干就没有编译行可打）：

```
$ touch /tmp/cmake05-Y4fEmM/proj/src/calc.c /tmp/cmake05-Y4fEmM/proj/src/main.c && cmake --build /tmp/cmake05-Y4fEmM/build/inc-off --verbose 2>&1 | grep -E '^/usr/bin/cc' | head -2
/usr/bin/cc  -I/tmp/cmake05-Y4fEmM/proj/include -I/tmp/cmake05-Y4fEmM/proj/alt/include  -DFROM_SOURCE_PROPERTY=1 -MD -MT CMakeFiles/app.dir/src/calc.c.o -MF CMakeFiles/app.dir/src/calc.c.o.d -o CMakeFiles/app.dir/src/calc.c.o -c /tmp/cmake05-Y4fEmM/proj/src/calc.c
/usr/bin/cc  -I/tmp/cmake05-Y4fEmM/proj/include -I/tmp/cmake05-Y4fEmM/proj/alt/include  -MD -MT CMakeFiles/app.dir/src/main.c.o -MF CMakeFiles/app.dir/src/main.c.o.d -o CMakeFiles/app.dir/src/main.c.o -c /tmp/cmake05-Y4fEmM/proj/src/main.c
```

第一行有 `-DFROM_SOURCE_PROPERTY=1`，第二行（`main.c`）干净。
注意两行的 `C_FLAGS` 部分都是空的 —— **单文件标志是加在那一条命令上的，不是改全局变量**，
所以它不会被 `--config`、`CMAKE_BUILD_TYPE` 之类的事情波及。

### `compile_commands.json`：编辑器眼里的世界

```
$ cmake -S /tmp/cmake05-Y4fEmM/proj -B /tmp/cmake05-Y4fEmM/build/cxx -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
-- The C compiler identification is GNU 15.2.0
-- Detecting C compiler ABI info
-- Detecting C compiler ABI info - done
-- Check for working C compiler: /usr/bin/cc - skipped
-- Detecting C compile features
-- Detecting C compile features - done
-- 拨档回执：ALT_FIRST=OFF USE_GLOBAL_INCLUDE_DIR=OFF
-- 当前生成器 = Unix Makefiles   构建类型 = []
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake05-Y4fEmM/build/cxx
[退出码 0]

$ ls -l /tmp/cmake05-Y4fEmM/build/cxx/compile_commands.json
-rw-rw-r-- 1 <用户> <用户> 679 <日期> /tmp/cmake05-Y4fEmM/build/cxx/compile_commands.json
```

（`679` 是字节数，`<日期>` 那一列是 `ls` 的 mtime —— 这两处每次重新构建都会变，其余不随机器变。）内容：

```
$ python3 -c "import json;d=json.load(open('/tmp/cmake05-Y4fEmM/build/cxx/compile_commands.json'));[print(e['file'].split('/')[-1],'|',e['command']) for e in d]"
main.c | /usr/bin/cc  -I/tmp/cmake05-Y4fEmM/proj/include -I/tmp/cmake05-Y4fEmM/proj/alt/include  -o CMakeFiles/app.dir/src/main.c.o -c /tmp/cmake05-Y4fEmM/proj/src/main.c
calc.c | /usr/bin/cc  -I/tmp/cmake05-Y4fEmM/proj/include -I/tmp/cmake05-Y4fEmM/proj/alt/include  -DFROM_SOURCE_PROPERTY=1 -o CMakeFiles/app.dir/src/calc.c.o -c /tmp/cmake05-Y4fEmM/proj/src/calc.c
```

`calc.c` 那条尾巴上有 `-DFROM_SOURCE_PROPERTY=1`，`main.c` 没有 —— 和上面 `--verbose` 完全对得上。

这个文件为什么重要：**clangd / 各种 IDE 不是 CMake**，它不知道你 `-I` 给了哪几个目录。
不导出这个文件，编辑器只能自己猜，于是"编译能过、编辑器满屏红"。
两条边界要记住：

* 只有 Makefile / Ninja 这类生成器认 `CMAKE_EXPORT_COMPILE_COMMANDS`；Visual Studio / Xcode 不管这个。
* 文件在**构建目录**里，不在源码目录。想让用户态工具直接读到，常见做法是在源码目录做个软链。

---

## 构建目录解剖：以后自己查，就按这张表

```
$ ls /tmp/cmake05-Y4fEmM/build/cxx/CMakeFiles/app.dir
DependInfo.cmake
build.make
cmake_clean.cmake
compiler_depend.make
compiler_depend.ts
depend.make
flags.make
link.txt
progress.make
src
```

| 文件 | 是谁 | 什么时候要查它 |
|---|---|---|
| `flags.make` | 这个 target 的 `C_DEFINES / C_INCLUDES / C_FLAGS` | "我的 `-D`/`-I`/`-O` 到底进了没" —— 第一现场 |
| `build.make` | 每个源文件一条编译命令的原文 | 单文件标志、`--verbose` 之外想静态看命令 |
| `link.txt` | 链接命令的**原文** | 库顺序、少链了谁、`-Wl,` 参数 |
| `DependInfo.cmake` | 这个 target 依赖哪些文件 | "为什么改了 X 没重编" |
| `progress.make` | `[ 33%]` 那些百分比的编号 | 基本不查，认得就好 |
| `src/`（同名子目录） | `.o` 与 `.o.d`，按源文件**相对路径**铺开 | 见下 |
| `CMakeCache.txt` | configure 的全部记忆 | 第 11 节 |

```
$ ls /tmp/cmake05-Y4fEmM/build/cxx/CMakeFiles/app.dir/src
calc.c.o
calc.c.o.d
main.c.o
main.c.o.d
```

`.o` 和 `.o.d` 是**按相对路径**放的，所以 `src/util.c` 和 `test/util.c` 不会撞名 ——
这也是"构建目录为什么这么大、里面为什么有一层目录树"的答案。
`.o.d` 是头依赖快照：「改了头为什么没重编」和「没改头为什么重编了」这两个问题都要在这里查。

`link.txt` 抓出来长这样：

```
    /usr/bin/cc -Wl,--dependency-file=CMakeFiles/app.dir/link.d CMakeFiles/app.dir/src/main.c.o CMakeFiles/app.dir/src/calc.c.o -o app
```

---

## 把底层工具接进来：`--target` 和 `--`

`--target help` 是 Makefile 生成器白送的一张表（本机没装 ninja，Ninja 下有没有等价物**未实测**）：

```
$ cmake --build /tmp/cmake05-Y4fEmM/build/inc-off --target help
The following are some of the valid targets for this Makefile:
... all (the default if no target is provided)
... clean
... depend
... edit_cache
... rebuild_cache
... app
... src/calc.o
... src/calc.i
... src/calc.s
... src/main.o
... src/main.i
... src/main.s
[退出码 0]
```

`src/calc.i`（预处理）、`src/calc.s`（汇编）这两类**派生目标**很有用：
不用改 `CMakeLists.txt`，就能让编译器只跑到中间步骤 —— 第 14 节的头证据就是这么拿的。

target 名字写错时，错误不是 CMake 抛的：

```
$ cmake --build /tmp/cmake05-Y4fEmM/build/inc-off --target 我要一个不存在的目标
gmake: *** No rule to make target '我要一个不存在的目标'.  Stop.
[退出码 2]
```

CMake 只把名字转告底层工具，**自己并不校验 target 名**（它不知道你在 VS/Xcode 里想指向哪个虚拟文件夹）。
所以看到 `gmake: ***` 开头的错误，说明请求已经出CMake了。

`--` 之后的一切原封不动给 make：

```
$ cmake --build /tmp/cmake05-Y4fEmM/build/inc-off -- -n
/usr/bin/cmake -S/tmp/cmake05-Y4fEmM/proj -B/tmp/cmake05-Y4fEmM/build/inc-off --check-build-system CMakeFiles/Makefile.cmake 0
/usr/bin/cmake -E cmake_progress_start /tmp/cmake05-Y4fEmM/build/inc-off/CMakeFiles /tmp/cmake05-Y4fEmM/build/inc-off//CMakeFiles/progress.marks
/usr/bin/gmake -s -f CMakeFiles/Makefile2 all
/usr/bin/gmake -s -f CMakeFiles/app.dir/build.make CMakeFiles/app.dir/depend
/usr/bin/gmake -s -f CMakeFiles/app.dir/build.make CMakeFiles/app.dir/build
[退出码 0]
```

`-n` 是干跑。这条逃生口很重要：**底层工具的一半功能都能从这里进去**（`-n` 干跑、`-q` 只问要不要重编、
`--keep-going` 出错别停、`-l` 限制负载）。usage 原文里就是这么写的：

```
  --             = Pass remaining options to the native tool.
```

并发控制：

```
$ cmake --build /tmp/cmake05-Y4fEmM/build/inc-off -j 2 && cmake --build /tmp/cmake05-Y4fEmM/build/inc-off --clean-first
[100%] Built target app
[ 33%] Building C object CMakeFiles/app.dir/src/calc.c.o
[ 66%] Building C object CMakeFiles/app.dir/src/main.c.o
[100%] Linking C executable app
[100%] Built target app
[退出码 0]
```

`-j` 不给数字时用底层工具的默认并发；也可以在环境变量 `CMAKE_BUILD_PARALLEL_LEVEL` 里定死，
这样 CI 上不同机器不用改脚本。

---

## 排错速查

| 现象 | 真实原因 | 怎么办 |
|---|---|---|
| `-GNinja` 报 `Does not match the generator used previously` | 同目录换生成器 | 换目录，或 `--fresh`（别只删 `CMakeCache.txt`） |
| `-GNinja` 报 `was unable to find a build program` | 机器上没有 ninja 可执行文件 | 装 ninja，或回到 `-G "Unix Makefiles"` |
| 上面这句后面还跟 `CMAKE_C_COMPILER not set` | 连锁错误，不是第二个毛病 | 只解决前一条 |
| `--config Release` 退出 0 但没优化 | 单配置生成器不接收 build 期配置 | configure 时 `-DCMAKE_BUILD_TYPE=Release` |
| `size`/`nm` 数字全是 0 | 产物不存在 | 先 `ls`；`size` 对缺失文件也打全 0 行 |
| Release 里 `assert` 全消失 | `-DNDEBUG` 在 Release 三档里是默认 | 需要断言就用 `RelWithDebInfo`+`-U NDEBUG`，或自己判 |
| `-DSOME_OPTION=1` 成功但没生效 | 变量名拼错（注意只警告一次） | 找 `Manually-specified variables were not used`；`grep CMakeCache.txt` |
| `cmake -L` 突然跑了一堆配置 | `-L` 会重新 configure | 只读就 `grep CMakeCache.txt` |
| `-L`/`-LA` 里找不到我 `-D` 的变量 | `UNINITIALIZED` 类型不列 | `grep CMakeCache.txt` |
| `-U` 删了 option 又复活 | `option()` 在 configure 时兜底 | 用 `-D` 改值，不要用 `-U` |
| 换编译器后别的选项全乱 | CMake 删缓存重跑，不再带你的 `-D` | 用脚本/preset 固定 `-D`；关键项写进 `CMakeLists.txt` |
| 程序行为像用了旧版头，编译无警告 | `-I` 顺序 / 双引号命中同目录那份 | `.i` 的 `# 1 "路径"` 标记行定位；`target_include_directories` 取代全局 `BEFORE` |
| 编辑器满屏红但能编译 | 没导出编译数据库 | `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` |

---

## 怎么跑这章的 demo

```
cd cmake-handbook/05-build-internals
bash demo.sh                    # 跑完自动删沙箱
bash demo.sh --keep             # 保留沙箱，自己进去翻 build/ 里的文件
bash demo.sh > out.txt          # 重定向自动关颜色
```

`demo.sh` 会自己 `mktemp -d` 开沙箱，把本章 8 个源文件复制进去再构建，
**仓库目录里一个构建产物都不会留**（第 21 节自证）。全程不联网，`--prefix` 不指向 `/usr/local`。

本章的 21 节顺序和上面文章的节次略有出入（demo 里取证是按命令流排的），
建议对着读：先看文章建立框架，再用 demo 找那条命令的完整上下文。

沙箱路径每次不同（`cmake05-` + 6 位随机），本文引用的那一轮是 `/tmp/cmake05-Y4fEmM`。

---

## 术语小抄

| 词 | 大白话 |
|---|---|
| 生成器 generator | CMake 这次要把规则**写成哪种方言**（Makefile / ninja / VS 工程） |
| 单配置 / 多配置 | 规则里有没有"同时装 Debug 和 Release"的位置。Makefile/Ninja 没有 |
| `CMakeCache.txt` | configure 的记忆本。键值文本，可 grep、可手改 |
| `UNINITIALIZED` | 你 `-D` 进去了但工程从来没用过它 —— 拼错变量的形状 |
| `--fresh` | 删掉缓存 + `CMakeFiles/` 再重新 configure |
| `flags.make` | 一个 target 的"编译参数清单"，`-D/-I/-O` 的最终落点 |
| `link.txt` | 链接命令原文，库顺序问题的第一现场 |
| `.i` / `.s` | 只预处理 / 只编译到汇编的派生目标，取证用 |
| 编译数据库 | `compile_commands.json`，给编辑器复现编译命令用的 |

---

## 下一步

第 06 章讲**怎么把本章这些旋钮固化下来**：CTest（`enable_testing` / `add_test` / `ctest`）、
Sanitizer 的正确接法（以及为什么不能直接 `-fsanitize=address` 硬编）、
以及 `CMakePresets.json` —— 它正好解决本章第 10 节那个"手敲的 `-D` 一换编译器就蒸发"的问题。
