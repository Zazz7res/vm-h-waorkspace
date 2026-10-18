#!/usr/bin/env bash
# ============================================================================
# 第 01 章《第一次构建：把 CMake 的三件事看清楚》—— 整章证据一键复现
#
#   bash demo.sh          跑完自动删沙箱（默认）
#   bash demo.sh --keep   跑完保留沙箱并打印路径，方便自己进去接着敲
#   bash demo.sh > out.txt   重定向时自动关掉颜色，README 里引用的就是这份纯文本
#
# 这个脚本按 README 的小节顺序，把每一段的真实终端输出打出来。
# 屏幕上出现的每一行，都是这条命令在这台机器上的真实结果 —— 没有一行是手写的。
#
# 安全说明：
#   * 所有构建都发生在 mktemp 出来的临时沙箱里，绝不污染手册的示例源码目录
#   * 只编译本目录这份最小示例，不碰仓库里那 56 节数据结构代码
#   * 不写任何全局配置、不装东西、不联网
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

set -u                  # 故意不加 -e，也不用 pipefail：
                        # ① 本章要故意触发几次失败，得让报错打出来；
                        # ② 加了 pipefail 之后 `cmd | head -14` 会因为 head 提前关闭管道而报 141，
                        #    屏幕上就会出现一堆和教学无关的退出码。

export LC_ALL=C.UTF-8 LANG=C   # 固定输出语言，方便和 README 逐行对照

# 只有输出到终端才上色；重定向到文件时是干净纯文本
if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)              # 示例源码在哪
SB=$(mktemp -d "${TMPDIR:-/tmp}/cmake01-XXXXXX") # 沙箱

banner() { printf '\n%s══════ %s %s\n' "$B" "$1" "══════════════════════════════$R"; }

# 打印命令 → 执行 → 打印退出码。用 bash -c 是为了能自然地写管道和 glob。
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去接着敲： cd "%s/proj"     看完删掉： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# ============================================================================
banner "0. 这台机器上有什么（版本决定你能用哪些写法）"
# ============================================================================
runc 'cmake --version'
runc 'make --version | head -2'
runc 'gcc --version | head -1'
runc 'which cmake make gcc'
printf '\n# 有没有 C++ 编译器？这决定了 project() 里能不能写 LANGUAGES CXX\n'
runc 'command -v c++ || echo "c++: 未安装"'
if ! command -v c++ >/dev/null 2>&1; then
    echo '→ 这台机器没有 g++。所以示例里必须写 LANGUAGES C；写 CXX 会在“编译器探测”阶段就失败。'
fi

# ============================================================================
banner "1. 把示例复制进沙箱（源码目录从此保持干净）"
# ============================================================================
mkdir -p "$SB/proj"
cp -R "$HERE/CMakeLists.txt" "$HERE/src" "$SB/proj/"
cd "$SB/proj" || exit 1
runc 'find . -type f | sort'
echo '↑ 整个工程就 4 个文件：1 个 CMakeLists.txt + 3 个源文件。没有别的配置入口。'

# ============================================================================
banner "2. 第一次 configure：一句话，但它干了 4 件事"
# ============================================================================
echo '# -S 源码目录（放 CMakeLists.txt 的地方），-B 构建目录（所有产物堆在这）'
runc 'cmake -S . -B build'
runc 'ls build'
runc 'ls build/CMakeFiles | head -10'
runc 'du -sh build'
runc 'find build -name "*.o" | wc -l'
echo '↑ 关键：configure 完一个 .o 都没有。它只是“把计划写好”，还没动手编译。'

# ============================================================================
banner "3. build：cmake --build 是把活转包给底层构建系统（这里是 Unix Makefiles）"
# ============================================================================
runc 'cmake --build build'
runc 'ls -l build/hello'
runc 'file build/hello'
runc './build/hello'
echo '# 同一条命令立刻再跑一次：'
runc 'cmake --build build'
echo '↑ 第二次只有一行 “[100%] Built target hello”，没有任何 [33%]/[66%] 编译行 ——'
echo '  没有文件变动，所以一个 .o 都没重编。这就是增量构建。'

# ============================================================================
banner "4. 增量：碰一个文件，只重编那一个 .o"
# ============================================================================
runc 'touch src/greet.c && cmake --build build'
echo '# 改 main.c 并开 4 路并行：'
runc 'touch src/main.c && cmake --build build -j 4'

# ============================================================================
banner "5. 改 CMakeLists.txt：构建时自动 re-configure，不用你手动重跑"
# ============================================================================
echo '# 往 CMakeLists.txt 末尾追加一行真正的 target 级设置：'
runc "printf 'target_compile_definitions(hello PRIVATE BUILD_TAG=\"demo\")\n' >> CMakeLists.txt && tail -3 CMakeLists.txt"
runc 'cmake --build build'
echo '↑ 注意输出里先出现了 Configuring/Generating —— CMakeLists 变了会自动重新 configure，'
echo '  然后因为编译参数变了，两个 .o 全部重编。'
runc './build/hello'

# ============================================================================
banner "6. 缓存：configure 的结果全写在 CMakeCache.txt —— 90% 的怪事来源"
# ============================================================================
runc 'grep -E "^(CMAKE_C_COMPILER|CMAKE_C_FLAGS|CMAKE_BUILD_TYPE|HELLO_STRICT|CMAKE_GENERATOR):" build/CMakeCache.txt'
runc 'wc -l build/CMakeCache.txt'
runc 'cmake -L build | head -14'
runc 'cmake -LAH build | wc -l'
echo '↑ -L 只列出“给用户看的” 3 个变量；-LAH 把内部条目全倒出来，一共 148 行。查缓存先用 -L。'
runc 'cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug | tail -5'
runc 'grep -E "^CMAKE_BUILD_TYPE:" build/CMakeCache.txt'
runc "grep -E '^CMAKE_C_FLAGS' build/CMakeCache.txt"
echo '# 这些变量最终落到哪？落到每个 target 的 flags.make 里：'
runc 'grep -E "C_INCLUDES|C_DEFINES|C_FLAGS" build/CMakeFiles/hello.dir/flags.make'
runc 'cmake --build build'
echo '↑ C_FLAGS 里出现了 -g（Debug 的默认值）和 -std=gnu11；C_DEFINES 里是第 5 步加的 BUILD_TAG。'
echo '  变量→flags.make→真正的 gcc 命令行，这条链看懂了，CMake 就不再是黑盒。'

# ============================================================================
banner "7. option() + -D：自己加一个开关，顺便把“拼错变量名”跑给你看"
# ============================================================================
runc 'cmake -S . -B build-strict -DHELLO_STRICT=ON | grep -E "strict|Configuring done|Generating done|Build files"'
runc 'grep -E "^HELLO" build-strict/CMakeCache.txt'
runc 'cmake --build build-strict'
echo '↑ 带 -Werror 也能通过，说明示例代码本身没有警告。'
echo '# 现在把变量名拼错一个字母：STRICT → STRICTT'
runc 'cmake -S . -B build-typo -DHELLO_STRICTT=ON'
runc 'grep -E "^HELLO" build-typo/CMakeCache.txt'
echo '↑ 两件事同时发生：① 拼错的变量被原样塞进缓存，类型标成 UNINITIALIZED（意思是“项目里没人用它”），'
echo '  并且只给了一句 CMake Warning，构建照样成功退出码 0；② 真正的 HELLO_STRICT 仍然是 OFF，-Werror 根本没开。'
echo '  也就是说 -D 拼错不会让命令失败，只会**静默失效** —— 这是 CMake 最难查的一类 bug。'

# ============================================================================
banner "8. 故意犯的第二个错：在源码目录里就地构建（in-source）"
# ============================================================================
mkdir -p "$SB/insrc"
cp -R "$HERE/CMakeLists.txt" "$HERE/src" "$SB/insrc/"
cd "$SB/insrc" || exit 1
echo '# 老教程里最常见的写法 cmake . —— 今天一律别用：'
runc 'cmake .'
runc 'find . -type f | sort | head -14'
runc 'cmake --build . | tail -4'
echo '↑ 能编，但产物和源码混在一起：CMakeCache.txt、CMakeFiles/、Makefile、hello 全落在源码树里。'
echo '# 想清干净只能手工删（CMake 自己不会帮你删）：'
runc 'rm -rf CMakeCache.txt CMakeFiles cmake_install.cmake Makefile hello && find . -maxdepth 1 -type f | sort'
cd "$SB/proj" || exit 1

# ============================================================================
banner "9. 给编辑器 / clangd 用：导出 compile_commands.json"
# ============================================================================
runc 'cmake -S . -B build-cc -DCMAKE_EXPORT_COMPILE_COMMANDS=ON | tail -3'
runc 'head -c 320 build-cc/compile_commands.json'
printf '\n# 每条记录就是一个 target 的一条真实命令行：\n'
runc "python3 -c \"import json;[print(t['file'],'->',t['command'][:96]) for t in json.load(open('build-cc/compile_commands.json'))]\""
echo '↑ 里面能看见 -std=gnu11（CMAKE_C_STANDARD 变成了命令行参数）、-Wall -Wextra -Wshadow、-DBUILD_TAG。'

# ============================================================================
banner "10. 反悔与重来：删掉构建目录是唯一干净的“重置”"
# ============================================================================
runc 'rm -rf build && cmake -S . -B build | grep -E "compiler|Detecting|Configuring|Generating|Build files"'
runc 'cmake --fresh -S . -B build | tail -3'
echo '↑ --fresh 是 CMake 3.24+ 的现成开关，等价于“删缓存再 configure”，不用自己 rm -rf。'
runc 'cmake --build build --target help'
echo '# 这份 output 里能看到 hello、all、clean、rebuild_cache 等 —— 那是 Makefile 的 target，'
echo '# 不是 CMake 的 target。两个“target”不是同一个东西，第 02 章会专门区分。'

printf '\n%s[全部跑完] 上面每一段都是真实执行结果；README.md 引用的输出全部出自这一次运行。%s\n' "$B" "$R"
exit 0
