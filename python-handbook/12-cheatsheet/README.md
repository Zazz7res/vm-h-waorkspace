# 第 12 章 · 全书速查：出活时翻的这一页

> 前十一章是把道理**演**给你看，这一章是你**以后每天上班翻**的那一页。
> 所以它的写法不一样：**没有大段解释，只有「你以为是啥 → 屏幕上实际是啥 → 现场在哪一章」。**
>
> 这一章不新跑取证脚本。表里每一行都指回前十一章某一节真跑过的现场，出处写在最后一列；
> 不信、或者忘了为什么，就按出处回去看那一段屏幕输出。
> 唯一当场跑的东西在本章自己的 `demo.sh` 里 —— 它把枪口对准**这一页本身**：
> 出处有没有断链、给出的 `python3` 骨架过不过得了 `ast.parse`、表里的硬事实现在还在不在。

---

## 你会遇到的场景

- 写 `def f(xs=[])`，不确定第二次调用会不会带上新东西 → **表 5**。
- 抄了一段 `with open(p, 'w')`，中途想反悔不写 → **表 8**。
- 换个目录跑就 `ModuleNotFoundError`，不知道该翻哪一章 → **表 1** + **表 9**。
- `a = b = []`、`[[0]*3]*3`、`dict.fromkeys(ks, [])` 这一族，分不清谁共享 → **表 2** + **表 4**。
- 想给并发的活收尾，不知道谁 `join`、谁 `shutdown`、谁塞哨兵 → **骨架 4** + **表 11**。
- 只加了一行注解就以为运行时能拦住传错类型 → **表 10**。
- 想在电梯里说清这本书讲了啥 → **一页纸（20 条）**。

## 怎么用这一章（三条规矩）

1. **出处记法**：`第 07 章 · 第 18 节` = 第 07 章第 18 节（就是那章 README 里的 `## 第 18 节 · 标题`）。
   全书的书名号不统一，一共四种形状：第 02、03 章写 `## 18. 标题`，第 06 章写 `## 18. 第 18 节 · 标题`，
   第 01 章写 `### 场景 7：……（demo 第 7 节）`，其余七章才是 `## 第 18 节 · 标题`。
   `demo.sh` 第 3 节里躺着这四条正则，四种形状都归到同一个「第 M 节」上 —— 它不背映射表，是**当场去那一章的 README 里找标题**。
   后面几章一节里还分格，就再追一格：`第 10 章 · 第 02 节第 1 格`，那一格是取证脚本当场印出的
   `=== 1. 标题 ===`（也有章节把「第 K 格」直接写进正文，两种都认）。表里**每一条**出处都被解析一遍：
   落在哪一章、那一节在不在、那一格在不在，全数出来 —— 断一条就报一条。
2. **表里没有的写法，多半是本书故意没给的。** 比如「`pip install` 一下就好了」「装个 mypy/ruff 再说」
   「`python` 这个命令」「free-threading 能把 GIL 关掉」—— 它们都在各章的「本章没演」里被点名过，
   别从这一页反过来猜「应该也可以吧」。
3. **这一页的守卫全部能响**。`demo.sh` 每条断言在报 0 之前，都先被人为弄坏一次
   （塞一个不存在的章号、删掉骨架里的一个冒号、造一份必然五类全命中的脏副本、在章目录的同形状副本里
   真 `import` 出一份 `__pycache__`），响过之后才报正常值。**恒 0 的守卫比没有守卫更糟** ——
   这是全书纪律，不是口号（第 07 章 · 第 20 节、第 08 章 · 第 20 节、第 11 章 · 第 14 节 各演过一次）。

## 本章钉在哪台机器上（第 1 节当场探）

全书每一章都写「本机」，这一页把「本机」压成一次探测。值全部由 `python3 -c` 当场取，
没有一个字是背来的 —— 换一台机器（有 `python3.14t`、有 pip、默认 start method 不是 forkserver），
这张地板就得重打，本章的结论也要跟着重测。

先探七行「运行规律」相关的地板：

```
$ timeout 60 python3 -c "
import sys, sysconfig, os, selectors, multiprocessing as mp, importlib.util as u
print(\"版本 →\", sys.version.split()[0], \"｜ 实现 →\", sys.implementation.name, \"｜ 前缀 →\", sys.prefix != sys.base_prefix)
print(\"运行期 GIL 开着吗 sys._is_gil_enabled() →\", sys._is_gil_enabled(), \"｜ 编译期 Py_GIL_DISABLED →\", sysconfig.get_config_var(\"Py_GIL_DISABLED\"))
print(\"切换节拍 sys.getswitchinterval() →\", sys.getswitchinterval(), \"｜ 核数 os.cpu_count() →\", os.cpu_count(), \"｜ 可用核 sched_getaffinity →\", len(os.sched_getaffinity(0)))
print(\"multiprocessing 全部 start method →\", mp.get_all_start_methods(), \"｜ 默认 →\", mp.get_start_method())
print(\"selectors.DefaultSelector 是谁 →\", type(selectors.DefaultSelector()).__name__)
print(\"find_spec(\\\"pip\\\") →\", u.find_spec(\"pip\"), \"｜ which python3.14t →\", __import__(\"shutil\").which(\"python3.14t\"))
print(\"os.fork 在场吗 →\", hasattr(os, \"fork\"), \"｜ asyncio.to_thread 在场吗 →\", __import__(\"asyncio\").to_thread is not None)
"
版本 → 3.14.4 ｜ 实现 → cpython ｜ 前缀 → False
运行期 GIL 开着吗 sys._is_gil_enabled() → True ｜ 编译期 Py_GIL_DISABLED → 0
切换节拍 sys.getswitchinterval() → 0.005 ｜ 核数 os.cpu_count() → 4 ｜ 可用核 sched_getaffinity → 4
multiprocessing 全部 start method → ['forkserver', 'fork', 'spawn'] ｜ 默认 → forkserver
selectors.DefaultSelector 是谁 → EpollSelector
find_spec("pip") → None ｜ which python3.14t → None
os.fork 在场吗 → True ｜ asyncio.to_thread 在场吗 → True
[退出码 0]
```

再探一份名单。★ 两列必须由**同一份候选名单**切出来：前几章各探各的四五个名字，于是屏上只说得出
「那几把尺都不在」；这一页是全书收口，得把话说全 —— 本机确实没有 `pip`，但 `dist-packages`
里躺着的不止 `typeguard` 一个。

```
$ timeout 60 python3 -c "
import sys, os
import importlib.util as u, sysconfig
names = sorted([\"pip\",\"setuptools\",\"mypy\",\"pyright\",\"ruff\",\"black\",\"flake8\",\"pylint\",\"pytest\",
                \"uvloop\",\"trio\",\"curio\",\"numpy\",\"pandas\",\"requests\",\"rich\",\"yaml\",\"typeguard\"])
have = [m for m in names if u.find_spec(m) is not None]
print(\"候选名单共 %d 个名字 ｜ 在场的 →\" % len(names), have)
print(\"缺席的 →\", [m for m in names if u.find_spec(m) is None])
root = sysconfig.get_paths()[\"purelib\"]
print(\"这些在场的是从哪儿来的 →\", [os.path.dirname(u.find_spec(m).origin) for m in have])
print(\"标准库纯 Python 目录是 →\", root, \"｜ 它在 sys.path 里吗 →\", root in sys.path)
"
候选名单共 18 个名字 ｜ 在场的 → ['requests', 'rich', 'setuptools', 'typeguard', 'yaml']
缺席的 → ['black', 'curio', 'flake8', 'mypy', 'numpy', 'pandas', 'pip', 'pylint', 'pyright', 'pytest', 'ruff', 'trio', 'uvloop']
这些在场的是从哪儿来的 → ['/usr/lib/python3/dist-packages/requests', '/usr/lib/python3/dist-packages/rich', '/usr/lib/python3/dist-packages/setuptools', '/usr/lib/python3/dist-packages/typeguard', '/usr/lib/python3/dist-packages/yaml']
标准库纯 Python 目录是 → /usr/local/lib/python3.14/dist-packages ｜ 它在 sys.path 里吗 → True
[退出码 0]
```

这两块合起来才是本章的边界：**没有 pip 意味着「装一个就好了」这句话在本手册全程兑现不了**，
而在场的那五个包（`requests` / `rich` / `setuptools` / `typeguard` / `yaml`）都是发行版带来的，
本章的表与骨架**一个字都没用到它们**。

对照本手册根目录 `README.md` 里那张「本机环境」表：解释器 `3.14.4`、`python` 不存在、
`pip` 不存在、`EXTERNALLY-MANAGED` 在场、`python3 -m venv` 因缺 `ensurepip` 而失败。
**这一页不重复抄那张表，只声明一件事：下面 11 张表里的每个读数，都只在上面这块地板上成立。**

---

## 表 1 · 解释器与启动（第 01 章）

| 直觉以为什么 | 屏幕上实际是什么 | 出处 |
|---|---|---|
| `python3 x.py`、`-m`、`-c` 看到的搜索路径是同一份 | `sys.path[0]` 由**启动方式**决定：脚本给脚本目录、`-m` 给当前目录、`-c`/stdin 给空串。于是「我这能跑、他那跑不了」多半不是语法问题 | 第 01 章 · 第 3 节 + 第 09 章 · 第 4 节 |
| 管道里看不到输出 = 程序卡住了 | 不是卡住，是**块缓冲**：`isatty()` 为假时换行不 flush，写了 9000 字节带 4500 个换行照样 0 字节可见 | 第 01 章 · 第 5 节 |
| `python3 -u` 就是行缓冲 | `-u` 改的是 `write_through`，`line_buffering` 仍是 `False`。想判缓冲看属性，别猜 | 第 01 章 · 第 5 节 |
| 报错能立刻看到，进度也能 | stderr 恒行缓冲、stdout 不恒 —— 日志里「只有报错没有进度」是正常现象 | 第 01 章 · 第 5 节 |
| `sys.argv[0]` 就是脚本绝对路径 | 它可能是 `'work/path0.py'`（相对）、`'-c'`、`''`。要路径用 `__file__`，还要记得 stdin 启动时它是 `<stdin>` | 第 01 章 · 第 1 节 |
| 改完源码再跑，缓存会自动跟上 | **主脚本永远不写 pyc**，只有被 import 的才写；hash-based pyc 根本不看时间戳，`unchecked-hash` 档连源码都不读 | 第 01 章 · 第 8 节 + 第 01 章 · 第 2 节 |
| `assert` 是校验手段 | `-O`/`-OO` 下整条消失，`-OO` 连 `__doc__` 都变成 `None`。生产一律 `-O` 是个危险建议 | 第 01 章 · 第 8 节 + 第 07 章 · 第 6 节 |
| 本机有 `python`，也有 pip | 只有 `python3`：那一格 `command -v python python3 python3.14 py` 只回 `/usr/bin/python3`、`/usr/bin/python3.14`。至于 pip 没装、`compileall` 没有 `--force` 这个长选项（屏幕上只出现过小写 `-f` 那次成功），第 01 章把这两条写在**正文**里，没让它们上屏；pip 缺席在本手册只于本章地板那一格露过原文（`find_spec("pip") → None`） | 第 01 章 · 第 4 节 + 第 01 章 · 第 2 节 |

## 表 2 · 类型与变量（第 02 章）

| 直觉以为什么 | 屏幕上实际是什么 | 出处 |
|---|---|---|
| 赋值把值搬过去 | 名字是标签，对象才是东西（字节码叫 `STORE_NAME`）。`a = b = []` 是**一个对象两张标签**，函数默认参数是它的孪生兄弟 | 第 02 章 · 第 2 节 + 第 02 章 · 第 15 节 |
| 不可变对象就安全 | 不可变只管**浅层**：`+=` 对 list 原地扩、对 str 换新对象；`t[1] += [20]` 会报错**但内层已经改了** | 第 02 章 · 第 5 节 + 第 02 章 · 第 6 节 |
| `257 is 257` 一定是 False | 网上抄的这条在本机复现不了：同一段代码里的两个字面量被折成**同一个常量** → `True`。缓存区间 `-5..256 共 262 个` 要用**当场算出来**的对象才扫得出来，第 02 章那支扫描器的探针就是 `n + 0`（所以它问的是 `n + 0 is n + 0`） | 第 02 章 · 第 7 节 |
| `is` 比 `==` 快，所以拿它比数字 | 3.14 直接给你 `SyntaxWarning`，`-W error` 下变 `SyntaxError`。判值永远写 `==` | 第 02 章 · 第 8 节 |
| `if not x` 就是判空 | `0`、`''`、`[]`、`None` 一视同仁。要区分「没传」和「传了空」，只写 `is None` | 第 02 章 · 第 11 节 |
| `1`、`True`、`1.0` 是三个不同的键 | 在 dict 里是**同一个键**（`-0.0` 和 `0.0` 也是）。后写的覆盖先写的 | 第 02 章 · 第 10 节 + 第 02 章 · 第 12 节 |
| 切片 / `list()` / `dict()` 是复制 | 全是**浅**的：改内层两边一起变。只有一整棵树都要新的才用 `deepcopy` | 第 02 章 · 第 20 节 |
| 「先用后写」报的是赋值那一行 | `UnboundLocalError`，而且报错行是**读**那一行 —— 编译器只看这个名字有没有被赋值过 | 第 02 章 · 第 18 节 |

## 表 3 · 字符串与文本（第 03 章）

| 直觉以为什么 | 屏幕上实际是什么 | 出处 |
|---|---|---|
| `len(s)` 是字数或字节数 | `len()` 数的是**码点**。本机 `'中文字符串'` 三个数是 5 / 15 / 10（码点 / UTF-8 字节 / 显示格），谁都不是 `len` 一个能管的 | 第 03 章 · 第 1 节 + 第 03 章 · 第 22 节 |
| é 就是 é | 两种写法看起来一模一样、`==` 是 False、`len` 是 4 或 5。要比 Unicode 等价必先 `normalize('NFC', …)` | 第 03 章 · 第 2 节 + 第 03 章 · 第 3 节 |
| `split()` 就是 `split(' ')` | 不带参数吃连续空白且**不出空串**；带 `' '` 则首尾都是空串。`maxsplit` 砍完剩下的整段留在最后一项里 | 第 03 章 · 第 17 节 |
| 按行切就用 `split('\n')` | `splitlines()` 认 11 种换行；`split('\n')` 只认 LF，`\r` 那份会被留在串里 | 第 03 章 · 第 18 节 |
| `strip('pre_')` 去掉前缀 | `strip` 删的是**字符集合**，不是前缀；砍前后缀只有 `removeprefix` / `removesuffix` | 第 03 章 · 第 20 节 |
| `f"{'中':>6}"` 对齐 6 格 | 补的是 6 个**码点**不是 6 格。中文表格歪掉的原因永远是 `east_asian_width` 没参与 | 第 03 章 · 第 12 节 + 第 03 章 · 第 22 节 |
| `b"中文"` 只是编码写法不同 | 编译期就拒绝：bytes 字面量只许装 ASCII，一个语法都过不了 | 第 03 章 · 第 1 节 |
| 循环里 `s += x` 拼串没什么代价 | 二次方。收集可变文本只有一种写法：`list` 收集 + `''.join(parts)` | 第 03 章 · 第 16 节 + 第 03 章 · 第 19 节 |

## 表 4 · 容器（第 04 章）

| 直觉以为什么 | 屏幕上实际是什么 | 出处 |
|---|---|---|
| `b = a[:]` 算复制了一份 | 复制有六级梯子，前五级（贴标签 / 切片 / 构造器 / `.copy()` / `copy.copy`）**全是浅的**，只有 `deepcopy` 重造内层 | 第 04 章 · 第 9 节 |
| `[[0]*3]*3` 是个 3×3 矩阵 | 内层只有**一个**对象，改一格三行一起变。有几层独立就要写几个 `for`；`dict.fromkeys(ks, [])` 同理 | 第 04 章 · 第 10 节 |
| 边遍历边删会报错 | list **不报错，只给错答案**（`[2,4,6,8,10]` 删偶数剩 `[4, 8]`）；dict / deque 改大小才当场 `RuntimeError`，且发生在**下一次迭代**那一刻 | 第 04 章 · 第 11 节 |
| `xs.sort()` 返回排好的表 | 返回 `None`；要那个排好的东西就用原来的名字。`sort` **稳定**，`key=` 每个元素只算一次 | 第 04 章 · 第 13 节 + 第 04 章 · 第 14 节 |
| `{}` 是空集合、`(1)` 是单元素 tuple | `{}` 是空 **dict**（空 set 只能 `set()`）；`(1)` 是 int，**逗号**才是 tuple 的标志 | 第 04 章 · 第 15 节 |
| dict 的遍历顺序是哈希序 | 遍历 = **插入顺序**；删除后再插排到末尾；覆盖值不挪位置；`==` 与顺序无关 | 第 04 章 · 第 15 节 |
| `Counter(d)[缺键]` 会 KeyError | 返回 `0` 且**不建键**（`.get()` 反倒返回 `None`）。`defaultdict` 正相反：读一下缺失键就真的建键，于是边遍历边读它 → `RuntimeError` | 第 04 章 · 第 18 节 + 第 04 章 · 第 19 节 |
| 生成器第二次遍历只是慢一点 | 第二次是**空且不报错**，`sum` 给你 `0`。`map`/`filter`/文件对象全都是一次性的 | 第 04 章 · 第 17 节 |

## 表 5 · 函数（第 05 章）

| 直觉以为什么 | 屏幕上实际是什么 | 出处 |
|---|---|---|
| 默认值每次调用重新算 | `def` **只跑一次**：默认值的表达式在定义那一刻就被算掉了，`f.__defaults__` 里就是那个唯一对象 | 第 05 章 · 第 4 节 + 第 05 章 · 第 5 节第 1 格 |
| `def f(xs=[])` 只是不优雅 | 三次调用账越攒越多，而且**一个字都不报错**。判据不是「类型可不可变」，是「没有任何一条路径能原地改它」 | 第 05 章 · 第 5 节 |
| 传进去的 list 改了外面看不见 | 传参 = **一次赋值**（贴名字）。只有原地改（`.append` / `[i]=` / `+=`）外面才看得见 | 第 05 章 · 第 7 节 |
| `return 1,` 返回 1 | 返回 `(1,)`：逗号造元组，括号不管事。第 05 章那一节屏幕上只有 `x = 1, → (1,)` 这一格读数；「3.14 连警告都不给」是第 04 章的原话（写法完全合法，所以它是这类 bug 的第一名来源） | 第 05 章 · 第 12 节 + 第 04 章 · 第 15 节 |
| `[lambda: i for i in range(3)]` 三个函数各带一个数 | 三个都是 `2`：`i` 是**调用时才查**的名字，三个函数共用同一个盒子（晚绑定） | 第 05 章 · 第 14 节 |
| `locals()` 是个能写的字典 | 函数里往里写**不创建变量**；能写的是 `globals()`。`co_varnames` / `inspect.signature` 才是查证用的尺 | 第 05 章 · 第 16 节 |
| 尾递归会被优化成循环 | CPython 不做尾调用优化，累加版照样在约 1000 层炸；临时调高 `setrecursionlimit` 是借栈空间，用完必须改回去 | 第 05 章 · 第 20 节 |
| 装饰器是一门新语法 | `@X` 就是 `f = X(f)`。忘写 `@functools.wraps` 会吞掉 `__name__`/`__doc__`/签名；带参数的装饰器忘括号，报错是 `deco() missing 1 required positional argument: 'fn'` | 第 05 章 · 第 23 节 + 第 05 章 · 第 23 节第 1 格 |

## 表 6 · 类与 dataclass（第 06 章）

| 直觉以为什么 | 屏幕上实际是什么 | 出处 |
|---|---|---|
| 写 `__eq__` 只是改比较 | 解释器**主动**把 `__hash__` 设成 `None`：实例进 set、当 dict 键全部当场 `TypeError`，而且是三种不同原文 | 第 06 章 · 第 7 节 |
| `@dataclass` 就是不可变 / 能排序 | 默认没有 `<`（`TypeError`）、默认能改字段；`frozen=True` 只挡字段**赋值**，还白送 `__hash__` | 第 06 章 · 第 11 节 + 第 06 章 · 第 12 节 |
| dataclass 里可以写 `items: list = []` | 定义那一刻就 `ValueError: … mutable default … use default_factory` —— 好消息。手写 `def __init__(self, items=[])` 可没人拦你 | 第 06 章 · 第 10 节 |
| `self.x += 1` 改的是类变量 | 改不动，只在实例上建了个**遮挡物**，类上那份永远是 `0`；可变类变量则是全体一起脏 | 第 06 章 · 第 17 节 |
| `__repr__` 和 `__str__` 互为退路 | 只有 `__repr__` 时 `str()` 会退回它；**只有 `__str__` 时 `repr()` 绝不退回它**，于是容器里看到的一律是默认 repr | 第 06 章 · 第 6 节 |
| `__private` 是访问控制 | `v._Vault__secret` 直接拿到。它只防父子撞名，而且跨类报错里的名字跟着**写代码的类**改 | 第 06 章 · 第 20 节 |
| `super()` 找谁由「这段代码写在哪」定 | 由**实例的 MRO** 定。钻石继承里那条名单才是账 | 第 06 章 · 第 15 节 + 第 06 章 · 第 16 节 |
| 写了 `__slots__` 就没有 `__dict__` 了 | 父类没写 `__slots__`，子类写了**也白干**（`__dict__` 继承下来）；`slots=True` 之后 `vars(实例)` 报 `TypeError` | 第 06 章 · 第 13 节 + 第 06 章 · 第 14 节 |

## 表 7 · 异常与 with（第 07 章）

| 直觉以为什么 | 屏幕上实际是什么 | 出处 |
|---|---|---|
| `else` 和 `finally` 差不多 | 走位图只有一张：`else` = 没炸才走；`finally` = 离开那一刻必执行。`finally` 里 `return` 会把异常和返回值**一起吞掉**，只在编译那一刻发一条 `SyntaxWarning` | 第 07 章 · 第 2 节 |
| `except Exception` 兜一切 | 它接不住退出族（这是好事）；反过来说**裸 `except:`** 会把 `KeyboardInterrupt` / `SystemExit` 一起咽掉。要接一切就明写 `except BaseException:` 并给理由 | 第 07 章 · 第 3 节 + 第 07 章 · 第 16 节 |
| `except` 分支顺序无所谓 | 父亲在前，儿子那条分支永远变死代码，**不报错、不警告** | 第 07 章 · 第 4 节 |
| `__cause__` 和 `__context__` 是一回事 | 前者是你 `raise ... from ...` 写的，后者是解释器记的现场。看到报错先翻 `__context__` —— 收尾代码自己抛会把原始故障降级成它 | 第 07 章 · 第 5 节 + 第 07 章 · 第 14 节 |
| `with` 保证数据不丢 | `with` 只保证 `close()`，不管「写全」。配置文件一律「同目录临时文件 + `os.replace`」 | 第 07 章 · 第 18 节 + 第 08 章 · 第 17 节 |
| `__exit__` 返不返回都一样 | 返回**任何真值** = 异常就地抹掉，写字符串也算真值。第 07 章给这一格的原话是「本章最危险的一行代码」；全书「一个字都不报错」的坑不止这一条（表 4 的边遍历边删、表 5 的可变默认参数都是同一类） | 第 07 章 · 第 10 节 |
| 日志里存 `str(exc)` 就够了 | `KeyError` 那种只剩一个带引号的键名，类型和行号全丢。要么 `logging.exception(...)`，要么 `exc_info=True` | 第 07 章 · 第 16 节 |
| 子线程里没接住的异常会让程序失败 | 不改退出码：线程崩了，`join()` 也不报错，进程退 `0` | 第 07 章 · 第 17 节 |

## 表 8 · 文件、编码与路径（第 08 章）

| 直觉以为什么 | 屏幕上实际是什么 | 出处 |
|---|---|---|
| `open(p, 'w')` 到写的时候才动文件 | **打开那一刻就截成 0 字节**，进 `with` 块再反悔已经晚了，而且全程没有一条报错提醒你。`'x'` 撞 `FileExistsError` 时旧内容才还在 | 第 08 章 · 第 2 节第 2 格 |
| 写完就进内核了，`flush()` 就落盘了 | 三档：`write` 只进用户态 buffer；`flush` 只是交给内核（内核没承诺落盘）；`fsync` 才是那句「请求内核别丢」 | 第 08 章 · 第 17 节 + 第 08 章 · 第 3 节 |
| 默认编码是 UTF-8 | 默认编码有**四个来源**；用 `latin-1` 读一份 UTF-8 文本一个错都不报（它是 0..255 的一一映射），还能原样编回去。BOM 就是 U+FEFF 这个字符本身，读 Windows 文件用 `utf-8-sig` | 第 08 章 · 第 6 节 |
| `newline=''` 就是「什么都不翻译」 | 读侧只有 `newline=None`（通用换行）会**改内容**；`csv` 要 `newline=''` 是交给它自己管行尾，显式写 `\r\n` 会和文本层叠成 `\r\r\n` | 第 08 章 · 第 5 节 + 第 08 章 · 第 15 节 |
| `Path('/tmp') / '/etc/passwd'` 拼出一个子路径 | 得到 `/etc/passwd`：前缀被整个丢掉且**不报错**，`os.path.join` 两条路一样。`os.path.relpath` 能给你一个「看起来在目录里」的串，安全前缀校验要用会响的 `relative_to` | 第 08 章 · 第 9 节 |
| `resolve()` 和 `absolute()` 一样 | `resolve()` 问内核（跟着符号链接、能发现不存在），`absolute()` 与 `normpath` 是**纯字符串算术**。判「同一个文件」用 `samefile`，它比 `(st_dev, st_ino)` 这一对 | 第 08 章 · 第 14 节 + 第 08 章 · 第 11 节 |
| 临时文件就 `tempfile.mktemp()` 然后 `open()` | `mktemp` 只算名字不创建，中间有时间窗，docstring 原文写着 `THIS FUNCTION IS UNSAFE AND SHOULD NOT BE USED.`；`mkstemp` 硬编码 `0o600` 并带 `O_CREAT\|O_EXCL` | 第 08 章 · 第 13 节 |
| JSON 存类型 | 值能回来、**类型不能**：tuple → list、dict 的 int 键 → str；set / bytes / date 当场 `TypeError`。坏的一侧抛 `JSONDecodeError`（带 lineno/colno/pos），合法的一侧一个错都不报 | 第 08 章 · 第 16 节第 1 格 |

## 表 9 · 模块、包与 venv（第 09 章）

| 直觉以为什么 | 屏幕上实际是什么 | 出处 |
|---|---|---|
| 改了源码再 import 一次就生效 | `sys.modules` 命中就**连文件都不打开**：改的是磁盘、查的是账本 | 第 09 章 · 第 3 节 |
| `reload` 会把对象换掉 | 它只重跑模块体；你手里那句 `from mymod import VALUE` 绑的是当时的引用，**一个都不换** | 第 09 章 · 第 15 节 |
| 同名文件能顶掉任何标准库 | 磁盘上的标准库（`json`、`csv`、`select`…）会被 `sys.path[0]` 抢走；`time`、`sys` 这些**内置/冻结层压根不在 `sys.path` 上**，遮蔽不了 | 第 09 章 · 第 5 节 |
| 报错会告诉我它查过哪些目录 | `ModuleNotFoundError` 只提模块名，**不提任何一条搜索路径**。第一步是把 `sys.path` 摊开按层问，不是改代码 | 第 09 章 · 第 6 节 |
| pyc 靠内容哈希失效 | 头部那 16 字节里的两个数是**源码 mtime + 源码字节数**；它买到的是「省掉 parse/compile」，不是「省掉执行」 | 第 09 章 · 第 11 节第 2 格 + 第 09 章 · 第 12 节 |
| 循环 import 是「有环」这个结构错了 | 炸的是**模块体的先后顺序**：同一个环、四种取法给四种不同原文；同一个环换一种入口就换一种生死。`reload` 救不了 | 第 09 章 · 第 9 节 + 第 09 章 · 第 10 节 |
| `python3 包/子.py` 里也能相对导入 | `.` 只看 `__package__` 这个**名字**，脚本跑法里它是 `None`。正道是 `-m`，或者把入口写到包外面 | 第 09 章 · 第 7 节第 5 格 |
| 本机 `python3 -m venv 目录` 能建出带 pip 的环境 | 缺 `ensurepip`，原文失败 + 退出码 1；`--without-pip` 才建得出来。所以全书没有任何一句「装完包之后会怎样」 | 第 09 章 · 第 16 节第 1 格 |

## 表 10 · 注解与工具（第 10 章）

| 直觉以为什么 | 屏幕上实际是什么 | 出处 |
|---|---|---|
| 注解里名字写错，`def` 那一行就报错 | 定义**照样成功**：那一刻只编译出一个 `__annotate__` 取值函数。真正炸的是第一次**读** `__annotations__`——那一格里 ④⑤⑥ 三条读数逐字相同，原文都是 `NameError: name 'Widget' is not defined`；至于「炸在哪一帧」，第 10 章没让 traceback 正文上屏，所以本章不替它编 | 第 10 章 · 第 02 节第 1 格 |
| `hasattr(f, '__annotations__')` 是安全探针 | 它只吞 `AttributeError`，注解求值抛的 `NameError` 照原样往上抛。凡读注解必先 `try` | 第 10 章 · 第 02 节第 1 格 |
| 读成功过一次，下次就稳了 | **成功才缓存、失败不缓存**，所以每次读都会再炸一遍（那节拿计数器量的） | 第 10 章 · 第 02 节第 2 格 |
| `from __future__ import annotations` 更安全 | 它改的是**求值时机**，不是可读性也不是安全性：开了之后整份文件的注解全是文本 | 第 10 章 · 第 03 节 |
| 读注解的默认口径最稳 | `annotationlib.Format` 四把钥匙插进同一支坏注解的函数给四种东西，而 `get_annotations()` 的**默认**那把恰好是会炸的那把 | 第 10 章 · 第 04 节第 1 格 |
| 写了注解运行时就会检查 | 一行都不检查：`Final` 不拦再赋值、`Protocol` 的 `isinstance` **只查方法名不查签名**；真在干活的只有 `get_origin` / `get_args` 那批 | 第 10 章 · 第 08 节 + 第 10 章 · 第 09 节第 1 格 |
| `typeguard` 能顶替 mypy | 它是运行时插桩，只覆盖它插得到的调用点，默认策略**只查容器第一个元素**（`[1,'x']` 过、`['x',1]` 拦） | 第 10 章 · 第 10 节第 4 格 |
| 装个静态检查器是前提 | 本机 mypy / pyright / ruff / black / flake8 / pylint / pytest **全缺席，连 `pip` 都没有**；「类型检查」这一路本机一个都没有（发行版另外装了 `requests` / `rich` / `setuptools` / `typeguard` / `yaml`，但没有一个管类型）。真能用的标准库手段是 `compile()` / `ast` / `annotationlib` / `warnings` | 第 10 章 · 第 01 节 + 第 10 章 · 第 12 节 |

## 表 11 · 并发（第 11 章）

| 直觉以为什么 | 屏幕上实际是什么 | 出处 |
|---|---|---|
| Python 有 GIL，所以不能并发 | GIL 管的是「一次让谁跑字节码」。等待时它当场撒手（睡觉、堵空管道两种现场都 True），算死时才收税 | 第 11 章 · 第 02 节第 4 格 |
| 一行语句在 GIL 下是原子的 | `box["n"] += 1` 被拆成 13 条字节码，读和写之间隔着好几条 | 第 11 章 · 第 02 节第 2 格 |
| GIL 下 `+=` 一定丢更新 | 本机 4 线程 × 20 万次**纯 `+=` 一条不丢**（默认节拍和调细两种口径都不丢）；改成「读 → 让出 → 写」才结构性必丢，加锁才谈得上「一条不丢」 | 第 11 章 · 第 04 节第 5 格 + 第 11 章 · 第 04 节第 6 格 |
| 开 4 个线程就能用满 4 个核 | 不能：人多买来的是一起排队，时长跟着活一起涨。`os.cpu_count()` 是 4，不等于「4 倍速」 | 第 11 章 · 第 03 节 |
| `multiprocessing` 默认和 `fork` 一样 | 本机默认 **forkserver**，它把主模块整份重新 import：没有 `if __name__` 守卫 → 当场 `RuntimeError` 原文；函数藏在守卫里面 → `AttributeError: module '__mp_main__' has no attribute ...` | 第 11 章 · 第 06 节第 2 格 + 第 11 章 · 第 06 节第 3 格 |
| `f()` 调用了协程函数，就跑起来了 | 跑了 **0 行**：只生产一个协程对象，`await` 才有副作用；只创建不 await，GC 后漂出 `coroutine ... was never awaited` | 第 11 章 · 第 09 节第 1 格 |
| `submit` 之后异常当场抛 | 躺在 future 里，`.result()` 才抛原文；`map` 在迭代时抛、`as_completed` 一人一份。线程池的 `__cause__` 是 `None`，进程池挂 `_RemoteTraceback` | 第 11 章 · 第 10 节第 6 格 |
| `queue` 的消费者会自己结束 | 不塞哨兵就是**永久等待**，而卡住不会打印任何东西（外面必须套 `timeout` 才看得见） | 第 11 章 · 第 12 节 |

---

## 四段能直接抄的骨架

这四段是本章唯一「成段」的代码，每一段都由 `demo.sh` 第 5 节**从本页的围栏里原地抽出来**，
在内存里过一遍 `ast.parse` + `compile()`（不落盘、不 import、不执行）。
它们只是**形状**，别当完整程序抄：语义正确性靠各章那一节的现场。

**1. 一个脚本开头该有的那几行**（第 01 章 · 第 1 节、第 05 章 · 第 12 节、第 07 章 · 第 17 节）

```python
#!/usr/bin/env python3
"""一句话说清这脚本干什么。退出码就是它跟 CI / make / xargs 说话的唯一的嘴。"""
import sys


def main(argv: list[str]) -> int:
    if "--verbose" in argv:
        print("开 VERBOSE", file=sys.stderr)      # 日志走 stderr，stdout 留给被管道吃的数据
    return 0                                      # 0 = 干完了；非 0 必须有自己的理由


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))                  # 没写这行：被 import 时整份模块体照跑一遍
```

**2. 批量读文件 / 读 JSON 的姿势**（第 08 章 · 第 2 节、第 07 章 · 第 18 节、第 04 章 · 第 17 节）

```python
import json
from pathlib import Path


def read_json(path):
    """EAFP：直接开，不在前面加 exists() 把关。坏 JSON 抛 JSONDecodeError（带行、列、字符偏移）。"""
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))   # 编码写死，别赌默认值
    except FileNotFoundError:
        return None                              # 「没这个文件」和「文件是空的」是两件事


def read_lines(path):
    """大文件只能逐行；文件对象是**一次性**迭代器，第二次 list(f) 给空表且不报错。"""
    with open(path, encoding="utf-8", newline="") as f:   # newline="" 才不改内容
        return [line.rstrip("\n") for line in f]


def write_json_atomic(path, data):
    """原子替换唯一的姿势：同目录临时文件 → flush → fsync → os.replace。"""
    import os
    import tempfile
    target = Path(path)
    fd, tmp = tempfile.mkstemp(dir=str(target.parent), prefix=target.name + ".")
    try:
        with open(fd, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False)
            f.flush()
            os.fsync(f.fileno())
        os.replace(tmp, path)
    except BaseException:
        os.unlink(tmp)                           # 失败也别把 .tmp 留在原地
        raise
```

**3. 最小可用 CLI**（第 05 章 · 第 9 节、第 07 章 · 第 8 节、第 02 章 · 第 6 节）

```python
import argparse
import sys


def build_parser():
    p = argparse.ArgumentParser(description="举例：把名单里的 JSON 逐份读出来")
    p.add_argument("paths", nargs="+", help="至少一个文件；nargs='*' 会让 0 个也合法")
    p.add_argument("--limit", type=int, default=0)      # 0 = 不限；别用 None 当「没传」的假朋友
    p.add_argument("--strict", action="store_true")
    return p


def run(paths, limit=0, strict=False):
    bad = []
    for i, path in enumerate(paths, start=1):           # 从 1 开始数，报错里才说得出第几份
        if limit and i > limit:
            break
        if strict:
            pass
    return bad


if __name__ == "__main__":
    args = build_parser().parse_args()
    problems = run(args.paths, limit=args.limit, strict=args.strict)
    for item in problems:
        print(item, file=sys.stderr)                    # 报错别混进 stdout
    sys.exit(1 if problems else 0)
```

**4. 并发收口：谁 join、谁 shutdown、谁塞哨兵**（第 11 章 · 第 10 节、第 11 章 · 第 12 节、第 07 章 · 第 14 节）

```python
import queue
import threading
from concurrent.futures import ThreadPoolExecutor, as_completed


def with_pool(items, workers=4):
    """能用 with 就用 with：它负责 shutdown(wait=True)。max_workers 只是上限，不是承诺。"""
    with ThreadPoolExecutor(max_workers=workers) as ex:
        futures = [ex.submit(do_one, it) for it in items]
        results = []
        for fut in as_completed(futures):               # as_completed 给完成序，map 给投递序
            results.append(fut.result())                # 异常攒到这一刻才抛
    return results


def with_queue(items, workers=4):
    """自己起线程 + queue 时，收尾是**哨兵**的活：每个工人吃到一个 None 才会退出。"""
    q = queue.Queue()
    for it in items:
        q.put(it)
    for _ in range(workers):
        q.put(None)                                     # 少塞一个，就有一条线程永远等
    threads = [threading.Thread(target=consume, args=(q,)) for _ in range(workers)]
    for t in threads:
        t.daemon = True                                 # ★ daemon = 可以被硬切，别拿它当「自动收尾」
        t.start()
    for t in threads:
        t.join()                                        # 没 join：主流程退了，账也没人对
    return q


def do_one(item):
    return item


def consume(q):
    while True:
        item = q.get()
        if item is None:                                # 哨兵：拿到就收摊，并把它还回去给下一个工人
            q.put(None)
            return
        do_one(item)
```

★ 四段里唯一不能省的一条纪律：**别在模块体里干活**。第 4 段那两个函数只被调用、不被执行，
是因为 `multiprocessing` 的 forkserver 会把主模块**整份重新 import 一遍**（第 11 章 · 第 06 节第 2 格）。

---

## 一页纸（值得背下来的 20 条）

```text
1   赋值是贴标签不是搬值：a = b = [] 是一个对象两张标签；判「有没有共享」用 id 对比，不靠肉眼。
2   可变/不可变只管浅层：+= 对 list 原地、对 str 换新；tuple 里塞 list 照样能改内层。
3   判值一律写 ==，判单例才写 is；小整数缓存是 -5..256，同一段代码里的字面量会被折成同一个常量。
4   1 / True / 1.0 在 dict 里是同一个键；nan 跟谁都不相等，包括它自己。
5   str 是码点序列：len 数码点，字节要 encode，显示格要 east_asian_width —— 三个数经常不一样。
6   split() 与 split(' ') 是两种脾气；splitlines() 认 11 种换行；strip 删字符集合，砍前后缀用 removeprefix。
7   比 Unicode 等价先 normalize('NFC', …)；调试打印用 f"{x=}"，让空白现形用 !r。
8   list.sort() 返回 None、稳定、key= 每元素只算一次；空 set 只能 set()，逗号才造 tuple。
9   切片 / 构造器 / copy.copy 全是浅拷贝，只有 deepcopy 重造内层；有几层独立就写几个 for。
10  边遍历边改 list 不报错只给错答案；dict 改大小在下一次迭代那一刻 RuntimeError；生成器第二次遍历是空。
11  Counter 读缺失键给 0 不建键，defaultdict 读缺失键就建键；dict 遍历是插入顺序，== 与顺序无关。
12  def 只跑一次：默认值在定义那一刻算掉，可变默认参数一个字都不报错地攒账。
13  传参是一次赋值，只有原地改外面才看得见；空筐永远是 () 和 {}，判「没传」只写 is None。
14  名字是调用时才查的（晚绑定、闭包都从这句来）；编译器只看有没有赋值，所以先用后写炸在读那一行。
15  @X 就是 f = X(f)：不写 @functools.wraps 会吞掉 __name__ / __doc__ / 签名。
16  写了 __eq__ 就丢了 __hash__；dataclass 默认没有 <、默认可改，frozen 只挡赋值并白送 __hash__。
17  __exit__ 返回任何真值 = 异常就地抹掉；finally 里 return 把异常和返回值一起吞；线程里没接住的异常不改退出码。
18  open(p, 'w') 在打开那一刻就截断；with 只管 close 不管写全；原子写只有同目录 tmp → flush → fsync → os.replace。
19  sys.path[0] 的形状由启动方式决定；sys.modules 命中就什么都不做；pyc 头部那两个数是 mtime + 字节数。
20  注解是一段还没执行的代码；GIL 只管「一次让谁跑字节码」；并发收尾靠 join / shutdown / 哨兵，缺一件事就永远等。
```

---

## demo.sh 这一章在自查什么

这一章没有新的取证脚本，所以枪口对准**这一页本身**。`demo.sh` 八节，每节至少一条守卫，
每条守卫**先被人为弄响一次**再报正常值：

| 节 | 它在断言什么 | 它**不**保证什么 |
|---|---|---|
| 1 | 地板七行：解释器身份、GIL 的运行期与编译期、切换节拍与核数、默认 start method、`selectors` 后端、`pip` 缺席、os.fork 与 asyncio.to_thread 在场；再加一份 18 个名字的候选名单切成「在场 / 缺席」两列，在场那五个还报出落点目录 —— 值全部由 `python3 -c` 当场取 | 不保证你的机器一样 —— 上面那块地板就是让你回去重打的 |
| 2 | 证据来源只有一个：11 个章目录都在、每章 README 在、各自行数与节标题数当场数得出来。★ 这条先在**只有三个目录、其中一个没 README 的假书架**上跑：「缺一章」必须逐条点名、rc 非 0，否则「十一章都在」是恒真的。本章目录里除 README.md 只有 demo.sh | 不保证别章自己是对的（那归别章的闸门） |
| 3 | 表里**每一条**出处都落在 01~11 章，那一节的标题真在场、那一格真在场。认四种标题形状（`## 第 NN 节 ·`、`## N. 第 M 节 ·`、`## N. 标题`、`### 场景 K：……（demo 第 M 节）`）与两种格（取证脚本印出的 `=== K. 标题 ===`、正文写死的「第 K 格」）。四份坏页面分别响：假章号、假节号、假格号、出处条数不够地板线 | 不保证那一节的结论还适用 —— 出处只保证「翻得到」 |
| 4 | 页面结构五样当场数：表数 = 11 且编号连续、每表数据行在 4~8 的硬区间里、每行列数等于表头、三列都不许空、一页纸正文承诺的 N 与围栏里实数相等。五份坏样本各打中一条：空单元格、某行列数变 4、承诺 24 / 实数 20、表数掉到 10 且编号断、某表只剩 3 行 | 不保证内容好读，只管能不能核对 |
| 5 | 四段骨架（各自行数当场报出）逐个在内存里过 `ast.parse` + `compile()`。三份坏页面各响一次：删一个冒号（两道都炸）、模块层塞一个 `return`（`ast.parse` 放过去、只有 `compile()` 拦下 —— 这就是为什么要跑两道）、少一段围栏（段数 3 ≠ 4） | 语法过 ≠ 语义对；不 import、不执行，语义靠第 6 节抽 |
| 6 | 把 11 张表里的判断抽成 12 组探针（每组一行 `◆ 本条小结`，判据合计当场数）**当场再跑一遍**，屏上就是原写法的结果；账本再对一次账：小结行数、覆盖章数、全成立个数、屏上「→ 实测」行数与账本判据合计必须互相咬住 | 它是筛子，不是全量重跑；且一律挑确定性判据 —— 不数秒、不数 pid、不数线程进程个数 |
| 7 | 隐私扫描五类禁串：家目录绝对路径 / 登录名 / 主机名 / 本次沙箱路径 / 对象地址形态。先造一份必然五类全命中的脏副本让扫描器响，再**当场故意漏一条真家目录**（上屏必须只剩占位符），最后才对整份屏幕账报 0 | 不保证别处的字不敏感 —— 只扫本次这一份账 |
| 8 | 红线自证：章目录全程只读（先在沙箱里造一份同形状副本、真 `import` 出 `__pycache__` 再加一个非 .py 文件，数到新增 2，再对本体报 0）、家目录条目数与监听 socket 数跑前跑后一样、本 demo 的直接子进程归零（那个后台 sleep 由 demo 自己的 shell 挂，否则守卫恒真）、节标题数 = 8（先由一份截断的账报 7） | —— |

（下面每一段都是当场跑出来的屏幕原文，一个字都没有手抄：由 `@@R` 占位符从定稿捕获里整段搬回来，
`verify_quotes.py` 逐行核过。第 1 节的地板在上面「本章钉在哪台机器上」那一节，这里给剩下七节。）

**第 2 节 · 假书架：只摆三个目录，其中一个连 README 都没有 —— 「缺一章」被逐条点名，rc 非 0**

```
$ timeout 60 python3 "$SB/pages/check_sources.py" "$SB/fakebook"
  01-fake      3 行  节标题  1 个  取证分格  0 处  收尾节 0 个
  02-fake      3 行  节标题  1 个  取证分格  0 处  收尾节 0 个
读数：章目录 3 个 / 应有 11 个 / 问题 9 条
  [缺] 章目录 09-fake 没有 README.md
  [缺] 缺一章目录：03 开头
  [缺] 缺一章目录：04 开头
  [缺] 缺一章目录：05 开头
  [缺] 缺一章目录：06 开头
  [缺] 缺一章目录：07 开头
  [缺] 缺一章目录：08 开头
  [缺] 缺一章目录：10 开头
  [缺] 缺一章目录：11 开头
[退出码 1]
```

**同一个检查器对真书架：11/11 齐、问题 0 条**

```
$ timeout 60 python3 "$SB/pages/check_sources.py" "$BOOK"
  01-whats-really-happening   1168 行  节标题  9 个  取证分格  0 处  收尾节 1 个
  02-types-and-variables   2107 行  节标题 24 个  取证分格  0 处  收尾节 3 个
  03-strings-and-text   1841 行  节标题 25 个  取证分格  0 处  收尾节 5 个
  04-collections   2765 行  节标题 23 个  取证分格  0 处  收尾节 5 个
  05-functions-and-pitfalls   2463 行  节标题 26 个  取证分格 139 处  收尾节 5 个
  06-classes-and-dataclasses   1980 行  节标题 26 个  取证分格  0 处  收尾节 5 个
  07-errors-and-context   2785 行  节标题 22 个  取证分格 139 处  收尾节 6 个
  08-files-and-pathlib   3351 行  节标题 22 个  取证分格 113 处  收尾节 6 个
  09-modules-and-venv   2893 行  节标题 23 个  取证分格 56 处  收尾节 6 个
  10-typing-and-tools   2167 行  节标题 15 个  取证分格 50 处  收尾节 6 个
  11-concurrency   2237 行  节标题 15 个  取证分格 58 处  收尾节 6 个
读数：章目录 11 个 / 应有 11 个 / 问题 0 条
[退出码 0]
```

**第 3 节 · 四份坏页面：假章号、假节号、假格号各点出一条断链，第四份点出出处数不够**

```
$ timeout 60 python3 "$SB/pages/check_refs.py" "$BOOK" "$SB/pages/poison_ref_ch.md" 1 1
读数：出处出现 160 次（地板 1）/ 去重 120 条（地板 1）/ 断链 1 条
读数：覆盖章 01 02 03 04 05 06 07 08 09 10 11 99（12 个）
  [断] 第 99 章 · 第 7 节 —— 第 99 章的目录就不在
读数：本节判定 不通过（2 条）
[退出码 1]

$ timeout 60 python3 "$SB/pages/check_refs.py" "$BOOK" "$SB/pages/poison_ref_sec.md" 1 1
读数：出处出现 160 次（地板 1）/ 去重 119 条（地板 1）/ 断链 1 条
读数：覆盖章 01 02 03 04 05 06 07 08 09 10 11（11 个）
  [断] 第 08 章 · 第 99 节 —— 第 08 章里没有「第 99 节」这个标题
读数：本节判定 不通过（1 条）
[退出码 1]

$ timeout 60 python3 "$SB/pages/check_refs.py" "$BOOK" "$SB/pages/poison_ref_cell.md" 1 1
读数：出处出现 160 次（地板 1）/ 去重 120 条（地板 1）/ 断链 1 条
读数：覆盖章 01 02 03 04 05 06 07 08 09 10 11（11 个）
  [断] 第 10 章 · 第 02 节第 99 格 —— 第 10 章第 02 节里没有第 99 格
读数：本节判定 不通过（1 条）
[退出码 1]

$ timeout 60 python3 "$SB/pages/check_refs.py" "$BOOK" "$SB/pages/poison_thin.md" 120 90
读数：出处出现 5 次（地板 120）/ 去重 5 条（地板 90）/ 断链 0 条
读数：覆盖章 07 08 10 11（4 个）
读数：本节判定 不通过（3 条）
[退出码 1]
```

**本页真判：断链 0 条、覆盖 11 章**

```
$ timeout 60 python3 "$SB/pages/check_refs.py" "$BOOK" "$PAGE" 120 90
读数：出处出现 160 次（地板 120）/ 去重 119 条（地板 90）/ 断链 0 条
读数：覆盖章 01 02 03 04 05 06 07 08 09 10 11（11 个）
读数：本节判定 通过
[退出码 0]
```

**第 4 节 · 五份坏样本一份不落都印出来。前两份：空单元格被点名、某行列数变 4 被点名**

```
$ timeout 60 python3 "$SB/pages/check_page.py" "$SB/pages/poison_page_emptycell.md"
读数：表数 = 11，编号 = 01 02 03 04 05 06 07 08 09 10 11
读数：每表数据行 = 8 8 8 8 8 8 8 8 8 8 8（硬区间 4~8）
读数：一页纸正文承诺 20 条，围栏里实数 20 条
读数：python 围栏 4 个开栏标记（骨架四段，第 5 节再逐段验语法）
  [响] 表 3 有空单元格：| `split()` 就是 `split(' ')` | 不带参数吃连
读数：本节判定 不通过（1 条）
[退出码 1]

$ timeout 60 python3 "$SB/pages/check_page.py" "$SB/pages/poison_page_cols.md"
读数：表数 = 11，编号 = 01 02 03 04 05 06 07 08 09 10 11
读数：每表数据行 = 8 8 8 8 8 8 8 8 8 8 8（硬区间 4~8）
读数：一页纸正文承诺 20 条，围栏里实数 20 条
读数：python 围栏 4 个开栏标记（骨架四段，第 5 节再逐段验语法）
  [响] 表 4 有一行的列数是 4：| `b = a[:]` 算复制了一份 | 复制有六级梯子，前五级（贴标
读数：本节判定 不通过（1 条）
[退出码 1]
```

**第三份：正文把一页纸写成 24 条，围栏里数出来还是 20 条**

```
$ timeout 60 python3 "$SB/pages/check_page.py" "$SB/pages/poison_page_paper.md"
读数：表数 = 11，编号 = 01 02 03 04 05 06 07 08 09 10 11
读数：每表数据行 = 8 8 8 8 8 8 8 8 8 8 8（硬区间 4~8）
读数：一页纸正文承诺 24 条，围栏里实数 20 条
读数：python 围栏 4 个开栏标记（骨架四段，第 5 节再逐段验语法）
  [响] 一页纸的条数对不上：承诺 24、实数 20
读数：本节判定 不通过（1 条）
[退出码 1]
```

**第四份：表数掉到 10、编号断、某表 16 行 —— 一条命令同时响三条**

```
$ timeout 60 python3 "$SB/pages/check_page.py" "$SB/pages/poison_page_tables.md"
读数：表数 = 10，编号 = 01 02 03 04 05 06 08 09 10 11
读数：每表数据行 = 8 8 8 8 8 16 8 8 8 8（硬区间 4~8）
读数：一页纸正文承诺 20 条，围栏里实数 20 条
读数：python 围栏 4 个开栏标记（骨架四段，第 5 节再逐段验语法）
  [响] 表数 = 10，该是 11
  [响] 表的编号不连续：[1, 2, 3, 4, 5, 6, 8, 9, 10, 11]
  [响] 表 6 的数据行 16 条，不在 4~8 里
读数：本节判定 不通过（3 条）
[退出码 1]
```

**第五份：把表 5 砍到只剩三行 —— 硬区间 4~8 不许「太薄的那一张表」**

```
$ timeout 60 python3 "$SB/pages/check_page.py" "$SB/pages/poison_page_rows.md"
读数：表数 = 11，编号 = 01 02 03 04 05 06 07 08 09 10 11
读数：每表数据行 = 8 8 8 8 3 8 8 8 8 8 8（硬区间 4~8）
读数：一页纸正文承诺 20 条，围栏里实数 20 条
读数：python 围栏 4 个开栏标记（骨架四段，第 5 节再逐段验语法）
  [响] 表 5 的数据行 3 条，不在 4~8 里
读数：本节判定 不通过（1 条）
[退出码 1]
```

**本页真判**

```
$ timeout 60 python3 "$SB/pages/check_page.py" "$PAGE"
读数：表数 = 11，编号 = 01 02 03 04 05 06 07 08 09 10 11
读数：每表数据行 = 8 8 8 8 8 8 8 8 8 8 8（硬区间 4~8）
读数：一页纸正文承诺 20 条，围栏里实数 20 条
读数：python 围栏 4 个开栏标记（骨架四段，第 5 节再逐段验语法）
读数：本节判定 通过
[退出码 0]
```

**第 5 节 · 三份坏骨架的前两份：冒号删掉后两道都炸；模块层塞 `return` 只有 `compile()` 拦下**

```
$ timeout 60 python3 "$SB/pages/check_snips.py" "$SB/pages/poison_snip_colon.md" 4
读数：python 围栏 4 段（该是 4），各自行数 = 13 33 28 42
读数：ast.parse 通过 3/4，compile 通过 3/4
  [响] 第 1 段 ast.parse 就炸：expected ':'（行 12）
  [响] 第 1 段 compile 也炸：expected ':'（行 12）
读数：本节判定 不通过
[退出码 1]

$ timeout 60 python3 "$SB/pages/check_snips.py" "$SB/pages/poison_snip_return.md" 4
读数：python 围栏 4 段（该是 4），各自行数 = 13 34 28 42
读数：ast.parse 通过 4/4，compile 通过 3/4
  [响] 第 2 段 compile 也炸：'return' outside function（行 1）—— 这一段 ast.parse 放过去了、只有 compile 拦下
读数：本节判定 不通过
[退出码 1]
```

**第三份：抽到 3 段、正文承诺 4 段 —— 语法全对也照样不许**

```
$ timeout 60 python3 "$SB/pages/check_snips.py" "$SB/pages/poison_snip_count.md" 4
读数：python 围栏 3 段（该是 4），各自行数 = 33 28 42
读数：ast.parse 通过 3/3，compile 通过 3/3
  [响] 段数就不对：抽到 3 段，正文承诺 4 段
读数：本节判定 不通过
[退出码 1]
```

**本页真围栏：4 段全部过 `ast.parse` 与 `compile()`**

```
$ timeout 60 python3 "$SB/pages/check_snips.py" "$PAGE" 4
读数：python 围栏 4 段（该是 4），各自行数 = 13 33 28 42
读数：ast.parse 通过 4/4，compile 通过 4/4
读数：本节判定 通过
[退出码 0]
```

**第 6 节 · 探针的形状（第一组）：说法 ｜ 拿到什么 → True，小结行自己报判据数**

```
$ timeout 240 python3 probes.py p01
  [章 01] 启动方式与缓冲：属性才是尺，不是「卡住了」
  → 实测 管道里 stdout 默认既不是行缓冲也不直写：写了 9000 字节带 4500 个换行照样 0 字节可见 ｜ 默认档 line_buffering/write_through → False False → True
  → 实测 -u 改的是 write_through，line_buffering 仍是 False —— 想判缓冲看属性，别猜 ｜ -u 档 line_buffering/write_through → False True → True
  ◆ 本条小结：章 01｜判据 2 条｜全成立 True
[退出码 0]
```

**账本对账：两份坏账本各响一次（一条没全成立、删掉整行小结），真账本才报通过**

```
$ timeout 60 python3 "$SB/pages/check_ledger.py" "$SB/ledger-bad1.txt"
读数：小结 12 行 / 覆盖章 01 02 03 04 05 06 07 08 09 10 11 / 判据合计 51 条 / 屏上实测行 51 条
  [响] 章 01 那组没全成立（它自己报的是 False）
读数：本节判定 不通过（1 条）
[退出码 1]

$ timeout 60 python3 "$SB/pages/check_ledger.py" "$SB/ledger-bad2.txt"
读数：小结 11 行 / 覆盖章 02 03 04 05 06 07 08 09 10 11 / 判据合计 49 条 / 屏上实测行 51 条
  [响] 小结行数 = 11，该是 12 组
  [响] 覆盖 10 章，该是 11 章（每组对应一张表）
  [响] 屏上实测行 51 条 ≠ 账本判据合计 49 条
读数：本节判定 不通过（3 条）
[退出码 1]
上面两条 rc 都非 0，是**故意的**。下面是对真账本的判定。

$ timeout 60 python3 "$SB/pages/check_ledger.py" "$SB/ledger.txt"
读数：小结 12 行 / 覆盖章 01 02 03 04 05 06 07 08 09 10 11 / 判据合计 51 条 / 屏上实测行 51 条
读数：本节判定 通过
[退出码 0]
```

**第 7 节 · 先扫必然全命中的脏副本（五类合计 9 次、rc 非 0），再当场故意漏一条真家目录**

```
$ timeout 60 python3 "$SB/pages/make_dirty.py" "$SB/dirty.txt" "$HOME" "$USER" "$(hostname)" "$SB"
脏副本已写好（5 行：真路径 / 真登录名 / 真主机名 / 真沙箱路径 / 真对象地址）
[退出码 0]

$ timeout 60 python3 "$SB/pages/scan_priv.py" "$SB/dirty.txt" "$HOME" "$USER" "$(hostname)" "$SB"
  类 家目录绝对路径 命中 1 次
  类 登录名 命中 3 次
  类 主机名 命中 3 次
  类 本次沙箱路径 命中 1 次
  类 对象地址形态 命中 1 次
读数：五类禁串合计命中 9 次
[退出码 1]
上面那条 rc 非 0，是**故意的** —— 它证明扫描器真的认得这五类，不是摆设。
★ 折叠器也要**当场喂一条真泄漏**给它：这条命令把真家目录与真登录名打到 stdout，
  屏幕上印出来的必须是占位符。没有这一步，「fold 挡过东西」就只是上一趟的运气 ——
  上一趟它靠的是脚本崩溃时的 Traceback，那一响不是设计出来的，判据不能建在它上面。

$ printf "这条是故意漏的：家目录=[%s] 登录名=[%s]\n" "$HOME" "$USER"
这条是故意漏的：家目录=[<家目录>] 登录名=[<主机名>]
[退出码 0]
```

**第 8 节 · 同一个残留检查器两次调用：先在沙箱副本上数到新增 2，再对真章目录报 0**

```
$ timeout 60 python3 "$SB/pages/check_residue.py" "$SB/replica" "$SB/replica-before.txt"
读数：replica 现有 5 项 / 基准 3 项 / 新增 2 项 / 少了 0 项
  [残留] 多了 __pycache__
  [残留] 多了 stray.bak
[退出码 1]
上面那条 rc 非 0，是**故意的**：残留被数出来了（一个是真 import 生成的 __pycache__，一个是非 .py 笔记）。
下面是同一个检查器对**真章目录**报数 —— 这一趟跑完它必须是 0。

$ cd -- "$HERE" && ls -1 > "$SB/here-after.txt"; timeout 60 python3 "$SB/pages/check_residue.py" "$HERE" "$SB/here-before.txt"
读数：12-cheatsheet 现有 2 项 / 基准 2 项 / 新增 0 项 / 少了 0 项
[退出码 0]
```

**子进程守卫：先由 demo 自己的 shell 挂一个 sleep 让它报「不归零」，收回来才报归零**

```
子进程归零这条也先让它响一次。★ 这个后台 sleep 必须由 **demo 自己的 shell** 挂：
挂在 runc 里就是那个 bash -c 包装进程的孩子，--ppid 数到的是包装进程本身（下面按 comm 过滤掉了），
于是「有泄漏」永远数不出来 —— 那就是一条恒真的守卫。

$ kids=$(ps -o comm= --ppid "$MAIN_PID" 2>/dev/null | grep -v -e "^bash$" | tr "\n" " "); printf "故意挂进去一个 sleep 之后：[%s] → 归零吗 %s（该是 否，这才叫能响）\n" "$kids" "$([ -z "$kids" ] && echo 是 || echo 否)"
故意挂进去一个 sleep 之后：[sleep ] → 归零吗 否（该是 否，这才叫能响）
[退出码 0]

$ kill "$POISON_CHILD" 2>/dev/null; printf "已 kill 掉那个 sleep（pid 本身不上屏）\n"
已 kill 掉那个 sleep（pid 本身不上屏）
[退出码 0]

$ kids=$(ps -o comm= --ppid "$MAIN_PID" 2>/dev/null | grep -v -e "^bash$" | tr "\n" " "); printf "收回来之后：[%s] → 归零吗 %s\n" "$kids" "$([ -z "$kids" ] && echo 是 || echo 否)"
收回来之后：[] → 归零吗 是
[退出码 0]
```

**这一页的总账（末行自己也在账里，所以命令行比退出码行多 1、判据行比节数少 1）**

```
$ head -n $(( $(wc -l < "$SB/all.out") - 1 )) "$SB/all.out" | grep -v "^══════ 第 8 节" > "$SB/all-cut.out"; printf "截断后的账：节标题数=%s（该报 7，因为它被抹掉了一节）\n" "$(grep -c "^══════ 第 " "$SB/all-cut.out")"
截断后的账：节标题数=7（该报 7，因为它被抹掉了一节）
[退出码 0]

$ printf "屏幕总行数=%s  节标题数=%s（该是 8）  命令行=%s  退出码行=%s  判据行=%s\n" "$(wc -l < "$SB/all.out")" "$(grep -c "^══════ 第 " "$SB/all.out")" "$(grep -c "^[$] " "$SB/all.out")" "$(grep -c "^\[退出码 " "$SB/all.out")" "$(grep -c "^  → 判据：" "$SB/all.out")"
屏幕总行数=452  节标题数=8（该是 8）  命令行=57  退出码行=56  判据行=7
[退出码 0]
  → 判据：章目录跑前跑后清单一模一样、新增残留 0（同一个检查器先在副本上数到 2）；家目录条目数不变；监听 socket 数不变；本 demo 的直接子进程归零（先故意挂一个让它响）；节标题数 = 8（截断账上报 7）。
```

---

## 这章的坑（速查表自己也会骗人）

1. **这一页不能替代那一节。** 表里每行是「结论 + 出处」，不带现场。你要改一个正在跑的脚本，
   别只扫这一页 —— 按出处回那一节看一眼它到底是怎么响的。表 11 里那条「GIL 下 `+=` 一定丢」被
   压成一行之后，最容易丢的就是「本机 4 线程 × 20 万次**一条都不丢**」这半句。
2. **出处的节号会随改版漂。** 所以 `demo.sh` 第 3 节是**当场解析标题**，不是背一张映射表。
   哪天你重排了某一章，这一页会直接报断链，而不是让你翻到空处。
3. **「骨架」四段是形状，不是程序。** 它们只保证一件事：**过得了 `ast.parse` 与 `compile()`**（第 5 节），
   语义正确性一律靠出处那一节的现场。三处最容易踩的空壳：第 3 段那个 `run` 里 `strict` 分支就是一句
   `if strict: pass`、`bad` 永远是空表，所以照抄下来退出码恒 0；第 1 段的 `main` 只认 `--verbose`；
   第 2 段的 `write_json_atomic` **一调用就真的**在目标目录落临时文件再 `os.replace`，别在仓库目录里试它。
4. **`ast.parse` 过得了 ≠ 跑得对。** 它连「名字存不存在」都不管（这正是第 10 章那条惰性注解的现场：
   `def h(x: Nope)` 解析、定义都成功，炸的是第一次读注解）。这一页所有「必须真跑才知道」的结论，
   都是各章现场跑出来的，不是语法检查验出来的。第 5 节那两份坏骨架就是专门演这个分工的：
   删冒号两道都炸，模块层塞 `return` 只有 `compile()` 拦得下。
5. **别把这一页里的「唯一姿势」当成我的偏好。** 它们都是被反例演过的：`a = b = []`、`[[0]*3]*3`、
   `open(p, 'w')` 之后反悔、`tempfile.mktemp()`、`exists()` 把关、`except Exception` 兜取消、
   不塞哨兵的 `queue` —— 每一条都在后面那一章有一处真实事故。
6. **这一页没有「每跑必变」的数字。** 全书纪律：毫秒、pid、线程/进程个数、实际派出的 worker 数、
   inode 号每跑必变，一律不许当判据（第 11 章 · 第 13 节把「实际派出几个」直接从屏幕上撤了）。
   所以你在表里看到的「快 / 慢 / 贵」全部换成**次数、条数、顺序、布尔**。留下来的数字都是定死的：
   缓存区间 `-5..256`、`b"中文"` 在编译期就被拒、一行 `box["n"] += 1` 在本机翻成 13 条字节码 ——
   它们换一趟跑还是这几个数，第 6 节当场又判了一遍。

## 自检问题

1. `sys.path[0]` 在 `python3 x.py`、`python3 -m pkg.x`、`python3 -c` 三种入口下分别是什么？
   哪一种是空串，空串又是跟着谁变的？（提示：表 1 + 第 09 章 · 第 6 节）
2. `xs.sort()` 与 `sorted(xs)`、`list` 的 `+=` 与 `str` 的 `+=`，两组各自的「返回什么 / 改了什么」区别在哪？
3. 为什么 `def f(xs=[])` 比 `class D: items: list = []`（`@dataclass`）更危险？后者在本机炸在哪一刻？
4. `with open(p, 'w') as f:` 这一句里，「不可逆」发生在哪一刻？`__exit__` 返回 `True` 会挡住什么？
5. 一条 `queue` 消费线程 + 4 个工人，收尾要写哪三件事？少写哪一件会变成「卡住而且什么都不打印」？
6. `hasattr(f, '__annotations__')` 为什么不算「安全探针」？`annotationlib` 四把钥匙里默认那把为什么最危险？
7. 这一页里哪几条明确写了「本机演不出来 / 从屏幕上撤了」？它们为什么不许被写成结论？
8. 本章的 `demo.sh` 第 3 节和第 5 节各自先弄响一次的是什么？为什么恒真的检查比没有检查更糟？

## 本章没演 / 没验证

- **不新跑前十一章的任何取证脚本**：这一页是压缩件，所有现场都还在各章的 `demo.sh` 里，
  本章只是**核对出处在不在**、再把 12 组、合计 51 条判据用同一支解释器当场重跑一遍（第 6 节）。
  它是筛子：11 张表 88 行里，被当场重判的是每章挑出来的那几条形状判据，不是全部。
- **这一页被逐行审过一遍，88 行里有 5 行的「屏幕上实际是什么」写过头了**，五处都已改成「屏幕上到底有什么」：
  ① 表 1 行 8 把 pip 缺席和 `compileall --force` 说成当场跑出来的 —— 第 01 章里这两句都在**正文**，
  屏幕上只有小写 `-f` 那次成功；② 表 2 行 3 写探针是 `int('257')`，第 02 章那支扫描器实际用 `n + 0`；
  ③ 表 5 行 4 把「3.14 连警告都不给」挂在第 05 章 · 第 12 节上，原话在第 04 章 · 第 15 节（现在两章都引）；
  ④ 表 7 行 6 的「全书唯一一条完全没有报错的坑」被同一页的表 4 行 2、表 5 行 1 直接否掉，改成第 07 章自己的原话；
  ⑤ 表 10 行 1 说 traceback 里那一帧叫 `__annotate__`，可第 10 章从未让 traceback 正文上屏，
  那一格印的只有三条逐字相同的 `NameError: name 'Widget' is not defined`。
  这五处改动只有一处动了出处（③ 那行多引一章），而本节自己新写的话里又点了两处出处，
  所以屏幕上那行「出处出现」的读数一路从 157 变成 160 —— 去重 119 条、断链 0 这两头一个字没动。
- **free-threading（无 GIL）演不出来**：本机 `sys._is_gil_enabled()` 是 `True`、编译期
  `Py_GIL_DISABLED` 是 `0`、没有 `python3.14t` 那个解释器。表 11 里凡是关于 GIL 的结论，
  钉的都是「带着 GIL 的这台机器」。
- **装包、第三方库、网络演不出来**：本机 `find_spec("pip")` 是 `None`、`python3 -m venv` 缺
  `ensurepip` 而失败、`EXTERNALLY-MANAGED` 在场。★ 订正一句：第 10 章里写过「唯一在场的第三方库是
  `typeguard`」，那话只成立于它当场探的那几个名字 —— 本章第 1 节把一份 18 个名字的候选名单一次切两半，
  屏幕上印的是 `requests` / `rich` / `setuptools` / `typeguard` / `yaml` 五个都在 `dist-packages` 里。
  被否掉的是「装东西」这条路（没有 pip），不是「本机一个第三方包都没有」。本章的表与骨架
  **一个字都没用到这些包**，不联网、不开端口、不 `pip install`。
- **静态检查器（mypy / pyright / ruff / black / flake8 / pylint / pytest）全部不在场**：
  所以这一页不给「装好之后长这样」的写法。第 10 章 · 第 12 节那把 `ast` 尺是本机现搓的，不是 mypy 的替身。
- **Windows / macOS 的路径、换行、编码差异没测**：本机是 POSIX，`os.fork` 在场（Windows 上没有这条路），
  `selectors` 后端当场印作 `EpollSelector`（Linux 专属）。表 8 里 `WindowsPath` 那条只演到
  「在本机实例化会抛 `UnsupportedOperation`，而且它不是 `OSError` 的儿子」。
- **PyPy / GraalPy 的缓存边界、驻留规则、`__slots__` 收益没测**：全书只钉 CPython 3.14.4。
- **没有性能数字。** 表里所有「贵在哪里」的说法都是**次数与结构**（字节码条数、复杂度档位、
  复制层级、哨兵个数），没有毫秒 —— 这是全书纪律（第 09 章 · 第 1 节、第 11 章 · 第 13 节）。
- **异步 IO 的真实网络现场没演**：本章不连外部主机；第 11 章那台 `selectors` 分发器收的是
  本机子进程的输出，不是 socket。

## 怎么跑这一章

```text
cd <本书目录>/python-handbook/12-cheatsheet
bash demo.sh              # 八节自查，跑完自动删沙箱（前缀 py12-XXXXXX）
bash demo.sh --keep       # 保留沙箱，进去自己接着敲
bash demo.sh > /tmp/py12.txt 2>&1   # 重定向时自动关颜色（[ -t 1 ] 判），拿到干净纯文本
```

屏幕上有两种行，别混：`$ 命令` 到 `[退出码 N]` 之间的是这台机器当场跑出来的原文，一个字没手打；
`#` 开头和 `→ 判据：` 开头的是写给读的人的话与每节的收口判定，它们不参与「必须逐字复现」的承诺
（`script_check.py` 的 GUARD 就按这个形状查：八节，一节不许少一条收口判定）。

本章跟着全书的安全约定走，一条都没放宽：

- 只用**标准库**，不 `pip install`、不联网；写操作只发生在 `mktemp -d` 的沙箱里，`trap cleanup EXIT` 收尾。
- `demo.sh` **故意不加 `set -e`**：这一章的负样本（假出处、坏骨架、真残留、真用户名）全靠「响完继续跑」，
  加了它就只能看见第一个失败。
- 每条命令后面打 `[退出码 N]`，退出码就是判据；本脚本自己的退出码恒为 0（它是验收闸门，不是被验收的程序）。
- 拿退出码一律在父级管道上取（`${PIPESTATUS[0]}` 先落到临时文件再用），写成 `$(cmd | fold)` 拿到的是
  命令替换自己的状态，每条绿灯都会变成假凭证。
- 屏幕上不出现真实身份：家目录、登录名、主机名、本次沙箱路径上屏前一律折成 `<家目录>` `<用户>`
  `<主机名>` `<沙箱>`（折主机名排在折用户名之前，反之会漏半截）。沙箱名里那 6 个随机字符每跑必变，
  所以默认收尾只印「沙箱已删除」、不印路径。
  ★ 本机还有一条要交代：`$(hostname)` 与 `$USER` 当场比是**同一个串**，所以第 7 节那条故意漏出来的
  登录名在屏上印成了 `<主机名>` —— 这一页的捕获里 `<用户>` 出现 0 次。占位符少一种不是漏折，
  是这两个名字在这台机器上本来就相同。
- 章目录全程只读：这一页要翻**同级另外 11 章的 README**，所以 `demo.sh` 顶上有 `# CHECK: RUN=就地`；
  代价（跑前跑后目录清单必须一模一样）在第 8 节当场摊开。

## 术语小抄

| 术语 | 这本书里的确切所指 |
|---|---|
| 浅 / 深 | 「改内层，外面看不看得见」。切片、构造器、`.copy()`、`copy.copy` 全在同一级（浅），只有 `deepcopy` 重造内层 |
| 一次性迭代器 | 走过就空且不报错的那类对象：生成器、`map` / `filter`、文件对象、`csv.reader`。第二次拿到的是空 |
| 晚绑定 | 名字在**调用那一刻**才查。`[lambda: i for i in range(3)]` 三个都是 2 就是它，不是 `lambda` 的锅 |
| EAFP | 直接做事、按 `errno` / 异常类型分派，而不是先 `exists()` 再 `open()`（先测后写之间还有窗口） |
| 通用换行 | 读侧 `newline=None`：`\r\n` 与 `\r` 都翻译成 `\n`，**只有它会改内容**；`''` 与显式值都不改 |
| 不透明 cookie | 文本模式 `tell()` 的返回值：只能原样还给 `seek`，不是字节偏移 |
| 遮蔽（shadow） | 你放在 `sys.path` 前面的同名文件顶掉标准库。内置/冻结层不在 `sys.path` 上，顶不掉 |
| 三本账 | 第 09 章的骨架：`sys.modules`（跑不跑）、`sys.path`（从哪跑）、`__pycache__`（用不用新的）。venv 只换第二本 |
| 惰性注解 | 注解在写下那一刻不执行，编译成 `__annotate__`；第一次读 `__annotations__` 才可能炸 |
| GIL 的粒度 | 「一次只让谁跑字节码」。一行语句可能被拆成十几条，等待时它当场撒手 —— 所以它不是并行度开关 |
| 哨兵 | 往队列里塞一个约定结束标记（`None`）让工人收摊。少塞一个 = 永久等待，且什么都不打印 |
| 收口 | 并发里「谁负责等完」：`with Executor` 负责 `shutdown(wait=True)`，线程要 `join`，队列要哨兵 |
| 掩码后的形状 | 把每跑必变的字段（pid、毫秒、地址、随机后缀）换成占位符之后剩下的那串字 —— 本书唯一拿来当证据的形态 |

## 本章之后

十一章讲完，判据都在这一页上收拢了。接下来该做的是把它**用脏**：
挑一个你真的在跑的脚本，按骨架 1 给它补上 `if __name__` 与退出码，按表 8 给它换掉 `open(p, 'w')`
的写法，按表 9 排查一次 `ModuleNotFoundError`，按骨架 4 把它的并发收尾补齐。
要是你从这里回看前十一章，建议的顺序是 **08 → 09 → 05 → 11 → 07**：
代价从大到小 —— 数据被截断和被同名文件顶掉最容易在你手上出事，函数默认值第三，
并发和异常收尾是「出事时最不响」的那两族。
剩下的路在这一页之外：`mypy`/`pytest` 这类工具要先在**有网的机器**上装了才谈得上用，
而本书故意一条都没假设你装了它们。

## 本章回指

| 这一页哪里用到 | 出处 |
|---|---|
| 「名字是标签、对象才是东西」—— 表 2 整表的地基 | 第 02 章 · 第 2 节；表 5 的「传参是一次赋值」是同一条规律的函数版 |
| `def` 只跑一次、调用才查名字 | 第 05 章 · 第 4 节 + 第 05 章 · 第 14 节；第 06 章的 dataclass 开关、第 10 章的惰性注解全部建在这两句上 |
| 静悄悄杀手：那些不报错的错 | 第 07 章 · 第 16 节；表 1 的缓冲、表 4 的边遍历边删、表 8 的 `'w'` 截断都是它的落地版 |
| `with` 只管 close 不管事务，原子写是同目录 tmp + `os.replace` | 第 07 章 · 第 18 节 → 第 08 章 · 第 17 节做实；骨架 2 的第三个函数就是它的形状 |
| 迭代器只能走一遍 | 第 04 章 · 第 17 节；第 08 章 · 第 4 节把它落到文件对象上；表 4 与表 8 各有一行 |
| `import` 查三本账，venv 只换 `sys.path` | 第 09 章 · 第 21 节的合并视图；表 9 整表是它的压缩件 |
| 本机没有任何静态检查器，连 pip 都没有 | 第 10 章 · 第 01 节 + 第 10 章 · 第 12 节；所以这一页的骨架只用标准库，一条 import 都没多 |
| 每跑必变的东西不许当证据 | 第 09 章 · 第 1 节、第 11 章 · 第 13 节；本章第 6 节选材时把这条当筛选条件用了一遍 |
| 恒真守卫不算守卫：先把它故意弄响一次，再报 0 | 第 07 章 · 第 20 节、第 08 章 · 第 20 节、第 11 章 · 第 14 节；本章八节各自都带一份坏样本 |
