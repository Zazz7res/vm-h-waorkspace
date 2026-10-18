#!/usr/bin/env python3
# ============================================================================
# 取证脚本 c_who_reads_annotations.py —— 第 10 章第 3 篇：注解的「值」到底被谁读
#
# 要证明什么：3.14 那句「注解是一段还没执行的代码」的真正含义 ——
#   建 dataclass、建 NamedTuple、建 TypedDict、dataclass 数字段，
#   **全都不需要注解的值**（连注解里写的名字存不存在都不问）；
#   真需要值的只有三种人：① 显式读 __annotations__ / typing.get_type_hints 的代码，
#   ② inspect.signature（它走 VALUE 读法，是本章唯一一个「自己踩雷」的标准库调用），
#   ③ 你自己写的读取器。再加一格 TYPE_CHECKING 的子进程现场：那个 if 省下的
#   不只是一个名字，是**整次 import**（运行时 sys.modules 里根本没有 argparse）。
#
# 为什么要有它：前两篇只演了函数场景，读完容易得出「惰性注解 = 谁都炸不了」。
#   这一篇把「谁还会炸」点名列出来，顺带回答一个实战问题：
#   为什么 `@dataclass class E: a: 拼错的类型` 今天一声不响，
#   明天某个序列化框架读一眼注解就当场 NameError —— 中间隔的不是 bug，是「有没有人读」。
#
# 判据写法：每格用 eq() 把「实测值」和「我手写的预期值」并排印出来，成立/不成立当场见分晓
#   （实测错了就会印 [不成立！]，不存在糊弄人的恒真断言）；每格最后一行汇总几条判据成立。
# 现场：只在当前目录自建的 who-demo/ 里写一个给子进程跑的 .py，收尾 rmtree 删干净。
# 子进程 = [sys.executable, 脚本名] + cwd=沙箱 + 干净 env（只给 PATH 和 locale，
#   PYTHONPATH / PYTHONHOME 一律不继承，免得外部路径混进 sys.modules 那本账）。
# 屏幕上不出现：绝对路径（家目录 / 当前目录 / sys.base_prefix 当场折掉）、对象地址、
#   pid、耗时、随机名；报错原文逐字照抄，只折路径。
# 退出码恒为 0（每格 try/except 把报错当证据）；连跑两遍输出逐字一致。
# 本机 CPython 3.14.4 / Ubuntu。3.13 及更早没有 annotationlib、dataclass 的读取方式也不同，
#   Windows 行为本机未验证 —— 这些都别当事实引用。
# ============================================================================
import annotationlib as A
import dataclasses
import inspect
import os
import shutil
import subprocess
import sys
import typing
from pathlib import Path

WORK = Path("who-demo")
PYNAME = os.path.basename(sys.executable)
CWD = Path.cwd().resolve()
HOME = os.path.expanduser("~")
CELL = {"n": 0, "ok": 0}                       # 每格的判据计数：总数 / 成立数
TOTAL = {"n": 0, "ok": 0}                      # 全篇判据总数 / 成立数（收口那格用它）


def mask(text):
    """报错原文里可能带绝对路径（ImportError 会写出 typing.py 的位置），这里统一折掉。"""
    out = str(text)
    out = out.replace(sys.executable, PYNAME)
    for root, tag in ((str(CWD) + os.sep, ""), (str(CWD), "<cwd>"),
                      (HOME + os.sep, "<家>" + os.sep), (HOME, "<家>"),
                      (sys.base_prefix + os.sep, "<pyroot>" + os.sep),
                      (sys.prefix + os.sep, "<pyroot>" + os.sep)):
        if root and root != os.sep:
            out = out.replace(root, tag)
    return out


def show(tag, th):
    """跑一句，把结果或「报错原文」当一行证据打出来 —— 报错在本章是证据，不是事故。"""
    try:
        return f"  {tag} → {th()!r}"
    except Exception as exc:
        return f"  {tag} → 炸：{type(exc).__name__}: {mask(exc)}"


def eq(tag, th, want):
    """判据行：实测（炸了就取报错原文）与预期逐字比对，两边都印在屏上。"""
    try:
        got = th()
    except Exception as exc:
        got = f"{type(exc).__name__}: {mask(exc)}"
    ok = got == want
    CELL["n"] += 1
    CELL["ok"] += 1 if ok else 0
    TOTAL["n"] += 1
    TOTAL["ok"] += 1 if ok else 0
    return f"  [{'成立' if ok else '不成立！'}] {tag}：实测 {got!r} ｜ 预期 {want!r}"


def ledger(title):
    """每格收口那句：把本格的判据总数/成立数和逐字关键值钉死。"""
    return (f"  → 断言：{title} ｜ 本格判据 {CELL['ok']}/{CELL['n']} 条成立"
            f"（不成立的那条上面会印 [不成立！]）")


def reset():
    CELL["n"] = CELL["ok"] = 0


# ---- 第 5 格给子进程的现场源码：TYPE_CHECKING 的形状 ----
# 单独一个文件、单独一个进程：本进程可能已经 import 过 argparse，
# 只有在干净进程里「'argparse' in sys.modules」这句话才说明问题。
CHILD_SRC = '''"""tc_child.py：`if TYPE_CHECKING: import argparse` 在运行时到底发生了什么。"""
import sys
import typing
import annotationlib as A

if typing.TYPE_CHECKING:
    import argparse          # TYPE_CHECKING 运行时恒 False，所以这一行永远不执行


def f(x: argparse.Namespace) -> None:
    pass


def show(tag, th):
    try:
        print(f"{tag} -> {th()!r}")
    except Exception as exc:
        print(f"{tag} -> ERR {type(exc).__name__}: {exc}")


print("TYPE_CHECKING 的值 =", typing.TYPE_CHECKING)
show("1 定义 f 之后 f.__qualname__", lambda: f.__qualname__)
show("2 STRING 读法", lambda: A.get_annotations(f, format=A.Format.STRING))
show("3 FORWARDREF 读法（只打值的类型名）",
     lambda: {k: type(v).__name__ for k, v in
              A.get_annotations(f, format=A.Format.FORWARDREF).items()})
show("4 VALUE 读法", lambda: A.get_annotations(f, format=A.Format.VALUE))
show("5 typing.get_type_hints(f)", lambda: typing.get_type_hints(f))
print("6 'argparse' in sys.modules ->", "argparse" in sys.modules)
print("7 sys.modules 里含 argparse 字样的键 ->",
      sorted(k for k in sys.modules if "argparse" in k) or "(空)")
'''


def child_hit(out_lines, prefix, want):
    """在子进程输出里找前缀那一行，核对它是否包含 want —— 把「屏上真打了什么」变成判据。"""
    hit = [ln for ln in out_lines if ln.startswith(prefix)]
    return (bool(hit) and want in hit[0]), (hit[0] if hit else "（这行没打出来）")


print("=== 1. dataclass：注解里的名字不存在，类照样建出来 ===")
# 这一格是本章的地板：NotDefinedEver 全宇宙都没有，可 class 语句还是跑完了。
# 读屏的人要记住：dataclass 建类时只需要注解的形状，不需要它的值 ——
# 于是「写错的类型名」在这一刻是完全无声的。
reset()


@dataclasses.dataclass
class E:
    a: NotDefinedEver


print(f"  class E 执行完，屏幕上一个字都没多：E.__qualname__ = {E.__qualname__!r}"
      f" ｜ hasattr(E, '__annotate__') → {hasattr(E, '__annotate__')}"
      f"（惰性注解的钩子挂上了，但没人调用它）")
print(show("E 的字段名", lambda: [f.name for f in dataclasses.fields(E)]))
print(show("fields(E)[0].type 是什么（类场景的 ForwardRef，repr 不带地址）",
           lambda: repr(dataclasses.fields(E)[0].type)))
print(eq("字段名列表", lambda: [f.name for f in dataclasses.fields(E)], ["a"]))
print(eq("E() 的报错原文", lambda: E(),
         "TypeError: E.__init__() missing 1 required positional argument: 'a'"))
print(eq("E(1) 的 repr", lambda: repr(E(1)), "E(a=1)"))
print(eq("读 E.__annotations__ 的报错原文", lambda: E.__annotations__,
         "NameError: name 'NotDefinedEver' is not defined"))
print(eq("typing.get_type_hints(E) 的报错原文", lambda: typing.get_type_hints(E),
         "NameError: name 'NotDefinedEver' is not defined"))
print(ledger("建类与实例化都不需要注解的值，只有「读」的那一下需要"))

print("\n=== 2. 有默认值的字段：默认值当场执行，注解仍然不求值 ===")
# 同一行里两个表达式，命运完全不同：等号右边立刻跑，冒号右边永远不跑。
# 读屏的人要看到「[这一行是默认值表达式...]」出现在 class G 那一瞬，而不是后面某次读取时。
reset()


@dataclasses.dataclass
class G:
    a: NotDefinedEver = print("  [这一行是默认值表达式：class G 一执行它就跑了，"
                              "和注解活不活得起来没有半点关系]")


print(eq("默认值算出来的是 print 的返回值（None）",
         lambda: dataclasses.fields(G)[0].default is None, True))
print(eq("G() 的 repr", lambda: repr(G()), "G(a=None)"))
print(eq("G(7) 的 repr", lambda: repr(G(7)), "G(a=7)"))
print(eq("读 G.__annotations__ 的报错原文", lambda: G.__annotations__,
         "NameError: name 'NotDefinedEver' is not defined"))
print(ledger("class 体里「等号右边」当场执行，「冒号右边」延迟到有人读"))

print("\n=== 3. typing.NamedTuple：建类、实例化全正常，只有读注解炸 ===")
reset()


class NT(typing.NamedTuple):
    x: MissingNT
    y: int = 2


print(f"  class NT 也跑完了：NT.__qualname__ = {NT.__qualname__!r}"
      f" ｜ hasattr(NT, '__annotate__') → {hasattr(NT, '__annotate__')}")
print(eq("_fields", lambda: NT._fields, ("x", "y")))
print(eq("_field_defaults", lambda: NT._field_defaults, {"y": 2}))
print(eq("NT(1) 的 repr", lambda: repr(NT(1)), "NT(x=1, y=2)"))
print(eq("NT(1).y（默认值是真用上了）", lambda: NT(1).y, 2))
print(eq("读 NT.__annotations__ 的报错原文", lambda: NT.__annotations__,
         "NameError: name 'MissingNT' is not defined"))
print(eq("typing.get_type_hints(NT) 的报错原文", lambda: typing.get_type_hints(NT),
         "NameError: name 'MissingNT' is not defined"))
print(ledger("NamedTuple 建类和实例化同样不求值注解；_fields 只靠名字"))

print("\n=== 4. typing.TypedDict：__required_keys__ 有值，__annotations__ 炸 ===")
reset()


class TD(typing.TypedDict):
    a: MissingTD


print(f"  class TD 跑完：TD.__qualname__ = {TD.__qualname__!r}（这一句之前没人读过它的注解）")
print(eq("TD.__required_keys__ == frozenset({'a'})（写成比较，不靠 repr 顺序）",
         lambda: TD.__required_keys__ == frozenset({"a"}), True))
print(eq("TD.__optional_keys__ == frozenset()", lambda: TD.__optional_keys__ == frozenset(), True))
print(eq("TD 其实就是个 dict 工厂：TD(a=1)", lambda: repr(TD(a=1)), "{'a': 1}"))
print(eq("读 TD.__annotations__ 的报错原文", lambda: TD.__annotations__,
         "NameError: name 'MissingTD' is not defined"))
print(eq("typing.get_type_hints(TD) 的报错原文", lambda: typing.get_type_hints(TD),
         "NameError: name 'MissingTD' is not defined"))
print(ledger("TypedDict 的键账本照样建起来了，炸的只有「读注解」")
      )

print("\n=== 5. if TYPE_CHECKING: import argparse —— 省下的是整次 import ===")
# 这一格必须换进程演（现场在 who-demo/tc_child.py）。读屏的人盯三行：
# STRING 读法拿到逐字全文、VALUE 读法炸、'argparse' 压根不在 sys.modules 里。
reset()
shutil.rmtree(WORK, ignore_errors=True)
WORK.mkdir(parents=True, exist_ok=True)
(WORK / "tc_child.py").write_text(CHILD_SRC, encoding="utf-8")
child_env = {"PATH": os.environ.get("PATH", ""), "LC_ALL": "C.UTF-8", "LANG": "C.UTF-8",
             "PYTHONIOENCODING": "utf-8"}          # 干净 env：没有 PYTHONPATH / PYTHONHOME
r = subprocess.run([sys.executable, "tc_child.py"], cwd=str(WORK), env=child_env,
                   capture_output=True, text=True)
lines = r.stdout.splitlines()
for ln in lines:
    print(f"  │ {mask(ln)}")
print(f"  └ 子进程退出码 = {r.returncode} ｜ stderr 行数 = {len(r.stderr.splitlines())}")
ok2, raw2 = child_hit(lines, "2 ", "'x': 'argparse.Namespace', 'return': 'None'")
ok3, raw3 = child_hit(lines, "3 ", "'x': 'ForwardRef', 'return': 'NoneType'")
ok4, raw4 = child_hit(lines, "4 ", "NameError: name 'argparse' is not defined")
ok6, raw6 = child_hit(lines, "6 ", "False")
print(eq("子进程第 2 行（STRING 读法拿到全文）", lambda: ok2, True))
print(eq("子进程第 3 行（FORWARDREF：值退化成 ForwardRef）", lambda: ok3, True))
print(eq("子进程第 4 行（VALUE 读法：NameError）", lambda: ok4, True))
print(eq("子进程第 6 行（'argparse' in sys.modules 是 False）", lambda: ok6, True))
print(eq("子进程退出码", lambda: r.returncode, 0))
print(f"  ★ 注解里那个名字「运行时不存在」和「运行时没 import 那个模块」是同一件事的两面："
      f"逐字 = {raw6!r}")
print(ledger("TYPE_CHECKING 是运行时省下的那次 import，代价是 VALUE 读法当场 NameError"))

print("\n=== 6. inspect.signature：唯一一个自己踩雷的标准库调用 ===")
# 前面 1~4 格「读注解」都是我们主动去读的。这一格不是：
# signature 只想给你一份参数表，可它顺手把 __annotations__ 按 VALUE 读了一遍 —— 于是它炸了。
# 同一个函数、同一个时刻，STRING 读法却能把全文拿到手。这是本章最实用的一组对比。
reset()


def q(a: int, b: Missing2 = None, *, c: "Missing3" = 1) -> Missing4:
    pass


print(f"  函数 q 定义成功：q.__qualname__ = {q.__qualname__!r} ｜ hasattr(q, '__signature__') → "
      f"{hasattr(q, '__signature__')}")
print(eq("str(inspect.signature(q)) 的报错原文", lambda: str(inspect.signature(q)),
         "NameError: name 'Missing2' is not defined"))
print(eq("换参数也逃不掉：signature(q, eval_str=False)",
         lambda: str(inspect.signature(q, eval_str=False)),
         "NameError: name 'Missing2' is not defined"))
print(eq("STRING 读法：同一个函数，逐字全文",
         lambda: A.get_annotations(q, format=A.Format.STRING),
         {"a": "int", "b": "Missing2", "c": "Missing3", "return": "Missing4"}))
print(eq("FORWARDREF 读法里每个值的类型名",
         lambda: {k: type(v).__name__ for k, v in
                  A.get_annotations(q, format=A.Format.FORWARDREF).items()},
         {"a": "type", "b": "ForwardRef", "c": "str", "return": "ForwardRef"}))
print(eq("hasattr(q, '__signature__')", lambda: hasattr(q, "__signature__"), False))
print(show("STRING 读过一次之后 signature 是否就好了（两种读法不共享缓存）",
           lambda: str(inspect.signature(q))))
# ★ 本机量出来的绕法（事实基线里没写）：signature 只认 __annotations__，
#   所以自己先用 STRING 读法把「文本」写回去，它就肯干活了 —— 注解变成带引号的字符串。
q.__annotations__ = A.get_annotations(q, format=A.Format.STRING)
print(eq("把 STRING 读法的结果手动写回 q.__annotations__，再 signature",
         lambda: str(inspect.signature(q)),
         "(a: 'int', b: 'Missing2' = None, *, c: 'Missing3' = 1) -> 'Missing4'"))
print(ledger("同一个函数：signature 炸、STRING 读法不炸；带引号的注解在 FORWARDREF 里仍是 str"))

print("\n=== 7. dataclass 认 ClassVar 靠什么：靠能算出来的那个值 ===")
# ClassVar 是「不进 __init__」的特殊字段，dataclass 必须认出它。它用的是 FORWARDREF 读法
# （能算的先算，算不出的留成 ForwardRef），所以：
#   写法A `ClassVar[str]`（从 typing 导入）→ 认出来；写法B 带前缀 → 也认出来；
#   陷阱  写了 ClassVar 却忘了导入 → 算不出值、只剩 ForwardRef → **认不出来**，
#         于是它静默变成普通字段，__init__ 多收一个参数，全程没有任何报错。
reset()
from typing import ClassVar                    # 这一行是写法A/B 能成立的前提


@dataclasses.dataclass
class CV:
    a: NotDefinedEver
    b: ClassVar[str] = '写法A：从 typing 导入'
    c: typing.ClassVar[str] = '写法B：带前缀'


@dataclasses.dataclass
class Trap:
    a: int
    b: GhostClassVar[str] = '这个名字没人导入过'


print(eq("fields(CV)：只剩 a（b、c 都被认成 ClassVar）",
         lambda: [f.name for f in dataclasses.fields(CV)], ["a"]))
print(eq("CV.b / CV.c 变成类属性", lambda: (CV.b, CV.c),
         ('写法A：从 typing 导入', '写法B：带前缀')))
print(eq("CV(1) 的 repr（__init__ 只收一个参数）", lambda: repr(CV(1)), "CV(a=1)"))
print(eq("陷阱版 fields(Trap)：b 被当成普通字段",
         lambda: [f.name for f in dataclasses.fields(Trap)], ["a", "b"]))
print(eq("Trap(1, '共享') 照样实例化（__init__ 真收了两个参数）",
         lambda: repr(Trap(1, '共享')), "Trap(a=1, b='共享')"))
print(eq("Trap 的 FORWARDREF 读法里每个值的类型名",
         lambda: {k: type(v).__name__ for k, v in
                  A.get_annotations(Trap, format=A.Format.FORWARDREF).items()},
         {"a": "type", "b": "ForwardRef"}))
print(eq("读 CV.__annotations__ 的报错原文", lambda: CV.__annotations__,
         "NameError: name 'NotDefinedEver' is not defined"))
print(ledger("ClassVar 的识别 = 「这个注解能不能算出值」，算不出就当普通字段，静默"))

print("\n=== 8. InitVar 不在 typing 里 —— 报错原文一条字都别改 ===")
# 上一条讲 ClassVar，这条讲它的兄弟 InitVar：很多人以为它和 ClassVar 一样在 typing 里。
# 实测：不在，它在 dataclasses 里，而且是个「对象」不是「类型」。
reset()
INITVAR_EXPECT = ("ImportError: cannot import name 'InitVar' from 'typing' "
                  "(<pyroot>/lib/python3.14/typing.py)")
try:
    from typing import InitVar                  # noqa: F401  ← 故意踩一下
    INITVAR_GOT = "居然没炸"
except Exception as exc:
    INITVAR_GOT = f"{type(exc).__name__}: {mask(exc)}"
print(f"  from typing import InitVar → {INITVAR_GOT}")
print(eq("上面这行与预期逐字相同（路径已折成 <pyroot>）", lambda: INITVAR_GOT, INITVAR_EXPECT))
print(eq("typing 里有 InitVar 这个名字吗", lambda: hasattr(typing, "InitVar"), False))
print(eq("dataclasses.InitVar[int] 是什么东西", lambda: repr(dataclasses.InitVar[int]),
         "dataclasses.InitVar[int]"))


@dataclasses.dataclass
class CI:
    a: NotDefinedEver
    tmp: dataclasses.InitVar[int] = 0

    def __post_init__(self, tmp):
        self.real = tmp


print(eq("fields(CI)（InitVar 也不算 field）",
         lambda: [f.name for f in dataclasses.fields(CI)], ["a"]))
print(eq("CI(1, 2).real（InitVar 的值交给 __post_init__，实例上不留）",
         lambda: CI(1, 2).real, 2))
print(eq("CI(1, 2) 的 repr", lambda: repr(CI(1, 2)), "CI(a=1)"))
print(eq("读 CI.__annotations__ 的报错原文", lambda: CI.__annotations__,
         "NameError: name 'NotDefinedEver' is not defined"))
print(ledger("InitVar 的正确出处是 dataclasses；typing 里没这个名字（报错原文已逐字核对）"))

print("\n=== 9. 收口：谁真的需要注解的值 —— 一句话账本 ===")
print("  ① 建类不求值：dataclass / NamedTuple / TypedDict 只要注解的形状"
      "（叫什么名、有没有默认值），1~4 格建类全部成功、实例化也全部成功。")
print("  ② 类体里等号右边的默认值**当场执行**（第 2 格那行「[这一行是默认值表达式...]」），"
      "冒号右边的注解延迟到有人读 —— 一句话：默认值是代码，注解是「还没执行的代码」。")
print("  ③ 真需要值的只有读的那一下：__annotations__、typing.get_type_hints、"
      "get_annotations(format=VALUE)、inspect.signature —— 这四个在本机全部炸出 NameError。")
print("  ④ 不炸的读法有两种：STRING 拿逐字全文（第 5、6 格），FORWARDREF 拿到 ForwardRef 对象继续干活；"
      "源码里带引号的那条注解在 FORWARDREF 里本来就是 str（第 6 格 'c': 'str'）。")
print("  ⑤ TYPE_CHECKING 省下的是**整次 import**：'argparse' in sys.modules → False（第 5 格），"
      "它是运行时性能与循环依赖的解药，同时也是运行时读 VALUE 的炸弹。")
print("  ⑥ dataclass 认 ClassVar 用的是「能不能算出值」（第 7 格：导入的两种写法都认，"
      "忘导入的那个静默变成普通字段）；InitVar 在 dataclasses 不在 typing（第 8 格）。")
print("  ★ 代价：类型写错不报错 ≠ 类型写对了。上面每一行「成功」都只在说「还没有人读它」；")
print("     本机没有静态检查器（本章 g 篇实测 mypy/pyright 都不在场），")
print("     这个时间差只能靠「自己按 STRING/FORWARDREF 读一遍」或「自己写个检查器」来补。")

shutil.rmtree(WORK, ignore_errors=True)
left = sorted(p.name for p in Path.cwd().iterdir() if p.is_dir() and p.name.endswith("-demo"))
print(f"\n  收尾自证：{WORK.name}/ 还在吗 → {WORK.exists()}"
      f" ｜ 当前目录剩下的 *-demo 目录 → {left}（本篇只创建/删除 {WORK.name}/ 这一个，"
      f"列出来的是别的脚本的现场；__pycache__ 跟着沙箱一起没了）")
print(f"  → 断言：{WORK.name}/ 已删除 = {WORK.exists() is False}"
      f" ｜ 本篇 1~8 格判据 {TOTAL['ok']}/{TOTAL['n']} 条成立（少一条上面就有 [不成立！]）"
      f" ｜ 本篇写过的目录只有 {WORK.name}/ 一个，它现在存在吗 → {WORK.exists()}")
print(f"  全篇判据合计 {TOTAL['ok']}/{TOTAL['n']} 条成立（少一条上面就有 [不成立！]）")
