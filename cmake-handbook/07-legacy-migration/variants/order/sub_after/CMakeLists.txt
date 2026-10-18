# 在父目录写 include_directories **之后**被 add_subdirectory 进来。
add_executable(after_sub "${ROOT}/src/leaf/leaf_a.c")
get_target_property(PAS after_sub INCLUDE_DIRECTORIES)
message(STATUS "  after_sub（子目录，后加进来）的 INCLUDE_DIRECTORIES = [${PAS}]")
