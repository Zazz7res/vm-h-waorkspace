"""第 06 章第 10 节：可变默认值 —— 手写会中毒，dataclass 当场拒绝。

跑法：python3 h_dc_mutable_default.py

上集（第 05 章）：def f(items=[]) 的默认值是「函数对象上的一个槽」，只造一次，
所有调用共用。搬进类里就是本脚本第一幕。
dataclass 生成 __init__ 时看得见注解，所以它直接 ValueError 拦你（第二幕），
并给出解法：field(default_factory=list) —— 每次构造「现造一个新的」（第三幕）。
"""
from dataclasses import dataclass, field, fields


class NaiveCart:
    """手写的坑：默认列表长在函数默认值上，全宇宙共用一份。"""

    def __init__(self, items=[]):
        self.items = items

    def add(self, name):
        self.items.append(name)
        return self.items


@dataclass
class SafeCart:
    """default_factory 的正解。"""

    items: list = field(default_factory=list)
    owner: str = "?"

    def add(self, name):
        self.items.append(name)
        return self.items


print("=" * 68)
print("一、手写版：两个实例共用同一个默认列表（第 05 章的病根搬进类）")
print("=" * 68)
n1, n2 = NaiveCart(), NaiveCart()
n1.add("苹果")
print("n1.add 之后 n1.items =", n1.items)
print("没动过的 n2.items =", n2.items, "← 见鬼了，它怎么也有苹果")
print("两个实例的 items 是同一个对象吗：", n1.items is n2.items)
print("真凶在函数对象上：NaiveCart.__init__.__defaults__ =", NaiveCart.__init__.__defaults__)

print()
print("=" * 68)
print("二、dataclass 的拒绝原文（捕获版；裸 traceback 在 demo 上一条命令里）")
print("=" * 68)
try:
    @dataclass
    class Boom:
        items: list = []        # 想复刻上面的坑？没门
except ValueError as e:
    print("ValueError:", e)

print()
print("=" * 68)
print("三、field(default_factory=list)：每建一个实例现造一个列表")
print("=" * 68)
s1, s2 = SafeCart(), SafeCart()
s1.add("苹果")
print("s1.add 之后：s1.items =", s1.items, "｜ s2.items =", s2.items)
print("items 同源吗：", s1.items is s2.items, "← False，各用各的")
f = fields(SafeCart)[0]
print("fields(SafeCart) 里 items 的默认值：MISSING（对，字段本身没默认值）")
print("default_factory 上存的是：", f.default_factory.__name__, "← 构造时调用它拿新列表")
s3 = SafeCart(["现成的"], "张三")
print("显式传参照常：", s3)
