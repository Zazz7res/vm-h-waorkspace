#!/usr/bin/env bash
# ============================================================================
# errmix.sh —— 函数用 stdout 带回数据时，stderr 是怎么把返回值搅浑的
#
#   demo.sh 里这样调：bash errmix.sh
#   安全边界：只在**当前目录**里开 mix/ 子目录（放三个用来统计词数的小文本），跑完由
#             trap 'rm -rf mix' EXIT 删掉；不起后台进程、不开端口、不碰任何用户配置文件；
#             要看「另一个干净 shell」一律用 bash -c 起子进程，开关只在那条命令里活着。
#   故意不加 set -e：这一支整篇都是失败现场 —— 字符串比较假、整数比较报错、退出码被 local
#             吞掉。这些都得「把原文打出来然后继续跑完」，中途一崩后面的格子就全没了。
#
# 本章立场：bash 函数只能还你一个退出码；想带回别的东西只有三条通道 —— stdout、全局变量、
#   nameref。这一支演第一条，并把它的代价量出来：
#     ① out=$(f) 只兜 stdout —— 你写进 stdout 的**每一个字节**都会变成返回值的一部分；
#     ② 命令替换是在**子 shell** 里跑的 —— 函数里改的全局变量传不出来；
#     ③ 通道既然是「一坨文本」，比较、算术、取首行就全在赌「里面只有一行」。
#
# 和第 3 节 retcode.sh 的分工：那一支演 return 与退出码 0~255 取模；这一支只看两个流的分合，
#   return 的数值游戏一律不碰（第 4、5 段只借 return 问一句「这个码是谁的」）。
# ============================================================================

rm -rf mix
mkdir -p mix
trap 'rm -rf mix' EXIT

printf 'alpha beta gamma\n' > mix/a.txt      # 3 个词
printf 'one\n'              > mix/b.txt      # 1 个词
printf 'x y\nz\n'           > mix/c.txt      # 3 个词

# 教科书形状：结果走 stdout，诊断走 stderr（关键就是那句末尾的 >&2）
count_words() {
    local f=$1
    printf '诊断：正在统计 %s\n' "$f" >&2
    if [ -r "$f" ]; then
        wc -w < "$f"
    else
        printf '诊断：读不到文件 %s\n' "$f" >&2
        return 1
    fi
}

# 三把尺子：字符数 / 词数 / 行数。只用「几个」这类每次都成立的关系，不量耗时。
chars() { printf '%s' "${#1}"; }
words() { printf '%s' "$(wc -w <<<"$1")"; }
lines() { printf '%s' "$(printf '%s\n' "$1" | wc -l)"; }

echo "=== 1. 现场：同一个函数，out=\$(f) 到底兜住了什么 ==="
printf '  原文：out=$(count_words mix/a.txt)\n'
printf '  下一条就是这个调用。注意屏幕上会先冒出诊断那行，它**不会**出现在 out 里：\n'
out=$(count_words mix/a.txt)
printf '  out=[%s] ｜ 字符数=%s ｜ 词数=%s ｜ 行数=%s\n' \
    "$out" "$(chars "$out")" "$(words "$out")" "$(lines "$out")"
printf '  → ★ 命令替换只接管函数的 1 号 fd。stderr 没被接，所以原路走到屏幕上 —— 这正是想要的效果。\n'
printf '    但别忘了它是「碰巧没接」，不是你写的规矩；下一段把三种接法各跑一次。\n'

echo
echo "=== 2. 三条分流的写法：原文与拿到的值 ==="
o1=$(count_words mix/a.txt 2>/dev/null)
printf '  ① 只要 stdout：out=$(f 2>/dev/null)\n'
printf '     out=[%s] 字符数=%s 词数=%s 行数=%s ｜ 这一条屏幕上没出现诊断\n' \
    "$o1" "$(chars "$o1")" "$(words "$o1")" "$(lines "$o1")"
o2=$(count_words mix/a.txt 2>&1)
printf '  ② 诊断也进来：out=$(f 2>&1)\n'
printf '     out=[%s] 字符数=%s 词数=%s 行数=%s ｜ ★ 结果那行被挤到了第 2 行\n' \
    "$(printf '%s' "$o2" | tr '\n' '/')" "$(chars "$o2")" "$(words "$o2")" "$(lines "$o2")"
e3=$(count_words mix/a.txt 2>&1 >/dev/null)
printf '  ③ 只要 stderr：err=$(f 2>&1 >/dev/null)   ← 两个重定向的顺序**不能**换\n'
printf '     err=[%s] 字符数=%s 行数=%s ｜ 结果（stdout）被丢进 /dev/null 了\n' \
    "$e3" "$(chars "$e3")" "$(lines "$e3")"
o4=$(count_words mix/a.txt >/dev/null 2>&1)
printf '  ④ 对照：写反成 out=$(f >/dev/null 2>&1)\n'
printf '     out=[%s] 字符数=%s ｜ 两手空空\n' "$o4" "$(chars "$o4")"
echo "  → ③ 就是第 04 章那条顺序事故在命令替换里重演：重定向从左往右生效，"
echo "    先执行 2>&1 时，2 号抄走的是「1 号那一刻的指向」= 命令替换的那根管道；"
echo "    随后 >/dev/null 才把 1 号改走。于是 stderr 留在了替换里、stdout 被扔掉。"
echo "    反过来写（④）就是两个流一起扔 —— 一个字符都拿不到。"

echo
echo "=== 3. ★ 事故现场：函数里报错也 echo 到 stdout，比较当场假 ==="
bad_count_words() {                       # 和 count_words 只差一个 >&2
    local f=$1
    printf '诊断：正在统计 %s\n' "$f"     # ★ 漏了 >&2，这一行从此就是「返回值」的一部分
    wc -w < "$f"
}
out3=$(bad_count_words mix/a.txt)
printf '  原文：out=$(bad_count_words mix/a.txt)，然后 if [ "$out" = "3" ]; then 认为是成功\n'
printf '  out=[%s]（两行）\n' "$(printf '%s' "$out3" | tr '\n' '/')"
same3=no; [ "$out3" = "3" ] && same3=yes
printf '  字符数=%s 词数=%s 行数=%s ｜ 判定 [ "$out" = "3" ] → %s\n' \
    "$(chars "$out3")" "$(words "$out3")" "$(lines "$out3")" \
    "$([ "$same3" = yes ] && echo 真 || echo 假)"
printf '  拿它做整数比较会怎样（放到子进程里跑，报错前缀才是干净的 bash:）：\n'
out="$out3" bash -c '[ "$out" -gt 0 ]; echo "退出码=$?"' 2>&1 | sed 's/^/    /'
printf '  → ★ 同一段诊断：走 stderr 只是「屏幕上多一行字」，走 stdout 就把整条通道搅浑了 ——\n'
printf '    比字符串会假、做算术会报错、取首行取到的是「诊断：正在统计」那一行、判空（[ -z ]）也判错。\n'
printf '  正解只有一条：函数里凡不是「要带回的值」的输出，一律 >&2。\n'
printf '  兜底（改不动别人的函数时）：只取末行 [%s] —— 但这等于在赌「诊断永远印在前面」。\n' "${out3##*$'\n'}"
# 把「什么都没算出来」的那一幕写成独立小脚本：报错前缀才会是相对路径 mix/empty.sh
cat > mix/empty.sh <<'PEOF'
f() {
    printf '诊断：这一趟什么也没算出来\n'      # 报错/诊断都写进了 stdout（事故写法）
    wc -w < mix/没有这个文件.txt               # 真正的结果：一个字节都没产生
}
out=$(f)
printf 'out=[%s] ｜ 字符数=%s ｜ 判空 [ -z "$out" ] → %s\n' \
    "$out" "${#out}" "$([ -z "$out" ] && echo 空 || echo 非空)"
PEOF
printf '  再狠一点：函数其实什么都没算出来，判空却说「有结果」（bash mix/empty.sh，故意不带 2>&1）：\n'
bash mix/empty.sh 2>&1 | sed 's/^/     /'
printf '  → 上面那行 wc 的报错原路走到了屏幕，而 out 里躺着的就是那句诊断 —— 通道里「非空」和「有结果」是两回事。\n'

echo
echo "=== 4. out=\$(f) 的退出码：赋值带得回码，披上 local 就没了 ==="
f3() { echo 数据; return 3; }
out4=$(f3); rc4=$?
printf '  原文：out=$(f3)   → 实测 $?=%s ｜ out=[%s] ← 简单赋值的退出码 = 命令替换里那条命令的\n' "$rc4" "$out4"
g4() { local v; v=$(f3); printf '  原文：v=$(f3)（先 local 声明、再单独赋值）→ 实测 $?=%s\n' "$?"; }
g4
h4() { local v=$(f3); printf '  原文：local v=$(f3)（声明与赋值写同一行）→ 实测 $?=%s ← ★ 码被吞了\n' "$?"; }
h4
if f3 >/dev/null 2>&1; then br4=then; else br4=else; fi
printf '  对照：if f3; then A; else B; fi 走的是 %s 分支（f3 说了 3）\n' "$br4"
echo "  → ★ 三件事分开记：想拿码就判断调用本身，或者写成「先声明再赋值」；"
echo "    local v=\$(f) 这个最顺手的写法，拿到的是 local 这个 builtin 自己的 0，f3 的 3 当场蒸发；"
echo "    脚本如果加了 set -e，这条坑更狠 —— 该炸的那一行根本不炸。"

echo
echo "=== 5. if f; then 的退出码是函数自己的，不是体里最后那句 echo 的 ==="
loose()  { echo 第一步; false; echo 第二步; }   # 中间的 false 被后面的 echo 洗掉
strict() { echo 值; false; }                    # 不写 return：码 = 最后执行的那条
endsay() { echo 值; return 9; }                 # 写了 return：码 = return 说的
for pair in 'loose|体里 false 之后还有一句 echo' 'strict|体里最后一条就是 false' 'endsay|最后一条是 echo，但函数 return 9'; do
    fn=${pair%%|*}
    note=${pair#*|}
    "$fn" >/dev/null 2>&1
    rc5=$?
    if "$fn" >/dev/null 2>&1; then br5=then; else br5=else; fi
    printf '  %s（%s）→ 退出码=%s ｜ if %s 走 %s 分支\n' "$fn" "$note" "$rc5" "$fn" "$br5"
done
echo "  → ★ 函数的退出码 = 体里**最后执行的那条命令**的状态，或者自己 return 的数。"
echo "    echo 几乎永远返回 0，所以「函数里挂了个 false / 挂了个读不到的文件」在 if f 面前等于没发生；"
echo "    反过来 endsay 说明另一半：码归函数说了算，最后一句 echo 的 0 拦不住 return 9。"

echo
echo "=== 6. 走 stdout 这条通道的结构性代价：命令替换是个子 shell ==="
counter=0
bump() { counter=$((counter + 1)); echo 结果; }
r6=$(bump)
printf '  原文：r6=$(bump) ｜ r6=[%s] ｜ 函数里明明 counter+1 了，外面的 counter=%s\n' "$r6" "$counter"
p6a=$(pwd)
r6b=$(cd mix && pwd)
p6b=$(pwd)
printf '  原文：r6b=$(cd mix && pwd) ｜ 替换里的 pwd 和外面的 pwd 相等吗 → %s（不等 = 里面确实 cd 成了）\n' \
    "$([ "$r6b" = "$p6b" ] && echo 相等 || echo 不相等)"
printf '  外面这个 shell 的当前目录变了吗 → %s（前一次 pwd 与后一次 pwd 相等吗：%s）\n' \
    "$([ "$p6a" = "$p6b" ] && echo 没变 || echo 变了)" "$([ "$p6a" = "$p6b" ] && echo 是 || echo 否)"
bump > /dev/null
printf '  对照：直接 bump（不套命令替换）→ 外面的 counter=%s ｜ 这就是「全局变量」那条通道的来路\n' "$counter"
echo "  → ★ 只要走 stdout，函数就在子 shell 里跑：它改的全局变量、它的 cd、它起的后台作业，"
echo "    全都传不出来。所以「既要带回值、又要留下副作用」这种需求 \$(f) 天生做不到 —— 那是"
echo "    全局变量或 nameref（见 localscope.sh）的活；退出码则永远走 return，见 retcode.sh。"

echo
echo "=== 7. 收口：判据与自证 ==="
printf '  删之前 mix/ 里有 %s 个文件 ｜ 现在执行 rm -rf mix（trap 里是同一句，这里提前跑一遍好自证）\n' \
    "$(ls -A mix | wc -l)"
rm -rf mix
printf '  ① 现场目录 mix/ 还在吗 → %s ｜ ls -A 里名为 mix 的条目数 = %s\n' \
    "$([ -d mix ] && echo 在 || echo 不在了)" "$(ls -A | grep -cx mix)"
printf '  ② 后台进程：本脚本没起过后台作业，在**自己的进程里**复核 jobs -p 行数 = %s\n' "$(jobs -p | wc -l)"
printf '  ③ 同一次调用、四种写法拿到的字符数：默认=%s ｜ ①=%s ｜ ②=%s ｜ ③=%s ｜ ④=%s\n' \
    "$(chars "$out")" "$(chars "$o1")" "$(chars "$o2")" "$(chars "$e3")" "$(chars "$o4")"
echo "  ④ 判据一：函数里每一句 printf/echo 先问一句「这是要带回的值，还是给人看的？」"
echo "     给人看的一律 >&2；漏一行进 stdout，比较、算术、取首行会同时中招。"
echo "  ⑤ 判据二：只想捞 stderr 就写 err=\$(f 2>&1 >/dev/null)，顺序不能反；"
echo "     反过来的 >/dev/null 2>&1 是两个流一起扔（第 2 段④实测字符数 0）。"
echo "  ⑥ 判据三：out=\$(f) 带得回退出码（实测 3），local v=\$(f) 带不回（实测 0）；"
echo "     \$(f) 里改不动外面的变量、cd 也带不出来。三条通道各有代价：stdout 干净但要子 shell，"
echo "     全局变量留得住但要自己擦干净，nameref 只回填一个已有变量 —— 选哪条看你要带回什么。"
