#!/usr/bin/env python3
# ============================================================================
# 这个文件在演什么：`*` 重复运算符造出来的容器**共享内层对象**这一族坑，
#                   全部用真实输出 + id 判据钉死。
# 为什么演：
#   * 这是从 C 转过来的人最高频的坑：`int board[3][3] = {{0}}` 在 C 里是 9 个独立格子，
#     而 Python 的 `[[0]*3]*3` 只有 1 个内层 list 被贴了 3 张标签 —— 改一个，三行齐变。
#   * 关键在于「`*` 复制的是引用，不是对象」。这一步理解之后，
#     `dict.fromkeys(ks, [])`、`a = b = []`、`[[]] * 3` 全是同一个坑的变体。
#   * 每种错法都当场给「改一个看谁跟着变」的证据，而不是只说「别这么写」。
#
# 只用标准库。
# ============================================================================
import collections

SEP = "=" * 76


def ids(seq):
    return [id(x) for x in seq]


print(SEP)
print("一、`[0] * 5`：五个槽指同一个对象 —— 但这次坑不到你")
print(SEP)
a = [0] * 5
print(f"  a = {a}")
print(f"  五个元素的 id 有几个不同值：{len(set(ids(a)))} 个（1 = 全都指向同一个 int 0）")
a[0] = 9
print(f"  a[0] = 9 之后：a = {a}  ← 只有第 0 格换了标签，别的花纹都没动")
print("→ 因为 int 不可变：`a[0] = 9` 是**换标签**（第 02 章第 2 节那套词），")
print("  不是改对象，所以共享反而无害。这一条要记住，下一段才是咬人的地方。")

print()
print(SEP)
print("二、`[[0]*3]*2`：改一个格，两处一起变（真实现场）")
print(SEP)
m = [[0] * 3] * 2
print(f"  建好之后：m = {m}")
print(f"  m[0] is m[1] → {m[0] is m[1]}   ← 两行是同一个 list 对象")
m[0][1] = 9
print(f"  只改 m[0][1] = 9 之后：m = {m}   ← 第二行也跟着变了")
print("→ `* 2` 把**同一个**内层 list 的引用抄了两遍，所以你手上是「两排指向一格的指针」。")
print("  对 C 的直觉修正：C 里 `int m[2][3]` 是 6 个独立 int 躺在一块内存里；")
print("  这里只有 1 个 list（3 格）+ 一个 2 元素的外壳。")

print()
print(SEP)
print("三、同一招用在三维上：`[[[0]*4]*3]*2` 改一个，六处齐变")
print(SEP)
c3 = [[[0] * 4] * 3] * 2
print(f"  最深那层共享吗：c3[0][0] is c3[0][1] → {c3[0][0] is c3[0][1]}，"
      f"c3[0][0] is c3[1][0] → {c3[0][0] is c3[1][0]}")
c3[0][0][0] = 7          # 只改一个格
print(f"  只改一个格 c3[0][0][0] = 7，整个 c3 变成：")
print(f"    {c3}")
print(f"  一共 {len(c3) * len(c3[0])} 行，全带上了那个 7 —— 因为它们本来就是同一行")
print("→ 层数越多越隐蔽：`*` 只作用在最外面那一层，但它重复的是引用，")
print("  于是一整片格子其实是同一块内存。调试时你看到的是「我改了一个，怎么全变了」。")

print()
print(SEP)
print("四、对法：推导式给每一格造新对象")
print(SEP)
good = [[0] * 3 for _ in range(2)]
print(f"  good[0] is good[1] → {good[0] is good[1]}")
good[0][1] = 9
print(f"  改 good[0][1] = 9：good = {good}  ← 只有第 0 行动")
good3 = [[[0] * 4 for _ in range(3)] for _ in range(2)]
print(f"  三维对法：good3[0][0] is good3[1][2] → {good3[0][0] is good3[1][2]}")
print(f"  内层是新建的吗：{all(id(r) != id(good3[0][0]) for r in good3[1])}")
print("→ 记忆法：**有几层独立，就要写几个 for**。`*` 只造「引用」，`for` 才造「对象」。")
print("  （推导式那节会正式讲这个写法，这里先打个样。）")

print()
print(SEP)
print("五、变体 1：`[[]] * 3` —— 一上来就是三个空 list 挤在同一块")
print(SEP)
buckets = [[]] * 3
buckets[0].append("只有第 0 个桶")
print(f"  往 buckets[0] 塞一个元素之后：buckets = {buckets}")
print(f"  len(buckets[1]) = {len(buckets[1])}  ← 明明没碰过它，它已经有一个元素了")
right = [[] for _ in range(3)]
right[0].append("只有第 0 个桶")
print(f"  对法：{right}")

print()
print(SEP)
print("六、变体 2：`dict.fromkeys(keys, [])` —— 默认值是共享的（这个最阴）")
print(SEP)
keys = ["a", "b", "c"]
d = dict.fromkeys(keys, [])
print(f"  d = {d}")
d["a"].append(1)
print(f"  d['a'].append(1) 之后：d = {d}   ← 三个键的值全变了")
print(f"  判据：d['a'] is d['b'] → {d['a'] is d['b']}")
print("  （不写第二个参数时 fromkeys 填的是 None —— 见下面③，那个才是安全默认。）")
print("→ 三种对法，按场景挑：")
correct1 = {k: [] for k in keys}
correct1["a"].append(1)
print(f"  ① 推导式各造一个：{correct1}")
correct2 = collections.defaultdict(list)
correct2["a"].append(1)
print(f"  ② defaultdict(list)（工厂每次新建）：{dict(correct2)}")
correct3 = {}.fromkeys(keys)                        # 默认 None，只读场景够用
print(f"  ③ 真的只想要「都填 None」：{correct3} —— None 不可变，共享无害")

print()
print(SEP)
print("七、自检口诀：造完就打印 id 集合的大小")
print(SEP)
bad = [[0] * 3] * 2
good = [[0] * 3 for _ in range(2)]
print(f"  len(set(map(id, bad)))  = {len(set(map(id, bad)))}   ← 1 就是「两行一格」")
print(f"  len(set(map(id, good))) = {len(set(map(id, good)))}  ← 2 才是对的")
print("→ 上线前加一行 `assert len(set(map(id, rows))) == len(rows)`，")
print("  这种坑就再也藏不住了（比肉眼找『为什么三行一起变』快得多）。")
