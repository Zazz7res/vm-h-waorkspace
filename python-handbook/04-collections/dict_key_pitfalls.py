#!/usr/bin/env python3
# ============================================================================
# 这个文件在演什么：容器的「字面量陷阱」和「键的规矩」——
#   {} 是 dict 不是 set、单元素 tuple 的逗号、不可哈希的键报什么原文、
#   dict 的插入顺序（3.7 起）和 == 为什么无视顺序。
# 为什么演：
#   * 这四条都属于「语法看起来一样、类型完全不是那个」的坑。C 里没有这种坑：
#     类型写在声明里，`int a[10]` 不可能是别的东西。Python 的字面量靠符号猜，
#     猜错了不报错，只是行为跟你预期差一层。
#   * 「dict 保序」这件事网上的说法五花八门（3.6 是实现细节、3.7 起是语言规范、
#     CPython 更早就已经保序了）。这里不引用说法，直接在本机 3.14.4 上量：
#     键的遍历顺序 = 插入顺序，删除再插入会排到末尾，而 == 完全不看顺序。
#   * 不可哈希的报错原文，不同 Python 版本措辞不一样。本文件打的是**本机这一版**
#     的原文，逐字可复现；老教程里那句短的 `unhashable type: 'list'` 在哪里、
#     和这条什么关系，也一并量给你看。
#
# 只用标准库。每段后面用 → 给结论。
# ============================================================================

SEP = "=" * 76


def show(tag, obj):
    print(f"  {tag:<32} {obj}")


def try_expr(src):
    """真跑一句表达式；成功打值，失败打「异常类型: 原文」。报错原文逐字来自本机。"""
    try:
        val = eval(src)
    except Exception as e:
        return f"{type(e).__name__}: {e}"
    return f"{val!r}   类型 = {type(val).__name__}"


print(SEP)
print("一、{} 是空 dict，不是空 set；set() 才是空 set")
print(SEP)
for src in ["{}", "{1}", "{1: 2}", "set()", "frozenset()", "[]", "()", "tuple()", "dict()"]:
    show(src, try_expr(src))
print("→ 三行关键对比：")
print("    `{}` → dict（`{k: v}` 带冒号，这对空花括号历史上先被 dict 占了）")
print("    `{1}` → set（一个值、没冒号，那就是集合）")
print("    `set()` → set（空集合只能这么写，它没有字面量）")
print("  顺手量一下代价（数字出自 container_sizes.py，这里只贴同一条）：")
import sys
show("  sys.getsizeof({})", sys.getsizeof({}))
show("  sys.getsizeof(set())", sys.getsizeof(set()))
print("→ 所以「空的那个容器」写成 {} 比写成 set() 便宜 152 字节，")
print("  但 {} 是 dict，你要去重就得 set() —— 省不省内存取决于你装不装东西，别看空壳。")

print(SEP)
print("二、单元素 tuple 的逗号：括号不是标志，逗号才是")
print(SEP)
for src in ["(1)", "(1,)", "(1, 2)", "()", "(1,)[0]", "type((1))"]:
    show(src, try_expr(src))
show("1, 这个字面量的类型", eval("1,"))
show("(1,) == (1,)", (1,) == (1,))
print("→ `(1)` 就是带括号的整数 1；`(1,)` 才是长度为 1 的 tuple。")
print("  更反直觉的一行：连括号都可以不要 —— 逗号本身就是 tuple 的标志：")
bare = 1,
show("  bare = 1,  → bare", f"{bare}   type = {type(bare).__name__}")


def one_value():
    return 1,          # ← 这个逗号让函数返回 (1,) 而不是 1


show("  def f(): return 1,  → f()", one_value())
show("  长度", len(one_value()))
print("  → `return 1,` 返回的是 (1,)。这是「调用方拿到一个 tuple 而不是数字」")
print("    这类 bug 的第一名来源，而且它在 3.14 上连警告都不给（写法完全合法）。")
print("  对法：要么明确写 `return 1`，要么明确写 `return (1,)`，别留裸逗号。")
print("→ 顺带一条「跨容器类型 ==」的实测矩阵（别背规则，看结果）：")
import collections
matrix = [
    ("[1, 2] == (1, 2)", [1, 2] == (1, 2)),
    ("(1, 2) == [1, 2]", (1, 2) == [1, 2]),
    ("{1, 2} == [1, 2]", {1, 2} == [1, 2]),
    ("{1, 2} == frozenset({1, 2})", {1, 2} == frozenset({1, 2})),
    ("deque([1, 2]) == [1, 2]", collections.deque([1, 2]) == [1, 2]),
    ("OrderedDict([('a',1)]) == {'a': 1}", collections.OrderedDict([("a", 1)]) == {"a": 1}),
    ("1 == 1.0（数字对照组）", 1 == 1.0),
]
for tag, val in matrix:
    show(f"  {tag}", val)
print("  → 顺序型（list / tuple / deque）之间一律 False，可它们和「集合型」也是 False；")
print("    唯一跨类相等的是 set ↔ frozenset（同为「集合值」）和 dict ↔ OrderedDict（同为「映射值」）。")
print("    数字那行是对照组：1 == 1.0 是 True，所以「跨类型 == 必 False」这种记法也不能背。")

print(SEP)
print("三、不可哈希的键：报错原文逐字（本机 3.14.4）")
print(SEP)
for src in ["{[1, 2]: 1}", "{[1, 2]}", "set([[1, 2]])", "{{1: 2}}", "{(1, [2])}", "{(1, {2})}"]:
    show(src, try_expr(src))
print("→ 读这条原文：`cannot use 'list' as a dict key (unhashable type: 'list')`")
print("    前半句告诉你**用在哪个位置**（dict key 还是 set element），括号里那半句")
print("    `unhashable type: 'list'` 才是「不可哈希」本身。抓报错的时候两个都要看：")
print("    前半句定位场景，后半句定位病因。本手册只在 3.14.4 上验过，")
print("    别的版本措辞有没有差，我没在这台机器上跑过别版本，所以不下结论。")
print("  特别注意 `{(1, [2])}` 这行：外壳是 tuple，看着可哈希，")
print("  但 tuple 的哈希是**递归**算的，里面那个 list 让它当场不可哈希；")
print("  括号里报的类型是 'list' 不是 'tuple' —— 这是好事，它告诉你是哪一层坏了。")
print("  （第 02 章第 6 节演过同一个 tuple 装 list 的另一面：`t[1].append(...)` 能改。）")
print("  为什么必须是「不可变」才能当键？你要的那个东西在 08-hash 里写过：")
print("  桶的位置 = h(key) % 桶数。如果键自己变了，h 变了，它就该在另一个桶里，")
print("  于是「明明放进去过却找不到」。Python 直接把会变的东西的 __hash__ 设成 None：")
for src in ["list.__hash__", "dict.__hash__", "set.__hash__", "tuple.__hash__", "int.__hash__", "str.__hash__", "frozenset.__hash__"]:
    show(f"  {src}", eval(src))
print("→ 前三个是 None —— 这就是 unhashable 的物理来源；后四个才是真有哈希函数的。")
print("  对法：把「可变的身份」变成「不可变的快照」再当键：")
show("  用 tuple 当键", {(1, 2): "OK"}[(1, 2)])
show("  用 frozenset 当键", {frozenset({1, 2}): "OK"}[frozenset({1, 2})])

print(SEP)
print("四、dict 保序：遍历顺序 == 插入顺序（本机实测）")
print(SEP)
d = {}
for k in ["banana", "apple", "cherry", "date"]:
    d[k] = len(k)
show("插入 banana→apple→cherry→date 后", d)
show("list(d.keys())", list(d.keys()))
show("sorted(d)", sorted(d))
print("→ 遍历顺序就是插入顺序，不是字典序、也不是哈希序。想按别的顺序只能 sorted()。")
print("  删除后再插同一个键，它会**排到末尾**（不是回到原位）：")
del d["apple"]
show("  del 'apple' 之后", list(d.keys()))
d["apple"] = 5
show("  再插 'apple' 之后", list(d.keys()))
print("→ 这条对你有实际影响：你在 C 的哈希表里，槽位由 h(key) 决定，")
print("  「顺序」根本不存在；CPython 的 dict 额外维护了一个**紧凑的插入序列**，")
print("  所以顺序才是可依赖的（3.7 起是语言规范，不是 CPython 的实现巧合）。")
print("  整数键会不会露馅？我原本猜会：int 的哈希值就是它自己，投桶时应该按数值排。")
print("  实测把这条猜测打掉了：")
for ks in ([3, 1, 2, 10], [5, 4, 3, 2, 1], [1000003, 3, 1000001]):
    di = {}
    for k in ks:
        di[k] = 0
    show(f"  按 {ks} 插入", list(di.keys()))
print("→ 三行全是插入顺序，倒着插就倒着遍历。桶位（索引数组）决定的是「找得快不快」，")
print("  遍历走的是那条插入序列 —— 这正是 CPython 3.6 之后 dict 布局改动的目的。")
print("  结论别写反：**dict 保证的是「插入顺序」，不是「键的排序」**；要别的顺序只能 sorted()。")

print(SEP)
print("五、== 无视顺序；list / tuple 的 == 恰恰看顺序")
print(SEP)
a = {"x": 1, "y": 2}
b = {"y": 2, "x": 1}
show("a = {'x':1,'y':2} / b = {'y':2,'x':1}", a == b)
show("a is b", a is b)
show("list(a.items()) == list(b.items())", list(a.items()) == list(b.items()))
c = [1, 2]
e = [2, 1]
show("[1,2] == [2,1]", c == e)
show("(1,2) == (2,1)", (1, 2) == (2, 1))
show("{1,2} == {2,1}", {1, 2} == {2, 1})
print("→ 一句话分工：dict / set 的相等是「成员集合相同」，顺序无关；")
print("  list / tuple 的相等是「逐格、按位置比」，顺序有关。")
print("  上面第三行 `list(a.items()) == list(b.items())` 打的是 False ——")
print("  它是把 dict 降级成**有序** list 来比，顺序又变成有关系的了。")
print("  所以想要「内容一样、顺序也要一样」就写这一句；只比内容就写 ==。别混。")

print(SEP)
print("六、dict 的键必须唯一：同一个键插两次，后者赢，位置不变")
print(SEP)
d2 = {}
d2["k"] = 1
d2["z"] = 0
d2["k"] = 99
show("k=1, z=0, k=99 之后", d2)
show("list(d2.keys())", list(d2.keys()))
print("→ 覆盖值不会把这个键挪到末尾 —— 位置是第一次插入时定的。")
print("  用推导式「合并两个 dict」时，这条决定了谁赢：")
show("  {**base, **over}", {**{"a": 1, "b": 2}, **{"b": 20, "c": 3}})
show("  Counter 的加法是另一种语义", "见 counter_probe.py 第三节")
