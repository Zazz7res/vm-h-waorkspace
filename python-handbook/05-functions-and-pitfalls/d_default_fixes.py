#!/usr/bin/env python3
# ============================================================================
# d_default_fixes.py —— 第 05 章第 6 节：三种正解（以及「什么时候根本不用改」）
#
# 第 5 节那个坑的修法只有三种，按使用频率排：
#   A. None 哨兵 —— 90% 的场景，一行 if 解决；
#   B. keyword-only 必填 —— 把「你确定要共用这个容器吗」变成调用者必须回答的问题；
#   C. 工厂（dataclasses.field(default_factory=...) 那一族）—— 在类里、或想要「每次现造」的通用形态。
# ============================================================================

print("=== 正解 A：None 哨兵（最常用，也是标准库自己的写法）===")


def good_append(item, box=None):
    if box is None:                 # 必须用 is None，不能用 ==（0 和 "" 也会相等）
        box = []                    # 每次调用现造一个新表
    box.append(item)
    return box


print("连调三次：", good_append(1), good_append(2), good_append(3))
print("账本里没有 list（None 是不可变的，随便共用）：", good_append.__defaults__)
print("显式传表时照样能原地改（这是特性：调用方想收集就给一个）：")
mine = []
print("  good_append('A', mine) =", good_append("A", mine), "｜ mine =", mine)
print("  good_append('B') 会不会污染 mine：", good_append("B"), "｜ mine =", mine)
print("为什么必须写 `is None` 而不是 `if not box`：")


def sloppy(item, box=None):
    if not box:                     # ← 错法：空表、0、"" 全被当成「没传」
        box = []
    box.append(item)
    return box


given = [1, 2]
print("  sloppy('新', given) =", sloppy("新", given), "（这次运气好，given 非空）")
empty_given = []
print("  sloppy('新', empty_given) =", sloppy("新", empty_given), "｜ 传进来的 empty_given =", empty_given)
print("  → 传进来的空表被**扔掉**了，调用方以为自己在收集，实际拿到的是另一个对象。")

print()
print("=== 正解 B：keyword-only 必填，逼调用者自己交出容器 ===")


def strict_append(item, *, box):        # * 右边的参数只能写 名字=值（第 9 节详讲）
    box.append(item)
    return box


print("正常用法：strict_append(1, box=[]) =", strict_append(1, box=[]))
print("再调一次（新表）：", strict_append(2, box=[]))
try:
    strict_append(3)
except TypeError as e:
    print("想偷懒不传 box：", type(e).__name__, "|", e)
print("  → 坑变成**当场报错**，这是最好的结局：写代码的人立刻被拦下。")

print()
print("=== 正解 C：工厂函数（每调用一次现造一个），类里就是 default_factory ===")


def build(rows_factory=list, header="表头"):     # 默认值收的是「函数」本身，函数不可引用状态
    rows = rows_factory()                        # 用时才造
    rows.append(header)
    return rows


print("build() 三次：", build(), build(), build())
print("账本（存的是函数对象，不是产物）：", build.__defaults__)
print("传别的工厂也通：build(lambda: [\"先放一行\"]) =", build(lambda: ["先放一行"]))
print("  同一招在 dataclass 里叫 field(default_factory=list)（第 06 章第 10 节实测过那条 ValueError）。")

print()
print("=== 什么时候不用改：默认值本来就该是「共享的那个」===")
_registry = {}


def register(name, table=_registry):      # 故意共享：这就是全局注册表
    table[name] = True
    return sorted(table)


print("连续注册三个：", register("a"), register("b"), register("c"))
print("外部直接看那份注册表：", sorted(_registry))
print("  → 共享是**意图**时，写清注释并接受它；坑只在于「你以为每次是新的」。")

print()
print("=== 自查这条坑的两条硬办法 ===")


def suspect(**kw):
    return [(name, fn.__defaults__) for name, fn in kw.items()
            if any(isinstance(d, (list, dict, set, bytearray)) for d in (fn.__defaults__ or ()))]


def with_list(x, acc=[]):
    return acc.append(x)


def with_dict(x, acc={}):
    return acc


def with_tuple(x, acc=()):
    return acc


print("扫一遍三个函数，报出「默认值里有可变对象」的：", [name for name, _ in suspect(
    with_list=with_list, with_dict=with_dict, with_tuple=with_tuple)])
print("  真跑时用 `fn.__defaults__` 一眼就能看到那个共享容器；")
print("  代码评审时用 `grep -n 'def .*=\\[\\]' *.py`（或 ={} / =set()）也能一网打尽。")
