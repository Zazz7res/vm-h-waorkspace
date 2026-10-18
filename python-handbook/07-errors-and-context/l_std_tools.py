#!/usr/bin/env python3
# ============================================================================
# 取证脚本 l_std_tools.py —— 第 13 节：contextlib 里那四个现成的，和它们各自的代价
#
# 目标：suppress / closing / nullcontext / redirect_stdout —— 全部现场跑 + 拆实现。
# ============================================================================
import contextlib
import inspect
import io
import sys
import textwrap
import traceback
from contextlib import (closing, nullcontext, redirect_stderr, redirect_stdout,
                        suppress)

print("=== 引子 ｜ 先看这个模块里到底有多少公共名字（别背，扫一眼有个数）===")
public = sorted(n for n in dir(__import__("contextlib")) if not n.startswith("_"))
print("  ", public)
print(f"  共 {len(public)} 个；本节只演 4 个最常用的，第 14 节演 ExitStack，其余列在 README 的「没演」。")

print("\n=== 1. suppress(*类型)：把指定类型的异常就地扔掉 ===")
with suppress(ValueError):
    int("四十二")
print("  with suppress(ValueError): int('四十二') → 什么都没发生（这正是它的用途）")
try:
    with suppress(ValueError):
        int("五十五") + undefined_thing   # noqa
except NameError as exc:
    print("  边界：类型不对就**不**咽 →", type(exc).__name__, ":", exc)
with suppress(NameError, AttributeError):
    print("  多个类型可以并列（元组语义，第 4 节）：这行照常打印")
print("  ★代价现场：把上面那个 NameError 换成 suppress(NameError) 会发生什么？")
with suppress(NameError):
    print(undefinied_name)               # noqa  故意拼错
print("  → 打错了变量名，程序**安静地**跳过了整块，然后继续跑。没有任何输出告诉你少做了事。")
print("    suppress 的语义就是「我确定这类异常在这里不重要」；用它包住一段有副作用的代码 = 赌。")
print("  拆开看它到底做了什么（inspect 读本机标准库源码，一个字都没改）：")
print(textwrap.indent(inspect.getsource(contextlib.suppress), "    "))
print("  → 果然就是第 12 节那个形状：try/yield/except 里 pass。**suppress 不是关键字，是 6 行 Python。**")

print("\n=== 2. suppress 会连 KeyboardInterrupt 一起咽吗？（这条决定它能不能上生产）===")
print("  issubclass(KeyboardInterrupt, Exception) →", issubclass(KeyboardInterrupt, Exception))
print("  issubclass(KeyboardInterrupt, BaseException) →", issubclass(KeyboardInterrupt, BaseException))
print("  所以 suppress(Exception) **挡不住** Ctrl-C（好），但 suppress(BaseException) 挡得住（坏）：")
with suppress(BaseException):
    raise KeyboardInterrupt("假装的 Ctrl-C")
print("  → 这行能打到，说明那个 KeyboardInterrupt 已经被吞掉；")
print("    真实场景里这会让 Ctrl-C 按下去没反应，第 3 节说过的那道「例外」被你亲手封死了。")
print("  判据：**永远不要写 suppress(BaseException)**；要咽就咽具体的类型。")

print("\n=== 3. closing(obj)：给「只有 close()、没有 __exit__」的老对象补一个 with ===")


class Legacy:
    def __init__(self):
        self.closed = False

    def close(self):
        self.closed = True
        print("    close() 被调用了")


obj = Legacy()
print("  Legacy 有 __exit__ 吗 →", hasattr(obj, "__exit__"), "｜ 有 close 吗 →", hasattr(obj, "close"))
print("  直接 with obj: 会怎样：")
try:
    with obj:
        pass
except TypeError as exc:
    print(f"      → TypeError: {exc}")
with closing(obj):
    print("  块体里在用这个对象（as 拿不到东西，closing 返回的就是 obj 本身）")
print("  出了 with：obj.closed =", obj.closed, "→ closing 替你调了 .close()")
print("  closing(obj) ≡ 一个 __exit__ 里写 obj.close() 的薄壳；标准库用它包了 urllib 的响应之类。")

print("\n=== 4. nullcontext()：占位用的 with，让两条代码路径形状一样 ===")
with nullcontext() as nothing:
    print("  nullcontext() 的 as 绑到 =", repr(nothing))
with nullcontext("塞进去的东西原样出来") as value:
    print("  nullcontext(x) 的 as 绑到 =", repr(value))
print("  典型用法：有条件才加锁/才开文件，但块体只想写一遍：")
use_file = False
with (open("/dev/null") if use_file else nullcontext()):
    print("    块体完全不知道上面到底有没有真文件 —— 这就是它的价值（第 15 节还会用它拼 EAFP）")


class Locky:
    def __init__(self):
        self.deep = 0

    def __enter__(self):
        self.deep += 1
        return self

    def __exit__(self, *a):
        self.deep -= 1


lock = Locky()
for flag in (True, False, True):
    with (lock if flag else nullcontext()):
        pass
    print(f"  走过 flag={flag} 之后 lock.deep =", lock.deep)
print("  → 不写 nullcontext 就得把块体复制两遍，或者写 `if flag: ... else: ...` 两份分支。")

print("\n=== 5. redirect_stdout / redirect_stderr：把 print 的去处换掉 ===")
buf = io.StringIO()
print("  这行在真正的 stdout 上")
with redirect_stdout(buf):
    print("  这行进 buf")
    sys.stdout.write("  这行也是（sys.stdout 被整个换了）\n")
print("  回到真 stdout，buf 里抓到了：", repr(buf.getvalue()))
print("  redirect_stdout 的返回值 =", None, "｜ 它改的是**进程全局**的 sys.stdout")
err = io.StringIO()
print("  stderr 同理，而且 traceback 默认就是走 stderr：")
sys.stdout.flush()          # 先把 stdout 排空，免得重定向期间的写入串位
with redirect_stderr(err):
    try:
        1 / 0
    except ZeroDivisionError:
        traceback.print_exc(file=sys.stderr)   # 这一刻 sys.stderr 已经是 err
print("  被 redirect 走的 stderr 最后一行 =", repr(err.getvalue().splitlines()[-1]))
print("  buf 里一共抓到", len(err.getvalue().splitlines()), "行（抬头 + 帧 + caret + 消息，和第 7 节那份一样）")
print("  ★代价：这是**全局**替换。多线程 / async 里用它会串台（别的线程也被打进你的 buf）。")
print("    多线程场景的正解是把 stream 当参数传进去（print(x, file=f)），不是 redirect。")

print("\n=== 6. 把四件东西按「谁负责收尾」排一遍 ===")
rows = [
    ("suppress(类型)", "没有收尾，只咽指定类型的异常", "拼错名/漏逻辑时静默；BaseException 版会封死 Ctrl-C"),
    ("closing(obj)", "调 obj.close()", "对象本来该自己实现协议；close 失败会覆盖原异常（第 16 节）"),
    ("nullcontext(x)", "什么都不做，as 拿到 x 或 None", "只在「有条件才真的要开」时才值得"),
    ("redirect_stdout(s)", "退出时把 sys.stdout 换回来", "进程全局，多线程/async 会串台"),
]
for name, does, cost in rows:
    print(f"  {name:<22}→ {does}")
    print(f"  {'':<22}  代价：{cost}")

print("\n=== 7. 收口 ===")
print("  这四个在源码里全是 @contextmanager 写的（第 12 节那套形状），不是 C 层的特殊待遇。")
print("  选型判据：要「忽略某类错」→ suppress；要「给老对象补协议」→ closing；")
print("    要「让分支两端形状一致」→ nullcontext；要「抓 print 的输出」→ redirect_stdout（单线程）。")
print("  数量不固定 / 运行时才知道有几个资源 → 别用这四个凑，用第 14 节 ExitStack。")
