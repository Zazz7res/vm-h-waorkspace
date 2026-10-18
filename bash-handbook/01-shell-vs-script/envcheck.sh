#!/usr/bin/env bash
# ============================================================================
# envcheck.sh —— shell 变量 vs 环境变量：中间隔着一次 fork
#
# 铁律：**只有被 export 过的变量，才会进入子进程的环境。**
# 「shell 变量」和「环境变量」是两个东西，export 是它们之间唯一的桥。
# 反向也成立：子进程改自己的环境变量，**永远传不回父进程**
# （除非它把值打印出来，父进程用命令替换接住 —— 见 subshell.sh 来源 ③）。
# ============================================================================

PRIVATE_VAR=只在bash里
PUBLIC_VAR=会被继承
export PUBLIC_VAR

echo "  本层看到：PRIVATE_VAR=[$PRIVATE_VAR]  PUBLIC_VAR=[$PUBLIC_VAR]"
echo "  export -p（列出所有环境变量）里能不能找到它们两个："
export -p | grep -E ' (PRIVATE_VAR|PUBLIC_VAR)=' || echo "    grep 没匹配到 PRIVATE_VAR —— 因为它不是环境变量"

echo "  ---- 往下开一个子 bash，只看环境 ----"
bash -c 'echo "    子进程看到：PRIVATE_VAR=[$PRIVATE_VAR]  PUBLIC_VAR=[$PUBLIC_VAR]"'

echo "  ---- 子进程里改 PUBLIC_VAR，能传回来吗 ----"
bash -c 'PUBLIC_VAR=子进程改的; echo "    子进程里现在是 [$PUBLIC_VAR]"'
echo "    父进程这边还是：PUBLIC_VAR=[$PUBLIC_VAR]   ← 传不回来"

echo "  ---- 变量前缀写法：只给「这一条命令」加环境变量 ----"
PUBLIC_VAR=只给这一条命令 bash -c 'echo "    那条命令看到：PUBLIC_VAR=[$PUBLIC_VAR]"'
echo "    前缀写法之后，父进程：PUBLIC_VAR=[$PUBLIC_VAR]   ← 也只影响那一条命令"

echo "  ---- printenv 看单个变量到底在不在环境里 ----"
if printenv PRIVATE_VAR >/dev/null 2>&1; then
    echo "    printenv 找到了 PRIVATE_VAR"
else
    echo "    printenv PRIVATE_VAR → 找不到（退出码非 0），它真的不在环境里"
fi
printenv PUBLIC_VAR | sed 's/^/    printenv PUBLIC_VAR → /'

echo "  ---- env -i：把环境清空，看看还剩什么 ----"
echo "    我这个 shell 的环境变量条数：$(env | wc -l)"
env -i /bin/bash -c 'echo "    env -i 之后 PATH=[$PATH]"'
env -i /bin/bash -c 'echo "    env -i 之后 PUBLIC_VAR=[$PUBLIC_VAR]"'
env -i /bin/bash -c 'echo "    env -i 之后 BASH_VERSION=[$BASH_VERSION]"'
note2() { printf '%s\n' "$1"; }
note2 '    三条合起来的结论：PATH 是 bash **自己造的默认值**（不是继承来的，env -i 里根本没有 PATH），'
note2 '    PUBLIC_VAR 消失（证明继承只走环境），BASH_VERSION 还在（它是 bash 启动后自己设的 shell 变量，'
note2 '    不在环境里 —— 用 printenv BASH_VERSION 是查不到的）。'
printenv BASH_VERSION >/dev/null 2>&1 || note2 '    现场验证：printenv BASH_VERSION → 查不到，它确实不在环境里。'
