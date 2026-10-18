#!/usr/bin/env python3
# ============================================================================
# o_higher.py —— 第 05 章第 18 节：把函数当参数交出去（sorted/key、map/filter、zip、any/all）
#
# 三条最容易记错的规则，这一节全部量出来：
#   ① key= 收的是**函数**，而且它收的是「每一个元素」，返回的才拿来比；
#   ② map/filter 返回的是**惰性对象**（type 是 map/filter），只能用一遍，长度都问不出来；
#   ③ sorted() 给你一张新表，list.sort() 原地改并返回 None —— 后者当参数传出去就是 None。
# ============================================================================

print("=== 1. key= 收函数：同一个表四种比法 ===")
rows = [("苹果", 3, "red"), ("香蕉", 1, "yellow"), ("桃子", 2, "pink")]
print("  默认按整个元组（比第一项，再比第二项…）：", sorted(rows))
print("  按第二项（数量）：", sorted(rows, key=lambda t: t[1]))
print("  按第三项长度：", sorted(rows, key=lambda t: len(t[2])))
print("  内置函数直接当 key：", sorted(["香蕉", "苹果", "桃子"], key=len))
print("  降序：", sorted(rows, key=lambda t: t[1], reverse=True))
print("  稳定排序：数量相同的不改原顺序（第 04 章 Counter 那条「同计数按插入顺序」同源）：")
same = [("甲", 1), ("乙", 1), ("丙", 0)]
print("    sorted(同数量) =", sorted(same, key=lambda t: t[1]))


def neg(n):
    return -n


print("  key 也可以是有名字的函数：", sorted([3, 1, 2], key=neg))
print("  坑点：key 里写 `t[1]` 的方括号是取值，写 `key=1` 就 TypeError（要的是函数不是值）：")
try:
    sorted(rows, key=1)
except TypeError as e:
    print("    TypeError |", e)

print()
print("=== 2. map/filter 是惰性对象，不是结果 ===")


def double(n):
    print("      （真的算了）", n)
    return n * 2


print("  m = map(double, [1,2,3]) —— 这一步一行都没跑")
m = map(double, [1, 2, 3])
print("  type(m) =", type(m).__name__)
try:
    len(m)
except TypeError as e:
    print("  问长度：", "TypeError |", e)
print("  list(m) 才开始算：")
result = list(m)
print("    →", result)
print("  再 list(m) 一次（已经空了）：", list(m), "← map 是一次性的，第 10 章生成器同族")
print("  推导式版没有这个问题：", [double(n) for n in [1, 2, 3]])
print("  filter 收「判真假的函数」，None 表示用元素本身的真假：")
print("    filter(None, [0,1,'','a',[],[1]]) =", list(filter(None, [0, 1, "", "a", [], [1]])))
print("    filter(lambda n: n % 2, range(6)) =", list(filter(lambda n: n % 2, range(6))))

print()
print("=== 3. sorted vs list.sort：一个给新表一个给 None ===")
data = [3, 1, 2]
new_table = sorted(data)
print("  sorted(data) =", new_table, "｜ 原表 data =", data)
inplace = data.sort()
print("  data.sort() 返回 =", inplace, "｜ 原表变成 =", data)
print("  把 data.sort() 直接传给下一步 = 传了 None：")
try:
    len(data.sort())
except TypeError as e:
    print("    TypeError |", e)
print("  想要「排完还拿到表」：", sorted(data), "或者 (data.sort(), data)[1] 这种丑写法不如上一行")

print()
print("=== 4. zip 也是惰性的（第 04 章讲过），配 map 时最常踩 ===")
print("  type(zip([1,2],[3,4])) =", type(zip([1, 2], [3, 4])).__name__)
print("  list(zip(...)) =", list(zip([1, 2], [3, 4])))
print("  长度不齐时按短的：", list(zip([1, 2, 3], ["a"])))
import itertools
print("  想按长的补默认值：", list(itertools.zip_longest([1, 2, 3], ["a"], fillvalue="-")))

print()
print("=== 5. any/all 收「判真假的函数」吗？不，收的是可迭代对象 ===")
nums = [1, 2, 3]
print("  any(n > 2 for n in nums) =", any(n > 2 for n in nums), "（有个真的就 True，且是惰性的）")
print("  all(n > 0 for n in nums) =", all(n > 0 for n in nums))
print("  any(nums) =", any(nums), "｜ all([]) =", all([]), "（空表：any=False / all=True，是数学约定）")
print("  误写成函数会怎样：")
try:
    any(lambda n: n > 2, nums)
except TypeError as e:
    print("    TypeError |", e)
print("  any/all 短路：第一个真的出现就不再往下算 —— 用生成器传参才有这效果：")
checked = []


def probe(n):
    checked.append(n)
    return n > 2


print("    any(probe(n) for n in [1, 2, 3, 4]) =", any(probe(n) for n in [1, 2, 3, 4]),
      "｜ 实际只算了", checked, "这几个")

print()
print("=== 6. 收口：什么时候用哪个 ===")
print("  只是「取每个元素」→ 推导式（第 04 章）；需要「把已有函数原样套上去」→ map；")
print("  需要「按条件筛」→ 推导式带 if 或 filter；判断「有没有/全都」→ any/all + 生成器（能短路）。")
