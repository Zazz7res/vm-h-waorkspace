#!/usr/bin/env python3
# ============================================================================
# 取证脚本 b_sys_path_shape.py —— 第 09 章第 2 篇：sys.path 是怎么被填出来的
#
# 要证明什么：`import 不到` 九成不是语法问题，是「根在哪」的问题。sys.path 的来源只有四层，
# 每层都能起一个干净子进程单独证明一次：① 入口给的第 0 项（脚本所在目录 / 当前目录 / 空串）、
# ② PYTHONPATH、③ 标准库 + site 加的 dist-packages（`-S` 一关就掉）、④ 内置/冻结
# （它根本不在 sys.path 里，排在所有路径之前，所以同名文件遮蔽不了它）。
#
# 最该记住的两条，本篇现场演：
#   · `python3 x.py` / `-m x` / `-c` / `python3 -` 四种入口往 sys.path[0] 放的**不是同一个东西**，
#     同一份文件换种跑法，能不能 import 到旁边的兄弟模块就跟着变（第 3、5 段）；
#   · `-c` 和 `-` 放进去的是**空串**，空串不在启动时定死，而是每次 import 现查当前目录 ——
#     os.chdir 一下，同一份 sys.path 就换了根（第 4 段）。这也是 `-P`/PYTHONSAFEPATH 存在的理由。
#
# 现场全在本目录的 sys-path-demo/ 里现造现删；屏幕上只打相对路径（当前目录前缀当场替换掉），
# 家目录换 <家>、解释器安装前缀换 <py>，解释器只打 basename。子进程一律
# [sys.executable, ...] + cwd= + env=：干净 env 里 PYTHONPATH / PYTHONHOME / HOME / PATH
# 一个都不继承，需要的开关按行一个个加回去（否则「这一层是谁加的」就证不干净）。
# 退出码恒为 0；跑两遍输出逐字一致（无时间、无 PID、无绝对路径、无对象地址）。
# 本机 Python 3.14.4 / Ubuntu 26.04。3.8 的行为（隐式相对导入等）与 Windows 的路径语义
# 本机验不了 → 一律标「未验证」，别当事实引用。
# ============================================================================
import os
import shutil
import subprocess
import sys
from pathlib import Path

WORK = Path("sys-path-demo")
PYNAME = os.path.basename(sys.executable)
CWD = Path.cwd().resolve()
HOME = os.path.expanduser("~")
STDLIB = os.path.dirname(os.__file__)

JSON_CHECK = "import json\n" \
    "print(\"FILE=\" + json.__file__)\n" \
    "print(\"WHO=\" + getattr(json, \"FAKE_JSON\", \"（这是标准库那个 json）\"))\n" \
    "print(\"IS_PACKAGE=\" + str(hasattr(json, \"__path__\")))\n" \
    "print(\"HAS_DUMP=\" + str(hasattr(json, \"dumps\")))\n" \
    "try:\n" \
    "    json.dumps({\"a\": 1})\n" \
    "except Exception as exc:\n" \
    "    print(\"DUMPS=\" + type(exc).__name__ + \": \" + str(exc))\n" \
    "try:\n" \
    "    import json.tool\n" \
    "except Exception as exc:\n" \
    "    print(\"SUB=\" + type(exc).__name__ + \": \" + str(exc))\n"

FILES = {
    # ---- ① 入口那一层：sroot/ 里一个入口脚本 + 一个兄弟模块 ----
    "sroot/neighbor.py": 'NB = "sroot 里的邻居模块"\n',
    "sroot/entry.py": "import sys\n"
                      "print(\"PATH0=\" + sys.path[0])\n"
                      "print(\"ARGV0=\" + sys.argv[0])\n"
                      "print(\"PKG=\" + repr(__package__))\n"
                      "try:\n"
                      "    import neighbor\n"
                      "    print(\"SIBLING=\" + neighbor.NB)\n"
                      "except ModuleNotFoundError as exc:\n"
                      "    print(\"SIBLING=\" + type(exc).__name__ + \": \" + str(exc))\n"
                      "try:\n"
                      "    import rootmod\n"
                      "    print(\"OUTSIDE=\" + rootmod.TAG)\n"
                      "except ModuleNotFoundError as exc:\n"
                      "    print(\"OUTSIDE=\" + type(exc).__name__ + \": \" + str(exc))\n",
    "sroot/entryfix.py": "import sys\n"
                         "print(\"PATH0=\" + sys.path[0])\n"
                         "print(\"PKG=\" + repr(__package__))\n"
                         "import sroot.neighbor\n"
                         "from . import neighbor\n"
                         "print(\"FULL=\" + sroot.neighbor.NB)\n"
                         "print(\"DOT=\" + neighbor.NB)\n",
    # ---- ② PYTHONPATH 那一层：两个同名模块，看谁赢 ----
    "A/rootmod.py": 'TAG = "A/rootmod.py 那一份"\n',
    "B/rootmod.py": 'TAG = "B/rootmod.py 那一份"\n',
    "pp.py": "import sys\n"
             "print(\"TOP=\" + \" | \".join(repr(p) for p in sys.path[:4]))\n"
             "try:\n"
             "    import rootmod\n"
             "    print(\"TAG=\" + rootmod.TAG)\n"
             "    print(\"FILE=\" + rootmod.__file__)\n"
             "except ModuleNotFoundError as exc:\n"
             "    print(\"TAG=\" + type(exc).__name__ + \": \" + str(exc))\n"
             "    print(\"FILE=（没导到）\")\n",
    # ---- ③ 标准库 + site 那一层 ----
    "siteprobe.py": "import sys\n"
                    "print(\"SITE=\" + str(\"site\" in sys.modules))\n"
                    "print(\"N=\" + str(len(sys.path)))\n"
                    "for p in sys.path:\n"
                    "    print(\"P=\" + p)\n"
                    "def tag(f):\n"
                    "    return f.__name__ if isinstance(f, type) else type(f).__name__\n"
                    "print(\"META=\" + \",\".join(tag(m) for m in sys.meta_path))\n",
    # ---- ④ 内置/冻结那一层：造个假 time 放进 PYTHONPATH ----
    "shbid/time.py": 'FAKE_TIME = "假 time 模块，想冒充内置的那一个"\n',
    "bprobe.py": "import os\n"
                 "import sys\n"
                 "import time\n"
                 "print(\"IN_BUILTIN=\" + str(\"time\" in sys.builtin_module_names))\n"
                 "print(\"BUILTIN_N=\" + str(len(sys.builtin_module_names)) + \" 个，样例 \"\n"
                 "      + \",\".join(sorted(sys.builtin_module_names)[:6]))\n"
                 "print(\"FAKE_WON=\" + str(hasattr(time, \"FAKE_TIME\")))\n"
                 "print(\"TIME_ORIGIN=\" + str(time.__spec__.origin))\n"
                 "try:\n"
                 "    print(\"TIME_FILE=\" + repr(time.__file__))\n"
                 "except AttributeError as exc:\n"
                 "    print(\"TIME_FILE=\" + type(exc).__name__ + \": \" + str(exc))\n"
                 "zips = [p for p in sys.path if p.endswith('.zip')]\n"
                 "print(\"ZIP=\" + str(len(zips)) + \" 项 zip，磁盘上真存在的 \" + str(len([p for p in zips if os.path.exists(p)])) + \" 项\")\n"
                 "import _frozen_importlib as fi\n"
                 "print(\"FROZEN=\" + str(fi.__spec__.origin) + \" | 它的 __name__ = \" + fi.__name__ +\n"
                 "      \" | 有 __file__ 吗 = \" + str(hasattr(fi, \"__file__\")))\n",
    # ---- 遮蔽那一层：同一份假 json，两种放进去的方式 ----
    "shad/json.py": 'FAKE_JSON = "沙箱里的假 json：它没有 dumps"\n',
    "shad/peek.py": JSON_CHECK,
    "jsoncheck.py": JSON_CHECK,
    # ---- 空串那一层（运行时才解析）----
    "run/hid.py": 'HID = "藏在 run/ 里的模块"\n',
    "run/sib.py": 'SIB = "run/ 里的兄弟模块"\n',
    "run/probe_abs.py": "import os\n"
                        "import sys\n"
                        "print(\"PATH0=\" + sys.path[0])\n"
                        "import sib\n"
                        "print(\"① 刚打开就能 import sib → \" + sib.SIB)\n"
                        "os.chdir(\"..\")\n"
                        "print(\"② os.chdir 到上一层：cwd == sys.path[0] 吗 → \"\n"
                        "      + str(os.getcwd() == sys.path[0]) + \" ｜ sys.path[0] 仍是 \" + sys.path[0])\n"
                        "del sys.modules[\"sib\"]\n"
                        "try:\n"
                        "    import sib\n"
                        "    print(\"③ 清掉 sys.modules 再 import → \" + sib.SIB)\n"
                        "except ModuleNotFoundError as exc:\n"
                        "    print(\"③ 清掉 sys.modules 再 import → \" + type(exc).__name__ + \": \" + str(exc))\n",
    # ---- 最狠的一档：遮蔽启动阶段就要用的模块 ----
    "boom/encodings.py": 'raise RuntimeError("沙箱里的假 encodings 模块被执行了")\n',
    # ---- sys.path 是普通 list 的现场演示 ----
    "mutate.py": "import sys\n"
                 "print(\"起点：类型 =\", type(sys.path).__name__, \"| 项数 =\", len(sys.path))\n"
                 "sys.path.append(\"A\")\n"
                 "import rootmod\n"
                 "print(\"① append 一个相对目录之后 import rootmod →\", rootmod.TAG)\n"
                 "print(\"   rootmod.__file__ =\", rootmod.__file__)\n"
                 "sys.modules.pop(\"rootmod\", None)\n"
                 "sys.path.remove(\"A\")\n"
                 "try:\n"
                 "    import rootmod\n"
                 "except ModuleNotFoundError as exc:\n"
                 "    print(\"② 把它移出 sys.path 并清掉 sys.modules →\", type(exc).__name__ + \":\", str(exc))\n"
                 "sys.modules.pop(\"rootmod\", None)\n"
                 "sys.path.insert(0, \"B\")\n"
                 "import rootmod\n"
                 "print(\"③ insert(0, 'B')（B 排在 A 前无所谓，A 已经移出去了）→\", rootmod.TAG)\n"
                 "sys.modules.pop(\"rootmod\", None)\n"
                 "sys.path.insert(0, \"A\")\n"
                 "import rootmod\n"
                 "print(\"④ 再 insert(0, 'A') →\", rootmod.TAG, \"← 谁在前谁赢\")\n",
}

CHDIR_CODE = (
    "import os\n"
    "import sys\n"
    "print('起点 sys.path[0] =', repr(sys.path[0]), '| 当前目录 =', os.path.basename(os.getcwd()))\n"
    "try:\n"
    "    import hid\n"
    "    print('① 在这个目录里 import hid → 居然成功了？', hid.__file__)\n"
    "except ModuleNotFoundError as exc:\n"
    "    print('① 在这个目录里 import hid →', type(exc).__name__ + ':', str(exc))\n"
    "os.chdir('run')\n"
    "print('② 只 os.chdir(\"run\")，sys.path 一个字没改：', repr(sys.path[0]), '| 目录 =', os.path.basename(os.getcwd()))\n"
    "import hid\n"
    "print('   同一个名字再导一次就成功了 →', hid.HID, '| 文件 =', hid.__file__)\n"
    "del sys.modules['hid']\n"
    "os.chdir('..')\n"
    "try:\n"
    "    import hid\n"
    "    print('③ chdir 回来 + 清掉缓存 → 又成功了？', hid.__file__)\n"
    "except ModuleNotFoundError as exc:\n"
    "    print('③ chdir 回来 + 清掉缓存 →', type(exc).__name__ + ':', str(exc))\n"
)


def mask(text):
    """绝对路径 → 能读的形状：当前目录整段去掉、家目录换 <家>、解释器前缀换 <py>。"""
    out = str(text)
    out = out.replace(sys.executable, PYNAME)
    for root, tag in ((str(CWD) + os.sep, ""), (str(CWD), "<cwd>"),
                      (HOME + os.sep, "<家>" + os.sep), (HOME, "<家>"),
                      (sys.base_prefix + os.sep, "<py>" + os.sep),
                      (sys.prefix + os.sep, "<py>" + os.sep),
                      (STDLIB + os.sep, "<stdlib>" + os.sep)):
        if root and root != os.sep:
            out = out.replace(root, tag)
    return out


def show(entry):
    """sys.path 的一项 → 屏幕上的一项。"""
    if entry == "":
        return "''（空串）"
    tail = "/" if entry and os.path.isdir(entry) else ""
    return mask(entry) + tail


def label(entry):
    """这一项是哪一层给的 —— 认不出来就打「其他」，不硬编故事。"""
    if entry == "":
        return "① 入口给的空串 = 当前目录（每次 import 才解析）"
    abs_entry = os.path.normpath(os.path.abspath(entry))
    sandbox = {os.path.normpath(str(CWD / WORK)): "① 入口给的：脚本所在目录 / 当前目录（绝对路径）",
               os.path.normpath(str(CWD / WORK / "sroot")): "① 入口给的：脚本所在目录（绝对路径）",
               os.path.normpath(str(CWD / WORK / "A")): "② PYTHONPATH 加的",
               os.path.normpath(str(CWD / WORK / "B")): "② PYTHONPATH 加的"}
    if abs_entry in sandbox:
        return sandbox[abs_entry]
    if entry.endswith(".zip"):
        return "③ zip 位：标准库的打包入口（磁盘上根本没有也照样列着）"
    if entry.endswith("lib-dynload"):
        return "③ 标准库的 C 扩展目录（.so）"
    if "packages" in entry:
        return "③+ 这一项是 site 加的（下面第 2 段用 -S 单独证）"
    if abs_entry == os.path.normpath(STDLIB):
        return "③ 标准库 .py（安装时定好的默认项）"
    return "其他"


def kid_env(extra=None):
    env = {"LC_ALL": "C.UTF-8", "LANG": "C.UTF-8", "PYTHONIOENCODING": "utf-8"}
    env.update(extra or {})
    return {k: v for k, v in env.items() if v is not None}


def kid(args, cwd=WORK, extra_env=None, code_stdin=None):
    """起干净子进程；stdout+stderr 合成一路，报错原文按真实顺序出现。"""
    return subprocess.run([sys.executable, *args], cwd=str(cwd), env=kid_env(extra_env),
                          input=code_stdin, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, encoding="utf-8", errors="replace")


def dump(done):
    for line in done.stdout.splitlines():
        print(f"  │ {mask(line)}")
    print(f"  └ 退出码 = {done.returncode}")


def fields(done):
    """把子进程里 KEY=value 的行收成 dict。"""
    out = {}
    for line in done.stdout.splitlines():
        if "=" in line and line.split("=", 1)[0].isupper():
            out[line.split("=", 1)[0]] = line.split("=", 1)[1]
    return out


shutil.rmtree(WORK, ignore_errors=True)
for name, content in FILES.items():
    path = WORK / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")

print("=== 1. 先认清对象：sys.path 是一个能被现场改写的普通 list ===")
print(f"  本机 Python {sys.version.split()[0]}（只打版本号）｜子进程解释器 = {PYNAME}（只打 basename）。")
normal = kid(["siteprobe.py"])
for entry in [ln[2:] for ln in normal.stdout.splitlines() if ln.startswith("P=")]:
    print(f"  · {show(entry):<40} ← {label(entry)}")
print(f"  一共 {fields(normal).get('N')} 项，全是字符串；顺序就是查找顺序，**前面的赢**。")
print("  注意这张列表里**找不到第 ④ 层**（内置/冻结）：它不在 sys.path 里，第 2 段单独演。")
dump(kid(["mutate.py"]))
print("  → 判据：① append 进去的目录立刻能 import；② 移出去 + 清 sys.modules 就导不到；")
print("    ④ 同名模块谁排在前面谁赢 —— 顺序 = 优先级，而且这份顺序在你手里（但只在当前进程）。")
print("  ★ 代价：运行时改 sys.path 是全局副作用，之后每一次 import 都被它影响；")
print("    子进程**不继承**这件事（第 4 段那种干净 env 尤其），所以「我这能跑」不能跨进程外推。")

print("\n=== 2. 四层来源逐个点名（每层一个干净子进程）===")
print("  ① 入口给的第 0 项：cwd=sys-path-demo/，跑 sroot/entry.py")
dump(kid(["sroot/entry.py"]))
print("  ② PYTHONPATH：同一份 pp.py，只换 env 里这一项（相对值也认，按当前目录解析）")
for desc, extra in (("PYTHONPATH=A:B", {"PYTHONPATH": "A:B"}),
                    ("PYTHONPATH=B:A", {"PYTHONPATH": "B:A"}),
                    ("PYTHONPATH=.（当前目录）", {"PYTHONPATH": "."}),
                    ("PYTHONPATH=A:A（同一项写两遍）", {"PYTHONPATH": "A:A"})):
    got = fields(kid(["pp.py"], extra_env=extra))
    print(f"    · {desc:<30} 前 4 项 = {mask(got.get('TOP', '读取失败'))}")
    print(f"      {'':<30} 赢的是 {mask(got.get('TAG', '读取失败'))} ｜ 文件 = {mask(got.get('FILE', '读取失败'))}")
print("      → PYTHONPATH 的项插在**第 0 项之后、标准库之前**（TOP= 那行的位置就是证据），")
print("        顺序照抄你写的顺序、第一项赢（A:B 和 B:A 两行换的就是这个赢家）。")
print("      → 两条边角实测：同一个目录写两遍只留一份（第四行）；但 `.` 会被展开成当前目录的")
print("        绝对路径，和第 0 项**一模一样地重复**（第三行两项相同）—— 去重只发生在 PYTHONPATH")
print("        自己内部，跨层不去重；那一行没 rootmod.py 可拿，TAG 就原样报 ModuleNotFoundError。")
print("  ③ site 那一层：同一个探针，加与不加 -S")
no_site = kid(["-S", "siteprobe.py"])
np, qp = [ln[2:] for ln in normal.stdout.splitlines() if ln.startswith("P=")], \
         [ln[2:] for ln in no_site.stdout.splitlines() if ln.startswith("P=")]
print(f"    · 正常：{len(np)} 项 ｜ 'site' in sys.modules = {fields(normal).get('SITE')}")
print(f"    · 加 -S：{len(qp)} 项 ｜ 'site' in sys.modules = {fields(no_site).get('SITE')}")
for dropped in [p for p in np if p not in qp]:
    print(f"      -S 一关就少掉：{show(dropped)}")
print(f"    · sys.meta_path 正常 = {fields(normal).get('META')}")
print(f"      sys.meta_path -S   = {fields(no_site).get('META')}")
print("      → site 这一层不只是往 sys.path 里加目录：本机 dist-packages 里有个 .pth 文件，")
print("        它还往 sys.meta_path 里塞了一个 finder（上面多出来的那个名字）。第三方能改查找过程本身。")
print("  ④ 内置/冻结那一层：造假 time.py 放进 PYTHONPATH，看它能不能冒充内置的 time")
dump(kid(["bprobe.py"], extra_env={"PYTHONPATH": "shbid"}))
print("      → 假文件就躺在 sys.path[1]，依旧无效：内置层排在 sys.path **之前**，")
print("        `x in sys.builtin_module_names` 为 True 的名字遮蔽不了；内置模块连 __file__ 都没有。")

print("\n=== 3. 四种入口往 sys.path[0] 里放的不是同一个东西（实测表）===")
src = (WORK / "sroot" / "entry.py").read_text(encoding="utf-8")
runs = [
    ("python3 sroot/entry.py", ["sroot/entry.py"], WORK, None, None),
    ("python3 -m sroot.entry", ["-m", "sroot.entry"], WORK, None, None),
    ("python3 -c <同一段代码>", ["-c", src], WORK, None, None),
    ("python3 - < sroot/entry.py", ["-"], WORK, src, None),
    ("python3 entry.py（cwd 换成 sroot/）", ["entry.py"], WORK / "sroot", None, None),
    ("python3 -P sroot/entry.py", ["-P", "sroot/entry.py"], WORK, None, None),
    ("PYTHONSAFEPATH=1 python3 sroot/entry.py", ["sroot/entry.py"], WORK, None,
     {"PYTHONSAFEPATH": "1"}),
]
print("  入口                                      sys.path[0]（掩码后）      argv[0]                     __package__  import neighbor")
for name, args, cwd, stdin, extra in runs:
    got = fields(kid(args, cwd=cwd, code_stdin=stdin, extra_env=extra))
    sib = mask(got.get("SIBLING", "?"))
    print(f"  · {name:<39} {show(got.get('PATH0', '')):<24} {mask(got.get('ARGV0', '?')):<25} "
          f"{got.get('PKG', '?'):<12} {'★ ' + sib if sib.startswith('Module') else '→ ' + sib}")
done = kid(["-P", "-m", "sroot.entryfix"])
print(f"  · python3 -P -m sroot.entryfix → 退出码 {done.returncode}，当前目录整个不给了：")
for line in done.stdout.splitlines():
    print(f"    │ {mask(line)}")
print("  → 判据：脚本跑法放的是**脚本所在目录的绝对路径**（第 5 行换了 cwd 也一样，跟着文件走）；")
print("    -m 放的是**当前目录的绝对路径**（所以包名要写全，旁边的光板模块反而看不见）；")
print("    -c 和 `-`（管道喂代码）放的是**空串**（跟着进程当前目录走，见第 4 段）。")
print("    `-P` / PYTHONSAFEPATH=1（3.11+）就是把这一项整个删掉，第 6、7 行是它俩的下场。")

print("\n=== 4. 空串不是「启动时定死的当前目录」：chdir 之后它就换根 ===")
dump(kid(["-c", CHDIR_CODE]))
dump(kid(["run/probe_abs.py"]))
print("  → 上面那段（-c 入口）：同一个进程、同一份 sys.path（[0] 一直是 ''），只 os.chdir 了一下，")
print("    原本 ModuleNotFoundError 的名字就导得到了；chdir 回来 + 清掉 sys.modules，它又炸。")
print("    → 空串是在**每次 import 的那一刻**拿 os.getcwd() 去解析的。")
print("  → 下面那段（脚本跑法）：sys.path[0] 是绝对目录，chdir 到上一层之后清掉缓存照样 import 成功")
print("    —— 因为根定在**文件位置**上，跟当前目录无关。")
print("  ★ 代价：依赖 `-c`/管道 + 空串 = 「同一份代码在不同目录下行为不同」。IDE 里「运行选中的代码」")
print("    忽好忽坏、CI 换个 workspace 就 ModuleNotFoundError，都是这一条。要么用 -m（写全包名），")
print("    要么 -P/PYTHONSAFEPATH=1 把空串整个关掉。")

print("\n=== 5. 同一份文件，两种跑法：兄弟模块一个拿得到、一个拿不到 ===")
print("  磁盘上只有三个文件：sroot/entry.py、sroot/neighbor.py、A/rootmod.py（A/ 不在任何路径里）。")
for name, args in (("python3 sroot/entry.py", ["sroot/entry.py"]),
                   ("python3 -m sroot.entry", ["-m", "sroot.entry"])):
    got = fields(kid(args))
    print(f"  · {name:<26} 根 = {show(got.get('PATH0', ''))}")
    print(f"    {'':<26} import neighbor → {mask(got.get('SIBLING', '?'))}")
    print(f"    {'':<26} import rootmod  → {mask(got.get('OUTSIDE', '?'))}")
done = kid(["-m", "sroot.entryfix"])
print(f"  · {'python3 -m sroot.entryfix':<26} 换成两种「写全」的写法，退出码 {done.returncode}：")
for line in done.stdout.splitlines():
    print(f"    │ {mask(line)}")
print("  → 判据：-m 报错的是「No module named 'neighbor'」这种**光板名字**，不是「找不到 sroot 这个包」")
print("    —— 说明当前目录已经是根了，只是根里没有叫 neighbor 的文件。")
print("    两种修法当场都跑通了：`import sroot.neighbor`（写全包名，两种跑法都行，但脚本跑法要根对）")
print("    和 `from . import neighbor`（相对导入，只有当模块是「作为包的一部分被导入」时才可用，见第 c 篇）。")

print("\n=== 6. ★ 同名遮蔽：一份 json.py 抢走标准库，两种抢法、三层下场 ===")
got = fields(kid(["-c", "import json; print('FILE=' + json.__file__); print('IS_PACKAGE=' + str(hasattr(json, '__path__'))); print('WHO=' + getattr(json, 'FAKE_JSON', '（标准库那个）'))"]))
print(f"  基线（干净 env，什么都不加）：json.__file__ = {mask(got.get('FILE', '?'))} ｜ "
      f"它是包吗 → {got.get('IS_PACKAGE')}")
print("  · 抢法一（② 那一层）：PYTHONPATH=shad，入口脚本 jsoncheck.py 和假 json **不在**同一个目录")
dump(kid(["jsoncheck.py"], extra_env={"PYTHONPATH": "shad"}))
print("  · 抢法二（① 那一层）：假 json.py 一个字没挪，只是把入口脚本放进**同一个目录**：python3 peek.py")
dump(kid(["peek.py"], cwd=WORK / "shad"))
print("  → 两条路都拿到了假 json。抢法二是日常事故的原样：你把自己的脚本命名成 json.py / csv.py，")
print("    它所在目录就是 sys.path[0]，同目录里其它脚本 import 这个名字时全被它截走（连 PYTHONPATH 都不用）。")
print("  → 连锁原文（上面两次输出里）：AttributeError: module 'json' has no attribute 'dumps'，")
print("    以及 ModuleNotFoundError: No module named 'json.tool'; 'json' is not a package ——")
print("    假的那个是**模块**不是**包**（没有 __path__），所以真 json 的子模块也一起没了。")
print("  → 注意抢法二那条 AttributeError 后面跟了一长串括号：3.12 起解释器会**主动提醒**")
print("    「consider renaming … since it has the same name as the standard library module」，")
print("    条件是被抢的那个文件躺在**主脚本所在的目录**里。抢法一（PYTHONPATH）没这个提醒，更难查。")
done = kid(["-m", "json.tool"], extra_env={"PYTHONPATH": "shad"})
print(f"  · 连标准库自己的入口都换了下场：python3 -m json.tool 退出码 {done.returncode}")
for line in done.stdout.splitlines():
    print(f"    │ {mask(line)}")
print("  · 最狠的一档：遮蔽**解释器启动阶段**就要用的模块（boom/encodings.py 里只有一句 raise）")
done = kid(["-c", "print('这一行永远跑不到')"], extra_env={"PYTHONPATH": "boom"})
print(f"    python3 -c 的退出码 = {done.returncode}（开机就炸，你的代码一个字都没执行）：")
for line in done.stdout.splitlines()[-2:]:
    print(f"    │ {mask(line)}")
print("  ★ 三条判据：① 出「module 'xxx' has no attribute ...」这种怪错，第一件事是打 `xxx.__file__`")
print("      看它到底是谁；② 目录名和脚本名一律别和标准库重名（json、csv、random、select、site、")
print("      codeop、platform…… 一秒自查：python3 -c \"import 名字; print(名字.__file__)\"）；")
print("    ③ 内置/冻结那一层遮蔽不动（第 2 段④），别在「覆盖 sys」这件事上耗时间。")

print("\n=== 7. 收口：import 找不到东西时按这张表逐层问 ===")
print("  层     问这一句                                       当场一行                            本篇证据")
rows = [
    ("0", "它是不是早就在 sys.modules 里（改代码不生效）", "print(x.__file__)", "第 a 篇 1、2 段"),
    ("1", "根在哪：入口方式决定 sys.path[0]", "python3 -c \"import sys;print(sys.path[0])\"", "第 3 段七行表"),
    ("1b", "这个根是不是空串（换目录就换根）", "os.chdir 后再 import 试一次", "第 4 段"),
    ("2", "PYTHONPATH 有没有指过去、排在第几", "print(sys.path[1:4])", "第 2 段②"),
    ("3", "site 层加没加（venv / -S / 用户站点目录）", "python3 -S -c \"import sys;print(len(sys.path))\"", "第 2 段③"),
    ("3b", "有没有同名文件把标准库抢了", "print(模块.__file__) 对比 <stdlib>/", "第 6 段"),
    ("4", "它是不是内置/冻结（遮蔽不了）", "'time' in sys.builtin_module_names", "第 2 段④"),
    ("5", "是包还是散目录（__init__.py 有没有）", "见第 c 篇", "第 c 篇"),
]
for num, question, cmd, proof in rows:
    print(f"  {num:<6} {question:<40} {cmd:<37} {proof}")
print("  → 三条判据：① sys.path 的来源就这四层，第 0 项由**入口方式**决定、其余由**环境**决定；")
print("    ② 顺序 = 优先级，所以「找不到」和「找错」是同一张表的两面，查的时候按顺序问；")
print("    ③ 空串是把当前目录在 import 那一刻拿过来用 —— 不想要它就用 `-P` 或 PYTHONSAFEPATH=1。")
print("  ★ 代价：这张表是**解释器启动时**算好的，进程里 append/insert 只影响当前进程；")
print("    PYTHONPATH 却能穿透到子进程 —— 所以本脚本每个子进程都发一份干净 env，")
print("    否则你看到的「哪一层给的」全是上一行命令留下的幻觉。")

shutil.rmtree(WORK)
print(f"\n  收尾自证：{WORK.name}/ 还在吗 → {WORK.exists()} ｜ 当前目录剩下的目录 → "
      f"{sorted(p.name for p in Path('.').iterdir() if p.is_dir())}（章目录零产物，__pycache__ 跟着沙箱一起没了）")
