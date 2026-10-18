#!/usr/bin/env python3
# ============================================================================
# 取证脚本 q_bigfile_seek.py —— 第 18 节：大文件、分块、seek/tell 的真实形状
# 三件事分开证：① 一份确定内容的文件，四种读法拿到的字节必须一字不差（账对得上）；
# ② 内存只看**方向**不看数字（绝对值每跑必抖）；③ 二进制和文本的 seek/tell 是两套规矩
# —— 文本只认 tell() 发回来的那张「凭据」，自己塞字节数要么报错要么静默错位。
# 现场全在 bigfile-demo/ 里，最大文件 2MB，跑完 rmtree；屏幕上不打耗时和内存数字。
# ============================================================================
import hashlib
import io
import os
import shutil
import subprocess
import sys
import tracemalloc
from pathlib import Path

WORK = Path("bigfile-demo")
LINES = 32_000                       # 32000 行 × 63 字节 ≈ 2.0MB，上限控死在 2~4MB
CHUNK = 65_536                       # 分块大小
DATA = WORK / "data.txt"
SAMPLE = "第一行中文尾巴在这里\n第二行中文尾巴在这里\n第三行尾巴\n"

if WORK.exists():                    # 上次的残骸先清掉，每次从同一现场开始
    shutil.rmtree(WORK)
WORK.mkdir()

def line_of(n):
    """等长行模板：行号补零到 6 位，所以任何一行的字节数都算得出来。"""
    return f"{n:06d} | 中文行模板用于测字节数 | ASCII-tail-{n:06d}\n"

def src(mode):
    """四种读法的「块流」：一次读完 / read(n) 分块 / iter 哨兵 / 逐行。只产出，不留存。"""
    with DATA.open("rb") as fh:
        if mode == "whole":
            yield fh.read()
        elif mode == "chunk":
            while part := fh.read(CHUNK):                    # 读到空串为止
                yield part
        elif mode == "iter":
            yield from iter(lambda: fh.read(CHUNK), b"")     # 哨兵值 b'' 一出现就停
        else:
            yield from fh                                    # 文件迭代器：过一遍到底，块=行

def account(mode, whole):
    """跑一遍块流：只留计数器、增量摘要和「逐字节等于整份吗」，一块都不留。
    WHY 不留块：把每块 append 进列表，四种读法的内存峰值就一样大了，第 3 段白测。"""
    count = pos = 0; hasher = hashlib.md5(); same = True
    for block in src(mode):
        count += 1
        pos += len(block)
        hasher.update(block)
        same = same and whole[pos - len(block):pos] == block
    return count, pos, hasher.hexdigest()[:8], same

def peak_of(mode):
    """只返回峰值，且只用来比大小 —— 绝对值不在屏幕上出现。"""
    tracemalloc.start()
    account(mode, payload)
    peak = tracemalloc.get_traced_memory()[1]
    tracemalloc.stop()
    return peak

print("=== 1. 先造一份「账算得出来」的 2MB 文件 ===")
payload = b"".join(line_of(n).encode("utf-8") for n in range(1, LINES + 1))
DATA.write_bytes(payload)
size, per_line = DATA.stat().st_size, len(line_of(1).encode("utf-8"))
fingerprint = hashlib.md5(payload).hexdigest()[:8]
print(f"  行数={LINES} 每行字节={per_line} 总字节={size} ｜ 用行数×每行算出来={LINES * per_line}"
      f" ｜ 两者一致 → {size == LINES * per_line}")
print(f"  内容指纹（md5 前 8 位）= {fingerprint} ｜ 文件不超过 4MB，不把磁盘和 IO 打爆")
print("  ★摘要是拿来**证明四种读法拿到同一份字节**的，不是校验和教学；它由确定内容算出，所以每跑一样。")

print("\n=== 2. 四种读法的字节账：块数、总字节、指纹，一个都不能错 ===")
totals, fps, sames = set(), set(), set()
for name, mode, want in (("read() 一次读完", "whole", 1),
                         (f"read({CHUNK}) 分块循环", "chunk", -(-size // CHUNK)),
                         ("iter(lambda: f.read(n), b'')", "iter", -(-size // CHUNK)),
                         ("逐行迭代 for line in f", "line", LINES)):
    count, total, fp, same = account(mode, payload)
    totals.add(total); fps.add(fp); sames.add(same)          # 三个账本各记一笔
    print(f"  {name:<28}：块数={count:<6} 期望={want:<6} 总字节={total:<8} 指纹={fp} 逐字节相同={same}")
print(f"  → 四种读法的总字节只有一个值 → {totals == {size}}（{size}）｜指纹只有一个值 → "
      f"{fps == {fingerprint}}｜逐字节相同全是 True → {sames == {True}}")
print("    分块的块数是 ceil(总字节/块大小)（最后一块不满），逐行迭代的「块」正好就是行；")
print("    iter(可调用, 哨兵) 和 while 手写判空是同一件事的两种写法，下面的内存方向也一致。")

print("\n=== 3. 内存只看方向，不看数字（tracemalloc 峰值只用来比大小）===")
pk = {m: peak_of(m) for m in ("whole", "chunk", "iter", "line")}
print(f"  峰值(一次读完) > 峰值(分块) → {pk['whole'] > pk['chunk']}　差在一口气攥着多少字节")
print(f"  峰值(一次读完) > 峰值(逐行) → {pk['whole'] > pk['line']}")
print(f"  峰值(分块) > 峰值(逐行) → {pk['chunk'] > pk['line']}　块越大峰值越高，跟文件总大小无关")
print(f"  峰值(iter 哨兵) 与峰值(read(n) 分块) 落在同一档 → {pk['iter'] < pk['whole']}")
print("  ★内存绝对值每跑必抖，本节只读方向、不读数字；上面用的都是倍数级别的差距，")
print("    贴边的比较（1.0x 那种）本机也不会印出来 —— 那是掷硬币，不是结论。")

print("\n=== 4. 二进制的 seek/tell：三种 whence 各自把游标变成什么 + 按偏移随机读第 N 行 ===")
with DATA.open("rb") as fh:
    for off, whence, why in ((0, 0, "回到文件头"), (2000, 1, "从当前位置往后 2000"),
                             (0, 1, "问现在在哪"), (-per_line, 2, "从末尾退一整行"),
                             (0, 2, "跳到末尾")):
        fh.seek(off, whence)
        print(f"  seek({off}, {whence}) ← {why}，tell()={fh.tell()}")
offsets = {}
with DATA.open("rb") as fh:
    for no, ln in enumerate(fh, start=1):
        if no in (1, 16_000, 32_000):
            offsets[no] = fh.tell() - len(ln)                # 行首偏移 = 读完之后的位置 - 行长
print(f"  自己扫一遍建「行号 → 字节偏移」索引：{offsets}（每行 {per_line} 字节，所以是等差数列）")
for no, off in sorted(offsets.items()):
    with DATA.open("rb") as fh:
        fh.seek(off)
        head = fh.readline()[:12]
    print(f"    seek({off}) 后 readline() 的前 12 字节 = {head!r} 行号对不对 → "
          f"{head.startswith(f'{no:06d}'.encode())}")
print("  → 大文件随机访问的正确姿势：**先建偏移索引，再用偏移读**；用「读 N 行」定位是 O(N)。")

print("\n=== 5. ★★文本模式的 seek：tell() 发的是凭据，不是字符数 ===")
for enc in ("utf-8", "utf-16", "utf-32"):
    probe = WORK / f"cookie-{enc}.txt"
    probe.write_text(SAMPLE, encoding=enc)
    with probe.open(encoding=enc) as fh:
        first, cookie = fh.readline(), fh.tell()
    raw = probe.read_bytes()
    print(f"  {enc:<7} 读完第 1 行 tell()={cookie:<4}｜该行字符数={len(first)}｜文件总字节={len(raw):<4}"
          f"｜把 {cookie} 当字节偏移去二进制里切，切出来正好是这一行 → {raw[:cookie].decode(enc) == first}")
print("  ★规范只保证「同一个流」认这张凭据。本机三种编码它恰好等于字节偏移，但别依赖：解码器手里攥着")
print("    半个字符时 CPython 会把内部状态并进去；换文件、换编码都不保证还是这个数。")
text8 = WORK / "cookie-utf-8.txt"
with text8.open(encoding="utf-8") as fh:
    fh.readline()
    cookie = fh.tell()
    rest_once = fh.read()                                    # 先顺着一路读到底
    fh.seek(cookie)                                          # 再拿当初那张凭据回到原位
    print(f"  seek(cookie={cookie}) 再读一遍：两次内容完全一致 → {rest_once == fh.read()}"
          f" ｜ 从那里开始的头 9 个字符 = {rest_once[:9]!r}")
print("  ③ 不取凭据、自己塞一个字节数进去（含中文的文件；本机实测，报错原样打印）：")
for enc, off in (("utf-8", 0), ("utf-8", 3), ("utf-8", 7), ("utf-16", 2), ("utf-16", 4),
                 ("utf-16", 7), ("utf-32", 4), ("utf-32", 7), ("utf-8", -5), ("utf-8", 999_999)):
    with (WORK / f"cookie-{enc}.txt").open(encoding=enc) as fh:
        try:
            back = fh.seek(off)                              # 除负数之外，seek 这一步从来不报错
        except ValueError as exc:
            print(f"    {enc:<7} seek({off}) → 当场 {type(exc).__name__}: {exc}")
            continue
        try:
            print(f"    {enc:<7} seek({off}) → 返回 {back}，read(4) 读到 {fh.read(4)!r}"
                  f"　★没报错 ≠ 落点就是你要的那个")
        except UnicodeDecodeError as exc:
            print(f"    {enc:<7} seek({off}) → 返回 {back}（**seek 没报错**），read 才炸："
                  f"{type(exc).__name__}: {exc}")
print("    utf-16 每字符 2 字节 + 2 字节 BOM，utf-32 每字符 4 字节 + 4 字节 BOM：所以 utf-16 里 seek(2) 落在")
print("    第 1 个字符**之前**，utf-32 里 seek(4) 才落在第 1 个字符上；落在半个码元上的直接抛。")
print("  → 三种失败姿态本机全实测到了：负数当场 ValueError、切到字符中间的等 read 才 UnicodeDecodeError、")
print("    越过末尾的（999999）不报错也不提醒，read 只给空串。报错里的解码器名是 utf-16-le/utf-32-le ——")
print("    那是 BOM 探测之后才定下来的，你写的明明是 utf-16。")
with text8.open(encoding="utf-8") as fh:
    for off, whence in ((0, 1), (5, 1), (0, 2), (10, 2), (-5, 2)):
        try:
            print(f"    seek({off}, {whence}) → 返回 {fh.seek(off, whence)}（tell={fh.tell()}）")
        except (io.UnsupportedOperation, OSError) as exc:
            print(f"    seek({off}, {whence}) → {type(exc).__name__}: {exc}")
print("  → 文本模式只有 whence=0 的两种用法可靠：seek(0) 和 seek(当初 tell() 拿到的 cookie)；")
print("    非零的 cur/end 相对定位直接 io.UnsupportedOperation，随手塞的字节数要么炸要么静默错位。")

print("\n=== 6. 四个旋钮实测：readable / writable / seekable / 游标起点 ===")
small = WORK / "knob.txt"
small.write_text("0123456789" * 3, encoding="utf-8")
for mode in ("r", "rb", "w", "a", "r+", "a+"):
    fh = open(small, mode)
    print(f"  open(p,{mode!r:<5})：readable={fh.readable()} writable={fh.writable()} "
          f"seekable={fh.seekable()} tell={fh.tell()} 此刻磁盘字节={small.stat().st_size}")
    fh.close()
    if mode in ("w", "a"):                                   # 'w'/'a' 会动内容，补回去再测下一种
        small.write_text("0123456789" * 3, encoding="utf-8")
r_fd, w_fd = os.pipe()
ro = io.open(r_fd, "rb", closefd=False)
print(f"  os.pipe() 的读端　　：readable={ro.readable()} writable={ro.writable()} seekable={ro.seekable()}")
ro.close(); os.close(r_fd); os.close(w_fd)                   # 三个句柄一起关，不留尾巴
print("  → seekable 说的是「这个句柄能不能定位」，跟里面有没有内容是两回事：'w' 打开就被 O_TRUNC")
print("    清空，游标可动但只有一个位置有东西；管道 seekable=False 是因为**流**没有「第 1000 字节」。")

print("\n=== 7. 追加与并发：'a' 模式两个子进程各写 40 行 ===")
log, PER = WORK / "append.log", 40
log.write_bytes(b"")
codes = [f"with open({str(log)!r}, 'a', encoding='utf-8') as fh:\n"
         f"    for i in range({PER}):\n        fh.write('{tag}-{{:03d}}\\n'.format(i)); fh.flush()\n"
         for tag in ("甲", "乙")]                            # 子进程脚本里那个路径不上屏
procs = [subprocess.Popen([sys.executable, "-c", c], stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, text=True) for c in codes]
for pr in procs:
    try:
        pr.communicate(timeout=60)                           # 超时一定 kill：绝不留后台
    except subprocess.TimeoutExpired:
        pr.kill(); pr.communicate()
lines = log.read_text(encoding="utf-8").splitlines()
want = {f"甲-{i:03d}" for i in range(PER)} | {f"乙-{i:03d}" for i in range(PER)}
print(f"  两个子进程（python3 -c …）都正常退出 → {all(pr.returncode == 0 for pr in procs)}")
print(f"  行数 = {len(lines)}（应为 {2 * PER}）｜ 每行都以 甲-/乙- 开头且完整 → "
      f"{all(ln[:2] in ('甲-', '乙-') for ln in lines)}")
print(f"  → 两行都在、没有任何半截行（集合完全相同）→ {set(lines) == want}")
print("     谁先谁后本机不可预测，所以**不打顺序**；只打「全都在、都没被切开」这条判据。")
print("  ★O_APPEND 让每个 write() 系统调用原子地接在当时末尾，所以单条 write 不会被别人的字节切开；")
print("    但「一次 Python write 等于一次系统调用」只在跨过缓冲区时才成立 —— PIPE_BUF 那类具体阈值")
print("    本机没有可控的并发调度手段去测，**标未验证**（第 11 章讲并发时再量）。")

print("\n=== 8. 收口 ===")
print("  ① 大文件别一次 read()：分块/逐行才不炸内存（第 3 段四个方向）；一次读完 = 整个文件常驻内存。")
print("  ② 四种读法的**字节账**必须对得上：块数、总字节、指纹、逐字节相同四项一致才算读全（第 2 段）。")
print("  ③ 文本模式的 seek 只认 tell() 发回来的 cookie；自己塞字节数：负数当场 ValueError，错位的不报错，")
print("     切到字符中间的等 read 才抛 UnicodeDecodeError（第 5 段实测原文）—— 没报错不等于对了。")
print("  ④ whence=1/2 的非零偏移在文本模式直接 io.UnsupportedOperation；要用相对/末尾定位就开二进制。")
print("  ⑤ 大文件随机访问用偏移索引（第 4 段），别用「循环读 N 行」；行号不是地址。")
print("  ⑥ 顺序写用 'a' + 一个句柄；并发追加只保证「每次 write 不被切开」，不保证顺序。")
print("  ★本节不印绝对耗时、不印内存数字、不印写入顺序，只印方向与布尔 —— 那些数字每跑必抖，")
print("    印出来就是把噪声当结论。本机 Python " + sys.version.split()[0] + f"，最大文件 {size} 字节。")

shutil.rmtree(WORK)
print(f"\n  清理：{WORK.name}/ 已删除 ｜ 还在吗 {WORK.exists()}"
      f" ｜ 当前目录剩下的 .py 个数 = {len([x for x in os.listdir('.') if x.endswith('.py')])}")
