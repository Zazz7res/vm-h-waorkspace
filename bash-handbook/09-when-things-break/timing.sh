#!/usr/bin/env bash
# CHECK: RUN=非复现 理由：第 8 格 ③「同一句 time 连跑 6 次得到几个不同值」是对本机时钟与调度的真实测量，两趟可能是 3 也可能是 4；判据是它 > 1，屏上那行把期望值一并印出来了
#        （免除的证据只有「两遍逐字相同」这一条；rc、残留、隐私、每格的收口判定照判。）
# ============================================================================
# timing.sh —— 四把尺：内建 time、外部 /usr/bin/time、times、SECONDS
#
#   demo.sh 里这样调：bash timing.sh
#   安全：只在当前目录建 tm/（被演示的小脚本与对拍文件），跑完 trap 整目录删掉。
#         唯一一次等满一秒是第 6 格量 SECONDS 自增；其余计时对象都只等 0.05 秒。不联网、不碰 ~。
# ★ 这一支要还的账：time 看起来是一把尺，实测是四把，而且刻度互不相通：
#   内建 time 是个「关键字」，它的 TIMEFORMAT 只认五个字母，变量本身读出来还是未设；
#   /usr/bin/time 是另一个程序，它认 %M（内存），遇到不认的字母打 ?X 而不是报错；
#   times 与 SECONDS 连「这一次花了多久」都不回答，它们答的是累计。
# ★ 所有真实数字都不上屏：屏上只留糊过数字的形状与「认/不认」，第 8 格把这条钉成可对拍的证据。
# ============================================================================

rm -rf tm
mkdir -p tm
trap 'rm -rf tm' EXIT

fold() { sed -E "s#$HOME#<家目录>#g; s#\\b$USER\\b#<用户>#g; s#$(hostname)#<主机名>#g"; }
# mk：把一切数字（整数与小数）压成 N，只留格式骨架。这是「时间证据能上屏」的唯一形态。
mk() { sed -E 's/[0-9][0-9.]*/N/g'; }

echo "=== 1. time 有几重身：关键字、两个外部程序，还有一个内建 times ==="
printf '  type -a time →\n'
type -a time 2>&1 | fold | sed -E 's/^/      /'
printf '  command -v time=[%s] ｜ type -t time=[%s] ｜ type -t times=[%s]\n' \
    "$(command -v time)" "$(type -t time)" "$(type -t times)"
printf '  外部那个的版本首行=[%s]\n' "$(/usr/bin/time --version 2>&1 | head -1 | fold)"
printf '  内建 time 的输出走哪条流：stdout 行数=%s ｜ stderr 行数=%s\n' \
    "$(bash -c 'time sleep 0.05' 2>/dev/null | wc -l)" "$(bash -c 'time sleep 0.05' 2>&1 >/dev/null | wc -l)"
printf '  外部 time 不带任何参数走哪条流：stdout 行数=%s ｜ stderr 行数=%s\n' \
    "$(/usr/bin/time sleep 0.05 2>/dev/null | wc -l)" "$(/usr/bin/time sleep 0.05 2>&1 >/dev/null | wc -l)"
printf '  → 屏上三行说明「time」这个名字被三样东西接走：关键字（不进 PATH，command 找的是命令所以照样答 time，\n'
printf '    但 type -t 答 keyword）、/usr/bin/time 与 /bin/time（同一家的两个副本）。\n'
printf '    写 time 就是关键字，写 /usr/bin/time 才是那个程序；两把尺的输出都走 stderr ——\n'
printf '    所以把 time 接进管道之前先想清楚 stderr，否则量到的是「空气」（第 3 格演示 2>/dev/null）。\n'

echo
echo "=== 2. 默认 TIMEFORMAT：变量根本没设，但一打就是四行（首行空行 + tab 分隔）==="
printf '  父 shell 里读 TIMEFORMAT=[%s] ｜ 子进程里读=[%s]\n' \
    "${TIMEFORMAT-未设}" "$(bash -c 'printf %s "${TIMEFORMAT-未设}"')"
printf '  不设 TIMEFORMAT 时，内建 time 的原始字节（cat -A，$=行尾，^I=制表符）：\n'
bash -c 'time sleep 0.05' 2>&1 >/dev/null | mk | cat -A | sed -E 's/^/      /'
printf '  → 三件事：① 默认格式不住在 TIMEFORMAT 这个变量里（读出来是未设），它是编在 shell 里的；\n'
printf '    ② 输出第一行是个空行，所以 time 前面永远多一个空行 —— 拿 awk 逐行数会多数一行；\n'
printf '    ③ 名字与数字之间是制表符不是空格，把 time 的输出喂给按空格切词的字段号会全错。\n'

echo
echo "=== 3. 内建的字母表：52 个字母全扫一遍，只印认/不认 ==="
: > tm/scan.txt
for L in A B C D E F G H I J K L M N O P Q R S T U V W X Y Z \
         a b c d e f g h i j k l m n o p q r s t u v w x y z; do
    bash -c "TIMEFORMAT='x=%$L'; time :" >tm/o.txt 2>tm/e.txt
    if grep -q '^x=' tm/e.txt; then
        printf '%s 认\n' "$L" >> tm/scan.txt
    else
        printf '%s 不认\n' "$L" >> tm/scan.txt
    fi
done
printf '  认的字母：%s\n' "$(awk '$2=="认"{printf "%s ", $1}' tm/scan.txt)"
printf '  不认的字母（连排，共 %s 个）：%s\n' \
    "$(awk '$2=="不认"' tm/scan.txt | wc -l)" "$(awk '$2=="不认"{printf "%s", $1}' tm/scan.txt)"
printf '  一个不认的字母，报错原文（本机是中文语言环境，所以是中文）：[%s]\n' \
    "$(bash -c 'TIMEFORMAT="x=%M"; time :' 2>&1 >/dev/null | fold | head -1)"
printf '  写成两个百分号时打出来是字面百分号：[%s]\n' "$(bash -c 'TIMEFORMAT="q=%%R=%%"; time :' 2>&1 >/dev/null)"
printf '  TIMEFORMAT 设成空串之后的输出行数=%s（0 = 一个字都不打）\n' \
    "$(bash -c 'TIMEFORMAT=""; time sleep 0.05' 2>&1 >/dev/null | wc -l)"
printf '  认的那五个各自的形状（只把数字换成 #，小数位留着）：%s\n' \
    "$(for L in E P R S U; do printf '%s=[%s] ' "$L" "$(bash -c "TIMEFORMAT=\"x=%$L\"; time sleep 0.05" 2>&1 >/dev/null | tr '0123456789' '##########' | tr -d '\n')"; done)"
printf '  → 内建只认五个：R=实耗、U=用户 CPU、S=内核 CPU、P=CPU 占实比、E=实耗（另一种写法）；\n'
printf '    其余 47 个字母全部当场报「无效的格式字符」并且什么都不打 —— 网上抄来的 TIMEFORMAT="%%l%%m" 这类\n'
printf '    在别的实现里能用的写法，本机直接炸；想验自己的格式串有没有踩到，就用这一格的扫法。\n'
printf '    最隐蔽的是空串那一档：设了空串之后 time 一个字都不打，看着像「time 坏了」，其实是格式串被清空了。\n'

echo
echo "=== 4. 外部 /usr/bin/time 是另一套字母表：它认 %M，不认的打 ?X ==="
printf '  外部 time -f 里内建不认的那些字母（数字全糊）：[%s]\n' \
    "$(/usr/bin/time -f '%R %U %S %M %P %e %x' sleep 0.05 2>&1 | mk | fold | tr '\n' '/')"
printf '  同一个 %%M 交给内建：[%s]\n' "$(bash -c 'TIMEFORMAT="%M"; time sleep 0.05' 2>&1 >/dev/null | fold | head -1)"
printf '  外部 time 遇到不认的字母（内建会报错的那个位置）=[%s]\n' "$(/usr/bin/time -f '%l' true 2>&1 | fold)"
printf '  外部 time 不带 -f 的默认形状：行数=%s ｜ [%s]\n' \
    "$(/usr/bin/time sleep 0.05 2>&1 | wc -l)" "$(/usr/bin/time sleep 0.05 2>&1 | mk | fold | tr '\n' '/')"
printf '  外部 time -v 的行数=%s ｜ 里面有 Maximum resident set size 这一项吗=%s\n' \
    "$(/usr/bin/time -v true 2>&1 | wc -l)" "$(/usr/bin/time -v true 2>&1 | grep -c 'Maximum resident')"
printf '  → 两套字母表别抄串：%%M（峰值内存）只有外部那把尺给得出数字，内建拿到它只报错；\n'
printf '    坏字母的下场也不同：内建报错并整行不打，外部把 ?X 留在原位继续跑完。\n'
printf '    想要内存/换页/文件系统计数只能 /usr/bin/time；想要「不装东西、顺手一量」只能用内建。\n'

echo
echo "=== 5. time 的位置与脾气：量的是什么、什么情况下它照样打数 ==="
printf '  time 一个不存在的命令：[%s] rc=%s\n' \
    "$(bash -c 'TIMEFORMAT="R=%R"; time nosuchcmd-xyz' 2>&1 >/dev/null | fold | mk | tr '\n' '/')" \
    "$(bash -c 'time nosuchcmd-xyz' >/dev/null 2>&1; echo $?)"
printf '  time 一整块（花括号包两条命令）：[%s]\n' \
    "$(bash -c 'TIMEFORMAT="R=%R U=%U S=%S P=%P"; time { sleep 0.05; sleep 0.05; }' 2>&1 >/dev/null | mk | tr '\n' '/')"
printf '  time 一个函数调用（量的是整棵调用树）：[%s]\n' \
    "$(bash -c 'TIMEFORMAT="R=%R U=%U S=%S"; f(){ sleep 0.05; g; }; g(){ sleep 0.05; }; time f' 2>&1 >/dev/null | mk | tr '\n' '/')"
printf '  time 前面还有别的语句（echo 前; time ...）会不会报错：[%s]\n' \
    "$(bash -c 'TIMEFORMAT="R=%R"; echo 前; time sleep 0.05' 2>&1 | fold | mk | tr '\n' '/')"
printf '  → 三条实测：① 命令不存在时它照样打时间行（R=N），rc 还是 127 —— 时间行不是「跑成功了」的凭证；\n'
printf '    ② 花括号一块、函数调用都能量，量到的是整棵树加起来的用户/内核 CPU（具体数字每跑变，这里只留形状）；\n'
printf '    ③ time 是一条命令的前缀而不是独立语句：echo 前; time ... 合法，但 echo 前 与 R=N 是分开的两条流，\n'
printf '    最后一行把 stdout 也并进来了才看得到「前」—— 别把 time 当管道里的一环去接东西。\n'

echo
echo "=== 6. 另外三把尺：times 两行、SECONDS 自增、EPOCHREALTIME 带小数 ==="
printf '  times 的原始形状（值全糊，只留字段数）：\n'
bash -c 'sleep 0.05; times' 2>&1 | mk | sed -E 's/^/      /'
printf '  times 第一行=本 shell 自己的 user/sys，第二行=它等过的子进程累计 —— 都不回答「这一段花了多久」\n'
printf '  SECONDS：刚进来=%s ｜ sleep 0.05 之后=%s ｜ 置 0 再 sleep 0.05 之后=%s\n' \
    "$(bash -c 'printf %s "$SECONDS"')" \
    "$(bash -c 'sleep 0.05; printf %s "$SECONDS"')" \
    "$(bash -c 'SECONDS=0; sleep 0.05; printf %s "$SECONDS"')"
printf '  SECONDS 置 0 之后 sleep 1 的读数=%s（这一格是全脚本唯一一次真等一秒）\n' \
    "$(bash -c 'SECONDS=0; sleep 1; printf %s "$SECONDS"')"
printf '  EPOCHREALTIME 的形状（每个数字换成 N，看小数点）=[%s]\n' "$(bash -c 'printf %s "$EPOCHREALTIME"' | tr '0123456789' 'N')"
printf '  → 三把尺各答一个问题：times 答「这个 shell 一共花了多少 CPU」，SECONDS 答「从（可重设的）起点到现在几秒」，\n'
printf '    EPOCHREALTIME 答「现在几点，带小数」。\n'
printf '    屏上第二行就是坑：sleep 0.05 之后 SECONDS 还是 0 —— 它只有整秒，用它量 50 毫秒会得到 0，\n'
printf '    别把 0 读成「很快」；要量亚秒级只有内建 time 或 EPOCHREALTIME 前后相减这两条路。\n'

echo
echo "=== 7. TIMEFORMAT 会不会跟着进子进程：不 export 就不传 ==="
TIMEFORMAT='x=%R'
printf '  父里设了 TIMEFORMAT（没 export），子里读到=[%s]\n' "$(bash -c 'printf %s "${TIMEFORMAT-未设}"')"
printf '  显式加 env 前缀之后，子里读到=[%s]\n' "$(TIMEFORMAT='y=%R' bash -c 'printf %s "${TIMEFORMAT-未设}"')"
printf '  没传进去时子里 time 用的是默认格式，行数=%s\n' "$(bash -c 'time sleep 0.05' 2>&1 >/dev/null | wc -l)"
printf '  把一个非法格式传进子里（TIMEFORMAT=abc）整条 stderr=[%s]\n' \
    "$(TIMEFORMAT=abc bash -c 'time sleep 0.05' 2>&1 >/dev/null | fold | mk | tr '\n' '/')"
unset TIMEFORMAT
printf '  → 结论：TIMEFORMAT 是普通 shell 变量，不是选项 —— 不 export 就不进子进程，\n'
printf '    所以「父 shell 设好格式，脚本里的 time 还是老样子」是正常现象，得靠 export 或让脚本自己设；\n'
printf '    反过来说，随手把 TIMEFORMAT 导进环境也会污染子脚本，而这一格的污染是**静默的**：\n'
printf '    传进去的 abc 里一个百分号都没有，于是子里的 time 把「abc」原样打出来当成结果行——\n'
printf '    一个数字都没有、也不报错。格式串坏了不响，是这一格真正的坑（正菜那头的 sleep 照跑）。\n'

echo
echo "=== 8. 这些数字能不能进对拍：先让守卫响，再糊过对拍，最后数有几个不同值 ==="
CPU='s=0; for ((i=0;i<200000;i++)); do s=$((s+i)); done'
bash -c "TIMEFORMAT='R=%R U=%U S=%S P=%P'; time $CPU" 2>&1 >/dev/null | mk > tm/r1.txt
bash -c "TIMEFORMAT='R=%R U=%U S=%S P=%P'; time $CPU" 2>&1 >/dev/null | mk > tm/r2.txt
sed -E 's|^R=N|R-X|' tm/r2.txt > tm/r3.txt
printf '  ① 先看守卫能不能响：把第三份的 R=N 改成 R-X，与第二份 diff 命中=%s 行（期望非 0）\n' \
    "$(diff tm/r3.txt tm/r2.txt | grep -c '^[<>]')"
printf '  ② 糊过数字的两遍 diff=%s 行（期望 0）｜ 糊过的内容=[%s]\n' \
    "$(diff tm/r1.txt tm/r2.txt | grep -c '^[<>]')" "$(tr '\n' '/' < tm/r1.txt)"
: > tm/raw.txt
for i in 1 2 3 4 5 6; do
    bash -c "TIMEFORMAT='R=%R U=%U S=%S P=%P'; time $CPU" >> tm/raw.txt 2>&1
done
printf '  ③ 同一句连跑 6 次、原值不糊：不同值的个数=%s（期望 > 1，所以这 6 行一个字都不上屏）\n' \
    "$(sort -u tm/raw.txt | wc -l)"
printf '  → 三条读完就明白本章的规矩：时间数字能证明「跑过、量到了」，不能当断言；\n'
printf '    想把它写进 CI，只能糊成形状再来回对拍（② 这种），或者只断言「不同值 > 1」这种存在性（③）。\n'

echo
printf '[timing.sh 自查] 本脚本格数=%s ｜ tm/ 里现有文件数=%s（跑完由 trap 整目录删除）\n' \
    "$(grep -c '^echo .=== ' "$0")" "$(find tm -type f | wc -l)"
