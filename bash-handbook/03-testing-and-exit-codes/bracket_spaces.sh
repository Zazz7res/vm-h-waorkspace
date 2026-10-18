#!/usr/bin/env bash
# ============================================================================
# bracket_spaces.sh —— `[ ]` 到底是个什么东西：程序身份、空格语法、经典事故
#
# 这个文件在演什么：
#   ① `[` 是一条**命令**（bash 内建；/usr/bin/[ 也确实有个程序），`test` 是它的别名，
#      而 `[[` 是 bash 的**语法关键字**（type -a 的输出当场给出身份）；
#   ② 因为 `[ ]` 是命令，「空格」就是参数分隔符：少一个空格 = 换一条命令；
#   ③ 因为 `[ ]` 是命令，参数会先被展开，变量没加引号就会改变**参数个数**，
#      每种坏法的报错原文和退出码都不一样，这里一条条隔离跑出来；
#   ④ `-a` / `-o` / `!` 的优先级：同一道题三种写法，跑出不同答案；
#   ⑤ 数字用 -eq、字符串用 =：以及 `[ 010 -eq 10 ]` 和 `(( 010 == 8 ))` 同时成立
#      这种「test 不认识八进制」的惊喜。
#
# 输出格式：
#   每条都是「$ 原样代码 → 真实输出（含我们自己补的 rc=）→ ↪ rc=子 shell 的码」。
#   代码交给 `bash -c` 的全新子 shell 跑，所以报错文案就是你在终端里敲会看到的那种
#   （`bash: line 1: ...`），而且段与段之间不共享变量。
#
# 安全说明：只在沙箱里读，不写任何文件；外部 `[` 程序在 /usr/bin 下，也只调用不修改。
# ============================================================================
set -u

try() {                          # try '一条 shell 代码'
    printf '\n  $ %s\n' "$1"
    bash -c "$1" 2>&1 | sed 's/^/      /'
    printf '      ↪ rc=%d\n' "${PIPESTATUS[0]}"
}

printf '\n════ 1. `[` 的程序身份 ════'
try 'type -a ['
try 'command -v [; echo "↑ 只打印一个 [ ：命令名解析到的是内建，所以不带路径"'
try 'env /usr/bin/[ x = x ]; echo "env 强制调用外部那份 [ → rc=$?"'
try 'type -a test'
try 'type -a "[["'
try 'ls -l /usr/bin/['
try '/usr/bin/[ x = x ]; echo "外部 [ 也能跑 → rc=$?"'
try '/usr/bin/[ -v HOME ]; echo "外部 [ 支持 -v 吗 → rc=$?"'
try '/usr/bin/[ a == a ]; echo "外部 [ 支持 == 吗 → rc=$?"'
try '/usr/bin/[ 010 -eq 10 ]; echo "外部 [ 怎么读 010 → rc=$?"'
try '/usr/bin/[ x; echo "外部 [ 少 ] → rc=$?"'

printf '\n════ 2. 空格就是语法（少一个空格 = 换一条命令）════'
try '[ -z x ]; echo "[ -z x ] 正常写法 → rc=$?"'
try '[-z x]; echo "上面报完错，这一行照样跑 → rc=$?"'
try '[ -z x]; echo "同上 → rc=$?"'
try 'x=y; [ "$x"=y ]; echo "粘成一个参数 [y=y] → rc=$?"'
try 'x=y; [ "$x" =y ]; echo "= 右边少空格（其实还是两个词）→ rc=$?"'
try 'x=y; [x = y]; echo "左边少空格 → rc=$?"'

printf '\n════ 3. 参数个数决定 [ ] 怎么解释同一串字 ════'
try '[ x ]; echo "1 个参数：非空字符串就是真 → rc=$?"'
try '[ "" ]; echo "1 个参数：空串 → rc=$?"'
try '[ 0 ]; echo "1 个参数：0 被当字符串看 → rc=$?"'
try '[ -n ]; echo "1 个参数：-n 被当成普通字符串 → rc=$?"'
try '[ -z ]; echo "1 个参数：-z 同理 → rc=$?"'
try '[ -v ]; echo "1 个参数：-v 同理 → rc=$?"'
try '[ ! x ]; echo "2 个参数 → rc=$?"'
try '[ a = b ]; echo "3 个参数：三元比较 → rc=$?"'
try '[ a b c ]; echo "3 个参数：中间那个不是运算符 → rc=$?"'
try '[ a b c d ]; echo "4 个参数 → rc=$?"'
try '[ a -a b ]; echo "3 个参数：-a 是「与」 → rc=$?"'
try '[ a -a b -a c ]; echo "5 个参数：POSIX 只规定到 4 → rc=$?"'

printf '\n════ 4. 变量没加引号：同一个语句，x 取三种值 ════'
try 'x="";    [ $x = a ]; echo "x 为空 → rc=$?"'
try 'x="b c"; [ $x = a ]; echo "x 含空格 → rc=$?"'
try 'x="abc"; [ $x = a ]; echo "x 是单个词（不报错，只是判假）→ rc=$?"'
try 'x="-n";  [ $x = a ]; echo "x 以 - 开头 → rc=$?"'
try 'x="";    [ $x ]; echo "[ \$x ] x 为空 → rc=$?"'
try 'x="b c"; [ $x ]; echo "[ \$x ] x 含空格 → rc=$?"'
try 'x="";    [ -n $x ]; echo "[ -n \$x ] x 为空 → rc=$?"'
try 'x="有值"; [ -n $x ]; echo "[ -n \$x ] x 有值 → rc=$?"'
try 'x="a b"; [ -z $x ]; echo "[ -z \$x ] x 含空格 → rc=$?"'
try 'x="a b"; [ -z "$x" ]; echo "加引号的正确写法 → rc=$?"'

printf '\n════ 5. 报错归报错：if 整条还是 0，set -e 抓不到 ════'
try 'x=""; if [ $x = a ]; then echo 走then; else echo 走else; fi; echo "整条 if 的 rc=$?"'
try 'bash -e -c "x=; [ \$x = a ]; echo 报错之后脚本还在跑"'
try 'bash -e -c "x=; if [ \$x = a ]; then echo t; fi; echo if 之后还活着"'

printf '\n════ 6. 数字 -eq vs 字符串 =：带前导零的惊喜 ════'
try '[ 010 -eq 10 ]; echo "[ 010 -eq 10 ] → rc=$?"'
try '[ 010 -eq 8 ];  echo "[ 010 -eq 8 ] → rc=$?"'
try '[ 010 = 10 ];   echo "[ 010 = 10 ] → rc=$?"'
try '[ 010 = 010 ];  echo "[ 010 = 010 ] → rc=$?"'
try '(( 010 == 8 )); echo "(( 010 == 8 )) → rc=$?"'
try 'echo "\$((010)) = $((010))"'
try 'printf "printf %%d 010 → %d\n" 010'
try '[ 008 -eq 8 ];  echo "[ 008 -eq 8 ]（真八进制里没 8）→ rc=$?"'
try '[ 0x10 -eq 16 ]; echo "[ 0x10 -eq 16 ] → rc=$?"'
try '(( 0x10 == 16 )); echo "(( 0x10 == 16 )) → rc=$?"'
try '[ abc -eq 1 ];  echo "[ abc -eq 1 ] → rc=$?"'
try '[[ abc -eq 1 ]]; echo "[[ abc -eq 1 ]] → rc=$?"'
try '[ 10 -gt 9 ]; [[ 10 -gt 9 ]]; echo "两版 -gt 都真 → $?"'

printf '\n════ 7. `=` 与 `==`：[ ] 里比字面，[[ ]] 里右边是模式 ════'
try '[ abc = abc ];  echo "[ abc = abc ] → rc=$?"'
try '[ abc == abc ]; echo "[ abc == abc ] → rc=$?"'
try '[ abc = a* ];   echo "[ abc = a* ]（[ ] 不做模式匹配）→ rc=$?"'
try '[ abc == a* ];  echo "[ abc == a* ]（bash 的 [ ] 也不匹配）→ rc=$?"'
try '[[ abc == a* ]]; echo "[[ abc == a* ]]（右边是 glob）→ rc=$?"'
try '[[ abc == "a*" ]]; echo "右边加引号 → 变字面比较 → rc=$?"'
try '[[ abc = a* ]];   echo "[[ abc = a* ]]（= 与 == 等价）→ rc=$?"'
try 'dash -c "[ abc == abc ]; echo dash的== → rc=\$?"'
try 'dash -c "[ abc = abc ]; echo dash的= → rc=\$?"'

printf '\n════ 8. `[ ]` 里的 `<`：不转义是重定向，转义才是比较，且不受 locale 影响 ════'
try '[ a < b ]; echo "[ a < b ] 没转义 → rc=$?"'
try '[ a \< b ]; echo "[ a \< b ] 转义后 → rc=$?"'
try '[[ a < b ]]; echo "[[ a < b ]] 不用转义 → rc=$?"'
try 'LC_ALL=C bash -c "[[ b < B ]]"'
try 'LC_ALL=zh_CN.UTF-8 bash -c "[[ b < B ]]"'
try 'LC_ALL=C bash -c "[ b \< B ]"'
try 'LC_ALL=zh_CN.UTF-8 bash -c "[ b \< B ]"'
try 'LC_ALL=C bash -c "[ Z \< a ]"'
try 'LC_ALL=zh_CN.UTF-8 bash -c "[ Z \< a ]"'
try 'LC_ALL=C bash -c "[[ Z < a ]]"'
try 'LC_ALL=zh_CN.UTF-8 bash -c "[[ Z < a ]]"'
try 'echo "排序证据："; printf "Z\na\n" | LC_ALL=C sort | tr "\n" " "; echo; printf "Z\na\n" | LC_ALL=zh_CN.UTF-8 sort | tr "\n" " "; echo'

printf '\n════ 9. -a / -o / ! 的优先级：一题三写法三个答案 ════'
try 'a=1; b=9; c=9; [ "$a" = 1 -o "$b" = 2 -a "$c" = 3 ]; echo "不加括号（意图 (a或b)且c=假）→ rc=$?"'
try 'a=1; b=9; c=9; [ \( "$a" = 1 -o "$b" = 2 \) -a "$c" = 3 ]; echo "加 \\( \\) 分组 → rc=$?"'
try 'a=1; b=9; c=9; [[ ( $a == 1 || $b == 2 ) && $c == 3 ]]; echo "[[ ]] 里括号无需转义 → rc=$?"'
try 'a=1; b=9; c=9; [ "$a" = 1 -o \( "$b" = 2 -a "$c" = 3 \) ]; echo "把 -a 括起来 → rc=$?"'
try 'x=""; y=""; [ ! -z "$x" -a -n "$y" ]; echo "意图 !(x空 且 y非空)=真 → rc=$?"'
try 'x=""; y=""; [ ! \( -z "$x" -a -n "$y" \) ]; echo "显式分组 → rc=$?"'
try 'x=""; y=""; [[ ! ( -z "$x" && -n "$y" ) ]]; echo "[[ ]] 版 → rc=$?"'
try 'a=1; [ ( $a = 1 ) ]; echo "[ ] 里裸括号（不转义）→ rc=$?"'
try 'a=1; b=9; [[ $a == 1 && ( $b == 2 || $b == 9 ) ]]; echo "[[ ]] 里嵌套分组 → rc=$?"'

printf '\n════ 10. [[ ]] 的错误是「语法错误」：会把脚本当场掐掉 ════'
try 'printf "%s\n" "echo 第一行" "[[ 1 -eq ]]" "echo 第三行" > bad_double.sh; bash bad_double.sh; echo "脚本 rc=$?"'
try 'printf "%s\n" "echo 第一行" "[ 1 -eq 2 -zz ]" "echo 第三行" > bad_single.sh; bash bad_single.sh; echo "脚本 rc=$?"'
