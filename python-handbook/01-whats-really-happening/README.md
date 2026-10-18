# 第 01 章：按下回车之后，Python 到底做了什么

本章只回答一件事：`python3 某个文件.py` 这行命令背后，解释器**怎么启动、怎么编译、去哪儿找模块、
输出为什么有时看不见、报错为什么长成那样、退出码从哪来**。

不讲语法糖，不讲「最佳实践」，不讲框架。每一条结论都出自本机 **一次真实运行**：
CPython 3.14.4 / Ubuntu（详见仓库根 `python-handbook/README.md` 的「本机环境」），
全部命令都在 `demo.sh` 里，`bash demo.sh` 一键复现。

> **关于引用输出里的路径**：`demo.sh` 每次跑都会 `mktemp -d` 一个随机沙箱，本章这次跑的沙箱是
> `/tmp/py01-dqIL4x`。所以你复现时，所有带 `/tmp/py01-XXXXXX` 的行，后缀会换成你自己的随机名，
> 其它字符逐字不变。这一处不可复现是刻意的 —— 它换来的是「绝不往仓库里写 `__pycache__`」。

## 你会遇到的场景

| 你的困惑 | 本章哪节回答 | 对应 demo |
| --- | --- | --- |
| `python3 -c "..."` 里 `sys.argv` 怎么是 `['-c']`？我的参数跑哪去了 | 场景 1 | 第 1 节 |
| `python3 app.py` 和 `python3 -m app` 差在哪，`__name__` / `__file__` 谁给的 | 场景 1 | 第 1 节 |
| 字节码到底是什么，`__pycache__/xxx.cpython-314.pyc` 里存了什么，能拆开看吗 | 场景 2 | 第 2 节 |
| 改了源码 pyc 什么时候作废？为什么有时改了没生效 | 场景 2 | 第 2.2 / 2.3 / 2.5 小节 |
| `import` 到底按什么顺序翻哪些目录，为什么换个目录跑就 `ModuleNotFoundError` | 场景 3 | 第 3 节 |
| `python`、`python3`、`python3.14` 谁是真身，我在不在虚拟环境里 | 场景 4 | 第 4 节 |
| 程序在跑，日志里一行都没有；`| tee`、CI 里全憋着；报错却实时可见 | 场景 5 | 第 5 节 |
| REPL 里 `1 + 1` 自动出结果，脚本里不出；`_` 是什么；`>>>` 是谁打印的 | 场景 6 | 第 6 节 |
| 报错那一大堆行怎么读，`^^^^^^` 是什么意思，「Did you mean」从哪来 | 场景 7 | 第 7 节 |
| `PYTHONPATH` / `-O` / `-OO` / `-B` / `-S` / `-E` / `-P` 各留下什么可核对的证据 | 场景 8 | 第 8 节 |
| 想当场量一下 dict 和 list 的差距，不装任何第三方库 | 场景 9 | 第 9 节 |

## 心智模型

```text
        你打的命令                  启动阶段（解释器 C 代码，看不见的部分）            运行阶段
  ┌─────────────────────┐   ┌──────────────────────────────────────────┐   ┌──────────────────┐
  │ python3 -c '...'    │──┐│ 1. shell 按 PATH 找 python3 → 软链真身    │   │ __main__ 模块     │
  │ python3 dir/app.py  │  ││ 2. 决定 sys.path[0]  ← ★整章最大的分岔口★ │──▶│  .__file__       │
  │ python3 -m pkg.mod  │  ││    -c / stdin → ''（=当前目录）           │   │  .__name__       │
  │ python3 < app.py    │  ││    脚本   → 脚本所在目录的绝对路径        │   └────────┬─────────┘
  │ python3 app.py -a -b│──┘│    -m     → 当前目录的绝对路径            │            │
  └─────────────────────┘   │ 3. 读环境变量（PYTHONPATH/-E/-S/-B/-O）   │            ▼
                            │ 4. site.py 追加 dist-packages             │   compile(源码, 文件名, mode)
                            │ 5. import 时才查 __pycache__/*.pyc        │        │  mode: exec / single / eval
                            └──────────────────────────────────────────┘        ▼
                                                                        字节码 code object
                        两个「看不见但一直在起作用」的东西：                       │
                        · sys.stdout 是 tty？→ 行缓冲；是管道？→ 块缓冲（会憋住）  ▼
                        · 出错 → 沿 __traceback__ 从外到内打印栈 + 源码行 + ^^^   执行；表达式结果
                                                                                   mode=single 才 repr 打印
```

一句话记法：**「启动方式决定 `sys.path[0]` 和 compile 的 mode，`mode` 决定你有没有回显，
`isatty` 决定别人能不能实时看见你」**。本章九节就是把这三句话各证一遍。

## 场景逐个实战

> **引用约定**：下面每个 ```text 代码块都是 `bash demo.sh` 那一次运行（1238 行输出）里的**原文**，
> 逐行取自 `/tmp/py01.txt`。贴出来的都是「`$ 命令` 之后那段输出的**连续前缀**」，或者同一次运行里
> 前后相邻的几条命令 —— **不跳行、不加省略号、不把两行并成一行**；一段输出没贴完的，就是从头连续贴到
> 那儿为止，剩下几行用 `bash demo.sh > /tmp/py01.txt 2>&1` 自己看。
> 引用里的 `/tmp/py01-dqIL4x` 是那次的沙箱随机后缀，你复现时会换成自己的。
> **唯一允许的截断**：每次跑必变的计时数字（`-X importtime` 的微秒数、`timeit` 的循环数与纳秒数）
> 不进带 `$` 提示符的围栏，改成「关围栏 → 说明一句 → 再开一个不带 `$` 的围栏贴本轮真值」。
> demo 自己打的点评行（`↑` 开头那些）也不混进围栏，一律挪到围栏外面当正文。

### 场景 1：`-c` 一行式 vs 脚本文件 vs `-m`，`sys.argv` 从哪来（demo 第 1 节）

先看最小的一条：`-c` 后面跟的那串**不进 argv**，argv[0] 被硬写成 `'-c'`，你多打的参数才从 argv[1] 开始。

```text
$ python3 -c "import sys; print(sys.argv)"
['-c']
[退出码 0]

$ python3 -c "import sys; print(sys.argv)" 甲 乙
['-c', '甲', '乙']
[退出码 0]

$ python3 -c "import sys; print(sys.argv)" -- 甲 乙
['-c', '--', '甲', '乙']
```

第三条顺手推翻了「`--` 是 Python 的参数分隔符」这个流传很广的说法：它就是被当成一个普通参数收进去了。

同一个脚本用三种方式启动，`sys.argv[0]` 和 `__file__` 取的值完全不同（`-m` 给的是绝对路径）：

```text
$ python3 -m a_argv_and_main 甲 乙
====================================================================
一、argv：命令行参数是怎么传进来的
====================================================================
len(sys.argv)          3
sys.argv[0]            '/tmp/py01-dqIL4x/work/a_argv_and_main.py'
sys.argv[1]            '甲'
sys.argv[2]            '乙'
```

从标准输入喂脚本时，argv[0] 变成空字符串，`__file__` 变成 `<stdin>` —— 这时 `os.path.abspath(__file__)`
会拼出一个假路径，任何「按 `__file__` 找同目录资源」的代码在这里都会踩空：

```text
$ python3 < a_argv_and_main.py
====================================================================
一、argv：命令行参数是怎么传进来的
====================================================================
len(sys.argv)          1
sys.argv[0]            ''

====================================================================
二、这段代码是「谁」：__name__ / __file__ / 位置
====================================================================
__name__               __main__
__file__               <stdin>
os.path.abspath(__file__) /tmp/py01-dqIL4x/work/<stdin>
```

`python3` 光秃秃地起来（没有脚本、没有 `-c`、stdin 又不是终端）时也是同一条路：

```text
$ printf 'import sys\nprint(sys.argv)\n' | python3
['']
```

退出码这件事在本章先对上号（第 07 章正式讲异常与退出码的设计）：

```text
$ python3 -c "import sys; sys.exit(3)"; echo "shell 看到的 $? = $?"
shell 看到的 3 = 3
[退出码 0]

$ python3 -c "raise RuntimeError(1)" 2>/dev/null; echo "shell 看到的 $? = $?"
shell 看到的 1 = 1
```

（那条 `shell 看到的 3 = 3` 不是笔误：`echo "… $? = $?"` 里的 `$?` 被展了两次。要点是**前面那个数字
就是 shell 拿到的退出码**；未显式 `sys.exit` 时是 0，未捕获异常是 1。）

顺带看清 stdio 的状态，这是场景 5 的地基 —— 注意 **stderr 永远是行缓冲，stdout 不是**：

```text
sys.stdin   isatty=False  line_buffering=False  encoding=utf-8
sys.stdout  isatty=False  line_buffering=False  encoding=utf-8
sys.stderr  isatty=False  line_buffering=True   encoding=utf-8
```

### 场景 2：源码 → 字节码 → `__pycache__` 里的 pyc（demo 第 2 节）

`compile()` 是整条流水线的第一步，也是最容易看见的一步。`b_bytecode.py` 把 4 行源码编译成 code object：

```text
type(code)               <class 'code'>
len(code.co_code)        74   ← 字节码总共有 74 个字节
code.co_consts           (0, None)
code.co_names            ('total', 'range', 'i', 'print')
code.co_varnames         ()
```

「名字」在字节码里其实是**整数索引**，所以 `dis` 打印出来是 `LOAD_NAME 1 (range)` 这种形式：

```text
  2           LOAD_NAME                1 (range)
              PUSH_NULL
              LOAD_SMALL_INT           3
              CALL                     1
              GET_ITER
      L1:     FOR_ITER                12 (to L2)
              STORE_NAME               2 (i)
```

常量折叠是编译期干的，运行时一次加法都不发生；但**函数调用不会折叠**（解释器不敢假设 `len` 还是那个 `len`）：

```text
源码 "x = 1 + 2" → co_consts = (1, None)
它的字节码是：
  0           RESUME                   0

  1           LOAD_SMALL_INT           3
              STORE_NAME               0 (x)
              LOAD_CONST               1 (None)
              RETURN_VALUE
→ 只有 LOAD_SMALL_INT 3：加法在编译期做完了，运行时一次加法都不发生。
  3.14 还把小整数直接编进指令里（LOAD_SMALL_INT），连常量表都不用查。
源码 "z = 2 ** 10" → co_consts = (2, None, 1024)（1024 已经算好了）
源码 "y = len([1, 2, 3])" → co_names = ('len', 'y')
→ 函数调用**不会**在编译期做：解释器不敢假设 `len` 还是那个 len（你可能自己定义了一个）。
```

三种 compile 模式跑同一个表达式，输出差别就是「交互式 vs 脚本」的全部真相：

```text
exec   模式执行 `1+1` → 屏幕上：（上面这行后面什么都没有 → 结果被丢掉了。脚本文件就是这个模式！）
single 模式执行 `1+1` → 屏幕上：2
（自动把结果打印出来了 → 交互式解释器每敲一行用的就是这个模式）
eval   模式执行 `1+1` → 拿到返回值： 2
```

**pyc 到底存了什么**。先让 `import` 真的产出一个 pyc（沙箱里，仓库里不会留东西），
再手工拆头部 —— 顺带破一个误解：`marshal` 没有 `__main__`，所以 `python3 -m marshal` 是跑不起来的：

```text
$ python3 -m marshal
/usr/bin/python3: No code object available for marshal
[退出码 1]
```

（这两条之间 demo 还跑了一条 `rm -rf __pycache__ && find . -maxdepth 1 -name "*.py" | sort`，
作用是把复制进沙箱的 11 个 `.py` 列一遍、顺手清空 `__pycache__`，与本章结论无关，这里不引。）

接着让 `import_once.py` 真的 import 一次，再看目录里多出来什么：

```text
$ printf "import demo_module\n" > import_once.py && touch -d "2026-01-01T10:00:00+08:00" import_once.py && python3 import_once.py
demo_module 被执行了 value = 42 total = 3
[退出码 0]

$ ls __pycache__
demo_module.cpython-314.pyc
[退出码 0]

$ for f in __pycache__/*.pyc; do printf "%s  %s 字节\n" "$f" "$(wc -c < "$f")"; done
__pycache__/demo_module.cpython-314.pyc  504 字节
[退出码 0]
```

`c_pyc_inspect.py` 用 `struct` + `marshal` 把 504 字节拆成「16 字节头 + code object」：

```text
文件大小                504 字节
头部（前 16 字节）       2b 0e 0d 0a 00 00 00 00 20 d5 55 69 20 04 00 00
magic                   2b0e0d0a   当前解释器期望的 magic：2b0e0d0a
flags                   0 = 传统时间戳失效法：比对源码的 mtime + 字节数
头部第 9~12 字节         1767232800  → 源码 mtime 2026-01-01 10:00:00
头部第 13~16 字节        1056  → 源码字节数
正文（第 17 字节起）     488 字节 = marshal 序列化的 code object
```

头部 16 字节读成：**magic(4) + flags(4) + 字段3(4) + 字段4(4)**。`flags=0` 时后 8 字节是源码的
`mtime` 和字节数 —— 也就是说「pyc 会不会作废」这件事，答案就写在这文件头里，不需要任何工具。

那注释呢？`demo_module.py` 里写了好几行注释和一行文档字符串，反汇编出来只有 docstring：

```text
  1           LOAD_CONST               0 ('这行是文档字符串：它会被编译成常量存进 co_consts[0]，还会被绑到 __doc__ 上。')
              STORE_NAME               0 (__doc__)

 11           LOAD_SMALL_INT           2
              STORE_NAME               1 (FACTOR)
```

注释彻底消失，docstring 进 `co_consts[0]` 并绑到 `__doc__`。这也解释了 `-OO` 为什么能省体积（场景 8）。

**pyc 什么时候作废**：源码 mtime 与大小都没变 → 逐字节复用；源码变新 → 重编译并覆盖。

```text
$ cp __pycache__/demo_module.cpython-314.pyc /tmp/py01-first.pyc && python3 import_once.py && cmp -s __pycache__/demo_module.cpython-314.pyc /tmp/py01-first.pyc && echo "两次跑完 pyc 逐字节相同 → 第二次没有重新编译"
demo_module 被执行了 value = 42 total = 3
两次跑完 pyc 逐字节相同 → 第二次没有重新编译
[退出码 0]
```

接下来是 demo 的 2.3 小节：往 `demo_module.py` 末尾追加一行 `print`，再把它的 mtime 改到第二天，
然后重新 import 一次（两条围栏之间只隔着 demo 自己打的那行小节标题，没有别的命令）。
源码变新 → 解释器重编译并覆盖 pyc：

```text
$ printf 'print("改过的版本")\n' >> demo_module.py && touch -d '2026-01-02T10:00:00+08:00' demo_module.py && python3 import_once.py
demo_module 被执行了 value = 42 total = 3
改过的版本
[退出码 0]

$ for f in __pycache__/*.pyc; do printf "%s  %s 字节\n" "$f" "$(wc -c < "$f")"; done
__pycache__/demo_module.cpython-314.pyc  549 字节
```

还有一种失效法叫 hash-based pyc，`compileall` 就能造出来（注意小写 `-f`，`--force` 这个长选项**本机不存在**）：

```text
$ cd "$SB/hashdemo" && printf "VALUE = 1\n" > hmod.py && touch -d "2026-01-01T10:00:00+08:00" hmod.py && python3 -m compileall -q -f --invalidation-mode checked-hash hmod.py && ls __pycache__
hmod.cpython-314.pyc
[退出码 0]

$ cd "$SB/hashdemo" && python3 "$SB/work/c_pyc_inspect.py" __pycache__/hmod.cpython-314.pyc
====================================================================
文件：__pycache__/hmod.cpython-314.pyc
====================================================================
文件大小                124 字节
头部（前 16 字节）       2b 0e 0d 0a 03 00 00 00 74 3d 8b 7f 60 c8 9b c0
magic                   2b0e0d0a   当前解释器期望的 magic：2b0e0d0a
flags                   3 = 基于哈希 · checked：每次 import 都会现算源码哈希来比对
头部第 9~16 字节         743d8b7f60c89bc0   ← 源码哈希（8 字节，小端存放）
```

现算一遍哈希，和头部**完全一致**（算法是 `_imp.source_hash`，拿 magic 当盐，**不是 SHA-256**）：

```text
头部第 9~16 字节          743d8b7f60c89bc0
现算的源码哈希            743d8b7f60c89bc0
→ 完全一致：hash 模式下头部存的就是它（算法是 `_imp.source_hash`，拿 magic 当盐，不是 SHA-256）。
（对照）SHA-256 前 8 字节  e13df8c44af5dea1  ← 很多人以为是它，其实 pyc 头部用的不是 SHA-256
```

于是可以做一个决定性实验：**把源码 mtime 改到 2033 年**，hash-based 的 pyc 毫无反应；
**只改内容一个字节**，pyc 立刻作废并被重写（头部哈希跟着变）。

```text
$ cd "$SB/hashdemo" && touch -d "2033-12-24T21:00:00+08:00" hmod.py && python3 -c "import hmod; print(\"mtime 被改到 2033 年，import 照样成功：VALUE =\", hmod.VALUE)"
mtime 被改到 2033 年，import 照样成功：VALUE = 1
[退出码 0]

$ cd "$SB/hashdemo" && python3 "$SB/work/c_pyc_inspect.py" __pycache__/hmod.cpython-314.pyc | grep -E "flags|头部第 9~16|现算"
flags                   3 = 基于哈希 · checked：每次 import 都会现算源码哈希来比对
头部第 9~16 字节         743d8b7f60c89bc0   ← 源码哈希（8 字节，小端存放）
头部第 9~16 字节          743d8b7f60c89bc0
现算的源码哈希            743d8b7f60c89bc0
[退出码 0]

$ cd "$SB/hashdemo" && printf "VALUE = 2\n" > hmod.py && python3 -c "import hmod; print(\"内容改了 1 个字节 → VALUE =\", hmod.VALUE)" && python3 "$SB/work/c_pyc_inspect.py" __pycache__/hmod.cpython-314.pyc | grep -E "flags|头部第 9~16|现算"
内容改了 1 个字节 → VALUE = 2
flags                   3 = 基于哈希 · checked：每次 import 都会现算源码哈希来比对
头部第 9~16 字节         5b6249f8e07e8e2b   ← 源码哈希（8 字节，小端存放）
头部第 9~16 字节          5b6249f8e07e8e2b
现算的源码哈希            5b6249f8e07e8e2b
```

最后是 `unchecked-hash`（wheel 发行版爱用）：`flags=1`，解释器**连源码都不读**。
这就是「你手改 `site-packages` 里的 `.py`，Python 毫无反应」的根源。

```text
$ cd "$SB/hashdemo" && rm -rf __pycache__ && python3 -m compileall -q -f --invalidation-mode unchecked-hash hmod.py && python3 "$SB/work/c_pyc_inspect.py" __pycache__/hmod.cpython-314.pyc | grep -E "^flags|头部第 9~16"
flags                   1 = 基于哈希 · unchecked：只认头部里那 8 字节，根本不去读源码
头部第 9~16 字节         5b6249f8e07e8e2b   ← 源码哈希（8 字节，小端存放）
```

不想写代码看字节码？`python3 -m dis` 直接反汇编源文件：

```text
$ python3 -m dis demo_module.py | head -16
  0           RESUME                   0

  1           LOAD_CONST               0 ('这行是文档字符串：它会被编译成常量存进 co_consts[0]，还会被绑到 __doc__ 上。')
              STORE_NAME               0 (__doc__)

 11           LOAD_SMALL_INT           2
              STORE_NAME               1 (FACTOR)

 12           LOAD_SMALL_INT          21
              LOAD_NAME                1 (FACTOR)
              BINARY_OP                5 (*)
              STORE_NAME               2 (value)
```

### 场景 3：`sys.path` 是怎么拼出来的，当前目录什么时候算数（demo 第 3 节）

`d_path_probe.py` 把 `sys.path` 逐条打印并贴标签（顺序 = 查找顺序，前面的赢）：

```text
[0] '/tmp/py01-dqIL4x/work'
     状态=存在　说明=★ 本脚本所在的目录（用脚本文件方式启动时，它被放在第 0 项）
[1] '/usr/lib/python314.zip'
     状态=不存在　说明=标准库的 zip 打包位（本机根本没有这个文件，但这一项照样在列表里）
[2] '/usr/lib/python3.14'
     状态=存在　说明=标准库的 .py 文件都在这（os、json、dis……）
[3] '/usr/lib/python3.14/lib-dynload'
     状态=存在　说明=C 扩展（.so）专用目录，比如 _json、math 这些
[4] '/usr/local/lib/python3.14/dist-packages'
     状态=存在　说明=发行版（apt）装的第三方包目录
[5] '/usr/lib/python3/dist-packages'
     状态=存在　说明=发行版（apt）装的第三方包目录
```

**整章最大的地雷就在第 0 项**：它随启动方式变。四种方式一次跑齐（脚本目录 ≠ 当前目录时高下立判）：

```text
$ python3 -c "import sys; print(\"-c    :\", repr(sys.path[0]), repr(sys.argv[0]))"
-c    : '' '-c'
[退出码 0]

$ python3 path0.py
sys.path[0] = '/tmp/py01-dqIL4x/work'
sys.argv[0] = 'path0.py'
当前目录     = /tmp/py01-dqIL4x/work
有 __file__ 吗 = True
[退出码 0]

$ python3 -m path0
sys.path[0] = '/tmp/py01-dqIL4x/work'
sys.argv[0] = '/tmp/py01-dqIL4x/work/path0.py'
当前目录     = /tmp/py01-dqIL4x/work
有 __file__ 吗 = True
[退出码 0]

$ python3 < path0.py
sys.path[0] = ''
sys.argv[0] = ''
```

| 启动方式 | `sys.path[0]` | 含义 |
| --- | --- | --- |
| `python3 -c '...'` | `''` | 当前目录（**每次都跟着你的 shell 走**） |
| `python3 < app.py`（stdin） | `''` | 当前目录；`__file__` 是 `<stdin>`，别指望它 |
| `python3 dir/app.py` | 脚本所在目录的**绝对路径** | 与当前目录无关（这是好消息也是坏消息） |
| `python3 -m pkg.mod` | 当前目录的**绝对路径** | 模块名要能按 path 找到；换目录就找不到（下面实测） |

两条容易记反的关键结论：脚本方式下**换目录不影响找同目录模块**，而 `-m` 方式下**换目录直接影响**：

```text
$ cd "$SB" && python3 work/path0.py
sys.path[0] = '/tmp/py01-dqIL4x/work'
sys.argv[0] = 'work/path0.py'
当前目录     = /tmp/py01-dqIL4x
有 __file__ 吗 = True
[退出码 0]
# 但 -c 换到上级目录，找模块的地方就跟着变了：

$ cd "$SB" && python3 -c "import sys; print(\"-c 在上级目录:\", repr(sys.path[0]))"
-c 在上级目录: ''
[退出码 0]

$ cd "$SB" && python3 -m path0
/usr/bin/python3: No module named path0
[退出码 1]
```

`PYTHONPATH` 插在标准库**前面**（第 0 项之后），所以它能顶掉标准库 —— 这是它的用途，也是它的危险：

```text
$ PYTHONPATH=/tmp:/var python3 -c "import sys; print(sys.path)"
['', '/tmp', '/var', '/usr/lib/python314.zip', '/usr/lib/python3.14', '/usr/lib/python3.14/lib-dynload', '/usr/local/lib/python3.14/dist-packages', '/usr/lib/python3/dist-packages']
[退出码 0]

$ PYTHONPATH=/usr/lib/python3.14:/tmp python3 -c "import os, sys; print(os.__file__)"
/usr/lib/python3.14/os.py
```

**真实踩坑现场**：`e_import_trap/` 里有 `main.py`、`helper.py`，还有一个**故意抢标准库名字**的 `random.py`。
在目录里跑，一切正常；从外面跑，`sys.path[0]` 依然是脚本目录，所以照样找得到 `helper` ——
真正把新手打死的是后面那半句：`random` 被同名文件抢走了。

```text
$ python3 e_import_trap/main.py
--------------------------------------------------------------------
cwd（当前目录）    = /tmp/py01-dqIL4x/work
脚本所在目录        = /tmp/py01-dqIL4x/work/e_import_trap
sys.path[0]         = '/tmp/py01-dqIL4x/work/e_import_trap'
--------------------------------------------------------------------
import helper 成功： 找到 helper.py 了
  helper.py 实际来自： /tmp/py01-dqIL4x/work/e_import_trap/helper.py
import random 完成：
  random 实际来自： /tmp/py01-dqIL4x/work/e_import_trap/random.py
  标准库目录是： /usr/lib/python3.14 ← 真正的 random.py 本该在这里，但你拿到的是上面那个
  AttributeError: module 'random' has no attribute 'randint' (consider renaming '/tmp/py01-dqIL4x/work/e_import_trap/random.py' since it has the same name as the standard library module named 'random' and prevents importing that standard library module)
[退出码 0]
```

那条 `AttributeError` 里没有任何代码写错 —— 只是 `random` 这个名字被当前目录里那个 `random.py` 抢走了。

反过来，`-c` 在沙箱根目录找 `helper` 就找不到（它在 `e_import_trap/` 里，但没人去那儿看）：

```text
$ cd "$SB" && python3 -c "import helper"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    import helper
ModuleNotFoundError: No module named 'helper'
[退出码 1]
```

### 场景 4：`which python3` / `sys.executable` / `sys.prefix`（demo 第 4 节）

```text
$ command -v python python3 python3.14 py
/usr/bin/python3
/usr/bin/python3.14
[退出码 0]

$ ls -l /usr/bin/python3 /usr/bin/python3.14
lrwxrwxrwx 1 root root      10 Mar 21  2026 /usr/bin/python3 -> python3.14
-rwxr-xr-x 1 root root 7477160 Aug 20 18:41 /usr/bin/python3.14
[退出码 0]

$ python3 -c "import sys, os; print(sys.executable); print(os.path.realpath(sys.executable))"
/usr/bin/python3
/usr/bin/python3.14
[退出码 0]

$ python3 -c "import sys; print(sys.prefix, sys.base_prefix, sys.exec_prefix)"
/usr /usr /usr
```

本机事实：`python` **不存在**（第 0 节里 `python --version` 是 `command not found`，退出码 127），
`python3` 是软链，`sys.executable` 报的是「你叫进来的那个名字」而不是真身。
**判断「我在不在虚拟环境里」只用一个条件：`sys.prefix == sys.base_prefix`。**

`f_where_python.py` 把身份、ABI、C 扩展目录、编码默认值一次打印齐：

```text
$ python3 f_where_python.py
====================================================================
一、解释器自己的身份
====================================================================
sys.executable       /usr/bin/python3
sys.version          3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]
sys.implementation   cpython 3.14.4
sys.api_version      1013  (ABI 版本号，决定 .so 能不能混用)
platform             linux
maxsize / 位宽        9223372036854775807 → 64 位

====================================================================
二、两个 prefix：判断「我在不在虚拟环境里」的铁证
====================================================================
sys.prefix           /usr
sys.base_prefix      /usr
→ 相同 ⇒ 当前不在虚拟环境里（本机就是这种情况）

====================================================================
三、安装目录（这些就是 sys.path 里那几个标准项的来源）
====================================================================
stdlib       /usr/lib/python3.14
platstdlib   /usr/lib/python3.14
purelib      /usr/local/lib/python3.14/dist-packages
platlib      /usr/local/lib/python3.14/dist-packages
include      /usr/include/python3.14

====================================================================
四、C 扩展长什么样（lib-dynload 里是真的 .so 文件）
====================================================================
/usr/lib/python3.14/lib-dynload
共 49 个文件，前 3 个：
  _asyncio.cpython-314-x86_64-linux-gnu.so
  _bz2.cpython-314-x86_64-linux-gnu.so
  _codecs_cn.cpython-314-x86_64-linux-gnu.so
→ 文件名里带 cpython-314-x86_64-linux-gnu：解释器版本 + ABI + 平台，三者都要对上。

====================================================================
五、编码默认值（03/08 章的地基）
====================================================================
sys.getdefaultencoding()  utf-8
sys.getfilesystemencoding() utf-8 (errors=surrogateescape)
stdout.encoding           utf-8  ← 受 locale 影响！
→ 源码文件的编码：不写 `# -*- coding:` 时按 UTF-8；但 print 出去用什么编码是上面这个值。
[退出码 0]
```

顺带把「子进程该用哪个解释器」这个坑说清楚：写 `subprocess.run(["python3", …])` 会在 PATH 里重新找一遍，
可能找到**另一个** Python；正确做法是 `sys.executable`。

**4.1 本机想建 venv 的真实下场**（09 章的地基，这里先立此存照 —— 全程在 `/tmp` 里，没装任何东西）：

```text
$ mkdir -p /tmp/py01-venv-probe && python3 -m venv /tmp/py01-venv-probe/v
The virtual environment was not created successfully because ensurepip is not
available.  On Debian/Ubuntu systems, you need to install the python3-venv
package using the following command.

    apt install python3.14-venv

You may need to use sudo with that command.  After installing the python3-venv
package, recreate your virtual environment.

Failing command: /tmp/py01-venv-probe/v/bin/python3

[退出码 1]

$ python3 -m venv --without-pip /tmp/py01-venv-probe/v2 && ls /tmp/py01-venv-probe/v2/bin
Activate.ps1
activate
activate.csh
activate.fish
python
python3
python3.14
𝜋thon
[退出码 0]

$ /tmp/py01-venv-probe/v2/bin/python3 -c "import sys; print(sys.prefix); print(sys.base_prefix); print(len(sys.path))"
/tmp/py01-venv-probe/v2
/usr
5
```

`--without-pip` 建得出来，进去之后 `sys.prefix` 与 `sys.base_prefix` **不再相同** —— 这就是那个判据的反例。
本机 `python3 -m pip` 也是 `No module named pip`（未安装，本章不装），所以第 09 章只能按这个真实情况讲。

### 场景 5：stdout 缓冲 —— 管道和终端不是一种东西（demo 第 5 节）

`g_buffer_probe.py` 用 `pty.openpty()` 造一个伪终端，再开一条管道，**同一个子进程**分别接到两者上，
0.4 秒时偷看「已经出来几行」：

```text
伪终端 + 默认           isatty=True  line_buffering=True  write_through=False  底层=BufferedWriter
管道 + 默认            isatty=False  line_buffering=False  write_through=False  底层=BufferedWriter
管道 + -u            isatty=False  line_buffering=False  write_through=True  底层=FileIO
→ 注意 `-u` 那行：line_buffering 仍然是 False，变的是 write_through。
  所以 -u 不是「改成行缓冲」，而是「绕过用户态缓冲区，直接写进 fd」。

==========================================================================
二、计时实验：0.4 秒时子进程已经吐出几行了？（子进程共 2 行）
==========================================================================
伪终端(pty) + 默认              0.4 秒时 1 行   结束时 2 行   → 第 1 行当场就发出来
管道(pipe) + 默认              0.4 秒时 0 行   结束时 2 行   → 两行全被憋到进程结束
管道 + -u                    0.4 秒时 1 行   结束时 2 行   → 第 1 行当场就发出来
管道 + PYTHONUNBUFFERED=1    0.4 秒时 1 行   结束时 2 行   → 第 1 行当场就发出来
管道 + print(flush=True)     0.4 秒时 1 行   结束时 2 行   → 第 1 行当场就发出来
管道 + 只往 stderr 打印          0.4 秒时 1 行   结束时 2 行   → 第 1 行当场就发出来
```

这就是你在 CI/`| tee`/日志文件里「程序明明在跑却一行都没有」的全部原因，也是「报错能立刻看见、
进度却看不见」的原因（stderr 天生行缓冲）。修法按侵入性排序：

```text
  * 修法按侵入性排序：print(..., flush=True)
    < 环境变量 PYTHONUNBUFFERED=1 < 启动参数 python3 -u
    < 代码里 sys.stdout.reconfigure(line_buffering=True)
```

字节实验推翻了「写满一行就 flush」这个流行误解 —— **管道里换行不触发 flush**：

```text
写 100 字节，不 flush 就睡          0.4 秒时     0 字节   结束时   100 字节
写 100 字节，先 flush 再睡          0.4 秒时   100 字节   结束时   100 字节
一次写 9000 字节（无换行）             0.4 秒时     0 字节   结束时  9000 字节
一次写 9000 字节（4500 个换行）        0.4 秒时     0 字节   结束时  9000 字节
```

**本机未测准的一条**（如实记下，不编）：

```text
  * 「写到多大才会自己冲出去」这个临界点本机没测准：试到 200000 字节时，
    父进程被自己没及时读走的管道数据卡死了（子进程写不进 64KB 的管道 → 阻塞，
    父进程却在等子进程退出 → 互相等）。这本身就是重要一课：
    **只要读端不持续读，管道就会反压把写端冻住**。所以本章的实验都控制在几 KB。
```

5.1 节是 shell 侧的同一件事，可以直接抄进你的终端：

```text
$ python3 -c "import sys; print(sys.stdout.isatty(), sys.stdout.line_buffering)" | cat
False False
[退出码 0]

$ python3 -u -c "import sys; print(sys.stdout.isatty(), sys.stdout.line_buffering, sys.stdout.write_through)" | cat
False False True
[退出码 0]

$ python3 -c "import sys; sys.stdout.reconfigure(line_buffering=True); print(sys.stdout.line_buffering)" | cat
True
```

（第一条要在终端里跑才会看到 `True True`；demo 里 stdout 被接进管道，所以是 `False False` ——
这个对比本身就是本节的主角，别把它当噪音。）

### 场景 6：交互式解释器 vs 脚本（`>>>`、`_`、回显是谁打的）（demo 第 6 节）

`h_repl_probe.py` 用伪终端喂给 `python3` 八行，抓回「真实屏幕」。
（3.13+ 默认是带语法高亮的 PyREPL，逐字符重画会让 transcript 不可读，所以用官方开关
`PYTHON_BASIC_REPL=1` 抓经典 REPL；PyREPL 的重画原文也在 demo 第 6 节里，作为反面证据保留。）

```text
Python 3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>> 1 + 1
2
>>> x = 10
>>> x * 3
30
>>> _
30
>>> _ + 1
31
>>> 'abc'.upper()
'ABC'
>>> print('我加了 print')
我加了 print
>>> exit()
```

四个要点：

```text
读这份输出的四个要点：
  * 前两行是 banner，里面有版本、编译日期、编译器 —— 报 bug 时要连着它一起贴。
  * `>>>` 是 Python 打的提示符；`1 + 1` 是你打的、由**终端**回显；`2` 才是 Python 打的。
  * `x = 10` 那行后面什么都不打印：赋值表达式没有值可显示。
  * `_` 单独一行 → 打印 30（上一次的结果）；`_ + 1` → 31。这个变量只在 REPL 里存在。
```

`_` 是 REPL 专用变量（`builtins._`），脚本里没有；而且它会被下一次成功求值覆盖。

**证明回显是终端干的**：把 stdin 从伪终端换成管道，同样喂 `1 + 1`，屏幕上你打的命令消失了：

```text
Python 3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>> 2
>>> → 看 `>>> 2` 那一行：`1 + 1` 消失了！它从来没在 Python 的输出流里出现过。
  所以「终端里能看到自己敲的命令」这件事是终端（或行编辑器）干的，
  把 python3 的输出重定向到文件时，文件里只会有结果，不会有命令 —— 别以为日志丢了。
```

脚本模式（`exec`）里同样的三行，只剩 `print` 那一条 —— 和场景 2 的 mode 实验严丝合缝对上：

```text
把 `1 + 1` / `'abc'.upper()` / `print(...)` 三行喂给「非交互的 python3」，输出是：
'只有这行有输出\n'
→ 只剩 print 那一行。前两行的表达式结果被直接丢弃（compile mode = exec）。
```

REPL 独有的还有 `sys.ps1` / `sys.ps2` / `exit`，它们都是 `site` 模块给的：

```text
本进程（脚本模式）里 sys.ps1 存在吗：False
在伪终端 REPL 里现问 sys.ps1 / sys.ps2，抓到的是：
Python 3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>> import sys
>>> sys.ps1
'>>> '
>>> sys.ps2
'... '
>>> exit()
```

```text
→ exit / quit 同样是 site 注入 builtins 的糖。本机实测：
    脚本里 `exit()` 能用（等价 sys.exit()），并不像有些老教程说的会报 RuntimeError；
    但 `python3 -S -c "exit()"`（-S = 跳过 site）会报
    NameError: name 'exit' is not defined —— 这就是「它是 site 给的，不是语言的一部分」的铁证。
    Ctrl-D 才是通吃的退出方式（对 stdin 而言就是 EOF）。
```

（脚本里那句「想自动显示表达式结果就加 `-i`」是脚本给的提示，**本机未验证**：
本章 demo 没跑 `python3 -i`，想核对就自己试一次。）

### 场景 7：报错原文逐行拆（demo 第 7 节）

新手最该练的不是写代码，是**读 traceback**。3.14 的报错质量比想象中高得多，先看 NameError：

```text
Traceback (most recent call last):
  File "/tmp/py01-dqIL4x/work/i_errors/name_error.py", line 37, in <module>
    print(greting)
          ^^^^^^^
NameError: name 'greting' is not defined. Did you mean: 'greeting'?
[退出码 1]
```

四行各有职责：第 1 行说「下面开始是栈，从外到内」；`File/line/in` 那行是**一个栈帧**；
再下一行是源码原文；`^^^^^^^` 精确指出表达式里出事的那一段；最后一行是异常类型 + 消息，
`Did you mean` 是解释器现场比对名字后给的建议。同一个异常被接住时，对象身上还有更多料：

```text
type(exc)            NameError
str(exc)             name 'greting' is not defined
exc.name             greting
exc.args             ("name 'greting' is not defined",)
```

`ModuleNotFoundError` 有三种长得很不一样的失败（`i_errors/module_not_found.py` 用参数选第几种）：

```text
$ python3 i_errors/module_not_found.py 1
第 1 种失败，现在开始 import：
ModuleNotFoundError: No module named 'requests_here_nope'
  exc.name = 'requests_here_nope'   exc.path = None
  → 这种才是「真没这个包」。要么装它（本手册不装），要么改代码。
[退出码 0]

$ python3 i_errors/module_not_found.py 2
第 2 种失败，现在开始 import：
ModuleNotFoundError: No module named 'helper'
  exc.name = 'helper'   exc.path = None
  本次搜索翻过 6 个目录，第 0 个是本脚本所在目录，里面没有 helper.py
  → 文件明明在磁盘上（在上一级的 e_import_trap/ 里），但没人去那儿找它。
```

第 3 种最容易误诊：**模块找到了，是它的顶层代码自己炸了**，于是 traceback 有两层，真凶在第二层：

```text
$ python3 i_errors/module_not_found.py 3
第 3 种失败，现在开始 import：
broken_import 的顶层代码开始执行（你能看到这行，说明它被读进来了）
Traceback (most recent call last):
  File "/tmp/py01-dqIL4x/work/i_errors/module_not_found.py", line 26, in <module>
    import broken_import                       # noqa: F401
    ^^^^^^^^^^^^^^^^^^^^
  File "/tmp/py01-dqIL4x/work/i_errors/broken_import.py", line 14, in <module>
    BAD = 1 / 0                    # ← 真凶在这一行
          ~~^~~
ZeroDivisionError: division by zero
[退出码 1]
```

看到 `No module named X` 之前先确认：是不是 X 找到了、但 X 的顶层代码挂了。
`~~~^^^` 是 3.13+ 的「操作数级」标记：`~~` 标左操作数，`^` 标出事那一侧。

缩进类错误有三兄弟，原文都短，但措辞信息量很大：

```text
$ python3 i_errors/indent_error.py
  File "/tmp/py01-dqIL4x/work/i_errors/indent_error.py", line 16
    print(f"你好, {name}")        # ← 这一行必须缩进，现在没缩进
    ^^^^^
IndentationError: expected an indented block after function definition on line 15
[退出码 1]

$ python3 i_errors/tab_error.py
  File "/tmp/py01-dqIL4x/work/i_errors/tab_error.py", line 20
    return "小"          # ← 这一行用 8 个空格；上面的 if 用 1 个制表符，列正好撞上
TabError: inconsistent use of tabs and spaces in indentation
[退出码 1]

$ python3 i_errors/syntax_error.py
  File "/tmp/py01-dqIL4x/work/i_errors/syntax_error.py", line 16
    for it in items
                   ^
SyntaxError: expected ':'
[退出码 1]
```

`IndentationError`/`TabError` 都是 `SyntaxError` 的子类，而且这类错误**编译期就挂**，
一行代码都不会执行 —— 所以「报错行号在文件末尾，我却没看到任何 print」是正常的。
`cat -A` 把制表符显形，确认这不是注释里说说而已：

```text
$ cat -A i_errors/tab_error.py | sed -n "17,21p"
def roll(dice):$
^Iif dice > 3:$
^I^Ireturn "M-eM-$M-'"$
        return "M-eM-0M-^O"          # M-bM-^FM-^P M-hM-?M-^YM-dM-8M-^@M-hM-!M-^LM-gM-^TM-( 8 M-dM-8M-*M-gM-)M-:M-fM- M-<M-oM-<M-^[M-dM-8M-^JM-iM-^]M-"M-gM-^ZM-^D if M-gM-^TM-( 1 M-dM-8M-*M-eM-^HM-6M-hM-!M-(M-gM-,M-&M-oM-<M-^LM-eM-^HM-^WM-fM--M-#M-eM-%M-=M-fM-^RM-^^M-dM-8M-^J$
$
[退出码 0]
```

`^I` 就是制表符：第 18、19 行各有一个和两个 `^I`，第 20 行开头是 8 个普通空格 —— 这个文件的缩进真的
混用了 tab 和空格，不是注释里说说而已。另外别被吓到：`cat -A` 会把非 ASCII 字节按字节转义成
`M-eM-0M-^O`（那是「小」字的 UTF-8 三字节）、`M-bM-^FM-^P`（那是注释里那个 `←`），行尾的 `$` 是它画的换行符，
文件本身没问题。

深层调用栈：四层函数，从外到内打印，**最后一条帧才是出事地点**，读 traceback 要倒着读：

```text
异常类型：KeyError    参数：'timeout'
栈帧一共 4 层（顺序 = 从外到内，最后一层是出事地点）：
  [0] <module>     行 34  main()
  [1] main         行 27  return start()
  [2] start        行 23  return read_config("app.ini")
  [3] read_config  行 19  return data["timeout"]           # ← 真凶：键不存在 → KeyError
```

```text
Traceback (most recent call last):
  File "/tmp/py01-dqIL4x/work/i_errors/deep_traceback.py", line 50, in <module>
    main()
    ~~~~^^
  File "/tmp/py01-dqIL4x/work/i_errors/deep_traceback.py", line 27, in main
    return start()
  File "/tmp/py01-dqIL4x/work/i_errors/deep_traceback.py", line 23, in start
    return read_config("app.ini")
  File "/tmp/py01-dqIL4x/work/i_errors/deep_traceback.py", line 19, in read_config
    return data["timeout"]           # ← 真凶：键不存在 → KeyError
           ~~~~^^^^^^^^^^^
KeyError: 'timeout'
[退出码 1]
```

### 场景 8：`PYTHONPATH` / `-O` / `-OO` / `-B` 各留下什么证据（demo 第 8 节）

默认级别，`import` 会编译并写 pyc —— 注意 `__pycache__` 里**只有被 import 的模块，没有主脚本**：

```text
$ rm -rf __pycache__ && python3 opt_run.py && ls __pycache__
sys.flags.optimize        0
sys.dont_write_bytecode   False
__debug__                 True   （模块里读到的是 True）
opt_target.__doc__ 前 12 字：'opt_target.p'
double_checked(-1) 触发 AssertionError: 必须是正数
opt_target.cpython-314.pyc
[退出码 0]
```

看清楚最后那行 `ls __pycache__`：目录里只有 `opt_target`，**没有 `opt_run.py`** —— 被当 `__main__`
直接跑的那个脚本永远不写 pyc（写了也没人按这个文件名去找它）。

`-B`（等价 `PYTHONDONTWRITEBYTECODE=1`）之后目录根本不出现：

```text
$ rm -rf __pycache__ && python3 -B opt_run.py >/dev/null && (ls __pycache__ 2>/dev/null || echo "(__pycache__ 根本没被创建)")
(__pycache__ 根本没被创建)
[退出码 0]

$ PYTHONDONTWRITEBYTECODE=1 python3 -c "import sys; print(sys.dont_write_bytecode)"
True
```

`-O` 丢 `assert`，`-OO` 连 docstring 一起丢，而且 pyc 文件名带上级别后缀：

```text
$ rm -rf __pycache__ && python3 -O opt_run.py && ls __pycache__
sys.flags.optimize        1
sys.dont_write_bytecode   False
__debug__                 False   （模块里读到的是 False）
opt_target.__doc__ 前 12 字：'opt_target.p'
double_checked(-1) 没有报错，返回值 = -2   ← assert 已经被编译期丢掉了
opt_target.cpython-314.opt-1.pyc
[退出码 0]

$ rm -rf __pycache__ && python3 -OO opt_run.py && ls __pycache__
sys.flags.optimize        2
sys.dont_write_bytecode   False
__debug__                 False   （模块里读到的是 False）
opt_target.__doc__ 前 12 字：'None'
double_checked(-1) 没有报错，返回值 = -2   ← assert 已经被编译期丢掉了
opt_target.cpython-314.opt-2.pyc
```

所以「生产环境统一 `-O`」是个危险动作：**别用 assert 做参数校验/业务规则**（第 07 章正式讲）。

```text
$ python3 -O -c "assert False, \"这条在 -O 下不执行\"; print(\"跑到这里说明 assert 真的没生成字节码\")"
跑到这里说明 assert 真的没生成字节码
[退出码 0]

$ python3 -c "assert False, \"默认级别下这条会炸\""
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    assert False, "默认级别下这条会炸"
           ^^^^^
AssertionError: 默认级别下这条会炸
[退出码 1]
```

不同优化级别的 pyc 互不覆盖（两个文件各归各用）：

```text
$ rm -rf __pycache__ && python3 -O opt_run.py >/dev/null && ls __pycache__ && python3 opt_run.py >/dev/null && ls __pycache__
opt_target.cpython-314.opt-1.pyc
opt_target.cpython-314.opt-1.pyc
opt_target.cpython-314.pyc
[退出码 0]
```

（第一条 `ls` 是 `-O` 之后的目录，第二次 `ls` 多了一行：默认级别那份。）

8.5 把其它开关的「可见痕迹」一次跑齐：

```text
$ python3 -S -c "import sys; print(len(sys.path), sys.path)"
4 ['', '/usr/lib/python314.zip', '/usr/lib/python3.14', '/usr/lib/python3.14/lib-dynload']
[退出码 0]

$ python3 -S -c "exit()"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    exit()
    ^^^^
NameError: name 'exit' is not defined
[退出码 1]

$ python3 -E -c "import sys; print(sys.flags.ignore_environment)"
1
[退出码 0]

$ PYTHONPATH=/tmp python3 -E -c "import sys; print(\"-E 让 PYTHONPATH 被无视:\", sys.path)"
-E 让 PYTHONPATH 被无视: ['', '/usr/lib/python314.zip', '/usr/lib/python3.14', '/usr/lib/python3.14/lib-dynload', '/usr/local/lib/python3.14/dist-packages', '/usr/lib/python3/dist-packages']
[退出码 0]

$ python3 -P -c "import sys; print(sys.path[0], sys.flags.safe_path)"
/usr/lib/python314.zip True
[退出码 0]

$ python3 -W error -c "import warnings; warnings.warn(\"只是一个警告\"); print(\"这行不会打印\")"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    import warnings; warnings.warn("只是一个警告"); print("这行不会打印")
                     ~~~~~~~~~~~~~^^^^^^^^^^^^^^^^
UserWarning: 只是一个警告
[退出码 1]

$ python3 -X importtime -c "import json" 2>&1 | tail -4
[退出码 0]
```

`tail -4` 打出来的那四行**每跑必变**（左边一列是这次 import 自身的花费、右边是含依赖的累计花费，单位微秒），
**本章不拿它当证据**，所以按引用约定把它们移出带 `$` 的围栏；下面是不带提示符的一栏，贴的就是本轮抓到的原样数字：

```text
import time:       330 |        374 |     json.scanner
import time:       349 |       4789 |   json.decoder
import time:       310 |        310 |   json.encoder
import time:       166 |       5264 | json
```

读法：`-S` 跳过 `site`（所以两个 dist-packages 没了、`exit` 没了）；`-E` 无视环境变量
（`PYTHONPATH=/tmp` 被吃掉）；`-P` 把第 0 项从当前目录/脚本目录换成安装目录（`safe_path=True`）；
`-W error` 把警告升级成异常；`-X importtime` 上面那栏看的是**谁挂在谁下面**那棵树 ——
`json` 的累计值最大、`json.decoder` 次之，因为它把 `json.scanner` 也算在自己头上，缩进一级就是依赖链深一级。
最后一行 `py_compile` 是「只编译不运行」：照样写 pyc，但一行 `print` 都没执行。

### 场景 9：`python3 -m timeit` 真实跑一组小数据（demo 第 9 节）

只用标准库，不装任何东西。demo 每段都额外打一行 `[形状]`（把 timeit 输出里的所有数字换成 `N`），
形状行是稳定可复现的，下面引用的就是它。

```text
$ python3 -m timeit -s "d = {i: i for i in range(1000)}" "500 in d"
[退出码 0]
[形状] N loops, best of N: N nsec per loop

$ python3 -m timeit -s "s = {i for i in range(1000)}" "500 in s"
[退出码 0]
[形状] N loops, best of N: N nsec per loop

$ python3 -m timeit -s "l = list(range(1000))" "500 in l"
[退出码 0]
[形状] N loops, best of N: N usec per loop
```

光看形状行就够了：`500 in d` / `500 in s` 落在 `nsec` 级，`500 in l` 落在 `usec` 级 ——
dict / set 的存在性检查走哈希，list 要逐个比，数据一大就是数量级差别（04 章展开）。

```text
$ python3 -m timeit -n 100000 -r 3 -s "l = []" "l.append(1)"
[退出码 0]
[形状] N loops, best of N: N nsec per loop
```

`-n` 是每轮跑几次，`-r` 是跑几轮取最好值，`-s` 是只执行一次的准备工作（setup），不写就自动选次数。
最后一个例子说明 setup 的重要性 —— `l = []` 放进 `-s` 而不是语句里，否则测的是「建列表」而不是「append」：

```text
$ python3 -m timeit -s "
def build():
    a = []
    for i in range(1000):
        a.append(i)
    return a
" "build()"
[退出码 0]
[形状] N loops, best of N: N usec per loop
```

上面 5 段 timeit 每次跑的那行 `N loops, best of N: N unit per loop` 数字都属于「跑一次就变一次」的东西，
**本章不拿它当证据**；下面这个不带 `$` 的围栏贴的是本轮真值，只用于「这 5 条命令确实跑过了、量级没变」这个
佐证，数字本身换台机器就不同：

```text
20000000 loops, best of 5: 17.3 nsec per loop
20000000 loops, best of 5: 15.5 nsec per loop
200000 loops, best of 5: 1.24 usec per loop
100000 loops, best of 3: 11.7 nsec per loop
20000 loops, best of 5: 13.3 usec per loop
```

## 命令速查表

| 想干什么 | 命令（本机都真跑过） | 关键输出 |
| --- | --- | --- |
| 看 argv | `python3 -c "import sys; print(sys.argv)" 甲` | `['-c', '甲']` |
| 看 argv + 身份 + stdio | `python3 a_argv_and_main.py 甲 乙` | `sys.argv[0]` = 你打的那个写法 |
| 反汇编一段源码 | `python3 b_bytecode.py` | `LOAD_NAME 1 (range)` |
| 反汇编一个文件 | `python3 -m dis demo_module.py` | 助记符全文 |
| 拆 pyc 头部 | `python3 c_pyc_inspect.py __pycache__/x.cpython-314.pyc` | magic / flags / mtime+size 或哈希 |
| 造 hash-based pyc | `python3 -m compileall -q -f --invalidation-mode checked-hash x.py` | `flags 3`（`-f` 不能写成 `--force`） |
| 给 sys.path 贴标签 | `python3 d_path_probe.py` | `[0] … 状态=存在` |
| 比较四种启动方式的 path[0] | `python3 -m path0` / `python3 path0.py` / `python3 < path0.py` | 见场景 3 的表 |
| 找解释器真身 | `python3 -c "import sys; print(sys.executable, sys.prefix, sys.base_prefix)"` | 相等 = 不在 venv |
| 试建 venv（不装东西） | `python3 -m venv --without-pip 目录` | 能建，但没有 pip |
| 看缓冲模式 | `python3 g_buffer_probe.py`；`… \| cat` 对照 | `管道 + 默认 → 0.4 秒时 0 行` |
| 抓 REPL 真实屏幕 | `python3 h_repl_probe.py`（内部用 pty + `PYTHON_BASIC_REPL=1`） | `>>> _` → `30` |
| 只看报错类型 | `python3 i_errors/indent_error.py` 等 6 个文件 | 见场景 7 |
| 优化级别证据 | `python3 -O opt_run.py` / `-OO` / `-B` | `opt-1` / `opt-2` / 目录不出现 |
| 跳过 site | `python3 -S -c "import sys; print(sys.path)"` | 少 2 项 |
| 无视环境变量 | `PYTHONPATH=/tmp python3 -E -c …` | path 里没有 `/tmp` |
| 安全路径 | `python3 -P -c "import sys; print(sys.path[0])"` | 变成安装目录 |
| 警告当错误 | `python3 -W error -c "import warnings; warnings.warn('x')"` | 退出码 1 |
| 看 import 花了多久 | `python3 -X importtime -c "import json" 2>&1 \| tail` | 微秒数会抖，看树形 |
| 只编译不运行 | `python3 -m py_compile demo_module.py` | 写 pyc，无 print |
| 量一小段代码 | `python3 -m timeit -s "…" "…" -n 100000 -r 3` | `N loops, best of N` |

## 这章的坑

1. **`sys.path[0]` 随启动方式变**。`-c`/stdin 是 `''`（当前目录），脚本是**脚本目录绝对路径**，`-m` 是**当前目录绝对路径**。
   于是 `python3 -m` 在别的目录里就 `No module named`（本机实测退出码 1）。
2. **当前目录里有和标准库同名的文件**（`random.py`、`json.py`、`email.py`…）会静默顶掉标准库。
   3.14 会直接劝你改名：`consider renaming '…/random.py'`。别信「import 成功了就说明没问题」。
3. **`PYTHONPATH` 排在标准库前面**，能顶掉标准库；`-E` 才是「别读环境变量」。
4. **管道里不是行缓冲**。`isatty()` 为假时换行不 flush，写 9000 字节带 4500 个换行照样 0 字节可见。
   要实时：`print(..., flush=True)` < `PYTHONUNBUFFERED=1` < `python3 -u` < `reconfigure(line_buffering=True)`。
   **`-u` 改的是 `write_through`，`line_buffering` 仍是 `False`**（很多人以为 `-u` 就是行缓冲）。
5. **stderr 恒行缓冲**：报错能立刻看到不代表 stdout 也能；日志里「只有报错没有进度」是正常现象。
6. **别拿 `sys.argv[0]` 当路径用**。它可能是 `'work/path0.py'`（相对）、`'-c'`、`''`。要路径就用 `__file__`，
   还要记得 stdin 启动时 `__file__` 是 `<stdin>`。
7. **主脚本永远不写 pyc**（第 8 节实测 `__pycache__` 里只有被 import 的那个）。
8. **hash-based pyc 不看时间戳**。改了 `.py` 但没改内容 → pyc 不重编；
   `unchecked-hash` 模式下解释器**连源码都不读**，手改 `site-packages` 里的源码没反应。
9. **`assert` 在 `-O`/`-OO` 下整条消失**，`-OO` 连 `__doc__` 都没了（实测变成 `None`）。
   用 assert 做校验的代码在生产 `-O` 下会静默失效。
10. **`compileall` 没有 `--force` 这个长选项**（本机会报 `unrecognized arguments: --force`，退出码 2），只有 `-f`。
11. **PyREPL 的重画不适合当证据**。要用伪终端抓 REPL 输出时设 `PYTHON_BASIC_REPL=1`。
12. **`python3 -m marshal` 跑不起来**（`No code object available for marshal`，退出码 1）——
    pyc 只能按 PEP 552 手工解析，本章就是这么做的。
13. **本机 `python3 -m venv` 建不出带 pip 的环境**（缺 `ensurepip`，退出码 1），`python3 -m pip` 也是
    `No module named pip`。本章一律不安装任何东西。

**本机未验证 / 未测准清单**（不含推测结论）：

- 管道「写多少字节才会自动冲出去」的临界点：200000 字节的实验把父进程自己卡死了（反压死锁），没测准。
- `python3 -i`（脚本跑完再进交互）：脚本里给了提示，但没跑进 demo。
- 第 09/10 章要用到的 `venv + pip`、`mypy`：本机没有，届时按真实情况写或标注「本机未验证」。

## 自检问题

1. `python3 dir/app.py` 在 `/tmp` 里跑，`app.py` 里 `import helper`（`helper.py` 和它同目录）能成功吗？
   换成 `python3 -m dir.app` 呢？（提示：`sys.path[0]` 两个值分别是什么。）
2. 你在 CI 里 `python3 long.py | tee log.txt`，10 分钟看不到一行输出，但 `log.txt` 里最后是完整的。
   最可能的原因是什么？给出**三种**修法并按侵入性排序。
3. `python3 -u` 之后 `sys.stdout.line_buffering` 是 `True` 还是 `False`？为什么？
4. 一个 pyc 头部 16 字节里，`flags=0` 和 `flags=3` 分别拿什么判断「要不要重新编译」？
   把源码 `touch` 到未来，哪种模式会重编译？
5. `NameError: name 'greting' is not defined. Did you mean: 'greeting'?` —— 这行里的
   `Did you mean` 是语法分析器给的还是异常处理给的？这对「同样的名字错误发生在 `-O` 下」有影响吗？
6. `ModuleNotFoundError: No module named 'foo'` 有哪三种失败？其中哪一种其实模块被找到了？
   怎么从 traceback 的行数看出来？
7. REPL 里 `x = 10` 什么都不打印，`1 + 1` 打印 `10` 之后的结果 —— 同一个 REPL 两种命运，原因是什么？
   `_` 在脚本里能用吗？
8. `python3 -OO` 之后 `assert` 和 `__doc__` 分别怎么了？为什么「生产一律 `-O`」是个危险建议？
9. 为什么 `python3 app.py` 不会生成 `app.cpython-314.pyc`，但 `import app`（被别的脚本 import）会？
10. `sys.prefix == sys.base_prefix` 这个判断在干什么用？本机是相等还是不等？在 `--without-pip` 建出的 venv 里呢？

答不上来的，直接跳到对应小节再看一次真实输出 —— 本章的输出就是答案原文。

## 怎么跑这章的 demo

```bash
cd python-handbook/01-whats-really-happening
bash demo.sh              # 全跑一遍，退出码 0，约 1200 行输出
bash demo.sh --keep       # 跑完保留沙箱并打印路径，可以自己进去继续敲
bash demo.sh -h           # 用法
```

要点（都在 `demo.sh` 里，不用你操心）：

- 所有会写文件的操作都在 `mktemp -d` 出来的沙箱里；跑完 `trap cleanup EXIT` 删掉。
  所以**仓库里不会出现 `__pycache__`/`*.pyc`**（demo 第 10 节自己用 `find` 证明了）。
- **故意不加 `set -e`**：本章有十几条命令是「注定失败」的（`python --version`、`-m path0` 换目录、
  `compileall --force`、各种 traceback）。加了 `set -e` 就只会看到第一段，学不到东西。
- 每段前打 `$ 命令`，段末打 `[退出码 N]`，非零也照样继续。
- `export LC_ALL=C.UTF-8` 保证中文对齐；只有 `[ -t 1 ]`（真的接到终端）才上色，
  所以 `bash demo.sh > out.txt` 拿到的是干净纯文本。
- 沙箱里源码的 mtime 被 `touch` 固定成 `2026-01-01T10:00:00+08:00`，pyc 头部才是可逐字比对的。

单独跑某一节也行，把那条 `demo.sh` 里打印出来的命令复制进终端即可（注意：脚本方式的 `sys.path[0]`
会在沙箱里，你在仓库目录直接跑也一样能复现，只是会当场生成 `__pycache__`，记得删）。

## 下一步

- 现在你知道了：`python3` 是软链、`sys.executable` 可能是另一个名字、venv 的判据是 `prefix != base_prefix`，
  而本机连 pip 都没有 → 第 **09 章《环境、venv、以及为什么本机没有 pip》** 会按这个真实情况讲。
- 场景 2 里「名字是整数索引、全局要查 dict、局部按索引取」这句话还没证 → 第 **04 章《变量、作用域与查找顺序》**。
- 场景 3 只证了「import 按 sys.path 顺序翻」，没讲 finder/loader 协议、包与 `__init__.py`、循环 import →
  第 **05 章《import 与模块系统》**。
- 场景 5 只处理了「看得见/看不见」，没讲 stdout/stderr 的编码、换行、重定向姿势 →
  第 **03 章《文本、编码与 I/O》**、第 **08 章《文件与路径》**。
- 场景 7 里 `exc.__traceback__` 是「栈的快照」这句，和第 8 节的 `-W error`，都指向第 **07 章《异常》**。
- 场景 9 只量了一次；`-X importtime`、`timeit` 的陷阱、什么时候该用 `cProfile` → 第 **10 章《性能直觉与测量》**。

本章全部结论的原始证据：`demo.sh` + 上面各节引用的输出。想核对任何一行，
`bash demo.sh > /tmp/py01.txt 2>&1` 之后 `grep -F '那一行' /tmp/py01.txt` —— 除了沙箱随机后缀和
带 jitter 的计时数字，都应当逐字命中。
