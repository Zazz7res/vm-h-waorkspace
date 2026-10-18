#!/usr/bin/env bash
# ============================================================================
# 第 07 章《文本三剑客：grep / sed / awk 各管哪一段》—— 整章证据一键复现
#
#   bash demo.sh            跑完自动删沙箱（默认）
#   bash demo.sh --keep     跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt  重定向时自动关掉颜色，README 引用的就是这份纯文本
#
# 屏幕上出现的每一行，都是这条命令在这台机器上的真实结果 —— 没有一行是手写的。
#
# 这一章要还的三笔账（十个取证脚本 + 下面 25 节全部挂在其中一条上）：
#   主线① **三个工具的单位不一样**：grep 的单位是「行」（它只会给你整行，不会给你片段），
#          sed 的单位也是「行」但它默认把每一行原样打印（p 是"额外再打一遍"），
#          awk 的单位是「一行里的字段」（给任意 $n 赋值 = 整行按 OFS 重建，原对齐当场阵亡）。
#   主线② **退出码表不表结果，三家各不相同**：grep 是三态（0 命中 / 1 没有 / 2 出错），
#          sed 的 0 既包括"改成功"也包括"一个字都没改"，awk 除非自己出错否则恒 0；
#          管道的码默认由**最后一环**说了算，而 -i 家族、就地改写、静默失败全都藏在这些缝里。
#   主线③ **"没报错"永远不等于"按你的意思生效了"**：本机 awk 是 mawk，gawk 的 FIELDWIDTHS
#          和 /\s/ 在它这里是两声不吭的错（rc=0），而同机的 GNU grep/sed 把 \s 读成空白。
#          所以每一类写法都要现场验收：数 NF、打出来对内容、先让守卫故意响一次。
#
# 本章的取证路线（25 节，全部真跑）：
#   第 1 节   钉死身份：bash / grep / sed / awk 各是谁、软链指向哪、哪些东西本机根本没有
#   第 2 节   一张地图：三条主线各一条最小证据
#   第 3 节   grepdialect.sh 完整跑（一个 grep 里三套正则引擎、\| 与 | 相反、0/1/2 三态）
#   第 4 节   小证据：模式本身以 - 开头时被当成开关，grep 一声不响改读 stdin
#   第 5 节   greppat.sh 完整跑（整行 vs -o、数行 vs 数次数、末行无换行、中文切段）
#   第 6 节   小证据：-o 不回头重叠 —— 四个 a 里取 aa 得 2 段不是 3 段
#   第 7 节   grepfix.sh 完整跑（-q vs -c、三态结构、-v、多 pattern、上下文分隔行、pipefail）
#   第 8 节   小证据：递归时 --include / --exclude-dir 挑文件，grep 默认会扫进 .git
#   第 9 节   sedflow.sh 完整跑（默认全打印、g/2/&、跨行看不到、地址范围关门、d/D/=/w）
#   第 10 节  小证据：s 的 I 标志（忽略大小写）—— GNU sed 认，POSIX 不保证
#   第 11 节  sedinplace.sh 完整跑（-i.bak/-i/-i 空格、inode 与硬链接、mode 照抄、幂等、自毁）
#   第 12 节  小证据：BSD 的 sed -i "" 撞本机 GNU sed 的下场 + sed 不替你补末尾换行
#   第 13 节  pipe3.sh 完整跑（三环单位错位、换行符=分帧、管道码归最后一环、空输入三态度）
#   第 14 节  小证据：同一根"前失败后成功"的管道，在 bash/dash 与开不开 pipefail 之间四个码
#   第 15 节  threeway.sh 完整跑（同一任务三种写法逐字一样 / 各掉在哪一段）
#   第 16 节  awkjoin.sh 完整跑（数组自增长、NR 与 FNR、建表查表、读一下就长元素）
#   第 17 节  小证据：awk 没有二维数组，a[x,1] 的键是被 SUBSEP 粘起来的一个字符串
#   第 18 节  awkframe.sh 完整跑（FS 决定字段数、赋值即整行重建、逗号走 OFS、空字段占坑）
#   第 19 节  小证据：给越界的 $5 赋值 —— NF 自己长到 5，中间空字段用 OFS 垫出来
#   第 20 节  awkmawk.sh 完整跑（会叫的缺件 rc=2 / 不会叫的 FIELDWIDTHS 与 /\s/ rc=0）
#   第 21 节  小证据：同一个 18 位整数走四条路 —— 进算术的两条末两位变 80，当串搬的那条原样
#   第 22 节  综合演练：一份写死的脏日志，grep 选 → sed 洗 → awk 汇总，三道守卫各先响一次
#   第 23 节  三张码表并排：同一件事 grep / sed / awk 各给什么码（含语法错与空输入）
#   第 24 节  对账：十个脚本各几格、合计 75 格、demo 自己几节（守卫先故意弄响）
#   第 25 节  自证：章目录开跑前后一致、沙箱里只剩该有的东西、本章没演什么
#
# 安全说明：
#   * 所有写操作只发生在 mktemp -d 的沙箱里（前缀 bash07-），由 trap cleanup EXIT 整体删掉；
#     本章十个取证脚本各自只在自己的子目录里造文件，跑完自己删并当场自证。
#   * 绝不修改 ~/.bashrc、~/.profile、~/.config、~/.local、/etc；不改仓库里任何既有文件。
#   * 不联网、不装包、不开监听端口、不起后台进程、不 sleep、不 kill 任何进程。
#   * 屏幕上不出现真实身份：家目录绝对路径、用户名、主机名、pid、每次不同的沙箱随机名都不上屏；
#     工具报错原文只在「带相对文件名」的那几格里才印，会带上随机临时名的那一律只印码。
#   * 耗时、内存占用、完成顺序、awk 数组遍历顺序一律不上屏：它们每跑必变。
#     需要顺序的地方全部外接 sort。本章能给的确定性是「计数、长度、排序、逐字节比较、退出码」。
#
# 为什么这个脚本**故意不加** set -e：
#   本章一半的内容是失败现场：grep 没命中退 1、sed 语法错退 1、awk 缺件退 2、
#   就地改写把文件清空却退 0。加了 set -e，第 2 节第一条就把 demo 掐了，
#   第 23 节的码表更是只能剩下第一行。同理不加 set -u，也不在顶层开 pipefail
#   （pipefail 只在第 14 节于子进程里临时开，开在哪、关在哪都写在屏幕上）。
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

# 语言环境钉死成「英文 + UTF-8」：本机默认 zh_CN.UTF-8，三个工具的报错原文会变成中文，
# 而 README 要逐字引用这些原文；grep -i 的大小写表、tr 的字节口径也跟着 locale 走。
ORIG_LANG="${LANG:-（未设）}"
ORIG_LC_ALL="${LC_ALL:-（未设）}"
export LC_ALL=C.UTF-8 LANG=C.UTF-8 LANGUAGE=C
export ORIG_LANG ORIG_LC_ALL

# 输出被重定向到文件时（README 捕获就是这样跑的）不要 ANSI 颜色码：纯文本才能逐行核对。
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/bash07-XXXXXX")

# 章目录的开跑前快照（只取条目名，第 25 节再取一次比对）。
BEFORE_LIST=$(cd "$HERE" && ls -A | sort | tr '\n' '|')

SCRIPTS="grepdialect greppat grepfix sedflow sedinplace pipe3 threeway awkjoin awkframe awkmawk"

banner() { printf '\n══════ %s %s\n' "$1" "════════════════════════════════"; }
note() { printf '%s\n' "$1"; }

# 打印命令 → 用 bash -c 真跑 → 打印退出码。用 bash -c 是为了每一条都在一个
# **全新的非交互 bash** 里开始：上一节改的 shopt / 建的函数不会串到下一节。
# 2>&1 把子进程的 stderr 并进 stdout，所以「报错原文」也留在这份纯文本捕获里。
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去接着敲： cd "%s/work"     看完删掉： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$(basename "$SB")"
    fi
}
trap cleanup EXIT

# 本章的十个辅助脚本 + 本 demo 自己复制进沙箱，所有加工都在副本上进行（脚本里写的都是相对路径）。
# ★ 必须用相对文件名跑（bash xxx.sh，不是 bash /绝对路径/xxx.sh）：工具的报错原文会带上 $0，
#   跑绝对路径就把家目录绝对路径印上屏了。
mkdir -p "$SB/work"
cp "$HERE"/*.sh "$SB/work/"
cd "$SB/work" || exit 1
umask 022

# ============================================================================
banner "第 1 节 先把这三个工具的身份钉死"
# ============================================================================
note '# 本章每一条「本机实测」都挂在这一节上：awk 尤其容易骗人 —— /usr/bin/awk 只是别人家的马甲。'
runc 'bash --version | head -1'
runc 'for t in grep sed awk sort cat wc tr head find xargs; do printf "  %-6s → %s\n" "$t" "$(readlink -f "$(command -v "$t")")"; done'
note '# ★ 看第二列：本章的 grep/sed/awk 来自 /usr/bin，而 sort/cat/wc/tr/head 来自 coreutils 的另一套。'
note '#   所以「GNU 家怎么行为」不能整体外推：三剑客是 GNU 的，配套的切行数字工具不是。'
runc 'grep --version | head -1; sed --version | head -1; awk --version | head -1'
note '# 这几样本章一律按「本机没有」处理（装不装是另一件事，本章不装东西）：'
runc 'for t in gawk sponge pcre2grep shellcheck; do printf "  %-11s %s\n" "$t" "$(command -v "$t" || echo 本机没有)"; done'
runc 'printf "  /bin/sh 的真身=%s\n" "$(readlink -f /bin/sh)"; echo "  ★ 第 14 节要让同一根管道去撞一下它"'
runc 'echo "  dash 想要 --version 的话，原话是："; dash --version 2>&1 | head -1'
runc 'printf "  语言环境：demo 已把自己钉成 LC_ALL=[%s] LANG=[%s]（你原来的终端是 LANG=%s、LC_ALL=%s）\n" "$LC_ALL" "$LANG" "$ORIG_LANG" "$ORIG_LC_ALL"'
runc 'printf "  umask=%s ｜ 沙箱里的样本文件权限都从这个数长出来\n" "$(umask)"'
runc 'printf "  交互 shell 里有 grep 的 --color alias，但脚本里没有：shopt expand_aliases → %s\n" "$(shopt -q expand_aliases && echo on || echo off)"'

# ============================================================================
banner "第 2 节 一张地图：三条主线各一条最小证据"
# ============================================================================
note '# 先把三笔账各亮一次，后面 22 节都在把它们放大到「为什么会这样」。'
note '# 主线①之一：grep 的单位是行 —— 你要的是「90」这一段，它给你的是整行。'
runc 'printf "Ada 90\nAlan 88\n" | grep 90'
note '# 主线①之二：sed 不写 -n 时每行都原样打印，所以 s 之后又冒出一遍；-n 才是"只打我说的"。'
runc 'printf "a\nb\n" | sed "s/a/A/"'
runc 'printf "a\nb\n" | sed -n "s/a/A/p"'
note '# 主线①之三：awk 的单位是字段，默认按空白切，所以它能干 grep/sed 干不了的「比大小、求和」。'
runc 'printf "Ada 90\nAlan 88\n" | awk "{ s += \$2 } END { print s }"'
note '# 主线②：同一件事三个码 —— 命中/没有/出错，grep 是三态，sed 与 awk 都不是。'
runc 'printf "error here\nplain\n" > zz-map.txt
printf "  grep 命中码=%s ｜ grep 没命中码=%s ｜ sed 没命中也码=%s\n" \
  "$(grep -c error zz-map.txt >/dev/null; echo $?)" "$(grep -c zzz zz-map.txt >/dev/null; echo $?)" \
  "$(sed -n "s/zzz/X/p" zz-map.txt; echo $?)"
rm -f zz-map.txt'
note '# 主线③：本机 awk 是 mawk，gawk 的写法在它这里可以一声不响（详见第 20 节整格）。'
runc 'printf "abcdefgh\n" | awk "BEGIN{ FIELDWIDTHS=\"3 3\"; } { print \"  屏幕上=[\" \$0 \"][\" \$2 \"] NF=\" NF }"; printf "  ↑ 这条 awk 自己的码=%s（★ gawk 文档里这一刀该切成 NF=3）\n" "$?"'

# ============================================================================
banner "第 3 节 grepdialect.sh 完整跑：一个 grep 里装着三套正则引擎"
# ============================================================================
note '# 7 格路线：开关决定用哪套引擎 → 「或」在 BRE 里写 \\| 在 ERE 里写 |（正好相反）'
note '#   → + ? {n,m} 在 BRE 里要戴反斜杠 → 只有 PCRE 认的写法到 ERE/BRE 不报错，\\d 还给你错的行 → . 与 -F 与 -w 的分工'
note '#   → 0/1/2 三态各撞一次 → 收口。每一格都多印一列「开关」：标签写错、开关没传，屏幕上自己露出来。'
runc 'bash grepdialect.sh'

# ============================================================================
banner "第 4 节 小证据：模式自己以 - 开头时，grep 把它当成开关"
# ============================================================================
note '# grepfix 教了「问有没有用 -q」，这一节补一刀更常见的：要匹配的串以减号开头。'
note '# 直觉是「grep -f file 就是找 -f 这两个字符」——错，-f 的意思是「模式在这个文件里」。'
runc 'printf -- "-f flag\nfoo bar\n" > zz-sw.txt
printf "  样本两行都含 -f 这两个字符\n"
printf "  grep -f zz-sw.txt（想匹配 -f）→ 命中 %s 行，rc=%s\n" \
  "$(grep -f zz-sw.txt < zz-sw.txt | wc -l)" "$(grep -f zz-sw.txt < zz-sw.txt >/dev/null; echo $?)"
printf "  ★ 它把你的文件名吞成了 -f 的参数，然后开始读 stdin —— 一个字的报错都没有\n"
printf "  正确写法一：grep -e \"-f\" 文件 → 命中 %s 行\n" "$(grep -e "-f" zz-sw.txt | wc -l)"
printf "  正确写法二：grep -- \"-f\" 文件 → 命中 %s 行\n" "$(grep -- "-f" zz-sw.txt | wc -l)"
printf "  对照：把 stdin 换成空设备，同一道命令就谁都不匹配（rc=%s）→ 静默失败全套形状\n" "$(grep -f zz-sw.txt < /dev/null >/dev/null; echo $?)"
rm -f zz-sw.txt'
note '# 这条判据跨工具通用：sed/awk 也认 --，凡是自己可能以减号开头的串（用户输入、变量）都放 -- 后面。'

# ============================================================================
banner "第 5 节 greppat.sh 完整跑：grep 给的是整行，数的是行"
# ============================================================================
note '# 8 格路线：整行 vs -o 片段 → 一行命中两次时 -c 与 -o|wc -l 分家 → 四把尺量同一份样本'
note '#   → \b \< \> [[:space:]] 本机实测 → 把点写成任意字符的静默多命中 → 末行无换行（grep 与 wc 口径分家）'
note '#   → 中文一行里 -o 切出来的是什么 → 收口。'
runc 'bash greppat.sh'

# ============================================================================
banner "第 6 节 小证据：-o 不回头重叠"
# ============================================================================
note '# greppat 第 2 格数了「行 vs 次」，这一节补一把尺：同一份文本里，-o 的段数会比你数少。'
runc 'printf "aaaa\n" > zz-oo.txt
printf "  四个连续 a：-o 取 aa → %s 段 ｜ -c → %s 行\n" "$(grep -o aa zz-oo.txt | wc -l)" "$(grep -c aa zz-oo.txt)"
printf "  为什么不是 3 段：吃完第 1、2 个 a 之后，扫描指针停在第 3 个 a，从这里起算\n"
printf "  换个写法就换个数：-o \"aa*\"（贪婪吃掉整串）→ %s 段\n" "$(grep -o "aa*" zz-oo.txt | wc -l)"
rm -f zz-oo.txt'
note '# 判据：段数跟着「你怎么写正则」走，不跟着「文本里实际有几个 aa」走 —— 数次数之前先定死模式。'

# ============================================================================
banner "第 7 节 grepfix.sh 完整跑：把 grep 用在管道和判断里"
# ============================================================================
note '# 8 格路线：-q 与 -c 的分工 → 0/1/2 三态写成能用的结构 → -v 滤干净反而退 1'
note '#   → 多 pattern 三种写法（-f 文件里的空行会放行整份）→ -A/-B/-C 的那个 -- 会被下游当数据'
note '#   → 递归与带空格的文件名 → -m 1 与 -q 各自少做了什么 → 收口。'
runc 'bash grepfix.sh'

# ============================================================================
banner "第 8 节 小证据：递归时「挑哪些文件」是另一套开关"
# ============================================================================
note '# grepfix 第 6 格演的是「-l 的单位是文件」，这一节补挑文件的三把钥匙，顺带一个意外。'
runc 'mkdir -p zz-tree/sub zz-tree/.git
printf "hit\n" > zz-tree/a.txt; printf "hit\n" > zz-tree/b.log; printf "hit\n" > zz-tree/sub/c.txt; printf "hit\n" > zz-tree/.git/d.txt
printf "  含 hit 的文件（不分类型、不进任何排除）：%s 个\n" "$(grep -rl hit zz-tree | wc -l)"
printf "  --include=*.txt → %s 个 ｜ 再加 --exclude-dir=.git → %s 个\n" \
  "$(grep -rl --include=*.txt hit zz-tree | wc -l)" \
  "$(grep -rl --include=*.txt --exclude-dir=.git hit zz-tree | wc -l)"
printf "  ★ 第一行那 %s 个里有版本库里的：grep -r 不会替你排 .git\n" "$(grep -rl hit zz-tree | wc -l)"
printf "  反过来，把目录名写进模式里是没用的：grep -r \"sub/c\" zz-tree → 命中 %s 个文件\n" \
  "$(grep -rl "sub/c" zz-tree | wc -l)"
rm -rf zz-tree'
note '# 模式管「行里的内容」，--include/--exclude 管「文件名」，两套开关不能互相代劳。'

# ============================================================================
banner "第 9 节 sedflow.sh 完整跑：sed 每一行都过一遍流水线"
# ============================================================================
note '# 8 格路线：默认全打印，p 是"再打一遍" → 一行里换几个（g/2/&）→ 被换行劈开的文本看不见'
note '#   → 循环把整文件拼成一条、-z 把整文件当"一行" → 地址范围打到结束就关门 → 分隔符与方言'
note '#   → d/D/=/w 四种下场 → 收口（含 sed 语法错退 1 与 grep 退 2 的对照原文）。'
runc 'bash sedflow.sh'

# ============================================================================
banner "第 10 节 小证据：s 的地址后面还能挂 I —— GNU 认，POSIX 不保证"
# ============================================================================
note '# sedflow 演了 g 与 2 与 &，这一节补最后一个常用标志：忽略大小写。'
runc 'printf "ERROR error eRRoR plain\n" | sed "s/error/E/g"'
runc 'printf "ERROR error eRRoR plain\n" | sed "s/error/E/gI"'
note '# 一行的差别：写 I 之前只剩 2 个 E，写了 I 三种大小写全被认出来（★ 同一份输入，两个输出）。'
note '# I（以及小写 i）在 POSIX 里没有定义，所以它是「各家自己决定要不要给」的那一类 ——'
note '#   本机两台 sed 恰好都给（上面是 GNU sed 4.9，下面是 busybox 的 sed），但这句话到别的机器上就失效了：'
runc 'printf "  本机 busybox 的 sed 认不认 I → "; printf "ERROR\n" | busybox sed "s/error/E/I" 2>&1 || true'
note '# 判据回指第 20 节：跨工具的写法（标志、转义、扩展）必须在每一台引擎上单独验一遍，'
note '#   不能凭「这一台认」推「那台认」，更不能凭「文档里有」推「本机有」。'

# ============================================================================
banner "第 11 节 sedinplace.sh 完整跑：就地改的四个代价"
# ============================================================================
note '# 8 格路线：-i.bak / -i / -i 空格 .bak 三种写法三种下场 → -i 换的是 inode（断硬链接、动符号链接）'
note '#   → mode 照抄、只读拦不住（要的是目录能写）→ 幂等是模式的性质不是 sed 的 → 没命中照样退 0'
note '#   → 备份名里的 * 另有身份 → 经典自毁 f > f → 收口码表。'
runc 'bash sedinplace.sh'

# ============================================================================
banner "第 12 节 小证据：BSD 的 sed -i \"\" 撞本机 GNU sed，以及 sed 不补换行"
# ============================================================================
note '# 网上大量教程写 sed -i "" s/a/A/ file —— 那是 BSD/macOS 的要求（-i 后面必须跟后缀串）。'
runc 'printf "alpha beta\n" > zz-bsd.txt
out=$(sed -i "" "s/a/A/" zz-bsd.txt 2>&1); rc=$?
printf "  sed -i \"\" 撞 GNU sed ｜ rc=%s ｜ 原文[%s]\n" "$rc" "$out"
printf "  文件动了吗 → [%s]（★ 那个空串被当成了 sed 脚本，真正的脚本被当成了文件名）\n" "$(head -1 zz-bsd.txt)"
rm -f zz-bsd.txt'
note '# 顺带还 sedinplace 第 7 格一笔：既然重定向会截断，那就地改完的另一个疑问是"末尾换行会不会被补回来"。'
runc 'printf "x\ny" > zz-nl.txt
printf "  改前 %s 字节，最后一个字符是 [%s]（没有换行）\n" "$(wc -c < zz-nl.txt)" "$(tail -c 1 zz-nl.txt)"
sed -i "s/y/Z/" zz-nl.txt
printf "  sed -i 之后 %s 字节，最后一个字符是 [%s] —— ★ 它不替你补末尾换行\n" "$(wc -c < zz-nl.txt)" "$(tail -c 1 zz-nl.txt)"
printf "  所以下游的 wc -l 还是比 grep 少 1 行（回看 greppat 第 6 格）：现在 wc -l=%s ｜ grep -c \"\"=%s\n" \
  "$(wc -l < zz-nl.txt)" "$(grep -c "" zz-nl.txt)"
rm -f zz-nl.txt'

# ============================================================================
banner "第 13 节 pipe3.sh 完整跑：管道三环，单位错位"
# ============================================================================
note '# 8 格路线：同一根管道三环手里拿的不是同一种东西 → 换行符是分帧符（漏一个换行下一环就并帧）'
note '#   → 管道的码由最后一环说了算，pipefail 取最右非 0 → 空输入时三个工具态度不同'
note '#   → 管道里的 read → 减号 = 从我这根管道读 → 下游提前收工会掐掉上游（这格故意不印那个码）→ 收口。'
runc 'bash pipe3.sh'

# ============================================================================
banner "第 14 节 小证据：同一根「前失败后成功」的管道，四个环境四个码"
# ============================================================================
note '# pipe3 在本机 bash 里演了开关，这一节把「换个 shell 就等于换了一套规矩」摊开。'
runc 'printf "a\n" > zz-pf.txt
bash -c "grep nomatch zz-pf.txt | cat; echo \"  bash 默认              管道码=\$?\""
bash -c "set -o pipefail; grep nomatch zz-pf.txt | cat; echo \"  bash + pipefail         管道码=\$?\""
dash  -c "grep nomatch zz-pf.txt | cat; echo \"  dash（/bin/sh 真身）    管道码=\$?\""
dash  -c "set -o pipefail; grep nomatch zz-pf.txt | cat; echo \"  dash + pipefail         管道码=\$?\""
rm -f zz-pf.txt'
note '# ★ 反直觉的那行是最后一行：本机 dash 0.5.12 竟然认 pipefail。写脚本时"这个 sh 认不认"必须当场问，'
note '#   不能按「POSIX 没有这个开关」的老印象下结论 —— 本章的判据是屏幕上那四个数，不是记忆。'
runc 'printf "  dash 有没有数组："; dash -c "a=1; echo \${a[@]}" 2>&1 | head -1'

# ============================================================================
banner "第 15 节 threeway.sh 完整跑：同一个任务交给三个人"
# ============================================================================
note '# 7 格路线：每行取最后一个数（三种写法逐字一样）→ 名字里多一个空格，取第 2 列的掉下去'
note '#   → 「分数低于 60」grep 只会长得像不会比大小 → 一共几行平均几分只有 awk 顺手'
note '#   → 命中行再往下多打一行（sed 要拼、awk 有 getline、grep 靠 -A）→ 就地改这份文件'
note '#   → 收口三张判据。'
runc 'bash threeway.sh'

# ============================================================================
banner "第 16 节 awkjoin.sh 完整跑：awk 的数组与两份文件"
# ============================================================================
note '# 7 格路线：数组不声明、下标是字符串、写一下自己长 → NR 与 FNR 两台计数器（NR==FNR 的真相）'
note '#   → 建表查表把 uid 换成名字，查不到必须当场标出来 → ★ 只是读一下不存在的键就多一个元素'
note '#   → 101 / "101" / "0101" 是不是同一个键 → 真 join 加合计校验 → 收口。'
runc 'bash awkjoin.sh'

# ============================================================================
banner "第 17 节 小证据：a[x,1] 里没有二维数组，只有一个被粘起来的字符串键"
# ============================================================================
note '# awkjoin 讲了「下标先变字符串」，这一节讲它的孪生坑：逗号下标其实是 SUBSEP 拼接。'
runc 'awk "BEGIN{
  a[\"Ada\",1]=90; a[\"Alan\",2]=88
  for (k in a) { n=split(k,p,SUBSEP); printf \"第一段=%s 第二段=%s 值=%s 整键字符数=%s 切开份数=%s\n\", p[1], p[2], a[k], length(k), n }
}" | sort'
note '# 屏幕上两行的键长分别是 5 和 6：姓名几个字 + 一个不可见的 SUBSEP + 一位数字 —— 键是一条拼出来的字符串。'
note '# 判据：awk 的"二维数组"是字符串键的把戏，所以「姓名里恰好带 SUBSEP 那个字节」就会撞成同一个键。'
runc 'awk "BEGIN{ printf \"  SUBSEP 占几个字符=%s ｜ 用它粘出来的 a SUBSEP b 长度=%s（三个字符）\n\", length(SUBSEP), length(\"a\" SUBSEP \"b\") }"'
runc 'awk "BEGIN{ k1 = \"x\" SUBSEP \"1\"; a[\"x\",\"1\"] = 90
  printf \"  逗号下标建出来的键，拿手写拼接的 k1 能查到吗 → %s ｜ 数组里元素数=%s（★ 它们本来就是同一个键）\n\", (k1 in a ? \"能\" : \"不能\"), length(a) }"'

# ============================================================================
banner "第 18 节 awkframe.sh 完整跑：字段、OFS 与整行重建"
# ============================================================================
note '# 7 格路线：同一行切三刀（FS 决定字段数 3/1/17）→ ★ 给字段赋值 = 整行按 OFS 重建（cat -A 可见）'
note '#   → print 逗号/无逗号/printf 三种拼接的字节账 → 设了 OFS 也对 print \$0 无效'
note '#   → 空字段也占坑 → NF 当守卫（先故意弄响一次）→ 收口单位表。'
runc 'bash awkframe.sh'

# ============================================================================
banner "第 19 节 小证据：给越界的字段赋值，NF 自己长，空位用 OFS 垫"
# ============================================================================
note '# awkframe 讲的是"改现有字段"，这一节补"改到不存在的位置"—— 它会当场把行撑长。'
runc 'printf "a b c\n" | awk "{ \$5=\"EE\"; printf \"NF=%s 重建=[%s]\n\", NF, \$0 }"'
runc 'printf "a b c\n" | awk "{ \$5=\"EE\"; print \$0 }" | cat -A'
note '# cat -A 那一行里 c 和 EE 之间是**两个**空格：第 4 个字段从来没存在过，重建时按 OFS 垫了一格。'
runc 'printf "a b c\n" | awk "{ NF=2; printf \"砍到 NF=%s 之后整行=[%s]\n\", NF, \$0 }"'
note '# 反方向也成立：把 NF 调小就当场丢掉后面的字段。这两条合起来是 awkframe 第 2 格的完整形状 ——'
note '#   awk 手里的"一行"是**字段列表**，$0 只是它的显示形式，一动字段整个显示就重排。'

# ============================================================================
banner "第 20 节 awkmawk.sh 完整跑：会叫的缺件与不会叫的缺件"
# ============================================================================
note '# 7 格路线：先自证本机 awk 是谁 → gensub/asort/strtonum 会叫（rc 全 2，报错原文逐字上屏）'
note '#   → ★★ FIELDWIDTHS 静默空转（rc=0、NF=1）→ ★★ /\s/ 在 mawk 里是字母 s，同机 grep/sed 读成空白'
note '#   → 别当残废：数组/ENVIRON/管道 getline/strftime 全都在 → 数字走 double，2^53 上 +1 被吞 → 收口两张清单。'
runc 'bash awkmawk.sh'

# ============================================================================
banner "第 21 节 小证据：一个 18 位整数走三条路，两条掉精度、一条原样保留"
# ============================================================================
note '# awkmawk 第 6 格演了 2^53 的断崖，这一格把「大整数从哪儿进来」三条路都摆一遍。'
runc 'awk "BEGIN{ x=123456789012345678; printf \"  ① 字面量直接打：%s\n\", x }"'
runc 'awk -v n=123456789012345678 "BEGIN{ printf \"  ② -v 传进来再 +0：%s\n\", n+0 }"'
runc 'printf "123456789012345678\n" | awk "{ printf \"  ③ 从文件读进来、原样拼成串：%s\n\", \$1 \"\" }"'
runc 'printf "123456789012345678\n" | awk "{ printf \"  ④ 同一个字段只做一次 +0：%s（★ 掉精度就发生在这一动）\n\", \$1+0 }"'
runc 'awk "BEGIN{ x=123456789012345678; printf \"  ⑤ 和原始字面串比一下：%s（无报错、rc 照样 0）\n\", (x \"\" == \"123456789012345678\") ? \"相同\" : \"不同\" }"'
note '# 三条路的分水岭是「有没有被当成数」：①②④ 一旦进算术就换成 double，末两位 78 变 80；'
note '#   ③ 之所以原样，是因为字段是「长得像数的串」，没被 +0、没被比较，就还是那串字符。'
note '# 判据：雪花 ID、毫秒时间戳、订单号这类超过 15 位的整数，在 awk 里要么一路当串搬（不 +0、不比大小），'
note '#   要么交给真能处理任意精度的一方 —— 本章不演后者（那是别的工具，且不在三剑客这一章的账上）。'

# ============================================================================
banner "第 22 节 综合演练：一份写死的脏日志，三连各就各位"
# ============================================================================
note '# 三剑客串一遍：grep 负责选行、sed 负责改形状、awk 负责按字段汇总。'
note '# 日志的每一行都是下面现造的常量，所以每一遍输出都相同（时间戳由脚本写死，绝不用 date）。'
runc 'mkdir -p zz-drive
printf "%s\n" \
  "2026-10-14 07:12:01 ERROR disk quota uid=1001" \
  "2026-10-14 07:12:09 WARN cache miss uid=1002" \
  "2026-10-14 07:13:22 ERROR disk quota uid=1001" \
  "2026-10-14 07:14:05 INFO retry scheduled uid=1003" \
  "2026-10-14 07:15:41 ERROR network unreachable uid=1002" > zz-drive/app.log
printf "  现场：%s 行 ｜ 字节数 %s\n" "$(wc -l < zz-drive/app.log)" "$(wc -c < zz-drive/app.log)"'
note '# ── 第一刀 grep：选行。先让守卫响一次，它的 0 才算数。'
runc 'printf "  守卫探针：挑 FATAL → 命中 %s 行，grep 自己的码=%s（★ 这一响证明它不是哑的）\n" \
  "$(grep -c FATAL zz-drive/app.log)" "$(grep -q FATAL zz-drive/app.log; echo $?)"
printf "  真要的：ERROR 行数=%s ｜ -o 数次数=%s（这道题里两个数刚好相等，因为一行只出现一次）\n" \
  "$(grep -c ERROR zz-drive/app.log)" "$(grep -o ERROR zz-drive/app.log | wc -l)"'
note '# ── 第二刀 sed：把选出来的行洗成「时间 级别 uid」三列，原文件一个字不动。'
runc 'grep ERROR zz-drive/app.log | sed -E "s/^([0-9-]+) ([0-9:]+) ([A-Z]+) .* (uid=[0-9]+)$/\2 \3 \4/" | tee zz-drive/cols.txt
printf "  洗出来 %s 行 ｜ 第一行现在只剩 [%s]\n" "$(wc -l < zz-drive/cols.txt)" "$(head -1 zz-drive/cols.txt)"'
note '# ── 第三刀 awk：按最后一个字段分组计数。数组遍历顺序不保证，所以外接 sort。'
runc 'sort -k1,1 -k2,2nr zz-drive/cols.txt | awk "{ c[\$NF]++ } END { for (k in c) printf \"%s %s\\n\", k, c[k] }" | sort -k2,2nr -k1,1'
runc 'printf "  合计校验：awk 数出 %s 行，grep -c 数出 %s 行 → %s\n" \
  "$(grep ERROR zz-drive/app.log | awk "END{ print NR }")" "$(grep -c ERROR zz-drive/app.log)" \
  "$([ "$(grep ERROR zz-drive/app.log | awk "END{ print NR }")" = "$(grep -c ERROR zz-drive/app.log)" ] && echo 一样 || echo 不一样)"'
note '# ── 收尾再补一刀 sed：这一层是「就地改」，所以先把守卫数命中做在前面（第 11 节那条判据）。'
runc 'cp zz-drive/app.log zz-drive/app2.log
before=$(grep -c "uid=1001" zz-drive/app2.log)
sed -i "s/uid=1001/uid=2001/g" zz-drive/app2.log
printf "  改前该模式命中 %s 处 ｜ 改后 uid=1001 还剩 %s 处 ｜ 新值有几处 %s 处\n" \
  "$before" "$(grep -c "uid=1001" zz-drive/app2.log)" "$(grep -c "uid=2001" zz-drive/app2.log)"
printf "  → 三件事分开数：动手前数一遍、动手后数一遍残留、再数一遍新值。只看 sed 的码（永远 0）等于没数。\n"'
note '# 这一节的现场自己收（第 25 节要检查沙箱里除脚本外不留任何东西）：'
runc 'rm -rf zz-drive; printf "  删掉之后，本目录里跟 zz-drive 有关的条目=%s 个\n" "$(ls -d zz-drive 2>/dev/null | wc -l)"'

# ============================================================================
banner "第 23 节 三张码表并排：同一件事 grep / sed / awk 各给什么码"
# ============================================================================
note '# 第 22 节用了「三态」，这里把三个工具的五种场景一次摊平（每一格都当场跑，只印码不印原文）。'
runc 'mkdir -p zz-codes; cd zz-codes
printf "error here\nplain line\n" > s.txt
code() { "$@" >/dev/null 2>&1; printf "%s" "$?"; }
printf "  %-22s %-7s %-7s %-7s\n" 场景 grep sed awk
printf "  %-20s %-7s %-7s %-7s\n" "命中/改成" \
  "$(code grep -q error s.txt)" "$(code sed "s/e/E/" s.txt)" "$(code awk 1 s.txt)"
printf "  %-20s %-7s %-7s %-7s\n" "没命中" \
  "$(code grep -q nosuch s.txt)" "$(code sed "s/nosuch/X/" s.txt)" "$(code awk "nosuch" s.txt)"
printf "  %-20s %-7s %-7s %-7s\n" "文件不存在" \
  "$(code grep -q error 没有这个.txt)" "$(code sed "s/a/A/" 没有这个.txt)" "$(code awk 1 没有这个.txt)"
printf "  %-20s %-7s %-7s %-7s\n" "自己的语法错" \
  "$(code grep -E "(" s.txt)" "$(code sed "s/a" s.txt)" "$(code awk "{" s.txt)"
printf "  %-20s %-7s %-7s %-7s\n" "空输入（管道里没东西）" \
  "$(code grep -q x </dev/null)" "$(code sed "s/a/A/" </dev/null)" "$(code awk 1 </dev/null)"
cd ..; rm -rf zz-codes'
note '# 读表：第二行是全章最重要的一行 —— 「没命中」只有 grep 给你码，sed 和 awk 都是 0。'
note '# 第四行是第二重要的一行：同样是「你的正则写错了」，sed 退 1、grep 和 awk 退 2（三家的档不同）。'
note '# 所以本章的纪律：判「有没有」问 grep；判「改没改」动手前后各数一遍；码只用来判「工具自己有没有出事」。'

# ============================================================================
banner "第 24 节 对账：十个脚本各几格、合计几格、demo 自己几节"
# ============================================================================
note '# 先看守卫能不能响：故意用一个匹配不到任何东西的模式跑一遍，十个 0 —— 那下面这条计数才可信。'
runc 'for f in grepdialect greppat grepfix sedflow sedinplace pipe3 threeway awkjoin awkframe awkmawk; do
  printf "  %-12s 假守卫计数=%s\n" "$f" "$(grep -c "=== ZZZ-NOT-THERE " "$f.sh")"
done
echo "  ★ 全 0 —— 所以下面这条真计数必须非 0，否则同样不可信"'
runc 'total=0
for f in grepdialect greppat grepfix sedflow sedinplace pipe3 threeway awkjoin awkframe awkmawk; do
  n=$(grep -c "=== " "$f.sh")
  printf "  %-12s %s 格\n" "$f" "$n"
  total=$((total + n))
done
printf "  ★ 十个脚本合计 %s 格（屏幕上每一格都在第 3~21 节真实出现过）\n" "$total"'
runc 'printf "  demo 自己的节标题数（grep ^banner）：%s\n" "$(grep -c "^banner " demo.sh)"
printf "  章目录里除 demo 外的脚本数：%s\n" "$(ls *.sh | grep -vc "^demo.sh$")"'
runc 'printf "  源文件行数：demo=%s ｜ 十脚本=%s\n" "$(wc -l < demo.sh)" "$(cat grepdialect.sh greppat.sh grepfix.sh sedflow.sh sedinplace.sh pipe3.sh threeway.sh awkjoin.sh awkframe.sh awkmawk.sh | wc -l)"'
note '# 这里数的「格」是脚本源码里的分格标题行；每一格都在屏幕上真实出现过（第 3~21 节逐条跑过）。'

# ============================================================================
banner "第 25 节 自证：沙箱里的事只留在沙箱"
# ============================================================================
note '# ① 章目录（沙箱外面）开跑前后各列一次条目名，比对是否一模一样。'
AFTER_LIST=$(cd "$HERE" && ls -A | sort | tr '\n' '|')
printf '  开跑前 %s 个条目 ｜ 现在 %s 个条目 → %s\n' \
    "$(printf '%s' "$BEFORE_LIST" | tr '|' '\n' | grep -c .)" \
    "$(printf '%s' "$AFTER_LIST" | tr '|' '\n' | grep -c .)" \
    "$([ "$BEFORE_LIST" = "$AFTER_LIST" ] && echo 一模一样 || echo "不一样：[$BEFORE_LIST] vs [$AFTER_LIST]")"
note '# ② 十个辅助脚本的现场子目录 + 第 22 节演练的 zz-drive，是不是都当场收干净了（先故意造一个，让守卫响）。'
runc 'mkdir -p zz-故意留下的目录
printf "  故意造一个多余目录之后，多余目录数=%s（★ 守卫响了）\n" "$(ls -d */ 2>/dev/null | wc -l)"
rmdir zz-故意留下的目录
printf "  删掉之后，多余目录数=%s（必须是 0：十个脚本的子目录由它们自己的 trap 清，演练由第 22 节末尾清）\n" "$(ls -d */ 2>/dev/null | wc -l)"'
note '# ③ 沙箱 work 里现在还剩什么（只印相对条目名）。'
runc 'ls -A | sort | tr "\n" " "; echo'
note '# ④ 本章没写过联网、装包、开监听端口、后台作业的语句。扫描对象是十个取证脚本 + demo 自己。'
note '#   守卫先要能响，这个 0 才算数 —— 所以这里插一个含禁词的假脚本把守卫弄响，再撤掉。'
runc 'pat="curl|wget|nc -l|nohup|pip install|apt-get|yum install|sleep"
list=(grepdialect.sh greppat.sh grepfix.sh sedflow.sh sedinplace.sh pipe3.sh threeway.sh awkjoin.sh awkframe.sh awkmawk.sh)
printf "curl -s https://example.com\nsleep 1\n" > zz-probe.sh
printf "  ④-1 先弄响：把探针 zz-probe.sh 算进来，命中文件数=%s\n" "$(grep -lE "$pat" "${list[@]}" zz-probe.sh | wc -l)"
rm zz-probe.sh
printf "  ④-2 撤掉探针：同一道守卫命中文件数=%s（★ 响过一次，这个 0 才算数）\n" "$(grep -lE "$pat" "${list[@]}" | wc -l)"'
runc 'pat="curl|wget|nc -l|nohup|pip install|apt-get|yum install|sleep"
list=(grepdialect.sh greppat.sh grepfix.sh sedflow.sh sedinplace.sh pipe3.sh threeway.sh awkjoin.sh awkframe.sh awkmawk.sh)
printf "  ④-3 假阳性对照：把 demo.sh 也扫进来，命中文件数=%s\n" "$(grep -lE "$pat" "${list[@]}" demo.sh | wc -l)"
printf "     ↑ 多出来的那一个就是 demo.sh —— 禁词的字面文本就写在它自己那行 pat= 里。\n"
printf "     任何「扫源码找禁词」的守卫都会自匹配，所以扫描清单必须把持有模式串的那个文件挑出去。\n"'
note '# ⑤ 屏幕上不该有真实身份。匹配串故意写成 /hom[e]/ 带方括号：不带方括号时它会命中'
note '#   「写着它自己的那一行」（永远非 0 的假阳性，④-3 就是活例子）。先拿假想路径把守卫弄响。'
runc 'list=(grepdialect.sh greppat.sh grepfix.sh sedflow.sh sedinplace.sh pipe3.sh threeway.sh awkjoin.sh awkframe.sh awkmawk.sh demo.sh)
printf "/hom%s/somebody/nowhere\n" "e" > zz-path-probe.txt
printf "  ⑤-1 先弄响：把含假想绝对路径的探针算进来，命中=%s 处\n" "$(grep -o "/hom[e]/[A-Za-z0-9._-]*" "${list[@]}" zz-path-probe.txt | wc -l)"
rm zz-path-probe.txt
printf "  ⑤-2 撤掉探针：连 demo.sh 自己一起扫，命中=%s 处（★ 响过一次，这个 0 才算数）\n" "$(grep -o "/hom[e]/[A-Za-z0-9._-]*" "${list[@]}" | wc -l)"'
note '# ⑤-3 这一节只印「命中几处」，不印命中的内容：grep -o 会把匹配到的路径原样送上屏幕。'
runc 'printf "  本 shell 的后台作业数=%s（本章没起过 & 作业）\n" "$(jobs -l | wc -l)"'
note '# ⑥ 本章没演（写了就是假承诺）：别的进程正打开着文件时 -i 的可见性、属主不是自己的文件、'
note '#   跨文件系统 mv、NFS、gawk 真装上以后 FIELDWIDTHS 的正解、任何耗时与内存表现。'
printf '  demo 自己跑到这里，最后一节的退出码=%s\n' "$?"

exit 0
