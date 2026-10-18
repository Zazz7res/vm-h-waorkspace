#!/usr/bin/env python3
# ============================================================================
# 取证脚本 p_atomic_write.py —— 第 17 节：怎么把「写完」变成磁盘上的事实
#
# 第 07 章 q_files.py 立过一条边界：「配置文件只能靠同目录临时文件 + os.replace」。
# 这一节把它做实：量 flush/fsync 各自买到什么 → 看半截文件怎么发生 → 用两个子进程做
# 「就地写」vs「tmp + replace」的崩溃一致性对比。现场全在 atomic-demo/ 里，跑完 rmtree。
# ★掉电后的真实行为本机验不了（无可控断电手段），一律标未验证。
# ============================================================================
import errno
import io
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

WORK = Path("atomic-demo")
BUF = 200_000                        # 第 1 段的总量：200KB
HALF, YI_LEN, JA_LEN = 100_000, 40_000, 200_000   # 甲每次写 HALF（共 2*HALF），乙写 YI_LEN；长度不同才看得出混
MIXED = b"B" * YI_LEN + b"\0" * (HALF - YI_LEN) + b"A" * HALF   # 提前算好的「事故串」

if WORK.exists():                    # 上次崩在这里留下的残骸：先清干净，保证每次同一现场
    shutil.rmtree(WORK)
WORK.mkdir()

def run_children(codes):
    """同时起若干子进程（屏幕上只写成 python3 -c，不打解释器绝对路径），输出全 capture。
    WHY 并发+capture：子进程靠标志文件互相排队，串行起等于死锁；不接住管道会堵、上屏会掺调度顺序。"""
    procs = [subprocess.Popen([sys.executable, "-c", c], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True) for c in codes]
    for pr in procs:                 # 超时一定 kill：绝不把子进程留在后台
        try:
            pr.communicate(timeout=60)
        except subprocess.TimeoutExpired:
            pr.kill(); pr.communicate()            # 超时的一定要先杀再收，绝不把子进程留在后台
    return [pr.returncode for pr in procs]


print("=== 1. 写完 ≠ 落盘：不 close 不 flush 时另一个句柄读到多少（0 / 部分 / 全部都有）===")
print(f"  本机 io.DEFAULT_BUFFER_SIZE = {io.DEFAULT_BUFFER_SIZE} 字节（①②③ 的分界线就是它）")
p = WORK / "buf.bin"
for title, chunks in (("① 一次 write 200 字节（小于缓冲区）", [200]),
                      (f"② 分 {BUF // 1000} 次各写 1000 字节", [1000] * (BUF // 1000)),
                      (f"③ 一次 write {BUF} 字节（大于缓冲区）", [BUF])):
    f = open(p, "wb")                # 故意不用 with：要看的正是「还没 close」的那一刻
    for size in chunks:
        f.write(b"x" * size)
    seen = [len(p.read_bytes())]     # 另一个句柄、另一条读路径：只看内核已经收到多少
    f.flush()
    seen.append(len(p.read_bytes()))
    os.fsync(f.fileno())
    seen.append(len(p.read_bytes()))
    f.close()
    print(f"  {title:<28}：不 flush={seen[0]} ｜ flush 后={seen[1]} ｜ fsync 后={seen[2]} ｜ 应有={sum(chunks)}")
print("  → 三个数并排：flush 买到「用户态缓冲区 → 内核」，fsync 买到「请求内核别丢」；从另一个句柄读，")
print("    这两笔数字一模一样 —— read 走的是内核页缓存，压根没下磁盘。")
print("  ★「fsync 之后拔电会不会丢」本机验不了（无可控掉电手段），这条**未验证**：fsync ≠ 已物理写入介质。")

print("\n=== 2. 半截文件事故：open(p,'w') 之后崩在写入中间，旧数据就没了 ===")
cfg = WORK / "conf.json"
cfg.write_text(json.dumps({"name": "旧配置", "items": [1, 2, 3]}, ensure_ascii=False), encoding="utf-8")
before = cfg.stat().st_size
try:
    with cfg.open("w", encoding="utf-8") as fh:      # 'w' 在打开那一刻就已 O_TRUNC
        fh.write('{"name": "新配置", "items": [')
        for i in range(50):
            fh.write(f"{i},")
        raise RuntimeError("序列化到一半崩了")
except RuntimeError as exc:
    print(f"  接住的异常：{type(exc).__name__}: {exc} ｜ 原文件字节数 {before} → {cfg.stat().st_size}")
print(f"  磁盘上剩下的尾巴：{cfg.read_bytes()[-24:]!r}")
try:
    json.loads(cfg.read_text(encoding="utf-8"))
except json.JSONDecodeError as exc:
    print(f"    → {type(exc).__name__}: {exc}")
print("  → with 只保证 close，不保证写全：事故不是「新的没写成」，是**旧的也没了** —— 没备份、没事务、不报错。")

print("\n=== 3. 同目录临时文件 + os.replace：读者只会看到完整旧版或完整新版 ===")
stable = WORK / "settings.txt"
OLD_LEN, NEW_LEN = 30_000, 70_000
stable.write_text("o" * OLD_LEN, encoding="utf-8")
tmp = stable.with_name(stable.name + ".tmp")         # 同一目录，名字确定（随机名会让输出抖）
with tmp.open("w", encoding="utf-8") as fh:
    fh.write("n" * NEW_LEN)                           # 一次写全
    fh.flush()                                        # 用户态 → 内核
    os.fsync(fh.fileno())                             # 请求内核别丢（掉电未验证）
probe_1 = len(stable.read_text(encoding="utf-8"))     # tmp 已写好，但还没 replace
os.chmod(tmp, 0o644)                                  # 见第 4 段：权限必须在这里改
probe_2 = len(stable.read_text(encoding="utf-8"))
os.replace(tmp, stable)                               # 同目录 → rename(2) 原子替换
probes = (probe_1, probe_2, len(stable.read_text(encoding="utf-8")))   # 第三次：replace 之后
print(f"  旧版本长度={OLD_LEN} 新版本长度={NEW_LEN} ｜ 三次现场探测读到的长度 = {probes}")
print(f"  → 每次都严格等于旧或新 → {all(x in (OLD_LEN, NEW_LEN) for x in probes)}"
      f" ｜ 读到中间值的次数 = {sum(x not in (OLD_LEN, NEW_LEN) for x in probes)}")
print(f"  replace 之后目录里的条目 = {sorted(x.name for x in WORK.iterdir())}　← .tmp 已经没了")
print("  → 判据就印在屏幕上：读者能看到的长度只允许是「旧长度」或「新长度」之一，出现第三种长度就是有")
print("    人在原文件上就地写。")

print("\n=== 4. ★mkstemp 的权限会跟着文件走：os.replace 之后它还是 0600 ===")

def promote(dest, fix_mode):
    """mkstemp 建 tmp（0600）→ 可选地补 chmod → replace 转正，返回建好时与转正后的权限。"""
    fd, raw = tempfile.mkstemp(dir=str(WORK), prefix=".perm", suffix=".tmp")
    os.write(fd, b"y" * 16)
    os.fsync(fd)
    os.close(fd)                                     # mkstemp 给的是裸 fd，必须自己关
    before_mode = oct(os.stat(raw).st_mode & 0o777)
    if fix_mode:
        os.chmod(raw, 0o644)                          # ★必须在 replace 之前改
    os.replace(raw, WORK / dest)
    return before_mode, oct(os.stat(WORK / dest).st_mode & 0o777)


for dest, fix in (("perm_bad.txt", False), ("perm_good.txt", True)):
    m_before, m_after = promote(dest, fix)
    print(f"  tmp 名 .perm????????.tmp（8 个随机字符不打）{'replace 前补 chmod(0o644)' if fix else 'replace 前没 chmod       '}：建好时={m_before}"
          f" ｜ replace 之后={m_after} ｜ 等于本该有的 0o644 吗：{m_after == oct(0o644)}")
print("  → rename(2) 只改目录项、不改 inode：临时文件什么权限，转正之后就是什么权限。mkstemp 故意给 0600")
print("    （防别人抢注），所以它**不能**直接当配置文件的 tmp：要么 replace 前 chmod，要么改用普通 open() 建 tmp —— 那才是 umask 0o022 给你的 0644。")

print("\n=== 5. 崩溃一致性对比：A 组就地写 vs B 组 tmp + replace（各两个子进程）===")
racy, flag1, flag2 = WORK / "racy.bin", str(WORK / "f1"), str(WORK / "f2")
racy.write_bytes(b"init")
code_a1 = (f"import os, time\n"
           f"h = open({str(racy)!r}, 'wb'); h.write(b'A' * {HALF}); h.flush()\n"   # 前半落下去 → 甲的 OS 游标 = HALF
           f"open({flag1!r}, 'w').close()\n"                      # 告诉乙：现在可以截断了
           f"while not os.path.exists({flag2!r}): time.sleep(0.005)\n"
           f"h.write(b'A' * {HALF}); h.flush(); h.close()\n")      # 后半照旧落在自己的游标上
code_b1 = (f"import os, time\n"
           f"while not os.path.exists({flag1!r}): time.sleep(0.005)\n"
           f"q = open({str(racy)!r}, 'wb'); q.write(b'B' * {YI_LEN}); q.flush(); q.close()\n"  # ★'w' 截成 0 字节，却改不了甲的游标
           f"open({flag2!r}, 'w').close()\n")
rc = run_children([code_a1, code_b1])
got = racy.read_bytes()
print(f"  A 组 两个子进程（python3 -c …，全 capture）都正常退出 → {all(x == 0 for x in rc)} ｜ 最终字节数 = {len(got)}"
      f" ｜ 甲的完整长度 = {JA_LEN} ｜ 乙的完整长度 = {YI_LEN}")
print(f"  → 内容等于甲的完整 payload → {got == b'A' * JA_LEN} ｜ 等于乙的完整 payload → {got == b'B' * YI_LEN}")
print(f"     顺序被两个标志文件钉死，结果 =「乙的整段 + \\x00 空洞 + 甲的后半」，长度 {len(got)} 恰好等于甲的 {JA_LEN}。")
print(f"  → 「长度对得上」根本不能当完整性判据：等于事故串 → {got == MIXED} ｜ 有 \\x00 空洞 → {bytes([0]) in got}")

def atomic_child(target, char, length, tmp_name, ready, wait=None):
    """B 组子进程的五步：tmp → 写全 → fsync → chmod → replace（wait 用来钉死先后顺序）。"""
    return ("import os, time\n"
            f"tmp = {tmp_name!r}\n"
            f"f = open(tmp, 'wb'); f.write(b'{char}' * {length}); f.flush(); os.fsync(f.fileno()); f.close()\n"
            "os.chmod(tmp, 0o644)\n"
            + (f"while not os.path.exists({wait!r}): time.sleep(0.005)\n" if wait else "")
            + f"os.replace(tmp, {target!r})\nopen({ready!r}, 'w').close()\n")


targ, flag3, flag4 = WORK / "final.bin", str(WORK / "f3"), str(WORK / "f4")
targ.write_bytes(b"init")
rc2 = run_children([atomic_child(str(targ), "A", JA_LEN, str(WORK / "t1.tmp"), flag3),
                    atomic_child(str(targ), "B", YI_LEN, str(WORK / "t2.tmp"), flag4, flag3)])
final = targ.read_bytes()
print(f"  B 组 两个子进程都正常退出 → {all(x == 0 for x in rc2)} ｜ 最终字节数 = {len(final)}"
      f"（只允许 {JA_LEN} 或 {YI_LEN}）｜ 落在候选之一 → {len(final) in (JA_LEN, YI_LEN)}")
print(f"  → 最终内容 = 乙（标志文件排在后面的那个）完整 payload → {final == b'B' * YI_LEN}"
      f" ｜ = 甲的完整 payload → {final == b'A' * JA_LEN}")
print(f"  → 出现半截 / 混合 / 空洞 → {bytes([0]) in final or final not in (b'A' * JA_LEN, b'B' * YI_LEN)}"
      f" ｜ replace 之后残留 .tmp 个数 = {len(list(WORK.glob('*.tmp')))}")
print("  → B 组无论谁赢，输的那一方脏的都是**自己那个 tmp**，碰不到目标文件：结果集只有 {完整甲, 完整乙}，没有第三种形状 —— 这就叫原子。")

print("\n=== 6. rename / replace 的边界：同目录才谈原子，跨设备直接失败 ===")
s1, s2, q1, adir = WORK / "s1.txt", WORK / "s2.txt", WORK / "q1.txt", WORK / "dir"
s1.write_text("源", encoding="utf-8")
s2.write_text("目标", encoding="utf-8")
os.rename(s1, s2)
adir.mkdir(exist_ok=True)
print(f"  本机 os.rename 到「已存在的普通文件」→ 成功，目标现在={s2.read_text(encoding='utf-8')!r}")
s1.write_text("源2", encoding="utf-8")
q1.write_text("pathlib 源", encoding="utf-8")
for label, call in (("os.rename", lambda: os.rename(s1, adir)), ("Path.rename", lambda: Path(q1).rename(adir))):
    try:
        call()
    except OSError as exc:
        print(f"    → {label} 撞已存在的目录：{type(exc).__name__}: [Errno {exc.errno}] {exc.strerror}")
print("  → 本机是 POSIX：rename/replace、Path.rename/Path.replace 都落到同一个 rename(2)，对已存在的**普通")
print("    文件**都会替换；「rename 报错、只有 replace 强制覆盖」是 Windows 语义，本机验不了、**标未验证**。")
q1.write_text("pathlib 源2", encoding="utf-8")
Path(q1).replace(s2)
print(f"  Path.replace 到已存在普通文件 → 成功；q1 还在吗={q1.exists()} s2 内容={s2.read_text(encoding='utf-8')!r}")
cross = next((b for b in ("/dev/shm", tempfile.gettempdir(), "/var/tmp") if os.path.isdir(b)
              and os.access(b, os.W_OK) and os.stat(b).st_dev != os.stat(WORK).st_dev), None)
if cross is None:
    print("  ★本机没找到第二个可写的文件系统 → 跨设备这条**未验证**（不编造结论）。")
else:
    fdx, foreign = tempfile.mkstemp(dir=cross, prefix="py08-xdev", suffix=".bin")
    with os.fdopen(fdx, "wb") as fh:
        fh.write(b"x" * 16)
    print(f"  借道另一台文件系统（basename={os.path.basename(cross)}，与 {WORK.name}/ 同设备吗="
          f"{os.stat(cross).st_dev == os.stat(WORK).st_dev}）")
    try:
        os.rename(foreign, str(WORK / "xdev.bin"))
    except OSError as exc:
        # 原文尾部还带两个路径，本机那两个既是绝对路径又含随机后缀，两条打印规矩都不许上屏，
        # 所以只留报错主体（类名 + Errno + strerror），一字未改。
        print(f"    → {type(exc).__name__}: [Errno {exc.errno}] {exc.strerror} ｜ errno.EXDEV={errno.EXDEV}")
        try:
            os.EXDEV
        except AttributeError as exc2:
            print(f"      os.EXDEV 实测：{type(exc2).__name__}: {exc2}")
    os.remove(foreign)
    print("  ★所以临时文件必须和被覆盖的文件在**同一个目录**（连文件系统都不用比了）：跨设备时")
    print("    rename/replace 都抛 EXDEV，只能退回「复制 + 改名」，那时就再也不原子了。")

print("\n=== 7. 收口：配置文件的安全写法只有这四行 ===")
print("  ① tmp 与目标**同目录**：tmp = path.with_name(path.name + '.tmp')（第 3 段这句；顺带堵死 EXDEV）")
print("  ② 一次写全 → f.flush() → os.fsync(fd) → 再 close（fsync 只到「交给内核」为止，掉电未验证）。")
print("  ③ os.chmod(tmp, 0o644) → os.replace(tmp, path)（第 4 段：权限跟 inode 走，不跟名字走）。")
print("  ④ 任何一步崩了都要 tmp.unlink(missing_ok=True)，别让 .tmp 留在目录里等下次咬人。")
print("  → with 只保证 close，不保证事务；「写完」「落盘」「原子」是三件不同的事，各有各的写法。")
print("  → 有没有留半截别靠感觉：清点 .tmp 个数（第 5 段 B 组 = 0），并**故意留一个再数一次**，响过才算数。")

shutil.rmtree(WORK)
print(f"\n  清理：{WORK.name}/（含全部子目录与 tmp）已删除 ｜ 还在吗 {WORK.exists()} ｜ 脚本目录 .tmp 残留 = "
      f"{len(list(Path('.').glob('*.tmp')))} ｜ 本机 Python {sys.version.split()[0]}，全程只写相对路径，无绝对路径上屏。")
