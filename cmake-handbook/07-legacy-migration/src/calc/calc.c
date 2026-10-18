#include "ch07/calc.h"
#include <stdio.h>

/* calc_add 的实现：行为完全由两个宏决定。
 * 这个文件是 add_definitions vs target_compile_definitions 的靶子 ——
 * 宏传进来了，屏幕上就是数字；没传进来，屏幕上就是 0。 */

int calc_add(int a, int b) {
#if defined(CH07_MODE_DOUBLE)
    printf("[calc] 走的是 DOUBLE 分支\n");
    return a * 2 + b * 2;
#elif defined(CH07_MODE_PLUS)
    printf("[calc] 走的是 PLUS 分支\n");
    return a + b;
#else
    printf("[calc] 两个宏都没见到，只能返回 0\n");
    return 0;
#endif
}
