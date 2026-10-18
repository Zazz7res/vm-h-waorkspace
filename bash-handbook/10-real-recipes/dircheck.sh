#!/usr/bin/env bash
# CHECK: RUN=复现 说明：本支没有每跑必变的量 —— inode 号与 mtime 数值一律只做「相等吗」比较，清单里的 hash 上屏前折成 <hash>，
#        耗时一个字都不上屏；屏上出现的只有 rc、命中次数、相对路径与掩码后的字段形状，两遍逐字相同。
# ============================================================================
# dircheck.sh —— 目录校验与清单九格：字段形状、怪名字、排序口径、key 用哪一段、mtime 骗得过谁
#
#   demo.sh 里这样调：bash dircheck.sh（一律相对文件名：报错原文第一段就是 $0，写绝对路径会把沙箱路径带上屏）
#   安全：只在当前目录建 dc/，每一格在 dc/ 里再开自己的小目录；开头 mkdir -p，结尾 trap 先把不可读目录
#         chmod 回来、再整目录删掉。不碰 ~ 与任何 rc 文件、不联网、不装包、不开端口、不起后台（本支没有 &）。
#   故意不开 set -e：这一支的量点就是「校验到底报了什么」，rc 与被跳过的行数是原料，不是要绕开的障碍。
#
# ★ 清单是「路径 + 内容」这一对东西的快照，所以坑分布在两端：
#   路径端 —— 名字里的空格/中文/换行怎么把一行清单劈开（第 1、2 格）、排序口径不一致时 comm 直接作废（第 3、5 格）、
#             key 取了 basename 就少数一个（第 5 格）、find 撞上不可读目录时那份清单天生不完整（第 7 格）；
#   内容端 —— mtime 可以被 touch -d 按回去而内容已经换了（第 6 格）、-c 对「被改」「被删」「读不了」给的是三种话（第 4 格）。
# ★ 每条「应当 0 / 应当相等」的判据都先喂真违规让计数器响一次；一个没出过非零数的计数器报 0 不算证据。
# ============================================================================

rm -rf dc
mkdir -p dc
trap 'chmod -R u+rwX dc 2>/dev/null; rm -rf dc' EXIT

# 上屏前先折身份：本支全程相对文件名，理论上不带绝对路径；这条是保险（外部程序的报错会自己带上家目录）。
fold() { sed -E "s#$PWD#<沙箱>#g; s#$HOME#<家目录>#g; s#\\b$USER\\b#<用户>#g; s#\\b$(hostname)\\b#<主机名>#g"; }

cell() { printf '\n=== %s ===\n' "$1"; }

show() { sed -E 's/^/      /'; }

# 清单行的掩码：把 hash 那一段折成 <hash>，行首那个反斜杠留着 —— 它正是「名字里有换行/反斜杠」的记号，是证据不是噪声。
maskhash() { sed -E 's/^(\\?)[0-9a-f]{6,}/\1<hash>/' | fold; }

# 文件个数这把尺：名字里能放换行，所以 wc -l 会多数；本支一律用 NUL 个数（-printf '.' 也一样是「按个数」不按行数）。
countfiles() { find "$1" -type f -print0 | tr -dc '\000' | wc -c; }

rec() { printf '%s\n' "$1" >> dc/rc.txt; }
: > dc/rc.txt

TAB=$(printf '\t')

# ============================================================================
cell '1. 清单三件套都在场，但字段形状不是一套：NF 两种切法各得几段'
# ============================================================================
present=0
roster=''
for c in sha256sum md5sum cksum; do
    if command -v "$c" >/dev/null 2>&1; then
        roster="$roster$c=有/"
        present=$((present + 1))
    else
        roster="$roster$c=无/"
    fi
done
printf '  三件套在场名单：%s ｜ 有=%s（期望 3）\n' "${roster%/}" "$present"
mkdir -p dc/sh1
printf hello > dc/sh1/one.txt
printf hello > 'dc/sh1/sp ace.txt'
printf hello > 'dc/sh1/双  空.txt'
nfc=$(sha256sum dc/sh1/one.txt | awk '{print NF}')
nfcs=$(sha256sum dc/sh1/one.txt | awk -F'  ' '{print NF}')
nfm=$(md5sum dc/sh1/one.txt | awk '{print NF}')
nfk=$(cksum dc/sh1/one.txt | awk '{print NF}')
for c in sha256sum md5sum cksum; do
    line=$("$c" dc/sh1/one.txt)
    len=$(printf '%s\n' "$line" | awk '{print length($1)}')
    twosp=$(printf '%s\n' "$line" | grep -c '  ')
    printf '  %s 的掩码形状=[%s] ｜ 第一个字段字符数=%s ｜ 这一行含两连续空格吗=%s 行\n' \
        "$c" "$(printf '%s\n' "$line" | maskhash)" "$len" "$twosp"
done
printf '  同一行清单两种切法各得几段（默认空白切 / 两空格当分隔符切）：sha256 那行 NF=%s 与 %s ｜ cksum 那行 NF=%s\n' \
    "$nfc" "$nfcs" "$nfk"
nfsp=$(sha256sum 'dc/sh1/sp ace.txt' | awk '{print NF}')
nfdbl=$(sha256sum 'dc/sh1/双  空.txt' | awk -F'  ' '{print NF}')
printf '  名字里带一个空格时默认切法 NF=%s ｜ 名字里带两个空格时「两空格切法」NF=%s（都比 2 大，字段号当场作废）\n' \
    "$nfsp" "$nfdbl"
nfail=0
frag=''
for f in 'dc/sh1/one.txt' 'dc/sh1/sp ace.txt' 'dc/sh1/双  空.txt'; do
    p=$(sha256sum "$f" | awk '{print $2}')
    if [ ! -e "$p" ]; then
        nfail=$((nfail + 1))
        [ -z "$frag" ] && frag="$p"
    fi
done
printf '  三个名字里拿 awk 的 $2 当名字后 stat 不到的个数=%s（期望非 0，第一个碎片=[%s]）\n' "$nfail" "$frag"
cutok=0
for f in 'dc/sh1/one.txt' 'dc/sh1/sp ace.txt' 'dc/sh1/双  空.txt'; do
    p=$(sha256sum "$f" | sed -E 's/^[0-9a-f]{64}  //')
    [ "$p" = "$f" ] && cutok=$((cutok + 1))
done
printf '  换成「砍掉前 64 个字符加两个空格」之后名字逐字对得上的个数=%s（期望 3）\n' "$cutok"
printf '  → 三件套的字段数天生不一样：sha256/md5 那两行是 <hash> 两空格 名字（默认切 NF=%s，md5 也是 %s），\n' "$nfc" "$nfm"
printf '    cksum 是 crc 空格 字节数 空格 名字（默认切 NF=%s）—— 一套 awk 语句通吃三件工具必然出错。\n' "$nfk"
printf '  ★ 更要命的是名字那一列：带空格的名字让默认切法 NF 涨到 %s，$2 只剩半截（stat 不到 %s 个，碎片 [%s]）；\n' \
    "$nfsp" "$nfail" "$frag"
printf '    连「两空格切法」也在双空格的名字上给出 NF=%s。取名字只能按定长 hash 砍字符（对得上 %s 个），\n' "$nfdbl" "$cutok"
printf '    别按字段号、也别拿 basename 当 key（第 5 格演它少数一个）。\n'

# ============================================================================
cell '2. 名字带空格/中文/换行时 sha256sum -c 的真实行为（含那行反斜杠转义）'
# ============================================================================
mkdir -p dc/wd
printf a > 'dc/wd/sp ace.txt'
printf b > 'dc/wd/中文.txt'
printf c > "$(printf 'dc/wd/new\nline.txt')"
find dc/wd -type f -print0 | xargs -0 sha256sum | LC_ALL=C sort > dc/wd.sha
mline=$(wc -l < dc/wd.sha)
mfile=$(countfiles dc/wd)
mwrong=$(find dc/wd -type f | wc -l)
printf '  清单行数=%s ｜ 磁盘上文件个数=%s（用 NUL 口径数）｜ 同一批用 find | wc -l 会数到=%s\n' \
    "$mline" "$mfile" "$mwrong"
esc=$(grep -c '^\\' dc/wd.sha)
printf '  行首带反斜杠的转义行=%s 条（带换行的那个名字被这样处理）\n' "$esc"
grep '^\\' dc/wd.sha | maskhash | show
firstchar=$(grep '^\\' dc/wd.sha | cut -c1)
nametext=$(grep '^\\' dc/wd.sha | sed -E 's/^[^ ]*  //')
printf '  那条行的首字符=[%s] ｜ 名字列印出来是=[%s]（真名字里那个换行被写成反斜杠加 n）\n' "$firstchar" "$nametext"
printf CHANGED > 'dc/wd/sp ace.txt'
o=$(sha256sum -c dc/wd.sha 2>&1)
rc=$?
rec "$rc"
printf '%s\n' "$o" | fold | show
printf '  改掉一个内容再 -c：rc=%s\n' "$rc"
okn=$(printf '%s\n' "$o" | grep -c ': OK$')
fad=$(printf '%s\n' "$o" | grep -c ': FAILED$')
warn=$(printf '%s\n' "$o" | grep -c 'WARNING')
checked=$((okn + fad))
skip=$((mline - checked))
printf '  屏上关键词计数：OK 行=%s ｜ FAILED 行=%s ｜ 带 WARNING 的行=%s\n' "$okn" "$fad" "$warn"
printf '  真被校验的行数=%s ｜ 清单行数=%s ｜ 被跳过的行数=%s（期望非 0：那个带换行的名字根本没校）\n' \
    "$checked" "$mline" "$skip"
onlyesc=$(grep '^\\' dc/wd.sha | sha256sum -c 2>&1)
rconly=$?
rec "$rconly"
printf '  只把那条转义行喂给 -c：rc=%s ｜ 原话=[%s]\n' "$rconly" "$(printf '%s\n' "$onlyesc" | fold)"
printf '  → 三笔账都在屏上：清单 %s 行、真校了 %s 行、被跳过 %s 行 —— 带换行名字那行被当成「格式不对」直接放过，\n' \
    "$mline" "$checked" "$skip"
printf '    整体 rc 还是 %s（因为有 FAILED 才非 0）；单独喂它时 rc=%s、原话是「没有一行格式正确」。\n' "$rc" "$rconly"
printf '    结论：-c 会一声不响少校几个文件，rc=0 不等于「清单每行都校过了」，得自己把两个行数摆在一起比。\n'

# ============================================================================
cell '3. 排序不是免费的：同一份清单两种 collation 差几行、find 原序又差几行'
# ============================================================================
mkdir -p dc/ord
for n in f1 f2 f10 Zz aa 中文a 中文b; do
    : > "dc/ord/$n"
done
find dc/ord -type f -printf '%f\n' | LC_ALL=C sort > dc/ord.c.txt
find dc/ord -type f -printf '%f\n' | LC_ALL=zh_CN.UTF-8 sort > dc/ord.zh.txt
find dc/ord -type f -printf '%f\n' > dc/ord.raw.txt
dcnt=$(diff dc/ord.c.txt dc/ord.zh.txt | grep -c '^[<>]')
rcnt=$(diff dc/ord.raw.txt dc/ord.c.txt | grep -c '^[<>]')
self=$(diff dc/ord.c.txt dc/ord.c.txt | grep -c '^[<>]')
printf '  清单行数=%s ｜ C 口径的样子：%s\n' "$(wc -l < dc/ord.c.txt)" "$(tr '\n' '/' < dc/ord.c.txt)"
printf '  zh_CN 口径的样子：%s\n' "$(tr '\n' '/' < dc/ord.zh.txt)"
printf '  find 原序的样子（不排序、直接遍历）：%s\n' "$(tr '\n' '/' < dc/ord.raw.txt)"
printf '  先让判据响一次：同一份 C 排序自己跟自己 diff=%s 行（期望 0，这把尺不虚报）\n' "$self"
printf '  两种 collation 的 diff 命中=%s 行（期望非 0）｜ find 原序 vs C 排序 diff 命中=%s 行\n' "$dcnt" "$rcnt"
pc10=$(grep -n '^f10$' dc/ord.c.txt | cut -d: -f1)
pf2=$(grep -n '^f2$' dc/ord.c.txt | cut -d: -f1)
zh10=$(grep -n '^f10$' dc/ord.zh.txt | cut -d: -f1)
printf '  数字名的坑：C 口径下 f10 在第 %s 行、f2 在第 %s 行（f10 排在 f2 前面，行号差=%s）\n' \
    "$pc10" "$pf2" "$((pf2 - pc10))"
printf '  同一个 f10 换到 zh 口径在第 %s 行 ｜ 两种口径给它的行号差=%s\n' "$zh10" "$((zh10 - pc10))"
printf '  → 三个数摆在一起就明白排序要钉死：同一份清单两种 collation 差 %s 行、find 原序与 C 排序差 %s 行，\n' "$dcnt" "$rcnt"
printf '    而同一把尺重复量自己只差 %s 行 —— diff 命中非 0 时，问题在口径不在内容。\n' "$self"
printf '    comm / join / diff 都要求两边同一个口径：本支之后一律 LC_ALL=C sort（第 5 格先让 comm 响一次）。\n'

# ============================================================================
cell '4. 校验到底校了什么：完好 / 改内容 / 缺文件 / 不可读，四种破坏各跑一遍'
# ============================================================================
runcheck() {
    tag=$1
    dmg=$2
    d="dc/bd-$tag"
    mkdir -p "$d"
    printf k1 > "$d/ok.txt"
    printf k2 > "$d/chg.txt"
    printf k3 > "$d/gone.txt"
    printf k4 > "$d/noread.txt"
    sha256sum "$d/ok.txt" "$d/chg.txt" "$d/gone.txt" "$d/noread.txt" > "$d.sha"
    case $dmg in
        无) : ;;
        改内容) printf DIRTY > "$d/chg.txt" ;;
        缺文件) rm -f "$d/gone.txt" ;;
        不可读) chmod 000 "$d/noread.txt" ;;
    esac
    out=$(sha256sum -c "$d.sha" 2>&1)
    rcn=$?
    rec "$rcn"
    okc=$(printf '%s\n' "$out" | grep -c ': OK$')
    fa=$(printf '%s\n' "$out" | grep -c ': FAILED$')
    fb=$(printf '%s\n' "$out" | grep -c ': FAILED open or read$')
    wn=$(printf '%s\n' "$out" | grep -c 'WARNING')
    printf '  %s：rc=%s ｜ 清单行数=%s ｜ OK 行=%s ｜ FAILED 行=%s ｜ FAILED open or read 行=%s ｜ WARNING 行=%s\n' \
        "$dmg" "$rcn" "$(wc -l < "$d.sha")" "$okc" "$fa" "$fb" "$wn"
    printf '%s\n' "$out" | grep -v ': OK' | fold | show
}
runcheck ok 无
b_rc=$rcn
b_ok=$okc
b_bad=$((fa + fb))
runcheck chg 改内容
c_rc=$rcn
c_ok=$okc
c_fa=$fa
c_fb=$fb
runcheck gone 缺文件
g_rc=$rcn
g_fa=$fa
g_fb=$fb
g_wn=$wn
runcheck noread 不可读
r_rc=$rcn
r_fa=$fa
r_fb=$fb
r_wn=$wn
# 先让「000 条目」这把尺响一次：此刻那个不可读样本还真挂在磁盘上，它必须报非 0，
# 否则第 9 格那句「收尾时 000 条目=0」就只是恒真（没出过数的计数器不算证据）。
d_ring=$(find dc -perm 000 | wc -l)
printf '  先让判据响一次：此刻 dc/ 里权限为 000 的条目=%s（就是上面那个 noread 样本，还没 chmod 回来）\n' "$d_ring"
chmod 644 dc/bd-noread/noread.txt
printf '  当场把权限改回来（trap 之前不留 000）：现在还能读吗=%s ｜ 改回来后 000 条目=%s\n' \
    "$([ -r dc/bd-noread/noread.txt ] && printf 能 || printf 不能)" "$(find dc -perm 000 | wc -l)"
printf '  → 第一行是基线：完好那趟 rc=%s、OK 行=%s、两类 FAILED 相加=%s —— 后面三档的数全跟它对照。\n' \
    "$b_rc" "$b_ok" "$b_bad"
printf '  ★ 三种破坏说的是三种话：改内容只报 FAILED（=%s 行，open or read=%s 行），缺失与不可读各报 open or read（=%s、=%s 行）；\n' \
    "$c_fa" "$c_fb" "$g_fb" "$r_fb"
printf '    两趟的 rc 都是 %s 和 %s，但 WARNING 那句一条讲「算出来的和不一致」、一条讲「读不了」（屏上各 %s 行、%s 行）——\n' \
    "$g_rc" "$r_rc" "$g_wn" "$r_wn"
printf '    所以「校了什么」要按行分类数：只看 rc 会把「文件被删了」「文件被改了」「没权限读」当成同一件事。\n'

# ============================================================================
cell '5. 增量比对：key 必须是整条路径，用 basename 当 key 会少数一个'
# ============================================================================
mkdir -p dc/inc/s1 dc/inc/s2
printf SAME > dc/inc/s1/x.txt
printf SAME > dc/inc/s2/x.txt
printf Y > dc/inc/y.txt
printf Z > dc/inc/z.txt
mktsv() {
    sed -E 's/^([0-9a-f]{64})  (.*)$/\2\t\1/' | LC_ALL=C sort
}
( cd dc/inc && find . -type f -print0 | xargs -0 sha256sum ) | mktsv > dc/incA.tsv
fullA=$(cut -f1 dc/incA.tsv | LC_ALL=C sort -u | wc -l)
basA=$(cut -f1 dc/incA.tsv | sed -E 's#.*/##' | LC_ALL=C sort -u | wc -l)
printf '  清单 A 行数=%s ｜ 拿整条路径当 key 的个数=%s ｜ 拿 basename 当 key 的个数=%s\n' \
    "$(wc -l < dc/incA.tsv)" "$fullA" "$basA"
printf '  basename 口径少数了=%s 个（s1 与 s2 里那一对同名同内容的文件被并成一个 key）\n' "$((fullA - basA))"
mkdir -p dc/plain
printf SAME > dc/plain/x.txt
printf Y > dc/plain/y.txt
printf Z > dc/plain/z.txt
sha256sum dc/plain/x.txt dc/plain/y.txt dc/plain/z.txt | mktsv > dc/plain.tsv
fullP=$(cut -f1 dc/plain.tsv | LC_ALL=C sort -u | wc -l)
basP=$(cut -f1 dc/plain.tsv | sed -E 's#.*/##' | LC_ALL=C sort -u | wc -l)
printf '  同一把尺放在没有同名文件的目录上：整条路径=%s ｜ basename=%s ｜ 少数=%s（期望 0，说明这个计数器会动）\n' \
    "$fullP" "$basP" "$((fullP - basP))"
printf NEW > dc/inc/w.txt
rm -f dc/inc/y.txt
printf ZCHANGED > dc/inc/z.txt
( cd dc/inc && find . -type f -print0 | xargs -0 sha256sum ) | mktsv > dc/incB.tsv
cut -f1 dc/incA.tsv | LC_ALL=C sort > dc/kA.txt
cut -f1 dc/incB.tsv | LC_ALL=C sort > dc/kB.txt
tail -1 dc/kB.txt > dc/badA.txt
head -1 dc/kB.txt >> dc/badA.txt
bad=$(comm dc/badA.txt dc/kB.txt 2>&1)
rbad=$?
printf '  先让判据响一次：拿一份故意没排序的清单去 comm —— rc=%s ｜ 它抱怨「不成序」的行数=%s\n' \
    "$rbad" "$(printf '%s\n' "$bad" | grep -c 'comm:')"
printf '%s\n' "$bad" | fold | show
un=$(comm -13 dc/kA.txt dc/kB.txt | grep -c '.')
de=$(comm -23 dc/kA.txt dc/kB.txt | grep -c '.')
ch=$(join -t "$TAB" dc/incA.tsv dc/incB.tsv | awk -F'\t' '$2 != $3' | grep -c '.')
printf '  两边都按 LC_ALL=C 排好之后再 comm：新增=%s ｜ 删除=%s ｜ 同路径但 hash 变了=%s（三个都期望 1）\n' \
    "$un" "$de" "$ch"
printf '  新增的名字=[%s] ｜ 删除的名字=[%s]\n' \
    "$(comm -13 dc/kA.txt dc/kB.txt | tr '\n' '/')" "$(comm -23 dc/kA.txt dc/kB.txt | tr '\n' '/')"
printf '  路径相同但 hash 变了的那一条=[%s]\n' \
    "$(join -t "$TAB" dc/incA.tsv dc/incB.tsv | awk -F'\t' '$2 != $3 {print $1}')"
printf '  → 两个坑各响了一次：basename 与整路径两种 key 差 %s 个（%s 对 %s），换个没有同名的目录这个差是 %s；\n' \
    "$((fullA - basA))" "$fullA" "$basA" "$((fullP - basP))"
printf '    comm 那一趟 rc=%s —— 排序口径不对它不是算错给你看，是当场作废，所以先钉 LC_ALL=C 再谈 diff。\n' "$rbad"
printf '    增量比对的正解：key 用整条路径、两份同一个 collation 排序，「改动」那一档还得比 hash（路径相同不代表内容相同）。\n'

# ============================================================================
cell '6. mtime 骗得过谁、inode 证明 mv 只换了名字'
# ============================================================================
mkdir -p dc/mt
printf OLD > dc/mt/v1.txt
touch -d '2020-01-01 00:00:00' dc/mt/v1.txt
m0=$(stat -c %Y dc/mt/v1.txt)
h0=$(sha256sum dc/mt/v1.txt | cut -c1-64)
printf ANOTHER > dc/mt/v2.txt
touch -d '2021-06-06 06:06:06' dc/mt/v2.txt
m1=$(stat -c %Y dc/mt/v2.txt)
guard=$([ "$m0" -eq "$m1" ] && printf 1 || printf 0)
printf '  先让判据响一次（两个时间戳本来就不一样的样本）：mtime 相等吗=%s（相等个数=%s，期望 0）\n' \
    "$([ "$m0" -eq "$m1" ] && printf 相等 || printf 不等)" "$guard"
printf DIRTY > dc/mt/v1.txt
touch -d '2020-01-01 00:00:00' dc/mt/v1.txt
m2=$(stat -c %Y dc/mt/v1.txt)
h1=$(sha256sum dc/mt/v1.txt | cut -c1-64)
mtsame=$([ "$m0" -eq "$m2" ] && printf 1 || printf 0)
hsame=$([ "$h0" = "$h1" ] && printf 1 || printf 0)
printf '  内容改了、再把时间戳按回原值：mtime 相等吗=%s ｜ hash 相等吗=%s\n' \
    "$([ "$mtsame" -eq 1 ] && printf 相等 || printf 不等)" "$([ "$hsame" -eq 1 ] && printf 相等 || printf 不等)"
printf '  两个口径各数一次「相等」的个数：mtime 口径=%s ｜ hash 口径=%s（一个骗得过、一个骗不过）\n' "$mtsame" "$hsame"
i0=$(stat -c %i dc/mt/v2.txt)
hv2=$(sha256sum dc/mt/v2.txt | cut -c1-64)
cbefore=$(countfiles dc/mt)
mv -- dc/mt/v2.txt dc/mt/v3.txt
i1=$(stat -c %i dc/mt/v3.txt)
hv3=$(sha256sum dc/mt/v3.txt | cut -c1-64)
cafter=$(countfiles dc/mt)
isame=$([ "$i0" -eq "$i1" ] && printf 1 || printf 0)
hasame=$([ "$hv2" = "$hv3" ] && printf 1 || printf 0)
printf '  mv 换名之后：inode 相等吗=%s ｜ hash 相等吗=%s ｜ 老名字 v2.txt 还在吗=%s\n' \
    "$([ "$isame" -eq 1 ] && printf 相等 || printf 不等)" \
    "$([ "$hasame" -eq 1 ] && printf 相等 || printf 不等)" \
    "$([ -e dc/mt/v2.txt ] && printf 在 || printf 不在)"
# 个数分两趟各数一次（都在 mv 的那一侧取）：只印「换名后」那两个数是自说自话，
# 换名前那个数当时没读，就没有「改名不增不减」这笔账。
printf '  这一趟「相等」的个数：inode 口径=%s ｜ hash 口径=%s ｜ 文件个数 换名前=%s 换名后=%s（差=%s）\n' \
    "$isame" "$hasame" "$cbefore" "$cafter" "$((cafter - cbefore))"
printf '  dc/mt 里现在的文件：%s\n' "$(find dc/mt -type f -printf '%f\n' | LC_ALL=C sort | tr '\n' '/')"
printf '  → 两个口径并排才看得出谁在骗人：内容换了、时间按回去之后 mtime 说「相等」（个数=%s），hash 说「不等」（个数=%s）——\n' \
    "$mtsame" "$hsame"
printf '    mtime 能被 touch -d 单方面改写，它当不了内容的证据，只能当「大概什么时候动过」的线索；\n'
printf '    而 mv 那一趟 inode 与 hash 都相等（各 %s 个）、文件个数前后差=%s：只有名字变了 —— 改名动的是目录项，不是数据。\n' \
    "$isame" "$((cafter - cbefore))"

# ============================================================================
cell '7. find 跟不跟链接、撞上不可读目录：清单能不能自证完整'
# ============================================================================
mkdir -p dc/ln/sub dc/ln_out
printf REAL > dc/ln_out/real.txt
ln -sfn ../ln_out/real.txt dc/ln/link1
printf P > dc/ln/sub/plain.txt
mkdir -p dc/ln/sub/noread
printf H > dc/ln/sub/noread/hidden.txt
chmod 000 dc/ln/sub/noread
err7=$(find dc/ln -type f 2>&1 >/dev/null)
rc7=$?
rec "$rc7"
n7=$(find dc/ln -type f 2>/dev/null | wc -l)
nl7=$(find -L dc/ln -type f 2>/dev/null | wc -l)
all7=$(find dc/ln 2>/dev/null | wc -l)
errl=$(printf '%s\n' "$err7" | grep -c '.')
printf '  find -type f 命中=%s 行 ｜ find -L -type f 命中=%s 行 ｜ 不带 -type 的全部条目=%s 行\n' "$n7" "$nl7" "$all7"
printf '  撞上 000 目录那趟：rc=%s ｜ stderr 行数=%s ｜ 原话如下：\n' "$rc7" "$errl"
printf '%s\n' "$err7" | fold | show
linkhit=$(find dc/ln -type f 2>/dev/null | grep -c 'link1')
linkall=$(find dc/ln 2>/dev/null | grep -c 'link1')
printf '  链接指向的内容当场读得到吗=[%s] ｜ -type f 的清单里有它=%s 行 ｜ 不带 -type 的清单里有它=%s 行\n' \
    "$(cat dc/ln/link1)" "$linkhit" "$linkall"
chmod 700 dc/ln/sub/noread
n7b=$(find dc/ln -type f 2>/dev/null | wc -l)
nl7b=$(find -L dc/ln -type f 2>/dev/null | wc -l)
madeN=$(countfiles dc/ln)                                         # 不跟链接的 NUL 口径
madeL=$(find -L dc/ln -type f -print0 | tr -dc '\000' | wc -c)     # 跟链接的 NUL 口径
printf '  把权限改回来之后：find -type f 命中=%s 行 ｜ -L 命中=%s 行 ｜ 之前被藏起来的文件=%s 个\n' \
    "$n7b" "$nl7b" "$((n7b - n7))"
printf '  先让判据响一次：不跟链接的清单只有 %s 行，跟链接的口径有 %s 个 —— 少掉的就是那个 link1，差=%s\n' \
    "$n7b" "$madeL" "$((madeL - n7b))"
printf '  两边同口径才谈自证：-L 清单行数=%s ｜ -L 的 NUL 个数=%s ｜ 差=%s（期望 0，上一行刚响过 %s）\n' \
    "$nl7b" "$madeL" "$((nl7b - madeL))" "$((madeL - n7b))"
printf '  → 三个数各管一件事：跟链接（-L）比不跟多 %s 个（那个指向别处的 link1 只有 -L 才算文件，\n' "$((nl7 - n7))"
printf '    不跟链接时 -type f 清单提到它 %s 行、不带 -type 的提到它 %s 行）；目录不可读时 rc=%s、stderr 有 %s 行，\n' \
    "$linkhit" "$linkall" "$rc7" "$errl"
printf '    而 stdout 那份清单看着照样「成功」，它少掉的正是后来数出来的那 %s 个 —— 同口径自证那次差=%s 才算成立。\n' \
    "$((n7b - n7))" "$((nl7b - madeL))"

# ============================================================================
cell '8. 规模与代价只报计数：50 个小文件的清单与目录条目'
# ============================================================================
mkdir -p dc/many
for i in 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 \
         21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 \
         41 42 43 44 45 46 47 48 49 50; do
    printf 'row-%s' "$i" > "dc/many/f$i.txt"
done
made=$(countfiles dc/many)
ent=$(find dc/many | wc -l)
find dc/many -type f -print0 | xargs -0 sha256sum | LC_ALL=C sort > dc/many.sha
ml=$(wc -l < dc/many.sha)
o8=$(sha256sum -c dc/many.sha 2>&1)
r8=$?
rec "$r8"
ok8=$(printf '%s\n' "$o8" | grep -c ': OK$')
bad8=$(printf '%s\n' "$o8" | grep -c ': FAILED')
printf '  建出的文件数=%s ｜ 目录条目数（含目录本身）=%s ｜ 清单行数=%s ｜ 条目数减清单行数=%s\n' \
    "$made" "$ent" "$ml" "$((ent - ml))"
printf '  这份清单再 -c：rc=%s ｜ OK 行=%s ｜ 带 FAILED 的行=%s\n' "$r8" "$ok8" "$bad8"
printf '  清单第一行掩码后=[%s]\n' "$(head -1 dc/many.sha | maskhash)"
printf '  → 这一格一句断言都没有，只有四个计数：%s / %s / %s / %s —— 规模在本章只以「多少个、多少行」上屏，\n' \
    "$made" "$ent" "$ml" "$ok8"
printf '    耗时一秒都不印（第 09 章量过：那种数两遍就不一样，判不了也抄不得）；差值 %s 就是目录自己那一行。\n' "$((ent - ml))"

# ============================================================================
cell '9. 收账：本支量到的退出码全在台账里，先证明那个「非 0」计数器会响'
# ============================================================================
n=$(wc -l < dc/rc.txt)
z=$(grep -c '^0$' dc/rc.txt)
nz=$(grep -vc '^0$' dc/rc.txt)
printf '  台账条数=%s ｜ rc=0 的=%s ｜ 非 0 的=%s ｜ 两个加起来等于条数吗=%s\n' \
    "$n" "$z" "$nz" "$([ "$((z + nz))" -eq "$n" ] && printf 等于 || printf 不等于)"
paste -sd ' ' dc/rc.txt | sed -E 's/^/      出场顺序：/' | show
cp dc/rc.txt dc/rc.bak
printf '1\n' >> dc/rc.txt
ring=$(grep -vc '^0$' dc/rc.txt)
mv dc/rc.bak dc/rc.txt
back=$(grep -vc '^0$' dc/rc.txt)
printf '  ★ 那个「非 0 的个数」会不会恒等于 0？塞一条真违规（1）进台账：从 %s 变成 %s（响了）；撤掉探针回到 %s\n' \
    "$nz" "$ring" "$back"
resfile=$(countfiles dc)
zero0=$(find dc -perm 000 | wc -l)
printf '  收尾前 dc/ 里的文件数=%s ｜ 其中还是 000 权限的条目=%s（期望 0；第 4 格同一把尺响过 %s）\n' \
    "$resfile" "$zero0" "$d_ring"
printf '  → 台账上 %s 条码：%s 条是 0、%s 条非 0 —— 非 0 那几条就是校验失败、-c 拿到坏清单、find 撞不可读目录，\n' "$n" "$z" "$nz"
printf '    每一条都是本支想要的东西而不是事故；计数器先响过一次（%s → %s）才敢报它回到 %s。\n' "$nz" "$ring" "$back"
printf '    trap 那条先 chmod -R u+rwX dc 再 rm -rf dc：000 条目从 %s 降到 %s 就是它删得掉的前提。\n' "$d_ring" "$zero0"

printf '\n[dircheck.sh 自查] 本脚本格数=%s ｜ dc/ 里现有文件数=%s（跑完由 trap 整目录删除）\n' \
    "$(grep -c '^cell ' "$0")" "$(countfiles dc)"
exit 0
