"""第 02 章第 15 节：赋值语义全家桶（多重赋值 / 交换 / 解包）。

跑法：python3 n_assign_pack.py

这些写法看着是语法糖，本质全是「同一时刻给多个名字贴标签」。
本节用 dis 看字节码，把「不用调包也能看见实现」这件事做实。
"""
import dis


def same(x, y):
    return id(x) == id(y)


print("=" * 68)
print("一、a = b = []：一个对象，两张标签（本章第二大坑）")
print("=" * 68)
a = b = []
print("a =", a, " b =", b)
print("a is b 吗？", a is b, "← 两个名字贴的是同一个 list")
a.append(1)
print("往 a 追加之后，b =", b, "← 它不是复制品，它就是同一个东西")
print("对照正确写法（各贴各的）：")
c, d = [], []
c.append(1)
print("c =", c, " d =", d, "；c is d =", c is d)
print("再对照一个「长得一样但不会咬人」的：")
e = f = 0
e = e + 1
print("整数版：e =", e, " f =", f, "← int 不可变，e + 1 造了新对象，f 还贴在 0 上")
print("→ 这个坑只在可变对象上咬人；和第 4 节是同一件事的两面。")

print()
print("=" * 68)
print("二、a, b = b, a 是怎么做到的（看字节码）")
print("=" * 68)
print("源码： a, b = b, a")
dis.dis(compile("a, b = b, a", "<交换>", "exec"))
print("读法：先把 b、a 都 LOAD 上栈（值已经在栈上，和名字脱钩），")
print("      SWAP 2 把栈顶两个换位置，然后依次 STORE_NAME 给 a、b。")
print("      因为两个值都先被读进栈里，所以不存在「先把 a 覆盖掉再读」的问题。")
print()
print("对照 C 风格的错误写法（右边先覆盖了左边）：")
p, q = 1, 2
p = q
q = p
print("不用元组：p =", p, " q =", q, "← 两个都变成 2，原来的 1 丢了")
p, q = 1, 2
p, q = q, p
print("用 a, b = b, a：  p =", p, " q =", q)
print()
print("顺带：3.14 里这个「元组」其实没被建出来 —— 上面那段字节码里没有 BUILD_TUPLE，")
print("只有 SWAP，这就是证据。编译器看穿了你只是要交换。")

print()
print("=" * 68)
print("三、解包：星号目标拿走剩下的")
print("=" * 68)
first, *rest = [10, 20, 30, 40]
print("first =", first, " rest =", rest, "；rest 的类型 =", type(rest).__name__)
*head, last = (10, 20, 30)
print("*head =", head, " last =", last)
low, *mid, high = range(5)
print("low =", low, " mid =", mid, " high =", high)
one, = [7]
print("注意那个逗号：one, = [7] → one =", one, "（写成 one = [7] 的话 one 是个 list）")
x, *y, z = (1, 2)
print("星号目标可以空手：x, *y, z = (1, 2) → x =", x, " y =", y, " z =", z)

print()
print("失败现场（这三条报错原文值得背下来）：")
for src in ["x, y = 1, 2, 3", "x, y, z = 1, 2", "*a, *b = [1, 2]", "x, y = 5"]:
    print("  %-16s →" % src, end=" ")
    try:
        exec(src)
    except Exception as err:
        print(type(err).__name__ + ":", err)
print("→ 解包要求「可迭代」且「数量对得上」。最后一条说明 int 根本不是可迭代对象。")

print()
print("=" * 68)
print("四、链式赋值和增强赋值也还是贴标签")
print("=" * 68)
m = n = o = 5
print("m = n = o = 5 → 三个名字同一个对象吗？", same(m, n) and same(n, o))
xs1 = xs2 = [1, 2]
xs1 += [3]
print("xs1 += [3] 之后 xs2 =", xs2, "← 原地吸收，两张标签都看见")
dis.dis(compile("xs1 += [3]", "<iadd>", "exec"))
print("→ 本机 3.14 打出来是 BINARY_OP 13 (+=)：普通加法和原地加法共用一条 BINARY_OP，")
print("  靠后面那个编号区分该用哪套槽位方法（先试 __iadd__，没有才退回 __add__）。")
