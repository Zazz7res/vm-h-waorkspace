"""第 06 章第 21 节：__call__ —— 让实例本身变成「可调用」。

跑法：python3 r_callable.py

f(x) 的本质是 type(f).__call__(f, x)。函数能这么写是因为函数类定义了 __call__；
普通类没有，所以「实例不能调用」不是天条，补上 __call__ 就行。
"""
from collections.abc import Callable


class Multiplier:
    def __init__(self, k):
        self.k = k

    def __call__(self, x):
        return self.k * x


class Tally:
    """带状态的「函数」：每次调用记一笔。"""

    def __init__(self):
        self.hits = 0

    def __call__(self):
        self.hits += 1
        return self.hits


m2 = Multiplier(2)
m5 = Multiplier(5)
t = Tally()

print("=" * 68)
print("一、实例直接带括号调用")
print("=" * 68)
print("m2(10) →", m2(10), "｜ m5(10) →", m5(10))
print("等价写法 type(m2).__call__(m2, 10) →", type(m2).__call__(m2, 10))
print("顺手塞进 map：list(map(m2, [1, 2, 3])) →", list(map(m2, [1, 2, 3])))

print()
print("=" * 68)
print("二、有状态的调用对象：替代闭包的另一种写法")
print("=" * 68)
print("t() ×3 →", [t(), t(), t()], "｜ t.hits =", t.hits)
print("isinstance(m2, Callable)（collections.abc）→", isinstance(m2, Callable))
print("普通实例呢：isinstance(42, Callable) →", isinstance(42, Callable),
      "｜ isinstance(t, Callable) →", isinstance(t, Callable))
print("三参数 type() 造类时给上 __call__ 也能用：")
Invoker = type("Invoker", (), {"__call__": lambda self: "我也能被调用"})
print("  Invoker()() →", Invoker()())
