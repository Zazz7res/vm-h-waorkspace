#include <stdio.h>

/* notepad：假装是"从网上拷进来的第三方小程序"。
 *
 * 它只 include <stdio.h>，本工程的头目录一个都不需要，宏一个都不想要。
 * 老写法的问题是：include_directories / add_definitions 是"目录级"的，
 * 而这个目录是 add_subdirectory 进来的，所以它照样全吃。
 * 下面那个 #ifdef 就是它"被污染之后才会说话"的口子。 */

int main(void) {
#ifdef CH07_DEBUG_TRACE
    printf("[notepad] 有人给全工程打开了 trace，我被迫打印这行\n");
#endif
    printf("[notepad] 我只是想记一行笔记，第 1 行\n");
    return 0;
}
