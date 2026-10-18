# ============================================================================
# -I 的「顺序」也是命令行的一部分：include_directories(BEFORE ...) 实测
#
# 磁盘上有两份**同名**头文件：
#   variants/inc-order/std/ch07/hdrwin.h   → CH07_WHO "std 目录赢了"
#   variants/inc-order/alt/ch07/hdrwin.h   → CH07_WHO "alt 目录赢了"
# 源码只有一份，两个目录每次都**同一批**给出去，只改一个词：BEFORE。
#
# 于是"顺序"这件事从玄学变成一行屏幕上的字：
#   WAY=plain   include_directories(ALT STD)         → -I alt 在前 → alt 赢
#   WAY=before  include_directories(BEFORE ALT STD)  → 本机实测：整批**反着**插到最前面
#                                                    → -I std 在前 → std 赢
# ★ 两次都配置成功、构建成功、测试没有任何意见 —— 只有程序输出换了。
#   这就是为什么迁移 -I 的时候不能只比"有没有"，必须把 -I 抽成一行一个再比顺序。
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(ch07incorder LANGUAGES C)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(STD "${ROOT}/variants/inc-order/std")
set(ALT "${ROOT}/variants/inc-order/alt")

set(WAY "plain" CACHE STRING "include_directories 的两种写法：plain 或 before")

if(WAY STREQUAL "before")
    include_directories(BEFORE "${ALT}" "${STD}")
else()
    include_directories("${ALT}" "${STD}")
endif()

add_executable(hdrwin "${ROOT}/src/hdrwin.c")

message(STATUS "inc-order：WAY=${WAY} 两个目录 = [${ALT}] [${STD}]")
