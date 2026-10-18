# ============================================================================
# "第三方库" mathx 自己的构建脚本。
#
# 它在现实里的身份是：某个已经发布了的库，作者已经替你把安装和交付做好了。
# 本章用 MX_EXPORT 这一个开关，让同一份源码以**两种形状**装到磁盘上：
#
#   MX_EXPORT=ON   现代上游：.a + 公开头 + lib/cmake/mathx/ 里的三个配置文件
#                  → 消费者可以直接 find_package(mathx)（Config 模式）
#   MX_EXPORT=OFF  上古上游：只有 .a + 公开头，什么 .cmake 都不给
#                  → 消费者只能靠自己写的 Findmathx.cmake（Module 模式）
#
# 这两种形状加起来，就是本章前半段所有实验的舞台。
# ============================================================================
cmake_minimum_required(VERSION 3.20)
project(mathx VERSION 1.3.0 LANGUAGES C)

option(MX_EXPORT "装出去时是否附送 mathxConfig.cmake（ON=现代上游，OFF=上古只给 .a 和头）" ON)

add_library(mathx STATIC src/mathx.c)

# 头目录用两条 genex（第 03 章的老朋友）：
#   BUILD_INTERFACE   = 给"和我在同一条构建树里的人"
#   INSTALL_INTERFACE = 给"装完之后从磁盘上找过来的人"，所以是相对安装前缀的
target_include_directories(mathx
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

# ALIAS 目标就是"名字空间写法"。第 03 章里我们叫它 Vector::vector；
# 有了它，消费者写 target_link_libraries(app PRIVATE mathx::mathx) 才不会被
# 同名的别的 target 撞车。install(EXPORT) 会把 NAMESPACE 拼在前面，
# 所以下面 NAMESPACE 必须写 mathx::，导出的名字才正好等于这个 ALIAS。
add_library(mathx::mathx ALIAS mathx)

include(GNUInstallDirs)

# ---------- 交付部分：无论 MX_EXPORT 开不开，这两条都要 ----------
install(TARGETS mathx
        EXPORT mathxTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# ---------- 只有"现代上游"才会做的部分 ----------
if(MX_EXPORT)
    include(CMakePackageConfigHelpers)     # 提供下面两个函数

    configure_package_config_file(
        cmake/mathxConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/mathxConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mathx)

    # 这个 helper 生成的 mathxConfigVersion.cmake 才是"版本能不能要"的裁判。
    # SameMajorVersion：主版本号相同就放行（要 1.0 → 1.3.0 OK；要 2.0 → 拒绝）。
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/mathxConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)

    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/mathxConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/mathxConfigVersion.cmake"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mathx)

    install(EXPORT mathxTargets
            NAMESPACE mathx::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/mathx)
endif()
