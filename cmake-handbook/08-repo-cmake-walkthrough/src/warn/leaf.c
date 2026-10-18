#include <stdio.h>

/* 只有 -Wshadow 能发现的毛病：内层的 total 把外层的 total 挡住了。
   -Wall、-Wextra 对它一个字都不说。
   第 14 节拿它当尺子：警告集那一行删掉 / 只留 -Wall，屏幕上立刻见分晓。 */
int main(void)
{
    int total = 10;

    {
        int total = 32;                 /* ← -Wshadow 在这里响 */
        printf("inner total=%d\n", total);
    }

    printf("outer total=%d\n", total);  /* 还是 10：内层那次赋值从来没碰到它 */
    return 0;
}
