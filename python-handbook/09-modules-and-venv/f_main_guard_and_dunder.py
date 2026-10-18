#!/usr/bin/env python3
# ============================================================================
# 取证脚本 f_main_guard_and_dunder.py —— 第 09 章《模块、导入与环境》第 6 讲
#
# 只证三件事：
#   ① `__name__` / `__package__` 不是仪式用的装饰：四种启动方式打出四种值，
#      相对导入能不能用、包的 __init__ 跑不跑，全由它们决定；
#   ② 没有 `if __name__ == "__main__"` 时，别人 import 你一下，你就把整个脚本跑一遍
#      （建文件、跑主逻辑 —— 用计数证明，不用嘴证明）；
#   ③ `from x import *` 的真实边界：默认只放公开名字；`__all__` 一旦存在就是**唯一**白名单
#      （私有名字能放进来、公开名字能挡在外面）；它会无声覆盖你命名空间里的同名对象；
#      而且它进不了函数体 —— 本机 3.14.4 给的是编译期 SyntaxError 原文。
#
# 沙箱：当前目录下的 guard-demo/，末尾 shutil.rmtree 删干净并当场自证。
# 只用标准库；不联网、不装包、不 import 兄弟脚本；子进程 [sys.executable] + cwd= + env=
# （先剥掉继承的 PYTHON*）。屏幕上只有 basename、布尔和计数，没有绝对路径、没有毫秒。
# 本机：CPython 3.14.4 / Ubuntu 26.04。其它版本/平台的措辞 —— 未验证，别当事实引用。
# ============================================================================
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path.cwd().resolve()
BOX = Path("guard-demo")
PYC_BEFORE = Path("__pycache__").exists()
LINES = []
RUNS = 0

# 每个探测模块都带这段：打一行五个字段（路径只留 basename，spec 只打「是不是 None」）
PROBE = '''import os
import sys


def line(tag):
    return (f"{tag} | __name__={__name__!r} __package__={__package__!r} "
            f"__spec__是None={__spec__ is None} "
            f"__file__={os.path.basename(__file__)} "
            f"argv0={os.path.basename(sys.argv[0])} "
            f"sys.path[0]是cwd={os.path.abspath(sys.path[0]) == os.getcwd()} "
            f"sys.path[0是本文件目录="
            f"{os.path.abspath(sys.path[0]) == os.path.dirname(os.path.abspath(__file__))}")
'''

SRC_SOLO = PROBE + '''

print(line("solo"))
'''
SRC_PK_INIT = '''print("BODY-pk__init__")
'''
SRC_PK_MOD = PROBE + '''

print(line("pk.mod"))
'''
SRC_PK_USE = PROBE + '''

from . import mod                          # 包内相对导入：靠的是 __package__

print("相对导入成功，拿到的是", mod.__name__)
print(line("pk.use"))
'''
SRC_PK_USEABS = '''import mod                                 # 绝对导入兄弟模块：靠的是 sys.path[0]

print("绝对导入成功，mod.__name__ =", mod.__name__)
'''
SRC_PK_MAIN = PROBE + '''

print(line("pk.__main__"))
from . import mod                          # 包入口里相对导入也成立
print("入口拿到的是", mod.__name__)
'''
SRC_NOG = '''from pathlib import Path

print("BODY-nog")
Path("side_nog.txt").write_text("被模块体建出来了", encoding="utf-8")
print("MAIN-nog：主逻辑无条件跑了")
'''
SRC_HAS = '''from pathlib import Path

print("BODY-has")


def main():
    Path("side_has.txt").write_text("只有被当脚本时才建", encoding="utf-8")
    print("MAIN-has：主逻辑只在当脚本时跑")


if __name__ == "__main__":
    main()
'''
SRC_PUB = '''pi = 3.14159
gamma = "公开的 gamma"
_alpha = "一个下划线开头的 _alpha"
__beta = "两个下划线开头的 __beta"
'''
SRC_ALL = '''__all__ = ["pi", "_hidden"]                # 一旦存在，它就是唯一白名单

pi = 3.14159
_hidden = "写进 __all__ 的私有名字"
public_thing = "公开但没写进 __all__ 的名字"
'''
SRC_MISSING = '''__all__ = ["nope"]                         # 白名单里写了一个不存在的名字
'''
SRC_WRONGTYPE = '''__all__ = 5                                # 白名单不是字符串序列
'''
SRC_STAR_CHECK = '''import m_pub

MACHINE = {"__name__", "__doc__", "__package__", "__loader__", "__spec__",
           "__file__", "__cached__", "__builtins__", "__annotations__"}   # 解释器自己塞的
print("m_pub 里的公开名字 →", sorted(k for k in vars(m_pub) if not k.startswith("_")))
print("m_pub 里以下划线开头、归作者自己的名字 →",
      sorted(k for k in vars(m_pub) if k.startswith("_") and k not in MACHINE))
snap = set(globals())
from m_pub import *
got = sorted(k for k in globals() if k not in snap and k != "snap")
print("from m_pub import * 实际给进来的 →", got)
'''
SRC_STAR_LOCAL = '''pi = 9.99                                  # 本地先占着这个名字
before = sorted(k for k in globals() if not k.startswith("__") and k != "before")
from m_pub import *                        # ← 星号导入
after = sorted(k for k in globals() if not k.startswith("__") and k != "before")
print("导入前本地公开名字 =", before)
print("导入后本地公开名字 =", after)
print("pi 现在是 →", pi, "（原来是 9.99）")
print("新增的名字（排序）→", [n for n in after if n not in before])
print("__name__ 还在吗、有没有被冲掉 →", __name__)
print("_alpha 进来过吗 →", "_alpha" in globals(), "｜ __beta 呢 →", "__beta" in globals())
'''
SRC_STAR_FN = '''def f():
    from m_pub import *                    # 函数体里用星号
    return pi
'''
SRC_STAR_CLS = '''class C:
    from m_pub import *                    # 类体里用星号
'''
DRIVER_IMPORT_TWICE = '''import nog
import nog
print("上面两行的行数就是答案")
'''

FILES = [
    ("solo.py", SRC_SOLO),
    ("pk/__init__.py", SRC_PK_INIT),
    ("pk/mod.py", SRC_PK_MOD),
    ("pk/use.py", SRC_PK_USE),
    ("pk/use_abs.py", SRC_PK_USEABS),
    ("pk/__main__.py", SRC_PK_MAIN),
    ("nog.py", SRC_NOG),
    ("has.py", SRC_HAS),
    ("m_pub.py", SRC_PUB),
    ("m_all.py", SRC_ALL),
    ("m_missing.py", SRC_MISSING),
    ("m_wrongtype.py", SRC_WRONGTYPE),
    ("star_check.py", SRC_STAR_CHECK),
    ("star_local.py", SRC_STAR_LOCAL),
    ("star_fn.py", SRC_STAR_FN),
    ("star_cls.py", SRC_STAR_CLS),
    ("drv_import_twice.py", DRIVER_IMPORT_TWICE),
]


def mask(text):
    out = text
    for base in (str(ROOT), os.getcwd(), str(Path.home())):
        if base and base != "/":
            out = out.replace(base + "/", "").replace(base, ".")
    return out


def emit(text):
    for ln in mask(str(text)).splitlines():
        LINES.append(ln)
        print(ln)


def clean_env(extra=None):
    env = {k: v for k, v in os.environ.items() if not k.startswith("PYTHON")}
    env["PYTHONIOENCODING"] = "utf-8"
    env.update(extra or {})
    return env


def run(args, sub="."):
    global RUNS
    RUNS += 1
    cwd = BOX if sub == "." else BOX / sub
    proc = subprocess.run([sys.executable, *args], cwd=str(ROOT / cwd), env=clean_env(),
                          capture_output=True, text=True, encoding="utf-8")
    return proc.returncode, mask(proc.stdout), mask(proc.stderr)


def dump(text, indent="      "):
    """原样搬上屏幕：只去尾空格，保留 traceback 的缩进和 ^^^^ 指示行。"""
    for ln in text.splitlines():
        if ln.strip():
            emit(indent + ln.rstrip())


def flat(text):
    return " ; ".join(ln.strip() for ln in text.splitlines() if ln.strip()) or "(无输出)"


def errline(text):
    lines = [ln.strip() for ln in text.splitlines() if ln.strip()]
    return lines[-1] if lines else "(没有 stderr)"


shutil.rmtree(BOX, ignore_errors=True)
for rel, text in FILES:
    path = BOX / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")

print("=== 1. 同一个文件，四种启动方式打出四种 __name__ / __package__（真子进程，原文照打） ===")
for label, args in [
    ("python3 solo.py", ["solo.py"]),
    ("python3 -c 'import solo'", ["-c", "import solo"]),
    ("python3 -m pk.mod", ["-m", "pk.mod"]),
    ("python3 pk/mod.py", ["pk/mod.py"]),
]:
    rc, out, err = run(args)
    emit(f"  {label}｜ rc={rc}")
    dump(out)
    if err.strip():
        emit(f"      stderr → {errline(err)}")
emit("  → 脚本模式和 -m 模式都把 __name__ 打成 '__main__'，区别全在 __package__：")
emit("    脚本模式 None（这文件不属于任何包 → 相对导入必炸），-m 模式 'pk'（有包 → 相对导入可用）。")
emit("  → 被 import 时 __name__ 就是模块名、__package__ 是空串、__spec__ 不是 None。")
emit("  → 最后两列顺带解释了「同一条 import 换个目录就找不到」：脚本模式 sys.path[0] 是**脚本所在目录**，")
emit("    -m / -c 模式是**当前工作目录**。同一个模块名，在两种跑法下能是两个不同的东西。")

print()
print("=== 2. 没有 guard 会怎样：别人 import 你一下，你整个脚本跑一遍（计数说话） ===")
for label, args, body, main, side in [
    ("python3 nog.py（当脚本）", ["nog.py"], "BODY-nog", "MAIN-nog", "side_nog.txt"),
    ("python3 -c 'import nog'（当模块）", ["-c", "import nog"], "BODY-nog", "MAIN-nog", "side_nog.txt"),
    ("python3 has.py（当脚本）", ["has.py"], "BODY-has", "MAIN-has", "side_has.txt"),
    ("python3 -c 'import has'（当模块）", ["-c", "import has"], "BODY-has", "MAIN-has", "side_has.txt"),
]:
    (BOX / side).unlink(missing_ok=True)
    rc, out, err = run(args)
    emit(f"  {label}｜ rc={rc} ｜ 模块体执行={out.count(body)} 次 ｜ 主逻辑执行={out.count(main)} 次"
         f" ｜ 副作用文件建了吗 → {(BOX / side).exists()}")
(BOX / "side_nog.txt").unlink(missing_ok=True)
rc, out, err = run(["drv_import_twice.py"])
emit(f"  同一进程里 import nog 两遍｜ rc={rc} ｜ 模块体执行={out.count('BODY-nog')} 次 ｜ "
     f"主逻辑执行={out.count('MAIN-nog')} 次 ｜ 副作用文件建了吗 → {(BOX / 'side_nog.txt').exists()}")
emit("  → 两件事分得很清：guard 管「当脚本还是当模块」，sys.modules 管「一个进程里跑几遍」。")
emit("    没有 guard 时，任何一次 import 都把主逻辑和副作用全量执行一遍 —— 别人用你是无意的，事故是有意的。")
emit("  ★ 加了 guard 也不等于安全：模块体那一半照样执行（BODY-has=1）。import 是执行，不是声明。")

print()
print("=== 3. 包内相对导入：-m 与 路径/x.py 的差别就写在 __package__ 上 ===")
rc, out, err = run(["-m", "pk.use"])
emit(f"  `python3 -m pk.use`｜ rc={rc}")
dump(out)
emit(f"    包的 __init__ 跑了吗（BODY-pk__init__ 计数）→ {out.count('BODY-pk__init__')}")
rc, out, err = run(["pk/use.py"])
emit(f"  `python3 pk/use.py`（同一份文件，用路径起）｜ rc={rc}")
dump(err)
emit(f"    这一跑里 __package__ 是 None（见第 1 节），所以相对导入当场被拒 —— 报错原文就是上面那行。")
rc, out, err = run(["pk/use_abs.py"])
emit(f"  `python3 pk/use_abs.py`（改成绝对导入兄弟模块）｜ rc={rc} ｜ 输出 → {flat(out)}")
rc, out, err = run(["-m", "pk.use_abs"])
emit(f"  `python3 -m pk.use_abs`（同一行代码，换 -m 起）｜ rc={rc} ｜ 原文 → {errline(err)}")
rc, out, err = run(["-m", "pk"])
emit(f"  `python3 -m pk`（跑包的 __main__.py）｜ rc={rc}")
dump(out)
emit("  → 两套查找规则别混：相对导入问 __package__，绝对导入问 sys.path。")
emit("    脚本模式下 sys.path[0] 恰好是 pk/，所以 `import mod` 能成 —— 但它拿到的是**另一个模块**")
emit("    （__name__='mod'，不是 'pk.mod'）：同一份源码在进程里存在两份实例，第 1 行就看得出来。")
emit("  ★ 结论只有一句：住在包里的东西用 `-m` 起，别用路径起。")

print()
print("=== 4. from x import *：默认只放「不以下划线开头」的名字 ===")
rc, out, err = run(["star_check.py"])
emit(f"  驱动 `star_check.py`（先 import m_pub 看家底，再星号导入看进来了什么）：rc={rc}")
dump(out)
emit("  → _alpha、__beta 一个都没进来：默认过滤器就是「下划线开头一律不给」，一个字节都不商量。")
emit("    在 `import *` 这一条规则里，前导下划线是**语法**，不是「我自己看的注释」。")

print()
print("=== 5. __all__ 一旦出现就是唯一白名单：私有能放进来，公开能被挡在外面 ===")
rc, out, err = run(["-c", "from m_all import *\nprint(sorted(k for k in globals() if not k.startswith('__')))"])
emit(f"  m_all.py 里写着 __all__ = ['pi', '_hidden']｜`from m_all import *` 之后本地名字 → {flat(out)}")
rc, out, err = run(["-c", "import m_all\n"
                    "print('模块里其实有：', sorted(k for k in vars(m_all) if not k.startswith('_')))\n"
                    "print('dir() 里还有 public_thing 吗：', 'public_thing' in dir(m_all))"])
emit(f"  对照（白名单只管星号，不管 getattr）→ {flat(out)}")
rc, out, err = run(["-c", "from m_missing import *"])
emit(f"  ★ 白名单里写了不存在的名字 → rc={rc} ｜ 原文 → {errline(err)}")
rc, out, err = run(["-c", "from m_wrongtype import *"])
emit(f"  ★ 白名单类型写错（__all__ = 5）→ rc={rc} ｜ 原文 → {errline(err)}")
emit("  → 所以 __all__ 不是 README，是 `*` 的定义域：写了它就要对每个名字负责，漏一个就是运行时响。")

print()
print("=== 6. 星号的两条硬边界：它会无声覆盖同名对象，而且进不了函数体 / 类体 ===")
rc, out, err = run(["star_local.py"])
emit(f"  驱动 `star_local.py`（本地先有 pi = 9.99，再 from m_pub import *）：rc={rc}")
dump(out)
emit("  → pi 被覆盖成模块里那个 3.14159，屏幕上不会有任何警告；双下划线名字反而安全（默认过滤器不给）。")
for label, script in (("函数体里用星号", "star_fn.py"), ("类体里用星号", "star_cls.py")):
    rc, out, err = run([script])
    emit(f"  {label}（`python3 {script}`）｜ rc={rc} ｜ 这个文件一行标准输出都没有 → {not out.strip()}")
    dump(err, indent="      ")
emit("  → 本机 3.14.4 给的是**编译期** SyntaxError：文件里一行代码都没执行就死了（注意上面 rc=1、无 stdout）。")
emit("    原因很实在：函数体/类体的名字集合要到运行时才知道，而星号要求编译期就把命名空间定下来。")
emit("  ★ 更早的版本在这里是 SyntaxWarning 还是 SyntaxError —— 未验证，别当事实引用。")

print()
print("=== 7. 收口：guard 不是仪式，它决定「这个文件被当脚本还是被当模块」 ===")
print("  你敲的东西               │ 本机实测的字段（第 1 节原文缩写）              │ 后果")
emit("  python3 x.py             │ __name__='__main__' __package__=None          │ 相对导入必炸；guard 里的主逻辑跑")
emit("  python3 -m pkg.x         │ __name__='__main__' __package__='pkg'         │ 相对导入可用；pkg/__init__ 先跑一遍")
emit("  import x / import pkg.x  │ __name__='x' / 'pkg.x' __package__='' / 'pkg' │ guard 里的主逻辑一次都不跑")
emit("  计数小结（本机）：无 guard 被 import → 主逻辑 1 次 + 副作用文件建了；有 guard 被 import → 主逻辑 0 次、"
     "文件没建、但模块体仍执行 1 次")
emit("  星号小结（本机）：默认过滤下划线开头 ｜ __all__ 是唯一白名单（私有进、公开挡）｜ 同名对象被无声覆盖 ｜ "
     "函数体与类体里直接 SyntaxError")
print("  三条判据：")
emit("  ① 这个文件只要可能被人 import，主逻辑就必须待在 guard 里 —— 不写就是把执行权交出去。")
emit("  ② 包里的东西用 `-m` 起，别用路径起：__package__ 决定相对导入的生死（第 3 节两条原文互为对照）。")
emit("  ③ 库要暴露 API 就显式写 __all__ 并保证每个名字都对得上；应用代码里别写星号 —— 覆盖是无形的。")
emit("  ★ 落点：__name__ / __package__ 不是玄学标签，是解释器在启动那一刻写好的两个变量，"
     "它们替你决定了这一份源码是谁、能看见谁。")

shutil.rmtree(BOX)
left = len(list(BOX.rglob("*"))) if BOX.exists() else 0
emit(f"  收尾自证：{BOX.name}/ 还在吗 → {BOX.exists()} ｜ 残留条目 {left} 个 ｜ 本次起真子进程 {RUNS} 个 ｜ "
     f"沙箱里的 __pycache__ 随它一起删除")
emit(f"  当前目录的 __pycache__ 有没有被本脚本新增 → {Path('__pycache__').exists() != PYC_BEFORE}")
user = Path.home().name
leaks = sum(1 for ln in LINES if str(ROOT) in ln or (user and user in ln) or "/tmp/" in ln or ln.startswith("/"))
emit(f"  输出自检：出现绝对路径 / 临时目录 / 登录名的行数 = {leaks}")
