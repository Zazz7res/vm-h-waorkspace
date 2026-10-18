#include <stdio.h>

/* ub —— 未定义行为：有符号整数左移出符号位。
   1 << 31 在 int 里放不进（int 是 32 位有符号，最大值 2^31-1）。

   为什么 shift 要用 volatile：
   写成 1 << 31 是常量表达式，gcc 在编译期就折叠掉了，UBSan 的**运行时**
   检查根本不会执行到这一行。绕成变量，才留到运行时被抓。

   这个样本专门用来演示本章第 16 节那个坑：
   UBSan 默认"报告后继续跑"（recover 模式），打印完 runtime error
   还是退 0 —— 于是 ctest 给它绿灯。要它变红必须加 -fno-sanitize-recover=all。 */
int main(void)
{
    volatile int shift = 31;                    /* 不让编译器常量折叠 */
    int result = 1 << shift;                    /* ★ 有符号溢出：UB */
    printf("ub: 1 << %d = %d（这行值不重要，重要的是有没有被抓到）\n", shift, result);
    return 0;
}
