#!/usr/bin/env python3
# ============================================================================
# t_decorator.py —— 第 05 章第 23 节：装饰器就是把函数交给函数（`@X` ≡ `f = X(f)`）
#
# 前面 22 节攒齐了所有零件：函数是一等对象（第 2 节）、def 会换名字（第 2 节）、
# *args/**kwargs 转发（第 9 节）、闭包共享盒子（第 13 节）、functools.wraps 恢复身份证（第 19 节）。
# 装饰器没有新语法，只有一个赋值缩写 —— 这一节当场把等价式跑给你看。
# ============================================================================

import functools
import inspect


def shout(fn):                       # 收一个函数，还一个函数 —— 就这一句话
    def wrapper(text):
        return fn(text).upper()
    return wrapper


@shout
def hello(text):
    """打招呼。"""
    return f"hello, {text}"


print("=== 1. `@X` 只是 `hello = X(hello)` 的缩写 ===")
print("  hello('world') =", hello("world"))
print("  名字 hello 现在绑的是谁：", hello.__name__, "（wrapper，不是 hello 了）")
print("  文档串也没了：", repr(hello.__doc__))
print("  手写等价版：")


def plain(text):
    return f"hello, {text}"


plain = shout(plain)
print("    plain = shout(plain) →", plain("world"), "｜ 一模一样")

print()
print("=== 2. 被吞掉的不只是名字：一份对照表 ===")


def show(tag, fn):
    print(f"  {tag:10} __name__={fn.__name__:12} __doc__={fn.__doc__!r:14} "
          f"签名={inspect.signature(fn)}  模块={fn.__module__}")


def original(text):
    """原始说明。"""
    return text


@shout
def decorated(text):
    """原始说明。"""
    return text


show("装饰前", original)
show("装饰后", decorated)
print("  → 名字、文档、签名全变成 wrapper 的了；调试时这是灾难，所以有下一节。")

print()
print("=== 3. functools.wraps 把身份证贴回来 ===")


def shout_wrapped(fn):
    @functools.wraps(fn)             # 把 fn 的 __name__/__doc__/__module__/__wrapped__ 等搬给 wrapper
    def wrapper(text):
        return fn(text).upper()
    return wrapper


@shout_wrapped
def fixed(text):
    """原始说明。"""
    return text


show("wraps 后", fixed)
print("  还多送一个反向指针：fixed.__wrapped__ is fixed →", fixed.__wrapped__ is fixed,
      "｜ __wrapped__.__name__ =", fixed.__wrapped__.__name__)
print("  （functools.WRAPPER_ASSIGNMENTS =", functools.WRAPPER_ASSIGNMENTS, "）")

print()
print("=== 4. 带参数的装饰器 = 三层函数 ===")


def repeat(times):
    def deco(fn):
        @functools.wraps(fn)
        def wrapper(*args, **kw):
            return [fn(*args, **kw) for _ in range(times)]
        return wrapper
    return deco


@repeat(3)
def roll(side):
    return f"掷 {side}"


print("  roll('骰子') =", roll("骰子"))
print("  展开式：roll = repeat(3)(roll) —— 先调用 repeat(3) 拿到 deco，再用 deco 包函数")
print("  分两步验证：")
step1 = repeat(3)
print("    repeat(3) 返回的是哪个函数：", step1.__name__)


def sample(side):
    return f"掷 {side}"


step2 = step1(sample)
print("    再拿它包一个函数：step2('一次') =", step2("一次"))

print()
print("=== 5. 忘括号的两种现场 ===")


def needs_args(times):
    def deco(fn):
        @functools.wraps(fn)
        def wrapper(*a, **k):
            return fn(*a, **k)
        return wrapper
    return deco


print("  正确：@needs_args(2)")


@needs_args(2)
def ok():
    return "跑完了"


print("    ok() =", ok())
print("  错误：把 @needs_args 直接挂上去（times 收到的是函数本身）：")


@needs_args
def wrong():
    return "跑完了"


print("    wrong() →", end=" ")
try:
    wrong()
except TypeError as e:
    print("TypeError |", e)

print()
print("=== 6. 多个装饰器：离 def 近的**先**包（里层），远的在外层 ===")


def open_tag(fn):
    @functools.wraps(fn)
    def w(*a, **k):
        return "<" + fn(*a, **k) + ">"
    return w


def bold_tag(fn):
    @functools.wraps(fn)
    def w(*a, **k):
        return "[" + fn(*a, **k) + "]"
    return w


@open_tag
@bold_tag
def word():
    return "hi"


@bold_tag
@open_tag
def word2():
    return "hi"


print("  @open_tag 在上、@bold_tag 在下 →", word())
print("  反过来写 →", word2())
print("  执行顺序：装饰（自下而上）与调用（自上而下）是**两件事**，别混。")

print()
print("=== 7. 装饰器在 def 那一刻就跑了一次（呼应第 4 节默认值）===")
registered = []


def register(fn):
    registered.append(fn.__name__)
    return fn


@register
def tool_a():
    pass


@register
def tool_b():
    pass


print("  还没调用任何函数，注册表已经满了：", registered)
print("  这就是第 19 节 lru_cache、第 06 章 @property、@dataclass 能在「定义时」动手的原因。")

print()
print("=== 8. 用装饰器踩一遍本章所有坑的合体版 ===")
LOG = []


def logged(fn):
    @functools.wraps(fn)
    def wrapper(*args, **kw):
        LOG.append(f"{fn.__name__}{args}{sorted(kw)}")
        return fn(*args, **kw)
    return wrapper


@logged
def append_log(item, box=[]):         # ← 故意留的可变默认参数（第 5 节那个坑）
    box.append(item)
    return box


print("  调用两次：", append_log(1), append_log(2))
print("  日志：", LOG)
print("  坑还在，而且现在连 __defaults__ 都得穿过 wrapper 才看得到：", append_log.__wrapped__.__defaults__)
print("  （wraps 帮你留了 __wrapped__ 这条后门。）")
