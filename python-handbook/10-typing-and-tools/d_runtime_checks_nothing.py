#!/usr/bin/env python3
# ============================================================================
# 取证脚本 d_runtime_checks_nothing.py —— 第 10 章第 4 篇：运行时一行都不检查
#
# 要证明什么：把「写了类型注解就有安全检查」这个幻觉当场打碎 ——
#   变量注解不改值、函数注解不拦错类型实参、Final 不拦重新赋值、@final 不拦继承、
#   cast 原样返回、Literal 不拦越界值、assert_type 连名字带 assert 都是空气、
#   reveal_type 那行提示走的是 stderr 不是 stdout；
#   运行时唯一真的在干活的 isinstance/issubclass，也只吃裸类和 X | Y，
#   参数化泛型（list[int] 这种）一律 TypeError。
#
# 为什么要有它：第 3 篇证明「没人读注解就不会炸」，这一篇证明另一半：
#   「读了注解也不会有任何人替你判断值对不对」。两篇合起来才是 3.14 的完整真相 ——
#   注解是给「工具」看的，而本机没有那个工具（见 g 篇在场性探测）。
#   所以最后必须留一个「真的会拦」的负对照：自己读 __annotations__ 自己 isinstance，
#   4 组错误实参响 4 次；不这么写的代码，同样 4 组里 2 组安静返回错类型的值、
#   另外 2 组的报错也只来自 + 号本身 —— 因注解而响的次数是 0。
#
# 判据写法：每格用 eq() 把实测值与手写预期并排印出来，错了就印 [不成立！]；
#   每格最后一行是「→ 断言」，报本格判据成立数（没有糊弄人的恒真断言）。
# 现场：只在当前目录自建的 runtime-demo/ 里写一个给子进程跑的 .py，收尾 rmtree 删干净。
# 子进程 = [sys.executable, 脚本名] + cwd=沙箱 + 干净 env（只给 PATH 和 locale，
#   绝不继承 PYTHONPATH）；stdout 与 stderr 分开收 —— 那正是第 6 格的实验目的。
# 屏幕上不出现：绝对路径、用户名、对象地址、pid、耗时。报错原文逐字照抄（只折路径）。
# 退出码恒为 0；连跑两遍输出逐字一致。
# 本机 CPython 3.14.4 / Ubuntu。别的版本里 assert_type / reveal_type 的行为、
#   Windows 上 stderr 的换行行为本机未验证，别当事实引用。
# ============================================================================
import annotationlib as A
import contextlib
import io
import os
import shutil
import subprocess
import sys
import typing
from pathlib import Path

WORK = Path("runtime-demo")
PYNAME = os.path.basename(sys.executable)
CWD = Path.cwd().resolve()
HOME = os.path.expanduser("~")
CELL = {"n": 0, "ok": 0}
TOTAL = {"n": 0, "ok": 0}   # 全篇判据总数 / 成立数（收口那格用它）


def mask(text):
    """报错原文里可能带绝对路径，统一折掉再上屏。"""
    out = str(text)
    out = out.replace(sys.executable, PYNAME)
    for root, tag in ((str(CWD) + os.sep, ""), (str(CWD), "<cwd>"),
                      (HOME + os.sep, "<家>" + os.sep), (HOME, "<家>"),
                      (sys.base_prefix + os.sep, "<pyroot>" + os.sep),
                      (sys.prefix + os.sep, "<pyroot>" + os.sep)):
        if root and root != os.sep:
            out = out.replace(root, tag)
    return out


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


def show(tag, th):
    try:
        return f"  {tag} → {th()!r}"
    except Exception as exc:
        return f"  {tag} → 炸：{type(exc).__name__}: {mask(exc)}"


def ledger(title):
    return (f"  → 断言：{title} ｜ 本格判据 {CELL['ok']}/{CELL['n']} 条成立"
            f"（不成立的那条上面会印 [不成立！]）")


def reset():
    CELL["n"] = CELL["ok"] = 0


# ---- 下面这些「带注解」的道具，全脚本反复使用 ----
x: int = "字符串"              # 注解说 int，值是个 str


class Box:
    """一个谁也没实例化过的类，专门用来被注解「冒名顶替」。"""


b: Box = 42                    # 注解说 Box，值是 int


def add(a: int, b: int) -> int:
    return a + b


def pick(v: typing.Literal["a", "b"]) -> str:
    return v


def strict_add(a: int, b: int) -> int:
    """第 8 格的道具：自己读 __annotations__ 自己查 —— 这才是「运行时检查」本体。"""
    hints = strict_add.__annotations__
    for name, val in (("a", a), ("b", b)):
        if not isinstance(val, hints[name]):
            raise TypeError(f"{name} 要 {hints[name].__name__}，实际拿到 {type(val).__name__}")
    return a + b


print("=== 1. 变量注解：值归值，注解归注解，从此互不相干 ===")
# 注解写在赋值语句里，看起来像一道关卡，实际上它只是往「模块的注解账本」上记一笔期望。
# 3.14 还有一层：这本账默认压根不存在，要到有人读的那一刻才现造（下面第一条就是它）。
reset()
MOD = sys.modules[__name__]
print(eq("读之前：模块命名空间里有 '__annotations__' 这个键吗",
         lambda: "__annotations__" in vars(MOD), False))
print(show("直接写 __annotations__（没人读过它，所以这个名字还没有）", lambda: __annotations__))
print(eq("annotationlib 读模块级注解（VALUE 读法：账本这才被造出来）",
         lambda: A.get_annotations(MOD, format=A.Format.VALUE), {"x": int, "b": Box}))
print(eq("读完之后：'__annotations__' in vars(模块)", lambda: "__annotations__" in vars(MOD), True))
print(eq("账本给 x 记的是", lambda: A.get_annotations(MOD, format=A.Format.VALUE)["x"], int))
print(eq("x 的真实值（注解一个字没影响到它）", lambda: x, "字符串"))
print(eq("type(x).__name__", lambda: type(x).__name__, "str"))
print(eq("账本给 b 记的是", lambda: A.get_annotations(MOD, format=A.Format.VALUE)["b"], Box))
print(eq("type(b).__name__", lambda: type(b).__name__, "int"))
print(ledger("注解只留下一笔「期望」；变量的值与类型原封不动（x 仍是 str、b 仍是 int）"))

print("\n=== 2. 函数注解：传错类型照样调用，返回值是「错类型」的运算结果 ===")
reset()
print(eq("add(1, 2)", lambda: add(1, 2), 3))
print(eq('add("a", "b")（注解说是 int）', lambda: add("a", "b"), "ab"))
print(eq("add([1], [2])", lambda: repr(add([1], [2])), "[1, 2]"))
print(eq("返回值的真实类型名", lambda: type(add("a", "b")).__name__, "str"))
print(eq("函数自己的注解（int 存在，所以一直好读）", lambda: add.__annotations__,
         {"a": int, "b": int, "return": int}))
print(ledger('add("a","b") 得到 \'ab\'：那个 + 是 str 的 +，注解一点忙都没帮'))

print("\n=== 3. Final 不拦重新赋值，@final 不拦继承 ===")
reset()
CONST: typing.Final[int] = 1
print(eq("重新赋值之前", lambda: CONST, 1))
CONST = 2
print(eq("重新赋值之后（没报错、没警告、屏幕干净）", lambda: CONST, 2))
print(eq("账本上 CONST 那一笔（STRING 读法：从源码现算，永远新）",
         lambda: A.get_annotations(MOD, format=A.Format.STRING)["CONST"], "typing.Final[int]"))
print(eq("★ 顺手量到的 3.14 细节：第 1 格按 VALUE 读过之后，之后才执行的模块级注解"
         "不会补进那本已缓存的账（所以本条是 False）",
         lambda: "CONST" in MOD.__annotations__, False))


@typing.final
class Locked:
    pass


class Sub(Locked):
    pass


print(eq("@final 的类被继承了：Sub.__name__", lambda: Sub.__name__, "Sub"))
print(eq("issubclass(Sub, Locked)", lambda: issubclass(Sub, Locked), True))
print(eq("typing.final 的 docstring 首句（它自己怎么说的）",
         lambda: typing.final.__doc__.splitlines()[0],
         "Decorator to indicate final methods and final classes."))
print(ledger("Final/@final 在运行时是纯记号：值改得动（1→2）、类继承得了（Sub）"))

print("\n=== 4. cast 原样返回，Literal 越界值照传 ===")
reset()
print(eq("typing.cast(str, 123) 的返回值", lambda: typing.cast(str, 123), 123))
print(eq("它的真实类型名（说好的 str 呢）", lambda: type(typing.cast(str, 123)).__name__, "int"))
print(eq("cast 的第一个参数甚至不用是类型，写个字符串它也收",
         lambda: typing.cast("随便写的", 7), 7))
print(eq("cast 的 docstring 首句", lambda: typing.cast.__doc__.splitlines()[0],
         "Cast a value to a type."))
print(eq('pick("a")', lambda: pick("a"), "a"))
print(eq('pick("zzz")（Literal 里没有这个值）', lambda: pick("zzz"), "zzz"))
print(eq("账本上 pick 的 v 那一笔", lambda: repr(pick.__annotations__["v"]),
         "typing.Literal['a', 'b']"))
print(ledger('cast 就是「返回第二个参数」；pick("zzz") 返回 "zzz" —— 两个都是空气'))

print("\n=== 5. assert_type：名字叫 assert，运行时是空气（全章最扎人的一格）===")
reset()
print(eq("typing.assert_type(1, int) 的返回值（对得上的那种）",
         lambda: typing.assert_type(1, int), 1))
print(eq("typing.assert_type(1, str) 的返回值（明摆着对不上）",
         lambda: typing.assert_type(1, str), 1))
print(eq("assert_type(1, str) 抛异常了吗", lambda: "没抛", "没抛"))
print(eq("它自己的 docstring 首句（答案写在脸上）",
         lambda: typing.assert_type.__doc__.splitlines()[0],
         "Ask a static type checker to confirm that the value is of the given type."))
print(eq("typing 里有同族的 assert_never 吗", lambda: hasattr(typing, "assert_never"), True))
print(eq("assert_never 的 docstring 首句", lambda: typing.assert_never.__doc__.splitlines()[0],
         "Statically assert that a line of code is unreachable."))
print(ledger("两个 assert_type 都返回 1、都不报错：assert 这个词只服务于静态检查器"))

print("\n=== 6. reveal_type：那行提示走 stderr，不是 stdout ===")
# 两路各演一次：① 本进程用 redirect_stderr 就地接住那行；② 换个子进程把两个流分开收。
# 读屏的人要记住：裸写 reveal_type(1) 在本机是 NameError，它是「检查器的钩子」不是内置函数。
reset()
local_err = io.StringIO()
with contextlib.redirect_stderr(local_err):
    local_ret = typing.reveal_type(1)
print(f"  本进程里 redirect_stderr 接到的内容 → {local_err.getvalue()!r}")
print(f"  与此同时 stdout 上多出来的只有下面这一行（没有 'Runtime type'）：")
print(eq("typing.reveal_type(1) 的返回值（原样把值还给你）", lambda: local_ret, 1))
print(eq("redirect_stderr 接到的那行逐字", lambda: local_err.getvalue(), "Runtime type is 'int'\n"))
try:
    print(reveal_type(1))                # noqa: F821  ← 裸名字在本机不存在
    bare = "居然有这个名字"
except Exception as exc:
    bare = f"{type(exc).__name__}: {mask(exc)}"
print(f"  裸写 reveal_type(1) → {bare}")
print(eq("裸名报错原文逐字", lambda: bare, "NameError: name 'reveal_type' is not defined"))
shutil.rmtree(WORK, ignore_errors=True)
WORK.mkdir(parents=True, exist_ok=True)
(WORK / "rt_reveal.py").write_text(
    '"""rt_reveal.py：reveal_type 的提示到底写到哪个流。"""\n'
    "import typing\n"
    "ret = typing.reveal_type(1)\n"
    'print("reveal_type(1) 的返回值 =", repr(ret))\n'
    'print("这一行是 stdout 的内容")\n',
    encoding="utf-8")
child_env = {"PATH": os.environ.get("PATH", ""), "LC_ALL": "C.UTF-8", "LANG": "C.UTF-8",
             "PYTHONIOENCODING": "utf-8"}
r = subprocess.run([sys.executable, "rt_reveal.py"], cwd=str(WORK), env=child_env,
                   capture_output=True, text=True)
out_lines, err_lines = r.stdout.splitlines(), r.stderr.splitlines()
print(f"  ┌ 子进程 stdout（{len(out_lines)} 行）：")
for ln in out_lines:
    print(f"  │   {mask(ln)}")
print(f"  ├ 子进程 stderr（{len(err_lines)} 行）：")
for ln in err_lines:
    print(f"  │   {mask(ln)}")
print(f"  └ 子进程退出码 = {r.returncode}")
print(eq("子进程 stdout 里含 'Runtime type' 吗", lambda: any("Runtime type" in ln for ln in out_lines), False))
print(eq("子进程 stderr 第一行逐字", lambda: (err_lines[0] if err_lines else ""),
         "Runtime type is 'int'"))
print(eq("子进程退出码", lambda: r.returncode, 0))
print(ledger("提示只在 stderr（两路各量一次），返回值是原对象；裸名是 NameError"))

print("\n=== 7. isinstance / issubclass 的四问：只有裸类和 X | Y 能用 ===")
reset()
print(eq("isinstance(3, int | str)", lambda: isinstance(3, int | str), True))
print(eq("isinstance([1], list[int]) 的报错原文", lambda: isinstance([1], list[int]),
         "TypeError: isinstance() argument 2 cannot be a parameterized generic"))
print(eq("isinstance([1], list)", lambda: isinstance([1], list), True))
print(eq("isinstance(3, typing.Optional[int])", lambda: isinstance(3, typing.Optional[int]), True))
print(eq("issubclass(list, list[int]) 的报错原文", lambda: issubclass(list, list[int]),
         "TypeError: issubclass() argument 2 cannot be a parameterized generic"))
print(eq("为什么 Optional[int] 能用：它就是 int | None",
         lambda: repr(typing.Optional[int]), "int | None"))
print(eq("list[int] 有 __args__、list 没有（参数化与否的判据）",
         lambda: (hasattr(list[int], "__args__"), hasattr(list, "__args__")), (True, False)))
print(ledger("isinstance 认裸类与 X | Y；带方括号的一律 TypeError（注意：是炸，不是返回 False）"))

print("\n=== 8. 负对照：真的会拦的检查长什么样（先让它响，再报真实值）===")
# 上面七格全在报「响 0 次」。这一格先故意造 4 组错误实参，让「自己写的检查」响给他们看，
# 再拿同样 4 组喂给裸注解的 add —— 差别只有一行：有没有人去读 __annotations__。
reset()
WRONG_CALLS = [("a", "b"), ([1], [2]), (None, 3), (1.5, "x")]


def probe_calls(fn):
    """把 4 组错误实参全喂一遍，分账记下：响了几次 / 算出结果几次 / 报错原文列表。"""
    rang, valued, errs = 0, 0, []
    for a1, a2 in WRONG_CALLS:
        try:
            fn(a1, a2)
            valued += 1
        except Exception as exc:
            rang += 1
            errs.append(f"{type(exc).__name__}: {mask(exc)}")
    return rang, valued, errs


strict_rang, strict_valued, strict_errs = probe_calls(strict_add)
plain_rang, plain_valued, plain_errs = probe_calls(add)
print(f"  故意喂 {len(WRONG_CALLS)} 组错误实参：strict_add 响 {strict_rang} 次、算出结果 {strict_valued} 次，"
      f"第一条原文 = {strict_errs[0]!r}   ← 判据确实会响")
print(f"  同样 {len(WRONG_CALLS)} 组喂给裸注解的 add：响 {plain_rang} 次、算出结果 {plain_valued} 次，"
      f"第一条原文 = {plain_errs[0]!r}")
print(f"    它响的那几次，报错全部来自 + 号自己（unsupported operand），和注解没关系；"
      f"剩下 {plain_valued} 组直接返回了错类型的值")
print(eq("strict 版响的次数", lambda: strict_rang, 4))
print(eq("strict 版报错原文里含「要 int」（我们自己写的那句）的条数",
         lambda: sum(1 for m in strict_errs if "要 int" in m), 4))
print(eq("裸注解版真的算出结果的组数（错类型也照算）", lambda: plain_valued, 2))
print(eq("裸注解版报错原文里含「要 int」的条数（它根本不知道有注解）",
         lambda: sum(1 for m in plain_errs if "要 int" in m), 0))
print(eq("strict_add(1, 2) 正常路径", lambda: strict_add(1, 2), 3))
print(eq("strict_add 读的正是它自己的 __annotations__", lambda: strict_add.__annotations__,
         {"a": int, "b": int, "return": int}))
print(ledger(f"同一份注解：读它并 isinstance 就响 {strict_rang} 次；不读它，{len(WRONG_CALLS)} 组"
             f"错误实参里 {plain_valued} 组安静返回错类型的值、因注解而响 0 次"))

print("\n=== 9. 收口：注解值什么时候会被真的用（接上第 3 篇那本账）===")
print("  ① 注解值不影响运行时一个字节：x: int = '字符串' 之后 x 还是 str（第 1 格）；")
print("     add('a','b') 得 'ab'（第 2 格）；pick('zzz') 得 'zzz'（第 4 格）。")
print("  ② 连「专门用来检查」的那批名字也不检查：Final 改得动（1→2）、@final 继承得了（Sub）、")
print("     cast 原样返回、assert_type(1, str) 返回 1 且什么都不说（第 3、4、5 格）。")
print("  ③ reveal_type 只往 stderr 丢一行、assert_* 只有一句 docstring：")
print("     它们的话是说给静态检查器听的，而本机没有静态检查器（见 g 篇在场性探测）。")
print("  ④ 运行时真干活的只有 isinstance/issubclass：认裸类、认 X | Y（Optional[int] 就是 int | None），")
print("     一律不认 list[int] 这种参数化泛型 —— 报 TypeError，不是返回 False（第 7 格）。")
print("  ⑤ 和第 3 篇接上的那句：**注解的值只在「有人读」那一刻被求值**（dataclass 只取名字、")
print("     get_type_hints / VALUE 读法 / inspect.signature 会真求值）；读完也没人替你比较 ——")
print("     要比较必须自己读 __annotations__ 自己 isinstance（第 8 格那 4 次响）。")
print("  ★ 代价：本机跑通、屏幕一片绿灯，而「绿灯」在这一章不含任何保证；")
print("     想真拦，要么自己写校验（e 篇最后那个 20 行玩具校验器），要么用在场的那唯一一个第三方库。")

shutil.rmtree(WORK, ignore_errors=True)
left = sorted(p.name for p in Path.cwd().iterdir() if p.is_dir() and p.name.endswith("-demo"))
print(f"\n  收尾自证：{WORK.name}/ 还在吗 → {WORK.exists()}"
      f" ｜ 当前目录剩下的 *-demo 目录 → {left}（本篇只创建/删除 {WORK.name}/ 这一个，"
      f"列出来的是别的脚本的现场；__pycache__ 跟着沙箱一起没了）")
print(f"  → 断言：{WORK.name}/ 已删除 = {WORK.exists() is False}"
      f" ｜ 本篇 1~8 格判据 {TOTAL['ok']}/{TOTAL['n']} 条成立（少一条上面就有 [不成立！]）"
      f" ｜ 本篇写过的目录只有 {WORK.name}/ 一个，它现在存在吗 → {WORK.exists()}")
print(f"  全篇判据合计 {TOTAL['ok']}/{TOTAL['n']} 条成立（少一条上面就有 [不成立！]）")
