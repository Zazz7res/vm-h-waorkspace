# -*- coding: utf-8 -*-
"""第 03 章示例 F：strip 的误会、大小写、isalpha/isdigit/isnumeric。

用法：python3 f_strip_case.py 1|2|3
"""
import sys
import unicodedata


def row(text, value, note=""):
    tail = f"  ← {note}" if note else ""
    print(f"{text:<40} → {value!r}{tail}")


def part1():
    print("=" * 68)
    print("一、strip(带参数) 删的是「字符集合」，不是前缀")
    print("=" * 68)
    row("'xyfooxy'.strip('xy')", "xyfooxy".strip("xy"))
    row("'mississippi'.strip('isp')", "mississippi".strip("isp"), "经典：几乎全被吃光")
    row("'banana'.strip('ban')", "banana".strip("ban"), "空串！每个字母都在集合里")
    row("'banana'.strip('bn')", "banana".strip("bn"), "左端 b 进集合啃一个，右端 a 不进就停")
    row("'  hi \\n'.strip()", "  hi \n".strip(), "无参=删所有空白")
    row("'\\t a \\t'.lstrip()", "\t a \t".lstrip(), "lstrip 只管左端")
    print("→ 规则：从两端往中间啃，只要【当前字符】落在参数集合里就啃掉，")
    print("  碰到第一个不在集合里的就停；两端各自独立。所以它没有「前缀」概念。")
    print("\n真要砍前缀/后缀：removeprefix / removesuffix（3.9+，整段匹配或原样返还）")
    row("'abcabc'.removeprefix('abc')", "abcabc".removeprefix("abc"))
    row("'abcabc'.removeprefix('xyz')", "abcabc".removeprefix("xyz"), "不匹配→原样，不报错")
    row("'notes.txt'.removesuffix('.txt')", "notes.txt".removesuffix(".txt"))
    row("'XfooX'.removeprefix('X')", "XfooX".removeprefix("X"), "只动左端一个")
    row("'XfooX'.removesuffix('X')", "XfooX".removesuffix("X"), "只动右端一个")


def part2():
    print("=" * 68)
    print("二、大小写：长度会变、一对多、locale 完全不掺和")
    print("=" * 68)
    row("'ß'.upper()", "ß".upper(), "一个字符变两个：len 1 → " + str(len("ß".upper())))
    row("'Straße'.upper()", "Straße".upper())
    row("'ß'.casefold()", "ß".casefold(), "casefold 才是给「不分大小写比较」用的")
    dotI = "İ"  # U+0130 带点大写 I
    row("U+0130 'İ'.lower()", dotI.lower(), "len=" + str(len(dotI.lower())) +
        "，展开成 i + 组合上句点")
    print(f"    拆开看码点：{[hex(ord(c)) for c in dotI.lower()]}, "
          f"第二个的名字：{unicodedata.name(dotI.lower()[1])}")
    row("'ı'(小写无点 i).upper()", "ı".upper(), "变 ASCII I —— 土耳其人看了摇头")
    row("'ﬀ'(连字).upper()", "ﬀ".upper(), "一个码点变两个")
    row("'hello world'.title()", "hello world".title())
    row("\"they're ok\".title()", "they're ok".title(), "撇号后也大写：不是你要的别用 title")
    row("'hello world'.capitalize()", "hello world".capitalize(), "只首字母大写")
    print("\n不分大小写比较/排序的正确姿势：")
    row("sorted(['Banana', 'apple'])", sorted(["Banana", "apple"]), "默认按码点：大写在前")
    row("sorted(['Banana', 'apple'], key=str.casefold)",
        sorted(["Banana", "apple"], key=str.casefold))
    row("'ß'.casefold() == 'ss'", "ß".casefold() == "ss")
    print("→ 上面这些都与 locale 无关。locale 掺和的是 strftime 那类（第 14 节）。")


def part3():
    print("=" * 68)
    print("三、isalpha / isdigit / isnumeric：汉字、全角、上标、分数的判定")
    print("=" * 68)
    samples = [("中", "汉字"), ("a", "ASCII 字母"), ("１", "全角数字 1"), ("2", "半角 2"),
               ("²", "上标 2"), ("½", "二分之一"), ("五", "汉字五"), ("①", "圆圈 1"),
               ("Ⅷ", "罗马数字八"), ("_", "下划线"), (" ", "空格")]
    print(f"{'字符':<4} {'说明':<10} {'isalpha':>8} {'isdigit':>8} {'isnumeric':>10} "
          f"{'isalnum':>8} {'isidentifier':>12}")
    for ch, desc in samples:
        print(f"{ch:<4} {desc:<10} {str(ch.isalpha()):>8} {str(ch.isdigit()):>8} "
              f"{str(ch.isnumeric()):>10} {str(ch.isalnum()):>8} "
              f"{str(ch.isidentifier()):>12}")
    print("→ 分工：isdigit 认「能当十进制数位用」的（含全角/上标/圈数字）；")
    print("  isnumeric 最宽（连 ½、五、Ⅷ 都算「有数值」）；isalpha 管「字母类」（汉字也算）。")
    print("\n但 isdigit=True 不等于 int() 能用：")
    row("int('１２３')（全角）", int("１２３"))
    try:
        int("²")
    except ValueError as e:
        print(f"int('²') → ValueError: {e}")
    try:
        int("½")
    except ValueError as e:
        print(f"int('½') → ValueError: {e}")
    print("→ 判「能不能 int()」：ch.isdigit() 再 int() 兜住大多数，或者干脆 try/except；")
    print("  isnumeric 只说明『这是个数量』，不保证能转。")
    row("'中'.islower()", "中".islower(), "无字母可判 → False（不是 True！）")
    row("'abc123'.isascii()", "abc123".isascii())
    row("' '.isspace()", " ".isspace())


if __name__ == "__main__":
    which = sys.argv[1] if len(sys.argv) > 1 else "1"
    {"1": part1, "2": part2, "3": part3}[which]()
