"""第 06 章第 9 节：@dataclass 到底替你写了什么 —— 对照手写版逐条看。

跑法：python3 g_dataclass_intro.py

@dataclass 不是新语法，是一个「读类型标注 → 往类字典里塞函数」的装饰器。
本脚本把生成的零件拆出来打印：
  * 类字典里多了哪几个名字（__init__ / __repr__ / __eq__）；
  * inspect.signature 看生成 __init__ 的参数表；
  * dataclasses.fields() 看字段清单（含 ClassVar 被排除的证据）；
  * 和一个「手写等价版」并排跑，行为完全一致 —— 它就只是帮你写了那几行。
"""
import inspect
from dataclasses import MISSING, dataclass, fields
from typing import ClassVar


@dataclass
class Vec:
    x: int
    y: int
    label: str = "v"
    count: ClassVar[int] = 0      # 类变量：明确不该进字段清单


class HandVec:
    """和上面 dataclass 手写等价的最小实现。"""

    count = 0                     # ClassVar 的替身

    def __init__(self, x: int, y: int, label: str = "v"):
        self.x = x
        self.y = y
        self.label = label

    def __repr__(self):
        # 故意把类名也写成 Vec：和 dataclass 生成的 repr 对齐，方便逐字比较
        return f"Vec(x={self.x!r}, y={self.y!r}, label={self.label!r})"

    def __eq__(self, other):
        return isinstance(other, HandVec) and (self.x, self.y, self.label) == (other.x, other.y, other.label)


print("=" * 68)
print("一、装饰完，类字典里多了什么")
print("=" * 68)
extra = [k for k in ("__init__", "__repr__", "__eq__") if k in vars(Vec)]
print("三件套都在类字典里：", extra)
print("它们都只是普通函数：", [type(vars(Vec)[k]).__name__ for k in extra])
print("inspect 看得到签名（这就是它替你写的 __init__ 参数表）：")
print("  ", inspect.signature(Vec.__init__))

print()
print("=" * 68)
print("二、fields()：字段清单，ClassVar 不在其中")
print("=" * 68)
fs = fields(Vec)
print("字段名：", [f.name for f in fs])
for f in fs:
    has_default = f.default is not MISSING
    print(f"  {f.name}: type={f.type.__name__ if isinstance(f.type, type) else f.type}"
          f" 默认值={'MISSING' if not has_default else f.default!r}"
          f" default_factory={'无' if f.default_factory is MISSING else f.default_factory}")
print("count（ClassVar）在字段清单里吗：", "count" in [f.name for f in fs])

print()
print("=" * 68)
print("三、实例上存的东西：字段老老实实进 vars(实例)")
print("=" * 68)
v = Vec(1, 2)
print("vars(Vec(1, 2)) =", dict(vars(v)))
print("count 在实例字典里吗：", "count" in vars(v), "← ClassVar 只住类字典：", "count" in vars(Vec))

print()
print("=" * 68)
print("四、和手写版并排跑：行为一致")
print("=" * 68)
a1, a2 = Vec(1, 2), Vec(1, 2)
b1, b2 = HandVec(1, 2), HandVec(1, 2)
print("repr 对比：", repr(a1), "｜", repr(b1))
print("判等对比：", (a1 == a2, b1 == b2), "← 都是「字段全相等才相等」")
print("字段访问对比：", (a1.x, a1.y, a1.label) == (b1.x, b1.y, b1.label))
try:
    Vec(1)
except TypeError as e:
    print("缺参数报错对比 —— dataclass 版：", e)
try:
    HandVec(1)
except TypeError as e:
    print("                          手写版：", e)
print("结论：@dataclass = 读注解，把上面这些函数生成好塞进类字典，仅此而已。")
