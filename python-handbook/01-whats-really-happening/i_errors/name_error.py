#!/usr/bin/env python3
"""报错原文解剖（一）：NameError —— 手里这个名字从来没存在过。

同一个错误看两遍：
  第一遍用 try 接住它，看清「异常对象」身上有哪些字段（你在屏幕上看到的那份 traceback
  就是按这些字段拼出来的）；
  第二遍让它真的漏出去，屏幕上打出**完整原始 traceback**，进程以退出码 1 结束。

跑法：
    python3 i_errors/name_error.py
"""

import traceback

greeting = "你好"

print("=" * 66)
print("第一遍：接住它，看异常对象身上有什么")
print("=" * 66)
try:
    # 手滑把 greeting 打成 greting（少了一个 t）。这是 NameError 最常见的来源。
    print(greting)
except NameError as exc:
    print(f"type(exc)            {type(exc).__name__}")
    print(f"str(exc)             {exc}")
    print(f"exc.name             {exc.name}")     # 3.10+ 起有：具体是哪个名字没找到
    print(f"exc.args             {exc.args}")
    print("用 traceback.format_exc() 拼出来的字符串（前 4 行）：")
    for n, line in enumerate(traceback.format_exc().splitlines()[:4]):
        print(f"  {n + 1}| {line}")

print()
print("=" * 66)
print("第二遍：不接了，让解释器自己处理（脚本会在这里挂掉）")
print("=" * 66)
print("下面这一行执行后，屏幕上看到的就是真实报错原文：")
print(greting)
print("你永远看不到这一行 —— 因为上面那行已经把进程带走了。")
