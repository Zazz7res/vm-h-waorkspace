#!/usr/bin/env python3
# ============================================================================
# 这个文件在演什么：推导式的四件事 —— 造出什么类型、双 for 谁在外层、
#                   写反了到底报什么错、变量会不会泄漏出去、两个 if 怎么排。
# 为什么演：
#   * 推导式是 Python 里唯一「写法顺序和执行顺序相反」的语句：
#       `[表达式 for a in A for b in a]` —— 读起来从左往右，执行时**左边的 for 是外层**。
#     你在 C 里写嵌套 for 永远是「外层写在外面」，视觉和结构一致；这里不一致，
#     所以写反的概率极高。本文件不解释「应该怎么写」，而是把写反的**真实报错**打出来：
#     一层是 NameError，一层是 IndexError，两种都跑给你看。
#   * 「推导式的变量不外泄」这条正好和你的 C 直觉相反：C99 里 `for (int i...)` 的 i
#     出了花括号就没了，Python 的**普通 for 循环没有这层作用域**，而**推导式有**。
#     谁泄漏谁不泄漏，用 NameError 当场判。
#
# 只用标准库。每段后面用 → 给结论。
# ============================================================================

SEP = "=" * 76


def show(tag, obj):
    print(f"  {tag:<34} {obj}")


def try_stmt(fn):
    try:
        return repr(fn())
    except Exception as e:
        return f"{type(e).__name__}: {e}"


print(SEP)
print("一、四种推导式，造出四种类型（第一个字面符号决定）")
print(SEP)
src = range(6)
lc = [x for x in src if x % 2]
sc = {x % 3 for x in src}
dc = {x: x * x for x in src if x < 3}
gc = (x for x in src if x % 2)
show("[x for x in ...] →", f"{lc}   type = {type(lc).__name__}")
show("{x for x in ...} →", f"{sc}   type = {type(sc).__name__}")
show("{k: v for ...} →", f"{dc}   type = {type(dc).__name__}")
show("(x for x in ...) →", f"{gc}   type = {type(gc).__name__}")
print("→ 注意最后一条：括号圆起来**不是** tuple，是 generator（惰性）。")
print("  想要 tuple 版本的推导式，只能套一层：`tuple(x for x in ...)`。")
show("  tuple(x for x in range(4))", tuple(x for x in range(4)))
print("  想要 list 版本也可以直接套：`list(x for x in ...)` —— 但那就该写方括号了。")
import sys
show("  量一下：list 推导式 1000 个的 getsizeof", sys.getsizeof([x for x in range(1000)]))
show("  量一下：genexpr 1000 个的 getsizeof", sys.getsizeof(x for x in range(1000)))
print("→ 200 字节 vs 8856 字节（差 44 倍），因为 genexpr 只存「怎么生成」，不存结果。")
print("  这条的尺寸差和「只能遍历一次」是同一件事的两面，genexpr_vs_map.py 正面演。")

print(SEP)
print("二、双 for：左边的 for 是外层。写反了报什么？（当场跑）")
print(SEP)
rows = [[1, 2, 3], [4, 5, 6], [7, 8, 9]]
show("正确：打平", [x for row in rows for x in row])
show("执行顺序读法", "先 for row in rows（外层），再 for x in row（内层）")
print("  错法（把两个 for 调换）：")
show("  [x for x in row for row in rows]", try_stmt(lambda: eval("[x for x in row for row in rows]", {"rows": rows})))
print("→ 报错原文逐字：`NameError: name 'row' is not defined`。")
print("  为什么不是「顺序不对所以结果奇怪」而是直接 NameError：")
print("  推导式里第一个 for 是**最外层**，它要先绑定 row；而内层表达式 `for x in row`")
print("  先执行了，那一刻 row 还不存在。也就是说 Python 是**照你写的顺序**执行的，")
print("  没有偷偷帮你重排。这一条比任何规则描述都有用。")
print("  顺带一个「不报错但顺序不一样」的合法变体 —— 两个 for 都合法，结果不同：")
show("  [(i, j) for i in range(2) for j in range(3)]", [(i, j) for i in range(2) for j in range(3)])
show("  [(j, i) for j in range(3) for i in range(2)]", [(j, i) for j in range(3) for i in range(2)])
print("→ 两行长度都是 6，元素对是彼此的转置。i 和 j 谁当外层，决定的是**谁连续变化**：")
print("  第一行是「j 变化最快」（行优先），第二行是「i 变化最快」（列优先）。")
print("  这就是你 C 里 `for i / for j` 两层嵌套的两种排法，Python 把它们并排写在同一行。")

print(SEP)
print("三、转置：两种正确写法 + 下标写反的真实报错")
print(SEP)
m = [[1, 2, 3], [4, 5, 6]]          # 2 行 3 列，故意不是方阵
show("原矩阵 m", f"{m}   {len(m)} 行 {len(m[0])} 列")
show("写法 A：行优先扫（每列一行）", [[row[i] for row in m] for i in range(len(m[0]))])
show("写法 B：用 zip 直接换", [list(col) for col in zip(*m)])
print("  错法：把两个 range 的边界写反（拿行数去索引列，或反过来）：")
show("  [[m[i][j] for j in range(len(m))] for i in range(len(m[0]))]",
     try_stmt(lambda: [[m[i][j] for j in range(len(m))] for i in range(len(m[0]))]))
print("→ 报错原文：`IndexError: list index out of range`。")
print("  过程：外层 i 会取到 0..2（因为用了 len(m[0]) = 3），")
print("        内层 j 取到 0..1（因为用了 len(m) = 2），于是 m[2] 这一行根本不存在。")
print("  方阵（3×3）时这段代码**不报错、只是结果错**，这就是它最难查的原因。量给你看：")
sq = [[1, 2, 3], [4, 5, 6], [7, 8, 9]]
show("  3×3 时用同一句错法", [[sq[i][j] for j in range(len(sq))] for i in range(len(sq[0]))])
show("  3×3 时的正确写法", [[row[i] for row in sq] for i in range(len(sq[0]))])
print("→ 两行结果不一样，但都没有报错。所以：**转置这种几何操作要用非方阵测**，")
print("  方阵会把下标越界和顺序写反两种 bug 全藏起来。")

print(SEP)
print("四、两个 if：过滤子句可以写两遍，顺序就是执行顺序")
print(SEP)
show("[x for x in range(20) if x % 2 if x > 5]", [x for x in range(20) if x % 2 if x > 5])
show("等价手写", [x for x in range(20) if (x % 2 and x > 5)])
print("→ 两个 if 是**串联的过滤器**，第一个紧挨着循环变量，所以它只能用 x，")
print("  第二个也只能用 x。想在过滤里用「算完的结果」，有两种办法：")
show("  办法①：表达式算两遍", [x * 2 for x in range(10) if x * 2 > 13])
show("  办法②：海象给中间结果起名", [y for x in range(10) if (y := x * 2) > 13])
print("  办法①把 `x * 2` 算了两次（过滤一次、生成一次）；办法②算一次。")
print("  注意办法②里 `:=` 绑定的 y 属于**推导式的作用域**，不会漏到外面（第五节验证）。")
print("  还有一个 if 放在最前面的写法，那是另一种东西（三元表达式）：")
show("  [x if x % 2 else -x for x in range(5)]（改值不改长度）", [x if x % 2 else -x for x in range(5)])
show("  [x for x in range(5) if x % 2]（改长度不改值）", [x for x in range(5) if x % 2])
print("→ 一句话分工：`for` 后面的 if 决定**要不要这个元素**（长度会变），")
print("  最前面的 `A if cond else B` 决定**这个元素变成什么**（长度不变）。")

print(SEP)
print("五、变量泄漏：普通 for 会漏，推导式不漏（对照 C 的块作用域）")
print(SEP)
i = "外面原有的 i"
for i in range(3):
    pass
show("普通 for 跑完后 i", i)
j = "外面原有的 j"
out = [j for j in range(4)]
show("推导式跑完后 j", j)
show("推导式自己的结果", out)
print("→ 这就是本机量出来的差别：")
print("    普通 for：**没有**作用域，循环变量写进了当前作用域，把 j 顶掉了；")
print("    推导式：**有**自己的作用域，j 出不来，外面那个 j 完好。")
print("  你的 C 直觉在这里要反过来用：C99 里 `for (int k = 0; ...)` 的 k 出花括号就没了，")
print("  Python 的 for 循环是「漏」的那一方，推导式才是「不漏」的那一方。")
print("  嵌套推导式也一起验：")
inner = [[q for q in range(2)] for p in range(2)]
for name in ("p", "q"):
    print(f"  {name} 还能访问吗：", end="")
    try:
        print(f"能，值 = {eval(name)!r}")
    except NameError as e:
        print(f"NameError: {e}")
show("  嵌套推导式的结果", inner)
print("  最外面那层的**可迭代对象**是在外层作用域求值的，这一条容易记混，测一下：")
seq = [1, 2]
show("  迭代器本身写错名字", try_stmt(lambda: eval("[v for v in nosuchseq]", {"seq": seq})))
print("→ 名字找不到时，报错指向的是那个没定义的名字，不会指向推导式内部。")
print("  实用结论：推导式里出现的名字要么在外层存在，要么是它自己的循环变量。")

print(SEP)
print("六、两个容易忽略的语义：键冲突 和 自动去重")
print(SEP)
show("dict 推导式遇到同名键（banana）", {ch: idx for idx, ch in enumerate("banana")})
print("→ 同一个键写了多次，**最后一次赢**；但键的位置是第一次插入时定的")
print("  （和 dict_key_pitfalls.py 第六节那条完全一致：覆盖不改变位置）。")
show("set 推导式的长度", len({x % 3 for x in range(10)}))
show("  原来的长度", 10)
print("→ set 推导式会**悄悄变短**：10 个元素只有 3 个余数。")
print("  如果你要靠长度判断「处理了几条」，用 list，别用 set。")
show("list 推导式造 3 层独立对象（对法）", [[0 for _ in range(3)] for _ in range(2)])
bad = [[0] * 3] * 2
good = [[0 for _ in range(3)] for _ in range(2)]
good[0][1] = 9
show("  对法改一格", f"{good}   ← 另一行不动")
bad[0][1] = 9
show("  错法改一格", f"{bad}   ← 两行一起变")
print("→ alias_pitfalls.py 第四节的口诀在这里兑现：**有几层独立，就写几个 for**。")
