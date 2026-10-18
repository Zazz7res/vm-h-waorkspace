#include "ch07/util.h"
#include <stdio.h>

/* util.c 是"宏有没有真的传进编译器"的探针。
 * CH07_STAMP 没定义时打印 0，定义了就把它的值原样打印出来。
 * 于是同一份源码在不同 flags 下会给出不同的屏幕 —— 不用读 CMakeLists 也能看出来。 */

void util_stamp(void) {
#ifdef CH07_STAMP
    printf("[util] 编译器给我的 CH07_STAMP = %d\n", CH07_STAMP);
#else
    printf("[util] 没有任何人给我 CH07_STAMP（= 0）\n");
#endif
}
