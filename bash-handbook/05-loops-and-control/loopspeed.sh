#!/usr/bin/env bash
# ============================================================================
# loopspeed.sh —— 循环贵在哪里：词表是「现算」还是「先摊开」，循环体是不是每次都 fork
#
#   demo.sh 里这样调：bash loopspeed.sh
#   安全：只在**当前目录**（= demo 的沙箱 work/）里建 speed/ 子目录，跑完由 trap 删掉。
#   故意不加 set -e：第 5 段要拿 `set -u` 下空数组的报错原文当证据。
#
# ★ 本脚本刻意**不印毫秒数**（耗时每跑必变，印出来就是一张每次都对不上的表）。
#   能复现的只有两件事：词表被摊开成几个元素，和「循环体那一轮要不要 fork 一个进程」——
#   后者用 type -t / command -v 直接问出来，是确定答案，不用掐表。
# ============================================================================

rm -rf speed
mkdir -p speed
trap 'rm -rf speed' EXIT

N=2000

echo "=== 1. 三种 for：词表是被「摊开成一排」还是「每轮现算」 ==="
arr1=({1..2000})
printf '  for i in {1..2000}     → 进循环前就摊开成 %s 个词（数组长度实测，不是估算）\n' "${#arr1[@]}"
arrb=({1..$N})
printf '  ★ 但 {1..%s} 写成 {1..$N} 就废了：摊开成 %s 个词，第一个词是 [%s]（花括号展开发生在变量替换**之前**，' \
    "$N" "${#arrb[@]}" "${arrb[0]}"
printf '    它压根不认识 $N）→ 这一条是第 02 章「展开有固定顺序」在循环里的现场。\n'
arr2=($(seq 1 "$N"))
printf '  for i in $(seq 1 %s)   → 先起一个 seq 进程，再把它 %s 行输出摊成 %s 个词\n' "$N" "$N" "${#arr2[@]}"
printf '  for ((i=1;i<=%s;i++))  → 没有词表这回事，每轮现算一个数（所以它给不出上面那种数组长度）\n' "$N"
i=0
for ((x = 1; x <= N; x++)); do i=$((i + 1)); done
printf '  C 风格那一支实测跑了 %s 轮，最后一轮 x=%s（★ 循环变量出了循环还活着，见第 3 节）\n' "$i" "$x"
echo "  → 为什么要紧：前两种写法把 N 个元素**同时**握在内存里（N=2000 无所谓，"
echo "    N=2000000 或者元素是长路径时就很难看）；for (( )) 一个都不存。"

echo
echo "=== 2. 循环体贵不贵，取决于它是内建还是外部命令（这里不掐表，直接问） ==="
for c in : true echo printf cd wait mapfile; do
    printf '    %-8s type -t → %s\n' "$c" "$(type -t "$c")"
done
printf '  /bin/true 这种带路径的：command -v → %s ｜ type -t → %s\n' "$(command -v /bin/true)" "$(type -t /bin/true)"
echo "  → 判据一句话：**type -t 给出 builtin 的，循环一百万轮也不 fork；给出 file 的，每一轮都 fork+exec 一次。**"
printf '  现场量一下 fork 的次数（办法：让循环体自己报告 pid，用 BASHPID 而不是 $$）：\n'
: > speed/pids_builtin.txt
for i in $(seq 1 50); do printf '%s\n' "$BASHPID" >> speed/pids_builtin.txt; done
: > speed/pids_external.txt
for i in $(seq 1 50); do /bin/bash -c 'printf "%s\n" "$BASHPID"' >> speed/pids_external.txt; done
printf '    循环体全是内建（printf）  → 出现的不同 pid 个数=%s\n' "$(sort -u speed/pids_builtin.txt | wc -l)"
printf '    循环体每轮起一个外部 bash → 出现的不同 pid 个数=%s（★ 每个 pid 就是一次 fork+exec）\n' "$(sort -u speed/pids_external.txt | wc -l)"

echo
echo "=== 3. 管道版为什么慢一整截：它把循环体放进了另一个进程 ==="
echo '  判据用 $BASHPID（本进程真实 pid），不要用 $$（它在子 shell 里仍然等于父进程的 pid）。'
if [ "$BASHPID" = "$$" ]; then
    echo '    写法 A：for i in $(seq 1 50)　→ 循环体里 [ "$BASHPID" = "$$" ] 成立，循环体就在**本进程**里跑'
else
    echo '    写法 A：for i in $(seq 1 50)　→ ★ 反了，这里 BASHPID ≠ $$，说明前面那个 $(seq) 把事搅了'
fi
seq 50 | {
    if [ "$BASHPID" = "$$" ]; then
        echo '    写法 B：seq 1 50 | while read i → 不该出现这一行'
    else
        echo '    写法 B：seq 1 50 | while read i　→ 循环体里 [ "$BASHPID" = "$$" ] **不成立**，它在另一个进程里'
    fi
}
echo '    ★ 自己踩过的坑：pa=$(BASHPID) 想拿主 shell 的 pid，结果命令替换**本身就 fork 了一个子进程**，'
echo '      量到的是那个 subprocess 的 pid（和 $$ 永远不等）。凡是用 $() 去问「我在哪个进程」，问的都是别人。'
echo "  → 这条和第 04 章「管道每一段都是自己的进程」是同一条规矩，代价在这里加了一层：进程之间只能通过那根管道说话。"
echo '    循环体里赋值、开数组、cd，主 shell 一个都收不到 —— 第 14 节的 lastpipe 三接回就是为了治这个。'

echo
echo "=== 4. 逐行读文件 vs 一次读全：两种「循环」的内存形状（元素个数可复现） ==="
big=speed/big.txt
awk 'BEGIN{for(i=1;i<=3000;i++) printf "第%d行\n", i}' > "$big"
printf '  文件行数=%s ｜ 字节数=%s\n' "$(wc -l < "$big")" "$(wc -c < "$big")"
mapfile -t allrows < "$big"
printf '  mapfile 一次读全 → 数组元素个数=%s（★ 全部行同时在内存里）\n' "${#allrows[@]}"
one=""
n=0
while read -r one; do n=$((n + 1)); done < "$big"
printf '  while read 流式   → 循环跑了 %s 轮，任意时刻内存里只有 %s 行（变量 one 现在装的是最后读到的：[%s]）\n' \
    "$n" "1" "$one"
echo "  → 选谁不看哪个代码短，看**规模**：几千行随便 mapfile；日志级的大小就流式读，"
echo "    并且带上第 12 段的 || [ -n \"\$REPLY\" ] 和第 13 段的 < /dev/null。"

echo
echo "=== 5. 顺手把 set -u 撞一次（第 03 章那条规矩在这里的样子） ==="
printf '  空数组 + set -u，老写法：'; bash -uc 'arr=(); for x in "${arr[@]}"; do :; done; echo "跑完没报错（bash 5.x 修好了）"' 2>&1 | tail -1
printf '  空数组 + set -u，展开成标量：'; bash -uc 'arr=(); echo "[${arr[*]}]"' 2>&1 | tail -1
echo "  → 本机 bash 5.3 这两条都不报错；老版本（4.3 及更早）第一条就 unbound variable。"
echo "    结论别写成「空数组安全」，要写成「**取决于版本**」，并把这一条挂到你 CI 的那个 bash 上验一次。"

echo
echo "=== 6. 收口：三句话 ==="
echo "  ① 循环体的成本用 type -t 问，不用掐表：builtin = 一百万轮也不 fork；file = 每轮一次 fork+exec。"
echo "     慢的脚本几乎都是「循环里放了外部命令」，而不是「循环本身慢」。"
echo "  ② 词表形态决定内存形状：{a..b} 与 \$(seq) 都是**先摊开一排词**（第 4 节讲的快照），for (( )) 每轮现算。"
echo "     量小的时候随便挑，量大的时候（百万级、或者元素是很长的路径）只有 for (( )) 顶得住。"
echo "  ③ 计时数字不进证据：本章所有「快多少」都换成可复现的计数（元素个数、不同 pid 个数、轮数），"
echo "     因为毫秒每次跑都不一样，README 里印了就永远对不上。"
