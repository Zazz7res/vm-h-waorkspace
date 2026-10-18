#!/usr/bin/env bash
# ============================================================================
# awkjoin.sh —— awk 的「关联数组」：拿一个小表当索引，把两份文件拼成一份
#
# 一句话主旨：awk 手里除了「一行里的字段」，还有一整个**不需要声明、下标是字符串**
#             的数组；两份文件按 key 对齐（俗称 join）在 awk 里就是「第一个文件建表、
#             第二个文件查表」这一句话 —— 而 NR==FNR 就是那句里的「第一个文件」。
#
# demo.sh 里这样调：bash awkjoin.sh         （必须用相对文件名：见下方安全说明）
#
# 安全说明：只在本脚本当前目录下自建的一次性子目录 aj/ 里造样本做实验，
#           结尾由 trap 删掉；不碰 ~、不碰绝对 /tmp、不读写仓库既有文件、
#           不联网、不开端口、不 kill 任何不是本脚本起的进程。
#
# ★ 本机不拿什么当证据：
#   1) `for (k in arr)` 的**遍历顺序**不保证（本机是 mawk，换版本顺序就可能变），
#      所以凡是要上屏的数组遍历一律外接 sort，屏幕上只留「排好序的稳定形状」；
#   2) 不印耗时/inode/随机名；两份样本都是本脚本 here-doc 现造的固定内容，
#      所以每格的行数、计数、命中与否都是确定值，连跑两遍才可能一字不差。
# ============================================================================

trap 'rm -rf aj' EXIT
mkdir -p aj

cat > aj/name.tbl <<'EOF'
101 Ada
102 Grace
103 Linus
EOF

cat > aj/orders.log <<'EOF'
101 3 2026-01-05
103 7 2026-01-06
104 1 2026-01-07
101 2 2026-01-08
102 5 2026-01-09
EOF

# ---------------------------------------------------------------------------
echo "=== 1. 先认清 awk 的数组：不声明、下标是字符串、写一下就自己长出来 ==="
printf "  按 uid 数单量，整个程序只有 count[\$1]++ 这一句：\n"
awk '{c[$1]++} END{for (k in c) printf "    uid %s → %s 单\n", k, c[k]}' aj/orders.log | sort -k2,2n
printf "  样本一共 %s 行；三个 uid 各自的次数加起来 = %s（和总行数相等吗 → %s）\n" \
  "$(wc -l < aj/orders.log)" \
  "$(awk '{c[$1]++} END{t=0; for (k in c) t+=c[k]; print t}' aj/orders.log)" \
  "$([ "$(awk '{c[$1]++} END{t=0; for (k in c) t+=c[k]; print t}' aj/orders.log)" = "$(wc -l < aj/orders.log)" ] && echo 相等 || echo 不等)"
echo "  → 直觉以为「数组要先建、要声明长度」，实际 awk 里写 count[下标]++ 的那一刻它自己就长出来了。"
echo "  ★ bash 数组的下标是整数，awk 数组的下标是**字符串** —— 后面每一格的分歧都来自这一条。"

# ---------------------------------------------------------------------------
echo "=== 2. ★ NR 与 FNR：两台计数器，NR==FNR 就是「正在读第一个文件」 ==="
awk 'NR<=8{printf "    NR=%d FNR=%d 这一行来自 %s\n", NR, FNR, FILENAME}' aj/name.tbl aj/orders.log
printf "  name.tbl %s 行、orders.log %s 行；两文件一起喂，进程结束时 NR = %s\n" \
  "$(wc -l < aj/name.tbl)" "$(wc -l < aj/orders.log)" \
  "$(awk 'END{print NR}' aj/name.tbl aj/orders.log)"
echo "  → 直觉以为「NR 就是行号」，实际它是**整个 awk 进程**的总行号；FNR 才是「当前这个文件」的行号。"
echo "  ★ 所以 NR==FNR 只在第一个文件里成立 —— 这是 awk 里流传最广、也最容易被整句抄错的一行。"

# ---------------------------------------------------------------------------
echo "=== 3. 建表 → 查表：把 uid 换成名字，查不到的必须当场标出来 ==="
awk 'NR==FNR{name[$1]=$2; next}
     { if ($1 in name) printf "    %s → %-8s %s 单（第 %d 行）\n", $1, name[$1], $2, FNR;
       else            printf "    %s → %-8s %s 单（第 %d 行）★表里没这个 uid\n", $1, "<无档>", $2, FNR }' \
  aj/name.tbl aj/orders.log
printf "  ★ 先让守卫响一次：走「<无档>」分支的行数 = %s\n" \
  "$(awk 'NR==FNR{n[$1]=1; next} !($1 in n){c++} END{print c+0}' aj/name.tbl aj/orders.log)"
cp aj/name.tbl aj/namex.tbl
printf '104 Hopper\n' >> aj/namex.tbl
printf "  把 104 补进另一份表（aj/namex.tbl）之后，无档行数 = %s（1 归 0，这个 0 才算数）\n" \
  "$(awk 'NR==FNR{n[$1]=1; next} !($1 in n){c++} END{print c+0}' aj/namex.tbl aj/orders.log)"
echo "  → 直觉以为「join 不上整行就丢了」，实际丢不丢完全由你写的分支说了算，awk 默认谁也不丢。"

# ---------------------------------------------------------------------------
echo "=== 4. ★ 最阴的一格：只是**读一下**不存在的键，数组就多了一个元素 ==="
printf "  ① 正常建表：元素个数 = %s\n" "$(awk '{n[$1]=$2} END{print length(n)}' aj/name.tbl)"
printf "  ② 结尾多写一句 n[\"999\"]（只是读，没赋值）：元素个数 = %s\n" \
  "$(awk '{n[$1]=$2} END{ v = n["999"]; print length(n) }' aj/name.tbl)"
printf "  ③ 结尾改用 (\"999\" in n) 判存在：元素个数 = %s\n" \
  "$(awk '{n[$1]=$2} END{ d = ("999" in n); print length(n) }' aj/name.tbl)"
printf "  ② 和 ① 差了几个元素 → %s（★ 读一下就凭空造了一个，后面的统计全被污染）\n" \
  "$(( $(awk '{n[$1]=$2} END{ v = n["999"]; print length(n) }' aj/name.tbl) - $(awk '{n[$1]=$2} END{print length(n)}' aj/name.tbl) ))"
echo "  → 直觉以为「读一个不存在的键最多给个空串」，实际它**顺手创建**了这个键。"
echo "  ★ 判据：判断存在一律用「k in arr」；写「arr[k]」是**有副作用的读**，永远不能当存在性检查用。"

# ---------------------------------------------------------------------------
echo "=== 5. 下标一律先变字符串：101、\"101\"、\"0101\" 是不是同一个键 ==="
awk 'BEGIN{
       a[101]="甲"; a["101"]="乙";
       printf "    只写 a[101]=\"甲\" 与 a[\"101\"]=\"乙\"：元素个数 = %s\n", length(a);
       printf "    回头读 a[101] 得到 [%s]（★ 后写的那次把它盖掉了 → 两次写落在同一个格子 = 同一个键）\n", a[101];
       a["0101"]="丙";
       printf "    再写 a[\"0101\"]=\"丙\"：元素个数 = %s ｜ 读 a[\"0101\"] 得到 [%s]（自己的格子，没跟 101 抢）\n", length(a), a["0101"]
     }'
printf "  拿前导零的 key 去查表：造一行 uid 写成 0101，它在表里命中的个数 = %s\n" \
  "$(printf '0101 9 x\n' | awk 'NR==FNR{n[$1]=$2; next} {if ($1 in n) c++} END{print c+0}' aj/name.tbl -)"
printf "  同一行若写成 101：命中个数 = %s（差的就是那个前导零）\n" \
  "$(printf '101 9 x\n' | awk 'NR==FNR{n[$1]=$2; next} {if ($1 in n) c++} END{print c+0}' aj/name.tbl -)"
echo "  → 直觉以为「awk 会把 0101 当数字 101」，实际下标先变成**字符串**再比较，前导零一个都不能少。"
echo "  ★ 两份文件的 key 必须逐字节一致 —— 这正是第 3 格那个「<无档>」最常见的真实成因。"

# ---------------------------------------------------------------------------
echo "=== 6. 真正的 join：按 key 拼名字，顺手把同一个 key 的量加起来 ==="
awk 'NR==FNR{name[$1]=$2; next}
     { tot[$1] += $2; cnt[$1]++ }
     END{ for (k in tot) printf "    uid %s %-8s 出现 %s 单 合计 %s\n", k, (k in name ? name[k] : "<无档>"), cnt[k], tot[k] }' \
  aj/name.tbl aj/orders.log | sort -k2,2n
printf "  合计校验：分组后把各组再加起来 = %s ｜ 不分组直接数列求和 = %s ｜ 一样吗 → %s\n" \
  "$(awk 'NR==FNR{n[$1]=1; next}{t[$1]+=$2} END{s=0; for(k in t)s+=t[k]; print s}' aj/name.tbl aj/orders.log)" \
  "$(awk '{s+=$2} END{print s}' aj/orders.log)" \
  "$([ "$(awk 'NR==FNR{n[$1]=1; next}{t[$1]+=$2} END{s=0; for(k in t)s+=t[k]; print s}' aj/name.tbl aj/orders.log)" = \
        "$(awk '{s+=$2} END{print s}' aj/orders.log)" ] && echo 一样 || echo 不一样)"
echo "  → 直觉以为「join 完还得再跑一遍才能统计」，实际 awk 在**同一次扫描**里就把表建完、数完、加起来。"

# ---------------------------------------------------------------------------
echo "=== 7. 收口：数组三句话 + 当场自证现场清干净 ==="
printf "  ① NR==FNR 是「第一个文件」的判据，靠两台计数器在同一行**相等**；换文件时 FNR 回到 1。\n"
printf "  ② 判存在用「k in arr」；写成「arr[k]」会创建元素 —— 第 4 格当场数给你看过。\n"
printf "  ③ 下标一律先变字符串，key 要逐字节一致；遍历顺序不保证，所以每格都外接了 sort。\n"
printf "  自证一：删之前 aj 在吗 → %s\n" "$([ -d aj ] && echo 在 || echo 不在)"
rm -rf aj
printf "  删之后在吗 → %s ｜ 当前目录里名叫 aj 的条目 = %s（trap 只是备胎）\n" \
  "$([ -d aj ] && echo 在 || echo 不在)" "$(find . -maxdepth 1 -name aj | wc -l)"
printf "  ★ 一句话带走：awk 把「两份文件对齐」压成一行 —— 建表、next、查表；\n"
printf "    代价是它给了你一个会**自己长出来**的数组，读错一次就多一个假元素。\n"

exit 0
