#!/usr/bin/env bash
# ============================================================================
# trapreturn.sh —— return 和 exit 到底在什么场合生效，trap 把这件事放大成什么
#
#   demo.sh 里这样调：bash trapreturn.sh
#   安全：只在**当前目录**里开 traps/ 子目录（放临时脚本和用来被 source 的文件），跑完由
#         trap 'rm -rf traps' EXIT 删掉；不起后台进程、不开端口、不碰任何配置文件；
#         临时文件一律用相对路径调用，所以 bash 的报错原文里也漏不出沙箱的绝对路径。
#   故意不加 set -e：这一支专门收集非 0 和「当场结束」的现场，中途一停后面的对照全没了。
# 主线①的延伸：retcode.sh 量清了 return 只能交出一个 0~255 的码；这一支问**生效场合** ——
#   函数外面能不能 return、被 source 的文件里能不能 return、EXIT 处理器里再 exit 会不会自己递归、
#   functrace 开与不开让 RETURN / DEBUG 差多少次。
# ★ 本机不拿耗时、pid 当证据：每条结论都是「计数 / 报错原文 / 最后一行出现没出现」，连跑两遍逐字一样；
#   报错文字跟着本机语言环境（zh_CN → 中文），逐行核对要在同一环境下跑。
# ============================================================================

rm -rf traps
mkdir -p traps
trap 'rm -rf traps' EXIT

# grid <标签> <bash 的参数...>：起一个干净子进程跑它，输出逐行加缩进原样上屏，末尾报退出码
grid() { local label=$1 out rc; shift
    out=$(bash "$@" 2>&1); rc=$?
    printf '  %s ｜ 退出码=%s\n' "$label" "$rc"; printf '%s\n' "$out" | sed 's/^/    /'; }

echo "=== 1. return 写在函数外面：bash 自己的原话（逐字照打，一个字没改） ==="
cat > traps/top.sh <<'PEOF'
echo "  〔脚本第 1 行〕"
return 5
echo "  〔脚本最后一行〕"
PEOF
grid '① bash traps/top.sh（脚本顶层第 2 行是 return 5）' traps/top.sh
grid '② bash -c "return 3"（连脚本文件这层皮都没有）' -c 'return 3'
echo '  → ★ 它报错，但它不拦路：第 1 行和最后一行都照打，脚本整体退 0 —— 那个 2 只属于 return 那一句自己。'
echo '    两句原文里都有「被 source 的脚本」五个字，那就是第 6 节的入口：换个加载方式，同一条 return 就合法。'

echo
echo "=== 2. EXIT 处理器里写 exit / 写 return / 什么都不写：三种下场 ==="
cat > traps/h.sh <<'PEOF'
c=0
case "${1-none}" in
    exit)   trap 'c=$((c + 1)); echo "  处理器进场：这是第 $c 次；下一句我自己 exit 3"; exit 3' EXIT ;;
    return) trap 'echo "  处理器进场；下一句是 return 4"; return 4; echo "  ★ return 之后这句居然也跑了"' EXIT ;;
    *)      trap 'echo "  处理器看到的退出码 = $?（主干写的也是 exit 9）"' EXIT ;;
esac
echo "  主干：exit 9"
exit 9
PEOF
grid '① 处理器里 exit 3' traps/h.sh exit
grid '② 处理器里 return 4（此刻没有函数上下文）' traps/h.sh return
grid '③ 处理器什么都不写' traps/h.sh none
echo '  → ★ ①只进场 1 次：EXIT 处理器里再 exit，bash 不让 EXIT trap 自己递归（真递归会印第 2 次），但它把最终'
echo '    码顶成了 3、主干那个 9 作废。②就是第 1 节那句原文错：报完照样跑，码还是 9。③是另一半：处理器里的 $?'
echo '    恰好等于主干交出来的码 —— 收尾时能知道自己是「因为什么」被叫起来的。'

echo
echo "=== 3. DEBUG trap：一次函数调用让它加几次，全看 functrace 开不开 ==="
cat > traps/dbg.sh <<'PEOF'
[ "${1-off}" = on ] && set -o functrace
d=0
trap 'd=$((d + 1))' DEBUG
b1() { :; }; b2() { :; :; }; b3() { :; :; :; }
m=$d; b1; x1=$((d - m))
m=$d; b2; x2=$((d - m))
m=$d; b3; x3=$((d - m))
echo "  functrace=$1 ｜ 调一次函数让计数加了多少：体里 1 条 → +$x1 ｜ 2 条 → +$x2 ｜ 3 条 → +$x3"
PEOF
grid '① 关掉 functrace' traps/dbg.sh off
grid '② 开着 functrace' traps/dbg.sh on
echo '  → ★ 关着时恒等于 2（「调用那一行」+「紧接着那一行」），函数体里 1 条还是 3 条命令**都不算数**；'
echo '    开着时差值是「体里命令数 + 1」。多出来那 1 次是什么？把处理器改成会自报家门的，当场问它：'
grid '③ 体里 2 条命令的函数被调一次，逐次印出触发点' -c 'set -o functrace; trap "d=\$((d + 1)); echo \"    DEBUG 第 \$d 次 ← 即将执行 [\${BASH_COMMAND}] ｜ 此刻在函数 [\${FUNCNAME[0]:-不在函数里}]\"" DEBUG; b2() { :; :; }; b2'
echo '    ★ 第 1、2 次是同一句 [b2] 被追了两遍，第二遍的 FUNCNAME 已经是 b2 —— 多出的那次发生在**进函数的'
echo '      那一刻**，不是函数返回；而处理器自己的「赋值 + echo」没再触发自己（不然 4 次会变成十几次）。'
grid '④ DEBUG 会不会截胡函数的码' -c 'trap "true" DEBUG; f(){ return 7; }; f; echo "  函数的码还是 $?"'

echo
echo "=== 4. RETURN trap：functrace 不开时函数返回一次都不触发，source 反倒触发 ==="
printf '%s\n' 'x=1' > traps/inner.sh
cat > traps/ret.sh <<'PEOF'
[ "${1-off}" = on ] && set -o functrace
trap 'r=$((r + 1)); echo "    第 $r 次触发 ← 刚从 [${FUNCNAME[0]:-不在函数里（source 结束了）}] 返回"' RETURN
l3() { :; }; l2() { l3; }; l1() { l2; }; k() { :; }; j() { k; k; }
r=0; l1; echo "  A｜三层嵌套 l1→l2→l3 跑完：触发 $r 次"
r=0; j; echo "  B｜一个 j 把同一个 k 调了两次：触发 $r 次"
r=0; . traps/inner.sh; echo "  C｜只 source 一个文件、不调任何函数：触发 $r 次"
PEOF
grid '① 关掉 functrace' traps/ret.sh off
grid '② 开着 functrace' traps/ret.sh on
echo '  → ★ 关着：A、B 全是 0 —— 「挂了 RETURN trap 就替你盯着每个函数」是误解，默认它谁都不管；'
echo '    可 C 照样触发 1 次（source 结束算一次，这条不要 functrace）。开着：A=3，一层一次，顺序是'
echo '    l3→l2→l1（最里层先回）；B 也是 3 —— 它数的是**实际返回了几回**，不是代码里写了几个函数。'

echo
echo "=== 5. 裸 return 拿谁的码，以及 return / exit 的「最后一行出现没出现」 ==="
grid '① 体里一条 false 然后裸 return' -c 'f(){ false; return; }; f; echo "  上一条是 false → 码=$?"'
grid '② 体里一条 true 然后裸 return' -c 'f(){ true; return; }; f; echo "  上一条是 true → 码=$?"'
grid '③ 体里一条 false|true 然后裸 return' -c 'f(){ false | true; return; }; f; echo "  上一条是这条管道 → 码=$?（没开 pipefail，只看最后一段）"'
A=$(bash -c 'c=0; trap "c=\$((c + 1)); echo BYE\$c" EXIT; f(){ return 3; }; f; echo 最后一行' 2>&1); ra=$?
B=$(bash -c 'c=0; trap "c=\$((c + 1)); echo BYE\$c" EXIT; f(){ exit 4;   }; f; echo 最后一行' 2>&1); rb=$?
printf '  函数里写 return → 外层跑到最后一行了吗 → %s ｜ EXIT 处理器进场 %s 次 ｜ 脚本退出码=%s\n' "$(case "$A" in *最后一行*) echo 是 ;; *) echo 否 ;; esac)" "$(printf '%s' "$A" | grep -c BYE)" "$ra"
printf '  函数里写 exit   → 外层跑到最后一行了吗 → %s ｜ EXIT 处理器进场 %s 次 ｜ 脚本退出码=%s\n' "$(case "$B" in *最后一行*) echo 是 ;; *) echo 否 ;; esac)" "$(printf '%s' "$B" | grep -c BYE)" "$rb"
echo '  → 用「最后一行有没有出现」这个二值就够判了：return 只结束这次调用、调用方还在；exit 连调用方一起结束。'
echo '    可两版的 EXIT 处理器都进场 1 次 —— ★ 别指望用 return 跳过收尾，也别以为 exit 会绕过 EXIT trap；'
echo '    分清它俩看的是「之后还有没有代码可跑」，不是「trap 跑没跑」。裸 return 交的就是上一条命令的码。'

echo
echo "=== 6. 被 source 的文件里：return 5 与 exit 5 的下场完全不同 ==="
printf '%s\n' 'echo "  〔被 source 的文件：开头这句〕"' 'return 5' 'echo "  〔文件里 return 之后的这句〕"' > traps/src_ret.sh
printf '%s\n' 'echo "  〔被 source 的文件：开头这句〕"' 'exit 5'   'echo "  〔文件里 exit  之后的这句〕"'   > traps/src_exit.sh
. traps/src_ret.sh
printf '  ① 外层跑 . traps/src_ret.sh（文件里 return 5）→ 外层还活着吗 → 是（这行就是外层打的）｜ 码=%s\n' "$?"
B6=$(bash -c '. traps/src_exit.sh; echo "  外层：我还活着"' 2>&1); r6=$?
printf '  ② 外层跑 . traps/src_exit.sh（文件里 exit 5）：这格不敢在本脚本里演，放进子进程：\n'
printf '%s\n' "$B6" | sed 's/^/    /'
printf '  ★ 子进程屏幕上有「我还活着」吗 → %s ｜ 那个子进程的退出码=%s\n' "$(case "$B6" in *我还活着*) echo 有 ;; *) echo 没有 ;; esac)" "$r6"
echo '  → source 进来的文件就是当前 shell 自己：里面 return 是「这段结束」，外面接着活；里面 exit 是'
echo '    「这个 shell 没了」—— 写脚本的人最常在这里把整场跑批一起带走。同一条 return，加载方式不同'
echo '    就从第 1 节那句报错变成这里合法的「结束本段」，这就是报错原文里那五个字的分量。'

echo
echo "=== 7. 收口 + 自证 ==="
echo '  ① return 只能在函数里或被 source 的文件里用；在函数外面用就是第 1 节那句原文错，但脚本不会停。'
echo '  ② EXIT 处理器里 exit 不会递归（进场实测 1 次），却会顶掉最终退出码；处理器里写 return 等于白写。'
echo '  ③ RETURN trap 默认完全不跟函数返回（但跟 source 结束）；开了 functrace 才是「每返回一层算一次」。'
echo '  ④ DEBUG 的代价是「每条命令一次」，函数体里的命令归不归它管同样看 functrace（多的那次在进函数时）。'
printf '  删之前 traps/ 里的文件数=%s ｜ 现场目录还在吗 → %s\n' "$(ls -A traps | wc -l)" "$([ -d traps ] && echo 在 || echo 不在了)"
rm -rf traps
printf '  现在 traps/ 还在吗 → %s ｜ 当前目录里名为 traps 的条目数=%s ｜ 本脚本作业表行数=%s\n' "$([ -d traps ] && echo 在 || echo 不在了)" "$(ls -A | grep -cx traps || true)" "$(jobs -p | wc -l)"
