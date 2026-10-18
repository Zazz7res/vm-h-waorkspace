#!/usr/bin/env bash
# ============================================================================
# argpass.sh —— 函数参数从哪来：一进来就接管位置参数，"$*"/"$@"/引号各什么后果
#
#   demo.sh 里这样调：bash argpass.sh
#   主线：函数体的 $1 $2 $# $@ 是**调用时传进来的**那一组，脚本级的位置参数在函数里整个
#   被换掉（不是被遮蔽、是看不见）；返回后外层的又原样回来。传参时引号决定「几个词」，
#   不带引号还会触发通配展开 —— 名字接不住值，词边界全在引号里。
#   安全：只在**当前目录**建 ap/ 子目录做通配实验（造两个能被 * 匹配的文件），跑完由
#         trap 'rm -rf ap' EXIT 删掉；不碰 ~、不碰绝对 /tmp、不联网、不开端口。
#   故意不加 set -e：本支要看「带星号那条到底炸成几个词」，非 0 也要继续跑对照组。
#   ★ 本机不拿什么当证据：ap/ 里只有我固定的两个文件，* 匹配结果确定，不依赖 cwd 别的
#     东西；所有计数用 $#、"$@" 逐元素，都是每次一样的量，不量耗时、不打 pid。
#   ★ 通配展开 / 拆词的差异要**在 bash 里跑**才算数（zsh 对不带引号的 $v 不拆词），
#     本脚本的 shebang 与 demo 的 `bash argpass.sh` 都钉死用 bash。
# ============================================================================

rm -rf ap
mkdir -p ap
trap 'rm -rf ap' EXIT

echo '=== 1. 函数一进来，看到的 $# 是传进来的个数，脚本级 $@ 不见了 ==='
set -- 脚本甲 脚本乙 脚本丙
printf '  脚本级：$#=%s  $1=[%s]  "$*"=[%s]\n' "$#" "$1" "$*"
show() { printf '    函数里：$#=%s  $1=[%s]  $2=[%s]\n' "$#" "${1-（无）}" "${2-（无）}"; }
printf '  调用 show 传两个：\n'; show x y
printf '  调用 show 一个都不传（★ 函数里看到的是 0，不是脚本级那 3 个）：\n'; show
printf '  回到脚本级：$#=%s  $1=[%s]（一点没被 show 影响）\n' "$#" "$1"
echo '  → ★ 位置参数是「每次调用临时换上去的一组」：函数体里读到的 $1..$# 永远是这次的，脚本级那组进不来。'

echo
echo '=== 2. "$*" 拼成一行 vs "$@" 分成多个词（默认 IFS，逐字贴）==='
f2() {
    local joined="$*"
    printf '    "$*"=[%s]（%s 个字符，是 1 个词）\n' "$joined" "${#joined}"
    printf '    "$@" 逐元素：'
    local n=0
    for w in "$@"; do printf '[%s] ' "$w"; n=$((n + 1)); done
    printf '  ← 共 %s 个元素\n' "$n"
}
printf '  用 f2 "p q" r s 调用（第一个词自带空格）：\n'
f2 "p q" r s
echo '  → "$*" 把所有参数用空格粘成 1 个词；"$@" 保持每个参数各自一个词（含它自带的空格）。'

echo
echo '=== 3. IFS 改成 - 再各演一次（只有 "$*" 受影响）==='
f3() { printf '    "$*"=[%s]   "$@" 元素数=%s\n' "$*" "$#"; for w in "$@"; do printf '      [%s]\n' "$w"; done; }
printf '  IFS=- 后 f3 "p q" r s（在子 shell 里只改这一次的 IFS）：\n'
( IFS=-; f3 "p q" r s )
echo '  → ★ "$*" 用当前 IFS 的第一个字符当粘合剂（这里变成 -）；"$@" 不受 IFS 影响，元素照旧分开。'

echo
echo '=== 4. 函数里写 set -- 新值，会不会改写外层的位置参数？==='
set -- 外层甲 外层乙
printf '  外层调用前：$#=%s  "$*"=[%s]\n' "$#" "$*"
changer() { set -- 新1 新2 新3 新4; printf '    函数里 set -- 之后：$#=%s  "$*"=[%s]\n' "$#" "$*"; }
changer
printf '  外层还剩几个 → $#=%s  "$*"=[%s]（★ 返回时 bash 把外层的还原回来了）\n' "$#" "$*"
echo '  → set -- 在函数里只动这一层的位置参数；返回后 bash 自动恢复调用前那组，外层不脏。'

echo
echo '=== 5. 三种传参写法的真实后果（通配实验在 ap/ 里做）==='
: > ap/one.txt; : > ap/two.txt
collect() { printf '    收到 %s 个词：' "$#"; for w in "$@"; do printf '[%s] ' "$w"; done; printf '\n'; }
v="ap/one.txt ap/two.txt ap/*.txt"
printf '  造了 ap/one.txt ap/two.txt；令 v="ap/one.txt ap/two.txt ap/*.txt"\n'
printf '  ① 不带引号 collect $v（词分割 + 通配展开同时发生）：\n'
collect $v
printf '  ② 带引号 collect "$v"（正解：1 个词，星号原样不展开）：\n'
collect "$v"
arr=("两个 词" 单独 又一个)
printf '  ③ 数组整个传 collect "${arr[@]}"（每个元素一个词，含空格那个不被切）：\n'
collect "${arr[@]}"
printf '     对照不带引号 collect ${arr[@]}（★ 空格又被二次切开，元素数变了）：\n'
collect ${arr[@]}
echo '  → 不带引号会先按 IFS 拆词、再对每段做通配展开：v 本是 3 个源词，第三段 ap/*.txt 被展成两个真文件，'
echo '    一个词顶替成两个，于是实测收到 4 个词（比你要传的还多）。传参默认就该 "$var"；传数组用 "${arr[@]}"。'

echo
echo '=== 6. 名字接不住值：格数差别用 $# 计数 ==='
x="hello world there"
count() { printf '    收到 %s 个参数\n' "$#"; }
printf '  x="hello world there"（中间两个空格）\n'
printf '  count $x   →'; count $x
printf '  count "$x" →'; count "$x"
echo '  → 同一个名字、同一个值：不带引号被拆成 3 个，带引号还是 1 个。词边界不是「值」决定，是引号决定。'

echo
echo '=== 7. 收口 ==='
printf '  删之前 ap/ 里有 %s 个文件 ｜ 现在 rm -rf ap ｜ 之后名为 ap 的条目数=%s\n' \
    "$(ls -A ap | wc -l)" "$(rm -rf ap; ls -A | grep -cx ap)"
printf '  自证：本脚本作业表 jobs -p 行数=%s（全程没用过 &）\n' "$(jobs -p | wc -l)"
echo '  ① 函数体里的 $1..$# 永远是这次调用传进来的一组；脚本级位置参数在函数里看不见，返回后原样回来。'
echo '  ② "$*" 粘成一行（受 IFS 影响）、"$@" 保持每参数一个词（不受 IFS 影响）—— 转发参数一律用 "$@"。'
echo '  ③ 传参带引号 "$var"、传数组 "${arr[@]}"；不带引号会同时触发拆词和通配展开，值里有空格或 * 就坏。'
