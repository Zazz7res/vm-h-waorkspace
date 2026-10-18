#!/usr/bin/env bash
# ============================================================================
# 第 03 章《字符串与文本》—— 整章证据一键复现
#
#   bash demo.sh                 跑完自动删沙箱（默认）
#   bash demo.sh --keep          跑完保留沙箱并打印路径，进去自己接着敲
#   bash demo.sh > out.txt 2>&1  重定向时自动关颜色，得到和本章 README 一样的纯文本
#
# 屏幕上出现的每一行，都是这条命令在这台机器上的真实结果 —— 没有一行是手写的。
#
# 安全说明：
#   * 示例源码整份复制进 mktemp 沙箱再运行；手册目录里不会留下 __pycache__ 等
#     任何产物（最后一节当场用 find 证明，输出必须是 0）。
#   * 只用标准库；不 pip install、不联网、不改任何配置文件；环境变量只在
#     单条命令前缀里设（子进程级），不 export 到你的 shell。
#   * 故意不加 `set -e`：本章有十几条「注定失败」的命令（IndexError、
#     UnicodeDecodeError、bytes+str……），要看的就是报错原文。
#   * 屏幕上不打印任何含本机用户名的绝对路径：要「在某目录里跑」的命令
#     用 runc_at，只显示目录名（basename）+ 相对命令。
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
# 记录脚本改动之前的语言环境（README 要引用这个事实）
ORIG_LANG="${LANG:-（未设）}"
ORIG_LC_ALL="${LC_ALL:-（未设）}"
# 固定语言：Python 自己的报错恒为英文，但 bash 的消息是翻译过的；
# 统一 C.UTF-8 后整份 transcript 才能逐字比对（你手敲看到中文完全正常）。
export LC_ALL=C.UTF-8
export LANG=C.UTF-8
# 清掉可能影响结论的环境变量（只影响本脚本及其子进程，不碰配置文件）。
unset PYTHONPATH PYTHONUTF8 PYTHONIOENCODING PYTHONDONTWRITEBYTECODE

if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/py03-XXXXXX")
export SB

banner() { printf '\n%s══════ %s %s\n' "$B" "$1" "══════════════════════════════════$R"; }
note()   { printf '%s\n' "$1"; }

# 打印命令 → 执行（stderr 合并进来）→ 打印退出码
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}

# 计时类命令数字每次都抖：先原样打一遍，再打一行 [形状]（数字全换成 N）。
# README 引用的输出只取形状行，所以你重跑一定能对上。
runc_norm() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    raw=$(bash -c "$1" 2>&1)
    code=$?
    printf '%s\n' "$raw"
    printf '[退出码 %d]\n' "$code"
    printf '%s\n' "$raw" | sed -E 's/[0-9]+([.][0-9]+)?/N/g' | sed 's/^/[形状] /'
}

# runc_at <目录> <命令>：先 cd 进去再跑；屏幕只显示目录 basename + 相对命令，
# 命令行里不出现绝对路径 —— 本机用户名的脱敏靠「根本不打印」，不是事后抹。
runc_at() {
    printf '\n%s$ 〔在 %s 里〕 %s%s\n' "$C" "$(basename "$1")" "$2" "$R"
    ( cd "$1" && bash -c "$2" ) 2>&1
    printf '[退出码 %d]\n' "$?"
    return 0
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

# 示例源码整份复制进沙箱（仓库目录只读；mtime 无关紧要，本章不碰 pyc）
mkdir -p "$SB/work"
cp -R "$HERE"/. "$SB/work"/
cd "$SB/work" || exit 1

# ============================================================================
banner "0. 环境体检：默认 locale、脚本设了什么、两个 encoding 各是什么"
# ============================================================================
note "[环境] 本脚本运行前：LANG=$ORIG_LANG  LC_ALL=$ORIG_LC_ALL"
note "[环境] 脚本已 export LC_ALL=C.UTF-8（只为报错/消息可逐字比对；str 方法本身与 locale 无关）"
runc 'python3 --version'
runc 'command -v python python3'
note '↑ PATH 里没有 python 这个名字（第 01 章讲过），本章一律写 python3。'
runc 'python3 -c "import sys; print(sys.getdefaultencoding())"'
runc 'python3 -c "import locale; print(locale.getpreferredencoding())"'
note '↑ 前一个是「str→bytes 的缺省编码」（恒 utf-8，与 locale 无关）；'
note '  后一个是「打开文件不给 encoding 时用什么」（跟 locale 走）。读写文件留给第 08 章。'
runc 'python3 -c "import unicodedata; print(unicodedata.unidata_version)"'
runc 'python3 -X utf8 -c "import sys; print(sys.flags.utf8_mode)"'

# ============================================================================
banner "1. str 是码点序列，不是字节：三个数字并排 + b\"中文\" 的编译期拒绝"
# ============================================================================
runc "python3 -c \"s = '中文'; print(len(s), len(s.encode('utf-8')), len(bytes(s, 'utf-8')))\""
note '↑ 2、6、6：str 数码点，bytes 数字节。C 里 char* 一个东西两件事，Python 拆成两个类型。'
runc 'python3 -c "print(len(b\"中文\"))"'
note '↑ 想在源码里直接写 b"中文"？编译期就拒绝（bytes 字面量只许 ASCII）。'
runc 'python3 a_codepoints.py 1'

# ============================================================================
banner "2. é 的两种写法：看起来一模一样，== 却是 False"
# ============================================================================
runc 'python3 a_codepoints.py 2'

# ============================================================================
banner "3. unicodedata.normalize 现场来回（NFC/NFD/NFKC）"
# ============================================================================
runc 'python3 a_codepoints.py 3'

# ============================================================================
banner "4. 同一个字符在不同 codec 下占几个字节（utf-8/gbk/gb18030/big5/latin-1/utf-16）"
# ============================================================================
runc 'python3 b_codecs.py 1'

# ============================================================================
banner "5. 编码侧编不下：报错原文 + errors='strict'/'replace'/'ignore' 三种命运"
# ============================================================================
runc "python3 -c \"'中文'.encode('latin-1')\""
note '↑ 这就是「latin-1 编不下中文」的报错原文：position 0-1 指的是【字符下标】（编码方向数的是 str 的码点）。'
runc 'python3 b_codecs.py 2'

# ============================================================================
banner "6. 解码侧字节不对：UnicodeDecodeError 完整原文（报的是字节位置！）"
# ============================================================================
runc "python3 -c \"b'\xd6\xd0\xce\xc4'.decode('utf-8')\""
note '↑ 拿 GBK 的 4 个字节冒充 UTF-8。注意 in position 0 —— 这里的 position 是【字节下标】，'
note '  和上面编码方向报错（str 码点下标）正好相反。b_codecs.py 3 把 e.start/e.end 拆给你看。'
runc 'python3 b_codecs.py 3'

# ============================================================================
banner "7. BOM：EF BB BF、U+FEFF、utf-8-sig"
# ============================================================================
runc 'python3 b_codecs.py 4'

# ============================================================================
banner "8. 本机有哪些 codec：codecs.lookup 逐个试，不猜"
# ============================================================================
runc 'python3 b_codecs.py 5'
runc "python3 -c \"import codecs; codecs.lookup('mbcs')\""
note '↑ 未注册 codec 的报错原文；注意它不是 UnicodeError 家族，是 LookupError。'

# ============================================================================
banner "9. 切片越界：s[100] 抛 IndexError，s[i:j] 越界却安静给短串"
# ============================================================================
runc "python3 -c \"'hello'[100]\""
runc "python3 -c \"print(repr('hello'[2:100]), repr('abc'[10:20]), repr('hello'[100:]), repr('hello'[-100:2]))\""
note '↑ 四条切片全都没报错。C 里越界是未定义行为；Python 切片是「自动夹断」——安全，但藏 bug。'
runc 'python3 c_slicing.py 1'

# ============================================================================
banner "10. 负索引、步长、slice 对象；切片赋值：list 行、str 不行（原文）"
# ============================================================================
runc 'python3 c_slicing.py 2'
runc 'python3 -c "s = \"hello\"; s[1:3] = \"X\""'
note '↑ 切片能读不能写：str 不可变（第 02 章）。list 的切片赋值在同一节里对照跑了。'

# ============================================================================
banner "11. f-string：{x=} 调试语法、!r/!s/!a、花括号转义"
# ============================================================================
runc 'python3 d_fstring.py 1'

# ============================================================================
banner "12. f-string 格式规范：填充/对齐/宽度/千分位/精度/符号/进制"
# ============================================================================
runc 'python3 d_fstring.py 2'

# ============================================================================
banner "13. 嵌套引号在 3.14 上到底能怎么写（全实测）"
# ============================================================================
runc 'python3 d_fstring.py 3'

# ============================================================================
banner "14. datetime 格式串；strftime 是本章唯一和 locale 有关的事"
# ============================================================================
runc 'python3 d_fstring.py 4'
runc "LC_ALL=zh_CN.UTF-8 python3 -c \"from datetime import datetime; print(datetime(2026, 10, 3, 14, 5, 9).strftime('%A %d %B'))\""
note '↑ 环境变量给 zh_CN.UTF-8 也没用（实测）：Python 不替你把 LC_TIME 设进去，要代码里 setlocale。'

# ============================================================================
banner "15. str.format / % / 加号 / join 同台对比"
# ============================================================================
runc 'python3 d_fstring.py 5'

# ============================================================================
banner "16. 循环里 += 拼串的代价：timeit 实测（数字会抖，看 [形状] 行）"
# ============================================================================
runc_norm "python3 -m timeit -s \"
words = ['词%d' % i for i in range(1000)]
def plus():
    out = ''
    for w in words:
        out += w
    return out
\" \"plus()\""
runc_norm "python3 -m timeit -s \"words = ['词%d' % i for i in range(1000)]\" \"''.join(words)\""
note '↑ 同样 1000 个词：+= 是逐次复制整串（O(n²)），join 先数总长再一次填（O(n)）。'
runc_norm "python3 -m timeit -s \"a, b = '张三', 42\" \"f'{a}-{b}'\""
runc_norm "python3 -m timeit -s \"a, b = '张三', 42\" \"'{}-{}'.format(a, b)\""
note '↑ 单次格式化 f-string 比 .format 快；但差距是几十纳秒级 —— 选型看可读性，不看这点速度。'

# ============================================================================
banner "17. split 家族的真脾气：split() vs split(' ')、maxsplit、rsplit、partition"
# ============================================================================
runc 'python3 e_split_join.py 1'
runc 'python3 e_split_join.py 2'
runc "python3 -c \"'abc'.split('')\""
note '↑ split 不接受空串分隔符：ValueError 原文。'

# ============================================================================
banner "18. splitlines 认哪些「换行」：11 种逐个实测，和 split('\\n') 对照"
# ============================================================================
runc 'python3 e_split_join.py 3'

# ============================================================================
banner "19. join 的脾气与「split 再 join 不还原」最小反例"
# ============================================================================
runc 'python3 e_split_join.py 4'
runc "python3 -c \"','.join([1, 2])\""
note '↑ join 只吃 str 序列；报错原文连第几项出错都告诉你（sequence item 0）。'

# ============================================================================
banner "20. strip 删的是字符集合，不是前缀；removeprefix/removesuffix 才是砍前后缀"
# ============================================================================
runc 'python3 f_strip_case.py 1'

# ============================================================================
banner "21. 大小写（ß→SS、土耳其 İ）与 isalpha/isdigit/isnumeric 判定表"
# ============================================================================
runc 'python3 f_strip_case.py 2'
runc 'python3 f_strip_case.py 3'
runc "LC_ALL=zh_CN.UTF-8 python3 -c \"print('ß'.upper(), 'ß'.casefold(), len('ß'.upper()))\""
runc "python3 -c \"print('ß'.upper(), 'ß'.casefold(), len('ß'.upper()))\""
note '↑ 两条结果逐字相同：str 的大小写方法按 Unicode 表走，locale 完全不掺和（对照第 14 节 strftime）。'

# ============================================================================
banner "22. 显示宽度：east_asian_width、三个数字、CJK 对齐表格两种补法"
# ============================================================================
runc 'python3 g_width.py'

# ============================================================================
banner "23. bytes 与 str 的边界、bytearray 可变、in 与逐字符遍历"
# ============================================================================
runc 'python3 h_bytes_iter.py 1'
runc 'python3 h_bytes_iter.py 2'
runc "python3 -c \"b'abc' + 'abc'\""
runc "python3 -c \"bytes([256])\""
note '↑ bytes 和 str 不许直接相加；bytes() 只收 0~255 —— 越界报错都给你指出来。'

# ============================================================================
banner "24. 收尾：证明这一章没在手册目录里留下任何产物"
# ============================================================================
runc_at "$HERE" "find . \\( -name '__pycache__' -o -name '*.pyc' -o -name '*.pyo' -o -name '*.tmp' \\) | wc -l"
note '↑ 这一条的输出必须是 0：所有示例都不写文件，沙箱也只在 /tmp 里活动。'
runc "find . -newer a_codepoints.py -name '*.pyc' | wc -l"

printf '\n%s[全部跑完] 上面每一段都是真实执行结果；README.md 引用的输出全部出自这一次运行。%s\n' "$B" "$R"
exit 0
