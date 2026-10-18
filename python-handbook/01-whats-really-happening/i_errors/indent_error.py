#!/usr/bin/env python3
"""报错原文解剖（三）：IndentationError —— 缩进是语法，不是美观问题。

跑法：python3 i_errors/indent_error.py

注意看输出里**没有** `Traceback (most recent call last):` 这一行。
原因：这类错误发生在「编译阶段」，字节码还没生成，一行代码都还没开始跑，
所以不存在「调用栈」可打印。记住这条，就能一眼区分：

    有 Traceback 开头   → 语法没问题，是运行期出错（NameError / ValueError / ...）
    没有 Traceback      → 语法/缩进错了，程序根本没启动
"""


def greet(name):
print(f"你好, {name}")        # ← 这一行必须缩进，现在没缩进


greet("学习者")
