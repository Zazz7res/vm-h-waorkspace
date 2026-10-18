#!/usr/bin/env python3
# ============================================================================
# 取证脚本 h_pathlib_vs_ospath.py —— 第 08 章第 9 节：os.path 的字符串函数逐个对照
#
# 这个脚本要证明什么：
#   os.path.* 和 pathlib 不是「新旧两代 API」这么简单，它们的**返回类型**和**容错范围**都不同：
#   os.path 收字符串、吐字符串，除了那几个 stat 之外全不碰内核；pathlib 收/吐 Path 对象，
#   把「算路径」和「问路径」拆成了两组方法。本脚本把两边逐个函数并排打出来，
#   每一行**明标**这一格是「纯字符串」还是「问内核」，并撞出三条最容易记住的差别。
#
# 安全：只用标准库；沙箱是当前目录的 ospath-demo/，脚本退出时删；
#   abspath/realpath/absolute/resolve 的结果里含真实当前目录，一律整段换成占位符 <当前目录>。
# ============================================================================
import atexit
import os
import shutil
import sys
from pathlib import Path, PurePath, PureWindowsPath, WindowsPath

WORK = Path("ospath-demo")
S = "data/reports/季度 报告 v2.tar.gz"          # 相对道具路径（不存在）
A = "/data/reports/报告.tar.gz"                 # 绝对道具路径（不存在）
P = Path(S)
CWD = os.getcwd()


def mask(value):
    """屏幕规矩：不打印真实绝对路径。把结果里的当前目录整段换成占位符 <当前目录>。"""
    return str(value).replace(CWD, "<当前目录>")


def show(v):
    """把两边的答案统一成可比较的字符串：Path 打它的字符串形式，其余打 repr。"""
    return repr(str(v)) if isinstance(v, PurePath) else repr(v)


def cleanup():
    if WORK.exists():
        shutil.rmtree(WORK)
    print(f"  清理自证：{WORK.name}/ 已删除，现在它还在吗 → {WORK.exists()}")


atexit.register(cleanup)


print("=== 1. 先认两套 API 的真身：一个是模块，一个是类 ===")
print(f"  os.path 其实是 → {type(os.path).__name__} 模块 {os.path.__name__}（本机 POSIX 挑到的实现）；"
      f"在 Windows 上它会被换成 ntpath")
print(f"  pathlib 的 Path → {Path.__module__}.{Path.__name__}（工厂函数，实际返回 {type(P).__name__}）")
print(f"  两边的入口类型：os.path.split(str) → {type(os.path.split(S)).__name__} 里装 {type(os.path.split(S)[0]).__name__}；"
      f"Path(s).parent → {type(P.parent).__name__}")
print(f"  两支的父子关系：issubclass(Path, PurePath) → {issubclass(Path, PurePath)} ｜ "
      f"Path 的 MRO：{' → '.join(c.__name__ for c in Path.__mro__[:4])}")
print(f"  收 Path 时 os.path 靠什么活：os.path.basename(P) → {os.path.basename(P)!r}（{type(os.path.basename(P)).__name__}）")
print("  → 判据：os.path 的每个函数都是「字符串进、字符串出」，它不需要你懂类型，")
print("    代价是返回值之间没有结构，dirname 和 basename 拼回去要你自己加斜杠。")

print("\n=== 2. 拆名字：split / dirname / basename ↔ parent / name / parts（全标纯字符串）===")
print("  表头：函数（同一批输入）→ os.path 的答案 ‖ pathlib 的写法 → 答案 ｜ 这一格归谁")
rows = [
    ("os.path.split(s)", os.path.split(S), "p.parent, p.name", (str(P.parent), P.name)),
    ("os.path.dirname(s)", os.path.dirname(S), "str(p.parent)", P.parent),
    ("os.path.basename(s)", os.path.basename(S), "p.name", P.name),
    ("os.path.split('a/b/')", os.path.split("a/b/"), "Path('a/b/').name", Path("a/b/").name),
    ("os.path.basename('a')", os.path.basename("a"), "Path('a').parent", Path("a").parent),
    ("os.path.splitdrive(s)", os.path.splitdrive(S), "p.drive, p.root", (P.drive, P.root)),
    ("（os.path 里没有）", "只能自己 split('/')", "p.parts", P.parts),
    ("（os.path 里没有）", "只能自己切后缀", "p.suffixes", P.suffixes),
]
for olabel, oval, plabel, pval in rows:
    print(f"    {olabel:<24}→ {show(oval):<38} ‖ {plabel:<20}→ {show(pval)}   ｜ 纯字符串")
print(f"  尾斜杠那条值得多看一眼：os.path 眼里 'a/b/' 的 basename 是 {os.path.basename('a/b/')!r}（空串），")
print(f"    pathlib 眼里 Path('a/b/') == Path('a/b') 是 {Path('a/b/') == Path('a/b')} —— 它先把尾斜杠吃掉再比较。")
print(f"    而单文件名 'a'：dirname 给你 {os.path.dirname('a')!r}（空串），Path('a').parent 给你 {str(Path('a').parent)!r}（当前目录）")
print("  → 判据：这两套「空值」语义不同。os.path 用空串表示「没有父目录」，")
print("    pathlib 用 Path('.') 表示；把 dirname 的结果直接喂给下一层 join 之前，得想清楚是哪一种。")

print("\n=== 3. ★join：绝对片段会把前面整段吞掉，两个 API 一样狠 ===")
print(f"  os.path.join('/a', '/b')        → {os.path.join('/a', '/b')!r}   ← '/a' 被丢掉，不是拼成 '/a/b'")
print(f"  str(Path('/a') / '/b')          → {str(Path('/a') / '/b')!r}   ← pathlib 一模一样")
print(f"  对比正常片段：os.path.join('/a', 'b') → {os.path.join('/a', 'b')!r} ｜ Path('/a') / 'b' → {str(Path('/a') / 'b')!r}")
print("  ★这就是「把用户给的路径 join 进自己的目录」时的越界写法：只要那一段以 / 开头，")
print("    你的前缀当场作废。两个 API 都不挡这条路，所以必须自己先拒绝绝对路径（第 10 节 p.is_absolute()）。")
print(f"  中间给空串：os.path.join('a', '', 'b') → {os.path.join('a', '', 'b')!r} ｜ "
      f"Path('a') / '' / 'b' → {str(Path('a') / '' / 'b')!r}")
print("  join 收 None 的原文：")
try:
    os.path.join("/a", None)
    print(f"    ★没响，算出了 {os.path.join('/a', None)!r}")
except TypeError as exc:
    print(f"    os.path.join('/a', None) → {type(exc).__name__}: {exc}")
try:
    Path("/a") / None
    print("    ★没响")
except TypeError as exc:
    print(f"    Path('/a') / None      → {type(exc).__name__}: {exc}")
print("  → 判据：两边的报错措辞不同，但都是 TypeError 不是 ValueError —— 传错类型就是一次编程错误，")
print("    别用 except Exception 把它和「文件不存在」混成一锅（呼应第 07 章那棵类树）。")

print("\n=== 4. splitext ↔ suffix / suffixes：隐藏文件和多点后缀两边答案不同 ===")
print(f"  {'输入':<18}{'os.path.splitext':<26}{'Path.suffix':<16}{'Path.suffixes':<20}标签")
for name in (".bashrc", "报告.tar.gz", "a.", "a.b/c", "no_ext", "..bashrc.txt"):
    sp = os.path.splitext(name)
    print(f"    {name!r:<16}{sp!r:<30}{Path(name).suffix!r:<16}{str(Path(name).suffixes):<22}纯字符串")
print(f"  ★隐藏文件那条：os.path.splitext('.bashrc') → {os.path.splitext('.bashrc')!r} —— 它认为这个文件**没后缀**")
print(f"    （第一个点算名字的一部分），而 Path('.bashrc').suffix → {Path('.bashrc').suffix!r}、"
      f".name → {Path('.bashrc').name!r} —— 两边答案一致，只是含义不同")
print(f"  多点后缀：splitext 只切最后一个点（{os.path.splitext('报告.tar.gz')!r}），")
print(f"    pathlib 的 suffix 同样只给最后一个（{Path('报告.tar.gz').suffix!r}），但 suffixes 把整串列出来（{Path('报告.tar.gz').suffixes}）")
print("  → 判据：想按后缀分文件，用 Path.suffixes 或先自己 rsplit('.')，别拿 splitext 的判断当「扩展名」。")

print("\n=== 5. normpath：一个折叠一个不折叠；PurePath 还能在非本平台上算 ===")
messy = "a//b/./c/../d"
print(f"  输入 {messy!r}")
print(f"    os.path.normpath → {os.path.normpath(messy)!r}   ← 折叠：// 和 ./ 消失，c/.. 抵消   ｜ 纯字符串")
print(f"    Path(s).parts    → {Path(messy).parts}   ← 不折叠 '..'（但 // 和 ./ 被切段时消化了）｜ 纯字符串")
print(f"    str(Path(s))     → {str(Path(messy))!r}   ← pathlib 有自己的归一化，和 normpath 不等价    ｜ 纯字符串")
print(f"    os.path.abspath  → {mask(os.path.abspath(messy))!r}   ← 折叠 + 拼 cwd（<当前目录> 是占位符）｜ 要一次 getcwd，不查这个路径")
print(f"    Path.absolute()  → {mask(str(Path(messy).absolute()))!r}   ← 只拼 cwd，**不折叠** '..'      ｜ 同上")
print(f"    os.path.realpath → {mask(os.path.realpath(messy))!r}   ← 折叠 + 解符号链接      ｜ 问内核")
print(f"    Path.resolve()   → {mask(str(Path(messy).resolve()))!r}   ← 和 realpath 同一条路   ｜ 问内核")
print(f"  证明上面 mask 过的那几行确实以 cwd 开头（否则替换就是假的）→ "
      f"{os.path.abspath(messy).startswith(CWD)}；resolve 和 absolute 的区别一个布尔："
      f"{Path(messy).absolute() != Path(messy).resolve()}")
pw = PureWindowsPath("C:/x/y.txt")
print("  跨平台算路径（本机是 Linux，一个字节的 Windows 文件系统都没碰）：")
print(f"    PureWindowsPath('C:/x/y.txt').parts → {pw.parts} ｜ drive → {pw.drive!r} ｜ name → {pw.name!r}   ｜ 纯字符串")
print(f"    同一个字符串交给 PosixPath：{Path('C:/x/y.txt').parts}（盘符被当成一段目录名）")
print(f"  PurePath 和 Path 的差别只在有没有 IO 方法：PureWindowsPath 没有 exists() → "
      f"{hasattr(pw, 'exists')}；Path 有 → {hasattr(P, 'exists')}")
print(f"  而带 IO 的那一支**不许你在错的平台上造**（这是本机原文，注意异常类型不是 OSError）：")
try:
    print(f"    ★没响，WindowsPath('C:/x') → {WindowsPath('C:/x')!r}")
except Exception as exc:
    print(f"    WindowsPath('C:/x') → {type(exc).__name__}: {exc}")
print("  → 判据：算「另一种平台」的路径就用 Pure*Path —— 它不碰内核，所以既不需要那台机器存在，")
print("    也不会因为「这机器上没有」而报错；想对它做 IO，解释器先拦你。")

print("\n=== 6. 问内核那一组：exists / isfile / isdir / getsize ↔ is_file / is_dir / stat ===")
WORK.mkdir()                                    # 本脚本唯一的真目录（第 6、7 段用）
(WORK / "真文件.txt").write_text("0123456789", encoding="utf-8")
real = WORK / "真文件.txt"
print("  表头：函数 → os.path 的答案 ‖ pathlib 的写法 → 答案 ｜ 这一格归谁")
pairs = [
    ("os.path.exists(p)", os.path.exists(real), "p.exists()", real.exists()),
    ("os.path.isfile(p)", os.path.isfile(real), "p.is_file()", real.is_file()),
    ("os.path.isdir(p)", os.path.isdir(real), "p.is_dir()", real.is_dir()),
    ("os.path.isdir(目录)", os.path.isdir(WORK), "p.is_dir()", WORK.is_dir()),
    ("os.path.getsize(p)", os.path.getsize(real), "p.stat().st_size", real.stat().st_size),
    ("os.path.islink(p)", os.path.islink(real), "p.is_symlink()", real.is_symlink()),
    ("os.path.exists(s)", os.path.exists(S), "P.exists()", P.exists()),
]
for olabel, oval, plabel, pval in pairs:
    print(f"    {olabel:<19}→ {oval!r:<12} ‖ {plabel:<20}→ {pval!r}   ｜ 问内核")
print(f"  getsize 和 stat().st_size 是同一个数的两条路 → {os.path.getsize(real) == real.stat().st_size}"
      f"（getsize 内部就是 os.stat 那一次调用，只多包了一层取 st_size）")
print("  对不存在的文件，两边的原文（一个字都不改）：")
try:
    os.path.getsize(S)
except OSError as exc:
    print(f"    os.path.getsize(s) → {type(exc).__name__}: {exc}")
try:
    P.stat()
except OSError as exc:
    print(f"    Path.stat()        → {type(exc).__name__}: {exc}")
print("  → 判据：这一组里 os.path 和 pathlib 是同一个 syscall 的两层皮，报错也一样 ——")
print("    所以「exists 换 is_file」不会让程序更稳，只是换个写法（别为改而改）。")

print("\n=== 7. relpath / isabs：一个不响一个响，这是最容易被换错的一对 ===")
print(f"  输入：完整路径 {str(real)!r}（相对沙箱），基准 {str(WORK)!r}")
print(f"    os.path.relpath(p, start) → {os.path.relpath(str(real), str(WORK))!r}   ｜ 纯字符串（但要一次 getcwd 当默认基准）")
print(f"    p.relative_to(基准)       → {str(real.relative_to(WORK))!r}   ｜ 纯字符串，返回 {type(real.relative_to(WORK)).__name__}")
print(f"    两者相等吗 → {os.path.relpath(str(real), str(WORK)) == str(real.relative_to(WORK))}")
print("  ★换个不相关的基准，两边立刻分道扬镳：")
try:
    print(f"    os.path.relpath(p, 'other') → {os.path.relpath(str(real), 'other')!r}   ← 它不响，给你一串 ../")
except OSError as exc:
    print(f"    os.path.relpath → {type(exc).__name__}: {exc}")
try:
    print(f"    p.relative_to('other') → ★没响，{str(real.relative_to('other'))!r}")
except ValueError as exc:
    print(f"    p.relative_to('other')  → ValueError: {exc}   ← 它只承认「真的在我里面」")
print(f"  os.path.isabs(s) → {os.path.isabs(S)} / os.path.isabs(A) → {os.path.isabs(A)} ｜ "
      f"P.is_absolute() → {P.is_absolute()} / Path(A).is_absolute() → {Path(A).is_absolute()}   ｜ 纯字符串")
print("  → 判据：relpath 是「算出怎么走过去」（可能全是 ../，也可能是错的），relative_to 是「断言它在我下面」。")
print("    拿 relpath 的结果去当「安全前缀校验」是漏洞；要用 relative_to / is_relative_to（不响的那个）。")

print("\n=== 8. 收口：什么时候值得用 Path，以及 os.fspath(p) 与 str(p) 的区别 ===")
print("  判据一（老代码）：os.path 那套全是字符串函数，返回值是 str，和 open()/subprocess/日志已经")
print("    配合了二十年；把它整体换成 pathlib 只是把同一件事写得更长，还会引入「哪里漏了 str()」的新 bug。")
print("  判据二（新代码）：需要**拆名字、改后缀、拼片段**时 Path 少转字符串 —— 第 2、3、4 段那几张表里")
print("    pathlib 一列的中间值全是 Path，而 os.path 一列每步都在吐 str，字符串拼接的责任回到你身上。")
print(f"  混用靠 __fspath__：两边结果相等吗 → {os.fspath(P) == str(P)}（{type(os.fspath(P)).__name__} vs {type(str(P)).__name__}）")
print("  但它们的**容错范围**差一大截，这是收口真正要留的那一行：")
for bad in (None, 3, ["a", "b"], Path("a/b")):
    try:
        verdict = f"os.fspath → {os.fspath(bad)!r}"
    except TypeError as exc:
        verdict = f"os.fspath → {type(exc).__name__}: {exc}"
    print(f"    输入 {bad!r:<18}｜ {verdict}")
    print(f"    （同一个输入 str() → {str(bad)!r}，永远不响）")
print("  → 判据：写「收一个路径参数」的函数，入口第一件事是 `p = os.fspath(p)`，别写 `str(p)` ——")
print("    后者会把 None 变成 'None'、把 list 变成 \"['a', 'b']\"，一路带到 open() 才炸，报错还看不出是谁传错的。")
print(f"  ★一句话记住这一节：os.path 是「字符串的几何」，pathlib 是「带着类型的路径」，")
print("    而它们唯一共享的地基是那一组 syscall —— exists/isfile/getsize 在哪边都是问内核，")
print("    parts/splitext/join 在哪边都是纯字符串。分清了这两层，换不换 API 都只是风格问题。")
print(f"  本机 Python {sys.version.split()[0]}；屏幕上没有真实绝对路径，"
      "<当前目录> 是 os.getcwd() 的占位符、不是本机输出。")
