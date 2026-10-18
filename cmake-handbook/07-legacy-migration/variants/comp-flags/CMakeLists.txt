# ============================================================================
# COMPILE_FLAGS（老属性）vs COMPILE_OPTIONS（现代命令）：三处能实测出来的差别
#
# 表面看两者都会出现在 flags.make 的 C_FLAGS 里，很像一回事。
# 这一份工程准备了四对靶子，前两对"看着一样"，后两对当场分开：
#   ① 同一份参数写两遍：属性是**覆盖**，命令是**追加**
#   ② 带空格的一个参数：属性是原样字符串（会被拆成两个参数），命令会被引号保护
#   ③ 生成器表达式：属性**不展开**，字面量原封不动漏进 Makefile，
#      然后被 make 自己展开成别的东西（$< 在 Makefile 里 = 第一个依赖）
#   ④ 什么都不加时两者的默认值
# ③ 是这一节最贵的一条：它是"老属性不经过 CMake 的 flag 派发"的铁证。
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(ch07compflags LANGUAGES C)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(SRC "${ROOT}/src/leaf/leaf_a.c")

# ①覆盖 vs 追加：同一个 target 写两遍
add_executable(cf_over "${SRC}")
set_target_properties(cf_over PROPERTIES COMPILE_FLAGS "-DCF_A=1")
set_target_properties(cf_over PROPERTIES COMPILE_FLAGS "-DCF_B=2")

add_executable(cf_append "${SRC}")
target_compile_options(cf_append PRIVATE -DCF_A=1)
target_compile_options(cf_append PRIVATE -DCF_B=2)

# ②带空格的一个参数
add_executable(cf_space "${SRC}")
set_target_properties(cf_space PROPERTIES COMPILE_FLAGS "-DCF_MSG=a b")

add_executable(cf_list "${SRC}")
target_compile_options(cf_list PRIVATE "-DCF_MSG=a b")

# ③生成器表达式
add_executable(cf_genex "${SRC}")
set_target_properties(cf_genex PROPERTIES COMPILE_FLAGS "-DCF_CFG=$<CONFIG>")

add_executable(cf_list_genex "${SRC}")
target_compile_options(cf_list_genex PRIVATE "-DCF_CFG=$<CONFIG>")

message(STATUS "comp-flags：六个 target，两两一对")
