"""第 02 章第 18 节：作用域最小版 —— 只证「函数里赋值会让这个名字整个函数都算局部」。

跑法：python3 p_scope_min.py
（demo 第 18 节先跑一条一模一样的 -c 版本，那条不接异常，退出码是 1；本脚本把异常接住了所以是 0。）

LEGB 查找链、闭包、cell/free、推导式自己的作用域，全部留给第 05 章。
"""
import dis

count = 0


def read_only():
    """只读全局名字：没问题。"""
    return count


def read_then_write():
    """先读后写同一个 count：编译期就看到函数里有对它的赋值，于是它是局部的。"""
    print("这一行会打印，下一行才炸")
    count = count + 1
    return count


def annotated_wrong():
    """加个类型注解也救不了：注解不改变作用域判定。"""
    n: int = n + 1
    return n


print("=" * 68)
print("一、只读全局：没问题")
print("=" * 68)
print("read_only() =", read_only())
print("globals() 里有 count 吗？", "count" in globals())
print("函数对象自己不保存全局值，它在被调用的那一刻去模块的 globals dict 里取：")
count = 42
print("改了全局之后再调：read_only() =", read_only())

print()
print("=" * 68)
print("二、函数里赋值 → 这名字在整个函数里都算局部 → 读它就炸")
print("=" * 68)
try:
    read_then_write()
except UnboundLocalError as err:
    print("UnboundLocalError 原文（本节主角）：")
    print("  ", err)

print()
print("换成带注解的写法，一模一样：")
try:
    annotated_wrong()
except UnboundLocalError as err:
    print("  ", err)

print()
print("字节码层看得最清楚：编译期就把 count 定成了 LOAD_FAST/STORE_FAST（局部槽），")
print("而不是 LOAD_NAME/STORE_NAME（查模块 globals dict）：")
dis.dis(read_then_write)

print()
print("=" * 68)
print("三、两种修法（这里只演示「存在」，为什么这样留给第 05 章）")
print("=" * 68)


def with_global():
    global count
    count = count + 1
    return count


print("global 声明之后写全局名字：with_global() =", with_global(), "；全局 count 现在是", count)


def outer_demo():
    seen = 0

    def inner():
        nonlocal seen
        seen = seen + 1
        return seen

    return inner()


print("nonlocal 声明之后写外层函数的名字：outer_demo() =", outer_demo())
print("两个关键字都只是「别把这个名字当本地的」，不是「复制一份进来」。")

print()
print("=" * 68)
print("四、del 在函数里删的是局部名字，下一句读它就是 UnboundLocalError")
print("=" * 68)


def del_local():
    tmp = [1, 2]
    del tmp
    return tmp


try:
    del_local()
except UnboundLocalError as err:
    print("  ", err)
print("→ 和第 16 节连起来了：del 删名字；对象要是没人指着就当场释放。")

print()
print("=" * 68)
print("五、类体里也是一层独立名字空间（边界摆一下，内容留给第 06 章）")
print("=" * 68)


class InClass:
    base = 10
    doubled = base * 2

    def method(self):
        return self.base


print("InClass.doubled =", InClass.doubled, "← 类体里 base 那一刻可见")
print("InClass().method() =", InClass().method(), "← 方法里要靠 self，裸写 base 会 NameError")
