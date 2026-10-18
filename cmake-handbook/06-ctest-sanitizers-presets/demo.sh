#!/usr/bin/env bash
# ============================================================================
# 第 06 章《CTest + Sanitizer + CMakePresets：把旋钮固化成一条命令》整章证据
#
#   bash demo.sh             跑完自动删沙箱（默认）
#   bash demo.sh --keep      保留沙箱，自己进去接着拨开关
#   bash demo.sh > out.txt   重定向自动关颜色，拿纯文本
#
# 本章只回答一个问题：**"我这份工程现在到底算通过了没有"** 由谁说了算。
#   CTest       —— 用例名单、六种裁判、ctest 的退出码
#   Sanitizer   —— ASan/LSan/UBSan 怎么挂进 target，三种挂错的现场
#   Presets     —— 把上面所有 -D 变成三个字的 preset 名
#
# 每一节都是真跑：命令原样打印、输出原样抓下来、退出码一个都不藏。
#
# 安全说明：
#   * 全程在 mktemp 沙箱里。仓库目录（含那份 56 节的根 CMakeLists）只读不写，
#     第 29 节末尾用 find 自证仓库里没落任何构建产物。
#   * 全程不联网：本章不需要任何外部依赖。
#   * 只读工具：cmake/make/gcc/ldd/grep/python3/du，都不需要 root。
#   * 不写任何全局配置：cmake 的用户级文件一个都不碰；preset 全在沙箱工程里。
#   * 输出里不出现本机登录名：要读仓库根的真实文件时用 runc_at（屏幕上只留相对路径），
#     preset 演示一律用脚本自造的中立环境变量（绝不写 $penv{USER}、$penv{HOME}）。

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
SB=$(mktemp -d "${TMPDIR:-/tmp}/cmake06-XXXXXX")
PROJ="$SB/ch06"
BD="$SB/b"

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
# only <正则> <命令>：只留感兴趣的行，过滤条件同时打在提示行里
only() {
    local pat="$1"; shift
    printf '\n%s$ %s | grep -E "%s"%s\n' "$C" "$*" "$pat" "$R"
    bash -c "set -o pipefail; $*" 2>&1 | grep -E -- "$pat"
    printf '[退出码 %d]\n' "$?"
    return 0
}
# show <正则> <文件>：抓文件里匹配的行（带行号）
show() { printf '\n%s$ grep -nE "%s" %s%s\n' "$C" "$1" "$2" "$R"; grep -nE -- "$1" "$2" 2>&1; }
# ★ 注意：runc 是用 bash -c 跑字符串的，**子 shell 看不见本脚本的函数**。
#   第一版这里有个 fmk() 辅助函数，写进 runc "… && fmk sanorder oob" 之后
#   屏幕上得到的是 bash: line 1: fmk: command not found（退出码 127）。
#   所以要 grep 文件就老老实实把 grep 写全 —— 这个坑现在写成明文挡在这儿。

# runc_at <目录> <命令>：先 cd 进那个目录再跑，屏幕上只留目录名（basename）+ 相对命令。
#   要读仓库根那种真实文件时用它：命令行里不出现绝对路径，本机登录名一个都不留。
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

# ---------------------------------------------------------------------------
banner "00 准备：把章节工程复制进沙箱（仓库目录只读，不落任何产物）"
# ---------------------------------------------------------------------------
mkdir -p "$BD"
# 只拷工程素材：README.md 和 demo.sh 不是 CMake 的输入
mkdir -p "$PROJ"
cp "$HERE/CMakeLists.txt" "$HERE/CMakePresets.json" "$PROJ/"
cp -r "$HERE/src" "$HERE/include" "$HERE/variants" "$PROJ/"
runc "cmake --version | head -1 && gcc --version | head -1 && make --version | head -1"
runc "ls -1 $PROJ && echo '(variants/ 里是六份故意写坏的工程，第 2、3、11、13、14、15 节各用一份)'"

# ---------------------------------------------------------------------------
banner "01 CTest 的最小闭环：注册之后，名单到底落在哪个文件里"
# ---------------------------------------------------------------------------
# 先立一个事实：CTest 没有数据库，它的"名单"就是构建目录里一个 CMake 脚本。
runc "cmake -S $PROJ -B $BD/main 2>&1 | tail -6"
runc "ls -1 $BD/main/CTestTestfile.cmake && echo '(配置期就生成了，构建之前它已经存在)'"
runc "cmake --build $BD/main -j4 >/dev/null && ctest --test-dir $BD/main -N"
note "ctest -N 只列名单不执行；它读的正是上面那个文件。"
note "★ 注意这条命令里有个 >/dev/null 的构建 —— 不是为了让测试跑，是为了让 -N 干净："
note "  没构建过时，-N 会给**每一个**用例打印一坨「Could not find executable / Looked in the following places」，"
# 故意留一条不构建的对照，让读者看见那坨噪音长什么样（第 5 节还会正面用它一次）
runc "rm -rf $BD/noisy && cmake -S $PROJ -B $BD/noisy >/dev/null 2>&1 && ctest --test-dir $BD/noisy -N 2>&1 | grep -c 'Could not find executable'"
note "↑ 同一个 -N，只是没构建：这一行数字就是它会刷屏的行数。"

# ---------------------------------------------------------------------------
banner "02 陷阱一：写了 add_test，但没写 enable_testing()"
# ---------------------------------------------------------------------------
# variants/no-enable-testing：配置一路绿灯、构建一路绿灯，测试名单压根不存在。
runc "cmake -S $PROJ/variants/no-enable-testing -B $BD/noenable 2>&1 | tail -3"
runc "ls -1 $BD/noenable/CTestTestfile.cmake ; echo '(上面是 ls 的报错：这个文件根本没生成)'"
runc "ctest --test-dir $BD/noenable 2>&1"
note "★ 记住屏幕上真正出现的是这句：No tests were found!!!，退出码 0 —— 很多人以为会看到"
note "  「No test configuration file found!」，那是别的场景（下面两条探针把边界找出来）。"
runc "cd $BD/noenable && ctest 2>&1"
runc "ctest --test-dir $SB/这个目录根本不存在 2>&1"
note "CI 只看退出码的话，第 1 条这条路就是「配置成功、构建成功、测试通过」—— 但它一个测试都没跑。"

# ---------------------------------------------------------------------------
banner "03 陷阱二：enable_testing() 写了，但一条 add_test 都没有"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ/variants/zero-tests -B $BD/zerotest >/dev/null && echo '(配置成功)'"
runc "ls -1 $BD/zerotest/CTestTestfile.cmake && echo '(这回文件在，但里面一条用例都没有)'"
runc "ctest --test-dir $BD/zerotest 2>&1"
note "第二根陷阱：文件找到了、ctest 也跑了，No tests were found!!! —— 退出码还是 0。"
note "★ 这行屏幕和第 2 节**一字不差**：忘了总闸、和注册了零条用例，长得完全一样。"
note "区分它们只能靠 CTestTestfile.cmake 在不在（两节第一条 ls 的差别就是全部证据）。"
note "这两节合起来是本章最贵的一个认知：「ctest 退 0」至少有三种意思，屏幕上得自己分清。"

# ---------------------------------------------------------------------------
banner "04 解剖 CTestTestfile.cmake：裸 target 名和 \$<TARGET_FILE:> 最后长一样"
# ---------------------------------------------------------------------------
show 'add_test' "$BD/main/CTestTestfile.cmake"
note "上面每行开头的 _BACKTRACE_TRIPLES 记的是「哪个文件的第几行注册的」—— 排查重复注册/漏注册就靠它。"
runc "grep -c 'add_test' $BD/main/CTestTestfile.cmake && echo '(名单里一共几条用例)'"

# ---------------------------------------------------------------------------
banner "05 ctest 不替你构建：只配置不 make，ctest 会说什么"
# ---------------------------------------------------------------------------
runc "rm -rf $BD/nobuild && cmake -S $PROJ -B $BD/nobuild >/dev/null 2>&1 && echo '(只配置，不构建)'"
runc "ctest --test-dir $BD/nobuild -R smoke_ok 2>&1"
note "Could not find executable ... 这条不是「测试失败」，是「要测的东西还没造出来」。"
note "正确顺序永远是三条：cmake -S -B → cmake --build → ctest。第 22 节用 preset 把这三条钉死。"

# ---------------------------------------------------------------------------
banner "06 第一次真跑：构建完，看 ctest 的汇总和退出码"
# ---------------------------------------------------------------------------
runc "cmake --build $BD/main -j4 2>&1 | tail -3"
runc "ctest --test-dir $BD/main 2>&1"
note "★ 汇总里的三行：N% tests passed / The following tests FAILED: / Errors while running CTest。"
note "退出码 8 是 ctest 的「有测试没过」。它不等于配置错误（那是 cmake 的事），也不等于段错误。"

# ---------------------------------------------------------------------------
banner "07 挑着跑：-N -R -L 三种筛子，以及 -V / --output-on-failure"
# ---------------------------------------------------------------------------
runc "ctest --test-dir $BD/main -N -L asan"
runc "ctest --test-dir $BD/main -R 'env_' 2>&1 | grep -E 'Test #|tests passed'"
runc "ctest --test-dir $BD/main -L env -V 2>&1 | grep -E 'Test command|CH06_TAG|Passed|Failed'"
note "-L 筛的是 LABELS 属性；-R 筛的是名字正则。两条筛子叠用时取交集。"

# ---------------------------------------------------------------------------
banner "08 裁判一：PASS_REGULAR_EXPRESSION —— 退出码之外的第二把尺子"
# ---------------------------------------------------------------------------
# wrong 退 0（默认裁判会放它过关），但输出是 43 不是 42。
runc "$BD/main/wrong; echo \"程序自己退了 \$?，注意是 0\""
runc "ctest --test-dir $BD/main -R regex_wrong --output-on-failure 2>&1"
note "***Failed Required regular expression not found. Regex=[结果 = 42\n] —— 它把要求的模式原样打给你。"
note "同一条命令换成默认裁判（smoke_ok）就是 Passed：区别只在属性。"

# ---------------------------------------------------------------------------
banner "09 裁判二：WILL_FAIL 只认退出码，不认输出"
# ---------------------------------------------------------------------------
runc "ctest --test-dir $BD/main -R known_defect -V 2>&1 | grep -E 'Test command|Result|Passed|Failed'"
note "nonzero 打印完退 1 → 挂了 WILL_FAIL TRUE 就叫「通过」。这是「已知缺陷」当哨兵的用法。"
# 反例：把 WILL_FAIL 挂到本来退 0 的 ok 上
runc "cmake -S $PROJ -B $BD/willfail -DCH06_MISUSE_WILL_FAIL=ON >/dev/null && cmake --build $BD/willfail --target ok -j4 >/dev/null && echo '(挂上反面开关，只重建 ok)'"
runc "ctest --test-dir $BD/willfail -R ok_marked_willfail --output-on-failure 2>&1"
note "ok 明明跑得好好的、退 0，却因「我期望它失败」被判 Failed。WILL_FAIL 完全不看输出内容。"

# ---------------------------------------------------------------------------
banner "10 裁判三：TIMEOUT —— 到点就掐，秒数来自一个缓存变量"
# ---------------------------------------------------------------------------
runc "ctest --test-dir $BD/main -R slow_times_out --output-on-failure 2>&1"
note "***Timeout 1.00 sec：1 是 CH06_TIMEOUT 的值。第 22 节 preset 把它改成 2 —— 程序睡 3 秒，所以照样超时；"
note "本节第二条把值放宽到 4，它才终于 Passed。超时这件事「值多大」和「程序多慢」必须分开看。"
runc "cmake -S $PROJ -B $BD/to2 -DCH06_TIMEOUT=4 >/dev/null && cmake --build $BD/to2 --target slow -j4 >/dev/null && ctest --test-dir $BD/to2 -R slow_times_out --output-on-failure 2>&1 | tail -6"
note "把超时放宽到 4 秒，同一条用例就从 ***Timeout 变成 Passed —— 它是「没测出问题」还是「跑得慢」，只有对比才看得出来。"

# ---------------------------------------------------------------------------
banner "11 属性写错测试名：唯一会**当场响**的那类 CTest 错误"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ/variants/properties-before-add-test -B $BD/badprop 2>&1 | tail -6"
note "set_tests_properties 找不到用例名 → Configuring incomplete, errors occurred!（退出码 1）。"
note "对比第 2、3 节：写错名字会响，忘了总闸不会响 —— 本章反复回到这个不对称。"

# ---------------------------------------------------------------------------
banner "12 add_test 里的 \${PWD} 是配置期展开的，不是测试期的事"
# ---------------------------------------------------------------------------
runc "ctest --test-dir $BD/main -R configure_time_expand -V 2>&1 | grep -E 'Test command|配置期展开'"
runc "grep -n 'configure_time_expand' $BD/main/CTestTestfile.cmake"
note "方括号里那个值是**配置那一刻**就定下来的字符串，测试跑的时候 CMake 已经不在了。"
note "要测试期的当前目录，用 WORKING_DIRECTORY 属性（本章没演，见 README「本章没演」）。"

# ---------------------------------------------------------------------------
banner "13 反面教材：set_target_properties(SANITIZER \"address\") 到底做了什么"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ/variants/san-property -B $BD/sanprop 2>&1 | grep -E '读回来|Configuring done'"
runc "grep -E '^C_FLAGS' $BD/sanprop/CMakeFiles/oob.dir/flags.make; echo '(↑ 这就是 ASan「开没开」唯一可信的地方)'"
runc "grep -c fsanitize $BD/sanprop/CMakeFiles/oob.dir/flags.make"
note "↑ 命中 0 行、退出码 1：这里的 [退出码 1] 是 grep 自己报「没找到」，不是 CMake 出错。"
runc "cmake --build $BD/sanprop -j4 >/dev/null && ctest --test-dir $BD/sanprop 2>&1 | tail -3"
note "越界写的那个程序，用例是绿的 —— 因为 ASan 从头到尾没进去过。"
note "★ 未知属性名 CMake 照单全收：连「随便写的属性名」都能 set 能 get。属性读写成功 ≠ 生效。"

# ---------------------------------------------------------------------------
banner "14 反面教材：只加编译期，忘了链接期 → undefined reference to '__asan_init'"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ/variants/san-compile-only -B $BD/sanlink >/dev/null && echo '(配置成功：注意，配置期一点都不报错)'"
runc "cmake --build $BD/sanlink 2>&1 | tail -20"
note "看到 __asan_ 打头的未定义符号，结论只有一个：-fsanitize 加在编译了，没加在链接。"
note "编译期插检查代码，链接期才把 asan 运行时带进来 —— 这是两半的活，缺半边就停在链接。"

# ---------------------------------------------------------------------------
banner "15 反面教材：add_compile_options 写在 add_executable 之后 → 等于没写"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ/variants/san-order -B $BD/sanorder >/dev/null && grep -E '^C_FLAGS' $BD/sanorder/CMakeFiles/oob.dir/flags.make"
note "C_FLAGS 里连 -fsanitize 的影子都没有：add_executable 在它自己被调用的那一刻拍快照，之后的目录属性进不去。"
runc "cmake --build $BD/sanorder -j4 >/dev/null && $BD/sanorder/oob; echo \"程序退出码 \$?（越界写了，但它不敢说）\""
runc "ctest --test-dir $BD/sanorder 2>&1 | tail -3"
note "「加了 ASan 的工程」和「没加 ASan 的工程」在这一步屏幕上完全一样，只有 flags.make 分开它们。"

# ---------------------------------------------------------------------------
banner "16 正确接法：编译期和链接期成对出现，两半都能取证"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ -B $BD/asan -DCH06_WITH_ASAN=ON 2>&1 | grep -E 'CH06|第 06 章'"
runc "grep -E '^C_FLAGS' $BD/asan/CMakeFiles/oob.dir/flags.make"
runc "grep -E '^C_FLAGS' $BD/main/CMakeFiles/oob.dir/flags.make; echo '(对照：同一份 CMakeLists、没开开关的那份)' "
runc "cat $BD/asan/CMakeFiles/oob.dir/link.txt"
note "★ 上半：-fsanitize=address -g -fno-omit-frame-pointer 落在编译标志里；"
note "  下半：link.txt 第一个非编译器参数就是 -fsanitize=address。两条都命中，ASan 才算真装上。"
note "-g 不是可选的：没有它 ASan 只能报到函数名，有它才报到源码行号。"

# ---------------------------------------------------------------------------
banner "17 ASan 真的抓到了：同一批用例，开关一开就从绿变红"
# ---------------------------------------------------------------------------
runc "cmake --build $BD/asan -j4 2>&1 | tail -2"
runc "ctest --test-dir $BD/asan -R oob_asan --output-on-failure 2>&1"
note "heap-buffer-overflow 那段是 ASan 的原话，不是 CMake 的；ctest 只是把程序 stderr 转述出来。"
runc "ctest --test-dir $BD/asan -R leak_asan --output-on-failure 2>&1 | grep -E 'LeakSanitizer|SUMMARY|Direct leak|Failed|ok$'"
note "leak 这个程序没开 ASan 时退 0（第 6 节里它是绿的）。开了以后 LeakSanitizer 在进程退出时体检并改写退出码。"
runc "ctest --test-dir $BD/main -R 'leak_asan|oob_asan|ub_asan' 2>&1 | grep -E 'Test #|tests passed'"
note "★ 上面这三条用例在**没挂 sanitizer** 的构建里全绿 —— 「测试全过」和「内存干净」是两件事。"

# ---------------------------------------------------------------------------
banner "18 libasan 到底链没链进来：ldd 的对照"
# ---------------------------------------------------------------------------
runc "ldd $BD/asan/oob | grep -i asan"
note "↑ 动态版：运行时必须有 libasan.so.8。开发机上没问题，部署到没装 sanitizer 的机器上就是「文件找不到」。"
runc "cmake -S $PROJ -B $BD/asanstatic -DCH06_WITH_ASAN=ON -DCH06_ASAN_STATIC=ON >/dev/null && cmake --build $BD/asanstatic --target oob -j4 >/dev/null && ldd $BD/asanstatic/oob | grep -ci asan"
note "↑ 静态版：命中 0 条（所以 [退出码 1] 是 grep 报「没找到」）。asan 运行时已经焊进可执行文件本身。"
runc "grep -c static-libasan $BD/asanstatic/CMakeFiles/oob.dir/link.txt"
note "本机两种都能跑（共享库在 /usr/lib/x86_64-linux-gnu/）。选哪个是部署问题，不是能不能编译的问题。"

# ---------------------------------------------------------------------------
banner "19 UBSan 最阴的一课：它报了错，测试还是绿的"
# ---------------------------------------------------------------------------
runc "cmake -S $PROJ -B $BD/ubrecover -DCH06_UBSAN=ON -DCH06_UBSAN_STOP=OFF >/dev/null && cmake --build $BD/ubrecover --target ub -j4 >/dev/null && $BD/ubrecover/ub; echo \"程序退出码 \$?\""
runc "ctest --test-dir $BD/ubrecover -R ub_asan --output-on-failure 2>&1 | grep -E 'Test #|tests passed'"
note "runtime error 打印了，但 UBSan 默认是 recover 模式：报完继续跑、退出码不变 → ctest 判 Passed。"
runc "cmake -S $PROJ -B $BD/ubstop -DCH06_UBSAN=ON >/dev/null && cmake --build $BD/ubstop --target ub -j4 >/dev/null && $BD/ubstop/ub; echo \"程序退出码 \$?\""
runc "ctest --test-dir $BD/ubstop -R ub_asan --output-on-failure 2>&1 | grep -E 'Test #|tests passed|FAILED'"
note "加上 -fno-sanitize-recover=all，同一条用例立刻变红。UBSan 不加这条，在 CI 上约等于只打日志。"

# ---------------------------------------------------------------------------
banner "20 三条命令的完整验收：配置(asan) → 构建 → ctest"
# ---------------------------------------------------------------------------
runc "ctest --test-dir $BD/asan 2>&1 | tail -8"
note "★ 挂了 ASan 之后汇总从「2 条红」变成「4 条红」：多出来的是 leak 和 oob —— 它们一直都在，只是以前没人查。"

# ---------------------------------------------------------------------------
banner "21 preset 入门：先让 CMake 把名单念给你听"
# ---------------------------------------------------------------------------
runc "cd $PROJ && cmake --list-presets"
note "先记两条规则，第 24 节当场对质：hidden:true 的不出现；condition 不成立的不出现。"
note "所以这张表里的条数一定小于等于文件里写的条数 —— 对不上时别怀疑 CMake，先数文件。"
runc "cd $PROJ && cmake --list-presets=build && cmake --list-presets=test"
runc "cd $PROJ && cmake --list-presets=all"

# ---------------------------------------------------------------------------
banner "22 三条命令对齐：--preset / --build --preset / ctest --preset"
# ---------------------------------------------------------------------------
runc "cd $PROJ && cmake --preset dbg 2>&1 | tail -8"
runc "ls -1 $PROJ/out && echo '(binaryDir 由 preset 里的 \${sourceDir}/out/\${presetName} 决定，命令行一个 -B 都没敲)'"
runc "cd $PROJ && cmake --build --preset dbg 2>&1 | tail -3"
runc "cd $PROJ && ctest --preset dbg 2>&1 | tail -8"
note "ctest --preset dbg 用的是 test preset 里的 outputOnFailure，所以不用手敲 --output-on-failure。"
note "CH06_TIMEOUT 在这里是 2：慢用例睡 3 秒，所以它仍然 ***Timeout —— 值来自 preset 而不是 CMakeLists 默认值。"

# ---------------------------------------------------------------------------
banner "23 覆盖顺序：命令行 -D 赢 preset 的 cacheVariables，-B 赢 preset 的 binaryDir"
# ---------------------------------------------------------------------------
runc "cd $PROJ && cmake --preset dbg -DCH06_GREETING='命令行赢了' 2>&1 | grep -E 'GREETING'"
runc "grep -m1 '^CH06_GREETING' $PROJ/out/dbg/CMakeCache.txt && echo '(↑ 缓存里存的就是命令行那个值 —— 手敲的 -D 盖住了 preset 的 cacheVariables)'"
runc "cd $PROJ && cmake --preset dbg -B $PROJ/out/elsewhere >/dev/null && grep -m1 CMAKE_CACHEFILE_DIR $PROJ/out/elsewhere/CMakeCache.txt"
runc "grep -m1 CMAKE_CACHEFILE_DIR $PROJ/out/dbg/CMakeCache.txt && echo '(↑ 对照：out/dbg 那份缓存一个字没动，这次 configure 压根没进它)'"
note "★ 记忆点：手敲的永远赢，preset 只是「你没敲时用什么」。所以临时调试可以 --preset dbg -DX=1，不用另存一份 preset。"

# ---------------------------------------------------------------------------
banner "24 condition 与 hidden：谁会被藏起来，藏到哪去了"
# ---------------------------------------------------------------------------
runc "cd $PROJ && python3 -c \"import json;d=json.load(open('CMakePresets.json'));print('文件里写了这些：', ' '.join(p['name'] for p in d['configurePresets']))\""
runc "cd $PROJ && cmake --list-presets | grep -c '^  \\\"' && echo '(↑ 名单里几条 configure preset)'"
runc "cd $PROJ && cmake --list-presets=all | grep -c '^  \\\"' && echo '(↑ --list-presets=all 把 configure/build/test 三类都列，条数是三张表相加)'"
runc "cd $PROJ && cmake --list-presets=all | grep -c base"
note "↑ 上一条：命中 0 行、退出码 1 —— hidden 的那条在**任何**名单里都不出现，all 也不会把它放出来。"
runc "cd $PROJ && cmake --preset only-linux >/dev/null && echo '(condition 成立 → 这条在本机可用)'"
runc "cd $PROJ && cmake --preset base 2>&1 | grep -E 'Error|Available'"
note "★ 两个结论：hidden:true 不是「折叠」而是「不对外」—— 不进名单、也不能被点名，只给 inherits 用；"
note "  --list-presets=all 的 all 指的是三类名单都印，不是「连 hidden 一起印」。这两件事全靠上面两条命令分清。"

# ---------------------------------------------------------------------------
banner "25 preset 的 environment 段：它只管 ctest --preset 这条路，管不到你手敲的 ctest"
# ---------------------------------------------------------------------------
runc "cd $PROJ && cmake --preset tagged >/dev/null && cmake --build --preset tagged 2>&1 | tail -2"
runc "cd $PROJ && ctest --test-dir out/tagged -R env_ -V 2>&1 | grep -E 'env_demo: CH06_TAG|Test #'"
note "↑ 直接敲 ctest（不经过 preset）：preset 里那个 environment 段一点作用都没有，屏幕上是什么取决于当前 shell。"
runc "cd $PROJ && ctest --preset tagged -V 2>&1 | grep -E 'env_demo: CH06_TAG|Test #'"
note "↑ 换成 ctest --preset tagged：test preset 的 environment 生效了，而带 ENVIRONMENT 属性的那条仍打印属性里的值。"
runc "cd $PROJ && CH06_TAG='shell 里的值' ctest --preset tagged -V 2>&1 | grep -E 'env_demo: CH06_TAG|Test #'"
note "★ 三个来源同屏：CMakeLists 的 ENVIRONMENT 属性 > test preset 的 environment > 当前 shell。谁赢不是猜的，是这三条命令给出的。"
note "顺带一条：configure / build / test 是**三个各自独立的名单**，同名不互通 —— 所以第 22 节那三条命令的 preset 都得分别存在。"

# ---------------------------------------------------------------------------
banner "26 \$env{} 和 \$penv{}：同一个环境变量两种读法，读不到时是什么下场"
# ---------------------------------------------------------------------------
mkdir -p "$SB/penv"
cp "$PROJ/variants/presets/penv.json" "$SB/penv/CMakePresets.json"
cp "$PROJ/CMakeLists.txt" "$SB/penv/"
cp -r "$PROJ/src" "$PROJ/include" "$SB/penv/"
runc "cd $SB/penv && cmake --list-presets"
runc "cd $SB/penv && cmake --preset env-lookup 2>&1 | grep -E 'GREETING|Error'"
runc "cd $SB/penv && cmake --preset penv-lookup 2>&1 | grep -E 'GREETING|Error'"
runc "cd $SB/penv && CH06_GREETER='从环境里读到的问候' cmake --preset env-lookup 2>&1 | grep -E 'GREETING'"
runc "cd $SB/penv && CH06_GREETER='从环境里读到的问候' cmake --preset penv-lookup 2>&1 | grep -E 'GREETING'"
note "变量存在时两种读法给出同一个值；差别全在「变量不存在」那一半 —— 上面前两条命令就是答案。"
note "本章文档里不出现任何真实用户名：CH06_GREETER 是脚本自己造的中立值。\$penv{USER} 这类写法会把登录名印进构建目录，别用。"

# ---------------------------------------------------------------------------
banner "27 CMakeUserPresets.json：机器相关的东西不进版本库"
# ---------------------------------------------------------------------------
cat > "$PROJ/CMakeUserPresets.json" <<'EOF'
{
  "version": 3,
  "configurePresets": [
    {
      "name": "mine",
      "displayName": "我本机的临时开关（这个文件不该提交）",
      "inherits": "dbg",
      "cacheVariables": { "CH06_TIMEOUT": "8" }
    }
  ]
}
EOF
runc "cd $PROJ && ls -1 CMakePresets.json CMakeUserPresets.json && cmake --list-presets | grep -E 'mine|\"dbg\"'"
runc "cd $PROJ && cmake --preset mine 2>&1 | grep -E 'TIMEOUT|GREETING|Configuring done'"
note "两个文件同时存在时，CMake 把它们当一个预设池：user 文件里的 preset 能 inherits 公共文件里的。"
note "CMakeUserPresets.json 的定位等同于 .gitignore 里的本地文件 —— 版本库只留 CMakePresets.json。"

# ---------------------------------------------------------------------------
banner "28 preset 的七种报错原文（挑着抄，报错时对照用）"
# ---------------------------------------------------------------------------
runc "cd $PROJ && cmake --preset 不存在的名字 2>&1"
runc "mkdir -p $SB/nopresets && cp $PROJ/CMakeLists.txt $SB/nopresets/ && cd $SB/nopresets && cmake --preset dbg 2>&1"
for f in json-trailing-comma version-99 cyclic-inherit extra-field bad-condition; do
    mkdir -p "$SB/bad-$f"
    cp "$PROJ/CMakeLists.txt" "$SB/bad-$f/"
    cp -r "$PROJ/src" "$PROJ/include" "$SB/bad-$f/"
    cp "$PROJ/variants/presets/$f.json" "$SB/bad-$f/CMakePresets.json"
    runc "cd $SB/bad-$f && cmake --list-presets 2>&1 | tail -5"
done
note "★ 五种 JSON/schema 错法里只有 trailing-comma 是「JSON 解析失败」，其余四条都是 CMake 自己的校验语。"
note "cyclic 那条报的是 Cyclic preset inheritance for preset \"a\"；version 99 那条会顺便告诉你本机支持到几。"

# ---------------------------------------------------------------------------
banner "29 接到真工程：那份 56 节的根 CMakeLists 里，本章的东西占哪几行"
# ---------------------------------------------------------------------------
DSROOT=$(cd "${DS_ROOT:-$HERE/../..}" 2>/dev/null && pwd)   # 归一化成绝对路径，屏幕上不出现 ../..
if [ -f "$DSROOT/CMakeLists.txt" ]; then
    note "只读引用仓库根那份 CMakeLists（不 configure、不 build、不写它一个字节）："
    runc_at "$DSROOT" "grep -nE 'option\(DS_WITH_ASAN|enable_testing|add_test|target_link_options' CMakeLists.txt | head -8"
else
    note "没找到仓库根 CMakeLists（正常：demo 可以脱离那本教材单独跑）。"
fi

# 沙箱里复刻一个三节的小 DS，结构与真仓库一致：一个 helper 函数 + 每节一条 ctest + 一个 ASan 开关
mkdir -p "$SB/ds/01-array/static-array" "$SB/ds/02-linked-list/slist" "$SB/ds/03-stack/stack-array"
cat > "$SB/ds/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.20)
project(DsMini VERSION 0.1.0 LANGUAGES C)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(DS_WITH_ASAN "给每个可执行文件挂 AddressSanitizer + LeakSanitizer" OFF)
set(DS_WARNINGS -Wall -Wextra -Wshadow)

# 一节 = 一个可执行 + 一条 smoke 用例。规则只写一遍，新加一节漏不掉。
function(ds_add_leaf leaf_dir)
    string(REPLACE "/" "-" _target "${leaf_dir}")
    add_executable(${_target} "${CMAKE_CURRENT_SOURCE_DIR}/${leaf_dir}/leaf.c")
    target_compile_options(${_target} PRIVATE ${DS_WARNINGS})
    if(DS_WITH_ASAN)
        target_compile_options(${_target} PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
        target_link_options(${_target} PRIVATE -fsanitize=address)
    endif()
    add_test(NAME smoke_${_target} COMMAND ${_target})
endfunction()

enable_testing()
ds_add_leaf(01-array/static-array)
ds_add_leaf(02-linked-list/slist)
ds_add_leaf(03-stack/stack-array)
EOF

cat > "$SB/ds/01-array/static-array/leaf.c" <<'EOF'
#include <stdio.h>
/* 静态数组：不碰堆，所以它永远不该被 ASan 抓到。 */
int main(void) {
    int a[5] = {1, 2, 3, 4, 5};
    int s = 0;
    for (int i = 0; i < 5; i++) s += a[i];
    printf("static-array sum=%d\n", s);
    return 0;
}
EOF

cat > "$SB/ds/02-linked-list/slist/leaf.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
/* 单链表：节点头一个个 malloc 出来，就一定要有对应的 free。
   这一版故意只建不拆 —— 它退 0、输出正常、零警告，只有 LSan 看得见。 */
int main(void) {
    int n = 0;
    for (int i = 0; i < 5; i++) {
        int *node = malloc(sizeof(int));      /* 5 次 malloc */
        if (node == NULL) return 1;
        *node = i;
        n += *node;
        /* ★ 故意没有 free(node) */
    }
    printf("slist sum=%d\n", n);
    return 0;
}
EOF

cat > "$SB/ds/03-stack/stack-array/leaf.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
/* 数组栈：push 里 realloc，最后 pop 干净并 free —— 这一节该是干净的。 */
typedef struct { int *d; int n; int cap; } Stack;
int main(void) {
    Stack s = {0, 0, 0};
    for (int i = 0; i < 4; i++) {
        if (s.n == s.cap) { s.cap = s.cap ? s.cap * 2 : 4; s.d = realloc(s.d, (size_t)s.cap * sizeof(int)); }
        if (s.d == NULL) return 1;
        s.d[s.n++] = i * i;
    }
    int top = s.d[--s.n];
    free(s.d);
    printf("stack top=%d\n", top);
    return 0;
}
EOF

runc "cmake -S $SB/ds -B $BD/ds 2>&1 | tail -2 && cmake --build $BD/ds -j4 >/dev/null && ctest --test-dir $BD/ds 2>&1 | tail -3"
note "不开 ASan：三节全绿 —— 包括那节一直在漏的链表。这就是第 17 节那个道理在真工程形状上的复现。"
runc "cmake -S $SB/ds -B $BD/dsasan -DDS_WITH_ASAN=ON >/dev/null && cmake --build $BD/dsasan -j4 >/dev/null && ctest --test-dir $BD/dsasan --output-on-failure 2>&1 | tail -22"
note "★ 只多了一个 -DDS_WITH_ASAN=ON：用例名单一模一样，红的那条把漏了多少字节、在哪一行 malloc 全说了。"
note "这就是把本章接到真教材上的全部工作量：一个开关 + 一条 smoke 用例 + 一个函数把规则写一遍。"

# 自证：所有产物都在沙箱里，章节目录一个字节都没多
runc_at "$HERE" "find . -name CMakeCache.txt -o -name '*.o' -o -name CTestTestfile.cmake -o -type d -name out | wc -l"
note "↑ 0 = 章节目录里没有任何构建产物；上面 30 节写出来的东西全部在沙箱。"
runc "du -sh $SB"

# ---------------------------------------------------------------------------
banner "30 本章一页纸：命令、退出码、以及「谁在说话」"
# ---------------------------------------------------------------------------
cat <<'EOF'

  命令                        它到底做什么                      没做成功时屏幕上是什么
  ------------------------  --------------------------------  --------------------------
  cmake --preset dbg          按 preset 配置（含 binaryDir）    No such preset / schema 错，退 1
  cmake --build --preset dbg  构建 preset 绑定的那个目录          编译错误原样转发
  ctest --preset dbg          按 test preset 跑用例              汇总 + 退出码
  ctest --test-dir X -N       只列名单                           名单空 = 0 条，仍然退 0
  ctest --test-dir X          全跑                               全过退 0，有红退 8
  ctest -R 正则 / -L 标签      筛子（交集）                        一条都没筛中：No tests were found!!!，退 0

  读报错先认「谁在说话」：
    CMake Error                     —— 配置期的 CMake，看 -- Configuring incomplete
    undefined reference to '__asan_ —— 链接期，asan 运行时没链进来
    runtime error: / ERROR: Address —— 程序自己跑出来的，CMake 只是转述
    ***Failed / ***Timeout          —— ctest 的裁判结论，不是编译器
EOF

printf '\n%s全章跑完%s  共 30 节；沙箱：%s%s\n' "$B" "$R" "$SB" "$R"
