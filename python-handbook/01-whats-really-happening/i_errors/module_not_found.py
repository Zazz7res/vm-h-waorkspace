#!/usr/bin/env python3
"""报错原文解剖（二）：ModuleNotFoundError —— 「找不到」和「找到了但它自己崩了」是两回事。

三种不同的失败，README 里逐条对照：

    (1) 名字根本不存在        → import requests_here_nope
    (2) 文件在磁盘上，只是没在搜索路径里 → import helper（它住在 e_import_trap/ 目录）
    (3) 模块找到了，但它自己内部又炸了 → import broken_import
        第 (3) 种最容易被误读成「模块找不到」：traceback 里会有**两层**，
        下面那个 `File ".../broken_import.py"` 才是真凶所在。

跑法（三种都要跑一遍）：
    python3 i_errors/module_not_found.py 1
    python3 i_errors/module_not_found.py 2
    python3 i_errors/module_not_found.py 3
"""

import sys

which = sys.argv[1] if len(sys.argv) > 1 else "1"
print(f"第 {which} 种失败，现在开始 import：")

if which == "3":
    # 故意不接异常：让 broken_import 内部的错误一路冒出来，
    # 这样屏幕上就是「原生的、两层的」完整 traceback。
    import broken_import                       # noqa: F401
    print("如果你看到这一行，说明 broken_import 没炸 —— 那就有问题了")
elif which == "2":
    try:
        import helper                          # noqa: F401
    except ModuleNotFoundError as exc:
        print(f"{type(exc).__name__}: {exc}")
        print(f"  exc.name = {exc.name!r}   exc.path = {exc.path!r}")
        print(f"  本次搜索翻过 {len(sys.path)} 个目录，第 0 个是本脚本所在目录，"
              f"里面没有 helper.py")
        print("  → 文件明明在磁盘上（在上一级的 e_import_trap/ 里），但没人去那儿找它。")
else:
    try:
        import requests_here_nope               # noqa: F401
    except ModuleNotFoundError as exc:
        print(f"{type(exc).__name__}: {exc}")
        print(f"  exc.name = {exc.name!r}   exc.path = {exc.path!r}")
        print("  → 这种才是「真没这个包」。要么装它（本手册不装），要么改代码。")
