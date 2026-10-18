"""第 06 章第 7 节：__eq__ 的代价 —— 你定义了相等，hash 就被没收。

跑法：python3 e_eq_hash.py

规则（不是 dataclass 特有，是所有类的规则）：
  类里一旦写了 __eq__，Python 就自动把 __hash__ 设成 None，
  因为「相等」的定义变了之后，原来那套「按对象身份算的哈希」就自相矛盾了
  （相等必须同哈希）。于是这个对象既进不了 set，也当不了 dict 的键。
三种踢法（set / dict 键 / hash()）报的原文各不相同，都打出来看。
"""


class Point:
    """只有 __eq__：能判等，但废了哈希。"""

    def __init__(self, x, y):
        self.x = x
        self.y = y

    def __repr__(self):
        return f"Point({self.x}, {self.y})"

    def __eq__(self, other):
        return isinstance(other, Point) and (self.x, self.y) == (other.x, other.y)


class HashedPoint(Point):
    """补上 __hash__：判等规则不变（继承父类 __eq__），哈希恢复。"""

    def __hash__(self):
        return hash((self.x, self.y))


p1, p2 = Point(1, 2), Point(1, 2)
p3 = Point(3, 4)

print("=" * 68)
print("一、__eq__ 本身是好的：字段相等就相等")
print("=" * 68)
print("p1 == p2 →", p1 == p2, "｜ p1 == p3 →", p1 == p3)

print()
print("=" * 68)
print("二、代价：__hash__ 被自动设成 None")
print("=" * 68)
print("Point.__hash__ is None →", Point.__hash__ is None)
print("（注意不是「没继承到」，是解释器在类创建时主动塞的 None）")

print()
print("=" * 68)
print("三、三种踢法，三种报错原文")
print("=" * 68)
for label, fn in [
    ("放进 set：", lambda: {p1}),
    ("当 dict 键：", lambda: {p1: "甲"}),
    ("直接 hash()：", lambda: hash(p1)),
]:
    try:
        fn()
    except TypeError as e:
        print(label, "TypeError:", e)

print()
print("=" * 68)
print("四、补 __hash__ 之后：set / dict 都活了")
print("=" * 68)
h1, h2, h3 = HashedPoint(1, 2), HashedPoint(1, 2), HashedPoint(3, 4)
pool = {h1, h2, h3}
print("set 里有几个：", len(pool), "← h1、h2 判等又同哈希，被当成同一个，去重成功")
lookup = {h1: "坐标 1,2 的备注"}
print("用另一个等值新实例去查 dict：", lookup[HashedPoint(1, 2)], "← 靠 __eq__ + __hash__ 配合命中")
print("哈希值相等吗：", hash(h1) == hash(h2), "← 这就是「相等必须同哈希」约定的兑现")

print()
print("=" * 68)
print("五、对照：完全不写 __eq__ 的类，默认可哈希（按对象身份）")
print("=" * 68)


class Plain:
    def __init__(self, v):
        self.v = v


q1, q2 = Plain(1), Plain(1)
print("q1 == q2 →", q1 == q2, "｜ hash(q1) == hash(q2) →", hash(q1) == hash(q2))
print("放进 set：", len({q1, q2}), "个 ← 两个不同对象，谁也去重不了谁")
print("结论：默认的「相等」是「是不是同一个对象」；你一改判等规则，就必须同批改哈希规则。")
