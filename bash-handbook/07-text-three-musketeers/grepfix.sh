#!/usr/bin/env bash
# ============================================================================
# grepfix.sh —— 用 grep 做工程判断时那三种「说没有 / 说出错 / 把码吃掉」的现场
#             （第 07 章 · 主线②「退出码 ≠ 没匹配上」的 grep 半边）
#
#   demo.sh 里这样调：bash grepfix.sh
#   安全：只在当前目录建 gf/ 沙箱（含一棵小树），输入全部当场造（here-doc + printf），
#         全程相对路径，trap 在退出时删掉；不联网、不装包、不开端口、不起后台、不碰家目录。
#   故意不写 set -e / set -u、不加全局 pipefail：这一页的主角就是非 0 码；pipefail 只在
#         子进程里现场演示（bash -c 那几条）。空值一律 ${VAR:-—} 兜住。
#   方言对照归邻居 grepdialect.sh，「整行还是片段」归同目录 greppat.sh，这一页只管**码**。
#
# ★ 本机不拿耗时当证据：-m 1 / -q 的「早停」全部换成屏幕上多打几行、-c 的数字变成几；
#   目录遍历顺序不稳的地方一律 sort 过，所以两遍输出一字不差。
# ============================================================================

rm -rf gf
mkdir -p gf gf/tree/sub 'gf/tree/has space'
trap 'rm -rf gf' EXIT

cat > gf/app.log <<'APPEOF'
error: disk full on node-3
warn: slow response
info: everything ok
error: retry later
nothing to see here
APPEOF
printf 'warn: only warnings here\n' > gf/app2.log
printf 'error: disk\nwarning: slow\ninfo: ok\n# 这是配置行\n空行在下面\n\nlast error end\n' > gf/mp.txt
printf 'line1 ok\nline2 ERROR disk\nline3 ok\nline4 ok\nline5 ERROR net\nline6 ok\n' > gf/ctx.txt
printf 'error\n\n# 这是配置行\n' > gf/pats.txt; printf '# 这是配置行\n' > gf/pats_hash.txt; : > gf/pats_empty.txt
printf 'HEAD needl first\n' > gf/tree/a.txt; printf 'nothing here\n' > gf/tree/b.log
printf 'needle two\nneedle again\n' > gf/tree/sub/c.txt; printf 'needle skip\n' > gf/tree/skip.md
printf 'needle spaced\n' > 'gf/tree/has space/d o k.txt'
{ printf 'HEAD needl first\n'; seq 1 4000; printf 'TAIL needl last\n'; } > gf/big.txt

# three <标签> <文件> <pattern>：先把码拿到手再看数字 —— 本脚本自己在用的判据
three() {
    local tag=$1 f=$2 pat=$3 out rc verdict
    out=$(grep -c -- "$pat" "$f" 2>&1); rc=$?
    case $rc in
        0) verdict="命中，数字=${out:-—}（码 0：这是答案）" ;;
        1) verdict="一行都没有，数字=${out:-—}（码 1：这也是答案，不是故障）" ;;
        2) verdict="出错，别看数字，原文[$out]" ;;
        *) verdict="没见过的码 $rc" ;;
    esac
    printf '  %-18s rc=%s ｜ %s\n' "$tag" "$rc" "$verdict"
}
# oneline：把多行字符串压成一行好放进 printf（★ 只用 ASCII 空格，别拿多字节字符喂 tr）
oneline() { printf '%s' "$1" | tr '\n' ' '; }

echo
echo "=== 1. 问「有没有」该用 -q；grep -c 把数字打进 stdout，串起来就出事 ==="
printf '  grep -q：stdout 字节 %s（真静默）｜ 命中时 rc=%s ｜ 找不到时 rc=%s\n' "$(grep -q error gf/app.log | wc -c)" "$(grep -q error gf/app.log; echo $?)" "$(grep -q nosuchpattern gf/app.log 2>/dev/null; echo $?)"
printf '  grep -c：stdout 是这一串 [%s] ｜ rc=%s ← 数字是**打印出来的数据**，不只是码\n' "$(grep -c error gf/app.log)" "$(grep -c error gf/app.log >/dev/null; echo $?)"
printf '  污染现场：把 -c 和别的输出拼成一段报告 → [%s]（数字混在中间，下游数行就多数一行）\n' "$( { grep -c error gf/app.log; echo 报告结束; } | tr '\n' '/')"
acc=$( { if [ "$(grep -c x gf/nosuch.log)" -gt 0 ]; then printf '分支=有命中\n'; else printf '分支=没有命中\n'; fi; } 2>&1 ); grc=$(grep -c x gf/nosuch.log 2>/dev/null; echo $?)
printf '  现场一（文件不存在）：if [ "$(grep -c x 不存在的文件)" -gt 0 ] 整句跑一遍，屏幕按顺序是：\n'; printf '%s\n' "$acc" | sed 's/^/      /'
printf '      ★ grep 自己的码=%s（2=出错），可 if 只看得到 [ 那一层的码 —— 真错在这一步被翻译成「没有命中」\n' "$grc"
v2=$(grep -c error gf/app.log gf/app2.log 2>/dev/null); bsplit=$( { [ "$v2" -gt 0 ]; echo "这一层的rc=$?"; } 2>&1 )
printf '  现场二（多个文件）：-c 的 stdout 变成 [%s] —— 一行一个「文件名:数字」，[ 直接罢工：\n' "$(printf '%s' "$v2" | tr '\n' '|')"
printf '%s\n' "$bsplit" | sed 's/^/      /'

echo
echo "=== 2. 三态要写成能用的结构：0 命中 / 1 没有 / 2 出错 ==="
three '① 有东西的文件' gf/app.log 'error'; three '② 真没有的串' gf/app.log 'nosuchpattern'
three '③ 文件不存在' gf/nosuch.log 'error'; three '④ 坏正则' gf/app.log '['
accident=$( { grep -c 'error' gf/nosuch.log || echo 没找到; } 2>&1 )
printf '  ★ 事故写法 grep 模式 文件 || echo 没找到 —— 把 ③ 喂进去，屏幕上按顺序出现：\n'; printf '%s\n' "$accident" | sed 's/^/      /'
printf '  → 合起来读就是「没找到」，可它的码是 %s —— CI 会绿着漏检。|| 只能挂在「码正好是 1」那一支，写法见上面 three() 的 case。\n' "$(grep -c 'error' gf/nosuch.log >/dev/null 2>&1; echo $?)"

echo
echo "=== 3. grep -v：滤干净了退 1，还在管道里把上游的码吃掉 ==="
printf '  整份文件都被 -v 滤掉 → 输出 %s 行，rc=%s ｜ 空输入（printf 空串 | grep -v x）→ rc=%s\n' "$(grep -v '.*' gf/app.log | wc -l)" "$(grep -v '.*' gf/app.log >/dev/null; echo $?)" "$(printf '' | grep -v x >/dev/null; echo $?)"
printf '  ★ -v 的 1 =「一行都没剩下」，和「没找到」共用同一个码 —— 日志被滤干净时它就是在报 1\n'
pl1=$(bash -c '{ grep -c "[" gf/nosuch.log | grep -v zzz >/dev/null; echo "PIPESTATUS=${PIPESTATUS[*]} 整条rc=$?"; } 2>&1')
pl1f=$(bash -c 'set -o pipefail; { grep -c "[" gf/nosuch.log | grep -v zzz >/dev/null; echo "PIPESTATUS=${PIPESTATUS[*]} 整条rc=$?"; } 2>&1')
printf '  甲（上游 2、下游也非 0）：不开 pipefail → [%s]｜开了 → [%s]\n' "$(oneline "$pl1")" "$(oneline "$pl1f")"
printf '  ★ pipefail 取的是**最右边那个非 0**：甲这两支的整条码都是 1，上游那个 2 照样看不见 —— 它不是「每段都报」开关\n'
pl3=$(bash -c '{ grep -c error gf/app.log gf/nosuch.log | grep -v qqqzz >/dev/null; echo "PIPESTATUS=${PIPESTATUS[*]} 整条rc=$?"; } 2>&1')
pl3f=$(bash -c 'set -o pipefail; { grep -c error gf/app.log gf/nosuch.log | grep -v qqqzz >/dev/null; echo "PIPESTATUS=${PIPESTATUS[*]} 整条rc=$?"; } 2>&1')
printf '  乙（上游 2、下游 0）：不开 pipefail → [%s]｜开了 → [%s]\n' "$(oneline "$pl3")" "$(oneline "$pl3f")"
printf '  ★ 乙才是事故本体：报错原文仍在屏幕上，整条管道却报**成功**；开关只在那个 bash -c 里活着，本脚本不开（不然前面几格早停了）\n'
printf '  → 想留住每一段的码只有 PIPESTATUS；grep -v 在 CI 里的正确姿势是把 1 当「没有剩下」，别当崩溃。\n'

echo
echo "=== 4. 多 pattern 的三种写法：-e、-E 的 |、-f 文件（空行和 # 行会怎样）==="
printf '  样本 gf/mp.txt 共 %s 行（其中一行是空行，一行以 # 开头）\n' "$(grep -c '' gf/mp.txt)"
printf '  -e error -e warning → %s 行 ｜ -E error|warning → %s 行 ｜ 相等吗：%s\n' "$(grep -c -e error -e warning gf/mp.txt)" "$(grep -cE 'error|warning' gf/mp.txt)" "$([ "$(grep -c -e error -e warning gf/mp.txt)" -eq "$(grep -cE 'error|warning' gf/mp.txt)" ] && echo 是 || echo 否)"
printf '  -f gf/pats.txt（三行：error、空行、# 这是配置行）→ 命中 %s 行；文件总行数 %s → 相等吗：%s\n' "$(grep -c -f gf/pats.txt gf/mp.txt)" "$(grep -c '' gf/mp.txt)" "$([ "$(grep -c -f gf/pats.txt gf/mp.txt)" -eq "$(grep -c '' gf/mp.txt)" ] && echo 是 || echo 否)"
printf '  ★ 空行 = 空 pattern = **匹配每一行**；# 那行不是注释、是字面 pattern（单独放它只命中 %s 行）；-e 后面给空串同样下场（%s 行）\n' "$(grep -c -f gf/pats_hash.txt gf/mp.txt)" "$(grep -c -e error -e '' gf/mp.txt)"
ezc=$(grep -c -f gf/pats_empty.txt gf/mp.txt 2>/dev/null); ezrc=$?; nz=$(grep -c nosuchpattern gf/mp.txt 2>/dev/null); nzrc=$?
printf '  两种「rc=%s / rc=%s」摆一起：空 pattern 文件 → stdout=[%s] ｜ 正常「没有」 → stdout=[%s] —— 码一样、stdout 不一样，前者连 0 都不打\n' "$ezrc" "$nzrc" "${ezc:-（一个字符都没打）}" "${nz:-（一个字符都没打）}"
printf '  → 从别处生成的 pattern 文件，先数它有几行（gf/pats.txt=%s 行）再上生产线：多一个空行 = 全放行。\n' "$(grep -c '' gf/pats.txt)"

echo
echo "=== 5. -A/-B/-C 是给人看的：那个 -- 会被下游当成数据 ==="
printf '  样本 gf/ctx.txt：%s 行，ERROR 命中 %s 行\n' "$(grep -c '' gf/ctx.txt)" "$(grep -c ERROR gf/ctx.txt)"
printf '  -A1 → %s 行 ｜ -B1 → %s 行 ｜ -C1 → %s 行 ｜ 不带上下文 → %s 行\n' "$(grep -A1 ERROR gf/ctx.txt | wc -l)" "$(grep -B1 ERROR gf/ctx.txt | wc -l)" "$(grep -C1 ERROR gf/ctx.txt | wc -l)" "$(grep ERROR gf/ctx.txt | wc -l)"
printf '  ★ -A1 比命中行数多出 %s 行，其中有 %s 行是分隔符（屏幕上就是那个 --）—— 它是排版，不是数据\n' "$(( $(grep -A1 ERROR gf/ctx.txt | wc -l) - $(grep -c ERROR gf/ctx.txt) ))" "$(grep -A1 ERROR gf/ctx.txt | grep -c '^--$')"
printf '  -C1 输出 %s 行等于整个文件 %s 行吗 → %s（两组上下文粘在一起了，此时分隔符有 %s 条）\n' "$(grep -C1 ERROR gf/ctx.txt | wc -l)" "$(grep -c '' gf/ctx.txt)" "$([ "$(grep -C1 ERROR gf/ctx.txt | wc -l)" -eq "$(grep -c '' gf/ctx.txt)" ] && echo 是 || echo 否)" "$(grep -C1 ERROR gf/ctx.txt | grep -c '^--$')"
printf '  --group-separator 不是删掉那一行，是换掉内容：钉成 #### 出现 [%s]；钉成空串仍 %s 行（分隔位变成空行）\n' "$(grep -A1 --group-separator='####' ERROR gf/ctx.txt | grep '^####' | head -1)" "$(grep -A1 --group-separator= ERROR gf/ctx.txt | wc -l)"
printf '  → 要统计就在**没有** -A/-B/-C 的那一步数；上下文只加在给人看的那一条命令上。\n'

echo
echo "=== 6. 递归与文件名：-l 的单位是文件，带空格的名字在 xargs 里散架 ==="
printf '  gf/tree 共 %s 个文件；grep -rl 列出 %s 个（单位=文件）｜命中行一共 %s 行（有个文件中了 2 行）\n' "$(find gf/tree -type f | wc -l)" "$(grep -rl needl gf/tree | sort | wc -l)" "$(grep -rn needl gf/tree | wc -l)"
printf '  --include=*.txt → %s 个 ｜ --exclude=*.md → %s 个 ｜ 相等吗：%s（名单比文件总数少 %s 个 = 那个 b.log）\n' "$(grep -rl --include='*.txt' needl gf/tree | sort | wc -l)" "$(grep -rl --exclude='*.md' needl gf/tree | sort | wc -l)" "$([ "$(grep -rl --include='*.txt' needl gf/tree | sort | wc -l)" -eq "$(grep -rl --exclude='*.md' needl gf/tree | sort | wc -l)" ] && echo 是 || echo 否)" "$(( $(find gf/tree -type f | wc -l) - $(grep -rl needl gf/tree | sort | wc -l) ))"
bad=$(grep -rl needl gf/tree | xargs grep -c needl 2>&1); badrc=$?
printf '  ★ 散架现场 grep -rl ... | xargs grep -c（本机 xargs：%s）：按空白切名字，带空格那个被切成 4 段\n' "$(xargs --version | head -1)"
printf '%s\n' "$bad" | sort | sed 's/^/      /'
printf '  → 报错 %s 条、只数到 %s 个文件（本该 %s 个）、xargs 退出码=%s：一句没说错，结论却少了一个文件\n' "$(printf '%s\n' "$bad" | grep -c 'No such file')" "$(printf '%s\n' "$bad" | grep -cE ':[0-9]+$')" "$(grep -rl needl gf/tree | wc -l)" "$badrc"
good=$(grep -rlZ needl gf/tree | xargs -0 grep -c needl 2>&1); goodrc=$?
printf '  ★ 对写法 grep -rlZ ... | xargs -0：数到 %s 个文件、报错 %s 条、rc=%s → 和名单行数 %s 相等吗：%s\n' "$(printf '%s\n' "$good" | grep -cE ':[0-9]+$')" "$(printf '%s\n' "$good" | grep -c 'No such file')" "$goodrc" "$(grep -rl needl gf/tree | wc -l)" "$([ "$(printf '%s\n' "$good" | grep -cE ':[0-9]+$')" -eq "$(grep -rl needl gf/tree | wc -l)" ] && echo 是 || echo 否)"
printf '  → 只用行数和 rc 下结论：带空格/怪名字的文件名只能靠 -Z 配 -0 这一对，加引号救不了 xargs。\n'

echo
echo "=== 7. 早停：-m 1 和 -q 各自少做了什么（只演屏幕上看得见的）==="
printf '  样本 gf/big.txt 共 %s 行，needl 在第 %s 行和第 %s 行（★ 只报行号，不报耗时）\n' "$(grep -c '' gf/big.txt)" "$(grep -n needl gf/big.txt | head -1 | cut -d: -f1)" "$(grep -n needl gf/big.txt | tail -1 | cut -d: -f1)"
printf '  不加开关打印 %s 行 ｜ -m 1 打印 %s 行（留下的是 [%s]）｜ -q 打印 %s 字节\n' "$(grep needl gf/big.txt | wc -l)" "$(grep -m1 needl gf/big.txt | wc -l)" "$(grep -m1 needl gf/big.txt)" "$(grep -q needl gf/big.txt | wc -c)"
printf '  -m 1 的 rc=%s ｜ -q 的 rc=%s ｜ 找不到时 -m 1 的 rc=%s（和「没有」同一个码）｜ 本机 -m 0：打印 %s 行 rc=%s\n' "$(grep -m1 needl gf/big.txt >/dev/null; echo $?)" "$(grep -q needl gf/big.txt; echo $?)" "$(grep -m1 zzznoone gf/big.txt >/dev/null 2>&1; echo $?)" "$(grep -m0 needl gf/big.txt | wc -l)" "$(grep -m0 needl gf/big.txt >/dev/null 2>&1; echo $?)"
printf '  ★ 最狠的一条：grep -c 报 %s，grep -m1 -c 报 %s —— 早停改的是**数字**本身，不只是输出长短\n' "$(grep -c needl gf/big.txt)" "$(grep -m1 -c needl gf/big.txt)"
printf '  → 只想知道有没有就 -q（它连数字都不打）；要数几行就别配 -m；-m 的「找不到」和「没有」同为 1，别当成崩溃。\n'

echo
echo "=== 8. 收口：码的分工 + 当场自证 ==="
printf '  grep    0=有 ｜ 1=没有 ｜ 2=出错（只有 2 需要去读报错原文）\n'
printf '  -v      滤空了退 1；管道里它盖掉上游的码，只有 PIPESTATUS 看得见\n'
printf '  -l/-r   单位是文件；文件名交给下游必须 -Z 配 -0\n'
printf '  -c      数字是 stdout 上的数据：多文件变多行，塞进 [ ] 就是事故\n'
printf '  -m/-q   早停：少打行、少读文件，也会把 -c 的数字改小\n'
r1=$(bash -c 'grep -v ".*" gf/app.log | grep -v zzz; echo "rc=$? PIPESTATUS=${PIPESTATUS[*]}"' 2>/dev/null); r2=$(bash -c 'grep -v ".*" gf/app.log | grep -v zzz; echo "rc=$? PIPESTATUS=${PIPESTATUS[*]}"' 2>/dev/null)
printf '  自证一（可复现）：第 3 格那条管道连跑两遍，两串码一样吗 → %s ｜ 内容=[%s]\n' "$([ "$r1" = "$r2" ] && echo 一字不差 || echo 不一样)" "$r1"
printf '  自证二（现场清干净）：删之前 gf 在吗 → %s\n' "$([ -d gf ] && echo 在 || echo 不在)"
rm -rf gf
printf '  删之后在吗 → %s ｜ 本目录里叫 gf 的残留条目 = %s（trap 只是备胎）\n' "$([ -d gf ] && echo 在 || echo 不在)" "$(find . -maxdepth 1 -name gf | wc -l)"
printf '  ★ 一句话带走：1 是答案不是故障，2 才是事故；凡是 || 、[ “$(grep -c ...)” ] 、管道 rc，都在把 2 往 1 上糊。\n'
exit 0
