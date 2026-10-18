#!/usr/bin/env python3
# ============================================================================
# 取证脚本 e_encoding_default.py —— 第 08 章第 6 节：默认编码是从哪来的，BOM 是怎么回事
#
# 这个脚本要证明什么：open() 不写 encoding 时用的那个编码，来自**进程启动时的 locale**，
# 不来自文件内容、也不来自 sys.getdefaultencoding()。四个「默认编码」是四个不同的东西，
# 各管各的方向；再叠加 PYTHONUTF8 / PYTHONIOENCODING / PYTHONCOERCECLOCALE 三个开关，
# 同一份脚本在不同环境下能读出完全不同的字节 —— 所以「在我机器上好的」不成立。
#
# 为什么要有它：本章最常见的两类事故是「换台机器就乱码」和「首行判断莫名失败」。
# 前者只有把开关逐个拨一遍打在一张表里才看得清；后者多半就是一个 BOM。
# 本脚本不改自己的环境（demo.sh 已把三个开关 unset，那就是基线）：所有开关只写在
# subprocess 的 env 字典里，子进程输出全部 capture 后由本脚本打印。
# 现场文件全在 encoding-demo/ 里现造现删；屏幕上不出现绝对路径。
# ============================================================================
import codecs
import locale
import os
import shutil
import subprocess
import sys
from pathlib import Path

WORK = Path("encoding-demo")
PYNAME = os.path.basename(sys.executable)      # 只上屏幕 basename，不打绝对路径
shutil.rmtree(WORK, ignore_errors=True)        # 上次崩在半路的残骸先清掉，保证每次同一现场
WORK.mkdir()

print("=== 1. 四个「默认」到底是谁，本机实测值 + 各管哪个方向 ===")
de = sys.getdefaultencoding()
fs = sys.getfilesystemencoding()
pref = locale.getpreferredencoding(False)
print(f"  sys.getdefaultencoding()          = {de!r:<16} 管 str.encode()/bytes.decode() 不写编码时，**和文件无关**")
print(f"  sys.getfilesystemencoding()       = {fs!r:<16} 管文件名 / argv / env 的 str↔bytes")
print(f"  locale.getpreferredencoding(False)= {pref!r:<16} 管 open() 不写 encoding 时")
print(f"  sys.stdout.encoding               = {sys.stdout.encoding!r:<16} 管 print() 到屏幕，"
      f"errors={sys.stdout.errors!r}")
print(f"  locale.getlocale()                = {locale.getlocale()!r}")
with open(WORK / "探测.txt", "w", encoding=None) as fh:   # encoding=None = 按默认走
    real, real_errors = fh.encoding, fh.errors
print(f"  open(p, encoding=None).encoding    = {real!r}（errors={real_errors!r}）—— 实测，不是照抄")
print(f"  → open() 用的是 getpreferredencoding(False)：相等 → {real == pref}；"
      f"和 getdefaultencoding() 的字符串相等吗 → {real == de}。")
print(f"  → 但 codecs.lookup 之后两者是同一个 codec → "
      f"{codecs.lookup(real).name == codecs.lookup(de).name}"
      f"（{real!r} 是 locale 给的写法，{de!r} 是解释器给的写法，比的是**出处**）")
print("  ★ 判据第一条：四个默认各管一个方向。搞混了就出现「文件名能建、内容读不出」这类怪事。")

print("\n=== 2. 开关现场对比：同一个子进程小程序，六套 env，结果打一张表 ===")
CHILD = (
    "import sys, locale\n"
    "print('de', sys.getdefaultencoding())\n"
    "print('fs', sys.getfilesystemencoding())\n"
    "print('pref', locale.getpreferredencoding(False))\n"
    "print('out', sys.stdout.encoding + ':' + sys.stdout.errors)\n"
    "print('loc', str(locale.getlocale()).replace(' ', ''))\n"
    "print('u8', sys.flags.utf8_mode)\n"
    "with open('probe.txt', 'w', encoding=None) as fh:\n"
    "    print('open', fh.encoding)\n"
)
BASE = {"LC_ALL": "C.UTF-8", "LANG": "C.UTF-8"}
# 三元组：编号、这套 env、它的含义（编号进表，含义在表下面逐条解释，少写一份并列的表）
CASES = (
    ("A", BASE, "基线：LC_ALL=C.UTF-8，三个开关都不设（就是本条 demo 命令拨好的位置）"),
    ("B", {**BASE, "PYTHONUTF8": "1"}, "加 PYTHONUTF8=1 → 强制开 PEP 540 UTF-8 模式"),
    ("C", {**BASE, "PYTHONIOENCODING": "utf-8:replace"}, "加 PYTHONIOENCODING → 只管标准流的编码和 errors"),
    ("D", {**BASE, "LC_ALL": "C", "LANG": "C"}, "换 LC_ALL=C（POSIX locale）"),
    ("E", {**BASE, "LC_ALL": "C", "LANG": "C", "PYTHONCOERCECLOCALE": "0"},
     "LC_ALL=C ＋ 关掉 PEP 538 的 locale 自动改造"),
    ("F", {**BASE, "LC_ALL": "C", "LANG": "C", "PYTHONCOERCECLOCALE": "0", "PYTHONUTF8": "0"},
     "LC_ALL=C ＋ 把 UTF-8 模式也按住"),
)
COLS = ("de", "fs", "pref", "out", "loc", "u8", "open")
rows = []
for tag, env, _desc in CASES:
    done = subprocess.run([sys.executable, "-c", CHILD], env=env, cwd=str(WORK),
                          capture_output=True, text=True, encoding="utf-8", errors="replace")
    got = dict(line.split(" ", 1) for line in done.stdout.splitlines() if " " in line)
    rows.append((tag, [got.get(c, "读取失败") for c in COLS], done.returncode == 0))
print(f"  子进程命令统一是：{PYNAME} -c <同一段 {len(CHILD.strip().splitlines())} 行小程序>，"
      "只有 env 不同（屏幕上不出现解释器路径）")
print("  编号  defaultenc filesystem preferredencoding      stdout:errors          getlocale        utf8mode open")
for tag, vals, ok in rows:
    print(f"  {tag:<5} {vals[0]:<9} {vals[1]:<9} {vals[2]:<22} {vals[3]:<22} {vals[4]:<16} {vals[5]:<8} {vals[6]:<8} 正常结束={ok}")
print("  每套 env 的含义：")
for tag, _env, desc in CASES:
    print(f"    · {tag} {desc}")
print("  → sys.getdefaultencoding() 六格全不变（3.7 起编译期就固定成 utf-8）—— "
      "这正好证明它**不是** open() 的默认编码。")
print("  → 只有 locale.getpreferredencoding(False) 跟着动，而 open 那一列和它逐格相等："
      "open() 的默认编码就是这个函数的返回值。")
print("  → 大小写能认出来源：'UTF-8'（大写）是 locale 报的，'utf-8'（小写）是 UTF-8 模式给的；")
print("    D、E 两格 pref=utf-8 且 utf8mode=1 —— **LC_ALL=C 会自动打开 UTF-8 模式**，")
print("    所以「POSIX locale 就是 ascii」这句老话在本机默认不成立。")
print("  → 只有 F（把 UTF-8 模式也按住）才让 default 真变成 ANSI_X3.4-1968，也就是 ascii。")
print("  → sys.stdout.errors 基线就是 'surrogateescape'，这是 print 出坏字节不报错的根；"
      "C 那一格被 PYTHONIOENCODING 改成了 replace。")

print("\n=== 3. 现场撞一次：F 那套 env 下 print 一个中文会怎样 ===")
crash = subprocess.run([sys.executable, "-c", r"print('\u4e2d')"],
                       env=CASES[5][1], cwd=str(WORK), capture_output=True,
                       text=True, encoding="utf-8", errors="replace")
last = crash.stderr.strip().splitlines()[-1] if crash.stderr.strip() else "（没有 stderr）"
print(f"  子进程 {PYNAME} -c \"print('\\\\u4e2d')\" 正常结束吗 → {crash.returncode == 0}（退非 0）")
print(f"  屏幕上打出的字节数 → {len(crash.stdout)}（一个字都没出去）")
print(f"  异常原文：{last}")
ok_case = subprocess.run([sys.executable, "-c", r"print('\u4e2d')"],
                         env=BASE, cwd=str(WORK), capture_output=True,
                         text=True, encoding="utf-8", errors="replace")
print(f"  同一句在基线 A 下：输出 = {ok_case.stdout!r}，正常结束 = {ok_case.returncode == 0}")
print("  ★ 代价：这段代码一个字都没改，改的只有环境变量。"
      "「我这台机器能跑」在这件事上一点证据都不是。")

print("\n=== 4. 同一串文本，四种 codec 各给几个字节；latin-1 永不报错 ≠ 读对了 ===")
sample = "中文字幕"
print(f"  编码方向：{sample!r}（4 个汉字）")
for enc in ("utf-8", "gbk", "gb18030", "latin-1"):
    try:
        raw = sample.encode(enc)
        print(f"    {enc:<9} 字节数={len(raw):<3} hex 前 12={raw.hex()[:12]} 字节={raw!r}")
    except UnicodeEncodeError as exc:
        print(f"    {enc:<9} {type(exc).__name__}: {exc}")
emoji = "\U0001f642"
cells = []
for enc in ("utf-8", "gbk", "gb18030", "latin-1"):
    try:
        cells.append(f"{enc}={len(emoji.encode(enc))} 字节")
    except UnicodeEncodeError:
        cells.append(f"{enc}=编不下")
print(f"  再加一个 emoji {emoji!r}：" + "、".join(cells) + "（逐个真试过，不是背的）")
print("  解码方向才是重点：把上面那串 **UTF-8 字节**当 latin-1 读：")
raw = sample.encode("utf-8")
mojibake = raw.decode("latin-1")
print(f"    这串字节共 {len(raw)} 个：{raw.hex()}")
print(f"    latin-1 解出 {mojibake!r}")
print(f"    {len(mojibake)} 个字符，**没报任何错** —— 每个坏字节都领到了一个合法字符")
print(f"    再 latin-1 编回去能还原字节吗 → {mojibake.encode('latin-1') == raw}"
      "（latin-1 是 0x00~0xFF 的一一映射）")
print("  ★ latin-1 永远不会报错，它只是把乱码给你：0x00~0xFF 每个字节都有对应字符，"
      "所以「读成功了」和「读对了」是两件事。")
print("  反方向才有响：把 latin-1 的字节当 utf-8 解 ——")
cafe = "café".encode("latin-1")
try:
    cafe.decode("utf-8")
except UnicodeDecodeError as exc:
    print(f"    'café'.encode('latin-1') = {cafe.hex()} → {type(exc).__name__}: {exc}")
print("    这就是「文件明明存在，一读就 UnicodeDecodeError」的机制：字节是对的，codec 是错的。")

print("\n=== 5. BOM：三个字节让首行判断当场失败 ===")
conf = "# 配置说明\n值=1\n"
bom_bytes = conf.encode("utf-8-sig")
print(f"  同一份文本，utf-8 编出 {len(conf.encode('utf-8'))} 字节，"
      f"utf-8-sig 编出 {len(bom_bytes)} 字节，多的那三个 hex = {bom_bytes[:3].hex()}")
bom_single = "\ufeff".encode("utf-8").hex()    # 一个 U+FEFF 自己编出来就是那三个字节
print(f"  而 '\\ufeff'.encode('utf-8') = {bom_single} —— BOM 就是 U+FEFF 的 UTF-8 编码本身")
path = WORK / "带BOM.conf"
path.write_bytes(bom_bytes)
with open(path, encoding="utf-8") as fh:
    plain = fh.read()
with open(path, encoding="utf-8-sig") as fh:
    sig = fh.read()
print(f"  encoding='utf-8'     读到 {plain!r} ← 首字符是 U+{ord(plain[0]):04X}")
print(f"  encoding='utf-8-sig' 读到 {sig!r} ← 头三个字节被吃掉；没有 BOM 的文件用它读也正常 → "
      f"{conf.encode('utf-8').decode('utf-8-sig') == conf}")
print("  现场撞一次：跳过注释行的那段代码，两种读法各跑一遍：")
for label, text in (("utf-8", plain), ("utf-8-sig", sig)):
    kept = [ln for ln in text.splitlines() if ln.strip() and not ln.startswith("#")]
    print(f"    encoding={label:<10} 首行 startswith('#') = {text.splitlines()[0].startswith('#')!s:<6} "
          f"→ 当成配置的行数 = {len(kept)}（正确应是 1）")
print("  → 首行那条注释没被认出来，配置里凭空多出一行垃圾；报错没有，警告没有。")
print("  UTF-16 的 BOM 与字节序（同一份文本，只打头四字节）：")
for enc in ("utf-16", "utf-16-le", "utf-16-be"):
    print(f"    {enc:<10} 头四字节 hex = {conf.encode(enc)[:4].hex():<10} 全串字节数 = {len(conf.encode(enc))}")
print("    ff fe = 小端标记、fe ff = 大端标记；utf-16 带标记而 -le/-be 不带 —— "
      "所以不带 BOM 的 UTF-16 只能靠约定猜字节序。")

print("\n=== 6. 文件名是字节，str 只是按 fsencoding 解码后的样子 ===")
(WORK / "中文文件名.txt").write_text("x", encoding="utf-8")
with open(WORK / "\U0001f642.txt", "w", encoding="utf-8") as fh:
    fh.write("y")
weird_name = b"\xff\xfe\xe4\xb8\xad.txt"                        # 开头两字节不是合法 UTF-8
weird_full = os.path.join(os.fsencode(WORK), weird_name)        # 字节名字 + 字节目录，直接递给内核
fd = os.open(weird_full, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
os.close(fd)
print(f"  在 {WORK}/ 里建了 4 个名字：两个中文、一个 emoji、一个开头字节根本不构成 UTF-8 的")
print(f"  os.listdir(str) 拿到的（已排序）：{sorted(os.listdir(WORK))}")
print("  os.listdir(bytes) 拿到的同一批（已排序）—— 这次是字节：")
for name in sorted(os.listdir(os.fsencode(WORK))):
    print(f"    hex={name.hex():<40} 字节数={len(name):<3} 按 {fs} 解出来 = {os.fsdecode(name)!r}")
print("  → 同一个目录项有两种写法：字节是内核里的真身，str 是 Python 按 filesystemencoding")
print("    （+surrogateescape）替你解出来的样子。")
print("  → 那个脏名字开头 0xff 0xfe 不是合法 UTF-8，解不成真字符就各配一个代理码点；")
print("    \\udcff\\udxfe 不是「一个字符」，是「一个存不下去的字节」的临时座位。")
print("    用带代理码点的 str 名把它删掉 → ", end="")
try:
    os.remove(os.fsdecode(weird_full))
    print("成功（说明 str 名和 bytes 名指的是同一个目录项，往返无损）")
except OSError as exc:
    print(f"{type(exc).__name__}: {exc}")
print("  ★ 代价：把这种进程换成第 2 段 F 那格（fsencoding=ascii），")
print("    上面那个中文名字连**建都建不出来** —— open 之前就先撞一条：")
print("    UnicodeEncodeError: 'ascii' codec can't encode characters")
print("    因为文件名这一步就得先把 str 编成字节，才谈得上递给内核。")

print("\n=== 7. 收口：默认编码来自 locale，不来自你的文本 ===")
print("  → 判据一：open() 的默认编码 = locale.getpreferredencoding(False)，")
print("    它由进程的 locale 和 PYTHONUTF8 决定；文件内容里没有任何信息能改变它。")
print("  → 判据二：sys.getdefaultencoding() 管 str↔bytes 的隐含转换，")
print("    sys.getfilesystemencoding() 管文件名，sys.stdout.encoding 管 print —— 四个互不替补。")
print("  → 判据三：latin-1 解码永不报错、utf-8 解码会报错。「没报错」不是「读对了」的证据。")
print("  → 判据四：带 BOM 的 UTF-8 用 encoding='utf-8' 读会留下 \\ufeff，首行判断当场失手；")
print("    要读 Windows 记事本 / Excel 导出的文件就用 'utf-8-sig'（没 BOM 的它也能读）。")
print("  写死的做法只有一条，写进肌肉记忆：")
print('    with open(path, encoding="utf-8") as fh: ...     # encoding 永远显式写，别赌默认')
print("  ★ 代价：不写 encoding 的脚本，读的是「这台机器的配置」而不是「这个文件的编码」——")
print("    换机器、换容器、换 CI、加一个 PYTHONUTF8，结果就变，而且常常是悄悄变。")

shutil.rmtree(WORK)
print(f"\n  清理：{WORK.name}/ 还在吗 → {WORK.exists()} ｜ 本进程环境一个变量都没改，"
      "开关只给在子进程的 env 字典里。")
