#!/usr/bin/env python3
# ============================================================================
# 取证脚本 k_contextlib.py —— 第 12 节：@contextmanager：一个 yield 把函数劈成 enter/exit
#
# 目标：演清楚 yield 前后各归谁管、yield 的值给谁、异常怎么被扔回生成器里。
# ============================================================================
from contextlib import contextmanager

LOG = []


@contextmanager
def gate(name):
    LOG.append(f"enter({name})")      # ← yield **之前**：这就是 __enter__ 的函数体
    yield f"<{name}>"                 # ← yield 的值 = with ... as 左边绑的东西
    LOG.append(f"exit({name})")       # ← yield **之后**：只有正常退出才会跑到这


print("=== 1. 最小形状：函数体被 yield 劈成三截 ===")
with gate("G") as bound:
    LOG.append(f"  块体里 as 拿到 = {bound}")
for line in LOG:
    print("  ", line)
print("  → 对比第 10 节手写的类：__enter__ 的 return 变成了 yield 的值，")
print("    __exit__ 的函数体变成了 yield 之后的那几行。**少写一个类、少写两个 def。**")

print("\n=== 2. 裸 yield（不加 try）：块体崩了就没人收尾 ===")
LOG.clear()


@contextmanager
def sloppy():
    LOG.append("enter")
    yield
    LOG.append("exit")        # ← 只有**正常**退出才跑得回来


try:
    with sloppy():
        LOG.append("  块体")
        raise ValueError("崩了")
except ValueError:
    LOG.append("  外面接住")
print("  日志 =", LOG, "→ 没有 exit：yield 之后那行被整个跳过了。")
LOG.clear()


@contextmanager
def careful():
    LOG.append("enter")
    try:
        yield
    finally:
        LOG.append("exit")    # ← try/finally 包起来才等于「必然执行」


try:
    with careful():
        LOG.append("  块体")
        raise ValueError("崩了")
except ValueError:
    LOG.append("  外面接住")
print("  换成 try/finally 包 yield：日志 =", LOG)
print("  判据：**@contextmanager 里 yield 之后写的任何清理，都必须包在 try/finally 里。**")
print("    标准库自己的实现就是这么写的，所以口诀是「yield 前后各一段，中间用 try 夹住」。")

print("\n=== 3. 异常是被「扔回 yield 那一行」的：except 写在生成器里就能接 ===")
LOG.clear()


@contextmanager
def only_value_errors():
    LOG.append("enter")
    try:
        yield
    except ValueError as exc:
        LOG.append(f"  yield 这一行收到了 ValueError: {exc} → 我选择咽掉")
    finally:
        LOG.append("exit")


try:
    with only_value_errors():
        raise ValueError("被生成器接住了")
    LOG.append("  块体外：异常没出来（被咽了）")
except ValueError:
    LOG.append("  块体外：异常出来了")
for line in LOG:
    print("  ", line)
print("  → 这就是 contextlib.suppress 的实现原理（第 13 节现场拆）。")
print("    生成器把异常接住并正常结束 → contextlib 认为「这件事你已经处理完了」→ 异常就地消失；")
print("    第 13 节会把 suppress 的源码打出来，它干的就是这一件事。")
LOG.clear()
try:
    with only_value_errors():
        raise TypeError("生成器只 except 了 ValueError")
except TypeError as exc:
    LOG.append(f"  外面接住了：{exc}")
print("  ", LOG, "→ 接不住的类型照样往上跑，日志顺序仍是 enter → exit（finally 兜住）。")

print("\n=== 4. 同一个 @contextmanager 的结果只能用一次（生成器已经耗尽）===")
cm = gate("复用测试")
print("  type(cm) =", type(cm).__name__)
with cm:
    pass
print("  第一次用完，把**同一个对象**再放进 with：")
try:
    with cm:
        pass
except BaseException as exc:
    print(f"      → {type(exc).__name__}: {exc}")
print("  → 注意原文是 **AttributeError**，不是 RuntimeError：第一次 __exit__ 时 contextlib")
print("    把它内部那几个属性 del 掉了（源码里那句 `del self.args, self.kwds, self.func`），")
print("    所以第二次进来撞的是「属性不存在」。**报错类型和你想的往往不一样，别看猜。**")
print("  正确做法：每次 with 都**重新调用**那个工厂函数（gate('x')），别存对象复用。")
print("  对照：第 10 节手写的类没这个限制（__enter__ 是普通方法，可以反复调）——")
print("    差别就在 @contextmanager 的底子是生成器，生成器只能跑完一遍。")

print("\n=== 5. 「忘了 yield」和「yield 两次」：两条原文完全不同的错 ===")


@contextmanager
def no_yield():
    LOG.append("enter 但忘了 yield")


@contextmanager
def two_yields():
    yield 1
    yield 2


for label, factory in [("没 yield 的 @contextmanager", no_yield), ("yield 了两次", two_yields)]:
    try:
        with factory():
            pass
    except BaseException as exc:
        print(f"  {label} → {type(exc).__name__}: {exc}")
print("  → 第一条的因果链：函数体里**一个 yield 都没有**，它就根本不是生成器函数，")
print("    no_yield() 返回 None，contextlib 拿去 next(None) → TypeError。报错不提「yield」半个字，")
print("    但病因就是它。判据：@contextmanager 装饰的函数**必须恰好 yield 一次**：")
print("    前置 = __enter__，yield 的值 = as 的东西，后置 = __exit__。")

print("\n=== 6. 收口 ===")
print("  @contextmanager 做的事：把「生成器函数」包成一个「返回 __enter__/__exit__ 对象」的工厂，")
print("    它本身是第 05 章第 23 节那个装饰器 —— f = contextmanager(f)，名字和 doc 由 wraps 保住。")
print("  三条纪律：① yield 之后必须 try/finally；② 一次 with 一个新对象；③ 想咽异常就在 except 里接，")
print("    接了就等于咽（所以**默认别接**，让它往上跑，除非你就是 suppress）。")
