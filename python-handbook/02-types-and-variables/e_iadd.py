"""第 02 章第 5 节：+= 的两种命运（有没有 __iadd__ 决定一切）。

跑法：python3 e_iadd.py
（本节证据就是这段脚本的输出，demo.sh 里有对应小节。）
"""


def same(x, y):
    return id(x) == id(y)


print("=" * 68)
print("一、+= 对 list 和 str 是两种命运（用 __iadd__ 解释）")
print("=" * 68)
lst = [1, 2]
lst_id = id(lst)
lst += [3]
print("list 走 += 之后对象换了吗？", id(lst) != lst_id)
print("  → 没换：list.__iadd__ 原地吸收，返回的还是自己")

txt = "ab"
txt_id = id(txt)
txt += "c"
print("str 走 += 之后对象换了吗？", id(txt) != txt_id)
print("  → 换了：str 没有真正的原地加法，只能造一个新 str 再贴标签")

tpl = (1, 2)
tpl_id = id(tpl)
tpl += (3,)
print("tuple 走 += 之后对象换了吗？", id(tpl) != tpl_id)
print("  → 也换了：tuple 不可变，+= 等价于 tpl = tpl + (3,)")

print()
print("两个写法看着一样，语义完全不同：")
print("  lst += [3]   等价于 lst.__iadd__([3])，改的是 lst 指向的那个对象本体")
print("  txt += 'c'   等价于 txt = txt + 'c'，造新对象并把标签撕过来")
print("所以：多个标签贴着同一个 list 时，+= 会影响所有标签；贴着 str 时不会。")

print()
print("自己写一个类来验证这个协议（谁实现了 __iadd__，+= 才可能原地）：")


class InPlace:
    """实现了 __iadd__：+= 之后还是同一个对象。"""

    def __init__(self):
        self.hits = 0

    def __iadd__(self, other):
        self.hits += other
        return self            # 返回 self → 标签不用换


class NewObject:
    """没实现 __iadd__：+= 退回普通 +，返回一个新对象。"""

    def __init__(self, tag):
        self.tag = tag

    def __add__(self, other):
        return NewObject(self.tag + other)


p = InPlace()
pid = id(p)
p += 5
q = p                        # 再贴一个标签
p += 3
print("InPlace：+= 之后对象换了吗？", id(p) != pid, "；第二个标签看到的 hits =", q.hits)
r = NewObject("a")
rid = id(r)
r += "b"
print("NewObject：+= 之后对象换了吗？", id(r) != rid, "；r.tag =", r.tag)

print()
