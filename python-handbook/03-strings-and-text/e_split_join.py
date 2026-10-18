# -*- coding: utf-8 -*-
"""第 03 章示例 E：split/join 的真脾气。

用法：python3 e_split_join.py 1|2|3|4
"""
import sys


def part1():
    print("=" * 68)
    print("一、split() 与 split(' ')：不是一回事")
    print("=" * 68)
    s = "  a  b\tc\nd "
    print(f"s = {s!r}")
    print(f"s.split()   → {s.split()!r}")
    print(f"s.split(' ') → {s.split(' ')!r}")
    print("→ 无参 split()：把【所有连续空白】当一个分隔符，还扔掉首尾空白，结果无空串；")
    print("  split(' ')：只按【单个空格字符】切，连续空格/制表符/换行都照切，空串照留。")
    print(f"''  .split()   → {''.split()!r}   ''.split(' ') → {''.split(' ')!r}")
    print(f"' '.split()    → {' '.split()!r}    ' '.split(' ') → {' '.split(' ')!r}")
    print("  （空串 split() 给 []；split(' ') 给 ['']——差一个空元素，代码会崩在这）")
    full = "a\u3000b"
    print(f"全角空格也算空白：{full!r}.split() → {full.split()!r}，"
          f"split(' ') → {full.split(' ')!r}")
    try:
        "abc".split("")
    except ValueError as e:
        print(f"split('') → ValueError: {e}")


def part2():
    print("=" * 68)
    print("二、maxsplit、rsplit、partition/rpartition")
    print("=" * 68)
    s = "a-b-c-d"
    print(f"{s!r}.split('-')    → {s.split('-')!r}")
    print(f"{s!r}.split('-', 2) → {s.split('-', 2)!r}   ← 最多切 2 刀，右边不再切")
    print(f"{s!r}.rsplit('-', 2) → {s.rsplit('-', 2)!r}  ← 从右边数 2 刀")
    print(f"{s!r}.split('-', -1) → {s.split('-', -1)!r}   ← 负数=不限")
    kv = "a=b=c"
    print(f"\n{kv!r}.partition('=')    → {kv.partition('=')!r}  ← 只切第一刀，三段定死")
    print(f"{kv!r}.rpartition('=')   → {kv.rpartition('=')!r} ← 只切最后一刀")
    print(f"'abc'.partition('=')     → {'abc'.partition('=')!r} ← 找不到时不是异常，是原串+两个空")
    k, sep, v = kv.partition("=")
    print(f"三元组直接解包：k={k!r} sep={sep!r} v={v!r}")
    print("→ 取「键=值」用 partition 比 split 稳：结果长度永远是 3，不会 IndexError。")


def part3():
    print("=" * 68)
    print("三、splitlines 认哪些分隔符（split('\\n') 只认一种）")
    print("=" * 68)
    seps = [("LF", "\n"), ("CR", "\r"), ("CRLF", "\r\n"), ("VT 0x0b", "\x0b"),
            ("FF 0x0c", "\x0c"), ("FS 0x1c", "\x1c"), ("GS 0x1d", "\x1d"),
            ("RS 0x1e", "\x1e"), ("NEL 0x85", "\x85"),
            ("LSEP U+2028", "\u2028"), ("PSEP U+2029", "\u2029")]
    print(f"{'名字':<12} 两列 = splitlines / split('\\n')（都作用在 A+分隔符+B 上）")
    for name, ch in seps:
        line = "A" + ch + "B"
        print(f"{name:<12} {line.splitlines()!r} / {line.split(chr(10))!r}")
    print("→ splitlines 一口气认 11 种「换行」；split('\\n') 只认 LF，CR 老 Mac 风格照吞。")
    print(f"'a\\r\\nb'.splitlines(keepends=True) → {'a\r\nb'.splitlines(keepends=True)!r}")
    print(f"'a\\n'.splitlines() → {'a\n'.splitlines()!r}  "
          f"'a\\n'.split('\\n') → {'a\n'.split(chr(10))!r}  ← 尾部空元素差别")
    print(f"'\\n'.splitlines() → {chr(10).splitlines()!r}   ''.splitlines() → {''.splitlines()!r}")
    print("→ 提醒：splitlines 不收分隔符参数，也没有 maxsplit（想要限量就 split）。")


def part4():
    print("=" * 68)
    print("四、join 的脾气，和「split 再 join 不还原」")
    print("=" * 68)
    print(f"','.join([])      → {','.join([])!r}     ← 空列表给空串，不报错")
    print(f"','.join(['x'])    → {','.join(['x'])!r}         ← 单元素不加尾巴逗号")
    print(f"'-'.join('abc')     → {'-'.join('abc')!r}   ← 分隔符在前！把 'abc' 当可迭代对象拆开了")
    try:
        ",".join([1, 2])
    except TypeError as e:
        print(f"','.join([1, 2])  → TypeError: {e}")
    print(f"补救：{','.join(str(n) for n in [1, 2])!r} 或 map(str, ...)")

    print("\n不还原的最小反例：")
    s1 = "a  b"
    print(f"原文 {s1!r} → split() → {s1.split()!r} → ' '.join → "
          f"{' '.join(s1.split())!r} ≠ 原文（两个空格塌成一个）")
    s2 = "a\r\nb"
    print(f"原文 {s2!r} → splitlines() → '\\n'.join → "
          f"{'\n'.join(s2.splitlines())!r} ≠ 原文（CR 被没收）")
    s3 = " a "
    print(f"原文 {s3!r} → split() → {s3.split()!r} → ''.join → "
          f"{''.join(s3.split())!r} ≠ 原文（首尾空格和分隔全没了）")
    print("→ 想「拆开还能装回去」：用 split(同一个 sep) + sep.join；"
          "split()/splitlines() 是丢信息的。")
    print(f"对照：'x,y'.split(',') 再 ','.join → "
          f"{','.join('x,y'.split(','))!r}（无损）")


if __name__ == "__main__":
    which = sys.argv[1] if len(sys.argv) > 1 else "1"
    {"1": part1, "2": part2, "3": part3, "4": part4}[which]()
