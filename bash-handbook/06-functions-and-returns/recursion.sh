#!/usr/bin/env bash
# ============================================================================
# recursion.sh —— 递归在 bash 里靠什么成立：local、FUNCNEST、和调用栈
#
#   demo.sh 里这样调：bash recursion.sh
#   主线②：递归的每一层共用同一张调用栈，变量按**调用栈**查（动态作用域）。所以一个正确的
#   递归必须把所有中间变量都 local，否则内层往回退时改掉的同名全局，会把外层的算错。
#   深度由 bash 用栈扛：FUNCNEST=10 会当场拦下这次调用并报错（脚本继续活），不设 FUNCNEST
#   就只能等栈爆 —— 那一下打死的是**另一个 bash 进程**，本脚本不受影响。
#   代价用计数不用耗时：全程打「进来了多少次函数」这种每次一样的量，不掐表、不打 pid。
#   安全：只在**当前目录**建 rec/ 子目录（放两个「敢在子进程里炸」的递归小脚本：一个撞
#         FUNCNEST、一个真爆栈），跑完由 trap 'rm -rf rec' EXIT 删掉；不碰 ~、不碰绝对
#         /tmp、不开端口、不联网。爆栈那格只崩被 fork 出去的子 bash，本脚本当场收敛继续。
#   故意不加 set -e：递归炸出的错误值、被拦的非 0、段错误 rc=139 都是本章要看的现场。
#   ★ 本机不拿什么当证据：段错误时子 bash 崩前一个字都没吐（捕获字符数=0）、也没印递归
#     深度（那数每跑必变）；只报「崩没崩 / 退出码几 / 本脚本还活不活」这类每次都成立的事实。
# ============================================================================

rm -rf rec
mkdir -p rec
trap 'rm -rf rec' EXIT

echo '=== 1. 漏掉 local 的递归会炸：回来时 n 被内层改掉 ==='
# 两版结构一模一样，唯一差别是中间变量 n 有没有 local；累加器 res 两版共用同一个全局
res=1
bad_run()  { n=$1;       [ "$n" -le 1 ] && return; bad_run  $((n - 1)); res=$((res * n)); }
good_run() { local n=$1; [ "$n" -le 1 ] && return; good_run $((n - 1)); res=$((res * n)); }
res=1; bad_run 4
printf '  漏 local 版 bad_run 4 → res=%s（正解应是 24）｜ 跑完外层那个全局 n=[%s]\n' "$res" "$n"
res=1; good_run 4
printf '  正确版   good_run 4 → res=%s\n' "$res"
echo '  → ★ 判据：漏 local 那版只有一个全局 n；递归到最底层时 n 被写成 1，回退路上每一层'
echo '    乘的都是那个被改剩的 1，于是 4*3*2*1 实际算成了 1*1*1*1=1。正确版每层有自己的 local n。'

echo
echo '=== 2. FUNCNEST：本机不设 = 只受栈限制；设 10 会当场拦下这次调用（脚本继续活）==='
printf '  本脚本进程里 echo $FUNCNEST → [%s]（空 = 没设上限，递归只能靠栈来扛）\n' "${FUNCNEST-}"
printf '  FUNCNEST 是普通 shell 变量、不 export，所以每个新起的 bash 里它都是空的。\n'
printf '  在一个子进程脚本里设 FUNCNEST=10 再往深里递归（只活在那条命令里）：\n'
printf 'deep(){ deep "$(( $1 + 1 ))"; }\nFUNCNEST=10\ndeep 0\necho "deep 这次调用退出码=$?"\necho "被拦之后脚本还在跑：reach=after"\n' > rec/nest10.sh
err=$(env LC_ALL=C.UTF-8 bash rec/nest10.sh 2>&1); rc=$?
printf '%s\n' "$err" | sed 's/^/    /'
printf '  子进程整体退出码=%s ｜ 那句「超出最大函数嵌套层数」是 bash 的原文（这里强制取英文，好逐字复现）\n' "$rc"
echo '  → FUNCNEST 拦下来时：那次超限的调用返回非 0（上面「deep 这次调用退出码=1」），但 bash 报一句就'
echo '    放过脚本继续跑，末尾那句 echo 成功 → 整个子进程退出码又是 0。拦你，不等于打死你。'

echo
echo '=== 3. 不设 FUNCNEST、真递归到爆：崩的是子进程，本脚本不受影响 ==='
printf 'deep(){ deep "$(( $1 + 1 ))"; }\ndeep 0\n' > rec/runaway.sh
cap=$(env LC_ALL=C.UTF-8 bash rec/runaway.sh 2>&1); rc2=$?
printf '  让子 bash 无限递归（不设 FUNCNEST），捕获它的输出与退出码：\n'
printf '    崩溃前 bash 吐出的字符数=%s（0 = bash 一个字都没说，直接撞栈）\n' "${#cap}"
printf '    子进程退出码=%s ← ★ 139 = 128+11，11 是 SIGSEGV（段错误），是内核砍掉了那个孩子\n' "$rc2"
printf '  本脚本走到这一行还活着 → 是（爆栈只打死了 fork 出去的那个 bash，没打满整台机器的栈）\n'
echo '  → 拿「子进程退出码 139 + 崩前零输出」当证据就够了；递归深度计数每跑必变，不印上屏。'

echo
echo '=== 4. 递归的代价用计数：fib 进来了多少次函数 ==='
calls=0
ret=0
fib() {
    calls=$((calls + 1))        # 计数器必须是全局：为什么见下面两条
    local n=$1
    if [ "$n" -le 1 ]; then ret=$n; return; fi
    fib $((n - 1)); local a=$ret
    fib $((n - 2)); local b=$ret
    ret=$((a + b))
}
calls=0; ret=0; fib 5;  printf '  fib 5  → 结果=%s ｜ 函数被进入 %s 次\n' "$ret" "$calls"
calls=0; ret=0; fib 10; printf '  fib 10 → 结果=%s ｜ 函数被进入 %s 次\n' "$ret" "$calls"
echo '  为什么计数器是全局不是 local：local calls 让每层各数各的、回退时清零，总数就丢了。'
echo '  为什么不用 v=$(fib ...)：命令替换在子 shell 里跑，里面 calls 怎么加都带不回父进程 —— 实测：'
lost=0
g() { lost=$((lost + 1)); echo 值; }
v=$(g)
printf '    g 里给 lost 加了一次，用 v=$(g) 调 → 外层 lost=%s（加在子 shell 里，回不来）\n' "$lost"

echo
echo '=== 5. local 的作用域由调用栈决定：三层同名 x 各是什么 ==='
walk() {
    local d=$1
    local x="层${d}的x"
    printf '  下去 深度=%s 本地 x=[%s]\n' "$d" "$x"
    [ "$d" -lt 3 ] && walk $((d + 1))
    printf '  回来 深度=%s 本地 x=[%s]\n' "$d" "$x"
}
walk 1
echo '  → 下去印 1→2→3、回来印 3→2→1：每层的 local x 只在自己那一帧有效，深度=2 那层从头到尾没碰过层 3 的 x。'

echo
echo '=== 6. 递归 + 命令替换：v=$(f 5) 把每一层的 echo 全拼进来 ==='
f6() { echo "第 $1 层的 echo"; [ "$1" -le 0 ] && return; f6 $(( $1 - 1 )); }
v6=$(f6 5)
printf '  捕获到的行数=%s（f6 5 一路 echo 到第 0 层，全被同一个 $( ) 接住）\n' "$(printf '%s\n' "$v6" | wc -l)"
printf '%s\n' "$v6" | sed 's/^/    /'
printf '  对比「每层自己捕获」的写法（内层被 local s=$(...) 截走，外层只剩自己那行）：\n'
g6() { local s=""; [ "$1" -gt 0 ] && s=$(g6 $(( $1 - 1 ))); echo "第 $1 层"; }
v6b=$(g6 3)
printf '    捕获到的行数=%s ｜ 内容=[%s]\n' "$(printf '%s\n' "$v6b" | wc -l)" "$(printf '%s' "$v6b" | tr '\n' /)"
echo '  → 谁没被下一层的 $( ) 接走，谁的 echo 就一路冒到最外层那次捕获 —— 递归函数里 echo 要格外当心。'

echo
echo '=== 7. 收口 ==='
printf '  删之前 rec/ 里有 %s 个文件 ｜ 现在 rm -rf rec ｜ 之后名为 rec 的条目数=%s\n' \
    "$(ls -A rec | wc -l)" "$(rm -rf rec; ls -A | grep -cx rec)"
printf '  自证：本脚本作业表 jobs -p 行数=%s（第 3 段那个是用 $( ) 起命令替换，跑完即收，没留后台）\n' "$(jobs -p | wc -l)"
echo '  ① 递归必须把所有中间变量都 local：漏一个，内层回退时就把外层那个同名全局改了（第 1 段 4! 算成 1）。'
echo '  ② 深度由栈扛：FUNCNEST=10 会让那次超限调用返回 1（脚本继续、末尾 echo 让它整体退 0），'
echo '     不设就只能等段错误 rc=139，且崩的只是 fork 出去的那个 bash，本进程照活。'
echo '  ③ 代价用计数不用 time：fib(10) 进了 177 次函数；计数要放全局，且不能套在 $( ) 里，否则带不回来。'
echo '  ④ 递归函数里的 echo 会被最外层 $( ) 一路接走（第 6 段捕获 6 行）：想只拿最外层那个值就别让中间层 echo。'
