#!/usr/bin/env bash
# ============================================================================
# readlast.sh —— while read 为什么会漏掉最后一行（以及三种修法）
#
#   demo.sh 里这样调：bash readlast.sh
#   安全：只在**当前目录**（= demo 的沙箱 work/）里建 last/ 子目录，跑完由 trap 删掉。
#   故意不加 set -e：要的就是 read 返回非 0 时循环「体面地少跑一轮」那个现场。
#
# 这一条几乎每个写过 shell 的人都踩过一次：文件是人手编辑的，编辑器保存时
# 到底加不加末尾换行**完全看配置**（POSIX 说该加，很多工具不加）。
# 于是「同样内容的两份文件，一份数出 3 行、一份数出 2 行」，而且没有任何报错。
# ============================================================================

rm -rf last
mkdir -p last
trap 'rm -rf last' EXIT

printf 'apple\nbanana\ncherry\n' > last/with_nl.txt     # 末尾有换行
printf 'apple\nbanana\ncherry'   > last/no_nl.txt       # 末尾没有换行（只差这一个字节）

echo "=== 1. 先把两份文件的形状钉死：只差 1 个字节 ==="
for f in last/with_nl.txt last/no_nl.txt; do
    printf '  %-20s 字节数=%s  wc -l 行数=%s  最后一字节=%s\n' \
        "$f" "$(wc -c < "$f")" "$(wc -l < "$f")" "$(tail -c 1 "$f" | od -An -c | tr -d ' \n')"
done
echo "  → 注意 wc -l 说 no_nl.txt 只有 2 行：wc -l 数的是**换行符的个数**，不是「有几段文本」。"
echo "    可这两份文件的**内容**都是三种水果。后面所有「数一数跑了几轮」的地方，都以这个差值为源头。"

echo
echo "=== 2. 事故现场：同一个 while read，两份文件少跑一轮 ==="
for f in last/with_nl.txt last/no_nl.txt; do
    n=0
    while read -r fruit; do
        n=$((n + 1))
        printf '    第 %s 轮读到：%s\n' "$n" "$fruit"
    done < "$f"
    printf '  %-20s 循环跑了 %s 轮\n' "$f" "$n"
done
echo "  → 第二份文件里那个 cherry **一个字都没被读到**，而且循环正常结束、退出码 0。"
echo "    这就是它危险的地方：不报错，只是安静地少干一件活。"

echo
echo "=== 3. 为什么：read 的返回值问的是「读到分隔符了吗」，不是「读到字了吗」 ==="
printf 'cherry' | { read -r v; printf '  输入没有换行：read 退出码=%s ｜ 变量 v=[%s]\n' "$?" "$v"; }
printf 'cherry\n' | { read -r v; printf '  输入有换行：  read 退出码=%s ｜ 变量 v=[%s]\n' "$?" "$v"; }
echo "  → ★ 关键就在这两行：**赋值成功了，返回值仍是 1**。"
echo "    while 只看返回值决定「进不进循环体」，所以那次成功的赋值被条件判断丢掉了。"
echo "    记住这句：read 的退出码 = 「这次有没有撞上换行（或 -d 指定的分隔符）」，与「有没有拿到内容」无关。"

echo
echo "=== 4. 修法一（推荐，一行）：条件里补一句「手里还有货就再进一次」 ==="
n=0
while read -r fruit || [ -n "$fruit" ]; do
    n=$((n + 1))
    printf '    第 %s 轮读到：%s\n' "$n" "$fruit"
done < last/no_nl.txt
echo "  → 现在 no_nl.txt 也是 $n 轮。为什么用 [ -n \"\$fruit\" ] 就够：EOF 且手里空 → 条件假，退出。"
echo "    ★ 别写成 while read -r fruit; do ... 之后再在循环外补一次 —— 有换行的文件会被多处理一遍。"
echo "    拿两份文件各跑一次对账（这才是这条修法合格的判据）："
for f in last/with_nl.txt last/no_nl.txt; do
    n=0
    while read -r fruit || [ -n "$fruit" ]; do n=$((n + 1)); done < "$f"
    printf '      %-20s 跑了 %s 轮（两份现在都是 3 → %s）\n' "$f" "$n" \
        "$([ "$n" = 3 ] && echo 是 || echo 否)"
done

echo
echo "=== 5. 修法二/三：不改循环条件的那两条路 ==="
echo "  修法二：把「补一个换行」交给数据管道，循环体一个字不动"
n=0
while read -r fruit; do n=$((n + 1)); done < <(cat last/no_nl.txt; echo)
echo "      用 <(cat 文件; echo) 喂进去 → $n 轮（代价：多起一个进程，且路径要能 cat）"
echo "  修法三：一次读进数组，压根不用 read 做循环条件"
mapfile -t fruits < last/no_nl.txt
printf '      mapfile -t 读到 %s 个元素：%s（末行不需要换行也能拿到）\n' "${#fruits[@]}" "${fruits[*]}"
echo "      ★ 但 mapfile 是把**整份文件留在内存里**，大文件要回头用第 2 节的流式写法。"

echo
echo "=== 6. 顺手把 wc -l 的账也结掉：谁会被这个坑骗 ==="
printf '  awk / sed / cut / grep 是按「记录」切行的，末行没有换行照样算一行：\n'
printf '    awk 数到的行数 = %s ｜ grep -c "" 数到的行数 = %s\n' \
    "$(awk 'END{print NR}' last/no_nl.txt)" "$(grep -c "" last/no_nl.txt)"
printf '  → 只有 while read 这一族（bash 的 read、以及任何「以换行判成功」的读法）会漏。\n'
printf '    所以「脚本里数出来的行数」和「awk 数出来的行数」不等时，先怀疑末行换行，再怀疑逻辑。\n'

echo
echo "=== 7. 收口：三条能带走的话 ==="
echo "  ① 见到 while read 就条件反射地问一句：最后一行有换行吗？修法只有 || [ -n \"\$REPLY\" ] 这一条最省。"
echo "  ② read 的退出码语义是「撞到分隔符没有」，不是「读到东西没有」—— 这一条也解释了"
echo "     read -t 超时（>128）和 read 到 EOF（1）为什么都算失败，见第 11 节。"
echo "  ③ 判断「两份文件一样吗」别用 wc -l：它数的是换行符。要比内容就用 cmp / wc -c。"
