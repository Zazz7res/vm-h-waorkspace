# 第三方目录：作者不认识本工程的任何头文件、任何宏，也不该认识。
# 它只有一行有用的话 —— 这一行就是本章"污染检测器"（flags.make 里 -I/-D 该是空的）。
add_executable(notepad "${CMAKE_CURRENT_LIST_DIR}/notepad.c")
