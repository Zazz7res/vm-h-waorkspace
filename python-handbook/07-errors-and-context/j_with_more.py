#!/usr/bin/env python3
# ============================================================================
# 取证脚本 j_with_more.py —— 第 11 节：多个 with：一行、嵌套、括号写法、退出顺序
#
# 目标：把「后进先出」这条顺序律钉死，并演两个边界：第二个 enter 就崩、块体里 raise。
# ============================================================================
LOG = []


class Res:
    """登记自己进/出的资源；exit_rc 控制 __exit__ 返回值。"""

    def __init__(self, name, exit_rc=False, enter_fails=False):
        self.name = name
        self.exit_rc = exit_rc
        self.enter_fails = enter_fails

    def __enter__(self):
        if self.enter_fails:
            LOG.append(f"enter({self.name}) **失败**")
            raise OSError(f"{self.name} 打不开")
        LOG.append(f"enter({self.name})")
        return self.name.upper()

    def __exit__(self, exc_type, exc_value, tb):
        tail = "" if exc_type is None else f" ←带异常 {exc_type.__name__}"
        LOG.append(f"exit({self.name}){tail}")
        return self.exit_rc


print("=== 1. 一行写两个：enter 从左到右，exit 从右到左（后进先出）===")
with Res("A") as a, Res("B") as b:
    LOG.append(f"  块体里 a={a} b={b}")
for line in LOG:
    print("  ", line)
print("  → 顺序律和栈一样：最后一个拿到的最先还。")

print("\n=== 2. 嵌套写法和一行写法逐字节等价 ===")
LOG.clear()
with Res("A"):
    with Res("B"):
        LOG.append("  块体")
nested = list(LOG)
LOG.clear()
with Res("A"), Res("B"):
    LOG.append("  块体")
flat = list(LOG)
print("  嵌套 =", nested)
print("  一行 =", flat)
print("  两者完全一样吗 →", nested == flat)
print("  三个以上同理：with Res('A'), Res('B'), Res('C'): 的 exit 顺序是 C、B、A。")
LOG.clear()
with Res("A"), Res("B"), Res("C"):
    pass
print("  实测 =", LOG)

print("\n=== 3. 括号写法（本机 3.14 支持；长列表好换行，语义与逗号形式相同）===")
LOG.clear()
with (Res("A") as a, Res("B") as b, Res("C") as c):
    LOG.append(f"  块体里 a={a} b={b} c={c}")
print("  ", LOG[0], "｜ 括号形式和上一行那种逗号形式产出的日志 =", LOG)
print("  注意括号里**推荐带 as**。不带 as 的 `with (Res('A'), Res('B')):` 本机 3.14 也照跑（下面）：")
LOG.clear()
with (Res("A"), Res("B")):
    LOG.append("  带括号、不带 as，也当成两个上下文管理器")
print("  ", LOG)
print("  → 但不带 as 的那条**别写**。通行的说法是老解释器（3.8 及以前）会把那对括号当成一个元组表达式，")
print("    元组没有 __enter__ 直接 TypeError —— 本机只有 3.14，没有老解释器，**这条未验证**，别当事实引用。")
print("    稳妥做法就两条：要么带 as，要么干脆不加括号；括号只给「太长要换行」这一种场景用。")

print("\n=== 4. 边界一：第二个 __enter__ 就失败，第一个的 __exit__ 还跑吗？===")
LOG.clear()
try:
    with Res("OK"), Res("BAD", enter_fails=True):
        LOG.append("  这行不会打")
except OSError as exc:
    LOG.append(f"  外面接住：{exc}")
for line in LOG:
    print("  ", line)
print("  → 跑。第一个已经成功 enter 了，解释器有责任替它还；")
print("    而且看那行的尾巴：exit(OK) 收到的是**那个 OSError**（带异常 OSError），")
print("    也就是「enter 失败」这件事会以正常异常的形式穿过前面每一层 —— 层可以拿它决定要不要咽。")
print("  失败的那个（BAD）自己的 __exit__ 没跑：它连enter都没成功，日志里没有 exit(BAD)。")
LOG.clear()
with Res("SPY"):
    try:
        with Res("BAD2", enter_fails=True):
            pass
    except OSError:
        LOG.append("  内层自己把异常接住了")
print("  对照：同一件事，如果**内层自己 try 住了**，外层的 exit 就只看到正常退出 =", LOG)
print("    → __exit__ 收到的 exc_type 完全取决于「异常有没有穿出这一层的块体」，")
print("      跟块体里到底出过事无关。这是第 16 节那类静默漏 cleanup 的根源之一。")

print("\n=== 5. 边界二：块体里 raise，每层的 __exit__ 都收到异常三元组 ===")
LOG.clear()
try:
    with Res("X"), Res("Y"):
        raise ValueError("块体里炸")
except ValueError:
    LOG.append("  最外面接住了")
for line in LOG:
    print("  ", line)
print("  → 两层都标了「←带异常」：异常会**依次穿过**每个 __exit__，每层都能看到它。")
print("    哪一层返回真值，就在哪一层被截住（第 10 节），后面的层根本不会被调用。")

print("\n=== 6. 一层咽掉：中间层 return True，最内层照常、最外层不再收到 ===")
LOG.clear()
try:
    with Res("OUT"):
        with Res("MID", exit_rc=True):
            with Res("IN"):
                raise ValueError("我只会被 MID 看见")
    LOG.append("  出了 with 还在正常流程里（说明被 MID 咽了）")
except ValueError:
    LOG.append("  外面接住了（说明没咽掉）")
for line in LOG:
    print("  ", line)

print("\n=== 7. with 不产生新作用域（对照第 04 章第 16 节：推导式才是开了新作用域的那个）===")
with Res("SCOPE"):
    only_here = 42
print("  块体外还能读到 only_here =", only_here, "｜ vars() 里有没有它 →", "only_here" in vars())
print("  → 会开新作用域的只有函数（第 05 章的 LEGB）和推导式（第 04 章第 16 节实测过变量不泄漏）；")
print("    with、if、for 都和这块代码本身的层级无关，不开。类体也开一个作用域，但本手册没演它。")

print("\n=== 8. 收口 ===")
print("  ① 一行多个 with ≡ 逐层嵌套；enter 左→右，exit 右→左（栈）。")
print("  ② 第 k 个 __enter__ 失败：前 k-1 个都会被还，失败那个自己的 __exit__ 不跑（第 10 节第 5 段）。")
print("  ③ 异常从内往外穿过每一层 __exit__，遇到第一个返回真值的层就停 ——")
print("     这就是为什么「咽异常」这件事在多层 with 里特别难查：现场只剩一条日志都没有的静默。")
print("  ④ 资源数量**不定**时别用逗号堆（要写死个数），那是第 14 节 ExitStack 的地盘。")
