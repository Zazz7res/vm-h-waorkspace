#!/usr/bin/env python3
# ============================================================================
# 取证脚本 a_lazy_annotations.py —— 第 10 章第 1 篇：注解是一段「还没执行的代码」
#
# 要证明什么：`def f(x: Widget) -> Result` 这一行里，`Widget` 和 `Result` 一个都没被求值。
# 解释器在 def 的那一刻只做了一件事：把「怎么求这些表达式」编译成一个藏在函数对象上的
# 私有函数 `__annotate__`。注解表达式真正跑起来，是**你第一次读它的那一刻** ——
# 本脚本把这条时间线撞穿九次：
#   ① 定义成功、调用正常，名字不存在也不拦你；② 读 `__annotations__` 才炸 NameError；
#   ③ 求值**成功**才缓存（连读两遍，注解表达式只跑一遍，两次拿到同一个 dict 对象），
#      求值**失败**不缓存（读两遍，表达式跑两遍，计数 1→2）；
#   ④ 那个 dict 是活的，改得动、加了键也留得下来，但 get_annotations 给你的是一份拷贝；
#   ⑤ `__annotate__` 里只编了 1、2 两个口径，3、4 直接调是 NotImplementedError 且消息是空串；
#   ⑥ 加引号不是延迟，只是存了个 str，get_type_hints 照样炸；
#   ⑦⑧⑨ 函数体里的局部注解 / 类体里的裸注解 / 模块级的裸注解：三种地方三种下场。
# 每格的判据都用「计数器」「两次读数是不是同一个对象 is」「报错逐字原文」这种硬东西，
# 并且第 2、9 格做成了负对照（先让判据真的响一次，再报真实值）——
# 因为「无论对错都印同一句话」的断言等于没断言。
#
# 为什么要有它：后面几篇（四种读法 / typeguard / 自制检查器）全都站在这块地基上：
# 「注解什么时候求值、求几次、谁能看见」。这一篇没钉死，
# 「为什么检查器报的错我跑起来一点事没有」和「为什么我读个注解把程序读崩了」都没法回答。
# 三个最常见错觉在这一篇一次打掉：「注解是运行时检查」（连名字存不存在都不查）、
# 「加了引号就安全了」（只是存了个 str）、「读一次失败就记下了，不会再炸」（失败不缓存）。
#
# 现场：只在当前工作目录现造 lazy-demo/（两个真模块 + 一个读它们的子进程脚本），
# 结尾整目录删掉，章目录零产物。屏幕上不出现绝对路径 / 对象地址 / 时间 / PID：
# 家目录前缀折成 <家目录>、当前目录前缀去掉、`0x…` 折成 <地址>；traceback 一律不印，
# 只印 `异常类型: 消息` 这一行原文。
# 子进程一律 [sys.executable, ...] + cwd=lazy-demo + 干净 env（只给 LC_ALL/LANG/PYTHONIOENCODING，
# 绝不继承 PYTHONPATH / PYTHONHOME）。退出码恒为 0：所有报错都被接住当成证据打印。
#
# 本机 Python 3.14.4 / Ubuntu 26.04（CPython）。以下本机未验证，别当事实引用：
#   - mypy / pyright 等静态检查器的任何行为（本机没装，`import mypy` 直接 ModuleNotFoundError）；
#   - 3.7~3.13 的注解求值时机差异（`from __future__ import annotations` 是老写法，见第 b 篇）；
#   - PyPy 等其他实现有没有 `__annotate__`（这是 CPython 3.14 的实现细节，不是语言规范）；
#   - Windows 上的路径与编码表现。
# ============================================================================
import os
import re
import shutil
import subprocess
import sys
import typing
from pathlib import Path

import annotationlib

WORK = Path("lazy-demo")
CWD = Path.cwd().resolve()
HOME = os.path.expanduser("~")
PYNAME = os.path.basename(sys.executable)
Format = annotationlib.Format

# ---- 模块级注解语句：第 8 格的取证对象。这两行就这么光天化日地放着，脚本跑完全程 ----
Y: SomeUndefinedName          # 裸注解语句：不绑任何名字，求值的时候才去查 SomeUndefinedName
Z: AnotherUndefined = 1       # 带值的注解语句：赋值照做（Z 确实是 1），注解那半句没跑


def mask(text):
    """把不能上屏的东西折掉：绝对路径（家目录/沙箱/安装目录）、对象地址 0x…。"""
    out = str(text)
    out = out.replace(sys.executable, PYNAME)
    for root, tag in ((str(CWD) + os.sep, ""), (str(CWD), "<沙箱>"),
                      (HOME + os.sep, "<家目录>" + os.sep), (HOME, "<家目录>"),
                      (sys.base_prefix + os.sep, "<python安装目录>" + os.sep)):
        if root and root != os.sep:
            out = out.replace(root, tag)
    return re.sub(r"0x[0-9a-fA-F]+", "<地址>", out)


def line(tag, thunk):
    """本脚本唯一的取证通道：读一个可能炸的表达式并当场打印。

    成功印 `OK → repr(值)`，失败印 `异常类型: 原文`（消息是空串时补一句 repr，
    免得读屏的人以为漏印了）—— 报错原文和正常读数走同一种排版，
    并且一律先经过 mask()，所以绝对路径和对象地址不可能漏到屏幕上。
    """
    try:
        body = "OK → " + repr(thunk())
    except BaseException as exc:            # noqa: BLE001 —— 报错本身就是这一格的证据
        head = f"{type(exc).__name__}: {exc}"
        if str(exc) == "":
            head += f"（str(exc) 的 repr = {str(exc)!r}，即空串）"
        body = head
    print(f"  │ {tag} {mask(body)}")


CELL = {"n": 0, "ok": 0}      # 本格判据数：只有第 10 格（核对格）用得到
TOTAL = {"n": 0, "ok": 0}     # 全篇判据总数 / 成立数（收尾那条合计行用它）


def fail(thunk):
    """跑一下，炸了就返回（异常类型名, str(异常)）；没炸返回 None。

    第 10 格要判的是「报错原文逐字相同」这类事，所以把异常摊成一个可比较的二元组。
    """
    try:
        thunk()
    except BaseException as exc:                     # noqa: BLE001 —— 报错本身就是证据
        return (type(exc).__name__, mask(str(exc)))
    return None


def eq(tag, thunk, want):
    """判据行：实测与预期逐字比对，两边都印在屏上（比对失败会印 [不成立！]）。"""
    got = thunk()
    ok = got == want
    CELL["n"] += 1
    CELL["ok"] += 1 if ok else 0
    TOTAL["n"] += 1
    TOTAL["ok"] += 1 if ok else 0
    return f"  [{'成立' if ok else '不成立！'}] {tag}：实测 {got!r} ｜ 预期 {want!r}"


def ledger(title):
    return (f"  → 断言：{title} ｜ 本格判据 {CELL['ok']}/{CELL['n']} 条成立"
            f"（不成立的那条上面会印 [不成立！]）")


# ===================== 现场：两个真模块 + 一个读它们的子进程脚本 =====================
# 第 9 格必须在「一个真·被 import 的模块」里量：模块级裸注解在模块内部按名字查不到，
# 但**属性读成功之后**同一个裸名就能查到了。这个翻转动作在本脚本自己的 __main__ 里做不出来
# （这里 Y 的名字故意不存在，属性读永远不成功），所以现场造一个注解全合法的 lazy_ok.py。
MODULES = {
    "lazy_ok.py": '"""注解里的名字全存在：属性读能成功，于是能看见缓存塞键那一刻。"""\n'
                  "W: int\n"
                  "NAMES: list[str] = []\n"
                  "\n"
                  "def peek_bare():\n"
                  "    try:\n"
                  "        return ('读到了', __annotations__)\n"
                  "    except NameError as exc:\n"
                  "        return ('NameError', str(exc))\n",
    "lazy_bad.py": '"""注解里的名字不存在：属性读也炸，而且炸的是注解里那个名字。"""\n'
                   "K: Ghost\n"
                   "M: int = 5\n",
    "r_module.py": "import annotationlib as A\n"
                   "import lazy_bad\n"
                   "import lazy_ok\n"
                   "print('① import 两个模块都成功（模块体里就写着不存在的注解名）→', "
                   "[lazy_ok.__name__, lazy_bad.__name__])\n"
                   "print(\"② 读之前 vars(lazy_ok) 里有 '__annotations__' 这个键吗 →\", "
                   "\"__annotations__\" in vars(lazy_ok))\n"
                   "print('③ 模块自己内部的裸名读法 lazy_ok.peek_bare() →', lazy_ok.peek_bare())\n"
                   "print('④ 属性读 lazy_ok.__annotations__ →', lazy_ok.__annotations__)\n"
                   "print('⑤ 读之后再问一次裸名 lazy_ok.peek_bare() →', lazy_ok.peek_bare())\n"
                   "print(\"⑥ 这次 vars(lazy_ok) 里有那个键了吗 →\", "
                   "\"__annotations__\" in vars(lazy_ok))\n"
                   "print('⑦ ②④⑥ 三行连起来：键是 ④ 那一刻由解释器写进模块 __dict__ 的')\n"
                   "try:\n"
                   "    lazy_bad.__annotations__\n"
                   "except NameError as exc:\n"
                   "    print('⑧ lazy_bad 属性读 →', type(exc).__name__ + ': ' + str(exc))\n"
                   "print(\"⑨ 失败不会把键留下：vars(lazy_bad) 里有 '__annotations__' 吗 →\", "
                   "\"__annotations__\" in vars(lazy_bad))\n"
                   "print('⑩ STRING 口径读 lazy_bad →', A.get_annotations(lazy_bad, format=A.Format.STRING))\n"
                   "print(\"⑪ 'K' in vars(lazy_bad) →\", \"K\" in vars(lazy_bad), '｜ 带值的 M =', lazy_bad.M)\n"
                   "print('⑫ 两个模块的 __conditional_annotations__ →', "
                   "vars(lazy_ok).get('__conditional_annotations__'), "
                   "vars(lazy_bad).get('__conditional_annotations__'))\n",
}

shutil.rmtree(WORK, ignore_errors=True)     # 上次的残骸先清掉，两遍跑出同一个现场
for _name, _content in MODULES.items():
    _path = WORK / _name
    _path.parent.mkdir(parents=True, exist_ok=True)
    _path.write_text(_content, encoding="utf-8")

print("=== 1. `def` 那一行什么都没查：名字不存在也照样定义成功 ===")
print("  取证对象：`def f(x: Widget) -> Result` —— Widget、Result 在本文件里从来没有出现过。")


def f(x: Widget) -> Result:
    return x


print("  上面那行 def 已经过去了（脚本能跑到这里就是证据），接着量它身上有什么：")
line("① f 本身：名字 / 调用 f(3) / 返回值类型 →",
     lambda: (f.__name__, f(3), type(f(3)).__name__))
line("② 函数对象上有 __annotate__ 吗 / 它的类型 →",
     lambda: (hasattr(f, "__annotate__"), type(f.__annotate__).__name__))
line("③ __annotate__ 出自哪个文件（只打 basename）→",
     lambda: os.path.basename(f.__annotate__.__code__.co_filename))
line("④ 第一次读 f.__annotations__ →", lambda: f.__annotations__)
line("⑤ 换个问法 hasattr(f, '__annotations__') →", lambda: hasattr(f, "__annotations__"))
line("⑥ 第二遍读 f.__annotations__ →", lambda: f.__annotations__)
print("  → ④⑥ 是同一句话，而且**每次读都炸**：失败不会被记下来（第 2 格拿计数器证明）。")
print("  → ⑤ 这条最容易记错：hasattr 只吞 AttributeError，NameError 照原样往上抛 ——")
print("     所以「用 hasattr 试探有没有注解」在 3.14 会把程序带崩，运行时读注解得用 try/except。")
print("  → 断言：def 那行零副作用（① 里 f(3) 的读数 = 3、类型 = int）；④⑤⑥ 三条原文逐字相同 = "
      "NameError: name 'Widget' is not defined；② 的类型 = function。")

print("\n=== 2. 求值「成功才缓存」，失败「不缓存」：拿计数器量（本格的负对照在前） ===")
print("  判据仪器：注解表达式写成自定义类的下标 `Boom[int]`，它每被求值一次就把 hits +1。")
print("  先跑负对照 —— 如果这台仪器数不出差别，下面所有读数都不可信。")


class BoomFail:
    """注解表达式自己会炸：每求值一次 hits +1 并 raise，用来量「失败会不会被缓存」。"""
    hits = 0

    def __class_getitem__(cls, item):
        BoomFail.hits += 1
        raise RuntimeError("注解表达式自己炸了")


class Boom:
    """注解表达式会成功：每求值一次 hits +1 并返回类本身，用来量「成功会不会被缓存」。"""
    hits = 0

    def __class_getitem__(cls, item):
        Boom.hits += 1
        return cls


def bad_fn(x: BoomFail[int]) -> None:
    pass


def ok_fn(x: Boom[int]) -> None:
    pass


line("负对照① bad_fn 定义完之后 BoomFail.hits →", lambda: BoomFail.hits)
line("负对照② 读 bad_fn.__annotations__ →", lambda: bad_fn.__annotations__)
line("负对照③ 读一遍之后 hits →", lambda: BoomFail.hits)
line("负对照④ 再读一遍 bad_fn.__annotations__ →", lambda: bad_fn.__annotations__)
line("负对照⑤ 再读一遍之后 hits →", lambda: BoomFail.hits)
print("  ↑ 计数从 1 走到 2：这台仪器真的数得出来，所以「读两遍只涨一格」是有意义的读数，")
print("     不是因为「读不到差别所以两次都一样」。")
line("真实值① ok_fn 定义完之后 Boom.hits →", lambda: Boom.hits)
first = ok_fn.__annotations__
line("真实值② 读 ok_fn.__annotations__ →", lambda: first)
line("真实值③ 读一遍之后 hits →", lambda: Boom.hits)
second = ok_fn.__annotations__
line("真实值④ 再读一遍之后 hits →", lambda: Boom.hits)
line("真实值⑤ 两次拿到的是同一个 dict 对象吗（is）→", lambda: first is second)
third = annotationlib.get_annotations(ok_fn, format=Format.VALUE)
line("真实值⑥ get_annotations(VALUE) 读完之后 hits →", lambda: Boom.hits)
line("真实值⑦ 它给的是同一个 dict 对象吗（is）→", lambda: third is first)
stringified = annotationlib.get_annotations(ok_fn, format=Format.STRING)
line("真实值⑧ get_annotations(STRING) 读完之后 hits →", lambda: Boom.hits)
line("真实值⑨ STRING 给的东西 →", lambda: stringified)
print("  → ⑤ True + ③ 只涨到 1：成功那一次把 dict 缓存在函数对象上，之后每次读都复用同一个对象。")
print("  → ⑥⑦ 合起来很阴：get_annotations(VALUE) 不重跑表达式（hits 不动），但它给的是")
print("     一份**新拷贝**（is 为 False）—— 所以改它不会污染函数本体（第 3 格分别改给你看）。")
print("  → ⑧ 是这一格最贵的发现：STRING 口径把表达式**又跑了一遍**（hits 再 +1）。")
print("     「转成字符串」不是查缓存，是把注解表达式在替身环境里重新走一遍 ——")
print("     注解里放有副作用的表达式时，每读一次 STRING 就多跑一次副作用。")
print("  → 断言：负对照那支 hits 逐字 = 0 → 1 → 2（两次的报错原文都是 "
      "RuntimeError: 注解表达式自己炸了）；真实值那支 hits 逐字 = 0 → 1 → 1 → 1 → 2，"
      "并且 ⑤ = True、⑦ = False、⑨ = {'x': 'Boom[int]', 'return': 'None'}。")

print("\n=== 3. 缓存里那个 dict 是活的：改得动，改完再读就是新值（但拷贝改不动本体）===")
cache = ok_fn.__annotations__
line("① 改之前 →", lambda: dict(cache))
cache["x"] = int
line("② cache['x'] = int 之后再读 ok_fn.__annotations__ →", lambda: ok_fn.__annotations__)
line("③ 它和 cache 是同一个对象吗（is）→", lambda: ok_fn.__annotations__ is cache)
cache["zz"] = list[str]
line("④ 往里面加一个**代码里没写过的键** zz，再读的键表（排序）→", lambda: sorted(ok_fn.__annotations__))
copy = annotationlib.get_annotations(ok_fn, format=Format.VALUE)
line("⑤ 拷一份 copy = get_annotations(VALUE) →", lambda: copy)
copy["x"] = str
line("⑥ 把 copy['x'] 改成 str，函数本体那份的 x 是谁 →", lambda: ok_fn.__annotations__["x"])
line("⑦ 改拷贝之后再读 hits →", lambda: Boom.hits)
del cache["zz"]
cache["x"] = Boom
line("⑧ 收拾现场（删 zz、x 改回 Boom）→", lambda: dict(ok_fn.__annotations__))
print("  → ②③ 合起来：`f.__annotations__` 不是每次现造的快照，它就是缓存里那个对象本体，")
print("     拿到它的人改它 = 改这个函数的注解（第 09 章「模块属性就是全局状态」的注解版）。")
print("  → ④：加一个新键也留得下来 —— 所以运行时读到的注解 dict 里可能有代码里没写过的键；")
print("     反过来也成立：有人能在你不改源码的前提下往注解里塞东西（框架就是这么干的）。")
print("  → ⑤⑥⑦：get_annotations 给的拷贝改不动本体（⑥ 读到的还是 int），")
print("     它也不重跑表达式（⑦ 的 hits 和第 2 格末尾一样）。")
print("  → 判据：只想读就 `dict(obj.__annotations__)` 拷一份；想改注解就直接改那个 dict，")
print("     但要知道这是进程级、永久的改动（没有自动还原，本脚本用 ⑧ 手动还原）。")
print("  → 断言：② 的读数逐字 = {'x': <class 'int'>, 'return': None}、③ = True、"
      "④ 的键表逐字 = ['return', 'x', 'zz']、⑥ = <class 'int'>（拷贝没污染本体）、"
      "⑧ 回到 {'x': <class '__main__.Boom'>, 'return': None}。")

print("\n=== 4. `__annotate__` 里只编了 1 号和 2 号口径，3、4 号一调就 NotImplementedError ===")


def h(x: int, y: str = "s") -> None:
    pass


line("① h.__annotate__() 不传参数 →", lambda: h.__annotate__())
for value in (1, 2, 3, 4):
    line(f"② h.__annotate__({value}) →", lambda v=value: h.__annotate__(v))
line("③ 换成注解里有未定义名的 f：f.__annotate__(1) →", lambda: f.__annotate__(Format.VALUE))
line("④ 同一个 f：f.__annotate__(2) →", lambda: f.__annotate__(Format.VALUE_WITH_FAKE_GLOBALS))
line("⑤ 同一个 f：f.__annotate__(3) →", lambda: f.__annotate__(Format.FORWARDREF))
line("⑥ 同一个 f：f.__annotate__(Format.STRING) →", lambda: f.__annotate__(Format.STRING))
caught = None
try:
    h.__annotate__(Format.FORWARDREF)
except NotImplementedError as exc:
    caught = exc
line("⑦ 那个 NotImplementedError 的 str(exc) 逐字（repr）→", lambda: repr(str(caught)))
line("⑧ f.__annotate__.__code__.co_names（这个函数引用了哪些名字）→",
     lambda: f.__annotate__.__code__.co_names)
line("⑨ Format 的四个成员 [(name, value)] →", lambda: [(m.name, m.value) for m in Format])
print("  → ①② 的分工：1（VALUE）和 2（VALUE_WITH_FAKE_GLOBALS）是**编译进 __annotate__ 的**，")
print("     直接调能出 dict；3（FORWARDREF）和 4（STRING）是 annotationlib 在外面替你做包装")
print("     （3 靠替身 globals、4 靠字符串化替身），编译版根本不存在，直接调只能拿到暗号。")
print("  → ⑦ 消息是空串：except NotImplementedError 之后 str(exc) 什么也读不到 ——")
print("     它是「本口径没有编译版」的暗号，不是线索，排错时别在这句话里找原因。")
print("  → ⑧ 这条把话钉死：__annotate__ 的 co_names 里只有 ('Widget', 'Result') 这种")
print("     注解里的名字，没有 annotationlib / Format —— 它就是编译器替你写的一个取值小函数。")
print("  → 判据：对外一律走 annotationlib.get_annotations(obj, format=...)（第 b 篇）；")
print("     直接调 __annotate__ 只有你自己调试时值得，2 号口径更是内部专用（b 篇有实测）。")
print("  → 断言：h.__annotate__(1) 与 (2) 的读数逐字相同 = "
      "{'x': <class 'int'>, 'y': <class 'str'>, 'return': None}；(3)(4) 与 ⑤⑥ 四条全是 "
      "NotImplementedError 且 repr(str(exc)) == \"''\"；① 的原文 = "
      "TypeError: __annotate__() missing 1 required positional argument: 'format'；"
      "③④ 的原文 = NameError: name 'Widget' is not defined。")

print("\n=== 5. 「加引号 = 延迟」是个错觉：引号只是把一个 str 存进去 ===")


def f2(y: "Missing") -> None:
    pass


line("① f2.__annotations__（名字根本不存在，但加了引号）→", lambda: f2.__annotations__)
line("② get_annotations(f2, VALUE) →", lambda: annotationlib.get_annotations(f2, format=Format.VALUE))
line("③ 里面 'y' 的值类型名 →", lambda: type(f2.__annotations__["y"]).__name__)
line("④ typing.get_type_hints(f2) →", lambda: typing.get_type_hints(f2))
line("⑤ 自己 eval 那个字符串 eval(f2.__annotations__['y']) →", lambda: eval(f2.__annotations__["y"]))
line("⑥ get_annotations(f2, eval_str=True) →",
     lambda: annotationlib.get_annotations(f2, eval_str=True))
print("  → ①②③ 三行一起看：引号包起来之后没有任何东西被求值，存进去的就是那个 str 本身，")
print("     连 VALUE 口径也原样给你 str —— 因为被求值的表达式就是个字符串字面量。")
print("  → ④⑤⑥ 三行一起看：谁想把它当「真类型」用，谁就得自己去 eval，于是那个不存在的名字")
print("     立刻把 NameError 还给你。get_type_hints 会 eval，eval_str=True 也会 eval。")
print("  → 判据：引号能骗过的只有「不读就不炸」这一种场景；只要有人把它当类型用 ——")
print("     get_type_hints（④）、自己 eval（⑤）、get_annotations(eval_str=True)（⑥）——")
print("     那个名字还是得在场。要真的不碰名字，走 b 篇的 STRING / FORWARDREF 口径。")
print("  → 断言：① ② 读数逐字 = {'y': 'Missing', 'return': None}（注意 return 是 None 不是字符串），"
      "③ 逐字 = 'str'，④⑤⑥ 三条原文逐字相同 = NameError: name 'Missing' is not defined。")

print("\n=== 6. 函数体里的局部变量注解：跑得过、查不到、运行时哪儿都不存 ===")


def g():
    x: Widget = 1
    return x


def noteval():
    q: UndefinedName = 1
    r: int = 2
    return q + r


line("① 调用 g() →", lambda: g())
line("② g.__annotations__ →", lambda: g.__annotations__)
line("③ g.__annotate__ →", lambda: g.__annotate__)
line("④ 试着调它 g.__annotate__(Format.VALUE) →", lambda: g.__annotate__(Format.VALUE))
line("⑤ g.__code__.co_varnames（局部注解留下的唯一痕迹）→", lambda: g.__code__.co_varnames)
line("⑥ 注解名字根本不存在的 noteval() 调用 →", lambda: noteval())
line("⑦ noteval.__annotations__ →", lambda: noteval.__annotations__)
line("⑧ noteval.__code__.co_varnames →", lambda: noteval.__code__.co_varnames)
print("  → ①⑥：函数体里的 `x: Widget = 1` 那行只做赋值，注解那半句连求值都没有 ——")
print("     它不像 def 的签名那样编译出 __annotate__，所以连「以后会炸」都没得炸。")
print("  → ②⑦ 是空 dict（不是缺键、也不是 NameError）：局部注解不进函数的 __annotations__。")
print("  → ③④ 是一对：g.__annotate__ 这个属性存在（继承自函数类型）但值是 None，")
print("     所以「用 hasattr 判断有没有注解」在这里也会骗你 —— 判据是 `__annotate__ is not None`。")
print("  → ⑤⑧ 只剩变量名本身：局部注解在运行时**完全不可读**，")
print("     所以任何想检查它的工具（第 g 篇自制检查器）必须回到源码层（ast），没捷径。")
print("  → 断言：① = 1、⑥ = 3、②⑦ 逐字 = {}、③ 是 None、④ 的原文 = "
      "TypeError: 'NoneType' object is not callable、⑤ 逐字 = ('x',)、⑧ 逐字 = ('q', 'r')。")

print("\n=== 7. 类体里的裸注解：类定义得成，读的时候炸，而且不产生任何类属性 ===")


class C:
    x: Widget
    y: int = 3


line("① 类定义成功了：C.__name__ →", lambda: C.__name__)
line("② C.y →", lambda: C.y)
line("③ C().y（实例上也拿得到）→", lambda: C().y)
line("④ 读 C.__annotations__ →", lambda: C.__annotations__)
line("⑤ typing.get_type_hints(C) →", lambda: typing.get_type_hints(C))
line("⑥ 'x' in vars(C)（裸注解变成类属性了吗）→", lambda: "x" in vars(C))
line("⑦ vars(C) 里非双下划线的键（排序）→",
     lambda: sorted(k for k in vars(C) if not k.startswith("__")))
line("⑧ 'x' in C.__dict__ ｜ getattr(C, 'x', '没这个属性') →",
     lambda: ("x" in C.__dict__, getattr(C, "x", "没这个属性")))
line("⑨ C.__annotate__ 的类型 →", lambda: type(C.__annotate__).__name__)
print("  → ⑥⑦⑧ 是同一件事：`x: Widget` 这行**不产生 x**。它只把「怎么求 Widget」记进")
print("     __annotate__，所以类体里的裸注解连一个类属性都不绑 —— 别指望 `C.x` 取到类型。")
print("  → ②③ 对照：同一个类体里带值的 `y: int = 3` 就是普通赋值，类和实例都拿得到 3。")
print("  → ④⑤ 是同一句话：一读就把整份注解求值，第一个不存在的名字把整份顶回来 ——")
print("     所以「谁去读类注解谁炸」；反过来第 6 章的 dataclass 建字段时为什么不炸，")
print("     是本篇 ④（2 号口径编译版）留下的伏笔，第 c 篇用计数器量。")
print("  → 断言：⑥ = False、⑦ 逐字 = ['y']、⑧ 逐字 = (False, '没这个属性')、② ③ 都 = 3、"
      "④⑤ 原文逐字相同 = NameError: name 'Widget' is not defined、⑨ = 'function'。")

print("\n=== 8. 模块级注解：同一个脚本里三种读法，两个**不同消息**的 NameError ===")
me = sys.modules["__main__"]
line("① 在脚本自己内部按名字读裸的 __annotations__ →", lambda: __annotations__)   # noqa: F821
line("② '__annotations__' in globals() →", lambda: "__annotations__" in globals())
line("③ 按属性读 me.__annotations__ →", lambda: me.__annotations__)
line("④ 属性读炸过之后，globals 里有那个键了吗 →", lambda: "__annotations__" in globals())
line("⑤ 再按名字读一次 →", lambda: __annotations__)                              # noqa: F821
line("⑥ STRING 口径读模块 →", lambda: annotationlib.get_annotations(me, format=Format.STRING))
line("⑦ FORWARDREF 口径读模块（值的类型名）→",
     lambda: {k: type(v).__name__ for k, v in
              annotationlib.get_annotations(me, format=Format.FORWARDREF).items()})
line("⑧ VALUE 口径读模块 →", lambda: annotationlib.get_annotations(me, format=Format.VALUE))
line("⑨ 'Y' in dir()（裸注解那行绑了吗）→", lambda: "Y" in dir())
line("⑩ Z 的值（带值那行绑了）→", lambda: Z)
line("⑪ 模块对象上的 __annotate__ 类型 →", lambda: type(me.__annotate__).__name__)
print("  → ①和③ 都叫 NameError，但**消息不一样**：① 说的是 '__annotations__' 这个名字不在")
print("     globals 里；③ 说的是注解里的 'SomeUndefinedName' 求值时查不到。")
print("     只看异常类型名就会查错方向：① 是「那个键还没被创建」，③ 才是「注解里的名字不存在」。")
print("  → ②④⑪：模块级 __annotations__ 是个**懒属性**（走 module __getattr__），")
print("     所以模块内部按名字查它查不到；而 __annotate__ 是模块 __dict__ 里的真键。")
print("  → ⑥⑦：不求值也能读，而且两种读法给的形状不同 —— STRING 给源码文本，")
print("     FORWARDREF 把缺的名字包成 ForwardRef 对象（⑦ 两个值的类型名都是 ForwardRef）。")
print("  → ⑨⑩：`Y: SomeUndefinedName` 什么都不绑，`Z: AnotherUndefined = 1` 只绑 Z=1 ——")
print("     模块级注解语句和类体里一样，注解那半句不产生名字。")
print("  → 断言：①⑤ 原文逐字 = NameError: name '__annotations__' is not defined；"
      "③⑧ 原文逐字 = NameError: name 'SomeUndefinedName' is not defined；②④ 逐字 = False；"
      "⑥ 逐字 = {'Y': 'SomeUndefinedName', 'Z': 'AnotherUndefined'}；⑩ = 1。")

print("\n=== 9. 负对照：换注解全合法的真模块，同一行裸名读法会从炸变成读得到 ===")
print(f"  现场：{WORK.name}/lazy_ok.py（注解都是内置类型）+ lazy_bad.py（注解名不存在）；"
      f"子进程 = {PYNAME} + cwd={WORK.name} + 干净 env。下面 │ 开头是它的原始输出。")
done = subprocess.run([sys.executable, "r_module.py"], cwd=str(WORK),
                      env={"LC_ALL": "C.UTF-8", "LANG": "C.UTF-8", "PYTHONIOENCODING": "utf-8"},
                      stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                      text=True, encoding="utf-8", errors="replace")
for out in done.stdout.splitlines():
    print(f"  │ {mask(out)}")
print(f"  └ 退出码 = {done.returncode}")
print("  → 这一格专门把第 8 格的断言「逼响」：③ 和 ⑤ 是 lazy_ok 里**同一个函数的同一行代码**，")
print("     在属性读之前返回 ('NameError', ...)、之后返回 ('读到了', dict) ——")
print("     所以第 8 格②④ 的 False 不是「这个名字永远查不到」，而是「还没有人成功读过它」。")
print("  → ⑨ 是另一半对照：lazy_bad 的属性读**失败**了，于是键也没被留下 ——")
print("     跟第 2 格「失败不缓存」完全对上：模块和函数的规矩是一样的。")
print("  → ⑦ 说明键的归属：那个 '__annotations__' 键不是源码写的（两个模块里一个字没提），")
print("     是属性读成功那一刻由解释器塞进模块 __dict__ 的（和第 09 章 __builtins__ 同一套路）。")
print("  → 判据：读模块级注解请用 annotationlib.get_annotations(模块对象, format=...) 或")
print("     模块对象.__annotations__；在模块内部写裸名 `__annotations__` 是个坑。")
print("  → ⑫ 那一对 {0, 1} 是编译器为「模块级注解语句」留在 globals 里的开关（每条注解一个下标），")
print("     它是什么、谁能看见它，第 b 篇最后一格用 symtable 钉住。")
print("  → 断言：③ 逐字 = ('NameError', \"name '__annotations__' is not defined\")、"
      "⑤ 逐字 = ('读到了', {'W': <class 'int'>, 'NAMES': list[str]})、"
      "② = False 而 ⑥ = True、⑧ 原文 = NameError: name 'Ghost' is not defined、⑨ = False、"
      "⑩ 逐字 = {'K': 'Ghost', 'M': 'int'}、⑪ = (False, 5)、⑫ 两个模块都是 {0, 1}。")

print("\n=== 10. 核对格：把这篇最要紧的读数改成会响的据（前面各格的 │ 行只打印、不判）===")
CELL = {"n": 0, "ok": 0}


def delta(thunk):
    """读一次注解，返回 Boom.hits 的增量 —— 用来判「这次有没有真把表达式重跑一遍」。"""
    before = Boom.hits
    thunk()
    return Boom.hits - before


WIDGET_ERR = ("NameError", "name 'Widget' is not defined")
print(eq("① 定义那一行什么都没查：f(3) 照样算得出", lambda: f(3), 3))
print(eq("② 读 f.__annotations__ 连读两遍，报错逐字相同（失败不缓存）",
         lambda: (fail(lambda: f.__annotations__), fail(lambda: f.__annotations__)),
         (WIDGET_ERR, WIDGET_ERR)))
print(eq("③ hasattr 试探被注解的 NameError 打穿（它只吞 AttributeError）",
         lambda: fail(lambda: hasattr(f, "__annotations__")), WIDGET_ERR))
print(eq("④ 成功那一次才缓存：再读拿到的是同一个 dict 对象",
         lambda: ok_fn.__annotations__ is cache, True))
print(eq("⑤ STRING 口径每读一遍就把注解表达式重跑一次（hits 增量）",
         lambda: delta(lambda: annotationlib.get_annotations(ok_fn, format=Format.STRING)), 1))
print(eq("⑥ VALUE 口径命中缓存，不重跑（hits 增量 0）",
         lambda: delta(lambda: annotationlib.get_annotations(ok_fn, format=Format.VALUE)), 0))
print(eq("⑦ 3、4 号没有编译版：直接调只得到空消息的 NotImplementedError",
         lambda: (fail(lambda: h.__annotate__(Format.FORWARDREF)),
                  fail(lambda: h.__annotate__(Format.STRING))),
         (("NotImplementedError", ""), ("NotImplementedError", ""))))
print(eq("⑧ 函数体里的局部注解：调用得过、运行时读不到（__annotations__ 是空 dict）",
         lambda: (g(), g.__annotations__, noteval()), (1, {}, 3)))
print(eq("⑨ 类体里的裸注解不产生类属性，带值的那行照常绑",
         lambda: ("x" in vars(C), C.y, Z), (False, 3, 1)))
print(eq("⑩ 引号 ≠ 延迟：VALUE 原样给 str，get_type_hints 却自己去 eval",
         lambda: (f2.__annotations__, fail(lambda: typing.get_type_hints(f2))),
         ({"y": "Missing", "return": None}, ("NameError", "name 'Missing' is not defined"))))
print(eq("⑪ 模块里两种读法的报错**消息不同名**：键不存在 vs 注解名不存在",
         lambda: (fail(lambda: __annotations__), fail(lambda: me.__annotations__)),
         (("NameError", "name '__annotations__' is not defined"),
          ("NameError", "name 'SomeUndefinedName' is not defined"))))
print(ledger("第 1~9 格最要紧的十一条读数逐字钉住：定义时不求值、失败不缓存、成功才缓存、"
             "STRING 重跑而 VALUE 不重跑、3/4 号无编译版、体内注解运行时不可读、"
             "裸注解不绑名字、引号只是 str、模块两种读法消息不同名"))

print("\n=== 收口：注解的一句话账本 ===")
print("  ① `def` / `class` / 模块级里的注解，在写下的那一刻**一行都不执行**：解释器只编译出一个")
print("     `__annotate__` 函数挂在对象上（第 1、7、8 格：名字不存在照样定义成功，f(3) 照样回 3）。")
print("  ② 第一次「读」才求值；成功才缓存成那个 dict（第 2 格：hits 只涨到 1、两次 is True），")
print("     失败**不**缓存（负对照：hits 1→2），所以「刚才读没炸」保证不了「下一次不炸」。")
print("  ③ 缓存的 dict 是活的：直接改它 = 改这个函数的注解，加新键也留得下来（第 3 格）；")
print("     而 get_annotations 给的是拷贝，改它不影响本体。")
print("  ④ 四个口径里只有 1 VALUE、2 VALUE_WITH_FAKE_GLOBALS 编译进了 __annotate__，")
print("     3 FORWARDREF、4 STRING 直接调只得到空消息的 NotImplementedError（第 4 格）。")
print("  ⑤ 引号 ≠ 延迟，只是存了个 str；谁当真类型用谁自己 eval（第 5 格）。")
print("  ⑥ 位置决定下场：签名 → 可读、类体 → 可读但不绑名字、函数体内 → 运行时完全不可读")
print("     （第 6、7、8 格），要查局部变量注解只能回源码层（ast，第 g 篇）。")
print("  ★ 三条代价：注解里放有副作用的表达式 = 每读一次 STRING 口径就多跑一次副作用（第 2 格⑧）；")
print("     注解里的名字必须在「读它的人」的 globals 里（第 5、8 格）；")
print("     hasattr 这类试探在 3.14 会被注解的 NameError 直接打穿（第 1 格⑤）——")
print("     所以运行时凡读注解都得 try/except 包着，这也是本章后面每支脚本都自带 safe() 的原因。")

shutil.rmtree(WORK)
dirs = sorted(p.name for p in Path.cwd().iterdir() if p.is_dir() and p.name.endswith("-demo"))
pycs = [p.name for p in Path.cwd().rglob("*.pyc")]
caches = [p.name for p in Path.cwd().iterdir() if p.name == "__pycache__"]
print(f"\n  收尾自证：{WORK.name}/ 还在吗 → {WORK.exists()} ｜ 当前目录剩下的 *-demo 目录 → {dirs} ｜"
      f" 剩下的 __pycache__ 目录 → {caches} ｜ 剩下的 .pyc 文件 → {len(pycs)} 个"
      f"（章目录零产物；子进程的 __pycache__ 跟着沙箱目录一起删了）")
print(f"  全篇判据合计 {TOTAL['ok']}/{TOTAL['n']} 条成立（少一条上面就有 [不成立！]）")
