"""第 06 章第 14 节：__slots__ 的账 —— 10 万个实例实测省多少，代价是什么。

跑法：python3 l_slots_cost.py

口径（全部数字出自这一把尺子，别处不要混用）：
  各造 N=100_000 个实例、都持有 x/y 两个 int 字段，
  逐实例累加 sys.getsizeof(实例)（没有 __slots__ 的再加 sys.getsizeof(实例.__dict__)），
  列表本身不计（两边一样、且 del 掉了）。
为什么 __dict__ 在批量口径下只有 88 字节：3.14 的实例字典跨实例共享「keys 半边」
（CPython 的 key-sharing dict），getsizeof 只报它独占的 values 半边 —— 这也是
「省内存」这件事在水面下的部分：不用 slots 时 Python 已经在用共享 keys 自救了，
slots 省掉的是「每实例一个 values 数组 + 一次哈希查表」。
"""
import sys
import time

N = 100_000


class Loose:
    def __init__(self, x, y):
        self.x = x
        self.y = y


class Tight:
    __slots__ = ("x", "y")

    def __init__(self, x, y):
        self.x = x
        self.y = y


class LooseBase:              # 父类没 slots
    pass


class TightChild(LooseBase):  # 子类补 slots 也白干
    __slots__ = ("x", "y")


def batch_total(cls):
    objs = [cls(i, i + 1) for i in range(N)]
    total = 0
    for o in objs:
        total += sys.getsizeof(o)
        d = getattr(o, "__dict__", None)
        if d is not None:
            total += sys.getsizeof(d)
    del objs
    return total


def batch_time(cls):
    t0 = time.perf_counter()
    objs = [cls(i, i + 1) for i in range(N)]
    dt = time.perf_counter() - t0
    del objs
    return dt


print("=" * 68)
print(f"一、{N:,} 个实例的总账（批量口径，见脚本 docstring）")
print("=" * 68)
loose_total = batch_total(Loose)
tight_total = batch_total(Tight)
print(f"Loose（无 slots）合计：{loose_total:,} 字节，摊到每实例 {loose_total // N:,} 字节")
print(f"Tight（有 slots）合计：{tight_total:,} 字节，摊到每实例 {tight_total // N:,} 字节")
saved = loose_total - tight_total
print(f"__slots__ 省了：{saved:,} 字节（约为 Loose 的 {saved * 100 // loose_total}%，每实例省 {saved // N:,} 字节）")
a, b = Loose(1, 2), Tight(3, 4)
print(f"对照单实例：本体都是 {sys.getsizeof(b)} 字节；Loose 多挂着的那坨就是它的 __dict__"
      f"（{type(a.__dict__).__name__}），Tight 压根没有：hasattr(b, '__dict__') → {hasattr(b, '__dict__')}")

print()
print("二、造 10 万个的耗时（每次会抖，只看量级，README 别引具体数）")
print(f"Loose：{batch_time(Loose) * 1000:.0f} 毫秒 ｜ Tight：{batch_time(Tight) * 1000:.0f} 毫秒")

print()
print("=" * 68)
print("三、三个代价（每个都是原文）")
print("=" * 68)
t = Tight(0, 0)
try:
    t.extra = 1
except AttributeError as e:
    print("代价 1 加不上新属性 →", e)
try:
    vars(t)
except TypeError as e:
    print("代价 2 vars() 用不了 → TypeError:", e)
c = TightChild()
c.x = 1
c.anything = 2
print("代价 3 父类没写 __slots__，子类写了也白干：hasattr(c, '__dict__') →", hasattr(c, "__dict__"),
      "（c.anything =", c.anything, "照样塞得进去）")
