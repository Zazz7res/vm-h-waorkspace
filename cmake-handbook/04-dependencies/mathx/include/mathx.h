/* mathx.h —— 假装这是"别人家的库"的公开头文件。
   第 04 章整章都在折腾这一份代码：它可以用四种完全不同的方式到货。 */
#ifndef MATHX_H
#define MATHX_H

/* 版本号写在头里，是让"模块模式的 Find 脚本"能把它 grep 出来用的。
   真实的库（zlib.h 里的 ZLIB_VERSION）就是这么干的，见 Modules/FindZLIB.cmake。 */
#define MATHX_VERSION "1.3.0"

/* 把 v 夹到 [lo,hi] 里 */
int mx_clamp(int v, int lo, int hi);

/* 欧几里得求最大公约数 */
int mx_gcd(int a, int b);

/* 一个很土的整数哈希：只演示"库里有函数"这件事 */
int mx_hash(const int *arr, int n);

#endif
