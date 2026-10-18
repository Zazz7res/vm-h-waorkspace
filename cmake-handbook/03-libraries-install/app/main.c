/* app/main.c —— 同一棵树里的消费者：它只 include 公开头 vector.h，
 * 所以 build 期靠 vector 的 PUBLIC 头目录就能编过。 */
#include <stdio.h>

#include "vector.h"

int main(void)
{
    Vec v;
    if (vec_init(&v) != 0) {
        printf("初始化失败\n");
        return 1;
    }

    for (int i = 1; i <= 6; i++) {
        if (vec_push(&v, i * 3) != 0) {
            printf("扩容失败，已存 %zu 个\n", vec_len(&v));
            vec_free(&v);
            return 1;
        }
    }

    printf("表里有 %zu 个元素：", vec_len(&v));
    for (size_t i = 0; i < vec_len(&v); i++) {
        printf("%d ", vec_get(&v, i));
    }
    printf("\n总和 = %ld\n", vec_sum(&v));

    vec_free(&v);
    return 0;
}
