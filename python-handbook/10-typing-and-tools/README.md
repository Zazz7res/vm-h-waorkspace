# 第 10 章 · 类型注解与工具：注解本身不做事，读它的人才做事

> 本章所有输出都是本机 `Python 3.14.4` 真跑出来的，一条都没编、一条都没改。
> 跑法：`cd python-handbook/10-typing-and-tools && bash demo.sh`（14 节，跑完自动删沙箱）。
> 想自己拨开关：`bash demo.sh --keep`，然后进打印出来的那个 `<沙箱>/work` 里接着敲。
>
> 本章的命令只有两类：`$ python3 某个文件.py`、`$ python3 -c "..."`（外加几条 `ls` / `find` / `grep` 看现场），
> 全部只读写 `mktemp -d` 出来的沙箱。**不联网、不装包、不开任何监听端口，也不动 `~/.local`、`~/.config`、`~/.bashrc`**。
> 本机**没有 mypy、pyright、ruff、black、flake8、pylint、pytest，连 pip 都没有** —— 这不是本章的遗憾，是本章的地板：
> 第 01 节把这条在场性红线当场探出来，第 12 节就在「没有尺」的前提下用 `ast` 现搓一把给你看。
>
> 会变的量不进围栏：沙箱随机名、pid、毫秒、对象地址（`0x…`）一律不印在屏幕上，
> 真实家目录折成 `<家目录>`、安装前缀折成 `<pyroot>`、当前目录折成 `<cwd>`、地址折成 `<地址>`；
> 能判定的都改成了「相等吗 / 在第几项 / 几个 / 有没有」。
> 同一份 `demo.sh` 连跑两遍 `diff` 为空（本章定稿前实测过），第 14 节是它给自己的自证。
> 章内七支取证脚本自己判了 233/233 条据，那七条合计行在第 14 节被原样列出来。

---

## 第 00 节 · 本章在讲什么：注解是一段「还没跑」的代码

前九章讲的是「一个文件里的 Python 怎么跑」。这一章讲的是一批**写了但不跑**的代码：
`def load(x: Widget) -> list[int]` 里的 `Widget`、`list[int]`，它们既不是给运行时执行的，
也不是给运行时检查的 —— 它们是一段**存起来等着别人读**的元数据。

这一章的主干是一句话：

> **注解本身不做事，读它的人才做事。**
>
> **一、写下的那一刻一行都不执行。** 3.14 把注解编译成一个 `__annotate__` 小函数挂在函数/类/模块上，
> 名字不存在照样定义成功 —— 你甚至可以让 `Widget` 根本不存在，程序照跑。
> 求值**成功才缓存、失败不缓存**，所以「刚才读没炸」保证不了「下一次不炸」。
>
> **二、读注解只有一个入口族。** `annotationlib.Format` 四把钥匙：
> 1 号 VALUE（会炸的那把）、2 号内部专用（公开入口直接 `ValueError`）、3 号 FORWARDREF（把缺的名字包成对象）、
> 4 号 STRING（只给源码文本）。同一份注解，插哪把钥匙得到四种完全不同的形状。
>
> **三、运行时没有任何人替你检查值。** `Final` 不拦重新赋值、`@final` 不拦继承、`Literal` 越界照传、
> `assert_type` 是空气、`Protocol` 的 `isinstance` 只看方法名不看签名；
> dataclass / NamedTuple / TypedDict 用注解但**不求值**它。
>
> **四、本机没有静态检查器。** mypy、pyright、ruff、black、flake8、pylint、pytest 全缺席，连 `pip` 都没有；
> 本机另外装着发行版带来的 `requests` / `rich` / `setuptools` / `yaml`（第 12 章第 1 节当场把名单切给你看），
> 但**类型检查这一路一个都没有**；本章用得上的第三方库只有 `typeguard` 一个，而它是**运行时插桩**。
> 剩下的路是自己用 `ast` 搓一把尺。
>
> —— 而「有人去读」这件事，才是工具链全部戏份的来源：dataclass 读、`inspect.signature` 读、
> `typing.get_type_hints` 读、`typeguard` 读、你将来装的 mypy 也读。**它们读的不是同一个口径，
> 所以同一份代码在它们眼里长得完全不一样。**

四条主线就按这四句排（屏上每节末尾那句 `↑ 主线②/③/④` 指的是同一套编号）：

- **主线①** 惰性求值（第 2~3 节）：`a` 篇量「什么时候才求值、缓存不缓存」，第 3 节把 future 开关做成两份真文件并排。
- **主线②** 四个读取口径（第 4~5 节）：`b` 篇量「四把钥匙各给什么」，第 5 节是一张跨 a/b 的合并矩阵。
- **主线③** 运行时不检查（第 6~9、11 节）：`c` 篇数「到底谁在读注解」，`d` 篇八个「写了也不拦」的现场，
  `e` 篇量 `isinstance` 家族能吃什么，第 11 节把同一个坏值摆到四把工具前面。
- **主线④** 没有静态检查器怎么活（第 10、12 节）：`f` 篇是在场的唯一第三方库 `typeguard`
  （运行时插桩，不是静态检查），`g` 篇用 `ast` 手搓一把最小的尺，并当场演示这把尺漏在哪。

| 你想搞清楚的东西 | 本章用哪一节回答 | 一句话答案 |
|---|---|---|
| `def f(x: Widget)` 里 `Widget` 不存在，为什么还能跑 | 第 02 节 | 定义那一刻只编译出一个 `__annotate__`，一行都没执行；`f(3)` 照样回 3 |
| 那它到底什么时候炸 | 第 02 节 | 第一次**读** `f.__annotations__` 才炸，而且 `hasattr` 挡不住它（只吞 `AttributeError`） |
| 为什么「刚才读没炸」保证不了「下一次不炸」 | 第 02 节 | 成功才缓存、失败不缓存 —— 计数器量出来是 1→2 |
| 注解里能不能放有副作用的表达式 | 第 02 节 | 能，但每读一次 STRING 口径就重跑一次副作用；VALUE 命中缓存不重跑 |
| `x: "SomeType"` 加引号算不算延迟求值 | 第 02、03 节 | 不算。它只是存了个 `str`，谁想当类型用谁自己去 `eval`，NameError 立刻还回来 |
| `from __future__ import annotations` 和不加，差在哪 | 第 03、04 节 | 差在**能不能拿到真对象**：开了之后所有注解都是文本，下游想求真类型还得自己 eval |
| 读注解到底有几个入口 | 第 04 节 | `obj.__annotations__`、`annotationlib.get_annotations(format=…)`、`typing.get_type_hints`、`inspect.signature` —— 默认口径都是「会炸的那一把」 |
| dataclass 建类的时候为什么不怕坏注解 | 第 06、07 节 | 它压根没求值注解（它读的是 2 号口径的编译版）；真正炸的是后来去读注解的那个人 |
| `if TYPE_CHECKING:` 到底省了什么 | 第 06 节 | 省的是**运行时那次 import**，注解本身一个都没少；工具看不见这一行就等于少了名字 |
| `x: Final = 3` 拦得住重新赋值吗 | 第 08 节 | 拦不住。`Final`、`@final`、`Literal`、`cast`、`assert_type` 在运行时全是空气 |
| `isinstance(x, SomeProtocol)` 查什么 | 第 09 节 | 只查方法名存不存在，一个签名都不查 —— 说 True 之后真调用当场炸 |
| `typeguard` 装了是不是就有 mypy 了 | 第 10 节 | 不是。它是运行时插桩，只查它插得到的调用点，而且默认策略只查容器第一个元素 |
| 没有 mypy 的时候我能自己查点什么 | 第 12 节 | 用 `ast` 摊开源码做静态检查：一行都不执行也能抓到「这个名字没 import」 |
| 为什么本章每格末尾都印「实测 ｜ 预期」 | 第 14 节 | 章里印出来的每句承诺都得有机器判得住的断言撑着，恒真的守卫比没有更糟 |

**读法建议**：只想知道「我加了注解到底有什么在用它」，读第 02、04、06 节这三篇就够；
被 `NameError: name 'XXX' is not defined` 在**运行时**咬过的人直奔第 02 节第 1 格和第 04 节第 2 格；
准备给项目上类型检查、但机器上装不了工具的人，第 10、12 节是为本机这种情况写的；
第 05、07、11 三张矩阵格是把前面几篇的读数并排成一张表，跳读时可以只看这三节先把地图拿到。
第 01 节是地基（这台机器上到底有哪些工具、`annotationlib` 在不在标准库里），
后面每一节的「本机实测」都能回指到这里的某一格。

**和相邻章节的接缝**：注解「在 `def` 那一刻被求值」这条家族规矩，第 05 章（函数与坑）讲过默认值冻结，
本章第 02 节把同一套机制推到注解上；`__annotations__` 是模块/类 `__dict__` 里的普通键这一条接第 09 章
（模块属性就是全局状态，本章第 02 节第 3、9 格是它的注解版）；`typing` 名字查不到时的
`AttributeError` 家谱接第 07 章；dataclass 本体在第 06 章，本章只回答「它什么时候读注解、读哪个口径」。
本章不管第三方工具的安装（本机装不了，第 01 节实测），也不管并发与子进程 ——
那是第 11 章的事；第 12 章把本章的判据思路收成速查。

---

## 第 01 节 · 环境体检：注解这条路的起点，和这台机器上到底有什么工具

类型注解的结论**全部跟版本绑定**：惰性注解这套机制和 `annotationlib` 这个模块都是 3.14 才有的，
所以本章每一条「本机实测」离了这台 `Python 3.14.4` 就不许直接套用 —— 你换一台机器，第一件事就是重跑下面那两格布尔。
第一节先把解释器身份钉死，再把「这台机器上有哪些工具」当场探一遍 ——
本章后面每一句「本机实测」都挂在这一节上。

章目录里那七支取证脚本先列出来，`demo.sh` 把整份副本拷进沙箱 `work/` 再跑，章目录本身不参与写：

```
$ ls
a_lazy_annotations.py
b_four_reads.py
c_who_reads_annotations.py
d_runtime_checks_nothing.py
e_protocols_generics.py
f_typeguard.py
g_mini_checker.py
[退出码 0]
```

解释器和 `annotationlib` 的身份，只看布尔和成员表，不印一堆带安装前缀的路径：

```
$ python3 --version && python3 -c "import sys; print(sys.version)"
Python 3.14.4
3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]
[退出码 0]
```

```
$ python3 -c "
import sys, annotationlib
print(\"annotationlib 在标准库名单里吗 →\", annotationlib.__name__ in sys.stdlib_module_names)
print(\"它的 Format 有几个成员 →\", len(list(annotationlib.Format)), \"｜ 逐个（名字, 值）→\", [(f.name, f.value) for f in annotationlib.Format])
"
annotationlib 在标准库名单里吗 → True
它的 Format 有几个成员 → 4 ｜ 逐个（名字, 值）→ [('VALUE', 1), ('VALUE_WITH_FAKE_GLOBALS', 2), ('FORWARDREF', 3), ('STRING', 4)]
[退出码 0]
```

**① `annotationlib 在标准库名单里吗 → True`** 这一行是本章的入场券：`annotationlib` 是 3.14 新增的标准库模块，
它把「读注解」从一句属性访问做成了一个有四个口径的 API。
**② 那几个成员（名字, 值）→ `[('VALUE', 1), ('VALUE_WITH_FAKE_GLOBALS', 2), ('FORWARDREF', 3), ('STRING', 4)]`** ——
编号本身就是本章后面所有读数的坐标系，第 04 节整篇就在拿这四把钥匙插同一份坏注解。
注意它是 `IntEnum`，所以 `format=1` 和 `format=Format.VALUE` 是同一个东西，报错原文里出现的也是 `<Format.VALUE: 1>` 这种带编号的形状。

包从哪儿来、工具为什么不在场，是本章读者的第一个实际问题。这一段只回答两件事：

```
$ python3 -c "
import os, sys
h = os.path.expanduser(\"~\")
path = os.environ[\"PATH\"].split(os.pathsep)
print(\"PATH 里有没有条目落在家目录下面 →\", any(x.startswith(h) for x in path), \"（True 也只是这台机器的 shell 配置，名字一律不上屏）\")
dp = [x for x in sys.path if \"dist-packages\" in x]
print(\"第三方包是从 sys.path 里来的，不是 PATH：含 dist-packages 字样的条数 →\", len(dp))
print(\"  折掉安装前缀后 →\", [x.replace(sys.base_prefix, \"<pyroot>\") for x in dp])
"
PATH 里有没有条目落在家目录下面 → True （True 也只是这台机器的 shell 配置，名字一律不上屏）
第三方包是从 sys.path 里来的，不是 PATH：含 dist-packages 字样的条数 → 2
  折掉安装前缀后 → ['<pyroot>/local/lib/python3.14/dist-packages', '<pyroot>/lib/python3/dist-packages']
[退出码 0]
```

`第三方包是从 sys.path 里来的，不是 PATH` 这句是写给「`pip install` 完了却 `import` 不到」那种事故的：
`PATH` 管的是可执行文件在哪，`sys.path` 管的才是模块在哪。上面那格量出本机有 2 条 `dist-packages`，
`typeguard` 就落在 `<pyroot>/lib/python3` 那一条里 —— 本章所有第三方结论都只涉及这一个库。

然后是本章的**在场性红线**。读者的第一反应是「装个 mypy 就好了」，本机做不到，而且这不是我口头声明的：

```
$ python3 -c "
import importlib.util as u, sys
tools = [\"mypy\", \"pyright\", \"ruff\", \"black\", \"flake8\", \"pylint\", \"pytest\", \"pytype\"]
missing = sorted(t for t in tools if u.find_spec(t) is None)
print(\"八把静态尺里缺席的 →\", missing, \"｜ 在场的 →\", sorted(set(tools) - set(missing)))
print(\"typeguard 在场吗 →\", u.find_spec(\"typeguard\") is not None)
"
八把静态尺里缺席的 → ['black', 'flake8', 'mypy', 'pylint', 'pyright', 'pytest', 'pytype', 'ruff'] ｜ 在场的 → []
typeguard 在场吗 → True
[退出码 0]
```

```
$ python3 -m pip --version; echo "（上面这行的 rc 与原文，g/f 两篇都要用：本机装不了任何东西）"
/usr/bin/python3: No module named pip
（上面这行的 rc 与原文，g/f 两篇都要用：本机装不了任何东西）
[退出码 0]
```

**八把尺全缺席、在场的为空**，连 `pytest` 都没有；`pip` 那格更直接 —— `/usr/bin/python3: No module named pip`，
所以本章**没有任何一条结论依赖「读者装了东西」**。这一格是第 12 节存在的理由：既然没有静态检查器，
那就用标准库的 `ast` 现搓一把，并且把那把尺的漏网也一起量出来。

本章唯一用得上的那个第三方库要先报清身份，因为它自己不提供版本号：

```
$ python3 -c "
import typeguard, os, pathlib
d = pathlib.Path(os.path.dirname(typeguard.__file__)).parent
info = sorted(x.name for x in d.glob(\"typeguard*dist-info\"))
print(\"typeguard 自己有没有 __version__ 属性 →\", hasattr(typeguard, \"__version__\"))
print(\"版本号只能从目录名读 →\", info)
"
typeguard 自己有没有 __version__ 属性 → False
版本号只能从目录名读 → ['typeguard-4.4.4.dist-info']
[退出码 0]
```

`typeguard 自己有没有 __version__ 属性 → False` 是本机实测；版本号只能从包目录旁边的
`typeguard-4.4.4.dist-info` 这个名字里读。第 10 节会自己再演一遍这件事 —— 凡引用库版本，
都得先证明「我确实能拿到它」，而不是照抄文档。

最后两条是本章的两条「环境副作用」声明。语言环境被折成 `C.UTF-8`，是因为报错原文要跟版本对齐、
不跟 locale 对齐（中文报错里的措辞和 Python 自己的英文 traceback 会混在一张屏上）：

```
$ printf "语言环境：原值 LANG=%s LC_ALL=%s → 现在 LANG=%s LC_ALL=%s LANGUAGE=%s\n" "${ORIG_LANG:-（未设）}" "${ORIG_LC_ALL:-（未设）}" "$LANG" "$LC_ALL" "$LANGUAGE"
语言环境：原值 LANG=zh_CN.UTF-8 LC_ALL=（未设） → 现在 LANG=C.UTF-8 LC_ALL=C.UTF-8 LANGUAGE=C
[退出码 0]
↑ 原值能印出来，是因为 ORIG_LANG / ORIG_LC_ALL 被 export 了 —— 不导出就只能看到两个空括号（上一章踩过）。
  取证脚本里的报错原文全是 Python 自己的（英文），不受这三行影响；你手敲时看到中文提示也正常。
```

```
$ python3 -c "
import os
def n(p):
    p = os.path.expanduser(p)
    return len(os.listdir(p)) if os.path.isdir(p) else \"目录不存在\"
print(\"~/.local/lib 条目数 =\", n(\"~/.local/lib\"), \"｜ ~/.config 条目数 =\", n(\"~/.config\"), \"｜ ~/.bashrc 存在吗 →\", os.path.isfile(os.path.expanduser(\"~/.bashrc\")))
"
~/.local/lib 条目数 = 目录不存在 ｜ ~/.config 条目数 = 31 ｜ ~/.bashrc 存在吗 → True
[退出码 0]
★ 本机有一条会影响读表的事实：用户名与主机名是同一个串，所以第 14 节隐私扫描里那两格数字必然相同 ——
  不是扫描重了两次，是这台机器上本来就是同一个值。
```

`~/.local/lib 条目数 = 目录不存在` 和 `~/.config 条目数 = 31` 这两个数是**跑之前**的基线；
第 14 节跑完之后会再数一遍同一格 —— 两次一样，就是「demo 没往家目录写过东西」的凭证。
这一节最后还钉了一条会影响读表的事实：本机用户名与主机名是同一个串，所以第 14 节隐私扫描里那两格数字必然相同，
不是扫描重了两次。

**这一节记住一句话**：本章的地板是「3.14 + 没有任何静态检查器 + 只有 `typeguard` 一个第三方库」，
这三件事都是当场探出来的，不是文档里抄来的。

---

## 第 02 节 · a 篇完整跑：注解什么时候才被求值（定义时不碰，读时才碰）

这一篇是主线① 的正文。取证对象只有一行：`def f(x: Widget) -> Result`，而 `Widget`、`Result`
在本文件里**从来没有出现过**。按直觉，这一行应该在定义时就 `NameError` —— 屏幕上实际是：函数建起来了，
`f(3)` 照样回 3，炸的是后面某一刻。

```
=== 1. `def` 那一行什么都没查：名字不存在也照样定义成功 ===
  取证对象：`def f(x: Widget) -> Result` —— Widget、Result 在本文件里从来没有出现过。
  上面那行 def 已经过去了（脚本能跑到这里就是证据），接着量它身上有什么：
  │ ① f 本身：名字 / 调用 f(3) / 返回值类型 → OK → ('f', 3, 'int')
  │ ② 函数对象上有 __annotate__ 吗 / 它的类型 → OK → (True, 'function')
  │ ③ __annotate__ 出自哪个文件（只打 basename）→ OK → 'a_lazy_annotations.py'
  │ ④ 第一次读 f.__annotations__ → NameError: name 'Widget' is not defined
  │ ⑤ 换个问法 hasattr(f, '__annotations__') → NameError: name 'Widget' is not defined
  │ ⑥ 第二遍读 f.__annotations__ → NameError: name 'Widget' is not defined
  → ④⑥ 是同一句话，而且**每次读都炸**：失败不会被记下来（第 2 格拿计数器证明）。
  → ⑤ 这条最容易记错：hasattr 只吞 AttributeError，NameError 照原样往上抛 ——
     所以「用 hasattr 试探有没有注解」在 3.14 会把程序带崩，运行时读注解得用 try/except。
  → 断言：def 那行零副作用（① 里 f(3) 的读数 = 3、类型 = int）；④⑤⑥ 三条原文逐字相同 = NameError: name 'Widget' is not defined；② 的类型 = function。
```

**① 到 ⑥ 六行就是本篇的全部地基**：
① `('f', 3, 'int')` 说明 `def` 那行什么都没查；② 函数对象上多了一个 `__annotate__`，它的类型是 `function`
（这就是编译器替你写的那个「取值小函数」）；④⑤⑥ 三条原文**逐字相同**，都是
`NameError: name 'Widget' is not defined`。
最容易被记错的是 ⑤：`hasattr(f, '__annotations__')` 本想安全地试探一下，结果把程序带崩了 ——
`hasattr` 只吞 `AttributeError`，`NameError` 照原样往上抛。所以 3.14 里凡是运行时读注解都得 `try/except` 包着，
这也是本章七支脚本每支都自带一个安全读数的原因。

「失败不缓存、成功才缓存」是口头结论，本篇拿一台仪器把它量成计数器：注解表达式写成自定义类的下标
`Boom[int]`，它每被求值一次就把 `hits` +1。注意这台仪器先用**负对照**证明自己能数出差别：

```
=== 2. 求值「成功才缓存」，失败「不缓存」：拿计数器量（本格的负对照在前） ===
  判据仪器：注解表达式写成自定义类的下标 `Boom[int]`，它每被求值一次就把 hits +1。
  先跑负对照 —— 如果这台仪器数不出差别，下面所有读数都不可信。
  │ 负对照① bad_fn 定义完之后 BoomFail.hits → OK → 0
  │ 负对照② 读 bad_fn.__annotations__ → RuntimeError: 注解表达式自己炸了
  │ 负对照③ 读一遍之后 hits → OK → 1
  │ 负对照④ 再读一遍 bad_fn.__annotations__ → RuntimeError: 注解表达式自己炸了
  │ 负对照⑤ 再读一遍之后 hits → OK → 2
  ↑ 计数从 1 走到 2：这台仪器真的数得出来，所以「读两遍只涨一格」是有意义的读数，
     不是因为「读不到差别所以两次都一样」。
  │ 真实值① ok_fn 定义完之后 Boom.hits → OK → 0
  │ 真实值② 读 ok_fn.__annotations__ → OK → {'x': <class '__main__.Boom'>, 'return': None}
  │ 真实值③ 读一遍之后 hits → OK → 1
  │ 真实值④ 再读一遍之后 hits → OK → 1
  │ 真实值⑤ 两次拿到的是同一个 dict 对象吗（is）→ OK → True
  │ 真实值⑥ get_annotations(VALUE) 读完之后 hits → OK → 1
  │ 真实值⑦ 它给的是同一个 dict 对象吗（is）→ OK → False
  │ 真实值⑧ get_annotations(STRING) 读完之后 hits → OK → 2
  │ 真实值⑨ STRING 给的东西 → OK → {'x': 'Boom[int]', 'return': 'None'}
  → ⑤ True + ③ 只涨到 1：成功那一次把 dict 缓存在函数对象上，之后每次读都复用同一个对象。
  → ⑥⑦ 合起来很阴：get_annotations(VALUE) 不重跑表达式（hits 不动），但它给的是
     一份**新拷贝**（is 为 False）—— 所以改它不会污染函数本体（第 3 格分别改给你看）。
  → ⑧ 是这一格最贵的发现：STRING 口径把表达式**又跑了一遍**（hits 再 +1）。
     「转成字符串」不是查缓存，是把注解表达式在替身环境里重新走一遍 ——
     注解里放有副作用的表达式时，每读一次 STRING 就多跑一次副作用。
  → 断言：负对照那支 hits 逐字 = 0 → 1 → 2（两次的报错原文都是 RuntimeError: 注解表达式自己炸了）；真实值那支 hits 逐字 = 0 → 1 → 1 → 1 → 2，并且 ⑤ = True、⑦ = False、⑨ = {'x': 'Boom[int]', 'return': 'None'}。
```

读这一格只看三处：**负对照那支 `hits` 走 0 → 1 → 2**（两次读都炸、每次都重跑，所以「失败不缓存」是量出来的）；
**真实值那支 `hits` 走 0 → 1 → 1 → 1 → 2**（③ 涨到 1、④ 不动、⑥ 不动，⑧ 才再涨一格）；
**⑤ 是 True 而 ⑦ 是 False**（`obj.__annotations__` 给的是缓存里那个对象本体，`get_annotations` 给的是一份新拷贝）。
⑧ 那一格是整篇最贵的发现：**STRING 口径把表达式又跑了一遍**。「转成字符串」不是查缓存，
是把注解表达式在替身环境里重新走一遍 —— 所以注解里放了有副作用的东西时，
每读一次 4 号口径就多跑一次副作用。

既然缓存里那个 dict 是活的，就能改它：

```
=== 3. 缓存里那个 dict 是活的：改得动，改完再读就是新值（但拷贝改不动本体）===
  │ ① 改之前 → OK → {'x': <class '__main__.Boom'>, 'return': None}
  │ ② cache['x'] = int 之后再读 ok_fn.__annotations__ → OK → {'x': <class 'int'>, 'return': None}
  │ ③ 它和 cache 是同一个对象吗（is）→ OK → True
  │ ④ 往里面加一个**代码里没写过的键** zz，再读的键表（排序）→ OK → ['return', 'x', 'zz']
  │ ⑤ 拷一份 copy = get_annotations(VALUE) → OK → {'x': <class 'int'>, 'return': None, 'zz': list[str]}
  │ ⑥ 把 copy['x'] 改成 str，函数本体那份的 x 是谁 → OK → <class 'int'>
  │ ⑦ 改拷贝之后再读 hits → OK → 2
  │ ⑧ 收拾现场（删 zz、x 改回 Boom）→ OK → {'x': <class '__main__.Boom'>, 'return': None}
  → ②③ 合起来：`f.__annotations__` 不是每次现造的快照，它就是缓存里那个对象本体，
     拿到它的人改它 = 改这个函数的注解（第 09 章「模块属性就是全局状态」的注解版）。
  → ④：加一个新键也留得下来 —— 所以运行时读到的注解 dict 里可能有代码里没写过的键；
     反过来也成立：有人能在你不改源码的前提下往注解里塞东西（框架就是这么干的）。
  → ⑤⑥⑦：get_annotations 给的拷贝改不动本体（⑥ 读到的还是 int），
     它也不重跑表达式（⑦ 的 hits 和第 2 格末尾一样）。
  → 判据：只想读就 `dict(obj.__annotations__)` 拷一份；想改注解就直接改那个 dict，
     但要知道这是进程级、永久的改动（没有自动还原，本脚本用 ⑧ 手动还原）。
  → 断言：② 的读数逐字 = {'x': <class 'int'>, 'return': None}、③ = True、④ 的键表逐字 = ['return', 'x', 'zz']、⑥ = <class 'int'>（拷贝没污染本体）、⑧ 回到 {'x': <class '__main__.Boom'>, 'return': None}。
```

②③ 合起来是：**`f.__annotations__` 不是每次现造的快照，它就是缓存里那个对象本体**，
拿到它的人改它 = 改这个函数的注解；④ 更狠 —— 往里面加一个**代码里没写过的键**也留得下来。
所以「运行时读到的注解 dict 里有你源码里没写的键」是正常现象，框架就是这么往里塞东西的。
反过来 ⑤⑥ 给出安全的读法：只想读就拷一份，拷的那份改不动本体。

四个口径不是平权的。直接调 `__annotate__` 才能看清哪些是编译进去的、哪些是 `annotationlib` 在外面替你做包装：

```
=== 4. `__annotate__` 里只编了 1 号和 2 号口径，3、4 号一调就 NotImplementedError ===
  │ ① h.__annotate__() 不传参数 → TypeError: __annotate__() missing 1 required positional argument: 'format'
  │ ② h.__annotate__(1) → OK → {'x': <class 'int'>, 'y': <class 'str'>, 'return': None}
  │ ② h.__annotate__(2) → OK → {'x': <class 'int'>, 'y': <class 'str'>, 'return': None}
  │ ② h.__annotate__(3) → NotImplementedError: （str(exc) 的 repr = ''，即空串）
  │ ② h.__annotate__(4) → NotImplementedError: （str(exc) 的 repr = ''，即空串）
  │ ③ 换成注解里有未定义名的 f：f.__annotate__(1) → NameError: name 'Widget' is not defined
  │ ④ 同一个 f：f.__annotate__(2) → NameError: name 'Widget' is not defined
  │ ⑤ 同一个 f：f.__annotate__(3) → NotImplementedError: （str(exc) 的 repr = ''，即空串）
  │ ⑥ 同一个 f：f.__annotate__(Format.STRING) → NotImplementedError: （str(exc) 的 repr = ''，即空串）
  │ ⑦ 那个 NotImplementedError 的 str(exc) 逐字（repr）→ OK → "''"
  │ ⑧ f.__annotate__.__code__.co_names（这个函数引用了哪些名字）→ OK → ('Widget', 'Result')
  │ ⑨ Format 的四个成员 [(name, value)] → OK → [('VALUE', 1), ('VALUE_WITH_FAKE_GLOBALS', 2), ('FORWARDREF', 3), ('STRING', 4)]
  → ①② 的分工：1（VALUE）和 2（VALUE_WITH_FAKE_GLOBALS）是**编译进 __annotate__ 的**，
     直接调能出 dict；3（FORWARDREF）和 4（STRING）是 annotationlib 在外面替你做包装
     （3 靠替身 globals、4 靠字符串化替身），编译版根本不存在，直接调只能拿到暗号。
  → ⑦ 消息是空串：except NotImplementedError 之后 str(exc) 什么也读不到 ——
     它是「本口径没有编译版」的暗号，不是线索，排错时别在这句话里找原因。
  → ⑧ 这条把话钉死：__annotate__ 的 co_names 里只有 ('Widget', 'Result') 这种
     注解里的名字，没有 annotationlib / Format —— 它就是编译器替你写的一个取值小函数。
  → 判据：对外一律走 annotationlib.get_annotations(obj, format=...)（第 b 篇）；
     直接调 __annotate__ 只有你自己调试时值得，2 号口径更是内部专用（b 篇有实测）。
  → 断言：h.__annotate__(1) 与 (2) 的读数逐字相同 = {'x': <class 'int'>, 'y': <class 'str'>, 'return': None}；(3)(4) 与 ⑤⑥ 四条全是 NotImplementedError 且 repr(str(exc)) == "''"；① 的原文 = TypeError: __annotate__() missing 1 required positional argument: 'format'；③④ 的原文 = NameError: name 'Widget' is not defined。
```

**只有 1 号和 2 号有编译版**，3、4 号直接调只得到一句**消息为空串**的 `NotImplementedError`（⑦ 专门把 `str(exc)` 的 repr 打出来给你看是空串）。
这条对排错很实际：`except NotImplementedError` 之后你什么线索都读不到，它是「本口径没有编译版」的暗号，不是原因。
⑧ 那条 `co_names → ('Widget', 'Result')` 把 `__annotate__` 的身份钉死了：里面只有注解里的名字，
没有 `annotationlib`、没有 `Format` —— 它就是编译器写的一个取值小函数。

「加引号 = 延迟求值」是本章要拆的第一个错觉：

```
=== 5. 「加引号 = 延迟」是个错觉：引号只是把一个 str 存进去 ===
  │ ① f2.__annotations__（名字根本不存在，但加了引号）→ OK → {'y': 'Missing', 'return': None}
  │ ② get_annotations(f2, VALUE) → OK → {'y': 'Missing', 'return': None}
  │ ③ 里面 'y' 的值类型名 → OK → 'str'
  │ ④ typing.get_type_hints(f2) → NameError: name 'Missing' is not defined
  │ ⑤ 自己 eval 那个字符串 eval(f2.__annotations__['y']) → NameError: name 'Missing' is not defined
  │ ⑥ get_annotations(f2, eval_str=True) → NameError: name 'Missing' is not defined
  → ①②③ 三行一起看：引号包起来之后没有任何东西被求值，存进去的就是那个 str 本身，
     连 VALUE 口径也原样给你 str —— 因为被求值的表达式就是个字符串字面量。
  → ④⑤⑥ 三行一起看：谁想把它当「真类型」用，谁就得自己去 eval，于是那个不存在的名字
     立刻把 NameError 还给你。get_type_hints 会 eval，eval_str=True 也会 eval。
  → 判据：引号能骗过的只有「不读就不炸」这一种场景；只要有人把它当类型用 ——
     get_type_hints（④）、自己 eval（⑤）、get_annotations(eval_str=True)（⑥）——
     那个名字还是得在场。要真的不碰名字，走 b 篇的 STRING / FORWARDREF 口径。
  → 断言：① ② 读数逐字 = {'y': 'Missing', 'return': None}（注意 return 是 None 不是字符串），③ 逐字 = 'str'，④⑤⑥ 三条原文逐字相同 = NameError: name 'Missing' is not defined。
```

①②③ 三行说引号什么都没延迟，存进去的就是那个 `str` 本身（注意 `return` 是 `None` 不是字符串）；
④⑤⑥ 三行说**谁想把它当真类型用，谁就得自己 `eval`**，于是那个不存在的名字立刻把 `NameError` 还回来 ——
`get_type_hints` 会 eval、`eval_str=True` 会 eval、你自己 eval 也一样。
判据：要真的不碰名字，得走第 04 节的 3、4 号口径，不是加引号。

位置决定下场。函数体里的局部变量注解和签名里的注解，运行时完全两回事：

```
=== 6. 函数体里的局部变量注解：跑得过、查不到、运行时哪儿都不存 ===
  │ ① 调用 g() → OK → 1
  │ ② g.__annotations__ → OK → {}
  │ ③ g.__annotate__ → OK → None
  │ ④ 试着调它 g.__annotate__(Format.VALUE) → TypeError: 'NoneType' object is not callable
  │ ⑤ g.__code__.co_varnames（局部注解留下的唯一痕迹）→ OK → ('x',)
  │ ⑥ 注解名字根本不存在的 noteval() 调用 → OK → 3
  │ ⑦ noteval.__annotations__ → OK → {}
  │ ⑧ noteval.__code__.co_varnames → OK → ('q', 'r')
  → ①⑥：函数体里的 `x: Widget = 1` 那行只做赋值，注解那半句连求值都没有 ——
     它不像 def 的签名那样编译出 __annotate__，所以连「以后会炸」都没得炸。
  → ②⑦ 是空 dict（不是缺键、也不是 NameError）：局部注解不进函数的 __annotations__。
  → ③④ 是一对：g.__annotate__ 这个属性存在（继承自函数类型）但值是 None，
     所以「用 hasattr 判断有没有注解」在这里也会骗你 —— 判据是 `__annotate__ is not None`。
  → ⑤⑧ 只剩变量名本身：局部注解在运行时**完全不可读**，
     所以任何想检查它的工具（第 g 篇自制检查器）必须回到源码层（ast），没捷径。
  → 断言：① = 1、⑥ = 3、②⑦ 逐字 = {}、③ 是 None、④ 的原文 = TypeError: 'NoneType' object is not callable、⑤ 逐字 = ('x',)、⑧ 逐字 = ('q', 'r')。
```

②⑦ 是**空 dict**（不是缺键、也不是 NameError）：局部注解不进 `__annotations__`；
③④ 是一对：`g.__annotate__` 这个属性存在但值是 `None`，所以判据必须写 `__annotate__ is not None`，
用 `hasattr` 判「有没有注解」在这里也会骗你；
⑤⑧ 只剩变量名本身（`co_varnames`）—— 局部注解在运行时**完全不可读**，
任何想检查它的工具都必须回到源码层，这就是第 12 节那把 `ast` 尺存在的理由。

类体和模块级的裸注解也不产生名字：

```
=== 7. 类体里的裸注解：类定义得成，读的时候炸，而且不产生任何类属性 ===
  │ ① 类定义成功了：C.__name__ → OK → 'C'
  │ ② C.y → OK → 3
  │ ③ C().y（实例上也拿得到）→ OK → 3
  │ ④ 读 C.__annotations__ → NameError: name 'Widget' is not defined
  │ ⑤ typing.get_type_hints(C) → NameError: name 'Widget' is not defined
  │ ⑥ 'x' in vars(C)（裸注解变成类属性了吗）→ OK → False
  │ ⑦ vars(C) 里非双下划线的键（排序）→ OK → ['y']
  │ ⑧ 'x' in C.__dict__ ｜ getattr(C, 'x', '没这个属性') → OK → (False, '没这个属性')
  │ ⑨ C.__annotate__ 的类型 → OK → 'function'
  → ⑥⑦⑧ 是同一件事：`x: Widget` 这行**不产生 x**。它只把「怎么求 Widget」记进
     __annotate__，所以类体里的裸注解连一个类属性都不绑 —— 别指望 `C.x` 取到类型。
  → ②③ 对照：同一个类体里带值的 `y: int = 3` 就是普通赋值，类和实例都拿得到 3。
  → ④⑤ 是同一句话：一读就把整份注解求值，第一个不存在的名字把整份顶回来 ——
     所以「谁去读类注解谁炸」；反过来第 6 章的 dataclass 建字段时为什么不炸，
     是本篇 ④（2 号口径编译版）留下的伏笔，第 c 篇用计数器量。
  → 断言：⑥ = False、⑦ 逐字 = ['y']、⑧ 逐字 = (False, '没这个属性')、② ③ 都 = 3、④⑤ 原文逐字相同 = NameError: name 'Widget' is not defined、⑨ = 'function'。
```

```
=== 8. 模块级注解：同一个脚本里三种读法，两个**不同消息**的 NameError ===
  │ ① 在脚本自己内部按名字读裸的 __annotations__ → NameError: name '__annotations__' is not defined
  │ ② '__annotations__' in globals() → OK → False
  │ ③ 按属性读 me.__annotations__ → NameError: name 'SomeUndefinedName' is not defined
  │ ④ 属性读炸过之后，globals 里有那个键了吗 → OK → False
  │ ⑤ 再按名字读一次 → NameError: name '__annotations__' is not defined
  │ ⑥ STRING 口径读模块 → OK → {'Y': 'SomeUndefinedName', 'Z': 'AnotherUndefined'}
  │ ⑦ FORWARDREF 口径读模块（值的类型名）→ OK → {'Y': 'ForwardRef', 'Z': 'ForwardRef'}
  │ ⑧ VALUE 口径读模块 → NameError: name 'SomeUndefinedName' is not defined
  │ ⑨ 'Y' in dir()（裸注解那行绑了吗）→ OK → False
  │ ⑩ Z 的值（带值那行绑了）→ OK → 1
  │ ⑪ 模块对象上的 __annotate__ 类型 → OK → 'function'
  → ①和③ 都叫 NameError，但**消息不一样**：① 说的是 '__annotations__' 这个名字不在
     globals 里；③ 说的是注解里的 'SomeUndefinedName' 求值时查不到。
     只看异常类型名就会查错方向：① 是「那个键还没被创建」，③ 才是「注解里的名字不存在」。
  → ②④⑪：模块级 __annotations__ 是个**懒属性**（走 module __getattr__），
     所以模块内部按名字查它查不到；而 __annotate__ 是模块 __dict__ 里的真键。
  → ⑥⑦：不求值也能读，而且两种读法给的形状不同 —— STRING 给源码文本，
     FORWARDREF 把缺的名字包成 ForwardRef 对象（⑦ 两个值的类型名都是 ForwardRef）。
  → ⑨⑩：`Y: SomeUndefinedName` 什么都不绑，`Z: AnotherUndefined = 1` 只绑 Z=1 ——
     模块级注解语句和类体里一样，注解那半句不产生名字。
  → 断言：①⑤ 原文逐字 = NameError: name '__annotations__' is not defined；③⑧ 原文逐字 = NameError: name 'SomeUndefinedName' is not defined；②④ 逐字 = False；⑥ 逐字 = {'Y': 'SomeUndefinedName', 'Z': 'AnotherUndefined'}；⑩ = 1。
```

类体那格的 ⑥⑦⑧ 是同一件事：`x: Widget` 这行**不产生 `x`**，
`'x' in vars(C)` 是 False、`getattr(C, 'x', '没这个属性')` 拿的是兜底值；同一个类体里带值的 `y: int = 3` 就是普通赋值。
模块那格多一个坑：**两种读法都叫 `NameError`，但消息不一样**。
① 说的是 `'__annotations__'` 这个名字不在 `globals` 里（模块级 `__annotations__` 是个走 `module __getattr__` 的**懒属性**，
所以在模块内部按裸名去查它就是查不到）；③ 说的是注解里的 `SomeUndefinedName` 求值时查不到。
只看异常类型名就会查错方向 —— 这一格的价值全在这两条消息的差别上。

第 9 格把上面那句断言逼响：同一份模块内部裸名读法，在「有人成功读过注解」之前返回 `NameError`、
之后就返回 dict；而注解名真不存在的 `lazy_bad` 属性读**失败**，于是那个键也没被留下 ——
跟第 2 格「失败不缓存」完全对上，函数和模块的规矩是一样的。

最后，本篇把自己最要紧的十一条读数改成会响的据（前面的 `│` 行只打印、不判）：

```
=== 10. 核对格：把这篇最要紧的读数改成会响的据（前面各格的 │ 行只打印、不判）===
  [成立] ① 定义那一行什么都没查：f(3) 照样算得出：实测 3 ｜ 预期 3
  [成立] ② 读 f.__annotations__ 连读两遍，报错逐字相同（失败不缓存）：实测 (('NameError', "name 'Widget' is not defined"), ('NameError', "name 'Widget' is not defined")) ｜ 预期 (('NameError', "name 'Widget' is not defined"), ('NameError', "name 'Widget' is not defined"))
  [成立] ③ hasattr 试探被注解的 NameError 打穿（它只吞 AttributeError）：实测 ('NameError', "name 'Widget' is not defined") ｜ 预期 ('NameError', "name 'Widget' is not defined")
  [成立] ④ 成功那一次才缓存：再读拿到的是同一个 dict 对象：实测 True ｜ 预期 True
  [成立] ⑤ STRING 口径每读一遍就把注解表达式重跑一次（hits 增量）：实测 1 ｜ 预期 1
  [成立] ⑥ VALUE 口径命中缓存，不重跑（hits 增量 0）：实测 0 ｜ 预期 0
  [成立] ⑦ 3、4 号没有编译版：直接调只得到空消息的 NotImplementedError：实测 (('NotImplementedError', ''), ('NotImplementedError', '')) ｜ 预期 (('NotImplementedError', ''), ('NotImplementedError', ''))
  [成立] ⑧ 函数体里的局部注解：调用得过、运行时读不到（__annotations__ 是空 dict）：实测 (1, {}, 3) ｜ 预期 (1, {}, 3)
  [成立] ⑨ 类体里的裸注解不产生类属性，带值的那行照常绑：实测 (False, 3, 1) ｜ 预期 (False, 3, 1)
  [成立] ⑩ 引号 ≠ 延迟：VALUE 原样给 str，get_type_hints 却自己去 eval：实测 ({'y': 'Missing', 'return': None}, ('NameError', "name 'Missing' is not defined")) ｜ 预期 ({'y': 'Missing', 'return': None}, ('NameError', "name 'Missing' is not defined"))
  [成立] ⑪ 模块里两种读法的报错**消息不同名**：键不存在 vs 注解名不存在：实测 (('NameError', "name '__annotations__' is not defined"), ('NameError', "name 'SomeUndefinedName' is not defined")) ｜ 预期 (('NameError', "name '__annotations__' is not defined"), ('NameError', "name 'SomeUndefinedName' is not defined"))
  → 断言：第 1~9 格最要紧的十一条读数逐字钉住：定义时不求值、失败不缓存、成功才缓存、STRING 重跑而 VALUE 不重跑、3/4 号无编译版、体内注解运行时不可读、裸注解不绑名字、引号只是 str、模块两种读法消息不同名 ｜ 本格判据 11/11 条成立（不成立的那条上面会印 [不成立！]）
```

**这一节记住一句话**：注解是一段「编译好但还没跑」的代码，`def` 那一刻一行都不执行；
第一次读才求值，成功才缓存、失败不缓存，而 4 号 STRING 口径每读一遍就把表达式重跑一遍。

---

## 第 03 节 · 现场格：两份只差一行的文件并排 —— future 开关改的是求值时机，不是可读性

`a`、`b` 两篇里都出现过 `from __future__ import annotations`，但它们各自只能演自己那半。
这一格把「开」和「关」做成沙箱里两个真文件，**同一个进程里 import 两个模块**，一次读注解直接给两种形状 ——
因为这个开关的效果只在文件级发生，必须并排才看得出来它是「编译期的选择」。

```
$ mkdir -p fut-check && cd fut-check
cat > off.py <<PY
class Known: pass
def fn(k: Known, q: "Missing2") -> int: return 0
PY
cat > on.py <<PY
from __future__ import annotations
class Known: pass
def fn(k: Known, q: "Missing2") -> int: return 0
PY
python3 -c "
import re
def fold(s): return re.sub(r\"0x[0-9a-f]+\", \"<地址>\", str(s))
for tag, mod in ((\"不加 future\", \"off\"), (\"加了 future\", \"on\")):
    m = __import__(mod)
    a = m.fn.__annotations__
    print(f\"{tag} → 每个注解的【类型】：\", {k: type(v).__name__ for k, v in a.items()})
    print(f\"{tag} → 值本身（折过地址）：\", {k: fold(v) for k, v in a.items()})
"
cd ..; rm -rf fut-check
不加 future → 每个注解的【类型】： {'k': 'type', 'q': 'str', 'return': 'type'}
不加 future → 值本身（折过地址）： {'k': "<class 'off.Known'>", 'q': 'Missing2', 'return': "<class 'int'>"}
加了 future → 每个注解的【类型】： {'k': 'str', 'q': 'str', 'return': 'str'}
加了 future → 值本身（折过地址）： {'k': 'Known', 'q': "'Missing2'", 'return': 'int'}
[退出码 0]
```

两份文件只差开头那一行 `from __future__ import annotations`，函数体一个字没改，
而注解里故意塞了一个带引号的不存在名字 `"Missing2"`。四行读数里藏着本章最容易记混的一件事：

- 不加 future：`{'k': 'type', 'q': 'str', 'return': 'type'}` —— 能求值的两个注解求成了真类型对象，
  带引号那个保持 `str`。
- 加了 future：`{'k': 'str', 'q': 'str', 'return': 'str'}` —— **所有**注解都成了 `str`，
  包括那个本来完全能求值的 `Known`。
- 而 `q` 那一格在两种开关下都是 `str`：不加时是 `'Missing2'`，加 future 后变成 `"'Missing2'"`（多一层引号，因为它被原样字符串化了）。

于是「加了引号」和「开了开关」在下游看起来**几乎一模一样** —— 都是拿到一个字符串。
差别不在可读性，在于「能不能拿到真对象」：加了引号只影响那一个注解，开了 future 影响整份文件，
而下游谁想把文本变回类型，谁就得自己去 eval，那个不存在的名字照样会把 `NameError` 还回来。
第 04 节第 5 格把这条混淆量成了逐字对比，第 07 格再把 future 分支下四个口径的读数全摆出来。

★ 顺带一条红线：这一格的 `fold()` 把 `0x…` 折成 `<地址>`，因为 `ForwardRef` 的 repr 里带函数对象地址
（`owner=<function … at 0x…>`）。第 05 节那张矩阵用的是同一个 `fold()`，七支取证脚本里则是各自的 `mask()` ——
屏幕上出现一次地址，就意味着某条 repr 被整坨打出来了。

**这一节记住一句话**：future 那一行改的是**求值时机**（整份文件的注解都别求值），不是「让注解更安全」；
它和加引号在下游是同一个形状，所以任何下游拿到字符串都得自己决定要不要 eval。

---
## 第 04 节 · b 篇完整跑：读注解的四个入口，和它们各自的规矩

读注解这件事，读者的直觉是「只有一种读法」：拿 `__annotations__` 看就是了，`format=` 那种参数是文档生成器才关心的可选项；
至于「注解加引号就能延迟」这类江湖说法，听起来只要加上引号，从此怎么读都安全。

屏幕上实际是：3.14 把「读注解」做成了一张有编号的表 —— `annotationlib.Format` 一共四格，
把同一支「注解里的名字不存在」的函数依次插进去，四把钥匙给出四种完全不同的东西：
一把 `NameError`、一把 `ValueError`、一把 `ForwardRef` 对象、一把源码文本。
而且 `get_annotations` 的**默认**那把，恰好是会炸的那把。先把编号牌立起来，下面每一格都按这个顺序插：

```
=== 1. 四把钥匙的编号牌：annotationlib.Format 有哪几格 ===
  │ ① [(m.name, m.value) for m in Format] → OK → [('VALUE', 1), ('VALUE_WITH_FAKE_GLOBALS', 2), ('FORWARDREF', 3), ('STRING', 4)]
  │ ② Format.__members__ 的键（声明顺序）→ OK → ['VALUE', 'VALUE_WITH_FAKE_GLOBALS', 'FORWARDREF', 'STRING']
  │ ③ Format(4).name ｜ Format['STRING'].name → OK → ('STRING', 'STRING')
  │ ④ isinstance(Format.VALUE, int) ｜ Format.STRING == 4 → OK → (True, True)
  │ ⑤ get_annotations 的签名（默认 format 是哪个）→ OK → '(obj, *, globals=None, locals=None, eval_str=False, format=<Format.VALUE: 1>)'
  → ①② 是这张表的表头：四格的编号是 1~4，后面每一格都按这个顺序插一遍。
  → ④ 说明 Format 是 IntEnum：所以 a 篇里 `h.__annotate__(3)` 那种裸整数也认，
     读源码时看到 `format=2` 别以为是布尔，它是 VALUE_WITH_FAKE_GLOBALS。
  → ⑤ 里 `format=<Format.VALUE: 1>`：不传 format 的 get_annotations 就是 1 号，
     也就是「会炸的那一把」—— 这是最反直觉的默认值，a 篇第 1 格已经撞过一次。
  → 断言：① 逐字 = [('VALUE', 1), ('VALUE_WITH_FAKE_GLOBALS', 2), ('FORWARDREF', 3), ('STRING', 4)]；④ 逐字 = (True, True)；⑤ 里出现 format=<Format.VALUE: 1>。
```

- **①②** 表头钉死了：1 VALUE、2 VALUE_WITH_FAKE_GLOBALS、3 FORWARDREF、4 STRING，声明顺序也是这个顺序。
  后面凡是说「几号钥匙」，都按这张表读。
- **③④** `Format['STRING'].name` 反查得到，`isinstance(Format.VALUE, int)` 为真、`Format.STRING == 4` 也为真 ——
  它是个 `IntEnum`。所以 a 篇里 `f.__annotate__(3)` 那种裸整数也认；读到源码写 `format=2`，别以为是某个布尔开关，那是 2 号。
- **⑤** 这一行是全节的地基：签名里明写着默认 `format=<Format.VALUE: 1>`。不传参数的默认读法就是最脆的那把，
  于是「凡读注解必先 try」不是谨慎，是唯一走得通的写法。

主现场来了 —— 同一支函数（注解里两个名字都不存在）插四把钥匙：

```
=== 2. 同一个 f（注解里两个名字都不存在）插四把钥匙：只有 3、4 两把不炸 ===
  │ ① 1 号 VALUE → NameError: name 'Widget' is not defined
  │ ② 2 号 VALUE_WITH_FAKE_GLOBALS → ValueError: The VALUE_WITH_FAKE_GLOBALS format is for internal use only
  │ ③ 3 号 FORWARDREF（值的类型名，repr 带对象地址所以不印）→ OK → {'x': 'ForwardRef', 'return': 'ForwardRef'}
  │ ④ 4 号 STRING → OK → {'x': 'Widget', 'return': 'Result'}
  │ ⑤ 顺手量不传 format（默认 1 号）→ NameError: name 'Widget' is not defined
  │ ⑥ 2 号绕过 get_annotations，直接 f.__annotate__(2) → NameError: name 'Widget' is not defined
  │ ⑦ 对照：同一个 2 号直接插在注解合法的 plain_fn 上 → OK → {'x': <class 'int'>, 'y': <class 'str'>, 'return': None}
  → ② 是这一格最该记住的一行：2 号在**公开入口**上被直接拒绝，报的还是 ValueError，
     连注解名字都不看就拦下来了 —— 它是 dataclass 这类标准库内部用的后门
     （a 篇第 4 格证明它确实编译进了 __annotate__，但那不是给你我用的）。
  → ⑥⑦ 两行一起看：绕过公开入口之后，2 号在注解名字不存在时**照样 NameError**，
     在名字合法时能出 dict —— 所以「2 号能救未定义的名字」是彻底错的，
     它救的是「求值时不想让模块 globals 被看见」（fake globals 的字面意思），不是救名字不存在。
  → ③④ 是不炸的两把：3 号给你 ForwardRef 对象、4 号给你源码文本 str。
  → 断言：① ⑤ 原文逐字 = NameError: name 'Widget' is not defined；② 原文逐字 = ValueError: The VALUE_WITH_FAKE_GLOBALS format is for internal use only；⑥ 原文 = NameError: name 'Widget' is not defined；⑦ 逐字 = {'x': <class 'int'>, 'y': <class 'str'>, 'return': None}；③ 逐字 = {'x': 'ForwardRef', 'return': 'ForwardRef'}；④ 逐字 = {'x': 'Widget', 'return': 'Result'}。
```

- **①⑤** 1 号炸；「不传 format」炸出来的和它是同一句话 —— 正面印证上一格的 ⑤。
- **②** 2 号在公开入口直接被 `ValueError` 拒收，理由写的是「内部专用」，注解长什么样它压根没看 —— 这把钥匙轮不到普通代码用。
- **⑥⑦** 这两行一起看才说明问题：绕过公开入口把 2 号直接塞给 `__annotate__`，坏名场景**照样** `NameError`；
  换一支注解全合法的函数，它才给出装着真类对象的 dict。结论很干脆：2 号既不救名字，也不改变炸不炸。
  它名字里的 fake globals 指的是「求值时别让模块 globals 露出来」，跟名字存不存在无关。
- **③④** 只有这两把递给你东西：3 号是一块 `ForwardRef` 牌子（屏上只印类型名，因为 repr 带 `<地址>`），4 号是源码里那段文本。

3 号钥匙带回来的那块牌子，规矩比想象的多，而且 3.14 里它只有一家在用：

```
=== 3. 3 号钥匙给你的 ForwardRef 对象：能问名字，能自己给环境求值 ===
  │ ① type(fr) 的模块.类名 → OK → 'annotationlib.ForwardRef'
  │ ② fr.__arg__（它装的那个名字）→ OK → 'Widget'
  │ ③ str(fr) 的原文（对象地址当场折掉）→ OK → "ForwardRef('Widget', owner=<function f at <地址>>)"
  │ ④ fr == annotationlib.ForwardRef('Widget') 吗 → OK → False
  │ ⑤ 两个都不带 owner 的呢：ForwardRef('Widget') == ForwardRef('Widget') → OK → True
  │ ⑥ fr.evaluate(locals={'Widget': int}) → OK → <class 'int'>
  │ ⑦ fr.evaluate() 空手求值 → NameError: name 'Widget' is not defined
  │ ⑧ fr 身上非双下划线的成员 → OK → ['_evaluate', 'evaluate']
  → ④⑤ 合起来是「ForwardRef 的相等算 owner」：③ 那个 owner=<function f at …> 不是装饰，
     owner 参与相等判断，所以你**不能**拿 `ForwardRef('Widget')` 去当键查表。
     要比较只比名字：用 ② 的 `fr.__arg__` 或先 `evaluate`。
  → ⑥⑦ 是「延迟求值」真正落地的那一步：3 号钥匙只是把名字带回来，
     求不求、拿什么环境求，是你自己的事；给不出环境（⑦）还是 NameError。
  → 判据：`fr.evaluate(globals=..., locals=...)` 全是关键字参数（⑥ 那种写法），
     位置参数会 TypeError；读到的值要落定就先 evaluate，别把 ForwardRef 当类型传下去。
  → 断言：① 逐字 = 'annotationlib.ForwardRef'；② 逐字 = 'Widget'；④ = False 而 ⑤ = True；⑥ 逐字 = <class 'int'>；⑦ 原文 = NameError: name 'Widget' is not defined；⑧ 逐字 = ['_evaluate', 'evaluate']。

=== 4. ForwardRef 这块牌子在 3.14 只有一家在用：typing 的是别名，而且没有 __evaluate__ ===
  │ ① annotationlib.ForwardRef is typing.ForwardRef → OK → True
  │ ② typing.ForwardRef('Widget') 的真实类型 → OK → 'annotationlib.ForwardRef'
  │ ③ issubclass(annotationlib.ForwardRef, str) → OK → False
  │ ④ typing.ForwardRef('int').__evaluate__ → AttributeError: 'ForwardRef' object has no attribute '__evaluate__'
  │ ⑤ annotationlib.ForwardRef('int').evaluate()（换成 3.14 的真名字）→ OK → <class 'int'>
  │ ⑥ ForwardRef.__mro__ → OK → ['ForwardRef', 'object']
  │ ⑦ typing 里还有别的 ForwardRef 吗（typing 命名空间里带 forward 的名字）→ OK → ['ForwardRef', '_make_forward_ref', 'evaluate_forward_ref']
  → ①②⑥⑦ 四条合起来把「两家在用」这个说法纠正掉：3.14 里 ForwardRef 这个类就一个，
     住在 annotationlib，typing.ForwardRef 只是同一个对象的别名（`is` 为 True）。
  → ③ 是防错的第二条：ForwardRef **不是** str 的子类 —— 别把它当字符串拼接或 .startswith，
     它是个带 owner 的普通对象。
  → ④ 的原文很关键：AttributeError: 'ForwardRef' object has no attribute '__evaluate__' ——
     旧草案里那个双下划线的 __evaluate__ 在本机 3.14.4 不存在，能用的是 ⑤ 的 evaluate()；
     抄旧文章的 `fr.__evaluate__(globals, locals)` 在这儿直接 AttributeError。
  → ⑤ 那一行是「空手 evaluate」的下场：传的是内置名 int，所以缺省环境（builtins 兜底）
     就能求出来；换成第 3 格 ⑦ 那个自定义名 Widget，空手就是 NameError ——
     evaluate 的缺省环境只到 builtins，不到你的模块 globals。
  → 断言：① = True、② 逐字 = 'annotationlib.ForwardRef'、③ = False、④ 原文逐字 = AttributeError: 'ForwardRef' object has no attribute '__evaluate__'、⑤ 逐字 = <class 'int'>、⑥ 逐字 = ['ForwardRef', 'object']、⑦ 逐字 = ['ForwardRef', '_make_forward_ref', 'evaluate_forward_ref']。
```

- **第 3 格 ①②** 它的类型住在 `annotationlib`，装的名字在 `__arg__` 里 —— 想知道它指谁，读 `__arg__`，别去 parse 它的 repr。
- **第 3 格 ④⑤** 带着 owner 的那块牌，和当场新建的同名牌**不相等**；只有两边都不带 owner 才相等。
  实用结论：**别拿 ForwardRef 当字典键**，要比就比名字，或者先求值再比。
- **第 3 格 ⑥⑦⑧** `evaluate(...)` 才是「延迟」真正落地的一步：它只接关键字参数，写成位置参数会 `TypeError`；
  环境给不出来还是 `NameError`。牌子身上公开的成员只有一个。
- **第 4 格 ①②③⑥** `typing.ForwardRef` 就是 `annotationlib.ForwardRef`（`is` 为真，只是别名），继承链只有两级，
  而且它**不是** `str` 的子类 —— 别对它做拼接或前缀判断。
- **第 4 格 ④⑤** 是全节最容易照抄出错的地方：旧草案里那个双下划线的 `__evaluate__` 在本机 3.14 不存在，
  抄旧文章的写法当场撞 `AttributeError`，能用的是 `evaluate()`；而它对内置名空手能成、对自定义名不能 ——
  缺省环境只兜到 builtins，不兜到你的模块 globals。

这一节最反直觉的是第 5 格：你以为「加引号」和「不加引号」是两件不同的事，读注解的人能查出来谁加了；
屏幕上是不一样的一回事 —— 换到 4 号口径，两者产物**逐字相等**：

```
=== 5. 加引号 vs 不加引号，在 4 号钥匙下长得一模一样（分不清就是分不清）===
  │ ① bare(q: Missing) 用 STRING 读 → OK → {'q': 'Missing', 'return': 'None'}
  │ ② quoted(q: 'Missing') 用 STRING 读 → OK → {'q': 'Missing', 'return': 'None'}
  │ ③ 两者 STRING 的值相等吗 → OK → True
  │ ④ bare 用 VALUE 读 → NameError: name 'Missing' is not defined
  │ ⑤ quoted 用 VALUE 读 → OK → {'q': 'Missing', 'return': None}
  │ ⑥ quoted 用 FORWARDREF 读（值的类型名）→ OK → {'q': 'str', 'return': 'NoneType'}
  │ ⑦ bare 用 FORWARDREF 读（值的类型名）→ OK → {'q': 'ForwardRef', 'return': 'NoneType'}
  → ①②③ 三行是本格的全部内容：4 号钥匙读出来的都是字符串 'Missing'，
     `q: Missing` 和 `q: "Missing"` 在 STRING 口径下**逐字相等**，读的人无从分辨。
  → ④⑤⑥⑦ 是分辨得开的两把：VALUE 下裸名炸、引号给 str；
     FORWARDREF 下裸名给 ForwardRef、引号给 str（⑥ 和 ⑦ 的类型名不一样）。
  → 判据：一旦代码改成 STRING 口径（或加了 future 那行），你写在注解里的引号就
     从「多余」变成「无痕」—— 两种写法产物相同，也就没法靠读注解把写法查出来，
     要查只能读源码（ast，第 g 篇）。
  → 断言：① ② 读数逐字相同 = {'q': 'Missing', 'return': 'None'}；③ = True；④ 原文 = NameError: name 'Missing' is not defined；⑤ 逐字 = {'q': 'Missing', 'return': None}；⑥ 逐字 = {'q': 'str', 'return': 'NoneType'}；⑦ 逐字 = {'q': 'ForwardRef', 'return': 'NoneType'}。
```

- **①②③** 4 号读出来都是文本，裸名和加引号给出的读数一模一样，连相等判断都是真 —— 读注解这条路根本查不出写法。
- **④⑤** 1 号口径分得开：裸名炸，加引号的那个给出一个 `str` 值（它把引号里的东西当字面量，而不是「延迟」）。
- **⑥⑦** FORWARDREF 也分得开，但证据在**类型名**上：加引号那条退化成 `str`，裸名那条才是 `ForwardRef`。
  判据只有一条：代码只要改成 4 号口径（或者加上 future 那行），你写的引号就从「多余」变成「无痕」，
  要查写法只能回去读源码（本章 g 篇的 `ast`）。

再看「别人手里的钥匙」—— 那些不是你写的、你也改不了的下游：

```
=== 6. 别人手里的钥匙：inspect.signature 和 typing.get_type_hints 默认都插 1 号 ===
  │ ① typing.get_type_hints(f) → NameError: name 'Widget' is not defined
  │ ② inspect.signature(f) → NameError: name 'Widget' is not defined
  │ ③ inspect.signature 的 annotation_format 默认值 → OK → <Format.VALUE: 1>
  │ ④ signature(f, annotation_format=Format.STRING) → OK → "(x: 'Widget') -> 'Result'"
  │ ⑤ signature(f, annotation_format=Format.FORWARDREF) → OK → '(x: Widget) -> Result'
  │ ⑥ signature(f, eval_str=True) → NameError: name 'Widget' is not defined
  │ ⑦ signature(quoted)（引号那支，默认 1 号）→ OK → "(q: 'Missing') -> None"
  │ ⑧ typing.get_type_hints(quoted) → NameError: name 'Missing' is not defined
  → ①②⑥ 三条全是 NameError：get_type_hints 和 signature 都不接 3、4 号（⑥ 只是把
     字符串再 eval 一次），所以「文档字符串生成器 / 序列化库 / 老代码里的 signature()」
     在注解名字不存在时照样把你的程序读崩 —— 它们不是你写的，你改不了。
  → ③ 是这一格的解药：3.14 的 inspect.signature 多了一个 annotation_format 参数，
     默认值就是 1 号；显式换成 4 号（④）或 3 号（⑤）就能读出来。
  → ④ 那个读数 `(x: 'Widget') -> 'Result'` 里多了引号 —— 这就是「读到了文本」的样子，
     别以为是 bug：它跟第 ⑤ 格里 quoted 的产物长一样。
  → ⑦⑧ 合起来：同一支引号注解，signature 放过（str 就是合法注解）、get_type_hints 炸 ——
     所以「加了引号到底安不安全」取决于**谁在读**，不取决于引号本身。
  → 断言：① ② ⑥ 原文逐字 = NameError: name 'Widget' is not defined；③ 逐字 = <Format.VALUE: 1>；④ 逐字 = "(x: 'Widget') -> 'Result'"；⑤ 逐字 = '(x: Widget) -> Result'；⑦ 逐字 = "(q: 'Missing') -> None"；⑧ 原文 = NameError: name 'Missing' is not defined。
```

- **①②⑥** `typing.get_type_hints`、`inspect.signature`，以及给 `signature` 加 `eval_str=True`，三条全是 `NameError`：
  它们只走 1 号口径。崩点常常在别人的代码里（自动文档、序列化、调试工具），你改不了，只能从注解这一侧躲。
- **③** 3.14 的 `inspect.signature` 多了一个 `annotation_format` 参数，默认值就是 `<Format.VALUE: 1>` ——
  这一行是唯一的解药：显式换成 4 号或 3 号才读得出来。
- **④⑤** 两行读数长得不一样，各有用处：4 号给的样子里注解**带引号**（那是「读到文本」的形状，不是 bug），
  3 号给的样子看起来像正常签名。拿去打印给人看，选哪把要事先想好。
- **⑦⑧** 同一支加了引号的注解：`signature` 放过（`str` 本身就是合法注解），`get_type_hints` 照炸。
  引号到底安不安全，问的是读者，不是引号。

第 7 格把那句 `from __future__ import annotations` 做成两份只差一行的源码，开两个子进程对照跑
（那句开关只能在文件最开头生效，同一进程里切不了，所以必须两个进程各跑一遍）：

```
=== 7. `from __future__ import annotations` 那一行：两份只差一行的源码，子进程对照跑 ===
  现场：reads-demo/plain_case.py（无 future 行）和 reads-demo/future_case.py（正文逐字相同，只在第一行多了那句）；子进程 = python3 + cwd=reads-demo + 干净 env。
  为什么只能这么跑：那句 future 只能在**文件/字符串最开头**生效，同一个进程里切不了，
  所以「一行改变一切」必须开两个进程各跑一遍，两份源码共用同一段正文。
  ┌ 不带 future（plain_case.py 的原始输出）
  │ ① f.__annotations__ → NameError: name 'Widget' is not defined
  │ ② get_annotations VALUE → NameError: name 'Widget' is not defined
  │ ③ get_annotations FORWARDREF 的值的类型名 → OK: {'x': 'ForwardRef', 'return': 'ForwardRef'}
  │ ③b FORWARDREF 里装的那个名字（不印 repr，它带对象地址） → OK: {'x': 'Widget', 'return': 'Result'}
  │ ④ get_annotations STRING → OK: {'x': 'Widget', 'return': 'Result'}
  │ ⑤ get_annotations VALUE_WITH_FAKE_GLOBALS → ValueError: The VALUE_WITH_FAKE_GLOBALS format is for internal use only
  │ ⑤b 直接调 f.__annotate__(2) → NameError: name 'Widget' is not defined
  │ ⑥ typing.get_type_hints(f) → NameError: name 'Widget' is not defined
  │ ⑦ 直接调 f.__annotate__(Format.STRING) → NotImplementedError: （空消息）
  │ ⑧ FORWARDREF 里还有没有 ForwardRef 对象（值的类型名集合） → OK: ['ForwardRef']
  └ 退出码 = 0
  ┌ 带 future（future_case.py 的原始输出）
  │ ① f.__annotations__ → OK: {'x': 'Widget', 'return': 'Result'}
  │ ② get_annotations VALUE → OK: {'x': 'Widget', 'return': 'Result'}
  │ ③ get_annotations FORWARDREF 的值的类型名 → OK: {'x': 'str', 'return': 'str'}
  │ ③b FORWARDREF 里装的那个名字（不印 repr，它带对象地址） → OK: {'x': 'Widget', 'return': 'Result'}
  │ ④ get_annotations STRING → OK: {'x': 'Widget', 'return': 'Result'}
  │ ⑤ get_annotations VALUE_WITH_FAKE_GLOBALS → ValueError: The VALUE_WITH_FAKE_GLOBALS format is for internal use only
  │ ⑤b 直接调 f.__annotate__(2) → OK: {'x': 'Widget', 'return': 'Result'}
  │ ⑥ typing.get_type_hints(f) → NameError: name 'Widget' is not defined
  │ ⑦ 直接调 f.__annotate__(Format.STRING) → NotImplementedError: （空消息）
  │ ⑧ FORWARDREF 里还有没有 ForwardRef 对象（值的类型名集合） → OK: ['str']
  └ 退出码 = 0
```

- **①②④**（两份各看一遍）不带 future 时属性直读和 1 号都炸、4 号给文本；带 future 后这三行全变成文本 ——
  那一行让 1 号口径直接返回文本，读起来就不炸了。
- **⑥** 两边**都**炸，读数逐字相同：`get_type_hints` 拿到字符串就去 eval，名字不在它那边的 globals 里。
  换句人话：那一行只保护自己去读注解的代码，保护不了替你 eval 注解的第三方。
- **③ + ⑧** 最容易被看漏的坑：不带 future 时 FORWARDREF 给 `ForwardRef`，带 future 时给的全是 `str`
  （⑧ 那个集合里一个 ForwardRef 都不剩）。future 分支下你想用 3 号钥匙分辨「谁没定义」，分辨不出来。
- **③b** 两行的读数**逐字相同**（都是那两个名字），只看这一行你分不清自己在哪个分支，必须回头看 ③ 的类型名或 ⑧ 的集合。
- **⑤⑤b** 2 号在公开入口永远被 `ValueError` 拦，跟 future 无关；绕过公开入口直调时，不带 future 照样 `NameError`，
  带 future 却直接给文本。可见两个分支里 2 号都不是护身符：future 分支不炸，是那一行改写了全部注解，与 2 号无关。
- **⑦** 两边都不动：`__annotate__` 从来没编出 4 号那条路，加不加 future 都是 `NotImplementedError`，而且是空消息 ——
  a 篇第 4 格的结论在这里原样复现。两个子进程退出码都是 0。

b 篇剩下三格量的是「这把工具本身有多大」：`annotationlib` 自己声明的公开 API 有 8 个，
`dir()` 里下划线开头的私有名字有 24 个，另外还有 6 个是它 import 进来顺手漏在 `dir()` 里的**模块名**
（`ast`、`builtins`、`enum`、`keyword`、`sys`、`types`）—— 那不是 API，是你家门口的电线杆，要用就自己 import。
三个工具函数各有边界：`type_repr` 给的是「写回源码时的样子」（联合类型给新式写法，函数和类给「模块名.名字」，不带路径）；
`annotations_to_string` 吃的是**字典**，喂函数进去是 `AttributeError`；
`get_annotate_from_class_namespace` 在只有旧式 `__annotations__` 的命名空间里返回 `None`，
有真 `__annotate__` 才原样把函数给你（同一个对象的判断为真）—— 写元类或类装饰器要搬这个钩子就用它，别自己翻 `vars()`。
最后一格给出 future 那行落到实处的**开关位**：只有**模块级**注解语句（裸的或带值的）会在模块 globals 里留下
`__conditional_annotations__`，一个 `set`，每条模块级注解往里加一个下标；函数签名、函数体、类体里的注解都不产生它。
读它的人正是模块自己那个 `__annotate__`，它一进门先看这个集合，再决定按 1 号求值还是直接给文本。
顺带一句给 g 篇埋的伏笔：`symtable` 只看符号表，注解表达式里面的名字它一个都不给。

**这一节记住一句话**：读注解有四把钥匙，默认那把（1 号）就是会炸的那把，能救你的只有 3 号和 4 号 —— 而 3 号把求值这件事推给你自己，4 号给你的是文本，加了引号或加了 future 都不等于安全。

---

## 第 05 节 · 矩阵格：一份三种坏法并排的函数，五种读法各给什么（跨 a/b 两篇的合并视图）

a、b 两篇各自在自己的进程里一格一格演，跑到后面脑子里的东西很容易糊。这一格把镜头拉平：
现造一支签名里同时摆着三种「坏法」的函数 —— 一个真存在的类、一个裸写的未定义名、一个加了引号的未定义名 ——
然后一次性喂给五个入口。直觉是「要炸一起炸」，屏幕上却是**一张表里同时给出四种形状**：
两条报错、一个装着混合类型的 dict、一份全字符串，最后还有一条绕过去的路。

```
a、b 两篇各自在自己的进程里演。这一格把「同一份签名」一次喂给五个入口，
  屏上一张表看完：哪个炸、炸成什么、哪个给你 str、哪个给你 ForwardRef。

$ python3 matrix_reads.py
```

```
被读的那个签名：def fn(k: Known, bad: Missing1, q: "Missing2") -> int
  ① VALUE(1) → 炸 NameError: name 'Missing1' is not defined
  ① VALUE_WITH_FAKE_GLOBALS(2) → 炸 ValueError: The VALUE_WITH_FAKE_GLOBALS format is for internal use only
  ① FORWARDREF(3) → OK {'k': <class '__main__.Known'>, 'bad': ForwardRef('Missing1', owner=<function fn at <地址>>), 'q': 'Missing2', 'return': <class 'int'>}
  ① STRING(4) → OK {'k': 'Known', 'bad': 'Missing1', 'q': 'Missing2', 'return': 'int'}
  ② typing.get_type_hints → 炸 NameError: name 'Missing1' is not defined
  ③ inspect.signature → 炸 NameError: name 'Missing1' is not defined
  ④ 属性读 f.__annotations__ → 炸 NameError: name 'Missing1' is not defined
  ⑤ 唯一的绕法：先把 STRING 读法的结果**写回** __annotations__，再读一次
     写回后 VALUE → {'k': 'Known', 'bad': 'Missing1', 'q': 'Missing2', 'return': 'int'}
     写回后 signature → (k: 'Known', bad: 'Missing1', q: 'Missing2') -> 'int'
[退出码 0]
```

- **① 1 号** 只报第一个坏名字 —— 求值是**从左往右**撞墙的，带引号那条排在它后面，连「有没有问题」都没被问到。
  想靠这条报错清点有几个坏注解，数不出来。
- **① 2 号** 又是那句「内部专用」的 `ValueError`，跟注解内容无关，公开入口直接拒收 —— 与 b 篇第 2 格 ② 逐字对得上。
- **① FORWARDREF** 是这张表里最值得盯的一行：同一个 dict 里四个值**类型不齐** ——
  已知的给真类对象、裸坏名给 `ForwardRef`、带引号那条给 `str`、返回值又给真类对象。
  这一行把 b 篇第 3 格（牌子长什么样）和第 5 格（引号在这口径下退化成 `str`）两格拼到了一起。
- **① STRING** 四条全变文本，三种坏法在这个口径下**看不出区别** —— 这是 b 篇第 5 格结论的合并版。
- **②③④** `get_type_hints`、`inspect.signature`、属性直读 `__annotations__` 三条入口，报的是**同一个** `NameError`。
  「换个工具是不是就不炸」这个问题，在这里得到答案：没有换。
- **⑤** 表里最后那条是**绕过去**的路：先用 STRING 读一遍，再把结果赋回 `__annotations__`，于是 1 号口径读得通了 ——
  但它拿到的是字符串，`signature` 打出来也带着引号。
  属性读 `__annotations__` 是活的、可写的，这是 a 篇第 3 格那个「活字典」的用途版。

```
↑ 五行表里最值得记住的是最后两行：注解**可以写回去**（__annotations__ 是个普通可写属性），
  于是 inspect 那类「读 VALUE 就炸」的工具能被绕过 —— 但绕过去之后它拿到的是 str 而不是类对象，
  这不是「修好了」，是「把求值这件事推给了自己」。b 篇第 11 格量的是同一条路的另一头。
```

**这一节记住一句话**：同一份坏注解摆在五个入口前给出五种东西 —— 炸的三条是同一个错，不炸的两条一个给你混合类型、一个给你纯文本，而想让下游闭嘴靠的是把文本塞回去，不是把注解修对。

---

## 第 06 节 · c 篇完整跑：到底谁在读注解 —— dataclass / inspect / TYPE_CHECKING 三本账

这一篇的直觉最硬：`@dataclass` 是**靠注解**建 `__init__` 的，那注解里写了个不存在的名字，
总该在类装饰器那一行就炸吧？屏幕上偏偏不 —— 类建出来了、实例化成功了、字段清单也列出来了，
炸的只有「读注解」的那一下。第二个直觉是 `if TYPE_CHECKING:` 只是代码风格问题；
实际它省下的是整个模块的导入，代价全落在运行时读注解的人身上。

先看第一本账，dataclass 建类：

```
=== 1. dataclass：注解里的名字不存在，类照样建出来 ===
  class E 执行完，屏幕上一个字都没多：E.__qualname__ = 'E' ｜ hasattr(E, '__annotate__') → True（惰性注解的钩子挂上了，但没人调用它）
  E 的字段名 → ['a']
  fields(E)[0].type 是什么（类场景的 ForwardRef，repr 不带地址） → "ForwardRef('NotDefinedEver', is_class=True, owner=<class '__main__.E'>)"
  [成立] 字段名列表：实测 ['a'] ｜ 预期 ['a']
  [成立] E() 的报错原文：实测 "TypeError: E.__init__() missing 1 required positional argument: 'a'" ｜ 预期 "TypeError: E.__init__() missing 1 required positional argument: 'a'"
  [成立] E(1) 的 repr：实测 'E(a=1)' ｜ 预期 'E(a=1)'
  [成立] 读 E.__annotations__ 的报错原文：实测 "NameError: name 'NotDefinedEver' is not defined" ｜ 预期 "NameError: name 'NotDefinedEver' is not defined"
  [成立] typing.get_type_hints(E) 的报错原文：实测 "NameError: name 'NotDefinedEver' is not defined" ｜ 预期 "NameError: name 'NotDefinedEver' is not defined"
  → 断言：建类与实例化都不需要注解的值，只有「读」的那一下需要 ｜ 本格判据 5/5 条成立（不成立的那条上面会印 [不成立！]）
```

- 类体执行完，屏上一个字都没多：`__qualname__` 照常，`__annotate__` 这个钩子挂上了但没人调用它 —— 账从这里开始。
- 字段清单只有名字。`fields` 吃得饱 —— 它要的是注解的**形状**（叫什么、有没有默认值），不是注解的值。
- 那个坏名字的字段照样能实例化，`repr` 正常；少传参数时报的还是普通 `TypeError`，跟注解一点关系都没有。
- 只有属性直读 `__annotations__` 和 `typing.get_type_hints` 这两条炸出 `NameError` ——
  「建类不求值，读才求值」这句话的证据全在这几行里。

第二本账：类体里等号右边和冒号右边**根本不是同一类东西**。第 2 到第 4 格连着看：

```
=== 2. 有默认值的字段：默认值当场执行，注解仍然不求值 ===
  [这一行是默认值表达式：class G 一执行它就跑了，和注解活不活得起来没有半点关系]
  [成立] 默认值算出来的是 print 的返回值（None）：实测 True ｜ 预期 True
  [成立] G() 的 repr：实测 'G(a=None)' ｜ 预期 'G(a=None)'
  [成立] G(7) 的 repr：实测 'G(a=7)' ｜ 预期 'G(a=7)'
  [成立] 读 G.__annotations__ 的报错原文：实测 "NameError: name 'NotDefinedEver' is not defined" ｜ 预期 "NameError: name 'NotDefinedEver' is not defined"
  → 断言：class 体里「等号右边」当场执行，「冒号右边」延迟到有人读 ｜ 本格判据 4/4 条成立（不成立的那条上面会印 [不成立！]）

=== 3. typing.NamedTuple：建类、实例化全正常，只有读注解炸 ===
  class NT 也跑完了：NT.__qualname__ = 'NT' ｜ hasattr(NT, '__annotate__') → True
  [成立] _fields：实测 ('x', 'y') ｜ 预期 ('x', 'y')
  [成立] _field_defaults：实测 {'y': 2} ｜ 预期 {'y': 2}
  [成立] NT(1) 的 repr：实测 'NT(x=1, y=2)' ｜ 预期 'NT(x=1, y=2)'
  [成立] NT(1).y（默认值是真用上了）：实测 2 ｜ 预期 2
  [成立] 读 NT.__annotations__ 的报错原文：实测 "NameError: name 'MissingNT' is not defined" ｜ 预期 "NameError: name 'MissingNT' is not defined"
  [成立] typing.get_type_hints(NT) 的报错原文：实测 "NameError: name 'MissingNT' is not defined" ｜ 预期 "NameError: name 'MissingNT' is not defined"
  → 断言：NamedTuple 建类和实例化同样不求值注解；_fields 只靠名字 ｜ 本格判据 6/6 条成立（不成立的那条上面会印 [不成立！]）

=== 4. typing.TypedDict：__required_keys__ 有值，__annotations__ 炸 ===
  class TD 跑完：TD.__qualname__ = 'TD'（这一句之前没人读过它的注解）
  [成立] TD.__required_keys__ == frozenset({'a'})（写成比较，不靠 repr 顺序）：实测 True ｜ 预期 True
  [成立] TD.__optional_keys__ == frozenset()：实测 True ｜ 预期 True
  [成立] TD 其实就是个 dict 工厂：TD(a=1)：实测 "{'a': 1}" ｜ 预期 "{'a': 1}"
  [成立] 读 TD.__annotations__ 的报错原文：实测 "NameError: name 'MissingTD' is not defined" ｜ 预期 "NameError: name 'MissingTD' is not defined"
  [成立] typing.get_type_hints(TD) 的报错原文：实测 "NameError: name 'MissingTD' is not defined" ｜ 预期 "NameError: name 'MissingTD' is not defined"
  → 断言：TypedDict 的键账本照样建起来了，炸的只有「读注解」 ｜ 本格判据 5/5 条成立（不成立的那条上面会印 [不成立！]）
```

- **第 2 格** 那行默认值表达式在 `class G` 一执行就**真跑了一遍**（屏上直接留下它打印的那句话），
  而它的注解要到有人读时才炸。记法：等号右边立刻执行，冒号右边是一段还没跑的代码。
- **第 3 格** `NamedTuple` 的字段名表和默认值表都建起来了，默认值是真用上的（少传一个参数也照样拿到），
  炸的仍然只有读注解那两条。
- **第 4 格** `TypedDict` 的必选/可选键账本照样算得出来，它本身就是个 dict 工厂；读注解照样炸。
  三格合起来是一句话：**标准库那几个「以注解为原料」的构造器，用的都是注解的形状，不是注解的值。**

第 5 格是第三本账，`if TYPE_CHECKING:` 省下的东西用子进程量得最准：

```
=== 5. if TYPE_CHECKING: import argparse —— 省下的是整次 import ===
  │ TYPE_CHECKING 的值 = False
  │ 1 定义 f 之后 f.__qualname__ -> 'f'
  │ 2 STRING 读法 -> {'x': 'argparse.Namespace', 'return': 'None'}
  │ 3 FORWARDREF 读法（只打值的类型名） -> {'x': 'ForwardRef', 'return': 'NoneType'}
  │ 4 VALUE 读法 -> ERR NameError: name 'argparse' is not defined
  │ 5 typing.get_type_hints(f) -> ERR NameError: name 'argparse' is not defined
  │ 6 'argparse' in sys.modules -> False
  │ 7 sys.modules 里含 argparse 字样的键 -> (空)
  └ 子进程退出码 = 0 ｜ stderr 行数 = 0
  [成立] 子进程第 2 行（STRING 读法拿到全文）：实测 True ｜ 预期 True
  [成立] 子进程第 3 行（FORWARDREF：值退化成 ForwardRef）：实测 True ｜ 预期 True
  [成立] 子进程第 4 行（VALUE 读法：NameError）：实测 True ｜ 预期 True
  [成立] 子进程第 6 行（'argparse' in sys.modules 是 False）：实测 True ｜ 预期 True
  [成立] 子进程退出码：实测 0 ｜ 预期 0
  ★ 注解里那个名字「运行时不存在」和「运行时没 import 那个模块」是同一件事的两面：逐字 = "6 'argparse' in sys.modules -> False"
  → 断言：TYPE_CHECKING 是运行时省下的那次 import，代价是 VALUE 读法当场 NameError ｜ 本格判据 5/5 条成立（不成立的那条上面会印 [不成立！]）
```

- **①** `TYPE_CHECKING` 运行时就是 `False`，所以那条 import 根本不执行。
- **⑥⑦** 那个模块压根没进过 `sys.modules`，连含它字样的键都是空的 —— 省下的不是几行代码，是一整次导入。
  这正是它存在的理由：循环依赖和启动开销都靠它躲掉。
- **②③④⑤** 代价同时摆着：STRING 读法能拿到全文，FORWARDREF 退化成一个牌，1 号和 `get_type_hints` 两条直接 `NameError`。
  也就是说，注解里的名字运行时取不到，和那个模块运行时没导进来，是同一枚硬币。
- 最后那行收尾更值得看：子进程退出码 0、stderr 行数 0 —— 这一段安静得可怕，
  没读注解的代码，永远不知道自己欠了一颗雷。

第 6 格把 b 篇第 6 格的结论搬到真实调用点上：`inspect.signature` 是唯一一个**自己踩雷**的标准库调用。

```
=== 6. inspect.signature：唯一一个自己踩雷的标准库调用 ===
  函数 q 定义成功：q.__qualname__ = 'q' ｜ hasattr(q, '__signature__') → False
  [成立] str(inspect.signature(q)) 的报错原文：实测 "NameError: name 'Missing2' is not defined" ｜ 预期 "NameError: name 'Missing2' is not defined"
  [成立] 换参数也逃不掉：signature(q, eval_str=False)：实测 "NameError: name 'Missing2' is not defined" ｜ 预期 "NameError: name 'Missing2' is not defined"
  [成立] STRING 读法：同一个函数，逐字全文：实测 {'a': 'int', 'b': 'Missing2', 'c': 'Missing3', 'return': 'Missing4'} ｜ 预期 {'a': 'int', 'b': 'Missing2', 'c': 'Missing3', 'return': 'Missing4'}
  [成立] FORWARDREF 读法里每个值的类型名：实测 {'a': 'type', 'b': 'ForwardRef', 'c': 'str', 'return': 'ForwardRef'} ｜ 预期 {'a': 'type', 'b': 'ForwardRef', 'c': 'str', 'return': 'ForwardRef'}
  [成立] hasattr(q, '__signature__')：实测 False ｜ 预期 False
  STRING 读过一次之后 signature 是否就好了（两种读法不共享缓存） → 炸：NameError: name 'Missing2' is not defined
  [成立] 把 STRING 读法的结果手动写回 q.__annotations__，再 signature：实测 "(a: 'int', b: 'Missing2' = None, *, c: 'Missing3' = 1) -> 'Missing4'" ｜ 预期 "(a: 'int', b: 'Missing2' = None, *, c: 'Missing3' = 1) -> 'Missing4'"
  → 断言：同一个函数：signature 炸、STRING 读法不炸；带引号的注解在 FORWARDREF 里仍是 str ｜ 本格判据 6/6 条成立（不成立的那条上面会印 [不成立！]）
```

- 函数定义成功，而 `__signature__` 这个属性它没有（False），于是 `signature` 只能现读注解 —— 一读就炸。
- 换个参数逃不掉：`eval_str=False` 报的还是同一条 `NameError`。
- 同一个函数，STRING 读法能拿到全文；FORWARDREF 里四个值的类型名混在一起：真类、牌、`str`、牌。
  **源码里带引号那条在 FORWARDREF 里本来就是 `str`**，和 b 篇第 5 格 ⑥ 对得上。
- 用 STRING 读过一次之后，`signature` 仍然炸：各读法各走各的，
  别指望「先温和地读一遍，就能把暴烈的读法治好」。
- 真正把它救活的是手动写回 —— 也就是第 05 节矩阵里 ⑤ 那条绕法的原件；
  救活之后 `signature` 给的是带引号的文本，默认值形状也跟着原样出来了。

第 7 格是全节最阴的一格，也是我认为整章最该抄下来贴在显示器边上的一格：

```
=== 7. dataclass 认 ClassVar 靠什么：靠能算出来的那个值 ===
  [成立] fields(CV)：只剩 a（b、c 都被认成 ClassVar）：实测 ['a'] ｜ 预期 ['a']
  [成立] CV.b / CV.c 变成类属性：实测 ('写法A：从 typing 导入', '写法B：带前缀') ｜ 预期 ('写法A：从 typing 导入', '写法B：带前缀')
  [成立] CV(1) 的 repr（__init__ 只收一个参数）：实测 'CV(a=1)' ｜ 预期 'CV(a=1)'
  [成立] 陷阱版 fields(Trap)：b 被当成普通字段：实测 ['a', 'b'] ｜ 预期 ['a', 'b']
  [成立] Trap(1, '共享') 照样实例化（__init__ 真收了两个参数）：实测 "Trap(a=1, b='共享')" ｜ 预期 "Trap(a=1, b='共享')"
  [成立] Trap 的 FORWARDREF 读法里每个值的类型名：实测 {'a': 'type', 'b': 'ForwardRef'} ｜ 预期 {'a': 'type', 'b': 'ForwardRef'}
  [成立] 读 CV.__annotations__ 的报错原文：实测 "NameError: name 'NotDefinedEver' is not defined" ｜ 预期 "NameError: name 'NotDefinedEver' is not defined"
  → 断言：ClassVar 的识别 = 「这个注解能不能算出值」，算不出就当普通字段，静默 ｜ 本格判据 7/7 条成立（不成立的那条上面会印 [不成立！]）
```

- 两种「真能算出值」的 ClassVar 写法（从 `typing` 导入的、带前缀写全的）都被认出来：
  它们变成类属性，字段清单只剩一个，`__init__` 只收一个参数。
- 陷阱版里那个名字从来没被定义过，于是它逃过了识别：字段清单多出第二个，实例化时 `__init__` 老老实实收两个参数，
  屏幕上一句提示都没有。
- 唯一的线索藏在读法里：FORWARDREF 口径下它的类型名是 `ForwardRef`，跟真正写坏的字段长得一模一样，
  看不出「本来想写 ClassVar」；而类自己的 `__annotations__` 仍然炸 —— dataclass 自己读得到，你读不到。
- 这类降级的含义是：不报错、不警告，只把程序语义换掉 —— 类变量变成实例字段，每个实例一份。

第 8 格把一个常见笔误的报错原文钉在屏上（原文一个字没改，路径折成 `<pyroot>`）：

```
=== 8. InitVar 不在 typing 里 —— 报错原文一条字都别改 ===
  from typing import InitVar → ImportError: cannot import name 'InitVar' from 'typing' (<pyroot>/lib/python3.14/typing.py)
  [成立] 上面这行与预期逐字相同（路径已折成 <pyroot>）：实测 "ImportError: cannot import name 'InitVar' from 'typing' (<pyroot>/lib/python3.14/typing.py)" ｜ 预期 "ImportError: cannot import name 'InitVar' from 'typing' (<pyroot>/lib/python3.14/typing.py)"
  [成立] typing 里有 InitVar 这个名字吗：实测 False ｜ 预期 False
  [成立] dataclasses.InitVar[int] 是什么东西：实测 'dataclasses.InitVar[int]' ｜ 预期 'dataclasses.InitVar[int]'
  [成立] fields(CI)（InitVar 也不算 field）：实测 ['a'] ｜ 预期 ['a']
  [成立] CI(1, 2).real（InitVar 的值交给 __post_init__，实例上不留）：实测 2 ｜ 预期 2
  [成立] CI(1, 2) 的 repr：实测 'CI(a=1)' ｜ 预期 'CI(a=1)'
  [成立] 读 CI.__annotations__ 的报错原文：实测 "NameError: name 'NotDefinedEver' is not defined" ｜ 预期 "NameError: name 'NotDefinedEver' is not defined"
  → 断言：InitVar 的正确出处是 dataclasses；typing 里没这个名字（报错原文已逐字核对） ｜ 本格判据 7/7 条成立（不成立的那条上面会印 [不成立！]）
```

- `InitVar` 不在 `typing` 里 —— 撞出来的是 `ImportError`，直说没有这个名字，而 `typing` 里确实没它。
  正确出处是 `dataclasses`，紧接着那行给出它带上下标之后的样子。
- 它也不算 field：字段清单只剩一个，值交给 `__post_init__`，实例上不留（`repr` 里看不到它）。
- 这一格的注解里照样埋着坏名，所以读 `__annotations__` 仍然炸 —— 和前几格同一个结论。

**这一节记住一句话**：dataclass / NamedTuple / TypedDict 吃的是注解的形状，`inspect.signature` 和 `get_type_hints` 吃的是注解的值 —— 前者建类、实例化全安静，后者一读就把坏名炸给你看，而 ClassVar 没被认出来的时候连炸都不炸。

---

## 第 07 节 · 矩阵格：一个坏注解字段的 dataclass，七个调用点谁响（跨 c 篇的合并视图）

c 篇是按「谁在读」逐个演的，每格讲各的读法。这一格反过来搭：建好一个类，字段里就写着一个源码里没有的名字，
然后把七个**真实调用点**一个个撞上去。直觉是「坏注解嘛，一开始就该响」；
屏幕上是一条干净的分界线 —— 前面三个调用点一声不响，后面四个报的是**同一条** `NameError`。

```
c 篇按「谁读」逐个演。这一格反过来：建好一个类（字段里就写着源码里没有的名字），
  然后把七个真实调用点逐个撞上去 —— 前三个是本章大部分程序一辈子走的路，后四个才是会炸的地方。

$ python3 matrix_class.py
```

```
  ① 建类（decorator 返回时） → 不响：Row
  ② 实例化 Row(name=..., count=...) → 不响：Row(name='a', count=1)
  ③ dataclasses.fields 清单 → 不响：['name', 'count']
  ④ inspect.signature(Row) → 响：NameError: name 'MissingHere' is not defined
  ⑤ typing.get_type_hints(Row) → 响：NameError: name 'MissingHere' is not defined
  ⑥ 属性读 Row.__annotations__ → 响：NameError: name 'MissingHere' is not defined
  ⑦ 读生成 __init__ 的 VALUE 注解 → 响：NameError: name 'MissingHere' is not defined
[退出码 0]
```

- **①②③** 装饰器返回时、实例化时、`fields` 列清单时都不响，而且各给得出正常读数（类名、实例的 `repr`、两个字段名）。
  这三行就是本章大部分程序一辈子走的路 —— 你的代码可以天天带着坏注解跑。
- **④⑤⑥⑦** 四个响的点：`inspect.signature`、`typing.get_type_hints`、属性直读 `__annotations__`，
  以及去读那支**生成出来的 `__init__`** 的 1 号注解。四条报的是同一个错、同一个名字，只是入口不同。
  这条分界线右边站着的，全是**替你读注解的那些库** —— 本章后面几篇会一个个撞上它们。
- **和单篇对得上的格子**：①②③ 是 c 篇第 1 格那几条判据的合并版（同一结论）；④ 对应 c 篇第 6 格 ——
  两边坏名不同，所以消息里的名字不同，但「signature 自己踩雷」是同一条路；
  ⑤⑥ 对应 c 篇第 1 到第 4 格里每个类都被读出来的那一对 `NameError`。
- **⑦ 最值钱**：它把「类上的注解炸」和「`__init__` 上的注解炸」连起来了 ——
  dataclass 生成 `__init__` 时也要去读 1 号口径，于是坏名会从类这一层转嫁到函数那一层。
  第 05 节矩阵 ⑤ 那条写回的路子，量的正是这条线的另一头。

```
↑ 三响四不响，这条分界线就是本章给读者的实用结论：**你的程序能跑**，
  直到某个库去读注解 —— 序列化库、CLI 框架、依赖注入、测试框架的 fixture，全都站在这条线的右边。
  所以「我加了注解结果程序崩了」这种报告，第一个要问的是：谁在读它，读的是哪个 format。
```

**这一节记住一句话**：坏注解不是崩溃的原因，是一次崩溃的**预约** —— 只有当有人去读它时才兑现，所以查案先查读它的人和它用的口径。

---
## 第 08 节 · d 篇完整跑：运行时一行都不检查（八个「写了也不拦」的现场）

道具是 `d_runtime_checks_nothing.py`：前七格全是「写了也不拦」的现场，第八格是负对照 —— 自己读注解、自己比较，
它才真的响。先摆读者的直觉：`x: int` 是一道门，塞字符串就该被拦；标过 `Final` 的名字不许再赋值；
`@final` 的类不许被继承；`cast` 会把值真变成另一个类型；`assert_type` 名字里带 `assert`，对不上就该炸。
这五条直觉在屏幕上撞了一圈，一条都没拦住 —— 这就是主线② 的本体。

实际发生的事只有一句：**注解只往账本 `__annotations__` 里记一笔「期望」，值的类型一个字节都没被改**。
第 02 节演的是「这笔期望要到被读的那一刻才被求值」，这一节演的是后半句：**就算求出了值，也没有任何人替你拿它
去比较**。两半合起来才叫「运行时一行都不检查」。

最反直觉的是第 5 格：一次明摆着对不上的 `assert_type`，把原值还给你、异常一个不抛，它自己的 docstring 第一句
就把答案写在脸上。而全节唯一真的会拦的是 `isinstance` 家族 —— 它只吃裸类和 `X | Y`，带方括号的一律不给吃，
而且给的是炸，不是 `False`。

**变量注解与函数注解：一笔期望，互不相干**

```
=== 1. 变量注解：值归值，注解归注解，从此互不相干 ===
  [成立] 读之前：模块命名空间里有 '__annotations__' 这个键吗：实测 False ｜ 预期 False
  直接写 __annotations__（没人读过它，所以这个名字还没有） → 炸：NameError: name '__annotations__' is not defined
  [成立] annotationlib 读模块级注解（VALUE 读法：账本这才被造出来）：实测 {'x': <class 'int'>, 'b': <class '__main__.Box'>} ｜ 预期 {'x': <class 'int'>, 'b': <class '__main__.Box'>}
  [成立] 读完之后：'__annotations__' in vars(模块)：实测 True ｜ 预期 True
  [成立] 账本给 x 记的是：实测 <class 'int'> ｜ 预期 <class 'int'>
  [成立] x 的真实值（注解一个字没影响到它）：实测 '字符串' ｜ 预期 '字符串'
  [成立] type(x).__name__：实测 'str' ｜ 预期 'str'
  [成立] 账本给 b 记的是：实测 <class '__main__.Box'> ｜ 预期 <class '__main__.Box'>
  [成立] type(b).__name__：实测 'int' ｜ 预期 'int'
  → 断言：注解只留下一笔「期望」；变量的值与类型原封不动（x 仍是 str、b 仍是 int） ｜ 本格判据 8/8 条成立（不成立的那条上面会印 [不成立！]）
```

- **①** 声明成 `int` 的那个变量实际是字符串：屏幕上「账本给它记的是 `int`」和「它的真实类型名是 `str`」
  两行**同时成立**，谁也不提醒谁；另一个对象正好反过来（注解是个自定义类，值实际是 `int`）。
- **②** 模块级那本账是**被读出来那一刻才存在**的：读之前直接写那个键会 `NameError`，
  按 VALUE 读法读一遍之后，`vars(模块)` 里才出现它。别把账本当「定义时就写好」的东西（机制见第 02 节）。
- **③** 用法：想知道运行时到底是什么类型，只能问对象自己（`type(x)`）；注解那一笔只服务读代码的人。
  什么时候会失效：你以为「IDE 会拦、跑起来也会拦」—— 拦的那半边靠工具，本机一件都没有（见第 01 节）。

```
=== 2. 函数注解：传错类型照样调用，返回值是「错类型」的运算结果 ===
  [成立] add(1, 2)：实测 3 ｜ 预期 3
  [成立] add("a", "b")（注解说是 int）：实测 'ab' ｜ 预期 'ab'
  [成立] add([1], [2])：实测 '[1, 2]' ｜ 预期 '[1, 2]'
  [成立] 返回值的真实类型名：实测 'str' ｜ 预期 'str'
  [成立] 函数自己的注解（int 存在，所以一直好读）：实测 {'a': <class 'int'>, 'b': <class 'int'>, 'return': <class 'int'>} ｜ 预期 {'a': <class 'int'>, 'b': <class 'int'>, 'return': <class 'int'>}
  → 断言：add("a","b") 得到 'ab'：那个 + 是 str 的 +，注解一点忙都没帮 ｜ 本格判据 5/5 条成立（不成立的那条上面会印 [不成立！]）
```

- **④** 三组实参全部跑通：注解声明收 `int`，你喂字符串它就走字符串的加号，喂列表它就走列表的加号。
  最后一行报的是返回值的真实类型名，它跟注解声明的那个不是一回事。
- **⑤** 函数自己的注解反倒一直好读（因为 `int` 是个真存在的名字）——
  「注解读得到」和「注解被拿来检查」是两件不同的事，这一格把它们并排放在同一屏上。

**`Final`、`@final`：纯记号**

```
=== 3. Final 不拦重新赋值，@final 不拦继承 ===
  [成立] 重新赋值之前：实测 1 ｜ 预期 1
  [成立] 重新赋值之后（没报错、没警告、屏幕干净）：实测 2 ｜ 预期 2
  [成立] 账本上 CONST 那一笔（STRING 读法：从源码现算，永远新）：实测 'typing.Final[int]' ｜ 预期 'typing.Final[int]'
  [成立] ★ 顺手量到的 3.14 细节：第 1 格按 VALUE 读过之后，之后才执行的模块级注解不会补进那本已缓存的账（所以本条是 False）：实测 False ｜ 预期 False
  [成立] @final 的类被继承了：Sub.__name__：实测 'Sub' ｜ 预期 'Sub'
  [成立] issubclass(Sub, Locked)：实测 True ｜ 预期 True
  [成立] typing.final 的 docstring 首句（它自己怎么说的）：实测 'Decorator to indicate final methods and final classes.' ｜ 预期 'Decorator to indicate final methods and final classes.'
  → 断言：Final/@final 在运行时是纯记号：值改得动（1→2）、类继承得了（Sub） ｜ 本格判据 7/7 条成立（不成立的那条上面会印 [不成立！]）
```

- **⑥** 同一个名字改第二次赋值：没报错、没警告、屏幕干净；账本上那一笔仍是字符串形态的 `Final` 声明，
  而这一笔是 STRING 读法「从源码现算、永远新」量出来的。
- **⑦** 被 `@final` 标记的类照样能被继承，`issubclass` 还认这层父子关系。它自己的 docstring 用的是
  「标示」这个词，不是「阻止」—— 词用得比读者的直觉准。
- **⑧** 顺手量到一条 3.14 的缓存细节：模块级账本按 VALUE 读过一次之后，**之后才执行的模块级注解不会补进那本
  已缓存的账**。想拿全量就别抱着那份 dict 不放（这条接第 02 节「成功才缓存、失败不缓存」）。
- **⑨** 同族的 `cast` 和 `Literal` 也不拦（第 4 格）：`cast` 原样把第二个参数还给你，第一个参数甚至不用是类型；
  `Literal` 没列出的越界值照传。要真拦，运行时只有两条路：自己 `assert`，或者用第 10 节那个库。

**名字叫 assert 的那一格（全节最扎人）**

```
=== 5. assert_type：名字叫 assert，运行时是空气（全章最扎人的一格）===
  [成立] typing.assert_type(1, int) 的返回值（对得上的那种）：实测 1 ｜ 预期 1
  [成立] typing.assert_type(1, str) 的返回值（明摆着对不上）：实测 1 ｜ 预期 1
  [成立] assert_type(1, str) 抛异常了吗：实测 '没抛' ｜ 预期 '没抛'
  [成立] 它自己的 docstring 首句（答案写在脸上）：实测 'Ask a static type checker to confirm that the value is of the given type.' ｜ 预期 'Ask a static type checker to confirm that the value is of the given type.'
  [成立] typing 里有同族的 assert_never 吗：实测 True ｜ 预期 True
  [成立] assert_never 的 docstring 首句：实测 'Statically assert that a line of code is unreachable.' ｜ 预期 'Statically assert that a line of code is unreachable.'
  → 断言：两个 assert_type 都返回 1、都不报错：assert 这个词只服务于静态检查器 ｜ 本格判据 6/6 条成立（不成立的那条上面会印 [不成立！]）
```

- **⑩** 两次调用返回的都是原值：对得上返回它，明摆着对不上也返回它，异常一个都没抛。
- **⑪** 两个 docstring 的首句都自陈「这是说给静态检查器听的」，逐字看上面围栏。所以规矩是：
  `assert_type` / `assert_never` 永远不是运行时守卫，把它们当断言写进代码 = 写了一行空气。
- **⑫** 什么时候失效：这一条不存在「生效」的时候 —— 哪怕装了静态检查器，响的也是检查器那一步，
  不是这一行本身；把程序的正确性押在它上面，等于押在一个本机不存在的工具上（见第 01 节）。

**唯一真干活的一族：`isinstance` / `issubclass` 挑食**

```
=== 7. isinstance / issubclass 的四问：只有裸类和 X | Y 能用 ===
  [成立] isinstance(3, int | str)：实测 True ｜ 预期 True
  [成立] isinstance([1], list[int]) 的报错原文：实测 'TypeError: isinstance() argument 2 cannot be a parameterized generic' ｜ 预期 'TypeError: isinstance() argument 2 cannot be a parameterized generic'
  [成立] isinstance([1], list)：实测 True ｜ 预期 True
  [成立] isinstance(3, typing.Optional[int])：实测 True ｜ 预期 True
  [成立] issubclass(list, list[int]) 的报错原文：实测 'TypeError: issubclass() argument 2 cannot be a parameterized generic' ｜ 预期 'TypeError: issubclass() argument 2 cannot be a parameterized generic'
  [成立] 为什么 Optional[int] 能用：它就是 int | None：实测 'int | None' ｜ 预期 'int | None'
  [成立] list[int] 有 __args__、list 没有（参数化与否的判据）：实测 (True, False) ｜ 预期 (True, False)
  → 断言：isinstance 认裸类与 X | Y；带方括号的一律 TypeError（注意：是炸，不是返回 False） ｜ 本格判据 7/7 条成立（不成立的那条上面会印 [不成立！]）
```

- **⑬** 能吃的：裸类、`X | Y`。`Optional[int]` 能吃是因为**它本来就是个 union**（屏幕那行直接给出了它的真身）。
- **⑭** 不能吃的：任何带方括号的参数化泛型 —— `isinstance` 和 `issubclass` 各给一句 `TypeError`。
  用法上最要紧的是那句判定：**是炸，不是返回 `False`**。拿 `try` 兜住再退回到裸类，是这个写法唯一安全的形状。
- **⑮** 参数化与否的运行时判据是那个存参数的属性有没有（`list[int]` 有、`list` 没有）—— 这正是第 09 节
  `get_args` 那条路要用的东西。

**负对照：真的会拦的检查长什么样**

```
=== 8. 负对照：真的会拦的检查长什么样（先让它响，再报真实值）===
  故意喂 4 组错误实参：strict_add 响 4 次、算出结果 0 次，第一条原文 = 'TypeError: a 要 int，实际拿到 str'   ← 判据确实会响
  同样 4 组喂给裸注解的 add：响 2 次、算出结果 2 次，第一条原文 = "TypeError: unsupported operand type(s) for +: 'NoneType' and 'int'"
    它响的那几次，报错全部来自 + 号自己（unsupported operand），和注解没关系；剩下 2 组直接返回了错类型的值
  [成立] strict 版响的次数：实测 4 ｜ 预期 4
  [成立] strict 版报错原文里含「要 int」（我们自己写的那句）的条数：实测 4 ｜ 预期 4
  [成立] 裸注解版真的算出结果的组数（错类型也照算）：实测 2 ｜ 预期 2
  [成立] 裸注解版报错原文里含「要 int」的条数（它根本不知道有注解）：实测 0 ｜ 预期 0
  [成立] strict_add(1, 2) 正常路径：实测 3 ｜ 预期 3
  [成立] strict_add 读的正是它自己的 __annotations__：实测 {'a': <class 'int'>, 'b': <class 'int'>, 'return': <class 'int'>} ｜ 预期 {'a': <class 'int'>, 'b': <class 'int'>, 'return': <class 'int'>}
  → 断言：同一份注解：读它并 isinstance 就响 4 次；不读它，4 组错误实参里 2 组安静返回错类型的值、因注解而响 0 次 ｜ 本格判据 6/6 条成立（不成立的那条上面会印 [不成立！]）
```

- **⑯** 同一份注解、同一个函数，只是函数体里自己读 `__annotations__` 再 `isinstance`：
  四组错误实参响满四组、一次结果都没算出来。先让它响，这一句才是证据。
- **⑰** 裸注解那版同样四组只响两组，而且那两组的报错来自加号运算符自己翻脸，和注解无关；剩下两组直接返回
  错类型的值。「包含我们想查的那句话的报错条数」实测是 0 —— 这是「运行时不查」最硬的一条读数。
- **⑱** 结论落在用法上：**要拦就必须有人读注解 + 读的人自己比较**。第 8 格那几行就是全部工作量；
  第 10 节的 typeguard 干的是同一件事，只是把这几行自动插到每次调用前面。

**这一节记住一句话**：注解是写下来的期望，不是执行中的检查 —— 想让期望生效，只有「有人读它、读它的人自己去比」这一条路。

---

## 第 09 节 · e 篇完整跑：Protocol 与泛型 —— isinstance 家族能吃什么

道具是 `e_protocols_generics.py`。读者对 `Protocol` 的直觉是从别的语言带来的：这是接口，
加了 `@runtime_checkable` 就能在运行时验「这个对象实现了没」；而对泛型的直觉是 `list[int]` 让运行时
知道了元素类型。屏幕上：第一条被还原成「属性存在检查」，第二条根本不算判据（第 08 节第 7 格已经演过它不给吃）。

`@runtime_checkable` 的 `isinstance` 与 `hasattr` **逐位相同**，签名一个都不查 —— 这是本节的主结论，
也是主线② 在协议上的形状。最反直觉的一格是第 8 格那个手搭的两层校验器：`Any` 那处不是「放过」，
是当场 `TypeError`，根因是 `Any` 长得像类、骗过了裸类分支。

**协议只查名字**

```
=== 1. @runtime_checkable 的 Protocol：名字对上就算，签名一个都不查 ===
  协议 Speaks 要求的成员名 = ['speak'] ｜ 它的 _is_runtime_protocol 标记 = True
  [成立] isinstance(Right(), Speaks)（正经实现）：实测 True ｜ 预期 True
  [成立] isinstance(Wrong(), Speaks)（speak 少一个参数）：实测 True ｜ 预期 True
  [成立] isinstance(Nothing(), Speaks)（啥都没有）：实测 False ｜ 预期 False
  [成立] isinstance 的判据 == hasattr 逐个对象比（三个对象的全对列表）：实测 [True, True, False] ｜ 预期 [True, True, False]
  [成立] 连「speak 是个 property 不是方法」也算实现：实测 True ｜ 预期 True
  [成立] issubclass(Wrong, Speaks)（类层面也只查名字）：实测 True ｜ 预期 True
  → 断言：名字对上就算，签名一个都不查：isinstance 与 hasattr 在本机逐位相同 ｜ 本格判据 6/6 条成立（不成立的那条上面会印 [不成立！]）
```

- **①** 协议要求的成员名就一个：正经实现 → `True`；把那个方法**少写一个参数** → 还是 `True`；
  什么都没有 → `False`。本格最硬的一行是把 `isinstance` 的判据和 `hasattr` 逐个对象比，读数完全一致。
- **②** 连「那个名字是个 `property` 不是方法」都算实现，类层面 `issubclass` 也只查名字。
- **③** 用法：`runtime_checkable` 适合「看对象像不像」再做分发，绝不适合当契约。
  什么时候会失效：你把 `True` 读成「它能按协议的签名跑起来」—— 下一格专门演这个下场。

```
=== 2. 代价（负对照）：isinstance 说 True，真按协议调用当场炸 ===
  [成立] 冒牌货先过检查这一关：isinstance(Wrong(), Speaks)：实测 True ｜ 预期 True
  [成立] 按协议签名调用它：Wrong().speak(loud=True)：实测 "TypeError: Wrong.speak() got an unexpected keyword argument 'loud'" ｜ 预期 "TypeError: Wrong.speak() got an unexpected keyword argument 'loud'"
  [成立] 少一个参数地调用（它自己能跑）：Wrong().speak()：实测 'no' ｜ 预期 'no'
  [成立] 正牌调用对照：Right().speak(loud=True)：实测 '汪' ｜ 预期 '汪'
  → 断言：判据响了 1 次（上面那条 TypeError 原文），而它的 isinstance 是 True：runtime_checkable 保证不了任何一次调用 ｜ 本格判据 4/4 条成立（不成立的那条上面会印 [不成立！]）
```

- **④** 冒牌货先过了检查这一关，再按协议签名调用它 → `TypeError`，那句原文（少了关键字参数的下场）逐字在上面围栏。
  少一个参数地调用它自己倒是能跑；正牌实现按协议调用也正常。
- **⑤** 判据响了一次，而它的 `isinstance` 是 `True`：`runtime_checkable` 保证不了任何一次调用。
  签名层面的检查只有静态工具做得到（本机没有，见第 01 节），运行时最多做到「形状对不对」。

**数据成员与没打标记的两种下场**

```
=== 4. 没加 @runtime_checkable 的 Protocol：连 isinstance 都不许用 ===
  │ isinstance -> ERR TypeError: Instance and class checks can only be used with @runtime_checkable protocols
  │ 标记之前 _is_runtime_protocol = False
  │ runtime_checkable 返回的是同一个类吗 = True
  │ 标记之后 _is_runtime_protocol = True
  │ 现在再 isinstance = True
  └ 子进程退出码 = 0 ｜ stderr 行数 = 0
  [成立] 子进程里未加标记时 isinstance 的报错原文（整行核对）：实测 True ｜ 预期 True
    逐字 = 'isinstance -> ERR TypeError: Instance and class checks can only be used with @runtime_checkable protocols'
  [成立] 子进程里 runtime_checkable 返回的是同一个类：实测 True ｜ 预期 True
  [成立] 子进程里标记之前 _is_runtime_protocol 是 False：实测 True ｜ 预期 True
  [成立] 子进程里标记之后 isinstance 变 True（原地打标记，不换类）：实测 True ｜ 预期 True
  [成立] 本进程里同样的事：runtime_checkable(Speaks) is Speaks：实测 True ｜ 预期 True
  → 断言：没标记 = TypeError（原文已整行核对）；runtime_checkable 原地改标记、返回值 is 原类（True） ｜ 本格判据 5/5 条成立（不成立的那条上面会印 [不成立！]）
```

- **⑥** 没打标记时 `isinstance` 连用都不许用：`TypeError` 那句原文被整行核对过（围栏里有）。
  带数据成员的协议更干脆：`isinstance` 还是 `hasattr`，`issubclass` 一律 `TypeError` 并点名那个数据成员（第 3 格）。
- **⑦** `runtime_checkable` 返回的还是**同一个类**（`is` 为 `True`），它只是把类上那个「可运行时检查」的标记
  从假改成真。所以「谁加了标记」是**全局事实**：某个库替你对一个协议加了标记，别人对同一个协议的判断就跟着变。

**唯一真把注解拆开看的那批函数**

```
=== 5. get_origin / get_args：唯一真把注解拆开看的那批 ===
  list[int]              → origin=<class 'list'>  args=(<class 'int'>,)
  dict[str,int]          → origin=<class 'dict'>  args=(<class 'str'>, <class 'int'>)
  Optional[int]          → origin=<class 'typing.Union'>  args=(<class 'int'>, <class 'NoneType'>)
  int | str              → origin=<class 'typing.Union'>  args=(<class 'int'>, <class 'str'>)
  Literal['a','b']       → origin=typing.Literal  args=('a', 'b')
  Callable[[int],str]    → origin=<class 'collections.abc.Callable'>  args=([<class 'int'>], <class 'str'>)
  Tuple[int, ...]        → origin=<class 'tuple'>  args=(<class 'int'>, Ellipsis)
  Any                    → origin=None  args=()
  Speaks（协议类本身）          → origin=None  args=()
  [成立] list[int] 的 origin 就是 list 本身：实测 True ｜ 预期 True
  [成立] list[int] 的 args：实测 (<class 'int'>,) ｜ 预期 (<class 'int'>,)
  [成立] dict[str,int] 的 args：实测 (<class 'str'>, <class 'int'>) ｜ 预期 (<class 'str'>, <class 'int'>)
  [成立] Optional[int] 与 int|str 的 origin 是同一个东西（typing.Union）：实测 (<class 'typing.Union'>, <class 'typing.Union'>) ｜ 预期 (<class 'typing.Union'>, <class 'typing.Union'>)
  [成立] Optional[int] 的 args 里那位 NoneType：实测 (<class 'int'>, <class 'NoneType'>) ｜ 预期 (<class 'int'>, <class 'NoneType'>)
  [成立] Literal 的 origin 与 args（args 是字面量本身，不是类型）：实测 (typing.Literal, ('a', 'b')) ｜ 预期 (typing.Literal, ('a', 'b'))
  [成立] Callable 的 origin 走 collections.abc，args 第一位是「参数列表」：实测 (<class 'collections.abc.Callable'>, ([<class 'int'>], <class 'str'>)) ｜ 预期 (<class 'collections.abc.Callable'>, ([<class 'int'>], <class 'str'>))
  [成立] Tuple[int, ...] 的 args 里带 Ellipsis（可变参数的记号）：实测 (<class 'int'>, Ellipsis) ｜ 预期 (<class 'int'>, Ellipsis)
  [成立] Any 拆开是 (None, ())：它没有 origin，所以第 8 格会把它当裸类送给 isinstance（然后就炸了）：实测 (None, ()) ｜ 预期 (None, ())
  [成立] 协议类本身也拆不出东西（origin=None）：实测 (None, ()) ｜ 预期 (None, ())
  → 断言：表上 9 行逐字可核；origin 给出「容器真身」，args 给出「参数元组」，Any 与协议类都是 (None, ()) ｜ 本格判据 10/10 条成立（不成立的那条上面会印 [不成立！]）
```

- **⑧** 这是本章唯一「真的读懂了注解」的标准库函数组：`get_origin` 给容器真身，`get_args` 给参数元组，
  表上九行逐字可核。
- **⑨** 三个容易记错的读数：`Optional[int]` 和 `int | str` 的 origin 是**同一个东西**；`Callable` 的 origin
  走 `collections.abc`；`Tuple[int, ...]` 的 args 里带 `Ellipsis`。`Literal` 的 args 是字面量本身，不是类型。
- **⑩** 最要紧的是最后两行：`Any` 和协议类本身拆开都是 `(None, ())` —— 它们**没有 origin**，
  所以写校验器时会被当成裸类送进 `isinstance`，然后就炸。这就是第 8 格那处的根因。

**比注解一律用 `==`，别用 `is`**

```
=== 6. list[int] 每次都是新对象，但相等：判据要写 ==，别写 is ===
  [成立] list[int] is list[int]：实测 False ｜ 预期 False
  [成立] list[int] == list[int]：实测 True ｜ 预期 True
  [成立] list[int].__args__ == (int,)：实测 (<class 'int'>,) ｜ 预期 (<class 'int'>,)
  [成立] dict[str,int] == dict[str,int]：实测 True ｜ 预期 True
  [成立] list[int] == list[str]：实测 False ｜ 预期 False
  [成立] Optional[int] == int | None（两条写法是同一个东西）：实测 True ｜ 预期 True
  [成立] 缓存一次再比：同一个对象比较自己 is → True：实测 True ｜ 预期 True
  → 断言：参数化泛型是「每次现造」的对象：is False / == True，所以第 8 格的校验器比 args 一律用 == ｜ 本格判据 7/7 条成立（不成立的那条上面会印 [不成立！]）
```

- **⑪** 参数化泛型是每次现造的对象：拿 `is` 比自己是 `False`，拿 `==` 比才是 `True`。
  所有「比注解」的代码一律写 `==`；写成 `is` 不会报错，只会在自己的分支里静默判假 —— 最难查的那种 bug。
- **⑫** 先存进变量再比自己 → `True`，所以这不是「对象不稳定」，是「每次现造」。
- **⑬** 顺带钉一条等价关系：`Optional[int]` 与 `int | None` 两种写法比出 `True`，
  和第 08 节第 7 格「它其实是个 union」是同一件事。

**手搭一个两层的最小运行时候验器（外加它漏在哪）**

```
=== 8. 手搭一个两层的最小运行时候验器（外加它漏在哪）===
  check 的源码 14 行（含 def 和 docstring），只认三种形状：裸类 / X | Y / list+dict，其余一律放过
  [成立] 负对照（先让它响）：check([1, 'x'], list[int])：实测 False ｜ 预期 False
  [成立] 正例：check([1, 2], list[int])：实测 True ｜ 预期 True
  [成立] 负例：check({'a': 1, 'b': 'x'}, dict[str, int])：实测 False ｜ 预期 False
  [成立] 正例：check({'a': 1}, dict[str, int])：实测 True ｜ 预期 True
  [成立] Union 正例：check(3, int | str)：实测 True ｜ 预期 True
  [成立] Union 负例：check(3.5, int | str)：实测 False ｜ 预期 False
  [成立] Optional[int] 认 None（NoneType 也是裸类）：实测 True ｜ 预期 True
  [成立] 嵌套：check({'a': [1, 'x']}, dict[str, list[int]])（递归吃到里层）：实测 False ｜ 预期 False
  —— 它漏的地方，逐条实测（True = 没查出问题，也就是漏了）：
  [成立] 漏 1 定长元组：check((1, 2), Tuple[int, str])（正确答案是 False）：实测 True ｜ 预期 True
  [成立] 漏 2 可变参数：check((1, 'x'), Tuple[int, ...])（正确答案也是 False）：实测 True ｜ 预期 True
  [成立] 漏 3 Literal：check('zzz', Literal['a','b'])：实测 True ｜ 预期 True
  [成立] 漏 4 Annotated：check('x', Annotated[int, '元数据'])：实测 True ｜ 预期 True
  [成立] 漏 5 Any：check('x', Any) 不是放过，是当场 TypeError：实测 'TypeError: typing.Any cannot be used with isinstance()' ｜ 预期 'TypeError: typing.Any cannot be used with isinstance()'
      根因：isinstance(Any, type) 是 True —— Any 的「长得像类」骗过了裸类分支
  [成立] 漏 6 注解写成 None 这个词：check(1, None) 直接放过：实测 True ｜ 预期 True
  [成立] 漏 7 协议：check(Wrong(), Speaks) 落到 isinstance，于是只剩「有没有 speak 这个名字」：实测 True ｜ 预期 True
      （这条 True 是真漏：第 2 格已经演过 Wrong().speak(loud=True) 当场 TypeError）
  [成立] 漏 8 bool 是 int 的儿子：check([True], list[int]) 里 True 算合法元素：实测 True ｜ 预期 True
  → 断言：对 list/dict/Union 它真的响（4 条负例全 False）；元组（定长与 ...）、Literal、Annotated、None 这个词、协议、bool 子类共 7 处一律放过，Any 那处直接 TypeError —— 自己搭的网，网眼要自己数 ｜ 本格判据 16/16 条成立（不成立的那条上面会印 [不成立！]）
```

- **⑭** 这个校验器源码十四行，只认三种形状：裸类 / `X | Y` / 列表与字典，其余一律放过。
  对这三种它**真的响**：四条负例全部给出 `False`，嵌套的字典里层也递归吃到。
- **⑮** 漏的每一处都不是「没想到」，是分支没写：定长元组、可变参数元组、`Literal`、`Annotated`、
  `None` 这个词、协议、`bool` 是 `int` 的儿子（`True` 当合法元素），加上 `Any` 那处直接 `TypeError`。
- **⑯** 那条协议分支是本格的续集：`check(Wrong(), Speaks)` 落到 `isinstance`，于是只剩「有没有那个名字」——
  第 2 格已经演过它调用时当场炸。用法：**把网眼清单写进注释**，否则下一个读代码的人会以为字典全查过了。

**这一节记住一句话**：Protocol 只查名字、`get_origin`/`get_args` 只查形状，签名与越界值永远没人查 —— 运行时所有的「类型检查」都是你在替解释器写它不肯写的那段循环。

---

## 第 10 节 · f 篇完整跑：本机唯一的第三方库 typeguard —— 它到底替你挡了什么

读者的直觉很省事：「装个类型检查库就等于有了静态检查器，写完就报错」。屏幕上它是**运行时插桩**：
只有你真的调用那个函数时，它才去读注解、做比较。它和静态检查的差别，恰好是第 08 节那条「谁读、谁比」的
自动化版本 —— 而它是本章唯一的第三方库，因为静态检查器在本机一件都不在场（第 01 节实测过，本篇第 1 格再当场探一遍）。

最反直觉的是第 4 格：默认策略**只查容器的第一个元素**。同一个类型、同一个坏值，
把它放在第 0 位就响，挪到第 1 位就静默放行 —— 「我加了类型检查」和「检查真的覆盖到了」是两句话。

**先点名：工具清单，和版本号从哪来**

```
=== 1. 先点名：本章的六把静态尺一把都不在，连 pip 都没有 ===
  取证对象：`importlib.util.find_spec(工具名)` 是「这个包能不能被 import」的最省事的问法。
  │ ① 七件里在场的 → []
  │ ② 缺席的（按字母序）→ ['black', 'flake8', 'mypy', 'pylint', 'pyright', 'pytest', 'ruff']
  │ ③ `python3 -m pip --version` 的 rc 与原文 → rc=1 'python3: No module named pip'
  → 所以「装一个 mypy 再学这一章」在本机不是建议，是做不到的事（不联网、不装包是本章红线）。
  [成立] ①：实测 [] ｜ 预期 []
  [成立] ③：实测 (1, 'python3: No module named pip') ｜ 预期 (1, 'python3: No module named pip')
  → 断言：静态检查器七件全缺席（在场清单逐字 = []），pip 那条是 rc=1 + 原文 'No module named pip' ｜ 本格判据 2/2 条成立（不成立的那条上面会印 [不成立！]）

=== 2. typeguard 是谁、版本号从哪来（它自己身上没有 __version__）===
  取证对象：`typeguard.__file__` 与 dist-packages 里的 dist-info 目录名。
  │ ① typeguard.__file__（折过前缀）→ '<pyroot>/lib/python3/dist-packages/typeguard/__init__.py'
  │ ② hasattr(typeguard, '__version__') → False
  │ ③ 从旁边那个 dist-info 目录名读到的版本 → ['4.4.4']
  → 想知道第三方库的版本，`import x; x.__version__` 只是**运气好**；
    真正稳定的位置是包目录旁边那份 `<包名>-<版本>.dist-info`（pip 不在场时也只能这么读）。
  [成立] ②：实测 False ｜ 预期 False
  [成立] ③：实测 ['4.4.4'] ｜ 预期 ['4.4.4']
  → 断言：这份 typeguard 没有 __version__ 属性（False），版本号只能从 dist-info 目录名读到 4.4.4 ｜ 本格判据 2/2 条成立（不成立的那条上面会印 [不成立！]）
```

- **①** 在场清单逐字是空的，七件按字母序全落在缺席里；`pip` 那条是 `rc=1` 加一句原文（逐字在围栏）。
  所以「装一个 mypy 再学这一章」在本机不是建议，是做不到的事。
- **②** 这份 typeguard 自己身上**没有** `__version__` 属性（实测 `False`），那个版本号 `4.4.4` 是从包目录旁边
  那份 `<包名>-<版本>.dist-info` 的目录名读出来的。写取版本的代码时别指望 `x.__version__` 一定在 —— 它只是运气好。

**默认只查第一个元素**

```
=== 4. 默认策略只查容器第一个元素：[1,'x'] 过，['x',1] 拦 ===
  取证对象：同一个 list[int]，把错元素放在第 0 位和第 1 位。
  │ ① check_type(['x', 1], list[int]) →  → 炸：TypeCheckError: item 0 of list is not an instance of int
  │ ② check_type([1, 'x'], list[int]) →  → [1, 'x']
  │ ③ dict[str,int]：错值在第一个键 →  → 炸：TypeCheckError: value of key 'b' of dict is not an instance of int
  │ ④ dict[str,int]：错值在第二个键 →  → {'a': 1, 'b': 'x'}
  → 判据是「第 0 位响、后面的静默」。dict 那两格为什么两个键都没响？因为第一个键的**值**是 1（合法），
    它连第二个键根本不看 —— 顺序稳定（同一进程里字面量顺序就是插入顺序），所以这条可以逐字复现。
  [成立] ①：实测 'TypeCheckError: item 0 of list is not an instance of int' ｜ 预期 'TypeCheckError: item 0 of list is not an instance of int'
  [成立] ②：实测 [1, 'x'] ｜ 预期 [1, 'x']
  [成立] ④：实测 {'a': 1, 'b': 'x'} ｜ 预期 {'a': 1, 'b': 'x'}
  → 断言：list：错在第 0 位报 'item 0 of list is not an instance of int'，错在第 1 位原样返回；dict 第二个键同样静默 ｜ 本格判据 3/3 条成立（不成立的那条上面会印 [不成立！]）
```

- **③** 列表：坏元素在第 0 位 → `TypeCheckError`，屏幕上那句报错点的就是它；把同一个坏元素挪到第 1 位 →
  原样返回、一声不响。
- **④** 字典更阴：它只看第一个键的**值**，那个值合法就整本连第二个键都不看。顺序是稳定的
  （字面量顺序就是插入顺序），所以这条静默**可以逐字复现**，不是偶发。
- **⑤** 用法：把那个容器策略开关显式改开 —— 它就在 `check_type` 的签名里躺着（枚举两个成员，第 5 格），
  换成全量策略后同一个输入就拦。什么时候失效：你以为「它查了整个列表」。
- **⑥** 顺带一条签名坑：`check_type` 现在只吃两个位置参数，按旧文档传三个位置参数当场 `TypeError`
  （原文见第 3 格）—— 抄来的老代码在这台机器上第一步就撞。

**`@typechecked` 管三处、不管两处**

```
=== 6. @typechecked 管什么：参数、返回值、函数体里的局部变量注解 ===
  取证对象：三个被 @typechecked 装饰的函数，各撞一处。
  │ ① add('a', 2) →  → 炸：TypeCheckError: argument "a" (str) is not an instance of int
  │ ② to_str(1)（返回值不是 str）→  → 炸：TypeCheckError: the return value (int) is not an instance of str
  │ ③ local_ann(1)（体内 y: str = 5）→  → 炸：TypeCheckError: value assigned to y (int) is not an instance of str
  │ ④ 实例属性赋值不拦：Holder(1).n = '字符串' 之后 holder.n →  → '字符串'
  → ③ 是本篇最值钱的读数：局部变量注解在标准库里是纯粹的文档（a 篇第 5 格），
    但 typeguard 是**插桩执行**的，它把赋值也管起来了。④ 则说明它管不到属性：
    `self.n = 值` 不是「参数」，没有注解可查 —— 数据类的字段类型它也不管。
  [成立] ①：实测 'TypeCheckError: argument "a" (str) is not an instance of int' ｜ 预期 'TypeCheckError: argument "a" (str) is not an instance of int'
  [成立] ②：实测 'TypeCheckError: the return value (int) is not an instance of str' ｜ 预期 'TypeCheckError: the return value (int) is not an instance of str'
  [成立] ③：实测 'TypeCheckError: value assigned to y (int) is not an instance of str' ｜ 预期 'TypeCheckError: value assigned to y (int) is not an instance of str'
  → 断言：参数、返回值、体内局部变量赋值三处原文逐字如上；实例属性赋值不拦（值原封不动是 '字符串'） ｜ 本格判据 3/3 条成立（不成立的那条上面会印 [不成立！]）
```

- **⑦** 它管：参数类型、返回值类型、**函数体内被注解变量的赋值**。第三条最值钱：局部变量注解在标准库里
  纯粹是文档（第 02 节第 6 格），插桩之后连赋值也被管起来了。
- **⑧** 它不管：实例属性赋值（值原封不动留在对象上），以及 dataclass 字段 —— `self.n = 值` 不是「参数」，
  没有注解可查。什么时候失效：你以为装饰了函数就兜住了对象的状态，而状态住在属性上，它够不着。

**它靠读源码：三种入口三种结局**

```
=== 7. 它靠读源码：`-c` 里当场 TypeError，标准输入里只发一句警告，写成文件才好 ===
  取证对象：同一份八行代码，三种入口。
  │ ① python3 -c '<代码>' 的 rc 与最后一行报错 → rc=1 "TypeError: <module '__main__' (<class '_frozen_importlib.BuiltinImporter'>)> is a built-in module"
  │ ② 同样的代码写成 tg_plain.py 再跑 → rc=0 out='g([1, 2]) = 3'
  │ ③ 喂给标准输入（`python3` 不带参数）→ rc=0 最后一行 '<stdin>:3: InstrumentationWarning: cannot instrument functions defined in a REPL -- not typechecking __main__.g'
  → 报错原文里那句 "<module '__main__' ...> is a built-in module" 是 inspect.getfile 说的：
    typeguard 装饰函数时要拿这个函数的源码来插桩，`-c`/REPL/标准输入这三份代码**没有文件**可读。
    两条路的区别只在「有没有人告诉你」：`-c` 里它拿源码就炸了（TypeError 冒到顶层，rc=1）；
    喂标准输入时 `inspect` 那条路它绕开了，改成发一句 InstrumentationWarning 然后照常跑完 —— rc=0、
    g([1, 2]) 也算出来了，但那个装饰器**根本没装上**，检查一次都没发生。
    这条对读者的实际意义：别在 `python -c` 或交互式里试 typeguard，会以为它坏了或以为它在管。
  [成立] ①：实测 1 ｜ 预期 1
  [成立] ②：实测 'g([1, 2]) = 3' ｜ 预期 'g([1, 2]) = 3'
  [成立] ③ rc：实测 0 ｜ 预期 0
  [成立] ③ 最后一行：实测 '<stdin>:3: InstrumentationWarning: cannot instrument functions defined in a REPL -- not typechecking __main__.g' ｜ 预期 '<stdin>:3: InstrumentationWarning: cannot instrument functions defined in a REPL -- not typechecking __main__.g'
  → 断言：同一个装饰器三种入口：-c 是 rc=1 的 TypeError，标准输入是 rc=0 + 一句 InstrumentationWarning（警告里那句 not typechecking 就是「没装上」的原文），只有写成 .py 才真跑出 g([1, 2]) = 3 ｜ 本格判据 4/4 条成立（不成立的那条上面会印 [不成立！]）
```

- **⑨** 同一份代码三个入口：`-c` 里它拿源码就炸（`rc=1`）；喂标准输入它绕开那条路，只留一句警告，
  `rc=0` 且函数照常算出结果 —— 但**装饰器根本没装上**，一次检查都没发生；写成 `.py` 才真的检查。
- **⑩** 那句警告自己就说明了「没在检查」（逐字在围栏）。实际意义：别在 `-c` 或交互式里试 typeguard，
  你会要么以为它坏了、要么以为它在管。看到 `rc=0` 别读成「它检查过且通过了」，先把 stderr 翻一遍。

**注解里的名字求不出值时：两条路，三种脸**

```
=== 8. 注解里的名字求不出值时：装饰器当场崩，check_type 按策略给三张脸 ===
  取证对象 ①：`@typechecked` + `def g(v: NotImported)`（注解里的名字根本不存在），子进程跑。
  │ rc=1，traceback 最后一行 → "NameError: name 'NotImported' is not defined"
  取证对象 ②：同一个名字改成 **ForwardRef 对象** 喂 check_type，三种策略各跑一遍：
  │ WARN（默认）→ 返回 '随便什么'｜warning 1 条 [('TypeHintWarning', "Cannot resolve forward reference 'NotImported'")]
  │ ERROR      → 炸 NameError: name 'NotImported' is not defined｜warning 0 条
  │ IGNORE     → 返回 '随便什么'｜warning 0 条
  取证对象 ③：把注解写成**字符串** "NotImported"，三种策略跑一遍，看它们是否给得出区别：
  │ 三条读数是否完全相同 → True，实际内容：('返回', "'随便什么'")｜[('TypeHintWarning', "Skipping type check against 'NotImported'; this looks like a string-form forward reference imported from another module")]
  → 两件事要分开记：
    a) 装饰器那条路**没有降级** —— 插桩后的函数体在调用时才去读注解，读不到就是 NameError，程序直接崩
       （上面 rc=1 那句就是它）。所以「typeguard 会宽容地跳过求不出的注解」这句只适用于 check_type。
    b) check_type 那条路确实给三种脸：ERROR 抛 NameError、WARN 发一条 warning 后放行、IGNORE 连 warning 都不发
       （静默放行是三者里最危险的：你既没被检查，也没被通知）。
    c) 但 ③ 说明这套策略**只对真正的 ForwardRef 对象生效**：字符串注解会被更早的一条分支拦掉，
       三种策略给出逐字相同的读数 —— 那句原文里的 imported from another module 就是它自陈的理由。
  [成立] ① rc：实测 1 ｜ 预期 1
  [成立] ① 最后一行：实测 "NameError: name 'NotImported' is not defined" ｜ 预期 "NameError: name 'NotImported' is not defined"
  [成立] ② WARN：实测 (('返回', "'随便什么'"), [('TypeHintWarning', "Cannot resolve forward reference 'NotImported'")]) ｜ 预期 (('返回', "'随便什么'"), [('TypeHintWarning', "Cannot resolve forward reference 'NotImported'")])
  [成立] ② ERROR：实测 (('炸', "NameError: name 'NotImported' is not defined"), []) ｜ 预期 (('炸', "NameError: name 'NotImported' is not defined"), [])
  [成立] ② IGNORE：实测 (('返回', "'随便什么'"), []) ｜ 预期 (('返回', "'随便什么'"), [])
  [成立] ③ 字符串三种策略同读数：实测 True ｜ 预期 True
  [成立] ③ 原文：实测 [('TypeHintWarning', "Skipping type check against 'NotImported'; this looks like a string-form forward reference imported from another module")] ｜ 预期 [('TypeHintWarning', "Skipping type check against 'NotImported'; this looks like a string-form forward reference imported from another module")]
  → 断言：装饰器 rc=1 + NameError 原文；ForwardRef 三种策略分别是「1 条 warning 放行 / 抛 NameError / 0 条静默放行」；字符串注解三种策略逐字相同（判据条数=1） ｜ 本格判据 7/7 条成立（不成立的那条上面会印 [不成立！]）
```

- **⑪** 装饰器那条路**没有降级**：插桩后的函数体在调用时才去读注解，读不到就是 `NameError`，程序直接崩
  （屏幕上那句 `rc=1` 就是它）。所以「它会宽容地跳过求不出的注解」这句**只适用于直调 `check_type`**。
- **⑫** `check_type` 那条路给三种脸：`ERROR` 抛 `NameError`；`WARN`（默认）发一条 warning 后放行；
  `IGNORE` 连 warning 都不发 —— 静默放行是三者里最危险的：你既没被检查，也没被通知。
- **⑬** 而这套策略**只对真正的 ForwardRef 对象生效**：字符串注解被更早的一条分支拦掉，
  三种策略给出逐字相同的读数，那句 warning 原文自己陈了理由。什么时候失效：
  开了第 03 节那个「强制字符串」的开关之后满屏都是字符串注解，此时你以为在按策略降级，其实一条都没走。

**不想逐个写装饰器：给整个包自动加检查**

```
=== 9. 不想逐个写装饰器：install_import_hook 给整个包自动加检查 ===
  取证对象 ①：`install_import_hook` 的签名（第一个位置参数叫什么，决定你传字符串有没有用）。
  │ 签名 → "(packages: 'Iterable[str] | None' = None, *, cls: 'type[TypeguardFinder]' = <class 'typeguard.TypeguardFinder'>) -> 'ImportHookManager'"
  取证对象 ②：三个内容完全一样的模块 payload/payload2/payload3，四种「装 hook / 加载」顺序。
  │ 子进程 rc=0，四行读数逐字：
  │    A 不带 hook：TypeError | unsupported operand type(s) for +: 'int' and 'str'
  │    B hook 内：TypeCheckError | item 0 of argument "v" (list) is not an instance of int
  │    C 退出 with 之后再调 payload2：TypeCheckError | item 0 of argument "v" (list) is not an instance of int
  │    D 先 import 后装 hook：TypeError | unsupported operand type(s) for +: 'int' and 'str'
  → 四行里有两对是**同一句原文**，而它们之间的差别恰好就是这格要教的：
    A=D（都报 TypeError）：payload 从没被插桩；payload3 是「先 import 后装 hook」—— 它已经在
      sys.modules 里，hook 只改「之后加载的模块」，对它一次都没生效。这两行同读数就是那条 docstring
      「This only affects modules loaded **after** this hook has been installed」的实证。
    B=C（都报 TypeCheckError）：hook 退出只卸载 finder，已经插好桩的那个模块对象继续被检查。
      换句话说「出了 with 就恢复干净」是错的直觉 —— 影响范围是**加载时机**，不是 with 的生命周期。
    还有一条：把包名当第一个位置参数传（`install_import_hook("payload2")`）不会报错，
    只会**静默地什么都不插桩** —— 上面那个签名里的参数名 packages 是唯一正确的写法。
  [成立] ① 参数名是 packages（关键字），没有 position 的 path：实测 "(packages: 'Iterable[str] | None' = None, *, cls: 'type[TypeguardFinder]' = <class 'typeguard.TypeguardFinder'>) -> 'ImportHookManager'" ｜ 预期 "(packages: 'Iterable[str] | None' = None, *, cls: 'type[TypeguardFinder]' = <class 'typeguard.TypeguardFinder'>) -> 'ImportHookManager'"
  [成立] ② 四行读数逐字：实测 ["A 不带 hook：TypeError | unsupported operand type(s) for +: 'int' and 'str'", 'B hook 内：TypeCheckError | item 0 of argument "v" (list) is not an instance of int', 'C 退出 with 之后再调 payload2：TypeCheckError | item 0 of argument "v" (list) is not an instance of int', "D 先 import 后装 hook：TypeError | unsupported operand type(s) for +: 'int' and 'str'"] ｜ 预期 ["A 不带 hook：TypeError | unsupported operand type(s) for +: 'int' and 'str'", 'B hook 内：TypeCheckError | item 0 of argument "v" (list) is not an instance of int', 'C 退出 with 之后再调 payload2：TypeCheckError | item 0 of argument "v" (list) is not an instance of int', "D 先 import 后装 hook：TypeError | unsupported operand type(s) for +: 'int' and 'str'"]
  → 断言：签名参数名 packages 逐字；四行读数 = A/D 同一句 TypeError、B/C 同一句 TypeCheckError（判据是这四行逐字对上，两两相同正是「管加载时机、不管 with 生命周期」的形状） ｜ 本格判据 2/2 条成立（不成立的那条上面会印 [不成立！]）
```

- **⑭** 四行读数两两相同，这就是结论。`A=D`（同一句 `TypeError`）：先 import 后装 hook 的那个模块已经在
  `sys.modules` 里，hook 只改「**之后**加载的模块」，对它一次都没生效。`B=C`（同一句 `TypeCheckError`）：
  退出 with 只卸载 finder，已经插好桩的那个模块对象继续被检查。
- **⑮** 所以「出了 with 就恢复干净」是错的直觉 —— 影响范围是**加载时机**，不是 with 的生命周期。
- **⑯** 还有一个静默坑：把包名当第一个位置参数传不会报错，只会**什么都不插桩**；
  正确的写法是那个表示「包集合」的关键字参数（签名逐字在上面围栏里）。

**它的账本**：它管参数、返回值、体内被注解变量的赋值；它不管实例属性、dataclass 字段、以及容器里第一个元素
以外的元素；它要求代码在文件里；本篇故意不测耗时 —— 慢多少倍每跑必变，而本章的捕获要求两遍逐字一致，
要量就自己跑 `python -m timeit`，别引用别处的倍数。

**这一节记住一句话**：typeguard 替你挡的是「每次调用时那几处比较」，替你挡不住的是第一个元素之外的容器成员、属性、dataclass 字段，以及一切静态问题。

---

## 第 11 节 · 矩阵格：同一个坏值摆在四把工具前面（跨 d/e/f/g 四篇的合并视图）

前面几篇各说各的能拦什么，容易各信各的。这一格把**同一个调用**摆到四把工具前面：两个字符串喂进一个
声明收两个整数、返回整数的函数 —— 三个被比较的函数签名完全相同，区别只在「有没有人读注解、读它的人查不查值」。
一张表看完：裸解释器、自己写的 `isinstance`、`@typechecked`、以及 g 篇那把 `ast` 尺。

```
$ python3 matrix_tools.py
被调用的是同一个：add_plain / add_manual / add_tg 都是 def add(a: int, b: int) -> int
  ① 裸解释器 + 注解（d 篇） → 静默：返回 '两个字符串'
  ② 自己在函数体里 isinstance（d 篇的负对照） → 响：TypeError: 自己写的那一遍检查响了
  ③ @typeguard.typechecked（f 篇） → 响：TypeCheckError: argument "a" (str) is not an instance of int
```

- **①** 第一行永远静默：它返回拼出来的字符串，一声不响（第 08 节第 2 格的同一家族）。
- **②** 第二行要你真的在函数体里比较：响，报错原文是自己写的那句。
- **③** 第三行要你真的装上装饰器再调用：响，而且那句原文与第 10 节第 6 格逐字同源 ——
  那一格给的是单独取证，这里给的是调用现场，两屏对着看最省时间。

```
  ④ 用 g 篇那把 ast 尺扫本文件里的 add_plain：
     尺对这份源码报的清单 → [] ← 空的是对的：int/str/add 全是可见名字
     尺对「值传错了」这件事报几条 → 0
  → 结论排一排：①永远静默；②③要你真的去调用那把工具；④根本不针对值。
     「加了注解就安全」这句话里被省略的是：谁读它、什么时候读、读它的人查不查值。
[退出码 0]
```

- **④** 第四行的尺对「值传错了」这件事报 0 条：**它根本不针对值**。它对这份源码给出的清单是空的，
  而空是正确结果（那几个名字全都可见）—— 千万别把「尺没报」读成「尺替你查过了」。
- **⑤** 四行排完就是本章的用法表：① 永远静默；②③ 要你真的去调用那把工具；④ 只看名字可见性。
  「加了注解就安全」这句话里被省略的三个成分是：谁读它、什么时候读、读它的人查不查值。

```
★ 注意尺那一行有个坑：它扫的是自己所在的**整个文件**，那份清单里连一个字符串都没多 ——
  「守卫扫到自己」这件事 bash 手册第 10 章专门演过，这里是它在 Python 侧的同款现场。
```

- **⑥** 最后那条 ★ 是个坑，抄尺之前先想清楚：这把尺扫的是它自己所在的**整个文件**，那份清单里连一个字符串都
  没多 —— 「守卫扫到自己」这件事在 Python 侧的同款现场就在这里。写自查脚本时，先问它会不会把自己算进去。

**这一节记住一句话**：同一个坏值面前，天真运行时永远静默，两把真会响的工具都要你主动调用它们，而那把静态尺从头到尾不看值。

---

## 第 12 节 · g 篇完整跑：本机没有 mypy，那就用 ast 现搓一把最小的尺

主线④ 的落点。第 01 节已经探明：八把静态尺一把都不在，连 `pip` 都没有。读者的自然反应是
「那本章最后一篇一定是空谈」—— 恰恰相反，这一篇**不装任何东西**也能把「静态检查」做出来，
因为原料 `ast` 就在标准库里，而注解在语法树里就是一个普通表达式节点。

```
=== 1. 先看清原料：注解在语法树里就是普通表达式节点 ===
  取证对象：上面那份 SRC1（模块里就一个函数 f，签名里埋了四种形状）。
  │ ① 尺从 f 的签名里读到的注解节点（位置 | AST 节点类 | 里面出现的名字）：
  │    f 参数 a ｜ Name ｜ ['Widget']
  │    f 参数 b ｜ Name ｜ ['Missing']
  │    f 参数 c ｜ Attribute ｜ ['argparse']
  │    f 参数 d ｜ Name ｜ ['Another']
  │    f 返回值 ｜ Attribute ｜ ['json']
  │ ② 函数体里那条 `x: int = 1` 是 AnnAssign 节点，默认口径**不**算它：
  │    只算签名 → 5 条；连函数体里的变量注解一起算 → 6 条
  → 关键事实：这里没有一个字节需要**执行**这份代码。ast.parse 只是把源码读成树，
    所以哪怕这个函数你永远不调用，注解长什么样、里面用了哪些名字，工具照样看得见。
  [成立] ① 签名里的注解位置（按 ast.walk 的先后）：实测 ['f 参数 a', 'f 参数 b', 'f 参数 c', 'f 参数 d', 'f 返回值'] ｜ 预期 ['f 参数 a', 'f 参数 b', 'f 参数 c', 'f 参数 d', 'f 返回值']
  [成立] ① 对应的 AST 节点类：实测 ['Name', 'Name', 'Attribute', 'Name', 'Attribute'] ｜ 预期 ['Name', 'Name', 'Attribute', 'Name', 'Attribute']
  [成立] ① 每个注解里的名字：实测 [['Widget'], ['Missing'], ['argparse'], ['Another'], ['json']] ｜ 预期 [['Widget'], ['Missing'], ['argparse'], ['Another'], ['json']]
  [成立] ② 两种口径的条数：实测 (5, 6) ｜ 预期 (5, 6)
  → 断言：签名 5 个注解的位置/节点类/名字三张清单逐字，两种口径分别 5 与 6 条 ｜ 本格判据 4/4 条成立（不成立的那条上面会印 [不成立！]）
```

第一格先把原料摊开：`f` 的签名里埋了四种形状（`Widget`、`Missing`、`argparse`、`Another`、返回 `json.Cfg` 那种属性式注解），
`ast` 读出来是 `(位置 | 节点类 | 里面的名字)` 三张清单，一个字节都不需要执行这份代码。
② 那行是**口径问题的起点**：`只算签名 → 5 条；连函数体里的变量注解一起算 → 6 条` ——
同一份源码，两种「什么算注解」的口径，条数就差一格。后面每一格都在还这笔账。

第一把尺写出来之后，先让它跑，再看它跟另一把尺的分歧：

```
=== 3. 把口径收紧：只认写在模块顶层的 import → 同一份源码多报一条 ===
  取证对象：同一份 SRC1，`deep=False`（只扫 tree.body 那一层的 import/def/class）。
  │ ① 收紧后的清单 → ['f 参数 b:Missing', 'f 参数 c:argparse', 'f 参数 d:Another']
  │ ② 两口径的差集 → ['f 参数 c:argparse']
  → 多出来的正是 c 那条。此刻三把尺对着同一行源码给三种答案：
      mypy 型（deep）：不报 —— 它认 TYPE_CHECKING 分支；
      运行时型（tight）：报 —— 第 3 篇屏上那句 `NameError: name 'argparse' is not defined` 就是它；
      第 3 篇里 `import argparse` 在 TYPE_CHECKING 下没执行、sys.modules 里没有它，正是这一条的实证。
    所以「注解里这个名字到底可见吗」不是一个问题，是**两个问题**：对谁可见。
    这把几十行的尺第一次显出它的难处：它必须替自己选一个「谁在看」，选错方向就漏报或误报。
  [成立] ① tight 口径报什么：实测 ['f 参数 b:Missing', 'f 参数 c:argparse', 'f 参数 d:Another'] ｜ 预期 ['f 参数 b:Missing', 'f 参数 c:argparse', 'f 参数 d:Another']
  [成立] ② 差集：实测 ['f 参数 c:argparse'] ｜ 预期 ['f 参数 c:argparse']
  → 断言：tight 口径逐字 3 条、两口径差集恰为 c:argparse 一条 ｜ 本格判据 2/2 条成立（不成立的那条上面会印 [不成立！]）
```

`deep` 口径（把 `if TYPE_CHECKING:` 分支里那句 `import argparse` 也算成可见）报 2 条；
`tight` 口径（只认模块顶层的 import）报 3 条，差集恰好是 `['f 参数 c:argparse']` 那一条。
这一格是本篇最容易被跳过、也最该被记住的一格：**「这个名字可见吗」不是一个问题，是两个问题 —— 对谁可见。**
类型检查器眼里 `TYPE_CHECKING` 恒真，所以它不报；运行时那句 import 根本没执行，所以它炸。
一把几十行的尺必须替自己选「站在谁的角度」，选错方向就漏报或误报。

选完之后，尺和运行时站到同一行源码两边：

```
=== 4. 同一份源码交给运行时：它只报第一个 ===
  取证对象：把 SRC1 落成 src1.py，子进程里读 `f.__annotations__`（VALUE 读法）。
  │ ① 子进程 rc=0，屏上那行 → "读注解炸： NameError | name 'Missing' is not defined"
  │ ② 尺（deep 口径）报了几条 → 2 条；运行时这条路上报了几条 → 1 条
  → 这就是本章最实在的一条对照：**静态尺一次给全部，运行时一次只给第一个**。
    Missing 与 Another 都是真错，但读注解的过程在第一个名字上就抛了异常，
    另一个名字**根本没轮到**。你想「跑一下就看完所有注解错误」是办不到的 ——
    这条差异就是「为什么值得自己搓一把尺」的全部理由。
  [成立] ① rc 与原文：实测 (0, "读注解炸： NameError | name 'Missing' is not defined") ｜ 预期 (0, "读注解炸： NameError | name 'Missing' is not defined")
  [成立] ② 条数对照：实测 (2, 1) ｜ 预期 (2, 1)
  → 断言：子进程 rc=0（异常自己接住了）且原文只有 Missing；尺 2 条 vs 运行时 1 条 ｜ 本格判据 2/2 条成立（不成立的那条上面会印 [不成立！]）
```

**静态尺一次给全部，运行时一次只给第一个。** `Missing` 和 `Another` 都是真错，
但读注解的过程在第一个名字上就抛异常了，另一个名字**根本没轮到**（屏上 `② 尺（deep 口径）报了几条 → 2 条；运行时这条路上报了几条 → 1 条`）。
你想「跑一下就看完所有注解错误」是办不到的 —— 这一条差异就是「值得自己搓一把尺」的全部理由。

尺也有结构性盲区，本篇把它当场量出来，而不是藏起来：

```
=== 5. 尺的第一类漏网：属性注解只看根上的名字 ===
  取证对象：`def h(v: int) -> json.Cfg`（json 是真有的模块，Cfg 这个属性它没有）。
  │ ① 尺报的清单 → []   ← 空的
  │ ② 同一段交给运行时 → rc=0，那行 "读注解炸： AttributeError | module 'json' has no attribute 'Cfg'"
  → `json` 在可见名表里，于是尺放行了；但 `json.Cfg` **取属性**这一步只有真读注解才会发生，
    一发生就是 AttributeError。尺只看名字，不看名字身上有没有那个属性 —— 这是它的结构性盲区。
    要补这一格，就得真去 import 那个模块再 getattr 一遍：那已经不再是「纯静态」，
    而是 mypy 那种「跟着 import 图把语义也解一遍」的活，几十行的尺做不动，本章也不假装做得到。
  [成立] ① 尺一条都不报：实测 [] ｜ 预期 []
  [成立] ② 运行时原文：实测 "读注解炸： AttributeError | module 'json' has no attribute 'Cfg'" ｜ 预期 "读注解炸： AttributeError | module 'json' has no attribute 'Cfg'"
  → 断言：尺报 []、运行时报 AttributeError 原文 —— 同一行源码一个放行一个崩 ｜ 本格判据 2/2 条成立（不成立的那条上面会印 [不成立！]）
```

`json` 在可见名表里，于是尺放行了；可 `json.Cfg` **取属性**那一步只有真去读注解才发生，
一发生就是 `AttributeError: module 'json' has no attribute 'Cfg'`。要补这一格就得真去 import 那个模块再 `getattr`，
那已经不是「纯静态」，而是 mypy 那种「跟着 import 图把语义解一遍」的活 —— 几十行的尺做不动，本章也不假装做得到。

再看两条「尺赢」和「口径要先选」的现场：

```
=== 8. 看着更懂作用域的 symtable，恰恰不把注解当使用 ===
  取证对象：同一份 SRC1 交给标准库 symtable（它是编译器的作用域视图，比 ast 更「懂」层次）。
  │ ① 顶层三个「只在注解里被用过」的名字，(is_imported, is_referenced) → json=(True, False), argparse=(True, False), TYPE_CHECKING=(True, True)
  │ ② f 那张子表里的符号名 → ['a', 'b', 'c', 'd', 'x']
  │ ③ 其中标了「被注解过」的 → ['x']
  → 三处一起看：`json` 与 `argparse` 在 symtable 眼里 imported 但 **referenced=False** ——
    注解里的使用它一个都不记；f 的子表里没有 Missing、也没有 Another，
    那两个名字连「未定义」都算不上，就是**不存在**；is_annotated() 只认得到体内那条 x。
    结论很实用：想做注解相关的检查，别走 symtable 这条路，自己 ast.walk 是唯一稳的。
  [成立] ① json / argparse 的 (imported, referenced)：实测 ((True, False), (True, False)) ｜ 预期 ((True, False), (True, False))
  [成立] ② Widget 也一样：只在注解里用 → referenced False：实测 (False, False) ｜ 预期 (False, False)
  [成立] ③ f 的子表里没有那两个坏名字：实测 [] ｜ 预期 []
  [成立] ④ f 的子表符号名与体内标注：实测 (['a', 'b', 'c', 'd', 'x'], ['x']) ｜ 预期 (['a', 'b', 'c', 'd', 'x'], ['x'])
  → 断言：json/argparse 全 (True,False)、Widget (False,False)、f 子表 5 个符号里无 Missing/Another、annotated 只有 x ｜ 本格判据 4/4 条成立（不成立的那条上面会印 [不成立！]）
```

第 8 格这条极其实用：`symtable` 是编译器的作用域视图，看着比 `ast` 更「懂」层次，
可是 `json=(True, False), argparse=(True, False)` —— **imported 但 referenced=False**，
注解里的使用它一个都不记；`f` 那张子表里连 `Missing`、`Another` 两个名字都没有（它们不是「未定义」，是不存在），
`is_annotated()` 只认得到函数体内那条 `x`。所以想做注解相关的检查，别走 `symtable`，自己 `ast.walk` 是唯一稳的。

```
=== 9. 尺最值钱的形状：一行都不执行，错照样抓到 ===
  取证对象：一个**永远不会被调用**的函数，签名里写错一个名字。
  │ ① 尺报的清单 → ['never_called 参数 x:WeDontCallIt']
  │ ② 直接把这个文件当脚本跑：rc=0 stdout='' stderr=''
  → 定义函数不会读注解（第 1 篇的第一格），不调用更不会 —— 运行时**全程静默**，
    rc=0、一个字都不输出。这不是「还没触发」：这段代码活着的那一天它都不会触发。
    尺在这里给出一条能改的错误，而它的成本是 ast.parse 一次。
    把本篇几格摆一排：第 4、5 格运行时更准，第 7、9 格尺更准，第 6 格看你要哪种口径 ——
    「静态 vs 运行时」不是一场谁赢谁输，是两张各有盲区的地图。
  [成立] ① 尺报的清单：实测 ['never_called 参数 x:WeDontCallIt'] ｜ 预期 ['never_called 参数 x:WeDontCallIt']
  [成立] ② 运行时的沉默：实测 (0, '', '') ｜ 预期 (0, '', '')
  → 断言：尺 1 条、运行时 rc=0 且 stdout/stderr 全空 —— 两条读数都在屏上 ｜ 本格判据 2/2 条成立（不成立的那条上面会印 [不成立！]）
```

第 9 格是这把尺最值钱的形状：一个**永远不会被调用**的函数，签名里写错一个名字 ——
尺报 1 条，而把那份源码当脚本跑是 `rc=0 stdout='' stderr=''`，运行时全程静默。
这不是「还没触发」：这段代码活着的那一天它都不会触发。成本只有 `ast.parse` 一次。

最后收口那一格先把尺自己的判据列全，再给它划一条「不能查」的线：

```
=== 10. 收口：这把尺能查什么、不能查什么（含一条负对照）===
  取证对象：`add(a: int, b: int) -> int` 被真真切切地用字符串调用了（第 4 篇那一族）。
  │ ① 尺对这份源码报的清单 → []   ← 空的
  │ ② 那份源码 import 之后立刻（模块顶层就调用了）→ rc=0
  │    total = 两个字符串
  → 尺报 0 条：`int`、`str`、`add` 全都在可见名表里，**没有任何名字是错的**。
    它只查「这个名字从这里能不能看见」，不查「这个值和这个类型匹不匹配」——
    后者要么交给运行时（第 6 篇 typeguard 那种插桩），要么就得真去实现一套类型规则（几十行做不到）。
  → 三条账，本篇每格都有屏上读数兜着：
     能查：注解里用了源码里没弄出来的名字（第 2/3/7/9 格），一次给全部（第 4 格）。
     不能查：属性存在性（第 5 格）、值与类型是否匹配（上面这条）、跨作用域的可见性细节。
     口径要先选：deep 对齐 mypy、tight 对齐运行时（第 3 格），函数体内变量注解要不要管（第 6 格）。
  [成立] ① 尺对值错误完全无感：实测 [] ｜ 预期 []
  [成立] ② 运行时那一行读数：实测 ['total = 两个字符串'] ｜ 预期 ['total = 两个字符串']
  [成立] ③ 本篇五个有错样本一共抓到这五个坏名字：实测 ['Another', 'Missing', 'MissingField', 'NeverImported', 'WeDontCallIt'] ｜ 预期 ['Another', 'Missing', 'MissingField', 'NeverImported', 'WeDontCallIt']
  → 断言：尺对 add 那份报 []、运行时那声 TypeError 在输出里一个字都没出现；五个有错的样本合起来恰好抓到 Missing/Another/NeverImported/MissingField/WeDontCallIt 五个名字 ｜ 本格判据 3/3 条成立（不成立的那条上面会印 [不成立！]）

  收尾自查：沙箱 mini-demo/ 删干净了吗 → True
  全篇判据合计 27/27 条成立（少一条上面就有 [不成立！]）
[退出码 0]
```

① 那行是诚实的边界：`add(a: int, b: int) -> int` 被字符串调用，尺报的是 **空清单** ——
`int`、`str`、`add` 全都在可见名表里，没有任何名字是错的。它查的是「这个名字从这里能不能看见」，
不查「这个值和这个类型匹不匹配」；后者要么交给运行时插桩（第 10 节的 typeguard），
要么就得真实现一套类型规则。③ 那行给了本篇的总账：五个有错的样本，尺恰好抓到五个坏名字，
`['Another', 'Missing', 'MissingField', 'NeverImported', 'WeDontCallIt']` 一个不多一个不少。

**这一节记住一句话**：静态检查不必等工具 —— `ast` 就能查「注解里的名字有没有被弄出来」，
一次给全部、不执行也能查；但它查属性存在性、查值与类型匹不匹配都是盲区，而「对谁可见」这个口径必须先替自己选。

---

## 第 13 节 · 跨篇小结：一张「谁在什么时候读注解」的合并账

这一节没有新事实：它把前面各篇的读数合成一张账，每一行都能回指到某一节。
现场只用 `exec` 造一个注解名不存在的函数，然后把四个时刻和两个入口依次过一遍 ——
**六段读数全部由同一条命令当场跑出**，没有一行手打：

```
$ python3 -c "
import annotationlib as A, inspect, re, sys, typing
from typing import get_type_hints
print(\"【时刻① 定义函数时】注解求值了吗 →\")
exec(\"def probe(v: NoSuchName) -> int:\n    return 0\", globals())
print(\"   函数建起来了吗 →\", callable(probe), \"｜ 此刻读没读过那个名字 → 屏幕上没有 NameError 就是没读\")
print(\"   （a 篇把这条量成了计数器：定义那一遍计数 0）\")
print(\"【时刻② 第一次读注解】谁读谁求值：\")
try:
    probe.__annotations__
    first = \"没炸\"
except Exception as e:
    first = type(e).__name__
print(\"   属性读 →\", first)
try:
    probe.__annotations__
    second = \"没炸\"
except Exception as e:
    second = type(e).__name__
print(\"   再读一次 →\", second, \"｜ 两遍都炸 = 失败不缓存（a 篇计数 1→2 的同一条）\")
print(\"【时刻③ 调用函数时】求值注解了吗 →\", \"屏幕上这行之前没有任何报错，就是没求值\")
print(\"   调用返回 =\", probe(3), \"｜ 运行时对值查了吗（d 篇八个现场 + 第 11 节矩阵①）→ 一行都不查\")
print(\"【时刻④ 建 dataclass 时】\")
import dataclasses
print(\"   c 篇/第 7 节矩阵量过：建类与实例化都不求值注解；忘 import ClassVar 会静默降级成普通字段\")
print(\"【入口】Format 四个成员（名字, 值）→\", [(f.name, f.value) for f in A.Format])
print(\"   其中 2 是内部用的：公开调用 →\", end=\" \")
try:
    A.get_annotations(probe, format=2)
    print(\"居然成功了（与本章结论不符，请报告）\")
except Exception as e:
    print(f\"{type(e).__name__}: {str(e)[:60]}\")
print(\"【工具在场性】本机标准库里有没有静态检查器 →\", \"mypy\" in sys.stdlib_module_names,
      \"｜ typeguard 是运行时插桩，不是静态检查 →\", __import__(\"typeguard\").__name__ not in sys.stdlib_module_names)
"
【时刻① 定义函数时】注解求值了吗 →
   函数建起来了吗 → True ｜ 此刻读没读过那个名字 → 屏幕上没有 NameError 就是没读
   （a 篇把这条量成了计数器：定义那一遍计数 0）
【时刻② 第一次读注解】谁读谁求值：
   属性读 → NameError
   再读一次 → NameError ｜ 两遍都炸 = 失败不缓存（a 篇计数 1→2 的同一条）
【时刻③ 调用函数时】求值注解了吗 → 屏幕上这行之前没有任何报错，就是没求值
   调用返回 = 0 ｜ 运行时对值查了吗（d 篇八个现场 + 第 11 节矩阵①）→ 一行都不查
【时刻④ 建 dataclass 时】
   c 篇/第 7 节矩阵量过：建类与实例化都不求值注解；忘 import ClassVar 会静默降级成普通字段
【入口】Format 四个成员（名字, 值）→ [('VALUE', 1), ('VALUE_WITH_FAKE_GLOBALS', 2), ('FORWARDREF', 3), ('STRING', 4)]
   其中 2 是内部用的：公开调用 → ValueError: The VALUE_WITH_FAKE_GLOBALS format is for internal use only
【工具在场性】本机标准库里有没有静态检查器 → False ｜ typeguard 是运行时插桩，不是静态检查 → True
[退出码 0]
```

按屏上那六个时刻读：

- **时刻① 定义函数时** —— 函数建起来了（`callable → True`），屏上没有 `NameError` 就是没读。
  `a` 篇把这条量成了计数器（定义那一遍计数 0）。
- **时刻② 第一次读注解** —— 属性读 `NameError`，**再读一次还是 `NameError`**；
  两遍都炸就是「失败不缓存」，跟计数器 1→2 是同一条结论。
- **时刻③ 调用函数时** —— `调用返回 = 0`，而运行时对这个值**一行都不查**（第 08 节八个现场 + 第 11 节矩阵①）。
- **时刻④ 建 dataclass 时** —— 建类与实例化都不求值注解；忘 `import ClassVar` 会**静默降级**成普通字段（第 06、07 节）。
- **入口** —— `Format` 四个成员（名字, 值）→ `[('VALUE', 1), ('VALUE_WITH_FAKE_GLOBALS', 2), ('FORWARDREF', 3), ('STRING', 4)]`，
  其中 2 号在公开入口直接被拦：`ValueError: The VALUE_WITH_FAKE_GLOBALS format is for internal use only`。
- **工具在场性** —— `mypy` 不在标准库名单里（False），`typeguard` 在场但它是运行时插桩、不是静态检查。

结论那一行不是手打的，是同一屏上现算的 —— 下面这条命令把「2 号被拦」再撞一次，
并把撞下来那句原文和 `isinstance(exc, ValueError)` 一起钉成断言：

```
$ python3 -c "
import annotationlib as A
vals = sorted(f.value for f in A.Format)
def try2():
    try:
        A.get_annotations(lambda x: None, format=2)
        return \"没拦住（若看到这句请报告）\", False
    except BaseException as exc:
        return type(exc).__name__ + \": \" + str(exc), isinstance(exc, ValueError)
msg, is_value = try2()
print(\"  → 断言：本节六段读数都由上面那条现场跑出（没有一行手打）；Format 四个值排序 =\", vals,
      \"｜2 号在公开入口确实被拦（这里当场再撞一次）=\", is_value, \"｜拦下时那句原文 →\", msg)
"
  → 断言：本节六段读数都由上面那条现场跑出（没有一行手打）；Format 四个值排序 = [1, 2, 3, 4] ｜2 号在公开入口确实被拦（这里当场再撞一次）= True ｜拦下时那句原文 → ValueError: The VALUE_WITH_FAKE_GLOBALS format is for internal use only
[退出码 0]
```

这一节把全章的因果链摊平给你看：**注解是一段没跑的代码 → 谁读它谁负责求值 → 读的口径决定拿到的是
对象、ForwardRef 还是文本 → 而运行时自己一个都不查。** 第 02~05 节证前半句，
第 06~09、11 节证后半句，第 10、12 节是「那我能拿它做什么」的两条路。

**这一节记住一句话**：「这段代码有没有类型错误」这个问题，运行时不答、dataclass 不答、`isinstance` 只答一半 ——
答它的人必须先是**读**注解的那个人。

---

## 第 14 节 · 自证：沙箱里的事只留在沙箱，章目录一个字都没多

这一节是给读者的验收单，也是给本章自己的。规矩很简单：**任何「干净」的计数，都必须先演示一次「它能响」**，
否则那个 0 只是「看着干净」。

先看沙箱里该有的东西，把「该有什么」写成能判的式子：

```
$ echo "沙箱 work/ 里的 .py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)（这 7 个是章目录副本，不是产物）  非 .py 文件数=$(find . -maxdepth 1 -type f ! -name "*.py" | wc -l)"
沙箱 work/ 里的 .py 个数=7（这 7 个是章目录副本，不是产物）  非 .py 文件数=0
[退出码 0]
↑ 守卫如果不会响，就等于没检查。先故意造四样残留：一个取证脚本同款名字的目录、一个非 .py 文件、
```

然后故意造四样残留 —— 一个取证脚本同款名字的目录、一个非 `.py` 文件、一次真 `import` 产生的 `__pycache__`、
一个矩阵格没删掉的 `.py`，同一套计数必须全部报非 0：

```
$ mkdir -p typeguard-demo; touch 残留.tmp; printf "y = 1\n" > probe_mod.py; printf "z = 2\n" > matrix_leftover.py; python3 -c "import probe_mod"; echo "塞进假产物之后：子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  .py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)  非 .py 文件数=$(find . -maxdepth 1 -type f ! -name "*.py" | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l)  ← 五项都会响"
塞进假产物之后：子目录数=2  .py 个数=9  非 .py 文件数=1  __pycache__ 个数=1  *.pyc 个数=1  ← 五项都会响
[退出码 0]

$ rm -rf typeguard-demo 残留.tmp probe_mod.py matrix_leftover.py __pycache__; echo "清掉之后：子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  .py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)  非 .py 文件数=$(find . -maxdepth 1 -type f ! -name "*.py" | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l)"
清掉之后：子目录数=0  .py 个数=7  非 .py 文件数=0  __pycache__ 个数=0  *.pyc 个数=0
[退出码 0]
```

`塞进假产物之后：子目录数=2  .py 个数=9  非 .py 文件数=1  __pycache__ 个数=1  *.pyc 个数=1` ——
五项都响了，所以后面那排 0 才有意义。清掉之后：

```
$ for d in lazy-demo reads-demo who-demo runtime-demo proto-demo typeguard-demo mini-demo; do printf "%s 还在吗 → %s\n" "$d" "$([ -d "$d" ] && echo 在 || echo 不在)"; done; echo "七个沙箱目录一个都不在，才算这七支脚本自己收好了尾"
lazy-demo 还在吗 → 不在
reads-demo 还在吗 → 不在
who-demo 还在吗 → 不在
runtime-demo 还在吗 → 不在
proto-demo 还在吗 → 不在
typeguard-demo 还在吗 → 不在
mini-demo 还在吗 → 不在
七个沙箱目录一个都不在，才算这七支脚本自己收好了尾
[退出码 0]
```

七个沙箱目录（`lazy-demo`…`mini-demo`）一个都不在，才算七支脚本自己收好了尾。
这里还顺手复演了本章主题：**只有 `import` 会造 `__pycache__`**，而 `python3 名字.py` 这种直跑脚本不产缓存 ——
上面那次「响」就是真 `import` 了一下 `probe_mod` 造成的。

家目录那两处再数一遍，和第 01 节同一格对照。两次一样，就是「demo 没往家目录写过东西」的凭证：

```
$ python3 -c "
import os
def n(p):
    p = os.path.expanduser(p)
    return len(os.listdir(p)) if os.path.isdir(p) else \"目录不存在\"
print(\"~/.local/lib 条目数 =\", n(\"~/.local/lib\"), \"｜ ~/.config 条目数 =\", n(\"~/.config\"), \"｜ ~/.bashrc 存在吗 →\", os.path.isfile(os.path.expanduser(\"~/.bashrc\")))
"
~/.local/lib 条目数 = 目录不存在 ｜ ~/.config 条目数 = 31 ｜ ~/.bashrc 存在吗 → True
[退出码 0]
★ 章目录（源文件本体在这里，demo 全程只读它）。清单显式排除 README.md，
```

章目录（源文件本体在这里，demo 全程只读它）的清单**显式排除 `README.md`** ——
所以「README 写完之后重跑」得到的还是同一份输出，这一节不会自我打脸：

```
$ 〔在 10-typing-and-tools 里〕 echo ".py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)  demo.sh 个数=$(find . -maxdepth 1 -name "demo.sh" | wc -l)  README 以外的其它文件数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l)  子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)"
.py 个数=7  demo.sh 个数=1  README 以外的其它文件数=0  子目录数=0  __pycache__ 个数=0
[退出码 0]
```

接下来是本章最硬的一格：扫**整份屏幕账**（`all.out`）。四类能写成精确串的身份用 `str.count`，
只有形态没有固定串的（对象地址）用正则 —— 因为正则会把自己那条命令行也算进去，那是假警报。
关键在顺序：**先让这套扫描响一次**（往账的副本里塞五行当场取出来的真身份，命令行里一个字都没写真身份），
报非 0 才说明扫描是活的：

```
$ cd .. && python3 -c "
import os, re, socket
sb = os.environ[\"SB\"]
exact = [(\"家目录绝对路径\", os.path.expanduser(\"~\")), (\"用户名\", os.environ.get(\"USER\", \"\")),
         (\"主机名\", socket.gethostname()), (\"本次沙箱路径\", sb)]
labels = [k for k, _ in exact] + [\"对象地址形态\"]
with open(\"poison.out\", \"w\", encoding=\"utf-8\") as fh:
    fh.write(open(\"all.out\", encoding=\"utf-8\").read())
    for _, v in exact:
        fh.write(v + chr(10))
    fh.write(repr(object()) + chr(10))
def scan(t):
    hits = [t.count(v) for _, v in exact]
    hits.append(len(re.findall(\"0x[0-9a-f][0-9a-f]+\", t)))
    return dict(zip(labels, hits))
poison = scan(open(\"poison.out\", encoding=\"utf-8\").read())
screen_text = open(\"all.out\", encoding=\"utf-8\").read()
screen = scan(screen_text)
print(\"  │ ① 故意污染过的那份副本（五行真身份）→\", poison)
print(\"  │ ② 真正的屏幕账 all.out →\", screen)
print(\"  → 断言：污染那份五项全非 0（扫描会响）=\", all(v > 0 for v in poison.values()),
      \"｜屏幕账那五项全 0（扫描干净）=\", all(v == 0 for v in screen.values()),
      \"｜本机用户名与主机名同一个串（所以两格数字相同不是重扫）=\",
      os.environ.get(\"USER\", \"\") == socket.gethostname())
"
  │ ① 故意污染过的那份副本（五行真身份）→ {'家目录绝对路径': 1, '用户名': 3, '主机名': 3, '本次沙箱路径': 1, '对象地址形态': 1}
  │ ② 真正的屏幕账 all.out → {'家目录绝对路径': 0, '用户名': 0, '主机名': 0, '本次沙箱路径': 0, '对象地址形态': 0}
  → 断言：污染那份五项全非 0（扫描会响）= True ｜屏幕账那五项全 0（扫描干净）= True ｜本机用户名与主机名同一个串（所以两格数字相同不是重扫）= True
[退出码 0]
```

`①` 那行五项全非 0，`②` 那行（真正的屏幕账）五项全 0，收口的 `→ 断言：` 把两件事和
「本机用户名与主机名是同一个串」一起判掉了。这一格里没有任何一条身份是手打的：
家目录、用户名、主机名、沙箱路径全部**当场取值**再 `count`，所以它既不泄漏、又能响。

下一格把「形状」和「真名」分开量：模板串 `py10-XXXXXX` 是写在正文里给人看的，永远不等于本次真名；
真名只出现在 `--keep` 那句提示里，而那句只进终端、不进账。

```
$ cd .. && rm -f poison.out && python3 -c "
import os
t = open(\"all.out\", encoding=\"utf-8\", errors=\"replace\").read()
shape = \"py10-\" + \"X\" * 6
print(\"  │ ③ 污染副本删干净了吗 →\", not os.path.exists(\"poison.out\"))
print(\"  │ ④ 形状与真名分开量：账里模板字样\", shape, \"出现\", t.count(shape),
      \"次（给人看的形状，允许）｜本次真沙箱名出现\", t.count(os.environ[\"SB\"]), \"次（必须是 0）\")
" && echo "屏幕账行数=$(wc -l < all.out)  沙箱根目录的文件数=$(find . -maxdepth 1 -type f | wc -l)"
  │ ③ 污染副本删干净了吗 → True
  │ ④ 形状与真名分开量：账里模板字样 py10-XXXXXX 出现 0 次（给人看的形状，允许）｜本次真沙箱名出现 0 次（必须是 0）
屏幕账行数=1473  沙箱根目录的文件数=1
[退出码 0]
```

最后是判据总账。这里有一个本章反复踩的坑，被当成现场演了一遍：`runc` 会把**每条命令和它的输出**都抄进同一份账，
所以「扫账的会扫到自己」。数「七支脚本各一条合计行」必须抢在复述之前数：

```
$ cd .. && python3 -c "
import re
pat = re.compile(\"全篇判据合计 ([0-9]+)/([0-9]+) 条成立\")
lines = list(open(\"all.out\", encoding=\"utf-8\"))
rows = [m.groups() for l in lines for m in [pat.search(l)] if m]
bad = sum(1 for l in lines if l.strip().startswith(\"[不成立！]\"))
print(\"  │ ① 此刻账里抓到几条合计行 →\", len(rows), \"（该是 7：七支脚本各一条，下面那条 grep 还没复写它们）｜ 逐条（成立/总数）→\", rows)
print(\"  │ ② 全章成立条数 =\", sum(int(a) for a, b in rows), \"｜ 总条数 =\", sum(int(b) for a, b in rows))
print(\"  → 断言：七支脚本各一条 =\", len(rows) == 7, \"｜ 每条分子都等于分母 =\",
      all(a == b for a, b in rows), \"｜ 失配行（行首那种格式）条数 =\", bad)
"
  │ ① 此刻账里抓到几条合计行 → 7 （该是 7：七支脚本各一条，下面那条 grep 还没复写它们）｜ 逐条（成立/总数）→ [('11', '11'), ('10', '10'), ('45', '45'), ('52', '52'), ('60', '60'), ('28', '28'), ('27', '27')]
  │ ② 全章成立条数 = 233 ｜ 总条数 = 233
  → 断言：七支脚本各一条 = True ｜ 每条分子都等于分母 = True ｜ 失配行（行首那种格式）条数 = 0
[退出码 0]
```

`①` 抓到 7 条（七支脚本各一条）、`②` 全章成立条数 = 总条数 = 233，断言行三条判据全 True。
紧接着下面两条把「翻倍」这件事也数出来，不假装没这回事：先 `grep -hoE` 把七条原样列出（它的输出也进了账），
再同一套式子数一遍 —— 那里就会出现 14。最后那格守住「失配」的判据：
提示语「不成立的那条上面会印 [不成立！]」的计数只是文案计数，
**真正的判定是行首那种格式的失配行数，它必须是 0**。

```
$ cd .. && echo "带「不成立的那条上面会印 [不成立！]」这句提示的行数=$(grep -cF "上面会印 [不成立！]" all.out)  ← 它是提示语的计数，不是判定"; echo "真正的失配行数=$(grep -cE "^[[:space:]]+\\[不成立！\\]" all.out)  ← 这个才必须是 0"; grep -E "^[[:space:]]+\\[不成立" all.out | head -3; echo "（上面那条 grep 打不出任何行，才是真的 0）"
带「不成立的那条上面会印 [不成立！]」这句提示的行数=47  ← 它是提示语的计数，不是判定
真正的失配行数=0  ← 这个才必须是 0
（上面那条 grep 打不出任何行，才是真的 0）
```

**这一节记住一句话**：任何「0」都要先有一次「非 0」证明它会响 —— 章目录、家目录、屏幕账三处干净，
是靠故意污染一次再清干净换来的，不是靠一句「应该没问题」。

---

## 自检问题

1. `def f(x: Widget) -> Result`，两个名字都不存在。这行会报错吗？什么时候报？
   —— 不会。定义那一刻只编译出 `__annotate__`（第 02 节第 1 格）。第一次**读** `f.__annotations__` 才 `NameError`。
2. `hasattr(f, '__annotations__')` 安全吗？
   —— 不安全。它只吞 `AttributeError`，注解求值抛的 `NameError` 照原样往上抛（第 02 节第 1 格⑤、核对格③）。
3. 读注解读到一次成功，能保证下次不炸吗？
   —— 不能。成功才缓存、失败不缓存；负对照那支计数器是 0 → 1 → 2（第 02 节第 2 格）。
4. 想用「绝对不求值」的方式拿到注解源码文本，走哪个口径？代价是什么？
   —— `annotationlib.get_annotations(obj, format=Format.STRING)`（第 4 格⑨ 是四把钥匙的编号牌）。
      代价是它把表达式**又跑了一遍**，注解里有副作用就别用它（第 02 节第 2 格⑧）。
5. `x: "SomeType"` 加了引号，是不是就延迟求值了？
   —— 不是。存进去的就是那个 `str`，`get_type_hints`、`eval_str=True`、你自己 `eval` 都会把它炸回来（第 02 节第 5 格）。
6. `from __future__ import annotations` 和不加，下游差在哪？
   —— 差在**能不能拿到真对象**：开了之后整份文件的注解全是文本（第 03 节四行读数）。安全性一点没变。
7. dataclass 建类时为什么不因为坏注解炸？那谁炸？
   —— 它读的不是求值那份（第 06 节第 1 格 + 第 07 节矩阵）；后来去读类注解的人（`get_type_hints`、`inspect.signature`）才炸。
8. `x: Final = 3` 之后再赋值会怎样？`isinstance(x, SomeProtocol)` 查什么？
   —— 前者毫无反应（第 08 节第 3 格），后者只查方法名存不存在、一个签名都不查（第 09 节第 1、2 格）。
9. `typeguard` 能代替 mypy 吗？它默认查多深？
   —— 不能，它是运行时插桩，只覆盖它插得到的调用点；默认策略只查容器第一个元素（第 10 节第 4、5 格）。
10. 没有 mypy，你自己能静态查点什么？查不了什么？
    —— 能查「注解里的名字源码里有没有弄出来」，一次给全部、不执行也能查（第 12 节第 4、9 格）；
    查不了属性存在性、值与类型匹不匹配（第 12 节第 5 格、第 10 格）。

## 本章没演 / 没验证（绝不编造，都在这）

- **旧版本对照没演**：本机只有 `3.14.4`，没有第二台解释器，所以「更早版本上读 `__annotations__` 的行为」
  本章一条都没写。你在别的版本上跑，请重跑第 01 节那两格布尔再决定本章哪几条能搬过去。
- **mypy / pyright / ruff 的真实输出没演**：八把尺全不在场（第 01 节实测），连 `pip` 都没有，
  所以本章没引用过任何一句它们的报错原文。第 12 节那把 `ast` 尺是本机现搓的，**不是** mypy 的替身。
- **`Any == object`、`Optional[int] == int | None` 没验过**：这两条探测没跑完，
  所以本章正文一次都没用它们（用之前谁都得自己跑出来）。
- **耗时/性能一律没测**：`timeit` 在场但本章一格都没用 —— 惰性求值省掉了多少、
  STRING 口径重跑表达式慢不慢，屏幕上一个数字都没有，也不许在正文里估。
- **框架层没演**：`pydantic`、`attrs`、FastAPI 这类「靠读注解干活」的库本机没有，
  第 06 节只演了标准库里的 dataclass / NamedTuple / TypedDict。
- **`type` 语句那种较新的类型别名语法、以及 `NewType` 的运行时形状没演**：本章只用 `TypeVar` 老写法（第 09 节第 7 格）。
- **`install_import_hook` 只演了「装钩之后才加载的模块」那半边**：已加载模块不补钩（第 10 节第 9 格），
  包内子模块、命名空间包上的更细边界没测。
- **`__future__` 开关会不会在未来变成默认没验证**：这是路线问题，不是本机事实，本章不预测。
- **子进程之外的真实工程场景没演**：CI 里怎么在没有类型检查器时先跑一把 `ast` 尺、
  怎么把 4 号口径的文本存进文件再读回来 —— 后者在第 09 节脚本内部演过一次，本章没做成完整工作流。

## 怎么跑这一章

```
cd python-handbook/10-typing-and-tools
bash demo.sh                  # 14 节全跑，跑完自动删沙箱
bash demo.sh --keep           # 跑完把沙箱留着，屏幕最后一行打出它的路径，进去自己接着敲
```

想单点复现某一节，看标题就知道去哪找：

- 标着「**x 篇完整跑**」的节（第 2、4、6、8、9、10、12 节）= 跑一个 `.py` 脚本，
  例如 `python3 b_four_reads.py`，脚本自己在当前目录造一个 `xxx-demo/` 子目录、自己收尾。
- 标着「**现场格**」「**矩阵格**」的节（第 3、5、7、11 节）= 一段 `python3 -c` 加几条 shell，
  围栏里连命令一起贴全了，可以直接抄着敲。
- 第 13 节是把七篇合成一张合并视图的跨篇小结，第 14 节是本章给自己的对账。

自己敲单支脚本时，请先把它拷进一个 `mktemp -d` 出来的临时目录再跑：

```
SB=$(mktemp -d /tmp/py10chk-XXXXXX)
cp python-handbook/10-typing-and-tools/b_four_reads.py "$SB"/
cd "$SB" && python3 b_four_reads.py
```

★ 别在章目录里直接 `python3 某支.py`：每支脚本都在**当前目录**自建一个 `xxx-demo/` 沙箱，
正常收尾会自己删掉（第 14 节那七行「还在吗 → 不在」就是这件事的判据），
但中途 `Ctrl-C` 就会把残骸留在章目录里。拷进临时目录再跑，炸了也只是炸一个临时目录。

三件事先记住，否则读数会跟本章对不上：
① 必须在 `3.14` 上跑，`annotationlib` 不在场的话本章大部分格子直接 `ImportError`；
② `typeguard` 不在场时，第 10 节整篇会退化成「在场性探测报一个 False」，其余六篇不受影响；
③ 报错原文跟版本绑定，本章屏上那些逐字引用的 `NameError:` / `ValueError:` 句子只在本机 `3.14.4` 验过。

本章的每条引文都由脚本从定稿捕获按行号搬出（正文里只写 `@@R 起-止@@` 占位符，
再用 `expand_fences.py` 换成逐字围栏），交付闸门是一条命令：

```
TAIL_TITLES='自检问题|本章没演|怎么跑这一章|术语小抄|本章之后|本章回指' \
bash ~/.local/bin/chapter_gate.sh README.md ~/.cache/handbook-captures/py10-run1.txt \
     '/tmp/py10-[A-Za-z0-9]{6}'
```

它判六件事：占位符残留 0、围栏成对、含提示符的行逐字对上捕获、无真实身份、片段与 README 同步、收尾节齐。
最后那件要另传片段文件才跑；正则前面带 `/tmp/` 是因为正文里就写着沙箱名**模板** `py10-XXXXXX`（第 14 节那一格），
不带前缀它会把自己写的这句话当成泄漏 —— 这是「未匹配先怀疑沙箱正则，再怀疑书」的一条现成例子。
`py10-final.txt` 是同一份捕获在开头加了一块说明（跑法、两遍 diff、八个文件的 md5）的存档版，正文引用的行号一律指不带头的 `py10-run1.txt`。


## 术语小抄

| 词 | 本章里它指什么 | 在哪一格量过 |
|---|---|---|
| 惰性注解 | 注解在写下那一刻不执行，编译成一个 `__annotate__` 取值函数 | 第 02 节第 1、4 格 |
| `__annotate__` | 挂在函数/类/模块上的那个函数；`__annotate__(1)`/`(2)` 有编译版，`(3)`/`(4)` 只给空消息的 `NotImplementedError` | 第 02 节第 4 格 |
| `annotationlib.Format` | 读注解的四个口径：1 VALUE、2 VALUE_WITH_FAKE_GLOBALS（公开入口直接拦）、3 FORWARDREF、4 STRING | 第 04 节第 1、2 格 |
| `ForwardRef` | 3 号口径给的那个「名字先包起来」的对象；算 owner 相等所以不能当键，求值要自己给环境 | 第 04 节第 3、4 格 |
| `__conditional_annotations__` | 模块级注解语句在 globals 里留下的开关（`{0, 1}`），future 的作用落点 | 第 02 节第 9 格⑫、第 04 节第 10 格 |
| `if TYPE_CHECKING:` | 运行时不执行、静态工具恒真的一段分支 —— 省的是那次 import | 第 06 节第 5 格、第 12 节第 2、3 格 |
| `runtime_checkable` | 让 `Protocol` 能被 `isinstance` 用的装饰器；它只让查方法名 | 第 09 节第 1、2、4 格 |
| `get_origin` / `get_args` | 标准库里真把注解拆开看的那对函数（`list[int]` → `list` / `(int,)`） | 第 09 节第 5、6 格 |
| `install_import_hook` | 给整个包自动加 typeguard 检查；只管**装钩之后**才加载的模块 | 第 10 节第 9 格 |
| `ast` / `symtable` | 源码层两把尺：前者能看注解里的名字，后者**不把注解当使用**（`referenced=False`） | 第 12 节第 1、8 格 |
| `mask()` / `fold()` | 本章的脱敏函数：地址折成 `<地址>`、家目录折成 `<家目录>`、安装前缀折成 `<pyroot>` | 第 03 节收尾、第 14 节 |

## 本章之后

第 11 章讲子进程与并发 —— 本章有两处「只能开子进程才能测」的现场（`from __future__` 那份文件级开关、
`install_import_hook` 那种只管后加载模块的钩子），那一章会把这类「一个进程里摆不下两份环境」的问题正式解决。
第 12 章是速查：四个读入口的口径表、`isinstance` 家族能吃什么的清单、以及一把 `ast` 尺的骨架，都会收成一张能打印的纸。

## 本章回指

- **第 05 章（函数与坑）**：「默认值在 `def` 那一刻冻住」是本章「注解在 `def` 那一刻**不**求值」的兄弟命题，
  第 02 节第 6 格把两者分开：签名编译出 `__annotate__`，函数体里的 `loc: T = 1` 连 `__annotate__` 都没有。
- **第 06 章（类与 dataclass）**：本章第 06、07 节回答的是「dataclass 到底拿注解干什么」，
  忘 `import ClassVar` 会静默降级成普通字段这条，是第 06 章那条 dataclass 规矩的反面补完。
- **第 07 章（异常与 with）**：本章每格那句「报错原文逐字」都靠 `except` 接住再打印 ——
  `hasattr` 只吞 `AttributeError`（第 02 节第 1 格⑤）用的就是那一章的异常家谱。
- **第 08 章（文件与 pathlib）**：`typeguard` 靠读源码工作（第 10 节第 7 格：`-c` 里当场炸、写成文件才拦），
  前提就是「源码在不在磁盘上」这一章的内容。
- **第 09 章（模块与 venv）**：`__annotations__` 是模块 `__dict__` 里由解释器**在成功那一刻**塞进去的键
  （第 02 节第 9 格），和那一章 `__builtins__` 同一套路；`sys.path` 决定 `typeguard` 从哪条 `dist-packages` 来（第 01 节）。
- **bash 手册第 09 章（出事怎么查）**：本章第 14 节「先让守卫响一次再报 0」的纪律直接沿用那一章的排查思路。
