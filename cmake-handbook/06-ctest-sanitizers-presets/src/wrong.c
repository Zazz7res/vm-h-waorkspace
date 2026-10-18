#include <stdio.h>
#include "calc.h"

/* wrong —— 一个"逻辑错但程序不崩"的经典样本。
   它打印 结果 = 43 然后**正常退出 0**。

   为什么要它：ctest 的默认裁判只看退出码。这个程序退出码是 0，
   所以默认裁判会给它绿灯。本章第 5 节给它加一条 PASS_REGULAR_EXPRESSION
   （要求输出里必须有「结果 = 42」），它才当场变红 ——
   这就是"退出码之外的第二把尺子"存在的理由。 */
int main(void)
{
    printf("结果 = %d\n", calc_add(40, 3));    /* 手滑写成 3，正确答案是 2 */
    return 0;                                   /* ★ 退出码依然是 0 */
}
