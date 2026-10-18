#include <stdio.h>

/* 头文件查找顺序探测器。
 * 它只干一件事：把"到底哪一份同名头被编译器看见了"打印出来。
 * 这个信息在 CMakeLists 里是读不出来的 —— 两个目录都写了、都合法，
 * 赢家却只由命令行上 -I 的先后决定。 */

#include <ch07/hdrwin.h>

int main(void) {
    printf("[hdrwin] %s\n", CH07_WHO);
    return 0;
}
