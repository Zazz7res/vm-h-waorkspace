#include <stdio.h>
#include "calc.h"      /* 双引号：先搜 src/（没有这份），再按 -I 顺序找 → 顺序决定谁是赢家 */
#include "local.h"     /* 双引号：src/ 里就有，第一步直接命中，两个 -I 完全插不上手 */
#include <local.h>     /* 尖括号：跳过"包含者所在目录"，只能从 -I 里找 → 拿到 include/ 那份 */

/* 忙等一小段，好让 -O0 和 -O2 生成的代码量差得肉眼可见（第 02 节拿 size/nm 取证） */
static int burn(int n)
{
    int acc = 0;
    for (int i = 0; i < n; i++) {
        acc ^= (i * 2654435761u);
        acc = (acc << 5) | (acc >> 27);       /* 一个旋转，防止被优化成一个常数 */
    }
    return acc;
}

int main(void)
{
    printf("[app] calc.h 的赢家 = %s\n", CALC_WHICH);
    printf("[app] CALC_FLAVOR   = %s\n", CALC_FLAVOR);
    printf("[app] %s\n", LOCAL_QUOTED);
    printf("[app] %s\n", LOCAL_BRACKET);
    printf("[app] calc_add(2,3)=%d calc_mul(4,5)=%d burn=%d\n",
           calc_add(2, 3), calc_mul(4, 5), burn(2000));
    return 0;
}
