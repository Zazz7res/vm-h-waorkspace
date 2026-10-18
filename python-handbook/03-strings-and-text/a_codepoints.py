# -*- coding: utf-8 -*-
"""第 03 章示例 A：str 是码点序列，不是字节；é 的两种写法；NFC/NFD 现场来回。

用法：python3 a_codepoints.py 1|2|3   （1/2/3 对应 README 的三个小节）
注意：本文件里所有"长得一样但码点不同"的字符一律用反斜杠-u 转义写死，
      防止任何编辑器/输入法把它们悄悄合并成别的东西。
"""
import sys
import unicodedata

PRECOMPOSED = "\u00e9"      # é：一个码点 U+00E9（预组合）
COMBINED = "e\u0301"        # é：e + U+0301 组合尖音符（两个码点）


def part1():
    print("=" * 68)
    print("一、三个数字并排：字符数 / UTF-8 字节数 / bytes 对象长度")
    print("=" * 68)
    s = "中文"
    b = s.encode("utf-8")
    print(f"len(s)              = {len(s)}          ← s 是 str，数的是【码点】")
    print(f"len(s.encode())     = {len(b)}           ← 先变 bytes，数的是【字节】")
    print(f"len(bytes(s,'utf-8')) = {len(bytes(s, 'utf-8'))}           ← 同上，构造器写法")
    print(f"repr(b)             = {b!r}")
    print(f"b[0] 是码点吗？      = {b[0]}（{hex(b[0])}），是第 1 个【字节】，不是任何汉字")
    print(f"'中' 的 UTF-8 字节   = {'中'.encode('utf-8')!r}（{len('中'.encode('utf-8'))} 字节）")
    print(f"'a' 的 UTF-8 字节    = {'a'.encode('utf-8')!r}（{len('a'.encode('utf-8'))} 字节）")
    print("→ 同一个 str，len 是 2；编码成 UTF-8 是 6 个字节。差 3 倍，不是巧合：")
    print("  CJK 基本区汉字每个码点在 UTF-8 里占 3 字节，ASCII 占 1 字节。")

    print("\n字面量 b\"中文\" 在写的时候就会被编译期拒绝：")
    try:
        compile("x = b'中文'", "<a_codepoints>", "exec")
    except SyntaxError as e:
        print(f"  SyntaxError: {e.msg}")
        print(f"  （发生在 {e.lineno} 行 {e.offset} 列；bytes 字面量只许放 ASCII）")
    print("→ 想要「中文的字节」只有两条路：'中文'.encode('utf-8') 或 bytes('中文', 'utf-8')，")
    print("  或者手写转义 b'\\xe4\\xb8\\xad\\xe6\\x96\\x87'。没有第三种。")


def part2():
    print("=" * 68)
    print("二、é 的两种写法：看起来一模一样，== 却是 False")
    print("=" * 68)
    a = PRECOMPOSED
    b = COMBINED
    print(f"写法一 a = \"\\u00e9\"    → repr: {a!r}   len: {len(a)}")
    print(f"写法二 b = \"e\\u0301\"  → repr: {b!r}   len: {len(b)}")
    print(f"（上面两行的引号里长得一样 —— 终端按字形画出来就是同一个 é）")
    print(f"a 的码点: {[hex(ord(c)) for c in a]}")
    print(f"b 的码点: {[hex(ord(c)) for c in b]}")
    print(f"a == b  → {a == b}")
    print(f"名字 a: {unicodedata.name(a)}")
    print(f"名字 b: {[unicodedata.name(c) for c in b]}")
    print(f"类别: U+00E9 是 {unicodedata.category(a)}；"
          f"U+0301 是 {unicodedata.category(b[1])}（Mn=组合记号，不单独成字）")
    print("→ str 的 == 比的是码点序列是否逐个相同，不是「看起来像不像」。")
    print("  所以搜索引擎/表格软件里算同一个词，在 Python 里默认算两个不同的键。")


def part3():
    print("=" * 68)
    print("三、unicodedata.normalize 现场来回")
    print("=" * 68)
    a = PRECOMPOSED
    b = COMBINED
    print(f"NFC(b) == a → {unicodedata.normalize('NFC', b) == a}")
    print(f"NFD(a) == b → {unicodedata.normalize('NFD', a) == b}")
    print(f"来回一轮 NFC(NFD(a)) == a → "
          f"{unicodedata.normalize('NFC', unicodedata.normalize('NFD', a)) == a}")
    print(f"len: a={len(a)}  NFC(b)={len(unicodedata.normalize('NFC', b))}  "
          f"NFD(a)={len(unicodedata.normalize('NFD', a))}")

    word_nfc = unicodedata.normalize("NFC", "café")
    word_nfd = unicodedata.normalize("NFD", "café")
    print(f"\n café 两种形态：NFC 长度 {len(word_nfc)}，NFD 长度 {len(word_nfd)}，"
          f"相等？{word_nfc == word_nfd}")
    d = {word_nfc: 3.5}
    print(f"字典里存了 NFC 版；用 NFD 版查能命中吗 → {word_nfd in d}"
          f"（False 就意味着实际会抛 KeyError）")
    print("  （真实现场：macOS 的文件名走 NFD，你在 Windows/iPhone 打的 é 是 NFC —— "
          "跨平台比对用户名/歌名时这就是个 bug。）")
    print(f"比对前先归一：NFC(NFD 版) in d → {unicodedata.normalize('NFC', word_nfd) in d}")

    print("\n顺手记两条：")
    print(f"本机 Unicode 数据版本 unicodedata.unidata_version = {unicodedata.unidata_version}")
    print(f"NFKC（兼容分解，比 NFC 更狠）：'\\u00bd'(½)→{unicodedata.normalize('NFKC', chr(0xBD))!r}，"
          f"'\\u00b2'(²)→{unicodedata.normalize('NFKC', chr(0xB2))!r}")
    print(f"NFKC 拆连字：'ﬁ'(U+FB01)→{unicodedata.normalize('NFKC', chr(0xFB01))!r}")
    print("→ 判「Unicode 等不等」的标准做法：unicodedata.is_normalize(")
    print("    'NFC', x) 先验，或两边都 normalize('NFC', …) 再 ==。")


if __name__ == "__main__":
    which = sys.argv[1] if len(sys.argv) > 1 else "1"
    {"1": part1, "2": part2, "3": part3}[which]()
