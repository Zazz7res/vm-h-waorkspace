#!/usr/bin/env bash
# ============================================================================
# exportfun.sh —— 函数是 shell 进程里的对象，不是磁盘上的文件
#
#   demo.sh 里这样调：bash exportfun.sh
#   主线③：函数活在「定义它的那个 bash 进程」的函数表里；换一个新进程（bash -c）就看不见它，
#   除非 export -f 把它的文本塞进环境变量传下去。别名根本不在这张表里。
#   安全：只在**当前目录**建 exp/ 子目录（放那个「已经启动的子进程」的落盘文件），跑完由
#         trap 'rm -rf exp' EXIT 删掉；不碰 ~、不碰绝对 /tmp、不开端口、不联网。
#   故意不加 set -e：这一整支就是要看「bash 自己报的错」——没导出 rc=127、export -f 撞别名 rc=1，
#         加了 -e 后面的对照组全看不见。
#   ★ 本机不拿什么当证据：现成躺着哪些 BASH_FUNC_ 键、env 一共多少行，每跑每台都不一样，
#     所以改成「键名以 BASH_FUNC_ 开头吗、值以 () { 开头吗」这类判定，或只报增量/差值。
#   ★ 报错原文一律放进 bash -c 起的新进程里跑：前缀才是干净的 "bash:"，不会把本脚本被调用时
#     的绝对路径（含真实家目录）印上屏。
# ============================================================================

rm -rf exp
mkdir -p exp
trap 'rm -rf exp' EXIT

echo "=== 1. 同一个进程看得见，换个新进程就看不见 ==="
xpf_hello() { printf '    〔xpf_hello 真的跑了〕\n'; }
printf '  本进程里 type -t xpf_hello → %s\n' "$(type -t xpf_hello)"
printf '  本进程里直接调用：\n'; xpf_hello
printf '  换一个新的 bash 进程跑同一条命令（bash -c "xpf_hello"）：\n'
err1=$(bash -c 'xpf_hello' 2>&1); rc1=$?
printf '    原文=[%s] ｜ 退出码=%s\n' "$err1" "$rc1"
printf '  → ★ 函数没导出 = 只存在于当前这个 bash 进程的函数表里；bash -c 起的是新进程，那张表从零开始。\n'

echo
echo "=== 2. export -f 之后：同一条命令的输出差别（两处都是真实原文） ==="
export -f xpf_hello
printf '  export -f 之后再 bash -c "xpf_hello"：\n'
bash -c 'xpf_hello' 2>&1
printf '    退出码=%s\n' "$?"
printf '  对照组：新进程里先 unset -f 再调（等于没导出）：\n'
err2=$(bash -c 'unset -f xpf_hello; xpf_hello' 2>&1); rc2=$?
printf '    原文=[%s] ｜ 退出码=%s\n' "$err2" "$rc2"
printf '  → export -f 不是「让函数更厉害」，是把它的**文本**写进环境变量，好让下一个 bash 进程读得到。\n'

echo
echo "=== 3. 导出后 env 里的键名形状（用判定，不依赖本机具体函数集合） ==="
printf '  env 里和这个函数有关的那一行（整行原文）：\n'
env | grep '^BASH_FUNC_xpf_hello%%=' | sed 's/^/    /'
kv=$(env | grep '^BASH_FUNC_xpf_hello%%=' | head -1)
key=${kv%%=*}
val=${kv#*=}
printf '  键名 = [%s]\n' "$key"
printf '    → 以 BASH_FUNC_ 开头吗：%s ｜ 名字两侧夹了双百分号吗：%s\n' \
    "$([ "${key:0:10}" = "BASH_FUNC_" ] && echo 是 || echo 否)" \
    "$([ "$key" = "BASH_FUNC_xpf_hello%%" ] && echo 是 || echo 否)"
printf '  值 = [%s]\n' "$(printf '%s' "$val" | tr '\n' '/')"
printf '    → 以 "() {" 开头吗：%s ← 值就是函数体文本，不是一个路径\n' "$([ "${val:0:4}" = "() {" ] && echo 是 || echo 否)"
printf '  printenv "BASH_FUNC_xpf_hello%%" 单独取出来（注意它是跨行的）：\n'
printenv 'BASH_FUNC_xpf_hello%%' | sed 's/^/    /'
echo "  ★ 键名里那两个 % 不是笔误，是 bash 的固定包裹；正常环境变量名不允许有 %，所以这一行很扎眼。"

echo
echo "=== 4. 别名不会跟着 export -f 走（真实报错原文） ==="
printf '  先做一个别名，再看它进不进函数表（都在新进程里跑）：\n'
bash -c '
    alias xpf_ll="printf 别名的身体\n"
    printf "    declare -F xpf_ll → [%s]（空 = 别名压根不在函数表里）\n" "$(declare -F xpf_ll)"
'
err3=$(bash -c 'alias xpf_ll="echo hi"; export -f xpf_ll' 2>&1); rc3=$?
printf '    export -f 一个别名名字 → 原文=[%s] ｜ 退出码=%s\n' "$err3" "$rc3"
echo "  → 别名是「读代码时的一次文字替换」，函数是「进程里一个真正的对象」；两条线，export -f 只认函数。"

echo
echo "=== 5. ( ) 子 shell / { } 分组 / bash -c：谁看得见函数 ==="
xpf_g() { echo "看见 xpf_g"; }
printf '  定义 xpf_g 后，三种「换个写法调用」的结果：\n'
r_g=$( { xpf_g; } 2>&1 ); printf '    { xpf_g; } 分组      → [%s] ← 就是当前进程本身，当然看得见\n' "$r_g"
r_s=$( ( xpf_g ) 2>&1 ); printf '    ( xpf_g )   子 shell → [%s] ← 当前进程 fork 的副本，函数表一起抄走\n' "$r_s"
r_b=$( bash -c 'xpf_g' 2>&1 ); printf '    bash -c xpf_g 新进程 → [%s]\n' "$r_b"
printf '  现在把同名函数导出，再走一次 bash -c：\n'
export -f xpf_g
r_b2=$( bash -c 'xpf_g' 2>&1 ); printf '    bash -c xpf_g（已导出）→ [%s]\n' "$r_b2"
echo "  → ★ 判据：( ) 和 { } 都还是**同一个进程**（前者是它的 fork 副本，函数表跟着抄），所以看得见；"
echo "    bash -c 是 exec 一个全新进程，函数表从零开始，不 export 就看不见 —— 上面那个原文就是它说的。"

echo
echo "=== 6. 导出的是「那一刻的文本」：已启动的孩子冻旧版，之后新起的孩子拿新版 ==="
xpf_snap() { echo "OLD_导出那一刻的文本"; }
export -f xpf_snap
( bash -c 'sleep 0.2; xpf_snap' ) > exp/frozen.txt 2>&1 &
bg=$!
xpf_snap() { echo "NEW_父shell后来重新定义"; }   # bash 5.3 会把新版重新同步进 env
wait "$bg"
printf '  在重新定义**之前**就 fork 出去的那个后台子进程，打印的是：[%s]\n' "$(cat exp/frozen.txt)"
printf '  父进程自己现在打印的是：[%s]\n' "$(xpf_snap)"
printf '  重新定义**之后**才新起的 bash -c，打印的是：[%s]\n' "$(bash -c xpf_snap)"
echo "  → ★ 反直觉在中间和最后两行：子进程只认**它自己 exec 那一刻** env 里躺着的那份文本，冻住就不回头同步；"
echo "    所以「旧定义不变」说的是已经跑起来的那个孩子；bash 5.3 在父进程重新定义时会刷新 env，故之后新起的孩子看到新版。"

echo
echo "=== 7. 真实代价（只写实测到的）：每一个子进程都被塞，env 输出变难读 ==="
before=$(env | wc -l)
xpf_big() {
    echo 第一行
    echo 第二行
    echo 第三行
}
export -f xpf_big
after=$(env | wc -l)
printf '  同一个进程里多导出一个 3 行体的函数，env 行数净增 %s 行（★ 只取差值：绝对值随启动环境变，不当证据）\n' "$((after - before))"
printf '  再确认它跟着进每一个 fork 出去的孩子（只报「孩子里能不能查到」，不报绝对计数）：\n'
bash -c '
    xpf_child(){ echo a; echo b; }
    export -f xpf_child
    found=$(bash -c "env | grep -c \"^BASH_FUNC_xpf_child%%\" ")
    printf "    孙子进程里能查到 xpf_child 这个 BASH_FUNC_ 条目吗 → %s\n" \
        "$([ "$found" -ge 1 ] && echo 查到了 || echo 没有)"
'
echo "  → 实测到的风险：export -f 会把函数塞进**每一个**子进程的环境；函数体一带换行，env 输出里就冒出"
echo "    跨行、带空格和 () { 的怪条目，肉眼很难对齐 —— 所以生产脚本用「磁盘上的脚本 + 显式 source」传函数，而不是到处 export -f。"

echo
echo "=== 8. 收口 ==="
printf '  删之前 exp/ 里有 %s 个文件 ｜ 现在执行 rm -rf exp（trap 里是同一句，这里提前跑一遍好自证）\n' \
    "$(ls -A exp | wc -l)"
rm -rf exp
printf '  自证：现场目录 exp/ 还在吗 → %s ｜ ls -A 里名为 exp 的条目数 = %s\n' \
    "$([ -d exp ] && echo 在 || echo 不在了)" "$(ls -A | grep -cx exp)"
printf '  自证：本脚本自己的后台作业表 jobs -p 行数 = %s（第 6 段那个孩子已经 wait 收掉了）\n' "$(jobs -p | wc -l)"
echo "  ① 函数是**进程里的对象**：type -t 说它在本进程存在，bash -c 起的下一个进程默认读不到它。"
echo "  ② export -f 走的是环境变量这条路：键名 BASH_FUNC_名字%%、值以 () { 开头 —— 传的是文本快照不是活引用。"
echo "  ③ 别名不在函数表里（declare -F 查为空），export -f 撞它会当场报「不是函数」rc=1。"
echo "  ④ 想跨进程传函数，正解是把函数写进磁盘上的脚本再 source，而不是到处 export -f。"
