#!/usr/bin/env python3
# ============================================================================
# b_default_once.py —— 第 05 章第 4 节：默认值只在 `def` 那一刻求值**一次**
#
# 这是全书最值钱的一条机制，也是可变默认参数那个坑的**唯一根因**。
# 记住一句话：`def f(n=表达式)` 里的「表达式」属于 def 语句，不属于调用。
# def 跑一次 = 算一次 = 结果存进 f.__defaults__，以后每次调用都复用那个**对象**。
# ============================================================================

import time

print("=== 1. 用一个会变的表达式当默认值：它是几就是几，不再变 ===")
built = 0


def make_default():
    global built                # 第 15 节会讲为什么必须写 global
    built += 1
    return built


def with_default(n=make_default()):    # ← 这一行执行时就把 make_default() 算完了
    return n


print("def 跑完之后 built =", built, "（表达式被调用了一次）")
print("连调三次 with_default()：", with_default(), with_default(), with_default())
print("三次之后 built =", built, "← 还是 1，说明函数体里**没有**再算默认值")

print()
print("=== 2. 拿时间做同一个实验：睡 0.3 秒也不会让默认值变新 ===")


def stamp(t=time.time()):
    return t


first, second = stamp(), stamp()
print("两次调用相等吗（==）：", first == second, "｜ 是同一个对象吗（is）：", first is second)
print("def 之后过了 0.3 秒，默认值还是那个旧时刻吗：", end=" ")
time.sleep(0.3)
print(stamp() == first)
print("证据在账本上：stamp.__defaults__ 里那个数 == 第一次调用的返回值 →",
      stamp.__defaults__[0] == first)

print()
print("=== 3. 账本可以直接改：__defaults__ 是个元组，赋值就换默认值 ===")
print("改之前 with_default.__defaults__ =", with_default.__defaults__)
with_default.__defaults__ = (777,)
print("改之后 with_default() =", with_default(), "｜ 账本 =", with_default.__defaults__)
print("（这招能救命：调试时想看「默认值到底是哪个对象」，直接 print f.__defaults__）")

print()
print("=== 4. def 那一刻算不出来，就在 def 那行报错（不是调用时）===")
try:
    exec("def f(x=helper()):\n    return x", {"__name__": "__demo__"})
except NameError as e:
    print("调一个还没定义的东西当默认值：", type(e).__name__, "|", e)
print("换成调用时才找名字就没事：")
exec("def g(x=None):\n    return helper()\ng()", {"__name__": "__demo__", "helper": lambda: "运行时才有"})
print("  （函数体里的名字是**调用时**才查的，这正和第 15 节 LEGB 对上）")

print()
print("=== 5. 默认值 vs 函数体里读全局：一个拍快照，一个每次现读 ===")
LIMIT = 3


def pick(n=LIMIT):              # def 时 LIMIT 还是 3 → 快照成 3
    return n


def pick_live():                # 没有默认值，函数体里现查全局
    return LIMIT


LIMIT = 99
print("改了全局 LIMIT 之后：pick() =", pick(), "｜ pick_live() =", pick_live())
print("  pick 的账本：", pick.__defaults__, "← 99 根本没进来")

print()
print("=== 6. 同一条规律的另外两个变体 ===")
BASE = [1, 2]


def uses_list_literal(x=BASE):      # 默认值是个「名字」：def 时查一次这个名字，拿到当时那个 list
    return x


print("默认值写名字，拿到的是 def 那一刻绑定的对象：", uses_list_literal())
BASE.append(99)
print("之后往 BASE 里塞东西，默认值那个对象跟着变吗：", uses_list_literal(), "（同一个 list，当然跟着变）")
print("  这行就是第 5 节那个坑的另一半：默认值不是快照**值**，是快照**引用**。")

print()
print("=== 7. 一句话收口 ===")
print("判据：看这行代码属于 def 还是属于调用。")
print("  属于 def（只跑一次）：默认值表达式、装饰器、函数体里 def 出来的名字绑定；")
print("  属于调用（每次都跑）：函数体里的每一行、参数表达式的求值（f(make_default()) 每次都会 +1）。")


def caller_side(n=make_default()):
    return n


print("对照：def 的默认值 1 次 vs 调用处传参每次都算：",
      end=" ")
before = built
caller_side()                      # 用默认值：built 不动
caller_side(make_default())        # 调用处现算：built +1
print("跑完两次之后 built 从", before, "变成", built)
