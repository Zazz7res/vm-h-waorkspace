# ============================================================================
# 和 variants/glob 唯一的区别：GLOB 后面多了 CONFIGURE_DEPENDS 这一个关键词。
#
# 这一份是拿来实测的：本机的生成器是 Unix Makefiles、CMake 是 4.2.3，
# CONFIGURE_DEPENDS 到底会不会让 cmake --build 自己回头重新 configure，
# 只有跑一次才知道（本机实测写在章 README 第 20、21 节，跑成什么样就写成什么样）。
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(ch07globdep LANGUAGES C)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

file(GLOB LEAF CONFIGURE_DEPENDS "${ROOT}/src/leaf/*.c")
message(STATUS "CONFIGURE_DEPENDS 版：扫到 ${LEAF}")

enable_testing()
foreach(_f IN LISTS LEAF)
    get_filename_component(_n "${_f}" NAME_WE)
    add_executable(${_n} "${_f}")
    add_test(NAME smoke_${_n} COMMAND ${_n})
endforeach()
