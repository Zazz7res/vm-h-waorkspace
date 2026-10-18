#include <stdio.h>

/* leaf_b：第二个自包含小程序。和 leaf_a 一样，一个文件 = 一个 target = 一条用例。 */

int main(void) {
    int square = 7 * 7;
    printf("[leaf_b] 7 的平方 = %d\n", square);
    return 0;
}
