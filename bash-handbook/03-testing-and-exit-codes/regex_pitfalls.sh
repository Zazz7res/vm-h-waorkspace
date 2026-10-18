#!/usr/bin/env bash
# ============================================================================
# regex_pitfalls.sh —— `[[ ]]` 的 ==（glob）与 =~（正则）：引号一加，含义全变
#
# 这个文件在演什么：
#   ① `=~` 右边**不加引号**才是正则，加了引号就退化成字面量 —— 两种结果各跑一遍；
#   ② 捕获组在 ${BASH_REMATCH[@]}：下标 0 是整个匹配，1..n 是各捕获组；
#   ③ 正则里带空格必须整体引号，否则是**语法错误**（rc=2）；
#   ④ 把正则存进变量再 `=~ $var`（不加引号）是官方推荐写法，含空格也能用；
#   ⑤ `=~` 是 ERE，不是 PCRE：`\d` 不匹配数字，它匹配字母 d；
#   ⑥ `=~` 是「找子串」，不锚定 `$` 就会误判长字符串；
#   ⑦ `==` 右边是 glob（`* ? [ ]` 和 extglob `@() +() ?()`），`=~` 右边是正则 ——
#      同一个字符串、两种写法、两种答案；
#   ⑧ 裸 `(a|b)` 在 `[[ ]]` 里是语法错误，`@(a|b)` 才对，而且不需要 shopt -s extglob。
#
# 为什么要演：
#   「正则到底该怎么写进 bash」这件事，网上教程互相矛盾。只有把每种写法各跑一次，
#   才能看清「同一个模式，加不加引号是两个不同的程序行为」。
#
# 输出格式同 bracket_spaces.sh：$ 代码 → 真实输出 → ↪ rc=子 shell 退出码。
# 安全说明：全程纯字符串判断，不碰文件系统（除了在本目录写 rp_ 前缀的临时样本）。
# ============================================================================
set -u

try() {
    printf '\n  $ %s\n' "$1"
    bash -c "$1" 2>&1 | sed 's/^/      /'
    printf '      ↪ rc=%d\n' "${PIPESTATUS[0]}"
}

printf '\n════ 1. 不加引号 = 正则：捕获组进 BASH_REMATCH ════'
try 's="v1.2.3"
[[ $s =~ ^v([0-9]+)\.([0-9]+) ]]
echo "rc=$?  组0=${BASH_REMATCH[0]}  组1=${BASH_REMATCH[1]}  组2=${BASH_REMATCH[2]}"
echo "整个数组=[${BASH_REMATCH[*]}] 元素个数=${#BASH_REMATCH[@]}"'

printf '\n════ 2. 加引号 = 字面量：同一条正则，答案反过来 ════'
try 's="v1.2.3"
[[ $s =~ "^v([0-9]+)" ]]
echo "加引号版 rc=$?"'
try 's="v1.2.3"
[[ $s =~ ^v([0-9]+) ]]
echo "不加引号版 rc=$?"'
try '[[ "axxb" =~ a.x ]];  echo "点号不加引号（任意字符）rc=$?"'
try '[[ "axxb" =~ "a.x" ]]; echo "点号加引号（字面点）rc=$?"'
try '[[ "a.c"  =~ "a.c" ]]; echo "字面 a.c 命中 a.c rc=$?"'

printf '\n════ 3. 模式里有空格：不加引号是语法错误 ════'
try '[[ "a b c" =~ a b c ]]; echo "不该走到这里"'
try '[[ "a b c" =~ "a b c" ]]; echo "整体加引号 rc=$?"'
try 're="a b c"; [[ "a b c" =~ $re ]]; echo "模式放变量、引用变量时不加引号 rc=$?"'

printf '\n════ 4. 这是 ERE，不是 PCRE ════'
try '[[ "a1" =~ \d ]]; echo "\\d 匹配数字吗 rc=$?"'
try '[[ "xd" =~ \d ]]; echo "那它到底匹配了什么 rc=$?"'
try '[[ "a1" =~ [0-9] ]]; echo "正确写法：字符类 rc=$?"'
try '[[ "ab" =~ ^[a-c]+$ ]]; echo "量词 + 锚定 rc=$?"'
try '[[ "ABC" =~ [a-z] ]]; echo "默认区分大小写 rc=$?"'

printf '\n════ 5. 不锚定 \$ 就是「找子串」 ════'
try '[[ "v1x" =~ ^v[0-9] ]]; echo "不写尾巴锚 rc=$?"'
try '[[ "v1x" =~ ^v[0-9]$ ]]; echo "写了尾巴锚 rc=$?"'
try '[[ "版本 v12 发布" =~ ^v[0-9]+$ ]]; echo "整串必须全是版本号 rc=$?"'
try '[[ "版本 v12 发布" =~ v[0-9]+ ]]; echo "只要求出现一次 rc=$?"'

printf '\n════ 6. == 是 glob，=~ 是正则：同一个串两种答案 ════'
try '[[ "abc" == a*c ]]; echo "== 用 glob：* 是任意串 rc=$?"'
try '[[ "abc" =~ a.c ]]; echo "=~ 用正则：. 只顶一个字符 rc=$?"'
try '[[ "abbc" =~ a.c ]]; echo "abbc 对 a.c（要正好一个中间字符）rc=$?"'
try '[[ "abbc" =~ a.*c ]]; echo "正则里要 a.*c 才等价 rc=$?"'
try 'f=app.log; [[ $f == *.log ]]; echo "按扩展名分派就用 == glob rc=$?"'
try 'f=app.log; [[ $f =~ \.log$ ]]; echo "换成正则要转义点并锚定 rc=$?"'

printf '\n════ 7. 引号是把刀：模式侧加了引号就不再是模式 ════'
try '[[ "aXb" == a*b ]];   echo "右边裸着写：* 是模式 rc=$?"'
try '[[ "aXb" == "a*b" ]]; echo "右边加引号：* 是字面星号 rc=$?"'
try '[[ "a*b" == "a*b" ]]; echo "值真的叫 a*b 时才靠引号版 rc=$?"'
try 'pat="a*b"; [[ "aXb" == $pat ]];   echo "模式存进变量、引用时不加引号 → 仍然是模式 rc=$?"'
try 'pat="a*b"; [[ "aXb" == "$pat" ]]; echo "模式存进变量、引用时加引号 → 变成字面 rc=$?"'
try 'x="*"; [[ $x == "*" ]]; echo "[[ ]] 左边不做文件名展开：x 就还是一个星号 rc=$?"'

printf '\n════ 8. 括号：@( ) 可以，裸 ( ) 是语法错误 ════'
try 'a=dog; [[ $a == @(cat|dog) ]]; echo "extglob @() 不用 shopt 就能用 rc=$?"'
try 'a=dog; [[ $a == (cat|dog) ]]; echo "裸括号：不该走到这里"'
try 'a=dog; shopt -s extglob; [[ $a == @(cat|dog) ]]; echo "显式开 extglob 之后 rc=$?"'
try '[[ "aaa" == +(a) ]]; echo "+() 一个以上 rc=$?"'
try '[[ "" == ?(a) ]]; echo "?() 零或一个 rc=$?"'
try '[[ "abc" =~ ^(a|b) ]]; echo "正则里的 ( ) 是捕获组 rc=$?"; echo "  BASH_REMATCH=[${BASH_REMATCH[*]}]"'

printf '\n════ 9. 两种错误：解析期语法错误 vs 运行期正则错误 ════'
try '[[ 1 -eq ]]; echo "不该走到这里"'
try '[[ "abc" =~ a[b ]]; echo "正则里 [ 不闭合 rc=$?"'
try 'printf "%s\n" "echo 第一行" "[[ 1 -eq ]]" "echo 第三行" > bad_double.sh; bash bad_double.sh; echo "脚本 rc=$?"'
try 'printf "%s\n" "echo 第一行" "[[ abc =~ a[b ]]" "echo 第三行（正则错误不中断脚本）" > bad_re.sh; bash bad_re.sh; echo "脚本 rc=$?"'
