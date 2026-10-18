#!/usr/bin/env python3
# ============================================================================
# j_closure.py —— 第 05 章第 13 节：闭包 = 函数 + 它出门时带的那几个 cell
#
# 判据只有一条：内层函数引用了**外层的局部名字**，那个名字就变成 cell（小盒子），
# 外层函数返回后盒子不销毁，由内层函数的 __closure__ 拽着。
# 关键性质：盒子是**共享**的、装的是**当前值**不是快照 —— 这就是第 14 节那个坑的根。
# ============================================================================

print("=== 1. 最小闭包：计数器 ===")


def make_counter():
    count = 0                      # 局部名字，被内层引用 → 编译期就变成 cell
    def hit():
        nonlocal count             # 不加 nonlocal 就是第 15 节那个 UnboundLocalError
        count += 1
        return count
    return hit


c1, c2 = make_counter(), make_counter()
print("  c1 三次：", c1(), c1(), c1())
print("  c2 独立（另一个盒子）：", c2())
print("  每个盒子归谁管：c1 和 c2 的 cell 是同一个吗 →", c1.__closure__[0] is c2.__closure__[0])

print()
print("=== 2. 编译器把闭包记在账本哪里 ===")
code = c1.__code__
print("  co_varnames（自己的局部表）=", code.co_varnames)
print("  co_freevars（从外面带进来的名字）=", code.co_freevars)
print("  make_counter 那一侧：co_cellvars（被内层盯上的名字）=", make_counter.__code__.co_cellvars)
print("  __closure__ 是", type(c1.__closure__).__name__, "，长度 =", len(c1.__closure__))
print("  盒子里现在装的值：c1.__closure__[0].cell_contents =", c1.__closure__[0].cell_contents)
print("  没有闭包名的函数 __closure__ 是 None：hit 之外看 greet 例子 →",
      make_counter.__closure__)

print()
print("=== 3. 盒子是共享的，不是快照：外面改一下里面立刻看见 ===")


def shared_box_demo():
    value = "初始"

    def reader():
        return value              # 只读也算引用 → value 被装进盒子

    value = "改过了"               # 外层自己改，用的是同一个盒子
    return reader


print("  外层 def 之后又赋值，内层读到的是：", shared_box_demo()())

print()
print("=== 4. 两个内层函数共用同一个盒子 ===")


def two_viewers():
    n = 10

    def plus():
        return n + 1

    def show():
        return n

    return plus, show


plus, show = two_viewers()
print("  plus() =", plus(), "｜ show() =", show())
print("  两个函数的盒子是同一个：", plus.__closure__[0] is show.__closure__[0])

print()
print("=== 5. 盒子可以被写（nonlocal），也可以被整个换掉 ===")


def make_toggle():
    state = False

    def flip():
        nonlocal state
        state = not state
        return state

    return flip


t = make_toggle()
print("  flip 四次：", t(), t(), t(), t())
print("  盒子里当前：", t.__closure__[0].cell_contents)

print()
print("=== 6. 不用 nonlocal 的等价写法：把账本装进可变对象（第 7 节那条规律）===")


def make_counter_list():
    box = [0]                      # box 本身不需要 nonlocal：我们没给它重新赋值，只改它的内容

    def hit():
        box[0] += 1
        return box[0]

    return hit


l1 = make_counter_list()
print("  三次：", l1(), l1(), l1())
print("  盒子里装的是那个 list：", l1.__closure__[0].cell_contents)
print("  两种写法对比：nonlocal 走「换内容」，可变对象走「原地改」；都能存状态，")
print("  但 nonlocal 版本一眼能看出这里有共享状态，list 版本要看两遍 —— 可读性算账也要记。")

print()
print("=== 7. 闭包 vs 类实例：同一件事的两种写法（回指第 06 章）===")


class CounterClass:
    def __init__(self):
        self.count = 0

    def hit(self):
        self.count += 1
        return self.count


cc = CounterClass()
print("  类版三次：", cc.hit(), cc.hit(), cc.hit())
lc = make_counter()
print("  闭包版三次：", lc(), lc(), lc())
print("  状态个数少、只要一个操作 → 闭包轻；状态多、操作多、要看一眼全貌 → 类。")
print("  内存账：闭包函数对象 + 一个 cell vs 实例 + __dict__（第 06 章 10 万实例那张表可类比）。")

print()
print("=== 8. 一句话判据 ===")
print("  看到「内层函数用了外层的名字」就先问：这个名字有没有被**赋值**？")
print("    没赋值（只读）→ 直接读盒子；")
print("    要赋值 → 必须 nonlocal（内层）或 global（模块级），否则就是第 15 节的坑。")
