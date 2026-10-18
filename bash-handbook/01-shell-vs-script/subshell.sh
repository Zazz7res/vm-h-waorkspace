#!/usr/bin/env bash
# ============================================================================
# subshell.sh —— 子 shell 的三个来源，和「管道里改的变量外面看不到」这个经典坑
#
# 判断是不是子 shell 的唯一硬证据是**进程号**。
# 但注意：bash 里 `$$` 在子 shell 中**保持不变**（它永远是当初那个 shell 的 PID），
# 只有 `$BASHPID` 会真的变。很多人用 `$$` 判断，于是永远看不出子 shell。
# ============================================================================

echo "  顶层：\$\$=$$  \$BASHPID=$BASHPID"

# --- 来源 ①：小括号 ( ) ---------------------------------------------------
# 括号里是一个完整的子 shell：它的赋值、cd、trap 全都出不去。
(
    echo "  ① ( ) 里面：\$\$=$$  \$BASHPID=$BASHPID   ← \$\$ 没变，BASHPID 变了"
    INNER_VAR=只在括号里存在
    echo "  ① 括号里看 INNER_VAR = [$INNER_VAR]"
)
echo "  ① 括号外看 INNER_VAR = [${INNER_VAR-（没定义：括号里赋的值出不来）}]"

# --- 来源 ②：管道的**右侧** -------------------------------------------------
# 管道的每一段都是子 shell（bash 默认如此；这是新手第一大坑）。
# 用 `read` 从管道里数行数，最外面永远数是 0。
COUNT=0
printf 'apple\nbanana\ncherry\n' | while IFS= read -r fruit; do
    COUNT=$((COUNT + 1))
    echo "  ② 管道右侧数到第 $COUNT 个：$fruit   （\$BASHPID=$BASHPID）"
done
echo "  ② 管道结束后外面的 COUNT = [$COUNT]   ← 坑：刚才明明加到 3 了！"

# 同一个逻辑用「不在管道里」的写法，COUNT 就能留住（here-string 是 02/04 章的内容，
# 这里先让你看到两种写法的差别）
COUNT2=0
for fruit in apple banana cherry; do
    COUNT2=$((COUNT2 + 1))
done
echo "  ② 改用 for 循环（没有管道）外面的 COUNT2 = [$COUNT2]"

# --- 来源 ③：命令替换 $( ) ---------------------------------------------------
# 赋值号右边的 `$( )` 也是一个子 shell：里面赋值，外面看不见；
# 但里面 echo 出来的**文本**会通过替换回到外面 —— 这就是「只能靠输出传值」。
OUTER=原来的值
RESULT=$(OUTER=被改过了; printf '%s' "$OUTER")
echo "  ③ 命令替换里 echo 出来的文本 RESULT = [$RESULT]"
echo "  ③ 外面的 OUTER 还是 = [$OUTER]   ← 赋值没传出来，只有文本传出来了"

# --- 补充：哪些写法**不**产生子 shell（很多人以为会） -------------------------
# `!!` 反过来说：`cmd | { ... ; }` 的花括号只是分组，不 fork；
# 但因为它在管道右侧，它**仍然**是子 shell —— 分组和 fork 是两件事。
SUM=0
printf '10\n20\n30\n' | {
    while IFS= read -r x; do SUM=$((SUM + x)); done
    echo "  ④ 用 { } 分组：SUM=$SUM   \$BASHPID=$BASHPID   ← 分组救不了管道右侧"
}
echo "  ④ 外面的 SUM = [$SUM]"
