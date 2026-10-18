#!/usr/bin/env python3
# ============================================================================
# 取证脚本 i_mkdir_unlink.py —— 第 08 章第 10 节：幂等建目录、缺失不删炸，以及权限位怎么挡你
#
# 这个脚本要证明三件事（全部本机实测，不猜）：
#   ① mkdir() 和 unlink() 的默认语义都是「不幂等」：撞已存在就抛、删不存在的也抛。幂等要么靠
#      exist_ok / missing_ok 两个开关，要么靠 except 接住 —— 而 exist_ok 只放过「已经是目录」。
#   ② mode= 参数打不过 umask：os.mkdir 内部是 mode & ~umask，想强制权限必须再 chmod 一次。
#   ③ ★能不能删掉一个文件，取决于**父目录**的写权限，跟文件自己的 mode 一点关系都没有。
#
# 安全：只用标准库；所有东西都建在当前目录的 mkdir-demo/ 里；umask 一开头钉成 0o022（否则下面的
#   八进制数跟着各人的 shell 变）；所有 chmod 过的目录在退出前先全部放可写、再 rmtree —— 不然就是
#   给自己造一个删不掉的垃圾堆。屏幕上只有相对路径。
# ============================================================================
import atexit
import errno
import inspect
import os
import shutil
import stat
import sys
from pathlib import Path

WORK = Path("mkdir-demo")
OLD_UMASK = os.umask(0o022)        # 钉住 umask；进来之前的值只在第 2 段报一次，不设任何结论


def mode_of(p):
    """只打权限那 12 位里的低 9 位（八进制），别的位（setuid 之类）这里不碰。"""
    return oct(stat.S_IMODE(os.stat(p).st_mode))


def fails(label, act):
    """打一次「故意失败」的原文 + errno + 类树。它要是没响，屏幕上就出现 ★（= 本节结论作废）。"""
    try:
        return f"{label:<26} → ★没响，返回了 {act()!r}"
    except Exception as exc:
        code = (f"｜errno {exc.errno} = {errno.errorcode.get(exc.errno)}（{exc.strerror}）"
                if isinstance(exc, OSError) else "")
        tree = " → ".join(c.__name__ for c in type(exc).__mro__[:3])
        return (f"{label:<26} → {type(exc).__name__}: {exc}{code}"
                f"\n{'':<26}     类树 {tree}")


def cleanup():
    """退出前的固定动作：先把沙箱里每个目录放成可写，再整份删。"""
    if WORK.exists():
        for root, _dirs, _files in os.walk(WORK):     # os.walk 先给父再往下走，所以边走边 chmod 够用
            os.chmod(root, 0o700)
        shutil.rmtree(WORK)
    os.umask(OLD_UMASK)                               # 把进程自己的 umask 还回去
    print(f"  清理自证：{WORK.name}/ 已删除，现在它还在吗 → {WORK.exists()}"
          f" ｜ umask 已还回 {oct(OLD_UMASK)}")


atexit.register(cleanup)

print("=== 1. mkdir：默认不幂等，三层靠 parents，缺父级当场炸 ===")
WORK.mkdir()                                          # 沙箱本体，后面每一段都往里放东西
one = WORK / "一层"
one.mkdir()
print("  " + fails("已存在时再 one.mkdir()", one.mkdir))
one.mkdir(exist_ok=True)                              # 静默成功：走到下一行说明它没抛
print(f"  {'mkdir(exist_ok=True)':<26} → 静默成功（没抛，{one.name}/ 还在 → {one.exists()}）")
three = WORK / "甲" / "乙" / "丙"
three.mkdir(parents=True)
print(f"  {'mkdir(parents=True) 三层':<26} → 建成：{str(three)!r}（{len(three.parts)} 段路径，一次调用）")
deep = WORK / "单" / "层" / "深"
print("  " + fails("不带 parents 建同样三层", deep.mkdir))
print("  → 判据：parents=True 就是「缺哪层补哪层」，内部是对自己 parent 递归调用；下面是 pathlib 真源码：")
print("      " + " ".join(l.strip() for l in inspect.getsource(Path.mkdir).splitlines()
                          if "os.mkdir" in l or "self.parent.mkdir" in l))
print("  exist_ok=True 会不会顺手放过一个同名**文件**？（先把这个名字占上）")
occupier = WORK / "一个文件.txt"
occupier.write_text("我占着这个名字", encoding="utf-8")
print("  " + fails("文件已存在时 mkdir()", occupier.mkdir))
print("  " + fails("文件已存在时 mkdir(exist_ok=True)", lambda: occupier.mkdir(exist_ok=True)))
print("    → 判据：exist_ok 的判据是「已经是个目录」（源码里那句 not self.is_dir()），不是「名字被占了」。")

print("\n=== 2. ★mode= 打不过 umask：本机 0o777 建出来的目录照样是 0o755 ===")
print(f"  进脚本时的 umask = {oct(OLD_UMASK)}（各人 shell 不同），脚本已把它钉成 0o022 —— 下面这几个八进制数才敢写进教材")
plain = WORK / "默认位"
plain.mkdir()
print(f"  {'mkdir()':<26} → {mode_of(plain)}")
give777 = WORK / "给了777"
give777.mkdir(mode=0o777)
print(f"  {'mkdir(mode=0o777)':<26} → {mode_of(give777)}   ← 和上一行一模一样：777 里那两个写位被 umask 减掉了")
give700 = WORK / "给了700"
give700.mkdir(mode=0o700)
print(f"  {'mkdir(mode=0o700)':<26} → {mode_of(give700)}   ← 这条守住了：umask 0o022 只减组和其他的写位，不碰 owner")
afile = WORK / "默认文件位.txt"
afile.write_text("x", encoding="utf-8")
print(f"  {'write_text 新建文件':<26} → {mode_of(afile)}   ← 文件模板 0o666、目录模板 0o777，都要再减一次 umask")
os.chmod(give777, 0o777)
print(f"  {'mkdir(mode=0o777) 之后再 chmod':<26} → {mode_of(give777)}   ← 想要 777 只有这条路")
print("  → 判据：mode= 是「你想要的位」，umask 是「一定要减的位」，两者是**与非**关系（mode & ~umask）。")
print("  ★所以「脚本建出来的目录别人写不了」这类问题，根因常常在 umask（cron、systemd、Docker 构建层里")
print("    umask 常常是 0o077 或 0），不在 mkdir 那一行 —— 要么显式 chmod，要么别依赖别人能写。")

print("\n=== 3. parents=True 撞在一个同名普通文件上：本机答案不是 FileExistsError ===")
blocker = WORK / "挡住.txt"
blocker.write_text("我不是目录", encoding="utf-8")
behind = blocker / "子" / "孙"
print(f"  现场：{str(blocker)!r} 是一个普通文件，要建 {str(behind)!r}（三个 errno 各是一条不同的路）")
print("  " + fails("blocker.mkdir()（名字被占）", blocker.mkdir))
print("  " + fails("behind.mkdir()（不带 parents）", behind.mkdir))
print("  " + fails("behind.mkdir(parents=True)", lambda: behind.mkdir(parents=True)))
print("  → 判据：EEXIST(17) 说「这个名字已经有东西了」，ENOTDIR(20) 说「中间有个不是目录的东西」；")
print("    上面三行的类树里没有一条是另一条的儿子 —— FileExistsError 和 NotADirectoryError 是")
print("    OSError 的两个平行儿子。只知道「mkdir 会抛 OSError」就写 except OSError: pass，等于把")
print("    「改个名」和「先删掉占位的文件」两种完全不同的处置，压成了一次静默失败。")

print("\n=== 4. 删：unlink 三格、rmdir 两格，外加三个名字是不是同一件事 ===")
bye = WORK / "删我.txt"
bye.write_text("bye", encoding="utf-8")
before, ret = bye.exists(), bye.unlink()
print(f"  {'正常 unlink()':<26} → 之前 exists {before} ｜ 之后 exists {bye.exists()}"
      f" ｜ 返回值 = {ret!r}（别拿返回值判断，要么看 exists 要么接异常）")
print("  " + fails("对不存在的 unlink()", (WORK / "没这个.txt").unlink))
(WORK / "没这个.txt").unlink(missing_ok=True)          # 静默：走到下一行就是没抛
print(f"  {'unlink(missing_ok=True)':<26} → 静默成功，而且没有顺手把文件建出来 → "
      f"{(WORK / '没这个.txt').exists()}")
nonempty = WORK / "非空"
nonempty.mkdir()
(nonempty / "里面有东西.txt").write_text("x", encoding="utf-8")
print("  " + fails("对非空目录 rmdir()", nonempty.rmdir))
print("  " + fails("对不存在的目录 rmdir()", (WORK / "根本没这个目录").rmdir))
print("  → 判据：rmdir 没有 exist_ok、也没有 force —— POSIX 的 rmdir(2) 本来就只允许删空目录。")
print("    想递归删是 shutil.rmtree，那是另一个决定（它会一路删到你给的这个名字），本章第 12 节讲。")
print("  三个名字是不是同一件事（本机实测，跟很多人以为的不一样）：")
print(f"    os.unlink is os.remove → {os.unlink is os.remove}   ← ★False！两个是**不同的 builtin 对象**；")
print(f"      但 doc 互相指认：{os.remove.__doc__.splitlines()[0]!r} / {os.unlink.__doc__.splitlines()[0]!r}")
print(f"    Path.unlink 的源码里那一行：{' '.join(l.strip() for l in inspect.getsource(Path.unlink).splitlines() if 'os.unlink' in l)}")
for name, fn in (("os.unlink", os.unlink), ("os.remove", os.remove)):
    victim = WORK / f"由{name}删.txt"
    victim.write_text("x", encoding="utf-8")
    fn(victim)
    print(f"    行为实测 {name}(...) → 文件还在吗 {victim.exists()}   ← 两条路都是 unlink(2)，结果必然相同")
print("  → 判据：is 比的是**对象身份**，不是「做的事」；想知道是不是同一件事，看 errno 和效果，别看 is。")

print("\n=== 5. ★本章最反直觉：删文件看的是父目录的写权限，不是文件自己的 ===")
locked = WORK / "locked"
locked.mkdir()
secret = locked / "secret.txt"
secret.write_text("密", encoding="utf-8")
os.chmod(secret, 0o444)
print(f"  {secret.name} 自己 chmod 成 {mode_of(secret)}（只读，任何写都不许）｜父目录 {locked.name}/ 是 {mode_of(locked)}")
secret.unlink()
print(f"  删得掉吗 → {not secret.exists()}   ← 删的是**目录里那条名字映射**，不是文件的字节")
print("  把这条钉死：同一个不可写的目录里放三个不同 mode 的文件，一次全撞：")
victims = []
for bits in (0o644, 0o777, 0o000):
    v = locked / f"位{oct(bits)[2:]}.txt"
    v.write_text("x", encoding="utf-8")               # 必须在 chmod 之前建：目录不可写就建不出来
    os.chmod(v, bits)
    victims.append((bits, v))
os.chmod(locked, 0o500)                               # r-x：能列能进，唯独不能改目录项
print(f"    现在 {locked.name}/ 是 {mode_of(locked)}（文件自己的位见左列）：")
for bits, v in victims:
    print("    " + fails(f"unlink 位{oct(bits)[2:]}.txt", v.unlink))
os.chmod(locked, 0o700)                               # 加回写位：删之前必须改回来，这是本节的硬规矩
for _bits, v in victims:
    v.unlink()
print(f"    目录 chmod 回 {oct(0o700)} 之后三个全删掉 → {not any(v.exists() for _b, v in victims)}")
harder = WORK / "看不见"
harder.mkdir()
inside = harder / "有名字.txt"
inside.write_text("x", encoding="utf-8")
os.chmod(harder, 0o300)                               # 可写可进、不可列：反过来的那条也成立
print(f"    {harder.name}/ 改成 {mode_of(harder)} 之后：列目录当场响，按已知名字却删得掉：")
print("      " + fails("os.listdir(harder)", lambda: os.listdir(harder)))
blind = ""
try:
    inside.unlink()
except OSError as exc:
    blind = f"★居然抛了 {type(exc).__name__}: {exc}"
print(f"      inside.unlink()            → {blind or f'没抛（还在吗 → {inside.exists()}）'}")
os.chmod(harder, 0o700)
print("      → 读位（r）管「能不能列出名字」，写位（w）管「能不能改目录项」，两条互不相干")
print("  → 判据：Unix 上「删除」是对**目录**的写操作，所以 chmod 0o444 保护的是内容不被改，")
print("    它保护不了「这个文件还存在」；要防删得动父目录，或者用只读文件系统/不可写挂载点。")

print("\n=== 6. open() 的三种目录相关失败：三条原文、三个 errno、各自是谁的儿子 ===")
middle = WORK / "中间是文件.txt"
middle.write_text("x", encoding="utf-8")
for label, act in (
    ("open(一个目录)", lambda: open(WORK, "r")),
    ("open(路径中间是文件)", lambda: open(middle / "子" / "x.txt", "w")),
    ("open(父目录不存在)", lambda: open(WORK / "没这个目录" / "x.txt", "w")),
):
    print("  " + fails(label, act))
print("  → 判据：三个 errno 各不相同，但三个类都是 OSError 的儿子，而且互相不是父子 ——")
print("    所以 except FileNotFoundError 接不住 IsADirectoryError，写成 except OSError 又太宽。")
print("  ★「路径中间是文件」（ENOTDIR 20）和「父目录不存在」（ENOENT 2）是同一个位置上的两种失败：")
print("    看到 ENOENT 就下结论「文件被删了」，很可能其实是上游某个目录变成了文件。这三个都发生在")
print("    open() 那一刻而不是读的那一刻 —— 打开阶段的错和读取阶段的错（编码），except 别写在一起。")

print("\n=== 7. 收口：幂等三件套，和「按 errno 分派」这条第 07 章的规矩 ===")
print("  → 幂等三件套：① 建目录 mkdir(parents=True, exist_ok=True)；② 删文件 unlink(missing_ok=True)；")
print("    ③ 删目录只有 rmdir()（非空必响）。三件套的共同点是「已经是我想要的样子」算成功，不算异常；")
print("    重跑同一个部署脚本不炸，靠的就是这个语义。")
print("  → 判据：「先 if exists 再动手」看着稳，其实三处不如直接 try：① 查完到动手之间有一次竞态；")
print("    ② exists() 会把权限问题报成 False（第 8 节第 3 段现场），你的 if 分支会走错；")
print("    ③ 多一条分支就多一条没被测过的路。")
print("  ★按 errno 分派（errno 是整数，不随本机 strerror 的语言变；异常类名也不如它精确）：")
mapping = [
    (errno.ENOENT, "路径里少一层 / 文件不存在", "跳过，或按「本来就不该有」处理"),
    (errno.EEXIST, "这个名字已被占（可能是目录也可能是文件）", "换名，或先看清占着的是什么"),
    (errno.ENOTDIR, "路径中间有个不是目录的东西", "多半是上游被文件挡住了"),
    (errno.ENOTEMPTY, "rmdir 一个非空目录", "要么先清空，要么改用 rmtree（另一个决定）"),
    (errno.EACCES, "权限位挡的（第 5 段那四种全是它）", "要改的是父目录，不是文件"),
    (errno.EISDIR, "把目录当普通文件 open", "路径拼错了，回去查 parts"),
]
for code, when, what in mapping:
    print(f"    {code} = {errno.errorcode[code]:<10}｜{when:<34}→ {what}")
print("  → 呼应第 07 章：except 匹配的是 isinstance，抓父亲就抓到全部儿子。所以「不存在就跳过、")
print("    没权限就报警」要写成 except OSError + 比 exc.errno，而不是 except FileNotFoundError,")
print("    PermissionError —— 前者精确到数字，后者连 IsADirectoryError 都会漏掉。")
print(f"  本机 Python {sys.version.split()[0]} ｜ umask 钉在 0o022 ｜ 屏幕上只有相对路径；"
      "所有 chmod 过的目录退出前都放回可写再整份删除。")
