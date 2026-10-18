# ============================================================================
# 第 07 章《老写法 → 新写法迁移》的实验室工程
#
# 这份文件是本章的"靶场"：它同时装着**两套写法**，由下面 7 个 CH07_LEG_* 开关决定
# 这一份构建用哪一套。开关全 OFF = 现代版；只开一个 = "只有这一处还没改过来"。
# 于是"迁移"这件事在这里变成可以一条命令复现的事：
#
#   配置两次（一次老写法、一次新写法）→ 比两份 flags.make / link.txt 的字符
#
# 为什么要这么设计：改写安不安全，不看"能不能跑"，看**传给编译器的字符是否一致**。
# 详见章 README 的「一句话版本」和第 4 节。
#
# ★ 这份文件本身故意保留了 7 处 if(CH07_LEG_*) 分叉，它是实验装置，不是写法示范。
#   真实工程里应该长什么样，看同目录的 modern/CMakeLists.txt（那才是答案）。
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(ch07 VERSION 0.1.0 LANGUAGES C)

# ---------------------------------------------------------------------------
# 一、七把"回滚开关"：每把只关掉一种现代写法，换来对应的那条老指令
# ---------------------------------------------------------------------------
set(CH07_LEG_INCLUDE  "OFF" CACHE BOOL "用 include_directories() 代替 target_include_directories()")
set(CH07_LEG_DEFS     "OFF" CACHE BOOL "用 add_definitions() 代替 target_compile_definitions()")
set(CH07_LEG_LINK     "OFF" CACHE BOOL "用 link_directories() + -lfoo 代替给 target 一个 .a 绝对路径")
set(CH07_LEG_GLOB     "OFF" CACHE BOOL "用 file(GLOB) 代替写死的源文件清单")
set(CH07_LEG_FLAGS    "OFF" CACHE BOOL "用 SET_TARGET_PROPERTIES(COMPILE_FLAGS) 代替 target_compile_options")
set(CH07_LEG_PLAIN    "OFF" CACHE BOOL "用无关键字 target_link_libraries(app 库) 代替 PRIVATE 关键字")
set(CH07_LEG_CACHEVAR "OFF" CACHE BOOL "用全局变量传依赖清单 代替 INTERFACE 库")

# 二、行为旋钮：让"宏到底传没传进去"在程序屏幕上可见
set(CH07_MODE  "PLUS" CACHE STRING "calc_add 走哪个分支：PLUS 或 DOUBLE")
set(CH07_STAMP "0"    CACHE STRING "CH07_STAMP 宏的值，util_stamp() 会把它打印出来")
set(CH07_TRACE "OFF"  CACHE BOOL "打开 CH07_DEBUG_TRACE（现代写法只给 app，老写法给全工程）")

# 三、两份同名 libfoo.a 的位置（由 demo.sh 在沙箱里用 gcc+ar 现场造出来）
set(CH07_FOO_DIR1 "" CACHE PATH "第一份 libfoo.a 所在目录")
set(CH07_FOO_DIR2 "" CACHE PATH "第二份 libfoo.a 所在目录")
set(CH07_FOO_PICK "1" CACHE STRING "现代写法（给绝对路径那一版）选第几份：1 或 2")
set(CH07_FOO_ORDER "12" CACHE STRING "老写法（link_directories 那一版）两个 -L 的先后：12 或 21")

set(CH07_ROOT "${CMAKE_CURRENT_LIST_DIR}")

# ---------------------------------------------------------------------------
# 四、老写法那一半：三条目录级全局指令，全都写在 target 之前（老教程都这么教）
# ---------------------------------------------------------------------------
if(CH07_LEG_INCLUDE)
    include_directories("${CH07_ROOT}/include")
endif()

if(CH07_LEG_DEFS)
    add_definitions(-DCH07_MODE_${CH07_MODE}=1 -DCH07_STAMP=${CH07_STAMP})
    if(CH07_TRACE)
        add_definitions(-DCH07_DEBUG_TRACE=1)      # ← 全工程，包括 greet 和第三方 notepad
    endif()
endif()

if(CH07_LEG_LINK)
    if(CH07_FOO_ORDER STREQUAL "21")
        link_directories("${CH07_FOO_DIR2}" "${CH07_FOO_DIR1}")
    else()
        link_directories("${CH07_FOO_DIR1}" "${CH07_FOO_DIR2}")
    endif()
endif()

# ---------------------------------------------------------------------------
# 五、两个内部静态库
# ---------------------------------------------------------------------------
add_library(util STATIC "${CH07_ROOT}/src/util/util.c")
add_library(calc STATIC "${CH07_ROOT}/src/calc/calc.c")

if(NOT CH07_LEG_INCLUDE)
    # PUBLIC：库自己要，链接它的人也一起要（第 02 章的主题）
    target_include_directories(util PUBLIC "${CH07_ROOT}/include")
    target_include_directories(calc PUBLIC "${CH07_ROOT}/include")
endif()

if(NOT CH07_LEG_DEFS)
    # PRIVATE：这两个宏只有库自己的 .c 用得到，不该外传
    target_compile_definitions(calc PRIVATE CH07_MODE_${CH07_MODE}=1)
    target_compile_definitions(util PRIVATE CH07_STAMP=${CH07_STAMP})
endif()

if(CH07_LEG_FLAGS)
    set_target_properties(calc PROPERTIES COMPILE_FLAGS "-Wall -Wextra -DLEG_FLAGS=1")
else()
    target_compile_options(calc PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# 六、foo：两份同名不同内容的 libfoo.a（由 demo.sh 用 gcc + ar 现场造在沙箱里）。
#     只有给了那两个目录，本工程的链接故事才出场 —— 不给就是一个干净的现代工程。
#     老写法：link_directories + -lfoo，谁赢看 -L 的顺序
#     新写法：IMPORTED target + .a 的绝对路径，选谁写在文件里，屏幕上看得见
# ---------------------------------------------------------------------------
add_library(FooImported STATIC IMPORTED)
if(CH07_FOO_PICK STREQUAL "2")
    set_target_properties(FooImported PROPERTIES IMPORTED_LOCATION "${CH07_FOO_DIR2}/libfoo.a")
else()
    set_target_properties(FooImported PROPERTIES IMPORTED_LOCATION "${CH07_FOO_DIR1}/libfoo.a")
endif()

if(CH07_LEG_LINK)
    set(CH07_FOO_ITEM foo)             # → 链接行里是 -lfoo，赢的是 -L 里排前面的那份
else()
    set(CH07_FOO_ITEM FooImported)     # → 链接行里是一个 .a 的绝对路径，没有 -L
endif()

if(CH07_FOO_DIR1 AND CH07_FOO_DIR2)
    add_executable(fooprog "${CH07_ROOT}/src/fooprog.c")
    if(NOT CH07_LEG_INCLUDE)
        target_include_directories(fooprog PRIVATE "${CH07_ROOT}/include")
    endif()
    if(CH07_LEG_PLAIN)
        target_link_libraries(fooprog ${CH07_FOO_ITEM})
    else()
        target_link_libraries(fooprog PRIVATE ${CH07_FOO_ITEM})
    endif()
endif()

# ---------------------------------------------------------------------------
# 七、依赖清单怎么传给消费者：全局变量 vs 一个 INTERFACE 库
# ---------------------------------------------------------------------------
add_library(ch07_deps INTERFACE)
target_link_libraries(ch07_deps INTERFACE calc util)
if(CH07_LEG_CACHEVAR)
    set(CH07_ALL_DEPS calc util)       # 普通变量：谁都能改写它，且没有传递语义
else()
    set(CH07_ALL_DEPS ch07_deps)       # 一个名字，背后是一组要求
endif()

# ---------------------------------------------------------------------------
# 八、消费者
# ---------------------------------------------------------------------------
add_executable(app   "${CH07_ROOT}/src/app.c")
add_executable(greet "${CH07_ROOT}/src/greet.c")

if(CH07_LEG_PLAIN)
    target_link_libraries(app ${CH07_ALL_DEPS})
    target_link_libraries(greet util)
else()
    target_link_libraries(app PRIVATE ${CH07_ALL_DEPS})
    target_link_libraries(greet PRIVATE util)
endif()

if(CH07_TRACE AND NOT CH07_LEG_DEFS)
    target_compile_definitions(app PRIVATE CH07_DEBUG_TRACE=1)
endif()

# ---------------------------------------------------------------------------
# 九、第三方目录：它一个本工程的东西都不该要（污染检测器）
# ---------------------------------------------------------------------------
add_subdirectory(tp)

# ---------------------------------------------------------------------------
# 十、leaf：一个 .c = 一个可执行 + 一条用例。CH07_LEG_GLOB 决定清单怎么来
# ---------------------------------------------------------------------------
if(CH07_LEG_GLOB)
    file(GLOB CH07_LEAF_SRC "${CH07_ROOT}/src/leaf/*.c")
else()
    set(CH07_LEAF_SRC
        "${CH07_ROOT}/src/leaf/leaf_a.c"
        "${CH07_ROOT}/src/leaf/leaf_b.c")
endif()

enable_testing()
foreach(_f IN LISTS CH07_LEAF_SRC)
    get_filename_component(_n "${_f}" NAME_WE)
    add_executable(${_n} "${_f}")
    add_test(NAME smoke_${_n} COMMAND ${_n})
endforeach()

message(STATUS "第 07 章实验室：MODE=${CH07_MODE} STAMP=${CH07_STAMP} TRACE=${CH07_TRACE}")
message(STATUS "  回滚开关：INCLUDE=${CH07_LEG_INCLUDE} DEFS=${CH07_LEG_DEFS} LINK=${CH07_LEG_LINK}")
message(STATUS "            GLOB=${CH07_LEG_GLOB} FLAGS=${CH07_LEG_FLAGS} PLAIN=${CH07_LEG_PLAIN} CACHEVAR=${CH07_LEG_CACHEVAR}")
message(STATUS "  leaf 清单里有 ${CH07_LEAF_SRC} 这几个文件")
