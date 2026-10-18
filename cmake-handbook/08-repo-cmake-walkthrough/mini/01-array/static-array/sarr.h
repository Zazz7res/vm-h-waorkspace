#ifndef SARR_H
#define SARR_H

/* 静态数组这一节：长度写死在编译期，容量永远不会变。 */

/* 数组求和：参数是"数组首地址 + 元素个数"，这是 C 里传数组的唯一诚实写法。 */
static inline int arr_sum(const int *a, int n)
{
    int s = 0;
    for (int i = 0; i < n; i++) {
        s += a[i];
    }
    return s;
}

#endif /* SARR_H */
