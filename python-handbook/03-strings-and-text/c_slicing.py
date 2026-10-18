# -*- coding: utf-8 -*-
"""第 03 章示例 C：切片 —— 越界的两种命运、负索引、步长、slice 对象、能不能赋值。

用法：python3 c_slicing.py 1|2
"""
import sys

S = "hello world"


def part1():
    print("=" * 68)
    print("一、s[i] 越界就抛，s[i:j] 越界只给短串 —— 两种越界两种命运")
    print("=" * 68)
    print(f"s = {S!r}（len={len(S)}）")
    print(f"s[4:100]   → {S[4:100]!r}     ← 终点越界：安静截到结尾")
    print(f"s[100:]    → {S[100:]!r}        ← 起点越界：安静给空串")
    print(f"s[4:2]     → {S[4:2]!r}        ← 起点在终点右边：空串，不报错")
    print(f"s[-100:2]  → {S[-100:2]!r}     ← 起点负得过头：夹到 0，正常截")
    print(f"'abc'[10:20] → {'abc'[10:20]!r}      ← 整段都在串外：还是空串")
    try:
        S[100]
    except IndexError as e:
        print(f"s[100]  → IndexError: {e}   ← 单下标越界就没这么客气")
    try:
        S[-100]
    except IndexError as e:
        print(f"s[-100] → IndexError: {e}")
    print("→ 对照 C：arr[i] 越界是未定义行为（读到隔壁变量或段错误）；")
    print("  Python 把「单下标」变成必抛异常，把「切片」变成自动夹断 —— 两种都安全，")
    print("  但脾气不同：切片越界不报错这件事，藏 bug 的本事一流（见 README ★）。")


def part2():
    print("=" * 68)
    print("二、负索引、步长、slice 对象")
    print("=" * 68)
    print(f"s[-1]      → {S[-1]!r}      s[-5:] → {S[-5:]!r}")
    print(f"s[-1] == s[len(s)-1] → {S[-1] == S[len(S) - 1]}")
    print(f"s[::2]     → {S[::2]!r}    ← 步长 2：隔一个取一个")
    print(f"s[::-1]    → {S[::-1]!r}  ← 步长 -1：整串反转")
    print(f"s[::-2]    → {'hello'[::-2]!r}（对 'hello'）  s[5:0:-2] → {S[5:0:-2]!r}  s[:0:-1] → {S[:0:-1]!r}")
    print("→ 负步长时省略的默认端点会互换（起点=尾、终点=头），所以 [:0:-1] 不含 s[0]。")

    sl = slice(1, 8, 2)
    print(f"\nslice 对象就是三个数打包：slice({sl.start}, {sl.stop}, {sl.step})")
    print(f"s[sl] == s[1:8:2] → {S[sl] == S[1:8:2]}（冒号切片 = 构造 slice 再传进去）")
    print(f"sl.indices(11) → {sl.indices(11)} ← 把越界/负数夹成合法三元组，切片内部就干这个")
    print(f"slice(-4).indices(11) → {slice(-4).indices(11)}")

    print("\n三、切片能不能赋值：list 能，str 不能")
    lst = [1, 2, 3, 4]
    lst[1:3] = ["a", "b"]
    print(f"list 切片赋值 [1,2,3,4] 的 [1:3] ← ['a','b'] → {lst!r}（长度可变，左边右边不必等长）")
    lst[1:1] = ["插"]
    print(f"零宽切片赋值（lst[1:1]=[x]）→ {lst!r} ← 这就是「在索引 1 处插入」")
    lst[0:2] = []
    print(f"赋空列表 = 删除 → {lst!r}")
    try:
        s2 = "hello"
        s2[1:3] = "X"
    except TypeError as e:
        print(f"str 切片赋值 → TypeError: {e}")
    print("→ 呼应第 02 章：str 不可变。想「改」只能重建："
          f"{S[:1] + 'X' + S[2:]!r}")


if __name__ == "__main__":
    which = sys.argv[1] if len(sys.argv) > 1 else "1"
    {"1": part1, "2": part2}[which]()
