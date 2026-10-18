#!/usr/bin/env bash
# ============================================================================
# awkmawk.sh —— 本机 awk 是 mawk 1.3.4：缺件分两种，会叫的和不会叫的
#
#   一句话主旨：按 gawk 习惯写的代码到本机，要么**报错停下**（gensub 那族，rc=2），
#             要么**一声不吭地不生效**（FIELDWIDTHS、/\s/、大整数那族，rc=0）—— 后者才是真窟窿。
#   demo.sh 里这样调：bash awkmawk.sh
#   安全：只在脚本当前目录自建 amk/ 子目录里造样本，结尾 trap 删掉；
#         不碰 ~、不碰绝对 /tmp、不读写仓库既有文件、不联网、不开端口、不 kill 任何进程。
#   故意不写 set -e：第 2 格的 rc=2 就是要抓下来上屏的证据，一遇非 0 就停后面全看不到。
#
#   ★ 本机不拿什么当证据：strftime 的具体日期（每天必变）→ 只判「4-2-2 位数字」这个形状；
#   超大整数的长串 → 只判「加 1 之后还等不等于原值」；版本串实测两遍逐字一致、不含
#   用户名/路径，才敢原样打印首行。mawk 的报错原文一律 2>&1 抓下来一字不动。
# ============================================================================

rm -rf amk
mkdir -p amk
trap 'rm -rf amk' EXIT
printf 'abcdefgh\n'   > amk/fixed      # 8 个连续字符，一个空白都没有
printf 'a b\njusts\n' > amk/ws         # 第 1 行有空白没 s，第 2 行有 s 没空白

echo "=== 1. 先自证：本机这个 awk 到底是谁（屏幕上见，不口头声明） ==="
awk --version > amk/v1 2>&1; rcv=$?
awk -W version > amk/v2 2>&1
printf "  awk --version 首行: [%s]（rc=%s）\n" "$(head -1 amk/v1)" "$rcv"
printf "  awk -W version 首行与 --version 的首行一致 → %s\n" "$([ "$(head -1 amk/v1)" = "$(head -1 amk/v2)" ] && echo 是 || echo 否)"
printf "  首行含 mawk 这个词 → %s ｜ 现再跑一遍取首行，逐字相同 → %s（所以敢当证据印）\n" \
  "$(grep -qi mawk amk/v1 && echo 有 || echo 没有)" \
  "$([ "$(head -1 amk/v1)" = "$(awk --version | head -1)" ] && echo 相同 || echo 不同)"
echo "  → 「Linux 上 awk 自然是 gawk」的直觉，被屏幕上这一行打脸：本机是 mawk。"
echo "  ★ 先验明工具身份，再谈「这段写法怎么不灵」—— 下面所有缺件证据都站在这行上。"

echo
echo "=== 2. 会叫的缺件：gensub / asort / strtonum（报错原文逐字上屏） ==="
awk 'BEGIN{ print gensub(/a/,"Z","g","banana") }' > amk/e1 2>&1; rc1=$?
awk 'BEGIN{ split("c a b", arr, " "); print asort(arr) }' > amk/e2 2>&1; rc2=$?
awk 'BEGIN{ print strtonum("0x10") }' > amk/e3 2>&1; rc3=$?
printf "  ① gensub（gawk 的带正则全局替换版）rc=%s，它的报错原文：\n" "$rc1"
sed 's/^/    /' amk/e1
printf "  ② asort（gawk 的数组排序）rc=%s：\n" "$rc2"
sed 's/^/    /' amk/e2
printf "  ③ strtonum（gawk 的 0x 转数字）rc=%s：\n" "$rc3"
sed 's/^/    /' amk/e3
printf "  ★ 三个 rc 全等于 2 吗 → %s（不是 1！和 grep 的「真出错」同档，三态见 grepdialect）\n" \
  "$([ "$rc1" = 2 ] && [ "$rc2" = 2 ] && [ "$rc3" = 2 ] && echo 是 || echo 否)"
echo "    程序当场死在这一行，不会带着错结果往下跑 —— 会叫的缺件其实是友善的。"
printf "  对照：同样的活 mawk 自带的 gsub 能干 → [%s]\n" \
  "$(awk 'BEGIN{s="banana"; gsub(/a/,"Z",s); print s}')"

echo
echo "=== 3. ★★ 不会叫的缺件（一）：FIELDWIDTHS 固定宽度切分，静默空转 ==="
printf "  输入行 [%s]（8 个连续字符）。按 gawk 手册的写法是 BEGIN{FIELDWIDTHS=\"3 3\"}\n" "$(cat amk/fixed)"
printf "  你的期待（那是 GNU awk 的固定宽度切分；本机没装 gawk，这里只标期待）：[abc][def]... NF>2\n"
got=$(awk 'BEGIN{FIELDWIDTHS="3 3"} {printf "[%s][%s] NF=%s", $1, $2, NF}' amk/fixed 2>&1); rc4=$?
printf "  本机 mawk 屏幕上真正发生的: %s ｜ rc=%s —— ★ 一声不吭！\n" "$got" "$rc4"
printf "  本机真正能干的替代 substr 手切: %s\n" \
  "$(awk '{printf "[%s][%s][%s]", substr($0,1,3), substr($0,4,3), substr($0,7,3)}' amk/fixed)"
echo '  → 直觉最怕的是「工具报错」；可屏幕上给你的是 rc=0，整行塞进 $1、$2 空、NF=1。'
echo "  ★ 这一格是全章最贵的判据：「没报错 + rc 0」和「按你的意思生效了」是两回事；"
echo "    切分类写法必须现场验收 —— 把 NF 数出来、把字段打出来对一遍。"

echo
echo "=== 4. ★★ 不会叫的缺件（二）：awk 的 /\s/ 匹配的是字母 s，不是空白 ==="
printf "  样本两行：第 1 行[%s]（有空白、没 s），第 2 行[%s]（有 s、没空白）\n" \
  "$(sed -n '1p' amk/ws)" "$(sed -n '2p' amk/ws)"
awk '/\s/{print NR}' amk/ws > amk/h1; rc5=$?
printf "  mawk 用 %s：命中的行号 [%s]，rc=%s —— ★ 它命中了第 2 行，那个有字母 s 的行！\n" \
  '/\s/' "$(paste -sd, amk/h1)" "$rc5"
printf "  同一个 \\s 在本机 GNU grep -E：命中 %s 行（有空白那行）；GNU sed：命中 %s 行 —— 都读成空白\n" \
  "$(grep -cE '\s' amk/ws)" "$(sed -n '/\s/p' amk/ws | wc -l)"
printf "  本机 mawk 认的正确写法 %s：命中行号 [%s]\n" \
  '/[[:space:]]/' "$(awk '/[[:space:]]/{print NR}' amk/ws | paste -sd,)"
echo '  → 三组数字撞出一件事：同一个 \s，grep/sed（GNU）读成空白，mawk 读成字母 s，'
echo '    而且 mawk 那趟零报错、rc=0 —— 第 2 行这根探针就是「先故意弄响」：响了，但响在错的东西上。'
echo "  ★ 跨工具的写法（\s、\w、\b 这类）必须在每台引擎上单独验一遍，不能凭一家文档推三家。"

echo
echo "=== 5. 别把 mawk 当残废：这一半全都能用（输出全是确定值） ==="
printf "  length(arr) 数元素 → %s；delete arr 整个数组清零后 → %s\n" \
  "$(awk 'BEGIN{split("c a b", arr, " "); print length(arr)}')" \
  "$(awk 'BEGIN{split("c a b", arr, " "); delete arr; print length(arr)}')"
export AMK_STABLE='fixed-value-42'
printf '  ENVIRON["AMK_STABLE"] 读回 → [%s]（值由本脚本自己 export，恒定，所以敢印）\n' \
  "$(awk 'BEGIN{print ENVIRON["AMK_STABLE"]}')"
printf '  管道外命令 "cmd" | getline x → 拿到 [%s]\n' \
  "$(awk 'BEGIN{"echo from-mawk-cmd" | getline x; print x; close("echo from-mawk-cmd")}')"
st=$(awk 'BEGIN{s=strftime("%Y-%m-%d"); print (s ~ /^[0-9]{4}-[0-9]{2}-[0-9]{2}$/ ? "是" : "否"), length(s)}')
printf '  strftime 只验形状（不断行首 4-2-2 位数字吗 → %s；总长 %s 位）—— 日期值不上屏，它每天必变\n' $st
printf '  顺带守纪律：读未定义变量 ${AMK_NOSET-} → [%s]（空串，不报错）\n' "${AMK_NOSET-}"
echo "  → 直觉容易走两个极端：「mawk 是残废」或「和 gawk 全等」；屏幕上：数组、环境变量、"
echo "    管道 getline、strftime 全都在，缺的只是 gawk 私有语法（第 2、3 格那些）。"

echo
echo "=== 6. awk 的数字走 double：2^53 的断崖上，数字骗你不吭声 ==="
printf "  直接打 2^53 → %s（纯计算，每跑必同，才敢印出来）\n" "$(awk 'BEGIN{print 2^53}')"
printf "  判据一：2^53+1 打印回来还等于 2^53 吗 → %s（★ +1 被吞了，rc 照样 0）\n" \
  "$(awk 'BEGIN{print (2^53+1==2^53)?"是":"否"}')"
printf "  判据二：2^53+2 等于 2^53 吗 → %s（这个坡上只剩下偶数，+2 恰好还站得住）\n" \
  "$(awk 'BEGIN{print (2^53+2==2^53)?"是":"否"}')"
printf "  判据三：2^60+1 等于 2^60 吗 → %s（数越大，坑越深）\n" \
  "$(awk 'BEGIN{print (2^60+1==2^60)?"是":"否"}')"
echo "  → 直觉以为「数字做算术不会骗人」，屏幕上 +1 整个被吃掉，无报错、无警告。"
echo "  ★ 所以别拿 awk 数雪花 ID、累加超大整数 —— 过了 2^53 和第 3、4 格同族：静静地错。"

echo
echo "=== 7. 收口：「会叫 / 不会叫」两张清单并排 + 清场自证 ==="
printf "  --- 会叫的缺件（友善的那半：报错停下）---\n"
sed 's/^/    /' amk/e1
printf "    gensub/asort/strtonum 同款报错格式，rc 全 %s（真出错档）\n" "$rc1"
printf "  --- 不会叫的缺件（危险的那半：rc=0 但活没干）---\n"
printf '    FIELDWIDTHS：整行塞进 $1、$2 空、NF=1，rc=0\n'
printf '    %s：被当字母 s 用（同机 grep/sed 却读成空白类），rc=0\n' '/\s/'
printf "    超大整数 +1：精度掉崖，rc=0，纹丝不响\n"
echo "  ★ 带走判据：grep/sed/awk 里「没报错」绝不等于「按你的意思生效了」；"
echo "    现场验收三件兵：数 NF、打出来对内容、先让探针响一次再信守卫。"
printf "  自证（现场清干净）：删之前 amk 在吗 → %s\n" "$([ -d amk ] && echo 在 || echo 不在)"
rm -rf amk
printf "  删之后在吗 → %s ｜ 当前目录里名叫 amk 的条目 = %s（trap 只是备胎）\n" \
  "$([ -d amk ] && echo 在 || echo 不在)" "$(find . -maxdepth 1 -name amk | wc -l)"

exit 0
