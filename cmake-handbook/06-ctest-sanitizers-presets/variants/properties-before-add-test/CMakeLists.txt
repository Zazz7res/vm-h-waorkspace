# 反面教材六：set_tests_properties 写在 add_test 前面 / 或者名字打错。
#
# 这条和上面几条不一样：它**会**报错，而且是配置期就报错。
# 这是本章最好的一课 —— CTest 的两种错法里，"响的"比"哑的"值钱得多：
#   测试名打错 → Configuring incomplete, errors occurred!（当场告诉你）
#   忘了 enable_testing() → 什么都不说（第 1 节）
# 所以真出问题时，先去 CTestTestfile.cmake 里核对注册的是哪几行。
cmake_minimum_required(VERSION 3.20)
project(PropertiesFirst LANGUAGES C)

add_executable(ok "${CMAKE_CURRENT_LIST_DIR}/../../src/ok.c")
enable_testing()

# 先给属性，后注册用例：此刻 ctest 的名单里还没有 smoke_ok。
set_tests_properties(smoke_ok PROPERTIES PASS_REGULAR_EXPRESSION "结果 = 42\n")
add_test(NAME smoke_ok COMMAND ok)
