#!/usr/bin/env python3
# ============================================================================
# 取证脚本 d_chain.py —— 第 5 节：异常链 __cause__ / __context__ / raise from
#
# 这一节回答的是「报错里那段 The above exception was ... 到底哪来的」。
# 答案：解释器一直在替你记账，__cause__ 是你显式写的，__context__ 是它自动记的。
# ============================================================================
import subprocess
import sys

print("=== 1. 显式链：raise ... from ... ===")


def load_number(text):
    try:
        return int(text)
    except ValueError as exc:
        raise ValueError(f"配置项要的是数字，拿到的是 {text!r}") from exc


try:
    load_number("四十二")
except ValueError as exc:
    print("    最外层看到：", exc)
    print("    __cause__   =", repr(exc.__cause__), "← 你写的「原因」")
    print("    __context__ =", repr(exc.__context__))
    print("    __suppress_context__ =", exc.__suppress_context__)

print("\n=== 2. 隐式链：没写 from，原异常自动降级成 __context__ ===")


def risky():
    try:
        return {}["缺键"]
    except KeyError:
        return 1 / 0


try:
    risky()
except ZeroDivisionError as exc:
    print("    __cause__   =", repr(exc.__cause__), "← 没写 from 就真的是 None")
    print("    __context__ =", repr(exc.__context__))
    print("  → 所以「处理时又炸了一个」不会丢现场：两个都在对象上。")

print("\n=== 3. 一条链打印出来长什么样（整份原文，跑在子进程里）===")
print("  为什么要跑到子进程里打：在本进程打 traceback 会带上**本机绝对路径**，")
print("  放进 `<string>` 里则只有行号。取证要的是形状，不是我这台机器的目录。")
EXPLICIT_SRC = (
    "import sys, traceback\n"
    "def make_chain():\n"
    "    try:\n"
    "        raise ValueError('根因')\n"
    "    except ValueError as exc:\n"
    "        raise RuntimeError('外层说法') from exc\n"
    "try:\n"
    "    make_chain()\n"
    "except RuntimeError as exc:\n"
    "    traceback.print_exception(exc, file=sys.stdout)\n"
)
proc = subprocess.run([sys.executable, "-c", EXPLICIT_SRC], capture_output=True, text=True, check=False)
chain_text = proc.stdout
print(chain_text.rstrip("\n"))
print("  --- 上面这段的行数 =", len(chain_text.splitlines()),
      "｜ 中间那句连接语 =",
      [l for l in chain_text.splitlines() if l.startswith("The above exception")][0])
print("  --- 有几处 'Traceback (most recent call last):' →",
      chain_text.count("Traceback (most recent call last):"), "（每一段链各一个抬头）")

print("\n=== 4. 隐式链的打印形状：那句连接语不一样 ===")
IMPLICIT_SRC = (
    "import sys, traceback\n"
    "def risky():\n"
    "    try:\n"
    "        return {}['缺键']\n"
    "    except KeyError:\n"
    "        return 1 / 0\n"
    "try:\n"
    "    risky()\n"
    "except ZeroDivisionError as exc:\n"
    "    traceback.print_exception(exc, file=sys.stdout)\n"
)
proc = subprocess.run([sys.executable, "-c", IMPLICIT_SRC], capture_output=True, text=True, check=False)
lines = proc.stdout.splitlines()
print("  连接语原文 =", [l for l in lines if "another exception occurred" in l][0])
print("  整段行数 =", len(lines), "｜ 抬头出现次数 =", proc.stdout.count("Traceback (most recent call last):"))
print("  → 两段形状一模一样，**只有中间那句话不同**；所以查日志时先找那句连接语，")
print("    它告诉你「这个链是有人显式写的，还是处理时又炸了一次」。")

print("\n=== 5. raise ... from None：把上下文藏起来（标准库自己就这么干）===")


def convert(text):
    try:
        value = {"甲": 1}[text]
        return int(value)
    except (KeyError, TypeError) as exc:
        raise ValueError("必须是甲") from None


try:
    convert("乙")
except ValueError as exc:
    print("    报出来的只有一句：", exc)
    print("    __context__ 对象还在吗 →", repr(exc.__context__), "（在，只是不打印）")
    print("    __suppress_context__ =", exc.__suppress_context__)
print("  判据：`from None` 不是「不产生链」，是「不把链打给你看」。调试时可以把")
print("  __suppress_context__ 改回 False 再打一遍，现场其实一个字都没丢。")

print("\n=== 6. __traceback__ 是一个链表，帧号能直接读出来 ===")


def make_chain():
    try:
        raise ValueError("根因")
    except ValueError as exc:
        raise RuntimeError("外层说法") from exc


def depth_and_lines(exc):
    tb, frames, lines = exc.__traceback__, 0, []
    while tb is not None:
        frames += 1
        lines.append(tb.tb_lineno)
        tb = tb.tb_next
    return frames, lines


try:
    make_chain()
except RuntimeError as exc:
    frames, lines = depth_and_lines(exc)
    print("  最外层异常的帧数 =", frames, "| 各帧所在的源码行号 =", lines)
    print("  每一帧上能拿到的东西 =", [a for a in dir(exc.__traceback__) if a.startswith("tb_")])
    print("  __cause__ 自己也有 traceback：帧数 =",
          depth_and_lines(exc.__cause__)[0])

print("\n=== 7. with_traceback：把「别处的现场」挂到新异常上（库作者的工具，谨慎）===")
try:
    try:
        raise ValueError("真实发生的错")
    except ValueError as exc:
        new = TypeError("换个说法重抛")
        new = new.with_traceback(exc.__traceback__)
        raise new
except TypeError as exc:
    print("  拿到的是：", type(exc).__name__, "|", exc)
    print("  但它的 traceback 来自上面那条 ValueError —— 帧数 =",
          depth_and_lines(exc)[0])
print("  实际项目里几乎只有「重新包装异常」时才用；能用 from 就别动 traceback。")

print("\n=== 8. 收口 ===")
print("  ① __cause__ 只由 `raise ... from X` 写入；__context__ 由解释器在处理异常期间")
print("     又抛新异常时自动写入 —— 两者可以都在，打印时优先 __cause__。")
print("  ② 看到 `The above exception was the direct cause of ...` = 有人显式写了 from；")
print("     看到 `During handling of the above exception, another exception occurred` = 隐式链。")
print("  ③ 异常对象本身带着 __traceback__ 链表，帧号是读出来的，不是只有打印才有。")
