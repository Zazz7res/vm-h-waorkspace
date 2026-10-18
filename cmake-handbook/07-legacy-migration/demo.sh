#!/usr/bin/env bash
# ============================================================================
# 第 07 章《老写法 → 新写法迁移》整章证据
#
#   bash demo.sh             跑完自动删沙箱（默认）
#   bash demo.sh --keep      保留沙箱，自己进去接着拨开关
#   bash demo.sh > out.txt   重定向自动关颜色，拿纯文本
#
# 本章只回答一个问题：**已经存在的老 CMakeLists 怎么改，才能确认自己没改坏。**
# 方法论只有一句：改写安不安全，不靠「能跑」，靠**改写前后传给编译器的字符是否一致**
# —— 所以全章反复用三个文件：flags.make（-I / -D / 编译标志）、link.txt（-L / 库路径）、
# build.make（源文件清单）。每个结论后面都跟着一次真跑。
#
# 安全说明：
#   * 全程在 mktemp 沙箱里；章节目录只有一份副本被拷进去，跑完 trap 只删自己建的那个。
#   * 章节目录里不留任何构建产物：最后一节用 find 自证 0。
#   * 全程不联网、不装东西、不 sudo、不写任何全局配置（~/.cmake 一个字节都不碰）。
#   * --prefix 之类的外部落点一个都不用；两份 libfoo.a 也是在沙箱里现造的。
#   * 输出里不出现本机登录名：要读仓库根那份真实文件时用 runc_at（屏幕上只留目录名 + 相对命令），
#     也全程不打印 make 程序在哪（不 which make、不 cmake -LA）。
#   * 只讲 C：project(... C)，生成器一律用默认的 Unix Makefiles（本机没有 ninja / g++）。
#
# 写作约定（改这个脚本前先看）：
#   * note 的字符串里不要出现反引号，也不要出现成对的 ASCII 双引号 —— 双引号会短路。
#   * runc 的字符串是被 bash -c 执行的：凡是想让**子 shell** 处理的 $t / $? / $( )
#     一律写成 \$t、\$?、\$( )，否则它们会在本脚本里就被吃掉了。

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

set -u
export LC_ALL=C.UTF-8 LANG=C

if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/cmake07-XXXXXX")
PROJ="$SB/ch07"
BD="$SB/b"
LIBS="$SB/libs"

banner() { printf '\n%s══════ %s %s\n' "$B" "$1" "════════════════════════════════$R"; }
note()   { printf '  %s\n' "$1"; }

runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    # pipefail：[退出码] 报的是整条管道的真实成败，不被 grep/head 的 0 掩盖
    bash -c "set -o pipefail; $1" 2>&1
    local ec=$?          # 必须写成一条语句：local ec; ec=$? 会被 local 自己把 $? 洗成 0
    printf '[退出码 %d]\n' "$ec"
    return 0
}

# runc_at <目录> <命令>：先 cd 进那个目录再跑，屏幕上只留目录名（basename）+ 相对命令。
#   要读仓库根那种真实文件时用它：命令行里不出现本机绝对路径，登录名一个都不留。
runc_at() {
    printf '\n%s$ 〔在 %s 里〕 %s%s\n' "$C" "$(basename "$1")" "$2" "$R"
    ( cd "$1" && bash -c "set -o pipefail; $2" ) 2>&1
    printf '[退出码 %d]\n' "$?"
    return 0
}

# ★ runc 是 bash -c 跑一个字符串的，**子 shell 看不见本脚本里的函数**。
#   所以本章所有「在子 shell 里也要用」的工具都落成一个真的脚本文件（seq.sh / dump.sh / snap.sh），
#   而不是写成 demo.sh 里的函数 —— 这个坑是第 06 章第 15 节当场撞出来的。
cat > "$SB/seq.sh" <<'SEQEOF'
#!/usr/bin/env bash
# 取证小工具（一）：把一类标志**按出现顺序**一行一个抽出来，方便 diff。
#   用法: seq.sh <flags.make 或 link.txt> <-I | -D | -L | -W>
# flags.make 把同类标志全挤在一行里，人眼比不了顺序、diff 也说不清改了哪一项；
# link.txt 里没有 C_INCLUDES 这种行名，所以按文件名分两条路走。
F=$1
K=$2
case "$(basename "$F")" in
    link.txt)
        grep -oE -- "$K[^ ]*" "$F"
        ;;
    flags.make)
        case "$K" in
            -I) L=C_INCLUDES ;;
            -D) L=C_DEFINES ;;
            -W) L=C_FLAGS ;;
            *)  L=$K ;;
        esac
        grep -m1 "^$L = " "$F" | grep -oE -- "$K[^ ]*"
        ;;
    *)
        echo "seq.sh 只认 flags.make 和 link.txt" >&2
        exit 2
        ;;
esac
SEQEOF

cat > "$SB/dump.sh" <<'DUMPEOF'
#!/usr/bin/env bash
# 取证小工具（二）：一个构建目录里，每个 target 的某一行标志单独打印，前面带上 target 名。
#   用法: dump.sh <构建目录> <C_INCLUDES | C_DEFINES | C_FLAGS>
# 全章「覆盖范围」类的问题都靠它一眼看完：谁有、谁没有、谁多了一条。
BD=$1
KEY=$2
find "$BD" -name flags.make | sort | while read -r f; do
    t=$(basename "$(dirname "$f")" .dir)
    printf '%-9s %s\n' "$t" "$(grep -m1 "^$KEY = " "$f" | sed "s|^$KEY = ||")"
done
DUMPEOF

cat > "$SB/snap.sh" <<'SNAPEOF'
#!/usr/bin/env bash
# 取证小工具（三）：把一个构建目录里所有 flags.make + link.txt 原样钉进一个文件。
#   用法: snap.sh <构建目录> <存盘文件>
# 迁移第 0 步要用它抓基线：没有左边，后面每一步的 diff 都没有左边。
BD=$1
OUT=$2
: > "$OUT"
find "$BD" -name flags.make -o -name link.txt | sort | while read -r f; do
    printf '== %s\n' "${f#"$BD"/}" >> "$OUT"
    cat "$f" >> "$OUT"
done
wc -l "$OUT"
SNAPEOF

chmod +x "$SB/seq.sh" "$SB/dump.sh" "$SB/snap.sh"

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  工程副本：%s\n  构建目录：%s\n  看完删： rm -rf "%s"\n' \
               "$SB" "$PROJ" "$BD" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# ---------------------------------------------------------------------------
banner "01 本章的三套工程布局，和那份「能跑的老写法」"
# ---------------------------------------------------------------------------
mkdir -p "$BD" "$PROJ"
cp "$HERE/CMakeLists.txt" "$PROJ/"
cp -r "$HERE/include" "$HERE/src" "$HERE/tp" "$HERE/legacy" "$HERE/modern" "$HERE/variants" "$PROJ/"
runc "cmake --version | head -1 && gcc --version | head -1 && make --version | head -1"
runc "ls -1 $PROJ"
note "上面这一份就是本章全部素材：CMakeLists.txt（实验室）+ legacy/（老写法整份）+ modern/（改写后整份）"
note "+ include/ src/ tp/（三套工程共用的源码）+ variants/（每节一份故意写坏的工程）。"
note "★ 源码只有一份。legacy/ 和 modern/ 算出来的 ROOT 是同一个目录，"
note "  所以它们的 flags.make 可以直接 diff —— 改的只有机制，没有意图。"
runc "ls -1 $PROJ/variants"
runc "wc -l $PROJ/legacy/CMakeLists.txt $PROJ/modern/CMakeLists.txt $PROJ/CMakeLists.txt"

# 先造两份同名不同内容的 libfoo.a（第 2 部分的靶子）
mkdir -p "$LIBS/v1" "$LIBS/v2"
runc "gcc -c -I$PROJ/include -DCH07_FOO_V=1 $PROJ/src/foo/foo.c -o $LIBS/v1/foo.o && ar rcs $LIBS/v1/libfoo.a $LIBS/v1/foo.o && gcc -c -I$PROJ/include -DCH07_FOO_V=2 $PROJ/src/foo/foo.c -o $LIBS/v2/foo.o && ar rcs $LIBS/v2/libfoo.a $LIBS/v2/foo.o && find $LIBS -name libfoo.a"
note "两份 libfoo.a：名字一模一样，内容不同（一份 foo_tag() 返回 1、一份返回 2），"
note "区别只在造它们时那两条 -DCH07_FOO_V。**只有两份同名的库才能把 -L 顺序这件事照出来**"
note "—— 只有一份库的时候，顺序怎么乱都能跑。"

# legacy/ 整份：先证明它一点毛病没有
runc "cmake -S $PROJ/legacy -B $BD/legacyproj -DMODE=PLUS -DSTAMP=7 -DFOO_DIR1=$LIBS/v1 -DFOO_DIR2=$LIBS/v2 2>&1 | tail -3"
runc "cmake --build $BD/legacyproj -j4 2>&1 | tail -2"
runc "$BD/legacyproj/app && echo --- && $BD/legacyproj/greet && echo --- && $BD/legacyproj/fooprog"
note "配置成功、构建成功、三个程序都跑出「正确结果」，屏幕上一个警告都没有。"
note "★ 这就是本章最难的一点：**老写法不是坏的。** 它没有一条报错，所以没有任何机制逼着人改；"
note "  而它已经在偷偷影响别人了 —— 下面几节逐个取证。"

# ---------------------------------------------------------------------------
banner "02 取证现场：一个 target 真正吃到什么，只看三个文件"
# ---------------------------------------------------------------------------
# 现代版基线：七个回滚开关全 OFF，只把行为旋钮拨到和第 01 节那份老写法一样
runc "cmake -S $PROJ -B $BD/base -DCH07_FOO_DIR1=$LIBS/v1 -DCH07_FOO_DIR2=$LIBS/v2 -DCH07_STAMP=7 -DCH07_TRACE=ON 2>&1 | tail -5"
runc "cmake --build $BD/base -j4 2>&1 | tail -3"
runc "find $BD/base -name flags.make | sort"
note "★ flags.make 一个 target 一份；它在生成阶段写出来，构建阶段被生成的 Makefile 读。"
note "  这一节把 app 的那份整个念出来，后面每一节比的都是它里面的行。"
runc "cat $BD/base/CMakeFiles/app.dir/flags.make"
note "三行是本章的取证对象：C_DEFINES（-D）、C_INCLUDES（-I）、C_FLAGS（编译标志）。"
note "它们**不是** CMakeLists 里那些行的拷贝，是 CMake 把目录属性和 target 属性派发完之后的产物 —— 所以只有可信。"
runc "cat $BD/base/CMakeFiles/app.dir/link.txt"
runc "cat $BD/base/CMakeFiles/fooprog.dir/link.txt"
note "link.txt 里是链接命令本体：-L、库路径、-l 都在这里。注意 fooprog 那行**没有 -L**，"
note "它拿到的是一个 .a 的绝对路径 —— 这就是第 2 部分要对照的东西。"
runc "grep -c src/app.c $BD/base/CMakeFiles/app.dir/build.make"
note "↑ 第三个文件 build.make 里才是一张真正的源文件清单（第 4 部分讲 file(GLOB) 时盯的就是它和用例名单）。"

runc "cat $SB/seq.sh"
note "本章的取证工具就上面这一份小脚本（还有后面两份 dump.sh / snap.sh），全部只落在沙箱里。"
note "它存在的唯一理由：flags.make 把同类标志全挤在一行里，看不出顺序；抽成一行一个之后才能 diff。"
runc "$SB/seq.sh $BD/base/CMakeFiles/app.dir/flags.make -I"
runc "$SB/seq.sh $BD/base/CMakeFiles/app.dir/flags.make -D"
runc "cat $SB/dump.sh"
runc "$SB/dump.sh $BD/base C_INCLUDES"
note "★ 这份输出后面反复要用：现代写法里 -I 只出现在真正 include 了本工程头文件的 target 上，"
note "  leaf_a、leaf_b、notepad 三行是空的 —— 它们一个本工程的头都不需要。"

# ---------------------------------------------------------------------------
banner "03 include_directories 到底什么时候算：三个靶子站三个位置"
# ---------------------------------------------------------------------------
# 流传最广的一句话是「include_directories 必须写在 add_executable 之前，否则吃不到」。
# 这一节当场判真假。
runc "cmake -S $PROJ/variants/order -B $BD/order 2>&1 | grep -E 'early|late|before_sub|after_sub'"
note "五个靶子的位置：early 在 include_directories **之前**创建；late 在**之后**；"
note "before_sub 在**之前**被 add_subdirectory；after_sub 在**之后**；三个 message 直接读属性。"
runc "for t in early late; do printf '%-6s ' \$t; $SB/seq.sh $BD/order/CMakeFiles/\$t.dir/flags.make -I; done"
runc "for p in CMakeFiles/early.dir sub_before/CMakeFiles/before_sub.dir sub_after/CMakeFiles/after_sub.dir; do printf '%-40s ' \$p; grep -h '^C_INCLUDES' $BD/order/\$p/flags.make; done"
note "★ 屏幕上真正的一条线：**同一个目录里不分先后，全都吃到**（early 在那条指令之前就创建了，照样有 -I）；"
note "  真正分先后的是**子目录** —— 它吃不吃得到，只看 add_subdirectory 那一刻目录属性里有没有东西。"
runc "for p in CMakeFiles/early.dir sub_before/CMakeFiles/before_sub.dir sub_after/CMakeFiles/after_sub.dir; do printf '%-40s ' \$p; grep -h '^C_DEFINES' $BD/order/\$p/flags.make; done"
note "add_definitions 和它同一套规则（上面三条 C_DEFINES 的形状和 C_INCLUDES 完全对应）。"
note "而 link_directories 恰好**相反**，它是真快照 —— 第 10 节当场演。"
note "所以「老写法必须写在最前面」这一句话把三种不同的东西说成了一种：它只对 link_directories 成立；"
note "对 include_directories / add_definitions 反而是「写在前面污染面更大」。"

# ---------------------------------------------------------------------------
banner "04 真病一：一个目录里所有 target 都吃到，包括你不想给的那个"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ -B $BD/legacyall -DCH07_LEG_INCLUDE=ON -DCH07_LEG_DEFS=ON -DCH07_FOO_DIR1=$LIBS/v1 -DCH07_FOO_DIR2=$LIBS/v2 -DCH07_STAMP=7 -DCH07_TRACE=ON 2>&1 | tail -4"
runc "cmake --build $BD/legacyall -j4 2>&1 | tail -2"
runc "$SB/dump.sh $BD/base C_INCLUDES"
runc "$SB/dump.sh $BD/legacyall C_INCLUDES"
note "↑ 前一份是现代写法，后一份只多开了 include_directories 和 add_definitions 两个开关。"
note "★ 这才是 include_directories 的真病：不是「忘了写就吃不到」，而是「你想给一个、结果全给了」。"
note "  危害不是编译不过（它永远编译得过），是另外两件事：同名头文件的赢家会变（第 07 节当场演），"
note "  以及别人导出的库带着你的 -I 一起交付（第 03 章第 8 节演的就是这件事）。"

# ---------------------------------------------------------------------------
banner "05 真病二：污染会顺着 add_subdirectory 走进第三方目录"
# ---------------------------------------------------------------------------
runc "for d in base legacyall; do printf '%-10s ' \$d; grep -h '^C_INCLUDES' $BD/\$d/tp/CMakeFiles/notepad.dir/flags.make; done"
runc "cmake --build $BD/base --target notepad -j4 >/dev/null && $BD/base/tp/notepad"
runc "$BD/legacyall/tp/notepad"
note "同一份 notepad.c、同一份源码树，屏幕上多了一行「有人给全工程打开了 trace，我被迫打印这行」。"
note "第三方目录的作者压根不知道本工程有个 CH07_DEBUG_TRACE，更不想知道。"
runc "diff <($SB/seq.sh $BD/base/tp/CMakeFiles/notepad.dir/flags.make -I) <($SB/seq.sh $BD/legacyall/tp/CMakeFiles/notepad.dir/flags.make -I); echo \"[notepad 的 -I diff 退出码 \$?]\""
note "★ 迁移这一项的验收动作就是上面这条 diff：它现在**不为 0**（老写法多给了一个 -I），"
note "  改对之后它应当为 0（对需要的 target）或「少一项」（对不该给的 target，比如这里）。"

# ---------------------------------------------------------------------------
banner "06 改写对质：同一份源码，两种写法，-I 序列逐 target diff"
# ---------------------------------------------------------------------------
runc "for t in app greet calc util fooprog; do printf '%-8s ' \$t; diff <($SB/seq.sh $BD/base/CMakeFiles/\$t.dir/flags.make -I) <($SB/seq.sh $BD/legacyall/CMakeFiles/\$t.dir/flags.make -I) >/dev/null && echo 完全一致 || echo 有差别; done"
note "前五个 target 的 -I 序列一字不差 —— 对它们来说这次改写是**等价改写**，可以放行。"
runc "for t in leaf_a leaf_b; do printf '%-8s 现代 ' \$t; $SB/seq.sh $BD/base/CMakeFiles/\$t.dir/flags.make -I | wc -l; printf '%-8s 老写 ' \$t; $SB/seq.sh $BD/legacyall/CMakeFiles/\$t.dir/flags.make -I | wc -l; done; printf '%-8s 现代 ' notepad; $SB/seq.sh $BD/base/tp/CMakeFiles/notepad.dir/flags.make -I | wc -l; printf '%-8s 老写 ' notepad; $SB/seq.sh $BD/legacyall/tp/CMakeFiles/notepad.dir/flags.make -I | wc -l"
note "↑ 每个数字都是「这个 target 的 -I 有几条」。现代写法给 0 的那三个，老写法全给了 1 条。"
note "★ 两类差别要分开记账，这正是第 6 部分配方里「diff 不为 0 时怎么判断是不是自己改坏了」的依据："
note "  ① 需要的 target：diff 必须为 0，不为 0 就是改坏了；"
note "  ② 不该给的 target：diff 应当「少一项」，且程序行为要回来重新看一眼（第 15 节那种）。"

# ---------------------------------------------------------------------------
banner "07 -I 的「顺序」也是命令行的一部分：BEFORE 那一手当场测"
# ---------------------------------------------------------------------------
runc_at "$PROJ/variants/inc-order" "grep -H 'define CH07_WHO' std/ch07/hdrwin.h alt/ch07/hdrwin.h"
note "两份**同名**头文件，宏名也一样，只有字符串不同 —— 谁排在 -I 前面，屏幕上就出现谁。"
runc "cmake -S $PROJ/variants/inc-order -B $BD/incplain -DWAY=plain 2>&1 | grep -E 'inc-order|Configuring done'"
runc "cmake --build $BD/incplain 2>&1 | tail -1"
runc "$SB/seq.sh $BD/incplain/CMakeFiles/hdrwin.dir/flags.make -I | sed 's|-I$PROJ/variants/inc-order/|-I|' && $BD/incplain/hdrwin"
runc "cmake -S $PROJ/variants/inc-order -B $BD/incbefore -DWAY=before 2>&1 | grep -E 'inc-order|Configuring done'"
runc "cmake --build $BD/incbefore 2>&1 | tail -1"
runc "$SB/seq.sh $BD/incbefore/CMakeFiles/hdrwin.dir/flags.make -I | sed 's|-I$PROJ/variants/inc-order/|-I|' && $BD/incbefore/hdrwin"
note "★ 本机实测：include_directories(BEFORE 目录1 目录2) 把**整批反着**插到最前面，"
note "  于是 -I 从 alt,std 变成 std,alt，屏幕上换了一个赢家。两次都构建成功、测试没有意见。"
runc "diff <($SB/seq.sh $BD/incplain/CMakeFiles/hdrwin.dir/flags.make -I) <($SB/seq.sh $BD/incbefore/CMakeFiles/hdrwin.dir/flags.make -I); echo \"[-I 序列 diff 退出码 \$?]\""
runc "$SB/seq.sh $BD/incplain/CMakeFiles/hdrwin.dir/flags.make -I | wc -l && $SB/seq.sh $BD/incbefore/CMakeFiles/hdrwin.dir/flags.make -I | wc -l"
note "↑ 条数一模一样、集合一模一样，只有顺序相反 —— 所以「比谁多谁少」不够，必须抽成一行一个再 diff。"

# ---------------------------------------------------------------------------
banner "08 link_directories 落到命令行上是什么样子：-L 之外还多送一个 -rpath"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ -B $BD/legacyall -DCH07_LEG_LINK=ON -DCH07_LEG_GLOB=ON -DCH07_LEG_FLAGS=ON -DCH07_LEG_PLAIN=ON -DCH07_LEG_CACHEVAR=ON 2>&1 | tail -4"
runc "cmake --build $BD/legacyall -j4 2>&1 | tail -2"
runc "cat $BD/legacyall/CMakeFiles/fooprog.dir/link.txt"
runc "$SB/seq.sh $BD/legacyall/CMakeFiles/fooprog.dir/link.txt -L"
note "link_directories 的产物就是那两个 -L；同一行里还多了一条 -Wl,-rpath,……（运行时搜索路径），"
note "这是目录级命令顺手替你加的，删它要另外花力气 —— 第 03 章第 13 节讲 RPATH 时会再认它。"
note "★ 注意：命令行里只有目录，**没有任何一行写着我要哪一份 libfoo.a**。挑哪一份是链接器按顺序决定的。"

# ---------------------------------------------------------------------------
banner "09 -L 的顺序决定谁赢：两份同名库，屏幕上是两个不同的数字"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ/variants/link-order -B $BD/lo12 -DFOO_A=$LIBS/v1 -DFOO_B=$LIBS/v2 -DORDER=ab -DWHEN=before >/dev/null && cmake --build $BD/lo12 >/dev/null && $SB/seq.sh $BD/lo12/CMakeFiles/orderapp.dir/link.txt -L && $BD/lo12/orderapp"
runc "cmake -S $PROJ/variants/link-order -B $BD/lo21 -DFOO_A=$LIBS/v1 -DFOO_B=$LIBS/v2 -DORDER=ba -DWHEN=before >/dev/null && cmake --build $BD/lo21 >/dev/null && $SB/seq.sh $BD/lo21/CMakeFiles/orderapp.dir/link.txt -L && $BD/lo21/orderapp"
runc "diff <($SB/seq.sh $BD/lo12/CMakeFiles/orderapp.dir/link.txt -L) <($SB/seq.sh $BD/lo21/CMakeFiles/orderapp.dir/link.txt -L); echo \"[-L 序列 diff 退出码 \$?]\""
note "★ 同一份源码、同一个 CMakeLists，只换了 link_directories 里两个目录的先后："
note "  程序一个打印 1、一个打印 2，**两边都构建成功、谁也不报错**。"
note "  （target 叫 orderapp，屏幕上打的是 [fooprog] —— 因为它编的就是第 01 节那份 src/fooprog.c，全章只有那一个 foo 消费者。）"
note "  这就是 link_directories 的全部危险性：赢家写在你的手指头上，不写在文件里。"

# ---------------------------------------------------------------------------
banner "10 link_directories 是快照：写在 add_executable 之后，一个 -L 都没有"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ/variants/link-order -B $BD/loafter -DFOO_A=$LIBS/v1 -DFOO_B=$LIBS/v2 -DORDER=ab -DWHEN=after 2>&1 | grep -E 'link-order|Configuring done|warn'"
note "↑ 配置干净退出，一条警告都没有。"
runc "cat $BD/loafter/CMakeFiles/orderapp.dir/link.txt"
runc "$SB/seq.sh $BD/loafter/CMakeFiles/orderapp.dir/link.txt -L | wc -l"
runc "cmake --build $BD/loafter 2>&1 | tail -4"
note "三种目录级全局指令，这里是**第三种行为**：link_directories 只看 target 创建那一刻。"
note "★ 所以那句「老写法一定要写在前面」对 link_directories 是真、对 include_directories/add_definitions 是假。"
note "  迁移时这一条最容易被「顺手整理一下顺序」改坏：把 link_directories 往文件后面挪一行，"
note "  它就从生效变成完全失效，而配置照样 Configuring done —— 只有链接这一步会响。"

# ---------------------------------------------------------------------------
banner "11 新写法不需要 -L：给 target 名字，或者给 .a 的绝对路径"
# ---------------------------------------------------------------------------
runc "cat $BD/base/CMakeFiles/fooprog.dir/link.txt"
runc "$BD/base/fooprog"
runc "cmake -S $PROJ -B $BD/pick2 -DCH07_FOO_PICK=2 -DCH07_FOO_DIR1=$LIBS/v1 -DCH07_FOO_DIR2=$LIBS/v2 2>&1 | grep -E '回滚开关|Configuring done'"
runc "cmake --build $BD/pick2 --target fooprog >/dev/null && cat $BD/pick2/CMakeFiles/fooprog.dir/link.txt && $BD/pick2/fooprog"
note "现代写法里「选哪一份」是**写在 CMakeLists 里的一行**（IMPORTED_LOCATION），不是 -L 排出来的。"
note "第 09 节那两种顺序在这里变成两个明确的值，diff 得出来、grep 得出来、review 也看得见。"

# ---------------------------------------------------------------------------
banner "12 链接这一项的迁移验收：link.txt 规范化 diff"
# ---------------------------------------------------------------------------
runc "$SB/seq.sh $BD/legacyall/CMakeFiles/fooprog.dir/link.txt -L"
runc "$SB/seq.sh $BD/base/CMakeFiles/fooprog.dir/link.txt -L"
runc "diff <($SB/seq.sh $BD/legacyall/CMakeFiles/fooprog.dir/link.txt -L) <($SB/seq.sh $BD/base/CMakeFiles/fooprog.dir/link.txt -L); echo \"[-L diff 退出码 \$?]\""
runc "grep -oE '[^ ]*libfoo.a' $BD/base/CMakeFiles/fooprog.dir/link.txt"
runc "grep -oE -- '-lfoo' $BD/legacyall/CMakeFiles/fooprog.dir/link.txt"
runc "grep -l -- -L $BD/legacyall/CMakeFiles/*.dir/link.txt $BD/legacyall/tp/CMakeFiles/*.dir/link.txt 2>/dev/null"
runc "grep -l -- -L $BD/base/CMakeFiles/*.dir/link.txt $BD/base/tp/CMakeFiles/*.dir/link.txt 2>/dev/null | wc -l"
note "★ 这一项的 diff 天生不为 0（机制换了，命令行必然变）。所以它的验收不是一模一样，而是三条："
note "  ① 库文件路径必须**唯一确定**（一个绝对路径，不是一个名字）；"
note "  ② 两份构建跑出来的**程序输出**必须一致；"
note "  ③ 少掉的 -L 要确认没有别的 target 还依赖它 —— 上两条命令就是把两边所有 link.txt 全扫一遍。"

# ---------------------------------------------------------------------------
banner "13 add_definitions 的覆盖范围：一次配置，所有 target 一起变"
# ---------------------------------------------------------------------------
runc "$SB/dump.sh $BD/base C_DEFINES"
runc "$SB/dump.sh $BD/legacyall C_DEFINES"
note "老写法那一半：**每一个 target 都拿到一模一样的三个 -D**，包括第三方 notepad 和两个 leaf 小程序。"
note "现代写法那一半：calc 只有 CH07_MODE_*、util 只有 CH07_STAMP、app 只有 TRACE，其余全是空的。"
runc "for t in util calc app; do printf '%-6s ' \$t; grep -h '^C_DEFINES' $BD/base/CMakeFiles/\$t.dir/flags.make; done"
note "★ PRIVATE 的意义在这里看得见摸得着：宏只在「编译这个 .c 的那一刻」存在，不外传。"

# ---------------------------------------------------------------------------
banner "14 不问 CMake，直接问预处理器：-dM -E"
# ---------------------------------------------------------------------------
runc "gcc -dM -E -I$PROJ/include $PROJ/src/util/util.c | grep -c CH07_STAMP"
note "↑ 0 且退出码 1：什么 -D 都不给的时候，预处理完就没有这个宏。这是对照组（-I 只是为了让头文件找得到）。"
runc "gcc -dM -E \$(grep -m1 '^C_DEFINES' $BD/base/CMakeFiles/util.dir/flags.make | sed 's/^C_DEFINES = //') \$(grep -m1 '^C_INCLUDES' $BD/base/CMakeFiles/util.dir/flags.make | sed 's/^C_INCLUDES = //') $PROJ/src/util/util.c | grep CH07"
runc "gcc -dM -E \$(grep -m1 '^C_DEFINES' $BD/base/CMakeFiles/greet.dir/flags.make | sed 's/^C_DEFINES = //') \$(grep -m1 '^C_INCLUDES' $BD/base/CMakeFiles/greet.dir/flags.make | sed 's/^C_INCLUDES = //') $PROJ/src/greet.c | grep CH07"
runc "gcc -dM -E \$(grep -m1 '^C_DEFINES' $BD/legacyall/tp/CMakeFiles/notepad.dir/flags.make | sed 's/^C_DEFINES = //') $PROJ/tp/notepad.c | grep CH07"
note "★ 中间那两条是本章最喜欢的一招：把 flags.make 里的 C_DEFINES **原样喂回 gcc**，"
note "  再用 -dM -E 把「预处理完到底剩下哪些宏」打出来。它绕开 CMake 的一切说法，直接和编译器对质。"
note "  同一个 greet.c：现代写法里没有 CH07_DEBUG_TRACE，老写法的 notepad 里却有三个 —— 用预处理器的嘴说出来。"

# ---------------------------------------------------------------------------
banner "15 最贵的一种失败：构建全绿，程序行为变了"
# ---------------------------------------------------------------------------
runc "$BD/base/app && echo --- && $BD/base/greet && echo --- && $BD/base/tp/notepad"
runc "$BD/legacyall/app && echo --- && $BD/legacyall/greet && echo --- && $BD/legacyall/tp/notepad"
note "★ 两份构建：配置都是 0、构建都是 0、ctest 都能跑。区别只在屏幕上多了两行 trace。"
note "  这一类「编译照过、行为变了」的事，第 02 章场景 8 已经演过一次，本章是它的老写法版本："
note "  全局指令的失败模式**永远是这一类**，不会是编译不过。"
runc "ctest --test-dir $BD/base 2>&1 | tail -3"
runc "ctest --test-dir $BD/legacyall 2>&1 | tail -3"
note "两份都是绿的。**测试绿不绿和宏传给了谁，是两件互不相干的事**（第 06 章那句话的迁移版）。"

# ---------------------------------------------------------------------------
banner "16 target_compile_definitions 的 -D 序列 diff"
# ---------------------------------------------------------------------------
runc "for t in calc util app greet; do printf '%-6s 现代: ' \$t; $SB/seq.sh $BD/base/CMakeFiles/\$t.dir/flags.make -D | tr '\n' ' '; echo; printf '%-6s 老式: ' \$t; $SB/seq.sh $BD/legacyall/CMakeFiles/\$t.dir/flags.make -D | tr '\n' ' '; echo; done"
runc "for t in calc util; do diff <($SB/seq.sh $BD/base/CMakeFiles/\$t.dir/flags.make -D) <($SB/seq.sh $BD/legacyall/CMakeFiles/\$t.dir/flags.make -D) >/dev/null && echo \"\$t 一致\" || echo \"\$t 有差别\"; done"
note "★ 等价判定的正确姿势：对**确实需要这个宏的 target**（calc、util），要盯的是「那条需要的宏两边同值」，"
note "  而不是整行 diff 为 0 —— 上面第二条 diff 不为 0，就是因为老式那边还多捎了两条它不需要的宏。"
note "  对不需要的 target（greet），现代是空、老式是三条，这一项**不要求 diff 为 0**，"
note "  它要求的是第 15 节那种行为差别消失。验收标准要跟着病灶选，不能一条 diff 走天下。"

# ---------------------------------------------------------------------------
banner "17 file(GLOB) 的清单落在哪：只在配置期算一次"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ/variants/glob -B $BD/glob 2>&1 | grep -E 'GLOB|Configuring done'"
runc "ls -1 $PROJ/src/leaf"
runc "cmake --build $BD/glob -j4 >/dev/null && ctest --test-dir $BD/glob -N 2>&1 | tail -4"
note "★ 现在磁盘上有 leaf_a.c、leaf_b.c，名单里有两条用例。这是基线，下面往里加文件。"

# ---------------------------------------------------------------------------
banner "18 真病：新增一个 .c，cmake --build 根本不知道它存在"
# ---------------------------------------------------------------------------
cat > "$PROJ/src/leaf/leaf_c.c" <<'NEWC'
#include <stdio.h>

/* leaf_c：demo 跑到第 18 节时才在沙箱里造出来的新文件。
 * 它存在的唯一目的：证明 file(GLOB) 的清单不会因为它出现而变。 */

int main(void) {
    printf("[leaf_c] 我是新加的，但没人知道\n");
    return 0;
}
NEWC
runc "ls -1 $PROJ/src/leaf"
runc "cmake --build $BD/glob 2>&1 | tail -2"
runc "grep -c leaf_c $BD/glob/CMakeFiles/Makefile2"
note "↑ 0 + 退出码 1：生成的规则里没有这个文件名。grep 报「一个字都没找到」，构建那边一声不吭。"
runc "find $BD/glob -name 'leaf_c*' | wc -l"
runc "ctest --test-dir $BD/glob -N 2>&1 | tail -3"
runc "ctest --test-dir $BD/glob 2>&1 | grep -E 'tests passed'"
note "★ 这一节是本章最安静的一次事故：文件在磁盘上、构建成功、测试全绿、退出码 0。"
note "  但它没有被编译过、没有可执行文件、没有用例。CMake 一个字都没说。"
note "  这就是仓库根那份 CMakeLists 宁可写 56 行清单也不要 GLOB 的原因（第 22 节看原文）。"

# ---------------------------------------------------------------------------
banner "19 唯一的补救（不带 CONFIGURE_DEPENDS 时）：重跑 configure"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ/variants/glob -B $BD/glob 2>&1 | grep -E 'GLOB|Build files'"
runc "cmake --build $BD/glob 2>&1 | grep -E 'leaf_c|Built target'"
runc "ctest --test-dir $BD/glob -N 2>&1 | tail -3"
note "★ 重跑一次 configure，清单才重新算 —— 全部差别就是这一条命令。"
note "  显式清单靠的是「改了文件就得改 CMakeLists」，改了就一定会在版本库的 diff 里留痕；"
note "  GLOB 把这个保障换成了「谁记得要跑上面这一条」，而磁盘上多一个文件不会改动任何文本。"

# ---------------------------------------------------------------------------
banner "20 CONFIGURE_DEPENDS：本机（Unix Makefiles + CMake 4.2.3）到底会不会自动重跑"
# ---------------------------------------------------------------------------
runc "rm -f $PROJ/src/leaf/leaf_c.c && ls -1 $PROJ/src/leaf"
runc "cmake -S $PROJ/variants/glob-depends -B $BD/globdep 2>&1 | grep -E 'CONFIGURE_DEPENDS|Configuring done'"
runc "cmake --build $BD/globdep -j4 >/dev/null && ctest --test-dir $BD/globdep -N 2>&1 | tail -3"
cat > "$PROJ/src/leaf/leaf_c.c" <<'NEWC2'
#include <stdio.h>

/* 第二次造出来：这一次清单带 CONFIGURE_DEPENDS。 */

int main(void) {
    printf("[leaf_c] 我是新加的，CONFIGURE_DEPENDS 能看见吗\n");
    return 0;
}
NEWC2
runc "cmake --build $BD/globdep 2>&1 | tail -16"
runc "ctest --test-dir $BD/globdep -N 2>&1 | tail -3"
note "★ 本机实测：**会**。屏幕上那句 -- GLOB mismatch! + The following files were added: 就是它自己重跑了 configure，"
note "  然后 leaf_c 被编译、被链接、进了用例名单 —— 一条命令都没多敲。"
note "  注意这是**和生成器有关**的行为（CMake 文档明说各生成器支持程度不同），"
note "  本机只有 Unix Makefiles，所以本章只替你担保这一条（其余生成器在「本章没演」清单里）。"

# ---------------------------------------------------------------------------
banner "21 CONFIGURE_DEPENDS 的另一半：删文件、以及它没扫干净的东西"
# ---------------------------------------------------------------------------
runc "rm -f $PROJ/src/leaf/leaf_c.c && cmake --build $BD/globdep 2>&1 | tail -14"
runc "ctest --test-dir $BD/globdep -N 2>&1 | tail -3"
runc "ls -1 $BD/globdep | grep leaf_c"
note "★ 删文件这一半本机也测了：GLOB mismatch 照样报，用例名单回到两条 —— 但它没碰磁盘："
note "  构建目录里那个旧的 leaf_c 可执行文件还在上面这一行里。**名单是对的，目录里有尸体。**"
note "  所以验收要看用例名单（ctest -N），不要看构建目录里有什么文件。"
runc "find $BD/globdep -maxdepth 1 -type f -perm -u+x | sort"
note "↑ 三个可执行文件、却只有两条用例 —— 多出来那个就是尸体。"
runc "cmake --build $BD/glob 2>&1 | tail -4"
note "★ 这一条是 GLOB 的另一半病：leaf_c.c 已经不在磁盘上了，而 $BD/glob 的清单还是上一次配置拍的那张快照，"
note "  于是它照着一条指向空气的规则去编译，当场 fatal error。同一个动作，CONFIGURE_DEPENDS 那份是**安静地把它从清单里摘掉**。"
runc "cmake -S $PROJ/variants/glob -B $BD/glob 2>&1 | grep -E 'GLOB|Build files'"
runc "cmake --build $BD/glob 2>&1 | tail -2"
note "↑ 普通 GLOB 这份只能靠人重跑 configure 才恢复 —— 和第 19 节是同一条命令。"
runc "time cmake --build $BD/globdep >/dev/null"
runc "time cmake --build $BD/glob >/dev/null"
note "↑ 到这一步两份才都是「无事可做」的构建。CONFIGURE_DEPENDS 的代价是每次构建都要重扫一遍目录，"
note "  但这个两文件的小工程扫不扫都只差几毫秒，量不出有意义的数 —— 本章只担保它**不为零**，"
note "  大工程里到底多重，得在你自己的仓库上量（这一条在「本章没演」清单里）。"

# ---------------------------------------------------------------------------
banner "22 接回本仓库：那份 56 节的 CMakeLists 为什么写着「不写 file(GLOB)」"
# ---------------------------------------------------------------------------
DSROOT=$(cd "${DS_ROOT:-$HERE/../..}" 2>/dev/null && pwd)
if [ -f "$DSROOT/CMakeLists.txt" ]; then
    note "只读引用（不 configure、不 build、不写它一个字节）："
    runc_at "$DSROOT" "sed -n '31,36p' CMakeLists.txt"
    runc_at "$DSROOT" "grep -n 'file(GLOB' CMakeLists.txt"
    runc_at "$DSROOT" "grep -cE 'add_executable|add_library' CMakeLists.txt"
else
    note "没找到仓库根 CMakeLists（正常：demo 可以脱离那本教材单独跑）。"
fi
note "★ 那几行注释里最要紧的是最后那句括号：本仓库的 Makefile 用 find 每次执行都重扫，"
note "  而 CMake 的清单只在生成阶段跑一次 —— 时机差别就是这条规矩的全部理由。"
note "  上面第二条命令数出来只有 1 处：全仓库唯一出现 file(GLOB 的地方，就是这条禁止写它的注释。"
note "  第三条数出来是 1 —— 56 个目录的 add_executable 收进了一个函数里，但那 56 个源文件名还是一个一个列的。"

# ---------------------------------------------------------------------------
banner "23 老写法四：无关键字 target_link_libraries —— 命令行一模一样，差别全在属性里"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ/variants/plain-link -B $BD/plain 2>&1 | grep -E 'INTERFACE_LINK'"
runc "cmake -S $PROJ/variants/plain-link -B $BD/plain 2>&1 | grep -ciE 'warning|deprecat'"
note "↑ 0 且退出码 1：本机对无关键字签名**一条警告都不说**（CMake 4.2.3 实测）。"
runc "cmake --build $BD/plain -j4 >/dev/null && for t in use_plain use_priv use_pub; do printf '%-10s ' \$t; cat $BD/plain/CMakeFiles/\$t.dir/link.txt; done"
runc "diff <(sed -e s/use_plain/USEX/g -e s/calc_plain/CALCX/g $BD/plain/CMakeFiles/use_plain.dir/link.txt) <(sed -e s/use_priv/USEX/g -e s/calc_priv/CALCX/g $BD/plain/CMakeFiles/use_priv.dir/link.txt); echo \"[无关键字 vs PRIVATE：link.txt diff 退出码 \$?]\""
note "★ 结论（本机 4.2.3）：无关键字签名**照工作**，而且和 PRIVATE 的 link.txt 逐字符相同。"
note "  也就是说：**命令行 diff 这一把尺子抓不到它** —— 这是本章方法论的边界，必须写清楚。"
note "  差别在上面那三条 message 里：无关键字和 PUBLIC 都写成裸名字 util，只有 PRIVATE 包了一层 \$<LINK_ONLY:util>。"
note "  同一棵构建树里两者行为一样，要到「导出给别人」那天才炸 —— 第 03 章第 11 节那个报错就是它。"
mkdir -p "$SB/mix"
cat > "$SB/mix/CMakeLists.txt" <<'MIX'
# 故意只改一半：前两个依赖不加关键字，第三个想加 —— 老仓库改到一半就是这个样子
cmake_minimum_required(VERSION 3.16)
project(ch07mix LANGUAGES C)
add_library(mm STATIC "${MIXROOT}/util/util.c")
add_executable(m "${MIXROOT}/leaf/leaf_a.c")
target_link_libraries(m mm PRIVATE mm)
MIX
runc "cmake -S $SB/mix -B $BD/mix -DMIXROOT=$PROJ/src 2>&1 | tail -6"
note "★ 同一次调用里混用裸名字和关键字，本机是**硬错误**（不是警告）—— 这是好事：改到一半会停住，不会静默。"

# ---------------------------------------------------------------------------
banner "24 老写法五：全局变量 / CACHE 变量传依赖清单 → 一个 INTERFACE 库"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ/variants/cache-var -B $BD/cv 2>&1 | grep -E 'DEPS'"
note "↑ 第一行是子目录里打出来的：它在 add_subdirectory 之后才设的 LATE_DEPS 是**空的** —— 变量看不见晚设的值。"
runc "cmake --build $BD/cv -j4 >/dev/null && for t in cv_var cv_cache cv_iface; do printf '%-9s ' \$t; cat $BD/cv/sub/CMakeFiles/\$t.dir/link.txt; done"
runc "diff <(sed s/cv_var/CVX/g $BD/cv/sub/CMakeFiles/cv_var.dir/link.txt) <(sed s/cv_iface/CVX/g $BD/cv/sub/CMakeFiles/cv_iface.dir/link.txt); echo \"[全局变量 vs INTERFACE 库：link.txt diff 退出码 \$?]\""
runc "cmake -S $PROJ/variants/cache-var -B $BD/cvlate -DCACHE_DEPS=nope 2>&1 | grep -E 'Configuring done'"
runc "cat $BD/cvlate/sub/CMakeFiles/cv_cache.dir/link.txt"
note "★ 最后两条是这一节的结论：CACHE 变量能被命令行改写（这里改成不存在的 nope），配置照样成功，"
note "  而 link.txt 里就多了一个 -lnope —— 一个拼错的清单值不会让配置失败，它只会安静地进到命令行里。"
note "  INTERFACE 库那条路的清单只有一个名字，要改它必须在 CMakeLists 里改，review 看得见。"

# ---------------------------------------------------------------------------
banner "25 COMPILE_FLAGS vs COMPILE_OPTIONS：覆盖 / 追加 / 带空格的那个参数"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ/variants/comp-flags -B $BD/cf 2>&1 | grep -E 'comp-flags|Configuring done'"
note "↑ 六个 target，两两一对：每对的源码一样、想表达的参数一样，只有写法不同。"
runc "for t in cf_over cf_append; do printf '%-10s ' \$t; grep -m1 '^C_FLAGS' $BD/cf/CMakeFiles/\$t.dir/flags.make | sed 's|^C_FLAGS = ||'; done"
runc "for t in cf_space cf_list; do printf '%-9s ' \$t; grep -m1 '^C_FLAGS' $BD/cf/CMakeFiles/\$t.dir/flags.make | sed 's|^C_FLAGS = ||'; done"
runc "cmake --build $BD/cf --target cf_space 2>&1 | tail -4"
runc "cmake --build $BD/cf --target cf_list 2>&1 | tail -1"
note "★ 两处差别：① 属性是**覆盖**（第二次 set_target_properties 把第一次整个换掉，C_FLAGS 里只剩 CF_B），"
note "  target_compile_options 是**追加**（两条都在）；② 带空格的「一个参数」，属性是原样字符串"
note "  （被拆成两个参数，cf_space 当场编不过），列表会被引号保护（cf_list 照常编过）。"
note "  差别 ① 在 flags.make 里就能看见；差别 ② 必须真跑一次构建才看得出来。"

# ---------------------------------------------------------------------------
banner "26 COMPILE_FLAGS 最贵的一条：生成器表达式原样漏进 Makefile，被 make 自己展开"
# ---------------------------------------------------------------------------
runc "for t in cf_genex cf_list_genex; do printf '%-13s ' \$t; grep -m1 '^C_FLAGS' $BD/cf/CMakeFiles/\$t.dir/flags.make | sed 's|^C_FLAGS = ||'; done"
runc "cmake --build $BD/cf --target cf_list_genex 2>&1 | tail -1"
runc "cmake --build $BD/cf --target cf_genex --verbose 2>&1 | grep -oE -- '-DCF_CFG=[^ ]*'"
runc "cmake --build $BD/cf --target cf_genex 2>&1 | tail -3"
note "★ 上面那行 -DCF_CFG=CMakeFiles/cf_genex.dir/compiler_depend.tsCONFIG> 是本章最硬的一张证据："
note "  COMPILE_FLAGS 里的 \$<CONFIG> **没有被 CMake 展开**，它作为字面量进了生成的 Makefile，"
note "  然后被 make 解释成「第一个依赖」（Makefile 里 \$< 的语义），拼出一个莫名其妙的宏名，编译失败。"
note "  而 COMPILE_OPTIONS 那一条被正常展开：本机是单配置生成器，\$<CONFIG> 展开成空串，"
note "  所以 flags.make 里那一行只剩 -DCF_CFG=（看再上面那条 dump）。"

# ---------------------------------------------------------------------------
banner "27 完整迁移走查 · 步骤 0：抓基线，以及一定绕不过的那条版本行"
# ---------------------------------------------------------------------------
runc "mkdir -p $SB/baseline && $SB/snap.sh $BD/legacyall $SB/baseline/cmdline.txt"
runc "grep -c '^== ' $SB/baseline/cmdline.txt"
runc "grep -c 'link.txt' $SB/baseline/cmdline.txt"
note "★ 这一步是整套配方的地基：**先把改写前的命令行存盘**。没有它，后面每一步的 diff 都没有左边；"
note "  有了它，「改坏了」是一个可判定的事实，而不是一种感觉。"
runc "{ $BD/legacyall/app; echo ---; $BD/legacyall/greet; echo ---; $BD/legacyall/tp/notepad; echo ---; $BD/legacyall/fooprog; } > $SB/baseline/output.txt && cat $SB/baseline/output.txt"
note "↑ 顺手把程序输出也钉一份 —— 行为等价性要用它来对，不是用退出码对。"
runc "cat $PROJ/variants/min-version/CMakeLists.txt | head -12"
runc "cmake -S $PROJ/variants/min-version -B $BD/minver 2>&1 | tail -10"
note "★ 老仓库第一件事就撞它：CMake 4.x 上不兼容 < 3.5 的策略基线，这是**硬错误**，不是警告。"
note "  报错里给了三条出路（改 VERSION / 用 <min>...<max> / 加 -DCMAKE_POLICY_VERSION_MINIMUM=3.5）。"
note "  本章的立场：前两条是迁移，第三条是「先让它跑起来」—— 用它要单独记账，别当成改完了。"

# ---------------------------------------------------------------------------
banner "28 完整迁移走查 · 步骤 1~5：一次只关一个开关，每步一条 diff"
# ---------------------------------------------------------------------------
# 步骤 1 link_directories → 给 .a 绝对路径；步骤 2 include_directories → target_include_directories；
# 步骤 3 add_definitions → target_compile_definitions；步骤 4 file(GLOB) → 写死清单；
# 步骤 5 COMPILE_FLAGS / 无关键字 / 全局变量清单 → COMPILE_OPTIONS / PRIVATE / INTERFACE 库。
# 每一步都是「只把一个开关拨回 OFF」，所以每步的 diff 都只解释一件事。
runc "cmake -S $PROJ -B $BD/s1 -DCH07_LEG_INCLUDE=ON -DCH07_LEG_DEFS=ON -DCH07_LEG_GLOB=ON -DCH07_LEG_FLAGS=ON -DCH07_LEG_PLAIN=ON -DCH07_LEG_CACHEVAR=ON -DCH07_FOO_DIR1=$LIBS/v1 -DCH07_FOO_DIR2=$LIBS/v2 -DCH07_STAMP=7 -DCH07_TRACE=ON >/dev/null && cmake --build $BD/s1 -j4 >/dev/null && echo 步骤1_构建成功"
runc "for t in app greet calc util; do printf '%-8s ' \$t; diff <($SB/seq.sh $BD/legacyall/CMakeFiles/\$t.dir/flags.make -I) <($SB/seq.sh $BD/s1/CMakeFiles/\$t.dir/flags.make -I) >/dev/null && echo '命令行一致' || echo 有差别; done"
runc "diff <($SB/seq.sh $BD/legacyall/CMakeFiles/fooprog.dir/link.txt -L) <($SB/seq.sh $BD/s1/CMakeFiles/fooprog.dir/link.txt -L); echo \"[步骤1 的 -L diff 退出码 \$?]\""
runc "grep -oE '[^ ]*libfoo.a' $BD/s1/CMakeFiles/fooprog.dir/link.txt"
runc "{ $BD/s1/app; echo ---; $BD/s1/greet; echo ---; $BD/s1/tp/notepad; echo ---; $BD/s1/fooprog; } | diff $SB/baseline/output.txt - ; echo \"[步骤1 程序输出 diff 退出码 \$?]\""
note "★ 步骤 1 的验收：-L 序列变了（机制换了必然变），但所有 -I、C_DEFINES、**程序输出**都不该变。"
note "  输出里 fooprog 那行是 1 还是 2，从这一步起变成**写在文件里的一行**（第 09 节里它取决于手指头）。"

runc "cmake -S $PROJ -B $BD/s2 -DCH07_LEG_DEFS=ON -DCH07_LEG_GLOB=ON -DCH07_LEG_FLAGS=ON -DCH07_LEG_PLAIN=ON -DCH07_LEG_CACHEVAR=ON -DCH07_FOO_DIR1=$LIBS/v1 -DCH07_FOO_DIR2=$LIBS/v2 -DCH07_STAMP=7 -DCH07_TRACE=ON >/dev/null && cmake --build $BD/s2 -j4 >/dev/null && echo 步骤2_构建成功"
runc "for t in app greet calc util fooprog leaf_a leaf_b; do printf '%-8s ' \$t; if diff <($SB/seq.sh $BD/s1/CMakeFiles/\$t.dir/flags.make -I) <($SB/seq.sh $BD/s2/CMakeFiles/\$t.dir/flags.make -I) >/dev/null; then echo 一致; else echo 有差别; fi; done"
runc "printf 'notepad    '; diff <($SB/seq.sh $BD/s1/tp/CMakeFiles/notepad.dir/flags.make -I) <($SB/seq.sh $BD/s2/tp/CMakeFiles/notepad.dir/flags.make -I) >/dev/null && echo 一致 || echo 有差别"
runc "{ $BD/s2/app; echo ---; $BD/s2/greet; echo ---; $BD/s2/tp/notepad; echo ---; $BD/s2/fooprog; } | diff $SB/baseline/output.txt - >/dev/null; echo \"[步骤2 程序输出 diff 退出码 \$?]\""
note "步骤 2 里 leaf_a / leaf_b / notepad 是**预期会差**的三行（它们本来就不该有那个 -I）。"

runc "cmake -S $PROJ -B $BD/s3 -DCH07_LEG_GLOB=ON -DCH07_LEG_FLAGS=ON -DCH07_LEG_PLAIN=ON -DCH07_LEG_CACHEVAR=ON -DCH07_FOO_DIR1=$LIBS/v1 -DCH07_FOO_DIR2=$LIBS/v2 -DCH07_STAMP=7 -DCH07_TRACE=ON >/dev/null && cmake --build $BD/s3 -j4 >/dev/null && echo 步骤3_构建成功"
runc "$SB/dump.sh $BD/s2 C_DEFINES"
runc "$SB/dump.sh $BD/s3 C_DEFINES"
runc "{ $BD/s3/app; echo ---; $BD/s3/greet; echo ---; $BD/s3/tp/notepad; echo ---; $BD/s3/fooprog; } | diff $SB/baseline/output.txt -; echo \"[步骤3 程序输出 diff 退出码 \$?]\""
note "★ 步骤 3 是本章唯一一个「行为一定会变」的步骤：宏从全局改成点名之后，greet / notepad 那两行 trace 消失。"
note "  它的验收标准是第 15 节那条：diff 不为 0 是预期的，但要能**逐行说出少掉的是哪两行**。"

runc "cmake -S $PROJ -B $BD/s4 -DCH07_LEG_FLAGS=ON -DCH07_LEG_PLAIN=ON -DCH07_LEG_CACHEVAR=ON -DCH07_FOO_DIR1=$LIBS/v1 -DCH07_FOO_DIR2=$LIBS/v2 -DCH07_STAMP=7 -DCH07_TRACE=ON 2>&1 | grep -E 'leaf 清单'"
runc "cmake -S $PROJ -B $BD/s4 >/dev/null && cmake --build $BD/s4 -j4 >/dev/null && ctest --test-dir $BD/s4 -N 2>&1 | tail -3"
runc "ctest --test-dir $BD/s3 -N 2>&1 | tail -3"
note "步骤 4（GLOB → 清单）的验收是**用例名单条数**，不是命令行 —— GLOB 不改变已有 target 的命令行，"
note "它改变的是「你有没有可能悄悄少一个 target」。第 18 节已经演过它病发时的样子。"

runc "cmake -S $PROJ -B $BD/s5 -DCH07_FOO_DIR1=$LIBS/v1 -DCH07_FOO_DIR2=$LIBS/v2 -DCH07_STAMP=7 -DCH07_TRACE=ON >/dev/null && cmake --build $BD/s5 -j4 >/dev/null && echo 步骤5_构建成功"
runc "for t in calc app greet; do printf '%-6s 老: ' \$t; grep -m1 '^C_FLAGS' $BD/legacyall/CMakeFiles/\$t.dir/flags.make | sed 's|^C_FLAGS = ||'; printf '%-6s 新: ' \$t; grep -m1 '^C_FLAGS' $BD/s5/CMakeFiles/\$t.dir/flags.make | sed 's|^C_FLAGS = ||'; done"
runc "diff <(sed s/app/APX/g $BD/s4/CMakeFiles/app.dir/link.txt) <(sed s/app/APX/g $BD/s5/CMakeFiles/app.dir/link.txt); echo \"[步骤5 app 的 link.txt diff 退出码 \$?]\""
runc "diff <(sed s/greet/GRX/g $BD/s4/CMakeFiles/greet.dir/link.txt) <(sed s/greet/GRX/g $BD/s5/CMakeFiles/greet.dir/link.txt); echo \"[步骤5 greet 的 link.txt diff 退出码 \$?]\""
runc "diff <($SB/seq.sh $BD/s5/CMakeFiles/calc.dir/flags.make -I; $SB/seq.sh $BD/s5/CMakeFiles/calc.dir/flags.make -D) <($SB/seq.sh $BD/base/CMakeFiles/calc.dir/flags.make -I; $SB/seq.sh $BD/base/CMakeFiles/calc.dir/flags.make -D); echo \"[步骤5 之后 vs 第 02 节现代基线 diff 退出码 \$?]\""
note "★ 步骤 5 里无关键字和全局变量清单这两处，相邻两步的 link.txt diff 为 0（第 23 节说过尺子抓不到它们，"
note "  这一步靠的是「改到一半会硬报错」+ 属性）。唯一留下来要记账的是 COMPILE_FLAGS 那版多带的 -DLEG_FLAGS=1，"
note "  它在上面那段 C_FLAGS 对照里看得见 —— 要么保留，要么单独写一行「它为什么可以删」。"

# ---------------------------------------------------------------------------
banner "29 反面：一次改太多，编译照过、测试照绿，只有命令行说了真话"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ -B $BD/after -DCH07_FOO_PICK=2 -DCH07_MODE=DOUBLE -DCH07_STAMP=7 -DCH07_TRACE=ON -DCH07_FOO_DIR1=$LIBS/v1 -DCH07_FOO_DIR2=$LIBS/v2 >/dev/null && cmake --build $BD/after -j4 >/dev/null && echo 改完之后_配置0_构建0"
runc "ctest --test-dir $BD/after 2>&1 | grep -E 'tests passed'"
runc "{ $BD/after/app; echo ---; $BD/after/greet; echo ---; $BD/after/tp/notepad; echo ---; $BD/after/fooprog; } | diff $SB/baseline/output.txt -; echo \"[程序输出 diff 退出码 \$?]\""
runc "for t in calc; do printf '%-6s 老: ' \$t; $SB/seq.sh $BD/legacyall/CMakeFiles/\$t.dir/flags.make -D | tr '\n' ' '; echo; printf '%-6s 新: ' \$t; $SB/seq.sh $BD/after/CMakeFiles/\$t.dir/flags.make -D | tr '\n' ' '; echo; done"
runc "grep -oE '[^ ]*libfoo.a' $BD/after/CMakeFiles/fooprog.dir/link.txt"
note "★ 两处「顺手」：换库路径时挑了另一份 libfoo.a（v1 → v2），以及把 MODE 也顺手统一成 DOUBLE。"
note "  结果是 [calc] 那行从 PLUS 变成 DOUBLE、数字从 5 变成 10、foo_tag 从 1 变成 2，"
note "  而配置 0、构建 0、测试 100%。两处各被抓到一次：flags.make 的 -D 序列，和 link.txt 里那个唯一确定的路径。"
runc "cmake -S $PROJ -B $BD/fixed -DCH07_FOO_PICK=1 -DCH07_MODE=PLUS -DCH07_STAMP=7 -DCH07_TRACE=ON -DCH07_FOO_DIR1=$LIBS/v1 -DCH07_FOO_DIR2=$LIBS/v2 >/dev/null && cmake --build $BD/fixed -j4 >/dev/null && { $BD/fixed/app; echo ---; $BD/fixed/greet; echo ---; $BD/fixed/tp/notepad; echo ---; $BD/fixed/fooprog; } | diff $SB/baseline/output.txt -; echo \"[把两个顺手改回去之后：输出 diff 退出码 \$?]\""
note "↑ 最后一行仍然不为 0：少的正是 greet / notepad 那两行 trace —— 那是步骤 3 的**预期变化**。"
note "  能被逐行说出来的差异才叫差异，说不出来的就是事故 —— 这就是迁移必须一次一步的全部理由。"

# ---------------------------------------------------------------------------
banner "30 本章之后：一页配方 + 自证一个产物都没留在章节目录"
# ---------------------------------------------------------------------------
cat <<'RECIPE'

  迁移顺序配方（每一步：改一处 → 构建 → diff 指定的那本账 → 记一行结论）

   步  改什么                                   兜底证据（本机的物证）
  ----  --------------------------------------  -------------------------------------------------
   0    什么都不改，抓基线                       flags.make + link.txt + 程序输出，三份存盘（第 27 节）
        顺手把 cmake_minimum_required 抬到能过    CMake 4.x 上是硬错误，不是警告（第 27 节）
   1    link_directories → 给 .a 绝对路径        link.txt：-L 序列变、库路径唯一（第 08~12 节）
   2    include_directories → target_*_dirs      -I 序列 diff：需要的 target 必须为 0（第 04~07 节）
   3    add_definitions → target_compile_defs    -D 序列 diff + 程序输出（这步行为**该**变，第 15 节）
   4    file(GLOB) → 写死清单                     用例名单条数 + build.make 源清单（第 17~21 节）
   5    COMPILE_FLAGS / 无关键字 / 全局变量清单    C_FLAGS 序列 + INTERFACE_LINK_LIBRARIES（第 23~26 节）
        → target_compile_options / PRIVATE / INTERFACE 库

  三条纪律
   ① 一次只关一个开关：diff 不为 0 时才有唯一解释（第 29 节那两个「顺手」就是反面）
   ② 命令行 diff 是主证据，但它抓不到无关键字签名（第 23 节实测），那一类要看属性和硬报错
   ③ 行为等价用程序输出对，不用退出码对 —— 老写法的失败模式永远是「编译照过、行为变了」

RECIPE
runc "cmake --version | head -1"
runc_at "$HERE" "find . -name CMakeCache.txt -o -name '*.o' -o -name '*.a' -o -name flags.make -o -name link.txt -o -name leaf_c.c -o -type d -name out | wc -l"
note "↑ 0 = 章节目录里一个构建产物都没留下；上面 30 节写出来的东西全部在沙箱里。"
runc "du -sh $SB"
