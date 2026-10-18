#include <stdio.h>
#include "mathx.h"

int main(void)
{
    printf("[vendor_app] 用 vendored 源码编出来的 mathx %s\n", MATHX_VERSION);
    printf("[vendor_app] mx_gcd(1071,462) = %d\n", mx_gcd(1071, 462));
    return 0;
}
