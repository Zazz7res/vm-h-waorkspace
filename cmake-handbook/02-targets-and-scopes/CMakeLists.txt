# =====================================================================
# 第 02 章示例：三个 target，两个可拨的开关
# =====================================================================
# 这一章的唯一主题：**PUBLIC / PRIVATE / INTERFACE 到底改变了谁的编译命令行**。
# 光看文字讲不清，所以这个工程把两个作用域做成了缓存开关，
# demo.sh 会当场把它们拨到另一个值，让你看到**真实的失败输出**。
#
# 注意：`target_include_directories(vector ${VEC_INCLUDE_SCOPE} ...)`
# 把关键字做成变量，是**本章为了做实验**的特殊写法，真实项目里不要这么写。
# =====================================================================

cmake_minimum_required(VERSION 3.16)
project(scopes_demo VERSION 0.1 LANGUAGES C)   # 本机没有 g++，只声明 C

# ── 两个实验开关（都是 CACHE 变量，所以能被 -D 覆盖）────────────────
set(VEC_INCLUDE_SCOPE "PUBLIC" CACHE STRING "vector 自己的头目录用什么作用域（PUBLIC/PRIVATE）")
set(VEC_DEP_SCOPE     "PUBLIC" CACHE STRING "vector 链接 util 时用什么作用域（PUBLIC/PRIVATE/INTERFACE）")

# 把可选值写进缓存属性：cmake-gui / ccmake 里会显示成下拉框，少打错一次
set_property(CACHE VEC_INCLUDE_SCOPE PROPERTY STRINGS PUBLIC PRIVATE)
set_property(CACHE VEC_DEP_SCOPE     PROPERTY STRINGS PUBLIC PRIVATE INTERFACE)

# ⚠ 校验必须自己写：CMake 对 CACHE STRING 的取值**不做任何检查**。
# 不校验的后果就是第 01 章那个场景 —— 你写了 -DVEC_DEP_SCOPE=PUBIC，它照着用，编出个莫名其妙的东西。
if(NOT VEC_INCLUDE_SCOPE MATCHES "^(PUBLIC|PRIVATE)$")
    message(FATAL_ERROR "VEC_INCLUDE_SCOPE='${VEC_INCLUDE_SCOPE}' 非法，只接受 PUBLIC 或 PRIVATE")
endif()
if(NOT VEC_DEP_SCOPE MATCHES "^(PUBLIC|PRIVATE|INTERFACE)$")
    message(FATAL_ERROR "VEC_DEP_SCOPE='${VEC_DEP_SCOPE}' 非法，只接受 PUBLIC / PRIVATE / INTERFACE")
endif()

message(STATUS "实验开关：VEC_INCLUDE_SCOPE=${VEC_INCLUDE_SCOPE}  VEC_DEP_SCOPE=${VEC_DEP_SCOPE}")

# =====================================================================
# target 1：util —— 最底层的静态库
# =====================================================================
add_library(util STATIC util/util.c)

# PUBLIC：这个目录既用于编 util 自己，也**传染给所有链接 util 的人**。
# 这就是“消费者只要 -I 到 public 头目录”的标准写法。
target_include_directories(util PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/util)

# 同样是 PUBLIC：-DUTIL_TAG=... 会传染给消费者。
# vstats.c 里用 #ifdef 探测它，所以它消失时**不会报错，只会静默改变程序行为**（本章的软失败演示）。
target_compile_definitions(util PUBLIC UTIL_TAG="util-v1")

# =====================================================================
# target 2：strict_flags —— 一个纯头/纯设置的 INTERFACE 库
# =====================================================================
# INTERFACE 库没有源码，它只是“一组设置的打包名字”，专门用来给多个 target 复用编译选项。
add_library(strict_flags INTERFACE)
target_compile_options(strict_flags INTERFACE -Wall -Wextra -Wshadow)

# =====================================================================
# target 3：vector —— 中间的静态库，本章的两个开关都长在它身上
# =====================================================================
add_library(vector STATIC lib/vector.c)

# 开关①：vector 的头目录作用域
target_include_directories(vector ${VEC_INCLUDE_SCOPE} ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# PRIVATE：只用于编 vector 自己，绝不传染给 app / vstats。
# 它会在 build/CMakeFiles/vector.dir/flags.make 里出现，而在 app.dir 里**搜不到** —— 这是本章要核对的证据之一。
target_compile_definitions(vector PRIVATE BUILDING_VECTOR=1)

# 语言标准用现代写法：PUBLIC 表示“我用 C11，链接我的人也得按 C11 编我这两个头”。
target_compile_features(vector PUBLIC c_std_11)

# 开关②：vector 依赖 util 的作用域 —— 整章最烧脑也最值钱的一行
target_link_libraries(vector ${VEC_DEP_SCOPE} util)

# 警告选项也走 PUBLIC：vector 自己吃到 -Wall，app/vstats 同样吃到
target_link_libraries(vector PUBLIC strict_flags)

# =====================================================================
# 两个可执行文件：都是 vector 的消费者
# =====================================================================
add_executable(app app/main.c)                 # 只碰 vector.h
target_link_libraries(app PRIVATE vector)

add_executable(vstats tools/vstats.c)          # 同时碰 vector.h 和 util.h + UTIL_TAG
target_link_libraries(vstats PRIVATE vector)

# =====================================================================
# 把“CMake 眼里的 target 清单”打在 configure 输出里
# （第 01 章末尾说过：Makefile 的 target 和 CMake 的 target 不是同一个东西）
# =====================================================================
get_property(_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(_t IN LISTS _targets)
    message(STATUS "CMake target: ${_t}")
endforeach()
