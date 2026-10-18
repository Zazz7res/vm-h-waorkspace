"""第 06 章第 12 节：eq 怎么比、order 才有的 <、repr 长什么样。

跑法：python3 j_dc_eq_order.py

生成的 __eq__ 是「逐字段比 + 要求两边是同一个类」；
order=True 才生成 __lt__ 等四件套，比法是「字段元组字典序」；
repr 默认把所有字段按声明顺序排成 name=value。
"""
from dataclasses import dataclass


@dataclass(order=True)
class Version:
    major: int
    minor: int
    patch: int = 0


@dataclass
class Plain:
    a: int


class StrictEq:
    """手写 __eq__ 的对照组：用 isinstance 的话，子类也算相等。"""

    def __init__(self, a):
        self.a = a

    def __eq__(self, other):
        return isinstance(other, StrictEq) and self.a == other.a


class SubVersion(Version):
    """子类不加任何新字段——专门用来撞「同一个类」这条规则。"""


v1, v2 = Version(1, 2), Version(1, 2)
same_elsewhere = Version(1, 2)

print("=" * 68)
print("一、生成的 __eq__：逐字段 + 必须同类")
print("=" * 68)
print("Version(1,2) == Version(1,2)：", v1 == same_elsewhere)
print("Version(1,2,0) == Version(1,2)：", Version(1, 2, 0) == v1, "← patch 也是字段，默认 0 也参与比")
print("Version(1,2) == SubVersion(1,2)：", v1 == SubVersion(1, 2), "← 字段全同也不给过：不是同一个类")
print("对照组手写 isinstance 版：", StrictEq(1) == StrictEq(1), "（它管你是不是子类）")
print("比不过就返回 NotImplemented 让位：", Version(1, 2) == "字符串", "← 对面类接不住才 False")

print()
print("=" * 68)
print("二、order=True 才有 < ：字段元组字典序")
print("=" * 68)
print("Version(1,2) < Version(1,10)：", Version(1, 2) < Version(1, 10), "← (1,2,0)<(1,10,0) 逐位比，不是字符串比")
print("Version(1,9,1) < Version(1,10,0)：", Version(1, 9, 1) < Version(1, 10, 0))
xs = [Version(2, 0), Version(1, 9), Version(1, 10)]
print("sorted 直接可用：", sorted(xs))
print("对照组 Plain（没开 order）的类字典里有 __lt__ 吗：", "__lt__" in vars(Plain),
      "→ Plain(1) < Plain(2) 就是 demo 上一条命令里那句 TypeError")

print()
print("=" * 68)
print("三、repr：按字段声明顺序，name=value，str 用带引号的 repr")
print("=" * 68)
print("repr(Version(1,2)) →", repr(Version(1, 2)))
print("repr(Version(1,2,'rc1')) 这种字符串字段 →", repr(Version(1, 2, "rc1")))
print("塞进 list：", [Version(1, 0), Version(0, 1)], "← 容器打印走 repr，和第 6 节呼应")
