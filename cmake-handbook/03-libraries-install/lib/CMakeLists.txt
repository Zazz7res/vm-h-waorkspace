# =====================================================================
# lib —— 本章的主角：一个真正要交付出去的库
# =====================================================================
# 这里不写 STATIC/SHARED，让它听 BUILD_SHARED_LIBS 的（标准开关的全部意义）。
add_library(vector src/vector.c)

# ── 对外头目录：两种写法，差别在 install(EXPORT) 那一刻才暴露 ──────────
if(VECTOR_PLAIN_INCLUDE)
    # ❌ 新手写法：裸绝对路径。CMake 把它同时当成 build 用和 install 用，
    #    但这个路径只存在于**你这台机器的源码树**里。
    #    同树编译一个字都不报错；一旦 install(EXPORT)，CMake 拒绝把一个
    #    「源码目录里的路径」写进交付给别人的文件 —— 当场报错。
    target_include_directories(vector PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
else()
    # ✅ 现代写法：同一件事说两遍，一遍给构建树里的人，一遍给付费装完包的人。
    #    $<INSTALL_INTERFACE:include> 里写的是**相对安装前缀**的路径，
    #    CMake 在消费者那边把它拼成 <前缀>/include，
    #    所以整个前缀目录挪走、换台机器解压，find_package 依然能用。
    target_include_directories(vector PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
endif()

# ── 内部头目录：PRIVATE，永远不传染，也永远不装出去 ────────────────────
# vector.c 里那句 #include "vector_internal.h" 全靠这一行才找得到。
target_include_directories(vector PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 公开头里用了 C11 的东西（for 里声明变量等）→ 消费者也必须按至少 C11 编译。
# PUBLIC 在这里的含义：这句话会被原样写进导出的 VectorTargets.cmake。
target_compile_features(vector PUBLIC c_std_11)

# 警告只约束库自己：消费者的代码质量不该由一个库来指定 -Werror。
target_compile_options(vector PRIVATE -Wall -Wextra -Wshadow)

# =====================================================================
# 内部依赖怎么接进来：本章的胜负手
# =====================================================================
if(VECTOR_UTIL_AS_OBJECT AND NOT VECTOR_EXPORT_UTIL)
    # 「彻底隐藏」那条路：既不 link，也不导出，只把 util 编译出来的 .o
    # 直接塞进 vector 自己的产物里。于是：
    #   · libvector.a 里真的有 util.c.o（demo 里用 ar t / nm 验）
    #   · 导出的 VectorTargets.cmake 里连 util 这个名字都不出现
    #   · install(EXPORT) 那一关不用过（因为没有依赖需要交代）
    # 代价：vector.c 想要 util.h，得自己把 -I 补上，而且这个 -I 也只能是 PRIVATE。
    target_sources(vector PRIVATE $<TARGET_OBJECTS:util>)
    target_include_directories(vector PRIVATE $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/util/include>)
else()
    # 常规路：真的建立一条依赖边。作用域由 VECTOR_DEP_SCOPE 决定，
    # 但**无论是 PRIVATE 还是 PUBLIC，util 都必须一起导出**（见 demo 第 9 节）。
    target_link_libraries(vector ${VECTOR_DEP_SCOPE} util)
endif()

# ALIAS：给树内的人一个「和装完之后同名」的写法。
# 这样 app/main.c 里写 Vector::vector，consumer/main.c 里也写 Vector::vector，
# 两边一字不差 —— 库内部改结构时，消费者代码不用跟着改。
add_library(Vector::vector ALIAS vector)

# =====================================================================
# 交付：把 target 变成别人能用的东西
# =====================================================================
# ① 库本体进导出集合。三种产物目的分别是：
#    ARCHIVE = 静态库 .a 去哪；LIBRARY = 共享库 .so 去哪；RUNTIME = Windows 的 .dll 去哪。
#    三个都写上，同一份 CMakeLists 在 Linux 和 Windows 上都不用改。
install(TARGETS vector EXPORT VectorTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# ② 公开头：注意装的是 include/ 这个**目录的内容**（结尾那个斜杠是关键：
#    写了斜杠装「目录里的东西」，不写斜杠连目录名一起装，会变成 include/include/vector.h）。
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# ③ 把 target 的全部信息写成一份 .cmake 交给别人 —— 这一步会读整张依赖图，
#    任何一条依赖边指向了没导出的 target，就在这里当场报错。
install(EXPORT VectorTargets
        NAMESPACE Vector::          # 导出的名字统一加前缀：util → Vector::util
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Vector)

# ④ find_package(Vector) 真正要找的两个入口文件（都在构建目录里先「生成」再装）。
#    configure_package_config_file：处理 @PACKAGE_INIT@，它会把安装前缀算好，
#    并把 INSTALL_DESTINATION 用来检查「这份包是不是被放到了预期的相对位置」。
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/VectorConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/VectorConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Vector)

#    版本文件决定「别人要求 1.0 / 2.0 时，我这个 1.0.2 算不算数」。
#    SameMajorVersion：1.x 一律满足 1.0 的要求；2.0 直接拒绝（demo 第 8 节现场撞一次）。
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/VectorConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/VectorConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/VectorConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Vector)
