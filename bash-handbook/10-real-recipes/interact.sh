#!/usr/bin/env bash
# ============================================================================
# interact.sh —— 把交互操作固化成脚本：read 在脚本里读到什么、alias 只在交互里存在、tty 怎么改变输出
#
#   demo.sh 里这样调：bash interact.sh
#   安全：只在当前目录建 ia/（被演示的小脚本、伪 tty 样本、计数器、对拍文件），跑完由 trap 整目录删掉；
#         全用相对文件名上屏（跑 bash -i 时要临时 cd 进沙箱，那条绝对路径一上屏就当场折成 <沙箱>）；
#         绝不写 ~、绝不写交互 shell 的 rc 文件、不联网、不开端口；不装包；不起后台常驻进程（全同步），
#         结尾 jobs 必须是 0 条。危险调用（cron/systemd 两把）本章一次都不调用，第 9 格扫给你看。
# ★ 这一支要还的账：你手敲那套「会问你、会变色、会读你键盘」的东西，一旦写进脚本，stdin 不再是 tty、
#   alias 不再加载、read 直接读到 EOF —— 每一步都得先测出真相，不能凭手感。
# ★ 每个「应当 0 / 应当一致」的判据都先喂一条真违规把它弄响，再报真实值。
# ============================================================================

rm -rf ia
mkdir -p ia
PIDS=()
cleanup() {
    local p
    for p in ${PIDS[@]+"${PIDS[@]}"}; do kill "$p" 2>/dev/null || :; done
    wait 2>/dev/null || :
    rm -rf ia
}
trap cleanup EXIT

SBX=$(pwd)
HN=$(hostname)
IA="$SBX/ia"
fold() { sed -E "s#$IA#<沙箱>/ia#g; s#$SBX#<沙箱>#g; s#$HOME#<家目录>#g; s#\\b$USER\\b#<用户>#g; s#\\b$HN\\b#<主机名>#g"; }
# diff_files 旧 新：两份校验和一样印 0（没改动），不一样印 1（有改动）
diff_files() { [ "$1" = "$2" ] && echo 0 || echo 1; }

# ---------------------------------------------------------------------------
printf '=== 1. 交互程序在脚本里读到 EOF 的真相：三种入口各自的 rc 与方括号内容 ===\n'
printf '  先测本脚本自己的 stdin 是不是 tty：[ -t 0 ]=%s（0=不是 tty，所以裸 read 只能读到 EOF）\n' \
    "$( [ -t 0 ] && echo 1 || echo 0 )"
printf '  ① 不喂（继承脚本 stdin）：%s\n' \
    "$(bash -c 'read x; rc=$?; printf "方括号=[%s] read的rc=%s\n" "$x" "$rc"' | fold)"
printf '  ② </dev/null：%s\n' \
    "$(bash -c 'read x; rc=$?; printf "方括号=[%s] read的rc=%s\n" "$x" "$rc"' </dev/null | fold)"
printf '  ③ printf 喂一行：%s\n' \
    "$(printf '喂进来的\n' | bash -c 'read x; rc=$?; printf "方括号=[%s] read的rc=%s\n" "$x" "$rc"' | fold)"
printf '  → 上面三个 read 的 rc：① ② 都读到 EOF（rc=1、方括号空），③ 读到内容（rc=0）—— 这是实测不是猜。\n'
printf '    结论：EOF 时 read 的 rc=1 且变量为空，所以脚本里裸 `read` 等不到人也会往下跑、还带着空值。\n'

# ---------------------------------------------------------------------------
printf '\n=== 2. mv -i 那个 alias 只在交互 shell 里存在：手敲会问、脚本里直接盖 ===\n'
printf '  bash -c  里 type mv=[%s]\n' "$(bash -c 'type mv' 2>/dev/null | fold)"
printf '  bash -ic 里 type mv=[%s]\n' "$(bash -ic 'type mv' </dev/null 2>/dev/null | grep '^mv ' | fold)"
mv_case() {
    printf 'NEW\n' > "$IA/s"; printf 'OLD\n' > "$IA/t"
    bash -ic "cd '$IA'; $1" </dev/null >/dev/null 2>&1; local rc=$?
    local cov=0; grep -q NEW "$IA/t" && cov=1
    printf '    %-24s rc=%s 目标被盖成NEW吗=%s\n' "$1" "$rc" "$cov"
}
printf '  在「alias 已加载」的交互 shell 里，把同内容文件 mv 到已存在目标：\n'
mv_case 'mv s t'
mv_case 'command mv s t'
mv_case '\mv s t'
mv_case '/usr/bin/mv s t'
printf 'unalias mv 2>/dev/null || true\nmv s t\n' > ia/u.sh
bash -ic "cd '$IA'; printf 'NEW\n' > s; printf 'OLD\n' > t; source u.sh" </dev/null >/dev/null 2>&1
ucov=0; grep -q NEW "$IA/t" && ucov=1
printf '    %-24s 目标被盖成NEW吗=%s（unalias 要换到下一个 parse 单元才生效）\n' '脚本里先 unalias mv 再 mv' "$ucov"
# 固化后的安全写法：脚本里没有 mv -i 这个 alias，用「不覆盖已存在目标」的 mv -n 把「先确认再盖」这件事写死
printf 'NEW\n' > "$IA/s"; printf 'OLD\n' > "$IA/t"
bash -c "cd '$IA'; mv -n s t" </dev/null >/dev/null 2>&1; mvn_rc=$?
mvn_cov=0; grep -q NEW "$IA/t" && mvn_cov=1
mvn_left=0; [ -f "$IA/s" ] && mvn_left=1
printf '    %-24s rc=%s 目标被盖成NEW吗=%s 源文件还在吗=%s（-n：目标在就绝不盖、源留着，且不问）\n' '脚本里 mv -n s t' "$mvn_rc" "$mvn_cov" "$mvn_left"
printf '  → 成因就在头两行：脚本里 type mv=/usr/bin/mv（不加载 alias），交互里是 mv -i（每次盖之前问一句）。\n'
printf '    裸 mv 那一行（rc/覆盖）与四种免法（command、\\mv、绝对路径、unalias）结果不同 —— 免法才会真的盖上去。\n'
printf '    固化到脚本没有 alias 可依，就把安全写死成 mv -n：实测 rc=%s、目标被盖=%s、源还在=%s（不盖不报错）。\n' "$mvn_rc" "$mvn_cov" "$mvn_left"

# ---------------------------------------------------------------------------
printf '\n=== 3. read -p 的提示语去哪了：走 stderr、且只在 stdin 是 tty 时才印；IFS/-r 各保住什么 ===\n'
cat > ia/ask.sh <<'ASK'
#!/usr/bin/env bash
read -p "名：" v
printf '答=[%s]\n' "$v"
ASK
printf '张三\n' | bash ia/ask.sh >ia/a_out.txt 2>ia/a_err.txt
printf '  ① 管道喂 stdin（非 tty）：stdout 字节=%s ｜ stderr 字节=%s（提示语在非 tty 下根本不印）\n' \
    "$(wc -c <ia/a_out.txt)" "$(wc -c <ia/a_err.txt)"
printf '张三\n' | script -qec "bash ia/ask.sh 2>ia/p_err.txt" /dev/null >ia/p_out.txt 2>/dev/null
printf '  ② 伪 tty：stdout 字节=%s ｜ 提示语落到的 stderr 文件字节=%s（>0，且 stdout 里没有它）\n' \
    "$(wc -c <ia/p_out.txt)" "$(wc -c <ia/p_err.txt)"
printf '  提示语「名：」出现在 stdout 的次数=%s ｜ 出现在 stderr 的次数=%s\n' \
    "$(grep -c '名：' ia/p_out.txt)" "$(grep -c '名：' ia/p_err.txt)"
printf '  a\\b 尾\n' | bash -c 'read v; printf "plain=[%s]\n" "$v"' >ia/r_plain.txt
printf '  a\\b 尾\n' | bash -c 'IFS= read -r v; printf "rIFS=[%s]\n" "$v"' >ia/r_keep.txt
printf '  守卫（先弄响）不带 -r 把反斜杠吃掉：plain 含反斜杠吗=%s ｜ IFS= read -r 含反斜杠吗=%s\n' \
    "$(grep -c '\\' ia/r_plain.txt)" "$(grep -c '\\' ia/r_keep.txt)"
printf '  不带 IFS= 把前导空格 strip：plain 含前导空格吗=%s ｜ IFS= 版含前导空格吗=%s\n' \
    "$(grep -cE '=\[ ' ia/r_plain.txt)" "$(grep -cE '=\[ ' ia/r_keep.txt)"
printf '  → 提示语写的是 stderr（②stderr>0、stdout 里=0），管道里看不到它不是玄学；\n'
printf '    -r 保住反斜杠、IFS= 保住前导空格 —— 逐行读原文一律 IFS= read -r。\n'

# ---------------------------------------------------------------------------
printf '\n=== 4. 喂多行输入的三种形状：heredoc / here-string / 管道，各真读到几行；yes 的 rc 实测 ===\n'
cat > ia/count.sh <<'CNT'
#!/usr/bin/env bash
n=0
while IFS= read -r l; do n=$((n + 1)); done
printf '读到行数=%s\n' "$n"
CNT
h1=$(bash ia/count.sh <<'HEOF'
甲
乙
丙
HEOF
)
h2=$(bash ia/count.sh <<<$'x1\nx2\nx3')
h3=$(printf 'y1\ny2\ny3\n' | bash ia/count.sh)
printf '  heredoc(<<'"'"'EOF'"'"')=[%s] ｜ here-string(<<<)=[%s] ｜ 管道(printf|)=[%s]\n' "$h1" "$h2" "$h3"
cat > ia/peek.sh <<'PEEK'
#!/usr/bin/env bash
IFS= read -r l
printf '%s' "$l"
PEEK
q1=$(bash ia/peek.sh <<'Q'
$HOME
Q
)
u1=$(bash ia/peek.sh <<U
$HOME
U
)
printf '  守卫（原样 vs 展开）：<<'"'"'Q'"'"' 里写 $HOME，传进去仍含字面 HOME 吗=%s（期望 1，原样）｜ <<U 里写 $HOME，传进去含字面 HOME 吗=%s（期望 0，被父 shell 展开）\n' \
    "$(printf '%s' "$q1" | grep -c 'HOME')" "$(printf '%s' "$u1" | grep -c 'HOME')"
yes_dollar=$(bash -c 'yes 2>/dev/null | head -n 3 >/dev/null; echo $?')
yes_pipestat=$(bash -c 'yes 2>/dev/null | head -n 3 >/dev/null; echo "${PIPESTATUS[0]}"')
printf '  yes | head -n 3 → \$?=%s（这是末端 head 的码）｜ PIPESTATUS[0]（yes 的码）=%s（本机实测）\n' "$yes_dollar" "$yes_pipestat"
printf '  → 三种入口都把「逐行读」喂成读到行数=3；差别在形状不在结果。引号 heredoc 保住 $HOME 原样、不带引号会被展开。\n'
printf '    最后一行是 PIPESTATUS 的用处：管道跑完 $? 只是末端 head 的码，上游 yes 被 SIGPIPE 打成 %s —— 别只读 $?。\n' "$yes_pipestat"

# ---------------------------------------------------------------------------
printf '\n=== 5. 非交互检测四把尺的真值表：-t 0 / -t 1 / $- 里的 i / login_shell × 四种入口 ===\n'
cat > ia/probe.sh <<'PROBE'
#!/usr/bin/env bash
v0=0; [ -t 0 ] && v0=1
v1=0; [ -t 1 ] && v1=1
vi=0; [[ $- == *i* ]] && vi=1
vl=0; shopt -q login_shell && vl=1
printf 'T0=%s T1=%s I=%s L=%s\n' "$v0" "$v1" "$vi" "$vl"
PROBE
printf '  列含义：T0=stdin 是 tty T1=stdout 是 tty I=交互 L=登录 shell（每格都是真跑出来的 0/1）\n'
bash ia/probe.sh </dev/null >ia/o5.txt 2>/dev/null
r1=$(grep -oE 'T0=[01] T1=[01] I=[01] L=[01]' ia/o5.txt)
r2=$(bash -c "$(cat ia/probe.sh)" 2>/dev/null | grep -oE 'T0=[01] T1=[01] I=[01] L=[01]')
r3=$(bash -i ia/probe.sh </dev/null 2>/dev/null | grep -oE 'T0=[01] T1=[01] I=[01] L=[01]')
r4=$(script -qec 'bash ia/probe.sh' /dev/null 2>/dev/null | grep -oE 'T0=[01] T1=[01] I=[01] L=[01]')
printf '  入口① bash 脚本(</dev/null >file)：%s\n' "${r1:-读空}"
printf '  入口② bash -c (继承 stdin/stdout)：%s\n' "${r2:-读空}"
printf '  入口③ bash -i (stdin </dev/null)  ：%s\n' "${r3:-读空}"
printf '  入口④ script 伪 tty               ：%s\n' "${r4:-读空}"
bash -l ia/probe.sh </dev/null >ia/lout.txt 2>/dev/null
rl=$(grep -oE 'T0=[01] T1=[01] I=[01] L=[01]' ia/lout.txt)
printf '  守卫（让 login_shell 那一列先响一次）bash -l：%s（L=1 才证明这一列不是恒零的死探针）\n' "${rl:-读空}"
printf '  → 四行里唯一让 T1（stdout 是 tty）变真的是入口④伪 tty（T1=1）—— 这就是「有 tty 程序就换一套输出」的开关；\n'
printf '    入口③ bash -i 只把 I 变 1（交互），进不去 login_shell（L=0）；要 L=1 得 bash -l（守卫那行）。四把尺各测各的。\n'

# ---------------------------------------------------------------------------
printf '\n=== 6. 有 tty 与没 tty，同一条命令的输出真的不一样：ls --color=auto 的转义命中数 ===\n'
mkdir -p ia/d ia/d/子目录; : > ia/d/普通文件
# 数「真颜色转义」要认 CSI 序列 ^[[ （ESC 紧跟 [），不能只数 ^[：
# 中文文件名的字节（如 0x9B）经 cat -v 会变成 M-^[，里面正好含 ^[，只数 ^[ 会误报成管道也有颜色。
pipe_hits=$(ls -1 --color=auto ia/d 2>/dev/null | cat -v | grep -c '\^\[\[')
pty_hits=$(script -qec 'ls -1 --color=auto ia/d' /dev/null 2>/dev/null | cat -v | grep -c '\^\[\[')
printf '  守卫（先弄响）伪 tty 下带颜色转义(^[[)的行数=%s（期望 >0，目录名被上色）\n' "$pty_hits"
printf '  直连管道下带颜色转义(^[[)的行数=%s（期望 0，程序判定 stdout 不是 tty 就关掉颜色）\n' "$pipe_hits"
printf '  → 同一句 ls --color=auto：管道里转义命中=%s、伪 tty 里命中=%s（^[[ 用 cat -v 显形后才数得到）。\n' "$pipe_hits" "$pty_hits"
printf '    所以脚本里复制粘贴「没颜色 / 进度条消失」不是坏了，是程序自己在看 fd 是不是 tty 来决定输出形状。\n'

# ---------------------------------------------------------------------------
printf '\n=== 7. 把一步步手敲的东西固化成脚本：参数化三档 + 多入口 + getopts 该不该有 -h ===\n'
cat > ia/run7.sh <<'R7'
#!/usr/bin/env bash
if [ $# -ge 1 ]; then src=位置参数; val="$1"; rc=0
elif [ -n "${GREET_NAME:-}" ]; then src=环境变量; val="$GREET_NAME"; rc=0
else src=stdin; IFS= read -r val; rc=$?
fi
printf 'src=%s val=[%s] rc=%s\n' "$src" "$val" "$rc"
R7
printf '  无参数 + 无环境变量（stdin </dev/null 读到 EOF）：%s\n' "$(bash ia/run7.sh </dev/null | fold)"
printf '  带位置参数（甲）：%s\n' "$(bash ia/run7.sh 甲 | fold)"
printf '  带环境变量 GREET_NAME=乙：%s\n' "$(GREET_NAME=乙 bash ia/run7.sh | fold)"
printf '  喂 stdin（printf 丙）：%s\n' "$(printf '丙\n' | bash ia/run7.sh | fold)"
cat > ia/with_h.sh <<'WH'
#!/usr/bin/env bash
while getopts "h" o; do
    case "$o" in
        h) printf '用法：xxx [-h]\n'; exit 0 ;;
        *) exit 2 ;;
    esac
done
printf '跑正事\n'
WH
cat > ia/no_h.sh <<'NH'
#!/usr/bin/env bash
printf 'val=[%s]\n' "$1"
NH
printf '  有 getopts 的脚本传 -h：打印用法了吗=%s ｜ rc=%s\n' \
    "$(bash ia/with_h.sh -h 2>/dev/null | grep -c '用法')" "$(bash ia/with_h.sh -h >/dev/null 2>&1; echo $?)"
printf '  没有 getopts 的脚本传 -h：把它当普通位置参数咽下（脚本自己打印的整行）=%s ｜ rc=%s\n' \
    "$(bash ia/no_h.sh -h)" "$(bash ia/no_h.sh -h >/dev/null 2>&1; echo $?)"
printf '  → 三档读取各测到一个值；-h 这件事：有 getopts 的脚本打用法并退 0，没有的脚本把 -h 当参数咽下去（rc 仍 0、不报错）。\n'

# ---------------------------------------------------------------------------
printf '\n=== 8. 固化完必须自带 dry-run 与幂等：先喂一条真会改动的样本把计数弄响 ===\n'
cat > ia/tag.sh <<'TG'
#!/usr/bin/env bash
dry=0; [ "${1:-}" = --dry ] && dry=1
n=0
for f in ia/t/*; do
    if ! grep -q '#TAG' "$f" 2>/dev/null; then
        [ "$dry" = 0 ] && printf '#TAG\n' >> "$f"
        n=$((n + 1))
    fi
done
printf 'would=%s\n' "$n"
TG
# 守卫：真改一趟（改动=1），把「实际改动=0」这个断言先弄响
rm -rf ia/t; mkdir -p ia/t; : > ia/t/f1; : > ia/t/f2
ring_b=$(cksum ia/t/f1 ia/t/f2); ring_would=$(bash ia/tag.sh | grep -oE 'would=[0-9]+') ; ring_a=$(cksum ia/t/f1 ia/t/f2)
ring_changed=$(diff_files "$ring_b" "$ring_a")
printf '  守卫（先弄响）真跑一趟：%s（该脚本报自己要改几份）｜ 实际有文件被改吗=%s（期望 1，证明这个改动计数器会响）\n' "$ring_would" "$ring_changed"
# dry 那趟：不练，只说 —— 实际改动应=0
rm -rf ia/t; mkdir -p ia/t; : > ia/t/f1; : > ia/t/f2
dry_b=$(cksum ia/t/f1 ia/t/f2); dry_would=$(bash ia/tag.sh --dry | grep -oE 'would=[0-9]+'); dry_a=$(cksum ia/t/f1 ia/t/f2)
dry_changed=$(diff_files "$dry_b" "$dry_a")
printf '  dry 那趟：%s（报想改几份）｜ 实际被改动的文件数=%s（期望 0：只说不练）\n' "$dry_would" "$dry_changed"
# 幂等：连跑两遍真版，第二遍改动=0
r1w=$(bash ia/tag.sh | grep -oE 'would=[0-9]+')
idem_b=$(cksum ia/t/f1 ia/t/f2); r2w=$(bash ia/tag.sh | grep -oE 'would=[0-9]+'); idem_a=$(cksum ia/t/f1 ia/t/f2)
idem_changed=$(diff_files "$idem_b" "$idem_a")
printf '  幂等第二遍：%s（还想改几份=0）｜ 实际被改动的文件数=%s（期望 0：已打过标就不再动）\n' "$r2w" "$idem_changed"
printf '  → dry 改动=%s、真一趟改动=%s（把守卫弄响）、幂等第二遍改动=%s：固化脚本的红线是「先能 dry，再能重跑不重复改」。\n' "$dry_changed" "$ring_changed" "$idem_changed"

# ---------------------------------------------------------------------------
printf '\n=== 9. 本章红线自查：危险调用 / 联网 / 开端口 / 碰 rc 文件 —— 全部先弄响再报真实值 ===\n'
# 持违规样本的文件（模式串一律拆开写，保证本脚本源码里不出现「整串」被自己扫到）
printf '%s\n' "cro"'ntab -l' > ia/bad.sh
printf '%s\n' "syste"'mctl status x' >> ia/bad.sh
printf '%s\n' "cu"'rl http://x.invalid' >> ia/bad.sh
printf '%s\n' "n"'c -l 9999' >> ia/bad.sh
printf '%s\n' 'echo hi >> ~/.b'"a"'shrc' >> ia/bad.sh
scan() { # $1=正则 $2=标题
    local ring real
    ring=$(grep -Ec "$1" ia/bad.sh)
    real=$(grep -Ec "$1" "$0")
    printf '    %-20s 样本命中=%s（先弄响 期望>0）｜ 本脚本真实命中=%s（期望 0）\n' "$2" "$ring" "$real"
}
scan '[c]rontab |[s]ystemctl ' '危险调用 cron/systemd'
scan '[c]url |[w]get ' '联网'
scan '[n]c -l ' '开监听端口'
scan '[.]bashrc|[.]profile|[.]config' '碰 rc / 配置目录'
printf '  → 四类都是「样本命中非 0（证明这套正则真抓得到违规）、本脚本命中=0」：一次没调用、没联网、没开端口、没写 rc 文件。\n'
printf '    没有一条守卫是恒零死数字 —— 先拿 ia/bad.sh 的真违规把它弄响，再对本脚本源码扫，得到的是真 0。\n'

# ---------------------------------------------------------------------------
printf '\n=== 10. 收尾自查：没有遗留后台进程（jobs 条数必须=0）===\n'
jobn=$(jobs -l 2>/dev/null | grep -c '^\[')
printf '  本脚本自己起的后台进程数（jobs 计数）=%s（期望 0，全脚本没有 & 起来的常驻进程）\n' "$jobn"
printf '  → 计数=%s：interact.sh 全程同步、没起后台；trap 退出时把 ia/ 整目录删掉，不留残留。\n' "$jobn"

printf '\n[interact.sh 自查] 本脚本格数=%s ｜ ia/ 里现有文件数=%s（跑完由 trap 整目录删除）\n' \
    "$(grep -Ec "^printf '.*=== [0-9]" "$0")" "$(find ia -type f | wc -l)"
exit 0
