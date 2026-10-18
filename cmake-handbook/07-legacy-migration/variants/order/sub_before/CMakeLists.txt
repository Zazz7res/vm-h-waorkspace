# 在父目录写 include_directories **之前**被 add_subdirectory 进来。
add_executable(before_sub "${ROOT}/src/leaf/leaf_a.c")
get_target_property(PBS before_sub INCLUDE_DIRECTORIES)
message(STATUS "  before_sub（子目录，先加进来）的 INCLUDE_DIRECTORIES = [${PBS}]")
