"""broken_import.py —— 一个「能被找到、但执行时会炸」的模块，专门用来制造两层 traceback。

关键点：模块被 import 的**那一刻**，它里面的顶层代码就会执行（这是 09 章的重点，
也是为什么「模块顶层不要干重活」）。所以这里的除法会直接打断那次 import。

跑法：不要直接跑它，用 module_not_found.py 3 去 import 它。
"""

import sys

print("broken_import 的顶层代码开始执行（你能看到这行，说明它被读进来了）")

VERSION = sys.version_info[:2]
BAD = 1 / 0                    # ← 真凶在这一行
print("这行永远执行不到")
