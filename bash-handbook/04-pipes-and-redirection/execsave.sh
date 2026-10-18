#!/usr/bin/env bash
# ============================================================================
# execsave.sh —— exec 改的是**这个 shell 自己的** fd：永久重定向、保存、恢复
#
#   demo.sh 里这样调：bash execsave.sh
#   为什么单独做一个文件？因为 `exec >文件` 会把这个 shell 之后所有 stdout 都带走，
#   写在 demo.sh 主体里就等于把整章的屏幕输出灌进文件；放在独立文件里，
#   它只影响这个子进程，跑完就结束。
#   本文件只在当前目录（demo 的沙箱）写 exec_*.txt。
# ============================================================================

printf '第 1 步：此刻 1 号 fd 还指向调用方（屏幕），所以这行你看得见。\n'

printf '\n第 2 步：先把 1 号 dup 到 9 号留底，再 exec >exec_log.txt —— 从这一瞬间起，\n'
printf '        本 shell 后面所有 stdout 都写进文件了。\n'
exec 9>&1
exec >exec_log.txt
echo '这句在 exec_log.txt 里，屏幕上看不见'
echo '这句也看不见，连 date 的输出也进了文件：'"$(date '+%Y-%m-%d %H:%M')"
echo '但 stderr 没被动过 —— 下面这行是 >&2 打的，所以它还上屏' >&2

printf '\n第 3 步：用留底的 9 号把 1 号接回来：exec 1>&9 9>&-\n' 1>&2
exec 1>&9
exec 9>&-
echo '这句又上屏了 —— 说明 1 号 fd 真的换回去了'

printf '\n第 4 步：再看一次「没留底就关档」的后果（放在子 shell 里，不炸本脚本）：\n'
(
    exec 1>&-                     # 关掉 stdout，且没有留底
    echo '这句试着写到已经关掉的 1 号 fd'
    echo '↑ 上面那句报错了；我这句走 2 号 fd，还能上屏' >&2
)
printf '   子 shell 的退出码=[%s]\n' "$?"

printf '\n第 5 步：文件里到底落了什么：\n'
printf -- '--- exec_log.txt (%s 行) ---\n' "$(wc -l <exec_log.txt)"
cat exec_log.txt
printf -- '--- 本目录留下的文件 ---\n'
ls exec_*.txt
