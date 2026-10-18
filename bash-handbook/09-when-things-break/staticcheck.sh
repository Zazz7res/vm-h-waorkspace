#!/usr/bin/env bash
# ============================================================================
# staticcheck.sh —— 不跑起来也能查出来的那三层：bash -n、-o noexec、-u，以及报错怎么读
#
#   demo.sh 里这样调：bash staticcheck.sh
#   安全：只在当前目录建 sc/（几份被演示的小脚本 + 两遍对拍的对照文件），跑完 trap 整目录删掉。
#         全部用相对文件名跑，不碰 ~、不写绝对 /tmp、不联网、不 sleep、不起后台。
# ★ 这一支要还的账：很多人把「静态检查」当成一把尺，实测它至少是三把：
#   bash -n 只看语法（少 fi 抓得到，未定义变量一个字都不报）；
#   -o noexec 连解析带展开全做但一条都不执行（所以「脚本跑完没留下垃圾」这种验证是可做的）；
#   -u 不在静态层，它得真跑到那一行才翻脸。
#   另外这机器上没有 shellcheck —— 这一格只如实报「没有」，并给出本机真能跑的替代三层。
# ★ 报错那一行是有结构的：文件: 行号: 命令: 消息；三种死法的 rc 分别 127 / 1 / 139，都能对上号。
# ============================================================================

rm -rf sc
mkdir -p sc
trap 'rm -rf sc' EXIT

fold() { sed -E "s#$HOME#<家目录>#g; s#\\b$USER\\b#<用户>#g; s#$(hostname)#<主机名>#g"; }
# 落一个小脚本：文件名 + 若干行内容（用 heredoc 之外的写法，保证内容里的引号原样落地）
put() { local f=sc/"$1"; shift; : > "$f"; while [ $# -gt 0 ]; do printf '%s\n' "$1" >> "$f"; shift; done; }
# one <用例名> <命令...>：把 stdout 与 stderr 分开标来源，并附上 rc
one() {
    local name=$1; shift
    local out err rc
    out=$("$@" 2>/dev/null | fold | tr '\n' '/')
    err=$("$@" 2>&1 >/dev/null | fold | tr '\n' '/')
    rc=$("$@" >/dev/null 2>&1; echo $?)
    printf '  %-14s rc=%-4s stdout=[%s] ｜ stderr=[%s]\n' "$name" "$rc" "$out" "$err"
}

echo "=== 1. 三层各查到哪一层：同一批「问题」在三层面前各是什么下场 ==="
put bad.sh 'if [ -f x; then' '  echo hi'
put quiet.sh 'echo "值是：$undefined_var"'
put plain.sh 'echo 正常一句话'
printf '  bad.sh 的源码（少一个 fi，第 1 行的 [ 也少一个 ]）：\n'
nl -ba sc/bad.sh | sed 's/^/    /'
one 'bash -n bad.sh' bash -n sc/bad.sh
one 'bash -n quiet.sh' bash -n sc/quiet.sh
one 'bash -n plain.sh' bash -n sc/plain.sh
printf '  同一份 quiet.sh 真跑（不穿 -u）：\n'
one 'bash quiet.sh' bash sc/quiet.sh
printf '  同一份 quiet.sh 穿 -u 再跑：\n'
one 'bash -u quiet.sh' bash -u sc/quiet.sh
printf '  → 三层分得很清楚：① -n 只报 bad.sh 那一类（少 fi，rc=2），对未定义变量完全放行；\n'
printf '    ② quiet.sh 在 -n 与真跑两层都是 rc=0、屏上一个字都不提醒（打出「值是：」后面空的）；\n'
printf '    ③ 要它翻脸得靠 -u，而 -u 是运行时才检查的：跑到那一行才死，rc=1。\n'
printf '    所以「过了 bash -n」不等于「没有 bug」，它只证明这份文本能被解析。\n'

echo
echo "=== 2. -o noexec：解析照做、语法错照报，但一条命令都不执行 ==="
put dirty.sh 'echo 应该被写出来 > sc/should-not-exist.txt' 'echo 正菜' 'exit 7'
put exp.sh 'x=$(touch sc/marker-from-subst.txt; echo 值)' 'echo 用了 $x > sc/should-not-exist2.txt'
printf '  同一份 bad.sh（少 fi）在两层面前：\n'
one 'bash -n bad' bash -n sc/bad.sh
one 'bash noexec bad' bash -o noexec sc/bad.sh
printf '  有副作用的 dirty.sh（脚本里写着 exit 7）：\n'
one 'bash noexec dirty' bash -o noexec sc/dirty.sh
printf '  noexec 跑完，sc/should-not-exist.txt 在不在=%s ｜ 命令替换里那句 touch 的 sc/marker-from-subst.txt 在不在=%s\n' \
    "$([ -e sc/should-not-exist.txt ] && echo 在 || echo 不在)" \
    "$([ -e sc/marker-from-subst.txt ] && echo 在 || echo 不在)"
printf '  noexec 再加 -x，trace 打了几行=%s（0 = 一条都不打）\n' "$(bash -o noexec -x sc/dirty.sh 2>&1 | wc -l)"
printf '  在脚本**中途**开 noexec，之后再关，看三句各自落没落：\n'
put mid.sh 'echo 第一句 > sc/A.txt' 'set -o noexec' 'echo 第二句 > sc/B.txt' 'set +o noexec' 'echo 第三句 > sc/C.txt'
one 'bash mid.sh' bash sc/mid.sh
printf '    A.txt=%s ｜ B.txt=%s ｜ C.txt=%s\n' \
    "$([ -e sc/A.txt ] && echo 在 || echo 不在)" \
    "$([ -e sc/B.txt ] && echo 在 || echo 不在)" \
    "$([ -e sc/C.txt ] && echo 在 || echo 不在)"
printf '  → 五件事分开读：① 语法错照报、rc 一样是 2 —— noexec 的解析这一步跟 -n 等价；\n'
printf '    ② 但执行一步都不做：echo、重定向、命令替换里的 touch 全没落地，脚本里那句 exit 7 也没生效（rc=0）；\n'
printf '    ③ 加 -x 也一条 trace 都不打 —— trace 属于「执行」那一步，所以「屏上安静」在这里是预期，不是脚本没问题；\n'
printf '    ④ 中途 set -o noexec 之后连 set +o noexec 自己都执行不到（C.txt 不在），这一开就回不来，只能让脚本走完；\n'
printf '    ⑤ 所以它的用法只有一条：拿它当「会不会写坏东西」的 dry-run；想验「输出对不对」它一个字都给不了。\n'

echo
echo "=== 3. 报错那一行怎么读：四段结构 + 三种死法的 rc ==="
put notfound.sh 'nosuchcmd-xyz'
put divzero.sh 'echo $((1 / 0))'
put boom.sh 'r() { r; }; r'
one '未找到命令' bash sc/notfound.sh
one '除零' bash sc/divzero.sh
one '递归爆栈' bash sc/boom.sh
printf '  拆开看第一段（未找到命令那条的 stderr 原文）：\n'
bash sc/notfound.sh 2>&1 >/dev/null | fold | sed -E 's/^/      /' | sed -E 's/^      ([^:]*): 行 ([0-9]+): ([^:]*): (.*)$/      文件=\1 ｜ 行号=\2 ｜ 谁报的=\3 ｜ 消息=\4/'
printf '  rc 的两套读法：命令自己返回的码，与「被信号杀死」的 128+信号号\n'
printf '    爆栈那条 rc=%s → 128+%s=128+SIGSEGV(11)\n' "$(bash sc/boom.sh >/dev/null 2>&1; echo $?)" 11
printf '    除零那条 rc=%s（这是 bash 自己返回的 1，不是信号）\n' "$(bash sc/divzero.sh >/dev/null 2>&1; echo $?)"
printf '    未找到命令那条 rc=%s（POSIX 约定 127=找不到命令；126=找到了但不能执行）\n' "$(bash sc/notfound.sh >/dev/null 2>&1; echo $?)"
printf '  → 报错行是「文件: 行号: 命令: 消息」四段，第一段能直接回指到编辑器里的哪一行；\n'
printf '    三种死法的 rc 各自成号：127 找不着、1 是 shell 自己的算术/条件错、139 是被 SIGSEGV 打死。\n'
printf '  ★ 注意第三行：爆栈那条 stderr 是**空的** —— 它一个字都不解释，只留下 rc=139。\n'
printf '    所以看到 13x 系列先去数信号号（128+n），别在脚本里找那句「返回 139」的代码 —— 没有那句；\n'
printf '    而这种「没有报错只有码」的死法，只能靠码发现，这就是第 08 章 trap EXIT 里打码的意义。\n'

echo
echo "=== 4. 本机没有 shellcheck：这一格只报在场情况，不给假输出 ==="
printf '  command -v shellcheck → [%s]\n' "$(command -v shellcheck || echo 本机没有)"
printf '  同一族工具挨个问一遍：%s\n' \
    "$(for t in shellcheck shfmt ltrace valgrind strace gdb; do printf '%s=%s ' "$t" "$(command -v "$t" || echo 无)"; done)"
printf '  没有它之后的替代三层（本机真跑过，就是上面三格）：%s\n' \
    "$(printf 'bash -n（语法）+ bash -o noexec（展开但不执行）+ bash -u（未定义当场死）')"
printf '  替代层能抓到什么、抓不到什么，用三份小样本各证一次：\n'
cat > sc/quote.sh <<'EOF'
a=有 空格
printf 'a=[%s]\n' "$a"
echo 后面
EOF
printf '    语法错（bad.sh 少 fi）：%s ｜ 未定义变量（quiet.sh 穿 -u）：%s\n' \
    "$(bash -n sc/bad.sh >/dev/null 2>&1; echo 抓到rc=$?)" \
    "$(bash -u sc/quiet.sh >/dev/null 2>&1; echo 抓到rc=$?)"
printf '    少引号的赋值（quote.sh 第 1 行）在 -n 那层：%s\n' \
    "$(bash -n sc/quote.sh >/dev/null 2>&1; echo 放行rc=$?)"
printf '  少引号那条 -n 放行了，但 -x 一眼能看见它被拆成两句：[%s]\n' \
    "$(bash -x sc/quote.sh 2>&1 >/dev/null | fold | tr '\n' '/')"
printf '  → 事实要说清楚：本机没有 shellcheck，本章一个 shellcheck 的输出都没有，也不许你照抄别人的输出；\n'
printf '    它抓的是「写法反模式」（少引号、无用 cat、== 与 = 混用），上面三层抓的是「会不会炸」——\n'
printf '    替代三层抓不到少引号，但 -x 能把后果现形（屏上分成 + a=有 / + 空格 两条）。装途径见 README，本章不装。\n'

echo
echo "=== 5. 「把当前设置打印出来」一族：谁打几行、谁会把隐私一起打出来 ==="
printf '  set -o 行数=%s ｜ shopt 行数=%s ｜ 裸 set 行数=%s\n' \
    "$(set -o | wc -l)" "$(shopt | wc -l)" "$(set | wc -l)"
printf '  裸 set 里除了开关还有什么：函数体命中=%s 行 ｜ 变量赋值命中=%s 行\n' \
    "$(set | grep -c ' ()')" "$(set | grep -cE '^[A-Za-z_][A-Za-z0-9_]*=')"
printf '  ★ 裸 set 含真实家目录的行数=%s（>0 —— 所以任何 set 的转储都必须先折一道再落盘）\n' \
    "$(set | grep -c "$HOME")"
printf '  折过之后还剩=%s 行（应为 0）\n' "$(set | fold | grep -c "$HOME")"
printf '  trap -p 的三种形状：\n'
cat > sc/tp.sh <<'EOF'
printf '    空表行数=%s\n' "$(trap -p | wc -l)"
trap 'x=1' DEBUG
printf '    设一个之后行数=%s 内容=[%s]\n' "$(trap -p | wc -l)" "$(trap -p | tr '\n' '/')"
trap 'x=1' EXIT
printf '    两个都有时顺序=[%s]\n' "$(trap -p | cut -d" " -f4 | tr '\n' '/')"
EOF
bash sc/tp.sh
printf '  declare -F 与 typeset -f 的分工（函数名 vs 函数体）：\n'
cat > sc/fn.sh <<'EOF'
f() { echo 体; }
printf '    declare -F f → [%s]\n' "$(declare -F f)"
printf '    typeset -f f 行数=%s ｜ 第一行=[%s]\n' "$(typeset -f f | wc -l)" "$(typeset -f f | sed -n 1p)"
printf '    compgen -A function 命中自己吗=%s\n' "$(compgen -A function | grep -c '^f$')"
printf '    compgen -A variable 行数=%s\n' "$(compgen -A variable | wc -l)"
EOF
bash sc/fn.sh
printf '  开了 extdebug 之后 declare -F 多给两列（名字/行号/文件）：\n'
cat > sc/fn2.sh <<'EOF'
shopt -s extdebug
f() { echo 体; }
printf '    declare -F f → [%s]\n' "$(declare -F f)"
printf '    不开 extdebug 的同一句 → [%s]\n' "$(bash -c 'f(){ echo 体; }; declare -F f')"
EOF
bash sc/fn2.sh
printf '  → 读法：set -o / shopt 各 27 与 59 行是「开关表」，裸 set 是「开关 + 变量 + 函数」的全量转储，\n'
printf '    所以别把裸 set 直接贴进工单或日志 —— 它带着 PWD、HOME 和所有函数的源码；\n'
printf '    trap -p 只在你真设过陷阱之后才有内容（空表 0 行），它是「这脚本有没有留清理」的最快验法；\n'
printf '    declare -F 给名字（extdebug 下多给行号与文件），typeset -f 给整段函数体 —— 想知道函数从哪跑来的用前者。\n'

echo
echo "=== 6. bash -v 与 bash -x：一个打原文，一个打展开之后（静态层的另一半） ==="
put vx.sh 'a=甲 乙' 'printf "a=[%s]\n" "$a"'
printf '  同一份两行脚本，-v 与 -x 各自打出什么（只取 stderr）：\n'
printf '    -v：[%s]\n' "$(bash -v sc/vx.sh 2>&1 >/dev/null | fold | tr '\n' '/')"
printf '    -x：[%s]\n' "$(bash -x sc/vx.sh 2>&1 >/dev/null | fold | tr '\n' '/')"
printf '  → -v 连「读到的每一行原文」都打（含 printf 那行的引号原样），-x 只打展开完要执行的那一次；\n'
printf '    排查「我写的到底是什么」用 -v（引号、注释、续行一目了然），排查「写对了但展开成什么」用 -x。\n'
printf '    两个都走 stderr，都能 2>/dev/null 折掉，也都能被 BASH_XTRACEFD 单独引走（见 ps4fd.sh 第 6 格）。\n'

echo
echo "=== 7. 这一串证据自己可复现吗：先让守卫响，再让它对上 ==="
bash -x sc/vx.sh 2>&1 | fold > sc/x1.txt
bash -x sc/vx.sh 2>&1 | fold > sc/x2.txt
sed 's/^+ a=甲$/+ A=甲/' sc/x1.txt > sc/x3.txt
printf '  ① 先看守卫能不能响：把第三份里那条赋值的首字母改成大写，与第二份 diff 命中=%s 行（期望非 0）\n' \
    "$(diff sc/x3.txt sc/x2.txt | grep -c '^[<>]')"
diff sc/x1.txt sc/x2.txt > sc/d.txt 2>&1
printf '  ② 撤掉改动：两遍 -x 的 stderr diff=%s 行（期望 0）｜ 两遍各 %s/%s 行\n' \
    "$(grep -c '^[<>]' sc/d.txt)" "$(wc -l < sc/x1.txt)" "$(wc -l < sc/x2.txt)"
printf '  ③ -n 的 rc 连跑三遍稳不稳：%s\n' \
    "$(for i in 1 2 3; do bash -n sc/bad.sh >/dev/null 2>&1; printf '%s ' "$?"; done)"
printf '  → 静态层与默认 xtrace 都不含时间戳/pid，是本章少数「整屏可对拍」的证据；\n'
printf '    守卫必须先响一次再对上，否则「diff 为空」可能只是两条命令都没输出东西（对照第 08 章的恒真守卫）。\n'

echo
printf '[staticcheck.sh 自查] 本脚本格数=%s ｜ sc/ 里现有文件数=%s（跑完由 trap 整目录删除）\n' \
    "$(grep -c '^echo .=== ' "$0")" "$(find sc -type f | wc -l)"
