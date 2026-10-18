#!/usr/bin/env python3
# ============================================================================
# u_capstone.py —— 第 05 章第 24 节：capstone，把本章零件装成一个真工具
#
# 要造的东西：一个「参数体检 + 失败重试 + 调用记账」的装饰器组，
# 和一个「按 (名字, 参数) 缓存结果」的小 memo。全程只用标准库。
# 每个零件都标了它来自本章第几节 —— 这一节是索引，也是验收。
# ============================================================================

import functools
import inspect

# ---------- 零件 1：签名自检（第 9 节：五种参数筐；账本：第 3 节）----------
def describe(fn):
    print(f"  {fn.__name__} 的签名：{inspect.signature(fn)}")
    for name, param in inspect.signature(fn).parameters.items():
        default = "必填" if param.default is inspect.Parameter.empty else repr(param.default)
        print(f"    {name:8} 类型={param.kind.name:24} 默认={default}")


# ---------- 零件 2：重试装饰器（第 23 节三层结构 + 第 9 节转发 + 第 13 节闭包记账）----------
def retry(times=3, exceptions=(ValueError,)):
    attempts = []                                   # 闭包盒子：每个被装饰函数一份账本

    def deco(fn):
        @functools.wraps(fn)                        # 第 19/23 节：把身份证贴回来
        def wrapper(*args, **kw):                   # 第 9 节：什么形状都能转发
            last = None
            for i in range(1, times + 1):
                attempts.append(fn.__name__)
                try:
                    return fn(*args, **kw)
                except exceptions as e:
                    last = e
                    print(f"    第 {i} 次失败：{type(e).__name__}: {e}")
            raise last
        wrapper.attempts = attempts                 # 第 2 节：函数对象可以挂属性
        return wrapper
    return deco


state = {"calls": 0}                                 # 第 7 节：模块级账本，函数里要 global/或用可变对象


@retry(times=4, exceptions=(ValueError,))
def flaky(threshold, *, note="无"):                 # 第 9 节：keyword-only 当「必须回答的问题」
    state["calls"] += 1
    if state["calls"] < threshold:
        raise ValueError(f"才第 {state['calls']} 次，还没到 {threshold}")
    return f"第 {state['calls']} 次成功（备注：{note}）"


print("=== 1. 先让工具自己交代签名（第 9 节的五个筐，一眼看全）===")
describe(flaky)
describe(retry)

print()
print("=== 2. 重试：前三次失败、第四次成功 ===")
state["calls"] = 0
print("  flaky(4, note='走网络') →", flaky(4, note="走网络"))
print("  累计尝试次数（装饰器自己记的账）：", len(flaky.attempts), "｜ 前两笔 =", flaky.attempts[:2])
print("  身份证还在吗：__name__ =", flaky.__name__, "｜ __wrapped__ =", flaky.__wrapped__.__name__)

print()
print("=== 3. 重试全部失败：把最后一次的异常原样抛出去 ===")
state["calls"] = 0
try:
    flaky(99, note="永远不到")
except ValueError as e:
    print("  抛回来了：ValueError |", e, "｜ 一共试了", len(flaky.attempts), "次")

print()
print("=== 4. partial 把「常用配置」定下来（第 19 节）===")
always_three = functools.partial(flaky, 3)
state["calls"] = 0
print("  always_three() →", always_three())
print("  partial 对象没有 __name__，但有 func/keywords：", always_three.func.__name__, always_three.keywords)

print()
print("=== 5. memo：给纯函数加缓存（第 19 节 lru_cache 的手工版，顺便看清门票）===")
hits = {"call": 0}


@functools.lru_cache(maxsize=32)
def score(name, weights=(1, 2)):                   # 第 5 节：默认值给 tuple（不可变）才敢进缓存
    hits["call"] += 1
    return sum(w * (len(name) + i) for i, w in enumerate(weights))


print("  score('ab') =", score("ab"))
print("  再来一次同样的：", score("ab"), score("ab"), "｜ 函数体跑了几次：", hits["call"])
print("  传 list 当参数 → 缓存门票都没了：")
try:
    score("ab", [1, 2])
except TypeError as e:
    print("    TypeError |", e)
print("  账本：", score.cache_info())

print()
print("=== 6. 记账装饰器：默认参数那条坑在这里会怎么咬人（第 5 节回马枪）===")


def naive_log(fn, log=[]):                          # ← 可变默认值：所有被装饰函数共用一本账
    @functools.wraps(fn)
    def wrapper(*a, **k):
        log.append(fn.__name__)
        return fn(*a, **k)
    wrapper.log = log
    return wrapper


@naive_log
def one():
    return 1


@naive_log
def two():
    return 2


one(), one(), two()
print("  one.log =", one.log)
print("  two.log =", two.log, "← 两本账合成了一本（两个函数共享同一个 list）")
print("  证据：", one.log is two.log)
print("  正解（第 6 节）：把默认值换成 None 哨兵，或者让 log 待在**每次装饰新建的闭包**里：")


def good_log(fn, log=None):
    if log is None:
        log = []                                    # 每次装饰各造一本（第 13 节：每层闭包一个新盒子）

    @functools.wraps(fn)
    def wrapper(*a, **k):
        log.append(fn.__name__)
        return fn(*a, **k)
    wrapper.log = log
    return wrapper


@good_log
def three():
    return 3


@good_log
def four():
    return 4


three(), three(), four()
print("  three.log =", three.log, "｜ four.log =", four.log, "｜ 同一本吗：", three.log is four.log)

print()
print("=== 7. 本章零件清单（工具里每一行都能回指到某节）===")
table = [
    ("函数对象可挂属性", "第 2 节"),
    ("inspect.signature / __code__ 当账本", "第 3 节"),
    ("默认值只在 def 时求值", "第 4 节"),
    ("可变默认参数坑 + None 哨兵", "第 5、6 节"),
    ("原地改 vs 重绑定（state 字典）", "第 7 节"),
    ("五种参数筐与 *args/**kwargs 转发", "第 9 节"),
    ("报错原文认形状", "第 10 节"),
    ("闭包盒子做记账本", "第 13 节"),
    ("global / nonlocal", "第 15 节"),
    ("partial / lru_cache / wraps", "第 19 节"),
    ("@ 就是赋值", "第 23 节"),
]
for item, where in table:
    print(f"  {item:34} ← {where}")
