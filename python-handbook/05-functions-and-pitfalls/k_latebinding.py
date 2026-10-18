#!/usr/bin/env python3
# ============================================================================
# k_latebinding.py —— 第 05 章第 14 节：★本章第二大坑 —— 闭包里的名字「晚绑定」
#
# 现场：`[lambda: i for i in range(3)]` 三个函数跑出来全是 2，不是 [0, 1, 2]。
# 原因和第 13 节同一条：内层函数存的是**盒子（名字）**，不是当时的值；
# 三个 lambda 抢的是同一个盒子，循环结束时那个盒子里躺着最后一个值。
# 修法三条，本质都是一个：**把当时的值拍成快照**（默认参数 / partial / 每次新建一层闭包）。
# ============================================================================

import functools

print("=== 1. 现场复现（函数里、模块里都一样）===")


def build_in_function():
    funcs = [lambda: i for i in range(3)]
    return [f() for f in funcs]


print("  函数里造三个 lambda，跑出来：", build_in_function())
funcs_at_module = [lambda: i for i in range(3)]
print("  模块级造三个 lambda，跑出来：", [f() for f in funcs_at_module])
print("  三个函数是同一个盒子的三个引用：",
      funcs_at_module[0].__closure__[0] is funcs_at_module[2].__closure__[0])
print("  那个盒子现在装的：", funcs_at_module[0].__closure__[0].cell_contents)

print()
print("=== 2. 为什么是 2：lambda 里那个 i 是「调用时才去查的名字」 ===")
print("  对比第 4 节：默认值属于 def（当场算），函数体里的名字属于调用（事后查）。")
print("  这条规律在 for 循环版上更明显：")
loop_funcs = []
for i in range(3):
    loop_funcs.append(lambda: i)
print("  for 版跑出来：", [f() for f in loop_funcs], "｜ 循环变量 i 还活着 =", i)
print("  （for 的循环变量会泄漏到外面，推导式里的不会 —— 第 6 节专门量这个差别）")

print()
print("=== 3. 修法 A：默认参数拍快照（最短，最常见）===")
fixed_default = [lambda i=i: i for i in range(3)]
print("  跑出来：", [f() for f in fixed_default])
print("  快照存在每个函数自己的账本里：三个 __defaults__ 分别是",
      [f.__defaults__ for f in fixed_default])
print("  有没有闭包盒子了：", fixed_default[0].__closure__)
print("  原理：`i=i` 的右边属于 def 那一刻（第 4 节），把当时那个值塞进默认值，之后各不相干。")

print()
print("=== 4. 修法 B：functools.partial（语义最清楚）===")
fixed_partial = [functools.partial(lambda i: i, n) for n in range(3)]
print("  跑出来：", [f() for f in fixed_partial])
print("  partial 把值存在对象上：", [f.args for f in fixed_partial])
print("  代价：拿到的是 partial 对象，不是函数（没有 __name__，第 19 节有账）。")

print()
print("=== 5. 修法 C：每轮新建一层闭包（工厂函数）===")


def make_getter(value):
    def getter():
        return value
    return getter


fixed_factory = [make_getter(n) for n in range(3)]
print("  跑出来：", [f() for f in fixed_factory])
print("  这次的盒子是三个不同的盒子：",
      fixed_factory[0].__closure__[0] is not fixed_factory[1].__closure__[0],
      "｜ 内容分别 =", [f.__closure__[0].cell_contents for f in fixed_factory])
print("  关键：外层函数的局部名字 value 每调一次 make_getter 就新造一个盒子。")

print()
print("=== 6. 顺带把「泄漏」量清楚：for 泄漏、推导式不泄漏、class 体另算（第 06 章）===")
for leaked in range(3):
    pass
print("  for 循环之外还能看到 leaked =", leaked)
try:
    [comp_var for comp_var in range(3)]
    comp_var                       # noqa
except NameError as e:
    print("  推导式变量出不来：", type(e).__name__, "|", e)


class Leaky:
    for cls_var in range(3):
        pass
    picked = cls_var               # class 体没有自己的「查找作用域」→ 循环变量在类里看得见


print("  class 体里的循环变量在类里可见：Leaky.picked =", Leaky.picked)
print("  但 class 体的名字不会进模块：'cls_var' 在 globals 里吗 →", "cls_var" in globals())

print()
print("=== 7. 同一族坑的实战形态：回调/信号里现取循环变量 ===")
handlers = {}
for key in ("a", "b"):
    handlers[key] = lambda: key    # ← 和上面一模一样的错法
print("  错法：handlers['a']() =", handlers["a"](), "（本该是 'a'）")
handlers_ok = {k: (lambda k=k: k) for k in ("a", "b")}
print("  修法：handlers_ok['a']() =", handlers_ok["a"]())

print()
print("=== 8. 判据一句话 ===")
print("  闭包/lambda 里出现的每个名字，都是**调用那一刻**去查的；")
print("  想要「那一刻之前的值」，就得在 def 行上把它变成默认参数，或者每次新建一层作用域。")
