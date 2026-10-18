#!/usr/bin/env python3
# ============================================================================
# 取证脚本 b_buffering.py —— 第 3 节：缓冲：为什么管道里看不到进度
#
# 要证明什么：同一段 print 循环，输出接到「终端形状的 fd」上是行缓冲（当场就到对端），
# 接到管道上是块缓冲（整段憋到进程退出）。这不是玄学：本脚本用标准库 pty.openpty() 在
# **本进程内**造一个伪终端，再拿 select 的 0.3 秒超时在子进程退出**之前**读一次 ——
# 读得到就是读得到，读不到就是读不到。屏幕上只留布尔和「有 / 0 条」这种量级：tty 一次
# 给一行还是两行本来就会抖，把精确字节数写进 README 就是给它埋雷。
#
# 为什么要有它：`python3 x.py | tee log`、CI 日志、`>> app.log` 里看不到进度，全是这一条，
# 而「我已经 print 了啊」是最常见的辩解。顺手把 flush / close / 缓冲区大小三个旋钮量清楚。
#
# 安全边界：只用标准库；不联网、不开监听端口（pty 是本机伪终端）；子进程只跑本脚本现写的
# 一段 -c 代码，不 spawn 交互 shell；每个子进程都等到退出才收工；单次 sleep ≤ 0.5 秒。
# 现场全在 buffering-demo/ 里现造现删，只打相对路径，退出码恒为 0，跑两遍输出逐字一致。
# ============================================================================
import io
import os
import pty
import select
import shutil
import subprocess
import sys
import time
from pathlib import Path

WORK = Path("buffering-demo")
LINES = 20                     # 子进程打这么多行：一块就装得下，才好看清「整块憋着」这件事
SELECT_TIMEOUT = 0.3           # 本节承诺的采样窗口：0.3 秒，一次，不多试
POLL = 0.005                   # 握手轮询步长

# 子进程源码（三个格子共用，只差一个参数）：打完 → 留哨兵 → 原地等父进程的暗号。
# 「等暗号」是关键：这样父进程 select 的那 0.3 秒里子进程**一定还活着**，「退出前」三个字才站得住。
CHILD = (
    "import os, sys, time\n"
    "FLUSH = sys.argv[2] == 'flush'\n"
    "for i in range(%d):\n"
    "    print(i, flush=FLUSH)\n"
    "ready, go = sys.argv[1], sys.argv[3]\n"
    "with open(ready + '.tmp', 'w', encoding='utf-8') as fh:\n"
    "    fh.write('isatty=%%s line_buffering=%%s 二进制层=%%s' %% (\n"
    "        sys.stdout.isatty(), sys.stdout.line_buffering, type(sys.stdout.buffer).__name__))\n"
    "os.replace(ready + '.tmp', ready)\n"
    "for _ in range(100):\n"
    "    if os.path.exists(go):\n"
    "        break\n"
    "    time.sleep(%s)\n" % (LINES, POLL)
)


def drain(fd):
    """把此刻已经能读到的字节全读走，读不到立刻返回（绝不阻塞）。"""
    got = b""
    while True:
        ready, _, _ = select.select([fd], [], [], 0)
        if not ready:
            return got
        try:
            chunk = os.read(fd, 65536)
        except OSError:                      # pty 收场时 master 侧抛 EIO，而不是返回空字节
            return got
        if not chunk:
            return got
        got += chunk


def run_case(tag, to_tty, flush):
    """跑一次子进程：stdout 接伪终端或接管道，在它退出前用 select 采样一次。"""
    ready, go = WORK / f"ready-{tag}", WORK / f"go-{tag}"
    tmp = Path(str(ready) + ".tmp")
    for p in (ready, go, tmp):
        p.unlink(missing_ok=True)
    cmd = [sys.executable, "-c", CHILD, str(ready), "flush" if flush else "noflush", str(go)]
    master = None
    if to_tty:
        master, slave = pty.openpty()        # 只借它一个「tty 形状的 fd」，不 spawn shell
        proc = subprocess.Popen(cmd, stdout=slave, stdin=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        os.close(slave)
        fd = master
    else:
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stdin=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        fd = proc.stdout.fileno()
    waited = 0.0
    while not ready.exists() and waited < 0.5:      # 等「打印循环跑完了」这个哨兵
        time.sleep(POLL)
        waited += POLL
    seen, alive = ready.exists(), proc.poll() is None
    readable, _, _ = select.select([fd], [], [], SELECT_TIMEOUT)
    early = os.read(fd, 65536) if readable else b""
    go.write_text("go", encoding="utf-8")          # 放孩子走：采样结束，别让它一直等
    proc.wait()
    late = drain(fd)                               # 退出时才涌出来的那一截
    exited = proc.poll() is not None
    report = ready.read_text(encoding="utf-8") if seen else "（没跑完打印循环）"
    if master is not None:
        os.close(master)
    else:
        proc.stdout.close()
    for p in (ready, go, tmp):
        p.unlink(missing_ok=True)
    return {"seen": seen, "alive": alive, "visible": seen and alive and bool(early),
            "early": bool(early), "late": bool(late), "cr": b"\r" in early,
            "report": report, "exited": exited}


if WORK.exists():
    shutil.rmtree(WORK)          # 上次中途崩了留下的哨兵：先清干净，保证每次都是同一个现场
WORK.mkdir()

print("=== 1. 同一段 print 循环：接伪终端 / 接管道 / 接管道但 flush=True ===")
print("  子进程跑的源码就这几行（三个格子共用，第 3 格只多一个 flush=True）：")
for src_line in CHILD.splitlines()[:5]:
    print(f"      {src_line}")
print("  采样方式：等子进程留下「打印完了」的哨兵，再 select 0.3 秒一次；此时子进程一定还没退出。")
CASES = {}
for tag, to_tty, flush in (("tty", True, False), ("pipe", False, False), ("pipe-flush", False, True)):
    CASES[tag] = run_case(tag, to_tty, flush)
LABELS = {"tty": "伪终端 pty", "pipe": "管道 PIPE", "pipe-flush": "管道 PIPE + flush=True"}
for tag, r in CASES.items():
    print(f"  {LABELS[tag]} ｜ 打印循环跑完了吗 → {r['seen']} ｜ 采样时子进程还活着 → {r['alive']} ｜ 退出前就看得见输出吗 → {r['visible']}")
    print(f"  退出前读到的量 → {'有' if r['early'] else '0 条'} ｜ 退出时才涌出来的量 → {'有' if r['late'] else '0 条'} ｜ 子进程自报：{r['report']}")
print("  ★ 屏幕上没有精确字节数：tty 那一侧一次给一行还是两行本来就不固定，写死进 README 就会假。")
print(f"  → 判据（本机实测）：接 tty 退出前 {CASES['tty']['visible']}、接管道退出前 {CASES['pipe']['visible']}、加 flush=True 退出前 {CASES['pipe-flush']['visible']}。")
print("    同一段代码、同一个解释器，差别只在 stdout 接到的是什么 fd —— 这就是 tail -f 看不到进度的全部原因。")
print(f"  → 附带一条：tty 那次的字节里带 \\r 吗 → {CASES['tty']['cr']}（终端行尾翻译加的，管道那次是 {CASES['pipe']['cr']}）。换行翻译归本章第 5 节，这里只登记它存在。")

print("\n=== 2. 三个缓冲旋钮：buffering=0 / 1 / -1 分别拿到什么对象 ===")
print(f"  io.DEFAULT_BUFFER_SIZE = {io.DEFAULT_BUFFER_SIZE}（二进制层的默认块，本机实测的常量）")
p = WORK / "旋钮.txt"
for mode, buf in (("r", -1), ("r", 1), ("rb", -1), ("rb", 0), ("rb", 8), ("wb", -1), ("wb", 8), ("a", 1)):
    p.write_text("0123456789\n", encoding="utf-8")   # 'w' 会截断，每轮都补回同一份内容
    fh = open(p, mode, buffering=buf)
    layers = [type(fh).__name__]
    layer = getattr(fh, "buffer", None)
    while layer is not None:                        # 一层层往下扒：文本 → 缓冲 → 裸 fd
        layers.append(type(layer).__name__)
        layer = getattr(layer, "buffer", None)
    buf_name = getattr(getattr(fh, "buffer", None), "name", "没有 buffer 这一层")
    print(f"  open(mode={mode!r}, buffering={buf:>2}) → {' → '.join(layers)} ｜ f.name={fh.name!r} ｜ "
          f"f.buffer.name={buf_name!r} ｜ 没有公开 buffer_size 属性 → {not hasattr(fh, 'buffer_size')}")
    fh.close()
try:
    open(p, "r", buffering=0).close()
except ValueError as exc:
    print(f"  open(mode='r', buffering=0) → {type(exc).__name__}: {exc}")
print("  → buffering=0 只许用在二进制模式：文本层必须缓存，它得攒够一个完整的字符才敢解码。")
print("  → buffering=1 的那个「1」不是 1 字节，是「行缓冲」开关；文本层底下的 f.buffer 仍是默认块缓冲。")
print("  缓冲区多大没法问属性，只能量行为 —— 把 buffering 设成 8，一段一段看它什么时候才交：")
handle = open(p, "wb", buffering=8)
for written in (5, 3, 1):
    handle.write(b"x" * written)
    print(f"    累计写了 {handle.tell():>2} 字节 → 另一个 open() 只看到 {os.path.getsize(p)} 字节"
          f"（差额 {handle.tell() - os.path.getsize(p)} 字节躺在用户态缓冲区里）")
handle.close()
print(f"    close() 之后盘上 {os.path.getsize(p)} 字节 = tell（close 里含 flush，这才是 with 保证的那件事）")
print("  → 攒满 8 字节那次都没往下写，第 9 个字节才把前 8 个推下去：边界是缓冲区，不是「我写完了」。")

print("\n=== 3. 文本层的两个开关：line_buffering 与 write_through ===")
p = WORK / "开关.txt"
for label, opener in (
    ("open(p,'w')", lambda: open(p, "w", encoding="utf-8")),
    ("open(p,'w', buffering=1)", lambda: open(p, "w", encoding="utf-8", buffering=1)),
    ("TextIOWrapper(FileIO, write_through=True)", lambda: io.TextIOWrapper(open(p, "wb", buffering=0), write_through=True)),
):
    fh = opener()
    print(f"  {label} → line_buffering={fh.line_buffering} write_through={fh.write_through}")
    fh.write("第一行\n第二行")
    during = os.path.getsize(p)
    fh.close()
    print(f"    写完这两行（第二行不带换行）的那一刻盘上 {during} 字节 ｜ close 后 {os.path.getsize(p)} 字节")
binary = open(p, "rb")
print(f"  二进制层对象 {type(binary).__name__} 有这两个开关吗 → "
      f"{hasattr(binary, 'line_buffering')} / {hasattr(binary, 'write_through')}（它只认 flush）")
binary.close()
print("  → line_buffering=True 那次连没带换行的「第二行」也一起到了盘上：它遇到换行是**整块**推下去，")
print("    不是只推到换行为止。write_through=True 更干脆：文本层不存货，每次 write 都直接交给底层。")

print("\n=== 4. flush 才是边界：写完不 close 不 flush，另一个 open() 读到什么 ===")
p = WORK / "半截.txt"
payload = "这一行很重要" * 2
fh = open(p, "w", encoding="utf-8")
fh.write(payload)
print(f"  写了 {len(payload)} 个字符（{len(payload.encode('utf-8'))} 字节的量），没 flush 没 close，fh.closed={fh.closed}")
print(f"    此时另一个 open(p) 读到的字符数 = {len(open(p, encoding='utf-8').read())} ｜ 磁盘字节数 = {os.path.getsize(p)}")
fh.flush()
print(f"    fh.flush() 之后读到的字符数 = {len(open(p, encoding='utf-8').read())} ｜ 磁盘字节数 = {os.path.getsize(p)}")
fh.close()
print(f"    fh.close() 之后再读 = {len(open(p, encoding='utf-8').read())} 个字符 ｜ 磁盘 {os.path.getsize(p)} 字节")
probe = open(p, "w", encoding="utf-8")
probe.write(payload)
silent = os.path.getsize(p)
probe.tell()                                       # 只是「问一下写到哪了」，没 flush、没 close
asked = os.path.getsize(p)
probe.close()
print(f"  顺带一条本机实测的坑：文本模式里 f.tell() 自己就会 flush —— 问之前磁盘 {silent} 字节，问完变 {asked} 字节")
print("  → write 只到用户态缓冲区：同一进程里另一句 open() 都看不见你写的东西，更别说别的进程和 tail -f；")
print("    而「我先 tell 一下看写了多少」会顺手把这条边界抹掉 —— 上面那行就是证据，别把它当玄学。")
print("  ★ flush 也只是「交给操作系统」，不是「落到磁盘」；断电不丢要 fsync，那是本章第 17 节。")

print("\n=== 5. 本脚本自己这一次：sys.stdout 接的到底是什么 ===")
print(f"  sys.stdout.isatty() → {sys.stdout.isatty()} ｜ line_buffering → {sys.stdout.line_buffering} "
      f"｜ write_through → {sys.stdout.write_through}")
print(f"  二进制层是 {type(sys.stdout.buffer).__name__} ｜ sys.stdout.encoding = {sys.stdout.encoding}")
print("  → 这两行的真实值取决于「这次是谁在收输出」：demo.sh 用 bash -c 跑本脚本、stdout 接的是管道，")
print("    所以行缓冲是 False —— 这就是整章重定向到文件时既没有颜色、也没有实时进度的原因（见第 1 段）。")
print("    你把本脚本直接在终端里跑一遍，这两行会变成 True —— 变的不是代码，是它接到哪。")

print("\n=== 6. 收口：tty 行缓冲 / 管道块缓冲 / flush 才是边界 / close 才是终点 ===")
print(f"  ① 接 tty → 行缓冲，退出前就看得见（实测 {CASES['tty']['visible']}）；接管道 → 块缓冲，"
      f"退出前一条都读不到（实测 {CASES['pipe']['visible']}）；")
print("  ② 想实时可见就自己划边界：print(..., flush=True) 或 sys.stdout.flush() —— 日志、进度条、CI 输出都该这么写；")
print("  ③ flush 交出数据（第 4 段 flush 前另一个 open() 读到 0 个字符），close 交出 fd 并顺带 flush —— 这就是 with 的合同；")
print(f"  ④ 旋钮只有三个：默认块 {io.DEFAULT_BUFFER_SIZE} 字节、buffering=1 换行缓冲、buffering=0 只许二进制（第 2 段那条 ValueError 原文）。")
print("  ★ 反面教训：拿「我在终端里看得见」当依据写代码，接到管道上就全是灰 —— 可见性不是程序的性质，")
print("    是它和 stdout 之间那根 fd 的性质。")
sentinels = sorted(x.name for x in WORK.iterdir() if x.name.startswith(("ready", "go")))
print(f"  收尾自证：握手哨兵还剩 {len(sentinels)} 个（ready*/go* 全删了）；三个子进程都退出了吗 → {all(r['exited'] for r in CASES.values())}")
shutil.rmtree(WORK)
print(f"  {WORK.name}/ 还在吗 → {WORK.exists()}")
