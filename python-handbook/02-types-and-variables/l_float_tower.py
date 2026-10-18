"""第 02 章第 13 节：0.1 + 0.2 != 0.3 的真相，以及 1 == 1.0 为什么成立。

跑法：python3 l_float_tower.py
（本节证据就是这段脚本的输出，demo.sh 里有对应小节。）
"""
import math
import sys

print("=" * 68)
print("一、0.1 + 0.2 != 0.3：不是 Python 的 bug，是二进制小数的必然")
print("=" * 68)
print("0.1 + 0.2 == 0.3  →", 0.1 + 0.2 == 0.3)
print("0.1 + 0.2 精确到 20 位：", format(0.1 + 0.2, ".20f"))
print("0.3   精确到 20 位：", format(0.3, ".20f"))
print("0.1 真实存的值（53 位二进制还原成十进制）：", format(0.1, ".55f"))
import decimal
print("用 Decimal 看 0.1 的真身：", decimal.Decimal(0.1))
print("正确姿势一：比较用容差", math.isclose(0.1 + 0.2, 0.3))
print("正确姿势二：要精确就用 Decimal（十进制精确，标准库自带）",
      decimal.Decimal("0.1") + decimal.Decimal("0.2") == decimal.Decimal("0.3"))
print("错误姿势：把 float 直接当钱数累加 —— 0.1 加十次：", end=" ")
acc = 0.0
for _ in range(10):
    acc += 0.1
print(repr(acc), "；等于 1 吗？", acc == 1)

print()
print("=" * 68)
print("二、1 == 1.0：跨类型比较怎么成立的（数值塔）")
print("=" * 68)
print("1 == 1.0 →", 1 == 1.0, "；type(1) =", type(1).__name__, "type(1.0) =", type(1.0).__name__)
one_int, one_float = 1, 1.0
print("它们是同一个对象吗（is）→", one_int is one_float, "← 不同类型必然是两个对象")
print("int 和 float 能比，是因为它们都在 numbers 塔的「精确值」语义下比较：")
import numbers
print("  isinstance(1, numbers.Integral) =", isinstance(1, numbers.Integral))
print("  isinstance(1.0, numbers.Integral) =", isinstance(1.0, numbers.Integral))
print("  isinstance(True, numbers.Integral) =", isinstance(True, numbers.Integral), "← bool 也算整数")
print("关键：int 和 float 直接比时，Python 不偷懒把 int 转成 float，它按精确值比 ——")
big = 2**53
print("  float 只能精确表示 2^53 以内那一档连续整数。先把两边都转成 float 再比：")
print("    float(2**53 + 1) == float(2**53) →", float(big + 1) == float(big), " ← 转 float 就撞车了（精度不够）")
print("  再让 int 直接和 float 比（不转换）：")
print("    2**53 + 1 == float(2**53) →", (big + 1) == float(big), " ← 仍然 False：这是精确比较的结果")
print("  对照纯 int 之间：2**53 + 1 == 2**53 →", (big + 1) == big)
print("→ C 里 int 和 float 比较靠隐式转换，撞车是无声无息的；Python 这条路上不会撞。")

print()
