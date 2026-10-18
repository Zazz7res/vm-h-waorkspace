# 第 06 章 · CTest + Sanitizer + CMakePresets：把旋钮固化成一条命令

> 前面五章一直在**拨旋钮**：target、作用域、库、依赖、生成器/缓存/flags.make。
> 这一章回答旋钮拨完之后那个真正的问题：**"这份工程现在到底算通过了没有"，是谁说了算、在哪本账上、怎么让它说话。**
>
> 下面**每一段输出都是这台机器上真实跑出来的**（cmake 4.2.3 + GNU Make 4.4.1 + gcc 15.2.0 + binutils ld 的 ld.bfd），
> 由 `bash demo.sh` 一次生成，共 30 节，跑法见文末「怎么跑这章的 demo」。
> 输出里的 `/tmp/cmake06-Fson4O/...` 是 `mktemp -d` 随机生成的沙箱路径，你跑出来会是 `cmake06-<另外 6 位>`；
> 会变的不止路径：**ASan 报告里的 PID、地址、BuildId、`sec*proc` 的耗时数字每次都变**，这些我都单独标了。

本章的取舍：**我不给你"标准答案式的 CMakeLists"，我给你"三本账 + 一把尺子"**。
CTest 的名单、Sanitizer 的命令行、preset 的 JSON —— 它们是三个独立的东西，
出事的时候长得一模一样（都是"配置成功、构建成功、ctest 退 0"），
所以本章花一半篇幅在演**假绿灯**，另一半在演**怎么把它抓出来**。

---

## 你会遇到的场景

先对上号，这些是不是你的现场：

1. CI 上一片绿，`ctest` 退 0，你很开心。后来发现**这个仓库一条用例都没注册** —— 或者注册了，但没人写 `enable_testing()`。
2. 你给 target 加了 `set_target_properties(... SANITIZER "address")`，网上搜来的写法，`get_target_property` 还原样读回来了，可越界 bug 一个都没抓到。
3. 你 `-fsanitize=address` 加进编译标志就完事了，结果构建直接停在链接，报一堆 `undefined reference to '__asan_init'`，你以为 gcc 的 asan 坏了。
4. 你把 `add_compile_options(-fsanitize=address)` 写在 `add_executable` 后面，构建成功、测试全绿 —— **ASan 从头到尾没进去过**。
5. UBSan 明明在屏幕上打印了 `runtime error: ...`，你的测试还是 `Passed`。
6. 同一份代码，`ctest` 一次跑 12 条用例，其中一条被 `***Timeout` 掐了；你把超时改成 4 秒它就绿了 —— 你分不清是"修好了"还是"跑得慢"。
7. 你 `cmake --build` 忘了跑，直接 `ctest`，屏幕上出现 30 行 `Looked in the following places`，你以为测试框架坏了。
8. 你手敲 `cmake -S . -B build -DXXX=ON -DYYY=Debug`，换台机器忘了其中两个 `-D`，行为就不一样了；你想把这套参数固化，只找到 `CMakePresets.json`，然后被它的报错和 `$penv` 折磨。
9. 你用 `cmake --preset dbg` 配好了，`cmake --build --preset dbg` 却说 `No such build preset` —— 你以为 preset 是一份名单。
10. 你 `cmake --preset base`（那个 `hidden: true` 的公共底座），报 `Cannot use hidden preset`。

这 10 个现场，答案分别落在本章的第 2/3 节、13 节、14 节、15 节、19 节、10 节、5 节、21–28 节、24 节、24 节。

---

## 一句话版本

* **CTest 的账本是 `CTestTestfile.cmake`**，不是你的 `CMakeLists.txt`。注册没生效，去那个文件里看，一眼定案。
* **ctest 退 0 至少有三种意思**：真的全过、一条都没跑、没找到配置。屏幕上"绿灯"这一件事不可信，要看汇总里那行数。
* **Sanitizer 是两半的活**：编译期插检查，链接期带运行时。少一半就停在链接；只看 `CMakeLists.txt` 看不出它装没装，要看 `flags.make` 和 `link.txt`。
* **CMake 对不认识的属性名照单全收**：能 set 能 get ≠ 生效。
* **preset 是三份名单**：configure / build / test 各一份，同名不互通；命令行永远盖过 preset。

---

## 引用约定（这一章怎么读）

1. **围栏里的都是真输出**，一行没改；我给的解释放在围栏外面，用 ★ 或"注意"起头。
2. `$ ` 开头的是**敲的命令**，它的 stdout+stderr 原样跟在下面。`demo.sh` 每条命令后面都会打一行 `[退出码 N]`，那是脚本加的，不是 CMake 打的 —— 本章凡是"退出码很重要"的地方都靠它。
3. 命令里出现 `| grep`、`| tail` 的，是**为了屏幕干净**；脚本统一 `set -o pipefail`，所以 `[退出码]` 报的是整条管道的真实成败（谁挂了报谁）。
4. `〔在 9a503fd8 里〕`、`〔在 06-ctest-sanitizers-presets 里〕` 这两行是脚本的记号：命令在那个目录里跑，但屏幕上**不印它的绝对路径**（免得把本机登录名带进文档）。
5. **每次运行都会变的东西**：ASan 报告里的 `==31226==` 这种 PID、`0x7404365e0060` 这种地址、`(BuildId: ...)`、`Total Test time (real)`、`0.09 sec` 这类耗时。它们的**形状**是稳定的，值不是。
6. 我用的 target 名、测试名、开关名都带 `CH06_` / `ch06` 前缀，是为了和仓库根那份 56 节的 `CMakeLists.txt` 区分开；**本章不动它一个字节**（第 29 节末尾自证）。

---

## 心智模型：本章有三本账，别把它们混成一本

```
   你写的东西             落到哪本账                     谁读它
 ┌──────────────────┐   ┌──────────────────────────┐   ┌──────────────────┐
 │ add_test()       │   │ <构建目录>/              │   │ ctest            │
 │ set_tests_       │──►│   CTestTestfile.cmake    │──►│ 读名单 → 跑可执  │
 │ properties()     │   │ （配置期生成，纯 CMake   │   │ 行文件 → 按属性  │
 │ enable_testing() │   │   脚本）                 │   │ 裁判 → 给退出码  │
 └──────────────────┘   └──────────────────────────┘   └──────────────────┘
 ┌──────────────────┐   ┌──────────────────────────┐   ┌──────────────────┐
 │ target_compile_  │   │ CMakeFiles/<t>.dir/      │   │ make → 真的调    │
 │ options()        │──►│   flags.make             │──►│ gcc：编译标志 +  │
 │ target_link_     │   │ <t>.dir/link.txt         │   │ 链接命令         │
 │ options()        │   │ （第 05 章的取证现场）   │   │                  │
 └──────────────────┘   └──────────────────────────┘   └──────────────────┘
 ┌──────────────────┐   ┌──────────────────────────┐   ┌──────────────────┐
 │ CMakePresets.json│   │ 不落盘成"规则"，它是     │   │ cmake/ctest 在   │
 │ CMakeUserPresets │──►│ **命令行替身**：展开成    │──►│ 启动那一刻替你   │
 │                  │   │ -S/-B/-D/生成器/环境      │   │ 敲那一串参数     │
 └──────────────────┘   └──────────────────────────┘   └──────────────────┘
```

三句话背下来：

* **第一本账（CTestTestfile.cmake）决定"有哪些用例"。** 你以为注册了 12 条、它里面只有 0 条，那就是 0 条。
* **第二本账（flags.make / link.txt）决定"程序里有没有 sanitizer"。** `CMakeLists.txt` 里写了 `-fsanitize` 不算，进了这两行才算。
* **第三本账其实是"没有账"**：preset 不生成任何规则，它只是**你没敲参数时 CMake 替你敲的那一串**。所以它盖不过命令行，也没法盖住缓存里已经存的值。

### 读报错先认「谁在说话」

这一章同时有 **5 个说话人**。它们都在同一个终端里打印，认出是谁比记住它更重要：

| 屏幕上是什么样 | 说话人 | 本章哪节 | 真实退出码 |
|---|---|---|---|
| `CMake Error at CMakeLists.txt:15 (set_tests_properties)` | 配置期的 CMake | 11 | 1 |
| `CMake Error: Cannot use hidden preset ...` | 命令行解析期的 CMake | 24 | 1 |
| `undefined reference to '__asan_init'` + `collect2: error: ld returned 1` | 链接期的 ld/collect2 | 14 | 2 |
| `ERROR: AddressSanitizer: heap-buffer-overflow` / `runtime error: ...` | 程序自己（sanitizer 运行时） | 17、19 | 1 或 8（ctest 汇总） |
| `***Failed` / `***Timeout` / `No tests were found!!!` | ctest 的裁判 | 2、3、6、8、10 | **8**，但"没找到"那类是 **0** |

最后一行是本章最疼的一条：**裁判说"我什么都没测"的时候，退的是 0。**

---

## 第 0 部分 · 先把工程立起来

### 第 1 节 CTest 的最小闭环：注册之后，名单到底落在哪个文件里

CTest 没有数据库、没有守护进程、没有配置文件。它的全部"名单"就是构建目录里**一个 CMake 脚本**：

```
$ cmake -S /tmp/cmake06-Fson4O/ch06 -B /tmp/cmake06-Fson4O/b/main 2>&1 | tail -6
-- CH06_GREETING = 来自 CMakeLists 的默认
-- CH06_TIMEOUT  = 1
-- 第 06 章工程就绪：ASan=OFF UBSan=OFF 静态asan=OFF
-- Configuring done (0.2s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake06-Fson4O/b/main
[退出码 0]

$ ls -1 /tmp/cmake06-Fson4O/b/main/CTestTestfile.cmake && echo '(配置期就生成了，构建之前它已经存在)'
/tmp/cmake06-Fson4O/b/main/CTestTestfile.cmake
(配置期就生成了，构建之前它已经存在)
[退出码 0]
```

★ 注意时间顺序：`CTestTestfile.cmake` 是**配置期**就写出来的，构建之前它已经在那儿了。
这解释了本章后面所有事 —— 名单在 configure 那一刻就定死，`ctest` 只是个读名单的播放器。

然后 `ctest -N`（只列名单、不执行）：

```
$ cmake --build /tmp/cmake06-Fson4O/b/main -j4 >/dev/null && ctest --test-dir /tmp/cmake06-Fson4O/b/main -N
Test project /tmp/cmake06-Fson4O/b/main
  Test  #1: smoke_ok
  Test  #2: via_bare_target
  Test  #3: via_target_file
  Test  #4: leak_asan
  Test  #5: oob_asan
  Test  #6: ub_asan
  Test  #7: regex_wrong
  Test  #8: known_defect
  Test #10: env_plain
  Test #11: env_tagged
  Test #12: configure_time_expand

Total Tests: 12
[退出码 0]
```

（我把 `Test #9: slow_times_out` 那一行省了，它在真输出里就夹在 8 和 10 之间 —— 它是第 10 节的靶子。）

**这条命令上面为什么有个 `>/dev/null` 的构建？** 不是为了让测试跑，是因为 `-N` 会挨个去**查可执行文件在不在**。没构建过的时候，它会给每一条用例刷一坨 `Could not find executable` + 十几个候选路径：

```
$ rm -rf /tmp/cmake06-Fson4O/b/noisy && cmake -S /tmp/cmake06-Fson4O/ch06 -B /tmp/cmake06-Fson4O/b/noisy >/dev/null 2>&1 && ctest --test-dir /tmp/cmake06-Fson4O/b/noisy -N 2>&1 | grep -c 'Could not find executable'
11
[退出码 0]
```

同一个 `-N`，只是没构建：**11 条用例 = 11 坨刷屏**。`-N` 是"只列名单"，但它仍然要做可执行文件查找 —— 这条经常被当成"ctest 坏了"，其实是正常的。

### 第 2 节 陷阱一：写了 `add_test`，但没写 `enable_testing()`

`variants/no-enable-testing/` 是章内六个"故意写坏"的小工程之一，内容只有三行有用：

```cmake
add_executable(ok "${CMAKE_CURRENT_LIST_DIR}/../../src/ok.c")
add_test(NAME should_be_invisible COMMAND ok)      # ← 没有 enable_testing()
```

配置一路绿灯：

```
$ cmake -S /tmp/cmake06-Fson4O/ch06/variants/no-enable-testing -B /tmp/cmake06-Fson4O/b/noenable 2>&1 | tail -3
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake06-Fson4O/b/noenable
[退出码 0]
```

★ 屏幕上没有任何一句"你注册了测试但没开总闸"。CMake 不警告、不报错、不提。

然后 ctest 到场：

```
$ ls -1 /tmp/cmake06-Fson4O/b/noenable/CTestTestfile.cmake ; echo '(上面是 ls 的报错：这个文件根本没生成)'
ls: cannot access '/tmp/cmake06-Fson4O/b/noenable/CTestTestfile.cmake': No such file or directory
(上面是 ls 的报错：这个文件根本没生成)
[退出码 0]

$ ctest --test-dir /tmp/cmake06-Fson4O/b/noenable 2>&1
Test project /tmp/cmake06-Fson4O/b/noenable
No tests were found!!!
[退出码 0]
```

**两句要记住的话**：

* 名单文件**根本没生成**（`ls` 那句话是铁证），`add_test` 那行静默消失了。
* `ctest` 打印的是 `No tests were found!!!`，**退出码 0**。

★ 很多教程会让你去找 `No test configuration file found!` 那句 —— 它确实存在，但**不是 `--test-dir` 这条路**。下面两条探针把边界找出来：

```
$ cd /tmp/cmake06-Fson4O/b/noenable && ctest 2>&1
*********************************
No test configuration file found!
*********************************
Usage

  ctest [options]

[退出码 0]

$ ctest --test-dir /tmp/cmake06-Fson4O/这个目录根本不存在 2>&1
Failed to change working directory to "/tmp/cmake06-Fson4O/这个目录根本不存在": No such file or directory
[退出码 1]
```

三条路，三种屏幕，**只有一条退非 0**：

| 怎么跑的 ctest | 屏幕第一句 | 退出码 |
|---|---|---|
| `ctest --test-dir <配置过但没名单>` | `No tests were found!!!` | **0** |
| `cd <目录> && ctest`（不带 --test-dir） | `No test configuration file found!` + Usage | **0** |
| `ctest --test-dir <不存在的目录>` | `Failed to change working directory to ...` | **1** |

CI 只看退出码的话，前两条路都是「配置成功、构建成功、测试通过」—— 但它一个测试都没跑。

### 第 3 节 陷阱二：`enable_testing()` 写了，但一条 `add_test` 都没有

第二根陷阱单独成节，是因为它**和第一根陷阱的屏幕一模一样**：

```
$ cmake -S /tmp/cmake06-Fson4O/ch06/variants/zero-tests -B /tmp/cmake06-Fson4O/b/zerotest >/dev/null && echo '(配置成功)'
(配置成功)
[退出码 0]

$ ls -1 /tmp/cmake06-Fson4O/b/zerotest/CTestTestfile.cmake && echo '(这回文件在，但里面一条用例都没有)'
/tmp/cmake06-Fson4O/b/zerotest/CTestTestfile.cmake
(这回文件在，但里面一条用例都没有)
[退出码 0]

$ ctest --test-dir /tmp/cmake06-Fson4O/b/zerotest 2>&1
Test project /tmp/cmake06-Fson4O/b/zerotest
No tests were found!!!
[退出码 0]
```

对照第 2 节：`ctest` 那三行输出**一字不差**。区别只在第一条 `ls` —— 这回文件在，里面是空的。

★ 所以「忘了总闸」和「注册了零条用例」在终端上无法区分，**只能去数名单文件**。
这就是本章反复要你 `cat CTestTestfile.cmake` 的原因，也是我一直不肯把 `-N` 只当成"列名单"的原因：
`-N` 的最后一行 `Total Tests: 12` 才是真正的验收点。

顺带说一句：想在 CI 上把这两种假绿灯堵住，可用的土办法就一条 ——
`ctest -N | grep -c '  Test '` 拿条数做门槛，或者给 `-R` 的筛子结果做断言。**本章没有演这条**（见文末「本章没演」）。

### 第 4 节 解剖 `CTestTestfile.cmake`：裸 target 名和 `$<TARGET_FILE:>` 最后长一样

这是本章最重要的一次"翻账本"。第 1 节那个工程里我在 `CMakeLists.txt` 写了 12 条 `add_test`，屏幕上翻出来：

```
$ grep -nE "add_test" /tmp/cmake06-Fson4O/b/main/CTestTestfile.cmake
7:add_test([=[smoke_ok]=] "/tmp/cmake06-Fson4O/b/main/ok")
8:set_tests_properties([=[smoke_ok]=] PROPERTIES  LABELS "smoke" _BACKTRACE_TRIPLES "/tmp/cmake06-Fson4O/ch06/CMakeLists.txt;93;add_test;/tmp/cmake06-Fson4O/ch06/CMakeLists.txt;0;")
9:add_test([=[via_bare_target]=] "/tmp/cmake06-Fson4O/b/main/ok")
11:add_test([=[via_target_file]=] "/tmp/cmake06-Fson4O/b/main/ok")
13:add_test([=[leak_asan]=] "/tmp/cmake06-Fson4O/b/main/leak")
22:add_test([=[known_defect]=] "/tmp/cmake06-Fson4O/b/main/nonzero")
24:add_test([=[slow_times_out]=] "/tmp/cmake06-Fson4O/b/main/slow")
26:add_test([=[env_plain]=] "/tmp/cmake06-Fson4O/b/main/env_demo")
28:add_test([=[env_tagged]=] "/tmp/cmake06-Fson4O/b/main/env_demo")
30:add_test([=[configure_time_expand]=] "/usr/bin/cmake" "-E" "echo" "配置期展开的 PWD = []")
[退出码 0]
```

（我按行号挑了 10 行，真输出里 12 条 `add_test` + 12 条 `set_tests_properties` 交替排列，共 24 行。）

三个信息，一条条说：

1. **`COMMAND ok` 变成了绝对路径 `/tmp/.../b/main/ok`。** 我一条 `add_test` 写的是裸 target 名（`via_bare_target`），
   另一条写的是 `$<TARGET_FILE:ok>` 生成器表达式（`via_target_file`）——
   账本里两者**完全相同**（第 9 行和第 11 行）。所以"裸名不行、必须写 genex"这类说法可以直接否掉：
   对 `add_test` 而言，同一个目录里的裸 target 名会被解析成它的产物路径。
2. **每条 `add_test` 都会配一条 `set_tests_properties`，哪怕你没写属性。**
   那是 CMake 在挂 `_BACKTRACE_TRIPLES`，值形如 `<CMakeLists 路径>;<行号>;add_test;<...>;0;`。
   看到第 8 行末尾的 `;93;add_test;` 就知道：这条用例是 `CMakeLists.txt` **第 93 行**注册的。
   ★ 排查"重复注册 / 哪一行的用例 / 这条属性谁加的"，靠的就是这个行号，不用猜。
3. **`[=[ ]=]` 是 CMake 的 bracket argument**，里面原样存名字，不展开变量 ——
   所以测试名里出现 `[` `$` 之类的怪字符也不会被吃掉。

数一下名单条数，就是本章反复要用的那个动作：

```
$ grep -c 'add_test' /tmp/cmake06-Fson4O/b/main/CTestTestfile.cmake && echo '(名单里一共几条用例)'
24
(名单里一共几条用例)
[退出码 0]
```

★ 24 不是 12 —— 因为 `set_tests_properties` 那些行里也含字符串 `add_test`（`_BACKTRACE_TRIPLES` 里的 `;add_test;`）。
**"数几条用例"这件事，用 `ctest -N` 的 `Total Tests: 12`，别用 `grep -c add_test`。**
这条我自己撞了才写下来：字段名会骗人，`-N` 的最后一行不会。

### 第 5 节 ctest 不替你构建：只配置不 make，ctest 会说什么

很多工具的直觉是"跑测试会自动先把东西编出来"。ctest **不是**。

```
$ rm -rf /tmp/cmake06-Fson4O/b/nobuild && cmake -S /tmp/cmake06-Fson4O/ch06 -B /tmp/cmake06-Fson4O/b/nobuild >/dev/null 2>&1 && echo '(只配置，不构建)'
(只配置，不构建)
[退出码 0]

$ ctest --test-dir /tmp/cmake06-Fson4O/b/nobuild -R smoke_ok 2>&1
Test project /tmp/cmake06-Fson4O/b/nobuild
    Start 1: smoke_ok
Could not find executable /tmp/cmake06-Fson4O/b/nobuild/ok
Looked in the following places:
/tmp/cmake06-Fson4O/b/nobuild/ok
/tmp/cmake06-Fson4O/b/nobuild/ok
/tmp/cmake06-Fson4O/b/nobuild/Release/ok
/tmp/cmake06-Fson4O/b/nobuild/Debug/ok
/tmp/cmake06-Fson4O/b/nobuild/MinSizeRel/ok
/tmp/cmake06-Fson4O/b/nobuild/RelWithDebInfo/ok
/tmp/cmake06-Fson4O/b/nobuild/Deployment/ok
/tmp/cmake06-Fson4O/b/nobuild/Development/ok
Unable to find executable: /tmp/cmake06-Fson4O/b/nobuild/ok
1/1 Test #1: smoke_ok .........................***Not Run   0.00 sec

0% tests passed, 1 tests failed out of 1

The following tests FAILED:
	  1 - smoke_ok (Not Run)                                smoke
Errors while running CTest
Output from these tests are in: /tmp/cmake06-Fson4O/b/nobuild/Testing/Temporary/LastTest.log
Use "--rerun-failed --output-on-failure" to re-run the failed cases verbosely.
[退出码 8]
```

（中间那坨候选路径我砍成 8 行，真的输出里每个路径都出现两次：带前导 `/` 和不带的。）

* **`***Not Run` 不是"测试失败"，是"要测的东西还没造出来"。** 但它在汇总里和真失败一样计入 `tests failed`，退出码一样是 8。
* 那串 `Release/ Debug/ MinSizeRel/ RelWithDebInfo/ Deployment/ Development/` 是**多配置生成器的候选目录**（第 05 章旋钮一讲过）。
  本机 Unix Makefiles 是单配置生成器，产物直接落在构建目录根，所以第一行候选就命中了 —— 一旦命中不到，它就把每种配置都试一遍给你看。
* 正确顺序永远是三条：`cmake -S -B` → `cmake --build` → `ctest`。第 22 节用 preset 把这三条钉死。

---

## 第 1 部分 · CTest：谁在裁判你的测试

### 第 6 节 第一次真跑：看 ctest 的汇总和退出码

```
$ cmake --build /tmp/cmake06-Fson4O/b/main -j4 2>&1 | tail -3
[ 77%] Built target ub
[ 88%] Built target slow
[100%] Built target env_demo
[退出码 0]

$ ctest --test-dir /tmp/cmake06-Fson4O/b/main 2>&1
Test project /tmp/cmake06-Fson4O/b/main
      Start  1: smoke_ok
 1/12 Test  #1: smoke_ok .........................   Passed    0.00 sec
      Start  4: leak_asan
 4/12 Test #4: leak_asan ........................   Passed    0.00 sec
      Start  5: oob_asan
 5/12 Test #5: oob_asan .........................   Passed    0.00 sec
      Start  6: ub_asan
 6/12 Test #6: ub_asan ..........................   Passed    0.00 sec
      Start  7: regex_wrong
 7/12 Test #7: regex_wrong ......................***Failed  Required regular expression not found. Regex=[结果 = 42

]  0.00 sec
      Start  9: slow_times_out
 9/12 Test #9: slow_times_out ...................***Timeout   1.00 sec
      Start  8: known_defect
 8/12 Test #8: known_defect .....................   Passed    0.00 sec

83% tests passed, 2 tests failed out of 12

Label Time Summary:
asan     =   0.00 sec*proc (3 tests)
env      =   0.00 sec*proc (1 test)
smoke    =   0.00 sec*proc (2 tests)

Total Test time (real) =   1.02 sec

The following tests FAILED:
	  7 - regex_wrong (Failed)                              smoke
	  9 - slow_times_out (Timeout)
Errors while running CTest
Output from these tests are in: /tmp/cmake06-Fson4O/b/main/Testing/Temporary/LastTest.log
Use "--rerun-failed --output-on-failure" to re-run the failed cases verbosely.
[退出码 8]
```

（12 条我只贴了 7 条，中间省略的都是 `Passed`；真输出的顺序就是编号 1→12。）

这张屏幕读法，按重要性排：

1. **`83% tests passed, 2 tests failed out of 12`** —— 分母 `out of 12` 是"名单里到底几条"，
   第 2/3 节那两种假绿灯就是死在这个数上（它会写成 `out of 0`）。
2. **`退出码 8`** —— ctest 的"有测试没过"。它不等于配置错误（那是 cmake 退 1），也不等于编译失败（make 退 2）。
3. **`***Failed Required regular expression not found. Regex=[...]`** —— 裁判是**属性**决定的，第 8 节展开。
4. **`***Timeout 1.00 sec`** —— 注意这个 `1.00` 来自哪：第 10 节。
5. **`Label Time Summary`** —— 按 `LABELS` 分组的耗时，`sec*proc` 是"秒×用例数"，并行跑时它会大于真实时间。
6. **`Output from these tests are in: .../Testing/Temporary/LastTest.log`** ——
   ★ 这是**失败现场的完整日志文件**。CI 上刷掉的输出全在这个文件里，比 `-V` 重跑一遍快。

### 第 7 节 挑着跑：`-N` / `-R` / `-L` 三种筛子，以及 `-V`、`--output-on-failure`

```
$ ctest --test-dir /tmp/cmake06-Fson4O/b/main -N -L asan
Test project /tmp/cmake06-Fson4O/b/main
  Test #4: leak_asan
  Test #5: oob_asan
  Test #6: ub_asan

Total Tests: 3
[退出码 0]

$ ctest --test-dir /tmp/cmake06-Fson4O/b/main -R 'env_' 2>&1 | grep -E 'Test #|tests passed'
1/2 Test #10: env_plain ........................   Passed    0.00 sec
2/2 Test #11: env_tagged .......................   Passed    0.00 sec
100% tests passed, 0 tests failed out of 2
[退出码 0]

$ ctest --test-dir /tmp/cmake06-Fson4O/b/main -L env -V 2>&1 | grep -E 'Test command|CH06_TAG|Passed|Failed'
11: Test command: /tmp/cmake06-Fson4O/b/main/env_demo
11:  CH06_TAG=来自测试属性
11: env_demo: CH06_TAG = 来自测试属性
1/1 Test #11: env_tagged .......................   Passed    0.00 sec
[退出码 0]
```

| 你要什么 | 用什么 | 本章的实测细节 |
|---|---|---|
| 只看名单，一条都别跑 | `-N` | 仍然会做可执行文件查找（第 1 节那 11 坨刷屏） |
| 按名字筛 | `-R 正则` | 正则匹配的是**用例名**，`'env_'` 命中 `env_plain`+`env_tagged` 两条 |
| 按标签筛 | `-L 标签` | 标签来自 `LABELS` 属性；`-N -L asan` 只列不跑 |
| 看真正执行了什么命令 | `-V` | `Test command:` 那行是**解析完的最终命令行**，第 12 节用它取证 |
| 只关心失败的输出 | `--output-on-failure` | 过了就一个字都不打印（对比 `-V` 全打） |
| 只重跑上回失败的 | `--rerun-failed` | 第 6 节屏幕最后那句提示就是它 |

★ 一个容易踩的点：`-R` 和 `-L` **同时给**时是交集，不是并集；筛剩 0 条时屏幕回到
`No tests were found!!!` + 退出码 0（第 2 节那张表的第一行）。所以"我明明加了筛子怎么没跑"和"这个仓库没测试"在屏幕上是同一句话。

### 第 8 节 裁判一：`PASS_REGULAR_EXPRESSION` —— 退出码之外的第二把尺子

先看看被裁判的那个程序自己是什么样子：

```
$ /tmp/cmake06-Fson4O/b/main/wrong; echo "程序自己退了 $?，注意是 0"
结果 = 43
程序自己退了 0，注意是 0
[退出码 0]
```

**退 0**。ctest 的默认裁判只看退出码，所以这一条默认是绿的 —— 这就是"逻辑 bug 抓不住"的根源。
给它加第二把尺子（`CMakeLists.txt` 里的写法）：

```cmake
add_test(NAME regex_wrong COMMAND wrong)
set_tests_properties(regex_wrong PROPERTIES
    PASS_REGULAR_EXPRESSION "结果 = 42\n")
```

跑出来：

```
$ ctest --test-dir /tmp/cmake06-Fson4O/b/main -R regex_wrong --output-on-failure 2>&1
Test project /tmp/cmake06-Fson4O/b/main
    Start 7: regex_wrong
1/1 Test #7: regex_wrong ......................***Failed  Required regular expression not found. Regex=[结果 = 42

]  0.00 sec
结果 = 43


0% tests passed, 1 tests failed out of 1

The following tests FAILED:
	  7 - regex_wrong (Failed)                              smoke
Errors while running CTest
[退出码 8]
```

三处细节值得停一下：

* **`Regex=[结果 = 42` 后面断了一行。** 因为 `"...\n"` 里的 `\n` 在 CMake 的引号参数里被展开成了**真换行**，
  ctest 打印模式时原样吐出来，所以屏幕上是一个方括号里跨了两行。想匹配"行尾"其实可以直接写 `结果 = 42`，
  正则里 `\n` 的意义和 CMake 字符串里的 `\n` 不是一回事。
* **`--output-on-failure` 打的 `结果 = 43` 是被测程序的 stdout**，紧跟在裁判结论后面。
  这就是"哪个是裁判说的、哪个是程序说的"的现场版。
* 反过来，同一条命令换成默认裁判的用例（`smoke_ok`）就是 `Passed`：**唯一的区别是那一条属性**。

★ `PASS_REGULAR_EXPRESSION` 的语义是"**输出里必须能匹配到至少一个**（可以给多个模式）"。
它的反面 `FAIL_REGULAR_EXPRESSION`（匹配到就判失败）和 `MISSING_REGULAR_EXPRESSION`（4.x 新加的"必须不出现"）
**本章没有演**，见文末清单 —— 我没有在本机验证过它们的准确行为，所以不给你抄。

### 第 9 节 裁判二：`WILL_FAIL` 只认退出码，不认输出

正例：一个"已知缺陷"的程序 `nonzero.c`，打印完就 `return 1`。

```
$ ctest --test-dir /tmp/cmake06-Fson4O/b/main -R known_defect -V 2>&1 | grep -E 'Test command|Result|Passed|Failed'
8: Test command: /tmp/cmake06-Fson4O/b/main/nonzero
1/1 Test #8: known_defect .....................   Passed    0.00 sec
[退出码 0]
```

退 1 + `WILL_FAIL TRUE` = `Passed`。这个用法真实存在，叫**把已知缺陷挂成哨兵**：
哪天它突然退 0（bug 被修好了），这条会翻成 `Failed`，逼你回来把它转正。

反例更有价值 —— 我把 `WILL_FAIL` 挂到了本来退 0 的 `ok` 上（`-DCH06_MISUSE_WILL_FAIL=ON`）：

```
$ cmake -S /tmp/cmake06-Fson4O/ch06 -B /tmp/cmake06-Fson4O/b/willfail -DCH06_MISUSE_WILL_FAIL=ON >/dev/null && cmake --build /tmp/cmake06-Fson4O/b/willfail --target ok -j4 >/dev/null && echo '(挂上反面开关，只重建 ok)'
(挂上反面开关，只重建 ok)
[退出码 0]

$ ctest --test-dir /tmp/cmake06-Fson4O/b/willfail -R ok_marked_willfail --output-on-failure 2>&1
Test project /tmp/cmake06-Fson4O/b/willfail
    Start 9: ok_marked_willfail
1/1 Test #9: ok_marked_willfail ...............***Failed    0.00 sec
结果 = 42


0% tests passed, 1 tests failed out of 1

The following tests FAILED:
	  9 - ok_marked_willfail (Failed)
Errors while running CTest
[退出码 8]
```

**程序输出了 `结果 = 42`（完全正确），却判 `Failed`。**
★ `WILL_FAIL` 只看退出码，一行输出都不看。名字里带 `will_fail` 不代表"这条用例预期失败"，
只代表"我期望它退非 0"—— 这是 CI 里最容易误用的一条属性。

### 第 10 节 裁判三：`TIMEOUT` —— 到点就掐，秒数来自一个缓存变量

`slow.c` 睡 3 秒；用例上 `TIMEOUT 1`（值来自 `CH06_TIMEOUT` 缓存变量）。

```
$ ctest --test-dir /tmp/cmake06-Fson4O/b/main -R slow_times_out --output-on-failure 2>&1
Test project /tmp/cmake06-Fson4O/b/main
    Start 9: slow_times_out
1/1 Test #9: slow_times_out ...................***Timeout   1.00 sec
slow: 开始睡 3 秒（用例的 TIMEOUT 设成 1 秒，等不到我跑完）


0% tests passed, 1 tests failed out of 1

The following tests FAILED:
	  9 - slow_times_out (Timeout)
Errors while running CTest
[退出码 8]
```

把超时放宽到 4 秒，**同一条用例、同一个程序、同一份源码**：

```
$ cmake -S /tmp/cmake06-Fson4O/ch06 -B /tmp/cmake06-Fson4O/b/to2 -DCH06_TIMEOUT=4 >/dev/null && cmake --build /tmp/cmake06-Fson4O/b/to2 --target slow -j4 >/dev/null && ctest --test-dir /tmp/cmake06-Fson4O/b/to2 -R slow_times_out --output-on-failure 2>&1 | tail -6
    Start 9: slow_times_out
1/1 Test #9: slow_times_out ...................   Passed    3.00 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   3.00 sec
[退出码 0]
```

三条结论：

* `***Timeout` 是 ctest **主动掐**（发信号），不是程序自己退出 —— 所以它没有"退出码 = 失败"这种线索，只有 `Timeout` 这个词。
  屏幕上第一行 `1/1 Test #9: ...***Timeout 1.00 sec` 里的 `1.00` 就是你设的那个值。
* 被掐的时候 `--output-on-failure` 还是把程序已经打印的部分给你了（`slow: 开始睡 3 秒...`），
  睡完那句没出现 —— 它确实是被砍在中途。
* **`Passed 3.00 sec` 不代表"修好了"**，只代表"你等了足够久"。这就是为什么超时值应该由 preset/CI 统一给，
  而不是每个人手敲一个 `-D`（第 22、27 节）。

★ 顺便一个 CI 陷阱：不写 `TIMEOUT` 时 ctest **不会**给你默认超时（本机没有超时用例被无限等下去的兜底）。
本章只演了"挂了 TIMEOUT 会被掐"，**没有演"没挂 TIMEOUT 的死循环"**，见文末清单。

### 第 11 节 属性写错测试名：唯一会**当场响**的那类 CTest 错误

`variants/properties-before-add-test/` 故意把 `set_tests_properties` 写在 `add_test` 前面：

```
$ cmake -S /tmp/cmake06-Fson4O/ch06/variants/properties-before-add-test -B /tmp/cmake06-Fson4O/b/badprop 2>&1 | tail -6
-- Detecting C compile features - done
CMake Error at CMakeLists.txt:15 (set_tests_properties):
  set_tests_properties Can not find test to add properties to: smoke_ok


-- Configuring incomplete, errors occurred!
[退出码 1]
```

这一节我要的是那个**不对称**：

| 错法 | 有没有声音 | 什么时候响 |
|---|---|---|
| 测试名打错 / 属性加早了 | **响**，`CMake Error at ...`，配置失败退 1 | 配置期 |
| 忘了 `enable_testing()` | 完全安静 | —— |
| 注册了零条用例 | 完全安静 | —— |
| sanitizer 没生效（第 13–15 节） | 完全安静 | —— |

★ 记住这一行：**CTest 的错里，只有"名字对不上"会响；其余基本都是哑的。**
所以配好之后花 10 秒 `ctest -N` 看一眼 `Total Tests: N`，是本章性价比最高的习惯。

### 第 12 节 `add_test` 里的 `${PWD}` 是配置期展开的，不是测试期的事

`CMakeLists.txt` 里我埋了一条：

```cmake
add_test(NAME configure_time_expand
         COMMAND ${CMAKE_COMMAND} -E echo "配置期展开的 PWD = [${PWD}]")
```

```
$ ctest --test-dir /tmp/cmake06-Fson4O/b/main -R configure_time_expand -V 2>&1 | grep -E 'Test command|配置期展开'
12: Test command: /usr/bin/cmake "-E" "echo" "配置期展开的 PWD = []"
12: 配置期展开的 PWD = []
[退出码 0]

$ grep -n 'configure_time_expand' /tmp/cmake06-Fson4O/b/main/CTestTestfile.cmake
30:add_test([=[configure_time_expand]=] "/usr/bin/cmake" "-E" "echo" "配置期展开的 PWD = []")
[退出码 0]
```

三行都在说同一件事：**方括号里那个值在配置那一刻就定下来了**，
到第 30 行已经写进账本，测试跑的时候 CMake 早就不在场了。
本机 CMake 4.2.3 里 `${PWD}` 是未定义的，展开成**空字符串**（老版本 CMake 曾把 `PWD` 当遗留变量定义过，
所以你在别的环境上看到的可能是源码目录 —— 这一点**我没有在本机复现，属于「没验证」**）。

* 想让变量在**测试期**才决定 → 用 `ENVIRONMENT` 属性（第 25 节）或 `WORKING_DIRECTORY` 属性。
* 想要可执行文件的路径在测试期正确 → 用 `$<TARGET_FILE:...>`，它是生成器表达式，**在生成期**展开，
  所以第 4 节里两种写法结果一样。

★ 判断口径：**`${}` 是配置期的事，`$<...>` 是生成期的事，测试期只认属性和环境变量。** 这一句能解决 90% 的"CMake 变量怎么没生效"。

---

## 第 2 部分 · Sanitizer：把"程序自己没喊疼"的那些 bug 抓出来

这一部分的三个反面教材（13/14/15）合起来是同一句话：
**`CMakeLists.txt` 里写了 sanitizer，和 sanitizer 真的在你二进制里，中间隔着两个可能失败的环节。**

### 第 13 节 反面教材：`set_target_properties(... SANITIZER "address")` 到底做了什么

网上流传的写法（`variants/san-property/`）：

```cmake
add_executable(oob "${CMAKE_CURRENT_LIST_DIR}/../../src/oob.c")
set_target_properties(oob PROPERTIES SANITIZER "address")
```

配置期**看起来全对**：

```
$ cmake -S /tmp/cmake06-Fson4O/ch06/variants/san-property -B /tmp/cmake06-Fson4O/b/sanprop 2>&1 | grep -E '读回来|Configuring done'
-- 读回来的 SANITIZER = address
-- 读回来的中文属性 = 照样存进去
-- Configuring done (0.1s)
[退出码 0]
```

★ 我在这份 CMakeLists 里还额外 set 了一个**中文名字的属性**（`随便写的属性名`），它也原样读回来了。
也就是说：**CMake 对不认识的 target 属性名照单全收，既不校验也不警告。**

再看真正交给编译器的东西（第 05 章的取证现场）：

```
$ grep -E '^C_FLAGS' /tmp/cmake06-Fson4O/b/sanprop/CMakeFiles/oob.dir/flags.make; echo '(↑ 这就是 ASan「开没开」唯一可信的地方)'
C_FLAGS = -Wall -Wextra
(↑ 这就是 ASan「开没开」唯一可信的地方)
[退出码 0]

$ grep -c fsanitize /tmp/cmake06-Fson4O/b/sanprop/CMakeFiles/oob.dir/flags.make
0
[退出码 1]
```

`C_FLAGS` 里只有警告集，`fsanitize` **命中 0 行、退出码 1**（这里的 `1` 是 grep 报"没找到"，不是 CMake 出错）。

后果：

```
$ cmake --build /tmp/cmake06-Fson4O/b/sanprop -j4 >/dev/null && ctest --test-dir /tmp/cmake06-Fson4O/b/sanprop 2>&1 | tail -3
100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.00 sec
[退出码 0]
```

那个越界写内存的程序，用例是**绿的**。ASan 从头到尾没进去过。

★ 三条可以带走的话：
1. 属性读写成功 ≠ 生效。想知道生效没生效，去 `flags.make` / `link.txt`。
2. 想要"属性名写错会报错"，CMake 给不了你 —— 它连中文属性名都收。
3. 一个测试全绿的工程，可能只是**没人检查过检查在不在**。

（顺带回答你可能想问的：`cmake --sanitize=address` 这种**命令行**写法本机 CMake 4.2.3 不支持，
报 `CMake Error: Unknown argument --sanitize=address`。这条是第 05 章那批探针里验过的，
**本章 demo 没有再演一次**，所以我不把它放进"已验证"的表里。）

### 第 14 节 反面教材：只加编译期、忘了链接期 → `undefined reference to '__asan_init'`

`variants/san-compile-only/` 只有 `target_compile_options(... -fsanitize=address -g)`，
`target_link_options` 那行被我注释掉了。

```
$ cmake -S /tmp/cmake06-Fson4O/ch06/variants/san-compile-only -B /tmp/cmake06-Fson4O/b/sanlink >/dev/null && echo '(配置成功：注意，配置期一点都不报错)'
(配置成功：注意，配置期一点都不报错)
[退出码 0]

$ cmake --build /tmp/cmake06-Fson4O/b/sanlink 2>&1 | tail -20
[ 50%] Building C object CMakeFiles/oob.dir/tmp/cmake06-Fson4O/ch06/src/oob.c.o
[100%] Linking C executable oob
/usr/bin/x86_64-linux-gnu-ld.bfd: CMakeFiles/oob.dir/tmp/cmake06-Fson4O/ch06/src/oob.c.o: warning: relocation against `__asan_option_detect_stack_use_after_return' in read-only section `.text'
/usr/bin/x86_64-linux-gnu-ld.bfd: CMakeFiles/oob.dir/tmp/cmake06-Fson4O/ch06/src/oob.c.o: in function `main':
/tmp/cmake06-Fson4O/ch06/src/oob.c:13:(.text+0x1c): undefined reference to `__asan_option_detect_stack_use_after_return'
/usr/bin/x86_64-linux-gnu-ld.bfd: /tmp/cmake06-Fson4O/ch06/src/oob.c:13:(.text+0x29): undefined reference to `__asan_stack_malloc_0'
/usr/bin/x86_64-linux-gnu-ld.bfd: /tmp/cmake06-Fson4O/ch06/src/oob.c:19:(.text+0xe8): undefined reference to `__asan_report_store4'
/usr/bin/x86_64-linux-gnu-ld.bfd: CMakeFiles/oob.dir/tmp/cmake06-Fson4O/ch06/src/oob.c.o: in function `_sub_D_00099_0':
/tmp/cmake06-Fson4O/ch06/src/oob.c:23:(.text+0x179): undefined reference to `__asan_unregister_globals'
/usr/bin/x86_64-linux-gnu-ld.bfd: CMakeFiles/oob.dir/tmp/cmake06-Fson4O/ch06/src/oob.c.o: in function `_sub_I_00099_1':
/tmp/cmake06-Fson4O/ch06/src/oob.c:23:(.text+0x188): undefined reference to `__asan_init'
/usr/bin/x86_64-linux-gnu-ld.bfd: /tmp/cmake06-Fson4O/ch06/src/oob.c:23:(.text+0x18d): undefined reference to `__asan_version_mismatch_check_v8'
/usr/bin/x86_64-linux-gnu-ld.bfd: /tmp/cmake06-Fson4O/ch06/src/oob.c:23:(.text+0x1a1): undefined reference to `__asan_register_globals'
/usr/bin/x86_64-linux-gnu-ld.bfd: warning: creating DT_TEXTREL in a PIE
collect2: error: ld returned 1 exit status
gmake[2]: *** [CMakeFiles/oob.dir/build.make:101: oob] Error 1
gmake[1]: *** [CMakeFiles/Makefile2:87: CMakeFiles/oob.dir/all] Error 2
gmake: *** [Makefile:91: all] Error 2
[退出码 2]
```

读法：

* 说话人是 **`ld.bfd`（链接器）**，不是 CMake。前缀 `/usr/bin/...-ld.bfd:` 就是它的署名。
* **看到 `__asan_` 打头的未定义符号，结论只有一个：编译期加了，链接期没加。**
  编译期那一步往你的指令里插了"调用 asan 运行时"的代码，运行时本体要靠链接期那半边带进来。
* 具体三个符号各管什么不用记，记形状：`__asan_init` / `__asan_report_*` / `__asan_*_globals`。
* `collect2: error: ld returned 1 exit status` + `gmake: *** Error 2` 是链路最后的两跳；
  最终 `cmake --build` 退的是 **2**（make 的退出码），不是 1。

★ 这一节最好的地方是它**会响**。第 13 节那种"安静地没生效"比它危险十倍。

### 第 15 节 反面教材：`add_compile_options` 写在 `add_executable` 之后 → 等于没写

`variants/san-order/` 的关键就是两行的**先后**：

```cmake
add_executable(oob "${CMAKE_CURRENT_LIST_DIR}/../../src/oob.c")   # ← 快照在这一刻拍
add_compile_options(-fsanitize=address -g)                        # ← 之后才改目录属性
add_link_options(-fsanitize=address)
```

```
$ cmake -S /tmp/cmake06-Fson4O/ch06/variants/san-order -B /tmp/cmake06-Fson4O/b/sanorder >/dev/null && grep -E '^C_FLAGS' /tmp/cmake06-Fson4O/b/sanorder/CMakeFiles/oob.dir/flags.make
C_FLAGS = -Wall -Wextra
[退出码 0]

$ cmake --build /tmp/cmake06-Fson4O/b/sanorder -j4 >/dev/null && /tmp/cmake06-Fson4O/b/sanorder/oob; echo "程序退出码 $?（越界写了，但它不敢说）"
oob: 写完就走，看着一切正常（下标 8）
程序退出码 0（越界写了，但它不敢说）
[退出码 0]

$ ctest --test-dir /tmp/cmake06-Fson4O/b/sanorder 2>&1 | tail -3
100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.00 sec
[退出码 0]
```

机制一句话：**`add_compile_options` / `add_link_options` 改的是"目录属性"，
而 `add_executable` 在它自己被调用的那一刻给 target 拍快照。** 写在后面，快照里什么都没有。

★ 屏幕上三个信号都在骗你：`oob` 正常打印、退出码 0、ctest 全绿。
**这个越界写（`buf[8]` 越界 4 字节）连一条 gcc 警告都没触发** —— 我用 `volatile int idx = 8`
把下标藏成运行时值，就是为了让 `-Wall -Wextra -Wshadow` 全部保持沉默，好演"安静 ≠ 正确"。

顺带解释这个 demo 脚本自己撞过的坑（它也是"取证纪律"的一部分）：

```
上面那条命令我第一次写的是 fmk sanorder oob —— 一个我自己定义在 demo.sh 里的函数。
屏幕上得到的是：bash: line 1: fmk: command not found（退出码 127）。
原因：runc 是用 bash -c 跑一个字符串的，子 shell 看不见父脚本里的函数。
```

★ 结论：**`bash -c "..."` 里要 grep 文件就把 grep 写全**，别指望外层脚本的辅助函数。
这个坑现在写在 `demo.sh` 的注释里挡着，第 40 行附近。

### 第 16 节 正确接法：编译期和链接期成对出现，两半都能取证

正解长这样（章内工程 `CMakeLists.txt` 里 `ch06_probe()` 函数的一角）：

```cmake
if(CH06_WITH_ASAN)
    target_compile_options(${_name} PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
    target_link_options(${_name} PRIVATE -fsanitize=address)
endif()
```

用 `-DCH06_WITH_ASAN=ON` 打开它：

```
$ cmake -S /tmp/cmake06-Fson4O/ch06 -B /tmp/cmake06-Fson4O/b/asan -DCH06_WITH_ASAN=ON 2>&1 | grep -E 'CH06|第 06 章'
-- CH06_GREETING = 来自 CMakeLists 的默认
-- CH06_TIMEOUT  = 1
-- 第 06 章工程就绪：ASan=ON UBSan=OFF 静态asan=OFF
[退出码 0]

$ grep -E '^C_FLAGS' /tmp/cmake06-Fson4O/b/asan/CMakeFiles/oob.dir/flags.make
C_FLAGS = -Wall -Wextra -Wshadow -fsanitize=address -g -fno-omit-frame-pointer
[退出码 0]

$ grep -E '^C_FLAGS' /tmp/cmake06-Fson4O/b/main/CMakeFiles/oob.dir/flags.make; echo '(对照：同一份 CMakeLists、没开开关的那份)' 
C_FLAGS = -Wall -Wextra -Wshadow
(对照：同一份 CMakeLists、没开开关的那份)
[退出码 0]

$ cat /tmp/cmake06-Fson4O/b/asan/CMakeFiles/oob.dir/link.txt
/usr/bin/cc -fsanitize=address -Wl,--dependency-file=CMakeFiles/oob.dir/link.d CMakeFiles/oob.dir/src/oob.c.o -o oob  libcalc.a
[退出码 0]
```

四个标志各自负责什么，一次讲完：

| 标志 | 出现在 | 少了它会怎样 |
|---|---|---|
| `-fsanitize=address`（编译） | `flags.make` | 检查代码不插进去，ASan 等于没开（第 13/15 节的安静） |
| `-fsanitize=address`（链接） | `link.txt` | 停在链接：`undefined reference to '__asan_init'`（第 14 节） |
| `-g` | `flags.make` | 报告只有函数名，没有 `oob.c:19` 这种行号 |
| `-fno-omit-frame-pointer` | `flags.make` | 栈回溯断在半路，看不出是谁调用的 |

★ 验收动作就两条命令：`grep '^C_FLAGS' flags.make` 和 `cat link.txt`。
两条都命中，ASan 才算真装上 —— **别看 `CMakeLists.txt`，别看 `Configuring done`。**

### 第 17 节 ASan 真的抓到了：同一批用例，开关一开就从绿变红

先看越界写（`oob`）。这份报告很长，我只保留了骨架（地址/PID/BuildId 每次都变，见引用约定 5）：

```
$ ctest --test-dir /tmp/cmake06-Fson4O/b/asan -R oob_asan --output-on-failure 2>&1
Test project /tmp/cmake06-Fson4O/b/asan
    Start 5: oob_asan
1/1 Test #5: oob_asan .........................***Failed    0.09 sec
=================================================================
==31226==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x7404365e0060 at pc 0x59e6b57e2335 bp 0x7ffcaee1dab0 sp 0x7ffcaee1daa0
WRITE of size 4 at 0x7404365e0060 thread T0
    #0 0x59e6b57e2334 in main /tmp/cmake06-Fson4O/ch06/src/oob.c:19
    #1 0x77d43722a600 in __libc_start_call_main ../sysdeps/nptl/libc_start_call_main.h:59
    #2 0x77d43722a717 in __libc_start_main_impl ../csu/libc-start.c:360
    #3 0x59e6b57e2184 in _start (/tmp/cmake06-Fson4O/b/asan/oob+0x1184) (BuildId: 10204491d63099e79549728e3c69f7e955e96b8e)

0x7404365e0060 is located 0 bytes after 32-byte region [0x7404365e0040,0x7404365e0060)
allocated by thread T0 here:
    #0 0x77d43772b60f in malloc ../../../../src/libsanitizer/asan/asan_malloc_linux.cpp:67
    #1 0x59e6b57e22ca in main /tmp/cmake06-Fson4O/ch06/src/oob.c:14

SUMMARY: AddressSanitizer: heap-buffer-overflow /tmp/cmake06-Fson4O/ch06/src/oob.c:19 in main
Shadow bytes around the buggy address:
=>0x7404365e0000: fa fa 00 00 00 fa fa fa 00 00 00 00[fa]fa fa fa
```

（再往下是 6 行 shadow 地图 + 23 行图例 —— `fa` = 堆 redzone、`fd` = 已释放的块，日常排错可以先不看 —— 这里整段砍掉。
砍完之后报告只剩最后一行，后面接的是 ctest 自己的汇总；`0% tests passed` 和 `The following tests FAILED` 之间
真输出里还有 `Label Time Summary:` / `Total Test time (real)` 两小段，也省了。）

```
==31226==ABORTING

0% tests passed, 1 tests failed out of 1

The following tests FAILED:
	  5 - oob_asan (Failed)                                 asan
Errors while running CTest
[退出码 8]
```

读 ASan 报告的固定顺序（比记住每个字段有用）：

1. `ERROR: AddressSanitizer: heap-buffer-overflow` —— **什么病**。
2. `WRITE of size 4 at ... thread T0` —— **读还是写、几个字节、哪个线程**。
3. `#0 ... in main oob.c:19` —— **肇事源码行**（这就是第 16 节坚持加 `-g` 的理由）。
4. `is located 0 bytes after 32-byte region` —— **越界了几个字节**。`0 bytes after` = 正好写在块尾之后一格。
5. `allocated by thread T0 here:` + `#1 ... main oob.c:14` —— **这块内存是谁 malloc 的**。
6. `SUMMARY: ...` —— 一句话给 CI 抓，也是这份报告里唯一值得贴进 issue 的行。

再看泄漏（`leak`）：

```
$ ctest --test-dir /tmp/cmake06-Fson4O/b/asan -R leak_asan --output-on-failure 2>&1 | grep -E 'LeakSanitizer|SUMMARY|Direct leak|Failed|ok$'
1/1 Test #4: leak_asan ........................***Failed    0.08 sec
==31230==ERROR: LeakSanitizer: detected memory leaks
Direct leak of 40 byte(s) in 1 object(s) allocated from:
SUMMARY: AddressSanitizer: 40 byte(s) leaked in 1 allocation(s).
	  4 - leak_asan (Failed)                                asan
[退出码 8]
```

★ `leak.c` 里 `malloc(10 * sizeof(int))` = **正好 40 字节、1 次分配**，报告里两个数字和源码严丝合缝。
另外两个概念：`Direct leak`（没人指着的块）和 `Indirect leak`（只有泄漏块才指着的块）—— 本例只有一块，所以只出 Direct。

最关键的一节对照实验 —— **同一批用例，不开 ASan 的那份构建里它们全是绿的**：

```
$ ctest --test-dir /tmp/cmake06-Fson4O/b/main -R 'leak_asan|oob_asan|ub_asan' 2>&1 | grep -E 'Test #|tests passed'
1/3 Test #4: leak_asan ........................   Passed    0.00 sec
2/3 Test #5: oob_asan .........................   Passed    0.00 sec
3/3 Test #6: ub_asan ..........................   Passed    0.00 sec
100% tests passed, 0 tests failed out of 3
[退出码 0]
```

**"测试全过"和"内存干净"是两件事。** 前者的名单里没有 sanitizer，后者要 `-DCH06_WITH_ASAN=ON` 重新构建一份。

### 第 18 节 libasan 到底链没链进来：`ldd` 的对照

ASan 的运行时在本机有两种落地方式：

```
$ ldd /tmp/cmake06-Fson4O/b/asan/oob | grep -i asan
	libasan.so.8 => /usr/lib/x86_64-linux-gnu/libasan.so.8 (0x00007c37de600000)
[退出码 0]

$ cmake -S /tmp/cmake06-Fson4O/ch06 -B /tmp/cmake06-Fson4O/b/asanstatic -DCH06_WITH_ASAN=ON -DCH06_ASAN_STATIC=ON >/dev/null && cmake --build /tmp/cmake06-Fson4O/b/asanstatic --target oob -j4 >/dev/null && ldd /tmp/cmake06-Fson4O/b/asanstatic/oob | grep -ci asan
0
[退出码 1]

$ grep -c static-libasan /tmp/cmake06-Fson4O/b/asanstatic/CMakeFiles/oob.dir/link.txt
1
[退出码 0]
```

* **动态版**（不加 `-static-libasan`）：`ldd` 里出现 `libasan.so.8`，路径在本机 `/usr/lib/x86_64-linux-gnu/`。
  开发机上毫无问题；把这份二进制拷到没装 sanitizer 运行时的机器上，就是"文件找不到"。
* **静态版**：`ldd` 命中 **0 条**（所以那个 `1` 是 grep 的退出码），asan 运行时已经焊进可执行文件。
  代价是体积大、编译稍慢。
* 括号里那个 `(0x00007c37de...)` 是**加载地址**，每次都变，别去比它。

★ 本机结论（第 05 章探针 + 本节实测）：**两种都能跑**，`-static-libasan` 不是"能不能编译"的硬要求，
而是"产物能不能拷走"的部署选择。仓库根那份 56 节的 `CMakeLists.txt` 里选了静态（第 29 节会看到原文），
那是作者的部署偏好 —— 你在别的机器上（共享 `libasan` 不存在的发行版）可能确实必须静态，
**那种情况我没有办法在本机验证**。

### 第 19 节 UBSan 最阴的一课：它报了错，测试还是绿的

第一份构建：开 UBSan，但**不加** `-fno-sanitize-recover=all`（`-DCH06_UBSAN_STOP=OFF`）。

```
$ cmake -S /tmp/cmake06-Fson4O/ch06 -B /tmp/cmake06-Fson4O/b/ubrecover -DCH06_UBSAN=ON -DCH06_UBSAN_STOP=OFF >/dev/null && cmake --build /tmp/cmake06-Fson4O/b/ubrecover --target ub -j4 >/dev/null && /tmp/cmake06-Fson4O/b/ubrecover/ub; echo "程序退出码 $?"
/tmp/cmake06-Fson4O/ch06/src/ub.c:16:20: runtime error: left shift of 1 by 31 places cannot be represented in type 'int'
ub: 1 << 31 = -2147483648（这行值不重要，重要的是有没有被抓到）
程序退出码 0
[退出码 0]

$ ctest --test-dir /tmp/cmake06-Fson4O/b/ubrecover -R ub_asan --output-on-failure 2>&1 | grep -E 'Test #|tests passed'
1/1 Test #6: ub_asan ..........................   Passed    0.00 sec
100% tests passed, 0 tests failed out of 1
[退出码 0]
```

**`runtime error` 打印出来了，程序继续跑完，退出码 0，ctest 判 `Passed`。**
UBSan 默认是 **recover 模式**："我告诉你哪儿 UB 了，然后按原来的语义继续"。
所以 UBSan 只加 `-fsanitize=undefined`、不加第二条，在 CI 上的实际效果是**只打日志不拦人**。

第二份构建：加 `-fno-sanitize-recover=all`。

```
$ cmake -S /tmp/cmake06-Fson4O/ch06 -B /tmp/cmake06-Fson4O/b/ubstop -DCH06_UBSAN=ON >/dev/null && cmake --build /tmp/cmake06-Fson4O/b/ubstop --target ub -j4 >/dev/null && /tmp/cmake06-Fson4O/b/ubstop/ub; echo "程序退出码 $?"
/tmp/cmake06-Fson4O/ch06/src/ub.c:16:20: runtime error: left shift of 1 by 31 places cannot be represented in type 'int'
程序退出码 1
[退出码 0]

$ ctest --test-dir /tmp/cmake06-Fson4O/b/ubstop -R ub_asan --output-on-failure 2>&1 | grep -E 'Test #|tests passed|FAILED'
1/1 Test #6: ub_asan ..........................***Failed    0.00 sec
0% tests passed, 1 tests failed out of 1
The following tests FAILED:
[退出码 8]
```

同一条用例，从 `Passed` 翻成 `***Failed`，差别只有那一个 flag。

* 为什么用 `volatile int shift = 31; 1 << shift`：写成 `1 << 31` 会被编译器**常量折叠**，
  运行时的 UBSan 检查根本执行不到那一行，屏幕上什么都不会报。UB 探针必须把值藏到运行时。
* `__int128` / 数组越界 / 空指针解引用之类是 **ASan** 或编译期警告的活，UBSan 管的是"语义未定义"那一层。
  两者互不替代 —— 本章两条线分开演，就是为了让这个分工可见。

### 第 20 节 三条命令的完整验收：配置(asan) → 构建 → ctest

```
$ ctest --test-dir /tmp/cmake06-Fson4O/b/asan 2>&1 | tail -8
The following tests FAILED:
	  4 - leak_asan (Failed)                                asan
	  5 - oob_asan (Failed)                                 asan
	  7 - regex_wrong (Failed)                              smoke
	  9 - slow_times_out (Timeout)
Errors while running CTest
Output from these tests are in: /tmp/cmake06-Fson4O/b/asan/Testing/Temporary/LastTest.log
Use "--rerun-failed --output-on-failure" to re-run the failed cases verbosely.
[退出码 8]
```

★ 这份名单在两个构建目录里**一模一样**（`-N` 都是 12 条），红的数量从 2 变成 4：
多出来的是 `leak_asan` 和 `oob_asan`。**它们一直都在，只是以前没人查。**

这就是本章第一部分的落点：
一份 `CMakeLists.txt`、一份名单、两种编译标志 → 两份"通过"的定义。
真实项目里这两种都得跑（一份快、一份狠），第 22 节用 preset 把它们变成两个名字。

---

## 第 2 部分结束前，插一张 sanitizer 挂法对照表

| 挂法 | 配置 | 构建 | 用例 | ASan 真的在里面吗 | 本章哪节 |
|---|---|---|---|---|---|
| `set_target_properties(SANITIZER "address")` | 成功 | 成功 | 全绿 | **不在**（安静地没进去） | 13 |
| 只 `target_compile_options(-fsanitize=...)` | 成功 | **失败** | 跑不到 | 停在链接 | 14 |
| `add_compile_options` 写在 `add_executable` 之后 | 成功 | 成功 | 全绿 | **不在**（快照没拍到） | 15 |
| `target_compile_options` + `target_link_options` | 成功 | 成功 | 变红 | **在** | 16、17 |
| `-fsanitize=undefined`（不加 recover 那条） | 成功 | 成功 | **全绿** | 在，但不改变退出码 | 19 |
| `-fsanitize=undefined -fno-sanitize-recover=all` | 成功 | 成功 | 变红 | 在且拦人 | 19 |

倒数第三行是这张表里唯一一条"东西生效了但没用上"的：**检查跑了、报告打了、退出码没改。**
CI 拿退出码当唯一裁判时，它和"没装"没有区别。

---

## 第 3 部分 · CMakePresets：把那一长串 `-D` 变成三个字

第 05 章末尾我留了一句抱怨：手敲的 `-D` 一换编译器就蒸发。这一部分就是它的解药。

### 第 21 节 preset 入门：先让 CMake 把名单念给你听

章内工程的 `CMakePresets.json` 写了 7 个 configure preset（一个是 `hidden: true` 的公共底座 `base`），
外加 build/test 各 3 个。先让 CMake 自己念：

```
$ cd /tmp/cmake06-Fson4O/ch06 && cmake --list-presets
Available configure presets:

  "dbg"         - 本地调试（Debug）
  "rel"         - 发布（Release）
  "asan"        - 带 ASan/UBSan 的调试构建
  "asan-static" - ASan + 静态 libasan
  "only-linux"  - condition：不是 Linux 就整条消失
  "tagged"      - environment：给整条命令链注入环境变量
[退出码 0]

$ cd /tmp/cmake06-Fson4O/ch06 && cmake --list-presets=build && cmake --list-presets=test
Available build presets:

  "dbg"
  "asan"
  "tagged"
Available test presets:

  "dbg"
  "asan"
  "tagged"
[退出码 0]
```

* 每条后面的中文名是 JSON 里的 **`displayName`**；没写就只显示引号里的 `name`（build/test 那两张表都没写）。
* `--list-presets` 只念 configure；要念哪一类用 `--list-presets=build|test|all`。
* ★ 这张表的条数**永远 ≤ 文件里写的条数**。少掉的是被 `hidden` 和 `condition` 藏的（第 24 节当场对质）。
  对不上时别怀疑 CMake，先数文件：

```
$ cd /tmp/cmake06-Fson4O/ch06 && python3 -c "import json;d=json.load(open('CMakePresets.json'));print('文件里写了这些：', ' '.join(p['name'] for p in d['configurePresets']))"
文件里写了这些： base dbg rel asan asan-static only-linux tagged
[退出码 0]
```

### 第 22 节 三条命令对齐：`--preset` / `--build --preset` / `ctest --preset`

```
$ cd /tmp/cmake06-Fson4O/ch06 && cmake --preset dbg 2>&1 | tail -8
-- Detecting C compile features
-- Detecting C compile features - done
-- CH06_GREETING = 来自 preset dbg
-- CH06_TIMEOUT  = 2
-- 第 06 章工程就绪：ASan=OFF UBSan=OFF 静态asan=OFF
-- Configuring done (0.1s)
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake06-Fson4O/ch06/out/dbg
[退出码 0]

$ ls -1 /tmp/cmake06-Fson4O/ch06/out && echo '(binaryDir 由 preset 里的 ${sourceDir}/out/${presetName} 决定，命令行一个 -B 都没敲)'
dbg
(binaryDir 由 preset 里的 ${sourceDir}/out/${presetName} 决定，命令行一个 -B 都没敲)
[退出码 0]

$ cd /tmp/cmake06-Fson4O/ch06 && cmake --build --preset dbg 2>&1 | tail -3
[100%] Built target ub
[100%] Built target slow
[100%] Built target env_demo
[退出码 0]

$ cd /tmp/cmake06-Fson4O/ch06 && ctest --preset dbg 2>&1 | tail -8
smoke    =   0.00 sec*proc (2 tests)

Total Test time (real) =   2.02 sec

The following tests FAILED:
	  7 - regex_wrong (Failed)                              smoke
	  9 - slow_times_out (Timeout)
Errors while running CTest
[退出码 8]
```

四个观察点：

1. **`--preset` 同时把 `-S`、`-B`、生成器、一堆 `-D` 全替你敲了。** 屏幕上"Build files have been written to"
   那个 `out/dbg` 来自 JSON 里的 `"binaryDir": "${sourceDir}/out/${presetName}"` —— `${presetName}` 就是当前这条 preset 的名字，
   所以一份 preset 一个构建目录，天然互不污染（第 05 章"一个目录一个生成器"那条约束自动满足）。
2. **`CH06_TIMEOUT = 2` 是 preset 给的**，`CH06_GREETING = 来自 preset dbg` 盖住了 `CMakeLists.txt` 里的默认值 ——
   preset 的 `cacheVariables` 优先级高于 `set(... CACHE ...)` 的**初次赋值**（和已有缓存的关系见第 23 节）。
3. **`ctest --preset dbg` 里没敲 `--output-on-failure`**，但失败用例仍然进了汇总 ——
   因为 test preset 写了 `"output": { "outputOnFailure": true }`。第 3 点是 preset 最省心的地方：
   它把"每个人记不记住那个 flag"变成"文件里写没写"。
4. **`slow_times_out` 在这里还是 `Timeout`**：preset 把超时从 1 秒放宽到 2 秒，而程序睡 3 秒。
   对照第 10 节的 4 秒版 —— **preset 的值也要有人负责**，这正是第 27 节 `CMakeUserPresets.json` 的存在理由。

### 第 23 节 覆盖顺序：命令行 `-D` 赢 preset，`-B` 赢 preset 的 `binaryDir`

```
$ cd /tmp/cmake06-Fson4O/ch06 && cmake --preset dbg -DCH06_GREETING='命令行赢了' 2>&1 | grep -E 'GREETING'
-- CH06_GREETING = 命令行赢了
[退出码 0]

$ grep -m1 '^CH06_GREETING' /tmp/cmake06-Fson4O/ch06/out/dbg/CMakeCache.txt && echo '(↑ 缓存里存的就是命令行那个值 —— 手敲的 -D 盖住了 preset 的 cacheVariables)'
CH06_GREETING:STRING=命令行赢了
(↑ 缓存里存的就是命令行那个值 —— 手敲的 -D 盖住了 preset 的 cacheVariables)
[退出码 0]
```

`-B` 也赢：

```
$ cd /tmp/cmake06-Fson4O/ch06 && cmake --preset dbg -B /tmp/cmake06-Fson4O/ch06/out/elsewhere >/dev/null && grep -m1 CMAKE_CACHEFILE_DIR /tmp/cmake06-Fson4O/ch06/out/elsewhere/CMakeCache.txt
CMAKE_CACHEFILE_DIR:INTERNAL=/tmp/cmake06-Fson4O/ch06/out/elsewhere
[退出码 0]

$ grep -m1 CMAKE_CACHEFILE_DIR /tmp/cmake06-Fson4O/ch06/out/dbg/CMakeCache.txt && echo '(↑ 对照：out/dbg 那份缓存一个字没动，这次 configure 压根没进它)'
CMAKE_CACHEFILE_DIR:INTERNAL=/tmp/cmake06-Fson4O/ch06/out/dbg
(↑ 对照：out/dbg 那份缓存一个字没动，这次 configure 压根没进它)
[退出码 0]
```

★ 记忆点：**手敲的永远赢，preset 只是"你没敲时用什么"。**
所以临时调试写 `cmake --preset dbg -DSOME_OPTION=ON` 就够了，不用另存一份 preset。

但这条有个必须记住的后半句 —— 和第 05 章的缓存那条连起来读：
上面 `-DCH06_GREETING=命令行赢了` 是**写进 `out/dbg/CMakeCache.txt`** 的。
下一次你老老实实 `cmake --preset dbg`（不带 `-D`），preset 会给这个变量重新赋值 —— 但如果 CMake 认为该缓存项
"已存在且不该被覆盖"，你看到的就还是上一次的值。**临时 `-D` 不是无痕操作，它会留在缓存里。**
（本节没有专门演这个回读现场，属于「本章没演」。）

### 第 24 节 `condition` 与 `hidden`：谁会被藏起来，藏到哪去了

先量一下三个数：

```
$ cd /tmp/cmake06-Fson4O/ch06 && cmake --list-presets | grep -c '^  \"' && echo '(↑ 名单里几条 configure preset)'
6
(↑ 名单里几条 configure preset)
[退出码 0]

$ cd /tmp/cmake06-Fson4O/ch06 && cmake --list-presets=all | grep -c '^  \"' && echo '(↑ --list-presets=all 把 configure/build/test 三类都列，条数是三张表相加)'
12
(↑ --list-presets=all 把 configure/build/test 三类都列，条数是三张表相加)
[退出码 0]

$ cd /tmp/cmake06-Fson4O/ch06 && cmake --list-presets=all | grep -c base
0
[退出码 1]
```

文件里 7 个、名单里 6 个、`all` 是 6+3+3=12 个，而 `base` **在任何名单里都命中 0 次**。
最后一条是这一节的重点：**`--list-presets=all` 的 `all` 指"三类名单都印"，不是"连 hidden 一起印"。**

`condition` 那条在本机（Linux）成立，所以它既能列也能用：

```
$ cd /tmp/cmake06-Fson4O/ch06 && cmake --preset only-linux >/dev/null && echo '(condition 成立 → 这条在本机可用)'
(condition 成立 → 这条在本机可用)
[退出码 0]

$ cd /tmp/cmake06-Fson4O/ch06 && cmake --preset base 2>&1 | grep -E 'Error|Available'
CMake Error: Cannot use hidden preset in /tmp/cmake06-Fson4O/ch06: "base"
Available configure presets:
[退出码 1]
```

★ 两个结论：

* `hidden: true` 不是"折叠显示"，是**不对外**：不进名单、不能被 `--preset` 点名，只能被别人 `inherits`。
  它是公共底座的正确写法 —— 你想让 `base` 出现在别人的 `--list-presets` 里，反而做不到。
* `condition` 不影响能不能用，只影响**在不在名单里**。它的价值是跨平台：Windows 上的同事看不到 `only-linux`，
  自然也不会误用；而 condition 成立的一侧完全无感。
  （本机是 Linux，所以"不成立的那一侧"我只演了 `--list-presets` 的差别，**没有真的换系统验证**。）

### 第 25 节 preset 的 `environment` 段：它只管 `ctest --preset` 这条路

这一步要用到第 12 节那个"配置期展开"的教训反过来用：**让环境变量在测试期才决定**。
章内 JSON 里有两处 `environment`：configure preset `tagged` 写 `"CH06_TAG": "preset 注入的"`，
test preset `tagged` 写 `"CH06_TAG": "test preset 注入的"`。三条命令把它们区分开：

```
$ cd /tmp/cmake06-Fson4O/ch06 && ctest --test-dir out/tagged -R env_ -V 2>&1 | grep -E 'env_demo: CH06_TAG|Test #'
10: env_demo: CH06_TAG = (未设置)
1/2 Test #10: env_plain ........................   Passed    0.00 sec
11: env_demo: CH06_TAG = 来自测试属性
2/2 Test #11: env_tagged .......................   Passed    0.00 sec
[退出码 0]

$ cd /tmp/cmake06-Fson4O/ch06 && ctest --preset tagged -V 2>&1 | grep -E 'env_demo: CH06_TAG|Test #'
10: env_demo: CH06_TAG = test preset 注入的
1/2 Test #10: env_plain ........................   Passed    0.00 sec
11: env_demo: CH06_TAG = 来自测试属性
2/2 Test #11: env_tagged .......................   Passed    0.00 sec
[退出码 0]

$ cd /tmp/cmake06-Fson4O/ch06 && CH06_TAG='shell 里的值' ctest --preset tagged -V 2>&1 | grep -E 'env_demo: CH06_TAG|Test #'
10: env_demo: CH06_TAG = test preset 注入的
1/2 Test #10: env_plain ........................   Passed    0.00 sec
11: env_demo: CH06_TAG = 来自测试属性
2/2 Test #11: env_tagged .......................   Passed    0.00 sec
[退出码 0]
```

三条命令读出的优先级链：

1. **CMakeLists 里 `set_tests_properties(... ENVIRONMENT "CH06_TAG=...")` 最硬**（`env_tagged` 在三条命令里都打印"来自测试属性"）。
2. **test preset 的 `environment` 次之**（`env_plain` 只在第 2、3 条命令里拿到值）。
3. **shell 里的值最弱**：第 3 条命令我明明 `CH06_TAG='shell 里的值'`，屏幕上还是 `test preset 注入的`。
4. 第 1 条命令证明了一件容易被误解的事：**configure preset 里的 `environment` 不会自动带到测试** ——
   我在 `tagged` 这个 configure preset 里写的 `"CH06_TAG": "preset 注入的"` 一次都没出现，
   手敲 `ctest --test-dir out/tagged` 时它完全是空气。

★ 一句话：**preset 的 `environment` 只对"通过该 preset 跑起来的那一步"生效。**
`env_plain` 在第 1 条命令里打印 `(未设置)`，就是这条规则的铁证。

顺带一条第 21 节已经埋好的：**configure / build / test 是三份各自独立的名单，同名不互通。**
所以第 22 节那三条命令的 preset 必须**分别存在**。你可以 `cmake --preset asan` 配得很开心，
然后 `cmake --build --preset asan` 报 `No such build preset` —— 那是两份名单，不是一份。

### 第 26 节 `$env{}` 和 `$penv{}`：读不到时是什么下场

`variants/presets/penv.json` 里两个 preset，唯一区别是一个用 `$env{CH06_GREETER}`、一个用 `$penv{CH06_GREETER}`：

```
$ cd /tmp/cmake06-Fson4O/penv && cmake --preset env-lookup 2>&1 | grep -E 'GREETING|Error'
-- CH06_GREETING = 问候语来自 
[退出码 0]

$ cd /tmp/cmake06-Fson4O/penv && cmake --preset penv-lookup 2>&1 | grep -E 'GREETING|Error'
-- CH06_GREETING = 问候语来自 
[退出码 0]

$ cd /tmp/cmake06-Fson4O/penv && CH06_GREETER='从环境里读到的问候' cmake --preset env-lookup 2>&1 | grep -E 'GREETING'
-- CH06_GREETING = 问候语来自 从环境里读到的问候
[退出码 0]

$ cd /tmp/cmake06-Fson4O/penv && CH06_GREETER='从环境里读到的问候' cmake --preset penv-lookup 2>&1 | grep -E 'GREETING'
-- CH06_GREETING = 问候语来自 从环境里读到的问候
[退出码 0]
```

★ 本节最值钱的是**前两条**：变量不存在时，两种写法都**安静地展开成空字符串**，
配置照常 `Configuring done`。也就是说 —— **`$env{}` 里拼错一个变量名不会报错，只会让你的构建配置悄悄变成另一个样子。**
这和第 13 节那个"属性名写错没人管"是同一种沉默。

（两种写法的官方区别是：`$env{}` 走 CMake 的可移植环境查找、`$penv{}` 直接读进程环境。
本机在这个用例上给出的屏幕**一模一样**，所以我不会告诉你"该用哪个"，
只告诉你两者共同的失败模式：**读不到 → 空 → 不响**。）

**本章文档里绝不出现任何真实用户名。** 所以：

* 我用的是脚本自己造的 `CH06_GREETER`。
* 你大概率会看到网上有人写 `"$penv{USER}"`、`"$penv{HOME}"` —— 那会把你的登录名、家目录
  印进 `CMakeCache.txt` 和构建目录名里，然后跟着 CI 缓存、跟着日志、跟着截图跑出去。
  **这条我在这次 demo 里没有再演一次**（刻意不演），但它属于本章的硬约束。

### 第 27 节 `CMakeUserPresets.json`：机器相关的东西不进版本库

章内公共 preset 里没有"我本机想临时把超时放宽到 8 秒"这一条。正确做法是本地另开一个文件：

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "mine",
      "displayName": "我本机的临时开关（这个文件不该提交）",
      "inherits": "dbg",
      "cacheVariables": { "CH06_TIMEOUT": "8" }
    }
  ]
}
```

把它作为 `CMakeUserPresets.json` 放进同一个目录，然后：

```
$ cd /tmp/cmake06-Fson4O/ch06 && ls -1 CMakePresets.json CMakeUserPresets.json && cmake --list-presets | grep -E 'mine|"dbg"'
CMakePresets.json
CMakeUserPresets.json
  "mine"        - 我本机的临时开关（这个文件不该提交）
  "dbg"         - 本地调试（Debug）
[退出码 0]

$ cd /tmp/cmake06-Fson4O/ch06 && cmake --preset mine 2>&1 | grep -E 'TIMEOUT|GREETING|Configuring done'
-- CH06_GREETING = 来自 preset dbg
-- CH06_TIMEOUT  = 8
-- Configuring done (0.1s)
[退出码 0]
```

★ `mine` 自己没写 `CH06_GREETING`，屏幕上却有 `来自 preset dbg` ——
**两个文件是一个预设池**，`CMakeUserPresets.json` 里的 preset 可以直接 `inherits` 公共文件里的。
这一条把"个人偏好"和"团队约定"接上了：团队约定留在 `CMakePresets.json`，个人值留在本地文件，
不用为"我机器上要多等 5 秒"新增一条公共 preset。

* `CMakeUserPresets.json` 的定位等同于 `.gitignore` 里的本地文件：**版本库只留 `CMakePresets.json`**。
  （本章的 demo 是把它写进沙箱副本的，仓库里那个文件是我临时造的，你不会在版本库里看到它。）
* 顺序上 CMake 读 `CMakePresets.json` → `CMakeUserPresets.json`，两边同名 preset 时公共文件那条为基准 ——
  **这条覆盖行为我没有实测**，见「本章没演」。

### 第 28 节 preset 的七种报错原文（报错时对照用）

这一节不解释，当字典用。全部七种，都是本机 `4.2.3` 的原话。

**一、名字不存在**（注意它顺手把可用列表念了一遍，`mine` 在第 27 节才存在过）：

```
$ cd /tmp/cmake06-Fson4O/ch06 && cmake --preset 不存在的名字 2>&1
CMake Error: No such preset in /tmp/cmake06-Fson4O/ch06: "不存在的名字"
Available configure presets:

  "mine"        - 我本机的临时开关（这个文件不该提交）
  "dbg"         - 本地调试（Debug）
  "rel"         - 发布（Release）
  "asan"        - 带 ASan/UBSan 的调试构建
  "asan-static" - ASan + 静态 libasan
  "only-linux"  - condition：不是 Linux 就整条消失
  "tagged"      - environment：给整条命令链注入环境变量
[退出码 1]
```

**二、没有 `CMakePresets.json` 的目录**：

```
$ mkdir -p /tmp/cmake06-Fson4O/nopresets && cp /tmp/cmake06-Fson4O/ch06/CMakeLists.txt /tmp/cmake06-Fson4O/nopresets/ && cd /tmp/cmake06-Fson4O/nopresets && cmake --preset dbg 2>&1
CMake Error: Could not read presets from /tmp/cmake06-Fson4O/nopresets:
File not found: /tmp/cmake06-Fson4O/nopresets/CMakePresets.json
[退出码 1]
```

**三、JSON 语法错（多一个逗号）** —— 带 `^` 指针：

```
$ cd /tmp/cmake06-Fson4O/bad-json-trailing-comma && cmake --list-presets 2>&1 | tail -5
CMake Error: Could not read presets from /tmp/cmake06-Fson4O/bad-json-trailing-comma:
CMakePresets.json:9: Missing '}' or object member name
      }
      ^
[退出码 1]
```

**四、`version` 太大**：

```
$ cd /tmp/cmake06-Fson4O/bad-version-99 && cmake --list-presets 2>&1 | tail -5
CMake Error: Could not read presets from /tmp/cmake06-Fson4O/bad-version-99:
CMakePresets.json:2: Unrecognized "version" 99: must be >=1 and <=10
  "version": 99,
             ^
[退出码 1]
```

★ 这句 `must be >=1 and <=10` 顺带告诉你**本机最高支持 preset schema 10**。
所以 `cmakePresets`/`testPresets` 之类较新字段能不能用，看的是这个数，不是 CMake 版本号。

**五、继承成环**（a 继承 b、b 继承 a）：

```
$ cd /tmp/cmake06-Fson4O/bad-cyclic-inherit && cmake --list-presets 2>&1 | tail -5
CMake Error: Could not read presets from /tmp/cmake06-Fson4O/bad-cyclic-inherit:
Cyclic preset inheritance for preset "a"
[退出码 1]
```

**六、多余的字段**（自己加个 `cat` 想当注释用）：

```
$ cd /tmp/cmake06-Fson4O/bad-extra-field && cmake --list-presets 2>&1 | tail -5
CMake Error: Could not read presets from /tmp/cmake06-Fson4O/bad-extra-field:
CMakePresets.json:7: Invalid extra field "cat" in Preset
      "cat": "这个字段不在 schema 里"
             ^
[退出码 1]
```

★ 这条对第 13 节是个反衬：**preset 的 JSON 校验很严**（多余字段直接拒绝），
而 **CMake 的 target 属性很松**（乱写名字照单全收）。同一个 CMake，两种性格。
想给 preset 加注释，JSON 做不到 —— 只能用 `displayName` / `description` 顶替。

**七、`condition` 写错类型**：

```
$ cd /tmp/cmake06-Fson4O/bad-bad-condition && cmake --list-presets 2>&1 | tail -5
CMake Error: Could not read presets from /tmp/cmake06-Fson4O/bad-bad-condition:
CMakePresets.json:7: Invalid condition for preset "dbg"
      "condition": { "type": "banana", "lhs": "x", "rhs": "x" }
                   ^
[退出码 1]
```

七条的共同点：**全部退出码 1，全部在碰构建之前就说完了。**
这一类"响的错"是 CMake 里最舒服的一类 —— 和第 2/3 节那种哑错相比，它们值得你专门记住形状。

---

## 第 4 部分 · 接到真工程上

### 第 29 节 本章的东西，在仓库根那份 56 节的 `CMakeLists.txt` 里占哪几行

**只读引用，不 configure、不 build、不写它一个字节**（命令在仓库根目录里执行，屏幕上不印绝对路径）：

```
$ 〔在 9a503fd8 里〕 grep -nE 'option\(DS_WITH_ASAN|enable_testing|add_test|target_link_options' CMakeLists.txt | head -8
27:#    target_include_directories / target_link_options），**不使用**
58:option(DS_WITH_ASAN "给每个可执行文件挂 AddressSanitizer + LeakSanitizer" OFF)
83:        target_link_options(${_target} PRIVATE -fsanitize=address -static-libasan)
87:    add_test(NAME smoke_${_target} COMMAND ${_target})
90:enable_testing()
[退出码 0]
```

那一本教材里，本章四件事各占一行：`option(...)`（开关）、`target_link_options`（链接期那半边）、
`add_test`（一条 smoke 用例）、`enable_testing()`（总闸）。

于是我在沙箱里复刻了一个**三节的小 DS**，结构一模一样：一个 `ds_add_leaf()` 函数、
每节一条 `smoke_` 用例、一个 `DS_WITH_ASAN` 开关。三份源码：静态数组（不碰堆）、
单链表（**故意只 malloc 不 free**）、数组栈（realloc + free 干净）。

```
$ cmake -S /tmp/cmake06-Fson4O/ds -B /tmp/cmake06-Fson4O/b/ds 2>&1 | tail -2 && cmake --build /tmp/cmake06-Fson4O/b/ds -j4 >/dev/null && ctest --test-dir /tmp/cmake06-Fson4O/b/ds 2>&1 | tail -3
-- Generating done (0.0s)
-- Build files have been written to: /tmp/cmake06-Fson4O/b/ds
100% tests passed, 0 tests failed out of 3

Total Test time (real) =   0.00 sec
[退出码 0]
```

三节全绿 —— **包括那节一直在漏的链表**。这就是第 17 节那个道理在真工程形状上的复现。
然后只多一个开关：

```
$ cmake -S /tmp/cmake06-Fson4O/ds -B /tmp/cmake06-Fson4O/b/dsasan -DDS_WITH_ASAN=ON >/dev/null && cmake --build $BD/dsasan -j4 >/dev/null && ctest --test-dir /tmp/cmake06-Fson4O/b/dsasan --output-on-failure 2>&1 | tail -22
==32341==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 20 byte(s) in 5 object(s) allocated from:
    #1 0x627c72f5322e in main /tmp/cmake06-Fson4O/ds/02-linked-list/slist/leaf.c:8

SUMMARY: AddressSanitizer: 20 byte(s) leaked in 5 allocation(s).

    Start 3: smoke_03-stack-stack-array
3/3 Test #3: smoke_03-stack-stack-array .......   Passed    0.01 sec

67% tests passed, 1 tests failed out of 3

The following tests FAILED:
	  2 - smoke_02-linked-list-slist (Failed)
Errors while running CTest
[退出码 8]
```

* `20 byte(s) leaked in 5 allocation(s)`：源码里 `for (int i = 0; i < 5; i++) malloc(sizeof(int))`，
  **5 次 × 4 字节 = 20 字节**，两个数字和代码对得上。
* `leaf.c:8` 直接指到 malloc 那一行。
* 名单还是 3 条，一条没多、一条没少 —— 变的是**"通过"的定义**。
* target 名 `02-linked-list-slist` 前面带数字：CMake 允许 target 名以数字开头，
  第 08 章拆那本教材时会看到同样的形状。

★ 这就是把本章接到真工程上的**全部工作量**：一个 `option`、一个把规则写一遍的函数、
一节一条 smoke 用例。剩下的都是这份手册前面五章的东西。

### 第 30 节 一页纸：命令、退出码、以及「谁在说话」

demo 脚本最后自己打了一张表（原样贴在下面，它不是命令输出，是脚本里写死的小抄）：

```text
  命令                        它到底做什么                      没做成功时屏幕上是什么
  ------------------------  --------------------------------  --------------------------
  cmake --preset dbg          按 preset 配置（含 binaryDir）    No such preset / schema 错，退 1
  cmake --build --preset dbg  构建 preset 绑定的那个目录          编译错误原样转发
  ctest --preset dbg          按 test preset 跑用例              汇总 + 退出码
  ctest --test-dir X -N       只列名单                           名单空 = 0 条，仍然退 0
  ctest --test-dir X          全跑                               全过退 0，有红退 8
  ctest -R 正则 / -L 标签      筛子（交集）                        一条都没筛中：No tests were found!!!，退 0

  读报错先认「谁在说话」：
    CMake Error                     —— 配置期的 CMake，看 -- Configuring incomplete
    undefined reference to '__asan_ —— 链接期，asan 运行时没链进来
    runtime error: / ERROR: Address —— 程序自己跑出来的，CMake 只是转述
    ***Failed / ***Timeout          —— ctest 的裁判结论，不是编译器
```

最后用两条命令自证安全边界：

```
$ 〔在 06-ctest-sanitizers-presets 里〕 find . -name CMakeCache.txt -o -name '*.o' -o -name CTestTestfile.cmake -o -type d -name out | wc -l
0
[退出码 0]

$ du -sh /tmp/cmake06-Fson4O
16M	/tmp/cmake06-Fson4O
[退出码 0]
```

**0** —— 章节目录里一个构建产物都没有。第 29 节那个"读到仓库根"的动作也只 grep 了一行，没写任何东西。

---

## 本章的坑清单（按代价从大到小）

1. **`ctest` 退 0 ≠ 测了东西。** `No tests were found!!!` 也是 0。唯一可信的是汇总里的 `out of N`。
2. **忘了 `enable_testing()`，`add_test` 静默消失**，名单文件根本不生成。配置、构建一声不吭。
3. **`set_target_properties(SANITIZER ...)` 之类的"未知属性"照单全收**：能 set、能 get、不生效。
   属性写成功不是生效的证据，`flags.make` / `link.txt` 才是。
4. **只加编译期不加链接期**：报一堆 `__asan_*` 未定义符号 —— 至少它会响。
5. **`add_compile_options` 写在 `add_executable` 之后等于没写**（目录属性 vs target 快照）。
6. **UBSan 默认 recover**：打印 `runtime error` 但退 0，测试绿。必须 `-fno-sanitize-recover=all`。
7. **`WILL_FAIL` 只看退出码，一行输出都不看。** 挂错对象会让正确的程序变红。
8. **`PASS_REGULAR_EXPRESSION` 里的 `\n` 被 CMake 展开成真换行**，ctest 打印时会断行，看着像 bug 其实是格式。
9. **`ctest` 不会替你构建。** 未构建时 `***Not Run` 混在失败汇总里，退出码同样是 8。
10. **没构建过时连 `-N` 都会刷 `Looked in the following places`**，每条用例一坨 —— 不是故障。
11. **`--test-dir` 和不带 `--test-dir` 的 ctest 报错文案不同**（`No tests were found!!!` vs `No test configuration file found!`），
    但**退出码都是 0**，别指望用文案区分。
12. **`grep -c add_test CTestTestfile.cmake` 会数多一倍**：`_BACKTRACE_TRIPLES` 里也含 `add_test` 字符串。数用例用 `ctest -N`。
13. **preset 是三份名单**（configure / build / test），同名不互通。`cmake --build --preset X` 报 `No such build preset` 时不是文件坏了。
14. **`--list-presets` 少条目不是 bug**：`hidden: true` 和 condition 不成立的都不出现。
15. **`--list-presets=all` 不会把 hidden 放出来**：`all` 指三类名单，不指"全部 preset"。
16. **hidden preset 不能被 `--preset` 点名**：`Cannot use hidden preset`，它只给 `inherits` 用。
17. **命令行的 `-D` / `-B` 盖过 preset**，且 `-D` 的值会**写进缓存**，不是无痕操作。
18. **configure preset 的 `environment` 不会带到测试**：想让测试拿到环境变量，写在 test preset 或测试属性里。
19. **`ENVIRONMENT` 测试属性 > test preset 的 `environment` > shell**，这条链靠 `ctest --preset` 才看得出。
20. **`$env{}` / `$penv{}` 读不到时展开成空字符串，不报错。** 拼错变量名的代价是"配置悄悄变了"。
21. **别在 preset 里写 `$penv{USER}`、`$penv{HOME}`**：本机身份会进缓存、进构建目录名、进 CI 日志。
22. **JSON 的校验比 CMake 属性严**：多余字段直接 `Invalid extra field`。想注释就用 `description`。
23. **`-R` 和 `-L` 同时给是交集**，筛剩 0 条也退 0（第 1 条的兄弟）。
24. **超时值没兜底**：不写 `TIMEOUT` 的用例可以无限等；写了也要有人对这个数负责（preset 里改一个值就改变结论）。
25. **`***Timeout` 是 ctest 主动掐**，和被测程序自己失败是两回事，但都计入 `tests failed`。

---

## 自检题（能答上就算这章读完了）

**1. CI 上 `ctest --test-dir build` 退 0，你就放行。这安全吗？**
不安全。至少三种情况都退 0：真的全过、名单里一条用例都没有（`No tests were found!!!`）、
构建目录配置过但没生成名单文件（同样 `No tests were found!!!`）。
要看的是汇总里 `... out of N` 那个 **N**，或者单独跑 `ctest -N` 断言条数。

**2. `add_test(NAME x COMMAND app)` 里 `app` 是裸 target 名，行不行？要写 `$<TARGET_FILE:app>` 吗？**
行。第 4 节的账本里两种写法都变成了同一个绝对路径 `<构建目录>/ok`。
裸名的限制是"该 target 必须在同一个 CMakeLists 目录里可见"；跨目录时 genex 更稳。

**3. 你怎么用两条命令判断"这个工程真的开了 ASan"？**
`grep '^C_FLAGS' <b>/CMakeFiles/<t>.dir/flags.make` 要有 `-fsanitize=address`，
`cat <b>/CMakeFiles/<t>.dir/link.txt` 也要有 `-fsanitize=address`。第 16 节那两条就是标准动作。

**4. `undefined reference to '__asan_init'` 是什么问题？`runtime error: left shift ...` 但测试绿又是什么问题？**
前者：编译期加了 ASan、链接期没加（第 14 节）。
后者：UBSan 装了、检查也报了，但默认 recover 模式不改退出码（第 19 节），
加 `-fno-sanitize-recover=all` 才拦得住。

**5. 一条用例输出完全正确却判 `***Failed`，最可能是哪两个原因？**
① 挂了 `WILL_FAIL TRUE` 而它其实退 0（第 9 节反例）；
② 挂了 `PASS_REGULAR_EXPRESSION` 而匹配模式写错（比如 `\n`、空格、换行差异，第 8 节）。
反过来，一条用例明明是 bug 却判 `Passed`，最可能是裁判只有"退出码"这一把尺子。

**6. `cmake --preset dbg` 之后，`cmake --build --preset dbg` 报 `No such build preset`。文件坏了吗？**
没坏。build preset 是**另一份名单**（`buildPresets` 数组），得单独写。
第 21 节那三条 `--list-presets` / `=build` / `=test` 就是用来分别检查这三份名单的。

**7. `cmake --preset dbg -DBUILD_TESTING=OFF` 和 preset 里的 `cacheVariables` 冲突，谁赢？会不会留痕迹？**
命令行赢（第 23 节实测），而且值会**写进 `out/dbg/CMakeCache.txt`** —— 下一次不带 `-D` 重用同一目录时，
缓存里的旧值可能还在。所以调试完要么换个构建目录，要么删掉缓存重来（第 05 章旋钮三）。

**8. 为什么本手册说"ASan 报告里的地址不要拿去和同事对"？**
`==31226==`（PID）、`0x7404365e0060`（地址）、`(BuildId: ...)`、`0.09 sec`（耗时）每次都变，
连沙箱路径都是 `mktemp` 随机名。稳定的只有**形状和文字**：`heap-buffer-overflow`、`WRITE of size 4`、
`is located 0 bytes after 32-byte region`、`SUMMARY:` 行的语义。**对报告要认句子，不要认数字。**

---

## 本章没演 / 没验证（请当它们不存在，别照抄）

写手册最容易骗人的地方是"顺手多写一句看起来合理的结论"。下面这些我**没有**在本机跑出来，
所以只列问题、不给答案：

* `FAIL_REGULAR_EXPRESSION`、`MISSING_REGULAR_EXPRESSION`（4.x 新加的"输出里必须不出现"）的实际判定行为。
* `WORKING_DIRECTORY` 测试属性：本章只讲了 `${PWD}` 在配置期展开，**没演**"用这个属性改测试当前目录"。
* `ctest` 的并行相关：`-j`、`PROCESSORS`、`RUN_SERIAL`、`FIXTURES_CLEANUP/SETUP`（依赖夹具）一个都没演。
* `CTest Script`（`ctest -S` / `ContinuousBuild`/`Nightly` 那一套 dashboard）、`ctest --show-only=json-v1`。
* `Coverage`（`-DSITE=...`、`ctest -T Coverage`）、`MemoryCheck`（Valgrind）。
* **没挂 `TIMEOUT` 的死循环用例**会怎样（会不会永远等、CI 自己怎么掐）—— 只演了挂 TIMEOUT 的行为。
* 多配置生成器（Xcode / Visual Studio）下的 ctest 与 preset：本机没有那些生成器（第 05 章旋钮一验证过 ninja 也不存在）。
* ASan 的其他 sanitizer 家族：`-fsanitize=thread`（TSan，需要多线程样本）、`memory`（MSan）、
  `HWAddressSanitizer`、`-fsanitize=address` 的 `detect_stack_use_after_return` 等 ASAN_OPTIONS 调参。
* `cmake --workflow`（preset 的 workflow 字段，schema 较新）本机命令行里有 `--workflow [<options>]`，
  但我没有真的跑过一条 workflow preset，**不给你结论**。
* `CMakePresets.json` 与 `CMakeUserPresets.json` **同名 preset 的覆盖顺序**：文档说法我不敢直接抄，没实测。
* `pkgConfig`、`toolchainFile`、`vendorPaths`、`installDir`、`testSpec` 这些 preset 字段：本章一个都没用。
* 第 23 节末尾那句"`-D` 写进缓存后 preset 下次能不能盖回来"：**没有回读验证**，我只证明了写进去的是命令行的值。
* 第 26 节里 `$env{}` 与 `$penv{}` 的官方差异：本机这个用例上两者屏幕一致，我没有构造出能区分它们的现场。
* 仓库根那本 56 节教材的 `ctest` 全量实跑：本章只在沙箱里复刻了三节小样本，
  **没有**去动真实仓库的构建目录（红线）。

---

## 怎么跑这章的 demo

```bash
cd cmake-handbook/06-ctest-sanitizers-presets
bash demo.sh                     # 30 节全跑，跑完自动删沙箱
bash demo.sh --keep              # 保留沙箱并打印路径，进去自己接着拨开关
bash demo.sh > /tmp/c06.txt 2>&1 # 重定向时自动关颜色，拿纯文本
```

章内文件一览：

```text
06-ctest-sanitizers-presets/
├── CMakeLists.txt            正例工程：8 个探针可执行 + 12 条用例 + 4 个 sanitizer 开关
├── CMakePresets.json         dbg / rel / asan / asan-static / only-linux / tagged + build/test 名单
├── include/calc.h  src/*.c   八支探针（ok wrong nonzero leak oob ub slow env_demo）+ 一个小静态库
├── variants/                 六个故意写坏的工程 + presets/ 里六份坏 JSON
│   ├── no-enable-testing     第 2 节
│   ├── zero-tests            第 3 节
│   ├── properties-before-add-test  第 11 节
│   ├── san-property          第 13 节
│   ├── san-compile-only      第 14 节
│   ├── san-order             第 15 节
│   └── presets/              第 26、28 节的 JSON 素材（含 penv.json）
└── demo.sh                   本章全部证据的唯一来源
```

安全边界（这几条是硬规矩，不是客套）：

* 所有 configure/build/ctest **都在 `mktemp -d` 的沙箱里**，工程是章节目录的副本；
  章节目录里连一个 `.o` 都没有（第 30 节 `find` 报 0）。
* **全程只读**引用仓库根那份 56 节的 `CMakeLists.txt`（第 29 节 grep 五行），不写它、不在它旁边构建。
* **不联网**、不装任何东西、不写任何全局配置（`~/.cmake` 之类的文件本章一个都不碰）。
* 不访问 `~/.ssh`，不执行任何 git 写操作。
* 输出里不出现本机登录名：读仓库根文件时用 `runc_at`（屏幕上只留目录名 + 相对路径），
  preset 的环境变量一律用脚本自造的 `CH06_*`。

想把这一章接回你自己的工程，顺序应该是：

1. 先跑 `ctest -N`，确认 `Total Tests: N` 里的 **N** 是你以为的那个数（第 1/3 节）。
2. 再 `grep '^C_FLAGS' .../flags.make` + `cat .../link.txt`，确认 sanitizer 真的在（第 16 节）。
3. 然后才谈 preset：把上面两条需要的 `-D` 写成 `asan` / `dbg` 两个名字（第 22 节）。
4. 最后把个人差异挪进 `CMakeUserPresets.json`（第 27 节）。

---

## 本章之后

| 章 | 讲什么 | 现在有没有 |
|---|---|---|
| 07 | 老写法 → 新写法迁移：`include_directories` / `link_directories` / `add_definitions` / `file(GLOB)` / 全局变量那一套怎么改，改完之后谁受影响 | ✅ **已交付**（`07-legacy-migration/`，30 节） |
| 08 | 逐行拆解仓库根那份 249 行的 `CMakeLists.txt`（`ds_add_leaf`、56 个调用点、`DS_WITH_ASAN` 开关） | ✅ **已交付**（`08-repo-cmake-walkthrough/`） |

本章给这两章留下的两件事都已经还上了：**`target_link_options` 那两行为什么必须成对**
（本章第 14/16 节证明过，第 08 章在真文件第 83 行那一处又把它演了一遍 —— 它在 `link.txt` 里看到的形状是
`-fsanitize=address -static-libasan` 一起进链接命令，并在正文里明说这就是本章第 16 节"两半成对"的样本）；
**`-static-libasan` 背后的部署假设**（本章第 18 节只说了它被顺手加上，第 08 章把那条注释原文读给你听，
并当场把这一行删掉重跑对比）。
第 05 章抱怨过的那个"手敲 `-D` 一换编译器就蒸发"，本章用 preset 收了账。
