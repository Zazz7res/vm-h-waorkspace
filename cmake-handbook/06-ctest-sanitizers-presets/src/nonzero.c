#include <stdio.h>

/* nonzero —— 一个"故意失败"的程序，专门给 WILL_FAIL 用。
   WILL_FAIL TRUE 的意思是：这条用例**期望**它退非 0，退了非 0 才算绿。
   真实场景就是"已知缺陷/待实现"的用例：先让它挂在名单里当哨兵，
   等哪天它突然退 0 了（bug 修好了），ctest 会报 Failed，提醒你把它转正。 */
int main(void)
{
    printf("nonzero: 已知缺陷 —— 这个功能还没实现，所以按约定退 1\n");
    return 1;
}
