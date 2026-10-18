"""第 02 章第 12 节：NaN 跟谁都不相等（包括它自己）；inf 和 -0.0 也有话说。

跑法：python3 k_nan.py
（本节证据就是这段脚本的输出，demo.sh 里有对应小节。）
"""
import math
import sys

print("=" * 68)
print("一、NaN：按 IEEE 754，它跟谁都不相等，包括它自己")
print("=" * 68)
nan = float("nan")
print("nan == nan  →", nan == nan)
print("nan != nan  →", nan != nan, "← 唯一满足 x != x 的值")
print("nan is nan  →", nan is nan, "← 但对象还是同一个：is 比地址，== 比数值规则")
print("math.isnan(nan) →", math.isnan(nan))
print("两个不同的 NaN 对象也互不相等：", float("nan") == float("nan"))
print("坑：`if x != x:` 判 NaN 能用，但 list 里找 NaN 走的是 is 短路 —— ")
print("  [nan].count(nan) =", [nan].count(nan), "；nan in [nan] =", nan in [nan])
print("→ 同一性短路让容器操作「看起来正常」，排序/聚合时 NaN 会一路传染。")
print("  sorted 遇 NaN 不报错但顺序未定义：", end=" ")
try:
    print(sorted([3, nan, 1]))
except Exception as err:
    print(type(err).__name__, err)

print()
print("=" * 68)
print("二、float 相等判定的边界：inf、-0.0、还有 hash")
print("=" * 68)
inf = float("inf")
print("inf == inf →", inf == inf, "；inf > 10**1000 →", inf > 10**1000)
print("-0.0 == 0.0 →", -0.0 == 0.0, "；但符号还在：", math.copysign(1, -0.0))
print("于是 dict 里 -0.0 和 0.0 是同一个键：", len({-0.0: "a", 0.0: "b"}))
print("hash(1) == hash(1.0) →", hash(1) == hash(1.0), "← 相等的数必须同哈希，这是哈希表的合同")
print("hash(True) == hash(1) →", hash(True) == hash(1))
print("→ 这套「跨类型相等 + 同哈希」是 Python 数值塔的规则，")
print("  C 里 int/float 比较靠隐式转换，2^53+1 那种撞车是无声无息的。")
