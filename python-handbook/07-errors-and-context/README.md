# 第 07 章 · 报错与清理：异常不是惩罚，是你拿到的第一手证据

> 本章所有输出都是本机 `Python 3.14.4` 真跑出来的，一条都没编、一条都没改。
> 跑法：`cd python-handbook/07-errors-and-context && bash demo.sh`（21 节，跑完自动删沙箱）。
> 想自己拨开关：`bash demo.sh --keep`，然后进打印出来的那个 `/tmp/py07-XXXXXX` 里接着敲。
>
> 本章的每条命令都长这样：`$ python3 某个文件.py` 或 `$ python3 -c "..."`，
> 全部只读写 `mktemp -d` 出来的沙箱，不碰你的 `~/.config`、不联网、不装包、不开任何监听端口。
> 第 15 节有计时：**绝对值每跑必抖，那一节只有倍率可读**，其它节的输出是确定的。

---

## 第 00 节 · 本章在讲什么，以及为什么把它叫「证据」

前六章讲的是「怎么把东西造出来」：类型、字符串、容器、函数、类。这一章讲另一半 ——
**事情不对的时候，程序里到底发生了什么，以及你怎么知道**。

很多人学 `try/except` 学成了一套「防崩咒语」：把可疑代码包起来，`except: pass`，于是一个都不崩了。
本章要反过来讲两件事：

> **一、异常是一条有走位、有顺序、有链的控制流。它不是「崩了」，它是「你现在拿到了第一手证据」——
> 类型、消息、`errno`、行列号、帧链，全在那个对象上，你接不接是你的选择。**
>
> **二、`with` 不是「写起来好看」，它只保证一件事：清理必然执行。
> 而 `__exit__` 的返回值能把异常就地抹掉 —— 这是全章最危险的一行代码。**

这两句话各自对应本章一半的篇幅。前半（第 1~9 节）把 `try` 的走位、异常的家谱、`except` 的匹配顺序、
异常链 `__cause__`/`__context__`、`raise` 的三种形态、traceback 怎么读、报错原文怎么查、自定义异常怎么写，
一条一条现场构造出来；后半（第 10~19 节）把 `with` 拆成两个 dunder，再往上叠
`@contextmanager`、`contextlib` 的现成件、`ExitStack`、计时、静默坑、进程边界、真实文件，
最后第 19 节把零件装成一个真工具。第 20~21 节是本章自己给自己做的对账与自证。

| 你想搞清楚的东西 | 本章用哪一节回答 | 一句话答案 |
|---|---|---|
| `else` 和 `finally` 到底什么时候跑 | 第 2 节 | `else` = try 没炸才走；`finally` = 离开这条语句那一刻**必执行的尾巴**，return / break / 正在往外抛都先跑它 |
| `except Exception` 接不住什么 | 第 3 节 | `KeyboardInterrupt` / `SystemExit` / `GeneratorExit` 是 `BaseException` 的儿子，不是 `Exception` 的 —— 这是故意的 |
| 两个 `except` 分支谁先命中 | 第 4 节 | 从上往下，第一个 `isinstance` 成立的赢；**写反了不报错、不警告**，只是窄的那条永远变死代码 |
| `raise X from Y` 那条链是什么 | 第 5 节 | `__cause__` 是你写的，`__context__` 是解释器记的；traceback 中间那句连接语就是它的出生证明 |
| `raise` 后面能跟什么 | 第 6 节 | 异常类、异常实例、或者什么都不跟（裸 raise）；跟别的当场 `TypeError` |
| `assert` 为什么不能守用户输入 | 第 6 节 | `assert` 就是 `raise` 的缩写，而 `python3 -O` 把那一整行**在编译时删掉**（本章真跑给你看） |
| traceback 那一大坨怎么读 | 第 7 节 | 只看两处：最后一行的「类型: 消息」，和最深那一帧的 文件 / 行号 / 函数名 |
| 能不能不靠肉眼拿现场 | 第 7 节 | `sys.exc_info()` / `exc.__traceback__` / `traceback.extract_tb` 全是结构化数据，帧上有 `.name` 和 `.line` |
| 看到报错怎么定位 | 第 8 节 | 一张「看到什么往哪查」的表；`TypeError` 查形状、`ValueError` 查值 |
| 自定义异常要写几行 | 第 9 节 | 三行：类 + 继承 + 需要 payload 时挂字段。要跨进程还得管 pickle |
| `with` 到底是什么机制 | 第 10 节 | `__enter__` 的返回值绑给 `as`，`__exit__(type, val, tb)` 的**返回值**决定异常要不要继续跑 |
| 一行写三个 `with` 谁先关 | 第 11 节 | enter 左→右，exit 右→左；第 k 个 enter 失败时，前 k−1 个照样被还 |
| `@contextmanager` 那套 | 第 12 节 | 一个 `yield` 把函数劈成 enter / exit；**yield 之后必须包 `try/finally`**，且结果只能用一次 |
| `suppress` / `closing` / `nullcontext` / `redirect_stdout` | 第 13 节 | 四个都是 `@contextmanager` 写的普通 Python，本章把 `suppress` 的源码整个打出来了 |
| 资源个数运行时才知道怎么办 | 第 14 节 | `ExitStack.enter_context()` + `callback()`；`pop_all()` 把「谁负责关」整个转手出去 |
| 包一层 `try` 要不要钱 | 第 15 节 | 不进 `except` 的 try 几乎免费；**真的抛**才贵，而且栈越深越贵（本机倍率实测） |
| 什么错最不危险 | 第 16 节 | **不报错的错**。裸 except、循环里吞异常、只存 `str(exc)`、接了异常还退 0 |
| 异常走到进程边界会怎样 | 第 17 节 | 未捕获 = 退 1 + 全文进 stderr；`sys.exit(n)` 就是 `raise SystemExit(n)`；子线程崩了**不改变**退出码 |
| 配置文件怎么写才不会留半截 | 第 18 节 | `with` 只管 close，不管事务：同目录临时文件 + `os.replace`；I/O 一律 EAFP 按 `errno` 分派 |
| 这些零件拼起来长什么样 | 第 19 节 | 一个「安全读配置」的工具：三层异常家族 + 只对该重试的那一支重试 + 现场对账 |
| 本章的「19 个脚本 / 139 段」是谁数的 | 第 20 节 | 一个对账脚本数的，而且它当场**造三个坏样本让自己响** —— 不响的检查等于没检查 |

**读法建议**：第 2~6 节是地基，顺序别打乱（走位 → 家谱 → 顺序 → 链 → raise）；
第 7~8 节是「查错用的字典」，平时跳着查就行；第 9 节开始写自己的异常。
第 10~14 节是 `with` 专区，只想会用 `with open()` 的人看第 10、12、14 三节就够；
第 15~18 节是工程账（贵在哪、静默在哪、边界在哪、文件怎么写）；第 19 节把前面全部串起来。
本章有两个「★」级的大坑：**第 2 节第 5 段**（`finally` 里 `return` 把异常整个吞掉）和
**第 10 节第 4 段**（`__exit__` 返回真值咽异常）—— 它们共同点是**现场一点痕迹都没有**。

---

## 第 01 节 · 环境体检：本章结论只在这么一个解释器上验过

异常这一块比前面几章更**版本敏感**：`except*` 和 `BaseExceptionGroup` 是 3.11 才有的语法，
`@dataclass(slots=True)` 之类影响 `AttributeError` 文案，报错消息这几年一直在改（`UnboundLocalError`
那句「cannot access local variable」就是改过的）。所以第一步照例把解释器的身份钉死。

本章 19 个 `.py`（18 个取证脚本 + 第 20 节那个对账工具）+ `demo.sh`，demo 把它们整份复制进沙箱再跑 ——
章目录里**不会**留下任何产物（第 21 节当场自证）：

```
$ ls
a_minimal_shape.py
b_tree.py
c_matching.py
d_chain.py
demo.sh
e_raise.py
f_traceback.py
g_reading.py
h_custom.py
i_with_protocol.py
j_with_more.py
k_contextlib.py
l_std_tools.py
m_exitstack.py
n_cost.py
o_silent.py
p_process.py
q_files.py
r_capstone.py
s_checklist.py
[退出码 0]
```

解释器和它报出的自己：

```
$ python3 --version && python3 -c "import sys; print(sys.version)"
Python 3.14.4
3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]
[退出码 0]
```

本章用到的模块只有标准库那几个，一句话验齐 —— 顺便把「`contextlib` 里本章点名的六个名字是不是真在」也验了：

```
$ echo "LC_ALL=$LC_ALL  PYTHONHASHSEED=$PYTHONHASHSEED"; python3 -c "
import sys, contextlib, traceback, json
print(\"只用标准库这四个 + 内置：\", {\"contextlib\", \"traceback\", \"json\"} <= sys.stdlib_module_names)
print(\"contextlib 里本章用到的名字：\", [n for n in (\"suppress\",\"closing\",\"nullcontext\",\"redirect_stdout\",\"ExitStack\",\"contextmanager\") if hasattr(contextlib, n)])
"
LC_ALL=C.UTF-8  PYTHONHASHSEED=0
只用标准库这四个 + 内置： True
contextlib 里本章用到的名字： ['suppress', 'closing', 'nullcontext', 'redirect_stdout', 'ExitStack', 'contextmanager']
[退出码 0]
```

两条与后文直接相关的定值：

```
$ python3 -c "import os; print(\"os.cpu_count() =\", os.cpu_count())"
os.cpu_count() = 4
[退出码 0]
```

```
$ python3 -c "import sys; print(\"递归限额 =\", sys.getrecursionlimit(), \"｜ 第 15 节的深栈成本就按它算\")"
递归限额 = 1000 ｜ 第 15 节的深栈成本就按它算
[退出码 0]
```

三点要点：

1. **`contextlib` / `traceback` / `json` 全在 `sys.stdlib_module_names` 里** —— 也就是说
   本章**不需要装任何东西**。第 13 节还要用 `inspect.getsource` 读标准库自己的源码，那也是标准库。
2. **`PYTHONHASHSEED=0`** 是 demo 显式设的：本章要看日志列表的顺序、`args` 的元组内容，
   不固定种子的话同一份代码两次跑顺序可能翻，就没法拿它当证据。
3. **`os.cpu_count() = 4`、递归限额 = 1000**：第 15 节所有的 µs 数都受这台机器当时的负载影响，
   **只有倍率可读**；第 7 节第 6 段那个「重复 N 次」的 N 也因此不能当证据引用。

---

## 第 02 节 · 一条 `try` 语句的走位：`else` / `finally` 到底什么时候跑

`try` 的四个块（`try` / `except` / `else` / `finally`）不是四个并列选项，它们是**一条有分支的走位图**。
绝大多数「这段代码到底跑不跑」的问题，背这张图就够了。本节的脚本把每一步都打一行，
于是两条走法（炸 / 不炸）各自跑过哪几行，屏幕上直接看得见。

```
$ python3 a_minimal_shape.py
=== 1. 一条 try 语句的两种走法（每一步都打一行）===
  不炸的走法：
    [1] try 块开始
    [2] try 块结尾（异常之后这行不跑）
    [4] else：只有 try 里一行都没炸才走这里
    [5] finally：无论如何都走
    [6] 整条 try 语句之后的第一行
  返回值 = 正常收尾
  炸的走法：
    [1] try 块开始
    [3] except 接住了：故意炸
    [5] finally：无论如何都走
    [6] 整条 try 语句之后的第一行
  返回值 = 正常收尾
```

注意上面那个细节：**炸的那条走法里，`[2] try 块结尾` 这一行没有出现** ——
`try` 块里一旦抛出，同一个块里剩下的行就全跳过了，直接去找 `except`。
而 `else` 那行只在「一行都没炸」的时候出现。

`except` 的类型不对时，接不住、但 `finally` 照跑，`else` 也轮不到：

```
=== 2. except 类型不对：接不住，但 finally 照跑，后面的行不跑 ===
    [1] try 块开始
    [5] finally —— 这行还是会打
  外面接住了： KeyError | '我是 KeyError，ValueError 那条 except 接不住'
```

实战里最常见的一种写法 —— `except` 里给个兜底返回值。它合法，但它埋了个雷：

```
=== 3. 实战最常见的一种：except 里给个兜底返回值 ===
  parse('42') = 42 ｜ parse('四十二') = None
  → 这就是第 05 章第 12 节那条坑的另一面：调用方分不清「值就是 None」和「转换失败」，
     要么返回 (成功, 值) 元组，要么**让它抛出去**由调用方 except（见第 5、6 节）。
```

调用方拿到 `None`，**分不清「转换失败」还是「值本来就是 None」**。这就是第 05 章第 12 节那条
「没写 `return` 就是 `None`」的坑的另一面。两条正解：返回 `(成功, 值)` 元组，或者**让它抛出去**、
由调用方 `except`（第 5、6 节讲怎么抛得有价值）。

接下来三条是本节的正菜：**`finally` 到底在哪些时刻执行**。

```
=== 4. try 里 return，finally 依然执行 ===
    finally 跑了（return 的值先存好，但函数还没真正离开）
  with_return('7') = 7
    finally 跑了（return 的值先存好，但函数还没真正离开）
  with_return('x') = -1
```

`try` 里已经写好 `return` 的值了，`finally` 依然跑 —— 因为「返回」这个动作要等 `finally` 这段尾巴走完。

★ 但如果 `finally` 里自己 `return`，事情就完全变样了：

```
=== 5. ★finally 里自己 return：把异常整个吞掉，现场一点痕迹都没有 ===
  编译那一刻解释器主动发出 1 条警告：
    SyntaxWarning: 'return' in a 'finally' block
  跑它：swallow() = from finally ← 异常没了，一个字的报错都没打
    finally 只 print，不 return
  对照：把 finally 的 return 换掉，异常就照常抛出去 → ValueError | 这句会被看见
```

这一条是本章第一个**没有任何痕迹**的坑：异常被吞了，屏幕上连一个字都没打。
解释器自己都看不下去，在**编译那一刻**主动发了 `SyntaxWarning: 'return' in a 'finally' block` ——
注意这是警告不是错误，代码照样跑，警告只在编译那一次出现。这条评论在第 16 节还会出现一次。

跳出（`break` / `continue`）也不算例外：

```
=== 6. 从 try 里 break / continue 出去，finally 也照样执行 ===
    第 0 轮的 finally
    第 1 轮的 finally
  break 那一轮的 finally 也跑了（第 0 轮是 continue，第 2 轮根本没开始）。
```

**一句话判据**（脚本第 7 段自己打的）：

```
=== 7. 收口：一句话判据 ===
  try 只决定「异常从哪一行开始往外找 except」；
  else = try 没炸才走；finally = 离开这条 try 语句那一刻**必执行的尾巴**，
  不管是正常走完、return、break，还是正在往外抛 —— 都先把它跑完。
  而 finally 里的 return / raise 会覆盖一切 —— 这正是「吞异常」的机制。
[退出码 0]
```

---

## 第 03 节 · 异常也是一棵树：家谱、以及 `except Exception` 接不住的那三个

`except ValueError` 接不住 `KeyError`，但 `except LookupError` 能 —— 因为异常不是一个个孤立的类，
它们是一棵树。这一节先把树画出来（用的还是第 06 章那套 `__mro__`），再回答一个非常具体的问题：
**为什么写 `except Exception` 接不住 Ctrl-C？**

一条线的家谱：

```
=== 1. 一条线的家谱：从 object 到 ValueError ===
  ValueError     的 MRO： ValueError → Exception → BaseException → object
  Exception      的 MRO： Exception → BaseException → object
  BaseException  的 MRO： BaseException → object
  object         的 MRO： object
```

常见的内置异常直接挂在哪个分支上（这个顺序是脚本里写死的名单，逐个打印）：

```
=== 2. 常见的内置异常直接挂在哪个分支上（逐个打印，顺序固定）===
  TypeError            的直接父亲 = Exception
  StopIteration        的直接父亲 = Exception
  StopAsyncIteration   的直接父亲 = Exception
  AttributeError       的直接父亲 = Exception
  SyntaxError          的直接父亲 = Exception
  NameError            的直接父亲 = Exception
  RuntimeError         的直接父亲 = Exception
  ImportError          的直接父亲 = Exception
  OSError              的直接父亲 = Exception
  EOFError             的直接父亲 = Exception
  ValueError           的直接父亲 = Exception
  AssertionError       的直接父亲 = Exception
  ArithmeticError      的直接父亲 = Exception
  LookupError          的直接父亲 = Exception
  IndexError           的直接父亲 = LookupError
  KeyError             的直接父亲 = LookupError
  MemoryError          的直接父亲 = Exception
  SystemError          的直接父亲 = Exception
```

有三个位置值得单独记：`IndexError` 和 `KeyError` 都是 `LookupError` 的儿子（所以 `except LookupError`
一网打尽），`ImportError` 和 `OSError` 是**直接挂在 `Exception` 上**的（所以 `except OSError` 只兜文件/网络，
兜不住 `ImportError`）。

还有几个「父亲是 `Exception` 的窄门」，捕获时只能靠子类名：

```
=== 3. 两个「父亲是 Exception」的窄门：捕获时只能靠子类名 ===
  UnicodeError 的父亲： ValueError （所以是 OSError 的儿子）
  UnboundLocalError 的父亲： NameError
  ModuleNotFoundError 的父亲： ImportError
  FileExistsError / FileNotFoundError 的父亲： OSError / OSError
  → 第 05 章那个 UnboundLocalError 是 NameError 的儿子：except NameError 能接住它，实测过。
  接一下试试：
    except NameError 接住了： UnboundLocalError | cannot access local variable 'counter' where it is not associated with a value
```

第 05 章那个 `UnboundLocalError` 是 `NameError` 的儿子 —— 上面最后一行是**现场接住它**的原文，
消息就是那句「cannot access local variable」。

★ 然后是本章最重要的一条结构事实：**树根上挂着三个「不该被 `except Exception` 抓住」的退出类异常**。

```
=== 4. ★树根上挂着三个「不该被 except Exception 抓住」的退出类异常 ===
  KeyboardInterrupt  的父亲 = BaseException ｜ 是 Exception 的儿子吗 → False
  SystemExit         的父亲 = BaseException ｜ 是 Exception 的儿子吗 → False
  GeneratorExit      的父亲 = BaseException ｜ 是 Exception 的儿子吗 → False
```

它们仨的父亲是 `BaseException`，不是 `Exception`。这不是遗漏，是设计：

```
=== 5. 现场验证：except Exception 让 KeyboardInterrupt 跑掉了 ===
    finally 还是跑了 —— 清理逻辑不受影响
  外层接住了： KeyboardInterrupt | 模拟按下 Ctrl-C
  → 结论：except Exception **故意**放过「用户/解释器要求退出」这一族。
```

`except Exception` **故意**放过「用户 / 解释器要求退出」这一族，所以你写的兜底不会把 Ctrl-C 变成没反应。
而一旦你写裸 `except:`（等价于 `except BaseException:`），这道闸门就被你自己拆了：

```
=== 6. 裸 except 会把 SystemExit 一起咽掉（这就是它被禁的理由） ===
  用 except BaseException： 被接住了 —— 退出请求消失
  用 except Exception：
    SystemExit 原样抛出 → 想退出
```

上面这两段是同一个脚本里的两个分支，看清楚那条对比：`except BaseException` 把退出请求**接住了**，
`except Exception` 让它**原样抛出**。这就是裸 except 被禁的全部理由 —— 它不是「不优雅」，它是**安全洞**。

最后一族要挂号：`except*`。它不是「另一种写法的 except」，它面对的是**另一棵树**（一次抛出一组异常）：

```
=== 7. except* 是另一棵树（ExceptionGroup，本机 3.14 支持）：本章只证明它存在 ===
  本机有这个内置类吗 → True
  except* ValueError 收走了其中一个分支： ExceptionGroup | 子异常个数 = 1 | 甲
  except* KeyError 收走了另一个分支： ExceptionGroup | 子异常个数 = 1 | '乙'
  → 两个分支都跑了，这就是 except* 和 except 的根本区别（它按**类型**切开一组异常，各自分派）。
```

`except*` 按**类型**把一组异常切开、各自分派，所以两个分支都跑了。
本机 3.14 支持，但本章只证明它存在、语义是什么，**不把它当日常工具讲**（并发框架里才会真撞上，见「本章没演」）。

**收口**：

```
=== 8. 收口 ===
  ① except 匹配的是 **isinstance**，所以「抓父亲就抓到全部儿子」；
  ② 只有 BaseException 那一层会连退出信号一起抓走，写 except: / except BaseException: 都要给理由；
  ③ 报错原文里的第一行永远是 `模块.类名: 消息`，那个类名就是这棵树上的一个叶子 —— 认树就能认错。
[退出码 0]
```

---

## 第 04 节 · `except` 的匹配顺序：写反了不报错，只是永远轮不到

上一节说了异常是一棵树。这一节说树带来的第一个工程后果：**多个 `except` 分支是从上往下匹配的**，
父亲写在儿子前面，儿子的分支就变成死代码 —— 而且**不报错、不警告**。
这是本章最容易被 code review 漏掉的一条。

```
$ python3 c_matching.py
=== 1. 顺序决定一切：把父亲写在前面，儿子的分支永远跑不到 ===
  raise Timeout() → 走进的分支 = 父亲 NetError
  → 结果是对的（父亲能接儿子），但那个专门写 Timeout 的分支**永远跑不到**，
     而且不报错、不警告 —— 这是本章最容易被 code review 漏掉的一条。
```

注意上面那句「结果是对的」：`Timeout` 确实被接住了，程序行为看起来没问题。
坏的是那个专门为 `Timeout` 写的分支再也不会执行 —— 它可能带着你精心写的重试逻辑。
把顺序倒过来才是对的：

```
=== 2. 正确顺序：儿子在前、父亲在后，窄门先过 ===
  raise Timeout   → 儿子 Timeout 分支（timeout）
  raise DNSFail   → 儿子 DNSFail 分支（dns）
  raise NetError  → 父亲 NetError 兜底
```

三条分支各归各位：窄的先过，父亲在最后兜底。

一个 `except` 收多类要用**元组**：

```
=== 3. 一个 except 收几类：元组，以及为什么不能用 `except A or B` ===
  except (TypeError, ValueError) as exc → ValueError | 元组分支
  写法对比（都是同一个意思，但只有第一种合法且可读）：
    except (TypeError, ValueError):   ← 推荐
    except TypeError, ValueError:     ← Python 2 写法，3 里是 SyntaxError
```

`except A or B` 这种「看起来更省事」的写法根本不存在；`except TypeError, ValueError:` 是 Python 2 的写法，
在 3 里当场 `SyntaxError`（脚本第 3 段把这条也构造出来了）。元组里放父亲，就连儿子一起收；
放进去的类型才是真的「不接」：

```
=== 4. 元组里放父亲 = 连儿子一起收；放不进去的那类原样往上抛 ===
  raise Timeout            → 接住了 Timeout
  raise KeyError           → 接住了 KeyError
  raise ZeroDivisionError  → 没接住，原样往上抛（外层才接）：ZeroDivisionError
```

然后是两条「和直觉不一样」的：

```
=== 5. ★except 分支里再抛异常：原来那个变成「上下文」，不是原因 ===
  外面接住的是： TypeError | 处理时又炸了一个
  它的 __context__ 是： KeyError | '原始的键错'
  __cause__ 是： None ← 没写 raise from 就真的是 None（第 5 节细讲）
```

`except` 分支里再抛一个新异常，**外面看到的是新那个**，原来那个降级成 `__context__`（第 5 节细讲）。
而 `__cause__` 是 `None` —— 因为 `__cause__` **只由你写 `from` 产生**，解释器从不替你填。

第二条：`except ... as exc` 的那个名字，出了块就被删掉了。这不是「回收时机」，是语言规定的 `del`：

```
=== 6. except 分支里的名字会被自动删掉（这是真规矩，不是回收时机）===
  except 块里能看到 exc 吗 → True | 内容 = ValueError('临时错误')
  出了 except 块还在吗 → False
  原因：CPython 在离开 except 块时执行 del exc（防止引用环把帧留住）。
  想留证据就自己接一份：saved = exc.args 或者 saved = str(exc)。
  留档之后： ('ValueError', '要留档的错误') ｜ 块外的 exc 存在吗 → False
```

所以想把证据留到块外，得自己接一份（`saved = exc.args` 或 `str(exc)`），别指望 `exc` 活着。
上面最后一行还留了一份档，用来证明「留档可以，但 `exc` 本身确实没了」。

**收口**：

```
=== 7. 收口 ===
  ① 从上往下，第一个 isinstance 成立的分支赢；顺序错 = 静悄悄少收一类；
  ② 元组收多类，父亲在元组里就连儿子一起收；
  ③ except 块里再抛异常，原异常降级成 __context__（不是 __cause__）；
  ④ except as 的那个名字在块结束时被删掉，别指望它活着。
```

---

## 第 05 节 · 异常链：`__cause__` 是你写的，`__context__` 是解释器记的

包装异常是好事（把「底层细节」翻译成「业务说法」），但传话的时候最容易丢现场。
Python 在异常对象上挂了两个字段专门干这件事，而它们**语义完全不同**，记混了查错方向就反了：

- `__cause__` —— 只有你写 `raise New(...) from exc` 时才填，意思是「**这个**就是原因」。
- `__context__` —— 只要「处理异常期间又抛了一个」，解释器自动记下前一个，意思是「当时手上正处理着**这个**」。

显式链：

```
=== 1. 显式链：raise ... from ... ===
    最外层看到： 配置项要的是数字，拿到的是 '四十二'
    __cause__   = ValueError("invalid literal for int() with base 10: '四十二'") ← 你写的「原因」
    __context__ = ValueError("invalid literal for int() with base 10: '四十二'")
    __suppress_context__ = True
```

隐式链（同一段代码，只把 `from exc` 删掉）：

```
=== 2. 隐式链：没写 from，原异常自动降级成 __context__ ===
    __cause__   = None ← 没写 from 就真的是 None
    __context__ = KeyError('缺键')
  → 所以「处理时又炸了一个」不会丢现场：两个都在对象上。
```

差别就在 `__cause__` 是不是 `None`。**现场两个都在对象上，一个字都没丢** —— 丢不丢是你打印时决定的。

那两条链打印出来长什么样？本节把它整个跑在子进程里打（脚本第 3 段解释了为什么：在本进程打 traceback
会带上本机绝对路径，塞进 `<string>` 里则只有行号，取证要的是**形状**）：

```
=== 3. 一条链打印出来长什么样（整份原文，跑在子进程里）===
  为什么要跑到子进程里打：在本进程打 traceback 会带上**本机绝对路径**，
  放进 `<string>` 里则只有行号。取证要的是形状，不是我这台机器的目录。
Traceback (most recent call last):
  File "<string>", line 4, in make_chain
    raise ValueError('根因')
ValueError: 根因

The above exception was the direct cause of the following exception:

Traceback (most recent call last):
  File "<string>", line 8, in <module>
    make_chain()
    ~~~~~~~~~~^^
  File "<string>", line 6, in make_chain
    raise RuntimeError('外层说法') from exc
RuntimeError: 外层说法
  --- 上面这段的行数 = 14 ｜ 中间那句连接语 = The above exception was the direct cause of the following exception:
  --- 有几处 'Traceback (most recent call last):' → 2 （每一段链各一个抬头）
```

中间那句连接语是这一段的身份证。隐式链的形状一模一样，只有那句话不同：

```
=== 4. 隐式链的打印形状：那句连接语不一样 ===
  连接语原文 = During handling of the above exception, another exception occurred:
  整段行数 = 16 ｜ 抬头出现次数 = 2
  → 两段形状一模一样，**只有中间那句话不同**；所以查日志时先找那句连接语，
    它告诉你「这个链是有人显式写的，还是处理时又炸了一次」。
```

所以查日志时**先找那句连接语**，它告诉你「这条链是有人显式写的，还是处理时又炸了一次」——
前者的方向是「包装层说人话了没有」，后者的方向是「处理逻辑本身有 bug」。

★ 反方向的操作也值得钉一条：`raise ... from None` 是**把链藏起来**（标准库自己大量使用，
目的是不让内部细节污染用户的报错）：

```
=== 5. raise ... from None：把上下文藏起来（标准库自己就这么干）===
    报出来的只有一句： 必须是甲
    __context__ 对象还在吗 → KeyError('乙') （在，只是不打印）
    __suppress_context__ = True
  判据：`from None` 不是「不产生链」，是「不把链打给你看」。调试时可以把
  __suppress_context__ 改回 False 再打一遍，现场其实一个字都没丢。
```

判据要写准：`from None` 不是「不产生链」，是「不把链打给你看」—— 对象上 `__context__` 还在，
`__suppress_context__` 变成 `True`。调试的时候把它改回 `False` 再打一遍，现场其实一个字都没丢。

帧链也不只是文本。异常对象自己带着 `__traceback__`，那是一条链表：

```
=== 6. __traceback__ 是一个链表，帧号能直接读出来 ===
  最外层异常的帧数 = 2 | 各帧所在的源码行号 = [130, 117]
  每一帧上能拿到的东西 = ['tb_frame', 'tb_lasti', 'tb_lineno', 'tb_next']
  __cause__ 自己也有 traceback：帧数 = 1
```

四个字段里 `tb_next` 就是「下一帧」，所以行号是**读出来**的，不需要去 split 那段文本（第 7 节把这条做完）。
最后一个入口是给库作者用的，谨慎：

```
=== 7. with_traceback：把「别处的现场」挂到新异常上（库作者的工具，谨慎）===
  拿到的是： TypeError | 换个说法重抛
  但它的 traceback 来自上面那条 ValueError —— 帧数 = 2
  实际项目里几乎只有「重新包装异常」时才用；能用 from 就别动 traceback。
```

**收口**：

```
=== 8. 收口 ===
  ① __cause__ 只由 `raise ... from X` 写入；__context__ 由解释器在处理异常期间
     又抛新异常时自动写入 —— 两者可以都在，打印时优先 __cause__。
  ② 看到 `The above exception was the direct cause of ...` = 有人显式写了 from；
     看到 `During handling of the above exception, another exception occurred` = 隐式链。
  ③ 异常对象本身带着 __traceback__ 链表，帧号是读出来的，不是只有打印才有。
[退出码 0]
```

---

## 第 06 节 · `raise` 的三种形态、裸 `raise`、以及 `assert` 为什么不该给用户输入

`raise` 后面能跟的东西比想象中少，但每一种的**对象身份**都不一样：

```
$ python3 e_raise.py
=== 1. 类 / 实例 / 带参数：三种都合法，但对象不同 ===
  raise ValueError（类，现实例化）           → str() = '' ｜ args = ()
  raise ValueError('甲')（实例）          → str() = '甲' ｜ args = ('甲',)
  raise ValueError('乙','丙')（两个参数）    → str() = "('乙', '丙')" ｜ args = ('乙', '丙')
```

`raise ValueError`（跟的是类）当场替你实例化，所以 `args` 是空元组、`str()` 是空串；
跟实例就原样用；给多个参数，`str()` 打出来的是那个**元组的样子** —— 记住 `args` 才是原样保存的地方。

跟别的东西就直接拒绝：

```
=== 2. raise 一个「不能当异常」的东西：当场 TypeError ===
  raise 字符串    → TypeError | exceptions must derive from BaseException
  raise 整数     → TypeError | exceptions must derive from BaseException
  raise None   → TypeError | exceptions must derive from BaseException
  raise 普通类    → TypeError | exceptions must derive from BaseException
```

四条报错原文完全一致：`exceptions must derive from BaseException` —— 也就是说这条判据不看形状、只看**家谱**（第 3 节那棵树）。

裸 `raise`（不跟任何东西）只有一条规矩：得正处在「处理异常」里。

```
=== 3. 裸 raise：只有一条规矩 —— 得有「正在处理的异常」 ===
  except 里裸 raise → 帧数 = 2 | 帧名 = ['<module>', 'boom'] | 消息 = 'k'
  没有上下文时裸 raise → RuntimeError | No active exception to reraise

=== 4. `raise` 与 `raise exc`：同一条异常，traceback 长度不一样 ===
  裸 raise        → 帧数 = 2 | 帧名 = ['<module>', 'boom2']
  raise 那个对象  → 帧数 = 3 | 帧名 = ['<module>', '<module>', 'boom2']
  → 两行的差别就在「有没有把 raise 所在这一帧也记进去」。
     想原样往上交就裸 raise；要改消息/换类型就 raise New(...) from exc（第 5 节）。
```

`RuntimeError: No active exception to reraise` 就是没上下文时的原文。
而同一份逻辑里，`raise` 和 `raise exc` 的** traceback 长度不一样** —— 上面那段最后一行把帧名列出来了：
裸 raise 不把自己这一帧再记一遍。判据很简单：**想原样往上交就裸 raise；要改消息 / 换类型就 `raise New(...) from exc`**。

自定义异常的父亲选谁，直接决定调用方能怎么接你：

```
=== 5. 自定义异常：父亲选谁，决定别人怎么接你 ===
  AppError 的 MRO      = AppError → Exception → BaseException → object
  ConfigMissing 的 MRO = ConfigMissing → AppError → LookupError → Exception → BaseException → object
  except AppError     能接住 ConfigMissing 吗 → True
  except LookupError  能接住 ConfigMissing 吗 → True
  except KeyError     能接住 ConfigMissing 吗 → False
  except Exception    能接住 ConfigMissing 吗 → True
  实测：直接 raise 一个 BaseException 的儿子也合法，
    → KeyboardInterrupt | 自己造的（只是为了看形状）
```

`ConfigMissing` 同时是 `AppError` 和 `LookupError` 的儿子，所以两种口径都能接住它；
但它是 `ValueError` 的**路人**，`except KeyError` 接不住（上一节那条元组 / 家谱在这儿继续生效）。
最后一行是「直接 raise 一个 `BaseException` 的儿子也合法」—— 合法，但那是自找麻烦（第 9 节判据②）。

★ 本节最值钱的一条是 `assert`。它平时看起来像「轻量版 raise」，其实它是**一个能被命令行开关整行删掉的语句**：

```
=== 6. assert：它就是一个 raise，但能被 -O 整行删掉 ===
  正常跑：AssertionError | 除数不能为零（assert 的话）
  等价写法（assert 只是这条语句的缩写）：
    if not (b != 0): raise AssertionError('除数不能为零（assert 的话）')
  同一段代码用 `python3 -O` 跑（优化模式，assert 整行不存在）：
  -O 之下 assert 消失了，撞上真正的除零： ZeroDivisionError | division by zero
  → 同一份源码、只多一个 -O：assert 那一行在编译时就被扔掉了，于是除零真的发生，
     报错文案从「除数不能为零（assert 的话）」换成了内置那句 'division by zero'。
     判据不是「assert 会失效」这么轻：**你写的校验可以被一个命令行开关整行删除**，
     所以外部输入的守卫不能放在它身上（stderr 这次是空的吗 → True ）。
```

同一份源码，只多一个 `-O`：`assert` 那一行在**编译时**就被扔掉了，于是被它挡住的除零真的发生，
报错文案换成了内置那句 `division by zero`。子进程的 stderr 是空的（脚本里当场验了）。

判据不是「assert 会失效」这么轻：**你写的校验可以被一个命令行开关整行删除**。所以：

```
=== 7. 判据：什么时候 assert、什么时候 raise ===
  assert → 「这里不可能不成立」的内部不变量（写完就希望永远不为假）；
  raise  → 「输入/环境可能不合法」的外部事实（用户给的、磁盘上的、网络来的）。
  拿 assert 校验外部输入 = 用 -O 跑一次就全线失守，这是本章第三条大坑。
```

**收口**：

```
=== 8. 收口 ===
  ① raise 后面能跟：异常类、异常实例、什么都不跟（裸 raise）；别的都 TypeError。
  ② 裸 raise 只在「正在处理异常」的地方有效，否则 RuntimeError: No active exception to re-raise。
  ③ 要原样往上交就裸 raise（帧数最少）；要换说法就 `raise New(...) from exc`。
  ④ assert 是 raise 的缩写，而且能被 -O 删掉 —— 别用它守外部输入。
[退出码 0]
```

---

## 第 07 节 · traceback 怎么读，以及怎么不靠肉眼拿到它

一大坨 traceback 的恐怖感来自「不知道看哪」。其实**只需要看两行**，本节先把一整份原文打出来，
再告诉你读哪两行，最后把「程序里怎么拿到结构化现场」三件套演完。

```
$ python3 f_traceback.py
=== 1. 一份「没人接」的 traceback 长什么样（子进程里的原文）===
Traceback (most recent call last):
  File "<string>", line 10, in <module>
    top()
    ~~~^^
  File "<string>", line 8, in top
    return middle(6)
  File "<string>", line 5, in middle
    return leaf(x)
  File "<string>", line 2, in leaf
    return x / 0
           ~~^~~
ZeroDivisionError: division by zero
  ↑ 以上整段都是子进程的 stderr ｜ stdout 是空的吗 → True ｜ returncode = 1
```

（第 2 段把这三处标出来）

```
=== 2. 读法：只看两行就够了（最后一行 + 最深那一帧）===
  ① 最后一行 = 异常类型 + 消息： ZeroDivisionError: division by zero
  ② 第一行   = 固定开头： Traceback (most recent call last):
  ③ 中间的帧（从上往下 = 从调用者往被调用者），出事现场在最深的最后一帧：
      File "<string>", line 10, in <module>
      File "<string>", line 8, in top
      File "<string>", line 5, in middle
      File "<string>", line 2, in leaf
  函数名出现的顺序 = ['<module>', 'top', 'middle', 'leaf']
  帧数 = 4 ｜ stderr 总行数 = 12
```

顺序是：抬头永远是 `Traceback (most recent call last):`，中间的帧**从调用者往被调用者**排，
出事现场是**最深那一帧**（本例 `leaf`），最后一行才是结论「类型: 消息」。
函数名出现的顺序 `['<module>', 'top', 'middle', 'leaf']` 就是调用链本身。

想在程序里用现场，别去解析文本 —— 三个入口全是结构化数据：

```
=== 3. except 里能直接拿到的三样东西 ===
  sys.exc_info() 三个槽 = ZeroDivisionError | ZeroDivisionError('division by zero') | traceback
  和 `except ... as exc` 拿到的是同一个对象吗 → True ｜ True
  tb 不是字符串，是链表：type(tb) = traceback ｜ tb_next 的类型 = traceback
  结构化帧（函数名, 源码行）从上到下 = [('<module>', 'outer(9)'), ('outer', 'return middle(x)'), ('middle', 'return leaf(x)'), ('leaf', 'return x / 0')]
```

`sys.exc_info()` 的三个槽和 `except ... as exc` 拿到的是**同一个对象**（上面那行 True 就是验证），
`tb` 不是字符串而是链表（`type(tb) = traceback`），而 `traceback.extract_tb()` 直接给你
`(函数名, 源码行)` 的列表 —— 第 5 节那条 `__traceback__` 链表的可读版本。

打印有两个入口，内容一样、去处不同：

```
=== 4. 同一份现场的两种打印入口：内容一样，去处不同 ===
  format_exc() 给你字符串；print_exc(file=...) 往文件对象里写，返回值 = None
  两份文本逐字节一样吗 → True ｜ 行数 = 12
  这段文本的最后一行 = ZeroDivisionError: division by zero
  （不传 file= 的 print_exc() 写的是 sys.stderr，所以它进日志要靠第 18 节那套重定向）
```

`format_exc()` 给你字符串（进日志用它），`print_exc()` 往文件对象写、返回值是 `None`；
不传 `file=` 时它写的是 `sys.stderr`（第 13 节的 `redirect_stderr` 就是拦这个）。

`SyntaxError` 那一族长得完全不一样，因为它**死在编译期**：

```
=== 5. SyntaxError 是另一种形状：带 caret 指针，字段问得出来 ===
  类型 = SyntaxError ｜ 消息 = invalid syntax
  能问出来的定位字段 = filename 示例.py / lineno 1 / offset 7 / end_offset 8
  text 字段（出错那一行的原文）= 'def f(:\n'
  它的 traceback 帧数 = 1 （SyntaxError 是编译期报的，执行根本没开始）
  儿子们：IndentationError → True ｜ TabError → True
  ↑ 上面是「程序跑起来之后才 compile 字符串」，所以还剩 1 帧（就是调 compile 的那一帧）。
  真正启动时的语法错**连 Traceback 抬头都没有**（把同样的坏内容交给子进程去启动）：
     File "<string>", line 1
       def f(:
             ^
   SyntaxError: invalid syntax
  退出码 = 1 ｜ 里面有没有 'Traceback (most recent call last):' 这行 → False
```

三个要点：① 它带 `lineno` / `offset` / `end_offset` / `text` 这些**可以直接问出来的字段**，
不用去数caret；② 上面那段是「程序跑起来之后才 compile 字符串」，所以还剩 1 帧，
真正启动时的语法错**连 `Traceback (most recent call last):` 这行都没有**（脚本用子进程验了 False）；
③ `IndentationError` / `TabError` 都是它的儿子（`issubclass` 两个 True）。

深递归的 traceback 会被折叠：

```
=== 6. 深递归的 traceback 会被折叠（也跑在子进程里）===
   return f(n + 1)
   [Previous line repeated 996 more times]
   RecursionError: maximum recursion depth exceeded
  stderr 总行数 = 12 ｜ returncode = 1
  → `[Previous line repeated N more times]` 就是折叠标记；N 每跑都可能变，
     所以它只当量级看，**不能当证据引用**（第 05 章第 20 节：递归限额本身才是定值）。
```

**收口**：

```
=== 7. 收口 ===
  ① 读 traceback 只看两处：最后一行的「类型: 消息」，和最深那一帧的 文件 / 行号 / 函数名。
  ② 想存进日志：except 里 traceback.format_exc() 拿到字符串（第 18 节演它值多少钱）。
  ③ 想在程序里用：sys.exc_info() / exc.__traceback__ / traceback.extract_tb 全是结构化数据，
     不需要去 split 那段文本 —— 帧上有 .name 和 .line，直接读。
  ④ SyntaxError 那一族没有帧链可读，因为它死在编译期；报错信息里带 caret，看 lineno/offset。
[退出码 0]
```

---

## 第 08 节 · 报错原文集：这一节当字典查（看到什么往哪查）

这一节没有新机制，全是**在本机现场构造报错并把原文抄下来**。用法是：撞到某个报错，
按类型查这一节，看「同样这个类型还有哪几种病因」。本节构造了 8 族，
最后给一张「看到什么往哪查」的表。

`NameError` 一家两条原文，长得像、病因不同：

```
=== 1. NameError 一家的原文：长得像，病因不同 ===
  引用一个从没出现过的名字
      → NameError: name 'undefined_name' is not defined
  函数里读一个「这条路径没赋过值」的本地名
      → UnboundLocalError: cannot access local variable 'total' where it is not associated with a value
  模块里读一个不存在的名字（真·没定义）
      → NameError: name 'undefined_here' is not defined
  判据：`NameError: name 'x' is not defined` = 这个名字在这层根本没出现过；
        `UnboundLocalError` = 出现过（同函数里有赋值），但**这条执行路径**还没轮到它。
        父子关系在第 3 节证过：issubclass(UnboundLocalError, NameError) → True
```

判据：`NameError: name 'x' is not defined` = 这层**根本没出现过**这个名字；
`UnboundLocalError` = 出现过（同函数里有赋值），编译器判它成本地名，但**这条执行路径**还没轮到它。
父子关系第 3 节已经证过（`issubclass(UnboundLocalError, NameError)` → `True`），所以 `except NameError` 能接住它。

`AttributeError` 的三种原文：

```
=== 2. AttributeError：三种原文对应三种不同的病因 ===
  'abc'.append('x')（str 没这个方法）
      → AttributeError: 'str' object has no attribute 'append'
  None.append(1)（点号左边是 None）
      → AttributeError: 'NoneType' object has no attribute 'append'
  Bare().x（实例上没这个字段）
      → AttributeError: 'Bare' object has no attribute 'x'
  打错方法名时解释器给建议： 'str' object has no attribute 'apend'
  机器可读的字段只有这些：name = 'apend' ｜ obj 的类型 = str
  「Did you mean」是**模糊匹配**算出来的最近名字，不保证是你想要的 —— 别把建议当结论。
  另外三种本章另外几节演过的 AttributeError，先在这里挂个号：
    property 没有 setter（第 06 章第 8 节）、frozen dataclass 赋值（第 06 章第 11 节）、
    slots 实例加新属性（第 06 章第 13 节）。**报错类型一样，病因完全不同。**
```

中间那条「`None.append`」是真实项目里出现频率最高的一个：某个函数返回了 `None`，你当列表用了。
最后一行要点是：`Did you mean 'append'?` 是**模糊匹配**算出来的最近名字，不保证是你想要的 ——
建议是线索，不是结论。而机器可读字段只有 `name` 和 `obj`。

`TypeError` 家族 —— 同一句「类型不对」有九种原文（本节前八种都在这一节构造，第九种在下面）：

```
=== 3. TypeError 家族：同一句「类型不对」有九种原文 ===
  1 + '1'
      → TypeError: unsupported operand type(s) for +: 'int' and 'str'
  '1' + 1
      → TypeError: can only concatenate str (not "int") to str
  len(42)
      → TypeError: object of type 'int' has no len()
  sorted([1, 'a'])（元素互相比不了）
      → TypeError: '<' not supported between instances of 'str' and 'int'
  函数少传参数
      → TypeError: <lambda>() missing 1 required positional argument: 'b'
  函数多传参数
      → TypeError: <lambda>() takes 1 positional argument but 2 were given
  不可调用：x 是 1 却写 x()
      → TypeError: 'int' object is not callable
  不可迭代：list(42)
      → TypeError: 'int' object is not iterable
  不可哈希：把一个 list 放进 set
      → TypeError: cannot use 'list' as a set element (unhashable type: 'list')
  对照：`int('abc')` 报的是 ValueError，不是 TypeError ——
  int('abc')
      → ValueError: invalid literal for int() with base 10: 'abc'
  这条判据最省时间：**TypeError 查形状（类型/个数/能不能调），ValueError 查值（内容读不出来）**。
```

这一族最省时间的判据在最后两行：**`TypeError` 查形状（类型 / 个数 / 能不能调 / 能不能哈希），`ValueError` 查值（内容读不出来）**。
`int('abc')` 是 `ValueError` 而不是 `TypeError`，因为「字符串」这个形状是对的，是内容不行。

`LookupError` 家族 —— 都是「找不到」，但文案有三种长相：

```
=== 4. LookupError 家族：找不到，但文案有三种长相 ===
  [1, 2][9]
      → IndexError: list index out of range
  (1, 2)[9]
      → IndexError: tuple index out of range
  b'ab'[9]（bytes 也是同一族）
      → IndexError: index out of range
  'abc'[9]
      → IndexError: string index out of range
  {'a': 1}['b']
      → KeyError: 'b'
  del 一个不存在的键
      → KeyError: 'b'
  KeyError 的消息**自带引号**（它用 repr 表示那个键，和第 06 章第 6 节「容器里各走 repr」是同一套规矩），
  IndexError 的消息没有引号 —— 只看报错文本就能区分这两族。
  注意最后那条 `del` 报的也是 KeyError：取值和删值是同一扇门，错也是同一个错。
```

`KeyError` 的消息**自带引号**（它用 `repr` 表示那个键，和第 06 章第 6 节「容器里各走 `repr`」同一套规矩），
`IndexError` 没有 —— 只看报错文本就能区分这两族。最后那条 `del` 报的也是 `KeyError`：取值和删值是同一扇门。

`StopIteration` 是「收尾信号」，不是事故：

```
=== 5. StopIteration：它是收尾信号，不是事故 ===
  手动 next 到底 = 1
  next(已耗尽的生成器)
      → StopIteration: 
  同样的东西交给 list() 收尾（打得到这行就说明自动接住了）
      → 没报错，返回值 = [7, 8]
  判据：for / list() / sum() 内部替你接了 StopIteration；只有**手动 next()** 才需要接。
    在 traceback 里看到裸的 StopIteration，说明这段代码在手写迭代协议；`__iter__`/`__next__`
    那套本手册**没演**（第 04 章第 17 节只到生成器和惰性为止，它自己把迭代协议列进了「没演」）。
```

`for` / `list()` / `sum()` 内部替你接掉了它，只有**手动 `next()`** 才要接。
如果你在 traceback 里看到裸的 `StopIteration`，说明这段代码在手写迭代协议 —— 那套 `__iter__`/`__next__`
本手册没演（第 04 章第 17 节只到生成器和惰性为止，它自己把迭代协议列进了「没演」）。

和外部世界打交道的是 `OSError` 一家（本节只碰本地路径，不碰网络）：

```
=== 6. 和外部世界打交道：OSError 家族（只碰本地，不碰网络）===
  open('/肯定不存在/没有这个文件')
      → FileNotFoundError: [Errno 2] No such file or directory: '/肯定不存在/没有这个文件'
  os.remove 一个不存在的路径
      → FileNotFoundError: [Errno 2] No such file or directory: '/tmp/py07-绝对不存在'
  0 ** -1（不是 OSError，放进来做对照）
      → ZeroDivisionError: zero to a negative power
  OSError 独有的机器可读字段：errno = 2 ｜ strerror = No such file or directory ｜ filename = /肯定不存在/x
  args = (2, 'No such file or directory') （**两个**元素：errno + strerror，和其它异常不一样）
  FileNotFoundError 是它的儿子吗 → True ｜ PermissionError → True
  判据：文件/权限/网络一族的错**全都**是 OSError 的儿子。接 OSError 一网打尽，
    但要分清「没有」和「没权限」就得接窄的那两个。
```

`OSError` 独有三个机器可读字段：`errno` / `strerror` / `filename`，而且 `args` 是**两个**元素
（`errno` + `strerror`）—— 和其它异常「args 就是消息」的习惯不同。
判据：文件 / 权限 / 网络一族的错**全都**是 `OSError` 的儿子；接 `OSError` 一网打尽，
但要分清「没有」和「没权限」就得接窄的那两个（第 18 节落地）。

`RecursionError` —— 第 05 章那个递归限额在这里的长相：

```
=== 7. RecursionError：第 05 章的递归限额在这里的长相 ===
  deep(n) = deep(n+1)（这次接住了）
      → RecursionError: maximum recursion depth exceeded
  它是 RuntimeError 的儿子吗 → True ｜ 当前限额 = 1000
  → 限额到了是**抛异常**，不是把进程搞崩；所以能接住，接住之后栈已经退回来了一层。
```

关键结论：**限额到了是抛异常，不是把进程搞崩**。所以能接住，接住之后栈已经退回来了。

最后一张表把本节全部收拢（脚本自己打的）：

```
=== 8. 收口：一张「看到什么往哪查」的表 ===
  NameError         → 这层根本没出现过这个名字（拼错 / 忘 import / 定义在别的分支）
  UnboundLocalError → 同函数里有赋值，编译器判它成本地名，但这条路径先读了
  AttributeError    → 点号左边的东西没这个名字（也可能是 property 无 setter / frozen / slots）
  TypeError         → 形状不对：类型不匹配、参数个数、不可调用、不可迭代、不可哈希
  ValueError        → 类型对但值不行：int('abc')、转换失败、内容不合法
  IndexError        → 序列下标越界（list / tuple / str / bytes）
  KeyError          → 映射里没这个键；消息用 repr 所以自带引号
  StopIteration     → 迭代器耗尽 —— for 自动接，手动 next 才要接
  RecursionError    → 递归超限额（RuntimeError 的儿子），能接住，不是崩
  OSError 及儿子       → 外部世界：文件 / 权限 / 网络；带 errno、strerror、filename
  SyntaxError 及儿子   → 编译期，代码一行没跑；带 lineno / offset / text
  上面 11 条里，只有 SyntaxError 那一族不在本节造的（第 7 节第 5 段现场演过），
  其余每一条都是本节当场构造出来的 —— 本节没有一条是背的。
```

---

## 第 09 节 · 自定义异常：继承谁、带不带 payload、能不能 pickle

自定义异常的成本**只有三行**，收益有三条：调用方能按类型分派、能一网打尽、报错消息里能带上下文。
这一节把三行写完，再把三个真实会撞上的问题（多继承、pickle、反面写法）现场演一遍。

只继承、什么都不加，就已经完全能用：

```
=== 1. 只继承、不加东西，就已经完全能用了 ===
  type(exc) = Lazy
  str(exc) = 这行就是消息本体 ｜ repr(exc) = Lazy('这行就是消息本体')
  exc.args = ('这行就是消息本体',) ｜ args 来自 BaseException.__init__ 的原样保存
  用 Exception 能接住它吗 → True ｜ 用 BaseException → True
  → 只要 raise 的是**类或实例**，`except` 按继承链匹配；这一条和第 3 节的家谱是同一套机制。
```

`args` 是 `BaseException.__init__` 原样存的元组，`str(exc)` 是消息本体，`repr(exc)` 带类名 ——
三个各有用处，别混。

带 payload（结构化数据）时，字段要**显式挂**，`args` 不会替你带：

```
=== 2. 带 payload（结构化数据）：字段要显式挂，args 只管消息 ===
  人类看的：str(exc) = 数据库连不上
  机器看的：exc.code = E-DB-503 ｜ exc.args = ('数据库连不上',)
  关键：args 里**没有** code —— super().__init__(message) 只收了第一个参数。
  → 所以给日志用 str(exc)，给代码用 exc.code；两者别混在一句话里再反解析。
```

这是本节最重要的一行判据：`super().__init__(message)` 只收了第一个参数，所以 `args` 里**没有** `code`。
结论：**给日志用 `str(exc)`，给代码用 `exc.code`**，别把两者混进一句话再反解析回来。

多继承是自定义异常最实用的一招 —— 让调用方既能按你的口径接，也能按标准口径接：

```
=== 3. 多继承：让调用方既能接「自家的」也能接「标准的」 ===
  ConfigMissing.__mro__ = ['ConfigMissing', 'AppError', 'LookupError', 'Exception', 'BaseException', 'object']
  except AppError     → 接住了（ConfigMissing: 少一个 section）
  except LookupError  → 接住了（ConfigMissing: 少一个 section）
  except ValueError   → 接不住，漏出去的是 ConfigMissing
  except Exception    → 接住了（ConfigMissing: 少一个 section）
  → 自定义异常**同时**挂一个标准库的父亲，外部代码就能按标准口径接你。
    这就是第 3 节里 OSError/FileNotFoundError 那种关系的用法：一层宽一层窄。
```

`ConfigMissing.__mro__` 那份名单里同时有 `AppError` 和 `LookupError`，所以两种 `except` 都命中；
`except ValueError` 接不住（它不是 `LookupError` 的儿子，第 3 节那棵树在这里第三次生效）。
这层关系和 `OSError` / `FileNotFoundError` 一模一样：一层宽、一层窄。

该继承谁？三条判据（这是本节唯一需要背的东西）：

```
=== 4. 该继承谁？三条判据（这节唯一需要背的东西）===
  ① 想让调用方用 `except OSError` 之类现成口径接住 → 挂对应的标准异常做第二个父亲（第 3 小段）。
  ② 纯自家业务错 → 直接继承 Exception，**不要**继承 BaseException（那会躲过 except Exception，
     和第 3 节 KeyboardInterrupt 一样变成「必须显式接」的东西，除非你就是在做退出信号）。
  ③ 不要继承 str / dict 之类具体容器来「顺便带数据」：异常的 __str__、args、pickle 全靠
     BaseException 那套，多一套父亲会把报错文案搅浑。带数据就显式挂字段（第 2 小段）。
```

第三个问题最容易被忽略：**异常能不能 pickle**。跨进程传异常、或者把异常对象写进缓存/日志文件时，
`pickle` 认的是 `__module__ + __qualname__` 这对**名字**，不是类对象本身：

```
=== 5. 能不能 pickle：跨进程/写日志时会撞上的问题 ===
  Lazy（无自定义 __init__） → 往返成功：Lazy args=('消息',) ｜ code 还在吗 → E-GENERIC
  AppError（自定义 __init__ 收两个参数） → 往返成功：AppError args=('消息',) ｜ code 还在吗 → E-X
  函数里造的类、名字没绑在模块上 → PicklingError: Can't pickle <class '__main__.Hidden'>: it's not found as __main__.Hidden
  同一份 type()，但这次把名字绑到了模块全局 → 往返成功：Transient args=('x',) ｜ code 还在吗 → （这个类没这字段）
```

上面第 3 行那个类是 `type()` 在函数里造的、名字没绑在模块全局上，于是 `PicklingError`；
第 4 行用的是**同一份 `type()` 代码**，只是把名字绑回去了，往返就成功 —— 唯一区别是有没有这个名字能被找回去。
而另一条更常见：自定义 `__init__` 若改了 `super().__init__()` 收的参数个数，往返也可能崩
（重建时只会把 `args` 当位置参数原样再喂一次）。要跨进程就**保持一致**。

三个反面写法（能被接住，但没人会去看）：

```
=== 6. 三个反面：能被接住但没人会去看 ===
  ① raise Exception('出错') —— 类型信息为 0，调用方只能接 Exception 这个大筐，
     于是把 MemoryError 之外的所有错都当同一件事处理（第 3 节那张家谱白画了）。
  ② 自定义异常里再 raise 一个别的类型 —— 链会变长（第 5 节），
     调用方接的是**最后一个**，前面那串只在 traceback 里当背景。
  ③ 消息只写一个 'failed' —— 出错时最需要的是**上下文**：哪个文件、哪个键、什么值。
     对比第 8 节那些内置报错原文：它们都带名字、带类型、带位置。你的报错也该带。
  正解示例（现场跑一遍，看消息长什么样）：
    ConfigMissing (E-CFG-001): 配置段 'db' 不存在；可用的有 ['log', 'server']
```

**收口**：

```
=== 7. 收口 ===
  自定义异常的成本只有三行：类 + 继承 + 需要 payload 时 super().__init__(message) 加一个字段。
  收益有三条：调用方能按类型分派（第 4 节）、能一网打尽（宽父亲）、
     报错消息里能带上下文（上面 load() 那条就是内置报错的水平）。
[退出码 0]
```

---

## 第 10 节 · `with` 是什么协议：手写一遍 `__enter__` / `__exit__`

从这里开始是本章的后半。`with` 没有新语法魔法，它只要求两件事：**类字典里有 `__enter__` 和 `__exit__`**。
第 06 章第 23 节在这里埋了钩子，本节把它补完 —— 先把等价展开写清楚：

```
$ python3 i_with_protocol.py
=== 1. 协议本身：两个 dunder，鸭子类型，不要求继承任何基类 ===
  手写展开 `with Tag('A', log) as a:` 的等价四步：
    ① 先求值 X，拿到上下文管理器对象 = Tag
    ② 调 type(ctx).__enter__(ctx)，as 左边绑的是这个返回值 = '[A(expand)]'
    ③ 执行 with 块体（这里就一行 print）
    ④ 块体正常结束时调 __exit__(None, None, None)，返回值 = None
  日志 = ['enter(A(expand))', 'exit(A(expand)) 异常=None']
  判据：能不能写进 with，只看**类字典里有没有** __enter__ / __exit__（第 06 章第 23 节就是在这里埋的钩子，本节把它补完）；
        没有任何 ABC 或基类要求，所以标准库和你自己写的类在这里是平权的。
```

四步里有两个关键点：`as` 左边绑的是 `__enter__` **的返回值**，`__exit__` 的**返回值**决定异常要不要继续跑。
先说第一个：

```
=== 2. as 左边绑的是 __enter__ 的返回值，不是 X 本身 ===
  as got 拿到的 = '[B]' ｜ 类型 = str
  → 是 __enter__ 里 return 的那个字符串；Tag 实例本身没有绑到任何名字上。
  不写 as 也一样跑（只是拿不到那个返回值）：
    块体里只打了这一行
  日志 = ['enter(C)', 'exit(C) 异常=None']
```

`as got` 拿到的是 `__enter__` 里 `return` 的那个字符串，`Tag` 实例本身没有绑到任何名字上。
不写 `as` 也一样跑（只是拿不到那个返回值）。

第二个关键点是 `__exit__` 的三个参数 —— 它就是 `sys.exc_info()` 的三元组换了个交付方式：

```
=== 3. __exit__ 收到的三个参数：正常退出时全是 None ===
  正常退出的日志 = ['enter(D)', 'exit(D) 异常=None'] ｜ 那条 exit 里打的 异常=None 就是三个 None 的样子
  异常退出时 __exit__ 收到的 =  ValueError | ValueError('块体里炸了') | traceback
  和外面 `except ... as exc` 是同一个对象吗 → True ｜ True
  → 这三样就是 sys.exc_info() 的三元组（第 7 节那份数据，换了个交付方式）；
    所以「正常退出」和「异常退出」在 __exit__ 里要靠 `if exc_type is None` 分岔。
```

正常退出时三个全是 `None`，所以 `__exit__` 里靠 `if exc_type is None` 分岔；
异常退出时接到的和外面 `except ... as exc` 是**同一个对象**（上面那行 True）。

★ 然后是**全章最危险的一行代码** —— `__exit__` 的返回值：

```
=== 4. __exit__ 的返回值 = 要不要咽掉异常（本章最危险的一行代码）===
  ① return True：
    __exit__ 看到异常 = RuntimeError → 我返回 True
     跑到了 with 之后 —— 异常**已经消失**，除了 __exit__ 那行打印，没有任何痕迹。
  ② return None / False：
     外面接住了： 这条会往外跑
  ③ return 一个真值但不是 True（字符串）：
    __exit__ 收到 <class 'RuntimeError'> → 我返回一个非空字符串（不是 True）
     也跑到了 with 之后 → 解释器只看**真假**（真值就咽），不是只认 True。
  判据：__exit__ 的返回值是**布尔语义**。不写值（= None）和 return False 才是照常往上抛；
    return 任何真值 —— 哪怕是 'x'、(0,)、1 这些和 True 毫无关系的东西 —— 都会把异常就地抹掉。
```

看清楚第 ③ 种：返回的不是 `True`，是一个**非空字符串**，异常照样消失。
所以判据要写成布尔语义：`__exit__` 返回**任何真值**（`'x'`、`(0,)`、`1` 都算）就把异常就地抹掉，
只有 `None`（不写）和 `False` 才是「照常往上抛」。这就是 `contextlib.suppress` 的全部原理（第 13 节拆源码验证）。

再一个边界，它和第 11 节那条顺序律是一对孪生坑：

```
=== 5. __enter__ 自己抛异常：__exit__ 根本不会被调用 ===
  接住的报错 = OSError : enter 就失败了，资源还没拿到
  日志 = [] → __exit__ 没跑，块体也没跑。
  → 所以**清理代码不能只写在 __exit__ 里**：资源还没拿到就崩的话，没人替你收尾。
    正解：__enter__ 内部自己 try/回滚；或者用第 14 节 ExitStack，拿到一样登记一样。
```

`__enter__` 自己抛异常时，`__exit__` **根本不会被调用** —— 所以清理代码不能只写在 `__exit__` 里。
正解两条：`__enter__` 内部自己 `try` / 回滚；或者用第 14 节的 `ExitStack`，**拿到一样登记一样**。

最后是 `with` 相对 `finally` 的价值所在 —— 它保证清理在任何出口都执行：

```
=== 6. with 块里的 return / break：__exit__ 照样跑 ===
  函数返回值 = 块体里 return ｜ 日志 = ['enter', 'exit'] → 退出函数前先把 with 收干净（finally 语义）
  循环里 break 时也一样：日志 = ['enter', 'exit']
  （对照：块体正常走完也是 enter → exit 各一次，日志 = ['enter', 'exit'] ）
```

块体里 `return`、循环里 `break`、或者正在往外抛，`__exit__` 都照样跑（最后一行是正常走完的对照）。

**收口**（把 `with` 记成一句话）：

```
=== 7. 收口（把 with 记成一句话）===
  with X() as a: body  ≡  tmp = X(); a = tmp.__enter__();
                        try: body; finally: tmp.__exit__(*sys.exc_info())
  它保证三件事：① 清理必然执行（return / break / 异常都跑）；② 块体崩了也释放；
  ③ 拿 __exit__ 的返回值决定异常要不要继续往上跑。
  此刻（块体外）sys.exc_info() = (None, None, None)
```

那个等价展开里有个细节值得抄走：`__exit__(*sys.exc_info())` —— 第 10 节和前面第 7 节在这儿接上了。

---

## 第 11 节 · 多个 `with`：逗号堆、嵌套、括号写法，和那条后进先出的顺序律

一行写两个 `with` 是不是「一起打开、一起关」？不是 —— 它等价于逐层嵌套，
而嵌套的顺序律就是**栈**。

```
$ python3 j_with_more.py
=== 1. 一行写两个：enter 从左到右，exit 从右到左（后进先出）===
   enter(A)
   enter(B)
     块体里 a=A b=B
   exit(B)
   exit(A)
  → 顺序律和栈一样：最后一个拿到的最先还。
```

把「一行写」换成「逐层嵌套写」，产出的日志**逐字节相同**：

```
=== 2. 嵌套写法和一行写法逐字节等价 ===
  嵌套 = ['enter(A)', 'enter(B)', '  块体', 'exit(B)', 'exit(A)']
  一行 = ['enter(A)', 'enter(B)', '  块体', 'exit(B)', 'exit(A)']
  两者完全一样吗 → True
  三个以上同理：with Res('A'), Res('B'), Res('C'): 的 exit 顺序是 C、B、A。
  实测 = ['enter(A)', 'enter(B)', 'enter(C)', 'exit(C)', 'exit(B)', 'exit(A)']
```

三个以上同理，`exit` 顺序是 C、B、A。所以记一句话就够：**enter 左→右，exit 右→左**。

本机 3.14 还支持括号写法（长列表好换行，语义与逗号形式完全相同）：

```
=== 3. 括号写法（本机 3.14 支持；长列表好换行，语义与逗号形式相同）===
   enter(A) ｜ 括号形式和上一行那种逗号形式产出的日志 = ['enter(A)', 'enter(B)', 'enter(C)', '  块体里 a=A b=B c=C', 'exit(C)', 'exit(B)', 'exit(A)']
  注意括号里**推荐带 as**。不带 as 的 `with (Res('A'), Res('B')):` 本机 3.14 也照跑（下面）：
   ['enter(A)', 'enter(B)', '  带括号、不带 as，也当成两个上下文管理器', 'exit(B)', 'exit(A)']
  → 但不带 as 的那条**别写**。通行的说法是老解释器（3.8 及以前）会把那对括号当成一个元组表达式，
    元组没有 __enter__ 直接 TypeError —— 本机只有 3.14，没有老解释器，**这条未验证**，别当事实引用。
    稳妥做法就两条：要么带 as，要么干脆不加括号；括号只给「太长要换行」这一种场景用。
```

括号里**推荐带 `as`**。不带 `as` 的写法本机也照跑（上面第 4 行），但别写：
通行的说法是老解释器（3.8 及以前）会把那对括号当成一个元组表达式、元组没有 `__enter__` 直接 `TypeError` ——
**本机只有 3.14，没有老解释器，这条未验证**，别当事实引用（本章末尾「没演 / 没验证」里也记了这条）。

真正的两个边界问题。第一个：**第二个 `__enter__` 就失败了，第一个的 `__exit__` 还跑吗？**

```
=== 4. 边界一：第二个 __enter__ 就失败，第一个的 __exit__ 还跑吗？===
   enter(OK)
   enter(BAD) **失败**
   exit(OK) ←带异常 OSError
     外面接住：BAD 打不开
  → 跑。第一个已经成功 enter 了，解释器有责任替它还；
    而且看那行的尾巴：exit(OK) 收到的是**那个 OSError**（带异常 OSError），
    也就是「enter 失败」这件事会以正常异常的形式穿过前面每一层 —— 层可以拿它决定要不要咽。
  失败的那个（BAD）自己的 __exit__ 没跑：它连enter都没成功，日志里没有 exit(BAD)。
  对照：同一件事，如果**内层自己 try 住了**，外层的 exit 就只看到正常退出 = ['enter(SPY)', 'enter(BAD2) **失败**', '  内层自己把异常接住了', 'exit(SPY)']
    → __exit__ 收到的 exc_type 完全取决于「异常有没有穿出这一层的块体」，
      跟块体里到底出过事无关。这是第 16 节那类静默漏 cleanup 的根源之一。
```

跑。第一个已经成功 enter 了，解释器有责任替它还；而且注意那行尾巴 ——
`exit(OK)` 收到的是**那个 OSError**，也就是「enter 失败」这件事会以正常异常的形式穿过前面每一层。
失败那个（BAD）自己的 `__exit__` 没跑：它连 enter 都没成功。

对照第 3 行那个例子更能说明问题：**同一件事，如果内层自己 `try` 住了，外层的 `exit` 就只看到正常退出**。
`__exit__` 收到的 `exc_type` 完全取决于「异常有没有穿出这一层的块体」，跟块体里到底出过事无关 ——
这是第 16 节那类静默漏 cleanup 的根源之一。

第二个边界：**块体里 raise，每层的 `__exit__` 都收到异常三元组**。

```
=== 5. 边界二：块体里 raise，每层的 __exit__ 都收到异常三元组 ===
   enter(X)
   enter(Y)
   exit(Y) ←带异常 ValueError
   exit(X) ←带异常 ValueError
     最外面接住了
  → 两层都标了「←带异常」：异常会**依次穿过**每个 __exit__，每层都能看到它。
    哪一层返回真值，就在哪一层被截住（第 10 节），后面的层根本不会被调用。
```

异常**依次穿过**每一层，每层都能看到它；哪一层返回真值就在哪一层被截住（第 10 节），后面的层根本不会被调用：

```
=== 6. 一层咽掉：中间层 return True，最内层照常、最外层不再收到 ===
   enter(OUT)
   enter(MID)
   enter(IN)
   exit(IN) ←带异常 ValueError
   exit(MID) ←带异常 ValueError
   exit(OUT)
     出了 with 还在正常流程里（说明被 MID 咽了）
```

上面这份日志里 `exit(OUT)` 那行**没有「← 带异常」的尾巴** —— 因为异常已经被中间层 MID 咽掉了。
这就是「咽异常」在多层 `with` 里特别难查的原因：现场只剩一条什么都没打的静默。

补一条经常被误传的性质：`with` **不产生新作用域**。

```
=== 7. with 不产生新作用域（对照第 04 章第 16 节：推导式才是开了新作用域的那个）===
  块体外还能读到 only_here = 42 ｜ vars() 里有没有它 → True
  → 会开新作用域的只有函数（第 05 章的 LEGB）和推导式（第 04 章第 16 节实测过变量不泄漏）；
    with、if、for 都和这块代码本身的层级无关，不开。类体也开一个作用域，但本手册没演它。
```

会开新作用域的只有函数（第 05 章 LEGB）和推导式（第 04 章第 16 节实测过变量不泄漏）；
`with`、`if`、`for` 都和这块代码本身的层级无关，不开。

**收口**：

```
=== 8. 收口 ===
  ① 一行多个 with ≡ 逐层嵌套；enter 左→右，exit 右→左（栈）。
  ② 第 k 个 __enter__ 失败：前 k-1 个都会被还，失败那个自己的 __exit__ 不跑（第 10 节第 5 段）。
  ③ 异常从内往外穿过每一层 __exit__，遇到第一个返回真值的层就停 ——
     这就是为什么「咽异常」这件事在多层 with 里特别难查：现场只剩一条日志都没有的静默。
  ④ 资源数量**不定**时别用逗号堆（要写死个数），那是第 14 节 ExitStack 的地盘。
```

第 ④ 条正好把本节交给第 14 节：资源个数**运行时才知道**的话，逗号堆要写死个数，那是 `ExitStack` 的地盘。

---

## 第 12 节 · `@contextmanager`：一个 `yield` 把函数劈成 enter 和 exit

手写一个类要实现两个方法、还得管状态，太累。标准库给了个装饰器：**`@contextmanager` 把一个生成器函数
变成上下文管理器工厂**，函数体被那一个 `yield` 劈成三截。

```
$ python3 k_contextlib.py
=== 1. 最小形状：函数体被 yield 劈成三截 ===
   enter(G)
     块体里 as 拿到 = <G>
   exit(G)
  → 对比第 10 节手写的类：__enter__ 的 return 变成了 yield 的值，
    __exit__ 的函数体变成了 yield 之后的那几行。**少写一个类、少写两个 def。**
```

对照第 10 节手写的类：`__enter__` 的 `return` 变成了 `yield` 的值，`__exit__` 的函数体变成了 `yield` 之后的那几行。
**少写一个类、少写两个 `def`。**

但「裸 yield」有个致命缺口 —— 块体崩了，`yield` 之后那行**根本不会被执行**：

```
=== 2. 裸 yield（不加 try）：块体崩了就没人收尾 ===
  日志 = ['enter', '  块体', '  外面接住'] → 没有 exit：yield 之后那行被整个跳过了。
  换成 try/finally 包 yield：日志 = ['enter', '  块体', 'exit', '  外面接住']
  判据：**@contextmanager 里 yield 之后写的任何清理，都必须包在 try/finally 里。**
    标准库自己的实现就是这么写的，所以口诀是「yield 前后各一段，中间用 try 夹住」。
```

这就是本节的第一条纪律，而且它不是口味问题：**`@contextmanager` 里 yield 之后写的任何清理，都必须包在 `try/finally` 里。**
标准库自己的实现就是这么写的，所以口诀是「yield 前后各一段，中间用 try 夹住」。

第二个特性很反直觉但极其有用：**块体的异常是被「扔回 `yield` 那一行」的**，
所以你在生成器里写 `except` 就能接住它：

```
=== 3. 异常是被「扔回 yield 那一行」的：except 写在生成器里就能接 ===
   enter
     yield 这一行收到了 ValueError: 被生成器接住了 → 我选择咽掉
   exit
     块体外：异常没出来（被咽了）
  → 这就是 contextlib.suppress 的实现原理（第 13 节现场拆）。
    生成器把异常接住并正常结束 → contextlib 认为「这件事你已经处理完了」→ 异常就地消失；
    第 13 节会把 suppress 的源码打出来，它干的就是这一件事。
   ['enter', 'exit', '  外面接住了：生成器只 except 了 ValueError'] → 接不住的类型照样往上跑，日志顺序仍是 enter → exit（finally 兜住）。
```

生成器把异常接住并正常结束 → `contextlib` 认为「这件事你已经处理完了」→ 异常就地消失。
**这就是 `suppress` 的实现原理**，第 13 节会把它在本机的源码整个打出来验证。
上面最后一行是对照：接不住的类型照样往上跑，而日志顺序仍是 enter → exit（因为 `finally` 兜住了）。

第三个坑最容易被踩：同一个 `@contextmanager` 的结果**只能用一次**。

```
=== 4. 同一个 @contextmanager 的结果只能用一次（生成器已经耗尽）===
  type(cm) = _GeneratorContextManager
  第一次用完，把**同一个对象**再放进 with：
      → AttributeError: '_GeneratorContextManager' object has no attribute 'args'
  → 注意原文是 **AttributeError**，不是 RuntimeError：第一次 __exit__ 时 contextlib
    把它内部那几个属性 del 掉了（源码里那句 `del self.args, self.kwds, self.func`），
    所以第二次进来撞的是「属性不存在」。**报错类型和你想的往往不一样，别看猜。**
  正确做法：每次 with 都**重新调用**那个工厂函数（gate('x')），别存对象复用。
  对照：第 10 节手写的类没这个限制（__enter__ 是普通方法，可以反复调）——
    差别就在 @contextmanager 的底子是生成器，生成器只能跑完一遍。
```

要看清原文类型：是 **`AttributeError`**，不是 `RuntimeError`。原因是第一次 `__exit__` 时 contextlib
把它内部那几个属性 `del` 掉了（源码里那句 `del self.args, self.kwds, self.func`），
所以第二次进来撞的是「属性不存在」。**报错类型和你猜的往往不一样，别看猜。**
正确做法：每次 `with` 都**重新调用**那个工厂函数（`gate('x')`），别存对象复用。
对照一下：第 10 节手写的类没这个限制（`__enter__` 是普通方法，可以反复调）——
差别就在 `@contextmanager` 的底子是**生成器**，生成器只能跑完一遍。

第四条：「忘了 yield」和「yield 两次」是两条完全不同的错：

```
=== 5. 「忘了 yield」和「yield 两次」：两条原文完全不同的错 ===
  没 yield 的 @contextmanager → TypeError: 'NoneType' object is not an iterator
  yield 了两次 → RuntimeError: generator didn't stop
  → 第一条的因果链：函数体里**一个 yield 都没有**，它就根本不是生成器函数，
    no_yield() 返回 None，contextlib 拿去 next(None) → TypeError。报错不提「yield」半个字，
    但病因就是它。判据：@contextmanager 装饰的函数**必须恰好 yield 一次**：
    前置 = __enter__，yield 的值 = as 的东西，后置 = __exit__。
```

第一条的因果链值得慢慢读：函数体里**一个 yield 都没有**，它就根本不是生成器函数，
`no_yield()` 返回 `None`，contextlib 拿去 `next(None)` → `TypeError`。
报错不提「yield」半个字，但病因就是它。所以判据是：**必须恰好 yield 一次** ——
前置 = `__enter__`，yield 的值 = `as` 的东西，后置 = `__exit__`。

**收口**：

```
=== 6. 收口 ===
  @contextmanager 做的事：把「生成器函数」包成一个「返回 __enter__/__exit__ 对象」的工厂，
    它本身是第 05 章第 23 节那个装饰器 —— f = contextmanager(f)，名字和 doc 由 wraps 保住。
  三条纪律：① yield 之后必须 try/finally；② 一次 with 一个新对象；③ 想咽异常就在 except 里接，
    接了就等于咽（所以**默认别接**，让它往上跑，除非你就是 suppress）。
```

那句「它本身是第 05 章第 23 节那个装饰器」是关键：`f = contextmanager(f)`，
`@contextmanager` 没有任何特权，它就是第 05 章那套装饰器语义的一个用户。

---

## 第 13 节 · `contextlib` 的四个现成件：`suppress` / `closing` / `nullcontext` / `redirect_stdout`

先把「这个模块里到底有多少公共名字」扫一眼，有个数就行，不用背：

```
=== 引子 ｜ 先看这个模块里到底有多少公共名字（别背，扫一眼有个数）===
   ['AbstractAsyncContextManager', 'AbstractContextManager', 'AsyncContextDecorator', 'AsyncExitStack', 'ContextDecorator', 'ExitStack', 'GenericAlias', 'MethodType', 'abc', 'aclosing', 'asynccontextmanager', 'chdir', 'closing', 'contextmanager', 'deque', 'nullcontext', 'os', 'redirect_stderr', 'redirect_stdout', 'suppress', 'sys', 'wraps']
  共 22 个；本节只演 4 个最常用的，第 14 节演 ExitStack，其余列在 README 的「没演」。
```

22 个名字，本节演最常用的 4 个，第 14 节单独演 `ExitStack`，其余全列在本章末尾的「没演」里。

### `suppress(*类型)`：把指定类型的异常就地扔掉

```
=== 1. suppress(*类型)：把指定类型的异常就地扔掉 ===
  with suppress(ValueError): int('四十二') → 什么都没发生（这正是它的用途）
  多个类型可以并列（元组语义，第 4 节）：这行照常打印
  ★代价现场：把上面那个 NameError 换成 suppress(NameError) 会发生什么？
  → 打错了变量名，程序**安静地**跳过了整块，然后继续跑。没有任何输出告诉你少做了事。
    suppress 的语义就是「我确定这类异常在这里不重要」；用它包住一段有副作用的代码 = 赌。
```

前两条是它的正当用途；★ 后面那条是它的代价现场：**打错了变量名，程序安静地跳过了整块，
然后继续跑**，没有任何输出告诉你少做了事。用它包住一段有副作用的代码 = 赌。

那它到底是什么做的？`inspect` 读本机标准库源码，一个字都没改：

```
  拆开看它到底做了什么（inspect 读本机标准库源码，一个字都没改）：
    class suppress(AbstractContextManager):
        """Context manager to suppress specified exceptions

        After the exception is suppressed, execution proceeds with the next
        statement following the with statement.

             with suppress(FileNotFoundError):
                 os.remove(somefile)
             # Execution still resumes here if the file was already removed
        """

        def __init__(self, *exceptions):
            self._exceptions = exceptions

        def __enter__(self):
            pass

        def __exit__(self, exctype, excinst, exctb):
            # Unlike isinstance and issubclass, CPython exception handling
            # currently only looks at the concrete type hierarchy (ignoring
            # the instance and subclass checking hooks). While Guido considers
            # that a bug rather than a feature, it's a fairly hard one to fix
            # due to various internal implementation details. suppress provides
            # the simpler issubclass based semantics, rather than trying to
            # exactly reproduce the limitations of the CPython interpreter.
            #
            # See http://bugs.python.org/issue12029 for more details
            if exctype is None:
                return
            if issubclass(exctype, self._exceptions):
                return True
            if issubclass(exctype, BaseExceptionGroup):
                match, rest = excinst.split(self._exceptions)
                if rest is None:
                    return True
                raise rest
            return False
```

```
  → 果然就是第 12 节那个形状：try/yield/except 里 pass。**suppress 不是关键字，是 6 行 Python。**
```

**`suppress` 不是关键字，是 6 行 Python**，而且它就是第 12 节那个形状（`try` / `yield` / `except` 里 `pass`）。
注意源码里那句注释：`suppress` 用的是 `issubclass` 语义，比解释器自己的匹配规则更简单直接。

第二个问题决定它能不能上生产 —— `suppress` 会连 Ctrl-C 一起咽吗？

```
=== 2. suppress 会连 KeyboardInterrupt 一起咽吗？（这条决定它能不能上生产）===
  issubclass(KeyboardInterrupt, Exception) → False
  issubclass(KeyboardInterrupt, BaseException) → True
  所以 suppress(Exception) **挡不住** Ctrl-C（好），但 suppress(BaseException) 挡得住（坏）：
  → 这行能打到，说明那个 KeyboardInterrupt 已经被吞掉；
    真实场景里这会让 Ctrl-C 按下去没反应，第 3 节说过的那道「例外」被你亲手封死了。
  判据：**永远不要写 suppress(BaseException)**；要咽就咽具体的类型。
```

好：`KeyboardInterrupt` 不是 `Exception` 的儿子，所以 `suppress(Exception)` **挡不住** Ctrl-C。
坏：`suppress(BaseException)` 挡得住 —— 上面那行能打到，就说明那个 `KeyboardInterrupt` 已经被吞掉了。
真实场景里这会让 Ctrl-C 按下去没反应，把第 3 节说过的那道「例外」亲手封死。
判据：**永远不要写 `suppress(BaseException)`**，要咽就咽具体的类型。

### `closing(obj)`：给「只有 `close()`、没有 `__exit__`」的老对象补一个 `with`

```
=== 3. closing(obj)：给「只有 close()、没有 __exit__」的老对象补一个 with ===
  Legacy 有 __exit__ 吗 → False ｜ 有 close 吗 → True
  直接 with obj: 会怎样：
      → TypeError: 'Legacy' object does not support the context manager protocol (missed __exit__ method)
  块体里在用这个对象（as 拿不到东西，closing 返回的就是 obj 本身）
    close() 被调用了
  出了 with：obj.closed = True → closing 替你调了 .close()
  closing(obj) ≡ 一个 __exit__ 里写 obj.close() 的薄壳；标准库用它包了 urllib 的响应之类。
```

`with obj:` 直接撞 `TypeError`（原文里连「missed `__exit__` method」都告诉你了），
`closing` 就是 `__exit__` 里写 `obj.close()` 的薄壳 —— 用它包 urllib 的响应之类。

### `nullcontext()`：占位用的 `with`，让两条代码路径形状一样

```
=== 4. nullcontext()：占位用的 with，让两条代码路径形状一样 ===
  nullcontext() 的 as 绑到 = None
  nullcontext(x) 的 as 绑到 = '塞进去的东西原样出来'
  典型用法：有条件才加锁/才开文件，但块体只想写一遍：
    块体完全不知道上面到底有没有真文件 —— 这就是它的价值（第 15 节还会用它拼 EAFP）
  走过 flag=True 之后 lock.deep = 0
  走过 flag=False 之后 lock.deep = 0
  走过 flag=True 之后 lock.deep = 0
  → 不写 nullcontext 就得把块体复制两遍，或者写 `if flag: ... else: ...` 两份分支。
```

典型用法是「有条件才真的加锁 / 开文件，但块体只想写一遍」：
`nullcontext()` 的 `as` 绑到 `None`，`nullcontext(x)` 把 `x` 原样交出去。
不写它就得把块体复制两遍，或者写两份 `if/else` 分支。

### `redirect_stdout` / `redirect_stderr`：把 `print` 的去处换掉

```
=== 5. redirect_stdout / redirect_stderr：把 print 的去处换掉 ===
  这行在真正的 stdout 上
  回到真 stdout，buf 里抓到了： '  这行进 buf\n  这行也是（sys.stdout 被整个换了）\n'
  redirect_stdout 的返回值 = None ｜ 它改的是**进程全局**的 sys.stdout
  stderr 同理，而且 traceback 默认就是走 stderr：
  被 redirect 走的 stderr 最后一行 = 'ZeroDivisionError: division by zero'
  buf 里一共抓到 5 行（抬头 + 帧 + caret + 消息，和第 7 节那份一样）
  ★代价：这是**全局**替换。多线程 / async 里用它会串台（别的线程也被打进你的 buf）。
    多线程场景的正解是把 stream 当参数传进去（print(x, file=f)），不是 redirect。
```

要点在最后两条：它的返回值是 `None`，它改的是**进程全局**的 `sys.stdout`；
`stderr` 同理，而 traceback 默认就是走 stderr（上面抓到 5 行，正是第 7 节那份形状）。
★ 代价：**这是全局替换**，多线程 / async 里用它会串台（别的线程也被打进你的 buf）。
正解是把 stream 当参数传进去（`print(x, file=f)`），不是 redirect。

把四件东西按「谁负责收尾」排一遍，顺手把各自的代价钉在下一行：

```
=== 6. 把四件东西按「谁负责收尾」排一遍 ===
  suppress(类型)          → 没有收尾，只咽指定类型的异常
                          代价：拼错名/漏逻辑时静默；BaseException 版会封死 Ctrl-C
  closing(obj)          → 调 obj.close()
                          代价：对象本来该自己实现协议；close 失败会覆盖原异常（第 16 节）
  nullcontext(x)        → 什么都不做，as 拿到 x 或 None
                          代价：只在「有条件才真的要开」时才值得
  redirect_stdout(s)    → 退出时把 sys.stdout 换回来
                          代价：进程全局，多线程/async 会串台

=== 7. 收口 ===
  这四个在源码里全是 @contextmanager 写的（第 12 节那套形状），不是 C 层的特殊待遇。
  选型判据：要「忽略某类错」→ suppress；要「给老对象补协议」→ closing；
    要「让分支两端形状一致」→ nullcontext；要「抓 print 的输出」→ redirect_stdout（单线程）。
  数量不固定 / 运行时才知道有几个资源 → 别用这四个凑，用第 14 节 ExitStack。
```

---

## 第 14 节 · `ExitStack`：资源个数运行时才知道

前三节（`with` 协议、逗号堆、`@contextmanager`）都有一个隐含前提：**你要在代码里写清楚有几个资源**。
真实代码常常是「拿到多少算多少」：一个配置里列了 3 个后端就连 3 个，列了 7 个就连 7 个。
逗号堆写不死，递归写又不像人话 —— 这就是 `ExitStack` 的唯一职责。

```
$ python3 m_exitstack.py
=== 1. 为什么需要它：资源个数写在代码里就输了 ===
  资源清单 = ['db', 'cache', 'log', 'metrics'] ｜ 长度 = 4
  用逗号堆 with 就得写死个数（第 11 节）；用递归写又不像人话。ExitStack 的写法：
   enter(db)
   enter(cache)
   enter(log)
   enter(metrics)
     块体（业务代码在这里跑）
   exit(metrics)
   exit(log)
   exit(cache)
   exit(db)
  → 后进先出：enter 是 db→cache→log→metrics，exit 反过来。和第 11 节那条顺序律一模一样。
    enter_context(x) 干的事就是「替你把 with x 往后写」，退出时按登记的逆序调用。
```

`enter_context(x)` 干的事就是「替你把 `with x` 往后写」，退出时按登记的**逆序**调用 ——
和第 11 节那条顺序律一模一样。

第二个入口最省事：**只要一行收尾代码，不想为它写一个类**。

```
=== 2. 只要一行收尾代码，不想为它写一个类：callback() ===
   ['  块体', 'exit-2', 'exit-1']
  → callback(fn, *args, **kw)：退出时调 fn(*args)。它是**栈**，所以后登记的先跑。
    注意上面那两行参数是在登记时就定好的 —— 块体里再改局部变量不影响已登记的调用。
  对照（登记的参数不受块体影响）： ["['初始']"]
  想要「退出时才求值」就包一个无参 lambda：stack.callback(lambda: LOG.append(str(late)))
```

`callback(fn, *args, **kw)`：退出时调 `fn(*args)`。它是**栈**，所以后登记的先跑。
中间那两行「对照」有个容易被忽略的性质：**登记的参数是登记那一刻就绑死的**，
块体里再改局部变量不影响已登记的调用 —— 想要「退出时才求值」，就包一个无参 lambda（脚本最后一行演示了）。

第三个问题：块体崩了，登记过的收尾会不会漏？

```
  改成 lambda 之后： ["['初始', '块体里改过']"]

=== 3. 收尾顺序与异常：块体崩了，登记过的**全部**倒着还 ===
   enter(A)
   enter(B)
   callback-C
   exit(B) ←带ValueError
   exit(A) ←带ValueError
     外面接住
  → 每个 exit 都带上了那个 ValueError（因为异常正在往外跑）。
    这比 finally 好在哪里： finally 只能写在一层里，ExitStack 能跨函数收集（下面第 5 段）。
```

**全部**倒着还，而且每个 `exit` 都带上了那个 ValueError（因为异常正在往外跑）。
这比 `finally` 好在哪：`finally` 只能写在一层里，`ExitStack` 能跨函数收集（第 5 段）。

★ 第四个问题最重要：**收尾代码自己也抛，两个异常怎么共存** —— 第 5 节那条链在这里第二次咬人。

```
=== 4. 收尾时自己也抛：两个异常怎么共存（第 5 节的链接在这里第二次咬人）===
  ① 块体正常、收尾抛：
     外面看到的是 OSError : 收尾代码自己炸了 （块体其实成功了，报错却是收尾那条）
  ② 块体先抛 ValueError、收尾又抛 OSError：
     外面看到的是 OSError : 收尾代码自己炸了
     __context__ = ValueError ｜ __cause__ = None
  → **原始异常被丢掉，只剩收尾那条**，原始故障降级成 __context__ 里的背景。
    这是所有 finally / __exit__ / callback 共同的坑，不是 ExitStack 独有的；
    判据：看到报错时先翻 __context__，「原始故障」常常藏在那儿（第 5 节那条判据的复用）。
  → 怎么防：收尾代码自己包 try，或者改用 push()。这两个入口的区别就写在 docstring 里：
     callback 的 docstring： Cannot suppress exceptions.
     push 的 docstring     ： Can suppress exceptions the same way __exit__ method can.
      ['  收尾自己把异常吃了（块体的异常因此保住）', '  外面接住：ValueError: 块体的真故障'] → 收尾不抛，块体那条 ValueError 才是你看到的报错（这就是想要的行为）。
      ['  外面看到的是 OSError: 收尾里的事没办成 ｜ __context__ = ValueError'] → 收尾一抛，块体的故障就被顶替了；callback **没有**任何吞异常的开关。
```

结论是残酷的：**原始异常被丢掉，只剩收尾那条**，原始故障降级成 `__context__` 里的背景。
这不是 `ExitStack` 独有的锅，所有 `finally` / `__exit__` / `callback` 都一样。
判据：看到报错时**先翻 `__context__`**，「原始故障」常常藏在那儿。
防御手段写在 docstring 里（脚本把两份原文打出来了）：`callback` 是 **Cannot suppress exceptions**，
`push` 是 **Can suppress exceptions the same way `__exit__` can** ——
也就是说 `push` 收的是 `__exit__` 签名的函数，返回真值就能咽掉异常，从而保住块体那条。别记反。

第五个入口是 `pop_all()`，它把「谁负责关」整个转手出去：

```
      ['  push 进去的函数看到 ValueError，返回真值 = 咽掉'] → push 收的是 __exit__ 签名的函数，返回值真值就能咽异常。

=== 5. pop_all()：把「谁负责关」整个转手出去（经典：函数返回一个还在用的资源）===
  pop_all 之前 inner 手里有 1 个收尾
  pop_all 之后：inner 剩 0 ｜ 接手的那个 stack 有 1 个
   ['enter(X)', '  块体', 'exit(X)'] → 谁 with 这个 stack，谁负责还；和「资源是谁创建的」脱钩了。
  函数已经返回，日志 = ['enter(conn)', 'enter(cursor)'] → 资源**还开着**（登记项搬进了 owned，函数栈帧退了也不放）
   ['enter(conn)', 'enter(cursor)', '  使用之中（这才是调用方该写的 with）', 'exit(cursor)', 'exit(conn)']
  忘 pop_all 时： ['enter(only)', 'exit(only)'] → return 那一刻 with 就退出，资源在调用方拿到之前就还掉了
  → pop_all 的意义：**收尾责任是个可以传手的值**。第 18 节写临时文件就用这一手：
    函数返回文件名，「负责删」这一项交给调用方的 stack。
```

`pop_all` 之后原 stack 手里空了，接手的那个有了；函数已经返回，但资源**还开着** ——
登记项搬进了 `owned`，函数栈帧退了也不放。忘 `pop_all` 的对照组在最后一行：`return` 那一刻 `with` 就退出，
资源在调用方拿到之前就还掉了。所以：**收尾责任是个可以传手的值**。
第 18 节写临时文件就用这一手 —— 函数返回文件名，「负责删」这一项交给调用方的 stack。

最后一个玩法是把「要不要容错」变成一个**参数**：

```
=== 6. 拿 ExitStack 拼一个「能吞也能放」的可组合块 ===
  tolerant=False → 抛出来了：坏输入
  tolerant=True → None
  → 这就是「要不要容错」变成一个**参数**、而不是把块体复制两份的写法。
    注意 tolerant=True 那行拿到的是 **None** 不是「跑完了」：suppress 咽掉异常的位置在 with 退出时，
    也就是 job() 之后的 `return` 那行**根本没跑到**（第 12 节：yield 之后的代码不执行）。
```

`tolerant=True` 那行拿到的是 **`None`** 不是「跑完了」：`suppress` 咽异常的位置在 `with` 退出时，
`job()` 之后那句 `return` **根本没跑到**（第 12 节那条：yield 之后的代码不执行）。
代价是读的人得知道你往里塞的是哪个上下文管理器 —— 只在真要开关时用。

**收口**：

```
    代价：读的人得知道你往里塞的是哪个上下文管理器 —— 只在真要开关时用。

=== 7. 收口 ===
  ① 资源个数写不死 → ExitStack + enter_context；只要一行收尾 → callback（参数登记时就绑死）。
  ② 退出顺序永远是登记顺序的**逆序**，和逗号堆 with / 嵌套 with 同一条律。
  ③ 收尾代码抛异常会**盖住**块体的异常，原始故障只在 __context__ 里 —— 这不是 ExitStack 的锅，
     finally / __exit__ 全一样；所以收尾要么不抛，要么自己 suppress 掉（第 16 节专讲这个坑）。
```

---

## 第 15 节 · 计时：一次 `try` 值多少钱、一次真抛值多少钱

「包一层 `try` 会不会变慢」是这门语言最经典的口水题。本节不谈理论，直接量：
同一份代码，命中 / 未命中各跑 100 万次，拆开量 raise / 构造 / 长消息 / 深栈，再量存 traceback 的内存代价。
**所有 µs 数字每跑必抖，只当量级看；本节唯一稳的结论是倍率的大小顺序。**

```
$ python3 n_cost.py
=== 1. 什么都不抛的 try：贵不贵？ ===
  命中时（不进 except 分支）跑 1,000,000 次：
    EAFP  try/except = 0.025 µs/次
    LBYL  if k in d  = 0.031 µs/次
    哨兵  d.get(k, MISSING) = 0.029 µs/次
```

第一条结论就推翻了很多人的直觉：**不进 `except` 的 `try`不但不要钱，还比 LBYL 快** ——
因为 `if k in d:` 之后还要 `d[k]`，每一次都查了两遍哈希。

一旦真的抛，就是另一个世界：

```
=== 2. 一旦真的抛：同一个函数，未命中时完全是另一个世界 ===
  未命中时跑 1,000,000 次：
    EAFP（每次都抛+接）  = 0.093 µs/次
    LBYL（if 判掉，不抛）= 0.023 µs/次
  → EAFP / LBYL = 4.04 倍；
    同一个 EAFP：命中 0.025 → 未命中 0.093，差 3.7 倍。
```

同一个函数，`EAFP` 从 0.026 µs 跳到 0.108 µs。结论要说准：
**拿异常当普通控制流**（每次都走 `except`）是真的慢；**拿异常报真正的失败**不慢，因为失败本来就该是少数事件。

贵的到底是哪件事？拆开量：

```
=== 3. 贵的三件事分别是什么（拆开量，跑 100,000 次）===
  ① raise + 立刻接住       ：0.084 µs/次
  ② 只构造实例、不抛        ：0.032 µs/次  ← 纯构造其实很便宜
  ③ raise 带 500 字长消息    ：0.085 µs/次
  ④ 从 50 层深的调用里 raise ：2.873 µs/次  ← 这一条最贵
  比值：④/① = 34.1 ｜ ③/① = 1.01 ｜ ①/② = 2.66
  → 只有 ④ 是真的贵（差几十倍）：**栈越深越贵**，因为要给每一帧建 traceback 记录。
    ③ 和 ① 的比值是 1.01，落在噪声里 —— 说明**消息多长几乎不影响**：
    raise 只把参数原样塞进 exc.args，字符串本身不格式化、不裁剪。格式化发生在你打印它的时候（第 5 段）。
    ①/② 说的是：抛 + 接比「只造对象」贵好几倍，但绝对值仍是 0.1 µs 级 ——
      真正的问题不是它贵，是它**每次都要付**，所以别把它放进热循环当普通分支用。
  对照：空迭代器上走一遍 for（内部靠 StopIteration 收尾）= 0.010 µs/次，
        手动 next 到底再接住                        = 0.087 µs/次
    → for 把那个异常**在解释器内部**接掉了，不建 traceback、不格式化，所以便宜得多；
      自己手写 next() 循环时才会真的付这份钱（`__iter__`/`__next__` 那套本手册没演，
```

只有第 ④ 条（50 层深的调用里 raise）是真的贵，因为要给每一帧建 traceback 记录。
第 ③ 条（500 字长消息）比值落在噪声里 —— 说明**消息多长几乎不影响**：
`raise` 只把参数原样塞进 `exc.args`，字符串本身不格式化、不裁剪，格式化发生在你打印它的时候（第 5 段）。
最后那组对照很实用：`for` 把 `StopIteration` **在解释器内部**接掉了，不建 traceback、不格式化，
所以便宜近十倍；只有手写 `next()` 循环时才真的付这份钱。

还有两个不那么常被考虑的代价。第一，`traceback` 顺手把**栈帧和局部变量一起攥住**了：

```
      第 04 章第 17 节只到生成器和惰性为止）。

=== 4. traceback 顺手把栈帧和局部变量一起攥住了（内存代价）===
  帧链一共 2 层；最深那层的局部变量键 = ['local_blob']
  里面那份大对象还活着吗 → True
  → 把一个 exception 对象长期存着（塞进全局、存进缓存、挂到返回值上）=
    把出事那一刻**整层局部变量**一起留着（帧对象攥着 f_locals，f_locals 攥着你的数据）。
```

把一个 exception 对象长期存着（塞进全局、存进缓存、挂到返回值上）= 把出事那一刻**整层局部变量**一起留着。
正解：只存 `type(exc).__name__` / `str(exc)` / `exc.args`，需要行号就存 `extract_tb` 的结果。

第二，把整份 traceback 写进日志值多少钱：

```
    正解：只存 type(exc).__name__ / str(exc) / exc.args，需要行号就存 extract_tb 的结果。

=== 5. 把整份 traceback 写进日志，比只存 str 贵多少（跑 100,000 次）===
  只 str(exc)：0.217 µs/次 ｜ 整份 format_exc()：86.499 µs/次 ｜ 比值 = 399
  → 差**两个数量级**：format 一份 traceback 要读源码文件、排版每一帧。
    但故障是少数事件，这点钱**该花**（第 16 节第 3 段：少了帧你查不到是哪一行）。
```

差**两个数量级** —— 排版一份 traceback 要读源码文件、逐帧格式化。
但故障是少数事件，这点钱**该花**（第 16 节第 3 段：少了帧你查不到是哪一行）。
不该做的是在**正常路径**上每次成功都 format 一份。

format 出来的东西长什么样（跑在子进程里，免得把本机绝对路径打进这份输出）：

```
    不该做的是在**正常路径**上每次成功都 format 一份。

=== 6. format 出来的东西长什么样（跑在子进程里，免得把本机绝对路径打进这份输出）===
  这一次 7 行：
    | Traceback (most recent call last):
    |   File "<string>", line 6, in <module>
    |     make()
    |     ~~~~^^
    |   File "<string>", line 3, in make
    |     raise ValueError('消息就这一句')
    | ValueError: 消息就这一句
```

每帧下面多出来的那行是**源码原文**（第 7 节那个 `.line` 字段），带 caret 指出表达式位置 ——
这就是上面那笔开销的来源：它得把源码文件读进来（`linecache`）。

最后是选型判据，不靠口味：

```
  → 每帧下面多出来的那行是**源码原文**（第 7 节的 .line 字段），带 caret 指出表达式位置；
    这就是第 5 段那笔开销的来源：它得去把源码文件读进来（linecache）。

=== 7. EAFP / LBYL 的选型判据（不靠口味，靠这三条）===
  ① 这条路**常见**吗？常见 → 它是正常流程，别用异常表达（第 2 段量过：每次都抛贵几十倍）。
     反例：`for x in items: try: use(x) except TypeError: ...`，十次有九次进 except → 先判类型。
  ② 检查和使用之间会不会**变**？会 → LBYL 不可靠：先 os.path.exists(p) 再 open(p)，
     中间文件可能被人删掉（这叫 TOCTOU）。本机起不了并发竞争，**这条只做记录、不下结论**；
     但「查过 ≠ 还在」这个方向本身就够说明为什么 Python 偏爱直接试。
  ③ 检查本身贵不贵？`k in d` 一次哈希 + `d[k]` 又一次 = 白付一半（第 1 段实测）。
     一次查完的正解是哨兵：
     d.get('a', MISSING) → 有，值是 None（None 也分得清）
```

第 ② 条（TOCTOU）要说实话：本机起不了并发竞争，**那条只做记录、不下结论**；
但「查过 ≠ 还在」这个方向本身就够说明为什么 Python 偏爱直接试。

**收口**：

```
     d.get('b', MISSING) → 有，值是 1（None 也分得清）
     d.get('zz', MISSING) → 真的没有

=== 8. 收口 ===
  ① 不进 except 的 try 几乎免费（本机 3.14 实测；社区叫它「零成本异常」，
     从哪个版本开始免费的本章不验证），「包一层」本身不是负担。
  ② 贵的是**真的抛**：栈深 × traceback × 格式化。异常只用来报「异常」。
```

---

## 第 16 节 · 静悄悄杀手：那些**不报错**的错

本章到这里都在讲「怎么把报错用好」。这一节讲它的反面：**最不危险的错是报错，最危险的是不报错**。
这一节每一条都是日常代码里最常见的写法 —— 这正是它们危险的原因：那种代码逐行读下来，每一行都像负责任的写法。

### 一、裸 `except`：连退出信号一起咽

```
$ python3 o_silent.py
=== 1. 裸 except：连退出信号一起咽 ===
  正常用法（接除零）：bare([1, 0, 2]) = [1.0, '跳过', 0.5]
  把 SystemExit 换成给它看： SystemExit 被裸 except 咽掉了，函数正常返回
  为什么这是安全洞：`except:` 同时接住了 KeyboardInterrupt / SystemExit / GeneratorExit，
    也就是第 3 节那条「Ctrl-C 不是 Exception」的例外被你**反向**封死了 —— Ctrl-C 按了没反应。
```

`except:` 同时接住了 `KeyboardInterrupt` / `SystemExit` / `GeneratorExit` ——
也就是第 3 节那条「Ctrl-C 不是 `Exception`」的例外被你**反向**封死了。
判据：**永远写 `except 具体类型:`**；要接住一切就明写 `except BaseException:`，
写出来是让读代码的人知道你是故意的，而不是打漏了一个类型名。

### 二、循环里吞异常：数据少了一半，程序说它跑成功了

```
  判据：**永远写 `except 具体类型:`**；要接住一切就明写 `except BaseException:`，
    写出来是让读代码的人知道你是故意的，而不是打漏了一个类型名。

=== 2. 循环里吞异常：数据少了一半，程序说它跑成功了 ===
  输入 6 条 → 静默版产出 4 条 = [1, 2, 4, 6]
    程序退出码 0、没有任何输出告诉你少了 2 条。这就是第 1 类静默。
```

输入 6 条，静默版产出 4 条，**退出码 0、没有任何输出告诉你少了 2 条**。
判据：可以咽异常，**不可以丢掉证据**。要么记账（像上面那条），要么让它往上跑。

### 三、只存 `str(exc)`：日志里只剩一个键名，谁都找不到是哪一行

```
  记账版：产出 [1, 2, 4, 6] ｜ 丢掉的及原因 = [('三', "invalid literal for int() with base 10: '三'"), ('五', "invalid literal for int() with base 10: '五'")]
  判据：可以咽异常，**不可以丢掉证据**。要么记账（像上面），要么让它往上跑。

=== 3. 只存 str(exc)：日志里只剩一个键名，谁都找不到是哪一行 ===
  日志一共 10 行；前 3 行：
    | WARNING py07-demo 只存了消息：'rate'
    | ERROR py07-demo 存了 exc_info（exception() 自动带上）
    | Traceback (most recent call last):
  → 第一条只剩一个键名 'rate'，连是什么类型的错都看不出来；第二条带完整 traceback（帧 + 源码行）。
    顺带钉一条 KeyError 的坑：str(exc) 只是**带引号的键名**，不是 "KeyError: 'rate'"；
    本机现场：str(first) = "'rate'" ｜ repr(first) = "KeyError('rate')" ｜ type(first).__name__ = 'KeyError'
    所以「存 str(exc)」连类型都要靠猜，更别提行号。
    `logger.exception(...)` ≡ `logger.error(..., exc_info=True)`，
```

第一条日志只剩一个键名 `'rate'`，连是什么类型的错都看不出来。这里顺带钉一条 `KeyError` 的坑：
`str(exc)` 只是**带引号的键名**，不是 `"KeyError: 'rate'"`（上面第 5 行把 `str` / `repr` / `__name__` 三种都打了）。
`logger.exception(...)` ≡ `logger.error(..., exc_info=True)`，而且**只在 `except` 块里调用才有效** ——
出了 `except` 块 `sys.exc_info()` 就是 `(None, None, None)`（第 7 节实测过）。

### 四、「顺手换个说法」把根因挤到背景里

```
    在 except 块里调用才有效 —— 出了 except 块 sys.exc_info() 就是 (None, None, None)（第 7 节实测过）。
  对照：`print(exc)` 和 `logging.error(str(exc))` 都丢帧；`traceback.print_exc()` 不丢但走 stderr。

=== 4. 「顺手换个说法」把根因挤到背景里（第 5 节的链在这里的代价）===
  最后一行 = RuntimeError: 配置读不到 ｜ returncode = 1
  中间那句连接语 = ['During handling of the above exception, another exception occurred:']
```

只看最后一行会以为错在「读配置这件事没做成」，其实根因（`No such file`）在**上面那一段**。
写了 `from exc` 的话，连接语会变成 `The above exception was the direct cause ...`（第 5 节）——
也就是说连接语同时告诉你「有没有人替你留了路标」。

### 五、顶层把异常换成「出错了」+ 退出码 0：CI 看不见的失败

```
  → 只看最后一行会以为错在「读配置这件事没做成」，其实根因（No such file）在**上面那一段**。
    写了 `from exc` 的话，连接语会变成 'The above exception was the direct cause...'（第 5 节）。

=== 5. 顶层把异常换成「出错了」+ 退出码 0：CI 看不见的失败 ===
  stdout = '出错了： division by zero\n' ｜ stderr = '' ｜ returncode = 0
  → 这条最贵：脚本、CI、make、xargs 全都在看退出码（bash 手册第 3 章整章讲这个）。
    打印了「出错了」却 exit(0)，等于向上游宣布成功。要么非零退出，要么别接。
```

这条最贵：脚本、CI、make、xargs 全都在看退出码（bash 手册第 3 章整章讲这个）。
打印了「出错了」却 `exit(0)`，等于向上游宣布成功。**要么非零退出，要么别接。**

### 六、三种「看不见的清理失败」，同一族

```
  改成 exit(1)：returncode = 1 ｜ stdout = '出错了： division by zero\n'

=== 6. 三种「看不见的清理失败」，同一族 ===
  ① finally 里抛：把块体的异常顶掉（第 2 节第 6 段演过）→ 现场只剩收尾那条报错。
  ② __exit__ 返回真值：异常就地消失（第 10 节第 4 段）。
  ③ __exit__ 里自己抛：块体成功的结果被覆盖 / 块体失败的根因被顶掉（第 14 节第 4 段）。
```

三条的共同点是**都不影响下一行照常执行**。所以唯一可靠的排查动作是
「把可疑的那一层临时改成 `raise` / 打日志」，而不是继续读代码。

本节收成一五条纪律（每条都回指上面某一段）：

```
  → 三条的共同点：**都不影响下一行照常执行**。所以唯一可靠的排查动作是
    「把可疑的那一层临时改成 raise / 打日志」，而不是继续读代码。

=== 7. 本章推荐的五条纪律（每条都回指上面某一段）===
  · 接得窄 —— 只接你真的能处理的那一两个类型；接 BaseException / 裸 except 一律当 bug（第 1 段）
  · 要么记要么抛 —— 绝不写 except: pass；咽掉时至少留下 (数据, 原因) 的账（第 2 段）
  · 日志带 exc_info —— logging.exception 或 exc_info=True；只存 str(exc) 等于把帧扔掉（第 3 段）
```

**收口**：

```
  · 换说法就写 from —— raise New(...) from exc；跨层包装时连接语是给下一个读日志的人留的路标（第 4 段）
  · 失败要给非零退出码 —— CLI 顶层接了异常就必须 sys.exit(非 0)（第 5 段）

=== 8. 收口 ===
```

---

## 第 17 节 · 异常走到程序边界：退出码、`atexit`、`os._exit`、线程里的异常

前面都在进程内部。这一节问：**异常跑到进程边界会发生什么**。
这一节的每一条都用子进程跑（要的就是真实退出码和真实的 stdout / stderr 分离）。

```
$ python3 p_process.py
=== 1. 未捕获异常 = 退出码 1 + 全文进 stderr，stdout 一点不剩 ===
  ① 正常打印过后才崩：
      returncode = 1 ｜ stdout = '这条在 stdout\n' ｜ stderr 首行 = 'Traceback (most recent call last):'
  ② 崩在前面：
      returncode = 1 ｜ stdout = '' ｜ stderr 首行 = 'Traceback (most recent call last):'
```

分工是死的：**traceback 只走 stderr**。把 stdout 存进文件 / 管道时，报错不会污染数据；
反过来说，`python3 x.py > out.txt` 时你可能看不见报错在哪，除非终端还在打 stderr。

`sys.exit(值)` 的三种形态，以及它**其实就是个异常**：

```
=== 2. sys.exit(值) 的三种形态：退出码怎么来的 ===
  ① sys.exit(3)
      returncode = 3 ｜ stdout = '' ｜ stderr 首行 = ''
  ② sys.exit()（不给值）
      returncode = 0 ｜ stdout = '' ｜ stderr 首行 = ''
  ③ sys.exit('字符串')
      returncode = 1 ｜ stdout = '' ｜ stderr 首行 = '这行会打到 stderr'
  → 字符串那条固定给退出码 1，并把文本写到 stderr；给整数就用那个整数。
    sys.exit 做的事就是**抛 SystemExit**：
  ④ 在同一个进程里把 sys.exit 接住：
      returncode = 0 ｜ stdout = '接到了 SystemExit, code = 7\n' ｜ stderr 首行 = ''
    接住之后进程继续跑到末尾，退出码回到 0 —— 那条 7 被你自己吃掉了。
  ⑤ 改用 except Exception 拦 sys.exit：
      returncode = 7 ｜ stdout = '' ｜ stderr 首行 = ''
    拦不住（第 3 节：SystemExit **不是** Exception 的儿子）→ 实测 returncode = 7
```

第 ③ 条（字符串）固定退 1 并把文本写到 stderr；第 ④ 条最有意思：`sys.exit` 做的事就是**抛 `SystemExit`**，
所以它能在同进程被接住 —— 接住之后进程继续跑到末尾，退出码回到 0，那条 `7` 被你自己吃掉了。
第 ⑤ 条是对照：`except Exception` **拦不住** `sys.exit`（第 3 节：`SystemExit` 不是 `Exception` 的儿子），
所以那个 `returncode = 7` 仍然是解释器按 `SystemExit` 的 code 退的。

三类收尾动作的执行顺序，一条命令验完：

```
=== 3. finally / with / atexit 的执行顺序（谁先谁后）===
  stdout 逐行 = ['body', 'finally', 'with-exit', 'main-end', 'atexit-2', 'atexit-1']
  → atexit 注册的回调**逆序**执行（后登记先跑，和第 11/14 节那条栈律一致），
    而且跑在 finally / with 全部结束**之后**、解释器关闭之前。
  未捕获异常时 atexit 还跑吗？
      returncode = 1 ｜ stdout = 'atexit 跑了\n'
```

`atexit` 注册的回调**逆序**执行（后登记先跑，和第 11 / 14 节那条栈律一致），
而且跑在 `finally` / `with` 全部结束**之后**、解释器关闭之前。最后一行还答了一个常见疑问：
**未捕获异常时 `atexit` 照样跑**（退 1，`atexit 跑了`）。

★ 但 `os._exit` 把这三步全省了：

```
=== 4. os._exit：跳过 atexit、跳过 finally、连缓冲区都不冲 ===
  子进程 stdout（**被管道捕获，所以是块缓冲**）= '' ｜ returncode = 0
  同一份代码，只在 os._exit 前加一句 flush() = '这条进了缓冲区\n'
  同一份代码，把 os._exit(0) 换成 sys.exit(0) = '这条进了缓冲区\n这条来自 atexit\n'
    → 三份只差那一行：'这条来自 atexit' 只在最后一份里出现，说明 os._exit 把 atexit 整步跳过了；
      第一份连'这条进了缓冲区'都没有，是因为它躺在块缓冲里没来得及冲出去（不是没执行 print）。
  → os._exit 是「立刻死」：不冲 stdout、不跑 finally、不跑 atexit、不看异常。
    它的正当用处极少（典型场景是 fork 之后让子进程立刻退出，不碰父进程留下的任何收尾）；
    平时写它 = 亲手制造第 16 节那种「没有任何痕迹的失败」。
```

三份代码只差那一行。第一份连「进了缓冲区」那句都没有 —— 那句躺在**块缓冲区**里，
而 `os._exit` 不给它冲出去的机会；`atexit` 整步也被跳过。正当用处极少
（典型场景是 fork 之后让子进程立刻退出，不碰父进程留下的任何收尾），其余场合写它就是在造静默失败。

未捕获异常的最后一道处理是 `sys.excepthook`（只在主线程）：

```
=== 5. sys.excepthook：未捕获异常的最后一道处理（只在主线程）===
  装了自定义 hook 之后，stderr = 'HOOK 收到 ZeroDivisionError | division by zero\n' ｜ returncode = 1
  → hook 换掉只换**打印方式**，退出码仍是 1（hook 里 return 不能改退出码）。
    hook 收到的三元组 = 第 10 节 __exit__ 收到的那三个（同一个 exc_info 形状）。
  未捕获异常时的**三条出口**：① sys.excepthook（主线程）② threading.excepthook（子线程）
    ③ 都没装就打 traceback。本机有没有 threading.excepthook → True
```

hook 换掉的只是**打印方式**，退出码仍是 1（hook 里 `return` 不能改退出码）。
它收到的三元组 = 第 10 节 `__exit__` 收到的那三个，同一个 `exc_info` 形状。

最后一个边界，也是并发代码最常见的静默：

```
=== 6. 子线程里的未捕获异常：不会杀进程，退出码照样 0 ===
  returncode = 0 ｜ stdout = '主线程活得好好的，退出码由它决定\n'
  stderr 最后一行 = 'ZeroDivisionError: division by zero'
  stderr 行数 = 12 （traceback 照样打全，只是不改变结局）
  → 这是并发代码最常见的静默：线程崩了，主流程不知道，join 也不报错。
    判据：**线程不会把异常传给创建者**；要传就得自己收（把 exc 放进队列 / 存到结果对象上，
    或者用 concurrent.futures 那种把异常装进 Future 的写法 —— 本章不演，只把这条记在「没验证」里）。
```

`concurrent.futures` 那种「把异常装进 Future」的写法本章**不演**（记在「没演」里），
第 11 章讲并发时会正面处理「子线程的故障怎么交回主线程」。

**收口**：

```
=== 7. 收口 ===
  ① 未捕获异常 → traceback 进 stderr、退出码 1；这是解释器替你做的最后一层 except。
  ② sys.exit(n) 就是 raise SystemExit(n)：能接（except SystemExit），但 except Exception 接不住。
  ③ 顺序：块体 → finally / __exit__ → atexit（逆序）→ 解释器关闭。os._exit 把这三步全省了。
  ④ 子线程的未捕获异常只打 traceback，不改变进程退出码 —— 别把「没崩」当「没出错」。
  ⑤ CLI 里 except 之后**必须**给非零退出码（第 16 节第 5 段），否则上游全在骗自己。
```

---

## 第 18 节 · `with` 落到真实文件上：半截文件、原子替换、一次性的文件迭代器

这一节把前面所有零件对准一个真实场景：**写配置文件**。
脚本会在**沙箱当前目录**里真建真删文件（`files-demo/`，跑完自己 `rmtree`），所以这里的输出都是磁盘上的事实。

```
$ python3 q_files.py
=== 1. with 只保证「关掉」，不保证「写全了」——半截文件是真实现场 ===
  崩在外面： 写到一半崩了
  磁盘上留下了什么： '{"a": 1,'
  用 json 读它会怎样：
    → JSONDecodeError: Expecting property name enclosed in double quotes: line 1 column 9 (char 8)
    结构化字段：msg='Expecting property name enclosed in double quotes' lineno=1 colno=9 pos=8
```

上面是本章最重要的一条边界澄清：`with` 帮你把文件**关好**（数据交给了操作系统），
但它不是事务 —— 磁盘上真的留下了 `'{"a": 1,'`，拿 `json` 读它就当场炸
（炸出来的 `JSONDecodeError` 自带 `lineno` / `colno` / `pos`，就是第 7 节那套结构化字段）。

第二个问题：关掉之后能不能立刻读到？

```
=== 2. 关掉之后能不能立刻读到？（close 的时机就是 with 退出那一刻）===
  块体内用另一个句柄读 = '' ← 还没 close，读到的是旧的/空的
  块体外再读 = '第一行\n' ← with 一退出就 close 了
  判据：写模式打开时，**同一份内容对别的句柄不一定可见**；close 之后才保证可见。
    再往下一层（数据是否真落盘）要靠 flush + os.fsync；本机无法验证掉电行为，**不做结论**。
```

判据：写模式打开时，**同一份内容对别的句柄不一定可见**；`close` 之后才保证可见。
再往下一层（数据是否真落盘）要靠 `flush` + `os.fsync`，本机无法验证掉电行为，**不做结论**。

所以配置文件的正确姿势是**原子替换**：先写临时文件，再 `os.replace`。

```
=== 3. 原子替换：先写临时文件，再 os.replace（崩溃也留不下半截配置）===
  改动前的内容 = {"a": 1}
  正常改完之后 = {"a": 2, "b": [1, 2]} ｜ 目录里剩几个文件 = ['cfg.json', 'real.json', 'seq.txt']
  现在让它在写到一半时崩（崩在 replace 之前）：
    又是写到一半崩了 → 走 except 分支删掉 tmp
  真文件的完整内容 = {"a": 2, "b": [1, 2]} （没被半截数据污染）
  → 这就是「with + 临时文件 + replace」这套写法的全部理由：
    任何一步崩了，要么旧文件原样在，要么新文件原样在，**没有第三种**。
```

这就是「with + 临时文件 + replace」这套写法的全部理由：任何一步崩了，
要么旧文件原样在，要么新文件原样在，**没有第三种**。

临时文件的删除有两种契约，本节都量了：

```
=== 4. 临时文件的自动删除：NamedTemporaryFile 的 delete 参数是干什么的 ===
  delete=True：块体内存在 = True ｜ 块体外存在 = False → 出块即删，不用你管
  delete=False：块体外存在 = True → 得自己删，忘了就是垃圾文件
  想要「登记了就一定删、哪怕中间崩」，就交给 ExitStack 的 callback（第 14 节）：
      ['  登记好了，中间崩一次', '  外面接住：中途崩'] ｜ 崩了之后文件还在吗 → False
  → callback 登记在 with 退出时**必然**执行，不管走的是正常路径还是异常路径 ——
    这一条就是它比「在函数末尾写 os.remove」强的地方。
```

`delete=True` 出块即删；`delete=False` 得自己删，忘了就是垃圾文件。
想要「登记了就一定删、哪怕中间崩」，就交给 `ExitStack.callback`（第 14 节）——
它强的地方正是最后那两行：中间崩一次，文件照样没了。

读文件的正确形状是 EAFP + 按 `errno` 分派：

```
=== 5. 读文件的 EAFP：用 errno 分辨「没有」和「没权限」（第 8 节那条判据的落地）===
  open('files-demo/没有这个.json') → FileNotFoundError errno=2 ：真的不存在（可以当成首次运行）
  open('/etc/hosts'          ) → 读到了 '127.0.0.1 lo'
  → LBYL 写法（if os.path.exists: open）在这里有两个问题：
    ① 判了存在还是不保证能读（权限、路径是目录、文件被删）；② 检查和打开之间可能被人动过。
    所以：**直接 open，接 OSError，按 errno 分派**。这就是 EAFP 在 I/O 上的标准形状。
```

LBYL 写法（`if os.path.exists(p): open(p)`）的两个问题都印在上面：判了存在还是不保证能读，
以及检查和打开之间可能被人动过（第 15 节那条 TOCTOU）。

最后一条和第 04 章那条规律接上了 —— **文件对象是一次性的迭代器**：

```
=== 6. 文件对象是**一次性**的迭代器（第 04 章那条规律在 I/O 上的样子）===
  同一个句柄：第一次 list(f) = ['一\n', '二\n', '三\n']
            第二次 list(f) = [] ← 游标已经在末尾了，**不报错，只是空**
  想读两遍要么 seek(0)： ['一\n', '二\n', '三\n']
  → 这就是第 04 章「迭代器只能过一遍」的 I/O 版本；配置读两遍的人都会在这里翻车，
    而且翻车方式是**静默的空列表**，不是报错。
```

第二个 `list(f)` 是**空的，而且不报错**。配置读两遍的人都会在这里翻车，
翻车方式就是这种静默的空列表。

**收口**：

```
=== 7. 收口 ===
  ① with open 管的是 close，不管「写全」「落盘」「原子」——那三件事要你自己安排（第 1、3 段）。
  ② 配置/状态文件一律「同目录临时文件 + os.replace」；崩了留旧不留半截。
  ③ I/O 用 EAFP：直接 open，接 OSError，**按 errno 分派**，不要 if exists 再 open。
  ④ 临时文件的删除要么 delete=False + 自己登记 callback，要么信任 delete=True（第 4 段实测）。
  ⑤ 文件对象是迭代器，过一遍就空；要重读就 seek(0) 或者一次 read 进来存着（第 6 段）。

  清理：files-demo/ 已删除，现在这个目录里剩 = ['a_minimal_shape.py', 'b_tree.py', 'c_matching.py', 'd_chain.py', 'demo.sh', 'e_raise.py', 'f_traceback.py', 'g_reading.py', 'h_custom.py', 'i_with_protocol.py', 'j_with_more.py', 'k_contextlib.py', 'l_std_tools.py', 'm_exitstack.py', 'n_cost.py', 'o_silent.py', 'p_process.py', 'q_files.py', 'r_capstone.py', 's_checklist.py']
```

---

## 第 19 节 · capstone：把本章零件装成一个真工具（安全读配置）

本节把第 2~18 节的零件装成一个能用的函数：读一个 JSON 配置，
分五类失败（不存在 / 是目录 / 内容坏 / 字段缺 / 瞬时故障），只对**该重试的那一支**重试，
每次尝试都登记一条审计流水，跑完自己核对有没有留下临时文件。
所有文件都在沙箱的 `capstone-demo/` 里，跑完删干净。

先看正常路径 —— 它一个字都不打：

```
=== 1. 正常路径：读到了就返回，报错一个字都不打 ===
  read_config(good.json) = {'name': 'demo', 'port': 8080}
  require(...) 取走的字段 = {'name': 'demo', 'port': 8080}
  记账： {'attempts': 1, 'ok': 1, 'retried': 0}
```

四类「看起来都叫读不到」的错，走的是四条不同支路，每一层报错都带着可动的信息：

```
=== 2. 文件不存在：不重试，直接给出「这是首次运行」那一类结论 ===
    第 1 层：ConfigUnreadable(E-CFG-ENOENT) 配置文件 capstone-demo/没有.json 不存在（No such file or directory）
    第 2 层：FileNotFoundError [Errno 2] No such file or directory: 'capstone-demo/没有.json'
  记账（attempts 只 +1，因为 ENOENT 不重试）： {'attempts': 2, 'ok': 1, 'retried': 0}

=== 3. 路径是个目录：三类 OSError 走的是三条不同的支路 ===
    第 1 层：ConfigUnreadable(E-CFG-ISDIR) capstone-demo/是个目录 是个目录，不是一个文件
    第 2 层：IsADirectoryError [Errno 21] Is a directory: 'capstone-demo/是个目录'
```

第 4 段的原文里那个「第 2 行第 15 列」值得单独看：

```
=== 4. 内容坏：JSONDecodeError 自带行列，直接进消息（第 7 节那份字段的复用）===
    第 1 层：ConfigInvalid(E-CFG-JSON) capstone-demo/bad.json 第 2 行第 15 列不是合法 JSON：Expecting property name enclosed in double quotes
    第 2 层：JSONDecodeError Expecting property name enclosed in double quotes: line 2 column 15 (char 31)
  → 有了行列，你去修的是第 2 行；只有「Expecting value」的话整个文件都得读一遍。
```

有了行列，你去修的是第 2 行；只有「Expecting value」的话整个文件都得读一遍 ——
这就是第 7 节那份结构化字段在**用户侧**的兑现。
第 5 段是另一类：IO 没错、JSON 没错，但**不合约定**，所以走的是家族里另一个儿子：

```
=== 5. 字段缺：IO 没错、JSON 没错，但**不合约定** → 另一个儿子 ===
    第 1 层：ConfigInvalid(E-CFG-SCHEMA) capstone-demo/thin.json 里缺字段 ['port']；实际读到的字段是 ['name']
```

★ 第 6 段是这一节的核心判断：**只有可能是瞬时的故障才值得重试**。
本机没有真网络，所以脚本用一个假 `open` 造出「前两次 `EBUSY`、第三次成功」：

```
=== 6. 唯一该重试的那一支：瞬时故障（用假 open 演，不靠运气）===
  读到 = {'name': 'demo', 'port': 8080} ｜ open 被叫了 3 次
  记账变化： {'attempts': 5, 'ok': 2, 'retried': 0} → {'attempts': 8, 'ok': 3, 'retried': 2} （retried 终于不是 0 了）
    第 1 层：ConfigUnreadable(E-CFG-IO) capstone-demo/good.json 连试 3 次都失败：一直忙
    第 2 层：OSError [Errno 16] 一直忙
  → 耗尽次数才报错，报的是**最后一次**那个 OSError（from last_oserror），
    并带着「试了几次」这个数字。真实代码这里该 sleep 一下再试；
    本章不睡，因为 sleep 会让输出不可复现（第 15 节的计时也因此只读倍率）。
```

真实代码这里该 `sleep` 一下再试；本章不睡，因为 `sleep` 会让输出不可复现
（第 15 节的计时也因此只读倍率）。

调用方怎么用这个家族 —— 一网打尽还是分派，由调用方选：

```
=== 7. 调用方只要接 ConfigError 一网打尽；想区分就接窄的（第 4 节的顺序律）===
  capstone-demo/没有.json    → 走「读不到」分支：E-CFG-ENOENT
  capstone-demo/bad.json   → 走「内容坏」分支：E-CFG-JSON
  capstone-demo/thin.json  → 走「内容坏」分支：E-CFG-SCHEMA
  → 三条支路的**处理方式本来就该不同**：读不到→提示路径/权限；内容坏→报行列让人修文件；
    字段缺→给默认值或拒绝启动。要是只有一个 Exception，这三件事就会被写成同一个 pass。
```

然后是第 18 节那句「with 不管事务」的兑现 —— 现场对账有没有留下临时文件，
并且**顺便证明这条对账会响**：

```
=== 8. 现场对账：临时文件有没有留下？（顺便证明这条对账**会响**）===
  目录内容 = ['bad.json', 'good.json', 'thin.json', '是个目录']
  带 .tmp 后缀的残留 = [] → 期望是 []
  先故意漏一个，同一条扫法立刻变红： ['故意漏下的.tmp']
  删掉再扫： [] → 所以上面那句 [] 是查出来的，不是默认值。
```

审计流水（每次尝试都记一条，失败的也记），以及总数对得上不上：

```
  审计流水（每次尝试都记了一条，失败的也记）：
    · 第 1 次尝试读 capstone-demo/good.json
    · 第 1 次尝试读 capstone-demo/没有.json
    · 第 1 次尝试读 capstone-demo/是个目录
    · 第 1 次尝试读 capstone-demo/bad.json
    · 第 1 次尝试读 capstone-demo/thin.json
    · 第 1 次尝试读 capstone-demo/good.json
    · 第 2 次尝试读 capstone-demo/good.json
    · 第 3 次尝试读 capstone-demo/good.json
    · 第 1 次尝试读 capstone-demo/good.json
    · 第 2 次尝试读 capstone-demo/good.json
    · 第 3 次尝试读 capstone-demo/good.json
    · 第 1 次尝试读 capstone-demo/没有.json
    · 第 1 次尝试读 capstone-demo/bad.json
    · 第 1 次尝试读 capstone-demo/thin.json
  记账总数 = {'attempts': 14, 'ok': 4, 'retried': 5} ｜ attempts 和 ok 的差 = 失败次数 = 10
  清理后：capstone-demo/ 还在吗 → False ｜ 当前目录剩 = ['a_minimal_shape.py', 'b_tree.py', 'c_matching.py', 'd_chain.py', 'demo.sh', 'e_raise.py', 'f_traceback.py', 'g_reading.py', 'h_custom.py', 'i_with_protocol.py', 'j_with_more.py', 'k_contextlib.py', 'l_std_tools.py', 'm_exitstack.py', 'n_cost.py', 'o_silent.py', 'p_process.py', 'q_files.py', 'r_capstone.py', 's_checklist.py']
```

最后是两条「为什么不做得更多」的说明，它们比代码本身更容易被抄错：

```
=== 9. 为什么 report() 里不打 traceback ===
  ① 这是**给用户看的 CLI 报错**：路径和帧对修配置的人没用，行列和 code 才有。
     第 7 节教的是「怎么把帧拿出来」，第 15 节量的是「把帧排版出来要多少钱」——
     两条都不等于「每一处报错都该带上整份栈」；带上不带上，取决于读者是谁。
  ② 打整份 traceback 是第 15 节量过的那笔大开销（比 str 贵两个数量级），
     但对**故障**来说该花 —— 所以工程做法是两条都要：
     给人看的那一行走 stdout/CLI，给运维查的那份带帧的走日志（logging.exception，第 16 节第 3 段）。
  ③ 上面这个函数走的是第一条；这个脚本没有日志系统，所以第二条留在 README 的示例里。
```

**收口**（脚本自己打的对照表，标明这一版**没**演的那一条）：

```
=== 10. 收口：这个小工具用掉了本章哪几节 ===
  第 2 节   → except 接住之后 continue、正常路径才走到 return —— 谁跑谁不跑就是第 2 节那张走位图
  第 4 节   → except 的顺序：窄的先（FileNotFoundError 在 OSError 之前）
  第 5 节   → 每一处包装都写 from exc，根因不丢
  第 8 节   → 报错里读的是 strerror / lineno / colno 这些**结构化字段**，不是猜消息
  第 9 节   → AppError(ConfigError(...)) 三层家族 + code 字段
  第 11 节  → 读文件全程写在 with 里：块体崩了也照样 close
  第 14 节  → ExitStack.callback 登记审计流水：成功失败都留下一条
  第 15 节  → 只对可能是瞬时的错重试；ENOENT 重试三遍是纯粹的浪费
  第 16 节  → 没有任何一处 except: pass；每条支路都有结论或者往上抛
  第 18 节  → close 不等于事务，所以第 8 段当场扫一遍 .tmp 残留
  这一版**没**演的一条：第 17 节「CLI 顶层接住 ConfigError 之后要 sys.exit(非 0)」——
  这个脚本不是 CLI（它的作用是把过程打给你看），所以它的退出码恒 0；写法见 README 第 19 节。
```

---

## 第 20 节 · 对账：这 19 个脚本是不是真按规矩写的，以及一条守卫怎么证明它会响

本章前面所有「19 个脚本 / 139 个小节」这类数字，都不是人写的，是 `s_checklist.py` 数出来的。
这一节有两个目的：**把数字的出处钉住**，以及把第 16 节那条纪律用在自己身上 ——
**不响的检查等于没检查**，所以它必须先证明自己会响。

规矩只有三条，写在屏幕上，然后拿它去扫：

```
══════ 第 20 节 对账：这 19 个脚本是不是真按规矩写的，以及一条守卫怎么证明它会响 ═══════════════════════════════

$ python3 s_checklist.py
=== 1. 先把规矩写在屏幕上，再拿它去扫（三条，每条都可能失败）===
  ① 分段编号必须 1..N 连续 —— 跳号说明有一段被删掉了，可末段编号没跟着改；
  ② 最后一段的标题里必须有「收口」—— 没有就说明它演了一半，判据没给；
  ③ 至少得有一条分段 —— 一条都没有的文件不配进这一章的清单。
  注意这里**没有**的几条，免得你以为它们被检查了：
  · 没查脚本能不能跑通（跑通与否看第 2..19 节的退出码）；
```

注意它**明确写了自己没查什么** —— 三条。检查工具最坏的失败不是漏报，是它的说明夸大了覆盖面。

开扫（这就是那两个数字的出处）：

```
  · 没查每段的标题内容对不对（内容只有你自己读得出来）；
  · 没数「每段是不是都真打了输出」（只看标题行，行数再多也查不出灌水）。

=== 2. 开扫：本章每个脚本的段数、编号、末段标题 ===
  a_minimal_shape.py     段数=7   编号 1..7 连续 ｜ 末段「收口：一句话判据」
  b_tree.py              段数=8   编号 1..8 连续 ｜ 末段「收口」
  c_matching.py          段数=7   编号 1..7 连续 ｜ 末段「收口」
  d_chain.py             段数=8   编号 1..8 连续 ｜ 末段「收口」
  e_raise.py             段数=8   编号 1..8 连续 ｜ 末段「收口」
  f_traceback.py         段数=7   编号 1..7 连续 ｜ 末段「收口」
  g_reading.py           段数=8   编号 1..8 连续 ｜ 末段「收口：一张「看到什么往哪查」的表」
  h_custom.py            段数=7   编号 1..7 连续 ｜ 末段「收口」
  i_with_protocol.py     段数=7   编号 1..7 连续 ｜ 末段「收口（把 with 记成一句话）」
  j_with_more.py         段数=8   编号 1..8 连续 ｜ 末段「收口」
  k_contextlib.py        段数=6   编号 1..6 连续 ｜ 末段「收口」
  l_std_tools.py         段数=7   编号 1..7 连续 ｜ 末段「收口」
  m_exitstack.py         段数=7   编号 1..7 连续 ｜ 末段「收口」
  n_cost.py              段数=8   编号 1..8 连续 ｜ 末段「收口」
  o_silent.py            段数=8   编号 1..8 连续 ｜ 末段「收口」
  p_process.py           段数=7   编号 1..7 连续 ｜ 末段「收口」
  q_files.py             段数=7   编号 1..7 连续 ｜ 末段「收口」
  r_capstone.py          段数=10  编号 1..10 连续 ｜ 末段「收口：这个小工具用掉了本章哪几节」
  s_checklist.py         段数=4   编号 1..4 连续 ｜ 末段「收口：一条检查的价值 = 它能失败的那几种方式」
```

然后是关键的一步：现场造三个**不合格**的样本 + 一个合格样本，让三条判据各响一次：

```
  —— 合计：脚本 19 个 ｜ 小节 139 段 ｜ 不合格 0 个
  本脚本自己 s_checklist.py 也在这份清单里 → 它同样得过这三条，不然它就是一盏只照别人的灯。

=== 3. 现场造三个坏样本 + 一个好样本：证明这条守卫能响，也不误报 ===
  bad1_numbering.py    段数=3   ★响了 1 条：编号不连续：实际是 [1, 2, 4]
  bad2_no_closure.py   段数=3   ★响了 1 条：末段标题是「最后一演」，没写收口 = 没给判据
  bad3_no_section.py   段数=0   ★响了 1 条：一条分段都没有，它不是取证脚本
  good_sample.py       段数=3   ✓ 一条都没响（它确实合格，不是漏查）
  → 三条规矩各被一个样本触发了一次（跳号 / 没收口 / 没分段），
    第四个是真合格的样本、没被冤枉；
```

没有这一步，上面那句「不合格 0 个」就只是一句恒真的废话 —— 你永远不知道那条判据是不是从来没被执行过。
（同一条纪律在数据结构那本里也出现过：**恒真守卫比没有更糟**。）

**收口**：

```
    所以第 2 段那句「不合格 0 个」是有判断力的结论，不是一句恒真的废话。
  收尾自证：check-demo/ 还在吗 → False ｜ 当前目录里 .py 个数 = 19（坏样本没留下尸体）

=== 4. 收口：一条检查的价值 = 它能失败的那几种方式 ===
  ① 写「每个脚本都有收口」这种承诺之前，先问自己：有没有一条命令能把它证伪？
     没有 → 这条承诺三个月后一定和现实不符，而且没人知道。
  ② 守卫必须**当场演示过一次触发**（第 3 段），否则你不知道它是不是恒真。
     本章第 16 节那条「安静地跳过」是同一个道理：不响的检查等于没检查。
  ③ 结论要能被上游接住：合格退 0、不合格退 1，脚本才可能进 make check / CI；
     只会往屏幕上打「看起来没问题」的脚本，人不在的时候就是零。
```

最后现场演示一次「它什么时候退非 0」：往参数里塞一个**不是取证脚本**的文件，闸门当场就响。
（下面只留不合格那一行，完整表上一条已经打过了。）

```
  再放一个合格样本证明它不误报。这就是第 16 节「不响的检查等于没检查」的正面写法。
  它什么时候退非 0？往参数里塞一个**不是取证脚本**的文件，闸门当场就响（下面只留不合格那一行，完整表上一条已经打过了）：
```

这个「合格退 0、不合格退 1」的设计就是它能进 `make check` / CI 的原因：
人不在的时候，屏幕上打的字没有用，退出码才有用。

---

## 第 21 节 · 自证：沙箱里的事只留在沙箱，章目录一个字都没多

demo 的写操作全部发生在 `mktemp -d` 的沙箱里；此刻的当前目录就是沙箱 `work/`
（示例源码是 `cp -R` 进去的）。

但这一节不是「打一排 0 给你看」。先把「该有什么」写成一条**能判的式子**：
本章的源码只有 `*.py` 和 `demo.sh` 两种；`README.md` 是文档（跑之前有没有它都不该影响结论，
所以从清单里**显式放过**）；除此之外多出来的任何文件都算 demo 产物 —— 数的就是它。

```
$ echo "沙箱 work/ 里的项数（不含 README.md）=$(ls | grep -vc "^README.md$")，前 12 个："; ls | grep -v "^README.md$" | head -12
沙箱 work/ 里的项数（不含 README.md）=20，前 12 个：
a_minimal_shape.py
b_tree.py
c_matching.py
d_chain.py
demo.sh
e_raise.py
f_traceback.py
g_reading.py
h_custom.py
i_with_protocol.py
j_with_more.py
k_contextlib.py
[退出码 0]
```

每一项计数都得能响。所以先给它一个**正对照**：塞一个假的残留进去，同一条 `find` 立刻变红，再删掉：

```
$ touch 残留.tmp; echo "塞进一个假产物之后，非源码产物数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l) ← 会响"; rm 残留.tmp
塞进一个假产物之后，非源码产物数=1 ← 会响
[退出码 0]

$ echo "清掉之后：非源码产物数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l)  残留 .tmp 个数=$(find . -name "*.tmp" | wc -l)  残留子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)"
清掉之后：非源码产物数=0  残留 .tmp 个数=0  残留子目录数=0
[退出码 0]
```

`__pycache__` / `*.pyc` 这一对也先弄响一次 —— 随便 `import` 一个模块，Python 就会把这两样造出来：

```
$ printf "x = 1\n" > probe_mod.py; python3 -c "import probe_mod"; echo "import 之后：__pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l) ← 会响"; rm -rf __pycache__ probe_mod.py
import 之后：__pycache__ 个数=1  *.pyc 个数=1 ← 会响
[退出码 0]
```

顺带钉一条容易被记错的事实：只有 **`import`** 才产 `__pycache__`；
`python3 xxx.py` 直接跑脚本不产它 —— 本章全程跑脚本，所以下面那两条 0 是真的。

```
$ echo "本章跑完之后：__pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l)"
本章跑完之后：__pycache__ 个数=0  *.pyc 个数=0
[退出码 0]
```

章目录（源文件本体在这里）。清单显式排除 `README.md`，所以「README 写完之后重跑」得到的是**同一份输出**：

```
$ 〔在 07-errors-and-context 里〕 find . -maxdepth 1 -type f ! -name "README.md" | sort
./a_minimal_shape.py
./b_tree.py
./c_matching.py
./d_chain.py
./demo.sh
./e_raise.py
./f_traceback.py
./g_reading.py
./h_custom.py
./i_with_protocol.py
./j_with_more.py
./k_contextlib.py
./l_std_tools.py
./m_exitstack.py
./n_cost.py
./o_silent.py
./p_process.py
./q_files.py
./r_capstone.py
./s_checklist.py
[退出码 0]
```

```
$ 〔在 07-errors-and-context 里〕 echo ".py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)（这 19 个是源码本身，不是产物）  非源码产物数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l)  子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)"
.py 个数=19（这 19 个是源码本身，不是产物）  非源码产物数=0  子目录数=0
[退出码 0]
```

这一章一条网络命令都没用、没起过任何监听端口。本机在听的端口只打印**条数** ——
端口号和本章无关，不该写进教材：

```
$ if command -v ss >/dev/null 2>&1; then echo "本机 LISTEN 条目数=$(ss -lnt | grep -c LISTEN)"; else echo "ss 本机没有（那就更没得起）"; fi
本机 LISTEN 条目数=7
[退出码 0]
```

```
$ pgrep -a sleep | head -3; echo "（上面这一行为空 = 系统里没有 demo 留下的后台进程）"
（上面这一行为空 = 系统里没有 demo 留下的后台进程）
[退出码 0]
```

> 本章的取证纪律（写在脚本头部，不是空话）：临时目录 `mktemp -d "${TMPDIR:-/tmp}/py07-XXXXXX"`，
> `trap cleanup EXIT` 只删自己建的这一个；`--keep` 时不删、改成打一行提示。
> 屏幕上不主动打印绝对路径，只有 `runc_at` 会打目录的 basename。
> 子进程一律带 capture —— 本脚本自己的退出码必须一路是 0，才能当验收闸门用。

---

## 对照表

### C 里的概念 vs Python 的异常与清理（全章汇总）

| 你在 C 里的经验 | Python 的真实情况 | 本章证据 |
|---|---|---|
| 错误码 `int f(...)` + `errno` | 异常是**对象**：类型 + `args` + 帧链 + 结构化字段（`errno`/`lineno`/`colno`） | 第 5、7、8 节 |
| 返回值当「成功/失败」的唯一通道 | 常见路径走正常返回；失败走 `raise`。用异常当普通控制流每次多付几倍 | 第 15 节 |
| `goto fail;` 集中收尾 | `finally` / `__exit__` / `ExitStack.callback`：离开那一刻必然执行 | 第 2、10、14 节 |
| 手动 `free()`，忘了就漏 | `with` 把「还」绑在块体出口上；但 `__enter__` 崩了 `__exit__` 不跑，一样要防 | 第 10、11 节 |
| 析构顺序 = 构造逆序 | 同一律：enter 左→右，exit 右→左；`ExitStack` 也是栈 | 第 11、14 节 |
| `assert()` 是调试期检查 | Python 的 `assert` 就是 `raise` 的缩写，且被 `-O` **整行删除**（本章真跑） | 第 6 节 |
| `atexit()` 注册退出钩子 | 有，且**逆序**执行、跑在 `finally` / `with` 之后；`os._exit` 把这套全省了 | 第 17 节 |
| `fork` 后子进程立刻 `_exit` | 这就是 `os._exit` 的正当场景：不冲缓冲、不跑 finally、不跑 atexit | 第 17 节 |
| 退出码 0 / 非 0 是接口 | 完全一致：接了异常还 `exit(0)` = 向上游撒谎（bash 手册第 3 章） | 第 16、17 节 |
| `errno == ENOENT` 分派 | 同一套：接 `OSError`，按 `errno` 分派，而不是 `if exists` | 第 8、18 节 |
| `rename()` 原子替换落盘 | 一模一样：同目录临时文件 + `os.replace`，崩了留旧不留半截 | 第 18 节 |
| 回调函数指针 + `void* ctx` | `ExitStack.callback(fn, *args)`：登记时就绑死参数，退出时调用 | 第 14 节 |
| 自己写一套错误枚举 + switch | 自定义异常家族（宽父亲给外面、窄儿子给分派）+ `except` 顺序 | 第 4、9、19 节 |

### 四条「咽掉异常」的入口（本章反复出现，集中记一次）

| 写法 | 咽掉的是什么 | 本章哪一节现场演过 |
|---|---|---|
| `finally` 里 `return` | 块体那条异常整个消失（编译期还发 `SyntaxWarning`） | 第 2 节第 5 段 |
| `__exit__` 返回真值 | 异常在**这一层**被截住，外层根本看不到 | 第 10 节第 4 段、第 11 节第 6 段 |
| 生成器里 `except` 接住 `yield` 抛回异常 | 接了就等于咽（`suppress` 的原理） | 第 12 节第 3 段、第 13 节第 1 段 |
| `except:` / `except BaseException:` / `suppress(BaseException)` | 连 `SystemExit` / `KeyboardInterrupt` 一起咽 | 第 3 节第 6 段、第 16 节第 1 段 |

---

## 一页纸（考前扫这半页）

### 两句话，覆盖本章全部结论

```text
一、异常是有走位的控制流：try 只决定「从哪一行开始往外找 except」；
    else = 没炸才走；finally = 离开那一刻必执行的尾巴（return/break/正在抛都先跑它）。
    except 从上往下第一个 isinstance 成立的赢；__cause__ 是你写的，__context__ 是解释器记的。
二、with 只保证一件事：清理必然执行。__enter__ 的返回值给 as，
    __exit__ 的返回值决定异常要不要继续跑 —— 返回任何真值 = 就地抹掉，写字符串也算真值。
```

### 五条现抄判据（**这些不是捕获原文**，是本章脚本里反复用过的取证动作）

```python
sys.exc_info()                                  # (type, value, tb)，和 except as 是同一对象（第 7 节）
exc.__cause__, exc.__context__, exc.__suppress_context__   # 链的三种状态（第 5 节）
traceback.extract_tb(tb)                        # 结构化帧，有 .name .lineno .line，别 split 文本（第 7 节）
errno = exc.errno                               # OSError 一族用 errno 分派，不是猜消息（第 8、18 节）
contextlib.ExitStack().enter_context(cm)        # 资源个数运行时才知道（第 14 节）
```

### `try` 的走位图（背这一张就够）

| 情况 | try 剩余行 | except | else | finally |
|---|---|---|---|---|
| 一行都没炸 | 全跑 | 跳过 | **跑** | 跑 |
| 炸了且类型对上 | 从炸那行起全跳过 | **跑** | 跳过 | 跑 |
| 炸了但类型不对上 | 从炸那行起全跳过 | 跳过 | 跳过 | **跑**，然后异常继续往外 |
| `try` 里 `return` | —— | —— | —— | **照样跑**，跑完才真的返回 |
| `finally` 里 `return` | —— | —— | —— | 跑，并**把异常和返回值一起吞掉**（★别写） |

### `with` 的四条顺序律

1. enter 左→右，exit **右→左**（栈）；逗号堆 ≡ 逐层嵌套，日志逐字节相同。
2. 第 k 个 `__enter__` 失败：前 k−1 个照样被还，失败那个自己的 `__exit__` 不跑。
3. `__exit__` 收到什么，取决于异常**有没有穿出这一层的块体**（内层自己 `try` 住，外层就只看到正常退出）。
4. `@contextmanager`：`yield` 前 = enter，`yield` 的值 = `as`，`yield` 后 = exit；
   后置清理**必须**包 `try/finally`，且结果对象一次 `with` 用一次。

---

## 这章的坑（按代价从大到小）

1. **`__exit__` 返回真值 = 异常就地消失**，写 `'x'`、`1`、`(0,)` 都算真值。
   代价：现场只剩一条日志都没有的静默，多层 `with` 里几乎查不出来。
   正解：不写返回值，或明确 `return False`；真要咽就用 `contextlib.suppress(具体类型)`。（第 10、11 节）
2. **`finally` 里 `return`**（或 `raise`）把块体的异常整个吞掉，而且是在**编译那一刻**发一条 `SyntaxWarning`
   就当提醒过了。代价：`swallow()` 正常返回、报错一个字没打。（第 2 节）
3. **`except Exception` 接不住退出族** —— 这是好事；但反过来说，**裸 `except:` 会把 `KeyboardInterrupt`
   / `SystemExit` 一起咽掉**，Ctrl-C 按了没反应。要接一切就明写 `except BaseException:` 并给理由。（第 3、16 节）
4. **顶层接了异常却 `exit(0)`**：脚本 / CI / make / xargs 全看退出码，你打印了「出错了」就等于向上游撒谎。（第 16、17 节）
5. **用 `assert` 校验外部输入**：`python3 -O` 把那一整行**在编译时删掉**，被它挡住的真实故障随即发生。（第 6 节）
6. **except 分支顺序写反**：父亲在前 → 儿子那条分支永远变死代码，**不报错、不警告**，code review 最容易漏。（第 4 节）
7. **只存 `str(exc)`**：日志里只剩一个带引号的键名（`KeyError` 尤其坑），类型和行号全丢。
   要么 `logging.exception(...)`，要么 `exc_info=True`。（第 16 节）
8. **循环里 `except: pass`**：6 条数据产出 4 条，退出码 0。咽异常可以，丢证据不行 —— 至少留 `(数据, 原因)` 的账。（第 16 节）
9. **`with open(...)` 不是事务**：`with` 只管 close，不管「写全」；崩在中间磁盘上就是半截 JSON。
   配置文件一律「同目录临时文件 + `os.replace`」。（第 18 节）
10. **`@contextmanager` 的结果只能用一次**：复用撞的是 **`AttributeError`**（不是你以为的 `RuntimeError`）；
    而忘了在 `yield` 后写 `try/finally`，块体一崩就**没人收尾**。（第 12 节）
11. **收尾代码自己抛会盖住块体的异常**：`finally` / `__exit__` / `ExitStack.callback` 全一样，
    原始故障降级成 `__context__`。看到报错先翻 `__context__`。（第 14 节）
12. **子线程的未捕获异常不改变退出码**：线程崩了，主流程不知道，`join()` 也不报错，进程退 0。（第 17 节）
13. **长期持有 exception 对象 = 持有那一层所有局部变量**：帧链攥着 `f_locals`。只存 `args` / `str` / 行号。（第 15 节）
14. **把 `try` 当普通分支用**：不进 `except` 的 try 几乎免费，**每次都抛**才是真贵（本机 4.75 倍），
    栈深再叠一层（50 层深是本机浅栈的 35 倍）。（第 15 节）
15. **`redirect_stdout` 是进程全局**：多线程 / async 会串台。正解是 `print(x, file=f)`。（第 13 节）
16. **文件对象是一次性迭代器**：第二次 `list(f)` 是**空列表且不报错**。（第 18 节）

---

## 自检问题

1. `try` 块里抛了异常、`except` 类型对上、块体里还有一句 `return`：`else` 跑吗？`finally` 跑吗？
   （答：`else` 不跑 —— 它只在「try 一行都没炸」时跑；`finally` 跑。）
2. `except Exception` 能接住 `SystemExit` 吗？`except BaseException` 呢？为什么这么设计？
   （答：前者不能 —— `SystemExit` 是 `BaseException` 的直接儿子；后者能，而且这是裸 except 被禁的理由。第 3 节两条原文都跑了。）
3. `raise A from B` 之后，`A.__context__` 是 `None` 吗？
   （答：不一定 —— 现场是 `__cause__` 和 `__context__` **可以都在**，打印时优先 `__cause__`。第 5 节第 1 段那两行就是。）
4. traceback 中间那句 `During handling of the above exception, another exception occurred:` 说明什么？
   改成另一句说明什么？（答：前者 = 处理期间又炸了一个（隐式链，`__cause__` 是 `None`）；
   `The above exception was the direct cause of ...` = 有人显式写了 `from`。）
5. 一行 `with A() as a, B() as b:`，B 的 `__enter__` 抛异常：A 的 `__exit__` 跑不跑？B 的 `__exit__` 呢？
   （答：A 的跑，而且**收到那个异常**；B 的没跑（它 enter 就没成功）。第 11 节第 4 段。）
6. `__exit__` 返回 `'no'`（非空字符串）和返回 `False`，行为差别是什么？
   （答：返回 `'no'` 是**真值 → 异常被咽掉**；返回 `False` → 照常往上抛。解释器只看真假。）
7. `@contextmanager` 的函数里写了两个 `yield`，撞到的原文是什么？一个都没写呢？
   （答：两个 → `RuntimeError: generator didn't stop`；零个 → `TypeError: 'NoneType' object is not an iterator`。）
8. `suppress(ValueError)` 会咽掉 `KeyboardInterrupt` 吗？`suppress(BaseException)` 呢？
   （答：前者不会（`issubclass(KeyboardInterrupt, Exception)` → False）；后者会，所以永远别写。）
9. 100 万次里 `EAFP` 全部命中和 `LBYL` 全部命中，哪个快？反过来 EAFP 每次未命中呢？
   （答：命中时 EAFP 更快（0.026 vs 0.029 µs，LBYL 查了两遍哈希）；未命中时 EAFP 慢 4.75 倍。数字每跑必抖，读倍率。）
10. `except ... as exc` 里的 `exc` 在块外面还能用吗？`logger.exception()` 为什么必须在 except 块里调？
    （答：都不能 —— CPython 离开 except 块时执行 `del exc`；块外 `sys.exc_info()` 是 `(None, None, None)`。）
11. 进程里一个子线程抛了未捕获异常，主线程 `join()` 之后 `sys.exit` 正常返回：退出码是多少？
    （答：0。traceback 只打进 stderr，不改变退出码，`join()` 也不报错。）
12. 配置文件写到一半崩了，`with open(cfg, 'w')` 帮你保住了什么、没保住什么？正确写法是什么？
    （答：保住 close（数据交给 OS），没保住「要么全写要么不写」；正解是同目录临时文件 + `os.replace`。）
13. `json` 里读到的字段是 `None`，你怎么区分「值就是 None」和「键不存在」？两种答案分别对应本章哪一节？
    （答：`d.get(k, MISSING)` 哨兵（第 15 节第 7 段）或 `raise KeyError` 让调用方接（第 2 节第 3 段的反面）；
    返回值给 `None` 的那条写法是第 05 章第 12 节那条坑的另一面。）
14. 本章那个对账脚本为什么非要**现场造三个坏样本**？
    （答：不演示触发，你无法知道那条判据是不是恒真 —— 恒真守卫比没有更糟。第 20 节第 3 段。）

---

## 本章没演 / 没验证（绝不编造，都在这）

**语言机制类**

| 内容 | 状态 |
|---|---|
| `except*` 与 `BaseExceptionGroup` 的完整语义 | 只证明了**本机有、两个分支都跑**（第 3 节第 7 段）；嵌套组、`add_note`、非对称分组都没演 |
| `async with` / `asynccontextmanager` / `aclosing` / `AsyncExitStack` | 完全没演（本章全程同步）；`contextlib` 那 22 个名字里这几个一个没碰 |
| `ContextDecorator` / `AsyncContextDecorator`（把上下文管理器当装饰器用） | 没演 |
| `contextlib` 其余名字：`chdir`、`profiler` 类工具、`abc` 里那两个抽象基类 | 没演；`chdir` 会真改工作目录，不适合放进教材的定长输出里 |
| 迭代器协议 `__iter__` / `__next__` | **全手册都没演**（第 04 章第 17 节自己把它列在「没演」里），所以第 8、15 节只说「手写会付这份钱」 |
| 类体作用域 | 第 11 节第 7 段提了一句「类体也开一个作用域」，**没演** |
| `os.fsync` / 掉电语义 / `O_DIRECT` | 第 18 节只验到「close 之后另一个句柄读得到」，再往下一层**本机无法验证，不做结论** |
| TOCTOU 竞争（检查与使用之间被改动） | 方向说清了（第 15 节第 7 段②、第 18 节第 5 段），但**本机没起并发竞争**，那条不下结论 |
| 用假 `open` 之外的真实瞬时故障（NFS、EINTR、网络超时） | 第 19 节用**假 open** 造 EBUSY，真实网络一概不碰（红线） |
| `concurrent.futures` 把异常装进 Future | 没演，只在第 17 节第 6 段留了一句「正解」的方向 |
| `sys.excepthook` 的子线程版 `threading.excepthook` | 只验了**本机有这个名字**（True），没有装 hook 去演它 |
| 第三方日志框架、`logging.config`、结构化日志 | 第 16 节第 3 段只用了标准库 `logging` 的最小组合 |

**版本 / 环境类**

- 第 15 节所有 µs 数字**每跑必抖**，只有倍率的大小顺序可读（本机 4 核、`PYTHONHASHSEED=0`）。
- 第 7 节第 6 段那个 `[Previous line repeated N more times]` 的 **N 每跑都可能变**，脚本里已注明不能当证据引用。
- 第 11 节第 3 段那条「3.8 及以前把括号当元组」的老解释器行为：**本机只有 3.14，未验证**，别当事实引用。
- 第 12 节那条「`@contextmanager` 复用撞 `AttributeError`」的**成因**引用了标准库源码里那句
  `del self.args, self.kwds, self.func` —— 那是本机那份实现的读法，别的版本可能改。
- 第 15 节收口提到的社区说法「零成本异常（zero-cost exceptions）」：**从哪个版本起免费本章不验证**，
  只报本机 3.14 的实测。

---

## 怎么跑这一章

```bash
cd python-handbook/07-errors-and-context
bash demo.sh              # 21 节全跑，跑完自动删沙箱
bash demo.sh --keep       # 保留沙箱并把路径打给你，进去自己接着敲
bash demo.sh > out.txt 2>&1   # 重定向时自动关颜色，得到和 README 一样的纯文本
```

- **整章一次跑完约 10 秒**（第 15 节的计时占大头：它自己跑 100 万 / 10 万次）。
- 只想跑某一节：进沙箱单独跑那个脚本即可，例如 `python3 m_exitstack.py`。
  沙箱目录名每次随机（`/tmp/py07-XXXXXX`），所以 `demo.sh --keep` 打出来的那一行才是你该进的地方。
- 单独验本章的**闸门**：`python3 s_checklist.py` 合格退 0，`python3 s_checklist.py demo.sh` 退 1
  （第 20 节现场演过这两条）。这条就是本章能进 `make check` / CI 的部分。
- 故意不加 `set -e` / `set -o pipefail`：本章大半是「失败现场」，加了就只能看到第一段；
  `cmd | head -3` 这类截取会因为下游提前关管道报 141，屏幕上会冒出一堆和教学无关的退出码。
- 安全边界：不 `pip install`、不联网、不装东西、不开监听端口、不改 `~/.bashrc` / `~/.profile` /
  `~/.config` / `~/.local`；一切写操作都在 `mktemp -d` 的沙箱里，由 `trap cleanup EXIT` 整份删掉；
  第 18、19、20 节会当场建文件 / 删文件，也**只在沙箱**，跑完各自 `rmtree`，第 21 节负责核对没留下尸体。

---

## 术语小抄

| 术语 | 大白话 |
|---|---|
| EAFP / LBYL | 「先做，撞了再接」/「先查，查过再做」。第 15 节量的就是这两条的成本差 |
| `__cause__` | 你写 `raise X from Y` 时挂上去的「原因」，解释器从不替你填 |
| `__context__` | 「我手上正处理着这个」，处理期间又抛新异常时解释器自动记 |
| `__suppress_context__` | `from None` 把它设成 True：**链还在，只是不打印** |
| traceback（帧链） | 不是一个字符串，是链表：`tb_next` 指向下一帧，帧上有 `.name` / `.lineno` |
| exc_info 三元组 | `(类型, 实例, traceback)`。`sys.exc_info()`、`__exit__` 的三个参数、excepthook 收到的，都是它 |
| 鸭子类型协议 | `with` 只问「类字典里有没有 `__enter__` / `__exit__`」，不问出身、不要求继承任何基类 |
| 后条件（后置清理） | `@contextmanager` 里 `yield` 之后那段 —— 不包 `try/finally` 就会在块体崩时被整个跳过 |
| `enter_context` | 「替我把 `with x` 往后写」，退出时按登记的逆序还 |
| `callback` / `push` | 前者只能登记收尾（**Cannot suppress exceptions**），后者收 `__exit__` 签名的函数、返回真值能咽 |
| `pop_all` | 把整叠收尾登记**转手**给另一个 stack —— 让「谁负责关」变成一个可传手的值 |
| TOCTOU | Time-of-check / time-of-use：查过之后、用之前，世界可能已经变了 |
| 原子替换 | 同目录临时文件写完 + `os.replace`：任何时刻磁盘上只有「旧的完整」或「新的完整」两种状态 |
| 半截文件 | `with` 崩在写中途留下的残骸：已经交给 OS，但**结构上不是合法文件** |
| errno | `OSError` 一族的机器可读错误号（本机看到 2=ENOENT、17=EEXIST、16=EBUSY、21=ISDIR） |
| 静默失败 | 程序不报错、下一行照常执行、退出码 0 —— 第 16 节整节讲这个 |
| 零成本异常 | 社区对「不进 except 的 try 几乎不花钱」的叫法；本章只报本机实测，不追版本 |
| 正对照 | 一条检查先被**故意弄响一次**，再报 0 —— 否则那个 0 可能只是模式写错了（第 21 节） |

---

## 本章之后

- **第 08 章 `files-and-pathlib`**：本章第 18 节只演到「open / close / 原子替换」这一层。
  下一章接上 `pathlib`、编码与 `newline=` 到底管什么、`csv` / `json` 的完整接口 ——
  以及本章故意没碰的**文件名编码**（第 01 节固定 `LC_ALL` 但没讲文件名那套）。
- **第 09 章 `modules-and-venv`**：`import` 撞上的 `ModuleNotFoundError` 本章按「OSError 之外另一个窄门」
  提过一次（第 3 节），它的完整机制（`sys.path`、包、`if __name__ == "__main__"`）在那一章。
- **第 11 章 `concurrency`**：本章第 17 节第 6 段那条「子线程的异常不改变退出码」是并发代码第一大坑，
  `concurrent.futures` 装异常的写法、`redirect_stdout` 为什么在多线程里串台（第 13 节第 5 段），到那里才演。
- **数据结构那条线**：本章第 15 节「栈越深、抛一次越贵」正是你在 C 里给递归改迭代的那笔账的 Python 版；
  第 14 节 `ExitStack` 是**栈**，第 11 节那条 enter/exit 顺序律是**后进先出**，
  第 18 节「文件句柄是一次性迭代器」是**游标**——三处都能直接回指你手写过的 C 结构。

## 本章回指

| 本章哪里用到 | 出处 |
|---|---|
| 第 2 节第 3 段「返回 None 分不清失败」的另一面 | 第 05 章第 12 节（`return` 的真相：没写就是 `None`） |
| 第 3 节「`UnboundLocalError` 是 `NameError` 的儿子」 | 第 05 章（LEGB 与 `UnboundLocalError` 那节） |
| 第 8 节 三种 `AttributeError`（property / frozen / slots） | 第 06 章第 8、11、13 节 |
| 第 8 节「`KeyError` 消息用 repr 所以自带引号」 | 第 06 章第 6 节（`print`、f-string、容器里各走哪个） |
| 第 8、15 节「迭代器只能过一遍 / 手写 next 要付钱」 | 第 04 章第 17 节（生成器与惰性；迭代协议它自己列在「没演」） |
| 第 10 节「`__enter__` / `__exit__` 住在类字典里」 | 第 06 章第 23 节（半个上下文管理器，本章的钩子） |
| 第 11 节第 7 段「推导式才开新作用域」 | 第 04 章第 16 节（推导式变量泄不泄漏，实测过） |
| 第 12 节「`@contextmanager` 就是 `f = contextmanager(f)`」 | 第 05 章第 23 节（装饰器：`@X` 就是 `f = X(f)`） |
| 第 15 节收口「常见路径走正常返回」 | 第 05 章第 6、12 节（`None` 哨兵 / 多值打包成元组） |
| 第 16 节第 5 段「退出码是接口」 | bash 手册第 3 章（判断与退出码） |
| 第 17 节「递归限额是解释器数的」 | 第 05 章第 20 节（递归：限额是解释器数的） |
| 第 20 节「恒真守卫比没有更糟」 | 数据结构那本每一节的 `make check` 闸门 |
