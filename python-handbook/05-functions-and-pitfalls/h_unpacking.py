#!/usr/bin/env python3
# ============================================================================
# h_unpacking.py —— 第 05 章第 11 节：* 和 ** 的另一半 —— 解包
#
# 同一个符号两个方向：
#   def 处  =打包（把多出来的收进 tuple / dict）   ← 第 9 节
#   调用处/字面量处 =解包（把一个可迭代对象摊成多个位置/多个键值） ← 本节
# 记住：解包**只摊一层**，而且只复制「槽位里的引用」（第 04 章浅拷贝那条规律）。
# ============================================================================


def probe(*args, **kw):
    print("    args =", args, f"（{type(args).__name__}）")
    print("    kw   =", kw, f"（{type(kw).__name__}）")


def dist(x1, y1, x2, y2):
    return ((x2 - x1) ** 2 + (y2 - y1) ** 2) ** 0.5


print("=== 1. 一个符号，两个方向 ===")
nums = [1, 2, 3]
print("  不写星号：整个 list 当成**一个**参数塞进去")
probe(nums)
print("  写一个星号：摊成三个位置参数")
probe(*nums)
print("  字典配两个星号：整份灌进关键字筐")
probe(**{"名字": "学习者", "层级": 1})

print()
print("=== 2. 解包用在任何可迭代对象上（不限 list）===")
print("  max(*[3, 9, 1]) =", max(*[3, 9, 1]), "｜ 对比 max([3, 9, 1]) =", max([3, 9, 1]))
print("  dist(*[0, 0, 3, 4]) =", dist(*[0, 0, 3, 4]))
print("  拆两段一起传：dist(*[0, 0], *[3, 4]) =", dist(*[0, 0], *[3, 4]))
print("  字符串也能摊：probe(*'ab') →", end=" ")
probe(*"ab")
print("  range 也能摊：", [*range(5)])

print()
print("=== 3. 星号接收（解包用法的第三种出现位置：赋值左边）===")
first, *rest = [1, 2, 3, 4]
print("  first, *rest =", first, rest, f"（rest 恒为 {type(rest).__name__}）")
*a, last = "hello"
print("  *a, last =", a, repr(last))
only, *nothing = [42]
print("  只剩空表时也照样是 list（不是 None）：", only, nothing, len(nothing))
p, q = 1, 2
p, q = q, p
print("  交换不用临时变量（右边先打包成元组再摊开）：p, q =", p, q)

print()
print("=== 4. zip(*rows) 转置：本章最实用的一招 ===")
rows = [("名字", 18, "北京"), ("代号", 20, "上海"), ("年龄", 22, "广州")]
print("  rows 有", len(rows), "行，每行", len(rows[0]), "列")
print("  zip(*rows) 展开成：", list(zip(*rows)))
print("  想要「按列」遍历就靠它：", [list(col) for col in zip(*rows)])
print("  手写等价式：zip(rows[0], rows[1], rows[2]) →", list(zip(rows[0], rows[1], rows[2])) == list(zip(*rows)))

print()
print("=== 5. 字面量里的解包（3.5+）：合并容器不靠额外函数 ===")
a1, a2, t1 = [1, 2], (3, 4), [5]
print("  [*a1, *a2, 5] =", [*a1, *a2, 5])
print("  {**{'x': 1}, 'y': 2} =", {**{"x": 1}, "y": 2})
print("  set 里也行：{*a1, *a2} =", {*a1, *a2})
print("  3.9+ 的 | 也能合字典（第 04 章讲过 dict + 会 TypeError）：", {"x": 1} | {"y": 2})
print("  函数调用里传合并后的参数：dist(*[*[0, 0], *[3, 4]]) =", dist(*[*[0, 0], *[3, 4]]))

print()
print("=== 6. 解包只摊一层：槽位复制的是引用（第 04 章那条规律再验一次）===")
src = [[1], [2]]
copy = [*src]
print("  外层是两个不同的 list：copy is src →", copy is src)
print("  内层是同一个 list：copy[0] is src[0] →", copy[0] is src[0])
copy[0].append(99)
print("  改 copy[0] 里面的东西，src 跟着脏：", src)

print()
print("=== 7. 三个高频用途收尾 ===")
print("  ① 打印时摊开：print(*rows, sep=';') →", end=" ")
print(*[1, 2, 3], sep="+", end=" ")
print("｜ ② 把可迭代对象变 tuple：tuple(zip(*rows)) 的元素数 =", len(tuple(zip(*rows))))


def call_with_config(fn, cfg):
    return fn(**cfg)


print("  ③ 配置字典驱动调用：call_with_config(dist, {'x1':0,'y1':0,'x2':6,'y2':8}) =",
      call_with_config(dist, {"x1": 0, "y1": 0, "x2": 6, "y2": 8}))
