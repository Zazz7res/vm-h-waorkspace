#!/usr/bin/env python3
# ============================================================================
# 取证脚本 e_raise.py —— 第 6 节：raise 的六种形态（含 assert 与 -O 现场）
#
# 判据：`raise X` 里 X 是类就现实例化，是实例就直接用，别的都是 TypeError。
# ============================================================================
import subprocess
import sys


def frames(exc):
    n, tb = 0, exc.__traceback__
    while tb is not None:
        n += 1
        tb = tb.tb_next
    return n


def frame_names(exc):
    names, tb = [], exc.__traceback__
    while tb is not None:
        names.append(tb.tb_frame.f_code.co_name)
        tb = tb.tb_next
    return names


print("=== 1. 类 / 实例 / 带参数：三种都合法，但对象不同 ===")
for label, exc in [("raise ValueError（类，现实例化）", ValueError),
                   ("raise ValueError('甲')（实例）", ValueError("甲")),
                   ("raise ValueError('乙','丙')（两个参数）", ValueError("乙", "丙"))]:
    try:
        if isinstance(exc, type):
            raise exc
        raise exc
    except ValueError as got:
        print(f"  {label:34} → str() = {str(got)!r} ｜ args = {got.args}")

print("\n=== 2. raise 一个「不能当异常」的东西：当场 TypeError ===")
for label, bad in [("字符串", "普通字符串"), ("整数", 42), ("None", None), ("普通类", object)]:
    try:
        try:
            if bad is object:
                raise object
            raise bad
        except TypeError as exc:
            print(f"  raise {label:6} → TypeError | {exc}")
        else:
            print(f"  raise {label:6} → 竟然合法")
    except Exception as exc:  # 有些是别的错，原样打出来
        print(f"  raise {label:6} → {type(exc).__name__} | {exc}")

print("\n=== 3. 裸 raise：只有一条规矩 —— 得有「正在处理的异常」 ===")


def boom():
    raise KeyError("k")


try:
    try:
        boom()
    except KeyError:
        raise  # 裸 raise：原样重抛
except KeyError as exc:
    print("  except 里裸 raise → 帧数 =", frames(exc), "| 帧名 =", frame_names(exc), "| 消息 =", exc)


def rehost():
    raise  # 这里没有正在处理的异常


try:
    rehost()
except RuntimeError as exc:
    print("  没有上下文时裸 raise → RuntimeError |", exc)

print("\n=== 4. `raise` 与 `raise exc`：同一条异常，traceback 长度不一样 ===")


def boom2():
    raise ValueError("原始")


try:
    try:
        boom2()
    except ValueError:
        raise
except ValueError as exc:
    print("  裸 raise        → 帧数 =", frames(exc), "| 帧名 =", frame_names(exc))

try:
    try:
        boom2()
    except ValueError as exc:
        raise exc  # 显式重抛同一个对象
except ValueError as exc:
    print("  raise 那个对象  → 帧数 =", frames(exc), "| 帧名 =", frame_names(exc))
print("  → 两行的差别就在「有没有把 raise 所在这一帧也记进去」。")
print("     想原样往上交就裸 raise；要改消息/换类型就 raise New(...) from exc（第 5 节）。")

print("\n=== 5. 自定义异常：父亲选谁，决定别人怎么接你 ===")


class AppError(Exception):
    """本项目的根异常：别人可以只接这一类。"""


class ConfigMissing(AppError, LookupError):
    """多继承一个内置类，让「按键错接」的人也能接住。"""


print("  AppError 的 MRO      =", " → ".join(c.__name__ for c in AppError.__mro__))
print("  ConfigMissing 的 MRO =", " → ".join(c.__name__ for c in ConfigMissing.__mro__))
for target in (AppError, LookupError, KeyError, Exception):
    print(f"  except {target.__name__:12} 能接住 ConfigMissing 吗 →",
          isinstance(ConfigMissing(), target))
print("  实测：直接 raise 一个 BaseException 的儿子也合法，")
try:
    raise KeyboardInterrupt("自己造的（只是为了看形状）")
except KeyboardInterrupt as exc:
    print("    →", type(exc).__name__, "|", exc)

print("\n=== 6. assert：它就是一个 raise，但能被 -O 整行删掉 ===")


def divide(a, b):
    assert b != 0, "除数不能为零（assert 的话）"
    return a / b


try:
    divide(1, 0)
except AssertionError as exc:
    print("  正常跑：AssertionError |", exc)
print("  等价写法（assert 只是这条语句的缩写）：")
print("    if not (b != 0): raise AssertionError('除数不能为零（assert 的话）')")
print("  同一段代码用 `python3 -O` 跑（优化模式，assert 整行不存在）：")
code = ("def divide(a, b):\n"
        "    assert b != 0, '除数不能为零'\n"
        "    return a / b\n"
        "try:\n"
        "    print('  -O 之下 divide(1,0) =', divide(1, 0))\n"
        "except AssertionError as exc:\n"
        "    print('  -O 之下仍然报：', exc)\n"
        "except ZeroDivisionError as exc:\n"
        "    print('  -O 之下 assert 消失了，撞上真正的除零：', 'ZeroDivisionError |', exc)")
proc = subprocess.run([sys.executable, "-O", "-c", code],
                      capture_output=True, text=True, check=False)
for ln in proc.stdout.splitlines():
    print(ln)
print("  → 同一份源码、只多一个 -O：assert 那一行在编译时就被扔掉了，于是除零真的发生，")
print("     报错文案从「除数不能为零（assert 的话）」换成了内置那句 'division by zero'。")
print("     判据不是「assert 会失效」这么轻：**你写的校验可以被一个命令行开关整行删除**，")
print("     所以外部输入的守卫不能放在它身上（stderr 这次是空的吗 →", proc.stderr == "", "）。")

print("\n=== 7. 判据：什么时候 assert、什么时候 raise ===")
print("  assert → 「这里不可能不成立」的内部不变量（写完就希望永远不为假）；")
print("  raise  → 「输入/环境可能不合法」的外部事实（用户给的、磁盘上的、网络来的）。")
print("  拿 assert 校验外部输入 = 用 -O 跑一次就全线失守，这是本章第三条大坑。")

print("\n=== 8. 收口 ===")
print("  ① raise 后面能跟：异常类、异常实例、什么都不跟（裸 raise）；别的都 TypeError。")
print("  ② 裸 raise 只在「正在处理异常」的地方有效，否则 RuntimeError: No active exception to re-raise。")
print("  ③ 要原样往上交就裸 raise（帧数最少）；要换说法就 `raise New(...) from exc`。")
print("  ④ assert 是 raise 的缩写，而且能被 -O 删掉 —— 别用它守外部输入。")
