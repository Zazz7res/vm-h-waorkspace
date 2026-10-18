#!/usr/bin/env python3
"""场景 6：交互式解释器和脚本到底差在哪 —— 用伪终端把真 REPL 的屏幕抓下来。

跑法：
    python3 h_repl_probe.py

本章要讲清楚的三件事，全靠这个脚本产出证据：
  1. `>>>` 是 REPL 打印的提示符（prompt），你敲的那一行是**终端**回显的，不是 Python 打印的；
  2. REPL 里不写 print 也有回显，脚本里没有 —— 因为 compile 的 mode 不同（见 b_bytecode.py）；
  3. `_` 是 REPL 特有的变量，存「上一次表达式算出来的值」。

技术要点（也是这一节最有意思的地方）：
  * 想拿到「和你在终端里看到一模一样」的文本，必须让 python3 的 stdin/stdout 真的是终端。
    标准库 `pty` 可以自己造一个伪终端（openpty），一头给子进程，一头我们自己读。
  * 3.13 起默认 REPL 换成了新的 PyREPL，它会做逐字符重画（行编辑、语法高亮），
    在伪终端里抓下来是一堆重复片段，没法当证据。所以抓证据这一份用 `PYTHON_BASIC_REPL=1`
    退回经典 REPL —— 这是官方支持的开关，行为差异只在「好不好看」，不在「算什么」。
  * 伪终端会把 `\n` 变成 `\r\n`（终端的行结束约定），脚本最后统一还原成 `\n` 再打印。
"""

import os
import pty
import select
import subprocess
import sys
import time

FEED = ["1 + 1", "x = 10", "x * 3", "_", "_ + 1", "'abc'.upper()", "print('我加了 print')", "exit()"]


def read_quiet(fd, quiet=0.25, limit=4.0):
    """一直读，直到「连续 quiet 秒没有新字节」为止 —— 用来把一段屏幕输出收全。"""
    buf = b""
    deadline = time.monotonic() + limit
    while time.monotonic() < deadline:
        ready, _, _ = select.select([fd], [], [], quiet)
        if not ready:
            break
        try:
            chunk = os.read(fd, 65536)
        except OSError:          # 子进程退出后读 pty master 会抛 EIO
            break
        if not chunk:
            break
        buf += chunk
        deadline = time.monotonic() + 2.0
    return buf


def repl_in_pty(python_args, feed, env_extra=None, one_at_a_time=True):
    """在伪终端里起 REPL，逐行喂 feed，返回抓到的屏幕文本。"""
    env = dict(os.environ)
    env.update(env_extra or {})
    master, slave = pty.openpty()
    proc = subprocess.Popen([sys.executable, *python_args],
                            stdout=slave, stderr=slave, stdin=slave, env=env)
    os.close(slave)
    out = read_quiet(master, quiet=0.4)          # 先收 banner + 第一个 >>>
    for line in feed:
        os.write(master, (line + "\n").encode())
        if one_at_a_time:
            out += read_quiet(master)
    proc.wait()
    os.close(master)
    return out.decode("utf-8", "replace").replace("\r\n", "\n")


print("=" * 72)
print("一、经典 REPL（PYTHON_BASIC_REPL=1）在伪终端里的真实屏幕")
print("=" * 72)
basic = repl_in_pty(["-i"], FEED, env_extra={"PYTHON_BASIC_REPL": "1"})
print(basic, end="")
print()
print("读这份输出的四个要点：")
print("  * 前两行是 banner，里面有版本、编译日期、编译器 —— 报 bug 时要连着它一起贴。")
print("  * `>>>` 是 Python 打的提示符；`1 + 1` 是你打的、由**终端**回显；`2` 才是 Python 打的。")
print("  * `x = 10` 那行后面什么都不打印：赋值表达式没有值可显示。")
print("  * `_` 单独一行 → 打印 30（上一次的结果）；`_ + 1` → 31。这个变量只在 REPL 里存在。")

print()
print("=" * 72)
print("二、同样输入喂给默认的新 REPL（PyREPL）会怎样")
print("=" * 72)
fancy = repl_in_pty(["-i"], ["1 + 1", "exit()"])
lines = fancy.splitlines()
print("\n".join(lines[:9]))
print(f"……（这一段一共 {len(lines)} 行，上面只截了前 {min(9, len(lines))} 行）")
print("→ 同一条命令被重画了 N 遍（行编辑器逐字符回显 + 语法高亮重排），")
print("  作为「可逐字比对」的证据没法用。这就是本章抓 REPL 证据时关掉它的原因。")

print()
print("=" * 72)
print("三、证明「你敲的那一行不是 Python 打印的」")
print("=" * 72)
# stdin 换成管道（不是终端）→ 没有终端回显 → Python 的 stdout 里就只剩提示符和结果。
proc = subprocess.Popen([sys.executable, "-i"], stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
out, _ = proc.communicate(b"1 + 1\nexit()\n")
print(out.decode("utf-8", "replace"), end="")
print("→ 看 `>>> 2` 那一行：`1 + 1` 消失了！它从来没在 Python 的输出流里出现过。")
print("  所以「终端里能看到自己敲的命令」这件事是终端（或行编辑器）干的，")
print("  把 python3 的输出重定向到文件时，文件里只会有结果，不会有命令 —— 别以为日志丢了。")

print()
print("=" * 72)
print("四、脚本模式（exec）里同样的表达式：什么都不打印")
print("=" * 72)
script_src = "1 + 1\n'abc'.upper()\nprint('只有这行有输出')\n"
proc = subprocess.Popen([sys.executable], stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
out, _ = proc.communicate(script_src.encode())
shown = out.decode("utf-8", "replace")
print(f"把 `1 + 1` / `'abc'.upper()` / `print(...)` 三行喂给「非交互的 python3」，输出是：")
print(repr(shown))
print("→ 只剩 print 那一行。前两行的表达式结果被直接丢弃（compile mode = exec）。")
print("  想「脚本里也自动显示表达式结果」？给 python3 加 -i，或者自己写 print/repr。")

print()
print("=" * 72)
print("五、REPL 里独有的东西：sys.ps1 / sys.ps2 / exit")
print("=" * 72)
print(f"本进程（脚本模式）里 sys.ps1 存在吗：{hasattr(sys, 'ps1')}")
prompt_src = repl_in_pty(["-i"], ["import sys", "sys.ps1", "sys.ps2", "exit()"],
                         env_extra={"PYTHON_BASIC_REPL": "1"})
print("在伪终端 REPL 里现问 sys.ps1 / sys.ps2，抓到的是：")
print(prompt_src, end="")
print()
print("→ 交互式启动时 site 模块把 sys.ps1='>>> '、sys.ps2='... ' 设好；")
print("  脚本模式里根本没有这两个属性。`... ` 是多行输入（def/for）的续行提示符 —— 两个点，不是省略号。")
print("→ exit / quit 同样是 site 注入 builtins 的糖。本机实测：")
print("    脚本里 `exit()` 能用（等价 sys.exit()），并不像有些老教程说的会报 RuntimeError；")
print("    但 `python3 -S -c \"exit()\"`（-S = 跳过 site）会报")
print("    NameError: name 'exit' is not defined —— 这就是「它是 site 给的，不是语言的一部分」的铁证。")
print("    Ctrl-D 才是通吃的退出方式（对 stdin 而言就是 EOF）。")
