#!/usr/bin/env bash
# ============================================================================
# herestring.sh —— 第 04 章 here-string（<<<）取证脚本
#
#   demo.sh 里这样调它：bash herestring.sh
#   <<< 是 bash 的简写：「把这一个词的内容当作对方的 stdin」。
#   和 here-doc 的区别就在「一个词」这四个字上 —— 本文件把这条限制跑出证据。
#   只在当前目录（demo 的沙箱）写文件。
# ============================================================================

printf '== A. <<< 后面只接「一个词」：第二个词掉下去变成了命令行参数 ==\n'
printf '   跑 cat <<< 一 二 三 —— 只有「一」进了 stdin，「二」「三」成了 cat 要打开的文件名：\n'
cat <<< 一 二 三
printf '   ↑ 注意 cat 连「一」都没打印：一旦有文件名参数，它就压根不读 stdin 了。\n'

printf '\n== B. 末尾那个换行是 bash 补的 ==\n'
printf '   源字符串里没有换行，wc -l 却数出：'
wc -l <<< "只有一行，没有结尾换行"
printf '   wc -c（含换行的字节数）：'
wc -c <<< "abc"

printf '\n== C. <<< 的内容是字符串，不参与通配展开 ==\n'
printf '   当前目录里有 .txt 文件（列两个给你看：%s），但 <<< 里的星号原样输出：\n' "$(ls *.txt | head -2 | tr '\n' ' ')"
cat <<< '*/*.txt'
printf '   换成 here-doc 也一样不展开成文件名（但变量会展开）：\n'
cat <<STAR
*/*.txt
STAR

printf '\n== D. 想要多行就得自己造真换行 ==\n'
printf '   双引号里的 %s 不是换行（反斜杠是字面字符），所以 wc -l 只数出 1：\n' '"行1\n行2"'
wc -l <<< "行1\n行2"
printf '   换成 bash 的 $'"'"'...'"'"' 语法才是真换行，wc -l 数出 2：\n'
wc -l <<< $'行1\n行2'
printf '   或者干脆用命令替换造出来（这里喂的是三行）：\n'
cat <<< "$(printf '甲\n乙\n丙')"

printf '\n== E. <<< 和管道不是一回事：左边没有命令，只有一个字符串 ==\n'
printf '   管道版（左边真起了一个 echo 进程）：\n'
printf '   ' ; echo from-echo | cat
printf '   <<< 版（没有左边进程，bash 自己把字符串塞进 stdin）：\n'
cat <<< from-here-string

printf '\n== F. 变量里存着一段文本时，<<< 比 here-doc 方便 ==\n'
TEXT='第一行内容
第二行内容'
grep -c '' <<< "$TEXT"
printf '   grep -c "" 数出的行数就是上面那个变量真实的行数（这里必须是真换行才行）\n'

printf '\n[herestring.sh 跑完] 本目录没多写任何文件\n'
