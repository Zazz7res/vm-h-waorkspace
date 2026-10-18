#!/usr/bin/env python3
# ============================================================================
# 取证脚本 b_tree.py —— 第 3 节：异常是一棵类树；except Exception 有抓不到的东西
#
# 目的：把「为什么不能写裸 except」「为什么 except Exception 抓不到 Ctrl-C」
#      变成可以当场验证的 MRO 与 True/False。
# ============================================================================

print("=== 1. 一条线的家谱：从 object 到 ValueError ===")
for cls in (ValueError, Exception, BaseException, object):
    print(f"  {cls.__name__:14} 的 MRO：", " → ".join(c.__name__ for c in cls.__mro__))

print("\n=== 2. 常见的内置异常直接挂在哪个分支上（逐个打印，顺序固定）===")
names = [
    "TypeError", "StopIteration", "StopAsyncIteration", "AttributeError", "SyntaxError",
    "NameError", "RuntimeError", "ImportError", "OSError", "EOFError", "ValueError",
    "AssertionError", "ArithmeticError", "LookupError", "IndexError", "KeyError",
    "MemoryError", "SystemError",
]
import builtins

for n in names:
    cls = getattr(builtins, n)
    print(f"  {n:20} 的直接父亲 = {cls.__bases__[0].__name__}")

print("\n=== 3. 两个「父亲是 Exception」的窄门：捕获时只能靠子类名 ===")
print("  UnicodeError 的父亲：", UnicodeError.__bases__[0].__name__, "（所以是 OSError 的儿子）")
print("  UnboundLocalError 的父亲：", UnboundLocalError.__bases__[0].__name__)
print("  ModuleNotFoundError 的父亲：", ModuleNotFoundError.__bases__[0].__name__)
print("  FileExistsError / FileNotFoundError 的父亲：",
      FileExistsError.__bases__[0].__name__, "/", FileNotFoundError.__bases__[0].__name__)
print("  → 第 05 章那个 UnboundLocalError 是 NameError 的儿子：except NameError 能接住它，实测过。")
print("  接一下试试：")
counter = 0


def bad_add():
    counter += 1


try:
    bad_add()
except NameError as exc:
    print("    except NameError 接住了：", type(exc).__name__, "|", exc)

print("\n=== 4. ★树根上挂着三个「不该被 except Exception 抓住」的退出类异常 ===")
for cls in (KeyboardInterrupt, SystemExit, GeneratorExit):
    print(f"  {cls.__name__:18} 的父亲 = {cls.__bases__[0].__name__}"
          f" ｜ 是 Exception 的儿子吗 → {issubclass(cls, Exception)}")

print("\n=== 5. 现场验证：except Exception 让 KeyboardInterrupt 跑掉了 ===")


def worker():
    try:
        raise KeyboardInterrupt("模拟按下 Ctrl-C")
    except Exception:
        print("    except Exception 接住了（**这条不该打**）")
    finally:
        print("    finally 还是跑了 —— 清理逻辑不受影响")


try:
    worker()
except KeyboardInterrupt as exc:
    print("  外层接住了：", "KeyboardInterrupt |", exc)
    print("  → 结论：except Exception **故意**放过「用户/解释器要求退出」这一族。")

print("\n=== 6. 裸 except 会把 SystemExit 一起咽掉（这就是它被禁的理由） ===")


def bare_handler():
    try:
        raise SystemExit("想退出")
    except BaseException:  # 裸 except（连冒号后面什么都不写）的等价写法
        return "被接住了 —— 退出请求消失"


print("  用 except BaseException：", bare_handler())
print("  用 except Exception：")


def normal_handler():
    try:
        raise SystemExit("想退出")
    except Exception:
        return "接不住，会被抛出去"


try:
    r = normal_handler()
    print("    竟然返回了：", r)
except SystemExit as exc:
    print("    SystemExit 原样抛出 →", exc)

print("\n=== 7. except* 是另一棵树（ExceptionGroup，本机 3.14 支持）：本章只证明它存在 ===")
print("  本机有这个内置类吗 →", hasattr(builtins, "ExceptionGroup"))
try:
    raise ExceptionGroup("一批错", [ValueError("甲"), KeyError("乙")])
except* ValueError as eg:
    print("  except* ValueError 收走了其中一个分支：",
          type(eg).__name__, "| 子异常个数 =", len(eg.exceptions), "|", eg.exceptions[0])
except* KeyError as eg:
    print("  except* KeyError 收走了另一个分支：",
          type(eg).__name__, "| 子异常个数 =", len(eg.exceptions), "|", eg.exceptions[0])
print("  → 两个分支都跑了，这就是 except* 和 except 的根本区别（它按**类型**切开一组异常，各自分派）。")

print("\n=== 8. 收口 ===")
print("  ① except 匹配的是 **isinstance**，所以「抓父亲就抓到全部儿子」；")
print("  ② 只有 BaseException 那一层会连退出信号一起抓走，写 except: / except BaseException: 都要给理由；")
print("  ③ 报错原文里的第一行永远是 `模块.类名: 消息`，那个类名就是这棵树上的一个叶子 —— 认树就能认错。")
