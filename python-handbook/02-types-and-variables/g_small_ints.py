"""第 02 章第 7 节：小整数缓存的边界，自己扫出来，不抄网上数字。

跑法：python3 g_small_ints.py

关键手法：必须用**运行时算出来**的整数，不能用字面量。
  * `256 is 256` 在 3.14 里恒为 True —— 编译器把两个 256 折成同一个常量对象，
    而且解释器还会警告你「别拿 is 比 int 字面量」（第 8 节的现场）。
  * 所以这里所有整数都过一道 `mk()`：参数是运行时值，`+ 0` 让 C 层重新算出一个整数，
    缓存才会在「该生效的时候」生效。
"""
import sys


def mk(n):
    """运行时生成一个整数：n + 0 走 int 加法，结果可能从缓存里拿现成的对象。"""
    return n + 0


print("=" * 68)
print("一、直接比字面量是无效实验（3.14 还会骂你）")
print("=" * 68)
print("demo 第 8 节里能看到这条警告原文：")
print('    SyntaxWarning: "is" with \'int\' literal. Did you mean "=="?')
print("本机实测：写成字面量的 `256 is 256` 和 `257 is 257` 都是 True —— 两个字面量被折成同一个常量。")
print("→ 网上那句「257 is 257 是 False」在这台 3.14 上直接复现不了，必须换成运行时值。")

print()
print("=" * 68)
print("二、扫一遍：哪些整数会从缓存里发")
print("=" * 68)
low, high = -300, 400
cached = [n for n in range(low, high + 1) if mk(n) is mk(n)]
print("扫描区间：", low, "..", high)
print("命中缓存的区间：", cached[0], "..", cached[-1], "  共", len(cached), "个")
print("这段区间是不是连续不断？", cached == list(range(cached[0], cached[-1] + 1)))
print("上边界那一个：", cached[-1], "→ mk(n) is mk(n) =", mk(cached[-1]) is mk(cached[-1]))
print("上边界再往上一个：", cached[-1] + 1, "→", mk(cached[-1] + 1) is mk(cached[-1] + 1))
print("下边界那一个：", cached[0], "→", mk(cached[0]) is mk(cached[0]))
print("下边界再往下一个：", cached[0] - 1, "→", mk(cached[0] - 1) is mk(cached[0] - 1))
print("→ 本机边界实测结论：闭区间", cached[0], "到", cached[-1])

print()
print("=" * 68)
print("三、缓存对象是「同一个对象」，不是「相等的两个对象」")
print("=" * 68)
seven = mk(7)
another = mk(mk(70) - mk(63))
print("7 和 70-63（都过运行时）是同一个对象吗？", seven is another)
big = mk(2**31)
other = mk(2**31)
print("2**31（缓存外）两次运算结果同一个对象吗？", big is other, "；但相等：", big == other)
print("→ 缓存外 == 成立、is 不成立。所以「用 is 判数值」永远是错的：")
print("  它在小整数区间里刚好对，出了区间就翻脸 —— 这种「时对时错」最难查。")

print()
print("=" * 68)
print("四、这是 CPython 的实现细节，不是语言保证")
print("=" * 68)
print("sys.int_info =", sys.int_info)
print("语言参考里没有任何一处保证整数缓存的区间大小；换解释器（PyPy）或换版本都可能不同。")
print("顺带看看别的内置单例走没走同一条路：")
print("  bool(1) is True ：", bool(1) is True)
print("  None is None    ：", None is None)
one_char = "x"
print("  sys.intern(那个字符串) is 同一个变量 ：", sys.intern(one_char) is one_char)
