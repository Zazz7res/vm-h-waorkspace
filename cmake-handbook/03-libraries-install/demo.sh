#!/usr/bin/env bash
# ============================================================================
# 第 03 章《库、install 与 export：让别人 find_package 得到你的库》整章证据
#
#   bash demo.sh            跑完自动删沙箱（默认）
#   bash demo.sh --keep     保留沙箱（里面有几个完整的安装树，值得进去翻）
#   bash demo.sh > out.txt  重定向会自动关掉颜色
#
# 本章看点：同一份源码，经过 install/export 变成「别人能用的库」，
# 再由一个**独立工程**用 find_package 找回来。中间故意踩四种坑：
#   ① 没告诉 CMake 库装在哪        → find_package 找不到（第 5 节）
#   ② 内部依赖没进导出集合         → install(EXPORT) 拒绝生成（第 11 节）
#   ③ 公开头目录写裸绝对路径       → install(EXPORT) 拒绝生成（第 14 节）
#   ④ 版本要求越过 SameMajorVersion → 配置期拒绝（第 10 节）
#
# 安全说明：
#   * 全程在 mktemp 沙箱里；--prefix 永远指向沙箱，**绝不往 /usr/local 装东西**
#   * 最后一节演示 DESTDIR 暂存，并在结尾用 ls 证明 /usr/local 没被碰
#   * 不碰仓库里那 56 节数据结构代码；不写全局配置；不联网
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

set -u                                   # 故意不加 -e：本章要故意触发很多次失败
export LC_ALL=C.UTF-8 LANG=C

if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/cmake03-XXXXXX")
PREFIX="$SB/prefix"                       # 安装前缀：只在沙箱里

banner() { printf '\n%s══════ %s %s\n' "$B" "$1" "══════════════════════════════$R"; }

# 打出命令，执行，打退出码
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}
# 同上，但只保留关心的行（长输出时用）
only() { printf '\n%s$ %s | grep %s%s\n' "$C" "$1" "'$2'" "$R"; bash -c "$1" 2>&1 | grep -E "$2"; }

# 安装树形状（只要文件，不要目录）
tree() { printf '\n%s$ find \"%s\" -type f | sort%s\n' "$C" "$1" "$R"; find "$1" -type f 2>&1 | sort | sed "s|$SB|<沙箱>|g"; }

# 导出文件里最能说明问题的几行
expgrep() {
    printf '\n%s$ grep -nE \"add_library|LINK_LIBRARIES|INCLUDE_DIRECTORIES\" %s%s\n' "$C" "$1" "$R"
    grep -nE 'add_library|LINK_LIBRARIES|INCLUDE_DIRECTORIES' "$1" 2>&1 | sed "s|$SB|<沙箱>|g"
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  默认档安装树：%s/prefix\n  进去自己敲： cd %s/proj，看完删： rm -rf \"%s\"\n' "$SB" "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# ============================================================================
banner "1. 工程结构：一个库 + 一个内部依赖 + 两种消费者"
# ============================================================================
mkdir -p "$SB/proj"
cp -R "$HERE/CMakeLists.txt" "$HERE/cmake" "$HERE/util" "$HERE/lib" "$HERE/app" "$HERE/consumer" "$SB/proj/"
cd "$SB/proj" || exit 1
runc 'find . -type f | sort'
cat <<'TXT'

两个消费者的区别是本章的主线：

    app/        和库在**同一条构建树**里（顶层 add_subdirectory 加进来的）
                → 直接 target_link_libraries(app PRIVATE Vector::vector) 就完事
    consumer/   是**另一个独立工程**，库早已被 cmake --install 装到磁盘上
                → 只能 find_package(Vector) 把 Vector::vector 找回来

consumer/ 故意**没有**被顶层加进构建树：加了它就等于作弊 —— 它能看见源码路径，
于是「装漏了什么」「头有没有导出」这类问题全都暴露不出来。
TXT

# ============================================================================
banner "2. 默认档 configure：那几行 message 是「当前拨到哪一档」的回执"
# ============================================================================
runc 'cmake -S . -B build'
cat <<'TXT'

CMAKE_INSTALL_PREFIX 现在是默认的 /usr/local —— 我们**不会**用它，第 4 节换成沙箱里的前缀。

顺手验证一件事：这些 CACHE 开关的取值 CMake 一律**不做检查**，所以顶层自己写了一段 MATCHES 校验。
把 PRIVATE 拼错成 PROVITE 试试：
TXT
only 'cmake -S . -B build-typo -DVECTOR_DEP_SCOPE=PROVITE' 'VECTOR_DEP_SCOPE|非法|Configuring incomplete'
echo '↑ 对照第 02 章的「拼错变量名 → 静默失效」：加了校验之后就变成当场报错。'
echo '  set_property(CACHE ... PROPERTY STRINGS ...) 只让 cmake-gui 显示下拉框，不拦命令行 -D。'

# ============================================================================
banner "3. 构建：产物落在各自子目录里，可执行文件的完整路径是 build/app/app"
# ============================================================================
runc 'cmake --build build'
runc "find build \\( -name '*.a' -o -name '*.so*' \\) | sort"
runc 'ls -ld build/app; ls -l build/app/app'
runc './build/app/app'
cat <<'TXT'

↑ 两个新手常踩的坑在这一节一起出现：
  · 可执行文件是 build/app/app（目录名和可执行名重名），不是 build/app —— 后者是目录，
    直接执行会报 Is a directory。
  · find 里那对 \\( \\)：不分组时 -o 右边的规则只作用于它自己那一项，列出来的东西会不符合预期。
TXT

# ============================================================================
banner "4. install：把 --prefix 指到沙箱（千万别装进 /usr/local）"
# ============================================================================
runc "cmake --install build --prefix \"$PREFIX\""
tree "$PREFIX"
cat <<'TXT'

安装树的形状就是「别人怎么用你」的答案：

    bin/app                      ← RUNTIME DESTINATION
    lib/libvector.a              ← ARCHIVE DESTINATION（共享库时换成 LIBRARY DESTINATION）
    lib/libutil.a                ← 内部依赖也被装了（第 11 节演示不装会怎样）
    include/vector.h             ← install(DIRECTORY include/)：**只有公开头**
    include/vector/util.h        ← 内部依赖的头，多包一层目录，暗示「不是给你用的」
    lib/cmake/Vector/*.cmake     ← find_package 的全部入口

vector_internal.h 没出现在这里 —— 它从来没被 install，这正是第 9 节的靶子。
TXT

# ============================================================================
banner "5. 先看 find_package 怎么「找不到」：装了 ≠ 能被找到"
# ============================================================================
runc 'cmake -S consumer -B build-consumer 2>&1 | tail -18'
echo '↑ 库确实装好了，但没有任何人告诉 CMake 前缀在哪，于是 find_package 一无所获。'
echo '  报错里那句 Add the installation prefix of "Vector" to CMAKE_PREFIX_PATH or set'
echo '  "Vector_DIR" 就是第 6 节要做的事 —— 这是 CMake 少见的会把解决办法写在脸上的报错。'

# ============================================================================
banner "6. 用 CMAKE_PREFIX_PATH 接通：consumer 配置、构建、运行"
# ============================================================================
runc "cmake -S consumer -B build-consumer -DCMAKE_PREFIX_PATH=\"$PREFIX\""
runc 'cmake --build build-consumer'
runc './build-consumer/consumer'
echo '↑ 中间那三行 message 是 consumer 自己去读导入目标身上的属性读出来的：'
echo '  Vector_DIR = find_package 实际加载 VectorConfig.cmake 的目录；'
echo '  头目录来自 \$<INSTALL_INTERFACE:include>；链接要求那一行是第 7 节的主角。'

# ============================================================================
banner "7. 物证：导出的 VectorTargets.cmake 里 target 是怎么「复活」的"
# ============================================================================
expgrep "$PREFIX/lib/cmake/Vector/VectorTargets.cmake"
only "grep -c 'LINK_ONLY' \"$PREFIX/lib/cmake/Vector/VectorTargets.cmake\"" '^[0-9]+$'
runc "grep -h 'C_INCLUDES' build-consumer/CMakeFiles/consumer.dir/flags.make | sed \"s|$SB|<沙箱>|g\""
cat <<'TXT'

三件事值得停一下：

  · INTERFACE_INCLUDE_DIRECTORIES 用的是 ${_IMPORT_PREFIX} 这种**运行时算出来的**变量，
    不是绝对路径 —— 所以整个前缀目录可以搬到任何机器、任何位置，解压即可用。
  · 消费者拿到的是 -isystem 而不是 -I：CMake 默认把导入目标的头目录标成 system 目录，
    好处是库头文件里的警告不会刷屏你自己的构建输出。
  · \$<LINK_ONLY:Vector::util> 的意思是「util 只参与链接，不参与编译」。
    这就是 PRIVATE 在导出文件里的长相 —— 第 02 章的语义换了身衣服还在。
TXT

# ============================================================================
banner "8. A/B 实验：只把作用域换成 PUBLIC，导出文件、-I、可见的头全变"
# ============================================================================
only 'cmake -S . -B build-pub -DVECTOR_DEP_SCOPE=PUBLIC' 'VECTOR_DEP_SCOPE|Configuring done|Build files'
runc "cmake --build build-pub 2>&1 | tail -2"
only "cmake --install build-pub --prefix \"$PREFIX-pub\"" 'libutil|libvector|util.h'
expgrep "$PREFIX-pub/lib/cmake/Vector/VectorTargets.cmake"
only "cmake -S consumer -B bpub -DCMAKE_PREFIX_PATH=\"$PREFIX-pub\"" '链接要求|头目录'
runc "grep -h C_INCLUDES bpub/CMakeFiles/consumer.dir/flags.make | sed \"s|$SB|<沙箱>|g\""
cat <<'TXT'

现在让 consumer 去 include 内部依赖的头 util.h，两个档各试一次：
TXT
runc "cmake -S consumer -B bpub2 -DCMAKE_PREFIX_PATH=\"$PREFIX-pub\" -DCONSUMER_PROBES_UTIL_H=ON >/dev/null && cmake --build bpub2 2>&1 | tail -2 && ./bpub2/consumer"
runc "cmake -S consumer -B bpriv -DCMAKE_PREFIX_PATH=\"$PREFIX\" -DCONSUMER_PROBES_UTIL_H=ON >/dev/null && cmake --build bpriv 2>&1 | grep -E 'fatal error|Error 1' | head -2"
cat <<'TXT'

同一个 consumer/main.c、同一行 #include "util.h"：
  · PUBLIC 档（上面第一个）编过了，还打印出 u_clamp 的结果；
  · PRIVATE 档（上面第二个）fatal error: util.h: No such file or directory。

差别只在导出的那一行：PUBLIC 是裸的 Vector::util（编译和链接都传染），
PRIVATE 是 $<LINK_ONLY:Vector::util>（只传染链接）。
但两档里 libutil.a **都装了**，消费者的链接命令里**都有**它 ——
「不传染」从来不等于「不交付」。这句话是第 11 节的伏笔。
TXT

# ============================================================================
banner "9. 越界实验：消费者想去 include 库自己的内部头"
# ============================================================================
runc "cmake -S consumer -B build-leak -DCMAKE_PREFIX_PATH=\"$PREFIX\" -DCONSUMER_PROBES_INTERNAL=ON >/dev/null && cmake --build build-leak 2>&1 | grep -E 'fatal error|Error 1' | head -2"
echo '↑ vector_internal.h 从来没被 install，安装树里没有它，编译当场失败。'
echo '  注意它和 util.h 是**两种不同的看不见**：util.h 装了、只是不给 -I（作用域问题）；'
echo '  vector_internal.h 压根没装（交付清单问题），所以无论 PUBLIC 还是 PRIVATE 都编不过。'

# ============================================================================
banner "10. 版本实验：consumer 要求 2.0，而包是 1.0.2"
# ============================================================================
runc "cmake -S consumer -B build-ver -DCMAKE_PREFIX_PATH=\"$PREFIX\" -DCONSUMER_FIND_VERSION=2.0 2>&1 | tail -12"
only "cmake -S consumer -B build-ver1 -DCMAKE_PREFIX_PATH=\"$PREFIX\" -DCONSUMER_FIND_VERSION=1.0" '找到 Vector 的配置|Configuring done'
cat <<'TXT'

报错里那句 is not compatible with requested version 来自 write_basic_package_version_file
生成的 VectorConfigVersion.cmake，本章用的 COMPATIBILITY 是 SameMajorVersion：
  要求 1.0 → 1.0.2 满足（主版本号相同）；要求 2.0 → 直接拒绝。
另外两个常用值：ExactVersion（一字不差）、AnyNewerVersion（不比它旧就行）。

还有第三种更常见的病：CMakeLists.txt:18 (find_package) 后面跟的是「找不到」而不是「版本不对」
（第 5 节）。分不清这两种，就会花半小时去改版本号，其实前缀压根没给。
TXT

# ============================================================================
banner "11. 本章最硬的一条报错：内部依赖没进导出集合 —— PRIVATE 也照样逃不掉"
# ============================================================================
runc 'cmake -S . -B build-noexport -DVECTOR_EXPORT_UTIL=OFF 2>&1 | grep -E "CMake Error|not in any export|Generating step failed" -A2 | head -10'
runc 'cmake --build build-noexport 2>&1 | tail -2'
runc "cmake --install build-noexport --prefix \"$PREFIX-noexport\" 2>&1 | tail -3"
tree "$PREFIX-noexport"
only "cmake -S consumer -B bnoex -DCMAKE_PREFIX_PATH=\"$PREFIX-noexport\"" '找到 Vector 的配置|Configuring done'
expgrep "$PREFIX-noexport/lib/cmake/Vector/VectorTargets.cmake"
runc "cmake --build bnoex 2>&1 | grep -E 'undefined reference|ld returned|Error 1' | head -4"
cat <<'TXT'

这条链条是真实项目里最难查的一类事故，所以完整跑给你看：

  ① 配置期 CMake 明确报错：install(EXPORT) includes target "vector" which requires
     target "util" that is not in any export set.
     它**不区分 PRIVATE 和 PUBLIC**：只要你 link 了，就得交代清楚这个 target 归谁交付。
  ② 这个报错发生在 generate 阶段 —— configure 已经成功，构建脚本大部分已经落盘，
     于是 cmake --build 和 cmake --install 居然还是**成功**的，
     装出去一个 VectorTargets.cmake 里写着裸 util（没有对应的导入目标）的半残包。
  ③ consumer 那边 find_package 一切正常，直到链接才炸：undefined reference to u_clamp。
     报错里一个 export/install 字样都没有，症状和病因隔了两层楼。

所以本章的硬规矩：**导出集合必须覆盖整条依赖链**，link 过的 target 一个都不能漏。
TXT

# ============================================================================
banner "12. 内部依赖的第三条路：OBJECT 库 + 不导出 = 彻底不外露"
# ============================================================================
runc 'cmake -S . -B build-hide -DVECTOR_UTIL_AS_OBJECT=ON -DVECTOR_EXPORT_UTIL=OFF 2>&1 | grep -E "CMake Error|not in any export|Configuring done|Build files" | head -6'
runc "cmake --build build-hide 2>&1 | tail -2"
only "cmake --install build-hide --prefix \"$PREFIX-hide\"" 'Installing'
tree "$PREFIX-hide"
runc 'ar t build-hide/lib/libvector.a'
runc 'nm --defined-only build-hide/lib/libvector.a | grep u_clamp'
expgrep "$PREFIX-hide/lib/cmake/Vector/VectorTargets.cmake"
runc "cmake -S consumer -B bhide -DCMAKE_PREFIX_PATH=\"$PREFIX-hide\" >/dev/null && cmake --build bhide 2>&1 | tail -1 && ./bhide/consumer"
cat <<'TXT'

这一档和第 11 节的区别全在 lib/CMakeLists.txt 那个 if 里：
util 编成 OBJECT 库（只产 .o，不产 .a），vector 不 link 它，而是
    target_sources(vector PRIVATE $<TARGET_OBJECTS:util>)
于是 util 的代码**物理上就长在 libvector.a 里面**（ar t 看得见 util.c.o，
nm 看得见 u_clamp 是这份归档自己定义的符号）。依赖边根本不存在，
install(EXPORT) 自然没什么可拒绝的 —— 配置期干干净净，导出文件里连 LINK_LIBRARIES 都没有。

代价也要看清：
  · vector.c 里那句 #include "util.h" 没人给 -I 了，得自己在 PRIVATE 里补上；
  · util 的头干脆不装（上面安装树里没有 include/vector/util.h），
    于是第 8 节那种 #include "util.h" 无论 PUBLIC/PRIVATE 都编不过 —— 这才是真正的隐藏；
  · 复制 .o 意味着同一份代码可能被吸进好几个库里，重复符号要自己当心。
TXT

# ============================================================================
banner "13. 换成共享库：BUILD_SHARED_LIBS=ON，产物、链接、加载三件事全变"
# ============================================================================
mkdir -p "$SB/shared"
cp -R "$HERE/CMakeLists.txt" "$HERE/cmake" "$HERE/util" "$HERE/lib" "$HERE/app" "$HERE/consumer" "$SB/shared/"
cd "$SB/shared" || exit 1
only 'cmake -S . -B build -DBUILD_SHARED_LIBS=ON' '库类型|Configuring done|Build files'
runc 'cmake --build build 2>&1 | tail -6'
runc "find build \\( -name '*.a' -o -name '*.so*' \\) | sort"
runc "cmake --install build --prefix \"$PREFIX-shared\" 2>&1 | tail -2"
expgrep "$PREFIX-shared/lib/cmake/Vector/VectorTargets.cmake"
runc "cmake -S consumer -B bsh -DCMAKE_PREFIX_PATH=\"$PREFIX-shared\" >/dev/null && cmake --build bsh 2>&1 | tail -1 && ./bsh/consumer"
runc "nm -D --defined-only \"$PREFIX-shared/lib/libvector.so\" | grep u_clamp"
runc 'ldd ./bsh/consumer | grep -E "vector|not found" | sed "s|0x.*||"'
cat <<'TXT'

上面这几行各自的含义：

  · 产物从 libvector.a 变成 libvector.so，走的还是同一份 install(TARGETS ...)：
    .a 命中 ARCHIVE DESTINATION，.so 命中 LIBRARY DESTINATION —— 所以三个目的都写上才不用改文件。
  · libutil.a 依然被装了（这一档 VECTOR_EXPORT_UTIL 还是 ON）。
  · nm -D 显示 u_clamp 是 libvector.so 自己**定义**的符号：
    PRIVATE 依赖在链接 .so 的那一刻被真的吸了进去，所以消费者链接期不再需要 util。
    导出文件里 Vector::vector 那一行也只剩 INCLUDE_DIRECTORIES，连 LINK_LIBRARIES 都没有 ——
    共享库天生自带依赖，这是它和静态库在「交付」上最大的区别。
  · 构建树里的 consumer（上面 ./bsh/consumer）能跑，是因为 CMake 给它写了构建期 RPATH，
    ldd 那一行解析到的就是 <沙箱>/prefix-shared/lib/libvector.so。

那**装出去**的 app 呢？它已经不在构建目录里了：
TXT
runc "ldd \"$PREFIX-shared/bin/app\" | grep -E 'vector|not found' | sed \"s|0x.*||\""
runc "{ \"$PREFIX-shared/bin/app\"; echo \"退出码=\$?\"; } 2>&1 | head -2"
runc "readelf -d \"$PREFIX-shared/bin/app\" | grep -E 'RPATH|RUNPATH' || echo '（这个二进制里一个 RPATH/RUNPATH 字段都没有）'"
cat <<'TXT'

事故是**安装这一步造成的**。回看上面 cmake --install 的输出，最后那句：
    Set non-toolchain portion of runtime path of ".../bin/app" to ""
意思是 CMake 故意把构建期的 RPATH 擦掉了（构建期它指向源码树旁边那个 .so）。
擦掉之后没人告诉动态链接器 libvector.so 在哪，于是 ldd 报 not found，
执行时连 main 都没进去就失败了 —— 注意这种失败**没有任何 CMake 报错**：
它发生在运行期，第一个遇到它的人是你的用户。

三种正规解法（下面现场演示第一种）：
  ① 装的时候就把 RPATH 写对：set_target_properties(app PROPERTIES INSTALL_RPATH "$ORIGIN/../lib")
     $ORIGIN 是**链接器**的语法（不是 shell 变量），意思是「这个可执行文件自己所在的目录」，
     所以整个前缀目录搬到哪都能跑 —— 绿色软件/AppImage 那类打包就是这么做的。
  ② 交给系统：把库目录写进 /etc/ld.so.conf.d/ 再 ldconfig（需要 root，属于打包脚本的活）
  ③ 临时救场：LD_LIBRARY_PATH=<前缀>/lib ./bin/app（只适合调试，别写进服务启动脚本）
TXT
runc "printf '%s\n' 'set_target_properties(app PROPERTIES INSTALL_RPATH \"\$ORIGIN/../lib\")' >> app/CMakeLists.txt && tail -1 app/CMakeLists.txt"
runc "cmake --build build 2>&1 | grep -E 'Configuring done|Linking C executable|Built target app'"
runc "cmake --install build --prefix \"$PREFIX-shared\" 2>&1 | tail -2"
runc "readelf -d \"$PREFIX-shared/bin/app\" | grep -E 'RPATH|RUNPATH'"
runc "{ \"$PREFIX-shared/bin/app\"; echo \"退出码=\$?\"; } 2>&1 | head -3"
cat <<'TXT'

同一条安装路径，只多了一行 INSTALL_RPATH，装出去的 app 就能跑了。
两件事顺带记牢：
  · 改 CMakeLists 之后 cmake --build 会自动重新配置（第 01 章那条规则在这里也生效）；
  · RPATH 和头目录、链接库一样，都是 target 属性，也就是**交付物的一部分**。
  · 顶层那句 set(CMAKE_POSITION_INDEPENDENT_CODE ON) 是这一节能链接成功的前提：
    静态库 libutil.a 的代码要被吸进 .so，机器码就必须是位置无关的。
TXT

# ============================================================================
banner "14. 裸绝对路径的公开头目录：同树一个字不报错，一导出就炸"
# ============================================================================
runc 'cmake -S . -B build-plain -DVECTOR_PLAIN_INCLUDE=ON 2>&1 | grep -E "CMake Error" -A5 | head -12'
runc "cmake --build build-plain 2>&1 | tail -2"
cat <<'TXT'

报错说的是：要导出的 target 身上有个 INTERFACE_INCLUDE_DIRECTORIES 指向**源码树里的路径**，
而 install(EXPORT) 拒绝把它写进交付给别人的文件 —— 那个路径只存在于你这台机器上。

同树编译阶段一个字错都没有（下面照样 Built target app）：
app 只需要「这个目录里有 vector.h」，它确实有。区别只在于 app 站在源码树里，
而 consumer 站在安装树里。

这就是 VECTOR_PLAIN_INCLUDE=OFF 那份写法存在的理由：
$<BUILD_INTERFACE:...> 给构建树里的人，$<INSTALL_INTERFACE:...> 给装完之后的人，
两者的路径基准完全不同，一条裸路径不可能同时满足。
TXT

# ============================================================================
banner "15. DESTDIR：打包脚本为什么能把库「装」进一个暂存目录"
# ============================================================================
runc "cmake --install build --prefix /usr/local --destdir \"$SB/stage\" 2>&1 | head -6"
echo '↑ 先给你看一个**不存在**的选项：这台机器的 cmake --install（CMake 4.2.3）没有 --destdir，'
echo '  用法列表里根本搜不到它。暂存目录要靠环境变量 DESTDIR。'
runc "DESTDIR=\"$SB/stage\" cmake --install build --prefix /usr/local 2>&1 | tail -4"
runc "find \"$SB/stage\" -type f | sort | sed \"s|$SB|<沙箱>|g\" | head -10"
runc "ls /usr/local/lib /usr/local/include 2>/dev/null | grep -iE 'vector|libutil' || echo '（/usr/local 里搜不到任何 vector 相关的东西 —— 确实一个字都没写进去）'"
echo '↑ --prefix 声明「最终会落在 /usr/local」，DESTDIR 把每一层路径前面再加一个暂存根目录。'
echo '  打 .deb/.rpm/tar 包就是这么干的：先 DESTDIR 到 staging，再由打包工具搬进真系统。'
echo '  最后那条 ls 证明：这一次运行没有往 /usr/local 写一个字节。'

printf '\n%s[全部跑完] 本章 README 引用的每一段输出都出自这一次运行。%s\n' "$B" "$R"
exit 0
