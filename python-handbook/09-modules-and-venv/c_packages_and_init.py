#!/usr/bin/env python3
# ============================================================================
# 取证脚本 c_packages_and_init.py —— 第 09 章第 3 篇：包、__init__.py、相对导入、命名空间包
#
# 要证明什么：「包」不是抽象概念，它就是**一个带 __init__.py 的目录**，而 __init__.py 就是
# 这个包的模块体（第 a 篇讲的一切对它都成立）。本篇把四件事演穿：
#   ① `import pkg.sub.mod` 会**从最外层往里**依次执行 __init__.py（每层印一行，顺序直接上屏幕）；
#   ② 「子模块被导入」的物理表现是**父包对象上多了一个属性**，所以 import 父包不带子模块，
#      `from pkg import sub` 也不带 sub.mod；
#   ③ 没有 __init__.py 的目录也能 import（PEP 420，3.3+）：__file__ 是 None、__path__ 是
#      _NamespacePath，而且**多个根里的同名目录会被合成同一个包**（隐患），反过来一个
#      __init__.py 就能把其它根整个切断；
#   ④ 相对导入的 `.`/`..` 只看 __package__ / __spec__.parent 这两个**名字**，不看文件位置 ——
#      所以同一个文件 `python3 pkgr/modr.py` 炸、`python3 -m pkgr.modr` 成。
#
# 现场全在本目录的 pkg-init-demo/ 里现造现删；屏幕上只打相对路径（当前目录前缀当场替换），
# 解释器只打 basename。子进程一律 [sys.executable, ...] + cwd= + env=（干净 env，
# PYTHONPATH/PYTHONHOME 不继承，需要的开关按行加回去）。退出码恒为 0；跑两遍输出逐字一致。
# 本机 Python 3.14.4 / Ubuntu 26.04。3.8 及更早的隐式相对导入规则、Windows 的行为本机验不了
# → 一律标「未验证」，别当事实引用。
# ============================================================================
import os
import shutil
import subprocess
import sys
from pathlib import Path

WORK = Path("pkg-init-demo")
PYNAME = os.path.basename(sys.executable)
CWD = Path.cwd().resolve()
HOME = os.path.expanduser("~")

FILES = {
    # ---- ① 三层包：每层 __init__ 印一行 ----
    "ord/pkg/__init__.py": 'print("[INIT] pkg/__init__.py 被执行")\nPKG_MARK = "pkg 的标记"\n',
    "ord/pkg/sub/__init__.py": 'print("[INIT] pkg/sub/__init__.py 被执行")\nSUB_MARK = "sub 的标记"\n',
    "ord/pkg/sub/mod.py": 'print("[BODY] pkg/sub/mod.py 被执行")\nMOD_MARK = "mod 的标记"\n',
    "ord/pkg/one.py": 'ONE = "pkg/one.py"\n',
    # ---- ② 父包属性 ----
    "bind/pkgb/__init__.py": 'from . import subb2\nAPI = "pkgb 的 __init__ 顺手 import 了子模块"\n',
    "bind/pkgb/subb2.py": 'X = "subb2 的 X"\n',
    # ---- ③ __all__ 对照：无 / 有 / 谎报 ----
    "star/plainpkg/__init__.py": 'PUBLIC = "plain-PUBLIC"\n_HIDDEN = "plain-下划线"\n\n\nclass Thing:\n    pass\n',
    "star/plainpkg/suba.py": 'SUBA = "plainpkg 的子模块 suba"\n',
    "star/alledpkg/__init__.py": '__all__ = ["PUBLIC", "fn", "subb"]\n'
                                 'PUBLIC = "alled-PUBLIC"\n_HIDDEN = "alled-下划线"\n\n\n'
                                 'def fn():\n    return "alled-fn"\n',
    "star/alledpkg/subb.py": 'SUBB = "alledpkg 的子模块 subb（写在 __all__ 里）"\n',
    "star/alledpkg/subc.py": 'SUBC = "alledpkg 的子模块 subc（没写进 __all__）"\n',
    "star/ghostpkg/__init__.py": '__all__ = ["ghost"]\npresent = 1\n',
    # ---- ④ 命名空间包：三个根，同一个包名 merged ----
    "ns/root1/merged/leaf.py": 'WHERE = "root1 的 merged/leaf.py"\n',
    "ns/root1/merged/extra.py": 'EXTRA = "root1 独有的 merged/extra"\n',
    "ns/root2/merged/leaf.py": 'WHERE = "root2 的 merged/leaf.py（和 root1 同名）"\n',
    "ns/root2/merged/only2.py": 'ONLY2 = "root2 独有的 merged/only2"\n',
    "ns/root3/merged/__init__.py": 'print("[INIT] root3 的 merged/__init__.py —— 有它就不是命名空间包了")\n',
    "ns/root3/merged/leaf.py": 'WHERE = "root3 的 merged/leaf.py"\n',
    # ---- ⑤⑥ 相对导入与 -m 的入口 ----
    "rel/pkgr/__init__.py": 'print("[INIT] pkgr/__init__.py")\nTOP_TAG = "pkgr 顶层的标记"\n',
    "rel/pkgr/helper.py": 'TAG = "pkgr/helper.py"\n',
    "rel/pkgr/modr.py": "print(\"__name__ =\", __name__)\n"
                        "print(\"__package__ =\", repr(globals().get(\"__package__\")))\n"
                        "_sp = globals().get(\"__spec__\")\n"
                        "print(\"__spec__ 这个键在吗 =\", \"__spec__\" in globals(),\n"
                        "      \"| 它的值是 None 吗 =\", _sp is None,\n"
                        "      \"| parent =\", repr(getattr(_sp, \"parent\", \"（没有可问的 __spec__）\")))\n"
                        "from . import helper\n"
                        "print(\"相对导入 . 成功 →\", helper.TAG)\n",
    "rel/pkgr/sub/__init__.py": 'print("[INIT] pkgr/sub/__init__.py")\n',
    "rel/pkgr/sub/sibling.py": 'TAG2 = "pkgr/sub/sibling.py"\n',
    "rel/pkgr/sub/up.py": "from .. import helper\n"
                          "from . import sibling\n"
                          "print(\"__package__ =\", repr(__package__), \"| __spec__.parent =\", repr(__spec__.parent))\n"
                          "print(\".. 找到的是 →\", helper.TAG)\n"
                          "print(\".  找到的是 →\", sibling.TAG2)\n",
    "rel/pkgr/sub/over.py": "from ... import os\nprint('永远跑不到')\n",
}

MAIN_SRC = (
    "from . import helper\n"
    "print(\"__main__.py 被 -m 跑起来了：__name__ =\", __name__,\n"
    "      \"| __package__ =\", repr(__package__))\n"
    "print(\"        __spec__.name =\", repr(__spec__.name),\n"
    "      \"| __spec__.parent =\", repr(__spec__.parent))\n"
    "print(\"在 __main__.py 里相对导入照样可用 →\", helper.TAG)\n"
)

# 每行入口自己 import，这里只负责「import 完之后 sys.modules 里多了哪几个键」
ORD_CODE = ("print('  sys.modules 里 pkg 开头的键 →', "
            "sorted(m for m in __import__('sys').modules if m.startswith('pkg')))\n")

STAR_CODE = (
    "import sys\n"
    "ns = {}\n"
    "exec('from @PKG@ import *', ns)\n"
    "print('@TAG@: from @PKG@ import * → 绑到本地的名字', sorted(k for k in ns if not k.startswith('__')))\n"
    "print('        sys.modules 里 @PKG@. 开头的键 →', "
    "sorted(m for m in sys.modules if m.startswith('@PKG@.')))\n"
)

NS_CODE = (
    "import merged\n"
    "print('type →', type(merged).__name__, '| repr →', repr(merged))\n"
    "print('有 __file__ 这个键吗 →', hasattr(merged, '__file__'), '| merged.__file__ =', repr(merged.__file__))\n"
    "print('__path__ 类型 →', type(merged.__path__).__name__, '| 几项 →', len(list(merged.__path__)))\n"
    "print('__path__ 内容 →', sorted(merged.__path__))\n"
    "print('__spec__.origin →', repr(merged.__spec__.origin))\n"
    "ns = {}\n"
    "exec('from merged import *', ns)\n"
    "print('from merged import * → 绑到的名字', sorted(k for k in ns if not k.startswith('__')))\n"
    "import merged.leaf\n"
    "print('merged.leaf.WHERE →', merged.leaf.WHERE)\n"
    "for name, attr in (('merged.extra', 'EXTRA'), ('merged.only2', 'ONLY2')):\n"
    "    try:\n"
    "        mod = __import__(name, fromlist=['_'])\n"
    "        print(name, '→', getattr(mod, attr))\n"
    "    except Exception as exc:\n"
    "        print(name, '→', type(exc).__name__ + ':', exc)\n"
)


def mask(text):
    """绝对路径 → 能读的形状。"""
    out = str(text)
    out = out.replace(sys.executable, PYNAME)
    for root, tag in ((str(CWD) + os.sep, ""), (str(CWD), "<cwd>"),
                      (HOME + os.sep, "<家>" + os.sep), (HOME, "<家>"),
                      (sys.base_prefix + os.sep, "<py>" + os.sep),
                      (sys.prefix + os.sep, "<py>" + os.sep)):
        if root and root != os.sep:
            out = out.replace(root, tag)
    return out


def kid(args, cwd=WORK, extra_env=None):
    env = {"LC_ALL": "C.UTF-8", "LANG": "C.UTF-8", "PYTHONIOENCODING": "utf-8"}
    env.update(extra_env or {})
    env = {k: v for k, v in env.items() if v is not None}
    return subprocess.run([sys.executable, *args], cwd=str(cwd), env=env,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                          text=True, encoding="utf-8", errors="replace")


def echo(done, pad="    │ "):
    for line in done.stdout.splitlines():
        print(pad + mask(line))


def dump(done):
    echo(done)
    print(f"  └ 退出码 = {done.returncode}")


shutil.rmtree(WORK, ignore_errors=True)
for name, content in FILES.items():
    path = WORK / name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")

print("=== 1. 包 = 带 __init__.py 的目录：__init__ 的执行顺序直接上屏幕 ===")
print("  现场：ord/pkg/__init__.py、ord/pkg/sub/__init__.py、ord/pkg/sub/mod.py ——")
print("  三个文件开头各印一行标记，于是「谁被执行、什么顺序」是看得见的（cwd 都是 ord/）。")
for label, code in (("import pkg.sub.mod", "import pkg.sub.mod"),
                    ("from pkg.sub import mod", "from pkg.sub import mod"),
                    ("import pkg.sub", "import pkg.sub"),
                    ("import pkg", "import pkg")):
    print(f"  · {label}")
    echo(kid(["-c", code + "\n" + ORD_CODE], cwd=WORK / "ord"))
print("  → 判据：① 子模块被导入时，__init__.py **从最外层往里**一层层跑，最后才是模块体；")
print("    ② `from pkg.sub import mod` 一样把 pkg、pkg.sub 的 __init__ 跑掉 —— 想拿到子模块，")
print("    父包必须先成为真模块；③ 只 import 到 pkg.sub 时，mod 的模块体一行都没跑。")
print("  · 顺手把常规包身上的三个键打出来（import pkg, pkg.sub）：")
dump(kid(["-c", "import pkg, pkg.sub\n"
                 "print('pkg.__file__ =', pkg.__file__)\n"
                 "print('pkg.__path__ =', pkg.__path__, '| 类型', type(pkg.__path__).__name__)\n"
                 "print('pkg.sub.__file__ =', pkg.sub.__file__)\n"
                 "print('pkg.__package__ =', repr(pkg.__package__), '| pkg.sub.__package__ =', repr(pkg.sub.__package__))"],
           cwd=WORK / "ord"))
print("  → 常规包的三个键：__file__ 指向 __init__.py、__path__ 是**一个目录列表**（这个包的搜索根）、")
print("    __package__ 是自己的名字。第 4 段的命名空间包在这三个键上全不一样。")

print("\n=== 2. 父包对象上被绑了属性：这就是 pkg.sub 写得出来的原因 ===")
cases = [
    ("import pkg.sub.mod 之后", "import pkg.sub.mod\n"
                               "print('hasattr(pkg, \"sub\") →', hasattr(pkg, 'sub'))\n"
                               "print('hasattr(pkg.sub, \"mod\") →', hasattr(pkg.sub, 'mod'))\n"
                               "print('pkg 上非下划线的名字 →', sorted(n for n in vars(pkg) if not n.startswith('__')))",
     WORK / "ord"),
    ("只 import pkg，然后写 pkg.sub", "import pkg\n"
                                     "try:\n"
                                     "    pkg.sub\n"
                                     "except Exception as exc:\n"
                                     "    print(type(exc).__name__ + ':', exc)\n"
                                     "print(\"'pkg.sub' 在 sys.modules 里吗 →\", 'pkg.sub' in __import__('sys').modules)",
     WORK / "ord"),
    ("from pkg import sub，然后写 sub.mod", "from pkg import sub\n"
                                           "try:\n"
                                           "    sub.mod\n"
                                           "except Exception as exc:\n"
                                           "    print(type(exc).__name__ + ':', exc)\n"
                                           "print(\"'pkg.sub.mod' 在 sys.modules 里吗 →\",\n"
                                           "      'pkg.sub.mod' in __import__('sys').modules)",
     WORK / "ord"),
    ("pkgb：它的 __init__.py 里写了 from . import subb2", "import pkgb\n"
                                                         "print('hasattr(pkgb, \"subb2\") →', hasattr(pkgb, 'subb2'))\n"
                                                         "print('pkgb.subb2.X →', pkgb.subb2.X)\n"
                                                         "print(\"'pkgb.subb2' 在 sys.modules 里吗 →\", 'pkgb.subb2' in __import__('sys').modules)",
     WORK / "bind"),
]
for label, code, cwd in cases:
    print(f"  · {label}")
    echo(kid(["-c", code], cwd=cwd))
print("  → 判据：`import pkg.sub.mod` 除了跑那三个模块体，干的另一件事就是**沿父链 setattr**：")
print("    给 pkg 绑 sub、给 pkg.sub 绑 mod。所以 `pkg.sub` 写得出来不是路径魔法，是属性访问；")
print("    反过来 `import pkg` 永远不带子模块，`from pkg import sub` 也不带 sub.mod。")
print("  ★ 代价：想让 `import 顶层; 顶层.子.东西` 成立，就得在 __init__.py 里显式 import 子模块")
print("    （第四行就是那个写法，父包 __init__ 一跑，子模块体跟着全跑）。这就是「API 在 __init__.py")
print("    收口」的实现方式，代价是 import 变慢、模块体副作用提前兑现 —— 两件事都不可逆。")

print("\n=== 3. __all__ 对 from pkg import * 的影响：无 / 有 / 谎报，三种一起跑 ===")
print("  三个包都在 star/ 下：plainpkg 没有 __all__（另有子模块 suba）；")
print("  alledpkg 有 __all__ = ['PUBLIC', 'fn', 'subb']（另有 subb、subc 两个子模块）；")
print("  ghostpkg 的 __all__ = ['ghost']，可 ghost 根本不存在。")
for tag, pkg, desc in (("A", "plainpkg", "没有 __all__"), ("B", "alledpkg", "有 __all__")):
    print(f"  · {tag}：{pkg}（{desc}）")
    echo(kid(["-c", STAR_CODE.replace("@TAG@", tag).replace("@PKG@", pkg)], cwd=WORK / "star"))
print("  · C：ghostpkg（__all__ 撒谎）")
dump(kid(["-c", "ns = {}\n"
                 "try:\n"
                 "    exec('from ghostpkg import *', ns)\n"
                 "except Exception as exc:\n"
                 "    print(type(exc).__name__ + ':', exc)\n"
                 "print('本地绑到的名字（一个都没有 = 全句作废）→', sorted(k for k in ns if not k.startswith('__')))"],
           cwd=WORK / "star"))
print("  → 判据：① 没有 __all__ → `import *` 拿的是父包命名空间里**非下划线的现成名字**，")
print("    它**不替你 import 子模块**（A 那两行：suba 既没绑上也不在 sys.modules）；")
print("    ② 写了 __all__ 就是换了一份名单：只给名单里的，而且名单里的**子模块名会被真的导入**")
print("    （B：subb 进了 sys.modules，同目录的 subc 没进）；③ 名单撒谎 → 抛 AttributeError，")
print("    但一句名字都没绑上 —— 和第 a 篇第 6 段 `from m import RATE, missing` 留下半套相反：")
print("    star 是「先验名单、再一次性绑」，所以它要么全成要么全不成立。")
print("  ★ 代价：__all__ 里列子模块 = 用一行字符串换「import * 就执行这些模块体」；")
print("    列多了是启动税、列错名字直接炸。它还同时被 Sphinx/pylint 一类工具当公开 API 名单读。")

print("\n=== 4. 命名空间包：没有 __init__.py 的目录也能 import，三个键全变了 ===")
print("  现场：ns/root1/merged/ 和 ns/root2/merged/ 都**没有** __init__.py，ns/root3/merged/ 有；")
print("  root1 里有 extra.py、root2 里有 only2.py，两边都有 leaf.py。")
for desc, pypath in (("PYTHONPATH=root1:root2", "root1:root2"),
                     ("PYTHONPATH=root3:root1:root2（把带 __init__.py 的根排第一）", "root3:root1:root2")):
    print(f"  · {desc}")
    dump(kid(["-c", NS_CODE], cwd=WORK / "ns", extra_env={"PYTHONPATH": pypath}))
print("  → 判据：同一个包名 merged，两种下场——")
print("    · 两个根都没 __init__.py → **合成一个包**：__path__ 有两项、__file__ 是 None、")
print("      __spec__.origin 是 None、__path__ 的类型是 _NamespacePath；同名的 leaf 只有第一个根的")
print("      那份被看到（另一份被静默压住），但两边**不同名**的 extra / only2 都拿得到。")
print("    · 排在前面的根放了 __init__.py → 立刻退化成常规包：__path__ 只剩那一项，")
print("      另外两个根里的 merged 内容整个不可见（extra / only2 都变 ModuleNotFoundError）。")
print("  ★ 隐患就压在这条分界线上：少一个 __init__.py，「你的包」和「别人装的同名包」会被并成")
print("    同一个命名空间 → 出现「import 到 A 的模块、却找不到 B 的名字」这种查不出来的错；")
print("    多一个 __init__.py 又反过来把别人的目录整个切断。排查一行就够：print(包.__path__)，")
print("    看这个包到底由哪几个目录拼成。（PEP 420，3.3+；本机 3.14 实测，3.8 及更早未验证。）")

print("\n=== 5. 相对导入：. 和 .. 只看 __package__ / __spec__.parent，不看文件在哪 ===")
print("  同一个文件 rel/pkgr/modr.py，它的第一句实质代码就是 from . import helper：")
for label, args in (("python3 pkgr/modr.py（脚本跑法）", ["pkgr/modr.py"]),
                    ("python3 -m pkgr.modr（模块跑法）", ["-m", "pkgr.modr"])):
    done = kid(args, cwd=WORK / "rel")
    print(f"  · {label} → 退出码 {done.returncode}")
    echo(done)
print("  · 手搓一个 __package__ 再 exec 同一份源码（既不是脚本跑法也不是 -m）：")
dump(kid(["-c", "src = open('pkgr/modr.py', encoding='utf-8').read()\n"
                 "g = {'__name__': 'pkgr.modr', '__package__': 'pkgr', '__file__': 'pkgr/modr.py'}\n"
                 "exec(compile(src, 'pkgr/modr.py', 'exec'), g)\n"
                 "print('（文件位置一个字没动，只是命名空间里给了包名）')"], cwd=WORK / "rel"))
for label, code in (("子包里的 from .. import helper（python3 -c 'import pkgr.sub.up'）", "import pkgr.sub.up"),
                    ("越界的 from ... import os（python3 -c 'import pkgr.sub.over'）", "import pkgr.sub.over")):
    done = kid(["-c", code], cwd=WORK / "rel")
    print(f"  · {label} → 退出码 {done.returncode}")
    echo(done)
print("  ★ 顺序提醒：最后一跑里两行 [INIT] 排在 traceback **后面**，那只是这个入口把 stdout 留到退出才冲出来。")
print("    执行顺序仍是 __init__.py 先跑完，才轮到 over.py 的那句 from ... import os。")
print("  → 判据：`.` = 当前包（由 __package__ / __spec__.parent 那个**名字**决定），`..` = 再上一级，")
print("    级数不能越过顶层包（最后那跑：attempted relative import beyond top-level package）。")
print("    脚本跑法里 __package__ 是 None、__spec__ 的值也是 None → 「当前包」这个概念不存在，")
print("    于是 attempted relative import with no known parent package；而手搓那一跑连 __spec__ 这个键")
print("    都没有、只给了 __package__，相对导入照样成 —— 这就是「只看名字、不看文件位置」的直接证据。")
print("  ★ 代价：带相对导入的文件**不能**当脚本跑（IDE 点「运行当前文件」正是这个下场）。")
print("    正道只有两条：-m，或者把入口写到包外面。在文件顶部 sys.path.append(...) 那种补丁会让")
print("    同一个文件有两个名字（pkgr.modr 和 modr）→ 两份对象两份状态（第 a 篇的单例），更难查。")

print("\n=== 6. python3 -m 包名：跑的是 pkg.__main__，缺它就是这一句 ===")
print("  先连跑三个「没有 __main__.py」的包：常规包 pkgr、常规子包 pkgr.sub、命名空间包 merged。")
for label, args, cwd, envadd in (
        ("python3 -m pkgr", ["-m", "pkgr"], WORK / "rel", {}),
        ("python3 -m pkgr.sub", ["-m", "pkgr.sub"], WORK / "rel", {}),
        ("PYTHONPATH=root1:root2 python3 -m merged", ["-m", "merged"], WORK / "ns",
         {"PYTHONPATH": "root1:root2"})):
    done = kid(args, cwd=cwd, extra_env=envadd)
    print(f"  · {label} → 退出码 {done.returncode}")
    echo(done)
print("    （顺序提醒：上面三跑把报错印在 [INIT] 前面，是因为这条命令走的是「直接写 stderr 就退出」的路径，")
print("      子进程的 stdout 还压在缓冲区里，退出时才吐。真实顺序是包的 __init__.py 先跑完、才去找")
print("      __main__ —— 下面成功那一跑里 [INIT] 就老老实实排在第一行。）")
(WORK / "rel" / "pkgr" / "__main__.py").write_text(MAIN_SRC, encoding="utf-8")
print("  · 往 pkgr/ 里现写一个 __main__.py（里面照常用 from . import helper），同一条命令再跑：")
dump(kid(["-m", "pkgr"], cwd=WORK / "rel"))
print("  → 判据：`-m 包名` 找的是 `<包>.__main__` 这个模块，把它的 __name__ 设成 '__main__'、")
print("    __package__ 设成包名（上面 __spec__.name 那行就是证据）—— 所以 __main__.py 里既能用相对")
print("    导入、又能当命令行入口，而 pkg/__init__.py 仍在它之前跑完（[INIT] 排在最前）。")
done = kid(["-m", "json", "--help"], cwd=WORK / "rel")
first = (done.stdout.splitlines() or [""])[0]
print(f"  · 标准库对照组：json 真的有 json/__main__.py → python3 -m json --help 退出码 {done.returncode}，首行 {mask(first)}")
done = kid(["-m", "json.tool", "--help"], cwd=WORK / "rel")
first = (done.stdout.splitlines() or [""])[0]
print(f"  · 另一种跑法：python3 -m json.tool --help 退出码 {done.returncode}，首行 {mask(first)}")
print("  ★ `-m json.tool` 跑的是**包里的一个模块** tool.py，跟 __main__.py 无关（`json` 那份 __main__.py")
print("    做的事和 tool.py 并不完全一样）。对照第 b 篇第 6 段：那里的假 json.py 抢走名字之后，")
print("    `-m json.tool` 报的是「__path__ attribute not found」—— 因为那时 json 根本不是包。")
print("    两句原文很像，指向的原因完全不同，别看错。")
print("  ★ 代价：一个包能 `import` 不等于能 `-m`；而 `-m pkg` 跑起来时 pkg/__init__.py 会先被完整")
print("    执行一遍（上面那行 [INIT] 就排在 __main__ 的输出前面）—— 启动开销和副作用都在这。")

print("\n=== 7. 收口：模块 / 常规包 / 命名空间包 对照表 ===")
print("  三种东西并排（每一格都是本篇跑过的，段号标在括号里）：")
rows = [
    ("普通模块 m.py",
     "__file__ = m.py 本身（1）", "__path__ = 没这个键（1）",
     "模块体跑一次（1）", "__init__.py = 谈不上",
     "import * → 非下划线名字，或按 __all__（3）", ".. → 不能用：__package__ 是 None（5）",
     "-m m → 跑它自己（b 篇第 3 段）"),
    ("常规包（有 __init__.py）",
     "__file__ = 指向 __init__.py（1）", "__path__ = list，只有本目录 1 项（1）",
     "模块体 = __init__.py，首次 import 跑（1）", "__init__.py = 会执行的那份",
     "import * → 同上；__all__ 里的子模块名会被真的 import（3）",
     ".. → 看 __package__，-m 或包内 import 才有（5）", "-m 包名 → 必须有 __main__.py，否则 6 段那一句"),
    ("命名空间包（无 __init__）",
     "__file__ = None（4）", "__path__ = _NamespacePath，可多项（4）",
     "没有可跑的东西（4）", "没有这个文件",
     "import * → 空列表，一个名字都拿不到（4）", ".. → 同样看 __package__（4）",
     "-m 包名 → 照样报 No module named merged.__main__（6 段第三行）"),
]
for row in rows:
    print("  " + row[0])
    for cell in row[1:]:
        print("      · " + cell)
print("  → 三条判据：① 一个目录算不算「常规包」就是 __init__.py 这一个文件的事：有它 = 一个会执行的")
print("    模块体 + 一条把同名目录切断的界线；没它 = 多个根合并出来的命名空间（第 4 段）；")
print("    ② 子模块会不会被带出来，只看有没有人 import 它 —— 父包 __init__.py 是唯一能替你 import 的")
print("    地方（第 2 段第四行），`from pkg import *` 要不要 import 子模块则由 __all__ 决定（第 3 段）；")
print("    ③ 相对导入的锚点是 __package__ 这个**名字**，所以「怎么跑起来的」比「文件在哪」重要。")
print("  ★ 代价：__init__.py 的每一行都在首次 import 那一刻执行，写多了就是启动税；__all__ 和")
print("    __init__ 里的 re-export 会让「包暴露的名字」和「文件里的名字」脱钩 —— 好处是 API 能收口、")
print("    内部能重构，坏处是报错里出现的名字在你打开的那个文件里找不到，只能靠这张表反推。")

shutil.rmtree(WORK)
print(f"\n  收尾自证：{WORK.name}/ 还在吗 → {WORK.exists()} ｜ 当前目录剩下的目录 → "
      f"{sorted(p.name for p in Path('.').iterdir() if p.is_dir())}（含中途现造的 __main__.py，一起删净）")
