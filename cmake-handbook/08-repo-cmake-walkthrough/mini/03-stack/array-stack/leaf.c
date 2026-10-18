#include <stdio.h>
#include <stdlib.h>
#include "astk.h"

/* 第 03 节：数组栈。push 里 realloc，收尾 free —— 这一节该是干净的。
   它和 02 那节的差别就是"有没有对应的释放"，
   而两节在不开 ASan 的时候看起来一模一样（都退 0、都零警告）。 */
int main(void)
{
    int *data = NULL;
    int n = 0;
    int cap = 0;
    int top = 0;

    for (int i = 0; i < 5; i++) {
        if (n == cap) {                                  /* 满了就扩容 */
            cap = (cap == 0) ? astk_init_cap() : cap * 2;
            data = realloc(data, (size_t)cap * sizeof(int));
            if (data == NULL) {
                return 1;
            }
        }
        data[n++] = i * i;                               /* push 0,1,4,9,16 */
    }

    top = data[--n];                                     /* pop 最后一个：16 */
    free(data);                                          /* ★ 唯一的 free，配得上那次 realloc */

    printf("array-stack top=%d\n", top);
    return 0;
}
