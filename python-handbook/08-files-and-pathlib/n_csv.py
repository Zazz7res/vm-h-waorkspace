#!/usr/bin/env python3
# ============================================================================
# 取证脚本 n_csv.py —— 第 08 章第 15 节：csv 到底替你处理了什么
#
# 这个脚本要证明三件事：
#   ① CSV 的难不在「按逗号切」，在**引号**：字段里允许有逗号、有 doubling 过的双引号、
#      甚至有换行。手工 split(",") 在这里必错，而且错得很安静（列数变了，程序不报）。
#   ② csv 模块只管「引号与分隔符」：不管编码（encoding 你自己给）、不管类型（读回来全是 str）、
#      也不管行尾。主菜是第 ③ 条：写 CSV 必须 open(..., newline="") —— csv 自己写 \r\n，
#      文本层默认还会再把 \n 翻译一次，两次叠起来就是 \r\r\n。第 3 段用 read_bytes 打真实字节。
# 现场全在 csv-demo/ 里现造现删；故意失败的（csv.Error / ValueError）一律接住、原样打印。
# ============================================================================
import csv
import io
import os
import shutil
from pathlib import Path

WORK = Path("csv-demo")
BS = "\\"          # 一个反斜杠：escapechar 必须是 1 个字符
NL = "\n"          # 真换行：f-string 表达式里写 '\n' 是俩字符，所以先存成变量
CRLF = b"\r\n"    # 数磁盘上的字节要用真字节常量，别在 f-string 里现拼
CRCRLF = b"\r\r\n"
# 贯穿全脚本的三行数据：备注列占了两种麻烦（逗号 / 双引号）；带换行的字段第 2 段单独演。
ROWS = [["姓名", "备注"], ["张三", "预算, 超了"], ["李四", '他说"好"']]


def rows_of(path, newline=""):
    """按指定 newline 读成行列表 —— 第 3 段对照的正是这一个参数。"""
    with open(path, newline=newline, encoding="utf-8") as fh:
        return list(csv.reader(fh))


def wtext(name, text):
    """原样落一份文本（不经 csv），用来造「别人给的坏文件」。返回路径。"""
    path = WORK / name
    path.write_text(text, encoding="utf-8", newline="")
    return path


def dict_rows(name, text):
    """落一份别人给的 CSV 再读成 dict 行；DictReader 一起带回来（要用它的 restkey/restval）。"""
    path = wtext(name, text)
    with open(path, newline="", encoding="utf-8") as fh:
        dr = csv.DictReader(fh)
        return dr, list(dr)


def write_case(name, newline_value, rows=ROWS):
    """按给定的 newline= 写一份 CSV，返回路径。只有这一个参数在变，别的全一样。"""
    path = WORK / name
    with open(path, "w", newline=newline_value, encoding="utf-8") as fh:
        csv.writer(fh).writerows(rows)
    return path


def show_bytes(path, label):
    """打磁盘上的真实字节：repr 才让 \\r \\n 看得见，终端会直接把它们吃掉。"""
    raw = path.read_bytes()
    print(f"  {label:<17} {path.name:<17} 字节={len(raw):<4}"
          f"\\r\\n ×{raw.count(CRLF)} 其中 \\r\\r\\n ×{raw.count(CRCRLF)}")
    print(f"      repr = {raw!r}")


def oneline(row, **kw):
    """把一行数据写进内存只拿那一行：引号实验都用它，免得文件读写来干扰结论。"""
    buf = io.StringIO()
    csv.writer(buf, **kw).writerow(row)
    return buf.getvalue()


print("=== 1. 先演「不用 csv 会怎么死」：同一行数据，split 和 reader 并排 ===")
if WORK.exists():                        # 上次崩溃留下的残骸：先清干净，保证每次同一现场
    shutil.rmtree(WORK)
WORK.mkdir()
for line in ['张三,"预算, 超了"', '李四,"他说""好"""', '"第一行\n第二行",备注']:
    cut = line.split(",")
    parsed = next(csv.reader([line]))    # csv.reader 收的是「产出若干行文本」的可迭代对象
    flag = "★列数错了" if len(cut) != len(parsed) else f"列数都是 {len(cut)}，内容却不同"
    print(f"  原文 {line!r} → split(',') 给 {cut!r}（{len(cut)} 列）"
          f"‖ csv.reader 给 {parsed!r}（{len(parsed)} 列）{flag}")
print("  → split 的错不抛异常：它把「预算, 超了」切成两列，整张表当场错位，")
print("    要到下游按列名取值时才炸 —— 那时候已经不知道是第几行的锅了。")

print("\n=== 2. csv 的引号规则实测：三种麻烦字段 × quoting 四档 × escapechar ===")
for label, value in (("含逗号", "预算, 超了"), ("含双引号", '他说"好"'), ("含换行", "第一行\n第二行")):
    print(f"  {label:<8} → 写成 {oneline(['字段', value], lineterminator=NL)!r}")
print("  → 规则只有两条：需要保护才加引号（QUOTE_MINIMAL）；字段里的 \" 写成 \"\"（doubling）。")
print("    读的时候反过来：**只有被引号包住的换行才算字段内容** —— 这是第 6 段那个坑的根源。")
probe = ["甲", 1, "有,逗号", '有"引"号']
for qname in ("QUOTE_MINIMAL", "QUOTE_ALL", "QUOTE_NONNUMERIC", "QUOTE_NONE"):
    try:
        print(f"  {qname:<17} → {oneline(probe, quoting=getattr(csv, qname))!r}")
    except csv.Error as exc:
        print(f"  {qname:<17} → ★故意失败：{type(exc).__name__}: {exc}")
print(f"  escapechar=反斜杠（仍 QUOTE_MINIMAL）→ {oneline(probe, escapechar=BS)!r}")
print(f"  escapechar=反斜杠 + QUOTE_NONE       → {oneline(probe, escapechar=BS, quoting=csv.QUOTE_NONE)!r}")
print("  → 实测：QUOTE_MINIMAL 下 escapechar **完全不起作用**（上一行和 QUOTE_MINIMAL 那行逐字一样，")
print("    引号照旧 doubling）；只有 QUOTE_NONE「不用引号」时它才接手。")
print(f"  默认 lineterminator = {csv.writer(io.StringIO()).dialect.lineterminator!r}"
      f" ｜ os.linesep = {os.linesep!r} ｜ 本机自带 dialect 名 {sorted(csv.list_dialects())}")
print("  → 这两个默认值不一致：csv 硬写 \\r\\n，本机文本层写的是 \\n。第 3 段演的就是它俩打架。")
print("  → QUOTE_NONE 是「我不用引号」的宣言：字段里一旦有分隔符它只能报错 —— 宁可报错，")
print("    也不产出一份看着正常、实际错位的文件（这条原文就是上面那句 need to escape）。")

print("\n=== 3. ★主菜：写 CSV 为什么必须 newline=\"\" —— 两份文件的真实字节对着看 ===")
p_blank = write_case("nl-blank.csv", "")          # 教科书写法
p_none = write_case("nl-none.csv", None)          # 手滑写法：open(p, 'w') 就是这个默认值
show_bytes(p_blank, 'newline=""')
show_bytes(p_none, "newline=None 默认")
print(f"  本机 os.linesep={os.linesep!r} → 写侧翻译是 \\n→\\n，所以这两份**在这台机器上字节相同**，")
print("  而且两份都带着 csv 自己写的 \\r\\n。差别不是假的，只是本机看不见 —— 那就把翻译层真的动")
print("  起来：显式 newline='\\r\\n' 等价于把 os.linesep 换成 \\r\\n（Windows 的默认值就是这个形状）。")
print("  ★ Windows 本身本机验不了，这一行按「机制实测 + 平台未验证」读，别当跨平台已验证结论。")
p_fake = write_case("nl-fake-win.csv", "\r\n")
show_bytes(p_fake, "newline='\\r\\n' 模拟")
back_fake = rows_of(p_fake)
print(f"      读回来（newline=''）→ {back_fake}")
print(f"      ★ 空记录 [] 出现 {back_fake.count([])} 次：\\r\\r\\n 把一行劈成两半，csv 只能认为")
print("        遇到了空行。列数没错、编码没错，行数却翻倍 —— 最难查的就是这种。")
print("  读侧两条世界线，读同一份 nl-blank.csv：")
with open(p_blank, encoding="utf-8") as fh:
    txt_default = fh.read()
print(f"      open 默认 newline 读到的 str → {txt_default!r}")
print(f"      open newline=''  读到的 str → {p_blank.read_text(encoding='utf-8', newline='')!r}")
p_cr = write_case("bare-cr.csv", "", [["x"], ["行1\r行2"]])
show_bytes(p_cr, "字段里带裸 \\r")
print(f"      newline='' 读 → {rows_of(p_cr)!r} ｜ 默认 newline 读 → {rows_of(p_cr, newline=None)!r}")
print("      ★ 那个 \\r 被悄悄换成 \\n：字段内容变了，一个异常都没有。")
print("  → 判据落在一行：csv 自己写 \\r\\n，文本层默认会再翻译一次 → \\r\\r\\n；")
print("    所以读写两侧都要 newline=\"\"，让 csv 自己判断哪里是行尾、哪里是字段内容。")

print("\n=== 4. 读侧 API：reader 是一次性迭代器，DictReader 的 key 从哪来 ===")
with open(p_blank, newline="", encoding="utf-8") as fh:
    it = csv.reader(fh)
    print(f"  csv.reader(...) 的类型 = {type(it).__name__} ｜ 是迭代器本身吗 → {iter(it) is it}")
    print(f"      next(it) → {next(it)!r}（拿到的是 list，不是 dict）")
    print(f"      list(it) → {list(it)!r}（游标已经走过第一行了）")
    print(f"      再 list(it) → {list(it)!r}  ★空且不报错 —— 呼应第 04 章「迭代器只能走一遍」")
body = rows_of(wtext("num.csv", "数量,单价\n42,3.5\n"))[0]
print(f"  数字列读回来 → {body!r} 元素类型 {[type(v).__name__ for v in body]}")
print("  ★ csv 不认识类型：那两列看着是数字，回来还是 str，int()/float() 必须你自己补一刀。")
print(f"  空文件：list(reader) = {rows_of(wtext('empty.csv', ''))}"
      f"（不是 None、不是报错，就是零行 —— 对它 next() 只会拿到 StopIteration）")
dr_hdr, rows_hdr = dict_rows("hdr.csv", "a,b\n")
print(f"  只有表头：fieldnames={dr_hdr.fieldnames} ｜ 数据行数={len(rows_hdr)}（表头不算数据行，但它是 key 的唯一来源）")
dr_wide, rows_wide = dict_rows("wide.csv", "姓名,年龄\n张三,20,多余A,多余B\n")
print(f"  列多于表头 → 多出来的挂在 key {dr_wide.restkey!r} 下：{rows_wide[0]}")
dr_short, rows_short = dict_rows("short.csv", "姓名,年龄,城市\n张三\n")
print(f"  列少于表头 → 缺的补 {dr_short.restval!r}：{rows_short[0]}（None 和空串在 csv 里永远分不出来）")
try:
    with open(WORK / "dw.csv", "w", newline="", encoding="utf-8") as fh:
        dw = csv.DictWriter(fh, fieldnames=["a", "b"])
        dw.writeheader()
        dw.writerow({"a": "1", "b": "2", "c": "3"})   # 多余键，默认 extrasaction='raise'
except (csv.Error, ValueError) as exc:
    print(f"  DictWriter 多余键（extrasaction='raise'）→ {type(exc).__name__}: {exc}")
    print("    ★ 它抛的是 ValueError 不是 csv.Error —— 查文档不如认家谱（第 07 章那棵树）")
with open(WORK / "dw-ignore.csv", "w", newline="", encoding="utf-8") as fh:
    csv.DictWriter(fh, fieldnames=["a", "b"], extrasaction="ignore").writerow({"a": "1", "b": "2", "c": "3"})
print(f"  extrasaction='ignore' → 磁盘上只剩 {(WORK / 'dw-ignore.csv').read_bytes()!r}（'c' 悄悄没了，和删列一样安静）")

print("\n=== 5. dialect：分隔符 / 行终止符，以及 Sniffer 各猜出什么 ===")
print(f"  delimiter=';'       → {oneline(['a', 'b', 'c;d'], delimiter=';')!r}")
print(f"  lineterminator=换行 → {oneline(['a', 'b'], lineterminator=NL)!r}")
SAMPLES = [("comma.csv", "姓名,年龄\n张三,20\n李四,30\n"), ("semi.csv", "a;b\nc;d\n")]
for name, text in sorted(SAMPLES):
    sniffed = csv.Sniffer().sniff(wtext(name, text).read_text(encoding="utf-8", newline="")[:2048])
    print(f"  Sniffer 看 {name:<10} → delimiter={sniffed.delimiter!r}"
          f" lineterminator={sniffed.lineterminator!r} quotechar={sniffed.quotechar!r}")
try:
    csv.Sniffer().sniff("只有一列\n还是只有一列\n")
except csv.Error as exc:
    print(f"  猜不出来时不是退回默认值，是报错：{type(exc).__name__}: {exc}")
print("  → Sniffer 是**猜**（数哪个字符出现得最规律），不是解析。生产代码把分隔符写死；")
print("    猜错那次不一定报错，可能只是静默把整表读成一列。")

print("\n=== 6. 两个坑的原文：裸换行没加引号，和字段大到超出限制 ===")
raw = "姓名,备注\n张三,第一行\n第二行\n"
rows_back = rows_of(wtext("bad-nl.csv", raw))
print(f"  字段里的换行没被引号包住时，磁盘上就长这样：{raw!r}")
print(f"      读出来 {len(rows_back)} 行 → {rows_back}")
print("      ★ 你以为写了 2 行数据，csv 读到 3 行：换行没被引号包住就是**行尾**，")
print("        它没有任何办法知道那是字段内容 —— 这是把 CSV 写坏的唯一方式。")
good = oneline(["张三", "第一行\n第二行"], lineterminator=NL)
print(f"      同样内容交给 csv.writer：{good!r} → 文本 {len(good.splitlines())} 行，"
      f"记录却是 {len(list(csv.reader(io.StringIO(good))))} 条 ← 这两个数从此不是一回事")
limit = csv.field_size_limit()
print(f"  csv.field_size_limit() 当前值 = {limit}（进程级常量，能直接打数字）")
csv.field_size_limit(10)
try:
    list(csv.reader(io.StringIO("x" * 100 + "\n")))
except csv.Error as exc:
    print(f"  现场把限制调到 {csv.field_size_limit()} 再读一个 100 字节的字段 → "
          f"{type(exc).__name__}: {exc}")
csv.field_size_limit(limit)
print(f"  调回去 = {csv.field_size_limit()} ｜ 调完照样读小字段 → {list(csv.reader(io.StringIO('ok,1')))}")
print("  → 这条是**内存防护**（防一个畸形字段撑爆你），不是格式规则；真要读大字段就调它，")
print("    但它是全局状态：在库代码里改它，等于改了同进程所有人读 CSV 的规则。")

print("\n=== 7. 收口：csv 管引号和分隔符，编码、类型、行尾都不归它 ===")
print("  ① 它管「引号与分隔符」这一套转义：逗号、双引号、换行都能进字段，")
print("     前提是必须让它自己写引号 —— 手工 split(',') 在这套规则面前必错（第 1 段）。")
print("  ② 它不管编码：encoding= 你自己给；Excel 导出的文件常带 BOM，用 utf-8-sig 才不被吃掉。")
print("  ③ 它不管类型：读回来**全是 str**（第 4 段那行类型名单），int() 得自己来；")
print("     None 与空串它也不分 ——「这格没填」和「这格填了 null 两个字」在 CSV 里同一个样。")
print("  ④ 它不管行尾：行尾归 newline=。csv 写 \\r\\n + 文本层再翻译 = \\r\\r\\n（第 3 段的字节为证）。")
print("  → 读 CSV 的正确三件套：")
print('      with open(p, encoding="utf-8-sig", newline="") as fh:')
print("          for row in csv.DictReader(fh):            # 表头当 key，顺序不靠猜")
print('              n = int(row["数量"])                   # csv 不给类型，这一刀必须你补')
print("  ★ 最后一句：字段里出现「没被引号包住的换行」，文件本身就已经坏了，")
print("    读它的人只会默默错位 —— 校验 CSV 时数的应该是**记录数**，不是行数。")

shutil.rmtree(WORK)
left = sorted(p.name for p in Path(".").iterdir() if p.name.endswith("-demo"))
print(f"\n  清理：{WORK.name}/ 已删除 → 还在吗 {WORK.exists()} ｜ 当前目录里 *-demo 残留 = {left or '无'}")
