# ============================================================================
# 迁移第 0 步一定会撞上的东西：cmake_minimum_required 的版本号
#
# 老仓库里 `cmake_minimum_required(VERSION 2.8.12)` 这种行，在 CMake 4.x 上不是警告，
# 是**硬错误**（配置直接失败）。这一节把原文抓下来当字典用：
# 报错里那三行"你可以怎么怎么办"是 CMake 自己给的解法，抄哪个要想清楚。
# ============================================================================

cmake_minimum_required(VERSION 2.8.12)
project(ch07minver LANGUAGES C)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
add_executable(minver_app "${ROOT}/src/leaf/leaf_a.c")
