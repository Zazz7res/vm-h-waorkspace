#!/usr/bin/env bash
# ============================================================================
# strictfour.sh —— 「健壮四件套」是四件互不相干的衣服，不是同一句话的四遍
#
#   demo.sh 里这样调：bash strictfour.sh
#   安全：只在当前目录建 strict/（p.txt 输入、probe.txt 计数探针、rcs.txt 退出码台账），跑完 trap 整目录删掉；
#         本脚本自己什么都不穿（它就是对照组）；衣服只穿进 bash -c 的子进程。不碰 ~、不写绝对 /tmp、不联网。
# ★ 本机不拿「网上那行 set -euo pipefail」当证据。每格都是同一条命令「穿前 vs 穿后」并排跑；run 先把
#   喂给 bash -c 的字符串原样打上屏（标签和命令是同一个字符串），再打子 bash 的真码并记进台账。
# ============================================================================

rm -rf strict
mkdir -p strict
trap 'rm -rf strict' EXIT

cat > strict/p.txt <<'EOF'
alpha
beta
gamma
EOF
cat > strict/probe.txt <<'EOF'
这一行故意含 zzznomatch，用来把第 7 格的计数守卫弄响，用完就删
EOF
: > strict/rcs.txt

run() {
    printf '  敲的是：%s\n' "$1"
    bash -c "$1" 2>&1 | sed 's/^/    /'
    code=${PIPESTATUS[0]}
    printf '  rc=%s\n' "$code"
    printf '%s\n' "$code" >> strict/rcs.txt
}

echo "=== 1. 先把四件衣服的默认位置钉死：全是 off ==="
printf '  本机 bash 版本串：%s\n' "$(bash -c 'echo "$BASH_VERSION"')"
bash -c 'set -o | grep -E "^(errexit|errtrace|nounset|pipefail)"' 2>&1 | sed 's/^/    /'
printf '  → 上面那块是四件衣服的学名与默认位置（-e=errexit、-u=nounset、-o pipefail、-E=errtrace）；不写就等于没穿。\n'
printf '  ★ 别把 -E 和 functrace 混 —— 那件管 DEBUG trap。下面每格都是穿前/穿后并排，两行 rc 的差就是分工。\n'

echo
echo "=== 2. 衣服一 -e：挡的是「普通位置上的失败」 ==="
run 'false; echo "不穿：还活着"'
run 'set -e; false; echo "穿了：这行印不出来"'
printf '  → 不穿 rc=0（失败没人认领），穿了 rc=1 且后半句没执行 —— 它只认「普通位置」，条件位一概不管（那半边归 errescue.sh）。\n'

echo
echo "=== 3. 衣服二 -u：挡「没定义过的名字被读」，顺带看一件衣服怎么改掉另一件的码 ==="
run 'set -u; echo "读到=[$NOPE]"; echo "后面还在"'
run 'set -u; echo "换成 \${NOPE-} 写法：读到=[${NOPE-}]"; echo "后面还在"'
run 'set -euo pipefail; echo "读到=[$NOPE]"; echo "后面还在"'
printf '  → 只穿 -u：报错原文「unbound variable」＋码 127；换成 ${NOPE-} 就正常读出空串、码 0。\n'
printf '  → ★ 第三条把三件套穿满，同一条命令、同一句报错，码却从 127 变成 1 —— 报错由 -u 触发，退出码却由 -e 决定。\n'

echo
echo "=== 4. 衣服二实战：读一个没传进来的位置参数 ==="
run 'set -u; echo "穿了：[$1]"; echo "后面还在"'
run 'set -u; echo "换成 \${1-} 写法：[${1-}] 个数=$#"; echo "后面还在"'
run 'set -u; printf "[%s]\n" "$@"; echo "一个参数都没转发，后面还在"'
printf '  → 报错原文里的名字是 $1（不是变量名打错），码还是 127 —— 新脚本最常见的第一次崩就是忘了传参。\n'
printf '  → ${1-} 与 "$@" 都活着（rc=0）：前者兜底成空串，后者空参数展开在本机不算未绑定（边界见第 5 格）。\n'

echo
echo "=== 5. 衣服二的边界：本机 bash 5.3 里空数组展开不报错（老手册那条结论已过期） ==="
run 'set -u; a=(); echo "空数组长度=${#a[@]}"; printf "[%s]\n" "${a[@]}"; printf "[%s]\n" "${a[*]}"; echo "还在"'
run 'set -u; echo "压根没声明过的 c：展开=[${c[@]}]，还在"'
run 'set -u; echo "压根没声明过的 c：长度=${#c[@]}"'
run 'set -u; b=(); echo "空数组取第 0 个=[${b[0]}]"'
printf '  → 四条里两条活着、两条报错：空数组整体展开（[@] 与 [*]）和没声明过的名字整体展开都不报。\n'
printf '  → ★ 报的是「对没声明的名字求长度」（码 1）与「取具体下标」（码 127），两种码还不一样。\n'
printf '    所以「set -u 会因空数组炸掉」在本机不成立、别照抄，要防的是下标与 ${#…[@]} 这两种写法。\n'

echo
echo "=== 6. 衣服三 -o pipefail：管道的码默认只算最后一棒 ==="
run 'false | true; echo "不穿：管道码=$?"'
run 'set -o pipefail; false | true; echo "穿了：管道码=$?"'
printf '  → 同一格：不穿 0、穿了 1。★ 两行的 rc 却都是 0 —— 衣服三只改变「管道交出什么码」，它自己不会终止脚本；\n'
printf '    要让它终止脚本得再穿一件 -e（第 8 格）。这是四件互不相干的第一个证据。\n'

echo
echo "=== 7. 衣服三实战：grep 一行都没中时，码才第一次有意义 ==="
printf '  ★ 先证明这道计数会响：把探针 strict/probe.txt（含要找的 zzznomatch）一起算进来 → 命中 %s 行\n' "$(cat strict/p.txt strict/probe.txt | grep -c zzznomatch)"
rm -f strict/probe.txt
printf '  撤掉探针 → 命中 %s 行（响过一次，这个 0 才算数；下面管道里的 grep 就是这一次真没中）\n' "$(grep -c zzznomatch strict/p.txt)"
run 'grep zzznomatch strict/p.txt | cat; echo "不穿：管道码=$?"'
run 'set -o pipefail; grep zzznomatch strict/p.txt | cat; echo "穿了：管道码=$?"'
run 'set -o pipefail; grep gamma strict/p.txt | cat; echo "有匹配时穿不穿都一样：码=$?"'
printf '  → 真实脚本里最常见的形状就是「grep 一下再交给 cat/awk/sed」：不穿 pipefail 时 grep 一行没中（上面实测\n'
printf '    命中 0 行）也被报成 0 —— 「查无此事」和「查到了」在码上长得一模一样。\n'
printf '  → 第 3 行是对照：换成命中的 gamma，两种穿法都交 0，而且 gamma 那一行被 cat 原样打了出来。\n'

echo
echo "=== 8. 衣服不互相顶班：-e 单穿挡不住管道里的失败 ==="
run 'set -e; grep zzznomatch strict/p.txt | cat; echo "只穿 -e：还在"'
run 'set -o pipefail; grep zzznomatch strict/p.txt | cat; echo "只穿 pipefail：还在"'
run 'set -e; set -o pipefail; grep zzznomatch strict/p.txt | cat; echo "两件都穿：这行印不出来"'
printf '  → 三行码是 0 / 0 / 1：-e 认的是「这条命令的码」，不穿 pipefail 时管道交出的永远是最后一棒的 0，于是 -e\n'
printf '    什么都没做错却放行了；只穿 pipefail 则码变 1 但没人终止脚本，线索照样躺在 $? 里。\n'
printf '  ★ 这两件得一起穿（写全就是 set -eo pipefail）：少一件就是漏的，四件套不是随便挑两件。\n'

echo
echo "=== 9. 衣服四 -E（errtrace）：只管一件事 —— ERR trap 进不进函数 ==="
run 'trap "echo 〔ERR〕响了" ERR; false; echo "还在"'
run 'trap "echo 〔ERR〕响了" ERR; h(){ false; echo "体内"; }; h; echo "还在"'
run 'set -e; trap "echo 〔ERR〕响了" ERR; h(){ false; }; h; echo "还在"'
run 'set -e; set -E; trap "echo 〔ERR〕响了" ERR; h(){ false; }; h; echo "还在"'
printf '  → 第 1、2 格：顶层 false 让 trap 响一次，同一个 false 搬进函数就一声不响 —— ERR trap 默认不进函数体。\n'
printf '  → 第 3、4 格只差一件 -E：不穿时一声不响、穿了响一次，而两格 rc 都是 1。★ 这件衣服既不报错也不终止，\n'
printf '    它只决定 trap 在函数里还活着不活着；想让函数里的失败留痕就得穿上它（细节归 trapsafe.sh）。\n'

echo
echo "=== 10. 三件套 set -euo pipefail 穿满，仍然挡不住这几类 ==="
run 'set -euo pipefail; f(){ false; }; if f; then echo "T"; else echo "F"; fi; echo "还在"'
run 'set -euo pipefail; h(){ false; echo "体内第二行"; }; if h; then echo "T"; else echo "F"; fi; echo "还在"'
run 'set -euo pipefail; false || true; echo "还在"'
run 'set -euo pipefail; v=$(grep zzznomatch strict/p.txt) || echo "赋值失败被 || 接住了"; echo "还在"'
run 'set -euo pipefail; a=(); echo "空数组长度=${#a[@]}，还在"'
printf '  → 五条全 rc=0：条件位（if / ||）穿多少件衣服都是豁免区。★ 第 2 条最贵 —— 三件套穿满，函数体里的\n'
printf '    「体内第二行」照样打、照样走 T，和 errescue.sh 第 7 格那个形状一个字都没差。\n'
printf '  → 第 4 条：|| 一接，赋值型命令替换的失败也不致命；第 5 条：-u 管不到空数组展开。衣服是叠加的，不是覆盖的。\n'

echo
echo "=== 11. 收账：每条 bash -c 的真实码，和四件衣服的分工 ==="
printf '  run 出去的命令与它们的码（按出场顺序）：%s\n' "$(paste -sd ' ' strict/rcs.txt)"
n=$(wc -l < strict/rcs.txt)
z=$(grep -c '^0$' strict/rcs.txt); o=$(grep -c '^1$' strict/rcs.txt)
u=$(grep -c '^127$' strict/rcs.txt); k=$(grep -vcE '^(0|1|127)$' strict/rcs.txt)
printf '  条数=%s ｜ 码 0 的=%s ｜ 码 1 的=%s ｜ 码 127 的=%s ｜ 别的码=%s → 四个加起来等于条数吗：%s\n' \
    "$n" "$z" "$o" "$u" "$k" "$([ "$((z + o + u + k))" -eq "$n" ] && echo 等于 || echo 不等于)"
cp strict/rcs.txt strict/rcs.bak
printf '127\n' >> strict/rcs.txt
ring=$(grep -c '^127$' strict/rcs.txt)
mv strict/rcs.bak strict/rcs.txt
back=$(grep -c '^127$' strict/rcs.txt)
printf '  ★ 127 会不会是个恒零的假守卫？塞一条假码 127 进账本 → 从 %s 变 %s（响了）；撤掉后回到 %s\n' "$u" "$ring" "$back"
printf '  → 分工表（只报本机屏幕上真出现过的码）：-e 挡普通位置的失败（→1）；-u 挡没定义就读（→127：标量与下标，\n'
printf '    但空数组整体展开与 "$@" 它不管）；-o pipefail 管非最后一棒的码（→管道码 0 变 1）；-E 只管 ERR trap 进不进\n'
printf '    函数（→多一声，rc 不变）。0 / 1 / 127 三种码都在这块屏幕上出现过，所以「穿几件」=「挡几种」。\n'
exit 0
