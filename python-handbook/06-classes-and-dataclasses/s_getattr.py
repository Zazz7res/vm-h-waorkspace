"""第 06 章第 22 节：__getattr__ 是兜底，__getattribute__ 是全量拦截（写错就死循环）。

跑法：python3 -u s_getattr.py（前半正常跑完，后半 RecursionError 崩掉，是本脚本的目的）

  __getattr__  ：正常查找链（实例→类→父类）全落空之后才叫它 —— 安全，适合做代理/转发；
  __getattribute__：每一次属性访问都必经它 —— 强大，但里面哪怕碰一下 self.任何东西，
                    又是走它自己 → 自己调自己 → RecursionError。
"""


class Spy:
    def __init__(self):
        self.real = "正主属性"

    def __getattr__(self, name):
        print(f"    （__getattr__ 被叫了：'{name}' 走正常查找没找到）")
        return f"伪造的 {name}"


print("=" * 68)
print("一、__getattr__：只在「查无此人」时被叫")
print("=" * 68)
sp = Spy()
print("读已有的 sp.real →", sp.real, "（__getattr__ 没出声，正常链路直接命中）")
print("读不存在的 sp.ghost →", sp.ghost)
print("读不存在的 sp.number42 →", sp.number42)


class Boom:
    def __init__(self):
        self.data = {"x": 1}

    def __getattribute__(self, name):
        # 想「拦截所有属性访问」的写法 —— 错得最经典的一种
        return self.__dict__[name]      # ← 取 self.__dict__ 本身也要走 __getattribute__！


print()
print("=" * 68)
print("二、__getattribute__ 写错的死循环现场（接下来崩）")
print("=" * 68)
print("b = Boom() 构造本身不崩（赋值走 __setattr__，不经过 __getattribute__），")
print("崩的是下一句读属性：")
b = Boom()
print("现在执行 b.x，看它自己咬自己：")
print(b.x)
print("这行也永远执行不到")
