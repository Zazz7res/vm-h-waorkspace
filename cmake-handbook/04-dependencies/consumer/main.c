#include <stdio.h>
#include "mathx.h"

int main(void)
{
    int a[5] = {3, 1, 4, 1, 5};
    printf("[consumer] mathx %s 在场\n", MATHX_VERSION);
    printf("[consumer] mx_clamp(999,0,10) = %d\n", mx_clamp(999, 0, 10));
    printf("[consumer] mx_gcd(48,180)     = %d\n", mx_gcd(48, 180));
    printf("[consumer] mx_hash(a,5)       = %d\n", mx_hash(a, 5));
    return 0;
}
