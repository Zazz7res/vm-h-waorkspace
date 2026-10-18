/* greet.c —— 和 main.c 一起被 add_executable 列进去，编译成两个 .o 再链接 */
#include <stdio.h>
#include "greet.h"

int greet_build(char *buf, size_t n, const char *name)
{
    if (buf == NULL || n == 0 || name == NULL) {
        return -1;                     /* 边界：宁可返回错误，也不要写坏调用者的内存 */
    }
    /* snprintf 保证结尾一定带 '\0'（只要 n > 0），这是它比 sprintf 安全的唯一理由。
     * 返回值是"本该写多少字节"，所以 返回值 >= n 就说明被截断了。 */
    return snprintf(buf, n, "你好, %s!", name);
}
