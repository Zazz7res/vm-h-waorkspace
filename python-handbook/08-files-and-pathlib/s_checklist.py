#!/usr/bin/env python3
# ============================================================================
# 取证脚本 s_checklist.py —— 第 20 节：对账（本章这 19 个脚本是不是真按规矩写的）
#
# 为什么要有它：本章 README 会承诺两件事 ——
#   「每个取证脚本都分了段、末段都给了判据」，以及「每个脚本自己建的文件自己删干净」。
# 两句承诺如果没有一条命令能证伪，它们就只是两句漂亮话，三个月后一定和现实不符。
# 所以把承诺拆成五条**能失败**的规矩，拿它们去扫全章 .py；
# 第 3 段再当场造五个坏样本，让五条规矩各响一次 —— 不响的检查比没有检查更糟。
#
# 它只**读源码文本**，不 import、不执行任何脚本：执行是 demo.sh 的事，
# 一旦对账工具开始跑被检查对象，它就把自己弄成了新的不确定性来源。
#
# 用法：python3 s_checklist.py            扫当前目录全部 *.py（demo.sh 第 20 节就是这么调的）
#       python3 s_checklist.py a.py b.py  只扫点名的这几个 —— 扫描范围跟着变
#       python3 s_checklist.py demo.sh    塞一个不是取证脚本的文件进去，闸门当场响（退出码非 0）
# 退出码：0 = 全合格；1 = 有不合格。这个码是给上游（CI / make check）看的，不是给人看的。
# 输出确定性：只依赖文件内容和文件名排序 —— 无时间、无随机、无绝对路径、无内存数字。
# ============================================================================
import glob
import os
import re
import shutil
import sys

# 一行分段标题长什么样：print("=== 1. 标题 ===") 或 print("\n=== 2. 标题 ===")（允许 f"" 前缀）。
# 行首允许缩进 —— 有的脚本把分段包在函数里（print 缩进在 def 体内），那也是分段。
# 这条正则同时是「什么算取证脚本」的定义 —— 它认不出来，那个文件就没进对账。
HEAD = re.compile(r'^\s*print\((?:[a-zA-Z]*)"(?:\\n)?=== (\d+)\.\s*(.*?)\s*===')
# 规矩 ④a：建目录的三种写法，和唯一能配对的两种清理写法。
CREATE_DIR = re.compile(r'\bmakedirs\s*\(|\bmkdir\s*\(')
CLEAN_DIR = re.compile(r'\brmtree\s*\(|\brmdir\s*\(')
# 规矩 ④b：建临时文件的三种写法，配对的清理动作。
CREATE_TEMP = re.compile(r'\bmkstemp\s*\(|\bNamedTemporaryFile\s*\(|\bTemporaryDirectory\s*\(')
CLEAN_TEMP = re.compile(r'\bunlink\s*\(|\bremove\s*\(|\bclose\s*\(|\brmtree\s*\(')
# 规矩 ⑤：本章一条网络命令都不该有（import 层面就拦住，不看它有没有真调用）。
NETWORK = re.compile(r'^\s*(?:import|from)\s+(socket|urllib|urllib\.request|http|http\.client|'
                     r'ftplib|smtplib|poplib|imaplib|nntplib|ssl|multiprocessing\.connection)\b',
                     re.MULTILINE)
DEMO_DIR = "check-demo"          # 第 3 段造坏样本用的子目录：放在子目录里才不会被第 2 段扫到
RULES = "① 编号连续 ② 末段收口 ③ 至少一段 ④ 建过就得收 ⑤ 不联网"


def check(path):
    """按五条规矩查一个文件，返回 (分段列表, 问题列表)。问题列表空 = 合格。

    这里刻意**不**查的东西也值得说一句：没查脚本能不能跑通、每段内容对不对、
    有没有把绝对路径印上屏幕 —— 前三条只有人或 demo.sh 知道，最后一条由 demo.sh 第 21 节
    用 find 数残留目录来兜（源码里写了 rmtree 不代表异常路径真执行到了，两个工具正好互补）。
    """
    try:
        text = open(path, encoding="utf-8").read()
    except OSError as exc:
        return None, [f"读不了：{type(exc).__name__}: {exc}"]
    sections = []
    for line in text.splitlines():
        m = HEAD.match(line)
        if m:
            sections.append((int(m.group(1)), m.group(2)))
    problems = []
    # ③ 和 ① ②：先确定它是不是取证脚本，再看编号和末段。
    if not sections:
        problems.append("③ 一条分段标题都没有，它不是取证脚本")
    else:
        nums = [n for n, _ in sections]
        if nums != list(range(1, len(nums) + 1)):        # ①
            problems.append(f"① 编号不连续：实际是 {nums}")
        if "收口" not in sections[-1][1]:                # ②
            problems.append(f"② 末段标题是「{sections[-1][1]}」，没写收口 = 演了一半没给判据")
    # ④ 建过东西必须自己收：本章的主题就是文件，残留就是污染章目录。
    if CREATE_DIR.search(text) and not CLEAN_DIR.search(text):
        problems.append("④ 建了目录（mkdir/makedirs）却全程没有 rmtree/rmdir")
    if CREATE_TEMP.search(text) and not CLEAN_TEMP.search(text):
        problems.append("④ 用了 mkstemp/NamedTemporaryFile 却没有 unlink/close 把它收掉")
    # ⑤ 本章不联网：从 import 层面就拦住（写了 socket 就算，哪怕没调用）。
    hit = NETWORK.search(text)
    if hit:
        problems.append(f"⑤ 出现联网/跨进程模块：import {hit.group(1)}")
    return sections, problems


def report(path, sections, problems, width=24):
    """把一次检查的结果打成一行 —— 合格和不合格用同一种格式，才看得出一个 0 是真的 0。"""
    n = len(sections) if sections else 0
    name = os.path.basename(path)
    if problems:
        print(f"  {name:<{width}} 段数={n:<3} ★不合格（{len(problems)} 条）：{'；'.join(problems)}")
    else:
        print(f"  {name:<{width}} 段数={n:<3} 编号 1..{n} 连续 ｜ 末段「{sections[-1][1]}」")
    return bool(problems)


print("=== 1. 先把规矩写在屏幕上，再拿它去扫（五条，每一条都可能失败）===")
print(f"  {RULES}")
print("  ① 分段编号必须 1..N 连续 —— 跳号说明有一段被删了、末段编号没跟着改；")
print("  ② 最后一段的标题里必须有「收口」—— 没有就说明它只演了现象，没把判据交给你；")
print("  ③ 至少得有一条分段 —— 一句分段都没有的文件不配进这一章的清单（demo.sh 就撞在这条上）；")
print("  ④ 建过目录/临时文件就必须有清理动作 —— 本章每个脚本都会在沙箱里真建文件，不清理就是污染；")
print("  ⑤ 不许 import 网络模块 —— 本章全部结论都应该在断网时成立。")
print("  注意这里**没有**的几条，免得你以为它们被检查了：")
print("  · 没查脚本能不能跑通（跑通与否看第 2..19 节的退出码）；")
print("  · 没查每段标题的内容对不对、有没有灌水（只看标题行，正文写三行还是三十行它不管）；")
print("  · 没查屏幕上有没有绝对路径（④ 只查「源码里写了清理」，异常路径有没有真执行到，由第 21 节 find 兜）。")

print("\n=== 2. 开扫：本章每个脚本的段数、编号、末段标题 ===")
targets = sys.argv[1:] or sorted(glob.glob("*.py"))
total_sections = 0
bad_files = 0
for path in targets:
    sections, problems = check(path)
    total_sections += len(sections) if sections else 0
    bad_files += 1 if report(path, sections, problems) else 0
print(f"  —— 合计：脚本 {len(targets)} 个 ｜ 小节 {total_sections} 段 ｜ 不合格 {bad_files} 个")
print("  → README 里「19 个脚本 / N 段」这两个数字的出处就是上面这一行，不是人手写的。")
here = os.path.basename(__file__)
if here in targets:
    print(f"  本脚本自己（{here}）也在这份清单里 → 它同样得过这五条，"
          "不然它就是一盏只照别人的灯。")
else:
    print(f"  本次点名没扫到自己（{here} 不在参数里）—— 上面那句「不合格 0」只覆盖被点名的文件，"
          "别把它读成全章结论。")

print("\n=== 3. 现场造五个坏样本 + 一个好样本：证明这五条能响，也不误报 ===")


def write_case(name, nums, last_title, body=""):
    """按给定的分段编号 + 一段自定义正文，拼一个假脚本源码，只落盘不执行。"""
    lines = [f'print("=== {n}. 假段 {n} ===")' for n in (nums or [])]
    if nums:
        lines[-1] = f'print("=== {nums[-1]}. {last_title} ===")'
    path = os.path.join(DEMO_DIR, name)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + ("\n" if lines else "") + body + "\n")
    return path


os.makedirs(DEMO_DIR, exist_ok=True)
cases = [
    # 名字, 分段编号, 末段标题, 额外正文 —— 五个坏样本各只踩一条，好样本一条都不踩
    ("bad1_numbering.py", [1, 2, 4], "收口", ""),
    ("bad2_no_closure.py", [1, 2, 3], "最后一演", ""),
    ("bad3_no_section.py", None, None, 'print("整份脚本一句分段标题都没写")'),
    ("bad4_leaks_dir.py", [1, 2, 3], "收口", 'os.makedirs("x-demo", exist_ok=True)'),
    ("bad5_network.py", [1, 2, 3], "收口", "import socket  # 手滑"),
    ("good_sample.py", [1, 2, 3], "收口：三条判据",
     'os.makedirs("ok-demo", exist_ok=True)\nshutil.rmtree("ok-demo")'),
]
fired = set()
for name, nums, title, body in cases:
    path = write_case(name, nums, title, body)
    sections, problems = check(path)
    for p in problems:
        fired.add(p[0])                      # 记下这次是哪条规矩响了（①②③④⑤ 的首字符）
    report(path, sections, problems, width=22)
print("  → 五条规矩各自被一个样本触发过一次（响过的编号：" + "".join(sorted(fired)) + "），")
print("    第六个是真合格的样本、没被冤枉 —— 它建了目录也 rmtree 了，所以 ④ 不误报。")
print("    所以第 2 段那句「不合格 0 个」是有判断力的结论，不是一句恒真的废话。")
shutil.rmtree(DEMO_DIR)
print(f"  收尾自证：{DEMO_DIR}/ 还在吗 → {os.path.exists(DEMO_DIR)}"
      f" ｜ 当前目录里 .py 个数 = {len(glob.glob('*.py'))}（坏样本没留下尸体）")

print("\n=== 4. 收口：一条检查的价值 = 它能失败的那几种方式 ===")
print("  ① 写「每个脚本都分段、都自清理」这种承诺之前，先问：有没有一条命令能把它证伪？")
print("     没有 → 这条承诺三个月后一定和现实不符，而且没人知道。")
print("  ② 守卫必须**当场演示过一次触发**（第 3 段五个样本）：否则你不知道它是不是恒真。")
print("     本章第 12 节那条「没人替你 close」、第 17 节那条「有没有留 .tmp」都是同一个道理。")
print("  ③ 结论要能被上游接住：合格退 0、不合格退 1，这个脚本才可能进 make check / CI；")
print("     只会往屏幕上打「看起来没问题」的工具，人不在的时候价值是零。")
print("  ④ 判据要**只写你真的查了的东西**（第 1 段那三条「注意这里没有的」）：")
print("     检查工具最坏的失败不是漏报，是它的说明夸大了自己的覆盖面。")
print("  ⑤ 两个工具互补比一个工具万能可信：本脚本查「源码里写了清理」，第 21 节查「磁盘上真没留下」；")
print("     第 07 章那套逐字校验查「README 引的每一行真跑出来过」。三条都响不了的问题，就不该印在书里。")

sys.exit(1 if bad_files else 0)
