#!/usr/bin/env python3
# ============================================================================
# 取证脚本 f_errors_param.py —— 第 08 章第 7 节：errors= 的五种结局和它的代价
#
# 这个脚本要证明什么：encoding= 决定「字节怎么变字符」，errors= 决定「变不成时怎么办」。
# 五个常见取值给出五种完全不同的结局：响 / 悄悄扔 / 换占位符 / 先留着 / 写成可见转义。
# 重点是**代价**：errors="ignore" 不是「宽容处理」，它把字节扔掉且不警告、程序照常退 0；
# 而 surrogateescape 是唯一能原样回去的那个 —— 读进来是临时座位，写回去还是那几个字节。
#
# 为什么要有它：日志、CSV、别人发来的配置里混进一个坏字节是常态。
# 选了 ignore 的人半年后发现数据少了一截，选 replace 的人发现文件里长出一堆 U+FFFD，
# 两种人都以为「当时没报错就说明没事」。这个脚本把「没事」拆开称重：丢了几字节、
# 有没有警告、退码是不是 0、能不能还原 —— 四条全部当场量出来。
#
# 现场文件全在 errors-demo/ 里现造现删；不打印时间戳与退出码数字，只打「正常结束=True/False」；
# 子进程输出全部 capture 后由本脚本打印，屏幕上不出现绝对路径。
# ============================================================================
import hashlib
import os
import shutil
import subprocess
import sys
import warnings
from pathlib import Path

WORK = Path("errors-demo")
PYNAME = os.path.basename(sys.executable)
BAD = b"\xff\xfe\x81"                       # 三个字节：UTF-8 里都不是合法的起始字节
PAYLOAD = "正常abc".encode("utf-8") + BAD + "尾巴".encode("utf-8")
READ_ERRORS = ("strict", "ignore", "replace", "surrogateescape", "backslashreplace")
WRITE_ERRORS = ("backslashreplace", "xmlcharrefreplace", "namereplace", "ignore", "replace")
WRITE_TEXT = "正常\U0001f642尾巴"           # 夹一个 emoji：gbk / latin-1 都装不下它

shutil.rmtree(WORK, ignore_errors=True)        # 上次崩在半路的残骸先清掉，保证每次同一现场
WORK.mkdir()

print("=== 1. 先造一份「明知不合法」的字节，把 hex 打出来给读者复现 ===")
bad_path = WORK / "坏字节.bin"
bad_path.write_bytes(PAYLOAD)
print(f"  构造式：'正常abc'.encode('utf-8') + {BAD!r} + '尾巴'.encode('utf-8')")
print(f"  落盘 hex = {PAYLOAD.hex()}")
print(f"  字节数 = {len(PAYLOAD)} ｜ 其中坏的是第 {len('正常abc'.encode('utf-8'))}~"
      f"{len('正常abc'.encode('utf-8')) + 2} 号（从 0 数）")
print(f"  逐字节看这三位：{', '.join(f'0x{b:02x}' for b in BAD)} —— "
      "UTF-8 里 0xff/0xfe 从来不是任何字符的首字节，0x81 是续字节却站在开头。")
print("  → 这一份字节**没有**任何修复余地：不是编码选错了，是这三位根本不是 UTF-8。")
print("  ★ 所以本节全部对照都用同一份 bytes，只换 errors=，别的都不动。")

print("\n=== 2. 读侧五种结局：同一份字节，errors= 换一个，屏幕上就换一种东西 ===")
for err in READ_ERRORS:
    try:
        with open(bad_path, encoding="utf-8", errors=err) as fh:
            text = fh.read()
    except UnicodeDecodeError as exc:
        print(f"  errors={err!r:<19} → 抛异常（原文）：{type(exc).__name__}: {exc}")
        print(f"      结构化字段：位置 {exc.start}~{exc.end}（字节下标）、原因 {exc.reason!r}")
        continue
    print(f"  errors={err!r:<19} → 字符数={len(text):<3} repr={text!r}")
print("  → 同一份 18 字节的文件：ignore 给 7 个字符、replace 和 surrogateescape 各给 10 个、")
print("    backslashreplace 给 19 个；strict 那格没有字符，只有异常。")
print("  → ignore 的 7 个字符看着「干净」，其实是被切掉了三块；replace 用 U+FFFD 顶位，"
      "坏字节至少留了个占位符。")
print("  → surrogateescape 把每个坏字节塞成一个 \\udcXX 代理码点 —— 它不是字符，是**临时座位**。")
print("  → backslashreplace 把坏字节写成 '\\xff' 这样的可见文本，字符数反而最多。")
print("  ★ 只有 strict 会响。剩下四个都在**改内容**，改完还给你一个「读取成功」。")

print("\n=== 3. 数一数 ignore 到底丢了几克数据：字节账 + 警告账 + 退出账 ===")
with warnings.catch_warnings(record=True) as caught:
    warnings.simplefilter("always")
    with open(bad_path, encoding="utf-8", errors="ignore") as fh:
        ignored = fh.read()
    out_path = WORK / "ignore回写.txt"
    with open(out_path, "w", encoding="utf-8") as fh:
        fh.write(ignored)
    warn_count = len(caught)
before = bad_path.stat().st_size
after = out_path.stat().st_size
print(f"  原文件 {before} 字节 → ignore 读出 {len(ignored)} 个字符 → 写回磁盘 {after} 字节")
print(f"  被悄悄扔掉的字节数 = {before} - {after} = {before - after}（正好是 {BAD!r} 那三个）")
print(f"  期间捕获到的警告条数 = {warn_count}（simplefilter('always') 已经把该报的都报了）")
child_src = ("import pathlib,sys\n"
             "p=pathlib.Path('坏字节.bin')\n"
             "t=p.read_text(encoding='utf-8', errors='ignore')\n"
             "pathlib.Path('子进程回写.txt').write_text(t, encoding='utf-8')\n")
kid = subprocess.run([sys.executable, "-c", child_src], env=os.environ, cwd=str(WORK),
                     capture_output=True, text=True, encoding="utf-8")
print(f"  同一个操作交给子进程 {PYNAME} -c 跑：正常结束 → {kid.returncode == 0}"
      f" ｜ 它往 stderr 打了字吗 → {bool(kid.stderr.strip())}")
kid_out = WORK / "子进程回写.txt"
kid_size = os.path.getsize(kid_out) if kid_out.exists() else -1
print(f"  子进程自己写回磁盘的那份 = {kid_size} 字节 —— 和上面那笔账对得上")
print("  ★ 没有一条警告，程序还退 0 ——「ignore 没报错」这件事本身，就是它最贵的地方。")
print(f"  → 差额 {before - after} 字节听起来不多，但这是一次**不可见的删除**："
      "同样写法跑在 8GB 日志上，少掉的东西没人能补回来。")

print("\n=== 4. surrogateescape 能原样回去，ignore 之后回不去（sha256 逐字对比）===")
with open(bad_path, encoding="utf-8", errors="surrogateescape") as fh:
    kept = fh.read()
back = WORK / "surrogate回写.bin"
with open(back, "w", encoding="utf-8", errors="surrogateescape") as fh:
    fh.write(kept)
h_orig = hashlib.sha256(bad_path.read_bytes()).hexdigest()
h_back = hashlib.sha256(back.read_bytes()).hexdigest()
print(f"  读进来 {len(kept)} 个字符：{kept!r}")
print(f"  用 errors='surrogateescape' 写回去：{back.read_bytes()!r}")
print(f"  原文件 sha256 前 16 位 = {h_orig[:16]} ｜ 回写后 = {h_back[:16]} ｜ 逐字节相同 → "
      f"{bad_path.read_bytes() == back.read_bytes()}")
lost = WORK / "ignore回写2.bin"
with open(lost, "w", encoding="utf-8", errors="surrogateescape") as fh:
    fh.write(ignored)
h_lost = hashlib.sha256(lost.read_bytes()).hexdigest()
print(f"  对照：ignore 的字符串再怎么写都造不回原文件 —— 回写 sha256 前 16 位 = {h_lost[:16]}"
      f" ｜ 相同 → {h_lost == h_orig}")
print("  那这些代理码点能不能用默认 errors 写出去？不能：")
try:
    with open(WORK / "撞一次.bin", "w", encoding="utf-8") as fh:
        fh.write(kept)
except UnicodeEncodeError as exc:
    print(f"    → {type(exc).__name__}: {exc}")
print("  → 判据：surrogateescape 是**成对**的。读的时候用它，写回去也必须用它，")
print("    中间那步换了 strict 就当场响（这是好事），换了 ignore 就悄悄丢。")

print("\n=== 5. 写侧的 strict：编码装不下时先撞两次（gbk / latin-1 各一次）===")
print(f"  待写入文本 = {WRITE_TEXT!r}（{len(WRITE_TEXT)} 个字符，夹一个 emoji）")
for enc in ("gbk", "latin-1"):
    path = WORK / f"strict-{enc}.txt"
    try:
        path.write_text(WRITE_TEXT, encoding=enc)
        print(f"  encoding={enc!r:<9} 竟然写成功了？→ 磁盘 = {path.read_bytes()!r}（本节不该走到这里）")
    except UnicodeEncodeError as exc:
        size = path.stat().st_size if path.exists() else -1
        print(f"  encoding={enc!r:<9} → 抛异常（原文）：{type(exc).__name__}: {exc}")
        print(f"      异常之后文件已存在吗 → {path.exists()}，磁盘字节数 = {size}")
print("  → 两条消息措辞不同：gbk 说 'illegal multibyte sequence'，")
print("    latin-1 说 'ordinal not in range(256)' —— 都是「这个 codec 装不下它」。")
print("  → 本机实测：失败之后磁盘上是 **0 字节**，不是「前半句已经落盘」。")
print("    原因在 str→bytes 那一步：整个字符串先编码再进缓冲区，编码器一抛，一个字节都没交出去。")
print("  ★ 别把这条外推：它说的是「一次 write 一个小字符串」。")
print("    缓冲区什么时候 flush 是另一件事（第 3、17 节）：长跑的日志程序崩在半路，")
print("    已经 flush 出去的那部分是真在盘上的。")

print("\n=== 6. 写侧五种 errors：落到磁盘上的真实文本，这一屏就是对照表 ===")
for enc, back_enc in (("gbk", "gb18030"), ("latin-1", "latin-1")):
    print(f"  encoding={enc}（回读时按 {back_enc} 解，只为了给眼睛看；磁盘字节以 repr 为准）")
    for err in WRITE_ERRORS:
        path = WORK / f"写{enc}-{err}.txt"
        try:
            with open(path, "w", encoding=enc, errors=err) as fh:
                fh.write(WRITE_TEXT)
        except (UnicodeEncodeError, LookupError) as exc:
            print(f"    errors={err!r:<20} → {type(exc).__name__}: {exc}")
            continue
        raw = path.read_bytes()
        print(f"    errors={err!r:<20} 磁盘字节数={len(raw):<4} 磁盘={raw!r}")
        if back_enc != enc:               # latin-1 的磁盘字节本来就是可读 ASCII，不重复打一行
            print(f"        按 {back_enc} 回读 → {raw.decode(back_enc)!r}")
print("  → backslashreplace 留下 '\\U0001f642'、xmlcharrefreplace 留下 '&#128578;'、")
print("    namereplace 留下 '\\N{SLIGHTLY SMILING FACE}' —— 三种都是**可读的替身**，")
print("    坏掉那个字符是谁，回文件里还查得回来。")
print("  → replace 打成一个 '?'、ignore 什么都不留：")
print("    latin-1 + ignore 写出来是 0 字节文件，整句中文凭空蒸发，而且不报错。")
try:
    b"\xff".decode("utf-8", errors="xmlcharrefreplace")
except TypeError as exc:
    print(f"  → 反过来，xmlcharrefreplace 用在**解码**方向会当场撞：{type(exc).__name__}: {exc}")
print("    所以写侧和读侧能用的 handler 名单不一样，不是同一套五个随便挑：")
print("    surrogateescape 只在解码方向有意义，namereplace / xmlcharrefreplace 只在编码方向。")

print("\n=== 7. errors='strict' 和「不写 errors」是不是同一回事：实测句柄上的 errors 属性 ===")
for label, kwargs in (("open(p, encoding='utf-8')", {"encoding": "utf-8"}),
                      ("open(p, encoding='utf-8', errors=None)", {"encoding": "utf-8", "errors": None}),
                      ("open(p, encoding='utf-8', errors='strict')",
                       {"encoding": "utf-8", "errors": "strict"}),
                      ("open(p, encoding='gbk')", {"encoding": "gbk"}),
                      ("open(p)", {})):
    fh = open(bad_path, **kwargs)
    fh.close()
    print(f"  {label:<42} → encoding={fh.encoding!r:<10} errors={fh.errors!r}")
print("  （最后一行 open(p) 是 encoding 和 errors **两个都不写**的那个写法）")
print("  → 是的，同一回事：不写 errors 就是 'strict'，一个字节都不放过。")
try:
    open(bad_path, "rb", errors="ignore")
except ValueError as exc:
    print(f"  二进制模式给 errors → {type(exc).__name__}: {exc}")
print("  → errors 是**文本层**的旋钮：b 模式根本没有字符的概念，"
      "想容错得自己 decode(errors=...)，那是同一批 handler 的另一条路。")
print(f"  → 顺带一个本机事实：sys.stdout.errors = {sys.stdout.errors!r}（这是基线 env 下的值）。")
print("    标准流不归 open() 管，它的编码和 errors 由 locale / PYTHONIOENCODING 决定 —— "
      "见第 6 节那张表。")

print("\n=== 8. 收口：哪个会响 / 哪个会改内容 / 哪个能原样回去 ===")
print("  errors= 后面三列分别是：会响吗 ｜ 改不改内容 ｜ 还能不能还原原字节")
verdicts = (
    ("strict", "★会响", "不改内容（直接失败）", "文件没动过，当然能"),
    ("ignore", "不响", "★改：把坏字节删掉", "不能，删了就没了"),
    ("replace", "不响", "改：换成 U+FFFD", "不能，一长串坏字节会塌成同一种占位符"),
    ("surrogateescape", "不响", "改：换成 \\udcXX", "★能，写回去逐字节相同（第 4 段 sha256 已验）"),
    ("backslashreplace", "不响", "改：换成 '\\xff' 文本", "不能原样，但内容留在文件里、肉眼可读"),
)
for row in verdicts:
    print(f"    {row[0]:<17}｜ {row[1]:<7}｜ {row[2]:<19}｜ {row[3]}")
print("  → 判据一：五个取值里只有 strict 会响；**「读成功」不等于「读对了」**，"
      "另外四个都在悄悄改内容。")
print("  → 判据二：要「先留着、以后原样写回去」只有 surrogateescape（第 4 段那对 sha256）。"
      "读写必须成对使用同一个 errors。")
print("  → 判据三：要「人眼还要能看出这里坏过」用 replace 或 backslashreplace；"
      "要「坏字节也要当文本存档」用 surrogateescape。")
print("  ★ 永远不要把 ignore 当「宽容处理」：它是删除。第 3 段量到了本次删除的确切重量 —— "
      f"{before - after} 字节、0 条警告、退码 0；")
print("    一次删三个字节你会觉得无所谓，同一个循环跑一晚上日志，你连少了哪一段都不知道。")

shutil.rmtree(WORK)
print(f"\n  清理：{WORK.name}/ 还在吗 → {WORK.exists()} ｜ 上面每个数字都出自这一次运行，"
      "重跑逐字一致。")
