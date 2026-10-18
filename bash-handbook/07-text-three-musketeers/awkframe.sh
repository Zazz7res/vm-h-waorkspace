#!/usr/bin/env bash
# ============================================================================
# awkframe.sh —— awk 手里拿的是「一行里的字段」，而且字段可以赋值
#
#   一句话主旨：awk 先按 FS 把一行切成 $1..$NF；给任意字段赋值的那一刻，
#             整行按 OFS **重建** —— 原来的对齐空白一个都留不住。
#   demo.sh 里这样调：bash awkframe.sh
#   安全：只在脚本当前目录自建 af/ 子目录里造样本，结尾 trap 删掉；
#         不碰 ~、不碰绝对 /tmp、不读写仓库既有文件、不联网、不开端口、不 kill 任何进程。
#   故意不写 set -e：第 6 格「挑到 1 行 / 挑到 0 行」两支都要拿到屏幕上再说。
#
#   ★ 本机不拿什么当证据：全程没有时间类输出；每个字段数/字节数都是自建样本
#   数得出的确定数，连跑两遍逐字一样。空白可见化用 cat -A（本机 cat 是 uutils
#   coreutils，实测支持 -A：连续空格原样、制表符显示 ^I、行尾显示 $）。
# ============================================================================

rm -rf af
mkdir -p af
trap 'rm -rf af' EXIT

printf 'alpha  beta\tgamma\n'  > af/plain       # 两个空格 + 一个制表符混排的一行
printf 'ID1  name1   price9\n' > af/align       # 故意拿 2 空格/3 空格「对齐」
printf 'ID2\tname2\tprice8\n'  > af/tabalign    # 同样内容，制表符对齐版
printf 'a,,b\n'                > af/comma2
printf 'a:b:\n'                > af/colon2

echo "=== 1. 同一行切三刀：字段数是 FS 说了算 ==="
awk '{ printf "  FS=默认（连续空白算一个分隔）: NF=%s, $1=[%s], $NF=[%s]\n", NF, $1, $NF }' af/plain
awk -F, '{ printf "  FS=逗号（这行没一个逗号）:   NF=%s —— 整行原封落进 $1\n", NF }' af/plain
awk 'BEGIN{FS=""}{ printf "  FS=空串（本机 mawk 的下场）: NF=%s, $2=[%s], $8=[%s] —— 逐字符切，空白也各算一个字段\n", NF, $2, $8 }' af/plain
echo "  → 直觉以为「这行不管谁来切都是那三列」，屏幕上同一行切出 3 / 1 / 17 三种字段数。"
echo "  ★ awk 里没有「列」，只有「按 FS 切完全行的第 n 段」；FS 没交代清楚，后面每个 \$n 都在错位。"

echo
echo "=== 2. ★ 给字段赋值 = 整行按 OFS 重建：对齐空白当场没了（cat -A 可见） ==="
printf "  原行（cat -A：空格原样、制表符=^I、行尾=\$）：\n"
cat -A af/align | sed 's/^/    /'
awk '{ $3="done" } 1' af/align | cat -A | sed 's/^/    /'
printf "  ↑ 只动了第 3 个字段，前两列之间的 2 空格、3 空格全塌成 awk 默认 OFS 的一个空格\n"
awk -F'\t' '{ $2="X" } 1' af/tabalign | cat -A | sed 's/^/    /'
printf "  ↑ 制表符对齐版同理：改完中间字段，制表符也被换成了空格\n"
echo "  → 直觉以为「我只补第 3 个字段，前两个字段凭什么跟着变」，屏幕上空白确实全变。"
echo "  ★ awk 改一行不是打补丁，是**整行重建**：给任意 \$n 赋值后，\$0 会按 OFS 重新拼一遍。"
echo "    「改一列结果 diff 炸一整行」的 awk 层原因就在这。"

echo
echo "=== 3. 三种「拼接」：有没有逗号，字节数当场不一样 ==="
printf "  print a,b（逗号 → 中间垫 OFS，默认一空格）: 输出[%s]，%s 字节\n" \
  "$(awk 'BEGIN{a="xy"; b="z"; print a, b}')" \
  "$(awk 'BEGIN{a="xy"; b="z"; print a, b}' | wc -c)"
printf "  print a b（无逗号 → 直接贴一起）:           输出[%s]，%s 字节\n" \
  "$(awk 'BEGIN{a="xy"; b="z"; print a b}')" \
  "$(awk 'BEGIN{a="xy"; b="z"; print a b}' | wc -c)"
printf "  printf 两个 %%s（每个字节自己负责）:        输出[%s]，%s 字节 —— ★ 连换行都没有\n" \
  "$(awk 'BEGIN{a="xy"; b="z"; printf "%s%s", a, b}')" \
  "$(awk 'BEGIN{a="xy"; b="z"; printf "%s%s", a, b}' | wc -c)"
echo "  → 直觉以为「三种写法只是风格差异」，屏幕上 5/4/3 字节各有各的后果："
echo "    逗号=走 OFS；无逗号=贴死；printf 自带 0 换行 —— 接在管道里就是不分帧（回看 pipe3.sh 第 2 格）。"

echo
echo "=== 4. OFS 不是万能：同一个 -v OFS=竖线，两种写法对撞 ==="
printf "  写法1 %s → 输出[%s]\n" '{print $0}' "$(awk -v OFS='|' '{print $0}' af/align)"
printf "  写法2 %s → 输出[%s]\n" '{print $1, $2}' "$(awk -v OFS='|' '{print $1, $2}' af/align)"
printf "  写法3 %s → 输出[%s]\n" '{ $3="X" }1' "$(awk -v OFS='|' '{ $3="X" }1' af/align)"
echo "  → 直觉以为「设了 OFS 输出去哪都该带竖线」，屏幕上写法 1 原样保留空格对齐、一个竖线没有。"
echo "  ★ OFS 只在两处生效：① print 的逗号清单（写法2），② 字段被赋值后的 \$0 重建（写法3）。"
echo "    没动过字段的整行输出（print \$0 或光秃秃的 1）原样透传，OFS 管不着。"

echo
echo "=== 5. 空字段：'a,,b' 和 'a:b:' 各有几个字段（本机 mawk 实测数） ==="
awk -F, '{ printf "  [%s] → NF=%s，三个字段是 [%s] [%s] [%s] —— 两个逗号中间夹着空\n", $0, NF, $1, $2, $3 }' af/comma2
awk -F: '{ printf "  [%s] → NF=%s，三个字段是 [%s] [%s] [%s] —— 末尾分隔符也多咬一个空字段\n", $0, NF, $1, $2, $3 }' af/colon2
printf '  x  y  \n' > af/blank
awk '{ printf "  [前后带空格的两词] → NF=%s —— 首尾空白不算字段\n", NF }' af/blank
echo "  → 直觉以为「看得见几个东西就几个字段」，屏幕上显式 FS 时字段数 = 分隔符个数 + 1，空档也占坑；"
echo "    只有默认 FS（空白）是「连续算一个、首尾不算」的宽松切法。"
echo "  ★ 同一份内容，逗号切出来 3 个字段、默认空白切可能只有 2 个 —— 两个 NF 不是同一种货币，别混着比。"

echo
echo "=== 6. NF 当现场判据：先让守卫响一次，它才可信 ==="
cat > af/tbl <<'EOF'
user1  20   ok   extra
user2  7
user3  99   ok   ok
EOF
awk 'NF<3' af/tbl > af/short1; rc1=$?
printf "  探针轮：awk %s 挑「字段不够的行」→ 挑中 %s 行，退出码 %s（守卫响了，证明它有牙）\n" \
  "'NF<3'" "$(wc -l < af/short1)" "$rc1"
sed 's/^user2  7$/user2  7   ok/' af/tbl > af/tbl2
awk 'NF<3' af/tbl2 > af/short2; rc2=$?
printf "  把短行修成 3 字段再挑 → %s 行，退出码 %s —— 这次的 0 是「真没鱼」\n" "$(wc -l < af/short2)" "$rc2"
awk 'NF>99' af/tbl > af/fake; rc3=$?
printf "  ★ 写成 %s 的恒假守卫：照样 %s 行、退出码照样 %s —— 屏幕上和「真没短行」一模一样\n" \
  "'NF>99'" "$(wc -l < af/fake)" "$rc3"
echo "  → 直觉以为「挑不到 awk 总会报错」，上面两种 0 行退出码全是 0（回看 pipe3 第 4 格：awk 退出码不表结果）。"
echo "  ★ 所以守卫上岗前必须先喂一根「注定会被挑中」的探针行；只见过 0 的计数，你不知道它是干净还是哑了。"

echo
echo "=== 7. 收口：awk 的单位表 + 自建目录清场自证 ==="
printf "  单位   = 一行里的字段：FS 决定怎么切，NF=这行的字段数，\$n 可以赋值\n"
printf "  副作用 = 给任意 \$n 赋值 → \$0 按 OFS 整行重建（默认一个空格），原对齐阵亡\n"
printf "  拼接   = print 逗号清单走 OFS；无逗号贴死；printf 逐字节自理（换行也要自己给）\n"
printf "  退出码 = 除非 awk 自己出错，否则恒 0：挑没挑中、改没改到，与 rc 无关\n"
printf "  自证一（现场清干净）：删之前 af 在吗 → %s\n" "$([ -d af ] && echo 在 || echo 不在)"
rm -rf af
printf "  删之后在吗 → %s ｜ 当前目录里名叫 af 的条目 = %s（trap 只是备胎）\n" \
  "$([ -d af ] && echo 在 || echo 不在)" "$(find . -maxdepth 1 -name af | wc -l)"
printf "  ★ 一句话带走：sed 往「行」上打补丁，awk 拿「字段」重建整行；\n"
printf "    把空白对齐的行交给 awk 之前，先问两句：我的 OFS 接得住原空白吗？这行的 NF 和我想的一致吗？\n"

exit 0
