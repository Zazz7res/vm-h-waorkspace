"""第 06 章第 17 节：类变量在继承里的两种「共享」事故。

跑法：python3 n_shared_classvar.py

  写法一 Base.count += 1  —— 改的是类字典，所有子类/实例抬头看同一份，天然共享；
  写法二 self.count += 1  —— 读的时候落到类，写的时候写进实例字典，
         第一次 += 就悄悄给这个实例建了一个「同名遮挡物」（第 3 节的老朋友）。
"""


class ViaClass:
    n = 0

    def __init__(self):
        ViaClass.n += 1


class ViaSub(ViaClass):
    pass


class ViaSelf:
    n = 0

    def __init__(self):
        self.n += 1


class SharedBox:
    items = []          # 可变类变量：共享得更狠

    def add(self, x):
        self.items.append(x)   # 注意：这里没有赋值，append 直接改共享对象


print("=" * 68)
print("一、ViaClass：全类一张计数器")
print("=" * 68)
x1, x2 = ViaClass(), ViaClass()
xs = ViaSub()
print("两个实例 + 一个子类实例构造后：ViaClass.n =", ViaClass.n)
print("子类没自己的 n，抬头读到同一个：ViaSub.n =", ViaSub.n)
print("vars(ViaSub) 里有 n 吗：", "n" in vars(ViaSub), "← 子类字典没添过东西")

print()
print("=" * 68)
print("二、ViaSelf：+= 在实例上顺手建了遮挡物")
print("=" * 68)
s1, s2 = ViaSelf(), ViaSelf()
print("ViaSelf.n =", ViaSelf.n, "← 类上那份从头到尾没动过")
print("s1 的实例字典：", vars(s1), "｜ s2 的：", vars(s2))
print("s1.n =", s1.n, "（读的是自己的），s2.n =", s2.n, "（也读自己的）")
print("拆步骤看真相：self.n += 1 拆成「读类上的 → 写实例的」：")
fresh = ViaSelf()
print("  刚 new 出来 vars(fresh) =", vars(fresh), "（__init__ 里那次 += 已经写了它自己的 n）")

print()
print("=" * 68)
print("三、SharedBox：可变类变量，append 不建遮挡物，全体一起脏")
print("=" * 68)
b1, b2 = SharedBox(), SharedBox()
b1.add(999)
print("只给 b1 塞了一发：b1.items =", b1.items, "｜ b2.items =", b2.items)
print("b1.items is b2.items is SharedBox.items →",
      b1.items is b2.items is SharedBox.items, "← 三个名字一块字典里的同一个 list")
print("（和第 10 节可变默认值一个病根：共享的是对象，不是名字。第 02 章的老账。）")
