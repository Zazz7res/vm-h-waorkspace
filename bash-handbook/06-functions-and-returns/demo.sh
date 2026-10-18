#!/usr/bin/env bash
# ============================================================================
# 第 06 章《函数与返回：退出码、作用域、可见性》—— 整章证据一键复现
#
#   bash demo.sh            跑完自动删沙箱（默认）
#   bash demo.sh --keep     跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt  重定向时自动关掉颜色，README 引用的就是这份纯文本
#
# 屏幕上出现的每一行，都是这条命令在这台机器上的真实结果 —— 没有一行是手写的。
#
# 这一章要还的三笔账（九个取证脚本 + 下面 24 节全部挂在其中一条上）：
#   主线① **函数没有返回值，只有退出码**：return 只能给 0~255，它和 $? 是同一条线；
#          想让函数「给出数据」只能走 stdout，而命令替换会把尾部换行全吃光、把多值挤成一行。
#   主线② **函数体里的名字是动态作用域**：local 的范围由调用栈决定，不由写在哪个文件决定；
#          漏写一个 local，改的就是调用者的变量 —— 递归里漏写，回来的时候值已经被内层换掉了。
#   主线③ **函数是 shell 里的对象，不是磁盘上的文件**：type / declare -f / export -f 决定
#          另一个进程看不看得见它；bash -c 里看不见没导出的函数，别名和函数不是一回事。
#
# 本章的取证路线（24 节，全部真跑）：
#   第 1 节    钉死环境：bash 版本、语言环境、身份五类、FUNCNEST/栈、/bin/sh 真身
#   第 2 节    一张地图：三条主线各一条最小证据
#   第 3 节    retcode.sh 完整跑（return 能写什么、低 8 位、不写 return 时谁的码）
#   第 4 节    小证据：退出码只保留低 8 位 —— 当场把 0/255/256/257/512/-1 排一遍
#   第 5 节    outcapture.sh 完整跑（命令替换的四刀）
#   第 6 节    小证据：尾部换行是「全吃光」，函数只多打一个空行返回值就成了空串
#   第 7 节    errmix.sh 完整跑（stdout / stderr / 退出码 三条通道怎么分）
#   第 8 节    小证据：同一次调用，码走一条道、文本走另一条道，互不相干
#   第 9 节    trapreturn.sh 完整跑（return 与 exit 在 trap、在 source 里的四种下场）
#   第 10 节   小证据：本章为什么不写 set -e —— 它会当场把对照组掐掉
#   第 11 节   localscope.sh 完整跑（遮蔽、恢复、动态作用域、local -n 两个坑）
#   第 12 节   小证据：bash 没有块作用域：if/for 里的 local 出了块照样活着
#   第 13 节   recursion.sh 完整跑（漏 local 的递归、FUNCNEST、爆栈崩的是谁）
#   第 14 节   小证据：定义顺序不重要、重定义立刻生效 —— bash 里没有「先声明后使用」
#   第 15 节   functype.sh 完整跑（三种声明写法、type -t/-a、同名遮蔽怎么赢）
#   第 16 节   小证据：被函数遮蔽之后怎么夺回来：command / builtin / 绝对路径
#   第 17 节   exportfun.sh 完整跑（export -f、env 里的键名形状、别名不跟着走）
#   第 18 节   小证据：负对照 —— 不 source 不导出，孙子进程就是看不见
#   第 19 节   argpass.sh 完整跑（$# / "$*" / "$*" 与 IFS / set -- / 三种传参写法）
#   第 20 节   小证据：转发参数只有一个姿势："$@"，空串也占一格
#   第 21 节   综合演练：一个能上线的小函数库（lib/fnlib.sh，四条规矩全来自前面）
#   第 22 节   把这个库交给另一个进程：source / export -f / declare -f 三条路 + dash 的原文
#   第 23 节   对账：九个脚本各几格、合计几格、demo 自己几节（每道守卫先故意弄响一次）
#   第 24 节   自证：章目录开跑前后一致、沙箱里只剩该有的东西、本章没写过联网与装包
#
# 安全说明：
#   * 所有写操作只发生在 mktemp -d 的沙箱里（前缀 bash06-），由 trap cleanup EXIT 整体删掉；
#     本章九个辅助脚本各自只在自己的子目录里造文件，跑完自己删并当场自证。
#   * 绝不修改 ~/.bashrc、~/.profile、~/.config、~/.local、/etc：要看「另一个干净 shell」
#     的行为一律用 bash -c 起新进程，开关（shopt / set -o）只在那个子进程里活着。
#   * 本章不起后台进程、不 sleep、不联网、不装包、不开任何监听端口；也不 kill 任何进程。
#   * 屏幕上不出现真实身份：pid、家目录绝对路径、毫秒耗时、每次不同的沙箱随机名都不上屏。
#     第 24 节的沙箱清单只印条目名（相对路径），不印所在目录。
#   * 耗时/完成顺序/内存占用一律不上屏：它们每跑必变，README 印了就永远对不上。
#     本章能给的确定性是「计数、长度、排序、布尔、退出码」这五种。
#
# 为什么这个脚本**故意不加** set -e：
#   本章一半的内容是失败现场：return 256 的回绕、bash -c 里看不见函数的原文错、
#   FUNCNEST 拦下那次调用、dash 不认 export -f、故意漏 local 的递归算成 1。
#   加了 set -e，第 2 节第一条就把 demo 掐了。同理不加 set -u（演示里大量故意留空的变量），
#   也不全局开 pipefail —— 管道退出码归第 04 章，本章只在第 10 节于单条命令里临时开 set -e。
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
# 而且 wc -m 的计数口径、declare -p 给非 ASCII 值加不加 $'' 转义都跟着 locale 走。
# 改动只发生在本脚本和它 fork 出的子进程里。
ORIG_LANG="${LANG:-（未设）}"
ORIG_LC_ALL="${LC_ALL:-（未设）}"
export LC_ALL=C.UTF-8 LANG=C.UTF-8 LANGUAGE=C
export ORIG_LANG ORIG_LC_ALL

# 输出被重定向到文件时（README 捕获就是这样跑的）不要 ANSI 颜色码：纯文本才能逐行核对。
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/bash06-XXXXXX")

# 章目录的开跑前快照（只取条目名，第 24 节再取一次比对）。
BEFORE_LIST=$(cd "$HERE" && ls -A | sort | tr '\n' '|')

banner() { printf '\n══════ %s %s\n' "$1" "════════════════════════════════"; }
note() { printf '%s\n' "$1"; }

# 打印命令 → 用 bash -c 真跑 → 打印退出码。用 bash -c 是为了每一条都在一个
# **全新的非交互 bash** 里开始：上一节开的 set -e / 定义的遮蔽函数不会串到下一节。
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

# 本章的九个辅助脚本 + 本 demo 自己复制进沙箱，所有加工都在副本上进行（脚本里写的都是相对路径）。
# ★ 必须用相对文件名跑（bash xxx.sh，不是 bash /绝对路径/xxx.sh）：bash 的报错原文会带上 $0，
#   跑绝对路径就把家目录绝对路径印上屏了（实测：相对跑法 0 处，绝对跑法 16 处）。
mkdir -p "$SB/work/lib"
cp "$HERE"/*.sh "$SB/work/"
cd "$SB/work" || exit 1
umask 022

# ============================================================================
banner "第 1 节 先把这台机器的发言权确认一遍"
# ============================================================================
note '# 本章每条结论都和 bash 版本、语言环境有关（报错原文、declare -p 的转义形状、'
note '#   以及 export -f / declare -f / FUNCNEST 到底存不存在），所以第一步把它们钉死。'
runc 'bash --version | head -1'
runc 'echo "demo 已把自己改成：LC_ALL=[$LC_ALL] LANG=[$LANG]（你原来的终端是 LANG=$ORIG_LANG，LC_ALL=$ORIG_LC_ALL）"'
note '# 为什么非钉不可：同一份脚本在两种语言环境下跑出的字符数不一样（中文按 UTF-8 数与按字节数）。'
runc 'f(){ echo 一二三; }; v=$(f); printf "  同一个函数返回值，本机数出来的字符数=%s（本章只报这一档）\n" "${#v}"'
note '# 一个名字的五种身份，type -t 给分类（本章主线③的地基：函数只是其中一格）：'
runc 'for c in echo printf local declare set jobs type true not-a-name grep sed awk; do printf "  %-9s type -t → %s\n" "$c" "$(type -t "$c" 2>/dev/null || echo 查无此名)"; done'
note '# 本章会碰的三个「关」，此刻的位置（off=关）；第 9 节的 RETURN/DEBUG 全靠它们：'
runc 'for o in errexit pipefunctrace functrace xtrace verbose; do printf "  %-12s %s\n" "$o" "$(set -o 2>/dev/null | awk "\$1==\"$o\"{print \$2}")"; done'
runc 'printf "  FUNCNEST=[%s]（未设=只受栈限制，第 13 节会把它设成 10 看拦截）\n" "${FUNCNEST-未设}"'
runc 'printf "  ulimit -s（栈 KB）=%s ｜ 它是本机固定参数，不是每跑必变的量\n" "$(ulimit -s)"'
runc 'printf "  /bin/sh 的真身=%s\n" "$(readlink -f /bin/sh)"; echo "  ★ 第 22 节要让同一个函数库去撞一下它"'
runc 'command -v shellcheck >/dev/null 2>&1 && echo "shellcheck 在" || echo "shellcheck 本机没有 —— 本章不装东西，脚本只能靠逐条真跑来验"'

# ============================================================================
banner "第 2 节 一张地图：三条主线各一条最小证据"
# ============================================================================
note '# 先把三笔账各亮一次，后面 22 节都在把它们放大到「为什么会这样」。'
note '# 主线①：函数能给的只有一个小整数。'
runc 'f(){ return 42; }; f; code=$?; printf "  只写 return 的函数，外面拿到的是数字 %s（它和 \$? 是同一条线）\n" "$code"'
note '# 主线②：函数体里的赋值，默认改的是外面那个变量。'
runc 'x=外面; f(){ x=里面; }; f; printf "  调用一次之后，外面的 x=%s（函数没有把变量藏起来）\n" "$x"'
note '# 主线③：函数活在当前这个 shell 的表里，不在磁盘上，所以新进程看不见。'
runc 'f(){ echo 在里面; }; bash -c "f"; echo "  ★ 上面那句是 bash 自己的原话，同一个名字换个进程就不认识了"'

# ============================================================================
banner "第 3 节 retcode.sh 完整跑：return 后面能写什么"
# ============================================================================
note '# 七格：五个写法、低 8 位回绕、return 的三个边角、不写 return 时谁的码、exit 与 return 的分工、'
note '#   以及「调用完之后退出码还保得住吗」的四种写法。'
runc 'bash retcode.sh'

# ============================================================================
banner "第 4 节 小证据：退出码只保留低 8 位（把整条数轴排一遍）"
# ============================================================================
note '# 第 3 节里那格用的是 256/258 两个数，这里把它们连成一排，让「回绕」的形状自己浮出来。'
runc 'f(){ return "$1"; }
for v in 0 1 7 42 255 256 257 511 512 -1 -2; do
  f "$v"; code=$?
  printf "  return %-4s → 外面拿到 %s\n" "$v" "$code"
done
echo "  ★ 256 与 0 同、257 与 1 同、512 与 0 同：低 8 位之外的全被丢掉；负数按补码回绕"'
runc 'f(){ return 256; }; g(){ return 0; }; f; a=$?; g; b=$?; printf "  于是 return 256 和 return 0 分得开吗 → a=%s b=%s，%s\n" "$a" "$b" "$( [ "$a" = "$b" ] && echo 分不开 || echo 能分开 )"'
note '# ★ 这条线要带走的判据：想让函数报告「成功/失败」，只能在一个字节的范围内编码；'
note '#   想让函数报告「多少/是什么」，那就不该用 return（第 5 节整节在讲另一条道）。'

# ============================================================================
banner "第 5 节 outcapture.sh 完整跑：命令替换的四刀"
# ============================================================================
note '# 六格：尾部换行、多值的两种编码、诊断污染、子 shell 副作用、码与文本互不相干、收口自证。'
runc 'bash outcapture.sh'

# ============================================================================
banner "第 6 节 小证据：尾部换行是「全吃光」，多一个空行返回值就成空串"
# ============================================================================
runc 'a(){ printf "x\n"; }
b(){ printf "x\n\n"; }
c(){ printf "\n"; }
d(){ printf "x\n\n\n"; }
for f in a b c d; do v=$($f); printf "  %s：捕获后长度=%s 内容=[%s]\n" "$f" "${#v}" "$v"; done
echo "  ★ 一个尾换行和三个尾换行，捕获结果一模一样；只打一个空行的函数，返回值是空串"'
runc 'ok(){ printf "done\n\n"; }
v=$(ok)
printf "  函数打的是 done + 一个空行：捕获到 [%s] 长度=%s（被吃掉的只有换行，done 还在）\n" "$v" "${#v}"
empty(){ printf "\n"; }
w=$(empty)
printf "  函数只打一个空行：捕获到 [%s] 长度=%s → 判空结果=%s\n" "$w" "${#w}" "$([ -z "$w" ] && echo 是空串 || echo 非空)"'
note '# ★ 两种「看着像空」要分开：done 加一个空行，捕获后还是 4 个字符；只打空行才是真的空串。'
note '#   前者是尾部换行被吃光，后者是函数从头到尾没往 1 号 fd 写过任何非换行字节。'

# ============================================================================
banner "第 7 节 errmix.sh 完整跑：三条通道怎么分"
# ============================================================================
note '# 七格：out=$(f) 到底兜住了什么、三条分流的写法、诊断误写 stdout 导致的假通过现场、'
note '#   赋值能不能带回码、if f 的码是谁的、命令替换的结构性代价、收口判据。'
runc 'bash errmix.sh'

# ============================================================================
banner "第 8 节 小证据：同一次调用，码走一条道、文本走另一条道"
# ============================================================================
runc 'f(){ echo 给捕获的; echo 给屏幕的 >&2; return 3; }
v=$(f); code=$?
printf "  捕获到的 v=[%s] ｜ 退出码=%s\n" "$v" "$code"
echo "  ★ 屏幕上那句「给屏幕的」没进 v：命令替换只接管 1 号 fd，2 号它接不着"'
runc 'f(){ echo 给捕获的; echo 给屏幕的 >&2; return 3; }
if f; then echo "  走进 then（说明码是 0）"; else echo "  走进 else（说明码非 0，而且和 stdout 内容无关）"; fi'
runc 'f(){ return 0; }; v=$(f); printf "  函数一个字都不输出、只 return 0：v=[%s] 长度=%s 码=%s\n" "$v" "${#v}" "$?"
f2(){ echo 有内容; return 1; }; w=$(f2); printf "  函数有内容但 return 1：w=[%s] 码非 0 → %s\n" "$w" "$( [ "$?" -ne 0 ] && echo 是 || echo 否 )"'
note '# 这一格是本章的分水岭：**判成败看码，拿数据看文本，两套并行且互不解释**。'
note '# 「函数返回了什么」这句话本身是错的问法 —— 它要么返回一个码，要么输出一些字节。'

# ============================================================================
banner "第 9 节 trapreturn.sh 完整跑：return 与 exit 在 trap、在 source 里"
# ============================================================================
note '# 七格：函数外 return 的原文、EXIT 处理器里三种写法、DEBUG trap 计几次、'
note '#   RETURN trap 在 functrace 开/关下的真实触发次数、裸 return 拿谁的码、被 source 的文件里 return 5 与 exit 5。'
runc 'bash trapreturn.sh'

# ============================================================================
banner "第 10 节 小证据：本章为什么不写 set -e"
# ============================================================================
note '# 九个辅助脚本的头部都写着「故意不加 set -e」，这里当场演一次它会把什么掐掉。'
runc 'set -e
f(){ false; }
g(){ f; }
g
echo "  这行印不出来：第 2 条命令一失败，整个脚本当场结束"'
runc 'set -e
f(){ false; }
g(){ f || true; }
g
echo "  加了 || true 就走到这里了 —— 但你的失败现场已经被自己咽掉了"'
runc 'set -e
f(){ false; }
f || echo "  在 || 左边时 set -e 根本不管制（这是它最有名的例外）"; echo "  所以 set -e 既不可靠、又会误伤"'
runc 'bash -c "set -e; f(){ return 7; }; f; echo 到不了"; printf "  子进程整体退出码=%s（demo 自己不受影响，因为每条 runc 都是新进程）\n" "$?"'
note '# ★ 结论：本章的失败要靠**显式判码**（if f / code=$? / || ）来表达，不靠 set -e 兜。'

# ============================================================================
banner "第 11 节 localscope.sh 完整跑：动态作用域"
# ============================================================================
note '# 八格：local 的基本面、遮蔽与恢复、★ 被调用者读得到并改得动调用者的 local、'
note '#   被调用者自己写一句 local 就断链、函数外写 local 的原文、local -n 的正用与两个坑、收口数全局。'
runc 'bash localscope.sh'

# ============================================================================
banner "第 12 节 小证据：bash 没有块作用域，local 的范围是整个函数"
# ============================================================================
runc 'f(){ if true; then local t=1; fi
printf "  出了 if 的大括号，t 还在吗 → t=[%s]\n" "$t"
for i in 1 2 3; do local u=$i; done
printf "  出了 for 的大括号，u 还在吗 → u=[%s]\n" "$u"
printf "  循环变量 i 我没写 local，它现在=[%s]\n" "$i"; }
f
printf "  函数返回之后，外面的 t=[%s] i=[%s]（★ i 是全局的，它一路活到了函数外面）\n" "${t-未定义}" "${i-未定义}"'
runc 'local q=1
printf "  报完错这行照样印出来（q=[%s]）：非 0 不等于终止，第 10 节讲的正是它的另一面\n" "${q-没赋上值}"'
note '# ★ 这一格把第 05 章那条「bash 没有块作用域」在函数里再钉一遍：'
note '#   local 划的圈是「这次函数调用」，不是那对花括号 —— 递归每层一个圈（第 13 节）。'

# ============================================================================
banner "第 13 节 recursion.sh 完整跑：递归靠什么成立、代价怎么量"
# ============================================================================
note '# 七格：漏 local 的阶乘把 4! 算成 1、FUNCNEST 设 10 当场拦下、不设就撞真栈（崩的是子进程）、'
note '#   fib 的函数进入次数用计数不用耗时、三层同名 x 各是什么、递归 + 命令替换把每层 echo 全拼进来。'
runc 'bash recursion.sh'

# ============================================================================
banner "第 14 节 小证据：定义顺序不重要，重定义立刻生效"
# ============================================================================
runc 'first(){ second; }
second(){ echo "  first 定义时 second 还不存在，照样能调 —— 名字是**调用那一刻**才查的"; }
first'
runc 'g(){ echo "  第一版"; }
g
g(){ echo "  第二版（同名重定义，bash 不报错，直接盖掉）"; }
g'
runc 'notyet; echo "  这行印不出来"'
note '# 对照第 05 章 python 手册那条「def 只跑一次」：bash 里函数不是对象快照，'
note '#   而是名字表里的一条**可随时替换的条目**；跑的时刻查表，所以顺序无所谓、覆盖也无通知。'

# ============================================================================
banner "第 15 节 functype.sh 完整跑：一个名字有几种身份"
# ============================================================================
note '# 七格：三种声明写法进表后一模一样、type -t/-a 的分类与全部来源、PATH 前置的同名遮蔽、'
note '#   把同名函数定义上 PATH 就追不上、内建也压不过函数、同一份定义在 /bin/sh 里撞的原文、收口自证。'
runc 'bash functype.sh'

# ============================================================================
banner "第 16 节 小证据：被函数遮蔽之后怎么夺回来"
# ============================================================================
runc 'ls(){ echo "  【函数版】我把真的 ls 挡掉了"; }
ls /nonexistent-x
printf "  → 屏幕上没有 ls 的结果，函数吃掉了参数（退出码 %s）\n" "$?"'
runc 'ls(){ echo "  【函数版】我把真的 ls 挡掉了"; }
command ls /nonexistent-x
printf "  → command 一挡在前面就绕过函数，拿到真 ls 的原文与码（退出码 %s）\n" "$?"'
runc 'true(){ echo "  我的 true 抢在内建前面"; }
true
builtin true
printf "  → builtin 强制走内建，退出码=%s\n" "$?"'
runc 'true(){ echo "  我的 true"; }
/usr/bin/env true
printf "  → 第三条路：写出绝对路径/走 env，查的是 PATH 而不是名字表，退出码=%s\n" "$?"'
note '# ★ 这三条绕法就是「函数遮蔽」事故的急救包：command（绕过函数与别名）、'
note '#   builtin（只走内建）、绝对路径（绕名字表也绕 PATH）。第 21 节的库里用的是 command。'

# ============================================================================
banner "第 17 节 exportfun.sh 完整跑：函数能不能跨进程"
# ============================================================================
note '# 八格：同进程看得见新进程看不见、export -f 前后的对照、env 里的键名形状（用判定）、'
note '#   别名不跟着 export -f 走、( ) 与 { } 与 bash -c 三者谁能看见、'
note '#   导出的是「那一刻的文本」（已启动的孩子冻旧版，之后新起的孩子拿新版）、真实代价、收口。'
runc 'bash exportfun.sh'

# ============================================================================
banner "第 18 节 小证据：负对照 —— 什么都不做时，孙子进程就是看不见"
# ============================================================================
runc 'have(){ command -v "$1" >/dev/null 2>&1; }
bash -c "have bash"; printf "  没导出时，孙子进程里调用它的退出码=%s（上面是原文错）\n" "$?"'
runc 'have(){ command -v "$1" >/dev/null 2>&1; }
export -f have
bash -c "have bash"; printf "  export -f 之后，同一条命令的退出码=%s（0 = 找到了，函数活着）\n" "$?"'
runc 'have(){ command -v "$1" >/dev/null 2>&1; }
printf "  不导出时 env 里的同名条目数=%s（★ 恒 0 的守卫等于没守卫，下一条专门把它弄响）\n" "$(env | grep -c "^BASH_FUNC_have")"'
runc 'have(){ command -v "$1" >/dev/null 2>&1; }; export -f have
printf "  导出之后同名条目数=%s（同一道守卫，非 0 —— 说明它真的能响）\n" "$(env | grep -c "^BASH_FUNC_have")"'
note '# 这条「先弄响再报 0」的规矩本章用了三次（第 18、23、24 节），它也是第 21 节那个库的组成部分。'

# ============================================================================
banner "第 19 节 argpass.sh 完整跑：函数的参数从哪来"
# ============================================================================
note '# 七格：函数一进来 $# 就是传进来的个数、"$*" 与 "$@"、IFS 改成 - 只有 "$*" 受影响、'
note '#   函数里 set -- 改不了外层、三种传参写法的真实后果（含空格与通配）、名字接不住值、收口。'
runc 'bash argpass.sh'

# ============================================================================
banner "第 20 节 小证据：转发参数只有一个姿势"
# ============================================================================
runc 'show(){ printf "  收到 %s 个：\n" "$#"; for a in "$@"; do printf "    [%s]\n" "$a"; done; }
show "a b" "" c'
runc 'count(){ printf "%s\n" "$#"; }
x="a b"
printf "  不加引号 f \$x      → %s 个\n" "$(count $x)"
printf "  加引号   f \"\$x\"    → %s 个\n" "$(count "$x")"
printf "  转发     f \"\$@\"    → 见下一条"'
runc 'inner(){ printf "  inner 看到 %s 个：" "$#"; for v in "$@"; do printf "[%s] " "$v"; done; echo; }
outer(){ inner "$@"; }
outer "带 空格" "" 单个'
note '# ★ 空串也占一格（上面 $#=3）；把 "$@" 换成 "$*"，第三格就变成一格长文本。'
note '#   写包装函数时唯一正确的转发是 "$@"，其余写法都在悄悄改变格数。'

# ============================================================================
banner "第 21 节 综合演练：一个能上线的小函数库"
# ============================================================================
note '# 四条规矩，逐条来自前面：日志走 stderr（第 5/7 节）、失败用 return 不用 exit（第 3/9 节）、'
note '#   中间变量一律 local（第 11/12/13 节）、转发只用 "$@"（第 19/20 节）、查命令用 command -v（第 16 节）。'
cat > lib/fnlib.sh <<'LIBEOF'
# 本章的收尾件：一个可以直接抄进生产脚本的小函数库。
# 它没有一个 echo 走 stdout（除了「给出数据」的那一个），所以任何一句都能安全地被 $( ) 捕获。

log()  { printf 'log: %s\n' "$*" >&2; }            # 诊断：只上 stderr，永远不污染捕获
die()  { printf 'die: %s\n' "$*" >&2; return 1; }   # 报告并交出一个非 0，杀不杀由调用方决定
have() { command -v "$1" >/dev/null 2>&1; }         # 命令存在吗：答案在 $? 里，不在输出里

# retry <最多几次> <命令...>：靠退出码轮询。故意不 sleep —— 耗时每跑必变，本章不上屏。
retry() {
  local max="$1" i=1
  shift
  while [ "$i" -le "$max" ]; do
    if "$@"; then
      printf 'retry: 第 %s 次成功\n' "$i" >&2
      return 0
    fi
    printf 'retry: 第 %s 次失败\n' "$i" >&2
    i=$((i + 1))
  done
  return 1
}

# last_line：真正的「返回数据」——把 stdout 交出去，同时用 local 接住尾换行的后果。
last_line() {
  local all
  all="$("$@")"
  printf '%s\n' "${all##*$'\n'}"
}
LIBEOF
runc 'cat lib/fnlib.sh'
runc 'cd lib || exit 1
source ./fnlib.sh
log 启动 "两个参数"
v=$(log 这条不会进 v; echo 真正的值)
printf "  捕获结果=[%s] 长度=%s ｜ ★ log 走 stderr，所以捕获干干净净\n" "$v" "${#v}"'
runc 'cd lib || exit 1
source ./fnlib.sh
have bash;      printf "  have bash（本机在）        → 码=%s\n" "$?"
have 没有这个命令; printf "  have 没有这个命令（不在）  → 码=%s\n" "$?"'
runc 'cd lib || exit 1
source ./fnlib.sh
n=0
flaky(){ n=$((n + 1)); [ "$n" -ge 3 ]; }
retry 5 flaky; printf "  前两次必失败、retry 5 次额度 → 最终码=%s（计数器 n=%s）\n" "$?" "$n"
retry 2 flaky; printf "  再给 2 次额度（此时 n 已 >=3）→ 最终码=%s\n" "$?"'
runc 'cd lib || exit 1
source ./fnlib.sh
if die "这一步炸了"; then echo "  不该走到 then"; else echo "  die 交出一个码，但脚本还活着 → 这行印出来了"; fi
printf "  die 之后还能继续说话，说明它用的是 return 不是 exit\n"'
runc 'cd lib || exit 1
source ./fnlib.sh
printf "  last_line 拿数据：[%s]\n" "$(last_line printf "1\n2\n3\n")"'
note '# 这个库的每一行都能回指到本章某一节 —— 这就是「函数只有码、数据走 stdout、'
note '#   名字是动态作用域、可见性由名字表决定」四句话落到实处的样子。'

# ============================================================================
banner "第 22 节 把这个库交给另一个进程：三条路，和 dash 的一堵墙"
# ============================================================================
cat > lib/try_source.sh <<'SRCEOF'
source ./fnlib.sh
echo "  〔路①source〕本进程里 have 能用吗："
have bash && echo "    能用（码 0）"
echo "  〔路①的边界〕同一个 have，孙子进程还是看不见："
bash -c 'have bash'
echo "    ↑ 上面是原文错，退出码=$?"
SRCEOF
cat > lib/try_export.sh <<'EXPEOF'
source ./fnlib.sh
export -f have
echo "  〔路②export -f〕把定义塞进环境，交给新起的 bash："
bash -c 'have bash && echo "    孙子进程能用 have（码 0）"'
echo "  〔路②的代价〕env 里多了一个键名很怪的条目："
env | grep '^BASH_FUNC_have' | sed -n 's/=.*/=<函数体，以 () { 开头>/p'
EXPEOF
cat > lib/try_declare.sh <<'DECEOF'
source ./fnlib.sh
declare -f log die have retry > funcs.inc
printf "  〔路③declare -f〕把定义抄成纯文本，%s 行：\n" "$(wc -l < funcs.inc)"
printf "  抄出来的第一行长这样："; head -1 funcs.inc
bash -c 'source ./funcs.inc; have bash && echo "    新进程 source 这份文本就复活了（码 0）"'
DECEOF
cat > lib/try_dash.sh <<'DASHEOF'
printf "  本机 /bin/sh 的真身：%s\n" "$(readlink -f /bin/sh)"
echo "  ① 同一个名字，让 dash 来执行 export -f："
sh -c 'export -f have'
printf "    退出码=%s（原文就在上面那行）\n" "$?"
echo "  ② 让 dash 执行 declare -f（本章第 22 节的路③靠它）："
sh -c 'declare -f'
printf "    退出码=%s\n" "$?"
echo "  ③ 让 dash 在函数外面写 local："
sh -c 'local x=1'
printf "    退出码=%s\n" "$?"
echo "  ④ 把本章那个库文件整份交给 dash 读："
sh ./fnlib.sh
printf "    退出码=%s ★ 库里那句 ANSI-C 转义（dollar-quote）dash 不认 —— 换 shell 就不是同一套语法\n" "$?"
DASHEOF
runc_at lib 'bash try_source.sh'
runc_at lib 'bash try_export.sh'
runc_at lib 'bash try_declare.sh'
runc_at lib 'bash try_dash.sh'
note '# ★ 三条路的分工：source 只管当前进程；export -f 能跨进程但把每一份子进程的环境都弄脏，'
note '#   而且 dash/busybox sh 根本不认；declare -f 抄出来的纯文本是最 portable 的一族写法'
note '#   （它落到磁盘上，就成了「库文件」——第 15 节那条「函数不是磁盘上的文件」的反面）。'

# ============================================================================
banner "第 23 节 对账：九个脚本各几格、合计几格、demo 自己几节"
# ============================================================================
note '# 先看守卫能不能响：故意用一个匹配不到任何东西的模式跑一遍，九个 0 —— 那这守卫是假的。'
runc 'for f in retcode outcapture errmix trapreturn localscope recursion functype exportfun argpass; do
  printf "  %-12s 假守卫计数=%s\n" "$f" "$(grep -c "=== ZZZ-NOT-THERE " "$f.sh")"
done
echo "  ★ 全 0 —— 所以下面这条真计数必须非 0，否则同样不可信"'
runc 'total=0
for f in retcode outcapture errmix trapreturn localscope recursion functype exportfun argpass; do
  n=$(grep -c "=== " "$f.sh")
  printf "  %-12s %s 格\n" "$f" "$n"
  total=$((total + n))
done
printf "  ★ 九个脚本合计 %s 格\n" "$total"'
runc 'printf "  demo 自己的节标题数（grep ^banner）：%s\n" "$(grep -c "^banner " demo.sh)"
printf "  章目录里除 demo 外的脚本数：%s\n" "$(ls *.sh | grep -vc "^demo.sh$")"'
runc 'printf "  源文件行数：demo=%s ｜ 九脚本=%s\n" "$(wc -l < demo.sh)" "$(cat retcode.sh outcapture.sh errmix.sh trapreturn.sh localscope.sh recursion.sh functype.sh exportfun.sh argpass.sh | wc -l)"'
note '# 这里数的「格」是脚本源码里的分格标题行；每一格都在屏幕上真实出现过（第 3~19 节逐条跑过）。'

# ============================================================================
banner "第 24 节 自证：沙箱里的事只留在沙箱"
# ============================================================================
note '# ① 章目录（沙箱外面）开跑前后各列一次条目名，比对是否一模一样。'
AFTER_LIST=$(cd "$HERE" && ls -A | sort | tr '\n' '|')
printf '  开跑前 %s 个条目 ｜ 现在 %s 个条目 → %s\n' \
    "$(printf '%s' "$BEFORE_LIST" | tr '|' '\n' | grep -c .)" \
    "$(printf '%s' "$AFTER_LIST" | tr '|' '\n' | grep -c .)" \
    "$([ "$BEFORE_LIST" = "$AFTER_LIST" ] && echo 一模一样 || echo "不一样：[$BEFORE_LIST] vs [$AFTER_LIST]")"
note '# ② 每个辅助脚本自己的现场子目录是否都删干净了（先故意造一个，让这道守卫响一次）。'
runc 'mkdir -p zz-故意留下的目录
printf "  故意造一个多余目录之后，多余目录数=%s（★ 守卫响了）\n" "$(ls -d */ 2>/dev/null | grep -v "^lib/$" | wc -l)"
rmdir zz-故意留下的目录
printf "  删掉之后，多余目录数=%s（九个脚本的子目录都由它们自己的 trap 清掉了）\n" "$(ls -d */ 2>/dev/null | grep -v "^lib/$" | wc -l)"'
note '# ③ 沙箱 work 里现在还剩什么（只印相对条目名）。'
runc 'ls -A | sort | tr "\n" " "; echo'
runc 'printf "  lib/ 里剩：%s\n" "$(ls -A lib | sort | tr "\n" " ")"'
note '# ④ 本章没写过联网、装包、开监听端口的语句。扫描对象是九个取证脚本 + lib/ 里的辅助脚本。'
note '#   守卫先要能响，这个 0 才算数 —— 所以这里插一个含禁词的假脚本把守卫弄响，再撤掉。'
runc 'pat="curl|wget|nc -l|nohup|pip install|apt-get|yum install"
list=(retcode.sh outcapture.sh errmix.sh trapreturn.sh localscope.sh recursion.sh functype.sh exportfun.sh argpass.sh lib/*.sh)
printf "curl -s https://example.com\napt-get install x\n" > zz-probe.sh
printf "  ④-1 先弄响：把探针 zz-probe.sh 算进来，命中文件数=%s\n" "$(grep -lE "$pat" "${list[@]}" zz-probe.sh | wc -l)"
rm zz-probe.sh
printf "  ④-2 撤掉探针：同一道守卫命中文件数=%s（★ 响过一次，这个 0 才算数）\n" "$(grep -lE "$pat" "${list[@]}" | wc -l)"'
runc 'pat="curl|wget|nc -l|nohup|pip install|apt-get|yum install"
list=(retcode.sh outcapture.sh errmix.sh trapreturn.sh localscope.sh recursion.sh functype.sh exportfun.sh argpass.sh lib/*.sh)
printf "  ④-3 假阳性对照：把 demo.sh 也扫进来，命中文件数=%s\n" "$(grep -lE "$pat" "${list[@]}" demo.sh | wc -l)"
printf "     ↑ 多出来的那一个就是 demo.sh —— 禁词的字面文本就写在它自己那行 pat= 里。\n"
printf "     任何「扫源码找禁词」的守卫都会自匹配，所以扫描清单必须把持有模式串的那个文件挑出去。\n"
printf "     本章的处理：清单里不含 demo.sh，④-2 的那个 0 才是真结论。\n"'
runc 'printf "  本 shell 的后台作业数=%s（本章没起过 & 作业）\n" "$(jobs -l | wc -l)"'
note '# ⑤ 屏幕上不该有真实身份。匹配串故意写成 /hom[e]/ 带方括号：'
note '#   不带方括号时，它会命中「写着它自己的那一行」（永远非 0 的假阳性，④-3 就是活例子）；'
note '#   带了方括号，模式文本不再自匹配，只有真实路径才算命中。同样先拿假想路径把守卫弄响。'
runc 'list=(retcode.sh outcapture.sh errmix.sh trapreturn.sh localscope.sh recursion.sh functype.sh exportfun.sh argpass.sh lib/*.sh demo.sh)
printf "/hom%s/somebody/nowhere\n" "e" > zz-path-probe.txt
printf "  ⑤-1 先弄响：把含假想绝对路径的探针算进来，命中=%s 处\n" "$(grep -o "/hom[e]/[A-Za-z0-9._-]*" "${list[@]}" zz-path-probe.txt | wc -l)"
rm zz-path-probe.txt
printf "  ⑤-2 撤掉探针：连 demo.sh 自己一起扫，命中=%s 处（★ 响过一次，这个 0 才算数）\n" "$(grep -o "/hom[e]/[A-Za-z0-9._-]*" "${list[@]}" | wc -l)"'
note '# ⑤-3 这一节只印「命中几处」，不印命中的内容：grep -o 会把匹配到的路径原样送上屏幕，'
note '#   数一遍就够 —— 想把内容印出来，等于亲手把家目录打到捕获里（所以第 3~19 节全用相对文件名跑）。'
note '# ⑥ 真实身份的整屏扫描（用户名/主机名/家目录）由 README「怎么跑这一章」那道闸门在捕获外面做。'
printf '  demo 自己跑到这里，最后一节的退出码=%s\n' "$?"

exit 0
