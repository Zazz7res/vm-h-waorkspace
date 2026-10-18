"""第 06 章第 20 节：名字改写（name mangling）—— __私 其实是 _类名__私。

跑法：python3 q_mangling.py

规则一句话：类体里写的「两个下划线开头、且结尾不足两个下划线」的名字，
编译时被改写成 _类名__名字。为的是子类撞名时互不干扰，不是「藏起来防你」。
前后都双下划线的（__init__、__version__）反而完全不动 —— 那是协议名，轮不到你私有。
"""


class Vault:
    __max = 100                      # 类体里的「私有」变量

    def __init__(self):
        self.__secret = "库房钥匙"    # 实例属性，也会被改写

    def __peek(self):                # 「私有」方法
        return self.__secret

    def show(self):
        return self.__peek()         # 类体内写 __peek，编译器替我们改成 _Vault__peek


class SubVault(Vault):
    __max = 777                       # 子类同名的 __max 改写成 _SubVault__max，不碰父亲的


v = Vault()
sv = SubVault()

print("=" * 68)
print("一、实例字典里的真名")
print("=" * 68)
print("vars(v) =", v.__dict__)
print("写 self.__secret，存的键其实是：", sorted(v.__dict__))

print()
print("=" * 68)
print("二、类字典里的真名；协议名原样保留")
print("=" * 68)
names = [k for k in vars(Vault) if "max" in k or "peek" in k or "secret" in k or k == "__init__"]
print("相关键：", sorted(names))
print("__max 变成 _Vault__max 在类字典：", "_Vault__max" in vars(Vault),
      "｜ 原名 __max 还在吗：", "__max" in vars(Vault))
print("__init__ 改写了吗：", "__init__" in vars(Vault) and "_Vault__init__" not in vars(Vault),
      "← 前后都双下划线 = 协议名，豁免")

print()
print("=" * 68)
print("三、外面想用「私」名字：字面名撞 AttributeError，真名倒是随便拿")
print("=" * 68)
try:
    v.__secret
except AttributeError as e:
    print("v.__secret →", e)
    print("  （这句写在模块里，没被改写，它真去找一个叫 '__secret' 的键 —— 而那个键从没存在过）")


class Nosy:
    @staticmethod
    def look(obj):
        return obj.__secret      # 写在类体里 → 被改写成 _Nosy__secret，改的是「代码所在的类」


try:
    Nosy.look(v)
except AttributeError as e:
    print("在别的类体里写 obj.__secret →", e)
    print("  （报错自报 _Nosy__secret：改写跟着「写代码的类」走，不是跟着被访问的对象）")
print("真名直取：v._Vault__secret →", v._Vault__secret, "← 拿到，mangling 不是安全盒")
print("v.show()（类体内正规通路）→", v.show())

print()
print("=" * 68)
print("四、改写的真实目的：父子撞名互不干扰")
print("=" * 68)
print("Vault.__max 的真身：", Vault._Vault__max, "｜ SubVault 的同名 __max 真身：", SubVault._SubVault__max)
print("_Vault__max 出现在 SubVault 自己的类字典里吗：", "_Vault__max" in vars(SubVault),
      "← 各存各的键，这就是改写的全部意义")
