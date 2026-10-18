#!/usr/bin/env bash
# ============================================================================
# threeway.sh —— 同一件小事，grep / sed / awk 各做一次：谁够得着，谁只是看着像
#
# 一句话主旨：选工具不是选「我熟哪个」，是选「这件事要不要跨行、要不要算术」；
#             grep 只回答「这一行有没有」，sed 只回答「这一行怎么变成另一行」，
#             awk 才回答「这一行里那个字段是几，加起来多少」。
#
# demo.sh 里这样调：bash threeway.sh        （必须用相对文件名：见下方安全说明）
#
# 安全说明：只在本脚本当前目录下自建的一次性子目录 tw/ 里造样本做实验，
#           结尾由 trap 删掉；不碰 ~、不碰绝对 /tmp、不读写仓库既有文件、
#           不联网、不开端口、不 kill 任何不是本脚本起的进程。
#           ★ 第 6 格会**故意把一个文件截成 0 字节**，但被截的是本格刚造的副本，
#             原件在同一格内先备份、演示完立刻用副本还原，不伤任何别的东西。
#
# ★ 本机不拿什么当证据：不印耗时/inode/随机名；第 3、6 格的数字（命中几行、
#   截断后剩几字节）都是本机实测确定值，所以敢当证据。计数类断言全部
#   「先让它响一次」（用错误写法跑一遍，让计数器叫），再报正确的 0。
# ============================================================================

trap 'rm -rf tw' EXIT
mkdir -p tw

cat > tw/score.txt <<'EOF'
Ada 71
Grace 95
Linus 63
Margaret 88
Alan 52
EOF
printf 'Marie Curie 90\n' >> tw/score.txt

# ---------------------------------------------------------------------------
echo "=== 1. 任务「每行只要最后那个数」：三种写法，输出逐字一样 ==="
grep -oE '[0-9]+$' tw/score.txt > tw/bygrep
sed 's/.* //' tw/score.txt > tw/bysed
awk '{print $NF}' tw/score.txt > tw/byawk
printf "  grep -oE 得 %s 行 ｜ sed 贪婪删到最后一个空格得 %s 行 ｜ awk 末字段得 %s 行\n" \
  "$(wc -l < tw/bygrep)" "$(wc -l < tw/bysed)" "$(wc -l < tw/byawk)"
printf "  三份逐字比较：%s\n" \
  "$([ "$(cmp -s tw/bygrep tw/bysed && cmp -s tw/bysed tw/byawk && echo 同)" = 同 ] && echo 'grep = sed = awk，一个字都不差' || echo '三者不一样，看下面')"
echo "  → 直觉以为「三种写法肯定各有各的样子」，实际这个任务上它们交付的是同一串字节。"
echo "  ★ 所以别用「写法帅不帅」选工具；要往下两格看，那里才开始分家。"

# ---------------------------------------------------------------------------
echo "=== 2. 名字里多一个空格，「取第 2 列」立刻掉下去（计数器先响一次）==="
awk '{print $2}' tw/score.txt > tw/col2
printf "  用 awk 取第 2 字段：第 6 行给出的是[%s]，它其实是姓氏不是分数\n" "$(sed -n '6p' tw/col2)"
printf "  ★ 先让守卫响：这 6 行结果里纯数字的只有 %s 行（本该 6 行）→ 已经叫了\n" \
  "$(grep -cE '^[0-9]+$' tw/col2)"
awk '{print $NF}' tw/score.txt > tw/last
printf "  换成「最后一个字段」：纯数字行数 = %s，和总行数 %s 相等吗 → %s\n" \
  "$(grep -cE '^[0-9]+$' tw/last)" "$(wc -l < tw/score.txt)" \
  "$([ "$(grep -cE '^[0-9]+$' tw/last)" = "$(wc -l < tw/score.txt)" ] && echo 相等 || echo 不等)"
echo "  → 直觉以为「第 2 列就是分数」，实际 awk 的「列」是按空白切出来的**字段**，名字里一个空格就整体错位。"
echo "  ★ sed 的 .* （贪婪）和 awk 的 \$NF 在这件事上是同一句话：都从行尾往回数。"

# ---------------------------------------------------------------------------
echo "=== 3. ★ 任务变成「分数低于 60 的挑出来」：grep 只会「长得像」，不会比大小 ==="
cp tw/score.txt tw/s2.txt
printf 'Curie 59.5\nHopper 105\n' >> tw/s2.txt
printf "  新样本比原来多两行（%s 行），里面 59.5 是低于 60 的、105 不是\n" "$(wc -l < tw/s2.txt)"
printf "  用 grep 枚举数字位数（5[0-9]|4[0-9]|…）：命中 %s 行\n" \
  "$(grep -cE ' (5[0-9]|4[0-9]|3[0-9]|2[0-9]|1[0-9]|[0-9])$' tw/s2.txt)"
printf "  用 awk 真的比大小 \$NF+0 < 60：命中 %s 行，分别是[%s]\n" \
  "$(awk '$NF+0<60{n++;printf "%s ", $1} END{printf ""}' tw/s2.txt | wc -w)" \
  "$(awk '$NF+0<60{printf "%s ", $1}' tw/s2.txt | sed 's/ $//')"
grep -E ' (5[0-9]|4[0-9]|3[0-9]|2[0-9]|1[0-9]|[0-9])$' tw/s2.txt > tw/g60
awk '$NF+0<60' tw/s2.txt > tw/a60
printf "  两份差几行 → %s 行（★ 差的就是那个 59.5，grep 的模式结尾必须是数字，小数点把它挡在外面）\n" \
  "$(diff tw/g60 tw/a60 | grep -c '^>')"
echo "  → 直觉以为「grep 能匹配数字，所以能筛范围」，实际它匹配的是**字形的形状**，不是数的大小。"
echo "  ★ 判据：这件事要不要「把这段文本当数读」？要 → awk（或 sed 也做不了比较）。"

# ---------------------------------------------------------------------------
echo "=== 4. 任务变成「一共几行、平均几分」：只有 awk 顺手 ==="
printf "  grep -c '' 数行数 = %s ｜ wc -l 数行数 = %s（本机两个一致，因为每行都有换行符）\n" \
  "$(grep -c '' tw/score.txt)" "$(wc -l < tw/score.txt)"
printf "  grep 能不能顺手给出总分 → 不能：它交回的只有匹配到的**文本**，没有「把这段当数加起来」这一步\n"
printf "  awk 一行给出：行数 %s、总分 %s、平均 %s\n" \
  "$(awk 'END{print NR}' tw/score.txt)" \
  "$(awk '{s+=$NF} END{print s}' tw/score.txt)" \
  "$(awk '{s+=$NF} END{printf "%.1f", s/NR}' tw/score.txt)"
printf "  最后一行没有换行符时，两个尺子会分家：造一份尾部无换行的样本后 wc -l = %s，grep -c '' = %s\n" \
  "$(printf 'x 1\ny 2' > tw/nonl.txt; wc -l < tw/nonl.txt)" "$(grep -c '' tw/nonl.txt)"
echo "  → 直觉以为「文件就是几行」，实际 wc 数的是**换行符个数**，awk 的 NR 数的是**它见过的记录数**。"

# ---------------------------------------------------------------------------
echo "=== 5. 任务变成「命中行再往下多打一行」：sed 要主动拼，awk 有 getline，grep 靠 -A ==="
printf "  grep -A1 -E 'Ada|Alan' 一共给 %s 行，其中 %s 行是 grep 自己插进去的分隔符\n" \
  "$(grep -A1 -E 'Ada|Alan' tw/score.txt | wc -l)" "$(grep -A1 -E 'Ada|Alan' tw/score.txt | grep -c '^--$')"
printf "  sed -n '/Ada/{n;p}' 给 = [%s]（n 把下一条行拉进模式空间，此刻它手里才有两行）\n" \
  "$(sed -n '/Ada/{n;p}' tw/score.txt)"
printf "  awk '/Ada/{getline; print}' 给 = [%s]\n" "$(awk '/Ada/{getline; print}' tw/score.txt)"
printf "  后两条逐字一样吗 → %s\n" \
  "$([ "$(sed -n '/Ada/{n;p}' tw/score.txt)" = "$(awk '/Ada/{getline; print}' tw/score.txt)" ] && echo 一样 || echo 不一样)"
echo "  → 直觉以为「sed 一次只能看一行，所以做不了这件事」，实际 N 和 {n;p} 就是它伸向下一行的手。"

# ---------------------------------------------------------------------------
echo "=== 6. ★ 任务变成「就地改这份文件」：sed 能，grep 不能，awk 会把自己的输入截成 0 字节 ==="
cp tw/score.txt tw/orig.txt
awk '{print $1 ": ok"}' tw/orig.txt > tw/orig.txt
printf "  把 awk 的输出发回它自己正在读的那个文件，之后该文件剩 %s 字节（★ 重定向在 awk 读之前就先清空）\n" \
  "$(wc -c < tw/orig.txt)"
cp tw/score.txt tw/orig.txt
printf "  用同一份样本还原之后：%s 字节、%s 行（和格 1 开始时一样，数据没丢）\n" \
  "$(wc -c < tw/orig.txt)" "$(wc -l < tw/orig.txt)"
awk '{print $1 ": ok"}' tw/orig.txt > tw/orig.new && mv -f tw/orig.new tw/orig.txt
printf "  正确写法（先写 .new 再换掉）：%s 行，第一行现在是[%s]\n" "$(wc -l < tw/orig.txt)" "$(head -1 tw/orig.txt)"
cp tw/score.txt tw/orig2.txt
sed -i 's/^Margaret/Marge/' tw/orig2.txt
printf "  sed -i 在一份干净副本上就地改：还搜得到 Margaret 吗 → 命中 %s 次 ｜ 搜 Marge → 命中 %s 次\n" \
  "$(grep -c '^Margaret' tw/orig2.txt)" "$(grep -c '^Marge' tw/orig2.txt)"
before=$(wc -c < tw/orig2.txt)
grep 'Ada' tw/orig2.txt > /dev/null
after=$(wc -c < tw/orig2.txt)
printf "  grep 跑完一遍，文件字节数 %s → %s，变了吗 → %s（★ grep 从头到尾只读不写）\n" \
  "$before" "$after" "$([ "$before" = "$after" ] && echo 没变 || echo 变了)"
echo "  → 直觉以为「加个 > 就是写回原文件」，实际那个 > 是**先把文件清空、再启动命令**，命令读到的就是空文件。"

# ---------------------------------------------------------------------------
echo "=== 7. 收口：三张判据 + 当场自证现场清干净 ==="
printf "  这件事要不要**跨行**？要 → sed 的 N / awk 的 getline；grep 换行就是它的天花板。\n"
printf "  要不要**把文本当数读**（比大小、求和、平均）？要 → awk；grep 的字形匹配会在 59.5 上漏。\n"
printf "  要不要**落回文件**？sed -i 一步；awk 只能「写新文件再换」；grep 不参与。\n"
printf "  自证一：删之前 tw 在吗 → %s\n" "$([ -d tw ] && echo 在 || echo 不在)"
rm -rf tw
printf "  删之后在吗 → %s ｜ 当前目录里名叫 tw 的条目 = %s（trap 只是备胎）\n" \
  "$([ -d tw ] && echo 在 || echo 不在)" "$(find . -maxdepth 1 -name tw | wc -l)"
printf "  ★ 一句话带走：先问「单位的形状」够不够，再问「要不要算」，最后问「要不要落地」——\n"
printf "    三问答完，该用哪个工具基本就自己浮出来了；反过来按「哪个我背得熟」选，就一定会在第 3 格翻车。\n"

exit 0
