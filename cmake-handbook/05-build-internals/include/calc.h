/* calc.h —— 「正牌」那份头。
   第 10 节它会和 alt/include/calc.h 抢同一个名字：两份都定义了 CALC_FLAVOR，
   值不一样，于是程序一跑就知道 -I 的顺序让谁赢了。 */
#ifndef CALC_H
#define CALC_H
#define CALC_FLAVOR "正牌（include/ 里那份）"
#define CALC_WHICH  "include/calc.h"
int calc_add(int a, int b);
int calc_mul(int a, int b);
#endif
