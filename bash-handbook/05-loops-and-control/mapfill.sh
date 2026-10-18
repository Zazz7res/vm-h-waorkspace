#!/usr/bin/env bash
# ============================================================================
# mapfill.sh —— mapfile / readarray：把「读进来」变成一次动作，而不是一串循环
#
#   demo.sh 里这样调：bash mapfill.sh
#   安全：只在**当前目录**（= demo 的沙箱 work/）里建 map/ 子目录，跑完由 trap 删掉。
#   故意不加 set -e：第 6 段要演「文件不存在」和「set -u 下的空数组」两种失败原文。
#
# 为什么本章要专门给它一节：第 12、13 段那两个坑（漏末行、循环体吃 stdin）的共同解法就是
# 「别一边读一边循环」—— 一次 mapfile 进数组，for 走数组，坑就整片消失。
# 代价也写在第 7 段：它是**整份进内存**的。
# ============================================================================

rm -rf map
mkdir -p map
trap 'rm -rf map' EXIT

printf 'alpha\nbeta\ngamma\n' > map/plain.txt

echo "=== 1. 一次读全：mapfile 拿到什么、和 while read 的分工差别在哪 ==="
mapfile -t rows < map/plain.txt
printf '  数组长度 = %s ｜ 元素 = [%s] ｜ 第一个 = [%s] ｜ 最后一个 = [%s]\n' \
    "${#rows[@]}" "${rows[*]}" "${rows[0]}" "${rows[-1]}"
printf '  ★ 注意末行：map/plain.txt 末尾**有**换行；有没有都无所谓，mapfile 不靠它判成功（对比第 12 段）。\n'
mapfile no_t < map/plain.txt
printf '  不加 -t 时元素带不带行尾？ [%s]（-t 的意思就是「把行尾那个分隔符去掉」）\n' "${no_t[0]}"

echo
echo "=== 2. 三种来源：< 文件 ｜ <<< 一个串 ｜ < <(命令) —— 第三种最容易写错 ==="
mapfile -t a < map/plain.txt
mapfile -t b <<< "x y z"
mapfile -t c < <(printf 'one\ntwo\nthree\n')
printf '  < 文件      → %s（%s 个元素）\n' "${a[*]}" "${#a[@]}"
printf '  <<< 一个串  → %s（%s 个元素 ★ 整个串只有 1 行 → 就是 1 个元素，不会按空格拆）\n' "${b[*]}" "${#b[@]}"
printf '  < <(命令)   → %s（%s 个元素）\n' "${c[*]}" "${#c[@]}"
echo "  → 想把命令的输出按空格拆成多个元素，mapfile 帮不了你（它是**按行**的），"
echo "    那属于第 02 章的词与 IFS；mapfile 的定位是「一份行数组」。"

echo
echo "=== 3. ★ 不加 -t 时 \\r 会留在元素里：CRLF 文件的隐形后果 ==="
printf 'win1\r\nwin2\r\n' > map/crlf.txt
mapfile -t with_t < map/crlf.txt
mapfile without_t < map/crlf.txt
printf '  带 -t 的第 1 个元素字节数 = %s ｜ 不带 -t 的 = %s（多的那 1 个字节就是 \\r）\n' \
    "$(printf '%s' "${with_t[0]}" | wc -c)" "$(printf '%s' "${without_t[0]}" | wc -c)"
printf '  拿它去比字符串 → [%s] = "win1" 吗：%s\n' "${without_t[0]}" \
    "$([ "${without_t[0]}" = "win1" ] && echo 相等 || echo 不相等)"
echo "  → 这就是「脚本在 Windows 生成的 CSV 上莫名对不上」的一种形状：差别看不见，比较却全错。"
echo "    注意 -t 去掉的是**行尾分隔符**（\\n），它不管 \\r；要连 \\r 一起去掉只有两条路："
echo "    ① 整份先过一遍 tr -d '\\r' 再 mapfile；② 读完之后把行尾那个 CR 当后缀剪掉，本机当场剪了一刀："
echo "    ★ 剪的是 mapfile -t **之后**的那个元素：不带 -t 时元素结尾是 \\n（6 字节 = win1+CR+LF），"
echo "      那时候剪 CR 会一刀剪空（我上一版就剪错了对象，屏幕上 6 字节剪完还是 6 字节）。"
CR=$(printf '\r')
printf '      现场剪一刀：带 -t 的元素=%s 字节 ｜ 剪掉行尾 CR 后=%s 字节 ｜ 剪完和 "win1" 相等吗 → %s\n' \
    "$(printf '%s' "${with_t[0]}" | wc -c)" \
    "$(printf '%s' "${with_t[0]%$CR}" | wc -c)" \
    "$([ "${with_t[0]%$CR}" = "win1" ] && echo 是 || echo 否)" 

echo
echo "=== 4. 三个数字开关：-s 跳过 / -n 限量 / -O 从数组第几格开始写 ==="
printf 'L1\nL2\nL3\nL4\nL5\n' > map/five.txt
mapfile -t all < map/five.txt
mapfile -t -s 2 few < map/five.txt
mapfile -t -n 2 cap < map/five.txt
mapfile -t -O 3 merged < map/five.txt
printf '  全读        → %s（%s 个）\n' "${all[*]}" "${#all[@]}"
printf '  -s 2 跳两行 → %s（%s 个）\n' "${few[*]}" "${#few[@]}"
printf '  -n 2 只取两条 → %s（%s 个）\n' "${cap[*]}" "${#cap[@]}"
printf '  -O 3 从第 3 格开始写 → 下标 0,1,2 = [%s][%s][%s]，3,4 = [%s][%s]\n' \
    "${merged[0]}" "${merged[1]}" "${merged[2]}" "${merged[3]}" "${merged[4]}"
echo "  → 想要「第 3 到第 5 行」这种切片，bash 里没有 while 计数器优雅的做法，"
echo "    要么 -s + -n 组合，要么读全再用 \${arr[@]:2:3} 切（第 02 章的数组切片）。"

echo
echo "=== 5. 换分隔符：mapfile -d 能把「按行」改成按任意单字符（bash 4.4+） ==="
printf 'k1=v1;k2=v2;k3=v3;' > map/semi.txt
mapfile -t -d ';' pairs < map/semi.txt
printf '  按 ; 切 → %s 个元素：[%s] ｜ 末尾那个空段被留着了吗 → 长度=%s\n' "${#pairs[@]}" "${pairs[*]}" "${#pairs[@]}"
mapfile -t -d '' whole < map/five.txt
printf '  -d "" = 读到 EOF 为止 → %s 个元素（整份文件成了 1 格，含原来的换行）\n' "${#whole[@]}"
echo "  → -d 的用处：把「以 NUL 分隔的 find -print0」安全读进数组（第 04 章 xargs 那两节的循环版解法）："
mapfile -t -d '' files < <(find map -maxdepth 1 -type f -name '*.txt' -print0)
names=""
for f in "${files[@]}"; do names="$names ${f##*/}"; done
printf '    本机读到 %s 个文件名：%s（NUL 分隔，名字里有空格/换行也不会散）\n' "${#files[@]}" "$names"
echo "    ★ 这里刻意用 < <(...) 而不是 find ... | mapfile：管道会让 mapfile 跑到子进程里，"
echo "      数组读完了也带不回来（第 14 节那条规矩在第 15 节同样生效）。上一版就是写成管道的，"
echo "      屏幕上量到 0 个 —— 那 0 不是「没有找到文件」，是「找到的东西留在了别的进程里」。"

echo
echo "=== 6. 两种失败原文：文件不存在 ／ set -u 下的空数组 ==="
mapfile -t miss < map/根本没有.txt
printf '  读不存在的文件：mapfile 退出码=%s（重定向在**进 mapfile 之前**就失败了）\n' "$?"
mapfile -t empty < <(:)
printf '  空输入：数组长度=%s ｜ 这个数组存在吗 → %s\n' "${#empty[@]}" "$(declare -p empty 2>/dev/null >/dev/null && echo 存在 || echo 不存在)"
printf '  set -u 下问长度：'; bash -uc 'mapfile -t e < <(:); echo "长度=${#e[@]} ｜ 退出码=$? ← 问长度不炸"'
printf '  set -u 下展开它：'; bash -uc 'mapfile -t e < <(:); echo "[${e[@]}] ｜ 退出码=$? ← 5.x 这里也不炸，只给你一个空串"' 2>&1
echo "  → ★ 本机（bash 5.3）两条都安静地给空值。老 bash（4.3 及更早）里第二条会报 unbound variable，"
echo "    这条差别**本机未验证**（这里没有旧版本可跑），只当作「别在 5.x 上依赖它报错」来记。"

echo
echo "=== 7. 什么时候别用 mapfile：它是整份进内存（这里只报方向，不报字节数） ==="
printf '  同一个文件，两种读法各自「手里同时握着多少」：\n'
printf '    mapfile -t：文件的全部行 → 一个数组（行数=%s，**一次性**都在内存里）\n' "${#all[@]}"
printf '    while read ：任意时刻只有一行在变量里（流式，多大都读得动）\n'
echo "  → 判据：行数和单行大小都能预估 → mapfile（干净）；可能是几百 MB 的日志 → 老老实实 while read，"
echo "    并且把第 12、13 段的两个修法一起带上。"

echo
echo "=== 8. 收口：三句话 ==="
echo "  ① mapfile 是「按行」的读取器：-t 去行尾、-s 跳、-n 限量、-O 偏移、-d 换分隔符，readarray 是同一条命令的别名。"
echo "  ② 它是第 12/13 两段事故的整片解法：先 mapfile 进数组，再 for 走数组 —— 循环不再碰 stdin，也不再漏末行。"
echo "  ③ 代价是内存：它把整份输入握在数组里。大文件回到 while read + || [ -n \"\$REPLY\" ] + < /dev/null 那套。"
