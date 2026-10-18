#!/usr/bin/env python3
# ============================================================================
# r_bytecode.py —— 第 05 章第 21 节：字节码视角看「局部变量为什么快」（dis + co_varnames）
#
# CPython 把函数的局部名字编译成**数组下标**（LOAD_FAST/STORE_FAST），
# 全局名字要走字典查找（LOAD_GLOBAL，3.11+ 有「自适应」缓存，但仍是查名字），
# 闭包名字要打开 cell（LOAD_DEREF/STORE_DEREF）。
# 这一节用 dis 把这三条指令数出来 —— 第 22 节再去量它们值多少纳秒。
# ============================================================================

import dis
from collections import Counter

GLOBAL_TOTAL = 0


def use_local(n):
    total = 0
    for i in range(n):
        total += i
    return total


def global_accum(n):
    global GLOBAL_TOTAL            # 声明了 global → 这个函数里没有同名局部
    GLOBAL_TOTAL += n              # 读 + 写都走全局：LOAD_GLOBAL + STORE_GLOBAL
    return GLOBAL_TOTAL


def make_closure():
    captured = 10

    def use_deref(n):
        total = 0
        for i in range(n):
            total += captured        # 每次都要打开那个 cell（第 13 节的盒子）
        return total
    return use_deref


def global_accum(n):
    global GLOBAL_TOTAL
    GLOBAL_TOTAL += n          # 读全局 + 写全局：LOAD_GLOBAL + STORE_GLOBAL
    return GLOBAL_TOTAL


print("=== 1. 三种写法对应三种指令（只看关键那几行）===")
for label, fn, interesting in (
    ("局部累加", use_local, {"total", "i"}),
    ("全局累加", global_accum, {"GLOBAL_TOTAL", "n"}),
    ("闭包累加", make_closure(), {"captured", "total"}),
):
    print(f"  {label}（{fn.__name__}）：")
    for ins in dis.get_instructions(fn):
        if ins.opname.startswith(("LOAD_", "STORE_")) and ins.argval in interesting:
            print(f"    {ins.opname:14} {ins.argval}")

print()
print("=== 2. 指令计数（整份字节码，不看循环次数）===")


def tally(fn):
    c = Counter(ins.opname for ins in dis.get_instructions(fn))
    fast = sum(v for k, v in c.items() if k.endswith("_FAST"))
    glob = sum(v for k, v in c.items() if k.endswith("_GLOBAL"))
    deref = sum(v for k, v in c.items() if k.endswith("_DEREF"))
    name = sum(v for k, v in c.items() if k.endswith("_NAME"))
    return c, fast, glob, deref, name


for label, fn in (("use_local", use_local), ("global_accum", global_accum)):
    c, fast, glob, deref, name = tally(fn)
    print(f"  {label:11} *_FAST={fast:3}  *_GLOBAL={glob:3}  *_NAME={name:3}  *_DEREF={deref:3}  总指令={sum(c.values())}")

print()
print("=== 3. 闭包那一层：LOAD_DEREF ===")
inner = make_closure()
inner_code = inner.__code__
c, fast, glob, deref, name = tally(inner)
print(f"  use_deref  *_FAST={fast}  *_DEREF={deref}  总指令={sum(c.values())}")
print("  co_varnames（自己的局部表）=", inner_code.co_varnames)
print("  co_freevars（从外面带的名字）=", inner_code.co_freevars, "← 这些走 *_DEREF")
print("  同一份算法，只是把 total 换成全局/闭包变量，指令就换一族：")
print("    局部 → LOAD_FAST（数组下标）｜全局 → LOAD_GLOBAL（查字典，带缓存）｜闭包 → LOAD_DEREF（打开盒子）")

print()
print("=== 4. 局部变量表是编译期就点好数的数组 ===")


def three(a, b):
    c = a + b
    d = c * 2
    e = d - 1
    return e


print("  co_argcount =", three.__code__.co_argcount, "｜ co_varnames =", three.__code__.co_varnames)
print("  前 co_argcount 个是参数，后面全是函数体里被赋值出来的名字（c/d/e），一个都不多一个都不少")
print("  co_stacksize（操作数栈峰值，也是编译期算的）=", three.__code__.co_stacksize)
print("  整份字节码（只有 8 条，逐行读一遍就懂「局部变量=数组下标」）：")
for line in dis.get_instructions(three):
    print(f"    {line.opname:12} {line.argrepr}")
print("  对照 global_accum（把同一件事写在全局上）：")
for line in dis.get_instructions(global_accum):
    print(f"    {line.opname:12} {line.argrepr}")

print()
print("=== 5. 一个真实推论：函数里少用全局、多用手递参数 ===")
LOOKUPS = list(range(200000))


def global_list_sum():
    total = 0
    for x in LOOKUPS:                 # LOOKUPS 是全局 → 但循环头只 LOAD_GLOBAL 一次
        total += x
    return total


def param_list_sum(items):
    total = 0
    for x in items:                   # items 是局部 → LOAD_FAST
        total += x
    return total


print("  global_list_sum() =", global_list_sum())
print("  param_list_sum(LOOKUPS) =", param_list_sum(LOOKUPS))
print("  两个函数体指令差异（循环头那一处）：")
for fn in (global_list_sum, param_list_sum):
    ops = [i.opname for i in dis.get_instructions(fn)]
    print(f"    {fn.__name__:18} LOAD_GLOBAL={ops.count('LOAD_GLOBAL')}  LOAD_FAST={ops.count('LOAD_FAST')}")
print("  循环头只查一次全局名，所以差距不大；**在循环体里反复读全局名**才真的贵（第 22 节量的是那种）。")
