"""opt_target.py —— 给 `-O` / `-OO` 当靶子的模块。

跑法：不要单独跑它，用 opt_run.py 去 import 它（三种优化级别各跑一遍）。

这个模块里有两样会被优化开关动到的东西：
  1. 文档字符串（-OO 会把它删成 None）
  2. assert 语句（-O 起就整条不生成字节码）
"""

FACTOR = 2


def double_checked(n):
    """这个文档字符串本身也是观察对象：-OO 之后 __doc__ 会变成 None。"""
    # assert 是「调试期的自检」，不是给用户的报错。它只在该被 __debug__ 保护时存在。
    assert n > 0, "必须是正数"
    return n * FACTOR


def what_debug_says():
    # __debug__ 是一个编译期常量：-O 时它是 False，assert 整条被丢掉。
    return __debug__
