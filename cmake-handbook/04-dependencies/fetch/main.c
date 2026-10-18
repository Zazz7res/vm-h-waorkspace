#include <stdio.h>
#include "mathx.h"

int main(void)
{
    int a[4] = {7, 7, 7, 7};
    printf("[fetch_app] 用 FetchContent 现拉的 mathx %s\n", MATHX_VERSION);
    printf("[fetch_app] mx_clamp(-5,0,10) = %d，mx_hash(a,4) = %d\n",
           mx_clamp(-5, 0, 10), mx_hash(a, 4));
    return 0;
}
