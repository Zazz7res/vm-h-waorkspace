#include <stdio.h>
#include "calc.h"

/* ok —— 全场唯一"从生到死都正确"的程序。
   它存在的意义是当参照物：退出码 0、输出「结果 = 42」、不碰越界内存。
   demo 里凡是"裁判本身是不是坏了"的判断，都拿它来对照。 */
int main(void)
{
    printf("结果 = %d\n", calc_add(40, 2));
    return 0;
}
