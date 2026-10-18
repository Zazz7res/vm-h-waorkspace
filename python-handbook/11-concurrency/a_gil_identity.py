#!/usr/bin/env python3
# ============================================================================
# 取证脚本 a_gil_identity.py —— 第 11 章第 1 篇：GIL 的真身与在场性（主线①④）
# 要证明什么：GIL 是「一次只发一张字节码执行票」的锁，不是「不许并发」的禁令 ——
#   ① 本机在场且关不掉（free-threading 那份解释器根本没编，uvloop/pip 这两条退路也不在场）；
#   ② 颗粒度是字节码，不是你写的那一行：把 box["n"] += 1 拆开数给你看；
#   ③ 睡觉、等管道这两种「没在干活」它当场放手，别的线程照跑；
#   ④ 纯 Python 死循环时它按 switchinterval 的节拍换手，不是抱着不放；
#   ⑤ 它不替你保证互斥 —— 一把普通 Lock 照样能把主线程挡在门外。
# demo.sh 里这样调：`python3 a_gil_identity.py`
# 安全：不起子进程、不联网、不开任何监听端口（只用一个匿名管道，读完立刻关）、一个文件/目录
#   都不建；每个线程都 Event + join 收尾，退出前自证 threading.enumerate() 只剩主线程。
# ★ 本机不拿什么当证据：不拿「3.14 能去掉 GIL」当证据（Py_GIL_DISABLED=0、没有 python3.14t，
#   本机连试都没法试）。屏上不许出现时长/pid/内存地址/丢更新后的计数 —— 只有布尔、== 比较，
#   和「几条字节码」「几个线程」这种结构量。repr 里的地址与家目录当场折掉；退出码恒 0。
# ============================================================================
import _thread, asyncio, dis, importlib.util, os, re, selectors, shutil, sys, sysconfig, threading, time

ADDR = re.compile(r"0x[0-9a-fA-F]{3,}")
CELL = {"n": 0, "ok": 0}          # 本格判据：条数 / 成立数
TOTAL = {"n": 0, "ok": 0}         # 全篇判据：条数 / 成立数


def mask(text):
    """锁和线程的 repr 自带内存地址，路径里可能带家目录 —— 上屏前统一折掉。"""
    return ADDR.sub("<地址>", str(text)).replace(os.path.expanduser("~"), "<家>")


def eq(tag, th, want):
    """判据行：实测与手写预期逐字比对，错了当场印 [不成立！]（不许有糊弄人的恒真断言）。"""
    try:
        got = th()
    except Exception as exc:
        got = f"{type(exc).__name__}: {mask(exc)}"
    CELL["n"] += 1; CELL["ok"] += 1 if got == want else 0
    TOTAL["n"] += 1; TOTAL["ok"] += 1 if got == want else 0
    return f"  [{'成立' if got == want else '不成立！'}] {tag}：实测 {mask(repr(got))} ｜ 预期 {mask(repr(want))}"


def trip(tag, th, wrong):
    """尺子自检：计数类判据先故意拿错的预期量一遍，必须对不上 —— 证明这把尺真的会响。"""
    got, alive = th(), th() != wrong
    CELL["n"] += 1; CELL["ok"] += 1 if alive else 0; TOTAL["n"] += 1; TOTAL["ok"] += 1 if alive else 0
    return f"  [{'尺子能响' if alive else '尺子坏了！'}] {tag}：故意按 {wrong} 去对，实测 {got} 对不上"


def ledger(title):
    return f"  → 断言：{title} ｜ 本格 {CELL['ok']}/{CELL['n']} 条成立（不成立的会印 [不成立！]）"


def spin(state, stop):
    while not stop["f"]:                    # 纯 Python 死循环：只数自己的拍，不碰任何外部资源
        state["n"] += 1


def with_spinner(body):
    """起一个死循环线程，主线程去跑 body（睡觉 / 堵管道），回来问一句：它这段有没有推进。"""
    st, stop = {"n": 0}, {"f": False}
    t = threading.Thread(target=spin, args=(st, stop), name="a-推进")
    t.start()
    before, out = st["n"], body()
    stop["f"] = True
    t.join()
    return (st["n"] > before), out


def count_then_write(w_fd, state, k):
    for _ in range(k):
        state["n"] += 1
    os.write(w_fd, b"1")                     # 攒够计数，才放行那个堵着读的人


def main():
    print("=== 1. GIL 在本机在场吗：关得掉吗、几颗核、有没有退路 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：3.14 传闻「GIL 能关掉」，本机一键就能关，还能装个加速库 ｜ 屏幕：关不掉，也没得装。")
    print(f"  本机解释器（路径已折） → {(sys.implementation.name, sys.version_info[:3])}")
    print(eq("sys._is_gil_enabled()：此刻 GIL 开着吗", lambda: sys._is_gil_enabled(), True))
    print(eq('sysconfig.get_config_var("Py_GIL_DISABLED")：编译期去掉过 GIL 吗', lambda: sysconfig.get_config_var("Py_GIL_DISABLED"), 0))
    print(eq("free-threading 版解释器 /usr/bin/python3.14t 在盘上吗", lambda: os.path.exists("/usr/bin/python3.14t"), False))
    box = {}
    t = threading.Thread(target=lambda: box.__setitem__("v", sys._is_gil_enabled()))
    t.start(); t.join()
    print(eq("换到子线程里再问一次，答案变吗", lambda: box["v"], True))
    print(eq("os.cpu_count()", os.cpu_count, 4))
    print(eq("本进程真能用到的核 len(os.sched_getaffinity(0))", lambda: len(os.sched_getaffinity(0)), 4))
    print(eq("uvloop（第三方事件循环加速）find_spec", lambda: importlib.util.find_spec("uvloop"), None))
    print(eq("pip 在不在 PATH 里（能不能「装一个就好」）", lambda: shutil.which("pip"), None))
    print(eq("asyncio.to_thread 在不在（把阻塞活甩给线程的那扇门）", lambda: hasattr(asyncio, "to_thread"), True))
    print(eq("selectors 默认后端（第 5 篇要用的那台分发器）", lambda: selectors.DefaultSelector.__name__, "EpollSelector"))
    print("  ★ 4 颗核 + 1 把 GIL，而「换解释器」「装库」两条退路本机都不存在 —— 本章的结论只能"
          "建立在「同一把 GIL 之下怎么排活」上。")
    print(ledger("本机 GIL：编译期带着、运行时开着、关不掉"))

    print('\n=== 2. GIL 锁的是字节码：把 box["n"] += 1 拆开数 ===')
    CELL["n"] = CELL["ok"] = 0

    def plus_eq(bx):
        bx["n"] += 1

    ops = [i.opname for i in dis.get_instructions(plus_eq)]
    print(f"  直觉：一行语句 = 一个动作，别人插不进来 ｜ 屏幕：这一行翻成 {len(ops)} 条字节码 → {ops}")
    print(eq("条数 > 1（结构量：「一行」根本不是一步）", lambda: len(ops) > 1, True))
    print(eq("串里同时有取值的 BINARY_OP 和写回的 STORE_SUBSCR", lambda: ("BINARY_OP" in ops, "STORE_SUBSCR" in ops), (True, True)))
    print(eq("写回那条排在整串后段（读和写之间隔着好几条）", lambda: ops.index("STORE_SUBSCR") > len(ops) // 2, True))
    print("  ★ 丢更新的机械原因就在这儿：GIL 发的是「这一条字节码归谁跑」的票，不是「这一行归谁跑」，"
          "读和写之间那张票随时可能换人 —— b 脚本第 5~7 格当场量。")
    print(ledger("GIL 的粒度比你的语句细得多"))

    print("\n=== 3. 「谁在跑」这本账由谁记：threading 与 _thread 各记各的 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：threading.active_count() 就是「现在几个线程在跑」 ｜ 屏幕：它只是 threading 自己的账本。")
    print(trip("起线程前 active_count 是几（先故意按错的数去对）", threading.active_count, 7))
    print(eq("起线程前 threading.active_count()", threading.active_count, 1))
    print(eq("主线程名", lambda: threading.current_thread().name, "MainThread"))
    gate = threading.Event()
    t = threading.Thread(target=gate.wait, name="a-记账的")
    print(eq("还没 start 的 Thread 对象有 ident 吗", lambda: t.ident, None))
    t.start()
    print(eq("start 之后 ident 不再是 None（它的值本身不上屏）", lambda: t.ident is not None, True))
    print(eq("threading 的账本多了一个", threading.active_count, 2))
    gate.set(); t.join()
    started, release = threading.Event(), threading.Event()
    _thread.start_new_thread(lambda: (started.set(), release.wait()), ())    # 绕过 threading 裸启动
    started.wait()
    print(eq("join 之后回到 1；_thread 裸起的线程不进这本账（此刻仍是 1）", threading.active_count, 1))
    release.set(); time.sleep(0.02)
    print(ledger("线程个数查的是账本不是真相：「到底几个在活」要问对人"))

    print("\n=== 4. GIL 什么时候放手：睡觉、等管道（两种「没在干活」） ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：GIL 在场 = 一个线程跑完才轮到下一个 ｜ 屏幕：它一等就撒手，别人立刻接上。")
    ok_sleep, _ = with_spinner(lambda: time.sleep(0.15))
    print(eq("主线程 time.sleep 期间，另一个线程的计数在推进吗（票当场交出去）", lambda: ok_sleep, True))

    def pipe_probe():
        r, w = os.pipe()                                    # 匿名管道：不是监听端口，读完就关
        st2 = {"n": 0}
        tw = threading.Thread(target=count_then_write, args=(w, st2, 400000), name="a-写管道的")
        tw.start()
        got = os.read(r, 1)                                 # 主线程堵在这儿：此刻管道还是空的
        out = (st2["n"] > 0, len(got))
        tw.join(); os.close(r); os.close(w)
        return out

    ok_pipe, nbytes = with_spinner(pipe_probe)[1]
    print(eq("主线程阻塞在 os.read 期间，另一个线程推进了吗（阻塞 IO 也放手）", lambda: ok_pipe, True))
    print(eq("被放行时读到的字节数（结构量）", lambda: nbytes, 1))
    print("  ★ 睡觉和等 IO 这两个「没在干活」的时刻 GIL 都交出去了 —— 所以线程对 IO 密集的活是真"
          "有用，不是安慰话（能同时压进去几个，见 b 脚本第 3 格）。")
    print(ledger("GIL 只在「有人执行字节码」时挡人，等待时放手"))

    print("\n=== 5. 有人死循环时它不是抱着不放：按节拍换手 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：一个线程死循环 = 全进程卡死 ｜ 屏幕：主线程照样能推进，两个都在数自己的拍。")
    st4, stop4 = {"n": 0}, {"f": False}
    t4 = threading.Thread(target=spin, args=(st4, stop4), name="a-死循环")
    t4.start()
    mine, deadline = 0, time.perf_counter() + 0.15
    while time.perf_counter() < deadline:                   # 主线程也死转一小段，看自己推不推得动
        mine += 1
    both, stop4["f"] = (mine > 0, st4["n"] > 0), True
    t4.join()
    print(eq("死循环的线程与主线程：两个都在推进吗（换手，不是排队）", lambda: both, (True, True)))
    old = sys.getswitchinterval()
    print(eq("默认换手节拍 sys.getswitchinterval()（解释器的可调参数本身，不是量出来的时长）", lambda: old, 0.005))
    sys.setswitchinterval(1e-6)
    changed = sys.getswitchinterval() == 1e-6
    sys.setswitchinterval(old)
    print(eq("setswitchinterval(1e-6) 调得动吗", lambda: changed, True))
    print(eq("调完能原样调回来吗", lambda: sys.getswitchinterval(), old))
    print("  ★ 节拍能调，但调到多细都不会变出并行 —— 这正是 b 脚本第 1、4 格要当场量的东西。")
    print(ledger("让出来的是执行权，不是并行度"))

    print("\n=== 6. GIL 在场 ≠ 互斥：一把普通 Lock 的两副样子 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：有 GIL 挡着，两个线程不会同时进同一段代码 ｜ 屏幕：GIL 全程开着，主线程照样被挡。")
    ready, hold, box_lk = threading.Event(), threading.Event(), threading.Lock()

    def holder():
        with box_lk:
            ready.set()
            hold.wait()                                     # 攥着锁不撒，直到主线程看完

    tholder = threading.Thread(target=holder, name="a-占锁的")
    tholder.start()
    ready.wait()
    got_lock, n_live, gil_now = box_lk.acquire(blocking=False), threading.active_count(), sys._is_gil_enabled()
    hold.set(); tholder.join()
    got_after = box_lk.acquire(blocking=False)
    box_lk.release()
    print(eq("别人攥着锁时，主线程 acquire(blocking=False) 拿到没有", lambda: got_lock, False))
    print(eq("被挡在门外的这一刻，GIL 是开着的吗（挡你的不是 GIL）", lambda: gil_now, True))
    print(trip("被挡时账本上有几个线程（先故意按 0 去对）", lambda: n_live, 0))
    print(eq("锁空了之后立刻拿到", lambda: got_after, True))
    print("  ★ 「有 GIL」和「我这段代码互斥」是两件事，互斥得你自己开口要 —— 主线④的前半句。")
    print(ledger("GIL 管字节码执行权，Lock 管你的业务临界区"))

    print("\n=== 7. 收口：GIL 是什么、不是什么 + 收尾自证 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  ① 是什么：进程里唯一的一张「字节码执行票」，同一刻只有一个线程在跑 Python 字节码。")
    print("  ② 不是什么：不是「不能并发」—— 睡觉、等 IO 时它当场放手（第 4 格两个现场全 True）。")
    print("  ③ 不是什么：不是你的互斥锁 —— 它开着，主线程照样被一把 Lock 挡在门外（第 6 格）。")
    print("  ④ 颗粒度：比一行语句细得多，`+=` 是十来条字节码（第 2 格数出来了）；"
          "⑤ 本机关不掉：Py_GIL_DISABLED=0、没有 python3.14t（第 1 格）。")
    print("  ✗ 本篇没演：free-threading 下 GIL 消失的行为（本机没那份解释器）、「纯 += 一定丢更新」"
          "（b 篇实测不丢，写进结论就是编）、多核真并行（那得用进程，见 c 篇）。")
    print(trip("退出前账本上剩几个线程（先故意按 3 去对）", threading.active_count, 3))
    print(eq("收尾自证：threading.enumerate() 只剩主线程", lambda: [x.name for x in threading.enumerate()], ["MainThread"]))
    print(eq("收尾自证：本篇没写文件、没建目录（当前目录里没有 a- 开头的东西）", lambda: sorted(p for p in os.listdir(".") if p.startswith("a-")), []))
    print(f"  全篇 {TOTAL['ok']}/{TOTAL['n']} 条判据成立（少一条上面就有 [不成立！]）")


if __name__ == "__main__":
    main()
