#!/usr/bin/env python3
# ============================================================================
# g_param_errors.py —— 第 05 章第 10 节：参数写错的报错原文（五种 TypeError + 五种 SyntaxError）
#
# 分工先记住：**SyntaxError = 这一行根本没法编译**（def 的写法违规、调用时同一个名字写两遍），
#              **TypeError = 能编译，但调用时形状对不上**（多给、少给、名字乱认、一个参数喂两遍）。
# 这一节的原文值得整段抄进笔记：以后碰到时你能一眼认出是哪一类。
# ============================================================================

print("=== 1. 调用形状对不上：六种 TypeError 原文 ===")


def two(a, b):
    return a + b


def kwonly(a, *, need):
    return (a, need)


def pos_only(a, /, b):
    return (a, b)


cases = [
    ("位置参数多给一个", lambda: two(1, 2, 3)),
    ("必填少给一个", lambda: two(1)),
    ("名字不认识", lambda: two(1, 2, c=3)),
    ("同一个参数喂两遍", lambda: two(1, 2, b=3)),
    ("关键字专用没给", lambda: kwonly(1)),
    ("仅位置参数按名字给", lambda: pos_only(a=1, b=2)),
]

for label, call in cases:
    try:
        print(f"  {label:20} → 没报错？结果 = {call()}")
    except TypeError as e:
        print(f"  {label:20} → TypeError | {e}")

print()
print("读法：前五条都是「数量/名字」问题；最后一条 pos_only 是**传法**问题 —— "
      "同一个值换个写法就不报错（pos_only(1, 2) 正常）。")
print("  对照：", pos_only(1, 2), "｜", pos_only(1, b=2), "｜", end=" ")
try:
    pos_only(1, 2, 3)
except TypeError as e:
    print("多给 →", e)

print()
print("=== 2. def 那一行写错顺序：六种 SyntaxError 原文（用 compile 单独抓，不让它带走整个文件）===")
orders = [
    ("默认参数排在必填前面", "def f(a=1, b): pass"),
    ("参数没有值（等号后面空着）", "def f(a=): pass"),
    ("**kwargs 不是最后一位", "def f(**kw, c): pass"),
    ("/ 排在 * 后面", "def f(*args, /): pass"),
    ("两个 *args", "def f(*a, *b): pass"),
    ("关键字专用排在 / 前面（顺序反了）", "def f(*, a, b=1, /): pass"),
]
for label, src in orders:
    try:
        compile(src, "<示例>", "exec")
        print(f"  {label:26} → 竟然编译通过")
    except SyntaxError as e:
        print(f"  {label:26} → SyntaxError | {e.msg}")

print("  一个**合法**但常被误判的写法：*args 后面还能跟名字，只是它从此只能按关键字传：")


def star_then_name(*args, b):
    return (args, b)


print("    star_then_name(1, 2, b=3) =", star_then_name(1, 2, b=3), "｜ 不给 b 的名字：", end=" ")
try:
    star_then_name(1, 2, 3)
except TypeError as e:
    print(f"TypeError | {e}")

print()
print("=== 3. 调用处的语法错误（这三条也是 SyntaxError，不是 TypeError）===")
for label, src in [
    ("同一个关键字写两遍", "two(a=1, a=2)"),
    ("位置参数排在了关键字参数后面", "two(1, b=2, 3)"),
    ("def 里裸 * 后面什么都没有", "def f(*, **kw): pass"),
]:
    try:
        compile(src, "<示例>", "eval")
        print(f"  {label:24} → 编译通过（那就是运行时问题）")
    except SyntaxError as e:
        print(f"  {label:24} → SyntaxError | {e.msg}")

print()
print("=== 4. ** 展开时字典里装了什么：三条边界 ===")
print("  键是 int：", end=" ")
try:
    two(**{1: 1, 2: 2})
except Exception as e:
    print(f"{type(e).__name__} | {e}")
print("  键是带空格的字符串：", end=" ")
try:
    print(two(**{"a": 1, "b c": 2}))
except Exception as e:
    print(f"{type(e).__name__} | {e}")
print("  键多了个不认识的（能跑，进 **kwargs 才不报错；这里 two 没有 kwargs）：", end=" ")
try:
    print(two(**{"a": 1, "b": 2, "c": 3}))
except Exception as e:
    print(f"{type(e).__name__} | {e}")

print()
print("=== 5. 一个「能跑但一定错」的隐蔽形状：参数名和局部名撞了 ===")


def shadowed(value):
    value = value.strip() if isinstance(value, str) else value
    return value


print("  shadowed('  去空格  ') =", repr(shadowed("  去空格  ")))
print("  这条不报错，但和第 15 节 UnboundLocalError 只差一个 if：")


def almost(value):
    if False:
        value = "改了"          # 编译期就算数 → value 已经是「局部」
    return value


print("  almost 的局部表里有 value 吗：", "value" in almost.__code__.co_varnames)
print("  （因为参数本来就是局部名字，所以参数名被自己遮蔽时**不会**报 UnboundLocalError，"
      "只会静悄悄变成参数原值 —— 比第 15 节那个更难查。）")

print()
print("=== 6. 收口：三类错误各自的修法 ===")
print("  SyntaxError（def 行/调用行写坏）→ 按第 9 节那条顺序改列；")
print("  TypeError（形状对不上）→ 看报错里的**名字**，它总告诉你缺的是哪个；")
print("  静悄悄错（本节第 5 条）→ 只能靠 inspect.signature 或读 __code__.co_varnames（第 3 节的账本）。")
