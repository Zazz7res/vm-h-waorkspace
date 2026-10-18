#!/usr/bin/env bash
# ============================================================================
# 第 08 章《写出炸不响的脚本：四件衣服、陷阱、三道边界》—— 整章证据一键复现
#
#   bash demo.sh            跑完自动删沙箱（默认）
#   bash demo.sh --keep     跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt  重定向时自动关掉颜色，README 引用的就是这份纯文本
#
# 屏幕上出现的每一行，都是这条命令在这台机器上的真实结果 —— 没有一行是手写的。
#
# 这一章要还的三笔账（六个取证脚本 + 下面 18 节全部挂在其中一条上）：
#   主线① **四件衣服各挡一面，谁也不顶谁**：-e 管「普通位置上的失败停不停」，
#          -u 管「没定义的名字被读」，-o pipefail 管「管道的码算不算前棒」，
#          -E 只管「ERR 陷阱进不进函数」。穿满四件，还有整整一类失败照旧穿过去。
#   主线② **陷阱是脚本最后一次说话的机会**：陷阱体里第一条赋值就把 $? 吞了；
#          EXIT 在信号杀死时还跑，KILL 时一个字都不跑；清理跑到一半再被打断就剩残留。
#   主线③ **边界只有三道**：进来的参数（getopts 与 --）、碰到的文件（测与开之间那一刀）、
#          走完全链的名字（空格 / 中文 / 换行 / 前导 -）。三道边界的共同点是**不报错**。
#
# 本章的取证路线（18 节，全部真跑）：
#   第 1 节   钉死环境：bash 版本、语言环境、六个工具的厂商、/bin/sh 真身、四件衣服的默认位置
#   第 2 节   一张地图：三条主线各一条最小证据
#   第 3 节   strictfour.sh 完整跑（四件衣服各挡什么、穿满还漏什么）
#   第 4 节   小证据：同一条管道在四种开关组合下的「码」与「停不停」
#   第 5 节   errescue.sh 完整跑（-e 的四位豁免与两个位置）
#   第 6 节   小证据：函数最后一条失败 —— 裸调用、放进 if、在子 shell 里
#   第 7 节   trapsafe.sh 完整跑（$? 什么时候被读、三种死法、半截清理）
#   第 8 节   小证据：陷阱体第一条赋值吞码，__rc=$? 打头救回来
#   第 9 节   argscheck.sh 完整跑（$# 那道闸、getopts 两种模式、-- 的位置）
#   第 10 节  小证据：同一批坏输入，非静默与静默 optstring 各怎么答
#   第 11 节  filesafe.sh 完整跑（判断跟谁、mktemp 权限、set -C、flock）
#   第 12 节  小证据：先测后开那一刀 —— 手动把「改指」插在测与写之间
#   第 13 节  weirdnames.sh 完整跑（怪名字走完整条链，只报实测个数）
#   第 14 节  小证据：rm 的四种写法排成一张真值表（引号 × --）
#   第 15 节  综合演练：一个能上线的脚本骨架 lib/skel.sh，九条输入挨个撞
#   第 16 节  把这个骨架交给 /bin/sh（dash）撞一遍
#   第 17 节  对账：六个脚本各几格、合计几格、demo 自己几节（守卫先故意弄响）
#   第 18 节  自证：章目录开跑前后一致、沙箱里只剩该有的东西、本章没写过联网与装包
#
# 安全说明：
#   * 所有写操作只发生在 mktemp -d 的沙箱里（前缀 bash08-），由 trap cleanup EXIT 整体删掉；
#     六个取证脚本各自只在自己的子目录里造文件，跑完自己删并当场自证。
#   * 绝不修改 ~/.bashrc、~/.profile、~/.config、~/.local、/etc：要看「另一个干净 shell」
#     的行为一律用 bash -c 起新进程；set -e / set -C / IFS 只在被演示的那个子进程里活着。
#   * 本章不起后台进程、不 sleep、不联网、不装包、不开监听端口；不 kill 任何进程，
#     第 7 节里被信号打中的进程全部是 trapsafe.sh 自己 bash -c 起的。
#   * 屏幕上不出现真实身份：pid、家目录绝对路径、毫秒耗时、每次不同的沙箱随机名都不上屏；
#     mktemp 造出的随机名一律不进屏幕（第 15 节的骨架只报「建了几个、删了几个」）。
#   * 耗时/完成顺序/内存占用一律不上屏：它们每跑必变，README 印了就永远对不上。
#     本章能给的确定性是「计数、长度、排序、布尔、退出码」这五种。
#
# 为什么这个脚本**故意不加** set -e：
#   本章一半的内容是失败现场：-e 掐死的那几个子进程、getopts 认出的坏选项、
#   rm 撞在 -dash.txt 上的原文报错、dash 不认 set -E、骨架故意少传参数的那两次。
#   加了 set -e，第 4 节第一条就把 demo 掐了。同理不加 set -u（演示里大量故意留空的变量），
#   也不全局开 pipefail —— 管道的码归第 4 节单独演，全局开会把 runc 的码改成管道末棒。
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

# 把语言环境钉死：这台机器默认 LANG=zh_CN.UTF-8、LANGUAGE=zh_CN:zh，bash 与 GNU 工具的报错是中文，
# 而 wc -m 的计数口径跟着 LC_ALL 走。报错那句话的语言由 LANGUAGE > LC_MESSAGES > LANG 决定
# （第 1 节拿同一句错误在三种组合下实测过），所以三个一起钉才有效。改动只发生在本脚本和它 fork 出的子进程里。
ORIG_LANG="${LANG:-（未设）}"
ORIG_LC_ALL="${LC_ALL:-（未设）}"
ORIG_LANGUAGE="${LANGUAGE:-（未设）}"
export LC_ALL=C.UTF-8 LANG=C.UTF-8 LANGUAGE=C
export ORIG_LANG ORIG_LC_ALL ORIG_LANGUAGE

# 输出被重定向到文件时（README 捕获就是这样跑的）不要 ANSI 颜色码：纯文本才能逐行核对。
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/bash08-XXXXXX")
BEFORE_LIST=$(cd "$HERE" && ls -A | sort | tr '\n' '|')

banner() { printf '\n══════ %s %s\n' "$1" "════════════════════════════════"; }
note() { printf '%s\n' "$1"; }

# 打印命令 → 用 bash -c 真跑 → 打印退出码。用 bash -c 是为了每一条都在一个
# **全新的非交互 bash** 里开始：上一节开的 set -e / IFS / 陷阱不会串到下一节。
# 2>&1 把子进程的 stderr 并进 stdout，所以「报错原文」也留在这份纯文本捕获里。
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}

# runc_at <目录> <命令>：先 cd 进那个目录再跑，屏幕上只留目录名（basename）+ 相对命令。
runc_at() {
    printf '\n%s$ 〔在 %s 里〕 %s%s\n' "$C" "$(basename "$1")" "$2" "$R"
    ( cd "$1" && bash -c "$2" ) 2>&1
    printf '[退出码 %d]\n' "$?"
    return 0
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去接着敲： cd "%s/work"     看完删掉： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] 前缀 bash08-（随机后缀不进屏幕；只有 --keep 时才印出完整路径）\n'
    fi
}
trap cleanup EXIT

# 本章的六个取证脚本 + 本 demo 自己复制进沙箱，所有加工都在副本上进行（脚本里写的都是相对路径）。
# ★ 必须用相对文件名跑（bash xxx.sh，不是 bash /绝对路径/xxx.sh）：bash 的报错原文会带上 $0，
#   跑绝对路径就把沙箱绝对路径印上屏了。
mkdir -p "$SB/work/lib"
cp "$HERE"/*.sh "$SB/work/"
cd "$SB/work" || exit 1
umask 022

# ============================================================================
banner "第 1 节 先把这台机器的发言权确认一遍"
# ============================================================================
note '# 本章每条结论都跟三样东西绑定：哪个 bash、哪个厂商的 rm/wc/awk、哪一套语言环境。'
note '#   前两件决定「有没有这个写法」，第三件决定「报错那句话用什么语言说」。第一步先把它们钉死，'
note '#   第 16 节再演「换个 shell 就不是同一套语法」。'
runc 'bash --version | head -1'
runc 'echo "demo 已把自己改成：LC_ALL=[$LC_ALL] LANG=[$LANG] LANGUAGE=[$LANGUAGE]（你原来的终端是 LANG=$ORIG_LANG，LC_ALL=$ORIG_LC_ALL，LANGUAGE=$ORIG_LANGUAGE）"'
note '# 本章会挨个用到的外部命令，各自是谁家的（第 9 节第 8 格那条报错来自 GNU rm，cat 却来自 uutils）：'
runc 'for c in rm cat wc tr sort head awk sed grep find xargs paste; do printf "  %-7s → %s\n" "$c" "$("$c" --version 2>&1 | head -1)"; done'
note '# ★ 报错那句话用什么语言，跟着 LANGUAGE > LC_MESSAGES > LANG 走（gettext 的优先顺序）；'
note '#   LC_ALL 盖得住后两个，盖不住已经设好的 LANGUAGE —— 同一句错误在三种组合下各长这样：'
runc 'mkdir -p loc && touch -- loc/-dash.txt
printf "  ① LC_ALL=C.UTF-8 + LANGUAGE=C（本章钉的就是这组）：%s\n" "$( (cd loc && env LC_ALL=C.UTF-8 LANGUAGE=C rm -f -dash.txt) 2>&1 | head -1)"
touch -- loc/-dash.txt
printf "  ② LC_ALL=zh_CN.UTF-8，把 LANGUAGE 撤掉：%s\n" "$( (cd loc && env -u LANGUAGE LC_ALL=zh_CN.UTF-8 rm -f -dash.txt) 2>&1 | head -1)"
touch -- loc/-dash.txt
printf "  ③ LC_ALL=zh_CN.UTF-8，但 LANGUAGE=C 还在：%s ← ★ 换语言没生效\n" "$( (cd loc && env LANGUAGE=C LC_ALL=zh_CN.UTF-8 rm -f -dash.txt) 2>&1 | head -1)"
touch -- loc/-dash.txt
printf "  bash 自己在 ② 那组下：%s\n" "$(env -u LANGUAGE LC_ALL=zh_CN.UTF-8 bash -c "while getopts \"a:b\" o; do :; done" _ -z 2>&1 | head -1)"
printf "  bash 自己在 ③ 那组下：%s\n" "$(env LANGUAGE=C LC_ALL=zh_CN.UTF-8 bash -c "while getopts \"a:b\" o; do :; done" _ -z 2>&1 | head -1)"
printf "  ★ 三次 rm 全都报错，那个文件还在吗：%s（错的就是没删成 —— 第 9、13 节各放大一次）\n" "$( [ -e loc/-dash.txt ] && echo 在 || echo 不在了 )"
printf "  你原来的终端是 LANG=%s、LANGUAGE=%s、LC_ALL=%s —— 所以单独跑本章脚本看到的是中文；\n" "$ORIG_LANG" "$ORIG_LANGUAGE" "$ORIG_LC_ALL"
printf "  demo 把 LC_ALL 与 LANGUAGE 一起钉住，这份捕获里才是英文。两种都是本机真实输出。\n"
rm -f -- loc/-dash.txt
rm -rf loc'
runc 'printf "  /bin/sh 的真身=%s\n" "$(readlink -f /bin/sh)"; echo "  ★ 第 16 节要让本章的骨架去撞一下它"'
runc 'command -v shellcheck >/dev/null 2>&1 && echo "shellcheck 在" || echo "shellcheck 本机没有 —— 本章不装东西，脚本只能靠逐条真跑来验"'
note '# 四件衣服在本机新进程里的默认位置（off=没穿）；第 3 节整节在量它们各挡什么：'
runc 'for o in errexit nounset pipefail errtrace noclobber; do printf "  %-12s %s\n" "$o" "$(set -o 2>/dev/null | awk "\$1==\"$o\"{print \$2}")"; done'
runc 'printf "  BASHOPTS=[%s] ｜ SHELLOPTS=[%s]\n" "${BASHOPTS-未设}" "${SHELLOPTS-未设}"'
runc 'printf "  空数组在 set -u 下展开（第 3 节第 5 格要用）→ %s\n" "$(bash -c "set -u; a=(); echo \"[\${a[@]}]\"" 2>&1)"'

# ============================================================================
banner "第 2 节 一张地图：三条主线各一条最小证据"
# ============================================================================
note '# 先把三笔账各亮一次，后面 16 节都在把它们放大到「为什么会这样」。'
note '# 主线①：四件衣服不互相顶班 —— 管道里前一棒失败，-e 单穿根本看不见。'
runc 'bash -c "set -e; false | true; echo 走到这里，码=\$?"'
note '# 主线②：陷阱体里第一条赋值就把 $? 吞了 —— 清理代码写在前，退出码就变了。'
runc 'trap "x=1; echo \"  EXIT 陷阱读到的码=[\$?]\"" EXIT; exit 7'
note '# 主线③：怪名字一路不报错，只在「个数」上露馅 —— 4 个名字，无引号展开变 7 段。'
runc 'a=("有 空格.txt" "中文 名.txt" "$(printf "带\n换行.txt")" "-dash.txt"); c=0; for f in ${a[@]}; do c=$((c + 1)); done; printf "  数组元素 %s 个，无引号循环体走 %s 次\n" "${#a[@]}" "$c"'

# ============================================================================
banner "第 3 节 strictfour.sh 完整跑：四件衣服各挡什么"
# ============================================================================
note '# 十一格：默认位置、-e 挡普通位置、-u 与 127/1 两个码、-u 读裸 $1、本机空数组不报错、'
note '#   pipefail 只改码、grep 没中的那一次、-e 单穿挡不住管道、-E 只管 ERR 进不进函数、'
note '#   穿满三件套仍然挡不住的四类、收账。'
runc 'bash strictfour.sh'

# ============================================================================
banner "第 4 节 小证据：同一条管道，四种开关组合下的「码」与「停不停」"
# ============================================================================
note '# 第 3 节第 6、8 格分别讲了「码」和「停」，这里把两件事并排放在同一条管道上。'
runc 'show(){ v=$(bash -c "$1" 2>&1); printf "    %-16s → %s\n" "$2" "${v:-（那行 echo 没跑到，脚本被掐了）}"; }
show "false | true; echo 码=\$?" 什么都不穿
show "set -e; false | true; echo 码=\$?" 只穿-e
show "set -o pipefail; false | true; echo 码=\$?" 只穿pipefail
show "set -eo pipefail; false | true; echo 码=\$?" 两件都穿
echo "  ★ 码由 pipefail 决定，停不停由 -e 决定；第 2 格与第 4 格的区别正好只有 pipefail。"'
note '# 反过来也一样：管道后棒成功、前棒失败，不加 pipefail 时 -e 根本没有非 0 可判 ——'
note '#   于是「穿了 -e 却什么都没挡」不是玄学，是码在这一路上就没变成非 0。'
runc 'bash -c "set -e; ls /no-such-dir-xyz | wc -l; echo 码=\$?" 2>&1 | tail -2'

# ============================================================================
banner "第 5 节 errescue.sh 完整跑：-e 的四位豁免"
# ============================================================================
note '# 十一格：普通位置确实会停、&& 左边、|| 左边、if 条件位、感叹号、函数最后一条失败不终止、'
note '#   ★ 同一个函数裸调用 vs 放进 if、被掐死时交出的码不一定是你 return 的、循环两个位置、'
note '#   命令替换在赋值位与取值位的两种下场、收账。'
runc 'bash errescue.sh'

# ============================================================================
banner "第 6 节 小证据：函数最后一条失败，三种摆法三种下场"
# ============================================================================
runc 'f(){ false; }
f
printf "  不带 -e：函数里最后一条失败，外面码=%s，脚本照跑（这行印出来了）\n" "$?"'
runc 'set -e; f(){ false; }; f; echo "  这行跑不到：裸调用 + -e → 当场结束"'
runc 'set -e; f(){ false; }; if f; then :; else echo "  同一个 f 放进 if 条件位：-e 让路，脚本还活着"; fi'
runc 'set -e; f(){ false; }; g(){ f; }; g; echo "  到不了：失败藏在被调用者的函数体里，-e 照样管"'
runc 'set -e; f(){ false; }; g(){ f; }; if g; then :; else echo "  把最外层 g 放进 if：整条调用链一起进条件位，-e 一路让路（这行印出来了）"; fi'
note '# ★ 第 4 与第 5 条是本章最常被误伤的形状：你的库函数在裸调用时能炸掉调用者，'
note '#   在 if 里就只是「告诉调用者一声」。第 15 节那个骨架里所有失败都写成 if 判断，不靠 -e 兜。'

# ============================================================================
banner "第 7 节 trapsafe.sh 完整跑：陷阱的三个时间差"
# ============================================================================
note '# 十一格：\$? 在定义时展开 vs 执行时展开、ERR 响一声照跑、-E×-e 四格并排、四位豁免对 ERR 同样有效、'
note '#   ★ 陷阱体第一条赋值吞码、trap - ERR 撤掉之后、TERM/INT/KILL 三种死法、挂了 TERM 就杀不动、'
note '#   清理跑到一半被打断（残留当场数）、dash 的两种报错原文、收账（含 128 以上的码几个）。'
runc 'bash trapsafe.sh'

# ============================================================================
banner "第 8 节 小证据：陷阱体第一条赋值就把 \$? 吞了"
# ============================================================================
note '# 第 7 节第 5 格给了结论，这里把「为什么」摊开成三行同一条命令：$? 是最后一次命令的状态，'
note '#   而陷阱体里的第一条语句通常就是赋值 —— 赋值本身成功，码立刻变 0。'
runc 'trap "echo \"  陷阱体第一条就是 echo（没人碰过退出码）：码=[\$?]\"" EXIT; exit 9'
runc 'trap "x=1; echo \"  陷阱体先写一句 x=1：码=[\$?]\"" EXIT; exit 9'
runc 'trap "__rc=\$?; x=1; echo \"  第一句把码抢进 __rc：[__rc=\$__rc] ｜ 但 echo 此刻再读退出码，已经是 [\$?]（被中间那句赋值顶掉了）\"" EXIT; exit 9'
note '# ★ 修法只有一条：陷阱体的第一句先 __rc=$?。第 15 节的骨架照这个写法钉死。'
note '#   注意第 1 条里那几个反斜杠：双引号定义陷阱时，\$? 才会留到执行那一刻（第 7 节第 1 格）。'

# ============================================================================
banner "第 9 节 argscheck.sh 完整跑：进来的参数先过三道闸"
# ============================================================================
note '# 九格：最外层的闸是 $#、-u 下裸 $1 的下场、getopts 最小骨架与 shift $((OPTIND-1))、'
note '#   -ab 打包的真实含义、非静默模式 ? 分支里 OPTARG 根本没设、静默模式两种事故靠一个字符分开、'
note '#   -- 会被带值选项当值吃掉、第一个非选项之后就停、以 - 开头的文件名的坏写法与两种正解、收账。'
runc 'bash argscheck.sh'

# ============================================================================
banner "第 10 节 小证据：同一批坏输入，非静默与静默 optstring 各怎么答"
# ============================================================================
note '# optstring 开头多写一个冒号，报错的人就从 bash 变成你自己 —— 差别是你能不能给出人话。'
runc 'G(){ while getopts "a:b" o; do case $o in
  a) echo "    取到 a=[${OPTARG-}]";;
esac; done; echo "    OPTIND=$OPTIND 剩下[$*]"; }
echo "  同一个进程里连调两次、中间没有 shift（写这一节时真踩到的坑）："
echo "  第一次："; G -a x
echo "  第二次："; G -a x
echo "  ★ 第二次那个 case 一声没出、OPTIND 还是 3 —— getopts 从第 OPTIND 个参数看起，"
echo "    上一次已经把它推到 3 了。函数里用 getopts，函数第一句得写 OPTIND=1。"'
runc 'G(){ OPTIND=1; while getopts "a:b" o; do case $o in
  a) echo "    取到 a=[${OPTARG-}]";; b) echo "    取到 b";;
  \?) echo "    落到 ?，OPTARG=[${OPTARG-（根本没设）}]";;
  :) echo "    落到 :，OPTARG=[${OPTARG-（根本没设）}]";;
esac; done; echo "    OPTIND=$OPTIND 剩下[$*]"; }
echo "  非静默（optstring 不带冒号），输入 -z x："; G -z x
echo "  非静默，输入 -a x（-a 要值，这里 -a 拿到 x）："; G -a x
echo "  非静默，输入 -a（缺值）："; G -a'
runc 'G(){ OPTIND=1; while getopts ":a:b" o; do case $o in
  a) echo "    取到 a=[${OPTARG-}]";; b) echo "    取到 b";;
  \?) echo "    落到 ?，OPTARG=[${OPTARG-（根本没设）}]";;
  :) echo "    落到 :，OPTARG=[${OPTARG-（根本没设）}]";;
esac; done; echo "    OPTIND=$OPTIND 剩下[$*]"; }
echo "  静默（optstring 开头写冒号），输入 -z x："; G -z x
echo "  静默，输入 -a x："; G -a x
echo "  静默，输入 -a（缺值）："; G -a
echo "  同一个函数连着调两遍（这一版每调一遍先复位一次）："; G -z x; G -a'
note '# ★ 「（根本没设）」不是脚本写漏了：非静默模式下 bash 自己报完错，? 分支里的 OPTARG 是未定义的，'
note '#   所以那句 ${OPTARG-…} 是必需的（穿了 set -u 时少写这一笔就是第 9 节第 1 格的下场）。'
note '#   顺带看一眼非静默那组的第三行：连「选项缺值」这种看着该落到 : 的事故，也照样落到 ?，'
note '#   因为静默模式（optstring 开头那个冒号）才是 : 分支唯一的入口。'

# ============================================================================
banner "第 11 节 filesafe.sh 完整跑：碰文件之前的那些刀"
# ============================================================================
note '# 九格：四个对象 × 六个判断的矩阵、mktemp 给的权限（随机名不上屏）、模板里 X 的规则、'
note '#   ★ 断链让 -e 说「不存在」而写下去凭空造文件、★ 测完到开写之间链接被改指、'
note '#   正解一 set -C（以及 >| 怎么破功）、正解二 flock 的锁跟着「打开的文件描述」走、'
note '#   %s 与 %q 的差别（名字回不回复现）、收账。'
runc 'bash filesafe.sh'

# ============================================================================
banner "第 12 节 小证据：把「改指」手动插在测与写之间"
# ============================================================================
note '# 第 11 节第 5 格那一刀是脚本自己演的（第三方改不了你的时序），这里用两个普通文件把它放大：'
note '#   测的那一刻为真，写的那一刻已经不是同一个对象了 —— 中间没有任何报错。'
runc 'mkdir -p t && cd t || exit 1
printf "%s\n" 原始内容 > important.txt
printf "%s\n" 别人的东西 > other.txt
ln -sf other.txt link
printf "  ① 测：[ -f link ] → %s ｜ [ -L link ] → %s\n" "$( [ -f link ] && echo 是 || echo 否)" "$( [ -L link ] && echo 是 || echo 否)"
printf "  ② 就在这一行与下一行之间，别人把 link 改指到 important.txt（本章手动插这一步）\n"
ln -sf important.txt link
printf "  ③ 脚本按 ① 的结论继续写：echo 新东西 > link\n"
echo 追加的内容 > link
printf "  结果 important.txt=[%s] ｜ other.txt=[%s]\n" "$(cat important.txt)" "$(cat other.txt)"
echo "  ★ 它写进了你「测过之后」才出现的那个对象，全程零报错。要么用 set -C / O_EXCL，要么别测了再开。"
cd .. && rm -rf t
printf "  现场已收拾：本目录还剩 %s 个多余目录\n" "$(ls -d */ 2>/dev/null | grep -vc '^lib/$')"'

# ============================================================================
banner "第 13 节 weirdnames.sh 完整跑：名字走完整条链"
# ============================================================================
note '# 八格：造 4 个并证明计数函数会响（NUL 4 vs ls|wc -l 5）、引号 4 次 vs 无引号 7 段、'
note '#   内层 "$@" 接住 4 个参数、"$*" 与 IFS 的粘合剂、IFS=换行的 5 段错觉、'
note '#   find|xargs 7 条 vs -print0|xargs -0 4 条、rm 的三跑真值表（含 rc=0 却少删）、收账。'
runc 'bash weirdnames.sh'

# ============================================================================
banner "第 14 节 小证据：rm 的四种写法排成真值表"
# ============================================================================
note '# 第 13 节第 7 格跑的是「数组交出去」那条链，这里把四种写法并排，只看两个数：码、还剩几个。'
runc 'make(){ rm -rf w; mkdir -p w; touch -- "w/有 空格.txt" "w/-dash.txt"; }
try(){ make; o=$(cd w && eval "$1" 2>&1); r=$?; printf "    写法：%s\n" "$1"; printf "      → 码=%s ｜ 剩=%s ｜ %s\n" "$r" "$(find w -type f | wc -l)" "$( [ -n "$o" ] && echo 有报错 || echo 一声没出)"; }
echo "  每次造的现场都是两个名字：带空格的那个 + 以 - 开头的那个（这里用 eval 只是为了排成表，真实脚本写的是字面命令）"
try "rm -f 有 空格.txt -dash.txt"
try "rm -f -- 有 空格.txt -dash.txt"
try "rm -f \"有 空格.txt\" -dash.txt"
try "rm -f -- \"有 空格.txt\" \"-dash.txt\""
echo "  ★ 其中 rm -f -- 有 空格.txt -dash.txt 那一条：码=0、一声没出，却少删了一个 ——
    rc=0 证明的是「没报错」，不是「办成了」。"
rm -rf w'
runc 'make(){ rm -rf w; mkdir -p w; touch -- "w/有 空格.txt" "w/-dash.txt"; }
make; rm -f -- "w/有 空格.txt" "w/-dash.txt"
printf "  同一个 make 之后按第 4 行写法删：剩 %s 个（判据是数出来的，不是猜的）\n" "$(find w -type f | wc -l)"
make; ( cd w && rm -f "有 空格.txt" "-dash.txt" ) 2>/dev/null; rc=$?
printf "  单引住第一个名字、不写 -- 的那一条：码=%s 剩 %s 个\n" "$rc" "$(find w -type f | wc -l)"
printf "  ★ 这一格写的时候真踩过坑：原来这里紧跟一句 cd ..，于是 \$? 读到的是 cd 的 0 —— 第 8 节讲的正是它。\n"
rm -rf w'
note '# ★ 引号管「不被劈开」，-- 管「不被当选项」：两件事各自独立，缺一个就少删一片。'

# ============================================================================
banner "第 15 节 综合演练：一个能上线的脚本骨架"
# ============================================================================
note '# 每条规矩都能回指到本章某一节：四件衣服穿满（第 3 节）、陷阱第一句抢码（第 8 节）、'
note '#   $# 当最外层闸 + 静默 getopts（第 9/10 节）、失败写进 if 不靠 -e 兜（第 6 节）、'
note '#   set -C 而不是先测后开（第 11/12 节）、名字全程数组形态 + --（第 13/14 节）。'
cat > lib/skel.sh <<'SKLEOF'
#!/usr/bin/env bash
# skel.sh —— 本章结论拼出来的最小可用骨架：给一批文件各写一份「行数报告」。
# 它故意不打印「一切正常」，只在最后给出实测计数；失败一律走 stderr + 显式退出码。
set -euEo pipefail                 # 四件衣服一次穿满（第 3 节）
__rc=0
work=''
cleanup() {
    __rc=$?                        # ★ 第一句先把码抢下来（第 8 节）
    if [ -n "$work" ]; then rm -rf -- "$work"; fi
    printf '[收尾] 退出码=%s ｜ 临时目录建在本目录，已删\n' "$__rc" >&2
}
trap cleanup EXIT
umask 077                          # 报告和临时目录默认不给别人读写

usage() { printf '用法: %s [-n 行数上限] [-q] 文件...\n' "$0" >&2; }

quiet=0 max=3
while getopts ':n:qh' o; do        # 首字符冒号 = 静默模式，报错自己写（第 10 节）
    case $o in
        n) max=$OPTARG ;;
        q) quiet=1 ;;
        h) usage; exit 0 ;;
        :) printf '选项 -%s 少了一个值\n' "$OPTARG" >&2; exit 2 ;;
        \?) printf '不认识的选项 -%s\n' "${OPTARG-（OPTARG 未设）}" >&2; exit 2 ;;
        *) printf '不该落到这里 -%s\n' "$o" >&2; exit 2 ;;
    esac
done
shift $((OPTIND - 1))

[ "$#" -gt 0 ] || { usage; exit 2; }   # ★ 最外层那道闸是 $#（第 9 节）

work=$(mktemp -d ./skel-XXXXXX)        # 建在当前目录，跑完自己删；随机名不进屏幕
done_n=0
skip_n=0
for f in "$@"; do                       # 名字全程保持「一个元素」的形态（第 13 节）
    if [ ! -f "$f" ]; then
        printf '跳过（不是普通文件）：%s\n' "$f" >&2
        skip_n=$((skip_n + 1))
        continue
    fi
    n=$(wc -l < "$f")
    if [ "$n" -gt "$max" ]; then
        printf '超过上限：%s（%s 行 > %s）\n' "$f" "$n" "$max" >&2
    fi
    dest="$work/r$done_n.rpt"
    ( set -C; printf '行数=%s\n来源=%s\n' "$n" "$f" > "$dest" )   # 不覆盖已存在的（第 11 节）
    done_n=$((done_n + 1))
    [ "$quiet" -eq 1 ] || printf '已报告：%s → %s 行\n' "$f" "$n"
done

printf '[完成] 收到 %s 个操作数 ｜ 写了 %s 份报告 ｜ 跳过 %s 个\n' "$#" "$done_n" "$skip_n"
[ "$skip_n" -eq 0 ] || exit 1           # 有跳过就报非 0，让调用方能判
SKLEOF
cp lib/skel.sh .
runc_at . 'touch -- "有 空格.txt" "-dash.txt"; bash skel.sh -- "有 空格.txt" "-dash.txt"'
runc_at . 'bash skel.sh "有 空格.txt" "-dash.txt"'
runc_at . 'bash skel.sh 有 空格.txt -dash.txt'
runc_at . 'printf "a\nb\nc\n" > 三个.txt; printf "x\n" > 一个.txt; bash skel.sh -n 2 三个.txt 一个.txt'
runc_at . 'bash skel.sh -q 一个.txt'
runc_at . 'bash skel.sh'
runc_at . 'bash skel.sh -x 一个.txt'
runc_at . 'bash skel.sh -n'
runc_at . 'bash skel.sh -n 1 -- 一个.txt'
note '# ★ 上面九条是骨架的全部输入面：怪名带 --、怪名不带 --、怪名连引号也没了，再是超限、安静、空、'
note '#   坏选项、缺值、带值选项配 --。三条怪名三种下场：第 1、2 条的码都是 0，因为这个骨架从不把'
note '#   名字交给「会读选项」的命令，-dash.txt 落到操作数位上没人把它当选项；第 3 条引号一丢，两个'
note '#   名字被劈成三段（屏幕上实测：收到 3 个操作数、跳过 2 个），脚本自己交出 rc=1。'
note '#   换到第 14 节就不一样了：rm 会自己读选项，那边实测有一行是「码=0、一声没出，却少删了一个」。'
runc_at . 'printf "  跑完九条之后，本目录里残留的 skel-* 临时目录数：%s（守卫先响：下面故意造一个）\n" "$(ls -d skel-* 2>/dev/null | wc -l)"
mkdir skel-故意留下
printf "  故意造一个之后：%s ← 数得出来，所以上一条的 0 才算数\n" "$(ls -d skel-* 2>/dev/null | wc -l)"
rmdir skel-故意留下
printf "  撤掉探针之后：%s\n" "$(ls -d skel-* 2>/dev/null | wc -l)"'

# ============================================================================
banner "第 16 节 把这个骨架交给 /bin/sh（dash）撞一遍"
# ============================================================================
note '# 同一个文件、同一句调用，换 shell 就换一套语法。先让 dash 真跑一遍本章骨架，再逐条撞它不认的东西。'
runc_at . 'sh skel.sh 一个.txt'
runc_at . 'printf "l1\nl2\n" > "有 空格.txt"; touch -- "-dash.txt"; sh skel.sh -- "有 空格.txt" "-dash.txt"'
runc_at . 'sh -c "set -euEo pipefail; echo 走到这里"; printf "  码=%s ｜ -e -u -E -o pipefail 这四件 dash 全收下\n" "$?"'
runc_at . 'sh -c "false | true; echo 不加码=\$?; set -o pipefail; false | true; echo 加了pipefail=\$?"'
runc_at . 'sh -c "trap \"echo 〔dash 里 ERR 陷阱〕\" ERR; false"; printf "  码=%s ｜ 这就是第 7 节第 10 格那条原文\n" "$?"'
runc_at . 'sh -c "a=1; echo [\${a[@]}]"; printf "  码=%s（bash 的数组展开在 dash 里是 Bad substitution）\n" "$?"'
runc_at . 'bash -c "set -u; a=(); echo [\${a[@]}]"; printf "  同一个写法在 bash 5.3 里=%s（第 3 节第 5 格）\n" "$?"'
note '# ★ 结论不是「别用 dash」，也不是「bash 写法一律不能移植」：这条骨架 dash 能整份跑完，'
note '#   是因为它恰好一个 bash 专属特征都没用到（没有数组、没有 [[ ]]、没有 ERR 陷阱、没有 local -n）。'
note '#   真正的分界就在上面那几行原文：set -o pipefail 它认，而且照样把管道的码改成 1'
note '#   （第 4 节那张「码由 pipefail 决定」的表在 dash 里同样成立）；'
note '#   trap ... ERR 它直接 bad trap —— 第 7 节整节的写法到 dash 里就没了。'
note '#   要把脚本交给 /bin/sh，就得整份退回 POSIX 写法（第 01 章那条 shebang 的意义就在这）。'

# ============================================================================
banner "第 17 节 对账：六个脚本各几格、合计几格、demo 自己几节"
# ============================================================================
note '# 先看守卫能不能响：故意用一个匹配不到任何东西的模式跑一遍，六个 0 —— 那这守卫是假的。'
runc 'for f in strictfour errescue trapsafe argscheck filesafe weirdnames; do
  printf "  %-12s 假守卫计数=%s\n" "$f" "$(grep -c "=== ZZZ-NOT-THERE " "$f.sh")"
done
echo "  ★ 全 0 —— 所以下面这条真计数必须非 0，否则同样不可信"'
runc 'total=0
for f in strictfour errescue trapsafe argscheck filesafe weirdnames; do
  n=$(grep -c "^echo .=== " "$f.sh")
  printf "  %-12s %s 格\n" "$f" "$n"
  total=$((total + n))
done
printf "  ★ 六个脚本合计 %s 格（匹配式写成 ^echo .=== ：两种引号的分格标题都算，漏一种就会少数）\n" "$total"'
runc 'printf "  demo 自己的节标题数（grep ^banner）：%s\n" "$(grep -c "^banner " demo.sh)"
printf "  沙箱 work 里除 demo 外的脚本数（含本章现造的骨架）：%s\n" "$(ls *.sh | grep -vc "^demo.sh$")"'
runc 'printf "  源文件行数：demo=%s ｜ 骨架 skel.sh=%s ｜ 六脚本=%s\n" "$(wc -l < demo.sh)" "$(wc -l < skel.sh)" "$(cat strictfour.sh errescue.sh trapsafe.sh argscheck.sh filesafe.sh weirdnames.sh | wc -l)"'
note '# 这里数的「格」是脚本源码里的分格标题行；每一格都在屏幕上真实出现过（第 3~14 节逐条跑过）。'

# ============================================================================
banner "第 18 节 自证：沙箱里的事只留在沙箱"
# ============================================================================
note '# ① 章目录（沙箱外面）开跑前后各列一次条目名，比对是否一模一样。'
AFTER_LIST=$(cd "$HERE" && ls -A | sort | tr '\n' '|')
printf '  开跑前 %s 个条目 ｜ 现在 %s 个条目 → %s\n' \
    "$(printf '%s' "$BEFORE_LIST" | tr '|' '\n' | grep -c .)" \
    "$(printf '%s' "$AFTER_LIST" | tr '|' '\n' | grep -c .)" \
    "$([ "$BEFORE_LIST" = "$AFTER_LIST" ] && echo 一模一样 || echo "不一样：[$BEFORE_LIST] vs [$AFTER_LIST]")"
note '# ② 每个取证脚本自己的现场子目录是否都删干净了（先故意造一个，让这道守卫响一次）。'
runc 'mkdir -p zz-故意留下的目录
printf "  故意造一个多余目录之后，多余目录数=%s（★ 守卫响了）\n" "$(ls -d */ 2>/dev/null | grep -v "^lib/$" | wc -l)"
rmdir zz-故意留下的目录
printf "  删掉之后，多余目录数=%s（六个脚本的子目录都由它们自己的 trap 清掉了）\n" "$(ls -d */ 2>/dev/null | grep -v "^lib/$" | wc -l)"'
note '# ③ 沙箱 work 里现在还剩什么（只印相对条目名）。'
runc 'ls -A | sort | tr "\n" " "; echo'
runc 'printf "  lib/ 里剩：%s\n" "$(ls -A lib | sort | tr "\n" " ")"'
note '# ④ 本章没写过联网、装包、开监听端口的语句。扫描对象是六个取证脚本 + 骨架。'
note '#   守卫先要能响，这个 0 才算数 —— 所以这里插一个含禁词的假脚本把守卫弄响，再撤掉。'
runc 'pat="curl|wget|nc -l|nohup|pip install|apt-get|yum install"
list=(strictfour.sh errescue.sh trapsafe.sh argscheck.sh filesafe.sh weirdnames.sh skel.sh lib/skel.sh)
printf "curl -s https://example.com\napt-get install x\n" > zz-probe.sh
printf "  ④-1 先弄响：把探针 zz-probe.sh 算进来，命中文件数=%s\n" "$(grep -lE "$pat" "${list[@]}" zz-probe.sh | wc -l)"
rm zz-probe.sh
printf "  ④-2 撤掉探针：同一道守卫命中文件数=%s（★ 响过一次，这个 0 才算数）\n" "$(grep -lE "$pat" "${list[@]}" | wc -l)"'
runc 'pat="curl|wget|nc -l|nohup|pip install|apt-get|yum install"
list=(strictfour.sh errescue.sh trapsafe.sh argscheck.sh filesafe.sh weirdnames.sh skel.sh lib/skel.sh)
printf "  ④-3 假阳性对照：把 demo.sh 也扫进来，命中文件数=%s\n" "$(grep -lE "$pat" "${list[@]}" demo.sh | wc -l)"
printf "     ↑ 多出来的那一个就是 demo.sh —— 禁词的字面文本就写在它自己那行 pat= 里。\n"
printf "     任何「扫源码找禁词」的守卫都会自匹配，所以扫描清单必须把持有模式串的那个文件挑出去。\n"'
runc 'printf "  本 shell 的后台作业数=%s（本章没起过 & 作业）\n" "$(jobs -l | wc -l)"'
note '# ⑤ 屏幕上不该有真实身份。匹配串故意写成 /hom[e]/ 带方括号：'
note '#   不带方括号时，它会命中「写着它自己的那一行」（永远非 0 的假阳性，④-3 就是活例子）；'
note '#   带了方括号，模式文本不再自匹配，只有真实路径才算命中。同样先拿假想路径把守卫弄响。'
runc 'list=(strictfour.sh errescue.sh trapsafe.sh argscheck.sh filesafe.sh weirdnames.sh skel.sh lib/skel.sh demo.sh)
printf "/hom%s/somebody/nowhere\n" "e" > zz-path-probe.txt
printf "  ⑤-1 先弄响：把含假想绝对路径的探针算进来，命中=%s 处\n" "$(grep -o "/hom[e]/[A-Za-z0-9._-]*" "${list[@]}" zz-path-probe.txt | wc -l)"
rm zz-path-probe.txt
printf "  ⑤-2 撤掉探针：连 demo.sh 自己一起扫，命中=%s 处（★ 响过一次，这个 0 才算数）\n" "$(grep -o "/hom[e]/[A-Za-z0-9._-]*" "${list[@]}" | wc -l)"'
note '# ⑤-3 这一节只印「命中几处」，不印命中的内容：grep -o 会把匹配到的路径原样送上屏幕，'
note '#   数一遍就够 —— 想把内容印出来，等于亲手把家目录打到捕获里（所以第 3~17 节全用相对文件名跑）。'
note '# ⑥ 真实身份的整屏扫描（用户名/主机名/家目录）由 README「怎么跑这一章」那道闸门在捕获外面做。'
printf '  demo 自己跑到这里，最后一节的退出码=%s\n' "$?"

exit 0
