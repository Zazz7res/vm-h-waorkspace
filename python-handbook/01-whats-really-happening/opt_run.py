#!/usr/bin/env python3
"""opt_run.py —— 把 `-O` / `-OO` 到底改了什么，打成可对照的四行。

跑法（三种级别各一次，README 里三份输出都有）：
    python3    opt_run.py      # 默认：optimize=0
    python3 -O opt_run.py      # optimize=1：assert 消失
    python3 -OO opt_run.py     # optimize=2：连文档字符串也没了

顺带演示 `-B`：它只影响「要不要把 .pyc 写到磁盘」，不影响任何运行结果。
"""

import sys

import opt_target

print(f"sys.flags.optimize        {sys.flags.optimize}")
print(f"sys.dont_write_bytecode   {sys.dont_write_bytecode}")
print(f"__debug__                 {__debug__}   （模块里读到的是 {opt_target.what_debug_says()}）")
print(f"opt_target.__doc__ 前 12 字：{str(opt_target.__doc__)[:12]!r}")

# assert 的行为是最能体现「字节码里到底有没有这条语句」的。
# 默认级别下：n=-1 触发 assert， AssertionError 被抛出；
# -O 级别下：这条语句在编译期就被整条丢掉，函数安静地返回 -2。
try:
    result = opt_target.double_checked(-1)
except AssertionError as exc:
    print(f"double_checked(-1) 触发 AssertionError: {exc}")
else:
    print(f"double_checked(-1) 没有报错，返回值 = {result}"
          f"   ← assert 已经被编译期丢掉了")
