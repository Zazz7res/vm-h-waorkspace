#!/usr/bin/env python3
# ============================================================================
# 这个文件在演什么：拼一串东西的时候，四种写法的真实代价 ——
#   列表：acc = acc + [x] / acc += [x] / acc.append(x) / acc.extend(...)
#   字符串：s = s + ch / s += ch / ''.join(parts)
# 为什么演：
#   * 「循环里拼字符串是 O(n²)」这条铁律你一定听过。这里要把它量出来，
#     并且**当场把它打折**：CPython 对 `s = s + ch` 有个私有优化，
#     单引用时就地在原缓冲区后面续，看起来像 O(n)；一旦这个字符串多了一个引用，
#     优化立刻失效，同一份代码慢两三个数量级。这种「靠编译器私有优化」的写法
#     就是不能赌的东西 —— 只有真跑才看得出来。
#   * 列表这边同一个道理，但方向相反：`acc += [x]` 和 `acc.append(x)` 是两种语义
#     （吸收可迭代对象 vs 塞一个元素），`acc = acc + [x]` 才是每次都新建外壳。
#
# 计时口径：每次计时都跑完「造出 N 个元素的整个循环」，number=1、repeat=3 取最小；
# 报的是整轮毫秒数 + 每元素微秒数。N 取 5000 / 20000，能看出斜率又不等到天黑。
# 只用标准库（timeit）。
# ============================================================================
import timeit

SEP = "=" * 78


def bench(stmt, g, repeat=3):
    """整轮计时（ms）。stmt 里自己写循环，globals 提供数据；
    每次 repeat 都从头跑一遍完整循环，所以容器状态不会串味。"""
    return min(timeit.repeat(stmt, globals=dict(g), number=1, repeat=repeat)) * 1000.0


print(SEP)
print("一、列表拼接：N=5000 次，把整个循环当一次来测")
print(SEP)
N = 5000
items = list(range(N))
G = {"items": items, "N": N}
cases = [
    ("acc = acc + [x]        （每轮新建外壳）",
     "acc = []\nfor x in items: acc = acc + [x]"),
    ("acc += [x]             （原地吸收）",
     "acc = []\nfor x in items: acc += [x]"),
    ("acc.append(x)          （塞一个）",
     "acc = []\nfor x in items: acc.append(x)"),
    ("acc.extend([x])        （吸收一个可迭代对象）",
     "acc = []\nfor x in items: acc.extend([x])"),
    ("acc = list(items)      （一次成型）",
     "acc = list(items)"),
]
first = None
for label, stmt in cases:
    ms = bench(stmt, G)
    if first is None:
        first = ms
        note = "← 基准"
    else:
        note = f"比基准快 {first / ms:.0f} 倍"
    print(f"  {label:<44} {ms:>9.2f} ms   {ms * 1000 / N:>7.3f} µs/元素   {note}")
print("→ 第一行就是 O(n²)：每轮 `acc + [x]` 都要新建外壳并把已有的 i 个指针全抄一遍，")
print(f"  合计抄 N(N+1)/2 = {N * (N + 1) // 2} 个指针；后面三行都是 N 次常数操作。")
print("  ★ `acc += [x]` 和 `acc = acc + [x]` 差着数量级：+= 走 list.__iadd__ 原地吸收")
print("    （第 02 章第 5 节那条「同一个 += 号，list 原地、str 换对象」在这里兑现），")
print("    而 `= acc + [x]` 每轮都造一个新 list 再把标签挪过去。")
print("  顺手钉死一个语义差：append 塞「整个对象」，extend 拆开塞「每个元素」。")
a, b = [], []
for x in [[1, 2], [3, 4]]:
    a.append(x)
    b.extend(x)
print(f"  append 的结果：{a}   extend 的结果：{b}")

print()
print(SEP)
print("二、字符串拼接：N=20000 次，以及那个会被打断的私有优化")
print(SEP)
M = 20_000
chars = [chr(97 + i % 26) for i in range(M)]
G2 = {"chars": chars, "M": M}
cases2 = [
    ("s = s + ch        （s 只被 s 这一个名字指着）",
     "s = ''\nfor ch in chars: s = s + ch"),
    ("s += ch           （同上，+= 对 str 也是换对象）",
     "s = ''\nfor ch in chars: s += ch"),
    ("s = s + ch        （每轮先 hist.append(s) 留一份引用）",
     "s = ''\nhist = []\nfor ch in chars: hist.append(s); s = s + ch"),
    ("parts.append(ch) + ''.join(parts)",
     "parts = []\nfor ch in chars: parts.append(ch)\ns = ''.join(parts)"),
    ("s = ''.join(chars)（数据本来就是 list，直接 join）",
     "s = ''.join(chars)"),
]
base = None
for label, stmt in cases2:
    ms = bench(stmt, G2)
    if base is None:
        base = ms
        note = "← 基准"
    else:
        note = f"×{ms / base:.1f}"
    print(f"  {label:<48} {ms:>9.2f} ms   {note}")
print("→ 这条最反直觉，值得逐行读：")
print("  · 第 1、2 行几乎一样快 —— CPython 发现「左边的 str 只剩一个引用、马上要被覆盖」时，")
print("    就在那块缓冲区后面原地续字节，于是循环拼串看起来是 O(n)。")
print("  · 第 3 行只是多写了一句 hist.append(s)（让那个 str 多一个引用），优化立刻失效，")
print("    数字直接翻两个数量级（倍率见上表最右列）—— 这才是「O(n²)」原形毕露的样子。")
print("  · join 那一行不受影响：它先扫一遍算总长度、一次分配、再一次填，")
print("    跟引用计数没有任何关系。**所以 join 是可以写进规范的，+= 是赌运气。**")
print("  结论：拼字符串固定写 `''.join(parts)`；别用『反正 += 也挺快』的结论去优化，")
print("    那个快是本机 CPython 的私有优化给的，多一个引用、换个实现就没了。")

print()
print(SEP)
print("三、规模翻倍看斜率：谁能线性、谁是平方")
print(SEP)
print(f"  {'N':>8} {'acc = acc + [x]':>18} {'相对首行':>10}   "
      f"{'append+join 版':>15} {'相对首行':>10}")
head = None
prev = None
for size in (2_000, 4_000, 8_000, 16_000):
    data = list(range(size))
    g = {"items": data}
    t_slow = bench("acc = []\nfor x in items: acc = acc + [x]", g)
    t_fast = bench("parts = []\nfor x in items: parts.append(str(x))\n' '.join(parts)", g)
    if head is None:
        head = (t_slow, t_fast)
    print(f"  {size:>8} {t_slow:>15.2f} ms {t_slow / head[0]:>9.0f}x   "
          f"{t_fast:>12.2f} ms {t_fast / head[1]:>9.1f}x")
    prev = (t_slow, t_fast)
print(f"  读法：N 从 2000 涨到 16000（×8）。最后一行相对第一行："
      f"左列 ×{prev[0] / head[0]:.0f}（平方律给的是 ×64），"
      f"右列 ×{prev[1] / head[1]:.1f}（线性律给的是 ×8）。")
print(f"  · 左列从 ×2 规模到 ×{prev[0] / head[0]:.0f} 耗时，比 ×8 大得多 → 平方律；"
      f"右列只 ×{prev[1] / head[1]:.1f}，和 N 同数量级 → 线性律。")
print("  · 左列甚至比纯平方律（×64）还高一点，多出来的是内存分配器的压力：")
print("    每一轮都要新建并丢弃一个外壳，垃圾越多分配越贵。这个偏差本身就是信息。")
print("→ 结论：N×2 时耗时×4 = O(n²)；N×2 时耗时×2 = O(n)。")
print("  不用背表，把 N 翻一倍量一次就知道自己写的是哪个 —— 看倍率不看绝对毫秒数。")
print("  这招你在 10-sorting 那类实验里应该已经用过：看的是**倍率**，不是绝对毫秒数。")
