#!/usr/bin/env python3
# ============================================================================
# 取证脚本 r_cli_capstone.py —— 第 08 章第 19 节：把 1~18 节的零件拧成一个真能跑的 CLI 工具
#
# 前 18 节每节量一个零件，这一节把它们拼起来：`cfgnorm` —— 安全读一份 JSON 配置、按契约
# 校验、规范化，再把结果**原子**写出去，顺带产一份 CSV 体检单。它没有一行是新知识：
# 每一处写法都回指本章某一节，末段那张对照表逐行编号，并且自己核对 1..18 有没有漏。
#
# 三条价值观各落在一个位置上：
#   ① 读侧：encoding 和 errors 都写死（第 6、7 节）—— 默认值是别人的决定；
#   ② 写侧：产出只走「同目录 tmp → flush → fsync → chmod → os.replace」（第 17 节），
#      目标文件上绝不就地写（第 2 节：'w' 打开那一刻旧的就已经没了）；
#   ③ 守卫：任何「数出来等于 0」的对账，先当场故意让它响一次再报 0（第 20 节那条纪律）。
#      第 4 段种两份残留 .tmp（其中一份名字以点开头）把它弄响，并把三种数法并排打出来：
#      iterdir 抓到 2 个、glob 模块只抓到 1 个、Path.glob 抓到 2 个 —— 第 11 节那条
#      「同一个 `*` 两套规矩」就在这里兑现：拿 glob 模块写的对账会永远少报那一个。
#
# 安全：只用标准库，不联网、不装包、不开端口；写操作全落在当前目录的 capstone-demo/ 里，
#   atexit 兜底 rmtree；屏幕上只有相对路径和 basename，无绝对路径、无时间、无随机名；
#   dict 与目录一律 sorted()，所以跑两遍逐字一致。
# 用法：python3 r_cli_capstone.py                                        ← 无参数 = 自检
#       python3 r_cli_capstone.py --in x.json --out <目录> [--check-only]  ← 当真工具用
# ============================================================================
import atexit
import csv
import errno
import glob
import io
import json
import locale
import os
import shutil
import stat
import sys
from pathlib import Path

WORK = Path("capstone-demo")
IN_DIR = WORK / "in"
OUT_DIR = WORK / "out"
TMP = ".tmp"
BOM = b"\xef\xbb\xbf"
RET_OK, RET_BAD = 0, 1              # 工具只区分「产出了」和「没产出」两种
HEADER = ["键", "来的是", "契约要", "落的是", "处置"]
# 配置契约：json 的世界只有六种值（第 16 节），所以校验的天花板就是这张类型表
SPEC = {"name": (str, "app"), "port": (int, 8080), "tags": (list, []),
        "debug": (bool, False), "ratio": (float, 1.0)}


def usage():
    return ["cfgnorm —— 安全读配置、规范化产出（本章 1~18 节的总成）",
            "  用法：python3 r_cli_capstone.py --in <配置.json> --out <目录> [--check-only]",
            "  产出：<目录>/<名>.normalized.json（UTF-8、键 sorted、indent=2、LF）",
            "        <目录>/<名>.audit.csv      （体检单：每个契约键一行，写清落的是什么、为什么）",
            "  约定：产出只走同目录 tmp + os.replace；开工前先扫上一版崩留下的 .tmp 并报警",
            "  失败即不产出：读不到 / JSON 坏了 / 参数认不出，一律一个字节都不往目标上写"]


# ------------------------------------------------------------------ 工具本体 --
def sweep_stale_tmp(d):
    """对账：目录里所有以 .tmp 收尾的名字（第 17 节「别让 .tmp 留在目录里等下次咬人」）。
    ★用 iterdir 而不是 glob 模块：glob 的 `*` 不匹配点开头的名字（第 11 节），那一版会漏掉
      以点开头的残留 —— 三种数法各抓到几个，第 4 段本机实测。"""
    return sorted(p.name for p in d.iterdir() if p.name.endswith(TMP))


def read_config_text(path):
    """读侧四件事全写死（第 5、6、7、12 节）：字节先钉住、BOM 交给 utf-8-sig、errors=replace。"""
    raw = path.read_bytes()                          # 第 12 节：先问内核要**字节**，形状才钉得住
    with open(path, encoding="utf-8-sig", errors="replace") as fh:
        text = fh.read()
    return {"text": text, "bytes": len(raw), "chars": len(text),
            "had_bom": raw.startswith(BOM), "crlf_on_disk": raw.count(b"\r\n"),
            "cr_in_text": text.count("\r"), "replaced": text.count("\ufffd"),
            "enc": fh.encoding, "errs": fh.errors}    # 句柄属性是实测值，不是我写的期望值


def parse_config(text):
    """第 16 节：JSONDecodeError 不是玄学，四个字段结构化交回，原文一个字都不改写。"""
    try:
        return json.loads(text), None
    except json.JSONDecodeError as exc:
        return None, (f"{type(exc).__name__}: {exc} ｜ msg={exc.msg!r} lineno={exc.lineno}"
                      f" colno={exc.colno} pos={exc.pos}"
                      f" ｜ 错在这一行：{exc.doc.splitlines()[exc.lineno - 1]!r}")


def normalize(doc):
    """按 SPEC 校验 + 规范化：返回 (干净 dict, 体检行)。判据只有「落的是什么、为什么」。"""
    clean, rows = {}, []
    for key in sorted(set(doc) | set(SPEC)):
        if key not in SPEC:
            rows.append([key, json.dumps(doc[key], sort_keys=True, ensure_ascii=False),
                         "<契约里没有>", "<丢弃>", "★契约外的键：丢掉，不写进产出"])
            continue
        want, default = SPEC[key]
        if key not in doc:
            clean[key] = default
            rows.append([key, "<缺失>", want.__name__, json.dumps(default), "补默认值"])
            continue
        got, why = doc[key], "类型对，原样保留"
        if want is int and isinstance(got, bool):
            got, why = int(got), "bool 冒充 int（bool 是 int 的儿子，第 07 章那条）"
        elif want is int and isinstance(got, str) and got.isdigit():
            got, why = int(got), "数字字符串 → int"
        elif want is float and isinstance(got, int) and not isinstance(got, bool):
            got, why = float(got), "int → float（契约要浮点，别让它停在整型）"
        elif want is str and isinstance(got, (int, float)) and not isinstance(got, bool):
            got, why = str(got), "数字 → str"
        elif want is list and isinstance(got, list) and any(not isinstance(x, str) for x in got):
            got, why = [str(x) for x in got], "数组里混非字符串 → 逐个 str()"
        elif not isinstance(got, want):
            got, why = default, f"★类型不符（来的是 {type(got).__name__}）→ 退回默认值"
        clean[key] = got
        rows.append([key, json.dumps(doc[key], sort_keys=True, ensure_ascii=False),
                     want.__name__, json.dumps(got, sort_keys=True, ensure_ascii=False), why])
    return clean, rows


def render_csv(rows):
    """csv 自己写 \\r\\n（第 15 节）：这里先进 StringIO，再由 atomic_write 原样落盘。"""
    buf = io.StringIO()
    writer = csv.writer(buf)
    writer.writerow(HEADER)
    writer.writerows(rows)
    return buf.getvalue()


def atomic_write(path, text, fix_mode=True, before_replace=None):
    """第 17 节四步曲，一步都不能少：同目录 tmp → 一次写全 → flush+fsync → chmod → replace。"""
    tmp = path.with_name(path.name + TMP)            # 同目录 + 名字确定（随机名会让屏幕抖）
    try:
        with tmp.open("w", encoding="utf-8", newline="\n") as fh:   # 第 5 节：newline="\n" = 不翻译
            fh.write(text)
            fh.flush()                               # 第 3 节：flush 才是缓冲区边界，write 不是
            os.fsync(fh.fileno())                    # 只到「交给内核」为止；掉电行为本机**未验证**
        mode_tmp = stat.S_IMODE(os.stat(tmp).st_mode)
        if fix_mode:
            os.chmod(tmp, 0o644)                     # 第 13 节：必须在 replace 之前 —— 权限跟 inode 走
        if before_replace is not None:
            before_replace(path, tmp)                # 给「读者此刻看到什么」留一个探测点
        os.replace(tmp, path)                        # 同目录 → rename(2) 原子替换
        return {"tmp": tmp.name, "mode_tmp": mode_tmp,
                "mode_out": stat.S_IMODE(os.stat(path).st_mode)}
    except BaseException:
        tmp.unlink(missing_ok=True)                  # 第 10 节：missing_ok 才幂等
        raise


def main(argv):
    """CLI 主流程：读 → 解析 → 规范化 → 产出。返回 (判定码, 现场信息)，不自己往屏幕上写。"""
    opt = {"in": None, "out": OUT_DIR, "check": False, "unknown": []}
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg in ("--in", "--out") and i + 1 < len(argv):
            opt[arg[2:]] = Path(argv[i + 1])
            i += 2
        elif arg == "--check-only":
            opt["check"] = True
            i += 1
        else:
            opt["unknown"].append(arg)
            i += 1
    info = {"why": "", "stale": [], "rows": []}
    if opt["unknown"] or opt["in"] is None:
        info["why"] = f"认不出或缺参数 {sorted(opt['unknown'])} → 不猜、不产出"
        return RET_BAD, info
    src, out_dir = opt["in"], opt["out"]
    info["src"] = src.name
    out_dir.mkdir(parents=True, exist_ok=True)        # 第 10 节：幂等
    info["stale"] = sweep_stale_tmp(out_dir)          # 先扫残留，别让上一版的半截咬着这一版
    for name in info["stale"]:
        (out_dir / name).unlink()
    try:
        info.update(read_config_text(src))
    except OSError as exc:
        info["why"] = (f"{errno.errorcode.get(exc.errno, '?')}：读不到 {src.name} → 不产出"
                       f"（{type(exc).__name__}）")
        return RET_BAD, info
    doc, err = parse_config(info["text"])
    if err:
        info["why"] = err
        return RET_BAD, info
    clean, rows = normalize(doc)
    info["rows"] = rows
    info["json_text"] = json.dumps(clean, ensure_ascii=False, sort_keys=True, indent=2) + "\n"
    info["csv_text"] = render_csv(rows)
    if opt["check"]:
        info["why"] = "--check-only：校验通过，按约定一个字节都不落盘"
        return RET_OK, info
    jp = out_dir / (src.stem + ".normalized.json")
    cp = out_dir / (src.stem + ".audit.csv")
    info["jinfo"] = atomic_write(jp, info["json_text"])
    info["cinfo"] = atomic_write(cp, info["csv_text"])
    info["jp"], info["cp"] = jp, cp
    info["why"] = f"产出 {jp.name} + {cp.name}"
    return RET_OK, info


def count_tmp_glob(d):
    """对照组：大家最爱写的那种对账 —— glob 模块的 `*`。第 4 段拿它和 iterdir 版并排数。"""
    return sorted(os.path.basename(x) for x in glob.glob("*" + TMP, root_dir=str(d)))


def cleanup():
    """退出前的固定动作：整份删自己建的沙箱，并把「删干净了吗」打在屏幕上。"""
    if WORK.exists():
        shutil.rmtree(WORK)
    print(f"\n  清理自证：{WORK.name}/ 已删除 → 还在吗 {WORK.exists()}"
          f" ｜ 当前目录剩下的 *-demo = "
          f"{sorted(p.name for p in Path('.').iterdir() if p.name.endswith('-demo')) or '无'}"
          f" ｜ umask 已还回进来之前的 {oct(OLD_UMASK)}")


OLD_UMASK = os.umask(0o022)                           # 钉住 umask（第 1 节）：否则权限数字各人不同
atexit.register(cleanup)

# -------------------------------------------------------------------- 自检 ----
print("=== 1. 工具亮相：用法、契约、现场布局（幂等建目录 + Path 纯算拆名字）===")
for line in usage():
    print("  " + line)
IN_DIR.mkdir(parents=True, exist_ok=True)
IN_DIR.mkdir(parents=True, exist_ok=True)             # 第二次调用：静默 —— 幂等才敢重复部署
print(f"\n  {IN_DIR.name}/ 连建两次都没抛 → {IN_DIR.is_dir()}"
      f" ｜ {OUT_DIR.name}/ 还没人建，exists={OUT_DIR.exists()}（main() 会自己补上）")
probe = IN_DIR / "app.json"
print("  Path 纯算（第 8、9 节：对**根本不存在**的名字照样算得出，一个异常不抛）：")
print(f"    str={str(probe)!r} ｜ parts={probe.parts} ｜ name={probe.name!r}"
      f" ｜ suffix={probe.suffix!r} ｜ stem={probe.stem!r}")
print(f"    同一行输入交给 os.path：split={os.path.split(str(probe))}"
      f" ｜ 两派算出来的 stem 相同吗 → {os.path.splitext(probe.name)[0] == probe.stem}")
blocked = WORK / "挡住.txt"
blocked.write_text("我不是目录", encoding="utf-8")      # 脚手架用 write_text 无所谓（第 12 节），产出不这么写
for label, act in (("父级不存在时 mkdir()", lambda: (IN_DIR / "没这个" / "x").mkdir()),
                   ("名字被普通文件占着时 mkdir()", blocked.mkdir),
                   ("中间是普通文件时 mkdir(parents=True)",
                    lambda: (blocked / "子" / "孙").mkdir(parents=True))):
    try:
        act()
        print(f"    {label:<34} → ★没响（那这条结论作废）")
    except OSError as exc:
        print(f"    {label:<34} → {type(exc).__name__}: [Errno {exc.errno}]"
              f" {errno.errorcode.get(exc.errno)}（{exc.strerror}）")
print("  → 三个 errno 三条处置路径（第 10 节）：ENOENT 少一层、EEXIST 名字被占、ENOTDIR 中间被文件挡住；")
print("    写成 except OSError: pass 等于把三种事故压成一次静默失败。")
FIXTURES = {
    "app.json": '{"name": "生产网关", "port": 9000, "tags": ["a", "b"], "debug": false, "ratio": 0.5}',
    "wrongtypes.json": '{"port": "8080", "tags": {"a": 1}, "debug": 1, "备注": "契约外的键"}',
    "bom.json": '{"name": "windows 那边存的", "port": 7000, "tags": [], "debug": true, "ratio": 2}',
    "dirty.json": '{"name": "尾巴 here 有个坏字节", "port": 6000, "tags": ["x"], "debug": false, "ratio": 1.5}',
    "broken.json": '{"name": "甲", "port": 1,}',
    "gbk编码.json": '{"name": "老机器存的", "port": 2}',
}
for name in sorted(FIXTURES):
    body = FIXTURES[name]
    target = IN_DIR / name
    if name == "bom.json":
        target.write_bytes(BOM + body.replace(",", ",\r\n").encode("utf-8"))
    elif name == "dirty.json":
        target.write_bytes(body.encode("utf-8").replace(b"here", b"here" + b"\xff\xfe"))
    elif name == "gbk编码.json":
        target.write_bytes(body.encode("gbk"))
    else:
        target.write_text(body, encoding="utf-8")
print(f"  现场布置：{IN_DIR.name}/ 放了 {len(FIXTURES)} 份配置（现造现删）")
print(f"    {[(p.name, p.stat().st_size) for p in sorted(IN_DIR.iterdir())]}")
print("    ← 六个字节形状各不同的输入：干净 / 类型错 / BOM+CRLF / 坏字节 / 语法坏 / gbk")

print("\n=== 2. 读侧：encoding 与 errors 都写死，BOM 和 \\r\\n 先用 read_bytes 钉住 ===")
print(f"  基线（第 6 节）：locale.getpreferredencoding(False)={locale.getpreferredencoding(False)!r}"
      f" ｜ sys.stdout.encoding={sys.stdout.encoding!r} —— 工具一个都不靠它们")
for fx in sorted(IN_DIR.iterdir()):
    if fx.suffix != ".json":
        continue
    got = read_config_text(fx)
    print(f"  {fx.name:<14} 字节={got['bytes']:<5} 字符={got['chars']:<5}"
          f" BOM={str(got['had_bom']):<6} 磁盘 \\r\\n={got['crlf_on_disk']:<4}"
          f" 读回 '\\r'={got['cr_in_text']:<4} U+FFFD={got['replaced']:<4}"
          f" 句柄 encoding={got['enc']!r} errors={got['errs']!r}")
print("  → 判据一：同一份文件字节数 ≠ 字符数（一个中文 3 字节）；dirty 那一行的 U+FFFD 就是**被改掉的")
print("    位置**，工具把它当质量信号报出来，而不是当成「读取成功」。")
print("  → 判据二：bom 那一行磁盘上有 \\r\\n、读回来 \\r 剩 0 个 —— 是文本层翻译的（第 5 节），不是文件坏了。")
gbk = IN_DIR / "gbk编码.json"
with open(gbk) as fh:                                   # ★故意不写 encoding
    print(f"  open(p) 不写 encoding → 句柄 encoding={fh.encoding!r}（来自 locale，不来自文件内容）")
    try:
        fh.read()
        print("    ★竟然读通了 → 本机 locale 恰好和文件编码对得上，别当普遍结论")
    except UnicodeDecodeError as exc:
        print(f"    → {type(exc).__name__}: {exc}")
print(f"  同一份字节显式 encoding='gbk' → {gbk.read_bytes().decode('gbk')!r} ← 读通了，中文一个都没坏")

print("\n=== 3. 校验：契约就是 json 的那张类型表，报错必须带 lineno/colno/pos ===")
for name in ("broken.json", "没这份.json"):
    code, info = main(["--in", str(IN_DIR / name), "--out", str(OUT_DIR)])
    print(f"  {name:<14} 判失败 → {code == RET_BAD} ｜ 工具的原始判词：")
    print(f"    {info['why']}")
print("  → 判据：JSONDecodeError 是 ValueError 的儿子，msg/lineno/colno/pos 直接指出「哪一格」；只打一句")
print("    「配置读失败」的工具，出了事还得人再开一遍文件。语法坏 = 不产出，半个字都不写。")
code, info = main(["--in", str(IN_DIR / "wrongtypes.json"), "--out", str(OUT_DIR), "--check-only"])
print(f"  契约校验（wrongtypes.json，--check-only 不落盘）判通过 → {code == RET_OK}：")
print(f"    {'键':<7}{'来的是':<15}{'契约要':<8}{'落的是':<10}处置")
for row in info["rows"]:
    print(f"    {row[0]:<7}{row[1]:<15}{row[2]:<8}{row[3]:<10}{row[4]}")
print("  → 三条最容易放过的：tuple 会**降级成 list 且不报错**、字典的 int 键回来变 str、")
print("    bool 是 int 的儿子所以 isinstance(True, int) 为真 —— 契约表必须先单独挡 bool 再谈 int。")
for label, val in (("tuple", (1, 2)), ("set", {1, 2})):
    try:
        shown = json.dumps(val)
    except TypeError as exc:
        shown = f"{type(exc).__name__}: {exc}"
    print(f"    loads 永远给不出 {label}，但内存对象会走到这一步：dumps({val!r}) → {shown}")
try:
    shown = f"★居然读通了：{json.loads('None')!r}（本机不该走到这里）"
except json.JSONDecodeError as exc:
    shown = f"{type(exc).__name__}: {exc}"
print(f"  Python 的字面量 json 不认：loads('null') → {json.loads('null')!r} ｜ 配置里手打 None → {shown}")

print("\n=== 4. ★产出：同目录 tmp + fsync + os.replace，而「有没有留 .tmp」这笔账当场响一次 ===")
jname = "app.normalized.json"
(OUT_DIR / jname).write_text("o" * 30, encoding="utf-8")   # 旧版本先占位，才比得出「读者看到什么」
OLD_LEN, NEW_LEN = 30, 74
seen = []


def probe_now(target, tmp):
    """replace 之前的一刻：读者能看到的长度只允许是旧版或新版之一。"""
    seen.append((len(target.read_text(encoding="utf-8")), tmp.exists()))


probe_now(OUT_DIR / jname, OUT_DIR / (jname + TMP))
atomic_write(OUT_DIR / jname, "n" * NEW_LEN, before_replace=probe_now)
seen.append((len((OUT_DIR / jname).read_text(encoding="utf-8")), False))
print(f"  旧版长度={OLD_LEN} 新版长度={NEW_LEN} ｜ 三个探测点的（读者看到的长度, 此刻 .tmp 在不在）= {seen}")
print(f"  → 长度只落在 ({OLD_LEN}, {NEW_LEN}) 里吗 → {all(x[0] in (OLD_LEN, NEW_LEN) for x in seen)}"
      f" ｜ 出现第三种长度（= 有人在目标上就地写）的次数 = "
      f"{sum(x[0] not in (OLD_LEN, NEW_LEN) for x in seen)}")
saved = os.umask(0o077)                                   # 模拟 cron / systemd 里常见的严格 umask
no_fix = atomic_write(OUT_DIR / "umask1.json", "x" * 8, fix_mode=False)
with_fix = atomic_write(OUT_DIR / "umask2.json", "x" * 8, fix_mode=True)
os.umask(saved)
print(f"  umask 0o077 下（第 1、13 节）：tmp 建出来 {oct(no_fix['mode_tmp'])} → 不 chmod 就 replace → 产出 {oct(no_fix['mode_out'])}"
      f" ｜ replace 前补一句 chmod(0o644) → 产出 {oct(with_fix['mode_out'])}")
print("  → rename(2) 只改目录项、不改 inode：临时文件什么权限，转正之后就是什么权限。漏掉那次 chmod，")
print("    别人（组/其他）就读不到你刚「发布」的配置 —— 而工具自己一路判成功，什么都不报。")
(OUT_DIR / "umask1.json").unlink()
(OUT_DIR / "umask2.json").unlink()
ghost = OUT_DIR / (jname + TMP)                           # 上次崩在「tmp 写完、还没 replace」之间
ghost.write_text('{"name": "半截配置", "po', encoding="utf-8")
ghost2 = OUT_DIR / (".audit" + TMP)                        # 同一种残留，但名字以点开头
ghost2.write_text("半截之二", encoding="utf-8")
fired = sweep_stale_tmp(OUT_DIR)
by_glob = count_tmp_glob(OUT_DIR)
by_pathglob = sorted(p.name for p in OUT_DIR.glob("*" + TMP))
print(f"  对账第一次（现场真有残留）：工具用的 iterdir 版抓到 {len(fired)} 个 = {fired}")
print(f"    残留原文的尾部 = {ghost.read_text(encoding='utf-8')[-10:]!r} ← 这就是「崩在半路」的形状")
print(f"  同一个现场换两种数法：glob 模块 → {len(by_glob)} 个 = {by_glob}"
      f" ｜ Path.glob → {len(by_pathglob)} 个 = {by_pathglob}")
print(f"    ★glob 模块漏掉的那一个 = {sorted(set(fired) - set(by_glob))}"
      f"（第 11 节：glob 的 `*` 不匹配点开头的名字，Path.glob 却匹配 —— 同一个 `*` 两套规矩，本机实测）")
print("    → 对账要是写成 glob 模块那一版，它对 .audit.tmp 这种残留就永远少报一个。漏报比不查更糟：")
print("      屏幕上那个数字看着像有人盯着，其实被盯住的那一半根本不存在。工具用 iterdir 版，两个都抓到。")
code, info = main(["--in", str(IN_DIR / "app.json"), "--out", str(OUT_DIR)])
after = sweep_stale_tmp(OUT_DIR)
after_glob = count_tmp_glob(OUT_DIR)
print(f"  工具自己扫过一遍（开工先清残留）→ 这次抓到 {len(info['stale'])} 个 ｜ 现在 iterdir 版 = {after}"
      f" ｜ glob 版 = {after_glob} ｜ 两个都归 0 → {after == [] and after_glob == []}")
print(f"  产出：{info['jinfo']['tmp']} 转正为 {info['jp'].name}（{info['jp'].stat().st_size} 字节，"
      f"权限 {oct(info['jinfo']['mode_out'])}）｜ 判成功 → {code == RET_OK}")
print("  → 这一段的判据：目标文件从头到尾只被 rename(2) 换过一次名；fsync 买到的是「请求内核别丢」，")
print("    不是「已经在介质上」（本机没有可控断电手段，掉电行为**未验证**）。")

print("\n=== 5. 边界：配置整份读、日志逐行读；文本模式 seek 只认 tell() 给的 cookie ===")
code, dirty = main(["--in", str(IN_DIR / "dirty.json"), "--out", str(OUT_DIR)])
code2, bom = main(["--in", str(IN_DIR / "bom.json"), "--out", str(OUT_DIR)])
print(f"  另两份配置走同一条流水线 → 都判成功 {code == RET_OK and code2 == RET_OK}"
      f" ｜ dirty 的产出里 U+FFFD 个数 = {dirty['json_text'].count(chr(0xfffd))}"
      f" ｜ BOM 那份的产出首字符是 BOM 吗 → {bom['json_text'].startswith(chr(0xfeff))}")
print("  → 坏字节没被藏起来：它变成产出里的 U+FFFD，谁读产出都看得见这里坏过（第 7 节的判据）。")
log = OUT_DIR / "sweep.log"
lines = [f"{n:05d},这是一行中文备注,7\n" for n in range(1, 301)]
log.write_text("".join(lines), encoding="utf-8")
raw = log.read_bytes()
whole = log.read_text(encoding="utf-8")
print(f"  造一份 {len(lines)} 行的产出日志：磁盘 {len(raw)} 字节 ｜ 整份 read_text 得到 {len(whole)} 个**字符**")
n_chunk, total = 0, 0
with open(log, "rb") as fh:
    while True:
        block = fh.read(4096)
        if not block:
            break
        n_chunk += 1
        total += len(block)
n_line = 0
with open(log, encoding="utf-8") as fh:
    for _ in fh:
        n_line += 1
    second = sum(1 for _ in fh)                          # 第 4 节：文件对象是一次性迭代器
print(f"  二进制 read(4096) 分块 → {n_chunk} 块、合计 {total} 字节 ｜ 和整份一致 → {total == len(raw)}")
print(f"  逐行迭代 → {n_line} 行 ｜ 同一个句柄第二次迭代 → {second} 行（不报错，就是空）")
print("  → 本工具对配置用 read_text（小、整份），对日志一律逐行或分块：峰值内存那笔账在第 3、18 节量过，")
print("    这里只点一句 —— 配置永远不该长成 GB 级，长到那个尺寸它就不是配置文件了。")
LINE0_BYTES, LINE0_CHARS = len(lines[0].encode("utf-8")), len(lines[0])
MID = len(b"00001,") + 1
with open(log, encoding="utf-8") as fh:
    fh.readline()
    cookie = fh.tell()
    rest = fh.readline()
    fh.seek(cookie)
    again = fh.readline()
    print(f"  读完第一行后 tell() 给的 cookie = {cookie} ｜ 第二行的字节偏移 = {LINE0_BYTES}"
          f" ｜ 字符偏移 = {LINE0_CHARS}")
    print(f"    seek(cookie) 之后重读，拿回同一行吗 → {again == rest} ｜ 那一行 {rest.rstrip()!r}")
    print(f"    本机这份文件里 cookie 恰好等于字节偏移 → {cookie == LINE0_BYTES}，"
          f"等于字符偏移 → {cookie == LINE0_CHARS}")
    print("    → 别把这条相等当契约：文档只承诺「seek(tell()) 有效」，cookie 的内部形状是实现细节；")
    print(f"      随手塞一个自造的偏移 {MID}（落在一个中文字中间）再读，代价当场付：")
    fh.seek(MID)
    try:
        bad = fh.read(6)
        print(f"        没响，读出 {bad!r} ← 切在半个字符上")
    except (UnicodeDecodeError, OSError) as exc:
        print(f"        {type(exc).__name__}: {exc}")
listing = sorted(OUT_DIR.iterdir())
print(f"  清点 {OUT_DIR.name}/（第 11 节：iterdir/glob 都不保证顺序，逐字比对的前提是先 sorted）：")
for p in listing:
    st = p.stat()
    print(f"    {p.name:<26} st_size={st.st_size:<7} 实际占盘={st.st_blocks * 512:<7}"
          f" 两者相等 → {st.st_size == st.st_blocks * 512}")
print(f"  resolve() 只在内部问内核、结果不上屏（第 14 节）：上面每一项都在 {OUT_DIR.name}/ 下面吗 → "
      f"{all(str(p.resolve()).startswith(str(OUT_DIR.resolve())) for p in listing)}"
      f" ｜ normpath 纯算出来的名字仍等于磁盘上那个吗 → "
      f"{os.path.normpath(str(OUT_DIR / '.' / jname)) == str(OUT_DIR / jname)}")

print("\n=== 6. 收口：这张对照表 —— 工具的每一处写法回指本章第几节 ===")
TABLE = [
    ('open(p, encoding="utf-8-sig", errors="replace")', (6, 7),
     "不写 encoding 用的是 locale 的首选编码；BOM 会变成首字符 \\ufeff；errors 里只有 strict 会响，其余四个都在改内容"),
    ("path.read_bytes() 先把字节钉住", (12, 18),
     "字节 ≠ 字符（一个中文 3 字节）；read_text/write_text 只配「整份 + 小文件」，而 write_text 是截断写"),
    ('读回的 "\\r" 个数、写 tmp 时那句 newline="\\n"', (5,),
     "读侧默认把 \\r\\n 翻成 \\n，写侧把 \\n 换成 os.linesep：磁盘形状和字符串形状是两回事"),
    ("p.parts / name / suffix / stem / parent", (8, 9),
     "纯字符串运算，对不存在的名字照样算得出、一个异常不抛；os.path 那一派是同一条界线，老代码里全是它"),
    ("mkdir(parents=True, exist_ok=True) / unlink(missing_ok=True)", (10,),
     "幂等三件套；ENOENT / EEXIST / ENOTDIR 是三条不同的处置路径，except OSError: pass 把它们压成一次静默"),
    ('csv.writer(buf) + open(..., newline="")', (5, 15),
     "csv 自己已经写了 \\r\\n，再让文本层翻译一次就成了 \\r\\r\\n；它不管编码也不管类型，读出来全是 str"),
    ("json.loads + JSONDecodeError 三字段；dumps(sort_keys=True, ensure_ascii=False)", (16,),
     "json 是**值**的格式不是类型的格式（tuple 降级、set 当场 TypeError）；msg/lineno/colno/pos 一行定位到「哪一格」；默认 ensure_ascii 让中文体积翻倍"),
    ('open(out, "w") 只用在 tmp 上，绝不用在目标上', (2, 12),
     "'w' 在打开那一刻就 O_TRUNC：事故不是「新的没写成」，是**旧的也没了** —— 没备份、不报错"),
    ("同目录 .tmp → f.flush() → os.fsync(fd) → os.replace", (3, 17),
     "flush 才是缓冲区边界；「写完」「落盘」「原子」是三件事；fsync 只到「交给内核」为止（掉电本机未验证）"),
    ("os.chmod(tmp, 0o644) 与新建文件的权限", (1, 13),
     "新建文件位 = 0o666 & ~umask；mkstemp 故意给 0600；权限跟 inode 走不跟名字走，所以 chmod 必须赶在 replace 之前"),
    ("sweep_stale_tmp() 用 iterdir，而不是 glob 模块的 `*`", (11,),
     "glob 模块的 `*` 不匹配点开头的名字（Path.glob 却匹配）→ 那一版永远少报；计数守卫要先被故意弄响一次再报 0"),
    ("with open(...) / for line in fh / p.resolve()", (4, 14),
     "with 只保证 close 不保证事务，文件对象还是一次性迭代器；resolve 问内核并展开符号链接，absolute 只拼 cwd、不折叠 .."),
]
for code_text, nums, why in TABLE:
    print(f"  {code_text:<50} → 第 {'、'.join(str(n) for n in nums):<7} 节：{why}")
covered = sorted({n for _, nums, _ in TABLE for n in nums})
missing = sorted(set(range(1, 19)) - set(covered))
print(f"  对照表 {len(TABLE)} 行 ｜ 1..18 全对上号了吗 → {covered == list(range(1, 19))}"
      f" ｜ 没对上的节 = {missing or '无'}")
print("  → 三条带走：① 「读成功」不等于「读对了」、「写完了」不等于「别人看得懂」——中间每一步都有自己的")
print("    默认值，所以全部写死；② 产出只有一个安全形状：同目录 tmp 写全 → fsync → chmod → replace，")
print("    任何一步崩了就 unlink(missing_ok=True)，绝不让半截东西顶在目标位置上；③ 这个工具最值钱的一行")
print("    不是写文件那段代码，是**先抓到一个残留 .tmp、再报那个 0**：没有那一次响，后面的 0 一文不值。")
print(f"  本机 Python {sys.version.split()[0]} ｜ 全程只写 {WORK.name}/ 下的相对路径 ｜ umask 钉在 0o022"
      " ｜ 无绝对路径、无时间戳、无随机名，跑两遍逐字一致。")

if __name__ == "__main__":
    if len(sys.argv) > 1:                              # 带参数 = 当真工具用
        _code, _info = main(sys.argv[1:])
        print(_info["why"])
        sys.exit(_code)
    # 无参数 = 上面那六段自检（demo.sh 第 19 节跑的就是这一条路）
