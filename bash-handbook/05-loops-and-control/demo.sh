#!/usr/bin/env bash
# ============================================================================
# 第 05 章《循环、select 与作业控制：谁在跑、跑几轮、跑完归谁》—— 整章证据一键复现
#
#   bash demo.sh            跑完自动删沙箱（默认）
#   bash demo.sh --keep     跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt  重定向时自动关掉颜色，README 引用的就是这份纯文本
#
# 屏幕上出现的每一行，都是这条命令在这台机器上的真实结果 —— 没有一行是手写的。
#
# 和前面几章的衔接：
#   02 章讲「一行怎么被切成词」，03 章讲「一句话成功没有（退出码）」，04 章讲
#   「进程之间怎么倒水」。这一章把三样东西同时放进一个循环里：
#     ① 循环的词表是**开跑前一次性摊开**的快照 —— 循环体里再改目录、改变量都晚了；
#     ② 循环体是不是**在另一个进程里**，决定你写在里面的变量能不能带出来；
#     ③ 后台作业（&）的收尾权在你手上：不 wait 就等于把失败咽下去。
#
# 本章的取证路线（22 节，全部真跑）：
#   第 1~2 节   钉死环境：bash 版本、几个关键 shopt 开关、内建 vs 外部命令
#   第 3 节     for 的三种形态，以及「循环变量出了循环还活着」
#   第 4 节     词表快照：loopsnap.sh
#   第 5 节     通配符匹配到 0 个文件时的三种人生（默认 / nullglob / failglob）
#   第 6 节     for (( )) 只吃整数，以及「无限循环」怎么被 head 掐掉
#   第 7 节     不加引号的展开把词表切成几份
#   第 8~11 节  while/until 的镜像关系、read 的最小语义、多字段切分、read 的开关柜
#   第 12~13 节 readlast.sh（最后一行被吞）、readsteal.sh（循环体偷吃 stdin）
#   第 14 节    管道里的 while 是子 shell：lastpipe 与三种接回办法
#   第 15~16 节 mapfill.sh（一次读全）、selectmenu.sh（菜单的真实行为）
#   第 17 节    break/continue 的层数，和 bash 没有循环标签这件事
#   第 18~21 节 jobwait.sh（作业控制）、untilretry.sh（重试环）、parlimit.sh（限并发）、
#               loopspeed.sh（循环贵在哪里）
#   第 22 节    自证：沙箱已删、章目录干净、没留下后台进程、没起过端口
#
# 安全说明：
#   * 所有写操作只发生在 mktemp -d 的沙箱里（前缀 bash05-），由 trap cleanup EXIT 整体删掉；
#     本章九个辅助脚本各自只在自己的子目录里造文件，跑完自己删并当场自证。
#   * 绝不修改 ~/.bashrc、~/.profile、~/.config、~/.local、/etc：要看「另一个干净 shell」
#     的行为一律用 bash -c 起新进程，开关（shopt / set -m）只在那个子进程里活着。
#   * 本章起的后台进程只有 sleep（以及沙箱内自造的短命令），每一个都当场 wait/kill 收尾；
#     第 22 节再用 pgrep 复核系统里没有残留。不 kill 任何不是本章起的进程：
#     kill -0 探 pid 1 那一次**不投递信号**，只是问内核一句，屏幕上也不印真实 pid。
#   * 不联网、不装包（缺的工具如实打印，例如 shellcheck）。
#   * 屏幕上不出现真实身份：pid 一律折成 <pid> 或只报「相等吗 → 是/否」，
#     命令行里不出现绝对路径（要列真实目录用 runc_at，只留 basename）。
#   * 耗时/毫秒/轮询次数/完成顺序**一律不上屏**：它们每跑必变，README 印了就永远对不上。
#     本章能给的确定性是「计数、排序、布尔、退出码」这四种。
#
# 为什么这个脚本**故意不加** set -e：
#   本章一半的内容是失败现场：循环体里的命令失败、wait 拿到 143、read 超时拿到 142、
#   failglob 直接拒绝展开、break 写在循环外面。加了 set -e，第 5 节第一段就把 demo 掐了。
#   同理不加 set -u（演示里大量故意留空的变量），也不全局开 pipefail ——
#   管道的退出码归第 04 章，本章只在需要时于单条命令里临时开一下。
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

# 把语言环境钉死成「英文 + UTF-8」：这台机器默认 zh_CN.UTF-8，bash 的报错会是中文，
# README 就没法逐行和真实输出核对。改动只发生在本脚本和它 fork 出的子进程里。
ORIG_LANG="${LANG:-（未设）}"
ORIG_LC_ALL="${LC_ALL:-（未设）}"
export LC_ALL=C.UTF-8 LANG=C.UTF-8 LANGUAGE=C
# ★ 必须 export：第 1 节那条 runc 是在 bash -c 起的**子进程**里跑的，不导出的话它只能看到
#   改过的新值，「你原来的终端是 LANG=」会印成两个空括号，看着像本机没设语言环境。
export ORIG_LANG ORIG_LC_ALL

# 输出被重定向到文件时（README 捕获就是这样跑的）不要 ANSI 颜色码：纯文本才能逐行核对。
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/bash05-XXXXXX")

banner() { printf '\n══════ %s %s\n' "$1" "════════════════════════════════"; }
note() { printf '%s\n' "$1"; }

# 打印命令 → 用 bash -c 真跑 → 打印退出码。
# 用 bash -c 是为了能自然地写管道、`;`、各种引号；也保证每一条都在一个
# **全新的非交互 bash** 里开始：上一节开的 shopt / set -m 开关不会串到下一节。
# 2>&1 把子进程的 stderr 并进 stdout，所以「报错原文」也留在这份纯文本捕获里。
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}

# runc_at <目录> <命令>：先 cd 进那个目录再跑，屏幕上只留目录名（basename）+ 相对命令。
runc_at() {
    printf '\n%s$ 〔在 %s 里〕 %s%s\n' "$C" "$(basename "$1")" "$2" "$R"
    ( cd "$1" && bash -c "$2" ) 2>&1
    printf '[退出码 %d]\n' "$?"
    return 0
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去接着敲： cd "%s/work"     看完删掉： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$(basename "$SB")"
    fi
}
trap cleanup EXIT

# 本章的九个辅助脚本复制进沙箱，所有加工都在副本上进行（脚本里写的都是相对路径）。
mkdir -p "$SB/work"
cp "$HERE"/*.sh "$SB/work/"
cd "$SB/work" || exit 1
# umask 钉在 0022：本章脚本在沙箱里现造文件，继承来的 umask 各机器不同（这台机器的外层
#   shell 给的是 0002），钉死之后同一份 demo 在任何机器上跑出的权限形状都是同一个。
umask 022

# ============================================================================
banner "第 1 节 先把这台机器的发言权确认一遍"
# ============================================================================
note '# 本章讲的每一件事（lastpipe、nullglob、failglob、wait -n、mapfile -d、break N）都是版本相关的，'
note '#   报错文本还和语言环境有关，所以第一步是把它们钉死，后面才谈得上逐字核对。'
runc 'bash --version | head -1'
runc 'echo "demo 已把自己改成：LC_ALL=[$LC_ALL] LANG=[$LANG]（你原来的终端是 LANG=$ORIG_LANG，LC_ALL=$ORIG_LC_ALL）"'
note '# 六个本章会碰的开关，此刻的默认位置（on=开）：'
runc 'for o in lastpipe nullglob failglob globstar extglob nocaseglob; do printf "  %-12s %s\n" "$o" "$(shopt -q "$o" && echo on || echo off)"; done'
note '# ★ 为什么 lastpipe 值得单独看一眼：它默认是 off，而 off 的代价是「管道最后一段里的赋值带不出来」。'
note '#   第 14 节整节就在处理这一件事。'
note '# 后台并发的天花板（第 20 节限并发时为什么不能一把全扔）：'
runc 'echo "ulimit -u（最大用户进程数）= $(ulimit -u) ｜ /proc/sys/kernel/pid_max = $(cat /proc/sys/kernel/pid_max)"; echo "屏幕上不印真实 pid，pid_max 是个固定的内核参数，不是本机进程号"'
runc 'command -v shellcheck >/dev/null 2>&1 && echo "shellcheck 在：$(shellcheck --version | grep version: | head -1)" || echo "shellcheck 本机没有 —— 本章不装东西，脚本只能靠逐条真跑来验"'

# ============================================================================
banner "第 2 节 循环体里的东西是内建还是外部命令：一次 fork 都不许猜"
# ============================================================================
note '# 这一节先把「内建/外部」这条界线立起来，第 21 节再用计数把它量出来。'
runc 'for c in : true echo printf read mapfile for while jobs wait kill test seq grep cat; do printf "  %-8s type -t → %s\n" "$c" "$(type -t "$c")"; done'
note '# type -t 给 builtin 的（read/mapfile/jobs/wait/[ ]）跑在**当前进程**里；给 file 的（seq/grep/cat）'
note '#   每一轮都要 fork+exec 一个进程。慢的脚本几乎都是这个原因，而不是「循环本身慢」。'
runc 'printf "seq 的真身：%s\n" "$(command -v seq)"; printf "/bin/true 的真身：%s ｜ type -t → %s\n" "$(command -v /bin/true)" "$(type -t /bin/true)"'
runc 'n=0; for i in $(seq 1 5); do n=$((n + 1)); done; echo "for i in \$(seq 1 5) 跑了 $n 轮；seq 自己是一个外部进程"'
runc 'n=0; for ((i = 1; i <= 5; i++)); do n=$((n + 1)); done; echo "for ((i=1;i<=5;i++)) 跑了 $n 轮；一个进程都没起，也没有词表"'

# ============================================================================
banner "第 3 节 for 的三种形态，和「循环变量出了循环还活着」"
# ============================================================================
runc 'for x in a b c; do printf "  拿到 [%s]\n" "$x"; done; echo "循环结束后 x=[$x]（★ 它不是块内变量，bash 没有块作用域）"'
runc 'for ((i = 0; i < 3; i++)); do printf "  i=%s\n" "$i"; done; echo "循环结束后 i=$i（所以复用同一个变量名的两层循环会互相踩）"'
runc 'echo "空词表：for x in ; do echo 不该出现; done"; for x in ; do echo "不该出现"; done; echo "  一次都没跑，退出码=$?"'
runc 'echo "没有位置参数时："; for a in "$@"; do echo "  不该出现"; done; echo "  退出码=$? ｜ 注意它和上面那条都不报错 —— 空循环在 bash 里是 0 不是失败"'
note '# ★ 带走第一条：循环变量是**普通 shell 变量**。它带着上一个循环留下的值进来，'
note '#   带着最后一个值出去。所以下面这条「看起来在数个数」的写法是坏的：'
runc 'n=0; for f in x y z; do :; done; echo "n=$n ← 全程没碰过 n，但它没报错，看着像跑了 3 次"'
note '#   数个数请用 "${#arr[@]}"（第 15 节）或干脆计数：for f in ...; do n=$((n+1)); done。'

# ============================================================================
banner "第 4 节 词表快照：循环里新建/删掉的文件，本轮根本看不见"
# ============================================================================
runc 'bash loopsnap.sh'
note '↑ 四段全是同一件事的两面：`for` 的词表在进入循环**之前**就摊成了一排固定的词。'
note '  所以「跑一遍目录里的文件，处理完就删掉它」这种写法，删掉的永远是上一轮的、看不见的永远是新出现的。'
note '  要跟着目录走只能换结构：while + 每轮重新 ls，或者 find -print0 | while read -d（第 15 节）。'

# ============================================================================
banner "第 5 节 通配符匹配到 0 个文件时，循环里有三种人生"
# ============================================================================
note '# 沙箱里现在有什么、故意点一个不存在的名：'
runc 'ls; echo "----- 下面全部用 nosuch*.txt 这个匹配不到任何东西的图案 -----" '
note '# ① 默认（nullglob/failglob 都关）：图案**原样**当词交给循环，循环照跑一轮。'
runc 'for f in nosuch*.txt; do echo "  拿到的是 [$f] ← 这不是文件，是图案本身"; done; echo "  跑了 1 轮"'
note '#   灾难现场是 rm / cp 这类拿到字面图案的写法：它不会删你的文件，但会让脚本一脸认真地报错。'
runc 'for f in nosuch*.txt; do rm "$f"; done; echo "  退出码=$?"'
note '# ② nullglob：匹配不到就展开成**空**，循环一次都不进。这是「按文件列表干活」最该有的开关。'
runc 'shopt -s nullglob; n=0; for f in nosuch*.txt; do n=$((n + 1)); done; echo "  进了 $n 轮 ← 循环体压根没执行"; shopt -u nullglob'
note '# ③ failglob：匹配不到就**当场报错、整条命令不执行**，脚本类工具里想「宁可炸不能静默」时用它。'
runc 'shopt -s failglob; for f in nosuch*.txt; do echo 不该出现; done; echo "  退出码=$?"'
note '# ★ 三种人生选哪种，取决于「没匹配到」算不算正常。默认那一档最危险：它把「没文件」'
note '#   伪装成「有一个叫 nosuch*.txt 的文件」。第 22 节会看到本章所有脚本都不依赖默认档。'

# ============================================================================
banner "第 6 节 for (( )) 只吃整数，以及无限循环怎么被掐掉"
# ============================================================================
runc 'for ((i = 1; i <= 3; i++)); do printf "%s " "$i"; done; echo "  ← 三段都是算术上下文，不需要 \$(( ))"'
runc 'N=3; for ((i = 1; i <= N; i++)); do printf "%s " "$i"; done; echo "  ← 这里能写变量，因为它本来就是算术上下文"'
note '# 但**花括号展开**不是算术上下文：它发生在变量替换之前。这一条和第 02 章直接相关：'
runc 'N=3; echo "  {1..\$N} 展开成：[{1..$N}] ← 一个字都没动"; echo "  \$(seq 1 \$N) 展开成：[$(seq 1 $N)] ← 这条才是对的写法"'
runc 'for ((i = 0; i < abc; i++)); do echo "  不该出现"; done; echo "  非数字在算术上下文里当变量名查，查不到就是 0 → 条件 0<0 假 → 一轮都没跑，退出码=$?"'
runc 'for ((i = 0; i < 1.5; i++)); do echo "  第 $i 轮"; done; echo "  退出码=$? ← ★ for (( )) 不接受小数，直接语法错误"'
note '# 无限循环本身不是 bug，`for ((i=1;;i++))` 是合法的。真正的问题是它怎么停：'
runc 'for ((i = 1;; i++)); do echo "$i"; done | head -3; echo "  ← head 读完 3 行就关门，循环收到 SIGPIPE 死掉（第 04 章第 17 节那条）"'

# ============================================================================
banner "第 7 节 不加引号的展开会把词表切成几份"
# ============================================================================
runc 'v="a b  c"; echo "带引号："; for x in "$v"; do echo "  [$x]"; done; echo "不带引号："; for x in $v; do echo "  [$x]"; done'
runc 'arr=("a b" c d); echo "数组 \${arr[@]} 带引号："; for x in "${arr[@]}"; do echo "  [$x]"; done; echo "不带引号（★ 空格被二次切开，元素数从 3 变 4）："; for x in ${arr[@]}; do echo "  [$x]"; done'
runc 'IFS=":"; v="a b:c"; for x in $v; do echo "  [$x]"; done; unset IFS'
note '# ★ 循环头里的展开和命令行上任何位置的展开是同一条规矩（第 02 章）：'
note '#   不带引号的 $var / ${arr[@]} 会按 IFS 拆词，还会再做一次通配展开。'
note '#   名单里可能有空格（真实文件名、用户输入）时，唯一的写法是 "${arr[@]}"。'

# ============================================================================
banner "第 8 节 while / until 是镜像：先看形状，再看第 19 节的重试环"
# ============================================================================
runc 'n=0; while [ "$n" -lt 3 ]; do n=$((n + 1)); done; echo "while [ n -lt 3 ] → n=$n"'
runc 'n=0; until [ "$n" -ge 3 ]; do n=$((n + 1)); done; echo "until [ n -ge 3 ] → n=$n ← 条件正好反过来，轮数一样"'
note '# 最容易想当然的是「一次都不跑」那一档：'
runc 'n=9; while [ "$n" -lt 3 ]; do echo 不该出现; done; echo "while 一进来就假 → 0 轮，退出码=$?"'
runc 'n=9; until [ "$n" -ge 3 ]; do echo 不该出现; done; echo "until 一进来就真 → 0 轮，退出码=$?"'
runc 'while false; do echo 不该出现; done; echo "while false → 0 轮，退出码=$? ｜ 反过来说 until false 就是无限循环，见第 6 节最后一条"'
note '# ★ 停止条件写反一次，后果是「一次都不干」或「永远干不完」。第 19 节把这两档都演成现场，'
note '#   并且给「必须停下来」的那一支补上界。'

# ============================================================================
banner "第 9 节 read 的最小语义：它的退出码问的不是「读到字了吗」"
# ============================================================================
runc 'printf "hello\n" | { read x; echo "read x → x=[$x] ｜ 退出码=$? ← 内容给了点名的变量"; }'
runc 'printf "hello\n" | { read; echo "裸 read → REPLY=[$REPLY] ｜ 退出码=$? ← 没点变量名时整行进 REPLY 这个 bash 保留变量（第 16 节 select 也用它）"; }'
runc 'printf "没有结尾换行" | { read x; rc=$?; echo "read 退出码=$rc ← ★ 撞 EOF 没读到换行，算失败"; echo "但 x=[$x] ← 内容已经拿到了"; }'
runc 'printf "\n" | { read x; echo "空行：read 退出码=$? ｜ x=[$x]"; }'
runc 'read x < /dev/null; echo "立刻 EOF：退出码=$?（不是超时，超时是第 11 节那个 142）"'
note '# 这一节立的规矩，第 12 节（最后一行被吞）和第 13 节（循环体偷吃 stdin）都是从它长出来的：'
note '#   read 的退出码 = 「有没有撞到分隔符」，不是「有没有拿到字符」。'

# ============================================================================
banner "第 10 节 一行拆成多个字段：末变量拿剩余，IFS 说了算"
# ============================================================================
runc 'printf "a b c d\n" | { read p q r; echo "read p q r → p=[$p] q=[$q] r=[$r] ← ★ 最后一个变量拿走剩下的全部"; }'
runc 'printf "x,,y,z\n" | { IFS=, read a b c d; echo "IFS=, 切 x,,y,z → a=[$a] b=[$b] c=[$c] d=[$d] ← 连续逗号不折叠，中间那个空是真的空"; }'
runc 'printf "  前后空白都要  \n" | { IFS= read -r line; echo "IFS= read -r → [$line] ← 不拆词、不处理反斜杠，整行原样"; }'
runc 'printf "one\\ttab\ttwo\n" | { read a b c; echo "默认 IFS 也吃制表符：a=[$a] b=[$b] c=[$c]"; }'
note '# ★ 两条常错：① 想「整行原样」必须 IFS= read -r，只写 read 会把首尾空白和连续空白吃掉；'
note '#   ② read 的字段数是写死的，源里的列数变了不会报错，只会让最后一个变量默默变长。'

# ============================================================================
banner "第 11 节 read 的开关柜：-t 的超时码、-p 在非终端为什么不显示"
# ============================================================================
runc 'sleep 2 | { read -t 0.3 x; echo "read -t 0.3 但没人喂 → 退出码=$? ← ★ 超时是 128+14=142，和 EOF 的 1 是两回事"; }'
runc 'read -p "这个提示你看得见吗？" x < /dev/null; echo "read -p 在非终端 stdin 下：退出码=$? ｜ 屏幕上一行提示都没有 ← 提示只在 stdin 是终端时才印"'
runc 'printf "abcdef\n" | { read -n 3 x; echo "read -n 3 → x=[$x] ｜ 剩下的 [def] 还挂在输入里，下一个 read 拿走它"; }'
runc 'printf "a\\0b\\n" | { read -d "" x; echo "  第一句：退出码=$? ｜ x=[$x] ← -d 空串 = 拿 NUL 当分隔符，只吃到那个 NUL 就停"; read -d "" y; echo "  第二句：退出码=$? ｜ y=[$y] ← NUL 之后的 b 还挂在输入里；这句撞到 EOF（没等到分隔符）所以退 1"; }'
runc 'printf "路径： a\\\\b\\n" | { read x; echo "普通 read 把 \\\\ 当转义吃掉 → x=[$x]"; }'
runc 'printf "路径： a\\\\b\\n" | { read -r y; echo "read -r 原样保留 → y=[$y]"; }'
note '# ★ -t 的 142 和 EOF 的 1 必须分开处理：等不到输入是「还没好」，读到结尾是「没有了」，'
note '#   把两者都当失败的重试环会提前放弃。第 19 节的重试写法就靠这条分类。'

# ============================================================================
banner "第 12 节 最后一行没有换行符就被吞掉：readlast.sh"
# ============================================================================
runc 'bash readlast.sh'
note '↑ 两份文件只差 1 个字节（最后一个换行符），坏写法就少跑一轮 —— 这是本章命中率最高的一个 bug。'
note '  修法只有一条值得背：while read -r x || [ -n "$x" ]。其余两条（补换行、mapfile）在第 15 节。'

# ============================================================================
banner "第 13 节 循环体偷吃 stdin：readsteal.sh"
# ============================================================================
runc 'bash readsteal.sh'
note '↑ 4 行名单只跑了 1 轮：循环体里那句会读标准输入的命令（这里用裸 cat 模拟 ssh）把剩下的 3 行端走了。'
note '  三条修法各有适用面：喂 /dev/null 最省事；名单挂 fd 3 最干净；先读进数组则和第 15 节同源。'

# ============================================================================
banner "第 14 节 管道里的 while 是另一个进程：lastpipe 与三种接回办法"
# ============================================================================
note '# 这一节和第 04 章第 3 节是同一件事，但发生在循环里，坑得更隐蔽：计数器在循环体里明明加了。'
runc 'n=0; printf "a\nb\nc\n" | while read l; do n=$((n + 1)); done; echo "循环里加了 3 次，外面看 n=$n ← ★ 最后一段是子进程，它的赋值带不回来"'
note '# ① lastpipe：让**最后一段**回到当前 shell（要求作业控制关着，非交互 bash 默认就是关的）。'
runc 'shopt -s lastpipe; n=0; printf "a\nb\nc\n" | while read l; do n=$((n + 1)); done; echo "shopt -s lastpipe 之后 n=$n"'
note '# ② 把要带出来的东西变成「输出」，用命令替换接住：'
runc 'n=$(printf "a\nb\nc\n" | while read l; do echo x; done | wc -l); echo "用 wc -l 数出来 n=$n"'
note '# ③ 干脆不走管道：把输入用重定向交给当前 shell 的循环（第 15 节的 mapfile 是它的加强版）。'
runc 'n=0; while read l; do n=$((n + 1)); done < <(printf "a\nb\nc\n"); echo "while ... < <(...) → n=$n ← 循环留在当前进程里，赋值保住了"'
note '# ★ 三条里最该背的是最后一条的判断依据：**需要把循环体里的状态带出来，就别让循环进管道**。'
note '#   lastpipe 只救最后一段，且要求 set +m；中间段的赋值照样丢。'

# ============================================================================
banner "第 15 节 一次读全：mapfile 的全部开关和它的适用边界"
# ============================================================================
runc 'bash mapfill.sh'
note '↑ mapfile 把「逐行 read」的三件麻烦（最后一行没换行、循环体偷 stdin、管道里丢状态）一次性消掉，'
note '  代价是整份输入进内存 —— 第 7 小段专门量了这个边界，第 21 节把它和流式读对比。'

# ============================================================================
banner "第 16 节 select：它是菜单，不是「遍历」"
# ============================================================================
runc 'bash selectmenu.sh'
note '↑ 每一条 select 都自带输入（done <<< "..."），因为 select 的语义就是「读用户输入」——'
note '  在非交互脚本里它必须有人喂，喂不到就一次都不跑、还返回非 0。'
note '  三条反直觉的：REPLY 装编号、item 装内容；非法输入不报错只给空串；菜单和提示符走 stderr。'

# ============================================================================
banner "第 17 节 break / continue 的层数，和 bash 根本没有循环标签"
# ============================================================================
runc 'for i in 1 2; do for j in 1 2 3; do [ "$j" = 2 ] && break; printf "  i=%s j=%s\n" "$i" "$j"; done; done; echo "  ← 裸 break 只出内层，外层照跑（第 02 轮 j=1 又打了一次）"'
runc 'for i in 1 2; do for j in 1 2 3; do [ "$j" = 2 ] && break 2; printf "  i=%s j=%s\n" "$i" "$j"; done; done; echo "  ← break 2 一次出两层，整个嵌套当场结束"'
runc 'for i in 1 2 3; do for j in a b c; do if [ "$j" = b ]; then continue 2; fi; printf "  i=%s j=%s\n" "$i" "$j"; done; done; echo "  ← continue 2 不是「跳过内层剩下的」，是「直接进外层下一轮」"'
runc 'for i in a b; do break 9; done; echo "  break 9 超出层数：退出码=$? ｜ 上面一行都没打 ← ★ 它把所有循环都跳掉了，脚本继续"'
note '# ★ bash 没有 C/Java 那种 label。要「从第三层跳回最外层」只有三条路：break N（数得清层数时）、'
note '#   把内层包进函数用 return（最稳）、或者置一个 flag 让外层条件去查。'
runc 'lookup() { local n=0; for a in 1 2; do for b in 3 4; do for c in 5 6; do n=$((n + 1)); [ "$c" = 6 ] && { echo "  命中：第 $n 次内层迭代，在这里 return"; return 0; }; done; done; done; echo "  走不到这行（$n）"; return 1; }; lookup; echo "  ← 三层循环里的 return 一次跳到底；函数的退出码=$?"'

# ============================================================================
banner "第 18 节 作业控制：& / \$! / jobs / wait / disown / kill -0"
# ============================================================================
runc 'bash jobwait.sh'
note '↑ 这一节把「后台作业的失败去哪了」问到底：wait 是拿到后台退出码的唯一办法，'
note '  而 wait 返回 0 有三种成因（真成功 / 翻旧账 / 没孩子可等）。'
note '  所有 pid 都折成 <pid> 或换成「相等吗」，因为这个数字每跑必变，而关系每次都一样。'

# ============================================================================
banner "第 19 节 重试环：until 的正解，和它必须带的上界"
# ============================================================================
runc 'bash untilretry.sh'
note '↑ 前四段用计数模拟出「第 3 轮才 ready」，所以逐字可复现；第 5 段是真异步轮询，'
note '  那一段的轮询次数每次跑都可能不同 —— 脚本里只报「等到了吗」这个布尔值。'

# ============================================================================
banner "第 20 节 限并发：一把全扔 vs 数闸门 vs wait -n"
# ============================================================================
runc 'bash parlimit.sh'
note '↑ 第 2 小段「一把全扔」这里只有 6 个任务，什么毛病都看不出来 —— 它危险在「任务数不由你定」的那一天。'
note '  本章没有真跑一千个任务去撞 ulimit -u（第 1 节那个 50372 是这台机器的上限），所以「放大就会撞」只当推理给出，不当证据；两把闸门里 wait -n 那把更省：不用轮询，但它「等任意一个结束」的语义要求你只 wait 自己起过的。'

# ============================================================================
banner "第 21 节 循环贵在哪里：词表形状与一次 fork"
# ============================================================================
runc 'bash loopspeed.sh'
note '↑ 这一节全程不掐表：耗时每跑必变，能复现的是「词表被摊成几个元素」「循环体出现了几个不同 pid」。'
note '  第 3 小段有个自己踩过的坑值得一看：用 $(BASHPID) 去问「我在哪个进程」，量到的是那个命令替换'
note '  自己 fork 出来的子进程 —— 主 shell 的 pid 得在主 shell 里当场比。'

# ============================================================================
banner "第 22 节 自证：沙箱里的事只留在沙箱，章目录一个字都没多"
# ============================================================================
note '# demo 的写操作全部发生在 mktemp -d 的沙箱里；此刻的当前目录就是沙箱的 work/，'
note '#   本章跑出来的文件全在这里（跑完整体删掉，仓库目录从头到尾只读）：'
runc 'echo "沙箱 work/ 里的文件数=$(ls | wc -l)"; ls'
note '# 九个辅助脚本各自的现场目录（loopsnap/readlast/readsteal/selectmenu/untilretry/jobwait/'
note '#   mapfill/parlimit/loopspeed）在脚本内部已经自删并当场自证，所以这里应该只剩 *.sh：'
runc 'echo "work/ 里剩下的非 .sh 文件个数=$(ls -A | grep -vc "\.sh$")"; ls -A | grep -v "\.sh$" || echo "  （无）"'
note '# 本章目录里应该只有源文件：demo.sh + 九个辅助 .sh，没有任何 demo 产生的文件：'
runc_at "$HERE" 'find . -maxdepth 1 -type f | sort'
runc_at "$HERE" 'echo "子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)"'
note '# 后台残留：本章起过的所有进程都是 sleep 或沙箱内的短命令，且都在同一条命令里 wait/kill 收尾。'
note '#   下面两条都挑的是**能被弄响**的写法，不是摆样子 —— 第一条尤其要注意它不走 runc。'
note '# ★ 为什么不走 runc：runc 起的是 bash -c 子进程，那里是一张全新的空作业表，'
note '#   拿它自证「本章收尾了」量到的是假 0。所以这条直接在 demo 自己的进程里问：'
printf '\n%s$ jobs -p | wc -l%s\n' "$C" "$R"
printf '  本 demo 进程的作业表行数=%s ← 应为 0（第 18 节讲的正是这张表归谁）\n' "$(jobs -p | wc -l)"
printf '\n%s$ pgrep -c -f "[l]oopsnap|[r]eadlast|[r]eadsteal|[s]electmenu|[u]ntilretry|[j]obwait|[m]apfill|[p]arlimit|[l]oopspeed" || true%s\n' "$C" "$R"
printf '  本章九个辅助脚本里还活着的进程数=%s ← 应为 0\n' \
    "$(pgrep -c -f '[l]oopsnap|[r]eadlast|[r]eadsteal|[s]electmenu|[u]ntilretry|[j]obwait|[m]apfill|[p]arlimit|[l]oopspeed' || true)"
note '#   模式每个词都写成 [x]xxx 是**防自捕**：pgrep -f 匹配的是整条命令行，照直写 "loopsnap" 就把正在执行'
note '#   这条命令的 bash 自己也数进去了（它的 argv 里就有那个字符串），量出一个永远不为 0 的假数。'
note '#   结尾挂 || true 是因为 pgrep 找不到匹配时自己退 1 —— 不挂的话这个 0 压根印不出来。'
note '# 本章没起过监听端口、没调用过任何网络命令，这是本章的设计约束；不贴 ss 的监听表，'
note '#   因为那几行是本机其它服务的端口号，每跑必变，贴进书里就永远复现不了（本机端口归属怎么看是另一件事，'
note '#   在 ssh 手册第 05 章《端口转发》那条线上；本章只需要它证明「这里没有本章的贡献」）。'
