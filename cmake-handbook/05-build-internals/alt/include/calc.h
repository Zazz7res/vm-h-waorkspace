/* calc.h —— 「山寨」那份：路径不同、宏值不同，别的都一样。
   存在的唯一目的：证明 #include "calc.h" 到底解析到哪一份，
   是可以被 -I 的**顺序**改变的（第 10 节把 ALT_FIRST 拨到 ON 看它赢）。 */
#ifndef CALC_H
#define CALC_H
#define CALC_FLAVOR "山寨（alt/include/ 里那份）"
#define CALC_WHICH  "alt/include/calc.h"
int calc_add(int a, int b);
int calc_mul(int a, int b);
#endif
