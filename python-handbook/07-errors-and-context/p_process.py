#!/usr/bin/env python3
# ============================================================================
# 取证脚本 p_process.py —— 第 17 节：异常走到程序边界上会发生什么
#
# 目标：退出码 / stdout 与 stderr 的分工 / atexit 与 finally 的顺序 / 线程里的未捕获异常。
# 全部跑在子进程里（用 capture 抓回来打），所以不会影响 demo.sh 自己的退出码。
# ============================================================================
import subprocess
import sys


def run(code, label, args=()):
    proc = subprocess.run([sys.executable, *args, "-c", code],
                          capture_output=True, text=True, check=False)
    print(f"  {label}")
    print(f"      returncode = {proc.returncode}"
          f" ｜ stdout = {proc.stdout!r} ｜ stderr 首行 = "
          f"{(proc.stderr.splitlines() or [''])[0]!r}")
    return proc


print("=== 1. 未捕获异常 = 退出码 1 + 全文进 stderr，stdout 一点不剩 ===")
run("print('这条在 stdout')\n1 / 0\n", "① 正常打印过后才崩：")
run("1 / 0\nprint('这条永远不执行')\n", "② 崩在前面：")
print("  → 分工是死的：traceback 只走 stderr。把 stdout 存进文件/管道时，报错不会污染数据（第 13 节）。")
print("    反过来说：`python3 x.py > out.txt` 时你**看不见**报错位置，除非终端还在打 stderr。")

print("\n=== 2. sys.exit(值) 的三种形态：退出码怎么来的 ===")
run("import sys\nsys.exit(3)\n", "① sys.exit(3)")
run("import sys\nsys.exit()\n", "② sys.exit()（不给值）")
run("import sys\nsys.exit('这行会打到 stderr')\n", "③ sys.exit('字符串')")
print("  → 字符串那条固定给退出码 1，并把文本写到 stderr；给整数就用那个整数。")
print("    sys.exit 做的事就是**抛 SystemExit**：")
run("import sys\ntry:\n    sys.exit(7)\nexcept SystemExit as e:\n    print('接到了 SystemExit, code =', e.code)\n",
    "④ 在同一个进程里把 sys.exit 接住：")
print("    接住之后进程继续跑到末尾，退出码回到 0 —— 那条 7 被你自己吃掉了。")
proc = run("import sys\ntry:\n    sys.exit(7)\nexcept Exception:\n    print('这行不会打')\n",
           "⑤ 改用 except Exception 拦 sys.exit：")
print("    拦不住（第 3 节：SystemExit **不是** Exception 的儿子）→ 实测 returncode =", proc.returncode)

print("\n=== 3. finally / with / atexit 的执行顺序（谁先谁后）===")
order_code = (
    "import atexit, sys\n"
    "atexit.register(lambda: print('atexit-1'))\n"
    "atexit.register(lambda: print('atexit-2'))\n"
    "class C:\n"
    "    def __enter__(self): return self\n"
    "    def __exit__(self, *a): print('with-exit')\n"
    "def f():\n"
    "    try:\n"
    "        print('body')\n"
    "    finally:\n"
    "        print('finally')\n"
    "with C():\n"
    "    f()\n"
    "print('main-end')\n"
)
proc = subprocess.run([sys.executable, "-c", order_code], capture_output=True, text=True, check=False)
print("  stdout 逐行 =", proc.stdout.splitlines())
print("  → atexit 注册的回调**逆序**执行（后登记先跑，和第 11/14 节那条栈律一致），")
print("    而且跑在 finally / with 全部结束**之后**、解释器关闭之前。")
print("  未捕获异常时 atexit 还跑吗？")
proc = subprocess.run([sys.executable, "-c",
                       "import atexit\natexit.register(lambda: print('atexit 跑了'))\n1 / 0\n"],
                      capture_output=True, text=True, check=False)
print("      returncode =", proc.returncode, "｜ stdout =", repr(proc.stdout))

print("\n=== 4. os._exit：跳过 atexit、跳过 finally、连缓冲区都不冲 ===")
buffered = (
    "import os, sys, atexit\n"
    "atexit.register(lambda: print('这条来自 atexit'))\n"
    "print('这条进了缓冲区')\n"
    "os._exit(0)\n"
    "print('到不了')\n"
)
proc = subprocess.run([sys.executable, "-c", buffered], capture_output=True, text=True, check=False)
print("  子进程 stdout（**被管道捕获，所以是块缓冲**）=", repr(proc.stdout),
      "｜ returncode =", proc.returncode)
flushed = buffered.replace("os._exit(0)", "sys.stdout.flush(); os._exit(0)")
proc = subprocess.run([sys.executable, "-c", flushed], capture_output=True, text=True, check=False)
print("  同一份代码，只在 os._exit 前加一句 flush() =", repr(proc.stdout))
never = buffered.replace("os._exit(0)", "sys.exit(0)")
proc = subprocess.run([sys.executable, "-c", never], capture_output=True, text=True, check=False)
print("  同一份代码，把 os._exit(0) 换成 sys.exit(0) =", repr(proc.stdout))
print("    → 三份只差那一行：'这条来自 atexit' 只在最后一份里出现，说明 os._exit 把 atexit 整步跳过了；")
print("      第一份连'这条进了缓冲区'都没有，是因为它躺在块缓冲里没来得及冲出去（不是没执行 print）。")
print("  → os._exit 是「立刻死」：不冲 stdout、不跑 finally、不跑 atexit、不看异常。")
print("    它的正当用处极少（典型场景是 fork 之后让子进程立刻退出，不碰父进程留下的任何收尾）；")
print("    平时写它 = 亲手制造第 16 节那种「没有任何痕迹的失败」。")

print("\n=== 5. sys.excepthook：未捕获异常的最后一道处理（只在主线程）===")
hook = (
    "import sys\n"
    "def mine(etype, value, tb):\n"
    "    print('HOOK 收到', etype.__name__, '|', value, file=sys.stderr)\n"
    "sys.excepthook = mine\n"
    "1 / 0\n"
)
proc = subprocess.run([sys.executable, "-c", hook], capture_output=True, text=True, check=False)
print("  装了自定义 hook 之后，stderr =", repr(proc.stderr), "｜ returncode =", proc.returncode)
print("  → hook 换掉只换**打印方式**，退出码仍是 1（hook 里 return 不能改退出码）。")
print("    hook 收到的三元组 = 第 10 节 __exit__ 收到的那三个（同一个 exc_info 形状）。")
print("  未捕获异常时的**三条出口**：① sys.excepthook（主线程）② threading.excepthook（子线程）")
print("    ③ 都没装就打 traceback。本机有没有 threading.excepthook →", end=" ")
proc = subprocess.run([sys.executable, "-c",
                       "import threading; print(hasattr(threading, 'excepthook'))"],
                      capture_output=True, text=True, check=False)
print(proc.stdout.strip())

print("\n=== 6. 子线程里的未捕获异常：不会杀进程，退出码照样 0 ===")
thread_code = (
    "import threading, sys\n"
    "def bad():\n"
    "    1 / 0\n"
    "t = threading.Thread(target=bad)\n"
    "t.start()\n"
    "t.join()\n"
    "print('主线程活得好好的，退出码由它决定')\n"
)
proc = subprocess.run([sys.executable, "-c", thread_code], capture_output=True, text=True, check=False)
print("  returncode =", proc.returncode, "｜ stdout =", repr(proc.stdout))
print("  stderr 最后一行 =", repr((proc.stderr.splitlines() or [""])[-1]))
print("  stderr 行数 =", len(proc.stderr.splitlines()), "（traceback 照样打全，只是不改变结局）")
print("  → 这是并发代码最常见的静默：线程崩了，主流程不知道，join 也不报错。")
print("    判据：**线程不会把异常传给创建者**；要传就得自己收（把 exc 放进队列 / 存到结果对象上，")
print("    或者用 concurrent.futures 那种把异常装进 Future 的写法 —— 本章不演，只把这条记在「没验证」里）。")

print("\n=== 7. 收口 ===")
print("  ① 未捕获异常 → traceback 进 stderr、退出码 1；这是解释器替你做的最后一层 except。")
print("  ② sys.exit(n) 就是 raise SystemExit(n)：能接（except SystemExit），但 except Exception 接不住。")
print("  ③ 顺序：块体 → finally / __exit__ → atexit（逆序）→ 解释器关闭。os._exit 把这三步全省了。")
print("  ④ 子线程的未捕获异常只打 traceback，不改变进程退出码 —— 别把「没崩」当「没出错」。")
print("  ⑤ CLI 里 except 之后**必须**给非零退出码（第 16 节第 5 段），否则上游全在骗自己。")
