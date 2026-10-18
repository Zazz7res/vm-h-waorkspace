#!/usr/bin/env python3
# ============================================================================
# 取证脚本 m_exitstack.py —— 第 14 节：ExitStack：资源个数运行时才知道
#
# 目标：动态登记、后进先出、pop_all 转手，以及「收尾时又抛一个异常」会怎样。
# ============================================================================
import contextlib
from contextlib import ExitStack, suppress

LOG = []


class Res:
    def __init__(self, name):
        self.name = name

    def __enter__(self):
        LOG.append(f"enter({self.name})")
        return self.name.upper()

    def __exit__(self, exc_type, exc_value, tb):
        tail = "" if exc_type is None else f" ←带{exc_type.__name__}"
        LOG.append(f"exit({self.name}){tail}")
        return False


print("=== 1. 为什么需要它：资源个数写在代码里就输了 ===")
names = ["db", "cache", "log", "metrics"]      # 真实场景：这个列表来自配置，长度不固定
print("  资源清单 =", names, "｜ 长度 =", len(names))
print("  用逗号堆 with 就得写死个数（第 11 节）；用递归写又不像人话。ExitStack 的写法：")
LOG.clear()
with ExitStack() as stack:
    for name in names:
        stack.enter_context(Res(name))          # ← 拿到一个就登记一个
    LOG.append("  块体（业务代码在这里跑）")
for line in LOG:
    print("  ", line)
print("  → 后进先出：enter 是 db→cache→log→metrics，exit 反过来。和第 11 节那条顺序律一模一样。")
print("    enter_context(x) 干的事就是「替你把 with x 往后写」，退出时按登记的逆序调用。")

print("\n=== 2. 只要一行收尾代码，不想为它写一个类：callback() ===")
LOG.clear()
with ExitStack() as stack:
    stack.callback(LOG.append, "exit-1")        # 参数在**登记那一刻**就绑定死
    stack.callback(LOG.append, "exit-2")
    LOG.append("  块体")
print("  ", LOG)
print("  → callback(fn, *args, **kw)：退出时调 fn(*args)。它是**栈**，所以后登记的先跑。")
print("    注意上面那两行参数是在登记时就定好的 —— 块体里再改局部变量不影响已登记的调用。")
LOG.clear()
with ExitStack() as stack:
    late = ["初始"]
    stack.callback(LOG.append, str(late))
    late.append("块体里改过")
print("  对照（登记的参数不受块体影响）：", LOG)
print("  想要「退出时才求值」就包一个无参 lambda：stack.callback(lambda: LOG.append(str(late)))")
LOG.clear()
with ExitStack() as stack:
    late = ["初始"]
    stack.callback(lambda: LOG.append(str(late)))
    late.append("块体里改过")
print("  改成 lambda 之后：", LOG)

print("\n=== 3. 收尾顺序与异常：块体崩了，登记过的**全部**倒着还 ===")
LOG.clear()
try:
    with ExitStack() as stack:
        stack.enter_context(Res("A"))
        stack.enter_context(Res("B"))
        stack.callback(LOG.append, "callback-C")
        raise ValueError("块体炸了")
except ValueError:
    LOG.append("  外面接住")
for line in LOG:
    print("  ", line)
print("  → 每个 exit 都带上了那个 ValueError（因为异常正在往外跑）。")
print("    这比 finally 好在哪里： finally 只能写在一层里，ExitStack 能跨函数收集（下面第 5 段）。")

print("\n=== 4. 收尾时自己也抛：两个异常怎么共存（第 5 节的链接在这里第二次咬人）===")


class Boom:
    def __enter__(self):
        return self

    def __exit__(self, *a):
        raise OSError("收尾代码自己炸了")


print("  ① 块体正常、收尾抛：")
try:
    with ExitStack() as stack:
        stack.enter_context(Boom())
        LOG.append("  块体正常结束")
except OSError as exc:
    print("     外面看到的是", type(exc).__name__, ":", exc, "（块体其实成功了，报错却是收尾那条）")
print("  ② 块体先抛 ValueError、收尾又抛 OSError：")
try:
    with ExitStack() as stack:
        stack.enter_context(Boom())
        raise ValueError("原始故障")
except (ValueError, OSError) as exc:
    print("     外面看到的是", type(exc).__name__, ":", exc)
    print("     __context__ =", type(exc.__context__).__name__ if exc.__context__ else None,
          "｜ __cause__ =", type(exc.__cause__).__name__ if exc.__cause__ else None)
print("  → **原始异常被丢掉，只剩收尾那条**，原始故障降级成 __context__ 里的背景。")
print("    这是所有 finally / __exit__ / callback 共同的坑，不是 ExitStack 独有的；")
print("    判据：看到报错时先翻 __context__，「原始故障」常常藏在那儿（第 5 节那条判据的复用）。")
print("  → 怎么防：收尾代码自己包 try，或者改用 push()。这两个入口的区别就写在 docstring 里：")
print("     callback 的 docstring：", contextlib.ExitStack.callback.__doc__.splitlines()[2].strip())
print("     push 的 docstring     ：", [l.strip() for l in contextlib.ExitStack.push.__doc__.splitlines()
                                        if "suppress" in l][0])


def tolerant_cleanup():
    try:
        raise OSError("收尾里的事没办成")
    except OSError:
        LOG.append("  收尾自己把异常吃了（块体的异常因此保住）")


def strict_cleanup():
    raise OSError("收尾里的事没办成")


def swallowing_exit(exc_type, exc, tb):
    LOG.append(f"  push 进去的函数看到 {exc_type.__name__}，返回真值 = 咽掉")
    return True


LOG.clear()
try:
    with ExitStack() as stack:
        stack.callback(tolerant_cleanup)
        raise ValueError("块体的真故障")
except ValueError as exc:
    LOG.append(f"  外面接住：ValueError: {exc}")
print("     ", LOG, "→ 收尾不抛，块体那条 ValueError 才是你看到的报错（这就是想要的行为）。")
LOG.clear()
try:
    with ExitStack() as stack:
        stack.callback(strict_cleanup)
        raise ValueError("块体的真故障")
except (ValueError, OSError) as exc:
    LOG.append(f"  外面看到的是 {type(exc).__name__}: {exc}"
               f" ｜ __context__ = {type(exc.__context__).__name__ if exc.__context__ else None}")
print("     ", LOG, "→ 收尾一抛，块体的故障就被顶替了；callback **没有**任何吞异常的开关。")
LOG.clear()
with ExitStack() as stack:
    stack.push(swallowing_exit)
    raise ValueError("这条被 push 的函数咽掉了")
print("     ", LOG, "→ push 收的是 __exit__ 签名的函数，返回值真值就能咽异常。")

print("\n=== 5. pop_all()：把「谁负责关」整个转手出去（经典：函数返回一个还在用的资源）===")
LOG.clear()
inner = ExitStack()
inner.enter_context(Res("X"))
print("  pop_all 之前 inner 手里有", len(inner._exit_callbacks), "个收尾")
transferred = inner.pop_all()
print("  pop_all 之后：inner 剩", len(inner._exit_callbacks),
      "｜ 接手的那个 stack 有", len(transferred._exit_callbacks), "个")
with transferred:
    LOG.append("  块体")
print("  ", LOG, "→ 谁 with 这个 stack，谁负责还；和「资源是谁创建的」脱钩了。")


def acquire(names):
    stack = ExitStack()
    handles = [stack.enter_context(Res(n)) for n in names]
    return handles, stack.pop_all()        # ← 关键：把登记好的收尾责任**打包带走**


LOG.clear()
handles, owned = acquire(["conn", "cursor"])
print("  函数已经返回，日志 =", LOG, "→ 资源**还开着**（登记项搬进了 owned，函数栈帧退了也不放）")
with owned:
    LOG.append("  使用之中（这才是调用方该写的 with）")
print("  ", LOG)


def acquire_leaky(names):
    with ExitStack() as stack:             # ← 忘了 pop_all 的下场
        return [stack.enter_context(Res(n)) for n in names]


LOG.clear()
acquire_leaky(["only"])
print("  忘 pop_all 时：", LOG, "→ return 那一刻 with 就退出，资源在调用方拿到之前就还掉了")
print("  → pop_all 的意义：**收尾责任是个可以传手的值**。第 18 节写临时文件就用这一手：")
print("    函数返回文件名，「负责删」这一项交给调用方的 stack。")

print("\n=== 6. 拿 ExitStack 拼一个「能吞也能放」的可组合块 ===")


def bad_job():
    raise ValueError("坏输入")


def run(job, tolerant):
    with ExitStack() as stack:
        if tolerant:
            stack.enter_context(suppress(ValueError))   # suppress 本身就是个上下文管理器
        job()
        return "跑完了"


for tolerant in (False, True):
    try:
        out = run(bad_job, tolerant)
        print(f"  tolerant={tolerant} → {out}")
    except ValueError as exc:
        print(f"  tolerant={tolerant} → 抛出来了：{exc}")
print("  → 这就是「要不要容错」变成一个**参数**、而不是把块体复制两份的写法。")
print("    注意 tolerant=True 那行拿到的是 **None** 不是「跑完了」：suppress 咽掉异常的位置在 with 退出时，")
print("    也就是 job() 之后的 `return` 那行**根本没跑到**（第 12 节：yield 之后的代码不执行）。")
print("    代价：读的人得知道你往里塞的是哪个上下文管理器 —— 只在真要开关时用。")

print("\n=== 7. 收口 ===")
print("  ① 资源个数写不死 → ExitStack + enter_context；只要一行收尾 → callback（参数登记时就绑死）。")
print("  ② 退出顺序永远是登记顺序的**逆序**，和逗号堆 with / 嵌套 with 同一条律。")
print("  ③ 收尾代码抛异常会**盖住**块体的异常，原始故障只在 __context__ 里 —— 这不是 ExitStack 的锅，")
print("     finally / __exit__ 全一样；所以收尾要么不抛，要么自己 suppress 掉（第 16 节专讲这个坑）。")
print("  ④ pop_all 让「谁负责关」变成可以传手的值 —— 这是 raise ... from 之外")
print("     第二种把 cleanup 当数据流动的方式（第 18 节写文件时用它收临时文件）。")
