#!/usr/bin/env bash
# =============================================================================
# 03-testing-and-exit-codes/demo.sh
#
# 这个文件在演什么：
#   把「决策层」的全部真相跑成一份可复现的捕获：
#     1. 退出码是 if/&&/|| 唯一能看懂的东西，$? 从哪来、谁会把它改掉；
#     2. 常见命令的退出码约定（grep / diff / test / 被信号杀死 / 126 / 127）逐条真测；
#     3. `[ ]` 其实是一个**程序**，所以空格是语法、参数个数决定解释方式；
#     4. `[ ]` 的经典事故全部现场复现（未加引号的变量、-eq 与 =、-a/-o 优先级、< 被当重定向）；
#     5. 一整套文件测试运算符（-e -f -d -L -nt -ot -ef -S -b -c -p -u -g -k -t …），
#        每个都拿沙箱里**现造出来的真对象**（符号链接、断链、硬链接、fifo、unix socket、
#        setuid 位、/dev/null、/dev/loop0）测一遍；
#     6. `[[ ]]` 为什么存在：同一份未加引号的变量分别喂给 `[ ]` 和 `[[ ]]`；
#     7. =~ 正则（右边不能加引号）、BASH_REMATCH、&&/|| 从左往右、! 与 set -e 互相拆台、
#        case 的 ;; / ;& / ;;& 三种轨迹与 glob 分派；
#     8. test / [ / [[ 的关系，以及「什么时候只能用 [ ]」：sh（本机是 dash）现场跑；
#     9. 判据写法的取舍（括号转义、if ! 与 set -e、取反的位置）；
#    10. 一个 30 行的真决策脚本，把上面全部串起来。
#
# 为什么要演：
#   这一章的规则全是「看起来懂、写出来错」的那类。任何一条只要我没在本机跑出真输出，
#   README 里就不写。每个例子都带 rc=N，因为结论就是那个数字。
#
# 安全说明：
#   - 所有副作用只在 mktemp -d 出来的沙箱（/tmp/bash03-XXXXXX）里；脚本末尾 trap 里 rm -rf。
#   - 绝不往你的家目录、绝不往仓库目录写任何东西；系统文件只做只读判断（/dev/null、/tmp、
#     /etc/passwd 的读、/etc/shadow 的「能不能读」判断）。
#   - 不联网、不 sudo、不装包、不跑 git 写操作。setuid 文件只造出来**判位**，从不执行它。
#   - 跑法：bash demo.sh         （或 bash demo.sh --keep 保留沙箱接着玩）
#          bash demo.sh 2>&1 | tee ~/.cache/handbook-captures/bash03.txt   （写 README 用的捕获）
# =============================================================================

# --keep：把沙箱留着，方便你自己进去接着敲；默认跑完就删。
KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help)
            printf '用法: bash demo.sh [--keep]\n  --keep  跑完不删沙箱，并打印路径\n'
            exit 0 ;;
        *) printf '未知参数: %s（只认 --keep / --help）\n' "$arg" >&2; exit 2 ;;
    esac
done

# 注意：**没有** set -e。这一章的主角就是「非 0 退出码」，
# 加了 set -e 会让一半例子当场中断，你就看不到 $? 的真实值了（第 9 节有对照实验）。
set -u

# 把语言钉死成英文，否则 bash 的报错原文是中文，README 里引的报错就不唯一了。
# 先把原值记下来，第 0 节打出来给你看 —— 你终端里的报错是中文就是这个原因。
ORIG_LANG="${LANG-（未设）}"
ORIG_LANGUAGE="${LANGUAGE-（未设）}"
ORIG_LC_ALL="${LC_ALL-（未设）}"
export LC_ALL=C.UTF-8 LANG=C.UTF-8 LANGUAGE=C

# 只有真的接到终端才上色；被 tee 捕获时是干净纯文本（第 5 节会用 -t 反过来证明这条）。
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/bash03-XXXXXX")

banner() { printf '\n══════ %s %s\n' "$1" "════════════════════════════════"; }
note() { printf '%s\n' "$1"; }

# 打印命令 → 用 bash -c 真跑一遍 → 打印退出码。
# 用全新的 bash -c 有两个好处：① 能自然写管道、;、glob；② 段与段互不污染，
# 最后那行 [退出码 N] 就是这条命令自己的码。
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去接着敲： cd "%s/work"     看完删掉： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        cd /tmp || true
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# 助手脚本一律**复制进沙箱再跑**：很多报错原文会带上 $0 的路径，
# 直接在仓库目录里跑就会把家目录打到屏幕上，捕获就不干净了。
mkdir -p "$SB/work"
cp "$HERE"/*.sh "$SB/work/"
cd "$SB/work" || exit 1

# ============================================================================
banner "0. 先确认这台机器上「[ 」到底是什么东西"
# ============================================================================
note '# demo.sh 把语言钉成了 C.UTF-8，所以下面所有报错原文都是英文；'
note "# 你原来的终端是：LANG=$ORIG_LANG  LANGUAGE=$ORIG_LANGUAGE  LC_ALL=$ORIG_LC_ALL"
note '# 同一句 [ -z $x ]，你屏幕上可能是中文报错 —— 这就是差别。'
runc 'bash --version | head -2'
note '# [ 和 test 同时存在「内建」和「外部程序」两份，type -a 会把两份都列出来：'
runc 'type -a [ test'
runc 'command -v [ test'
note '# 外面那份 /usr/bin/[ 在这台机器上是什么？（用 stat 不用 ls -l，免得把用户名打到屏幕上）'
runc 'stat -c "%s 字节  %F  %n" /usr/bin/[; readlink -f /usr/bin/['
note '# [[ 不是命令，是 bash 语法里的**保留字**，所以 type 的说法完全不同：'
runc 'type -a "[["'
note '# 外部工具的实现决定报错原文，先记下（第 2 节的 grep/diff 退出码就是它们的实现给的）：'
runc 'grep --version | head -1'
runc 'diff --version | head -1'
runc 'sed --version | head -1'
runc 'command -v awk; awk -W version 2>&1 | head -1'
runc 'command -v shellcheck || echo "shellcheck 本机没有：本章每个写法都是靠真跑验证的，没有静态检查器的意见"'

# ============================================================================
banner "1. 退出码：if 唯一能看懂的东西；\$? 从哪来、谁会改掉它"
# ============================================================================
note '# 最小的一组事实：命令自己返回的数字存在 $? 里，而且只有一条命令的距离。'
runc 'true; echo $?'
runc 'false; echo $?'
runc 'echo hello; echo $?'
runc 'grep -c root /etc/passwd; echo "$?"'
runc 'grep -c zzz_no_such_pattern /etc/hostname; echo "$?"'
note '# ↑ grep -c 这两条：第一条打印 2、退出码 0；第二条打印 0、退出码 1。'
note '#   打印的是**数据**，判据是**退出码**，两件事。「没匹配到」是 grep 的正常答复，不是错误。'
runc 'ls /nonexistent-file-03; echo "$?"'
note '# ↑ 报错原文和数字都来自外部命令（本机 coreutils 是 uutils 版），bash 只是转述出来。'

note '# 谁在改 $? —— 任何一条命令都会覆盖它，包括 echo 本身，也包括**纯赋值**：'
runc 'false; echo "紧跟 false 的 \$? = $?"'
runc 'false; echo 打断一下 >/dev/null; echo "中间插了一条 echo 之后 \$? = $?"'
runc 'false; :; echo "中间插了一条冒号（内建空命令）之后 \$? = $?"'
runc 'false; v=1; echo "纯赋值 v=1 之后 \$? = $?"'
runc 'false; a=(1 2); echo "数组赋值 a=(1 2) 之后 \$? = $?"'
runc 'false; v=$((5-5)); echo "算术赋值 v=\$((5-5)) 之后 \$? = $?"'
note '# ↑ 结论：想拿退出码就得**紧贴着取**，或者干脆别经过 $? —— 直接把命令写成 if/&& 的条件。'

note '# 「非 0 一律为假」是唯一判据；数值大小不参与判断：'
runc 'for n in 0 1 2 3 126 127 128 255; do if bash -c "exit $n"; then echo "  exit $n → if 走 then（真）"; else echo "  exit $n → if 走 else（假）"; fi; done'
note '# ↑ 2 和 255 和 1 在 if 眼里一模一样。「错误严重到什么程度」不是 shell 的语义，'
note '#   只是**命令作者给人的约定**：0=成功，1=一般失败，2=用法/严重错误，126 127 128+n 有固定来历。'
note '#   这条约定不属于 shell，属于每个命令自己 —— 第 2 节把它一条条跑出来。'

note '# 取值只有 0-255，超出按 mod 256 回绕（第 2 节里 ( exit 300 ) 之后是 44，不是 300）：'
runc 'bash -c "exit 256"; echo "exit 256 → $?"'
runc 'bash -c "exit 257"; echo "exit 257 → $?"'
runc 'bash -c "exit -1"; echo "exit -1 → $?"'

# ============================================================================
banner "2. 常见退出码约定：每一条都是现跑的（exitcodes.sh）"
# ============================================================================
note '# grep 的 0/1/2、diff 的 0/1/2、test 的 0/1/2、被信号杀死 = 128+signum、'
note '# 127（命令找不到）、126（找到了但不能执行）、以及 exit 自己的回绕，'
note '# 全在 exitcodes.sh 里逐条跑。下面的输出就是本节的证据。'
runc 'bash exitcodes.sh'

# ============================================================================
banner "3. 退出码的传播与丢失（status_chain.sh）"
# ============================================================================
note '# 谁会改写 $?、命令替换怎么吃掉它、local v=$(false) 为什么是 0、'
note '# 函数里前面失败了但函数返回 0、管道只留最后一段的码（PIPESTATUS 取证）、'
note '# if [ $? -eq 0 ] 为什么会被一行 echo 打断 —— 都在 status_chain.sh 里。'
runc 'bash status_chain.sh'

# ============================================================================
banner '4. [ ] 是一个程序：空格是语法，参数个数决定解释（bracket_spaces.sh）'
# ============================================================================
note '# 这一节最容易返工：`[` 的身份、空格、参数个数、未加引号的变量、-eq vs =、'
note '# = 与 ==、< 被当成重定向、-a/-o 优先级、[[ ]] 的语法错误会掐断脚本 —— 全部现场复现。'
runc 'bash bracket_spaces.sh'
note '# 再从 demo 这边补三条「[ 就是个命令」的直接证据（同一个判断三种写法）：'
runc '"[" -z "" ]'
note '# ↑ 上一条说明 [ 就是命令名：加引号也能调用，但它仍然是「以 ] 结尾的一串参数」。'
runc '[ -z "" ]; echo "[ 的退出码就是 test 的退出码：$?"'
runc 'test -z ""; echo "写成 test 一字不差：$?"'

# ============================================================================
banner "5. 文件测试运算符全表：每个都用真对象测（file_tests.sh）"
# ============================================================================
note '# file_tests.sh 先在沙箱 objs/ 里造出普通文件、空文件、目录、好链接、断链、硬链接、'
note '# fifo、unix socket、setuid/setgid/sticky 位、000 权限文件、差 2 秒的两个 mtime，'
note '# 再把 -e -f -d -L -r -w -x -s -p -S -b -c -u -g -k -t -nt -ot -ef -O -G 全跑一遍。'
runc 'bash file_tests.sh'
note '# 上面第 6 组里 [ -t 1 ] 全是假，因为 demo 的输出被接到了管道（tee）里。'
note '# 把同一句放进**真 pty** 再看一次，才知道 -t 判的是「这个 fd 接到什么」而不是「脚本长什么样」：'
runc 'script -qec "[ -t 1 ] && echo \"-t 1 在 pty 里为真（标准输出是终端）\" || echo \"假\"; [ -t 0 ] && echo \"-t 0 在 pty 里为真（标准输入是终端）\" || echo \"假\"; true" /dev/null'
note '# ↑ 这就是为什么脚本里 `if [ -t 1 ]; then 上色; fi` 有用：被重定向时它是假，不污染文件。'
note '#   本 demo 开头那句 `if [ -t 1 ]` 用的正是这条规则，所以捕获文件里没有任何转义序列。'

note '# /dev/stdin 到底是哪种类型，取决于它**接到什么**上 —— 写死一个答案就是错的。'
note '# 造一个探针脚本，分别在 stdin=/dev/null、stdin=管道、stdin=普通文件 三种情况下跑：'
cat > stdin_probe.sh <<'STDINPROBE'
for op in -p -c -f -r -s; do
    [ $op /dev/stdin ]; printf '  [ %s /dev/stdin ] rc=%d\n' "$op" "$?"
done
[ -t 0 ]; printf '  [ -t 0 ] rc=%d\n' "$?"
STDINPROBE
runc 'echo "──── 情况1：stdin 接到 /dev/null"; bash stdin_probe.sh < /dev/null'
runc 'echo "──── 情况2：stdin 接到管道"; echo hi | bash stdin_probe.sh'
runc 'echo "──── 情况3：stdin 接到普通文件"; bash stdin_probe.sh < stdin_probe.sh'

# ============================================================================
banner '6. [[ ]] 为什么存在：同一份变量喂给两边（对照实测）'
# ============================================================================
note '# 规则：`[ ]` 是命令，它的参数在**送进去之前**就已经被「拆词 + 文件名展开」改过了；'
note '#       `[[ ]]` 是语法，里面不拆词、不做文件名展开（只有右边才是模式）。'
note '# 先造两个目录：g1 里只有一个文件，g2 里有两个 —— 展开成几个词完全由目录决定。'
runc 'mkdir -p g1 g2; touch g1/onlyfile g2/aaa g2/bbb; ls g1; ls g2'

note '# ① 变量里是「a b」：[ ] 拆成两个词，语句变形；[[ ]] 原样当一个词。'
runc 'x="a b"; [ $x = "a b" ]; echo "[ ] 未加引号 rc=$?"'
runc 'x="a b"; [ "$x" = "a b" ]; echo "[ ] 加引号   rc=$?"'
runc 'x="a b"; [[ $x == "a b" ]]; echo "[[ ]] 不加引号 rc=$?"'

note '# ② 变量里是 glob 字符 *：最阴的一种 —— 不报错，答案是「对」，但判的根本不是你以为的东西。'
runc 'cd g1; x="*"; [ $x = onlyfile ]; echo "目录里只有一个文件时 [ ] rc=$?"'
runc 'cd g1; x="*"; [[ $x == onlyfile ]]; echo "同一个目录里 [[ ]] rc=$?"'
runc 'cd g2; x="*"; [ $x = "*" ]; echo "目录里有两个文件时 [ ] rc=$?"'
runc 'cd g2; x="*"; [[ $x == "*" ]]; echo "同样是两个文件的目录 [[ ]] rc=$?"'
note '# ↑ 结论：用 `[ ]` 时漏一个引号，判据可能**悄悄变真**；`[[ ]]` 把这一类整类消灭掉了。'

note '# ③ `[[ ]]` 的独门武器：&&/|| 直写、== 走 glob、=~ 走正则、-v 判断变量是否已设。'
runc 'a=1; b=2; [[ $a == 1 && $b == 2 ]]; echo "[[ ]] 里直写 && rc=$?"'
runc 'a=1; b=2; [ $a = 1 && $b = 2 ]; echo "[ ] 里直写 && rc=$?（上一行就是报错原文）"'
runc 'f=v1.2.3; [[ $f == v[0-9]* ]]; echo "[[ == 走 glob rc=$?"'
runc 'f=v1.2.3; [ $f = v[0-9]* ]; echo "[ ] 里同样的写法 rc=$?（右边是字面量，而且还要先做文件名展开）"'
runc 'v=1; [[ -v v ]]; echo "[[ -v v ]]（v 已设）rc=$?"'
runc 'unset -v never_set_03; [[ -v never_set_03 ]]; echo "[[ -v 未设的 ]] rc=$?"'
runc 'unset -v never_set_03; [ -v never_set_03 ]; echo "[ ] 里 -v rc=$?（bash 的内建 [ 支持，POSIX/dash 不支持，见第 8 节）"'
runc 'arr=(7 8); [[ -v arr[1] ]]; echo "[[ -v arr[1] ]] rc=$?"'
runc 'arr=(7 8); [[ -v arr[5] ]]; echo "[[ -v arr[5] ]] rc=$?"'

note '# ④ < 与 > 的字典序受 locale 影响，这是 [[ ]] 和 [ ] 的 \< 最大的分歧点。'
note '#    必须把判断写进**文件**，再用带 locale 的子 bash 去跑它；在命令行上给 [[ 前面贴 LC_ALL= 是无效的写法。'
cat > cmp_bB.sh <<'CMPS'
[[ b < B ]]
CMPS
cat > cmp_Za.sh <<'CMPS'
[[ Z < a ]]
CMPS
cat > cmp_sq.sh <<'CMPS'
[ Z \< a ]
CMPS
runc 'LC_ALL=C bash cmp_bB.sh; echo "[[ b < B ]] 在 LC_ALL=C 下 rc=$?"'
runc 'LC_ALL=zh_CN.UTF-8 bash cmp_bB.sh; echo "[[ b < B ]] 在中文 locale 下 rc=$?"'
runc 'LC_ALL=C bash cmp_Za.sh; echo "[[ Z < a ]] 在 LC_ALL=C 下 rc=$?"'
runc 'LC_ALL=zh_CN.UTF-8 bash cmp_Za.sh; echo "[[ Z < a ]] 在中文 locale 下 rc=$?"'
runc 'LC_ALL=zh_CN.UTF-8 bash cmp_sq.sh; echo "[ ] 里 \\< 在中文 locale 下 rc=$?（不受 locale 影响；完整矩阵见 bracket_spaces.sh 第 8 组）"'
note '# 再用 sort 印证 locale 真的改排序（第 02 章讲过展开与引号，这里是它对「比较结果」的影响）：'
runc 'printf "Z\na\n" | LC_ALL=C sort | tr "\n" " "; echo "| 这是 C 排序"'
runc 'printf "Z\na\n" | LC_ALL=zh_CN.UTF-8 sort | tr "\n" " "; echo "| 这是中文排序"'

note '# 正则的坑全部集中在「右边加了引号」这一件事上，见 regex_pitfalls.sh：'
runc 'bash regex_pitfalls.sh'

# ============================================================================
banner "7. && || ! 与 if / case（and_or_trap.sh + case_dispatch.sh）"
# ============================================================================
note '# if 后面跟的是**任意命令**，不是「布尔表达式」；case 的模式是 glob 不是正则；'
note '# A && B || C 不是 if-else；! 会让 set -e 的保护失效；三种终止符 ;; ;& ;;& 走法不同。'
runc 'bash and_or_trap.sh'
runc 'bash case_dispatch.sh'

# ============================================================================
banner "8. test / [ / [[ 三者关系；什么时候只能用 [ ]"
# ============================================================================
note '# 本机的 /bin/sh 是谁，直接决定 #!/bin/sh 脚本里能用哪些判断：'
runc 'readlink -f /bin/sh; stat -c "%F %n" /bin/sh'
note '# ① 最容易被忽略的一条：`[[` 在 dash 里是「命令找不到」，但只要它站在 if 的条件位，'
note '#    整个 if 语句的退出码照样是 0 —— 脚本静默走 else，看起来「跑了，没报错」。'
runc "sh -c '[[ 1 == 1 ]]'"
note '# ↑ 上面这条的 [退出码 127] 是「命令找不到」；可一旦放进 if 就变成下面这样：'
runc "sh -c 'if [[ 1 == 1 ]]; then echo 走了then; else echo 走了else; fi'"
runc "bash -c 'if [[ 1 == 1 ]]; then echo 走了then; else echo 走了else; fi'"
note '# 同一个文件，换解释器就换分支 —— 这是「[[ 在 sh 里不可用」最隐蔽的表现形式。'

note '# ② 两个脚本：sh_only.sh 只用 POSIX 判据；bash_only.sh 用了 bash 专属糖。'
cat > sh_only.sh <<'SHONLY'
#!/bin/sh
# sh_only.sh —— 只用 POSIX test。这条脚本在 dash、bash、bash --posix 下结论必须逐字一样。
f=$1
if [ ! -f "$f" ]; then
    echo "用法错误：$f 不是普通文件"
    exit 2
fi
if [ -s "$f" ]; then
    echo "非空普通文件"
    exit 0
else
    echo "空普通文件"
    exit 1
fi
SHONLY
cat > bash_only.sh <<'BASHONLY'
#!/usr/bin/env bash
# bash_only.sh —— 用了 [[ ]]、=~、BASH_REMATCH：只能在 bash 里读。
f=$1
[[ -f $f ]] || { echo "用法错误：$f 不是普通文件"; exit 2; }
[[ $f == *.log ]] && echo "日志后缀（glob 模式）"
[[ v9.1 =~ ^v([0-9]+) ]] && echo "版本号（正则，组1=${BASH_REMATCH[1]}）"
exit 0
BASHONLY
runc 'printf "line\n" > data.txt; : > zero.txt'
note '# 同一个 POSIX 脚本，四种解释器/调用方式，结论必须完全一致（这就是可移植判据的价值）：'
runc 'sh         sh_only.sh data.txt; echo "  sh rc=$?"'
runc 'dash       sh_only.sh data.txt; echo " dash rc=$?"'
runc 'bash       sh_only.sh data.txt; echo " bash rc=$?"'
runc 'bash --posix sh_only.sh data.txt; echo " bash --posix rc=$?"'
note '# 换成 bash 专属脚本：换成 sh 之后立刻报错，注意每种的报错原文和退出码。'
runc 'bash      bash_only.sh data.txt; echo "  bash rc=$?"'
runc 'sh        bash_only.sh data.txt; echo "    sh rc=$?"'
runc 'dash      bash_only.sh data.txt; echo "  dash rc=$?"'
note '# ③ 关键澄清：`bash --posix` 并**不会**把 [[ ]]、=~、数组关掉（本机实测）。'
note '#    所以「可移植性」不是靠加 flag 得来的，而是靠**你写的是哪个文件、由谁来读它**。'
runc 'bash --posix -c '\''[[ 1 == 1 ]]'\''; echo "bash --posix 里的 [[ ]] rc=$?"'
runc 'bash --posix -c '\''[[ v9 =~ ^v([0-9]) ]]; echo "组1=${BASH_REMATCH[1]}"'\''; echo "bash --posix 里的 =~ rc=$?"'
runc 'bash --posix -c '\''a=(1 2); echo "元素数=${#a[@]}"'\''; echo "bash --posix 里的数组 rc=$?"'
runc "dash -c '((1))'"
runc "dash -c 'a=(1 2)'"
runc "dash -c '[ -v HOME ]'"
runc "dash -c '[ 010 -eq 10 ]'"
note '# ↑ 最后一条：dash 的 test 也做十进制解释（010 当成 10），bash 的内建 test 也一样 ——'
note '#   「前导零 = 八进制」只存在于 (( )) 和 $(( )) 里（见 bracket_spaces.sh 第 6 组）。'
note '# ④ 一句话记住三者：test 是**程序**，[ 是它的**同名别名**（空格因此成为语法），'
note '#    [[ 是 bash 的**语法**。前两个受拆词和文件名展开摆布，第三个不受。'

# ============================================================================
banner "9. 判据写法取舍：同一件事的几种写法，行为差别是实测出来的"
# ============================================================================
note '# ① 嵌套括号：[ ] 里括号必须转义成 \( \)，[[ ]] 里直接写；报错的时候两种写法后果不同。'
runc 'a=1; b=99; if [ \( "$a" = 1 -o "$b" = 99 \) ]; then echo "1a"; else echo "1b"; fi'
runc 'a=1; b=99; if [ ( "$a" = 1 -o "$b" = 99 ) ]; then echo "2a"; else echo "2b"; fi'
runc 'a=1; b=99; if [[ ( $a == 1 || $b == 99 ) ]]; then echo "3a"; else echo "3b"; fi'
runc 'a=1; b=99; if [[ $a == 1 ]] || [[ $b == 99 ]]; then echo "4a"; else echo "4b"; fi'
note '# ↑ 第 2 条是**当场解析失败**（整个 if 语句没执行完），不是「判据为假」；'
note '#   第 3、4 条才是可读又安全的写法。差别细节见 bracket_spaces.sh 第 9、10 两组。'

note '# ② `if ! cmd` 与 `if cmd; then … else …`：真值表相同，和 set -e 的配合完全不同。'
runc 'printf "%s\n" "set -e" "worker() { if ! false; then echo 走了then; fi; echo 函数第3步还在; }" "worker" "echo 脚本还在" > bang.sh'
runc 'bash bang.sh; echo "  带 ! 的这版 rc=$?"'
runc 'printf "%s\n" "set -e" "worker() { if false; then echo x; else echo 走了else; fi; echo 函数第3步还在; }" "worker" "echo 脚本还在" > nobang.sh'
runc 'bash nobang.sh; echo "  不带 ! 的这版 rc=$?"'
runc 'printf "%s\n" "set -e" "false" "echo 这行永远不执行" > plain.sh'
runc 'bash plain.sh; echo "  裸 false 在 set -e 下 rc=$?"'
note '# ↑ 只有第 3 条被 set -e 掐死。条件位（if 的判断、! 的右边、&&/|| 的左侧）天生关掉 set -e，'
note '#   所以「用 ! 还是用 else」不只是风格问题，还决定保护还在不在（详见 and_or_trap.sh 第 5 组）。'

note '# ③ 取反的三种位置：! 在 [[ ]] 外面、里面、以及 [ ] 里，实测有没有差别：'
runc 'f=nope.txt; ! [[ -e $f ]]; echo "! [[ ]] rc=$?"'
runc 'f=nope.txt; [[ ! -e $f ]]; echo "[[ ! ]] rc=$?"'
runc 'f=nope.txt; [ ! -e $f ]; echo "[ ! ]  rc=$?"'
runc 'f=""; [ ! -e $f ];   echo "f 是空串时，不加引号的那条 rc=$?"'
runc 'f=""; [ ! -e "$f" ]; echo "f 是空串时，加引号的那条   rc=$?"'
note '# ↑ 同一个语句，一个 1 一个 0：不加引号时词数变了，[ ] 走的解释规则也变了（第 3 组、第 4 组）。'

# ============================================================================
banner "10. 把本章判据串成一个真的决策脚本（capstone：decide.sh）"
# ============================================================================
note '# 40 行小工具，把 [[ ]]、=~、case、管道后的 PIPESTATUS、退出码约定全用上：'
note '#   用法：bash decide.sh [-v] <文件> [最小非空行数]'
note '#   退出码：0=达标 / 1=内容不达标 / 2=用法或文件问题（和 grep、diff 的约定同构）'
note '#   故意不加 set -e —— 让退出码自己说话（第 9 节刚证明 set -e 和条件位互相拆台）。'
cat > decide.sh <<'DECIDE'
#!/usr/bin/env bash
# decide.sh —— 「决策层」端到端示例。这个文件在演什么：
#   一次运行里连用五种判据：参数个数（[ $# ]）、选项分派（case）、文件类型（[[ -f/-s ]]）、
#   格式校验（[[ =~ ]]）、管道后的分段退出码（PIPESTATUS）；再把结果翻译成约定退出码。
set -u

usage() { printf '用法: %s [-v] <文件> [最小非空行数]\n' "$0"; }

verbose=0
# case 负责选项分派：模式是 glob 不是正则；;; 结束当前分支
while [ $# -gt 0 ]; do
    case "$1" in
        -v) verbose=1; shift ;;
        -h|--help) usage; exit 2 ;;
        -*) printf '未知选项: %s\n' "$1" >&2; usage >&2; exit 2 ;;
        *) break ;;
    esac
done

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    usage >&2
    exit 2                        # 用法错误 → 2（与 grep/diff 同构）
fi
target=$1
min=${2-1}

# 「门槛必须是纯数字」—— 这是 [[ =~ ]] 的活，注意右边不能加引号（第 6 节实测过）
if ! [[ $min =~ ^[0-9]+$ ]]; then
    printf '门槛必须是纯数字，收到的是 [%s]\n' "$min" >&2
    exit 2
fi

# 文件名不加引号也不怕空格：这是 [[ ]] 的地盘
if [[ ! -f $target ]]; then
    printf '文件不存在或不是普通文件: [%s]\n' "$target" >&2
    exit 2
fi

# 数非空行。grep -c 一次给齐「数字」和「判据」：它打印 0 时退出码是 1（第 2 节实测过）
lines=$(grep -c '[^[:space:]]' "$target"); grep_rc=$?
if [ "$grep_rc" -gt 1 ]; then     # grep 只有 0/1/2 三种码（第 2 节实测）
    printf '读文件失败（grep rc=%s）\n' "$grep_rc" >&2
    exit 2
fi

# 同一件事改用管道做：这时 $? 只剩 wc 的码，grep 的码只能从 PIPESTATUS 里拿
if [ "$verbose" -eq 1 ]; then
    printf '管道版计数：'
    grep '[^[:space:]]' "$target" | wc -l
    ps=("${PIPESTATUS[@]}")       # 必须紧贴管道取；插一条别的就没了（第 3 节实测过）
    printf 'PIPESTATUS=[%s]  ps[0]=%s(grep) ps[1]=%s(wc)\n' "${ps[*]}" "${ps[0]}" "${ps[1]}"
fi

if [ "$lines" -ge "$min" ]; then
    printf 'PASS %s：%s 行 >= 门槛 %s\n' "$target" "$lines" "$min"
    exit 0
fi
printf 'FAIL %s：%s 行 < 门槛 %s\n' "$target" "$lines" "$min"
exit 1
DECIDE
runc 'printf "a\nb\nc\n" > ok.log; : > zero.log; printf "x\n\n" > "name with space.log"'
runc 'bash decide.sh ok.log 2; echo "  达标           → rc=$?"'
runc 'bash decide.sh ok.log 9; echo "  门槛调到 9     → rc=$?"'
runc 'bash decide.sh zero.log 1; echo "  空文件         → rc=$?"'
runc 'bash decide.sh nosuchfile 1; echo "  文件不存在     → rc=$?"'
runc 'bash decide.sh ok.log abc; echo "  门槛不是数字   → rc=$?"'
runc 'bash decide.sh; echo "  没给参数       → rc=$?"'
runc 'bash decide.sh ok.log 1 2 3; echo "  参数过多       → rc=$?"'
runc 'bash decide.sh -x ok.log; echo "  未知选项       → rc=$?"'
runc 'bash decide.sh -v "name with space.log"; echo "  带空格的文件名+-v → rc=$?"'
note '# ↑ 这九条里没有一条是「读代码就能猜对」的：第 2 条门槛 9 而文件只有 3 行 → 1（内容不达标），'
note '#   第 3 条空文件 → grep -c 打印 0 且 rc=1，也是 1；两者在 $? 上完全同值，但原因不同。'
note '#   第 9 条告诉我们 -v 必须写在文件参数**之前**才会被当成选项（分派循环遇到第一个非 - 参数就 break）。'
printf '\n[全部跑完] 上面每一段都是真实执行；README.md 引用的输出全部出自这次运行（同一份捕获）。\n'
exit 0
