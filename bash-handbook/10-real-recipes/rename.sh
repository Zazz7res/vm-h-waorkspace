#!/usr/bin/env bash
# CHECK: RUN=复现 说明：本支每跑必变的量一个都不上屏 —— inode 号、pid、耗时、字节偏移全都不印，
#        改名前后的对账一律走「名字 + 字节数」的 cksum 指纹（同名同大小 → 同一指纹）与 NUL 口径计数；
#        所以两遍输出应当逐字相同。第 10 格那个「起点缺席」分支也一样：单独跑两遍都走同一条分支。
# ============================================================================
# rename.sh —— 批量改名的「名字」从哪儿来：glob 看不见点、换行把一个名字劈成两段、
#              开头的 - 会被当成旗号、跑第二遍会把刚改好的再改一遍
#
#   demo.sh 里这样调：bash rename.sh          （一律相对文件名，不写绝对路径）
#   安全：只在当前目录建 rn/，开头 rm -rf rn 保证可重跑，结尾 trap 'rm -rf rn' EXIT 收走。
#         唯一的临时名单文件 rnplan 也在 trap 里一起删。改名动作只发生在 rn/ 里面，
#         那里的每一个文件都是本脚本当场造的、当场删的。不碰 ~、不写 /tmp、不联网、
#         不装包、不起监听、不 sleep、不用 crontab/systemctl。
#
# ★ 这一支的账（以为 X ／ 实测 Y）：
#   以为 ls 出来的就是文件名 —— 实测一个带空格的名字被切成两段，段数比文件数还多；
#   以为 * 就是「所有文件」 —— 实测它以点开头的看不见，差的正好是隐藏文件数；
#   以为 -rf 只是个普通名字 —— 实测 mv 先把它当旗号，rc=1，文件原地一动不动；
#   以为名字里不会有换行     —— 实测有了一条 find -printf 就打两行，段数当场虚高；
#   以为跑第二遍顶多没改动   —— 实测不加守卫的第二遍把刚改好的三个又往后推了三位；
#   以为 mv 撞名会问我       —— 实测它一声不响把对面那个覆盖掉，文件数当场少一个。
#   ★ 本机没有 rename（Perl 那个）也没有 prename，所以全是 mv + glob + find 手拼；
#     wc/ls/sort/mkdir 是 uutils coreutils 0.8.0，mv 才是 GNU 的，awk 是 mawk ——
#     每个选项都是先单独跑过一次才敢用，长选项一律不碰。
#   ★ 判据一律写成 rc / 计数 / 掩码后的形状 / 布尔；报错原文照印，但结论不依赖它用什么字写。
# ============================================================================

rm -rf rn
mkdir -p rn
trap 'rm -rf rn rnplan' EXIT

# fold：凡可能带路径/用户名/主机名的输出，上屏前先过它折成占位符。
#       本机 hostname 与 $USER 是**同一个串**，所以「用户」和「主机名」两条规则必须都在，
#       只留一条的话另一半照样漏在屏上。
fold() { sed -E "s#$HOME#<家目录>#g; s#\\b$USER\\b#<用户>#g; s#$(hostname)#<主机名>#g"; }

# wipe：每格开头把 rn 清回空目录 —— 上一格留下的名字会污染这一格的计数。
wipe() { rm -rf rn; mkdir -p rn; }

# vis：只给「上屏」用。名字里的换行原样印出来会把一行劈成两行，读者就没法数了。
vis() { local s=$1; s=${s//$'\n'/[换行]}; printf '%s' "$s"; }

# realcount：真文件数的口径 —— 数 find -print0 吐出的 NUL 个数。
#            名字里塞空格、塞换行、塞什么都骗不了它：一条记录就是一个 NUL。
realcount() { find "$1" -maxdepth 1 -type f -print0 | tr -dc '\0' | wc -c; }

# jpgcount：同样的 NUL 口径，只数 .jpg —— 每格改名前后都用它对账，避免 wc -l 被换行名虚高。
jpgcount() { find rn -maxdepth 1 -type f -name '*.jpg' -print0 | tr -dc '\0' | wc -c; }

# digest：目录清单的指纹 = 「名字 + 字节数」按 C 排序后 cksum。
#         ★ 绝不拿 mtime 当证据：改名本身就动它，两趟之间它更是天生要变的。
digest() { find "$1" -maxdepth 1 -type f -printf '%f\t%s\n' | LC_ALL=C sort | cksum | cut -d' ' -f1,2; }

echo "=== 1. 名字从哪儿来：同一个目录，三种取法切出三、二、三段，段数就是不等于文件数 ==="
wipe
touch 'rn/a b.txt' 'rn/c.txt' 'rn/.hidden'
printf '  目录里造了 3 个东西：a b.txt ／ c.txt ／ .hidden（第三个以点开头）\n'
printf '  口径甲 for f in $(ls rn)   —— 未加引号的命令替换，按空白切：\n'
segs=(); for f in $(ls rn); do segs+=("$f"); done
i=0; for s in "${segs[@]}"; do i=$((i+1)); printf '        第 %s 段 [%s]\n' "$i" "$(vis "$s")"; done
printf '  口径乙 for f in rn/*       —— glob 展开，整条路径是一个词：\n'
g=(); for f in rn/*; do g+=("$f"); done
i=0; for s in "${g[@]}"; do i=$((i+1)); printf '        第 %s 段 [%s]\n' "$i" "$(vis "$s")"; done
printf '  口径丙 find rn -printf %%f  —— 一个文件一行，读的时候还是按行切：\n'
# ★ 这里加了一道 sort，只为让「第 N 段」的打印顺序可复现：find 的出场顺序跟着目录物理排布走，
#   换个目录就会变；段的**条数**与结论不受它影响，但两遍要逐字相同就必须先定序。
n=0; while IFS= read -r f; do n=$((n+1)); printf '        第 %s 段 [%s]\n' "$n" "$(vis "$f")"; done < <(find rn -maxdepth 1 -type f -printf '%f\n' | LC_ALL=C sort)
nls=$(ls rn | wc -l)
fake=0; for s in "${segs[@]}"; do [ -e "rn/$s" ] || fake=$((fake+1)); done
printf '  → 断言：真文件数=%s ｜ ls 列出的行数=%s ｜ ls 切成段=%s（其中不是文件的段=%s）｜ glob 段=%s ｜ find 行=%s\n' \
    "$(realcount rn)" "$nls" "${#segs[@]}" "$fake" "${#g[@]}" "$n"
printf '  ★ ls 只列了 %s 行名字，切成段却成了 %s 段 —— 「a b.txt」被空格劈成「a」和「b.txt」，\n' \
    "$nls" "${#segs[@]}"
printf '    当场拿这 %s 段去 [ -e rn/这一段 ] 的话有 %s 段答「不存在」：它们根本不是文件，\n' "${#segs[@]}" "$fake"
printf '    是名字的碎片。想用它 mv，报的就是「没有那个文件或目录」。\n'
printf '  ★ glob 比真数少 %s 个，少的正是 .hidden（第 2 格单独算这笔账）；find 口径=%s 与真数相等吗=%s\n' \
    "$(( $(realcount rn) - ${#g[@]} ))" "$n" \
    "$([ "$n" = "$(realcount rn)" ] && echo 是 || echo '★ 否')"

echo
echo "=== 2. 星号看不见以点开头的文件：glob 段数与 ls -A 的差，正好等于隐藏文件数 ==="
wipe
touch rn/vis1.txt rn/vis2.txt rn/.dot1 rn/.dot2 rn/.dot3
gl=(); for f in rn/*; do gl+=("$f"); done
nA=$(ls -A rn | wc -l)
nH=$(find rn -maxdepth 1 -mindepth 1 -printf '%f\n' | sed -n '/^\./p' | wc -l)
printf '  glob 展开出来 %s 个 ｜ ls -A 数出 %s 个 ｜ 以点开头的 %s 个\n' "${#gl[@]}" "$nA" "$nH"
find rn -maxdepth 1 -mindepth 1 -printf '%f\n' | LC_ALL=C sort | sed 's/^/      现读目录：/'
printf '  → 断言：差值 ls -A 减 glob = %s ｜ 隐藏文件数 = %s ｜ 两者相等吗 = %s\n' \
    "$(( nA - ${#gl[@]} ))" "$nH" \
    "$( [ "$(( nA - ${#gl[@]} ))" -eq "$nH" ] && echo 相符 || echo '★ 不符（口径坏了，别信这一格）' )"
printf '  ★ 注意反向的坑：glob 数（%s）比 ls（不带 -A）还诚实，它至少把每个名字当一整块给出；\n' "${#gl[@]}"
printf '    要连隐藏文件一起收，只有 find 或者显式加 rn/.* —— 但 rn/.* 会把 . 和 .. 也拽进来，慎用。\n'

echo
echo "=== 3. 开头的 -：mv 先把它当旗号，两种救法都是「让 - 别落在参数第一个字符上」 ==="
wipe
touch 'rn/-rf'
printf '  造好了，目录里现读到的名字：'
find rn -maxdepth 1 -type f -printf '%f\n' | tr '\n' ' ' | sed 's/$/\n/'
err=$( (cd rn && mv "-rf" gone) 2>&1 ); rc1=$?
k1=$(realcount rn)
printf '  写法一 mv "-rf" gone            rc=%s ｜ 旧名还在吗=%s ｜ 新名在场吗=%s ｜ 此刻文件数=%s\n' \
    "$rc1" "$([ -e 'rn/-rf' ] && echo 是 || echo 否)" "$([ -e rn/gone ] && echo 是 || echo 否)" "$k1"
printf '        报错原文（缩进的这行是这一格故意印的证据）：%s\n' "$(printf '%s' "$err" | fold | tr '\n' '/')"
( cd rn && mv -- "-rf" back1 ); rc2=$?
k2=$(realcount rn)
printf '  写法二 mv -- "-rf" back1        rc=%s ｜ 旧名残留=%s ｜ 新名在场=%s ｜ 此刻文件数=%s\n' \
    "$rc2" "$(find rn -maxdepth 1 -type f -name '-rf' | wc -l)" "$([ -e rn/back1 ] && echo 是 || echo 否)" "$k2"
touch 'rn/-rf'
k2b=$(realcount rn)
( cd rn && mv ./-rf back2 ); rc3=$?
k3=$(realcount rn)
printf '  写法三 mv ./-rf back2           rc=%s ｜ 旧名残留=%s ｜ 新名在场=%s ｜ 动手前=%s 此刻=%s\n' \
    "$rc3" "$(find rn -maxdepth 1 -type f -name '-rf' | wc -l)" "$([ -e rn/back2 ] && echo 是 || echo 否)" "$k2b" "$k3"
kmin=$(printf '%s\n' "$k1" "$k2" "$k3" | LC_ALL=C sort -n | head -1)
printf '  → 断言：起点 1 个文件 ｜ 写法一后=%s ｜ 写法二后=%s ｜ 写法三后=%s（中间补造过一次，所以多 1）｜ 三次取样里最小=%s ｜ 一个都没丢吗=%s\n' \
    "$k1" "$k2" "$k3" "$kmin" "$([ "$kmin" -ge 1 ] && echo 是 || echo '★ 否')"
printf '  ★ 写法一 rc=1，说明 mv 压根没看见那个文件：它先扫参数、认定 -r 是旗号就停了；\n'
printf '    -- 只写一次就够（它之后的都不当旗号），./ 则是从源头让第一个字符不是 -。\n'
printf '    第 5 格会看到更省事的解法：名字前面带上 rn/ 这个目录前缀，- 就不再打头了。\n'

echo
echo "=== 4. 名字里带换行：一个文件被 find 打成两行，段数当场虚高，只有 NUL 口径对得上 ==="
wipe
NL=$'nl\nfile'
touch rn/ok.txt "rn/$NL"
printf '  造了 2 个文件：ok.txt 与 nl[换行]file —— 后者名字中间真有一个换行符\n'
m=0; for f in $(find rn -maxdepth 1 -type f -printf '%f\n'); do m=$((m+1)); done
lines=$(find rn -maxdepth 1 -type f -printf '%f\n' | wc -l)
nul=$(find rn -maxdepth 1 -type f -print0 | tr -dc '\0' | wc -c)
printf '  口径甲 for f in $(find ... -printf %%f) 按空白切：段数=%s\n' "$m"
printf '  口径乙 直接把那段输出交给 wc -l        ：行数=%s\n' "$lines"
printf '  口径丙 find -print0 数 NUL             ：记录数=%s\n' "$nul"
r=0; while IFS= read -r -d '' f; do r=$((r+1)); printf '        读到一条 [%s]\n' "$(vis "${f#rn/}")"; done \
    < <(find rn -maxdepth 1 -type f -print0 | LC_ALL=C sort -z)
printf '  → 断言：真文件数=%s ｜ 空白口径=%s ｜ 行口径=%s ｜ NUL 口径=%s ｜ 两种 NUL 读法自己一致吗=%s\n' \
    "$nul" "$m" "$lines" "$r" \
    "$([ "$r" = "$nul" ] && echo 是 || echo '★ 否（数 NUL 与读 NUL 都对不上，这一格的尺坏了）')"
printf '  ★ 只有 NUL 口径（%s）与真数相等：名字里那个换行让 -printf 多吐了一行，\n' "$nul"
printf '    于是 wc -l 虚高到 %s。想按名字数文件，就别把换行当分隔符 —— 这是唯一稳的一条路。\n' "$lines"

echo
echo "=== 5. 空格 + 换行 + 开头的 - 三个怪名字一起走：read -d 空串 那套循环真的能把三个都改对 ==="
wipe
touch 'rn/sp ace.txt' 'rn/-dash.txt' "rn/$NL"
printf '  改名前目录里几条记录=%s（现读，NUL 口径）\n' "$(realcount rn)"
# ★ sort 也必须用 -z：默认它按换行分记录，NUL 流里那个带换行的名字会被当成两条，整条记录当场错行。
find rn -maxdepth 1 -type f -print0 | LC_ALL=C sort -z | while IFS= read -r -d '' f; do printf '      旧名 [%s]\n' "$(vis "${f#rn/}")"; done
before5=$(realcount rn)
done2=0; fail=0
# 抠名字用 ${f#rn/} 而不是 basename：basename 的输出结尾带换行，命令替换会把它吃掉，
# 名字本身以换行结尾时就把名字改短了 —— 前缀剥离是纯字符串操作，一个字符都不丢。
while IFS= read -r -d '' f; do
    mv -- "$f" "rn/P_${f#rn/}" && done2=$((done2+1)) || fail=$((fail+1))
done < <(find rn -maxdepth 1 -type f -print0 | LC_ALL=C sort -z)
# 旧名残留也按 NUL 数：这里的名字带换行，用 wc -l 数会凭空多出一截。
old=$(find rn -maxdepth 1 -type f -not -name 'P_*' -print0 | tr -dc '\0' | wc -c)
printf '  改名后目录里几条记录=%s，其中带 P_ 前缀的=%s\n' \
    "$(realcount rn)" "$(find rn -maxdepth 1 -type f -name 'P_*' -print0 | tr -dc '\0' | wc -c)"
find rn -maxdepth 1 -type f -printf '%f\0' | LC_ALL=C sort -z | while IFS= read -r -d '' f; do printf '      新名 [%s]\n' "$(vis "$f")"; done
printf '  → 断言：改名前=%s 改名后=%s 旧名残留=%s 成功=%s 失败=%s ｜ 三个怪名字全改对了吗=%s\n' \
    "$before5" "$(realcount rn)" "$old" "$done2" "$fail" \
    "$([ "$done2" = "$before5" ] && [ "$old" = 0 ] && [ "$fail" = 0 ] && echo 是 || echo '★ 否（有名字没改走或改了却没数进来）')"
printf '  ★ 三个怪名字（一个带空格、一个名字里有换行、一个以 - 打头）全数改对：成功=%s 失败=%s 旧名残留=%s。\n' \
    "$done2" "$fail" "$old"
printf '    mv 的源参数带着 rn/ 前缀，所以开头的 - 在这儿根本咬不着 —— 但 -- 照样留着，白拿的保险。\n'

echo
echo "=== 6. 顺序编号改名：先把名单落成文件排好序再动 mv；边遍历边改会把名单和现状错位 ==="
wipe
for n in a b c; do printf '%s.jpg' "$n" > "rn/$n.jpg"; done
before=$(realcount rn)
: > rnplan
find rn -maxdepth 1 -type f -name '*.jpg' -printf '%f\n' | LC_ALL=C sort > rnplan
i=0
while IFS= read -r f; do
    ext=${f##*.}                       # 扩展名自己抠出来，别让下一格换扩展名时把它写死
    dst=$(printf 'rn/IMG_%04d.%s' "$i" "$ext")
    mv -- "rn/$f" "$dst"
    i=$((i+1))
done < rnplan
after=$(jpgcount)
oldjpg=$(find rn -maxdepth 1 -type f -name '*.jpg' -not -name 'IMG_*' -print0 | tr -dc '\0' | wc -c)
printf '  改名后的名字（现读目录，按 C 排序）：\n'
find rn -maxdepth 1 -type f -printf '%f\n' | LC_ALL=C sort | sed 's/^/      /'
printf '  → 断言：改名前=%s 改名后=%s 旧名残留=%s 名单行数=%s ｜ 一个没丢也没多吗=%s\n' "$before" "$after" "$oldjpg" "$(wc -l < rnplan)" \
    "$([ "$after" = "$before" ] && [ "$oldjpg" = 0 ] && echo 是 || echo '★ 否（编号改名改出了增减）')"
printf '  ★ 现在演那个真坑：目标名撞上「还没轮到的源名」时，边遍历边改会连环覆盖。\n'
printf '    构造：目录里 p0.jpg p1.jpg p2.jpg，方案是把每个往前挪一格（p0→p1、p1→p2、p2→p3）。\n'
printf '    每个文件的内容就是它自己的名字，这样「谁落到了哪」当场可对账。\n'
mkp() { wipe; for n in p0 p1 p2; do printf '%s.jpg' "$n" > "rn/$n.jpg"; done; }
# plan_pairs：把「每个名字往前挪一格」这张计划落成文件（源<TAB>目标）。
#              A、B 两版共用同一张表，对账才公平：期望映射完全一样，只差在「什么时候读它」。
plan_pairs() { : > rnplan; for n in p0 p1 p2; do printf '%s.jpg\t%s.jpg\n' "$n" "p$(( ${n#p} + 1 )).jpg" >> rnplan; done; }
# tally：逐对核对「目标名里装的到底是不是它该装的那个源」，结果写进 HITV/MISSV 两个全局。
#        内容就是源名字本身，所以这一步能查出 mv 全部 rc=0、结果却错成一片的那种事故。
#        ★ 刻意不用 here-string 去接 tally 的输出：那要多一层子 shell，而且本机的静态检查
#          会把「两个小于号加一个引号」误认成 heredoc 开头，从此不再扫这个文件的后半截。
tally() {
    HITV=0; MISSV=0
    local s d
    while IFS=$'\t' read -r s d; do
        if [ -f "rn/$d" ] && [ "$(cat "rn/$d")" = "$s" ]; then HITV=$((HITV+1)); else MISSV=$((MISSV+1)); fi
    done < rnplan
}
mkp; plan_pairs
mvA=0
for f in rn/*.jpg; do                      # 名单来自 glob，改动却发生在同一个目录里 —— 典型写法
    stem=${f##*/}; stem=${stem%.jpg}
    mv -- "$f" "rn/p$(( ${stem#p} + 1 )).jpg" 2>/dev/null && mvA=$((mvA+1))
done
cntA=$(jpgcount)
tally; hitA=$HITV; missA=$MISSV
mkp; plan_pairs
mvB=0; k=0
while IFS=$'\t' read -r s d; do            # 第一段：全部先挪到不相干的中间名
    mv -- "rn/$s" "rn/.hop$k.jpg"
    k=$((k+1))
done < rnplan
i=0
while IFS=$'\t' read -r s d; do            # 第二段：中间名落到最终名。★ 中间名也带 .jpg，
    mv -- "rn/.hop$i.jpg" "rn/$d" && mvB=$((mvB+1))   # 但名单早就落成文件了，没人再回头 glob 一次
    i=$((i+1))
done < rnplan
cntB=$(jpgcount)
tally; hitB=$HITV; missB=$MISSV
printf '  写法A 直接在 for f in rn/*.jpg 的循环体里改：mv 成功=%s 次 ｜ 文件数 3→%s ｜ 落对位=%s/3 ｜ 落错或缺失=%s\n' \
    "$mvA" "$cntA" "$hitA" "$missA"
printf '  写法B 先把名单落成文件再改（两段，中间名过渡）：mv 成功=%s 次 ｜ 文件数 3→%s ｜ 落对位=%s/3 ｜ 落错或缺失=%s\n' \
    "$mvB" "$cntB" "$hitB" "$missB"
printf '  → 断言：落对位 A=%s vs B=%s ｜ 文件数 A=%s vs B=%s ｜ mv 成功次数 A=%s vs B=%s ｜ 两种写法落到一起了吗=%s\n' \
    "$hitA" "$hitB" "$cntA" "$cntB" "$mvA" "$mvB" \
    "$([ "$hitA" = "$hitB" ] && echo '★ 相等（这一格没演出错位，下面的结论别信）' || echo 不等，A 确实落错了位)"
printf '  ★ 最阴的地方在这儿：A 的 %s 次 mv 全部 rc=0，一条报错都没打，文件却从 3 掉到 %s、\n' "$mvA" "$cntA"
printf '    落对位只剩 %s 个（B 是 %s 个）—— 「mv 没报错」根本不等于「改对了」，\n' "$hitA" "$hitB"
printf '    只能靠内容对账兜住。错位的原因很朴素：目标名撞上了「名单里还没轮到的那个源名」。\n'

echo
echo "=== 7. 幂等：跑第二遍会不会把刚改好的再改一遍 —— 守卫 case 决定这件事 ==="
wipe
for n in x y z; do printf '%s.jpg' "$n" > "rn/$n.jpg"; done
# run_pass：$1=1 带守卫 / 0 不带守卫。MOVED=这一遍真的动了几次，NAMES=这一遍结束时的名单。
# 编号故意「接着目录里已有的 IMG_ 个数往后编」—— 真实脚本十有八九这么写，
# 而这正是第二遍把刚改好的三个又往后推三位的原因。
run_pass() {
    local guard=$1 n=0 f b dst
    MOVED=0
    n=$(find rn -maxdepth 1 -type f -name 'IMG_*.jpg' -print0 | tr -dc '\0' | wc -c)
    for f in rn/*.jpg; do
        b=${f##*/}                     # ★ 必须拿 basename 去匹配：$f 带着 rn/ 前缀，
        if [ "$guard" = 1 ]; then      #   写成 IMG_* 匹配 $f 的话一个都不中，守卫等于没加
            case "$b" in IMG_*) continue ;; esac
        fi
        dst=$(printf 'rn/IMG_%04d.jpg' "$n"); n=$((n+1))
        mv -- "$f" "$dst" && MOVED=$((MOVED+1))
    done
    NAMES=$(find rn -maxdepth 1 -type f -printf '%f\n' | LC_ALL=C sort | tr '\n' '/')
}
run_pass 1; g1=$MOVED; first1=$NAMES
run_pass 1; g2=$MOVED; second1=$NAMES
printf '  带守卫那一版：第一遍改动=%s 次 ｜ 第二遍改动=%s 次\n' "$g1" "$g2"
printf '    第二遍后的名单=[%s]\n' "$second1"
wipe
for n in x y z; do printf '%s.jpg' "$n" > "rn/$n.jpg"; done
run_pass 0; u1=$MOVED; first0=$NAMES
run_pass 0; u2=$MOVED; second0=$NAMES
printf '  不带守卫那一版：第一遍改动=%s 次 ｜ 第二遍改动=%s 次\n' "$u1" "$u2"
printf '    第一遍后名单=[%s]\n    第二遍后名单=[%s]\n' "$first0" "$second0"
printf '  → 断言：带守卫 第二遍改动=%s（应为 0）｜ 名单与第一遍相同=%s ｜ 不带守卫 第二遍改动=%s（应为 3）｜ 名单相同=%s\n' \
    "$g2" "$([ "$second1" = "$first1" ] && echo 是 || echo 否)" \
    "$u2" "$([ "$second0" = "$first0" ] && echo 是 || echo '★ 否（刚改好的又被推后三位）')"
printf '  ★ 两版并排看：差别只在 case 那一行。守卫的写法成本一行，省的是「第二遍毁掉第一遍」；\n'
printf '    还有个隐形炸弹：模式匹配在 $f 还是 basename 上 —— 带前缀时 IMG_* 永远不中，守卫默默失效。\n'

echo
echo "=== 8. 目标名已存在 = 一声不响地覆盖，文件数当场少一个 ==="
wipe
printf '%s' '我是 1.jpg' > rn/1.jpg
printf '%s' '我是 IMG_0000.jpg' > rn/IMG_0000.jpg
n0=$(realcount rn)
: > rnplan
find rn -maxdepth 1 -type f -name '*.jpg' -printf '%f\n' | LC_ALL=C sort > rnplan
i=0; mv8=0
while IFS= read -r f; do
    dst=$(printf 'rn/IMG_%04d.jpg' "$i"); i=$((i+1))
    mv -- "rn/$f" "$dst" && mv8=$((mv8+1))
done < rnplan
n1=$(realcount rn)
printf '  不设防：文件数 %s → %s ｜ mv 成功 %s 次 ｜ 全程 rc 都是 0，一条警告都没打\n' "$n0" "$n1" "$mv8"
wipe
printf '%s' '我是 1.jpg' > rn/1.jpg
printf '%s' '我是 IMG_0000.jpg' > rn/IMG_0000.jpg
i=0; mvn=0; rcn=0
for f in $(find rn -maxdepth 1 -type f -name '*.jpg' -printf '%f\n' | LC_ALL=C sort); do
    dst=$(printf 'rn/IMG_%04d.jpg' "$i"); i=$((i+1))
    mv -n -- "rn/$f" "$dst" 2>/dev/null; rcn=$?; [ "$rcn" = 0 ] && mvn=$((mvn+1))
done
nA8=$(realcount rn)
stuck=$(find rn -maxdepth 1 -type f -not -name 'IMG_*' -print0 | tr -dc '\0' | wc -c)
printf '  救法一 mv -n：文件数 %s → %s ｜ mv 调用 %s 次且每次 rc 都是 %s ｜ 被留在原地没动走的名字 %s 个：%s\n' \
    "$n0" "$nA8" "$mvn" "$rcn" "$stuck" "$(find rn -maxdepth 1 -type f -not -name 'IMG_*' -printf '%f ')"
wipe
printf '%s' '我是 1.jpg' > rn/1.jpg
printf '%s' '我是 IMG_0000.jpg' > rn/IMG_0000.jpg
i=0; skip=0; did=0
for f in $(find rn -maxdepth 1 -type f -name '*.jpg' -printf '%f\n' | LC_ALL=C sort); do
    dst=$(printf 'rn/IMG_%04d.jpg' "$i"); i=$((i+1))
    if [ -e "$dst" ]; then skip=$((skip+1)); continue; fi
    mv -- "rn/$f" "$dst" && did=$((did+1))
done
nB8=$(realcount rn)
printf '  救法二 先 [ -e 目标 ] 再动：文件数 %s → %s ｜ 真改=%s ｜ 撞名跳过=%s\n' "$n0" "$nB8" "$did" "$skip"
printf '  → 断言：不设防 %s→%s（少 %s 个）｜ mv -n %s→%s ｜ 先判断 %s→%s ｜ 后两种少的个数=%s/%s ｜ 和预期一致吗=%s\n' \
    "$n0" "$n1" "$(( n0 - n1 ))" "$n0" "$nA8" "$n0" "$nB8" "$(( n0 - nA8 ))" "$(( n0 - nB8 ))" \
    "$([ "$n1" -lt "$n0" ] && [ "$nA8" = "$n0" ] && [ "$nB8" = "$n0" ] && echo 是 || echo '★ 否（这一格没照剧本走，结论别照抄）')"
printf '  ★ 不设防那一路真的少了一个文件，内容没了，这一支不试图找回、也不该指望找回；\n'
printf '    能做的只有两件事：要么把撞名在动手之前就查出来（第 9 格的 dry-run），要么 mv -n。\n'

echo
echo "=== 9. dry-run 先行：同一份计划先只印不动，指纹一模一样才敢真跑 ==="
wipe
printf 'AAAA' > rn/t1.txt; printf 'BBBBB' > rn/t2.txt; printf 'CC' > rn/t3.txt
d_before=$(digest rn)
: > rnplan
find rn -maxdepth 1 -type f -name '*.txt' -printf '%f\n' | LC_ALL=C sort > rnplan
plans=(); while IFS= read -r f; do plans+=("$f"); done < rnplan
plan=0
for f in "${plans[@]}"; do
    stem=${f%.txt}
    printf '      会执行 mv %s → %s\n' "rn/$f" "rn/DO_${stem}.txt"
    plan=$((plan+1))
done
printf '  ↑ 这 %s 行全是 printf 打的，一条 mv 都没执行 —— 空跑的意思就是「只说话不动手」\n' "$plan"
d_after=$(digest rn)
printf '  空跑前指纹=[%s] ｜ 空跑后指纹=[%s] ｜ 一模一样吗=%s\n' \
    "$d_before" "$d_after" "$([ "$d_before" = "$d_after" ] && echo 是 || echo '★ 否（空跑偷偷动手了）')"
done9=0
while IFS= read -r f; do
    stem=${f%.txt}
    mv -- "rn/$f" "rn/DO_${stem}.txt" && done9=$((done9+1))
done < rnplan
d_real=$(digest rn)
printf '  真跑后指纹=[%s] ｜ 和空跑前比=%s\n' "$d_real" "$([ "$d_real" = "$d_before" ] && echo '★ 没变（真跑没动任何东西）' || echo 变了)"
printf '  真跑后的名字：'
find rn -maxdepth 1 -type f -printf '%f\n' | LC_ALL=C sort | tr '\n' ' ' | sed 's/$/\n/'
printf '  → 断言：计划行数=%s ｜ 应改文件数=%s ｜ 实改行数=%s ｜ 计划与实改相等=%s ｜ 空跑期间目录未变=%s\n' \
    "$plan" "${#plans[@]}" "$done9" \
    "$([ "$plan" = "$done9" ] && echo 是 || echo '★ 否')" \
    "$([ "$d_before" = "$d_after" ] && echo 是 || echo '★ 否')"
printf '  ★ 指纹用的是「名字+字节数」，不用 mtime：改名一定动 mtime，拿它当证据等于自己骗自己；\n'
printf '    而 dry-run 那一趟里除了 printf 一个字都没执行，所以目录清单前后完全相等 —— 这就是敢按第二遍的前提。\n'

echo
echo "=== 10. 收尾自证：rn 清零、当前目录没多出东西、脚本自己所在的目录跑前跑后一模一样 ==="
residue=$(find rn -mindepth 1 2>/dev/null | wc -l)
printf '  trap 之前先数一遍 rn 里的条目=%s（这是最后一格留下来的沙箱内容，随后整目录删掉）\n' "$residue"
rm -rf rn rnplan
left=$(find . -maxdepth 1 -mindepth 1 \( -name rn -o -name rnplan \) | wc -l)
printf '  当前目录现读清单（只有名字，过 fold 折身份）：'
ls -A . | LC_ALL=C sort | tr '\n' ' ' | fold | sed 's/$/\n/'
now_list=$(ls -A | sort | tr '\n' '|')
now_cnt=$(find . -maxdepth 1 -mindepth 1 | wc -l)
now_sum=$(find . -maxdepth 1 -mindepth 1 -type f -printf '%s\n' | awk '{s=s+$1} END{print s+0}')
# 起点从环境读：demo.sh 在把脚本复制进沙箱之后，按「这间屋子」（work/）现读一份 RN_BEFORE_*；
# 章目录那份 BEFORE_* 只是兜底 —— 跨目录比字节总和必然对不上，所以 RN_ 在前。
# 单独跑本脚本时两个名字都不在场，那笔对账就判 ★ FAIL（空串比空串是恒真，比没有判据更糟）。
# ★ 口径必须跟 demo.sh 里那两行逐字一样（名单：ls -A | sort | tr '\n' '|'；字节：find -printf '%s\n' 求和）。
BL="${RN_BEFORE_LIST:-${BEFORE_LIST:-}}"
BS="${RN_BEFORE_SUM:-${BEFORE_SUM:-}}"
printf '  起点名单（来自环境）：名单=[%s] 字节总和=[%s]\n' "${BL:-（未设）}" "${BS:-（未设）}"
printf '  本脚本此刻量到的终值：名单=[%s] 条目数=[%s] 字节总和=[%s]\n' "$now_list" "$now_cnt" "$now_sum"
printf '  → 清理这条不看起点，独立判定：rn 与 rnplan 现在在场=%s 条（应为 0）｜ trap 前 rn 条目=%s ｜ 判定=%s\n' \
    "$left" "$residue" "$([ "$left" = 0 ] && echo 已清干净 || echo '★ FAIL（有东西没清干净）')"
if [ -z "$BL" ] || [ -z "$BS" ]; then
    printf '  → 断言：起点缺席，「跑前跑后相同」这笔对账没做成 —— 判定为 ★ FAIL。\n'
    printf '  ★ 空串等于空串是恒真，比没有判据更糟，所以这里绝不把它退化成「相同」；\n'
    printf '    单独 bash rename.sh 必然走到这条分支 —— 只有 demo.sh 会 export 这两个变量。\n'
else
    # 先让「跑前跑后相同」这把尺响一次：往起点名单尾巴上人为接一个此刻不在场的名字，
    # 它必须答「不等」；响了之后才敢报下面那句真比较。
    ring=$([ "$now_list" = "${BL}zzz-absent.txt|" ] && echo 相等 || echo 不等)
    printf '  先让判据响一次：起点名单被故意改坏（末尾多一个不在场的名字）再比 —— 判定=%s（应为 不等）\n' "$ring"
    if [ "$now_list" = "$BL" ] && [ "$now_sum" = "$BS" ]; then
        printf '  → 断言：名单逐字相同=%s ｜ 字节总和 %s=%s ｜ 判定=全绿（脚本所在的目录跑前跑后一模一样）\n' \
            "$([ "$now_list" = "$BL" ] && echo 是 || echo 否)" "$now_sum" "$BS"
    else
        printf '  → 断言：名单 此刻=[%s] 起点=[%s] ｜ 字节总和 此刻=%s 起点=%s ｜ 判定=★ FAIL（目录被改过）\n' \
            "$now_list" "$BL" "$now_sum" "$BS"
    fi
fi
printf '  ★ 从头到尾只有 rn/ 与 rnplan 被写过，屏幕上每一个数字都来自当场的 find/ls/wc/cat，\n'
printf '    没有一行是手抄的示例输出 —— 贴回 README 之前你唯一该做的事，是再跑一遍拿它去 diff。\n'

# 自查这一行是给用户对的账，不是给读者的客套：格数按屏幕上的格头现数，
# rn/ 的文件数按此刻的真实状态现读（第 10 格已经把它 rm 掉了，所以这里是 0 ——
# 同一把尺在上面印过 residue=3，这个 0 是「响过的尺回到 0」，不是恒真）。
printf '\n[rename.sh 自查] 本脚本格数=%s ｜ rn/ 里现有文件数=%s（跑完由 trap 整目录删除）\n' \
    "$(grep -c '^echo "=== ' "$0")" "$(find rn -type f 2>/dev/null | wc -l)"
exit 0
