# 反面教材二：enable_testing() 写了，但一条 add_test 都没有。
#
# 和上一份连着的第二个陷阱：这回 CTestTestfile.cmake 生成了，ctest 也找到了它，
# 打印 No tests were found!!! —— 退出码**还是 0**。
# 于是"忘了写用例"的仓库在 CI 上永远全绿。
# 治它只能靠人：数一数用例条数（ctest -N 的行数），或者给汇总加一道门槛。
cmake_minimum_required(VERSION 3.20)
project(ZeroTests LANGUAGES C)

add_executable(ok "${CMAKE_CURRENT_LIST_DIR}/../../src/ok.c")
enable_testing()
# 手一抖，add_test 全没写
