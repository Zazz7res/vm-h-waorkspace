"""第 02 章第 14 节：类型挂在对象上；int 没有上限（C 的溢出在 Python 里不存在）。

跑法：python3 m_types_int.py

C 里 `int x` 是「这块内存永远是 int，宽度 4 字节」。
Python 里变量没有类型，只有名字；类型跟着对象走，运行时换个对象就换个类型。
"""
import array
import math
import sys

print("=" * 68)
print("一、变量没有类型，对象才有")
print("=" * 68)
x = 1
print("x =", x, "→ type(x) =", type(x).__name__)
x = "现在我是字符串"
print("换一次赋值：x =", repr(x), "→ type(x) =", type(x).__name__)
x = [1, 2]
print("再换一次：type(x) =", type(x).__name__)
print("→ 变量身上没有任何地方写着「x 是 int」。写 x: int = 1 也只是注解，运行时不拦你：")
ns = {}
exec(compile("y: int = 1\ny = '换了'", "<注解演示>", "exec"), ns)
print("   先 y: int = 1 再 y = '换了'，结果 y =", repr(ns["y"]))
ns2 = {}
exec(compile("def f(a: int) -> str:\n    return a\n", "<注解>", "exec"), ns2)
print("   注解只是被存起来没人执行：f.__annotations__ =", ns2["f"].__annotations__)

print()
print("=" * 68)
print("二、一个 list 里混着放 int 和 str（C 的数组做不到）")
print("=" * 68)
mixed = [1, "二", 3.0, None, True]
print("mixed =", mixed)
print("每个元素的类型：", [type(v).__name__ for v in mixed])
print("list 里存的不是值，是指向对象的指针（本机 64 位，每个槽 8 字节）")
print("sys.getsizeof(mixed) =", sys.getsizeof(mixed), "← 只是外壳 + 指针数组，不含元素本体")
print("把每个本体的大小加起来 =", sum(sys.getsizeof(v) for v in mixed))
print("→ 代价一：每个元素单独占一块内存（装箱），比 C 的 int 数组费得多；")
print("  代价二：遍历时每次都得先问类型才能干活，这就是「Python 慢」的一大半原因；")
print("  好处：写起来自由。想要 C 那种紧凑数组，标准库自带 array：")
compact = array.array("i", [1, 2, 3, 4, 5])
print("  array('i', ...) 的 sys.getsizeof =", sys.getsizeof(compact), "，元素类型统一，溢出会报错：", end=" ")
try:
    array.array("i", [2**40])
except OverflowError as err:
    print("OverflowError:", err)

print()
print("=" * 68)
print("三、类型检查的两种写法，差别就在 bool 和子类身上")
print("=" * 68)
print("type(1) is int     →", type(1) is int)
print("isinstance(1, int) →", isinstance(1, int))
print("type(True) is int  →", type(True) is int, "← 严格：bool 被排除")
print("isinstance(True, int) →", isinstance(True, int), "← 宽容：bool 是 int 的儿子")


class SubInt(int):
    pass


print("isinstance(SubInt(5), int) →", isinstance(SubInt(5), int))
print("type(SubInt(5)) is int     →", type(SubInt(5)) is int)
print("→ 一般写 isinstance（允许子类，也允许 numbers 那种虚拟子类）；")
print("  只有「我就要精确这一种类型」才写 type() is。")

print()
print("=" * 68)
print("四、int 无限精度：C 的 int 溢出在这里根本不存在")
print("=" * 68)
print("C 的 int 是 32 位（本机 sizeof 见第 1 节），INT_MAX = 2147483647；")
print("Python：2**31 =", 2**31, "；2**63 =", 2**63)
print("2**200 =", 2**200)
print("2**200 的十进制位数 =", len(str(2**200)))
print("2**1000 的十进制位数 =", len(str(2**1000)))
print("50! =", math.factorial(50), "；位数 =", len(str(math.factorial(50))))

print()
print("它是「怎么长大」的？看 sys.getsizeof 一档一档跳：")
print("（本机 sys.int_info：每", sys.int_info.bits_per_digit, "位一个 digit，一个 digit",
      sys.int_info.sizeof_digit, "字节）")
base = sys.getsizeof(0)
header = base - sys.int_info.sizeof_digit   # 28 - 4 = 24：纯头部，第一个 digit 的位置已经含在壳里
print("  值为 0 的对象占", base, "字节；去掉 digit 的纯头部是", header, "字节")
for exp in [0, 1, 29, 30, 31, 59, 60, 61, 90, 200, 1000, 10000]:
    value = 2**exp
    size = sys.getsizeof(value)
    digits = (size - header) // sys.int_info.sizeof_digit
    need = (exp + 1 + 29) // 30
    print("  2**%-5s 占 %4d 字节 → %3d 个 digit（%d 个二进制位需要 %d 个 30 位 digit）%s"
          % (exp, size, digits, exp + 1, need, "  ✓ 对得上" if digits == need else "  ← 对不上"))
print("→ 28 → 32 → 36 …… 每 30 个二进制位加 4 字节。C 里这叫「得换个更大的类型」，")
print("  Python 里是对象自己变长，标签（变量名）完全不知道也不关心。")

print()
print("唯一会拦你的是「把它转成字符串」这一步（防止有人拿它烧 CPU）：")
try:
    str(2**100000)
except ValueError as err:
    print("ValueError:", err)
print("默认上限 sys.int_info.default_max_str_digits =", sys.int_info.default_max_str_digits)
print("算出来不受限：2**100000 这个 int 建成了，getsizeof =", sys.getsizeof(2**100000), "字节；")
print("len(str(2**1000)) =", len(str(2**1000)), "，只有超过 4300 位才报。")
