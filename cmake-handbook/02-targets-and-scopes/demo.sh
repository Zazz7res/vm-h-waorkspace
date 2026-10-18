#!/usr/bin/env bash
# ============================================================================
# 第 02 章《target 与作用域：PUBLIC / PRIVATE / INTERFACE》—— 整章证据一键复现
#
#   bash demo.sh          跑完自动删沙箱（默认）
#   bash demo.sh --keep   保留沙箱并打印路径，进去自己接着改着玩
#   bash demo.sh > out.txt   重定向自动关颜色，得到纯文本
#
# 本章的看点：同一份源码，只把 CMakeLists 里两个“作用域关键字”拨一下，
# 就能让编译**从成功变成失败**、让程序**从有标签变成没标签**。
# 每一次拨动都现场跑一遍，报错原文全部是真机输出。
#
# 安全说明：全部构建在 mktemp 沙箱里；不碰仓库那 56 节代码；不写全局配置；不联网。
# ============================================================================

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

set -u            # 故意不加 -e：本章要故意制造编译失败
export LC_ALL=C.UTF-8 LANG=C

if [ -t 1 ]; then B=$'\033[1m'; C=$'\033[36m'; R=$'\033[0m'; else B=''; C=''; R=''; fi

HERE=$(cd "$(dirname "$0")" && pwd)
SB=$(mktemp -d "${TMPDIR:-/tmp}/cmake02-XXXXXX")

banner() { printf '\n%s══════ %s %s\n' "$B" "$1" "══════════════════════════════$R"; }
runc() {
    printf '\n%s$ %s%s\n' "$C" "$1" "$R"
    bash -c "$1" 2>&1
    printf '[退出码 %d]\n' "$?"
}
# 只把某个 target 的三条关键设置打出来，这是本章反复使用的“物证”动作
ev() {   # ev <build目录> <target名>
    printf '\n--- %s/%s ---\n' "$1" "$2"
    grep -E "C_INCLUDES|C_DEFINES|C_FLAGS" "$1/CMakeFiles/$2.dir/flags.make"
}

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  继续玩： cd "%s/proj"      看完删： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# ============================================================================
banner "1. 工程长什么样：3 个库/工具 target + 2 个可执行 target"
# ============================================================================
mkdir -p "$SB/proj"
cp -R "$HERE/CMakeLists.txt" "$HERE/lib" "$HERE/util" "$HERE/app" "$HERE/tools" "$SB/proj/"
cd "$SB/proj" || exit 1
runc 'find . -type f | sort'
cat <<'TXT'

依赖方向（箭头 = “用到了”）：

    app ──┐
          ├──► vector ──► util          strict_flags（纯 INTERFACE，只有编译选项）
    vstats┘                    ▲                              │
                               └──── vector 也链接了它 ────────┘

    · util    静态库，暴露 util.h，还带一个编译期宏 UTIL_TAG
    · vector  静态库，暴露 vector.h，源码里 #include "util.h" 并调用 u_clamp
    · app     只 #include "vector.h"
    · vstats  两个头都 include，并且用 #ifdef 探测 UTIL_TAG
TXT

# ============================================================================
banner "2. 默认配置 configure：开关都留在 PUBLIC，顺便看 CMake 的 target 清单"
# ============================================================================
runc 'cmake -S . -B build'
echo '↑ “实验开关”那行是 message(STATUS ...) 打的；“CMake target:”那 5 行是把目录属性'
echo '  BUILDSYSTEM_TARGETS 取出来遍历的（注意：属性名不是 TARGETS，写错就是一条 CMake Error）。'
echo '  注意 util / vector / strict_flags 也是 target，虽然它们不会生成可执行文件。'

# ============================================================================
banner "3. 构建 + 运行：先拿到“全 PUBLIC”的基线"
# ============================================================================
runc 'cmake --build build'
runc './build/app'
runc './build/vstats'
echo '↑ 基线记在这里：vstats 打印出了“库标签：util-v1”。第 9 节我们只用降级一个编译定义，'
echo '  就能让这一行悄悄变成 <没拿到 UTIL_TAG>，而且编译一个字都不报错。'

# ============================================================================
banner "4. 物证：每个 target 的编译设置分别落在 flags.make 里"
# ============================================================================
ev build util
ev build vector
ev build app
ev build vstats
cat <<'TXT'

读这四张物证，作用域规则就已经讲完了一半：

    · util 的 -I.../util 和 -DUTIL_TAG 出现在 vector / app / vstats 里 → 它们是 PUBLIC，会传染
    · vector 的 -DBUILDING_VECTOR=1 只出现在 vector 自己里 → 它是 PRIVATE，不传染
    · -Wall -Wextra -Wshadow 出现在 vector / app / vstats 里 → 来自 INTERFACE 库 strict_flags 的传染
    · util 的 C_FLAGS 是**空的** → 它谁都没链接，没有任何传染来源，警告也就没加上
TXT
echo '# 但上面四行里搜不到 -std，尽管 CMakeLists 明明写了 target_compile_features(vector PUBLIC c_std_11)。查一下：'
runc "grep -h '^C_FLAGS' build/CMakeFiles/*.dir/flags.make | sort -u"
runc 'gcc -dM -E -x c /dev/null | grep __STDC_VERSION__'
echo '# 本机 gcc 15 的默认方言已经是 C23 —— “至少 C11”自动满足，于是 CMake 一个 -std 都不加。'
echo '# 对照实验：把第 01 章那种“硬指定标准”（set(CMAKE_C_STANDARD 11)）塞进一份新拷贝，-std 立刻出现：'
mkdir -p "$SB/std"
cp -R "$HERE/CMakeLists.txt" "$HERE/lib" "$HERE/util" "$HERE/app" "$HERE/tools" "$SB/std/"
cd "$SB/std" || exit 1
runc "sed -i '/^project(scopes_demo/a set(CMAKE_C_STANDARD 11)' CMakeLists.txt && grep -n -A1 '^project' CMakeLists.txt"
runc 'cmake -S . -B build | grep -E "Configuring done|Build files"'
runc "grep -h '^C_FLAGS' build/CMakeFiles/*.dir/flags.make | sort -u"
cd "$SB/proj" || exit 1
cat <<'TXT'

    → target_compile_features 表达的是**最低要求**（“至少 C11”），能被默认满足就不发 flag；
      set(CMAKE_C_STANDARD) 是**硬指定**，永远发 -std=gnuXX。
      这就是为什么本章推荐前者：它只声明“我需要这个特性”，把“用什么 flag 满足”交给 CMake。
TXT
runc 'cat build/CMakeFiles/vstats.dir/link.txt'
echo '↑ link.txt 是另一个物证：链接命令里能看到 libvector.a 和 libutil.a —— util 是被“传染”进来的。'

# ============================================================================
banner "5. CMake 的 target ≠ Makefile 的 target（第 01 章埋的坑，这里收）"
# ============================================================================
runc 'cmake --build build --target help'
echo '↑ make 那边有 clean / all / rebuild_cache 这些动作名，还有 util/util.o 这种文件名 target；'
echo '  而 CMake 侧只有 app / vstats / util / vector / strict_flags 五个。别把两个“target”混为一谈。'

# ============================================================================
banner "6. 实验①：把头目录从 PUBLIC 改成 PRIVATE —— 消费者当场编不过"
# ============================================================================
runc 'cmake -S . -B build-p -DVEC_INCLUDE_SCOPE=PRIVATE'
runc 'cmake --build build-p'
echo '↑ 关键看点：vector 自己编**成功**了（它用的是 #include "vector.h"，同目录引号包含，本来就不需要 -I）；'
echo '  崩的是 app —— vstats 同理，只是 make 在 app 那里就停了，没轮到它。'
echo '  消费者在不同目录，只能靠传染过来的 -I 才找得到 vector.h：这就是 PUBLIC 存在的唯一理由。'
ev build-p vector
ev build-p app
echo '↑ 对比第 4 节：vector.dir 里 -Ilib 还在，app.dir 里消失了。PRIVATE 的含义就是“只给我自己”。'

# ============================================================================
banner "7. 实验②：把 vector→util 改成 INTERFACE —— 这次崩的是 vector 自己"
# ============================================================================
runc 'cmake -S . -B build-i -DVEC_DEP_SCOPE=INTERFACE | grep -E "实验开关|Configuring done|Build files"'
runc 'cmake --build build-i'
echo '↑ INTERFACE 的方向和直觉相反：**只给消费者，不给正在编译的自己**。'
echo '  所以 vector.c 里那句 #include "util.h" 拿不到 -I，最先崩的就是 vector 本身。'

# ============================================================================
banner "8. 实验③：改成 PRIVATE —— 消费者拿不到使用要求，但链接行照带不误"
# ============================================================================
runc 'cmake -S . -B build-dp -DVEC_DEP_SCOPE=PRIVATE | grep -E "实验开关|Configuring done|Build files"'
runc 'cmake --build build-dp'
echo '↑ 两个层次分开看：'
ev build-dp vector
ev build-dp vstats
runc 'cat build-dp/CMakeFiles/vstats.dir/link.txt'
echo '↑ 注意最后一行：PRIVATE 情况下 link.txt 里**仍然**有 libutil.a ——'
echo '  因为静态库要把符号闭合，链接这件事 CMake 会自动兜住；'
echo '  PRIVATE 真正砍掉的是“使用要求”（-I 和 -D），所以崩在编译期，不是链接期。'

# ============================================================================
banner "9. 软失败：只把 UTIL_TAG 变成 PRIVATE —— 编译照样过，程序行为变了"
# ============================================================================
mkdir -p "$SB/soft"
cp -R "$HERE/CMakeLists.txt" "$HERE/lib" "$HERE/util" "$HERE/app" "$HERE/tools" "$SB/soft/"
cd "$SB/soft" || exit 1
echo '# 只改一行：util 的 UTIL_TAG 从 PUBLIC 降级成 PRIVATE（头目录仍然 PUBLIC）'
runc "sed -i 's/^target_compile_definitions(util PUBLIC UTIL_TAG/target_compile_definitions(util PRIVATE UTIL_TAG/' CMakeLists.txt && grep -n 'UTIL_TAG' CMakeLists.txt"
runc 'cmake -S . -B build -DVEC_DEP_SCOPE=PUBLIC | grep -E "Configuring done|Build files"'
runc 'cmake --build build | tail -3'
runc './build/vstats'
echo '↑ 全绿、退出码 0、没有任何 warning，只是 vstats 打印的“库标签”变成了 <没拿到 UTIL_TAG>。'
echo '  这就是使用要求缺失最阴的一种形式：**不报错，悄悄换行为**。'
echo '  所以 vstats.c 里用 #ifdef 探测它只是“演示用”的宽容写法；真实项目里宁可让它编译失败。'

# ============================================================================
banner "10. 开关要校验：把值打错一个字母会发生什么"
# ============================================================================
runc 'cmake -S . -B build-bad -DVEC_DEP_SCOPE=POBLIC'
echo '↑ 这是我们自己写的 message(FATAL_ERROR) 在兜底。没有那段校验的话，'
echo '  target_link_libraries(vector POBLIC util) 会报一个和“你打错字”毫无关系的错，最难查。'
printf '\n# 顺手看一眼：非法值那次 configure 有没有留下垃圾目录？\n'
runc 'ls build-bad 2>/dev/null || echo "（没有：FATAL_ERROR 让它在生成构建文件之前就停了）"'
echo '↑ 实情是：留下了半截目录。CMakeCache.txt 和 CMakeFiles/ 在报错前就写下去了，只是没有 Makefile。'
echo '  所以 FATAL_ERROR 并不能帮你保持目录干净，CI 里失败后要么整个删掉构建目录，要么每次用新目录。'

# ============================================================================
banner "11. INTERFACE 库能不能带源码？—— 一个“不报错但也不干活”的坑"
# ============================================================================
echo '# 新手常以为 INTERFACE 库就是“普通库少写点东西”，于是把 .c 也列进去。真跑一次：'
runc "printf 'add_library(bad_iface INTERFACE lib/vector.c)\n' >> CMakeLists.txt && cmake -S . -B build-iface-err | tail -4"
runc 'cmake --build build-iface-err 2>&1 | grep -E "bad_iface|Built target"'
runc 'find build-iface-err -name "*bad*"'
runc 'ls -A build-iface-err/CMakeFiles/bad_iface.dir; echo "（上面全是 CMake 自己的记账文件，没有一个是 .o）"'
runc 'find build-iface-err -name "vector.c.o"'
cat <<'TXT'

→ 结论和直觉相反，而且这条**不报错**：
  ① configure 成功；② make 还煞有介事地报告 “[100%] Built target bad_iface”；
  ③ 但 bad_iface.dir 里除了记账文件没有任何编译产物，列进去的 lib/vector.c 也没被它编第二次
  （find 只找到 vector.dir 那一份 vector.c.o，那是 vector 自己编的）。
  这就是比报错更糟的一类坑：**它假装成功，其实什么都没干**。
  INTERFACE 库不产生物理文件，它的正当用途只有三种：纯头文件库、一组编译选项/定义的打包
  （本章的 strict_flags 就是）、一组依赖名的打包。想让源码参与构建，该用 STATIC 或 OBJECT。
TXT
runc 'grep -n "target_compile_features(vector PUBLIC c_std_11)" CMakeLists.txt'
runc 'cmake -S . -B build-fe -DVEC_DEP_SCOPE=PUBLIC >/dev/null && grep -h "^C_FLAGS" build-fe/CMakeFiles/app.dir/flags.make'
echo '↑ 顺带验证一句 C_FLAGS 的来源：app 的警告选项确实是从 vector PUBLIC strict_flags 传染来的，'
echo '  而 app.dir/flags.make 里没有 -std —— 与第 4 节对照实验的结论一致。'

printf '\n%s[全部跑完] 本章 README 引用的每一段输出都出自这一次运行。%s\n' "$B" "$R"
exit 0
