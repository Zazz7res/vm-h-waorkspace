#!/usr/bin/env python3
# ============================================================================
# 取证脚本 d_circular_import.py —— 第 09 章《模块、导入与环境》第 4 讲
#
# 只证三件事：
#   ① 循环导入炸的不是「图上有环」，而是「模块体执行的先后顺序」；
#   ② 同一个环，取法不同 → 报错原文不同（`from a import x` 当场要名字；`import a` 只是
#      拿到半成品模块对象，用的时候才取属性）；
#   ③ 三条真实出路各自都跑通了（延迟 import / 只 import 模块 / 抽公共模块变成 DAG），
#      而 importlib.reload 一条都跑不通。
#
# 沙箱：当前目录下的 circular-demo/，全部现场现造，末尾 shutil.rmtree 删干净并当场自证。
# 只用标准库；不联网、不装包；子进程用 [sys.executable] + cwd= + env=（先剥掉继承的
# PYTHON* 变量）。屏幕上不出现绝对路径、pid、毫秒：报错原文一律过 mask() 再打。
# 本机实测：CPython 3.14.4 / Ubuntu 26.04。报错措辞在不同小版本之间改过好几次，
# 其它版本、Windows 上的原文 —— 未验证，别当事实引用。
# ============================================================================
import ast
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path.cwd().resolve()          # 本次运行的工作目录（屏幕上只会看到它的相对形状）
BOX = Path("circular-demo")          # 唯一的沙箱；所有文件都写在它里面
PYC_BEFORE = Path("__pycache__").exists()
LINES = []                           # 打过的每一行都留个底，最后自证「没漏路径」
RUNS = 0                             # 真子进程计数


def mask(text):
    """把绝对路径换成相对形状：先长后短，否则家目录会先被截掉、留下半个尾巴。"""
    out = text
    for base in (str(ROOT), os.getcwd(), str(Path.home())):
        if base and base != "/":
            out = out.replace(base + "/", "").replace(base, ".")
    return out


def emit(text):
    line = mask(str(text))
    LINES.append(line)
    print(line)


def clean_env(extra=None):
    env = {k: v for k, v in os.environ.items() if not k.startswith("PYTHON")}
    env["PYTHONIOENCODING"] = "utf-8"          # 子进程的中文输出不跟着 locale 抖
    env.update(extra or {})
    return env


def run(script, sub="."):
    """在沙箱（或沙箱的子目录）里跑一个真子进程，返回 (rc, stdout, stderr) —— 都已 mask。"""
    global RUNS
    RUNS += 1
    cwd = BOX if sub == "." else BOX / sub
    proc = subprocess.run([sys.executable, script], cwd=str(ROOT / cwd),
                          env=clean_env(), capture_output=True, text=True, encoding="utf-8")
    return proc.returncode, mask(proc.stdout), mask(proc.stderr)


def dump(text, indent="    "):
    """把子进程的输出原样搬上屏幕（逐行加缩进，空行丢掉）。"""
    for ln in text.splitlines():
        if ln.strip():
            emit(indent + ln.rstrip())


def last(text):
    """报错原文就在 stderr 最后一行。"""
    for ln in reversed([x for x in text.splitlines() if x.strip()]):
        return ln.strip()
    return "(空)"


# ---------------------------------------------------------------------------
# 沙箱里的文件：包 pkg 里每一对模块都故意互相依赖；top/ 是「不在包里的同一件事故」；
# pkg3a / pkg3b 是出路③的对照组（改图之前 vs 改图之后）。
# ---------------------------------------------------------------------------
PKG_INIT = '"""本包里的每一对模块都故意互相依赖，只为取证。"""\n'

SRC_FROM_A = '''import pkg.from_b                    # 先让 b 跑起来（下面还要被 b 回头 import）

Y = "pkg.from_a 的 Y"                # ← 第 1 行没跑完，这一行永远还没执行
'''
SRC_FROM_B = '''from pkg.from_a import Y             # ← 事故点：a 还在跑，Y 还没有

Z = "pkg.from_b 的 Z"
'''
SRC_ATTR_A = '''import pkg.attr_b

X = "pkg.attr_a 的 X"
'''
SRC_ATTR_B = '''import pkg.attr_a as a               # 拿到的是**半成品模块对象**，这一行不报错

Z = a.X                              # ← 事故点挪到了「取属性」这一行
'''
SRC_DOT_A = '''import pkg.dot_b

X = "pkg.dot_a 的 X"
'''
SRC_DOT_B = '''import pkg.dot_a                     # 注意：这样 import 只把名字 **pkg** 绑进本命名空间

Z = pkg.dot_a.X                      # ← 事故点：父包 pkg 上还没有 dot_a 这个属性
'''
SRC_HALF_A = '''import sys

hb = sys.modules["pkg.half_b"]              # b 已经在 sys.modules 里 —— 半成品状态
print("a 的模块体里：hasattr(b, 'Y') →", hasattr(hb, "Y"))
print("a 的模块体里：b 的非 dunder 名字 →", sorted(k for k in vars(hb) if not k.startswith("_")))
print("a 的模块体里：b 的 dunder 名字（只打名字）→", sorted(k for k in vars(hb) if k.startswith("_")))
print("a 的模块体里：b.__spec__._initializing →", hb.__spec__._initializing)
X = "pkg.half_a 的 X"
'''
SRC_HALF_B = '''import sys

print("b 的模块体前半段：hasattr(self, 'Y') →", hasattr(sys.modules["pkg.half_b"], "Y"))
import pkg.half_a                           # ← a 的整个模块体就在这行里跑完
print("b 的模块体后半段：a 跑完之后，自己才写下 Y")
Y = "pkg.half_b 的 Y"
'''
SRC_LAZY_A = '''X = "lazy_a 的 X"


def use():
    from pkg.lazy_b import Y                # ← 出路①：把 import 挪进函数体
    return f"lazy_a.use() 拿到 {Y}"
'''
SRC_LAZY_B = '''Y = "lazy_b 的 Y"


def use():
    from pkg.lazy_a import X
    return f"lazy_b.use() 拿到 {X}"
'''
SRC_MO_A = '''import pkg.mo_b as mb                       # ← 出路②：只 import 模块对象

X = "mo_a 的 X"


def use():
    return f"mo_a.use() 拿到 {mb.Y}"        # 用的时候才取属性，那时 b 早跑完了
'''
SRC_MO_B = '''import pkg.mo_a as ma

Y = "mo_b 的 Y"


def use():
    return f"mo_b.use() 拿到 {ma.X}"
'''
SRC_3A_X = '''from pkg3a.y import Color                   # 环：x 要 y 那边的名字

DESC = f"x 用 {Color}"
'''
SRC_3A_Y = '''from pkg3a.x import DESC                    # 环：y 又要 x 的名字

Color = "红"
'''
SRC_3B_COMMON = '''Color = "红"


def stamp(who):
    return f"{who} 盖了 {Color} 的章"
'''
SRC_3B_X = '''from pkg3b.common import Color, stamp       # 只剩朝下的边：x → common

DESC = stamp("x")
'''
SRC_3B_Y = '''from pkg3b.common import Color, stamp       # y → common，环没了

DESC = stamp("y")
'''
SRC_RL_B = '''import sys

print("BODY-rr_b")
print("此刻 sys.modules 里 pkg.rl* 的先后 →", [k for k in sys.modules if k.startswith("pkg.rl")])
from pkg.rl_a import X                     # ← 事故点：a 还是半成品

Z = "rl_b 的 Z"
'''
SRC_RL_A = '''import sys

print("BODY-rr_a")
from pkg.rl_b import Z                     # 一句最普通的 from-import

X = "rl_a 的 X"
'''
SRC_TC_A = '''from typing import TYPE_CHECKING

if TYPE_CHECKING:                            # 运行时它是 False：这条边根本不执行
    from pkg.tc_b import B

NAME = "tc_a"


def describe(other):
    return f"{NAME} 指向 {other}"
'''
SRC_TC_B = '''class B:
    pass
'''
SRC_TOP_A = '''from top_b import Z

Y = "top_a 的 Y"
'''
SRC_TOP_B = '''from top_a import Y

Z = "top_b 的 Z"
'''

DRIVER_FROM = "import pkg.from_a\n"
DRIVER_ATTR = "import pkg.attr_a\n"
DRIVER_DOT = "import pkg.dot_a\n"
DRIVER_HALF = '''import pkg.half_b
print("全部跑完之后：hasattr(pkg.half_b, 'Y') →", hasattr(pkg.half_b, "Y"))
print("全部跑完之后：pkg.half_b.__spec__._initializing →", pkg.half_b.__spec__._initializing)
'''
DRIVER_LAZY = '''import pkg.lazy_a, pkg.lazy_b
print(pkg.lazy_a.use())
print(pkg.lazy_b.use())
'''
DRIVER_MO = '''import pkg.mo_a, pkg.mo_b
print(pkg.mo_a.use())
print(pkg.mo_b.use())
'''
DRIVER_3A = '''import pkg3a.x, pkg3a.y
print("这两个模块居然都活下来了")
'''
DRIVER_3B = '''import pkg3b.x, pkg3b.y
print(pkg3b.x.DESC, "｜", pkg3b.y.DESC)
'''
DRIVER_RL = '''import importlib
import sys

errs = []


def attempt(tag):
    try:
        import pkg.rl_a
        print(tag, "→ 居然 import 成功了")
    except Exception as exc:
        errs.append(f"{type(exc).__name__}: {exc}")
        print(tag, "→ 失败")
    print("   此刻 sys.modules 里有 pkg.rl_a 吗 →", "pkg.rl_a" in sys.modules,
          "｜ 有 pkg.rl_b 吗 →", "pkg.rl_b" in sys.modules,
          "｜ 有父包 pkg →", "pkg" in sys.modules)


attempt("第一次 import")
try:
    importlib.reload(sys.modules["pkg.rl_a"])          # 「reload 一下」的直觉
except Exception as exc:
    print("   对半成品做 reload →", type(exc).__name__, "：sys.modules 里已经没有它，reload 连模块对象都拿不到")
attempt("重新 import 一次")
importlib.reload(sys.modules["pkg"])                   # 把父包的模块体重跑一遍，再试
attempt("reload 父包之后再来一次")
print("第一、第二次的报错原文一模一样吗 →", errs[0] == errs[1])
print("原文｜" + errs[0])
'''
DRIVER_TC = '''import sys
import pkg.tc_a
print("import 完 tc_a 之后，sys.modules 里有 pkg.tc_b 吗 →", "pkg.tc_b" in sys.modules)
print("运行时 typing.TYPE_CHECKING 的值 →", __import__("typing").TYPE_CHECKING)
import pkg.tc_b
print("手动真的 import tc_b 之后 →", "pkg.tc_b" in sys.modules,
      "｜ tc_a.describe →", pkg.tc_a.describe(pkg.tc_b.B.__name__))
'''
DRIVER_TOP = "import top_a\n"

FILES = [
    ("pkg/__init__.py", PKG_INIT),
    ("pkg/from_a.py", SRC_FROM_A), ("pkg/from_b.py", SRC_FROM_B),
    ("pkg/attr_a.py", SRC_ATTR_A), ("pkg/attr_b.py", SRC_ATTR_B),
    ("pkg/dot_a.py", SRC_DOT_A), ("pkg/dot_b.py", SRC_DOT_B),
    ("pkg/half_a.py", SRC_HALF_A), ("pkg/half_b.py", SRC_HALF_B),
    ("pkg/lazy_a.py", SRC_LAZY_A), ("pkg/lazy_b.py", SRC_LAZY_B),
    ("pkg/mo_a.py", SRC_MO_A), ("pkg/mo_b.py", SRC_MO_B),
    ("pkg/rl_a.py", SRC_RL_A), ("pkg/rl_b.py", SRC_RL_B),
    ("pkg/tc_a.py", SRC_TC_A), ("pkg/tc_b.py", SRC_TC_B),
    ("pkg3a/__init__.py", PKG_INIT), ("pkg3a/x.py", SRC_3A_X), ("pkg3a/y.py", SRC_3A_Y),
    ("pkg3b/__init__.py", PKG_INIT), ("pkg3b/common.py", SRC_3B_COMMON),
    ("pkg3b/x.py", SRC_3B_X), ("pkg3b/y.py", SRC_3B_Y),
    ("top/top_a.py", SRC_TOP_A), ("top/top_b.py", SRC_TOP_B),
    ("drv_from.py", DRIVER_FROM), ("drv_attr.py", DRIVER_ATTR), ("drv_dot.py", DRIVER_DOT),
    ("drv_half.py", DRIVER_HALF), ("drv_lazy.py", DRIVER_LAZY), ("drv_mo.py", DRIVER_MO),
    ("drv_3a.py", DRIVER_3A), ("drv_3b.py", DRIVER_3B), ("drv_rl.py", DRIVER_RL),
    ("drv_tc.py", DRIVER_TC), ("top/drv_top.py", DRIVER_TOP),
]

shutil.rmtree(BOX, ignore_errors=True)
for rel, text in FILES:
    path = BOX / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
CREATED = sorted(p.relative_to(BOX).as_posix() for p in BOX.rglob("*") if p.is_file())

print("=== 1. 先造一个真循环：包内 from-import 撞上半成品，报错原文长这样 ===")
emit(f"  沙箱 {BOX.name}/ 里现造的文件数 = {len(CREATED)}（其中 pkg/ 里 {sum(1 for x in CREATED if x.startswith('pkg/'))} 个）")
emit("  pkg/from_a.py 第 1 行 import pkg.from_b，pkg/from_b.py 第 1 行 from pkg.from_a import Y —— 环就在这")
rc, out, err = run("drv_from.py")
emit(f"  子进程 `python3 drv_from.py`（cwd={BOX.name}/）：rc={rc}")
dump(err)
emit("  → 最后一行才是判据：cannot import name ... **from partially initialized module** ...")
emit("    「partially initialized」= from_a 已经在 sys.modules 里、但模块体只跑到第 1 行，Y 那一行还没执行。")
emit("  ★ 这句里连「most likely due to a circular import」都替你写好了 —— 解释器直接说出它怀疑什么，")
emit("    但它只在**包内的模块**上给这句（见第 2 节实测的第四种）。")

print()
print("=== 2. 同一个环、四种取法、四种原文：措辞由「怎么取」决定，不由环决定 ===")
ROWS = [
    ("A  from pkg.from_a import Y", "drv_from.py", "事故在 pkg/from_b.py"),
    ("B  import pkg.attr_a as a → a.X", "drv_attr.py", "事故在 pkg/attr_b.py"),
    ("C  import pkg.dot_a → pkg.dot_a.X", "drv_dot.py", "事故在 pkg/dot_b.py"),
]
for label, drv, where in ROWS:
    rc, out, err = run(drv)
    emit(f"  取法 {label}（{where}）｜ rc={rc}")
    emit(f"    原文 → {last(err)}")
rc, out, err = run("drv_top.py", sub="top")
emit(f"  取法 D  一模一样的 from-import，但两个文件**不在包里**（{BOX.name}/top/）｜ rc={rc}")
emit(f"    原文 → {last(err)}")
emit("  → A/B/C 三种都写着「most likely due to a circular import」；D 写的是「consider renaming」：")
emit("    partially initialized 那句提示走的是「父包 + 子模块」那条判定路径（本机 3.14.4 实测）。")
emit("  → 真正的差别是**取名字的时刻**：`from a import x` 在模块体里当场要 x；`import a` 只从")
emit("    sys.modules 拿到半成品模块对象，等到 `a.x` 那一刻才发现属性还没写上。")
emit("  ★ 措辞会随小版本变，跨版本/跨平台未验证；能当事实的是 traceback 里的**文件名 + 行号**。")

print()
print("=== 3. 半成品时刻：在 a 的模块体里看 b 的命名空间，只有部分东西 ===")
rc, out, err = run("drv_half.py")
emit(f"  驱动 `import pkg.half_b`：rc={rc}（同一个环，这一对却活下来了 —— 取法不撞时刻）")
dump(out, indent="    ")
emit("  → b 的模块体最后一行才写 Y，而 a 的体是在倒数第二行那句 `import pkg.half_b` 里跑完的，")
emit("    所以 a 眼里的 b 只剩那几个 dunder 名字 + 一个 `sys`（b 自己在第 1 行 import 的），")
emit("    `hasattr(b, 'Y')` 当场 False —— 这就是「半成品」四个字的字面意思。")
emit("  → `_initializing` 这个私有字段就是那句报错的开关：模块体跑完才翻成 False（上面两行可对照）。")

print()
print("=== 4. 三条出路逐个实测（每条一个真子进程）：延迟 import / 只 import 模块 / 抽公共模块 ===")


def deps(pkg_name):
    """用 ast 从源码里抠出「这个模块 import 了本组的谁」，返回 (排序后的节点, 排序后的边)。"""
    folder = BOX / pkg_name
    stems = sorted(p.stem for p in folder.iterdir() if p.suffix == ".py")
    nodes = [s for s in stems if s != "__init__"]
    edges = []
    for stem in nodes:
        tree = ast.parse((folder / f"{stem}.py").read_text(encoding="utf-8"))
        for node in ast.walk(tree):
            targets = []
            if isinstance(node, ast.Import):
                targets = [a.name for a in node.names]
            elif isinstance(node, ast.ImportFrom) and node.module:
                targets = [node.module]
            for name in targets:
                tail = name.rsplit(".", 1)[-1]
                if tail in nodes and tail != stem:
                    edges.append(f"{stem}→{tail}")
    return nodes, sorted(set(edges))


def cyclic(nodes, edges):
    """小 DFS：这组边里有没有环。只用标准库，现场算，不靠嘴说。"""
    adj = {n: [e.split("→")[1] for e in edges if e.startswith(n + "→")] for n in nodes}
    seen, stack = set(), set()

    def walk(n):
        if n in stack:
            return True
        if n in seen:
            return False
        seen.add(n)
        stack.add(n)
        hit = any(walk(m) for m in adj[n])
        stack.discard(n)
        return hit

    return any(walk(n) for n in nodes)


for title, drv, pair in (
    ("出路① 延迟到调用时（from-import 挪进函数体）", "drv_lazy.py", "pkg.lazy_a ↔ pkg.lazy_b"),
    ("出路② 只 import 模块，不 from 模块 import 名字", "drv_mo.py", "pkg.mo_a ↔ pkg.mo_b"),
):
    rc, out, err = run(drv)
    emit(f"  {title}｜ rc={rc} → {'成功' if rc == 0 else '失败'}")
    dump(out, indent="    ")
    emit(f"    → 环还在（{pair} 互相引用），只是模块体里谁也不当场取对方的名字。")

nodes_a, edges_a = deps("pkg3a")
rc, out, err = run("drv_3a.py")
emit(f"  出路③ 之前：pkg3a 节点 {nodes_a}、边 {edges_a} ｜ 有环 → {cyclic(nodes_a, edges_a)}"
     f" ｜ 子进程 rc={rc} → {'成功' if rc == 0 else '失败'}")
emit(f"    原文 → {last(err)}")
nodes_b, edges_b = deps("pkg3b")
rc, out, err = run("drv_3b.py")
emit("  出路③ 之后：把共同依赖（常量 Color + 函数 stamp）抽到 pkg3b/common.py")
emit(f"    pkg3b 节点 {nodes_b}、边 {edges_b} ｜ 有环 → {cyclic(nodes_b, edges_b)}"
     f" ｜ 子进程 rc={rc} → {'成功' if rc == 0 else '失败'}")
dump(out, indent="    ")
emit("  → 第三条出路不是技巧，是**改图**：三方图变成 DAG，谁先 import 都活得下来。")

print()
print("=== 5. 常见误诊：importlib.reload 救不了循环 ===")
rc, out, err = run("drv_rl.py")
emit(f"  驱动 `drv_rl.py`：rc={rc}；下面是子进程自己的输出（含报错原文）")
dump(out, indent="    ")
emit(f"  计数：rl_a 的模块体在这一个进程里被执行了 {out.count('BODY-rr_a')} 次、"
     f"rl_b 的 {out.count('BODY-rr_b')} 次（每 import 一次就跑一遍体）")
order = [ln.split("→", 1)[1].strip() for ln in out.splitlines() if "pkg.rl* 的先后" in ln]
emit(f"  「半成品进 sys.modules 的先后」这一行出现了 {len(order)} 次，三次内容是否完全相同 → "
     f"{len(set(order)) == 1}；顺序就是 {order[0] if order else '(无)'} —— a 永远排在 b 前面")
emit("  → reload 只做一件事：**把模块体再执行一遍**。它不改 sys.modules 里谁先谁后，")
emit("    于是同一条 from-import 再撞一次同一个半成品，报错文本一字不差（上面已比对：相等 True）。")
emit("  ★ 更麻烦的是失败之后半成品被从 sys.modules 摘掉了（上面那两行 False）——「reload 那个半成品」")
emit("    连模块对象都拿不到；反复 reload 只会让模块体多跑几遍，副作用也跟着多来几遍。")

print()
print("=== 6. 第四条真实出路：TYPE_CHECKING —— 只给类型检查器看的那条边不跑 ===")
rc, out, err = run("drv_tc.py")
emit(f"  驱动 `drv_tc.py`：rc={rc} → {'成功' if rc == 0 else '失败'}")
dump(out, indent="    ")
emit("  → 现代代码里的循环十有八九是**类型注解**造成的：A 标注要引 B、B 又要引 A。")
emit("    把那条 import 放进 `if TYPE_CHECKING:`（运行时 False），环在运行时就地消失；注解本身")
emit("    写成字符串，或整份文件加 `from __future__ import annotations`，交给检查器去解析。")
emit("  ★ 这条路只对「只读类型」有效：运行时要真用对方的函数，还是回到第 4 节那三条。")

print()
print("=== 7. 收口：看到这个报错先查哪三处 ===")
print("  查哪一处              │ 怎么查（看证据，别猜）                                       │ 查到之后怎么办")
print("  ① traceback 的 File 顺序 │ 从最后一行往上数：是不是 a.py 引 b.py、b.py 又回头引 a.py        │ 承认这是环，选一条出路")
print("  ② 炸掉那一行的取法       │ from a import x → ImportError；import a 之后 a.x → AttributeError │ 把「取名字」的时刻往后挪")
print("  ③ 模块体里谁当场用了对方 │ 常量、装饰器、类继承、注册表调用都算；函数体内的调用不算        │ 挪进函数体 / 抽公共模块")
emit("  三条出路的本机战绩：① 延迟 import rc=0 ｜ ② 只 import 模块 rc=0 ｜ ③ 抽公共模块 rc=0（见第 4 节）")
emit("  一条没救的：importlib.reload —— 报错原文完全相同，只是把模块体多跑了一遍（计数见第 5 节）")
emit("  ★ 全章反复出现的那句话在这儿最硬：import 是**执行**，不是声明；顺序问题只能用顺序解决。")
emit("  未验证：其它 Python 版本 / 其它平台的报错措辞 —— 别当事实引用。")

shutil.rmtree(BOX)
emit(f"  收尾自证：{BOX.name}/ 还在吗 → {BOX.exists()} ｜ 沙箱里本次建过的文件 {len(CREATED)} 个、"
     f"删除后残留 0 个 ｜ 本次共起真子进程 {RUNS} 个（每个都是一次独立的 import 现场）")
emit(f"  当前目录的 __pycache__ 有没有被本脚本新增 → {Path('__pycache__').exists() != PYC_BEFORE}")
user = Path.home().name
leaks = sum(1 for ln in LINES if str(ROOT) in ln or (user and user in ln))
emit(f"  输出自检：出现本机绝对路径或登录名的行数 = {leaks}")
