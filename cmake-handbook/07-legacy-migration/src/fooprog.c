#include "ch07/foo.h"
#include <stdio.h>

/* fooprog：整个工程里唯一需要 libfoo.a 的程序。
 * 它存在的全部意义：让"-L 的顺序"变成一件**能从屏幕上读出来**的事 ——
 * 链到 v1 就打印 1，链到 v2 就打印 2，源码一个字都没改。 */

int main(void) {
    printf("[fooprog] foo_tag() = %d\n", foo_tag());
    return 0;
}
