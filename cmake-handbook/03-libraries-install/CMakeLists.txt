# =====================================================================
# 第 03 章顶层：一个「库 + 内部依赖 + 两个消费者」的可交付骨架
# =====================================================================
# 第 02 章回答的是「同一条构建树里，target 之间怎么互相传话」。
# 本章回答的是：**这个库交付出去之后，别人还用什么找到它、用它的头、链接它的代码？**
#
# 目录布局（业界最常见的那一种）：
#   lib/include/vector.h         对外契约 → 会装到 <前缀>/include/vector.h
#   lib/src/vector.c             实现
#   lib/src/vector_internal.h    内部头 → **不装**，消费者永远看不见
#   util/                        vector 的内部依赖（本章的实验对象）
#   app/                         同树消费者：靠 target 名字直接链接
#   consumer/                    独立工程：靠 find_package + Vector::vector
#   cmake/VectorConfig.cmake.in  find_package 真正加载的那份模板
#
# ⚠ 下面 5 个 CACHE 开关是**教学装置**：真实项目里一个都不要写。
#   它们的唯一用途，是让 demo.sh 能把同一份工程拨成 5 种状态，
#   把「装完之后才发现坏了」的那几种坏法，提前到配置期当场暴露给你看。
# =====================================================================

cmake_minimum_required(VERSION 3.16)

# 工程版本会被 write_basic_package_version_file 写进 VectorConfigVersion.cmake，
# 于是 consumer 那句 find_package(Vector 1.0 REQUIRED) 才有东西可比。
project(Vector VERSION 1.0.2 LANGUAGES C)

# ── 1. BUILD_SHARED_LIBS：CMake 自己的**标准**开关（不是我们发明的）──────
# 它不为真时 add_library(vector ...) 默认编成静态库 .a，为真时默认编成 .so。
# 好处是：库作者一行代码都不用写，用户想静态就 -DBUILD_SHARED_LIBS=OFF，想动态就 ON。
option(BUILD_SHARED_LIBS "add_library 默认编成共享库（.so）而不是静态库（.a）" OFF)

# ── 2. VECTOR_DEP_SCOPE：vector 链接 util 用的作用域 ─────────────────────
# 第 02 章的老朋友。本章它不再只影响 -I，而是决定**导出的文件里长什么样**：
#   PRIVATE → 导出文件里写成 $<LINK_ONLY:Vector::util>（要链接，不要传染头）
#   PUBLIC  → 导出文件里写成裸的 Vector::util（链接 + 头目录都传染）
set(VECTOR_DEP_SCOPE "PRIVATE" CACHE STRING "vector 链接 util 的作用域（PRIVATE 或 PUBLIC）")
set_property(CACHE VECTOR_DEP_SCOPE PROPERTY STRINGS PRIVATE PUBLIC)

# ── 3. VECTOR_EXPORT_UTIL：把内部依赖 util 也放进导出集合 ─────────────────
# ON（默认）才是能跑通的写法。OFF 专门用来复现那条最著名的导出报错：
#   install(EXPORT "VectorTargets" ...) includes target "vector" which requires
#   target "util" that is not in any export set.
# 记住本章最重要的一句话：**PRIVATE 不等于「不用导出」**。
option(VECTOR_EXPORT_UTIL "把 util 一起放进导出集合（OFF 会当场报错，故意留给你看）" ON)

# ── 4. VECTOR_UTIL_AS_OBJECT：util 到底生成 .a 还是只生成一堆 .o ───────────
# ON 且 VECTOR_EXPORT_UTIL=OFF 时走「彻底隐藏内部依赖」那条路：
# 不 link，只把 util 的 .o 直接吸进 libvector.a —— 于是导出文件里连 util 都不出现。
option(VECTOR_UTIL_AS_OBJECT "util 编成 OBJECT 库（对象文件而不是 .a）" OFF)

# ── 5. VECTOR_PLAIN_INCLUDE：公开头目录用裸的绝对路径（新手写法）──────────
# 同树编译一个字都不报错，install(EXPORT) 那一刻才炸。第 02 章结尾埋的那颗雷就在这儿拆。
option(VECTOR_PLAIN_INCLUDE "对外头目录不加 BUILD/INSTALL_INTERFACE（专门复现导出报错）" OFF)

# CACHE STRING 的取值 CMake **一律不校验**（第 02 章踩过的坑），自己检查。
if(NOT VECTOR_DEP_SCOPE MATCHES "^(PRIVATE|PUBLIC)$")
    message(FATAL_ERROR "VECTOR_DEP_SCOPE='${VECTOR_DEP_SCOPE}' 非法，只接受 PRIVATE 或 PUBLIC")
endif()

# 静态库的代码将来可能被吸进共享库 .so 里，而 .so 要求每一段参与链接的机器码都是
# 位置无关的 → 位置无关代码必须一路开着，否则 BUILD_SHARED_LIBS=ON 那一节会链接报错。
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# GNUInstallDirs：给你 lib/、include/、bin/ 这些「该装哪儿」的标准变量
# （在 64 位发行版上 lib 可能是 lib64，写死 "lib" 是个跨机器隐患）。
# CMakePackageConfigHelpers：给你 configure_package_config_file() 和
# write_basic_package_version_file() 两个函数。
# 放在**顶层**，三个子目录自动继承 —— 子目录里不用再 include 一次。
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

add_subdirectory(util)   # 内部依赖，先定义才能被后面链接
add_subdirectory(lib)    # 本章主角：真正要交付出去的库
add_subdirectory(app)    # 同树消费者

# 注意 consumer/ **没有** add_subdirectory：它属于「装完之后另开的工程」，
# demo.sh 里会单独 cmake -S consumer。把它加进树里就等于作弊（能找到源码路径）。

# 这四行是 demo.sh 第 2 节要 grep 的「当前拨到了哪一档」回执。
message(STATUS "工程 Vector ${PROJECT_VERSION}，库类型 = ${BUILD_SHARED_LIBS}（OFF=静态 .a，ON=共享 .so）")
message(STATUS "  VECTOR_DEP_SCOPE      = ${VECTOR_DEP_SCOPE}")
message(STATUS "  VECTOR_EXPORT_UTIL    = ${VECTOR_EXPORT_UTIL}")
message(STATUS "  VECTOR_UTIL_AS_OBJECT = ${VECTOR_UTIL_AS_OBJECT}")
message(STATUS "  VECTOR_PLAIN_INCLUDE  = ${VECTOR_PLAIN_INCLUDE}")
message(STATUS "  CMAKE_INSTALL_PREFIX  = ${CMAKE_INSTALL_PREFIX}")
