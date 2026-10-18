#!/usr/bin/env bash
# ============================================================================
# errescue.sh —— set -e 的真实语义：不是「出错就停」，是「在某些位置出错才停」
#
#   demo.sh 里这样调：bash errescue.sh
#   安全：只在当前目录建 errescue/ 子目录（第 3、4、10 格要用的输入文件 p.txt + 退出码台账 rcs.txt），
#         跑完由 trap 整目录删掉；不碰 ~、不写绝对 /tmp、不联网、不装包、不起后台进程、不 kill。
#   故意不开 set -e：本章一半内容是「出错之后还在跑」的现场，本脚本自己必须活到打印 rc 那一行。
#
# ★ 本机不拿「文档说 set -e 会怎样」当证据。每一格都走 run 这个助手，它只干三件事：
#   ① 把喂给 bash -c 的那串命令**原样**打上屏 —— 屏幕上的标签和真正跑的是同一个字符串，不存在
#      「写着 ERE 却没传 -E」那种事（上一批次真栽过一次，所以这条是硬要求）；
#   ② 在一个全新的 bash -c 子进程里跑它（这一格开的 -e 绝不会串到下一格）；
#   ③ 打 rc —— 先落进变量再上屏，取的是 PIPESTATUS[0]，即子 bash 自己的退出码，不是后面那道 sed 的码。
#   退出码就是本章的证据本体：每条当场打印，并抄一份进台账，第 11 格一起收账。
# ============================================================================

rm -rf errescue
mkdir -p errescue
trap 'rm -rf errescue' EXIT

cat > errescue/p.txt <<'EOF'
alpha
beta
gamma
EOF
: > errescue/rcs.txt

run() {
    printf '  敲的是：%s\n' "$1"
    bash -c "$1" 2>&1 | sed 's/^/    /'
    code=${PIPESTATUS[0]}
    printf '  rc=%s\n' "$code"
    printf '%s\n' "$code" >> errescue/rcs.txt
}

echo "=== 1. 先给直觉留点面子：普通位置上的失败，set -e 确实会停 ==="
run 'false; echo "不开-e：这行照打"'
run 'set -e; false; echo "开了-e：这行印不出来"'
printf '  → 直觉在这两格里是对的：不开 -e 时，失败只在 $? 里留了个 1，没人读它就等于没发生；开了才当场终止。\n'
printf '    但「当场终止」的条件很苛刻 —— 下面 2~5 格那四个位置，-e 一概不管，一个都不停。\n'

echo
echo "=== 2. 豁免①：失败写在 && 的左边（条件位） ==="
run 'set -e; f(){ false; echo "体内第二行"; }; f && echo "右边"; echo "tail"'
run 'set -e; f(){ false; }; f && echo "右边"; echo "tail"'
run 'set -e; true && false; echo "tail"'
printf '  → 三条都带 &&，屏幕上却是三种样子：第 1 条体内第二行照打（函数一进条件位，-e 在**函数体内部**\n'
printf '    整块关灯），且 f 交出的是最后一条 echo 的 0，所以右边也打了；第 2 条右边没打 —— ★ 那是 && 短路，\n'
printf '    不是 -e 停的，因为 tail 还在、rc=0；第 3 条 rc=1：豁免只管条件位那一条，**列表最后一条**失败 -e 照杀。\n'

echo
echo "=== 3. 豁免②：失败写在 || 的左边，右边就是逃生门 ==="
run 'set -e; f(){ false; }; f || echo "右边接住了"; echo "还在"'
run 'set -e; f(){ false; echo "体内第二行"; }; f || echo "右边接住了"; echo "还在"'
run 'set -e; grep zzznomatch errescue/p.txt || echo "grep 没找到"; echo "还在"'
printf '  → 第 1 格：f 交出 1，右边接住、还在、rc=0 —— || 左边整条都算条件位，-e 全程闭嘴。\n'
printf '  → ★ 第 2 格只是往体内多塞一句 echo：体内第二行照打（体内 -e 已关灯），右边**却没接** —— f 交出的是\n'
printf '    最后一条 echo 的 0：函数返回什么，看的从来不是它中间失败过几次，而是它最后干了什么。\n'
printf '  → 第 3 格换成真命令：p.txt 里没有 zzznomatch，grep 码 1，逃生门打印「grep 没找到」，还在、rc=0。\n'

echo
echo "=== 4. 豁免③：失败写在 if 的条件位 ==="
run 'set -e; f(){ false; }; if f; then echo "T"; else echo "F"; fi; echo "if 之后"'
run 'set -e; if grep zzznomatch errescue/p.txt; then echo "T"; else echo "F"; fi; echo "if 之后"'
printf '  → 走 F、if 之后还在、rc=0：写在条件位上的失败被当成「答案」，不是「事故」。\n'
printf '  → 第 2 格换成真命令：grep 没匹配上返回 1，-e 一样不管 —— if grep -q 是 -e 脚本里靠得住的探测写法。\n'

echo
echo "=== 5. 豁免④：感叹号把整条列表推进条件位 ==="
run 'set -e; ! true; echo "还在，读到的码=$?"'
run 'set -e; ! false; echo "还在，读到的码=$?"'
run 'set -e; h(){ false; echo "体内"; }; ! h; echo "还在，读到的码=$?"'
printf '  → 三格都「还在」且 rc=0：! 一戴上，整条列表就进了条件位，-e 不管它。★ 第 3 格最阴：体内第一条 false\n'
printf '    失败了第二行照样打，而下一行读 $? 拿到的是 1（h 本身返回 0，被 ! 翻成 1）—— 失败被翻译成了成功取反。\n'

echo
echo "=== 6. 「函数最后一条失败」根本不会自动终止一个不带 -e 的脚本 ==="
run 'f(){ false; }; f; rc=$?; echo "tail 照打"; echo "刚才 f 交出过的码=$rc"'
run 'set -e; f(){ false; }; f; echo "开了-e：tail 印不出来"'
printf '  → 同一个 f，差别只在脚本头上有没有 -e：不穿时脚本跑到尾、整体 rc=0，那次失败只活在 rc 这一行里；\n'
printf '    穿了就死在调用那一行（rc=1）。★ 「函数最后一条失败」bash 从来不替你记账：码进了 $? 就等被覆盖。\n'

echo
echo "=== 7. ★ 全章最贵的一格：同一个函数，裸调用 vs 放进 if ==="
run 'set -e; h(){ false; echo "体内第二行"; }; h; echo "调用之后"'
run 'set -e; h(){ false; echo "体内第二行"; }; if h; then echo "T"; else echo "F"; fi; echo "if 之后"'
printf '  → 直觉：函数里第一条就失败了，两格都该停在 h 那一行、都该走 F。\n'
printf '  → 屏幕：裸调用那一格确实当场死（体内第二行没打、调用之后没打、rc=1）；放进 if 的这一格，\n'
printf '    体内第二行**照打**、走的是 **T**、if 之后还在跑、**rc=0**。\n'
printf '  ★ 条件位把函数体内的 errexit 整块关掉，函数返回的又是最后一条命令（echo，码 0）的状态，\n'
printf '    于是「体内明明失败了」被判成成功 —— 写 if 判成败的人，和写 set -e 图省心的人，\n'
printf '    会在同一行代码上拿到两个相反的答案。这一格是全章最贵的。\n'

echo
echo "=== 8. 被 -e 掐死时，脚本交出的码不一定是你 return 的那个 ==="
run 'h(){ false; return 7; }; h; rc=$?; echo "tail 照打，h 交出过的码=$rc"'
run 'set -e; h(){ false; return 7; }; h; echo "tail 印不出来"'
printf '  → 不穿 -e：h 老实交出 7（上面被 rc 接住了），脚本继续，整体 rc=0。穿 -e：屏幕上没有 tail，\n'
printf '    整体 rc=1 —— ★ 不是 7！被 errexit 终止时你 return 的数字根本没机会成为脚本的码，\n'
printf '    想把 7 留住只能靠 EXIT trap 去读（本章 trapsafe.sh 那一格）。\n'

echo
echo "=== 9. 循环的两个位置：条件位豁免，循环体不豁免 ==="
run 'set -e; while false; do echo "体内"; done; echo "还在"'
run 'set -e; for i in 1 2; do false; echo "体内"; done; echo "还在"'
printf '  → while 的条件一进来就是假：体内一次都没打、还在、rc=0（条件位天然豁免，第 05 章那条「until 一轮\n'
printf '    都不跑」的同族）；for 的循环体是**普通位置**，第一次迭代就当场死，rc=1。\n'

echo
echo "=== 10. 命令替换：写在赋值位会杀脚本，写在取值位不会 ==="
run 'set -e; v=$(grep zzznomatch errescue/p.txt); echo "活着 v=[$v]"'
run 'set -e; v=$(grep gamma errescue/p.txt); echo "活着 v=[$v]"'
run 'set -e; v=$(false; echo hi); echo "活着 v=[$v]"'
run 'set -e; echo "结果=$(grep zzznomatch errescue/p.txt)"; echo "还在"'
printf '  → 第 1 格 rc=1：赋值型命令替换（v=$(...)）的失败**会**终止脚本；第 2 格换成命中的 gamma 就活着，\n'
printf '    v=[gamma] 是 grep 真打出来的那一行。第 3 格说明 -e 只看替换串里**最后一条**命令：false 后面补一个\n'
printf '    echo 就变成 v=[hi]、rc=0（失败被最后一棒盖掉）；第 4 格没在赋值、只是嵌在字符串里，失败整个被咽掉。\n'

echo
echo "=== 11. 收账：上面每条 bash -c 的真实码都在台账里 ==="
printf '  run 出去的命令与它们的码（按出场顺序）：%s\n' "$(paste -sd ' ' errescue/rcs.txt)"
n=$(wc -l < errescue/rcs.txt)
z=$(grep -c '^0$' errescue/rcs.txt)
nz=$(grep -vc '^0$' errescue/rcs.txt)
printf '  条数=%s ｜ 码 0 的=%s ｜ 非 0 的=%s → 两个加起来等于条数吗：%s\n' \
    "$n" "$z" "$nz" "$([ "$((z + nz))" -eq "$n" ] && echo 等于 || echo 不等于)"
printf '  ★ 非 0 那个数会不会是恒零的假守卫？当场弄响一次：\n'
cp errescue/rcs.txt errescue/rcs.bak
printf '1\n' >> errescue/rcs.txt
ring=$(grep -vc '^0$' errescue/rcs.txt)
mv errescue/rcs.bak errescue/rcs.txt
back=$(grep -vc '^0$' errescue/rcs.txt)
printf '    塞进一条假码 1 → 非 0 的从 %s 变成 %s（响了）；撤掉探针之后回到 %s\n' "$nz" "$ring" "$back"
printf '  → 三条能带走的话：① -e 只管「普通位置」上的失败，&& || if ! 四个位置它一概不管；\n'
printf '    ② 函数一旦进了条件位，函数体内部的 -e 也一起关灯，它交出的是最后一条命令的码；\n'
printf '    ③ 所以别把 -e 当保险丝：成败要靠显式判码（if / rc=$? / || ），本章另一份 strictfour.sh\n'
printf '       把四件衣服穿满，也照样管不着这四位豁免 —— 两份脚本讲的是同一件事的两面。\n'
printf '  → 沙箱里只剩 p.txt 与台账 rcs.txt（探针 rcs.bak 已被 mv 吃掉），trap 退出时整目录删掉。\n'
exit 0
