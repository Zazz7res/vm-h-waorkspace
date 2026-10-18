# ============================================================================
# file(GLOB) 单独成节：一个 .c = 一个可执行 + 一条用例
#
# GLOB 只在**配置期**扫一次目录。这份工程配合 demo.sh 里那句
#   cat > src/leaf/leaf_c.c
# 就能演出最难受的一种病：文件在磁盘上、target 不存在、ctest 名单里也没有它，
# 而屏幕上一个字都不提示。
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(ch07glob LANGUAGES C)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

file(GLOB LEAF "${ROOT}/src/leaf/*.c")
message(STATUS "GLOB 在配置期扫到 ${CMAKE_SOURCE_DIR} 下这几个 leaf：${LEAF}")

enable_testing()
foreach(_f IN LISTS LEAF)
    get_filename_component(_n "${_f}" NAME_WE)
    add_executable(${_n} "${_f}")
    add_test(NAME smoke_${_n} COMMAND ${_n})
endforeach()
