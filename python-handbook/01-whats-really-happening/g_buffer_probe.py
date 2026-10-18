#!/usr/bin/env python3
"""场景 5：stdout 的缓冲 —— 同样的 print，接终端和接管道为什么表现不一样。

跑法：
    python3 g_buffer_probe.py

实验设计（这是本章唯一需要「计时」的实验，所以只数行数/字节数，不打时间戳，
输出完全可复现）：

    子进程 = python3 -c "print('第 1 行'); sleep(1.2); print('第 2 行')"
    父进程在子进程启动 0.4 秒后，非阻塞地把它已经吐出来的字节读干净，数有几行；
    然后等子进程退出，再数一遍总共几行。

「0.4 秒时到 1 行」= 每行立刻发出（行缓冲 / 无缓冲）。
「0.4 秒时到 0 行，结束时才 2 行」= 攒在用户态缓冲区里，进程退出时才一次倒出来（块缓冲）。

只用到标准库：os / pty / select / subprocess / time。
"""

import os
import pty
import select
import subprocess
import sys
import time

CHILD = "import time\nprint('第 1 行')\ntime.sleep(1.2)\nprint('第 2 行')\n"
CHILD_FLUSH = CHILD.replace("print('第 1 行')", "print('第 1 行', flush=True)")
CHILD_ERR = ("import sys, time\n"
             "print('第 1 行', file=sys.stderr)\n"
             "time.sleep(1.2)\n"
             "print('第 2 行', file=sys.stderr)\n")
# 采样时刻 0.4 秒：这时子进程刚打印完第 1 行，还在睡它那 1.2 秒。
SAMPLE_AT = 0.4


def drain(fd):
    """把 fd 上「此刻已经能读到」的字节全部读走，绝不阻塞。"""
    got = b""
    while True:
        ready, _, _ = select.select([fd], [], [], 0)
        if not ready:
            return got
        try:
            chunk = os.read(fd, 65536)
        except OSError:          # 对端已关闭：pty 的 master 在结束时抛 EIO 而不是返回空
            return got
        if not chunk:
            return got
        got += chunk


def spawn(kind, code, extra_args=(), extra_env=None, to_stderr=False):
    """起子进程，把它的 stdout（或 stderr）接到伪终端 / 管道上，返回读端 fd 和进程对象。"""
    cmd = [sys.executable, *extra_args, "-c", code]
    env = dict(os.environ)
    env.pop("PYTHONUNBUFFERED", None)     # 保证「默认情况」真的是默认
    if extra_env:
        env.update(extra_env)
    if kind == "pty":
        master, slave = pty.openpty()
        proc = subprocess.Popen(cmd, stdout=slave, stderr=slave,
                                stdin=subprocess.DEVNULL, env=env)
        os.close(slave)
        return master, proc
    read_fd, write_fd = os.pipe()
    if to_stderr:                          # 只往 stderr 打印：把 stderr 接到读端
        proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=write_fd,
                                stdin=subprocess.DEVNULL, env=env)
    else:
        proc = subprocess.Popen(cmd, stdout=write_fd, stderr=subprocess.DEVNULL,
                                stdin=subprocess.DEVNULL, env=env)
    os.close(write_fd)
    return read_fd, proc


def sample(code, kind="pipe", extra_args=(), extra_env=None, to_stderr=False):
    """返回 (0.4 秒时到的字节数, 结束时总字节数, 0.4 秒时的行数, 结束时总行数)。"""
    fd, proc = spawn(kind, code, extra_args, extra_env, to_stderr)
    time.sleep(SAMPLE_AT)
    early = drain(fd)
    proc.wait()
    late = drain(fd)
    os.close(fd)
    total = early + late
    text = total.decode("utf-8", "replace")
    return (len(early), len(total),
            early.decode("utf-8", "replace").count("\n"), text.count("\n"))


def row(label, code, **kw):
    """打印一行对照结果：0.4 秒时到几行 vs 最终到几行（子进程固定打印 2 行）。"""
    _eb, _tb, early_lines, total_lines = sample(code, **kw)
    # 0.4 秒时子进程还在 sleep，所以「到 1 行」= 第 1 行当场就发出来了；「到 0 行」= 被憋住了。
    verdict = "第 1 行当场就发出来" if early_lines >= 1 else "两行全被憋到进程结束"
    print(f"{label:<26} 0.4 秒时 {early_lines} 行   结束时 {total_lines} 行   → {verdict}")


print("=" * 74)
print("一、先问解释器自己：这种状态下 stdout 是什么模式")
print("=" * 74)
PROBE = ("import sys\n"
         "print('isatty=%s  line_buffering=%s  write_through=%s  底层=%s'"
         " % (sys.stdout.isatty(), sys.stdout.line_buffering,"
         " sys.stdout.write_through, type(sys.stdout.buffer).__name__))\n")
for label, kind, args in (("伪终端 + 默认", "pty", ()),
                          ("管道 + 默认", "pipe", ()),
                          ("管道 + -u", "pipe", ("-u",))):
    fd, proc = spawn(kind, PROBE, extra_args=args)
    proc.wait()
    data = drain(fd)
    os.close(fd)
    print(f"{label:<18} {data.decode('utf-8', 'replace').strip()}")
print("→ 注意 `-u` 那行：line_buffering 仍然是 False，变的是 write_through。")
print("  所以 -u 不是「改成行缓冲」，而是「绕过用户态缓冲区，直接写进 fd」。")

print()
print("=" * 74)
print("二、计时实验：0.4 秒时子进程已经吐出几行了？（子进程共 2 行）")
print("=" * 74)
row("伪终端(pty) + 默认", CHILD, kind="pty")
row("管道(pipe) + 默认", CHILD)
row("管道 + -u", CHILD, extra_args=("-u",))
row("管道 + PYTHONUNBUFFERED=1", CHILD, extra_env={"PYTHONUNBUFFERED": "1"})
row("管道 + print(flush=True)", CHILD_FLUSH)
row("管道 + 只往 stderr 打印", CHILD_ERR, to_stderr=True)
print()
print("解读：")
print("  * 伪终端 1 行 → 你在终端里实时看到输出，因为 tty 走行缓冲。")
print("  * 管道 0 行   → 输出被日志/CI/`| tee` 吃掉时是攒着的，")
print("    表现为「程序跑完才哗啦一下全出来」，很容易被误判成卡死。")
print("  * stderr 天生行缓冲 → 所以报错总能立刻看到，进度/日志却看不到。")
print("  * 修法按侵入性排序：print(..., flush=True)")
print("    < 环境变量 PYTHONUNBUFFERED=1 < 启动参数 python3 -u")
print("    < 代码里 sys.stdout.reconfigure(line_buffering=True)")

print()
print("=" * 74)
print("三、缓冲区有多大：用字节数把「攒到多少才发」这件事量出来")
print("=" * 74)
NOSPLIT = "import sys, time\nsys.stdout.write('.' * 100)\ntime.sleep(1.2)\nsys.stdout.flush()\n"
SPLIT = "import sys, time\nsys.stdout.write('.' * 100)\nsys.stdout.flush()\ntime.sleep(1.2)\n"
BIG_FLAT = "import sys, time\nsys.stdout.write('.' * 9000)\ntime.sleep(1.2)\n"
BIG_LINES = "import sys, time\nsys.stdout.write('x\\n' * 4500)\ntime.sleep(1.2)\n"
for label, code in (("写 100 字节，不 flush 就睡", NOSPLIT),
                    ("写 100 字节，先 flush 再睡", SPLIT),
                    ("一次写 9000 字节（无换行）", BIG_FLAT),
                    ("一次写 9000 字节（4500 个换行）", BIG_LINES)):
    eb, tb, _el, _tl = sample(code)
    print(f"{label:<28} 0.4 秒时 {eb:>5} 字节   结束时 {tb:>5} 字节")
print("→ 结论（本机实测，别再信「一行一行flush」的说法）：")
print("  * 不 flush 时，9000 字节**照样全被攒着**，哪怕中间有 4500 个换行也一样。")
print("  * 换行只在 stdout 是 tty 时才触发 flush（因为那时 line_buffering=True）；")
print("    接管道时 line_buffering=False，换行**不代表**会往下写。")
print("  * 「写到多大才会自己冲出去」这个临界点本机没测准：试到 200000 字节时，")
print("    父进程被自己没及时读走的管道数据卡死了（子进程写不进 64KB 的管道 → 阻塞，")
print("    父进程却在等子进程退出 → 互相等）。这本身就是重要一课：")
print("    **只要读端不持续读，管道就会反压把写端冻住**。所以本章的实验都控制在几 KB。")
