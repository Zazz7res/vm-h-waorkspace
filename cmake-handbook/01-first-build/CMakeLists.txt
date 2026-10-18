# =====================================================================
# 最小可跑的 CMake 工程 —— 第 01 章专用例子
# =====================================================================
# 这个文件是**故意留到最短**的：只有 6 行有效代码，每一行都在 demo.sh 里被验证过。
# 讲"为什么需要这一行"的正文在同目录 README.md，这里只留最短的提示。
#
# 想看懂整章，顺序是：
#   bash demo.sh          先把整条流程跑一遍，看到真实输出
#   再读 README.md         每段输出对应哪一条机制
#   最后回来看这个文件      此时每行都有意义了
# =====================================================================

# ── 1. 我要的 CMake 最低版本 ─────────────────────────────────────────
# 这一行是"承诺"：低于 3.16 直接报错，而不是用一套老语法将就跑通。
# ⚠ CMake 4.x 已经不再接受 VERSION 3.5 及以下的声明，写 3.16 是安全下限。
cmake_minimum_required(VERSION 3.16)

# ── 2. 声明工程：名字 + 版本 + 用到的语言 ────────────────────────────
# LANGUAGES C 的意思是"只去找 C 编译器"。不写这一句，CMake 会连 C++ 和 ASM 一起探测，
# 白等几秒，而且机器上没装 g++ 时会直接配置失败（本机就没有 g++ —— 只写了 C 才跑得通）。
project(hello_cmake VERSION 0.1 LANGUAGES C)

# ── 3. 语言标准：对应 gcc 的 -std=c11 ────────────────────────────────
# 现代写法是 target 级的 `target_compile_features(hello PRIVATE c_std_11)`（第 02 章讲）；
# 这里先用全局变量，是为了让你先看到"这两个变量到底改变了什么"。
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)   # 编译器不支持 C11 就报错，别默默降级糊弄你

# ── 4. 一个可执行文件 = 一个 target ─────────────────────────────────
# 源文件写成相对路径，CMake 会相对"当前这个 CMakeLists 所在目录"解析，
# 于是你在任何别的地方执行 `cmake --build` 都不会找不着文件。
add_executable(hello src/main.c src/greet.c)

# ── 5. 编译选项挂在 target 上，不污染别人 ───────────────────────────
# PRIVATE = 只影响 hello 自己这一个 target。
# 对照老写法 add_compile_options(-Wall ...)：那个会影响本目录及**之后所有**子目录的 target。
target_compile_options(hello PRIVATE -Wall -Wextra -Wshadow)

# ── 6. 一个开关：把警告升级成错误（demo.sh 里会现场开给你看）──────────
option(HELLO_STRICT "把警告当成错误（用来体验 -D 开关和缓存）" OFF)
if(HELLO_STRICT)
    target_compile_options(hello PRIVATE -Werror)
endif()
