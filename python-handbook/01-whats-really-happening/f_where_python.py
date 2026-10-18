#!/usr/bin/env python3
"""场景 4：`python3` 到底是哪个文件，它眼里的「自己家」在哪。

跑法：
    python3 f_where_python.py

这一页的输出用来回答四个具体问题：
  1. shell 找到的 `python3` 是哪个文件？是不是软链接？（shell 侧的 which/ls 见 demo.sh）
  2. 解释器自己报告的 `sys.executable` 是什么？（被 venv 包住时会不一样，见 09 章）
  3. `sys.prefix` / `sys.base_prefix` 分别是谁的家？两者相等 = 你没在虚拟环境里。
  4. 为什么本机没有 `python` 这个命令，只有 `python3`？（发行版故意不装，见 README 环境段）
"""

import os
import sys
import sysconfig

print("=" * 68)
print("一、解释器自己的身份")
print("=" * 68)
print(f"sys.executable       {sys.executable}")
print(f"sys.version          {sys.version.splitlines()[0]}")
print(f"sys.implementation   {sys.implementation.name} {sys.implementation.version.major}"
      f".{sys.implementation.version.minor}.{sys.implementation.version.micro}")
print(f"sys.api_version      {sys.api_version}  (ABI 版本号，决定 .so 能不能混用)")
print(f"platform             {sys.platform}")
print(f"maxsize / 位宽        {sys.maxsize} → {sys.maxsize.bit_length() + 1} 位")

print()
print("=" * 68)
print("二、两个 prefix：判断「我在不在虚拟环境里」的铁证")
print("=" * 68)
# sys.prefix = 「这次运行用哪套库」；sys.base_prefix = 「原始安装位置」。
# 在 venv 里两者会分开（prefix 指向 venv 目录）；系统 Python 直接跑时两者相同。
print(f"sys.prefix           {sys.prefix}")
print(f"sys.base_prefix      {sys.base_prefix}")
print("→ 相同 ⇒ 当前不在虚拟环境里（本机就是这种情况）")

print()
print("=" * 68)
print("三、安装目录（这些就是 sys.path 里那几个标准项的来源）")
print("=" * 68)
for key in ("stdlib", "platstdlib", "purelib", "platlib", "include"):
    print(f"{key:<12} {sysconfig.get_path(key)}")

print()
print("=" * 68)
print("四、C 扩展长什么样（lib-dynload 里是真的 .so 文件）")
print("=" * 68)
dyn = sysconfig.get_path("platstdlib") + "/lib-dynload"
if os.path.isdir(dyn):
    names = sorted(os.listdir(dyn))
    print(f"{dyn}")
    print(f"共 {len(names)} 个文件，前 3 个：")
    for n in names[:3]:
        print(f"  {n}")
    print("→ 文件名里带 cpython-314-x86_64-linux-gnu：解释器版本 + ABI + 平台，三者都要对上。")
else:
    print(f"{dyn} 不存在（有些发行版把 C 扩展直接放进 stdlib 目录）")

print()
print("=" * 68)
print("五、编码默认值（03/08 章的地基）")
print("=" * 68)
print(f"sys.getdefaultencoding()  {sys.getdefaultencoding()}")
print(f"sys.getfilesystemencoding() {sys.getfilesystemencoding()} "
      f"(errors={sys.getfilesystemencodeerrors()})")
print(f"stdout.encoding           {sys.stdout.encoding}  ← 受 locale 影响！")
print("→ 源码文件的编码：不写 `# -*- coding:` 时按 UTF-8；但 print 出去用什么编码是上面这个值。")
