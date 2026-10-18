#!/usr/bin/env bash
# ============================================================================
# rcprobe.sh —— 探测「我现在这个 shell 里有没有 alias / 有没有 rc 里设的 shopt」
#
# 这个文件本身**故意什么都不配**：不 source 任何 rc，不设 alias。
# 它只是把当前 shell 的状态报出来。于是同一份文件：
#
#   bash rcprobe.sh                     → 非交互：alias 表空的，expand_aliases=off
#   bash --rcfile 沙箱rc -ic 'source rcprobe.sh'  → rc 生效：alias 有了、开关变了
#
# 差别的原因：**非交互 shell 根本不读 rc 文件**
#   * 交互式非登录 shell：读 /etc/bash.bashrc + ~/.bashrc
#   * 非交互 shell：两个都不读，只认环境变量（以及 BASH_ENV 指定的那一个文件）
# 所以「我在终端里配好的 alias，写进脚本就不能用了」不是 bug，是规则。
# ============================================================================

echo "  \$-            = [$-]          ← 含 i 才是交互式（这里 hB / hiBHc 的差别就是它）"
echo "  \$0            = [$0]"

# alias 不带参数时打印当前所有别名。非交互 shell 里通常是 0 条。
echo "  当前 alias 条数 = [$(alias | wc -l)]"
if alias mytestalias >/dev/null 2>&1; then
    echo "  alias mytestalias 存在 → [$(alias mytestalias)]"
else
    echo "  alias mytestalias 不存在（rc 没生效）"
fi

# 判开关状态的**唯一可靠写法**是 shopt -q（它只返回退出码，0=on 1=off）。
# 坑在这里：`shopt -p 名字` 虽然会打印出 `shopt -s/-u 名字`，
# 但它的**退出码跟着开关状态走** —— 开关是 off 时它返回 1，
# 于是 `shopt -p x || echo 备用` 这种写法会在「开关正常为 off」时也走备用分支。
# （本机 bash 5.3 实测：非交互下 shopt -p expand_aliases 打印正常、退出码却是 1。）
for o in expand_aliases nullglob extglob cdable_vars histappend; do
    if shopt -q "$o"; then state=on; else state=off; fi
    printf '  shopt %-15s = %s\n' "$o" "$state"
done

# BASH_ENV 是非交互 shell **唯一**会主动读的启动文件（没设就什么都不读）
echo "  BASH_ENV       = [${BASH_ENV-（没设）}]"
echo "  本机 PATH 里第一个 bash = [$(command -v bash)]   ← shebang 写 env bash 时找到的就是它"
