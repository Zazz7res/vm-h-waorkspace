#!/usr/bin/env bash
# ============================================================================
# debugtrap.sh —— DEBUG 陷阱：什么时候响、响几次、它的返回值会不会把脚本打死
#
#   demo.sh 里这样调：bash debugtrap.sh
#   安全：只在当前目录建 d9/（被演示的小脚本、计数器文件、marker），跑完 trap 整目录删掉。
#         全用相对文件名；不碰 ~、不写绝对 /tmp、不联网、不 sleep、不起后台（第 4 格那次 & 立刻 wait 收走）。
# ★ 这一支要还的账：DEBUG 陷阱看起来是「每条命令前打一行日志」，但它同时是三把刀：
#   ① 它在 ( )、$( )、source 进来的文件里**根本不响** —— 日志会静默缺一块；
#   ② 它的返回值在 extdebug 下决定「这条命令跑不跑」—— 写错一个 return 就是全场跳过；
#   ③ 它失败时能把穿了 -e 的脚本当场掐死 —— 探针把自己测死了。
# ★ 三把刀都有能响的守卫：每格先给真次数，再给「期望值」，对不上就是这一章的 bug。
# ============================================================================

rm -rf d9
mkdir -p d9
trap 'rm -rf d9' EXIT

fold() { sed -E "s#$HOME#<家目录>#g; s#\\b$USER\\b#<用户>#g; s#$(hostname)#<主机名>#g"; }
pfx() { fold | sed 's/^/      /'; }

# dt <标签> <被测代码> [排序后join]：把「设了 DEBUG 陷阱 + 被测代码」落成文件跑一遍，只报计数与内容。
# 第三个参数只影响「屏上」那一格怎么拼：内容涉及并发（后台）时先后由调度决定，先排序再连起来才跨趟可比。
dt() {
    printf '%s\n' '#!/usr/bin/env bash' "trap 'printf \"〔D〕\$BASH_COMMAND\\n\"' DEBUG" > d9/case.sh
    printf '%s\n' "$2" >> d9/case.sh
    local out body
    out=$(bash d9/case.sh 2>&1 | fold)
    body=$out
    [ "${3:-}" = 排序后join ] && body=$(printf '%s\n' "$out" | LC_ALL=C sort)
    printf '  %-16s 〔D〕=%s 行 ｜ 正菜=%s 行 ｜ 屏上=[%s]\n' "$1" \
        "$(printf '%s\n' "$out" | grep -c 〔D〕)" \
        "$(printf '%s\n' "$out" | grep -v 〔D〕 | grep -c .)" \
        "$(printf '%s\n' "$body" | tr '\n' '/')"
}

echo "=== 1. 最小形状：BASH_COMMAND 与 LINENO 各自指向谁 ==="
cat > d9/one.sh <<'EOF'
#!/usr/bin/env bash
trap 'printf "〔D〕第%s行：%s\n" "$LINENO" "$BASH_COMMAND"' DEBUG
a=1
b=$((a + 1))
EOF
bash d9/one.sh 2>&1 | pfx
printf '  → 陷阱自己写在第 2 行，可 LINENO 打出来是 3、4 —— DEBUG 陷阱里的 LINENO 指向**即将执行的那条命令**，\n'
printf '    不是陷阱那一行。这条读法决定了能不能拿它做「行号级」的日志。\n'

echo
echo "=== 2. 哪些容器里它不响：三种静默缺失排成一列 ==="
dt '普通赋值'        'v=1'
dt '{ } 分组'        '{ echo 花括号; }'
dt '( ) 子 shell'    '( echo 圆括号 )'
dt '命令替换'        'v=$(echo 替换里)'
dt '管道两端'        'echo 甲 | cat'
dt '函数调用'        'f(){ echo 函数体; }; f'
dt 'source 进来的'   'printf "%s\n" "被source的一句" > d9/src.sh; source ./d9/src.sh'
dt 'bash -c 新进程'  'bash -c "echo 新bash"'
dt '后台 &'          'echo 后台 & wait' 排序后join
printf '  → 三个静默缺失分别是「( ) 子 shell」「命令替换」「source 进来的」这三行：\n'
printf '    ( ) 与 $( ) 里那句 echo 一个字都不响（只有顶层那条赋值/命令响了一次），\n'
printf '    source 进来的文件里那句也不响 —— 只有「调用 source」这一条响。管道两端却都响。\n'
printf '    含义很实际：拿 DEBUG 陷阱做全量审计日志，子 shell 与 source 的内容会**整块消失且不报错**。\n'
printf '  ★ 最后一行「后台 &」的屏上那格是把几行内容**排序之后**连起来的：正菜 `后台` 由子进程打印，\n'
printf '    它落在 〔D〕wait 之前还是之后由调度决定，两趟跑出来可能就是两个顺序 —— 计数（2 行 / 1 行）不受影响，\n'
printf '    顺序受影响。所以想把这种行印进可复现的文档，要么排序要么只印计数（第 13 节第 9 格是同一件事）。\n'

echo
echo "=== 3. 进函数要哪件衣服：-E 不管，-T 才管 ==="
run3() {
    printf '%s\n' '#!/usr/bin/env bash' "$1" "trap 'printf \"〔D〕\$BASH_COMMAND\\n\"' DEBUG" 'f(){ echo 体内; }' 'f' > d9/o3.sh
    out=$(bash d9/o3.sh 2>&1 | fold)
    printf '  %-18s 〔D〕=%s 行 ｜ 屏上=[%s]\n' "${2:-裸跑}" "$(printf '%s\n' "$out" | grep -c 〔D〕)" "$(printf '%s\n' "$out" | tr '\n' '/')"
}
run3 '' '裸跑'
run3 'set -E' '穿了 -E（errtrace）'
run3 'set -T' '穿了 -T（functrace）'
run3 'set -o functrace' '穿了 functrace 学名'
printf '  → -E 让的是 ERR 陷阱进函数，对 DEBUG 一个字都不改（两格 〔D〕 数相同）；\n'
printf '    -T / functrace 才让 DEBUG（和 xtrace）进函数体。第 08 章那句「-E 只管 ERR」在这里再钉一遍。\n'

echo
echo "=== 4. 复合命令响几次：if / for / while / && / || 的真实次数 ==="
cat > d9/cpx.sh <<'EOF'
#!/usr/bin/env bash
trap 'printf "〔D〕%s\n" "$BASH_COMMAND"' DEBUG
if true; then
    echo if里
fi
for i in 1 2; do
    echo 循环$i
done
while false; do :; done
true && echo 与右边
false || echo 或右边
EOF
bash d9/cpx.sh 2>&1 | pfx
printf '  计数：〔D〕=%s 行 ｜ 正菜=%s 行\n' "$(bash d9/cpx.sh 2>&1 | grep -c 〔D〕)" "$(bash d9/cpx.sh 2>&1 | grep -v 〔D〕 | grep -c .)"
printf '  → 三条结论：① if / for / while 这些**关键字自己不算一条命令**，条件那头的 true/false 才算；\n'
printf '    ② for 那一行每轮都重放一次（两轮就是两行），body 每轮也各一行；\n'
printf '    ③ while false 只响一次（条件失败就直接结束，body 从没执行也没被预告）。\n'
printf '    所以「陷阱次数 = 命令条数」里的「命令」是**执行过的简单命令**，不是源码行数（对照 xtrace.sh 第 9 格）。\n'

echo
echo "=== 5. 用 DEBUG 陷阱计数，读数那句自己也算一条 ==="
cat > d9/cnt.sh <<'EOF'
#!/usr/bin/env bash
: > d9/n.txt
trap 'printf "x\n" >> d9/n.txt' DEBUG
:
:
:
EOF
bash d9/cnt.sh
printf '  3 条命令跑完，n.txt 行数=%s\n' "$(wc -l < d9/n.txt)"
cat > d9/cnt2.sh <<'EOF'
#!/usr/bin/env bash
: > d9/n2.txt
trap 'printf "x\n" >> d9/n2.txt' DEBUG
:
:
:
n=$(wc -l < d9/n2.txt)
printf '  陷阱里再数一遍：%s\n' "$n"
EOF
bash d9/cnt2.sh
printf '  → 把「读数」放在另一个进程里，文件就是干净的 3 行；放进同一个脚本，读到的是 4。\n'
printf '    多的那一行不是漏跑：DEBUG 在**每条命令执行之前**都响一次，「读数这条命令」自己也响了一次才轮到它执行，\n'
printf '    所以同进程里读到的数天生比命令条数多一。要么减一，要么像屏上那样把读数挪到另一个进程里去数。\n'

echo
echo "=== 6. 陷阱体失败 + set -e：探针把自己掐死了 ==="
cat > d9/die.sh <<'EOF'
#!/usr/bin/env bash
set -e
trap 'false' DEBUG
echo 第一句
echo 第二句
EOF
cat > d9/alive.sh <<'EOF'
#!/usr/bin/env bash
trap 'false' DEBUG
echo 第一句
echo 第二句
EOF
printf '  穿 -e：屏上=[%s] ｜ rc=%s\n' "$(bash d9/die.sh 2>&1 | fold | tr '\n' '/')" "$(bash d9/die.sh >/dev/null 2>&1; echo $?)"
printf '  不穿 -e：屏上=[%s] ｜ rc=%s\n' "$(bash d9/alive.sh 2>&1 | fold | tr '\n' '/')" "$(bash d9/alive.sh >/dev/null 2>&1; echo $?)"
printf '  → 同一段陷阱体，一个当场死（第一句都没印出来，rc=1）、一个全跑完（rc=0）。\n'
printf '    原因是 -e 眼里「即将执行的命令」包含陷阱本身。写 DEBUG 日志时末尾留一个恒真的语句（比如 :\n'
printf '    或者干脆用 echo），不然严格模式脚本会被自己的日志杀掉 —— 这是本章最贵的一格。\n'

echo
echo "=== 7. extdebug：陷阱的末态决定那条命令跑不跑 ==="
cat > d9/skip.sh <<'EOF'
#!/usr/bin/env bash
shopt -s extdebug
trap 'false' DEBUG
echo 一
echo 二
: > d9/marker.txt
echo 三
EOF
printf '  不穿 -e + extdebug + 陷阱恒假：屏上=[%s] ｜ rc=%s ｜ marker.txt 建出来了吗=%s\n' \
    "$(bash d9/skip.sh 2>&1 | fold | tr '\n' '/')" "$(bash d9/skip.sh >/dev/null 2>&1; echo $?)" \
    "$(bash d9/skip.sh >/dev/null 2>&1; [ -e d9/marker.txt ] && echo 在 || echo 不在)"
rm -f d9/marker.txt
cat > d9/keep.sh <<'EOF'
#!/usr/bin/env bash
shopt -s extdebug
trap 'printf "〔预演〕%s\n" "$BASH_COMMAND"' DEBUG
echo 一
echo 二
EOF
printf '  陷阱末态为真（printf 成功返回 0）：屏上=[%s] ｜ rc=%s\n' \
    "$(bash d9/keep.sh 2>&1 | fold | tr '\n' '/')" "$(bash d9/keep.sh >/dev/null 2>&1; echo $?)"
printf '  → extdebug 把 DEBUG 陷阱变成 preexec：**末态非 0 就跳过那条命令**，脚本不报错、也不停，\n'
printf '    只是所有命令都不执行了（marker.txt 没建出来 = 连「建文件」那条都被跳过）。\n'

echo
echo "=== 8. 陷阱里那句 return：报错归报错，非 0 归非 0 ==="
cat > d9/ret.sh <<'EOF'
#!/usr/bin/env bash
shopt -s extdebug
trap 'return 0' DEBUG
echo 一
echo 二
EOF
printf '  顶层 DEBUG 陷阱里写 return 0：屏上=[%s] ｜ rc=%s\n' \
    "$(bash d9/ret.sh 2>&1 | fold | tr '\n' '/')" "$(bash d9/ret.sh >/dev/null 2>&1; echo $?)"
cat > d9/fnret.sh <<'EOF'
#!/usr/bin/env bash
shopt -s extdebug
f() { trap 'return 0' DEBUG; echo 在函数里设的; }
f
EOF
printf '  同样一句放进函数里设：屏上=[%s]\n' "$(bash d9/fnret.sh 2>&1 | fold | tr '\n' '/')"
printf '  → 第一格：return 在「顶层执行的陷阱体」里是非法的。这句报错跟着语言环境走，两种语言各跑一次：\n'
printf '    钉好的中文=[%s]\n    换英文=[%s]\n' \
    "$(bash -c 'return 0' 2>&1 | head -1)" \
    "$(LANGUAGE=C LC_ALL=C.UTF-8 LANG=C.UTF-8 bash -c 'return 0' 2>&1 | head -1)"
printf '    两句字面完全不同、末态都是非 0 —— 所以这一格的判据挑码与「正菜有没有少」，不挑那句话长什么样。\n'
printf '    要紧的是这条报错本身就是非 0 末态 → extdebug 把每条命令都跳过了，屏上正菜一个字都没有；\n'
printf '    想 return 0 表示「放行」，结果连 return 自己都没跑成。正确写法是不用 return、让末条语句的码当信号（第 9 格）。\n'

echo
echo "=== 9. 精准跳过一条：preexec 过滤的正解 ==="
cat > d9/filt.sh <<'EOF'
#!/usr/bin/env bash
shopt -s extdebug
trap '[[ "$BASH_COMMAND" != "echo 二" ]]' DEBUG
echo 一
echo 二
echo 三
EOF
printf '  过滤器只放行「不是 echo 二」的：屏上=[%s] ｜ rc=%s\n' \
    "$(bash d9/filt.sh 2>&1 | fold | tr '\n' '/')" "$(bash d9/filt.sh >/dev/null 2>&1; echo $?)"
printf '  守卫先要能响：把过滤条件写成恒真（!= ""）时，三条是不是都跑了=[%s]\n' \
    "$(bash -c 'shopt -s extdebug; trap "[[ \"\$BASH_COMMAND\" != \"\" ]]" DEBUG; echo 一; echo 二; echo 三' 2>&1 | fold | tr '\n' '/')"
printf '  → 只有「一、三」上屏，「二」被跳过且没有任何报错 —— 这就是 extdebug 的 preexec；\n'
printf '    它和「命令不存在」在屏上长得完全不同：跳过是少一行，报错是多一行。做拦截脚本时这条区别要写进测试。\n'

echo
echo "=== 10. 把调用栈整条打出来：三个数组按同一个下标配，而行号归「上一层的文件」 ==="
cat > d9/stack.sh <<'SKEOF'
#!/usr/bin/env bash
p() {
    local i src srcb line txt hit
    printf '  数组长度=%s（脚本里只定义了 p a b c 四个函数，多出来那项是 #%s %s）\n' \
        "${#FUNCNAME[@]}" "$(( ${#FUNCNAME[@]} - 1 ))" "${FUNCNAME[ ${#FUNCNAME[@]} - 1 ]}"
    for i in "${!FUNCNAME[@]}"; do
        src="${BASH_SOURCE[$((i + 1))]:-}"
        line="${BASH_LINENO[$i]}"
        if [ -z "$src" ]; then
            txt=
        else
            txt=$(awk -v ln="$line" 'NR==ln' "$src" 2>/dev/null)
        fi
        if [ -z "$txt" ]; then
            hit=读不到那行
        elif [[ "$txt" == *"${FUNCNAME[$i]}"* ]]; then
            hit=有
        else
            hit=没有
        fi
        srcb="${src##*/}"
        printf '  #%s %-5s ← 由 %s 在 %s 的第 %s 行叫起 ｜ 那一行原文=[%s] ｜ 有我的名字吗=%s ｜ 我定义在=%s\n' \
            "$i" "${FUNCNAME[$i]}" "${FUNCNAME[$((i + 1))]:-（上面没有了）}" \
            "${srcb:-（无文件）}" "$line" "$txt" "$hit" "${BASH_SOURCE[$i]##*/}"
    done
    printf '  ② 把行号配成 BL[i-1]（整列错一位）会打成下面这样：\n'
    for i in "${!FUNCNAME[@]}"; do
        src="${BASH_SOURCE[$i]:-}"
        line="${BASH_LINENO[$((i - 1))]:-?}"
        if [ -z "$src" ]; then txt=; else txt=$(awk -v ln="$line" 'NR==ln' "$src" 2>/dev/null); fi
        if [ -z "$txt" ]; then
            hit=读不到那行
        elif [[ "$txt" == *"${FUNCNAME[$i]}"* ]]; then
            hit=有
        else
            hit=没有
        fi
        printf '     #%s %-5s 会说在第 %s 行 ｜ 那一行原文=[%s] ｜ 有我的名字吗=%s\n' \
            "$i" "${FUNCNAME[$i]}" "$line" "$txt" "$hit"
    done
}
a() { p; }
b() { a; }
c() { b; }
c
SKEOF
bash d9/stack.sh 2>&1 | fold | tee d9/stack.out >/dev/null
cat d9/stack.out
g1=$(sed -n '/②/q;p' d9/stack.out | grep -c '有我的名字吗=有')
g2=$(sed -n '/②/,$p' d9/stack.out | grep -c '有我的名字吗=有')
g3=$(sed -n '/②/,$p' d9/stack.out | grep -cE '有我的名字吗=(没有|读不到那行)')
printf '  对账：正配段 %s 格「有」（4 个函数帧全对才算对）%s ｜ 错配段 %s 格「有」、%s 格露馅\n' \
    "$g1" "$( [ "$g1" -eq 4 ] && echo →对上了 || echo →对不上★ )" "$g2" "$g3"
printf '  同一份文件的行号清单（拿它逐行核对上面那几个数）：\n'
nl -ba d9/stack.sh | sed -E 's/^/      /'
cat > d9/libx.sh <<'LXEOF'
probe() {
    local bl bs0 bs1 t0 t1
    bl="${BASH_LINENO[0]}"; bs0="${BASH_SOURCE[0]}"; bs1="${BASH_SOURCE[1]}"
    t0=$(awk -v ln="$bl" 'NR==ln' "$bs0" 2>/dev/null)
    t1=$(awk -v ln="$bl" 'NR==ln' "$bs1" 2>/dev/null)
    printf '  行号 BL[0]=%s ｜ 我定义在 BS[0]=%s ｜ 叫起我的那句在 BS[1]=%s\n' "$bl" "${bs0##*/}" "${bs1##*/}"
    printf '  两种取法读同一号行：拿 BS[0] 读=[%s]\n' "$t0"
    printf '                    拿 BS[1] 读=[%s]\n' "$t1"
    printf '  哪一读真的含有这句调用 probe 吗：BS[0] 读=%s ｜ BS[1] 读=%s\n' \
        "$( [[ "$t0" == *probe* ]] && echo 含 || echo 不含 )" "$( [[ "$t1" == *probe* ]] && echo 含 || echo 不含 )"
}
LXEOF
cat > d9/cross.sh <<'CXEOF'
. ./d9/libx.sh
mid() { probe; }
top() { mid; }
top
CXEOF
printf '  ③ 函数分住两个文件时（库 d9/libx.sh、主脚本 d9/cross.sh），两种文件取法只差一个下标：\n'
bash d9/cross.sh 2>&1 | fold
printf '  → 三句话：\n'
printf '    ① 正配段这样读才对：**FUNCNAME[i] 由 FUNCNAME[i+1] 叫起，叫起地点是「BASH_SOURCE[i+1] 的第 BASH_LINENO[i] 行」**；\n'
printf '      而 BASH_SOURCE[i] 说的是这个函数**定义在哪个文件**。上面那份 stack.sh 里两件事恰好同名（四个函数都定义在\n'
printf '      stack.sh），所以拿 BS[i] 取原文这一格看不出破绽 —— 屏上「我定义在」那一列与「在 … 的第 N 行叫起」里的文件名现在是一样的。\n'
printf '    ② 把行号配成 BL[i-1]（错配段）：每一格报的行其实是上一格那个帧的调用点，往后每格错一位，\n'
printf '      可每一格指到的仍是文件里真实存在的一行 —— 这种「像模像样」的错位最难查。对账给的 %s 与 %s 就是它的实底：\n' "$g2" "$g3"
printf '      五格里三格照样是「有」（错配后指到的都是 a() { p; } 这种定义行，函数名本来就写在上面），\n'
printf '      只有 #0（说在第 0 行）和末行 main（指到那句裸 c）露馅。光看「有 / 没有」抓不住中段错位，得拿上面那份 nl 清单逐格对。\n'
printf '    ③ 函数一旦分住两个文件，BS[i] 这个取法就当场读错：屏上两读里只有一读含得到那句调用，\n'
printf '      含不到的那一读给的是**另一个文件**里同号的一行 —— 它照样是一行真实代码，所以不会报错，只会把你对到别的文件里去。\n'
printf '    这一串不依赖 extdebug，也不依赖 -T —— 它只在**函数里**读，正是 DEBUG 陷阱进不去的地方（第 3 格）。\n'

echo
echo "=== 11. DEBUG 与 ERR 并排：一次失败，两边各响几次 ==="
cat > d9/both.sh <<'EOF'
#!/usr/bin/env bash
set -E
trap 'printf "〔D〕%s\n" "$BASH_COMMAND"' DEBUG
trap 'printf "〔ERR〕码=%s 挂在=%s 行=%s\n" "$?" "$BASH_COMMAND" "$LINENO"' ERR
f() { false; }
f
echo 还在
EOF
bash d9/both.sh 2>&1 | pfx
printf '  计数：〔D〕=%s 行 ｜ 〔ERR〕=%s 行\n' "$(bash d9/both.sh 2>&1 | grep -c 〔D〕)" "$(bash d9/both.sh 2>&1 | grep -c 〔ERR〕)"
sed 's/^set -E$/set +E/' d9/both.sh > d9/both2.sh
printf '  同一份改成 set +E：〔D〕=%s 行 ｜ 〔ERR〕=%s 行 ｜ 屏上=[%s]\n' \
    "$(bash d9/both2.sh 2>&1 | grep -c 〔D〕)" "$(bash d9/both2.sh 2>&1 | grep -c 〔ERR〕)" \
    "$(bash d9/both2.sh 2>&1 | fold | tr '\n' '/')"
printf '  → ERR 那一头穿 -E 时响两次（函数体里的 false + 外面那句调用 f），不穿就只响外面那一次；\n'
printf '    DEBUG 那一头跟着 -E 完全不改变（第 3 格已钉），它响的次数只由「执行过几条简单命令」决定。\n'
printf '    两把探针在同一次失败上给的行号不一样：ERR 的 LINENO 指失败点，DEBUG 的指即将执行点 —— 别混着读。\n'

echo
printf '[debugtrap.sh 自查] 本脚本格数=%s ｜ d9/ 里现有文件数=%s（跑完由 trap 整目录删除）\n' \
    "$(grep -c '^echo .=== ' "$0")" "$(find d9 -type f | wc -l)"
