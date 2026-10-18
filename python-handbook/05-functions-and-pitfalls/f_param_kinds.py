#!/usr/bin/env python3
# ============================================================================
# f_param_kinds.py —— 第 05 章第 9 节：五种参数是同一条流水线上的五个筐
#
# 顺序硬规矩（记这一行就够）：
#     位置必填 → 位置默认 → / → c=默认 → *args → 关键字专用(必填或带默认) → **kwargs
#   / 之前：只能按位置传；* 之后：只能按名字传；*args 收走所有多出来的位置；**kwargs 收走所有没名字对上的。
# 本文件把五个筐一次填满，再印编译器的账本（co_argcount / co_posonlyargcount / …）让你看见它怎么数的。
# ============================================================================


def kitchen(salt, oil="半勺", *extras, sugar, note="无", **leftover):
    print("    salt（位置必填）    =", salt)
    print("    oil（位置可默认）   =", oil)
    print("    extras（多出的位置）=", extras, "｜类型", type(extras).__name__)
    print("    sugar（关键字专用） =", sugar)
    print("    note（关键字带默认）=", note)
    print("    leftover（没对上的）=", leftover, "｜类型", type(leftover).__name__)


print("=== 1. 一次填满五个筐 ===")
kitchen("一勺盐", "一大勺油", "辣椒", "葱花", sugar="两勺糖", 记录="A", 备注="B")

print()
print("=== 2. 只给必填的：其余全走默认/空筐 ===")
kitchen("一勺盐", sugar="两勺糖")
print("    （extras 是空 tuple、leftover 是空 dict，**不是 None**）")

print()
print("=== 3. oil 既能按位置也能按名字：它是「位置或关键字」筐 ===")
kitchen("一勺盐", "按位置给的油", sugar="糖")
kitchen("一勺盐", oil="按名字给的油", sugar="糖")
print("    同一个参数两种写法，效果一样 —— 除了 / 之前那几个（第 6 节现场看报错）。")

print()
print("=== 4. 整批传：*列表 灌位置筐，**字典 灌名字筐 ===")
positional = ["一勺盐", "一桶油", "花椒"]
named = {"sugar": "三勺糖", "自定义键": "Z"}
kitchen(*positional, **named)
print("    args 前两个填 salt/oil，剩下的进 extras；字典整份进关键字筐。")

print()
print("=== 5. 关键字筐是**一个 dict**：能被 ** 展开，也能被塞进别的关键字参数 ===")
def forward(*args, **kw):
    return kitchen(*args, **kw)


print("  forward('盐', sugar='糖') 直接转发：")
forward("盐", sugar="糖")

print()
print("=== 6. 编译器的账本：这五个筐在函数对象上怎么记 ===")
code = kitchen.__code__
print("  co_argcount（位置筐总个数，含带默认的）=", code.co_argcount)
print("  co_posonlyargcount（/ 左边的个数）    =", code.co_posonlyargcount)
print("  co_kwonlyargcount（* 右边的个数）     =", code.co_kwonlyargcount)
print("  co_varnames（局部名字表，前几个就是参数）=", code.co_varnames)
print("  __defaults__（位置筐的默认值元组）    =", kitchen.__defaults__)
print("  __kwdefaults__（关键字筐的默认值字典）=", kitchen.__kwdefaults__)
flags = {"CO_OPTIMIZED": 0x0001, "CO_NEWLOCALS": 0x0002, "CO_VARARGS": 0x0004,
         "CO_VARKEYWORDS": 0x0008}
print("  co_flags =", hex(code.co_flags), "→", [n for n, bit in flags.items() if code.co_flags & bit])
print("  （CO_VARARGS/CO_VARKEYWORDS 两位亮起来，就说明这个函数有 *args / **kwargs）")

print()
print("=== 7. 五种筐的最小样本：一行一个，看清边界 ===")
examples = {
    "只有位置必填": lambda: (lambda a: a)(1),
    "带默认（可省）": lambda: (lambda a=2: a)(),
    "仅位置 /": lambda: (lambda a, /: a)(3),
    "关键字专用 *": lambda: (lambda *, a: a)(a=4),
    "*args + **kwargs": lambda: (lambda *x, **k: (x, k))(5, 6, k=7),
}
for label, run in examples.items():
    print(f"  {label:18} → {run()}")

print()
print("=== 8. 实战最常见的一种：*args, **kwargs 转发（写包装函数就靠它）===")


def logged(fn):
    def wrapper(*args, **kw):         # 什么形状都收
        print(f"    调用 {fn.__name__}：位置={args} 关键字={kw}")
        result = fn(*args, **kw)      # 原样转发回去
        print(f"    → 返回 {result}")
        return result
    return wrapper


@logged
def add(a, b=10):
    return a + b


add(1)
add(1, 2)
add(1, b=3)
print("  → 有了 *args/**kwargs 转发，包装函数不需要知道被包函数的签名（第 23 节装饰器就这么写）。")
