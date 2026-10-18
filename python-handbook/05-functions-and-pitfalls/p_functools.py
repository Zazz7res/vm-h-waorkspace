#!/usr/bin/env python3
# ============================================================================
# p_functools.py —— 第 05 章第 19 节：functools 三件套（partial 造「定了参数的函数」、lru_cache 白送记忆化）
#
# partial：把一部分参数**固定**下来，得到一个新对象。它不是闭包（没有 cell），是 functools.partial 实例。
# lru_cache：给「参数可哈希」的函数加一层备忘录。它是装饰器（第 23 节），这里先当黑盒用。
# ============================================================================

import functools


def power(base, exp):
    return base ** exp


print("=== 1. partial：固定一部分参数 ===")
square = functools.partial(power, exp=2)
print("  square(9) =", square(9))
print("  type(square) =", type(square).__name__)
print("  square.func is power →", square.func is power)
print("  square.args =", square.args, "｜ square.keywords =", square.keywords)
print("  没有 __name__：", end=" ")
try:
    square.__name__
except AttributeError as e:
    print("AttributeError |", e)

print()
print("=== 2. 位置参数从左头塞进去，调用时的参数接在后面 ===")
cube = functools.partial(power, 2)          # 固定的是第一个位置参数
print("  cube = partial(power, 2) → cube(10) =", cube(10), "（等价 power(2, 10)）")
print("  调用时再给位置参数：cube(3, 4) →", end=" ")
try:
    cube(3, 4)
except TypeError as e:
    print("TypeError |", e)
print("  关键字能覆盖固定的值：square(9, exp=3) =", square(9, exp=3))
print("  也能整批转发：functools.partial(power)(3, 2) =", functools.partial(power)(3, 2))

print()
print("=== 3. partial vs lambda：两条路，账不一样 ===")
square_lambda = lambda base: power(base, 2)
print("  结果一样：", square(9) == square_lambda(9))
print("  partial 是对象不是函数（inspect 看到的是 __call__）；lambda 是真函数，有 __name__：",
      repr(square_lambda.__name__))
print("  调试时 partial 少一层栈帧；写回调/注册表时两种都常见 —— 记住报错里名字会丢就行。")

print()
print("=== 4. lru_cache：给递归函数加记忆，看命中账本 ===")
runs = 0


@functools.lru_cache(maxsize=None)
def fib(n):
    global runs
    runs += 1
    return n if n < 2 else fib(n - 1) + fib(n - 2)


print("  fib(30) =", fib(30), "｜ 函数体真跑了", runs, "次（不加缓存是 269 万次级别）")
before = runs
print("  再要一次 fib(30) =", fib(30), "｜ 函数体又多跑了吗 →", runs != before)
print("  命中账本：", fib.cache_info())
print("  参数表：", fib.cache_parameters())
print("  它是被包过的函数：type(fib).__name__ =", type(fib).__name__, "｜ __name__ 还在吗 →", fib.__name__)
fib.cache_clear()
print("  cache_clear() 之后：", fib.cache_info(), "｜ 函数体跑的总次数（含上一轮）=", runs)

print()
print("=== 5. lru_cache 的门票：参数必须可哈希（第 06 章 __hash__ 在这儿咬人）===")


@functools.lru_cache
def take(xs):
    return len(xs)


try:
    take([1, 2])
except TypeError as e:
    print("  传 list：", "TypeError |", e)
print("  换 tuple 就行：", take((1, 2)), take((1, 2)), "｜ 账本 =", take.cache_info())
print("  自定义类没写 __eq__ 时默认可哈希（按身份），写了 __eq__ 就不可哈希 —— 第 06 章第 7 节那条：")


@functools.lru_cache
def take_obj(obj):
    return id(obj) % 2


class WithEq:
    def __eq__(self, other):
        return True


try:
    take_obj(WithEq())
except TypeError as e:
    print("    TypeError |", e)
print("  （这就是「写 __eq__ 忘 __hash__」最典型的爆炸点：缓存/集合/dict 键。）")

print()
print("=== 6. maxsize 会淘汰：演示「算了三次但缓存里只留两个」 ===")


@functools.lru_cache(maxsize=2)
def square_cached(n):
    print("      （算了）", n)
    return n * n


for n in (1, 2, 3, 1):
    print(f"    square_cached({n}) =", end=" ")
    print(square_cached(n))
print("  最后账本：", square_cached.cache_info())

print()
print("=== 7. reduce：把二元操作折叠成一个值（第 04 章讲过 itertools，这条归 functools）===")
print("  reduce(lambda a, b: a * b, [1,2,3,4]) =", functools.reduce(lambda a, b: a * b, [1, 2, 3, 4]))
print("  带初值（空表也不炸）：", functools.reduce(lambda a, b: a + b, [], 100))
try:
    functools.reduce(lambda a, b: a + b, [])
except TypeError as e:
    print("  空表不带初值：", "TypeError |", e)
print("  实际项目里更常用 sum()/math.prod()/最大公约定制函数，reduce 读起来最费劲。")

print()
print("=== 8. 收口 ===")
print("  partial = 少写几个参数的语法；lru_cache = 用「参数可哈希」换「不重复算」；")
print("  两者都是「造一个新对象包住原函数」，原函数不受影响（第 23 节装饰器讲的正是这个套路的语法糖）。")
