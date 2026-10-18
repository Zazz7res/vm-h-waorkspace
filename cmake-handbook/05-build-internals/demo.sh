#!/usr/bin/env bash
# ============================================================================
# 第 05 章《构建的四个旋钮》整章证据
#
#   bash demo.sh             跑完自动删沙箱（默认）
#   bash demo.sh --keep      保留沙箱，自己进去翻
#   bash demo.sh > out.txt   重定向自动关颜色
#
# 本章不讲概念，只做**命令行级取证**：每个旋钮都在沙箱里敲一遍，
# 把 CMake 原话、CMakeCache.txt 里的行、flags.make 里的字符、
# 预处理器 .i 里的文件标记行、nm/size 的数字全部抓下来。
#
#   旋钮一 生成器（-G）                 —— 谁真的在执行编译，换错了是什么错
#   旋钮二 构建类型                     —— -O2/-g 到底落在哪几个字符上
#   旋钮三 缓存                         —— -D/-L/-U/--fresh，改了为什么不生效
#   旋钮四 include 路径与单文件标志     —— -I 顺序、双引号/尖括号、flags.make
#
# 安全说明：
#   * 全程在 mktemp 沙箱里，构建产物一个都不落在仓库目录（第 21 节自证）
#   * 全程不联网；本章不需要任何外部依赖
#   * 只读工具：gcc/size/nm/grep/python3，都不需要 root
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
export LC_ALL=C.UTF-8 LANG=C

if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/cmake05-XXXXXX")
SRC="$SB/proj"               # 章节源码的副本：所有构建都不落在仓库里
BD="$SB/build"               # 构建目录集中放这里，方便第 21 节清点

banner() { printf '\n%s══════ %s %s\n' "$B" "$1" "══════════════════════════════════════$R"; }
note()   { printf '  %s\n' "$1"; }
# 注意：note 里**不要放反引号**，双引号里的反引号会被当成命令替换执行掉。

runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    # pipefail：这样 [退出码] 报的是整条管道的真实成败，不会被 grep 的 0 掩盖
    bash -c "set -o pipefail; $1" 2>&1
    printf '[退出码 %d]\n' "$?"
}
# only <正则> <命令>：只留下感兴趣的行，但把过滤条件一起打在提示行里
only() {
    local pat="$1"; shift
    printf '\n%s$ %s | grep -E "%s"%s\n' "$C" "$*" "$pat" "$R"
    bash -c "$*" 2>&1 | grep -E -- "$pat"
}
# show <正则> <文件>：抓文件里匹配的行（带行号），用于解剖 CMakeCache / flags.make
show() { printf '\n%s$ grep -nE "%s" %s%s\n' "$C" "$1" "$2" "$R"; grep -nE -- "$1" "$2" 2>&1; }
# cflags <构建目录>：把这个目录真正用到的编译标志原样打出来
cflags() { grep -m1 '^C_FLAGS' "$BD/$1/CMakeFiles/app.dir/flags.make" | sed 's/^C_FLAGS = //'; }

fresh_dir() { rm -rf "$BD/$1"; }            # 每次重新配置前先清干净，保证现场是第一条命令

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  源码副本：%s\n  构建目录：%s\n  看完删： rm -rf "%s"\n' \
               "$SB" "$SRC" "$BD" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# ---------------------------------------------------------------------------
banner "00 准备：把章节源码复制进沙箱"
# ---------------------------------------------------------------------------
mkdir -p "$BD"
cp -a "$HERE/." "$SRC"
note "工程本身就 8 个文件（demo.sh 是脚本自己，不算），工程不是重点，命令行才是："
( cd "$SRC" && find . -type f -not -name 'demo.sh' | sort )
note "两份同名 calc.h 的差别只在宏值："
( cd "$SRC" && grep -n 'CALC_WHICH\|CALC_FLAVOR' include/calc.h alt/include/calc.h )

# ---------------------------------------------------------------------------
banner "01 旋钮一·生成器：CMake 只是写配方的人，做饭的是它下面那个工具"
# ---------------------------------------------------------------------------
fresh_dir one
runc "cmake -S $SRC -B $BD/one"
note "其中本工程自己打印的那行 STATUS 报了生成器和构建类型（构建类型是空串，第 07 节展开）。"
show '^CMAKE_GENERATOR' "$BD/one/CMakeCache.txt"
note "生成器名进了缓存 —— 这就埋下了第 03 节\"换生成器必须换目录\"的因。"
runc "cmake --build $BD/one"
runc "$BD/one/app"
only 'Unix Makefiles|Ninja|MSYS|NMake|^\*' "cmake -G \"不存在的生成器\" -S $SRC -B $BD/none"
note "报错顺带把本机可用的生成器全列了；行首的 * 是\"不写 -G 时就选它\"那个。"
note "注意：列表里有 Ninja 这个名字，不代表本机能使 —— 名字是 CMake 带的，程序要自己装。"
runc "command -v ninja; command -v gmake make cmake gcc"

# ---------------------------------------------------------------------------
banner "02 生成器写进了缓存，也写进了产物：顶层 Makefile 只是中间人"
# ---------------------------------------------------------------------------
runc "ls $BD/one"
note "顶层 Makefile 是 CMake 生成的**转发壳**，真编译规则在 CMakeFiles/ 下面。"
only '^[a-zA-Z_.-]+:' "head -20 $BD/one/Makefile"
show 'cmake_check_build_system|CMakeFiles/Makefile2' "$BD/one/Makefile"
note "看点：顶层 Makefile 里既没有 gcc 也没有 cc —— 它只会回调 cmake 和 make，剩下的活全交给子 makefile。"

# ---------------------------------------------------------------------------
banner "03 换生成器撞墙①：同一个目录，从 Unix Makefiles 改 -GNinja"
# ---------------------------------------------------------------------------
runc "cmake -S $SRC -B $BD/one -GNinja"
note "这是**最该记住的一条错误**：它说的不是\"ninja 没装\"，而是\"这个目录以前不是这个生成器\"。"
note "哪怕本机装了 ninja，这句话照样出现 —— CMake 不允许一个二进制目录中途换生成器。"

# ---------------------------------------------------------------------------
banner "04 换生成器撞墙②：全新目录 -GNinja，才是真正的\"工具没装\""
# ---------------------------------------------------------------------------
fresh_dir ninja1
runc "cmake -S $SRC -B $BD/ninja1 -GNinja"
note "两条 Ninja 错误长得像，根因完全不同："
note "  ① 目录已存在且是别的生成器 → Does not match the generator used previously"
note "  ② 全新目录但机器上没有 ninja → was unable to find a build program"
note "② 后面还跟着第二条 CMAKE_C_COMPILER not set —— 那是第一条的**连锁**，不是两个毛病。"

# ---------------------------------------------------------------------------
banner "05 生成器也能用 -D 给：-DCMAKE_GENERATOR 撞的是同一堵墙"
# ---------------------------------------------------------------------------
fresh_dir ninja2
runc "cmake -S $SRC -B $BD/ninja2 -DCMAKE_GENERATOR=Ninja"
note "报错和 -GNinja 一模一样 —— 说明 -G 干的事就是把值写进 CMAKE_GENERATOR 缓存项，两种写法殊途同归。"

# ---------------------------------------------------------------------------
banner "06 删掉 CMakeCache.txt 就能在这个目录里换生成器吗"
# ---------------------------------------------------------------------------
runc "rm -f $BD/ninja1/CMakeCache.txt && cmake -S $SRC -B $BD/ninja1 -GNinja"
note "只删缓存、留着 CMakeFiles/ 目录 —— 报错照旧，因为生成器痕迹不止在缓存里一处。"
note "所以\"删缓存\"不够，得连 CMakeFiles/ 一起删；下一节的 --fresh 就是干这个的。"
runc "cmake -S $SRC -B $BD/ninja1 --fresh -GNinja"
note "--fresh 把缓存和 CMakeFiles 一起清掉再配置。本机没 ninja，所以还是失败，"
note "但失败内容已经从\"不匹配\"变成\"找不到工具\" —— 也就是说\"换生成器\"这一步本身其实**通过**了。"

# ---------------------------------------------------------------------------
banner "07 旋钮二·构建类型：不写 CMAKE_BUILD_TYPE 时，一个优化标志都没有"
# ---------------------------------------------------------------------------
fresh_dir bt-empty
runc "cmake -S $SRC -B $BD/bt-empty"
show 'CMAKE_BUILD_TYPE:STRING' "$BD/bt-empty/CMakeCache.txt"
show '^C_FLAGS|^C_INCLUDES' "$BD/bt-empty/CMakeFiles/app.dir/flags.make"
note "C_FLAGS 后面是**空的**。新手最大的误会就在这：以为默认就是 Debug。"
note "默认是空串，等于 gcc 自己的默认（-O0，且不带 -g）。"

for t in Debug Release RelWithDebInfo MinSizeRel; do
    fresh_dir "bt-$t"
    printf '\n%s$ cmake -S %s -B %s -DCMAKE_BUILD_TYPE=%s%s\n' "$C" "$SRC" "$BD/bt-$t" "$t" "$R"
    cmake -S "$SRC" -B "$BD/bt-$t" -DCMAKE_BUILD_TYPE="$t" >/dev/null 2>&1
    printf 'C_FLAGS(%-14s) = [%s]\n' "$t" "$(cflags "bt-$t")"
done
note "这四串是 CMake 给 GNU 编译器的默认值，configure 时写进缓存，所以缓存里能直接看到键值："
show 'CMAKE_C_FLAGS(_DEBUG|_RELEASE|_RELWITHDEBINFO|_MINSIZEREL)?:STRING' "$BD/bt-Debug/CMakeCache.txt"
note "想给自己的标志又不想动这四串：追加用 string(APPEND CMAKE_C_FLAGS ...)，"
note "更规矩的做法是 target_compile_options（第 02 章讲过），它不会污染整棵树。"

# ---------------------------------------------------------------------------
banner "08 构建类型落到二进制上的样子：size 和 nm 直接作证"
# ---------------------------------------------------------------------------
for d in bt-empty bt-Debug bt-Release bt-RelWithDebInfo bt-MinSizeRel; do
    cmake --build "$BD/$d" >/dev/null 2>&1 || note "[警告] $d 没构建成功"
done
runc "ls -l $BD/bt-Debug/app $BD/bt-Release/app | awk '{print \$5, \$9}'"
note "先把\"文件真的存在\"钉死 —— 因为下一条有个坑。"
runc "for d in bt-empty bt-Debug bt-Release bt-RelWithDebInfo bt-MinSizeRel; do printf '%-16s ' \$d; size -t $BD/\$d/app | tail -1; done"
note "两个反直觉点：① bt-empty 和 bt-Debug 的 text 完全相同（都是 2135）—— 因为 -g 只加调试信息、不改代码，"
note "只有 -O 系列才动 text；② Release 和 RelWithDebInfo 的 text 也一样（2014），差别在有没有 -g。"
note "再对照 -O 三档：Release(-O3) 2014、RelWithDebInfo(-O2) 2014、MinSizeRel(-Os) 1963、Debug(-O0) 2135。"
runc "for d in bt-empty bt-Debug bt-Release bt-MinSizeRel; do printf '%-14s burn=%s calc_add=%s\n' \$d \"\$(nm $BD/\$d/app | grep -c ' burn')\" \"\$(nm $BD/\$d/app | grep -c 'calc_add')\"; done"
note "看点：main.c 里那个 static burn() 在 Debug 里是个真函数（符号在），在 Release 里被内联掉了（符号数 0）。"
note "这就是\"优化级别\"唯一诚实的验证方式：别看 CMake 打印了什么，看符号表里还剩什么。"
runc "size -t $BD/这个目录根本不存在/app 2>/dev/null | tail -1"
note "顺手抓一条**取证工具自己的坑**：文件不存在时 size 照样打一行全 0 的 TOTALS。"
note "它唯一的破绽是退出码 1（上面 [退出码 1] 就是 size 的，不是 tail 的）——"
note "而这只有配 pipefail 才看得见：不加 pipefail，管道报的是 tail 的 0，全 0 就被读成\"优化生效了\"。"
note "所以拿 size/nm 作证之前，先 ls 一次确认产物存在，再看退出码。"

# ---------------------------------------------------------------------------
banner "09 --config 在单配置生成器上被**静默**吃掉"
# ---------------------------------------------------------------------------
runc "cmake --build $BD/bt-empty --config Release"
show 'C_FLAGS' "$BD/bt-empty/CMakeFiles/app.dir/flags.make"
show 'CMAKE_BUILD_TYPE:STRING' "$BD/bt-empty/CMakeCache.txt"
note "上面 --config Release 一个字都没多说什么，构建照做，flags 还是空的、缓存里还是空串。"
note "单配置生成器（Unix Makefiles / Ninja）里构建类型是 configure 时定死的；"
note "--config 只对多配置生成器（Ninja Multi-Config / Xcode / Visual Studio）有意义。"
note "误用 --config 不报错也不生效，只会让你白跑一次 —— 这是本章最阴的一条。"

# ---------------------------------------------------------------------------
banner "10 换编译器：CMake 自己把缓存删了重跑，但不保证你原来的变量还在"
# ---------------------------------------------------------------------------
printf '#!/bin/sh\nexec /usr/bin/gcc "$@"\n' > "$SB/mycc.sh"
chmod +x "$SB/mycc.sh"
fresh_dir cc
runc "cmake -S $SRC -B $BD/cc -DCMAKE_C_COMPILER=$SB/mycc.sh"
note "包一层 shell 脚本当编译器，CMake 照收 —— 它只负责调用，不在乎你调的是不是真 gcc。"
show '^CMAKE_C_COMPILER:' "$BD/cc/CMakeCache.txt"
runc "cmake --build $BD/cc && $BD/cc/app | tail -1"
runc "cmake -S $SRC -B $BD/cc -DALT_FIRST=ON -DCMAKE_C_COMPILER=/usr/bin/cc"
note "\"You have changed variables that require your cache to be deleted.\" 是**提示**不是 FATAL："
note "CMake 当场删缓存、自己重跑一遍 configure。"
note "代价看上面那条 -DALT_FIRST=ON：它在第一轮被吃进缓存，缓存一删就没了，"
note "重跑的那轮并没有再带上它 —— 这就是老项目\"换个编译器配置全乱\"的由来。"
show 'ALT_FIRST' "$BD/cc/CMakeCache.txt"

# ---------------------------------------------------------------------------
banner "11 旋钮三·缓存手术：-D / -L / -LA / -U 各自到底动了什么"
# ---------------------------------------------------------------------------
fresh_dir cache
runc "cmake -S $SRC -B $BD/cache -DALT_FIRST=ON -DMY_TMP='随手塞的一个变量'"
note "第一条就要记住：塞一个工程不认识的变量，CMake 会警告 \"Manually-specified variables were not used by the project\"。"
note "警告归警告，构建照样成功 —— 很多人把这行当成\"拼错了没事\"，CI 里就该把它当错。"
show 'ALT_FIRST|MY_TMP|CMAKE_BUILD_TYPE' "$BD/cache/CMakeCache.txt"
note "MY_TMP 确实在缓存里，类型是 UNINITIALIZED —— 意思是\"没人用过它\"。"
runc "cmake -L $BD/cache"
note "看点一：-L 前面照样跑了一遍 configure 的 STATUS 行 —— **cmake -L 会重新配置**，不是纯读文件。"
note "看点二：列表里没有 MY_TMP。"
only 'MY_TMP|ALT_FIRST|Cache values' "cmake -LA $BD/cache"
note "换 -LA（连 advanced 项一起列）也没有 MY_TMP —— -L/-LA 都只列**有类型的**缓存项，"
note "UNINITIALIZED 的变量只能去 CMakeCache.txt 里 grep。这是 -L 系列最容易被误解的边界。"
runc "cmake -S $SRC -B $BD/cache -UALT_FIRST"
show 'ALT_FIRST' "$BD/cache/CMakeCache.txt"
note "-U 确实把 ALT_FIRST 从缓存里删了，但 option() 在 configure 时又把它**加回来**，值回到默认 OFF。"
note "结论：-U 对 option()/set(... CACHE) 声明的变量只能删一时，不能删一世。想改值就老老实实用 -D。"
runc "cmake -S $SRC -B $BD/cache --fresh"
show 'ALT_FIRST|MY_TMP' "$BD/cache/CMakeCache.txt"
note "--fresh 之后 MY_TMP 真没了（它不在 CMakeLists 里，缓存一删就彻底消失）；"
note "ALT_FIRST 却复活成 OFF（option 兜底）。两个变量的命运差在这儿看得最清楚。"

# ---------------------------------------------------------------------------
banner "12 直接编辑 CMakeCache.txt 也能生效（但目录名会开始骗人）"
# ---------------------------------------------------------------------------
show '^CMAKE_BUILD_TYPE:STRING' "$BD/bt-Debug/CMakeCache.txt"
runc "sed -i 's/CMAKE_BUILD_TYPE:STRING=Debug/CMAKE_BUILD_TYPE:STRING=Release/' $BD/bt-Debug/CMakeCache.txt"
runc "cmake -S $SRC -B $BD/bt-Debug >/dev/null && grep -nE '^C_FLAGS' $BD/bt-Debug/CMakeFiles/app.dir/flags.make"
show '^CMAKE_BUILD_TYPE:STRING' "$BD/bt-Debug/CMakeCache.txt"
note "CMakeCache.txt 就是个\"下次 configure 读回来\"的键值文件，手改确实生效。"
note "但注意现在这个目录**名字叫 bt-Debug，里面装的却是 Release 的产物** —— "
note "靠手改缓存换类型，目录名和产物就开始不一致，这是排查\"为什么我的 Debug 是优化过的\"的经典现场。"

# ---------------------------------------------------------------------------
banner "13 旋钮四·-I 的顺序竞争：同一个头名，谁赢看顺序"
# ---------------------------------------------------------------------------
fresh_dir inc-off
runc "cmake -S $SRC -B $BD/inc-off >/dev/null"
show 'C_INCLUDES' "$BD/inc-off/CMakeFiles/app.dir/flags.make"
cmake --build "$BD/inc-off" >/dev/null 2>&1
runc "$BD/inc-off/app"
fresh_dir inc-on
runc "cmake -S $SRC -B $BD/inc-on -DALT_FIRST=ON >/dev/null"
show 'C_INCLUDES' "$BD/inc-on/CMakeFiles/app.dir/flags.make"
cmake --build "$BD/inc-on" >/dev/null 2>&1
runc "$BD/inc-on/app"
note "两份 calc.h 的宏值不同，程序一跑就知道谁赢；flags.make 里 -I 的顺序换了，赢家就换了。"
note "整条链路没有任何一层报错 —— 头找不到才会报错，找错头**不会**。"

# ---------------------------------------------------------------------------
banner "14 不运行程序也能看到头是哪份：预处理器 .i 里的文件标记行"
# ---------------------------------------------------------------------------
runc "cmake --build $BD/inc-off --target src/calc.i"
runc "grep -aE '^# [0-9]+ \"' $BD/inc-off/CMakeFiles/app.dir/src/calc.c.i | grep -a calc.h"
runc "cmake --build $BD/inc-on --target src/calc.i"
runc "grep -aE '^# [0-9]+ \"' $BD/inc-on/CMakeFiles/app.dir/src/calc.c.i | grep -a calc.h"
note "第 20 节的 --target help 里能看到 src/calc.i、src/calc.s 这类\"派生目标\"，不编对象只出中间产物。"
note ".i 是只跑预处理的结果：每打开一个文件，就留一行「# 行号 \"路径\"」的标记 —— 这是最硬的头解析证据。"
note "宏定义本身在 .i 里看不到（被预处理器消费掉了），能看到的是这个文件标记行。"

# ---------------------------------------------------------------------------
banner "15 双引号 vs 尖括号：src 目录里那份 local.h 抢赢了 -I"
# ---------------------------------------------------------------------------
runc "cmake --build $BD/inc-off --target src/main.i"
runc "grep -aE '^# [0-9]+ \"' $BD/inc-off/CMakeFiles/app.dir/src/main.c.i | grep -aE 'local.h|calc.h'"
runc "$BD/inc-off/app"
note "main.c 里三行 include：\"calc.h\"（src/ 没有 → 按 -I 顺序）、\"local.h\"（src/ 有 → 第一步命中）、"
note "<local.h>（跳过同目录那一步 → 从 -I 拿到 include/ 那份）。"
note "两份 local.h 的**包含宏故意不同名**，否则后一份被防重复包含挡掉，两份都定义了 LOCAL_FLAVOR 也看不出区别。"

# ---------------------------------------------------------------------------
banner "16 老写法的 BEFORE 陷阱：传两个目录，顺序整个反过来"
# ---------------------------------------------------------------------------
fresh_dir legacy
runc "cmake -S $SRC -B $BD/legacy -DUSE_GLOBAL_INCLUDE_DIR=ON >/dev/null"
show 'C_INCLUDES' "$BD/legacy/CMakeFiles/app.dir/flags.make"
cmake --build "$BD/legacy" >/dev/null 2>&1
runc "$BD/legacy/app | head -2"
note "ALT_FIRST=OFF，CMakeLists 里给的顺序明明是 include → alt/include；"
note "但 include_directories(BEFORE 目录1 目录2) 是把**整批反着**插到最前面，结果 alt/include 赢了。"
note "同一份 CMakeLists，只把给头目录的命令从 target 版换成全局 BEFORE 版，程序就跑成了另一份头的行为，还是不报错。"
runc "printf 'include_directories(BEFORE /tmp/A /tmp/B)\n' > $SB/before.cmake && cmake -P $SB/before.cmake"
note "这条是**故意失败**的：-P 脚本模式没有目录概念，include_directories 直接报 command is not scriptable。"
note "所以想验 BEFORE 的反序，只能像上面那样开个真工程，抄不了 cmake -P 这条近路。"

# ---------------------------------------------------------------------------
banner "17 单个源文件的标志落在哪：set_source_files_properties(COMPILE_FLAGS)"
# ---------------------------------------------------------------------------
show 'FROM_SOURCE_PROPERTY' "$BD/inc-off/CMakeFiles/app.dir/flags.make"
runc "grep -nE 'FROM_SOURCE_PROPERTY' $BD/inc-off/CMakeFiles/app.dir/build.make | head -3"
runc "touch $SRC/src/calc.c $SRC/src/main.c && cmake --build $BD/inc-off --verbose 2>&1 | grep -E '^/usr/bin/cc' | head -2"
note "三处互相印证：flags.make 里是一行\"Custom flags\"注释，build.make 里是拼在命令尾巴上的实串，"
note "--verbose 里是展开后的真实命令行（所以要先 touch 让它重编，否则没有编译行可打）。"
note "注意 C_FLAGS 本体没变 —— 单文件标志是加在**那一条命令**上的，不是改全局变量。"

# ---------------------------------------------------------------------------
banner "18 compile_commands.json：编辑器眼里的世界"
# ---------------------------------------------------------------------------
fresh_dir cxx
runc "cmake -S $SRC -B $BD/cxx -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
runc "ls -l $BD/cxx/compile_commands.json"
runc "python3 -c \"import json;d=json.load(open('$BD/cxx/compile_commands.json'));[print(e['file'].split('/')[-1],'|',e['command']) for e in d]\""
note "每条记录三个关键字段：directory（在哪个目录执行）、command（完整编译行）、file（编的是谁）。"
note "clangd / IDE 就靠这个文件复现编译命令；不导出，编辑器只能自己猜 -I，红波浪线一大片。"
note "calc.c 那条尾巴上有 -DFROM_SOURCE_PROPERTY=1，main.c 没有 —— 和第 17 节完全对得上。"
note "只有 Makefile/Ninja 这类生成器认 CMAKE_EXPORT_COMPILE_COMMANDS；VS/Xcode 不管这个。"

# ---------------------------------------------------------------------------
banner "19 构建目录解剖：每个文件是谁、干什么"
# ---------------------------------------------------------------------------
cmake --build "$BD/cxx" >/dev/null 2>&1
runc "find $BD/cxx -maxdepth 2 | sort | head -20"
runc "ls $BD/cxx/CMakeFiles/app.dir"
for f in flags.make build.make link.txt DependInfo.cmake progress.make; do
    printf '\n%s---- CMakeFiles/app.dir/%s（前 6 行）----%s\n' "$B" "$f" "$R"
    head -6 "$BD/cxx/CMakeFiles/app.dir/$f" 2>/dev/null | sed 's/^/    /'
done
runc "ls $BD/cxx/CMakeFiles/app.dir/src"
note "看点：.o 和 .o.d 在 app.dir/src/ 下按源文件的**相对路径**铺开，所以 src/a.c 和 test/a.c 不会撞名。"
note "link.txt 是链接命令的原文，改链接行为时它是第一现场；.o.d 是头依赖快照，"
note "\"改了头为什么没重编 / 没改头为什么重编了\"这两个问题都要在这里查。"

# ---------------------------------------------------------------------------
banner "20 --target help / 不存在的 target / -- 透传底层 make"
# ---------------------------------------------------------------------------
runc "cmake --build $BD/inc-off --target help"
runc "cmake --build $BD/inc-off --target 我要一个不存在的目标"
note "第二条错误是 **gmake 抛的**，不是 CMake 抛的（CMake 只把名字转告底层工具，自己不校验 target 名）。"
runc "cmake --build $BD/inc-off -- -n"
note "-- 后面的东西原封不动给 make：-n 是干跑，只打印要执行的命令、不真编。"
note "这条逃生口很重要：底层工具的一半功能都能从这里进去（-n/-q/--keep-going/-l）。"
runc "cmake --build $BD/inc-off -j 2 && cmake --build $BD/inc-off --clean-first"
note "-j 不给数字时用底层工具的默认并发；也可以用环境变量 CMAKE_BUILD_PARALLEL_LEVEL 定死。"
only 'parallel|CMAKE_BUILD_PARALLEL_LEVEL|--config|clean-first|--target|--  ' "cmake --build $BD/inc-off --help"
note "这份 usage 是 --build 的参数表原文，第 09 节那句\"--config 只对多配置工具有效\"就写在里面。"

# ---------------------------------------------------------------------------
banner "21 收尾自证"
# ---------------------------------------------------------------------------
runc "echo '[沙箱里的构建目录清单，全部在沙箱]'; ls $BD"
runc "echo '[仓库里本章节目录的文件清单 —— 一个构建产物都不该有]'; cd $HERE && find . -type f | sort"
runc "cd $HERE && grep -nE 'https?://' CMakeLists.txt src/*.c src/*.h include/*.h alt/include/*.h || echo '[自证] 本章源码里没有任何 http(s) 地址，全程不联网'"

cat <<EOF

${B}本章结论（README 会逐条配上面的原话输出）：${R}
  1. 生成器名写进缓存；同一个二进制目录不能中途换。换工具报"不匹配"，没装工具报"unable to find a build program"。
  2. CMAKE_BUILD_TYPE 默认是**空串**，一个优化标志都没有；四档真实 flags 在 flags.make 和缓存里都能逐字看到。
  3. --config 在单配置生成器上被静默吃掉：不报错、不生效。
  4. 用 size/nm 取证前必须先 ls 确认产物存在 —— size 对不存在的文件也打全 0 的 TOTALS、退出码 0。
  5. cmake -L 会重新配置；-L/-LA 都不列 UNINITIALIZED 变量；-U 删不掉 option() 兜底的项；--fresh 才是清空重来。
  6. 换编译器时 CMake 自己删缓存重跑，手动 -D 进去的野变量会一起蒸发。
  7. -I 顺序决定同名头谁赢；双引号先搜"包含者所在目录"，尖括号跳过这一步；找错头不报错。
  8. include_directories(BEFORE 多个目录) 会把整批反序 —— 老写法的静默陷阱，cmake -P 里还验不了。
  9. 单文件 COMPILE_FLAGS 在 flags.make / build.make / --verbose / compile_commands.json 四处都能看到。
EOF
