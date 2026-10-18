"""第 06 章第 11 节：frozen=True —— 生成 __setattr__ 拦你，报错原文全在这。

跑法：python3 i_dc_frozen.py

frozen=True 的实现就是往类字典里塞一个拒绝一切的 __setattr__/__delattr__。要点：
  * 报的是 dataclasses.FrozenInstanceError，它是 AttributeError 的儿子；
  * frozen + eq=True 会顺带生成 __hash__（不 frozen 反而把哈希废了，和第 7 节连起来）；
  * 想「改」请用 dataclasses.replace()：它造新的，不动旧的。
"""
from dataclasses import FrozenInstanceError, dataclass, replace


@dataclass(frozen=True, eq=True)
class Config:
    host: str
    port: int = 8000


@dataclass(eq=True)
class Mutable:
    """对照组：非 frozen 的 eq dataclass，__hash__ 被设成 None。"""

    v: int


c = Config("localhost")

print("=" * 68)
print("一、赋值报什么原文（捕获版，和裸 traceback 同一行字）")
print("=" * 68)
try:
    c.host = "evil.example"
except FrozenInstanceError as e:
    print("FrozenInstanceError:", e)
try:
    del c.port
except FrozenInstanceError as e:
    print("删除也一样：", e)
print("FrozenInstanceError 是 AttributeError 的儿子吗：", issubclass(FrozenInstanceError, AttributeError))

print()
print("=" * 68)
print("二、frozen 反而白送 __hash__：能进 set；非 frozen 的 eq 类不能")
print("=" * 68)
print("Config(1,1) == Config(1,1)：", Config("a", 1) == Config("a", 1))
pool = {Config("a", 1), Config("a", 1), Config("b", 2)}
print("放进 set 去重后剩：", len(pool), "个")
print("对照组 Mutable.__hash__ is None：", Mutable.__hash__ is None, "← 第 7 节那条规则，dataclass 也守")
try:
    {Mutable(1)}
except TypeError as e:
    print("把 Mutable 放进 set → TypeError:", e)

print()
print("=" * 68)
print("三、frozen 的正确「改法」：replace 造新物件")
print("=" * 68)
c2 = replace(c, port=9000)
print("原 c =", c, "　新 c2 =", c2)
print("c2 是新对象吗：", c2 is not c, "｜ 旧对象字段没被动：", c.port)

print()
print("=" * 68)
print("四、真要写内部缓存怎么办：正路是不进字段（或 ClassVar），歪路给你看一眼")
print("=" * 68)
object.__setattr__(c, "port", 1234)
print("歪路：object.__setattr__ 能硬写（c.port 现在是", c.port, "）——这是逃生舱不是日常 API")
object.__setattr__(c, "port", 8000)  # 恢复，别让后面的输出带着脏数据
