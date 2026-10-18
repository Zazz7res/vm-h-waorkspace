# =====================================================================
# consumer —— 一个**完全独立**的工程：它不知道 lib/ 在哪，只认识 Vector 这个名字
# =====================================================================
# 这个目录**没有**被顶层 add_subdirectory 加进来（加了就等于作弊：那样它能直接看见源码路径）。
# 它模拟的是：库已经 cmake --install 到某个前缀下了，
# 现在换个目录、换个工程，用 find_package 把它找回来 ——
# 「三个月后别人怎么用你的库」就是这个场景。
# =====================================================================

cmake_minimum_required(VERSION 3.16)
project(vector_consumer VERSION 0.1 LANGUAGES C)

# 向 Vector 要求的最低版本。demo.sh 会把它拨到 2.0 去现场撞一次「版本不兼容」。
set(CONSUMER_FIND_VERSION "1.0" CACHE STRING "find_package 时要求的 Vector 最低版本")

# 越界实验一：去 include 库的**内部头**（vector_internal.h）。
# 它从来没被 install 出去，所以无论哪种作用域都必然编译失败 —— 这是「契约边界」的证据。
option(CONSUMER_PROBES_INTERNAL "越界去 include 库自己的内部头" OFF)

# 越界实验二：去 include **内部依赖 util 的头**。
# 这一条才是 VECTOR_DEP_SCOPE 的照妖镜：
#   PUBLIC  → 头目录传染过来了，编得过（代价：util.h 必须一起交付）
#   PRIVATE → 只传染链接、不传染头，#include "util.h" 当场失败（但链接照样成功）
option(CONSUMER_PROBES_UTIL_H "越界去 include vector 的内部依赖头 util.h" OFF)

# 这一句就是全部「连接方式」：找到一个装好的 Vector，并拿到 Vector::vector 这个导入目标。
# 注意 REQUIRED：找不到就直接配置失败，而不是留一个 Vector_FOUND=FALSE 让你自己查。
find_package(Vector ${CONSUMER_FIND_VERSION} REQUIRED)

# 直接把导入目标身上的属性打出来：这就是 find_package 帮你「复活」出来的东西。
get_target_property(_vec_inc Vector::vector INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(_vec_lnk Vector::vector INTERFACE_LINK_LIBRARIES)
message(STATUS "consumer 找到 Vector 的配置文件目录：${Vector_DIR}")
message(STATUS "consumer 眼里 Vector::vector 的头目录  = ${_vec_inc}")
message(STATUS "consumer 眼里 Vector::vector 的链接要求 = ${_vec_lnk}")

add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE Vector::vector)

if(CONSUMER_PROBES_INTERNAL)
    target_compile_definitions(consumer PRIVATE CONSUMER_PROBES_INTERNAL=1)
endif()
if(CONSUMER_PROBES_UTIL_H)
    target_compile_definitions(consumer PRIVATE CONSUMER_PROBES_UTIL_H=1)
endif()
