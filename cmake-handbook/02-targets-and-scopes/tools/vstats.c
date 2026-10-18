/* tools/vstats.c —— 消费者 2：同时需要 vector 和 util 的“使用要求”
 *
 * 它有两处依赖，故意做成两种不同的失败方式，这样实验结果更有说服力：
 *   ① #include "util.h"  —— 拿不到 -I 就直接**编译失败**（硬失败，报错很明显）
 *   ② UTIL_TAG 这个宏    —— 拿不到 -D 也能编过，但程序行为会**静默变化**（软失败，最难查）
 */
#include <stdio.h>
#include "vector.h"
#include "util.h"

int main(void)
{
    Vec v;
    vec_init(&v);

    const int samples[] = { 7, 3, 11, 2, 9, 4 };
    long sum = 0;
    int max = 0;
    for (size_t i = 0; i < sizeof samples / sizeof *samples; i++) {
        if (vec_push(&v, samples[i]) != 0) {
            printf("分配失败\n");
            vec_free(&v);
            return 1;
        }
        sum += samples[i];
        max = u_clamp(samples[i], 0, 10) > max ? u_clamp(samples[i], 0, 10) : max;
    }

    printf("元素个数 = %zu，总和 = %ld，夹到 [0,10] 之后的最大值 = %d\n",
           vec_len(&v), sum, max);

#ifdef UTIL_TAG
    printf("库标签（来自 util 的编译期定义）：%s\n", UTIL_TAG);
#else
    printf("库标签（来自 util 的编译期定义）：<没拿到 UTIL_TAG>\n");
#endif

    vec_free(&v);
    return 0;
}
