# -*- coding: utf-8 -*-
"""第 03 章示例 B：编码/解码 —— 字节数对照、errors= 三种命运、BOM、本机 codec 名录。

用法：python3 b_codecs.py 1|2|3|4|5
"""
import codecs
import sys


def part1():
    print("=" * 68)
    print("一、同一个字符在不同 codec 下占几个字节")
    print("=" * 68)
    encs = ["utf-8", "gbk", "gb18030", "big5", "latin-1", "utf-16"]
    print("字符  " + "  ".join(f"{e:>9}" for e in encs))
    for ch in ["中", "ü", "ß", "±", "\U0001f600"]:
        cells = []
        for e in encs:
            try:
                cells.append(str(len(ch.encode(e))))
            except UnicodeEncodeError:
                cells.append("编不下")
        disp = "emoji" if ord(ch) > 0xFFFF else ch
        print(f"{disp:<4}  " + "  ".join(f"{c:>9}" for c in cells))
    print("→ 同一行数字不同就是「编码」的全部意义：码点不变，字节数随 codec 变。")
    print("→ latin-1 只覆盖 U+0000~U+00FF，汉字直接编不下；gbk 连 ß 和 emoji 都不收，")
    print("  但 gb18030（国标强制版）全收 —— 收不收是 codec 的属性，不是 Python 的。")

    print("\n编不下的报错原文（一字不改）：")
    try:
        "ß".encode("gbk")
    except UnicodeEncodeError as e:
        print(f"  UnicodeEncodeError: {e}")
    print("  这条消息措辞很怪（illegal multibyte sequence），别被绕进去：")
    print("  它就是「这个字符该 codec 装不下」的意思。")

    print("\n往返与乱码：")
    gbk = "中文".encode("gbk")
    print(f"  '中文'.encode('gbk') → {gbk!r}；原样 decode('gbk') 回去 → "
          f"{gbk.decode('gbk')!r}")
    wrong = "中".encode("utf-8").decode("latin-1")
    print(f"  UTF-8 的 3 字节按 latin-1 读 → {wrong!r}（每个字节各配一个字符）")
    print(f"  再按 latin-1 编回去、按 UTF-8 解 → "
          f"{wrong.encode('latin-1').decode('utf-8')!r} ← 经典乱码的自救手法")


def part2():
    print("=" * 68)
    print("二、编码侧的三种命运：errors='strict' / 'replace' / 'ignore'")
    print("=" * 68)
    try:
        "中文".encode("latin-1")
    except UnicodeEncodeError as e:
        print(f"strict  → 抛异常：%s: %s" % (type(e).__name__, e))
        print(f"          e.encoding={e.encoding!r} e.start={e.start} e.end={e.end} "
              f"e.object={e.object!r}")
        print(f"          e.reason={e.reason!r}（第一个编不下的是 {e.object[e.start]!r}）")
    print(f"replace → {'中文x'.encode('latin-1', errors='replace')!r}   ← 编不下的变 ?")
    print(f"ignore  → {'中文x'.encode('latin-1', errors='ignore')!r}            ← 直接扔掉")
    print(f"backslashreplace → {'中'.encode('ascii', errors='backslashreplace')!r}")
    print(f"xmlcharrefreplace → {'中'.encode('ascii', errors='xmlcharrefreplace')!r}")
    print(f"namereplace → {'中'.encode('ascii', errors='namereplace')!r}")
    print("→ replace/ignore 是「数据无声消失」的开关，默认 strict 才是安全的。")


def part3():
    print("=" * 68)
    print("三、解码侧：错字节的三种命运 + UnicodeDecodeError 的料")
    print("=" * 68)
    gbk = "中文".encode("gbk")          # 拿 GBK 的字节冒充 UTF-8
    try:
        gbk.decode("utf-8")
    except UnicodeDecodeError as e:
        print(f"strict  → 抛异常：%s: %s" % (type(e).__name__, e))
        print(f"          e.start={e.start}  e.end={e.end}  ← 这是【字节位置】，不是字符位置")
        print(f"          e.object={e.object!r}（字节原样，共 {len(e.object)} 个）")
        print(f"          e.reason={e.reason!r}")
    rep = gbk.decode("utf-8", errors="replace")
    print(f"replace → {rep!r}   ← 长度 {len(rep)}，每段坏字节换一个 U+FFFD")
    print(f"ignore  → {gbk.decode('utf-8', errors='ignore')!r}   ← 长度 "
          f"{len(gbk.decode('utf-8', errors='ignore'))}，坏字节人间蒸发")
    print(f"backslashreplace → {gbk.decode('utf-8', errors='backslashreplace')!r}")
    try:
        gbk.decode("utf-8", errors="xmlcharrefreplace")
    except TypeError as e:
        print(f"decode 用 xmlcharrefreplace → TypeError: {e}")
        print("  （那个 handler 只服务于编码方向，解码方向没有对应物）")

    print("\n位置报的是字节下标，这里最容易按字符数错位：")
    mid = b"ok" + "中".encode("gbk") + b"!"
    try:
        mid.decode("utf-8")
    except UnicodeDecodeError as e:
        print(f"  前面有 2 个 ASCII 字节 → 报错说 {e}（字节位置 {e.start}，不是字符第 0 个）")
    trunc = "中".encode("utf-8")[:2]
    try:
        trunc.decode("utf-8")
    except UnicodeDecodeError as e:
        print(f"  只留半个汉字：{e}")
        print(f"  → start={e.start} end={e.end} 是【坏字节区间】，区间可以不止 1 字节")


def part4():
    print("=" * 68)
    print("四、BOM：U+FEFF 与 utf-8-sig")
    print("=" * 68)
    raw = "中文".encode("utf-8")
    withbom = b"\xef\xbb\xbf" + raw
    print(f"'中文'.encode('utf-8')      = {raw!r}")
    print(f"'中文'.encode('utf-8-sig')  = {withbom!r}")
    print(f"BOM 三字节 EF BB BF 就是字符 U+FEFF 的 UTF-8 编码："
          f"{'\ufeff'.encode('utf-8')!r}")
    import unicodedata
    print(f"名字：U+FEFF 叫 {unicodedata.name('\ufeff')}（历史包袱，如今主要当 BOM 用）")
    got = withbom.decode("utf-8")
    print(f"用 utf-8 读带 BOM 的字节流：{got!r} → 首字符是 U+{ord(got[0]):04X}，混进正文了")
    print(f"用 utf-8-sig 读同一串：      {withbom.decode('utf-8-sig')!r} → BOM 被吃掉")
    print(f"用 utf-8-sig 读不带 BOM 的：  {raw.decode('utf-8-sig')!r} → 也正常（sig 遇无则原样解）")
    print(f"utf-16 默认自带 BOM：'中'.encode('utf-16') = "
          f"{'中'.encode('utf-16')!r}（FF FE = 小端标记）")
    print("→ Windows 记事本的 UTF-8 文件常带 BOM；拿 utf-8 读，第一个字符就多出来。")


def part5():
    print("=" * 68)
    print("五、本机 codec 名录：逐个 codecs.lookup，不猜")
    print("=" * 68)
    names = ["utf8", "UTF-8", "utf-8", "ascii", "latin-1", "latin1", "iso-8859-1",
             "cp1252", "cp936", "gbk", "gb2312", "gb18030", "big5", "shift_jis",
             "euc_jp", "koi8-r", "utf-16", "utf-16-le", "utf-8-sig", "utf-32",
             "punycode", "idna", "rot13", "hex", "base64", "unicode_escape",
             "string_escape", "mbcs", "oem", "不存在的东西"]
    for n in names:
        try:
            print(f"  {n:<16} → {codecs.lookup(n).name}")
        except LookupError as e:
            print(f"  {n:<16} → LookupError: {e}")
    import encodings
    print(f"\nalias 表里登记的别名条目数：{len(encodings.aliases.aliases)}")
    print("→ 'utf8'/'UTF-8' 是同一 codec 的别名；cp936 就是 gbk 的马甲；")
    print("  'string_escape'、'mbcs' 是 Python 2 / Windows 的旧事，本机没有。")
    print(f"  中文 codec 靠 C 扩展 _codecs_cn（第 01 章的 lib-dynload 里能看到它）。")


if __name__ == "__main__":
    which = sys.argv[1] if len(sys.argv) > 1 else "1"
    {"1": part1, "2": part2, "3": part3, "4": part4, "5": part5}[which]()
