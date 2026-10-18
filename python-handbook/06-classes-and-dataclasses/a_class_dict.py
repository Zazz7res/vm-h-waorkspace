"""第 06 章第 2~3 节：类就是一个字典，实例是挂在它旁边的小字典。

跑法：python3 a_class_dict.py

这一节把「类到底是什么」拆开给你看：
  * class A: ... 编译完，解释器做的事就是把 A 里写的名字塞进一个 dict；
  * 你在外面看到的 A.__dict__ 是它的只读投影（mappingproxy）；
  * 每个实例另有一个自己的 dict（vars(实例)），只存「这个对象自己的属性」；
  * 查属性：先翻实例 dict，再沿类 dict（及其父类 dict）一层层翻，找不到才报 AttributeError。
所以「方法」不是什么特殊公民——它就是恰好被放在类 dict 里的一个函数。
"""


class Employee:
    """一个最小但五脏俱全的类。"""

    company = "Acme 制造"          # 类属性：全体 Employee 共用一份

    def __init__(self, name):      # 也是类字典里一个名字叫 __init__ 的函数
        self.name = name           # 实例属性：写进 vars(实例)

    def greet(self):
        return f"您好，我是 {self.name}，来自 {self.company}"


print("=" * 68)
print("一、Employee 的类字典里到底有什么")
print("=" * 68)
keys = list(vars(Employee))
print("vars(Employee) 的键（按写入顺序）：")
print(keys)
print("普通名字（不带双下划线）只有：", [k for k in keys if not k.startswith("__")])
print("company 在类字典里：", "company" in vars(Employee))
print("greet 在类字典里：", "greet" in vars(Employee), "→ 它是个", type(vars(Employee)["greet"]).__name__)

print()
print("=" * 68)
print("二、实例有自己的字典；类字典只是只读投影")
print("=" * 68)
ann = Employee("小安")
print("vars(ann) =", ann.__dict__)
print("ann.__dict__ is vars(ann)：", ann.__dict__ is vars(ann))
print("Employee.__dict__ 的类型：", type(Employee.__dict__).__name__, "（mappingproxy = dict 的只读门面）")

print()
print("=" * 68)
print("三、实例存同名属性 → 遮住类属性；del 掉 → 又露出来")
print("=" * 68)
print("遮挡前：ann.company =", ann.company, "｜ id(ann.company) == id(Employee.company)：",
      ann.company is Employee.company)
ann.company = "邪恶集团"
print("ann.company = '邪恶集团' 之后：")
print("  ann.company      =", ann.company, "← 查的是实例字典，查到就用它")
print("  Employee.company =", Employee.company, "← 类属性一根手指头都没被碰")
print("  vars(ann)        =", ann.__dict__, "← 遮挡物就是实例字典里的这个键")
bob = Employee("小鲍")
print("  bob.company      =", bob.company, "← 别的实例不受影响，照样落到类属性")
del ann.company
print("del ann.company 之后：")
print("  ann.company      =", ann.company, "← 实例字典里没这个键了，查找继续往下落到类")
print("  vars(ann)        =", ann.__dict__)
print("greet() 读的也是同一条查找链：", ann.greet())
