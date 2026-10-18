# 反面教材四：add_compile_options 写得比 add_executable 晚 —— 等于没写。
#
# add_compile_options / add_link_options 改的是**目录属性**，
# 而 add_executable 在**它自己被调用的那一刻**给 target 拍的快照。
# 所以先后顺序就是全部：写在后面，target 快照里什么都没有。
# 这份文件故意按错误顺序写，第 15 节会 grep flags.make 证明 C_FLAGS 是空的，
# 然后跑越界程序 oob —— 它一声不吭退 0，你以为 ASan 在工作，其实它压根没进去。
cmake_minimum_required(VERSION 3.20)
project(SanitizerOrder LANGUAGES C)

add_executable(oob "${CMAKE_CURRENT_LIST_DIR}/../../src/oob.c")
target_compile_options(oob PRIVATE -Wall -Wextra)

# ★ 顺序错了：这一行在 add_executable 之后，快照已经拍完了。
add_compile_options(-fsanitize=address -g)
add_link_options(-fsanitize=address)

enable_testing()
add_test(NAME oob_thinks_it_is_safe COMMAND oob)
