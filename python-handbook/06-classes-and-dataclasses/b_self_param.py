"""第 06 章第 4 节：self 没有任何魔法，就是第一个参数。

跑法：python3 b_self_param.py

obj.f(x) 做的事 = 取出「类字典里的 f」+「把 obj 塞进第一个参数」+「调用」。
这一节做三件事：
  1. A.f(obj) 和 obj.f() 亲手验证等价；
  2. 拆绑定方法对象，看 __func__ / __self__ 两个零件；
  3. 反证：同一个函数挂在实例属性上（不进类字典），自动塞 self 的机制就没了。
"""


class Counter:
    def __init__(self, start=0):
        self.n = start

    def bump(self, step=1):
        self.n += step
        return self.n


def standalone(counter_obj, step=1):
    """长得像方法的普通函数：Python 不看长相，只看它住在哪个字典里。"""
    counter_obj.n += step
    return counter_obj.n


print("=" * 68)
print("一、两条路调用同一个函数体")
print("=" * 68)
c1, c2 = Counter(0), Counter(0)
print("Counter.bump(c1, 5) =", Counter.bump(c1, 5), "（类上取函数，手动把实例当第一个参数传）")
print("c2.bump(5)          =", c2.bump(5), "（语法糖，干的同一件事）")
print("两次调用后状态一致：", c1.n == c2.n)

print()
print("=" * 68)
print("二、拆开绑定方法：它就是 (函数, 实例) 一对")
print("=" * 68)
b = c1.bump
print("type(c1.bump).__name__ =", type(b).__name__)
print("b.__func__ is Counter.bump：", b.__func__ is Counter.bump)
print("b.__self__ is c1：", b.__self__ is c1)
before = c1.n
print("b(3) =", b(3), "｜ c1.n 从", before, "推进到", c1.n, "← 动的正是 __self__ 那个实例")

print()
print("=" * 68)
print("三、把函数当值传出去 —— 它收不收 self 一眼看清")
print("=" * 68)


def apply_three(fn, arg):
    """fn 是谁、从哪来的我不管，我就调它三次。"""
    return [fn(arg), fn(arg), fn(arg)]


c3 = Counter(0)
print("把绑定方法 c3.bump 传进去（每次自动带 c3）：", apply_three(c3.bump, 1))
print("c3.n 现在是：", c3.n)
print("把类上的裸函数 Counter.bump 传进去：没人塞实例，第一个位置被参数 1 占了，")
print("1.n 当场崩，报错原文：")
try:
    apply_three(Counter.bump, 1)
except AttributeError as e:
    print("  原文：", e)
print("正确姿势：裸函数自己补第一参数 →", [Counter.bump(c3, 1), Counter.bump(c3, 1)],
      "（连推两步，c3.n =", c3.n, "）")

print()
print("=" * 68)
print("四、反证：函数住在实例字典（不是类字典）时，self 不会自动塞")
print("=" * 68)
c4 = Counter(0)
c4.handy = standalone          # standalone 需要 (counter_obj, step) 两个参数
try:
    c4.handy()
except TypeError as e:
    print("c4.handy() 报错原文：", e)
    print("↑ 注意：如果 handy 是「方法」，counter_obj 会被自动塞好；这里没有，")
    print("  说明自动塞只发生在「名字从类字典里查出来」那条路上。")
print("手动把实例补上就能跑：c4.handy(c4, 9) =", c4.handy(c4, 9), "｜ c4.n =", c4.n)
print("对照：c4.bump(1) 不用补 self，因为 bump 在类字典里 →", c4.bump(1))
print("结论：方法 = 恰好放在类字典里的函数；其余都是普通函数。")
