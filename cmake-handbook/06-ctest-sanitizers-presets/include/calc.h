/* calc.h —— 本章唯一的"真功能"，为的是让 ok / wrong 两个程序有东西可算。
   第 05 章用同名头演过 -I 的顺序竞争；这里只借用一个加法函数。 */
#ifndef CH06_CALC_H
#define CH06_CALC_H

#define CH06_FLAVOR "本章的 calc.h（include/ 里那一份）"

int calc_add(int a, int b);
int calc_mul(int a, int b);

#endif
