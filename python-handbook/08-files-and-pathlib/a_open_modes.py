#!/usr/bin/env python3
# ============================================================================
# 取证脚本 a_open_modes.py —— 第 2 节：open() 的模式字符到底改了哪三件事
#
# 要证明什么：mode 那一两个字符只拨三个开关 —— ① 能不能读 ② 能不能写 ③ 游标起点在
# 哪，外加一个藏在开关里的动作「打开这一刻要不要清空旧内容」。表上每个 ✓/✗ 都是当场
# 撞出来的：真跑一次 f.read() / f.write()，接住异常再落笔，一个都不凭记忆写。
#
# 为什么要有它：'w' 是「打开即截断」，不是「写的时候才截断」。这条记反就是数据事故，
# 而且事故现场一声报错都没有 —— 文件还在、名字还在、mtime 还更新了，只有内容空了。
# 第 2 段专门演「open 之后一个字都不写就关掉」这条路径，把 72 字节当场演成 0 字节。
#
# 现场全在本目录的 open-modes-demo/ 里现造现删（章目录零产物）；只打相对路径；所有故意失败
# 都被 try/except 接住并原样打印异常类型和消息 —— 那些原文就是本章要交付的证据。
# 退出码恒为 0；跑两遍输出逐字一致（无时间、无 PID、无内存绝对值）。
# ============================================================================
import os
import shutil
from pathlib import Path

WORK = Path("open-modes-demo")
MODES = ("r", "r+", "w", "w+", "a", "a+", "x")
OLD = "旧的记账内容" * 4      # 4 轮共 72 字节（UTF-8 一个汉字 3 字节），下面当场量给你看


def fresh(name, text=OLD):
    """把文件恢复成已知内容，返回它的字节数（我们亲手写进去的，所以是确定值）。"""
    path = WORK / name
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)
    return os.path.getsize(path)


def try_io(fh, action, arg):
    """真做一次 action(arg)：成功返回 ('✓', None)，撞墙返回 ('✗', '异常名: 原文')。"""
    try:
        getattr(fh, action)(arg)
    except Exception as exc:
        return "✗", f"{type(exc).__name__}: {exc}"
    return "✓", None


def show(label, path, mode):
    """只为了把一条打开失败的**原文**打到屏幕上：成功反而没得看。"""
    try:
        open(path, mode).close()
    except OSError as exc:
        print(f"  {label} → {type(exc).__name__}: {exc}")
        return type(exc).__name__
    print(f"  {label} → 竟然打开了")
    return None


def probe(mode):
    """按 mode 开一个全新文件，把「起点 / 旧内容 / 能不能读 / 能不能写」各测一遍。"""
    name = f"模式-{mode}.txt"
    path = WORK / name
    row = {"size": fresh(name), "tell": "—", "read": "—", "write": "—",
           "cls": "—", "says": "—", "open_msg": None}
    try:
        fh = open(path, mode)          # 故意不用 with：这里要看的正是 open() 那一刻
    except OSError as exc:
        row["open_msg"] = f"{type(exc).__name__}: {exc}"
        row["read"] = row["write"] = "✗"
        row["cls"] = "根本没建成对象"
        row["rest"] = os.path.getsize(path)   # 打不开之后，旧内容有没有被顺手带走？
        return row
    row["tell"] = fh.tell()
    row["cls"] = type(fh).__name__
    row["says"] = (fh.readable(), fh.writable())   # 对象的自报家门
    fh.close()
    row["rest"] = os.path.getsize(path)            # 只 open 再 close，一个字没写
    for key, action, arg in (("read", "read", 1), ("write", "write", "z")):
        fresh(name)                                # 每个方向都用新文件，游标规则不搅浑方向
        fh = open(path, mode)
        row[key], msg = try_io(fh, action, arg)
        if msg:
            row[key + "_msg"] = msg
        fh.close()
    return row


if WORK.exists():
    shutil.rmtree(WORK)      # 上次中途崩在这里留下的残骸：先清干净，保证每次都是同一个现场
WORK.mkdir()
ROWS = {m: probe(m) for m in MODES}

print("=== 1. 模式全景表：每个格子都是拿真文件撞出来的 ===")
print(f"  初始内容统一是同一份记账文本 = {ROWS['r']['size']} 字节（'{OLD[:6]}…' × 4 轮）：")
print("  模式   可读  可写  打开后 tell()  只 open 不写就 close：旧内容剩多少   自报(读,写)")
for mode in MODES:
    r = ROWS[mode]
    rest = f"{r['rest']} 字节" if r["open_msg"] is None else f"{r['rest']} 字节（不让打开）"
    print(f"  {mode:<6} {r['read']:<5} {r['write']:<5} {str(r['tell']):<14} {rest:<26} {r['says']}")
print("  撞墙时屏幕上调出来的原文（上面那些 ✗ 就是它们，一个字符没改）：")
for mode in MODES:
    for op, key in (("open", "open_msg"), ("read", "read_msg"), ("write", "write_msg")):
        if key in ROWS[mode]:
            print(f"    '{mode}' 的 {op}() → {ROWS[mode][key]}")
agree = all((ROWS[m]["read"] == "✓") == ROWS[m]["says"][0] and
            (ROWS[m]["write"] == "✓") == ROWS[m]["says"][1]
            for m in MODES if ROWS[m]["says"] != "—")
print(f"  → 真撞出来的方向和 f.readable()/f.writable() 自报的一致吗 → {agree}")
print("  → 方向和「游标起点」是两件事：'r' 和 'r+' 起点都是 0，区别只在能不能写；")
print("    文本层撞墙给 io.UnsupportedOperation（原文见上），别去猜它是不是 ValueError。")
if ROWS["x"]["open_msg"]:
    print(f"    'x' 那一行整行是 ✗：它对**已存在**的文件连打开都不让（上面那条 open 原文）；")
    print(f"    但报错的同时旧内容还剩 {ROWS['x']['rest']} 字节 —— 拒绝得干干净净，一个字节没碰。")
print("\n=== 2. ★本章最贵的一条：'w' 在 open() 那一刻就截断，不等 write ===")
path = WORK / "账本.txt"
before = fresh("账本.txt")
print(f"  ① 先写一份账本：{before} 字节，第一行 = {open(path, encoding='utf-8').readline()!r}")
fh = open(path, "w", encoding="utf-8")
print(f"  ② 只执行 open(p,'w')，一个字节都还没写：磁盘上剩 {os.path.getsize(path)} 字节（fh.closed={fh.closed}）")
fh.close()
print(f"  ③ 立刻 close（全程没 write）：{os.path.getsize(path)} 字节，读回来 = {open(path, encoding='utf-8').read()!r}")
print("  → 截断发生在 open() 返回**之前**。等你想清楚「还是别写了」，旧内容已经没了，")
print("    而且没有任何一条报错提醒你 —— 第 07 章第 16 节说的「不报错的错」就是这个形状。")
print("  每个模式各开一次、都一个字不写就关掉，看谁真的动了磁盘：")
for mode in MODES:
    r = ROWS[mode]
    flag = ("拒绝打开，旧内容原封不动" if r["open_msg"]
            else "★当场截断" if r["rest"] == 0 else "旧内容没动")
    print(f"    '{mode:<2}' {r['size']} → {r['rest']} 字节 ｜ {flag}")
print("  → 掉到 0 的只有 'w' 和 'w+'；'x' 是直接拒绝打开。这就是「宁可失败也不覆盖」的写法。")
print("  ★ 想让「打开即截断」不发生只有两条路：用 'x'，或者先 'r' 读进来、决定要写了再 'w'。")

print("\n=== 3. 'a' 的起点：把「传闻」和两层实测并排放 ===")
path = WORK / "日志.txt"
size = fresh("日志.txt", "0123456789AB")
print(f"  文件现有 {size} 字节。传闻：append 打开时游标在 0，第一次写才跳到末尾。")
fd = os.open(path, os.O_WRONLY | os.O_APPEND)
print(f"  裸 syscall 层：os.open(O_WRONLY|O_APPEND) 刚返回时 f_pos = {os.lseek(fd, 0, os.SEEK_CUR)}")
os.write(fd, b"Z")
print(f"  裸写一个字节之后 f_pos = {os.lseek(fd, 0, os.SEEK_CUR)} ｜ 传闻在**这一层**是真的")
os.close(fd)
fresh("日志.txt", "0123456789AB")
fh = open(path, "a", encoding="utf-8")
print(f"  Python 层：open(p,'a') 之后连 tell() 都没调，内核 f_pos 已经是 {os.lseek(fh.fileno(), 0, os.SEEK_CUR)}")
print(f"  Python 层：fh.tell() 实测 = {fh.tell()} ｜ readable={fh.readable()} writable={fh.writable()} ｜ {type(fh).__name__}")
fh.write("Z")
fh.flush()
print(f"  写一个字节后 tell = {fh.tell()}，磁盘 = {path.read_bytes()!r}")
fh.seek(0)
print(f"  fh.seek(0) 之后 tell = {fh.tell()}（游标真的被挪回 0 了），再 write('Q')：")
fh.write("Q")
fh.flush()
print(f"  磁盘 = {path.read_bytes()!r} ← 'Q' 还是接在最末尾")
fh.close()
print("  → 两层答案不一样，而且都是实测：内核刚打开时在 0；CPython 在 open() 里自己补了一次")
print("    seek 到末尾，所以你在 Python 看到的 tell() 是文件字节数。别把「传闻 0」抄进注释。")
print("  ★ 也别拿这个起点当「我知道文件多长」：tell 只描述打开那一刻；O_APPEND 保证的是")
print("    「我这一笔一定在最后」—— 多个进程安全追加靠这个标记，不靠谁先 tell 过。")
print("\n=== 4. 三种「打不开」的报错原文：一个字都别改地抄下来 ===")
show("'r' 对不存在的文件", WORK / "查无此文件.txt", "r")
show("'w' 对不存在的父目录", WORK / "没有的目录/子/x.txt", "w")
fresh("已存在.txt")
for mode in ("x", "xb", "x+"):
    show(f"'{mode}' 对已存在的文件", WORK / "已存在.txt", mode)
print("  → 前两条是同一个类同一条 strerror（只差路径），因为内核只知道「这一级路径不存在」；")
print("    父目录不存在时 'w'/'a'/'x'/'r' 全给 FileNotFoundError —— 建目录不归模式串管，")
print("    归 os.makedirs，本章第 10 节。这条错最容易被误读成「没权限」。")
print("  → 看 errno 别看中文语感：No such file or directory = 2，File exists = 17；")
print("    两者都是 OSError 的儿子（第 07 章第 3 节那棵树），except OSError 一网能打尽。")

print("\n=== 5. 文本还是二进制：write 收什么、read 给什么 ===")
path = WORK / "汉字.txt"
with open(path, "w", encoding="utf-8") as fh:
    n = fh.write("第一行\n")
print(f"  'w' + write('第一行\\n') → 成功，write 返回 {n}（文本模式的计数单位是**字符**）；")
print(f"      磁盘上实际是 {os.path.getsize(path)} 字节（4 个汉字 × 3 + 换行 1）。")
fh = open(path, "w", encoding="utf-8")
got, msg = try_io(fh, "write", "字符串")
fh.close()
print(f"  'w' + write(str) → {got}{'（' + msg + '）' if msg else ''}")
fresh("汉字.txt", "第一行\n")            # 'wb' 一打开就截断，所以先补回内容再演它
bad = open(WORK / "错型.bin", "wb")
got, msg = try_io(bad, "write", "中文")
bad.close()
print(f"  'wb' + write('中文') → {got} {msg}")
with open(path, "r", encoding="utf-8") as fh:
    txt = fh.read(3)
print(f"  'r' read(3) → {type(txt).__name__}：{txt!r}（3 个**字符**）")
with open(path, "rb") as fb:
    raw = fb.read(3)
print(f"  'rb' read(3) → {type(raw).__name__}：{raw!r}（3 个**字节** = 刚好一个「第」字）")
try:
    open(path, "rb").read(2).decode("utf-8")
except UnicodeDecodeError as exc:
    print(f"  'rb' read(2).decode('utf-8') → {type(exc).__name__}: {exc}")
print("  → 字节、字符、路径是三件不同的事：'b' 换的是**单位**，不是「换个编码读」的开关；")
print("    没有 'b' 才有 encoding= 可谈。截在半个字上，就是上面那条 UnicodeDecodeError。")
print("  → 'wb' 这类二进制句柄的 tell()/seek() 单位、以及能不能 seek 到任意字节，见本章第 18 节。")

print("\n=== 6. 收口：一张「模式 → 不存在时 / 存在时 / 起点 / 截断 / 读 / 写」判据表 ===")
missing = {}
for mode in MODES:
    path = WORK / f"缺失-{mode}.txt"
    path.unlink(missing_ok=True)
    try:
        fh = open(path, mode)
    except OSError as exc:
        missing[mode] = type(exc).__name__
        continue
    missing[mode] = f"新建（{os.path.getsize(path)} 字节）"
    fh.close()
print("  模式  不存在时            存在时（只 open 不写）    起点 tell  截断  读  写")
for mode in MODES:
    r = ROWS[mode]
    present = "拒绝打开" if r["open_msg"] else f"旧内容剩 {r['rest']} 字节"
    cut = "★是" if (r["open_msg"] is None and r["rest"] == 0) else "否"
    print(f"  {mode:<5}  {missing[mode]:<20}{present:<22}{str(r['tell']):<10}{cut:<5} {r['read']}  {r['write']}")
print("  → ① 'w' 是打开即截断，不是写时才截断：open() 还没返回，旧内容已经没了；")
print("    ② 会删数据的只有 w / w+，要「存在就别碰」只有 x；a / a+ 的起点本机实测是文件末尾（不是传闻的 0）；")
print("    ③ 「能不能读写」和「字节还是字符」是两个独立开关：'+' 加方向，'b' 换单位，谁也不替谁担保。")
print("  ★ 这张表的读法：先在「不存在时」那列确认它会不会替你建文件，")
print("    再在「截断」那列确认它会不会吃掉旧数据 —— 这两列决定事故，其余四列只决定你用得顺不顺。")

shutil.rmtree(WORK)
print(f"  收尾自证：{WORK.name}/ 还在吗 → {WORK.exists()}（现场全部现造现删，章目录零产物）")
