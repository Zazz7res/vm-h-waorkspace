#!/usr/bin/env python3
# ============================================================================
# 取证脚本 c_matching.py —— 第 4 节：except 的匹配规则（顺序、元组、as、except 里的异常）
#
# 判据只有一条：except 从上往下找**第一个 isinstance 成立**的分支。
# 这一节把「顺序写错会静悄悄少收一类」演成可复现的现象。
# ============================================================================


class NetError(Exception):
    """网络类错误的父亲。"""


class Timeout(NetError):
    """儿子：超时。"""


class DNSFail(NetError):
    """儿子：域名解析失败。"""


print("=== 1. 顺序决定一切：把父亲写在前面，儿子的分支永远跑不到 ===")


def handle(exc, order):
    try:
        raise exc
    except NetError:
        branch = "父亲 NetError"
    except Timeout:
        branch = "儿子 Timeout"
    return branch


print("  raise Timeout() → 走进的分支 =", handle(Timeout(), "father-first"))
print("  → 结果是对的（父亲能接儿子），但那个专门写 Timeout 的分支**永远跑不到**，")
print("     而且不报错、不警告 —— 这是本章最容易被 code review 漏掉的一条。")

print("\n=== 2. 正确顺序：儿子在前、父亲在后，窄门先过 ===")


def handle_good(exc):
    if isinstance(exc, Timeout):
        kind = "timeout"
    elif isinstance(exc, DNSFail):
        kind = "dns"
    else:
        kind = "other-net"
    try:
        raise exc
    except Timeout:
        branch = f"儿子 Timeout 分支（{kind}）"
    except DNSFail:
        branch = f"儿子 DNSFail 分支（{kind}）"
    except NetError:
        branch = "父亲 NetError 兜底"
    return branch


for exc in (Timeout(), DNSFail(), NetError()):
    print(f"  raise {type(exc).__name__:9} → {handle_good(exc)}")

print("\n=== 3. 一个 except 收几类：元组，以及为什么不能用 `except A or B` ===")
try:
    raise ValueError("元组分支")
except (TypeError, ValueError) as exc:
    print("  except (TypeError, ValueError) as exc →", type(exc).__name__, "|", exc)
print("  写法对比（都是同一个意思，但只有第一种合法且可读）：")
print("    except (TypeError, ValueError):   ← 推荐")
print("    except TypeError, ValueError:     ← Python 2 写法，3 里是 SyntaxError")
try:
    exec("try:\n    pass\nexcept TypeError, ValueError:\n    pass\n")
except SyntaxError as exc:
    print("    实测那条老写法：", "SyntaxError |", exc.msg)

print("\n=== 4. 元组里放父亲 = 连儿子一起收；放不进去的那类原样往上抛 ===")
for exc in (Timeout(), KeyError("k"), ZeroDivisionError()):
    try:
        try:
            raise exc
        except (NetError, KeyError) as got:
            print(f"  raise {type(exc).__name__:18} → 接住了 {type(got).__name__}")
    except ZeroDivisionError as outer:
        print(f"  raise {type(exc).__name__:18} → 没接住，原样往上抛（外层才接）："
              f"{type(outer).__name__}")

print("\n=== 5. ★except 分支里再抛异常：原来那个变成「上下文」，不是原因 ===")
try:
    try:
        raise KeyError("原始的键错")
    except KeyError:
        raise TypeError("处理时又炸了一个")
except TypeError as exc:
    print("  外面接住的是：", type(exc).__name__, "|", exc)
    print("  它的 __context__ 是：", type(exc.__context__).__name__, "|", exc.__context__)
    print("  __cause__ 是：", exc.__cause__, "← 没写 raise from 就真的是 None（第 5 节细讲）")

print("\n=== 6. except 分支里的名字会被自动删掉（这是真规矩，不是回收时机）===")


def check_binding():
    try:
        raise ValueError("临时错误")
    except ValueError as exc:
        inside = "exc" in dir()
        got = repr(exc)
    after = "exc" in dir()
    return inside, got, after


inside, got, after = check_binding()
print("  except 块里能看到 exc 吗 →", inside, "| 内容 =", got)
print("  出了 except 块还在吗 →", after)
print("  原因：CPython 在离开 except 块时执行 del exc（防止引用环把帧留住）。")
print("  想留证据就自己接一份：saved = exc.args 或者 saved = str(exc)。")
saved = None


def keep():
    global saved
    try:
        raise ValueError("要留档的错误")
    except ValueError as exc:
        saved = (type(exc).__name__, str(exc))
    return saved


print("  留档之后：", keep(), "｜ 块外的 exc 存在吗 →", "exc" in dir())

print("\n=== 7. 收口 ===")
print("  ① 从上往下，第一个 isinstance 成立的分支赢；顺序错 = 静悄悄少收一类；")
print("  ② 元组收多类，父亲在元组里就连儿子一起收；")
print("  ③ except 块里再抛异常，原异常降级成 __context__（不是 __cause__）；")
print("  ④ except as 的那个名字在块结束时被删掉，别指望它活着。")
