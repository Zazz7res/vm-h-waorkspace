/* app/main.c —— 消费者 1：只用 vector，不碰 util（它只需要 -Ilib） */
#include <stdio.h>
#include "vector.h"

int main(void)
{
    Vec v;
    vec_init(&v);

    for (int i = 1; i <= 10; i++) {
        if (vec_push(&v, i * i) != 0) {
            printf("分配失败，已存 %zu 个\n", vec_len(&v));
            vec_free(&v);
            return 1;
        }
    }

    printf("共 %zu 个元素：", vec_len(&v));
    for (size_t i = 0; i < vec_len(&v); i++) {
        printf("%d ", vec_get(&v, i));
    }
    printf("\n越界取值 vec_get(&v, 99) = %d\n", vec_get(&v, 99));

    vec_free(&v);
    return 0;
}
