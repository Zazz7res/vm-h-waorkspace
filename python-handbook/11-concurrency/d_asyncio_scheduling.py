#!/usr/bin/env python3
# ============================================================================
# 取证脚本 d_asyncio_scheduling.py —— 第 11 章第 4 篇：协程到底什么时候才真的跑
#
# 要证明什么：① `f()` 只是拿到一张预约单，协程体一行都没执行，没人 await 就永远不跑
#   （连报错都是原话「coroutine ... was never awaited」）；② `await` 是「我在这儿等」，
#   `create_task` 才是「交给循环排班」，而且主协程不让出一次，任务一个都不动；
#   ③ `gather` 的返回顺序跟你交出去的顺序一致，跟谁先做完无关；④ 一个同步
#   `time.sleep` 能把整个循环冻住 —— 正确量法不是「慢了多少」，是「阻塞那一刻
#   ticker 已经跳了几拍」（同步 0 拍 / await 版跳了一部分但没跳满；两种写法最后
#   总拍数都是 20：任务没死，只是全部迟到）；⑤ `to_thread` 是本机唯一在场的解法；
#   ⑥ `wait_for` 超时 = 循环替你把任务 cancel 掉，而 `CancelledError` 不是 `Exception`
#   的儿子，`except Exception` 抓不住它，在任务里把它吞掉还能骗过 `cancelled()`。
#
# demo.sh 里这样调：`python3 d_asyncio_scheduling.py`
#
# 安全说明：不落一个文件、不起子进程、不联网、不开任何监听端口；每一格的任务都在
#   这一格里 await 到结束，第 8 格当场数「还没收尾的任务剩几个」自证清零。
# 屏幕上不出现：耗时/秒/pid/内存地址（协程对象的 repr 当场折掉）。拍数只印结构量
#   （总拍数 20 是脚本写死的），阻塞期间跳了几拍只印「== 0 吗 / >= 2 吗 / < 20 吗」
#   这种带大余量的判定 —— 具体跳了 9 还是 10 拍每跑必变，所以不上屏。
# ★ 本机不拿什么当证据：不写「换个事件循环就快了」（uvloop 当场探为不在场，本机也没 pip）；
#   不写「to_thread 让 CPU 活并行」（本机 GIL 当场探为在场，它只是不堵循环）；
#   正文全缩进在函数里、由 __main__ 守卫调用 —— 本章实测默认 start method 会把主模块整份
#   重跑一遍，顶层 print 会被子进程复读，这篇也照同一个规矩写，读者好直接抄。
# 报错原文照抄，只折地址；退出码恒 0；连跑三遍输出逐字一致。本机 CPython 3.14.4 / Linux。
# ============================================================================
import asyncio
import gc
import importlib.util
import inspect
import re
import sys
import threading
import time
import warnings

CELL = {"n": 0, "ok": 0}
TOTAL = {"n": 0, "ok": 0}
TICK_N = 20            # ticker 一共跳几拍：写死的结构量，屏上可以印
TICK_GAP = 0.02        # 每拍之间
BLOCK = 0.2            # 两种写法各阻塞这么久；判定只用「>= 2 拍」这种大余量口径


def mask(text):
    """repr 里的内存地址一律折掉：本章屏上不许出现 0x...。"""
    return re.sub(r"0x[0-9a-fA-F]+", "<地址>", str(text))


def eq(tag, th, want):
    """判据行：实测与预期逐字比对，不成立当场印 [不成立！]（不存在恒真的守卫）。"""
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


# 模块顶层的协程函数：这样报错原文里的名字是干净的 do_nothing，
# 而不是 cell1.<locals>.do_nothing —— 读者才可能对得上号。
async def do_nothing(log):
    log.append("协程体真的跑了")
    return 1


async def nap(name, gap, out):
    await asyncio.sleep(gap)
    out.append(name)
    return name


async def ticker(state):
    for _ in range(TICK_N):
        await asyncio.sleep(TICK_GAP)
        state["t"] += 1


async def busywork():
    await asyncio.sleep(30)          # 用来被取消，正常情况下永远等不到它返回


def measure(mode):
    """阻塞那一刻 ticker 跳了几拍：sync = 同步 time.sleep，await = 交给循环睡。"""
    async def run():
        state = {"t": 0}
        tk = asyncio.create_task(ticker(state))
        await asyncio.sleep(0)                 # 先把 ticker 挂上闹钟
        if mode == "sync":
            time.sleep(BLOCK)                  # ← 误区写法：在协程里调同步阻塞
        else:
            await asyncio.sleep(BLOCK)
        during = state["t"]                    # ★ 就在阻塞结束的那一刻读表
        await tk
        return during, state["t"]
    return asyncio.run(run())


def cell1():
    print("=== 1. `f()` 不等于「跑了」：协程不 await 就一行都不执行 ===")
    # 直觉以为：调用函数就有副作用。屏幕上：log 是空的，返回值只是一张预约单。
    reset()
    log = []
    co = do_nothing(log)
    print(f"  刚写完 `co = do_nothing(log)`：log = {log} ｜ inspect.iscoroutine(co) → "
          f"{inspect.iscoroutine(co)} ｜ 类型 → {type(co).__name__}")
    print(f"  co 的 repr 带内存地址，按本章规矩折掉：{mask(repr(co))}")

    async def use(obj):
        return await obj
    got = asyncio.run(use(co))
    print(eq("await 过一次之后 log 里才有东西", lambda: log, ["协程体真的跑了"]))
    print(eq("await 拿到的返回值", lambda: got, 1))
    print(eq("同一个协程对象 await 第二次（报错原文）", lambda: asyncio.run(use(co)),
             "RuntimeError: cannot reuse already awaited coroutine"))
    with warnings.catch_warnings(record=True) as ws:
        warnings.simplefilter("always")
        leaked = do_nothing([])                # 只创建，永不 await
        del leaked
        gc.collect()                           # 没人 await 的协程是 GC 当场举报的
        hit = [f"{w.category.__name__}: {w.message}" for w in ws
               if "never awaited" in str(w.message)]
    print(eq("只创建不 await，GC 之后漂出来的原文", lambda: hit,
             ["RuntimeWarning: coroutine 'do_nothing' was never awaited"]))
    print(ledger("协程函数调用只生产一个协程对象；执行它的那一下叫 await，没人替你做"))


def cell2():
    print("=== 2. create_task 是排班，await 是干等：主协程不让出，任务一个都不动 ===")
    reset()

    async def run():
        started = []

        async def job(i):
            started.append(i)
            # ★ 任务里必须放 sleep：不放的话 done() 量的是「谁跑得快」，不是形状。
            await asyncio.sleep(0.05)
            return i
        ts = [asyncio.create_task(job(i)) for i in range(3)]
        s0, d0 = len(started), sum(1 for t in ts if t.done())
        await asyncio.sleep(0)                 # 主协程让出一次，闹钟才响
        s1, d1 = len(started), sum(1 for t in ts if t.done())
        await asyncio.gather(*ts)
        serial, par = [], []                   # 直接 await = 排队；create_task = 一起排
        for name, gap in [("慢", 0.06), ("快", 0.02)]:
            await nap(name, gap, serial)
        a = asyncio.create_task(nap("慢", 0.06, par))
        b = asyncio.create_task(nap("快", 0.02, par))
        await asyncio.gather(a, b)
        return s0, d0, s1, d1, serial, par

    s0, d0, s1, d1, serial, par = asyncio.run(run())
    print(eq("create_task 之后立刻：已经跑起来的任务个数（主协程还没让出）", lambda: s0, 0))
    print(eq("同上：已经做完的任务个数", lambda: d0, 0))
    print(eq("主协程 await 让出一次之后：已经跑起来的任务个数", lambda: s1, 3))
    print(eq("同一时刻做完的个数（任务里放了 sleep，所以稳稳是 0）", lambda: d1, 0))
    print(eq("两个直接 await：完成的先后 == 发出的先后", lambda: serial, ["慢", "快"]))
    print(eq("两个 create_task：完成的先后变成「谁先干完谁在前」", lambda: par, ["快", "慢"]))
    print(ledger("await 一个任务只是等它；create_task 才是交给循环，而它也得等主协程让出"))


def cell3():
    print("=== 3. gather 保的是「你交出去的顺序」，不是「谁先做完」 ===")
    reset()

    async def run():
        finished = []
        r = await asyncio.gather(nap("c", 0.06, finished), nap("a", 0.02, finished),
                                 nap("b", 0.04, finished))
        return r, finished

    r, finished = asyncio.run(run())
    print(eq("gather 返回值的顺序 == 交出去的顺序", lambda: r, ["c", "a", "b"]))
    print(eq("实际做完的先后（另一本账）", lambda: finished, ["a", "b", "c"]))
    print(eq("两本账一模一样吗", lambda: r == finished, False))
    print(eq("gather 直接吃协程对象（不用你先 create_task）收到的份数", lambda: len(r), 3))
    print(ledger("gather 给你「按参数顺序对齐」的结果表；想知道谁先做完得自己另外记账"))


def cell4():
    print("=== 4. 一个 time.sleep 把整个循环冻住：正确量法是「那一刻跳了几拍」 ===")
    # 直觉以为：主协程睡多久，同一循环里的 ticker 也该睡多久（它确实没跳）；
    # 或者以为「ticker 会自己找时间补上」。屏幕上：同步版一拍没跳，await 版跳了一部分。
    reset()
    sync_during, sync_total = measure("sync")
    await_during, await_total = measure("await")
    print(f"  ticker 设计成 {TICK_N} 拍、每拍一次闹钟；两种写法各阻塞同样一段（时间不上屏）")
    print(eq("同步 time.sleep 期间：已跳拍数 == 0 吗", lambda: sync_during == 0, True))
    print(eq("同步版：已跳拍数 >= 2 吗（这条要是 True 就说明秤坏了）", lambda: sync_during >= 2, False))
    print(eq("换成 await asyncio.sleep：已跳拍数 >= 2 吗", lambda: await_during >= 2, True))
    print(eq("换成 await：已跳拍数 == 0 吗", lambda: await_during == 0, False))
    print(eq("换成 await：已跳满 {0} 拍了吗（没跳满：阻塞那段闹钟是迟到不是消失）".format(TICK_N),
             lambda: await_during >= TICK_N, False))
    print(eq("同步版最后总拍数（等 ticker 收完再看）", lambda: sync_total, TICK_N))
    print(eq("await 版最后总拍数", lambda: await_total, TICK_N))
    print(eq("两种写法最后总拍数一样 → 任务没死，只是全部迟到",
             lambda: sync_total == await_total, True))
    print(ledger("阻塞循环的证据不是「慢了多久」，是「那段时间里循环一格都没走」"))


def cell5():
    print("=== 5. to_thread：把同步阻塞甩给别的线程，循环照跳（本机唯一在场的解法）===")
    reset()
    print(eq("本机 asyncio.to_thread 在场吗", lambda: hasattr(asyncio, "to_thread"), True))
    print(eq("本机有没有 uvloop 可换（None = 没装，且没有 pip）",
             lambda: importlib.util.find_spec("uvloop") is None, True))
    print(eq("本机 GIL 仍然在场（sys._is_gil_enabled()）", lambda: sys._is_gil_enabled(), True))

    async def run():
        state = {"t": 0}
        base = threading.active_count()
        peak = []

        async def watch():
            for _ in range(6):
                await asyncio.sleep(0.01)
                peak.append(threading.active_count())
        w = asyncio.create_task(watch())
        await asyncio.to_thread(time.sleep, 0.08)     # 同步调用搬进线程池
        await w
        tk = asyncio.create_task(ticker(state))
        await asyncio.to_thread(time.sleep, BLOCK)
        during = state["t"]
        await tk
        return base, max(peak), during, state["t"]

    base, peak, during, total = asyncio.run(run())
    print(eq("等待期间的活跃线程数比开跑前多吗（真换了个人干活）", lambda: peak > base, True))
    print(eq("包进 to_thread 之后：阻塞期间 ticker 拍数 >= 2 吗", lambda: during >= 2, True))
    print(eq("包进 to_thread 之后：总拍数", lambda: total, TICK_N))
    print(f"  ★ 别把它读成「并行」：GIL 在上面那条判据里仍在场，CPU 密集的活丢进 to_thread"
          f"只是不堵循环，两个线程照样不会同时执行字节码。")
    print(ledger("to_thread = 别拿事件循环的命去等一个同步调用；本机没有第二个官方解法"))


def cell6():
    print("=== 6. wait_for 超时：循环替你把任务取消，不是「任务自己失败了」 ===")
    reset()
    seen = []

    async def slow():
        try:
            await asyncio.sleep(30)            # 远超超时线，保证每次都超时
        except asyncio.CancelledError:
            seen.append("收到 CancelledError")
            raise
        seen.append("跑完了")
        return "结果"

    async def run():
        before = len(asyncio.all_tasks())
        try:
            await asyncio.wait_for(slow(), 0.03)
            return "居然没超时", before, len(asyncio.all_tasks())
        except BaseException as exc:
            return f"{type(exc).__name__}: {mask(exc)}", before, len(asyncio.all_tasks())

    err, before, after = asyncio.run(run())
    print(f"  超时那一刻屏上出现的原文 → {err}")
    print(eq("上面这行 == TimeoutError 的原文（str 是空串，所以只剩冒号）",
             lambda: err, "TimeoutError: "))
    print(eq("asyncio.TimeoutError 就是内置 TimeoutError 吗",
             lambda: asyncio.TimeoutError is TimeoutError, True))
    print(eq("任务内部看到的是不是 CancelledError", lambda: seen, ["收到 CancelledError"]))
    print(eq("任务里那句「跑完了」有没有执行", lambda: "跑完了" in seen, False))
    print(eq("超时后这个循环里还没收尾的任务个数（回到只剩主任务）", lambda: after, before))
    print(ledger("wait_for 的超时是循环把任务 cancel：任务收到的是 CancelledError，不是 TimeoutError"))


def cell7():
    print("=== 7. cancel 的下场：取消只是「请求」，还能被任务自己吞掉 ===")
    reset()
    print(eq("CancelledError 是 Exception 的儿子吗（except Exception 抓不住它）",
             lambda: issubclass(asyncio.CancelledError, Exception), False))

    async def stubborn(out):
        try:
            await asyncio.sleep(30)
        except asyncio.CancelledError:
            out.append("我 catch 住取消，并且不再往上抛")
            return "我照常返回"

    async def run():
        t = asyncio.create_task(busywork())
        await asyncio.sleep(0)
        ret, done_now = t.cancel(), t.done()
        msg = ""
        try:
            await t
        except BaseException as exc:
            msg = f"{type(exc).__name__}: {mask(exc)}"
        out = []
        s = asyncio.create_task(stubborn(out))
        await asyncio.sleep(0)
        s.cancel()
        val = await s
        return ret, done_now, msg, t.cancelled(), val, s.cancelled(), s.done(), out

    ret, done_now, msg, cancelled, val, s_canc, s_done, out = asyncio.run(run())
    print(eq("cancel() 的返回值：取消请求发出去了吗", lambda: ret, True))
    print(eq("同一瞬间 t.done()：任务当场就死了吗（不是，得等下一拍）", lambda: done_now, False))
    print(f"  await 一个被正常取消的任务，抛出来的是 → {msg}")
    print(eq("上面这行 == CancelledError 的原文", lambda: msg, "CancelledError: "))
    print(eq("没被吞掉时 cancelled() 是 True", lambda: cancelled, True))
    print(eq("任务里 catch 住 CancelledError 且不再抛：cancelled() 变成", lambda: s_canc, False))
    print(eq("同一个任务的 done() 与 await 到的值", lambda: (s_done, val), (True, "我照常返回")))
    print(eq("它有没有把「我被取消过」这件事告诉外面", lambda: out,
             ["我 catch 住取消，并且不再往上抛"]))
    print(ledger("CancelledError 继承 BaseException：except Exception 漏抓；吞掉它 = 假装没被取消"))


def cell8():
    print("=== 8. 收口：asyncio 管的是「谁在什么时候让出」，不是「谁多」 ===")
    print("  ① 调用协程函数 = 得到一个协程对象，副作用 0 次（第 1 格：log 先是空的）。")
    print("  ② await = 在这儿等；create_task = 交给循环排班，但主协程不让出一次它就不动")
    print("     （第 2 格：create_task 之后「已跑起来」= 0，让出一次才变 3）。")
    print("  ③ gather 的返回顺序 == 你交出去的顺序，跟谁先做完无关（第 3 格两本账不同）。")
    print("  ④ 同步阻塞把循环按死：那段时间 ticker 跳 0 拍；换成 await 就跳一部分；")
    print(f"     两种写法总拍数都是 {TICK_N} —— 任务没死，全部迟到（第 4 格）。")
    print("  ⑤ 解法只有 to_thread（本机没有 uvloop，也没有 pip）：循环照跳，但 GIL 还在（第 5 格）。")
    print("  ⑥ wait_for 超时 = 循环把任务 cancel：外面 TimeoutError、里面 CancelledError（第 6 格）；")
    print("     CancelledError 不是 Exception 的儿子，吞掉它就能骗过 cancelled()（第 7 格）。")

    async def run():
        keep = asyncio.create_task(busywork())
        await asyncio.sleep(0)
        loud = len(asyncio.all_tasks())        # 故意留一个不收尾的，先看这条对账会不会响
        keep.cancel()
        try:
            await keep
        except BaseException:
            pass
        return loud, len(asyncio.all_tasks())

    loud, quiet = asyncio.run(run())
    print(eq("先故意留一个不收尾的任务：还没收尾的任务个数（证明这条对账真的会响）", lambda: loud, 2))
    print(eq("把它 cancel 并 await 收尾之后：还没收尾的任务个数", lambda: quiet, 1))
    print(eq("脚本退出前本进程还剩几条线程（只剩主线程 = 线程池已随循环关掉）",
             lambda: threading.active_count(), 1))
    print(f"  ★ 本机演不出来、所以本篇不写的东西：① 「换个事件循环让 CPU 活变快」—— "
          f"uvloop 第 5 格当场探为不在场，也没有 pip；② 「await 之后任务真的并行执行字节码」—— "
          f"GIL 当场探为在场；③ 「任务 cancel 后一定会立刻死」—— 第 7 格那个吞取消的就是反例，"
          f"但代价是 cancelled() 从此撒谎，不是一句「cancel 会立刻生效」。")
    print(f"  全篇判据合计 {TOTAL['ok']}/{TOTAL['n']} 条成立（少一条上面就有 [不成立！]）")


if __name__ == "__main__":
    cell1()
    cell2()
    cell3()
    cell4()
    cell5()
    cell6()
    cell7()
    cell8()
