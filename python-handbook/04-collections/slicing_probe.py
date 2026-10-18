#!/usr/bin/env python3
# ============================================================================
# 这个文件在演什么：切片的全部语义 —— 越界、步长、负步长、切片赋值改长度，
#                   以及「谁能切、谁不能切」。
# 为什么演：
#   * 你在 01-array/static-array 里越界一次，ASan 就给你一屏红字；Python 的切片
#     越界**什么都不报**，静悄悄给你一个短列表。这在写「分页取数据」时是好事，
#     在写「按下标取第 i 个」时是埋雷。这里把两者的边界一次划清。
#   * 切片不只是「读」：`a[1:3] = [...]` 能改长度、`a[::2] = [...]` 有额外规矩。
#     这相当于你 C 里 memmove + 手动调 size 那一整套，Python 一句话干完。
#   * 本文件也顺手回答「set/dict 能不能切」——答案带一个很妙的坑：dict 不但不报
#     TypeError，还把 slice 当成一个**键**去找。
#
# 只用标准库。每段后面用 → 给结论。
# ============================================================================
SEP = "=" * 74


def show(expr, obj):
    print(f"  {expr:<28} → {obj}")


print(SEP)
print("一、索引 vs 切片：一个越界就抛，一个越界闷声给你空的")
print(SEP)
a = [10, 20, 30, 40, 50]
print(f"  被切的对象：{a}（len={len(a)}）")
show("a[4]（最后一个合法下标）", a[4])
try:
    a[5]
except IndexError as exc:
    print(f"  a[5]                         → IndexError: {exc}")
show("a[5:9]（完全在范围外）", a[5:9])
show("a[100:200]", a[100:200])
show("a[-100:-50]（两头都越界）", a[-100:-50])
show("a[-100:]（越界但含有效部分）", a[-100:])
show("a[10:]（起点在 len 上）", a[10:])
show("a[3:1]（起点 > 终点，正向）", a[3:1])
print("→ 规矩一句话：**单下标越界 = IndexError；切片越界 = 自动夹到边界，给空不报错。**")
print("  原因是切片走的是「把 start/stop 夹进 [0, len] 再取」这条路（C 里你得自己 clamp），")
print("  而单下标走的是「直接算地址取」那条路，取不到就抛。")
print("  对照你写的 C：`arr[i]` 越界是未定义行为（编译器不管、ASan 才管）；")
print("  Python 的 `a[i]` 是**一定有答案**的：要么给值，要么 IndexError。")
print("  所以「切片不报错」这件事不能反过来用：a[5:9] 给空 ≠ a[5] 也给空 —— 它给异常。")
print("  另一个方向也一样静悄悄：a[3:1] 是 []，不是 [40, 30]。想要倒着取得写负步长（见下）。")

print()
print(SEP)
print("二、步长：a[::2] 挑偶数位，a[::-1] 整体反过来，a[::-2] 反向挑")
print(SEP)
b = list(range(10))
show("b", b)
show("b[::2]（隔一个取一个）", b[::2])
show("b[1::2]（从 1 开始隔一个）", b[1::2])
show("b[::-1]（反转，新对象）", b[::-1])
show("b[::-2]（反向隔一个）", b[::-2])
show("b[5:1:-1]（从 5 走到 2 停）", b[5:1:-1])
show("b[8:None:-1] 写成 b[8::-1]", b[8::-1])
print("→ 负步长的读法：stop 是**取不到**的那一格，所以 b[5:1:-1] 拿到 2 为止。")
print("  b[5:1:-1] 里有 5、4、3、2 四格，b[5:1] 是空的 —— 同一个 1:5 区间，")
print("  方向写在步长里，不写在 start/stop 的大小关系里。")
print(f"  反转是不是新对象？ b[::-1] is b → {b[::-1] is b}；")
print(f"  对没有反转的整段切片也一样：b[:] is b → {b[:] is b}（内容相等但对象不同）。")
print("  也就是说：**只要带冒号，一律新建外壳并抄一遍**，长度 O(k) 成本，")
print("  哪怕 k = n（a[:] 就是把整个指针数组 memcpy 一遍，你在 C 里得自己写）。")

print()
print(SEP)
print("三、切片赋值：能改长度，这是 C 里 memmove 那一整套的替身")
print(SEP)
c = list(range(10))
show("原始 c", c)
c[2:5] = ["a", "b", "c", "d"]        # 3 格换成 4 个 → 变长
show("c[2:5] = 4 个元素（换 3 填 4）", c)
print(f"    len 从 10 变成 {len(c)} —— 切片赋值的右边是「一个可迭代对象」，")
print("    它**不需要**和被切的段等长；不够就往前挪，多了就往后挤（一次 memmove）。")
c[2:6] = []
show("c[2:6] = []（这就是「删」）", c)
print("    给空可迭代对象 = 把这一段抹掉，len 直接少 4。等价于 del c[2:6]。")
d = list(range(6))
d[1:4] = "xyz"
show("d[1:4] = 'xyz'（右边是字符串）", d)
print("    字符串也是可迭代对象，所以这里塞进去的是 3 个单字符，而不是一个 'xyz'。")
print("    （和第 1 段末尾那个 append/extend 的区别一模一样：赋值看的是「拆开」。）")
e = list(range(6))
del e[1:4]
show("del e[1:4]", e)

print()
print(SEP)
print("四、带步长的切片赋值：唯一不能改长度的情形（报错原文）")
print(SEP)
f = list(range(5))
f[::2] = [10, 20, 30]                # f[::2] 正好是 3 格 → 合法
show("f[::2] = 3 个元素（格数刚好）", f)
try:
    g = list(range(5))
    g[::2] = [9]
except ValueError as exc:
    print(f"  g[::2] = 1 个元素          → ValueError: {exc}")
try:
    h = list(range(5))
    h[1:3] = 5
except TypeError as exc:
    print(f"  h[1:3] = 5（右边不是可迭代对象） → TypeError: {exc}")
print("→ 规矩：**只有步长为 1 的切片赋值能改长度**；带步长（含负步长）时左右必须等长。")
print("  道理很直白：`f[::2] = [...]` 是「把这些位置逐个覆盖」，位置集合是固定的，")
print("  没地方插新元素，也不可能凭空腾出地方。C 里等价于对固定下标集合逐个赋值。")

print()
print(SEP)
print("五、谁能切、谁不能切（sequence vs mapping 的分水岭，还带一个意外）")
print(SEP)
show("[10, 20, 30][0:2]（list）", [10, 20, 30][0:2])
show("(10, 20, 30)[0:2]（tuple）", (10, 20, 30)[0:2])
st = {10, 20}            # 先用变量接住，免得 3.14 对字面量 {10, 20}[0:1] 直接发 SyntaxWarning
dt = {"k": 1}
try:
    st[0:1]
except TypeError as exc:
    print(f"  set 切片 st[0:1]            → TypeError: {exc}")
try:
    dt[0:1]
except KeyError as exc:
    print(f"  dict 切片 dt[0:1]           → KeyError: {exc}   ← 注意！不是 TypeError")
print("→ 只有「有序可下标访问」的 sequence（list / tuple / str / bytes / range）能切。")
print("  set 直接拒绝：'set' object is not subscriptable —— 因为它没有顺序可言。")
print("  dict 最有意思：它**接受**下标语法（那是它的查表接口），而 slice 对象是哈希的，")
print("  于是 `d[1:2]` 被当成「拿 slice(1,2,None) 这个键去查」→ KeyError。")
print("  这个坑的真实面目：你以为在切片，其实在造一个永远查不到的键。")
print("  排查信号：报错是 KeyError: slice(...) 而不是 TypeError，就说明你切错了对象。")
print(f"  顺手确认 slice 真的可哈希：isinstance(hash(slice(1, 2, 3)), int) → "
      f"{isinstance(hash(slice(1, 2, 3)), int)}")
weird = {slice(1, 2): "这个键是个 slice"}
print(f"  甚至能当 dict 的键：d[slice(1, 2)] → {weird[slice(1, 2)]!r}")
