# -*- coding: utf-8 -*-
"""第 03 章示例 H：bytes 与 str 的边界、bytearray、in 与逐字符遍历。

用法：python3 h_bytes_iter.py 1|2
"""
import sys


def row(text, value, note=""):
    tail = f"  ← {note}" if note else ""
    print(f"{text:<36} → {value!r}{tail}")


def part1():
    print("=" * 68)
    print("一、bytes 的下标给出的是 int，不是「长度为 1 的 bytes」")
    print("=" * 68)
    row("b'abc'[0]", b"abc"[0], "int！对照 C：char 数组取下标是值不是指针")
    row("b'abc'[0:1]", b"abc"[0:1], "切片才回到 bytes")
    row("list(b'abc')", list(b"abc"))
    row("chr(b'abc'[0])（字节数再变回字符）", chr(b"abc"[0]))
    try:
        b"abc" + "abc"
    except TypeError as e:
        print(f"b'abc' + 'abc' → TypeError: {e}")
    try:
        "abc" + b"abc"
    except TypeError as e:
        print(f"'abc' + b'abc' → TypeError: {e}")
    print("→ bytes 和 str 是两个世界，+ 不跨界；要跨界就 encode/decode。")
    try:
        bytes([256])
    except ValueError as e:
        print(f"bytes([256]) → ValueError: {e}")
    row("'中'.encode('utf-8') == b'\\xe4\\xb8\\xad'",
        "中".encode("utf-8") == b"\xe4\xb8\xad")

    print("\nbytearray：可变的字节；bytes：不可变（呼应第 02 章）")
    ba = bytearray(b"ab")
    ba[0] = 65
    ba.append(99)
    row("bytearray(b'ab') 改 [0]=65、append(99)", bytes(ba))
    try:
        b"ab"[0] = 65
    except TypeError as e:
        print(f"b'ab'[0] = 65 → TypeError: {e}")
    row("bytes(ba) 转回不可变", bytes(ba))
    try:
        hash(bytearray(b"ab"))
    except TypeError as e:
        print(f"hash(bytearray) → TypeError: {e}")
    row("hash(b'ab') 可用", hash(b"ab") == hash(b"ab"))


def part2():
    print("=" * 68)
    print("二、in 判的是子串；for c in s 拿到的是长度为 1 的 str")
    print("=" * 68)
    s = "我爱你中国"
    print(f"s = {s!r}")
    row("'中' in s", "中" in s)
    row("'国中' in s", "国中" in s, "顺序反了就不是子串（in 要连续同序）")
    row("'love' in 'I love Python'", "love" in "I love Python")
    print("→ str 的 in = 「是不是子串」，等价 C 的 strstr，不是「集合里有没有」。")

    print("\n逐字符遍历：每个 c 都是 str、长度 1 —— 不是 C 的 char")
    for c in "中a":
        print(f"  c={c!r}  type={type(c).__name__}  len={len(c)}  ord={ord(c)} "
              f"U+{ord(c):04X}")
    print("换成遍历 bytes，元素就是 0~255 的整数：")
    for i, byte in enumerate("中a".encode("utf-8")):
        print(f"  [{i}] {byte!r} ({hex(byte)})")
    print("→ 一个「中」字：str 视角 1 个元素；bytes 视角 3 个元素。这就是章节开头的延续。")

    print("\nenumerate / zip 两个常用式：")
    row("list(enumerate('abc'))", list(enumerate("abc")))
    row("list(zip('abc', [1, 2]))", list(zip("abc", [1, 2])), "zip 默认配到短的为止")
    try:
        list(zip("abc", [1, 2], strict=True))
    except ValueError as e:
        print(f"strict=True 长度不齐 → ValueError: {e}")


if __name__ == "__main__":
    which = sys.argv[1] if len(sys.argv) > 1 else "1"
    {"1": part1, "2": part2}[which]()
