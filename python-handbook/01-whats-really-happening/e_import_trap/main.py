#!/usr/bin/env python3
"""e_import_trap/main.py —— 「同一段代码，换个目录跑就崩」的现场。

这个目录里有三个文件：
    main.py      ← 你运行的脚本
    helper.py    ← 它要 import 的邻居模块
    random.py    ← 一颗雷：抢了标准库 random 的名字

四种跑法，结果两两不同，README 里全部贴了真实输出：

    (1) cd <这个目录> && python3 main.py
    (2) cd /tmp      && python3 <这个目录>/main.py      ← 换个 cwd 跑同一个脚本
    (3) cd /tmp      && python3 -c "import helper"      ← 脚本没了，只有 -c
    (4) cd <这个目录> && python3 -c "import random; print(random.__file__)"

结论提前给你：
    * 跑「脚本文件」时，解释器把**脚本所在目录**放进 sys.path[0]，所以 (1)(2) 都能找到 helper；
    * 跑 `-c` / `-m` / 交互式时，放进 sys.path[0] 的是**当前目录**，
      所以 (3) 找不到 helper —— 哪怕你就站在项目的上一层目录里；
    * 而 (4) 更狠：当前目录里有个 random.py，`import random` 就直接拿到它，标准库那份被抢了。
"""

import os
import sys

print("-" * 68)
print(f"cwd（当前目录）    = {os.getcwd()}")
print(f"脚本所在目录        = {os.path.dirname(os.path.abspath(__file__))}")
print(f"sys.path[0]         = {sys.path[0]!r}")
print("-" * 68)

import helper          # ← 能不能成功取决于 sys.path[0] 是不是「脚本所在目录」
print("import helper 成功：", helper.hello())
print("  helper.py 实际来自：", helper.where_am_i())

import random          # ← 这一行在 (1)(2) 里都会拿到 e_import_trap/random.py
print("import random 完成：")
print("  random 实际来自：", random.whoami())
print("  标准库目录是：", os.path.dirname(os.__file__),
      "← 真正的 random.py 本该在这里，但你拿到的是上面那个")
try:
    random.randint(1, 6)
except AttributeError as exc:
    print("  AttributeError:", exc)
    print("  ↑ 你没写错任何代码，只是名字被当前目录里那个 random.py 抢走了。")
