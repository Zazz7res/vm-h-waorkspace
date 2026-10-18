#!/usr/bin/env python3
"""场景 3：`sys.path` 逐条拆开看 —— 谁往里塞了哪一项，顺序是什么。

四种启动方式的输出都不一样，README 里各跑了一遍：
    python3 -c "import d_path_probe"        （-c）
    python3 d_path_probe.py                 （脚本文件）
    python3 -m d_path_probe                 （-m）
    python3 < d_path_probe.py               （从标准输入喂）

真正要记住的只有一条：**第 0 项的含义随启动方式变**，
而它是「优先级最高」的那一项 —— import 找不到东西 / 找错东西，八成是它。
"""

import os
import sys


def classify(entry, index):
    """给 sys.path 里每一项贴个「它是谁」的标签。"""
    if entry == "":
        return "空字符串 → 意思是「当前工作目录」，在**每次 import 的那一刻**才解析"
    if entry.endswith(".zip"):
        return "标准库的 zip 打包位（本机根本没有这个文件，但这一项照样在列表里）"
    if entry.endswith("lib-dynload"):
        return "C 扩展（.so）专用目录，比如 _json、math 这些"
    if "dist-packages" in entry:
        return "发行版（apt）装的第三方包目录"
    if entry.endswith("site-packages"):
        return "pip / venv 装的第三方包目录"
    if entry == os.path.dirname(os.path.abspath(__file__)):
        return "★ 本脚本所在的目录（用脚本文件方式启动时，它被放在第 0 项）"
    if os.getcwd() == entry:
        return "★ 当前工作目录（-c / -m / 从 stdin 启动时放的是它，而且是绝对路径）"
    if entry.endswith(tuple(str(sys.version_info.major) + "." + str(sys.version_info.minor))):
        return "标准库的 .py 文件都在这（os、json、dis……）"
    return "其他"


print("=" * 72)
print("一、现场信息：谁在什么目录里跑")
print("=" * 72)
print(f"os.getcwd()                       {os.getcwd()}")
# __file__ 只有「脚本是从真实文件来的」才存在；`-c` 和 stdin 两种情况里没有，所以要兜底。
this_file = globals().get("__file__", None)
print(f"__file__（本文件）                 {this_file}")
if this_file:
    print(f"本文件所在目录                    {os.path.dirname(os.path.abspath(this_file))}")
else:
    print("本文件所在目录                    没有 __file__ —— 因为代码不是从文件加载进来的")
print(f"sys.argv[0]                       {sys.argv[0]!r}")
print()
print("对照看这两个值：`sys.argv[0]` 是「shell 传给你的那个写法」，可能是相对路径；")
print("而 sys.path[0] 是解释器算出来的「绝对目录」。两者不是一回事，别混。")

print()
print("=" * 72)
print("二、sys.path 全文（顺序 = 查找顺序，前面的赢）")
print("=" * 72)
for i, entry in enumerate(sys.path):
    exists = "存在" if entry and os.path.isdir(entry) else ("n/a" if not entry else "不存在")
    print(f"[{i}] {entry!r}")
    print(f"     状态={exists}　说明={classify(entry, i)}")

print()
print("=" * 72)
print("三、和 import 直接相关的三件事")
print("=" * 72)
print(f"sys.path_importer_cache 的键（已扫描过的目录）：{len(sys.path_importer_cache)} 个")
print(f"sys.modules 里已加载的模块数：{len(sys.modules)}")
# PYTHONPATH 是被 site/解释器插在「第 0 项之后、标准库之前」的，这里现算给你看。
pp = os.environ.get("PYTHONPATH")
print(f"PYTHONPATH 环境变量：{pp!r}")
if pp:
    wanted = [p for p in pp.split(os.pathsep) if p]
    print(f"→ 它被展开成 {len(wanted)} 项，插在了 sys.path 的下标 "
          f"{[sys.path.index(w) for w in wanted]}")
    print("  也就是说：PYTHONPATH 里的目录**排在标准库前面**，可以同名覆盖标准库模块（危险）。")
