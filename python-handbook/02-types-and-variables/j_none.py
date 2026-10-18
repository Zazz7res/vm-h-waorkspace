"""第 02 章第 11 节：None 是全进程唯一的对象，而且「空」不等于 None。

跑法：python3 j_none.py
（本节证据就是这段脚本的输出，demo.sh 里有对应小节。）
"""
import copy
import sys


class MyBoolish(int):
    """继承 int 的自定义类：用来证明 issubclass(bool, int) 不是空话。"""


print("=" * 68)
print("一、None：整个进程里只有一个")
print("=" * 68)
print("type(None) =", type(None).__name__)
print("None is None =", None is None)
a = None
b = None
print("两个 None 变量同一个对象吗？", id(a) == id(b))
print("bool(None) =", bool(None), "；None == 0 =", None == 0, "；None == False =", None == False)
print("copy.copy(None) is None =", copy.copy(None) is None)
print("copy.deepcopy(None) is None =", copy.deepcopy(None) is None)
print("sys.getsizeof(None) =", sys.getsizeof(None), "字节（它不携带任何数据，本身就是个记号）")

print()
print("None 和「空」是两件事，判空必须分开写：")
for probe in ([], "", {}, 0, False, None):
    print("  %-6r  为假值 %s   is None %s" % (probe, bool(probe) is False, probe is None))
print("→ `if not x:` 会把 0 / '' / [] / None 一视同仁；要区分「没传」和「传了空」必须写 is None。")

print()
print("=" * 68)
print("二、函数默认返回值就是 None（不是「没返回」）")
print("=" * 68)


def forgetful(x):
    """没有 return：其实返回了 None，这是新手最常见的 NoneType 报错来源。"""
    x + 1


ret = forgetful(2)
print("ret =", ret, "；type(ret) =", type(ret).__name__)
try:
    forgetful(2).bit_length()
except AttributeError as err:
    print("报错原文：", err)
