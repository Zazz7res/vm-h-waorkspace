# =====================================================================
# app —— 同一条构建树里的消费者（对照组：consumer/ 是装完之后的人）
# =====================================================================
add_executable(app main.c)

# 这里写 vector 或 Vector::vector 都行（上面那条 ALIAS 的作用）。
# 本章统一用带命名空间的名字，和 consumer/ 那边一字不差，少一个心智负担。
target_link_libraries(app PRIVATE Vector::vector)

# 可执行文件也装进 bin/：这样安装树里能直接跑，也顺便演示 RUNTIME DESTINATION。
install(TARGETS app RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
