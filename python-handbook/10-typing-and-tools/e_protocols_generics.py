#!/usr/bin/env python3
# ============================================================================
# 取证脚本 e_protocols_generics.py —— 第 10 章第 5 篇：Protocol 只查名字，get_origin 才是真干活的
#
# 要证明什么：本章最后两件事。
#   ① @runtime_checkable 的 Protocol 是「查名字不查签名」：方法签名差十万八千里，
#      isinstance 照样 True；它其实就等价于 hasattr。没加这个标记的 Protocol 连 hasattr 都不给你用。
#   ② get_origin / get_args 是标准库里唯一真的在「读懂注解」的那批函数：
#      它们把 list[int]、dict[str,int]、X | Y、Literal[...] 拆回「容器 + 元素」，
#      于是可以自己搭一个最小运行时候验器 —— 这正是「本机没有 mypy 时怎么活」的主线。
#
# 为什么要有它：第 3 篇说「没人读注解就不炸」，第 4 篇说「读了也没人替你比较」。
#   这一篇给出路：想比较，就得自己按 origin/args 拆；而 Protocol 那条看起来最像
#   「运行时接口检查」的路，实际上只认名字 —— 用它当安全网会漏掉整类事故。
#   所以 Protocol 那格配一个负对照：isinstance 说 True 之后，真按协议调用当场 TypeError。
#
# 判据写法：每格用 eq() 把实测值与手写预期并排印出来，错了就印 [不成立！]；每格末行「→ 断言」。
# 现场：proto-demo/ 只放一个子进程脚本（第 4 格用它证明「没加标记的 Protocol 报错原文」，
#   避开本进程已经打标记的类），收尾 rmtree 删干净；子进程 = [sys.executable, ...] +
#   cwd=沙箱 + 干净 env（只给 PATH 和 locale，绝不继承 PYTHONPATH）。
# 屏幕上不出现：绝对路径、用户名、对象地址、pid、耗时；报错原文逐字照抄（只折路径）。
# 退出码恒为 0；连跑两遍输出逐字一致（表格逐行印、frozenset 一律写成比较而不是看 repr）。
# 本机 CPython 3.14.4 / Ubuntu。3.11~3.13 的 Protocol/typing 细节与此不同，
#   Windows 未验证 —— 都别当事实引用。
# ============================================================================
import collections.abc
import inspect
import os
import shutil
import subprocess
import sys
import typing
from pathlib import Path
from typing import (Annotated, Any, Callable, Literal, Protocol, Tuple, TypeVar,
                    get_args, get_origin, runtime_checkable)

WORK = Path("proto-demo")
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


def ledger(title):
    return (f"  → 断言：{title} ｜ 本格判据 {CELL['ok']}/{CELL['n']} 条成立"
            f"（不成立的那条上面会印 [不成立！]）")


def reset():
    CELL["n"] = CELL["ok"] = 0


# ---- Protocol 现场：一个「会说」的协议 + 三个真假冒牌货 ----
@runtime_checkable
class Speaks(Protocol):
    def speak(self, loud: bool = False) -> str:
        ...


class Right:
    def speak(self, loud: bool = False) -> str:
        return "汪"


class Wrong:
    def speak(self):                     # 名字对上，签名差一个参数
        return "no"


class Nothing:
    pass


@runtime_checkable
class HasName(Protocol):
    name: str                            # 数据成员（不是方法）


print("=== 1. @runtime_checkable 的 Protocol：名字对上就算，签名一个都不查 ===")
reset()
print(f"  协议 Speaks 要求的成员名 = {sorted(getattr(Speaks, '__protocol_attrs__', []))}"
      f" ｜ 它的 _is_runtime_protocol 标记 = {Speaks._is_runtime_protocol}")
print(eq("isinstance(Right(), Speaks)（正经实现）", lambda: isinstance(Right(), Speaks), True))
print(eq("isinstance(Wrong(), Speaks)（speak 少一个参数）", lambda: isinstance(Wrong(), Speaks), True))
print(eq("isinstance(Nothing(), Speaks)（啥都没有）", lambda: isinstance(Nothing(), Speaks), False))
print(eq("isinstance 的判据 == hasattr 逐个对象比（三个对象的全对列表）",
         lambda: [isinstance(o, Speaks) for o in (Right(), Wrong(), Nothing())],
         [hasattr(o, "speak") for o in (Right(), Wrong(), Nothing())]))
print(eq("连「speak 是个 property 不是方法」也算实现",
         lambda: isinstance(type("Prop", (), {"speak": property(lambda s: 1)})(), Speaks), True))
print(eq("issubclass(Wrong, Speaks)（类层面也只查名字）", lambda: issubclass(Wrong, Speaks), True))
print(ledger("名字对上就算，签名一个都不查：isinstance 与 hasattr 在本机逐位相同"))

print("\n=== 2. 代价（负对照）：isinstance 说 True，真按协议调用当场炸 ===")
# 这一格先故意造一个「会让判据响」的输入：按协议签名去调用那个冒牌货。
# 读屏的人要同时看到两行：isinstance 一片绿灯，调用却 TypeError —— 安全网是纸的。
reset()
print(eq("冒牌货先过检查这一关：isinstance(Wrong(), Speaks)",
         lambda: isinstance(Wrong(), Speaks), True))
print(eq("按协议签名调用它：Wrong().speak(loud=True)", lambda: Wrong().speak(loud=True),
         "TypeError: Wrong.speak() got an unexpected keyword argument 'loud'"))
print(eq("少一个参数地调用（它自己能跑）：Wrong().speak()", lambda: Wrong().speak(), "no"))
print(eq("正牌调用对照：Right().speak(loud=True)", lambda: Right().speak(loud=True), "汪"))
print(ledger("判据响了 1 次（上面那条 TypeError 原文），而它的 isinstance 是 True："
             "runtime_checkable 保证不了任何一次调用"))

print("\n=== 3. 带数据成员的 Protocol：isinstance 就是 hasattr，issubclass 直接不许 ===")
reset()
Dynamic = type("Dynamic", (), {"name": "v"})      # 类身上挂一个 name，运行时造的，不带地址
print(eq("isinstance(Dynamic(), HasName)（对象上有 name 吗）",
         lambda: isinstance(Dynamic(), HasName), True))
print(eq("同一个判据用 hasattr 说：hasattr(Dynamic(), 'name')",
         lambda: hasattr(Dynamic(), "name"), True))
print(eq("没有 name 的对象：isinstance(Nothing(), HasName)",
         lambda: isinstance(Nothing(), HasName), False))
print(eq("issubclass(Dynamic, HasName) 的报错原文", lambda: issubclass(Dynamic, HasName),
         "TypeError: Protocols with non-method members don't support issubclass(). "
         "Non-method members: 'name'."))
print(eq("只有方法的 Speaks 反而能 issubclass（对比：卡的是数据成员）",
         lambda: issubclass(Right, Speaks), True))
print(eq("HasName 要求的成员名", lambda: sorted(getattr(HasName, "__protocol_attrs__", [])),
         ["name"]))
print(ledger("数据成员协议：isinstance = hasattr（True/False 各量一次）；"
             "issubclass 一律 TypeError，原文点名 'name'"))

print("\n=== 4. 没加 @runtime_checkable 的 Protocol：连 isinstance 都不许用 ===")
# 换个子进程演：本文件里已经有一堆打过标记的协议，干净进程才看得出「标记是谁给的」。
reset()
shutil.rmtree(WORK, ignore_errors=True)
WORK.mkdir(parents=True, exist_ok=True)
(WORK / "bare_proto.py").write_text(
    '"""bare_proto.py：没加 @runtime_checkable 的 Protocol 能不能 isinstance。"""\n'
    "import typing\n"
    "class Bare(typing.Protocol):\n"
    "    def foo(self) -> int: ...\n"
    "class Impl:\n"
    "    def foo(self): return 1\n"
    "try:\n"
    "    print('isinstance ->', isinstance(Impl(), Bare))\n"
    "except Exception as exc:\n"
    "    print('isinstance -> ERR', type(exc).__name__ + ': ' + str(exc))\n"
    "print('标记之前 _is_runtime_protocol =', Bare._is_runtime_protocol)\n"
    "same = typing.runtime_checkable(Bare)\n"
    "print('runtime_checkable 返回的是同一个类吗 =', same is Bare)\n"
    "print('标记之后 _is_runtime_protocol =', Bare._is_runtime_protocol)\n"
    "print('现在再 isinstance =', isinstance(Impl(), Bare))\n",
    encoding="utf-8")
child_env = {"PATH": os.environ.get("PATH", ""), "LC_ALL": "C.UTF-8", "LANG": "C.UTF-8",
             "PYTHONIOENCODING": "utf-8"}
r = subprocess.run([sys.executable, "bare_proto.py"], cwd=str(WORK), env=child_env,
                   capture_output=True, text=True)
lines = r.stdout.splitlines()
for ln in lines:
    print(f"  │ {mask(ln)}")
print(f"  └ 子进程退出码 = {r.returncode} ｜ stderr 行数 = {len(r.stderr.splitlines())}")


def hit(prefix, want):
    got = [ln for ln in lines if ln.startswith(prefix)]
    return (bool(got) and want in got[0]), (got[0] if got else "（这行没打出来）")


ok_err, raw_err = hit("isinstance", "Instance and class checks can only be used with @runtime_checkable protocols")
print(eq("子进程里未加标记时 isinstance 的报错原文（整行核对）", lambda: ok_err, True))
print(f"    逐字 = {raw_err!r}")
print(eq("子进程里 runtime_checkable 返回的是同一个类", lambda: "True" in hit("runtime_checkable 返回", "True")[1], True))
print(eq("子进程里标记之前 _is_runtime_protocol 是 False",
         lambda: "False" in hit("标记之前", "False")[1], True))
print(eq("子进程里标记之后 isinstance 变 True（原地打标记，不换类）",
         lambda: "True" in hit("现在再 isinstance", "True")[1], True))
print(eq("本进程里同样的事：runtime_checkable(Speaks) is Speaks",
         lambda: runtime_checkable(Speaks) is Speaks, True))
print(ledger("没标记 = TypeError（原文已整行核对）；runtime_checkable 原地改标记、"
             "返回值 is 原类（True）"))

print("\n=== 5. get_origin / get_args：唯一真把注解拆开看的那批 ===")
reset()
TABLE = [("list[int]", list[int]), ("dict[str,int]", dict[str, int]),
         ("Optional[int]", typing.Optional[int]), ("int | str", int | str),
         ("Literal['a','b']", Literal["a", "b"]),
         ("Callable[[int],str]", Callable[[int], str]),
         ("Tuple[int, ...]", Tuple[int, ...]), ("Any", Any), ("Speaks（协议类本身）", Speaks)]
for label, obj in TABLE:
    print(f"  {label:<22} → origin={get_origin(obj)!r}  args={get_args(obj)!r}")
print(eq("list[int] 的 origin 就是 list 本身", lambda: get_origin(list[int]) is list, True))
print(eq("list[int] 的 args", lambda: get_args(list[int]), (int,)))
print(eq("dict[str,int] 的 args", lambda: get_args(dict[str, int]), (str, int)))
print(eq("Optional[int] 与 int|str 的 origin 是同一个东西（typing.Union）",
         lambda: (get_origin(typing.Optional[int]), get_origin(int | str)),
         (typing.Union, typing.Union)))
print(eq("Optional[int] 的 args 里那位 NoneType", lambda: get_args(typing.Optional[int]),
         (int, type(None))))
print(eq("Literal 的 origin 与 args（args 是字面量本身，不是类型）",
         lambda: (get_origin(Literal["a", "b"]), get_args(Literal["a", "b"])),
         (typing.Literal, ("a", "b"))))
print(eq("Callable 的 origin 走 collections.abc，args 第一位是「参数列表」",
         lambda: (get_origin(Callable[[int], str]), get_args(Callable[[int], str])),
         (collections.abc.Callable, ([int], str))))
print(eq("Tuple[int, ...] 的 args 里带 Ellipsis（可变参数的记号）",
         lambda: get_args(Tuple[int, ...]), (int, Ellipsis)))
print(eq("Any 拆开是 (None, ())：它没有 origin，所以第 8 格会把它当裸类送给 isinstance（然后就炸了）",
         lambda: (get_origin(Any), get_args(Any)), (None, ())))
print(eq("协议类本身也拆不出东西（origin=None）", lambda: (get_origin(Speaks), get_args(Speaks)),
         (None, ())))
print(ledger("表上 9 行逐字可核；origin 给出「容器真身」，args 给出「参数元组」，"
             "Any 与协议类都是 (None, ())"))

print("\n=== 6. list[int] 每次都是新对象，但相等：判据要写 ==，别写 is ===")
reset()
print(eq("list[int] is list[int]", lambda: list[int] is list[int], False))
print(eq("list[int] == list[int]", lambda: list[int] == list[int], True))
print(eq("list[int].__args__ == (int,)", lambda: list[int].__args__, (int,)))
print(eq("dict[str,int] == dict[str,int]", lambda: dict[str, int] == dict[str, int], True))
print(eq("list[int] == list[str]", lambda: list[int] == list[str], False))
print(eq("Optional[int] == int | None（两条写法是同一个东西）",
         lambda: typing.Optional[int] == (int | None), True))
print(eq("缓存一次再比：同一个对象比较自己 is → True",
         lambda: (lambda o: o is o)(list[int]), True))
print(ledger("参数化泛型是「每次现造」的对象：is False / == True，"
             "所以第 8 格的校验器比 args 一律用 ==")
      )

print("\n=== 7. TypeVar：bound 存在 __bound__ 上，get_args 对它一无所获 ===")
reset()
T = TypeVar("T", bound=int)
U = TypeVar("U", int, str)                  # 约束列表写法
print(f"  T = TypeVar(\"T\", bound=int)：__name__ = {T.__name__} ｜ repr 是 {T!r}"
      f"（模块顶层和函数里都带这个 ~ 前缀，而且 T 没有 __qualname__：{hasattr(T, '__qualname__')}）"
      f"→ 判据一律用 __name__ / __bound__，别拿 repr 比")
print(eq("T.__bound__ is int", lambda: T.__bound__ is int, True))
print(eq("get_origin(T)", lambda: get_origin(T), None))
print(eq("get_args(T)（拆不出东西：TypeVar 不是参数化泛型）", lambda: get_args(T), ()))
print(eq("U.__constraints__（多约束走另一个属性）", lambda: U.__constraints__, (int, str)))
print(eq("get_args(U) 同样是空", lambda: get_args(U), ()))
print(eq("T 能拿去参数化：list[T] 的 args 里放的就是 T 本身",
         lambda: get_args(list[T]), (T,)))
print(ledger("bound 在 __bound__、constraints 在 __constraints__，"
             "get_origin/get_args 对 TypeVar 只给 (None, ()) —— 运行时没人替你查 bound"))

print("\n=== 8. 手搭一个两层的最小运行时候验器（外加它漏在哪）===")
# 这就是「本机没有 mypy 时怎么活」的样板：拿 origin/args 自己拆、自己 isinstance。
# 故意只做三层：裸类、X | Y、list[T] / dict[K, V]，其余一律放过（下一段就把放过的事演出来）。
reset()


def check(value, hint) -> bool:
    """两层最小校验：True = 「我没查出问题」，不等于「值是对的」。"""
    origin, args = get_origin(hint), get_args(hint)
    if origin is None:                                   # 裸类 / 拆不出东西的（Any 也走这条，见漏 5）
        return isinstance(value, hint) if isinstance(hint, type) else True
    if origin is typing.Union:                           # X | Y：任一分支对上就算
        return any(check(value, a) for a in args)
    if origin is list:
        return isinstance(value, list) and all(check(v, args[0]) for v in value)
    if origin is dict:
        return (isinstance(value, dict)
                and all(check(k, args[0]) for k in value)
                and all(check(v, args[1]) for v in value.values()))
    return True                                          # 其余一律放过（见下面「漏」那几条）


SRC_LINES = inspect.getsource(check).splitlines()
print(f"  check 的源码 {len(SRC_LINES)} 行（含 def 和 docstring），只认三种形状：裸类 / X | Y / list+dict，"
      f"其余一律放过")
print(eq("负对照（先让它响）：check([1, 'x'], list[int])", lambda: check([1, "x"], list[int]), False))
print(eq("正例：check([1, 2], list[int])", lambda: check([1, 2], list[int]), True))
print(eq("负例：check({'a': 1, 'b': 'x'}, dict[str, int])", lambda: check({"a": 1, "b": "x"}, dict[str, int]), False))
print(eq("正例：check({'a': 1}, dict[str, int])", lambda: check({"a": 1}, dict[str, int]), True))
print(eq("Union 正例：check(3, int | str)", lambda: check(3, int | str), True))
print(eq("Union 负例：check(3.5, int | str)", lambda: check(3.5, int | str), False))
print(eq("Optional[int] 认 None（NoneType 也是裸类）", lambda: check(None, typing.Optional[int]), True))
print(eq("嵌套：check({'a': [1, 'x']}, dict[str, list[int]])（递归吃到里层）",
         lambda: check({"a": [1, "x"]}, dict[str, list[int]]), False))
print("  —— 它漏的地方，逐条实测（True = 没查出问题，也就是漏了）：")
print(eq("漏 1 定长元组：check((1, 2), Tuple[int, str])（正确答案是 False）",
         lambda: check((1, 2), Tuple[int, str]), True))
print(eq("漏 2 可变参数：check((1, 'x'), Tuple[int, ...])（正确答案也是 False）",
         lambda: check((1, "x"), Tuple[int, ...]), True))
print(eq("漏 3 Literal：check('zzz', Literal['a','b'])", lambda: check("zzz", Literal["a", "b"]), True))
print(eq("漏 4 Annotated：check('x', Annotated[int, '元数据'])",
         lambda: check("x", Annotated[int, "元数据"]), True))
print(eq("漏 5 Any：check('x', Any) 不是放过，是当场 TypeError",
         lambda: check("x", Any), "TypeError: typing.Any cannot be used with isinstance()"))
print(f"      根因：isinstance(Any, type) 是 {isinstance(Any, type)} —— Any 的「长得像类」骗过了裸类分支")
print(eq("漏 6 注解写成 None 这个词：check(1, None) 直接放过", lambda: check(1, None), True))
print(eq("漏 7 协议：check(Wrong(), Speaks) 落到 isinstance，于是只剩「有没有 speak 这个名字」",
         lambda: check(Wrong(), Speaks), True))
print("      （这条 True 是真漏：第 2 格已经演过 Wrong().speak(loud=True) 当场 TypeError）")
print(eq("漏 8 bool 是 int 的儿子：check([True], list[int]) 里 True 算合法元素",
         lambda: check([True], list[int]), True))
print(ledger("对 list/dict/Union 它真的响（4 条负例全 False）；元组（定长与 ...）、Literal、"
             "Annotated、None 这个词、协议、bool 子类共 7 处一律放过，Any 那处直接 TypeError "
             "—— 自己搭的网，网眼要自己数")
      )

print("\n=== 9. 收口：Protocol 查名、origin/args 查形，别的都是自觉 ===")
print("  ① @runtime_checkable 的 Protocol：isinstance == hasattr（第 1 格三个对象逐位相同），"
      "签名一个都不查 —— 第 2 格那个冒牌货 isinstance 是 True，一调用就 TypeError。")
print("  ② 带数据成员的 Protocol：isinstance 还是 hasattr，issubclass 直接 TypeError 并点名 'name'（第 3 格）；"
      "没加标记的 Protocol：isinstance 连用都不许（第 4 格原文），而 runtime_checkable 是原地打标记、"
      "返回值就是原类（is → True）。")
print("  ③ get_origin/get_args 是本章唯一「真的读懂了注解」的标准库函数：容器给 origin、"
      "参数给 args（第 5 格 9 行表）；Union 的 origin 是 typing.Union、Callable 走 collections.abc、"
      "Tuple[int, ...] 的 args 里带 Ellipsis、Any 和协议类都是 (None, ())。")
print("  ④ 参数化泛型每次现造：is False / == True（第 6 格），TypeVar 的 bound 只在 __bound__ 里、"
      "get_args 对它给空（第 7 格）—— 所以比注解一律用 ==，查 TypeVar 得自己读那两个属性。")
print("  ⑤ 于是运行时能做的检查只有一条路：自己拆 origin/args 自己 isinstance（第 8 格那 14 行）。"
      "它响足 4 次（4 条负例全 False），也真漏 8 处（7 处放过 + Any 那处当场 TypeError），")
print("     漏的每一处都不是「没想到」，是 check 里没写那条分支：元组（定长与 ...）、Literal、Annotated、"
      "Any、None 这个词、协议、bool 是 int 的儿子。")
print("  ★ 代价：Protocol 的安全感是名字给的，校验器的安全感是自己写的循环给的；")
print("     两者都不如一个真正的静态检查器（本机没有，见 g 篇在场性探测）。")
print("     要用就先把「网眼清单」写进注释，让下一个读代码的人知道哪些形状没人管。")

shutil.rmtree(WORK, ignore_errors=True)
left = sorted(p.name for p in Path.cwd().iterdir() if p.is_dir() and p.name.endswith("-demo"))
print(f"\n  收尾自证：{WORK.name}/ 还在吗 → {WORK.exists()}"
      f" ｜ 当前目录剩下的 *-demo 目录 → {left}（本篇只创建/删除 {WORK.name}/ 这一个，"
      f"列出来的是别的脚本的现场；__pycache__ 跟着沙箱一起没了）")
print(f"  → 断言：{WORK.name}/ 已删除 = {WORK.exists() is False}"
      f" ｜ 本篇 1~8 格判据 {TOTAL['ok']}/{TOTAL['n']} 条成立（少一条上面就有 [不成立！]）"
      f" ｜ 本篇写过的目录只有 {WORK.name}/ 一个，它现在存在吗 → {WORK.exists()}")
print(f"  全篇判据合计 {TOTAL['ok']}/{TOTAL['n']} 条成立（少一条上面就有 [不成立！]）")
