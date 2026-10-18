#!/usr/bin/env python3
# ============================================================================
# 这个文件在演什么：把「复制一个容器」的六种写法排成一个梯子，量出每一级
#                   到底复制到哪一层，以及各自的代价。
# 为什么演：
#   * 第 02 章第 20 节在**对象层**演过 copy / deepcopy（`copy.copy(5) is 5` 那一类）。
#     这一节往前推两步：
#       ① 容器层：同一个二维 list，六种写法各自「共享哪一层」；
#       ② 成本层：浅拷贝和深拷贝差多少个数量级（真计时）；
#       ③ 结构层：深拷贝不只是「全部重造」，它带一张 memo 表，
#          **原来共享同一个对象的两处，复制后仍然共享** —— 这是 C 里 memcpy
#          永远做不到的事（你要么整块抄、要么指针全断，没有「保持别名关系」这个选项）。
#
# 只用标准库（copy、sys、timeit）。
# ============================================================================
import copy
import sys
import timeit

SEP = "=" * 78


def ladder(data):
    """对同一个源对象做六种复制，打印两级判据：
    ① 外壳是不是新对象（区分「贴标签」和「真复制」）
    ② 内层 data[0] 还是不是原来那个对象（区分浅拷贝和深拷贝）"""
    variants = [
        ("b = a              （贴标签）", lambda d: d),
        ("b = a[:]           （切片）", lambda d: d[:]),
        ("b = list(d) / dict(d)/set(d)（构造器）", lambda d: type(d)(d)),
        ("b = a.copy()       （容器的 copy 方法）", lambda d: d.copy()),
        ("b = copy.copy(a)   （标准库浅拷贝）", lambda d: copy.copy(d)),
        ("b = copy.deepcopy(a)（标准库深拷贝）", lambda d: copy.deepcopy(d)),
    ]
    src_inner = data[0]
    print(f"  源对象：{data!r}，它的第 0 项是另一个 list（id 就不打了，用 is 比）")
    print(f"  {'写法':<44} {'外壳是新的吗':>12}  {'内层[0] 还是原来那个吗':>20}")
    for name, fn in variants:
        b = fn(data)
        new_shell = b is not data
        print(f"  {name:<44} {str(new_shell):>12}  {str(b[0] is src_inner):>20}")


print(SEP)
print("一、六种写法在「二维 list」上的分层结果")
print(SEP)
grid = [[1, 2], [3, 4]]
ladder(grid)
print("→ 读法：`外壳是新的吗` 那一列只有第一行是 False —— 除了 `b = a`，")
print("  剩下五种写法**全都新建外壳**，它们是同一级（浅拷贝）。")
print("  真正的分水岭在第二列：`内层[0] 还是原来那个吗` —— 五种浅拷贝全是 True，")
print("  只有 deepcopy 是 False。也就是说浅拷贝复制的是那排**指针**，")
print("  里面那些小 list 一个都没重造（对照你 C 里 `memcpy(dst, src, n*sizeof(int*))`：")
print("  抄的是地址，地址指向的堆内存还是同一批）。")

print()
print(SEP)
print("二、这一层差别在改内层时立刻变成可见的行为差")
print(SEP)
src = [[10, 20], [30, 40]]
shallow = src.copy()
deep = copy.deepcopy(src)
print(f"  改之前：src={src}")
src[0].append(999)          # 改源对象的内层
print(f"  src[0].append(999) 之后：")
print(f"    src      = {src}")
print(f"    shallow  = {shallow}   ← 跟着变了（它俩的第 0 项是同一个 list）")
print(f"    deep     = {deep}    ← 纹丝不动")
shallow[1].append(777)
print(f"  再改 shallow[1].append(777)：src={src}  ← src 又跟着变了（浅拷贝是双向的）")
print(f"                          deep={deep}  ← 深拷贝此刻还是自己那份")
deep[0].append(-1)
print(f"  改 deep[0].append(-1)：src={src}  ← 这次 src 不动了")
print("→ 三条命令给出三个方向：改 src 内层 → 浅拷贝变、深拷贝不变；")
print("  改浅拷贝内层 → src 变；改深拷贝 → 谁都不变。")
print("  判据（可以直接当口诀）：**只要你要改的是『里层』，浅拷贝就等于没拷贝。**")
print("  外壳换了新的（所以 `src.append(...)` 不会串味），但内层全是共享的。")

print()
print(SEP)
print("三、dict / set 的同一把尺子")
print(SEP)
dsrc = {"k": [1, 2]}
dcopy = dsrc.copy()
ddict = dict(dsrc)
ddeep = copy.deepcopy(dsrc)
print(f"  dsrc.copy() 的键集合和 src 一样吗：{dcopy.keys() == dsrc.keys()}"
      f" ，值那个 list 是同一个对象吗：{dcopy['k'] is dsrc['k']}")
print(f"  dict(dsrc)  呢：{ddict['k'] is dsrc['k']}（构造器和 .copy() 同级别）")
print(f"  deepcopy 呢：{ddeep['k'] is dsrc['k']}")
dsrc["k"].append(3)
print(f"  改 dsrc['k'] 之后：copy→{dcopy} / dict()→{ddict} / deepcopy→{ddeep}")
print("→ dict 的浅拷贝复制的是「键→值 那张表」，值对象本身一个都不动。")
print("  set 更极端：元素必须是可哈希的（不可变的），所以 set 的浅拷贝通常已经够用 ——")
s1 = {1, 2, 3}
s2 = s1.copy()
s2.add(4)
print(f"  set 的例子：s1.copy() 后 add(4) → s1={sorted(s1)} s2={sorted(s2)}（互不影响）")
print("  因为 set 里没有「可变的内层」可以共享（能放进去的东西本来就不可变）。")
fs = frozenset([1])
ts = (1, [2])
print(f"  不可变容器的 copy 语义（和第 02 章第 20 节的对象层结论接上）：")
print(f"    copy.copy(fs) is fs        → {copy.copy(fs) is fs}"
      f"（frozenset 不可变，复制 = 给你同一个对象）")
print(f"    copy.copy(t) is t          → {copy.copy(ts) is ts}"
      f"（tuple 同理；但 deepcopy 会造新 tuple：{copy.deepcopy(ts) is ts}，"
      f"里面那个 list 也换了：{copy.deepcopy(ts)[1] is ts[1]}）")

print()
print(SEP)
print("四、深拷贝带 memo：原来的别名关系会被**保留**，这是 memcpy 学不来的")
print(SEP)
shared = [1, 2]
diamond = [shared, shared, [shared]]
sh = diamond.copy()
dp = copy.deepcopy(diamond)
print(f"  源结构：shared 被三处引用 → diamond[0] is diamond[1] = {diamond[0] is diamond[1]}，"
      f"diamond[2][0] is shared = {diamond[2][0] is shared}")
print(f"  浅拷贝：sh[0] is sh[1] = {sh[0] is sh[1]}（而且 sh[0] is shared = {sh[0] is shared}，"
      f"还是原对象）")
print(f"  深拷贝：dp[0] is dp[1] = {dp[0] is dp[1]}，但 dp[0] is shared = {dp[0] is shared}")
dp[0].append("只改一次")
print(f"  改 dp[0] 之后：dp = {dp}")
print("→ 关键在最后那两格：deepcopy **不是**「每个位置都新造一个」，")
print("  而是「每个**原对象**只新造一个」。它内部有张 memo（原对象 id → 新对象），")
print("  所以原本共享的两处，复制后仍然共享同一个新对象；改一处，另两处一起变。")
print("  你 C 里深复制一棵树要自己写「visited 表」才能不重复复制，")
print("  copy.deepcopy 把这张表内置了 —— 顺带也就能正确处理自引用结构。")
selfref = [1]
selfref.append(selfref)           # 列表里装着它自己
sr_deep = copy.deepcopy(selfref)
print(f"  自引用也能抄：sr_deep[0]={sr_deep[0]}，"
      f"sr_deep[1] is sr_deep = {sr_deep[1] is sr_deep}（换成手写递归复制早爆栈了）")

print()
print(SEP)
print("五、成本：浅拷贝 O(n) 抄指针，深拷贝 O(整棵树) 造对象（真计时）")
print(SEP)
n = 100_000
big_src = [[i] for i in range(n)]
t_shallow = min(timeit.repeat("container = src.copy()", setup="",
                              globals={"src": big_src}, number=5, repeat=3)) / 5
t_deep = min(timeit.repeat("container = deepcopy(src)",
                           globals={"src": big_src, "deepcopy": copy.deepcopy},
                           number=1, repeat=3))
print(f"  {n} 个内层 list 的外壳：")
print(f"    copy()     一轮 = {t_shallow * 1000:.2f} ms")
print(f"    deepcopy() 一轮 = {t_deep * 1000:.2f} ms   → 慢 {t_deep / t_shallow:.0f} 倍")
sh2 = big_src.copy()
dp2 = copy.deepcopy(big_src)
print(f"  内存侧账：外壳本身 sys.getsizeof = {sys.getsizeof(big_src)} 字节；"
      f"浅拷贝再要 {sys.getsizeof(sh2)} 字节")
print(f"    但 deepcopy 造了 {n} 个新内层 list，每个 {sys.getsizeof([1])} 字节 → "
      f"光内层就是约 {n * sys.getsizeof([1]) / 1024 / 1024:.1f} MB")
print("→ 所以「要不要 deepcopy」不是风格问题，是 100 倍时间和几十倍内存的问题。")
print("  实践顺序：能不抄就不抄（只读就传引用）→ 要抄外壳用 copy()/切片 →")
print("  真要改内层又必须独立，才上 deepcopy；再不行就是数据结构选错了（该建索引了）。")
