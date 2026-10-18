#!/usr/bin/env python3
# ============================================================================
# e_arg_passing.py —— 第 05 章第 7 节：传参到底是「传值」还是「传引用」？
#
# 都不是。Python 的规则就一条，和第 02 章一模一样：**传参是一次赋值，赋值是贴名字**。
# 于是「函数能不能改到外面的东西」只取决于一件事：
#   你写的是 `名字 = ...`（重绑定，外面看不见），还是 `东西.方法()/名字[i] = ...`（原地改，外面看得见）。
# 本文件把 list 的五种写法一次跑全，再把不可变对象和「不可变的壳装可变的芯」补上。
# ============================================================================

OUTER = [1, 2, 3]


def rebind(xs):
    xs = [9, 9, 9]                  # 把局部名字 xs 揭下来贴到新表上
    return xs


def mutate(xs):
    xs.append(9)                    # 同一个对象，往里塞 → 原地改
    return xs


def index_assign(xs):
    xs[0] = 9                       # 下标赋值走 __setitem__ → 也是原地改
    return xs


def concat(xs):
    xs = xs + [9]                   # + 造了一张新表，再贴给局部名字 → 相当于重绑定
    return xs


def augmented(xs):
    xs += [9]                       # ★ list 的 += 走 __iadd__，是**原地扩展** → 外面看得见！
    return xs


def pop_front(xs):
    return xs.pop(0)                # 原地删（第 04 章讲过 list.pop(0) 是 O(n)）


FUNCS = (rebind, mutate, index_assign, concat, augmented, pop_front)

print("=== 1. 六种写法：函数体里那一行到底是什么操作，决定外面脏不脏 ===")
print(f"{'函数':16}{'外面的表跑完后':22}{'返回值':22}{'返回值是同一个对象吗'}")
for fn in FUNCS:
    outer = [1, 2, 3]               # 每次都从干净的一张表开始
    result = fn(outer)
    print(f"{fn.__name__:16}{str(outer):22}{str(result):22}{result is outer}")

print()
print("=== 2. 只有两条规律，上面六行全能推出来 ===")
print("  名字 = anything        → 只动局部那个名字，外面一律看不见（rebind / concat）")
print("  名字.append / [i]= / += → 动的是那个对象本体，外面全看得见（mutate / index_assign / augmented）")
print("  判据就一个：这一行有没有出现「名字 =」这种单纯赋值。")

print()
print("=== 3. 传参没有拷贝：函数里那个名字和外面贴的是同一个对象 ===")


def probe(xs):
    return xs is OUTER


sample = [1, 2, 3]
print("  probe(OUTER) →", probe(OUTER))
print("  probe(sample)（另一个同名不同值的表）→", probe(sample))
print("  所以「传引用」也不准确：传的是**那个对象的绑定**，一次赋值而已。")

print()
print("=== 4. 不可变对象：函数里怎么折腾都改不动外面（不是魔法，是没得改）===")


def bump(n):
    n = n + 1
    return n


def try_mutate(text):
    try:
        text[0] = "X"
    except TypeError as e:
        return f"{type(e).__name__} | {e}"


x = 10
print("  bump(x) =", bump(x), "｜ x 还是 =", x)
s = "abc"
print("  s 里改第一个字符：", try_mutate(s))
print("  想要结果只能靠返回值：x =", x, "→ 接回来才变：", end=" ")
x = bump(x)
print("x 现在是", x)

print()
print("=== 5. 不可变的壳 + 可变的芯：照样改得动 ===")


def poke(inner):
    inner[0].append("脏了")         # 动的是壳里那个 list，不是壳
    return inner


container = (["干净"],)             # tuple 装 list
print("  传进去的 container =", container)
print("  poke 之后 =", poke(container), "｜ 外面看同一个：", container)
print("  外层是不是同一个对象：", poke(container) is container, "（tuple 没被换，只是芯被改了）")

print()
print("=== 6. 想「不收脏」就在函数里自己拷贝一层（第 04 章那六级梯子在这儿用）===")


def naive_sort_append(xs):
    xs.sort()                       # 原地排：调用方的表跟着被排（很多人踩）
    xs.append("尾巴")
    return xs


def polite_sort_append(xs):
    copy = list(xs)                 # 浅拷贝一层：外层槽位各拿一份，芯仍共享
    copy.sort()
    copy.append("尾巴")
    return copy


shared = [3, 1, 2]
print("  naive 版返回：", naive_sort_append(shared), "｜ 调用方的 shared 变成：", shared)
shared2 = [3, 1, 2]
print("  polite 版返回：", polite_sort_append(shared2), "｜ 调用方的 shared2 没动：", shared2)
print("  浅拷贝只保护外层槽位：内层还是同一个对象（第 04 章 copy 那一节的结论，这里再验一次）")

nested = [[3], [1]]
polite_copy = list(nested)
polite_copy[0].append(99)
print("  改拷贝里的内层，原件跟着脏：", nested)

print()
print("=== 7. 可变/不可变在这里的**唯一**实际差别 ===")
print("  不可变对象（int/str/tuple/frozenset/bytes/None）：想改只能造新的 → 函数永远改不动外面；")
print("  可变对象（list/dict/set/bytearray/自定义类实例）：能原地改 → 函数改得动外面。")
print("  所以第 5 节那个坑只会出现在可变默认值上，和第 15 节的作用域规律是同一族。")


def freeze_probe(value):
    kind = type(value).__name__
    try:
        value.append("?")
        return f"{kind}：能原地追加 → 函数改得动外面"
    except AttributeError:
        return f"{kind}：根本没有 append 这个方法 → 只能造新的"
    except TypeError as e:
        return f"{kind}：有 append，但这一类值不收（{e}）"


for sample_value in ([1], {"k": 1}, {1}, bytearray(b""), (1,), "文本", 42, None, frozenset({1})):
    print("  ", freeze_probe(sample_value))

print()
print("=== 8. 一句话收口 ===")
print("  函数改不改得动外面 = 你有没有原地改那个对象；")
print("  函数返回值能不能救回来 = 你接不接。两条都和第 02 章「赋值是贴名字」同一条规律。")

