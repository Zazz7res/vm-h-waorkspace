#!/usr/bin/env bash
# ============================================================================
# bashonly.sh —— 只用 bash 独有语法的脚本
#
# 本机的 /bin/sh 是 **dash**（`ls -l /bin/sh` 会看到软链接）。
# 所以 `sh bashonly.sh` 会在第 1 行就炸掉 —— 这正是这一章要的现场。
#
# 结论记牢：shebang 写 `#!/bin/sh` 的时候，你**必须**只用 POSIX 语法。
# 想在 /bin/sh 下跑 bash 语法，等于赌「这台机器的 sh 是不是 bash」，
# 在 Debian/Ubuntu 上你必输。
# ============================================================================

# ① bash 数组：dash 看到 `arr=(` 直接语法错误
#    报错原文：bashonly.sh: 1: Syntax error: "(" unexpected
arr=(alpha beta gamma)
echo "  数组第 2 个元素 \${arr[1]} = ${arr[1]}"

# ② [[ ]]：它是 bash 的关键字，不是命令。dash 里 `[[: not found`
name=dash
if [[ "$name" == d* ]]; then
    echo "  [[ ]] 模式匹配生效：$name 以 d 开头"
fi

# ③ here-string `<<<`：dash 报 `Syntax error: redirection unexpected`
while IFS= read -r line; do
    echo "  从 <<< 里读到一行：[$line]"
done <<< "来自 here-string 的第一行
来自 here-string 的第二行"

# ④ $'...'（ANSI-C 引用）：反斜杠转义会被真展开成换行。
#    dash 里 `$'` 不是引用语法，`$` 会原样留在字符串里 —— 输出会变成 `$'第1行`。
ansi_text=$'第1行\n第2行'
echo "  \$'...' 的展开结果（下面应当是两行）："
echo "$ansi_text"

# ⑤ bash 独有的数组长度展开 ${#arr[@]}
path=/usr/local/bin:/usr/bin:/bin
echo "  \${path%%:*}  = ${path%%:*}"
echo "  \${#arr[@]}   = ${#arr[@]} 个元素（元素个数：配合数组才是 bash 独有）"

echo "  —— 以上 5 段全部打印成功，说明这个脚本确实跑在 bash 里，不是 dash。"
