"""第 06 章第 18 节：isinstance 认户口，type() 认现形。

跑法：python3 o_isinstance_type.py

  isinstance(狗, 动物) → True  （沿继承链查，算「是不是某种」）
  type(狗) is 动物     → False （只看具体那个类）
写库时 99% 该用 isinstance；type() 用在「就是要精确这个类」的场合（比如工厂分发）。
"""


class Animal:
    pass


class Dog(Animal):
    pass


class ToyDog(Dog):
    pass


class Rock:
    pass


print("=" * 68)
print("一、三行看清差别")
print("=" * 68)
d = ToyDog()
print("isinstance(d, Animal)      →", isinstance(d, Animal), "← 认户口：曾祖是 Animal 就算")
print("type(d) is Animal          →", type(d) is Animal, "← 只认现形：你本体是 ToyDog")
print("type(d) is ToyDog          →", type(d) is ToyDog, "｜ isinstance(d, ToyDog) →", isinstance(d, ToyDog))

print()
print("=" * 68)
print("二、isinstance 支持元组（一条顶三条）")
print("=" * 68)
xs = [Dog(), Rock(), ToyDog()]
print([isinstance(o, (Dog, Rock)) for o in xs])
print("↑ 第二参数是元组时，「是其中任何一家的户口」都算 True")

print()
print("=" * 68)
print("三、type 的另一副面孔：三参数工厂（动态造类）")
print("=" * 68)
Dyn = type("Dyn", (Animal,), {"sound": lambda self: "叮"})
obj = Dyn()
print("type(Dyn).__name__ =", type(Dyn).__name__, "｜ isinstance(obj, Animal) →", isinstance(obj, Animal))
print("obj.sound() →", obj.sound())
print("↑ class Dyn(Animal): def sound... 编译完就是这个三元调用；「class 是字典」的另一半证据")

print()
print("=" * 68)
print("四、dataclass 也认：isinstance / type 照继承链")
print("=" * 68)
from dataclasses import dataclass


@dataclass
class Base:
    v: int


@dataclass
class Derived(Base):
    w: int = 0


dv = Derived(1, 2)
print("isinstance(dv, Base) →", isinstance(dv, Base), "｜ type(dv) is Base →", type(dv) is Base)
print("但判等不认户口！Base(1) == Derived(1, 2) →", Base(1) == Derived(1, 2), "（第 12 节：生成的 __eq__ 要求同类）")
