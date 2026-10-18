#!/usr/bin/env python3
# ============================================================================
# 取证脚本 s_checklist.py —— 第 20 节：对账（这一章的脚本是不是真按规矩写的）
#
# 为什么要有这个脚本：本章 README 里承诺了「每个取证脚本都分段、都有收口判据」。
# 一句承诺如果没有机器能复查，它就只是承诺。所以把承诺拆成三条**能失败**的规矩，
# 然后拿这三条去扫全章的 .py —— 扫不出问题不等于没问题，但扫得出问题就一定有问题；
# 第 3 段现场造三个坏样本，就是为了证明这条守卫确实会响（恒真的检查比没有检查更糟）。
#
# 它只读源码文本，不 import、不执行任何脚本 —— 执行是 demo.sh 的事。
# 用法：python3 s_checklist.py            扫当前目录全部 *.py
#      python3 s_checklist.py a.py b.py   只扫点名的这几个（第 2 段的扫描范围就跟着变）
# 退出码：0 = 全合格，1 = 有不合格。这个码是给上游（CI / make check）看的。
# 输出确定性：只依赖文件内容和文件名排序，无时间、无随机、无绝对路径。
# ============================================================================
import glob
import os
import re
import shutil
import sys

# 一行分段标题长什么样：print("=== 1. 标题 ===") 或 print("\n=== 2. 标题 ===")
HEAD = re.compile(r'^print\("(?:\\n)?=== (\d+)\.\s*(.*?)\s*===')
# 第 3 段造坏样本用的子目录：放在子目录里，才不会被当前目录的 *.py 扫到。
DEMO_DIR = "check-demo"


def check(path):
    """按三条规矩查一个文件，返回 (分段列表, 问题列表)。问题列表空 = 合格。"""
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
    if not sections:
        problems.append("一条分段都没有，它不是取证脚本")
    else:
        nums = [n for n, _ in sections]
        if nums != list(range(1, len(nums) + 1)):
            problems.append(f"编号不连续：实际是 {nums}")
        if "收口" not in sections[-1][1]:
            problems.append(f"末段标题是「{sections[-1][1]}」，没写收口 = 没给判据")
    return sections, problems


print("=== 1. 先把规矩写在屏幕上，再拿它去扫（三条，每条都可能失败）===")
print("  ① 分段编号必须 1..N 连续 —— 跳号说明有一段被删掉了，可末段编号没跟着改；")
print("  ② 最后一段的标题里必须有「收口」—— 没有就说明它演了一半，判据没给；")
print("  ③ 至少得有一条分段 —— 一条都没有的文件不配进这一章的清单。")
print("  注意这里**没有**的几条，免得你以为它们被检查了：")
print("  · 没查脚本能不能跑通（跑通与否看第 2..19 节的退出码）；")
print("  · 没查每段的标题内容对不对（内容只有你自己读得出来）；")
print("  · 没数「每段是不是都真打了输出」（只看标题行，行数再多也查不出灌水）。")

print("\n=== 2. 开扫：本章每个脚本的段数、编号、末段标题 ===")
targets = sys.argv[1:] or sorted(glob.glob("*.py"))
total_sections = 0
bad_files = 0
for path in targets:
    sections, problems = check(path)
    n = len(sections) if sections else 0
    total_sections += n
    if problems:
        bad_files += 1
        print(f"  {path:<22} 段数={n:<3} ★不合格：{'；'.join(problems)}")
    else:
        print(f"  {path:<22} 段数={n:<3} 编号 1..{n} 连续 ｜ 末段「{sections[-1][1]}」")
print(f"  —— 合计：脚本 {len(targets)} 个 ｜ 小节 {total_sections} 段 ｜ 不合格 {bad_files} 个")
here = os.path.basename(__file__)
if here in targets:
    print(f"  本脚本自己 {here} 也在这份清单里 → 它同样得过这三条，"
          "不然它就是一盏只照别人的灯。")
else:
    print(f"  本次点名没扫到自己（{here} 不在参数里）—— 上面那句「不合格 0」"
          "只覆盖被点名的文件，别把它读成全章结论。")

print("\n=== 3. 现场造三个坏样本 + 一个好样本：证明这条守卫能响，也不误报 ===")


def write_case(name, nums, last_title):
    """按给定的分段编号拼一个假脚本源码，只落盘不执行。"""
    body = "\n".join(f'print("=== {n}. 假段 {n} ===")' for n in nums[:-1])
    tail = f'print("=== {nums[-1]}. {last_title} ===")'
    path = os.path.join(DEMO_DIR, name)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write((body + "\n" if body else "") + tail + "\n")
    return path


os.makedirs(DEMO_DIR, exist_ok=True)
cases = [
    ("bad1_numbering.py", [1, 2, 4], "收口"),
    ("bad2_no_closure.py", [1, 2, 3], "最后一演"),
    ("bad3_no_section.py", None, None),
    ("good_sample.py", [1, 2, 3], "收口：三句话"),
]
for name, nums, title in cases:
    if nums is None:
        path = os.path.join(DEMO_DIR, name)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write('print("整份脚本一句分段标题都没写")\n')
    else:
        path = write_case(name, nums, title)
    sections, problems = check(path)
    n = len(sections) if sections else 0
    verdict = (f"★响了 {len(problems)} 条：{'；'.join(problems)}" if problems
               else "✓ 一条都没响（它确实合格，不是漏查）")
    print(f"  {name:<20} 段数={n:<3} {verdict}")
print("  → 三条规矩各被一个样本触发了一次（跳号 / 没收口 / 没分段），")
print("    第四个是真合格的样本、没被冤枉；")
print("    所以第 2 段那句「不合格 0 个」是有判断力的结论，不是一句恒真的废话。")
shutil.rmtree(DEMO_DIR)
print(f"  收尾自证：{DEMO_DIR}/ 还在吗 → {os.path.exists(DEMO_DIR)}"
      f" ｜ 当前目录里 .py 个数 = {len(glob.glob('*.py'))}（坏样本没留下尸体）")

print("\n=== 4. 收口：一条检查的价值 = 它能失败的那几种方式 ===")
print("  ① 写「每个脚本都有收口」这种承诺之前，先问自己：有没有一条命令能把它证伪？")
print("     没有 → 这条承诺三个月后一定和现实不符，而且没人知道。")
print("  ② 守卫必须**当场演示过一次触发**（第 3 段），否则你不知道它是不是恒真。")
print("     本章第 16 节那条「安静地跳过」是同一个道理：不响的检查等于没检查。")
print("  ③ 结论要能被上游接住：合格退 0、不合格退 1，脚本才可能进 make check / CI；")
print("     只会往屏幕上打「看起来没问题」的脚本，人不在的时候就是零。")
print("  ④ 判据要**只写你真的查了的东西**（第 1 段那三条「注意这里没有的」）：")
print("     检查工具最坏的失败不是漏报，是它的说明夸大了覆盖面。")

sys.exit(1 if bad_files else 0)
