"""第 06 章第 19 节：__init_subclass__ 自动登记 + ABC 拦住抽象类实例化。

跑法：python3 p_subclass_hooks.py

  __init_subclass__ 是「子类一被 class 语句造出来就自动跑的钩子」——插件表不用手登记；
  ABC + @abstractmethod 则反过来：父类拒绝被实例化，报错原文最后一并抄出。
"""
import abc


class Plugin:
    registry = []

    def __init_subclass__(cls, name=None, **kwargs):
        super().__init_subclass__(**kwargs)
        cls.registry.append(name or cls.__name__)

    def run(self):
        return f"{self.__class__.__name__} 在跑"


class Importer(Plugin, name="导入器"):
    pass


class Exporter(Plugin):
    pass


print("=" * 68)
print("一、__init_subclass__：class 语句执行的那一刻就被钩")
print("=" * 68)
print("registry =", Plugin.registry, "← 没人调用登记函数，定义子类时自动进表")
print("跑起来：", Importer().run(), "｜", Exporter().run())
print("__init_subclass__ 也住在类字典里，类型是", type(vars(Plugin)["__init_subclass__"]).__name__,
      "← 它自己也不过是类字典里的一个物件（第 2 节的延伸）")

print()
print("=" * 68)
print("二、ABC：抽象类不许实例化（捕获版原文；裸 traceback 见 demo 下一条命令）")
print("=" * 68)


class Shape(abc.ABC):
    @abc.abstractmethod
    def area(self):
        ...


try:
    Shape()
except TypeError as e:
    print("TypeError:", e)


class Square(Shape):
    def __init__(self, side):
        self.side = side

    def area(self):
        return self.side ** 2


print("补了 area 就能生：Square(3).area() =", Square(3).area())
print("isinstance(Square(1), Shape) →", isinstance(Square(1), Shape))
