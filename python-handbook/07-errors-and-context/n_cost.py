#!/usr/bin/env python3
# ============================================================================
# 取证脚本 n_cost.py —— 第 15 节：EAFP vs LBYL，以及异常到底值多少钱
#
# 目标：把「try 很贵」这句老话量出来 —— 贵的不是 try，是 raise 和格式化。
# 数字每跑必抖，本节只当**量级**看；能复现的是比值，不是绝对值。
# ============================================================================
import subprocess
import sys
import timeit
import traceback

MISSING = object()          # 哨兵：值本身可能就是 None，所以「没有」要造一个独一无二的对象（第 7 段现场对比）
N = 1_000_000               # 每次测多少遍
N_SMALL = 100_000


def us(seconds, number):
    """总秒数 → 平均每次多少微秒。"""
    return f"{seconds / number * 1e6:.3f}"


def eafp(d, k):
    try:
        return d[k]
    except KeyError:
        return None


def lbyl(d, k):
    if k in d:
        return d[k]
    return None


def get_sentinel(d, k):
    return d.get(k, MISSING)


def cost_raise_caught():
    try:
        raise ValueError("x")
    except ValueError:
        pass


def cost_big_msg():
    try:
        raise ValueError("x" * 500)
    except ValueError:
        pass


def rec(k):
    if k:
        return rec(k - 1)
    raise ValueError("x")


def cost_deep():
    try:
        rec(50)
    except ValueError:
        pass


big_payload = ["占位"] * 3
EMPTY = iter([])


def next_manual():
    try:
        return next(EMPTY)
    except StopIteration:
        return None


def make():
    local_blob = {"秘密": big_payload, "编号": 7}
    raise ValueError("这条异常会攥着 local_blob 不放")


def only_str():
    try:
        make()
    except ValueError as exc:
        return str(exc)


def full_traceback():
    try:
        make()
    except ValueError:
        return traceback.format_exc()


G = globals()
D = {"k": 1}

print("=== 1. 什么都不抛的 try：贵不贵？ ===")
t_eafp = timeit.timeit("eafp(D, 'k')", globals=G, number=N)
t_lbyl = timeit.timeit("lbyl(D, 'k')", globals=G, number=N)
t_get = timeit.timeit("get_sentinel(D, 'k')", globals=G, number=N)
print(f"  命中时（不进 except 分支）跑 {N:,} 次：")
print(f"    EAFP  try/except = {us(t_eafp, N)} µs/次")
print(f"    LBYL  if k in d  = {us(t_lbyl, N)} µs/次")
print(f"    哨兵  d.get(k, MISSING) = {us(t_get, N)} µs/次")
print(f"  → LBYL / EAFP = {t_lbyl / t_eafp:.2f}：LBYL 每一次都查了两遍哈希（先 in 再 []），")
print("    所以**不抛**的 try 不但不要钱，还比 LBYL 快。这就是「包一层 try 不是负担」的意思。")

print("\n=== 2. 一旦真的抛：同一个函数，未命中时完全是另一个世界 ===")
t_eafp_miss = timeit.timeit("eafp(D, 'nope')", globals=G, number=N)
t_lbyl_miss = timeit.timeit("lbyl(D, 'nope')", globals=G, number=N)
print(f"  未命中时跑 {N:,} 次：")
print(f"    EAFP（每次都抛+接）  = {us(t_eafp_miss, N)} µs/次")
print(f"    LBYL（if 判掉，不抛）= {us(t_lbyl_miss, N)} µs/次")
print(f"  → EAFP / LBYL = {t_eafp_miss / t_lbyl_miss:.2f} 倍；")
print(f"    同一个 EAFP：命中 {us(t_eafp, N)} → 未命中 {us(t_eafp_miss, N)}，差 "
      f"{t_eafp_miss / t_eafp:.1f} 倍。")
print("  结论：**拿异常当普通控制流**（每次都走 except）是真的慢；")
print("    拿异常报**真正的失败**不慢，因为失败本来就该是少数事件。")

print("\n=== 3. 贵的三件事分别是什么（拆开量，跑 " + f"{N_SMALL:,} 次）===")
t_raise = timeit.timeit("cost_raise_caught()", globals=G, number=N_SMALL)
t_build = timeit.timeit("ValueError('x')", globals=G, number=N_SMALL)
t_big = timeit.timeit("cost_big_msg()", globals=G, number=N_SMALL)
t_deep = timeit.timeit("cost_deep()", globals=G, number=N_SMALL)
print(f"  ① raise + 立刻接住       ：{us(t_raise, N_SMALL)} µs/次")
print(f"  ② 只构造实例、不抛        ：{us(t_build, N_SMALL)} µs/次  ← 纯构造其实很便宜")
print(f"  ③ raise 带 500 字长消息    ：{us(t_big, N_SMALL)} µs/次")
print(f"  ④ 从 50 层深的调用里 raise ：{us(t_deep, N_SMALL)} µs/次  ← 这一条最贵")
print(f"  比值：④/① = {t_deep / t_raise:.1f} ｜ ③/① = {t_big / t_raise:.2f} ｜ ①/② = {t_raise / t_build:.2f}")
print("  → 只有 ④ 是真的贵（差几十倍）：**栈越深越贵**，因为要给每一帧建 traceback 记录。")
print(f"    ③ 和 ① 的比值是 {t_big / t_raise:.2f}，落在噪声里 —— 说明**消息多长几乎不影响**：")
print("    raise 只把参数原样塞进 exc.args，字符串本身不格式化、不裁剪。格式化发生在你打印它的时候（第 5 段）。")
print("    ①/② 说的是：抛 + 接比「只造对象」贵好几倍，但绝对值仍是 0.1 µs 级 ——")
print("      真正的问题不是它贵，是它**每次都要付**，所以别把它放进热循环当普通分支用。")
t_for_empty = timeit.timeit("for _ in EMPTY: pass", globals=G, number=N)
t_manual_next = timeit.timeit("next_manual()", globals=G, number=N)
print(f"  对照：空迭代器上走一遍 for（内部靠 StopIteration 收尾）= {us(t_for_empty, N)} µs/次，")
print(f"        手动 next 到底再接住                        = {us(t_manual_next, N)} µs/次")
print("    → for 把那个异常**在解释器内部**接掉了，不建 traceback、不格式化，所以便宜得多；")
print("      自己手写 next() 循环时才会真的付这份钱（`__iter__`/`__next__` 那套本手册没演，")
print("      第 04 章第 17 节只到生成器和惰性为止）。")

print("\n=== 4. traceback 顺手把栈帧和局部变量一起攥住了（内存代价）===")
try:
    make()
except ValueError as exc:
    tb = exc.__traceback__
    depth = 1
    while tb.tb_next is not None:          # 走到**最深**那一帧，才是出事现场那一层
        tb = tb.tb_next
        depth += 1
    kept = tb.tb_frame.f_locals
    print("  帧链一共", depth, "层；最深那层的局部变量键 =", sorted(kept))
    print("  里面那份大对象还活着吗 →", kept["local_blob"]["秘密"] is big_payload)
    print("  → 把一个 exception 对象长期存着（塞进全局、存进缓存、挂到返回值上）=")
    print("    把出事那一刻**整层局部变量**一起留着（帧对象攥着 f_locals，f_locals 攥着你的数据）。")
    print("    正解：只存 type(exc).__name__ / str(exc) / exc.args，需要行号就存 extract_tb 的结果。")

print("\n=== 5. 把整份 traceback 写进日志，比只存 str 贵多少（跑 " + f"{N_SMALL:,} 次）===")
t_str = timeit.timeit("only_str()", globals=G, number=N_SMALL)
t_full = timeit.timeit("full_traceback()", globals=G, number=N_SMALL)
print(f"  只 str(exc)：{us(t_str, N_SMALL)} µs/次 ｜ 整份 format_exc()：{us(t_full, N_SMALL)} µs/次"
      f" ｜ 比值 = {t_full / t_str:.0f}")
print("  → 差**两个数量级**：format 一份 traceback 要读源码文件、排版每一帧。")
print("    但故障是少数事件，这点钱**该花**（第 16 节第 3 段：少了帧你查不到是哪一行）。")
print("    不该做的是在**正常路径**上每次成功都 format 一份。")

print("\n=== 6. format 出来的东西长什么样（跑在子进程里，免得把本机绝对路径打进这份输出）===")
demo = "def make():\n    blob = ['占位'] * 3\n    raise ValueError('消息就这一句')\n\n" \
       "try:\n    make()\nexcept ValueError:\n    import traceback; print(traceback.format_exc(), end='')\n"
proc = subprocess.run([sys.executable, "-c", demo], capture_output=True, text=True, check=False)
lines = proc.stdout.rstrip("\n").splitlines()
print(f"  这一次 {len(lines)} 行：")
for line in lines:
    print("    |", line)
print("  → 每帧下面多出来的那行是**源码原文**（第 7 节的 .line 字段），带 caret 指出表达式位置；")
print("    这就是第 5 段那笔开销的来源：它得去把源码文件读进来（linecache）。")

print("\n=== 7. EAFP / LBYL 的选型判据（不靠口味，靠这三条）===")
print("  ① 这条路**常见**吗？常见 → 它是正常流程，别用异常表达（第 2 段量过：每次都抛贵几十倍）。")
print("     反例：`for x in items: try: use(x) except TypeError: ...`，十次有九次进 except → 先判类型。")
print("  ② 检查和使用之间会不会**变**？会 → LBYL 不可靠：先 os.path.exists(p) 再 open(p)，")
print("     中间文件可能被人删掉（这叫 TOCTOU）。本机起不了并发竞争，**这条只做记录、不下结论**；")
print("     但「查过 ≠ 还在」这个方向本身就够说明为什么 Python 偏爱直接试。")
print("  ③ 检查本身贵不贵？`k in d` 一次哈希 + `d[k]` 又一次 = 白付一半（第 1 段实测）。")
print("     一次查完的正解是哨兵：")
d2 = {"a": None, "b": 1}
for key in ("a", "b", "zz"):
    got = d2.get(key, MISSING)
    tag = "真的没有" if got is MISSING else f"有，值是 {got!r}（None 也分得清）"
    print(f"     d.get({key!r}, MISSING) → {tag}")

print("\n=== 8. 收口 ===")
print("  ① 不进 except 的 try 几乎免费（本机 3.14 实测；社区叫它「零成本异常」，")
print("     从哪个版本开始免费的本章不验证），「包一层」本身不是负担。")
print("  ② 贵的是**真的抛**：栈深 × traceback × 格式化。异常只用来报「异常」。")
print("  ③ 常见路径走正常返回（None 哨兵见第 05 章第 6 节，多值打包成元组见第 12 节），少数情况才 raise。")
print("  ④ 别长期持有 exception 对象：它攥着整层局部变量（第 4 段现场）。")
print("  ⑤ 上面所有 µs 数字每跑必抖，只当量级看；本节唯一稳的结论是**比值的大小顺序**。")
