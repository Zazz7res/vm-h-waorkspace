#!/usr/bin/env python3
# ============================================================================
# 取证脚本 g_mini_checker.py —— 第 10 章第 7 篇：本机没有 mypy，就用 ast 现搓一把最小的尺
#
# 要证明什么：
#   ① 注解在语法树里就是**普通表达式节点**，标准库 `ast` 读得到 —— 所以「没有静态检查器」
#      不等于「只能等运行时崩」。一把几十行的尺就能查出「注解里用了看不见的名字」。
#   ② 尺的结论取决于你给它哪张**可见名表**：同一份源码，「连 if 分支里的 import 都算」
#      与「只算写在模块顶层的 import」报出不同的清单（第 2、3 格）。前者的运行时对得上
#      TYPE_CHECKING 那格的形状，后者才与 mypy 的规矩一致 —— 谁对？看你要跟谁对齐。
#   ③ 尺一次报**全部**，运行时只报**第一个**（第 4 格，直接对上 a/b 篇的结论）。
#   ④ 尺有三类必须写进账本的漏网：属性注解只看根名字（第 5 格）、值与类型匹不匹配它不管
#      （第 10 格负对照）、跨作用域它一律当同层。反过来运行时也有它的盲区：
#      函数体里的变量注解与类字段注解它根本不求值（第 6、7 格），而不执行的代码你永远
#      不知道它错不错（第 9 格）—— 那正是尺值钱的地方。
#   ⑤ `symtable` 看着比 ast 更「懂作用域」，但它**不把注解当使用**：三个只在注解里用过的名字
#      is_referenced() 全是 False（第 8 格）。想做这件事只能自己走 AST。
#
# 判据写法：每格用 eq() 把实测值与手写预期并排印出来，错了就印 [不成立！]；
#   每格最后一行是「→ 断言」，报本格判据成立数（没有糊弄人的恒真断言）。
# 现场：只在当前目录自建的 mini-demo/ 里写子进程要 import 的 .py，收尾 rmtree 删干净。
#   子进程 = [sys.executable, 脚本名] + cwd=沙箱 + 干净 env（只给 PATH，绝不继承 PYTHONPATH）。
# ast 与 symtable 那几格走的是**本进程内存里的源码字符串**，不落地文件；报错原文逐字照抄（只折路径）。
# 屏幕上不出现：绝对路径、用户名、对象地址、pid、耗时。退出码恒为 0；连跑两遍输出逐字一致。
# 本机 CPython 3.14.4 / Ubuntu。别的 Python 版本里注解的求值时机不同（3.10 之前是当场求值），
#   别把本篇的读数外推到旧版本；Windows 上的 traceback 换行本机未验证。
# ============================================================================
import ast
import builtins
import os
import shutil
import subprocess
import sys
import symtable
import textwrap
from pathlib import Path

WORK = Path("mini-demo")
PYNAME = os.path.basename(sys.executable)
CWD = Path.cwd().resolve()
HOME = os.path.expanduser("~")
CELL = {"n": 0, "ok": 0}
TOTAL = {"n": 0, "ok": 0}
WORK.mkdir(parents=True, exist_ok=True)      # 子进程要 import 的样本 .py 全写在这里


def mask(text):
    """报错原文里可能带绝对路径，统一折掉再上屏。"""
    out = str(text)
    out = out.replace(sys.executable, PYNAME)
    for root, tag in ((str(CWD) + os.sep, ""), (str(CWD), "<cwd>"),
                      (HOME + os.sep, "<家>" + os.sep), (HOME, "<家>"),
                      (sys.base_prefix + os.sep, "<pyroot>" + os.sep),
                      (sys.prefix + os.sep, "<pyroot>" + os.sep)):
        if root and root != os.sep:
            out = out.replace(root, tag)
    return out


def eq(tag, th, want):
    """判据行：实测（炸了就取报错原文）与预期逐字比对，两边都印在屏上。"""
    try:
        got = th()
    except Exception as exc:
        got = f"{type(exc).__name__}: {mask(exc)}"
    ok = got == want
    CELL["n"] += 1
    CELL["ok"] += 1 if ok else 0
    TOTAL["n"] += 1
    TOTAL["ok"] += 1 if ok else 0
    return f"  [{'成立' if ok else '不成立！'}] {tag}：实测 {got!r} ｜ 预期 {want!r}"


def ledger(title):
    return (f"  → 断言：{title} ｜ 本格判据 {CELL['ok']}/{CELL['n']} 条成立"
            f"（不成立的那条上面会印 [不成立！]）")


def reset():
    CELL["n"] = CELL["ok"] = 0


def run_child(name, src, args=()):
    """把样本源码写成沙箱里的 .py，再开个子进程跑它 —— 尺读字符串就够，运行时必须要真文件。"""
    path = WORK / name
    path.write_text(textwrap.dedent(src), encoding="utf-8")
    proc = subprocess.run([sys.executable, path.name, *args],
                          capture_output=True, text=True, cwd=str(WORK),
                          env={"PATH": os.environ.get("PATH", ""), "HOME": str(WORK)})
    return proc.returncode, proc.stdout.strip(), proc.stderr.strip()


# ---------------------------------------------------------------------------
# 本篇唯一的「被检查样本」：一份源码里同时埋着 4 种形状
SRC1 = '''
    import json
    from typing import TYPE_CHECKING

    if TYPE_CHECKING:
        import argparse


    class Widget:
        pass


    def f(a: Widget, b: Missing, c: argparse.Namespace, d: Another) -> json.Cfg:
        x: int = 1
        return x
    '''


# ---- 这把尺本体（三个函数，本篇后面每格都在用它）----------------------------
def annotations_of(tree, body_too=False):
    """哪些 AST 子树是「注解」。签名与类体/模块层的字段注解一律算；函数体里的变量注解看 body_too。"""
    owner = {child: node for node in ast.walk(tree) for child in ast.iter_child_nodes(node)}
    pairs = []
    for node in ast.walk(tree):
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
            a = node.args
            for arg in list(a.posonlyargs) + list(a.args) + list(a.kwonlyargs):
                if arg.annotation is not None:
                    pairs.append((f"{node.name} 参数 {arg.arg}", arg.annotation))
            for extra, label in ((a.vararg, "*args"), (a.kwarg, "**kwargs")):
                if extra is not None and extra.annotation is not None:
                    pairs.append((f"{node.name} 参数 {label}", extra.annotation))
            if node.returns is not None:
                pairs.append((f"{node.name} 返回值", node.returns))
        elif isinstance(node, ast.AnnAssign):
            in_func = isinstance(owner.get(node), (ast.FunctionDef, ast.AsyncFunctionDef))
            if body_too or not in_func:
                tgt = node.target
                where = f"变量注解 {tgt.id}" if isinstance(tgt, ast.Name) else "属性注解"
                pairs.append((where, node.annotation))
    return pairs


def used_names(node):
    """一个注解子树里出现的所有名字。Attribute 只取**最根上**那个 Name：json.Cfg → json。"""
    return sorted({n.id for n in ast.walk(node) if isinstance(n, ast.Name)})


def visible_names(tree, deep):
    """尺的可见名表：builtins + 源码里「弄出名字」的语句。deep=True 时连嵌在 if/函数里的 import 都算。"""
    out = set(dir(builtins))
    nodes = ast.walk(tree) if deep else list(tree.body)
    for node in nodes:
        if isinstance(node, (ast.Import, ast.ImportFrom)):
            for al in node.names:
                out.add(al.asname or al.name.split(".")[0])
        elif isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef)):
            out.add(node.name)
        elif isinstance(node, ast.Assign):
            for tgt in node.targets:
                if isinstance(tgt, ast.Name):
                    out.add(tgt.id)
    return out


def scan(src, *, deep=True, body_too=False):
    """返回值是「哪一处的哪个名字看不见」的清单 —— 一次给全，不是给第一个。"""
    tree = ast.parse(textwrap.dedent(src))
    vis = visible_names(tree, deep)
    bad = []
    for where, node in annotations_of(tree, body_too):
        for nm in used_names(node):
            if nm not in vis:
                bad.append(f"{where}:{nm}")
    return bad


# ===========================================================================
print("=== 1. 先看清原料：注解在语法树里就是普通表达式节点 ===")
reset()
tree1 = ast.parse(textwrap.dedent(SRC1))
sig_pairs = annotations_of(tree1)
print("  取证对象：上面那份 SRC1（模块里就一个函数 f，签名里埋了四种形状）。")
print("  │ ① 尺从 f 的签名里读到的注解节点（位置 | AST 节点类 | 里面出现的名字）：")
for where, node in sig_pairs:
    print(f"  │    {where} ｜ {type(node).__name__} ｜ {used_names(node)}")
print("  │ ② 函数体里那条 `x: int = 1` 是 AnnAssign 节点，默认口径**不**算它：")
print(f"  │    只算签名 → {len(sig_pairs)} 条；连函数体里的变量注解一起算 → "
      f"{len(annotations_of(tree1, body_too=True))} 条")
print("  → 关键事实：这里没有一个字节需要**执行**这份代码。ast.parse 只是把源码读成树，")
print("    所以哪怕这个函数你永远不调用，注解长什么样、里面用了哪些名字，工具照样看得见。")
print(eq("① 签名里的注解位置（按 ast.walk 的先后）", lambda: [w for w, _ in sig_pairs],
         ["f 参数 a", "f 参数 b", "f 参数 c", "f 参数 d", "f 返回值"]))
print(eq("① 对应的 AST 节点类", lambda: [type(n).__name__ for _, n in sig_pairs],
         ["Name", "Name", "Attribute", "Name", "Attribute"]))
print(eq("① 每个注解里的名字", lambda: [used_names(n) for _, n in sig_pairs],
         [["Widget"], ["Missing"], ["argparse"], ["Another"], ["json"]]))
print(eq("② 两种口径的条数", lambda: (len(sig_pairs), len(annotations_of(tree1, body_too=True))),
         (5, 6)))
print(ledger("签名 5 个注解的位置/节点类/名字三张清单逐字，两种口径分别 5 与 6 条"))

print("\n=== 2. 第一把尺：连 if 分支里的 import 都算可见 → 它报 2 条 ===")
reset()
bad_deep = scan(SRC1, deep=True)
print("  取证对象：`scan(SRC1, deep=True)`。可见名表 = builtins + 源码里**任何层**的 import/def/class。")
print(f"  │ ① 尺报的清单 → {bad_deep}")
print(f"  │ ② 它认为可见的（只看这份源码自己贡献的那部分）→ "
      f"{sorted({'json', 'TYPE_CHECKING', 'argparse', 'Widget', 'f'} )}")
print("  → 两条都是真错：Missing 与 Another 在这份源码里没有任何地方弄出过这两个名字。")
print("    argparse 为什么没被报？因为 deep 口径把 `if TYPE_CHECKING:` 分支里那句 import 也算了数 ——")
print("    它不看那个 if 会不会真的执行。这条口径与 mypy 的规矩一致（类型检查器眼里 TYPE_CHECKING 恒真），")
print("    但和第 3 篇屏上的运行时事实相反：那句 import 根本没执行。下一格把这条缝量出来。")
print(eq("① deep 口径报什么", lambda: bad_deep, ["f 参数 b:Missing", "f 参数 d:Another"]))
print(eq("② 尺没有报 c（argparse）", lambda: any("argparse" in b for b in bad_deep), False))
print(ledger("deep 口径逐字 = ['f 参数 b:Missing','f 参数 d:Another']，argparse 不在清单里"))

print("\n=== 3. 把口径收紧：只认写在模块顶层的 import → 同一份源码多报一条 ===")
reset()
bad_top = scan(SRC1, deep=False)
print("  取证对象：同一份 SRC1，`deep=False`（只扫 tree.body 那一层的 import/def/class）。")
print(f"  │ ① 收紧后的清单 → {bad_top}")
print(f"  │ ② 两口径的差集 → {sorted(set(bad_top) - set(bad_deep))}")
print("  → 多出来的正是 c 那条。此刻三把尺对着同一行源码给三种答案：")
print("      mypy 型（deep）：不报 —— 它认 TYPE_CHECKING 分支；")
print("      运行时型（tight）：报 —— 第 3 篇屏上那句 `NameError: name 'argparse' is not defined` 就是它；")
print("      第 3 篇里 `import argparse` 在 TYPE_CHECKING 下没执行、sys.modules 里没有它，正是这一条的实证。")
print("    所以「注解里这个名字到底可见吗」不是一个问题，是**两个问题**：对谁可见。")
print("    这把几十行的尺第一次显出它的难处：它必须替自己选一个「谁在看」，选错方向就漏报或误报。")
print(eq("① tight 口径报什么", lambda: bad_top,
         ["f 参数 b:Missing", "f 参数 c:argparse", "f 参数 d:Another"]))
print(eq("② 差集", lambda: sorted(set(bad_top) - set(bad_deep)), ["f 参数 c:argparse"]))
print(ledger("tight 口径逐字 3 条、两口径差集恰为 c:argparse 一条"))

print("\n=== 4. 同一份源码交给运行时：它只报第一个 ===")
reset()
run_child("src1.py", SRC1)
rc4, out4, err4 = run_child("g_reads.py", """
    import src1
    try:
        print("读到了 =", src1.f.__annotations__)
    except Exception as exc:
        print("读注解炸：", type(exc).__name__, "|", exc)
    """)
last4 = (out4.splitlines() or [""])[-1]
print("  取证对象：把 SRC1 落成 src1.py，子进程里读 `f.__annotations__`（VALUE 读法）。")
print(f"  │ ① 子进程 rc={rc4}，屏上那行 → {mask(last4)!r}")
print(f"  │ ② 尺（deep 口径）报了几条 → {len(bad_deep)} 条；运行时这条路上报了几条 → 1 条")
print("  → 这就是本章最实在的一条对照：**静态尺一次给全部，运行时一次只给第一个**。")
print("    Missing 与 Another 都是真错，但读注解的过程在第一个名字上就抛了异常，")
print("    另一个名字**根本没轮到**。你想「跑一下就看完所有注解错误」是办不到的 ——")
print("    这条差异就是「为什么值得自己搓一把尺」的全部理由。")
print(eq("① rc 与原文", lambda: (rc4, mask(last4)),
         (0, "读注解炸： NameError | name 'Missing' is not defined")))
print(eq("② 条数对照", lambda: (len(bad_deep), len(out4.splitlines())), (2, 1)))
print(ledger("子进程 rc=0（异常自己接住了）且原文只有 Missing；尺 2 条 vs 运行时 1 条"))

print("\n=== 5. 尺的第一类漏网：属性注解只看根上的名字 ===")
reset()
SRC_ATTR = '''
    import json


    def h(v: int) -> json.Cfg:
        return v
    '''
print(f"  取证对象：`def h(v: int) -> json.Cfg`（json 是真有的模块，Cfg 这个属性它没有）。")
print(f"  │ ① 尺报的清单 → {scan(SRC_ATTR)!r}   ← 空的")
run_child("src_attr.py", SRC_ATTR)
rc5, out5, err5 = run_child("g_attr.py", """
    import src_attr
    try:
        print("读到了 =", src_attr.h.__annotations__)
    except Exception as exc:
        print("读注解炸：", type(exc).__name__, "|", exc)
    """)
last5 = (out5.splitlines() or [""])[-1]
print(f"  │ ② 同一段交给运行时 → rc={rc5}，那行 {mask(last5)!r}")
print("  → `json` 在可见名表里，于是尺放行了；但 `json.Cfg` **取属性**这一步只有真读注解才会发生，")
print("    一发生就是 AttributeError。尺只看名字，不看名字身上有没有那个属性 —— 这是它的结构性盲区。")
print("    要补这一格，就得真去 import 那个模块再 getattr 一遍：那已经不再是「纯静态」，")
print("    而是 mypy 那种「跟着 import 图把语义也解一遍」的活，几十行的尺做不动，本章也不假装做得到。")
print(eq("① 尺一条都不报", lambda: scan(SRC_ATTR), []))
print(eq("② 运行时原文", lambda: mask(last5),
         "读注解炸： AttributeError | module 'json' has no attribute 'Cfg'"))
print(ledger("尺报 []、运行时报 AttributeError 原文 —— 同一行源码一个放行一个崩"))

print("\n=== 6. 尺的第二类口径问题：函数体里的变量注解，运行时根本不求值 ===")
reset()
SRC_LOCAL = '''
    def g(a: int) -> int:
        loc: NeverImported = 1
        return loc
    '''
sig_only = scan(SRC_LOCAL, body_too=False)
all_annot = scan(SRC_LOCAL, body_too=True)
run_child("src_local.py", SRC_LOCAL)
rc6, out6, err6 = run_child("g_local.py", """
    import src_local
    print("g(2) =", src_local.g(2))
    print("签名注解 =", src_local.g.__annotations__)
    """)
print("  取证对象：`def g(a: int) -> int:` 体内一行 `loc: NeverImported = 1`。")
print(f"  │ ① 只算签名 → {sig_only!r}；连变量注解一起算 → {all_annot!r}")
print(f"  │ ② 子进程真跑：rc={rc6}")
for line in out6.splitlines():
    print(f"  │    {mask(line)}")
if err6:
    print(f"  │    stderr：{mask(err6)[:120]}")
print("  → 第 1 篇量过的那条在这里对上号了：**函数体里的局部变量注解，运行时一次都不求值**。")
print("    所以 NeverImported 这个名字写错，函数照样调用、照样返回 1；`__annotations__` 里也只有 a 和 return。")
print("    尺该不该管这条？两种口径都对，取决于你要对齐谁：对齐运行时就该闭嘴（它永远不会因此崩），")
print("    对齐「代码写错了就该有人说」就该报 —— 真做 linter 的人会把它做成一条独立的规则，")
print("    而不是像这样靠一个 bool 参数。第 1 格里那两个数字（5 与 6）就是这条规则的雏形。")
print(eq("① 两口径清单", lambda: (sig_only, all_annot), ([], ["变量注解 loc:NeverImported"])))
print(eq("② 子进程 rc 与两行读数", lambda: (rc6, out6.splitlines()),
         (0, ["g(2) = 1", "签名注解 = {'a': <class 'int'>, 'return': <class 'int'>}"])))
print(ledger("尺两口径差一条；运行时 rc=0、g(2)=1、签名注解里没有 loc 这件事的原文两行都在屏上"))

print("\n=== 7. 类字段注解：尺报，dataclass 建类与实例化都不报（接第 3 篇）===")
reset()
SRC_CLASS = '''
    from dataclasses import dataclass


    @dataclass
    class Row:
        name: str
        count: MissingField
    '''
print("  取证对象：@dataclass 的字段 `count: MissingField`（这个名字源码里没有）。")
print(f"  │ ① 尺报的清单 → {scan(SRC_CLASS)!r}")
rc7, out7, err7 = run_child("src_class.py", SRC_CLASS)
rc7b, out7b, err7b = run_child("g_class.py", """
    import dataclasses
    import src_class
    row = src_class.Row(name="a", count=7)
    print("建类与实例化都没报错 =", row.name, row.count)
    print("字段名 =", [f.name for f in dataclasses.fields(row)])
    """)
print(f"  │ ② 子进程真跑：rc={rc7b}")
for line in out7b.splitlines():
    print(f"  │    {mask(line)}")
if err7b:
    print(f"  │    stderr：{mask(err7b)[:120]}")
print("  → 第 3 篇钉过的账在这里再收一次：dataclass 建类读注解走的是 FORWARDREF，")
print("    求不出的名字被它自己接住，字段照样留下、照样能实例化。运行时一句话都不说。")
print("    这一格是**尺赢**的方向：它是本篇里最典型的一种「代码里真写错了、运行时永远沉默、")
print("    而静态一读就中」的形状 —— 也是最值得你在自己项目里搓一把的那种。")
print(eq("① 尺报的清单", lambda: scan(SRC_CLASS), ["变量注解 count:MissingField"]))
print(eq("② 子进程 rc", lambda: rc7b, 0))
print(eq("② 报错文本是空的", lambda: err7b, ""))
print(eq("② 两行读数", lambda: out7b.splitlines(),
         ["建类与实例化都没报错 = a 7", "字段名 = ['name', 'count']"]))
print(ledger("尺 1 条 vs 运行时 rc=0 且 stderr 空；字段名清单证明那个坏注解字段没被吞掉"))

print("\n=== 8. 看着更懂作用域的 symtable，恰恰不把注解当使用 ===")
reset()
st = symtable.symtable(textwrap.dedent(SRC1), "<样本>", "exec")
top = {s.get_name(): (s.is_imported(), s.is_referenced()) for s in st.get_symbols()}
fsym = [c for c in st.get_children() if c.get_name() == "f"][0]
locals_f = sorted(s.get_name() for s in fsym.get_symbols())
annot_x = sorted(s.get_name() for s in fsym.get_symbols() if s.is_annotated())
print("  取证对象：同一份 SRC1 交给标准库 symtable（它是编译器的作用域视图，比 ast 更「懂」层次）。")
print(f"  │ ① 顶层三个「只在注解里被用过」的名字，(is_imported, is_referenced) → "
      f"json={top['json']}, argparse={top['argparse']}, TYPE_CHECKING={top['TYPE_CHECKING']}")
print(f"  │ ② f 那张子表里的符号名 → {locals_f}")
print(f"  │ ③ 其中标了「被注解过」的 → {annot_x}")
print("  → 三处一起看：`json` 与 `argparse` 在 symtable 眼里 imported 但 **referenced=False** ——")
print("    注解里的使用它一个都不记；f 的子表里没有 Missing、也没有 Another，")
print("    那两个名字连「未定义」都算不上，就是**不存在**；is_annotated() 只认得到体内那条 x。")
print("    结论很实用：想做注解相关的检查，别走 symtable 这条路，自己 ast.walk 是唯一稳的。")
print(eq("① json / argparse 的 (imported, referenced)", lambda: (top["json"], top["argparse"]),
         ((True, False), (True, False))))
print(eq("② Widget 也一样：只在注解里用 → referenced False", lambda: top["Widget"], (False, False)))
print(eq("③ f 的子表里没有那两个坏名字", lambda: [n for n in locals_f if n in ("Missing", "Another")], []))
print(eq("④ f 的子表符号名与体内标注", lambda: (locals_f, annot_x),
         (["a", "b", "c", "d", "x"], ["x"])))
print(ledger("json/argparse 全 (True,False)、Widget (False,False)、f 子表 5 个符号里无 Missing/Another、annotated 只有 x"))

print("\n=== 9. 尺最值钱的形状：一行都不执行，错照样抓到 ===")
reset()
SRC_UNCALLED = '''
    def never_called(x: WeDontCallIt) -> int:
        return 0
    '''
print("  取证对象：一个**永远不会被调用**的函数，签名里写错一个名字。")
print(f"  │ ① 尺报的清单 → {scan(SRC_UNCALLED)!r}")
rc9, out9, err9 = run_child("src_uncalled.py", SRC_UNCALLED)
print(f"  │ ② 直接把这个文件当脚本跑：rc={rc9} stdout={out9!r} stderr={mask(err9)!r}")
print("  → 定义函数不会读注解（第 1 篇的第一格），不调用更不会 —— 运行时**全程静默**，")
print("    rc=0、一个字都不输出。这不是「还没触发」：这段代码活着的那一天它都不会触发。")
print("    尺在这里给出一条能改的错误，而它的成本是 ast.parse 一次。")
print("    把本篇几格摆一排：第 4、5 格运行时更准，第 7、9 格尺更准，第 6 格看你要哪种口径 ——")
print("    「静态 vs 运行时」不是一场谁赢谁输，是两张各有盲区的地图。")
print(eq("① 尺报的清单", lambda: scan(SRC_UNCALLED), ["never_called 参数 x:WeDontCallIt"]))
print(eq("② 运行时的沉默", lambda: (rc9, out9, err9), (0, "", "")))
print(ledger("尺 1 条、运行时 rc=0 且 stdout/stderr 全空 —— 两条读数都在屏上"))

print("\n=== 10. 收口：这把尺能查什么、不能查什么（含一条负对照）===")
reset()
SRC_VALUE = '''
    def add(a: int, b: int) -> int:
        return a + b


    total = add("两个", "字符串")
    '''
print("  取证对象：`add(a: int, b: int) -> int` 被真真切切地用字符串调用了（第 4 篇那一族）。")
print(f"  │ ① 尺对这份源码报的清单 → {scan(SRC_VALUE)!r}   ← 空的")
rc10, out10, err10 = run_child("g_value.py", """
    import src_value
    print("跑不出错？不会的")
    """.replace("import src_value", "import src_value"))
run_child("src_value.py", SRC_VALUE)
rc10, out10, err10 = run_child("g_value.py", """
    import src_value
    try:
        print("total =", src_value.total)
    except Exception as exc:
        print("导入时就炸：", type(exc).__name__, "|", str(exc)[:60])
    """)
print(f"  │ ② 那份源码 import 之后立刻（模块顶层就调用了）→ rc={rc10}")
for line in out10.splitlines():
    print(f"  │    {mask(line)}")
print("  → 尺报 0 条：`int`、`str`、`add` 全都在可见名表里，**没有任何名字是错的**。")
print("    它只查「这个名字从这里能不能看见」，不查「这个值和这个类型匹不匹配」——")
print("    后者要么交给运行时（第 6 篇 typeguard 那种插桩），要么就得真去实现一套类型规则（几十行做不到）。")
print("  → 三条账，本篇每格都有屏上读数兜着：")
print(f"     能查：注解里用了源码里没弄出来的名字（第 2/3/7/9 格），一次给全部（第 4 格）。")
print(f"     不能查：属性存在性（第 5 格）、值与类型是否匹配（上面这条）、跨作用域的可见性细节。")
print(f"     口径要先选：deep 对齐 mypy、tight 对齐运行时（第 3 格），函数体内变量注解要不要管（第 6 格）。")
print(eq("① 尺对值错误完全无感", lambda: scan(SRC_VALUE), []))
print(eq("② 运行时那一行读数", lambda: out10.splitlines(), ["total = 两个字符串"]))
print(eq("③ 本篇五个有错样本一共抓到这五个坏名字", lambda: sorted({
        b.split(":")[1] for b in bad_deep + all_annot + scan(SRC_CLASS) + scan(SRC_UNCALLED)}),
        ["Another", "Missing", "MissingField", "NeverImported", "WeDontCallIt"]))
print(ledger("尺对 add 那份报 []、运行时那声 TypeError 在输出里一个字都没出现；"
             "五个有错的样本合起来恰好抓到 Missing/Another/NeverImported/MissingField/WeDontCallIt 五个名字"))

shutil.rmtree(WORK, ignore_errors=True)
print(f"\n  收尾自查：沙箱 {WORK}/ 删干净了吗 → {not WORK.exists()!r}")
print(f"  全篇判据合计 {TOTAL['ok']}/{TOTAL['n']} 条成立（少一条上面就有 [不成立！]）")
sys.exit(0)
