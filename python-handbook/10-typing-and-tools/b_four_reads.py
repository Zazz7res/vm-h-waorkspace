#!/usr/bin/env python3
# ============================================================================
# 取证脚本 b_four_reads.py —— 第 10 章第 2 篇：同一份注解的四把钥匙，只有两把不炸
#
# 要证明什么：a 篇证明了「注解是一段还没执行的代码」，这一篇要回答「那我到底该怎么读它」。
# 3.14 给出的对外入口是 `annotationlib.get_annotations(对象, format=...)`，format 有四个值，
# 本脚本对**同一个** `def f(x: Widget) -> Result`（Widget/Result 都不存在）把四把钥匙都插一遍：
#   1 VALUE                  → 当真类型求值 → NameError（和第 a 篇一样）
#   2 VALUE_WITH_FAKE_GLOBALS → 连门都不让进：get_annotations 直接 ValueError
#                              （它是给 dataclass 内部用的；直接调 __annotate__(2) 才走通，
#                               而那里照样是 NameError —— 别把 2 号当成救命稻草）
#   3 FORWARDREF             → 不炸，缺的名字包成 ForwardRef 对象
#   4 STRING                 → 不炸，给源码文本（str）
# 另外量五件读屏的人会做错的事：ForwardRef 这个牌子在 3.14 到底几家在用（④）、
# 加引号和没加引号在 STRING 口径下能不能区分（⑤）、inspect.signature 和
# typing.get_type_hints 这两把「别人的钥匙」默认走哪个口径（⑥）、
# `from __future__ import annotations` 那一行把上面全部改掉之后还剩什么炸（⑦，子进程对照跑）、
# annotationlib 这个模块的 dir() 里哪些是公开 API、哪些是顺手漏进来的实现细节（⑧⑨）。
#
# 为什么要有它：「读注解」是本章后面所有工具（typeguard、自制检查器、dataclass 的坑）的入口。
# 四把钥匙的差别不是风格，是**会不会把你的程序读崩**：1、2 会，3、4 不会。
# 而且第 7 格那条 future 分支只能开在文件最开头，同进程里切不了 ——
# 这是所有「我加了 future 那行为什么没生效」问题的物理原因。
#
# 现场：只在当前工作目录现造 reads-demo/，里面写两份只差第一行的子进程源码
# （plain_case.py / future_case.py），跑完整目录删掉，章目录零产物。
# 屏幕上不出现绝对路径 / 对象地址 / 时间 / PID：家目录前缀折成 <家目录>、当前目录前缀去掉、
# ForwardRef 的 repr 里那个 `<function f at 0x…>` 一律折成 <地址>（所以第 ③④ 格只印
# type(v).__name__、v.__arg__ 和相等比较，不印 repr）；traceback 不印，只印 `异常类型: 原文`。
# 子进程一律 [sys.executable, ...] + cwd=reads-demo + 干净 env
# （只给 LC_ALL/LANG/PYTHONIOENCODING，绝不继承 PYTHONPATH / PYTHONHOME）。
# 退出码恒为 0：所有报错都被接住当成证据打印。
#
# 本机 Python 3.14.4 / Ubuntu 26.04（CPython）。以下本机未验证，别当事实引用：
#   - mypy / pyright 等静态检查器（本机没装，`import mypy` 直接 ModuleNotFoundError）；
#   - annotationlib 官方文档收录的 API 清单（这里只量代码里 `__all__` 声明了什么，
#     「文档里写了什么」得翻 docs.python.org，本机不联网核对）；
#   - 3.13 及更早有没有 annotationlib 这个模块、ForwardRef 上有没有 __evaluate__；
#   - PyPy 等其他实现、Windows 上的路径与编码表现。
# ============================================================================
import os
import re
import shutil
import subprocess
import symtable
import sys
import typing
from pathlib import Path

import annotationlib
import inspect

WORK = Path("reads-demo")
CWD = Path.cwd().resolve()
HOME = os.path.expanduser("~")
PYNAME = os.path.basename(sys.executable)
Format = annotationlib.Format


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
    """唯一的取证通道：成功印 `OK → repr(值)`，失败印 `异常类型: 原文`，一律过 mask()。"""
    try:
        body = "OK → " + repr(thunk())
    except BaseException as exc:            # noqa: BLE001 —— 报错本身就是证据
        head = f"{type(exc).__name__}: {exc}"
        if str(exc) == "":
            head += f"（str(exc) 的 repr = {str(exc)!r}，即空串）"
        body = head
    print(f"  │ {tag} {mask(body)}")


CELL = {"n": 0, "ok": 0}      # 本格判据数：只有第 11 格（核对格）用得到
TOTAL = {"n": 0, "ok": 0}     # 全篇判据总数 / 成立数（收尾那条合计行用它）


def fail(thunk):
    """跑一下，炸了就返回（异常类型名, str(异常)）；没炸返回 None。报错原文一律过 mask()。"""
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


# ============================ 取证对象（全脚本共用）============================
def f(x: Widget) -> Result:        # Widget / Result 都不存在：四把钥匙的试金石
    return x


def sample_fn(x: list[int]) -> LocalPlaceholder:   # noqa: F821 —— 第 ⑨ 格 type_repr 用
    return x


class LocalPlaceholder:
    pass


def plain_fn(x: int, y: str = "s") -> None:
    pass


# 引号 vs 裸名：第 ⑤ 格要证明「STRING 口径下两者长得一模一样」
def bare(q: Missing) -> None:
    pass


def quoted(q: "Missing") -> None:
    pass


# ============ 现场：两份只差第一行的子进程源码（future 那行只能开在文件最开头）============
CASE_BODY = '''"""同一个函数、同一套读法，跑两遍：一次带 from __future__ import annotations，一次不带。"""
import annotationlib as A
import typing

FMT = A.Format


def safe(label, thunk):
    """把读数打成一列：成功印值，失败印「异常类型: 原文」。报错绝不往外抛。"""
    try:
        print(label + " → OK: " + repr(thunk()))
    except BaseException as exc:
        msg = type(exc).__name__ + ": " + str(exc)
        print(label + " → " + msg + ("" if str(exc) else "（空消息）"))


def f(x: Widget) -> Result:
    return x


safe("① f.__annotations__", lambda: f.__annotations__)
safe("② get_annotations VALUE", lambda: A.get_annotations(f, format=FMT.VALUE))
safe("③ get_annotations FORWARDREF 的值的类型名",
     lambda: {k: type(v).__name__ for k, v in A.get_annotations(f, format=FMT.FORWARDREF).items()})
safe("③b FORWARDREF 里装的那个名字（不印 repr，它带对象地址）",
     lambda: {k: getattr(v, "__arg__", v) for k, v in A.get_annotations(f, format=FMT.FORWARDREF).items()})
safe("④ get_annotations STRING", lambda: A.get_annotations(f, format=FMT.STRING))
safe("⑤ get_annotations VALUE_WITH_FAKE_GLOBALS",
     lambda: A.get_annotations(f, format=FMT.VALUE_WITH_FAKE_GLOBALS))
safe("⑤b 直接调 f.__annotate__(2)", lambda: f.__annotate__(FMT.VALUE_WITH_FAKE_GLOBALS))
safe("⑥ typing.get_type_hints(f)", lambda: typing.get_type_hints(f))
safe("⑦ 直接调 f.__annotate__(Format.STRING)", lambda: f.__annotate__(FMT.STRING))
safe("⑧ FORWARDREF 里还有没有 ForwardRef 对象（值的类型名集合）",
     lambda: sorted({type(v).__name__ for v in A.get_annotations(f, format=FMT.FORWARDREF).values()}))
'''

shutil.rmtree(WORK, ignore_errors=True)     # 上次的残骸先清掉，两遍跑出同一个现场
WORK.mkdir(parents=True, exist_ok=True)
(WORK / "plain_case.py").write_text(CASE_BODY, encoding="utf-8")
(WORK / "future_case.py").write_text("from __future__ import annotations\n" + CASE_BODY,
                                     encoding="utf-8")


def run_case(name):
    """在 reads-demo/ 里跑一份用例源码：stdout+stderr 合并，保证顺序和真实一致。"""
    return subprocess.run([sys.executable, name], cwd=str(WORK),
                          env={"LC_ALL": "C.UTF-8", "LANG": "C.UTF-8",
                               "PYTHONIOENCODING": "utf-8"},
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, encoding="utf-8", errors="replace")


print("=== 1. 四把钥匙的编号牌：annotationlib.Format 有哪几格 ===")
line("① [(m.name, m.value) for m in Format] →", lambda: [(m.name, m.value) for m in Format])
line("② Format.__members__ 的键（声明顺序）→", lambda: list(Format.__members__))
line("③ Format(4).name ｜ Format['STRING'].name →", lambda: (Format(4).name, Format["STRING"].name))
line("④ isinstance(Format.VALUE, int) ｜ Format.STRING == 4 →",
     lambda: (isinstance(Format.VALUE, int), Format.STRING == 4))
line("⑤ get_annotations 的签名（默认 format 是哪个）→",
     lambda: str(inspect.signature(annotationlib.get_annotations)))
print("  → ①② 是这张表的表头：四格的编号是 1~4，后面每一格都按这个顺序插一遍。")
print("  → ④ 说明 Format 是 IntEnum：所以 a 篇里 `h.__annotate__(3)` 那种裸整数也认，")
print("     读源码时看到 `format=2` 别以为是布尔，它是 VALUE_WITH_FAKE_GLOBALS。")
print("  → ⑤ 里 `format=<Format.VALUE: 1>`：不传 format 的 get_annotations 就是 1 号，")
print("     也就是「会炸的那一把」—— 这是最反直觉的默认值，a 篇第 1 格已经撞过一次。")
print("  → 断言：① 逐字 = [('VALUE', 1), ('VALUE_WITH_FAKE_GLOBALS', 2), ('FORWARDREF', 3), "
      "('STRING', 4)]；④ 逐字 = (True, True)；⑤ 里出现 format=<Format.VALUE: 1>。")

print("\n=== 2. 同一个 f（注解里两个名字都不存在）插四把钥匙：只有 3、4 两把不炸 ===")
line("① 1 号 VALUE →", lambda: annotationlib.get_annotations(f, format=Format.VALUE))
line("② 2 号 VALUE_WITH_FAKE_GLOBALS →",
     lambda: annotationlib.get_annotations(f, format=Format.VALUE_WITH_FAKE_GLOBALS))
line("③ 3 号 FORWARDREF（值的类型名，repr 带对象地址所以不印）→",
     lambda: {k: type(v).__name__ for k, v in annotationlib.get_annotations(f, format=Format.FORWARDREF).items()})
line("④ 4 号 STRING →", lambda: annotationlib.get_annotations(f, format=Format.STRING))
line("⑤ 顺手量不传 format（默认 1 号）→", lambda: annotationlib.get_annotations(f))
line("⑥ 2 号绕过 get_annotations，直接 f.__annotate__(2) →",
     lambda: f.__annotate__(Format.VALUE_WITH_FAKE_GLOBALS))
line("⑦ 对照：同一个 2 号直接插在注解合法的 plain_fn 上 →",
     lambda: plain_fn.__annotate__(Format.VALUE_WITH_FAKE_GLOBALS))
print("  → ② 是这一格最该记住的一行：2 号在**公开入口**上被直接拒绝，报的还是 ValueError，")
print("     连注解名字都不看就拦下来了 —— 它是 dataclass 这类标准库内部用的后门")
print("     （a 篇第 4 格证明它确实编译进了 __annotate__，但那不是给你我用的）。")
print("  → ⑥⑦ 两行一起看：绕过公开入口之后，2 号在注解名字不存在时**照样 NameError**，")
print("     在名字合法时能出 dict —— 所以「2 号能救未定义的名字」是彻底错的，")
print("     它救的是「求值时不想让模块 globals 被看见」（fake globals 的字面意思），不是救名字不存在。")
print("  → ③④ 是不炸的两把：3 号给你 ForwardRef 对象、4 号给你源码文本 str。")
print("  → 断言：① ⑤ 原文逐字 = NameError: name 'Widget' is not defined；"
      "② 原文逐字 = ValueError: The VALUE_WITH_FAKE_GLOBALS format is for internal use only；"
      "⑥ 原文 = NameError: name 'Widget' is not defined；⑦ 逐字 = "
      "{'x': <class 'int'>, 'y': <class 'str'>, 'return': None}；③ 逐字 = "
      "{'x': 'ForwardRef', 'return': 'ForwardRef'}；④ 逐字 = {'x': 'Widget', 'return': 'Result'}。")

print("\n=== 3. 3 号钥匙给你的 ForwardRef 对象：能问名字，能自己给环境求值 ===")
fr = annotationlib.get_annotations(f, format=Format.FORWARDREF)["x"]
line("① type(fr) 的模块.类名 →", lambda: type(fr).__module__ + "." + type(fr).__qualname__)
line("② fr.__arg__（它装的那个名字）→", lambda: fr.__arg__)
line("③ str(fr) 的原文（对象地址当场折掉）→", lambda: str(fr))
line("④ fr == annotationlib.ForwardRef('Widget') 吗 →", lambda: fr == annotationlib.ForwardRef("Widget"))
line("⑤ 两个都不带 owner 的呢：ForwardRef('Widget') == ForwardRef('Widget') →",
     lambda: annotationlib.ForwardRef("Widget") == annotationlib.ForwardRef("Widget"))
line("⑥ fr.evaluate(locals={'Widget': int}) →", lambda: fr.evaluate(locals={"Widget": int}))
line("⑦ fr.evaluate() 空手求值 →", lambda: fr.evaluate())
line("⑧ fr 身上非双下划线的成员 →", lambda: sorted(n for n in dir(fr) if not n.startswith("__")))
print("  → ④⑤ 合起来是「ForwardRef 的相等算 owner」：③ 那个 owner=<function f at …> 不是装饰，")
print("     owner 参与相等判断，所以你**不能**拿 `ForwardRef('Widget')` 去当键查表。")
print("     要比较只比名字：用 ② 的 `fr.__arg__` 或先 `evaluate`。")
print("  → ⑥⑦ 是「延迟求值」真正落地的那一步：3 号钥匙只是把名字带回来，")
print("     求不求、拿什么环境求，是你自己的事；给不出环境（⑦）还是 NameError。")
print("  → 判据：`fr.evaluate(globals=..., locals=...)` 全是关键字参数（⑥ 那种写法），")
print("     位置参数会 TypeError；读到的值要落定就先 evaluate，别把 ForwardRef 当类型传下去。")
print("  → 断言：① 逐字 = 'annotationlib.ForwardRef'；② 逐字 = 'Widget'；④ = False 而 ⑤ = True；"
      "⑥ 逐字 = <class 'int'>；⑦ 原文 = NameError: name 'Widget' is not defined；"
      "⑧ 逐字 = ['_evaluate', 'evaluate']。")

print("\n=== 4. ForwardRef 这块牌子在 3.14 只有一家在用：typing 的是别名，而且没有 __evaluate__ ===")
line("① annotationlib.ForwardRef is typing.ForwardRef →",
     lambda: annotationlib.ForwardRef is typing.ForwardRef)
line("② typing.ForwardRef('Widget') 的真实类型 →",
     lambda: type(typing.ForwardRef("Widget")).__module__ + "." + type(typing.ForwardRef("Widget")).__qualname__)
line("③ issubclass(annotationlib.ForwardRef, str) →", lambda: issubclass(annotationlib.ForwardRef, str))
line("④ typing.ForwardRef('int').__evaluate__ →", lambda: typing.ForwardRef("int").__evaluate__())
line("⑤ annotationlib.ForwardRef('int').evaluate()（换成 3.14 的真名字）→",
     lambda: annotationlib.ForwardRef("int").evaluate())
line("⑥ ForwardRef.__mro__ →", lambda: [c.__name__ for c in annotationlib.ForwardRef.__mro__])
line("⑦ typing 里还有别的 ForwardRef 吗（typing 命名空间里带 forward 的名字）→",
     lambda: sorted(n for n in dir(typing) if "orward" in n))
print("  → ①②⑥⑦ 四条合起来把「两家在用」这个说法纠正掉：3.14 里 ForwardRef 这个类就一个，")
print("     住在 annotationlib，typing.ForwardRef 只是同一个对象的别名（`is` 为 True）。")
print("  → ③ 是防错的第二条：ForwardRef **不是** str 的子类 —— 别把它当字符串拼接或 .startswith，")
print("     它是个带 owner 的普通对象。")
print("  → ④ 的原文很关键：AttributeError: 'ForwardRef' object has no attribute '__evaluate__' ——")
print("     旧草案里那个双下划线的 __evaluate__ 在本机 3.14.4 不存在，能用的是 ⑤ 的 evaluate()；")
print("     抄旧文章的 `fr.__evaluate__(globals, locals)` 在这儿直接 AttributeError。")
print("  → ⑤ 那一行是「空手 evaluate」的下场：传的是内置名 int，所以缺省环境（builtins 兜底）")
print("     就能求出来；换成第 3 格 ⑦ 那个自定义名 Widget，空手就是 NameError ——")
print("     evaluate 的缺省环境只到 builtins，不到你的模块 globals。")
print("  → 断言：① = True、② 逐字 = 'annotationlib.ForwardRef'、③ = False、"
      "④ 原文逐字 = AttributeError: 'ForwardRef' object has no attribute '__evaluate__'、"
      "⑤ 逐字 = <class 'int'>、⑥ 逐字 = ['ForwardRef', 'object']、"
      "⑦ 逐字 = ['ForwardRef', '_make_forward_ref', 'evaluate_forward_ref']。")

print("\n=== 5. 加引号 vs 不加引号，在 4 号钥匙下长得一模一样（分不清就是分不清）===")
line("① bare(q: Missing) 用 STRING 读 →", lambda: annotationlib.get_annotations(bare, format=Format.STRING))
line("② quoted(q: 'Missing') 用 STRING 读 →", lambda: annotationlib.get_annotations(quoted, format=Format.STRING))
line("③ 两者 STRING 的值相等吗 →",
     lambda: annotationlib.get_annotations(bare, format=Format.STRING)["q"]
             == annotationlib.get_annotations(quoted, format=Format.STRING)["q"])
line("④ bare 用 VALUE 读 →", lambda: annotationlib.get_annotations(bare, format=Format.VALUE))
line("⑤ quoted 用 VALUE 读 →", lambda: annotationlib.get_annotations(quoted, format=Format.VALUE))
line("⑥ quoted 用 FORWARDREF 读（值的类型名）→",
     lambda: {k: type(v).__name__ for k, v in
              annotationlib.get_annotations(quoted, format=Format.FORWARDREF).items()})
line("⑦ bare 用 FORWARDREF 读（值的类型名）→",
     lambda: {k: type(v).__name__ for k, v in
              annotationlib.get_annotations(bare, format=Format.FORWARDREF).items()})
print("  → ①②③ 三行是本格的全部内容：4 号钥匙读出来的都是字符串 'Missing'，")
print("     `q: Missing` 和 `q: \"Missing\"` 在 STRING 口径下**逐字相等**，读的人无从分辨。")
print("  → ④⑤⑥⑦ 是分辨得开的两把：VALUE 下裸名炸、引号给 str；")
print("     FORWARDREF 下裸名给 ForwardRef、引号给 str（⑥ 和 ⑦ 的类型名不一样）。")
print("  → 判据：一旦代码改成 STRING 口径（或加了 future 那行），你写在注解里的引号就")
print("     从「多余」变成「无痕」—— 两种写法产物相同，也就没法靠读注解把写法查出来，")
print("     要查只能读源码（ast，第 g 篇）。")
print("  → 断言：① ② 读数逐字相同 = {'q': 'Missing', 'return': 'None'}；③ = True；"
      "④ 原文 = NameError: name 'Missing' is not defined；⑤ 逐字 = {'q': 'Missing', 'return': None}；"
      "⑥ 逐字 = {'q': 'str', 'return': 'NoneType'}；⑦ 逐字 = {'q': 'ForwardRef', 'return': 'NoneType'}。")

print("\n=== 6. 别人手里的钥匙：inspect.signature 和 typing.get_type_hints 默认都插 1 号 ===")
line("① typing.get_type_hints(f) →", lambda: typing.get_type_hints(f))
line("② inspect.signature(f) →", lambda: str(inspect.signature(f)))
line("③ inspect.signature 的 annotation_format 默认值 →",
     lambda: inspect.signature(inspect.signature).parameters["annotation_format"].default)
line("④ signature(f, annotation_format=Format.STRING) →",
     lambda: str(inspect.signature(f, annotation_format=Format.STRING)))
line("⑤ signature(f, annotation_format=Format.FORWARDREF) →",
     lambda: str(inspect.signature(f, annotation_format=Format.FORWARDREF)))
line("⑥ signature(f, eval_str=True) →", lambda: str(inspect.signature(f, eval_str=True)))
line("⑦ signature(quoted)（引号那支，默认 1 号）→", lambda: str(inspect.signature(quoted)))
line("⑧ typing.get_type_hints(quoted) →", lambda: typing.get_type_hints(quoted))
print("  → ①②⑥ 三条全是 NameError：get_type_hints 和 signature 都不接 3、4 号（⑥ 只是把")
print("     字符串再 eval 一次），所以「文档字符串生成器 / 序列化库 / 老代码里的 signature()」")
print("     在注解名字不存在时照样把你的程序读崩 —— 它们不是你写的，你改不了。")
print("  → ③ 是这一格的解药：3.14 的 inspect.signature 多了一个 annotation_format 参数，")
print("     默认值就是 1 号；显式换成 4 号（④）或 3 号（⑤）就能读出来。")
print("  → ④ 那个读数 `(x: 'Widget') -> 'Result'` 里多了引号 —— 这就是「读到了文本」的样子，")
print("     别以为是 bug：它跟第 ⑤ 格里 quoted 的产物长一样。")
print("  → ⑦⑧ 合起来：同一支引号注解，signature 放过（str 就是合法注解）、get_type_hints 炸 ——")
print("     所以「加了引号到底安不安全」取决于**谁在读**，不取决于引号本身。")
print("  → 断言：① ② ⑥ 原文逐字 = NameError: name 'Widget' is not defined；"
      "③ 逐字 = <Format.VALUE: 1>；④ 逐字 = \"(x: 'Widget') -> 'Result'\"；⑤ 逐字 = '(x: Widget) -> Result'；"
      "⑦ 逐字 = \"(q: 'Missing') -> None\"；⑧ 原文 = NameError: name 'Missing' is not defined。")

print("\n=== 7. `from __future__ import annotations` 那一行：两份只差一行的源码，子进程对照跑 ===")
print(f"  现场：{WORK.name}/plain_case.py（无 future 行）和 {WORK.name}/future_case.py"
      f"（正文逐字相同，只在第一行多了那句）；子进程 = {PYNAME} + cwd={WORK.name} + 干净 env。")
print("  为什么只能这么跑：那句 future 只能在**文件/字符串最开头**生效，同一个进程里切不了，")
print("  所以「一行改变一切」必须开两个进程各跑一遍，两份源码共用同一段正文。")
plain = run_case("plain_case.py")
future = run_case("future_case.py")
print("  ┌ 不带 future（plain_case.py 的原始输出）")
for out in plain.stdout.splitlines():
    print(f"  │ {mask(out)}")
print(f"  └ 退出码 = {plain.returncode}")
print("  ┌ 带 future（future_case.py 的原始输出）")
for out in future.stdout.splitlines():
    print(f"  │ {mask(out)}")
print(f"  └ 退出码 = {future.returncode}")
print("  → 一对一对着看：①②④ 三行从「NameError / NameError / 文本」变成「文本 / 文本 / 文本」——")
print("     future 那行让编译进 __annotate__ 的 1 号也返回字符串，于是不炸了。")
print("  → 但 ⑥ 那一行**两边都炸**（NameError: name 'Widget' is not defined）：")
print("     get_type_hints 拿到字符串就去 eval，名字不在它那边的 globals 里，future 救不了它。")
print("  → ③ 两行的差别最有意思：不带 future 时 FORWARDREF 给 ForwardRef 对象，")
print("     带 future 时给的是 **'str'** —— 因为字符串常量本身被当成值求出来了")
print("     （⑧ 那行确认 future 分支下一个 ForwardRef 都没有）。")
print("     所以 future 分支下你没法用 FORWARDREF 分辨「谁没定义」。")
print("  → ③b 两行是最容易看漏的坑：不带 future 时它给的是 ForwardRef 里的名字，")
print("     带 future 时给的是字符串本身 —— 两行的**读数逐字相同**，")
print("     光看 ③b 分不清自己在哪个分支，必须看 ③ 的类型名（ForwardRef / str）或 ⑧ 的集合。")
print("  → ⑤ 两行不动：2 号在公开入口永远被 ValueError 拦下，跟 future 无关；")
print("     但 ⑤b 动了 —— 不带 future 时直接调 __annotate__(2) 照样 NameError，")
print("     带 future 时它连 2 号都直接返回文本、不炸了。所以「2 号会救你」在哪个分支都不成立：")
print("     不炸的原因从来不是 2 号，是那一行 future 把注解整体变成了文本。")
print("  → ⑦ 两行都不动：__annotate__(Format.STRING) 从来没被编译过，加不加 future 都是 "
      "NotImplementedError（空消息）—— a 篇第 4 格的结论在这里原样复现。")
print("  → 判据：加了那一行 = 全篇注解变文本，1 号口径也变成「读文本」不再炸；")
print("     代价是 3 号钥匙失去分辨力（③ 给 str 而不是 ForwardRef），而且 get_type_hints（⑥）照炸 ——")
print("     也就是说 future 只安慰「自己读注解」的代码，不安慰「替你 eval 注解」的下游。")
print("  → 断言：future 的 ① 逐字 = {'x': 'Widget', 'return': 'Result'} 而 plain 的 ① 原文 = "
      "NameError: name 'Widget' is not defined；future 的 ③ 逐字 = {'x': 'str', 'return': 'str'}"
      "（plain 是 {'x': 'ForwardRef', 'return': 'ForwardRef'}）；plain 的 ⑤b 原文 = "
      "NameError: name 'Widget' is not defined 而 future 的 ⑤b 逐字 = "
      "{'x': 'Widget', 'return': 'Result'}；⑤ 两边逐字相同 = ValueError: "
      "The VALUE_WITH_FAKE_GLOBALS format is for internal use only；⑥ 两边逐字相同 = "
      "NameError: name 'Widget' is not defined；两个退出码都是 0。")

print("\n=== 8. annotationlib 这个模块的 dir()：哪几个是它声明的公开 API，哪几个是漏进来的 ===")
all_names = sorted(dir(annotationlib))
public = sorted(annotationlib.__all__)
private = sorted(n for n in all_names if n.startswith("_"))
leaked = sorted(n for n in all_names if not n.startswith("_") and n not in annotationlib.__all__)
line("① __all__（模块自己声明的公开 API，排序）→", lambda: public)
line("② __all__ 的个数 →", lambda: len(public))
line("③ dir() 里下划线开头的私有名字（个数 + 全表）→", lambda: (len(private), private))
line("④ dir() 里非下划线、又不在 __all__ 里的（= 顺手漏进来的）→", lambda: leaked)
line("⑤ 这些漏进来的东西各是什么类型 →",
     lambda: {n: type(getattr(annotationlib, n)).__name__ for n in leaked})
line("⑥ annotationlib.__file__（只打 basename）→", lambda: os.path.basename(annotationlib.__file__))
line("⑦ annotationlib.__doc__ 第一行 →", lambda: (annotationlib.__doc__ or "").splitlines()[0])
print("  → ④⑤ 是本格的干货：漏进来的 6 个全是**模块名**（ast / builtins / enum / keyword / sys / types），")
print("     类型都是 module —— 它自己 import 了这些来实现字符串化，于是 dir() 里也看得见。")
print("     这不是 API，是「你家门口的电线杆」：别从 annotationlib.ast 拿东西，自己 import ast。")
print("  → ①② 那 8 个才是它声明的公开 API：Format / ForwardRef / get_annotations /")
print("     get_annotate_from_class_namespace / annotations_to_string / type_repr 是好认的六个，")
print("     剩下两个 call_annotate_function、call_evaluate_function 虽然挂在 __all__ 里，")
print("     签名却带 _is_evaluate 这种私有参数（第 ⑨ 格会印出来），是「写给标准库内部人」的。")
print("  → ★ 本机未验证：官方文档收录哪些算「文档里的公开 API」—— 这里只能量 `__all__` 声明了什么，")
print("     文档口径不在本机核对（见头部注释）。")
print("  → 断言：④ 逐字 = ['ast', 'builtins', 'enum', 'keyword', 'sys', 'types']（6 个）；"
      "⑤ 里 6 个值的类型名全是 'module'；② = 8；⑥ = 'annotationlib.py'。")

print("\n=== 9. 公开 API 里三个工具函数的实测读数（type_repr / annotations_to_string / 类命名空间）===")
line("① type_repr(list[int]) →", lambda: annotationlib.type_repr(list[int]))
line("② type_repr(sample_fn) →", lambda: annotationlib.type_repr(sample_fn))
line("③ type_repr(LocalPlaceholder) →", lambda: annotationlib.type_repr(LocalPlaceholder))
line("④ type_repr(typing.Optional[str]) →", lambda: annotationlib.type_repr(typing.Optional[str]))
line("⑤ type_repr('abc') / type_repr(3) →", lambda: (annotationlib.type_repr("abc"), annotationlib.type_repr(3)))
line("⑥ annotations_to_string({'x': list[int], 'r': typing.Optional[str]}) →",
     lambda: annotationlib.annotations_to_string({"x": list[int], "r": typing.Optional[str]}))
line("⑦ annotations_to_string(一个函数) →", lambda: annotationlib.annotations_to_string(sample_fn))
line("⑧ get_annotate_from_class_namespace({'__annotations__': {'x': int}}) →",
     lambda: annotationlib.get_annotate_from_class_namespace({"__annotations__": {"x": int}}))
line("⑨ get_annotate_from_class_namespace({'__annotate__': plain_fn.__annotate__}) →",
     lambda: annotationlib.get_annotate_from_class_namespace(
         {"__annotate__": plain_fn.__annotate__}))
line("⑩ get_annotate_from_class_namespace 是不是同一个对象 →",
     lambda: annotationlib.get_annotate_from_class_namespace(
         {"__annotate__": plain_fn.__annotate__}) is plain_fn.__annotate__)
line("⑪ call_annotate_function / call_evaluate_function 的签名 →",
     lambda: (str(inspect.signature(annotationlib.call_annotate_function)),
              str(inspect.signature(annotationlib.call_evaluate_function))))
print("  → ①③④⑤ 说明 type_repr 的口径：它给的是「写回源码时的样子」，"
      "list[int] 就印 list[int]，Optional[str] 印的是 `str | None`（新式写法，不是 typing.Optional）。")
print("  → ②③ 是本格的取舍：函数和类印出 `__main__.sample_fn` 这种「模块名.名字」——")
print("     带模块名是可以接受的（它是源码里真能写的东西），但**绝对路径一个字符都没带出来**，")
print("     这也是本脚本敢把它打上屏的原因（屏幕上任何路径都得先过 mask()）。")
print("  → ⑥⑦ 是一条使用守则：annotations_to_string 吃的是 **dict**，喂函数就 AttributeError"
      "（'function' object has no attribute 'items'）—— 名字里的 annotations 指那个字典。")
print("  → ⑧⑨⑩ 是第三个函数的真用途：类命名空间里**只有旧式 __annotations__ 时它返回 None**，")
print("     有编译出来的 __annotate__ 时原样给你那个函数（⑩ 的 is 为 True）——")
print("     写元类/类装饰器时要搬 __annotate__ 就用它，别自己去翻 vars()。")
print("  → 断言：① 逐字 = 'list[int]'；② 逐字 = '__main__.sample_fn'；③ 逐字 = "
      "'__main__.LocalPlaceholder'；④ 逐字 = 'str | None'；⑤ 逐字 = (\"'abc'\", '3')；"
      "⑥ 逐字 = {'x': 'list[int]', 'r': 'str | None'}；⑦ 原文 = "
      "AttributeError: 'function' object has no attribute 'items'；⑧ = None；⑩ = True。")

print("\n=== 10. symtable 眼里的那个开关：__conditional_annotations__ 是什么、谁能看见它 ===")
CASES = {
    "只有赋值 x = 1": "x = 1\n",
    "函数签名带注解": "def f(a: int) -> None: pass\n",
    "类体带注解": "class C:\n    a: int\n",
    "函数体带局部注解": "def f():\n    a: int = 1\n",
    "模块级裸注解 a: int": "a: int\n",
    "模块级带值注解 a: int = 1": "a: int = 1\n",
    "模块级两条注解": "a: int\nb: list[str]\n",
    "future + 模块级注解": "from __future__ import annotations\na: int\n",
}
for label, src in sorted(CASES.items()):
    table = symtable.symtable(src, "case.py", "exec")
    names = sorted(symbol.get_name() for symbol in table.get_symbols())
    has = "__conditional_annotations__" in names
    print(f"  │ {label:22s} 顶层符号 = {mask(names)} ｜ 里面有开关吗 → {has}")
ns = {}
exec(compile("a1: int\na2: list[str]\n", "casesrc.py", "exec"), ns)
line("① 运行时 globals 里有没有这个键（模块级有注解时）→",
     lambda: "__conditional_annotations__" in ns)
line("② 它的值和类型（两条模块级注解）→", lambda: (ns["__conditional_annotations__"], type(ns["__conditional_annotations__"]).__name__))
line("③ 模块自己那条注解函数的 co_names →", lambda: ns["__annotate__"].__code__.co_names)
line("④ 本脚本自己身上（这个文件没有模块级注解语句）→",
     lambda: ("__conditional_annotations__" in globals(),
              globals().get("__conditional_annotations__", "这个键不存在")))
line("⑤ 没有模块级注解的子进程里能不能看见 →",
     lambda: subprocess.run([sys.executable, "-c",
                             "print('__conditional_annotations__' in globals())"],
                            cwd=str(WORK), env={"LC_ALL": "C.UTF-8", "LANG": "C.UTF-8"},
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, encoding="utf-8").stdout.strip())
print("  → 上面八行把「什么时候有」钉死了：只有**模块级**注解语句（裸的或带值的）会让它出现；")
print("     函数签名、类体、函数体里的注解都不产生它（symtable 只看顶层符号表，正好对上）。")
print("  → ①②③④ 把「谁看得见」钉死：它是模块 globals 里的一个**真键**（dir() 看得见、")
print("     vars() 里排在字母序里），值是 set，每条模块级注解往里加一个下标（两条就是 {0, 1}）；")
print("     读它的人是模块自己那个 __annotate__ 函数 —— ③ 的 co_names 第一个名字就是它。")
print("  → 这就是 future 那行的物理开关：模块 __annotate__ 一进门先看这个集合，")
print("     决定「按 1 号求值」还是「直接给文本」。第 ⑦ 格两份源码的差别，落到这里就是一句话。")
print("  → 顺手记一笔（后面 g 篇的伏笔）：symtable 只看**符号表**，注解表达式里面的名字")
print("     它一个都不给 —— 想知道 Widget 有没有定义，symtable 帮不了你，得走 ast。")
print("  → 断言：False 的四行是「只有赋值」「函数签名带注解」「类体带注解」「函数体带局部注解」，True 的四行是「模块级裸注解」「模块级带值注解」「模块级两条注解」「future + 模块级注解」；① = True、② 逐字 = ({0, 1}, 'set')、③ 逐字 = ('__conditional_annotations__', 'int', 'list', 'str')、④ 逐字 = (False, '这个键不存在')、⑤ 逐字 = 'False'。")

print("\n=== 11. 核对格：把这篇最要紧的读数改成会响的据（前面各格的 │ 行只打印、不判）===")
CELL = {"n": 0, "ok": 0}
WIDGET_ERR = ("NameError", "name 'Widget' is not defined")
print(eq("① 编号牌：四个成员（名字, 值）与 IntEnum 身份",
         lambda: ([(m.name, m.value) for m in Format],
                  (isinstance(Format.VALUE, int), Format.STRING == 4)),
         ([("VALUE", 1), ("VALUE_WITH_FAKE_GLOBALS", 2), ("FORWARDREF", 3), ("STRING", 4)],
          (True, True))))
print(eq("② 公开入口的默认 format 就是 1 号（签名里写着）",
         lambda: str(inspect.signature(annotationlib.get_annotations)),
         "(obj, *, globals=None, locals=None, eval_str=False, format=<Format.VALUE: 1>)"))
print(eq("③ 同一份坏注解插四把钥匙：1 炸、2 拦、3 给 ForwardRef、4 给文本",
         lambda: (fail(lambda: annotationlib.get_annotations(f, format=Format.VALUE)),
                  fail(lambda: annotationlib.get_annotations(f, format=Format.VALUE_WITH_FAKE_GLOBALS)),
                  {k: type(v).__name__ for k, v in
                   annotationlib.get_annotations(f, format=Format.FORWARDREF).items()},
                  annotationlib.get_annotations(f, format=Format.STRING)),
         (WIDGET_ERR,
          ("ValueError", "The VALUE_WITH_FAKE_GLOBALS format is for internal use only"),
          {"x": "ForwardRef", "return": "ForwardRef"},
          {"x": "Widget", "return": "Result"})))
print(eq("④ 2 号不救名字：注解合法才出 dict，非法照样 NameError",
         lambda: (plain_fn.__annotate__(Format.VALUE_WITH_FAKE_GLOBALS),
                  fail(lambda: f.__annotate__(Format.VALUE_WITH_FAKE_GLOBALS))),
         ({"x": int, "y": str, "return": None}, WIDGET_ERR)))
print(eq("⑤ ForwardRef 算 owner 相等，所以不能拿它当键；名字在 __arg__ 里",
         lambda: (fr.__arg__, fr == annotationlib.ForwardRef("Widget"),
                  annotationlib.ForwardRef("Widget") == annotationlib.ForwardRef("Widget"),
                  fr.evaluate(locals={"Widget": int}) is int, fail(lambda: fr.evaluate())),
         ("Widget", False, True, True, WIDGET_ERR)))
print(eq("⑥ 3.14 里 ForwardRef 只有一家：typing 那个是别名，且不是 str 的子类",
         lambda: (annotationlib.ForwardRef is typing.ForwardRef,
                  issubclass(annotationlib.ForwardRef, str),
                  fail(lambda: typing.ForwardRef("int").__evaluate__())),
         (True, False,
          ("AttributeError", "'ForwardRef' object has no attribute '__evaluate__'"))))
print(eq("⑦ 引号写法与裸写法：4 号口径逐字相等，1 号与 FORWARDREF 口径分得开",
         lambda: (annotationlib.get_annotations(bare, format=Format.STRING)
                  == annotationlib.get_annotations(quoted, format=Format.STRING),
                  annotationlib.get_annotations(quoted, format=Format.VALUE),
                  fail(lambda: annotationlib.get_annotations(bare, format=Format.VALUE)),
                  {k: type(v).__name__ for k, v in
                   annotationlib.get_annotations(bare, format=Format.FORWARDREF).items()}),
         (True, {"q": "Missing", "return": None},
          ("NameError", "name 'Missing' is not defined"),
          {"q": "ForwardRef", "return": "NoneType"})))
print(eq("⑧ 别人手里的钥匙默认也插 1 号：get_type_hints 与 signature 一起炸",
         lambda: (fail(lambda: typing.get_type_hints(f)), fail(lambda: inspect.signature(f)),
                  str(inspect.signature(f, annotation_format=Format.STRING))),
         (WIDGET_ERR, WIDGET_ERR, "(x: 'Widget') -> 'Result'")))
print(eq("⑨ annotationlib 的 dir()：8 个声明的、6 个 import 漏进来的模块名",
         lambda: (len(public), leaked),
         (8, ["ast", "builtins", "enum", "keyword", "sys", "types"])))
print(eq("⑩ 三个工具函数：type_repr 给写回源码的样子、annotations_to_string 只吃 dict、"
          "类命名空间里没 __annotate__ 就给 None",
         lambda: (annotationlib.type_repr(list[int]), annotationlib.type_repr(typing.Optional[str]),
                  fail(lambda: annotationlib.annotations_to_string(sample_fn)),
                  annotationlib.get_annotate_from_class_namespace({"__annotations__": {"x": int}})),
         ("list[int]", "str | None",
          ("AttributeError", "'function' object has no attribute 'items'"), None)))
print(ledger("第 1~10 格最要紧的十条读数逐字钉住：编号牌与默认 1 号、四把钥匙各自的下场、"
             "2 号不救名字、ForwardRef 算 owner、typing 只是别名、引号与裸写在 4 号口径无痕、"
             "下游工具默认炸、dir() 的 8+6、三个工具函数的边界"))

print("\n=== 收口：四把钥匙的一句话账本 ===")
print("  ① 编号牌：1 VALUE、2 VALUE_WITH_FAKE_GLOBALS、3 FORWARDREF、4 STRING（第 1 格，IntEnum）。")
print("  ② 对着一份「名字不存在」的注解：1 号 NameError、2 号在公开入口直接 ValueError"
      "（绕过它直接调 __annotate__(2) 也还是 NameError）、3 号给 ForwardRef、4 号给文本 ——")
print("     能救你的只有 3 和 4（第 2 格）；第 ⑦ 格会看到 future 分支下 1、2 号也一起变文本。")
print("  ③ 3 号不是终点：那个 ForwardRef 要 `evaluate(globals=..., locals=...)` 才变成类型，")
print("     而且它算 owner 相等，不能拿来当键（第 3 格）；3.14 里它没有 __evaluate__，只有 evaluate（第 4 格）。")
print("  ④ typing.ForwardRef 就是 annotationlib.ForwardRef（`is` 为 True），它不是 str 的子类（第 4 格）；")
print("     引号写法和裸写法在 4 号口径下逐字相等、无从分辨（第 5 格）。")
print("  ⑤ 别人手里的钥匙默认都插 1 号：typing.get_type_hints、inspect.signature 都会把你的")
print("     程序读崩；signature 在 3.14 有 annotation_format 参数可换口径（第 6 格）。")
print("  ⑥ future 那一行把 1 号也变成文本（不炸了），代价是 3 号失去分辨力、")
print("     而 get_type_hints 照旧炸；它只能开在文件最开头，所以只能开子进程对照（第 7 格）。")
print("  ⑦ annotationlib 的 dir() 里 8 个是它声明的、6 个是 import 顺手漏进来的模块名；")
print("     type_repr 给「写回源码的样子」（list[int] / str | None / __main__.名字，不带路径），")
print("     annotations_to_string 吃 dict，get_annotate_from_class_namespace 只认真 __annotate__（第 8、9 格）。")
print("  ⑧ 模块级注解语句会在 globals 里留下 __conditional_annotations__（一个 set，每条注解一个下标），")
print("     它是 future 开关的落点，symtable 在编译期就能看见它（第 10 格）。")
print("  ★ 三条代价：读注解的默认口径是「会炸的那一把」，凡读必 try/except；")
print("     3 号口径把求值责任推给你自己（evaluate 要给环境）；")
print("     4 号口径 / future 分支给的是文本，任何想拿它当真类型用的下游都得自己 eval，")
print("     于是「加了引号就安全」和「加了 future 就安全」都是错觉。")

shutil.rmtree(WORK)
dirs = sorted(p.name for p in Path.cwd().iterdir() if p.is_dir() and p.name.endswith("-demo"))
pycs = [p.name for p in Path.cwd().rglob("*.pyc")]
caches = [p.name for p in Path.cwd().iterdir() if p.name == "__pycache__"]
print(f"\n  收尾自证：{WORK.name}/ 还在吗 → {WORK.exists()} ｜ 当前目录剩下的 *-demo 目录 → {dirs} ｜"
      f" 剩下的 __pycache__ 目录 → {caches} ｜ 剩下的 .pyc 文件 → {len(pycs)} 个"
      f"（future 那两份临时源码跟着沙箱目录一起删了）")
print(f"  全篇判据合计 {TOTAL['ok']}/{TOTAL['n']} 条成立（少一条上面就有 [不成立！]）")
