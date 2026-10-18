#!/usr/bin/env python3
"""场景 2（下）：`__pycache__/xxx.pyc` 里到底存了什么 —— 手动解析一个 pyc 文件。

跑法（先让 import 生成 pyc，再把它拆开）：
    python3 c_pyc_inspect.py __pycache__/mod_x.cpython-314.pyc

不依赖任何第三方工具，只用标准库的 struct + marshal + dis + hashlib + _imp。
顺便纠正一个常见误解：`python3 -m marshal` 在这台机器上是不存在的
（marshal 是纯 C 模块，没有 __main__），所以只能自己按格式解析：
pyc 文件 = 16 字节头部 + 一个 marshal 序列化后的 code object。
"""

import _imp
import dis
import hashlib
import importlib.util
import marshal
import os
import struct
import sys
import time

HEADER = struct.Struct("<4sIII")   # 官方格式：magic, flags, 字段3, 字段4（共 16 字节）


def describe_flags(flags):
    """PEP 552：头部第 2 个 4 字节是失效法标记位。

    bit0 = 是否基于源码哈希，bit1 = 是否还要真的读源码去校验这个哈希。
    所以 3 = checked hash（每次 import 都算一遍源码哈希），1 = unchecked hash（只看头部，不读源码）。
    """
    if flags & 0b11 == 0b11:
        return f"{flags} = 基于哈希 · checked：每次 import 都会现算源码哈希来比对"
    if flags & 0b11 == 0b01:
        return f"{flags} = 基于哈希 · unchecked：只认头部里那 8 字节，根本不去读源码"
    if flags == 0:
        return "0 = 传统时间戳失效法：比对源码的 mtime + 字节数"
    return f"{flags} = 未知组合"


def source_hash_of(path):
    """用解释器自己那套算法算 pyc 头部的源码哈希（8 字节，小端存放）。"""
    with open(path, "rb") as fh:
        data = fh.read()
    return _imp.source_hash(_imp.pyc_magic_number_token, data)


def main(path):
    raw = open(path, "rb").read()
    magic, flags, field3, field4 = HEADER.unpack(raw[:16])

    print("=" * 68)
    print(f"文件：{path}")
    print("=" * 68)
    print(f"文件大小                {os.path.getsize(path)} 字节")
    print(f"头部（前 16 字节）       {raw[:16].hex(' ')}")
    print(f"magic                   {magic.hex()}   "
          f"当前解释器期望的 magic：{importlib.util.MAGIC_NUMBER.hex()}")
    print(f"flags                   {describe_flags(flags)}")
    if flags & 0b11:
        print(f"头部第 9~16 字节         {raw[8:16].hex()}   ← 源码哈希（8 字节，小端存放）")
    else:
        stamp = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(field3))
        print(f"头部第 9~12 字节         {field3}  → 源码 mtime {stamp}")
        print(f"头部第 13~16 字节        {field4}  → 源码字节数")
    print(f"正文（第 17 字节起）     {len(raw) - 16} 字节 = marshal 序列化的 code object")

    print()
    print("=" * 68)
    print("marshal.loads(raw[16:]) 拿到的就是编译好的 code object")
    print("=" * 68)
    code = marshal.loads(raw[16:])
    print(f"type                    {type(code)}")
    print(f"co_name                 {code.co_name}")
    print(f"co_filename             {code.co_filename}")
    print(f"co_consts               {code.co_consts}")
    print(f"co_names                {code.co_names}")

    print()
    print("=" * 68)
    print("把这个 code object 反汇编出来（pyc 的正文就是这些东西）")
    print("=" * 68)
    dis.dis(code)

    print()
    print("=" * 68)
    print("源码哈希：自己算一遍，和头部对得上吗")
    print("=" * 68)
    src = code.co_filename
    if os.path.exists(src):
        data = open(src, "rb").read()
        mine = _imp.source_hash(_imp.pyc_magic_number_token, data).hex()
        print(f"头部第 9~16 字节          {raw[8:16].hex()}")
        print(f"现算的源码哈希            {mine}")
        if flags & 0b11:
            if mine == raw[8:16].hex():
                print("→ 完全一致：hash 模式下头部存的就是它（算法是 `_imp.source_hash`，"
                      "拿 magic 当盐，不是 SHA-256）。")
            else:
                print("→ 不一致：源码已经和这个 pyc 对不上了，import 时它会作废。")
        else:
            print("→ 不一致是正常的：本例是时间戳模式，头部那 8 字节是 mtime + 源码字节数，不是哈希。")
        print(f"（对照）SHA-256 前 8 字节  {hashlib.sha256(data).hexdigest()[:16]}"
              "  ← 很多人以为是它，其实 pyc 头部用的不是 SHA-256")
    else:
        print(f"源码文件 {src} 不在（pyc 里记的是当初编译时的路径），跳过哈希对照")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"用法：python3 {sys.argv[0]} <某个 .pyc 的路径>")
        sys.exit(2)
    main(sys.argv[1])
