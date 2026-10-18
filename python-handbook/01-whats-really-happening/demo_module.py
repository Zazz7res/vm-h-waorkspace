"""这行是文档字符串：它会被编译成常量存进 co_consts[0]，还会被绑到 __doc__ 上。"""

# 下面这些 # 注释则完全不会出现在字节码里 —— 对比一下 c_pyc_inspect.py 打出来的
# co_consts 和 co_names，就能看到「docstring 在，注释不在」。这是很多手册不讲的小事实。
#
# 另外：本模块**故意一个 def 都没有**。只要模块里有函数，编译结果里就会嵌一个子
# code object，它的 repr 是 `<code object xxx at 0x7f...., file "...", line N>`，
# 那串内存地址每次运行都变，会把「输出可以逐字比对」毁掉。
# 想看函数怎么编译，用 b_bytecode.py（它把函数写在字符串里，dis 出来一样清楚）。

FACTOR = 2                # 模块级常量 → 进 co_consts
value = 21 * FACTOR       # 顶层语句 → 运行时真会做一次乘法
total = 0
for step in range(3):     # 循环 → FOR_ITER / JUMP_BACKWARD / END_FOR
    total = total + step
print("demo_module 被执行了", "value =", value, "total =", total)
