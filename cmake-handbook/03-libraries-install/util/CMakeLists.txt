# =====================================================================
# util —— vector 的**内部**依赖
# =====================================================================
# 「内部」是意图，不是事实。本章后面会用 4 种拨法逼问这个意图：
#   ① 它必须被 install 吗？   ② 它的头必须被装出去吗？
#   ③ 消费者看不看得见它？   ④ 看不见的话，链接还成不成立？
# =====================================================================

# OBJECT 库不产出 libutil.a，只产出一堆 .o 等着被吸进别人身上；
# STATIC 库产出 libutil.a，谁链接它谁就得负责把它交付出去。
if(VECTOR_UTIL_AS_OBJECT)
    add_library(util OBJECT src/util.c)
else()
    add_library(util STATIC src/util.c)
endif()

# ── 头目录说两遍 ──────────────────────────────────────────────────────
# BUILD_INTERFACE：同一条构建树里的人（vector、app）→ 源码树里的 util/include
# INSTALL_INTERFACE：装完之后的人 → <安装前缀>/include/vector
# 为什么装到 include/**vector**/ 而不是 include/？
# 因为 util.h 是**别人的**内部名字，直接摆进公开的 include/ 会污染用户的全局命名空间。
target_include_directories(util PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/vector>)

# OBJECT 库不链接，但它的 .o 最终会进 libvector.a / libvector.so，
# 而共享库要求所有参与其中的机器码都是位置无关的 → 显式打开，别赌全局变量。
set_target_properties(util PROPERTIES POSITION_INDEPENDENT_CODE ON)

# =====================================================================
# 交付：只有被「放进导出集合」的 target 才允许出现在 install(EXPORT) 里
# =====================================================================
# 第 03 章的核心报错全部来自这一句开或关，所以它是个开关：
#   ON  → libutil.a 一起进包，consumer 链接得到它，一切正常
#   OFF → 配置期就拒绝生成：vector 要求 util，而 util 不在任何导出集合里
if(VECTOR_EXPORT_UTIL)
    install(TARGETS util EXPORT VectorTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    # util 进了导出集合，它的头目录就得真的存在，否则消费者拿到的 -I 指向空目录。
    # 装到 include/vector/ 这一层「藏起来」的目录里：路径是公开的，意图是内部的。
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/vector)
endif()
