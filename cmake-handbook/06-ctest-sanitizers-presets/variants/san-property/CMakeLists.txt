# 反面教材三：听说有个 SANITIZER 目标属性，set 上就以为开了 ASan。
#
# 这份 CMakeLists 的杀伤力在于：它在**每一步都表现得像成功了**——
#   set_target_properties(... SANITIZER "address")  不报错
#   get_target_property(... SANITIZER)              原样读回来
#   配置、构建、ctest 全绿
# 但 flags.make 里一个 -fsanitize 都没有（第 13 节 grep 给你看）。
# 原因：CMake 对**不认识的属性名**照单全收、静默存储，既不校验也不报警。
# 结论（本章反复用）：唯一可信的地方是真正交给编译器的命令行，
#   Unix Makefiles 看 CMakeFiles/<target>.dir/flags.make，Ninja 看 compile_commands.json。
cmake_minimum_required(VERSION 3.20)
project(SanitizerProperty LANGUAGES C)

add_executable(oob "${CMAKE_CURRENT_LIST_DIR}/../../src/oob.c")
target_compile_options(oob PRIVATE -Wall -Wextra)

# 网传写法，本机 CMake 4.2.3 + gcc 15.2.0：存进去了，但什么都不做。
set_target_properties(oob PROPERTIES SANITIZER "address")
get_target_property(_back oob SANITIZER)
message(STATUS "读回来的 SANITIZER = ${_back}")

# 连中文属性名都收 —— 这一行就是"未知属性不报错"的铁证。
set_target_properties(oob PROPERTIES "随便写的属性名" "照样存进去")
get_target_property(_cn oob "随便写的属性名")
message(STATUS "读回来的中文属性 = ${_cn}")

enable_testing()
add_test(NAME oob_quietly_passes COMMAND oob)
