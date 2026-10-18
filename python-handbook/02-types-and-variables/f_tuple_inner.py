"""第 02 章第 6 节：tuple 里装 list —— 「不可变」到底管到哪一层。

跑法：python3 f_tuple_inner.py
（本节证据就是这段脚本的输出，demo.sh 里有对应小节。）
"""


def same(x, y):
    return id(x) == id(y)


print("=" * 68)
print("一、tuple 里装 list：「不可变」到底管到哪一层")
print("=" * 68)
t = (1, [10, 20])
outer_id = id(t)
inner_id = id(t[1])
t[1].append(30)
print("往 tuple 里那个 list 追加元素之后：", t)
print("tuple 这个对象本身换了吗？", id(t) != outer_id)
print("里面的 list 还是原来那个对象吗？", id(t[1]) == inner_id)
print("→ tuple 的「不可变」只管**里面每一格指向哪个对象**，不管那个对象自己变不变。")
print("  这叫 shallow immutability（浅层不可变）。所以 tuple 不能当「深」的常量用。")

print()
print("顺带一个能当场把 hash 规则证出来的对照：")
print("hash((1, 2)) 能不能算？", end=" ")
try:
    hash((1, 2))
    print("能")
except TypeError as err:
    print("不能：", err)
print("hash((1, [2])) 能不能算？", end=" ")
try:
    hash((1, [2]))
    print("能")
except TypeError as err:
    print("不能：", err)
print("→ 因为 dict 的键/set 的元素必须永远不变，可变的 list 装进来就破坏哈希表，")
print("  所以解释器直接拒绝给含 list 的 tuple 算哈希。")

print()
print("最后一行是本节最狠的一个现场：")
t2 = (1, [10, 20])
print("执行 t2[1] += [30] 之前：", t2)
try:
    t2[1] += [30]
except TypeError as err:
    print("TypeError 报错原文：", err)
print("执行完（虽然是失败的一行）之后：", t2)
print("→ 报了错，但 list 真的被改了：+= 先原地吸收（成功），再往 tuple 的第 1 格贴标签（被禁止）。")
print("  半路失败、副作用留下。这一条是本章「不可变」最常被误解的地方。")
