/* consumer/main.c —— **另一个独立工程**：它不在 add_subdirectory 树里，
 * 只能通过 find_package(Vector) 拿到导入目标 Vector::vector，再链接它。
 *
 * 它模拟的就是「三个月后，别人在别的目录里用你的库」。
 * 这时候没有任何 -I 能靠「同一棵树」蒙过去 —— 装漏了什么，这里当场暴露。
 */
#include <stdio.h>

#include "vector.h"          /* 公开头：安装树里有，永远编得过 */

#ifdef CONSUMER_PROBES_INTERNAL
/* 库自己的内部头：从来没被 install，安装树里根本没有这个文件。
 * 打开它就必然 fatal error —— 这正是「公开/内部」用目录做物理分界的价值。 */
#include "vector_internal.h"
#endif

#ifdef CONSUMER_PROBES_UTIL_H
/* 库的**内部依赖**的头：它装了（在 include/vector/ 下面），
 * 但只有当 vector 用 PUBLIC 链接 util 时，那个 -I 才会传染到消费者身上。
 * 于是同一个开关值，在两种作用域下给出两种结果 —— 这一行就是 VECTOR_DEP_SCOPE 的照妖镜。 */
#include "util.h"
#endif

int main(void)
{
    Vec v;
    vec_init(&v);
    for (int i = 0; i < 5; i++) {
        if (vec_push(&v, (i + 1) * (i + 1)) != 0) {
            printf("[consumer] 插入失败\n");
            vec_free(&v);
            return 1;
        }
    }
    printf("[consumer] 元素数 = %zu，总和 = %ld\n", vec_len(&v), vec_sum(&v));

#ifdef CONSUMER_PROBES_INTERNAL
    printf("[consumer] 偷看到了内部宏 VEC_DEFAULT_CAP = %d\n", VEC_DEFAULT_CAP);
#endif

#ifdef CONSUMER_PROBES_UTIL_H
    printf("[consumer] 用到了内部依赖 u_clamp(999,0,10) = %d\n", u_clamp(999, 0, 10));
#endif

    vec_free(&v);
    return 0;
}
