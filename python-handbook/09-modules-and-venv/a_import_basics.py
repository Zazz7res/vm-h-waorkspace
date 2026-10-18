#!/usr/bin/env python3
# ============================================================================
# 取证脚本 a_import_basics.py —— 第 09 章第 1 篇：`import` 那一刻到底发生了什么
#
# 要证明什么：`import x` 不是「把文件读进来」，而是五件可以分开验证的事 ——
#   ① 先查 sys.modules 这本账，命中就什么都不做；② 没命中才按 sys.path 找文件；
#   ③ 找到一个模块对象，把模块体**在这个进程里唯一一次**执行掉；
#   ④ 把名字绑到「当前命名空间」，绑的是引用不是内容；⑤ 顺手在父包对象上挂个属性。
# 本脚本把①③④⑤各撞一次：模块体里写一个计数文件，import 三次只长一个字符；
# 把 sys.modules 里那条删掉，同一个文件立刻又跑一遍 —— 「只跑一次」的开关是缓存，
# 不是文件系统，这条记错就会以为「改了 .py 再 import 就生效」。
#
# 为什么要有它：第 05 章讲过「def 只跑一次，默认值在定义那一刻冻住」；模块级代码同理。
# 改模块属性 = 改全局状态（monkeypatch 的全部原理），`del x` 只删本地绑定、
# 模块对象还在 sys.modules 里活着 —— 这两件事是「为什么我的补丁没生效」的标准答案。
#
# 现场全在本目录的 import-basics-demo/ 里现造现删；屏幕上只打相对路径（绝对前缀当场替换）。
# 子进程一律 [sys.executable, ...] + cwd= + env=（干净 env，PYTHONPATH/PYTHONHOME 从不继承）。
# 退出码恒为 0；跑两遍输出逐字一致（无时间、无 PID、无对象地址、无内存数字）。
# 本机 Python 3.14.4 / Ubuntu 26.04。3.8 的隐式相对导入规则、Windows 行为本机未验证，
# 别当事实引用。
# ============================================================================
import os
import shutil
import subprocess
import sys
from pathlib import Path

WORK = Path("import-basics-demo")
APP = WORK / "app"
PYNAME = os.path.basename(sys.executable)
CWD = Path.cwd().resolve()
HOME = os.path.expanduser("~")

ONCE_SRC = '''"""once.py：模块体里自带一个计数器，用来数它到底被跑了几遍。"""
import os

with open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "run.log"),
          "a", encoding="utf-8") as _fh:
    _fh.write("R")

print("[once.py 的模块体正在执行]")
VALUE = 42


def double(v):
    return v * 2
'''

FILES = {
    "once.py": ONCE_SRC,
    "money.py": '"""money.py：默认值写成模块级的账本，第 5 段拿它做「改全局状态」的实验。"""\n'
                "RATE = 0.05\n\n\n"
                "def fee_fixed(amount, rate=RATE):\n"
                "    return amount * rate\n\n\n"
                "def fee_live(amount, rate=None):\n"
                "    return amount * (RATE if rate is None else rate)\n\n\n"
                "def total(amount):\n"
                "    return fee_live(amount) + fee_fixed(amount)\n",
    "pk/__init__.py": 'PKG = "pk 的 __init__"\n',
    "pk/sub/__init__.py": 'SUB = "pk.sub 的 __init__"\n',
    "pk/sub/leaf.py": 'LEAF = "pk.sub.leaf"\n',
    "doc_after_comment.py": "# 注释不算语句\n\n"
                            '"注释后面的这条字符串才是第一条语句 → 它就是 __doc__"\n',
    "doc_after_stmt.py": 'X = 1\n"已经赋值过了，这条字符串只是被丢掉的一个表达式"\n',
    "starbad.py": '__all__ = ["present", "ghost"]\npresent = 1\n',
    "relsrc.py": "from . import helper\n",
    # ---- 子进程脚本 ----
    "r1_once.py": "import sys\n"
                  "from importlib import import_module\n"
                  "import once\n"
                  "import once\n"
                  "import_module(\"once\")\n"
                  "print(\"① 同一进程里 import 三次之后，run.log 的 R 个数 =\", len(open(\"run.log\", encoding=\"utf-8\").read()))\n"
                  "print(\"② once is sys.modules['once'] →\", once is sys.modules[\"once\"])\n"
                  "print(\"③ vars(once) is vars(sys.modules['once']) →\", vars(once) is vars(sys.modules[\"once\"]))\n"
                  "once.VALUE = 99\n"
                  "print(\"④ 改一次，两边同时看到：once.VALUE =\", once.VALUE, \"| sys.modules 那份 =\", sys.modules[\"once\"].VALUE)\n"
                  "del sys.modules[\"once\"]\n"
                  "import once\n"
                  "print(\"⑤ 把 sys.modules 里那条删掉再 import → R 个数 =\", len(open(\"run.log\", encoding=\"utf-8\").read()))\n"
                  "print(\"⑥ 模块体重跑把 VALUE 重置回\", once.VALUE, \"（第 ④ 步的 99 被冲掉了）\")\n"
                  "before = sys.modules[\"once\"]\n"
                  "import_module(\"once\")\n"
                  "importlib_reload = __import__(\"importlib\").reload\n"
                  "importlib_reload(once)\n"
                  "print(\"⑦ 显式 reload 也重跑模块体 → R 个数 =\", len(open(\"run.log\", encoding=\"utf-8\").read()))\n"
                  "print(\"⑧ 但 reload 不换对象：还是同一个模块吗 →\", once is before is sys.modules[\"once\"])\n",
    "r2_newproc.py": "import once\n"
                     "print(\"⑨ 全新进程第一次 import → run.log 的 R 个数 =\", len(open(\"run.log\", encoding=\"utf-8\").read()))\n",
    "r3_bind.py": "import sys\n"
                  "base = None\n"
                  "base = set(globals())\n"
                  "import once\n"
                  "print(\"① import once                → 本地新增\", sorted(set(globals()) - base))\n"
                  "base = set(globals())\n"
                  "from once import VALUE\n"
                  "print(\"② from once import VALUE     → 本地新增\", sorted(set(globals()) - base))\n"
                  "base = set(globals())\n"
                  "import pk.sub.leaf\n"
                  "print(\"③ import pk.sub.leaf         → 本地新增\", sorted(set(globals()) - base))\n"
                  "print(\"   但 sys.modules 里进了三个:\", [k for k in (\"pk\", \"pk.sub\", \"pk.sub.leaf\") if k in sys.modules])\n"
                  "base = set(globals())\n"
                  "import once as od\n"
                  "print(\"④ import once as od          → 本地新增\", sorted(set(globals()) - base))\n"
                  "del od\n"
                  "del once\n"
                  "try:\n"
                  "    once.VALUE\n"
                  "except NameError as exc:\n"
                  "    print(\"⑤ del once 之后写 once.VALUE →\", type(exc).__name__ + \": \" + str(exc))\n"
                  "print(\"⑥ 'once' 还在 sys.modules 里吗 →\", \"once\" in sys.modules,\n"
                  "      \"| 从账本里拿值：sys.modules['once'].VALUE =\", sys.modules[\"once\"].VALUE)\n"
                  "print(\"⑦ from 进来的那个名字不受 del 影响：VALUE =\", VALUE, \"| 它是 int 吗 →\", type(VALUE).__name__)\n",
    "r4_attrs.py": "import builtins\n"
                   "import os\n"
                   "import sys\n"
                   "import types\n"
                   "import money\n"
                   "import doc_after_comment\n"
                   "import doc_after_stmt\n"
                   "print(\"① type(money) =\", type(money).__name__,\n"
                   "      \"| isinstance(money, types.ModuleType) →\", isinstance(money, types.ModuleType))\n"
                   "print(\"② money.__name__ =\", repr(money.__name__), \"| __package__ =\", repr(money.__package__),\n"
                   "      \"| 有 __path__ 吗 →\", hasattr(money, \"__path__\"))\n"
                   "import pk.sub.leaf\n"
                   "print(\"   同一个键在包里就不一样：pk.__package__ =\", repr(sys.modules[\"pk\"].__package__),\n"
                   "      \"| pk.sub.leaf.__package__ =\", repr(sys.modules[\"pk.sub.leaf\"].__package__),\n"
                   "      \"| leaf.__name__ =\", repr(sys.modules[\"pk.sub.leaf\"].__name__))\n"
                   "print(\"③ money.__doc__ =\", repr(money.__doc__))\n"
                   "print(\"   只有「第一条语句是字符串」才算 doc：赋值之后 →\", repr(doc_after_stmt.__doc__))\n"
                   "print(\"   注释不顶位置：注释后那条字符串 →\", repr(doc_after_comment.__doc__))\n"
                   "print(\"④ money.__file__ =\", money.__file__)\n"
                   "print(\"   money.__cached__ =\", money.__cached__)\n"
                   "print(\"   __spec__.origin =\", money.__spec__.origin,\n"
                   "      \"| __spec__.submodule_search_locations =\", repr(money.__spec__.submodule_search_locations))\n"
                   "print(\"⑤ __loader__ 的类型 =\", type(money.__loader__).__name__,\n"
                   "      \"| __spec__ 的类型 =\", type(money.__spec__).__name__)\n"
                   "print(\"⑥ money.__dict__ 类型 =\", type(money.__dict__).__name__,\n"
                   "      \"| vars(money) is money.__dict__ →\", vars(money) is money.__dict__)\n"
                   "print(\"   __dict__ 的键（排序） =\", sorted(money.__dict__))\n"
                   "print(\"⑦ 模块里的 __builtins__ =\", type(money.__builtins__).__name__,\n"
                   "      \"| is builtins.__dict__ →\", money.__builtins__ is builtins.__dict__,\n"
                   "      \"| is builtins 模块 →\", money.__builtins__ is builtins)\n"
                   "print(\"   __main__（脚本本体）里的 __builtins__ =\", type(sys.modules[\"__main__\"].__builtins__).__name__,\n"
                   "      \"| is builtins 模块 →\", sys.modules[\"__main__\"].__builtins__ is builtins)\n"
                   "box = {}\n"
                   "exec(\"X = 1\", box)\n"
                   "print(\"   exec(源码, {}) 里的 __builtins__ =\", type(box[\"__builtins__\"]).__name__,\n"
                   "      \"| len =\", len(box[\"__builtins__\"]))\n"
                   "print(\"   这个键不是脚本自己写的：money.py 里一个字没提它，⑥ 的键表里却有 → \"\n"
                   "      \"执行模块体那一刻由解释器塞进 __dict__\")\n"
                   "print(\"⑧ __pycache__ 里落下的文件 =\", sorted(os.listdir(\"__pycache__\")) if os.path.isdir(\"__pycache__\") else \"（无）\")\n"
                   "print(\"   sys.flags.dont_write_bytecode =\", sys.flags.dont_write_bytecode,\n"
                   "      \"| pyc 的名字跟着实现+版本走，所以它不是可移植的产物\")\n",
    "r5_object.py": "import sys\n"
                    "import money as m\n"
                    "print(\"① 函数就是个属性：getattr(m, 'fee_live')(100) =\", getattr(m, \"fee_live\")(100))\n"
                    "print(\"   getattr(m, 'RATE') =\", getattr(m, \"RATE\"))\n"
                    "print(\"② m.fee_fixed.__defaults__（def 那一刻冻住的） =\", m.fee_fixed.__defaults__)\n"
                    "m.RATE = 0.99\n"
                    "print(\"③ 赋值 m.RATE = 0.99 之后：fee_fixed(100) =\", m.fee_fixed(100),\n"
                    "      \"| fee_live(100) =\", m.fee_live(100))\n"
                    "print(\"   模块内部的 total(100)（它自己调 fee_live） =\", m.total(100))\n"
                    "orig = m.fee_live\n"
                    "m.fee_live = lambda amount, rate=None: 0\n"
                    "print(\"④ 把 m.fee_live 换成假的，total(100) =\", m.total(100), \"← monkeypatch 的全部原理\")\n"
                    "m.fee_live = orig\n"
                    "print(\"   手动还原后 total(100) =\", m.total(100))\n"
                    "other = sys.modules[\"money\"]\n"
                    "print(\"⑤ 赋值就是改 __dict__：m.__dict__['RATE'] =\", m.__dict__[\"RATE\"],\n"
                    "      \"| 另一个引用看到的也是新值 →\", other.RATE, \"| 同一对象 →\", other is m)\n",
    "r6_errors.py": "import sys\n"
                    "print(\"①\", issubclass(ModuleNotFoundError, ImportError),\n"
                    "      \"← ModuleNotFoundError 是 ImportError 的儿子（第 07 章那棵树）\")\n"
                    "try:\n"
                    "    import totally_absent_xyz\n"
                    "except ImportError as exc:\n"
                    "    print(\"②\", type(exc).__name__ + \": \" + str(exc))\n"
                    "    print(\"   exc.name =\", repr(exc.name), \"| exc.path =\", repr(exc.path),\n"
                    "          \"| exc.msg =\", repr(exc.msg))\n"
                    "try:\n"
                    "    import pk.sub.absent_leaf\n"
                    "except ImportError as exc:\n"
                    "    print(\"③\", type(exc).__name__ + \": \" + str(exc), \"| exc.name =\", repr(exc.name))\n"
                    "try:\n"
                    "    from pk import NOT_THERE\n"
                    "except ImportError as exc:\n"
                    "    print(\"④\", type(exc).__name__ + \": \" + str(exc))\n"
                    "    print(\"   exc.name =\", repr(exc.name), \"| exc.path =\", repr(exc.path),\n"
                    "          \"← 注意 name 装的是「从谁那儿导入」，不是缺的那个名字\")\n"
                    "try:\n"
                    "    from money import also_missing\n"
                    "except ImportError as exc:\n"
                    "    print(\"⑤\", type(exc).__name__ + \": \" + str(exc))\n"
                    "src = open(\"relsrc.py\", encoding=\"utf-8\").read()\n"
                    "try:\n"
                    "    exec(compile(src, \"relsrc.py\", \"exec\"), {\"__name__\": \"loose\", \"__file__\": \"relsrc.py\"})\n"
                    "except ImportError as exc:\n"
                    "    print(\"⑥\", type(exc).__name__ + \": \" + str(exc))\n"
                    "ns = {}\n"
                    "try:\n"
                    "    exec(\"from starbad import *\", ns)\n"
                    "except ImportError as exc:\n"
                    "    print(\"⑦ 按 ImportError 接 → 接不到，\", type(exc).__name__)\n"
                    "except Exception as exc:\n"
                    "    print(\"⑦\", type(exc).__name__ + \": \" + str(exc), \"← __all__ 里写了不存在的名字\")\n"
                    "print(\"   这次 exec 之后 ns 里有东西吗 →\", sorted(k for k in ns if not k.startswith(\"__\")),\n"
                    "      \"← 炸之前那半个已经绑上了\")\n"
                    "ns2 = {}\n"
                    "try:\n"
                    "    exec(\"from money import RATE, missing_one\", ns2)\n"
                    "except ImportError as exc:\n"
                    "    print(\"⑧\", type(exc).__name__ + \": \" + str(exc))\n"
                    "    print(\"   多名字的 from 失败后 ns 里留下的是 →\", sorted(k for k in ns2 if not k.startswith(\"__\")))\n",
}


def mask(text):
    """把绝对路径换成相对显示：当前目录整段去掉、家目录换 <家>、解释器前缀换 <py>。"""
    out = str(text)
    out = out.replace(sys.executable, PYNAME)
    for root, tag in ((str(CWD) + os.sep, ""), (str(CWD), "<cwd>"),
                      (HOME + os.sep, "<家>" + os.sep), (HOME, "<家>"),
                      (sys.base_prefix + os.sep, "<py>" + os.sep),
                      (sys.prefix + os.sep, "<py>" + os.sep)):
        if root and root != os.sep:
            out = out.replace(root, tag)
    return out


def kid_env(extra=None):
    """干净 env：不继承 PYTHONPATH / PYTHONHOME，也不给 HOME、PATH。"""
    env = {"LC_ALL": "C.UTF-8", "LANG": "C.UTF-8", "PYTHONIOENCODING": "utf-8"}
    env.update(extra or {})
    return env


def kid(script, extra_env=None):
    """在 app/ 里跑一个子进程：stdout+stderr 合并成一路，保证模块体 print 和报错按真实顺序出现。"""
    done = subprocess.run([sys.executable, script], cwd=str(APP), env=kid_env(extra_env),
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, encoding="utf-8", errors="replace")
    return done


def dump(done, rc_expect=True):
    for line in done.stdout.splitlines():
        print(f"  │ {mask(line)}")
    if rc_expect:
        print(f"  └ 退出码 = {done.returncode}")


shutil.rmtree(WORK, ignore_errors=True)      # 上次的残骸先清掉，每次都是同一个现场
for name, content in FILES.items():
    path = APP / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")

print("=== 1. 模块体只跑一次：跑几遍 import，计数器只涨一格 ===")
print(f"  现场：{WORK.name}/app/once.py —— 模块体第一行就往 run.log 追加一个 R，再 print 一句标记。")
print(f"  子进程 = {PYNAME}（只打 basename）+ cwd=沙箱 + 干净 env；下面 │ 开头的都是它的原始输出")
r = kid("r1_once.py")
dump(r)
log = (APP / "run.log").read_text(encoding="utf-8")
print(f"  跑完计数文件实际内容 = {log!r}（{len(log)} 个 R）")
print("  → 「只跑一次」的开关是 **sys.modules 这本账**，不是文件系统：")
print("    ⑤ 那一步把 sys.modules['once'] 删掉，同一个文件立刻又跑了一遍，⑥ 的 99 也被重置。")
print("  → 判据：`x is sys.modules['名字']` 为真 → 模块是进程内单例，`vars()` 都是同一个 dict。")
print("  ★ 代价：改了 .py 再 import 一次是没用的（进程不重启不生效），只有 reload 能重跑，")
print("    而 reload 不换对象 —— 已经 `from once import VALUE` 拿到旧引用的名字不会跟着更新。")

print("\n=== 2. 换一个进程：同一个文件，模块体又从头跑一遍 ===")
r = kid("r2_newproc.py")
dump(r)
print("  → 所以「一次性」的准确说法是**每个进程一次**，不是「每个文件一次」。")
print("    跨进程共享的必须是别的东西（文件、数据库、环境变量），模块变量不共享。")

print("\n=== 3. import x 与 from x import y：往谁的命名空间里放了什么 ===")
r = kid("r3_bind.py")
dump(r)
print("  → 四种写法各绑一个名字：`import x`→x、`import a.b.c`→只有 a（b、c 挂在父包属性上，")
print("    见第 c 篇）、`import x as y`→只有 y、`from x import y`→只有 y；")
print("    但**不管哪种写法，模块本体都被完整执行并进了 sys.modules**。")
print("  → `del x` 删的是本地那根线：x.y 立刻 NameError，可模块对象还活着（⑥），")
print("    从 sys.modules 里照样能取到值 —— 这也是「重复 import 不会把你的改动冲掉」的原因。")

print("\n=== 4. 模块对象身上的字段都是从哪来的 ===")
r = kid("r4_attrs.py")
dump(r)
print("  → __name__/__doc__/__file__/__cached__/__spec__/__loader__/__builtins__ 全是模块")
print("    __dict__ 里的普通键：__doc__ 取源码第一条语句那个字符串常量（③：注释不顶位置、")
print("    赋值之后的字符串不算），__file__ / __cached__ 由 finder 算出来并写进 __spec__.origin，")
print("    __package__ 是「我的父包叫什么」（②：顶层模块是空串，包里的模块是包名）。")
print("  → 判据：`vars(m) is m.__dict__` → True —— 模块没有任何私有存储，它就是一个")
print("    types.ModuleType 对象 + 一个 dict；`help(m)` 读的也只是 __dict__ 里的 __doc__。")
print("  ★ __builtins__ 这个键的**形状**不保证：本机 3.14.4 实测「被 import 的模块」里是")
print("    dict（而且 `is builtins.__dict__` 为 True），「__main__」里却是 builtins **模块**，")
print("    exec(源码, {}) 那种裸 dict 里又是 dict。所以别写 __builtins__['len']，")
print("    要用就 import builtins 再 builtins.len —— 官方文档明说这个键的取值是未定义的。")

print("\n=== 5. 模块就是对象：getattr 取得到函数，赋值改得动全局状态 ===")
r = kid("r5_object.py")
dump(r)
print("  → ③ 那一行是本篇最该记住的：m.RATE = 0.99 之后，fee_live(100) 从 5.0 变 99.0，")
print("    模块**内部**的 total() 也跟着变 —— 因为它查 RATE 走的是模块全局，也就是 __dict__。")
print("  → 但 ② 那个 __defaults__ 永远是 (0.05,)：默认值在第 05 章说的「def 那一刻」就冻住了，")
print("    改模块属性改的是「运行时现查的全局名」，改不了「定义时就绑好的默认值」。")
print("  → 判据：想让一个值「可被外部调」，函数签名里写 rate=None、函数体里现查 RATE；")
print("    想让一个函数「可被替换」，就在模块里通过全局名调用它（④ 的 lambda 才能生效）。")
print("  ★ 代价：这类改动是进程级全局的，谁 import 谁看到，且不会自动还原 ——")
print("    测试里必须成对地存旧值/还原（或用 monkeypatch 工具替你还原）。")

print("\n=== 6. 四种「导不到」的报错原文（一个字都别改地抄下来）===")
r = kid("r6_errors.py")
dump(r)
print("  → ② 找不到模块 = ModuleNotFoundError: No module named 'xxx'，exc.name 给你那个名字；")
print("    ③ 找的是包里缺的子模块，报错写**全名** 'pk.sub.absent_leaf'，别只盯最后一段；")
print("    ④⑤ 模块存在、名字不存在 = ImportError: cannot import name ... from ...（带文件位置），")
print("    而且 exc.name 装的是「从谁那儿导」，读错方向就会白查半小时。")
print("  → ⑥ 在没有父包的地方写相对导入 → attempted relative import with no known parent package")
print("    （同一个文件改用 `python3 -m pkg.sub` 跑就正常，机制见第 c 篇）；")
print("    ⑦ __all__ 里写了一个不存在的名字 → 炸出来的是 **AttributeError**，不是 ImportError；")
print("  → ⑦⑧ 这两条还有一个共同的代价：import 失败**之前**已经绑上的名字留在原地")
print("    （ns 里剩 ['present'] / ['RATE']），也就是说一个失败的 import 能留下半套状态。")
print("  → 判据：except ImportError 能一网打尽 ①②③④⑤⑥（ModuleNotFoundError 是它的儿子），")
print("    但接不住 ⑦；except Exception 才全接住 —— 而接住之后要像这里一样把原文打出来。")

print("\n=== 7. 收口：import 的一句话账本 ===")
print("  ① `import x` 的第一件事是**查账**：x 在 sys.modules 里就直接用，模块体一行都不跑；")
print("     所以要强制重跑只有两条路：删掉账本里那条（⑤）或显式 reload（⑦，对象不换）。")
print("  ② 没命中才去找文件：找哪、按什么顺序找，是 sys.path 的事（第 b 篇）；")
print("     找到之后建对象、执行模块体、写 __pycache__（⑧ 那个文件名带实现+版本，不可移植）。")
print("  ③ 执行完只做一件对用户可见的事：**往当前命名空间绑一个名字**，绑的是引用；")
print("     `import a.b.c` 只绑 a，`from m import f` 只绑 f，`del` 只断本地这根线（⑥）。")
print("  ④ 模块对象是普通对象：字段都在 __dict__ 里，读它的变量走全局查找 ——")
print("     所以「改模块属性」等于「改它的运行时默认值」（③④ 段），也等于污染全局。")
print("  ⑤ 报错分两族：没这个模块 / 没这个名字 / 没这个父包，全都能用 except ImportError")
print("     接住；只有 `from x import *` 撞上谎报的 __all__ 会给 AttributeError（⑦）。")
print("  ★ 三条代价：模块级代码 = 进程内只跑一次的副作用（不可重放）；")
print("     模块属性是全局状态，改了就全进程可见；sys.modules 是缓存，")
print("     它让「同一份代码在不同时刻 import 到的可能是同一个旧对象」—— 长跑进程改代码不生效，")
print("     热重载框架之所以存在就是为了对付这一条。")

shutil.rmtree(WORK)
left = sorted(p.name for p in WORK.parent.iterdir() if p.is_dir() and p.name.endswith("-demo"))
print(f"\n  收尾自证：{WORK.name}/ 还在吗 → {WORK.exists()} ｜ 当前目录剩下的 *-demo 目录 → {left}"
      f"（章目录零产物，__pycache__ 都跟着沙箱一起没了）")
