#!/usr/bin/env python3
# ============================================================================
# g_four_ways_same_job.py —— 第 11 章的收口格：**同一件活**，四种写法各跑一遍
#
#   demo.sh 里这样调：python3 g_four_ways_same_job.py
#
# 一句话主旨：a~f 分别讲了 GIL、线程、进程、asyncio、futures 各自的脾气，这一支把它们
#   摆成一张表。活只有两种：「**等别人**」（起 6 个 python 子进程，每个睡 0.25s）和
#   「**自己算死**」（对几百万个整数求和）。每种写法在两种活上的表现当场判出来。
#   屏幕上**一个秒数都没有**：判定只用「比串行快过 X 倍吗」这种带大余量的布尔，
#   结构只用「几个不同 pid / 几个不同线程 / 跳了几拍」——这些是跨趟稳定的数。
#
# 安全说明：
#   * 本脚本不写任何文件；起的每个子进程都 wait 收尾，线程全部 join，池全部 shutdown，
#     第 7 格当场数「还剩几个活跃子进程/线程」，并和第 0 格的基线对照。
#   * 不联网、不开监听端口、不 pip、不碰 ~ 下任何目录。
#   * ★ 本机不拿什么当证据：本机是**带 GIL 的 CPython 3.14.4**（a 篇当场探过），
#     没有 free-threading 解释器。所以「CPU 密集线程不加速」在本机成立；
#     换一台把 GIL 编译掉的机器这条要重测 —— 本章不替那台机器说话。
#   * ★ 加速倍数也不许背：本机 4 核、forkserver 起进程有成本，实测只能稳在
#     「明显快过串行（>1.3 倍）」和「远没有到 3 倍」这两条判定上，中间不做承诺。
# ============================================================================
import asyncio
import concurrent.futures as cf
import multiprocessing as mp
import os
import subprocess
import sys
import threading
import time

NJOBS = 6            # 「等别人」的活有几份：也就是线程池/进程池的 worker 数
SLEEP = 0.25         # 每份子进程睡这么久 —— 只有串行会老实排 6 次队
BURN = 4_000_000     # 「自己算死」的活：真算，不用 sleep 假装
POOL_WORKERS = 4     # 算死那份故意只给 4 个 worker（6 份活），好看清「不是份数=并行度」


def wait_job(i):
    """「等别人」型活：起一个子进程让它睡一会儿。
    返回 (退出码, 我这个进程, 我这个线程) —— 调用方只数**种类数**，不印这些值。"""
    p = subprocess.run([sys.executable, "-c", f"import time; time.sleep({SLEEP})"],
                       capture_output=True)
    return p.returncode, os.getpid(), threading.get_ident()


def cpu_job(i):
    """「自己算死」型活：一个真循环，返回值就是和。"""
    s = 0
    for x in range(BURN):
        s += x
    return s, os.getpid(), threading.get_ident()


def cpu_answer():
    return sum(range(BURN))


def shape(res):
    """从一堆结果里只取结构：几个不同的 pid、几个不同的线程号。"""
    return len({r[1] for r in res}), len({r[2] for r in res})


def in_several(res):
    """这批活有没有真的分给「多于一个」执行体（数 pid 或数线程号，看调用方给哪一路）。

    ★ 为什么屏幕上不印「池子实际派出几个 worker」那个数：max_workers 只是**上限**，
      不是「一定起满」—— 谁先跑到队列里抢活由调度决定。本机三趟里，6 份活有一趟
      只被 5 个线程干完。能跨趟承诺的只有「多于一个」这一档；至于几个，答案写在
      你声明的那个数字里，不在屏幕上。
    """
    return len({r[1] for r in res}) > 1, len({r[2] for r in res}) > 1


def run_serial(job):
    t0 = time.perf_counter()
    res = [job(i) for i in range(NJOBS)]
    return res, time.perf_counter() - t0


def grid_wait(base):
    print("=== 2. 同一件「等别人」的活，四种写法各跑一遍 ===")
    with cf.ThreadPoolExecutor(max_workers=NJOBS) as ex:
        t0 = time.perf_counter()
        r = list(ex.map(wait_job, range(NJOBS)))
        w_thr = time.perf_counter() - t0
    print(f"  · 写法 A 线程池（max_workers={NJOBS}）")
    print("      退出码全 0 吗 →", [x[0] for x in r] == [0] * NJOBS)
    print("      干这批活的是几个进程 →", shape(r)[0],
          "｜ 分到多于一个线程吗 →", in_several(r)[1], "（★ 进程数恒 1：线程出不了这个进程）")
    print("      ★ 比串行快过 3 倍吗 →", w_thr < base / 3, "（等待时 GIL 是放手的）")

    async def agg_async():
        async def one(i):
            p = await asyncio.create_subprocess_exec(
                sys.executable, "-c", f"import time; time.sleep({SLEEP})",
                stdout=asyncio.subprocess.DEVNULL, stderr=asyncio.subprocess.DEVNULL)
            return await p.wait(), os.getpid(), threading.get_ident()
        t0 = time.perf_counter()
        res = await asyncio.gather(*[one(i) for i in range(NJOBS)])
        return res, time.perf_counter() - t0

    r, w_async = asyncio.run(agg_async())
    print("  · 写法 B asyncio：一个线程 + 事件循环")
    print("      退出码全 0 吗 →", [x[0] for x in r] == [0] * NJOBS)
    print("      干活的是几个不同进程 / 几个不同线程 →", shape(r),
          "（★ 一个线程也能并发：卡的是内核，不是字节码）")
    print("      比串行快过 3 倍吗 →", w_async < base / 3)

    t0 = time.perf_counter()
    with cf.ProcessPoolExecutor(max_workers=NJOBS) as ex:
        r = list(ex.map(wait_job, range(NJOBS)))
    w_proc = time.perf_counter() - t0
    print(f"  · 写法 C 进程池（max_workers={NJOBS}）")
    print("      退出码全 0 吗 →", [x[0] for x in r] == [0] * NJOBS)
    print("      这批活落在多于一个进程里吗 →", in_several(r)[0],
          "（★ 同一件活，它要多起若干 python 进程才换来这份并发）")
    print("      比串行快过 3 倍吗 →", w_proc < base / 3)
    print("  → 判据：等外面的东西时四种写法都能叠；差别不在「快多少」，在**代价** ——")
    print(f"     线程版 0 个额外进程、asyncio 版 0 个额外线程、进程版为并发要多起进程（声明的上限 {NJOBS} 个），")
    print("     而且每份活都要 pickle 进出一次")


def grid_cpu(base):
    print("=== 3. 换一种活：不是等，是自己算死 ===")
    expect = cpu_answer()
    print("      串行基线立好：6 份答案全对吗 →", [x[0] for x in _CPU_SERIAL] == [expect] * NJOBS,
          "｜ 干活的进程数/线程数 →", shape(_CPU_SERIAL))
    with cf.ThreadPoolExecutor(max_workers=NJOBS) as ex:
        t0 = time.perf_counter()
        r = list(ex.map(cpu_job, range(NJOBS)))
        w_thr = time.perf_counter() - t0
    print("  · 线程版算死：答案全对 →", [x[0] for x in r] == [expect] * NJOBS,
          "｜ 落在多于一个线程里 →", in_several(r)[1],
          "｜ 全在这一个进程里 →", shape(r)[0] == 1)
    print("      ★ 比串行快过 1.3 倍吗 →", w_thr < base / 1.3, "（该是 False —— 这就是 GIL 的税）")

    async def agg_async_cpu():
        t0 = time.perf_counter()
        res = await asyncio.gather(*[asyncio.to_thread(cpu_job, i) for i in range(NJOBS)])
        return res, time.perf_counter() - t0

    r, w_at = asyncio.run(agg_async_cpu())
    print("  · asyncio + to_thread 算死：答案全对 →", [x[0] for x in r] == [expect] * NJOBS)
    print("      ★ 比串行快过 1.3 倍吗 →", w_at < base / 1.3,
          "（该是 False：换皮还是线程，GIL 还在）")

    t0 = time.perf_counter()
    with cf.ProcessPoolExecutor(max_workers=POOL_WORKERS) as ex:
        r = list(ex.map(cpu_job, range(NJOBS)))
    w_proc = time.perf_counter() - t0
    print(f"  · 进程版（{POOL_WORKERS} 个 worker 吃 {NJOBS} 份活）：答案全对 →",
          [x[0] for x in r] == [expect] * NJOBS)
    print("      这批活落在多于一个进程里吗 →", in_several(r)[0],
          f"（★ 并行度的天花板是**声明的 {POOL_WORKERS} 个 worker**，不是 {NJOBS} 份活）")
    print("      比串行快过 1.3 倍吗 →", w_proc < base / 1.3, "（该是 True：真的并行了一段）")
    print("      快到 3 倍了吗 →", w_proc < base / 3, "（该是 False：本机给不出核数倍的加速）")
    print("  → 判据：选型只问一句话 —— **你的时间花在等，还是花在算**")


def frozen_loop():
    print("=== 4. 附：asyncio 里做重活会把整个循环冻住（量的是拍数，不是秒）===")
    ticks = []

    async def ticker():
        for _ in range(20):
            await asyncio.sleep(0.01)
            ticks.append(1)

    async def bad():
        ticks.clear()
        t = asyncio.create_task(ticker())
        cpu_job(0)                      # 就在循环线程里算死
        seen = len(ticks)
        await t
        return seen, len(ticks)

    async def good():
        ticks.clear()
        t = asyncio.create_task(ticker())
        await asyncio.to_thread(cpu_job, 0)     # 挪走
        seen = len(ticks)
        await t
        return seen, len(ticks)

    s_bad, tot_bad = asyncio.run(bad())
    s_good, tot_good = asyncio.run(good())
    print("      重活留在循环里：那一刻一拍都没跳吗 →", s_bad == 0,
          "（结构上必是 0：算死期间循环一次都没被让出来）")
    print("      重活挪进 to_thread：那一刻已经跳过拍了吗 →", s_good > 0,
          "（跳了几拍每跑必变，所以这里只问有没有）")
    print("      两种写法最后总拍数都满了 20 拍吗 →", tot_bad == 20 and tot_good == 20,
          "（任务没死，只是全部迟到）")
    print("  → 判据：「循环被冻住」只能数拍子（那一刻是 0 拍还是已跳过拍），拿秒表量就废了 ——")
    print("     冻住多久每跑必变，而「一拍都没跳」是结构上跑不掉的。")

    print("=== 5. 附：to_thread 里的活到底落在谁的线程上 ===")
    def who():
        th = threading.current_thread()
        return th is threading.main_thread(), th.name.split("_")[0]

    is_main, prefix = asyncio.run(asyncio.to_thread(who))
    print("      在 to_thread 里 current_thread 是主线程吗 →", is_main, "（★ 该是 False）")
    print("      那个线程的名字前缀 →", prefix,
          "→ 是事件循环自己开的默认线程池吗", prefix == "asyncio")
    print("  → 判据：asyncio 帮你并发的是「等」；真要把重活搬走，它用的还是线程 —— 别信「async 就是并行」")


def cleanup_proof():
    print("=== 6. 收尾自证：这一支起过的东西必须一个不剩 ===")
    alive_threads = threading.active_count()
    alive_children = len(mp.active_children())
    print("      跑满全篇后活跃线程数 =", alive_threads, "→ 只剩主线程吗", alive_threads == 1)
    print("      multiprocessing 登记的活跃子进程数 =", alive_children, "→ 一个不剩吗", alive_children == 0)
    print("  → 判据 2/2 条成立" if alive_threads == 1 and alive_children == 0
          else "  [不成立！] 有东西没收尾")


def main():
    print("=== 0. 先把地板和「活」说清楚 ===")
    print(f"      等别人的活：起 {NJOBS} 个 python 子进程，每个被喂进一个固定的 sleep({SLEEP}) 参数"
          f"（这是**设进去的**，不是测出来的），收 {NJOBS} 个退出码")
    print(f"      算死的活：{NJOBS} 份「对 {BURN} 个整数求和」")
    print("      本机 python →", sys.version.split()[0],
          "｜ GIL 开着吗 →", sys._is_gil_enabled(),
          "｜ os.cpu_count() →", os.cpu_count(),
          "｜ multiprocessing 默认 start method →", mp.get_start_method())
    print("      → 判据：这张表所有倍数都拿**本机**当尺：带着 GIL、核数就是上面那个数、"
          "进程走 forkserver。换一台机器（或换成不带 GIL 的编译）请整张重跑，别引用本表。")
    global _CPU_SERIAL
    r_wait, SERIAL_WAIT = run_serial(wait_job)
    _CPU_SERIAL, SERIAL_CPU = run_serial(cpu_job)
    print("=== 1. 串行基线（后面所有判定都拿它当尺）===")
    print("      等别人那份：退出码全 0 →", [x[0] for x in r_wait] == [0] * NJOBS,
          "｜ 干活的进程数/线程数 →", shape(r_wait), "（串行就是 1/1，不需要线程）")
    print("  → 判据：串行**做完**了这件事，只是把 6 段等待**排成一条队**")
    grid_wait(SERIAL_WAIT)
    grid_cpu(SERIAL_CPU)
    frozen_loop()
    cleanup_proof()
    print("=== 7. 本章判据合计 ===")
    print("      这张表里所有判定都是当场算的；换一台机器（核数不同、有没有 GIL）请重跑，别引用本表。")


if __name__ == "__main__":
    main()
