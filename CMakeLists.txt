# =====================================================================
#  C 数据结构教材仓库 —— 现代 CMake 构建（target-based）
# =====================================================================
#
# 这份文件和根 `Makefile` 是**并存**的两条构建路线，各干一件事：
#   · Makefile   —— 你日常手搓单节用的（`make one DIR=...`、`make check`、`make status`）
#   · CMakeLists —— 用来体验"同一批代码在跨工具的构建系统里长什么样"，
#                   并且它本身就是一份现代 CMake 的可运行范例（配合 cmake-handbook 阅读）
#
# 用法（一律 out-of-source，产物绝不落在源码目录里）：
#     cmake -S . -B build-cmake
#     cmake --build build-cmake --parallel
#     ctest --test-dir build-cmake --output-on-failure
#
#   挂 ASan/LeakSanitizer（等价于 `make asan`）：
#     cmake -S . -B build-cmake-asan -DDS_WITH_ASAN=ON
#     cmake --build build-cmake-asan --parallel
#     ctest --test-dir build-cmake-asan --output-on-failure
#
#   只编某一节：
#     cmake --build build-cmake --target 01-array-dynamic-array
#
# ---------------------------------------------------------------------
# 这份文件刻意遵守的几条"现代 CMake"纪律（也是它作为范例的意义）
# ---------------------------------------------------------------------
# ① 全程 target-based：所有设置都落到具体 target 上（target_compile_options /
#    target_include_directories / target_link_options），**不使用**
#    `include_directories()`、`link_directories()`、`add_definitions()` 这类目录级全局污染。
#    理由：全局命令会悄悄影响"之后定义的所有 target"，而数据结构这门课每个 target
#    都是一个独立小世界，谁也不该被邻居的开关影响。
# ② **不写 `file(GLOB ...)` 自动收集源文件**。56 个目录的源文件是一个一个列出来的。
#    理由：GLOB 在 configure 时才扫目录 —— 新增一个 .c 不会让构建系统察觉，
#    于是出现"明明有这个文件却怎么都不参与编译"的玄学问题。显式清单看着啰嗦，
#    但每一次增删文件都会被迫重新 configure，错误当场暴露。
#    （本仓库另有 Makefile 用 `find` 自动发现目录，那是因为 Make 的 recipe 每次执行都重跑；
#     CMake 的清单只在生成阶段跑一次，两者的时机差别正是这条规矩的根。）
# ③ 重复 56 次的样板抽进一个 `ds_add_leaf()` 函数：一处定义规则，各处只报目录名和文件。
# ④ 每个 target 顺手注册成一条 ctest 用例，于是"56 节全部能跑通"变成一条命令可验证的事。
#
# 唯一的小麻烦：CMake 的 target 名不能含 `/`，所以目录 `01-array/dynamic-array`
# 变成 target `01-array-dynamic-array`（函数里那一行 string(REPLACE) 就是干这个的）。

cmake_minimum_required(VERSION 3.16)

project(ds_in_c
        VERSION 1.0
        DESCRIPTION "手写 C 数据结构教学仓库"
        LANGUAGES C)

# ---------- 全局的"语言标准"和"产物去向"：这两条确实是项目级的，放在这里合适 ----------
set(CMAKE_C_STANDARD 11)              # 对应 Makefile 里的 -std=c11
set(CMAKE_C_STANDARD_REQUIRED ON)     # 编译器不支持 C11 就直接报错，而不是偷偷降级
set(CMAKE_C_EXTENSIONS ON)            # 保留 GNU 扩展（我们没刻意用，但别因 strict 模式添乱）

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# ---------- 本项目的开关：只有 ASan 需要切 ----------
option(DS_WITH_ASAN "给每个可执行文件挂 AddressSanitizer + LeakSanitizer" OFF)

# 和 Makefile 里 CFLAGS 的警告集完全一致，这样两条路线的"零警告"是同一个标准
set(DS_WARNINGS -Wall -Wextra -Wshadow)

# =====================================================================
#  ds_add_leaf(<目录> <该目录下的 .c 文件...>)
#  —— 一节 = 一个可执行文件，本节的全部 .c 一起编译
# =====================================================================
function(ds_add_leaf leaf_dir)
    string(REPLACE "/" "-" _target "${leaf_dir}")

    # ARGVN 是 CMake 的"剩余参数"清单；这里就是该目录下所有 .c
    add_executable(${_target} ${ARGN})

    # 本节叶子目录**自包含**：只 include 本目录的 .h 和标准库。
    # 这一行就是把那条约定固化进构建系统 —— 跨目录 include 会直接编译不过。
    target_include_directories(${_target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/${leaf_dir}")

    target_compile_options(${_target} PRIVATE ${DS_WARNINGS})

    if(DS_WITH_ASAN)
        # -g 让 ASan 报到具体行号；-fno-omit-frame-pointer 让栈回溯可读
        target_compile_options(${_target} PRIVATE -fsanitize=address -g -fno-omit-frame-pointer)
        # 本机系统库不带共享版 asan，必须静态链接，否则运行时找不到 libasan.so
        target_link_options(${_target} PRIVATE -fsanitize=address -static-libasan)
    endif()

    # 每条 smoke 用例就是"把这节跑一遍，退出码必须是 0"
    add_test(NAME smoke_${_target} COMMAND ${_target})
endfunction()

enable_testing()

# =====================================================================
#  章节清单（按章 → 按目录顺序手工排列；新增一节就在这里加一行）
# =====================================================================

# ---------- 00 C 语言地基 ----------
ds_add_leaf(00-c-foundation/pointer-basics      00-c-foundation/pointer-basics/pointer-basics.c
                                             00-c-foundation/pointer-basics/main.c)
ds_add_leaf(00-c-foundation/struct-and-typedef  00-c-foundation/struct-and-typedef/struct-and-typedef.c
                                             00-c-foundation/struct-and-typedef/main.c)
ds_add_leaf(00-c-foundation/dynamic-memory      00-c-foundation/dynamic-memory/dynamic-memory.c
                                             00-c-foundation/dynamic-memory/main.c)

# ---------- 01 数组 ----------
ds_add_leaf(01-array/static-array             01-array/static-array/static-array.c
                                             01-array/static-array/main.c)
ds_add_leaf(01-array/dynamic-array            01-array/dynamic-array/dynamic-array.c
                                             01-array/dynamic-array/main.c)
ds_add_leaf(01-array/two-dimensional          01-array/two-dimensional/two-dimensional.c
                                             01-array/two-dimensional/main.c)

# ---------- 02 链表 ----------
ds_add_leaf(02-linked-list/singly             02-linked-list/singly/slist.c
                                             02-linked-list/singly/main.c)
ds_add_leaf(02-linked-list/doubly             02-linked-list/doubly/dlist.c
                                             02-linked-list/doubly/main.c)
ds_add_leaf(02-linked-list/circular-singly    02-linked-list/circular-singly/clist.c
                                             02-linked-list/circular-singly/main.c)
ds_add_leaf(02-linked-list/circular-doubly    02-linked-list/circular-doubly/cdlist.c
                                             02-linked-list/circular-doubly/main.c)
ds_add_leaf(02-linked-list/skip-list          02-linked-list/skip-list/ordlist.c
                                             02-linked-list/skip-list/skiplist.c
                                             02-linked-list/skip-list/main.c)

# ---------- 03 栈 ----------
ds_add_leaf(03-stack/array-stack              03-stack/array-stack/astack.c
                                             03-stack/array-stack/main.c)
ds_add_leaf(03-stack/linked-stack             03-stack/linked-stack/lstack.c
                                             03-stack/linked-stack/main.c)
ds_add_leaf(03-stack/monotonic-stack          03-stack/monotonic-stack/mstack.c
                                             03-stack/monotonic-stack/main.c)
ds_add_leaf(03-stack/expression-eval          03-stack/expression-eval/expr.c
                                             03-stack/expression-eval/main.c)

# ---------- 04 队列 ----------
ds_add_leaf(04-queue/array-queue              04-queue/array-queue/aqueue.c
                                             04-queue/array-queue/main.c)
ds_add_leaf(04-queue/linked-queue             04-queue/linked-queue/lqueue.c
                                             04-queue/linked-queue/main.c)
ds_add_leaf(04-queue/circular-queue           04-queue/circular-queue/cqueue.c
                                             04-queue/circular-queue/main.c)
ds_add_leaf(04-queue/deque                    04-queue/deque/deque.c
                                             04-queue/deque/main.c)
ds_add_leaf(04-queue/queue-by-stacks          04-queue/queue-by-stacks/istack.c
                                             04-queue/queue-by-stacks/qsq.c
                                             04-queue/queue-by-stacks/main.c)

# ---------- 05 树 ----------
ds_add_leaf(05-tree/binary-tree               05-tree/binary-tree/btree.c
                                             05-tree/binary-tree/main.c)
ds_add_leaf(05-tree/bst                       05-tree/bst/bst.c
                                             05-tree/bst/main.c)
ds_add_leaf(05-tree/avl-tree                  05-tree/avl-tree/mini_bst.c
                                             05-tree/avl-tree/avl.c
                                             05-tree/avl-tree/main.c)
ds_add_leaf(05-tree/red-black-tree            05-tree/red-black-tree/rbtree.c
                                             05-tree/red-black-tree/main.c)
ds_add_leaf(05-tree/b-tree                    05-tree/b-tree/btree.c
                                             05-tree/b-tree/main.c)
ds_add_leaf(05-tree/huffman-tree              05-tree/huffman-tree/huffman.c
                                             05-tree/huffman-tree/main.c)

# ---------- 06 堆 ----------
ds_add_leaf(06-heap/binary-heap               06-heap/binary-heap/heap.c
                                             06-heap/binary-heap/main.c)
ds_add_leaf(06-heap/priority-queue            06-heap/priority-queue/pqueue.c
                                             06-heap/priority-queue/main.c)
ds_add_leaf(06-heap/top-k                     06-heap/top-k/topk.c
                                             06-heap/top-k/main.c)

# ---------- 07 图 ----------
ds_add_leaf(07-graph/adjacency-matrix         07-graph/adjacency-matrix/gmatrix.c
                                             07-graph/adjacency-matrix/main.c)
ds_add_leaf(07-graph/adjacency-list           07-graph/adjacency-list/glist.c
                                             07-graph/adjacency-list/main.c)
ds_add_leaf(07-graph/bfs                      07-graph/bfs/gbfs.c
                                             07-graph/bfs/main.c)
ds_add_leaf(07-graph/dfs                      07-graph/dfs/gdfs.c
                                             07-graph/dfs/main.c)
ds_add_leaf(07-graph/connected-components     07-graph/connected-components/cc.c
                                             07-graph/connected-components/main.c)
ds_add_leaf(07-graph/topological-sort         07-graph/topological-sort/topo.c
                                             07-graph/topological-sort/main.c)
ds_add_leaf(07-graph/dijkstra                 07-graph/dijkstra/dijkstra.c
                                             07-graph/dijkstra/main.c)
ds_add_leaf(07-graph/bellman-ford             07-graph/bellman-ford/bellman.c
                                             07-graph/bellman-ford/main.c)
ds_add_leaf(07-graph/floyd                    07-graph/floyd/floyd.c
                                             07-graph/floyd/main.c)
ds_add_leaf(07-graph/prim-mst                 07-graph/prim-mst/prim.c
                                             07-graph/prim-mst/main.c)
ds_add_leaf(07-graph/kruskal-mst              07-graph/kruskal-mst/kruskal.c
                                             07-graph/kruskal-mst/main.c)

# ---------- 08 哈希 ----------
ds_add_leaf(08-hash/hash-table-chaining       08-hash/hash-table-chaining/chaining.c
                                             08-hash/hash-table-chaining/main.c)
ds_add_leaf(08-hash/hash-table-open-addressing 08-hash/hash-table-open-addressing/oa.c
                                              08-hash/hash-table-open-addressing/main.c)
ds_add_leaf(08-hash/trie                      08-hash/trie/trie.c
                                             08-hash/trie/main.c)

# ---------- 09 其他常见结构 ----------
ds_add_leaf(09-others/union-find              09-others/union-find/uf.c
                                             09-others/union-find/main.c)
ds_add_leaf(09-others/binary-indexed-tree     09-others/binary-indexed-tree/bit.c
                                             09-others/binary-indexed-tree/main.c)
ds_add_leaf(09-others/segment-tree            09-others/segment-tree/segtree.c
                                             09-others/segment-tree/main.c)
ds_add_leaf(09-others/lru-cache               09-others/lru-cache/lru.c
                                             09-others/lru-cache/main.c)
ds_add_leaf(09-others/bloom-filter            09-others/bloom-filter/bloom.c
                                             09-others/bloom-filter/main.c)

# ---------- 10 排序 ----------
ds_add_leaf(10-sorting/comparison-basic       10-sorting/comparison-basic/basic_sort.c
                                             10-sorting/comparison-basic/main.c)
ds_add_leaf(10-sorting/quick-sort             10-sorting/quick-sort/quick_sort.c
                                             10-sorting/quick-sort/main.c)
ds_add_leaf(10-sorting/merge-sort             10-sorting/merge-sort/merge_sort.c
                                             10-sorting/merge-sort/main.c)
ds_add_leaf(10-sorting/heap-sort              10-sorting/heap-sort/heap_sort.c
                                             10-sorting/heap-sort/main.c)
ds_add_leaf(10-sorting/non-comparison         10-sorting/non-comparison/nonsort.c
                                             10-sorting/non-comparison/main.c)

# ---------- 11 查找 ----------
ds_add_leaf(11-searching/binary-search        11-searching/binary-search/bs.c
                                             11-searching/binary-search/main.c)
ds_add_leaf(11-searching/rotated-array        11-searching/rotated-array/ra.c
                                             11-searching/rotated-array/main.c)
ds_add_leaf(11-searching/kth-smallest         11-searching/kth-smallest/kth.c
                                             11-searching/kth-smallest/main.c)

# =====================================================================
#  收尾：把本次配置的要点打出来，免得你忘了自己开没开 ASan
# =====================================================================
get_property(_all_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
list(LENGTH _all_targets _n_targets)

message(STATUS "--------------------------------------------------")
message(STATUS "  本项目：${PROJECT_NAME} ${PROJECT_VERSION} —— C11 数据结构教材")
message(STATUS "  可执行 target 数量：${_n_targets}（一节一个）")
message(STATUS "  警告集：${DS_WARNINGS}")
message(STATUS "  ASan：${DS_WITH_ASAN} （想开就 -DDS_WITH_ASAN=ON 重新 configure）")
message(STATUS "  产物目录：${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "  跑全部：ctest --test-dir ${CMAKE_BINARY_DIR} --output-on-failure")
message(STATUS "  注意：根 Makefile 与本文件互不影响，`make one DIR=...` 照常可用")
message(STATUS "--------------------------------------------------")
