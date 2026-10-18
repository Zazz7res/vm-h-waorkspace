# ============================================================================
# Findmathx.cmake —— Module 模式：站在"作者"这一侧写一次
#
# find_package(mathx) 有两条完全不同的实现路线：
#   Config 模式：包自己带 mathxConfig.cmake（第 03 章我们用 install(EXPORT) 生成过）
#   Module 模式：包**什么都没带**，由调用方（或 CMake 官方）写一个 Find<pkg>.cmake
#
# CMake 装了一堆现成模块，去这里看：/usr/share/cmake-4.2/Modules/Find*.cmake
# 本章要拿它们当证据，同时也要我们亲手写一个，才知道里面的责任怎么分。
#
# 本模块有三个可拨的档（都由调用方在 find_package 之前设好）：
#   MXM_STRICT_VERSION   ON=老老实实比较版本；OFF=装没听见（本章第 8 节用它做 A/B）
#   MXM_REDIRECT_TO_CONFIG ON=我找到配置文件后转交给 Config 模式
#   mathx_ROOT           指路的根目录（HINTS 用）
# ============================================================================

message(STATUS "[Findmathx.cmake] Module 模式应答：这个文件就是我，正在被 include 执行")

# ---- 0) 一条实测出来的边界 ----
# 有人以为模块作者可以「金蝉脱壳」：在自己模块里 set(mathx_CONFIG <路径>) 就把活儿
# 转交给上游的 Config 文件。实测（CMake 4.2.3）不生效 —— 普通变量、CACHE、FORCE
# 三种写法都不生效，模块设完 return() 之后 CMake 并不会去加载那份 Config。
# 真要绕，只有调用方那一侧的 <包名>_DIR 好使（第 15 节实测）。

# ---- 1) 找头：find_path 会在每个"前缀"下面试 <前缀>/include ----
find_path(MATHX_INCLUDE_DIR NAMES mathx.h
          HINTS ${mathx_ROOT} ${MATHX_ROOT} $ENV{MATHX_ROOT}
                "${CMAKE_CURRENT_LIST_DIR}/../../.."   # 允许把模块放在某个前缀里
          PATH_SUFFIXES include)

# ---- 2) 找库：find_library 会试 <前缀>/lib、<前缀>/lib64，并自动加 lib 前缀和 .a/.so 后缀 ----
find_library(MATHX_LIBRARY NAMES mathx
             HINTS ${mathx_ROOT} ${MATHX_ROOT} $ENV{MATHX_ROOT}
             PATH_SUFFIXES lib lib64)

# ---- 3) 版本号：CMake **不会**替你去比较，得模块作者自己抠 ----
# CMake 只是把你写的 "find_package(mathx 2.0)" 里的 2.0 塞进 mathx_FIND_VERSION，
# 用不用、怎么比，全是这个文件的自由。下面的 A/B 就是拿这一行做文章。
message(STATUS "[Findmathx.cmake] 调用方要求的最低版本 = ${mathx_FIND_VERSION}（CMake 自己不比较）")

set(MATHX_VERSION "")
if(MATHX_INCLUDE_DIR AND EXISTS "${MATHX_INCLUDE_DIR}/mathx.h")
    file(STRINGS "${MATHX_INCLUDE_DIR}/mathx.h" _mx_ver_line
         REGEX "^#[ \t]*define[ \t]+MATHX_VERSION[ \t]+\"[^\"]+\"")
    string(REGEX REPLACE "^#[ \t]*define[ \t]+MATHX_VERSION[ \t]+\"([^\"]+)\".*" "\\1"
           MATHX_VERSION "${_mx_ver_line}")
    unset(_mx_ver_line)
endif()

# ---- 4) 判定成败：把责任交给标准工具 ----
include(FindPackageHandleStandardArgs)
if(MXM_STRICT_VERSION)
    # VERSION_VAR 一写，"要求 2.0 而实际 1.3.0" 就会在这里被拒；
    # 报错文案和 Config 模式很像，但裁判完全不同（这里是我们这几行字）。
    find_package_handle_standard_args(mathx
        REQUIRED_VARS MATHX_LIBRARY MATHX_INCLUDE_DIR
        VERSION_VAR   MATHX_VERSION)
else()
    # 故意不写 VERSION_VAR：无论你要几版本，只要文件找到了就叫成功。
    find_package_handle_standard_args(mathx
        REQUIRED_VARS MATHX_LIBRARY MATHX_INCLUDE_DIR)
endif()

mark_as_advanced(MATHX_INCLUDE_DIR MATHX_LIBRARY)

# ---- 5) 成功的话，把结果包成 IMPORTED 目标 ----
# 这一步是"现代写法"的分水岭：只设 MATHX_LIBRARIES/MATHX_INCLUDE_DIRS 三个变量的
# 老式模块，作用域会像第 02 章那样到处漏；给一个带名字空间的 target 才干净。
if(mathx_FOUND AND NOT TARGET mathx::mathx)
    add_library(mathx::mathx UNKNOWN IMPORTED)
    set_target_properties(mathx::mathx PROPERTIES
        IMPORTED_LOCATION             "${MATHX_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MATHX_INCLUDE_DIR}")
    message(STATUS "[Findmathx.cmake] 交付：mathx::mathx = ${MATHX_LIBRARY}（版本 ${MATHX_VERSION}）")
endif()
