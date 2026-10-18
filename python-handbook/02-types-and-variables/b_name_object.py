"""第 02 章第 2 节：名字和对象是两回事。

跑法：python3 b_name_object.py

C 的直觉是「a = 1 挖个洞，把 1 放进洞里」。Python 不是：
    a = 1 的意思是 —— 先造出一个 int 对象 1，然后把标签 a 贴上去。
所以 `a = 2` 不是「把旧盒子里的东西换成 2」，而是**把标签从旧对象撕下来贴到新对象上**。
这一节的脚本全部打印「两个 id 相不相等」这种布尔值，而不是一串每次都变的地址数字，
所以你重跑一定能逐字对上。
"""
import sys


def same(x, y):
    """只报地址相不相等：地址本身每次都变，不适合当引用证据。"""
    return id(x) == id(y)


print("=" * 68)
print("一、a = 1; b = a; a = 2 —— b 到底是什么")
print("=" * 68)

a = 1
b = a
print("贴标签之后：a =", a, " b =", b)
print("a 和 b 贴的是同一个对象吗？", same(a, b))

a = 2
print("a = 2 之后：  a =", a, " b =", b, " ← b 没被 a 带着变")
print("现在 a 和 b 还是同一个对象吗？", same(a, b))
print("b 的类型是", type(b).__name__, "，值还是", b)
print()
print("C 里 int b = a; 是倒一份值，所以 b 永远是 1；")
print("Python 里 b = a 是把标签也贴到同一个对象上，之后 a 换对象跟 b 无关。")
print("两种写法这一次结果一样，但原因不一样 —— 区别在可变对象上会炸开（第 4 节）。")

print()
print("=" * 68)
print("二、可变对象上，「贴同一张标签」和「倒一份值」立刻分道扬镳")
print("=" * 68)
xs = [1, 2, 3]
alias = xs                 # 两个标签贴同一个对象（C 里做不到：那是复制一整份）
print("alias is xs 吗？", same(alias, xs))
xs.append(4)
print("改 xs 之后 alias =", alias, " ← 它不是复制品，它就是同一个东西")
xs = [9, 9]                # 这一行是换标签
print("换标签之后 alias =", alias, "，xs =", xs)
print("现在 alias is xs 吗？", same(alias, xs))
print()
print("读法：`xs.append(4)` 动的是对象本体；`xs = [9, 9]` 动的只是 xs 这张标签指向谁。")
print("C 里要「两个名字指同一块内存」得自己写 int *p = &a；Python 里默认全是这种指针对，")
print("想要 C 那种「倒一份值」反而要明说（第 20 节的 copy / deepcopy）。")

print()
print("=" * 68)
print("三、函数参数也是贴标签，不是复制盒子")
print("=" * 68)


def show(target):
    """参数名 target 是贴在传进来的那个对象上的新标签。"""
    print("  函数里 id(参数) == id(外面那个对象)？", same(target, items))
    print("  参数指向的对象类型：", type(target).__name__)


items = [1, 2, 3]
show(items)
print("所以函数里改列表，外面看得见 —— 传的是同一个对象（第 05 章可变默认参数那个坑的根）。")

print()
print("=" * 68)
print("四、id() 不是终身身份证号（本机实测）")
print("=" * 68)
one = [1, 2, 3]
one_id = id(one)
del one                    # 最后一个标签撕掉 → 引用计数归 0 → 对象立刻回收
two = [4, 5, 6]
print("del 之后新建一个同类型的 list，地址撞上原来那个了吗？", id(two) == one_id)
print("撞上了也别奇怪：id 就是内存地址，地址会被复用。")
print("结论：id() 只用来当**同一时刻**的对照证据，别把它的数值存下来当身份。")
print("顺手记两个尺寸（本机 3.14 / 64 位）：sys.getsizeof([1,2,3]) =",
      sys.getsizeof([1, 2, 3]), "；sys.getsizeof(()) =", sys.getsizeof(()))
