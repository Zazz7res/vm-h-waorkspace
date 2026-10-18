#!/usr/bin/env python3
# ============================================================================
# 取证脚本 b_threads_two_faces.py —— 第 11 章第 2 篇：线程的两副面孔（主线①④）
# 要证明什么：同一把 GIL 之下，线程对 CPU 密集是「白交锁税」，对 IO 密集是真并发；
#   而「有 GIL」不等于「线程安全」：纯 `+=` 在本机丢不了，但读→让出→写就必丢，
#   同一写法加一把 Lock 又一条不丢 —— 原子性不是「大概率没事」，是「中间有没有人插队」。
#   最后一副面孔是退出：daemon 线程被硬切，非 daemon 的活儿解释器必须等它干完。
# demo.sh 里这样调：`python3 b_threads_two_faces.py`
# 安全：不联网、不开任何监听端口；只在当前目录自建的 b-threads-demo/ 里写一个给子进程跑的 .py
#   和它的 marker 文件，收尾 rmtree 删干净；所有线程 join、子进程由 subprocess.run 收走。
# ★ 本机不拿什么当证据：① 屏上不出现任何时长/pid/内存地址/丢更新后的具体数字 —— 所有计时只在
#   内存里比大小，出来的一律是布尔判定，阈值都带大余量；② 「纯 += 一定会丢更新」本机演不出来
#   （4 线程 × 20 万次、两种换手节拍都实测不丢），所以那一格只能报「没丢」；③ 「4 进程 = 4 倍速」
#   也演不出来，那格在 c 篇。
# ============================================================================
import os, pathlib, shutil, subprocess, sys, threading, time

WORK = pathlib.Path("b-threads-demo")
N = 3_000_000                      # 一份 CPU 活的循环圈数：这个数不上屏，只负责把差距拉开
CELL = {"n": 0, "ok": 0}
TOTAL = {"n": 0, "ok": 0}

CHILD_SRC = '''"""daemon_child.py：主线程走到结尾而不 join，daemon 与非 daemon 两种命运各跑一遍。"""
import pathlib
import sys
import threading
import time

def work():
    for _ in range(20):                      # 一小段有界的活，干完才落盘
        time.sleep(0.02)
    pathlib.Path("marker_" + sys.argv[1]).write_text("干完了")

t = threading.Thread(target=work, name="b-活干到一半", daemon=(sys.argv[1] == "daemon"))
t.start()
print("ALIVE " + str(t.is_alive()))           # 主线程这就走到结尾，不 join
'''


def mask(text):
    return str(text).replace(os.path.expanduser("~"), "<家>").replace(str(pathlib.Path.cwd()), "<cwd>")


def eq(tag, th, want):
    """判据行：实测与手写预期逐字比对，错了当场印 [不成立！]。"""
    try:
        got = th()
    except Exception as exc:
        got = f"{type(exc).__name__}: {mask(exc)}"
    CELL["n"] += 1; CELL["ok"] += 1 if got == want else 0
    TOTAL["n"] += 1; TOTAL["ok"] += 1 if got == want else 0
    return f"  [{'成立' if got == want else '不成立！'}] {tag}：实测 {repr(got)} ｜ 预期 {repr(want)}"


def trip(tag, th, wrong):
    """尺子自检：计数类判据先故意拿错的预期量一遍，必须对不上 —— 证明这把尺真的会响。"""
    got = th()
    CELL["n"] += 1; CELL["ok"] += 1 if got != wrong else 0
    TOTAL["n"] += 1; TOTAL["ok"] += 1 if got != wrong else 0
    return f"  [{'尺子能响' if got != wrong else '尺子坏了！'}] {tag}：故意按 {wrong} 去对，实测 {got} 对不上"


def ledger(title):
    return f"  → 断言：{title} ｜ 本格 {CELL['ok']}/{CELL['n']} 条成立（不成立的会印 [不成立！]）"


def burn(n):
    s = 0
    for i in range(n):                        # 纯 CPU 活：全程都在抢那张字节码执行票
        s += i
    return s


def run(k, fn, serial=False):
    """k 个线程各跑一份 burn(N)（或串行跑 k 份），返回时长与结果。时长只在内存里比大小。"""
    if serial:
        t0 = time.perf_counter()
        res = [fn(N) for _ in range(k)]
        return time.perf_counter() - t0, res
    res = [None] * k
    ts = [threading.Thread(target=lambda i=i: res.__setitem__(i, fn(N))) for i in range(k)]
    t0 = time.perf_counter()
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    return time.perf_counter() - t0, res


def bump_plain(box, n, lk=None):
    for _ in range(n):
        box["n"] += 1                         # 最朴素的自增：读和写之间没人刻意让出


def bump_yield(box, n, lk=None):
    for _ in range(n):
        if lk:
            with lk:                          # 让出过一次也不放锁：别人插不进队
                v = box["n"]
                time.sleep(0)
                box["n"] = v + 1
        else:
            v = box["n"]
            time.sleep(0)                     # 读 → 让出 → 写：让出去那一下就是插队的门
            box["n"] = v + 1


def run_box(k, n, fn, lk=None):
    """k 个线程一起改同一个 box，只回两样东西：等不等于 k×n、和它自己（数字不上屏）。"""
    box = {"n": 0}
    ts = [threading.Thread(target=fn, args=(box, n, lk)) for _ in range(k)]
    for t in ts:
        t.start()
    for t in ts:
        t.join()
    return box["n"] == k * n


def main():
    print("=== 1. CPU 密集第一副面孔：4 个线程干 4 份，比 1 个线程干 1 份还慢 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：机器 4 个核，开 4 个线程就该 4 倍快 ｜ 屏幕：4 份活的总时长比 1 份还长。")
    t1, r1 = run(1, burn)
    t4, r4 = run(4, burn)
    print(eq("4 个线程做完 4 份活，比 1 个线程做 1 份快过 20% 吗（真并行该快好几倍）", lambda: t4 < t1 * 0.8, False))
    print(eq("是不是反而更慢（4 份的时长 ≥ 1 份的时长）", lambda: t4 >= t1, True))
    print(eq("活有没有白干：4 份结果全等于公式值吗（跟快慢无关）", lambda: r4 == [N * (N - 1) // 2] * 4, True))
    print("  ★ 活儿一件没少干、结果全对，时间一分没省 —— 4 个线程在排队领同一张票。")
    print(ledger("CPU 密集：线程数买来的是排队，不是并行"))

    print("\n=== 2. 顺着往下推：线程翻倍、活也翻倍，时长跟着翻倍 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：os.cpu_count() 是 4，8 个线程总该把 4 个核榨干 ｜ 屏幕：只是队排得更长。")
    t4b, _ = run(4, burn)
    t8b, _ = run(8, burn)
    print(eq("os.cpu_count()", os.cpu_count, 4))
    print(eq("8 线程干 8 份，比 4 线程干 4 份快过 30% 吗（有并行该差不多长）", lambda: t8b < t4b * 0.7, False))
    print(eq("时长是不是跟着活儿一起涨（≥ 4 份那次的 1.5 倍）", lambda: t8b >= t4b * 1.5, True))
    print(eq("同样 4 份活：4 线程 vs 1 线程串行跑 4 份，快过 30% 吗", lambda: run(4, burn)[0] < run(4, burn, True)[0] * 0.7, False))
    print("  ★ 屏幕上实际：线程数 ≠ 并行度。票只有一张，排队的人越多，每个人等得越久。")
    print(ledger("GIL 下的并行度不随线程数增长"))

    print("\n=== 3. IO 密集第三副面孔：真的能同时挂着 4 个 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：GIL 在场，等待也得排队 ｜ 屏幕：等待的人不占票，4 个能同时挂在 IO 上。")

    def peak(k, serial):
        live, hi = [0], [0]
        lk = threading.Lock()

        def waiter():
            with lk:
                live[0] += 1
                hi[0] = max(hi[0], live[0])
            time.sleep(0.1)                   # 挂在「等」上：这一段 GIL 是撒手的
            with lk:
                live[0] -= 1

        ts = [threading.Thread(target=waiter) for _ in range(k)]
        t0 = time.perf_counter()
        for t in ts:
            t.start()
            if serial:
                t.join()                      # 串行排法：起一个就等它完
        for t in ts:
            t.join()
        return hi[0], time.perf_counter() - t0

    pk_par, dur_par = peak(4, False)
    pk_ser, dur_ser = peak(4, True)
    print(trip("并发排法下同时在场的最多几个（先故意按 0 去对）", lambda: pk_par, 0))
    print(eq("并发排法：同时挂在等待里的峰值（结构量）", lambda: pk_par, 4))
    print(eq("串行排法（起一个等一个）的峰值", lambda: pk_ser, 1))
    print(eq("并发那次的总时长短过串行那次的六成吗（真并发才可能）", lambda: dur_par < dur_ser * 0.6, True))
    print("  ★ 同一把 GIL：CPU 那两格人多不省时间，IO 这格 4 个人同时挂着 —— 差别只在"
          "「你的活卡在什么上面」，这就是选型的判据。")
    print(ledger("IO 密集：线程是真并发，因为 GIL 在等待时撒手"))

    print("\n=== 4. setswitchinterval 调细了会怎样：什么都不会变快 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：换手节拍调到 1e-6，线程就该「并行得更紧」 ｜ 屏幕：只是锁交得更勤。")
    old = sys.getswitchinterval()
    sys.setswitchinterval(1e-6)
    on_now = sys.getswitchinterval() == 1e-6
    t4_fine, _ = run(4, burn)
    sys.setswitchinterval(old)
    t4_def, _ = run(4, burn)
    print(eq("节拍真的被改成 1e-6 了吗", lambda: on_now, True))
    print(eq("调回之后 get 到的还是默认值吗", lambda: sys.getswitchinterval(), old))
    print(eq("节拍调细后，4 线程比 1 线程快过 20% 吗", lambda: t4_fine < t1 * 0.8, False))
    print(eq("调细那次比默认那次快过 30% 吗（只是多交了几次锁）", lambda: t4_fine < t4_def * 0.7, False))
    print("  ★ 能调的是「多久换一次人」，不是「几个人同时干」。这两条判据都是 False，别指望旋钮。")
    print(ledger("换手节拍是公平性旋钮，不是并行度旋钮"))

    print('\n=== 5. 丢更新①：最朴素的 box["n"] += 1，本机就是不丢 ===')
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：+= 是十来条字节码（a 篇第 2 格数过），4 个线程肯定丢一堆 ｜ 屏幕：一条没丢。")
    ok_def = run_box(4, 200_000, bump_plain)
    sys.setswitchinterval(1e-6)
    ok_fine = run_box(4, 200_000, bump_plain)
    sys.setswitchinterval(old)
    print(eq("默认节拍：4 线程 × 20 万次，结果还等于 4×20 万吗（等于 = 没丢）", lambda: ok_def, True))
    print(eq("把节拍调到 1e-6 再跑一遍，还等于吗", lambda: ok_fine, True))
    print("  ✗ 本机演不出来：「纯 += 一定丢更新」在这台机器上怎么试都不丢。所以本章不写这句话 —— "
          "下一格只改一个写法，丢就立刻丢给你看。")
    print(ledger("没丢 ≠ 安全：本机只是没给插队的机会"))

    print("\n=== 6. 丢更新②：读 → 让出一次 → 写，结构性必丢 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：中间只让出一次，概率小得很，大不了偶发 ｜ 屏幕：不是偶发，是每次都没几个留下。")
    ok_yield = run_box(4, 2000, bump_yield)
    print(trip("这条判据会不会本来就恒真（先故意按「不丢」去对它）", lambda: ok_yield, True))
    print(eq("4 线程 × 2000 次：结果还等于 8000 吗（不等于 = 丢了）", lambda: ok_yield, False))
    print(eq("同一写法换线程数（8 × 2000）也照样丢吗", lambda: run_box(8, 2000, bump_yield), False))
    print(eq("把圈数减到 4 线程 × 200 次，照样丢吗（不是人多才挤）", lambda: run_box(4, 200, bump_yield), False))
    print("  ★ 让出过一次，就够别人把同一个旧值再读一遍。「读→让→写」不是运气问题，是结构问题"
          " —— 这才是主线④要说的那件事。")
    print(ledger("原子性要的不是「大概率没事」，是「中间没人插队」"))

    print("\n=== 7. 丢更新③：同一写法加一把 Lock，一条不丢 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：加了锁总该把数字锁歪 ｜ 屏幕：慢是真的，数是对的。")
    lk = threading.Lock()
    print(eq("4 线程 × 2000 次（读→让→写，全程在锁里）：等于 8000 吗", lambda: run_box(4, 2000, bump_yield, lk), True))
    print(eq("同一把锁交给 8 个线程（8 × 2000）也一条不丢吗", lambda: run_box(8, 2000, bump_yield, lk), True))
    print(eq("把锁传 None 再跑一次，立刻又丢（这条判据不是恒真的）", lambda: run_box(4, 2000, bump_yield, None), False))
    print("  ★ 第 6 格与第 7 格唯一的变量就是 `with lk:` 那一行。丢与不丢之间隔的不是运气，是一把没人要的锁。")
    print(ledger("Lock 把「读→让→写」重新焊成一次不可插队的动作"))

    print("\n=== 8. 第三副面孔是退出：daemon 被硬切，非 daemon 必须等 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：线程起了就跑得完，进程走了它也就走了 ｜ 屏幕：daemon 的活被拦腰砍断。")
    shutil.rmtree(WORK, ignore_errors=True)
    WORK.mkdir(parents=True)
    (WORK / "daemon_child.py").write_text(CHILD_SRC, encoding="utf-8")
    outs = {}
    for mode in ("daemon", "nodeaemon"):
        for f in WORK.glob("marker_*"):
            f.unlink()
        r = subprocess.run([sys.executable, "daemon_child.py", mode], cwd=str(WORK), timeout=60,
                           env={"PATH": os.environ.get("PATH", ""), "LC_ALL": "C.UTF-8", "PYTHONIOENCODING": "utf-8"},
                           capture_output=True, text=True)
        outs[mode] = (r.stdout.strip(), r.returncode, (WORK / f"marker_{mode}").exists())
    print(f"  子进程屏上：daemon 那次 → {mask(outs['daemon'][0])} ｜ 非 daemon 那次 → {mask(outs['nodeaemon'][0])}")
    print(eq("两次子进程退出码都是 0 吗（daemon 被硬切不算崩溃）", lambda: (outs["daemon"][1], outs["nodeaemon"][1]), (0, 0)))
    print(eq("主线程走到结尾那一刻，两种线程都还活着吗（屏上那句 ALIVE）",
             lambda: ("ALIVE True" in outs["daemon"][0], "ALIVE True" in outs["nodeaemon"][0]), (True, True)))
    print(eq("daemon 那次：活儿干完了吗（marker 文件在不在）", lambda: outs["daemon"][2], False))
    print(eq("非 daemon 那次：marker 在不在（解释器退出前替你等它）", lambda: outs["nodeaemon"][2], True))
    d = threading.Thread(target=lambda: time.sleep(0.05), daemon=True)
    d.start()
    alive_mid = d.is_alive()
    try:
        d.daemon = False
        flip = "没炸"
    except Exception as exc:
        flip = f"{type(exc).__name__}: {exc}"
    d.join()
    print(eq("本进程里当场问：正在跑的 daemon 线程 is_alive 吗", lambda: alive_mid, True))
    print(eq("join 之后还 is_alive 吗", lambda: d.is_alive(), False))
    print(eq("想把正在跑的线程改成非 daemon（报错原文照抄）", lambda: flip, "RuntimeError: cannot set daemon status of active thread"))
    print("  ★ pid 一律不上屏。屏幕上只有「marker 在不在」这一件事：daemon 那次不在。")
    print(ledger("daemon = 会被硬切的线程，非 daemon = 走之前必须等的线程"))

    print("\n=== 9. 收口：线程的两副面孔 + 本机没演的 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  ① CPU 密集：4 线程不但不快，还比 1 线程慢；8 线程 8 份活时长跟着翻倍（第 1、2 格）。")
    print("  ② IO 密集：4 个能同时挂在等待上（串行排法只有 1 个），时长也真省下来了（第 3 格）。")
    print("  ③ 换手节拍可调，但调不出并行（第 4 格）；调细之后 += 依然不丢（第 5 格）。")
    print("  ④ 有 GIL ≠ 线程安全：纯 += 本机不丢、读→让→写必丢、加锁又不丢 —— 三联（第 5~7 格）。")
    print("  ⑤ daemon 线程会被硬切，非 daemon 的活解释器替你等（第 8 格）。")
    print("  ✗ 本机没演：纯 += 丢更新（两种节拍都不丢，写进结论就是编）、「4 进程 = 4 倍速」"
          "（本机最多约 2 倍，见 c 篇）、free-threading 去掉 GIL（没那份解释器）。")
    print(trip("收尾自证：还剩几个线程（先故意按 5 去对）", threading.active_count, 5))
    print(eq("收尾自证：threading.enumerate() 只剩主线程", lambda: [x.name for x in threading.enumerate()], ["MainThread"]))
    print(f"  全篇 {TOTAL['ok']}/{TOTAL['n']} 条判据成立（少一条上面就有 [不成立！]）")


if __name__ == "__main__":
    try:
        main()
    finally:
        shutil.rmtree(WORK, ignore_errors=True)
        left = sorted(p.name for p in pathlib.Path(".").iterdir() if p.name.endswith("-demo"))
        print(f"  收尾自证：{WORK.name}/ 还在吗 → {WORK.exists()} ｜ 当前目录剩下的 *-demo → {left}"
              f"（本篇只创建并删除 {WORK.name}/ 这一个；__pycache__ 跟着沙箱一起没了）")
