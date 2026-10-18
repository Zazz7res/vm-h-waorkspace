#!/usr/bin/env python3
# ============================================================================
# n_lambda.py —— 第 05 章第 17 节：lambda 就是 def 的表达式版（它没有别的神通）
#
# 三条边界：① 只能写**一个表达式**（不能有语句、不能有赋值）；
#          ② 造出来的还是普通函数对象，只是 __name__ 变成 '<lambda>'；
#          ③ 「不要把它赋值给名字」不是审美洁癖 —— 报错的 traceback 里会少掉能认的信息。
# ============================================================================

print("=== 1. lambda 是个对象，和 def 造的完全同类 ===")
square = lambda x: x * x


def square_def(x):
    return x * x


print("  type(square) =", type(square))
print("  square(7) =", square(7), "｜ square_def(7) =", square_def(7))
print("  __name__ 分别是：", repr(square.__name__), "和", repr(square_def.__name__))
print("  __code__.co_name：", repr(square.__code__.co_name), "和", repr(square_def.__code__.co_name))
print("  签名信息也在账本上：co_varnames =", square.__code__.co_varnames,
      "｜ __defaults__ =", square.__defaults__)

print()
print("=== 2. lambda 能用 def 能用的全部参数语法 ===")
print("  带默认值：", (lambda x, base=10: x * base)(3))
print("  收 *args/**kwargs：", (lambda *a, **k: (a, k))(1, 2, k=3))
print("  仅位置参数：", (lambda a, /: a * 2)(5))
print("  关键字专用：", (lambda *, a: a + 1)(a=41))
print("  嵌套（闭包）：", (lambda outer: (lambda inner: outer + inner)(1))(41))

print()
print("=== 3. 唯一的语法限制：里面只能有一个表达式 ===")
for label, src in [
    ("赋值语句", "f = lambda x: x = 1"),
    ("import 语句", "f = lambda x: import os"),
    ("return 关键字", "f = lambda x: return x"),
    ("带分号的复合", "f = lambda x: (x; x)"),
]:
    try:
        compile(src, "<示例>", "exec")
        print(f"  {label:14} → 竟然编译通过")
    except SyntaxError as e:
        print(f"  {label:14} → SyntaxError | {e.msg}")
print("  想「表达式的副作用」可以用三元/元组/推导式绕，但一旦绕就说明该写 def 了：")
print("  ", (lambda x: x if x > 0 else -x)(-5), "← 这种三元是合法的（它是表达式）")

print()
print("=== 4. 赋值给名字的代价：traceback 里那一行变成 <lambda> ===")


import traceback


def crash_lambda():
    f_lambda = lambda x: 1 / 0       # 故意的：只看报错里那一行长什么样
    f_lambda(1)


def crash_named():
    def named():
        return 1 / 0
    named()


for label, fn in (("lambda 版", crash_lambda), ("def 版", crash_named)):
    try:
        fn()
    except ZeroDivisionError:
        lines = traceback.format_exc().splitlines()
        print(f"  {label}的最深那一帧：", lines[-3].strip())
        print(f"    对应源码行：", lines[-2].strip())
print("  区别就在帧名上：一个写 in <lambda>（三个 lambda 长得一模一样），一个写 in named。")

print()
print("=== 5. lambda 真正合适的场合：一次性、当参数交出去的小函数 ===")
rows = [("苹果", 3), ("香蕉", 1), ("桃子", 2)]
print("  排序按第二项：", sorted(rows, key=lambda t: t[1]))
print("  换成 def 版：")


def by_count(t):
    return t[1]


print("  ", sorted(rows, key=by_count), "← 结果一样，只是要多写三行")
print("  赋值给名字（不推荐）：", list(map(lambda s: s.strip(), ["  a ", " b  "])))
print("  同一件事的推导式写法（更推荐，第 04 章讲过）：", [s.strip() for s in ["  a ", " b  "]])

print()
print("=== 6. 一句话判据 ===")
print("  lambda 只在「这里要一个函数、而且就一行」时用；")
print("  一旦它需要名字、需要文档、需要多条语句、需要出现在 traceback 里被认出，就换 def。")
