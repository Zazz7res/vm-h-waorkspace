# ============================================================================
# 第 06 章《CTest + Sanitizer + CMakePresets》的实验工程
#
# 前两章把旋钮都试过了（第 05 章：生成器 / 构建类型 / 缓存 / flags.make）。
# 这一章解决的是**下一个问题**：旋钮试完了，怎么固化成"一条命令就能验收"。
#
# 三件事，全在这一份工程里：
#   一 CTest            —— enable_testing / add_test / 六种裁判属性 / ctest 命令行
#   二 Sanitizer        —— ASan + LSan + UBSan 怎么正确挂到 target 上，以及三种挂错的现场
#   三 CMakePresets.json —— 把上面两件事变成 cmake --preset dbg / --build --preset / ctest --preset
#
# src/ 里七个程序就是七支探针，每支专门让某一种裁判露出本来面目：
#   ok.c       退 0、输出「结果 = 42」            —— 参照物，怎么测都该绿
#   wrong.c    退 0、但输出「结果 = 43」          —— 默认裁判（只看退出码）会放它过关
#   nonzero.c  退 1                              —— 给 WILL_FAIL 当正例
#   leak.c     漏 40 字节、退 0                   —— 只有 LeakSanitizer 抓得到
#   oob.c      堆上越界写、退 0、零警告           —— 只有 AddressSanitizer 抓得到
#   ub.c       1 << 31，退 0                      —— 只有 UndefinedBehaviorSanitizer 抓得到
#   slow.c     睡 3 秒                            —— 给 TIMEOUT 当靶子
#   env_demo.c 把 CH06_TAG 打出来                —— ENVIRONMENT 属性的显影剂
#
# 安全说明：这份工程**只编译不安装**，任何 -D 开关都不写全局配置；
# 真正的实验一律由 demo.sh 复制进 mktemp 沙箱后再做，仓库里不落构建产物。
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(Ch06Testing VERSION 0.1.0 LANGUAGES C)

# ---------- 本章要反复拨的四个开关 ----------
option(CH06_WITH_ASAN "挂 AddressSanitizer + LeakSanitizer（编译期和链接期一起加）" OFF)
option(CH06_ASAN_STATIC "再叠加 -static-libasan：不把 libasan.so 留在运行时依赖里" OFF)
option(CH06_UBSAN "挂 UndefinedBehaviorSanitizer" OFF)
# 关掉它 = UBSan 用默认的 recover 模式：报错但继续跑、退出码还是 0。
option(CH06_UBSAN_STOP "给 UBSan 加 -fno-sanitize-recover=all，让 UB 真的把退出码改掉" ON)
# WILL_FAIL 用错对象的开关（第 8 节）：把 WILL_FAIL 挂到本来该退 0 的 ok 上。
option(CH06_MISUSE_WILL_FAIL "故意把 WILL_FAIL 挂到 ok 上，演它只认退出码" OFF)

set(CH06_TIMEOUT 1 CACHE STRING "慢用例的超时秒数（第 9 节用它演 ***Timeout）")

# 一个字符串缓存变量，专门用来演示"命令行 -D 和 preset 的 cacheVariables 谁赢"。
set(CH06_GREETING "来自 CMakeLists 的默认" CACHE STRING "配置期打印，用来看覆盖顺序")
message(STATUS "CH06_GREETING = ${CH06_GREETING}")
message(STATUS "CH06_TIMEOUT  = ${CH06_TIMEOUT}")

# ---------- 七个探针程序：一次定义，逐个注册成用例 ----------
# ch06_probe(<可执行名> <源文件>)
#   编译警告集和 ASan 都在这里集中处理 —— 这就是第 25 节"接到真工程上"的那个套路：
#   把"每个 target 都该带的东西"包进一个函数，规则只写一遍，漏不掉。
function(ch06_probe _name _src)
    add_executable(${_name} "${CMAKE_CURRENT_SOURCE_DIR}/src/${_src}.c")
    target_compile_options(${_name} PRIVATE -Wall -Wextra -Wshadow)
    target_link_libraries(${_name} PRIVATE calc)

    if(CH06_WITH_ASAN)
        # 编译期：插检查代码 + -g（要行号）+ 保留栈帧（要看得懂回溯）
        target_compile_options(${_name} PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
        # 链接期：★ 少了这一条就是第 14 节那个 undefined reference to '__asan_init'
        if(CH06_ASAN_STATIC)
            target_link_options(${_name} PRIVATE -fsanitize=address -static-libasan)
        else()
            target_link_options(${_name} PRIVATE -fsanitize=address)
        endif()
    endif()

    if(CH06_UBSAN)
        target_compile_options(${_name} PRIVATE -fsanitize=undefined)
        target_link_options(${_name} PRIVATE -fsanitize=undefined)
        if(CH06_UBSAN_STOP)
            # UBSan 默认"报错但继续跑"，退出码不变 —— 测试会照样绿。
            # 这一条把它改成"报错就死"，ctest 才看得见。
            target_compile_options(${_name} PRIVATE -fno-sanitize-recover=all)
        endif()
    endif()
endfunction()

add_library(calc STATIC "${CMAKE_CURRENT_SOURCE_DIR}/src/calc.c")
target_include_directories(calc PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_options(calc PRIVATE -Wall -Wextra -Wshadow)

ch06_probe(ok       ok)
ch06_probe(wrong    wrong)
ch06_probe(nonzero  nonzero)
ch06_probe(leak     leak)
ch06_probe(oob      oob)
ch06_probe(ub       ub)
ch06_probe(slow     slow)
ch06_probe(env_demo env_demo)

# ---------- CTest ----------
# enable_testing() 是总闸：没有它，下面所有 add_test 静默失效（第 2 节演）。
enable_testing()

# 裁判一：只看退出码（默认）。ok 该绿，nonzero 单独看该红。
add_test(NAME smoke_ok COMMAND ok)

# 裸 target 名和 $<TARGET_FILE:..> 两种写法各来一条，
# 第 4 节去 CTestTestfile.cmake 里看它们最后都变成了什么。
add_test(NAME via_bare_target COMMAND ok)
add_test(NAME via_target_file COMMAND $<TARGET_FILE:ok>)

# 那三支 sanitizer 探针：默认裁判只看退出码，所以没挂 sanitizer 时它们全绿。
# ★ 顺序很重要：add_test 在前、set_tests_properties 在后。
#   反过来写，配置期直接报错（第 11 节演这个错）。
add_test(NAME leak_asan COMMAND leak)
add_test(NAME oob_asan  COMMAND oob)
add_test(NAME ub_asan   COMMAND ub)

# 裁判二：PASS_REGULAR_EXPRESSION —— 输出里必须匹配到，否则红。
# wrong 退 0（默认裁判会放它过关），但它打印的是 43 不是 42，这条尺子能抓住。
add_test(NAME regex_wrong COMMAND wrong)
set_tests_properties(regex_wrong PROPERTIES
    PASS_REGULAR_EXPRESSION "结果 = 42\n")

# 裁判三：WILL_FAIL TRUE —— 期望它退非 0。
add_test(NAME known_defect COMMAND nonzero)
set_tests_properties(known_defect PROPERTIES WILL_FAIL TRUE)

if(CH06_MISUSE_WILL_FAIL)
    # ok 退 0，可这里偏偏告诉 ctest"它应该失败"—— 第 8 节的反面教材。
    add_test(NAME ok_marked_willfail COMMAND ok)
    set_tests_properties(ok_marked_willfail PROPERTIES WILL_FAIL TRUE)
endif()

# 裁判四：TIMEOUT —— 秒数，到点直接掐。
add_test(NAME slow_times_out COMMAND slow)
set_tests_properties(slow_times_out PROPERTIES TIMEOUT ${CH06_TIMEOUT})

# 裁判五：ENVIRONMENT + LABELS —— 前者注入环境变量，后者给 -L 筛选用。
add_test(NAME env_plain COMMAND env_demo)
add_test(NAME env_tagged COMMAND env_demo)
set_tests_properties(env_tagged PROPERTIES
    ENVIRONMENT "CH06_TAG=来自测试属性"
    LABELS "env")
set_tests_properties(smoke_ok regex_wrong PROPERTIES LABELS "smoke")
set_tests_properties(leak_asan oob_asan ub_asan PROPERTIES LABELS "asan")

# 顺手留一条证据：${PWD} 在 add_test 里是**配置期**就展开掉的（它不是测试期 shell 的当前目录）。
# 展开成什么、空不空，取决于 CMake 版本有没有把 PWD 当老变量定义 —— 第 12 节看屏幕上的真值。
add_test(NAME configure_time_expand
         COMMAND ${CMAKE_COMMAND} -E echo "配置期展开的 PWD = [${PWD}]")

message(STATUS "第 06 章工程就绪：ASan=${CH06_WITH_ASAN} UBSan=${CH06_UBSAN} 静态asan=${CH06_ASAN_STATIC}")
