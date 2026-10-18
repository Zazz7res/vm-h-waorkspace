"""第 06 章第 13 节：@dataclass(slots=True) —— 3.10+ 的选项，拆开看。

跑法：python3 k_dc_slots.py

dataclass(slots=True) 的做法是「另造一个新类」：把字段名塞进 __slots__，
实例就没有 __dict__ 了；fields()、默认值、frozen 照常工作。
顺手把「手写 __slots__ + slots=True 打架」的报错原文也抄下来。
"""
from dataclasses import dataclass, fields


@dataclass(slots=True)
class Point:
    x: int
    y: int
    tag: str = "pt"


p = Point(1, 2)

print("=" * 68)
print("一、__dict__ 真的没了；字段变成了类上的槽描述符")
print("=" * 68)
print("hasattr(p, '__dict__') →", hasattr(p, "__dict__"))
print("Point.__slots__ →", Point.__slots__)
print("类字典里 x 这个位置是什么：", type(vars(Point)["x"]).__name__)
print("但字段清单照常：", [f.name for f in fields(Point)])
print("实例数据照样读写：p.x =", p.x, "；p.tag =", p.tag)

print()
print("=" * 68)
print("二、加新属性：当场拒绝，原文")
print("=" * 68)
try:
    p.z = 99
except AttributeError as e:
    print("AttributeError:", e)

print()
print("=" * 68)
print("三、和手写 __slots__ 打架的原文")
print("=" * 68)
try:
    @dataclass(slots=True)
    class Both:
        __slots__ = ("x",)
        x: int
except TypeError as e:
    print("TypeError:", e)

print()
print("=" * 68)
print("四、frozen + slots 可以叠：既不能改也不能加")
print("=" * 68)

@dataclass(frozen=True, slots=True)
class Lock:
    id: int


lk = Lock(7)
print("没有 __dict__：", hasattr(lk, "__dict__"))
try:
    lk.id = 8
except Exception as e:
    print("改 →", type(e).__name__ + ":", e)
try:
    lk.other = 8
except Exception as e:
    print("加 →", type(e).__name__ + ":", e)
    print("  （这是 3.14 上 frozen+slots 的一个真实毛边：frozen 生成的 __setattr__ 拿的还是"
          "重建类之前的旧 cls 去 super()，报的不是 FrozenInstanceError 而是 super 检查失败；"
          "结论照旧——改也不行加也不行，只是加的报错比较怪）")
print("repr 正常：", repr(lk))
