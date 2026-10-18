#include "ch07/calc.h"
#include "ch07/util.h"
#include <stdio.h>

/* app：本章的主程序。两个"能被看见"的出口：
 *   calc_add   → 行为由 CH07_MODE_* 决定（宏）
 *   util_stamp → 打印编译期收到的 CH07_STAMP（宏）
 * 另外它自己有一个 CH07_DEBUG_TRACE 分支：现代写法里只有 app 该看到那个宏。
 * 它**不**需要 libfoo.a —— foo 的故事全在 src/fooprog.c 那一份里。 */

int main(void) {
#ifdef CH07_DEBUG_TRACE
    printf("[app] trace: 进 main 了，参数准备给 2 和 3\n");
#endif
    util_stamp();
    printf("[app] calc_add(2,3) = %d\n", calc_add(2, 3));
    return 0;
}
