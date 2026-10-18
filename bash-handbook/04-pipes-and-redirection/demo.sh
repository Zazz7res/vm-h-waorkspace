#!/usr/bin/env bash
# ============================================================================
# 第 04 章《管道与重定向：谁读到了什么》—— 整章证据一键复现
#
#   bash demo.sh            跑完自动删沙箱（默认）
#   bash demo.sh --keep     跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt  重定向时自动关掉颜色，README 引用的就是这份纯文本
#
# 屏幕上出现的每一行，都是这条命令在这台机器上的真实结果 —— 没有一行是手写的。
#
# 和 02 章的衔接：
#   02 章讲「一行命令怎么被切开」（词、IFS、展开）；本章讲「切完之后，
#   各个进程之间怎么倒水」。心智模型就三句话：
#     ① 管道里的每一段都是**自己的进程**，段与段之间隔着一根内核管道；
#     ② 重定向是在命令**启动之前**改变它的文件描述符（0 读、1 写、2 写错误）指向；
#     ③ `|` 从左到右流，同一行的多个重定向**按书写的先后顺序**逐个生效，不是同时生效。
#
# 本章的取证路线（26 节，每节 1~4 条命令，全部真跑）：
#   第 1~2 节   先把环境钉死：版本、语言环境、外部命令的来路
#   第 3~4 节   管道 = 多个进程：赋值为什么会丢，以及三种修法
#   第 5~6 节   一段管道到底成功了没有：PIPESTATUS 与 set -o pipefail
#   第 7~9 节   重定向本体：书写顺序、&> 家族、关闭 fd 的真实报错
#   第 10~11 节 exec 的持久改道，以及 /dev/null 怎么把故障藏起来
#   第 12~13 节 喂输入（here-doc / here-string）与分流（tee）
#   第 14~16 节 管道的三类事故：UUOC、xargs 分词、名字里的空白
#   第 17~19 节 信号记账：SIGPIPE 的 141、后台的 143、pipefail 撞 head
#   第 20~23 节 进阶工具：进程替换、管道缓冲、私有 fd 与自动 fd
#   第 24~25 节 分组重定向与 noclobber
#   第 26 节    自证：沙箱已删、章目录干净、没起过端口
#
# 安全说明：
#   * 所有写操作只发生在 mktemp -d 出来的沙箱里（前缀 bash04-）；
#     mkdir / touch / rm 全部只作用于沙箱内本章自己造的文件，仓库目录只读
#   * 绝不修改 ~/.bashrc、~/.profile、~/.config、~/.local、/etc：本章不需要 rc，
#     要看「另一个干净 shell」的行为时一律用 bash -c 起新进程
#   * 唯一的后台进程是本节里那几个 sleep（在子 shell 里起、当场 wait/kill 收尾），
#     不起任何监听端口；xargs 的 rm 只删沙箱里本章自造的文件，绝不碰仓库
#   * 不联网、不装包（缺的工具如实打印，例如 shellcheck）
#   * 屏幕上不打印任何绝对路径，也不出现本机登录名：要列真实目录用 runc_at，
#     命令行只留目录名 + 相对路径；/etc/hostname、ls -l 这类会泄露用户名的写法一律不用
#
# 为什么这个脚本**故意不加** set -e：
#   本章一半的内容就是「失败现场」：cat 一个不存在的文件、写到已关闭的 fd、
#   noclobber 拒绝覆盖、pipefail 把 head 判成失败、here-doc 终止符写错吞掉全文。
#   加了 set -e，第 5 节第一段就把整个 demo 掐了，后面的证据全都取不到。
#   同理也不加 set -u（演示里大量故意留空的变量）。
#   唯一的例外是 pipefail：它本身就是本章的**被演示对象**，只在第 6 节里
#   用「一条命令内开启、跑完就关」的方式局部出现，绝不写在脚本顶部全局生效。
#   （set -euo pipefail 在生产脚本里为什么必加，是第 08 章的内容。）
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

# 把语言环境钉死成「英文 + UTF-8」：这台机器默认 zh_CN.UTF-8，bash 和 coreutils 的报错
# 会是中文，README 就没法逐行和真实输出核对。改动只发生在本脚本和它 fork 出的子进程里。
ORIG_LANG="${LANG:-（未设）}"
ORIG_LC_ALL="${LC_ALL:-（未设）}"
export LC_ALL=C.UTF-8 LANG=C.UTF-8 LANGUAGE=C

# 输出被重定向到文件时（README 捕获就是这样跑的）不要 ANSI 颜色码：纯文本才能逐行核对。
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/bash04-XXXXXX")

banner() { printf '\n══════ %s %s\n' "$1" "════════════════════════════════"; }
note() { printf '%s\n' "$1"; }

# 打印命令 → 用 bash -c 真跑 → 打印退出码。
# 用 bash -c 是为了能自然地写管道、`;`、重定向、各种引号；也保证每一段都在一个
# **全新的非交互 bash** 里开始：上一节开的 shopt / set -o 开关不会串到下一节。
# 2>&1 把子进程的 stderr 并进 stdout，所以「报错原文」也留在这份纯文本捕获里。
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}

# runc_at <目录> <命令>：先 cd 进那个目录再跑，屏幕上只留目录名（basename）+ 相对命令。
#   要列真实目录（比如本章目录本身）时用它：命令行里不出现绝对路径，本机登录名一个都不留。
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
        printf '\n[沙箱已删除] %s\n' "$(basename "$SB")"
    fi
}
trap cleanup EXIT

# 准备沙箱工作目录：把本章源文件复制进去，后面所有加工都在副本上进行。
mkdir -p "$SB/work"
cp "$HERE"/*.sh "$SB/work/"
cd "$SB/work" || exit 1

# 本章要用到的几个小文件，全在沙箱里现造（名字都是 ASCII，免得和 02 章的分词话题抢戏）
printf 'fd3 的第一行\nfd3 的第二行\n' > lines3.txt
printf 'banana\napple\ncherry\n' > a.txt
printf 'apple\nbanana\ndurian\n' > b.txt
printf 'l1\nl2\nl3\nl4\n' > uuoc.txt
printf '需要被保护起来的原始文件\n' > keepme.txt

# ============================================================================
banner "第 1 节 先把这台机器的发言权确认一遍"
# ============================================================================
note '# 本章大量依赖 bash 自己的机制（PIPESTATUS、lastpipe、进程替换、自动 fd），'
note '# 它们全是版本相关的；报错文本还和语言环境有关，所以先钉死环境。'
runc 'bash --version | head -1'
runc 'echo "demo 已把自己改成：LC_ALL=[$LC_ALL] LANG=[$LANG]（你原来的终端是 LANG=$ORIG_LANG，LC_ALL=$ORIG_LC_ALL）"'
note '# 为什么要专门做这一步？因为本章有两条结论**长得一模一样但内容不同**：'
note '#   · cat 找不到文件时报的是 "No such file or directory"（英文）还是 "没有那个文件或目录"（中文）；'
note '#   · bash 内建命令写坏 fd 时报的是 "Bad file descriptor" 还是中文译名。'
note '# 报错文本跟着语言环境走。本章取证一律钉死在 LC_ALL=C.UTF-8 + LANGUAGE=C：'
note '#   这样 cat / bash / ls 的报错原文固定，README 才能逐行 grep -qF 核对。'
runc 'nosuchcommand04'
note '# ↑ 这条报的是英文 command not found；你终端里默认是中文「未找到命令」，同一条命令两种字。'
note '# 本章的源文件已经被复制进沙箱，后面的命令全在这个目录里跑（只显示名字，不显示绝对路径）：'
runc 'ls'
note '# ★ 带走第一条：bash 的语义 + 外部命令的报错文本 = 两个独立的版本相关变量。'
note '#   换台机器、换个 LANG，本章某些字符串就得重新取一遍证。'

# ============================================================================
banner "第 2 节 本章用到的外部命令都是谁家的"
# ============================================================================
note '# 管道的语义在 bash 手里，但管道里跑的是外部命令；它们的报错措辞、选项集合各不同，'
note '#   所以每章第一步都是「先探一次再用」。本机 coreutils 是 Rust 实现的那一套：'
runc 'echo "外部命令的来路（本机 coreutils 是 uutils 那套，报错措辞和 GNU 版不同）：" && ls --version | head -1 && wc --version | head -1'
runc 'echo "xargs 来自：$(xargs --version | head -1)"; echo "find 来自：$(find --version | head -1)"'
runc 'echo "sed 来自：$(sed --version | head -1)"; echo "grep 来自：$(grep --version | head -1)"'
note '# 这点差别在本章会露面两次：'
note '#   · 第 15~16 节的 xargs/find 是 GNU findutils 的，-d、-0、-print0 全都有；'
note '#     换成 BSD（macOS）那套，find 没有 -print0 之外的同名行为、xargs 没有 -d，脚本直接翻脸。'
note '#   · 第 22 节的 stdbuf 是 GNU coreutils 的东西，uutils 版也有，但行为细节以本机实测为准。'
note '# 本章会碰到的工具，先一次性点名；没有的就照实说没有，不装。'
runc 'for t in cat tee yes head tail xargs find diff sort stdbuf tr shellcheck; do printf "  %-11s %s\n" "$t" "$(command -v "$t" || echo 本机没有)"; done'
note '# ↑ shellcheck 本机没有（第 09 章才用它），本章只做实跑取证；stdbuf 有，第 22 节的缓冲实验靠它。'
note '# ★ 带走第二条：`|` 左边的 bash 只是**接线员**，真正干活的是这些外部命令；'
note '#   「管道怎么 behaving」问 bash，「某一段为什么这样报错」问那一段自己的实现。'

# ============================================================================
banner "第 3 节 管道两侧是两个进程：赋值为什么会丢"
# ============================================================================
note '# 一句话模型：`A | B` 是 fork 出两个进程，中间接一根内核管道；bash 自己在它们上面。'
note '# 所以「在管道里改变量」改的是那个子进程的记忆，父进程一无所知 —— 这不是 bash 的 bug，'
note '#   是 fork 的物理后果：子进程拿到的是父进程内存的**复印件**，写复印件不会改原件。'
note '# 先用进程号把「真的是两个进程」摆出来。bash 里有两个号码可以问：'
note '#   · $$      = 当前脚本/交互 shell 启动时那个进程.pid（ fork 之后**不会**更新，是继承的字符串）'
note '#   · $BASHPID = 当前正在执行的那段代码所属进程的 pid（真的会变）'
runc 'echo "这个 bash -c 自己：\$\$=[$$]  \$BASHPID=[$BASHPID]"'
runc 'echo x | { echo "管道右侧：\$\$=[$$]（没变，是从父进程继承的字符串）  \$BASHPID=[$BASHPID]（变了 = 真的换了一个进程）"; echo "     它从管道里读到了 $(cat)"; }'
runc 'echo "外层 bash -c 自己 BASHPID=[$BASHPID]"; { echo "第一段 BASHPID=[$BASHPID]"; } | { cat; echo "第二段 BASHPID=[$BASHPID]"; } | { cat; echo "第三段 BASHPID=[$BASHPID]"; }'
note '# ↑ 一次管道里四个不同的进程号：外层 bash -c 加上三段，每段都是独立进程；'
note '#   而它们打印的 $$ 全一样（那是继承来的字符串），所以认进程只能用 $BASHPID。'
note '#   顺带看见：管道右侧那句 $(cat) 读到的正是左边的 x —— 水确实是走管道倒过来的。'
note '# 现在把「复印件」这件事变成看得见的赋值丢失：两条最经典的翻车写法。'
runc 'v=7; echo x | v=9; echo "① 管道右侧把 v 改成 9，左侧看到的 v=[$v]（还是 7）"; echo x | read w; echo "② 管道右侧 read 到的 w=[$w]（空字符串）"'
note '# 两条共同点：右侧那一段跑完就退出，它的 v=9 / w=x 随进程一起没了；'
note '#   `echo x | read w` 更狠：w 连「曾经是什么」都没被外面的 shell 看见过。'
note '#   这就是「while read 循环里累加的计数器，循环结束后变成 0」的同一条病根（下一节治它）。'
note '# ★ 带走第三条：管道里每一段都是独立进程；想在管道之后接着用变量，必须换写法。'

# ============================================================================
banner "第 4 节 把水接回来的三种写法：改左边、lastpipe、整段搬进同一进程"
# ============================================================================
note '# 上面那个「赋值丢了」的坑，日常有三种修法，每一种都在这里真跑一遍。'
note '# 改法①：把「要留下来的东西」放到命令替换里，让结果从左边的门进来。'
runc 'v=""; v=$(echo 来自命令替换); echo "改法①：v=[$v]"; x=outer; y=$(x=inner; echo "$x"); echo "  附赠对照：命令替换里的赋值同样出不来（里面 x=[$y]，外面 x=[$x]）"'
note '# ↑ 注意后半句：命令替换也是子进程，所以「把结果递出来」靠的是 stdout，不是变量。'
note '# 改法②：shopt -s lastpipe —— 让**最后一段**回到当前 shell 里跑，不再另起进程。'
runc 'echo "bash -c 起的这个进程的开关串 \$-=[$-]（没有 i → 非交互 → lastpipe 才有资格生效）"; v=""; shopt -s lastpipe; echo x | read v; echo "改法②（开着 lastpipe）：v=[$v]"; shopt -u lastpipe; echo x | read v; echo "  同一句关掉 lastpipe 再来：v=[$v]"'
note '# lastpipe 的两个前提：① 这个 shell 必须**没有 job control**（交互终端里默认不生效，'
note '#   所以它只在脚本里有用）；② 只有最后一段回到当前 shell，前面的段依旧是独立进程，'
note '#   在中间段累加的计数器照样丢。'
note '# 改法③：把「循环」和「用结果的那句话」写进同一对 { }，让它俩待在同一个进程里。'
runc 'total=""; printf "a\nb\n" | { while IFS= read -r line; do total="$total$line"; done; echo "改法③：total=[$total]（大括号内赋值，大括号内就用掉了）"; }'
runc 'printf "a\nb\nc\n" | while IFS= read -r line; do n=$((n+1)); done; echo "对照（循环和 echo 分家）：循环数到 3，管道外面 n=[$n]"; shopt -s lastpipe; n=0; printf "a\nb\nc\n" | while IFS= read -r line; do n=$((n+1)); done; echo "同一条写法开了 lastpipe：n=[$n]"'
note '# ★ 带走第四条（选型顺序，别记反）：能放左边就放左边（改法①最稳）；'
note '#   脚本里、只需要最后一段的结果 → lastpipe（改法②）；'
note '#   结构复杂、想把状态圈在一小块里 → 同一段 { }（改法③）。'
note '#   永远不要指望「管道外面那个变量」自己活过来。'

# ============================================================================
banner "第 5 节 PIPESTATUS：整条管道的 \$? 只看最后一段"
# ============================================================================
note '# 造一条**中间真的会失败**的管道：cat 一个不存在的文件，后面接 wc -l。'
runc 'cat nosuchfile404.txt | wc -l'
note '# ↑ 报错是 cat 的，退出码却是 0 —— 因为 wc 成功了。这是本章最重要的一条反直觉。'
note '# bash 给了一条数组来记录「每段各自的退出码」：PIPESTATUS。'
note '#   注意这些值必须在**紧跟着的那一条命令里**读完：只要中间多插一条命令（哪怕就是一个 echo），'
note '#   PIPESTATUS 就被那条命令重置成一位数组了 —— 这也是下面每个例子都只 echo 一次的原因。'
runc 'cat nosuchfile404.txt | wc -l; echo "↑ 每段各自的退出码 PIPESTATUS=[${PIPESTATUS[@]}]，段数=${#PIPESTATUS[@]}，整条管道的 \$?=[$?]"'
note '# ↑ 数组里两个数：cat 退 1、wc 退 0。而 $? 就等于数组的最后一个元素。'
note '#   换句话说：默认情况下 bash 只把「最后一段」当成整条管道的成败，前面的失败一概不问。'
runc 'false | true; echo "两段 [${PIPESTATUS[@]}]  段数=${#PIPESTATUS[@]}  \$?=[$?]"; false | true | true; echo "三段 [${PIPESTATUS[@]}]  段数=${#PIPESTATUS[@]}  \$?=[$?]"'
runc 'nosuchcmd04 | wc -l; echo "命令不存在的 127 也照样被吞：[${PIPESTATUS[@]}]  \$?=[$?]"'
note '# 127 那个位置属于「bash 在子进程里报 command not found」，它也一样只在数组里露头。'
note '# 段数=${#PIPESTATUS[@]} 顺手当「这条管道跑了几段」的探针 —— 第 14 节数 UUOC 的成本就靠它。'
note '# 再给一条容易被当成「永远会挨刀」的写法：grep -q。它一命中就 exit，读端提前关门。'
runc 'echo "① 5 行的短管道（塞得进管道缓冲，上游早写完了）："; seq 1 5 | grep -q 3; echo "   [${PIPESTATUS[@]}]"; echo "② 20 万行 + 第一行就命中（上游还在写，读端没了）："; seq 1 200000 | grep -q 1; echo "   [${PIPESTATUS[@]}]"; echo "③ 20 万行 + 最后一行才命中（上游其实已经写完）："; seq 1 200000 | grep -q 199999; echo "   [${PIPESTATUS[@]}"'
note '# ↑ 同一个写法、只有「数据量 + 命中位置」不同，三个结果分别是 0、141、0。'
note '#   所以「grep -q 会让上游死」不是恒真的定律，取决于命中时上游有没有还在写。'
note '#   141 这个数从哪来的，第 17 节用 yes 和 kill -l 拆给你看。'

# ============================================================================
banner "第 6 节 set -o pipefail：让「中间那一段失败」变成整条管道的失败"
# ============================================================================
note '# pipefail 是本章唯一一个「先开再关」的开关，只在这几条命令内部有效：'
runc 'echo "开之前：$(set -o | grep pipefail)"; set -o pipefail; echo "开之后：$(set -o | grep pipefail)"'
note '# 现在把第 5 节那条一模一样的管道再跑一遍，只看 \$? 变不变。'
runc 'set -o pipefail; cat nosuchfile404.txt | wc -l; echo "pipefail 下：每段 PIPESTATUS=[${PIPESTATUS[@]}]（数组本身没变），整条管道的 \$?=[$?]，第 5 节不开时这里是 0"'
note '# ★ 关键读数：PIPESTATUS 数组**不会因为 pipefail 而改变**，pipefail 只改「整条管道的 $?」。'
note '#   所以想看每段的细节，任何时候都得读数组；pipefail 只是让默认的错误检查不再放过中间段。'
note '# 多段各有成败时，pipefail 取的是**最后一个非零**的那一段（不是第一个）：'
runc 'set -o pipefail; true | false | true; echo "三段 [${PIPESTATUS[@]}]  pipefail 后的 \$?=[$?]（最后一段是 true，但倒数第二个非零）"; set -o pipefail; false | true | false; echo "另一条 [${PIPESTATUS[@]}]  \$?=[$?]"'
runc 'set -e -o pipefail; cat nosuchfile404.txt | wc -l; echo "这行永远执行不到"'
note '# ↑ 上面这条只有报错、没有「这行永远执行不到」，并且退出码是 1：'
note '#   set -e 看到 pipefail 交上来的非零 $? 就当场把脚本杀了。这是两者配合的真实账单。'
runc 'cat nosuchfile404.txt | wc -l >/dev/null; echo "这是一条全新的 bash -c（默认没开 pipefail），\$? 又是[$?]"'
note '# ★ 带走第五条：pipefail 是「让 && 和 if 对整条管道负责」的开关；'
note '#   demo 里每一条都是全新的 bash -c，开关天然不串味；你自己的脚本里要么整份开，'
note '#   要么用完 set +o 关掉，别让它半开半关（第 19 节会看到它和 head 打架）。'

# ============================================================================
banner "第 7 节 重定向顺序：>f 2>&1 和 2>&1 >f 完全不是一回事"
# ============================================================================
note '# 探针：一个同时往 stdout 和 stderr 各写一行的命令块，先把「两个都上屏」的样子立起来。'
runc 'echo "探针本体（两个都上屏）："; { echo OUT-这是stdout; echo ERR-这是stderr >&2; }'
note '# 重定向的规则：同一行里的多个重定向**按书写顺序从左到右逐个生效**，'
note '#   而且 2>&1 的意思是「把 2 号复制成 1 号此刻的指向」—— 复制的是**那一刻**的指向，不是永久绑定。'
note '# 这条规则只有一处地方会咬人：就是 `>` 和 `2>&1` 的先后顺序。当场称两个版本：'
runc 'rm -f ord1.txt; { echo OUT-这是stdout; echo ERR-这是stderr >&2; } >ord1.txt 2>&1; echo "===== 屏幕上只有这一行 echo，两句话都不见了 ====="; echo "----- cat ord1.txt -----"; cat ord1.txt'
runc 'rm -f ord2.txt; { echo OUT-这是stdout; echo ERR-这是stderr >&2; } 2>&1 >ord2.txt; echo "===== 上面屏幕中间混进来的那行 ERR，就是没进文件的 ====="; echo "----- cat ord2.txt -----"; cat ord2.txt; wc -l ord1.txt ord2.txt'
note '# 读法（严格按从左到右执行，不是同时生效）：'
note '#   >ord1.txt 2>&1 → 先把 1 指向文件；再把 2 复制成「1 此刻的指向」= 同一个文件 → 两行都进文件。'
note '#   2>&1 >ord2.txt → 先把 2 复制成「1 此刻的指向」= 屏幕；再改 1 指向文件 → stdout 进文件、stderr 留在屏幕上。'
note '#   所以 ord1.txt 两行、ord2.txt 一行 —— 行数就是这段推理的收据。'
runc 'ls nosuchdir404 >ord3.txt 2>&1; echo "标准写法：报错进了文件，屏幕上干净，退出码=[$?]"; cat ord3.txt'
runc 'ls nosuchdir404 2>&1 >ord4.txt; echo "写反了：报错已经在上面屏幕上出来了；ord4.txt 行数=$(wc -l <ord4.txt)"'
note '# ★ 带走第六条：背下来没意义，理解「复制发生在写下的那一刻」就够了 ——'
note '#   `2>&1` 必须写在 `>文件` **后面**，它才可能跟文件走；写在前面的 2>&1 只会跟住旧的 1 号。'

# ============================================================================
banner "第 8 节 &> / &>> / 2>&1 / 1>&2 / >&2 逐条真跑"
# ============================================================================
runc 'rm -f amp.txt; ls nosuchdir404 &>amp.txt; echo "&> 一次收走两个流，屏幕上没有红字了；amp.txt："; cat amp.txt'
runc 'echo 追加的一行 &>>amp.txt; ls nosuchdir404 &>>amp.txt; echo "现在是 $(wc -l <amp.txt) 行："; cat amp.txt'
note '# &> / &>> 是 bash 的简写，等价于「>文件 2>&1」和「>>文件 2>&1」；POSIX sh 里没有这条。'
note '#   危险的是它**不会报错**：dash 把 & 当后台符、把 >文件 当成另一条空命令的重定向，'
note '#   于是同一个字符串在两个 shell 里得到完全不同的文件。当场称一下字节数：'
runc 'rm -f v_bash.txt v_dash.txt; bash -c "ls nosuchdir404 &>v_bash.txt"; dash -c "ls nosuchdir404 &>v_dash.txt"; sleep 0.2; echo "bash 写的文件=$(wc -c <v_bash.txt) 字节   dash 写的文件=$(wc -c <v_dash.txt) 字节"'
note '# ↑ dash 那份是 0 字节：报错丢了、命令也确实「成功」了。要跨 shell 就写 >文件 2>&1。'
runc 'echo "① >&2 和 1>&2 是同一件事（省略方向时默认就是 1）：两句都以报错流的形式出现在下面"; echo 甲 >&2; echo 乙 1>&2'
runc 'echo "② 反过来 2>&1 是把 stderr 并到 stdout：于是管道里的 tr 把两个流一起变成大写"; { echo go-stdout; echo go-stderr >&2; } 2>&1 | tr a-z A-Z'
note '#   如果分开写（只有 stdout 进管道），tr 就只能把第一行变大写 —— 这一条是并流的直接后果。'
runc 'echo "③ 把两个流对调（先给 stderr 留底，再让 stdout 走 stderr 的路）："; exec 3>&2 2>/dev/null; echo "这句写到了 fd3（=原来的 stderr，所以还能上屏）"; echo "这句本该上屏，但 2 号已经指向 /dev/null 了" >&2; exec 2>&3 3>&-; echo "恢复后再上屏的一句"'
note '# ★ 带走第七条：这些符号其实只有三种动作 —— 改指向（>、>>、<）、复制指向（n>&m、n<&m）、'
note '#   丢进黑洞（>/dev/null）。`&>` 只是 bash 给的「两种一起改」快捷键，不是新概念。'

# ============================================================================
banner "第 9 节 关闭 fd：>&- 与 Bad file descriptor 的原文"
# ============================================================================
note '# fd 不是用完自动就没了；显式关闭用 `n<&-`（读）/ `n>&-`（写）/ `exec n<&-`。'
note '# 「关掉之后再写」是什么下场？下面三条分别对应三种写法，报错原文各不相同：'
runc 'echo 这句写向已经关掉的1号 >&-'
note '# ↑ 原文是 bash: line 1: echo: write error: Bad file descriptor —— 报错来自**内建的 echo**，'
note '#   不是外部命令：1 号已经不在进程表里，write() 返回 EBADF，bash 把它翻译给你看。'
runc 'echo 这句在 >&- 之后又跟了一个 2>&1 试试 >&- 2>&1'
note '# ↑ 这条的原文是 bash: line 1: 1: Bad file descriptor —— 少了 "echo: write error"，'
note '#   因为它连**建立重定向**这一步就失败了（要把 2 复制给 1，可 1 已经不存在），命令根本没跑起来。'
note '#   同一行里的重定向还是从左到右生效（第 7 节那条规则），先关后复制就复制不到东西。'
runc 'exec 1>&-; echo "第一句：stdout 已经没了"; echo "第二句：也没有"; echo "第三句走 stderr，还能上屏" >&2; echo "最后这句也没有"'
note '# ↑ 上面这条是「两句报错 + 一句正常」：exec 1>&- 影响**之后所有**命令，不是一句一次。'
note '#   而且没有任何办法把它「恢复原样」—— 原地址没有事先 dup 出来就再也找不回了（第 10 节演示留底）。'
runc 'exec 3<lines3.txt; echo "① 开着 3 号读到的第一行："; read -r l <&3; echo "   [$l]"; exec 3<&-; echo "② 关掉 3 号之后再拿它去读："; read -r l <&3; echo "   read 的退出码=[$?]"; echo "③ 编号会被复用：关掉再开，bash 还是给你 3 号"; exec 3<lines3.txt; cat <&3; exec 3<&-'
note '# 读端的关闭报错是 `bash: line 1: 3: Bad file descriptor`，和外层命令无关（read 都没来得及跑）。'
note '# ★ 带走第八条：`>&-` 是「这个编号从这个进程消失」，不是「这一句不输出」。'
note '#   要临时静音用 >/dev/null（编号还在，只是指向黑洞）；要退出后不留幽灵占用就显式关闭。'

# ============================================================================
banner "第 10 节 exec 不带命令时：改的是本 shell 的 fd，而且是持久的"
# ============================================================================
note '# 普通重定向只影响它那一条命令；`exec >文件` 影响这个 shell **从这一刻起的所有命令**。'
note '# 这段放在 execsave.sh 里跑：它会带走 stdout，写在 demo.sh 主体里就把整章输出吞进文件了。'
note '# 文件里五步：留底 → 带走 → 恢复 → 看看没留底就关掉的代价 → 最后 ls 一下落了什么。'
runc 'bash execsave.sh'
runc 'echo "最小版：exec 2>文件 只搬 stderr，stdout 一点不动"; exec 2>err_only.txt; echo "这句 stdout 还在屏幕上"; echo "这句 stderr 进了 err_only.txt" >&2; exec 2>&1; echo "把 2 号接回 1 号之后，这句 stderr 又上屏了" >&2; echo "----- err_only.txt 里留下的只有这一句：-----"; cat err_only.txt'
note '# 注意最后那个 `exec 2>&1`：它把 2 号接回「1 号此刻的指向」，也就是恢复默认。'
note '#   这个恢复动作能成立，全靠第 10 节里 1 号**从头到尾没被动过**；它要是也被改过，就得另留底。'
runc 'exec >ov.txt; echo 第一次写; echo 第二次写; exec >ov.txt; echo 第三次写（前两次已经被截掉了）; exec >>ov.txt; echo 第四次写（追加）; exec >/dev/null; echo "这一行你看不见，它被丢进黑洞了"; cat ov.txt; echo "ov.txt 现在 $(wc -l <ov.txt) 行"'
note '# ↑ 读数：同一个 `exec >` 再写一次 = 重新截断，只剩「第三次」；`exec >>` 才把第四次接上。'
note '#   最后 `exec >/dev/null` 把自己写哑了 —— 所以脚本里用 exec 改道，一定要记得改回来。'
runc 'echo "对照：单命令重定向 vs exec 持久改道，谁活得久："; { echo "A 只属于这一条命令" >one.txt; echo "B 这一句没带重定向，所以还上屏"; }; echo "----- one.txt -----"; cat one.txt'
note '# ★ 带走第九条：想让改动只活一句话 → 把重定向贴在那句话后面或包在 { }/子 shell 里；'
note '#   用了 exec 就是给这个 shell 换水管，作用域直到「再 exec 一次」或进程结束。'

# ============================================================================
banner "第 11 节 /dev/null：把真实故障藏起来的现场复现"
# ============================================================================
note '# /dev/null 是一个「写多少都不留、一读就 EOF」的设备文件，它不参与管道，也不会失败。'
runc 'echo "① 什么都不加，能看到报错原文："; grep 关键字 nosuchfile404.txt; echo "   退出码=[$?]"'
runc 'echo "② 只把 stderr 丢进 /dev/null：报错原文没了，退出码一个字节都没变："; grep 关键字 nosuchfile404.txt 2>/dev/null; echo "   退出码=[$?]"'
note '# ↑ 两条退出码都是 2。/dev/null 不改命令的成败，只改你看得见多少 ——'
note '#   排障时最坑的就是这个：脚本里挂了 2>/dev/null，日志里就只剩「失败」两个字，原因全没了。'
runc 'echo "③ 想藏报错却顺手把正常输出也藏了（新手最常见的一次自伤）："; ls a.txt b.txt >/dev/null 2>&1; echo "   屏幕上什么都没显示，其实 ls 是成功的，退出码=[$?]"'
runc 'echo "④ 正确姿势：只丢 2 号、1 号留着；下面这条里那个不存在的文件只贡献一行「消失的报错」"; ls a.txt b.txt nosuchfile404.txt 2>/dev/null; echo "   存在的两个照常打印，退出码=[$?]（ls 因为那个文件其实也报了非零）"'
note '# 这条退出码是 2：/dev/null 藏住了「为什么」，但藏不住「失败了」。'
note '#   想两者兼得就把 stderr 落到日志文件（2>>log.txt），或者用第 13 节的 tee 同时上屏和落盘。'
runc 'echo "⑤ /dev/null 当输入用：它永远立刻给你 EOF"; wc -c </dev/null; read -r x </dev/null; echo "   read 的退出码=[$?]（读到 EOF 就算失败），x=[$x]"'
runc 'echo "⑥ 丢弃和落盘的差别（同一份数据两跑）："; head -c 100000 /dev/urandom >big_sandbox.bin; head -c 100000 /dev/urandom >/dev/null; echo "   落盘那份 $(wc -c <big_sandbox.bin) 字节，丢弃那份不占地方"; rm -f big_sandbox.bin; echo "   现在再 ls 它：$(ls big_sandbox.bin 2>&1)"'
note '# ★ 带走第十条：2>/dev/null 是「我已经不打算看这件事的失败原因」的书面声明。'
note '#   要么别写，要么写了就顺手把 || 的补救分支也写上，别让它替你把 bug 藏三个月。'

# ============================================================================
banner "第 12 节 here-doc 与 here-string：<<EOF、<<'EOF'、<<-、<<< 四种写法"
# ============================================================================
note '# 这一节交给两个独立文件跑：`<<-` 的证据必须是源码里真实的 Tab 字符，'
note '#   而「终止符写错会吞掉后面全部内容」这种事故只能发生在独立文件里 ——'
note '#   放进 demo.sh 主体就会把整章脚本一起吞掉。'
runc 'bash heredoc.sh'
runc 'bash herestring.sh'
note '# 上面 heredoc.sh 里 A~G 七块，按顺序证的是：定界符加不加引号、正文里的 $ 变量与 $(( ))'
note '#   到底展不展开、`<<-` 只删 Tab 不删空格（用 cat -A 把 ^I 打出来看）、终止符多一个空格的事故。'
note '# 下面这两条是同一件事的**内联**版本，让你不看源文件也能对比引号的区别：'
runc 'cat <<EOT
带引号的那句不会展开：$LC_ALL 和 $((6*7)) 保持原样写进来是不行的，这里定界符没加引号
所以这一行会展开成：LC_ALL=[$LC_ALL]，6×7=$((6*7))
EOT'
runc "cat <<'EOT'
同一份正文，定界符加上单引号：$LC_ALL 和 $((6*7)) 就一个都不展开了
这一行里的 \$LC_ALL 保持字面，$((6*7)) 也保持字面
EOT"
note '# ↑ 两行一对比就明白：加引号的定界符 = 「正文按原样喂给 stdin」，等价于把整段用单引号包住。'
note '#   规则和第 02 章的单/双引号是同一套，只是这里作用在「多行输入」上。'
runc 'echo "终止符一行都没写的空 here-doc："; cat <<EOF'
note "# ↑ 原文：bash: line 1: warning: here-document at line 1 delimited by end-of-file (wanted \`EOF')"
note '#   警告之后什么都没有 —— 这就是「忘写终止符」的真实下场：不报错、只是把后面的东西全吞掉。'
note '# 四条结论：① 定界符加不加引号 = 正文要不要过一遍展开（和双引号/单引号同一套规则，02 章）；'
note '#   ② `<<-` 只删行首的 Tab，空格一个都不删，所以要用 `<<-` 就得把缩进统一成 Tab；'
note '#   ③ 终止符必须**顶格**且完全一致（`<<-` 允许前面是 Tab），多一个空格就一路吞到文件尾；'
note '#   ④ 选型：一行文本用 <<<，多行文本用 <<EOF，多行又要缩进用 <<- + Tab。'

# ============================================================================
banner "第 13 节 tee：一份数据同时进文件和下游（含 /dev/stdout 那次「重复输出」）"
# ============================================================================
note '# tee 的语义：把 stdin 原样抄一份到每个参数文件，同时**继续往自己的 stdout 写**。'
note '#   所以它是管道里唯一一个「既落盘又不断流」的段 —— 想边存边往下游送就用它。'
runc 'rm -f t1.txt; printf "甲\n乙\n" | tee t1.txt; echo "----- tee 落到文件里的那份 -----"; cat t1.txt'
runc 'printf "11\n22\n33\n" | tee t2.txt | grep 2; echo "tee 自己占管道的一段，所以 PIPESTATUS=[${PIPESTATUS[@]}]"'
note '# ↑ 段数=3：tee 是货真价实的一段（也是独立进程）。它下游那个 grep 只留一行，'
note '#   文件里那份不受影响 —— 分流发生在 tee 内部，不是在管道上。'
note '# 网上很多脚本写 tee /dev/stdout 想「再打一遍」，它到底会不会重复？当场试：'
runc 'printf "只有一行z\n" | tee /dev/stdout | cat -n'
note '# ↑ 只喂了一行，下游 cat -n 却编到 2：`/dev/stdout` 就是**这条管道自己**，'
note '#   于是 tee 写「文件」（=管道）一次、再写自己的 stdout（=同一条管道）一次，两份都到了下游。'
runc 'rm -f o.txt e.txt; { printf "w\n" | tee /dev/stderr | wc -l; } > o.txt 2> e.txt; echo "o.txt(stdout)=[$(cat o.txt)]  e.txt(stderr)=[$(cat e.txt)]"'
note '# ↑ 这次 /dev/stderr 走的是另一条路（报错流），所以只多出一行 w，没有重复进 wc。'
note '# ⚠ 这条是本章现场踩出来的：裸写 `printf w | tee /dev/stderr | wc -l` 并把整个 demo'
note '#   重定向进文件（`demo.sh > 捕获 2>&1`）时，**捕获文件会被从头截断**。原因：那一刻'
note '#   /dev/stderr 就是那个文件，而 tee 打开输出文件默认带 O_TRUNC。'
note '#   所以这里用 { ...; } > o.txt 2> e.txt 把两个出口各自关进沙箱文件：现象一模一样，文件不坏。'
runc 'rm -f t3.txt; printf "第一版\n" | tee t3.txt >/dev/null; printf "第二版\n" | tee -a t3.txt >/dev/null; echo "tee 覆盖写 + tee -a 追加写，文件里最后是："; cat t3.txt; echo "----- 再来一次不关 stdout 的对照（屏幕上会再出现一遍）-----"; printf "第三版\n" | tee -a t3.txt'
note '# 「存一份顺便看」的正解就是最后这条：tee 文件名（+ -a 决定是否追加），stdout 保留。'
note '# ★ 带走第十一条：tee 的双写不是玄学，是「文件」和「stdout」两个出口各写一次；'
note '#   当这两个出口是同一个东西（/dev/stdout 在管道里就是那条管道）时，你才会看到重复。'

# ============================================================================
banner "第 14 节 UUOC：cat 用管道其实是多起一个进程（用段数证明）"
# ============================================================================
note '# UUOC = Useless Use of cat。网上说它「没必要」，这里用 PIPESTATUS 的段数把代价量化：'
runc 'wc -l <uuoc.txt; echo "重定向版：管道段数=${#PIPESTATUS[@]}  PIPESTATUS=[${PIPESTATUS[@]}]"'
runc 'cat uuoc.txt | wc -l; echo "cat 管道版：管道段数=${#PIPESTATUS[@]}  PIPESTATUS=[${PIPESTATUS[@]}]"'
note '# ↑ 两个结果一样，但下面那条多了一个进程（数组里多一位）。数据量小时完全感觉不到，'
note '#   放进 for 循环里处理几万个文件就是白烧一倍的 fork。'
note '# 顺便说清楚「段数」这个探针：管道里每多一个 `|` 就多一位数组元素，'
note '#   `wc -l <文件` 的重定向版根本不是管道，所以段数是 1（就是那条命令自己）。'
runc 'echo "cat 真正有价值的场合：把多个文件拼成一个流，或者需要 - 表示 stdin 的时候"; echo "① 拼流：wc -l <a.txt 只能读一个文件；cat a.txt b.txt | wc -l = $(cat a.txt b.txt | wc -l) 行"; echo "② 有些命令只吃 stdin，不接文件名参数，这时必须用管道或 < 喂它"; printf abc | grep -q abc; echo "   printf abc | grep -q abc 的退出码=[$?]（grep 从管道读到了内容，命中）"'
runc 'echo "而 grep 需要「哪个文件里的哪一行」时，把文件名交给它自己，别用管道："; grep -n an a.txt b.txt 2>/dev/null; echo "退出码=[$?]"'
note '# ↑ 走管道（cat a b | grep an）就没有文件名了，只有内容 —— 这才是「少开一个进程」的附带好处。'
note '# ★ 带走第十二条：`cat 文件 | 命令` 该写成 `命令 <文件`；但反过来，'
note '#   「命令只能吃 stdin」或「要把几个文件拼成一个流」时，cat 就不是废物而是必需品。'

# ============================================================================
banner "第 15 节 xargs 的分词陷阱（一）：名字里带空格"
# ============================================================================
note '# 先把「名字里有空白」的文件造出来（全在沙箱里，仓库目录一个都没碰）：'
runc 'bash mkweird.sh'
note '# mkweird.sh 造了 4 个文件：普通名、带空格、带真换行、带真 Tab。它自己用 -print0 数过一遍。'
note '# find 的输出用**换行**分隔，而 xargs 默认按 IFS 那套空白（空格/Tab/换行）再切一刀，'
note '#   于是名字里的空格也被切成两个参数。用 -t 让 xargs 把真要执行的命令打出来：'
runc 'find weird -type f | xargs -t echo 收到的参数是: 2>&1 | head -8'
note '# ↑ -t 先打印「即将执行的命令」，紧接着是 echo 自己打的那行，两份内容一模一样：'
note '#   weird/two words.txt 变成了两个参数（weird/two、words.txt）。这里先用 echo 代替 rm，不下手。'
note '# 顺手证明「切错了也不会自动删掉」：沙箱里的保护文件还在，因为 xargs 喂给 rm 的是被切碎的名字。'
runc 'cp keepme.txt keep_copy.txt; ls -1 weird | wc -l; echo "（沙箱里 keep_copy.txt 还在：$(ls keep_copy.txt 2>&1)）"'
runc 'echo "换成只按换行分词："; find weird -type f | xargs -d "\n" -t echo 收到的参数是: 2>&1 | head -8'
note '# ↑ -d "\n" 之后 two words.txt 是一个完整参数了（bash 的引号把 \n 解释成真换行交给 xargs）；'
note '#   但名字里带换行的那个还是会翻车 —— 因为分隔符本身也被它用了，见第 16 节。'
note '# ★ 带走第十三条：xargs 默认的分词单位是「空白」，不是「行」；'
note '#   只要名字可能有空白，就别用默认切法。'

# ============================================================================
banner "第 16 节 xargs 的分词陷阱（二）：换行 + -print0 修正 + 沙箱内真删"
# ============================================================================
note '# 名字里带真换行的文件，让 find 的「一行一个」彻底失效：一个文件被读成两个。'
note '# 这里不再用 sed 把 NUL 换成符号（那会把 NUL 本身混进输出，破坏纯文本捕获），'
note '#   改成让 xargs 用 printf 把每个名字方括号包起来 —— 名字里的换行就原样露出来了：'
runc 'echo "① 带换行的那个名字单独找出来（屏幕上它占两行，但它是一个文件）："; find weird -type f -name "nl*"; echo "② 四个名字各自的真面目（-print0 交给 xargs -0，再逐个 printf）："; find weird -type f -print0 | xargs -0 -I{} printf "名字=[%s]\n" "{}"'
note '# ↑ 上面那段是 4 个文件、6 行输出：带换行的名字自己就把两行占了，这正是 find|wc -l 会数错的原因。'
runc 'echo "对同一批文件：find | wc -l 数出来=$(find weird -type f | wc -l)，真实个数=$(find weird -type f -print0 | tr -dc "\0" | wc -c)（数 NUL 的个数）"'
note '# 修法只有一个：让 find 用 NUL 结尾（-print0），让 xargs 按 NUL 分（-0）。'
runc 'find weird -type f -print0 | xargs -0 -t echo 收到的参数是: 2>&1 | head -8'
note '# ↑ 看清楚 -t 那行：四个名字各成一个参数，带 Tab/换行的被 bash 风格引号包起来了。'
note '# ↓ 现在才真的动手删：先在沙箱里复制一份 weird2/，两条命令都只作用在复制品上。'
runc 'rm -rf weird2; cp -a weird weird2; echo "复制了 $(find weird2 -type f -print0 | tr -dc "\0" | wc -c) 个文件，先跑错误切法的版本："; find weird2 -type f | xargs -t rm 2>&1 | head -6; echo "剩下的文件："; find weird2 -type f; echo "----- 换成 -print0 | xargs -0 之后再删 -----"; find weird2 -type f -print0 | xargs -0 -t rm; echo "还剩 $(find weird2 -type f -print0 | tr -dc "\0" | wc -c) 个文件"; rm -rf weird2; echo "沙箱里的复制品已清理"'
note '# ↑ 看 rm 的报错：它去删 weird2/tab、weird2/nl、weird2/two 这种**根本不存在**的名字 ——'
note '#   带空格/换行的那个名字一个都没被删掉。反过来，如果某个切碎出来的片段正好撞上'
note '#   真实存在的文件（比如当前目录里就有个叫 name.txt 的文件），那就是无声误删。'
note '# ★ 带走第十四条：只要文件名要跨进程传递，就用 -print0 | xargs -0；'
note '#   「管道按行传名字」这件事本身就不安全，和你小心不小心没关系。'

# ============================================================================
banner "第 17 节 SIGPIPE：yes | head -1 之后那个退出码到底是什么"
# ============================================================================
note '# 前面说过「管道里每一段都是独立进程」，这一节看两个进程之间**读端先关门**会发生什么。'
runc 'yes | head -1; echo "PIPESTATUS=[${PIPESTATUS[@]}]  整条管道的 \$?=[$?]"'
note '# 读数：yes 那一段是 141，head 那一段是 0，整条管道 \$? 也是 0（默认只看最后一段，第 5 节）。'
note '# head 读满 1 行就 exit(0)，读端通道没人接了；yes 的下一次 write 触发内核信号 SIGPIPE，'
note '# 默认动作是终止进程 → 128+13=141。所以「141」不是 head 的错，是**写端被砍**的记账。'
runc 'echo "把 141 拆开：141 = 128 + 13；本机 13 号信号的名字 = [$(kill -l 13)]"; echo "bash 里 128+信号号 是「被信号杀死」的统一记账方式，不是命令自己返回的"'
note '# 换个下游、换个上游，刀还在不在？三条对照，全在同一台机器上跑出来：'
runc 'yes | head -1 >/dev/null; echo "① 连 head 的输出都丢掉，两段还是 [${PIPESTATUS[@]}]（第一段照旧 141）"; seq 1 200000 | awk "NR==1{exit 0}"; echo "② awk 读完第一行就退：[${PIPESTATUS[@]}]"'
runc 'seq 1 200000 | tail -1; echo "③ tail 必须读到最后一行才动手：上游 [${PIPESTATUS[@]}]（两个 0，没人挨刀）"; seq 1 200000 | sort | tail -3 >/dev/null; echo "④ sort 同理，它得把全部读完才输出：[${PIPESTATUS[@]}]"'
note '# ★ 带走第十五条：判断「上游会不会 141」只看一件事 —— 下游退出时上游**还有没有要写的**。'
note '#   head / grep -q / awk exit 这类提前收工 = 会砍；tail / sort / wc 这类必须读完 = 不砍。'
note '#   第 19 节会看到：一旦开了 pipefail，这条「上游 141」就会变成整条脚本的失败理由。'

# ============================================================================
banner "第 18 节 后台进程与 trap 收尾：143 = 128+15 的同一套记账"
# ============================================================================
note '# 本章唯一的后台进程就是这几条 sleep（无害、当场收干净）；不起任何端口。'
note '# 为什么要在这里讲后台？因为第 17 节那套「128 + 信号号」的记账，脚本收尾时天天用到：'
note '#   你 kill 掉一个后台任务，它的退出码不是 0，是 143。'
runc 'sleep 30 & p=$!; echo "起了一个后台 sleep，\$! 记到的 pid=[$p]"; kill $p; wait $p; echo "被 SIGTERM(15) 杀掉后 wait 到的退出码=[$?]"'
note '# 读数：`$!` 是「最后一个后台任务的 pid」；`kill` 只是发信号，必须 `wait` 才能拿到它的退出码。'
note '#   143 = 128 + 15（SIGTERM），和第 17 节的 141 = 128 + 13（SIGPIPE）同一个公式。'
runc 'sleep 1 & p=$!; trap "kill $p 2>/dev/null; echo \"[trap EXIT] 收尾：杀掉的后台 pid=$p\"; wait $p" EXIT; echo "这行跑完脚本就正常退出，看 trap 有没有替我们擦屁股"'
note '# ↑ trap EXIT = 「这个 shell 无论怎么结束都执行这一段」；demo.sh 自己清理沙箱用的就是它。'
runc 'sleep 0.2 & p=$!; echo "不写 trap、直接 exit 0 的话：后台 pid=[$p]（它还在睡）"; exit 0'
note '# ↑ 上一条打完 pid 就退了，trap 没写，那个 sleep 只能自己睡完（0.2 秒）；'
note '#   真脚本里长期跑的后台任务必须 trap EXIT 收尾，否则留一堆孤儿进程（demo 里靠 sleep 短到无所谓）。'
runc 'echo "信号退出码这一家的对照表（全是 128+信号号，本机名字由 kill -l 现查）："; printf "  141 = 128 + 13 (%s)  ← 写端没人接，第 17 节\n" "$(kill -l 13)"; printf "  143 = 128 + 15 (%s) ← 被 kill 默认信号干掉，本节\n" "$(kill -l 15)"; printf "  130 = 128 + 2  (%s)   ← 你按 Ctrl-C\n" "$(kill -l 2)"'
note '# ★ 带走第十六条：`kill` 发的是信号，退出码要 `wait` 才拿得到；被信号杀死的进程一律 128+信号号。'
note '#   看到 141/143 不要慌，先问「谁给它发了哪个信号」，答案就在数字里。'

# ============================================================================
banner "第 19 节 pipefail 撞上 head：set -euo pipefail 之后的假失败"
# ============================================================================
note '# 第 17 节里 `yes | head -1` 是「head 成功、上游被砍」；把它放进 pipefail 的脚本里就是另一回事。'
runc 'set -o pipefail; yes | head -1 >/dev/null; echo "pipefail 开着的整条管道 \$?=[$?]（head 明明成功了，141 从上游传上来了）"'
runc 'set -e -o pipefail; yes | head -1 >/dev/null; echo "这行永远执行不到：set -e 已经被 141 杀掉"'
note '# ↑ 真实账单：屏幕上只有 yes 的那一行 y 被丢进 /dev/null 了，「这行永远执行不到」没打出来，'
note '#   外层看到退出码 141。第 17 节那条无害的写法，在 set -e -o pipefail 里就是一次真失败。'
runc 'set -e -o pipefail; yes | head -1 >/dev/null || true; echo "解法①（|| true 明确说我不在意上游）：脚本活下来了，\$?=[$?]"'
runc 'set -e -o pipefail; if yes | head -1 >/dev/null; then echo "解法②：进 then（整条被判定成功）"; else echo "解法②：进 else —— pipefail 把整条判成失败，\$?=[$?]，但脚本还活着"; fi'
note '# 解法③（最干净）：在那几行局部关掉 —— `set +o pipefail; …; set -o pipefail`，'
note '#   或者干脆不用管道：head -1 文件 而不是 cat 文件 | head -1（第 14 节的 UUOC 在这里第二次有用）。'
note '# ★ 带走第十七条：head/tail -n 1/grep -q/awk exit 这类「提前收工」的下游，和 pipefail 天生打架。'
note '#   要么用 || true / if 把它明确标成「我看过并且不在乎」，要么就局部关开关。'

# ============================================================================
banner "第 20 节 进程替换 <(...)：它给的是一个文件名，不是 stdin"
# ============================================================================
note '# 进程替换是本章最后一个大杀器：把「一条命令的输出」伪装成一个**文件名**塞进参数位置。'
runc 'wc -c <(echo hi)'
note '# ↑ 注意 wc 打印了 /dev/fd/62 这样的「文件名」：<( ) 是把输出挂到一个 fd 上，'
note '#   再把 /dev/fd/N 这个名字塞进**参数位置**。所以能用它的命令必须是能吃文件名的。'
runc 'cat <(echo hi); echo "（cat 也是把它当文件名读出来的）"'
runc 'wc -c < <(echo hi); echo "换成 < <( ) 才是真的接到 stdin：这次没有文件名，只有数字"'
note '# 两个写法的区别不是风格，是**入口**不同：`cmd <( )` 走参数，`cmd < <( )` 走 stdin。'
runc 'read -r x <(echo hi) </dev/null; echo "① read 只吃 stdin，参数位置给它就翻脸：退出码=[$?]"; exec 6< <(echo hi); if [ -p /dev/fd/6 ]; then echo "② 挂在 6 号上的那个名字，test -p 说它是【管道】，不是普通文件"; else echo "② test -p 说它不是管道"; fi; echo "   从它读到的内容：$(cat <&6)"; exec 6<&-'
note '# ② 这条很重要：`/dev/fd/N` 背后是**匿名管道**，不是磁盘上的临时文件。'
note '#   后果一：没有真实的 inode 大小，`wc -c` 读它是读管道字节数；'
note '#   后果二：命令想「把这个文件打开两遍／seek 回去」就会翻车 —— 管道不能回退。'
note '# ★ 带走第十八条：能用 <( ) 的前提是「那条命令接文件名参数」；'
note '#   只吃 stdin 的命令（read、很多过滤器的默认行为）就得写 `< <( )`。'

# ============================================================================
banner "第 21 节 进程替换 >(…) 和 diff <(sort a) <(sort b)"
# ============================================================================
runc 'echo "两个文件各自的排序结果直接对比，中间不落任何临时文件："; echo "--- sort a.txt ---"; sort a.txt; echo "--- sort b.txt ---"; sort b.txt; echo "--- diff <(sort a.txt) <(sort b.txt) ---"; diff <(sort a.txt) <(sort b.txt); echo "diff 的退出码=[$?]（1 = 有差别）"'
note '# 这就是 <( ) 最实用的场合：diff/sort/join/paste 这些「要接文件名」的命令，'
note '#   现在可以直接吃「另一条命令的结果」，省掉 mktemp + 清理的那一段。'
runc 'paste -d+ <(printf "1\n2\n3\n") <(printf "a\nb\nc\n")'
note '# ↑ 两个 <( ) 占两个参数位，编号会各自是 /dev/fd/62、/dev/fd/63 —— 一次调用可以同时挂好几个。'
note '# >( ) 是反方向：把「写进这个名字的东西」交给一条命令去消化。它有一个必须记住的坑：异步。'
runc 'rm -f ps_async.txt; echo 这条走进程替换 > >(cat > ps_async.txt); echo "主命令立刻就退出了，此刻 ps_async.txt 里=[$(cat ps_async.txt 2>&1)]"; sleep 0.3; echo "等 0.3 秒再看：ps_async.txt 里=[$(cat ps_async.txt)]"'
note '# ↑ 第一枪读到的是「文件还不存在」的报错，第二枪才读到内容 ——'
note '#   >( ) 起的那个进程 bash **不等它**，写日志/边算边存很顺手，但下一行马上去读就可能是空的。'
runc 'printf "x\ny\n" > >(wc -l > ps_count.txt); echo "不等的话立刻读：[$(cat ps_count.txt 2>&1)]"; sleep 0.3; echo "等一下再读：ps_count.txt=[$(cat ps_count.txt)] 行"'
note '# 解法：要么像这样 sleep/wait，要么就别用 >( )，改成 `… > 文件` 之后再显式读那个文件。'
note '# ★ 带走第十九条：<( ) 同步（对方打开时数据就在），>( ) 异步（收尾要自己管）。'

# ============================================================================
banner "第 22 节 管道缓冲：为什么 | grep 的时候输出会「卡住」（stdbuf 本机有）"
# ============================================================================
note '# C 库的 stdout 有个规则：目标是**终端**时按行缓冲（一行一 flush），'
note '#   目标是**文件或管道**时改成块缓冲（攒满 4K/8K 才吐一次）。'
note '#   管道里每一段看到的 stdout 都不是终端，于是中间那段就把你的实时性吃掉了。'
runc 'echo "stdbuf 在本机：$(command -v stdbuf)  版本 $(stdbuf --version)"'
runc 'bash slowpipe.sh'
note '# 关键读数：中间段块缓冲时，A 的到达时间和 B 挤在一起（都被推迟到 350ms 之后）；'
note '#   行缓冲（stdbuf -oL / sed -u / grep --line-buffered）时 A 立刻到。'
note '# 所以「管道里加了个 grep 之后 tail -f 看不出实时效果」不是 grep 慢，是它的输出被攒住了。'
runc 'to_ms(){ local t=$1; printf "%d" $(( ${t%.*} * 1000 + 10#${t#*.} / 1000 )); }; b=$(to_ms "$EPOCHREALTIME"); seq 1 200000 | sed -n p >/dev/null; m=$(to_ms "$EPOCHREALTIME"); echo "块缓冲 sed -n p 跑 20 万行：$((m-b)) 毫秒"; b=$(to_ms "$EPOCHREALTIME"); seq 1 200000 | sed -u -n p >/dev/null; m=$(to_ms "$EPOCHREALTIME"); echo "行缓冲 sed -u -n p 跑 20 万行：$((m-b)) 毫秒"'
note '# ↑ 这就是「实时」的代价：行缓冲等于每行一次 write 系统调用，吞吐明显掉。'
note '#   所以别在生产里无脑加 --line-buffered，只有你真的需要边看边处理时才值得。'
runc 'echo "内核给单根管道的容量也有上限，本机可调到的最大值："; cat /proc/sys/fs/pipe-max-size; echo "同一家族还有哪些旋钮："; ls /proc/sys/fs | grep -i pipe'
note '# 1048576 = 1MB 是「允许把一根管道调到多大」的上限，不是默认容量（默认由内核决定，'
note '#   量级是几十 KB）；slowpipe.sh 最后那组「小数据 vs 12 万行」的对照就是默认容量的影子：'
note '#   小数据上游瞬间写完，大数据上游写到一半被堵住，只能等下游来读。'
note '# ★ 带走第二十条：管道不是无限大的传送带，它是一个**有限容量的缓冲区**，'
note '#   写满就堵、读空就等；「谁读到了什么」取决于谁在什么时候写、谁在什么时候读。'

# ============================================================================
banner "第 23 节 exec 3< 文件 + read -u 3；以及 {变量名}> 让 bash 自己挑 fd"
# ============================================================================
note '# 前面都是 0/1/2 三个公用水管；bash 还允许你自己接一根私有水管（3 号以上）。'
runc 'exec 3<lines3.txt; n=0; while IFS= read -r -u 3 line; do n=$((n+1)); echo "  从 3 号读到第 $n 行：[$line]"; done; echo "读完（read 的退出码=[$?]），关掉 3 号"; exec 3<&-'
note '# 为什么不直接 while read：因为 stdin 经常已经被管道占了；3 号这种「私设水龙头」'
note '#   让你能同时读两个来源（第 05 章讲 read 循环的坑时还要靠它）。'
note '# 硬编码 3 号有个隐患：这段代码可能被放进一个已经用了 3 号的环境里。'
note '#   bash 4.1+ 给了自动分配：`exec {变量名}>文件`，编号由 bash 挑（从 10 往上找空位）。'
runc 'echo "自动分配：exec {fd}>auto04.txt → 看 bash 本机给了几号"; exec {fd}>auto04.txt; echo "  拿到的编号 fd=[$fd]"; echo 写进去的一句 >&"$fd"; exec {fd}>&-; echo "  关掉后文件内容：$(cat auto04.txt)"'
note '# 三个细节：① `{fd}` 那个大括号是语法的一部分，不是花括号展开；② 用完的编号回填进变量；'
note '#   ③ 后面用的时候要写 `>&"$fd"`（把变量的**值**当编号），漏了引号或者写成 `>&$fd` 都行，'
note '#   但写成 `exec $fd>&-` 就翻脸了 —— 那是「执行一个叫 24 的命令」。'
runc 'echo "拿同一个变量再写一次（fd 已经关了）："; echo 还想写 >&"$fd"; echo "退出码=[$?]"'
runc 'echo "读侧同理：exec {in}<lines3.txt"; exec {in}<lines3.txt; echo "  编号=[$in]"; read -r -u "$in" one; read -r -u "$in" two; echo "  读到：[$one] / [$two]"; exec {in}<&-; echo "  再读第三次（文件已经到 EOF）："; read -r -u "$in" three; echo "  退出码=[$?] three=[$three]"'
note '# ★ 带走第二十一条：私有 fd 的完整生命周期是「开 → 用 → 关」，'
note '#   开用 `exec {v}<文件`（别让编号写死），用 `read -u "$v"`／`>&"$v"`，关 `exec {v}<&-`。'

# ============================================================================
banner "第 24 节 { } 分组重定向 vs ( ) 子 shell 重定向"
# ============================================================================
note '# 一整段命令想共用一个重定向，就把它包起来。包法有两种，差别在「起不起新进程」。'
runc 'v=1; { v=2; echo "组内 v=[$v]"; } >grp04.txt 2>&1; echo "组外 v=[$v]  ← { } 不起新进程，所以改动留住了"; echo "grp04.txt 里=[$(cat grp04.txt)]"'
runc 'v=1; ( v=3; echo "子 shell 内 v=[$v]" ) >sub04.txt 2>&1; echo "外面 v=[$v]  ← 和第 3 节管道右侧同一个病"; echo "sub04.txt 里=[$(cat sub04.txt)]"'
note '# 输出部分两者都能被重定向带走，区别全在**变量**上：{ } 改的是本 shell 的变量，'
note '#   ( ) 改的是复印件（第 3 节讲过的 fork 后果）。'
runc 'echo "整段丢弃 / 整段捕获，两种写法都要一对括号或大括号："; { echo 甲; echo 乙; } >/dev/null; echo "  { } > /dev/null 之后屏幕照样有字（说明只是那一段被吞了）"; out=$( { echo 丙; echo 丁; } ); echo "  命令替换捕获版 out=[$out]"; { echo 戊; echo 己; } | wc -l'
note '# 注意 `$( { } )` 这个组合：命令替换本身就是子 shell，所以里面再怎么改变量都出不来 —— '
note '#   想捕获输出就别改值，想留变量就别用 $()。'
runc 'echo "exec 在两种括号里的作用域差别："; ( exec >inner_exec.txt; echo "子 shell 里的 exec 只影响子 shell" ); echo "这句在外面，没被带走"; echo "----- inner_exec.txt -----"; cat inner_exec.txt'
note '# 语法细节（踩过的人才懂）：{ } 的大括号是保留字，后面要有空格、最后一条命令要有分号，'
note '#   `}` 也要单独成词；`( )` 是操作符，括号前后不需要空格，里面也不用分号。'
note '# ★ 带走第二十二条：要把变量的修改留在本 shell（计数、赋值）→ { }；要隔离脏改动 → ( )。'

# ============================================================================
banner "第 25 节 noclobber：> 拒绝覆盖，>| 强制覆盖"
# ============================================================================
note '# 本章最后一个开关：set -C（=set -o noclobber）让 `>` 变成「只许新建，不许覆盖」。'
runc 'cp keepme.txt nc04.txt; echo "先造一个已经存在的文件："; cat nc04.txt'
runc 'set -C; echo "第二版" >nc04.txt; echo "退出码=[$?]"; echo "文件还是原来的："; cat nc04.txt; set +C'
note '# ↑ 真实报错是 bash: line 1: nc04.txt: cannot overwrite existing file；'
note '#   文件一个字节都没动 —— 这就是它防的那件事：手滑把重要文件覆盖成一行字。'
runc 'echo "用 >| 强制覆盖（bash 的逃生门）："; set -C; echo "第三版" >|nc04.txt; echo "退出码=[$?]"; set +C; cat nc04.txt'
runc 'echo ">& 这种写法同样被拦；>> 追加不受影响；tee 是程序自己 open 的，管不着："; set -C; echo 追加成功 >>nc04.txt; echo "  >> 退出码=[$?]"; echo 被拦住 >&nc04.txt; echo "  >& 退出码=[$?]"; printf "tee 写的一句\n" | tee nc04.txt >/dev/null; echo "  tee 退出码=[$?]"; set +C; echo "----- 现在 nc04.txt 里有三行：-----"; cat nc04.txt'
note '# 读数：>> 是「追加」不是「覆盖」，noclobber 不拦；tee 根本没走重定向，它自己 open()，'
note '#   所以它照样把文件改写了 —— 别把 noclobber 当安全机制，它只是「挡住 `>` 这一只手滑」。'
note '# ★ 带走第二十三条：脚本里临时开一下、用完 set +C 关掉（上面每条都自己关了）；'
note '#   demo 每条命令都是全新的 bash -c，所以开关不会串到下一节。'

# ============================================================================
banner "第 26 节 自证：沙箱里的事只留在沙箱，章目录一个字都没多"
# ============================================================================
note '# demo 的写操作全部发生在 mktemp -d 的沙箱里；此刻的当前目录就是沙箱的 work/，'
note '#   本章跑出来的文件全在这里（跑完就整体删掉，仓库目录从头到尾只读）：'
runc 'echo "沙箱 work/ 里现在的文件数=$(ls | wc -l)，前 12 个："; ls | head -12'
note '# 这些名字（ord*.txt、weird/、t1.txt、ps_*.txt、exec_log.txt…）全是本章造的加工品，'
note '#   它们跟着沙箱一起被 trap cleanup 删掉，一个都不会带回仓库。'
note '# 本章目录里应该只有源文件：demo.sh + 五个辅助 .sh，没有任何 demo 产生的文件：'
runc_at "$HERE" 'find . -maxdepth 1 -type f | sort'
note '# 再确认两件事：目录里没有子目录残留，也没有 C 源码混进来（本章和 C 无关，那一层深度是别人的取证现场）：'
runc_at "$HERE" 'echo "子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  main.c 个数=$(find . -name main.c | wc -l)"'
note '# 本章一条网络命令都没用、没起过任何监听端口（后台只有几条 sleep，且都在同一条命令里收尾）：'
runc 'echo "本章未起任何端口；复核一下此刻的监听列表："; if command -v ss >/dev/null 2>&1; then ss -lnt | head -5; else echo "ss 本机没有，改用 netstat：$(command -v netstat || echo 也没有)"; fi'
note '# 后台残留检查：demo 里起过的只有 sleep，全部用 $! 记录并当场 kill/wait 或靠 trap EXIT 收尾。'
runc 'echo "此刻系统里还有没有 demo 留下的 sleep 进程（应当只剩你自己终端里的，或者干脆没有）："; pgrep -a sleep | head -5 || echo "pgrep 说没有（退出码 $?）"'

printf '\n[全部跑完] 上面每一段都是真实执行结果；README.md 引用的输出全部出自这一次运行。\n'
printf '[一句话回顾] 管道 = 每条命令各自的进程 + 三个 fd 的接力；重定向 = 命令启动前改 fd 指向；\n'
printf '              | 从左到右流，同一行的多个重定向按书写的先后顺序逐个生效。\n'
printf '[沙箱即将删除] 由 trap cleanup EXIT 负责；想看现场就 bash demo.sh --keep。\n'
exit 0
