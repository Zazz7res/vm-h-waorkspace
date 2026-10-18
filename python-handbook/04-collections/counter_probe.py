#!/usr/bin/env python3
# ============================================================================
# 这个文件在演什么：collections.Counter 的真实语义 —— 尤其是「加减与或」这四种
#                   运算符各自到底算了什么，和缺失键为什么不报 KeyError。
# 为什么演：
#   * Counter 是 dict 的子类，但它**改了 dict 的算术含义**：
#       c[缺失键] 在 dict 上是 KeyError，在 Counter 上是 0；
#       c1 + c2 在 dict 上是 TypeError，在 Counter 上是「计数相加」；
#       c1 - c2 是「相减并把 ≤0 的丢掉」，不是「差集」；
#       c1 & c2 取 min，c1 | c2 取 max，都不是集合运算。
#     每一条都在这儿跑出真结果，不背定义。
#   * 「缺失键返回 0」这件事还牵出一个副作用问题：有的实现会**顺手把键建出来**，
#     那样就会改变 len、进而触发「遍历时改大小」。本机到底会不会建键？量给你看。
#     （同一段还演出 defaultdict 那份**真的会建键**的对照，两者只差这一件事。）
#   * 你手写哈希表时，「统计词频」是最后那一步（插入 + 命中就 ++）。Counter 就是
#     那一段代码的名字版；`most_common` 则是「统计完再排一次序」，10-sorting 里你写过。
#
# 只用标准库（collections.Counter / defaultdict、inspect、sys、timeit）。
# ============================================================================
import inspect
import sys
from collections import Counter, defaultdict

SEP = "=" * 76


def show(tag, obj):
    print(f"  {tag:<40} {obj}")


print(SEP)
print("一、怎么造、造出来是什么")
print(SEP)
show("Counter('abracadabra')", Counter("abracadabra"))
show("Counter(['a', 'b', 'a'])", Counter(["a", "b", "a"]))
show("Counter(a=3, b=1)（关键字）", Counter(a=3, b=1))
show("Counter({'a': 3, 'b': 1})（映射）", Counter({"a": 3, "b": 1}))
c0 = Counter()
c0.update("aab")
show("先建空的、再 update('aab')", c0)
print("→ 四种入口，前三种是「数东西」，第四种（映射）是「直接给计数」。")
show("Counter 是 dict 的子类吗", isinstance(Counter(), dict))
show("所以它能干 dict 的一切", dict(Counter("aab")))
show("len（不同键的个数）", len(Counter("aab")))
show("总数（要自己加）", sum(Counter("aab").values()))
print("→ len(Counter) 是**键的个数**不是元素总个数，这一条写错的人会卡在断言上：")
show("  例：len(Counter('aaa'))", len(Counter("aaa")))
show("  sum(Counter('aaa').values())", sum(Counter("aaa").values()))

print(SEP)
print("二、缺失键：Counter 给 0，dict 给 KeyError（对照跑）")
print(SEP)
c = Counter("ab")
show("c['zz'] =", c["zz"])
show("c.get('zz') =", c.get("zz"))
d = dict(c)
try:
    show("dict(d)['zz'] =", d["zz"])
except KeyError as e:
    print(f"  dict 走 `[]` 取缺失键 → KeyError: {e}")
print("→ 上一行的 KeyError 是**故意**取缺失键拿到的报错原文（`'zz'`，带引号，")
print("  因为 KeyError 的参数是那个键对象本身，str() 会把它再 repr 一次）。")
print("  关键问题来了：Counter 返回 0 的时候，有没有顺手把 'zz' 建进表里？")
show("  访问前 len(c)", len(c))
show("  访问 c['zz'] 之后 len(c)", len(c))
show("  'zz' in c", "zz" in c)
show("  dict(c)", dict(c))
print("→ 本机 3.14.4：**不会建键**。len 没变、in 还是 False。")
print("  这个结论可以直接从源码读出来（Counter 是纯 Python，能 inspect）：")
src_lines = inspect.getsource(Counter.__missing__).rstrip().splitlines()
for line in src_lines:
    print("    " + line.strip())
print("  → 函数体就一行 `return 0`，没有任何 self[key] = ... 的写入。")
print("  也就是说 Counter 缺键不会污染表，也就**不会**触发 mutate_while_iterating.py")
print("  第五节那条 `dictionary changed size during iteration`。这一条很重要：")
c2 = Counter("ab")
for k in "abcxyz":
    print(f"    遍历中读 c2[{k!r}] = {c2[k]}   len(c2) = {len(c2)}")
print("    ↑ 边遍历边读缺失键，全程没报错，len 一直是", len(c2))
print("  对照 defaultdict：同样的动作会把键**造出来**（本机现场，不是推测）：")
dd = defaultdict(int)
show("    造出来时 len(dd)", len(dd))
for k in "abcxyz":
    _ = dd[k]                 # 只是「读」一下缺失键
show("    读了一圈缺失键之后 len(dd)", len(dd))
show("    表已经变成", dict(dd))
print("    ↑ 读一下就建键 —— 于是「边遍历边读缺失键」在 defaultdict 上会撞上")
print("      mutate_while_iterating.py 第五节那条报错：")
dd2 = defaultdict(int)
dd2["a"] = 1
try:
    for k in dd2:
        dd2[k + "_new"] += 1
except RuntimeError as exc:
    print(f"      RuntimeError: {exc}")
print("    → 同一行代码：Counter 什么也不发生（上面六格），defaultdict 直接把表改脏并报错。")
print("      所以「只是查一下计数」用 Counter，别用 defaultdict 当只读计数器。")

print(SEP)
print("三、四种运算符：+ - & | 各算什么（全部本机跑出来）")
print(SEP)
a = Counter(a=3, b=1)
b = Counter(b=2, c=4)
show("a", dict(a))
show("b", dict(b))
show("a + b（对应键相加，全留下）", dict(a + b))
show("a - b（相减，结果 ≤0 的键被丢掉）", dict(a - b))
show("a & b（取交集 = 每个键取 min）", dict(a & b))
show("a | b（取并集 = 每个键取 max）", dict(a | b))
print("→ 逐条对上：a 有 b=1、b 有 b=2 → a+b 里 b=3；a-b 里 b=1-2=-1 ≤0 所以**整个键消失**；")
print("  a&b 只在两边都有的键上算（这里只有 b），取小的 1；a|b 把所有键并起来，每个取大的。")
print("  注意 a-b **不是**「a 里有而 b 里没有的键」那种集合差 —— 它保留了 a=3。")
print("  想要「键集合的差」，得自己写：")
show("  键集合差 set(a) - set(b)", sorted(set(a) - set(b)))
show("  用推导式表达「只保留 a 有 b 没有的计数」", {k: a[k] for k in set(a) - set(b)})
print("→ 还有一个更阴的点：a-b 丢的是「结果 ≤0」的键，不是「负数」的键。")
print("  也就是说 a 和 b 同一个键计数相等时，减完这个键直接消失（0 不算保留）。验证：")
show("  Counter(x=5) - Counter(x=5)", dict(Counter(x=5) - Counter(x=5)))
print("  而 `subtract()` 是另一种语义：它**保留**负数，专门用来做「消耗扣减」：")
stock = Counter(a=1, b=5)
stock.subtract(Counter(a=3, b=1))
show("  stock.subtract(...) 之后", dict(stock))
print("  → 上面 a 变成 -2 并且**留在表里**。要「只关心剩余」就再用一次 `+ Counter()` 归一：")
show("    归一：stock + Counter()", dict(stock + Counter()))
print("  （`+ Counter()` 这招能成立，正因为加法会把 ≤0 的丢掉 —— 空 Counter 什么也不加。）")

print(SEP)
print("四、most_common / elements：排序和展开")
print(SEP)
c = Counter("hamburger")
show("c", dict(c))
show("most_common()（全部，按计数降序）", c.most_common())
show("most_common(3)（前 3）", c.most_common(3))
show("most_common(0)", c.most_common(0))
show("most_common(-1)", c.most_common(-1))
print("→ 参数是「前 N 个」，0 给空列表，负数也给空列表（本机量出来的两格）。")
print("  相同计数之间谁先谁后？本机这一份：")
show("  Counter('aabbc').most_common()", Counter("aabbc").most_common())
print("  a 和 b 都是 2，打出来是 ('a',2) 在前 —— 这是**插入顺序**（'aabbc' 里 a 先出现）。")
print("  语言规范**没有承诺**同计数的顺序，所以这是观察值、不能当规则写进代码里；")
print("  真需要确定的顺序就自己排：")
cc = Counter("aabbc")
show("  按 (-计数, 键) 排", sorted(cc.items(), key=lambda kv: (-kv[1], kv[0])))
print("  → 这一行就是你 10-sorting 里写的「先按频次、频次相同按字典序」，Python 一句话。")
print("  elements() 是反过来：把计数**展开**成可迭代的东西。")
show("  Counter(a=2,b=3).elements()", list(Counter(a=2, b=3).elements()))
show("  计数为 0 / 负数的键", list(Counter({"x": 0, "y": -1, "z": 2}).elements()))
print("  → 0 和负数直接不产出元素，不报错。它是惰性的（返回 iterator），")
print("    所以 list() 不套的话你只会看到一个 <itertools.chain> 对象。")
show("  未套 list 的 elements()", Counter(a=1).elements())
print("  → 和第四节的 genexpr 同一条规矩：iterator 读一次就没了，见 genexpr_vs_map.py 第四节。")

print(SEP)
print("五、Counter 的真实代价：它值不值得用")
print(SEP)
import timeit

words = [f"w{i % 700}" for i in range(200_000)]
MANUAL_GET = "d = {}\nfor w in words:\n    d[w] = d.get(w, 0) + 1\n"
MANUAL_SD = "d = {}\nfor w in words:\n    d.setdefault(w, 0)\n    d[w] += 1\n"
glb = {"words": words, "Counter": Counter}


def best(stmt, repeat=3):
    return min(timeit.repeat(stmt, globals=glb, number=1, repeat=repeat))


t_c = best("Counter(words)")
t_m = best(MANUAL_GET)
t_s = best(MANUAL_SD)
show("Counter(words)", f"{t_c * 1000:>6.1f} ms")
show("手写 dict + get(w, 0) + 1", f"{t_m * 1000:>6.1f} ms")
show("手写 dict + setdefault", f"{t_s * 1000:>6.1f} ms")
same = Counter(words)
dchk = {}
for w in words:
    dchk[w] = dchk.get(w, 0) + 1
show("结果和手写一致吗", same == dchk)
print(f"→ Counter 比手写快约 {t_m / t_c:.1f} 倍（本机这一刻的比值，别当常数用）。")
print("  原因：Counter 的更新循环在 C 层（_count_elements），你的 for 每次都要过一遍解释器。")
print("  尺寸顺手量一下：")
show("  sys.getsizeof(Counter(words))", sys.getsizeof(Counter(words)))
show("  sys.getsizeof(同样键的 dict)", sys.getsizeof(dchk))
print("  → 一样大（Counter 没多带字段，它只是 dict 加了方法）。")
