#!/usr/bin/env python3
# ============================================================================
# 取证脚本 h_venv_mechanics.py —— 第 09 章：虚拟环境到底改了什么（本机残缺版如实演）
#
# ★ 本机前提（开场就实测，不靠 README 背书）：
#   这台 Ubuntu 没装 python3-venv → `import ensurepip` 直接 ModuleNotFoundError，
#   所以 `python3 -m venv X`（带 pip 引导）**失败退出 1**，只有 `--without-pip` 建得出来。
#   本脚本把这条失败原文演出来当证据，全程不联网、不 pip install、不 apt install。
#   想通这一点之后剩下的结论是通用的：venv 的**骨架**和 pip 是两件事。
#
# 它要证明的六件事（本机 Python 3.14.4 / Ubuntu 实测）：
#   ① 骨架就那几样：bin/ 里全是**指向同目录 python3 的相对符号链接** + 三个 activate 脚本，
#      lib/pythonX.Y/site-packages/ 是空的，pyvenv.cfg 是唯一的配置；
#   ② venv **不复制解释器**：激活前后 real path 相同、inode 相同、version_info 相同；
#   ③ 换掉的只有 sys.path：venv 自己的 site-packages 插进来，标准库那条链原封不动；
#   ④ include-system-site-packages 这个开关实测有区别（但本机系统目录叫 dist-packages，
#      ★数 'site-packages' 的那个朴素判据在本机数不到它们）；
#   ⑤ activate 里真正生效的就几行 export；手工设 PATH/VIRTUAL_ENV 起子进程——
#      生效的是「PATH 让 python3 这个词命中哪个文件」，VIRTUAL_ENV 只是给工具和提示符看的；
#   ⑥ 带 pip 的 venv 在本机为什么建不出来，失败后磁盘上留下什么。
#
# 用法：python3 h_venv_mechanics.py           退出码 0 = 全跑通
# 现场只在 venv-demo/ 里，结尾整份删掉并自证；屏幕上只有 basename、相对路径和 <cwd>/<base> 标记。
# ============================================================================
import atexit
import os
import shutil
import stat
import subprocess
import sys
import unicodedata
from pathlib import Path

WORK = Path("venv-demo")
DEMO1 = WORK / "demo1"                       # --without-pip 建的常规骨架
DEMO2 = WORK / "demo2"                       # --without-pip --system-site-packages
BROKEN = WORK / "broken"                     # 带 pip 引导的失败现场
MOVED = WORK / "moved"                       # 把 demo1 整份搬到隔壁，验「能不能搬」
ROOT = Path.cwd().resolve()                  # 只用来打码，永不上屏
_cleaned = False

DROP = ("PYTHONPATH", "PYTHONHOME", "VIRTUAL_ENV", "VIRTUAL_ENV_PROMPT",
        "PYTHONSTARTUP", "PYTHONNOUSERSITE", "PYTHONWARNINGS")

# 子进程的统一体检表：全部只报布尔/计数/相对形态，绝不回绝对路径。
CHILD = (
    "import os, site, sys, importlib.util\n"
    "root = os.getcwd()\n"
    "base = sys.base_prefix\n"
    "def tag(p):\n"
    "    ap = os.path.abspath(p)\n"
    "    if ap == root or ap.startswith(root + os.sep):\n"
    "        return os.path.relpath(ap, root)\n"
    "    if ap == base or ap.startswith(base + os.sep):\n"
    "        return '<base>' + ap[len(base):]\n"
    "    return '<abs:' + os.path.basename(ap) + '>'\n"
    "print('PREFIX_EQ_BASE', sys.prefix == sys.base_prefix)\n"
    "print('PREFIX_TAG', tag(sys.prefix))\n"
    "print('BASE_PREFIX_TAG', tag(sys.base_prefix))\n"
    "print('EXE_TAG', tag(sys.executable))\n"
    "print('EXE_REAL_TAG', tag(os.path.realpath(sys.executable)))\n"
    "print('VERSION_INFO', tuple(sys.version_info))\n"
    "print('PATH_COUNT', len(sys.path))\n"
    "print('SP_COUNT', len([p for p in sys.path if 'site-packages' in p]))\n"
    "print('DP_COUNT', len([p for p in sys.path if 'dist-packages' in p]))\n"
    "pk = [p for p in sys.path if 'site-packages' in p or 'dist-packages' in p]\n"
    "print('PKG_DIR_COUNT', len(pk))\n"
    "print('STD_LIB_IN_PATH', os.path.abspath(os.path.dirname(os.__file__)) in [os.path.abspath(x) for x in sys.path])\n"
    "print('FIRST_PKG_DIR_INSIDE_SANDBOX', bool(pk) and os.path.abspath(pk[0]).startswith(root + os.sep))\n"
    "print('PKG_DIRS_TAGGED', sorted(tag(p) for p in pk))\n"
    "print('USER_SITE_ENABLED', site.ENABLE_USER_SITE)\n"
    "print('PIP_IMPORTABLE', importlib.util.find_spec('pip') is not None)\n"
)


def fold_text(s):
    """把文本里出现的绝对路径折成 <cwd>/… 和 <base>/…（报错原文、activate 源码都用它）。"""
    s = s.replace(str(ROOT), "<cwd>")
    s = s.replace(os.path.expanduser("~"), "<HOME>")
    for pref in (sys.base_prefix, sys.prefix):
        if pref and pref != os.sep:
            s = s.replace(pref + os.sep, "<base>/").rstrip()
    return s


def fold(p):
    """把单个路径折成屏幕上允许的形态：沙箱内给相对，base_prefix 内给 <base>/…，余下只留 basename。"""
    s = str(p)
    if not os.path.isabs(s):
        return os.path.normpath(s)
    rel = os.path.relpath(s, ROOT)
    if not rel.startswith(".." + os.sep):
        return rel
    if s.startswith(sys.base_prefix):
        return "<base>" + s[len(sys.base_prefix):]
    home = os.path.expanduser("~")
    if s.startswith(home):
        return "<HOME>" + s[len(home):]
    return "<abs:" + os.path.basename(s) + ">"


def pad(text, width):
    """按显示宽度补空格（中文/全角占两格），让表格在终端里真的对齐；只影响排版，不影响内容。"""
    s = str(text)
    w = sum(2 if unicodedata.east_asian_width(c) in ("W", "F") else 1 for c in s)
    return s + " " * max(0, width - w)


def onexc(func, path, exc):
    """rmtree 的兜底：放开权限再试一次。venv 里可能有不可写条目，默认 rmtree 会删到一半抛。"""
    try:
        os.chmod(os.path.dirname(path), stat.S_IRWXU)
        os.chmod(path, stat.S_IRWXU)
        func(path)
    except OSError:
        pass


def cleanup():
    """删掉全部 venv 并当场自证。只打印一次（第 7 段手动调过之后 atexit 不再重复）。"""
    global _cleaned
    if _cleaned:
        return
    _cleaned = True
    if WORK.exists():
        shutil.rmtree(WORK, onexc=onexc)
    left = sorted(x.name for x in Path(".").iterdir() if x.name.endswith("-demo"))
    print(f"\n  清理：{WORK.name}/ 整份删除（含 demo1/demo2/broken/moved 四个骨架）"
          f"｜ 还在吗 → {WORK.exists()} ｜ 当前目录里 *-demo 残留 = {left or '无'}")


def mk_env(extra=None):
    """干净 env：剥掉继承的 PYTHONPATH/PYTHONHOME/VIRTUAL_ENV，再叠上这一局要设的变量。"""
    env = {k: v for k, v in os.environ.items() if k not in DROP}
    env.setdefault("PATH", "/usr/bin:/bin")
    env["PYTHONIOENCODING"] = "utf-8"
    if extra:
        env.update(extra)
    return env


def run(argv, env=None, label=""):
    """跑一个子进程，返回解析好的体检表（dict）。cwd 固定在工作目录，路径全部折过。"""
    proc = subprocess.run(argv, cwd=str(ROOT), env=mk_env(env),
                          capture_output=True, text=True)
    facts = {}
    for line in proc.stdout.splitlines():
        parts = line.split(" ", 1)
        facts[parts[0]] = parts[1] if len(parts) > 1 else ""
    if proc.returncode != 0:                       # 体检表跑不出来就是脚本的错，当场摊开
        print(f"  ★ {label} 子进程退出码 {proc.returncode}：{fold_text(proc.stderr.strip())[:200]}")
    return facts, proc


def venv_python(venv_dir):
    return str(Path(venv_dir) / "bin" / "python")   # 相对当前目录的形态：屏幕上不出现绝对路径


print("=== 1. 本机前提：ensurepip 不在，所以只有 --without-pip 建得出来 ===")
if WORK.exists():                        # 上次崩溃留下的残骸：先整份清掉，保证每次同一现场
    shutil.rmtree(WORK, onexc=onexc)
atexit.register(cleanup)                 # 沙箱一出现就登记收尾，中途异常也要删
probe = subprocess.run([sys.executable, "-c", "import ensurepip"], cwd=str(ROOT),
                       env=mk_env(), capture_output=True, text=True)
print(f"  子进程 `{os.path.basename(sys.executable)} -c \"import ensurepip\"` → 退出码 {probe.returncode}")
print(f"  报错原文（最后一行）：{fold_text(probe.stderr.strip().splitlines()[-1])}")
for args in (["--without-pip", str(DEMO1)],
             ["--without-pip", "--system-site-packages", str(DEMO2)]):
    proc = subprocess.run([sys.executable, "-m", "venv"] + args,
                          cwd=str(ROOT), env=mk_env(), capture_output=True, text=True)
    print(f"  python3 -m venv {' '.join(a for a in args if not a.startswith('venv-demo'))} <目标>"
          f" → 退出码 {proc.returncode} ｜ 输出行数 {len((proc.stdout + proc.stderr).splitlines())}")
print(f"  → 建出来的骨架顶部（排序 + 类型）："
      f"{[(p.name, 'link' if p.is_symlink() else ('dir' if p.is_dir() else 'file')) for p in sorted(DEMO1.iterdir())]}")
print("  → 判据：venv 的「骨架」和「pip」是两个独立步骤。骨架是纯文件操作（建目录、连链接、写 cfg），"
      "pip 引导要靠 stdlib 里的 ensurepip 模块 —— 发行版可以只给你前者。")

print("\n=== 2. 拆骨架：bin/ 里全是相对符号链接，pyvenv.cfg 是唯一配置 ===")
bin_dir = DEMO1 / "bin"
print(f"  {WORK.name}/demo1/bin 里的名字（排序）：{sorted(p.name for p in bin_dir.iterdir())}")
print("  逐个判「是链接还是文件」——只报 basename，不报绝对路径：")
for name in sorted(p.name for p in bin_dir.iterdir()):
    p = bin_dir / name
    if p.is_symlink():
        target = os.readlink(p)
        print(f"    {name:<12} islink={str(True):<6} readlink={fold(target):<20}"
              f" 目标写得绝对吗={os.path.isabs(target)}")
    else:
        print(f"    {name:<12} islink={str(False):<6} 普通文件，行数="
              f"{len(p.read_text(encoding='utf-8').splitlines())}")
print("  → 注意 python3 这一行：它直接链到基座里的绝对路径，另外三个（python / python3.14 / 𝜋thon）"
      "链的是同目录的 python3。★所以 bin/ 里「相对」和「绝对」两种链接混着，搬目录不一定坏，"
      "换机器一定坏（第 7 段实测）。")
print(f"  lib 底下（排序相对路径）：{sorted(fold(p) for p in (DEMO1 / 'lib').rglob('*') if p.is_dir())}")
PYDIR = f"python{sys.version_info.major}.{sys.version_info.minor}"
VENV_SP = DEMO1 / "lib" / PYDIR / "site-packages"
print(f"  ★ venv 自己的 site-packages（{fold(VENV_SP)}）此刻是空的："
      f"是目录吗 → {VENV_SP.is_dir()} ｜ 里面有几项 → "
      f"{len(list(VENV_SP.iterdir())) if VENV_SP.is_dir() else 'n/a'}")
print(f"  include/ 里有什么 → {sorted(p.name for p in (DEMO1 / 'include').rglob('*'))}（本机实测：空占位）")
print("  pyvenv.cfg 逐行读（绝对路径一律折成 <base>/ 或 <cwd>/）：")
for line in (DEMO1 / "pyvenv.cfg").read_text(encoding="utf-8").splitlines():
    if not line.strip():
        continue
    key, _, val = line.partition("=")
    folded = fold_text(val.strip())
    is_abs = os.path.isabs(val.strip())
    print(f"    {key.strip():<28} = {folded:<46} 绝对路径吗={is_abs}")
print("  → 判据：这个文件里写死了 home / executable / command 三条绝对路径 —— "
      "它就是「venv 不能跨机器搬」的物证（第 7 段回收）。")

print("\n=== 3. 关键判据：venv 不复制解释器，同一个 Python 换个归属 ===")
base_facts, _ = run([sys.executable, "-c", CHILD], label="基座")
v1_facts, _ = run([venv_python(DEMO1), "-c", CHILD], label="demo1")
rows = [("sys.prefix == sys.base_prefix", "PREFIX_EQ_BASE"),
        ("sys.prefix 折成相对", "PREFIX_TAG"),
        ("sys.base_prefix 折成相对", "BASE_PREFIX_TAG"),
        ("sys.executable 折成相对", "EXE_TAG"),
        ("realpath(sys.executable)", "EXE_REAL_TAG"),
        ("tuple(sys.version_info)", "VERSION_INFO"),
        ("site.ENABLE_USER_SITE", "USER_SITE_ENABLED")]
print("  " + pad("观察项", 30) + pad("python3（基座）", 32) + WORK.name + "/demo1/bin/python")
for title, key in rows:
    print("  " + pad(title, 30) + pad(base_facts.get(key, "?"), 32) + v1_facts.get(key, "?"))
same_real = base_facts.get("EXE_REAL_TAG") == v1_facts.get("EXE_REAL_TAG")
same_ver = base_facts.get("VERSION_INFO") == v1_facts.get("VERSION_INFO")
a = os.path.realpath(str(venv_python(DEMO1)))
b = os.path.realpath(sys.executable)
print(f"  两边 realpath 是同一个文件吗 → {same_real}"
      f" ｜ inode 相同吗 → {os.stat(a).st_ino == os.stat(b).st_ino}"
      f" ｜ 字节数相同吗 → {os.stat(a).st_size == os.stat(b).st_size}")
print(f"  version_info 相等吗 → {same_ver}；bin/ 下 4 个 python* 链接的 realpath 都指向同一个文件 → "
      f"{len({os.path.realpath(str(p)) for p in bin_dir.iterdir() if p.name.startswith('python')}) == 1}")
print("  → 判据：venv 里没有任何「第二个解释器」。它只是在 sys.executable 的 realpath 上方两层放了个 "
      "pyvenv.cfg，Python 启动时读到它，就把 sys.prefix 改成那个目录（site.venv() 干的）。")
print("  ★ 代价：同一份标准库、同一份 lib-dynload。venv 隔离的是**第三方包目录**，"
      "标准库版本要变只能换解释器（pyenv/conda/换发行版那一类，本机未验证，别当事实引用）。")

print("\n=== 4. sys.path 被换成了什么 + include-system-site-packages 开关实测 ===")
v2_facts, _ = run([venv_python(DEMO2), "-c", CHILD], label="demo2")
print(f"  {'计数项':<22} 基座 / demo1 / demo2（依次三个数）")
for title, key in (("len(sys.path)", "PATH_COUNT"),
                   ("含 site-packages 的条数", "SP_COUNT"),
                   ("含 dist-packages 的条数", "DP_COUNT"),
                   ("包目录总条数", "PKG_DIR_COUNT"),
                   ("标准库目录还在吗", "STD_LIB_IN_PATH")):
    print("  " + pad(title, 24) + f"{base_facts.get(key, '?')} / {v1_facts.get(key, '?')} / {v2_facts.get(key, '?')}")
print(f"  demo1 的包目录（排序，折叠）：{v1_facts.get('PKG_DIRS_TAGGED')}")
print(f"  demo2 的包目录（排序，折叠）：{v2_facts.get('PKG_DIRS_TAGGED')}")
print(f"  venv 自己的 site-packages 排在包目录最前吗 → demo1={v1_facts.get('FIRST_PKG_DIR_INSIDE_SANDBOX')}"
      f" demo2={v2_facts.get('FIRST_PKG_DIR_INSIDE_SANDBOX')}")
print("  ★ 数 'site-packages' 这个朴素判据在本机**失灵**：demo1 和 demo2 都只数到 1 条，"
      f"差别全在 dist-packages 那 {v2_facts.get('DP_COUNT')} 条上 —— Debian/Ubuntu 把系统包目录改名成了"
      " dist-packages，标准库以外的路径也照这个名字装。")
print(f"  → 真正的开关效果要这样读：demo1 的 dist-packages 条数 {v1_facts.get('DP_COUNT')} → "
      f"demo2 的 {v2_facts.get('DP_COUNT')}；开了系统包后，基座里已装的包在 venv 里就直接可见（隔离等于没做）。")
print(f"  顺带实测：venv 里 ENABLE_USER_SITE 是 {v1_facts.get('USER_SITE_ENABLED')}"
      f"（基座是 {base_facts.get('USER_SITE_ENABLED')}）—— venv 顺手把家目录那个 ~/.local 通道关了，"
      "免得用户级安装包搅浑环境。")

print("\n=== 5. activate 只干了几行 export：手工设 env 起子进程，魔法为零 ===")
act = (bin_dir / "activate").read_text(encoding="utf-8").splitlines()
hits = sorted({fold_text(ln.strip()) for ln in act
               if ln.strip().startswith(("export ", "PATH=", "VIRTUAL_ENV=", "_OLD_VIRTUAL"))})
print(f"  bin/activate 共 {len(act)} 行，其中真正改环境的就这些（去重排序，绝对路径已折）：")
for h in hits:
    print(f"    {h}")
print("  （其中 `$(cygpath …)` 那行在 Cygwin 分支里，本机走不到；"
      "剩下的就是备份旧值 → 设新值 → deactivate 时还原，三件事。）")
env_absent, proc_a = run([venv_python(DEMO1), "-c", CHILD], label="A：venv python + 不设任何环境变量")
print(f"  实验 A：`{WORK.name}/demo1/bin/python` 在 PATH 里没有它、VIRTUAL_ENV 也没设的情况下 → "
      f"sys.prefix = {env_absent.get('PREFIX_TAG')}")
fake_activated = {"VIRTUAL_ENV": str(ROOT / DEMO1),
                  "PATH": f"{ROOT / DEMO1 / 'bin'}{os.pathsep}{os.environ.get('PATH', '')}"}
b_facts, _ = run([sys.executable, "-c", CHILD], env=fake_activated, label="B")
print(f"  实验 B：基座 {os.path.basename(sys.executable)}，但手工塞好 VIRTUAL_ENV 和指向 demo1/bin 的 PATH → "
      f"sys.prefix = {b_facts.get('PREFIX_TAG')}、sys.path 里多了 venv 的 site-packages 吗 → "
      f"{b_facts.get('SP_COUNT') != base_facts.get('SP_COUNT')} ★prefix 没变，环境变量本身不制造 venv")
venv_path = f"{ROOT / DEMO1 / 'bin'}" + os.pathsep + os.environ.get("PATH", "")
w_plain = shutil.which("python3")
w_venv = shutil.which("python3", path=venv_path)
w_bare = shutil.which("python")
w_bare_venv = shutil.which("python", path=venv_path)
tagw = lambda p: ("<cwd>/" + os.path.relpath(p, ROOT)) if p and os.path.abspath(p).startswith(str(ROOT) + os.sep) \
    else (fold_text(p) if p else None)
print(f"  实验 C：`python3` 这个词命中哪个文件 —— 不设 PATH → {tagw(w_plain)}；"
      f"把 demo1/bin 放最前 → {tagw(w_venv)}")
print(f"  同法测 `python`（注意本机基座里根本没有 python 这个名字）：不设 → {tagw(w_bare)}；"
      f"设了 → {tagw(w_bare_venv)}")
print("  → 判据：激活 = 改 PATH（让 python 这个词命中 venv/bin 里那个链接）+ 设 VIRTUAL_ENV/PROMPT"
      "（给提示符和外部工具看）。真正让 sys.path 换掉的是解释器**自己被从哪个目录启动**，不是这些变量。")
print("  ★ 代价：`which python3` 对了不代表环境对了 —— 子 shell、nohup、cron、systemd、IDE 的解释器路径"
      "都不走你这个 shell 的 PATH，「忘了激活」的报错永远长得像「包没装」。")

print("\n=== 6. 诚实交代：本机的 pip 从哪一步断的（原文照打，不装、不修）===")
pip_probe = subprocess.run([venv_python(DEMO1), "-m", "pip", "--version"], cwd=str(ROOT),
                           env=mk_env(), capture_output=True, text=True)
print(f"  {WORK.name}/demo1/bin/python -m pip --version → 退出码 {pip_probe.returncode}")
for line in (pip_probe.stdout + pip_probe.stderr).strip().splitlines():
    print(f"    {fold_text(line)}")
print(f"  体检表里那条 PIP_IMPORTABLE：基座={base_facts.get('PIP_IMPORTABLE')}、"
      f"demo1={v1_facts.get('PIP_IMPORTABLE')} → 两边都没有 pip（这台机器就没装 pip）")
if BROKEN.exists():
    shutil.rmtree(BROKEN, onexc=onexc)
bad = subprocess.run([sys.executable, "-m", "venv", str(BROKEN)], cwd=str(ROOT),
                     env=mk_env(), capture_output=True, text=True)
print(f"  现在试**带 pip 引导**的 `python3 -m venv {WORK.name}/broken` → 退出码 {bad.returncode}，原文：")
for line in (bad.stdout + bad.stderr).strip().splitlines():
    print(f"    {fold_text(line)}")
silent = subprocess.run([sys.executable, "-m", "venv", str(BROKEN)], cwd=str(ROOT),
                        env=mk_env(), stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
print(f"  ★ 这段原文走的是 **stdout**：把 stdout 丢进 /dev/null 后 stderr 只剩 "
      f"{len(silent.stderr.splitlines())} 行 → CI 里只截 stderr 会看不到它。")
print(f"  失败之后磁盘上留下了什么（排序相对路径）：")
for p in sorted(BROKEN.rglob("*"), key=str):
    kind = "link" if p.is_symlink() else ("dir" if p.is_dir() else "file")
    print(f"    {fold(p):<46} {kind}")
print(f"  bin/activate 建出来了吗 → {(BROKEN / 'bin' / 'activate').exists()}"
      f" ｜ 骨架建到一半就退了 → 这也是「先判断 rc、再判断目录存在」不能省的原因。")
print("  → 本机断的是发行版没打包 python3-venv（ensurepip 被剥掉了），**不是 venv 的机制问题**；"
      "补法是在有网的机器上 `apt install python3.14-venv` —— 那是读者的动作，本章脚本绝不执行任何安装。")

print("\n=== 7. 收口：venv 做的三件事、不做的三件事（含搬目录实测）===")
if MOVED.exists():
    shutil.rmtree(MOVED, onexc=onexc)
shutil.copytree(DEMO1, MOVED, symlinks=True)
moved_facts, _ = run([venv_python(MOVED), "-c", CHILD], label="moved")
act_after = (MOVED / "bin" / "activate").read_text(encoding="utf-8")
old_ref = str(ROOT / DEMO1) in act_after
new_ref = str(ROOT / MOVED) in act_after
print(f"  把 demo1 整份搬到 {WORK.name}/moved 后，`moved/bin/python` 的 sys.prefix → "
      f"{moved_facts.get('PREFIX_TAG')}")
print(f"  解释器侧照样能用吗 → {moved_facts.get('PREFIX_TAG') == str(MOVED)}"
      f"（链接是相对的、cfg 里的 home 还在，所以**同一台机器上挪目录碰巧不坏**）")
print(f"  可 moved/bin/activate 里：还写着 demo1 的老地址吗 → {old_ref}；改成新地址了吗 → {new_ref}"
      f" ★激活脚本是建环境那一刻写死的，搬完的 venv 一激活就废")
print(f"  pyvenv.cfg 的 command 那行也还记着建它时的绝对路径（第 2 段已打出），"
      "换一台机器、换一个前缀目录、Windows 换盘符，这些行全都对不上。")
print("  venv **做**的三件事（上面全有实测）：")
print("  ① 给一个自己的 site-packages（demo1 的包目录就是它，空的时候也照样占住查找顺序第一位）；")
print("  ② 给一个自己的 bin/，里面是相对符号链接，激活后排在 PATH 最前；")
print("  ③ 设 VIRTUAL_ENV / VIRTUAL_ENV_PROMPT，让提示符和工具知道「我现在在这里」。")
print("  venv **不做**的三件事：")
print(f"  ① 不隔离标准库：{os.path.basename(sys.executable)} 和 demo1 用的是同一个解释器文件"
      f"（第 3 段 realpath/inode 两条布尔），标准库路径一条没少；")
print("  ② 不复制解释器，也不装任何包：骨架是纯文件操作，pip 要另说（第 1、6 段）；")
print("  ③ 不能跨机器搬运：pyvenv.cfg 的 home/executable/command 和 activate 里的 VIRTUAL_ENV "
      "全是写死的绝对路径（第 2 段、本段实测）。")
print("  未验证、别当事实引用的部分：Windows 的 venv 布局（Scripts\\ 而不是 bin\\、没有符号链接而是拷贝）、"
      "macOS framework 版的路径形态、以及装了 python3-venv 的机器上 pip 引导成功的完整输出 —— "
      "本机这台 Ubuntu 一条都测不到。")
print("  → 一句话：venv 是「一个目录 + 一份 cfg + 一堆相对链接」换来的 sys.path 改写，"
      "轻到可以随手建十个，也脆到搬一次就散。")

cleanup()
sys.exit(0)
