#!/usr/bin/env python3
"""场景 2（上）：`.py` 是怎么变成字节码的 —— 现场编译一段源码，把它拆开看。

跑法：
    python3 b_bytecode.py

你会看到四件事：
  1. `compile()` 能把一段「字符串」变成 code object（代码对象），它就是字节码本体；
  2. code object 身上的字段：co_code（指令字节）、co_consts（常量表）、co_names（名字表）、
     co_varnames（局部变量名）—— 字节码里的操作数全是「表的下标」，不是名字本身；
  3. `dis` 把那串字节反汇编成人能看的助记符；
  4. 编译期就把算式算完了（常量折叠）：`x = 1 + 2` 的字节码里**没有加法指令**。
"""

import dis
import sys

print(f"这台机器的解释器：Python {sys.version.split()[0]} "
      f"(cache_tag={sys.implementation.cache_tag})")
print()

# ---------------------------------------------------------------- 一、最小的一段源码
SRC = """total = 0
for i in range(3):
    total = total + i
print(total)
"""

print("=" * 68)
print("一、源码长这样")
print("=" * 68)
print(SRC, end="")

# ---------------------------------------------------------------- 二、compile → code object
# 第三个参数 mode 决定这段源码被当成「一堆语句」还是「一个表达式」：
#   exec   —— 脚本文件用的模式（一堆语句）
#   eval   —— 只能一个表达式，算出来的值留在栈顶等着被 return
#   single —— 交互式解释器逐行执行用的模式，区别见第四节
code = compile(SRC, "<字符串源码>", "exec")

print("=" * 68)
print("二、compile(SRC, 文件名, 模式) 吐出来的 code object")
print("=" * 68)
print(f"type(code)               {type(code)}")
print(f"len(code.co_code)        {len(code.co_code)}   ← 字节码总共有 {len(code.co_code)} 个字节")
print(f"code.co_consts           {code.co_consts}")
print(f"code.co_names            {code.co_names}")
print(f"code.co_varnames         {code.co_varnames}")
print()
print("读懂这三张表：")
print("  co_consts 里躺着所有「写死的值」（含嵌套代码对象），LOAD_CONST 后面跟的是下标；")
print("  co_names  里躺着所有「全局名字」，LOAD_NAME/STORE_NAME 后面跟的也是下标；")
print("  co_varnames 是局部变量名表（这里是空的，因为这段代码在最外层，变量全是「全局」）。")
print("  所以「名字」在字节码里其实是整数索引 —— 这就是为什么变量名不影响运行速度，")
print("  但影响查表次数（全局要查 dict，局部直接按索引取，见 04/10 章的实测）。")

# ---------------------------------------------------------------- 三、反汇编
print()
print("=" * 68)
print("三、dis.dis(code)：把字节码反汇编成助记符")
print("=" * 68)
dis.dis(code)

# ---------------------------------------------------------------- 四、常量折叠
print()
print("=" * 68)
print("四、编译期就把算式算完了（常量折叠）")
print("=" * 68)
folded = compile("x = 1 + 2", "<折叠演示>", "exec")
print(f"源码 \"x = 1 + 2\" → co_consts = {folded.co_consts}")
print("它的字节码是：")
dis.dis(folded)
print("→ 只有 LOAD_SMALL_INT 3：加法在编译期做完了，运行时一次加法都不发生。")
print("  3.14 还把小整数直接编进指令里（LOAD_SMALL_INT），连常量表都不用查。")
powed = compile("z = 2 ** 10", "<折叠演示>", "exec")
print(f"源码 \"z = 2 ** 10\" → co_consts = {powed.co_consts}（1024 已经算好了）")
called = compile("y = len([1, 2, 3])", "<不折叠演示>", "exec")
print(f"源码 \"y = len([1, 2, 3])\" → co_names = {called.co_names}")
print("→ 函数调用**不会**在编译期做：解释器不敢假设 `len` 还是那个 len（你可能自己定义了一个）。")

# ---------------------------------------------------------------- 五、exec / eval / single 的真实差别
print()
print("=" * 68)
print("五、exec / eval / single 三种模式跑「同一个表达式」，输出完全不同")
print("=" * 68)
print("exec   模式执行 `1+1` → 屏幕上：", end="")
exec(compile("1 + 1", "<测试>", "exec"))
print("（上面这行后面什么都没有 → 结果被丢掉了。脚本文件就是这个模式！）")
print("single 模式执行 `1+1` → 屏幕上：", end="")
exec(compile("1 + 1", "<测试>", "single"))
print("（自动把结果打印出来了 → 交互式解释器每敲一行用的就是这个模式）")
print("eval   模式执行 `1+1` → 拿到返回值：", repr(eval(compile("1 + 1", "<测试>", "eval"))))
print()
print("结论：「不写 print 也有回显」不是魔法，是启动方式换了 compile 的 mode：")
print("  脚本 / `python3 -c`  → exec   → 表达式结果直接扔进垃圾桶")
print("  交互式（REPL）       → single → 结果非 None 时自动 repr 并打印")
