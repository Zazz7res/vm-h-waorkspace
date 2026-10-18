# ============================================================================
# legacy/CMakeLists.txt 的改写结果：**这才是真实工程里该交出去的写法**
#
# 和 legacy 那份的差别只有"机制"，没有"意图"：同一份源码、同一批 target、
# 同一个 -Wall、同一个 -lfoo 背后那份库。所以两份工程之间应该能逐条 diff，
# 而 diff 出来该是"只有 notepad 少了它本来就不该有的东西"。
#
# 六个病灶各自换来什么：
#   ① include_directories      → target_include_directories(... PUBLIC/PRIVATE)
#   ② add_definitions          → target_compile_definitions(... PRIVATE)
#   ③ link_directories + -lfoo → IMPORTED target + .a 绝对路径
#   ④ file(GLOB)               → 写死的清单（加文件时必须回来改这一行）
#   ⑤ 全局变量传清单           → 一个 INTERFACE 库（ch07_deps）
#   ⑥ COMPILE_FLAGS / 无关键字  → COMPILE_OPTIONS / PRIVATE|PUBLIC|INTERFACE
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(ch07modern VERSION 0.1.0 LANGUAGES C)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)   # 和 legacy/ 共用同一份源码

set(CH07_MODE  "PLUS" CACHE STRING "calc 的行为开关")
set(CH07_STAMP "0"    CACHE STRING "打进二进制里的构建号")
set(CH07_FOO   ""     CACHE FILEPATH "要链的那一份 libfoo.a（绝对路径，不给目录）")
set(CH07_TRACE "OFF"  CACHE BOOL    "只给 app 打开 trace")

add_library(util STATIC "${ROOT}/src/util/util.c")
target_include_directories(util PUBLIC "${ROOT}/include")
target_compile_definitions(util PRIVATE CH07_STAMP=${CH07_STAMP})

add_library(calc STATIC "${ROOT}/src/calc/calc.c")
target_include_directories(calc PUBLIC "${ROOT}/include")
target_compile_definitions(calc PRIVATE CH07_MODE_${CH07_MODE}=1)
target_compile_options(calc PRIVATE -Wall -Wextra)      # ⑥ 的新写法：列表 + 走 CMake 派发

# ③ 的新写法：库是一个 IMPORTED target，路径写死，不参与任何 -L 竞争
add_library(Foo::foo STATIC IMPORTED)
set_target_properties(Foo::foo PROPERTIES IMPORTED_LOCATION "${CH07_FOO}")

# ⑤ 的新写法：一组要求收进一个 INTERFACE 库，名字只有一个，语义是传递的
add_library(ch07_deps INTERFACE)
target_link_libraries(ch07_deps INTERFACE calc util)

add_executable(app     "${ROOT}/src/app.c")
add_executable(greet   "${ROOT}/src/greet.c")
add_executable(fooprog "${ROOT}/src/fooprog.c")
target_link_libraries(app     PRIVATE ch07_deps)      # ⑥ 的新写法：关键字必须给
target_link_libraries(greet   PRIVATE util)
target_link_libraries(fooprog PRIVATE Foo::foo)       # ③ 的新写法：链的是那一份 .a 的路径
target_include_directories(fooprog PRIVATE "${ROOT}/include")   # 它自己 include 了 ch07/foo.h
if(CH07_TRACE)
    target_compile_definitions(app PRIVATE CH07_DEBUG_TRACE=1)   # ② 的新写法：只点名 app
endif()

# ④ 的新写法：清单写死。新增一个 .c 时，这一行必须被改，改了就重新 configure，
#    于是"加了文件"这件事一定会在版本库的 diff 里留痕。
set(CH07_LEAF_SRC
    "${ROOT}/src/leaf/leaf_a.c"
    "${ROOT}/src/leaf/leaf_b.c")

enable_testing()
foreach(_f IN LISTS CH07_LEAF_SRC)
    get_filename_component(_n "${_f}" NAME_WE)
    add_executable(${_n} "${_f}")
    add_test(NAME smoke_${_n} COMMAND ${_n})
endforeach()

add_subdirectory("${ROOT}/tp" tp_build)   # 第三方：一个本工程的关键字都没继承到
