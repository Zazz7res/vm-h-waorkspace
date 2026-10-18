#!/usr/bin/env python3
# ============================================================================
# 这个文件在演什么：生成器表达式 / map / filter / 列表推导式 这四兄弟的
#                   真实差别 —— 尺寸、惰性、可遍历几次、以及快多少。
# 为什么演：
#   * 「生成器省内存」这种话你一定听过。这里把它量成三个可复核的数字：
#     同一批 1000 个元素，列表推导式的外壳是 8856 字节，生成器是 200 字节；
#     造 100 万个元素时，前者峰值内存 38 MiB，后者 29 KiB（本机 tracemalloc 量的）。
#   * 更要紧的是**语义**差别，不是性能差别：生成器 / map / filter 都是**一次性**的。
#     你在 C 里写一个「产出下一个值」的函数，得自己存游标（iter、pos、剩余长度），
#     存错了就死循环；Python 把游标封在对象里，代价就是「走过就没了」。
#     本文件第四节把它演成两次遍历，第一次一堆、第二次空的，当场看见。
#   * 第五节的计时结果和很多人的直觉相反：`sum(生成器)` 比 `sum(列表)` **快**，
#     不是慢。原因写在输出里（少了一次「造整份列表再拆掉」）。
#
# 只用标准库（sys / timeit / tracemalloc）。
# 计时口径：每格跑 timeit.repeat(number=1, repeat=5) 取最小值；
# 每格都是「从 range(0) 开始造完整一轮」，所以数字里含着造元素的成本 —— 这正是对比项。
# ============================================================================
import itertools
import sys
import timeit
import tracemalloc

SEP = "=" * 76


def show(tag, obj):
    print(f"  {tag:<38} {obj}")


print(SEP)
print("一、四种写法各造出什么对象，外壳多大")
print(SEP)
xs = list(range(1000))
objs = {
    "列表推导式 [x*x for x in xs]": [x * x for x in xs],
    "生成器表达式 (x*x for x in xs)": (x * x for x in xs),
    "map(str.upper, ['a'])": map(str.upper, ["a"]),
    "filter(None, [0, 1, 2])": filter(None, [0, 1, 2]),
    "range(1000)": range(1000),
}
for tag, o in objs.items():
    print(f"  {tag:<34} type={type(o).__name__:<10} getsizeof={sys.getsizeof(o):>6} 字节")
print("→ 三档尺寸，三种性格：")
print("    列表推导式：全部算完、全部存下来（8856 字节，且不含元素本体，见 container_sizes.py）。")
print("    生成器 / map / filter：只有 208 / 56 / 48 字节 —— 里面存的是「函数 + 输入 + 游标」，")
print("      一个结果都还没有。这三样在协议上是同一种东西：**一次性的迭代器**")
print("      （第四节正面演「第二次读就是空的」，那条比尺寸更要命）。")
print("    range：48 字节，和长度无关（container_sizes.py 第一节量过 range(10) 也是 48）。")

print(SEP)
print("二、惰性：到底什么时候才真的算")
print(SEP)
log = []


def work(n):
    log.append(n)
    return n * n


gen = (work(n) for n in [1, 2, 3])
show("  建好之后 log（还没取值）", log)
first = next(gen)
show(f"  取了第 1 个（值 = {first}）之后 log", log)
show("  再取第 2 个", (next(gen), log))
show("  全部喂给 list 之后", (list(gen), log))
print("→ 输出里 log 是一格一格变长的：`work` 只在被取走的那一刻执行。")
print("  列表推导式做不到这件事 —— 建好时 3 次调用已经全跑完了：")
log.clear()
lc = [work(n) for n in [1, 2, 3]]
show("  建好列表推导式的瞬间 log", log)
print("  这就是「惰性」的全部含义：**表达式写在那里不等于它执行了**。")
print("  反过来说，写 `[f(x) for x in big if guard(x)]` 时，big 里每一个 x 都被摸过一遍，")
print("  哪怕你只想要前 3 个。想要「摸 3 个就停」，只能生成器 + next() 或 itertools.islice。")

print(SEP)
print("三、停下来只要一点点：next() 和列表推导式的对比")
print(SEP)
calls = 0


def counted(n):
    global calls
    calls += 1
    return n


calls = 0
show("取前 3 个（生成器 + islice）", list(itertools.islice((counted(n) for n in range(10 ** 6)), 3)))
show("  实际调用了几次", calls)
calls = 0
show("取前 3 个（先造整份列表）", [counted(n) for n in range(10 ** 6)][:3])
show("  实际调用了几次", calls)
print("→ 同一个答案，调用次数 3 vs 1000000。")
print("  `[:3]` 那行是「先全造再切」，切片省的是**后面你的代码**的开销，省不了构造的开销")
print("  （切片本身为什么便宜，见 slicing_probe.py）。")

print(SEP)
print("四、一次性：第二次遍历是空的（这是最常见的 bug 来源）")
print(SEP)
gen = (x for x in [1, 2, 3])
show("第一次 list(gen)", list(gen))
show("第二次 list(gen)", list(gen))
mp = map(str.upper, ["a", "b"])
show("map 第一次 list(mp)", list(mp))
show("map 第二次 list(mp)", list(mp))
print("  再直接 next(mp)：", end="")
try:
    next(mp)
    print("居然给值了（不应该发生）")
except StopIteration:
    print("抛 StopIteration —— 迭代器已经耗尽，不会倒带")
print("  列表呢：")
lst = [1, 2, 3]
show("  list(lst) 第一次", list(lst))
show("  list(lst) 第二次", list(lst))
print("  生成器对象自己呢（同一个 for 里遍历两次）：")
gen2 = (x for x in [1, 2, 3])
for i, v in enumerate(gen2):
    pass
show(f"  遍历完（停在 {v}）之后再生成", list(gen2))
print("→ 记法：**列表是「数据」，可以反复读；生成器/map/filter 是「生产进度」，读完就没了。**")
print("  所以函数返回 `(x for x in ...)` 时，调用方必须一次用完；")
print("  如果你自己都想要第二次，返回 `list(...)` 或者用 `itertools.tee`（它会吃掉内存）。")
print("  踩坑的典型形状：")
data = {"a": [1, 2], "b": [3]}
flat = (x for v in data.values() for x in v)
show("  打印一次", list(flat))
show("  第二次想再算 sum", sum(flat))
print("  ↑ 这里 sum 是 0，不是报错。数据没了但不告诉你 —— 比报错更难查。")

print(SEP)
print("五、真实计时：sum 一百万个数，四种写法")
print(SEP)
N = 1_000_000


def best(stmt, glb, repeat=5):
    return min(timeit.repeat(stmt, globals=glb, number=1, repeat=repeat))


glb = {"N": N}
rows = [
    ("sum(x for x in range(N) if x % 3)", "生成器表达式"),
    ("sum([x for x in range(N) if x % 3])", "列表推导式（先造整份）"),
    ("sum(map(lambda x: x, filter(lambda x: x % 3, range(N))))", "map + filter"),
]
results = []
for code, label in rows:
    t = best(code, glb)
    results.append((label, code, t))
    show(f"  {label}", f"{t * 1000:>7.1f} ms   ← {code}")
print("→ 结果和「惰性一定慢」的直觉相反：生成器比列表推导式**快**。")
print("  因为 `sum(列表)` 那一步要多干两件事：先给整份列表租下槽位数组")
big = [x for x in range(N)]
show(f"    本机 N={N} 时这份列表的 sys.getsizeof", f"{sys.getsizeof(big)} 字节")
del big
print("  然后把每个元素塞进去、算完再把整份释放掉。生成器这些全免，")
print("  而 sum 本来就是一次收一个数（它不需要随机访问）。")
print("  什么时候生成器会**慢**：结果要用两次的时候 —— 你得把整条流水线重跑一遍。")
print("  什么时候列表更好：元素少、要下标访问、要 len、要复用。")
print()
print("  同一份数据量一下峰值内存（tracemalloc，单位换算成 MiB / KiB 好看）：")


def peak_of(expr):
    tracemalloc.start()
    obj = eval(expr)
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    del obj
    return peak


pk_list = peak_of("[x for x in range(1_000_000)]")
pk_gen = peak_of("(x for x in range(1_000_000))")
show("  [x for x in range(1_000_000)] 峰值", f"{pk_list / 1024 / 1024:.1f} MiB")
show("  (x for x in range(1_000_000)) 峰值", f"{pk_gen / 1024:.0f} KiB")
print(f"→ 差了 {pk_list // max(pk_gen, 1)} 倍。这就是「处理一亿行日志」时")
print("  生成器/逐行读 和 「先 list 起来」 的分水岭（逐行读文件是第 08 章的事）。")

print(SEP)
print("六、map 的代价：它要的是「函数」，多一个 lambda 就多一层调用")
print(SEP)
words = [f"w{i}" for i in range(200_000)]
glb2 = {"words": words, "map": map, "str": str}
cands = [
    ("list(map(str.upper, words))", "内置方法引用（最快）"),
    ("[w.upper() for w in words]", "列表推导式（同样方法引用不了）"),
    ("list(map(lambda w: w.upper(), words))", "map + lambda（多一层 lambda 调用）"),
]
for code, label in cands:
    t = best(code, {**glb2, "list": list}, repeat=3)
    show(f"  {label}", f"{t * 1000:>6.1f} ms")
print("→ 三行结果完全一样（本机验过：见下一行），只有速度不一样。")
one = list(map(str.upper, words))
two = [w.upper() for w in words]
three = list(map(lambda w: w.upper(), words))
show("  三份结果相等吗", one == two == three)
print("→ 读法：map 赢在「函数已经是一个现成的对象」（`str.upper` 是 slot wrapper，")
print("  第 02 章第 14 节量过这一类东西）；一旦要塞 lambda，它就退化成")
print("  「每个元素一次 Python 函数调用」，比推导式还慢。")
print("  实操判据（这是本机量出来的，不是规矩）：能直接写方法名/内置函数才用 map，")
print("  需要写表达式就用推导式 —— 推导式还能同时改键和过滤，map 做不到过滤那一步。")
