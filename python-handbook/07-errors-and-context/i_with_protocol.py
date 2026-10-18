#!/usr/bin/env python3
# ============================================================================
# 取证脚本 i_with_protocol.py —— 第 10 节：with 到底是什么协议（手写一遍）
#
# 目标：把 `with X() as a:` 拆成 4 步看得见的调用，并搞清 __exit__ 的返回值管什么。
# ============================================================================
import sys

SPY = []          # 用来存 __exit__ 实际收到的异常对象，好在块体外面对身份


class Tag:
    """最裸的协议实现：进叫 __enter__，退叫 __exit__，别的什么都没有。"""

    def __init__(self, name, log):
        self.name = name
        self.log = log

    def __enter__(self):
        self.log.append(f"enter({self.name})")
        return f"[{self.name}]"          # ← as 左边拿到的就是**这个返回值**

    def __exit__(self, exc_type, exc_value, tb):
        self.log.append(f"exit({self.name}) 异常={exc_type.__name__ if exc_type else None}")
        return None                       # 假值 → 异常照常往上跑


class Spy:
    """把 __exit__ 收到的三元组原样存进 SPY，出块体之后再验身份。"""

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, tb):
        SPY.append((exc_type, exc_value, tb))
        return False


def expand(log):
    """把 with 手工展开成协议调用，证明「没有魔法」。"""
    print("  手写展开 `with Tag('A', log) as a:` 的等价四步：")
    ctx = Tag("A(expand)", log)
    print("    ① 先求值 X，拿到上下文管理器对象 =", type(ctx).__name__)
    a = ctx.__enter__()
    print("    ② 调 type(ctx).__enter__(ctx)，as 左边绑的是这个返回值 =", repr(a))
    try:
        print("    ③ 执行 with 块体（这里就一行 print）")
    finally:
        r = ctx.__exit__(None, None, None)
        print("    ④ 块体正常结束时调 __exit__(None, None, None)，返回值 =", r)


log = []
print("=== 1. 协议本身：两个 dunder，鸭子类型，不要求继承任何基类 ===")
expand(log)
print("  日志 =", log)
print("  判据：能不能写进 with，只看**类字典里有没有** __enter__ / __exit__"
      "（第 06 章第 23 节就是在这里埋的钩子，本节把它补完）；")
print("        没有任何 ABC 或基类要求，所以标准库和你自己写的类在这里是平权的。")

print("\n=== 2. as 左边绑的是 __enter__ 的返回值，不是 X 本身 ===")
log = []
with Tag("B", log) as got:
    print("  as got 拿到的 =", repr(got), "｜ 类型 =", type(got).__name__)
    print("  → 是 __enter__ 里 return 的那个字符串；Tag 实例本身没有绑到任何名字上。")
print("  不写 as 也一样跑（只是拿不到那个返回值）：")
log = []
with Tag("C", log):
    print("    块体里只打了这一行")
print("  日志 =", log)

print("\n=== 3. __exit__ 收到的三个参数：正常退出时全是 None ===")
log = []
with Tag("D", log):
    pass
print("  正常退出的日志 =", log, "｜ 那条 exit 里打的 异常=None 就是三个 None 的样子")
try:
    with Spy():
        raise ValueError("块体里炸了")
except ValueError as exc:
    etype, evalue, etb = SPY[0]
    print("  异常退出时 __exit__ 收到的 = ", etype.__name__, "|", repr(evalue), "|", type(etb).__name__)
    print("  和外面 `except ... as exc` 是同一个对象吗 →", evalue is exc, "｜", etype is type(exc))
print("  → 这三样就是 sys.exc_info() 的三元组（第 7 节那份数据，换了个交付方式）；")
print("    所以「正常退出」和「异常退出」在 __exit__ 里要靠 `if exc_type is None` 分岔。")


class Swallower:
    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, tb):
        print("    __exit__ 看到异常 =", exc_type.__name__ if exc_type else None, "→ 我返回 True")
        return True


class Liar:
    """返回一个「不是 True 但是个真值」的东西，看解释器认不认。"""

    def __enter__(self):
        return self

    def __exit__(self, *args):
        print("    __exit__ 收到", args[0], "→ 我返回一个非空字符串（不是 True）")
        return "随便一个非空字符串"


print("\n=== 4. __exit__ 的返回值 = 要不要咽掉异常（本章最危险的一行代码）===")
print("  ① return True：")
with Swallower():
    raise RuntimeError("这条不会往外跑")
print("     跑到了 with 之后 —— 异常**已经消失**，除了 __exit__ 那行打印，没有任何痕迹。")
print("  ② return None / False：")
try:
    with Tag("F", []):
        raise RuntimeError("这条会往外跑")
except RuntimeError as exc:
    print("     外面接住了：", exc)
print("  ③ return 一个真值但不是 True（字符串）：")
with Liar():
    raise RuntimeError("看看这条还在不在")
print("     也跑到了 with 之后 → 解释器只看**真假**（真值就咽），不是只认 True。")
print("  判据：__exit__ 的返回值是**布尔语义**。不写值（= None）和 return False 才是照常往上抛；")
print("    return 任何真值 —— 哪怕是 'x'、(0,)、1 这些和 True 毫无关系的东西 —— 都会把异常就地抹掉。")

print("\n=== 5. __enter__ 自己抛异常：__exit__ 根本不会被调用 ===")
log = []


class EnterFails:
    def __enter__(self):
        raise OSError("enter 就失败了，资源还没拿到")

    def __exit__(self, *args):
        log.append("exit 被调用")          # 永远不会跑到


try:
    with EnterFails():
        log.append("块体")
except OSError as exc:
    print("  接住的报错 =", type(exc).__name__, ":", exc)
    print("  日志 =", log, "→ __exit__ 没跑，块体也没跑。")
print("  → 所以**清理代码不能只写在 __exit__ 里**：资源还没拿到就崩的话，没人替你收尾。")
print("    正解：__enter__ 内部自己 try/回滚；或者用第 14 节 ExitStack，拿到一样登记一样。")

print("\n=== 6. with 块里的 return / break：__exit__ 照样跑 ===")
log = []


class Fn:
    def __enter__(self):
        log.append("enter")
        return self

    def __exit__(self, *args):
        log.append("exit")
        return False


def wrapped_return():
    with Fn():
        return "块体里 return"


print("  函数返回值 =", wrapped_return(), "｜ 日志 =", log,
      "→ 退出函数前先把 with 收干净（finally 语义）")
log = []
for i in range(3):
    if i == 1:
        with Fn():
            break
print("  循环里 break 时也一样：日志 =", log)
log = []
with Fn():
    pass
print("  （对照：块体正常走完也是 enter → exit 各一次，日志 =", log, "）")

print("\n=== 7. 收口（把 with 记成一句话）===")
print("  with X() as a: body  ≡  tmp = X(); a = tmp.__enter__();")
print("                        try: body; finally: tmp.__exit__(*sys.exc_info())")
print("  它保证三件事：① 清理必然执行（return / break / 异常都跑）；② 块体崩了也释放；")
print("  ③ 拿 __exit__ 的返回值决定异常要不要继续往上跑。")
print("  此刻（块体外）sys.exc_info() =", sys.exc_info())
