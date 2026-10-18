#!/usr/bin/env python3
# ============================================================================
# 取证脚本 f_traceback.py —— 第 7 节：traceback 怎么读，以及怎么自己拿到它
#
# 目标：把「一屏红字」拆成可命名的部件，并给出不靠肉眼看它的每一扇门。
# ============================================================================
import io
import subprocess
import sys
import traceback


def leaf(x):
    return x / 0


def middle(x):
    return leaf(x)


def outer(x):
    return middle(x)


print("=== 1. 一份「没人接」的 traceback 长什么样（子进程里的原文）===")
code = (
    "def leaf(x):\n"
    "    return x / 0\n"
    "\n"
    "def middle(x):\n"
    "    return leaf(x)\n"
    "\n"
    "def top():\n"
    "    return middle(6)\n"
    "\n"
    "top()\n"
)
# 特意用 capture 抓回来、再由我们自己打到 stdout：demo 的整屏输出会被重定向进文件，
# 而子进程的 stderr 不缓冲、本脚本的 print 缓冲，让子进程直接写stderr会串到错误的位置。
proc = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, check=False)
sys.stdout.write(proc.stderr)
print("  ↑ 以上整段都是子进程的 stderr ｜ stdout 是空的吗 →", proc.stdout == "",
      "｜ returncode =", proc.returncode)

print("\n=== 2. 读法：只看两行就够了（最后一行 + 最深那一帧）===")
lines = proc.stderr.rstrip("\n").splitlines()
print("  ① 最后一行 = 异常类型 + 消息：", lines[-1])
print("  ② 第一行   = 固定开头：", lines[0])
frames = [ln.strip() for ln in lines if ln.strip().startswith("File ")]
print("  ③ 中间的帧（从上往下 = 从调用者往被调用者），出事现场在最深的最后一帧：")
for f in frames:
    print("     ", f)
print("  函数名出现的顺序 =", [f.split(", in ")[1] for f in frames])
print("  帧数 =", len(frames), "｜ stderr 总行数 =", len(lines))

print("\n=== 3. except 里能直接拿到的三样东西 ===")
try:
    outer(9)
except ZeroDivisionError as exc:
    etype, value, tb = sys.exc_info()
    print("  sys.exc_info() 三个槽 =", etype.__name__, "|", repr(value), "|", type(tb).__name__)
    print("  和 `except ... as exc` 拿到的是同一个对象吗 →", value is exc, "｜", etype is type(exc))
    print("  tb 不是字符串，是链表：type(tb) =", type(tb).__name__,
          "｜ tb_next 的类型 =", type(tb.tb_next).__name__)
    print("  结构化帧（函数名, 源码行）从上到下 =",
          [(f.name, f.line) for f in traceback.extract_tb(tb)])

print("\n=== 4. 同一份现场的两种打印入口：内容一样，去处不同 ===")
try:
    outer(9)
except ZeroDivisionError:
    text_a = traceback.format_exc()
    buf = io.StringIO()
    returned = traceback.print_exc(file=buf)
    print("  format_exc() 给你字符串；print_exc(file=...) 往文件对象里写，返回值 =", returned)
    print("  两份文本逐字节一样吗 →", text_a == buf.getvalue(), "｜ 行数 =", len(text_a.splitlines()))
    print("  这段文本的最后一行 =", text_a.splitlines()[-1])
    print("  （不传 file= 的 print_exc() 写的是 sys.stderr，所以它进日志要靠第 18 节那套重定向）")

print("\n=== 5. SyntaxError 是另一种形状：带 caret 指针，字段问得出来 ===")
try:
    compile("def f(:\n    return 1\n", "示例.py", "exec")
except SyntaxError as exc:
    print("  类型 =", type(exc).__name__, "｜ 消息 =", exc.msg)
    print("  能问出来的定位字段 = filename", exc.filename, "/ lineno", exc.lineno,
          "/ offset", exc.offset, "/ end_offset", getattr(exc, "end_offset", None))
    print("  text 字段（出错那一行的原文）=", repr(exc.text))
    print("  它的 traceback 帧数 =", len(traceback.extract_tb(exc.__traceback__)),
          "（SyntaxError 是编译期报的，执行根本没开始）")
    print("  儿子们：IndentationError →", issubclass(IndentationError, SyntaxError),
          "｜ TabError →", issubclass(TabError, SyntaxError))
print("  ↑ 上面是「程序跑起来之后才 compile 字符串」，所以还剩 1 帧（就是调 compile 的那一帧）。")
print("  真正启动时的语法错**连 Traceback 抬头都没有**（把同样的坏内容交给子进程去启动）：")
broken = subprocess.run([sys.executable, "-c", "def f(:\n    return 1\n"],
                        capture_output=True, text=True, check=False)
for line in broken.stderr.rstrip("\n").splitlines():
    print("  ", line)
print("  退出码 =", broken.returncode,
      "｜ 里面有没有 'Traceback (most recent call last):' 这行 →",
      "Traceback (most recent call last):" in broken.stderr)

print("\n=== 6. 深递归的 traceback 会被折叠（也跑在子进程里）===")
deep = "def f(n):\n    return f(n + 1)\n\nf(0)\n"
proc2 = subprocess.run([sys.executable, "-c", deep], capture_output=True, text=True, check=False)
tail = proc2.stderr.rstrip("\n").splitlines()[-3:]
for line in tail:
    print("  ", line.strip())
print("  stderr 总行数 =", len(proc2.stderr.splitlines()),
      "｜ returncode =", proc2.returncode)
print("  → `[Previous line repeated N more times]` 就是折叠标记；N 每跑都可能变，")
print("     所以它只当量级看，**不能当证据引用**（第 05 章第 20 节：递归限额本身才是定值）。")

print("\n=== 7. 收口 ===")
print("  ① 读 traceback 只看两处：最后一行的「类型: 消息」，和最深那一帧的 文件 / 行号 / 函数名。")
print("  ② 想存进日志：except 里 traceback.format_exc() 拿到字符串（第 18 节演它值多少钱）。")
print("  ③ 想在程序里用：sys.exc_info() / exc.__traceback__ / traceback.extract_tb 全是结构化数据，")
print("     不需要去 split 那段文本 —— 帧上有 .name 和 .line，直接读。")
print("  ④ SyntaxError 那一族没有帧链可读，因为它死在编译期；报错信息里带 caret，看 lineno/offset。")
