#!/usr/bin/env bash
# CHECK: RUN=非复现 理由：第 5、9 格印的是 strace 带 -f 的真实测量，总行数与 execve 被拆成 unfinished/resumed 的次数由调度决定（那两格要教的正是这件事），两遍不可能逐字相同
#   rc、残留、顶格报错、隐私、GUARD 五项照旧判，两遍真正差在哪几行由工具打出来给人读。
# ============================================================================
# tracewho.sh —— 名字被谁接走（type / command / enable -n）与 strace 看得见真身
#
#   demo.sh 里这样调：bash tracewho.sh
#   安全：只在当前目录建 tw/（被演示的小脚本、符号链接、strace 日志），跑完 trap 整目录删掉。
#         strace 只追本脚本自己用 bash -c 起的那几个进程，绝不 -p 挂别人的进程；不联网。
# ★ 这一支要还的两笔账：
#   ① 「ls 被我 alias 了怎么办」——查找顺序是 alias→函数→内建→文件，三种绕法各压不同的层
#      （\ 只压 alias、command 连函数一起压、builtin 只认内建那一层），并钉住「脚本里 alias 默认不生效」；
#      enable -n 关内建要用「外部没有同名命令」的名字才看得出差别（cd 可以，echo 不行）。
#   ② strace 能不能当证据 —— -f 决定看不看得见子进程，qualifier 决定两个数字可不可比，
#      -yy 才让符号链接的目标现形；而带 -f 的日志连「逐行顺序」都不稳定，只能断言计数。
# ★ 第 4 格说明 caller / BASH_SOURCE：只有落成文件的名字才会出现在调试输出里，bash -c 起的没有。
# ============================================================================

rm -rf tw
mkdir -p tw
trap 'rm -rf tw' EXIT

# 注意最前面那条：-yy 会把「当前工作目录」原样打在 AT_FDCWD<...> 里，而这里的工作目录就是 demo 的沙箱，
# 沙箱名是 mktemp 随机后缀 —— 上一章定的规矩是随机名不上屏，所以连它一起折成 <沙箱>。
fold() { sed -E "s#$PWD#<沙箱>#g; s#$HOME#<家目录>#g; s#\\b$USER\\b#<用户>#g; s#$(hostname)#<主机名>#g"; }
# msk：把 strace 里每跑必变的东西换成占位 —— 行首 pid、内存地址、返回值数字、信号结构里的 si_pid、env 条数
msk() { sed -E 's/^[0-9]+ /<pid> /; s/0x[0-9a-f]+/<地址>/g; s/si_pid=[0-9]+/si_pid=<pid>/g; s/\) = [0-9]+/) = <结果>/; s/[0-9]+ vars/<n> vars/'; }

echo "=== 1. 一个名字被谁接走：type -a 的清单顺序就是查找顺序 ==="
printf '  type -a echo →\n'
type -a echo 2>&1 | fold | sed -E 's/^/      /'
printf '  type -t echo=[%s] ｜ command -v echo=[%s] ｜ which echo=[%s]\n' \
    "$(type -t echo)" "$(command -v echo)" "$(which echo 2>&1 | fold)"
printf '  四个问法答的不是一件事：type -a 给全部候选并排顺序 ｜ type -t 只给「谁赢了」那一类 ｜ command -v 给 shell 自己要用的名字 ｜ which 只查 PATH（所以它永远看不到内建）\n'
cat > tw/cover.sh <<'EOF'
alias ls='echo 我是alias'
ls() { echo 我是函数; }
printf '  脚本里 alias 与函数同名，type -a ls →\n'
type -a ls 2>&1 | sed -E 's/^/      /'
printf '  裸 ls 打出来=[%s] ｜ type -t ls=[%s]\n' "$(ls)" "$(type -t ls)"
printf '  shopt expand_aliases 现在是=[%s]\n' "$(shopt expand_aliases | awk '{print $2}')"
shopt -s expand_aliases
printf '  开了 expand_aliases 之后：type -t ls=[%s] ｜ 裸 ls=[%s]\n' "$(type -t ls)" "$(ls)"
EOF
bash tw/cover.sh
printf '  → 前四行看不到 alias：脚本里 expand_aliases 默认关着，alias 定义了就等于没说；\n'
printf '    真正接走 ls 的是函数（type -a 第一行就是它）。最后两行才是「开了之后 alias 又赢过函数」。\n'
printf '    交互 shell 默认开着这个 shopt —— 这就是「同样的命令我手敲能跑、写成脚本就不跑」的常见出处。\n'

echo
echo "=== 2. 三种绕法各压掉哪一层：\\ 只压 alias ／ command 连函数一起压 ／ builtin 只认内建 ==="
cat > tw/bypass.sh <<'EOF'
cd tw || exit 1
ls() { echo 我是函数; }
cd() { echo 我是函数cd; }
alias ls='echo 我是alias'
alias cd='echo 我是alias的cd'
shopt -s expand_aliases
printf '  裸 ls        =[%s]（alias 这一层赢了）\n' "$(ls)"
printf '  反斜杠 \\ls    =[%s]（只压掉 alias，函数还在下面等着接）\n' "$(\ls)"
printf '  command ls   =[%s]（alias 和函数都跳过，落到真 ls 的第一个条目）\n' "$(command ls | head -1)"
printf '  builtin ls   →[%s]\n' "$(builtin ls 2>&1 | head -1)"
printf '  行数对照：反斜杠=%s（函数只打一行）｜ command=%s ｜ /usr/bin/ls 自己数=%s\n' \
    "$(\ls | wc -l)" "$(command ls | wc -l)" "$(/usr/bin/ls | wc -l)"
printf '  command -v ls=[%s]（有 alias 时它把 alias 定义原样打出来，不是路径）\n' "$(command -v ls)"
printf '  换个有内建副本的名字 cd：裸 cd=[%s] ｜ \\cd=[%s] ｜ command cd 后 pwd=[%s] ｜ builtin cd 后 pwd=[%s]\n' \
    "$(cd /tmp)" "$(\cd /tmp)" "$(command cd /; pwd)" "$(builtin cd /; pwd)"
alias q='echo 我是q'
printf '  换成外部根本没有同名命令的名字 q：裸 q=[%s] ｜ command q →[%s] ｜ 反斜杠 \\q →[%s]\n' \
    "$(q)" "$(command q 2>&1 | head -1)" "$(\q 2>&1 | head -1)"
EOF
bash tw/bypass.sh
printf '  ★ 附带发现（真踩过的坑）：alias 活着且 expand_aliases 开着时，同一脚本里再写同名函数定义会当场炸：\n'
cat > tw/poison.sh <<'EOF'
alias ls='echo 我是alias'
shopt -s expand_aliases
ls() { echo 我是函数; }
printf '  这行还跑得到吗=[%s]\n' "$(ls)"
EOF
bash tw/poison.sh 2>&1 | fold | sed -E 's/^/      /'
printf '  → 把上面几行按层排好就是查找顺序：alias → 函数 → 内建 → PATH 里的文件。三种绕法压的层不一样：\n'
printf '    \\ 只压 alias 一层，所以 \\ls 是被函数接走的，别把它当成「真正的 ls」；\n'
printf '    command 压掉 alias 和函数两层，然后内建优先、再外部 —— command ls 掉到文件，command cd 掉到内建；\n'
printf '    builtin 只在「内建」那一层找，ls 不是内建所以当场报错；想跳过内建这一层只能 enable -n（下一格）。\n'
printf '    最后那个 q 是反面教材：alias 指向一个 PATH 里根本没有的名字，裸跑能出「我是q」，\n'
printf '    一旦用 command 或 \\ 把 alias 压掉，唯一能接住它的东西就没了 → 未找到命令（rc=127）。\n'
printf '    所以排查时别只盯「绕过了没」，要问「绕过之后掉到了哪一层」。\n'
printf '    顺带记一笔：builtin 那行的报错把行号写成「第 N 行：」，q 那行写成「行 N:」—— 同一个 bash 两套措辞，\n'
printf '    所以别拿字符串去匹配报错定位行号，判据要挑与语言无关的东西（rc、命中次数）。\n'

echo
echo "=== 3. enable -n 的判别式：要用外部没有同名命令的那个名字 ==="
cat > tw/dis.sh <<'EOF'
printf '  关之前：type -t cd=[%s] ｜ cd . 的 rc=%s\n' "$(type -t cd)" "$(cd .; echo $?)"
enable -n cd
printf '  enable -n cd 之后（同一层 shell）：type -t cd=[%s]\n' "$(type -t cd)"
printf '  再跑一句 cd . → 报错=[%s] ｜ rc=%s\n' "$(cd . 2>&1)" "$(cd . 2>/dev/null; echo $?)"
enable cd
printf '  恢复 enable cd 之后：type -t cd=[%s] ｜ cd . 的 rc=%s\n' "$(type -t cd)" "$(cd .; echo $?)"
printf '  /bin/echo 在不在=%s\n' "$(ls /bin/echo 2>/dev/null)"
enable -n echo
printf '  对照 echo：enable -n echo 之后 type -t echo=[%s] ｜ 跑 echo hi=[%s]\n' "$(type -t echo)" "$(echo hi)"
printf '    掉到了哪：type -a echo 的后两行=[%s]\n' "$(type -a echo 2>&1 | tail -2 | tr '\n' '/')"
enable echo
EOF
bash tw/dis.sh
printf '  哪些关得掉、哪些关不掉（注意：enable -n 必须写在脚本层，放进 $( ) 里就只在子 shell 生效）：\n'
cat > tw/cant.sh <<'EOF'
for n in : . set eval exit times trap printf; do
    enable -n "$n" 2>tw/err.txt
    printf '    enable -n %-6s 报错=[%s] ｜ 关后 type -t=[%s]\n' "$n" "$(cat tw/err.txt)" "$(type -t "$n")"
    enable "$n"
done
rm -f tw/err.txt
EOF
bash tw/cant.sh
printf '  关掉 : 之后，脚本里当占位用的 : 会怎样：\n'
cat > tw/colon.sh <<'EOF'
enable -n :
printf '    跑一句 : x → 报错=[%s] rc=%s\n' "$(: x 2>&1)" "$(cd .; : x 2>/dev/null; echo $?)"
enable :
printf '    恢复之后再跑 → rc=%s ｜ 内建总数：enable -a 行数=%s ｜ compgen -A builtin 行数=%s\n' \
    "$(: x; echo $?)" "$(enable -a | wc -l)" "$(compgen -A builtin | wc -l)"
EOF
bash tw/colon.sh
printf '  → 判别式很硬：关内建必须挑「PATH 里没有同名命令」的名字才看得出效果（cd 变成 command not found）；\n'
printf '    echo/printf 这类外面有副本的，关掉之后只是掉到 file 那一层，屏上照样出 hi —— 拿它当「关掉了」的证据会读反。\n'
printf '    最后一格最吓人：bash 不拦你，连 : . set eval exit times trap 这些 POSIX 特判内建也关得掉，\n'
printf '    而 : 一关，脚本里所有当占位用的 : 当场变成 command not found —— 关完记得 enable 回来。\n'

echo
echo "=== 4. 落成文件才有名字：caller / BASH_SOURCE / FUNCNAME ==="
cat > tw/lib.sh <<'EOF'
lib_report() {
    printf '    caller        =[%s]\n' "$(caller)"
    printf '    FUNCNAME 前三 =[%s | %s | %s]\n' "${FUNCNAME[0]}" "${FUNCNAME[1]}" "${FUNCNAME[2]}"
    printf '    BASH_SOURCE[0]=[%s] ｜ BASH_LINENO[0]=[%s]\n' "${BASH_SOURCE[0]}" "${BASH_LINENO[0]}"
    printf '    本行 LINENO   =[%s]\n' "$LINENO"
}
EOF
cat > tw/main.sh <<'EOF'
main() { lib_report; }
b() { main; }
c() { b; }
. ./tw/lib.sh
c
printf '  顶层：BASH_SOURCE[0]=[%s] ｜ $0=[%s] ｜ FUNCNAME[0]=[%s]\n' "${BASH_SOURCE[0]}" "$0" "${FUNCNAME[0]-（空）}"
EOF
bash tw/main.sh
printf '  同一句 caller 在 bash -c 里（没落文件）=[%s]\n' "$(bash -c 'f(){ caller; }; f')"
printf '  → 读数各指一件事：caller 给「当前函数被谁、在第几行叫起」，BASH_SOURCE[0] 给「当前函数定义在哪个文件」，\n'
printf '    LINENO 在函数里指函数体内那一行，所以要看调用点得取上一层的 BASH_LINENO[i]；\n'
printf '    最后一行是 1 NULL：没落成文件的代码有行号但没有文件名 —— 排障时先把库写成文件，正是为了补上这一格。\n'

echo
echo "=== 5. strace 在场性与红线：只追自己起的进程 ==="
printf '  command -v strace=[%s] ｜ 版本首行=[%s]\n' "$(command -v strace)" "$(strace -V 2>&1 | head -1)"
printf '  -h 里关于 -f 与 -p 的原文（各取一行）：%s\n' "$(strace -h 2>&1 | grep -E '^\s+-(f|p)[, ]' | head -2 | tr -s ' ' | tr '\n' '/')"
printf '  ★ 红线：-p 是「挂到别人进程上」的能力，本机演示一次都不用它；下面每条 strace 都直接执行自己起的 bash -c。\n'
strace -e trace=process -o tw/nolf.log bash -c 'echo abc | tr a b' >/dev/null 2>&1
strace -f -e trace=process -o tw/wf.log bash -c 'echo abc | tr a b' >/dev/null 2>&1
printf '  同一条命令、同一口径（-e trace=process）带不带 -f 的差别：\n'
printf '    不带 -f：总行=%s ｜ clone 调用行=%s ｜ execve 调用行=%s ｜ 带 pid 前缀的行=%s\n' \
    "$(wc -l < tw/nolf.log)" "$(grep -c 'clone(' tw/nolf.log)" "$(grep -c 'execve(' tw/nolf.log)" \
    "$(grep -cE '^[0-9]+ ' tw/nolf.log)"
printf '    带   -f：总行=%s ｜ clone 调用行=%s ｜ execve 调用行=%s ｜ 带 pid 前缀的行=%s\n' \
    "$(wc -l < tw/wf.log)" "$(grep -c 'clone(' tw/wf.log)" "$(grep -c 'execve(' tw/wf.log)" \
    "$(grep -cE '^[0-9]+ ' tw/wf.log)"
printf '    看得见管道右半截 /usr/bin/tr 吗：不带 -f=%s ｜ 带 -f=%s（0 = 整块消失）\n' \
    "$(grep -c '/usr/bin/tr' tw/nolf.log)" "$(grep -c '/usr/bin/tr' tw/wf.log)"
printf '    同一份带 -f 的日志，execve 有三种数法：数单词 execve=%s ｜ 只数调用行 execve( =%s ｜ 数 <... execve resumed> 拆行=%s\n' \
    "$(grep -c execve tw/wf.log)" "$(grep -c 'execve(' tw/wf.log)" "$(grep -c '<\.\.\. execve resumed>' tw/wf.log)"
wf_word=$(grep -c execve tw/wf.log); wf_call=$(grep -c 'execve(' tw/wf.log); wf_res=$(grep -c '<\.\.\. execve resumed>' tw/wf.log)
printf '    ① 口径对账：数单词 - 数调用行 = %s，拆行数 = %s，两者相等吗=%s（不相等说明还有别的行里带 execve 这个词）\n' \
    "$((wf_word - wf_call))" "$wf_res" "$([ "$((wf_word - wf_call))" -eq "$wf_res" ] && echo 相等 || echo 不相等)"
printf '  → -f 不是「多打几行」，是「多追几个进程」：管道右半截是 fork 出来的，不带 -f 就整块看不见；\n'
printf '    clone 两份都是 2（fork 就那两次），差别全在跟不跟进去 —— 调用行 execve( 从 1 变 2，多出来那次就是 /usr/bin/tr。\n'
printf '    另外记住「数单词」这个坑：带 -f 时一个调用可能被拆成「先 <unfinished ...>、后 <... 名字 resumed>」两行，\n'
printf '    两行里都出现调用名，所以按单词数会把一次调用数成两次 —— 上面那行对账就是在量这个差，跨趟差几行都正常。\n'
printf '    要统计调用次数只认「调用行」口径（带括号的那个）；总行数会跟着拆行一起抖，第 9 格拿逐行 diff 实测。\n'

echo
echo "=== 6. qualifier 决定可比性：同一命令换 -e 就是在换尺 ==="
strace -e trace=file -o tw/file1.log bash -c 'cat /etc/hostname > /dev/null' >/dev/null 2>&1
strace -e trace=process -o tw/proc1.log bash -c 'cat /etc/hostname > /dev/null' >/dev/null 2>&1
strace -o tw/all1.log bash -c 'cat /etc/hostname > /dev/null' >/dev/null 2>&1
printf '  同一命令三种口径：-e trace=file 行=%s ｜ -e trace=process 行=%s ｜ 不加限定 行=%s\n' \
    "$(wc -l < tw/file1.log)" "$(wc -l < tw/proc1.log)" "$(wc -l < tw/all1.log)"
printf '  可用的 qualifier 各试一个（只看 rc）：file=%s process=%s desc=%s network=%s\n' \
    "$(strace -e trace=file -o /dev/null true >/dev/null 2>&1; echo $?)" \
    "$(strace -e trace=process -o /dev/null true >/dev/null 2>&1; echo $?)" \
    "$(strace -e trace=desc -o /dev/null true >/dev/null 2>&1; echo $?)" \
    "$(strace -e trace=network -o /dev/null true >/dev/null 2>&1; echo $?)"
printf '  照抄别处的 -e trace=timer（本机不存在的调用类）：[%s]\n' "$(strace -e trace=timer true 2>&1 | head -1)"
printf '  → 屏上那三个行数差着数量级 —— 它们比的是「口径」，不是程序；\n'
printf '    想比两次运行的差别，只能固定 -e 只变 -f（第 5 格就是这么比的）；\n'
printf '    不存在的调用类是当场拒绝（打印 invalid system call），不会给你一份少掉的文件日志。\n'

echo
echo "=== 7. 符号链接的真身：不带 -yy 只印你递过去的名字 ==="
printf '  原内容\n' > tw/victim.txt
ln -sfn victim.txt tw/link
strace -f -e trace=file -o tw/sf.log bash -c 'echo 追加 > tw/link' >/dev/null 2>&1
printf '  不带 -yy：日志里出现 victim.txt 的行数=%s ｜ 出现 "tw/link" 的行数=%s\n' \
    "$(grep -c 'victim\.txt' tw/sf.log)" "$(grep -c '"tw/link"' tw/sf.log)"
strace -f -yy -e trace=file -o tw/sy.log bash -c 'echo 追加 > tw/link' >/dev/null 2>&1
printf '  带   -yy：出现 victim.txt 的行数=%s ｜ 出现 link 字样的行数=%s\n' \
    "$(grep -c 'victim\.txt' tw/sy.log)" "$(grep -c 'link' tw/sy.log)"
printf '  那一行的原文（不带 -yy；pid、地址、返回值与沙箱路径都换成占位）：\n'
grep openat tw/sf.log | grep 'link' | head -1 | msk | fold | sed -E 's/^/      /'
printf '  带 -yy 的同一行（尖括号里就是内核最后落到的地方）：\n'
grep openat tw/sy.log | grep 'victim' | head -1 | msk | fold | sed -E 's/^/      /'
printf '  被写坏的文件现在的内容（行数=%s，最后一行=[%s]）\n' "$(wc -l < tw/victim.txt)" "$(tail -1 tw/victim.txt)"
printf '  → openat 打的是「你递给内核的名字」，不是「最后落到哪」：符号链接那一跳是内核做的，默认不替你显形；\n'
printf '    要追「到底写坏了哪个文件」得加 -yy（尖括号里就是解析后的真路径），否则你会盯着 link 的名字找半天。\n'
printf '    读这两行还要认一件事：同一个 AT_FDCWD 参数，不带 -yy 时它只是个常数名字，带了才带出目录；\n'
printf '    屏上那两处 <沙箱> 就是 demo 自己的临时目录被折掉的样子 —— 原文里它是绝对路径，直接存档就泄了。\n'

echo
echo "=== 8. strace 日志里的隐私与易变值：不折就不能上屏 ==="
strace -f -e trace=file -o tw/home.log bash -c 'cat "$HOME/没有这个文件" > /dev/null' >/dev/null 2>&1
printf '  含真实家目录的行数：这份日志=%s 行（>0 —— 所以只能报计数，原文必须先折）\n' "$(grep -c "$HOME" tw/home.log)"
printf '  折过之后还剩=%s 行 ｜ 第 7 格那两条原文就是折过之后才上屏的\n' "$(fold < tw/home.log | grep -c "$HOME")"
printf '  另一份只追 process 的日志里：家目录=%s ｜ 用户名=%s ｜ 主机名=%s（口径不同，泄漏面也不同）\n' \
    "$(grep -c "$HOME" tw/wf.log)" "$(grep -c "$USER" tw/wf.log)" "$(grep -c "$(hostname)" tw/wf.log)"
printf '  易变值有哪几类（拿带 -f 那份数）：pid 种类=%s ｜ 带 0x 地址的行=%s ｜ 返回值是数字的行=%s\n' \
    "$(awk '{print $1}' tw/wf.log | sort -u | wc -l)" "$(grep -c '0x' tw/wf.log)" "$(grep -cE '\) = [0-9]+' tw/wf.log)"
printf '  → 三条规矩：家目录/用户名/主机名要折；pid、地址、si_pid 要占位；调用名与参数骨架可以留。\n'
printf '    注意 execve 那行会带整个环境变量表的指针与条数，加 -y 或 -xx 更会把值漏出来 ——\n'
printf '    想把 strace 存档，先跑第 9 格那套「抹身份保数量」的管道，再谈落盘。\n'

echo
echo "=== 9. 抹干净之后能对拍吗：单进程逐行可拍，多进程只能断言计数 ==="
# pairs 前缀：把同前缀的三份文件两两 diff，报「完全对得上的组合数 / 总组合数」
pairs() {
    local z=0 n=0 a b
    for a in 1 2 3; do
        for b in 1 2 3; do
            if [ "$a" -lt "$b" ]; then
                n=$((n+1))
                if [ "$(diff "$1$a.txt" "$1$b.txt" | grep -c '^[<>]')" -eq 0 ]; then z=$((z+1)); fi
            fi
        done
    done
    printf '%s/%s' "$z" "$n"
}
for i in 1 2 3; do
    strace -e trace=process -o "tw/n$i.log" bash -c 'echo abc | tr a b' >/dev/null 2>&1
    msk < "tw/n$i.log" | fold > "tw/full$i.txt"
    sed -E 's/^[0-9]+ /<pid> /; s/0x[0-9a-f]+/<地址>/g; s/\) = [0-9]+/) = <结果>/; s/[0-9]+ vars/<n> vars/' \
        "tw/n$i.log" | fold > "tw/nos$i.txt"
    strace -f -e trace=process -o "tw/f$i.log" bash -c 'echo abc | tr a b' >/dev/null 2>&1
    msk < "tw/f$i.log" | fold > "tw/many$i.txt"
done
sed -E '1s/^/X/' tw/full1.txt > tw/tamper.txt
printf '  ① 先确认「比对」本身能响：把第一份行首插个 X 再与第二份 diff，命中=%s 行（期望非 0）\n' \
    "$(diff tw/tamper.txt tw/full2.txt | grep -c '^[<>]')"
printf '  ② 少抹一步（信号结构里的 si_pid 留原值）：三份两两完全对得上=%s（期望 0/3）\n' "$(pairs tw/nos)"
printf '  ③ 把 si_pid 也抹掉：三份两两完全对得上=%s（期望 3/3）｜ 三份各 %s/%s/%s 行\n' \
    "$(pairs tw/full)" "$(wc -l < tw/full1.txt)" "$(wc -l < tw/full2.txt)" "$(wc -l < tw/full3.txt)"
many_pairs=$(pairs tw/many)
printf '  ④ 同一套抹法换成带 -f 的三份：两两完全对得上=%s ｜ 三份各 %s/%s/%s 行\n' \
    "$many_pairs" "$(wc -l < tw/many1.txt)" "$(wc -l < tw/many2.txt)" "$(wc -l < tw/many3.txt)"
case "${many_pairs%%/*}" in
    0) printf '     → 一趟都对不上：多进程日志的逐行顺序由调度决定，天生不可复现。\n' ;;
    3) printf '     → 这趟三笔碰巧全对上了 —— 这是运气，不是保证；判据要落在 ⑤⑥ 的计数口径上。\n' ;;
    *) printf '     → 有的对得上、有的对不上：同一命令同一抹法忽对忽错，逐行 diff 就不能当判据。\n' ;;
esac
printf '  ⑤ 带 -f 时能跨趟断言的只有调用行计数：clone(=%s/%s/%s ｜ execve(=%s/%s/%s\n' \
    "$(grep -c 'clone(' tw/f1.log)" "$(grep -c 'clone(' tw/f2.log)" "$(grep -c 'clone(' tw/f3.log)" \
    "$(grep -c 'execve(' tw/f1.log)" "$(grep -c 'execve(' tw/f2.log)" "$(grep -c 'execve(' tw/f3.log)"
printf '     不保证跨趟相同的口径（屏上只是这一趟的值）：数单词 execve=%s/%s/%s ｜ 总行=%s/%s/%s\n' \
    "$(grep -c execve tw/f1.log)" "$(grep -c execve tw/f2.log)" "$(grep -c execve tw/f3.log)" \
    "$(wc -l < tw/f1.log)" "$(wc -l < tw/f2.log)" "$(wc -l < tw/f3.log)"
ci=是
for i in 1 2 3; do
    if [ "$(grep -c 'execve(' "tw/f$i.log")" != 2 ] || [ "$(grep -c '/usr/bin/tr' "tw/f$i.log")" != 1 ]; then ci=否; fi
done
printf '  ⑥ 把结论写成能进 CI 的断言（每份都要满足 execve(=2 且 /usr/bin/tr 出现 1 次）：三份全部通过=%s\n' "$ci"
printf '  → 三条规矩落地：单进程日志抹干净就能逐行对拍（③，三趟全对上）；少抹一步 si_pid 就会全对不上（②）；\n'
printf '    带 -f 的日志每一行来自不同进程，谁先落笔由调度决定 —— 所以 ④ 那个数不保证是几，两趟跑出来\n'
printf '    可能就是两个值（脚本自己印的 Δ 就是这个意思），它不能当判据；能进 CI 的只有计数（⑤⑥）。\n'
printf '    ⑥ 那行就是把结论写成断言后的运行结果 —— 本章对 strace 的最终用法结论到此为止。\n'

echo
echo "=== 10. strace -c 与一行怎么读：末两行的形状 + 五段结构 ==="
printf '  -c 汇总的末两行（每串数字折成一个 #，连续空格压成一个）：\n'
strace -c true 2>&1 | tail -2 | sed -E 's/[0-9]+/#/g' | tr -s ' ' | sed -E 's/^/      /'
printf '  一行原文（占位过）：\n'
grep -m1 execve tw/f1.log | msk | fold | sed -E 's/^/      /'
printf '  → 一行 strace 五段：pid ｜ 调用名(参数) ｜ 参数里的路径与标志位 ｜ ) = 返回值 ｜ 失败时是 = -1 ENOENT 这种形状；\n'
printf '    -c 只回答「每个调用名跑了几次、花了多少时间」，它把顺序丢了 —— 想知道「先开谁后开谁」必须看逐行输出。\n'

echo
printf '[tracewho.sh 自查] 本脚本格数=%s ｜ tw/ 里现有文件数=%s（跑完由 trap 整目录删除）\n' \
    "$(grep -c '^echo .=== ' "$0")" "$(find tw -type f | wc -l)"
