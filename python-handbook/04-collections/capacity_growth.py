#!/usr/bin/env python3
# ============================================================================
# 这个文件在演什么：把 list 的「容量增长序列」一格一格量出来，
#                   和你 01-array/dynamic-array 手写的倍增策略逐条对照。
# 为什么演：
#   * 你在 C 版动态数组里定的是 grow_multiplier = 2：容量 4 → 8 → 16 → 32 → 64，
#     每次 realloc 把老数据整段抄过去。这一节量出 Python 的真实台阶，
#     你会发现它**前半段像倍增、后半段根本不是倍增**（只多约 1/8）。
#   * 「append 是 O(1)」这句话的完整版本是「**均摊** O(1)」：偶尔一次 O(n) 搬家，
#     摊到每次 append 上。这里不靠嘴说，直接把「一共抄了多少个指针」数出来。
#   * 还有你没在 C 版里写过的方向：容量也会**缩**。pop 到什么时候缩、缩到哪，
#     量给你看。
#
# 只用标准库（sys）。每段前面写「量什么」，后面用 → 写结论。
# ============================================================================
import sys

SEP = "=" * 70


def slots(lst):
    """把 getsizeof 换算成『租了几个槽』。
    list 对象本体 40 字节 + GC 头 16 字节 = 56（这是空 list 的量到的 56），
    之后每个槽是一根指针 = 8 字节。所以 (尺寸 - 56) / 8 = 槽数。
    空 list 特殊：它一个槽都没租（尺寸正好 56）。"""
    return (sys.getsizeof(lst) - 56) // 8


print(SEP)
print("一、连续 append 1000 次，只在『容量真的变了』那一刻打一行")
print(SEP)
a = []
prev_slots = slots(a)
events = []
for n in range(1, 1001):
    a.append(n)
    cur = slots(a)
    if cur != prev_slots:
        events.append((n, cur, prev_slots))
        prev_slots = cur
print(f"  {'装到第几个元素':<16} {'新槽数':>8} {'倍数(新/旧)':>14}")
for n, cur, old in events:
    ratio = f"{cur / old:.3f}" if old else "第一次租"
    print(f"  {n:<18} {cur:>7} {ratio:>13}")
print(f"  一共扩容 {len(events)} 次才装到 1000 个元素")
print("→ 真实台阶：4 → 8 → 16 → 24 → 32 → 40 → 52 → 64 → 76 → 92 → …")
print("  只有头三档（4、8、16）像你说的『倍增』，后面是 24、32、40 这种 +8 的步子，")
print("  再往后步子逐渐变大，但每次倍数只有 1.1 左右 —— **Python 的 list 不翻倍**。")
print("  网上那句『list 容量按 2 的幂增长』在这台 CPython 3.14 上，第 4 档就不成立了。")

print()
print(SEP)
print("二、和你手写的 C 动态数组逐条对照（01-array/dynamic-array）")
print(SEP)


def c_double_ladder(target):
    """模拟你 C 版的策略：容量 0 → 4 → 8 → 16 …（grow_multiplier=2, increment=0）
    返回 (扩容次数, 装到 target 个元素时的最终容量, 累计复制的指针数)。"""
    cap = 0
    grows = 0
    copies = 0
    for n in range(1, target + 1):
        if n > cap:
            copies += cap              # realloc 前，老的 cap 个元素要整段抄过去
            cap = 4 if cap == 0 else cap * 2
            grows += 1
    return grows, cap, copies


def py_ladder(target):
    """用真 list 量：append 到 target 个，返回 (扩容次数, 最终槽数, 累计复制数)。"""
    xs = []
    grows = 0
    copies = 0
    prev = 0
    for n in range(1, target + 1):
        xs.append(n)
        cur = slots(xs)
        if cur != prev:
            copies += n - 1
            grows += 1
            prev = cur
    return grows, cur, copies


print("  C 版（倍增，MIN_CAPACITY=4）的台阶：1→4  5→8  9→16  17→32  33→64  65→128 …")
print("  Python 台阶见上面第一节全表。两种策略各算一笔账：")
print(f"  {'元素数':>7} {'C 扩容次数':>10} {'C 最终容量':>10} {'C 浪费':>8} "
      f"{'py 扩容次数':>10} {'py 最终槽数':>10} {'py 浪费':>8}")
for target in (5, 17, 65, 129, 513, 1000):
    cg, cc, _ = c_double_ladder(target)
    pg, pc, _ = py_ladder(target)
    print(f"  {target:>7} {cg:>10} {cc:>10} {cc / target:>7.2f}x"
          f" {pg:>10} {pc:>10} {pc / target:>7.2f}x")
print("→ 三种账一起看：")
print("  · 扩容次数：倍增少（1000 个元素 9 次 vs Python 28 次）；")
print("  · 空间浪费：倍增的浪费永远在 1.00x ～ 2.00x 之间振荡 —— 513 个元素要租 1024 槽")
print("    （2.00x），1024 个元素刚好占满（1.00x）；Python 在小规模也振荡")
print("    （9 个元素租 16 槽 = 1.78x），但**规模越大越收敛到 1.13x**：")
print("    看第一节那张表的倍数列，从 2.000 一路降到 1.132，越往后越稳。")
print("  · 共同点：**都是『一次 O(n) 搬家换 O(1) 追加』**，所以你在 C 那节写的摊还")
print("    分析可以原样搬过来 —— 只是把『2 的幂』换成『实测台阶』。")

print()
print(SEP)
print("三、把『一共抄了多少个指针』真的数出来（摊还成本的账本）")
print(SEP)
pg, _, total_copies = py_ladder(1000)
cg, _, c_copies = c_double_ladder(1000)
print(f"  Python 路线：append 1000 个元素，累计复制指针 = {total_copies} 个"
      f"（扩容 {pg} 次）")
print(f"                平均每次 append 复制 {total_copies / 1000:.2f} 个")
print(f"  你 C 版（纯倍增）路线：累计复制 = {c_copies} 个，平均每次 {c_copies / 1000:.2f} 个"
      f"（扩容 {cg} 次）")
print(f"  最坏对照（每加一个都重alloc 搬家，等于硬扩静态数组）：要复制 "
      f"{1000 * 999 // 2} 个 → 比 Python 实际多 {1000 * 999 // 2 / total_copies:.0f} 倍")
print("→ 这就是『append 均摊 O(1)』的真身：单个 append 最坏能到 O(n)（搬家那一次），")
print("  但 n 次加起来只有 O(n)。你 C 那节 copy_total 记的账，机器上就是这么个量级。")
print(f"  两个平均数（{total_copies / 1000:.2f} vs {c_copies / 1000:.2f}）说明这笔交换：")
print("  +12.5% 的步子把**复制常数**放大了约 7 倍，换来的是**容量浪费上限**从 2x 压到 1.13x。")
print("  教科书只说『倍增所以 O(1)』，没说两种策略都在 O(1) 里藏着不同的常数。")
print("  实操推论：**知道规模就先给足** —— list(range(n)) / [None] * n 一次成型，")
print(f"  后面只赋值不扩容；而 `for ... : xs.append(x)` 要搬 {pg} 次家。")
built_in_one = list(range(1000))
print(f"  验证：list(range(1000)) 一次成型 → 槽数 {slots(built_in_one)}（1000 个元素，"
      f"不浪费一格），append 路线是 {py_ladder(1000)[1]} 槽")

print()
print(SEP)
print("四、缩容：pop 到什么时候 Python 会把槽位还回去")
print(SEP)
c = list(range(1000))
start_slots = slots(c)
print(f"  建好 1000 个元素：槽数 {start_slots}，尺寸 {sys.getsizeof(c)} 字节")
first_shrink = None
while c:
    c.pop()
    cur = slots(c)
    if cur < start_slots and first_shrink is None:
        first_shrink = len(c)
    if len(c) in (900, 600, 500, 499, 400, 0):
        mark = "  ← 就是这一刻缩的" if first_shrink == len(c) else ""
        print(f"    剩 {len(c):>4} 个元素 → 槽数 {cur:>4}  尺寸 {sys.getsizeof(c):>5} 字节{mark}")
print(f"  pop 到真的空之后：len={len(c)} 槽数={slots(c)} 尺寸={sys.getsizeof(c)} 字节")
c.append(1)
print(f"  再塞回一个：len={len(c)} 槽数={slots(c)} 尺寸={sys.getsizeof(c)} 字节"
      f"  ← append 出身，又给 4 槽")
print("→ 读法：从 1000 个一直 pop 到 500 个，槽数纹丝不动（还是 1000）；")
print("  掉到 499 那一刻才缩。判据就是『元素数 < 已租槽数 >> 1（一半）』。")
print("  所以 `xs.pop()` 几下、`del xs[:100]` 这种砍不到一半的操作，内存一格都不还 ——")
print("  你 C 版的 shrink 阈值是同一族规则，只是百分比可能不同。")
print("  坑的一面：想立刻把那块指针数组还给系统，靠 pop 是不划算的（缩了也还要留着")
print("  约一半容量）；实测最干脆的是重新赋值 `xs = xs[:0]` 或者 `xs.clear()`。")
big = list(range(2000))
before_len, before_size = len(big), sys.getsizeof(big)
del big[:1000]          # 正好砍掉一半：1000 >= 2000 >> 1，不触发缩容
print(f"  对照：砍之前 len={before_len} 尺寸={before_size} → "
      f"del big[:1000] 之后 len={len(big)} 尺寸={sys.getsizeof(big)}（槽位一格没还）")
del big[:1]             # 再砍一个，跌破一半的门槛，这一刻才还
print(f"  再砍 1 个：len={len(big)} 尺寸={sys.getsizeof(big)} 槽数={slots(big)}  ← 跌破一半才还")
big = list(range(2000))
big.clear()
print(f"  而 big.clear() 之后：len={len(big)} 尺寸={sys.getsizeof(big)}（回到空壳 56）")
