#!/usr/bin/env python3
# ============================================================================
# 取证脚本 j_glob_stat.py —— 第 08 章第 11 讲：遍历、匹配与 stat 的字段
#
# 只证三件事：
#   ① 「把目录里符合某个样子的东西都列出来」这件事，pathlib 和 glob 模块的
#      匹配规矩**不一样**（点文件就是第一条分水岭），而且谁都不保证顺序；
#   ② `*` 不跨越 `/`，想要「任何深度」必须显式写 `**` 或用 rglob；
#   ③ stat 那一堆字段里，「文件多大」和「文件占了多少盘」是两回事，
#      「这是什么类型」要看 st_mode 的比特位而不是扩展名，
#      「是不是同一个文件」要看 (st_dev, st_ino) 而不是路径字符串。
#
# 现场全部在当前目录的 glob-demo/ 里现造现删；权限显式 chmod 成已知值，
# 免得屏幕上的八进制数字跟着外层 umask 抖；时间字段一个都不打（每跑必变）。
# ============================================================================
import glob as globmod          # 标准库那个 glob，和 pathlib 的 glob 不是同一个东西
import os
import shutil
import stat
from pathlib import Path

WORK = Path("glob-demo")        # 自建自删的沙箱；它同时会出现在报错原文里
# 一棵**固定**的小树：三层目录 + 隐藏文件 + 带空格的名字 + 中文名 + 只有大小写不同的后缀
TREE = [
    "a.txt", "两个 词.txt", "数据.csv", ".env", "x.log", "y.LOG",
    "sub/b.txt", "sub/deeper/c.txt",
]


def rel(p):
    """统一打成相对路径字符串——屏幕上不出现绝对路径，也比对得动。"""
    return str(p.relative_to(WORK))


def listing(items):
    """★ 本章所有遍历结果都过一遍 sorted()：不排序的列表没法逐字搬进书里。"""
    return sorted(rel(p) for p in items)


def mode(m):
    return oct(stat.S_IMODE(m))


print("=== 1. 先造一棵固定的树（8 个文件 + 2 个子目录），列一遍确认它真是这个样子 ===")
shutil.rmtree(WORK, ignore_errors=True)
for name in TREE:
    fp = WORK / name
    fp.parent.mkdir(parents=True, exist_ok=True)
    fp.write_text("x", encoding="utf-8")
# 把整棵树的权限钉成已知值。不钉的话，屏幕上每一个八进制数都跟着**外层 shell 的 umask** 抖，
# 这一页明天在 CI 里就对不上；chmod 的对象全是本脚本自己刚建的，不碰别人的东西。
for p in WORK.rglob("*"):
    os.chmod(p, 0o755 if p.is_dir() else 0o644)
print("  写入的文件（sorted 后逐个打一行）：")
for name in sorted(TREE):
    print("   ", name)
print("  子目录:", sorted(p.name for p in WORK.iterdir() if p.is_dir()),
      "｜ 全口径条目数（rglob('*')，含目录） =", len(listing(WORK.rglob("*"))),
      "｜ 纯文件数 =", len([p for p in WORK.rglob("*") if p.is_file()]))

print("\n=== 2. 五种「列一下」一次打全：iterdir / glob('*') / glob('*.txt') / glob('**/*.txt') / rglob('*.txt') ===")
print("  iterdir()          :", listing(WORK.iterdir()))
print("  glob('*')          :", listing(WORK.glob("*")))
print("  glob('*.txt')      :", listing(WORK.glob("*.txt")))
print("  glob('**/*.txt')   :", listing(WORK.glob("**/*.txt")))
print("  rglob('*.txt')     :", listing(WORK.rglob("*.txt")))
print("  → iterdir 和 glob('*') 的结果**一模一样**（都是「这一层的全部条目」，含目录本身）。")
print("  → glob('**/*.txt') 和 rglob('*.txt') 也一模一样：rglob 就是 glob('**/' + pattern)。")
top = listing(WORK.glob("**/*.txt"))
print("  根级的 a.txt 在 '**/*.txt' 结果里吗 →", "a.txt" in top,
      "（本机实测：`**` 匹配**零层或多层**目录，不是「必须往下走一层」）")

print("\n=== 3. ★ 匹配规矩一：点开头文件——两个 glob 在这里是**相反**的 ===")
pg = listing(WORK.glob("*"))
mg = sorted(os.path.relpath(x, WORK) for x in globmod.glob(str(WORK / "*")))
print("  pathlib 的 Path.glob('*')     :", pg, "｜ 里面有 .env 吗 →", ".env" in pg)
print("  glob 模块的 glob.glob('*')    :", mg, "｜ 里面有 .env 吗 →", ".env" in mg)
print("  只写点号才拿得到：Path.glob('.*') →", listing(WORK.glob(".*")),
      "｜ glob.glob('.*') →",
      sorted(os.path.relpath(x, WORK) for x in globmod.glob(str(WORK / ".*"))))
print("  ★ 这条要按本机事实纠正常见说法：「glob 的 `*` 不匹配点文件」只对 **glob 模块**成立；")
print("    pathlib 的 Path.glob 实测**会**把 .env 一起给你。同一个 `*`，两套规矩。")
print("  → 写「扫目录」的代码之前先问清楚用的是哪个 glob：漏文件（glob 模块）和"
      "多文件（pathlib）都是事故，而且方向相反。")

print("\n=== 4. ★ 匹配规矩二：`*` 不跨越 `/`；顺手把大小写也实测了 ===")
print("  glob('*')  条数 =", len(list(WORK.glob("*"))), " ｜ rglob('*')  条数 =", len(list(WORK.rglob("*"))))
print("  glob('*.txt') 条数 =", len(list(WORK.glob("*.txt"))), " ｜ rglob('*.txt') 条数 =", len(list(WORK.rglob("*.txt"))))
print("  glob('*/*.txt') :", listing(WORK.glob("*/*.txt")), " ← 一个 `*` 只吃掉**一层**（sub）")
print("  glob('*/*/*.txt'):", listing(WORK.glob("*/*/*.txt")), " ← 想够到 deeper 就得再写一个 `*`")
print("  → 上面这四行是同一个事实的四种说法：`*` 匹配「一个路径段里的任意字符」，"
      "路径分隔符 `/` 不在它的射程里；跨层只有 `**` 和 rglob 办得到。")
print("  大小写实测（树里放了 x.log 和 y.LOG 两个文件）：")
print("    glob('*.log')                        :", listing(WORK.glob("*.log")))
print("    glob('*.log', case_sensitive=False)  :", listing(WORK.glob("*.log", case_sensitive=False)))
print("    glob('*.LOG')                        :", listing(WORK.glob("*.LOG")))
print("  → 默认 `case_sensitive=None` 跟随平台：本机 Linux/ext4 实测**区分大小写**，"
      "x.log 和 y.LOG 是两个不同的匹配结果。")
print("  ★ 别写「.log 和 .LOG 是同一个后缀」的代码：Linux 上它俩就是两个文件；"
      "真要大小写无关，必须显式传 case_sensitive=False（3.13 起才有这个关键字）。")

print("\n=== 5. os.walk 和 rglob 走同一棵树：条数相等，顺序**不保证** ===")
walk_files = [os.path.relpath(os.path.join(d, f), WORK) for d, _, fs in os.walk(WORK) for f in fs]
rg_files = [rel(p) for p in WORK.rglob("*") if p.is_file()]
print("  os.walk  走到的文件数 =", len(walk_files), " ｜ rglob 走到的文件数 =", len(rg_files))
print("  sorted 之后两份列表相等吗 →", sorted(walk_files) == sorted(rg_files))
print("  sorted(os.walk) :", sorted(walk_files))
print("  rglob('*') 里的目录条目（walk 不进 files）:",
      sorted(rel(p) for p in WORK.rglob("*") if p.is_dir()))
print("  ★ 两边**原始迭代顺序**都不保证：os.walk 靠 readdir、rglob 靠 scandir，"
      "顺序由文件系统内部哈希决定，跟字母序、创建序都没关系。")
print("    所以本脚本刻意不把原始顺序打在屏幕上——打出来就等于在赌「下次还这个顺序」，"
      "这一页明年就错了。要顺序，自己 sorted()。")

print("\n=== 6. stat 字段屏：三种对象并排（普通文件 / 目录 / 符号链接）===")
normal, a_dir = WORK / "a.txt", WORK / "sub"
link = WORK / "link"
os.symlink("a.txt", link)        # 相对符号链接，指向同一目录里的 a.txt
for label, p in (("普通文件", normal), ("目录", a_dir), ("符号链接", link)):
    st, lst = p.stat(), p.lstat()        # stat 跟随链接；lstat 不跟随
    kinds = lambda m: (stat.S_ISREG(m), stat.S_ISDIR(m), stat.S_ISLNK(m))
    # 目录的 st_size 是「目录项自己占了多少字节」，随文件系统内部布局变（本机实测：
    # 同一个 sub 目录，往里多写几条就 60 → 1060），所以目录这一行两个 size 都不打值。
    is_dir = stat.S_ISDIR(st.st_mode)
    size = "None(目录不打)" if is_dir else st.st_size
    lsize = "None(目录不打)" if is_dir else lst.st_size
    print(f"  {label:5}  {rel(p)}")
    print(f"    权限   stat → {mode(st.st_mode):7} filemode → {stat.filemode(st.st_mode)!r}"
          f"   ｜ lstat → {mode(lst.st_mode):7} filemode → {stat.filemode(lst.st_mode)!r}")
    print(f"    类型   stat (REG,DIR,LNK) → {str(kinds(st.st_mode)):20}"
          f" ｜ lstat (REG,DIR,LNK) → {kinds(lst.st_mode)}")
    print(f"    链接数 st_nlink stat={st.st_nlink} ｜ lstat={lst.st_nlink}"
          f"   ｜  大小 st_size stat={size} ｜ lstat={lsize}")
print("  → 符号链接那一行是重点：stat() 跟着链接走，看到的是**目标**"
      "（0o644、st_size=1，就是 a.txt 那个字节）；")
print("    lstat() 不跟，看到的才是链接自己（0o777、lrwxrwxrwx、st_size=5 = 目标名字 'a.txt' 的字节数）。")
print(f"  → 目录的 st_nlink = {a_dir.stat().st_nlink}，不是「谁指着它」，而是 2 + 直接子目录数"
      "（. 和 .. 各算一个）—— 这个字段对目录没有日常意义。")
print("  ★ 目录的 st_size 上面故意没打值：它是「目录项存储占的字节数」，本机实测同一条路径"
      "会因为里面多了几条记录从 60 跳到 1060。任何把它当大小用的代码都是错的。")
print("  ★ 后缀名不能当类型用，现场做三个都叫 *.txt 的东西给你看：")
(WORK / "dir.txt").mkdir()                       # 一个「后缀是 .txt」的目录
os.chmod(WORK / "dir.txt", 0o755)                # 同样钉死，别让 filemode 列跟着 umask 抖
os.symlink("a.txt", WORK / "link.txt")           # 一个「后缀是 .txt」的符号链接
for p in sorted(WORK.glob("*.txt")):
    m = p.lstat().st_mode
    print(f"    {rel(p):9} S_ISREG={stat.S_ISREG(m)!s:5} S_ISDIR={stat.S_ISDIR(m)!s:5} "
          f"S_ISLNK={stat.S_ISLNK(m)!s:5} filemode={stat.filemode(m)!r}")
print("    → 判类型永远用 stat.S_ISREG / S_ISDIR / S_ISLNK 查 st_mode 的比特位，不看后缀。")
st0 = normal.stat()
print("  时间字段本段一个都没打。它们就在那儿（只打名字，不打值）：",
      [f for f in ("st_atime", "st_mtime", "st_ctime", "st_birthtime") if hasattr(st0, f)],
      "—— 读一次文件 atime 就可能变，把值写进书里第二天就对不上。")

print("\n=== 7. st_size 和「占了多少盘」是两个数：现场造一个稀疏文件 ===")
sp = WORK / "sparse.bin"
with open(sp, "wb") as fh:
    fh.seek(1 << 20)              # 游标跳到 1 MiB 处，中间这段一个字节都没写
    fh.write(b"x")
sst = sp.stat()
size, on_disk = sst.st_size, sst.st_blocks * 512
print("  写了几个字节 =", 1, "｜ st_size =", size, "｜ st_blocks*512 =", on_disk)
print("  占盘 / 逻辑大小 =", round(on_disk / size, 5), "｜ 差值 =", size - on_disk, "字节")
print("  注意 st_blocks 的单位是**固定的 512 字节**（POSIX 规定），不是本文件系统的块大小；"
      "本机 statvfs 的 f_frsize =", os.statvfs(str(WORK)).f_frsize, "字节")
print("  → 所以 `du` 和 `ls -l` 能对不上号：ls 报的是 st_size，du 报的是 st_blocks*512。")
print("  ★ 用 st_size 估「这份文件要占多少空间/拷多久」是错的：稀疏文件、"
      "带洞的数据库镜像、`cp --sparse=always` 的产物都能让两个数差几个量级。")

print("\n=== 8. 是不是同一个文件：samefile 和 == 给出相反的答案 ===")
h1, h2 = WORK / "hard1.txt", WORK / "hard2.txt"
h1.write_text("x", encoding="utf-8")
os.link(h1, h2)                   # 硬链接：同一个 inode 的两个名字
p1, p2 = Path(h1), Path(h2)
print("  两个硬链接名   str 相等（p1 == p2）→", p1 == p2, " ｜ p1.samefile(p2) →", p1.samefile(p2))
print("  链接数证明     h1.stat().st_nlink =", h1.stat().st_nlink,
      " ｜ unlink 掉 h2 之后 =", (h2.unlink(), h1.stat().st_nlink)[1])
os.link(h1, h2)
st_a, st_b = h1.stat(), h2.stat()
print("  只打布尔，不打 inode 数值：st_ino 相等 →", st_a.st_ino == st_b.st_ino,
      "｜ st_dev 相等 →", st_a.st_dev == st_b.st_dev)
print("  符号链接指过来  link.samefile(a.txt) →", link.samefile(normal),
      " ｜ 字符串相等 →", str(link) == str(normal))
other = WORK / "other.txt"
other.write_text("y", encoding="utf-8")
print("  两个真不同的文件 samefile →", h1.samefile(other),
      " ｜ st_ino 相等 →", h1.stat().st_ino == other.stat().st_ino)
print("  → samefile 判的是 (st_dev, st_ino) 这一对，不看路径字符串；"
      "Windows 上这对换成「卷序列号 + 文件 ID」，**本机未验证**，别当事实引用。")
print("  Path.lstat 本机 3.14 存在吗 →", hasattr(Path, "lstat"),
      "；link 的 lstat().st_mode 是 LNK 吗 →", stat.S_ISLNK(link.lstat().st_mode),
      "（老版本没有它就用 os.lstat）")

print("\n=== 9. 收口：这一节四条判据 ===")
print("  ① 遍历顺序：glob / rglob / os.walk **都不保证顺序**，要可复现就得自己 sorted()；")
print("     本脚本每一行列表输出都是排过序的，这就是它能被逐字印进书里的原因。")
print("  ② 匹配规矩：`*` 不吃 `/`（跨层要 `**`/rglob）；点文件归不归 `*` 管，")
print("     glob 模块和 pathlib 的答案**相反**——所以先确认用的是哪个 glob，再谈行为。")
print("  ③ 类型看 st_mode 的比特位（S_ISREG/S_ISDIR/S_ISLNK），不看后缀；")
print("     链接要看它是 stat 到的还是 lstat 到的，两者的 st_size 不是一个东西。")
print("  ④ 大小 ≠ 占盘（st_size vs st_blocks*512），是不是同一个文件看 (st_dev, st_ino)；")
print("     扩展名、路径字符串、`ls -l` 的字节数，三样都靠不住。")
shutil.rmtree(WORK)
print(f"  收尾自证：{WORK.name}/ 还在吗 → {WORK.exists()}"
      f" ｜ 沙箱里本次留下的条目数 = 0（整棵树是 {len(TREE)} 个文件 + 2 个子目录 + 本脚本现造的几个实验对象）")
