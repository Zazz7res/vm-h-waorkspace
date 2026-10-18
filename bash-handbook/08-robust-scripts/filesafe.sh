#!/usr/bin/env bash
# ============================================================================
# filesafe.sh —— 文件边界：判断跟着谁、mktemp 给什么权限、先测后开的那道缝
#
#   demo.sh 里这样调：bash filesafe.sh
#   安全：只在当前目录建 fs/ 子目录做全部实验（普通文件、空档、指向目录的链接、断链、符号链接改指、
#         锁文件都在 fs/ 里自己造自己删）；不碰 ~、不读不写 /etc 与任何系统路径、不联网、不装包、不 kill。
#         唯一一次默认 mktemp（不带模板，本机默认落在系统临时目录）当场折成 <沙箱> 上屏并立刻删掉。
#   故意不开 set -e：本脚本必须活到打印 rc 那一行；要演示 -C / 子 shell 一律写进 bash -c 里面。
#
# ★ 本机不拿「文档说 -e 跟随符号链接」这种话当证据：矩阵是逐格 [ -e ]/[ -f ]/[ -L ]/[ -s ] 真跑出来的；
#   mktemp 的权限是 stat 真读出来的；TOCTOU 那两格的「间隙」由我们自己插（真实事故里这一步是第三方做的，
#   屏幕上明说这是替它演，不假装抓到了并发）。每格走 run 助手：命令串与参数原样上屏（标签=真命令），
#   在全新子 bash 里跑，当场打印 rc 并抄进台账；mktemp/工具报错原文（中文英文）一字不改。
# ============================================================================

rm -rf fs
mkdir -p fs/d
trap 'rm -rf fs' EXIT
: > fs/rcs.txt

run() {
    cmd=$1
    shift
    printf '  敲的是：%s\n' "$cmd"
    [ $# -gt 0 ] && printf '  参数是：%s\n' "$(printf '[%s]' "$@")"
    out=$(bash -c "$cmd" _ "$@" 2>&1)
    code=$?
    if [ -n "$out" ]; then printf '%s\n' "$out" | sed 's|^/tmp/[^ ]*|<沙箱>|;s/^/    /'; else printf '    （一声没出）\n'; fi
    printf '  rc=%s\n' "$code"
    printf '%s\n' "$code" >> fs/rcs.txt
}

echo "=== 1. 四种判断各自跟着谁：四个对象 × 六个判断，逐格真跑 ==="
: > fs/plain
echo x > fs/plain
: > fs/empty
ln -sfn d fs/to_dir
ln -sfn gone fs/dangling
printf '  %-10s' '对象'
for t in -e -f -L -s -d -r; do printf '%s ' "$t"; done
echo
for o in plain empty to_dir dangling; do
    printf '  %-10s' "$o"
    for t in -e -f -L -s -d -r; do
        if [ "$t" "fs/$o" ]; then printf '是 '; else printf '否 '; fi
    done
    printf '← 它是链接吗：%s\n' "$([ -L "fs/$o" ] && echo 是 || echo 不是)"
done
printf '  → 直觉以为「-e 存在、-f 是文件、够用了」：断链那行 -L=是 而 -e=否 —— 名字明明在那儿，-e 却说没有。\n'
printf '  ★ 因为 -e/-f/-s/-d/-r 全都**跟着链接走**，只有 -L 看链接本身；指向目录的链接 -f=否、-d=是。\n'
printf '    所以「我要的是真文件」写 -f，「我要拦符号链接」必须显式 -L，两件事不能互相顶替。\n'

echo "=== 2. mktemp 给的是什么权限（stat 真读；随机名字压根不上屏，只报权限与类型）==="
run 'f=$(mktemp); echo "默认无模板 → 权限=$(stat -c "%a %F" "$f")"; rm -f "$f"'
run 'd=$(mktemp -d); echo "-d → 权限=$(stat -c "%a %F" "$d")"; rmdir "$d"'
run 'f=$(mktemp -t b08fs.XXXXXX); echo "-t 模板 → 权限=$(stat -c "%a" "$f")"; rm -f "$f"'
printf '  → 直觉以为「临时文件跟普通 > 建出来一样是 644/755」：mktemp 给的是 **600 文件 / 700 目录**。\n'
printf '  ★ 用 > 自己拼 /tmp/xxx 不但可预测（别人的进程能先占住这个名字），权限还是默认的宽权限 ——\n'
printf '    「名字不可猜 + 权限只给自己」这两条是 mktemp 一次买齐的，手写拼路径两条都丢。\n'

echo "=== 3. 模板里那串 X 的规则：不在末尾也能用，配 --suffix 却不行 ==="
run 'f=$(mktemp fs/XXXXXX.txt); echo "建出来了：后缀=$(printf %s "${f##*.}") 整串长度=${#f} X 有没有被换成随机串=$([ "$f" = "fs/XXXXXX.txt" ] && echo 没有 || echo 换了)"'
run 'mktemp --suffix=.log fs/XXXXXXX.tmp'
run 'mktemp fs/XX'
printf '  → 直觉以为「X 必须在模板末尾」：第一格 fs/XXXXXX.txt **成功**（尾巴 .txt 留着），rc=0。\n'
printf '  ★ 第二格加了 --suffix 反而失败，原话是「模板 must end in X」rc=1（同一台机器上两条规则打架）；\n'
printf '    第三格 X 只有两个，报错也是原话 —— 记不住规则就别猜，让 mktemp 自己说 rc 和原因。\n'

echo "=== 4. ★ 先测后开第一刀：断链让 -e 说「不存在」，写下去却凭空造出一个新文件 ==="
run 'cd fs; ln -sfn newtarget link1; if [ -e link1 ]; then echo "测到：存在"; else echo "测到：不存在，那就写"; fi; echo 秘密内容 > link1; echo "link1 还是链接吗=$([ -L link1 ] && echo 是 || echo 不是)"; echo "newtarget 出现了吗=$([ -e newtarget ] && echo 出现了 || echo 没有) 内容=$(cat newtarget 2>/dev/null)"'
printf '  → 直觉以为「测过不存在就能放心写」：-e 对断链回的是「不存在」，而 echo > 链接 把内容写进了\n'
printf '    链接**指向的那个名字** —— 屏幕上 newtarget 从无到有，我们从头到尾没写过它的名字。\n'
printf '  ★ 判断跟着链接走、写入也跟着链接走：测的是链接、落的却是别人替你选好的路径。\n'

echo "=== 5. ★ 先测后开第二刀：测完到开写之间链接被改指（这一步由我们替第三方演）==="
run 'cd fs; echo 原来的重要内容 > victimA; ln -sfn victimB swap; if [ -e swap ]; then echo "测到：存在，不写"; else echo "测到：swap 背后的名字不存在，可以放心写"; fi; ln -sfn victimA swap; echo 注入 > swap; echo "victimA 现在=[$(cat victimA)]"; echo "victimB 存在吗=$([ -e victimB ] && echo 存在 || echo 不存在)"'
printf '  → 判定那一刻 swap 指向 victimB（不存在）→ 屏幕上走的正是「可以放心写」这一支；间隙里把 swap 改指\n'
printf '    victimA（里面有内容），于是写入落到**一个已存在、我们从来没测过的文件**上：victimA 被覆盖。\n'
printf '  ★ 这一格不假装抓到了并发：真实事故里换链接的是别人，这里由我们手动插一步 —— 证的不是概率，而是\n'
printf '    「测完再开 = 开的是别人说了算的那个路径」。正解见第 6、7 格。\n'

echo "=== 6. 正解一：set -C（noclobber）把「存在就报错」挪到打开的那一刻 ==="
run 'cd fs; echo 已有 > target0; ( set -C; echo 新的 > target0 ); echo "同名已存在 rc=$?"'
run 'cd fs; ( set -C; echo 新的 > brandnew ); echo "新名字 rc=$? 内容=$(cat brandnew)"'
run 'cd fs; ( set -C; echo 强行 >| target0 ); echo ">| 之后 rc=$? 内容=$(cat target0)"'
printf '  → 直觉以为「noclobber 是交互 shell 的玩具」：第二格里它就是那条缝的封条 —— 判断不在脚本里做，\n'
printf '    由 open() 当场做（原话「无法覆盖已存在的文件」+ rc=1），中间没人能插一步。\n'
printf '  ★ 但 >| 一强行就破功（第三格 rc=0、内容被换）：封条只挡住「忘了」，挡不住「非要覆盖」。\n'

echo "=== 7. 正解二：flock 的锁跟着「打开的文件描述」走，不跟着 fd 号、也不跟着文件名 ==="
run 'cd fs; exec 9>mylock; flock -n 9 && echo "第一次：拿到"; flock -n 9 && echo "第二次：也拿到（同一个 fd 不会自锁）" || echo "第二次：拿不到"'
run 'cd fs; exec 9>mylock; flock -n 9; bash -c "exec 8>mylock; flock -n 8; echo \"子进程重新开一个 fd 去拿：rc=\$?\""'
run 'cd fs; exec 9>mylock; flock -n 9; bash -c "flock -n 9; echo \"子进程不重开、用继承来的同一个 fd：rc=\$?\""'
printf '  → 直觉以为「flock 会挡住重复加锁的自己」：第一格同一个 fd 两次都拿到 —— 锁登记在「打开的文件描述」\n'
printf '    上，同一个描述再 flock 一次只是换个状态，不会自锁。\n'
printf '  ★ 第二格另一个进程**重新 open 一个 fd**（8）去拿同一个文件：rc=1 真的拿不到；第三格那个子进程不重开、\n'
printf '    直接用 fork 继承来的同一个 fd 9：rc=0 —— 它天生就带着父进程那把锁。所以「派出去的子任务」连着锁，\n'
printf '    而「另起一个实例重新开文件」才被挡住；只记「flock 防并发」这一句，这两件事一件也想不到。\n'

echo "=== 8. printf %q：把肉眼看不出来的名字变成能核对的字面 ==="
run 'cd fs; n="怪 名.txt"; printf "名字(s格式)=[%s] 名字(q格式)=[%q]\n" "$n" "$n"'
run 'cd fs; printf "带引号的名字(q格式)=[%q]\n" "$(printf "带\x27引号\x27.txt")"'
run 'cd fs; printf "带制表符的名字(q格式)=[%q]\n" "$(printf "tab\t名.txt")"'
run 'cd fs; q=$(printf "%q" "怪 名.txt"); eval "back=$q"; echo "粘回命令行之后名字相等吗=$([ "$back" = "怪 名.txt" ] && echo 相等 || echo 不相等) 字符数=$(printf %s "$back" | wc -m)"'
printf '  → 直觉以为「echo 出来就能看出有没有空格」：同一行两种格式并排，s 那个 [怪 名.txt] 看不出边界在哪，\n'
printf '    q 那个在空格前面补了反斜杠 —— 同一个名字的两种说法并排，一比就知道差在哪。\n'
printf '  ★ 带引号的名字 q 给出「反斜杠+引号」，带制表符的名字 q 干脆换成美元加单引号的 ANSI-C 形状；\n'
printf '    最后一格才是 %%q 的用途：粘回命令行还能得到同一个名字（相等、字符数一致）—— 但那一步是 eval，\n'
printf '    只对自己生成的字符串用；用户给的东西一律走数组和 "$@"，别拿 eval 接（见 weirdnames.sh）。\n'

echo "=== 9. 收账：上面每一条的真实退出码 ==="
printf '  按出场顺序的码：%s\n' "$(paste -sd ' ' fs/rcs.txt)"
n=$(wc -l < fs/rcs.txt)
z=$(grep -c '^0$' fs/rcs.txt)
nz=$(grep -vc '^0$' fs/rcs.txt)
o=$(grep -c '^1$' fs/rcs.txt)
printf '  条数=%s ｜ 码 0 的=%s ｜ 非 0 的=%s ｜ 其中 1 的=%s\n' "$n" "$z" "$nz" "$o"
printf '  两个加起来等于条数吗：%s\n' "$([ "$((z + nz))" -eq "$n" ] && echo 等于 || echo 不等于)"
printf '  ★ 「码 1」会不会是恒零的假守卫？塞一条 1 进台账再撤掉：\n'
cp fs/rcs.txt fs/rcs.bak
printf '1\n' >> fs/rcs.txt
ring=$(grep -c '^1$' fs/rcs.txt)
mv fs/rcs.bak fs/rcs.txt
back=$(grep -c '^1$' fs/rcs.txt)
printf '    塞进去 → 从 %s 变成 %s（响了）；撤掉探针回到 %s\n' "$o" "$ring" "$back"
printf '  → 三条能带走的话：① -e/-f/-s 全跟着链接走，只有 -L 看链接本身，拦链接得显式写；\n'
printf '    ② 先测后开等于把开哪个路径交给别人 —— 用 mktemp（不可猜+600）或 set -C 让 open 自己判；\n'
printf '    ③ flock 防的是别的进程，不防同一个 fd 的自己；名字看不清就用 %%q，但别拿 eval 接用户输入。\n'
exit 0
