#!/usr/bin/env bash
# ============================================================================
# readsteal.sh —— 循环体把「下一行输入」吃掉了（本章最经典的一次事故）
#
#   demo.sh 里这样调：bash readsteal.sh
#   安全：只在**当前目录**（= demo 的沙箱 work/）里建 steal/ 子目录，跑完由 trap 删掉。
#   故意不加 set -e：要的就是「循环只跑了一轮却退出码 0」那个安静的错。
#
# 症状：明明 4 行名单，循环只处理了第 1 行就完了，不报错。
# 原因：`done < 名单` 这个重定向作用在**整个 while 语句**上，循环体继承同一个 stdin fd，
#       于是体里任何一个「会读 stdin」的命令都跟 read 抢同一份数据，而且是**它先拿走**。
# 现实里最常见的肇事者：ssh（会读 stdin）、scp、mplayer/ffmpeg、gpg、mysql、以及最随手就写出来的 cat。
# ============================================================================

rm -rf steal
mkdir -p steal
trap 'rm -rf steal' EXIT

printf 'node-1\nnode-2\nnode-3\nnode-4\n' > steal/hosts.txt
printf '这份文件是正文，跟名单无关。\n'   > steal/notes.txt

echo "=== 1. 现场：4 行名单，体里有一句会读 stdin 的命令（这里用裸 cat 模拟 ssh） ==="
printf '  名单内容：%s\n' "$(tr '\n' ' ' < steal/hosts.txt)"
printf '  裸 cat 不带参数 = 从 stdin 读。它和 while read 用的是同一个 fd，这就是事故的全部原因。\n'

echo
echo "=== 2. 事故：循环只跑 1 轮，剩下 3 行被 cat 端走 ==="
n=0
while read -r host; do
    n=$((n + 1))
    printf '  第 %s 轮：host=[%s]，现在跑一句裸 cat（它会从 stdin 读）\n' "$n" "$host"
    cat
done < steal/hosts.txt
printf '  → 循环跑了 %s 轮（名单有 4 行），退出码 %s —— 不报错、不警告。\n' "$n" "$?"
echo "  → 上面那 3 行 node-2/node-3/node-4 是被 cat 打印出来的：它们**作为输入**被吃掉了，"
echo "    所以 read 下次去拿的时候已经到 EOF，循环自然结束。"

echo
echo "=== 3. 判据：把「谁读了 stdin」变成能数的东西 ==="
printf '  名单总行数 = %s\n' "$(wc -l < steal/hosts.txt)"
n=0
while read -r host; do
    n=$((n + 1)); cat > /dev/null       # 同一份事故，只是别把偷来的行打到屏幕上
done < steal/hosts.txt
printf '  坏写法跑到的轮数 = %s   ← 少了 %s 轮，这就是这条 bug 的「可数」形态\n' "$n" "$((4 - n))"
echo "  → 以后调试这类问题就问一句：**轮数对不对**？对不上就是有人吃了 stdin。"

echo
echo "=== 4. 修法一：给循环体喂 /dev/null（改动最小，最常用） ==="
n=0
while read -r host; do
    n=$((n + 1))
    cat < /dev/null                     # 真实场景：ssh -n host / ssh host cmd < /dev/null
done < steal/hosts.txt
printf '  轮数 = %s ｜ cat 这次读到 0 字节，名单保住了\n' "$n"
echo "  → ssh 有专门的开关（-n 就是把 stdin 接 /dev/null），比事后补 < /dev/null 更显眼。"

echo
echo "=== 5. 修法二：名单挂到 fd 3，read 从 3 读，stdin 留给循环体 ==="
n=0
while read -r host <&3; do
    n=$((n + 1))
    # 这一支里体外的命令可以自由读 stdin —— 我们给它一份真数据，证明「stdin 通了」
    got=$(cat)
    printf '  第 %s 轮：host=[%s] ｜ 体内的 cat 从 stdin 拿到 [%s]\n' "$n" "$host" "$got"
done 3< steal/hosts.txt < steal/notes.txt
printf '  轮数 = %s ｜ 这就是第 04 章第 23 节 exec 3< / read -u 3 那一招的循环版\n' "$n"
echo "  → 什么时候必须用它：你**真的**要在循环体里读别的 stdin（比如喂密码给 gpg）。"
echo "    否则修法一更简单：一个 fd 干一件事，别让人共读。"

echo
echo "=== 6. 修法三：先把名单读进数组，循环就再也不碰 stdin 了 ==="
mapfile -t hosts < steal/hosts.txt
n=0
for host in "${hosts[@]}"; do
    n=$((n + 1))
    cat > /dev/null 2>&1               # 体里随便读 stdin，都动不到名单：名单在内存里
done
printf '  轮数 = %s ｜ mapfile 一次读完，for 走数组 —— 顺便把第 12 节末行漏读的问题也一起解决了\n' "$n"
echo "  → 注意这里 for 的词表是**展开好的数组**（第 4 节讲的快照），所以体里改 stdin、改磁盘都不影响轮数。"

echo
echo "=== 7. 收口：三条能带走的话 ==="
echo "  ① 症状识别：while read 的循环**跑不满**、又不报错 → 先怀疑循环体吃了 stdin；"
echo "     肇事名单：ssh / scp / mplayer / ffmpeg / gpg / mysql / cat / read（体里再 read 一次也是）。"
echo "  ② 三档修法按侵入性排：< /dev/null（最省事）< fd 3 分离（要真读 stdin 时）< mapfile+for（最干净，但吃内存）。"
echo "  ③ 判据要能数：轮数、字节数、行数——任何「应该跑 N 次」的循环，都在屏幕上把 N 打出来，"
echo "     别靠肉眼看输出长度；这一章后面第 18、21 节的后台作业对账用的是同一招。"
