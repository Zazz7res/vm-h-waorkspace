#include "mathx.h"

int mx_clamp(int v, int lo, int hi)
{
    if (lo > hi) { return lo; }          /* 参数写反时别死循环，退回下界 */
    if (v < lo)  { return lo; }
    if (v > hi)  { return hi; }
    return v;
}

int mx_gcd(int a, int b)
{
    if (a < 0) { a = -a; }
    if (b < 0) { b = -b; }
    while (b != 0) { int t = a % b; a = b; b = t; }
    return a;
}

int mx_hash(const int *arr, int n)
{
    unsigned h = 2166136261u;            /* FNV-1a 的初始值，抄来的常数，够用 */
    for (int i = 0; i < n; i++) {
        h = (h ^ (unsigned)arr[i]) * 16777619u;
    }
    return (int)(h & 0x7fffffffu);       /* 保证返回非负，打印好看 */
}
