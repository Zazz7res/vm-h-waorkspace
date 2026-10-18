# 反面教材一：写了 add_test，但整份文件里**没有** enable_testing()。
#
# 关键认知：CMake 不会因为"你注册了测试却没开测试总闸"而警告你半句。
# 配置一路绿灯，构建一路绿灯，ctest 到了现场才发现连 CTestTestfile.cmake 都没有。
# 更糟的是 ctest 那句 No test configuration file found! 的**退出码是 0** ——
# CI 上"ctest 成功"和"ctest 什么都没测"长得一模一样。
cmake_minimum_required(VERSION 3.20)
project(NoEnableTesting LANGUAGES C)

add_executable(ok "${CMAKE_CURRENT_LIST_DIR}/../../src/ok.c")
add_test(NAME should_be_invisible COMMAND ok)      # ← 这一行静默失效
