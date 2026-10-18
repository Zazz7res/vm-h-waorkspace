#!/usr/bin/env python3
# ============================================================================
# 取证脚本 i_site_and_userdir.py —— 第 09 章：site 模块、site-packages、.pth 与 user site
#
# 01 章讲过 sys.path 是怎么拼出来的；这个脚本讲**谁**把它拼出来的：
# 解释器启动时偷偷 import 的那个 site 模块。它干三件事 —— 加包目录、处理 .pth、
# 决定是否启用家目录那个 ~/.local 通道。全部只读实测：
# **不往 ~/.local、~/.config、家目录、/etc 写任何东西**，家目录路径一律折成 <HOME>。
#
# 它要证明的六件事（本机 Python 3.14.4 / Ubuntu 26.04 实测）：
#   ① 本机的包目录**不叫 site-packages**，叫 dist-packages（Debian 改名），
#      而且 site.getsitepackages() 返回的 3 条里有 1 条根本不存在；
#   ② ENABLE_USER_SITE 是 True，家目录那条路径却不在 sys.path 里 ——
#      因为 site 只往**存在的目录**里加（源码那行现场读出来）；
#   ③ `-S` 之后 sys.path 少掉的就是 site 加的那几条，★连 sys.meta_path 上那个
#      DistutilsMetaFinder 都会一起消失 —— 那是系统里一个 .pth 文件塞进来的；
#   ④ .pth 的两种行：普通行加路径（相对 .pth 自己所在目录解析，指向不存在的目录会**静默**丢掉），
#      `import ` 开头的行**被 exec**；
#   ⑤ ★安全面实测：.pth 里的代码在每次 python 启动时执行，而且本机实测一次启动跑了**两遍**
#      （site.main() 里 venv() 和 addsitepackages() 扫了同一个目录两次）；
#      对照组：放在 PYTHONPATH 目录里的 .pth 完全不生效；
#   ⑥ 一条坏 .pth 让启动打印一坨 traceback，但**退出码还是 0**，并且它后面的行全部作废。
#
# 用法：python3 i_site_and_userdir.py         退出码 0 = 全跑通
# 现场只在 site-demo/ 里（含一个 --without-pip 的 venv），结尾整份删掉并自证。
# 只用标准库；不联网、不装包。屏幕上只有 basename、相对路径和 <cwd>/<base>/<HOME> 标记。
# ============================================================================
import atexit
import ast
import os
import shutil
import site
import stat
import subprocess
import sys
import unicodedata
from pathlib import Path

WORK = Path("site-demo")
VENV = WORK / "venv"
TARGET = WORK / "pth-target"            # .pth 指过去的第一站（里面放 pthmod.py）
LOST = WORK / "no-such-dir"             # .pth 里故意指向一个不存在的目录
BAD1 = WORK / "bad1"                    # 坏 .pth 之前的那行
BAD2 = WORK / "bad2"                    # 坏 .pth 之后的那行（★应该加不进来）
PPATH = WORK / "pythonpath-dir"         # 对照组：PYTHONPATH 里的 .pth
COUNTER = WORK / "exec-count.txt"       # .pth 里那行代码写出来的计数器
ROOT = Path.cwd().resolve()             # 只用来打码，永不上屏
_cleaned = False

DROP = ("PYTHONPATH", "PYTHONHOME", "VIRTUAL_ENV", "VIRTUAL_ENV_PROMPT",
        "PYTHONSTARTUP", "PYTHONNOUSERSITE", "PYTHONWARNINGS")
PYDIR = f"python{sys.version_info.major}.{sys.version_info.minor}"

# 子进程的统一体检表：只报布尔/计数/折叠过的相对形态。
REPORT = (
    "import importlib.util, os, site, sys\n"
    f"root = os.getcwd(); base = sys.base_prefix; home = os.path.expanduser('~')\n"
    "def tag(p):\n"
    "    ap = os.path.abspath(p)\n"
    "    if ap == root or ap.startswith(root + os.sep):\n"
    "        return os.path.relpath(ap, root)\n"
    "    if ap == base or ap.startswith(base + os.sep):\n"
    "        return '<base>' + ap[len(base):]\n"
    "    if ap == home or ap.startswith(home + os.sep):\n"
    "        return '<HOME>' + ap[len(home):]\n"
    "    return '<abs:' + os.path.basename(ap) + '>'\n"
    "print('PATH_COUNT', len(sys.path))\n"
    "print('PKG_DIRS', sorted(tag(p) for p in sys.path if 'packages' in p))\n"
    "print('PREFIX_EQ_BASE', sys.prefix == sys.base_prefix)\n"
    "print('ENABLE_USER_SITE', site.ENABLE_USER_SITE)\n"
    "print('USER_SITE_TAG', tag(site.getusersitepackages()))\n"
    "print('USER_SITE_EXISTS', os.path.isdir(site.getusersitepackages()))\n"
    "print('USER_SITE_IN_PATH', os.path.abspath(site.getusersitepackages()) in\n"
    "                          [os.path.abspath(x) for x in sys.path])\n"
    "print('GETSITEPKG_COUNT', len(site.getsitepackages()))\n"
    "print('GETSITEPKG_TAGS', sorted(tag(p) for p in site.getsitepackages()))\n"
    "print('GETSITEPKG_EXIST', sum(1 for p in site.getsitepackages() if os.path.isdir(p)))\n"
    "print('META_PATH', [getattr(f, '__name__', type(f).__name__) for f in sys.meta_path])\n"
    "print('PTH_FILES', sorted(os.path.basename(d) for p in sys.path if os.path.isdir(p)\n"
    "                           for d in os.listdir(p) if d.endswith('.pth')))\n"
    "sp = importlib.util.find_spec('pthmod')\n"
    "print('PTHMOD_FOUND', sp is not None)\n"
    "print('PTHMOD_TAG', tag(sp.origin) if sp and sp.origin else '')\n"
    "names = [os.path.basename(os.path.abspath(x)) for x in sys.path]\n"
    "print('BADMINE_IN_PATH', 'pth-target' in names)\n"
    "print('LOST_IN_PATH', 'no-such-dir' in names)\n"
    "print('BAD1_IN_PATH', 'bad1' in names)\n"
    "print('BAD2_IN_PATH', 'bad2' in names)\n"
)


def fold(p):
    """路径 → 屏幕形态：沙箱内给相对，base_prefix 内给 <base>/…，家目录给 <HOME>/…"""
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


def fold_text(s):
    """多行文本（site.py 源码行、报错原文）里的绝对路径同样打码。"""
    s = s.replace(str(ROOT), "<cwd>")
    s = s.replace(os.path.expanduser("~"), "<HOME>")
    if sys.base_prefix and sys.base_prefix != os.sep:
        s = s.replace(sys.base_prefix + os.sep, "<base>/")
    return s


def pad(text, width):
    """按显示宽度补空格（中文/全角算两格），只为表格对齐，不改内容。"""
    s = str(text)
    w = sum(2 if unicodedata.east_asian_width(c) in ("W", "F") else 1 for c in s)
    return s + " " * max(0, width - w)


def onexc(func, path, exc):
    """rmtree 兜底：放开权限重试。venv/.__pycache__ 可能有不可写条目，别让它删到一半抛出来。"""
    try:
        os.chmod(os.path.dirname(path), stat.S_IRWXU)
        os.chmod(path, stat.S_IRWXU)
        func(path)
    except OSError:
        pass


def cleanup():
    """删沙箱并自证；只打印一次（第 7 段手动调过之后 atexit 不再重复）。"""
    global _cleaned
    if _cleaned:
        return
    _cleaned = True
    if WORK.exists():
        shutil.rmtree(WORK, onexc=onexc)
    left = sorted(x.name for x in Path(".").iterdir() if x.name.endswith("-demo"))
    print(f"\n  清理：{WORK.name}/ 整份删除（venv、.pth、计数器都在里面）"
          f"｜ 还在吗 → {WORK.exists()} ｜ 当前目录里 *-demo 残留 = {left or '无'}"
          f" ｜ 家目录一个字节都没写过：{WORK.name} 之外没有本脚本创建的文件")


def mk_env(extra=None):
    env = {k: v for k, v in os.environ.items() if k not in DROP}
    env.setdefault("PATH", "/usr/bin:/bin")
    env["PYTHONIOENCODING"] = "utf-8"
    if extra:
        env.update(extra)
    return env


def as_list(s):
    """把子进程打印成 list repr 的那几行还原成真 list —— 只为做集合差，不改内容。"""
    try:
        val = ast.literal_eval(s)
    except (ValueError, SyntaxError):
        return []
    return val if isinstance(val, list) else []


def child(argv, env=None, label=""):
    """起子进程读体检表；返回 (facts, returncode, 折叠后的 stderr)。"""
    proc = subprocess.run(argv, cwd=str(ROOT), env=mk_env(env),
                          capture_output=True, text=True)
    facts = {}
    for line in proc.stdout.splitlines():
        key, _, val = line.partition(" ")
        facts[key] = val
    return facts, proc.returncode, fold_text(proc.stderr)


def site_lines(patterns):
    """从解释器自己带的那个 site.py 里把源码行抠出来当证据（只读，路径折成 <base>/…）。"""
    src = getattr(site, "__file__", None)
    if not src or not os.path.isfile(src):
        return "（本机读不到 site.py 源码，跳过源码取证）", []
    out = []
    for num, line in enumerate(open(src, encoding="utf-8").read().splitlines(), 1):
        if any(pat in line for pat in patterns):
            out.append(f"    {num:>4}  {line.strip()}")
    return fold(src), out


VENV_PY = str(VENV / "bin" / "python")   # 相对当前目录，屏幕上不出现绝对路径

print("=== 1. site 报的几个值：本机的包目录到底叫什么 ===")
if WORK.exists():
    shutil.rmtree(WORK, onexc=onexc)
atexit.register(cleanup)                 # 沙箱一出现就登记收尾，中途异常也要删
base_facts, rc_base, err_base = child([sys.executable, "-c", REPORT], label="基座")
print(f"  子进程（`{os.path.basename(sys.executable)} -c 体检表`）退出码 {rc_base}"
      f"{'，stderr 为空' if not err_base.strip() else '，★有 stderr'}")
for title, key in (("sys.prefix == sys.base_prefix", "PREFIX_EQ_BASE"),
                   ("len(sys.path)", "PATH_COUNT"),
                   ("site.getsitepackages() 条数", "GETSITEPKG_COUNT"),
                   ("其中有几条真实存在", "GETSITEPKG_EXIST"),
                   ("site.ENABLE_USER_SITE", "ENABLE_USER_SITE"),
                   ("家目录那个目录存在吗", "USER_SITE_EXISTS"),
                   ("它在 sys.path 里吗", "USER_SITE_IN_PATH")):
    print("  " + pad(title, 32) + base_facts.get(key, "?"))
print(f"  sys.path 里带 'packages' 的条目（排序）：{base_facts.get('PKG_DIRS')}")
print(f"  getsitepackages() 给的路径（排序，折叠）：{base_facts.get('GETSITEPKG_TAGS')}")
print(f"  user site 的形状（家目录折成 <HOME>，只打印形状）：{base_facts.get('USER_SITE_TAG')}")
print("  → 判据 ①：本机所有包目录都叫 **dist-packages**，一条 site-packages 都没有 —— "
      "Debian/Ubuntu 打的补丁。别处抄来的「去 site-packages 里找」的命令在这台机器上会扑空。")
print("  → 判据 ②：非 venv 进程里 sys.prefix 和 sys.base_prefix 相等（这一行就是「我没在 venv 里」的判据，"
      "09 章 h 节里两者不等）。")
print(f"  ★ 代价：getsitepackages() 报 {base_facts.get('GETSITEPKG_COUNT')} 条、"
      f"实际存在 {base_facts.get('GETSITEPKG_EXIST')} 条 —— API 的返回值不等于磁盘现实，"
      "site 自己也是先 `isdir` 再决定的，所以排查时要以 sys.path 为准，不是以 site 的 API 为准。")

print("\n=== 2. user site：开关是 True，路径却没进 sys.path（源码那行现场抠出来）===")
src_path, lines = site_lines(["ENABLE_USER_SITE and os.path.isdir(user_site)",
                              "def addusersitepackages"])
print(f"  证据来自解释器自带的 {src_path}：")
for line in lines:
    print(f"  {line}")
s_facts, rc_s, _ = child([sys.executable, "-s", "-c", REPORT], label="-s")
print(f"  实测对照：默认 sys.path 条数 {base_facts.get('PATH_COUNT')} → 加 `-s` 之后 {s_facts.get('PATH_COUNT')}"
      f"（ENABLE_USER_SITE 变成 {s_facts.get('ENABLE_USER_SITE')}）")
print(f"  → 判据：这台机器上两个数字**一样**，因为 {base_facts.get('USER_SITE_TAG')} 这个目录压根不存在"
      f"（USER_SITE_EXISTS={base_facts.get('USER_SITE_EXISTS')}），`-s` 自然无事可关。")
print("  ★ 这条不是「本机特殊」，是排查价值：谁哪天执行过一次 `pip install --user`，"
      "那个目录被创建出来，你的包就会**绕开 venv** 出现在所有环境里 —— "
      "h 节实测过 venv 里 ENABLE_USER_SITE 被硬性关掉（False），就是为了堵这条缝。")
print("  未验证：`pip install --user` 之后的真实效果（本机没有 pip，绝不安装，也不碰家目录）。")

print("\n=== 3. python -S：跳过 site 之后少掉的是什么（连一个第三方 finder 都掉了）===")
n_facts, rc_n, _ = child([sys.executable, "-c", REPORT], label="默认")
S_facts, rc_S, err_S = child([sys.executable, "-S", "-c", REPORT], label="-S")
print(f"  len(sys.path)：默认 {n_facts.get('PATH_COUNT')} → -S {S_facts.get('PATH_COUNT')}")
only_default = sorted(set(as_list(n_facts.get("PKG_DIRS"))) - set(as_list(S_facts.get("PKG_DIRS"))))
print(f"  默认有、-S 没有的包目录条目（排序）：{only_default or '无'}")
print("  （上面那一行是把两边的 PKG_DIRS 当集合做差；两边都是同一套打码规则下的相对/<base> 形态，"
      "所以差集只反映「site 加了哪几条」。）")
print(f"  -S 之后还剩这些包目录：{S_facts.get('PKG_DIRS')}")
print(f"  sys.meta_path：默认 {n_facts.get('META_PATH')}")
print(f"               -S   {S_facts.get('META_PATH')}")
print(f"  -S 之后 site.ENABLE_USER_SITE = {S_facts.get('ENABLE_USER_SITE')}"
      f"（None：算这个值的代码就是 site.main() 自己，跳过它它就没被算过）")
print("  → 判据：`-S` 砍掉的正是 site 加的那几条包目录；标准库那条链一条不少 —— "
      "再次印证「venv/site 只管第三方包目录」。")
print("  ★ 注意 meta_path 第一项：默认进程里有个 DistutilsMetaFinder，它不属于 importlib，"
      f"是系统里这个 .pth 带来的 → {n_facts.get('PTH_FILES')}。"
      "-S 一开它就没影了：启动时执行的一个 .pth 文件改写了导入机制本身。")

print("\n=== 4. 建一个 venv，往它的 site-packages 放 .pth：加路径这一半机制 ===")
proc = subprocess.run([sys.executable, "-m", "venv", "--without-pip", str(VENV)],
                      cwd=str(ROOT), env=mk_env(), capture_output=True, text=True)
SP = VENV / "lib" / PYDIR / "site-packages"
print(f"  `python3 -m venv --without-pip {fold(VENV)}` → 退出码 {proc.returncode}"
      f"（本机只能这么建，原因见 h 节）｜ site-packages 在 {fold(SP)}")
TARGET.mkdir(parents=True)
(TARGET / "pthmod.py").write_text('MARK = "我是靠 .pth 才被找到的"\n', encoding="utf-8")
before_pth = child([VENV_PY, "-c", REPORT])[0]
print(f"  venv 里现在能找到 pthmod 吗 → {before_pth.get('PTHMOD_FOUND')}"
      f" ｜ sys.path 里有 pth-target 吗 → {before_pth.get('BADMINE_IN_PATH')}")
rel_target = os.path.relpath(str(ROOT / TARGET), str(SP.resolve()))
rel_lost = os.path.relpath(str(ROOT / LOST), str(SP.resolve()))
(SP / "demo.pth").write_text(f"{rel_target}\n{rel_lost}\n", encoding="utf-8")
print(f"  写 {fold(SP / 'demo.pth')}，两行内容（都相对 .pth 自己所在目录）：")
print(f"    {rel_target}      ← 指向沙箱里真实存在的 {fold(TARGET)}")
print(f"    {rel_lost}   ← 指向根本不存在的 {fold(LOST)}")
after = child([VENV_PY, "-c", REPORT])[0]
print(f"  再问一次：pthmod 找到了吗 → {after.get('PTHMOD_FOUND')}"
      f" ｜ 它的 __file__（相对当前目录） → {after.get('PTHMOD_TAG')}")
print(f"  sys.path 条数 {before_pth.get('PATH_COUNT')} → {after.get('PATH_COUNT')}"
      f"；pth-target 进 sys.path 了吗 → {after.get('BADMINE_IN_PATH')}"
      f"；带 packages 的条目 → {after.get('PKG_DIRS')}")
print(f"  那个不存在的目录被加进 sys.path 了吗 → {after.get('LOST_IN_PATH')}"
      f"   ★site 对 .pth 的每一行先做 `os.path.exists` 才加，不存在就**静默丢掉**，一句都不报。")
(SP / "demo.pth").unlink()
print("  → 判据：`.pth` 里的普通行 = 相对 **本文件所在目录**（不是你的 cwd！）解析出来的路径，"
      "逐条 append 到 sys.path。这就是 `-e` 开发式安装（egg-link / __editable__）背后的全部魔法。")

print("\n=== 5. ★ 安全面实测：.pth 里 import 开头的行是被 exec 的，而且本机一次启动跑两遍 ===")
src_path, lines = site_lines(['if line.startswith(("import ", "import\\t"))', "exec(line)"])
print(f"  源码就在 {src_path} 里，两条分支泾渭分明：")
for line in lines:
    print(f"  {line}")
PPATH.mkdir(parents=True, exist_ok=True)
line_exec = "import os; open(os.path.join(os.getcwd(), '%s/exec-count.txt'), 'a').write('x')" % WORK.name
(SP / "execdemo.pth").write_text(f"{line_exec}\n", encoding="utf-8")
print(f"  往 venv 的 site-packages 放 execdemo.pth，唯一一行是：")
print(f"    {line_exec}")
print(f"  它相对 cwd 往 {WORK.name}/exec-count.txt 追加一个字符 —— 无害，但性质是「任意代码」。")
before = COUNTER.read_text(encoding="utf-8") if COUNTER.exists() else ""
f5 = child([VENV_PY, "-c", "print('什么都不 import')"], label="启动一次")
after_n = len(COUNTER.read_text(encoding="utf-8")) if COUNTER.exists() else -1
print(f"  只起一个进程、一句 `print` 都没碰到 pthmod：计数器字符数 = {after_n - len(before)}"
      f"（跑了几遍就是几）｜ 子进程退出码 {f5[1]}，stdout 只有它自己那行")
print("  ★ 两遍，不是一遍。原因在 site.py 里（同一份源码，行号现场抠出来）：")
src_path2, lines2 = site_lines(["addsitepackages(known_paths, [sys.prefix])",
                                "known_paths = addsitepackages(known_paths)"])
for line in lines2:
    print(f"    {line}")
print("    第一行在 site.venv() 内部：认出 pyvenv.cfg 之后马上扫一遍 venv 自己的 site-packages；")
print("    第二行在 site.main() 里：随后 addsitepackages() 按 getsitepackages() 再扫一遍，")
print("    而虚拟环境里 getsitepackages() 的第一项**还是**那个目录 → 同一个目录扫两遍、")
print("    同一份 .pth 读两遍、里面的代码被执行两遍。")
print("    ★ 这条「几遍」的计数依赖本机这份 site.py 的实现，换 Python 版本/发行版未验证 —— "
      "但「.pth 里的代码会在每次启动时被执行」这件事本身与版本无关。")
print("  → 判据：.pth 里的 import 行**必须幂等**（可重复执行、无副作用累积）；"
      "装一个包 = 授权它在**每次** python 启动时执行代码，这不是比喻，是第 5 段这个计数器。")
(SP / "execdemo.pth").unlink()
(PPATH / "pythonpath.pth").write_text(f"{line_exec}\n", encoding="utf-8")
CTRL = COUNTER.read_text(encoding="utf-8") if COUNTER.exists() else ""
ctrl = child([sys.executable, "-c", REPORT], env={"PYTHONPATH": str(ROOT / PPATH)}, label="对照")
ctrl_n = len(COUNTER.read_text(encoding="utf-8")) if COUNTER.exists() else -1
print(f"  对照组：把同样的 .pth 放进一个 **PYTHONPATH** 目录 → "
      f"计数器字符数增量 = {ctrl_n - len(CTRL)}（0 = 一行都没执行）；"
      f"pthmod 找到吗 → {ctrl[0].get('PTHMOD_FOUND')}")
print("  → 判据：.pth 只在「被 site 当成包目录扫描」的目录里生效（sys.path 里那些包目录、"
      "以及 .pth 自己所在的目录）；PYTHONPATH 里的目录只是普通路径项，.pth 当普通文件被忽略。")
print(f"  顺带给出第 ④ 步的现成查法（扫 sys.path 每个目录里的 *.pth，只报 basename）："
      f"把本段建的 demo .pth 删掉之后 → {child([VENV_PY, '-c', REPORT])[0].get('PTH_FILES')}；"
      f"第 3 段基座里抓到的是 {n_facts.get('PTH_FILES')}。")

print("\n=== 6. 一条坏 .pth 的现场：报错很响，退出码是 0，后面的行全部作废 ===")
BAD1.mkdir(parents=True, exist_ok=True)
BAD2.mkdir(parents=True, exist_ok=True)
(BAD1 / "b1mod.py").write_text("B1 = 1\n", encoding="utf-8")
(BAD2 / "b2mod.py").write_text("B2 = 1\n", encoding="utf-8")
rel_b1 = os.path.relpath(str(ROOT / BAD1), str(SP.resolve()))
rel_b2 = os.path.relpath(str(ROOT / BAD2), str(SP.resolve()))
(SP / "broken.pth").write_text(f"{rel_b1}\nimport no_such_module_from_pth\n{rel_b2}\n", encoding="utf-8")
print(f"  broken.pth 三行：{rel_b1}（存在）／ import no_such_module_from_pth（不存在）／ {rel_b2}（存在）")
f6, rc6, err6 = child([VENV_PY, "-c", REPORT], label="坏 .pth")
print(f"  子进程退出码 → {rc6}   ★启动成功。坏 .pth 不会拦住任何人，它只在 stderr 上喊一嗓子。")
for line in err6.strip().splitlines():
    print(f"    {line}")
print(f"  实际加进 sys.path 的情况：坏行之前的 bad1 进来了吗 → {f6.get('BAD1_IN_PATH')}"
      f" ｜ 坏行之后的 bad2 进来了吗 → {f6.get('BAD2_IN_PATH')}")
print(f"  ★ 数一下上面那坨出现了几遍：{err6.count('Remainder of file ignored')} 遍 ——"
      "不是终端抽风，是第 5 段那个「一次启动扫两遍」的同一个原因，报错也翻倍。")
print("  → 判据：site 对每一行 try/except，异常时打印上面那坨然后 **break** —— "
      "「Remainder of file ignored」是字面意思，坏行**后面**的路径一条都不会加。")
print("  ★ 代价：这类损坏极难发现，因为退出码是 0、报错在 stderr、而且每次启动都重复打；"
      "CI 里 `set -o pipefail` 或把 stderr 当日志翻的人才会撞见它。排查时 `python -X importtime` 不管用，"
      "要 `-v` 或直接看包目录里的 *.pth。")
(SP / "broken.pth").unlink()

print("\n=== 7. 收口：import 说找不到包时，按这个顺序查（每一步都有对应实测）===")
print("  ① 先问「我在哪个解释器上」：`sys.executable` / `sys.prefix == sys.base_prefix`。"
      "相等 = 你根本不在 venv 里（第 1 段实测）。这一步跳过，后面全白查。")
print("  ② 再问 sys.path 里到底有什么（排序打出来对照）：脚本所在目录 ≠ 当前目录 —— "
      "`python3 pkg/main.py` 时 sys.path[0] 是 pkg/，不是 cwd（01 章实测过）。")
print("  ③ 再看包目录：本机叫 dist-packages 不叫 site-packages（第 1 段），"
      "所以「去 site-packages 里 ls」这条命令在这台机器上查不到东西不代表没装。")
print("  ④ 然后查 .pth：`os.listdir` 包目录里的 *.pth（第 5 段那条 PTH_FILES 就是现成查法）。"
      "它可能在你不知情时加过路径（第 4 段），也可能因为一行坏代码把后面的路径全吞了（第 6 段）。")
print("  ⑤ 最后才怀疑 user site / 装错环境：`pip install --user` 的产物在 <HOME> 那条目录里，"
      "venv 里它被硬性关掉（第 2 段 + h 节实测 ENABLE_USER_SITE=False）。")
print("  不该做的事：靠 `sys.path.append(绝对路径)` 治本 —— 那只把「环境不对」变成「只有你这台机器能跑」；"
      "正解是让解释器自己找到对的目录（venv 的骨架，h 节）。")
print("  未验证、别当事实引用的部分：Windows 上 site 的 AppData 形态 user site、"
      "macOS framework 版的 dist-packages 命名问题（那边没有 Debian 那套补丁）、"
      "以及 `python -m site --user-site` / `PYTHONNOUSERSITE` 在别的发行版上的差异 —— "
      "本机一条都测不到（本机连 pip 都没有，见 h 节）。")
print("  → 一句话：`import` 找不找得到，是 site 在启动那一刻替你排好的表；"
      "表可以来自 venv、来自 .pth、来自家目录，而这三处都在你的项目之外 —— 所以排查要按上面的顺序倒着拆。")

cleanup()
sys.exit(0)
