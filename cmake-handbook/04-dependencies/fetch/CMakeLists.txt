# ============================================================================
# fetch —— 第三种到货方式：FetchContent，"configure 的时候现把源码拉下来"
#
# 为了在这台机器上**完全不联网**也能演示，所有地址都是 file://（本地路径）。
# 真实项目里换成 https://github.com/... 即可，机制一模一样。
#
# 拨档：
#   ROUTE   GIT | URL          —— 两种到货协议
#   MX_URI  file:// 开头的地址（GIT 模式=仓库地址，URL 模式=tar.gz 地址）
#   MX_TAG  GIT_TAG（分支名 / 标签 / commit 全写）
#   MX_HASH SHA256；留空 = 不校验
#   MX_ETS  写 TRUE 才加 DOWNLOAD_EXTRACT_TIMESTAMP（留空就会吃 CMP0135 警告）
#   MX_EXCLUDE  ON = FetchContent 时用 EXCLUDE_FROM_ALL
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(fetch_demo LANGUAGES C)

include(FetchContent)      # 这个模块本身在 /usr/share/cmake-4.2/Modules/ 里

set(ROUTE "GIT" CACHE STRING "GIT 或 URL")
set(MX_URI "" CACHE STRING "file:// 地址")
set(MX_TAG "v1.3.0" CACHE STRING "GIT_TAG")
set(MX_HASH "" CACHE STRING "SHA256，留空不校验")
set(MX_ETS "" CACHE STRING "DOWNLOAD_EXTRACT_TIMESTAMP")
option(MX_EXCLUDE "用 EXCLUDE_FROM_ALL" OFF)
option(MX_SYSTEM  "给第三方加 SYSTEM（头目录从 -I 变 -isystem，CMake 3.25+）" OFF)

if(NOT MX_URI)
    message(FATAL_ERROR "必须用 -DMX_URI=file:///... 告诉 CMake 去哪拿源码（本章全程不联网）")
endif()

# 把可选参数拼成列表再展开，避免写两份几乎一样的 FetchContent_Declare
set(_ets "")
if(NOT MX_ETS STREQUAL "")
    set(_ets DOWNLOAD_EXTRACT_TIMESTAMP ${MX_ETS})
endif()
set(_sys "")
if(MX_SYSTEM)
    set(_sys SYSTEM)
endif()
set(_extra "")
if(MX_EXCLUDE)
    set(_extra EXCLUDE_FROM_ALL)
endif()

if(ROUTE STREQUAL GIT)
    message(STATUS "FetchContent 走 GIT：${MX_URI} tag=${MX_TAG}")
    FetchContent_Declare(mathx
        GIT_REPOSITORY "${MX_URI}"
        GIT_TAG        "${MX_TAG}"
        GIT_SHALLOW    TRUE        # 只要那一个 commit，别把历史全拖进来
        GIT_SUBMODULES ""          # 空串 = 不去初始化子模块（否则可能顺着子模块联网）
        ${_ets} ${_extra} ${_sys})
elseif(ROUTE STREQUAL URL)
    if(MX_HASH STREQUAL "")
        message(STATUS "FetchContent 走 URL（不校验）：${MX_URI}")
        FetchContent_Declare(mathx URL "${MX_URI}" ${_ets} ${_extra} ${_sys})
    else()
        message(STATUS "FetchContent 走 URL + SHA256=${MX_HASH}")
        FetchContent_Declare(mathx URL "${MX_URI}" URL_HASH SHA256=${MX_HASH} ${_ets} ${_extra} ${_sys})
    endif()
else()
    message(FATAL_ERROR "ROUTE='${ROUTE}' 非法，只接受 GIT 或 URL")
endif()

message(STATUS "FETCHCONTENT_BASE_DIR = ${FETCHCONTENT_BASE_DIR}")

# MakeAvailable = "拉下来 + add_subdirectory 进来"，一条命令两步。
# 老写法 FetchContent_Populate(mathx) + 手写 add_subdirectory 已经废弃（3.30 起警告）。
FetchContent_MakeAvailable(mathx)

# MakeAvailable 之后才有这两个变量；它们是"源码落到哪了"的唯一权威答案
message(STATUS "mathx 源码目录   = ${mathx_SOURCE_DIR}")
message(STATUS "mathx 构建目录   = ${mathx_BINARY_DIR}")

add_executable(fetch_app main.c)
# 注意这里用的是 mathx::mathx：即使是现拉的源码，也照用上游 ALIAS 的名字空间写法，
# 这样将来从 FetchContent 换成 find_package，这一行一个字都不用改。
target_link_libraries(fetch_app PRIVATE mathx::mathx)
