#!/usr/bin/env python3
# ============================================================================
# 这个文件在演什么：排序类操作（sort / sorted / min / max）的三个高频错法。
# 为什么演：
#   * `x = lst.sort()` 之后 x 是 None —— 这是「原地改 vs 造新对象」这条主线
#     （第 02 章第 5 节那个 `+=`）在容器方法上的具体形状：改完不返回东西，因为你手上已经有它了。
#   * sort 和 sorted 的差别不是「一个快一个慢」，而是**有没有换对象**：
#     tuple 只能 sorted（它没有 sort 方法），dict 的 keys 排序也只能 sorted。
#   * 稳定性、比较规则、混类型报错原文，都是你在 10-sorting 那章亲手写过的东西，
#     这里量的是「Python 给你哪一个」。
#
# 只用标准库（sys、timeit）。
# ============================================================================
import sys
import timeit

SEP = "=" * 76

print(SEP)
print("一、错法 1：`x = lst.sort()` —— x 是 None，list 自己才是结果")
print(SEP)
xs = [3, 1, 2]
ys = xs.sort()
print(f"  ys = xs.sort() 之后：ys = {ys!r}，xs = {xs}")
print(f"  xs.sort() 的返回值类型：{type(ys).__name__}")
try:
    ys.append(4)
except AttributeError as exc:
    print(f"  接着写 ys.append(4) → AttributeError: {exc}")
print("→ 对法：想要排好序的那个东西，就用**原来的名字** xs；")
print("  想要新对象才写 `ys = sorted(xs)`。空 list 也一样：`[].sort()` 还是返回 None。")
empty = []
print(f"  验证：([].sort() 的返回值) → {[].sort()!r}，而 empty 本身 = {empty}")

print()
print(SEP)
print("二、sort vs sorted：谁换对象，谁能用在谁身上")
print(SEP)
src = [5, 2, 9, 1]
before_id = id(src)
r = src.sort()
print(f"  src.sort()：src 的 id 变了吗 → {id(src) == before_id}"
      f"（同一个对象被原地重排），返回值 = {r!r}")
src2 = [5, 2, 9, 1]
out = sorted(src2)
print(f"  sorted(src2)：返回的是新对象吗 → {out is not src2}，"
      f"src2 保持原样 = {src2}，out = {out}")
print(f"  外壳尺寸对照：src2 = {sys.getsizeof(src2)} 字节，out = {sys.getsizeof(out)} 字节"
      f" → sorted 期间同时挂着两份外壳")
try:
    (3, 1, 2).sort()
except AttributeError as exc:
    print(f"  (3, 1, 2).sort() → AttributeError: {exc}")
print(f"  tuple 只能这样：sorted((3, 1, 2)) = {sorted((3, 1, 2))}"
      f"（返回的是 list，不是 tuple！）type = {type(sorted((3, 1, 2))).__name__}")
print("→ 三条规矩：")
print("  ① list.sort() 原地、返回 None；sorted(x) 造新 list、任何可迭代对象都能喂。")
print("  ② 想要 tuple 版的排序结果：`tuple(sorted(x))`，sorted 自己永远给你 list。")
print("  ③ 内存账：sort 不额外要一份外壳；sorted 要（所以大列表上 sort 更省）。")

print()
print(SEP)
print("三、比较规则：默认按元素比（跟 strcmp 同源），key 才是你的比较函数")
print(SEP)
print(f"  sorted([[1, 2], [1], [0, 9]]) = {sorted([[1, 2], [1], [0, 9]])}")
print("  ↑ 先比第 0 个元素，平了再比第 1 个；[1] 比 [1, 2] 短 → 短的在前（和你比字符串一样）。")
words = ["Banana", "apple", "Cherry"]
print(f"  默认按码位比：sorted({words}) = {sorted(words)}   ← 大写全在小写前面")
print(f"  key=str.lower：sorted(words, key=str.lower) = {sorted(words, key=str.lower)}")
print(f"  reverse=True ：sorted(words, key=str.lower, reverse=True) = "
      f"{sorted(words, key=str.lower, reverse=True)}")
try:
    sorted([1, "甲"])
except TypeError as exc:
    print(f"  混类型排序直接报错 → TypeError: {exc}")
print("→ key= 接的是「一个参数、返回用来比的东西」的函数（等价于你 C 里传给 qsort 的")
print("  cmp 函数的降级版：Python 只让你提供「取比较键」，不提供两两比较，")
print("  所以每个元素的 key 只算一次 —— 这正是它比自写 cmp 快的原因。")
print("  真需要两两比较函数：functools.cmp_to_key(...) 能包一层，本章没演（见「没演」清单）。")

print()
print(SEP)
print("四、稳定性：相等元素保持原相对顺序（Timsort 给的保证，实测）")
print(SEP)
rows = [("甲组", 3), ("乙组", 1), ("甲组", 2), ("乙组", 1), ("甲组", 3)]
by_score = sorted(rows, key=lambda r: r[1])
print(f"  原表：{rows}")
print(f"  只按分数排：{by_score}")
print("  ↑ 分数相同的『甲组3 / 甲组3』和『乙组1 / 乙组1』，先后顺序和原表一致 —— 这叫稳定。")
print("  实用推论：想要「先按组、再按分数」，只要按分排一次、再按组排一次（次要的键先排）：")
step1 = sorted(rows, key=lambda r: r[1])
step2 = sorted(step1, key=lambda r: r[0])
print(f"    两次稳定排序 = {step2}")
print(f"    一次到位（元组键） = {sorted(rows, key=lambda r: (r[0], r[1]))}")
print("  两个结果一模一样 —— 稳定是这两条路能等价的前提。")

print()
print(SEP)
print("五、dict 排序：sorted(d) 给你键，想按值排要 key=d.get")
print(SEP)
score = {"数学": 91, "语文": 88, "英语": 95}
print(f"  sorted(score)        = {sorted(score)}          ← 只有键，值一个没带")
print(f"  sorted(score.items()) = {sorted(score.items())}")
print(f"  sorted(score, key=score.get) = {sorted(score, key=score.get)}   ← 按值排键")
print(f"  要「值→键」的完整表：{sorted(score.items(), key=lambda kv: kv[1], reverse=True)}")
print("→ `sorted(d)` 迭代的是键（`for x in d` 迭代的就是键；"
      "键的遍历顺序 = 插入顺序，见 dict_key_pitfalls.py 第四节）。")
print("  按值排序的两种写法：key=d.get（简洁）/ key=lambda kv: kv[1]（配合 items()）。")

print()
print(SEP)
print("六、顺手把 min/max/sum 的复杂度定位一下（它们都是 O(n) 全扫）")
print(SEP)
import random

n = 100_000
ordered = list(range(n))
shuffled = list(range(n))
random.Random(42).shuffle(shuffled)     # 固定种子，你重跑也是同一个顺序
print("  两份数据：range(100000) 原样（已经有序）/ 同一份用种子 42 打乱")
timed = {}
cases = (
    ("max(shuffled)", "mx", "max(data)", 200, {"data": shuffled}),
    ("min(shuffled)", "mn", "min(data)", 200, {"data": shuffled}),
    ("sum(shuffled)", "sm", "sum(data)", 200, {"data": shuffled}),
    ("sorted(shuffled)", "so", "sorted(data)", 5, {"data": shuffled}),
    ("sorted(ordered)", "sp", "sorted(data)", 5, {"data": ordered}),
)
for label, key, stmt, num, g in cases:
    best = min(timeit.repeat(stmt, globals=g, number=num, repeat=3))
    timed[key] = best / num * 1e6
    print(f"  {label:<20} = {timed[key]:>9.2f} µs/次")
print("→ max / min / sum 都是一遍扫描（O(n)，常数小，全在 C 里跑）。")
print(f"  『为了取最值先把整个表排序』在打乱的数据上是 max 的 "
      f"{timed['so'] / timed['mx']:.0f} 倍 —— 同一件事，别用排序代替 max()。")
print(f"  意外的一条：**同样 10 万个数，已经有序那份排序只花 "
      f"{timed['sp']:.0f} µs，打乱那份要 {timed['so']:.0f} µs"
      f"（差 {timed['so'] / timed['sp']:.0f} 倍）。")
print("  这不是 bug，是 Timsort 的本事：它先扫一遍找「已经排好的段」（run），")
print("  全有序的数据一遍就承认，于是 O(n log n) 退化成 O(n)。")
print("  所以『排序很快』这种结论必须写清数据长什么样 —— 拿 range(n) 测排序，")
print("  测的是最坏情况还是最好情况，你自己都没数（10-sorting 那章的同一条纪律）。")
