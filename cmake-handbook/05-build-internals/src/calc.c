#include "calc.h"

/* 两个函数都故意写得能被 -O2 完全内联掉，第 2 节拿 nm 看符号消失没消失 */
int calc_add(int a, int b) { return a + b; }
int calc_mul(int a, int b) { return a * b; }
