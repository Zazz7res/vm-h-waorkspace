#!/usr/bin/env python3
"""报错原文解剖（六）：一个「四层」traceback，教你按顺序读。

跑法：python3 i_errors/deep_traceback.py

先接住它、把每一帧拆开编号打印（这是「看懂结构」的部分），
再原样放出去，让屏幕上出现**真实的完整 traceback**（这是「对上号」的部分）。

一句话读法：`Traceback (most recent call last)` 之后，
**从上到下 = 从外到内**，最后一行才是真正炸掉的地方，倒数第二行是当时的源码。
"""

import traceback


def read_config(path):
    """最里层：假装读配置文件，其实只返回一个空 dict。"""
    data = {}
    return data["timeout"]           # ← 真凶：键不存在 → KeyError


def start():
    return read_config("app.ini")


def main():
    return start()


print("=" * 66)
print("第一遍：接住它，把栈帧一层层编号列出来")
print("=" * 66)
try:
    main()
except KeyError as exc:
    tb = exc.__traceback__
    frames = traceback.extract_tb(tb)
    print(f"异常类型：{type(exc).__name__}    参数：{exc}")
    print(f"栈帧一共 {len(frames)} 层（顺序 = 从外到内，最后一层是出事地点）：")
    for i, fr in enumerate(frames):
        # fr.name 对模块顶层代码是 "<module>" —— 看到它就明白「这是脚本正文，不是函数里」
        print(f"  [{i}] {fr.name:<12} 行 {fr.lineno:<3} {fr.line}")
    print()
    print("exc.__traceback__ 本身就是那个「栈的快照」，07 章会拿它做异常链。")

print()
print("=" * 66)
print("第二遍：原样放出去（下面这份就是你在终端里会看到的东西）")
print("=" * 66)
main()
