# ============================================================================
# 时机探针：include_directories / add_definitions 到底"什么时候算"
#
# 这份工程里有三个 target，分别站在三个位置上：
#   early      —— 在 include_directories **之前** add_executable 的
#   before_sub —— 在它之前的子目录里
#   after_sub  —— 在它之后的子目录里
# 配置期把三者的 INCLUDE_DIRECTORIES 属性直接 message 出来，
# 于是"老写法必须写在 add_executable 前面"这句流传最广的话，当场可以判真假。
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(ch07order LANGUAGES C)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(SRC "${ROOT}/src/leaf/leaf_a.c")

# --- 靶子一：target 先创建，全局指令后写 -------------------------------------
add_executable(early "${SRC}")
get_target_property(P1 early INCLUDE_DIRECTORIES)
message(STATUS "① 刚 add_executable 完：early 的 INCLUDE_DIRECTORIES = [${P1}]")

add_subdirectory(sub_before)          # 这个子目录在 include_directories 之前

include_directories("${ROOT}/include")
add_definitions(-DORDER_MARK=1)

get_target_property(P2 early INCLUDE_DIRECTORIES)
message(STATUS "② include_directories 之后：early 的 INCLUDE_DIRECTORIES = [${P2}]")

add_subdirectory(sub_after)           # 这个子目录在 include_directories 之后

add_executable(late "${SRC}")
get_target_property(P3 late INCLUDE_DIRECTORIES)
message(STATUS "③ 在它之后才创建的 late 的 INCLUDE_DIRECTORIES = [${P3}]")
