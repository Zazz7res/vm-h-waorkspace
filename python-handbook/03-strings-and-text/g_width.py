# -*- coding: utf-8 -*-
"""第 03 章示例 G：显示宽度 —— east_asian_width、CJK 对齐表格、三个数字各管各的。

用法：python3 g_width.py
"""
import unicodedata


def char_cells(ch):
    """一个码点在等宽终端里占几格。启发式：W/F 算 2，组合记号算 0，其余算 1。
    注意 A（ambiguous）按 1 处理 —— 它在开了 CJK 字体的终端里可能是 2，见 README。"""
    if unicodedata.combining(ch):
        return 0
    eaw = unicodedata.east_asian_width(ch)
    return 2 if eaw in ("W", "F") else 1


def display_width(s):
    return sum(char_cells(c) for c in s)


def pad_naive(s, n):
    return s + " " * (n - len(s))          # 按码点数补 —— CJK 会歪


def pad_aware(s, n):
    return s + " " * (n - display_width(s))  # 按显示宽度补


def main():
    print("=" * 68)
    print("一、east_asian_width 的分类（本机逐个实测）")
    print("=" * 68)
    for code in [0x61, 0x4E2D, 0xFF30, 0xFF71, 0xB1, 0x3A9, 0x2192, 0x2605,
                 0x3000, 0x1F600, 0x301, 0x200B]:
        ch = chr(code)
        print(f"  U+{code:04X} east_asian_width={unicodedata.east_asian_width(ch):<3}"
              f" combining={unicodedata.combining(ch):>3}  类别={unicodedata.category(ch)}")
    print("→ W=宽（中日韩汉字）、F=全角、H=半角、Na=窄、N=中性、A=暧昧（看字体脸色）。")
    print("  U+3000 全角空格是 F：它吃掉两格，很多「多一个空格」的错位就是它。")

    print("\n二、三个数字各管各的：同一个字符串的 码点数 / UTF-8 字节数 / 显示格数")
    print("=" * 68)
    cases = ["hello", "中文", "中文字符串", "ＰＹＴＨＯＮ",
             unicodedata.normalize("NFC", "café"),
             unicodedata.normalize("NFD", "café"),
             "\U0001f600"]
    for s in cases:
        print(f"  {s!r:<22} len={len(s)}  utf8字节={len(s.encode('utf-8'))}  "
              f"显示格={display_width(s)}")

    print("\n三、同一张表两种补法（对比右边那条竖线）")
    print("=" * 68)
    rows = [("中文字符串", 6), ("café", 6), ("hello", 6), ("ｱｲｳ", 6), ("±x", 6)]
    print("按 len() 补（错位版）：")
    for name, w in rows:
        print(f"  |{pad_naive(name, 8)}| 数据 {w}")
    print("按显示宽度补（对齐版）：")
    for name, w in rows:
        print(f"  |{pad_aware(name, 8)}| 数据 {w}")
    print("→ len('中文字符串')=5 以为占 5 格，实际占 10 格：竖线就是这么歪的。")


if __name__ == "__main__":
    main()
