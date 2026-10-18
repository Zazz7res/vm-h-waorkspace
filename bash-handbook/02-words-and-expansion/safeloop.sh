#!/usr/bin/env bash
# ============================================================================
# safeloop.sh —— 遍历「文件名」时的四种写法对照（本脚本只读文件名，不写不删）
#
#   用法：bash safeloop.sh <要遍历的目录>
#   demo.sh 传的是沙箱里的 files/，那里有三个恶心但完全合法的文件名：
#       my file.txt   （中间有空格）
#       we*ird.txt    （带 glob 特殊字符）
#       aa.txt        （正常）
#   四种写法的差别全部体现在「程序拿到几个词」上，一行报错都没有才是最危险的。
# ============================================================================

# seg.sh 和这个脚本放在同一个目录里（沙箱的 work/），但我们要 cd 进目标目录去数文件，
# 所以先记住自己所在的目录，用绝对路径调 seg.sh —— 注意：这个路径只用于调用，不会被打印。
MYPATH=$(cd "$(dirname "$0")" && pwd)
cd "${1:-.}" || exit 1

printf '目录里的文件（ls 一行一个）：\n'
ls -1
printf '\n== 写法 A（坏）：先用 ls 塞进变量，再裸用 $files ==\n'
files=$(ls)
for f in $files; do
    printf '  一个词=[%s]' "$f"
    if [ -e "$f" ]; then printf '   存在\n'; else printf '   不存在（名字被切坏了）\n'; fi
done

printf '\n== 写法 B（好）：glob 直接写在 for 后面，一个词一个文件 ==\n'
for f in *.txt; do
    printf '  一个文件=[%s]\n' "$f"
done

printf '\n== 写法 C（好）：先收进数组，再用 "${arr[@]}" 展开 ==\n'
list=( *.txt )
printf '  数组里的元素个数 -> %d（这一格是用 ${#list[@]} 数出来的）\n' "${#list[@]}"
for f in "${list[@]}"; do
    printf '  一个文件=[%s]\n' "$f"
done

printf '\n== 写法 C 的下游验证：把数组原样转发给 seg.sh ==\n'
bash "$MYPATH/seg.sh" "${list[@]}"

printf '\n== 写法 D（好，要跨命令传清单时）：find -print0 + IFS= read -r -d "" ==\n'
find . -maxdepth 1 -name '*.txt' -print0 | while IFS= read -r -d '' f; do
    printf '  一条=[%s]\n' "${f#./}"
done
