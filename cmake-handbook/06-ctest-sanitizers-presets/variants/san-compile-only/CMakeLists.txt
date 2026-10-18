# 反面教材五：只给编译期加了 -fsanitize=address，忘了链接期。
#
# ASan 是两半的活：
#   编译期 -fsanitize=address → 往你的指令中间插检查代码，同时留下对 asan 运行时的调用
#   链接期 -fsanitize=address → 把 asan 运行时（libasan）链进来
# 只加前半：编译一路顺风，最后链接才炸，报的是最唬人的
#   undefined reference to '__asan_init'
#   undefined reference to '__asan_version_mismatch_check_v8'
# 看到 __asan_ 打头的未定义符号，结论只有一个：编译期加了、链接期没加。
cmake_minimum_required(VERSION 3.20)
project(SanitizerCompileOnly LANGUAGES C)

add_executable(oob "${CMAKE_CURRENT_LIST_DIR}/../../src/oob.c")
target_compile_options(oob PRIVATE -Wall -Wextra)
target_compile_options(oob PRIVATE -fsanitize=address -g)   # ★ 只有这一半
# target_link_options(oob PRIVATE -fsanitize=address)        # ← 另一半"忘了"
