#!/usr/bin/env python3
# ============================================================================
# 取证脚本 g_pathlib_basics.py —— 第 08 章第 8 节：Path 是对象，不是字符串
#
# 这个脚本要证明什么：
#   pathlib 的每个调用只属于两类，界线画得干干净净：
#     A 类「纯算」——在字符串上拆分/拼接，路径存不存在它根本不知道，永远不会抛 IO 异常；
#     B 类「问内核」——真去做一次 stat/open/unlink，路径不存在就抛 OSError 那一族。
#   绝大多数 pathlib 困惑（「Path 会不会报错」「/ 是除法吗」「~ 要不要自己展开」）
#   都是把 A 类当成 B 类，或者反过来。下面逐方法把这条线画出来，全部本机实测。
#   道具路径（/no/such/dir/... 那一类）全程不落盘 —— 这就是「纯算不需要存在」的证据。
#
# 安全：只用标准库；沙箱是当前目录的 pathlib-demo/，atexit 删；屏幕上只有假道具路径和相对路径，
#   真实家目录一律整段换成占位符 <家目录>。
# ============================================================================
import atexit
import inspect
import os
import shutil
import sys
from pathlib import Path

WORK = Path("pathlib-demo")
FAKE = Path("/no/such/dir/报告 v2.tar.gz")      # 道具：本机根本不存在，用来演「纯算不需要存在」
BASE = Path("data/reports")                     # 道具：拼接和变形都用它，同样不存在


def cleanup():
    """退出前把沙箱删掉。放进 atexit，是为了让「中途崩了也不留垃圾」这条也成立。"""
    if WORK.exists():
        shutil.rmtree(WORK)
    print(f"  清理自证：{WORK.name}/ 已删除，现在它还在吗 → {WORK.exists()}")


atexit.register(cleanup)


def fails(label, act, kind=Exception):
    """打一次「故意失败」的原文。它要是没响，屏幕上就出现 ★ —— 那说明本脚本的结论作废。"""
    try:
        return f"{label:<26} → ★没响，算出了 {str(act())!r}"
    except kind as exc:
        return f"{label:<26} → {type(exc).__name__}: {exc}"


def pure_answers(path):
    """把 A 类（纯算）的十个答案收成一个元组，第 7 段用它证明「落盘前后一字不变」。"""
    return (path.parts, path.anchor, path.drive, path.root, path.name, path.stem,
            path.suffix, tuple(path.suffixes), str(path.parent), path.is_absolute())


print("=== 1. 先认清手里的东西是什么类型（它不是 str）===")
print(f"  type(FAKE) → {type(FAKE).__name__} ｜ MRO：",
      " → ".join(c.__name__ for c in type(FAKE).__mro__))
print(f"  isinstance(FAKE, str) → {isinstance(FAKE, str)} ｜ type(FAKE) is str → {type(FAKE) is str}")
print("  → 判据：类树分两支 —— PurePath 一支只做字符串活，Path 一支多出 exists/stat/mkdir 那些问内核")
print(f"    的方法。本机是 POSIX，实现类是 {type(FAKE).__name__}；同一份代码在 Windows 上拿到的是 WindowsPath。")

print("\n=== 2. ★道具：一个本机根本不存在的路径，十个属性一次全算出来 ===")
print(f"  道具路径（它不存在，也永远不会被创建）：{str(FAKE)!r}")
seq = FAKE.parents
rows = [
    ("parts", FAKE.parts), ("anchor", FAKE.anchor), ("drive", FAKE.drive), ("root", FAKE.root),
    ("name", FAKE.name), ("stem", FAKE.stem), ("suffix", FAKE.suffix), ("suffixes", FAKE.suffixes),
    ("parent", FAKE.parent), ("is_absolute()", FAKE.is_absolute()),
]
blew = 0
for label, value in rows:
    try:
        print(f"  {label:<13} → {value!r}")
    except Exception as exc:                    # 这条 except 一次都不该走到；走到就说明结论错了
        blew += 1
        print(f"  {label:<13} → ★抛了 {type(exc).__name__}: {exc}")
print(f"  十项里抛异常的个数 = {blew} ｜ 一个 syscall 都没发起")
print(f"  parents 这个对象   → 类型 {type(seq).__name__}，它是生成器吗 → {inspect.isgenerator(seq)}；"
      f"iter(parents) 才是生成器 → {inspect.isgenerator(iter(seq))}")
print(f"  list(parents)      → {list(seq)}（能下标、能 len 的只读序列，越往右越靠近根）")
print("  ★这就是 pathlib 所有困惑的总根源：上面每一行都是字符串活 ——")
print("    不需要文件存在、不需要目录存在、不需要任何权限，也永远不会因为「不存在」而报错。")
print(f"    两个顺手实测：drive 在 POSIX 上恒为 {FAKE.drive!r}（那是留给 Windows 盘符的位）；"
      f"stem 只去掉**最后一个**后缀，所以 {FAKE.stem!r} 里那个 .tar 还留着。")

print("\n=== 3. 跨过那条线：同一个路径去问内核，三个 False 和一条原文 ===")
for name in ("exists", "is_file", "is_dir"):
    print(f"  {name + '()':<10} → {getattr(FAKE, name)()}")
print("  " + fails("FAKE.stat()", FAKE.stat, OSError))
print(f"  → FileNotFoundError 的父亲是 {FileNotFoundError.__bases__[0].__name__}；"
      "exists/is_file/is_dir 问的也是内核，但它们**把失败咽成 False**，stat 原样抛给你。")
print("  ★同一个「不存在」，两种答法，而且咽掉答案那种会骗人：父目录没「进入」权限时，真实存在的文件也答 False。")
WORK.mkdir()                                    # 本脚本唯一一次建目录，就为了这个反例
blind = WORK / "挡"
blind.mkdir()
target = blind / "在里面.txt"
target.write_text("x", encoding="utf-8")
print(f"    先问一遍：exists() → {target.exists()} ｜ 列这一层能列到它 → {sorted(os.listdir(blind))}")
os.chmod(blind, 0o600)                          # 只给读写、不给「进入」：子项的 stat 从此查不到
print(f"    把 {blind.name}/ 改成 {oct(0o600)} 之后（此刻文件还在，一个字节都没少）：")
print(f"      exists() → {target.exists()} ｜ is_file() → {target.is_file()}   ← 两个都是假的 False")
print("      " + fails("target.stat()", target.stat, OSError))
os.chmod(blind, 0o700)                          # 立刻改回来，不然 atexit 的 rmtree 也删不掉这层
target.unlink()
print("      → 判据：exists() 的 False 只证明「我查不到」，不证明「它不存在」；要分辨这两种情况，")
print("        就去 stat() 并接住 OSError，看 errno 是 2（真没有）还是 13（不让查）。")

print("\n=== 4. 拼接：/ 不是除法，但它也不校验任何东西 ===")
print(f"  {str(BASE)!r} / 'sub' / 'x.txt' → {str(BASE / 'sub' / 'x.txt')!r}（类型 {type(BASE / 'sub').__name__}）")
print(f"  {str(BASE)!r} / Path('a')       → {str(BASE / Path('a'))!r}")
print(f"  {str(BASE)!r} / '/abs/x'        → {str(BASE / '/abs/x')!r}   ← 前面整段被丢掉")
print("    规则：只要某一段是绝对路径（POSIX 上以 / 开头），前面全部作废 —— 和 os.path.join 一模一样。")
print("    ★「路径来自用户输入」时这一步就是越界：用户传一个绝对路径，你拼出来的前缀当场消失。")
print("  " + fails("BASE / 3", lambda: BASE / 3, TypeError)
      + "   ← / 只认 str 或有 __fspath__ 的对象，int 连试都不试")
print(f"  和字符串的 + 摆一起：'data/reports' + 'sub' → {('data/reports' + 'sub')!r}"
      f" ｜ BASE / 'sub' → {str(BASE / 'sub')!r}")
print("  → 判据：str 的 + 只管首尾相接，分隔符是你的责任；Path 的 / 才是「按路径语义接一段」。")

print("\n=== 5. 变形：with_name / with_suffix / with_stem / relative_to / match ===")
item = BASE / "季度 报告 v2.tar.gz"
print(f"  被变形的道具：{str(item)!r}（stem '季度 报告 v2.tar'，suffixes ['.tar', '.gz']）")
for label, value in (
    ("with_name('新名.txt')", item.with_name("新名.txt")),
    ("with_stem('新名')", item.with_stem("新名")),
    ("with_suffix('.md')", item.with_suffix(".md")),
    ("with_suffix('')", item.with_suffix("")),
    ("with_suffix('.tar.gz')", item.with_suffix(".tar.gz")),
):
    print(f"  {label:<22} → {str(value)!r}")
print("  ★「已有后缀时」本机实测两条：with_suffix 只替换**最后一个**后缀（.md 那次 .tar 还留着）；")
print("    传进去的串被整串当成新后缀接在 stem 后面，于是产出 '…v2.tar.tar.gz' —— 它不响，但答案是错的。")
print("  三个会响的，原文照打：")
print("    " + fails("with_suffix('md')（少点了个头）", lambda: item.with_suffix("md"), ValueError))
print("    " + fails("with_name('a/b')（带分隔符）", lambda: item.with_name("a/b"), ValueError))
print("    " + fails("relative_to('/other/root')（不同根）", lambda: BASE.relative_to("/other/root"), ValueError))
sub = Path("data/reports/一季度/a.csv")
print(f"  relative_to 同根时 → {str(sub.relative_to(BASE))!r}（返回 {type(sub.relative_to(BASE)).__name__}，不是 str）")
print(f"  is_relative_to 是它「不响」的兄弟：BASE.is_relative_to('data') → {BASE.is_relative_to('data')}"
      f" ｜ BASE.is_relative_to('other') → {BASE.is_relative_to('other')}")
print("  match 是从**右往左**对齐的 glob 比较（这是它最容易看错的地方）：")
for pat in ("*.gz", "data/*.gz", "reports/*.gz", "*/*/*.gz"):
    print(f"    {pat!r:<16} → {item.match(pat)}")
print("    'data/*.gz' 为什么 False：那个模式要求**父段**叫 data，而实际父段叫 reports。")
print("  → 判据：这一整段全是纯算，一个 syscall 都不发 —— with_suffix 算出来的新路径照样不存在；")
print("    真要把文件改名得用 rename()，那是 B 类（第 10 节）。")

print("\n=== 6. 点、点点、波浪号：纯算阶段谁都不折叠、不展开 ===")
dotty = str(Path(".") / ".." / "x")
print(f"  Path('.') / '..' / 'x' → {dotty!r} ｜ parts → {Path(dotty).parts}（'..' 原样留在 parts 里）")
print(f"  os.path.normpath(同一个字符串) → {os.path.normpath(dotty)!r}   ← 它折叠了")
print(f"  Path('a//b/./c/../d').parts → {Path('a//b/./c/../d').parts} ｜ str() → {str(Path('a//b/./c/../d'))!r}"
      f" ｜ normpath → {os.path.normpath('a//b/./c/../d')!r}")
print("  → pathlib 里没有 normpath：它在字符串层面只认 / 分段，绝不替你判断 '..' 指回哪。")
print("    想折叠只有两条路，而且**语义不等价**：os.path.normpath（还是纯算，只看字符串）或")
print("    p.resolve()（问内核，把真实符号链接一并解开 —— 第 14 节整节在讲这条差别）。")
tilde = Path("~/文档")
print(f"  Path('~/文档') → {str(tilde)!r} ｜ is_absolute() → {tilde.is_absolute()}   ← '~' 只是一个普通名字")
print(f"  它 == Path.home()/'文档' 吗 → {tilde == Path.home() / '文档'}；"
      f"expanduser() 之后 == 吗 → {tilde.expanduser() == Path.home() / '文档'}")
print("  展开结果含真实家目录，所以屏幕上整段换成占位符（<家目录> 是占位符，不是本机输出）：")
print(f"    {str(tilde.expanduser()).replace(str(Path.home()), '<家目录>')!r}")
print("  → 判据：~ 的展开是一次字符串替换，**必须你自己调 expanduser()**；")
print("    把 '~/x' 直接塞给 open() 只会拿到 FileNotFoundError，因为内核不认识 ~ 这个字符。")

print("\n=== 7. 同一个字符串，落盘前后：纯算一字不变，问内核当场翻转 ===")
live = WORK / "报告 v2.tar.gz"
print(f"  前面所有道具路径都没落盘；现在真写一个：3 个字符「报告x」｜ 落盘前 exists() → {live.exists()}")
before = pure_answers(live)
live.write_text("报告x", encoding="utf-8")
print(f"  落盘后 exists() → {live.exists()} ｜ is_file() → {live.is_file()} ｜ is_dir() → {live.is_dir()}")
print(f"  落盘后 stat() → st_size={live.stat().st_size} 字节（字符数 {len('报告x')}，汉字在 utf-8 下 3 字节一个）")
print(f"  A 类那一组（10 项）变了吗 → {'没变，逐项相等' if before == pure_answers(live) else '★变了，结论作废'}")
live.unlink()
print(f"  删掉之后 exists() → {live.exists()} ｜ A 类那一组还相等吗 → {pure_answers(live) == before}")
print("  ★结论：文件在不在、能不能读，只改变 B 类的答案；A 类从头到尾只看那串字符。")

print("\n=== 8. Path 不能当 str 用，但 os.path.* 收得到它：靠 __fspath__ ===")
print(f"  str(BASE) == BASE → {str(BASE) == BASE} ｜ BASE == 'data/reports' → {BASE == 'data/reports'}")
print("    → 内容一样、类型不一样，== 就是 False；在 dict 的 key 里 Path('a') 和 'a' 是两个不同的坑。")
print(f"  hasattr(BASE, 'startswith') → {hasattr(BASE, 'startswith')}：")
print("    " + fails("BASE.startswith('data')", lambda: BASE.startswith("data"), AttributeError))
print("  可 open() 和 os.path.* 偏偏收得下它：")
print("    " + fails("open(BASE)", lambda: open(BASE).close(), OSError))
joined = os.path.join(BASE, "x.txt")
print(f"  os.path.join(BASE, 'x.txt') → {joined!r} ｜ 返回类型 {type(joined).__name__}（吐回 str）；"
      f"同一段用 pathlib → {type(BASE / 'x.txt').__name__}")
print(f"  os.fspath(BASE) → {type(os.fspath(BASE)).__name__} ｜ str(BASE) → {type(str(BASE)).__name__} ｜ "
      f"两者相等 → {os.fspath(BASE) == str(BASE)}")
print("  → 判据：os.path.* 和 open() 能收 Path，不是因为它们认识 Path，而是因为 Path 实现了 __fspath__，")
print("    接口层当场把它换成字符串 —— 一过 os.path.join 这道口，类型就退回 str 了。")
print("    混用两套 API 时「谁是 str 谁是 Path」要按**边界**记，别按习惯记（第 9 节整节在对照这个）。")

print("\n=== 9. 收口：一张「纯算 / 问内核」判据表 + 三条能带走的话 ===")
groups = [
    ("纯算（字符串活）",
     "parts anchor drive root name stem suffix suffixes parent parents is_absolute as_posix "
     "with_name with_suffix with_stem relative_to is_relative_to match joinpath 和 / 运算符"),
    ("纯算，但读环境（不看这个路径本身）", "expanduser home"),
    ("问内核（会抛 OSError 那一族）",
     "exists is_file is_dir is_symlink stat lstat open read_text write_text read_bytes iterdir glob "
     "rglob walk mkdir rmdir unlink rename replace touch chmod owner group samefile resolve"),
    ("只拿 cwd 拼一下，不碰内容也不折叠", "absolute（对比 resolve：那个才真问内核）"),
]
for label, names in groups:
    print(f"  {label:<26} → {names}")
print("  → 判据一：名字是**名词**的（name/suffix/parts）几乎全是字符串活；动词（stat/open/mkdir/unlink）全是内核活。")
print("  → 判据二：B 类的失败还分两种 —— 吞掉答案的（exists/is_file，只答 True/False）和原样抛出的")
print("    （stat/open/mkdir，errno 就挂在异常对象上）。想知道「为什么不行」只能用后者。")
print("  → 判据三：别在 open() 前面加 exists() 把关。第 3 段那个假 False 已经证明它不可靠，而且查完到")
print("    打开之间还有一次竞态 —— 直接 open、按 errno 分派，这就是第 07 章立的 EAFP。")
print("  ★Path 不是 str，也不是「延迟的字符串」：它是一个带方法的值对象，能比较、能当 dict 的 key，")
print("    但它和 str 之间唯一的合法通道是 __fspath__ / os.fspath。")
print(f"  本机 Python {sys.version.split()[0]}；屏幕上只出现过假道具路径和相对路径，真实绝对路径一个没打。")
