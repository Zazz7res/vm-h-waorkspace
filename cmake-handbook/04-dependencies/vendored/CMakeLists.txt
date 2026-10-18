# ============================================================================
# vendored —— 第四种到货方式：源码直接躺在仓库里（vendoring）
#
# 和 FetchContent 的区别只有一个动作：源码不用"拉"，它本来就在那儿。
# 于是省掉了网络、哈希、_deps 目录，代价是你要自己负责跟上游升级。
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(vendored_demo LANGUAGES C)

option(VENDOR_EXCLUDE "add_subdirectory 时用 EXCLUDE_FROM_ALL" OFF)

set(_src "${CMAKE_CURRENT_SOURCE_DIR}/../mathx")
set(_bin "${CMAKE_CURRENT_BINARY_DIR}/_mathx")

message(STATUS "把树外源码 ${_src} 用 add_subdirectory 拉进本树")

# 关键坑：add_subdirectory 的第二个参数。
#   源码目录在当前目录之下 → 第二个参数可以省（默认同名子目录）
#   源码目录在**树外**（这里是 ../mathx）→ 必须显式给二进制目录，否则报错。
# 另外它带的 install() 规则也会被继承：主工程一 install，mathx 跟着被装走。
# 不想这样就用 EXCLUDE_FROM_ALL（只管"all 不构建它"），或者把它做成 OBJECT 库。
if(VENDOR_EXCLUDE)
    add_subdirectory("${_src}" "${_bin}" EXCLUDE_FROM_ALL)
else()
    add_subdirectory("${_src}" "${_bin}")
endif()

add_executable(vendor_app main.c)
target_link_libraries(vendor_app PRIVATE mathx::mathx)
