# ============================================================================
# 无关键字 target_link_libraries(app calc util)：本机还工作吗？差别在哪？
#
# 这一份把同一条链接关系写三遍（plain / PRIVATE / PUBLIC），
# 然后同时取证两个地方：
#   1) 磁盘上的 link.txt —— 三者**一模一样**（这是本章要说清楚的：命令行 diff 抓不到它）
#   2) 配置期的两个属性 —— INTERFACE_LINK_LIBRARIES 一个是裸名字、一个是 $<LINK_ONLY:...>
#      差别要到"导出给别人"那天才炸（第 03 章第 11 节），所以只能靠属性提前抓到
#
# 最后一条：同一次调用里混用裸名字和关键字，本机直接报错（不给你静默的机会）。
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(ch07plain LANGUAGES C)

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

add_library(util STATIC "${ROOT}/src/util/util.c")
target_include_directories(util PUBLIC "${ROOT}/include")

# 库 calc 依赖 util，三种签名各来一份（target 名不同，互不影响）
add_library(calc_plain STATIC "${ROOT}/src/calc/calc.c")
target_include_directories(calc_plain PUBLIC "${ROOT}/include")
target_link_libraries(calc_plain util)                    # ← 无关键字
get_target_property(IP calc_plain INTERFACE_LINK_LIBRARIES)
message(STATUS "plain   的 INTERFACE_LINK_LIBRARIES = [${IP}]")

add_library(calc_priv STATIC "${ROOT}/src/calc/calc.c")
target_include_directories(calc_priv PUBLIC "${ROOT}/include")
target_link_libraries(calc_priv PRIVATE util)
get_target_property(IQ calc_priv INTERFACE_LINK_LIBRARIES)
message(STATUS "PRIVATE 的 INTERFACE_LINK_LIBRARIES = [${IQ}]")

add_library(calc_pub STATIC "${ROOT}/src/calc/calc.c")
target_include_directories(calc_pub PUBLIC "${ROOT}/include")
target_link_libraries(calc_pub PUBLIC util)
get_target_property(IR calc_pub INTERFACE_LINK_LIBRARIES)
message(STATUS "PUBLIC  的 INTERFACE_LINK_LIBRARIES = [${IR}]")

foreach(_t plain priv pub)
    add_executable("use_${_t}" "${ROOT}/src/app.c")
    
    if(_t STREQUAL "plain")
        target_link_libraries("use_${_t}" calc_plain)     # 消费者也用无关键字
    elseif(_t STREQUAL "priv")
        target_link_libraries("use_${_t}" PRIVATE calc_priv)
    else()
        target_link_libraries("use_${_t}" PRIVATE calc_pub)
    endif()
endforeach()
