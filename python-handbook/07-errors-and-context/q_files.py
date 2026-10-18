#!/usr/bin/env python3
# ============================================================================
# 取证脚本 q_files.py —— 第 18 节：with 落到真实文件上（全部在当前目录的子目录里）
#
# 本脚本只在 cwd 下建一个 files-demo/ 子目录，跑完自己删干净（章目录零产物）。
# 所有路径都用相对名打出来，所以输出里不会出现任何本机绝对路径。
# ============================================================================
import errno
import json
import os
import shutil
import tempfile
from contextlib import ExitStack
from pathlib import Path

WORK = Path("files-demo")
if WORK.exists():
    shutil.rmtree(WORK)
WORK.mkdir()

print("=== 1. with 只保证「关掉」，不保证「写全了」——半截文件是真实现场 ===")
target = WORK / "cfg.json"
try:
    with target.open("w", encoding="utf-8") as f:
        f.write('{"a": 1,')          # 写了一半
        raise RuntimeError("写到一半崩了")
except RuntimeError as exc:
    print("  崩在外面：", exc)
print("  磁盘上留下了什么：", repr(target.read_text(encoding="utf-8")))
print("  用 json 读它会怎样：")
try:
    json.loads(target.read_text(encoding="utf-8"))
except json.JSONDecodeError as exc:
    print(f"    → {type(exc).__name__}: {exc}")
    print(f"    结构化字段：msg={exc.msg!r} lineno={exc.lineno} colno={exc.colno} pos={exc.pos}")
print("  → with 帮你把文件**关好**（数据交给了操作系统），但它不是事务：")
print("    没有「要么全写要么不写」。所以第 3 段那种写法才是配置文件的正确姿势。")

print("\n=== 2. 关掉之后能不能立刻读到？（close 的时机就是 with 退出那一刻）===")
p = WORK / "seq.txt"
with p.open("w", encoding="utf-8") as f:
    f.write("第一行\n")
    print("  块体内用另一个句柄读 =", repr(p.read_text(encoding="utf-8")), "← 还没 close，读到的是旧的/空的")
print("  块体外再读 =", repr(p.read_text(encoding="utf-8")), "← with 一退出就 close 了")
print("  判据：写模式打开时，**同一份内容对别的句柄不一定可见**；close 之后才保证可见。")
print("    再往下一层（数据是否真落盘）要靠 flush + os.fsync；本机无法验证掉电行为，**不做结论**。")

print("\n=== 3. 原子替换：先写临时文件，再 os.replace（崩溃也留不下半截配置）===")
stable = WORK / "real.json"
stable.write_text('{"a": 1}', encoding="utf-8")
print("  改动前的内容 =", stable.read_text(encoding="utf-8"))


def write_atomic(path, payload):
    tmp = path.with_name(path.name + ".tmp")
    try:
        with tmp.open("w", encoding="utf-8") as f:
            json.dump(payload, f)
            f.flush()               # Python 的缓冲区 → 操作系统
        os.replace(tmp, path)       # 同一目录内：rename 是原子的
    except BaseException:
        tmp.unlink(missing_ok=True)  # 临时文件绝不留在现场
        raise


write_atomic(stable, {"a": 2, "b": [1, 2]})
print("  正常改完之后 =", stable.read_text(encoding="utf-8"),
      "｜ 目录里剩几个文件 =", sorted(os.listdir(WORK)))

print("  现在让它在写到一半时崩（崩在 replace 之前）：")
try:
    tmp = stable.with_name(stable.name + ".tmp")
    with tmp.open("w", encoding="utf-8") as f:
        f.write('{"a": 3,')
        raise RuntimeError("又是写到一半崩了")
except RuntimeError as exc:
    print("   ", exc, "→ 走 except 分支删掉 tmp")
    tmp.unlink(missing_ok=True)
print("  真文件的完整内容 =", stable.read_text(encoding="utf-8"), "（没被半截数据污染）")
print("  → 这就是「with + 临时文件 + replace」这套写法的全部理由：")
print("    任何一步崩了，要么旧文件原样在，要么新文件原样在，**没有第三种**。")

print("\n=== 4. 临时文件的自动删除：NamedTemporaryFile 的 delete 参数是干什么的 ===")
with tempfile.NamedTemporaryFile("w", suffix=".tmp", dir=WORK, delete=True) as tf:
    tf.write("内容")
    tf.flush()
    inside = os.path.exists(tf.name)
outside = os.path.exists(tf.name)
print("  delete=True：块体内存在 =", inside, "｜ 块体外存在 =", outside, "→ 出块即删，不用你管")
with tempfile.NamedTemporaryFile("w", suffix=".tmp", dir=WORK, delete=False) as tf:
    name = tf.name
print("  delete=False：块体外存在 =", os.path.exists(name), "→ 得自己删，忘了就是垃圾文件")
os.remove(name)
print("  想要「登记了就一定删、哪怕中间崩」，就交给 ExitStack 的 callback（第 14 节）：")
survivor = WORK / "temp.json"
DEMO_LOG = []
try:
    with ExitStack() as stack:
        survivor.write_text("临时东西", encoding="utf-8")
        stack.callback(os.remove, survivor)
        DEMO_LOG.append("  登记好了，中间崩一次")
        raise RuntimeError("中途崩")
except RuntimeError as exc:
    DEMO_LOG.append(f"  外面接住：{exc}")
print("     ", DEMO_LOG, "｜ 崩了之后文件还在吗 →", survivor.exists())
print("  → callback 登记在 with 退出时**必然**执行，不管走的是正常路径还是异常路径 ——")
print("    这一条就是它比「在函数末尾写 os.remove」强的地方。")

print("\n=== 5. 读文件的 EAFP：用 errno 分辨「没有」和「没权限」（第 8 节那条判据的落地）===")
for path in (WORK / "没有这个.json", Path("/etc/hosts")):
    try:
        with path.open(encoding="utf-8") as f:
            head = f.read(12)
    except OSError as exc:
        kind = {errno.ENOENT: "真的不存在（可以当成首次运行）",
                errno.EACCES: "有但不给读（这是配置错误，必须报出来）"}.get(exc.errno, "别的")
        print(f"  open({str(path)!r:<22}) → {type(exc).__name__} errno={exc.errno} ：{kind}")
    else:
        print(f"  open({str(path)!r:<22}) → 读到了 {head!r}")
print("  → LBYL 写法（if os.path.exists: open）在这里有两个问题：")
print("    ① 判了存在还是不保证能读（权限、路径是目录、文件被删）；② 检查和打开之间可能被人动过。")
print("    所以：**直接 open，接 OSError，按 errno 分派**。这就是 EAFP 在 I/O 上的标准形状。")

print("\n=== 6. 文件对象是**一次性**的迭代器（第 04 章那条规律在 I/O 上的样子）===")
lines_file = WORK / "lines.txt"
lines_file.write_text("一\n二\n三\n", encoding="utf-8")
with lines_file.open(encoding="utf-8") as f:
    first = list(f)
    second = list(f)
print("  同一个句柄：第一次 list(f) =", first)
print("            第二次 list(f) =", second, "← 游标已经在末尾了，**不报错，只是空**")
with lines_file.open(encoding="utf-8") as f:
    print("  想读两遍要么 seek(0)：", (f.seek(0), list(f))[1])
print("  → 这就是第 04 章「迭代器只能过一遍」的 I/O 版本；配置读两遍的人都会在这里翻车，")
print("    而且翻车方式是**静默的空列表**，不是报错。")

print("\n=== 7. 收口 ===")
print("  ① with open 管的是 close，不管「写全」「落盘」「原子」——那三件事要你自己安排（第 1、3 段）。")
print("  ② 配置/状态文件一律「同目录临时文件 + os.replace」；崩了留旧不留半截。")
print("  ③ I/O 用 EAFP：直接 open，接 OSError，**按 errno 分派**，不要 if exists 再 open。")
print("  ④ 临时文件的删除要么 delete=False + 自己登记 callback，要么信任 delete=True（第 4 段实测）。")
print("  ⑤ 文件对象是迭代器，过一遍就空；要重读就 seek(0) 或者一次 read 进来存着（第 6 段）。")

shutil.rmtree(WORK)
print("\n  清理：files-demo/ 已删除，现在这个目录里剩 =", sorted(os.listdir(".")))
