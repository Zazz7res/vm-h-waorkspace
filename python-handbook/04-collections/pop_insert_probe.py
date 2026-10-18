#!/usr/bin/env python3
# ============================================================================
# 这个文件在演什么：把「增删改查里跟位置有关的那几个操作」量成数字 ——
#   pop(0) / pop() / deque.popleft() / insert(0) / append / del a[i] /
#   remove(值) / 下标访问，以及 deque 的隐藏弱点 a[i]。
# 为什么演：
#   * 复杂度表写 `list.pop(0) 是 O(n)`，你可以背；但「慢到什么程度、和 append
#     差几个数量级」只能靠量。这一份就是那把尺子。
#   * 每一格都接回你写过的 C 结构：
#       pop(0)/insert(0)/del a[0] 在数组上 = 你把 arr[1..n-1] 整体前移/后移的那个 for；
#       deque.popleft()  = 你在 02-linked-list/doubly 里 head = head->next 那一行；
#       04-queue/circular-queue 里 front = (front+1) % capacity 也是同一件事。
#     Python 的 list 做不到 O(1) 出队，因为它就是数组；deque 做得到，因为它内部是
#     「一块块定长小数组 + 双向串起来」。
#
# 计时口径（照这段读数字，别把小数位当结论）：
#   * setup 串里**重建容器**，所以 timeit 每一次 repeat 都从满长度开始；
#   * 每轮只动 K = n/50 个元素（≤2%），于是测到的是「规模为 n 时的单次代价」，
#     不会被「越 pop 越短」污染；
#   * repeat=3 取最小值。数字里含着 `container.pop(0)` 这一句本身的全局查找 +
#     方法调用开销（几十 ns 量级），所以看量级、别比小数位。
# 只用标准库（timeit、collections.deque）。
# ============================================================================
import sys
import timeit
from collections import deque

SEP = "=" * 78
MEASURED = {}


def cost(setup, stmt, number, key=None):
    """单次代价（µs）。setup 在每次 repeat 前重建容器，保证长度不被消耗。"""
    best = min(timeit.repeat(stmt, setup=setup, number=number, repeat=3))
    value = best / number * 1e6
    if key:
        MEASURED[key] = value
    return value


def setup_for(kind, n):
    """setup 串是在一个干净命名空间里执行的，所以 deque 这个名字要自己带进去。"""
    if kind == "deque":
        return f"from collections import deque; container = deque(range({n}))"
    return f"container = list(range({n}))"


print(SEP)
print("一、pop(0) vs pop() vs deque.popleft()：同一件事，两种复杂度")
print(SEP)
print(f"{'n':>9} {'list.pop(0) µs':>16} {'list.pop() µs':>15} {'deque.popleft() µs':>20}")
for n in (1_000, 10_000, 100_000):
    k = max(20, n // 50)
    p0 = cost(setup_for("list", n), "container.pop(0)", k, f"p0@{n}")
    plast = cost(setup_for("list", n), "container.pop()", k, f"plast@{n}")
    pdq = cost(setup_for("deque", n), "container.popleft()", k, f"pdq@{n}")
    print(f"{n:>9} {p0:>16.2f} {plast:>15.4f} {pdq:>20.4f}")
print(f"  倍数：n=1000 时 pop(0) 是 pop() 的 {MEASURED['p0@1000'] / MEASURED['plast@1000']:.0f} 倍；"
      f"n=100000 时是 {MEASURED['p0@100000'] / MEASURED['plast@100000']:.0f} 倍")
print("  同一列里 pop(0) 随 n 涨（1千→10万，×100 左右），pop() 和 popleft() 基本不涨。")
print("→ 为什么：list.pop(0) 要把**后面所有元素往前挪一格**（你在 01-array 里写过那个 for），")
print("  再把最后一个槽清掉；pop() 只清最后一格、size--，一个元素都不用挪。")
print("  deque.popleft() 和 list.pop() 一样是常数 —— 队头那块小数组空了就整块扔掉，")
print("  等于你双向链表里的 head = head->next。")
print("  ★ 推论：要队列语义就 deque。用 list 做队列（`xs.pop(0)`）代码看着对、")
print("    跑起来是 O(n²)，而且 list 那边**根本没有 popleft 这个方法**（见下面）。")
try:
    [1, 2].popleft()
except AttributeError as exc:
    print(f"  现场复现：[1, 2].popleft() → AttributeError: {exc}")

print()
print(SEP)
print("二、insert(0, x) vs append(x)：位置决定复杂度")
print(SEP)
print(f"{'n':>9} {'list.insert(0,x) µs':>20} {'list.append(x) µs':>19} {'deque.appendleft(x) µs':>24}")
for n in (1_000, 10_000, 100_000):
    k = max(20, n // 50)
    i0 = cost(setup_for("list", n), "container.insert(0, -1)", k, f"i0@{n}")
    ap = cost(setup_for("list", n), "container.append(-1)", k, f"ap@{n}")
    ial = cost(setup_for("deque", n), "container.appendleft(-1)", k, f"ial@{n}")
    print(f"{n:>9} {i0:>20.2f} {ap:>19.4f} {ial:>24.4f}")
print(f"  n=100000 那行：insert(0) 是 append 的 {MEASURED['i0@100000'] / MEASURED['ap@100000']:.0f} 倍。")
print("→ insert(0) = 「先整体后移一格，再填第 0 格」，那个整体后移是一次 memmove；")
print("  append = 「size < 已租槽位就直接写一格，写满才搬家」→ 均摊 O(1)。")
print("  搬家台阶实测在 capacity_growth.py：1000 个元素一共搬 28 次，摊下来每次 append")
print("  只多 7.56 个指针复制 —— 这就是那 0.0x µs 的来源。")

print()
print(SEP)
print("三、del / pop / remove：按索引砍 vs 按值找，成本完全不同")
print(SEP)
n = 100_000
k = 200
setup = setup_for("list", n)
t_head = cost(setup, "del container[0]", k)
t_last = cost(setup, "del container[-1]", k)
t_mid = cost(setup, "del container[50000]", k)
# 删「当下的队头值 / 队尾值」而不是固定某个值：base 里每个值只出现一次，
# 写死 remove(0) 的话第二轮就 ValueError 了（这个坑本身也是坑，本章
# mutate_while_iterating.py 第二节正面演它）。
r_head = cost(setup, "container.remove(container[0])", k)
# remove(container[-1])：值永远是新的，而且必须一路扫到数组尾部才算找到
# → 正是「找得最全、搬得最少」那一格。
r_last = cost(setup, "container.remove(container[-1])", k)
print(f"  del container[0]        {t_head:>8.2f} µs   ← 把后面 n-1 个往前挪")
print(f"  del container[-1]       {t_last:>8.2f} µs   ← 只清最后一格")
print(f"  del container[50000]    {t_mid:>8.2f} µs   ← 搬后一半，差不多是 del[0] 的一半")
print(f"  remove(container[0])    {r_head:>8.2f} µs   ← 找 1 步 + 挪 n-1 个")
print(f"  remove(container[-1])   {r_last:>8.2f} µs   ← 挪 0 个，但找穿了整个数组")
print("→ 两条规律，量出来正好对上：")
print("  · `del a[i]` / `pop(i)` 的成本 = 「从 i 一直搬到尾」，所以 del[0] 最贵、del[-1] 免费；")
print("  · `remove(v)` 的成本 = 「从头找到 v」+「从那个位置搬到尾」，两段各一份 O(n)。")
print("  所以 remove(队头值) 又找又挪、remove(队尾值) 只找不挪 —— 两格的数字")
print("  都在 O(n) 这一档，只是常数不同。按值删永远比按索引删多一遍扫描。")

print()
print(SEP)
print("四、下标访问：list 全位置等速，deque 中间最慢（数组 vs 链表的招牌差异）")
print(SEP)
n = 100_000
k = 2_000
print(f"{'位置':>16} {'list µs/次':>12} {'deque µs/次':>13}")
for label, idx in (("container[0]", 0), ("container[500]", 500),
                   ("container[50000]", 50_000), ("container[-1]", -1)):
    tl = cost(setup_for("list", n), label, k)
    td = cost(setup_for("deque", n), label, k)
    print(f"{label:>16} {tl:>12.4f} {td:>13.4f}")
print("→ list：四个位置同一个数（0.00x µs 级别，互相在噪声里）—— 指针数组，")
print("  下标直接算地址，这就是你在 01-array/static-array 里白拿到的 O(1) 随机访问。")
print("→ deque：两端 0.01x µs，中间 1.x µs，慢一百倍。它要从**最近的一端**一块一块跳过去")
print("  （链表行为）。注意 500 → 50000 是索引×100，而耗时只涨了几十倍：")
print("  说明它不是逐元素跳、而是逐「小块」跳（每块几十格）。块到底几格不用猜，")
print("  第五节用 sys.getsizeof 一格一格量出来。")
print("  你在 02-linked-list/singly 里写的 get(i) 要顺着 next 走 i 步，deque 同理，")
print("  只是每步跨一块，所以比纯链表快，但复杂度仍然是 O(n)。")
print("  ★ 结论：deque 是「队列/栈」，不是数组。别拿它按下标随机访问，")
print("    也别拿 list 当队列 —— 两个容器的强项正好互补，没有全能选手。")

print()
print(SEP)
print("五、顺手把 deque 的两个语义确认掉（maxlen 有界队列 / 判空）")
print(SEP)
dq = deque([1, 2, 3, 4, 5], maxlen=3)
print(f"  deque([1,2,3,4,5], maxlen=3) = {list(dq)} ← 建的时候就把 1、2 从队头挤掉了")
dq.append(6)
print(f"  再 append(6) → {list(dq)} ← 满 3 格再进人，队头自动掉，长度永不超上限")
full = deque([10, 20, 30], maxlen=3)
full.append(40)
print(f"  满员的 maxlen deque 塞第四个：{list(full)}（len={len(full)}）")
print("→ 你写 04-queue/circular-queue 时要自己处理「假满」（front/rear 绕圈、")
print("  要么多带一个 size 计数器、要么永久浪费一格）。maxlen 就是官方给你的那口")
print("  环形有界队列：**溢出的是最老的数据，不是新来的**（这点和数组版判满返回失败相反）。")
print(f"  尺寸对照：deque 空壳 = {sys.getsizeof(deque())} 字节，"
      f"三元素的 = {sys.getsizeof(deque([1, 2, 3]))} 字节，"
      f"同样三个元素的 list = {sys.getsizeof([1, 2, 3])} 字节")
print("  deque 的「一块」到底几格：连续加长度，只在 getsizeof 变化那一刻打一行")
prev = sys.getsizeof(deque())
jumps = []
for n in range(1, 257):
    now = sys.getsizeof(deque(range(n)))
    if now != prev:
        jumps.append((n, prev, now))
        prev = now
for n, old, now in jumps:
    print(f"    装到第 {n} 个元素：{old} → {now} 字节（+{now - old} = "
          f"{now - old - 16} 字节指针 + 16 字节块头 → {(now - old - 16) // 8} 格）")
print("→ 台阶正好落在 64 的整数倍上，每块 64 个元素 —— 第四节那个「逐块跳」到这里才算量出来：")
print(f"  container[50000] 要从最近的一端数 {50_000 // 64} 块（50000 // 64），")
print("  而 container[0] 只要摸第 1 块。这就是第四节里 deque 中间落到 1.0 µs、"
      "list 全位置都趴在 0.00x µs 的原因。")
print(f"  另外注意空壳就 {sys.getsizeof(deque())} 字节：deque 一建就租下整块（里面一个元素都没有），")
print("  空 list 只要 56 字节（数字出自 container_sizes.py 第一节）。")
print("  所以「很多个小队列」这种场景，deque 的这块固定开销要算进账里。")
