#!/usr/bin/env python3
# ============================================================================
# 取证脚本 a_minimal_shape.py —— 第 2 节：try/except/else/finally 的走位
#
# 目的：把「一条 try 语句里哪几行会跑、哪几行会被跳过」全部打成可见的行号。
#      很多人背了「else 是没异常才走」，但没意识到 except 接住之后
#      函数会**像什么都没发生一样**继续往下跑。
# 输出确定性：没有任何会变的东西（无时间、无地址、无随机）。
# ============================================================================

print("=== 1. 一条 try 语句的两种走法（每一步都打一行）===")


def run_a(ok):
    try:
        print("    [1] try 块开始")
        if not ok:
            raise ValueError("故意炸")
        print("    [2] try 块结尾（异常之后这行不跑）")
    except ValueError as exc:
        print(f"    [3] except 接住了：{exc}")
    else:
        print("    [4] else：只有 try 里一行都没炸才走这里")
    finally:
        print("    [5] finally：无论如何都走")
    print("    [6] 整条 try 语句之后的第一行")
    return "正常收尾"


print("  不炸的走法：")
print("  返回值 =", run_a(True))
print("  炸的走法：")
print("  返回值 =", run_a(False))

print("\n=== 2. except 类型不对：接不住，但 finally 照跑，后面的行不跑 ===")


def wrong_type(ok):
    try:
        print("    [1] try 块开始")
        if not ok:
            raise KeyError("我是 KeyError，ValueError 那条 except 接不住")
        print("    [2] try 块结尾")
    except ValueError:
        print("    [3] except（只收 ValueError）—— 不会打这行")
    else:
        print("    [4] else —— 也不会打这行")
    finally:
        print("    [5] finally —— 这行还是会打")
    print("    [6] 之后的行 —— 不跑了")
    return "到不了这里"


try:
    wrong_type(False)
except KeyError as exc:
    print("  外面接住了：", type(exc).__name__, "|", exc)

print("\n=== 3. 实战最常见的一种：except 里给个兜底返回值 ===")


def parse(s):
    try:
        return int(s)
    except ValueError:
        return None


print("  parse('42') =", parse("42"), "｜ parse('四十二') =", parse("四十二"))
print("  → 这就是第 05 章第 12 节那条坑的另一面：调用方分不清「值就是 None」和「转换失败」，")
print("     要么返回 (成功, 值) 元组，要么**让它抛出去**由调用方 except（见第 5、6 节）。")

print("\n=== 4. try 里 return，finally 依然执行 ===")


def with_return(s):
    try:
        value = int(s)
        return value
    except ValueError:
        return -1
    finally:
        print("    finally 跑了（return 的值先存好，但函数还没真正离开）")


print("  with_return('7') =", with_return("7"))
print("  with_return('x') =", with_return("x"))

print("\n=== 5. ★finally 里自己 return：把异常整个吞掉，现场一点痕迹都没有 ===")
# 这段代码**故意**写成字符串再 compile：直接写在文件里会在编译期就往 stderr 打一条
# SyntaxWarning（带本机绝对路径），既污染顺序也泄露路径。第 21 节解释这条警告为什么值得听。
SWALLOW_SRC = (
    "def swallow():\n"
    "    try:\n"
    "        raise ValueError('这句本该被看见')\n"
    "    finally:\n"
    "        return 'from finally'\n"
)
import warnings

with warnings.catch_warnings(record=True) as caught:
    warnings.simplefilter("always")
    ns = {}
    exec(compile(SWALLOW_SRC, "<示例>", "exec"), ns)
    print(f"  编译那一刻解释器主动发出 {len(caught)} 条警告：")
    for w in caught:
        print(f"    {w.category.__name__}: {w.message}")
print("  跑它：swallow() =", ns["swallow"](), "← 异常没了，一个字的报错都没打")


def visible():
    try:
        raise ValueError("这句会被看见")
    finally:
        print("    finally 只 print，不 return")


try:
    visible()
except ValueError as exc:
    print("  对照：把 finally 的 return 换掉，异常就照常抛出去 →", "ValueError |", exc)

print("\n=== 6. 从 try 里 break / continue 出去，finally 也照样执行 ===")
for i in range(3):
    try:
        if i == 1:
            break
        continue
    finally:
        print("    第", i, "轮的 finally")
print("  break 那一轮的 finally 也跑了（第 0 轮是 continue，第 2 轮根本没开始）。")

print("\n=== 7. 收口：一句话判据 ===")
print("  try 只决定「异常从哪一行开始往外找 except」；")
print("  else = try 没炸才走；finally = 离开这条 try 语句那一刻**必执行的尾巴**，")
print("  不管是正常走完、return、break，还是正在往外抛 —— 都先把它跑完。")
print("  而 finally 里的 return / raise 会覆盖一切 —— 这正是「吞异常」的机制。")
