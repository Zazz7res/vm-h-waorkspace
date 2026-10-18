# ============================================================================
# "把依赖清单传下去"的两条路：全局变量 vs 一个 INTERFACE 库
#
# 老写法：父目录 set(ALL_DEPS calc util)，子目录 target_link_libraries(${ALL_DEPS})。
# 它当然能用 —— 但它是**变量**，于是有三个后果：
#   ① 只有 add_subdirectory 之后设的值才看不见（快照），反过来"设晚了"就静默变空
#   ② 任何子目录都能 set(ALL_DEPS ...) 改写父目录以外的一切
#   ③ 传下去的是裸库名，谁都不带 PUBLIC/PRIVATE 语义 → 导出时变成别人的坑（第 03 章）
# 新写法：清单收进 add_library(ch07_deps INTERFACE)，传下去的只有一个名字。
#
# 这一份把两条路各配一个 target，用同一份源码，diff 它们的 link.txt。
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(ch07cachevar LANGUAGES C)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

add_library(calc STATIC "${ROOT}/src/calc/calc.c")
target_include_directories(calc PUBLIC "${ROOT}/include")
add_library(util STATIC "${ROOT}/src/util/util.c")
target_include_directories(util PUBLIC "${ROOT}/include")

add_library(ch07_deps INTERFACE)
target_link_libraries(ch07_deps INTERFACE calc util)

# 老路：普通全局变量当清单（顺便演一次 CACHE 变量当清单的坑，见 SET(... CACHE) 那条）
set(ALL_DEPS calc util)
set(CACHE_DEPS calc util CACHE STRING "假装是给别人 -D 覆盖用的开关")

add_subdirectory(sub)
set(LATE_DEPS calc)          # ★ 故意设在 add_subdirectory 之后：子目录已经跑完了

message(STATUS "cache-var：父目录 ALL_DEPS=[${ALL_DEPS}]  CACHE_DEPS=[${CACHE_DEPS}]")
