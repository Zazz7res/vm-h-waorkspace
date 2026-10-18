#!/usr/bin/env python3
# ============================================================================
# s_call_cost.py —— 第 05 章第 22 节：调用本身值多少钱（timeit 实测）
#
# 这一节的所有**绝对值每跑必抖**（本机 4 核，机器上还有别的东西在跑），
# 值得读的是**倍率**：谁比谁贵几倍、贵在哪一行。
# 口径写死在这里：timeit + min(repeat)，每次取最快的一轮，尽量避开别的进程干扰。
# ============================================================================

import timeit

REPEATS = 3


def bench(label, stmt, setup, number):
    timer = timeit.Timer(stmt, globals=setup)
    best = min(timer.repeat(repeat=REPEATS, number=number))
    ns = best / number * 1e9
    print(f"  {label:26} {ns:8.1f} ns/次（跑了 {number} 次，取 {REPEATS} 轮里最快的一轮）")
    return ns


print("=== 1. 一次空函数调用：这就是「把三行代码包成函数」的税 ===")
noop_ns = bench("空函数 f()", "f()", {"f": lambda: None}, 500_000)
inline_ns = bench("什么都不做的循环（对照）", "x = 1", {"x": 0}, 500_000)
print(f"  → 一次调用的税 ≈ {noop_ns - inline_ns:.1f} ns（{noop_ns / inline_ns:.1f} 倍）")


def one_arg(x):
    return x


print()
print("=== 2. 参数个数与打包方式 ===")
n0 = bench("f()（无参）", "f()", {"f": lambda: None}, 500_000)
n1 = bench("f(x)（一个位置参数）", "f(1)", {"f": one_arg}, 500_000)
n3 = bench("f(x, y, z)", "f(1, 2, 3)", {"f": lambda a, b, c: a}, 500_000)
nv = bench("f(*xs)（先解包 list）", "f(*xs)", {"f": lambda *a: a, "xs": [1, 2, 3]}, 500_000)
nk = bench("f(**kw)（先解包 dict）", "f(**kw)", {"f": lambda **k: k, "kw": {"a": 1}}, 500_000)
na = bench("def f(*a, **k) 被直接调用", "f(1, 2, k=3)", {"f": lambda *a, **k: (a, k)}, 500_000)
print(f"  → 三个参数比一个贵 {(n3 / n1):.2f} 倍；"
      f"**kw 解包比直接给关键字贵 {(nk / n1):.2f} 倍（要现造 dict 再摊开）")

print()
print("=== 3. 第 21 节那条规律的价钱：局部 vs 全局 vs 闭包 ===")
ACC = 0


def sum_local(items):
    total = 0
    for x in items:
        total += x
    return total


def sum_global(items):
    global GLOBAL_ACC
    GLOBAL_ACC = 0
    for x in items:
        GLOBAL_ACC += x
    return GLOBAL_ACC


def make_sum_closure():
    base = 0

    def inner(items):
        nonlocal base
        base = 0
        for x in items:
            base += x
        return base
    return inner


data = [1] * 100
s_ns = bench("累加进局部变量", "f(data)", {"f": sum_local, "data": data}, 20_000)
g_ns = bench("累加进全局变量", "f(data)", {"f": sum_global, "data": data}, 20_000)
c_ns = bench("累加进闭包变量（nonlocal）", "f(data)", {"f": make_sum_closure(), "data": data}, 20_000)
print(f"  → 同一件事：全局比局部贵 {(g_ns / s_ns):.2f} 倍，闭包比局部贵 {(c_ns / s_ns):.2f} 倍"
      f"（每轮多一次 LOAD/STORE_DEREF）")

print()
print("=== 4. 包一层装饰器的税（第 23 节讲的正是这个 wrapper）===")
import functools


def plain(n):
    return n + 1


@functools.wraps(plain)
def wrapped(n):
    return plain(n) + 0


p_ns = bench("直接调 plain(1)", "f(1)", {"f": plain}, 500_000)
w_ns = bench("调一层 wrapper(1)", "f(1)", {"f": wrapped}, 500_000)
print(f"  → 多一层函数调用 ≈ 贵 {(w_ns / p_ns):.2f} 倍；热循环里包三层就是三倍的税")

print()
print("=== 5. 内联 vs 拆函数：什么时候拆得不划算 ===")
big_data = list(range(1000))


def sum_all(items):
    return sum(items)


i_ns = bench("sum(data) 内联", "sum(data)", {"sum": sum, "data": big_data}, 2_000)
f_ns = bench("套一层自己的函数", "sum_all(data)", {"sum_all": sum_all, "data": big_data}, 2_000)
print(f"  → 这层封装的税 = {f_ns - i_ns:.1f} ns，相对总耗时只占 {(f_ns / i_ns - 1) * 100:.1f}%"
      f"（循环体够长时，函数边界这点钱根本不值一提）")

print()
print("=== 6. 怎么读这一节 ===")
print("  ① 函数边界很便宜（几十纳秒级），**为了可读性尽管拆**；")
print("  ② 真要抠的是循环体里的名字来源（局部最快）和每轮都做的解包；")
print("  ③ 上面这些数字只能当**量级**看，别把小数位抄进任何报告 —— 换一台机器、"
      "换个 CPU 占用率就全变，但倍率形状基本稳定。")
