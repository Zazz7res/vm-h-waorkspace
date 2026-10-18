#include <stdio.h>
#include "pbasics.h"

/* 第 00 节：指针地基。
   全部在栈上，一个 malloc 都没有 —— 所以挂了 ASan 它也永远不该被抓。
   这一节在章工程里的角色是"对照组"：证明绿灯不是 ASan 没装。 */
int main(void)
{
    int values[4] = {1, 2, 3, 4};
    int total = 0;

    for (int i = 0; i < 4; i++) {
        total += twice(values[i]);      /* 2+4+6+8 = 20 */
    }

    printf("pointer-basics total=%d\n", total);
    return 0;
}
