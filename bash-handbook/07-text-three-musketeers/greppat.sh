#!/usr/bin/env bash
# ============================================================================
# greppat.sh —— grep 手里只有「一整行」：-o / -c / 锚点 / locale 全在这上面错位
#             （第 07 章 · 主线①「匹配单位」的 grep 半边）
#
#   demo.sh 里这样调：bash greppat.sh
#   安全：只在当前目录建 gp/ 沙箱，输入全部当场造（here-doc + printf），全程相对路径，
#         trap 在退出时删掉；不联网、不装包、不开端口、不起后台、不碰家目录任何文件。
#   故意不写 set -e：这一页一半是「grep 退 1 / 退 2」的现场，一遇非 0 就停，后面对照组
#         全看不到；也不写 set -u、不加全局 pipefail；可能为空的量一律 ${VAR:-0} 兜住。
#   方言对照（BRE/ERE/PCRE 三套正则）归本章邻居 grepdialect.sh，这一页只管**单位**。
#
# ★ 本机不拿「耗时」当证据：grep 快慢每跑必抖，所以「给的是整行还是片段」全部换成
#   屏幕上打印几行 / 切出几段 / 一条几个字符 这种逐字能核的判定。
#   要动 locale 的格子只在**单条命令**前用 env 钉（第 7 格），脚本整体不改 locale
#   ——跑起来时外面已经是 demo 钉好的 C.UTF-8。
# ============================================================================

rm -rf gp
mkdir -p gp
trap 'rm -rf gp' EXIT

cat > gp/svc.log <<'LOGEOF'
error: disk full on node-3 error: retry later error: gave up
info: everything ok
error: only one here
nothing to see
LOGEOF
cat > gp/anchor.txt <<'ANTEOF'
log
login ok
cat log
the log file
colography
ANTEOF
printf 'foo=\nbar 1\n'                     > gp/kv.txt
printf 'a\tb\ncat and dog\n'               > gp/ws.txt
printf 'log\r\nlogin ok\r\ncat log\r\n'    > gp/crlf.txt
printf 'alpha\nbeta\ngamma'                > gp/nonl.txt
printf 'alpha\nbeta\ngamma\n'              > gp/wonl.txt
printf 'one: error\nplain line\ntwo: error only here\n' > gp/one.log
printf 'v1.2.3 发布\n配置 v1x2x3 生效\nIP 10.0.0.1 超时\nIP 10x0x0x2 也来了\n没有版本号\n' > gp/ver.txt
printf 'db_error\nfoo_bar\nuser-error\nerror.log\nERROR x\nmyerror\n' > gp/word.txt
printf '订单 价格12元 状态 OK\n价格3元 已发货\nno chinese here\n' > gp/zh.txt

# row <标签> <额外开关> <pattern> <文件>：同一条件下「整行口径」和「片段口径」并排打
row() {
    local tag=$1 opt=$2 pat=$3 f=$4 lines frag
    lines=$(grep $opt -c -- "$pat" "$f" 2>/dev/null); frag=$(grep $opt -o -- "$pat" "$f" 2>/dev/null | wc -l)
    printf '  %-12s 命中 %-3s 行 ｜ grep -o 切出 %-3s 段 ｜ 两数差 %s\n' "$tag" "${lines:-0}" "${frag:-0}" "$(( ${frag:-0} - ${lines:-0} ))"
}

echo
echo "=== 1. 直觉「把命中的那段给我」→ 它给的是整行（全章最贵的一格） ==="
printf '  样本 gp/svc.log（第 1 行里 error 出现 3 次）：\n'; sed 's/^/      /' gp/svc.log
row 'error' '' 'error' gp/svc.log
printf '  ★ 形状对账：整行版第 1 条 %s 个字符，片段版第 1 条 %s 个，pattern 本身 %s 个\n' "$(grep error gp/svc.log | sed -n 1p | wc -c)" "$(grep -o error gp/svc.log | sed -n 1p | wc -c)" "$(printf error | wc -c)"
printf '    → 整行版长度等于 pattern 长度吗：%s ｜ 片段版等于「pattern 长度 + 1」吗：%s（多的 1 是行尾换行）\n' "$([ "$(grep error gp/svc.log | sed -n 1p | wc -c)" -eq "$(printf error | wc -c)" ] && echo 相等 || echo 不相等)" "$([ "$(grep -o error gp/svc.log | sed -n 1p | wc -c)" -eq "$(( $(printf error | wc -c) + 1 ))" ] && echo 是 || echo 否)"
printf '  ★ 不能跨行：gp/kv.txt 里 foo= 命中 %s 行、bar 命中 %s 行，但 foo=bar 命中 %s 行\n' "$(grep -c 'foo=' gp/kv.txt)" "$(grep -c 'bar' gp/kv.txt)" "$(grep -c 'foo=bar' gp/kv.txt)"
printf '    → 两段字面明明都在文件里，★ grep 一次只看得见一行，所以它一口咬定「没有」；要跨行得换工具（sed 的 N、awk 的多行记录），不是换写法。\n'
printf '  ★ -o 也不是你想多短就多短：grep -o e.*r 第一条切出来是 [%s]\n' "$(grep -o 'e.*r' gp/svc.log | sed -n 1p)"
printf '    → 它是「这一行里能匹配的最长一段」，把两个 error 之间的字全吞了，但★ 一个字符都没越过行界。\n'

echo
echo "=== 2. 一行里命中两次：-c 数的是行，-o | wc -l 数的是次数 ==="
row 'svc.log' '' 'error' gp/svc.log
row 'one.log' '' 'error' gp/one.log
printf '  先弄响：pattern 放宽成单个字母 e → 行 %s / 段 %s，差 %s（证明两个计数器都是活的）\n' "$(grep -c 'e' gp/svc.log)" "$(grep -o 'e' gp/svc.log | wc -l)" "$(( $(grep -o 'e' gp/svc.log | wc -l) - $(grep -c 'e' gp/svc.log) ))"
printf '  再归零：one.log 每行至多一处命中 → 它的 -c 行数 %s 和 -o 段数 %s 相等吗：%s（差 %s；svc.log 那格差 %s）\n' "$(grep -c error gp/one.log)" "$(grep -o error gp/one.log | wc -l)" "$([ "$(grep -c error gp/one.log)" -eq "$(grep -o error gp/one.log | wc -l)" ] && echo 相等 || echo 不等)" "$(( $(grep -o error gp/one.log | wc -l) - $(grep -c error gp/one.log) ))" "$(( $(grep -o error gp/svc.log | wc -l) - $(grep -c error gp/svc.log) ))"
printf '  附：grep -o 扫的是**不重叠**片段 —— printf aaaa | grep -o aa → %s 段（不是 3 段）\n' "$(printf 'aaaa\n' | grep -o 'aa' | wc -l)"
printf '  → 数「几行中招」用 -c，数「出现几次」用 -o | wc -l；两数差几个，就是同一行里多出来的那几次。\n'

echo
echo "=== 3. 同一份样本上，四把尺量的不是一回事 ==="
printf '  样本 gp/anchor.txt：\n'; sed 's/^/      /' gp/anchor.txt
printf '  grep log → %s 行 ｜ -w log → %s 行 ｜ ^log → %s 行 ｜ log$ → %s 行\n' "$(grep -c 'log' gp/anchor.txt)" "$(grep -cw 'log' gp/anchor.txt)" "$(grep -c '^log' gp/anchor.txt)" "$(grep -c 'log$' gp/anchor.txt)"
printf '  先弄响：^log 命中 %s 行（样本非空、计数管道活着），下面那个 0 才有意义\n' "$(grep -c '^log' gp/anchor.txt)"
printf '  再来个 0：gp/crlf.txt（%s 行都以 log 收尾，但行尾多一个 \\r）—— log 命中 %s 行，log$ 命中 %s 行\n' "$(grep -c '' gp/crlf.txt)" "$(grep -c 'log' gp/crlf.txt)" "$(grep -c 'log$' gp/crlf.txt)"
printf '  ★ 那个 0 不是「样本里没有」：文件里 \\r 有 %s 个；$ 锚的是行尾，而 \\r 还占着行尾那个位置（Windows 存的文件十有九中）\n' "$(grep -o "$(printf '\r')" gp/crlf.txt | wc -l)"
printf '  → 四个数一般从大到小排：不锚 > -w > 锚一头 > 锚两头。搜 error/warn/id 满屏假阳性，多半是没戴 -w 或没戴锚。\n'

echo
echo "=== 4. \\b、\\<、\\>、[[:space:]] 在本机 GNU grep 的真实下场 ==="
printf '  gp/word.txt 找 error：裸 %s 行 ｜ -w %s 行 ｜ \\<error\\> %s 行 ｜ error\\b %s 行\n' "$(grep -c 'error' gp/word.txt)" "$(grep -cw 'error' gp/word.txt)" "$(grep -c '\<error\>' gp/word.txt)" "$(grep -c 'error\b' gp/word.txt)"
printf '  → 判定：-w 的数字和 \\<error\\> 相等吗 → %s（-w 就是给 pattern 戴上这对帽子）；error\\b 和裸 error 相等吗 → %s（只卡右边等于没卡）\n' "$([ "$(grep -cw error gp/word.txt)" -eq "$(grep -c '\<error\>' gp/word.txt)" ] && echo 是 || echo 否)" "$([ "$(grep -c 'error\b' gp/word.txt)" -eq "$(grep -c error gp/word.txt)" ] && echo 是 || echo 否)"
printf '  ★ 反直觉：-w error 偏偏只命中这两行 [%s] —— db_error 不算整词，因为下划线是**词字符**；连字符和点不算\n' "$(grep -w error gp/word.txt | tr '\n' ' ')"
printf '  空白类（gp/ws.txt：第 1 行有真制表符，第 2 行只有字母 t）：\n'
printf '      [[:space:]] 命中 %s 行 ｜ \\s 命中 %s 行 ｜ \\t 命中 %s 行，而命中的那条是 [%s]\n' "$(grep -c '[[:space:]]' gp/ws.txt)" "$(grep -c '\s' gp/ws.txt)" "$(grep -c '\t' gp/ws.txt)" "$(grep '\t' gp/ws.txt | tr '\n' ' ')"
printf '      拿真空白当 pattern 才命中 %s 行 ｜ -o 数的是空白**字符**：切出 %s 段\n' "$(grep -c "$(printf '\t')" gp/ws.txt)" "$(grep -o '[[:space:]]' gp/ws.txt | wc -l)"
printf '  ★ 所以 \\t 在 BRE 里是**字母 t**（上面命中的是带 t 的那行）；要 tab 就写成 bash 的 $%s\\t%s，让 shell 先把它展开成真的空白\n' "'" "'"
printf '  一行带过（方言不在这格）：本机 -P 也能跑 —— grep -Pc \\<error\\> → %s 行，grep -Pc \\berror\\b → %s 行\n' "$(grep -Pc '\<error\>' gp/word.txt 2>/dev/null)" "$(grep -Pc '\berror\b' gp/word.txt 2>/dev/null)"

echo
echo "=== 5. 只想切片段，却把点写成「任意字符」：静默多命中一堆 ==="
printf '  样本 gp/ver.txt：\n'; sed 's/^/      /' gp/ver.txt
printf '  行数口径：正则 v1.2.3 → %s 行 ｜ 固定串 -F v1.2.3 → %s 行（差 %s）｜ 片段口径：正则 [%s] / -F [%s]\n' "$(grep -c 'v1.2.3' gp/ver.txt)" "$(grep -cF 'v1.2.3' gp/ver.txt)" "$(( $(grep -c 'v1.2.3' gp/ver.txt) - $(grep -cF 'v1.2.3' gp/ver.txt) ))" "$(grep -o 'v1.2.3' gp/ver.txt | tr '\n' ' ')" "$(grep -oF 'v1.2.3' gp/ver.txt | tr '\n' ' ')"
printf '  先弄响：正则版里切出带 x 的段 = %s 段；再报 0：-F 版里切出带 x 的段 = %s 段\n' "$(grep -o 'v1.2.3' gp/ver.txt | grep -c 'x')" "$(grep -oF 'v1.2.3' gp/ver.txt | grep -c 'x')"
printf '  四个点的 IP：[0-9].[0-9].[0-9].[0-9] 切 %s 段 [%s]；把点写成 \\. 只切 %s 段 [%s]\n' "$(grep -o '[0-9].[0-9].[0-9].[0-9]' gp/ver.txt | wc -l)" "$(grep -o '[0-9].[0-9].[0-9].[0-9]' gp/ver.txt | tr '\n' ' ')" "$(grep -o '[0-9]\.[0-9]\.[0-9]\.[0-9]' gp/ver.txt | wc -l)" "$(grep -o '[0-9]\.[0-9]\.[0-9]\.[0-9]' gp/ver.txt | tr '\n' ' ')"
printf '  ★ 切出来的是 0.0.0.1 而不是 10.0.0.1：片段的最左起点由 pattern 说了算，不由「你以为的字段」说了算\n'
printf '  → 按字面切串就 -F，要真的点就 \\.；这一格从头到尾一句报错都没有，全靠行数/段数对账才看得见。\n'

echo
echo "=== 6. 最后一行没有换行符：grep 和 wc 的口径在这儿分家 ==="
printf '  gp/nonl.txt 最后一个字节是换行吗 → %s ｜ 同内容带换行的 gp/wonl.txt → %s\n' "$([ -z "$(tail -c 1 gp/nonl.txt)" ] && echo 是 || echo 否)" "$([ -z "$(tail -c 1 gp/wonl.txt)" ] && echo 是 || echo 否)"
printf '  grep -c gamma（命中吗）：nonl=%s ｜ wonl=%s\n' "$(grep -c 'gamma' gp/nonl.txt)" "$(grep -c 'gamma' gp/wonl.txt)"
printf '  grep -c 空串（grep 自己数行）：nonl=%s ｜ wonl=%s\n' "$(grep -c '' gp/nonl.txt)" "$(grep -c '' gp/wonl.txt)"
printf '  wc -l（本机 uutils，数换行符）：nonl=%s ｜ wonl=%s ｜ 两口径之差：nonl=%s 行 / wonl=%s 行（先响再归零）\n' "$(wc -l < gp/nonl.txt)" "$(wc -l < gp/wonl.txt)" "$(( $(grep -c '' gp/nonl.txt) - $(wc -l < gp/nonl.txt) ))" "$(( $(grep -c '' gp/wonl.txt) - $(wc -l < gp/wonl.txt) ))"
printf '  ★ 谁的口径：grep 把「一段文本」算一行，wc -l 把「一个换行符」算一行 —— 两个数都没错，是尺不一样\n'
printf '  连锁现场：grep gamma nonl.txt 打印时自己把换行补齐了 → 下游 wc -l 读到 %s 行（不是 0）\n' "$(grep 'gamma' gp/nonl.txt | wc -l)"

echo
echo "=== 7. 中文/UTF-8 一行里 grep -o 切出来的是什么 ==="
printf '  样本 gp/zh.txt 共 %s 行、%s 字节 ｜ 一个汉字 %s 字节\n' "$(grep -c '' gp/zh.txt)" "$(wc -c < gp/zh.txt)" "$(printf '订' | wc -c)"
printf '  片段 grep -o 价格[0-9]*元 → 本机 locale %s 段 [%s] ｜ 钉成 C %s 段 → 段数相等吗：%s\n' "$(grep -o '价格[0-9]*元' gp/zh.txt | wc -l)" "$(grep -o '价格[0-9]*元' gp/zh.txt | tr '\n' ' ')" "$(env LC_ALL=C grep -o '价格[0-9]*元' gp/zh.txt | wc -l)" "$([ "$(env LC_ALL=C grep -o '价格[0-9]*元' gp/zh.txt | wc -l)" -eq "$(grep -o '价格[0-9]*元' gp/zh.txt | wc -l)" ] && echo 是 || echo 否)"
printf '  ★ 一个点的口径立刻分家：grep -o . | wc -l —— 本机 locale %s 段（字符）/ LC_ALL=C %s 段（字节），差 %s\n' "$(grep -o '.' gp/zh.txt | wc -l)" "$(env LC_ALL=C grep -o '.' gp/zh.txt | wc -l)" "$(( $(env LC_ALL=C grep -o '.' gp/zh.txt | wc -l) - $(grep -o '.' gp/zh.txt | wc -l) ))"
printf '    第一个片段各几个字节：本机 locale %s（一个整汉字 + 换行）/ LC_ALL=C %s（半个汉字 + 换行）→ ★ C 下切片段会切出半个字\n' "$(grep -o '.' gp/zh.txt | sed -n 1p | wc -c)" "$(env LC_ALL=C grep -o '.' gp/zh.txt | sed -n 1p | wc -c)"
printf '  字符范围 [一-龥] 别猜，跑出来：本机 locale 原文[%s] rc=%s ｜ 钉成 C 反倒命中 %s 行\n' "$(grep -c '[一-龥]' gp/zh.txt 2>&1 | tail -1)" "$(grep -c '[一-龥]' gp/zh.txt >/dev/null 2>&1; echo $?)" "$(env LC_ALL=C grep -c '[一-龥]' gp/zh.txt 2>/dev/null)"
printf '  → 只报确定性的事实：中文面前 locale 就是「单位」本身 —— 命中几行很稳（订单=%s 行），切出几段不稳。\n' "$(grep -c '订单' gp/zh.txt)"

echo
echo "=== 8. 收口：grep 的单位表 + 当场自证 ==="
printf '  你写的          单位是                      想数什么就用它\n'
printf '  grep PAT 文件   命中过的**整行**，一行一遍    看现场\n'
printf '  grep -c PAT     行（一行中招 3 次也只 +1）    有几行中招\n'
printf '  grep -o PAT     片段：行内、不越界、不重叠    出现几次\n'
printf '  grep -l PAT     文件名（单位换了一个量级）    在哪些文件里（见 grepfix.sh 第 6 格）\n'
printf '  ^ $ -w \\< \\>   行首/行尾/词界，全是行内的锚   砍假阳性\n'
grep -o 'e.*r' gp/svc.log > gp/pass1 2>&1; grep -o 'e.*r' gp/svc.log > gp/pass2 2>&1
printf '  自证一（可复现）：第 1 格最贵那条命令连跑两遍，输出一样吗 → %s（%s 字节）\n' "$(cmp -s gp/pass1 gp/pass2 && echo 一字不差 || echo 不一样)" "$(wc -c < gp/pass1)"
printf '  自证二（现场清干净）：删之前 gp 在吗 → %s\n' "$([ -d gp ] && echo 在 || echo 不在)"
rm -rf gp
printf '  删之后在吗 → %s ｜ 本目录里叫 gp 的残留条目 = %s（trap 只是备胎）\n' "$([ -d gp ] && echo 在 || echo 不在)" "$(find . -maxdepth 1 -name gp | wc -l)"
printf '  ★ 一句话带走：grep 判的是「这**一行**有没有」，要段就得 -o，而段永远不跨行；看见 0 先问是哪把尺量的 —— 行数、段数，还是换行符个数。\n'
exit 0
