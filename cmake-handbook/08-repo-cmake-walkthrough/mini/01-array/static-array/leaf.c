#include <stdio.h>
#include "sarr.h"

/* 第 01 节：静态数组。同样不碰堆，是第二个"永远该绿"的对照组。 */
int main(void)
{
    int data[5] = {3, 1, 4, 1, 5};

    printf("static-array sum=%d\n", arr_sum(data, 5));   /* 3+1+4+1+5 = 14 */
    return 0;
}
