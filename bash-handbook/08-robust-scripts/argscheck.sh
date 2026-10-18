#!/usr/bin/env bash
# ============================================================================
# argscheck.sh —— 参数这道边界：getopts 收到什么、什么时候装死、`-` 开头的文件名怎么交出去
#
#   demo.sh 里这样调：bash argscheck.sh
#   安全：只在当前目录建 argscheck/ 子目录（f/ 里放一个以 - 开头的文件，rcs.txt 记退出码台账），
#         跑完由 trap 整目录删掉；不碰 ~、不写绝对 /tmp、不联网、不装包、不 kill、不起后台进程。
#   故意不开 set -e：本脚本必须活到打印 rc 那一行；要看 -u 的事故一律写进 bash -c 里面，当场印 rc。
#
# ★ 本机不拿「POSIX 怎么说」当证据：OPTARG 在 `?` 分支里到底有没有值、`--` 到底停在哪、
#   第一个非选项之后还认不认选项 —— 全部由屏幕上那行 rc 和 bash 自己打的报错原文来定（那句话是中文还是英文跟着 LC_ALL 走，
#   demo 第 1 节把两种并排印过一遍）。
#   每格走 try 助手：① 把喂给 bash -c 的命令串**和喂给它的位置参数**都原样打上屏（标签=真命令+真参数，
#   上一批次栽过「写着 -E 其实没传」，所以两样都得印）；② 在全新子 bash 里跑，本格的状态不串到下一格；
#   ③ 当场打印 rc 并抄进台账，最后一格一起收账。报错原文（本机终端是中文、被 demo 钉成 C.UTF-8 时是英文）一字不改、不翻译。
# ============================================================================

rm -rf argscheck
mkdir -p argscheck/f
trap 'rm -rf argscheck' EXIT
: > argscheck/rcs.txt

try() {
    cmd=$1
    shift
    printf '  敲的是：%s\n' "$cmd"
    printf '  参数是：%s\n' "$(printf '[%s]' "$@")"
    out=$(bash -c "$cmd" _ "$@" 2>&1)
    code=$?
    if [ -n "$out" ]; then printf '%s\n' "$out" | sed 's/^/    /'; else printf '    （一声没出）\n'; fi
    printf '  rc=%s\n' "$code"
    printf '%s\n' "$code" >> argscheck/rcs.txt
}
G='while getopts "a:b" o; do case $o in a) echo "取到 a=[${OPTARG-}]";; b) echo "取到 b";; \?) echo "取到 ? OPTARG=[${OPTARG-未设}]";; :) echo "取到 : OPTARG=[${OPTARG-未设}]";; esac; done; shift $((OPTIND-1)); echo "OPTIND=$OPTIND 剩下[$*]"'
D='while getopts ":a:b" o; do case $o in a) echo "静默:a=[${OPTARG-}]";; b) echo "静默:b";; \?) echo "静默:? OPTARG=[${OPTARG-未设}]";; :) echo "静默:: OPTARG=[${OPTARG-未设}]";; esac; done'

echo "=== 1. 最外层那道闸是 \$#：不是 getopts。顺手看 -u 下裸 \$1 的下场 ==="
try 'if [ "$#" -lt 2 ]; then echo "用法：要 2 个参数（<目录> <模式>），你这趟给了 $# 个"; exit 2; fi; echo 收工' -a
try 'set -u; echo "第一个=$1"'
try 'set -u; echo "第一个=${1-<没给>}"'
printf '  → 直觉以为「有 getopts 就不用查参数个数」：第一格 $# 是 1（-a 也算一个），闸门必须先过。\n'
printf '  ★ 第二格是 bash 的原话（本机终端里是「未绑定的变量」，钉成 C 之后是 unbound variable）+ rc=**127**：\n    脚本连「你没给参数」这句都来不及说就死了。\n'
printf '    读位置参数一律 ${1-} 或先查 $#。报错前缀打成 `_:` 是 try 把 $0 喂成了下划线 —— 屏幕上不出现真实路径。\n'

echo "=== 2. getopts 最小骨架：-a 带值、-b 开关，剩下的操作数靠 shift \$((OPTIND-1)) ==="
try "$G" -b -a foo extra1 extra2
try "$G" -a foo
printf '  → OPTIND 是「下一个该读第几个参数」的指针，1 起算（$0 不算），所以 shift 的量是 OPTIND-1。\n'
printf '  → 第二格 -a foo 两个参数都被吃进 a，剩下为空 —— 带值选项一次消耗两个位置，这决定了 $# 的算法。\n'

echo "=== 3. 打包：-ab 不是「-a 和 -b」，是「-a 的值等于 b」 ==="
try "$G" -ab
try "$G" -b -a b
printf '  → 直觉以为「-ab 等价于 -a -b」：屏幕上第一格只有一句 a=[b]，b 这个开关**根本没出现**。\n'
printf '  ★ optstring 里 a 后面有冒号（要吃值），打包时它就把尾巴整段当值；想同时要 a 和 b，得写成两格\n'
printf '    或 -b -a b（第二格才有「取到 b」）。这是「少了一个开关还不报错」的现场。\n'

echo "=== 4. 非静默模式：报错是 bash 自己打的，而 OPTARG 在 ? 分支里根本没设 ==="
try "$G" -a
try "$G" -z
try 'set -u; while getopts "a:b" o; do [ "$o" = "?" ] && echo "坏选项，OPTARG 是 [$OPTARG]"; done' -z
printf '  → 两格的报错原文都是 bash 打的，脚本一个字没写：本机终端（LANG=zh_CN.UTF-8、LANGUAGE=zh_CN:zh）里是「选项需要一个参数」
    「非法的选项」；demo 把 LC_ALL 与 LANGUAGE 一起钉成 C 之后，同一格是 option requires an argument / illegal option。\n'
printf '  ★ 同一格里 OPTARG=[未设] —— 本机 bash 5.3 在非静默模式下不给你 OPTARG；第三格接上 -u 之后\n'
printf '    直接 unbound 死掉（rc=127）。想拿那个坏字符只能走静默模式（下一格），别在 ? 分支读 $OPTARG。\n'
printf '  → 顺带一条：-u 单独穿时码是 127，同时穿了 -e 同一句报错就变成 1（对照 strictfour.sh 第 2 格）。\n'

echo "=== 5. 静默模式（optstring 开头写冒号）：两种事故只剩一个字符能分开 ==="
try "$D" -z
try "$D" -a
try "$G" -z
printf '  → 静默模式下 bash 的报错原文**一声不出**（对比第三格同一条 -z 是有原话的），全看你 case 写没写\n'
printf '    「? 支」和「: 支」：-z 给 ?（OPTARG=[z]），-a 缺值给 :（OPTARG=[a]）—— 少写一支就是静默吞掉。\n'
printf '  ★ 选了静默模式，usage 就得自己印；屏幕上「什么都没发生」不是没出错，是没人报。\n'

echo "=== 6. 「--」只在「选项位」才结束解析：它可以被带值选项当值吃掉 ==="
try "$G" -a -- -b 文件
try "$G" -- -b 文件
printf '  → 直觉以为「看到 -- 后面全是操作数」：第一格里 -- 紧跟 -a，被当成 a 的值（a=[--]），后面的 -b\n'
printf '    照样被解析成开关。第二格 -- 落在选项位上才真的刹车，-b 和 文件 一起落到「剩下」。\n'
printf '  ★ 所以「用户的文件名可能以 - 开头」时，调用方要么把 -- 顶到选项位（-a -- x），要么写成 ./x。\n'

echo "=== 7. 第一个非选项之后就停：后面的 -b 永远不会被当成选项 ==="
try "$G" x -b
try "$G" -b x -b
printf '  → 直觉以为「-b 在后面也会被认出来」：第一格 OPTIND=1、剩下[x -b]，循环一条都没进。\n'
printf '  ★ 第二格更阴：开头那个 -b 被认成开关（OPTIND 走到 2），停在 x 上，剩下的还是 [x -b] ——\n'
printf '    两格「剩下」一模一样，差别只在它前面有没有被解析掉的选项；屏幕上 OPTIND=1 对 2 就是证据。\n'

echo "=== 8. 以 - 开头的文件名交给外部命令：坏写法与两种正解 ==="
mkdash() { touch -- argscheck/f/-dash.txt; printf 'hi\n' > argscheck/f/-dash.txt; }
cntdash() { ls -A argscheck/f | grep -c -- -dash; }
mkdash
try 'cd argscheck/f; rm -f -dash.txt'
bad=$(cntdash)
try 'cd argscheck/f; rm -f -- -dash.txt'
ok1=$(cntdash)
mkdash
try 'cd argscheck/f; rm -f ./-dash.txt'
ok2=$(cntdash)
mkdash
try 'cd argscheck/f; cat -dash.txt'
try 'cd argscheck/f; cat -- -dash.txt'
try 'rm --version | head -1; cat --version | head -1'
printf '  ★ 计数守卫自己会响：坏写法 rm 报 rc=1、文件还剩 %s 个（它把 -dash.txt 当成一串选项了）；\n' "$bad"
printf '    加 -- 之后剩 %s 个、加 ./ 之后剩 %s 个。-- 是「选项到此为止」，./ 是把名字改成不以 - 开头。\n' "$ok1" "$ok2"
printf '  → 屏幕上 rm 那句原话自己就教你写 ./（中文终端里是「尝试使用」，C.UTF-8 里是 try）；
    本机 rm 是 GNU coreutils，而 cat 是 uutils，\n'
printf '    同一件事 cat 报的是英文 unexpected argument（tip 里也自己写了 use -- -d），两套厂商风格并存。\n'

echo "=== 9. 收账：上面每一条的真实退出码 ==="
printf '  按出场顺序的码：%s\n' "$(paste -sd ' ' argscheck/rcs.txt)"
n=$(wc -l < argscheck/rcs.txt)
z=$(grep -c '^0$' argscheck/rcs.txt)
nz=$(grep -vc '^0$' argscheck/rcs.txt)
two=$(grep -c '^2$' argscheck/rcs.txt)
printf '  条数=%s ｜ 码 0 的=%s ｜ 非 0 的=%s ｜ 其中 2（用法错）的=%s\n' "$n" "$z" "$nz" "$two"
printf '  两个加起来等于条数吗：%s\n' "$([ "$((z + nz))" -eq "$n" ] && echo 等于 || echo 不等于)"
printf '  ★ 「码 2」会不会是恒零的假守卫？塞一条 2 进台账再撤掉：\n'
cp argscheck/rcs.txt argscheck/rcs.bak
printf '2\n' >> argscheck/rcs.txt
ring=$(grep -c '^2$' argscheck/rcs.txt)
mv argscheck/rcs.bak argscheck/rcs.txt
back=$(grep -c '^2$' argscheck/rcs.txt)
printf '    塞进去 → 从 %s 变成 %s（响了）；撤掉探针回到 %s\n' "$two" "$ring" "$back"
printf '  → 三条能带走的话：① 闸门是 $# 与「非 0 就 exit 并印 usage」，getopts 只负责分选项；\n'
printf '    ② 非静默模式下 ? 分支里 OPTARG 没设，想要坏字符就开静默，开了静默就得自己印 usage；\n'
printf '    ③ 交给外部命令之前把 -- 或 ./ 用上 —— 屏幕上 rm 那句原话就是它自己想让你做的（中英文同一句）。\n'
exit 0
