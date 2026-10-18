#include "ch07/foo.h"

/* foo.c：一份源码，编译出两份 libfoo.a。
 * 两份唯一的区别是编译时给的 -DCH07_FOO_V=1 / =2，所以链接时选中哪一份，
 * 程序屏幕上就出现哪个数字。
 * 为什么要造两份同名的：这是 -L 顺序这件事**唯一**能被看见的方式 ——
 * 只有一份库的时候，顺序怎么乱都"能跑"，谁也不会发现问题。 */

#ifndef CH07_FOO_V
#define CH07_FOO_V 0        /* 没给宏的话就是 0，让"谁都没链上"也能被看出来 */
#endif

int foo_tag(void) {
    return CH07_FOO_V;
}
