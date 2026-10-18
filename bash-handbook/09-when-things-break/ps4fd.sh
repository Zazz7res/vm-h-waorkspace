#!/usr/bin/env bash
# ============================================================================
# ps4fd.sh —— PS4 能塞什么、它什么时候被展开，以及把 trace 单独引走的三条路
#
#   demo.sh 里这样调：bash ps4fd.sh
#   安全：只在当前目录建 p4/（被演示的小脚本、trace 落盘文件、计数器），跑完 trap 整目录删掉。
#         全用相对文件名；不碰 ~、不写绝对 /tmp、不联网、不 sleep、不起后台。
# ★ 这一支要还的账：PS4 不是「提示符装饰」，它是**每条命令前都要求值一次**的字符串。
#   三件事最容易写错：① 用双引号赋值 → 只在赋值那一刻展开一次，之后是一条死字符串；
#   ② 里面放 $( ) → 求值发生在子 shell，改父侧变量的企图全部落空，而你看到的 0 长得像「没跑」；
#   ③ 里面放身份与易变值（主机名、pid、时间）→ 屏上再也擦不掉，可复现性同时作废。
# ============================================================================

rm -rf p4
mkdir -p p4
trap 'rm -rf p4' EXIT

fold() { sed -E "s#$HOME#<家目录>#g; s#\\b$USER\\b#<用户>#g; s#$(hostname)#<主机名>#g"; }
so()  { "$@" 2>/dev/null | sed 's/^/      1│ /'; }

cat > p4/lib.sh <<'EOF'
#!/usr/bin/env bash
g() {
    echo 在g里
}
f() {
    g
}
f
EOF

echo "=== 1. PS4 的引号决定它被展开几次：双引号=一次，单引号=每条命令一次 ==="
spec='+[L${LINENO}] ${FUNCNAME[0]:-顶层}> '
printf '  想塞进去的模板（原样）=[%s]\n' "$spec"
printf '  A 写法 PS4="模板"（双引号）赋值之后 PS4 的实际内容=[%s]\n' "$(bash -c "PS4=\"$spec\"; printf %s \"\$PS4\"")"
printf '  B 写法 PS4=模板（单引号）赋值之后 PS4 的实际内容=[%s]\n' "$(bash -c "PS4='$spec'; printf %s \"\$PS4\"")"
printf '  同一个模板跑同一个 8 行脚本。A 的 trace：\n'
bash -c "PS4=\"$spec\"; set -x; source ./p4/lib.sh" 2>&1 | fold | sed 's/^/      A│ /'
printf '  B 的 trace：\n'
bash -c "PS4='$spec'; set -x; source ./p4/lib.sh" 2>&1 | fold | sed 's/^/      B│ /'
printf '  → A 四行的行号全是 L1、函数名全是「顶层」—— 模板在赋值那一刻就被展开成死串（那一刻 LINENO=1、\n'
printf '    FUNCNAME 是空），之后每条命令打印的都是这一条死串；B 才逐行跟着走。\n'
printf '    结论很短：PS4 要用单引号赋值。双引号不是语法错，屏上照样有 trace，只是内容是假的 ——\n'
printf '    「有输出但没信息」是这一章最难查的那类 bug。\n'

echo
echo "=== 2. 单引号之后，六种 PS4 各打出什么（认出哪两格的名字位置是空的） ==="
cat > p4/t6.sh <<'EOF'
f() { echo 体内; }
f
( f )
EOF
for spec in '+ ' '+[${LINENO}] ' '+[${FUNCNAME[0]}] ' '+[${BASH_SOURCE}] ' '+[s${BASH_SUBSHELL}] ' '+[$(echo 命令替换)] '; do
    printf '  PS4=[%s] →\n' "$spec"
    bash -c "PS4='$spec'; set -x; source ./p4/t6.sh" 2>&1 | fold | sed 's/^/      /'
done
printf '  → 认三件事：① 顶层那一次 FUNCNAME[0] 与 BASH_SOURCE 是空串，所以打出来是 "+[] …>" 这种名字位置什么都没有的形状；\n'
printf '    ② LINENO 在 source 进来的文件里是逐行真号（2、1、3），不是恒 1；\n'
printf '    ③ BASH_SUBSHELL 只在 ( f ) 那一次从 s0 跳到 s1 —— 它是变量展开，不额外起进程。\n'

echo
echo "=== 3. 加号的层数从哪来：source 与命令替换加号，函数调用不加 ==="
cat > p4/deep.sh <<'EOF'
f() { g() { echo 最里; }; g; }
f
echo $(echo $(echo 替换里))
( echo 圆括号 )
EOF
printf '  默认 PS4 跑这份混合深度：\n'
bash -x p4/deep.sh 2>&1 | fold | sed 's/^/      /'
printf '  → 数加号：函数 f→g→echo 三行全是单 +；命令替换那三行从 +++ 递减到 +（最里层先执行所以先打）；\n'
printf '    ( ) 里的 echo 还是单 +。再加上第 2 格看到的 source 会多一个 + ——\n'
printf '    默认 PS4 的加号说的是「source / 命令替换」的嵌套，跟函数调用深度无关（对照 xtrace.sh 第 1 格）。\n'

echo
echo "=== 4. PS4 里的两种展开：算术数得住，命令替换数不住 ==="
cat > p4/cnt.sh <<'EOF'
#!/usr/bin/env bash
n=0
PS4='+#$((n += 1)) '
set -x
:
:
:
set +x
printf '  A 算术展开版：父侧计数器 n=%s\n' "$n"
m=0
PS4='+$(m=$((m + 1))) '
set -x
:
:
:
set +x
printf '  B 命令替换版：父侧计数器 m=%s\n' "$m"
PS4='+$(echo x >> p4/cnt.txt) '
: > p4/cnt.txt
set -x
:
:
:
set +x
printf '  C 往文件追加的行数=%s\n' "$(wc -l < p4/cnt.txt)"
EOF
bash p4/cnt.sh 2>&1 | fold | sed 's/^/      /'
printf '  → 三种号码形状就是判据：A 打出 #1 #2 #3 #4 —— 算术展开**不 fork**，改的是父侧的 n（n=4 里含那句 set +x）；\n'
printf '    B 每条都打空 —— 命令替换 fork 出子 shell，m 在子 shell 里加完就丢，父侧 m 停在 0；\n'
printf '    C 屏上同样是空，但文件落住 4 行 —— 3 条命令 + 那句「读数用的 printf」自己也触发一次 PS4。\n'
printf '    B 最容易读反：m=0 看着像「一条都没跑」，其实是「跑了，但没地方落」—— 与第 08 章「rc=0 不等于办成」同族。\n'

echo
echo "=== 5. 身份与易变值进 PS4：主机名上屏、pid 让对拍作废 ==="
printf '  PS4 里放命令替换取主机名，上屏前过一遍本章的 fold（它依次折家目录、用户名、主机名）：\n'
bash -c "PS4='+[$(hostname)] '; set -x; echo 演示" 2>&1 | fold | sed 's/^/      /'
printf '  不折的话这一屏命中真实主机名=%s 处（所以本章只印「几处」，不印内容）\n' \
    "$(bash -c "PS4='+[$(hostname)] '; set -x; echo 演示" 2>&1 | grep -c "$(hostname)")"
h=$(hostname); u=$USER
printf '  同一串字被两条规则抢：本机 hostname 与 $USER 是同一个串吗=%s，那就把 fold 的两条规则换个先后各跑一次\n' \
    "$([ "$h" = "$u" ] && echo 是 || echo 否)"
printf '    先折用户名=[%s]\n    先折主机名=[%s]\n' \
    "$(bash -c "PS4='+[$h] '; set -x; echo 演示" 2>&1 | sed -E "s#\\b$u#<用户>#g; s#$h#<主机名>#g" | head -1)" \
    "$(bash -c "PS4='+[$h] '; set -x; echo 演示" 2>&1 | sed -E "s#$h#<主机名>#g; s#\\b$u#<用户>#g" | head -1)"
printf '  → 屏上第一行折出来的是 <用户> 而不是 <主机名>：这台机器上两个身份是同一个字符串，\n'
printf '    折叠规则谁写在前面谁赢。**占位符的名字不等于它折掉的原本是什么** —— 拿占位符当「我折掉了哪一类」的\n'
printf '    证据会读反；要么像上面这样把顺序写明，要么干脆把两条并成一条只印 <身份>。\n'
printf '  pid 只比相同/不同：同一个 shell 里 $$ 与 BASHPID 相同吗=%s ｜ 进 ( ) 后 $$ 与外面相同吗=%s ｜ 进 ( ) 后 BASHPID 与外面相同吗=%s\n' \
    "$(bash -c '[[ $$ == $BASHPID ]] && echo 相同 || echo 不同')" \
    "$(p=$$; ( r=$$; [[ "$p" == "$r" ]] && echo 相同 || echo 不同 ))" \
    "$(p=$BASHPID; ( r=$BASHPID; [[ "$p" == "$r" ]] && echo 相同 || echo 不同 ))"
printf '  另起一个 bash 进程的 $$ 与父进程相同吗=%s（pid 全是每跑必变的值，本章一个都不上屏）\n' \
    "$([[ "$$" == "$(bash -c 'echo $$')" ]] && echo 相同 || echo 不同)"
printf '  → BASHPID 才跟着 fork 变，$$ 一直是「最初那个 shell」；把任何一个塞进 PS4，\n'
printf '    就等于宣布这份日志不可对拍（第 9 格把它响给你看），还把 pid 永久留在文件里。\n'

echo
echo "=== 6. BASH_XTRACEFD 的三档：默认 2、并进 1、单开一个 fd ==="
cat > p4/x.sh <<'EOF'
#!/usr/bin/env bash
echo 正菜
EOF
printf '  档一（默认）：stdout=%s 行 ｜ stderr=%s 行\n' \
    "$(bash -c 'set -x; source ./p4/x.sh' 2>/dev/null | wc -l)" \
    "$(bash -c 'set -x; source ./p4/x.sh' 2>&1 >/dev/null | wc -l)"
printf '  档二 BASH_XTRACEFD=1：全部挤到 stdout\n'
so bash -c 'BASH_XTRACEFD=1; set -x; source ./p4/x.sh'
printf '  档二的 stderr 行数=%s\n' "$(bash -c 'BASH_XTRACEFD=1; set -x; source ./p4/x.sh' 2>&1 >/dev/null | wc -l)"
printf '  档三 BASH_XTRACEFD=9 + 先 exec 9>文件：stdout 只剩正菜\n'
so bash -c 'exec 9>p4/trace.log; BASH_XTRACEFD=9; set -x; source ./p4/x.sh; set +x; exec 9>&-'
printf '  档三的 trace 文件行数=%s ｜ 第一条=[%s]\n' "$(wc -l < p4/trace.log)" "$(head -1 p4/trace.log)"
printf '  → 档二适合「结果和日志一起进管道」，档三适合「正菜给调用方、trace 单独存档」；\n'
printf '    档一的坑是：你以为 2>error.log 收的是报错，其实 trace 也全在那条流里。\n'

echo
echo "=== 7. fd 是谁开的才算：父开子继承、子自开、中途关掉、没开过就哑掉 ==="
exec 7>p4/parent.log
bash -c 'BASH_XTRACEFD=7; set -x; echo 子进程写的; set +x' >/dev/null 2>&1
printf '  ① 父 shell 开 fd7，子 bash 只设 BASH_XTRACEFD=7 → 文件行数=%s\n' "$(wc -l < p4/parent.log)"
bash -c 'exec 7>p4/own.log; BASH_XTRACEFD=7; set -x; echo 自己开; f(){ :; }; f; set +x' >/dev/null 2>&1
printf '  ② 子 bash 自己 exec 7> 再设 → 文件行数=%s\n' "$(wc -l < p4/own.log)"
bash -c 'exec 7>p4/none.log; BASH_XTRACEFD=7; set -x; echo 第一段; exec 7>&-; echo 关完之后; echo 再一句; set +x' >/dev/null 2>&1
printf '  ③ 中途 exec 7>&- 关掉 → 文件行数=%s（关之后那两句的 trace 无处可去，直接丢）\n' "$(wc -l < p4/none.log)"
before=$(find p4 -type f | wc -l)
er=$(bash -c 'BASH_XTRACEFD=8; set -x; echo 一个字都没落; set +x' 2>&1 >/dev/null | wc -l)
ou=$(bash -c 'BASH_XTRACEFD=8; set -x; echo 一个字都没落; set +x' 2>/dev/null | wc -l)
after=$(find p4 -type f | wc -l)
printf '  ④ 父进程没开过 fd8，子 bash 直接设 BASH_XTRACEFD=8 → 新建文件数=%s ｜ stderr 行数=%s ｜ stdout 正菜行数=%s\n' \
    "$((after - before))" "$er" "$ou"
printf '     （正菜照打，trace 一个字都没有，也不报错 —— xtrace 突然「哑了」最常见的原因就是 fd 号指向空气）\n'
printf '  → fd 号是**进程自己的文件描述表**；exec 建的 fd 默认不带 close-on-exec，所以子进程接着能用（①）。\n'

echo
echo "=== 8. trace 落盘：权限靠 umask，家目录得当场折 ==="
( umask 077; bash -c 'exec 3>p4/secret-trace.log; BASH_XTRACEFD=3; set -x; echo 演示; f(){ :; }; f; set +x' >/dev/null 2>&1 )
( umask 022; bash -c 'exec 3>p4/world-trace.log; BASH_XTRACEFD=3; set -x; echo 演示' >/dev/null 2>&1 )
printf '  umask 077 建的 trace 权限=%s ｜ umask 022 建的=%s\n' \
    "$(stat -c %a p4/secret-trace.log)" "$(stat -c %a p4/world-trace.log)"
bash -c 'exec 3>p4/h.log; BASH_XTRACEFD=3; set -x; cd "$HOME"; echo 走过家目录; set +x' >/dev/null 2>&1
printf '  trace 里含绝对家目录的行数：演示脚本全用相对名=%s 行 ｜ 故意 cd 到家目录之后=%s 行\n' \
    "$(grep -c '/hom[e]/' p4/secret-trace.log)" "$(grep -c '/hom[e]/' p4/h.log)"
printf '  同一份折过一道之后还剩=%s 行\n' "$(fold < p4/h.log | grep -c '/hom[e]/')"
printf '  → trace 会把 cd 的目标、PWD、脚本绝对路径原样打出来；存档前先折一遍再落地，\n'
printf '    权限靠 umask 而不是事后 chmod —— 先建后改之间那一段是 644，谁都读得。\n'

echo
echo "=== 9. 这串 PS4 配方可复现吗：先让守卫响，再让它对上 ==="
cat > p4/recipe.sh <<'EOF'
#!/usr/bin/env bash
PS4='+[L${LINENO}] ${FUNCNAME[0]:-顶层} '
exec 9>p4/r.log
BASH_XTRACEFD=9
set -x
s=0
for i in 1 2; do
    s=$((s + i))
done
set +x
exec 9>&-
printf 's=%s ｜ trace 行数=%s\n' "$s" "$(wc -l < p4/r.log)"
EOF
bash p4/recipe.sh > p4/run1.txt 2>&1; cp p4/r.log p4/r1.log
bash p4/recipe.sh > p4/run2.txt 2>&1; cp p4/r.log p4/r2.log
sed 's/\[L6\]/[L99]/' p4/r1.log > p4/r3.log
printf '  ① 先看守卫能不能响：把 r3.log 里的 [L6] 改成 [L99]，与 r2.log diff 命中=%s 行（期望非 0）\n' \
    "$(diff p4/r3.log p4/r2.log | grep -c '^[<>]')"
diff p4/r1.log p4/r2.log > p4/tdiff.txt 2>&1
diff p4/run1.txt p4/run2.txt > p4/rdiff.txt 2>&1
printf '  ② 撤掉改动：两遍的 trace diff=%s 行 ｜ 两遍的 stdout diff=%s 行（都期望 0）｜ trace 各 %s/%s 行\n' \
    "$(grep -c '^[<>]' p4/tdiff.txt)" "$(grep -c '^[<>]' p4/rdiff.txt)" "$(wc -l < p4/r1.log)" "$(wc -l < p4/r2.log)"
printf '  ③ trace 头两条=[%s]\n' "$(head -2 p4/r1.log | fold | tr '\n' '/')"
printf '  ④ 屏上那行 stdout=[%s]\n' "$(fold < p4/run1.txt | tr '\n' '/')"
printf '  ⑤ 反向对照：往 PS4 里放一个每跑必变的值，两遍的 trace diff=%s 行（期望非 0）\n' \
    "$(bash -c "PS4='+[\$(date +%s%N)] '; set -x; echo 甲" >/dev/null 2>p4/v1.txt; \
       bash -c "PS4='+[\$(date +%s%N)] '; set -x; echo 甲" >/dev/null 2>p4/v2.txt; \
       diff p4/v1.txt p4/v2.txt | grep -c '^[<>]')"
printf '  → 行号、函数名、固定串：可对拍。时间、pid、随机名：立刻作废（⑤ 就是那条红线的可执行版本）。\n'
printf '    本章 README 引用的每一屏都出自 ② 这种对拍过的运行；⑤ 那种值一律不许上屏。\n'

echo
printf '[ps4fd.sh 自查] 本脚本格数=%s ｜ p4/ 里现有文件数=%s（跑完由 trap 整目录删除）\n' \
    "$(grep -c '^echo .=== ' "$0")" "$(find p4 -type f | wc -l)"
