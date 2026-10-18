#!/usr/bin/env bash
# CHECK: RUN=非复现 理由：第 3/13/14/15 节里 strace -f 的总行数、多进程日志两两 diff 的命中数由内核调度决定，同一台机器两趟就是两个值（第 13 节第 9 格把这当作教学内容演出来了）
#        免除的证据只有「两遍逐字相同」这一条；rc、残留、隐私、格子里的断言全部照判。
# ============================================================================
# 第 09 章《出事怎么查：让 bash 自己交代、不跑也能查、时间与系统调用》—— 整章证据一键复现
#
#   bash demo.sh            跑完自动删沙箱（默认）
#   bash demo.sh --keep     跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt  重定向时自动关掉颜色，README 引用的就是这份纯文本
#
# 屏幕上出现的每一行，都是这条命令在这台机器上的真实结果 —— 没有一行是手写的。
#
# 这一章要还的三笔账（六支取证脚本 + 下面 18 节全部挂在其中一条上）：
#   主线① **让 bash 自己交代**：-x 说「我要跑什么」、-v 说「我读到什么」，PS4 决定每行前缀，
#          BASH_XTRACEFD 决定这堆字去哪儿，DEBUG 陷阱决定「源码行号还作不作数」。
#          它们全是**说话**，不改行为 —— 唯一会改行为的是「陷阱体自己失败 + -e」。
#   主线② **不跑也能查**：-n 只看语法、-o noexec 读完一行都不执行、-u 抓没定义的名字；
#          三层各挡一面，三层全过了程序照样能错（第 9 节第 1、2 格实测）。
#   主线③ **口径**：bash 的 time 和 /usr/bin/time 不是同一把尺；strace 换一次 -e 就是换一次尺，
#          -f 决定看不看得见子进程，-yy 才让符号链接的目标现形；带 -f 的日志逐行不可复现。
#
# 本章的取证路线（18 节，全部真跑）：
#   第 1 节   钉死环境：bash 版本、语言环境（本章把报错钉成中文并当场验证）、外部命令厂商、
#             shellcheck 在场性、/bin/sh 真身
#   第 2 节   一张地图：三条主线各一条最小证据
#   第 3 节   xtrace.sh 完整跑
#   第 4 节   小证据：trace 输出能不能当回归基线（两遍 -x 先数行数再 diff；PS4 两种引号对比）
#   第 5 节   ps4fd.sh 完整跑
#   第 6 节   小证据：拿到一份 trace，一步定位「第一个变成空的值」
#   第 7 节   debugtrap.sh 完整跑
#   第 8 节   小证据：同一脚本并排跑 -x 与 DEBUG 陷阱，只比计数，看 -T 值多少条
#   第 9 节   staticcheck.sh 完整跑
#   第 10 节  小证据：同一个语法错从三个入口进来，指路的第一个字段各长什么样（rc 反倒都是 2）
#   第 11 节  timing.sh 完整跑
#   第 12 节  小证据：耗时不能上屏，但「掩码后的形状」能，而且两遍一样
#   第 13 节  tracewho.sh 完整跑
#   第 14 节  小证据：用 strace 复核名字 —— \ls 与 command ls 到底 exec 了谁
#   第 15 节  综合演练：一个坏脚本按五步配方走完全程（-n → -u → -x+PS4 → DEBUG → strace）
#   第 16 节  把同一个坏脚本交给 /bin/sh（dash）：哪几件设施直接没了
#   第 17 节  对账：六支脚本各几格、合计几格、demo 自己几节（守卫先故意弄响）
#   第 18 节  自证：章目录开跑前后一致、沙箱里只剩该有的东西、本章没联网没装包没 -p 挂别人进程
#
# 安全说明：
#   * 所有写操作只发生在 mktemp -d 的沙箱里（前缀 bash09-），由 trap cleanup EXIT 整体删掉；
#     六支取证脚本各自只在自己的子目录（xt p4 d9 sc tm tw）里造文件，跑完自己删并当场自证。
#   * 绝不修改 ~/.bashrc、~/.profile、~/.config、~/.local、/etc：要看「另一个 shell」的行为
#     一律用 bash -c / sh -c 起新进程；set -x、set -o noexec、PS4、DEBUG 陷阱只活在被演示的那个子进程里。
#   * strace 只追本脚本自己起的进程：每条 strace 都写成 `strace … bash xxx.sh` 这种自己起、自己等的样子，
#     绝不用 -p 挂到任何已存在的进程上（第 18 节把这条扫给你看，并且先证明那个计数器响得起来）。
#     本章不开监听端口、不联网、不装包。后台调用全章只有一处：第 7 节第 2 格里那句被演示的
#     `echo 后台 & wait` —— 它活在一个被演示的子进程里，同一行就被 wait 收掉，没有「发出去不管」的。
#   * sleep 只出现在第 11、12 节，作用是给 time 一点活干：最短 0.05 秒，最长一次 1 秒
#     （第 11 节第 6 格拿它演「SECONDS 只有整秒，量 50 毫秒会得到 0」）。数字一律只以掩码后的形状上屏。
#   * 屏幕上不出现真实身份：家目录、用户名、主机名一律当场折成 <家目录> <用户> <主机名>；
#     沙箱的绝对路径（含 mktemp 随机后缀）在每一节的输出上都折成 <沙箱>，只有 --keep 时才给完整路径。
#     ★ 本机 hostname 与 $USER 是同一个字符串，折叠规则谁在前谁赢 —— 第 5 节第 5 格把这件事演了一遍。
#   * 耗时、内存占用、strace 带 -f 的总行数一律不裸着上屏：它们每跑必变。
#     本章能给的确定性是「计数、掩码后的形状、排序、布尔、退出码」这五种。
#
# 为什么这个脚本**故意不加** set -e / -u / -o noexec：
#   本章的原料就是失败现场：127 的找不到命令、语法错的 2、爆栈时空掉的 stderr、
#   DEBUG 陷阱体自己失败把脚本掐死那一段、dash 那边 `trap DEBUG` 与 `set -T` 直接报错的那两句。
#   全局加任何一件，第 2 节第一条就把 demo 停住 —— 而「出事怎么查」这件事本身
#   就要求宿主进程能在错误里继续往下走。第 9、10、15 节里那些开关只在被演示的子进程里开。
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) printf '未知参数：%s（本脚本只认 --keep）\n' "$arg" >&2; exit 2 ;;
    esac
done

# 把报错钉成中文：这台机器的 bash 报错本来就是中文的（「未找到命令」「无效的格式字符」），
# 六支取证脚本的解说词按中文原文写，所以 demo 显式钉住中文语言环境，README 的围栏才和屏上一致。
# 换语言只改「那句话用什么字写」，不改 rc、计数与形状 —— 第 1 节末尾当场验证，并且自带告警分支。
ORIG_LANG="${LANG:-（未设）}"
ORIG_LC_ALL="${LC_ALL:-（未设）}"
ORIG_LANGUAGE="${LANGUAGE:-（未设）}"
export LANG=zh_CN.UTF-8 LC_ALL=zh_CN.UTF-8 LANGUAGE=zh_CN:zh
export ORIG_LANG ORIG_LC_ALL ORIG_LANGUAGE

if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/bash09-XXXXXX")
BEFORE_LIST=$(cd "$HERE" && ls -A | sort | tr '\n' '|')
BEFORE_SUM=$(cd "$HERE" && wc -c *.sh | tail -1 | awk '{print $1}')
# ★ 第 18 节的自证是拿 bash -c 起新进程跑的，不设 export 就看不见这三个变量，
#   于是那条「章目录一致」会退化成「空串 = 空串」恒真 —— 这是本章第 17 节反复演的那类假守卫，别在自己身上犯。
export HERE BEFORE_LIST BEFORE_SUM

banner() { printf '\n══════ %s %s\n' "$1" "════════════════════════════════"; }
note() { printf '%s\n' "$1"; }

# 身份折叠：任何可能被 bash 带上家、带上沙箱的原文，上屏前先过一遍（-yy、绝对路径、$PWD 都靠这条兜底）。
fold() { sed -E "s#$PWD#<沙箱>#g; s#$HOME#<家目录>#g; s#\\b$USER\\b#<用户>#g; s#$(hostname)#<主机名>#g"; }

# 打印命令 → 用 bash -c 真跑 → 打印退出码。用 bash -c 是为了每条都在全新的非交互 bash 里开始：
# 上一节开的 set -x / PS4 / 陷阱不会串到下一节。2>&1 把报错并进 stdout，捕获里才留得下原文。
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1 | fold
    printf '[退出码 %d]\n' "${PIPESTATUS[0]}"
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去接着敲： cd "%s/work"     看完删掉： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] 前缀 bash09-（随机后缀不进屏幕；只有 --keep 时才印出完整路径）\n'
    fi
}
trap cleanup EXIT

# 六支取证脚本 + 本 demo 自己复制进沙箱，所有加工都在副本上进行（脚本里写的都是相对路径）。
# ★ 必须用相对文件名跑（bash xtrace.sh，不是 bash /绝对路径/xtrace.sh）：bash 的报错原文带上的是 $0，
#   跑绝对路径就把沙箱绝对路径印上屏了 —— 这一章第 10 节专门演这件事。
mkdir -p "$SB/work"
cp "$HERE"/*.sh "$SB/work/"
cd "$SB/work" || exit 1
umask 022

# ============================================================================
banner "第 1 节 先把这台机器的发言权确认一遍"
# ============================================================================
note '# 本章每条结论都跟三件事绑定：哪个 bash、外部命令是谁家的、报错用什么语言说。'
note '# 第三件本章特别要紧 —— 六支脚本的解说词是按中文报错写的，语言环境一换，原文就换字。'
runc 'bash --version | head -1'
runc 'echo "demo 已把自己改成：LC_ALL=[$LC_ALL] LANG=[$LANG] LANGUAGE=[$LANGUAGE]"'
runc 'echo "你原来的终端是 LANG=$ORIG_LANG、LC_ALL=$ORIG_LC_ALL、LANGUAGE=$ORIG_LANGUAGE —— 只在 demo 的子进程里被改掉，你的 shell 一个字都没动"'
runc 'for t in sed awk strace tr wc grep time; do printf "  %-7s → %s\n" "$t" "$(command -v "$t" || echo 不在 PATH)"; done'
note '#   注意 time 那一行：command -v 答的是「time」不是路径 —— 它先是 shell 关键字，第 11 节第 1 格拆它的三重身份。'
runc 'strace -V 2>&1 | head -1'
runc 'sed --version 2>&1 | head -1'
runc 'printf "  awk 自报家门=[%s] ｜ command -v awk 的真身=[%s]\n" "$(awk -W version 2>&1 | head -1)" "$(readlink -f "$(command -v awk)")"'
note '#   两个都要看：-W version 是 mawk 的旗号（GNU awk 也认这个写法），readlink 才告诉你打包时 awk 链给了谁。'
note '#   本章所有 awk 正则都按这台机器的 awk 实测过 —— mawk 不认 POSIX 的那些扩展，第 6 节给一个当场炸的例子。'
note '# 本章用到、但本机不在场的那个工具，先把话说在前面：'
runc 'printf "  shellcheck 在场吗=%s ｜ command -v 给的=[%s]\n" "$(command -v shellcheck >/dev/null 2>&1 && echo 在 || echo 不在)" "$(command -v shellcheck 2>&1 | head -1 | fold)"'
note '#   ★ 不在场就不演它。第 9 节用的静态检查全是 bash 自己就能做的（-n、-o noexec、-u）。'
runc 'printf "  /bin/sh 真身=[%s]\n" "$(readlink -f /bin/sh)"'
note '# 语言环境到底改不改判定？同一条错误在两种语言下各来一次，只看 rc 变不变：'
runc 'o1=$(bash -c nosuchcmd 2>&1 | head -1); r1=$(bash -c nosuchcmd >/dev/null 2>&1; echo $?)
o2=$(env LC_ALL=C.UTF-8 LANGUAGE=C bash -c nosuchcmd 2>&1 | head -1); r2=$(env LC_ALL=C.UTF-8 LANGUAGE=C bash -c nosuchcmd >/dev/null 2>&1; echo $?)
printf "  钉好的中文那句=[%s]\n  换英文那句=[%s]\n" "$(printf %s "$o1")" "$(printf %s "$o2")"
printf "  两次的 rc=[%s / %s] ｜ 两句字面是否相同=%s\n" "$r1" "$r2" "$( [ "$o1" = "$o2" ] && echo 相同 || echo 不同字 )"'
runc 'bash -c nosuchcmd 2>&1 | grep -c 未找到命令 | sed "s/^/  这一趟命中「未找到命令」的行数=/"'
if [ "$(bash -c 'nosuchcmd' 2>&1 | grep -c 未找到命令)" -eq 0 ]; then
    note '  ★ 警告：这台机器没有中文语言包，报错会出英文 —— README 围栏里的中文原文会和你看到的不一致。'
    note '    但 rc、计数、形状、判定全部不变；本章结论没有一条依赖那句话用什么字写。'
else
    note '  → 钉住了：这份捕获里的报错原文是中文。同一处换成 C.UTF-8 会印 command not found，'
    note '    所以本章所有判据都写成与语言无关的东西（rc、命中次数、掩码后的形状）—— 第 4、12、14 节各演一次。'
fi

# ============================================================================
banner "第 2 节 一张地图：三条主线各一条最小证据"
# ============================================================================
note '# 三条主线、三句话，每句配一条当场跑出来的最小证据。后面 16 节都是这三句的展开。'
cat > map1.sh <<'EOF'
a=1
echo "用了 $a"
EOF
note '① 让 bash 自己交代：同一份脚本，-x 打「要跑的」（展开之后），-v 打「读到的」（源文本原样）。'
cat > map1t.sh <<'EOF'
x=$(bash -x map1.sh 2>&1 | wc -l)
v=$(bash -v map1.sh 2>&1 | wc -l)
b=$(bash -x -v map1.sh 2>&1 | wc -l)
printf "  -x 行数=%s ｜ -v 行数=%s ｜ 两个一起开=%s\n" "$x" "$v" "$b"
printf "  按「正菜只打一次」算 %s+%s-1=%s ｜ 跟一起开那个数对得上吗=%s\n" \
  "$x" "$v" "$((x + v - 1))" "$([ "$((x + v - 1))" -eq "$b" ] && echo 对上了 || echo 对不上★)"
printf '  一起开打出的全部 %s 行原文（其中 -v 两行=源文本、-x 两行=展开后、末行=正菜自己打的）：\n' "$b"
bash -x -v map1.sh 2>&1 | sed 's/^/      /'
EOF
runc 'bash map1t.sh'
note '   ★ 这一格故意只用两行脚本：行数越少，那个等式越容易一眼验 —— 两个都开时不是 3+3=6，因为正菜只打一次。'
note '     第 3 节把 -v 与 -x 的分工钉死，第 5 节管「每行前缀由谁决定」。'
note '② 不跑也能查：这份脚本语法没问题，-n 与 -o noexec 都一声不响、rc 都是 0。'
cat > map2.sh <<'EOF'
: 未开始的活
EOF
cat > map2t.sh <<'EOF'
printf "  bash -n rc=%s ｜ bash -o noexec rc=%s ｜ noexec 有没有输出=[%s]\n" \
  "$(bash -n map2.sh >/dev/null 2>&1; echo $?)" \
  "$(bash -o noexec map2.sh >/dev/null 2>&1; echo $?)" \
  "$(bash -o noexec map2.sh 2>&1 | head -1)"
EOF
runc 'bash map2t.sh'
note '   ★ noexec 的「不执行」是整份读完之后一行都不跑：第 9 节演「中途 set -o noexec 为什么回不来」。'
note '③ 口径：bash 的 time 和 /usr/bin/time 不是同一把尺，连它们认的百分号字母都不是同一套。'
cat > map3t.sh <<'EOF'
TIMEFORMAT='%R %U %S'
{ time : ; } >map3_out.txt 2>map3_err.txt
printf "  内建 time 打的行数=%s ｜ 外部 time 认百分号 M 吗（认则输出一行数字）=%s\n" \
  "$(wc -l < map3_err.txt)" \
  "$( { /usr/bin/time -f %M true; } 2>&1 | sed -n '/^[0-9]/p' | wc -l )"
EOF
runc 'bash map3t.sh'
note '   ★ 第 11 节把 TIMEFORMAT 认的字母表整个扫一遍；第 13 节把 strace 的尺也扫一遍。'
rm -f map1.sh map1t.sh map2.sh map2t.sh map3t.sh map3_out.txt map3_err.txt

# ============================================================================
banner "第 3 节 xtrace.sh 完整跑：让 bash 把每一步说出来"
# ============================================================================
note '# 10 格：-v 与 -x 各说什么、少引号那行为什么被拆成两条、trace 给词补引号的规则、'
note '# trace 走的是 stderr、哪些容器继承、泄密怎么遮、trace 行数不等于源码行数、两遍对拍。'
runc 'bash xtrace.sh'

# ============================================================================
banner "第 4 节 小证据：trace 输出能不能当回归基线"
# ============================================================================
note '# 排障时最常问的一句是「我把这份 trace 存下来，下次跑能不能直接 diff」。'
cat > base.sh <<'EOF'
f() { local x=$1; echo "x 是 $x"; }
g() { f "$1"; }
g 甲
g 乙
EOF
cat > basecheck.sh <<'EOF'
bash -x base.sh 2>&1 | grep "^+ " > b1.txt
bash -x base.sh 2>&1 | grep "^+ " > b2.txt
printf "  两遍的 trace 行数=%s/%s ｜ 两遍 diff 命中=%s 行\n" \
  "$(wc -l < b1.txt)" "$(wc -l < b2.txt)" "$(diff b1.txt b2.txt | sed -n '/^[<>]/p' | wc -l)"
printf "  两遍的源码行数（同一文件）=%s\n" "$(wc -l < base.sh)"
EOF
runc 'bash basecheck.sh'
note '  → 能当基线，前提是默认 PS4（不带 pid、不带时间戳）。行数与源码行数不等也已经印出来了。'
note '# 一旦把行号塞进 PS4，能不能对拍就看 PS4 用的是单引号还是双引号：'
cat > ps4check.sh <<'EOF'
PS4='+L$LINENO:S$SECONDS ' bash -x base.sh 2>&1 | grep "^+L" | head -2 | sed "s/^/    单引号版（每行现算）：/"
PS4="+L$LINENO:S$SECONDS " bash -x base.sh 2>&1 | grep "^+L" | head -2 | sed "s/^/    双引号版（定义时就算完）：/"
EOF
runc 'bash ps4check.sh'
note '# 单引号版每行给的是当时的行号；双引号版那个数在赋值那一刻就定死了，整份 trace 里它是常数 ——'
note '# 这就是第 5 节叫它「假行号」的原因。要不要把行号写进存档，是「trace 对拍」的第一道选择题。'
rm -f base.sh basecheck.sh b1.txt b2.txt ps4check.sh

# ============================================================================
banner "第 5 节 ps4fd.sh 完整跑：每行前缀由谁决定、这堆字去哪儿"
# ============================================================================
note '# 9 格：PS4 单引号与双引号、六种形状、加号层数从哪来、算术展开数得住而命令替换数不住、'
note '# 身份与 pid 上不上屏、BASH_XTRACEFD 三档、fd 是谁开的才算、trace 落盘的权限与家目录、对拍配方。'
runc 'bash ps4fd.sh'

# ============================================================================
banner "第 6 节 小证据：拿到一份 trace，一步定位第一个变成空的值"
# ============================================================================
note '# 本章最常用的一个动作：不读整份 trace，只找「哪一行开始，值没了」。'
cat > leak.sh <<'EOF'
name=张三
city=北京
job=""
printf '%s|%s|%s\n' "$name" "$city" "$job"
printf 'got %s\n' "$job"
EOF
cat > leakcheck.sh <<'EOF'
PS4='+ ' bash -x leak.sh > only_out.txt 2> trace.txt
printf "  trace 行数=%s ｜ stdout 行数=%s（trace 整个在 stderr 那一侧，第 3 节已钉）\n" \
  "$(wc -l < trace.txt)" "$(wc -l < only_out.txt)"
printf "  等号后面是空的赋值行有几条=%s\n" "$(sed -n '/^+ [a-z]*=$/p' trace.txt | wc -l)"
printf "  第一条空值在 trace 的第几行=[%s]\n" "$(grep -n '^+ [a-z]*=$' trace.txt | head -1)"
grep -n "^+ printf" trace.txt | head -2 | sed "s/^/    往下第一个用到它的地方：/"
printf "  同一件事交给 awk：写法一（^ 紧跟 +）报错原文=[%s]\n" \
  "$(awk '/^+ [a-z]*=$/ {print NR; exit}' trace.txt 2>&1 | head -1)"
printf "  写法二（把加号转义）=第 %s 行 ｜ 写法三（换成 grep 的判据）=第 %s 行\n" \
  "$(awk '/^\+ [a-z]*=$/ {print NR; exit}' trace.txt 2>&1 | head -1)" \
  "$(grep -n '^+ [a-z]*=$' trace.txt | head -1 | cut -d: -f1)"
EOF
runc 'bash leakcheck.sh'
note '  ★ 这里用的行号是「trace 的第几行」，不是源码行号 —— 两者不是一回事（第 3 节倒数第二格）。'
note '    要源码行号，得靠 PS4 里的 $LINENO（第 4 节那个单引号版）或者第 7 节的 DEBUG 陷阱。'
note '  ★ 第 1 节欠的那句「mawk 会当场炸给你看」就是写法一：POSIX 里 ^ 后面紧跟量词算非法，'
note '    本机这台 mawk 直接 compile failed、一个字符都不输出，rc=2（实测）—— 它不算隐蔽的坑，'
note '    隐蔽的是把这条 awk 的 stderr 单独引走（2>某文件）之后：屏上只剩「什么都没有」，看着像数据是空的。'
note '    同一条判据在 awk 里要么写成写法二，要么干脆交给 grep（写法三），三者给的行号必须是同一个数。'
rm -f leak.sh leakcheck.sh only_out.txt trace.txt

# ============================================================================
banner "第 7 节 debugtrap.sh 完整跑：DEBUG 陷阱给的是即将执行的那一条"
# ============================================================================
note '# 11 格：LINENO 指向即将执行的命令、子 shell/命令替换/source 文件三种静默缺失、'
note '# -E 不管 DEBUG 而 -T 才管、复合命令响几次、计数陷阱、陷阱体自己失败 + -e 会掐死自己、'
note '# extdebug 的 preexec、return 在顶层非法、精准过滤、调用栈三数组、DEBUG 与 ERR 并排。'
runc 'bash debugtrap.sh'

# ============================================================================
banner "第 8 节 小证据：同一脚本并排喂给 -x 与 DEBUG 陷阱，只看计数"
# ============================================================================
note '# 两支脚本各自演过自己那一半；这里把同一个样本同时喂两种设施，比「谁多报、谁漏报」。'
cat > both.sh <<'EOF'
v=1
for i in 1 2; do v=$((v+i)); done
if [ "$v" -gt 2 ]; then echo 大; else echo 小; fi
{ echo 甲; echo 乙; }
echo 丙 > /dev/null
EOF
cat > dt.sh <<'EOF'
trap 'printf "D\n"' DEBUG
source both.sh
EOF
cat > bothcheck.sh <<'EOF'
printf "  -x 报了几条命令      =%s\n" "$(bash -x both.sh 2>&1 | sed -n '/^+ /p' | wc -l)"
printf "  DEBUG 不带 -T 响几条 =%s\n" "$(bash dt.sh 2>&1 | sed -n '/^D$/p' | wc -l)"
printf "  DEBUG 带 -T  响几条  =%s\n" "$(bash -T dt.sh 2>&1 | sed -n '/^D$/p' | wc -l)"
EOF
runc 'bash bothcheck.sh'
note '  → 三个数的差就是这一节的结论：source 进来的那个文件不带 -T 就静默少报（第 7 节第 3 格），'
note '    而 -x 与 DEBUG 的差是「复合命令算几条」—— 所以本章的分工是：'
note '    想知道「到底跑了什么」用 -x；想要「源码行号、能在函数里停」用 DEBUG + -T；两个都要就并排。'
rm -f both.sh dt.sh bothcheck.sh

# ============================================================================
banner "第 9 节 staticcheck.sh 完整跑：不跑起来也能查出来的三层"
# ============================================================================
note '# 7 格：-n／-o noexec／-u 三层各挡什么、noexec 中途开就回不来、报错四段结构与 rc 127/1/139、'
note '# 设置打印族（set -o、shopt、裸 set、trap -p、declare -F 在 extdebug 下多两列）。'
runc 'bash staticcheck.sh'

# ============================================================================
banner "第 10 节 小证据：同一个错，三个入口的原文第一个字段不同（rc 反倒一样）"
# ============================================================================
note '# 排障最容易认错的是「这句错到底是哪个文件里的」。同一个没收尾的 if，从三个入口进来，'
note '# 第一个字段（bash 用来指路的那串 $0）各不相同，而 rc 三项都是 2 —— 所以这一节比的是原文不是 rc。'
cat > syn.txt <<'EOF'
echo 开
if [ 1 -gt 0 ]; then
echo 没收
EOF
cat > syncheck.sh <<'EOF'
r1=$(bash -n syn.txt >/dev/null 2>&1; echo $?)
r2=$(bash -c "$(cat syn.txt)" >/dev/null 2>&1; echo $?)
r3=$(bash syn.txt >/dev/null 2>&1; echo $?)
printf "  ① bash -n syn.txt        rc=%s\n" "$r1"
printf "  ② bash -c \"整段代码\"      rc=%s\n" "$r2"
printf "  ③ bash syn.txt（真跑）    rc=%s\n" "$r3"
printf "  三个 rc 互相相等吗=%s ｜ 而且都等于 2 吗=%s（不相等就是这一节的看点变了，得改解说）\n" \
  "$( [ "$r1" = "$r2" ] && [ "$r2" = "$r3" ] && echo 相等 || echo 不相等★ )" \
  "$( [ "$r1" = 2 ] && [ "$r2" = 2 ] && [ "$r3" = 2 ] && echo 是 || echo 不是★ )"
printf "  「echo 开」到底跑没跑（stdout 里有几行 开）：-n=%s ｜ -c=%s ｜ 真跑=%s\n" \
  "$(bash -n syn.txt 2>/dev/null | grep -c 开)" \
  "$(bash -c "$(cat syn.txt)" 2>/dev/null | grep -c 开)" \
  "$(bash syn.txt 2>/dev/null | grep -c 开)"
EOF
runc 'bash syncheck.sh'
note '# 再看三句原文各自出现的名字：'
cat > synwords.sh <<'EOF'
printf "  ① bash -n syn.txt 的全部输出（%s 行）：\n" "$(bash -n syn.txt 2>&1 | wc -l)"
bash -n syn.txt 2>&1 | sed "s|^|      |"
printf "  ② bash -c \"整段代码\" 的全部输出：\n"
bash -c "$(cat syn.txt)" 2>&1 | sed "s|^|      |"
printf "  ③ bash syn.txt 的全部输出：\n"
bash syn.txt 2>&1 | sed "s|^|      |"
printf "  ④ bash ./syn.txt 的最后一行=[%s]\n" "$(bash ./syn.txt 2>&1 | tail -1)"
printf "  ⑤ bash 绝对路径 的最后一行=[%s]（沙箱那截折成 <沙箱> 才敢上屏）\n" \
  "$(bash "$PWD/syn.txt" 2>&1 | tail -1 | sed -E "s#$PWD#<沙箱>#g")"
m3=$(bash syn.txt 2>&1 | tail -1); m4=$(bash ./syn.txt 2>&1 | tail -1); m5=$(bash "$PWD/syn.txt" 2>&1 | tail -1)
printf "  ③④⑤ 的第一字段分别是=[%s]｜[%s]｜[%s]\n" "${m3%%:*}" "${m4%%:*}" "${m5%%:*}"
printf "  三者互不相同吗=%s ｜ 把第一字段砍掉后，三句消息部分相同吗=%s\n" \
  "$( [ "$m3" != "$m4" ] && [ "$m4" != "$m5" ] && [ "$m3" != "$m5" ] && echo 是 || echo 不是★ )" \
  "$( [ "${m3#*: }" = "${m4#*: }" ] && [ "${m4#*: }" = "${m5#*: }" ] && echo 相同 || echo 不同★ )"
EOF
runc 'bash synwords.sh'
note '  ★ 一屏里三件事：'
note '    ① 只有 -n 那份「一个字都不执行」——上面那行「跑没跑」给的 0/1/1 就是它；② ③ 都先把 echo 开 跑了才在下一句翻脸，'
note '       ④ 与 ③ 是同一条命令换了个写法，所以也是 1。语法错不是整份脚本作废，是 bash 读到那一句才炸，'
note '       前面的副作用早落了地（对照第 9 节 -o noexec 的「一行都不跑」）。'
note '    ② 的第一字段是 bash: -c: —— 代码没落成文件就没有文件名，跟第 13 节 caller 那格的「1 NULL」同一个根。'
note '    ③④⑤ 的第一字段就是「你敲进去的那个串」原样：同一份文件，三种写法给出三个不同的指路串，'
note '       而砍掉第一字段后的消息部分一字不差（屏上那行断言就是在数这两件事）。'
note '       本章用相对文件名跑取证脚本，正是为了别把沙箱绝对路径印进捕获。'
rm -f syn.txt syncheck.sh synwords.sh

# ============================================================================
banner "第 11 节 timing.sh 完整跑：time 有几重身份、这把尺认哪些字母"
# ============================================================================
note '# 8 格：time 的三重身份、把大小写字母全扫一遍看 TIMEFORMAT 到底认几个、空 TIMEFORMAT 打几行、'
note '# 外部 time 认百分号 M 与 -v、time 撞上不存在的命令、SECONDS 与 EPOCHREALTIME 的粒度、'
note '# 为什么 time 的数字不能上屏（掩码成形状）、三种口径各差多少。'
runc 'bash timing.sh'

# ============================================================================
banner "第 12 节 小证据：耗时不能上屏，但「掩码后的形状」能"
# ============================================================================
note '# 第 11 节把规则说死了：耗时每跑必变。这一节演示怎么把它变成可印的东西 = 掩码 + 两遍自证。'
cat > shapecheck.sh <<'EOF'
mask() { sed -E 's/[0-9][0-9.]*/N/g'; }
bad=0; ok=0
for fmt in '%R %U %S' '%e %P' 'real %e'; do
    TIMEFORMAT="$fmt"
    o=$({ time sleep 0.05; } 2>&1)
    if printf %s "$o" | grep -qE '无效的格式字符|invalid time format'; then
        bad=$((bad + 1))
        printf '  TIMEFORMAT=%s → 没有耗时可掩：bash 当场报错，耗时一个字都不打\n' "$fmt"
    else
        ok=$((ok + 1))
        printf '  TIMEFORMAT=%s → 掩码后=[%s]\n' "$fmt" "$(printf %s "$o" | mask | tr '\n' ' ')"
    fi
done
printf "  三个格式里：打出耗时的=%s 个 ｜ 当场报错的=%s 个（报错那两条的原文行号不掩，因为它不是耗时）\n" "$ok" "$bad"
TIMEFORMAT='%R %U %S'
{ time sleep 0.05; } 2> m1.txt
{ time sleep 0.05; } 2> m2.txt
printf "  同一 TIMEFORMAT 跑两遍，掩码后 diff=%s 行（形状稳不稳就看这个数）\n" \
  "$(diff <(mask < m1.txt) <(mask < m2.txt) | sed -n '/^[<>]/p' | wc -l)"
raw=$(diff m1.txt m2.txt | sed -n '/^[<>]/p' | wc -l)
printf "  没掩码时两遍 diff=%s 行 ｜ 两趟的裸值一字不差吗=%s\n" \
  "$raw" "$( [ "$raw" -eq 0 ] && echo 这一趟恰好撞成一样 || echo 不同 )"
EOF
runc 'bash shapecheck.sh'
note '  → 判据只有上面那个「掩码后 diff=0」：裸值那一行是给对照用的，它非 0 是常态、偶尔撞成 0 也不奇怪'
note '    （这台机器的计时精度粗，两趟量到同一个 0.05 完全可能）—— 所以拿「裸值两遍不同」当断言会忽对忽错，'
note '    正如第 13 节第 9 格对多进程 strace 日志的结论：能跨趟断言的只有归一化后的形状与计数。'
note '    外部 time 的字段里还带路径与内存值，掩码规则要另写一遍 —— 第 11 节第 4、5 格已演。'
rm -f shapecheck.sh m1.txt m2.txt

# ============================================================================
banner "第 13 节 tracewho.sh 完整跑：名字被谁接走，与 strace 看得见真身"
# ============================================================================
note '# 10 格：type -a 的清单顺序就是查找顺序、command／反斜杠／builtin 各压掉哪一层、enable -n 的判别式、'
note '# caller/BASH_SOURCE 只有落成文件才有名字、strace 的 -f 与 -e 与 -yy、隐私与易变值、'
note '# 抹干净之后能不能逐行对拍（单进程能、多进程只能断言计数）、-c 汇总与一行五段。'
runc 'bash tracewho.sh'

# ============================================================================
banner "第 14 节 小证据：用 strace 复核「这个名字到底 exec 了谁」"
# ============================================================================
note '# 第 13 节前三格是问 shell（type / command）；这一节不问 shell，直接看内核收到了什么。'
cat > who.sh <<'EOF'
shopt -s expand_aliases
f() { echo 我是函数; }
alias e='echo 我是alias'
\ls > /dev/null
command ls > /dev/null
e hi
f
EOF
cat > whocheck.sh <<'EOF'
strace -f -e trace=execve -o who.log bash who.sh >/dev/null 2>&1
printf "  这份日志里的 execve 调用行=%s 条\n" "$(grep -c 'execve(' who.log)"
grep 'execve(' who.log | sed -E 's/.*execve\("([^"]*)".*/      \1/' | sort | uniq -c
printf "  日志里出现过 /usr/bin/ls 吗=%s ｜ 出现过 echo 这个程序吗=%s\n" \
  "$(grep -c '/usr/bin/ls' who.log)" "$(grep -c 'execve("/usr/bin/echo"' who.log)"
EOF
runc 'bash whocheck.sh'
note '  ★ 反斜杠那条与 command 那条在日志里合成同一个文件：alias 层与函数层根本不进内核，'
note '    所以「绕过之后掉到哪一层」这个问题，最后一格只能由 strace 回答。'
note '    echo 一次都没 exec —— 它是内建，内建跑得再多次也不会在 execve 里出现。'
cat > off.sh <<'EOF'
enable -n echo
echo 关掉了才看得见
EOF
cat > offcheck.sh <<'EOF'
strace -f -e trace=execve -o off.log bash off.sh >/dev/null 2>&1
printf "  关掉内建 echo 之后：execve 调用行=%s 条 ｜ 其中 /usr/bin/echo=%s 条\n" \
  "$(grep -c 'execve(' off.log)" "$(grep -c '/usr/bin/echo' off.log)"
EOF
runc 'bash offcheck.sh'
note '  → 一句 shell 层面的开关，落到内核就是「多了一次 execve」—— 这就是 enable -n 那格的机器证据。'
rm -f who.sh whocheck.sh who.log off.sh offcheck.sh off.log

# ============================================================================
banner "第 15 节 综合演练：一个坏脚本，按五步配方走完全程"
# ============================================================================
note '# 配方五步，每步只问一个问题：① 语法层有没有错（-n）② 有没有没定义的名字（-u）'
note '# ③ 到底跑了什么、第一个空值在哪（-x + PS4 落盘）④ 出错那行的源码行号（DEBUG + -T）'
note '# ⑤ 这个名字真的 exec 了谁（strace -f -e trace=execve）。下面这个脚本几种毛病各留一处。'
cat > broken.sh <<'EOF'
set -T
trap 'printf "〔D〕%s:%s → %s\n" "$(basename "${BASH_SOURCE[0]}")" "$LINENO" "$BASH_COMMAND" >> "${DBGF:-dbg.txt}"' DEBUG
misspelledcommand 参数
empty=""
printf '结果是 %s\n' "$empty"
bad=有 空格
echo "bad=[$bad]"
echo "没定义的那个=[$undefinedname]"
EOF
note '# 陷阱体把日志文件名交给环境变量 DBGF，于是每一步各写自己那份日志：五步都真跑过脚本的话，'
note '#   它们会全挤在同一个 dbg.txt 里累加，最后一个数谁也说不清是谁的（第一版就栽在这里）。'
cat > step1.sh <<'EOF'
printf "  ① bash -n rc=%s ｜ 报错行数=%s ｜ 原文=[%s]\n" \
  "$(bash -n broken.sh >/dev/null 2>&1; echo $?)" "$(bash -n broken.sh 2>&1 | wc -l)" "$(bash -n broken.sh 2>&1 | head -1)"
EOF
runc 'bash step1.sh'
note '#    语法层全过 —— 这类毛病 -n 一辈子查不出来，所以配方不能只停在第一步。'
cat > step2.sh <<'EOF'
DBGF=dbg2.txt bash -u broken.sh >u_out.txt 2>u_err.txt; rc=$?
printf "  ② bash -u rc=%s ｜ stderr 全文：\n" "$rc"
sed 's/^/      /' u_err.txt
printf "    其中「未绑定/unbound」的行数=%s ｜「未找到命令/command not found」的行数=%s\n" \
  "$(grep -cE '未绑定|unbound' u_err.txt)" "$(grep -cE '未找到命令|command not found' u_err.txt)"
printf "    点到的名字=[%s] ｜ 停在第 %s 行 ｜ 最后一行那个 undefinedname 轮到它了吗=%s\n" \
  "$(grep -E '未绑定|unbound' u_err.txt | sed -E 's/.*: ([^:]+): (未绑定的变量|unbound variable)/\1/')" \
  "$(grep -E '未绑定|unbound' u_err.txt | sed -E 's/^[^:]+: [^0-9]*([0-9]+):.*/\1/')" \
  "$(grep -c undefinedname u_err.txt)"
printf "    这一趟 DEBUG 记了几行=%s（② 也是真跑，它同样在写日志 —— 见 recap 那一屏）\n" "$(wc -l < dbg2.txt)"
EOF
runc 'bash step2.sh'
note '#    -u 只点名第一个未绑定的名字（bad）就收工：脚本停在第 7 行，最后一行的 undefinedname 根本没轮到。'
note '#    所以「-u 抓到几个名字」这种问法是错的 —— 它抓到第一个就把脚本杀死，一份脚本里有几个坏名字它都只报一个。'
note '#    那两条「未找到命令」也不是 -u 报的：那是拼错的命令与少引号那行的后果（第 3 节第 3 格演过它为什么算两条）。'
cat > step34.sh <<'EOF'
rm -f dbg34.txt
PS4='+ ' DBGF=dbg34.txt bash -x broken.sh > stdout.txt 2> trace.txt
rc=$?
printf "  ③ 真跑 rc=%s ｜ trace 行数=%s ｜ stdout 行数=%s\n" "$rc" "$(wc -l < trace.txt)" "$(wc -l < stdout.txt)"
printf "    「找不到命令」那类原文命中行数=%s（中文/英文两种写法都数，判据不看字面）\n" \
  "$(grep -cE '未找到命令|command not found' trace.txt)"
printf "    等号后面为空的赋值行有几条=%s ｜ 第一条在第几行=%s\n" \
  "$(sed -n '/^+ [a-z]*=$/p' trace.txt | wc -l)" "$(grep -n '^+ [a-z]*=$' trace.txt | head -1 | cut -d: -f1)"
printf "    少引号那行被拆成两条的样子=[%s]\n" "$(grep -A1 -m1 '^+ bad=' trace.txt | tr '\n' ' ')"
printf "  ④ DEBUG 陷阱记到的源码行数=%s ｜ 前 2 行：\n" "$(wc -l < dbg34.txt)"
head -2 dbg34.txt | sed "s/^/      /"
printf "    拼错那条命令在源码里的行号=[%s]\n" "$(grep -m1 misspelled dbg34.txt | sed -E 's/.*:([0-9]+) .*/\1/')"
EOF
runc 'bash step34.sh'
note '#    ③ 的读法：先数三份行数（trace 有、stdout 少），再按第 6 节的动作找「第一条不对劲的行」。'
note '#    ④ 给的是 broken.sh 自己的行号 —— 这就是第 7 节要 -T、要「落成文件」的全部理由。'
cat > step5.sh <<'EOF'
rm -f dbg5.txt
DBGF=dbg5.txt strace -f -e trace=execve -o brk.log bash broken.sh >/dev/null 2>&1
printf "  ⑤ 整趟一共 exec 了几个程序=%s 条，按文件并列：\n" "$(grep -c 'execve(' brk.log)"
grep 'execve(' brk.log | sed -E 's/.*execve\("([^"]*)".*/      \1/' | sort | uniq -c
printf "    拼错的那个名字出现在 execve 里吗=%s ｜ 没定义那个变量所在的行呢=%s\n" \
  "$(grep -c misspelled brk.log)" "$(grep -c undefinedname brk.log)"
printf "    同一趟 DEBUG 记到的行数=%s ｜ ③④ 那趟=%s ｜ 一样吗=%s\n" \
  "$(wc -l < dbg5.txt)" "$(wc -l < dbg34.txt)" "$([ "$(wc -l < dbg5.txt)" -eq "$(wc -l < dbg34.txt)" ] && echo 一样 || echo 不一样★)"
EOF
runc 'bash step5.sh'
note '  ★ 三条结论：拼错的名字根本没 execve（bash 自己就判了找不到命令）；被 strace 追着跑，陷阱条数一个字不变，'
note '    但内核层面多出来的账单全是 basename —— 那不是脚本的锅，是**观察者**的锅：陷阱体里的 $(basename ...) '
note '    每响应一条命令就 fork+exec 一次。第 7 节说「陷阱体越短越好」，这里是它在内核层面的价格。'
cat > recap.sh <<'EOF'
printf "  五步走完的清点：① -n 报 %s 条 ｜ ② -u 点名 %s 次（停在第 %s 行）｜ ③ trace 里空赋值 %s 处\n" \
  "$(bash -n broken.sh 2>&1 | wc -l)" "$(grep -cE '未绑定|unbound' u_err.txt)" \
  "$(grep -E '未绑定|unbound' u_err.txt | sed -E 's/^[^:]+: [^0-9]*([0-9]+):.*/\1/')" \
  "$(sed -n '/^+ [a-z]*=$/p' trace.txt | wc -l)"
printf "                    ④ DEBUG 记 %s 行 ｜ ⑤ execve 见 %s 条\n" \
  "$(wc -l < dbg34.txt)" "$(grep -c 'execve(' brk.log)"
printf "  三份 DEBUG 日志各写各的（这一步再跑一趟，日志名交给 DBGF）：②=%s 行 ｜ ③④=%s 行 ｜ ⑤=%s 行 ｜ 文件共 %s 份\n" \
  "$(wc -l < dbg2.txt)" "$(wc -l < dbg34.txt)" "$(wc -l < dbg5.txt)" "$(ls dbg*.txt | wc -l)"
printf "  为什么 ② 比 ③④ 少一行：② 的脚本死在第 7 行，第 8 行那句根本没轮到执行，DEBUG 就少预告一次 —— %s\n" \
  "$([ "$(wc -l < dbg2.txt)" -lt "$(wc -l < dbg34.txt)" ] && echo 少一条，对上了 || echo 没少，读错了★)"
EOF
runc 'bash recap.sh'
rm -f broken.sh step1.sh step2.sh step34.sh step5.sh recap.sh \
      dbg.txt dbg2.txt dbg34.txt dbg5.txt u_out.txt u_err.txt stdout.txt trace.txt brk.log

# ============================================================================
banner "第 16 节 把同一个坏脚本交给 /bin/sh（dash）：哪几件设施直接没了"
# ============================================================================
note '# dash 有 set -x、有 -n、有 trap，也认 PS4 这个变量；直接没了的是 DEBUG 陷阱 / -T / extdebug / BASH_XTRACEFD。'
note '# 这一节只问一件事：本章前面用的那些抓手，换到 dash 还剩几个、哪几个只剩个空壳。'
cat > dsh.sh <<'EOF'
a=1
echo "用了 $a"
EOF
cat > dashcheck.sh <<'EOF'
x=定义时算完
printf "  ① sh -n rc=%s ｜ bash -n rc=%s\n" "$(sh -n dsh.sh >/dev/null 2>&1; echo $?)" "$(bash -n dsh.sh >/dev/null 2>&1; echo $?)"
printf "  ② dash 的 set -x 有没有输出=%s 行\n" "$(sh -x dsh.sh 2>&1 | sed -n '/^+ /p' | wc -l)"
printf '  ③ 给 dash 设 PS4 再 -x，那个前缀出现了几次=%s ｜ 设成〔行号〕之后 trace 首行=[%s]\n' \
  "$(PS4='〔行号〕' sh -x dsh.sh 2>&1 | grep -c '〔行号〕')" \
  "$(PS4='〔行号〕' sh -x dsh.sh 2>&1 | sed -n '1p')"
printf '  ③b PS4 里塞 bash 那三个名字，dash 给得出吗：首行=[%s] ｜ dash 自己顶层读 LINENO=[%s]\n' \
  "$(PS4='<${LINENO}|${FUNCNAME}|${BASH_SOURCE}>' sh -x dsh.sh 2>&1 | sed -n '1p')" \
  "$(sh -c 'printf %s "$LINENO"')"
printf '  ③c 到底是谁在展开 PS4：单引号版前两行=[%s] ｜ 双引号版前两行=[%s]\n' \
  "$(PS4='+[$x] ' sh -x dsh.sh 2>&1 | head -2 | tr '\n' '@')" \
  "$(PS4="+[$x] " sh -x dsh.sh 2>&1 | head -2 | tr '\n' '@')"
printf '  ③d BASH_XTRACEFD=1 拐得走 dash 的 trace 吗：stdout=%s 行（1=只有正菜）｜ stderr=%s 行（2=trace 还留在 2 上）\n' \
  "$(PS4='X' BASH_XTRACEFD=1 sh -x dsh.sh 2>/dev/null | wc -l)" \
  "$(PS4='X' BASH_XTRACEFD=1 sh -x dsh.sh 2>&1 >/dev/null | wc -l)"
printf "  ④ dash 对 trap DEBUG 的回答=[%s]\n" "$(sh -c 'trap : DEBUG' 2>&1 | head -1)"
printf "  ④ dash 对 set -T 的回答=[%s]\n" "$(sh -c 'set -T' 2>&1 | head -1)"
printf "  ⑤ dash 里 time 是什么=[%s]\n" "$(sh -c 'type time' 2>&1 | head -1)"
printf "    dash 跑 time : →[%s]\n" "$(sh -c 'time :' 2>&1 | head -1)"
printf "    dash 跑 time /bin/true → %s 行，首行掩码后=[%s]\n" \
  "$(sh -c 'time /bin/true' 2>&1 | wc -l)" "$(sh -c 'time /bin/true' 2>&1 | head -1 | sed -E 's/[0-9]+/N/g')"
printf "    同一句 time : 交给 bash → %s 行（数字照样掩码，不上屏）\n" "$(bash -c 'time :' 2>&1 | wc -l)"
EOF
runc 'bash dashcheck.sh'
note '  → 剩下的是 -x、-n 与外部 time；trap DEBUG / -T / extdebug / BASH_XTRACEFD 直接没了；PS4 属于「还剩个空壳」那一类。'
note '    ③ 那两行别按老常识读反：本机这版 dash **认 PS4** —— 设成〔行号〕之后 trace 首行就以它开头，'
note '    所以第 5 节「给每行前缀换个东西」这件事在 dash 里也使得出。露底的是 ③b：把 bash 那三个名字塞进去，'
note '    ${LINENO} ${FUNCNAME} ${BASH_SOURCE} 在 dash 里全是空串（③b 首行就是一串空括号，右边那格是 dash 自己顶层读的 LINENO，也是空）'
note '    —— 本章真正靠得住的「行号前缀」配方，换到 dash 组不出来，不是变量不被认，而是没东西可填。'
note '    ③c 说明是谁在展开：单引号那版打出来是空括号（dash 侧没有 x 这个名字，它当场展开成空），'
note '    双引号那版打出来是 bash 早就算完的那个串 —— 和第 5 节第 1 格在 bash 里演的是一对儿的形状。'
note '    ③d 更直接：BASH_XTRACEFD 对 dash 一点作用都没有，trace 仍旧只在 stderr，stdout 只留下正菜那一行。'
note '    ⑤ 这一串最容易被读反：dash 里 time **不是关键字**，于是 `time :` 被当成「找一个叫 time 的程序」，'
note '    它找到了 /usr/bin/time，可外部 time 只会 exec 文件 —— : 是 shell 的内建语法，没有这个文件，所以报 cannot run。'
note '    把被量的东西换成一个真文件（/bin/true）它就打得出来，用的是外部 time 的默认格式，不是 TIMEFORMAT 那套。'
note '    同一句在 bash 里走的是关键字那条路，正常出四行 —— 这就是「同一个词在两个 shell 里根本不是同一个东西」。'
note '    所以本章所有配方都要求「用 bash 跑」，脚本第一行那句 #!/usr/bin/env bash 不是装饰 ——'
note '    第 10 节的「入口不同报错不同」到这里升级成「shell 不同设施不同」，两件事是同一个根。'
rm -f dsh.sh dashcheck.sh

# ============================================================================
banner "第 17 节 对账：本章到底跑了多少格"
# ============================================================================
note '# 这一节不是「报个数」，是把两个互相独立的口径摆在一起：数标题这一行的条数，和标题里最后写到的那个编号。'
note '# 只数条数会被「删了一格但编号照抄」骗过去 —— 所以先拿一份删掉一行标题的副本，让守卫当场响一次。'
cat > countcheck.sh <<'EOF'
six="xtrace.sh ps4fd.sh debugtrap.sh staticcheck.sh timing.sh tracewho.sh"
printf "  每支用两个互相独立的口径数：标题条数 vs 最后一个标题里写的编号，两个数必须相等\n"
tot=0; bad=0
for f in $six; do
    n=$(grep -c '^echo "=== ' "$f")
    m=$(grep -o '^echo "=== [0-9]\+' "$f" | tail -1 | grep -o '[0-9]\+$')
    if [ "$n" = "$m" ]; then v=相等; else v=不相等★; bad=$((bad + 1)); fi
    printf "    %-15s 标题条数=%-3s 末尾编号=%-3s %s\n" "$f" "$n" "$m" "$v"
    tot=$((tot + n))
done
printf "  六支合计=%s 格 ｜ 两个口径对不上的有几支=%s（应当 0）\n" "$tot" "$bad"
printf "  ★ 先证明这条守卫响得起来：拿 xtrace.sh 的副本删掉第 5 格那一行标题，再拿同样的判据量一次\n"
sed '/^echo "=== 5\./d' xtrace.sh > cc_probe.sh
n=$(grep -c '^echo "=== ' cc_probe.sh)
m=$(grep -o '^echo "=== [0-9]\+' cc_probe.sh | tail -1 | grep -o '[0-9]\+$')
printf "    删过的那份：标题条数=%s ｜ 末尾编号=%s ｜ 结论=%s\n" "$n" "$m" \
    "$([ "$n" = "$m" ] && echo 还是相等（那这条判据是摆设★白设了） || echo 响了的（少一格当场被抓住）)"
rm -f cc_probe.sh
EOF
runc 'bash countcheck.sh'
note '# 逐支点名，README 的分格数就是这几行来的：'
for s in xtrace.sh ps4fd.sh debugtrap.sh staticcheck.sh timing.sh tracewho.sh; do
    printf '  %-16s %s 格\n' "$s" "$(grep -c '^echo .=== ' "$s")"
done
cat > demosef.sh <<'EOF'
printf "  六支合计=%s 格 ｜ demo 自己 %s 节\n" \
  "$(grep -h "^echo .=== " xtrace.sh ps4fd.sh debugtrap.sh staticcheck.sh timing.sh tracewho.sh | wc -l)" \
  "$(grep -c '^banner ' demo.sh)"
EOF
runc 'bash demosef.sh'
note '  ★ demo 的节数靠数 `^banner ` 得来，第 1 节到第 18 节一条不漏；这个数如果对不上 18，'
note '    就是有人在中间加了节没更新顶上那份路线清单 —— 这条断言存在的意义就是让它当场暴露。'
rm -f countcheck.sh demosef.sh

# ============================================================================
banner "第 18 节 自证：这一章没动过你的东西"
# ============================================================================
cat > selfcheck.sh <<'EOF'
printf "  ① 章目录（demo 源码所在处）开跑前后条目一致吗=%s\n" \
  "$( [ -n "$BEFORE_LIST" ] && [ "$(ls -A "$HERE" | sort | tr '\n' '|')" = "$BEFORE_LIST" ] && echo 一致 || echo 不一致★ )"
case "$BEFORE_LIST" in
    *demo.sh*) has=在 ;;
    *) has=不在★（清单是空的，那条「一致」就是两个空串相等，纯摆设） ;;
esac
nowsum=$(cd "$HERE" && wc -c *.sh | tail -1 | awk '{print $1}')
printf "     这条判据自己得先不是摆设：清单里有 demo.sh 吗=%s ｜ 条目数=%s\n" \
  "$has" "$(printf %s "$BEFORE_LIST" | tr '|' '\n' | grep -c .)"
printf "     七支脚本的字节总和开跑前后一致吗=%s（前 %s ｜ 后 %s）—— 演示过程中没有一个字节被改回源码目录\n" \
  "$([ -n "$BEFORE_SUM" ] && [ "$BEFORE_SUM" = "$nowsum" ] && echo 一致 || echo 不一致★)" "$BEFORE_SUM" "$nowsum"
printf "  ② 沙箱 work/ 里除七支脚本外还剩：[%s]\n" \
  "$(ls -A | grep -vE '^(demo|xtrace|ps4fd|debugtrap|staticcheck|timing|tracewho)\.sh$' | tr '\n' ' ')"
printf "     本节两支 fixture 正在跑（selfcheck/promish），它俩之后就被 rm；除此之外还剩几个=%s（应当 0）\n" \
  "$(ls -A | grep -vE '^(demo|xtrace|ps4fd|debugtrap|staticcheck|timing|tracewho)\.sh$' | grep -vcE '^(selfcheck|promish)\.sh$')"
printf "  ③ 六个取证目录还在吗=%s（应当 0，六支脚本各自 trap 删过）\n" "$(ls -d xt p4 d9 sc tm tw 2>/dev/null | wc -l)"
EOF
runc 'bash selfcheck.sh'
note '# 本章承诺：不联网、不装包、不开端口、不 -p 挂别人的进程、不往任何配置文件或 /etc 里写、'
note '#   不留「发出去不管」的后台。每一条都配一个计数器，而且每个计数器都先被喂一条**真违规**的样本，'
note '#   先证明它响得起来再报真实值 —— 一个从来不出数的计数器，报 0 不算证据（第 08 章那条恒真守卫的教训）。'
cat > promish.sh <<'EOF'
# 扫描对象只有本章交付的七支脚本本身，写成显式清单而不是 *.sh：
# 本节自己的两支 fixture（selfcheck/promish）和喂出来的样本 _probe.txt 都不该混进真实值里 ——
# 上一版就是让样本留下了一个 _p.sh，被下一条判据的 *.sh 一起扫了进去，报出「3 处带 -p」。
LIST='demo.sh xtrace.sh ps4fd.sh debugtrap.sh staticcheck.sh timing.sh tracewho.sh'
# 模式串写成 cu[r]l、ap[t] 这种带括号的样子，是刻意让判据匹配不到定义它自己的那一行（同上一条道理）。
NET='/^[[:space:]]*#/{next}
/(^|[;&|( ])(cu[r]l|wge[t]|ap[t]|ap[t]-get|pi[p]|dn[f]|yu[m]|n[c]|ss[h]|sc[p])([[:space:]]|$)/{c++; print FILENAME ":" FNR}
END{print c+0}'
# 只认「-p 后面真跟了一个 pid」的形状（数字或 $ 变量）：解说词里那句「关于 -f 与 -p 的原文」不算挂进程。
PP='/^[[:space:]]*#/{next}
/strace[[:space:]]+-/{t++; if (/[[:space:]]-p[[:space:]]*[0-9$]/ || /--attach=[0-9$]/) {c++; print FILENAME ":" FNR}}
END{print c+0 "/" t+0}'
WR='/^[[:space:]]*#/{next}
/>>?[[:space:]]*(\/etc\/|\/home\/|~\/\.|\$HOME\/\.)/{c++; print FILENAME ":" FNR}
END{print c+0}'
BG='/^[[:space:]]*#/{next}
/&[[:space:]]*wait/{w++; print "收走:" FILENAME ":" FNR}
/&[[:space:]]*$/{d++; print "甩掉:" FILENAME ":" FNR}
END{print w+0 "/" d+0}'
# 样本内容用 \x2d\x70 这种转义写：拼出来是真违规（-p 1234），但这一行源码里没有 -p 这个 token，
# 判据扫自己的时候就匹配不到它 —— 上一版正是栽在把样本原文直接写在脚本里。
feed() { printf '%b' "$1" > _probe.txt; }
real() { awk "$1" $LIST | tail -1; }
hits() { awk "$1" $LIST | sed '$d' | tr '\n' ' '; }
samp() { feed "$2"; awk "$1" _probe.txt | tail -1; rm -f _probe.txt; }
printf '  ① 命令位置上的联网/装包=%s 处 ｜ 喂一条真违规（取一个网址）→ %s 处\n' \
  "$(real "$NET")" "$(samp "$NET" 'curl -s http://x.invalid/a\n')"
printf     '  ② 带 pid 的 strace -p / strace 调用行数=%s ｜ 喂一条挂别人的进程 → %s\n' \
  "$(real "$PP")" "$(samp "$PP" 'strace \x2d\x70 1234 ls\n')"
printf     '  ③ 往 /etc 或点文件重定向的行数=%s ｜ 喂一条写进 ~/.bashrc → %s\n' \
  "$(real "$WR")" "$(samp "$WR" 'echo x >> \x7e\x2f.bashrc\n')"
printf     '  ④ 后台调用（& 紧跟 wait）/ 发出去不管的（行尾就是 &）=%s ｜ 喂一条 & 收尾 → %s\n' \
  "$(real "$BG")" "$(samp "$BG" 'sleep 1 \x26\n')"
printf     '  ② 的分子必须是 0 —— 上面这几处命中点名的就是判据认为可疑的行（空=一处都没有）：[%s]\n' \
  "$(hits "$PP")"
printf     '  ④ 的分子逐处点名（哪个文件哪一行用了 &）：[%s]\n' "$(hits "$BG")"
printf     '  ③ 的分子逐处点名：[%s]\n' "$(hits "$WR")"
printf '  ⑤ 扫到的 sleep 时长种类（字样，不是执行次数）：[%s]\n' \
  "$(grep -hoE 'sleep [0-9.]+' $LIST | sort -u | tr '\n' ' ' | sed -E 's/^[[:space:]]+| +$//g')"
rm -f _probe.txt
EOF
runc 'bash promish.sh'
note '  → 每条都是「先喂一条真违规、看见它变 1，再看真实值」的两步：喂出去能响、收回来是 0，这个 0 才算证据。'
note '    ①③ 的真实值都是 0；② 的分子必须是 0，分母（strace 调用行数）非 0，'
note '    它证明本章真的跑了 strace，只是每一条都是「自己起、自己等」的那一种。'
note '  ④ 的分子是「& 后面紧跟 wait」这种形状出现了几次，屏上把每一处的 文件:行号 都点了名 ——'
note '    其中一处是被演示的那句 `echo 后台 & wait`（它活在一个被演示的子进程里，同一行就被 wait 收掉），'
note '    另一处是本节解说词里写过这个形状的那一行：这就是为什么光给计数不够，必须把命中点印上屏，'
note '    否则你会把「判据自己」也算成违规，或者反过来把真违规读成解说。分母 0 才是关键：全章没有一处 & 是发出去不管的。'
note '  ⑤ 里最长的一次是 1 秒（第 11 节第 6 格，专门用来演「SECONDS 只有整秒，拿它量 50 毫秒会得到 0」）。'
note '  六支脚本的写入全部落在 xt p4 d9 sc tm tw 六个目录里、由各自 trap 整目录删除；'
note '    demo 的写入全部落在 <沙箱> 那个 mktemp 目录里、由 cleanup 删除 —— 你的家目录一个字节都没被动过。'
rm -f selfcheck.sh promish.sh
printf '\n[本章证据跑完] 六支取证脚本 + 18 节现场，全部是这一条 bash demo.sh 在这台机器上的真实输出。\n'
