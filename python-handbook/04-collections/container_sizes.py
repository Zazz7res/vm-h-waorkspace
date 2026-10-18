#!/usr/bin/env python3
# ============================================================================
# 这个文件在演什么：四大内置容器的「空壳到底多少字节」，以及它们的扩容台阶。
# 为什么演：
#   * 你写 C 的时候，`int a[10]` 占多少字节编译期就知道；Python 容器的尺寸是
#     **运行时才决定的**，而且「空」和「有 1 个元素」差着一个数量级。
#   * 第 02 章收尾时留了一句「dict 空表 224 字节」当尺子。这一节要用真数字
#     把它纠正过来：224 是 **塞进第一个键之后** 的尺寸，空 dict 其实是 64。
#     搞错这个，你就永远算不清「我到底是在省内存还是费内存」。
#   * set / dict 的扩容台阶决定了「装 n 个元素要多少内存」，这和你手写哈希表时
#     「装到 0.75 就翻倍桶数」那条规矩是同一件事，只是阈值和布局不一样（见输出）。
#
# 怎么读输出：每一段先打「我在量什么」，再打真数字，最后用一行 `→` 给结论。
# 只用标准库（sys），不装包、不联网。
# ============================================================================
import sys

SEP = "=" * 70


def sizeof(obj):
    """getsizeof 只量「这个对象自己占的字节」，不量它指着的那些元素。
    这一句很重要，后面第 3 段专门用一条命令把这件事钉死。"""
    return sys.getsizeof(obj)


print(SEP)
print("一、四个容器的空壳：谁在『建的时候』就已经花钱了")
print(SEP)
empties = [("list []", []), ("dict {}", {}), ("set set()", set()), ("tuple ()", ())]
for name, obj in empties:
    print(f"  {name:<12} sys.getsizeof = {sizeof(obj):>4} 字节   类型 = {type(obj).__name__}")
print("→ 结论：空 list 56 / 空 tuple 48 是『纯壳』（还没租元素数组）；")
print("  空 dict 64 也是纯壳；但空 set 216 —— 它建的时候就顺手租了 8 个槽位。")
print("  所以『用 set() 而不是 [] 做去重』这句话，省不省内存要看你装几个元素。")
print(f"  顺带把两个兄弟量了：frozenset() = {sizeof(frozenset())} 字节，"
      f"bytearray() = {sizeof(bytearray())} 字节，range(10) = {sizeof(range(10))} 字节")
print("  （range 只存 start/stop/step 三个数，所以长度 10 和长度 10 亿一样大 —— "
      "这是 C 里做不到的事：数组不可能『不占地方』）")

print()
print(SEP)
print("二、纠正第 02 章那句话：224 不是空 dict，是『装了 1 个键』的 dict")
print(SEP)
print(f"  sys.getsizeof({{}})                = {sizeof({})}")
print(f"  sys.getsizeof({{1: 2}})             = {sizeof({1: 2})}")
print(f"  sys.getsizeof({{i: i for i in range(5)}})  = {sizeof({i: i for i in range(5)})}")
print(f"  sys.getsizeof({{i: i for i in range(6)}})  = {sizeof({i: i for i in range(6)})}")
print("→ 第 02 章量的那个 224，对象其实是 {1: 2}（1 个键），不是空表。")
print("  真·空 dict 只有 64 字节；一旦塞进第一个键，立刻跳到 224 —— 因为这时才")
print("  真的分配索引数组。第 6 个键再跳一档。这就是『懒分配』：")
print("  不塞东西不花钱，塞第一样东西一次性花一大笔。")

print()
print(SEP)
print("三、getsizeof 量的是『外壳』，不含元素本体（一条命令看穿）")
print(SEP)
small = [1, 2, 3]
print(f"  sys.getsizeof([1, 2, 3])              = {sizeof([1, 2, 3])} 字节")
print(f"  sys.getsizeof(['aaaaaaaaaaaaaaaa', 2, 3]) = {sizeof(['aaaaaaaaaaaaaaaa', 2, 3])} 字节")
big = [bytearray(10_000) for _ in range(3)]
print(f"  sys.getsizeof([bytearray(10000) × 3]) = {sizeof(big)} 字节"
      f"   （但里面三个 bytearray 各占 {sizeof(bytearray(10000))} 字节）")
print("→ list 里存的是『指针』，不是元素本体：所以 3 个 1 万字的元素，外壳一样 88 字节，")
print("  真实总内存要自己递归加。C 的 int arr[3] 是本体躺在数组里，两者内存模型不同。")
print("  这正是『list 是指针数组』这句话的物理含义（第 02 章第 14 节埋的那把尺子）。")

print()
print(SEP)
print("四、dict / set 的扩容台阶：装到几个键会跳一档")
print(SEP)
print("  dict：连续插键，只在尺寸变化的那一刻打一行")
d = {}
prev = sizeof(d)
print(f"    0 个键      {prev:>5} 字节")
for i in range(1, 41):
    d[i] = i
    now = sizeof(d)
    if now != prev:
        print(f"    第 {i:>2} 个键    {now:>5} 字节   （跳 +{now - prev}）")
        prev = now
print("  set：连续 add，把前 8 步的尺寸逐个打（看『平台』在哪断裂）")
s = set()
step = [f"0 个 → {sizeof(s)}"]
for i in range(1, 9):
    s.add(i)
    step.append(f"{i} 个 → {sizeof(s)}")
print("    " + "   ".join(step[:4]))
print("    " + "   ".join(step[4:]))
print("→ 读法一（dict）：尺寸按『装到第 6、第 11、第 22 个键』这三刻跳档，")
print("  也就是每档能装的键数是 5 → 10 → 21 → 41，接近翻倍。")
print("  你写 C 那条链式哈希表是『装到 capacity 的 0.75 就翻倍桶数 + 全部重投桶』；")
print("  CPython 的 dict 也在扩容重排，但阈值更保守（每档约留 1/3 空位），")
print("  而且它没有『每个桶挂一条链』这回事 —— 桶里直接就是键值对本身。")
print("→ 读法二（set）：空表就 216 字节，装 1～4 个元素尺寸纹丝不动，第 5 个才跳。")
print("  这就是『空 set 比空 dict 贵 3 倍多』的原因：set 一建就租了一整块探测表，")
print("  dict 则是等到第一个键才租。真要做『可能永远是空』的去重容器，dict 更省。")

print()
print(SEP)
print("五、同样 len=1，两种出身两种尺寸（这是最容易看走眼的一处）")
print(SEP)
lit = [1]
grown = []
grown.append(1)
print(f"  [1] 字面量：      len={len(lit)}  getsizeof={sizeof(lit)} 字节 → 槽位 {(sizeof(lit) - 56) // 8}")
print(f"  append 长出来的： len={len(grown)}  getsizeof={sizeof(grown)} 字节 → 槽位 {(sizeof(grown) - 56) // 8}")
print(f"  list(range(1))：  len=1  getsizeof={sizeof(list(range(1)))} 字节")
print(f"  [0]*5：           len=5  getsizeof={sizeof([0] * 5)} 字节   ← 一次成型，不浪费槽位")
print("→ 字面量 / list() / [0]*n 这类『一次成型』的构造按精确长度分配（1 槽），")
print("  append 走的扩容路线则直接给 4 槽。两者 len 相同、值相同，内存差 24 字节。")
print("  含义：`sys.getsizeof` 只能当『这一坨到底租了几格』的证人，")
print("  千万别拿它当两个相等 list 的相等判据（顺便：`[1] == [1]` 是 True，"
      "两条判据的分工见 mutate_while_iterating.py 第六节）。")
