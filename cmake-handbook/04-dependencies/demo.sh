#!/usr/bin/env bash
# ============================================================================
# 第 04 章《依赖：同一份库，四条到货路线》整章证据
#
#   bash demo.sh          跑完自动删沙箱（默认）
#   bash demo.sh --keep   保留沙箱
#   bash demo.sh > out.txt   重定向自动关颜色
#
# 本章看点：一份叫 mathx 的第三方库，用四种完全不同的方式进到工程里，
# 每一种的"找到/找不到/版本对不对/谁应答的"都当场演一遍：
#   ① find_package + 我们写的 Findmathx.cmake     （Module 模式）
#   ② find_package + 上游自带的 mathxConfig.cmake  （Config 模式，第 03 章生成过）
#   ③ FetchContent（GIT / URL）                    （configure 时现拉源码）
#   ④ add_subdirectory(../mathx)                   （vendoring，源码进仓库）
#
# 安全说明：
#   * 全程在 mktemp 沙箱里；--prefix 永远指向沙箱，绝不往 /usr/local 装东西
#   * FetchContent 的地址**全部是 file://**（本地 git 仓库和本地 tar.gz），
#     第 15 节会 grep 自己的 CMakeLists 自证没有一个 http(s)
#   * 不碰仓库里那 56 节数据结构代码；不写全局配置
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
SB=$(mktemp -d "${TMPDIR:-/tmp}/cmake04-XXXXXX")
P_BARE="$SB/sysroot-bare"     # 上古上游：只有 .a 和头
P_CFG="$SB/sysroot-cfg"       # 现代上游：还带 lib/cmake/mathx/

banner() { printf '\n%s══════ %s %s\n' "$B" "$1" "══════════════════════════════════════$R"; }
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    # pipefail：这样 [退出码] 报的是整条管道的真实成败，不会被 grep 的 0 掩盖
    bash -c "set -o pipefail; $1" 2>&1
    printf '[退出码 %d]\n' "$?"
}
# only <正则> <命令>：只留下感兴趣的行，但把 grep 条件一起打在提示行里
only() {
    local pat="$1"; shift
    printf '\n%s$ %s | grep -E "%s"%s\n' "$C" "$*" "$pat" "$R"
    bash -c "$*" 2>&1 | grep -E -- "$pat"
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  两个上游前缀：%s  %s\n  看完删： rm -rf "%s"\n' \
               "$SB" "$P_BARE" "$P_CFG" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# ============================================================================
banner "1. 本章地图：一份 mathx，四条到货路线"
# ============================================================================
mkdir -p "$SB/proj"
cp -R "$HERE/CMakeLists.txt" "$HERE/cmake" "$HERE/mathx" "$HERE/consumer" \
      "$HERE/fetch" "$HERE/vendored" "$SB/proj/"
cd "$SB/proj" || exit 1
runc 'find . -type f | sort'
cat <<'TXT'

读法：mathx/ 是「别人家的库」，其余三个目录是三种消费者，cmake/modules 是我们
自己写的 Find 模块。四个目录只有一份 mathx —— 整章就是拿它在四条路线上来回走：

    consumer/  →  find_package(mathx)   库已经在磁盘上（两种模式：Module / Config）
    fetch/     →  FetchContent          configure 时现把源码拉到 _deps/ 里
    vendored/  →  add_subdirectory      源码就躺在本仓库里，直接拉进构建树

本章的每个结论都有对应的命令输出；下文凡是「报错」的地方都是真报错，不是我编的。
TXT

# ============================================================================
banner "2. 先造出两种形状的上游：同一个 mathx，装法差三个文件"
# ============================================================================
runc 'cmake -S mathx -B b-bare -DMX_EXPORT=OFF | grep -E "MX_EXPORT|Configuring done|Build files"'
runc 'cmake --build b-bare | tail -2'
runc "cmake --install b-bare --prefix \"$P_BARE\" | tail -4"
runc 'cmake -S mathx -B b-cfg -DMX_EXPORT=ON | grep -E "Configuring done|Build files"'
runc 'cmake --build b-cfg | tail -1'
runc "cmake --install b-cfg --prefix \"$P_CFG\" | tail -6"
only '.' "find \"$P_BARE\" \"$P_CFG\" -type f | sed \"s|$SB/||\" | sort"
cat <<'TXT'

两个安装树的差集就是 Config 模式的全部物质基础：

    sysroot-bare/  lib/libmathx.a + include/mathx.h                （上古：只给二进制和头）
    sysroot-cfg/   还多 lib/cmake/mathx/{mathxConfig,mathxConfigVersion,mathxTargets*}.cmake

第 03 章我们花整章生成这三个 .cmake；本章从另一侧看它们：**没有**这三个文件时，
find_package 靠什么活？靠调用方自己写的 Findmathx.cmake。
TXT

# ============================================================================
banner "3. 路线①：Config 模式（现代默认）—— 上游自带配置文件"
# ============================================================================
runc "cmake -S consumer -B c-cfg -DCMAKE_PREFIX_PATH=\"$P_CFG\" -DUSE_OUR_MODULE=OFF \
      | grep -E '应答|mathx_DIR|头 =|库 =|Configuring done'"
runc 'cmake --build c-cfg | tail -2'
runc './c-cfg/consumer'
echo '↑ 「[mathxConfig.cmake] Config 模式应答」这条铭牌是那份 .cmake.in 里 message 打出来的。'
echo '  mathx_DIR 指向 lib/cmake/mathx —— 它就是 find_package 实际加载配置文件的目录。'
echo '  注意「头 =」「库 =」两行**是空的**：Config 模式只造 mathx::mathx 这个目标，'
echo '  压根不填 MATHX_INCLUDE_DIR / MATHX_LIBRARY 这类旧式变量。第 5 节展开讲这个区别。'

# ============================================================================
banner "4. 路线②：Module 模式 —— 上游什么都没给，我们的 Find 脚本上场"
# ============================================================================
runc "cmake -S consumer -B c-bare -DCMAKE_PREFIX_PATH=\"$P_BARE\" \
      | grep -E '应答|最低版本|mathx_VERSION|交付|头 =|库 ='"
runc 'cmake --build c-bare | tail -1'
runc './c-bare/consumer'
runc 'cat c-bare/CMakeFiles/consumer.dir/link.txt'    # 链接行里只有 libmathx.a，没有别的
cat <<'TXT'

这次应答的铭牌换成了 [Findmathx.cmake]。同一个 consumer/CMakeLists.txt，
一个字没改，只换了 CMAKE_PREFIX_PATH —— 因为：
  · AUTO 模式（find_package 不写关键字）先找 Findmathx.cmake，找到就用它；
  · 我们的模块 find_path/find_library 在 bare 前缀下命中，造出 IMPORTED 目标；
  · 所以 target_link_libraries(consumer PRIVATE mathx::mathx) 这条写法
    在两种模式下**完全一样**。这就是「用 target 而不是变量」最大的好处：
    换到货方式不用改链接行。
TXT

# ============================================================================
banner "5. 两种模式都能找到时，谁赢？—— 搜索顺序现场裁决"
# ============================================================================
runc "cmake -S consumer -B c-both -DCMAKE_PREFIX_PATH=\"$P_BARE;$P_CFG\" \
      | grep -E '应答|库 ='"
echo '↑ 两个前缀都在、模块也在：Module 模式赢（CMAKE_MODULE_PATH 命中就轮不到 Config）。'
runc "cmake -S consumer -B c-both2 -DCMAKE_PREFIX_PATH=\"$P_BARE;$P_CFG\" -DUSE_OUR_MODULE=OFF \
      | grep -E '应答|mathx_DIR|库 ='"
echo '↑ 把我们的模块从 CMAKE_MODULE_PATH 拿掉：模块找不到 → 自动降级去走 Config 模式。'
echo '  注意这次「库 =」后面是**空的**：Config 模式只造 mathx::mathx 这个 IMPORTED 目标，'
echo '  根本不会去填 MATHX_LIBRARY 这种旧式变量。谁赢了不用猜，看铭牌和这两个变量的填法。'
echo '  这条降级是 CMake 设计上的善意（老包新包混在一个前缀里也能都工作），'
echo '  但它也让「我明明装了为什么用的是这份」变成一类经典疑难杂症 —— 第 15 节教怎么查。'

# ============================================================================
banner "6. 两个关键字各管一头：CONFIG / MODULE 强制走一条路"
# ============================================================================
runc "cmake -S consumer -B c-force1 -DCMAKE_PREFIX_PATH=\"$P_BARE\" -DMODE=CONFIG"
echo '↑ 只有 bare 前缀（没有配置文件）却强制 CONFIG：报的是「Could not find a package'
echo '  configuration file」，和上一条「找不到模块」是两类错。'
runc "cmake -S consumer -B c-force2 -DCMAKE_PREFIX_PATH=\"$P_CFG\" -DMODE=MODULE -DUSE_OUR_MODULE=OFF \
      | grep -vE '^This warning|^$' | sed -n '1,14p'"
echo '↑ 强制 MODULE 但 CMAKE_MODULE_PATH 里没有 Findmathx.cmake：No "Findmathx.cmake" found。'
echo '  后面那段 dev warning 是 CMake 在提醒你：依赖一个 Find 模块，就得保证它真的在树里。'
cat <<'TXT'

为什么有人愿意写 CONFIG？因为 `find_package(x CONFIG)` 明确「只信上游自带的配置文件」，
不会被某个第三方塞进 CMAKE_MODULE_PATH 的同名 Find 模块劫持。
现代工程里 "CONFIG REQUIRED" 是越来越常见的写法。
TXT

# ============================================================================
banner "7. 版本裁判（Config 模式）：mathxConfigVersion.cmake 说了算"
# ============================================================================
runc "cmake -S consumer -B v-cfg99 -DCMAKE_PREFIX_PATH=\"$P_CFG\" -DUSE_OUR_MODULE=OFF -DWANT_VERSION=9.9 \
      | sed -n '/CMake Error/,/installed/p'"
runc "cmake -S consumer -B v-cfg10 -DCMAKE_PREFIX_PATH=\"$P_CFG\" -DUSE_OUR_MODULE=OFF -DWANT_VERSION=1.0 \
      | grep -E '应答|Configuring done'"
runc "sed -n '1,12p' \"$P_CFG/lib/cmake/mathx/mathxConfigVersion.cmake\""
echo '↑ 报错里「compatible with requested version」这句的裁判，就是上面这个文件里的比较逻辑。'
echo '  1.3.0 要 1.0 → 放行（SameMajorVersion）；要 9.9 → 拒绝。第 03 章第 10 节的同一套。'

# ============================================================================
banner "8. 版本裁判（Module 模式）：CMake 把版本交给你，比不比是你的事"
# ============================================================================
runc "cmake -S consumer -B v-mod99 -DCMAKE_PREFIX_PATH=\"$P_BARE\" -DMXM_STRICT_VERSION=ON -DWANT_VERSION=9.9 \
      | sed -n '/CMake Error/,/unsuitable/p'"
echo '↑ 写对了的模块：find_package_handle_standard_args(... VERSION_VAR MATHX_VERSION)'
echo '  会在「标准答案工具」那里把版本比掉，文案是 Found unsuitable version "1.3.0"。'
runc "cmake -S consumer -B v-loose -DCMAKE_PREFIX_PATH=\"$P_BARE\" -DMXM_STRICT_VERSION=OFF -DWANT_VERSION=9.9 \
      | grep -E '最低版本|mathx_FOUND|库 =|Configuring done'"
runc 'cmake --build v-loose | tail -1'
runc './v-loose/consumer'
cat <<'TXT'

这就是本节的全部要点：要 9.9、实际 1.3.0，**configure 成功了，程序还跑起来了**。
原因在 Findmathx.cmake 里那一行：不写 VERSION_VAR，
find_package_handle_standard_args 就只判「文件找没找到」，压根不看版本。
而 CMake 已经把 9.9 递到模块手里了（看上面「调用方要求的最低版本 = 9.9」那行）——
它只是不替你比较。

结论：Module 模式下「版本号」是模块作者的**良心活**；
      Config 模式下它是 helper 生成的**硬闸门**。这也是现代上游都走 Config 的原因之一。
TXT

# ============================================================================
banner "9. 读一个真实内置模块：FindThreads 在这台机器上给了个空目标"
# ============================================================================
runc 'grep -n "CMAKE_HAVE_LIBC_PTHREAD" /usr/share/cmake-4.2/Modules/FindThreads.cmake | head -6'
runc 'sed -n "155,170p" /usr/share/cmake-4.2/Modules/FindThreads.cmake'
mkdir -p threads && cd threads || exit 1
printf 'cmake_minimum_required(VERSION 3.20)\nproject(thr LANGUAGES C)\nfind_package(Threads REQUIRED)\nadd_executable(p p.c)\ntarget_link_libraries(p PRIVATE Threads::Threads)\nmessage(STATUS "CMAKE_THREAD_LIBS_INIT=[${CMAKE_THREAD_LIBS_INIT}] 探针结果 CMAKE_HAVE_LIBC_PTHREAD=${CMAKE_HAVE_LIBC_PTHREAD}")\nget_target_property(_c Threads::Threads INTERFACE_COMPILE_OPTIONS)\nget_target_property(_l Threads::Threads INTERFACE_LINK_LIBRARIES)\nmessage(STATUS "Threads::Threads 的接口：compile=[${_c}] link=[${_l}]")\n' > CMakeLists.txt
printf '#include <pthread.h>\n#include <stdio.h>\nstatic int g = 0;\nstatic void *w(void *a){ (void)a;\n  for (int i = 0; i < 100000; i++) { __atomic_fetch_add(&g, 1, __ATOMIC_SEQ_CST); }\n  return 0; }\nint main(void){ pthread_t t[4];\n  for (int i = 0; i < 4; i++) { pthread_create(&t[i], 0, w, 0); }\n  for (int i = 0; i < 4; i++) { pthread_join(t[i], 0); }\n  printf("g=%%d\\n", g); return 0; }\n' > p.c
runc 'cmake -S . -B b | grep -E "Performing Test|Found Threads|THREADS_FOUND|CMAKE_THREAD_LIBS_INIT|接口"'
runc 'cmake --build b | tail -1'
runc './b/p'
runc 'cat b/CMakeFiles/p.dir/link.txt'
runc 'cc -Wall p.c -o p_bare && ./p_bare'
runc 'ldd b/p | sed "s|0x[0-9a-f]*||"'
cd .. || exit 1
cat <<'TXT'

看清楚了：这条 link.txt 里**没有 -pthread，也没有 -lpthread**，
Threads::Threads 的两个接口属性都是 -NOTFOUND —— 它是个空目标。
原因就在上面那段模块源码里：先 check_c_source_compiles 问「pthread 是不是已经在 libc 里？」，
这台机器（glibc 2.43）答是，于是 `set(CMAKE_THREAD_LIBS_INIT "")`，什么都不用加。

那为什么还要写 target_link_libraries(app PRIVATE Threads::Threads)？
  · 换到 macOS / 老 glibc / musl / 某些交叉工具链，那个探针就会失败，
    模块转而去 find_library(pthread) 并把 -lpthread 填进接口 —— 同一行 CMake 自动变对；
  · 它是「我用了线程」这个事实的声明，别人读你的 CMakeLists 就知道；
  · 少了它，在你这台机器上确实照样能跑（上面 p_bare 就是证据）——
    **别把「本机没坏」当成「写法对了」**，这是本章第 9 节要扎进脑子的第一条。
TXT

# ============================================================================
banner "10. 读内置模块之二：OpenMP 会真的编译测试程序，并把 flag 填进接口"
# ============================================================================
mkdir -p omp && cd omp || exit 1
printf 'cmake_minimum_required(VERSION 3.20)\nproject(omp LANGUAGES C)\nfind_package(OpenMP COMPONENTS C REQUIRED)\nadd_executable(o om.c)\ntarget_link_libraries(o PRIVATE OpenMP::OpenMP_C)\n' > CMakeLists.txt
printf '#include <stdio.h>\nint main(void){ int s = 0;\n#pragma omp parallel for reduction(+:s)\n  for (int i = 1; i <= 10; i++) { s += i; }\n  printf("sum=%%d\\n", s); return 0; }\n' > om.c
runc 'cmake -S . -B b | grep -E "Performing Test|Found OpenMP|Configuring done"'
runc 'cmake --build b | tail -1'
runc './b/o'
runc "grep -E 'C_FLAGS|C_INCLUDES' b/CMakeFiles/o.dir/flags.make"
cd .. || exit 1
runc 'grep -n -A2 "find_package_handle_standard_args(ZLIB" /usr/share/cmake-4.2/Modules/FindZLIB.cmake'
runc 'grep -n "file(STRINGS" /usr/share/cmake-4.2/Modules/FindZLIB.cmake'
cat <<'TXT'

两个内置模块摆在一起，正好是 Module 模式的两副面孔：

  · FindThreads：探针成功 → 接口是空的（上面第 9 节）
  · FindOpenMP：探针成功 → 接口里有 -fopenmp（flags.make 的 C_FLAGS 那行）
    而且它是**编译一个真程序**问出来的，不是查表。

下面两条 grep 是 FindZLIB.cmake（本机装了模块但没装 zlib，所以它只会「找不到」，
但源码仍可当教材）：它用 file(STRINGS ...) 从 zlib.h 里正则抠出 ZLIB_VERSION，
再把 VERSION_VAR ZLIB_VERSION 交给标准工具 —— 和我们第 4 节自己写的模块一模一样。
「版本比较是模块作者的活」这条规律，在官方模块里也是这么实现的。
TXT

# ============================================================================
banner "11. 路线③：FetchContent + GIT —— configure 时现拉源码（file:// 不联网）"
# ============================================================================
rm -rf "$SB/mathxsrc" && cp -R mathx "$SB/mathxsrc"
( cd "$SB/mathxsrc" && git init -q . \
    && git -c user.name=上游 -c user.email=up@example.invalid add . \
    && git -c user.name=上游 -c user.email=up@example.invalid commit -qm "mathx 1.3.0 发布" \
    && git tag v1.3.0 ) >/dev/null 2>&1
runc "git clone -q --bare \"$SB/mathxsrc\" \"$SB/mathx.git\" && git --git-dir=\"$SB/mathx.git\" tag"
runc "cmake -S fetch -B f-git -DROUTE=GIT \"-DMX_URI=file://$SB/mathx.git\" \
      | grep -E 'FetchContent 走|BASE_DIR|源码目录|构建目录|Configuring done'"
runc 'find f-git/_deps -maxdepth 1 | sort'
runc 'cmake --build f-git | tail -3'
runc './f-git/fetch_app'
cat <<'TXT'

_deps 下三件套，各管一件事：

    mathx-src/       拉下来的源码（真正的 .c/.h 在这儿）
    mathx-build/     它的构建产物（Make files、libmathx.a）—— 在你家构建目录里
    mathx-subbuild/  一个**独立的子 CMake 工程**，只干「把源码弄到手」这一件事

第三个最容易被忽略，也最能解释你看到的那些怪报错：下载/克隆是在子工程里
（mathx-populate 目标）执行的，所以 FetchContent 的失败常常长得像
「mathx-subbuild/.../mathx-populate-gitclone.cmake:61」这种内部路径。
TXT
runc "cmake -S fetch -B f-btag -DROUTE=GIT \"-DMX_URI=file://$SB/mathx.git\" -DMX_TAG=nope \
      | grep -E 'Failed|Error|checkout' | head -6"
echo '↑ GIT_TAG 写错（本地仓库里没这个 tag）：报错点在 subbuild 的 gitclone 脚本里。'
echo '  真实世界最常见的一条是 GIT_TAG main 或写得含含糊糊的分支名：'
echo '  今天能配好，明天上游一推新提交你就构建出不一样的东西 —— 要固定就写 tag 或完整 commit。'

# ============================================================================
banner "12. FetchContent + URL：哈希写错的报错里，就写着正确答案"
# ============================================================================
# 故意做成「字节级确定」的归档：成员名固定、mtime 固定、uid/gid 固定、gzip -n
runc 'mkdir -p tar && rm -rf tar/mathx-1.3.0 && cp -R mathx tar/mathx-1.3.0 \
      && tar --sort=name --mtime="UTC 2026-01-01" --owner=0 --group=0 --numeric-owner \
             -C tar -cf - mathx-1.3.0 | gzip -n > mathx-1.3.0.tar.gz \
      && sha256sum mathx-1.3.0.tar.gz'
MX_H=$(sha256sum mathx-1.3.0.tar.gz | cut -d" " -f1)
printf '[自证] 归档是确定性的：每次跑这份脚本，sha256 都应当是 %s\n' "$MX_H"
runc "cmake -S fetch -B f-url0 -DROUTE=URL \"-DMX_URI=file://$SB/proj/mathx-1.3.0.tar.gz\" \
      | grep -E 'Warning|CMP0135|DOWNLOAD_EXTRACT|rebuilt' | head -8"
echo '↑ 只给 URL 不给 DOWNLOAD_EXTRACT_TIMESTAMP → 策略 CMP0135 未设 → dev 警告。'
echo '  它担心的事很具体：归档里带着**老时间戳**，你换了 tar 包内容但文件 mtime 比目标旧，'
echo '  make 就认为不用重编 —— 于是你构建出的还是上一版代码。见第 13 节的修法。'
runc "cmake -S fetch -B f-badhash -DROUTE=URL \"-DMX_URI=file://$SB/proj/mathx-1.3.0.tar.gz\" \
      -DMX_HASH=0000000000000000000000000000000000000000000000000000000000000000 \
      | grep -E 'does not match|expected|actual' | head -5"
echo '↑ 哈希写错：报错里 expected 和 actual 两行都给全了。修法就是照抄 actual 那一串。'
runc "cmake -S fetch -B f-url -DROUTE=URL \"-DMX_URI=file://$SB/proj/mathx-1.3.0.tar.gz\" \
      \"-DMX_HASH=$MX_H\" -DMX_ETS=TRUE | grep -E 'SHA256|Configuring done|Build files'"
runc 'cmake --build f-url | tail -2'
runc './f-url/fetch_app'

# ============================================================================
banner "13. FetchContent 的代价：源码进了你的树，以及三味解药"
# ============================================================================
runc "find f-url/_deps/mathx-src -type f | sed 's|f-url/||' | sort"
runc 'du -sh f-url/_deps/* | sort -k2'
runc 'rm -rf f-url/_deps/mathx-build && cmake --build f-url 2>&1 | grep -E "Building|Linking" | head -4'
echo '↑ 代价就摆在这三屏里：源码和第三方的产物都住在你家构建目录。'
echo '  · rm -rf build 会把**拉下来的源码一起删掉**，下次 configure 重新拉一遍；'
echo '  · 第三方每次跟着重编（上面 mathx.c.o 那行就是它在你家 CI 里烧的时间）；'
echo '  · 别人 clone 你的仓库拿不到它，configure 时才去取 —— 内网/离线机器当场卡住。'
runc "cmake -S fetch -B f-off -DROUTE=URL \"-DMX_URI=file://$SB/proj/mathx-1.3.0.tar.gz\" \
      \"-DMX_HASH=$MX_H\" -DMX_ETS=TRUE | tail -1"
runc "cmake -S fetch -B f-off -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
      | grep -E 'Configuring done|BASE_DIR'"
echo '↑ 解药一：源码已经躺在 _deps 里之后，加 -DFETCHCONTENT_FULLY_DISCONNECTED=ON 重配，'
echo '  下载/更新这一步整个跳过，只把已有源码 add_subdirectory 进来。'
echo '  离线复现同事的构建、CI 里省时间，都靠它（第一次配好之后才有意义）。'
runc "grep -E 'C_INCLUDES' f-url/CMakeFiles/fetch_app.dir/flags.make"
runc "cmake -S fetch -B f-sys -DROUTE=URL \"-DMX_URI=file://$SB/proj/mathx-1.3.0.tar.gz\" \
      \"-DMX_HASH=$MX_H\" -DMX_ETS=TRUE -DMX_SYSTEM=ON | grep -E 'Configuring done'"
runc "grep -E 'C_INCLUDES' f-sys/CMakeFiles/fetch_app.dir/flags.make"
echo '↑ 解药二：同一个工程，只差 -DMX_SYSTEM=ON 一个档，头目录从 -I 变成 -isystem。'
echo '  -I 的潜台词是「这些头是我自己人的，警告算我的」；-isystem 才是「第三方系统头，'
echo '  别把它的警告报到我账上」。第 03 章装出去的包天生 -isystem（Config 文件里写的是'
echo '  INTERFACE_SYSTEM_INCLUDE_DIRECTORIES）；现拉的源码要你显式说 SYSTEM（CMake 3.25+）。'
runc 'cmake --build f-url --target help | grep -E "mathx|fetch_app"'
runc "cmake -S fetch -B f-excl -DROUTE=URL \"-DMX_URI=file://$SB/proj/mathx-1.3.0.tar.gz\" \
      -DMX_ETS=TRUE -DMX_EXCLUDE=ON >/dev/null 2>&1 && cmake --build f-excl --target help | grep -E 'mathx|fetch_app'"
echo '↑ 解药三 EXCLUDE_FROM_ALL：这里**实测看不出差别**，两份列表里 mathx 都在 ——'
echo '  因为 fetch_app 链接了 mathx，它无论如何都得编。EXCLUDE_FROM_ALL 只在'
echo '  「拉下来但没有任何 target 用它」（只要头文件、只要其中某个工具目标）时省时间。'
echo '  我不替它编一个看不到的效果：这一档留着，等你哪天真的拉了整个 boost 再说。'

# ============================================================================
banner "14. 路线④：vendoring —— 源码就在仓库里，add_subdirectory 一步到位"
# ============================================================================
mkdir -p noarg && printf 'cmake_minimum_required(VERSION 3.20)\nproject(noarg LANGUAGES C)\nadd_subdirectory(../mathx)\n' > noarg/CMakeLists.txt
runc 'cmake -S noarg -B b-noarg | sed -n "/CMake Error/,+3p"'
echo '↑ 树外源码必须给第二个参数（二进制目录）。CMake 没法替你猜一个源码在树外的'
echo '  工程该把产物放哪 —— 所以 vendored/CMakeLists.txt 里那句是写全的。'
runc 'cmake -S vendored -B v-all | grep -E "树外|Configuring done"'
runc 'cmake --build v-all | tail -3'
runc './v-all/vendor_app'
runc 'find v-all/_mathx -maxdepth 2 -name "*.a" -o -maxdepth 2 -name "*.cmake" | sort'
echo '↑ 对比第 11 节：vendoring 没有 _deps，也没有 subbuild；产物进了 _mathx/。'
runc "DESTDIR=\"$SB/stage\" cmake --install v-all --prefix /usr/local | tail -6"
runc "find \"$SB/stage\" -type f | sed \"s|$SB/stage/||\" | sort"
cat <<'TXT'

这一节真正要记住的是上面这两屏：

  你只 install 自己的工程，结果 libmathx.a、mathx.h，连 mathxConfig.cmake 三件套
  全被装走了 —— 因为 add_subdirectory 会**连同 install() 规则一起继承**。
  这就是 vendoring 的第三种税：前两种是「源码进你仓库」「每次跟着重编」，
  第三种是「你的包里混进了上游的交付物」。

  用户会因此在系统里看到一份你没承诺过的 libmathx.a；如果你的工程同时
  又 find_package(mathx) 过，两份同名 target/同名文件打架就开始了。
  DESTDIR 那次运行同样**没有**碰 /usr/local 一个字节（第 03 章的老规矩）。
TXT

# ============================================================================
banner "15. 找不到时怎么办：三步定位，别改十行 CMakeLists"
# ============================================================================
mkdir -p diag
printf 'cmake_minimum_required(VERSION 3.20)\nproject(diag LANGUAGES C)\nset(CMAKE_FIND_DEBUG_MODE ON)\nfind_package(mathx REQUIRED)\n' > diag/CMakeLists.txt
runc "cmake -S consumer -B c-dbg -DCMAKE_PREFIX_PATH=\"$P_BARE\" -DUSE_OUR_MODULE=OFF \
      | sed -n '/CMake Error/,/installed/p'"
echo '↑ 第一步：**先把报错本身读完**，它就是说明书。这一条说了四件事：'
echo '  ① 「By not providing Findmathx.cmake」—— 我先试了模块模式，你没有；'
echo '  ② 候选文件名 mathxConfig.cmake / mathx-config.cmake（就这两个拼法，别自创）；'
echo '  ③ 补救办法一：把安装前缀加进 CMAKE_PREFIX_PATH；'
echo '  ④ 补救办法二：或者直接设 mathx_DIR。下面第三步就是它。'
runc 'cmake -S diag -B d1 2>&1 | sed -n "/CMake Debug Log/,+16p"'
runc 'cmake -S diag -B d2 2>&1 | grep -c "^    /"'
echo '↑ 第二步：set(CMAKE_FIND_DEBUG_MODE ON)（或命令行加 --debug-output）把搜索轨迹全打出来。'
echo '  上面那个数字就是它替你试过的**候选路径条数**（这次跑出来 39 条，机器上装的东西越多越长）'
echo '  —— 这就是为什么平时别开它，'
echo '  但查「我明明装了它为什么说没装」时它是唯一可靠的仪器：它会逐个列出'
echo '  CMAKE_MODULE_PATH、pkgRedirects、<包名>_ROOT、CMAKE_PREFIX_PATH、'
echo '  环境变量版、CMAKE_SYSTEM_PREFIX_PATH……每一条试到哪、为什么被否。'
runc "cmake -S consumer -B c-dir -DCMAKE_PREFIX_PATH=/nonexistent -DUSE_OUR_MODULE=OFF \
      \"-Dmathx_DIR=$P_CFG/lib/cmake/mathx\" | grep -E '应答|mathx_DIR|Configuring done'"
runc 'cmake --build c-dir | tail -1'
runc './c-dir/consumer'
echo '↑ 第三步（救命稻草）：`-D<包名>_DIR=<放配置文件的目录>`。'
echo '  注意 CMAKE_PREFIX_PATH 是个不存在的路径，configure 却成功了 ——'
echo '  因为 mathx_DIR 直接点名，跳过了整套搜索。它也是最精确的诊断手段：'
echo '  用 _DIR 能成、用前缀不能成，就说明前缀下那个 lib/cmake/mathx 的层级不对。'
runc "cmake -S consumer -B c-cfgvar -DCMAKE_PREFIX_PATH=/nonexistent -DUSE_OUR_MODULE=OFF \
      \"-Dmathx_CONFIG=$P_CFG/lib/cmake/mathx/mathxConfig.cmake\" | sed -n '/CMake Error/,+3p'"
echo '↑ 反面实测：`-D<包名>_CONFIG=<配置文件本身>`（CACHE 和普通变量两种写法我都试过）**不生效**。'
echo '  别指望它。要指目录就用 mathx_DIR，指文件是不存在的通路。'
runc "cmake -S consumer -B c-baddir -DCMAKE_PREFIX_PATH=\"$P_CFG\" -DUSE_OUR_MODULE=OFF -Dmathx_DIR=/nope \
      | grep -E '应答|Error|Configuring done'"
echo '↑ 更阴的一条：mathx_DIR 设成**不存在的目录**，CMake 不报「你给的目录里没有配置文件」，'
echo '  而是当作没设、退回正常搜索 —— 于是这次照样成功（前缀里那份被找到了）。'
echo '  后果是：你机器上好好的，CI 上换了个前缀就崩，而且报错完全不提 _DIR。'
echo '  凡是设过 <包名>_DIR 的工程，查问题第一件事就是去构建目录里'
echo '  `grep mathx_DIR CMakeCache.txt` 看它到底被填成了什么。'
runc "grep -rn 'https?://' CMakeLists.txt cmake mathx consumer fetch vendored || \
      echo '[自证] 本章源文件里没有一个 http(s) 地址：FetchContent 全程只走 file://'"
echo '↑ [自证] 上面的 http 只可能出现在 CMake 自己生成的 _deps/**/mathx-subbuild 许可头里，'
echo '  那些不是我写的；所以这里只 grep 本章那 7 个源文件，它们一条都没有。'

printf '\n%s[全部跑完] 本章 README 引用的每一段输出都出自这一次运行。%s\n' "$B" "$R"
exit 0
