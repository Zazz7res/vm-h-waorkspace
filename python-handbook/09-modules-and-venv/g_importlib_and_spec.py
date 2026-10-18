#!/usr/bin/env python3
# ============================================================================
# 取证脚本 g_importlib_and_spec.py —— 第 09 章：导入协议本身（查、载、重载）
#
# `import x` 这条语句背后是一套可以编程调用的协议。这个脚本只演这套协议，不演语法：
#   查（find_spec）→ 载（module_from_spec + exec_module）→ 重载（reload）。
# 现场全部在 importlib-demo/ 里现造现删，屏幕上只有相对路径、basename 和 <base> 前缀。
#
# 它要证明的六件事（本机 Python 3.14.4 / Ubuntu 实测，其它版本未验证）：
#   ① `__import__("pkg.sub")` 的返回值是**顶层包 pkg**，`import_module` 的才是 pkg.sub；
#   ② find_spec 的三种结局：标准库给文件路径、找不到返回 **None（不抛异常）**、
#      命名空间包 origin=None 但 submodule_search_locations 有值；
#      ★还有第四种：父包本身不存在时它**会抛** ModuleNotFoundError —— 同一个函数两种脾气；
#   ③ 按路径把任意文件装成模块的三件套，`__name__` 完全由你给的名字决定；
#   ④ 同一个文件用两个名字加载 = 两份互不相干的活状态（现场数数给你看）；
#   ⑤ reload 只是**再跑一遍模块体**：模块对象不换、它的 __dict__ 就地更新，
#      但你手里早就拿到的函数/类还是旧对象（旧实例的 isinstance 当场失效）；
#   ⑥ 代价：走了 importlib，缓存、异常、sys.modules 注册就**都归你管**了。
#
# 用法：python3 g_importlib_and_spec.py        退出码 0 = 全跑通
# 只用标准库；不联网、不装包、不碰家目录、不 import 本章其它脚本。
# ============================================================================
import atexit
import importlib
import importlib.util
import os
import shutil
import stat
import sys
from pathlib import Path

WORK = Path("importlib-demo")          # 唯一的沙箱：所有文件都造在这里
PKGS = WORK / "pkgs"                   # 这个目录会被插进 sys.path（按名字能找到）
PLUGINS = WORK / "plugins"             # 这个目录**不在** sys.path 里（只能按路径载）
_cleaned = False


def fold(p):
    """把绝对路径折成屏幕上允许的形态：沙箱内给相对，base_prefix 内给 <base>/…，家目录给 <HOME>/…"""
    s = str(p)
    if not os.path.isabs(s):
        return os.path.normpath(s)
    rel = os.path.relpath(s, ROOT_CWD)
    if not rel.startswith(".." + os.sep):
        return rel
    if s.startswith(sys.base_prefix):
        return "<base>" + s[len(sys.base_prefix):]
    home = os.path.expanduser("~")
    if s.startswith(home):
        return "<HOME>" + s[len(home):]
    return "<abs:" + os.path.basename(s) + ">"


def onexc(func, path, exc):
    """rmtree 撞到只读/权限问题时：放开权限再试一次，还不行就咽下去。

    WHY：venv 和 __pycache__ 都可能留下不可写条目，默认 rmtree 会整棵删到一半抛异常，
    那时沙箱就残在章目录里了 —— 宁可漏删一个文件，也不能让收尾动作本身崩在半路。
    """
    try:
        os.chmod(os.path.dirname(path), stat.S_IRWXU)
        os.chmod(path, stat.S_IRWXU)
        func(path)
    except OSError:
        pass


def cleanup():
    """删沙箱并当场自证。只打印一次（正常路径和第 7 段手动调过之后 atexit 不再重复）。"""
    global _cleaned
    if _cleaned:
        return
    _cleaned = True
    if WORK.exists():
        shutil.rmtree(WORK, onexc=onexc)
    left = sorted(x.name for x in Path(".").iterdir() if x.name.endswith("-demo"))
    print(f"\n  清理：{WORK.name}/ 已删除 ｜ 还在吗 → {WORK.exists()}"
          f" ｜ 当前目录里 *-demo 残留 = {left or '无'}")


def write(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")
    return path


ROOT_CWD = Path.cwd().resolve()        # 只用来打码，永不上屏

# ---------------------------------------------------------------- 造现场
if WORK.exists():                      # 上次崩溃留下的残骸：先整份清掉，保证每次同一现场
    shutil.rmtree(WORK, onexc=onexc)
atexit.register(cleanup)               # 沙箱一出现就登记收尾，中途异常也要删
write(PKGS / "pkg" / "__init__.py", 'PKG_MARK = "pkg 的 __init__ 跑过了"\n')
write(PKGS / "pkg" / "sub.py", 'VALUE = 42\n\n\ndef double(x):\n    return x * 2\n')
# 命名空间包：目录、子目录里都**没有** __init__.py，按名字依然能查到它
write(PKGS / "nspkg" / "leaf.py", 'LEAF = "namespace leaf"\n')
# 给 reload 用的模块（第 6 段会把它改写过一遍）
write(PKGS / "rload.py",
      'RUN_COUNT = globals().get("RUN_COUNT", 0) + 1   # 模块体每跑一次就 +1\n'
      'VERSION = 1\n'
      'WHO = "before"\n'
      '\n'
      '\n'
      'def greet(who=WHO):\n'
      '    return "hi " + who\n'
      '\n'
      '\n'
      'class Widget:\n'
      '    pass\n')
# 给「按路径加载」用的文件：它所在的目录不在 sys.path 里，所以按名字找不到
write(PLUGINS / "dyn.py",
      'STATE = {"count": 0}\n'
      '\n'
      '\n'
      'def bump():\n'
      '    STATE["count"] += 1\n'
      '    return STATE["count"]\n')

print("=== 1. 造现场：一个包、一个命名空间包、一个不在 sys.path 里的插件文件 ===")
tree = sorted(fold(p) for p in WORK.rglob("*") if p.is_file())
print(f"  沙箱 {WORK.name}/ 里的文件（排序，相对当前目录）：")
for item in tree:
    print(f"    {item}")
print("  三个角色的区别就在有没有 `__init__.py`：")
print(f"    pkg/    有 __init__.py → 普通包；"
      f"nspkg/  有 __init__.py 吗 → {(PKGS / 'nspkg' / '__init__.py').exists()}（所以它是命名空间包）")
print(f"    plugins/dyn.py  所在目录在 sys.path 里吗 → "
      f"{str(PLUGINS.resolve()) in [os.path.abspath(p) for p in sys.path]} —— 只能按路径载")
sys.path.insert(0, str(PKGS))          # 下面几段都靠这一行「按名字找得到 pkg / nspkg / rload」
print(f"  已把 {fold(PKGS)} 插到 sys.path[0]（这是 importlib 的用法演示，不是给读者抄的常规操作）")
print(f"  现在 sys.meta_path 上的 finder：{[getattr(f, '__name__', type(f).__name__) for f in sys.meta_path]}")
print("  → 查一个名字时，依次问过这些 finder；importlib.util.find_spec 就是这条链的可调用入口。")

print("\n=== 2. import_module 和 __import__：载进来的东西一样，给你的返回值不一样 ===")
via_importlib = importlib.import_module("pkg.sub")
via_builtin = __import__("pkg.sub")
via_fromlist = __import__("pkg.sub", fromlist=["VALUE"])
print(f"  importlib.import_module('pkg.sub')  → 返回对象的 __name__ = {via_importlib.__name__!r}")
print(f"  __import__('pkg.sub')               → 返回对象的 __name__ = {via_builtin.__name__!r}"
      f"   ★顶层包，不是 pkg.sub")
print(f"  __import__('pkg.sub', fromlist=[…])  → 返回对象的 __name__ = {via_fromlist.__name__!r}")
print(f"  import_module 的返回值和 fromlist 版 __import__ 是同一个对象吗 → {via_importlib is via_fromlist}")
print(f"  import_module 的返回值和裸 __import__ 是同一个对象吗 → {via_importlib is via_builtin}")
print(f"  但 sys.modules 里留下的键（排序）：{sorted(k for k in sys.modules if k == 'pkg' or k.startswith('pkg.'))}")
print(f"  顶层包对象上能不能直接摸到子模块 → hasattr(via_builtin, 'sub') = {hasattr(via_builtin, 'sub')}")
print("  → `import pkg.sub` 这条**语句**等价于裸 __import__：只把 pkg 绑进当前命名空间，"
      "所以 `sub.value` 用不了、必须写全 `pkg.sub.value`；这不是语法挑剔，是返回值本来就是 pkg。")
print("  → 判据：名字是动态的（配置里读出来的、用户传的）就用 import_module，它给的对象和你想的一样。")

print("\n=== 3. find_spec 的三种结局 + 第四种脾气（它是唯一「查而不载」的入口）===")
lib_spec = importlib.util.find_spec("textwrap")
print(f"  ① 标准库 textwrap：origin → {fold(lib_spec.origin)}"
      f"（basename = {os.path.basename(lib_spec.origin)}）｜ submodule_search_locations → {lib_spec.submodule_search_locations}")
pkg_spec = importlib.util.find_spec("pkg.sub")
print(f"     对比包内模块 pkg.sub：origin basename = {os.path.basename(pkg_spec.origin)}"
      f"｜ cached 是 .pyc 吗 → {str(pkg_spec.cached).endswith('.pyc')}｜ 相对位置 {fold(pkg_spec.origin)}")
missing = importlib.util.find_spec("no_such_module_xyz")
print(f"  ② 查不到的名字 no_such_module_xyz：find_spec → {missing!r}"
      f"   ★返回 None，不抛异常（这是它和 import_module 最大的性格差别）")
try:
    importlib.import_module("no_such_module_xyz")
except ModuleNotFoundError as exc:
    print(f"     同一个名字交给 import_module → 抛 {type(exc).__name__}: {exc}")
ns_spec = importlib.util.find_spec("nspkg")
print(f"  ③ 命名空间包 nspkg（目录里没 __init__.py）：origin → {ns_spec.origin!r}"
      f" ｜ loader → {ns_spec.loader!r}")
print(f"     但 submodule_search_locations 有值：类型 {type(ns_spec.submodule_search_locations).__name__}、"
      f"长度 {len(list(ns_spec.submodule_search_locations))}、"
      f"位置 {[fold(p) for p in list(ns_spec.submodule_search_locations)]}")
print("     → 判据：origin 是 None 而 search_locations 非 None，就是命名空间包；"
      "「一个包被拆在多个目录里」靠的就是这个（PEP 420）。")
try:
    importlib.util.find_spec("no_such_parent.child")
except ModuleNotFoundError as exc:
    print(f"  ④ ★第四种脾气：find_spec('no_such_parent.child') 不返回 None，抛 {type(exc).__name__}: {exc}")
print(f"     对比：父包存在、子模块不存在时它又回到返回 None —— find_spec('pkg.no_sub') → "
      f"{importlib.util.find_spec('pkg.no_sub')!r}")
print("  → 所以拿 find_spec 做「存在性检查」必须写成两层：整个调用包 try，返回值再判 None。")

print("\n=== 4. 从任意路径把一个文件装成模块：三件套，外加 __name__ 是你说了算 ===")
dyn_path = PLUGINS / "dyn.py"
print(f"  文件在 {fold(dyn_path)}；先确认按名字根本找不到它：find_spec('dyn') → "
      f"{importlib.util.find_spec('dyn')!r}")
spec_a = importlib.util.spec_from_file_location("alpha", dyn_path)
mod_a = importlib.util.module_from_spec(spec_a)
print(f"  spec_from_file_location(\"alpha\", …) → name={spec_a.name!r}、"
      f"loader={type(spec_a.loader).__name__}、origin={fold(spec_a.origin)}")
print(f"  module_from_spec 之后还没执行它：模块里已有的名字 → "
      f"{sorted(n for n in vars(mod_a) if not n.startswith('__')) or '（空）'}"
      f"   ★此时 STATE/bump 都不存在，少了 exec_module 就是拿到一个空壳")
spec_a.loader.exec_module(mod_a)
print(f"  exec_module 之后：__name__ = {mod_a.__name__!r} ｜ bump() → {mod_a.bump()} ｜ STATE = {mod_a.STATE}")
print(f"  ★文件名是 dyn.py，__name__ 却是 'alpha' —— 这个名字是**我传给 spec_from_file_location 的**，"
      f"跟文件名无关；`if __name__ == '__main__'` 那道闸门在这里由你决定放不放行。")
print(f"  exec 完了 sys.modules 里有 'alpha' 吗 → {'alpha' in sys.modules}"
      f"   ★手加载**不会**自动注册，注册是你自己的活（下一行就手动补上）")
sys.modules["alpha"] = mod_a
print(f"  手动注册后：sys.modules['alpha'] is mod_a → {sys.modules['alpha'] is mod_a}")
print("  → 不注册会踩的坑：模块内部第二次 import 自己会再执行一遍模块体（拿到第二份常量），"
      "dataclass/enum/异常类这些「按类对象认身份」的代码会当场精神分裂。")

print("\n=== 5. 同一个文件、两个名字 = 两份独立状态（用计数证明，不是嘴上说）===")
spec_b = importlib.util.spec_from_file_location("beta", dyn_path)
mod_b = importlib.util.module_from_spec(spec_b)
spec_b.loader.exec_module(mod_b)
sys.modules["beta"] = mod_b
print(f"  两次 exec 的是同一个文件 {fold(dyn_path)}，只是名字不同。")
print(f"  mod_a.bump() → {mod_a.bump()}")
print(f"  mod_a.bump() → {mod_a.bump()}")
print(f"  mod_b.bump() → {mod_b.bump()}   ★还是 1：它数的是自己那份 STATE")
print(f"  STATE 是同一个对象吗 → {mod_a.STATE is mod_b.STATE}"
      f" ｜ bump 是同一个函数对象吗 → {mod_a.bump is mod_b.bump}")
print(f"  各自的 STATE 现在：alpha={mod_a.STATE} beta={mod_b.STATE}")
print(f"  sys.modules 里这两个键都在：{[k for k in sorted(sys.modules) if k in ('alpha', 'beta')]}")
print("  → 判据：import 的**身份是名字（sys.modules 的键），不是文件**。"
      "同一个 .py 被两个名字引进来就是两个实例，模块级单例、注册表、计数器全部各算各的。")
print("  ★ 这也是「同一个包既被绝对导入又被相对导入」时全局状态凭空分裂的根因，"
      "插件框架最容易在这里漏一次初始化。")

print("\n=== 6. reload 的真实语义：模块体重跑，旧引用一个都不换 ===")
rmod = importlib.import_module("rload")
old_greet = rmod.greet
old_widget = rmod.Widget
old_instance = old_widget()
before = (rmod.VERSION, rmod.RUN_COUNT, old_greet.__defaults__, old_greet())
print(f"  reload 前：VERSION={before[0]}、模块体已跑 {before[1]} 次、"
      f"greet.__defaults__={before[2]}、greet() → {before[3]!r}")
write(PKGS / "rload.py",
      'RUN_COUNT = globals().get("RUN_COUNT", 0) + 1   # 模块体每跑一次就 +1\n'
      'VERSION = 2\n'
      'WHO = "after"\n'
      '\n'
      '\n'
      'def greet(who=WHO):\n'
      '    return "HI " + who\n'
      '\n'
      '\n'
      'class Widget:\n'
      '    pass\n')
after = importlib.reload(rmod)
print(f"  reload 返回的对象就是原来那个模块 → {after is rmod}"
      f" ｜ sys.modules['rload'] 还是它吗 → {sys.modules['rload'] is rmod}")
print(f"  reload 后：VERSION={rmod.VERSION}、模块体跑了 {rmod.RUN_COUNT} 次"
      f"   ★RUN_COUNT 能累加，说明它是**在原 __dict__ 上就地重跑**，不是新建模块")
print(f"  模块属性 rmod.greet 现在指向新的函数对象（旧的是 old_greet）→ {rmod.greet is not old_greet}")
print(f"  但旧函数还是旧的：old_greet.__defaults__={old_greet.__defaults__}、old_greet() → {old_greet()!r}")
print(f"  新函数：rmod.greet.__defaults__={rmod.greet.__defaults__}、rmod.greet() → {rmod.greet()!r}")
print(f"  ★旧实例的 isinstance 当场失效：isinstance(old_instance, rmod.Widget) → "
      f"{isinstance(old_instance, rmod.Widget)}"
      f"（它的 __class__ 还是 reload 前那个 Widget 类对象）")
print(f"  已经 in-flight 的默认参数、已经建好的实例、别处 `from rload import greet` 拿到的名字，"
      f"reload 一个都不会替它们换掉。")
print("  → 判据：reload 的作用域只有「模块自己重跑一遍」。"
      "它不清缓存、不改引用、不重启已运行的代码；想真正干净，只有新进程这一条路。")

print("\n=== 7. 收口：什么时候才该动 importlib，以及它的第一代价 ===")
print("  该动它的场景（就这三类，其它情况老实写 import 语句）：")
print("  ① 插件/驱动注册：名字来自配置目录或 entry 表，编译期根本不知道叫什么 → import_module(name)。")
print("  ② 从任意目录加载单个文件（用户脚本目录、打包资源）→ spec_from_file_location 三件套。")
print("  ③ 测试里要「重新拿一份干净的模块状态」→ 与其 reload，不如给同一个文件换个名字加载两次"
      "（第 5 段证过：换名字才有两份真状态，reload 只有一份就地改）。")
print("  不该动它的场景：想让 import「更聪明」一点 —— 手写 sys.path、手动 exec_module、"
      "自己往 sys.modules 塞东西，这三件事凑一起的项目，一年后没人说得清哪个模块是活的。")
print("  ★ 第一代价：**你自己接管了失败**。")
print("    · find_spec 找不到只给你 None，不报错 —— 你不在判空处停下，下一行 AttributeError 更难看；")
print(f"    · 父包缺失时它偏偏又抛异常（第 3 段第 ④ 条），一个 API 两种脾气，两层都得防；")
print("    · exec_module 抛的是模块体里的原始异常，栈里带着你的加载器帧，"
      "而且失败后 sys.modules 里可能留下半初始化的壳 —— 要么自己删，要么别注册；")
print("    · 缓存、编码、__pycache__、freeze、多线程导入锁，import 语句替你处理的那些事，全归你了。")
print("  → 一句话：`import` 是带护栏的自动档，importlib 是手动档。"
      "换手动档换来的是「名字/路径/时机」三个自由度，付出去的是失败路径全部自己写。")
print("  未验证、别当事实引用的部分：Windows 下 spec.origin 的分隔符与命名空间包行为、"
      "冻结/打包环境（PyInstaller 之类）里 find_spec 的结果、以及 3.12 及更早版本"
      "reload 对 __path__ 的处理 —— 本机这台 Ubuntu 3.14.4 测不到，就不下结论。")

cleanup()
sys.exit(0)
