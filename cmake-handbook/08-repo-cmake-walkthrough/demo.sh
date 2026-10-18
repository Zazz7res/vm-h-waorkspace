#!/usr/bin/env bash
# ============================================================================
# 第 08 章《逐行拆解本仓库那份数据结构工程的 CMakeLists.txt》整章证据
#
#   bash demo.sh             跑完自动删沙箱（默认）
#   bash demo.sh --keep      保留沙箱，自己进去接着拨开关
#   bash demo.sh > out.txt   重定向自动关颜色，拿纯文本
#
# 本章只回答一个问题：**仓库根那份 249 行的 CMakeLists.txt，每一行在干什么、
# 少写/换写会发生什么。** 拆法只有一条：能读的地方读磁盘物证（只读），
# 需要"拨开关看后果"的地方一律在 mktemp 沙箱里跑本章的迷你复刻。
#
# 安全说明（硬红线）：
#   * 全程不在仓库根 configure / build / ctest / make —— 那会把 build-cmake/ 里
#     那份唯一的物证改掉。所有实验都在 $SB 里，工程是本章节目录的副本。
#   * 仓库根与 build*/ 一律只读：只用 ls / grep / wc / sed 读，一个字节都不写。
#   * 输出里不出现本机登录名：读仓库根文件时用 runc_at（屏幕上只留目录名 + 相对路径），
#     含绝对路径的行（C_INCLUDES / CMakeCache 的 CACHEFILE_DIR / CTestTestfile 的 add_test）
#     一律在**命令里**用 sed "s|$PWD|<仓库根>|g" 换掉再落地，不是事后编辑捕获。
#   * 不联网、不装东西、不写任何全局配置、不做任何 git 写操作。
#   * 章工程里没有任何名为 main.c 的文件（仓库根 Makefile 会按 main.c 自动发现章节，
#     本章不能给它添假章节）。

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

HERE=$(cd "$(dirname "$0")" && pwd)              # 章节目录
ROOT=$(cd "$HERE/../.." && pwd)                  # 仓库根：只读的那份真文件在这里
SB=$(mktemp -d "${TMPDIR:-/tmp}/cmake08-XXXXXX")
PROJ="$SB/proj"                                  # 章工程的副本（本章的正例地基）
BD="$SB/b"                                       # 所有构建目录的家

banner() { printf '\n%s══════ %s %s\n' "$B" "$1" "════════════════════════════════$R"; }
note()   { printf '  %s\n' "$1"; }
# 注意：note 里**不要放反引号**，双引号里的反引号会被当命令替换执行掉。

runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    # pipefail：[退出码] 报的是整条管道的真实成败，不被 grep/head 的 0 掩盖
    bash -c "set -o pipefail; $1" 2>&1
    local ec=$?          # 必须写成一条语句：local ec; ec=$? 会被 local 自己把 $? 洗成 0
    printf '[退出码 %d]\n' "$ec"
    return 0
}

# runc_at <目录> <命令>：先 cd 进那个目录再跑，屏幕上只留目录名（basename）+ 相对命令。
#   读仓库根那份真文件全靠它：命令行里不出现绝对路径，本机登录名一个都不留。
#   ★ 命令串里如果要 sed 掉路径，写成 $PWD —— 它在子 shell 里才展开成那个目录，
#     屏幕上看到的始终是字面的 $PWD。
runc_at() {
    printf '\n%s$ 〔在 %s 里〕 %s%s\n' "$C" "$(basename "$1")" "$2" "$R"
    ( cd "$1" && bash -c "set -o pipefail; $2" ) 2>&1
    printf '[退出码 %d]\n' "$?"
    return 0
}

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

# 复制一份完整的章工程到 $SB/p/<名字>，供各个"拨开关"实验改造
mkproj() {
    local name="$1"
    rm -rf "$SB/p/$name"
    mkdir -p "$SB/p/$name"
    cp "$HERE/CMakeLists.txt" "$SB/p/$name/"
    cp -r "$HERE/mini" "$HERE/src" "$SB/p/$name/"
}

# ----------------------------------------------------------------------------
banner "00 准备：章节目录（只读）+ 把复刻工程拷进沙箱"
# ----------------------------------------------------------------------------
mkdir -p "$BD" "$SB/p" "$PROJ"
cp "$HERE/CMakeLists.txt" "$PROJ/"
cp -r "$HERE/mini" "$HERE/src" "$PROJ/"
runc "cmake --version | head -1 && gcc --version | head -1 && make --version | head -1"
runc "ls -1 $PROJ"
runc "find $PROJ -name '*.c' | sort"
note "★ 本章节目录里唯一的可执行入口是 demo.sh；上面这些 .c 全部是教材道具，"
note "  没有一份叫 main.c —— 仓库根的 Makefile 会按 <章>/<节>/main.c 自动发现章节，本章不能给它添假章节。"

# ----------------------------------------------------------------------------
banner "01 这份 249 行的文件在这台机器上留下了什么（仓库根，全程只读）"
# ----------------------------------------------------------------------------
runc_at "$ROOT" 'wc -l CMakeLists.txt'
runc_at "$ROOT" 'ls -1 build-cmake'
runc_at "$ROOT" 'ls -1 build-cmake/bin | wc -l && ls -1 build-cmake/bin | head -4'
runc_at "$ROOT" 'ls -d build-cmake/CMakeFiles/*.dir | wc -l'
runc_at "$ROOT" 'grep -c "add_test(" build-cmake/CTestTestfile.cmake'
note "↑ 三处 56：bin 里的可执行文件数、CMakeFiles/*.dir 目录数、CTestTestfile 里的 add_test 条数。"
note "  这三个数是这份文件第 55、71、87 行各自留下的账，能互相对质 —— 本章所有结论都从这种对质出发。"
runc_at "$ROOT" 'grep -E "^(CMAKE_BUILD_TYPE|DS_WITH_ASAN|CMAKE_C_COMPILER|CMAKE_GENERATOR):" build-cmake/CMakeCache.txt'
note "这四行缓存是后面三节的舞台：BUILD_TYPE 是空的（第 21 节）、ASan 是 OFF（第 9、15 节）、"
note "编译器是 /usr/bin/cc（第 5 节）。"
runc_at "$ROOT" 'grep -E "^(CMAKE_CACHEFILE_DIR|CMAKE_HOME_DIRECTORY)" build-cmake/CMakeCache.txt | sed "s|$PWD|<仓库根>|g"'
note "★ 这两行是本章唯一需要现场打码的输出：缓存把源码树和构建树的绝对路径原样记了下来。"
note "  任何把 CMakeCache.txt 贴进文档的人，都会顺手把你的登录名贴进去 —— 所以这里在命令里就换掉。"

# ----------------------------------------------------------------------------
banner "02 两条构建路线并存：根 Makefile 与根 CMakeLists 的口径对质"
# ----------------------------------------------------------------------------
runc_at "$ROOT" 'sed -n "1,8p" Makefile'
runc_at "$ROOT" 'grep -n "set(DS_WARNINGS" CMakeLists.txt'
note "Makefile 的 CFLAGS = -Wall -Wextra -Wshadow -std=c11 -g；DS_WARNINGS 只取前三把尺子。"
note "剩下两样（-std、-g）走了别的路：-std 来自 CMAKE_C_STANDARD（第 6 节），"
note "-g 在 CMake 这条路线上默认**根本没有**（第 21 节）—— 这就是"两条路线同一把尺子"这句注释的真边界。"
runc_at "$ROOT" 'grep -E "^(C_INCLUDES|C_FLAGS)" build-cmake/CMakeFiles/01-array-dynamic-array.dir/flags.make | sed "s|$PWD|<仓库根>|g"'
runc_at "$ROOT" 'ls -1 build | head -4'
runc_at "$ROOT" 'ls -1 build/01-array/dynamic-array'
runc_at "$ROOT" 'ls -1 build-cmake/bin | grep dynamic-array'
note "同一个 01-array/dynamic-array，两条路线两个名字：Makefile 交 build/01-array/dynamic-array/main，"
note "CMake 交 build-cmake/bin/01-array-dynamic-array。目录树形状就是路线的指纹，一眼分得清。"

# ----------------------------------------------------------------------------
banner "03 章工程登场：迷你复刻先跑一遍（后面所有实验的地基）"
# ----------------------------------------------------------------------------
runc "cmake -S $PROJ -B $BD/main 2>&1 | tail -8"
runc "cmake --build $BD/main -j4 2>&1 | tail -4"
runc "ls -1 $BD/main/bin"
runc "ctest --test-dir $BD/main 2>&1 | tail -3"
note "四节全绿，包括那节一直在漏的链表 —— 这是第 15 节的对照组，别急着修它。"
runc "grep -E '^(C_INCLUDES|C_FLAGS)' $BD/main/CMakeFiles/mini-01-array-static-array.dir/flags.make"
runc "cat $BD/main/CMakeFiles/mini-01-array-static-array.dir/link.txt"
note "↑ 和真仓库的 flags.make / link.txt 同一种形状：C_INCLUDES 恰好一条 -I，C_FLAGS 恰好那三把尺子，"
note "  链接行里没有 -O、没有 -g。真仓库那份见第 01 节和第 21 节。"

# ----------------------------------------------------------------------------
banner "04 第 43 行 cmake_minimum_required(VERSION 3.16)：下界管的是策略"
# ----------------------------------------------------------------------------
for v in 31 310 316 range; do
    case "$v" in
        31)   num='3.1'  ;;
        310)  num='3.10' ;;
        316)  num='3.16' ;;
        range) num='3.16...3.28' ;;
    esac
    rm -rf "$SB/p/min$v" "$BD/min$v"
    mkdir -p "$SB/p/min$v"
    printf 'cmake_minimum_required(VERSION %s)\nproject(min%s LANGUAGES C)\n' "$num" "$v" > "$SB/p/min$v/CMakeLists.txt"
done
runc "cmake -S $SB/p/min31 -B $BD/min31 2>&1 | tail -9"
note "★ 本机 cmake 4.2.3 把 3.5 以下的兼容彻底删掉了：3.1 不是警告，是 configure 失败（退出码 1）。"
note "  真文件写 3.16 就躲过了这道闸 —— 这个数字是**版本下界**，不是语法开关。"
runc "cmake -S $SB/p/min310 -B $BD/min310 2>&1 | tail -3"
runc "cmake -S $SB/p/min316 -B $BD/min316 2>&1 | tail -3"
note "3.10 和 3.16 在本机都不吵：configure 安静通过。也就是说'下界写多少'屏幕上看不出来，"
note "它改的是**策略默认值**（同一句代码在不同下界下行为可能不同）。"
runc "cmake --help-command cmake_minimum_required | sed -n '1,10p'"
runc "cmake -S $SB/p/minrange -B $BD/minrange 2>&1 | tail -3"
note "3.16...3.28 这种写法是'我按 3.16 的规矩写，但 3.28 的新策略我也认'——"
note "本仓库没用这个形式，只写了裸下界。"

# ----------------------------------------------------------------------------
banner "05 第 45~48 行 project(... LANGUAGES C)：少写 LANGUAGES 在本机会炸"
# ----------------------------------------------------------------------------
rm -rf "$SB/p/nolangs" "$BD/nolangs"; mkdir -p "$SB/p/nolangs"
printf 'cmake_minimum_required(VERSION 3.16)\nproject(nolangs VERSION 1.0)\n' > "$SB/p/nolangs/CMakeLists.txt"
rm -rf "$SB/p/cxx" "$BD/cxx"; mkdir -p "$SB/p/cxx"
printf 'cmake_minimum_required(VERSION 3.16)\nproject(cxxprobe VERSION 1.0 LANGUAGES CXX)\n' > "$SB/p/cxx/CMakeLists.txt"
runc "cmake -S $SB/p/nolangs -B $BD/nolangs 2>&1 | grep -E 'compiler identification|CMake Error|Configuring incomplete' | head -6"
runc "cmake -S $SB/p/cxx -B $BD/cxx 2>&1 | tail -8"
note "★ 本机没有 g++。project() 不写 LANGUAGES 时默认同时启用 C 和 CXX，于是第 45 行那句 LANGUAGES C"
note "  在这台机器上不是排版，是**能不能 configure 的分界**。真文件那行删掉的后果就是这个。"
runc "grep -E '^(CMAKE_PROJECT_NAME|CMAKE_C_COMPILER|ds_ch08_SOURCE_DIR|ds_ch08_BINARY_DIR)' $BD/main/CMakeCache.txt"
runc_at "$ROOT" 'grep -E "^CMAKE_C_COMPILER:" build-cmake/CMakeCache.txt'
note "project() 落的账：项目名、两个目录变量、以及一整套 CMAKE_C_* 工具链变量。"
note "PROJECT_NAME / PROJECT_VERSION 只在收尾那两行 message 里被读出来（第 22 节）。"

# ----------------------------------------------------------------------------
banner "06 第 51~53 行：命令行里那个 -std=gnu11 到底是谁写的"
# ----------------------------------------------------------------------------
for v in ext_on ext_off req_off nostd; do
    rm -rf "$SB/p/std-$v" "$BD/std-$v"; mkdir -p "$SB/p/std-$v"
    printf 'int main(void){ return 0; }\n' > "$SB/p/std-$v/a.c"
    {
      echo 'cmake_minimum_required(VERSION 3.16)'
      echo 'project(stdprobe LANGUAGES C)'
      case "$v" in
        ext_on)  echo 'set(CMAKE_C_STANDARD 11)'; echo 'set(CMAKE_C_STANDARD_REQUIRED ON)'; echo 'set(CMAKE_C_EXTENSIONS ON)' ;;
        ext_off) echo 'set(CMAKE_C_STANDARD 11)'; echo 'set(CMAKE_C_STANDARD_REQUIRED ON)'; echo 'set(CMAKE_C_EXTENSIONS OFF)' ;;
        req_off) echo 'set(CMAKE_C_STANDARD 11)'; echo 'set(CMAKE_C_STANDARD_REQUIRED OFF)'; echo 'set(CMAKE_C_EXTENSIONS ON)' ;;
        nostd)   echo '# 三行 C_STANDARD 全部删掉' ;;
      esac
      echo 'add_executable(app a.c)'
    } > "$SB/p/std-$v/CMakeLists.txt"
    cmake -S "$SB/p/std-$v" -B "$BD/std-$v" >/dev/null 2>&1
done
runc "grep '^C_FLAGS' $BD/std-ext_on/CMakeFiles/app.dir/flags.make"
runc "grep '^C_FLAGS' $BD/std-ext_off/CMakeFiles/app.dir/flags.make"
runc "grep '^C_FLAGS' $BD/std-req_off/CMakeFiles/app.dir/flags.make"
runc "grep '^C_FLAGS' $BD/std-nostd/CMakeFiles/app.dir/flags.make"
note "四档对质：gnu11 ← 第 51+53 行；EXTENSIONS OFF → c11；REQUIRED 拨成 OFF 本机看不出差别；"
note "三行全删 → C_FLAGS 空 → 编译器自己的标准接管。"
runc "gcc -dM -E -x c /dev/null | grep __STDC_VERSION__"
runc "gcc -dM -E -x c -std=gnu11 /dev/null | grep __STDC_VERSION__"
note "★ 本机 gcc 15 不带 -std 时是 C23（202311L）。所以第 51 行不是装饰：删掉它，这 56 节代码"
note "  就从 C11 世界挪到了 C23 世界 —— 屏幕上不会有任何提示。"

# ----------------------------------------------------------------------------
banner "07 c11 与 gnu11 的真实分界：那句「对应 Makefile 里的 -std=c11」其实不成立"
# ----------------------------------------------------------------------------
runc "cp $PROJ/src/std/strdup-leaf.c $SB/ && gcc -std=c11 -Wall -Wextra -Wshadow $SB/strdup-leaf.c -o $SB/sd-c11 2>&1 | head -7"
note "↑ 根 Makefile 那条路线的标准（-std=c11）：strdup 的声明没端出来，隐式声明在本机直接判 error。"
runc "gcc -std=gnu11 -Wall -Wextra -Wshadow $SB/strdup-leaf.c -o $SB/sd-gnu11 2>&1 | head -7 && $SB/sd-gnu11"
note "↑ CMake 这条路线的标准（-std=gnu11）：同一句代码，安静通过、跑得对。"
runc_at "$ROOT" 'grep -n "std=c11\|C_EXTENSIONS\|CMAKE_C_STANDARD" Makefile CMakeLists.txt'
note "★ 结论要照着屏幕上说：真文件第 53 行那句'我们没刻意用 GNU 扩展'是对的（现在没用到），"
note "  但第 51 行的注释'对应 Makefile 里的 -std=c11'不成立 —— 它对应的是 gnu11，"
note "  两条路线在'POSIX 声明要不要端出来'这一件事上不是同一个标准。"

# ----------------------------------------------------------------------------
banner "08 第 55 行 CMAKE_RUNTIME_OUTPUT_DIRECTORY：56 个产物为什么都躺在 bin/"
# ----------------------------------------------------------------------------
runc_at "$ROOT" 'ls -1 build-cmake/bin | wc -l && ls -1 build-cmake/bin | head -3'
mkproj nobin
sed -i 's|^set(CMAKE_RUNTIME_OUTPUT_DIRECTORY.*|# 实验：CMAKE_RUNTIME_OUTPUT_DIRECTORY 那一行被删掉|' "$SB/p/nobin/CMakeLists.txt"
runc "grep -n 'CMAKE_RUNTIME_OUTPUT_DIRECTORY' $SB/p/nobin/CMakeLists.txt"
note "↑ 把第 55 行换成一句注释：整个工程从此不知道产物该去哪。"
runc "cmake -S $SB/p/nobin -B $BD/nobin >/dev/null 2>&1 && cmake --build $BD/nobin -j4 >/dev/null 2>&1 && ls -1 $BD/nobin"
runc "cat $BD/nobin/CMakeFiles/mini-01-array-static-array.dir/link.txt"
note "★ 没有那一行，产物就散在构建目录根上（link.txt 末尾的 -o 也跟着少了 bin/ 一段）；"
note "  有那一行，56 个可执行文件挤在一个 bin/ 里 —— 第 01 节那条 ls 就是它的账。"
runc "ls -1 $BD/main | head -7"
note "顺带一条：这一行是**项目级**的，写在函数外面才管得住全部 56 个 target；"
note "放进 ds_add_leaf 里也生效，但那是把项目级的事伪装成 per-target 的事。"

# ----------------------------------------------------------------------------
banner "09 第 58 行 option(DS_WITH_ASAN ... OFF)：开关在缓存里的样子"
# ----------------------------------------------------------------------------
runc "grep -E '^DS_WITH_ASAN' $BD/main/CMakeCache.txt"
runc_at "$ROOT" 'grep -E "^DS_WITH_ASAN" build-cmake/CMakeCache.txt'
runc "cmake -L $BD/main 2>&1 | grep -E 'DS_WITH_ASAN'"
note "★ cmake -L 会顺手重跑一遍 configure（第 05 章旋钮三演过），所以屏幕上先冒出回显行。"
runc "cmake -S $PROJ -B $BD/typo -DDS_WITH_ASAN_ON=ON 2>&1 | tail -5"
note "这一条是'拼错开关名'的现场：CMake 会说这个变量没人用，但它**照退 0**。"
note "ASan 开关在本项目里是 OFF 的时候，flags.make 里搜不到 fsanitize："
runc "grep -c fsanitize $BD/main/CMakeFiles/mini-02-linked-list-slist.dir/flags.make"
runc "grep -c fsanitize $BD/typo/CMakeFiles/mini-02-linked-list-slist.dir/flags.make"
note "第二个 0 就是拼错名字的代价：命令成功、构建目录正常、ASan 一个字节都没挂上。"

# ----------------------------------------------------------------------------
banner "10 第 68 行 string(REPLACE \"/\" \"-\" _target)：target 名与 bin/ 里那 56 个文件"
# ----------------------------------------------------------------------------
runc_at "$ROOT" 'grep -m2 "^ds_add_leaf(01-array" CMakeLists.txt'
runc_at "$ROOT" 'ls -1 build-cmake/bin/01-array-dynamic-array'
note "↑ 目录 01-array/dynamic-array → target 01-array-dynamic-array，一行代码的对应关系，"
note "  磁盘上两份物证（清单那两行 + bin 里那个文件）说的是一件事。"
runc "ls -1 $BD/main/bin"
note "★ 本章节目录多了一层 mini/，所以同一行产出 mini-01-array-static-array。"
note "  这正好当场证明：那行 REPLACE 干的就是'把斜杠换成横线'，段数它不管。"
rm -rf "$SB/p/slashname" "$BD/slashname"; mkdir -p "$SB/p/slashname"
printf 'int main(void){ return 0; }\n' > "$SB/p/slashname/a.c"
cat > "$SB/p/slashname/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(slashname LANGUAGES C)
add_executable(01-array/dynamic-array a.c)
EOF
runc "cmake -S $SB/p/slashname -B $BD/slashname 2>&1 | tail -7"
note "这就是真文件第 40~41 行那句'唯一的小麻烦'的原文报错 —— 它不是风格问题，是 CMake 不让。"
note "target 名以数字开头（01-array-...）是合法的，所以 bin/ 里那 56 个文件都带编号，排序天然按章节。"

# ----------------------------------------------------------------------------
banner "11 第 71 行 add_executable(\${_target} \${ARGN})：ARGN 是什么，少传一个会怎样"
# ----------------------------------------------------------------------------
rm -rf "$SB/p/argn" "$BD/argn"; mkdir -p "$SB/p/argn"
cat > "$SB/p/argn/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(argn LANGUAGES C)
function(ds_add_leaf leaf_dir)
    list(LENGTH ARGN _cnt)
    message(STATUS "argv0(第一个参数)=${ARGV0}  ARGN 项数=${_cnt}  总参数个数(ARGC)=${ARGC}")
    foreach(one IN LISTS ARGN)
        message(STATUS "  ARGN 里的一项 => ${one}")
    endforeach()
endfunction()
ds_add_leaf(01-array/dynamic-array 01-array/dynamic-array/dynamic-array.c 01-array/dynamic-array/main.c)
EOF
runc "cmake -S $SB/p/argn -B $BD/argn 2>&1 | grep -E 'argv0|ARGN' | head -6"
note "↑ 真文件第 70 行的注释写的是 ARGVN，实际变量名是 ARGN（ARGVN 根本不存在）；"
note "  注释里这一处笔误不影响构建，因为代码用的是 ARGN —— 逐行读就要连注释一起验。"
note "ARGN = '除第一个之外的全部参数'。真文件里每个调用点的第一个参数是目录，剩下的是该目录的 .c。"
rm -rf "$SB/p/nosrc" "$BD/nosrc"; mkdir -p "$SB/p/nosrc"
printf 'int main(void){ return 0; }\n' > "$SB/p/nosrc/a.c"
cat > "$SB/p/nosrc/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(nosrc LANGUAGES C)
function(ds_add_leaf leaf_dir)
    string(REPLACE "/" "-" _target "${leaf_dir}")
    add_executable(${_target} ${ARGN})
endfunction()
ds_add_leaf(01-array/static-array)
EOF
runc "cmake -S $SB/p/nosrc -B $BD/nosrc 2>&1 | tail -8"
note "★ 只报目录名、忘了列 .c：报的是 No SOURCES given to target，而且**调用栈一路指回函数外面**"
note "  那一行（Call Stack 里能看到 ds_add_leaf）。函数把错误往自己肚子里藏了一点，但藏不住。"
rm -rf "$SB/p/barename" "$BD/barename"; mkdir -p "$SB/p/barename/sub"
printf 'int main(void){ return 0; }\n' > "$SB/p/barename/sub/a.c"
cat > "$SB/p/barename/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(barename LANGUAGES C)
add_executable(bare a.c)
EOF
runc "cmake -S $SB/p/barename -B $BD/barename 2>&1 | grep -m1 -A6 'Cannot find source file'"
note "第 71 行那些路径是相对 CMakeLists 所在目录展开的：写裸文件名 'a.c' 而文件在子目录里，"
note "报的就是 Cannot find source file。真文件把每份 .c 都写成 <章>/<节>/<文件>.c 这种全长路径，"
note "看着啰嗦，其实是把这层'相对谁'写死在纸面上。"

# ----------------------------------------------------------------------------
banner "12 第 75 行 target_include_directories(... PRIVATE)：叶子自包含的强制点"
# ----------------------------------------------------------------------------
mkproj cross
mkdir -p "$SB/p/cross/mini/04-cross"
cp "$HERE/src/cross/crossleaf.c" "$SB/p/cross/mini/04-cross/leaf.c"
sed -i '/^ds_add_leaf(mini\/03-stack/a ds_add_leaf(mini/04-cross mini/04-cross/leaf.c)' "$SB/p/cross/CMakeLists.txt"
runc "grep -n '^ds_add_leaf' $SB/p/cross/CMakeLists.txt"
note "↑ 加第五节 mini/04-cross：它的 leaf.c 只 include 邻居 slist-num.h（那份头在 mini/02-linked-list/slist/ 里）。"
runc "cmake -S $SB/p/cross -B $BD/cross >/dev/null 2>&1 && cmake --build $BD/cross 2>&1 | grep -E 'fatal error|Built target mini-00|slist-num' | head -6"
runc "grep '^C_INCLUDES' $BD/cross/CMakeFiles/mini-04-cross.dir/flags.make"
note "★ 第 75 行给这个 target 只放开它**自己那一节**的目录：C_INCLUDES 恰好一条 -I，"
note "  于是跨目录 include 死在编译期，屏幕上原话是 slist-num.h: No such file or directory。"
note "  这就是真文件第 73~74 行那句注释的物理实现：约定不是靠人自觉，是靠这一行。"
mkproj crossold
mkdir -p "$SB/p/crossold/mini/04-cross"
cp "$HERE/src/cross/crossleaf.c" "$SB/p/crossold/mini/04-cross/leaf.c"
# 反面教材：第 75 行注释掉，函数开头改成一目录级老写法 include_directories（指向邻居目录）
sed -i 's|^    target_include_directories|    # target_include_directories（实验：这一行被注释掉）|' "$SB/p/crossold/CMakeLists.txt"
sed -i '/^function(ds_add_leaf leaf_dir)/a\    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/mini/02-linked-list/slist")' "$SB/p/crossold/CMakeLists.txt"
sed -i '/^ds_add_leaf(mini\/03-stack/a ds_add_leaf(mini/04-cross mini/04-cross/leaf.c)' "$SB/p/crossold/CMakeLists.txt"
runc "sed -n '/^function(ds_add_leaf/,/endfunction/p' $SB/p/crossold/CMakeLists.txt"
note "↑ 反面教材工程长这样：target 版那一行没了，函数里塞进一行目录级 include_directories，指的就是邻居 slist 的目录。"
runc "cmake -S $SB/p/crossold -B $BD/crossold >/dev/null 2>&1 && cmake --build $BD/crossold 2>&1 | tail -3"
runc "ls -1 $BD/crossold/bin | grep cross"
runc "grep '^C_INCLUDES' $BD/crossold/CMakeFiles/mini-00-c-foundation-pointer-basics.dir/flags.make"
note "★ 老写法一路绿灯：抄邻居东西的那节建出来了，而**完全不相干的第一节也吃到了那条 -I**。"
note "  这就是真文件第 26~30 行拒绝目录级命令的理由，现在它在屏幕上：污染是'之后所有 target'。"
note "  （老写法 → 新写法的完整迁移是规划中的第 07 章，该章在本仓库尚未收口。）"

# ----------------------------------------------------------------------------
banner "13 把第 75 行整行删掉，居然照样能编：它的价值是名单，不是能用"
# ----------------------------------------------------------------------------
mkproj noin
sed -i '/^    target_include_directories/d' "$SB/p/noin/CMakeLists.txt"
runc "sed -n '/^function(ds_add_leaf/,/endfunction/p' $SB/p/noin/CMakeLists.txt"
note "↑ 这回是真删：函数体里那一行没了，只剩注释在原地。"
runc "cmake -S $SB/p/noin -B $BD/noin >/dev/null 2>&1 && cmake --build $BD/noin -j4 2>&1 | tail -2"
runc "grep '^C_INCLUDES' $BD/noin/CMakeFiles/mini-01-array-static-array.dir/flags.make"
note "★ 四节全过、C_INCLUDES 一个字都没有。为什么？#include \"sarr.h\" 找得到 —— 双引号形式"
note "  先搜**包含它的那个源文件自己的目录**（第 05 章旋钮四演过这条编译器规则）。"
note "  所以这一行删掉不会立刻炸；它管的是另一件事：把这个 target 的头搜索名单**写成一条且只有一条**。"
runc "grep '^C_INCLUDES' $BD/main/CMakeFiles/mini-01-array-static-array.dir/flags.make"
note "对照第 12 节反面教材：有第 75 行时名单是'本节目录'，用目录级老写法时名单变成'别人的目录'。"
note "换句话说：这一行不是为了让代码能编，是为了让**编辑器和 clangd 看到的名单**和构建一致（第 01 章场景 9）。"

# ----------------------------------------------------------------------------
banner "14 第 77 行 target_compile_options(... PRIVATE \${DS_WARNINGS})"
# ----------------------------------------------------------------------------
for v in warnfull wallonly nowarn; do mkproj "$v"; mkdir -p "$SB/p/$v/mini/05-warn"; cp "$HERE/src/warn/leaf.c" "$SB/p/$v/mini/05-warn/leaf.c"; done
for v in warnfull wallonly nowarn; do
  sed -i '/^ds_add_leaf(mini\/03-stack/a ds_add_leaf(mini/05-warn mini/05-warn/leaf.c)' "$SB/p/$v/CMakeLists.txt"
done
sed -i 's|set(DS_WARNINGS -Wall -Wextra -Wshadow)|set(DS_WARNINGS -Wall)|' "$SB/p/wallonly/CMakeLists.txt"
sed -i 's|    target_compile_options(${_target} PRIVATE ${DS_WARNINGS})|    # 实验：这一行整行删掉|' "$SB/p/nowarn/CMakeLists.txt"
runc "cmake -S $SB/p/warnfull -B $BD/warnfull >/dev/null 2>&1 && cmake --build $BD/warnfull 2>&1 | grep -E 'warning|shadow' | head -4"
runc "grep '^C_FLAGS' $BD/warnfull/CMakeFiles/mini-05-warn.dir/flags.make"
runc "cmake -S $SB/p/wallonly -B $BD/wallonly >/dev/null 2>&1 && cmake --build $BD/wallonly 2>&1 | grep -E 'warning' | grep -c ."
runc "cmake -S $SB/p/nowarn -B $BD/nowarn >/dev/null 2>&1 && cmake --build $BD/nowarn 2>&1 | grep -E 'warning' | grep -c ."
note "★ 三档：全套三把尺子 → 报 shadow；只留 -Wall → 0 条；第 77 行删掉 → 0 条。"
note "  道具 src/warn/leaf.c 是'内层变量遮住外层同名变量'，只有 -Wshadow 看得见 —— "
note "  这一条就是第 61 行那句'和 Makefile 里 CFLAGS 的警告集完全一致'的全部含金量。"
runc "grep -E '^(C_FLAGS|C_INCLUDES)' $BD/nowarn/CMakeFiles/mini-05-warn.dir/flags.make"
note "警告没了，-std=gnu11 还在：第 51~53 行是项目级的，第 77 行是 per-target 的，两件事两条路。"
rm -rf "$SB/p/optbefore" "$BD/optbefore"; mkdir -p "$SB/p/optbefore"
printf 'int main(void){ return 0; }\n' > "$SB/p/optbefore/a.c"
cat > "$SB/p/optbefore/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(optbefore LANGUAGES C)
target_compile_options(early PRIVATE -Wall)
add_executable(early a.c)
EOF
runc "cmake -S $SB/p/optbefore -B $BD/optbefore 2>&1 | tail -6"
note "target 版命令的位置是'必须在 target 存在之后'，写在前面直接报 Cannot specify compile options。"
note "反过来，目录级的 add_compile_options 必须是'写在 add_executable 之前才有效'（第 06 章第 15 节）。"
note "真文件把规则收在函数里、又总是先 add_executable 再 target_*，两个坑都绕开了。"

# ----------------------------------------------------------------------------
banner "15 第 79~84 行 ASan 那四行：编译期和链接期必须一起变"
# ----------------------------------------------------------------------------
runc "cmake -S $PROJ -B $BD/asan -DDS_WITH_ASAN=ON 2>&1 | tail -7"
runc "grep '^C_FLAGS' $BD/asan/CMakeFiles/mini-02-linked-list-slist.dir/flags.make"
runc "cat $BD/asan/CMakeFiles/mini-02-linked-list-slist.dir/link.txt"
note "★ 同一份源码、同一个 target，两处各多了一段：flags.make 里的 -fsanitize=address -g -fno-omit-frame-pointer，"
note "  link.txt 里的 -fsanitize=address -static-libasan。这就是第 06 章第 16 节'两半成对'在真文件形状上的样子。"
note "  对比第 03 节那份没开开关的 flags.make：差别只有 if(DS_WITH_ASAN) 那 4 行，其余一个字没动。"
runc "cmake --build $BD/asan -j4 >/dev/null 2>&1 && ctest --test-dir $BD/asan --output-on-failure 2>&1 | tail -14"
note "名单还是 4 条（第 18 节会单独看它），变的是'通过'的定义：那节漏 12 字节的链表当场红。"

# ----------------------------------------------------------------------------
banner "16 第 82~83 行那句注释（本机不带共享版 asan）在本机不成立"
# ----------------------------------------------------------------------------
runc_at "$ROOT" 'grep -n "libasan\|共享版" CMakeLists.txt'
runc "ls -1 /usr/lib/x86_64-linux-gnu/libasan.so*"
note "★ 共享版 libasan.so.8 就在这台机器的系统目录里。所以'必须静态链接'这个前提是过时的。"
mkproj asanshared
sed -i 's/ -static-libasan//' "$SB/p/asanshared/CMakeLists.txt"
runc "grep -n 'fsanitize=address' $SB/p/asanshared/CMakeLists.txt"
runc "cmake -S $SB/p/asanshared -B $BD/shared -DDS_WITH_ASAN=ON >/dev/null 2>&1 && cmake --build $BD/shared -j4 >/dev/null 2>&1 && ldd $BD/shared/bin/mini-01-array-static-array | grep -E 'asan'"
runc "ldd $BD/asan/bin/mini-01-array-static-array | grep -c asan"
runc "$BD/shared/bin/mini-01-array-static-array"
note "不加 -static-libasan：ldd 里明明白白一行 libasan.so.8 => /usr/lib/...，程序照跑。"
note "加 -static-libasan：ldd 里 asan 出现 0 次，运行时库整个塞进可执行文件里。"
runc "stat -c '%s 字节  %n' $BD/asan/bin/mini-01-array-static-array $BD/shared/bin/mini-01-array-static-array"
note "↑ 代价在上面两个数里：同一个 target，静态版是动态版的 70 多倍。真仓库是 56 个 target，"
note "  开一次 ASan 就是 56 份这个差额（这一句是从上面两个数推的，不是本机实测 56 份的总和）。"
note "那这条注释什么时候成立？'跑构建的机器'和'跑产物的机器'不是同一台、而且目标机器没有"
note "对应版本的 libasan.so.8 时（gcc 大版本一换，asan 的 so 名字就变）。"
note "老实的说法应该是：第 83 行不是'必须'，是'把 asan 运行时打进产物，换机器少一个依赖'。"

# ----------------------------------------------------------------------------
banner "17 第 80~81 行那个 -g：报告里 leaf.c:17 那行字是它给的"
# ----------------------------------------------------------------------------
runc "cd $BD/asan && ./bin/mini-02-linked-list-slist 2>&1 | head -12"
note "↑ 有 -g：Direct leak 的栈帧里能看到 源文件:行号。"
mkproj asannog
sed -i 's/ -g -fno-omit-frame-pointer/ -fno-omit-frame-pointer/' "$SB/p/asannog/CMakeLists.txt"
runc "grep -n 'fsanitize=address' $SB/p/asannog/CMakeLists.txt"
runc "cmake -S $SB/p/asannog -B $BD/asannog -DDS_WITH_ASAN=ON >/dev/null 2>&1 && grep '^C_FLAGS' $BD/asannog/CMakeFiles/mini-02-linked-list-slist.dir/flags.make"
runc "cd $BD/asannog && cmake --build . -j4 >/dev/null 2>&1 && ./bin/mini-02-linked-list-slist 2>&1 | head -12"
note "★ 只把 -g 摘掉：asan 照样装进编译和链接两半、照样报出 12 字节的泄漏，但栈帧变成"
note "  '在 main (可执行文件+偏移) (BuildId: ...)' —— 行号没了。抓到了，指不出在哪行。"
note "第 80 行那句注释'-g 让 ASan 报到具体行号'，是本机实测**成立**的那句。"
note "★ 同一份文件里的三条注释，本机结论各不相同：第 53 行'对应 Makefile 的 -std=c11'不成立（第 7 节）、"
note "  第 82 行'本机必须静态链接'不成立（第 16 节）、第 80 行这句成立（就是上面两屏对照）。"
note "  注释是给人看的承诺，不是机器验过的结论 —— 逐行拆的意思就是逐行对，不对不采信。"
runc "$BD/asan/bin/mini-02-linked-list-slist > $SB/o1.txt 2> $SB/e1.txt ; wc -c $SB/o1.txt $SB/e1.txt"
note "★ 顺手一个真工程里很坑的现象：这一节程序的 stdout 是 **0 字节**。不是它没跑，"
note "  是 LSan 报完泄漏就用非正常路径退出，stdio 缓冲区里那句 slist total=6 根本没来得及落地。"
runc "ASAN_OPTIONS=detect_leaks=0 $BD/asan/bin/mini-02-linked-list-slist"
note "把漏检关掉（asan 还在），那行 stdout 立刻回来了 —— 所以上面那 0 字节是'报泄漏的退出方式'的副作用。"
note "读 ASan 的 ctest 日志时别把'看不到程序输出'当成'程序没执行到'。"
runc_at "$ROOT" 'grep -h "^C_FLAGS" build-cmake/CMakeFiles/01-array-dynamic-array.dir/flags.make'
note "顺带看真仓库不开 ASan 时的 C_FLAGS：一个 -g 都没有。这条链第 21 节合上。"

# ----------------------------------------------------------------------------
banner "18 第 87 行 add_test(NAME smoke_\${_target} COMMAND \${_target})：名单长什么样"
# ----------------------------------------------------------------------------
runc "grep -E 'add_test|BACKTRACE' $BD/main/CTestTestfile.cmake | head -4"
runc "ctest --test-dir $BD/main -N 2>&1 | tail -7"
runc_at "$ROOT" 'grep -c "add_test(" build-cmake/CTestTestfile.cmake && grep -c add_test build-cmake/CTestTestfile.cmake'
runc_at "$ROOT" 'grep -m1 "smoke_01-array-static-array" build-cmake/CTestTestfile.cmake | sed "s|$PWD|<仓库根>|g"'
runc_at "$ROOT" 'grep -m1 "_BACKTRACE_TRIPLES" build-cmake/CTestTestfile.cmake | sed "s|$PWD|<仓库根>|g"'
note "★ 两个数（56 和 112）的差是第 06 章坑清单第 12 条：backtrace 里也写着 add_test 这个词。"
note "  带括号的 56 才是用例数。backtrace 那一串更有意思：它把'哪一行 add_test、哪一行调的函数'"
note "  都记进了名单文件 —— 87 是函数里那行 add_test，97 是清单里第一个调用点。"
note "  测试名 smoke_ 前缀 + target 名，和 target 名不完全一样，这是第 06 章第 4 节说的两种写法之一。"

# ----------------------------------------------------------------------------
banner "19 第 90 行 enable_testing() 的位置：致命的是缺失，不是先后"
# ----------------------------------------------------------------------------
runc_at "$ROOT" 'grep -n "^enable_testing" CMakeLists.txt'
mkproj late
sed -i '/^enable_testing()/d' "$SB/p/late/CMakeLists.txt"
printf '\nenable_testing()\n' >> "$SB/p/late/CMakeLists.txt"
runc "grep -n 'enable_testing' $SB/p/late/CMakeLists.txt"
runc "cmake -S $SB/p/late -B $BD/late >/dev/null 2>&1 && ls -1 $BD/late/CTestTestfile.cmake && ctest --test-dir $BD/late -N 2>&1 | tail -2"
note "★ 把第 90 行整个挪到文件末尾（所有 add_test 之后）：名单照样有 4 条。"
note "  同一份 CMakeLists 目录里，先后顺序不影响结果 —— 第 06 章第 2 节那块哑屏幕的开关是**这行在不在**。"
mkproj nonetest
sed -i '/^enable_testing()/d' "$SB/p/nonetest/CMakeLists.txt"
runc "grep -c 'enable_testing' $SB/p/nonetest/CMakeLists.txt"
runc "cmake -S $SB/p/nonetest -B $BD/nonetest >/dev/null 2>&1 && ls -1 $BD/nonetest/CTestTestfile.cmake ; ctest --test-dir $BD/nonetest 2>&1 | tail -2"
note "整行删掉才是真事故：配置文件根本不生成、ctest 打 No tests were found!!!、退出码 0。"
note "（为什么这里还要管位置：真文件把它写在函数之后、清单之前，读起来是'先立总闸再接线'，"
note "  这个次序的价值在于给人看，以及将来改用 add_subdirectory 分章时它是**必须在前**的那一行。）"

# ----------------------------------------------------------------------------
banner "20 第 92~233 行那 56 个调用点：手工清单 vs file(GLOB)"
# ----------------------------------------------------------------------------
runc_at "$ROOT" 'grep -c "^ds_add_leaf(" CMakeLists.txt && grep -c "ds_add_leaf(" CMakeLists.txt'
note "★ 56 和 58：第二个数把第 37、64 行两条注释也算进去了。取证要挑对模式，别只认 grep -c。"
runc_at "$ROOT" 'find . -mindepth 3 -maxdepth 3 -name main.c -not -path "./build*" | wc -l'
runc_at "$ROOT" 'grep -n "mindepth" Makefile'
note "同一台机器上两种发现方式各给一个数：Makefile 现场 find 到 56 个 main.c，清单里也正好 56 个调用点，"
note "两个 56 相等**不是巧合** —— 是有人在对账。第 35~36 行那两条注释说的就是这个时机差别。"
rm -rf "$SB/p/glob" "$BD/glob"; mkdir -p "$SB/p/glob/leaf"
cp "$HERE/src/glob/mainx.c" "$SB/p/glob/leaf/"
cat > "$SB/p/glob/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.16)
project(globprobe LANGUAGES C)
file(GLOB CH08_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/leaf/*.c")
add_executable(app ${CH08_SRCS})
EOF
runc "cmake -S $SB/p/glob -B $BD/glob >/dev/null 2>&1 && cmake --build $BD/glob 2>&1 | tail -2 && $BD/glob/app"
runc "cp $PROJ/src/glob/ctor.c $SB/p/glob/leaf/ && cmake --build $BD/glob 2>&1 | tail -2"
runc "grep -c 'ctor.c.o' $BD/glob/CMakeFiles/app.dir/link.txt"
runc "$BD/glob/app"
note "★ 文件真的躺在目录里，构建却说 '[100%] Built target app'（什么都没干）："
note "  link.txt 里搜不到它的 .o、跑起来屏幕上没有 ctor.c 那行字。这就是第 31~34 行说的玄学现场。"
runc "cmake -S $SB/p/glob -B $BD/glob >/dev/null 2>&1 && cmake --build $BD/glob 2>&1 | tail -2"
runc "$BD/glob/app"
note "重新 configure 一次就进去了 —— 差别只在'谁被迫重跑清单'。显式清单的代价是啰嗦，"
note "回报是：新增一个 .c 而没写进清单时，报的是 Cannot find source file（第 11 节），当场响。"

# ----------------------------------------------------------------------------
banner "21 CMAKE_BUILD_TYPE 是空的：这一件事在真工程里的后果"
# ----------------------------------------------------------------------------
runc_at "$ROOT" 'grep -h "^C_FLAGS" build-cmake/CMakeFiles/*.dir/flags.make | wc -l'
runc_at "$ROOT" 'grep -h "^C_FLAGS" build-cmake/CMakeFiles/*.dir/flags.make | sort -u'
note "★ 56 份 flags.make，去掉路径之后只有**一种** C_FLAGS：-std=gnu11 -Wall -Wextra -Wshadow。"
runc_at "$ROOT" 'grep -h "^C_FLAGS" build-cmake/CMakeFiles/*.dir/flags.make | grep -c -- "-O"'
runc_at "$ROOT" 'grep -h "^C_FLAGS" build-cmake/CMakeFiles/*.dir/flags.make | grep -c -- "-g"'
note "两个 0：既没有 -O 也没有 -g（退出码 1 是 grep 没匹配，不是命令坏了）。"
note "原因在第 5 节那条缓存：CMAKE_BUILD_TYPE:STRING= 是空的（第 05 章旋钮二讲过默认不是 Debug）。"
runc "cmake -S $PROJ -B $BD/dbg -DCMAKE_BUILD_TYPE=Debug >/dev/null 2>&1 && grep '^C_FLAGS' $BD/dbg/CMakeFiles/mini-01-array-static-array.dir/flags.make"
runc "cmake -S $PROJ -B $BD/rel -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1 && grep '^C_FLAGS' $BD/rel/CMakeFiles/mini-01-array-static-array.dir/flags.make"
runc "cmake -S $PROJ -B $BD/dbasan -DCMAKE_BUILD_TYPE=Debug -DDS_WITH_ASAN=ON >/dev/null 2>&1 && grep '^C_FLAGS' $BD/dbasan/CMakeFiles/mini-02-linked-list-slist.dir/flags.make"
note "三个旋钮拨到位：Debug 给 -g、Release 给 -O3 -DNDEBUG、两个都空就什么都不给。"
note "★ 这条链合上了：真文件里 -g 只有两个来源 —— CMAKE_BUILD_TYPE（文件根本没设默认值）"
note "  或 DS_WITH_ASAN=ON（函数里补一个 -g）。所以'平时无调试信息、一开 ASan 就有行号'不是巧合，"
note "  是第 80 行那一个 -g 顶上来的。"
runc "cmake -S $PROJ -B $BD/asan2 -DDS_WITH_ASAN=ON >/dev/null 2>&1 && cmake --build $BD/asan2 -j4 >/dev/null 2>&1 && cd $BD/asan2 && ctest 2>&1 | tail -5"
note "↑ 顺手把 ASan 档当验收闸门：4 条用例、1 条红、退出码 8 —— 这就是 CI 能拿的结论形状。"
mkproj preset
cat > "$SB/p/preset/CMakePresets.json" <<'EOF'
{
  "version": 3,
  "configurePresets": [
    { "name": "ci", "hidden": true,
      "binaryDir": "${sourceDir}/out/${presetName}",
      "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" } },
    { "name": "ci-asan", "inherits": ["ci"],
      "displayName": "CI 验收：Debug + ASan",
      "cacheVariables": { "DS_WITH_ASAN": "ON" } }
  ],
  "buildPresets": [ { "name": "ci-asan", "configurePreset": "ci-asan" } ],
  "testPresets": [ { "name": "ci-asan", "configurePreset": "ci-asan",
                     "output": { "outputOnFailure": true } } ]
}
EOF
runc "cd $SB/p/preset && cmake --preset ci-asan 2>&1 | tail -4"
runc "cd $SB/p/preset && cmake --build --preset ci-asan >/dev/null 2>&1 && ctest --preset ci-asan 2>&1 | tail -5"
runc "grep '^C_FLAGS' $SB/p/preset/out/ci-asan/CMakeFiles/mini-01-array-static-array.dir/flags.make"
note "★ 上面这三条是**本章沙箱里实测**的验收形状：把 BUILD_TYPE 和 ASan 固化进 preset，"
note "  命令行里就不该再有人手敲 -D（第 06 章第 21~23 节）。"
note "  至于'该不该把它落进仓库根'—— 那是给作者的决定，本章只演示形状；真仓库现在没有 CMakePresets.json（第 24 节核对）。"

# ----------------------------------------------------------------------------
banner "22 第 235~249 行收尾那段 message：那个数字其实会把库也数进去"
# ----------------------------------------------------------------------------
runc_at "$ROOT" 'grep -n "BUILDSYSTEM_TARGETS\|list(LENGTH\|_n_targets" CMakeLists.txt'
runc_at "$ROOT" 'grep -c "^ds_add_leaf(" CMakeLists.txt && ls -1 build-cmake/bin | wc -l'
note "真文件里'可执行 target 数量'那句之所以可信，是因为这份文件**只造可执行文件**。"
mkproj lib
sed -i '/^ds_add_leaf(mini\/03-stack/a add_library(numops STATIC src/lib/numops.c)' "$SB/p/lib/CMakeLists.txt"
runc "grep -n 'add_library\|^add_test' $SB/p/lib/CMakeLists.txt"
runc "cmake -S $SB/p/lib -B $BD/lib 2>&1 | grep -E 'target 数量|警告集|ASan|产物目录'"
runc "ctest --test-dir $BD/lib -N 2>&1 | tail -2"
note "★ 加了一个静态库之后：回显说 5，用例还是 4、产物还是 4 个可执行文件。"
note "  BUILDSYSTEM_TARGETS 是'本目录里所有构建期 target'，可执行/静态库/共享库/INTERFACE 库不分家。"
note "  所以这一行的正确读法是：'这个数字只在这份文件不造库的时候等于可执行文件数'。"
runc "cmake -S $PROJ -B $BD/echo 2>&1 | tail -7"
runc "cmake -S $PROJ -B $BD/echo2 -DDS_WITH_ASAN=ON 2>&1 | grep -E 'ASan|产物目录'"
note "这四行回显不是装饰：它们是**本次 configure 的回执**，和第 01 节的缓存、第 15 节的 flags.make"
note "互相同一条信息。回显里那个'警告集：-Wall;-Wextra;-Wshadow'用分号连，是因为它是一个列表变量。"

# ----------------------------------------------------------------------------
banner "23 第 20~21 行那句'只编某一节'：--target 与 Makefile 路线的 one DIR="
# ----------------------------------------------------------------------------
runc "rm -rf $BD/one && cmake -S $PROJ -B $BD/one >/dev/null 2>&1 && cmake --build $BD/one --target mini-01-array-static-array 2>&1 | tail -3 && ls -1 $BD/one/bin"
runc "cmake --build $BD/one --target 01-array-static-array 2>&1 | tail -3"
note "★ 第二个名字是本章节目录少写了 mini- 前缀的样子（真仓库里 01-array-dynamic-array 就是对的）。"
note "  报错来自底层 make：No rule to make target '01-array-static-array'，退出码 2 —— CMake 不猜你想编哪个。"
runc_at "$ROOT" 'sed -n "20,21p" CMakeLists.txt'
runc_at "$ROOT" 'grep -n "make one DIR" CMakeLists.txt Makefile | head -3'
note "两条路线各有一句'只做一个'：CMake 用 target 名，Makefile 用目录名 —— 第 02 节那两个名字在这里对上号。"

# ----------------------------------------------------------------------------
banner "24 一页纸：这份文件的构造清单，以及本章的自证"
# ----------------------------------------------------------------------------
cat <<'CH08TABLE'

  行段            在做什么                        少写/换写会怎样（本章哪节演过）
  -------------  ------------------------------  ------------------------------------
  43             cmake_minimum_required 3.16     写 3.1 → configure 失败（S4）
  45-48          project(... LANGUAGES C)        省掉 LANGUAGES → 本机找 CXX 炸（S5）
  51-53          C_STANDARD / REQUIRED / EXTENS  删三行 → 无 -std → 本机落到 C23（S6）
                 11                              EXTENSIONS OFF → -std=c11（S6、S7）
  55             RUNTIME_OUTPUT_DIRECTORY/bin    删掉 → 56 个产物散在构建根（S8）
  58             option(DS_WITH_ASAN OFF)        名字拼错 → 静默什么也没开（S9）
  61             set(DS_WARNINGS 三把尺子)        删掉 → -Wshadow 那类问题全绿（S14）
  68             string(REPLACE "/" "-")         不换 → target 名非法（S10）
  71             add_executable(${t} ${ARGN})    ARGN 空 → No SOURCES（S11）
  75             target_include_directories      跨节 include 的强制点（S12）；
                 (PRIVATE 本节目录)               删掉却照样能编（S13）
  77             target_compile_options PRIVATE  写在 add_executable 前 → 报错（S14）
  79-81          ASan 编译期 + -g + 帧指针        没 -g → 报告没有行号（S17）
  83             ASan 链接期 + static-libasan     两半缺一半停在链接（S15）；
                                                  本机不需要 static（S16）
  87             add_test(smoke_ + target)       名单文件里那 56 条（S18）
  90             enable_testing()                整行缺失 → 哑屏幕退 0（S19）
  92-233         56 个手工调用点                  换成 GLOB → 新文件静默不参与（S20）
  238-249        回显：target 数 / 警告集 / ASan   数字含库，不等于可执行数（S22）
CH08TABLE
runc_at "$ROOT" 'ls -1 build-cmake/bin | wc -l && grep -c "^ds_add_leaf(" CMakeLists.txt && grep -c "add_test(" build-cmake/CTestTestfile.cmake'
note "56 / 56 / 56：三个口径互相对质，仓库根那棵树本章只读了它没动它。"
runc_at "$ROOT" 'ls -1 CMakePresets.json 2>&1'
note "第 21 节说'真仓库现在没有 CMakePresets.json'，这就是那句话的证据。"
runc_at "$HERE" "find . -name CMakeCache.txt -o -name '*.o' -o -name CTestTestfile.cmake -o -type d -name bin -o -type d -name out | wc -l"
note "↑ 0 = 章节目录里没有任何构建产物；本章 24 节写出来的东西全在沙箱。"
runc "du -sh $SB"

printf '\n%s全章跑完%s  共 24 节；沙箱：%s%s\n' "$B" "$R" "$SB" "$R"
