#!/usr/bin/env python3
# ============================================================================
# 取证脚本 f_pitfalls.py —— 第 11 章第 6 篇：六个最常见的「我以为我会并发」
#
# 要证明什么（全是本机能当场炸出来的现场，不是警告文案）：
#   ① 信号只能在主线程装 —— 子线程里 `signal.signal(...)` 抛 ValueError（原文照抄）；
#   ② 已经有一个循环在跑了，循环里再 `asyncio.run(...)` 抛 RuntimeError（原文照抄），
#      顺带漂出「coroutine ... was never awaited」—— 那是 run 拒绝之后没人 await 它，
#      属于现场的一部分，本篇明说它不是判据；
#   ③ 闭包 / 局部函数 / lambda / 局部类进不了进程池：本机出现的是 `PicklingError: Can't
#      pickle local object ...`（地址折掉），线程池那边一声不响照样跑 —— 差别就在要不要跨进程；
#   ④ 线程数 != 并行度：8 条线程（= 2 倍核数）确实能同时等外部，但拿去跑纯计算时
#      比 2 条线程一点没快，因为「同时执行字节码」的那个数还是 1；
#   ⑤ 进程数 != 加速倍数：100 份活丢进 max_workers=2 的池，真干活的只有 2 个进程；
#      worker 从 4 加到 8（活还是 4 份）也不见得快；
#   ⑥ 有 GIL != 线程安全：`list.append` 一条不丢，同一份「读-写」中间让出一次就必丢，
#      加锁又一条不丢 —— 原子性不是「大概率没事」；
#   ⑦ queue 收尾要靠哨兵，哨兵少放一个就有一个 worker 永远等在那儿；
#      空队列上 `get_nowait()` 抛的是公开名 queue.Empty（本机它的全名印出来是
#      `_queue.Empty`，因为 queue.py 就是从 C 模块 _queue 里导出的同一个类），str 是空串。
#
# demo.sh 里这样调：`python3 f_pitfalls.py`
#
# 安全说明：不落一个文件、不起游离子进程（进程池由解释器管，随 with 退出）、不联网、
#   不开任何监听端口；起的每个线程都 join、每个池都 with 退出，最后一格当场数
#   「还活着的线程 / 还活着的子进程」为 0，并且先故意少放一个哨兵让那条对账响一次；
#   第 1 格在主线程装的信号处理函数当场装回原样（最后一格再核对一次）。
# 屏幕上不出现：时长读数、pid、内存地址、丢更新之后的具体计数。
#   「丢了吗 / 一条不丢吗 / 快过 20% 吗 / 个数等于几」这些判定才有资格上屏，阈值都带大余量。
# ★ 本机不拿什么当证据：契约里预告的那句 `AttributeError: Can't get local object ...`
#   在本机对闭包与局部函数**没有出现**（本机出现的是 PicklingError，第 3 格原样印），
#   所以本篇不写那句措辞；不写「GIL 下 `+=` 一定会丢」—— 本机两种口径都测出不丢，
#   写了就是编；不写「3.14 能去掉 GIL」「装个 uvloop 就好」—— 当场探：没有 free-threading
#   解释器、没有 uvloop、也没有 pip。
# 报错原文照抄，只折地址与路径；退出码恒 0；连跑三遍输出逐字一致。本机 CPython 3.14.4。
# ============================================================================
import asyncio
import concurrent.futures as cf
import gc
import importlib.util
import multiprocessing as mp
import os
import pickle
import queue
import re
import signal
import sys
import sysconfig
import threading
import time
import warnings

CELL = {"n": 0, "ok": 0}
TOTAL = {"n": 0, "ok": 0}
CUNIT = 8_000_000        # 一份 CPU 活的规模（够盖住调度噪声）
JOBS4 = 4                # 每轮几份活
WORKERS7 = 3             # 第 7 格几个工人
BOX = {}                 # 第 6 格被 4 条线程同时改的字典
ORIGINAL_SIG = signal.getsignal(signal.SIGUSR1)     # 第 1 格收尾要装回它
PICKLE_LOCAL_FN = ("PicklingError: Can't pickle local object "
                   "<function make_local_fn.<locals>.local_fn at <地址>>")


def mask(text):
    out = re.sub(r"0x[0-9a-fA-F]+", "<地址>", str(text))
    cwd = str(os.getcwd())
    for root, tag in ((cwd + os.sep, ""), (sys.base_prefix + os.sep, "<pyroot>" + os.sep),
                      (os.path.expanduser("~") + os.sep, "<家>" + os.sep)):
        if root:
            out = out.replace(root, tag)
    return out


def eq(tag, th, want):
    """判据行：实测与预期逐字比对，不成立当场印 [不成立！]（恒真的守卫比没有更糟）。"""
    try:
        got = th()
    except BaseException as exc:
        got = f"{type(exc).__name__}: {mask(exc)}"
    ok = got == want
    CELL["n"] += 1
    CELL["ok"] += 1 if ok else 0
    TOTAL["n"] += 1
    TOTAL["ok"] += 1 if ok else 0
    return f"  [{'成立' if ok else '不成立！'}] {tag}：实测 {got!r} ｜ 预期 {want!r}"


def ledger(title):
    return (f"  → 断言：{title} ｜ 本格判据 {CELL['ok']}/{CELL['n']} 条成立"
            f"（少一条上面就有 [不成立！]）")


def reset():
    CELL["n"] = CELL["ok"] = 0


def fastest(fn, k=3):
    """同一件事测 k 次取最快那次：把「别的程序正在抢 CPU」的噪声压下去（数值不上屏）。"""
    best = None
    for _ in range(k):
        t0 = time.perf_counter()
        fn()
        dur = time.perf_counter() - t0
        if best is None or dur < best:
            best = dur
    return best


# ---- 第 1 格 ----
def try_signal_in_thread(out):
    try:
        signal.signal(signal.SIGUSR1, lambda *a: None)
        out.append("居然没炸")
    except BaseException as exc:
        out.append(f"{type(exc).__name__}: {mask(exc)}")


# ---- 第 2 格：协程放模块顶层，警告里的名字才稳定可读 ----
async def never_awaited():
    await asyncio.sleep(0.01)
    return 1


# ---- 第 3 格：工厂在模块顶层（可 pickle），它返回的东西住在局部命名空间（不可 pickle）----
def make_local_fn():
    def local_fn(x):
        return x
    return local_fn


def make_closure(y):
    def inner(z):
        return z + y
    return inner


def make_local_class():
    class Local:
        pass
    return Local


def noop(x):
    return x


def error_text(th):
    """跑一句，把「报错原文」取回来；没炸就回一个标记串。"""
    try:
        th()
        return "没炸"
    except BaseException as exc:
        return f"{type(exc).__name__}: {mask(exc)}"


def class_of_error(th):
    """跑一句，把「抛出来那个类」原样取回来（比按文字猜种类可靠）。"""
    try:
        th()
        return "没炸"
    except BaseException as exc:
        return type(exc)


def dump_probe(fn):
    return error_text(lambda: pickle.dumps(fn))


def submit_probe(fn):
    with cf.ProcessPoolExecutor(max_workers=2) as ex:
        return error_text(lambda: ex.submit(fn, 1).result(10))


def run_in_thread(fn, arg):
    with cf.ThreadPoolExecutor(max_workers=1) as ex:
        return ex.submit(fn, arg).result(10)


def run_in_probe_top():
    with cf.ProcessPoolExecutor(max_workers=2) as ex:
        return ex.submit(noop, 42).result(10)


# ---- 第 4、5 格的工人 ----
def cpu(n):
    total = 0
    for i in range(n):
        total += i
    return total


def pid_of(x):
    return os.getpid()


def sleep_then_count_threads(d):
    time.sleep(d)
    return threading.active_count()


def thread_round(workers):
    with cf.ThreadPoolExecutor(max_workers=workers) as ex:
        return fastest(lambda: list(ex.map(cpu, [CUNIT] * JOBS4)))


def process_round(workers):
    with cf.ProcessPoolExecutor(max_workers=workers) as ex:
        return fastest(lambda: list(ex.map(cpu, [CUNIT] * JOBS4)))


# ---- 第 6 格的几种写法 ----
def append_many(lst, n):
    for _ in range(n):
        lst.append(1)


def yield_bump(key, n):
    for _ in range(n):
        cur = BOX[key]           # 读
        time.sleep(0)            # 让出一次（就一次）
        BOX[key] = cur + 1       # 写


def locked_bump(key, n, lk):
    for _ in range(n):
        with lk:
            cur = BOX[key]
            time.sleep(0)
            BOX[key] = cur + 1


def plus_equal(key, n):
    for _ in range(n):
        BOX[key] += 1            # 不让出：本机丢不丢由第 6 格当场数，别照抄别人的结论


def run_threads(target, args_for):
    ts = [threading.Thread(target=target, args=args_for(i)) for i in range(JOBS4)]
    for t in ts:
        t.start()
    for t in ts:
        t.join()


# ---- 第 7 格 ----
def sentinel_worker(q, out):
    while True:
        item = q.get()
        if item is None:                 # 哨兵 = 「没活了，收工」
            return
        out.append(item)


def cell1():
    print("=== 1. 误区「哪条线程都能装信号处理器」：子线程当场 ValueError ===")
    # 直觉以为：signal.signal 是个普通函数，谁调都一样。屏幕上：只有主线程有资格调。
    reset()
    print(eq("当前这条是主线程吗",
             lambda: threading.current_thread() is threading.main_thread(), True))
    out = []
    t = threading.Thread(target=try_signal_in_thread, args=(out,), name="想装信号的线程")
    t.start()
    t.join()
    print(f"  子线程里 signal.signal(SIGUSR1, ...) 的报错原文 → {out[0]}")
    print(eq("上面这句 == ValueError 的原文（逐字，不改写、不翻译）", lambda: out[0],
             "ValueError: signal only works in main thread of the main interpreter"))
    same = []
    try_signal_in_thread(same)            # 同一段代码搬到主线程再跑一次
    print(eq("同一段代码在主线程里跑（对照：换个线程就合法了）", lambda: same, ["居然没炸"]))
    signal.signal(signal.SIGUSR1, signal.SIG_IGN)
    print(eq("主线程装完之后 signal.getsignal(SIGUSR1) 变成什么",
             lambda: signal.getsignal(signal.SIGUSR1), signal.SIG_IGN))
    signal.signal(signal.SIGUSR1, ORIGINAL_SIG)     # 收尾：装回原样，不给进程留东西
    print(ledger("信号处理器只有一个落脚点：主线程。子线程要响应信号，得用 queue/Event 转交"))


def cell2():
    print("=== 2. 误区「asyncio.run 什么时候都能调」：循环里套循环，RuntimeError ===")
    reset()

    async def amain():
        res = []
        for _ in range(2):                 # ★ 循环：第二次也不会好一点
            try:
                asyncio.run(never_awaited())
                res.append("居然跑通了")
            except BaseException as exc:
                res.append(f"{type(exc).__name__}: {mask(exc)}")
        return res

    with warnings.catch_warnings(record=True) as ws:
        warnings.simplefilter("always")
        res = asyncio.run(amain())         # 外层本身已经跑在一个循环里
        gc.collect()                       # 被 run 拒绝的协程是它被丢弃时举报的
        drift = sorted({f"{w.category.__name__}: {w.message}" for w in ws})
    print(f"  循环里调 asyncio.run，两次的原文 → {res[0]}")
    print(eq("两次都是同一句（「重试」治不了它）", lambda: res[1] == res[0], True))
    print(eq("这句 == RuntimeError 的原文（逐字）", lambda: res[0],
             "RuntimeError: asyncio.run() cannot be called from a running event loop"))
    print(eq("run 被拒之后协程没人 await，漂出来的警告原文（现场的一部分，不是判据）",
             lambda: drift,
             ["RuntimeWarning: coroutine 'never_awaited' was never awaited"]))
    print("  ★ 正确写法：已经在循环里就直接 `await never_awaited()`（或 create_task），"
          "别再造第二个循环。")
    print(ledger("asyncio.run 是「从同步世界进循环的那扇门」，循环里再开一扇门就是 RuntimeError"))


def cell3():
    print("=== 3. 误区「随便一个函数都能丢进进程池」：闭包与 lambda 当场 PicklingError ===")
    reset()
    local_fn = make_local_fn()
    closure = make_closure(10)
    local_cls = make_local_class()
    anon = lambda v: v                     # 挂在变量上也不改它的户口：还在 cell3 的局部命名空间
    print(eq("线程池里跑局部函数（对照：同一个进程，不用 pickle，什么函数都能跑）",
             lambda: run_in_thread(local_fn, 1), 1))
    print(eq("线程池里跑闭包（对照）", lambda: run_in_thread(closure, 1), 11))
    print(eq("pickle.dumps(局部函数) 的原文", lambda: dump_probe(local_fn), PICKLE_LOCAL_FN))
    print(eq("进程池 submit(局部函数) 的原文（同一句措辞，只是换了个入口）",
             lambda: submit_probe(local_fn), PICKLE_LOCAL_FN))
    print(eq("进程池 submit(闭包) 的原文", lambda: submit_probe(closure),
             "PicklingError: Can't pickle local object "
             "<function make_closure.<locals>.inner at <地址>>"))
    print(eq("进程池 submit(lambda) 的原文", lambda: submit_probe(anon),
             "PicklingError: Can't pickle local object "
             "<function cell3.<locals>.<lambda> at <地址>>"))
    print(eq("进程池 submit(局部类的实例) 的原文", lambda: submit_probe(local_cls()),
             "PicklingError: Can't pickle local object "
             "<class '__main__.make_local_class.<locals>.Local'>"))
    print(eq("换成模块顶层的函数就进去了（本篇所有工人写在顶层，原因就在这）",
             lambda: run_in_probe_top(), 42))
    print("  ★ 契约里预告的那句 `AttributeError: Can't get local object ...` 本机对闭包和局部"
          "函数都没出现（本机出现的是上面这几条 PicklingError），所以本篇不写那句措辞。")
    print(ledger("进程池传的是「函数的名字」不是函数本身：住在局部命名空间的东西没有名字可查"))


def cell4(m):
    print("=== 4. 误区「线程数 = 并行度」：开到 2 倍核数，纯计算一点没多跑 ===")
    # 直觉以为：8 条线程 > 4 个核，CPU 活总能快一倍。屏幕上：等外部时确实 8 条一起在等，
    # 算数时 8 条和 2 条一样快 —— 「同时执行字节码」的那个数还是 1。
    reset()
    serial = m["serial"]
    t2 = thread_round(2)
    t8 = thread_round(8)
    with cf.ThreadPoolExecutor(max_workers=8) as ex:
        seen = list(ex.map(sleep_then_count_threads, [0.1] * 8))
    print(eq("本机核数（os.cpu_count()）", lambda: os.cpu_count(), 4))
    print(eq("这一格开的线程数是不是核数的两倍", lambda: 8 == 2 * os.cpu_count(), True))
    print(eq("8 份「等外部」同时在等时，本进程活跃线程数超过 8 条吗（人真的在场）",
             lambda: max(seen) > 8, True))
    print(eq("8 条线程比 2 条线程把 4 份纯计算快过 20% 吗", lambda: t2 / t8 > 1.2, False))
    print(eq("8 条线程比主线程一个人算快过 20% 吗", lambda: serial / t8 > 1.2, False))
    print(eq("本机 GIL 在场吗（sys._is_gil_enabled()）", lambda: sys._is_gil_enabled(), True))
    print(ledger("线程数是「多少人排队」，并行度是「几个窗口在办」；GIL 在场时纯计算只有一个窗口"))


def cell5(m):
    print("=== 5. 误区「进程数 = 加速倍数」：100 份活 / 2 个 worker，干活的就 2 个进程 ===")
    reset()
    serial = m["serial"]
    with cf.ProcessPoolExecutor(max_workers=2) as ex:
        used = set(ex.map(pid_of, range(100), timeout=30))
    p4 = process_round(4)
    p8 = process_round(8)
    print(eq("100 份活、max_workers=2：真干过活的进程个数 == 2 吗", lambda: len(used), 2))
    print(eq("== 任务数 100 吗（进程数被 max_workers 钉死，不由活的数量决定）",
             lambda: len(used) == 100, False))
    print(eq("worker 从 4 加到 8、活还是 4 份：快过 20% 吗", lambda: p4 / p8 > 1.2, False))
    print(eq("8 个 worker 比主线程一个人算快过 4 倍吗", lambda: serial / p8 >= 3.6, False))
    print(eq("4 个 worker 比串行快过 1.5 倍吗（本机实测口径）", lambda: serial / p4 > 1.5, True))
    print(f"  ★ 上限不是「你想要几倍」：本机 cpu_count() = {os.cpu_count()}，但 4 个进程只换来"
          f"两倍多；账面核数和你能占到的核也不是一回事。")
    print(ledger("加速倍数被「真并行窗口数」和「活能不能拆开」双重封顶，和被开几个进程无关"))


def cell6():
    print("=== 6. 误区「有 GIL 就线程安全」：同一份读写，中间让出过一次就必丢 ===")
    reset()
    lst = []
    run_threads(append_many, lambda i: (lst, 20000))
    print(eq("4 条线程各 append 2 万次：一条不丢吗（append 是 C 层一口气做完的）",
             lambda: len(lst), 80000))
    BOX["y"] = 0
    run_threads(yield_bump, lambda i: ("y", 2000))
    print(eq("读 → 让出一次 → 写，4 条线程各 2000 次：丢了吗（不印具体剩几个）",
             lambda: BOX["y"] < JOBS4 * 2000, True))
    BOX["l"] = 0
    lk = threading.Lock()
    run_threads(locked_bump, lambda i: ("l", 2000, lk))
    print(eq("同样的写法外面套一把 Lock：一条不丢吗", lambda: BOX["l"], 8000))
    BOX["p"] = 0
    run_threads(plus_equal, lambda i: ("p", 200000))
    print(eq("纯 `d[k] += 1`（中间不让出）本机这一跑丢了吗", lambda: BOX["p"] < 800000, False))
    print("  ★ 最后那条只说明「本机没赶上那次切换」：契约 2.1 和本机两种口径都测出不丢，"
          "所以「GIL 下 += 一定丢」这句本机演不出来、不许写成结论。能写成结论的是第三条："
          "让出过一次就是结构性必丢，加锁才治得住。具体丢了多少每跑必变，所以屏上不印。")
    BOX.clear()
    print(ledger("原子性不看「会不会碰巧」，只看「中间有没有让出过」：append 没有、读-让-写有"))


def cell7():
    print("=== 7. 误区「worker 自己知道该下班」：哨兵少放一个，就有一个永远在等 ===")
    reset()
    q = queue.Queue()
    got = []
    sent = []                              # 实际发出去的哨兵，当场数
    ws = [threading.Thread(target=sentinel_worker, args=(q, got), name=f"工人{i}")
          for i in range(WORKERS7)]
    for w in ws:
        w.start()
    for i in range(9):
        q.put(i)
    for _ in range(WORKERS7 - 1):           # ★ 3 个工人只发 2 个哨兵
        q.put(None)
        sent.append(1)
    time.sleep(0.3)                         # 给够时间：该收工的收工，没哨兵的还挂着
    print(eq(f"{WORKERS7} 个工人只发 {WORKERS7 - 1} 个哨兵：还在等的 worker 个数（这条对账先响一次）",
             lambda: sum(1 for w in ws if w.is_alive()), 1))
    q.put(None)                             # 补上最后一个
    sent.append(1)
    for w in ws:
        w.join()
    print(eq("补上最后一个哨兵之后：还在等的 worker 个数",
             lambda: sum(1 for w in ws if w.is_alive()), 0))
    print(eq("9 份活一份没少吃掉吗（哨兵不算活）", lambda: len(got), 9))
    print(eq("发出去的哨兵个数 == worker 个数了吗（收尾的规矩就这一句）",
             lambda: len(sent), len(ws)))
    empty_text = error_text(q.get_nowait)
    print(f"  队列空了还 get_nowait() → 报错原文：{empty_text}")
    print(eq("上面这句 == Empty 的原文（str 是空串，所以只剩冒号）", lambda: empty_text, "Empty: "))
    raised = class_of_error(q.get_nowait)
    print(eq("这个异常类印出来的全名（公开名是 queue.Empty，本体住在 C 模块 _queue 里）",
             lambda: f"{raised.__module__}.{raised.__name__}", "_queue.Empty"))
    print(eq("它和公开名 queue.Empty 是不是同一个类对象（是，只是全名不叫 queue）",
             lambda: raised is queue.Empty, True))
    print(eq("它的 str() 是不是空串（所以屏上看着像没写字，别按文字判种类）",
             lambda: str(queue.Empty()), ""))
    print(eq("带 timeout 的 get 超时抛的是不是同一个类",
             lambda: class_of_error(lambda: q.get(timeout=0.05)) is queue.Empty, True))
    print(eq("现在还剩几条线程（3 个工人全 join 了）", lambda: threading.active_count(), 1))
    print(ledger("哨兵个数必须等于 worker 个数；空队列不返回 None 而是抛 queue.Empty"
                 "（全名 _queue.Empty，str 空）"))


def cell8(m):
    print("=== 8. 收口：这一章最容易踩的坑，每一条都是本机当场炸出来的 ===")
    print("  ① 信号只能主线程装：子线程那句 ValueError 原文在第 1 格逐字印出。")
    print("  ② 循环里再 asyncio.run：RuntimeError 原文在第 2 格；重试没用，改成 await 才有用。")
    print("  ③ 闭包 / 局部函数 / lambda / 局部类进进程池 = PicklingError（第 3 格原文）；"
          "线程池不设这道门。")
    print("  ④ 线程数 != 并行度：8 条线程等外部时真有 8 条在等，算纯数时和 2 条一样（第 4 格）。")
    print("  ⑤ 进程数 != 加速倍数：100 份活 / 2 个 worker 只有 2 个进程干活；4 加到 8 也没快（第 5 格）。")
    print("  ⑥ 有 GIL != 线程安全：让出过一次就必丢，加锁才不丢（第 6 格）。")
    print("  ⑦ queue 收尾靠哨兵，个数必须等于 worker 数；空队列抛 queue.Empty"
          "（全名 _queue.Empty），str 是空串（第 7 格）。")
    print(eq("本进程还剩几条线程（工人全 join、池全随 with 退了）",
             lambda: threading.active_count(), 1))
    print(eq("multiprocessing 这边还登记着的活子进程个数", lambda: len(mp.active_children()), 0))
    print(eq("第 1 格那个信号处理函数已经装回原样了吗",
             lambda: signal.getsignal(signal.SIGUSR1) is ORIGINAL_SIG, True))
    print("  ★ 试过但本机演不出来的（这些一律不写进结论）："
          "① 「GIL 下 `d[k] += 1` 一定会丢」—— 本机 4×20 万次不让出的口径不丢；"
          "② 契约预告的 `AttributeError: Can't get local object ...` —— 本机对闭包/局部函数"
          "出现的是 PicklingError；"
          f"③ 「3.14 可以去掉 GIL」—— 本机 sysconfig 的 Py_GIL_DISABLED = "
          f"{sysconfig.get_config_var('Py_GIL_DISABLED')}，free-threading 解释器 "
          f"/usr/bin/python3.14t 存在吗 = {os.path.exists('/usr/bin/python3.14t')}；"
          f"④ 「换个事件循环就快」—— uvloop find_spec = {importlib.util.find_spec('uvloop')}，"
          "本机也没有 pip 可装；"
          "⑤ 「8 个进程 = 8 倍」—— 第 5 格那几条判定本机全是否。")
    print(f"  全篇判据合计 {TOTAL['ok']}/{TOTAL['n']} 条成立（少一条上面就有 [不成立！]）")


if __name__ == "__main__":
    m = {"serial": fastest(lambda: [cpu(CUNIT) for _ in range(JOBS4)])}
    cell1()
    cell2()
    cell3()
    cell4(m)
    cell5(m)
    cell6()
    cell7()
    cell8(m)
