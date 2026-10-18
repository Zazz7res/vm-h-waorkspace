#!/usr/bin/env python3
# ============================================================================
# 取证脚本 g_reading.py —— 第 8 节：常见报错的原文集（这一节当字典查）
#
# 目标：把最常撞上的报错**原文**逐条抓下来，每条配一个「看到它就往哪查」的判据。
# 全部用 try/except 接住、只打 type + str(exc)，所以本脚本自己不会崩。
# ============================================================================
import os
import sys


def catch(label, fn):
    """跑一下：崩了就把「类型: 消息」原样打出来，没崩就打返回值。"""
    print(f"  {label}")
    try:
        value = fn()
    except BaseException as exc:
        print(f"      → {type(exc).__name__}: {exc}")
    else:
        print(f"      → 没报错，返回值 = {value!r}")


def self_read():
    n = 0
    if False:
        n = 1
    return n + undefined_here  # 这条是 NameError


def unbound(coin):
    if coin:
        total = 1
    return total + 1  # coin 为假时：total 在这层是本地名，但这条路径没给它赋过值


def deep(n):
    return deep(n + 1)


print("=== 1. NameError 一家的原文：长得像，病因不同 ===")
catch("引用一个从没出现过的名字", lambda: eval("undefined_name"))
catch("函数里读一个「这条路径没赋过值」的本地名", lambda: unbound(False))
catch("模块里读一个不存在的名字（真·没定义）", self_read)
print("  判据：`NameError: name 'x' is not defined` = 这个名字在这层根本没出现过；")
print("        `UnboundLocalError` = 出现过（同函数里有赋值），但**这条执行路径**还没轮到它。")
print("        父子关系在第 3 节证过：issubclass(UnboundLocalError, NameError) → True")

print("\n=== 2. AttributeError：三种原文对应三种不同的病因 ===")
catch("'abc'.append('x')（str 没这个方法）", lambda: eval("'abc'.append('x')"))
catch("None.append(1)（点号左边是 None）", lambda: eval("None.append(1)"))


class Bare:
    pass


catch("Bare().x（实例上没这个字段）", lambda: Bare().x)
try:
    eval("'abc'.apend('x')")
except AttributeError as exc:
    print("  打错方法名时解释器给建议：", exc)
    print("  机器可读的字段只有这些：name =", repr(exc.name),
          "｜ obj 的类型 =", type(getattr(exc, "obj", None)).__name__)
    print("  「Did you mean」是**模糊匹配**算出来的最近名字，不保证是你想要的 —— 别把建议当结论。")
print("  另外三种本章另外几节演过的 AttributeError，先在这里挂个号：")
print("    property 没有 setter（第 06 章第 8 节）、frozen dataclass 赋值（第 06 章第 11 节）、")
print("    slots 实例加新属性（第 06 章第 13 节）。**报错类型一样，病因完全不同。**")

print("\n=== 3. TypeError 家族：同一句「类型不对」有九种原文 ===")
catch("1 + '1'", lambda: eval("1 + '1'"))
catch("'1' + 1", lambda: eval("'1' + 1"))
catch("len(42)", lambda: len(42))
catch("sorted([1, 'a'])（元素互相比不了）", lambda: sorted([1, "a"]))
catch("函数少传参数", lambda: eval("(lambda a, b: a)(1)"))
catch("函数多传参数", lambda: eval("(lambda a: a)(1, 2)"))
catch("不可调用：x 是 1 却写 x()", lambda: eval("(lambda x: x())(1)"))
catch("不可迭代：list(42)", lambda: eval("list(42)"))
catch("不可哈希：把一个 list 放进 set", lambda: eval("{[1, 2]}"))
print("  对照：`int('abc')` 报的是 ValueError，不是 TypeError ——")
catch("int('abc')", lambda: int("abc"))
print("  这条判据最省时间：**TypeError 查形状（类型/个数/能不能调），ValueError 查值（内容读不出来）**。")

print("\n=== 4. LookupError 家族：找不到，但文案有三种长相 ===")
BOX = {}
catch("[1, 2][9]", lambda: eval("[1, 2][9]"))
catch("(1, 2)[9]", lambda: eval("(1, 2)[9]"))
catch("b'ab'[9]（bytes 也是同一族）", lambda: eval("b'ab'[9]"))
catch("'abc'[9]", lambda: eval("'abc'[9]"))
catch("{'a': 1}['b']", lambda: eval("{'a': 1}['b']"))
catch("del 一个不存在的键", lambda: BOX.__delitem__("b"))
print("  KeyError 的消息**自带引号**（它用 repr 表示那个键，和第 06 章第 6 节「容器里各走 repr」是同一套规矩），")
print("  IndexError 的消息没有引号 —— 只看报错文本就能区分这两族。")
print("  注意最后那条 `del` 报的也是 KeyError：取值和删值是同一扇门，错也是同一个错。")

print("\n=== 5. StopIteration：它是收尾信号，不是事故 ===")
gen = (x for x in [1])
print("  手动 next 到底 =", next(gen))
catch("next(已耗尽的生成器)", lambda: next(gen))
catch("同样的东西交给 list() 收尾（打得到这行就说明自动接住了）", lambda: list(iter([7, 8])))
print("  判据：for / list() / sum() 内部替你接了 StopIteration；只有**手动 next()** 才需要接。")
print("    在 traceback 里看到裸的 StopIteration，说明这段代码在手写迭代协议；`__iter__`/`__next__`")
print("    那套本手册**没演**（第 04 章第 17 节只到生成器和惰性为止，它自己把迭代协议列进了「没演」）。")

print("\n=== 6. 和外部世界打交道：OSError 家族（只碰本地，不碰网络）===")
catch("open('/肯定不存在/没有这个文件')", lambda: open("/肯定不存在/没有这个文件"))
catch("os.remove 一个不存在的路径", lambda: os.remove("/tmp/py07-绝对不存在"))
catch("0 ** -1（不是 OSError，放进来做对照）", lambda: eval("0 ** -1"))
try:
    open("/肯定不存在/x")
except OSError as exc:
    print("  OSError 独有的机器可读字段：errno =", exc.errno,
          "｜ strerror =", exc.strerror, "｜ filename =", exc.filename)
    print("  args =", exc.args, "（**两个**元素：errno + strerror，和其它异常不一样）")
    print("  FileNotFoundError 是它的儿子吗 →", issubclass(FileNotFoundError, OSError),
          "｜ PermissionError →", issubclass(PermissionError, OSError))
print("  判据：文件/权限/网络一族的错**全都**是 OSError 的儿子。接 OSError 一网打尽，")
print("    但要分清「没有」和「没权限」就得接窄的那两个。")

print("\n=== 7. RecursionError：第 05 章的递归限额在这里的长相 ===")
catch("deep(n) = deep(n+1)（这次接住了）", lambda: deep(0))
print("  它是 RuntimeError 的儿子吗 →", issubclass(RecursionError, RuntimeError),
      "｜ 当前限额 =", sys.getrecursionlimit())
print("  → 限额到了是**抛异常**，不是把进程搞崩；所以能接住，接住之后栈已经退回来了一层。")

print("\n=== 8. 收口：一张「看到什么往哪查」的表 ===")
rows = [
    ("NameError", "这层根本没出现过这个名字（拼错 / 忘 import / 定义在别的分支）"),
    ("UnboundLocalError", "同函数里有赋值，编译器判它成本地名，但这条路径先读了"),
    ("AttributeError", "点号左边的东西没这个名字（也可能是 property 无 setter / frozen / slots）"),
    ("TypeError", "形状不对：类型不匹配、参数个数、不可调用、不可迭代、不可哈希"),
    ("ValueError", "类型对但值不行：int('abc')、转换失败、内容不合法"),
    ("IndexError", "序列下标越界（list / tuple / str / bytes）"),
    ("KeyError", "映射里没这个键；消息用 repr 所以自带引号"),
    ("StopIteration", "迭代器耗尽 —— for 自动接，手动 next 才要接"),
    ("RecursionError", "递归超限额（RuntimeError 的儿子），能接住，不是崩"),
    ("OSError 及儿子", "外部世界：文件 / 权限 / 网络；带 errno、strerror、filename"),
    ("SyntaxError 及儿子", "编译期，代码一行没跑；带 lineno / offset / text"),
]
for name, meaning in rows:
    print(f"  {name:<18}→ {meaning}")
print(f"  上面 {len(rows)} 条里，只有 SyntaxError 那一族不在本节造的（第 7 节第 5 段现场演过），")
print("  其余每一条都是本节当场构造出来的 —— 本节没有一条是背的。")
