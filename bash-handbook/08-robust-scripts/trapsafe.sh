#!/usr/bin/env bash
# ============================================================================
# trapsafe.sh —— trap 的响与不响，取决于「谁在什么上下文里响」
#
#   demo.sh 里这样调：bash trapsafe.sh
#   安全：只在当前目录建 trapsafe/ 子目录（第 9 格的清理现场用 trapsafe/work/ 里两个真文件），
#         跑完由 trap 整目录删掉；不碰 ~、不写绝对 /tmp、不联网、不装包、不起常驻后台进程；
#         信号只发给 bash -c 起来的那个子 bash 自己（kill -TERM/-INT/-9 $$ 或 $BASHPID），不碰别人的 pid。
#   故意不开 set -e：本脚本必须活到打印 rc 那一行；要演示 -e 一律写进 bash -c 里面。
#
# ★ 本机不拿「文档说 trap 会怎样」当证据，也不拿「我以为信号一定怎样」当证据。
#   每格走 run/rnd 助手，它只干三件事：① 把喂给子 shell 的那串命令**原样**打上屏（标签=真命令，
#   上一批次真栽过「写着 -E 其实没传」，所以这条是硬要求）；② 在全新子进程里跑，本格的陷阱绝不
#   串到下一格；③ 当场打印 rc 并抄进台账，最后一格一起收账。陷阱没响也是证据，必须连 rc 一起打。
#   dash 的格子走 rnd；两边的报错原文（中文英文）一字不改地打出来。
# ============================================================================

rm -rf trapsafe
mkdir -p trapsafe/work
trap 'rm -rf trapsafe' EXIT
: > trapsafe/rcs.txt

run() {
    printf '  敲的是：%s\n' "$1"
    bash -c "$1" 2>&1 | sed 's/^/    /'
    code=${PIPESTATUS[0]}
    printf '  rc=%s\n' "$code"
    printf '%s\n' "$code" >> trapsafe/rcs.txt
}
rnd() {
    printf '  敲的是（dash）：%s\n' "$1"
    dash -c "$1" 2>&1 | sed 's/^/    /'
    code=${PIPESTATUS[0]}
    printf '  rc=%s\n' "$code"
    printf '%s\n' "$code" >> trapsafe/rcs.txt
}
R='trap "echo 〔ERR〕" ERR'
E='trap "echo 〔EXIT〕码=\$?" EXIT'

echo "=== 1. 陷阱体里的 \$? 什么时候被读：双引号当场展开，转义了才留到执行时 ==="
run 'trap "echo 〔EXIT〕码=$?" EXIT; exit 3'
run "$E; exit 3"
run "$E; false; echo tail"
printf '  → 直觉以为「第一格会打印码=3」：屏幕上却是**码=0 而 rc=3**。因为写陷阱那一刻双引号里的\n'
printf '    $? 就已经被展开成一个具体数字（陷阱体里根本没有 $? 这东西）；把 \$? 留到执行时才有第 2 格。\n'
printf '  ★ 就算留到执行时（第 3 格），正常收尾读到的还是 0：中间那次 false 的码早被 echo 抹平了。\n'

echo "=== 2. ERR 陷阱在顶层响一声，响完照跑；停不停是 set -e 的事 ==="
run "$R; false; echo tail"
run "set -e; $R; false; echo tail"
printf '  → 直觉以为「挂了 ERR 陷阱就等于出错就停」：第一格响了〔ERR〕，tail 照打、rc=0。\n'
printf '  ★ ERR 只负责响，-e 只负责停；少挂 ERR 就没声音，少挂 -e 就不会停，两件事互不顶替。\n'

echo "=== 3. 四格并排：-E 有没有 × -e 有没有，函数里的失败到底响不响 ==="
run "$R; h(){ false; echo 体内; }; h"
run "set -E; $R; h(){ false; echo 体内; }; h"
run "set -e; $R; h(){ false; }; h"
run "set -e; set -E; $R; h(){ false; }; h"
printf '  → 直觉以为「陷阱挂了就会响」：不带 -E 的第 1、3 格**一声不响** —— ERR 陷阱不继承进函数。\n'
printf '  ★ 第 3 格最阴：脚本因为 -e 死掉了（rc=1），但你连一声响都没听到，日志里只剩一条不明所以的退出。\n'
printf '    加 -E 才响；第 2 格又说明 -E 并不管停不停（体内照打、rc=0）。两件衣服各挡各的事故。\n'

echo "=== 4. ERR 陷阱跟 -e 共享那四位豁免：&& 、|| 、if 里的失败不响 ==="
run "$R; false && echo y; echo tail"
run "$R; false || echo r; echo tail"
run "$R; if false; then echo T; else echo F; fi"
printf '  → 直觉以为「false 失败了就一定被陷阱抓到」：三格〔ERR〕一次都没响（tail/r/F 都打了）。\n'
printf '  ★ 条件上下文里的失败是「拿来判断的」不是「要报的」—— 别指望 ERR 陷阱替你兜底记一笔。\n'

echo "=== 5. ★ 陷阱体里第一条赋值就把 \$? 吞了（全章最贵的一格）==="
run 'trap "x=1; echo \"码=\$? x=\$x\"" ERR; false'
run 'trap "__rc=\$?; echo \"码=\$__rc\"" ERR; false'
printf '  → 直觉以为「陷阱体里读 $? 当然是失败的码」：第一格打的是 **码=0**（同时 x=1）——\n'
printf '    它前面那句 x=1 自己成功了，$? 当场被改写成 0，等 echo 去读已经晚了。\n'
printf '  ★ 修法只有一个动作：把 __rc=$? 写成陷阱体的**第一条语句**（第 2 格码=1）。顺序就是语义。\n'

echo "=== 6. trap - ERR 撤掉之后，同样的失败再也听不见 ==="
run "$R; false; trap - ERR; false; echo tail"
printf '  → 两次 false 只响一声：第二声之前 trap - ERR 把陷阱摘了，tail 照打、rc=0。\n'
printf '  → 这是「这一段我不想让它响」的正解（临时静音），但摘除是全局的，容易忘了装回去。\n'

echo "=== 7. 被信号杀死的时候 EXIT 跑不跑：TERM / INT / KILL 三种死法 ==="
run 'trap "echo 〔EXIT〕" EXIT; kill -TERM $$; echo tail'
run 'trap "echo 〔EXIT〕" EXIT; kill -INT $$; echo tail'
run 'trap "echo 〔EXIT〕" EXIT; kill -9 $$; echo tail'
printf '  → 直觉以为「挂了 EXIT 就一定收得到尾」：TERM(143)、INT(130) 时 EXIT 还跑，但 tail 没跑（主线被拦腰截）；\n'
printf '    -9 那一格屏幕上**一个字都没有**。143=128+15、130=128+2：rc 大于 128 就是被信号干掉的。\n'
printf '  ★ SIGKILL 拦不住，所以 trap 一律作废 —— 清理不能只靠 trap EXIT，临时目录要能容忍「上一趟没扫干净」。\n'

echo "=== 8. 一旦给 TERM 挂了陷阱，这个信号就杀不动你了 ==="
run 'trap "echo 〔TERM〕" TERM; trap "echo 〔EXIT〕" EXIT; kill -TERM $$; echo tail'
printf '  → 第 7 格 TERM 是拦腰截，这格〔TERM〕响完 tail 照打、EXIT 收尾、rc=0：陷阱顶掉了默认动作。\n'
printf '  ★ 看着安全其实更危险：管理器发 TERM 它不走，只能升级到 SIGKILL，然后就轮到第 7 格的「一个字都没有」。\n'
printf '    挂了 TERM 陷阱的脚本必须以 exit 收场（自己 exit 时 EXIT 陷阱照样收到那个码）。\n'

echo "=== 9. ★ 清理跑到一半被第二个信号打断：残留几个文件，当场数给你看 ==="
touch trapsafe/work/A trapsafe/work/B
run 'cd trapsafe/work; trap "echo 清①删A; rm -f A; kill -TERM $BASHPID; echo 清②删B; rm -f B" EXIT; echo 主体'
left=$(ls -A trapsafe/work | paste -sd ' ' -)
printf '  被打断这趟之后 trapsafe/work/ 还剩 %s 个：%s\n' "$(printf '%s' "$left" | grep -c '[^ ]')" "$left"
printf '  → 屏幕上〔清①删A〕打了（陷阱确实进去了），〔清②删B〕没打 —— 半截清理，文件 B 还在原地。\n'
touch trapsafe/work/A trapsafe/work/B
run 'cd trapsafe/work; trap "echo 清①一次删两个; rm -f A B" EXIT; echo 主体'
left2=$(ls -A trapsafe/work | grep -c '^[AB]$')
printf '  改成一条 rm -f A B（一次外部命令，中间没有可插进去的间隙）之后还剩 %s 个\n' "$left2"
printf '  ★ 计数守卫先证明它会响：被打断那趟 = 1（不是恒零），收尾那趟 = %s。半截清理比没清理更难查，\n' "$left2"
printf '    因为「trap 跑过了」在屏幕上有证据，而没跑完的那半截没有任何证据。\n'

echo "=== 10. dash 认不认这一套（两种报错原文都照打）==="
rnd "$E; exit 3"
rnd "$R; false; echo tail"
rnd 'set -o pipefail; false | true; echo 管道码=$?'
rnd 'arr=(1 2); echo 到不了这里'
rnd 'a=1; echo ${a[@]}'
printf '  → EXIT 陷阱 dash 认（码=3、rc=3）；ERR 陷阱 dash 报「trap: ERR: bad trap」，但 tail 照打、rc=0。\n'
printf '  ★ pipefail 在 dash 0.5.12 上**是认的**（管道码=1；不开时同一格是 0，见 strictfour.sh）；\n'
printf '    数组则是两样报法：声明 arr=(1 2) 当场「Syntax error: "(" unexpected」，光读 ${a[@]} 报\n'
printf '    「Bad substitution」（本格 rc=2）。「/bin/sh 就是阉割版 bash」这句老话两头都不能一概而论。\n'

echo "=== 11. 收账：上面每一条的真实退出码 ==="
printf '  按出场顺序的码：%s\n' "$(paste -sd ' ' trapsafe/rcs.txt)"
n=$(wc -l < trapsafe/rcs.txt)
z=$(grep -c '^0$' trapsafe/rcs.txt)
nz=$(grep -vc '^0$' trapsafe/rcs.txt)
sg=$(awk '$1>=128' trapsafe/rcs.txt | wc -l)
printf '  条数=%s ｜ 码 0 的=%s ｜ 非 0 的=%s ｜ 其中 >128（被信号杀）的=%s\n' "$n" "$z" "$nz" "$sg"
printf '  两个加起来等于条数吗：%s\n' "$([ "$((z + nz))" -eq "$n" ] && echo 等于 || echo 不等于)"
printf '  ★ 「>128」这个数会不会是恒零的假守卫？塞一条 143 进台账再撤掉：\n'
cp trapsafe/rcs.txt trapsafe/rcs.bak
printf '143\n' >> trapsafe/rcs.txt
ring=$(awk '$1>=128' trapsafe/rcs.txt | wc -l)
mv trapsafe/rcs.bak trapsafe/rcs.txt
back=$(awk '$1>=128' trapsafe/rcs.txt | wc -l)
printf '    塞进去 → 从 %s 变成 %s（响了）；撤掉探针回到 %s\n' "$sg" "$ring" "$back"
printf '  → 三条能带走的话：① 陷阱响不响看上下文（条件位不响、不带 -E 不进函数、-9 一律不响）；\n'
printf '    ② 陷阱体第一条就得把 $? 存下来，否则它被自己的赋值吃掉 —— 写陷阱时还得留神 $? 有没有被提前展开；\n'
printf '    ③ 清理要么一条命令做完，要么挂了 TERM 就必须 exit 收场：半截清理比没清理更难查。\n'
exit 0
