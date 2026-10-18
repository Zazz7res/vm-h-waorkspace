# ============================================================================
# consumer —— 本章的舞台：一份 find_package，四种命运
#
# 单独 configure（它不在顶层树里）：
#   cmake -S consumer -B build -DCMAKE_PREFIX_PATH=<mathx 装到哪>
#
# 拨档：
#   MODE                 AUTO | MODULE | CONFIG   —— find_package 的关键字
#   WANT_VERSION         传给 find_package 的版本号
#   USE_OUR_MODULE       OFF = 不把 cmake/modules 加进 CMAKE_MODULE_PATH
#   MXM_STRICT_VERSION   见 Findmathx.cmake
#   MXM_REDIRECT_TO_CONFIG 见 Findmathx.cmake
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(consumer LANGUAGES C)

set(MODE "AUTO" CACHE STRING "find_package 模式：AUTO / MODULE / CONFIG")
set(WANT_VERSION "1.0" CACHE STRING "向 mathx 要求的最低版本")
option(USE_OUR_MODULE "把 ../cmake/modules 塞进 CMAKE_MODULE_PATH（我们的 Find 模块）" ON)
option(MXM_STRICT_VERSION "Find 模块是否检查版本" ON)
option(MXM_REDIRECT_TO_CONFIG "Find 模块是否转交给 Config" OFF)
set(mathx_ROOT "" CACHE PATH "直接给 mathx 指路的前缀（HINTS）")

if(USE_OUR_MODULE)
    # list(PREPEND) = 插到最前面。CMAKE_MODULE_PATH 是按顺序试的，谁在前面谁赢。
    list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules")
endif()

message(STATUS "拨档回执：MODE=${MODE} WANT_VERSION=${WANT_VERSION} USE_OUR_MODULE=${USE_OUR_MODULE}")
message(STATUS "CMAKE_MODULE_PATH[0] = ${CMAKE_MODULE_PATH}")
message(STATUS "CMAKE_PREFIX_PATH    = ${CMAKE_PREFIX_PATH}")

if(WANT_VERSION STREQUAL "")
    set(_req "")
else()
    set(_req "${WANT_VERSION}")
endif()

if(MODE STREQUAL AUTO)
    find_package(mathx ${_req} REQUIRED)
elseif(MODE STREQUAL MODULE)
    find_package(mathx ${_req} MODULE REQUIRED)     # 只准走模块，没有就报错
elseif(MODE STREQUAL CONFIG)
    find_package(mathx ${_req} CONFIG REQUIRED)     # 只准走 Config，模块存在也当没有
else()
    message(FATAL_ERROR "MODE='${MODE}' 非法，只接受 AUTO / MODULE / CONFIG")
endif()

# ---- 找完之后，"谁应答了"全部可以从这几个变量里查出来 ----
message(STATUS "mathx_FOUND      = ${mathx_FOUND}")
message(STATUS "mathx_VERSION    = ${MATHX_VERSION}   (Module 模式由 Find 脚本填)")
message(STATUS "mathx_DIR        = ${mathx_DIR}   (Config 模式由它指到配置文件目录)")
message(STATUS "头 = ${MATHX_INCLUDE_DIR}")
message(STATUS "库 = ${MATHX_LIBRARY}")

add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE mathx::mathx)
