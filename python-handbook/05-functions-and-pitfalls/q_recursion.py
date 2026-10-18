#!/usr/bin/env python3
# ============================================================================
# q_recursion.py —— 第 05 章第 20 节：递归的边界（限额是解释器数的，尾递归不会变循环）
#
# 三条实测：① sys.getrecursionlimit() 是软限制，能调，但栈是真内存；
#          ② 超限报 RecursionError（也是 RuntimeError 的子类），不是段错误；
#          ③ Python 没有尾调用优化：`return f(n-1)` 一样叠帧 —— 想深就自己写循环+栈
#             （你在 C 里给树写的迭代遍历，到这边是同一招）。
# ============================================================================

import inspect
import sys

print("=== 1. 默认限额与它的形状 ===")
print("  sys.getrecursionlimit() =", sys.getrecursionlimit())


def depth(n):
    if n == 0:
        return 0
    return 1 + depth(n - 1)


print("  depth(500) =", depth(500), "（离限额还远）")
try:
    depth(1500)
except RecursionError as e:
    print("  depth(1500)：", "RecursionError |", e)
print("  RecursionError 是谁的子类：", [c.__name__ for c in type(RecursionError("x")).__mro__[:4]])

print()
print("=== 2. 调高限额能跑更多层（但这是借来的栈空间，不是免费的）===")
old_limit = sys.getrecursionlimit()
sys.setrecursionlimit(4000)
print("  新限额 =", sys.getrecursionlimit())
print("  depth(2000) =", depth(2000))
sys.setrecursionlimit(old_limit)
print("  跑完恢复 =", sys.getrecursionlimit(), "（脚本里临时改完一定要改回去）")

print()
print("=== 3. 无限递归：解释器数到第几层喊停 ===")
reached = 0


def boom(n):
    global reached
    reached = n
    return boom(n + 1)


try:
    boom(0)
except RecursionError as e:
    print("  最深跑到第", reached, "层就停了（限额 =", sys.getrecursionlimit(), "，还要扣掉外面已有的帧）")
    print("  报错原文 =", e)
print("  注意：抛出来之后所有帧**瞬间退光**，except 块里再数栈就只剩外面那一两层了。")

print()
print("=== 4. 尾递归不会被优化：累加版照样炸 ===")


def sum_rec(n, acc=0):
    if n == 0:
        return acc
    return sum_rec(n - 1, acc + n)     # 看起来像「尾递归」，Python 照样叠一层帧


print("  sum_rec(100) =", sum_rec(100))
print("  sum_rec(100000) →", end=" ")
try:
    sum_rec(100000)
except RecursionError as e:
    print("RecursionError |", e)


def sum_loop(n):
    total = 0
    for i in range(1, n + 1):
        total += i
    return total


print("  循环版 sum_loop(100000) =", sum_loop(100000), "（不吃栈）")

print()
print("=== 5. 「自己造栈」的模板：这就是你 C 里迭代遍历二叉树那套 ===")


def walk(n):
    stack = [n]
    seen = 0
    peak = 0
    while stack:
        peak = max(peak, len(stack))
        cur = stack.pop()
        seen += 1
        if cur > 1:
            stack.append(cur - 1)
    return seen, peak


print("  walk(50) → 处理个数 / 栈峰值 =", walk(50))
print("  递归版同样规模要 50 层帧；这里栈峰值只有 1（线性结构）—— 换成树才是队列/栈的用武之地。")

print()
print("=== 6. 递归里最贵的其实是「每层一个新帧 + 局部变量表」===")


def layers(k):
    if k == 0:
        return len(inspect.stack())       # 在最深处数一数此刻栈上有几个帧
    return layers(k - 1)


print("  layers(5) 在最底层数到的帧数 =", layers(5), "（5 层递归 + 解释器和 demo 自带的那几层）")
print("  depth.__code__.co_varnames =", depth.__code__.co_varnames, "← 每层帧都要开这一份")
print("  第 21 节 dis 里能看到这些名字全靠 LOAD_FAST/STORE_FAST 搬进搬出，层数一多就是真金白银。")
print("  递归深度大时先问两句：能不能改循环？能不能带备忘录（第 19 节 lru_cache）？")

print()
print("=== 7. 一句话收口 ===")
print("  递归在 Python 里是「表达清晰」的工具，不是「性能」工具；")
print("  深度可预测、几十到几百层 → 放心写递归；深度可能上千 → 直接上循环 + 显式栈。")
