#!/usr/bin/env python3
# ============================================================================
# 这个文件在演什么：把「成员运算 `x in c`」在 list / tuple / dict / set 上的
#                   真实耗时量出来（n = 1e3 / 1e4 / 1e5），并拆开 O(1) 的构成。
# 为什么演：
#   * 复杂度表上写 `in`：list O(n)、set O(1)。这句话你背得出来，但没量过，
#     就永远不知道 O(1) 那格有多平、O(n) 那格有多陡。
#   * 更重要的是把它接回你手写的结构：
#       list/tuple 的 in = 你在 01-array/static-array 里写的「从头比到尾」；
#       dict/set  的 in = 你在 08-hash/hash-table-* 里写的「h(key) 投桶 + 比对」。
#     测出来的形状（一条斜线 vs 一条水平线）就是那两段代码的形状。
#   * 表一用 **测不到的那个值**（target = n + 7）来测，也就是最坏情况：list 必须
#     把整个数组比完才敢说「不在」。测「命中」会因命中位置早晚而抖，测「落空」干净。
#
# 计时口径（重要，否则数字会被误读）：
#   * 每一格都跑 timeit.repeat(..., repeat=3) 取 **三次里的最小值**：
#     最小值 ≈ 没被系统打断的那一轮，比平均值稳；
#   * 次数 num 按 n 反比缩放，保证每格总耗时都在十几毫秒；
#   * 绝对值会随机器负载抖几微秒，**但「×10 规模 → ×10 耗时」和「100 倍规模不动」
#     这两个形状不抖**，那才是本节的证据。别拿本节的小数位当 KPI。
# 只用标准库（timeit）。整份脚本跑完不到 2 秒。
# ============================================================================
import timeit

SEP = "=" * 78
NS = (1_000, 10_000, 100_000)


def bench(container, target, n, stmt="target not in container"):
    """一条成员判断的平均耗时（µs/次），三次计时取最小。"""
    num = max(5, 2_000_000 // n)
    best = min(timeit.repeat(stmt, globals={"container": container, "target": target},
                             number=num, repeat=3))
    return best / num * 1e6


print(SEP)
print("表一：`x in c`，n 变大 10 倍，谁跟着变大 10 倍，谁一动不动")
print(SEP)
print("      n        list µs/次    tuple µs/次    dict µs/次    set µs/次")
rows = {}
for n in NS:
    xs = list(range(n))
    ts = tuple(xs)
    d = dict.fromkeys(range(n))       # 只有键，值无所谓：in 只看键
    s = set(xs)
    miss = n + 7                      # 保证不在里面 → 逼出最坏情况
    r = {name: bench(cont, miss, n) for name, cont in
         (("list", xs), ("tuple", ts), ("dict", d), ("set", s))}
    rows[n] = r
    print(f"{n:>8}   {r['list']:>12.2f} {r['tuple']:>13.2f} {r['dict']:>12.3f} {r['set']:>11.3f}")
ratio_l = rows[10_000]["list"] / rows[1_000]["list"]
ratio_t = rows[10_000]["tuple"] / rows[1_000]["tuple"]
print("→ 形状就是结论：")
print(f"  list / tuple：规模×10，耗时分别×{ratio_l:.1f} 和 ×{ratio_t:.1f} —— 斜率≈1 的直线 = O(n)。")
print("                两条线在同一个量级上互相咬住（谁是底层都是指针数组，差别只在迭代器实现，")
print("                为什么略有先后本章没演，不拿它当结论）。")
print(f"  dict / set：规模涨 100 倍（1000 → 100000），耗时从 {rows[1000]['set']:.3f} µs "
      f"爬到 {rows[100000]['set']:.3f} µs，")
print("                涨的是缓存命中率这种物理成本，不是算法轮次 —— 形状就是 O(1)。")
print("  倍数账："
      f"n=100000 时 list 的 in 比 set 慢 {rows[100000]['list'] / rows[100000]['set']:.0f} 倍；"
      f"n=1000 时慢 {rows[1000]['list'] / rows[1000]['set']:.0f} 倍。")
print("  ★ 这条倍数就是你在 08-hash 那节花力气写哈希表的理由 —— Python 已经把它做成")
print("    内置件了。`if x in seen:` 里的 seen 用 list 还是 set，差的不是常数，是量级。")

print()
print(SEP)
print("表二：命中位置对 list 的影响（对 set 则完全没有）")
print(SEP)
n = 100_000
xs = list(range(n))
s = set(xs)
for where, target in (("队头第 0 个", 0), ("正中间", n // 2),
                      ("最后一个", n - 1), ("根本不存在", n + 7)):
    tl = bench(xs, target, n, stmt="target in container")
    ts = bench(s, target, n, stmt="target in container")
    print(f"  {where}  target={target}  →  list {tl:.2f} µs/次   set {ts:.3f} µs/次")
print("→ list 那一列是一条上升的台阶：命中队头几乎免费；正中间 ≈ 最后一个的一半；")
print("  「根本不存在」比「命中最后一个」还稍贵一点 —— 它要把整个数组比完，")
print("  最后一个都不匹配，才能回答 False。")
print("  set 那一列四个位置全是同一个数：它不知道也不关心元素在哪，只看哈希。")

print()
print(SEP)
print("表三：规模再大也平的第三次确认 + 一次哈希大概多少时间")
print(SEP)
num = 500_000
lookup = {}
for n in (1_000, 100_000, 1_000_000):
    d = dict.fromkeys(range(n))
    best = min(timeit.repeat("target in d", globals={"d": d, "target": n // 3},
                             number=num, repeat=3))
    lookup[n] = best / num * 1e9
    print(f"  dict 有 {n:>8} 个键时，一次 in = {lookup[n]:>6.1f} ns")
best_hash = min(timeit.repeat("hash(123456)", number=num, repeat=3))
print(f"  在 Python 层面写一次 hash(123456) = {best_hash / num * 1e9:.1f} ns")
print(f"  → 一次 dict 查询（{lookup[1000]:.1f} ns）和一次 hash() 调用（{best_hash / num * 1e9:.1f} ns）同量级：")
print("    「查询的时间基本就是算哈希的时间」，桶数从 1 千涨到 100 万不改变它。")
print("    （两个数字都还含着 timeit 循环体本身那点开销，所以只能比量级，别比小数位。）")

print()
print(SEP)
print("表四：字符串键的真实代价 —— 哈希缓存能救同一个对象，救不了每个新对象")
print(SEP)
same = "x" * 100_000
best_same = min(timeit.repeat("hash(k)", globals={"k": same}, number=num, repeat=3))
print(f"  对**同一个** 10 万字符的 str 反复 hash：{best_same / num * 1e9:.1f} ns/次")
per_len = {}
for length in (10, 1_000, 100_000):
    rep = max(2_000, 1_000_000 // length)
    # 用变量 L 而不是字面量：'x' * 1000 这种写法会被编译期折成一个常量对象，
    # 于是每次拿到的都是同一个串，哈希缓存会把测量结果整个污染掉
    # （所以每条计时都重新造串，测到的才是「新键」的真实成本）。
    g = {"L": length}
    t_build = min(timeit.repeat("'x' * L", globals=g, number=rep, repeat=3))
    t_both = min(timeit.repeat("hash('x' * L)", globals=g, number=rep, repeat=3))
    per_len[length] = (t_both - t_build) / rep * 1e9
    print(f"  每次 hash 一个**新造**的 {length} 字符 str：约 {per_len[length]:.1f} ns/次"
          f"（两次计时相减，把造串的开销剔掉）")
print(f"  → 长度 10 → 1000 → 100000（×100 再 ×100），成本 "
      f"{per_len[10]:.0f} → {per_len[1000]:.0f} → {per_len[100000]:.0f} ns：跟着长度线性涨，")
print("    因为哈希必须把每个字节读一遍。**算哈希本身是 O(len(key))，O(1) 说的是「不看第几个元素」，")
print("    不是「免费」。**")
print("  → 同一个对象第二次 hash 只要几十 ns（第一格），因为 CPython 把算好的哈希存在 str")
print("    对象里；内容相同但对象不同，就要重算。")
print("  实操推论：拿长字符串当键时，别在循环里每次现场拼一个新 key 去查 ——")
print("  `d[prefix + str(i)]` 每轮都要重算整串哈希；把 key 对象留着复用，")
print("  或者改用整数/元组键（`(a, b)` 的哈希只看各元素的哈希，成本恒定）。")

print()
print(SEP)
print("表五：dict 的 in 只看键；查值立刻退回 O(n)")
print(SEP)
n = 100_000
num = max(5, 2_000_000 // n)
d = {f"k{i}": i for i in range(n)}
t_key = min(timeit.repeat("target in d", globals={"d": d, "target": "k50000"},
                          number=num, repeat=3))
t_val = min(timeit.repeat("target in d.values()", globals={"d": d, "target": 50_000},
                          number=num, repeat=3))
print(f"  'k50000' in d        = {t_key / num * 1e6:.3f} µs/次   ← 键：走哈希")
print(f"  50000 in d.values()  = {t_val / num * 1e6:.2f} µs/次   ← 值：挨个比")
print(f"  倍数：{t_val / t_key:.0f} 倍")
print("→ 哈希索引只建在键上。values() 是个「视图」，遍历它 = 走一遍整张表，O(n)。")
print("  需要反查值就自己再建一个 dict（值→键），别在循环里写 `in d.values()` ——")
print("  这一条是「把 O(n) 藏进循环」最常见的现场（你在 08-hash 里可没给值建索引）。")
