# =====================================================================
#  第 08 章的工程 —— 仓库根那份 249 行 `CMakeLists.txt` 的**迷你复刻**
# =====================================================================
#
# 这份文件不是教材本体，是**解剖台**：
#   · 函数名、开关名、变量名、每一行的顺序，都尽量和仓库根那份一致；
#   · 章节从 56 节缩成 4 节，这样每条命令都跑得快，屏幕上看得清；
#   · 目录形状也照抄（`<章>/<节>/` 两段），所以 `string(REPLACE)` 那行
#     产出的 target 名会和真仓库差一个 `mini-` 前缀 —— 这本身是第 10 节的证据。
#
# 为什么全部实验都在沙箱里跑这份复刻、而不去拨仓库根那份：
#   仓库根的 `build-cmake/` 是唯一那份"真物证"，一旦在它上面重新 configure，
#   磁盘上的 flags.make / CMakeCache.txt 就不是原来那个状态了，
#   后面每一节"读它"的证据都会失效。所以本章只读它，改动全在这里。
#
# 用法（和真文件同一条）：
#     cmake -S . -B <构建目录>
#     cmake --build <构建目录> --parallel
#     ctest --test-dir <构建目录> --output-on-failure
#     cmake --build <构建目录> --target mini-01-array-static-array

cmake_minimum_required(VERSION 3.16)

project(ds_ch08
        VERSION 1.0
        DESCRIPTION "第 08 章：逐行拆解用的迷你复刻"
        LANGUAGES C)

# ---------- 和真文件一样：语言标准与产物去向确实是项目级的 ----------
set(CMAKE_C_STANDARD 11)              # 真文件注释写"对应 Makefile 里的 -std=c11"，
                                      # 实际产出 -std=gnu11 —— 差在哪，第 6、7 节演
set(CMAKE_C_STANDARD_REQUIRED ON)     # 第 6 节会把它拨成 OFF，看命令行变不变
set(CMAKE_C_EXTENSIONS ON)            # 第 6 节拨成 OFF：-std=gnu11 → -std=c11

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---------- 本项目的开关：只有 ASan 需要切 ----------
option(DS_WITH_ASAN "给每个可执行文件挂 AddressSanitizer + LeakSanitizer" OFF)

# 和 Makefile 里 CFLAGS 的警告集完全一致，这样两条路线的"零警告"是同一个标准
set(DS_WARNINGS -Wall -Wextra -Wshadow)

# =====================================================================
#  ds_add_leaf(<目录> <该目录下的 .c 文件...>)
#  —— 一节 = 一个可执行文件，本节的全部 .c 一起编译
#  这 7 行是本章第 2 部分逐行拆的对象，一行都没合并、一行都没改名
# =====================================================================
function(ds_add_leaf leaf_dir)
    string(REPLACE "/" "-" _target "${leaf_dir}")

    # ARGVN 是 CMake 的"剩余参数"清单；这里就是该目录下所有 .c
    add_executable(${_target} ${ARGN})

    # 本节叶子目录**自包含**：只 include 本目录的 .h 和标准库。
    # 这一行就是把那条约定固化进构建系统 —— 跨目录 include 会直接编译不过。
    target_include_directories(${_target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${leaf_dir}")

    target_compile_options(${_target} PRIVATE ${DS_WARNINGS})

    if(DS_WITH_ASAN)
        # -g 让 ASan 报到具体行号；-fno-omit-frame-pointer 让栈回溯可读
        target_compile_options(${_target} PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
        # 本机系统库不带共享版 asan，必须静态链接，否则运行时找不到 libasan.so
        target_link_options(${_target} PRIVATE -fsanitize=address -static-libasan)
    endif()

    # 每条 smoke 用例就是"把这节跑一遍，退出码必须是 0"
    add_test(NAME smoke_${_target} COMMAND ${_target})
endfunction()

enable_testing()

# =====================================================================
#  章节清单（4 节；真文件这里是 56 节、占了 138 行）
#  mini/ 这一层前缀是本章和真仓库唯一的结构差别，第 10 节拿它做证据
# =====================================================================

# ---------- 00 C 语言地基 ----------
ds_add_leaf(mini/00-c-foundation/pointer-basics  mini/00-c-foundation/pointer-basics/leaf.c)

# ---------- 01 数组 ----------
ds_add_leaf(mini/01-array/static-array           mini/01-array/static-array/leaf.c)

# ---------- 02 链表 ----------
# 这一节**故意只 malloc 不 free**：不开 ASan 时它退 0、零警告、全绿
ds_add_leaf(mini/02-linked-list/slist            mini/02-linked-list/slist/leaf.c)

# ---------- 03 栈 ----------
ds_add_leaf(mini/03-stack/array-stack            mini/03-stack/array-stack/leaf.c)

# =====================================================================
#  收尾：把本次配置的要点打出来，免得你忘了自己开没开 ASan
# =====================================================================
get_property(_all_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
list(LENGTH _all_targets _n_targets)

message(STATUS "--------------------------------------------------")
message(STATUS "  本项目：${PROJECT_NAME} ${PROJECT_VERSION} —— 第 08 章迷你复刻")
message(STATUS "  可执行 target 数量：${_n_targets}（一节一个）")
message(STATUS "  警告集：${DS_WARNINGS}")
message(STATUS "  ASan：${DS_WITH_ASAN} （想开就 -DDS_WITH_ASAN=ON 重新 configure）")
message(STATUS "  产物目录：${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "--------------------------------------------------")
