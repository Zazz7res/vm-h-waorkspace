#!/usr/bin/env bash
# ============================================================================
# status_chain.sh —— `$?` 是怎么被「传下来」又「半路丢掉」的
#
# 这个文件在演什么：
#   ① 谁能改写 $?（echo、:、纯赋值、(( ))、case 没命中、[[ ]]……）
#   ② 命令替换 $( ) 吃退出码的两种姿势，以及 $? 和 $( ) 写在同一行时的先后
#   ③ 函数的退出码 = 函数体最后一条命令的退出码（前面的失败全部蒸发）
#   ④ local v=$(失败命令) 为什么能把 set -e 骗过去
#   ⑤ 管道里 $? 只属于最后一段，全部真相在 ${PIPESTATUS[*]}
#   ⑥ 「if cmd」和「if [ $? -eq 0 ]」的实测差别：中间插一句话，后者当场作废
#
# 为什么要演：
#   bash 里没有「异常」，只有一个 0~255 的整数在命令之间传来传去。
#   你以为它传到了，其实它已经被路过的一条 echo 改写过。
#   这些结论不带数字就没有意义 —— 下面每一行都打印出当时的 $?。
#
# 安全说明：只在自己所在目录（demo.sh 已把 cwd 切进 mktemp 沙箱）造 sc_ 前缀小文件。
# ============================================================================
set -u

line() { printf '  %-40s %s\n' "$1" "$2"; }     # line '现场说明' '结论'

printf '\n──── 1. $? 只活到「下一条命令」为止\n'
false
line 'false 之后立刻取'          "\$? = $?"
false; echo "    （中间插了一句 echo）"
line '同上，再取'                "\$? = $?（echo 成功了，码被换成 0）"
false; :
line 'false 之后紧跟 : （空操作）'  "\$? = $?（: 自己固定返回 0）"
false
x=5
line 'false 之后跑纯赋值 x=5'     "\$? = $?（赋值本身也算一条命令）"
false
[[ 1 = 2 ]]
line 'false 之后跑一个假的 [[ ]]' "\$? = $?"
false
[ 1 = 2 ]
line 'false 之后跑一个假的 [ ]'   "\$? = $?"

printf '\n──── 2. 几个容易看走眼的改写者\n'
( exit 3 );     line '( exit 3 ) 子 shell'          "\$? = $?"
( false; true ); line '( false; true ) 组里最后一条赢' "\$? = $?"
( exit 3; true ); line '( exit 3; true )：exit 之后没人跑' "\$? = $?"
false; if true; then :; fi
line 'false 之后跑 if true; then :; fi' "\$? = $?（then 分支改写）"
false; case nomatch_key in other_key) : ;; esac
line 'false 之后 case 一个分支都没命中' "\$? = $?（空跑也算成功）"
false; for i in ; do :; done
line 'false 之后跑一个空 for 循环'     "\$? = $?"
(( 1 ));  line '(( 1 )) 算术结果非零'   "\$? = $?"
(( 0 ));  line '(( 0 )) 算术结果为零'   "\$? = $?（这就是「失败」）"
v=$(( 5 - 5 )); line 'v=$(( 5 - 5 )) 只是展开' "\$? = $?（展开不产生码，赋值产生 0）"

printf '\n──── 3. 命令替换 $( ) 吃退出码\n'
false; a=$(true)
line 'false; a=$(true)'          "\$? = $?"
false; b=$(false)
line 'false; b=$(false)'         "\$? = $?（替换里的失败会传给赋值）"
true
echo "  同一行从左到右：$?  ←  $(false)  →  $?"
echo "  ↑ 左边的数字是替换**之前**的码，右边的是替换**之后**的：\$( ) 一跑完，\$? 就换人了"
echo "  把失败包进 echo：$(false)   ← 这一句里替换失败了"
line '上面那条 echo 整条的退出码' "\$? = $?（echo 自己成功 = 0，替换里的 1 被吞）"

printf '\n──── 4. local / export / declare 一律返回 0（set -e 的头号帮凶）\n'
f_local() {
    local v rc                    # 先只声明，避免干扰后面那行的码
    local v=$(false)              # ← 大家最爱写的一行：声明和赋值同一行
    rc=$?                         # 立刻存下来，否则下一条命令就把它改了
    line '  函数里 local v=$(false) 之后' "存下来的 rc = $rc"
    return "$rc"
}
f_local
line 'f_local 整个函数的退出码'   "\$? = $?（local 把替换的 1 换成了 0，return 拿到的就是 0）"
g_split() {
    local v rc
    v=$(false)                    # ← 正确写法：声明和赋值分成两条命令
    rc=$?
    line '  函数里分开写之后'      "存下来的 rc = $rc"
    return "$rc"
}
g_split
line 'g_split 整个函数的退出码'   "\$? = $?（分开写才保住了替换的 1）"
false; export SC_JUST_A_NAME=1
line 'false 之后 export NAME=1'   "\$? = $?（export 永远 0）"
false; declare -a SC_ARR=(1)
line 'false 之后 declare -a …'    "\$? = $?"

printf '\n──── 5. 函数的退出码 = 函数体最后执行的那条命令\n'
h_mid() { false; true; }
h_mid;  line 'h_mid(){ false; true; }'   "\$? = $?（中间的 false 没人记得）"
h_tail() { true; false; }
h_tail; line 'h_tail(){ true; false; }'  "\$? = $?（最后一条决定一切）"
h_echo() { false; echo "    （h_echo 里打印了这一行）"; }
h_echo; line 'h_echo(){ false; echo …; }' "\$? = $?（echo 才是最后一行）"
h_ret() { false; return 9; }
h_ret;  line 'h_ret(){ false; return 9; }' "\$? = $?（return 是显式判据）"
h_if() { if true; then false; fi; }
h_if;   line 'h_if(){ if true; then false; fi; }' "\$? = $?"
h_if2() { if false; then :; fi; }
h_if2;  line 'h_if2(){ if false; then :; fi; }'   "\$? = $?（条件假、没 else → 整条 if 是 0）"

printf '\n──── 6. 管道：$? 只是最后一段的，全部真相在 PIPESTATUS\n'
false | true;  line 'false | true' "\$?=$?  PIPESTATUS=[${PIPESTATUS[*]}]"
true | false;  line 'true | false' "\$?=$?  PIPESTATUS=[${PIPESTATUS[*]}]"
printf 'alpha\n' > sc_x.txt
grep -q alpha sc_x.txt | grep -q zzz sc_x.txt
line 'grep 命中 | grep 不命中' "\$?=$?  PIPESTATUS=[${PIPESTATUS[*]}]"
sleep 0.1 | nonexistent_prog_03 | true
line '真命令 | 不存在的命令 | true' "\$?=$?  PIPESTATUS=[${PIPESTATUS[*]}]"
( set -o pipefail; false | true )
line '在子 shell 里开 pipefail 再 false|true' "\$? = $?"
{ set -o pipefail; false | true; }
line '在当前 shell 里用 { } 开 pipefail'       "\$? = $?（括号组不 fork，开关留下来了）"
false | true
line '再跑一次 false|true（没再开开关）'       "\$? = $?（证明 pipefail 还在生效）"
set +o pipefail
false | true
line 'set +o pipefail 之后再跑'                "\$? = $?（回到「只看最后一段」）"

printf '\n──── 7. 「if cmd」和「if [ $? -eq 0 ]」的实测差别\n'
grep -q zzz sc_x.txt
if [ $? -eq 0 ]; then echo "  ① 紧贴着写：走到 then"; else echo "  ① 紧贴着写：走到 else（\$?=1，判对了）"; fi
grep -q zzz sc_x.txt
echo "  （中间就插了这么一句人畜无害的 echo）"
if [ $? -eq 0 ]; then echo "  ② 隔了一句再判：走到 then ← 判据已被上面的 echo 改写成 0"; else echo "  ② 走到 else"; fi
if grep -q zzz sc_x.txt; then echo "  ③ if grep -q …：走到 then"; else echo "  ③ if grep -q …：走到 else ← 推荐：条件就是命令本身"; fi
