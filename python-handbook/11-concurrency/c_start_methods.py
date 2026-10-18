#!/usr/bin/env python3
# ============================================================================
# 取证脚本 c_start_methods.py —— 第 11 章第 3 篇：forkserver 默认，与「没有 main 守卫」的现场
# 要证明什么（主线③）：本机 3.14 的 multiprocessing 默认 start method 是 **forkserver**，
#   而 forkserver/spawn 都会**重新 import 你的主模块** —— 没写 `if __name__ == "__main__":` 守卫
#   的脚本会被子进程整份重跑，当场炸出 RuntimeError（本脚本用 subprocess 跑三份小样本，
#   把原文逐字抄回屏上）。顺带量清：全局默认到底能不能中途换、三种 start method 在孩子眼里
#   分别看到什么、进程之间到底共享不共享（Value/Manager 共享，模块级列表不共享）、
#   maxtasksperchild、启动代价与加速倍数。
# demo.sh 里这样调：`python3 c_start_methods.py`
# 安全：不联网、不开任何监听端口；只在当前目录自建的 c-start-demo/ 里写 3 个给子进程跑的 .py，
#   收尾 rmtree 删干净；每个池 with 关掉、每个进程 join、Manager shutdown、子进程全带 timeout。
# ★ 本机不拿什么当证据：① 屏上没有计时数字/pid/内存地址，也没有丢更新的具体计数 —— 计时只在内存里
#   比大小，出来的是布尔判定，阈值都带大余量；② 「4 进程 = 4 倍速」本机给不出来（比值就在 2 倍上下），
#   所以只写「明显快过串行」和「没快到 1/4」两条；③ 「不带锁的跨进程 += 会不会丢」三遍跑会翻面，
#   所以第 6 格只演「加了锁共享得住」，不演「不加锁一定丢」；④ 跨机器的快慢排名一律不写。
# ============================================================================
import multiprocessing as mp
import os, pathlib, re, shutil, subprocess, sys, time

WORK = pathlib.Path("c-start-demo")
CELL = {"n": 0, "ok": 0}
TOTAL = {"n": 0, "ok": 0}
BIG, SMALL = 3_000_000, 300_000          # 两份活的大小：这两个数不上屏，只负责拉开差距
LOCAL_LIST = []                          # 模块级列表：孩子改它，父进程一点也看不见
LOCAL = {"n": 0}                         # 每个进程自己一份的计数器
SHARE_V, SHARE_ML, SHARE_LK = None, None, None   # 给池的 initializer 用：真共享的那几样

NOGUARD_SRC = '''"""noguard_child.py：故意不写 if __name__ == "__main__" 守卫。"""
import multiprocessing as mp

print("MARK 模块体被执行")

def work():
    print("WORK 子进程里这一行本来只该出现一次")

p = mp.Process(target=work)              # 模块级就起进程：本机默认 forkserver
p.start()
p.join()
print("END 主脚本结尾")
'''

GUARDED_SRC = '''"""guarded_child.py：加上守卫之后，三种 start method 各自在孩子眼里看到什么。"""
import multiprocessing as mp
import sys

MEM = []                                 # 模块级全局：fork 带得走，重新 import 的带不走

def probe(_):
    return "name=%s mem=%d argv=%s" % (__name__, len(MEM), sys.argv[0].rsplit("/", 1)[-1])

if __name__ == "__main__":
    print("MARK 模块体被执行")
    MEM.append(1); MEM.append(2); MEM.append(3)
    for meth in ("fork", "spawn", "forkserver"):
        with mp.get_context(meth).Pool(1) as pool:
            print(meth.upper(), pool.map(probe, [0])[0])

    def inner_target():                  # 故意把要干的活定义在守卫里面
        pass

    p = mp.get_context("forkserver").Process(target=inner_target)
    p.start()
    p.join()
    print("INNER_RC", p.exitcode)
'''

SETSRC = '''"""set_method_child.py：全局默认能不能中途换 —— 门槛不在「set 过」，在「问过」。"""
import multiprocessing as mp
import sys

mode = sys.argv[1]
if mode == "asked":                       # 先问一句普通 get_start_method()
    print("Q1 普通 get 到的默认：" + mp.get_start_method())
    try:
        mp.set_start_method("fork")
        print("Q2 set('fork')：成功了")
    except RuntimeError as exc:
        print("Q2 set('fork')：被拒 RuntimeError: " + str(exc))
    print("Q3 被拒之后 get 到的还是：" + mp.get_start_method())
else:                                     # 只问 allow_none=True，不敲定默认
    print("S1 只问 allow_none=True：" + str(mp.get_start_method(allow_none=True)))
    try:
        mp.set_start_method("fork")
        print("S2 set('fork')：成功了，get 到 " + mp.get_start_method())
    except RuntimeError as exc:
        print("S2 set('fork')：被拒 RuntimeError: " + str(exc))
    try:
        mp.set_start_method("spawn")
        print("S3 第二次不带 force：成功了")
    except RuntimeError as exc:
        print("S3 第二次不带 force：被拒 RuntimeError: " + str(exc))
try:
    mp.set_start_method("spawn", force=True)
    print(("Q4 " if mode == "asked" else "S4 ") + "带 force=True 再 set：get 到 " + mp.get_start_method())
except RuntimeError as exc:
    print("意外 force 也被拒 RuntimeError: " + str(exc))
'''


def mask(text):
    """把 cwd / 家目录 / 解释器根目录 / 内存地址从原文里折掉，其余一字不改。"""
    out = re.sub(r"0x[0-9a-fA-F]{3,}", "<地址>", str(text))
    for root, tag in ((str(pathlib.Path.cwd().resolve()) + os.sep, ""),
                      (os.path.expanduser("~") + os.sep, "<家>" + os.sep),
                      (sys.base_prefix + os.sep, "<pyroot>" + os.sep), (sys.executable, "python3")):
        if root:
            out = out.replace(root, tag)
    return out


def eq(tag, th, want):
    """判据行：实测与手写预期逐字比对，错了当场印 [不成立！]。"""
    try:
        got = th()
    except Exception as exc:
        got = f"{type(exc).__name__}: {mask(exc)}"
    CELL["n"] += 1; CELL["ok"] += 1 if got == want else 0
    TOTAL["n"] += 1; TOTAL["ok"] += 1 if got == want else 0
    return f"  [{'成立' if got == want else '不成立！'}] {tag}：实测 {mask(repr(got))} ｜ 预期 {mask(repr(want))}"


def trip(tag, th, wrong):
    """尺子自检：计数类判据先故意拿错的预期量一遍，必须对不上 —— 证明这把尺真的会响。"""
    got = th()
    CELL["n"] += 1; CELL["ok"] += 1 if got != wrong else 0
    TOTAL["n"] += 1; TOTAL["ok"] += 1 if got != wrong else 0
    return f"  [{'尺子能响' if got != wrong else '尺子坏了！'}] {tag}：故意按 {mask(repr(wrong))} 去对，实测 {mask(repr(got))} 对不上"


def ledger(title):
    return f"  → 断言：{title} ｜ 本格 {CELL['ok']}/{CELL['n']} 条成立（不成立的会印 [不成立！]）"


def run_child(name, src, *args):
    """把样本写进沙箱、用干净 env 起一个子进程跑它，返回 (stdout 行, stderr 行, 退出码)。"""
    (WORK / name).write_text(src, encoding="utf-8")
    r = subprocess.run([sys.executable, name, *args], cwd=str(WORK), timeout=120,
                       env={"PATH": os.environ.get("PATH", ""), "LC_ALL": "C.UTF-8", "PYTHONIOENCODING": "utf-8"},
                       capture_output=True, text=True)
    return r.stdout.splitlines(), r.stderr.splitlines(), r.returncode


def line_with(lines, prefix):
    hit = [ln for ln in lines if ln.startswith(prefix)]
    return hit[0] if hit else "(这一行没打出来)"


def field(line, key):
    """从 'name=xx mem=3 argv=yy' 里抠一个字段；行没打出来就返回哨兵，不让脚本自己炸。"""
    return line.split(key + "=")[1].split()[0] if key + "=" in line else "(没有)"


def burn(n):
    s = 0
    for i in range(n):
        s += i
    return s


def noop_task(_):
    return 0


def top_counter(_):
    LOCAL["n"] += 1
    return LOCAL["n"]                              # 本进程私有的「我被叫到第几次」


def local_append(_):
    LOCAL_LIST.append(1)
    return len(LOCAL_LIST)


def setup(v, ml, lk):
    global SHARE_V, SHARE_ML, SHARE_LK
    SHARE_V, SHARE_ML, SHARE_LK = v, ml, lk      # forkserver 的孩子靠 initializer 拿到这三样


def bump(_):
    with SHARE_LK:                              # 跨进程也要上锁：不加锁的 += 这里会偶发少加
        SHARE_V.value += 1


def push(_):
    SHARE_ML.append(1)


def pool_map(meth, workers, fn, args, pool_kw=None, map_kw=None):
    """开一个指定 start method 的池跑 map；池的构造参数和 map 的参数得分开传（chunksize 属于后者）。"""
    with mp.get_context(meth).Pool(workers, **(pool_kw or {})) as p:
        return p.map(fn, args, **(map_kw or {}))


def measure(fn, reps=3):
    """同一个动作连做 reps 次，只取最快的一次：别的进程在抢 CPU 时不至于把结论带跑。"""
    ts = []
    for _ in range(reps):
        t0 = time.perf_counter()
        fn()
        ts.append(time.perf_counter() - t0)
    return min(ts)


def main():
    print("=== 1. 本机默认是 forkserver：而且「有没有人设置过」这件事会被问话改变 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：默认 = fork（Linux 老印象）或 spawn（Windows 印象），问几次答案都一样。")
    print(eq("get_all_start_methods()（本机可供选择的三种）", mp.get_all_start_methods, ["forkserver", "fork", "spawn"]))
    print(eq("先问 allow_none=True：有人显式 set 过吗（没人 set = None）", lambda: mp.get_start_method(allow_none=True), None))
    print(eq("再问 get_start_method()：兜出来的是哪个", mp.get_start_method, "forkserver"))
    print(eq("回头再问同一句 allow_none=True，答案还是 None 吗", lambda: mp.get_start_method(allow_none=True), "forkserver"))
    print(eq("os.fork 在不在（fork 这条路本机物理上可走）", lambda: hasattr(os, "fork"), True))
    print(eq("当前进程名", lambda: mp.current_process().name, "MainProcess"))
    print("  ★ 屏幕上实际：本机默认是 forkserver，且没人 set 过；但「问一句 get_start_method()」本身"
          "就把默认上下文敲定了 —— 同一句 allow_none=True 前后两个答案。这一问之后，3.14 换的默认值"
          "立刻能在第 2 格把老代码炸掉。")
    print(ledger("本机默认 forkserver：没被 set 过，但会被第一问定死"))

    print("\n=== 2. 没有 main 守卫的脚本：被子进程整份重跑，当场炸 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：子进程只要我那个函数，不会碰我的主脚本 ｜ 屏幕：它把主模块整份重新 import 了一遍。")
    shutil.rmtree(WORK, ignore_errors=True)
    WORK.mkdir(parents=True)
    out2, err2, rc2 = run_child("noguard_child.py", NOGUARD_SRC)
    print(f"  子进程 stdout → {[mask(x) for x in out2]}")
    i = next((k for k, ln in enumerate(err2) if ln.startswith("RuntimeError:")), None)
    block = [mask(x) for x in err2[i:i + 3]] if i is not None else ["(没找到 RuntimeError)"]
    for ln in block:
        print(f"  │ {ln}")
    print(eq("「模块体被执行」出现了几次（父进程 1 次 + 子进程重跑 1 次 = 结构量）", lambda: sum(1 for x in out2 if x.startswith("MARK")), 2))
    print(eq("子进程那一遍真的跑到 work 了吗（WORK 那行次数：炸在半路 = 0）", lambda: sum(1 for x in out2 if x.startswith("WORK")), 0))
    print(eq("主脚本结尾那行打出来了吗（父进程在 p.start() 就断了）", lambda: any(x.startswith("END") for x in out2), False))
    print(eq("RuntimeError 第二行原文（逐字）", lambda: block[1] if len(block) > 1 else "", "        An attempt has been made to start a new process before the"))
    print(eq("RuntimeError 第三行原文（逐字）", lambda: block[2] if len(block) > 2 else "", "        current process has finished its bootstrapping phase."))
    print(eq("父进程被牵连出的报错原文", lambda: line_with(err2, "ConnectionResetError:"), "ConnectionResetError: [Errno 104] Connection reset by peer"))
    print(eq("报错里出现样本文件名了吗（证明它是按路径重新 import 主模块的）", lambda: sum(1 for x in err2 if "noguard_child.py" in x) > 0, True))
    print(eq("这份脚本的退出码（炸了就不是 0）", lambda: rc2, 1))
    print(ledger("forkserver 会重新 import 主模块：没有守卫 = 整份重跑 + 当场 RuntimeError"))

    print("\n=== 3. 同一份脚本加上守卫：只跑一次、干净退场 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：守卫只是个习惯写法，不改行为 ｜ 屏幕：它决定子进程那一遍 import 会不会再执行你的主逻辑。")
    out3, err3, rc3 = run_child("guarded_child.py", GUARDED_SRC)
    print(eq("「模块体被执行」出现了几次（这次只有父进程那 1 次）", lambda: sum(1 for x in out3 if x.startswith("MARK")), 1))
    print(eq("退出码", lambda: rc3, 0))
    print(eq("把活定义在守卫里面的下场：报错原文", lambda: line_with(err3, "AttributeError:"), "AttributeError: module '__mp_main__' has no attribute 'inner_target'"))
    print(eq("那次子进程的退出码（守卫挡不住「函数不在模块顶层」这件事）", lambda: line_with(out3, "INNER_RC"), "INNER_RC 1"))
    print("  ★ 屏幕上实际：守卫让「重跑」变成「只定义、不执行」；但要丢给孩子的函数必须在**模块顶层**，"
          "藏在 if 里面的那个，子进程照样按名字去模块上找、找不到。")
    print(ledger("有守卫 = import 时不执行主逻辑；函数还得写在守卫外面"))

    print("\n=== 4. fork / spawn / forkserver：三种方法在孩子眼里差在哪 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：三种方法只是启动快慢不同 ｜ 屏幕：差别在于「孩子有没有拿到你此刻的内存」。")
    got = {m: line_with(out3, m.upper()) for m in ("fork", "spawn", "forkserver")}
    for m in got:
        print(f"  │ {m} → {mask(got[m])}")
    print(eq("fork 那一路：__name__ 还是 __main__，模块级 MEM 带着 3 个元素（复印此刻内存）",
             lambda: (field(got["fork"], "name"), field(got["fork"], "mem")) == ("__main__", "3"), True))
    print(eq("spawn 那一路：__name__ 变成 __mp_main__、mem=0（重新 import 出一份干净的）",
             lambda: (field(got["spawn"], "name"), field(got["spawn"], "mem")) == ("__mp_main__", "0"), True))
    print(eq("forkserver 那一路：和 spawn 一样是重新 import（本机默认就是它）",
             lambda: (field(got["forkserver"], "name"), field(got["forkserver"], "mem")) == ("__mp_main__", "0"), True))
    print(eq("forkserver 与 spawn 两行去掉方法名之后逐字相同吗", lambda: got["spawn"].split(None, 1)[1:], got["forkserver"].split(None, 1)[1:]))
    print(eq("三路的命令行一样吗（差别不在 argv，在内存）", lambda: {field(x, "argv") for x in got.values()}, {"guarded_child.py"}))
    print("  ★ 屏幕上实际：fork 给你「此刻内存的复印件」，spawn/forkserver 给你「重新 import 的干净货」。"
          "所以 fork 能摸到父进程的模块级对象，本机默认那一路不能。")
    print(ledger("三种方法的真差别 = 继不继承内存；forkserver 与 spawn 同走重新 import"))

    print("\n=== 5. 能不能中途换 start method：门槛不是「set 过」，是「问过」 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：set_start_method 想调就调；只要我还没调过，就一定调得动 ｜ 屏幕：问一句就锁死了。")
    outQ, _, rcQ = run_child("set_method_child.py", SETSRC, "asked")
    outS, _, rcS = run_child("set_method_child.py", SETSRC, "quiet")
    for ln in outQ + outS:
        print(f"  │ {mask(ln)}")
    print(eq("先问普通 get_start_method()：拿到的是默认值而不是 None", lambda: line_with(outQ, "Q1"), "Q1 普通 get 到的默认：forkserver"))
    print(eq("问过之后再 set('fork')：报错原文（逐字）", lambda: line_with(outQ, "Q2"), "Q2 set('fork')：被拒 RuntimeError: context has already been set"))
    print(eq("被拒之后默认变了吗（没变，还是 forkserver）", lambda: line_with(outQ, "Q3"), "Q3 被拒之后 get 到的还是：forkserver"))
    print(eq("force=True 是唯一能把已敲定的默认掰开的门", lambda: line_with(outQ, "Q4"), "Q4 带 force=True 再 set：get 到 spawn"))
    print(eq("换个孩子：只问 allow_none=True（不敲定默认），拿到 None", lambda: line_with(outS, "S1"), "S1 只问 allow_none=True：None"))
    print(eq("没问过普通 get，set('fork') 就真的设上了", lambda: line_with(outS, "S2"), "S2 set('fork')：成功了，get 到 fork"))
    print(eq("set 过一次之后，第二次不带 force 的报错原文", lambda: line_with(outS, "S3"), "S3 第二次不带 force：被拒 RuntimeError: context has already been set"))
    print(eq("两个孩子的退出码都是 0（报错全被 except 兜住了）", lambda: (rcQ, rcS), (0, 0)))
    print(eq("本进程里 get_context('spawn') 自带的方法（随时可用的那扇门）", lambda: mp.get_context("spawn").get_start_method(), "spawn"))
    print(eq("用过 get_context 之后，本进程的全局默认被改掉了吗（不该被改）", lambda: mp.get_start_method(), "forkserver"))
    print(ledger("普通 get 一问就锁死；set 只能一次，force=True 才掰得开；get_context 不动全局"))

    print("\n=== 6. 进程之间共享什么：模块级列表不共享，Value / Manager 才共享 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：变量写在模块顶层，孩子进程当然看得见 ｜ 屏幕：它改的是自己那一份复印件。")
    LOCAL_LIST.clear()
    kids = pool_map("forkserver", 1, local_append, [0] * 4, pool_kw={"maxtasksperchild": 1})
    print(trip("「每个孩子各自数到的长度」这条判据会不会恒真（先故意按 [1,2,3,4] 去对）", lambda: kids, [1, 2, 3, 4]))
    print(eq("4 个任务各自 append 一次，孩子数到的都是 1（每个都是新副本）", lambda: kids, [1, 1, 1, 1]))
    print(eq("父进程这份模块级列表的长度：孩子改了 4 次，这边还是", lambda: len(LOCAL_LIST), 0))
    v = mp.Value("i", 0)
    lk = mp.Lock()
    m = mp.Manager()
    ml = m.list()
    with mp.Pool(2, initializer=setup, initargs=(v, ml, lk)) as p:
        p.map(bump, [0] * 8)
        p.map(push, [0] * 8)
    ok_v, ok_ml = (v.value == 8), (len(ml) == 8)
    m.shutdown()
    print(eq("mp.Value 被 2 个进程加锁加 8 次之后 .value == 8 吗（共享内存段，真看得见）", lambda: ok_v, True))
    print(eq("Manager 的 list 收到 8 条吗（跨进程代理，也真看得见）", lambda: ok_ml, True))
    print("  ★ 屏幕上实际：想让多个进程看同一份数据，只有「共享内存段」和「代理服务器」两条路；"
          "把普通变量写在模块顶层，等于给每个孩子发一份复印件。")
    print("  ✗ 本机演不出来（稳定版）：上面那条 bump 一开始是**不带锁**写的，三遍跑下来 .value == 8 有时"
          "成立有时不成立 —— 这正是丢更新，但它靠机器忙不忙，写不成一条能复现的判据，所以加了锁只证"
          "「共享看得见」这一件事；「纯 += 一定会丢更新」请按 b 篇的口径，本机演不出来。")
    print(ledger("不共享是默认，共享要专门挑工具（+ 跨进程也得配锁）"))

    print("\n=== 7. maxtasksperchild 与池复用：孩子干几票就换新 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：池里的 worker 是长命的，计数一路涨 ｜ 屏幕：设了上限就每几票换一个新人。")
    seq_def = pool_map("forkserver", 1, top_counter, [0] * 4, map_kw={"chunksize": 1})
    seq_1 = pool_map("forkserver", 1, top_counter, [0] * 4, pool_kw={"maxtasksperchild": 1}, map_kw={"chunksize": 1})
    seq_2 = pool_map("forkserver", 1, top_counter, [0] * 6, pool_kw={"maxtasksperchild": 2}, map_kw={"chunksize": 1})
    print(eq("不设上限（1 个 worker 干 4 票）：私有计数一路涨", lambda: seq_def, [1, 2, 3, 4]))
    print(eq("maxtasksperchild=1（干 1 票换人）：每次报到的都是新人", lambda: seq_1, [1, 1, 1, 1]))
    print(eq("maxtasksperchild=2（干 2 票换人）：1,2 之后重新从 1 开始", lambda: seq_2, [1, 2, 1, 2, 1, 2]))
    print(trip("每票换人那条序列其实根本没换人（故意按 [1,2,1,2,1,2] 那种「一人干两票」的排法去对）", lambda: seq_1, [1, 2, 1, 2, 1, 2]))
    print(eq("换人换得干不干净（结构量：不设上限有 4 种报到序号，每票换人只剩 1 种）", lambda: (len(set(seq_def)), len(set(seq_1))), (4, 1)))
    with mp.get_context("forkserver").Pool(1) as p:      # 同一个池，分两批派活
        batch_a = p.map(top_counter, [0] * 3, chunksize=1)
        batch_b = p.map(top_counter, [0] * 3, chunksize=1)
    print(eq("同一个池分两批派活：第二批是从 1 重来还是接着涨（接着涨 = worker 真被复用）", lambda: (batch_a, batch_b), ([1, 2, 3], [4, 5, 6])))
    print("  ✗ 本机演不出来：「同一个池第二批一定更便宜」。跑三遍能看到两批谁快谁慢会翻面，"
          "这种依赖机器状态的排名一律不写成结论；上面只留「第二批序号接着涨」这条不依赖时间的结构判据。")
    print(ledger("worker 寿命能设上限；池复用是真的（第二批序号接着涨），但「第二批更便宜」本机不打包票"))

    print("\n=== 8. 启动代价与加速倍数：fork 最省，4 进程 ≠ 4 倍速 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  直觉：进程比线程重一点而已；4 个 worker 就是 4 倍快 ｜ 屏幕：重得多，而且远不到 4 倍。")
    t_fork = measure(lambda: pool_map("fork", 1, noop_task, [0]))
    t_spawn = measure(lambda: pool_map("spawn", 1, noop_task, [0]))
    t_solo = measure(lambda: burn(SMALL))
    t_pooled = measure(lambda: pool_map("forkserver", 1, burn, [SMALL]))
    t_serial = measure(lambda: [burn(BIG) for _ in range(4)])
    t_pool4 = measure(lambda: pool_map("forkserver", 4, burn, [BIG] * 4))
    print(eq("「起一个 1 worker 的池跑 1 个空任务」：fork 明显比 spawn 便宜吗（不到 0.8 倍）", lambda: t_fork < t_spawn * 0.8, True))
    print(eq("一份小活丢进 1-worker 池，比主进程自己跑还贵吗（超过 1.5 倍 = 启动成本）", lambda: t_pooled > t_solo * 1.5, True))
    print(eq("4 个 worker 干 4 份同样的活，比串行 4 份明显快吗（阈值故意放宽到 1.2 倍）", lambda: t_pool4 < t_serial / 1.2, True))
    print(eq("够到 4 倍速了吗（时间只剩 1/4 才算；这条该是 False）", lambda: t_pool4 * 4 <= t_serial, False))
    print("  ✗ 本机演不出来：「4 进程 = 4 倍速」。比值就在 2 倍上下，所以本章只写「明显快过串行」"
          "和「没到 4 倍」—— 核数 + 启动成本 + 数据搬运，谁都不欠你 4 倍。"
          "（上面四条都连做三次只取最快的一次，免得被隔壁抢 CPU 的进程带跑。）")
    print(ledger("进程能真并行，但要付启动成本，加速远不如个数好看"))

    print("\n=== 9. 收口：本机这条默认值该怎么记 + 收尾自证 ===")
    CELL["n"] = CELL["ok"] = 0
    print("  ① 本机 3.14 默认 forkserver，三种可选，没人 set 过 —— 但问一句 get_start_method() 就定死了（第 1、5 格）。")
    print("  ② forkserver/spawn 都重新 import 主模块：没有 main 守卫 → 整份重跑 + RuntimeError 原文（第 2 格）。")
    print("  ③ 光有守卫不够，丢给孩子的函数必须在模块顶层（第 3 格那条 AttributeError）。")
    print("  ④ fork 给复印件（继承此刻内存），spawn/forkserver 给干净货；命令行看不出差别（第 4 格）。")
    print("  ⑤ 想中途换默认只剩 force=True；平时要用 get_context(meth)（第 5 格）。")
    print("  ⑥ 模块级变量不跨进程共享，Value/Manager 才共享；跨进程改同一个数照样要配锁（第 6 格）；worker 寿命用 maxtasksperchild 设上限（第 7 格）。")
    print("  ⑦ 进程能真并行但只到约 2 倍，小活进池反而更慢（第 8 格）：「4 进程 = 4 倍速」本机演不出来。")
    print(trip("收尾自证：还有几个 multiprocessing 子进程活着（先故意按 3 去对）", lambda: len(mp.active_children()), 3))
    print(eq("收尾自证：mp.active_children() 已清空（池 with 关了、进程 join 了、Manager shutdown 了）", lambda: mp.active_children(), []))
    print(eq("收尾自证：本进程名没变", lambda: mp.current_process().name, "MainProcess"))
    print(f"  全篇 {TOTAL['ok']}/{TOTAL['n']} 条判据成立（少一条上面就有 [不成立！]）")


if __name__ == "__main__":
    try:
        main()
    finally:
        shutil.rmtree(WORK, ignore_errors=True)
        left = sorted(x.name for x in pathlib.Path(".").iterdir() if x.name.endswith("-demo"))
        print(f"  收尾自证：{WORK.name}/ 还在吗 → {WORK.exists()} ｜ 当前目录剩下的 *-demo → {left}"
              f"（本篇只创建并删除 {WORK.name}/ 这一个；__pycache__ 跟着沙箱一起没了）")
