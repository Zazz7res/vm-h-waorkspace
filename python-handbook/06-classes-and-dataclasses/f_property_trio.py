"""第 06 章第 8 节：@property —— 读起来像属性，写起来是函数。

跑法：python3 f_property_trio.py

先给一个只有 getter 的属性，看赋值/删除分别报什么原文；
再上 getter/setter/deleter 三件套；
最后对照「方法调用要括号、property 不要括号」在调用方的写法差别。
"""


class ReadOnly:
    """只写 getter 的属性：能读，不能写不能删。"""

    @property
    def serial(self):
        return 2026


class Product:
    """完整三件套：getter / setter / deleter。"""

    def __init__(self, price):
        self._price = price       # 约定：下划线开头 = 内部字段，property 是对外门面

    @property
    def price(self):
        return self._price

    @price.setter
    def price(self, value):
        if value < 0:
            raise ValueError(f"价格不能为负，收到 {value}")
        self._price = value

    @price.deleter
    def price(self):
        print("（deleter 被调用：把价格清账，回到默认 0）")
        self._price = 0


class Rect:
    """对照组：同一个类里放一个普通方法和一个 property。"""

    def __init__(self, w, h):
        self.w = w
        self.h = h

    def area(self):               # 普通方法
        return self.w * self.h

    @property
    def size(self):               # property
        return self.w * self.h


r = ReadOnly()

print("=" * 68)
print("一、只有 getter：读 OK，写/删各自报什么原文")
print("=" * 68)
print("r.serial =", r.serial, "← 读没问题")
try:
    r.serial = 999
except AttributeError as e:
    print("赋值 r.serial = 999 → AttributeError:", e)
try:
    del r.serial
except AttributeError as e:
    print("删除 del r.serial → AttributeError:", e)

print()
print("=" * 68)
print("二、property 在类字典里是个什么物件")
print("=" * 68)
print("type(vars(ReadOnly)['serial']).__name__ =", type(vars(ReadOnly)["serial"]).__name__)
print("vars(ReadOnly)['serial'].getter 是函数吗：", vars(ReadOnly)["serial"].getter.__name__)

print()
print("=" * 68)
print("三、Product 三件套：setter 挡脏数据，deleter 清账")
print("=" * 68)
mp = Product(10)
print("读 mp.price =", mp.price)
mp.price = 25
print("mp.price = 25 之后读：", mp.price, "（setter 里我们改的是 _price）")
print("实例字典：", vars(mp), "← 对外名字是 price，内部存的是 _price")
try:
    mp.price = -1
except ValueError as e:
    print("mp.price = -1 → ValueError:", e)
del mp.price
print("del mp.price 之后：", mp.price)

print()
print("=" * 68)
print("四、调用方写法差别：obj.area vs obj.size")
print("=" * 68)
rc = Rect(3, 4)
print("rc.area 是什么类型：", type(rc.area).__name__, "← 不带括号只是拿到绑定方法，没算")
print("rc.area() =", rc.area(), "← 方法要括号才执行")
print("rc.size =", rc.size, "← property 不带括号，直接就是算好的值")
print("误写成 rc.size() 会怎样：")
try:
    rc.size()
except TypeError as e:
    print("  原文：", e)
print("误写成 print(rc.area)（忘加括号）：打印出来的是", type(rc.area).__name__, "对象而不是数，")
print("这是新手最常见的「property 与方法混用」事故现场。")
