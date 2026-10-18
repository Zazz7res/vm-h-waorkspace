#ifndef UTIL_H
#define UTIL_H

/*
 * util.h —— vector 的**内部依赖**的头。
 *
 * 「内部」说的是意图：它不是给消费者用的。但本章会让你看到，
 * 意图在 CMake 里不算数，算数的是两件硬事实：
 *   ① vector 链接了它 → 它必须一起被导出（否则 install(EXPORT) 拒绝生成）
 *   ② 它的头会不会出现在消费者的 -I 里 → 取决于 vector 链接它时写 PRIVATE 还是 PUBLIC
 * 所以这个文件会被装到 <前缀>/include/vector/util.h ——
 * 路径多了一层「vector」，是在向用户暗示「这里头不是给你直接用的」。
 */

/* 把 x 夹进 [lo, hi]。lo > hi 时按非法处理，直接返回 lo（教学库不做断言）。 */
int u_clamp(int x, int lo, int hi);

/* 单次扩容允许的上限：动态数组一次最多长到这么多元素。 */
#define U_MAX_GROWTH 1024

#endif /* UTIL_H */
