"""path0.py —— 只打印两个值，用来对比四种启动方式的 sys.path[0]。

为什么单独造这个文件：sys.path[0] 的含义随启动方式变化，而这一个值决定了
`import` 去哪儿找你自己的代码。它是最容易「换个目录就跑不起来」的根源。

跑法（demo.sh 第 3.1 节把这四种各跑一遍）：
    python3 -c "import path0"          ← -c：第 0 项是空字符串
    python3 path0.py                   ← 脚本：第 0 项是脚本所在目录（绝对路径）
    python3 -m path0                   ← -m：第 0 项是当前目录（绝对路径）
    python3 < path0.py                 ← stdin：第 0 项是空字符串

空字符串和绝对路径的区别很实际：空字符串是「每次 import 那一刻的当前目录」，
你在代码里 os.chdir() 一下，它的含义就跟着变了。
"""

import os
import sys

print(f"sys.path[0] = {sys.path[0]!r}")
print(f"sys.argv[0] = {sys.argv[0]!r}")
print(f"当前目录     = {os.getcwd()}")
print(f"有 __file__ 吗 = {globals().get('__file__') is not None}")
