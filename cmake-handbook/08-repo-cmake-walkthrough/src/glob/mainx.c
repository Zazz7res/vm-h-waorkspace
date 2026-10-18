#include <stdio.h>
#include <stdlib.h>

/* 只有这一份 .c 时，GLOB 收到的就是它。
   第 20 节会往同一个目录里再丢一份 ctor.c，然后**不重新 configure** 直接 --build：
   屏幕上照样是 "[100%] Built target app"，而新文件一个字都没进去。 */
int main(void)
{
    printf("glob-probe main only\n");
    return 0;
}
