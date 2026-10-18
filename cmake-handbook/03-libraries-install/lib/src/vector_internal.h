#ifndef VECTOR_INTERNAL_H
#define VECTOR_INTERNAL_H

/*
 * vector_internal.h —— **实现细节**，它不会被 install 到任何地方。
 *
 * 存在的唯一目的：给本章演示“PRIVATE 的头目录”。
 * 它放在 lib/src/ 里（不是 lib/include/），于是：
 *   · vector 自己编译时有 -I.../src，能看见这个文件
 *   · 消费者没有这个 -I，#include "vector_internal.h" 直接编译失败
 * 这就是“对外契约”和“对内实现”在目录结构上的物理分界。
 */
#define VEC_DEFAULT_CAP 4

#endif /* VECTOR_INTERNAL_H */
