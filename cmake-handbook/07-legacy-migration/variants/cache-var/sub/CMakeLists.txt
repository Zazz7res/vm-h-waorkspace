# 子目录：三个消费者，源码完全一样（src/app.c），只有"依赖清单从哪来"不同。
foreach(_n var cache iface)
    add_executable("cv_${_n}" "${ROOT}/src/app.c")
endforeach()

# ① 老路：全局变量
target_link_libraries(cv_var ${ALL_DEPS})

# ② 老路的加强版：CACHE 变量（能在命令行 -D 覆盖，看起来"很正规"）
target_link_libraries(cv_cache ${CACHE_DEPS})

# ③ 新路：一个 INTERFACE 库顶着整张清单
target_link_libraries(cv_iface PRIVATE ch07_deps)

# 父目录在 add_subdirectory **之后**才设的变量，这里看得见吗？
message(STATUS "  子目录看到的 LATE_DEPS = [${LATE_DEPS}]   ALL_DEPS = [${ALL_DEPS}]")
