#!/usr/bin/env python3
# ============================================================================
# 取证脚本 c_close_and_iter.py —— 第 4 节：close 由谁执行、一次性的文件迭代器、四种读法的内存账
#
# 证三件事：① 没写 close 的时候到底是谁在关 fd —— 引用计数、循环引用要等的 gc、被异常扣住的
# 那一帧，各演一遍，「关没关」用 os.fstat 问内核，不看 f.closed 自己怎么报；② 文件对象**就是**
# 它自己的迭代器：同一个句柄第二次 list(f) 是空的、一声不响（第 07 章第 18 节那条规律的展开）；
# ③ read / readlines / 逐行 / 分块 四种读法的内存账，差的只有方向。
# 为什么要有它：三件全是「不报错的错」—— 第二次读到空、fd 多一个、内存翻两倍，屏幕上一条警告
# 都没有，代码照常往下跑，所以每条都得配一个能失败的实测。
# 现场全在 close-iter-demo/ 里现造现删（章目录零产物），只打相对路径。内存那段刻意**一个字节数都
# 不印**：tracemalloc 峰值每跑必抖，印绝对值就是假证据（本章 README 立的规矩）。
# 所有故意失败都被 try/except 接住并原样打印异常名和消息。退出码恒 0；跑两遍输出逐字一致。
# ============================================================================
import gc
import os
import shutil
import tracemalloc
import warnings
from pathlib import Path

WORK = Path("close-iter-demo")
SMALL = 5            # 迭代器段样本：5 行，行数和字节数都是我们亲手写的
ROWS = 10000         # 内存段样本：1 万行 × 每行 63 字节 = 630000 字节（确定值）
WIDTH = 63
CHUNK = 8192         # 分块读法一次取多少个字符


def fd_open(fd):
    """问内核「这个号上还有活的文件吗」：比读 f.closed 硬，因为它不经过任何 Python 对象。"""
    try: os.fstat(fd); return True
    except OSError: return False


def bang(call):
    """真跑一次 call()：没炸就交出返回值，炸了就交出异常原文 —— 原文才是本章要的证物。"""
    try:
        return f"居然没报错 → {call()!r}"
    except Exception as exc:
        return f"{type(exc).__name__}: {exc}"


def build(name, text=""):
    """写一个内容完全已知的文件（text 可以是空串），返回 (路径, 字节数, 其中换行符个数)。"""
    path = WORK / name
    with open(path, "w", encoding="utf-8") as fh: fh.write(text)
    return path, os.path.getsize(path), text.count("\n")


def measure(action):
    """跑一次 action，只从 tracemalloc 取一个用来比大小的整数 —— 这个整数永远不上屏幕。"""
    gc.collect(); tracemalloc.start(); tracemalloc.reset_peak()
    result = action()
    peak = tracemalloc.get_traced_memory()[1]
    del result; tracemalloc.stop()
    return peak


shutil.rmtree(WORK, ignore_errors=True)      # 上次万一崩在这里留下的残骸：先清干净，现场每次一样
WORK.mkdir()

print("=== 1. f.closed 的三个时刻，和关完之后每一句话的报错原文 ===")
path, size, _ = build("账本.txt", "第一行\n第二行\n第三行\n")
fh = open(path, encoding="utf-8")
fd = fh.fileno()
print(f"  ① open 之后：f.closed={fh.closed} ｜ 拿这个号问内核还开着吗 → {fd_open(fd)}")
fh.close()
print(f"  ② 自己 close() 之后：f.closed={fh.closed} ｜ 同一个号问内核 → {fd_open(fd)}")
twice_ok = True
try: fh.close(); fh.close()
except Exception as exc: twice_ok = f"{type(exc).__name__}: {exc}"
print(f"  再 close 一次 → 不报错吗 {twice_ok}（close 幂等，重复关不炸；f.closed 仍是 {fh.closed}）")
_, other_size, _ = build("另一个.txt", "0" * (size * 3))
rb = open(WORK / "另一个.txt", "rb")
reused, fstat_size = rb.fileno() == fd, os.fstat(fd).st_size == other_size
rb.close()
print("  关完再碰它，每个方法的原文一个字符都没改（二进制那两行用的就是上面刚建好又刚关掉的那个句柄）：")
for layer, label, call in (("文本", "read()", lambda: fh.read()), ("文本", "write()", lambda: fh.write("字")),
                           ("文本", "tell()", lambda: fh.tell()), ("文本", "seek(0)", lambda: fh.seek(0)),
                           ("文本", "flush()", lambda: fh.flush()), ("文本", "fileno()", lambda: fh.fileno()),
                           ("二进制", "read(1)", lambda: rb.read(1)), ("二进制", "close()", lambda: rb.close())):
    print(f"    {layer} {label:<9} → {bang(call)}")
print(f"  ★ 那个二进制句柄拿到的号，就是刚被关掉的旧号 → {reused}；拿旧号去 fstat，量到的是**新**文件的")
print(f"     字节数 → {fstat_size}。号是内核的整数，用完立刻复用，别存下来当「我还开着」的证据。")
with open(path, encoding="utf-8") as wf:
    wf_fd, inside = wf.fileno(), wf.closed
print(f"  ③ with 退出之后：块体内 closed={inside} → 块体外 closed={wf.closed} ｜ 同一个号问内核 → {fd_open(wf_fd)}")
print("  → 唯一可信的字段是 f.closed：文本层五句共用一条消息、fileno() 只少一个句号，二进制层按方法各写一条 → 接异常请写 except ValueError，别去匹配消息字符串。")
print("  → close 幂等 + 关完再碰才炸，就是第 07 章第 10 节「__exit__ 重复调用也安全」的实测版。")

print("\n=== 2. ★你没写 close 的时候是谁在关：引用计数 / 循环引用 / 被异常扣住 / with ===")
with warnings.catch_warnings(record=True) as quiet:
    fh = open(path, encoding="utf-8"); fd = fh.fileno(); del fh; alive_after_del = fd_open(fd)
print(f"  A 只 del 名字、一次 close 都没写 → fd 还开着吗 {alive_after_del}（是 dealloc 替你关的）；屏幕上有过任何提示吗 → {len(quiet) > 0}（默认过滤级别对 ResourceWarning 是 ignore）")
with warnings.catch_warnings(record=True) as loud:
    warnings.simplefilter("always"); fh = open(path, encoding="utf-8"); del fh
print(f"     把过滤拨到 always 才看得见这一条：{[f'{w.category.__name__}: {w.message}' for w in loud]}")


def leaky(mode):
    """故意不 close 就返回：'plain' 干净返回，'tb' 让异常把整帧连同句柄一起扣住。"""
    handle = open(path, encoding="utf-8")
    if mode == "plain": return handle.fileno()
    try: raise RuntimeError("落盘前炸在半路")
    except RuntimeError as exc: return handle.fileno(), exc


fd_plain = leaky("plain")
print(f"  B 函数返回、局部句柄出作用域（全程没 close）→ fd 还开着吗 {fd_open(fd_plain)}")
fd_tb, saved = leaky("tb")
with_exc = fd_open(fd_tb); del saved
print(f"  C 异常对象被我存下来 → 它那一帧里的句柄还开着吗 {with_exc}；放掉异常之后 {fd_open(fd_tb)}")


class Leaker:
    """自引用环：me 指回自己，引用计数永远不会归零。"""
    def __init__(self, handle): self.handle, self.me = handle, self


holder = Leaker(open(path, encoding="utf-8")); fd = holder.handle.fileno()
del holder
before_gc = fd_open(fd); gc.collect()
print(f"  D 句柄被循环引用扣住：del 之后 fd 还开着吗 {before_gc} → gc.collect() 之后 {fd_open(fd)}")
print("  → A/B 那种「忘了 close 也没出事」，是 CPython 引用计数恰好补了刀：C 的异常把整帧扣住、D 的环")
print("    要等分代回收、模块级名字更不会归零 —— 这三种形状下 fd 真的在漏，不是保证。")
fh = open(path, encoding="utf-8")
print(f"  with 的三条合同逐条实测（第 07 章第 10 节的协议落在文件上）：fh.__enter__() is fh → {fh.__enter__() is fh}；fh.__exit__(None, None, None) → 返回 {fh.__exit__(None, None, None)!r}（假值 = 不吞异常）｜ 此时 closed={fh.closed}")
half = WORK / "半截.txt"
try:
    with open(half, "w", encoding="utf-8") as wf:
        wf.write("写了一半"); raise RuntimeError("块体里崩了")
except RuntimeError as exc:
    print(f"     块体抛异常，外面接住：{type(exc).__name__}: {exc}")
print(f"     异常路径上 wf.closed → {wf.closed} ｜ 磁盘上留下 {os.path.getsize(half)} 字节：{half.read_text(encoding='utf-8')!r}")
print("  → with 只保证 close（close 里含 flush），不保证写完、更不保证落盘：半截文件不违反合同；要「要么")
print("    全有要么全无」得走临时文件 + os.replace，本章第 17 节。")

print("\n=== 3. ★文件对象就是它自己的迭代器：第二次 list(f) 是空的，而且不报错 ===")
sample, sample_size, _ = build("五行.txt", "".join(f"line-{i}\n" for i in range(SMALL)))
fh = open(sample, encoding="utf-8"); first = list(fh); tell_mid, second = fh.tell(), list(fh)
print(f"  现场：{sample.as_posix()} = {sample_size} 字节 / {SMALL} 行。第一次 list(f) 拿到 {len(first)} 行（首行 {first[0]!r}）")
print(f"  读完这一刻 tell()={tell_mid}，正好等于文件字节数 → {tell_mid == sample_size}")
print(f"  同一个句柄第二次 list(f) → {second}，长度 {len(second)}，全程没接任何异常 ← ★这一格")
back = fh.seek(0)
print(f"  现场恢复：seek(0) 返回 {back}（新的绝对位置），重扫一遍又拿到 {len(list(fh))} 行 → 游标真被挪回开头")
print(f"  它是迭代器的三条硬证：iter(f) is f → {iter(fh) is fh} ｜ type(iter(f)).__name__ → {type(iter(fh)).__name__}"
      f" ｜ hasattr(f, '__len__') → {hasattr(fh, '__len__')}")
fh.seek(0)
lines = fh.readlines()
print(f"  readlines() 给的是容器：type={type(lines).__name__}、{len(lines)} 个元素，一次抱完；f 自己不是容器（__len__ 那格 False），它只在你要求时才往前走一步 —— 两本账的差在第 4 段量")
fh.close()
h = open(sample, encoding="utf-8"); one, rest = h.readline(), list(h)
print(f"  游标只有一根，readline 和 list 共用：先 readline 拿走 {one!r}，接着 list 只剩 {len(rest)} 行（{rest[0]!r} 开头）")
h.seek(0); next(h)
mid_tell, raw_ahead = bang(h.tell), h.buffer.raw.tell()
h.seek(0)
print(f"  只迭代过一行就问 tell() → {mid_tell}；seek(0) 之后再问 → {bang(h.tell)}（一次 seek 拿回许可）")
print(f"  → 拒答是因为底层已经替你读过头：拿到第 1 行后 f.buffer.raw.tell()={raw_ahead}，整个文件才 {sample_size} 字节")
h.close()
print("  → 「配置文件读两遍」的翻车形状：第二遍得到 []、长度 0、不报错、循环一次都不进，程序带着空配置")
print("    跑到底。要重读要么 seek(0)，要么第一遍就 list 住 —— 那就付 readlines 的账。")

print("\n=== 4. 四种读法的内存账：只印大小关系，不印任何绝对值 ===")
big = WORK / "一万行.txt"
with open(big, "w", encoding="utf-8") as fh:
    for i in range(ROWS): fh.write(f"line-{i}".ljust(WIDTH - 1, ".") + "\n")
size = os.path.getsize(big)
print(f"  现造样本：{big.name} = {size} 字节 / {ROWS} 行 / 每行 {WIDTH} 字节（本段唯一的确定数字）")


def read_style(how):
    """同一个样本文件五种过法：两种读完就丢（逐行、分块），三种整块抱住（read、readlines、留住）。"""
    with open(big, encoding="utf-8") as fh:
        if how == "read": return fh.read()
        if how == "readlines": return fh.readlines()
        if how == "iter": return sum(1 for _ in fh)
        if how == "chunk": return sum(len(p) for p in iter(lambda: fh.read(CHUNK), ""))
        return [line for line in fh]          # 逐行但用列表留住


STYLES = ("read()", "readlines()", "for line in f", f"read({CHUNK}) 分块", "for 但用列表留住")
PEAK = dict(zip(STYLES, [measure(lambda k=k: read_style(k)) for k in ("read", "readlines", "iter", "chunk", "kept")]))
for style in STYLES:
    print(f"  {style:<18} 峰值 ≥ 文件大小 → {PEAK[style] >= size} ｜ 峰值 < 文件大小 → {PEAK[style] < size}")
print(f"  两两比：read > 逐行 → {PEAK[STYLES[0]] > PEAK[STYLES[2]]} ｜ readlines > 逐行 → {PEAK[STYLES[1]] > PEAK[STYLES[2]]}"
      f" ｜ read > 分块 → {PEAK[STYLES[0]] > PEAK[STYLES[3]]} ｜ 留住 > 不留 → {PEAK[STYLES[4]] > PEAK[STYLES[2]]}")
print(f"  顺手拆一条传闻：readlines 峰值 > read 峰值 → {PEAK[STYLES[1]] > PEAK[STYLES[0]]}（本机同档，都不在「省」那一侧）")
print("  内存绝对值每跑必抖，本节不拿它当证据 —— 只读方向")
print("  → 分档只看一条线：读完就丢（逐行 / 分块）是常数级，整块抱住（read / readlines）在文件量级之上。")
print("  ★ 「逐行省内存」的前提是你**不留它**：最后一档 [line for line in f] 和 readlines() 同档（「留住 > 不留」那一格实测），写列表推导的那一刻账已经付掉了。")

print("\n=== 5. 空文件、最后一行没有换行符：[] 还是 ['abc'] 还是 ['abc\\n'] ===")
print("  内容              字节        换行符     list(f)                        行数==换行符")
for tag, content in (("空文件", ""), ("无尾换行", "abc"), ("带尾换行", "abc\n"),
                     ("只有换行", "\n\n"), ("多行无尾", "a\nbb\nccc")):
    p, nbyte, nl = build(f"形状-{tag}.txt", content)
    with open(p, encoding="utf-8") as fh: lines = list(fh)
    print(f"  {content!r:<16} {nbyte:<11} {nl:<10} {str(lines):<32} {len(lines) == nl}")
with open(WORK / "形状-空文件.txt", encoding="utf-8") as fh: empty_read, empty_list = fh.read(), list(fh)
with open(WORK / "形状-无尾换行.txt", encoding="utf-8") as fh: last_line, eof = fh.readline(), fh.readline()
print(f"  两个边界值一起抄走：空文件 read() → {empty_read!r}、list(f) → {empty_list}（不是 ['']）；缺尾换行时最后一行")
print(f"     readline() → {last_line!r}，再 readline() → {eof!r} ← 空串才是 EOF 哨兵，所以 while line := fh.readline() 判空串、不是 None。")
print("  → 分行按「读到 EOF 为止」，不按「看见换行符为止」：最后一行没有换行符也照样给你一行；两个连续")
print("    换行是**两行**空行、不是三行；行里自带定长换行符，所以 rstrip 是取内容的必要动作（第 5、12 节）。")
print("  ★ 最后一列只有两行 False，都是「缺尾换行」的文件：逐行计数比磁盘上换行符的个数多 1 —— 行数跟别")
print("    处（日志、别人的校验脚本）对不上时，第一个要问的就是最后一个字节是不是换行符。")

print("\n=== 6. 收口：close 归谁、迭代器一次性、内存只看方向 ===")
print("  ① close 只做三件事：flush、释放 fd、幂等。关完再碰是 ValueError，且文本层与二进制层消息不同（第 1 段照抄过）；f.closed 是唯一可信的字段，fileno() 的号会被内核立刻复用。")
print("  ② 「忘了 close 也没出事」是 CPython 引用计数的巧合：循环引用等 gc、异常扣着一帧、全局引用永不归零 —— 这三种形状下 fd 真漏。所以 with 不是风格，是唯一的合同。")
print("  ③ with 的合同只有 close：块体抛异常它照样关（实测 closed=True），但不管写全、不管落盘。")
print("  ④ 文件对象就是它自己的迭代器：iter(f) is f、第二次 list(f) 是 [] 且不报错、游标只有一根（readline/for/list 共用）；重读要么 seek(0)，要么第一遍就 list 住。")
print("  ⑤ 读法分两档：读完就丢（逐行、分块）常数级，整块抱住（read、readlines）文件量级之上；峰值绝对值每跑必抖，所以本章只印大小关系。留住一行就等于付 readlines 的账。")
print("  ⑥ 空文件给 []（不是 ['']），缺尾换行照样给最后一行，那种文件上逐行计数比换行符个数多 1。")
print("  ★ 这里没有一条来自记忆：每格都是当场撞出来的原文或布尔值，跑两遍逐字一致 —— 能重复的才叫证据。")

shutil.rmtree(WORK)
print(f"  收尾自证：{WORK.name}/ 还在吗 → {WORK.exists()}（现场全部现造现删，章目录零产物）")
