# ============================================================================
# 一份"典型的 2013 年 CMakeLists"：六个老病灶一次全上，而且**它能跑**。
#
# 这一份不是稻草人 —— 网上大量教程、公司内部老仓库、README 里抄来的片段，
# 就是这个形状。它能配置成功、能构建成功、程序能跑出正确结果，
# 于是没有任何一条路逼你改。这正是本章存在的原因。
#
# 病灶编号（对应章 README 的小节）：
#   ① include_directories      —— 目录级，子目录里所有 target 一起中招
#   ② add_definitions          —— 同上，连第三方的 trace 分支都被点亮
#   ③ link_directories + -lfoo —— 谁赢由 -L 顺序决定，且看不见
#   ④ file(GLOB)               —— 清单是配置期拍的快照
#   ⑤ 全局变量传依赖清单        —— 子目录看不见、谁也改得动
#   ⑥ COMPILE_FLAGS + 无关键字 target_link_libraries —— 不追加、不校验
# ============================================================================

cmake_minimum_required(VERSION 3.10)
project(ch07legacy VERSION 0.1.0 LANGUAGES C)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)   # 和 modern/ 共用同一份源码

set(MODE "PLUS" CACHE STRING "calc 的行为开关")
set(STAMP "0" CACHE STRING "打进二进制里的构建号")
set(FOO_DIR1 "" CACHE PATH "第一份 libfoo.a 在哪")
set(FOO_DIR2 "" CACHE PATH "第二份 libfoo.a 在哪")

# ①②：两条全局指令，写在最前面（老教程都强调"要写在前面"）
include_directories("${ROOT}/include")
add_definitions(-DCH07_MODE_${MODE}=1 -DCH07_STAMP=${STAMP} -DCH07_DEBUG_TRACE=1)

# ③：全局 -L，顺序 = 优先级
link_directories("${FOO_DIR1}" "${FOO_DIR2}")

# ⑥：单文件/单目标的标志，用字符串属性
add_library(calc STATIC "${ROOT}/src/calc/calc.c")
add_library(util STATIC "${ROOT}/src/util/util.c")
set_target_properties(calc PROPERTIES COMPILE_FLAGS "-Wall -Wextra")

# ⑤：一个全局变量当"依赖清单"传给后面所有目录用
set(ALL_DEPS calc util)

# ④：源文件用 GLOB 收，省得一个个写
file(GLOB LEAF_SRC "${ROOT}/src/leaf/*.c")

add_executable(app     "${ROOT}/src/app.c")
add_executable(greet   "${ROOT}/src/greet.c")
add_executable(fooprog "${ROOT}/src/fooprog.c")

# ⑥：无关键字签名 —— 库名一口气列完，谁也不带 PUBLIC/PRIVATE 语义
target_link_libraries(app ${ALL_DEPS})
target_link_libraries(greet util)
target_link_libraries(fooprog foo)          # ← 只给名字：链到哪一份由上面的 -L 顺序决定

add_subdirectory("${ROOT}/tp" tp_build)     # 第三方目录：它没要过任何东西，但全吃

foreach(f ${LEAF_SRC})
    get_filename_component(n "${f}" NAME_WE)
    add_executable(${n} "${f}")
endforeach()

enable_testing()
foreach(f ${LEAF_SRC})
    get_filename_component(n "${f}" NAME_WE)
    add_test(NAME smoke_${n} COMMAND ${n})
endforeach()

message(STATUS "legacy：能配置、能构建、能跑 —— 所以没人改它")
