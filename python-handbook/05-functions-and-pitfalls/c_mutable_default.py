#!/usr/bin/env python3
# ============================================================================
# c_mutable_default.py —— 第 05 章第 5 节：可变默认参数陷阱（全书最经典那个坑）
#
# 现场一句话：函数默认值是 `[]` / `{}` / `set()` 时，**所有调用共用同一个容器**，
# 于是「每次新表」变成了「越攒越多」。第 4 节已经证明了原因（def 只跑一次），
# 这一节只负责让你看见它有多离谱 —— 以及它**不会**报任何错。
# ============================================================================

print("=== 1. 坑本体：三次调用，账越攒越多 ===")


def bad_append(item, box=[]):          # ← 凶案现场就在这一行
    box.append(item)
    return box


print("bad_append(1) =", bad_append(1))
print("bad_append(2) =", bad_append(2), "← 1 为什么还在？")
print("bad_append(3) =", bad_append(3))
print("第四次调用之前，账本 bad_append.__defaults__ =", bad_append.__defaults__)

print()
print("=== 2. 换个名字调也一样：因为脏的是那个 list，不是名字 ===")
alias = bad_append
print("alias(4) =", alias(4))
print("两条路拿到的是同一个容器：bad_append(5) == alias(5) →", bad_append(5) == alias(5))
print("再印账本：", bad_append.__defaults__)

print()
print("=== 3. 返回值就是那个默认容器本身：外面拿到的是活的引用 ===")
mine = bad_append("新东西")
print("mine =", mine)
mine.append("外面塞的")
print("外面往 mine 里塞一个，账本也跟着变：", bad_append.__defaults__)
print("下次调用带着这个脏底开始：bad_append('下一次') =", bad_append("下一次"))

print()
print("=== 4. 报错是零：这就是它贵在哪 ===")
print("整段跑到现在没有一条 TypeError/ValueError —— 代码「按写的做」了，只是不是你想要的。")
print("典型生产事故形态：缓存函数、累积日志、递归收集器。下面这条就是递归版：")


def collect(n, out=[]):                # 递归 + 可变默认值 = 两次调用之间互相串数据
    if n:
        out.append(n)
        collect(n - 1, out)
    return out


print("  第一次 collect(3) =", collect(3))
print("  第二次 collect(2) =", collect(2), "← 上一次的 3 还在，而且这次又攒了一遍")
print("  账本：", collect.__defaults__)

print()
print("=== 5. 哪些默认值会中枪：只有「可变的」那几种 ===")


def bad_dict(k, table={}):
    table[k] = table.get(k, 0) + 1
    return table


def bad_set(item, seen=set()):
    seen.add(item)
    return sorted(seen)


def bad_bytearray(tag, buf=bytearray()):
    buf.extend(tag.encode())
    return len(buf)


print("dict 版三次：", bad_dict("a"), bad_dict("a"), bad_dict("b"))
print("set  版三次：", bad_set(3), bad_set(1), bad_set(3))
print("bytearray 版三次长度：", bad_bytearray("x"), bad_bytearray("yy"), bad_bytearray("z"))
print("账本们：", bad_dict.__defaults__, bad_set.__defaults__, bad_bytearray.__defaults__)
print("连类本身都能当默认值（可变的是它管的东西，坑更大）：")


def bad_class(rows=[]):
    rows.append(1)
    return rows


print("  bad_class 三次：", bad_class(), bad_class(), bad_class())

print()
print("=== 6. 安全的一半：不可变默认值不会中枪 ===")


def head(text, prefix=">>"):            # str 不可变 → 每次只能重造，攒不起来
    return prefix + text


def count_to(n, start=0):               # int 同理
    return start + n


def pair(a, b, empty=()):               # tuple 不可变（但里面装可变对象仍能被改，见第 7 节）
    return (a, b) + empty


print("head 两次：", head("甲"), head("乙"))
print("count_to 两次：", count_to(3), count_to(4))
print("pair 两次：", pair(1, 2), pair(3, 4))

print()
print("=== 7. 唯一的例外：不可变的壳 + 可变的芯 ===")
POISON = ([],)                          # tuple 里装一个 list


def bad_inner(tag, box=POISON):         # 外层 tuple 不可变，但 box[0] 能 append
    box[0].append(tag)
    return box[0]


print("bad_inner 三次：", bad_inner("a"), bad_inner("b"), bad_inner("c"))
print("POISON 本体已经被写脏：", POISON)
print("  判据修正：不是「默认值类型不可变就安全」，而是「**没有任何一条路径能原地改它**」才安全。")
