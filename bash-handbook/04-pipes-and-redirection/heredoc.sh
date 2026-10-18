#!/usr/bin/env bash
# ============================================================================
# heredoc.sh —— 第 04 章 here-doc 取证脚本（只讲 << / <<'..' / <<- 这三兄弟）
#
#   demo.sh 里这样调它：bash heredoc.sh
#   为什么单独做一个文件，而不写在 demo.sh 的 runc '...' 里？
#     ① `<<-` 只剥 **Tab**、不剥空格 —— 这个证据必须是「源码里真实的 Tab 字符」，
#        塞进 demo.sh 的单行字符串里很容易被编辑/复制粘贴把 Tab 换成空格，证据就假了。
#        放在独立文件里，Tab 是本文件自己的字节，谁也改不了（下面还会用 cat -A
#        把这几行源码原样打出来，^I 就是 Tab）。
#     ② here-doc 的终止符如果写错（比如前面多了空格），bash 会一路吞到**文件结尾**
#        并报警告。这种「吞掉后面全部内容」的现场只能待在独立文件里，
#        绝不能出现在 demo.sh 主体 —— 否则整章脚本直接被它吃干净。
#
#   本文件只在**当前目录**（demo 的沙箱 work/）写文件，不碰别处。
# ============================================================================

printf '== A. <<EOF（定界符不加引号）：正文里的 $变量、$(命令) 全都会展开 ==\n'
WHO=张三
cat <<EOF
这一行是真的 here-doc 正文：WHO=[$WHO]
命令替换也展开：年份字段 [$(date +%Y)]
变量紧贴普通字符：[$WHO的] —— $WHO 后面直接跟汉字，仍然被认成变量
EOF

printf '\n== B. <<'"'"'EOF'"'"'（定界符加引号）：一个字都不展开 ==\n'
cat <<'EOF'
同样两行字：WHO=[$WHO]  命令替换 [$(date +%Y)]  算术 [$((1+1))]
↑ 这一段在屏幕上必须原样出现，因为定界符带了单引号
EOF

printf '\n== C. <<- ：只剥每行行首的 Tab，空格一个都不剥 ==\n'
printf '   先看清源码里到底放了什么（本文件这一段，cat -A 里 ^I 就是 Tab，行尾 $ 是 cat -A 加的）：\n'
sed -n '/^cat <<-INDENT$/,/^INDENT$/p' heredoc.sh | cat -A
printf '   同一段正文，先用 <<INDENT（不带短横）跑一次 → Tab 原样留在输出里：\n'
cat <<INDENT | cat -A
	TabLine
  SpaceLine
	TabLine2
INDENT
printf '   再用 <<-INDENT 跑一次 → 行首 Tab 被剥掉，两个空格还在（SpaceLine 前面仍有两个空格）：\n'
cat <<-INDENT | cat -A
	TabLine
  SpaceLine
	TabLine2
INDENT

printf '\n== D. 反斜杠在两种定界符下的差别 ==\n'
cat <<EOF
不加引号的 here-doc 里 \$WHO 被转义成字面量：\$WHO，而 $WHO 照常展开
EOF
cat <<'EOF'
加引号的 here-doc 里连反斜杠都不特殊：\$WHO 原样两个字符都留下
EOF

printf '\n== E. 终止符前面多一个空格 → bash 一路吞到结尾（真实警告原文） ==\n'
# 用「喂给一个子 bash」的方式跑这个事故，避免吃掉本文件后面的内容
printf 'cat <<EOF\n被吞掉的正文\nEOF \n下一行也被吞\n' | bash
printf '   ↑ 子 bash 的退出码=[%s]：它把整段都当成 here-doc 正文，最后一行也没能被当命令执行\n' "$?"

printf '\n== F. here-doc 可以喂给任何读 stdin 的命令，包括 bash 自己 ==\n'
bash <<'INNER'
echo "这段代码是从 stdin 喂给一个新的 bash 的"
echo "它的 \$0=[$0]  \$#=[$#]（命令行上一个参数都没有）"
INNER
printf '   内层 bash 的退出码=[%s]\n' "$?"

printf '\n== G. here-doc 站在管道中间：它是「喂 stdin」，不是「当参数」 ==\n'
printf '   上游是 tr，here-doc 提供它的输入：\n'
tr a-z A-Z <<LOWER
这行本来是小写英文字母 abc
LOWER
printf '   here-doc 的内容也能同时进文件和下游（tee 接住）：\n'
tee hd_tee.txt <<LOWER2
一份数据两条路
LOWER2
printf '   tee 落到文件里的那份：\n'
cat hd_tee.txt

printf '\n[heredoc.sh 跑完] 只在当前目录留了 hd_tee.txt，没碰别处\n'
