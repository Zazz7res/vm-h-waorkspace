#!/usr/bin/env bash
# ============================================================================
# outcapture.sh —— 函数想「给出数据」只有 stdout 这一根管子，而 $( ) 会对它动四刀
#
#   demo.sh 里这样调：bash outcapture.sh
#   安全：只在**当前目录**里开 cap/ 子目录（用来演「替换里的 cd 带不出来」），跑完由
#         trap 'rm -rf cap' EXIT 删掉；不起后台进程、不开端口、不碰任何配置文件；
#         沙箱的绝对路径只参与比较，一个字都不上屏。
#   故意不加 set -e：$(exit 1) 那一支就是要拿非 0 当证据；也没全局开 pipefail。
#
# 主线①的另一半：return 只能交出一个 0~255 的整数（见 retcode.sh），**数据**只能写进 stdout
#   被 $( ) 接走。和 errmix.sh 的分工：那一支管「两个流怎么分合、重定向顺序为什么不能换」，
#   这一支管这根管子本身的形状 —— 尾部换行被吃光 / 多值只能是一坨文本 / 副作用出不了子 shell /
#   码和内容各走各的。
# ★ 本机不拿每跑必变的东西当证据：字节数、字符数、词数、行数全都由写死的字符串量出来，
#   同一个脚本第二次跑逐字一样；全篇没有耗时、没有 pid、没有绝对路径、没有随机名。
# ============================================================================

rm -rf cap
mkdir -p cap
trap 'rm -rf cap' EXIT

# show <屏幕上显示的写法> <给 printf %b 的内容>：同一段输出，量「进管子前」和「被接走后」各是多少
show() {
    local raw cap nraw ncap
    raw=$(printf '%b' "$2" | wc -c)                 # 原始字节
    cap=$(printf '%b' "$2")                         # 被命令替换接走
    nraw=$(printf '%b' "$2" | tr -dc '\n' | wc -c)  # 原始换行个数
    ncap=$(printf '%s' "$cap" | tr -dc '\n' | wc -c)
    printf '  %-16s 原始 %s 字节 / %s 个换行 → 捕获后 %s 字符 / %s 个换行\n' \
        "$1" "$raw" "$nraw" "${#cap}" "$ncap"
}

echo "=== 1. 第一刀：尾部换行被吃掉，吃的还是「所有连续的尾部」 ==="
show "printf 'a\nb\n'"   'a\nb\n'
show "printf 'a\nb'"     'a\nb'
show "printf 'a\nb\n\n'" 'a\nb\n\n'
show "printf '\n'"       '\n'
echo "  → 直觉以为「只吃一个换行、差一个字节返回值就差一个字节」：实测 4 字节、3 字节、5 字节三种写法"
echo "    捕获后都是 3 个字符 —— 它把结尾的换行**全**删光，中间的换行一个不动（前一行还剩 1 个）。"
echo "  ★ 最后一行最阴：函数只是多打了一个空行，捕获到的就是空字符串，[ -z \"\$v\" ] 当场把「有结果」判成没结果。"

echo
echo "=== 2. 第二刀：想带两个值，只有「一坨文本」这一种写法，两种编码各付一种代价 ==="
pair_space() { local x=12 y=34; echo "$x $y"; }
s=$(pair_space)
IFS=' ' read -r sx sy <<< "$s"
printf '  甲·空格版 echo "$x $y" → 捕获到 [%s] ｜ 行数=%s ｜ 词数=%s ｜ 拆出来 sx=[%s] sy=[%s]\n' \
    "$s" "$(printf '%s\n' "$s" | wc -l)" "$(printf '%s\n' "$s" | wc -w)" "$sx" "$sy"
bad_space() { local x="红 黄" y=蓝; echo "$x $y"; }
b=$(bad_space)
IFS=' ' read -r bx by <<< "$b"
printf '  ★ 代价现场：两个值里有一个本身带空格 → 捕获到 [%s]（%s 个词），拆出来 bx=[%s] by=[%s]\n' \
    "$b" "$(printf '%s\n' "$b" | wc -w)" "$bx" "$by"
pair_line() { local x=12 y=34; echo -e "$x\n$y"; }
l=$(pair_line)
read -r ly1 <<< "$l"
printf '  乙·换行版 echo -e "x\\ny" → 捕获到 [%s]（斜杠代表换行）｜ 字符数=%s ｜ 换行数=%s ← 没被吃，因为它不在尾部\n' \
    "$(printf '%s' "$l" | tr '\n' '/')" "${#l}" "$(printf '%s' "$l" | tr -dc '\n' | wc -c)"
printf '    可 read -r 只读得到第一行：ly1=[%s] ｜ ★ 第二个值压根没进任何变量（read 撞到换行就收工）\n' "$ly1"
n2=0
while IFS= read -r ln; do n2=$((n2 + 1)); done <<< "$l"
printf '    要拿全两个值就只能逐行读（while read / mapfile）：这里读到 %s 行\n' "$n2"
echo '  → 一句话：空格版在赌「值里没有空格」，换行版在赌「你记得逐行读」。$( ) 交回来的从来不是两个变量，'
echo "    只是一段文本；真要稳稳带出多个值，得走全局变量或 nameref（见 localscope.sh）。"

echo
echo "=== 3. 第三刀：诊断写进 stdout，返回值就变成「一个词加一句废话」 ==="
noisy() { echo "调试中…"; echo "结果"; }
n3=$(noisy)
printf '  事故版 v=$(noisy) → v=[%s]（斜杠=换行）｜ 行数=%s ｜ 首行=[%s] ← 首行是废话\n' \
    "$(printf '%s' "$n3" | tr '\n' '/')" "$(printf '%s\n' "$n3" | wc -l)" "${n3%%$'\n'*}"
same=no; [ "$n3" = "结果" ] && same=yes
printf '  判定 [ "$v" = "结果" ] → %s ｜ 判空 [ -z "$v" ] → %s（明明没拿到值也说「有」）\n' \
    "$([ "$same" = yes ] && echo 真 || echo 假)" "$([ -z "$n3" ] && echo 空 || echo 非空)"
quiet() { echo "调试中…" >&2; echo "结果"; }
printf '  正解版：下一行就是 v=$(quiet) 的执行现场，屏幕上会先掉出那句诊断 ——\n'
n4=$(quiet)
printf '  ★ 上面那行诊断**没有缩进**：它是函数的 2 号 fd 原路上屏，本脚本一个字都没加工；\n'
printf '    而 v=[%s]（字符数=%s）里干干净净 —— 命令替换只接管 1 号 fd，2 号它根本接不着\n' \
    "$n4" "${#n4}"
echo "  → 判据：函数里每一句 echo 先问一句「这是要带回的值，还是给人看的？」给人看的一律 >&2；"
echo "    漏一句，比较、判空、取首行同时中招（想把 stderr 也捞进变量得写 2>&1，顺序代价见 errmix.sh）。"

echo
echo "=== 4. 第四刀：\$( ) 是个子 shell，里面的副作用一个都带不出来 ==="
before=$PWD
r4=$(cd cap && pwd)
printf '  ① 替换里 cd cap：里面的 pwd 等于「外层 PWD/cap」吗 → %s ｜ 外层目录变了吗 → %s\n' \
    "$([ "$r4" = "$before/cap" ] && echo 是 || echo 否)" "$([ "$PWD" = "$before" ] && echo 没变 || echo 变了)"
G=外层
setinner() { G=里面; echo 值; }
r5=$(setinner)
printf '  ② 替换里给全局变量赋值：v=[%s] ｜ 外面的 G=[%s] ｜ 和「里面」相等吗 → %s\n' \
    "$r5" "$G" "$([ "$G" = 里面 ] && echo 是 || echo 否)"
r6=$(echo hi; exit 3); rc6=$?
printf '  ③ 替换里 exit 3（前面还有一句 echo）：v=[%s] ｜ 外层 $?=%s ｜ ★ 脚本还活着，这一行就是证据\n' "$r6" "$rc6"
r7=$(exit 1); rc7=$?
printf '     只写 exit 1：v=[%s]（字符数=%s）｜ 外层 $?=%s ｜ 外层脚本继续往下跑\n' "$r7" "${#r7}" "$rc7"
echo "  → ③ 那一格要说清：命令替换里的 exit 只杀掉**那个子 shell**，它把 3 交回给赋值语句当退出码，"
echo "    所以「码」传得出来（\$? = 3），「终止」传不出来。想真结束整个脚本，只能在主干上 exit（见 trapreturn.sh）。"

echo
echo "=== 5. 两条线别搅在一起：同一次调用里 return 的码和 stdout 的内容互不相干 ==="
both() { echo hello; return 7; }
v8=$(both); rc8=$?
printf '  原文：v=$(both) ｜ 函数里 echo 和 return 都执行了 → v=[%s]（%s 字符）｜ $?=%s\n' "$v8" "${#v8}" "$rc8"
if [ -n "$v8" ]; then by=有货; else by=没货; fi
if both >/dev/null 2>&1; then bc=成功; else bc=失败; fi
printf '  ★ 内容判据说「%s」、码判据说「%s」：两句都没错，因为它们量的不是同一件事\n' "$by" "$bc"
is_bad() { echo 在的; return 0; }
printf '  对照·布尔函数（本意只回答成没成，顺手 echo 了一句）：if is_bad >/dev/null → 走 %s 分支\n' \
    "$(if is_bad >/dev/null; then echo then; else echo else; fi)"
printf '  同一个 is_bad 放进替换里 v2=[%s] ← 它「以为」自己在报布尔，实际交出去的是一坨文本\n' "$(is_bad)"
echo "  → 一句话：\$? 这条通道只有 8 位宽，只能回答「成没成」；stdout 那条只能回答「是什么」。"
echo "    一个函数可以两条都给，但调用方每次必须清楚自己伸手拿的是哪一条。"

echo
echo "=== 6. 收口 + 自证 ==="
echo "  四刀背下来：① 尾部换行全吃光 ② 多值只能是一坨文本 ③ 漏一句 echo 就污染 ④ 副作用出不了子 shell。"
printf '  现场目录 cap/ 还在吗 → %s ｜ 删之前里面的条目数=%s ｜ 本脚本自己的作业表行数=%s\n' \
    "$([ -d cap ] && echo 在 || echo 不在了)" "$(ls -A cap | wc -l)" "$(jobs -p | wc -l)"
rm -rf cap
printf '  现在 cap/ 还在吗 → %s ｜ 当前目录里名为 cap 的条目数=%s\n' \
    "$([ -d cap ] && echo 在 || echo 不在了)" "$(ls -A | grep -cx cap || true)"
