#!/usr/bin/env bash
# ============================================================================
# 第 01 章《交互 shell、脚本、子 shell：到底谁在跑》—— 整章证据一键复现
#
#   bash demo.sh            跑完自动删沙箱（默认）
#   bash demo.sh --keep     跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt  重定向时自动关掉颜色，README 引用的就是这份纯文本
#
# 屏幕上出现的每一行，都是这条命令在这台机器上的真实结果 —— 没有一行是手写的。
#
# 安全说明：
#   * 所有写操作只发生在 mktemp -d 出来的沙箱里；chmod/cp/rm 全部只作用于沙箱内的副本
#   * 绝不修改 ~/.bashrc、~/.profile、~/.config、/etc：需要「rc 生效」的效果时，
#     在沙箱里写一个临时 rc，用 `bash --rcfile ./myrc -ic` 演示
#   * 不 kill 任何不是本脚本起的进程（本脚本压根不起后台进程）；不起监听端口
#   * 不联网、不装包（缺的工具如实打印，例如 shellcheck）
#
# 为什么这个脚本**故意不加** set -e：
#   本章要的就是「报错原文 + 脚本继续跑到下一行」这种现场。加了 -e，第一个错误就把
#   整个 demo 掐了，你反而看不见「bash 默认不会因为出错而停下」这条最重要的事实。
#   （set -euo pipefail 为什么生产必加，是第 08 章整章的内容。）
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

set -u                  # 只用 -u：变量名写错要当场炸，但命令失败不许中断演示

# ----------------------------------------------------------------------------
# 把语言环境钉死成「英文 + UTF-8」。
# 这台机器默认是 zh_CN.UTF-8，bash 的报错会是中文（例如「未找到命令」），
# README 就没法逐行和真实输出核对了。注意：**你的终端没被改动**，
# 改动只发生在这个脚本和它 fork 出来的子进程里。
# ----------------------------------------------------------------------------
ORIG_LANG="${LANG-（未设）}"
ORIG_LANGUAGE="${LANGUAGE-（未设）}"
ORIG_LC_ALL="${LC_ALL-（未设）}"
export LC_ALL=C.UTF-8 LANG=C.UTF-8 LANGUAGE=C

# 只有输出真的接到终端时才上色；重定向到文件时是干净纯文本
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/bash01-XXXXXX")

banner() { printf '\n══════ %s %s\n' "$1" "════════════════════════════════"; }
note() { printf '%s\n' "$1"; }

# 打印命令 → 用 bash -c 真跑 → 打印退出码。
# 用 bash -c 是为了能自然地写管道、`;`、glob；也保证每段都在一个
# **全新的非交互 bash** 里开始，段与段之间不互相污染。
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去接着敲： cd "%s/work"     看完删掉： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# ============================================================================
banner "0. 这台机器上，shell 到底是什么样子"
# ============================================================================
note '# demo.sh 自己把语言钉成了 C.UTF-8，所以你看到的报错是英文；'
note "# 你原来的终端是：LANG=$ORIG_LANG  LANGUAGE=$ORIG_LANGUAGE  LC_ALL=$ORIG_LC_ALL"
note '# 这就是同一句 bash 报错在你屏幕上可能是中文、在本 README 里是英文的原因。'
runc 'bash --version | head -3'
runc 'which bash sh'
runc 'ls -l /bin/sh'
runc 'readlink -f "$(which sh)"'
runc 'dpkg -l bash dash 2>/dev/null | grep ^ii'
runc 'echo "登录 shell 环境变量 \$SHELL = [$SHELL]"'
runc '/bin/sh --version 2>&1 | head -1'
note '# 上面那句 dash 不认 --version，报错原文是「Illegal option」—— 这也是一种证据：它不是 bash。'
runc 'type -a echo'
runc 'type -a test'
note '# 「内建 vs 外部」不是学术问题：同是 echo，三个实现的结果不一样（本机实测）：'
runc 'echo "a\nb"'
runc 'echo -e "a\nb"'
runc '/usr/bin/echo "a\nb"'
runc 'dash -c '\''echo "a\nb"'\'''
runc 'printf "a\nb\n"'
note '# ↑ 结论：bash 的内建 echo 默认**不**解释反斜杠转义（要加 -e），dash 的内建 echo 会解释，'
note '#   外部 /usr/bin/echo 又是另一个实现。跨机器写脚本请一律用 printf。'
runc 'ls --version | head -1'
runc 'sed --version | head -1'
runc 'awk --version | head -1'
runc 'grep --version | head -1'
note '# ↑ 本机外部命令的实现：coreutils 是 **uutils（Rust 版）**，awk 是 **mawk** 而不是 gawk。'
note '#   这直接决定第 07 章能写哪些 awk 特性，也决定别照抄 GNU 专属的 date/tr 写法。'
runc 'command -v shellcheck || echo "shellcheck: 本机没有（第 09 章给出安装途径，本次没有安装）"'
runc 'command -v xdg-open strace'
note '# shopt 是「当前 shell 的行为开关」表，一共几十个，这里只看前 10 行：'
runc 'shopt | head'
runc 'shopt | wc -l'

# ============================================================================
banner "1. 复制示例进沙箱；shebang 到底管不管用"
# ============================================================================
mkdir -p "$SB/work"
cp "$HERE"/*.sh "$SB/work/"
cd "$SB/work" || exit 1
runc 'chmod +x *.sh && ls -l *.sh | awk "{print \$1, \$NF}"'

note '# 用同一个文件体做两个版本：sb_yes.sh 保留 shebang，sb_no.sh 把第一行删掉。'
runc 'cp bashonly.sh sb_yes.sh'
runc 'tail -n +2 bashonly.sh > sb_no.sh'
runc 'chmod +x sb_yes.sh sb_no.sh'
runc 'echo "=== sb_yes.sh 第 1 行 ==="; head -1 sb_yes.sh; echo "=== sb_no.sh 第 1 行 ==="; head -1 sb_no.sh'

note '# ① 有 shebang：内核读第一行，按 #!/usr/bin/env bash 起 bash。'
runc './sb_yes.sh | head -4'
note '# ② 没有 shebang，但从 bash 里调用：内核 execve 失败（ENOEXEC），'
note '#    **调用方** bash 兜底，用「自己」来解释这个文件 —— 所以它居然也能跑。'
runc './sb_no.sh | head -4'
note '# ③ 同一个没有 shebang 的文件，换成从 dash 调用：兜底的是 dash，立刻炸。'
note '#    → 没有 shebang 时「谁调用你」决定你用哪种语法，脚本行为不可复现。'
runc 'sh -c "./sb_no.sh" | head -2'
note '# ④ 最反直觉的一条：**显式指定解释器时，shebang 被完全无视**。'
note '#    sb_yes.sh 明明写着 #!/usr/bin/env bash，但 `sh sb_yes.sh` 就是让 dash 去读它。'
runc 'sh sb_yes.sh | head -2'
runc 'dash sb_yes.sh | head -2'
note '# ⑤ 于是 shebang 的正确理解：它只对「./xxx 这种让内核去 exec 的方式」有效。'
runc 'bash sb_no.sh | head -2'
note '# ⑥ `env bash` 和 `/bin/bash` 的差别：前者按 PATH 找 bash，后者写死路径。'
runc 'echo "PATH 里的 bash：$(command -v bash)"; head -1 sb_yes.sh'

note '# ⑦ dash 到底哪些语法没有？逐个隔离试（每个都用**单独**的命令，否则第一个错误就中断后面全部）：'
cat > dash_probe.sh <<'DASHPROBE'
arr=(a b)
echo "数组可用"
DASHPROBE
runc 'dash dash_probe.sh'
runc 'bash dash_probe.sh'
runc 'dash -c '\''if [[ 1 == 1 ]]; then echo "[[ 可用"; fi'\'''
runc 'bash -c '\''if [[ 1 == 1 ]]; then echo "[[ 可用"; fi'\'''
note '#   注意上面 dash 那条退出码是 0：[[ 被当成「不存在的命令」，条件为假而已。'
note '#   单独跑才看得见 127；放进 if/else 里则**静默走 else**，这才是最坑的地方。'
runc 'dash -c '\''[[ 1 == 1 ]]'\'''
runc 'dash -c '\''if [[ 1 == 1 ]]; then echo 走了then; else echo 走了else; fi'\'''
runc 'dash -c '\''cat <<< hi'\'''
runc 'bash -c '\''cat <<< hi'\'''
cat > ansi_probe.sh <<'ANSIPROBE'
v=$'a\nb'
printf '  $%s 展开结果：[%s]\n' "'a\\nb'" "$v"
ANSIPROBE
runc 'dash ansi_probe.sh'
runc 'bash ansi_probe.sh'
note '# ↑ 这四组就是「sh 不是 bash」的完整证据：数组直接语法错误、[[ 被当成不存在的命令、'
note '#   <<< 语法错误、ANSI-C 引用（dollar 加单引号）在 dash 里不展开，$ 原样留着。'

note '# ⑧ 忘了加执行权限时 ./xxx 的报错原文（退出码 126，和「命令不存在」的 127 不是一回事）'
runc 'chmod -x sb_no.sh'
runc './sb_no.sh'
note '# 同一个文件换成 bash 来跑完全没问题 —— bash 只需要「能读」这个文件：'
runc 'bash sb_no.sh | head -2'
runc 'chmod +x sb_no.sh'

# ============================================================================
banner "2. bash x.sh / ./x.sh / source x.sh：三种跑法的真实进程关系"
# ============================================================================
note '# 外层这个 shell 先报出自己的 pid，再跑 pids.sh，让它报自己的 pid。'
note '# ① bash pids.sh —— fork 一个新 bash 进程；环境继承，但它自己赋的值出不去。'
runc 'echo "外层 pid=$$"; bash pids.sh; echo "外层 pid=$$ 还在；外面看到 SHELLVAR=[$SHELLVAR]"'
note '# ② ./pids.sh —— 同样是新进程，区别只在于 shebang 由内核来解释，$0 会带上 ./'
runc 'echo "外层 pid=$$"; ./pids.sh; echo "外层 pid=$$ 还在；外面看到 SHELLVAR=[$SHELLVAR]"'
note '# ③ source pids.sh —— 不 fork！直接在当前 shell 里逐行执行，所以 $0、$$ 都是外层 shell 的，'
note '#    而且脚本里赋的变量外面看得见（这正是 ~/.bashrc 必须被 source、不能被执行的原因）。'
runc 'echo "外层 pid=$$"; source pids.sh; echo "外层 pid=$$ 还在；外面看到 SHELLVAR=[$SHELLVAR]"'
note '# ④ `. pids.sh`（POSIX 的点命令）和 source 是同一件事，只是可移植写法。'
runc 'echo "外层 pid=$$"; . pids.sh; echo "外层 pid=$$ 还在；外面看到 SHELLVAR=[$SHELLVAR]"'
note '# ⑤ 副作用对比：source 里的 cd 和赋值会留在外面，两种 fork 跑法都不会。'
runc 'printf "cd /tmp\nGUESS=from-source\n" > cdtest.sh; echo "source 前目录=$(pwd)"; source ./cdtest.sh; echo "source 后目录=$(pwd) GUESS=[$GUESS]"'
runc 'echo "全新的 shell 里：目录=$(pwd) GUESS=[${GUESS-（没有：上一段 source 的效果不跨进程）}]"'

# ============================================================================
banner "3. 子 shell 的三个来源，以及「管道里改的变量外面看不到」"
# ============================================================================
runc 'bash subshell.sh'
note '# 上面用的判据是 $BASHPID。$$ 在子 shell 里是不变的，靠 $$ 判断永远看不出子 shell。'
note '# 现在把这个坑单独拎出来，并给出三种真正的解法：'
runc 'n=0; printf "a\nb\nc\n" | while IFS= read -r line; do n=$((n+1)); done; echo "管道右侧累加，外面 n=[$n]"'
runc 'shopt -s lastpipe; m=0; printf "a\nb\nc\n" | while IFS= read -r line; do m=$((m+1)); done; echo "开了 shopt -s lastpipe，外面 m=[$m]"'
runc 'k=0; while IFS= read -r line; do k=$((k+1)); done < <(printf "a\nb\nc\n"); echo "改用进程替换喂 while（第 04 章细讲），外面 k=[$k]"'
runc 'printf "a\nb\nc\n" > f.txt; j=0; while IFS= read -r line; do j=$((j+1)); done < f.txt; echo "改成从文件重定向读，外面 j=[$j]"'

# ============================================================================
banner '4. 脚本里的 $0 $1 $# $@ $* 到底是什么（含 set --）'
# ============================================================================
note '# 传参时 "beta gamma" 带引号，所以它是一个参数：$# 应当是 3，不是 4。'
runc 'bash posargs.sh alpha "beta gamma" delta'
note '# 同一条脚本换个调用方式，$0 就变了 —— $0 不是常量，是「你怎么被叫起来的」。'
runc './posargs.sh one two'

# ============================================================================
banner "5. 新手最需要背下来的报错原文"
# ============================================================================
runc 'bash errors.sh'
note '# 上面最后一段（set -u 引用未定义变量）让脚本当场结束，所以退出码非 0 —— 这就是 set -u 的价值。'
note '# 语法错误是另一类：它在「解析」阶段就被抓住。'
runc 'bash broken_syntax.sh'
note '# 关键现场：报错**之前**的 echo 已经打印出来了。bash 是边解析边执行，'
note '# 所以脚本可能在「已经写了一半文件」之后才炸。第 08 章的 trap 就是处理这个的。'

# ============================================================================
banner "6. shell 变量 vs 环境变量：中间隔着一次 fork"
# ============================================================================
runc 'bash envcheck.sh'
runc 'echo "当前环境里的变量条数：$(env | wc -l)"'
runc 'env -i /usr/bin/env | wc -l'
note '# 上一行是 env -i（清空所有环境变量）之后再让 env 打印自己收到的环境 —— 0 行，真·干净环境。'

# ============================================================================
banner "7. alias 和 shopt：交互 shell 里有，非交互脚本里没有"
# ============================================================================
note '# 沙箱里造一个 rc 文件（**不碰你的 ~/.bashrc**），里面设一个 alias 和两个 shopt 开关。'
runc "printf 'alias mytestalias=\"echo ALIAS 生效了\"\nshopt -s nullglob\nshopt -s cdable_vars\n' > myrc; cat myrc"
note '# ① 非交互（脚本的默认方式）：不读任何 rc，alias 表是空的，expand_aliases=off。'
runc 'bash rcprobe.sh'
note '# ② 同一份 rcprobe.sh，用 --rcfile 假装成交互 shell：alias 有了，shopt 也变了。'
runc 'bash --rcfile ./myrc -ic "source ./rcprobe.sh"'
note '# 上面头两行「cannot set terminal process group / no job control」不是结论的一部分，'
note '# 是 -i 在没有真终端（输出被接到管道）时的正常抱怨，忽略即可。'
note '# ③ 只看「交互与否」本身的开关差别，用 --norc 排除你自己 rc 文件的干扰：'
runc 'diff <(bash -c "shopt | sort") <(bash --norc --noprofile -ic "shopt | sort" 2>/dev/null) || echo "（上面就是全部差别）"'
note '# ④ 结论：alias 是交互 shell 的糖。脚本里要么别用，要么显式 shopt -s expand_aliases，'
note '#    要么直接写成函数。你终端里能用的 alias 写进 .sh 就是不生效 —— 这不是 bug，是规则。'
note '# 顺带一个查开关时的真坑（见 rcprobe.sh 注释）：shopt -q 的退出码才是开关状态，'
note '#   而 `shopt -p 名字` 虽然正常打印出 shopt -u 名字，退出码却是 1 —— 用它做 || 兜底会误判。'
runc 'shopt -p expand_aliases; echo "shopt -p 的退出码 = [$?]（开关明明是正常关闭的）"'

# ============================================================================
banner "8. bash -n（只解析不跑）和 bash -x（每行都告诉你真正执行了什么）"
# ============================================================================
runc 'bash -n pids.sh && echo "pids.sh 语法检查通过（一条命令都没执行）"'
runc 'bash -n errors.sh && echo "errors.sh 语法检查通过 —— 可它一跑就连报 5 个错：-n 管不到运行时错误"'
runc 'bash -n broken_syntax.sh'
note '# 上面这个报错和场景 5 里执行时的报错一模一样，但 -n 完全没有执行文件里的任何命令。'
note '# 先看默认提示符（PS4 的默认值就是一个「+ 」）：'
runc 'bash -x pids.sh 2>&1 | head -14'
note '# 再看自定义 PS4（trace_demo.sh 开头设的），把行号和函数名也带上：'
runc 'bash -x trace_demo.sh 2>&1 | head -26'
note '# 读法：+ 后面是**展开之后**的命令行，不是你以为的那一行。'
note '# 「+ 有几个」这件事，本机实测的真相是（很多人记错）：'
runc 'set -x; v=$(printf hi); echo "v=$v"'
note '# ↑ 命令替换 $( ) 里多了一个 +（真的 fork 了子 shell）。'
runc 'set -x; f() { local a=1; echo "$a"; }; f; ( g=2; echo "$g" ); { h=3; echo "$h"; }'
note '# ↑ 函数调用、( )、{ } 全都只有一个 + —— 所以「+ 的个数」不能用来判断有没有进函数，'
note '#   只有真的 fork（命令替换、管道各段、子 shell）才会加一个 +。'
runc 'echo "PS4 的默认值是：[$PS4]"'

printf '\n[全部跑完] 上面每一段都是真实执行结果；README.md 引用的输出全部出自这一次运行。\n'
exit 0
