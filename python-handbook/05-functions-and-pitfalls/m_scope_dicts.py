#!/usr/bin/env python3
# ============================================================================
# m_scope_dicts.py —— 第 05 章第 16 节：作用域其实是**字典**（globals/locals/vars/eval 的边界）
#
# 第 15 节讲的是「编译器的判定」，这一节讲「运行时的账本」：
#   模块级名字住在 module.__dict__（= globals()），函数局部名住在帧对象里（locals() 只是它的一份快照）。
# 于是有两条不对称：往 globals() 里写**真生效**，往 locals() 里写**不生效**。
# 这就是 exec/eval 能注入变量、而调试器改局部变量要靠特殊手段的原因。
# ============================================================================

module_var = "模块级"


def show_snapshot():
    local_a = 1
    local_b = "两个局部"
    snap = locals()
    print("    locals() 的键 =", sorted(snap))
    print("    'module_var' 在 locals 里吗 →", "module_var" in snap, "｜ 在 globals 里吗 →", "module_var" in globals())
    print("    globals() 里能直接取到模块级那份：", globals()["module_var"])
    return snap


print("=== 1. 函数里的 locals() 是一份快照 ===")
snap = show_snapshot()
print("  函数返回后拿到的那份：", sorted(snap), "｜ 它还能读，但改它不会影响任何真实变量")

print()
print("=== 2. 往 locals() 里写：不创建变量 ===")


def write_into_locals():
    locals()["injected"] = "我写进去了"
    try:
        return injected
    except NameError as e:
        return f"NameError | {e}"


print("  ", write_into_locals())
print("  （3.13 起官方明确：函数里对 locals() 的修改**不被保证生效**，别依赖它。）")

print()
print("=== 3. 往 globals() 里写：真生效 ===")


def write_into_globals():
    globals()["made_by_dict"] = "字典写出来的模块级名字"


write_into_globals()
print("  直接按名字读：", made_by_dict)
print("  也能覆盖已有的：先读 =", module_var, end=" ")
globals()["module_var"] = "被字典改过了"
print("｜ 改完再读 =", module_var)
print("  这就是 exec/eval 注入变量的通道（第 09 章 import 机制也走这条路）。")

print()
print("=== 4. 模块级那两层其实是同一个字典 ===")
print("  locals() is globals()（模块帧）→", locals() is globals())
print("  sys.modules[__name__].__dict__ is globals() →", end=" ")
import sys
print(sys.modules[__name__].__dict__ is globals())
print("  vars() 不带参数 = locals()：vars() is locals() →", vars() is locals())
print("  vars(模块) 拿到的是那个 __dict__：", type(vars(sys)).__name__)

print()
print("=== 5. 谁住在模块字典里（本章文件自己的账本，节选）===")
mine = sorted(k for k in globals() if not k.startswith("__"))
print("  本文件里非双下划线的名字共", len(mine), "个，前 12 个：", mine[:12])
print("  函数对象也在同一层：'show_snapshot' in globals() →", "show_snapshot" in globals())
print("  del 掉名字，对象才可能回收（第 2 节那个 first 拽着旧函数就是同一件事）：")
temp = lambda: "临时"
print("    赋值前后：'temp' in globals() →", "temp" in globals())
del temp
print("    del 之后：'temp' in globals() →", "temp" in globals())

print()
print("=== 6. eval/exec 的两个字典参数：这就是「人为造一层作用域」 ===")
print("  eval 收不到外面的局部名（因为局部名不在字典里，在帧里）：")


def eval_cannot_see_local():
    hidden = 42
    try:
        return eval("hidden")
    except NameError as e:
        return f"NameError | {e}"


print("    ", eval_cannot_see_local())
print("  但 eval 能收到全局名：", eval("module_var"))
print("  把局部值显式喂进去就行：", eval("a * b", {"a": 6, "b": 7}))
print("  exec 传第二个参数＝给它一个独立字典，不会污染本模块：")
sandbox = {}
exec("def f(): return '只在沙盒里'", sandbox)
print("    sandbox 的键 =", sorted(sandbox), "｜ 调用：", sandbox["f"]())
print("    'f' 有没有漏进本模块 globals →", "f" in globals())

print()
print("=== 7. 一句话收口 ===")
print("  模块级 = 一个真字典（能读能写能注入）；函数级 = 帧里的槽位（locals() 只是快照，写不进去）。")
print("  第 15 节的 UnboundLocalError 是「编译期把这个名字分给了局部槽位」，")
print("  这一节解释了为什么运行时也没法用字典改回来。")
