#ifndef CH07_CALC_H
#define CH07_CALC_H

/* 本章示范工程唯一的"公开头"。
 *
 * calc_add 的返回值由两个宏决定（专门用来演 add_definitions / target_compile_definitions）：
 *   CH07_MODE_DOUBLE 有 → 先各自翻倍再相加
 *   CH07_MODE_PLUS   有 → 老老实实相加
 *   两个都没有         → 返回 0
 * 为什么要设计成"没宏就返回 0"：这样"宏到底传没传进编译器"就变成一件
 * 既能从 flags.make 看出来、又能从程序输出看出来的事 —— 两本账互相印证。
 */
int calc_add(int a, int b);

/* 只有 src/calc/calc_extra.c 提供实现。
 * 这个函数是 file(GLOB) 那一节的靶子：新增的那个 .c 没被清单收进来时，
 * 链接就停在 undefined reference（或者安静地少一个用例）。 */
int calc_twice(int x);

#endif /* CH07_CALC_H */
