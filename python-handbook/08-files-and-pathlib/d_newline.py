#!/usr/bin/env python3
# ============================================================================
# 取证脚本 d_newline.py —— 第 08 章第 5 节：newline= 在读写两侧各管什么
#
# 这个脚本要证明什么：newline= 的名字在骗人。它不是「设置换行符」，而是两个不同的
# 开关：读侧管「要不要把磁盘上的 \r\n / \r 翻译成 \n，以及按什么切行」；写侧管
# 「你字符串里的 \n 落地前换成什么」。同一条 \n，读写两侧的规则并不对称。
#
# 为什么要有它：把 newline= 讲成「换行符」会直接产出三个真实事故 —— 读老 Mac 文件
# （\r 分行）整篇变一行、写 CSV 多出一堆空行、splitlines() 数出来的行数和逐行读的
# 行数对不上。只有先把磁盘上的真实字节钉死（read_bytes），再让 newline 取五个值逐个
# 读回来，「翻译」这件事才能在屏幕上被看见。
#
# 本机 os.linesep = '\n'，所以写侧 newline=None 那一格当场验不出跨平台差别 ——
# 脚本会明写「别当已验证结论」，不假装自己跑过 Windows。
# 现场文件全在 newline-demo/ 里现造现删；输出无时间、无随机、无绝对路径。
# ============================================================================
import os
import shutil
import sys
from pathlib import Path

WORK = Path("newline-demo")
# 三份「磁盘上的真实形状」：行边界分别是 \n、\r\n、\r。
SHAPES = (("LF", b"a\nb\n", "Unix / 现代文本，行边界是 \\n"),
          ("CRLF", b"a\r\nb\r\n", "Windows / Excel 导出的 CSV，行边界是 \\r\\n"),
          ("CR", b"a\rb\r", "老 Mac（System 9 之前），行边界是 \\r"))
NLS = (None, "", "\n", "\r", "\r\n")


def read_text(path, newline=...):
    """按某个 newline 值整份读回来，返回 str。传 ... 表示**根本不写这个参数**。"""
    kwargs = {} if newline is ... else {"newline": newline}
    with open(path, encoding="utf-8", **kwargs) as fh:
        return fh.read()


def read_lines(path, newline):
    """按某个 newline 值**逐行迭代**，返回行列表（换行符留在每行末尾）。"""
    with open(path, encoding="utf-8", newline=newline) as fh:
        return list(fh)


shutil.rmtree(WORK, ignore_errors=True)        # 上次崩在半路的残骸先清掉，保证每次同一现场
WORK.mkdir()

print("=== 1. 先把三份真实字节形状钉在磁盘上：wb 不过文本层，写下去就是字节 ===")
for code, data, desc in SHAPES:
    path = WORK / f"{code}.txt"
    with open(path, "wb") as fh:
        fh.write(data)
    raw = path.read_bytes()
    print(f"  {code:<5} 写入 {data!r:<14} hex={raw.hex():<14} 磁盘字节数={len(raw)}")
    print(f"        读回逐字节相同 → {raw == data} ｜ 这一份的来历：{desc}")
print("  为什么第一步必须用 wb：文本模式的写侧也会翻译一次，字节形状就不由你定了。")
probe = WORK / "WB对照.txt"
probe.write_text("a\r\nb\r\n", encoding="utf-8")      # 不写 newline → 默认 None
print(f"  对照：write_text('a\\r\\nb\\r\\n')（默认 newline=None）本机磁盘 = {probe.read_bytes()!r}")
print(f"  → 本机 os.linesep={os.linesep!r}，只翻译 \\n，所以这一格恰好没变；")
print("    在 os.linesep='\\r\\n' 的机器上它会变成 'a\\r\\r\\nb\\r\\r\\n'"
      "（本机未验证，别当事实引用）。")
print("  ★ 结论：想「钉住字节」就只有 wb / read_bytes 一条路，t 模式的字节形状是算出来的。")

print("\n=== 2. 读侧 3×5 表：同一份字节，newline 五种取值各读回什么字符串 ===")
print("  （看 repr 就知道 \\r 有没有被翻译掉。）")
print("  最后一列是 splitlines() 的行数 —— 那是 str 自己的切法，和 newline 无关，摆这儿当对照。")
for code, data, _desc in SHAPES:
    path = WORK / f"{code}.txt"
    print(f"  文件 {code}.txt，磁盘 = {data!r}")
    for nl in NLS:
        text = read_text(path, nl)
        mark = "  ← 不写 newline 时的默认值" if nl is None else ""
        print(f"    newline={nl!r:<7} → repr={text!r:<14} splitlines 行数={len(text.splitlines())}{mark}")
print("  → 整份 read() 时唯一会变的是 newline=None：\\r\\n 和 \\r 都被翻译成 \\n，"
      "三份文件读回来一模一样。")
print("  → newline='' 完全不翻译，磁盘上有什么字符串里就有什么 \\r —— 想让 \\r 留在字符串里就靠它。")
print("  → 指定某个串（'\\n' / '\\r' / '\\r\\n'）时也不翻译；指定值只影响「按行读」，"
      "所以本表这一列全是 2 行，看不出区别 —— 区别在第 3 段。")
same = read_text(WORK / "LF.txt") == read_text(WORK / "LF.txt", None)
print(f"  → 不写 newline 和写 newline=None 是同一件事（本机实测相等 → {same}）。")

print("\n=== 3. 读侧的真正效果：逐行迭代时，行数才会露出差别 ===")
for code, _data, _desc in SHAPES:
    path = WORK / f"{code}.txt"
    for nl in NLS:
        lines = read_lines(path, nl)
        print(f"  {code:<5} newline={nl!r:<7} → 迭代到 {len(lines)} 行 {lines}")
print("  → 三格最有信息量：CR 文件配 newline='\\n' 读出 **1 行**（整篇粘成一块）；")
print("    CRLF 文件配 newline='\\r' 读出 **3 行**（切在 \\r 上，剩下一堆以 \\n 开头的碎行）。")
print("  → splitlines() 永远按「所有已知行边界」切，所以它数出来恒是 2 行 —— ")
print("    别拿它当「我按这个 newline 能读到几行」的证据，两者不是一回事。")
print("  ★ 代价：行数对不上时先怀疑 newline，别怀疑文件坏了；"
      "同一个文件配不同 newline 迭代，行数可以从 1 变到 3。")

print("\n=== 4. 写侧 5 格：你写的 \\n 落地时变成了什么 ===")
payload = "a\nb\n"
print(f"  待写入的字符串恒为 {payload!r}（两个 \\n），落盘字节逐个用 read_bytes 验：")
for idx, nl in enumerate(NLS):
    path = WORK / f"写{idx}.txt"
    with open(path, "w", encoding="utf-8", newline=nl) as fh:
        fh.write(payload)
    raw = path.read_bytes()
    mark = "  ← 默认值这一格和 newline='' 完全一样（本机）" if nl is None else ""
    print(f"  newline={nl!r:<7} → 磁盘 = {raw!r:<16} hex={raw.hex()} 字节数={len(raw)}{mark}")
print(f"  本机 os.linesep = {os.linesep!r}，磁盘字节数={len(os.linesep)}。"
      "newline=None 那一格写出的还是 \\n，和 newline='' 一模一样。")
print("  ★ 这一格本机验不出跨平台差别：os.linesep='\\n' 时「翻译成 os.linesep」这句话"
      "没有可观察的后果。")
print("    它只在本机成立，别当已验证的跨平台结论引用。")
print("  → newline='' 与 newline='\\n' 都把 \\n 原样写下去（本机）；"
      "newline='\\r' 把每个 \\n 换成 \\r；newline='\\r\\n' 把每个 \\n 换成 \\r\\n。")
print("  → 写侧的规则只有一句：**只有 \\n 会被换掉**，字符串里现成的 \\r 一个都不动 —— 看第 5 段。")

print("\n=== 5. 写侧的连环翻译：串里本来就带 \\r\\n 时会变成 \\r\\r\\n（CSV 空行的根）===")
csvish = "x,y\r\nz,w\r\n"                # csv 的 writer 默认自己写 \r\n 结尾
print(f"  待写入字符串 = {csvish!r}（\\r 和 \\n 都在字符串里了）")
for idx, nl in enumerate((None, "", "\r\n")):
    path = WORK / f"csv{idx}.txt"
    with open(path, "w", encoding="utf-8", newline=nl) as fh:
        fh.write(csvish)
    print(f"  newline={nl!r:<7} → 磁盘 = {path.read_bytes()!r}")
print("  → newline='\\r\\n' 时字符串里那个 \\n 又被换成 \\r\\n，于是每个行尾变成 \\r\\r\\n；")
print("    Excel / 文本编辑器把这个多余的 \\r 当成一个单独的行边界，屏幕上就多了空行。")
print("  → 本机 newline=None 这一格恰好没变（os.linesep 就是 \\n）；")
print("    在 os.linesep='\\r\\n' 的机器上它同样会变成 \\r\\r\\n（本机未验证）。")
print("  → 所以 csv 模块的官方要求是**开文件时 newline=''**：让 csv 自己决定字节，文本层别插手。")
print("    为什么必须这样、以及不这样会打出什么字节，见第 15 节 n_csv.py。")
print("  ★ 本节只做文本层翻译的对照，不碰 csv 的引号与分隔符 —— 那是第 15 节的事。")

print("\n=== 6. 收口：读侧翻译谁决定、写侧翻译谁决定，一张表 + 三句话 ===")
print("  每行格式：newline 取值 ｜ 读侧（磁盘 → str）｜ 写侧（str → 磁盘）。None 就是不写时的默认值。")
rows = (
    ("None", "\\r\\n 和 \\r 都翻译成 \\n（通用换行）", "把 \\n 换成 os.linesep"),
    ("''", "不翻译，字节原样进字符串", "\\n 原样落地（本机等于不翻译）"),
    ("'\\n'", "只按 \\n 切行，不翻译", "把 \\n 换成 \\n（等于不翻译）"),
    ("'\\r'", "只按 \\r 切行，不翻译", "把每个 \\n 换成 \\r"),
    ("'\\r\\n'", "只按 \\r\\n 切行，不翻译", "把每个 \\n 换成 \\r\\n"),
)
for r in rows:
    print(f"    {r[0]:<9}｜ 读：{r[1]} ｜ 写：{r[2]}")
print("  → 判据一：只有 newline=None 会**改字符串内容**（把行边界统一成 \\n）；"
      "其余四种磁盘上是什么就读回什么。")
print("  → 判据二：写侧翻译的目标不由你填的值决定，由 **os.linesep** 决定的是 None 那一格，"
      "剩下几格是你填的串本身。")
print("  三句话背下来：")
print("  ① 想让 \\r 留在字符串里（自己数行、自己解析、交给 csv）就 newline=''；")
print("  ② 写 CSV 必须 newline=''，否则文本层会在 csv 已经写好的 \\r\\n 上再翻一次；")
print("  ③ newline='' 只是「别替我翻译」，不是「按这个切行」—— 行边界得你自己负责，")
print("     而 splitlines() 数出来的行数和 newline 无关，两者对不上是常态（见第 3 段）。")
print(f"  本机事实：os.linesep={os.linesep!r} ｜ sys.platform={sys.platform}"
      " ｜ 上面标了「本机未验证」的两格是跨平台部分，不在本章结论清单里。")

shutil.rmtree(WORK)
print(f"\n  清理：{WORK.name}/ 还在吗 → {WORK.exists()}"
      " ｜ 本脚本只往这一个目录里写过文件，章目录里没多出东西。")
