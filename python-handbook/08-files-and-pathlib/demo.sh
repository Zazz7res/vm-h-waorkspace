#!/usr/bin/env bash
# ============================================================================
# 第 08 章《文件与 pathlib：字节、字符、路径，是三件不同的事》—— 整章证据一键复现
#
#   bash demo.sh                 跑完自动删沙箱（默认）
#   bash demo.sh --keep          跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt 2>&1  重定向时自动关颜色，得到和本章 README 一样的纯文本
#
# 屏幕上出现的每一行，都是这条命令在这台机器（CPython 3.14.4 / Ubuntu / ext4）上的真实结果，
# 没有一行是手写的、没有一行是「我记得应该这样」。
#
# 安全说明：
#   * 示例源码整份复制进 mktemp 出来的沙箱再运行，所以章目录里**不会**留下
#     *-demo/、*.tmp、__pycache__/、*.pyc。第 21 节不直接报 0：每条计数都先被故意弄响一次
#     （造一个 .tmp、真 import 一个模块），再报 0 —— 恒 0 的检查比没有检查更糟。
#   * 每个脚本自己在**沙箱当前目录**里建 xxx-demo/ 真写真删文件（这是本章的主题本身），
#     跑完各自 rmtree；章目录从头到尾不参与写。
#   * 只用标准库 + 本机自带的 python3；不 pip install、不联网、不装东西、
#     不开任何监听端口、不改 ~/.bashrc / ~/.profile / ~/.config / ~/.local。
#   * 权限实验只 chmod 自己沙箱里自己建的目录（造 PermissionError 用），删除前改回来；
#     不碰 /、/etc、别人的家目录。
#   * 本章刻意 **unset PYTHONUTF8 / PYTHONCOERCECLOCALE**：第 5、6 节比的就是「默认编码从哪来」，
#     外部环境变量会让结论变样，所以先把开关拨到已知位置，再在脚本里逐个开关现场对比。
#   * 故意不加 `set -e`：本章一半是失败现场（FileExistsError、NotADirectoryError、
#     UnicodeDecodeError、json 的三样报错、悬空符号链接、EBUSY…），加了就只能看到第一段。
#   * 故意不加 `set -o pipefail`：`cmd | head -3` 这类截取会因为下游提前关管道报 141，
#     屏幕上会冒出一堆和教学无关的退出码。
#   * 屏幕上不主动打印绝对路径：需要「在某个目录里跑」时用 runc_at，只留目录 basename。
#     第 4、18 节用 tracemalloc 量内存：**峰值绝对值每跑必抖，所以那两节只印「谁比谁大」这类大小关系，
#     不印字节数** —— 印出来的数字必须两次跑逐字一样，否则 README 的逐字校验永远红。
#   * 子进程一律带 capture：本脚本自己的退出码必须一路是 0，才能当验收闸门用。
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

set -u

# 固定语言：Python 自己的报错恒为英文，但 `command not found` 这类消息是 bash 翻译的；
# 设成 C.UTF-8 后整份 transcript 才能逐字比对（你手敲时看到中文完全正常）。
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
export LANGUAGE=C
# 清掉可能干扰结论的环境变量（只影响本脚本这个进程和它的子进程，不碰你的配置文件）。
unset PYTHONPATH PYTHONSTARTUP PYTHONOPTIMIZE PYTHONINSPECT PYTHONWARNINGS
# 本章的核心变量之一就是「默认编码」，而 PEP 540/538 两个开关会直接改写它 —— 先拨回已知位置。
unset PYTHONUTF8 PYTHONCOERCECLOCALE PYTHONIOENCODING
# 本章大量打印 dict / set / glob 的结果；固定哈希种子 + 脚本内部一律 sorted()，
# 为的是同一份脚本在你机器上打出逐字一样的输出。
export PYTHONHASHSEED=0

# 只有输出到终端才上色；重定向到文件时是干净纯文本
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/py08-XXXXXX")
export SB

banner() { printf '\n%s══════ 第 %s 节 %s ═══════════════════════════════%s\n' "$B" "$1" "$2" "$R"; }
note()   { printf '%s\n' "$1"; }

# 打印命令 → 执行（stderr 合并进来）→ 打印退出码
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}

# runc_at <目录> <命令>：先 cd 进那个目录再跑，屏幕上只留目录 basename + 相对命令。
runc_at() {
    printf '\n%s$ 〔在 %s 里〕 %s%s\n' "$C" "$(basename "$1")" "$2" "$R"
    ( cd "$1" && eval "$2" ) 2>&1
    printf '[退出码 %d]\n' "$?"
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去接着敲： cd "%s/work"\n  看完自己删： rm -rf "%s"\n' \
               "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# ============================================================================
banner 1 "环境体检：这一章的每一条结论都挂在这几个数字上"
# ============================================================================
mkdir -p "$SB/work"
cp -R "$HERE"/. "$SB/work"/
cd "$SB/work" || exit 1
# ★ 把 umask 钉在 0022：本章满屏的 0644 / 0755 / 「0666 & ~umask」都挂在这个数上。
#   继承来的值各机器、各调用方式都不一样（这台机器的外层 shell 给的是 0002），不钉死的话
#   第 1 节那行 printf 会和第 13 节的对照组打架 —— 书里不能出现两种 umask。
umask 022
note '本章 19 个 .py（18 个取证脚本 + 第 20 节那个对账工具）+ demo.sh，沙箱里列一遍（章目录本身不产任何文件，第 21 节自证）：'
runc 'ls'
runc 'python3 --version && python3 -c "import sys; print(sys.version)"'
note '↑ 下面这几行是本章的地基：解释器怎么解释「一个字节流是文本」、新建文件默认是什么权限、临时文件落在哪。'
runc 'python3 -c "
import sys, locale, os, stat
print(\"sys.getdefaultencoding()      =\", sys.getdefaultencoding())
print(\"sys.getfilesystemencoding()   =\", sys.getfilesystemencoding())
print(\"locale.getpreferredencoding() =\", locale.getpreferredencoding(False))
print(\"sys.stdout.encoding           =\", sys.stdout.encoding)
print(\"locale.getlocale()            =\", locale.getlocale())
"'
runc 'um=$(umask); printf "umask=%s ｜ 新建文件默认 %04o ｜ 新建目录默认 %04o\n" "$um" "$((0666 & ~(8#$um)))" "$((0777 & ~(8#$um)))"'
note '↑ umask 这一行是第 13 节（临时文件为什么是 0600）的对照组；本章所有权限数字都在这个 umask 下读。'
runc 'python3 -c "import tempfile, os; p=tempfile.gettempdir(); print(\"tempfile.gettempdir() =\", p); print(\"是 /tmp 吗 →\", p == \"/tmp\")"'
runc 'python3 -c "import os, sys; print(\"os.linesep =\", repr(os.linesep), \"｜ 平台 =\", sys.platform)"'
runc 'python3 -c "
import sys
need = {\"pathlib\", \"csv\", \"json\", \"tempfile\", \"shutil\", \"stat\", \"os\", \"tracemalloc\", \"warnings\", \"pty\"}
print(\"本章用到的模块全在标准库里 →\", need <= sys.stdlib_module_names)
print(\"缺的：\", sorted(need - sys.stdlib_module_names) or \"无\")
"'
note '★ 本章不碰网络、不装包、不开端口；所有文件操作都在 mktemp 沙箱里，跑完各自删干净。'

# ============================================================================
banner 2 "open() 的模式字符到底改了哪三件事"
# ============================================================================
runc 'python3 a_open_modes.py'
note '↑ 最贵的一条在第 2 小段：`w` 是**打开那一刻就截断**，还没等你写，旧内容已经没了。'
note '  所以「先 open 再决定要不要写」这种写法本身就是数据事故，本章现场把它演给你看。'

# ============================================================================
banner 3 "缓冲：为什么管道里看不到进度"
# ============================================================================
runc 'python3 b_buffering.py'
note '↑ 这一节用 pty 在本机进程内造了一个假终端，同一段 print 循环分别接 tty 和接管道，'
note '  把「为什么 tail -f 看不到进度」这件事量出来了 —— 不起网络、不 spawn 交互 shell。'

# ============================================================================
banner 4 "close、一次性的文件迭代器、四种读法的内存账"
# ============================================================================
runc 'python3 c_close_and_iter.py'
note '↑ 第 3 小段是第 07 章第 18 节那条规律的展开：文件对象是**迭代器**，第二次 list(f) 是空的'
note '  而且不报错。第 4 小段用 tracemalloc 比四种读法（read / readlines / 逐行 / 分块，外加留住行的那一档）'
note '  的峰值 —— 峰值绝对值每跑必抖，所以它只印「谁比谁大」，一个字节数都不印。'

# ============================================================================
banner 5 "newline= 读写两侧各管什么"
# ============================================================================
runc 'python3 d_newline.py'
note '↑ 先写三种真实字节形状（\\n、\\r\\n、\\r）再读回来，才能看清「文本模式默认会把 \\r\\n 变成 \\n」。'
note '  写侧那条本机看不出差别（os.linesep 就是 \\n），脚本里如实标了「别当跨平台已验证结论」。'

# ============================================================================
banner 6 "默认编码是从哪来的，BOM 是怎么回事"
# ============================================================================
runc 'python3 e_encoding_default.py'
note '↑ 这一节是本命令头上那三行 unset 的存在理由：PYTHONUTF8 / PYTHONIOENCODING 会改写默认编码，'
note '  所以先把开关拨到已知位置，再在脚本里逐个开、逐个关地对比。'
note '  记住那条判据：latin-1 **永远不会报错**，它只是把乱码给你 —— 「没报错」不等于「读对了」。'

# ============================================================================
banner 7 "errors= 的五种结局和它的代价"
# ============================================================================
runc 'python3 f_errors_param.py'
note '↑ errors="ignore" 不是「宽容」，是**把字节悄悄扔掉**：脚本里数了一遍到底丢了几克数据，'
note '  并和 surrogateescape 的「先留着、写回去还能原样」对照。'

# ============================================================================
banner 8 "Path 是对象，不是字符串"
# ============================================================================
runc 'python3 g_pathlib_basics.py'
note '↑ 第 2 小段值得多看一眼：对一个**根本不存在**的路径，parts / name / suffix / parent 全部算得出来，'
note '  一个异常都不抛。这条界线（纯算 vs 问内核）是 pathlib 所有困惑的总根源。'

# ============================================================================
banner 9 "os.path 的字符串函数逐个对照"
# ============================================================================
runc 'python3 h_pathlib_vs_ospath.py'
note '↑ 同一个输入两栏并排打印，标清楚哪些是纯字符串活、哪些真的要问内核。'
note '  顺带钉一条必须现场看到的规矩：os.path.join("/a", "/b") 会把 "/a" 丢掉。'

# ============================================================================
banner 10 "幂等建目录、缺失不删炸，以及权限位怎么挡你"
# ============================================================================
runc 'python3 i_mkdir_unlink.py'
note '↑ 第 5 小段是 Unix 的一条反直觉规矩：**删文件看的是父目录的写权限，不是文件自己的**。'
note '  脚本把自己的目录 chmod 0o500 造出真 PermissionError，删之前再改回来 —— 全在沙箱内。'

# ============================================================================
banner 11 "遍历、匹配与 stat 的字段"
# ============================================================================
runc 'python3 j_glob_stat.py'
note '↑ glob / rglob 不保证顺序，所以本章所有结果都 sorted() 过 —— 这是能逐字比对的前提。'
note '  stat 那一屏只打字段名和非易变值（权限打八进制、inode 只用来判断「是不是同一个文件」）。'

# ============================================================================
banner 12 "read_text / write_text 的最短路径和它的三个坑"
# ============================================================================
runc 'python3 k_read_write_text.py'
note '↑ 第 2 小段：write_text 是**截断写**；第 5 小段用 warnings 把「open(p).read() 没人替你 close」'
note '  变成屏幕上看得见的一条 ResourceWarning —— 平时它被默认过滤器吃掉了。'

# ============================================================================
banner 13 "临时文件的四种粒度与权限位"
# ============================================================================
runc 'python3 l_tempfile_perms.py'
note '↑ 这一节把第 1 节那行 umask 兑现了：mkstemp 建出来是 0600，普通 open 建出来是 0644 ——'
note '  这不是风格差异，是「别的用户在你 /tmp 里抢先打开」的安全差异。'
note '  Windows 语义那一段本机验不了，脚本里明写了「未验证，别当事实引用」。'

# ============================================================================
banner 14 "符号链接、resolve() 和「我是谁」"
# ============================================================================
runc 'python3 m_symlink_resolve.py'
note '↑ 一句话判据：resolve() 是**问内核**，normpath 是**算字符串**；absolute() 只拼 cwd、不折叠 .. 。'
note '  第 4 小段那个「相对符号链接相对的是链接自己所在的目录」是运维现场最常见的坑，换 cwd 真跑了一遍。'

# ============================================================================
banner 15 "csv：它到底替你处理了什么"
# ============================================================================
runc 'python3 n_csv.py'
note '↑ 第 3 小段是本章抄错率最高的一行：写 CSV 必须 newline=""，否则 csv 自己写的 \\r\\n'
note '  会被文本层再翻译一次。脚本用 read_bytes 把真实字节打出来，看一眼就记住了。'

# ============================================================================
banner 16 "json：类型表、报错原文，以及它不保存类型"
# ============================================================================
runc 'python3 o_json.py'
note '↑ 两条要记住的：json 是**值**的格式不是**类型**的格式（tuple 回不来、自定义类要 default= 钩子）；'
note '  JSONDecodeError 是 ValueError 的儿子，而且带 lineno / colno / pos 三个结构化字段（第 07 章那棵树）。'

# ============================================================================
banner 17 "怎么把「写完」变成磁盘上的事实"
# ============================================================================
runc 'python3 p_atomic_write.py'
note '↑ 第 07 章第 18 节立的那条边界在这里做实：同目录临时文件 + os.replace 是唯一安全写法，'
note '  fsync 只到「交给内核」为止 —— 掉电行为本机验不了，脚本里老实标了未验证。'

# ============================================================================
banner 18 "大文件、分块、seek/tell 的真实形状"
# ============================================================================
runc 'python3 q_bigfile_seek.py'
note '↑ 文本模式的 seek 和二进制不一样：它接受的是 tell() 返回的**不透明 cookie**，'
note '  随便塞一个字节数进去会撞见原文报错 —— 这一条大多数人第一次撞上都以为是 bug。'

# ============================================================================
banner 19 "capstone：把本章零件装成一个真工具（安全读配置、规范化产出）"
# ============================================================================
runc 'python3 r_cli_capstone.py'
note '↑ 这个工具的每一行都能回指本章某一节，对照表在它自己最后一段里打出来了。'
note '  注意第 4 段：产出走 tmp + replace，而且这条「有没有留 .tmp」的对账当场被弄响过一次。'

# ============================================================================
banner 20 "对账：这 19 个脚本是不是真按规矩写的，以及一条守卫怎么证明它会响"
# ============================================================================
runc 'python3 s_checklist.py'
note '↑ 第 2 段那张表就是本章 README 里「19 个脚本 / 多少个小节」这两个数字的出处 —— 数字是脚本数出来的，不是人写的。'
note '  第 3 段是这一节真正的看点：它现场造三个**不合格**的假脚本，让三条判据各响一次，'
note '  再放一个合格样本证明它不误报。这就是第 07 章「不响的检查等于没检查」的正面写法。'
note '  它什么时候退非 0？往参数里塞一个**不是取证脚本**的文件，闸门当场就响（下面只留不合格那一行）：'
runc 'out=$(python3 s_checklist.py demo.sh); echo "退出码 = $?（非 0 = 能当 CI / make check 用）"; grep -F demo.sh <<<"$out"'

# ============================================================================
banner 21 "自证：沙箱里的事只留在沙箱，章目录一个字都没多"
# ============================================================================
note 'demo 的写操作全部发生在 mktemp -d 的沙箱里；此刻的当前目录就是沙箱 work/。'
note '先把「该有什么」写成一条能判的式子，否则下面那些 0 只是「看着干净」：'
runc 'echo "沙箱 work/ 里的项数（不含 README.md）=$(ls | grep -vc "^README.md$")，前 12 个："; ls | grep -v "^README.md$" | head -12'
note '↑ 本章每个脚本都会建 xxx-demo/，所以「残留子目录数」这一项最容易出问题。先故意弄响它：'
runc 'mkdir -p 残留-demo; touch 残留.tmp; echo "塞进一个假产物之后：残留子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  非源码产物数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l)  残留 .tmp 个数=$(find . -name "*.tmp" | wc -l) ← 三个都会响"; rm -rf 残留-demo; rm -f 残留.tmp'
runc 'echo "清掉之后：残留子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  非源码产物数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l)  残留 .tmp 个数=$(find . -name "*.tmp" | wc -l)"'
note '↑ __pycache__ / *.pyc 这一对也先弄响一次：随便 import 一个模块，Python 就会把这两样造出来。'
runc 'printf "x = 1\n" > probe_mod.py; python3 -c "import probe_mod"; echo "import 之后：__pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l) ← 会响"; rm -rf __pycache__ probe_mod.py'
runc 'echo "本章跑完之后：__pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l)"'
note '↑ 顺带钉一条：只有 **import** 才产 __pycache__；`python3 xxx.py` 直接跑脚本不产 —— 本章全程跑脚本。'
note '章目录（源文件本体在这里）。清单显式排除 README.md，所以「README 写完之后重跑」得到的是同一份输出：'
runc_at "$HERE" 'find . -maxdepth 1 -type f ! -name "README.md" | sort'
runc_at "$HERE" 'echo ".py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)（这 19 个是源码本身，不是产物）  非源码产物数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l)  子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)"'
note '这一章一条网络命令都没用、没起过任何监听端口。本机在听的端口只打印**条数**——端口号和本章无关，不该写进教材：'
runc 'if command -v ss >/dev/null 2>&1; then echo "本机 LISTEN 条目数=$(ss -lnt | grep -c LISTEN)"; else echo "ss 本机没有（那就更没得起）"; fi'
runc 'pgrep -a sleep | head -3; echo "（上面这一行为空 = 系统里没有 demo 留下的后台进程）"'
note '★ 本章的取证纪律：所有「故意失败」都由 try/except 在脚本里接住并原样打印；'
note '  临时目录全在 /tmp/py08-XXXXXX，由 trap cleanup EXIT 整份删掉。'

printf '\n%s[全部跑完]%s 上面每一段都是真实执行结果；README.md 引用的输出全部出自这一次运行。\n' "$B" "$R"
printf '[一句话回顾] 字节、字符、路径是三件不同的事：encoding 管字节↔字符，newline 管行↔字符，\n'
printf '                  pathlib 管路径↔操作；写完 ≠ 落盘，with 只保证 close，配置只能靠 tmp + replace。\n'
