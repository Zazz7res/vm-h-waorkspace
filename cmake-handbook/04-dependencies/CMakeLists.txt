# ============================================================================
# 第 04 章《依赖：同一份库，四条到货路线》顶层导航
#
# 这一章的三个子工程**故意不**由顶层 add_subdirectory：
# 它们各自代表一种"到货方式"，必须各配各的构建目录才看得出差别。
#   consumer/  find_package(mathx) —— 库已经装在磁盘上（Module / Config 两种模式）
#   fetch/     FetchContent        —— configure 时现拉源码（本章用 file:// 不联网）
#   vendored/  add_subdirectory   —— 源码就在仓库里
#
# 所以请直接用 demo.sh，或者单独：
#   cmake -S vendored -B bv && cmake --build bv && ./bv/vendor_app
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(cmake_ch04 LANGUAGES C)

message(STATUS "")
message(STATUS "第 04 章：依赖。同一份 mathx，四条到货路线：")
message(STATUS "  ① find_package + CMake 自带 Find 模块   （Module 模式，作者是我们）")
message(STATUS "  ② find_package + 上游自带 mathxConfig   （Config 模式，第 03 章生成过）")
message(STATUS "  ③ FetchContent（GIT / URL）             （源码现拉，进 _deps/）")
message(STATUS "  ④ add_subdirectory(../mathx)            （vendoring，源码进仓库）")
message(STATUS "")
message(STATUS "  本目录不构建任何东西。跑： bash demo.sh")
