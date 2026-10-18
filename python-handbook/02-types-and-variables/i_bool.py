"""第 02 章第 10 节：bool 不是独立类型，它是 int 的子类。

跑法：python3 i_bool.py
（本节证据就是这段脚本的输出，demo.sh 里有对应小节。）
"""
import copy
import sys


class MyBoolish(int):
    """继承 int 的自定义类：用来证明 issubclass(bool, int) 不是空话。"""


print("=" * 68)
print("一、bool 是 int 的儿子")
print("=" * 68)
print("issubclass(bool, int) =", issubclass(bool, int))
print("bool.__mro__ =", " -> ".join(c.__name__ for c in bool.__mro__))
print("type(True) =", type(True).__name__, "；isinstance(True, int) =", isinstance(True, int))
print("type(True) is int 吗？", type(True) is int, "← 类型判断用 type() is 就把 bool 排除了")
print("True + 1 =", True + 1, "；False * 10 =", False * 10, "；sum([True,True,False]) =", sum([True, True, False]))
print("True == 1 =", True == 1, "；1 is True =", 1 is True, "← 值相等，但不是同一个对象")
print("True.bit_length() =", (True).bit_length(), "← bool 直接能用 int 的方法，说明它真是 int")
print("int 有没有 bool 的方法？ hasattr(int, 'bit_length') =", hasattr(int, "bit_length"))

print()
print("=" * 68)
print("二、bool 的坑：它是 int，所以会被算术吞掉")
print("=" * 68)
flags = [True, False, True]
print("len([t for t in flags if t]) =", len([t for t in flags if t]))
print("sum(flags) =", sum(flags), "（当计数用很方便，但类型上它是 int 不是 bool）")
print("把 2 转成 bool：bool(2) =", bool(2), "；2 == True =", 2 == True)
print("字典里 1 和 True 是同一个键：", len({1: "a", True: "b"}), "个键")
print("字典里 1 和 1.0 也算同一个键：", len({1: "a", 1.0: "b"}), "个键")
print("→ 因为键的判据是 ==，而 1 == True == 1.0；后面的赋值把前面的值盖掉了。")

print()
