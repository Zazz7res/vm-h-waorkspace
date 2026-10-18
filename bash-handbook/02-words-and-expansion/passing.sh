#!/usr/bin/env bash
# ============================================================================
# passing.sh —— 同一个值在四种写法下的「传参变形记」
#
#   观测工具全是 seg.sh（它只报告「bash 递给我几个词」）。
#   这一段把 09 节的分词规则和 11 节的 "$@" 规则串成一条链：
#   你写的是同一个变量，程序可能拿到 1、2、3 个词，而且全程没有任何报错。
# ============================================================================

v="my file.txt"
printf '原始值：v=[%s]（里面有一个空格，文件名在真实世界里完全合法）\n\n' "$v"

printf '① bash seg.sh $v     —— 裸用，会被切\n'
bash seg.sh $v

printf '\n② bash seg.sh "$v"  —— 整体加引号，一个词\n'
bash seg.sh "$v"

printf '\n③ bash seg.sh ${v}  —— 只加花括号不加引号：和 ① 完全一样\n'
bash seg.sh ${v}

printf '\n④ bash seg.sh "${v}" —— 花括号 + 引号：和 ② 完全一样\n'
bash seg.sh "${v}"

printf '\n⑤ 中间转一手：set -- $v 把它切成参数，再用 "$@" 原样转发\n'
set -- $v
printf '   set -- 之后参数个数 = %s（切成了几段就固定成几段了）\n' "$#"
bash seg.sh "$@"

printf '\n⑥ 命令替换也一样：替换结果先分词，再进命令行\n'
bash seg.sh $(printf 'a b')
bash seg.sh "$(printf 'a b')"

printf '\n⑦ 想要「一个词」就加引号，想要「多个词」才裸写 —— 反过来越界就会出事：\n'
bash seg.sh "${v% *}" "${v##* }"
