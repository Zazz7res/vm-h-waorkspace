# ============================================================================
# 第 05 章《构建的四个旋钮》的实验工程
#
# 这一章不讲"什么是 configure/build"（第 01 章讲过了），讲的是**命令行级取证**：
#   旋钮一 生成器（-G）                —— 到底谁在执行编译
#   旋钮二 构建类型（CMAKE_BUILD_TYPE）—— 优化/调试标志变成了哪几个字符
#   旋钮三 缓存（CMakeCache.txt）      —— 为什么改了不生效、怎么改才生效
#   旋钮四 compile_commands.json       —— 编译器眼里的世界是什么样
#
# 工程本身很小：一个可执行 + 两份**同名**的头。
# include/calc.h 和 alt/include/calc.h 的 CALC_FLAVOR 值不一样，
# 而本文件把两个目录**都**挂到 -I 上，只是顺序随 ALT_FIRST 翻转
# —— 程序一跑就知道 -I 的顺序让谁赢了。
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(BuildInternals VERSION 0.1.0 LANGUAGES C)

# 旋钮 A：把山寨头目录排到前面（ALT_FIRST=ON），用来演示 #include 的顺序竞争。
option(ALT_FIRST "把 alt/include 排在 include 之前（演 -I 顺序竞争）" OFF)

# 旋钮 B：头目录用 target 属性给（现代写法），还是用全局命令给（老写法）。
#   第 03/07 章会回到这个话题；这里只关心它写进 flags.make 的样子。
option(USE_GLOBAL_INCLUDE_DIR "用 include_directories() 全局给头目录（老写法）" OFF)

if(ALT_FIRST)
    set(_order "${CMAKE_CURRENT_SOURCE_DIR}/alt/include" "${CMAKE_CURRENT_SOURCE_DIR}/include")
else()
    set(_order "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/alt/include")
endif()

add_executable(app src/main.c src/calc.c)

if(USE_GLOBAL_INCLUDE_DIR)
    # 老写法：全局命令，顺序照抄，作用范围却是整个目录里所有 target
    include_directories(BEFORE ${_order})
else()
    # 新写法：逐目录顺序展开成多条 target_include_directories
    foreach(_d IN LISTS _order)
        target_include_directories(app PRIVATE "$<BUILD_INTERFACE:${_d}>")
    endforeach()
endif()

# 顺手留一个"给某个源文件单独加标志"的样本：第 12 节解剖构建目录时用到
set_source_files_properties(src/calc.c PROPERTIES COMPILE_FLAGS "-DFROM_SOURCE_PROPERTY=1")

message(STATUS "拨档回执：ALT_FIRST=${ALT_FIRST} USE_GLOBAL_INCLUDE_DIR=${USE_GLOBAL_INCLUDE_DIR}")
message(STATUS "当前生成器 = ${CMAKE_GENERATOR}   构建类型 = [${CMAKE_BUILD_TYPE}]")
