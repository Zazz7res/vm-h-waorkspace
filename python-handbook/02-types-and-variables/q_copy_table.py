"""第 02 章第 20 节：想「换一份」的时候该写什么 —— 三种复制的真实边界。

跑法：python3 q_copy_table.py

因为变量只是标签，「赋值」永远不复制对象。真要复制得明说。
标准库 copy 模块给两档：copy() 浅、deepcopy() 深。浅拷贝只复制最外层的壳，
里面的元素还是共用 —— 这正是新手以为「我已经复制了」的地方。
"""
import array
import copy


def same(x, y):
    return id(x) == id(y)


print("=" * 68)
print("一、三种「得到一个变量」的写法，结果完全不同")
print("=" * 68)
original = {"tags": ["a", "b"], "count": 1}
alias = original                    # 贴标签：不复制
shallow = copy.copy(original)       # 浅拷贝：复制最外层
deep = copy.deepcopy(original)      # 深拷贝：递归复制能复制的

print("赋值来的 alias 是同一个对象吗？", same(alias, original))
print("浅拷贝 shallow 是新对象吗？", not same(shallow, original))
print("深拷贝 deep 是新对象吗？", not same(deep, original))
print("但浅拷贝里面那层 list 还是共用的：", same(shallow["tags"], original["tags"]))
print("深拷贝里面那层也是新的：", not same(deep["tags"], original["tags"]))

print()
original["tags"].append("改了原件的内层")
print("改原件内层 → alias 看到：", alias["tags"])
print("改原件内层 → shallow 也看到：", shallow["tags"], "← 浅拷贝的痛点")
print("改原件内层 → deep 不受影响：", deep["tags"])
original["count"] = 99
print("外层换成新值 → alias =", alias["count"], "；shallow =", shallow["count"],
      "；deep =", deep["count"])
print("→ 外层换值是贴标签，只影响还指着旧对象的那些名字；内层就地改，所有共用者都看见。")

print()
print("=" * 68)
print("二、不可变对象「复制」就是返回自己（复制没意义）")
print("=" * 68)
for value in [1, "abc", (1, 2), None, True, 3.5, frozenset()]:
    print("  %-12r  copy 之后同一个对象？ %-5s   deepcopy 之后同一个对象？ %s"
          % (value, same(copy.copy(value), value), same(copy.deepcopy(value), value)))
print("→ int / str / tuple / None / bool 这些不可变的，复制返回本体：白多一张标签，不浪费内存。")

print()
print("=" * 68)
print("三、含可变元素的 tuple：copy 返回本体，deepcopy 却造新的")
print("=" * 68)
inner = [1, 2]
tp = (inner,)
tp_copy = copy.copy(tp)
tp_deep = copy.deepcopy(tp)
print("copy.copy(tuple) 是新对象吗？", not same(tp_copy, tp))
print("copy.deepcopy(tuple) 是新对象吗？", not same(tp_deep, tp))
print("deepcopy 之后里面的 list 换了吗？", not same(tp_deep[0], tp[0]))
print("→ 壳虽然不可变，但里面元素被换了新对象，所以壳必须重造 —— 这就是两者的分界线。")

print()
print("=" * 68)
print("四、切片 / list() / dict() 全是浅拷贝（最容易踩）")
print("=" * 68)
rows = [[1, 2], [3, 4]]
sliced = rows[:]
built = list(rows)
print("rows[:] 是新 list 吗？", not same(sliced, rows), "；内层共用吗？", same(sliced[0], rows[0]))
print("list(rows) 同理，内层共用吗？", same(built[0], rows[0]))
sliced[0].append(99)
print("改 sliced 的内层 → rows =", rows, " ← 所谓「复制了二维数组」是假的")
independent = [[c for c in row] for row in rows]
rows[0].append(-1)
print("要真的独立：一层层列表推导 → 推出来的 =", independent, "；原件 =", rows, "（内层不共用了）")
snapshot = copy.deepcopy(rows)
rows[0].append(-1)
print("或者 deepcopy 之后改原件 → 快照 =", snapshot, "；原件 =", rows)

print()
print("=" * 68)
print("五、其他类型的「要一份独立副本」写法（本机都能跑）")
print("=" * 68)
st = "你好"
print("str 不可变，切片就是本体：st[:] is st →", st[:] is st)
bs = b"abc"
mut = bytearray(bs)
mut[0] = 65
print("bytes 不可变，要改就转 bytearray：", bytes(mut), "（原 bs 还是", bs, "）")
nums = array.array("i", [1, 2, 3])
print("array 的切片是独立副本吗？", not same(nums[:], nums))
sa = {1, 2}
print("set 的两种复制都给新外壳：", not same(sa.copy(), sa), not same(set(sa), sa))
