#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* strdup 是 POSIX 的，不是 ISO C11 的。
   -std=c11（根 Makefile 那条路线）默认不把 POSIX 声明端出来；
   -std=gnu11（本文件 + CMAKE_C_EXTENSIONS ON 这条路线）会。
   第 7 节拿它当探针：同一句代码，两条路线一条报错一条安静。
   本机 gcc 15 把"隐式函数声明"直接判成 error，不是 warning —— 屏幕上看到的会是 error。 */
int main(void)
{
    char *name = strdup("gnu11-or-c11");
    if (name == NULL) {
        return 1;
    }
    printf("strdup ok: %s\n", name);
    free(name);
    return 0;
}
