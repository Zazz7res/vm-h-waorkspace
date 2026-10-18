#!/usr/bin/env python3
# ============================================================================
# 取证脚本 m_symlink_resolve.py —— 第 08 章第 14 节：符号链接、resolve() 和「我是谁」
#
# 这个脚本要证明什么：
#   ① 四条判断（is_symlink / exists / is_file / is_dir）对「真身、文件链接、目录链接」
#      给的是三套不同答案 —— 只看 exists() 会让你把「链接指向的东西没了」当成「链接没了」。
#   ② readlink() 给的是**链接里存的那串原文**（相对形态照原样给回），resolve() 给的是
#      **问完内核之后的绝对路径**；normpath 只算字符串、absolute() 只拼 cwd 不折叠 ..。
#      这四件事经常被当成一件事。
#   ③ 相对符号链接的目标是**相对链接自己所在的目录**，不是相对你的 cwd —— 换 cwd 真跑一遍。
#   ④ 链接「删不掉真身」，但「写会写穿真身」；备份/打包跟不跟链接，决定你会不会把 /etc 拷走。
# 屏幕上只出现相对形态、basename 和 endswith 布尔，绝不出现绝对路径（第 2 段的 shown() 负责这件事）。
# 现场全在 symlink-demo/ 里现造现删，报错一律 try/except 接住后原样打印。
# ============================================================================
import atexit
import os
import os.path
import shutil
from pathlib import Path

WORK = Path("symlink-demo")
ROOT = Path.cwd().resolve()    # 开场就记下「我在哪」的绝对形态，只用来打码，永不上屏
START = Path.cwd()

def shown(path):
    """把绝对路径换成「<cwd>/相对尾巴」再上屏。

    WHY：沙箱目录名每次都不一样（mktemp 出来的），绝对路径一上屏，
    同一份脚本两遍跑就对不上、也没必要把家目录广播出去。
    """
    return str(path).replace(str(ROOT), "<cwd>")


def tail(path):
    """对起点取相对路径 —— 屏幕上只允许这种形态的「结果」。"""
    return os.path.relpath(path, ROOT)


def nine(target):
    """四个布尔打一行：链接 / 真身 / 悬空链接全靠这一行区分。"""
    print(f"  {target.name:<12} is_symlink={str(target.is_symlink()):<6}"
          f" exists={str(target.exists()):<6} is_file={str(target.is_file()):<6}"
          f" is_dir={str(target.is_dir())}")


def make_scene():
    """现造现场：real/a/hidden.txt 是真身，两个链接分别指向文件和指向目录。"""
    (WORK / "real" / "a").mkdir(parents=True)
    (WORK / "real" / "b").mkdir(parents=True)
    body = WORK / "real" / "a" / "hidden.txt"
    body.write_text("真身原始内容\n", encoding="utf-8")
    (WORK / "real" / "a" / "link-to-txt").symlink_to("hidden.txt")   # 相对：目标写在自己旁边
    (WORK / "real" / "b" / "link-dir").symlink_to("../a")            # 相对：../ 从 b/ 起算
    return body


def cleanup():
    """收尾：链接和沙箱都不能留在章目录里。

    WHY 用 atexit 而不是 try/finally：分段标题必须顶在第 1 列（守卫正则 ^print( 要求的），
    主流程一旦被 try 包住，标题就全缩进了 —— 注册一个退出钩子，崩溃时同样会删。
    """
    if WORK.exists():
        shutil.rmtree(WORK)
    left = sorted(p.name for p in Path(".").iterdir() if p.name.endswith("-demo"))
    print(f"\n  清理：{WORK.name}/ 已删除（连里面的链接一起）｜ 还在吗 {WORK.exists()}"
          f" ｜ 当前目录里 *-demo 残留 = {left or '无'}")


print("=== 1. 造现场，然后看「真身 / 文件链接 / 目录链接」的四个布尔 ===")
if WORK.exists():                        # 上次崩溃留下的残骸：先清干净，保证每次同一现场
    shutil.rmtree(WORK)
BODY = make_scene()
atexit.register(cleanup)                        # 一造出沙箱就登记收尾，中途崩也要删
LINK = WORK / "real" / "a" / "link-to-txt"
DLINK = WORK / "real" / "b" / "link-dir"
print("  现场（全是相对路径）：real/a/hidden.txt 是真身；"
      "real/a/link-to-txt → hidden.txt；real/b/link-dir → ../a")
print("  对象           is_symlink  exists   is_file  is_dir")
for item in (BODY, LINK, DLINK):
    nine(item)
print("  → 三个对象的 exists() 都是 True，但只有第一个 is_symlink() 是 False："
      "exists() 会**顺着链接走过去**看目标。")
print("  → 目录链接 link-dir 的 is_dir() 也是 True：所以 `if p.is_dir()` 挡不住"
      "「它其实是个指向别处的链接」。")
print("  → 迭代给的是目录项本身，不是链接的目标："
      f"sorted(a/ 里的名字) = {sorted(p.name for p in (WORK / 'real' / 'a').iterdir())}")

print("\n=== 2. readlink() 拿到的是什么，resolve() 拿到的又是什么 ===")
print("  readlink 给「链接里存的原文」，一个字符都不改（相对就还是相对）：")
for item in (LINK, DLINK):
    print(f"      {tail(item):<28} → readlink() = {item.readlink()!r}")
RP = LINK.resolve()
print(f"  resolve() 给绝对路径 —— 所以本脚本**不打**它，只打能确定身份的三样：")
print(f"      是绝对路径吗 → {RP.is_absolute()} ｜ is_symlink() → {RP.is_symlink()}"
      f"（走到底就不是链接了）")
print(f"      尾巴匹配：str(rp).endswith('/real/a/hidden.txt') → {str(RP).endswith('/real/a/hidden.txt')}")
print(f"      对沙箱根取相对形态（这就是屏幕上唯一允许的形态）→ {tail(RP)!r}")
RP2 = DLINK.resolve()
print(f"      目录链接 resolve 的尾巴 → {tail(RP2)!r}"
      f"（../a 被内核从 b/ 起算，落到了 real/a）")
print("  → 判据：readlink 是「问链接存了什么」，resolve 是「问内核最终是谁」；两个问题别混。")

print("\n=== 3. 同一个带 .. 的相对路径，四条算法各给什么（★顺带验 resolve 查不查存在）===")
P = Path("symlink-demo/real/b/../b/link-dir/hidden.txt")
print(f"  ① 原样 p                        → {str(P)!r}（parts 里有 '..' 吗 → {'..' in P.parts}）")
AP = P.absolute()
print(f"  ② p.absolute()（只拼 cwd）       → {shown(AP)!r}")
print(f"      拼完之后 parts 里还有 '..' 吗 → {'..' in AP.parts}  ★absolute() **不折叠 ..**")
print(f"  ③ os.path.normpath(str(p))       → {os.path.normpath(str(P))!r}"
      f"（纯字符串折叠，link-dir 还当成普通目录名）")
print(f"  ④ os.path.abspath(p)             → {shown(os.path.abspath(P))!r}"
      f" ＝ abspath 是「拼 cwd + 折叠」，仍然没问内核链接")
RPF = P.resolve()
print(f"  ⑤ p.resolve()（问内核）          → 尾巴 {tail(RPF)!r}"
      f" ｜ endswith('/real/a/hidden.txt') = {str(RPF).endswith('/real/a/hidden.txt')}")
print("      ★ 只有 ⑤ 知道 link-dir 是链接：normpath 那行的结果是 real/b/link-dir/hidden.txt，")
print("        拼出来的字符串看着对，**指到的东西不一定对**。")
print("  ★ 再当场验掉一个最常见的误区：resolve() 会不会检查存在？现造一个悬空链接：")
DANGLING = WORK / "real" / "a" / "dangling"
DANGLING.symlink_to("nowhere.txt")
print(f"  {DANGLING.name} → nowhere.txt（真身不存在）："
      f" is_symlink={DANGLING.is_symlink()} exists={DANGLING.exists()}"
      f" os.path.exists={os.path.exists(DANGLING)}")
try:
    RPD = DANGLING.resolve()                     # 默认 strict=False
    print(f"  默认 p.resolve() 没抛异常，照样给结果 → 尾巴 {tail(RPD)!r}")
    print(f"    结果存在吗 → {RPD.exists()}  ← resolve 只负责「把链接走通」，不负责「保证有东西」")
except OSError as exc:
    print(f"  默认 resolve 抛了：{type(exc).__name__}: {shown(exc)}")
print("  resolve(strict=True) 才会炸，原文：")
try:
    DANGLING.resolve(strict=True)
except OSError as exc:
    print(f"      → {type(exc).__name__}: {shown(exc)}")
try:
    DANGLING.read_text(encoding="utf-8")
except OSError as exc:
    print(f"  真去读它时才炸：{type(exc).__name__}: {exc}（filename 是相对形态，原样保留）")
print("  → 判据：默认 resolve() **不检查存在**；要「必须存在否则报错」得自己写 strict=True。")

print("\n=== 4. ★相对符号链接相对的是**链接自己所在的目录**：换 cwd 真跑一遍 ===")
print("  同一条相对路径 'link-to-txt'，只换 cwd，不换路径：")
# chdir 一律用绝对形态：换成相对路径的话，第二步的 "symlink-demo/real/b" 会从 a/ 里找，自己就悬了。
os.chdir(ROOT / WORK / "real" / "a")
print(f"      在 {Path.cwd().name}/ 里开 'link-to-txt' → 成功，"
      f"内容={Path('link-to-txt').read_text(encoding='utf-8')!r}")
os.chdir(ROOT / WORK / "real" / "b")
try:
    with open("link-to-txt", encoding="utf-8") as fh:
        print("      在 b/ 里开它 → 竟然成功？", fh.read())
except FileNotFoundError as exc:
    print(f"      在 {Path.cwd().name}/ 里开同一条 → {type(exc).__name__}: {exc}")
print("      但在 b/ 里开 'link-dir/hidden.txt' 是成功的 ——",
      f"内容={Path('link-dir/hidden.txt').read_text(encoding='utf-8')!r}")
print("  → 因为内核处理 ../a 时，起点是**链接所在的 b/**，不是你的 cwd。")
print("    这解释了为什么「脚本里用相对链接」换目录跑就会挂，而绝对链接不会。")
os.chdir(START)
print(f"  cwd 已回到原处 → {Path.cwd() == START}")

print("\n=== 5. 删与改：unlink 只删链接；但 'w' 会**穿过链接写进真身** ===")
print(f"  删之前：真身 exists={BODY.exists()} 内容={BODY.read_text(encoding='utf-8')!r}")
LINK.unlink()
print(f"  link.unlink() 之后：链接 is_symlink={LINK.is_symlink()}（没了）"
      f" ｜ ★真身 exists 仍是 {BODY.exists()}，内容={BODY.read_text(encoding='utf-8')!r}（一个字没掉）")
LINK.symlink_to("hidden.txt")                      # 把链接造回来，演写穿
with open(LINK, "w", encoding="utf-8") as fh:      # 打开的是链接，写的却是真身
    fh.write("穿过链接改过的内容\n")
print(f"  对链接用 'w' 写完 → 链接还是链接：is_symlink={LINK.is_symlink()}"
      f" ｜ ★真身内容变成 {BODY.read_text(encoding='utf-8')!r}")
print("  → ★「打开链接写」= 改链接指向的那个文件，而且 w 的截断当场生效（第 1 节那条）。")
print("    备份脚本里一个 `open(link,'w')` 就能把生产文件清成 0 字节，屏幕上还一切正常。")
DST = WORK / "dst"
DST.mkdir()
shutil.copy(LINK, DST / "copy-follow")                     # 默认 follow_symlinks=True
shutil.copy(LINK, DST / "copy-keep", follow_symlinks=False)
print("  shutil.copy(src, dst) 两种跟随，复制完当场验：")
for name in sorted(p.name for p in DST.iterdir()):
    q = DST / name
    linkish = q.is_symlink()
    extra = f" readlink={q.readlink()!r}" if linkish else ""
    print(f"      {name:<12} is_symlink={str(linkish):<6} exists={q.exists()}{extra}")
    if linkish:                                            # 链接搬到新目录就悬空了
        try:
            q.read_text(encoding="utf-8")
        except OSError as exc:
            print(f"        ★在新目录里读它 → {type(exc).__name__}: {exc}（'hidden.txt' 在这儿不存在）")
print("  → 打包/备份的分水岭就在这：跟链接（默认）会把真身拷进包里；"
      "不跟（follow_symlinks=False）只拷链接，链接换个窝就悬空。")

print("\n=== 6. samefile / os.path.realpath 与 resolve() 的实测对照 ===")
print(f"  os.path.samefile(link, 真身) → {os.path.samefile(str(LINK), str(BODY))}"
      f" ｜ Path(link).samefile(真身) → {LINK.samefile(BODY)}")
RPP, RPR = LINK.resolve(), Path(os.path.realpath(LINK))
print(f"  realpath 与 resolve 结果相同吗 → {RPP == RPR}"
      f" ｜ 尾巴分别是 {tail(RPP)!r} / {tail(RPR)!r}")
print("  realpath 是纯字符串算的吗 → 不是：它和 resolve 一样要逐个 lstat，会展开链接；")
print("  差别只在「查不查存在」这一条边界上，realpath 也有 strict 开关（本机实测）：")
try:
    os.path.realpath(DANGLING, strict=True)
    print("      strict=True 没抛（那这条判据不成立）")
except OSError as exc:
    print(f"      → {type(exc).__name__}: {shown(exc)}")
print("  → 判据：`samefile` 比的是「同一个文件对象」（内核身份），`normpath` 比的是字符串 ——")
print("    路径字符串相等 ≠ 同一个文件；要判断「会不会写坏同一个东西」只能问内核。")

print("\n=== 7. 收口：这四条以后当判据用 ===")
print("  ① resolve() 问内核、会展开链接；normpath 纯算字符串、什么链接都不知道；")
print("     absolute() 只拼 cwd、连 .. 都不折叠（要折叠得 abspath 或 normpath）。")
print("  ② 默认 resolve(strict=False) **不检查存在**：悬空链接照样返回一个绝对路径，")
print("     「拿到路径」和「文件在那儿」是两件事。")
print("  ③ 相对符号链接的目标相对的是**链接自己所在的目录**：换 cwd 会换掉结果，")
print("     脚本搬家、cron 换目录、容器换 WORKDIR 都是这一条在咬人。")
print("  ④ 删链接不碰真身；**写链接会写穿真身**；备份跟不跟链接决定你会不会把 /etc 拷进包里。")
print("     ★ 所以：打包用 follow_symlinks=False / copytree(symlinks=True)，")
print("       写之前先 is_symlink() 判一次，别信 open() 不会改到别人。")
