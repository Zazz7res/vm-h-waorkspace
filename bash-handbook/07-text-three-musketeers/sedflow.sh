#!/usr/bin/env bash
# ============================================================================
# sedflow.sh —— sed 手里永远只有一行：模式空间、地址、g/数字，跨行得自己主动拼
#             （第 07 章 sed 半区·之一；立主线①「三个工具的匹配单位不一样」）
#   demo.sh 里这样调：bash sedflow.sh
#   安全：只在本目录下建 sfdemo/ 沙箱，样本全用 here-doc 当场造；trap 退出时删掉，第 8 格当场自证残留 0 个。
#         不联网、不开端口、不起常驻进程、不碰家目录。故意不加 set -e：本脚本一半格子是「一条都没换成」和
#         「脚本写错」的现场，一遇非 0 就停，后面的对照组全看不到。
# ★ 本机不拿耗时当证据：-z 那格只演**形状**（sed 眼里有几条记录），钉它用的是 NUL 个数、行数、cmp 判定这些
#   每跑必一样的量；不印 inode / pid / 毫秒 / 随机名。工具报错原样捞 stderr 上屏、不改写不翻译（本机 sed、
#   grep 的报错文本跟着 locale 走，demo 钉 LC_ALL=C.UTF-8 时看到的就是 GNU sed 4.9 的英文原文）。
# ============================================================================

rm -rf sfdemo; mkdir -p sfdemo; trap 'rm -rf sfdemo' EXIT; LASTRC=0
cat > sfdemo/three.txt <<'EOF'
first
second
third
EOF
cat > sfdemo/aaa.txt <<'EOF'
aaa foo foo foo
EOF
cat > sfdemo/split.txt <<'EOF'
setting foo
bar = 1
EOF
cat > sfdemo/addr.txt <<'EOF'
start
one
end
skip
start
end
EOF
cat > sfdemo/q.txt <<'EOF'
aaa
ab
a+b
EOF
cat > sfdemo/path.txt <<'EOF'
/usr/bin/env
EOF

# show <标签> <命令...>：输出先落盘再数行 —— wc -l 数的是换行符个数，空输出就是 0 行；吃进变量再数，空串
# 会被补出一个换行、变成假的 1 行（本章就是要读这个 0）。
show() {
    local tag=$1
    shift
    "$@" > sfdemo/out.tmp 2>&1; LASTRC=$?
    printf '  %s → 输出 %s 行 ｜ rc=%s\n' "$tag" "$(wc -l < sfdemo/out.tmp)" "$LASTRC"
    sed 's/^/      | /' sfdemo/out.tmp
}

echo "=== 1. 默认 sed 每行都原样打印：p 是「额外再打一遍」，不是「改成只打这个」 ==="
show 'sed s/x/y/（一次都没命中）' sed 's/x/y/' sfdemo/three.txt
show 'sed 1p' sed '1p' sfdemo/three.txt
show 'sed -n 1p' sed -n '1p' sfdemo/three.txt
echo '  → 直觉「p = 打印这行，别的行就该消失」，屏幕上 1p 把 3 行打成 4 行（first 出现两次）：sed 每轮循环结尾'
echo '    自带一次「自动打印」，p 是在它之外**再多打一次」。所以「只打命中行」= -n 配 p；只写 p 不写 -n 的脚本输出'
echo '    行数翻倍，拿去数行数就是假账。'

echo; echo '=== 2. 同一行里换几个：不写 g 只换第一个，g 全换，2 只换第二个，& 是整段命中 ==='
show 's/a/A/' sed 's/a/A/' sfdemo/aaa.txt
show 's/a/A/g' sed 's/a/A/g' sfdemo/aaa.txt
show 's/a/A/2' sed 's/a/A/2' sfdemo/aaa.txt
show 's/foo/F&F/' sed 's/foo/F&F/' sfdemo/aaa.txt
show 's/foo/\&X/' sed 's/foo/\&X/' sfdemo/aaa.txt
echo '  → 尾巴那个数字是「第几个命中」不是「第几行」（行归地址管，第 5 格）；& = 整段命中的副本，打字面的 & 得写'
echo '    \& —— 少一个反斜杠就变成「把原文又贴一遍」。'

echo; echo '=== 3. ★ 被换行劈开的文本，sed 一眼看不到：没换成、退 0、文件也一字未动 ==='
show 'sed s/foo bar/X/（样本里这段被拆在两行）' sed 's/foo bar/X/' sfdemo/split.txt
printf '  输出和输入逐字节【%s】｜ rc=%s ｜ 进去 %s 行、出来 %s 行 —— 「换成 0 处」在 sed 里不算事故\n' "$(cmp -s sfdemo/split.txt sfdemo/out.tmp && echo 一样 || echo 不一样)" "$LASTRC" "$(wc -l < sfdemo/split.txt)" "$(wc -l < sfdemo/out.tmp)"
show 'sed -n s/foo bar/X/p（p 只在命中时打，0 行 = 一次都没命中）' sed -n 's/foo bar/X/p' sfdemo/split.txt
GN=$(grep -c 'foo bar' sfdemo/split.txt); printf '  同一条模式交给 grep：命中 %s 行 ｜ rc=%s ← grep 退 1 告诉你「真没有」，sed 连这个都不告诉你\n' "$GN" "$?"
show 'sed N;s/foo\nbar/X/（N 先把下一行拉进模式空间）' sed 'N;s/foo\nbar/X/' sfdemo/split.txt
echo '  ★ 主线①落点：grep 和 sed 的单位都是**整行**，换行是边界不是字符。跨行写法两个工具都不报错，但 sed 连退码'
echo '    都给 0；加了 N（两条行拼成一条记录）才换得到，代价是 2 行进去、1 行出来 —— 行数本身就是证据。'

echo; echo '=== 4. 主动把整文件拼成一条：循环读到底，以及 -z（NUL 分隔）把整文件当「一行」 ==='
show 'sed :loop;N;$!bloop;p（每轮 N 一条，没到末行就跳回去）' sed ':loop;N;$!bloop;p' sfdemo/three.txt
echo '    最后一轮手里是整个文件；p 打一遍 + 自动打印再打一遍 → 3 行的文件出 6 行。'
show '错示范：同一条脚本改用双引号包住' sed ":loop;N;$!bloop;p" sfdemo/three.txt
echo '    双引号里 $! 归 bash 解释（本脚本没后台任务 → 展开成空），sed 收到的是**另一条脚本**：6 行变 3 行、rc 还是 0、'
echo '    零报错；有后台任务时那位置会被换成进程号（屏幕上多一个 pid，脚本再变一次样）→ sed 脚本一律单引号。'
printf '  sed 眼里的记录条数：普通模式 %s 条（sed -n "=" 打几个号就是几条）｜ -z 模式 %s 条（-z 每条记录以 NUL 收尾，数 NUL 就是数记录）\n' "$(sed -n '=' sfdemo/split.txt | wc -l)" "$(sed -z -n '=' sfdemo/split.txt | tr -dc '\0' | wc -c)"
show '同一条 s/foo.bar/X/：左边的行数是 -z 的' sh -c "sed -z -n 's/foo.bar/X/p' sfdemo/split.txt | tr -d '\\0'"
echo '    普通模式下同一条一个字都没换（. 撞不到换行，输出 0 行、rc 照样 0），-z 下它命中了 —— 点号能不能吃换行跟着分隔符走。'
printf '  -z 下 sed 2d（删第 2 条记录）→ 出来 %s 行、rc=%s：整个文件只剩 1 条记录，这地址谁也打不着\n' "$(sed -z '2d' sfdemo/split.txt | tr -d '\0' | wc -l)" "$(sed -z '2d' sfdemo/split.txt >/dev/null 2>&1; echo $?)"
echo '  ★ 换 -z 不是换个口味：地址、范围、$、行号、. 一起跟着变，而它一个字都不报。用之前先数「sed 眼里有几条」。'

echo; echo '=== 5. 地址：范围打到「结束」那行就关门，关门后才会重新起算；等不到结束就一路打到文件尾 ==='
show 'sed -n 2,4p' sed -n '2,4p' sfdemo/addr.txt
show 'sed -n /start/,/end/p（start 在样本里出现两次）' sed -n '/start/,/end/p' sfdemo/addr.txt
show 'sed -n /one/,/never/p（结束地址永远不会出现）' sed -n '/one/,/never/p' sfdemo/addr.txt
show 'sed -n $p' sed -n '$p' sfdemo/addr.txt
echo '  → /start/,/end/ 那格：第一段的 end 一命中就关门，skip 不被打；第 5 行再撞 start 才**重新起算** —— 「起始模式'
echo '    出现两次」既不是只打第一段、也不是全打，得按开关几次数。/one/,/never/ 那格等不到结束就一直打到文件尾、'
echo '    rc=0：范围没收口不是语法错。$ 是「最后一条记录」，写 5,$ 才是「从第 5 行到结尾」。'

echo; echo '=== 6. 分隔符与方言：s|…|…| 只是少写反斜杠；-E 和 BRE 的 + 正好互为反面 ==='
show 's/\/usr\/bin/\/bin/' sed 's/\/usr\/bin/\/bin/' sfdemo/path.txt; cp sfdemo/out.tmp sfdemo/d1.tmp
show 's|/usr/bin|/bin|' sed 's|/usr/bin|/bin|' sfdemo/path.txt
printf '  两条输出逐字节一样吗 → %s ｜ 分隔符换成 | # @ 都行：命中内容不变，只是不用再堆反斜杠\n' "$(cmp -s sfdemo/d1.tmp sfdemo/out.tmp && echo 一样 || echo 不一样)"
show 'BRE  s/a\+b/Z/' sed 's/a\+b/Z/' sfdemo/q.txt
show 'ERE  -E s/a+b/Z/' sed -E 's/a+b/Z/' sfdemo/q.txt
show 'BRE  s/a+b/Z/（就是少写那个反斜杠的写法）' sed 's/a+b/Z/' sfdemo/q.txt
echo '  ★ 最后一格最贵：BRE 里裸 + 是**字面加号**，这条命令不报错、也不空转 —— 它换了 a+b 那一行，而真正想换的 ab'
echo '    那行原地不动、rc=0。反斜杠写串位 sed 不会提醒你，它只会换个地方动手（分组 () 写反了倒是会当场报错，因为'
echo '    \1 引用了一个不存在的组 —— 元字符和引用是两类错，一类安静、一类响。）'

echo; echo '=== 7. d / D / = / w：四种「下场」各长什么样 ==='
show 'sed /second/d（丢掉整条记录）' sed '/second/d' sfdemo/three.txt
show 'sed N 之后 /foo\nbar/D（只删模式空间的第一行）' sed 'N;/foo\nbar/D' sfdemo/split.txt
echo '  → d 丢掉整条记录（3 行变 2 行）；D 只丢**模式空间里的第一行**、剩下那条带着重开一轮 —— setting foo 没了、'
echo '    bar = 1 还在（2 行变 1 行）。这俩不是同义词，差着一整行。'
show 'sed =（行号 + 自动打印）' sed '=' sfdemo/three.txt
echo '  → = 打的是**记录号**不是把行换成号：3 行的文件出 6 行；配 -n 就只剩 3 个号（那 3 个号就是「几条记录」）。'
sed -n '/second/w sfdemo/keep.txt' sfdemo/three.txt > sfdemo/w.out; printf '  w 的下场：stdout 只有 %s 行（-n 且没写 p），磁盘上却多出 keep.txt（%s 行）：\n' "$(wc -l < sfdemo/w.out)" "$(wc -l < sfdemo/keep.txt)"
sed 's/^/      | /' sfdemo/keep.txt
rm -f sfdemo/keep.txt
printf '  当场清掉了吗 → %s（w 是「顺手写文件」的副作用：路径写错时，它就把东西稳稳落到那个错地方）\n' "$([ -f sfdemo/keep.txt ] && echo '还在，没删干净' || echo '不在了')"

echo; echo '=== 8. 收口：sed 手里的单位（一张表）+ 报错原文一格 + 自证 ==='
echo '  工具 ｜ 手里拿着什么          ｜ 一段话被换行劈开时          ｜ 「没换成」时的退出码'
echo '  grep ｜ 整行（要片段得 -o）    ｜ 一样看不见，得换记录分隔符   ｜ 没命中 1，真出错 2'
echo '  sed  ｜ 一行（模式空间）       ｜ 自己 N 拼，或 -z 换分隔符     ｜ 0，和改成功完全没区别'
echo '  awk  ｜ 一行里的字段           ｜ 改 RS（本章 awk 那支脚本演）  ｜ 0'
E1=$(sed 's/a/' sfdemo/three.txt 2>&1 >/dev/null); R1=$?; E2=$(grep -c '[' sfdemo/three.txt 2>&1 >/dev/null); R2=$?
printf '  故意写一条没闭合的 s，报错原文上屏（不改写不翻译）：sed ｜ 原文[%s] ｜ rc=%s\n' "$E1" "$R1"
printf '  同一条样本交给 grep 的语法错对照：grep｜ 原文[%s] ｜ rc=%s\n' "$E2" "$R2"
echo '  ★ 同一类「写错了」：本机 sed 退 1、grep 退 2 —— 退出码不跨工具通用。判据只有：0 才算跑成、非 0 一律去读原文；'
echo '    而 sed 的「什么都没改」也给 0，所以改文件必须自己数着对账（-i 那本账在 sedinplace.sh）。'
echo '  本章没演：-i 的真相（另支脚本）、--sandbox 下的 e/r/w、1~2 这类 GNU 步进地址、--posix 少哪些扩展、多文件时'
echo '  $ 与文件边界怎么纠缠、以及一切跟耗时和内存有关的表现。'
printf '  自证：沙箱里此刻有 %s 个条目（样本 + 中间产物），sfdemo 在吗 → %s\n' "$(ls -A sfdemo | wc -l)" "$([ -d sfdemo ] && echo 在 || echo 不在)"
rm -rf sfdemo
printf '  删掉之后 → sfdemo 在吗：%s ｜ 本目录里叫 sfdemo 的残留条目 = %s 个（必须是 0）\n' "$([ -d sfdemo ] && echo 在 || echo 不在)" "$(find . -maxdepth 1 -name sfdemo | wc -l)"
exit 0
