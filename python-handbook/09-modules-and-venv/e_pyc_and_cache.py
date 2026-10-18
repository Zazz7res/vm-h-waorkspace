#!/usr/bin/env python3
# ============================================================================
# 取证脚本 e_pyc_and_cache.py —— 第 09 章《模块、导入与环境》第 5 讲
#
# 只证四件事：
#   ① `__pycache__/xxx.cpython-314.pyc` 这个形状由谁决定，16 字节头部里存的是哪两个数；
#   ② 失效判据是 **(源码 mtime, 源码字节数)**，不是内容哈希 —— 所以「改了内容、mtime 和
#      字节数都没变」会命中陈旧缓存，跑的还是旧代码（本机现场复现，这是最值钱的一条）；
#   ③ `-B` / `PYTHONDONTWRITEBYTECODE` / `sys.dont_write_bytecode` 只管**写**不管**读**；
#      `sys.pycache_prefix` 把缓存整体搬到别处，搬的是绝对路径的镜像；
#   ④ 删掉 .py 只留 `__pycache__/*.pyc` 是**加载不了**的（老式同级 `xxx.pyc` 才行）。
#
# 沙箱：当前目录下的 pycache-demo/，末尾 shutil.rmtree 删干净并当场自证 —— 本脚本自己
# 产生的每一个 __pycache__、每一个 prefix 镜像目录都在这笔账里。只用标准库，不联网、
# 不装包、不 import 兄弟脚本。全程只打计数和布尔，一个毫秒都不打（那不是本节要证的）。
# 本机：CPython 3.14.4 / Ubuntu 26.04。tag、magic、报错措辞都跟版本绑定，
# 其它版本 / 其它平台 —— 未验证，别当事实引用。
# ============================================================================
import dis
import marshal
import os
import shutil
import struct
import subprocess
import sys
import importlib.util
from pathlib import Path

ROOT = Path.cwd().resolve()
BOX = Path("pycache-demo")
PYC_BEFORE = Path("__pycache__").exists()
TAG = sys.implementation.cache_tag                 # 本机：cpython-314
LINES = []
RUNS = 0
HEADER = struct.Struct("<4sIII")                   # 官方头部：magic, flags, 字段3, 字段4


def mask(text):
    out = text
    for base in (str(ROOT), os.getcwd(), str(Path.home())):
        if base and base != "/":
            out = out.replace(base + "/", "").replace(base, ".")
    return out


def emit(text):
    for ln in mask(str(text)).splitlines():
        LINES.append(ln)
        print(ln)


def clean_env(extra=None):
    """先剥掉继承的 PYTHON*（PYTHONPATH/PYTHONHOME/PYTHONDONTWRITEBYTECODE/PYTHONPYCACHEPREFIX 全在内），再按需加。"""
    env = {k: v for k, v in os.environ.items() if not k.startswith("PYTHON")}
    env["PYTHONIOENCODING"] = "utf-8"
    env.update(extra or {})
    return env


def run(args, sub=".", env=None):
    """起一个真子进程：cwd 在沙箱里，env 干净，默认**不加**任何开关（让缓存正常写）。"""
    global RUNS
    RUNS += 1
    cwd = BOX if sub == "." else BOX / sub
    proc = subprocess.run([sys.executable, *args], cwd=str(ROOT / cwd), env=clean_env(env),
                          capture_output=True, text=True, encoding="utf-8")
    return proc.returncode, mask(proc.stdout), mask(proc.stderr)


def flatten(text, joiner=" ; "):
    return joiner.join(ln.strip() for ln in text.splitlines() if ln.strip()) or "(空)"


def errline(text):
    return flatten(text).split(" ; ")[-1] if text.strip() else "(没有 stderr)"


def cache_name(mod_rel):
    """模块源码的相对路径 → 它的 .pyc 文件名（只在本机这一版解释器下成立）。"""
    folder = (BOX / mod_rel).parent
    stem = Path(mod_rel).stem
    hits = sorted(p.name for p in (folder / "__pycache__").glob(f"{stem}.*.pyc"))
    return hits[0] if hits else None


def pyc_of(mod_rel):
    return (BOX / mod_rel).parent / "__pycache__" / cache_name(mod_rel)


def head(mod_rel):
    return HEADER.unpack(pyc_of(mod_rel).read_bytes()[:16])


def stamp(mod_rel):
    st = (BOX / mod_rel).stat()
    return int(st.st_mtime), st.st_size


def source(rel, text):
    (BOX / rel).write_text(text, encoding="utf-8")


def touch(rel, delta_s):
    st = (BOX / rel).stat()
    os.utime(BOX / rel, ns=(st.st_atime_ns, st.st_mtime_ns + delta_s * 1_000_000_000))


def keep_mtime(rel, ns):
    """把 mtime 精确拨回 ns —— 本节的核心手法：只改内容，不让那两个缓存键变。"""
    st = (BOX / rel).stat()
    os.utime(BOX / rel, ns=(st.st_atime_ns, ns))


def wipe(rel):
    """清掉某个模块已有的缓存，回到「第一次 import」的状态（只动它自己那份）。"""
    folder = (BOX / rel).parent
    cache = folder / "__pycache__"
    if cache.exists():
        for p in sorted(cache.glob(f"{Path(rel).stem}.*.pyc")):
            p.unlink()
        if not any(cache.iterdir()):
            shutil.rmtree(cache, ignore_errors=True)


def listing():
    return sorted(str(p.relative_to(BOX)).replace(os.sep, "/") for p in BOX.rglob("*") if p.is_file())


print("=== 1. 文件名形状：xxx.cpython-314.pyc 是谁决定的，__pycache__ 长在哪 ===")
shutil.rmtree(BOX, ignore_errors=True)
(BOX / "pkg").mkdir(parents=True)
source("pkg/__init__.py", '"""包：验证缓存跟着源码所在目录走。"""\n')
source("pkg/mod.py", 'TAG = "V1"\n')
source("solo.py", 'print("BODY-solo")\nTAG = "V1"\n')
source("main_script.py", 'import solo\nprint("main 自己 import 了 solo")\n')
rc, out, err = run(["-c", "import pkg.mod, solo"])
emit(f"  子进程 `python3 -c 'import pkg.mod, solo'`（cwd={BOX.name}/）：rc={rc} ｜ 模块体打印 → {flatten(out)}")
found = [f for f in listing() if f.endswith(".pyc")]
emit(f"  沙箱里现在的 .pyc（sorted，相对路径）：共 {len(found)} 个")
for f in found:
    emit(f"    {f}")
emit(f"  tag 的来源：sys.implementation.cache_tag = {TAG}"
     f" ｜ importlib.util.MAGIC_NUMBER = {importlib.util.MAGIC_NUMBER.hex()}"
     f" ｜ 主.次版本 = {sys.version_info.major}.{sys.version_info.minor}")
rc, out, err = run(["main_script.py"])
emit(f"  把 main_script.py 当**脚本**直接跑：rc={rc} ｜ 输出 → {flatten(out)}")
emit(f"  它自己有没有留下 main_script.*.pyc → {any('main_script' in f for f in listing() if f.endswith('.pyc'))}"
     f"（被 import 的 solo 却仍然有缓存：{any('solo' in f for f in listing() if f.endswith('.pyc'))}）")
emit("  → 只有**被 import 的模块**写缓存；`__main__`（你直接敲进去的那个文件）永远不写 .pyc。")
emit("  → 文件名 = 模块名 + tag + .pyc；tag = 实现名-主.次版本（本机 cpython-314）。")
emit("    debug 构建 / free-threaded 构建 / Windows 上的 tag 形状，本机之外未验证，别当事实引用。")
emit(f"  ★ 缓存**不集中**：pkg/ 里那份和沙箱根上那份在不同目录 —— 删缓存要按目录删，find 一遍再删。")

print()
print("=== 2. 头部 16 字节：缓存键不是内容哈希，而是「源码 mtime + 源码字节数」 ===")
magic, flags, f3, f4 = head("pkg/mod.py")
m_src, s_src = stamp("pkg/mod.py")
emit(f"  magic 和当前解释器期望的一致 → {magic == importlib.util.MAGIC_NUMBER}")
emit(f"  flags = {flags}（0 = 时间戳失效法；1 = unchecked-hash；3 = checked-hash —— ④ 现场把它切成 3）")
emit(f"  第 9~12 字节 == 源码 mtime（整秒）→ {f3 == m_src}")
emit(f"  第 13~16 字节 == 源码字节数 → {f4 == s_src}")
emit(f"  这份头部里有没有「源码哈希」字段 → {flags != 0}（flags=0 时那 8 个字节被 mtime 和 size 占了）")
emit("  → 判据照抄：缓存键是 (mtime, size)。改了内容却没动这两个数，缓存就还「有效」。")

print()
print("=== 3. 失效判据实测：四种改法四种结局，每一格都是真子进程跑出来的行为 ===")
emit("  源码一直是 `TAG = \"V1\"`；改内容就是把它改成 V2（两行长度完全一样，字节数不变）。")
before = pyc_of("pkg/mod.py").read_bytes()
touch("pkg/mod.py", 3)
m1, _ = stamp("pkg/mod.py")
emit(f"  ① 只把 mtime 拨后 3 秒（内容/字节数一个没动）：现在 头部 mtime == 源码 mtime → {head('pkg/mod.py')[2] == m1}")
rc, out, err = run(["-c", "import pkg.mod; print('TAG =', pkg.mod.TAG)"])
after = pyc_of("pkg/mod.py").read_bytes()
emit(f"     再 import：rc={rc} ｜ 输出 {flatten(out)} ｜ .pyc 被重写过 → {after != before}"
     f"（头部 mtime 重新对上源码：{head('pkg/mod.py')[2] == stamp('pkg/mod.py')[0]}）")
emit("     → mtime 一变，缓存当场作废、重编译。这是大家都熟悉的方向。")
emit("  ★ 方向别记反（这一页两种改法各测过一次）：动 mtime → 缓存**失效**；只动内容而 mtime 和字节数不动 →")
emit("    缓存**照旧命中**。所以「只改内容会不会被发现」的答案是「不一定」，而不是「一定会」。")
keep = (BOX / "pkg/mod.py").stat().st_mtime_ns
size_b = (BOX / "pkg/mod.py").stat().st_size
before2 = pyc_of("pkg/mod.py").read_bytes()
source("pkg/mod.py", 'TAG = "V2"\n')
keep_mtime("pkg/mod.py", keep)
st2 = (BOX / "pkg/mod.py").stat()
emit(f"  ② 改内容 V1→V2：**字节数不变** → {st2.st_size == size_b}，mtime 拨回原值 → {st2.st_mtime_ns == keep}")
rc, out, err = run(["-c", "import pkg.mod; print('TAG =', pkg.mod.TAG)"])
after2 = pyc_of("pkg/mod.py").read_bytes()
emit(f"     下一次 import：rc={rc} ｜ 输出 → {flatten(out)}   ← 读到的是 V1，源码明明写着 V2")
emit(f"     .pyc 一个字节都没被重写 → {before2 == after2}：它压根没去读源码，只比了头部那两个数")
emit("  ★ 真实的坑就在这儿：`cp -p`、`rsync -a`、`git checkout` 之后只要 mtime 和字节数没变，")
emit("    Python 继续跑**旧字节码**。「我改了代码怎么没生效」有一半案例是这一条，不是编译器抽风。")
touch("pkg/mod.py", 5)
rc, out, err = run(["-c", "import pkg.mod; print('TAG =', pkg.mod.TAG)"])
emit(f"  ③ 同样的内容，把 mtime 拨后 5 秒：rc={rc} ｜ 输出 → {flatten(out)}   ← 这次才读到 V2")
source("pkg/mod.py", 'TAG = "V1"\n')
rc, out, err = run(["-m", "compileall", "-q", "-f", "--invalidation-mode", "checked-hash", "pkg"])
emit(f"  ④ `python3 -m compileall -q -f --invalidation-mode checked-hash pkg`：rc={rc}"
     f" ｜ 现在头部 flags = {head('pkg/mod.py')[1]}（非 0 = 哈希失效法；`-f` 是强制重写，别让它跳过）")
keep4 = (BOX / "pkg/mod.py").stat().st_mtime_ns
size4 = (BOX / "pkg/mod.py").stat().st_size
source("pkg/mod.py", 'TAG = "V2"\n')
keep_mtime("pkg/mod.py", keep4)
st4 = (BOX / "pkg/mod.py").stat()
rc, out, err = run(["-c", "import pkg.mod; print('TAG =', pkg.mod.TAG)"])
emit(f"     再演一遍「只改内容 + mtime 拨回 + 字节数不变」（size 同 {st4.st_size == size4} / mtime 同 {st4.st_mtime_ns == keep4}）")
emit(f"     下一次 import：rc={rc} ｜ 输出 → {flatten(out)}   ← 抓到了：哈希模式**会**现读源码算哈希")
emit("  → 默认模式（flags=0）省掉的是「读源码」这一步，代价就是②那个坑；checked-hash 用一次读源码换确定性。")

print()
print("=== 4. 四种「别写缓存」各演一遍：屏幕上只有布尔，没有路径 ===")
MODES = [
    ("默认（什么都不加）", ["-c", "import solo"], None),
    ("-B（命令行开关）", ["-B", "-c", "import solo"], None),
    ("PYTHONDONTWRITEBYTECODE=1", ["-c", "import solo"], {"PYTHONDONTWRITEBYTECODE": "1"}),
    ("sys.dont_write_bytecode = True", ["-c", "import sys; sys.dont_write_bytecode = True; import solo"], None),
]
for label, args, env in MODES:
    wipe("solo.py")
    rc, out, err = run(args, env=env)
    emit(f"  {label}｜ rc={rc} ｜ 源码旁边出现 __pycache__ → {(BOX / '__pycache__').exists()}")
wipe("solo.py")
PFX = "pcp"
rc, out, err = run(["-X", f"pycache_prefix={PFX}", "-c", "import solo"])
mirror = sorted(str(p.relative_to(BOX / PFX)).replace(os.sep, "/") for p in (BOX / PFX).rglob("*.pyc"))
want = str(ROOT / BOX / f"solo.{TAG}.pyc").lstrip("/")
mine = [m for m in mirror if m == want]
others = [m for m in mirror if m != want]
sys_side = sum(1 for m in others if m.startswith("usr/"))
emit(f"  -X pycache_prefix={PFX}（sys.pycache_prefix 的命令行版）｜ rc={rc}")
emit(f"    源码旁边有 __pycache__ → {(BOX / '__pycache__').exists()} ｜ prefix 目录里有 .pyc → {bool(mine)}")
emit(f"    prefix 里 solo 的缓存只此一份 → {len(mine) == 1} ｜ 它的路径 == prefix + 源码绝对路径去掉开头的 / → "
     f"{bool(mine) and mirror and mine[0] == want}")
emit(f"    用了 prefix 就**没有 __pycache__ 这一层**（.pyc 直接躺在镜像目录里）→ "
     f"{all('__pycache__' not in m for m in mirror)}")
emit(f"    ★ 它把标准库的缓存也一起搬走了：prefix 里除 solo 之外还有 {len(others)} 份 .pyc，"
     f"其中落在系统目录（usr/… 开头）下的 {sys_side} 份 —— 所以 prefix 别设到别人的目录里")
shutil.rmtree(BOX / PFX, ignore_errors=True)
wipe("solo.py")
PFX2 = str(ROOT / BOX / "pcp2")
rc, out, err = run(["-c", "import solo"], env={"PYTHONPYCACHEPREFIX": PFX2})
emit(f"  PYTHONPYCACHEPREFIX（同一个开关的环境变量版）｜ rc={rc} ｜ 源码旁边有 __pycache__ → "
     f"{(BOX / '__pycache__').exists()} ｜ prefix 目录里有 .pyc → {(BOX / 'pcp2').exists()}")
shutil.rmtree(BOX / "pcp2", ignore_errors=True)
wipe("solo.py")
run(["-c", "import solo"])                                   # 先正常缓存一份
keepw = (BOX / "solo.py").stat().st_mtime_ns
size_w = (BOX / "solo.py").stat().st_size
source("solo.py", 'print("BODY-solo")\nTAG = "V2"\n')
keep_mtime("solo.py", keepw)
st_w = (BOX / "solo.py").stat()
emit(f"  ★ 补一刀「只写不读」这半边：已有缓存 + 改内容（size 同 {st_w.st_size == size_w}、mtime 拨回 {st_w.st_mtime_ns == keepw}）")
rc, out, err = run(["-B", "-c", "import solo; print('TAG =', solo.TAG)"])
emit(f"     带 -B 再 import → rc={rc} ｜ 输出 {flatten(out)}   ← 还是 V1：-B 拦得住写，拦不住读")
rc, out, err = run(["-c", "import solo"])                    # 不带 -B，看它会不会把缓存修好
emit(f"     不带 -B 再 import 一次 → rc={rc} ｜ 输出 {flatten(out)} ｜ .pyc 仍与源码「对得上」→ "
     f"{head('solo.py')[2] == stamp('solo.py')[0]}")
emit("  → 三个开关只管**写**。想让陈旧缓存彻底闭嘴只有两条路：删 .pyc，或者让 mtime/字节数真的变一变。")

print()
print("=== 5. 删掉源码只留 .pyc：那个「大家都以为能跑」的传闻，实测 ===")
wipe("solo.py")
source("solo.py", 'print("BODY-solo")\nTAG = "V1"\n')
rc, out, err = run(["-c", "import solo"])
emit(f"  第一步：正常 import 一次，让它把缓存写出来 → rc={rc} ｜ 有 __pycache__ → {(BOX / '__pycache__').exists()}")
cached = cache_name("solo.py")
os.remove(BOX / "solo.py")
emit(f"  第二步：**只删 .py**，__pycache__ 原样留着（现在带 solo 的文件：{[f for f in listing() if 'solo' in f]}）")
rc, out, err = run(["-c", "import solo"])
emit(f"  第三步：再 import → rc={rc} ｜ 原文：{errline(err)}")
emit("  → 传闻证伪：Python 3 不会因为 __pycache__ 里有个 .pyc 就认它 —— 定位模块找的是 `solo.py`。")
shutil.copy(BOX / "__pycache__" / cached, BOX / "solo.pyc")
emit(f"  第四步：把那个 .pyc **改名成同级的 solo.pyc**（老式布局，__pycache__ 还留着）→ "
     f"{[f for f in listing() if 'solo' in f]}")
rc, out, err = run(["-c", "import solo; print('loader =', solo.__spec__.loader.__class__.__name__, '| TAG =', solo.TAG)"])
emit(f"  第五步：再 import → rc={rc} ｜ 输出 → {flatten(out)}")
emit("  → 这才能跑：认的是「模块名.pyc」这个同级老式名字（SourcelessFileLoader），不是 __pycache__ 里带 tag 的那个。")
bad = bytearray((BOX / "solo.pyc").read_bytes())
bad[0] ^= 0xFF
(BOX / "solo.pyc").write_bytes(bytes(bad))
rc, out, err = run(["-c", "import solo"])
emit(f"  ★ 只把 .pyc 头部的 magic 改坏 1 个字节 → rc={rc} ｜ 原文：{errline(err)}")
emit(f"     （本机期望的 magic 是 {importlib.util.MAGIC_NUMBER.hex()}，头部第一个字节被我不小心改成了 "
     f"{(BOX / 'solo.pyc').read_bytes()[0]:#04x} —— 这就是「拷 .pyc 去别的机器」必响的原因）")
emit("     其它 Python 版本上的原文未验证，别当事实引用；能确定的是：magic 绑解释器小版本。")

print()
print("=== 6. 编译这件事看得见：.pyc 的正文是字节码 + 常量，不是机器码，更不是加密 ===")
code = compile("A = 1", "<现场>", "exec")
ops = [ins.opname for ins in dis.get_instructions("A = 1")]
emit(f"  compile('A = 1', ...) → {type(code).__name__} 对象；co_code 是 bytes → {isinstance(code.co_code, bytes)}")
emit(f"  dis 拆出来的前三条指令（本机 3.14 实测；别的小版本指令名会变）：{ops[:3]}")
wipe("pkg/mod.py")
source("pkg/mod.py", 'TAG = "V1"\n')
run(["-c", "import pkg.mod"])
raw = pyc_of("pkg/mod.py").read_bytes()
body = marshal.loads(raw[16:])
emit(f"  marshal.loads(pyc[16:]) → {type(body).__name__}：co_name = {body.co_name}，"
     f"co_filename 只剩 basename = {Path(body.co_filename).name}，co_consts = {body.co_consts}")
emit("  → 一句话：.pyc 的正文就是一个 marshal 序列化的 code object —— **字节码**，CPU 一行都不认，")
emit("    仍然由解释器逐条解释执行。缓存跳过的只有 parse/compile，不是执行（第 7 节用计数证明）。")
source("canary.py", 'GUESS = "canary-金丝雀"\n')
wipe("canary.py")
run(["-c", "import canary"])
rawc = pyc_of("canary.py").read_bytes()
emit(f"  源码里的字符串字面量在不在 .pyc 里 → ASCII「canary」：{b'canary' in rawc}"
     f" ｜ 中文「金丝雀」（UTF-8）：{'金丝雀'.encode() in rawc}")
emit(f"  源码文本本身（'GUESS = ' 这一段）在不在 .pyc 里 → {b'GUESS = ' in rawc}")
emit("  ★ 「只发 .pyc 就能保护源码」是自欺：常量、名字、行号全在文件里，反编译几乎是原样。")

print()
print("=== 7. 收口：缓存买到的是「省掉 parse/compile」，不是「省掉执行」 ===")
source("cnt.py", 'print("BODY-cnt")\n')
wipe("cnt.py")
rc, out, err = run(["-c", "import cnt; import cnt; import cnt; from cnt import __name__ as _n"])
emit(f"  同一个进程里 import cnt 四次 → 模块体打印次数 = {out.count('BODY-cnt')}"
     f" ｜ 这是第 1 讲那条规律的镜像：一个进程内模块体只跑一遍（sys.modules 说了算）")
wipe("cnt.py")
run(["-c", "import cnt"])                                    # 先把缓存养出来
emit(f"  缓存已经在了 → {(BOX / '__pycache__' / cache_name('cnt.py')).exists()}"
     f"（下面三个进程每一个都能命中它）")
hits = 0
rcs = []
for _ in range(3):
    rc, out, err = run(["-c", "import cnt"])
    hits += out.count("BODY-cnt")
    rcs.append(rc)
emit(f"  有缓存：连开 3 个进程各 import 一次 → 模块体执行次数 = {hits} ｜ rc 序列 {rcs}")
noc = 0
rcs2 = []
for _ in range(3):
    wipe("cnt.py")                                           # 每轮都先把 .pyc 删干净
    rc, out, err = run(["-c", "import cnt"])
    noc += out.count("BODY-cnt")
    rcs2.append(rc)
emit(f"  无缓存（每轮删掉 .pyc）：同样 3 个进程 → 模块体执行次数 = {noc} ｜ rc 序列 {rcs2}")
emit(f"  → 两组计数相等 → {hits == noc}：有没有 .pyc，模块体该跑几次还是几次。")
emit("  → 第 3 节②已经证明「命中缓存时连源码都不读」，所以省下的只有读源码 + 解析 + 编译这三步；")
emit("    模块体里的 print、建连接、写注册表、发请求，一次都不会少。本节一个毫秒都没打，")
emit("    因为「省了多少时间」每跑必抖，不是这一页能保证的事实。")
print("  收口表（下面每一条都是本机 3.14.4 实测；跨版本 / 跨平台未验证，别当事实引用）")
print("  你看到的现象             │ 本机实测的判据                                    │ 该怎么办")
emit("  改了代码没生效            │ 缓存键 = (源码 mtime, 字节数)；两个都没变 → 旧码继续跑 │ 删 __pycache__，或让那两个数真的变")
emit("  目录里多出 __pycache__    │ 只有被 import 的模块写缓存，__main__ 永远不写        │ 可以删，下次 import 会长回来")
emit("  想让它别写缓存            │ -B / PYTHONDONTWRITEBYTECODE / sys.dont_write_bytecode │ 三个都只管写，不管读")
emit("  缓存想放到别处            │ sys.pycache_prefix 镜像的是**绝对路径**              │ 别把 prefix 设到别人家的目录")
emit("  源码不想给你              │ __pycache__/*.pyc 加载不了；同级 xxx.pyc 才行，且绑 magic │ 这是分发格式，不是加密格式")

shutil.rmtree(BOX)
left = len(list(BOX.rglob("*"))) if BOX.exists() else 0
emit(f"  收尾自证：{BOX.name}/ 还在吗 → {BOX.exists()} ｜ 残留条目 {left} 个 ｜ 本次起真子进程 {RUNS} 个 ｜ "
     f"沙箱里所有 __pycache__ 与 prefix 镜像目录都随它一起删除")
emit(f"  当前目录的 __pycache__ 有没有被本脚本新增 → {Path('__pycache__').exists() != PYC_BEFORE}")
user = Path.home().name
leaks = sum(1 for ln in LINES if str(ROOT) in ln or (user and user in ln) or "/tmp/" in ln or ln.startswith("/"))
emit(f"  输出自检：出现绝对路径 / 临时目录 / 登录名的行数 = {leaks}")
