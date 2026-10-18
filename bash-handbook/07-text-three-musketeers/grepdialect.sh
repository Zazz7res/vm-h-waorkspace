#!/usr/bin/env bash
# ============================================================================
# grepdialect.sh —— 同一个正则，在三套方言里是三份结果（第 07 章 grep 半区·之一）
#
#   demo.sh 里这样调：bash grepdialect.sh
#   安全：只在本目录下建 grepdemo/ 沙箱，全程相对路径；trap 在退出时删掉，最后一段当场自证。
#         不起后台常驻进程、不开端口、不联网、不装包、不碰 ~/.bashrc 之类的任何家目录文件。
#   故意不加 set -e：本脚本一大半内容就是「grep 返回 1 / 返回 2」的现场，加了 set -e
#         第一格静默不匹配就直接把脚本收掉，后面的方言对照全看不到。要看 set -e 的现场，
#         一律用 bash -ec '...' 起子进程（第 6 段），开关只活在那个进程里。
#   同理不加 set -u、不加全局 pipefail：$? 是本章要读的数据；管道里每一段的码都要单独拿。
#
# ★ 本机事实（都是探测过的，不是抄的）：grep 是 GNU grep 3.12（-P 可用）；sed 是 GNU 4.9；
#   wc/sort/tr/ls 是 uutils coreutils 0.8.0（Rust 实现，报错永远是英文、不跟着 locale 走），
#   而 grep 的报错跟着 locale 走（本机默认 zh_CN.UTF-8 时是中文）。awk 是 mawk，本章另有人写，
#   本脚本一个 sed / awk 都不调用。屏幕上的数字全部是当场测出来的，没有一个是硬编码的结论。
# ============================================================================

rm -rf grepdemo
mkdir -p grepdemo
trap 'rm -rf grepdemo' EXIT

# cell <标签> <文件> <正则> [方言开关...] —— 计数、退出码、以及「这一格属于哪种失败」
# ★ 关键设计一：标签是人写的，第三列「实际开关」是命令真正吃进去的那串 —— 两个都印出来，
#   只写标签没传开关的格子会在屏幕上自己露出来（标签说 ERE、开关列却写着「无开关=BRE」）。
# ★ 关键设计二：先分清「报错(2)」「静默不匹配(1)」「匹配(0)」这三种码，再看数字，
#   否则「0 行」到底是方言不认这个写法、还是这个写法认了但样本里没有，根本分不开。
cell() {
    local tag=$1 file=$2 pat=$3 out rc verdict sw
    shift 3
    sw="$*"
    [ -z "$sw" ] && sw='(无开关=BRE)'
    out=$(grep "$@" -c -- "$pat" "$file" 2>/dev/null); rc=$?
    case $rc in
        0) verdict=匹配 ;;
        1) verdict='静默不匹配(1)' ;;
        2) verdict='报错(2)' ;;
        *) verdict="其它($rc)" ;;
    esac
    printf '  %-11s %-14s 开关[%-14s] 命中 %-3s 行 ｜ rc=%-2s ｜ %s\n' \
        "$tag" "$pat" "$sw" "${out:-—}" "$rc" "$verdict"
}

printf 'ab\nabc\naxb\na.b\nhello\n'                     > grepdemo/p.txt
printf 'aaa\naab\nab\ncolor\ncoloor\n价格 12 元\n'      > grepdemo/quant.txt
printf '价格 12 元\n订单号 A123\nno digits here\nA+B\n' > grepdemo/pc.txt
printf 'a.b\naxb\n配置 a.b 生效\na--b\n版本a.b中文\n'   > grepdemo/meta.txt
printf 'hello\nhelloworld\nsay hello again\nhell\n'     > grepdemo/w.txt

echo
echo "=== 1. 现场：一个 grep 里装着三套正则引擎，开关决定用哪一套 ==="
printf '  本机 grep：%s\n' "$(grep --version | head -1)"
printf '  -P（PCRE 引擎）在本机可用吗 → %s\n' \
    "$(grep -P 'x' /dev/null >/dev/null 2>&1; [ $? = 2 ] && echo '不可用(2)' || echo '可用（rc≠2 就说明引擎在，只是没匹配）')"
printf '  沙箱样本：p.txt=%s 行 ｜ quant.txt=%s 行 ｜ pc.txt=%s 行 ｜ meta.txt=%s 行 ｜ w.txt=%s 行\n' \
    "$(wc -l < grepdemo/p.txt 2>/dev/null || echo 0)" "$(wc -l < grepdemo/quant.txt)" \
    "$(wc -l < grepdemo/pc.txt)" "$(wc -l < grepdemo/meta.txt)" "$(wc -l < grepdemo/w.txt)"
printf '  p.txt 内容：\n'
sed 's/^/      /' grepdemo/p.txt
printf '  → 三种方言：不带开关 = BRE（POSIX 基础型，反斜杠才是元字符），-E = ERE（网上抄的一行命令多半是它），-P = PCRE（另一套语言）。\n'
printf '  → 抄来的命令换一台机器就不响，九成是「开关和方言不匹配」，不是「样本里没有」——下面一格一格数给你看。\n'

echo
echo "=== 2. 「或」：\\| 和 | 在 BRE 里正好相反 ==="
printf '  样本 p.txt，找「ab 或 axb」：\n'
cell 'BRE 默认'   grepdemo/p.txt 'ab\|axb'
cell 'BRE 默认'   grepdemo/p.txt 'ab|axb'
cell 'ERE -E'     grepdemo/p.txt 'ab|axb' -E
cell 'ERE -E'     grepdemo/p.txt 'ab\|axb' -E
cell 'PCRE -P'    grepdemo/p.txt 'ab|axb' -P
printf '  → 同一串字符，BRE 里 | 是字面竖线、ERE 里 \\| 是字面竖线：两格都是 0 行、rc=1，一句报错都没有。\n'
printf '  → 也就是说「抄来的 -E 命令加了 \\|」不会失败，只会**安静地**一行都不给你。这是本章最贵的一课。\n'

echo
echo "=== 3. 分组和量词：+ ? {n,m} 在 BRE 里都要戴反斜杠 ==="
printf '  样本 quant.txt（aaa/aab/ab/color/coloor）：\n'
cell 'BRE 默认'   grepdemo/quant.txt 'a\{2,3\}'
cell 'BRE 默认'   grepdemo/quant.txt 'a{2,3}'
cell 'ERE -E'     grepdemo/quant.txt 'a{2,3}' -E
cell 'ERE -E'     grepdemo/quant.txt 'a\{2,3\}' -E
cell 'BRE 默认'   grepdemo/quant.txt 'a\+b'
cell 'BRE 默认'   grepdemo/quant.txt 'a+b'
cell 'ERE -E'     grepdemo/quant.txt 'a+b' -E
cell 'BRE 默认'   grepdemo/quant.txt 'colou\?r'
cell 'BRE 默认'   grepdemo/quant.txt 'colou?r'
cell 'ERE -E'     grepdemo/quant.txt 'colou?r' -E
printf '  → 数一下上面的 rc：所有「少戴/多戴一个反斜杠」的格子全是 rc=1 而不是 rc=2。\n'
printf '  ★ BRE 里裸 { ( ? 是**字面字符**（不是语法错），所以它照样跑完、照样返回「没匹配」——写反了没人告诉你。\n'

echo
echo "=== 4. 只有 PCRE 认的，和只有 GNU 认的：不认的那边一句报错都没有 ==="
printf '  样本 pc.txt（含中文与数字），找数字：\n'
cell 'ERE -E'     grepdemo/pc.txt '\d+' -E
cell 'BRE 默认'   grepdemo/pc.txt '[0-9]\{1,\}'
cell 'ERE -E'     grepdemo/pc.txt '[0-9]+' -E
cell 'PCRE -P'    grepdemo/pc.txt '\d+' -P
printf '  → ★ 同一串 \\d+ 在两边的下场：ERE 读成「字母 d 一个以上」（命中 %s 行，rc=0），PCRE 读成数字（命中 %s 行）。\n' \
    "$(grep -Ec '\d+' grepdemo/pc.txt 2>/dev/null)" "$(grep -Pc '\d+' grepdemo/pc.txt 2>/dev/null)"
printf '    ERE 那一格命中的原文=[%s] ← 它不是「没匹配」，是**匹配了错的东西**，而且一声不响。\n' \
    "$(grep -E '\d+' grepdemo/pc.txt | tr '\n' ' ')"
printf '  前瞻 (?= ) 这种写法：\n'
cell 'PCRE -P'    grepdemo/pc.txt 'A(?=[0-9])' -P
cell 'ERE -E'     grepdemo/pc.txt 'A(?=[0-9])' -E
printf '  ERE 那一格 -c 看不到报错，把两路输出和退出码分开捞出来看：\n'
werr=$(grep -Ec 'A(?=[0-9])' grepdemo/pc.txt 2>&1 >/dev/null)
wout=$(grep -Ec 'A(?=[0-9])' grepdemo/pc.txt 2>/dev/null); wrc=$?
printf '      stderr=[%s] ｜ stdout=[%s] ｜ rc=%s\n' "${werr#*: }" "$wout" "$wrc"
printf '      ← ★ 有警告、退 1，不是退 2：grep 认为「这写法我能懂，只是样本里没有」。警告不改变结论，所以只看 rc 会漏，只看文本也会漏。\n'
printf '  反过来：GNU 的 \\< \\>（整词）搬到 -P 上会怎样（样本 p.txt，只有一行 hello）：\n'
cell 'BRE 默认'   grepdemo/p.txt '\<hello\>'
cell 'ERE -E'     grepdemo/p.txt '\<hello\>' -E
cell 'PCRE -P'    grepdemo/p.txt '\<hello\>' -P
cell 'BRE 默认'   grepdemo/p.txt '\bhello\b'
cell 'ERE -E'     grepdemo/p.txt '\bhello\b' -E
cell 'PCRE -P'    grepdemo/p.txt '\bhello\b' -P
printf '  → PCRE 里 \\< 是「字面 <」，于是 GNU 的写法搬到 -P 变成 0 行、rc=1，还是不报错。\n'
printf '  ★ 本机实测（别信二手话）：\\b 在 BRE/ERE/PCRE 三边都能当词边界，但 \\< \\> 只有前两边认。同一份脚本要跨 -P 时，用 \\b 或干脆用 -w。\n'

echo
echo "=== 5. 元字符陷阱：. 是「任意字符」，-F 才是「这个串」；-w 只管整词 ==="
printf '  样本 meta.txt（5 行，其中有字面 a.b 的，也有 axb / a--b 的）：\n'
sed 's/^/      /' grepdemo/meta.txt
cell '正则'       grepdemo/meta.txt 'a.b'
cell '固定串 -F'  grepdemo/meta.txt 'a.b' -F
printf '  → 上面两格差 %s 行，多出来的那行原样列在这里：[%s] —— 这就是「. 被当元字符」的代价。\n' \
    "$(( $(grep -c 'a.b' grepdemo/meta.txt) - $(grep -cF 'a.b' grepdemo/meta.txt) ))" \
    "$(grep 'a.b' grepdemo/meta.txt | grep -vF 'a.b' | tr '\n' ' ')"
printf '    顺带一格：a--b 两把尺都没命中，因为 . 只吃**一个**字符，a 和 b 中间隔着两个。\n'
printf '  → 要按字面找「配置项 a.b」「1.2.3」「(fix me)」「$HOME」，一律 -F，别在正则里堆反斜杠杂技（-F 下这些字符全是字面，且快得多）。\n'
printf '  整词与非整词（样本 w.txt）：\n'
cell '不加 -w'    grepdemo/w.txt 'hello'
cell '加 -w'      grepdemo/w.txt 'hello' -w
printf '  → 差的那 %s 行就是 helloworld：子串算命中，整词不算。搜 error / warn / id 这类短词时，-w 经常是把假阳性砍掉 80%% 的那个开关。\n' \
    "$(( $(grep -c 'hello' grepdemo/w.txt) - $(grep -cw 'hello' grepdemo/w.txt) ))"

echo
echo "=== 6. ★ 通用判据：报错(2) ≠ 不匹配(1) ≠ 成功(0) —— 三个码各撞一次 ==="
r0=$(grep -c 'ab' grepdemo/p.txt 2>/dev/null); rc0=$?
r1=$(grep -c 'zzzz' grepdemo/p.txt 2>/dev/null); rc1=$?
e2=$(grep -c '[' grepdemo/p.txt 2>&1); rc2=$?
ef=$(grep -c 'x' grepdemo/nosuchfile.txt 2>&1); rcf=$?
printf '  ① 正常命中     ：rc=%s（%s 行）\n' "$rc0" "$r0"
printf '  ② 正常没命中   ：rc=%s（%s 行）← 这不是失败，是答案\n' "$rc1" "$r1"
printf '  ③ 正则语法错   ：rc=%s ｜ 本机 locale 原文=[%s]\n' "$rc2" "${e2#*: }"
printf '  ③b 同一格钉英文对照（只在这一条命令前加 env，脚本整体不动 locale）：[%s]\n' \
    "$(env LC_ALL=C.UTF-8 grep -c '[' grepdemo/p.txt 2>&1)"
printf '  ④ 文件不存在   ：rc=%s ｜ 原文=[%s]\n' "$rcf" "${ef#*: }"
sur=$(grep -c '*' grepdemo/p.txt 2>&1); rcs=$?
printf '  ⑤ 反直觉的一格：grep \x27*\x27 在本机不报错 → rc=%s ｜ 命中=[%s] 行 ｜ 原文=[%s]\n' "$rcs" "${sur%%$'\n'*}" "$(printf '%s' "$sur" | tail -1)"
printf '  → ★ 行首的 * 被 GNU grep 当字面星号，所以它走的是「没匹配(1)」而不是「语法错(2)」。想靠「有没有报错」判断正则写错没写错，本机判不出来。\n'
sed_e=$(bash -ec 'grep -r x grepdemo/nosuchdir' 2>&1); rcse=$?
plain=$(bash -c 'grep -r x grepdemo/nosuchdir' >/dev/null 2>&1; echo $?)
printf '  ⑥ set -e 现场（开关只在子进程里活着）：bash -ec 那一句 → rc=%s ｜ 原文=[%s]\n' "$rcse" "${sed_e#*: }"
printf '     同一个命令不加 -e → rc=%s（本脚本自己没被打断，所以上面 ①②③④ 才跑得完）\n' "$plain"
printf '  ★ ⑤/⑥ 这两格合起来就是判据：**只有退出码是出口**。2 才要去读报错文本，1 是「确实没有」，而「写错了但安静返回 1」这种格子必须靠数字对账才能抓到。\n'

echo
echo "=== 7. 收口：先自证现场清干净，再给能带走的判据 ==="
grep -rc . grepdemo > /dev/null 2>&1 &
jbg=$!
printf '  本脚本自己的作业表 jobs -p | wc -l = %s（先证明这个问法不是恒真的 0）\n' "$(jobs -p | wc -l)"
wait "$jbg"; rcj=$?
printf '  wait 之后 jobs -p | wc -l = %s ｜ 那个后台 grep 的退出码 = %s（必须是 0 和 0）\n' "$(jobs -p | wc -l)" "$rcj"
printf '  ★ 最后证明「开关」那一列不是装饰：故意把标签写成 ERE、却不传 -E，屏幕上两列自己打脸：\n'
cell 'ERE -E（故意漏开关）' grepdemo/p.txt 'ab|axb'
printf '    标签说 ERE，开关列却印着 (无开关=BRE)，命中也从 3 行掉回 0 行 —— 有这一列，标签就骗不了屏幕。\n'
printf '  沙箱 grepdemo 删之前还在吗 → %s\n' "$([ -d grepdemo ] && echo 在 || echo 不在)"
rm -rf grepdemo
printf '  删之后还在吗 → %s ｜ 本目录里叫 grepdemo 的残留条目 = %s（trap 只是备胎）\n' \
    "$([ -d grepdemo ] && echo 在 || echo 不在)" "$(find . -maxdepth 1 -name grepdemo | wc -l)"
printf '  ① 抄来的一行命令先认方言：不带开关是 BRE，反斜杠的含义和 ERE **正好相反**；带 -P 是第三套语言。\n'
printf '  ② 跨方言最大的坑不是报错，是**安静地返回 1**：\\| 到了 ERE、裸 + ? {} 到了 BRE、\\<\\> 到了 -P，全是 0 行 0 句报错。\n'
printf '    比 0 行更阴的是「两边都认、意思不同」：\\d 到了 ERE 是字母 d，它给你**命中 1 行的错答案**（第 4 格那行 no digits here）。\n'
printf '  ③ 数「几行」而不是数「行不行」：每格都打 rc + 命中行数，0 行配 rc=1 是答案，0 行配 rc=2 是事故，两者绝不能混着看。\n'
printf '  ④ 按字面找串就 -F，找整词就 -w；这两把尺比往正则里堆反斜杠可靠。\n'
printf '  ⑤ 报错文本跟着 locale 变（grep 中文、uutils 的 wc 永远英文），退出码不变 —— 所以脚本里只拿 0/1/2 做分支，别拿文本做分支。\n'
printf '  ⑥ 每格那一列「开关」是从命令里拿出来的，不是标签：标签是人手写的，它会错；实际传进去的开关不会。\n'
exit 0
