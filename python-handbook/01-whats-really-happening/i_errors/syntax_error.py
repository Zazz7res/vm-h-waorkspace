#!/usr/bin/env python3
"""报错原文解剖（五）：SyntaxError —— 连字节码都没生成出来。

跑法：python3 i_errors/syntax_error.py

这一份的重点是**看行号和那个 `^` 的位置**：
  * 括号没闭合时，Python 会把「整条语句」当成一个整体去读，
    于是报错行号常常指向**语句开始的那一行**，而不是你漏括号的那一行；
  * 有时更狠，直接报到文件末尾（`SyntaxError: '[' was never closed` 这类）。
  所以「报错行号不一定就是错的那一行」—— 往上看几行是常规操作。
"""


def describe(items):
    result = []
    for it in items
        result.append(str(it))      # ← 上一行少了冒号，报错却可能指到这一行
    return result


print(describe([1, 2, 3]))
