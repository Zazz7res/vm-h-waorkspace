#include <stdio.h>

/* leaf_a：一个自包含的小程序（自带 main，不依赖本章的库）。
 * variants/glob 那个工程用 file(GLOB src/leaf/*.c) 给每个 leaf 文件
 * 造一个可执行 + 一条用例，所以它是 GLOB 一节"数名单"的道具。 */

int main(void) {
    int total = 0;
    for (int i = 1; i <= 4; i++) total += i;
    printf("[leaf_a] 1+2+3+4 = %d\n", total);
    return 0;
}
