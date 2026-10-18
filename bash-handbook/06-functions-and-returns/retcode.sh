#!/usr/bin/env bash
# ============================================================================
# retcode.sh —— return 的真相：函数只能还你一个退出码，而且是 0~255 里的那一个
#
#   demo.sh 里这样调：bash retcode.sh
#   安全：本脚本不建目录、不写文件、不起后台进程、不碰任何配置；要看「bash 自己的报错原文」
#         一律放进 bash -c 起的全新子进程里跑 —— 那样报错前缀是干净的 "bash: 行 1:"，
#         不会把本脚本自己的路径（含真实家目录）印上屏。
#   故意不加 set -e：这一整支就是在收集非 0 退出码，一遇失败就停，后面的对照组全看不见了。
#
# 主线①：函数没有「返回值」这回事，只有退出码。return 和 $? 是同一条线的两端 —— return 说
#   什么，调用方的 $? 就是什么；不写 return，就是函数体里最后执行的那条命令说了算。
#   想把「数据」带出去只能走 stdout，那是 outcapture.sh / errmix.sh 那一支的事。
# ★ 本机不拿耗时当证据：「等于几 / 是不是 0 / 报错原文」这三样每跑都一样，毫秒不是，
#   所以全篇一个时间数字都不打。
# ============================================================================

echo "=== 1. return 后面能写什么：五个写法各跑一次（bash 原文照打） ==="
# probe <return 后面那串>：在子进程里定义 f() { return <那串>; } 并调用，退出码和原文一起收
probe() {
    out=$(bash -c "f() { return $1; }; f" 2>&1)
    rc=$?
    printf '  return %-5s → $?=%-4s ｜ 是不是 0 → %-2s ｜ 除了退出码还有什么：%s\n' \
        "$1" "$rc" "$([ "$rc" -eq 0 ] && echo 是 || echo 否)" "${out:-（一句报错都没有）}"
}
probe 0
probe 42
probe 300
probe -1
probe abc
echo '  → 直觉以为「写 300 会报错」：实测一句报错都没有，它只是被折成了 44；'
echo '    真报错的只有 abc（需要数值参数）。而且 bash 一报错就把那次调用当场结束，'
echo '    函数体里剩下的句子一句都不跑，退出码停在 2 —— 这个 2 是「内建命令用法不对」的固定码，不是你写的那个数。'

echo
echo "=== 2. ★ 退出码只保留低 8 位：256 和 258 当场对账 ==="
low8() { return "$1"; }
for n in 255 256 257 258 300 512 99999; do
    low8 "$n"; rc=$?
    printf '  return %-6s → $?=%-4s ｜ 和 %s %% 256 = %-4s 相等吗 → %s\n' \
        "$n" "$rc" "$n" "$((n % 256))" "$([ "$rc" -eq "$((n % 256))" ] && echo 相等 || echo 不相等)"
done
echo '  → 一句话：退出码是一个字节（0~255）。256 → 0、257 → 1、258 → 2、300 → 44、-1 → 255。'
echo '  ★ 256 那一格最阴：你本想「返回 256」表达某个状态，$? 却变成 0，'
echo '    调用方那句 if f; then 一路绿灯 —— 这不是无害的截断，是把失败伪装成了成功。'

echo
echo "=== 3. return 的三个边角：它之后的句子、空的函数体、写在函数外面 ==="
after_ret() { return 7; echo "  〔函数体里 return 之后写的这句〕"; }
cap=$(after_ret 2>&1)
printf '  f(){ return 7; echo 这句; } → 调用后收到的字符数=%s（0 = return 之后的话压根轮不到说）\n' "${#cap}"
nothing() { :; }
nothing
printf '  函数体里只写一条 : （空操作）→ 退出码=%s ← 想「什么都不干」只能这么写\n' "$?"
err3=$(bash -c 'e(){ }' 2>&1); rc3=$?
printf '  函数体一个字符都不留（e(){ }）→ 退出码=%s，原文：\n' "$rc3"
printf '%s\n' "$err3" | sed 's/^/    /'
err4=$(bash -c 'return 3' 2>&1); rc4=$?
printf '  在函数**外面**写 return 3 → 退出码=%s，原文：\n' "$rc4"
printf '%s\n' "$err4" | sed 's/^/    /'
echo '  → 最后那条原文里的「被 source 的脚本」是关键：source 进来的脚本就是当前 shell 自己，'
echo '    所以那里能 return；用 bash 跑起来的脚本是一个独立进程，它没有「调用方的函数」可回，只能 exit。'

echo
echo "=== 4. 不写 return 的函数：退出码 = 最后执行的那条命令（下面用一条一定失败的 ls 当证据） ==="
tail_fails() { echo "  〔第一条：干活，成功〕"; ls retcode-没有这个文件.txt; }
echo '  下面紧跟着的那一行**没有缩进**：它是 ls 的 stderr 直接上屏，本脚本一个字都没改（原文照打）：'
tail_fails; rc5=$?
printf '  ↑ 上面那行 ls 的报错原文，就是函数体最后一条命令留下的状态 → 退出码=%s\n' "$rc5"
washed() { ls retcode-没有这个文件.txt 2>/dev/null; echo "  〔第二条：把场子圆回来了〕"; }
washed; rc6=$?
printf '  中间那条失败、最后一条是 echo → 退出码=%s ← ★ echo 几乎永远返回 0，失败被洗掉了\n' "$rc6"
if washed >/dev/null 2>&1; then br=then; else br=else; fi
printf '  同一个 washed 放进 if 里走的是 %s 分支 ← 「明明挂了一步，if 却认为成功」就是这么来的\n' "$br"
echo '  → 两条判据：① 想让函数表达失败，就自己 return 一个非 0，别把希望寄托在「最后一条刚好是失败的那条」；'
echo '    ② bash 只看最后一个状态，函数体里挂过几次都不算数。'

echo
echo "=== 5. exit 和 return 在函数里的差别：谁结束、还剩多少话要说 ==="
echo '  先在**本脚本自己身上**演示 return 那一支：'
only_return() { return 3; echo "  〔函数体里 return 之后的这句〕"; }
only_return; rc7=$?
echo "  ★ 我还活着 —— 这一行是调用完 return 版函数之后才打出来的，脚本没被打断 ｜ 刚才的退出码=$rc7"
echo '  exit 那一支不敢在本脚本里跑（一跑后面全没了），所以两支都放进 bash -c 的子进程里对照。'
echo '  子进程末尾统一写 `exit "$rc"`，好让屏幕上那个退出码是函数的 7、不是后面 echo 的 0：'
out5r=$(bash -c 'g(){ return 7; echo "  〔函数体里 return 之后的这句〕"; }; g; rc=$?; echo "  〔调用方：我还活着，刚拿到 rc=$rc〕"; exit "$rc"' 2>&1); rc5r=$?
out5e=$(bash -c 'h(){ exit 7;   echo "  〔函数体里 exit 之后的这句〕";   }; h; rc=$?; echo "  〔调用方：我还活着，刚拿到 rc=$rc〕"; exit "$rc"' 2>&1); rc5e=$?
show5() {
    printf '  函数体里写 %-6s → 子进程输出的字符数=%-3s ｜ 「我还活着」出现了吗 → %-2s ｜ 子进程退出码=%s\n' \
        "$1" "${#2}" "$(case "$2" in *我还活着*) echo 有 ;; *) echo 没有 ;; esac)" "$3"
}
show5 return "$out5r" "$rc5r"
show5 exit "$out5e" "$rc5e"
bash -c 'exit 300'; rc5x=$?
printf '  顺带一问：exit 后面写 300 → 退出码=%s ← exit 也逃不出第 2 段那个取模规矩\n' "$rc5x"
echo '  → 一句话：return 结束「这次调用」，脚本继续往下跑；exit 结束「这个 shell 进程」，'
echo '    函数后面所有话（包括调用方那一行）都不再存在。所以被复用的函数里默认写 return，'
echo '    exit 只留给脚本主干上那句「到此为止」。'

echo
echo "=== 6. 调用完之后退出码还保得住吗：四种写法实测（函数一律 return 3 + echo 数据） ==="
fcode() { echo 数据; return 3; }
v6=$(fcode 2>/dev/null); rc6b=$?
printf '  ① v=$(fcode)           → $?=%s ｜ v=[%s] ← 码和数据都留下了\n' "$rc6b" "$v6"
swallow() { local x=$(fcode 2>/dev/null); printf '  ② local x=$(fcode)     → $?=%s ← ★ 码被 local 吞掉了\n' "$?"; }
swallow
keep() { local x; x=$(fcode 2>/dev/null); printf '  ③ local x; x=$(fcode)  → $?=%s ← 声明和赋值分成两句就保住了\n' "$?"; }
keep
if fcode >/dev/null 2>&1; then br6=then; else br6=else; fi
printf '  ④ if fcode; then/else  → 走 %s 分支 ← 判断调用本身永远拿的是函数的码\n' "$br6"
echo '  → 差别的成因：local/declare 自己也是一条命令，「声明 + 赋值」写在同一行时那句的退出码是'
echo '    local 的（成功就是 0），fcode 的 3 当场蒸发。要码就分成两句，或者直接 if f; then。'

echo
echo "=== 7. 收口：四句能带走的话 + 自证 ==="
echo '  ① return 只能给一个 0~255 的整数：写 300 得 44、写 256 得 0（★ 伪装成成功）、写 -1 得 255、写 abc 直接报错退 2。'
echo '  ② 不写 return 不等于没有码：退出码 = 函数体里最后执行的那条命令的状态，中间挂掉的一步会被后面的 echo 洗掉。'
echo '  ③ 函数里默认写 return，exit 留给「整个脚本到此为止」；两者共用同一套 0~255 取模规矩。'
echo '  ④ 这条通道只有 8 位宽，装不下数据：想带回东西只能走 stdout（见 outcapture.sh、errmix.sh）'
echo '     或者全局变量/nameref（见 localscope.sh）。$? 只能回答「成没成」，别拿它传内容。'
printf '  自证：本脚本没写过任何文件/目录（以自己名字开头的条目数=%s）｜ 自己的作业表行数=%s ｜ 退出码到这一行为止都是 0\n' \
    "$(ls -A | grep -c '^retcode-')" "$(jobs -p | wc -l)"
