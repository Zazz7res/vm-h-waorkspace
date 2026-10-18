#!/usr/bin/env bash
# CHECK: RUN=非复现 理由：第 5 节那几格当场印它自己记下的后台写进程 pid、「删名之后 fd 里又涨了多少字节」
#        以及第 11 格不加锁那组的竞态末值 —— 这些由内核调度决定，同一台机器两趟就是两个值。
#        ★ 实测过这个「不复现」到底有多大：连跑两遍全章 896 行，只有 5 行不同（第 413/417/424/473/475 行，
#          3 行是 pid、2 行是竞态末值），其余逐字一样，第 13 节那五格两遍逐字一样。
#        免除的证据只有「两遍逐字相同」这一条；rc、残留、隐私、每格的收口断言照旧从严。
#   另注：本 demo 自己造的那些量都不抖 —— 沙箱随机后缀不上屏，inode **号**也不上屏
#        （实测两趟同一份脚本印出 8427 与 8446，所以只报「换没换、挂在哪个名字上」这两种与分配顺序无关的判据），
#        权限、计数、排序、退出码都与随机无关。
# ============================================================================
# 第 10 章《综合演练：批量重命名 / 目录校验 / 日志轮转 / 把交互操作固化成脚本》—— 整章证据一键复现
#
#   bash demo.sh            跑完自动删沙箱（默认）
#   bash demo.sh --keep     跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt  重定向时自动关掉颜色，README 引用的就是这份纯文本
#
# 屏幕上出现的每一行，都是这条命令在这台机器上的真实结果 —— 没有一行是手写的。
#
# 这一章和前九章的分工：前九章讲的是「一件工具怎么使」，这一章是「四件活怎么干成」。
# 四条主线，每条都从**真会咬人的现场**开始，而不是从范例开始：
#   主线① **批量重命名**：难的不是 `mv`，是「名字」这两个字 —— 名字从哪儿来（ls？glob？find？）、
#          一个名字里有空格/换行/开头的 `-` 时那些写法各把它碎成几段、跑第二遍会不会把刚改好的再改一遍。
#   主线② **目录校验**：清单是「路径 + 内容」这一对东西的快照。排序口径、key 用 basename 还是整条路径、
#          mtime 能不能当证据、`find` 遇到不可读目录时那份清单到底完不完整 —— 全是可数的。
#   主线③ **日志轮转**：轮转动的是**目录项**，不是那个正在被写的东西。fd 跟 inode 走这一条
#          能把「日志怎么就没了」讲透，而且本机有 `/usr/sbin/logrotate`，可以直接拿真工具对答案。
#   主线④ **把交互操作固化成脚本**：同一个词在你手敲时和脚本里不是同一个东西（本机 `mv` 在交互 shell
#          里是 `mv -i` 的别名、在脚本里就是 `/usr/bin/mv`），而「等确认」的程序在管道里读到的是 EOF。
#          再往深一层：程序会自己看 fd 是不是 tty，所以你复制进脚本的颜色和进度条会凭空消失。
#
# 本章的取证路线（脚本落地后本节会重写成真实清单）：
#   第 1 节   钉死环境：bash 版本、语言环境、本章要用的工具有哪几件在场（不在场的先说明，不演）
#   第 2 节   一张地图：四条主线各一条最小证据
#   第 3 节   rename.sh 完整跑
#   第 4 节   dircheck.sh 完整跑
#   第 5 节   logrotate.sh 完整跑
#   第 6 节   interact.sh 完整跑
#   第 7 节   小证据：谁还开着那个已被轮转走的日志（三种口径互证）
#   第 8 节   小证据：把「批量改名」写成能重跑的形状（dry-run → 真跑 → 反向恢复，全程只报计数）
#   第 9 节   配方：四条演练共用的那五步
#   第 10 节  同一批配方交给 /bin/sh（dash）跑：哪几条成立、哪几条直接没了
#   第 11 节  危险动作对照：crontab / systemctl / 真实服务为什么本章一次都不调用
#   第 12 节  对账：本章各几格（两个口径 + 守卫先弄响）
#   第 13 节  自证：这一章没动过你的东西
#
# 安全说明（这一章比前九章更需要它，因为四条主线天生挨着「真的会改你文件」的动作）：
#   * 所有写操作只发生在 mktemp -d 的沙箱里（前缀 bash10-），由 trap cleanup EXIT 整体删掉；
#     四支取证脚本各自只在自己的子目录（rn dc lt ia）里造文件，跑完自己删并当场自证。
#   * **绝不碰你已有的任何文件**：本章演示的目录树、日志、清单全部由脚本当场造、当场校验、当场删。
#   * **绝不修改** ~/.bashrc、~/.profile、~/.config、~/.local、/etc：要看交互 shell 的行为一律
#     `bash -ic '…'` 起新进程；要看 tty 行为一律用 /usr/bin/script 在沙箱里造伪终端，不占用你的终端。
#   * **crontab 与 systemctl 在本机在场，但本章一次都不调用**（不装定时任务、不碰任何服务单元）。
#     第 13 节把这条从「我承诺」扫成「计数器先弄响、再报 0」。
#   * 后台进程只准是本脚本自己起的，全部 `wait` 收尾；kill 只 kill 自己记下的 pid，绝不 pkill 名字。
#   * 不联网、不装包、不开监听端口。
#   * 屏幕上不出现真实身份：家目录、用户名、主机名一律当场折成 <家目录> <用户> <主机名>
#     （本机 hostname 与 $USER 是**同一个串**，所以这两条必须各自折，一条顶不了另一条）；
#     沙箱绝对路径（含 mktemp 随机后缀）每一节输出都折成 <沙箱>，只有 --keep 时才印完整路径。
#   * 耗时、内存占用一律不上屏。本章能给的确定性是「计数、掩码后的形状、排序、布尔、退出码」这五种。
#
# 为什么这个脚本**故意不加** set -e：
#   本章的原料一半是失败现场 —— 覆盖丢文件、轮转丢日志、EOF 时 read 的 rc=1、yes 被 SIGPIPE 打死、
#   logrotate 对空文件与缺失文件的不同回答。全局 -e 会让第 2 节第一条就把 demo 停住。
#   各取证脚本内部要 -e 时只在自己那个子进程里开。
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) printf '用法: bash demo.sh [--keep]\n'; exit 0 ;;
        *) printf '未知参数：%s（本脚本只认 --keep）\n' "$arg" >&2; exit 2 ;;
    esac
done

# 把报错钉成中文：这台机器的 bash/外部命令报错本来就是中文的，四支脚本的解说词按中文原文写，
# README 围栏里引用的才和屏上一致。换语言只换「那句话用什么字写」，不换 rc 与计数 —— 第 1 节当场验。
ORIG_LANG="${LANG:-（未设）}"
ORIG_LC_ALL="${LC_ALL:-（未设）}"
ORIG_LANGUAGE="${LANGUAGE:-（未设）}"
export LANG=zh_CN.UTF-8 LC_ALL=zh_CN.UTF-8 LANGUAGE=zh_CN:zh
export ORIG_LANG ORIG_LC_ALL ORIG_LANGUAGE

if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/bash10-XXXXXX")
BEFORE_LIST=$(cd "$HERE" && ls -A | sort | tr '\n' '|')
BEFORE_SUM=$(cd "$HERE" && wc -c *.sh | tail -1 | awk '{print $1}')
# ★ 第 13 节那条「章目录一致」是起新进程判的：不设 export 就会退化成「空串 = 空串」的恒真守卫。
export HERE BEFORE_LIST BEFORE_SUM
# 家目录的「跑前跑后一样」也在这里定格：只记条目数与名字集合的校验和 —— 名单本身一个字段都不上屏，
# 那里面是你自己的文件名，本章没有权利把它们印进一本书里。
HOME_LS_BEFORE=$(ls -A "$HOME" | LC_ALL=C sort | cksum | cut -d' ' -f1)
HOME_N_BEFORE=$(ls -A "$HOME" | wc -l)
export HOME_LS_BEFORE HOME_N_BEFORE

# 两本内部账本（都不上屏，只给第 12、13 节当原料）：
#   all.out  = 每一趟 runc 折完身份之后的真实输出，逐趟累加 —— 第 12 节的「屏幕口径」数的就是它，
#              不是源码里的标题语句；两本账对不上就说明有标题写了没跑、或者跑漏了一节。
#   foldhits = 每一次折叠之前命中的「真实身份行数」—— 第 13 节用它把「屏幕上不出现身份」
#              从一句承诺扫成一个数字：命中过 >0 才证明这道折叠真的在挡东西，而不是没东西可挡。
: > "$SB/all.out"
: > "$SB/foldhits"

banner() {
    printf '\n══════ %s %s\n' "$1" "════════════════════════════════"
    printf '\n##SEC## %s\n' "$1" >> "$SB/all.out"
}
note() { printf '%s\n' "$1"; }

# 身份折叠：任何可能被 bash 带上家、带上沙箱的原文，上屏前先过一遍。
# 注意顺序与「同一个串被折两次」这件事：$USER 与 $(hostname) 在本机相等，谁先匹配谁赢 ——
# 所以两条都要在，且各自换成自己的占位符，第 1 节把这件事印出来。
# 落 tmp 文件而不是 $(cat)：命令替换会吃掉结尾的空行，捕获的排版就不跟屏上一模一样了。
fold() {
    local tmp="$SB/.foldtmp" hits
    cat > "$tmp"
    hits=$(grep -c -F -e "$HOME" -e "$USER" -e "$(hostname)" -e "$SB" -e "$PWD" "$tmp")
    printf '%s\n' "${hits:-0}" >> "$SB/foldhits"
    sed -E "s#$SB#<沙箱>#g; s#$PWD#<沙箱>#g; s#$HOME#<家目录>#g; s#\\b$USER\\b#<用户>#g; s#$(hostname)#<主机名>#g" \
        "$tmp" | tee -a "$SB/all.out"
    rm -f "$tmp"
}

# 打印命令 → 用 bash -c 真跑 → 打印退出码。每条都在全新的非交互 bash 里开始，
# 上一节开的 shopt / 别名 / 陷阱不会串到下一节；2>&1 把报错并进 stdout，捕获里才留得下原文。
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1 | fold
    printf '[退出码 %d]\n' "${PIPESTATUS[0]}"
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去接着敲： cd "%s/work"     看完删掉： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] 前缀 bash10-（随机后缀不进屏幕；只有 --keep 时才印出完整路径）\n'
    fi
}
trap cleanup EXIT

# 取证脚本 + 本 demo 自己复制进沙箱，所有加工都在副本上进行（脚本里写的都是相对路径）。
# ★ 一律用相对文件名跑（bash rename.sh，不是 bash /绝对路径/rename.sh）：报错原文的第一个字段就是 $0，
#   跑绝对路径会把沙箱绝对路径印上屏（第 09 章第 10 节专门演过这件事）。
mkdir -p "$SB/work"
cp "$HERE"/*.sh "$HERE/redline.tsv" "$SB/work/"
cd "$SB/work" || exit 1
umask 022
# 「跑前跑后一样」这笔账要按子脚本自己那间屋子定格：rename.sh 的第 10 格拿 RN_BEFORE_* 跟它自己 cwd 的终值比。
# 拿章目录那份（BEFORE_*）去比是跨目录比 —— 沙箱里多一个 redline.tsv、章目录里多一个 README.md，都会把好判据答成 FAIL。
RN_BEFORE_LIST=$(ls -A | sort | tr '\n' '|')
RN_BEFORE_SUM=$(find . -maxdepth 1 -mindepth 1 -type f -printf '%s\n' | awk '{s=s+$1} END{print s+0}')
export RN_BEFORE_LIST RN_BEFORE_SUM

# ============================================================================
banner "第 1 节 先把这台机器的发言权确认一遍"
# ============================================================================
note '# 本章每条结论都挂在三件事上：哪个 bash、外部命令是谁家的、报错用什么语言说。'
note '# 还有一件更要紧的：本章四个主题各自都有一位「业界标准工具」，本机有的能用、有的压根不在场。'
note '# 不在场的就明说不演 —— 这一节那张名单是本章后面所有「为什么没有 XXX」的出处。'
runc 'bash --version | head -1'
runc 'echo "demo 已把自己改成：LC_ALL=[$LC_ALL] LANG=[$LANG] LANGUAGE=[$LANGUAGE]"'
runc 'echo "你原来的终端是 LANG=$ORIG_LANG、LC_ALL=$ORIG_LC_ALL、LANGUAGE=$ORIG_LANGUAGE —— 只在 demo 的子进程里被改掉，你的 shell 一个字都没动"'
note '# 本章真正会调用到的命令，先逐个点名（不是假设它们在）：答出路径的是外部命令，答出自己名字的（printf）是 bash 内建。'
runc 'for t in mv find sort comm sha256sum md5sum cksum stat du flock lsof fuser logrotate script gzip xargs printf; do printf "  %-10s → %s\n" "$t" "$(command -v "$t" || echo 不在 PATH)"; done'
note '#   三个细节：logrotate 在 /usr/sbin（普通 PATH 未必收它，本章用绝对路径与沙箱配置跑它）；'
note '#   script 是唯一能造出「真 tty」的东西 —— 第 6 节那张 -t 真值表全靠它，没有它这一节只能空口说。'
note '#   printf 那一行答的是「printf」而不是路径 —— 它先是 bash 内建；本章要用外部的 /usr/bin/printf 时会写全路径。'
note '# 业界常拿来替代手写的这几件，本机在不在场？不在场的那几件本章一句代码都不演：'
runc 'for t in rename expect unbuffer inotifywait entr parallel sponge rsync crontab systemctl; do printf "  %-12s %s\n" "$t" "$(command -v "$t" 2>/dev/null || echo 无)"; done'
note '#   ★ rename（Perl 那个批量改名工具）**不在场** —— 所以主线①所有批量改名都是 mv + glob/find 手拼，'
note '#     这也正好把「名字从哪儿来」这一层坑逼出来了：没有捷径可抄。'
note '#   ★ expect / unbuffer 不在场，所以主线④不靠「模拟按键喂提示」，改用 stdin 重定向、heredoc 与伪 tty 三条实测路。'
note '#   ★ inotifywait / entr 不在场，所以「日志被动了就轮转」这类监听写法本章**没有数据**，不许你照抄。'
note '#   ★ crontab / systemctl **在场但本章零调用**：装定时任务和动服务单元都不是「学脚本」需要的东西，'
note '#     而且两者都会写到沙箱外面去。第 13 节拿计数器把这条扫给你看。'
note '#   ★ rsync 在场，但本章一次都没用它：两份清单的差在第 4 节第 5 格用 comm 手工拼 —— 真要增量同步才轮得到它，本章不演。'
runc 'printf "  本机 hostname 与 \$USER 是同一个串吗=%s\n" "$( [ "$(hostname)" = "$USER" ] && echo 是★（所以两条折叠规则必须都在）|| echo 不是 )"'
runc 'printf "  /bin/sh 真身=[%s]\n" "$(readlink -f /bin/sh)"'
note '# 语言环境到底改不改判定？同一条错误在两种语言下各来一次，只看 rc 变不变：'
runc 'o1=$(bash -c nosuchcmd 2>&1 | head -1); r1=$(bash -c nosuchcmd >/dev/null 2>&1; echo $?)
o2=$(env LC_ALL=C.UTF-8 LANGUAGE=C bash -c nosuchcmd 2>&1 | head -1); r2=$(env LC_ALL=C.UTF-8 LANGUAGE=C bash -c nosuchcmd >/dev/null 2>&1; echo $?)
printf "  钉好的中文那句=[%s]\n  换英文那句=[%s]\n" "$o1" "$o2"
printf "  两次的 rc=[%s / %s] ｜ 两句字面是否相同=%s\n" "$r1" "$r2" "$( [ "$o1" = "$o2" ] && echo 相同 || echo 不同字 )"'
if [ "$(bash -c 'nosuchcmd' 2>&1 | grep -c 未找到命令)" -eq 0 ]; then
    note '  ★ 警告：这台机器没有中文语言包，报错会出英文 —— README 围栏里的中文原文会和你看到的不一致。'
    note '    但 rc、计数、形状全部不变；本章没有一条结论依赖那句话用什么字写。'
else
    note '  → 钉住了：这份捕获里的报错原文是中文。所以本章所有判据都写成与语言无关的东西'
    note '    （rc、命中次数、掩码后的形状）—— 后面每节都在兑现这条。'
fi

# ============================================================================
banner "第 2 节 一张地图：四条主线各一条最小证据"
# ============================================================================
note '# 四条主线、四句话，每句一条当场跑出来的最小证据。后面每一节都是这四句的展开。'

note '① 批量改名：同一个目录，两种「拿到名字」的写法，拿到的段数不一样。'
cat > map1.sh <<'EOF'
mkdir -p rn0 && touch 'rn0/a b.txt' 'rn0/c.txt'
a=$(for f in $(ls rn0); do printf '[%s]' "$f"; done)
b=$(for f in rn0/*; do printf '[%s]' "$(basename "$f")"; done)
na=$(printf %s "$a" | tr -dc '[' | wc -c)
nb=$(printf %s "$b" | tr -dc '[' | wc -c)
printf "  走 ls 那趟：段数=%s ｜ 逐段=%s\n" "$na" "$a"
printf "  走 glob 那趟：段数=%s ｜ 逐段=%s\n" "$nb" "$b"
printf "  名字里只有一个空格，段数就从 %s 变 %s —— 多出来的那一段根本不是文件\n" "$nb" "$na"
rm -rf rn0
EOF
runc 'bash map1.sh'
note '   ★ 第 3 节把这条推到换行、开头的 `-`、以及 `find -print0` 配 `read -d ""` 那一整套。'

note '② 目录校验：mtime 相同 ≠ 内容相同 —— 两个口径并排就露馅。'
cat > map2.sh <<'EOF'
mkdir -p dc0 && printf '第一版\n' > dc0/f.txt
touch -d '2020-01-01 00:00:00' dc0/f.txt
h1=$(sha256sum dc0/f.txt | cut -c1-12); m1=$(stat -c %Y dc0/f.txt)
printf '第二版\n' > dc0/f.txt
touch -d '2020-01-01 00:00:00' dc0/f.txt
h2=$(sha256sum dc0/f.txt | cut -c1-12); m2=$(stat -c %Y dc0/f.txt)
printf "  改完内容并把 mtime 改回去：mtime 判为「没变」=%s ｜ 摘要判为「没变」=%s\n" \
  "$([ "$m1" = "$m2" ] && echo 1 || echo 0)" "$([ "$h1" = "$h2" ] && echo 1 || echo 0)"
printf "  前 12 位摘要：改前=[%s] 改后=[%s] ｜ mtime 两次数值=[%s / %s]\n" "$h1" "$h2" "$m1" "$m2"
rm -rf dc0
EOF
runc 'bash map2.sh'
note '   ★ 上面那行的判据是「mtime 判为没变=1、摘要判为没变=0」：前者被 touch 骗过去了，后者没有。'
note '   ★ 第 4 节把「校验到底校了什么」拆成内容改动 / 文件缺失 / 不可读 三种破坏，各数一次 rc。'

note '③ 日志轮转：mv 走的是目录项，正在写的那个 fd 跟着 inode 走 —— 两条路当场分开。'
cat > map3.sh <<'EOF'
mkdir -p lt0 && printf 'a\n' > lt0/app.log
i1=$(stat -c %i lt0/app.log)
mv lt0/app.log lt0/app.log.1
: > lt0/app.log
i2=$(stat -c %i lt0/app.log)
printf "  轮转后 app.log 的 inode 换了吗=%s（「换了」= 这个名字现在指向另一个东西）\n" \
  "$([ "$i1" = "$i2" ] && echo 没换 || echo 换了★)"
printf "  老 inode 现在挂在哪个名字上=[%s] ｜ 它和新建那个 app.log 是同一个东西吗=%s\n" \
  "$(find lt0 -inum "$i1" -printf '%f\n')" "$([ "$i1" = "$i2" ] && echo 是 || echo 不是★)"
printf "  目录里文件数=%s ｜ 新日志字节数=%s（新那个是 : > 出来的空壳）\n" \
  "$(ls lt0 | wc -l)" "$(wc -c < lt0/app.log)"
rm -rf lt0
EOF
runc 'bash map3.sh'
note '   ★ 「换了」这两个字就是第 5 节整节的根：谁还开着老 inode，日志就写进谁那里 —— 新文件永远是空的。'

note '④ 固化交互操作：同一个 read，喂与不喂是两种世界；同一个 mv，两个 shell 里是两种身份。'
cat > map4.sh <<'EOF'
f='read x; rc=$?; printf "读到=[%s] read 自己的 rc=%s" "$x" "$rc"'
printf '  不喂（stdin 是 /dev/null）：%s\n' "$(bash -c "$f" </dev/null)"
printf '  喂一行进去：%s\n' "$(printf '喂进来的\n' | bash -c "$f")"
printf "  非交互 shell（bash -c）：type -t mv=[%s] ｜ command -v mv=[%s]\n" \
  "$(bash -c 'type -t mv')" "$(bash -c 'command -v mv')"
printf "  交互 shell（bash -ic）：%s\n" "$(bash -ic 'type mv' </dev/null 2>/dev/null | head -1)"
EOF
runc 'bash map4.sh'
note '   ★ 同一个词 `mv`，在你手敲时是「会问确认」的别名，在脚本里是「直接覆盖」的程序 ——'
note '     「把交互操作固化成脚本」真正的坑不在语法，而在**两个入口根本不是同一个环境**。'
note '   ★ 第 6 节把这条钉成一张表：`-t 0`／`-t 1`／`$-` 里的 i／login_shell 四把尺 × 四种入口。'
rm -f map1.sh map2.sh map3.sh map4.sh

# ============================================================================
banner "第 3 节 rename.sh 完整跑：批量重命名，坑全在「名字从哪儿来」这一端"
# ============================================================================
note '# 这一节没有解说词，只有整支脚本的原样输出 —— 10 格逐格跑，每格自己带一行断言。'
note '# 看的时候盯一件事：每一格末尾那句「→ 断言」里的数字，都是当场数出来的，不是写死的。'
runc 'bash rename.sh'
note '   ★ 这一节的对账口径（一共几格、断言行几行）在第 12 节，由脚本自己去数，我这里不写死数字。'

# ============================================================================
banner "第 4 节 dircheck.sh 完整跑：目录校验，绿灯到底证明了什么"
# ============================================================================
note '# 校验 = 拿一份「路径 + 内容」的快照跟现状对。这一支拆的是三件事：清单是谁排的序、key 取的哪一列、'
note '# 以及最要紧的 —— 那个 rc=0 到底覆盖了几个文件。'
runc 'bash dircheck.sh'
note '   ★ 第 6 节那条「不可读目录让清单天生少一个文件，而校验器照样 rc=0」是本节的正菜：'
note '     它不是工具的错，是你把「清单上有的都对」听成了「目录没变」。'

# ============================================================================
banner "第 5 节 logrotate.sh 完整跑：轮转动的是目录项，不是那个正在被写的东西"
# ============================================================================
note '# 本机有 /usr/sbin/logrotate，这一节拿真工具对答案：mv 型轮转（create）与 copytruncate 的差别、'
note '# 空文件与缺失文件它各说什么、配置文件里那行路径写错时它怎么回答。'
runc 'bash logrotate.sh'
note '   ★ 全程只用沙箱配置与沙箱状态文件（-c 与 -s 都指到沙箱里），一次都没有碰 /etc/logrotate.d。'

# ============================================================================
banner "第 6 节 interact.sh 完整跑：把交互操作固化成脚本，两个入口不是同一个环境"
# ============================================================================
note '# 你手敲时 shell 给你的东西，脚本里一样都没有：别名、提示符、tty、读到 EOF 时 return 的那个值。'
note '# 这一支用 /usr/bin/script 在沙箱里造一个伪 tty，把「程序自己看 fd 是不是 tty」这件事演成可数的行。'
runc 'bash interact.sh'
note '   ★ 第 2 节那条 mv 别名只是开胃菜：真正的坑是**同一个程序在 tty 前后给你两套输出**，'
note '     以及「等确认」的 read 在管道里不是卡住，而是当场拿到 EOF 并 return 1。'

# ============================================================================
banner "第 7 节 小证据：谁还开着那个已被轮转走的日志（四种口径互证）"
# ============================================================================
note '# 第 2 节证明了 inode 换了，这一节接着问：那个正在写的进程知不知道？它写的字去了哪儿？'
note '# 四个口径各自独立回答同一个问题：／proc 直查、／proc 全扫、lsof、fuser，再加一条 inode 对比。'
note '# 起后台的写进程由旗标文件对时（不用 sleep 猜时长），收尾一定 wait 并当场证明那个 pid 没了。'
cat > wr.sh <<'EOF'
exec 3>>"$1"
for i in 1 2 3 4 5 6 7 8 9 10; do printf '轮转前第 %s 行\n' "$i" >&3; done
n=0
while [ ! -e "$2" ]; do n=$((n + 1)); [ "$n" -gt 400 ] && exit 91; sleep 0.05; done
for i in 1 2 3 4 5 6 7 8 9 10; do printf '轮转后第 %s 行\n' "$i" >&3; done
n=0
while [ ! -e "$3" ]; do n=$((n + 1)); [ "$n" -gt 400 ] && exit 92; sleep 0.05; done
exec 3>&-
EOF
cat > openfd.sh <<'EOF'
rm -rf lf
mkdir -p lf
log=lf/app.log
old=lf/app.log.1
bash wr.sh "$log" lf/rot lf/done &
WP=$!
n=0
while [ ! -e "$log" ] || [ "$(wc -l < "$log")" -lt 10 ]; do
    n=$((n + 1))
    [ "$n" -gt 400 ] && break
    sleep 0.05
done
L1=$(wc -l < "$log"); F1=$(basename "$(readlink "/proc/$WP/fd/3")")
printf '格1 轮转前：日志行数=%s ｜ 写进程 fd 3 指向的文件名=[%s]\n' "$L1" "$F1"
printf '   → 断言：基线这两个数=%s / [%s]（期望 10 / app.log）—— 对不上就说明写进程压根没起来，后面九格都不用看。\n' "$L1" "$F1"
mv "$log" "$old"
: > "$log"
D2=$(ls lf | tr '\n' ' '); N2=$(wc -l < "$log")
printf '格2 轮转完：目录里=[%s] ｜ 新 log 行数=%s\n' "$D2" "$N2"
printf '   → 断言：目录里两个名字都齐吗=%s（期望 齐）｜ 新那份行数=%s（期望 0：刚 : > 出来）—— 这一格只证明「目录项换了」。\n' \
  "$([ "$(printf %s "$D2" | wc -w)" = 2 ] && echo 齐 || echo 缺★)" "$N2"
touch lf/rot
n=0
while [ "$(wc -l < "$old")" -lt 20 ]; do
    n=$((n + 1))
    [ "$n" -gt 400 ] && break
    sleep 0.05
done
O3=$(wc -l < "$old"); W3=$(wc -l < "$log")
printf '格3 又写了 10 行之后的落点：老那个行数=%s ｜ 新那个行数=%s\n' "$O3" "$W3"
printf '   → 断言：%s + %s 等于 20 吗=%s（期望 等于：第二批 10 行全落进「已被轮走」那个名字背后）｜ 新那份还是 %s 行（期望 0）。\n' \
  "$O3" "$W3" "$([ "$((O3 + W3))" = 20 ] && echo 是 || echo 否★)" "$W3"
FD4=$(readlink "/proc/$WP/fd/3")
printf '格4 口径①／proc 直查：fd 3 现在指向的文件名=[%s] ｜ 这个链接跟的是名字还是那个东西本身=%s\n' \
  "$(basename "$FD4")" "$([ "$FD4" = "$log" ] && echo 跟名字 || echo 跟东西本身★)"
printf '   → 断言：fd 3 如今指 [%s]（期望 app.log.1，不是新的 app.log）—— 名字换了、fd 一个字没动，★ 就是这一格要的答案。\n' "$(basename "$FD4")"
holders=''
tgt=$(cd lf && printf '%s/app.log.1' "$(pwd -P)")
for d in /proc/[0-9]*/fd; do
    for l in "$d"/*; do
        [ "$(readlink "$l" 2>/dev/null)" = "$tgt" ] && holders="$holders $(basename "${d%/fd}")"
    done
done
H5=$(printf %s "$holders" | wc -w)
M5=$(case " $holders " in *" $WP "*) echo 是 ;; *) echo 不是★ ;; esac)
printf '格5 口径②／proc 全扫：扫出几个进程开着老那个=%s ｜ 它是不是我记下的那个 pid=%s\n' "$H5" "$M5"
printf '   → 断言：%s 个持有者（期望 1 个），且就是我记下的那个=%s（期望 是）—— 这条口径不靠工具、不靠名字，最原始也最硬。\n' "$H5" "$M5"
LOF=$(lsof "$old" 2>/dev/null | awk 'NR>1')
L6=$(printf %s "$LOF" | grep -c .)
C6=$(printf %s "$LOF" | awk '$4 ~ /^3/ {print $4}')
P6=$(printf %s "$LOF" | awk -v w="$WP" '$2==w {c++} END {print (c==1) ? "是" : "不是★"}')
printf '格6 口径③lsof：按名字查到的持有者行数=%s ｜ FD 那一列=[%s] ｜ 报的 pid 是不是我那个=%s\n' "$L6" "$C6" "$P6"
printf '   → 断言：%s 条（期望 1）、fd 号 [%s]（期望 3w：3 号、只写）＝与格5 那条独立口径互相咬上吗=%s。\n' \
  "$L6" "$C6" "$([ "$L6" = "$H5" ] && [ "$P6" = 是 ] && echo 咬上了 || echo 没咬上★)"
FR=$(cd lf && fuser -s app.log.1 2>/dev/null; echo $?)
FH=$(cd lf && fuser app.log.1 2>/dev/null)
M7=$(case " $(printf %s "$FH" | tr -s ' ') " in *" $WP "*) echo 有 ;; *) echo 没有★ ;; esac)
printf '格7 口径④fuser：rc=%s（0=有人在用、1=没人用）｜ 它返回的 pid 里有没有我那个=%s\n' "$FR" "$M7"
printf '   → 断言：rc=%s（期望 0）｜ 命中我那个 pid=%s（期望 有）—— 四条口径到这儿全是同一个答案；pid 数字本身每跑必变，所以只报命中与否。\n' "$FR" "$M7"
SAME8=$([ "$(stat -c %i "$log")" = "$(stat -c %i "$old")" ] && echo 是 || echo 不是)
printf '格8 口径⑤inode：新建的 app.log 与 app.log.1 是同一个 inode 吗=%s\n' "$SAME8"
printf '   → 断言：%s（期望 不是）—— 名字挨着、内容看着没关系，inode 早就分家了；这一条与格4 是同一件事的两个说法。\n' "$SAME8"
touch lf/done
wait "$WP"; rc=$?
AL9=$([ -d "/proc/$WP" ] && echo 在 || echo 不在)
O9=$(wc -l < "$old"); W9=$(wc -l < "$log"); S9=$((O9 + W9)); B9=$(wc -c < "$log")
printf '格9 收尾对账：写进程 rc=%s ｜ /proc 里这个 pid 还在吗=%s ｜ 老=%s 行 + 新=%s 行 =%s（应为 20）｜ 新那个字节数=%s\n' \
  "$rc" "$AL9" "$O9" "$W9" "$S9" "$B9"
printf '   → 断言：三件同时成立才算收干净：rc=%s（期望 0）、pid 已消失=%s（期望 不在）、总数守恒=%s（期望 20）。\n' \
  "$rc" "$AL9" "$([ "$S9" = 20 ] && echo 是 || echo 否★)"
R10=$(ls lf | tr '\n' ' ')
printf '格10 轮转完的残留清单=[%s]\n' "$R10"
printf '   → 断言：lf/ 里剩 %s 个条目（期望 4：两份日志 + 两个对时旗标）—— 多出来的就是哪一格落的临时文件，下一格整目录删掉。\n' \
  "$(printf %s "$R10" | wc -w)"
EOF
runc 'bash openfd.sh'
note '   ★ 四个口径给出同一个答案：老那个文件没消失，它只是改了名字，而被改名的那一刻没有任何人通知那个 fd。'
note '   ★ fuser 那格只报「有没有命中我记下的 pid」，不报它返回的原始 pid 列表 —— 本机实测那串里'
note '     常常多出一个当场就消失的瞬时 pid（同一趟 /proc 全扫与 lsof 都只报 1 个持有者），'
note '     所以 pid 数字本身每跑必变，本章一律把它折成布尔判定，不当证据用。'

# ============================================================================
banner "第 8 节 小证据：把「批量改名」写成能重跑的形状（计划 → 真跑 → 反向恢复）"
# ============================================================================
note '# 第 3 节拆的是「名字怎么拿」，这一节拆的是「怎么让它敢跑第二遍」：'
note '# 同一条 mv 分三种身份跑 —— 只打印计划、真执行、按刚记下的映射反向放回去。全程只报计数。'
cat > rerun.sh <<'EOF'
rm -rf r8 && mkdir -p r8/bk
cd r8 || exit 1
for n in 1 2 3; do printf '内容 %s\n' "$n" > "报告 第${n}期.txt"; done
cp *.txt bk/
# ★ 快照只数 *.txt，绝不数 ls -A：计划文件（plan.tsv）与错误捕获文件（err*.log）也躺在同一个目录里，
#   用 ls -A 就会把「我刚写的计划」当成「目录里多出来一个文件」，第 2 格那条断言当场就变成假警报。
snap()  { printf '%s\n' *.txt | LC_ALL=C sort | tr '\n' '|'; }
cnt()   { printf '%s\n' *.txt | wc -l; }
bytes() { wc -c *.txt | tail -1 | awk '{print $1}'; }
restore() { rm -f *.txt; cp bk/*.txt .; }
before=$(snap); bsum=$(bytes); n_before=$(cnt)
printf '格1 起点：txt 个数=%s ｜ 名单=[%s] ｜ 字节总和=%s\n' "$n_before" "$before" "$bsum"
printf '   → 断言：起点三个数各就各位：个数=%s（期望 3）｜ 字节=%s ｜ 名单非空=%s —— 后面每一格都拿这三个数对照，起点错了全节都错。\n' \
  "$n_before" "$bsum" "$([ -n "$before" ] && echo 是 || echo 否★)"

: > plan.tsv
i=0
for f in *.txt; do i=$((i + 1)); printf '%s\treport-%02d.txt\n' "$f" "$i" >> plan.tsv; done
printf '格2 计划先行（这一步一条 mv 都没跑）：计划行数=%s ｜ txt 名单变了吗=%s\n' \
  "$(wc -l < plan.tsv)" "$([ "$(snap)" = "$before" ] && echo 没变 || echo 变了★)"
printf '   计划内容（旧名 → 新名，制表符分隔）：\n'
sed 's/^/      /' plan.tsv
printf '   → 断言：计划行数=%s 与起点 txt 个数=%s 相等吗=%s ｜ 名单现在动了吗=%s（期望 相等、没动：这一步只写 plan.tsv，一条 mv 都没跑）\n' \
  "$(wc -l < plan.tsv)" "$n_before" "$([ "$(wc -l < plan.tsv)" = "$n_before" ] && echo 相等 || echo 不等★)" \
  "$([ "$(snap)" = "$before" ] && echo 没变 || echo 变了★)"

mvcount=0
while IFS=$'\t' read -r src dst; do
    [ -f "$src" ] || continue
    mv -- "$src" "$dst"
    mvcount=$((mvcount + 1))
done < plan.tsv
printf '格3 真跑：实际执行 mv 的次数=%s ｜ 现在名单=[%s] ｜ 字节总和=%s ｜ 与起点一样吗=%s\n' \
  "$mvcount" "$(snap)" "$(bytes)" "$([ "$(bytes)" = "$bsum" ] && echo 一样 || echo 不一样★)"
printf '   → 断言：mv 次数=%s（期望 3＝计划行数）｜ 字节总和=%s 与起点 %s 一样吗=%s（期望 一样：改名不动内容，动了就是内容被换过）\n' \
  "$mvcount" "$(bytes)" "$bsum" "$([ "$(bytes)" = "$bsum" ] && echo 一样 || echo 不一样★)"
hits=0
while IFS=$'\t' read -r src dst; do
    [ -f "$src" ] && hits=$((hits + 1))
done < plan.tsv
printf '格4 把同一条计划原样再跑一遍：这次还能命中几条源文件（命中 0 才是可重跑的形状）=%s\n' "$hits"
printf '   → 断言：命中=%s 条（期望 0）—— 这一格的「成功」恰恰是什么都没发生：计划是按旧名写的，旧名已经不在原位了。\n' "$hits"
back=0
while IFS=$'\t' read -r src dst; do
    [ -f "$dst" ] || continue
    mv -- "$dst" "$src"
    back=$((back + 1))
done < <(tac plan.tsv)
printf '格5 反向恢复：按计划逆序放回，放回次数=%s ｜ 名单回到起点了吗=%s ｜ 字节回到 %s 了吗=%s\n' \
  "$back" "$([ "$(snap)" = "$before" ] && echo 回来了 || echo 没回来★)" \
  "$bsum" "$([ "$(bytes)" = "$bsum" ] && echo 是 || echo 否★)"
printf '   → 断言：放回=%s 次（期望 3）｜ 名单=[%s] ｜ 与起点相等吗=%s（期望 相等：现场还原了，格6 才能从同一个起点重新起跑）\n' \
  "$back" "$(snap)" "$([ "$(snap)" = "$before" ] && echo 相等 || echo 不等★)"

: > plan2.tsv
for f in *.txt; do printf '%s\tclash.txt\n' "$f" >> plan2.tsv; done
printf '格6 多对一的计划（%s 条源名 → 同一个目标名 clash.txt）：\n' "$(wc -l < plan2.tsv)"
sed 's/^/      /' plan2.tsv
nv=0
while IFS=$'\t' read -r src dst; do
    [ -f "$src" ] || continue
    mv -- "$src" "$dst"
    nv=$((nv + 1))
done < plan2.tsv 2> err6.log
printf '   无守卫直接跑：mv 执行=%s 次 ｜ 跑完 txt 个数=%s ｜ 名单=[%s] ｜ 字节总和=%s ｜ mv 的报错行数=%s\n' \
  "$nv" "$(cnt)" "$(snap)" "$(bytes)" "$(wc -l < err6.log)"
printf '   → 断言：个数 %s→%s（少 %s 个）｜ 字节 %s→%s（少 %s 字节）｜ 报错行数=%s（判据：少东西 > 0 而报错 = 0，就是静默覆盖）\n' \
  "$n_before" "$(cnt)" "$(( n_before - $(cnt) ))" \
  "$bsum" "$(bytes)" "$(( bsum - $(bytes) ))" "$(wc -l < err6.log)"
restore
g=0; blk=0
while IFS=$'\t' read -r src dst; do
    [ -f "$src" ] || continue
    if [ -e "$dst" ]; then blk=$((blk + 1)); continue; fi
    mv -- "$src" "$dst"
    g=$((g + 1))
done < plan2.tsv 2> err7.log
printf '格7 场景复原（txt 个数=%s ｜ 字节=%s），同一份 plan2.tsv 带 [ -e "$dst" ] 守卫再跑：\n' "$(cnt)" "$(bytes)"
printf '   改了=%s 条 ｜ 撞名挡下=%s 条 ｜ 跑完 txt 个数=%s ｜ 字节总和=%s ｜ 与起点一样吗=%s ｜ 报错行数=%s\n' \
  "$g" "$blk" "$(cnt)" "$(bytes)" "$([ "$(bytes)" = "$bsum" ] && echo 一样 || echo 不一样★)" "$(wc -l < err7.log)"
printf '   → 断言：源文件条数=%s = 改了 %s + 挡下 %s ｜ 名单=[%s] ｜ 判据：挡下数必须 > 0（等于 0 就是守卫从没响过）\n' \
  "$(wc -l < plan2.tsv)" "$g" "$blk" "$(snap)"
cd .. || exit 1
N8=$(find r8 -mindepth 1 | wc -l)
printf '格8 收尾：r8 里的条目数=%s（含 bk/ 与两份计划、两份错误捕获）｜ 删除后还在吗=%s\n' \
  "$N8" "$(rm -rf r8; [ -d r8 ] && echo 还在★ || echo 不在了)"
printf '   → 断言：条目数=%s（期望 11＝bk 目录 1 + 原位 txt 3 + bk 里 txt 3 + 计划 2 + 错误捕获 2）｜ 这回真删净了吗=%s\n' \
  "$([ "$N8" = 11 ] && echo 对上了 || echo 对不上★)" "$([ -d r8 ] && echo 还在★ || echo 不在了)"
printf '     ★ 这一格原来踩过一个坑：判据里再 find 一次，可那时上一行的 $(rm -rf r8) 已经把它删了 —— 数出来永远是 0。\n'
EOF
runc 'bash rerun.sh'
note '   ★ 第 4 格那个「再跑一遍命中 0 条」就是可重跑的定义：计划是按旧名写的，旧名已经不在了，'
note '     所以这条 while 循环一趟都不执行 —— 它不是「跑成功了」，是「压根没跑」。'
note '   ★ 第 6 格与第 7 格用的是同一份 plan2.tsv，唯一区别是有没有那行 `[ -e "$dst" ]`：'
note '     没守卫时 3 个 txt 变 1 个、mv 一声不吭（报错 0 行）；有守卫时改 1 条、挡 2 条、字节回到起点。'
note '     判据落在「挡下数 > 0」上 —— 这一格必须响，第 12 节统一收它。'
rm -f wr.sh openfd.sh rerun.sh

# ============================================================================
banner "第 9 节 配方：四条演练共用的那五步"
# ============================================================================
note '# 前面八节看着像四件互不相干的活，其实骨架都是同五步：'
note '#   ① 取名单（只读，一个文件都不碰）'
note '#   ② 出计划（只往一个计划文件里写，还是不碰原文件）'
note '#   ③ 过守卫（源还在吗、目标撞了吗）'
note '#   ④ 执行并当场计数（改了几条、挡了几条、源不在跳过几条）'
note '#   ⑤ 收尾自证（能按②那张纸撤的就撤，撤不掉的先数清楚）'
note '# 下面把这五步压成一个函数 run_plan，然后换三个动词各跑一遍：改名、校验、轮转 —— 函数一个字都不改。'
cat > recipe.sh <<'RX'
rm -rf rc9 && mkdir -p rc9 && cd rc9 || exit 1
# 执行器：读「源 <TAB> 目标」的制表符计划。$1=计划文件 $2=守卫开关（1 开 0 关）。
# 计数走全局变量而不走 stdout：调用方还要把 mv 的报错并进自己的日志，stdout 不能占。
run_plan() {
    MOVED=0; BLOCKED=0; GONE=0
    while IFS=$'\t' read -r src dst; do
        if [ ! -f "$src" ]; then GONE=$((GONE + 1)); continue; fi
        if [ "$2" = 1 ] && [ -e "$dst" ]; then BLOCKED=$((BLOCKED + 1)); continue; fi
        mv -- "$src" "$dst" && MOVED=$((MOVED + 1))
    done < "$1"
}
snap() { find "$1" -maxdepth 1 -mindepth 1 | LC_ALL=C sort | tr '\n' '|'; }
cntf() { find "$1" -maxdepth 1 -type f | wc -l; }

mkdir -p d1
for n in 1 2 3; do printf '第%s页\n' "$n" > "d1/第 ${n} 页.txt"; done
start=$(snap d1)
printf '格1 动词=改名。步骤①取名单：%s 个文件 ｜ [%s]\n' "$(cntf d1)" "$start"
mkdir -p out
: > p1.tsv
find d1 -maxdepth 1 -type f | LC_ALL=C sort | while IFS= read -r f; do
    printf '%s\tout/%s\n' "$f" "$(basename "$f")"
done > p1.tsv
pre=0
while IFS=$'\t' read -r src dst; do
    [ -e "$dst" ] && pre=$((pre + 1))
done < p1.tsv
printf '   步骤②出计划：计划行数=%s ｜ 内容文件名单动了吗=%s ｜ 步骤③过守卫：目标已存在=%s 条\n' \
  "$(wc -l < p1.tsv)" "$([ "$(snap d1)" = "$start" ] && echo 没动 || echo 动了★)" "$pre"
run_plan p1.tsv 1
printf '   步骤④执行：改了=%s 条 ｜ 挡下=%s 条 ｜ 源不在=%s 条 ｜ 现在 [%s]\n' \
  "$MOVED" "$BLOCKED" "$GONE" "$(snap out)"
tac p1.tsv > p1rev.tsv
back=0
while IFS=$'\t' read -r src dst; do
    [ -f "$dst" ] || continue
    mv -- "$dst" "$src"
    back=$((back + 1))
done < p1rev.tsv
printf '   步骤⑤收尾自证：按逆序撤了=%s 条 ｜ 名单回到起点=%s ｜ out/ 里还剩 %s 个文件\n' \
  "$back" "$([ "$(snap d1)" = "$start" ] && echo 回来了 || echo 没回来★)" "$(cntf out)"
printf '   → 断言：改了 %s 条 = 步骤③放行 %s 条 ｜ 撤完的名单与起点逐字相同=%s（判据：②那张纸能让⑤逐条撤回去）\n' \
  "$MOVED" "$MOVED" "$([ "$(snap d1)" = "$start" ] && echo 是 || echo 否★)"

sha256sum d1/*.txt > sums.list
c1rc=0; sha256sum -c sums.list > c1.out 2>&1 || c1rc=$?
printf '格2 动词换成「校验」。步骤②=生成清单（%s 行）｜ 步骤④=sha256sum -c：rc=%s ｜ OK 行=%s ｜ FAILED 行=%s\n' \
  "$(wc -l < sums.list)" "$c1rc" "$(grep -c ': OK' c1.out)" "$(grep -c FAILED c1.out)"
printf '又改了一行\n' >> "d1/第 2 页.txt"
c2rc=0; sha256sum -c sums.list > c2.out 2>&1 || c2rc=$?
printf '   故意改掉其中一个文件的内容，同一份清单再校一次：rc=%s ｜ OK 行=%s ｜ FAILED 行=%s\n' \
  "$c2rc" "$(grep -c ': OK' c2.out)" "$(grep -c FAILED c2.out)"
grep FAILED c2.out | sed 's/^/      /'
printf '   → 断言：rc %s→%s ｜ OK 行 %s→%s ｜ FAILED 行 %s→%s ｜ 清单行数一个字没变=%s\n' \
  "$c1rc" "$c2rc" "$(grep -c ': OK' c1.out)" "$(grep -c ': OK' c2.out)" \
  "$(grep -c FAILED c1.out)" "$(grep -c FAILED c2.out)" "$(wc -l < sums.list)"
printf '     这个动词下步骤⑤撤不掉内容（摘要是不可逆的），它能给的只有「是哪一条变了」——\n'
printf '     所以用校验的时候，⑤ 的期望值要提前改成「指出」而不是「复原」。\n'

mkdir -p lt
printf '第一批\n' > lt/app.log
printf 'lt/app.log\tlt/app.log.1\n' > rot.tsv
run_plan rot.tsv 1; R1M=$MOVED; R1B=$BLOCKED
: > lt/app.log
printf '第二批\n' >> lt/app.log
run_plan rot.tsv 1; R2M=$MOVED; R2B=$BLOCKED
printf '格3 动词换成「轮转」（同一份 rot.tsv 跑两轮，守卫开着）：\n'
printf '   第一轮 改了=%s 挡下=%s ｜ 第二轮 改了=%s 挡下=%s ｜ 此刻 app.log.1 首行=[%s] ｜ app.log 首行=[%s]\n' \
  "$R1M" "$R1B" "$R2M" "$R2B" "$(head -1 lt/app.log.1)" "$(head -1 lt/app.log)"
run_plan rot.tsv 0
printf '   把守卫关掉再跑一轮（就是老式轮转的写法）：改了=%s 挡下=%s ｜ app.log.1 首行=[%s] ｜ 还能找到第一批吗=%s\n' \
  "$MOVED" "$BLOCKED" "$(head -1 lt/app.log.1)" \
  "$(grep -qc 第一批 lt/app.log lt/app.log.1 2>/dev/null && echo 能 || echo 不能★)"
printf '   → 断言：开守卫时第二轮挡下=%s（第一批保住，两批各在一个文件里）。\n' "$R2B"
printf '     关守卫后又跑一轮：改了=%s，而「内容里还留着第一批」的文件个数=%s（0 就是真没了）\n' \
  "$MOVED" "$(grep -c 第一批 lt/app.log lt/app.log.1 2>/dev/null | grep -v ':0' | wc -l)"

mkdir -p d5
for n in 1 2 3; do printf 'x%s\n' "$n" > "d5/凭据${n}.txt"; done
s5=$(snap d5)
hand=0
for f in d5/凭据*.txt; do
    mv -- "$f" "d5/记录${f##*凭据}"
    hand=$((hand + 1))
done
rev5=0
if [ -f p5.tsv ]; then
    while IFS=$'\t' read -r src dst; do
        [ -f "$dst" ] || continue
        mv -- "$dst" "$src"
        rev5=$((rev5 + 1))
    done < p5.tsv
fi
printf '格4 现场省掉步骤②（直接 mv，一张纸都不落）：mv 跑了=%s 条 ｜ 撤回时能从计划里读到=%s 行 ｜ 名单回到起点=%s\n' \
  "$hand" "$rev5" "$([ "$(snap d5)" = "$s5" ] && echo 是 || echo 否★)"
printf '   → 断言：省掉②之后 ⑤ 能从计划里读到 %s 行（没有依据可撤）｜ 名单和起点一样吗=%s。\n' "$rev5" \
  "$([ "$(snap d5)" = "$s5" ] && echo 一样 || echo 不一样★)"
printf '     判据：这一格必须响 —— 它就是「没落计划」的代价，不是脚本写错了。\n'
printf '   补救写法：把②和④并成一趟，边生成映射边执行（纸和动作永远同步）：\n'
: > p6.tsv
fix=0
for f in d5/记录*.txt; do
    n=${f##*记录}; n=${n%.txt}
    printf '%s\t%s\n' "d5/凭据${n}.txt" "$f" >> p6.tsv
    mv -- "$f" "d5/凭据${n}.txt"
    fix=$((fix + 1))
done
printf '   补救这一趟：落下 %s 行映射 ｜ 执行了 %s 次 mv ｜ 名单回到起点=%s\n' \
  "$(wc -l < p6.tsv)" "$fix" "$([ "$(snap d5)" = "$s5" ] && echo 是 || echo 否★)"

cd .. || exit 1
E9=$(find rc9 -mindepth 1 | wc -l)
printf '格5 收尾：rc9 里的条目数=%s ｜ 删除后还在吗=%s\n' \
  "$(find rc9 -mindepth 1 | wc -l)" "$(rm -rf rc9; [ -d rc9 ] && echo 还在★ || echo 不在了)"
printf '   → 断言：条目数=%s（期望 18＝4 个目录 d1/out/lt/d5 + 14 个文件：7 份计划与校验输出 + 6 份 txt + 1 份被轮走的日志）\n' \
  "$([ "$E9" = 18 ] && echo 对上了 || echo 对不上★)"
printf '     删除后还在吗=%s（期望 不在了）—— 五步跑完，能留下的只有这一句「删净了」。\n' \
  "$([ -d rc9 ] && echo 还在★ || echo 不在了)"
RX
runc 'bash recipe.sh'
note '   ★ 第 3 格是这一节最值钱的一条：轮转天然是「多对一」（每一轮都往同一个 .1 上撞），'
note '     所以它不需要额外设计就正好落在守卫的适用面上 —— 开守卫挡下第二轮，关守卫就把上一批日志整批覆盖掉，'
note '     而 mv 全程 rc=0、一声不吭。'
note '   ★ 第 4 格那个「撤到 0 行」是故意让它响的：省掉②就没有依据，响了才证明第 5 格那条撤回计数器不是恒零的死数字。'
rm -f recipe.sh

# ============================================================================
banner "第 10 节 同一批配方交给 /bin/sh（dash）跑：哪几条成立、哪几条直接没了"
# ============================================================================
note '# 第 9 节那个 run_plan 只用 while / read / mv / [ -e ] / $(( )) —— 它是照着「能整段搬进 /bin/sh」写的。'
note '# 但本章自己埋了两处搬不过去的东西，都在第 8 节：done < <(tac plan.tsv) 和 IFS=$'"'"'\t'"'"'。'
note '# 前一处 dash 当场报语法错（会响）；后一处 dash 一声不吭地把 $ \\ t 三个字符当字段分隔符（不会响）。'
note '# 判据一律是 rc、计数、掩码后的形状。/bin/sh 的真身第 1 节已经量出是 dash，这一节只管运行期差异。'
cat > port.sh <<'PT'
rm -rf p10 && mkdir -p p10 && cd p10 || exit 1
printf '旧1.txt\t新1.txt\n旧2.txt\t新2.txt\n旧3.txt\t新3.txt\n' > plan.tsv
snap() { printf '%s\n' *.txt | LC_ALL=C sort | tr '\n' '|'; }
fwd() { rm -f *.txt; for n in 1 2 3; do printf 'x%s\n' "$n" > "新${n}.txt"; done; }
fwd
printf '格1 现场：计划 3 行（旧N.txt → 新N.txt）｜ 当前名单=[%s]（已经改完，正等着撤回来）\n' "$(snap)"
printf '   → 断言：txt 份数=%s（期望 3）｜ 名单=[%s]（期望 全是「新」名）—— 两个 shell 接下来就在这一份现场上各跑一遍撤回。\n' \
  "$(ls *.txt 2>/dev/null | wc -l)" "$(snap)"

# 同一个脚本交两个 shell 各跑一遍，把 rc / 输出 / 跑完的名单都存下来，断言才有的比。
run2() {
    local script=$1 tag=$2 i=0 sh out rc
    for sh in bash /bin/sh; do
        i=$((i + 1))
        fwd
        out=$($sh "$script" 2>&1); rc=$?
        printf '   %-8s rc=%s ｜ 输出=[%s] ｜ 跑完名单=[%s]\n' "$sh" "$rc" "$out" "$(snap)"
        if [ "$i" = 1 ]; then
            eval "${tag}_rc=\$rc"; eval "${tag}_out=\$out"; eval "${tag}_names=\"\$(snap)\""
        else
            eval "${tag}_rc2=\$rc"; eval "${tag}_out2=\$out"; eval "${tag}_names2=\"\$(snap)\""
        fi
    done
}

cat > rev-bash.sh <<'RB'
back=0
while IFS=$'\t' read -r src dst; do
    [ -f "$dst" ] || continue
    mv -- "$dst" "$src"
    back=$((back + 1))
done < <(tac plan.tsv)
printf '放回 %s 条\n' "$back"
RB
printf '格2 第 8 节那种写法（进程替换喂 tac）交两个 shell 各跑一遍：\n'
run2 rev-bash.sh G2
printf '   → 断言：bash 那趟 rc=%s、名单=[%s]（撤回来了）；dash 那趟 rc=%s、名单=[%s]（一个都没撤）。\n' \
  "$G2_rc" "$G2_names" "$G2_rc2" "$G2_names2"
printf '     这一处会响：%s —— 语法错把整趟停在 rc=2，没人会误以为它跑成功了。\n' \
  "$(printf '%s\n' "$G2_out2" | head -1 | sed -E 's#^(dash|/bin/sh): [0-9]+: ##')"

cat > rev-sh.sh <<'RS'
back=0
tac plan.tsv > rev.tsv
while IFS="$(printf '\t')" read -r src dst; do
    [ -f "$dst" ] || continue
    mv -- "$dst" "$src"
    back=$((back + 1))
done < rev.tsv
rm -f rev.tsv
printf '放回 %s 条\n' "$back"
RS
printf '格3 同一件事的可移植写法（逆序先落临时文件，分隔符换成命令替换）：\n'
run2 rev-sh.sh G3
printf '   → 判据：rc %s vs %s ｜ 输出两串相等=%s ｜ 名单两串相等=%s —— 三项全等才叫可移植。\n' \
  "$G3_rc" "$G3_rc2" \
  "$([ "$G3_out" = "$G3_out2" ] && echo 是 || echo 否★)" \
  "$([ "$G3_names" = "$G3_names2" ] && echo 是 || echo 否★)"

cat > rev-ifs.sh <<'RI'
back=0; gone=0
while IFS=$'\t' read -r src dst; do
    [ -f "$dst" ] || { gone=$((gone + 1)); continue; }
    mv -- "$dst" "$src"
    back=$((back + 1))
done < plan.tsv
printf '放回=%s 源不在=%s\n' "$back" "$gone"
RI
printf '格4 ★ 只把逆序那处换掉、分隔符仍用 ANSI-C 引法（dash 语法认它，语义不认）：\n'
run2 rev-ifs.sh G4
printf '   → 断言：bash rc=%s ｜ dash rc=%s ｜ 两边输出相等吗=%s ｜ 两边名单相等吗=%s。\n' \
  "$G4_rc" "$G4_rc2" \
  "$([ "$G4_out" = "$G4_out2" ] && echo 相等 || echo 不等★)" \
  "$([ "$G4_names" = "$G4_names2" ] && echo 相等 || echo 不等★)"
printf '     这就是「rc 相同 ≠ 行为相同」那条证据：dash 把 dollar、反斜杠、字母 t 这三个字符当成字段分隔符，'
printf '于是在名字里那个 t 上切了一刀，源名和目标名都成了碎块 —— 而它 rc=0、零报错。\n'
printf '   机制当场量出来（同一段引法，两个 shell 各得到什么字节）：\n'
printf '%s\n' 'printf "字节数=%s ｜ 字节=[%s]\n" "$(printf "%s" $'"'"'\t'"'"' | wc -c)" "$(printf "%s" $'"'"'\t'"'"' | od -An -tx1 | xargs)"' > q.sh
for sh in bash /bin/sh; do
    printf '   %-8s %s\n' "$sh" "$($sh q.sh 2>&1)"
done
printf '     09 是一个制表符；24 5c 74 是那三个字符。字节数两边都数得出来，rc 却都是 0。\n'

cat > tbl.txt <<'TB'
[[ 双中括号 ]]|[[ 1 -eq 1 ]]
数组|a=(1 2 3); printf "%s" "${a[1]}"
进程替换|cat < <(printf x)
Here 字符串|cat <<< x
大写展开|s=abc; printf "%s" "${s^^}"
位置子串|s=abcdef; printf "%s" "${s:2:2}"
declare|declare -i n=3
nameref|f() { local -n r=x; }; x=1; f
read -d|read -r -d "" x
select|select x in a; do break; done
set -o pipefail|set -o pipefail
case 对照|case 1 in 1) : ;; esac
算术展开 对照|printf "%s" "$((2 * 3))"
字符串长度 对照|s=abc; printf "%s" "${#s}"
函数加 local 对照|f() { local n=1; printf "%s" "$n"; }; f
trap EXIT 对照|trap "rm -f 没有这个文件" EXIT
TB
rows=$(wc -l < tbl.txt)
same=0; nd2=0; nd127=0; ndother=0
printf '格5 支持面真值表（每行各交两个 shell 跑一次，只取 rc；报错只贴 dash 侧首行）：\n'
while IFS='|' read -r label snip; do
    rb=$(bash -c "$snip" >/dev/null 2>&1 </dev/null; echo $?)
    rs=$(/bin/sh -c "$snip" >/dev/null 2>&1 </dev/null; echo $?)
    se=$(/bin/sh -c "$snip" 2>&1 >/dev/null </dev/null | head -1 | sed -E 's#^(dash|/bin/sh): [0-9]+: ##')
    if [ "$rb" = "$rs" ]; then same=$((same + 1))
    elif [ "$rs" = 2 ]; then nd2=$((nd2 + 1))
    elif [ "$rs" = 127 ]; then nd127=$((nd127 + 1))
    else ndother=$((ndother + 1)); fi
    printf '   %-18s bash=%s ｜ sh=%s ｜ %s\n' "$label" "$rb" "$rs" "${se:-（dash 侧没报错）}"
done < tbl.txt
printf '   → 断言：表共 %s 行 ｜ rc 相同 %s 行 ｜ 不同 %s 行 =（dash 报语法错 rc=2）%s 行 +（dash 找不到命令 rc=127）%s 行 +（其它）%s 行\n' \
  "$rows" "$same" "$(( nd2 + nd127 + ndother ))" "$nd2" "$nd127" "$ndother"
printf '     rc 相同的那 %s 行里，对照那五行（case、算术展开、${#s}、函数加 local、trap EXIT）全是 0 —— 这就是配方能搬的根据。\n' "$same"

printf '%s\n' 'printf "展开=[%s]" a{1..3}' > brace.sh
printf '%s\n' 'printf " ｜ BASHPID 空吗=%s ｜ SECONDS 空吗=%s" "$([ -z "${BASHPID:-}" ] && echo 空 || echo 非空)" "$([ -z "${SECONDS:-}" ] && echo 空 || echo 非空)"' > var.sh
printf '格6 两边 rc 都是 0、输出却不一样的两处（只有比输出才会露馅）：\n'
bo=$(bash brace.sh 2>&1; bash var.sh 2>&1); rb6=$?
so6=$(/bin/sh brace.sh 2>&1; /bin/sh var.sh 2>&1); rs6=$?
printf '   %-8s %s\n' "bash" "$bo"
printf '   %-8s %s\n' "/bin/sh" "$so6"
bcnt=$(printf '%s' "$bo" | grep -o '展开=' | wc -l)
scnt=$(printf '%s' "$so6" | grep -o '展开=' | wc -l)
printf '   → 断言：两边 rc=%s 与 %s（都是 0）｜「展开=」出现次数 bash=%s dash=%s ｜ BASHPID 与 SECONDS 在 dash 里都是空串。\n' \
  "$rb6" "$rs6" "$bcnt" "$scnt"
printf '     判据：花括号扩展少了两段而 rc 没变 —— 这种差异只能靠比输出来发现。\n'

printf '%s\n' 'n=0
printf "a\nb\nc\n" | while IFS= read -r x; do n=$((n + 1)); done
printf "管道里数到 %s 个 " "$n"
printf "a\nb\nc\n" > t3.txt
m=0
while IFS= read -r x; do m=$((m + 1)); done < t3.txt
printf "重定向进来到 %s 个\n" "$m"' > cnt.sh
printf '格7 计数丢在管道里：这不是 dash 的锅，bash 也一样\n'
cb=$(bash cnt.sh 2>&1); cs=$(/bin/sh cnt.sh 2>&1)
printf '   %-8s %s\n' "bash" "$cb"
printf '   %-8s %s\n' "/bin/sh" "$cs"
printf '   → 断言：两串相等吗=%s（管道最后一段跑在子 shell 里，加的次数出不了那个子 shell；换成 < 文件两边都数到 3）。\n' \
  "$([ "$cb" = "$cs" ] && echo 相等 || echo 不等★)"
printf '     第 9 节的 run_plan 从一开始就用 done < 计划文件，正是为了这个。\n'

pf1=$(bash -c 'false | true'; echo $?)
pf2=$(bash -c 'set -o pipefail; false | true'; echo $?)
pf3=$(/bin/sh -c 'false | true'; echo $?)
pf4=$(/bin/sh -c 'set -o pipefail; false | true'; echo $?)
pferr=$(/bin/sh -c 'set -o pipefail' 2>/dev/null; echo $?)
printf '格8 pipefail：bash 默认=%s 开了=%s ｜ dash 默认=%s 开了=%s ｜ dash 认这个旗号吗=%s\n' \
  "$pf1" "$pf2" "$pf3" "$pf4" "$([ "$pferr" = 0 ] && echo 认 || echo 不认)"
printf '   → 断言：dash 0.5.12 认 set -o pipefail 且真生效（dash 上 false|true 默认 rc=%s，开了之后 rc=%s）。\n' "$pf3" "$pf4"
printf '     这条和「dash 没有 pipefail」的常见说法相反，是当场量出来的；表里那行 set -o pipefail 的 rc=%s 是它的语法面。\n' "$pferr"

cd .. || exit 1
E10=$(find p10 -mindepth 1 | wc -l)
printf '格9 收尾：p10 里的条目数=%s ｜ 删除后还在吗=%s\n' \
  "$(find p10 -mindepth 1 | wc -l)" "$(rm -rf p10; [ -d p10 ] && echo 还在★ || echo 不在了)"
printf '   → 断言：条目数=%s（期望 13＝4 份 txt + 7 支内嵌小脚本 + 计划 plan.tsv + 真值表 tbl.txt）\n' \
  "$([ "$E10" = 13 ] && echo 对上了 || echo 对不上★)"
printf '     判据：对不上就是哪一格落了东西没扫掉；删除后还在吗那一栏见上一行。\n'
PT
runc 'bash port.sh'
note '   ★ 这一节的收获只有一句：能搬的部分比想象的多（case、算术展开、函数、trap 全过），'
note '     不能搬的部分一半会响（语法错 rc=2、命令不存在 rc=127），另一半不会响 ——'
note '     ANSI-C 那种引法在 dash 里得到的是 dollar、反斜杠、字母 t 三个字符，rc 却是 0。'
note '     所以把脚本交给 /bin/sh 之前要比的是计数与名单，不是 rc。'
rm -f port.sh

# ============================================================================
banner "第 11 节 危险动作对照：定时任务、服务管理器、系统配置——为什么本章一次都不调用"
# ============================================================================
note '# 这四条主线天生挨着真会改东西的动作（装定时任务、动服务、换系统配置、抢别人的锁）。'
note '# 所以本节先说清「不做什么、为什么不做」，再给边界读数：工具只点名（command -v）、'
note '# 目录只看元数据（stat / ls，不读内容）、锁在沙箱里自己跟自己抢。全场零写入沙箱外。'
cat > danger.sh <<'DG'
rm -rf dg11 && mkdir -p dg11 || exit 1
cd dg11 || exit 1
printf '格1 只点名、不调用（command -v 的口径；最后那件在 /usr/sbin，普通 PATH 未必收它）：\n'
for t in crontab systemctl logrotate flock; do
    printf '   %-10s → %s\n' "$t" "$(command -v "$t" 2>/dev/null || echo 不在 PATH)"
done
printf '   → 断言：前两件本章一次都没执行 —— 第 13 节按十类模式扫全章代码，crontab 与 systemctl 的执行行各命中 0；'
printf '     后两件本章真用，次数就在本节第 3 格（logrotate 的执行行逐条数）与第 4 格（flock 当场互抢）。前两件一执行就写沙箱外面，那已经不算「学脚本」。\n'

printf '格2 它们要写的东西在沙箱外面的哪儿（只看元数据，一次都没写）：\n'
for p in /var/spool/cron /var/spool/cron/crontabs /etc/logrotate.conf /etc/logrotate.d /run/systemd/system; do
    if [ -e "$p" ]; then
        printf '   %-26s 在 ｜ %s ｜ 权限=%s 属主=%s 当前用户可写=%s\n' "$p" \
          "$([ -d "$p" ] && echo 目录 || echo 文件)" "$(stat -c %a "$p" 2>/dev/null)" \
          "$(stat -c %U "$p" 2>/dev/null)" "$([ -w "$p" ] && echo 是★ || echo 否)"
    else
        printf '   %-26s 不在\n' "$p"
    fi
done
printf '   只数条目不碰内容：/etc/logrotate.d 里的配置=%s 个 ｜ 这台机器的服务管理器在跑吗=%s\n' \
  "$(ls -A /etc/logrotate.d 2>/dev/null | wc -l)" "$([ -d /run/systemd/system ] && echo 在 || echo 不在)"
printf '   → 断言：装定时任务要落进 crontabs 那张表（普通用户可写=%s），动服务要过 PID 1 的管理器（在跑=%s）；\n' \
  "$([ -w /var/spool/cron/crontabs ] && echo 是 || echo 否)" "$([ -d /run/systemd/system ] && echo 是 || echo 否)"
printf '     两者要改的都是「整个系统」那一层的一个目录项，超出本章「一个沙箱目录」的边界，所以不碰。\n'

printf '格3 本章真用的那条路：把 logrotate.sh 里执行 logrotate 的行原样抠出来数一遍\n'
LRsrc=../logrotate.sh
tot=$(grep -v '^[[:space:]]*#' "$LRsrc" | grep -c '"\$LR"')
withs=$(grep -v '^[[:space:]]*#' "$LRsrc" | grep '"\$LR"' | grep -c -- '-s ')
withsbx=$(grep -v '^[[:space:]]*#' "$LRsrc" | grep '"\$LR"' | grep -c 'SBX/')
etc_run=$(grep -v '^[[:space:]]*#' "$LRsrc" | grep -c '/etc/logrotate')
etc_all=$(grep -c '/etc/logrotate' "$LRsrc")
printf '   执行行=%s ｜ 其中带 -s 状态文件的=%s ｜ 其中配置走沙箱路径的=%s\n' "$tot" "$withs" "$withsbx"
printf '   提到系统配置目录 /etc/logrotate* 的行：执行行=%s（期望 0）｜ 含注释的全文件=%s（注释里那句是「绝不用它」）\n' \
  "$etc_run" "$etc_all"
printf '   第一条执行行长这样（缩进原样贴出，源码里的 $SBX 是个变量名，不是路径）：\n'
grep -v '^[[:space:]]*#' "$LRsrc" | grep '"\$LR"' | head -1 | sed 's/^/      /'
printf '   → 断言：执行行 %s = 带 -s 的 %s = 配置在沙箱的 %s，且碰系统目录的执行行=%s\n' \
  "$tot" "$withs" "$withsbx" "$etc_run"
printf '     判据：三个等号成立且最后一项为 0，才算「真工具跑起来了、但配置与状态都在沙箱里」。\n'

printf '格4 不用定时任务也能演的那件事：同一把锁只许一个持有者（flock，两个 fd 互抢）\n'
exec 9>lock
exec 8<lock
flock -x 9; r1=$?
flock -n 8; r2=$?
flock -u 9; r3=$?
flock -n 8; r4=$?
printf '   拿锁=%s ｜ 第二个 fd 抢同一把=%s（期望 1=被挡住）｜ 释放=%s ｜ 释放后再抢=%s（期望 0）\n' "$r1" "$r2" "$r3" "$r4"
printf '   → 断言：%s —— 「定时任务防重叠」这件事的全部根据就在这四行里；\n' \
  "$([ "$r2" = 1 ] && [ "$r4" = 0 ] && echo 成立 || echo 不成立★)"
printf '     它不需要 crontab，也不需要服务，所以本章能在沙箱里把它测成可复现的四条 rc。\n'

E5=$(find . -mindepth 1 | wc -l)
printf '格5 收尾：dg11 里的条目数=%s ｜ 删除后还在吗=%s\n' \
  "$E5" "$(cd ..; rm -rf dg11; [ -d dg11 ] && echo 还在★ || echo 不在了)"
printf '   → 断言：条目数=%s（期望 1：只有那把 lock）=%s —— 本节对外面那些目录只读元数据，沙箱里多出来的就是没扫掉的现场。\n' \
  "$E5" "$([ "$E5" = 1 ] && echo 对上了 || echo 对不上★)"
DG
runc 'bash danger.sh'
note '   ★ 第 2 格那行「当前用户可写=否」是本节的重点：不是「我不敢试」，是这台机器上普通用户本来就'
note '     写不进那张表；真要写进去用的是包的安装动作（dpkg/postinst 那类），它会替你把整张表换掉 ——'
note '     那已经不是「学脚本」，是在动你的环境。本章要的只是「轮转/定时这件事的形状」，'
note '     那个形状第 5 节用沙箱配置 + 第 11 节用 flock 已经演全了。'

# ============================================================================
banner "第 12 节 对账：本章各几格（屏幕口径 × 源码口径 × 编号连续）"
# ============================================================================
note '# 这一节不取证，只算账。README 里要印「本章共几格」，那个数必须能当场复算，而且要拿三把尺子量：'
note '#   屏幕口径 = 本次运行真打出来的标题行数（原料是 fold() 逐趟累加的 all.out，不是源码）；'
note '#   源码口径 = 脚本里那些「打印标题」的语句有几条（写了没跑到、或跑漏一节，这一栏就会跟上一栏对不上）；'
note '#   编号连续 = 标题自带的 1..N 有没有断号或重号（前两把尺都数不出这个）。'
note '# 三把尺各自都可能骗人，所以第 3 格先当着你的面把它们各弄响一次，再报「平」。'
note '# 本节自己不在账上：tally.sh 跑起来时，all.out 里还没有它的标题行 —— 数自己这件事天生数不到。'
note '#   所以这里的合计只覆盖第 3~11 节九个单元；含本节的全章合计由第 13 节按同一把尺复算。'
cat > tally.sh <<'TY'
CAP=${1:?要先有捕获文件}
SRC=${2:?要有 demo 源码}
rm -rf t12 && mkdir -p t12 || exit 1

# 取数一律写成「吃参数」的函数。上一版这里靠改 sec / style 两个全局变量喂 awk，
# 同一个数在三处写得各不相同，读的人无法确认它们数的是不是同一个东西 —— 现在只剩一种写法。
slice()    { awk -v w="$2" 'index($0, "##SEC## " w " ") == 1 {k=1; next} /^##SEC## /{k=0} k{print}' "$1"; }
titles()   { if [ "$2" = eq ]; then printf '%s\n' "$1" | grep -cE '^=== '; else printf '%s\n' "$1" | grep -cE '^格[0-9]'; fi; }
nums()     { if [ "$2" = eq ]; then printf '%s\n' "$1" | grep -oE '^=== [0-9]+'
             else printf '%s\n' "$1" | grep -oE '^格[0-9]+'; fi | grep -oE '[0-9]+'; }
verdicts() { printf '%s\n' "$1" | grep -cE '^[[:space:]]*→'; }
# 源码口径（四支外部取证脚本）：只数「会打出标题的那一条语句」。它们有三种写法：
# echo "=== N. / printf（或 printf '\n'）"=== N. / cell 'N.，所以规则是行首为 echo|printf|cell
# 且带「=== 数字」，或者直接就是 cell 调用。函数定义行 cell() { 后面紧跟左括号，不进这条规则。
src_agent() {
    awk '($0 ~ /^[ \t]*(cell|echo|printf)[ \t]/) && ($0 ~ /=== [0-9]/ || $0 ~ /^[ \t]*cell[ \t]/) { c++ }
         END { print c+0 }' "$1"
}
# 源码口径（demo.sh 内嵌的那五支）：标题语句就在 cat > X.sh << 与 runc 'bash X.sh' 两行之间，数 格N 的 printf。
src_inner() {
    awk -v u="$1" 'index($0, "cat > " u " <<"){f=1; next}
                  f && /^runc / && index($0, u){f=0}
                  f && /^[ \t]*printf .格[0-9]/{c++}
                  END { print c+0 }' "$SRC"
}
# 四支外部脚本的标题在屏上是 === N.，demo 内嵌那五支是 格N —— 两把尺的匹配串不同，写进表里。
cat > units.tsv <<'UT'
第 3 节|rename.sh|eq|agent
第 4 节|dircheck.sh|eq|agent
第 5 节|logrotate.sh|eq|agent
第 6 节|interact.sh|eq|agent
第 7 节|openfd.sh|ge|inner
第 8 节|rerun.sh|ge|inner
第 9 节|recipe.sh|ge|inner
第 10 节|port.sh|ge|inner
第 11 节|danger.sh|ge|inner
UT

printf '格1 九个单元三口径对照（屏上标题 / 最大编号 / 编号连续 / 源码标题语句 / 两口径是否一致）：\n'
tA=0; tB=0; tV=0; bad=0
while IFS='|' read -r sec unit style whence; do
    body=$(slice "$CAP" "$sec")
    A=$(titles "$body" "$style")
    mx=$(nums "$body" "$style" | sort -n | tail -1); [ -z "$mx" ] && mx=0
    cont=$(nums "$body" "$style" | sort -n | awk 'NR != $1 {c++} END {printf (c+0 ? "断号或重号★" : "连续")}')
    if [ "$whence" = inner ]; then B=$(src_inner "$unit"); else B=$(src_agent "$unit"); fi
    V=$(verdicts "$body")
    ok=一致
    { [ "$A" = "$B" ] && [ "$A" = "$mx" ] && [ "$cont" = 连续 ] && [ "$V" -ge "$A" ]; } || { ok='不平★'; bad=$((bad + 1)); }
    tA=$((tA + A)); tB=$((tB + B)); tV=$((tV + V))
    printf '   %-12s 屏上=%-3s 最大编号=%-3s 编号=%-12s 源码=%-3s 判定行=%-3s ｜ %s\n' \
      "$unit" "$A" "$mx" "$cont" "$B" "$V" "$ok"
done < units.tsv
printf '   → 断言：屏上标题合计=%s ｜ 源码标题语句合计=%s ｜ 判定行合计=%s\n' "$tA" "$tB" "$tV"
printf '     四个口径全平的单元=%s / 9，不平=%s（不平必须逐行查，不存在「差一点点可以接受」）\n' \
  "$(( 9 - bad ))" "$bad"

printf '格2 节这一层：本次捕获里有几个节标题、有没有哪一节印了标题却一个字没输出\n'
# 一趟 awk 出三个数：节标题数、本节之前的空切片数、最后一节此刻已落盘的正文行数。
# 上一版这里 awk 自己 printf、shell 再 grep 一次同一个数，两处口径还不一样，现在只留一处。
shape=$(awk 'BEGIN{n=0; e=0; c=0}
            /^##SEC## /{ if (n > 0 && c == 0) e++; n++; c=0; next }
            { c++ }
            END { print n " " e " " c }' "$CAP")
set -- $shape
printf '   节标题=%s ｜ 本节之前各节里的空切片=%s（期望 0）\n' "$1" "$2"
printf '   最后一节（就是本节自己）此刻的正文行数=%s（期望 0：它的输出还压在 runc 的管道里，没落盘）\n' "$3"
printf '   → 断言：捕获里已有 %s 个节标题，与「第 1~12 节各一个」%s ｜ 此前的空切片=%s，%s\n' \
  "$1" "$([ "$1" = 12 ] && echo 对上了 || echo 对不上★)" "$2" "$([ "$2" = 0 ] && echo 一个都没有 || echo 有★)"
printf '     判据：少一个节标题就是有一节的 banner 与正文断开了；空切片>0 就是有一节正文压根没跑。\n'
printf '     「本节自己切不出东西」是同一件事的现场版：数不到自己是这一节的固有边界，写明白比掩盖强。\n'

printf '格3 先把两把尺各弄响一次（真文件与只读副本并排数，副本不动原文件）：\n'
S0=$(slice "$CAP" '第 3 节' | grep -cE '^=== ')
last=$(grep -nE '^[[:space:]]*echo "=== ' rename.sh | tail -1 | cut -d: -f1)
sed "${last}d" rename.sh > t12/src-minus1.sh
A0=$(src_agent rename.sh); B0=$(src_agent t12/src-minus1.sh)
printf '   响法①只动源码：rename.sh 标题语句 真文件=%s 条 → 删掉最后一行的副本=%s 条（差 %s）\n' \
  "$A0" "$B0" "$((A0 - B0))"
printf '     屏幕口径不跟着变（第 3 节屏上仍是 %s 条）→ 屏上 %s 对副本 %s = %s\n' \
  "$S0" "$S0" "$B0" "$([ "$S0" = "$B0" ] && echo 一致（那就查不出漏跑）|| echo 不一致★（这才叫响）)"
slice "$CAP" '第 3 节' > t12/sec3.txt
sed '0,/^=== /{/^=== /d}' t12/sec3.txt > t12/sec3-minus1.txt
C0=$(grep -cE '^=== ' t12/sec3.txt); C1=$(grep -cE '^=== ' t12/sec3-minus1.txt)
printf '   响法②只动捕获：第 3 节切片 真捕获=%s 条 → 删掉第一条标题行的副本=%s 条（差 %s）\n' \
  "$C0" "$C1" "$((C0 - C1))"
printf '     源码口径不跟着变（rename.sh 仍是 %s 条）→ 副本 %s 与源码 %s 之比 = %s\n' \
  "$A0" "$C1" "$A0" "$([ "$C1" = "$A0" ] && echo 一致（查不出没打出来的那一格）|| echo 不一致★（响）)"
printf '   → 断言：两次「响」分别只落在一把尺上（源码差=%s、屏幕差=%s），所以两把尺都在真数东西；\n' \
  "$((A0 - B0))" "$((C0 - C1))"
printf '     它们报的「平」不是「恒真」—— 第 1 格的平是这两把互相看不见的尺撞出来的。\n'

printf '格4 收口判定这一层：每格都得有一句能把话说死的话\n'
printf '   九个单元的「→ 开头行」合计=%s 条，标题合计=%s 条 ｜ 判定行 >= 标题行吗=%s\n' \
  "$tV" "$tA" "$([ "$tV" -ge "$tA" ] && echo 是 || echo 否★)"
printf '   不是一格一句：有的格两句 →（前一段收尾、后一段给判据），所以这条只按 >= 判，不按 = 判。\n'
printf '   → 断言：判定行=%s >= 标题行=%s，即每一格都至少带一句能验伪的话；格1 那栏「判定行」逐单元给数。\n' \
  "$tV" "$tA"

printf '格5 两本内部账的落盘形状（第 13 节的原料）：\n'
# 只数「跑到本节为止已经真的执行过的那些 runc」：从文件头数到本节自己那一行为止就退出。
# 数全文会天生对不上 —— 第 13 节的 runc 还一行都没跑，这是上一版埋着的坑。
RC=$(awk '/^runc .*tally\.sh/{exit} /^runc / || /^runc$/{c++} END{print c+0}' "$SRC")
FH=$(grep -c '' ../foldhits)
printf '   foldhits 行数=%s ｜ demo.sh 里 runc 语句条数=%s ｜ 一样吗=%s\n' \
  "$FH" "$RC" "$([ "$FH" = "$RC" ] && echo 一样 || echo 不一样★)"
printf '   all.out 行数=%s ｜ 本脚本造的 t12/ 还在吗=%s\n' \
  "$(wc -l < "$CAP")" "$(rm -rf t12; [ -d t12 ] && echo 还在★ || echo 删掉了)"
printf '   → 断言：foldhits 每折叠一次追加一行命中数，它必须等于真跑过的 runc 格数（%s vs %s → %s）；\n' \
  "$FH" "$RC" "$([ "$FH" = "$RC" ] && echo 对上了 || echo 对不上★)"
printf '     第 13 节就拿这本账回答「折叠到底响过没有」—— 光看屏上没有真实身份，证明不了有人在挡。\n'
TY
runc "bash tally.sh ../all.out demo.sh"
note '   ★ 这一节的收获：屏幕口径来自 fold() 逐趟累加的 all.out，也就是「这次运行真打出来的东西」；'
note '     源码口径来自脚本正文，也就是「你以为你写了的」。两者一撞，漏跑与写了没跑都会露出来 ——'
note '     第 3 格那两次「响」是给你看这两把尺确实互相看不见：删源码一行只动源码口径，删捕获一行只动屏幕口径。'
rm -f tally.sh units.tsv

# ============================================================================
banner "第 13 节 红线自证：本章「没做什么」，拿数字说"
# ============================================================================
note '# 前面十二节一直在替别人取证，这一节反过来查本章自己：有没有越过红线？越到哪儿了？'
note '# 四本账：① 屏幕上不该出现的真实身份（挡过没有）；② 十类危险动作（命中没有）；'
note '#   ③ 后台作业（收干净没有）；④ 跑前跑后（章目录与你的家目录动过没有）。'
note '# 规矩和前面一样：每本账都先喂一条真样本把探测器弄响，再报真实值 —— 扫不出东西的扫描器不算证据。'
# 折叠的「响法」先当场演示一遍：这一条把家目录、用户名、主机名的原文打出去，屏上看到的是占位符；
# 而 fold() 在折叠之前就把命中行数记进 foldhits —— 第 13 节第 2 格要的就是这两本账的对照。
runc 'printf "  演示一行原文：%s 与 %s@%s 与 %s/.ssh\n" "$HOME" "$USER" "$(hostname)" "$HOME"'
cat > redline.sh <<'RL'
SBX=$(cd .. && pwd)          # 本次沙箱目录（work 的上一级），只用来比对，绝不上屏
CAP=../all.out               # 屏幕账：fold() 每跑完一趟追加一行，所以本趟（redline 自己）还不在里面
HITS=../foldhits
SRC=demo.sh                  # demo.sh 的副本被 harness 复制进了 work/，就地读它，别往上级找
RLT=redline.tsv              # 数据文件同样被复制进 work/；它自己不在扫描范围里（它是规则，不是代码）
# 扫描范围：4 支取证脚本 + demo.sh 全文 —— 第 7~13 节那些内嵌脚本的正文就写在 demo.sh 里，一并扫到。
SCOPE='demo.sh rename.sh dircheck.sh logrotate.sh interact.sh'

printf '格1 全章格子合计复算（承第 12 节那三把尺，这次不切片，直接数全篇）\n'
eq=$(grep -cE '^=== ' "$CAP"); ge=$(grep -cE '^格[0-9]' "$CAP")
secs=$(grep -c '^##SEC## ' "$CAP")
# 和第 12 节同一把尺：数「cat > X.sh << 到 runc 'bash X.sh' 之间」有几条标题语句，用来补上那两节切不到的自己。
srccount() {
    awk -v u="$1" 'index($0, "cat > " u " <<"){f=1; next}
        f && /^runc / && index($0, u){f=0}
        f && /^[ \t]*printf .格[0-9]/{c++} END{print c+0}' "$SRC"
}
tw=$(srccount tally.sh)
inner=$(srccount redline.sh)
printf '   节标题=%s（期望 13：本节的 banner 已经落账）｜ 屏上带编号的格：=== 型 %s + 格N 型 %s = %s\n' \
  "$secs" "$eq" "$ge" "$((eq + ge))"
printf '   源码口径补两笔：第 12 节自己=%s 条 ｜ 本节=%s 条（这两节的输出此刻还压在各自 runc 的管道里，屏上账本收不到自己）\n' "$tw" "$inner"
printf '   → 断言：屏上 %s 能不能接上第 12 节报的 77=%s（77 + 第 12 节自己 %s 格 = %s）；节标题对上了吗=%s。\n' \
  "$((eq + ge))" "$([ "$((eq + ge))" = "$((77 + tw))" ] && echo 接得上 || echo 接不上★)" "$tw" "$((77 + tw))" \
  "$([ "$secs" = 13 ] && echo 对上了 || echo 对不上★)"
printf '     全章合计 = 屏上 %s + 本节 %s = %s 格 —— README 要印的就是这个数；它是拿同一把尺从 all.out 复算出来的，不是抄来的。\n' \
  "$((eq + ge))" "$inner" "$((eq + ge + inner))"

printf '格2 身份折叠到底响过没有：屏上「没有」＋账本「挡过」，两条一起才叫证据\n'
# 一趟 awk 出三个数：已完成折叠的趟数、其中命中过的趟数、命中行数合计（foldhits 每趟一行，记的是那趟折叠前的命中行数）。
set -- $(awk '{c++; s += $1; if ($1 > 0) h++} END {print c+0, h+0, s+0}' "$HITS")
t=$1; n=$2; s=$3
last=$(tail -1 "$HITS")
raw=$(grep -c -F -e "$HOME" -e "$USER" -e "$(hostname)" "$CAP")
sbx=$(grep -c -F "$SBX" "$CAP")
printf '   foldhits 到这里=%s 趟（不含本趟，本趟还没折叠）｜ 命中过的趟数=%s ｜ 命中行数合计=%s ｜ 上一趟=刚才那条演示=%s\n' "$t" "$n" "$s" "$last"
printf '   折叠之后再扫全篇 all.out：%s 行里有真实身份 ｜ %s 行里有沙箱绝对路径（两项期望都是 0）\n' "$raw" "$sbx"
printf '   → 断言：上一趟命中>0=%s ｜ 折叠后两项都为 0=%s —— 一响一零同时成立，才说明屏上的干净是挡出来的，不是没东西可挡。\n' \
  "$([ "$last" -gt 0 ] 2>/dev/null && echo 响了 || echo 没响★)" \
  "$([ "$raw" = 0 ] && [ "$sbx" = 0 ] && echo 平了 || echo '不平★')"
printf '     光看屏上「没有身份」是看不出门道的：可能压根没写过，也可能写了被折掉 —— 只有 foldhits 能分辨这两件事。\n'

printf '格3 十类危险动作扫描：模式全在数据文件 %s 里，扫描器自己一个敏感词都不写\n' "$RLT"
# 候选行过滤：注释、demo 的解说词（note/banner）、以及「只点名不调用」的那几行（command -v / for t in）都不算执行。
cand() { grep -vE '^[[:space:]]*#|^[[:space:]]*note |^[[:space:]]*banner |command -v|for t in' "$1"; }
cut -f3 "$RLT" > fake-violation.sh
printf '   类别 ｜ 模式 ｜ 假样本（探测器该响）｜ 真实范围（该是 0）\n'
fbad=0; rbad=0; cls=0
while IFS=$(printf '\t') read -r name pat samp; do
    cls=$((cls + 1))
    fa=$(cand fake-violation.sh | grep -Ec -- "$pat")
    re=0
    for f in $SCOPE; do re=$((re + $(cand "$f" | grep -Ec -- "$pat"))); done
    [ "$fa" = 0 ] && fbad=$((fbad + 1))
    [ "$re" != 0 ] && rbad=$((rbad + 1))
    printf '   %s ｜ %s ｜ 命中=%s ｜ 命中=%s ｜ %s\n' "$name" "$pat" "$fa" "$re" \
      "$([ "$fa" != 0 ] && [ "$re" = 0 ] && echo 探测器响、范围干净 || echo 要查★)"
done < <(grep -v '^#' "$RLT")
printf '   → 断言：类数=%s ｜ 假样本没响的类=%s（期望 0：模式连自己的样本都命中不了就是摆设）｜ 真实范围命中的类=%s（期望 0）\n' \
  "$cls" "$fbad" "$rbad"
printf '     判据：两栏都得对 —— 只报「真实=0」而探测器从没响过，等于没扫；fake-violation.sh 全程没被执行过一行，它只是数据。\n'

printf '格4 后台作业的收尾账：谁起了后台，谁就得自己收干净\n'
# 只数真会执行的行：注释与解说词（note / banner / 打印用的 printf、echo）都不算代码，否则一句「记得 wait」就能把账做平。
code() { grep -vE '^[[:space:]]*#|^[[:space:]]*(note|banner|printf|echo)[[:space:]]' "$1"; }
# 起后台的行：把 &&、||、>&n 先抹掉，再看剩下的「空格 + & + 空格或行尾」；保留原始行号，好让读者自己 sed -n 去对。
bgl() {
    grep -nE '[[:space:]]&' "$1" \
      | grep -vE '^[0-9]+:[[:space:]]*(#|note |banner |printf |echo )' \
      | sed 's/&&//g; s/>&[0-9]//g; s/||//g' \
      | grep -E '[[:space:]]&([[:space:]]|$)'
}
for f in $SCOPE; do
    amp=$(bgl "$f" | wc -l)
    wh=$(bgl "$f" | cut -d: -f1 | tr '\n' ' ' | sed 's/ *$//; s/ /、/g')
    [ -n "$wh" ] || wh=没有
    wt=$(code "$f" | grep -cE '^[[:space:]]*wait([[:space:];]|$)|[;&][[:space:]]+wait([[:space:];]|$)')
    tp=$(grep -cE '^[[:space:]]*trap ' "$f")
    printf '   %-14s 起后台=%s ｜ wait 行=%s ｜ trap 行=%s ｜ 起后台的行号=%s ｜ %s\n' "$f" "$amp" "$wt" "$tp" \
      "$wh" \
      "$([ "$amp" = 0 ] && echo 不起后台、无需收尾 || { [ "$wt" -ge "$amp" ] && [ "$tp" != 0 ] && echo 每个后台都排到 wait 且有 trap || echo 有后台没人收★; })"
done
printf '   （行号取自 work/ 里的副本，与章目录那份逐字节相同；demo.sh 那一栏是汇总口径 —— 第 7~13 节的内嵌脚本正文就写在它里面。）\n'
# 探测器自己先响一次：沙箱里正在跑的进程数，按 /proc/*/cwd 落在本沙箱之下来认。
n_in_sbx() {
    local d cw c=0
    for d in /proc/[0-9]*; do
        cw=$(readlink "$d/cwd" 2>/dev/null) || continue
        case "$cw" in "$SBX"*) c=$((c + 1)) ;; esac
    done
    printf '%s' "$c"
}
B4=$(n_in_sbx)
sleep 30 & SP=$!
A4=$(n_in_sbx)
kill "$SP" 2>/dev/null; wait "$SP" 2>/dev/null
C4=$(n_in_sbx)
printf '   数进程的这一把尺先弄响：基线=%s → 自己起一个 sleep 之后=%s → kill 并 wait 之后=%s\n' "$B4" "$A4" "$C4"
printf '   → 断言：起了要能看见=%s（期望 看得见）｜ 收了要回到基线=%s（期望 回得来）；基线=%s 是 demo 主进程、fold 与本格外层那几层 bash。\n' \
  "$([ "$A4" -gt "$B4" ] && echo 看得见 || echo 看不见★)" \
  "$([ "$C4" = "$B4" ] && echo 回来了 || echo 没回来★)" "$B4"

printf '格5 跑前跑后三个「一样」：章目录、你的家目录、/tmp 里沙箱的个数\n'
now_list=$(cd "$HERE" && ls -A | sort | tr '\n' '|')
now_sum=$(cd "$HERE" && wc -c *.sh | tail -1 | awk '{print $1}')
h_sum=$(ls -A "$HOME" | LC_ALL=C sort | cksum | cut -d' ' -f1)
h_n=$(ls -A "$HOME" | wc -l)
left=$(ls -d "${TMPDIR:-/tmp}"/bash10-* 2>/dev/null | wc -l)
printf '   章目录：名单一致吗=%s ｜ .sh 字节总和 %s → %s ｜ 一致吗=%s\n' \
  "$([ "$now_list" = "$BEFORE_LIST" ] && echo 一样 || echo '不一样★')" "$BEFORE_SUM" "$now_sum" \
  "$([ "$now_sum" = "$BEFORE_SUM" ] && echo 一样 || echo '不一样★')"
printf '   家目录：条目数 %s → %s ｜ 名字集合校验和 %s → %s（名单本身一个字都没上屏）\n' \
  "$HOME_N_BEFORE" "$h_n" "${HOME_LS_BEFORE:0:10}…" "${h_sum:0:10}…"
printf '   /tmp 里 bash10- 前缀的沙箱个数=%s（期望 1：只有正在跑的这一个，收尾时 trap 会把它整目录删掉）\n' "$left"
printf '   → 断言：三组判定 %s —— 章目录没被本章写过、你的家目录没被碰过、系统里没留下第二个沙箱。\n' \
  "$([ "$now_list" = "$BEFORE_LIST" ] && [ "$now_sum" = "$BEFORE_SUM" ] && [ "$h_n" = "$HOME_N_BEFORE" ] \
     && [ "$h_sum" = "$HOME_LS_BEFORE" ] && [ "$left" = 1 ] && echo 全平 || echo '有不平★')"
printf '     这里用的四个基准值都是 demo.sh 开头 export 进来的；不 export 的话子进程里它们就是空串，整格退化成恒真。\n'
RL
runc 'bash redline.sh'
note '   ★ 这一节没有新知识，只有本章一路承诺过的四件事被落成数字：身份折掉了（且折之前确实命中过）、'
note '     十类危险动作一次都没执行（且扫描器连得上自己的样本）、后台作业每个都排到 wait、跑前跑后你的目录一个字没变。'
note '   ★ 读到这里可以合上书去做一件事：把你手上任何一段「会改东西」的脚本，照第 9 节那五步重写一遍 ——'
note '     取名单、出计划、过守卫、执行并计数、收尾自证。第五步写不出来，就说明前四步迟早要你还。'
rm -f redline.sh fake-violation.sh
