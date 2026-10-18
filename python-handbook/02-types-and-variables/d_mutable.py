"""第 02 章第 4 节：可变 / 不可变 —— list 是原地改，int 和 str 只能换对象。

跑法：python3 d_mutable.py
（本节证据就是这段脚本的输出，demo.sh 里有对应小节。）
"""


def same(x, y):
    return id(x) == id(y)


print("=" * 68)
print("一、list：原地改，对象没换")
print("=" * 68)
xs = [1, 2, 3]
alias = xs                 # 两个标签贴同一个对象（C 里做不到：那是倒一份值）
xs.append(4)
xs[0] = 99
print("alias 看到的：", alias)
print("xs 看到的：  ", xs)
print("alias 和 xs 还是同一个对象吗？", same(xs, alias))
print("→ 这就是 Python 里最容易被咬的一步：改一个，另一个跟着变。")

print()
print("=" * 68)
print("二、int / str：想「原地改」根本不存在")
print("=" * 68)
n = 10
before = n
n = n + 1
print("n =", n, "；n + 1 生成的新对象和旧对象是同一个吗？", same(n, before))
print("（其实 10 和 11 都在小整数缓存里，这里比的必须是变量：见第 7 节）")

s = "abc"
old = s
s = s + "d"
print("字符串拼接后，标签指向的还是原来那个 str 吗？", same(s, old))
try:
    s[0] = "X"
except TypeError as err:
    print("直接按下标改字符串，报错原文：", err)
print("→ str 的每个方法都返回新对象，这是「不可变」的代价与好处：")
print("  好处是它可以当 dict 的键、可以被安全共享；代价是循环里拼字符串很贵（10 章实测）。")

print()
