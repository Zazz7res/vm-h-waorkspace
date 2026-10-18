#!/usr/bin/env bash
# ============================================================================
# mkweird.sh —— 在**当前目录**造出「名字里有空白字符」的文件，给 xargs 那两节用
#
#   demo.sh 里这样调：bash mkweird.sh
#   安全：只在当前目录（= demo 的沙箱 work/）里 mkdir/touch；不碰仓库、不碰 HOME。
#   名字里的换行符是用 bash 的 $'...\n...' 写出来的，所以这个文件必须是 bash 跑的。
# ============================================================================

rm -rf weird
mkdir -p weird

touch 'weird/plain.txt'                # 正常名字
touch 'weird/two words.txt'            # 名字里一个空格
touch $'weird/nl\nname.txt'            # 名字里一个真换行符
touch $'weird/tab\tname.txt'           # 名字里一个真 Tab

printf '造好了 4 个文件，字节名如下（每项一行，用 -print0 的 NUL 分隔再转成可见形式）：\n'
find weird -type f -print0 | tr '\0' '\n' | sed -e 's/^/  /'
printf '\n再用 sed -n l 打一遍（cat/sed 只会把 Tab 显示成 \\t；名字里那个换行符会被 sed 当成行分隔，\n所以一个文件被打印成两行 —— 这正是所有「按行读名字」的工具的死法）：\n'
find weird -type f | sed -n l

printf '\n★ 关键数字：文件有 4 个，但按行数字节名会得到几行？\n'
printf '  文件真实个数 = %s\n' "$(find weird -type f -print0 | tr -dc '\0' | wc -c)"
printf '  find 正常输出后 wc -l 行数 = %s   ← 名字里带换行的那个文件被算成两行\n' "$(find weird -type f | wc -l)"
