#!/usr/bin/env bash
# CHECK: RUN=就地   理由：第 3 节要翻开**前十章的 README** 逐条解析出处 —— 把本脚本拷进沙箱就一个兄弟都读不到，
#        检查器只会报一堆假断链。就地连跑两遍同样逐字一致：本章八节全是 grep / awk / bash -n 的**计数与布尔**，
#        不碰时间、不碰 pid、不起并发；唯一每跑必变的沙箱随机后缀按全书约定不上屏，
#        而「章目录清单跑前跑后一模一样」这条在第 8 节当场摊给你看。
# ============================================================================
# 第 11 章《全书速查》—— 这一章不取证，它**自查自己那一页**
#
#   bash demo.sh            跑完自动删沙箱（默认）
#   bash demo.sh --keep     跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt  重定向时自动关颜色，README 引用的就是这份纯文本
#
# 为什么要有这样一个 demo：速查表是整本书里**唯一一处没有现场的页面**。
# 前十章每句结论后面都跟着一段屏幕输出，第 11 章只剩「出处在哪一章哪一节」。
# 那就必须有人当场检查四件事：出处翻不翻得到、写了「屏上」的那句到底上没上屏、抄进去的骨架语法过不过得了、
# 表里的硬事实今天还在不在。这一页哪天断了链，得让脚本先响 —— 不是你翻到空处才发现。
#
# 八节各自在断言什么（README 里那张表就是这份清单）：
#   第 1 节  地板：bash 版本、/bin/sh 是谁、本 demo 是不是交互 shell、自查用到的工具都在场
#   第 2 节  这一页的证据来源只有一个：章目录里除 README.md 只有本脚本；不借别章的捕获
#   第 3 节  向外核对两件事：每一条出处当场解析（落在哪一章、标题在不在），每一句「屏上」回屏幕上对（先各塞一个假的让它们响）
#   第 4 节  页面结构四条：同表列数一致、出处那格不许空、正文里的节号符号必须绑章号、一页纸条数对得上承诺
#   第 5 节  「四段骨架」逐个过 bash -n（先塞一段缺 fi 的看它响）
#   第 6 节  抽 8 条硬事实当场再跑一遍 —— 这一节的输出就是 README 里那段围栏的原文
#   第 7 节  隐私扫描：先插一条真用户名让它响，再报 0
#   第 8 节  红线自证 + 这一页的账（沙箱删干净、章目录没多文件、家目录条目数没变）
#
# 屏幕上是两种行，别混：`$ 命令` 底下到 `[退出码 N]` 为止的，全是这台机器当场跑出来的原文，一个字没手打；
# 以 `# ` 开头的是写给读的人的话（说明这一步在防什么），以两个空格 + `→ 判据：` 开头的是一条收口判定。
# 后两种不参与「必须能复现」的承诺 —— 所以校验器（script_check.py 的 GUARD）只认前两种的形状，
# 并要求每节至少有一条 `→ 判据`：写了标题却没跑出一节，它会当场点名。
#
# 安全说明：
#   * 所有写操作只发生在 mktemp -d 的沙箱里（前缀 bash11-），由 trap EXIT 整目录删除。
#   * 对前十章的 README.md **只读**，一个字都不改；不碰 ~/.bashrc、~/.config、~/.local、/etc。
#   * 不联网、不装包、不开端口、不起后台进程 —— 本章连「等一个东西」都不需要。
#   * 骨架第 2 段里那个 ssh 只作为**文本**过一遍 bash -n，永远不执行；第 6 节跑的只有内建与 coreutils。
#   * 屏幕上不出现真实身份：家目录、登录名、主机名上屏前一律折成 <家目录> <用户> <主机名>，
#     沙箱绝对路径折成 <沙箱>。第 7 节先证明这道折叠真的挡过东西（命中 > 0），再说「屏幕上干净」。
#   * 命令行里出现的 $MD、$ROOT、$SB、$HERE 是**当场 export 的变量**（这一页 / 手册根 / 沙箱 / 章目录），
#     所以屏上不会出现任何真实路径；你把这四个变量设成自己机器上的值，就能原样重跑每一条。
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) printf '用法: bash demo.sh [--keep]\n'; exit 0 ;;
        *) printf '未知参数：%s（本脚本只认 --keep）\n' "$arg" >&2; exit 2 ;;
    esac
done

export LANG=zh_CN.UTF-8 LC_ALL=zh_CN.UTF-8 LANGUAGE=zh_CN:zh

if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd "$HERE/.." && pwd)
MD="$HERE/README.md"
SB=$(mktemp -d "${TMPDIR:-/tmp}/bash11-XXXXXX")
BEFORE_LIST=$(cd "$HERE" && ls -A | LC_ALL=C sort | tr '\n' '|')
HOME_N_BEFORE=$(ls -A "$HOME" | wc -l)
export HERE ROOT MD SB BEFORE_LIST HOME_N_BEFORE

: > "$SB/all.out"
: > "$SB/foldhits"

banner() {
    printf '\n%s══════ %s %s%s\n' "$B" "$1" "════════════════════════════════" "$R"
    printf '\n##SEC## %s\n' "$1" >> "$SB/all.out"
}
note() { printf '%s\n' "$1"; }
# 每节末尾这条是「收口判定」：它以两个空格 + → 开头，是校验器（script_check.py 的 GUARD）
# 认的那个形状。写了标题却没跑出一节，GUARD 会当场点名 —— 所以它不是排版，是守卫。
verdict() { printf '  → 判据：%s\n' "$1"; }

fold() {
    local tmp="$SB/.foldtmp" hits
    cat > "$tmp"
    hits=$(grep -c -F -e "$HOME" -e "$USER" -e "$(hostname)" -e "$SB" "$tmp")
    printf '%s\n' "${hits:-0}" >> "$SB/foldhits"
    sed -E "s#$SB#<沙箱>#g; s#$HOME#<家目录>#g; s#\\b$USER\\b#<用户>#g; s#$(hostname)#<主机名>#g" \
        "$tmp" | tee -a "$SB/all.out"
    rm -f "$tmp"
}

runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1 | fold
    printf '[退出码 %d]\n' "${PIPESTATUS[0]}"
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去接着敲： cd -- "%s/work"\n' "$SB" "$SB"
    else
        rm -rf -- "$SB"
        printf '\n[沙箱已删除] 前缀 bash11-（随机后缀不进屏幕；只有 --keep 时才印完整路径）\n'
    fi
}
trap cleanup EXIT

mkdir -p "$SB/work" "$SB/snip"
cd "$SB/work" || exit 1
umask 022

# ---- 三个检查器：正文内嵌在这份 demo 里，所以章目录永远只有 README.md 与 demo.sh 两个文件 ----
cat > "$SB/check_refs.sh" <<'CHECKREFS'
set -u
# 出处解析。两种形状：「两位章号 + 空格 + § + 节号」和「两位章号 + 空格 + § + 「标题片段」」。
# 前者要求目标章 README 里真有 "## N. " 这一节；后者要求那一章某条 "## " 标题里含那串字。
MD=$1; ROOT=$2
tot=0; bad=0
while IFS= read -r cite; do
    tot=$((tot+1))
    ch=${cite:0:2}
    rest=${cite#*§}
    dir=$(ls -d "$ROOT/$ch"-*/ 2>/dev/null | head -1)
    if [ -z "$dir" ]; then
        bad=$((bad+1)); printf '  [断链] %s → 第 %s 章的目录根本不存在\n' "$cite" "$ch"; continue
    fi
    t="${dir}README.md"
    if [ ! -f "$t" ]; then
        bad=$((bad+1)); printf '  [断链] %s → 第 %s 章没有 README.md\n' "$cite" "$ch"; continue
    fi
    case "$rest" in
        '' | *[!0-9]*)
            frag=${rest#「}; frag=${frag%」}
            if grep '^## ' "$t" | grep -qF -- "$frag"; then :
            else bad=$((bad+1)); printf '  [断链] %s → 第 %s 章没有含「%s」的节标题\n' "$cite" "$ch" "$frag"
            fi ;;
        *)
            if grep -qE "^## $rest\. " "$t"; then :
            else bad=$((bad+1)); printf '  [断链] %s → 第 %s 章没有 "## %s. " 这一节\n' "$cite" "$ch" "$rest"
            fi ;;
    esac
done < <( { grep -oE '[0-9]{2} §[0-9]{1,2}' "$MD"; grep -oE '[0-9]{2} §「[^」]+」' "$MD"; } | LC_ALL=C sort -u )
printf '  合计解析 %d 条出处，断链 %d 条\n' "$tot" "$bad"
[ "$tot" -ge 40 ] && [ "$bad" -eq 0 ]
CHECKREFS

cat > "$SB/check_snips.sh" <<'CHECKSNIPS'
set -u
# 把 README 里每个 bash 围栏抽成独立文件，逐个 bash -n。只查语法（第 09 章 §9 说的那三层里最外一层）。
MD=$1; OUT=$2
rm -f "$OUT"/*.sh
awk -v out="$OUT" '
    /^```bash$/ { inb=1; n++; f=sprintf("%s/%02d.sh", out, n); next }
    /^```$/     { if (inb) { inb=0; close(f) } ; next }
    inb         { print >> f }
' "$MD"
tot=0; bad=0
for s in "$OUT"/*.sh; do
    [ -e "$s" ] || continue
    tot=$((tot+1))
    if bash -n "$s" 2>/dev/null; then
        printf '  骨架 %02d：bash -n 通过\n' "$tot"
    else
        bad=$((bad+1)); printf '  骨架 %02d：[语法错] %s\n' "$tot" "$(bash -n "$s" 2>&1 | head -1)"
    fi
done
printf '  合计 %d 段骨架，语法错 %d 段\n' "$tot" "$bad"
[ "$tot" -ge 4 ] && [ "$bad" -eq 0 ]
CHECKSNIPS

cat > "$SB/check_rows.sh" <<'CHECKROWS'
set -u
# 这一页是表格做的，四类毛病最致命：
#   ① 「出处」那格空着 —— 等于一句没现场的断言；
#   ② 正文里出现没绑章号的节号符号 —— 读的人不知道往哪翻，检查器也解析不到；
#   ③ 一页纸的条数和正文里承诺的数字不一致 —— 正文写了「22 条」就必须真数到 22 条；
#   ④ 同一个表格里某行列数对不上表头 —— 十有八九是格子里写了没转义的竖线，渲染时整行错位。
# 围栏里的内容是 demo 的逐字记录，四条都不参与（那是命令原文与屏幕原文，不是本页的说法）。
MD=$1; WANT=${2:-22}
awk -v want="$WANT" '
    /^```/  { inF = !inF; ftext = ($0 ~ /^```text/) ? 1 : 0; intab = 0; next }
    inF     { if (ftext && $0 ~ /^[0-9]+ /) pager++
              fsec += gsub(/§/, "&")     # 围栏里的节号符号：只报数，不进②的判定
              next }
            # ★ 两个计数各拿一份副本：要是先替掉了 §，第二个计数就永远数不到东西（我在这儿翻过一次车）。
            { a = $0; seg += gsub(/§/, "@", a)
              b = $0; bnd += gsub(/[0-9][0-9] §/, "@", b) }
    /^\|/   { probe = $0; gsub(/\\\|/, "\001", probe)     # 先把「转义过的竖线」藏起来再数列
              nf = split(probe, cell, "|")
              row++
              if (!intab) { intab = 1; hdr = nf; tab++ }
              else if (nf != hdr) { b1++; printf "  [列数不符] 第 %d 行：%d 列，本表表头 %d 列（多半是格子里有没转义的竖线）\n", NR, nf, hdr }
              last = cell[nf-1]; gsub(/[ \t]/, "", last)
              if (last == "") { b2++; printf "  [出处空] 第 %d 行\n", NR } }
    !/^\|/  { intab = 0 }
    END     { printf "  表格行 %d 行、%d 张表；列数不符 %d 处 + 出处空 %d 处\n", row, tab+0, b1+0, b2+0
              printf "  正文里的节号符号 %d 处，其中绑着章号的 %d 处 → 全绑上了吗 %s\n", seg+0, bnd+0, (seg==bnd ? "True" : "False")
              printf "  围栏里的节号符号 %d 处（这一行只报数，不进上一条判定 —— 那是逐字记录，不是本页的说法）\n", fsec+0
              printf "  一页纸数到 %d 条，正文承诺 %d 条 → 对得上吗 %s\n", pager+0, want, ((pager+0)==want ? "True" : "False")
              exit ((b1+b2) > 0 || seg != bnd || (pager+0) != want) ? 1 : 0 }
' "$MD"
CHECKROWS

# 第五个检查器：把「屏上」这两个字当一句可以核对的承诺来判，而不是当修辞。
cat > "$SB/check_screen.sh" <<'CHECKSCREEN'
set -u
# 用法：check_screen.sh <本页README> <手册根> <沙箱目录>
# 判法两步：① 把别章 README 的**现场围栏**收成一份「这台机器的屏幕」——
#      现场围栏 = 不带语言标注的 ``` 块，或块里有提示词（`$ ` / `敲的是：`）的块；
#      「怎么用/怎么跑这一章」这类标题正下面的那种手敲命令不算（那是让人敲的，不是打出来的）。
#    ② 拿本页（正文与表格，围栏除外 —— 围栏是逐字记录不是本页的说法）每一小句（按 。；！ 切）里、
#      出现在「屏上」**之后**的反引号片段，回这张屏上找。
#      只认带「屏上」的那一小句、且只取它之后的片段，是为了不误伤两类合法写法：
#        「屏上打了 X。修法是 Y」—— Y 是建议，本来没上过屏；
#        「`X` 全书没演上屏」—— 这是主动招供，不该被要求现场。
MD=$1; ROOT=$2; SBXDIR=$3
SCREEN="$SBXDIR/screen.txt"
: > "$SCREEN"
for t in "$ROOT"/*/README.md; do
    [ -f "$t" ] || continue
    case "$t" in *11-cheatsheet*) continue ;; esac      # 本页不能当自己的屏幕
    awk '
      /^#{1,6} / { how = ($0 ~ /怎么跑|如何跑|怎么把这一章|复现这一章|怎么用这一章/) }
      /^```/ {
          if (inF) {
              if ((tag == "" || hp) && !how) for (i = 1; i <= n; i++) print buf[i]
              inF = 0; n = 0; hp = 0; tag = ""
          } else { inF = 1; tag = substr($0, 4); sub(/[ \t]+$/, "", tag); n = 0; hp = 0 }
          next
      }
      inF { buf[++n] = $0
            if ($0 ~ /^[ \t]*\$ / || $0 ~ /^[ \t]*敲的是：/) hp = 1 }
      END { if (inF && (tag == "" || hp) && !how) for (i = 1; i <= n; i++) print buf[i] }
    ' "$t" >> "$SCREEN"
done
ncap=$(wc -l < "$SCREEN")

judged=0; bad=0
while IFS= read -r row; do
    res=$(printf '%s\n' "$row" | awk -v scr="$SCREEN" '
      BEGIN { n = 0; while ((getline ln < scr) > 0) c[n++] = ln }
      { gsub(/。/, ";"); gsub(/；/, ";"); gsub(/！/, "!")
        ncl = split($0, cl, ";")
        for (i = 1; i <= ncl; i++) {
            at = index(cl[i], "屏上"); if (at == 0) at = index(cl[i], "屏幕上")
            if (at == 0) continue
            if (cl[i] ~ /没演|没上屏|没上过屏|没打过|没印|没跑过|没演示/) continue
            s = substr(cl[i], at)
            while ((q = index(s, "`")) > 0) {
                rest = substr(s, q + 1)
                r = index(rest, "`")
                if (r == 0) break
                if (r > 1) {
                    f = substr(rest, 1, r - 1)
                    gsub(/\\\|/, "|", f); gsub(/^[ \t]+/, "", f); gsub(/[ \t]+$/, "", f)
                    if (length(f) >= 4 && f ~ /[ -~]/ && f !~ /^[0-9][0-9] *§/) {
                        found = 0
                        for (k = 0; k < n; k++) if (index(c[k], f) > 0) { found = 1; break }
                        if (!found) printf "BAD %s\n", substr(f, 1, 60)
                        else printf "OK %s\n", substr(f, 1, 40)
                    }
                }
                s = substr(rest, r + 1)
            }
        } }')
    while IFS= read -r line; do
        [ -n "$line" ] || continue
        case "$line" in
            BAD\ *) judged=$((judged+1)); bad=$((bad+1))
                  printf '  [站不住] `%s`\n' "${line#BAD }" ;;
            OK\ *)  judged=$((judged+1)) ;;
        esac
    done <<< "$res"
done < <(awk '/^```/ { inF = !inF; next } !inF { print }' "$MD")

printf '  屏幕底本 %d 行；本页「屏上」后面共 %d 段反引号原文，站不住 %d 段\n' "$ncap" "$judged" "$bad"
[ "$judged" -ge 20 ] && [ "$bad" -eq 0 ]
CHECKSCREEN

# 两个「必须能响」的样本页：正文里写死在这里，第 4 节拿它们喂同一个检查器。
cat > "$SB/sample_empty.md" <<'SAMPLE1'
| 我要…… | 靠得住的写法 | 现场 |
|---|---|---|
| 一条出处留空的行 | 什么都对不到 |  |
SAMPLE1
cat > "$SB/sample_bare.md" <<'SAMPLE2'
正文里出现一个没绑章号的节号符号：见 §99。
SAMPLE2
cat > "$SB/sample_short.md" <<'SAMPLE3'
## 一页纸（这里故意只写两条）

```text
1  第一条。
2  第二条。
```
SAMPLE3
cat > "$SB/sample_wide.md" <<'SAMPLE4'
| 我要…… | 靠得住的写法 | 现场 |
|---|---|---|
| 格子里写了一条没转义的竖线 | `a | b` 这种字面串直接摊在格子里 | 03 §1 |
SAMPLE4

# ============================================================================
banner "第 1 节 先把这台机器的发言权确认一遍"
# ============================================================================
note '# 这一页所有结论都钉在「这台机器的 bash」上。版本、/bin/sh 是谁、是不是交互 shell，三件事先落地。'
runc 'bash --version | head -1'
note '# 下面这条回答「本 demo 自己是不是交互 shell」—— 它直接决定第 05 章 lastpipe 那一行对不对得上：'
runc 'printf "本 demo 的 \$- = [%s] → 交互吗 %s\n" "$-" "$(case \$- in *i*) echo 是 ;; *) echo 否 ;; esac)"'
note '#   ★ 答案是「否」：bash demo.sh 起的是非交互 shell。你在终端里手敲同样的命令时 $- 多半多个 i，'
note '#     作业控制开着，lastpipe 那句「且要求作业控制关着」就是从这里来的。'
runc 'printf "/bin/sh 现在指向 → %s\n" "$(readlink -f /bin/sh)"'
note '# 自查只用到这些工具，逐个点名（不在场的就别假装能跑）：'
runc 'for t in grep awk sed bash dash find mktemp wc sort tr; do printf "  %-7s → %s\n" "$t" "$(command -v "$t" || echo 不在 PATH)"; done'

verdict '这一页全部结论钉在「这台机器的 bash + 它自己的 /bin/sh」上；本 demo 是非交互 shell，所以凡是只有交互才有的东西都另写了一档（表 7 那一行 dash 就是让你回去核对自己机器的）。'

banner "第 2 节 这一页的证据来源只有一个"
# ============================================================================
note '# 前十章每章都有一堆取证脚本；这一章故意一个都没有 —— 它只核对「已经写下的那一页」。'
note '# 所以章目录里应该只有两个文件。清单当场摊开：'
runc 'cd -- "$HERE" && ls -1'
note '# 更要紧的一条：这一页不许去借别章的屏幕输出当自己的证据 —— 借了就等于「引用了一段我没跑过的东西」。'
note '# 扫法：README 里出现「某章定稿捕获」那种文件名形状（两字母+两数字-名字.txt）就报数。先拿一个真存在的名弄响：'
runc 'cp -- "$MD" "$SB/probe.md"; echo "这一行是我从别处借来的捕获：py10-run1.txt" >> "$SB/probe.md"; printf "弄响版命中 %s 处\n" "$(grep -cE "[a-z]{2}[0-9]{2}-[A-Za-z0-9._-]+[.]txt" "$SB/probe.md")"'
runc 'printf "正式版命中 %s 处\n" "$(grep -cE "[a-z]{2}[0-9]{2}-[A-Za-z0-9._-]+[.]txt" "$MD")"'
verdict '弄响版必须 > 0（不然这条扫描是摆设），正式版必须 = 0；两个数要一起读，只有一个为 0 不算证据。'

banner "第 3 节 每一条出处当场解析一遍，每一句「屏上」回屏幕上对"
# ============================================================================
note '# 出处记法就两种：「两位章号 空格 § 节号」（要求那一章 README 里真有 "## 9. " 这一节）'
note '# 和「两位章号 空格 § 「标题片段」」（要求那一章某条 "## " 标题里含这几个字）。先把两种各数一遍：'
runc 'printf "带节号的 %s 处 + 带标题片段的 %s 处（这是出现次数，去重后才是要解析的条数）\n" "$(grep -oE "[0-9]{2} §[0-9]{1,2}" "$MD" | wc -l)" "$(grep -oE "[0-9]{2} §「[^」]+」" "$MD" | wc -l)"'
note '# 然后逐条翻到那一章去对标题。★ 先把一条真出处全数字换成 9（等于「第 99 章」），确认这道检查真的会拦：'
runc 'cp -- "$MD" "$SB/broken.md"; grep -oE "[0-9][0-9] §[0-9]+" "$MD" | head -1 | tr "0123456789" "9999999999" >> "$SB/broken.md"; bash "$SB/check_refs.sh" "$SB/broken.md" "$ROOT" >/dev/null 2>&1; printf "弄响版退出码 = %s（该是非 0）\n" "$?"'
runc 'bash "$SB/check_refs.sh" "$MD" "$ROOT"; printf "本体退出码 = %s（该是 0）\n" "$?"'
note '# 出处只保证「翻得到」。这一页还有第二类向外借证据的写法：句子里写「屏上 X」—— 那两个字是一句承诺，'
note '# 它说紧跟其后的那段反引号原文是这台机器打出来的。判法两步：'
note '#   ① 先把别章 README 的**现场围栏**收成一份「这台机器的屏幕」。现场围栏 = 不带语言标注的 ``` 块，或块里真有'
note '#      提示词（`$ ` / `敲的是：`）的块；「怎么用这一章」那种标题正下面的手敲命令不算 —— 那是让人敲的，不是打出来的。'
note '#   ② 拿本页每一小句（按 。；！ 切）里、出现在「屏上」**之后**的反引号片段回这张屏上找。'
note '#      只认带「屏上」的那一小句、且只取它之后的片段，是为了不误伤两类合法写法：'
note '#        「屏上打了 X。修法是 Y」—— Y 是建议，本来没上过屏；「`X` 全书没演上屏」—— 这是主动招供，不该被要求现场。'
note '# ★ 先编一句屏幕上没有的话，确认这道检查真能拦住（它不许只做一次成功计数）：'
runc 'cp -- "$MD" "$SB/fabricated.md"; printf "| 编的一行 | 屏上还印过 \`绝无此句 ZZQQ 424242\`。 | 09 §9 |\n" >> "$SB/fabricated.md"; bash "$SB/check_screen.sh" "$SB/fabricated.md" "$ROOT" "$SB" >/dev/null 2>&1; printf "弄响版退出码 = %s（该是非 0）\n" "$?"'
note '# 现在扫这一页本体（屏幕上只印「站不住」的片段和最后一行账，全对上的那些不刷屏）：'
runc 'bash "$SB/check_screen.sh" "$MD" "$ROOT" "$SB"; printf "  本体退出码 = %s（该是 0）\n" "$?"'
verdict '两道向外核对都得退出码 0：出处那道要求「断链 0 条」且解析条数 ≥ 40，屏上那道要求「站不住 0 段」且判到 ≥ 20 段 —— 数字不许少，少了就是表格被删空、检查在空转。上面两个假样本（不存在的章号 / 屏幕上没有的一句话）各响过一次，所以这两个 0 是「拦下来过才叫住」，不是「从来没试过」。'

banner "第 4 节 页面结构：出处、绑定、承诺过的数字"
# ============================================================================
note '# 四类毛病各有致命理由：出处那格空 = 一句没现场的断言；节号符号没绑章号 = 谁都翻不到现场；'
note '# 正文写「一页纸 22 条」而真数下来不是 22 = 这一页自己在编数；格子里冒出一条没转义的竖线 = 整行渲染错位。'
note '# ★ 先把四个样本喂进去，它们必须全响：'
runc 'bash "$SB/check_rows.sh" "$SB/sample_empty.md"; printf "  样本一（出处留空）退出码 = %s（该是非 0）\n" "$?"'
runc 'bash "$SB/check_rows.sh" "$SB/sample_bare.md"; printf "  样本二（裸节号符号）退出码 = %s（该是非 0）\n" "$?"'
runc 'bash "$SB/check_rows.sh" "$SB/sample_short.md" 22; printf "  样本三（承诺 22 实际 2）退出码 = %s（该是非 0）\n" "$?"'
runc 'bash "$SB/check_rows.sh" "$SB/sample_wide.md"; printf "  样本四（格子里有裸竖线）退出码 = %s（该是非 0）\n" "$?"'
note '# 现在扫这一页本体：'
runc 'bash "$SB/check_rows.sh" "$MD" 22; printf "  本体退出码 = %s（该是 0）\n" "$?"'
note '#   ★ 上面读数第三行是刻意留的一条边界：「围栏不参与统计」到底挡住了什么。这一页围栏里的节号符号今天是 0，'
note '#     所以这条规则暂时没被用上；它挡的是将来把「带节号符号的屏幕原文」贴进围栏时自己扫响自己 ——'
note '#     真贴进来那天这条线会先变成非 0，而不是静悄悄地把围栏里的字当本页的说法。'

verdict '四条结构守卫各有一个专属坏样本（出处空 / 裸符号 / 承诺不符 / 裸竖线），四个全响过之后，本体的退出码 0 才叫「这一页结构上能核对」，不叫「这一页写对了」。'

banner "第 5 节 四段骨架逐个过 bash -n"
# ============================================================================
note '# 第 09 章 §9 把「不跑起来也能查出来的」分成三层，bash -n 是最外那层：它只管语法。'
note '# ★ 先塞一段缺 fi 的，确认它会响：'
runc 'printf "if true; then\n  echo ok\n" > "$SB/bad_snip.sh"; bash -n "$SB/bad_snip.sh" >/dev/null 2>&1; printf "缺 fi 那段：退出码 %s，第一行报错：%s\n" "$?" "$(bash -n "$SB/bad_snip.sh" 2>&1 | head -1)"'
runc 'bash "$SB/check_snips.sh" "$MD" "$SB/snip"; printf "  本体退出码 = %s（该是 0）\n" "$?"'
verdict '正式版必须「语法错 0 段」且段数 ≥ 4；上面那条缺 fi 的报错就是这层守卫自己的响声 —— 少一段就少一次能被查出来的机会。'
note '#   ★ 过了 bash -n ≠ 跑得对：骨架第 2 段里的 ssh、第 4 段里的 jobs 与 run_one 都只是形状，不是可执行计划。'

banner "第 6 节 抽 8 条硬事实，当场再跑一遍"
# ============================================================================
note '# 这八条不是装饰：每一条都对应表里一行「直觉以为什么 / 屏幕上实际什么」的对撞。'
note '# 下面这一段就是 README 里那段围栏的原文 —— 一个字都没有手抄。'
note '# 1) 表 1：花括号展开早于变量替换，所以序列写法里那个 N 根本不会被看见。'
runc 'N=3; echo "{1..$N}"'
note '# 2) 表 1：${x-D} 只判「没设」，${x:-D} 连空串一起判。'
runc 'v=""; echo "[${v-D}][${v:-D}]"'
note '# 3) 表 1 / 表 4：通配 0 匹配的三档人生，这里演默认档与 nullglob 两档（当前目录是个空沙箱）。'
runc 'echo "默认档：$(echo *.zzz)"; echo "nullglob：[$(shopt -s nullglob && echo *.zzz)]"'
note '# 4) 表 4：read 的退出码问的是「撞没撞到分隔符」，不是「拿到字没有」。'
runc 'printf "" | IFS= read -r x; echo "EOF：rc=$? x=[$x]"; printf "\n" | IFS= read -r x; echo "空行：rc=$? x=[$x]"; printf "a" | IFS= read -r x; echo "没换行的一行：rc=$? x=[$x]"'
note '# 5) 表 4：同一份数据，最后一行没换行 → while read 少跑一轮；|| 配 [ -n ] 是唯一要背的修法。'
runc 'n=0; while IFS= read -r l; do n=$((n+1)); done < <(printf "a\nb"); m=0; while IFS= read -r l || [ -n "$l" ]; do m=$((m+1)); done < <(printf "a\nb"); printf "普通写法 %s 轮 / 带 || 的写法 %s 轮\n" "$n" "$m"'
note '# 6) 表 2 / 表 10：退出码只保留低 8 位，所以业务码不能编到 255 以上。'
runc 'f(){ return 300; }; f; echo "return 300 → 外面看到的是 $?"'
note '# 7) 表 3：pipefail 撞上 head —— 同一条命令，开与不开差一个 141（= 128 + SIGPIPE）。'
runc 'yes | head -1 >/dev/null; echo "不开 pipefail：rc=$?"; set -o pipefail; yes | head -1 >/dev/null; echo "开了 pipefail：rc=$?"'
note '# 8) 表 2：[[ ]] 里的 < 是按字符串比；要数值比就得进算术上下文。'
runc '[[ "10" < "9" ]] && echo "[[ ]] 里 10 小于 9（按字符串）"; (( 10 > 9 )) && echo "算术里 10 大于 9"'

verdict '这 8 条是**筛子不是全量重跑**：它们只回答「表里那些写死了结果的句子，今天这台机器还认不认」。真要看某条的来龙去脉，按那一行出处回它自己那一章的现场。'

banner "第 7 节 隐私扫描：先让它响，再报 0"
# ============================================================================
note '# 扫描器一律用**当场取到的值**当模式（家目录、登录名、主机名），绝不写死字面串 ——'
note '# 写死的话，这个脚本自己就成了「拿着模式串的那个文件」，扫谁都会自匹配（第 09 章栽过那种）。'
note '# ★ 先造一份必然命中的样本，证明这条扫描不是摆设：'
runc 'printf "探针：%s 和 %s\n" "$HOME" "$USER" > "$SB/leak.txt"; printf "弄响版命中行数 = %s\n" "$(grep -c -F -e "$HOME" -e "$USER" "$SB/leak.txt")"'
note '# 现在扫这一页本体（它是纯文本，本该一个都不命中）：'
runc 'printf "正式版：README 命中真实身份的行数 = %s\n" "$(grep -c -F -e "$HOME" -e "$USER" -e "$(hostname)" "$MD")"'
note '# 再扫「本次已经打到屏幕上的全部内容」—— 这些在上屏之前都被折叠过一次，这里数的是折叠之前命中多少：'
runc 'printf "屏幕上被折叠掉的身份行数合计 = %s（>0 才证明折叠真在挡东西）\n" "$(awk "{s+=\$1} END {print s+0}" "$SB/foldhits")"'
verdict 'README 命中数必须 = 0，且折叠合计必须 > 0，两个要一起读：只有后者也是 0 时，「屏幕上干净」是因为根本没东西可挡 —— 那种干净不许当证据。'

banner "第 8 节 红线自证 + 这一页的账"
# ============================================================================
note '# 跑了一整节，你的家目录和章目录动没动 —— 用数目回答，名单本身一个字段都不上屏：'
runc 'h_now=$(ls -A "$HOME" | wc -l); printf "家目录条目数 跑前=%s 现在=%s → 一样吗 %s\n" "$HOME_N_BEFORE" "$h_now" "$([ "$h_now" = "$HOME_N_BEFORE" ] && echo True || echo False)"'
runc 'now=$(cd -- "$HERE" && ls -A | LC_ALL=C sort | tr "\n" "|"); printf "章目录清单 跑前=[%s]\n" "$BEFORE_LIST"; printf "章目录清单 现在=[%s] → 一个字都没多吗 %s\n" "$now" "$([ "$now" = "$BEFORE_LIST" ] && echo True || echo False)"'
note '# 这一页自己的账。这几个数一旦写进正文就成了承诺，所以它们必须和前面几节当场扫出来的对上：'
runc 'printf "README 行数 = %s\n" "$(wc -l < "$MD")"'
runc 'F=$(printf "\x60\x60\x60"); n=$(grep -c "^$F" "$MD"); b=$(grep -c "^${F}bash$" "$MD"); t=$(grep -c "^|" "$MD"); c=$(grep -oE "[0-9]{2} §([0-9]{1,2}|「[^」]+」)" "$MD" | wc -l); printf "表格行 %s 行；出处出现 %s 次（含重复）；围栏标记 %s 个 → 偶数吗 %s；其中 bash 围栏 %s 段\n" "$t" "$c" "$n" "$([ $((n % 2)) -eq 0 ] && echo True || echo False)" "$b"'
note '#   ★ 上面那条命令行里的围栏符号是用 \x60 当场拼出来的，源码里没有那个字面串 ——'
note '#     否则这份 demo 自己就带着模式串，任何字面扫描都会自匹配（第 09 章栽过的那种坑）。'
runc 'printf "内部账本行数（只记命令的结果行与节标题，命令行本身不记）= %s；节标题数 = %s\n" "$(wc -l < "$SB/all.out")" "$(grep -c "^##SEC## " "$SB/all.out")"'
verdict '节标题数必须是 8，家目录条目数与章目录清单必须跑前跑后一样。节数不是靠「我数了一遍源码」得出的，是当场从内部账本里 grep 出来的 —— 同一份账本也喂第 7 节的折叠命中统计，所以标题写了没跑、或跑漏一节，两边就对不上。'
