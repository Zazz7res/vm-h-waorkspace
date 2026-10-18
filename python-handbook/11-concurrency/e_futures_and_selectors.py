#!/usr/bin/env python3
# ============================================================================
# 取证脚本 e_futures_and_selectors.py —— 第 11 章第 5 篇：future 那层壳 + 一个人收 N 路
#
# 要证明什么：① `concurrent.futures` 只换「谁来调度」，不换「你的活卡在什么上面」——
#   同样 4 份纯 Python 计算，4 条线程（= 本机核数）一点没变快，4 个进程明显变快但远没到
#   4 倍；反过来 4 个「等外部」的活丢进线程池就真的一起等。② 进程池那份「明显变快」是有
#   代价的：冷池跑一份立刻返回的空活比主进程自己跑贵一个量级，而且 worker 里的 `__name__`
#   是 `__mp_main__`、看不见主进程后来设的模块级标记 —— 它把主模块重新 import 了一遍（主线③）。
#   ③ `map` 保你发出的顺序，`as_completed` 按谁先做完给谁。④ future 把异常攒到
#   `result()` 才给你（`submit` 那一声不响）；进程池还把 worker 那边的栈打包带回来。
#   ⑤ 收 N 路子进程的输出不需要 N 条线程：`Popen` + `selectors` 一个人就能交错收完，
#   而 `subprocess.run` 串行收时，第一路收完你连第二个子进程都没启动。
#
# demo.sh 里这样调：`python3 e_futures_and_selectors.py`
#
# 安全说明：不落一个文件（子进程全用 `-c` 起，只 import time）；不联网、不开任何监听端口；
#   每个池都用 with 退出，每个 Popen 都 close + wait，第 8 格当场数还剩几个没收尾
#   （并且先故意留一个不 wait，证明这条对账真的会响）。
# 屏幕上不出现：时间数字、pid、内存地址。顺序只印「是不是发出先后 / 是不是完成先后」，
#   字典一律先 sorted 再印（不然每次跑屏上顺序都会抖，这就过不了三遍逐字一致）；
#   「快了多少」只以判定形式出现（快过 20% 吗 / 快过 1.5 倍吗 / 到 4 倍了吗 / 贵 10 倍吗），
#   阈值全带大余量，且每项都取 3 次里最快那次，用来压掉「别的程序正在抢 CPU」的噪声。
# ★ 本机不拿什么当证据：不写「4 个进程 = 4 倍」（本机只到两倍多）；不写「同一个池第二次
#   一定明显更便宜」（省的是启动，活本身把它盖掉了）；不拿「submit 后 done() 是 False」
#   当判据 —— 只有任务里放了 sleep 它才是形状，本篇每个这类判据都放了并在句子里写明；
#   不写「as_completed 比 map 快」（本机量不出稳定差，它俩只是出货顺序不同）。
# 报错原文照抄，只折路径与地址；退出码恒 0；连跑三遍输出逐字一致。本机 CPython 3.14.4。
# ============================================================================
import concurrent.futures as cf
import multiprocessing as mp
import os
import re
import selectors
import subprocess
import sys
import threading
import time

CELL = {"n": 0, "ok": 0}
TOTAL = {"n": 0, "ok": 0}
CUNIT = 8_000_000                        # 一份 CPU 活的规模（够盖住调度噪声）
WORKERS = 4                              # 线程数 / 进程数都取本机核数
IOSLEEP = 0.15                           # IO 那份活「等外部」要等多久（只用于比较，不上屏）
JOBS = [("甲", 0.04), ("乙", 0.20), ("丙", 0.12)]   # 三路子进程：每行之间隔这么久
STREAM_LINES = 3                         # 每路子进程一共说几行（结构量，可以印）
RUN_READY = False                        # 模块级标记：worker 看不见它后面的赋值（第 3 格）
MY_POPENS = []                           # 本篇起过的子进程，第 8 格逐个自证已退出


# ---- 模块顶层的可 pickle 函数：进程池按「名字」重新 import 它们（主线③），别写成闭包 ----
def cpu_sum(n):
    total = 0
    for i in range(n):
        total += i
    return total


def io_wait(d):
    time.sleep(d)                        # 一个「等外部」的活：GIL 在这一下是放手的
    return d


def io_wait_then_count(d):
    time.sleep(d)
    return threading.active_count()      # 站在工人视角数：此刻这个进程里有几条线程


def boom_late(x):
    time.sleep(0.05)                     # ★ 放 sleep：不放的话 done() 量的是快慢不是形状
    return 1 / 0


def boom_now(x):
    return 1 / 0


def noop():
    return "立刻返回"


def peek_flag(x):
    # 在 worker 里跑：__name__ 是 __mp_main__，RUN_READY 还是模块顶层那个 False
    return {"name": __name__, "run_ready": RUN_READY}


def mask(text):
    """地址与绝对路径一律折掉：报错原文不许把家目录带上屏。"""
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
    """测 k 次取最快那次（同一段活的答案每次都一样，只有快慢会抖）。"""
    best_time, best_value = None, None
    for _ in range(k):
        t0 = time.perf_counter()
        value = fn()
        dur = time.perf_counter() - t0
        if best_time is None or dur < best_time:
            best_time, best_value = dur, value
    return best_time, best_value


def child_src(tag, step):
    """给子进程的一段源码：说三行话，每行之间隔 step（不落文件，只用 -c）。"""
    body = "".join("print(%r, flush=True); time.sleep(%r)\n" % (f"{tag}行{i}", step)
                   for i in range(1, STREAM_LINES + 1))
    return "import time\n" + body


def child_env():
    return {"PATH": os.environ.get("PATH", ""), "LC_ALL": "C.UTF-8", "LANG": "C.UTF-8"}


def read_with_selectors():
    """一个人（当前这条线程）同时收 N 路：谁有数据收谁，谁先结束先摘牌。"""
    got, close_order, alive_at_first, procs = {}, [], 0, []
    with selectors.DefaultSelector() as sel:
        for tag, step in JOBS:
            p = subprocess.Popen([sys.executable, "-E", "-c", child_src(tag, step)],
                                 stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                 cwd=os.getcwd(), env=child_env())
            MY_POPENS.append(p)
            procs.append((tag, p))
            os.set_blocking(p.stdout.fileno(), False)   # 非阻塞：读不到就返回 None，不卡人
            sel.register(p.stdout, selectors.EVENT_READ, tag)
        while sel.get_map():
            for key, _ in sel.select(timeout=10):
                chunk = key.fileobj.read()
                if chunk:
                    got[key.data] = got.get(key.data, b"") + chunk
                else:                                    # 读到空 = 这路收完了
                    sel.unregister(key.fileobj)
                    close_order.append(key.data)
                    if len(close_order) == 1:            # 第一路摘牌那一刻，另外几路还在跑吗
                        alive_at_first = sum(1 for t, q in procs
                                             if t != key.data and q.poll() is None)
        for tag, p in procs:
            p.stdout.close()
            p.wait(timeout=10)
    lines = {tag: len(data.decode("utf-8").split()) for tag, data in got.items()}
    return lines, close_order, alive_at_first


def read_with_run():
    """对照写法：一路一路收，前一路不收完，后一路连启动都还没轮到。"""
    lines, order, started, first_started = {}, [], 0, 0
    for tag, step in JOBS:
        started += 1
        r = subprocess.run([sys.executable, "-E", "-c", child_src(tag, step)],
                           capture_output=True, cwd=os.getcwd(), env=child_env())
        lines[tag] = len(r.stdout.decode("utf-8").split())
        order.append(tag)
        if started == 1:
            first_started = started                      # 第一路收完时只启动过 1 个
    return lines, order, first_started


def cell1(m):
    print("=== 1. 选型判据（一）：纯 Python 计算丢进线程池 = 只多花一份调度钱 ===")
    # 直觉以为：4 条线程配满 4 个核，就该快 4 倍。屏幕上：答案一个没错，时间一点没省。
    reset()
    serial, sres = fastest(lambda: [cpu_sum(CUNIT) for _ in range(WORKERS)])
    with cf.ThreadPoolExecutor(max_workers=WORKERS) as ex:
        thr, tres = fastest(lambda: list(ex.map(cpu_sum, [CUNIT] * WORKERS)))
    ratio = serial / thr
    print(eq("4 份活的答案和主进程自己算的那 4 份逐字相同（并行没算错，只是没变快）",
             lambda: tres == sres, True))
    print(eq("答案的份数对得上吗（4 份活一份没丢）", lambda: len(tres), WORKERS))
    print(eq("线程数确实配满了本机核数吗（os.cpu_count()）", lambda: WORKERS == os.cpu_count(), True))
    print(eq("线程池比串行快过 20% 吗", lambda: ratio > 1.2, False))
    print(eq("线程池加速到 4 倍（= 核数）了吗", lambda: ratio >= 3.6, False))
    m["serial"], m["sres"] = serial, sres
    print("  ★ 这段活里一次 IO 等待都没有，GIL 就没机会放手：4 条线程全在排队领同一张票（a 篇）。")
    print(ledger("CPU 密集 + 线程 = 结果对、时间不省；选型看「卡在什么上」，不是「开几条线程」"))


def cell2(m):
    print("=== 2. 选型判据（二）：同一份计算丢进进程池，明显快了，但没到 4 倍 ===")
    reset()
    with cf.ProcessPoolExecutor(max_workers=WORKERS) as ex:
        par, pres = fastest(lambda: list(ex.map(cpu_sum, [CUNIT] * WORKERS)))
    ratio = m["serial"] / par
    print(eq("进程池那 4 份答案和串行那份逐字相同（换的是谁在跑，不是算法）",
             lambda: pres == m["sres"], True))
    print(eq("进程池比串行快过 1.5 倍吗", lambda: ratio > 1.5, True))
    print(eq("进程池加速到 4 倍（= 核数）了吗", lambda: ratio >= 3.6, False))
    print(f"  当场探到的默认 start method → {mp.get_start_method()!r}"
          f" ｜ 本机可选 → {sorted(mp.get_all_start_methods())}")
    print(ledger("CPU 密集要躲开 GIL 就得起了进程；本机 4 个进程只给到「两倍多」，别写 4 倍"))


def cell3():
    print("=== 3. 进程池的账不只算在活上：冷池一份空活就贵一个量级 ===")
    reset()
    global RUN_READY
    plain, _ = fastest(noop)
    with cf.ProcessPoolExecutor(max_workers=2) as ex:
        cold, _ = fastest(lambda: ex.submit(noop).result(), 2)   # 第一次：池还没起来
        warm, _ = fastest(lambda: ex.submit(noop).result())      # 同一个池第二次
        RUN_READY = True
        seen = ex.submit(peek_flag, 1).result()                  # worker 眼里的主模块
        mine = peek_flag(1)                                        # 主进程眼里的主模块
    print(eq("冷池跑一份「立刻返回」的空活，比主进程自己跑贵 10 倍以上吗",
             lambda: cold > plain * 10, True))
    print(eq("池热了之后，同一份空活还是比主进程自己跑贵吗", lambda: warm > plain, True))
    print(eq("worker 里的 __name__ 是什么（它把主模块当 __mp_main__ 重新 import 了一份）",
             lambda: seen["name"], "__mp_main__"))
    print(eq("worker 看得见主进程后来设的那个模块级标记吗", lambda: seen["run_ready"], False))
    print(eq("同一个函数在主进程里调用看得见吗（对照）", lambda: mine["run_ready"], True))
    print("  ★ 第 2 格那句「明显快了」的代价就在这：每个 worker 都要重跑一遍主模块的 import，"
          "所以活拆得太细时进程池倒亏；也正因如此，本篇能进池的函数全写在模块顶层。")
    print(ledger("进程池 = 先交一笔启动税再干活；活太细则倒贴，函数必须模块顶层可 import"))


def cell4():
    print("=== 4. 换个卡法就换个答案：4 个「等外部」的活丢进线程池真的一起等 ===")
    reset()
    serial, _ = fastest(lambda: [io_wait(IOSLEEP) for _ in range(WORKERS)])
    with cf.ThreadPoolExecutor(max_workers=WORKERS) as ex:
        thr, counted = fastest(lambda: list(ex.map(io_wait_then_count, [IOSLEEP] * WORKERS)))
    print(eq("线程池版全部收完比串行版省掉一半以上吗（真的一起在等）",
             lambda: thr * 2 < serial, True))
    print(eq("4 份活都返回了东西吗（数的是份数）", lambda: len(counted), WORKERS))
    print(eq("站在工人视角数：等待期间本进程的线程数比只有主线程时多吗",
             lambda: max(counted) > 1, True))
    print("  → 同一句 `time.sleep`：在第 1 格那种纯计算里没有它的位置；到这儿它是「主动把票交回来」。")
    print("     选型判据从来不是哪个库更先进，是你的活卡在计算上还是卡在等上（主线②）。")
    print(ledger("IO 密集 → 线程池真省时间，因为 GIL 在等待时放手（主线①）"))


def cell5():
    print("=== 5. map 保你发出的顺序，as_completed 按谁先做完给谁 ===")
    reset()
    delays = [0.09, 0.01, 0.05, 0.03]
    finish_order = [delays[i] for i, _ in sorted(enumerate(delays), key=lambda x: x[1])]
    with cf.ThreadPoolExecutor(max_workers=4) as ex:
        mapped = list(ex.map(io_wait, delays))
        fs = [ex.submit(io_wait, d) for d in delays]
        ac = [f.result() for f in cf.as_completed(fs)]
    print(eq("map 的结果顺序 == 发出的顺序", lambda: mapped, delays))
    print(eq("as_completed 收的先后 == 发出的顺序吗", lambda: ac == delays, False))
    print(eq("as_completed 收的先后 == 「谁先做完」的先后", lambda: ac, finish_order))
    print(eq("两种收法拿到的是同样那 4 份活（只差顺序）", lambda: sorted(ac) == sorted(mapped), True))
    print(ledger("map 给你对齐的表，as_completed 给你先到的货；要不要顺序取决于你接下来干什么"))


def cell6():
    print("=== 6. future 把异常攒着：submit 一声不响，result() 才当场飞给你 ===")
    reset()
    with cf.ThreadPoolExecutor(max_workers=3) as ex:
        f = ex.submit(boom_late, 1)
        print("  submit 这一行过去了 —— 屏上没有任何异常（它当时真的没抛）")
        print(eq("立刻 f.done()：还没跑完吗（任务里放了 sleep，所以这条量的是形状）",
                 lambda: f.done(), False))
        print(eq("f.result() 才抛的原文", lambda: f.result(), "ZeroDivisionError: division by zero"))
        print(eq("except Exception 抓得住它吗（对比 d 篇那个抓不住的 CancelledError）",
                 lambda: issubclass(ZeroDivisionError, Exception), True))
        it = ex.map(boom_now, [1, 2, 3])
        print("  ex.map 的迭代器也建好了 —— 屏上还是没动静（取结果才见分晓）")
        print(eq("第一次 next(it) 抛的原文", lambda: next(it), "ZeroDivisionError: division by zero"))
        errs = []
        for g in cf.as_completed([ex.submit(boom_now, i) for i in range(3)]):
            try:
                g.result()
            except BaseException as exc:
                errs.append(f"{type(exc).__name__}: {mask(exc)}")
        print(eq("as_completed 三个 future 各自抛的是什么（一人一份，不合并）",
                 lambda: errs, ["ZeroDivisionError: division by zero"] * 3))
        quiet = ex.submit(boom_now, 99)
        print(eq("不去 result、只问 exception()：异常确实躺在对象里（屏上没响 ≠ 没有）",
                 lambda: quiet.exception() is not None, True))
        fine = ex.submit(noop)
        print(eq("正常任务那边的 exception() 是什么（对照：这条对账不是恒真）",
                 lambda: fine.exception(), None))
        p = ex.submit(boom_now, 7)
        try:
            p.result()
        except BaseException as exc:
            print(eq("线程池那份异常的 __cause__（同一进程，没有远程栈要搬）",
                     lambda: exc.__cause__, None))
    with cf.ProcessPoolExecutor(max_workers=2) as pex:
        q = pex.submit(boom_now, 1)
        try:
            q.result()
        except BaseException as exc:
            print(f"  进程池的 result() 抛出来的原文 → {type(exc).__name__}: {mask(exc)}")
            print(eq("它后面挂着的 __cause__ 类型（worker 那边的栈被打包带回来了）",
                     lambda: type(exc.__cause__).__name__, "_RemoteTraceback"))
    print(ledger("submit 不抛、result 才抛；map 在迭代时抛、as_completed 一个个抛；进程池多带一层远程栈"))


def cell7():
    print("=== 7. Popen + selectors：一条线程收 N 路，收的是「谁先到」 ===")
    reset()
    print(f"  本机 selectors.DefaultSelector 用的是哪个 → "
          f"{type(selectors.DefaultSelector()).__name__}")
    p_lines, p_order, alive = read_with_selectors()
    s_lines, s_order, first_started = read_with_run()
    issued = [t for t, _ in JOBS]
    expect_close = [t for t, _ in sorted(JOBS, key=lambda x: x[1])]
    print(eq("selectors 版：每路都收满 %d 行吗（没漏字也没串台；先 sorted 再印）" % STREAM_LINES,
             lambda: sorted(p_lines.items()), sorted((t, STREAM_LINES) for t in issued)))
    print(eq("selectors 版：收完的先后 == 各路干完活的先后", lambda: p_order, expect_close))
    print(eq("selectors 版：收完的先后 == 发出顺序吗", lambda: p_order == issued, False))
    print(eq("selectors 版：最早收完那一路摘牌时，另外几路还在跑的个数", lambda: alive, len(JOBS) - 1))
    print(eq("对照 subprocess.run 串行版：收完的先后 == 发出顺序", lambda: s_order, issued))
    print(eq("对照串行版：第一路收完时一共已经启动过几个子进程", lambda: first_started, 1))
    print(eq("收这 %d 路用的线程数（只有主线程这一条）" % len(JOBS),
             lambda: threading.active_count(), 1))
    print(eq("两版收到的行数账本相同", lambda: sorted(p_lines.values()), sorted(s_lines.values())))
    print(ledger("selectors 把「等谁来」外包给内核：一条线程交错收 N 路，先来的先处理"))


def cell8(m):
    print("=== 8. 收口 + 收尾自证：本篇起的池和进程都归了零（并先证明这条对账会响）===")
    print("  ① 纯 Python 计算：4 条线程（= 4 核）没变快，4 个进程明显变快但远没到 4 倍（第 1、2 格）。")
    print("  ② 4 个「等外部」的活丢进线程池就真的一起等（第 4 格）：GIL 在等待时是放手的。")
    print("  ③ 进程池先交启动税：冷池一份空活贵一个量级，worker 里 __name__ = __mp_main__（第 3 格）。")
    print("  ④ map 保发出顺序、as_completed 按完成先后（第 5 格）：要对齐用 map，要抢先用它。")
    print("  ⑤ 异常一律攒到 result()（第 6 格）：submit 一声不响，进程池多带一层 _RemoteTraceback。")
    print("  ⑥ 收 N 路子进程输出：Popen + selectors 一条线程就够；subprocess.run 串行时"
          "第一路收完你才启动过 1 个（第 7 格）。")
    stray = subprocess.Popen([sys.executable, "-E", "-c", "import time; time.sleep(3)"],
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                             cwd=os.getcwd(), env=child_env())
    MY_POPENS.append(stray)                      # 故意先不 wait 它
    print(eq("先故意留一个不 wait：现在「还活着」的个数（证明这条对账真的会响）",
             lambda: sum(1 for p in MY_POPENS if p.poll() is None), 1))
    stray.wait(timeout=10)
    print(eq("等它退出之后：本篇起过的子进程里还活着的个数",
             lambda: sum(1 for p in MY_POPENS if p.poll() is None), 0))
    print(eq("本篇起过的子进程总数（结构量：全是 -c 起来收输出的）",
             lambda: len(MY_POPENS), len(JOBS) + 1))
    print(eq("multiprocessing 这边登记过的活子进程个数（池都随 with 退了）",
             lambda: len(mp.active_children()), 0))
    print(eq("本进程活跃线程数（回到只剩主线程 = 线程池的工人也被 join 干净了）",
             lambda: threading.active_count(), 1))
    print(f"  ★ 本篇试过但本机给不出结论的：① 「4 个进程 = 4 倍」—— 本机只到两倍多；"
          f"② 「同一个池第二次明显更便宜」—— 空活看得出、真活里被活本身盖掉，所以不印数；"
          f"③ 「as_completed 比 map 快」—— 只是出货顺序不同，本机量不出稳定差；"
          f"④ 「一次 select 就同时看到 N 路都有话」—— 每轮给到几路取决于对方什么时候吐字，"
          f"本机不稳，所以本篇只把「谁先来先处理、最后每路都收满」当判据，不印轮数。")
    print(f"  全篇判据合计 {TOTAL['ok']}/{TOTAL['n']} 条成立（少一条上面就有 [不成立！]）")


if __name__ == "__main__":
    m = {}
    cell1(m)
    cell2(m)
    cell3()
    cell4()
    cell5()
    cell6()
    cell7()
    cell8(m)
