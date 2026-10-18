#!/usr/bin/env python3
"""场景 1：`python3 -c` 一行式 vs 脚本文件 —— sys.argv 到底从哪来。

这个文件本身就是一个「探针」：它不干活，只把解释器启动时塞给它的那些信息全部打印出来。
你可以用四种方式启动同一份代码（README 里有真实对照输出）：

    python3 -c "<把这里的内容抄成一行>"     # 一行式
    python3 a_argv_and_main.py 甲 乙        # 脚本文件，带两个参数
    python3 a_argv_and_main.py              # 脚本文件，不带参数
    python3 - < a_argv_and_main.py          # 从标准输入喂脚本（注意：-m 需要模块名，这里用 - 代替）

重点看三样东西：
  1. sys.argv 的第一个元素 —— 它不是「程序名」那么含糊，它有确定取值规则；
  2. __name__ —— 直接跑脚本时是 '__main__'，被别人 import 时是模块名；
  3. __file__ —— 只有「有真实文件」的时候才有，`-c` 里没有。
"""

import os
import sys


def line(label, value):
    """统一格式打印，方便和 README 里的输出逐行对齐。"""
    print(f"{label:<22} {value}")


print("=" * 68)
print("一、argv：命令行参数是怎么传进来的")
print("=" * 68)
# sys.argv 是一个「字符串列表」，不是「一整行字符串」。
# shell 已经替你把空格切好了（引号内的空格不会被切开），Python 只收到切完的结果。
line("len(sys.argv)", len(sys.argv))
for i, item in enumerate(sys.argv):
    # 每一项都用 repr 打印：这样空字符串、带空格的东西一眼能看出来是什么。
    line(f"sys.argv[{i}]", repr(item))

print()
print("=" * 68)
print("二、这段代码是「谁」：__name__ / __file__ / 位置")
print("=" * 68)
# __name__ 直接决定 `if __name__ == "__main__":` 会不会执行。
# 被 import 时 __name__ 变成模块名（'a_argv_and_main'），if 就不成立 —— 这就是那行的全部机关。
line("__name__", __name__)
line("__file__", __file__)
line("os.path.abspath(__file__)", os.path.abspath(__file__))
line("os.getcwd()（当前目录）", os.getcwd())
line("sys.executable", sys.executable)
line("sys.flags.interactive", sys.flags.interactive)
line("sys.flags.optimize", sys.flags.optimize)
line("sys.dont_write_bytecode", sys.dont_write_bytecode)

print()
print("=" * 68)
print("三、stdin / stdout / stderr 分别是什么东西")
print("=" * 68)
# 这三行的 isatty 结果会随「你直接把输出打到终端」还是「重定向到文件/管道」变化，
# 这正是场景 5（缓冲）的入口。在这里先把「是不是终端」这件事看清楚。
for name in ("stdin", "stdout", "stderr"):
    stream = getattr(sys, name)
    print(f"sys.{name:<7} isatty={stream.isatty()!s:<6} "
          f"line_buffering={getattr(stream, 'line_buffering', None)!s:<6} "
          f"encoding={getattr(stream, 'encoding', None)}")

print()
print("=" * 68)
print("四、退出码：没写 sys.exit 时是 0")
print("=" * 68)
print("（本脚本正常跑到这里就结束了；shell 里 `echo $?` 应当是 0）")
