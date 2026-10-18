#include <stdio.h>
#include <stdlib.h>

/* oob —— 越界写，但写在堆上，所以十有八九不会当场崩。
   合法下标是 0..7，这里写 p[8]，即块尾之后 4 个字节。

   为什么用 volatile int idx：
   下标写成常量 8，gcc 有可能在 -Wall 下直接警告（甚至优化）这段越界；
   绕一圈 volatile，编译器就"看不见"它是常量，警告一个都不出，
   于是这个 bug 在普通构建里是**完全安静**的 —— 退 0、无警告、无崩溃。
   这正是本章要演的：安静 ≠ 正确。开了 ASan 它才会叫。 */
int main(void)
{
    int *buf = malloc(8 * sizeof(int));         /* 32 字节 */
    if (buf == NULL) {
        return 1;
    }
    volatile int idx = 8;                       /* 运行时才知道的下标，骗过编译器 */
    buf[idx] = 42;                              /* ★ 越界：写到第 33~36 个字节 */
    printf("oob: 写完就走，看着一切正常（下标 %d）\n", idx);
    free(buf);
    return 0;
}
