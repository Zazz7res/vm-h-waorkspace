#include <stdio.h>
#include <stdlib.h>

/* leak —— 只泄漏，不崩。
   malloc 了 10 个 int（正好 40 字节），用完不 free，然后正常退 0。

   它是本章最重要的一枚探针：
   * 不开 ASan：ctest 给它绿灯，因为它退 0 —— "绿"不等于"没病"。
   * 开 ASan：LeakSanitizer 在 Linux 上默认就开着，程序退出时体检，
     报 detected memory leaks 并把退出码改成 1，于是同一条用例变红。
   同一段代码、同一条用例、同一个 ctest —— 唯一的变量是编译标志。 */
int main(void)
{
    int *buf = malloc(10 * sizeof(int));        /* 10 × 4 = 40 字节 */
    if (buf == NULL) {
        return 1;                               /* 分配失败不是本章要演的，退开 */
    }
    for (int i = 0; i < 10; i++) {
        buf[i] = i * i;
    }
    printf("leak: 前 10 个平方和 = %d\n", 0 + 1 + 4 + 9 + 16 + 25 + 36 + 49 + 64 + 81);
    /* ★ 这里故意没有 free(buf) —— 漏的就是它 */
    return 0;
}
