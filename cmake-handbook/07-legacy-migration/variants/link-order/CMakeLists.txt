# ============================================================================
# link_directories 的两个真病，一次演完：
#   病一：谁赢由 -L 的出现顺序决定（ORDER=ab / ba，两份 libfoo.a 同名不同内容）
#   病二：它**是快照语义** —— 写在 add_executable 之后，链接行里一个 -L 都没有
#
# 注意：病二和 include_directories/add_definitions 恰好相反（那两个是生成期读目录属性，
# 同目录的 target 不分先后都能吃到）。"老写法都必须在最前面"这句话把三种东西说成了一种，
# 这就是为什么这一节要把三条命令分开跑。
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(ch07linkorder LANGUAGES C)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(FOO_A "" CACHE PATH "第一份 libfoo.a 的目录")
set(FOO_B "" CACHE PATH "第二份 libfoo.a 的目录")
set(ORDER "ab" CACHE STRING "-L 的顺序：ab 或 ba")
set(WHEN "before" CACHE STRING "link_directories 写在 add_executable 之前(before) 还是之后(after)")

if(WHEN STREQUAL "before")
    if(ORDER STREQUAL "ba")
        link_directories("${FOO_B}" "${FOO_A}")
    else()
        link_directories("${FOO_A}" "${FOO_B}")
    endif()
endif()

add_executable(orderapp "${ROOT}/src/fooprog.c")
include_directories("${ROOT}/include")     # 这一份只谈 -L，头目录就用老写法给足
target_link_libraries(orderapp foo)                       # ← 无关键字：只给名字，让 -L 去猜

if(WHEN STREQUAL "after")
    if(ORDER STREQUAL "ba")
        link_directories("${FOO_B}" "${FOO_A}")
    else()
        link_directories("${FOO_A}" "${FOO_B}")
    endif()
endif()

message(STATUS "link-order：WHEN=${WHEN} ORDER=${ORDER}")
