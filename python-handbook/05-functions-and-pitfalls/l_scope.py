#!/usr/bin/env python3
# ============================================================================
# l_scope.py —— 第 05 章第 15 节：LEGB 与 ★UnboundLocalError（本章第一大坑）
#
# 查名字的顺序：L 局部 → E 封闭函数（闭包）→ G 模块全局 → B 内置。
# 但**赋值不参与查找**：编译器只问一句「这个函数里有没有对这个名字赋值」，
# 有 → 它就是局部名字（不管那行会不会执行）；于是「先读后写」当场炸 UnboundLocalError。
# ============================================================================

print("=== 1. 四层各有一个同名东西：看谁赢 ===")
name = "模块级（G）"


def where_sees():
    name = "函数级（L）"

    def inner():
        return name            # 找最近的封闭函数 → E
    return f"{name} / inner 读到：{inner()}"


print("  ", where_sees(), "｜ 外面这份还是：", name)
print("  内置那层（B）什么时候被读到：只在全局、封闭、局部都没有这个名字时，例如 len、print、int。")


def no_local_len():
    return len("四个字符")     # 一路找不到 L/E/G 里的 len → 落到 B


print("  ", no_local_len())

print()
print("=== 2. ★只读不算写，一赋值就变局部：UnboundLocalError 现场 ===")
counter = 0


def bad_add():
    counter = counter + 1       # 右边的 counter 被认定是「还没赋值的局部名」
    return counter


try:
    bad_add()
except UnboundLocalError as e:
    print("  bad_add：", type(e).__name__, "|", e)
    print("  这条异常还能直接问它缺谁：e.name =", e.name,
          "｜ 它是 NameError 的子类吗 →", isinstance(e, NameError))


def good_global():
    global counter              # 明确说：这个名字就是模块级那个
    counter += 1
    return counter


print("  good_global 三次：", good_global(), good_global(), good_global(), "｜ 模块级 counter =", counter)

print()
print("=== 3. 编译器不看会不会执行，只看有没有赋值 ===")
flag = "全局值"


def never_runs():
    if False:
        flag = "局部"           # 这行永远跑不到，但编译期已经判定 flag 是局部
    return flag


try:
    never_runs()
except UnboundLocalError as e:
    print("  never_runs：", type(e).__name__, "|", e)
print("  证据（账本，第 3 节那套）：flag 在 never_runs 的局部表里吗 →",
      "flag" in never_runs.__code__.co_varnames)


def only_reads():
    return flag


print("  只读的函数就不受影响：only_reads() =", only_reads(),
      "｜ 它的局部表 =", only_reads.__code__.co_varnames)

print()
print("=== 4. 内层要改外层的值：nonlocal（模块级用 global）===")


def outer_box():
    total = 0

    def bad_inner(n):
        total += n              # 忘了 nonlocal → 同一个坑，换个层级
        return total

    def good_inner(n):
        nonlocal total
        total += n
        return total

    return bad_inner, good_inner


bad_inner, good_inner = outer_box()
try:
    bad_inner(1)
except UnboundLocalError as e:
    print("  闭包里不写 nonlocal：", type(e).__name__, "|", e)
print("  写了 nonlocal：", good_inner(1), good_inner(2), good_inner(3))
print("  nonlocal 不能指向模块级（那是 global 的活），这条连编译都过不去：")
try:
    compile("counter = 0\n"
            "def outer():\n"
            "    def inner():\n"
            "        nonlocal counter\n"
            "        return counter\n"
            "    return inner", "<示例>", "exec")
    print("   竟然编译通过")
except SyntaxError as e:
    print("   SyntaxError |", e.msg)
print("  反过来，global 也管不到封闭函数那一层（global 只认模块级），所以两种声明不能互换。")

print()
print("=== 5. 同一层里同名遮蔽：参数名、循环变量、推导式 ===")


def param_and_loop(items):
    items = [i * 2 for i in items]      # 推导式里的 i 不外泄，items 被重绑定
    for items in range(3):              # ← 参数名被循环变量踩掉（能跑，读代码的人最容易被骗）
        pass
    return items


print("  param_and_loop([1,2,3]) 返回 =", param_and_loop([1, 2, 3]), "（已经不是那张表了）")


def cleaner(items):
    return [x * 2 for x in items]


print("  换个名字就干净：cleaner([1,2,3]) =", cleaner([1, 2, 3]))

print()
print("=== 6. class 那一层不在 LEGB 里（第 06 章那条规律在这边的样子）===")
module_v = "模块级"


class C:
    shared = "类里"

    def from_module(self):
        return module_v          # 直接跳到 G：能读到

    def from_class(self):
        try:
            return shared        # 类里那层被跳过
        except NameError as e:
            return f"NameError | {e}"

    def via_self(self):
        return self.shared       # 正确写法：走属性查找


print("  from_module →", C.from_module(None))
print("  from_class  →", C.from_class(None))
print("  via_self    →", C.via_self(C()))

print()
print("=== 7. 判据三句话 ===")
print("  ① 读名字：L→E→G→B，class 那一层**不算一层**；")
print("  ② 这个函数里只要出现过 `名字 = `、增量赋值、for/with/def/import 的绑定目标，")
print("     这个名字整层就是局部（编译期判定，不看执行路径）；")
print("  ③ 想跨层写：模块级 global、封闭层 nonlocal、对象属性 self.x（第 06 章第 17 节那个遮挡物）。")
