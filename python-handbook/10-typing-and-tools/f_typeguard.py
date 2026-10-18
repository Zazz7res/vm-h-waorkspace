#!/usr/bin/env python3
# ============================================================================
# 取证脚本 f_typeguard.py —— 第 10 章第 6 篇：本机唯一在场的第三方类型库 typeguard
#
# 要证明什么：
#   ① 这台机器上 mypy / pyright / ruff / black / flake8 / pylint / pytest **一个都没有**，
#      连 `python3 -m pip` 都是「No module named pip」—— 所以「装个静态检查器」这条路在本机走不通。
#      唯一在场的第三方库是系统 apt 带进来的 typeguard 4.4.4（它是**运行时**检查，不是静态检查）。
#   ② 它的默认策略是「容器只查第一个元素」：`list[int]` 收进 [1, "x"] 一声不响就放过去了，
#      把错的挪到第 0 位才拦。这条不是 bug，是 `collection_check_strategy=FIRST_ITEM` 的默认值 ——
#      但它长得太像 bug，所以必须用屏上读数把它钉清楚（第 3、4 格）。
#   ③ 它比标准库多管一件事：函数**体内的局部变量注解**它管（因为它是插桩执行的），
#      而实例属性赋值它不管。对照 a 篇「局部注解运行时根本不求值」，
#      就能看清「typeguard 加了什么」和「标准库什么都没加」。
#   ④ 它靠**读源码**工作：同一份代码 `python -c` 里 rc=1（TypeError），喂标准输入 rc=0 但只发一句
#      InstrumentationWarning（装饰器压根没装上），写成 .py 才真检查（第 7 格）。
#   ⑤ 注解名字求不出值时**分两条路**：被 `@typechecked` 装饰的函数当场 NameError 崩掉；
#      直调 check_type 才按 forward_ref_policy 给三张脸（WARN 放行+1 条 warning / ERROR 抛 / IGNORE 静默放行），
#      而且这套策略对**字符串注解**整体失效 —— 三种取值逐字同读数（第 8 格）。
#   ⑥ import hook 只管「装上之后才加载」的模块：先 import 再装 hook 等于没装；装完退出后已插桩的模块
#      继续插桩（第 9 格四条形分不同的读数）。
#
# 为什么要有它：本章的主线是「本机没有静态检查器时怎么活」。这一篇回答的是
# 「那台机器上到底有一个什么、它替你挡了什么、漏了什么」。别把 typeguard 当 mypy 用：
# 它是运行时插桩，代价花在每次调用上，而且默认只查容器第一个元素。
#
# 关于时间：本篇**故意不测性能**。跑两遍逐字一致是本手册的硬要求，而「快了多少倍」每跑必变；
# 想看开销请自己在终端里 `python -m timeit`，本章不给结论（第 10 格把这条说明印在屏上）。
#
# 现场全在本目录的 typeguard-demo/ 里现造现删（子进程用的 .py 也写在这里）；
# 屏幕上只打相对路径与折过的报错原文。退出码恒为 0；两遍输出逐字一致。
# 本机 CPython 3.14.4 / typeguard 4.4.4（apt 装的）。别的版本、别的发行版没验过，别当事实引用。
# ============================================================================
import importlib.util
import inspect
import os
import shutil
import subprocess
import sys
import textwrap
import typing
import warnings
from pathlib import Path

import typeguard

WORK = Path("typeguard-demo")
PYNAME = os.path.basename(sys.executable)
CWD = Path.cwd().resolve()
HOME = os.path.expanduser("~")
CELL = {"n": 0, "ok": 0}
TOTAL = {"n": 0, "ok": 0}
WORK.mkdir(parents=True, exist_ok=True)     # 子进程用的 .py 全写在这里，脚本结尾整目录删掉


def mask(text):
    """报错原文里可能带绝对路径（dist-packages 的位置就在原文里），统一折掉。"""
    out = str(text)
    out = out.replace(sys.executable, PYNAME)
    for root, tag in ((str(CWD) + os.sep, ""), (str(CWD), "<cwd>"),
                      (HOME + os.sep, "<家>" + os.sep), (HOME, "<家>"),
                      (sys.base_prefix + os.sep, "<pyroot>" + os.sep),
                      (sys.prefix + os.sep, "<pyroot>" + os.sep)):
        if root and root != os.sep:
            out = out.replace(root, tag)
    return out


def show(tag, th):
    try:
        return f"  {tag} → {th()!r}"
    except Exception as exc:
        return f"  {tag} → 炸：{type(exc).__name__}: {mask(exc)}"


def bang(th):
    """只取「撞出来的那句原文」给 eq 比：成功就给 `没炸：repr`，失败给 `类型: 原文`（过 mask）。"""
    try:
        return f"没炸：{th()!r}"
    except Exception as exc:
        return f"{type(exc).__name__}: {mask(exc)}"


def eq(tag, th, want):
    """判据行：实测与预期逐字比对，两边都印在屏上。"""
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
    """把源码写成沙箱里的 .py 再跑：typeguard 要读源码，`-c` 里它自己会炸（第 7 格正是演这个）。"""
    path = WORK / name
    path.write_text(textwrap.dedent(src), encoding="utf-8")
    # 命令里只给文件名：cwd 已经是沙箱，再把 WORK 前缀拼上去就变成 typeguard-demo/typeguard-demo/xxx.py。
    proc = subprocess.run([sys.executable, path.name, *args],
                          capture_output=True, text=True, cwd=str(WORK),
                          env={"PATH": os.environ.get("PATH", ""), "HOME": str(WORK)})
    return proc.returncode, proc.stdout.strip(), proc.stderr.strip()


print("=== 1. 先点名：本章的六把静态尺一把都不在，连 pip 都没有 ===")
reset()
TOOLS = ["mypy", "pyright", "ruff", "black", "flake8", "pylint", "pytest"]
present = sorted(t for t in TOOLS if importlib.util.find_spec(t) is not None)
print("  取证对象：`importlib.util.find_spec(工具名)` 是「这个包能不能被 import」的最省事的问法。")
print(f"  │ ① 七件里在场的 → {present!r}")
print(f"  │ ② 缺席的（按字母序）→ {sorted(set(TOOLS) - set(present))!r}")
proc = subprocess.run([sys.executable, "-m", "pip", "--version"],
                      capture_output=True, text=True, env={"PATH": os.environ.get("PATH", "")})
print(f"  │ ③ `python3 -m pip --version` 的 rc 与原文 → rc={proc.returncode} "
      f"{mask(proc.stdout.strip() or proc.stderr.strip())!r}")
print("  → 所以「装一个 mypy 再学这一章」在本机不是建议，是做不到的事（不联网、不装包是本章红线）。")
print(eq("①", lambda: present, []))
print(eq("③", lambda: (proc.returncode, mask((proc.stdout + proc.stderr).strip())),
         (1, "python3: No module named pip")))
print(ledger("静态检查器七件全缺席（在场清单逐字 = []），pip 那条是 rc=1 + 原文 'No module named pip'"))

print("\n=== 2. typeguard 是谁、版本号从哪来（它自己身上没有 __version__）===")
reset()
print("  取证对象：`typeguard.__file__` 与 dist-packages 里的 dist-info 目录名。")
print(f"  │ ① typeguard.__file__（折过前缀）→ {mask(typeguard.__file__)!r}")
print(f"  │ ② hasattr(typeguard, '__version__') → {hasattr(typeguard, '__version__')!r}")


def dist_version():
    base = Path(typeguard.__file__).resolve().parent.parent
    hits = sorted(p.name for p in base.glob("typeguard-*") if p.name.endswith(".dist-info"))
    return [h[len("typeguard-"):-len(".dist-info")] for h in hits]


print(f"  │ ③ 从旁边那个 dist-info 目录名读到的版本 → {dist_version()!r}")
print("  → 想知道第三方库的版本，`import x; x.__version__` 只是**运气好**；")
print("    真正稳定的位置是包目录旁边那份 `<包名>-<版本>.dist-info`（pip 不在场时也只能这么读）。")
print(eq("②", lambda: hasattr(typeguard, "__version__"), False))
print(eq("③", lambda: dist_version(), ["4.4.4"]))
print(ledger("这份 typeguard 没有 __version__ 属性（False），版本号只能从 dist-info 目录名读到 4.4.4"))

print("\n=== 3. check_type 的真实签名：老写法三个位置参数，当场 TypeError ===")
reset()
sig = str(inspect.signature(typeguard.check_type))
print(f"  取证对象：typeguard 的底层入口 check_type。签名（折掉路径后逐字印）：{mask(sig)}")
print(f"  │ ① check_type([1, 'x'], list[int])（两参写法）→ {show('', lambda: typeguard.check_type([1, 'x'], list[int]))[2:]}")
print(f"  │ ② 按 3.x 的老写法传三个位置参数 → {show('', lambda: typeguard.check_type('v', [1, 'x'], list[int]))[2:]}")
print("  → 4.x 把「argname」挪成了关键字参数，位置参数只剩两个。抄来的老代码在这台机器上第一步就撞。")
print(eq("①", lambda: str(inspect.signature(typeguard.check_type)).split(") ->")[0].split(", ")[0:3],
         ["(value: 'object'", "expected_type: 'Any'", "*"]))
print(eq("② 老写法那句原文",
         lambda: bang(lambda: typeguard.check_type("v", [1, "x"], list[int])),
         "TypeError: check_type() takes 2 positional arguments but 3 were given"))
print(ledger("签名头三段逐字 = value / expected_type / *（后面全是关键字参数）；"
             "三个位置参数的老写法当场 TypeError，原文已逐字比过"))

print("\n=== 4. 默认策略只查容器第一个元素：[1,'x'] 过，['x',1] 拦 ===")
reset()
print("  取证对象：同一个 list[int]，把错元素放在第 0 位和第 1 位。")
msg0 = "TypeCheckError: item 0 of list is not an instance of int"
print(f"  │ ① check_type(['x', 1], list[int]) → {show('', lambda: typeguard.check_type(['x', 1], list[int]))[2:]}")
print(f"  │ ② check_type([1, 'x'], list[int]) → {show('', lambda: typeguard.check_type([1, 'x'], list[int]))[2:]}")
print(f"  │ ③ dict[str,int]：错值在第一个键 → {show('', lambda: typeguard.check_type({'b': 'x', 'a': 1}, dict[str, int]))[2:]}")
print(f"  │ ④ dict[str,int]：错值在第二个键 → {show('', lambda: typeguard.check_type({'a': 1, 'b': 'x'}, dict[str, int]))[2:]}")
print("  → 判据是「第 0 位响、后面的静默」。dict 那两格为什么两个键都没响？因为第一个键的**值**是 1（合法），")
print("    它连第二个键根本不看 —— 顺序稳定（同一进程里字面量顺序就是插入顺序），所以这条可以逐字复现。")
print(eq("①", lambda: (lambda: typeguard.check_type(['x', 1], list[int]))() and "不该到这里", msg0))
print(eq("②", lambda: typeguard.check_type([1, 'x'], list[int]), [1, 'x']))
print(eq("④", lambda: typeguard.check_type({'a': 1, 'b': 'x'}, dict[str, int]), {'a': 1, 'b': 'x'}))
print(ledger("list：错在第 0 位报 'item 0 of list is not an instance of int'，错在第 1 位原样返回；dict 第二个键同样静默"))

print("\n=== 5. 把策略改成 ALL_ITEMS：同一个输入就拦（两个成员的值也钉一下）===")
reset()
members = [(m.name, m.value) for m in typeguard.CollectionCheckStrategy]
print(f"  取证对象：CollectionCheckStrategy 这个枚举 —— 默认值就在函数签名里躺着。")
print(f"  │ ① 枚举成员（名字, 值）→ {members!r}")
msg1 = "TypeCheckError: item 1 of list is not an instance of int"
print(f"  │ ② check_type([1, 'x'], list[int], ALL_ITEMS) → {show('', lambda: typeguard.check_type([1, 'x'], list[int], collection_check_strategy=typeguard.CollectionCheckStrategy.ALL_ITEMS))[2:]}")
print(f"  │ ③ dict 版 ALL_ITEMS → {show('', lambda: typeguard.check_type({'a': 1, 'b': 'x'}, dict[str, int], collection_check_strategy=typeguard.CollectionCheckStrategy.ALL_ITEMS))[2:]}")
print("  → 第 4 格那两条静默，全部由这一个开关解释。写检查的时候先问自己：我要的是不是只查第一个。")
print(eq("①", lambda: members, [("FIRST_ITEM", 1), ("ALL_ITEMS", 2)]))
print(eq("②", lambda: (lambda: typeguard.check_type([1, 'x'], list[int],
       collection_check_strategy=typeguard.CollectionCheckStrategy.ALL_ITEMS))() and "不该到这里", msg1))
print(ledger("枚举逐字 = [('FIRST_ITEM',1), ('ALL_ITEMS',2)]；ALL_ITEMS 下 [1,'x'] 报 'item 1 of list …'"))

print("\n=== 6. @typechecked 管什么：参数、返回值、函数体里的局部变量注解 ===")
reset()


@typeguard.typechecked
def add(a: int, b: int) -> int:
    return a + b


@typeguard.typechecked
def to_str(a: int) -> str:
    return a


@typeguard.typechecked
def local_ann(a: int) -> int:
    y: str = 5                 # 函数体内的局部变量注解：a 篇证明过标准库运行时根本不求值它
    return a


print("  取证对象：三个被 @typechecked 装饰的函数，各撞一处。")
print(f"  │ ① add('a', 2) → {show('', lambda: add('a', 2))[2:]}")
print(f"  │ ② to_str(1)（返回值不是 str）→ {show('', lambda: to_str(1))[2:]}")
print(f"  │ ③ local_ann(1)（体内 y: str = 5）→ {show('', lambda: local_ann(1))[2:]}")


class Holder:
    def __init__(self, n: int):
        self.n = n


print(f"  │ ④ 实例属性赋值不拦：Holder(1).n = '字符串' 之后 holder.n → "
      f"{show('', lambda: (lambda h: (setattr(h, 'n', '字符串'), h.n)[1])(Holder(1)))[2:]}")
print("  → ③ 是本篇最值钱的读数：局部变量注解在标准库里是纯粹的文档（a 篇第 5 格），")
print("    但 typeguard 是**插桩执行**的，它把赋值也管起来了。④ 则说明它管不到属性：")
print("    `self.n = 值` 不是「参数」，没有注解可查 —— 数据类的字段类型它也不管。")
print(eq("①", lambda: (lambda: add('a', 2))() and "不该到这里",
         'TypeCheckError: argument "a" (str) is not an instance of int'))
print(eq("②", lambda: (lambda: to_str(1))() and "不该到这里",
         "TypeCheckError: the return value (int) is not an instance of str"))
print(eq("③", lambda: (lambda: local_ann(1))() and "不该到这里",
         "TypeCheckError: value assigned to y (int) is not an instance of str"))
print(ledger("参数、返回值、体内局部变量赋值三处原文逐字如上；实例属性赋值不拦（值原封不动是 '字符串'）"))

print("\n=== 7. 它靠读源码：`-c` 里当场 TypeError，标准输入里只发一句警告，写成文件才好 ===")
reset()
code = """
import typeguard

@typeguard.typechecked
def g(v: list[int]) -> int:
    return sum(v)

print("g([1, 2]) =", g([1, 2]))
"""
rc_dashc = subprocess.run([sys.executable, "-c", code.strip()], capture_output=True, text=True,
                          env={"PATH": os.environ.get("PATH", "")})
err_dashc = (rc_dashc.stderr.strip().splitlines() or [""])[-1]
print("  取证对象：同一份八行代码，三种入口。")
print(f"  │ ① python3 -c '<代码>' 的 rc 与最后一行报错 → rc={rc_dashc.returncode} {mask(err_dashc)!r}")
rc_file = run_child("tg_plain.py", code)
print(f"  │ ② 同样的代码写成 tg_plain.py 再跑 → rc={rc_file[0]} out={mask(rc_file[1])!r}")
rc_stdin = subprocess.run([sys.executable], input=code.strip(), capture_output=True, text=True,
                          env={"PATH": os.environ.get("PATH", "")})
err_stdin = (rc_stdin.stderr.strip().splitlines() or [""])[-1]
print(f"  │ ③ 喂给标准输入（`python3` 不带参数）→ rc={rc_stdin.returncode} 最后一行 {mask(err_stdin)!r}")
print("  → 报错原文里那句 \"<module '__main__' ...> is a built-in module\" 是 inspect.getfile 说的：")
print("    typeguard 装饰函数时要拿这个函数的源码来插桩，`-c`/REPL/标准输入这三份代码**没有文件**可读。")
print("    两条路的区别只在「有没有人告诉你」：`-c` 里它拿源码就炸了（TypeError 冒到顶层，rc=1）；")
print("    喂标准输入时 `inspect` 那条路它绕开了，改成发一句 InstrumentationWarning 然后照常跑完 —— rc=0、")
print("    g([1, 2]) 也算出来了，但那个装饰器**根本没装上**，检查一次都没发生。")
print("    这条对读者的实际意义：别在 `python -c` 或交互式里试 typeguard，会以为它坏了或以为它在管。")
print(eq("①", lambda: rc_dashc.returncode, 1))
print(eq("②", lambda: rc_file[1], "g([1, 2]) = 3"))
print(eq("③ rc", lambda: rc_stdin.returncode, 0))
print(eq("③ 最后一行", lambda: mask(err_stdin),
         "<stdin>:3: InstrumentationWarning: cannot instrument functions defined in a REPL "
         "-- not typechecking __main__.g"))
print(ledger("同一个装饰器三种入口：-c 是 rc=1 的 TypeError，标准输入是 rc=0 + 一句 InstrumentationWarning"
             "（警告里那句 not typechecking 就是「没装上」的原文），只有写成 .py 才真跑出 g([1, 2]) = 3"))

print("\n=== 8. 注解里的名字求不出值时：装饰器当场崩，check_type 按策略给三张脸 ===")
reset()
code8 = """
import typeguard

@typeguard.typechecked
def g(v: NotImported) -> int:
    return 1

print("永远不会打到这行 =", g("随便什么"))
"""
rc8, out8, err8 = run_child("tg_forward.py", code8)
last8 = (err8.splitlines() or [""])[-1]
print("  取证对象 ①：`@typechecked` + `def g(v: NotImported)`（注解里的名字根本不存在），子进程跑。")
print(f"  │ rc={rc8}，traceback 最后一行 → {mask(last8)!r}")


def probe(annotation, policy):
    """喂一次 check_type，把「返回值/报错」和「warning 列表」一起收回来。"""
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        try:
            got = ("返回", repr(typeguard.check_type("随便什么", annotation,
                                                    forward_ref_policy=policy)))
        except Exception as exc:
            got = ("炸", f"{type(exc).__name__}: {exc}")
    return got, [(w.category.__name__, str(w.message)) for w in caught]


FR = typing.ForwardRef("NotImported")
got_warn, w_warn = probe(FR, typeguard.ForwardRefPolicy.WARN)
got_err, w_err = probe(FR, typeguard.ForwardRefPolicy.ERROR)
got_ign, w_ign = probe(FR, typeguard.ForwardRefPolicy.IGNORE)
print("  取证对象 ②：同一个名字改成 **ForwardRef 对象** 喂 check_type，三种策略各跑一遍：")
print(f"  │ WARN（默认）→ {got_warn[0]} {got_warn[1]}｜warning {len(w_warn)} 条 {w_warn}")
print(f"  │ ERROR      → {got_err[0]} {got_err[1]}｜warning {len(w_err)} 条")
print(f"  │ IGNORE     → 返回 {got_ign[1]}｜warning {len(w_ign)} 条")
str_three = [probe("NotImported", p) for p in typeguard.ForwardRefPolicy]
same_str = str_three[0] == str_three[1] == str_three[2]
print("  取证对象 ③：把注解写成**字符串** \"NotImported\"，三种策略跑一遍，看它们是否给得出区别：")
print(f"  │ 三条读数是否完全相同 → {same_str}，实际内容：{str_three[0][0]}｜{str_three[0][1]}")
print("  → 两件事要分开记：")
print("    a) 装饰器那条路**没有降级** —— 插桩后的函数体在调用时才去读注解，读不到就是 NameError，程序直接崩")
print("       （上面 rc=1 那句就是它）。所以「typeguard 会宽容地跳过求不出的注解」这句只适用于 check_type。")
print("    b) check_type 那条路确实给三种脸：ERROR 抛 NameError、WARN 发一条 warning 后放行、IGNORE 连 warning 都不发")
print("       （静默放行是三者里最危险的：你既没被检查，也没被通知）。")
print("    c) 但 ③ 说明这套策略**只对真正的 ForwardRef 对象生效**：字符串注解会被更早的一条分支拦掉，")
print("       三种策略给出逐字相同的读数 —— 那句原文里的 imported from another module 就是它自陈的理由。")
print(eq("① rc", lambda: rc8, 1))
print(eq("① 最后一行", lambda: mask(last8), "NameError: name 'NotImported' is not defined"))
print(eq("② WARN", lambda: (got_warn, w_warn),
         (("返回", "'随便什么'"), [("TypeHintWarning", "Cannot resolve forward reference 'NotImported'")])))
print(eq("② ERROR", lambda: (got_err, w_err),
         (("炸", "NameError: name 'NotImported' is not defined"), [])))
print(eq("② IGNORE", lambda: (got_ign, w_ign), (("返回", "'随便什么'"), [])))
print(eq("③ 字符串三种策略同读数", lambda: same_str, True))
print(eq("③ 原文", lambda: str_three[0][1],
         [("TypeHintWarning", "Skipping type check against 'NotImported'; this looks like a string-form "
                              "forward reference imported from another module")]))
print(ledger("装饰器 rc=1 + NameError 原文；ForwardRef 三种策略分别是「1 条 warning 放行 / 抛 NameError / 0 条静默放行」；"
             "字符串注解三种策略逐字相同（判据条数=1）"))

print("\n=== 9. 不想逐个写装饰器：install_import_hook 给整个包自动加检查 ===")
reset()
code9 = """
from typeguard import install_import_hook
import payload                       # 此刻 hook 还没装

def call(tag, mod):
    try:
        print(f"{tag}：没炸，返回 {mod.total(['x', 1])}")
    except Exception as exc:
        print(f"{tag}：{type(exc).__name__} | {str(exc)}")

call("A 不带 hook", payload)
with install_import_hook(packages=["payload2"]):
    import payload2                  # 装好 hook 之后才加载 → 这个模块被插桩
    call("B hook 内", payload2)
call("C 退出 with 之后再调 payload2", payload2)
import payload3                      # 先加载（无 hook）
with install_import_hook():          # packages=None：管「之后加载的」所有模块
    import payload3                  # sys.modules 已有同名缓存 → 不会重新加载
    call("D 先 import 后装 hook", payload3)
"""
for name in ("payload.py", "payload2.py", "payload3.py"):
    (WORK / name).write_text(
        "def total(v: list[int]) -> int:\n    return sum(v)\n", encoding="utf-8")
rc9, out9, err9 = run_child("tg_hook.py", code9)
print("  取证对象 ①：`install_import_hook` 的签名（第一个位置参数叫什么，决定你传字符串有没有用）。")
print(f"  │ 签名 → {mask(str(inspect.signature(typeguard.install_import_hook)))!r}")
print("  取证对象 ②：三个内容完全一样的模块 payload/payload2/payload3，四种「装 hook / 加载」顺序。")
print(f"  │ 子进程 rc={rc9}，四行读数逐字：")
for line in out9.splitlines():
    print(f"  │    {mask(line)}")
if err9:
    print(f"  │    stderr：{mask(err9)[:160]}")
print("  → 四行里有两对是**同一句原文**，而它们之间的差别恰好就是这格要教的：")
print("    A=D（都报 TypeError）：payload 从没被插桩；payload3 是「先 import 后装 hook」—— 它已经在")
print("      sys.modules 里，hook 只改「之后加载的模块」，对它一次都没生效。这两行同读数就是那条 docstring")
print("      「This only affects modules loaded **after** this hook has been installed」的实证。")
print("    B=C（都报 TypeCheckError）：hook 退出只卸载 finder，已经插好桩的那个模块对象继续被检查。")
print("      换句话说「出了 with 就恢复干净」是错的直觉 —— 影响范围是**加载时机**，不是 with 的生命周期。")
print("    还有一条：把包名当第一个位置参数传（`install_import_hook(\"payload2\")`）不会报错，")
print("    只会**静默地什么都不插桩** —— 上面那个签名里的参数名 packages 是唯一正确的写法。")
print(eq("① 参数名是 packages（关键字），没有 position 的 path", lambda:
         str(inspect.signature(typeguard.install_import_hook)),
         "(packages: 'Iterable[str] | None' = None, *, cls: 'type[TypeguardFinder]' = "
         "<class 'typeguard.TypeguardFinder'>) -> 'ImportHookManager'"))
print(eq("② 四行读数逐字", lambda: out9.splitlines(), [
    "A 不带 hook：TypeError | unsupported operand type(s) for +: 'int' and 'str'",
    'B hook 内：TypeCheckError | item 0 of argument "v" (list) is not an instance of int',
    'C 退出 with 之后再调 payload2：TypeCheckError | item 0 of argument "v" (list) is not an instance of int',
    "D 先 import 后装 hook：TypeError | unsupported operand type(s) for +: 'int' and 'str'",
]))
print(ledger("签名参数名 packages 逐字；四行读数 = A/D 同一句 TypeError、B/C 同一句 TypeCheckError"
             "（判据是这四行逐字对上，两两相同正是「管加载时机、不管 with 生命周期」的形状）"))

print("\n=== 10. 收尾：typeguard 的一句话账本，和它管不到的地方 ===")
reset()
print("  ① 它是**运行时插桩**：每次调用真的多干活。本篇故意不测耗时 ——")
print("     「慢多少倍」每跑必变，而本章的捕获要求两遍逐字一致，所以宁可不在屏上报这个数。")
print("     要量请自己跑 `python -m timeit`，别引用别处的倍数。")
print(f"  ② 它管：参数类型、返回值类型、函数体内被注解变量的赋值（第 6 格三条原文）。")
print(f"  ③ 它不管：实例属性、dataclass 字段、以及**容器里除第一个元素以外的元素**（第 4 格）。")
print(f"  ④ 它要求代码在**文件里**（第 7 格）：`-c` 当场 rc=1，标准输入 rc=0 但装饰器没装上、只留一句警告。")
print(f"  ⑤ 注解名求不出值时**分两条路**（第 8 格）：`@typechecked` 的函数当场 NameError 崩；")
print(f"     直调 check_type 才谈策略 —— WARN 放行+1 条 warning、ERROR 抛 NameError、IGNORE 连警告都不发。")
print(f"     字符串注解走不到这套策略：三种取值给出逐字相同的读数（第 8 格取证对象 ③）。")
print(f"  ⑥ forward_ref_policy 的三个成员（名字, 值）→ "
      f"{[(m.name, m.value) for m in typeguard.ForwardRefPolicy]!r}")
print("  ⑦ install_import_hook 管的是**加载时机**：装上之后才第一次 import 的模块才被插桩；"
      "已经进 sys.modules 的名字不会重插（第 9 格 A=D 那两行），退出 with 也不会让插好的模块掉桩（B=C）。")
print(eq("⑥", lambda: [(m.name, m.value) for m in typeguard.ForwardRefPolicy],
         [("ERROR", 1), ("WARN", 2), ("IGNORE", 3)]))
print(ledger("ForwardRefPolicy 三个成员逐字 = [('ERROR',1),('WARN',2),('IGNORE',3)]；"
             "七条账本全部有上面九格的屏上读数兜着"))

shutil.rmtree(WORK, ignore_errors=True)
print(f"\n  收尾自查：沙箱 {WORK}/ 删干净了吗 → {not WORK.exists()!r}")
print(f"  全篇判据合计 {TOTAL['ok']}/{TOTAL['n']} 条成立（少一条上面就有 [不成立！]）")
sys.exit(0)
