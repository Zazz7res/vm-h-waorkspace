"""第 06 章第 15 节：钻石继承 —— __mro__ 是名单，super() 照名单走。

跑法：python3 m_mro_super.py

A
B  C   （都继承 A）
D      （同时继承 B、C）

直觉里 D().greet() 应该走 D→B→A；真机走的是 D→B→C→A ——
因为每个类只有一张「MRO 名单」，super() 的意思是「名单里我的下一家」，
而不是「我的亲爹」。每层打印一行，顺序当场给你看。
"""


class A:
    def greet(self):
        print("  A.greet 被调（名单终点站）")
        return "A"


class B(A):
    def greet(self):
        print("  B.greet：我该喊 super() 了 —— 注意接下来不是 A！")
        super().greet()


class C(A):
    def greet(self):
        print("  C.greet：轮到我兜底了")
        super().greet()


class D(B, C):
    def greet(self):
        print("  D.greet：先干自己的，再 super()")
        super().greet()


print("=" * 68)
print("一、名单本体")
print("=" * 68)
print("D.__mro__ →", [c.__name__ for c in D.__mro__])
print("D.__bases__ →", [c.__name__ for c in D.__bases__], "← 亲爹只有 B、C 两个")
print("B.__mro__  →", [c.__name__ for c in B.__mro__])

print()
print("=" * 68)
print("二、真实调用链（打印顺序就是执行顺序）")
print("=" * 68)
print("D().greet():")
D().greet()
print("↑ 四行按 D→B→C→A 出现：C 抢在 A 前面，因为它是名单里 B 的下一家。")
print("  这就是「super 不是调用父类，是沿 MRO 往后走」的铁证。")

print()
print("=" * 68)
print("三、同一份 B.greet，站的位置不同，super() 找的人就不同")
print("=" * 68)
d_inst, b_inst = D(), B()
print("super(B, d_inst).greet.__func__ is C.greet →", super(B, d_inst).greet.__func__ is C.greet)
print("super(B, b_inst).greet.__func__ is A.greet →", super(B, b_inst).greet.__func__ is A.greet)
print("↑ 同一段 super() 代码，实例是 D 就去找 C，实例是 B 就去找 A：")
print("  找谁 = 实例所属类的 MRO 里 B 的下一家，与「写在哪个类里」无关。")
