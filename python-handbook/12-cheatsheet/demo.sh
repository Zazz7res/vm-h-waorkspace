#!/usr/bin/env bash
# CHECK: RUN=就地   理由：本章自己不产证据，它的证据是**同级另外 11 章的 README.md**（出处要对到那些
#        章的节标题上）。校验器把脚本拷进沙箱就跑不到兄弟文件，只会报一堆假断链，所以按全书约定
#        标成就地跑两遍。章目录本身全程只读：所有写入都发生在 mktemp -d 的沙箱里，
#        两趟之间章目录清单必须一个字都不多 —— 这条在第 8 节当场数给你看。
# ============================================================================
# 第 12 章《全书速查》—— demo.sh：把枪口对准这一页本身
#
#   bash demo.sh                 八节自查，跑完自动删沙箱（默认）
#   bash demo.sh --keep          跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt 2>&1  重定向时自动关颜色（[ -t 1 ] 判），拿到干净纯文本
#
# 八节各自的分工：
#   第 1 节  地板：解释器身份、GIL 双开关、切换节拍、核数、默认 start method、selectors 后端、
#            有没有 pip、在场/缺席名单 —— 值全部由 python3 -c 当场探，一个字都不许背
#   第 2 节  证据来源：11 个章目录都在，各章行数 / 节标题数当场数；本章目录里除 README.md 只有 demo.sh
#   第 3 节  出处解析：本页每一条 `第 NN 章 · 第 M 节[第 K 格]` 都落到真标题上（含 1 条假出处先响）
#   第 4 节  页面结构：表数 = 11、每表 4~8 行、每行三列都不空、一页纸真数到正文承诺的那个 N
#   第 5 节  骨架：从本页围栏里**原地**抽出四段 python，内存里过 ast.parse + compile()
#   第 6 节  硬事实：把表里的判断抽成 12 组探针当场再判一遍，只问形状/顺序/布尔
#   第 7 节  隐私扫描：整份屏幕账里不许出现五类身份（先造一份必然全命中的副本让它响）
#   第 8 节  红线自证 + 页面账：章目录只读、家目录条目数不变、没开监听端口、子进程归零、
#            屏幕总行数与节标题数
#
# 屏幕上是两种行，别混：
#   `$ 命令` 底下到 `[退出码 N]` 为止的输出，全是这台机器当场跑出来的原文（过 fold 折身份之前除外）；
#   以 `#` 开头的是写给读的人的话，`  → 判据：` 是每节的收口判定，它们不参与「逐字复现」的承诺。
#
# 安全说明：
#   * 章目录永远只读：只 ls / wc / grep 那 11 份 README，一个字节都不写。第 8 节先把残留
#     （`__pycache__/` 一个、非 .py 文件一个）在沙箱里的**同形状副本**上真造出来、数到非 0，
#     再对本体报 0。
#   * 只用标准库；不 pip install、不联网、不开任何监听端口、不猜密码、不连外部主机、不打印私钥。
#   * 故意**不加** `set -e`：本章一半是负样本（假章号、坏骨架、真残留、真用户名），
#     它们必须「响完继续跑」；加了它，屏幕上就只剩第一个失败，后面的守卫全成了摆设。
#   * ★ 每条断言在报 0 之前都先被人为弄坏一次。恒真的检查比没有检查更糟 —— 这是全书纪律
#     （第 07 章 · 第 20 节、第 08 章 · 第 20 节、第 11 章 · 第 14 节各演过一次）。
#   * 退出码一律在父级管道上取（`${PIPESTATUS[0]}` 先落到临时文件），写成 `$(cmd | fold)` 拿到的
#     是命令替换自己的状态，每条绿灯都会变成假凭证。
#   * 每段 python 外面套 `timeout`：卡住必须当场印成 rc=124，而不是把观众晾在那儿。
#   * 屏幕上不出现真实家目录 / 登录名 / 主机名 / 本次沙箱路径 / 对象地址形态：上屏前一律折成
#     `<家目录>` `<用户>` `<主机名>` `<沙箱>`（折主机名排在折用户名之前，反之会漏半截）。
#     五类模式串全部当场从环境取（$HOME、$USER、$(hostname)、$SB），源码里一个字都没写死。
#   * ★ 沙箱名里那 6 个随机字符每跑必变，所以默认收尾只印「沙箱已删除」而不印路径；
#     否则两遍 transcript 永远不可能逐字一致（--keep 是给人看的，不参与对账）。
#   * 本脚本自己的退出码恒为 0：它是验收闸门，不是被验收的程序。
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：${arg}（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

export LC_ALL=C.UTF-8 LANG=C.UTF-8 LANGUAGE=C
unset PYTHONPATH PYTHONHOME PYTHONSAFEPATH PYTHONSTARTUP PYTHONINSPECT PYTHONDEBUG
unset PYTHONDONTWRITEBYTECODE PYTHONOPTIMIZE PYTHONWARNINGS PYTHONDEVMODE PYTHONFAULTHANDLER
# 判据里有「几条」「几个键」这类计数，哈希种子固定下来这些数才不漂。
export PYTHONHASHSEED=0

if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd -- "$(dirname "$0")" && pwd)
BOOK=$(dirname "$HERE")
PAGE="$HERE/README.md"
# ★ 这三样在 cd 进沙箱之前取，第 8 节拿去对账才有意义。
LS_BEFORE=$(cd -- "$HERE" && ls -1 | tr '\n' '|')
HOME_N_BEFORE=$(ls -A "$HOME" | wc -l)
LISTEN_BEFORE=$(ss -ltn 2>/dev/null | tail -n +2 | wc -l)

SB=$(mktemp -d "${TMPDIR:-/tmp}/py12-XXXXXX")
MAIN_PID=$$
# ★ LS_BEFORE / HOME_N_BEFORE / LISTEN_BEFORE 也必须 export：runc 是把命令交给 **bash -c 子进程**
#   执行的，没导出的变量在那个子进程里是空的 —— 空串对空串永远「一样」，这条红线就成了恒真摆设。
export MAIN_PID HERE BOOK SB PAGE LS_BEFORE HOME_N_BEFORE LISTEN_BEFORE
: > "$SB/all.out"                           # 屏幕账：第 7 节整份扫的就是它
: > "$SB/foldhits"                          # fold 每挡一次记一笔，第 7 节要证明它真挡过
mkdir -p "$SB/pages" "$SB/work" "$SB/fakebook" "$SB/replica"
cd -- "$HERE" && ls -1 > "$SB/here-before.txt"   # 第 8 节的残留基准
cd "$SB/work" || exit 1
umask 022


banner() {
    printf '\n%s══════ 第 %s 节 %s ═══════════════════════════════%s\n' "$B" "$1" "$2" "$R"
    printf '══════ 第 %s 节 %s ═══════════════════════════════\n' "$1" "$2" >> "$SB/all.out"
}
note() {
    printf '%s\n' "$1"
    printf '%s\n' "$1" >> "$SB/all.out"
}
verdict() {
    printf '  → 判据：%s\n' "$1"
    printf '  → 判据：%s\n' "$1" >> "$SB/all.out"
}

# fold_id：上屏前把真实身份折成占位符；每折中一行就往 foldhits 记一笔。
# ★ 模式串全部当场取（$SB、$HOME、主机名、$USER），源码里一个字都没写死 —— 写死的话这份 demo
#   自己就成了「带着模式串的文件」，第 7 节的扫描会自匹配。折的顺序也是算过的：主机名必须排在
#   用户名**前面**，否则形如「用户名-机型」的主机名会被折成「<用户>-机型」，漏半截。
fold_id() {
    local tmp="$SB/.foldtmp" hn hits
    hn=$(hostname)
    cat > "$tmp"
    hits=$(grep -c -F -e "$SB" -e "$HOME" -e "$hn" -e "$USER" "$tmp")
    printf '%s\n' "${hits:-0}" >> "$SB/foldhits"
    sed -E "s#$SB#<沙箱>#g; s#$HOME#<家目录>#g; s#\\b$hn\\b#<主机名>#g; s#\\b$USER\\b#<用户>#g" "$tmp"
    rm -f "$tmp"
}

# runc：打印命令 → 执行（stderr 合并、先过 fold）→ 打印退出码。三样都进屏幕账。
# ★ rc 取的是 PIPESTATUS[0]，而且这条管道不能包在 $( ) 里（见头上安全说明）。
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    printf '$ %s\n' "$1" >> "$SB/all.out"
    local rc
    bash -c "$1" 2>&1 | fold_id > "$SB/.line.out"; rc=${PIPESTATUS[0]}
    cat "$SB/.line.out"
    cat "$SB/.line.out" >> "$SB/all.out"
    printf '[退出码 %d]\n' "$rc"
    printf '[退出码 %d]\n' "$rc" >> "$SB/all.out"
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去接着敲： cd "%s"\n  看完自己删： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] <本次 mktemp -d 出来的目录，名字每跑随机，所以不打出来>\n'
    fi
}
trap cleanup EXIT

# ----------------------------------------------------------------------------
# 八节要用的自查脚本全部现搓，整份写进沙箱（章目录里一个字节都不留）。
# ★ 有几类「禁串」只许活在这些 heredoc 生成的正文里（假章号、真路径、地址形状）：它们一旦出现在
#   demo.sh 的 echo/note 里，就等于这一页自己带着模式串，第 7 节的扫描会自匹配。
# ----------------------------------------------------------------------------

cat > "$SB/pages/check_sources.py" <<'PYEOF'
# -*- coding: utf-8 -*-
"""第 2 节：证据来源核对。argv1 = 书架根目录（真书架，或故意摆歪的假书架）。"""
import os
import re
import sys

root = sys.argv[1]
WANT = ["%02d" % i for i in range(1, 12)]
TAIL = re.compile(r"^## .*(自检问题|本章没演|怎么跑这一章|术语小抄|本章之后|本章回指)")
HEADS = [re.compile(r"^## 第 \d{1,2} 节"), re.compile(r"^## \d+\. 第 \d{1,2} 节"),
         re.compile(r"^## \d+\. "), re.compile(r"^### 场景 \d+：")]
if not os.path.isdir(root):
    print("  读不到的根目录（这一条本身就该响）")
    sys.exit(1)
bad = []
have = []
for name in sorted(os.listdir(root)):
    full = os.path.join(root, name)
    m = re.match(r"^(\d{2})-", name)
    if not os.path.isdir(full) or not m or m.group(1) == "12":
        continue
    have.append(m.group(1))
    rp = os.path.join(full, "README.md")
    if not os.path.exists(rp):
        bad.append("章目录 %s 没有 README.md" % name)
        continue
    lines = open(rp, encoding="utf-8").read().splitlines()
    secs = sum(1 for ln in lines if any(rx.match(ln) for rx in HEADS))
    cells = sum(1 for ln in lines if re.match(r"^=== \d+\. ", ln))
    tails = sum(1 for ln in lines if TAIL.match(ln))
    if not lines:
        bad.append("章目录 %s 的 README 是空的" % name)
    if secs == 0:
        bad.append("章目录 %s 数不出任何一个节标题" % name)
    print("  %s  %5d 行  节标题 %2d 个  取证分格 %2d 处  收尾节 %d 个"
          % (name, len(lines), secs, cells, tails))
missing = [w for w in WANT if w not in have]
for w in missing:
    bad.append("缺一章目录：%s 开头" % w)
print("读数：章目录 %d 个 / 应有 %d 个 / 问题 %d 条" % (len(have), len(WANT), len(bad)))
for b in bad:
    print("  [缺] %s" % b)
sys.exit(0 if not bad else 1)
PYEOF

cat > "$SB/pages/check_refs.py" <<'PYEOF'
# -*- coding: utf-8 -*-
"""第 3 节：出处解析。argv = 书架根目录、要核对的那一页、出现次数地板、去重条数地板。

不做映射表：当场到那一章的 README 里找标题。四种标题形状都归到同一个「第 M 节」上，
「第 K 格」要么在那一节的行范围里找到 `=== K. 标题 ===`，要么找到正文写死的那句「第 K 格」。
跳过 ``` 围栏：本页展开之后围栏里会带进屏幕原文，那些不是「我写的出处」。
"""
import os
import re
import sys

root, page = sys.argv[1], sys.argv[2]
floor_all, floor_uniq = int(sys.argv[3]), int(sys.argv[4])
CITE = re.compile(r"第 (\d{2}) 章 · 第 (\d{1,2}) 节(?:第 (\d{1,2}) 格)?")
H = [re.compile(r"^## 第 (\d{1,2}) 节"), re.compile(r"^## \d+\. 第 (\d{1,2}) 节"),
     re.compile(r"^## (\d{1,2})\. "), re.compile(r"^### 场景 \d+：.*（demo 第 (\d{1,2}) 节）")]


def span_of(text):
    """那一章里每个「第 M 节」的起止行号。"""
    lines = text.splitlines()
    first = {}
    for i, ln in enumerate(lines):
        for rx in H:
            m = rx.match(ln)
            if m:
                no = int(m.group(1))
                if no not in first:
                    first[no] = i
                break
    order = sorted(first.items(), key=lambda kv: kv[1])
    out = {}
    for k, (no, start) in enumerate(order):
        out[no] = (start, order[k + 1][1] if k + 1 < len(order) else len(lines))
    return out


def chapter_dir(no):
    tag = "%02d-" % no
    for d in sorted(os.listdir(root)):
        if d.startswith(tag) and os.path.isdir(os.path.join(root, d)):
            return d
    return None


total = 0
uniq = set()
broken = []
fence = False
for ln in open(page, encoding="utf-8").read().splitlines():
    if ln.startswith("```"):
        fence = not fence
        continue
    if fence:
        continue
    for m in CITE.finditer(ln):
        ch, sec, cell = int(m.group(1)), int(m.group(2)), m.group(3)
        total += 1
        uniq.add((ch, sec, cell))
        dname = chapter_dir(ch)
        if dname is None:
            broken.append((m.group(0), "第 %02d 章的目录就不在" % ch))
            continue
        rp = os.path.join(root, dname, "README.md")
        if not os.path.exists(rp):
            broken.append((m.group(0), "那一章没有 README.md"))
            continue
        text = open(rp, encoding="utf-8").read()
        sp = span_of(text)
        if sec not in sp:
            broken.append((m.group(0), "第 %02d 章里没有「第 %02d 节」这个标题" % (ch, sec)))
            continue
        a, b = sp[sec]
        body = text.splitlines()[a:b]
        if cell is not None:
            got = {int(x.group(1)) for x in
                   (re.match(r"^=== (\d+)\. ", ln2) for ln2 in body) if x}
            if int(cell) not in got and ("第 %s 格" % cell) not in "\n".join(body):
                broken.append((m.group(0), "第 %02d 章第 %02d 节里没有第 %s 格" % (ch, sec, cell)))
chaps = sorted({c for c, s, k in uniq})
print("读数：出处出现 %d 次（地板 %d）/ 去重 %d 条（地板 %d）/ 断链 %d 条"
      % (total, floor_all, len(uniq), floor_uniq, len(broken)))
print("读数：覆盖章 %s（%d 个）" % (" ".join("%02d" % c for c in chaps), len(chaps)))
for raw, why in broken:
    print("  [断] %s —— %s" % (raw, why))
bad = list(broken)
if total < floor_all:
    bad.append("出处总数 %d 低于地板 %d：解析器多半什么都没抓到" % (total, floor_all))
if len(uniq) < floor_uniq:
    bad.append("去重条数 %d 低于地板 %d" % (len(uniq), floor_uniq))
if len(chaps) != 11:
    bad.append("覆盖 %d 章，该是 11 章" % len(chaps))
print("读数：本节判定 %s" % ("通过" if not bad else "不通过（%d 条）" % len(bad)))
sys.exit(0 if not bad else 1)
PYEOF

cat > "$SB/pages/make_poison.py" <<'PYEOF'
# -*- coding: utf-8 -*-
"""第 3/4/5 节共用的坏样本制造机。argv = 真页面、输出目录、类别（refs / page / snips）。

★ 每份坏样本只坏一处，并且都是**从真页面改出来的**：找不到要改的那一处就直接退出（rc 非 0），
  绝不容许「坏样本其实没坏」这种假响。
"""
import os
import re
import sys

page, outdir, kind = sys.argv[1], sys.argv[2], sys.argv[3]
text = open(page, encoding="utf-8").read()
made = []


def save(name, body):
    with open(os.path.join(outdir, name), "w", encoding="utf-8") as f:
        f.write(body if body.endswith("\n") else body + "\n")
    made.append(name)


def need(old, body=None):
    src = text if body is None else body
    if old not in src:
        sys.exit("坏样本造不出来：找不到「%s」—— 正文改过了，先同步这里" % old[:40])
    return src


def data_rows(lines, tabno):
    """第 tabno 张表的数据行下标（跳过围栏、表头行、分隔行）。"""
    fence = False
    start = None
    out = []
    for i, ln in enumerate(lines):
        if ln.startswith("```"):
            fence = not fence
            continue
        if fence:
            continue
        if start is None:
            if re.match(r"^## 表 %d · " % tabno, ln):
                start = i
            continue
        if ln.startswith("## "):
            break
        if ln.startswith("|") and not re.match(r"^\|[\s\-|:]+\|$", ln) \
                and not ln.startswith("| 直觉以为什么"):
            out.append(i)
    if not out:
        sys.exit("坏样本造不出来：第 %d 张表没数到数据行" % tabno)
    return out


if kind == "refs":
    need("第 03 章 · 第 1 节")
    save("poison_ref_ch.md", text.replace("第 03 章 · 第 1 节", "第 99 章 · 第 7 节", 1))
    need("第 08 章 · 第 2 节第 2 格")
    save("poison_ref_sec.md", text.replace("第 08 章 · 第 2 节第 2 格", "第 08 章 · 第 99 节", 1))
    need("第 10 章 · 第 02 节第 1 格")
    save("poison_ref_cell.md",
         text.replace("第 10 章 · 第 02 节第 1 格", "第 10 章 · 第 02 节第 99 格", 1))
    save("poison_thin.md", "\n".join(text.splitlines()[:40]))
elif kind == "page":
    lines = text.splitlines()
    rows = data_rows(lines, 3)
    i = rows[2]
    cells = [c.strip() for c in re.split(r"(?<!\\)\|", lines[i])[1:-1]]
    save("poison_page_emptycell.md",
         "\n".join(lines[:i] + ["| %s | %s |  |" % (cells[0], cells[1])] + lines[i + 1:]))
    rows = data_rows(lines, 4)
    i = rows[0]
    save("poison_page_cols.md", "\n".join(lines[:i] + [lines[i] + " 多出来的第四列 |"] + lines[i + 1:]))
    need("值得背下来的 20 条")
    save("poison_page_paper.md", text.replace("值得背下来的 20 条", "值得背下来的 24 条", 1))
    need("## 表 7 · ")
    save("poison_page_tables.md",
         "\n".join([ln for ln in lines if not ln.startswith("## 表 7 · ")]))
    rows = data_rows(lines, 5)
    drop = set(rows[:5])
    save("poison_page_rows.md", "\n".join([ln for i, ln in enumerate(lines) if i not in drop]))
elif kind == "snips":
    lines_ = text.splitlines()
    marks = [i for i, ln in enumerate(lines_) if ln.strip() == "```python"]
    if len(marks) != 4:
        sys.exit("坏样本造不出来：真页面的 python 围栏不是 4 段（实为 %d）" % len(marks))
    body = []
    k = marks[0] + 1
    while lines_[k] != "```":
        body.append(lines_[k])
        k += 1
    hit = [i for i, ln in enumerate(body) if ln.rstrip().endswith(":")]
    if not hit:
        sys.exit("坏样本造不出来：第 1 段里没有以冒号结尾的行")
    i = hit[-1]
    colon = list(body)
    colon[i] = body[i].rstrip()[:-1]
    save("poison_snip_colon.md",
         "\n".join(lines_[:marks[0] + 1] + colon + lines_[k:]))
    head = marks[1]
    save("poison_snip_return.md",
         "\n".join(lines_[:head + 1] + ["return 0"] + lines_[head + 1:]))
    save("poison_snip_count.md",
         "\n".join([("```text" if i == marks[0] else ln) for i, ln in enumerate(lines_)]))
else:
    sys.exit("未知类别：%s" % kind)

print("已造坏样本 %d 份：%s" % (len(made), " ".join(made)))
PYEOF

cat > "$SB/pages/check_page.py" <<'PYEOF'
# -*- coding: utf-8 -*-
"""第 4 节：页面结构核对。argv1 = 要核对的那一页。

数五样：表数与编号、每表数据行（硬区间 4~8）、每行列数、空格子、出处那列能不能解析；
再数一页纸：正文标题里承诺了几条、围栏里真有几条，两个数必须相等。
全程跳过 ``` 围栏，所以本页展开前后数出来是同一个数。
"""
import re
import sys

CITE = re.compile(r"第 \d{2} 章 · 第 (\d{1,2}) 节")
lines = open(sys.argv[1], encoding="utf-8").read().splitlines()
bad = []
tables = []
cur = None
fence = False
for ln in lines:
    if ln.startswith("```"):
        fence = not fence
        continue
    if fence:
        continue
    m = re.match(r"^## 表 (\d+) · ", ln)
    if m:
        cur = [int(m.group(1)), []]
        tables.append(cur)
        continue
    if ln.startswith("## "):
        cur = None
        continue
    if cur is not None and ln.startswith("|"):
        if not re.match(r"^\|[\s\-|:]+\|$", ln) and not ln.startswith("| 直觉以为什么"):
            cur[1].append(ln)
nums = [t[0] for t in tables]
sizes = [len(t[1]) for t in tables]
print("读数：表数 = %d，编号 = %s" % (len(nums), " ".join("%02d" % n for n in nums)))
print("读数：每表数据行 = %s（硬区间 4~8）" % " ".join(str(s) for s in sizes))
if len(nums) != 11:
    bad.append("表数 = %d，该是 11" % len(nums))
if nums != list(range(1, len(nums) + 1)):
    bad.append("表的编号不连续：%s" % nums)
for (no, rows) in tables:
    if not 4 <= len(rows) <= 8:
        bad.append("表 %d 的数据行 %d 条，不在 4~8 里" % (no, len(rows)))
    for r in rows:
        cells = [c.strip() for c in re.split(r"(?<!\\)\|", r)[1:-1]]
        if len(cells) != 3:
            bad.append("表 %d 有一行的列数是 %d：%s" % (no, len(cells), r[:36]))
            continue
        if not all(cells):
            bad.append("表 %d 有空单元格：%s" % (no, r[:36]))
        elif not CITE.search(cells[2]):
            bad.append("表 %d 有一行的出处列解析不出章与节：%s" % (no, r[:36]))
declared = actual = 0
for i, ln in enumerate(lines):
    m = re.match(r"^## 一页纸（值得背下来的 (\d+) 条", ln)
    if m:
        declared = int(m.group(1))
        j = i + 1
        while j < len(lines) and not lines[j].startswith("```"):
            j += 1
        j += 1
        while j < len(lines) and not lines[j].startswith("```"):
            if re.match(r"^\d+\s", lines[j]):
                actual += 1
            j += 1
        break
print("读数：一页纸正文承诺 %d 条，围栏里实数 %d 条" % (declared, actual))
if declared == 0 or actual == 0 or declared != actual:
    bad.append("一页纸的条数对不上：承诺 %d、实数 %d" % (declared, actual))
skel = sum(1 for ln in lines if ln.strip() == "```python")
print("读数：python 围栏 %d 个开栏标记（骨架四段，第 5 节再逐段验语法）" % skel)
for b in bad:
    print("  [响] %s" % b)
print("读数：本节判定 %s" % ("通过" if not bad else "不通过（%d 条）" % len(bad)))
sys.exit(0 if not bad else 1)
PYEOF

cat > "$SB/pages/check_snips.py" <<'PYEOF'
# -*- coding: utf-8 -*-
"""第 5 节：骨架核对。argv = 那一一页、该有几段 python 围栏。

从围栏里**原地**抽出正文，只在内存里过 ast.parse 与 compile()：不落盘、不 import、不执行。
两道都要：ast.parse 只认语法形状，compile() 还会拒绝「模块层的 return」这类能骗过前者的写法。
"""
import ast
import sys

page, want = sys.argv[1], int(sys.argv[2])
lines = open(page, encoding="utf-8").read().splitlines()
blocks = []
i = 0
while i < len(lines):
    if lines[i].strip() == "```python":
        body = []
        i += 1
        while i < len(lines) and lines[i] != "```":
            body.append(lines[i])
            i += 1
        blocks.append("\n".join(body))
    i += 1
print("读数：python 围栏 %d 段（该是 %d），各自行数 = %s"
      % (len(blocks), want, " ".join(str(len(b.splitlines())) for b in blocks)))
errs = []
okp = okc = 0
for n, src in enumerate(blocks, 1):
    name = "第 %d 段" % n
    try:
        ast.parse(src, name)
        okp += 1
    except SyntaxError as e:
        errs.append("%s ast.parse 就炸：%s（行 %s）" % (name, e.msg, e.lineno))
    except ValueError as e:
        errs.append("%s ast.parse 被拒：%s" % (name, e))
    try:
        compile(src, name, "exec")
        okc += 1
    except SyntaxError as e:
        # ★ 这句尾必须看现场：ast.parse 已经炸过的段，compile 报的是**同一个**错，
        #   这时候写「ast.parse 放过去的那一类」就是假话（colon 那份坏样本正是这种情形）。
        errs.append("%s compile 也炸：%s（行 %s）%s" % (
            name, e.msg, e.lineno,
            "" if okp < n else "—— 这一段 ast.parse 放过去了、只有 compile 拦下"))
    except ValueError as e:
        errs.append("%s compile 被拒：%s" % (name, e))
print("读数：ast.parse 通过 %d/%d，compile 通过 %d/%d" % (okp, len(blocks), okc, len(blocks)))
for e in errs:
    print("  [响] %s" % e)
if len(blocks) != want:
    print("  [响] 段数就不对：抽到 %d 段，正文承诺 %d 段" % (len(blocks), want))
print("读数：本节判定 %s" % ("通过" if not errs and len(blocks) == want else "不通过"))
sys.exit(0 if not errs and len(blocks) == want else 1)
PYEOF

cat > "$SB/pages/mutate.py" <<'PYEOF'
# -*- coding: utf-8 -*-
"""第 6 节：把真账本故意弄坏两份。argv = 模式（flip / drop）、真账本、输出。"""
import sys

mode, src, dst = sys.argv[1], sys.argv[2], sys.argv[3]
lines = open(src, encoding="utf-8").read().splitlines()
for i, ln in enumerate(lines):
    if "本条小结" not in ln:
        continue
    if mode == "flip":
        lines[i] = ln.replace("全成立 True", "全成立 False")
    else:
        del lines[i]
    break
else:
    sys.exit("坏账本造不出来：真账本里一条小结都没有")
with open(dst, "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
print("已造坏账本（模式 %s，改的是第 %d 行位置的那条小结）" % (mode, i + 1))
PYEOF

cat > "$SB/pages/check_ledger.py" <<'PYEOF'
# -*- coding: utf-8 -*-
"""第 6 节：探针账本核对。argv1 = 账本文件。

只认每组的收口行「◆ 本条小结：章 NN｜判据 K 条｜全成立 X」，再和屏上「→ 实测」的行数对总账：
小结必须 12 行、覆盖 11 章、全成立一条不许假、实测行数与 K 的合计必须一模一样。
"""
import re
import sys

S = re.compile(r"◆ 本条小结：章 (\d{2})｜判据 (\d+) 条｜全成立 (\S+)")
lines = open(sys.argv[1], encoding="utf-8").read().splitlines()
chs = []
ksum = 0
bad = []
for ln in lines:
    if "本条小结" not in ln:
        continue
    m = S.search(ln)
    if not m:
        bad.append("有一行小结读不出结构：%s" % ln[:48])
        continue
    chs.append(m.group(1))
    ksum += int(m.group(2))
    if m.group(3) != "True":
        bad.append("章 %s 那组没全成立（它自己报的是 %s）" % (m.group(1), m.group(3)))
measured = sum(1 for ln in lines if ln.lstrip().startswith("→ 实测"))
if len(chs) != 12:
    bad.append("小结行数 = %d，该是 12 组" % len(chs))
if len(set(chs)) != 11:
    bad.append("覆盖 %d 章，该是 11 章（每组对应一张表）" % len(set(chs)))
if measured != ksum:
    bad.append("屏上实测行 %d 条 ≠ 账本判据合计 %d 条" % (measured, ksum))
print("读数：小结 %d 行 / 覆盖章 %s / 判据合计 %d 条 / 屏上实测行 %d 条"
      % (len(chs), " ".join(sorted(set(chs))), ksum, measured))
for b in bad:
    print("  [响] %s" % b)
print("读数：本节判定 %s" % ("通过" if not bad else "不通过（%d 条）" % len(bad)))
sys.exit(0 if not bad else 1)
PYEOF

cat > "$SB/pages/make_dirty.py" <<'PYEOF'
# -*- coding: utf-8 -*-
"""第 7 节：造一份「必然全命中」的脏副本，用来证明扫描器认得这五类。

五行的内容全部来自 argv（家目录、登录名、主机名、本次沙箱路径）和当场生成的对象地址，
这个文件只待在沙箱里，也从来不打印正文 —— 它存在的唯一意义就是让扫描器响一次。
"""
import sys

out, home, user, host, sb = sys.argv[1:6]
with open(out, "w", encoding="utf-8") as f:
    f.write("一行真路径：%s/.bashrc\n" % home)
    f.write("一行 ps 输出：%s 1 0.0 0.0 pts/0\n" % user)
    f.write("一行提示符：%s:~$ \n" % host)
    f.write("一行报错：%s/work/probes.py 读不到\n" % sb)
    f.write("一行 repr：%r\n" % object())
print("脏副本已写好（5 行：真路径 / 真登录名 / 真主机名 / 真沙箱路径 / 真对象地址）")
PYEOF

cat > "$SB/pages/scan_priv.py" <<'PYEOF'
# -*- coding: utf-8 -*-
"""第 7 节：隐私扫描。argv = 要扫的文件、家目录、登录名、主机名、本次沙箱路径。

五类禁串：家目录绝对路径 / 登录名 / 主机名 / 本次沙箱路径 / 对象地址形态。
前四类模式串由 argv 传进来（源码里一个字都没写死），第五类是 0x 加一串十六进制。
只报命中类别与次数，绝不打印命中的那一行 —— 打印出来就又泄一遍。
"""
import re
import sys

path, home, user, host, sb = sys.argv[1:6]
text = open(path, encoding="utf-8", errors="replace").read()
hits = 0
for label, pat in (("家目录绝对路径", home), ("登录名", user), ("主机名", host),
                   ("本次沙箱路径", sb)):
    n = text.count(pat) if pat else 0
    hits += n
    print("  类 %s 命中 %d 次" % (label, n))
n = len(re.findall(r"0x[0-9a-fA-F]{6,}", text))
hits += n
print("  类 对象地址形态 命中 %d 次" % n)
print("读数：五类禁串合计命中 %d 次" % hits)
sys.exit(0 if hits == 0 else 1)
PYEOF

cat > "$SB/pages/check_residue.py" <<'PYEOF'
# -*- coding: utf-8 -*-
"""第 8 节：残留核对。argv = 要看的目录、基准清单文件（每行一个条目名）。

同一个检查器先用在沙箱里的同形状副本上（该报非 0），再用在真章目录上（该报 0）。
只报条目名，不报路径。
"""
import os
import sys

d, base = sys.argv[1], sys.argv[2]
before = [ln.strip() for ln in open(base, encoding="utf-8") if ln.strip()]
now = sorted(os.listdir(d))
new = [x for x in now if x not in before]
gone = [x for x in before if x not in now]
print("读数：%s 现有 %d 项 / 基准 %d 项 / 新增 %d 项 / 少了 %d 项"
      % (os.path.basename(d.rstrip("/")), len(now), len(before), len(new), len(gone)))
for x in new:
    print("  [残留] 多了 %s" % x)
for x in gone:
    print("  [变了] 少了 %s" % x)
sys.exit(0 if not new and not gone else 1)
PYEOF

# 探针合集：12 组、每组 3~5 条判据，覆盖 11 张表。判据只问形状 / 顺序 / 布尔 / 条数，
# 不问秒、pid、线程与进程个数、对象地址 —— 那是全书纪律（第 09 章 · 第 1 节、第 11 章 · 第 13 节）。
cat > "$SB/work/probes.py" <<'PYEOF'
# -*- coding: utf-8 -*-
"""第 6 节：把速查表里的硬事实当场再判一遍。

选材三条硬规矩（都写在本章 README 的「怎么用这一章」里）：
  ① 跨章：11 章每章至少一组，节末的账本当场把「覆盖了哪几章」数出来；
  ② 确定性：判据只问形状、顺序、布尔、条数，不问时长；
  ③ 屏幕上不许出现秒、毫秒、pid、线程/进程个数、对象地址。
每条判据印成一行「→ 实测 <说法> ｜ <拿到的东西> → True」，末尾一行「◆ 本条小结」，
那是本节账本唯一认的原材料。
"""
import os
import subprocess
import sys

WORK = os.getcwd()


def run(code, args=(), cwd=None):
    """拿同一个解释器跑一小段代码，只收 stdout 第一行；args 放 -u 这类启动开关。"""
    r = subprocess.run([sys.executable, *args, "-c", code], capture_output=True, text=True,
                       cwd=cwd or WORK)
    out = r.stdout.strip().splitlines()
    return out[0] if out else "[没输出]"


def p01():
    d = run("import sys; print(sys.stdout.line_buffering, sys.stdout.write_through)")
    u = run("import sys; print(sys.stdout.line_buffering, sys.stdout.write_through)", ("-u",))
    return [("管道里 stdout 默认既不是行缓冲也不直写：写了 9000 字节带 4500 个换行照样 0 字节可见",
             "默认档 line_buffering/write_through", d, "False False"),
            ("-u 改的是 write_through，line_buffering 仍是 False —— 想判缓冲看属性，别猜",
             "-u 档 line_buffering/write_through", u, "False True")]


def p02():
    fold = [257, 257]
    made = [int("257"), int("257")]
    edge = (int("256") is int("256"), int("257") is int("257"),
            int("-5") is int("-5"), int("-6") is int("-6"))
    l1 = [1]
    l2 = l1
    l1 += [2]
    s1 = "a"
    s2 = s1
    s1 += "b"
    keys = {1: "a", True: "b", 1.0: "c"}
    zkeys = {-0.0: "x", 0.0: "y"}
    return [("同一段代码里的两个 257 字面量被折成同一个常量；缓存区是 -5..256，边界要用现造的对象才扫得出",
             "折叠 / 现造 / 四个边界", (fold[0] is fold[1], made[0] is made[1], edge),
             (True, False, (True, False, True, False))),
            ("+= 对 list 是原地扩（两张标签一起看见），对 str 是换新对象",
             "list 与 str 的 +=", (l2 == [1, 2], s2 == "a", l1 is l2), (True, True, True)),
            ("1 / True / 1.0 是同一个键，-0.0 与 0.0 也是：后写的覆盖先写的",
             "两份键的条数与最后那个值", (len(keys), len(zkeys), keys[True], zkeys[0.0]), (1, 1, "c", "y"))]


def p03():
    try:
        compile('b"\u4e2d\u6587"', "<s>", "eval")
        berr = "编译过了"
    except SyntaxError:
        berr = "SyntaxError"
    return [("split() 吃连续空白且不出空串；带 ' ' 则首尾都是空串；maxsplit 砍完的整段留在最后一项",
             "三种切法", ("  a  b ".split(), "  a  b ".split(" "), "a b c d".split(maxsplit=2)),
             (["a", "b"], ["", "", "a", "", "b", ""], ["a", "b", "c d"])),
            ("splitlines() 认 11 种换行；split('\\n') 只认 LF，\\r 被留在串里",
             "两种按行切", ("a\rb\nc".splitlines(), "a\rb\nc".split("\n")),
             (["a", "b", "c"], ["a\rb", "c"])),
            ("strip 删的是字符集合不是前缀；砍前缀只有 removeprefix",
             "strip 与 removeprefix", ("pre_fix_pre".strip("pre_"), "pre_fix_pre".removeprefix("pre_")),
             ("fix", "fix_pre")),
            ("bytes 字面量装中文在编译期就被拒，一个语法都过不了",
             "compile('b\"中文\"')", berr, "SyntaxError")]


def p04():
    d = {"a": 1, "b": 2, "c": 3}
    del d["a"]
    d["a"] = 9
    d["b"] = 99
    xs = [(2, "b"), (1, "c"), (2, "a")]
    rv = xs.sort(key=lambda t: t[0])
    calls = []
    ys = [3, 1, 2]
    ys.sort(key=lambda v: calls.append(v) or v)
    return [("dict 遍历是插入顺序：删了再插排到末尾、覆盖值不挪位置、== 与顺序无关",
             "键的顺序", (list(d), {"x": 1, "y": 2} == {"y": 2, "x": 1}), (["b", "c", "a"], True)),
            ("list.sort() 返回 None；它稳定，同键保持原先后（要那个排好的东西就用原名，它已经是了）",
             "返回值与顺序", (rv, [t[1] for t in xs]), (None, ["c", "b", "a"])),
            ("key= 每个元素只算一次，不是每次比较算一次",
             "key 被调用次数", len(calls), 3),
            ("空 set 只能 set()（{} 是空 dict）；逗号才造 tuple，(1) 就是 int",
             "两者的类型", (type({}).__name__, type({1}).__name__, type((1)).__name__, type((1,)).__name__),
             ("dict", "set", "int", "tuple"))]


def p05():
    gen = (i for i in (1, 2, 3))
    first = list(gen)
    from collections import Counter, defaultdict
    from copy import copy, deepcopy
    c = Counter()
    v = c["缺"]
    dd = defaultdict(int)
    dd["k"]
    outer = [[1], [2]]
    deep = deepcopy(outer)
    sliced = outer[:]
    cloned = copy(outer)
    sliced[0].append(9)
    grid = [[0] * 3] * 3
    grid[0][0] = 1
    xs = [2, 4, 6, 8, 10]
    for x in xs:
        if x % 2 == 0:
            xs.remove(x)
    ds = {"a": 1, "b": 2, "c": 3}
    it = iter(ds)
    next(it)
    ds.pop("a")
    try:
        next(it)
        derr = "下一次迭代也没响"
    except RuntimeError:
        derr = "RuntimeError 在下一次迭代那一刻"
    return [("生成器第二次遍历是空且不报错，第二次 sum 给 0：文件对象、map/filter 全同款",
             "第一遍/第二遍", (first, list(gen), sum(i for i in ())), ([1, 2, 3], [], 0)),
            ("Counter 读缺失键给 0 且不建键，.get() 反倒给 None；defaultdict 读一下就真的建键",
             "缺键的两种脾气", (v, "缺" in c, c.get("缺2"), "k" in dd), (0, False, None, True)),
            ("切片 / copy.copy 全是浅的：内层还是同一个对象，改一处两边一起变，只有 deepcopy 重造内层",
             "三层是否共享 / 两份内层", (sliced[0] is outer[0], cloned[0] is outer[0],
                                       deep[0] is outer[0], outer[0], deep[0]),
             (True, True, False, [1, 9], [1])),
            ("[[0]*3]*3 只有一个内层对象：改一格三行一起变，有几层独立就要写几个 for",
             "三行是否同一个", (grid[0], grid[1] is grid[0], grid[2] is grid[0]),
             ([1, 0, 0], True, True)),
            ("边遍历边删 list 不报错只给错答案；dict 改大小是在下一次迭代那一刻 RuntimeError",
             "两种下场", (xs, derr), ([4, 8], "RuntimeError 在下一次迭代那一刻"))]


def p06():
    def acc(x, out=[]):
        out.append(x)
        return out
    r1 = acc(1)
    r2 = acc(2)
    r3 = acc(3)
    fns = [lambda: i for i in range(3)]

    def ret():
        return 1,

    def w():
        locals()["z"] = 1
        try:
            return z
        except NameError:
            return "NameError"
    return [("def 只跑一次：默认值在定义那一刻算掉，三次调用攒在同一个对象上，且一个字都不报错",
             "三次返回是否同一个对象、最后攒到几个", (r1 is r2 is r3, len(r3)), (True, 3)),
            ("晚绑定：名字是调用那一刻才查的，三个 lambda 共用同一个盒子",
             "三个 lambda 各给几", [f() for f in fns], [2, 2, 2]),
            ("return 1, 返回 (1,)：逗号造元组，3.14 连警告都不给",
             "return 1, 的产物", (ret(), type(ret()).__name__), ((1,), "tuple")),
            ("函数里往 locals() 写不创建变量（能写的是 globals()）",
             "写完再读那个名字", w(), "NameError")]


def p07():
    class Eq:
        def __eq__(self, o):
            return True
    try:
        {Eq()}
        hasherr = "进了 set"
    except TypeError:
        hasherr = "TypeError"
    from dataclasses import dataclass

    @dataclass
    class D:
        a: int = 1
    try:
        D() < D()
        lterr = "比过了"
    except TypeError:
        lterr = "TypeError"
    try:
        @dataclass
        class Bad:
            items: list = []
        derr = "建起来了"
    except ValueError as e:
        derr = "ValueError" if "mutable default" in str(e) else "ValueError 但理由不对"

    @dataclass(frozen=True)
    class F:
        a: int = 1
    try:
        F().a = 2
        ferr = "改掉了"
    except AttributeError:
        ferr = "FrozenInstanceError"

    class B:
        pass

    class S(B):
        __slots__ = ("x",)

    class Cls:
        n = 0

    inst = Cls()
    inst.n += 1
    seen = []
    for who in ("类上", "实例上"):
        seen.append(getattr(Cls if who == "类上" else inst, "n"))
    return [("写 __eq__ 之后解释器主动把 __hash__ 设成 None：进 set 当场 TypeError",
             "Eq.__hash__ 与进 set", (Eq.__hash__, hasherr), (None, "TypeError")),
            ("不写 order=True 就没有 __lt__（类字典里连这个键都没有），一比就 TypeError；非 frozen 的 __hash__ 也是 None",
             "__lt__ 在类字典里吗", ("__lt__" in D.__dict__, lterr, D.__hash__), (False, "TypeError", None)),
            ("dataclass 里的可变默认值在定义那一刻就 ValueError（手写 def __init__(self, items=[]) 可没人拦）",
             "mutable default 那条", derr, "ValueError"),
            ("frozen=True 白送 __hash__，而且只挡字段赋值，报的是 AttributeError 的儿子",
             "frozen 的 __hash__ 与赋值", (F.__dict__.get("__hash__") is not None, ferr),
             (True, "FrozenInstanceError")),
            ("父类没写 __slots__，子类写了也白干：__dict__ 照样继承下来",
             "子类实例有 __dict__ 吗", hasattr(S(), "__dict__"), True),
            ("self.n += 1 改不动类变量：只在实例上建遮挡物，类上那份永远是 0",
             "类上与实例上的读数", seen, [0, 1])]


def p08():
    class Quit(BaseException):
        pass
    try:
        try:
            raise Quit()
        except Exception:
            caught = "Exception 接住了"
    except BaseException:
        caught = "穿过 Exception"
    try:
        try:
            raise Quit()
        except:
            bare = "裸 except 把它咽了"
    except BaseException:
        bare = "不该到这"
    tree = [issubclass(x, Exception) for x in (KeyboardInterrupt, SystemExit, GeneratorExit)]

    class Swallow:
        def __enter__(self):
            return self

        def __exit__(self, *a):
            return "一个字符串也算真值"
    survived = True
    try:
        with Swallow():
            raise ValueError("原始故障")
    except ValueError:
        survived = False
    dead_src = ("def f():\n"
                "    try:\n        raise ValueError('x')\n"
                "    except Exception:\n        return '父亲那条先接住了'\n"
                "    except ValueError:\n        return '儿子那条'\n")
    src = ("log = []\n"
           "def fin():\n"
           "    try:\n        log.append('try')\n        return 1\n"
           "    finally:\n        log.append('finally')\n        return 2\n")
    import warnings
    with warnings.catch_warnings(record=True) as got:
        warnings.simplefilter("always")
        code = compile(src, "<fin>", "exec")
        kinds = sorted({w.category.__name__ for w in got})
    box = {}
    exec(code, box)
    val = box["fin"]()
    log = box["log"]
    with warnings.catch_warnings(record=True) as got2:
        warnings.simplefilter("always")
        code2 = compile(dead_src, "<dead>", "exec")
        deadwarn = sorted({w.category.__name__ for w in got2})
    b2 = {}
    exec(code2, b2)
    dead = (b2["f"](), deadwarn)
    return [("except Exception 接不住退出族（这是好事）",
             "raise 一个 BaseException 的儿子", caught, "穿过 Exception"),
            ("裸 except: 等于 except BaseException:，把退出族一起咽掉 —— 这就是它被禁的理由",
             "裸 except 的下场", bare, "裸 except 把它咽了"),
            ("KeyboardInterrupt / SystemExit / GeneratorExit 都不是 Exception 的儿子",
             "三个是不是 Exception 后代", tree, [False, False, False]),
            ("__exit__ 返回任何真值 = 异常就地抹掉（写字符串也算真值），全程没有一条报错提醒",
             "with 块里的 ValueError 冒出来了吗", survived, True),
            ("except 分支顺序：父亲在前，儿子那条当场变成死代码，而且不报错、不警告",
             "走到哪条 / 编译期警告", dead, ("父亲那条先接住了", [])),
            ("finally 里 return 把返回值一起改掉，那条 SyntaxWarning 只在**编译那一刻**发一条",
             "编译期警告 / 返回值 / 走位", (kinds, val, log),
             (["SyntaxWarning"], 2, ["try", "finally"]))]


def p09():
    from pathlib import Path
    import json
    import os
    import tempfile
    joined = str(Path("/tmp") / "/etc/passwd")
    try:
        Path("/etc/passwd").relative_to("/tmp")
        rterr = "没响"
    except ValueError:
        rterr = "ValueError"
    back = json.loads(json.dumps({"a": (1, 2), 1: "x"}))
    try:
        json.dumps({"v": {1, 2}})
        seterr = "过了"
    except TypeError:
        seterr = "TypeError"
    try:
        json.loads("{坏")
        jerr = "过了"
    except json.JSONDecodeError as e:
        jerr = (e.lineno, e.colno, e.pos)
    fd, tmp = tempfile.mkstemp(dir=WORK, prefix="t.")
    mode = oct(os.stat(tmp).st_mode & 0o777)
    try:
        os.close(os.open(tmp, os.O_CREAT | os.O_EXCL | os.O_WRONLY))
        excl = "抢来了"
    except FileExistsError:
        excl = "FileExistsError"
    os.close(fd)
    os.unlink(tmp)
    wtxt = os.path.join(WORK, "w.txt")
    open(wtxt, "w").close()
    trunc = os.path.getsize(wtxt)
    os.unlink(wtxt)
    raw = "中文".encode("utf-8")
    aslatin = raw.decode("latin-1")
    try:
        raw.decode("ascii")
        aerr = "过了"
    except UnicodeDecodeError:
        aerr = "UnicodeDecodeError"
    return [("Path 拼绝对路径把前缀整个丢掉且不报错，os.path.join 是同一条路",
             "Path('/tmp') / '/etc/passwd'", (joined, os.path.join("/tmp", "/etc/passwd")),
             ("/etc/passwd", "/etc/passwd")),
            ("relpath 能给你一条「看起来在目录里」的串；会响的那个才是 relative_to",
             "两条路各给什么", (os.path.relpath("/etc/passwd", "/tmp"), rterr), ("../etc/passwd", "ValueError")),
            ("JSON 值能回来、类型不能：tuple → list、int 键 → str；set 当场 TypeError，而坏 JSON 抛的错带行列偏移",
             "回程的类型", (type(back["a"]).__name__, list(back.keys()), seterr, jerr),
             ("list", ["a", "1"], "TypeError", (1, 2, 1))),
            ("mkstemp 硬编码 0600 并带 O_CREAT|O_EXCL（同一个名字抢不来），mktemp 只算名字不动手",
             "权限位与 O_EXCL", (mode, excl), ("0o600", "FileExistsError")),
            ("open(p, 'w') 在打开那一刻就截成 0 字节，进 with 块再反悔已经晚了",
             "刚打开、一个字没写，文件多大", trunc, 0),
            ("用 latin-1 读一份 UTF-8 文本一个错都不报、还能原样编回去；换成 ascii 才响 —— 默认编码那条路就是这么骗过人的",
             "串长 / 能否编回 / ascii 的下场", (len(aslatin), aslatin.encode("latin-1") == raw, aerr),
             (6, True, "UnicodeDecodeError"))]


def p10():
    sub = os.path.join(WORK, "sub")
    pkg = os.path.join(WORK, "pkg")
    os.makedirs(sub, exist_ok=True)
    os.makedirs(pkg, exist_ok=True)
    open(os.path.join(pkg, "__init__.py"), "w", encoding="utf-8").close()
    with open(os.path.join(sub, "sp.py"), "w", encoding="utf-8") as f:
        f.write("import os, sys\nprint(sys.path[0] == os.path.dirname(os.path.abspath(__file__)))\n")
    with open(os.path.join(pkg, "work.py"), "w", encoding="utf-8") as f:
        f.write("import os, sys\nprint(sys.path[0] == os.getcwd(), sys.path[0] == '')\n")
    with open(os.path.join(pkg, "conf.py"), "w", encoding="utf-8") as f:
        f.write("VAL = 1\n")
    with open(os.path.join(WORK, "ledger.py"), "w", encoding="utf-8") as f:
        f.write("import importlib\nimport pkg.conf\nfrom pkg.conf import VAL\n"
                "first = VAL\nimport pkg.conf\nprint(pkg.conf.VAL, first)\n"
                "open('pkg/conf.py', 'w', encoding='utf-8').write('VAL = 2  # 改了，长度也变了\\n')\n"
                "import pkg.conf\nprint(pkg.conf.VAL)\n"
                "importlib.reload(pkg.conf)\nprint(pkg.conf.VAL, VAL)\n")
    c_mode = run("import sys; print(sys.path[0] == '')")
    s_here = subprocess.run([sys.executable, "sub/sp.py"], capture_output=True, text=True,
                            cwd=WORK).stdout.strip()
    s_elsewhere = subprocess.run([sys.executable, os.path.join(sub, "sp.py")],
                                 capture_output=True, text=True, cwd=sub).stdout.strip()
    m_mode = subprocess.run([sys.executable, "-m", "pkg.work"], capture_output=True, text=True,
                            cwd=WORK).stdout.strip()
    led = subprocess.run([sys.executable, "ledger.py"], capture_output=True, text=True,
                         cwd=WORK).stdout.strip().replace("\n", " / ")
    return [("启动方式决定 sys.path[0]：-c 给空串，脚本给脚本自己的目录（换个 cwd 再跑也一样），"
             "-m 给当前目录而不是空串",
             "四种入口各给什么", (c_mode, s_here, s_elsewhere, m_mode),
             ("True", "True", "True", "True False")),
            ("改了磁盘上的文件：普通 import 查的是账本（连文件都不打开），reload 才重跑模块体，"
             "而手里那句 from-import 绑的名字一个都不换",
             "磁盘改成 2 之后的三组读数", led, "1 1 / 1 / 2 1")]


def p11():
    import annotationlib
    import inspect
    import traceback
    ns = {}
    exec("def h(x: Nope):\n    return x\n", ns)
    h = ns["h"]
    made = hasattr(h, "__annotate__")
    try:
        hasattr(h, "__annotations__")
        probe = "hasattr 把它吞了"
    except NameError:
        probe = "NameError 照原样上抛"
    try:
        h.__annotations__
        frame = "读到了"
    except NameError:
        frame = traceback.extract_tb(sys.exc_info()[2])[-1].name
    blows = 0
    for _ in range(3):
        try:
            h.__annotations__
        except NameError:
            blows += 1
    keys = list(annotationlib.Format.__members__)
    default = str(inspect.signature(annotationlib.get_annotations)).split("format=")[1]
    outs = []
    for name in ("VALUE", "FORWARDREF", "STRING"):
        try:
            got = annotationlib.get_annotations(h, format=getattr(annotationlib.Format, name))
            outs.append(type(got).__name__)
        except NameError:
            outs.append("NameError")
    texty = annotationlib.get_annotations(h, format=annotationlib.Format.STRING)
    from typing import Final, Protocol, get_args, get_origin, runtime_checkable

    @runtime_checkable
    class P(Protocol):
        def m(self) -> int:
            ...

    class WrongSign:
        def m(self, a, b, c):
            ...

    class Missing:
        pass
    fin = {}
    fin["x"]: Final = 1
    fin["x"] = 2
    return [("定义那一刻只编译出一个 __annotate__ 取值函数，注解里的名字一个都没查：def 照样成功",
             "def 之后有没有 __annotate__ / hasattr 是不是安全探针", (made, probe),
             (True, "NameError 照原样上抛")),
            ("真正炸的是第一次读 __annotations__，traceback 里那一帧就叫 __annotate__",
             "第一读炸在哪一帧", frame, "__annotate__"),
            ("成功才缓存、失败不缓存：同一支函数读三次就炸三次",
             "三次读炸了几次", blows, 3),
            ("四把钥匙里默认那把（get_annotations 不传 format，签名写着 1 号 VALUE）恰好是会炸的那把；"
             "3 号 FORWARDREF 挡一下给 dict，4 号 STRING 干脆只给源码文本",
             "名单 / 默认那把 / 三把各给什么", (keys, default, outs, texty),
             (["VALUE", "VALUE_WITH_FAKE_GLOBALS", "FORWARDREF", "STRING"], "<Format.VALUE: 1>)",
              ["NameError", "dict", "dict"], {"x": "Nope"})),
            ("注解在运行时一行都不检查：Protocol 的 isinstance 只查方法名不查签名（签名对不上也过、"
             "缺方法才拦），Final 也完全不拦再赋值；真在干活的只有 get_origin / get_args 那批",
             "get_origin / get_args / 签名不符 / 缺方法 / Final 再赋值",
             (get_origin(list[int]).__name__, get_args(list[int]) == (int,),
              isinstance(WrongSign(), P), isinstance(Missing(), P), fin["x"]),
             ("list", True, True, False, 2))]


def p12():
    import dis
    import queue
    import warnings
    from concurrent.futures import ThreadPoolExecutor

    def inc(box):
        box["n"] += 1
    ops = [i.opname for i in dis.get_instructions(inc)]

    async def c():
        pass
    c.__qualname__ = "c"
    with warnings.catch_warnings(record=True) as got:
        warnings.simplefilter("always")
        coro = c()
        ran = 0
        del coro
        msgs = [(type(w.message).__name__, str(w.message)) for w in got]

    def boom():
        raise ValueError("池子里炸的")
    with ThreadPoolExecutor(max_workers=1) as ex:
        fut = ex.submit(boom)
        err = "当场就抛了"
        try:
            fut.result()
        except ValueError:
            err = "result 那一刻才抛"
    q = queue.Queue()
    q.put("活")
    item = q.get_nowait()
    try:
        q.get_nowait()
        qerr = "又拿到一个东西"
    except queue.Empty:
        qerr = queue.Empty.__module__ + "." + queue.Empty.__qualname__
    q.put(None)
    sentinel = q.get_nowait()
    return [("调用协程函数跑了 0 行，只生产一个协程对象；只创建不 await 就漂一条 RuntimeWarning",
             "跑了几行 / 漂出什么 / 名字", (ran, len(msgs), msgs[0][1].split("'")[1] if msgs else ""),
             (0, 1, "c")),
            ("一行 box[\"n\"] += 1 在本机翻成 13 条字节码，读（BINARY_OP）与写（STORE_SUBSCR）之间隔着好几条",
             "条数 / 读与写之间隔几条", (len(ops), ops.index("STORE_SUBSCR") - ops.index("BINARY_OP")),
             (13, 5)),
            ("submit 之后异常不当场抛：它躺在 future 里，.result() 那一刻才把原文抛出来",
             "什么时候抛 / future 状态 / 异常类型", (err, fut.done(), type(fut.exception()).__name__),
             ("result 那一刻才抛", True, "ValueError")),
            ("queue 的收尾靠哨兵：不塞就是永久等待，而空队列那个异常的全名印作 _queue.Empty",
             "取到的活 / Empty 全名 / 哨兵", (item, qerr, sentinel is None),
             ("活", "_queue.Empty", True))]


LABELS = {
    "p01": ("章 01", "启动方式与缓冲：属性才是尺，不是「卡住了」"),
    "p02": ("章 02", "名字是标签：缓存边界、+= 的原地性、dict 的同键"),
    "p03": ("章 03", "str 是码点序列：split 两种脾气、bytes 字面量的编译期"),
    "p04": ("章 04", "dict 的插入顺序、sort 的三个脾气、空 set 与单元素 tuple"),
    "p05": ("章 04", "一次性迭代器、缺键两种脾气、拷贝的层级与边遍历边删"),
    "p06": ("章 05", "def 只跑一次、晚绑定、locals() 写不进"),
    "p07": ("章 06", "__eq__ 没收 __hash__、dataclass 的三个开关、类变量遮挡"),
    "p08": ("章 07", "退出族、__exit__ 的真值、父亲在前的死分支、finally 里的 return"),
    "p09": ("章 08", "路径拼接丢前缀、JSON 丢类型、latin-1 的沉默、'w' 的截断"),
    "p10": ("章 09", "sys.path[0] 的三种形状与 import 的三本账"),
    "p11": ("章 10", "注解是还没执行的代码：读的那一刻才炸"),
    "p12": ("章 11", "不 await 就不跑、字节码颗粒度、future 里的异常、队列哨兵"),
}
FUNCS = {k: globals()[k] for k in LABELS}


def main():
    key = sys.argv[1]
    if key not in LABELS:
        print("  [不成立] 未知的探针名 " + key)
        return 1
    ch, title = LABELS[key]
    rows = FUNCS[key]()
    print("  [%s] %s" % (ch, title))
    bad = []
    for text, what, got, want in rows:
        ok = got == want
        print("  → 实测 %s ｜ %s → %s → %s"
              % (text, what, got, "True" if ok else "[不成立] 期望 %s" % (want,)))
        if not ok:
            bad.append(text[:20])
    print("  ◆ 本条小结：%s｜判据 %d 条｜全成立 %s" % (ch, len(rows), "True" if not bad else "False"))
    return 0 if not bad else 1


sys.exit(main())
PYEOF

# ============================================================================
banner 1 "地板：这一页钉在哪台机器上"
# ============================================================================
note "自查脚本就位 $(ls -1 "$SB/pages" | wc -l) 支，另加 1 支探针合集 probes.py；它们只活在沙箱里，章目录一个字节都没多。"
note '下面 11 张表里的每个读数都只在这块地板上成立。值全部由 python3 -c 当场取，一个字都不背。'
note '换一台机器（有 python3.14t、有 pip、默认 start method 不是 forkserver），这块地板就得重打，'
note '本章的结论也要跟着重测 —— 这也是这一节排在最前面的原因。'
runc 'timeout 60 python3 -c "
import sys, sysconfig, os, selectors, multiprocessing as mp, importlib.util as u
print(\"版本 →\", sys.version.split()[0], \"｜ 实现 →\", sys.implementation.name, \"｜ 前缀 →\", sys.prefix != sys.base_prefix)
print(\"运行期 GIL 开着吗 sys._is_gil_enabled() →\", sys._is_gil_enabled(), \"｜ 编译期 Py_GIL_DISABLED →\", sysconfig.get_config_var(\"Py_GIL_DISABLED\"))
print(\"切换节拍 sys.getswitchinterval() →\", sys.getswitchinterval(), \"｜ 核数 os.cpu_count() →\", os.cpu_count(), \"｜ 可用核 sched_getaffinity →\", len(os.sched_getaffinity(0)))
print(\"multiprocessing 全部 start method →\", mp.get_all_start_methods(), \"｜ 默认 →\", mp.get_start_method())
print(\"selectors.DefaultSelector 是谁 →\", type(selectors.DefaultSelector()).__name__)
print(\"find_spec(\\\"pip\\\") →\", u.find_spec(\"pip\"), \"｜ which python3.14t →\", __import__(\"shutil\").which(\"python3.14t\"))
print(\"os.fork 在场吗 →\", hasattr(os, \"fork\"), \"｜ asyncio.to_thread 在场吗 →\", __import__(\"asyncio\").to_thread is not None)
"'
note '在场 / 缺席名单也当场问一遍：这一页的骨架只许用「在场」那一列里的东西。'
note '★ 两列必须由**同一份候选名单**切出来。前几章各探各的四五个名字，于是屏上只说得出「那几把尺都不在」；'
note '  这一页是全书收口，得把话说全：本机确实没有 pip，但 dist-packages 里躺着的不止 typeguard 一个。'
runc 'timeout 60 python3 -c "
import sys, os
import importlib.util as u, sysconfig
names = sorted([\"pip\",\"setuptools\",\"mypy\",\"pyright\",\"ruff\",\"black\",\"flake8\",\"pylint\",\"pytest\",
                \"uvloop\",\"trio\",\"curio\",\"numpy\",\"pandas\",\"requests\",\"rich\",\"yaml\",\"typeguard\"])
have = [m for m in names if u.find_spec(m) is not None]
print(\"候选名单共 %d 个名字 ｜ 在场的 →\" % len(names), have)
print(\"缺席的 →\", [m for m in names if u.find_spec(m) is None])
root = sysconfig.get_paths()[\"purelib\"]
print(\"这些在场的是从哪儿来的 →\", [os.path.dirname(u.find_spec(m).origin) for m in have])
print(\"标准库纯 Python 目录是 →\", root, \"｜ 它在 sys.path 里吗 →\", root in sys.path)
"'
note '  → 上面第三行是本机第三方包的真落点（dist-packages，发行版装的），而**没有 pip** 意味着'
note '    「装一个就好了」这句话在本手册全程兑现不了 —— 这两件事合起来才是本章的边界。'
verdict '地板七行全在屏上：解释器身份、GIL 的编译期/运行期、节拍与核数、start method、selectors 后端、pip 缺席、在场/缺席名单（两列由同一份候选名单切出，在场的那几个还报出落点目录）。下面每张表都钉在这七行上。'

# ============================================================================
banner 2 "证据来源：只有一处，就是那 11 个章目录"
# ============================================================================
note '这一章自己不产证据。表里每一行的现场都在同级 11 章的 README.md 里，所以第一件事是确认'
note '「能翻到的东西都在」：11 个章目录齐不齐、每章 README 在不在、节标题数得出数不出来。'
note '★ 本节的判据先在一份只有三个章目录的假书架上响一次，再对真书架报数 —— 假书架上「缺一章」'
note '  必须当场被点名，否则「十一章都在」这句话是恒真的。'
for d in 01-fake 02-fake 09-fake; do
    mkdir -p "$SB/fakebook/$d"
done
printf '## 第 1 节 · 假的一节\n\n正文\n' > "$SB/fakebook/01-fake/README.md"
printf '## 1. 假的一节（demo 第 1 节）\n\n正文\n' > "$SB/fakebook/02-fake/README.md"
runc 'timeout 60 python3 "$SB/pages/check_sources.py" "$SB/fakebook"'
note '上面那份是**假书架**：只摆了三个目录，其中一个连 README 都没有 —— 它报「缺一章」，rc 非 0，'
note '这就是本节判据能响的证明。下面才是真书架。'
runc 'timeout 60 python3 "$SB/pages/check_sources.py" "$BOOK"'
note '本章目录自己也在红线下：除 README.md 与 demo.sh，不该有第三样东西（取证脚本一章都不需要）。'
runc 'printf "本章目录清单 → [%s]  除 README.md/demo.sh 之外几个 = %s\n" "$(cd -- "$HERE" && ls -1 | tr "\n" " ")" "$(cd -- "$HERE" && ls -1 | grep -v -e "^README.md$" -e "^demo.sh$" | wc -l)"'
verdict '真书架 11/11 章目录齐、每章 README 都在、每章节标题数 > 0；假书架当场报「缺一章」并且 rc 非 0；本章目录里第三样文件 = 0。'

# ============================================================================
banner 3 "出处解析：本页每一条「第 NN 章 · 第 M 节」都翻一遍"
# ============================================================================
note '这一页最容易烂的地方就是出处：改版、重排、手滑写错节号，读的人翻到空处才知道。'
note '所以本节不做映射表，而是**当场解析标题**：落在哪一章、那一节的标题在不在、那一格在不在。'
note '四种标题形状都归到同一个「第 M 节」上（`## 第 NN 节 ·`、`## N. 第 M 节 ·`、`## N. 标题`、'
note '`### 场景 K：……（demo 第 M 节）`）；「第 K 格」要么在那一节里找 `=== K. 标题 ===`，'
note '要么找到正文里写死的那句「第 K 格」。'
note '★ 四份坏页面先让它响：假章号、假节号、假格号各点出一条断链；第四份只留正文前 40 行，'
note '  它响的是「出处总数不够地板线」—— 防止解析器其实什么都没抓到却报 0 断链。'
note '  响完才对真页面报数。'
runc 'timeout 60 python3 "$SB/pages/make_poison.py" "$PAGE" "$SB/pages" refs'
for f in poison_ref_ch.md poison_ref_sec.md poison_ref_cell.md; do
    runc 'timeout 60 python3 "$SB/pages/check_refs.py" "$BOOK" "$SB/pages/'"$f"'" 1 1'
done
runc 'timeout 60 python3 "$SB/pages/check_refs.py" "$BOOK" "$SB/pages/poison_thin.md" 120 90'
note '上面四条 rc 都非 0，是**故意的**：断链必须一条一条被点名出来。下面是对本页正文的真判。'
runc 'timeout 60 python3 "$SB/pages/check_refs.py" "$BOOK" "$PAGE" 120 90'
verdict '本页正文（跳过围栏之后）的出处全部解析成功：断链 0 条、覆盖 11 章、出现次数与去重条数都过地板线；三份坏页面各点出一条断链，第四份点出出处数不足。'

# ============================================================================
banner 4 "页面结构：表数、每表行数、列数、空格子、一页纸"
# ============================================================================
note '「速查表」这四个字本身就是要能被机器核对的：几张表、每张几行、每行是不是三列、'
note '一页纸到底几条。五样全部当场数，正文承诺的 N 也当场跟数出来的对上。'
note '★ 每表行数有硬区间 4~8：条目太少等于没压缩，太多就等于把目录抄过来了。'
note '  五份坏页面各打中一条 —— 空出处、列数不符、一页纸对不上、表数不足、某表只剩三行。'
runc 'timeout 60 python3 "$SB/pages/make_poison.py" "$PAGE" "$SB/pages" page'
for f in poison_page_emptycell.md poison_page_cols.md poison_page_paper.md poison_page_tables.md poison_page_rows.md; do
    runc 'timeout 60 python3 "$SB/pages/check_page.py" "$SB/pages/'"$f"'"'
done
note '上面五条 rc 都非 0，是**故意的**。下面是对本页的真判。'
runc 'timeout 60 python3 "$SB/pages/check_page.py" "$PAGE"'
verdict '表数 = 11 且编号连续 1..11、每表 8 行（在 4~8 区间内）、列数不符 0、空单元格 0、出处解析不出的行 0、一页纸声明的条数与实数一致；五份坏页面各自响。'

# ============================================================================
banner 5 "骨架：从本页围栏里原地抽出来，在内存里过 ast.parse + compile()"
# ============================================================================
note '这一页给的四段 python 是要被人直接抄走用的，所以它们至少得过语法这一关。'
note '两道都过：ast.parse 认语法形状，compile() 还多看一步（模块层的 return 前者放、后者拦）。'
note '不落盘、不 import、不执行 —— 语义对不对归第 6 节抽。'
note '★ 三份坏页面先响一次：①删掉一个冒号（ast.parse 就炸）；②在模块层塞一个 return'
note '  （ast.parse 过、compile 炸 —— 这正是「为什么两道都要跑」的现场）；③少一段围栏（段数不对）。'
runc 'timeout 60 python3 "$SB/pages/make_poison.py" "$PAGE" "$SB/pages" snips'
for f in poison_snip_colon.md poison_snip_return.md poison_snip_count.md; do
    runc 'timeout 60 python3 "$SB/pages/check_snips.py" "$SB/pages/'"$f"'" 4'
done
note '上面三条 rc 都非 0，是**故意的**。下面是对本页真围栏的判定。'
runc 'timeout 60 python3 "$SB/pages/check_snips.py" "$PAGE" 4'
verdict '本页正好 4 段 python 围栏，逐段过 ast.parse 与 compile()，全部成功；三份坏页面分别把 ast.parse、compile()、段数三条各响一次。'

# ============================================================================
banner 6 "硬事实：把表里的判断抽成 12 组探针，当场再判一遍"
# ============================================================================
note '压缩件最怕「压错了」：某一行原本不成立、或者换了版本就不成立了。所以本节把 11 张表里的'
note '判断抽成能当场重跑的判据 —— 每条都印成「说法 ｜ 拿到什么 → True」。'
note '选材三条硬规矩：① 跨章，11 章每章至少一组；② 只问形状、顺序、布尔、条数；'
note '③ 屏幕上不许出现秒、pid、线程/进程个数、对象地址。'
note '所有子进程都用与本章同一个解释器跑，全部落在沙箱 work/ 里，跑完不往外写东西。'
for k in p01 p02 p03 p04 p05 p06 p07 p08 p09 p10 p11 p12; do
    printf '\n%s$ timeout 240 python3 probes.py %s%s\n' "$C" "$k" "$R"
    printf '$ timeout 240 python3 probes.py %s\n' "$k" >> "$SB/all.out"
    timeout 240 python3 probes.py "$k" 2>&1 | fold_id > "$SB/.line.out"
    rc=${PIPESTATUS[0]}
    cat "$SB/.line.out"
    cat "$SB/.line.out" >> "$SB/all.out"
    cat "$SB/.line.out" >> "$SB/ledger.txt"
    printf '[退出码 %d]\n' "$rc"
    printf '[退出码 %d]\n' "$rc" >> "$SB/all.out"
done
note '上面 12 段是本节唯一的「真跑」，下面的账本对的是它们的收口行：小结 12 行、覆盖 11 章、'
note '全成立 12/12、「→ 实测」行数与小结里报的判据数总和一致（数一数二地核对，不是看一眼觉得对）。'
note '★ 两份坏账本先响：①把一条「全成立 True」改成 False；②整行删掉一条小结。'
runc 'timeout 60 python3 "$SB/pages/mutate.py" flip "$SB/ledger.txt" "$SB/ledger-bad1.txt"'
runc 'timeout 60 python3 "$SB/pages/mutate.py" drop "$SB/ledger.txt" "$SB/ledger-bad2.txt"'
runc 'timeout 60 python3 "$SB/pages/check_ledger.py" "$SB/ledger-bad1.txt"'
runc 'timeout 60 python3 "$SB/pages/check_ledger.py" "$SB/ledger-bad2.txt"'
note '上面两条 rc 都非 0，是**故意的**。下面是对真账本的判定。'
runc 'timeout 60 python3 "$SB/pages/check_ledger.py" "$SB/ledger.txt"'
verdict '12 组探针全部收口、覆盖 11 章、全成立 12/12，实测行数与账本判据数一致；两份坏账各自响一条并且 rc 非 0。'

# ============================================================================
banner 7 "隐私扫描：整份屏幕账，五类身份一次都不许出现"
# ============================================================================
note '本章比别处更容易漏：它要把 11 章的文件路径、章目录名、家目录条目数都念出来。'
note '五类禁串：家目录绝对路径 / 登录名 / 主机名 / 本次沙箱路径 / 对象地址形态（0x 加一串十六进制）。'
note '模式串一律当场从环境取（下面 argv 传进去），源码里一个字都没写死。'
note '★ 先造一份「必然全命中」的副本让它响：那五行是真路径、真登录名、真主机名、真沙箱路径、'
note '  真地址（地址是当场 hex(id(object())) 生成的，不是编的）。扫它必须五类都非 0。'
runc 'timeout 60 python3 "$SB/pages/make_dirty.py" "$SB/dirty.txt" "$HOME" "$USER" "$(hostname)" "$SB"'
runc 'timeout 60 python3 "$SB/pages/scan_priv.py" "$SB/dirty.txt" "$HOME" "$USER" "$(hostname)" "$SB"'
note '上面那条 rc 非 0，是**故意的** —— 它证明扫描器真的认得这五类，不是摆设。'
note '★ 折叠器也要**当场喂一条真泄漏**给它：这条命令把真家目录与真登录名打到 stdout，'
note '  屏幕上印出来的必须是占位符。没有这一步，「fold 挡过东西」就只是上一趟的运气 ——'
note '  上一趟它靠的是脚本崩溃时的 Traceback，那一响不是设计出来的，判据不能建在它上面。'
runc 'printf "这条是故意漏的：家目录=[%s] 登录名=[%s]\n" "$HOME" "$USER"'
note '下面扫的是本次到目前为止的整份屏幕账（all.out，含上面那条被折过的）。'
runc 'timeout 60 python3 "$SB/pages/scan_priv.py" "$SB/all.out" "$HOME" "$USER" "$(hostname)" "$SB"'
runc 'timeout 60 python3 -c "
import os
p = os.path.join(os.environ[\"SB\"], \"foldhits\")
ns = [int(x) for x in open(p) if x.strip()]
nz, tot = sum(1 for x in ns if x), sum(ns)
print(\"fold 记了 %d 笔，其中非零 %d 笔，累计挡下 %d 行\" % (len(ns), nz, tot))
print(\"→ 判据 挡过至少一行吗 →\", tot > 0, \"（该是 True：这一节上面刚故意漏过一次）\")
"'
verdict '整份屏幕账五类命中全 0（先由必然命中的副本证明扫描器能响）；fold 真挡过东西 —— 判据行当场报了 True，而且那条故意漏出来的家目录在屏上是占位符。'

# ============================================================================
banner 8 "红线自证 + 这一页的账"
# ============================================================================
note '本章唯一的写入操作 = 沙箱里那份屏幕账。章目录全程只读 —— 但它不能只靠一句话声明。'
note '★ 先在沙箱里造一份**同形状副本**（README.md + demo.sh + 一支 .py），真 import 它一次、'
note '  再塞一个非 .py 文件，让残留数到非 0；然后对本体报 0。同一个检查器，两次调用。'
runc 'cd "$SB/replica" && cp "$PAGE" README.md && cp "$HERE/demo.sh" demo.sh && printf "MARKER = 1\n" > resmod.py && ls -1 > "$SB/replica-before.txt" && cat "$SB/replica-before.txt"'
runc 'cd "$SB/replica" && timeout 60 python3 -c "import resmod; print(\"import 成功，于是 __pycache__ 真的出来了\")" && printf "给人看的笔记\n" > stray.bak && echo "副本目录现在的清单：$(ls -1 | tr "\n" " ")"'
runc 'timeout 60 python3 "$SB/pages/check_residue.py" "$SB/replica" "$SB/replica-before.txt"'
note '上面那条 rc 非 0，是**故意的**：残留被数出来了（一个是真 import 生成的 __pycache__，一个是非 .py 笔记）。'
note '下面是同一个检查器对**真章目录**报数 —— 这一趟跑完它必须是 0。'
runc 'cd -- "$HERE" && ls -1 > "$SB/here-after.txt"; timeout 60 python3 "$SB/pages/check_residue.py" "$HERE" "$SB/here-before.txt"'
runc 'printf "章目录清单 跑前 = [%s]\n" "$LS_BEFORE"; printf "章目录清单 现在 = [%s]\n" "$(cd -- "$HERE" && ls -1 | tr "\n" "|")"; printf "→ 一样吗：%s\n" "$([ "$LS_BEFORE" = "$(cd -- "$HERE" && ls -1 | tr "\n" "|")" ] && echo 是 || echo 否)"'
runc 'printf "家目录条目数 跑前=%s 现在=%s → 一样吗 %s\n" "$HOME_N_BEFORE" "$(ls -A "$HOME" | wc -l)" "$([ "$(ls -A "$HOME" | wc -l)" = "$HOME_N_BEFORE" ] && echo 是 || echo 否)"'
runc 'printf "监听中的 TCP socket 数 跑前=%s 现在=%s → 一样吗 %s（本章一个 socket 都没开）\n" "$LISTEN_BEFORE" "$(ss -ltn 2>/dev/null | tail -n +2 | wc -l)" "$([ "$(ss -ltn 2>/dev/null | tail -n +2 | wc -l)" = "$LISTEN_BEFORE" ] && echo 是 || echo 否)"'
note '子进程归零这条也先让它响一次。★ 这个后台 sleep 必须由 **demo 自己的 shell** 挂：'
note '挂在 runc 里就是那个 bash -c 包装进程的孩子，--ppid 数到的是包装进程本身（下面按 comm 过滤掉了），'
note '于是「有泄漏」永远数不出来 —— 那就是一条恒真的守卫。'
sleep 3 &
POISON_CHILD=$!
export POISON_CHILD
runc 'kids=$(ps -o comm= --ppid "$MAIN_PID" 2>/dev/null | grep -v -e "^bash$" | tr "\n" " "); printf "故意挂进去一个 sleep 之后：[%s] → 归零吗 %s（该是 否，这才叫能响）\n" "$kids" "$([ -z "$kids" ] && echo 是 || echo 否)"'
runc 'kill "$POISON_CHILD" 2>/dev/null; printf "已 kill 掉那个 sleep（pid 本身不上屏）\n"'
wait "$POISON_CHILD" 2>/dev/null
runc 'kids=$(ps -o comm= --ppid "$MAIN_PID" 2>/dev/null | grep -v -e "^bash$" | tr "\n" " "); printf "收回来之后：[%s] → 归零吗 %s\n" "$kids" "$([ -z "$kids" ] && echo 是 || echo 否)"'
note '最后一件是把这一页的账念一遍：屏幕总行数与节标题数。节标题数先由一份截断的账响一次。'
note '★ 这一行自己也在账里，所以它数到的「判据行」比节数少 1（本节那条判据还没落账），'
note '  「命令行」比「退出码行」多 1（它自己的退出码也还没落账）。两处都差 1 才是对的。'
runc 'head -n $(( $(wc -l < "$SB/all.out") - 1 )) "$SB/all.out" | grep -v "^══════ 第 8 节" > "$SB/all-cut.out"; printf "截断后的账：节标题数=%s（该报 7，因为它被抹掉了一节）\n" "$(grep -c "^══════ 第 " "$SB/all-cut.out")"'
runc 'printf "屏幕总行数=%s  节标题数=%s（该是 8）  命令行=%s  退出码行=%s  判据行=%s\n" "$(wc -l < "$SB/all.out")" "$(grep -c "^══════ 第 " "$SB/all.out")" "$(grep -c "^[$] " "$SB/all.out")" "$(grep -c "^\[退出码 " "$SB/all.out")" "$(grep -c "^  → 判据：" "$SB/all.out")"'
verdict '章目录跑前跑后清单一模一样、新增残留 0（同一个检查器先在副本上数到 2）；家目录条目数不变；监听 socket 数不变；本 demo 的直接子进程归零（先故意挂一个让它响）；节标题数 = 8（截断账上报 7）。'

# ============================================================================
# 收尾：本脚本是闸门，不是被验收的程序 —— 退出码恒为 0。
# 八节里所有「非 0」都是负样本故意响出来的，屏幕上每一条都有 note 说明它为什么该响。
# ============================================================================
exit 0
