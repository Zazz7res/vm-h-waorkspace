#include "ch07/util.h"
#include <stdio.h>

/* greet：第二个可执行文件。它只需要 util_stamp()，压根不需要 calc.h。
 *
 * ★ 它是本章的"污染检测器"：下面那个 #ifdef 分支不是它自己要的功能，
 *   是别人（老写法的 add_definitions）替它打开的。
 *   现代写法只给需要的 target 挂宏，这里就该是安静的。 */

int main(void) {
#ifdef CH07_DEBUG_TRACE
    printf("[greet] 注意：有人给全工程打开了 CH07_DEBUG_TRACE，我也被打了 trace\n");
#endif
    util_stamp();
    printf("[greet] 你好，我只是来问个好\n");
    return 0;
}
