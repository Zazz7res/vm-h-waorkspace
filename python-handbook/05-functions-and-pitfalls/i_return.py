#!/usr/bin/env python3
# ============================================================================
# i_return.py —— 第 05 章第 12 节：return 的三件事（没写就是 None / 逗号造元组 / 提前收工）
#
# 澄清两个流传很广的误解：
#   误解 1「函数可以不写 return」→ 对，但不写不是「没有返回值」，是**返回 None**。
#   误解 2「return a, b 能返回两个值」→ 不对，return 永远只交一个东西，
#          是两个值的**逗号**先造了一个 tuple，return 只是把这个 tuple 交出去。
# 顺带：第 06 章第 5 节那个「__init__ 里 return self 报错」也是这条规律的下游。
# ============================================================================

print("=== 1. 三种「没给返回值」的写法，结果都是 None ===")


def only_print(text):
    print("    （只打印，不 return）", text.upper())


def bare_return(n):
    if n > 0:
        return                    # ← return 后面不写东西 = return None
    return n


def falls_off_end(n):
    if n > 0:                     # 没有 else，n<=0 时走到函数末尾
        return n


a = only_print("hi")
print("  only_print 的返回值 =", a, f"（{type(a).__name__}）")
print("  bare_return(-1) =", bare_return(-1), "｜ bare_return(5) =", bare_return(5))
print("  falls_off_end(0) =", falls_off_end(0), "｜ falls_off_end(3) =", falls_off_end(3))
print("  → 所以 `if f(x):` 在 f 返回 None 时是假；想区分「没结果」和「结果为 0」必须显式返回。")

print()
print("=== 2. 「返回多个值」的真相：逗号造元组 ===")


def quotient_remainder(a, b):
    return a // b, a % b          # 这一行的主角是逗号，不是 return


packed = quotient_remainder(7, 2)
print("  不拆开接：packed =", packed, f"（{type(packed).__name__}）")
q, r = quotient_remainder(7, 2)
print("  拆开接：q, r =", q, r)
q2, *tail = quotient_remainder(9, 4)
print("  星号接剩余：q2 =", q2, "tail =", tail)
print("  它俩完全等价：return (a, b) →", (lambda a, b: (a, b))(7, 2) == packed)

print()
print("=== 3. 逗号自己就会造元组（和函数没关系，回指第 02 章）===")
x = 1,
print("  x = 1,      →", x, f"（{type(x).__name__}）← 单元素元组必须带那个逗号")
y = 1, 2, 3
print("  y = 1, 2, 3 →", y)
print("  括号只是可选的：(1, 2, 3) == y →", (1, 2, 3) == y)
print("  函数返回三个值也是这一个 tuple：", end=" ")


def three():
    return 1, 2, 3


print(three(), len(three()))

print()
print("=== 4. return 提前收工：后面一行都不跑 ===")


def early(n):
    total = 0
    for i in range(n):
        if i == 3:
            return total          # 直接结束整个函数
        total += i
    return -1                     # ← 这行永远轮不到


print("  early(10) =", early(10), "（0+1+2=3，走到 i==3 就交了）")
print("  对比 continue 只跳过本次：")


def skips(n):
    seen = []
    for i in range(n):
        if i % 2:
            continue
        seen.append(i)
    return seen


print("  skips(6) =", skips(6))

print()
print("=== 5. 返回值只能有一个「出口」：想同时给状态和数据就打包 ===")


def parse(text):
    try:
        return True, int(text)
    except ValueError:
        return False, None


for sample in ("42", "四十二"):
    ok, value = parse(sample)
    print(f"  parse({sample!r}) → 成功={ok} 值={value!r}")
print("  反面写法（只返回数据，出错时返回 None）看着省一步，调用方就没法区分「值是 None」和「失败」：")


def parse_lossy(text):
    try:
        return int(text)
    except ValueError:
        return None


print("    parse_lossy('0') =", parse_lossy("0"), "｜ 用 `if not x` 判断就完了（第 02 章假值那一节）")

print()
print("=== 6. 生成器的 return 是另一回事（第 10 章），这里只留个对照 ===")


def gen(n):
    yield n
    return "收尾"


g = gen(1)
print("  type(gen(1)) =", type(g).__name__, "｜ 它不是 tuple 也不是值，是个迭代器")
print("  取出：", next(g), end=" ")
try:
    next(g)
except StopIteration as e:
    print("｜第二次 next 抛 StopIteration，value =", repr(e.value), "（return 的值藏在异常里）")

print()
print("=== 7. 收口 ===")
print("  return 只管「交一个东西 + 结束函数」；几个值、返回什么形状，全是逗号和括号决定的。")
