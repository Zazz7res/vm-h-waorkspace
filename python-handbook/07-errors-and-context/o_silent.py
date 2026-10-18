#!/usr/bin/env python3
# ============================================================================
# 取证脚本 o_silent.py —— 第 16 节：静悄悄杀手（那些**不报错**的错）
#
# 目标：本节每条都要能「演给人看」，因为这类坑的共同特征是：现场什么痕迹都没有。
# ============================================================================
import io
import logging
import subprocess
import sys

print("=== 1. 裸 except：连退出信号一起咽 ===")


def bare(items):
    out = []
    for x in items:
        try:
            out.append(1 / x)
        except:                      # noqa  ← 没写类型 = 接 BaseException
            out.append("跳过")
    return out


print("  正常用法（接除零）：bare([1, 0, 2]) =", bare([1, 0, 2]))


def bare_signal():
    try:
        raise SystemExit("进程该在这里退出")
    except:                          # noqa
        return "SystemExit 被裸 except 咽掉了，函数正常返回"


print("  把 SystemExit 换成给它看：", bare_signal())
print("  为什么这是安全洞：`except:` 同时接住了 KeyboardInterrupt / SystemExit / GeneratorExit，")
print("    也就是第 3 节那条「Ctrl-C 不是 Exception」的例外被你**反向**封死了 —— Ctrl-C 按了没反应。")
print("  判据：**永远写 `except 具体类型:`**；要接住一切就明写 `except BaseException:`，")
print("    写出来是让读代码的人知道你是故意的，而不是打漏了一个类型名。")

print("\n=== 2. 循环里吞异常：数据少了一半，程序说它跑成功了 ===")
raw = ["1", "2", "三", "4", "五", "6"]


def convert_silent(items):
    out = []
    for s in items:
        try:
            out.append(int(s))
        except ValueError:
            pass                 # ← 最常见的「我知道这里有脏数据」写法
    return out


def convert_loud(items):
    out = []
    bad = []
    for s in items:
        try:
            out.append(int(s))
        except ValueError as exc:
            bad.append((s, str(exc)))
    return out, bad


got = convert_silent(raw)
print("  输入", len(raw), "条 → 静默版产出", len(got), "条 =", got)
print("    程序退出码 0、没有任何输出告诉你少了", len(raw) - len(got), "条。这就是第 1 类静默。")
kept, dropped = convert_loud(raw)
print("  记账版：产出", kept, "｜ 丢掉的及原因 =", dropped)
print("  判据：可以咽异常，**不可以丢掉证据**。要么记账（像上面），要么让它往上跑。")

print("\n=== 3. 只存 str(exc)：日志里只剩一个键名，谁都找不到是哪一行 ===")


def build(cfg):
    return cfg["rate"] * 2


LOGS = io.StringIO()
handler = logging.StreamHandler(LOGS)
handler.setFormatter(logging.Formatter("%(levelname)s %(name)s %(message)s"))
root = logging.getLogger("py07-demo")
root.handlers[:] = [handler]
root.setLevel(logging.DEBUG)
root.propagate = False

try:
    build({"speed": 3})
except KeyError as exc:
    root.warning("只存了消息：%s", exc)
    first = exc  # except as 的名字出了块会被 del（第 4 节讲过），所以这里先存一份
try:
    build({"speed": 3})
except KeyError as exc:
    root.exception("存了 exc_info（exception() 自动带上）")
lines = LOGS.getvalue().splitlines()
print("  日志一共", len(lines), "行；前 3 行：")
for line in lines[:3]:
    print("    |", line)
print("  → 第一条只剩一个键名 'rate'，连是什么类型的错都看不出来；第二条带完整 traceback（帧 + 源码行）。")
print("    顺带钉一条 KeyError 的坑：str(exc) 只是**带引号的键名**，不是 \"KeyError: 'rate'\"；")
print(f"    本机现场：str(first) = {str(first)!r} ｜ repr(first) = {repr(first)!r} ｜ type(first).__name__ = {type(first).__name__!r}")
print("    所以「存 str(exc)」连类型都要靠猜，更别提行号。")
print("    `logger.exception(...)` ≡ `logger.error(..., exc_info=True)`，")
print("    在 except 块里调用才有效 —— 出了 except 块 sys.exc_info() 就是 (None, None, None)（第 7 节实测过）。")
print("  对照：`print(exc)` 和 `logging.error(str(exc))` 都丢帧；`traceback.print_exc()` 不丢但走 stderr。")

print("\n=== 4. 「顺手换个说法」把根因挤到背景里（第 5 节的链在这里的代价）===")
code = """
def read(path):
    try:
        return open(path).read()
    except OSError:
        raise RuntimeError('配置读不到')     # ← 忘了 from

read('/肯定不存在/x')
"""
proc = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, check=False)
tail = proc.stderr.rstrip("\n").splitlines()
print("  最后一行 =", tail[-1], "｜ returncode =", proc.returncode)
print("  中间那句连接语 =", [l for l in tail if "another exception occurred" in l])
print("  → 只看最后一行会以为错在「读配置这件事没做成」，其实根因（No such file）在**上面那一段**。")
print("    写了 `from exc` 的话，连接语会变成 'The above exception was the direct cause...'（第 5 节）。")

print("\n=== 5. 顶层把异常换成「出错了」+ 退出码 0：CI 看不见的失败 ===")
cli = """
import sys
def main():
    1 / 0
try:
    main()
except Exception as exc:
    print('出错了：', exc)
    sys.exit(0)          # ← 「我处理过了」= 骗 CI
"""
proc = subprocess.run([sys.executable, "-c", cli], capture_output=True, text=True, check=False)
print("  stdout =", repr(proc.stdout), "｜ stderr =", repr(proc.stderr),
      "｜ returncode =", proc.returncode)
print("  → 这条最贵：脚本、CI、make、xargs 全都在看退出码（bash 手册第 3 章整章讲这个）。")
print("    打印了「出错了」却 exit(0)，等于向上游宣布成功。要么非零退出，要么别接。")
cli_ok = cli.replace("sys.exit(0)", "sys.exit(1)")
proc = subprocess.run([sys.executable, "-c", cli_ok], capture_output=True, text=True, check=False)
print("  改成 exit(1)：returncode =", proc.returncode, "｜ stdout =", repr(proc.stdout))

print("\n=== 6. 三种「看不见的清理失败」，同一族 ===")
print("  ① finally 里抛：把块体的异常顶掉（第 2 节第 6 段演过）→ 现场只剩收尾那条报错。")
print("  ② __exit__ 返回真值：异常就地消失（第 10 节第 4 段）。")
print("  ③ __exit__ 里自己抛：块体成功的结果被覆盖 / 块体失败的根因被顶掉（第 14 节第 4 段）。")
print("  → 三条的共同点：**都不影响下一行照常执行**。所以唯一可靠的排查动作是")
print("    「把可疑的那一层临时改成 raise / 打日志」，而不是继续读代码。")

print("\n=== 7. 本章推荐的五条纪律（每条都回指上面某一段）===")
rows = [
    ("接得窄", "只接你真的能处理的那一两个类型；接 BaseException / 裸 except 一律当 bug（第 1 段）"),
    ("要么记要么抛", "绝不写 except: pass；咽掉时至少留下 (数据, 原因) 的账（第 2 段）"),
    ("日志带 exc_info", "logging.exception 或 exc_info=True；只存 str(exc) 等于把帧扔掉（第 3 段）"),
    ("换说法就写 from", "raise New(...) from exc；跨层包装时连接语是给下一个读日志的人留的路标（第 4 段）"),
    ("失败要给非零退出码", "CLI 顶层接了异常就必须 sys.exit(非 0)（第 5 段）"),
]
for rule, detail in rows:
    print(f"  · {rule} —— {detail}")

print("\n=== 8. 收口 ===")
print("  报错不是代价，**静默才是**。一条会炸的 except 是好事：它把问题交到你眼前。")
print("  上面 1..7 段没有一段是「想不到的刁钻用法」，全是日常代码里最常见的写法 ——")
print("  这正是它们危险的原因：那种代码逐行读下来，每一行都像是负责任的写法。")
