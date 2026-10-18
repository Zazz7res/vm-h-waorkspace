#include <stdio.h>
#include <stdlib.h>
#include "pointer-basics.h"

/* 小工具：打印一行分隔线，让输出好读 */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

int main(void)
{
    /* ============================================================
       第 1 幕：值传递，交换失败
       ============================================================ */
    section("1. 值传递：函数改不到外面的变量");

    int x = 3, y = 5;
    printf("  调用前   main 里的 x=%d, y=%d\n", x, y);
    swap_by_value(x, y);
    printf("  调用后   main 里的 x=%d, y=%d   ← 完全没变，因为改的是拷贝\n", x, y);

    /* ============================================================
       2 幕：指针传递，交换成功
       ============================================================ */
    section("2. 指针传递：传地址就能改到外面的变量");

    printf("  x 的地址 = %p, y 的地址 = %p\n", (void *)&x, (void *)&y);
    printf("  调用前   main 里的 x=%d, y=%d\n", x, y);
    swap_by_pointer(&x, &y);   /* & 是"取地址"，把门牌号告诉函数 */
    printf("  调用后   main 里的 x=%d, y=%d   ← 真的换了\n", x, y);

    /* ============================================================
       3 幕：输出参数，一个函数带回两个结果
       ============================================================ */
    section("3. 输出参数：商用 return，余数用指针带出来");

    int remainder = -999;   /* 故意放个垃圾值，方便确认它真被写进去了 */
    int quotient = divide_with_remainder(17, 5, &remainder);
    printf("  17 ÷ 5  =>  商=%d（return 带回来的）, 余数=%d（写进我给的地址的）\n",
           quotient, remainder);

    /* 不关心余数？传 NULL，函数会跳过写入 */
    int q2 = divide_with_remainder(100, 7, NULL);
    printf("  100 ÷ 7 =>  只要商=%d，传 NULL 表示不接收余数\n", q2);

    divide_with_remainder(10, 0, &remainder);   /* 演示除数为 0 的保护 */

    /* ============================================================
       4 幕：数组和指针是一对双胞胎
       ============================================================ */
    section("4. 数组名 == 指向首元素的指针");

    int data[] = {1, 2, 3, 4, 5};
    int n = (int)(sizeof(data) / sizeof(data[0]));   /* 在数组"出生的地方"才能这么算长度 */
    printf("  data 有 %d 个元素\n", n);
    printf("  用 arr[i]   读: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", data[i]);
    }
    printf("\n  用 *(ptr+i) 读: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", *(data + i));
    }
    printf("\n");
    printf("  两种写法一模一样，sum_array 求和 = %d\n", sum_array(data, n));

    /* 注意：这个 sizeof 陷阱只会在函数参数上出现 */
    printf("\n  ⚠ 陷阱：在函数里 sizeof(数组参数) 得到的是指针大小，不是数组大小。\n");
    printf("    所以传数组必须连长度一起传，这就是 sum_array 要第二个参数的原因。\n");

    /* ============================================================
       5 幕：堆上数组 —— 申请了就必须还
       ============================================================ */
    section("5. malloc / free：堆内存由我们自己管");

    int *heap_arr = make_array(6, 42);
    if (heap_arr != NULL) {
        printf("  make_array(6, 42) 造出来: ");
        for (int i = 0; i < 6; i++) {
            printf("%d ", heap_arr[i]);
        }
        printf("\n  它在堆上的入口地址 = %p\n", (void *)heap_arr);
        free_array(heap_arr);        /* 还回去 */
        heap_arr = NULL;             /* 关键一步：立刻置空，避免野指针 */
        printf("  free 之后已把指针置 NULL：再误用它，程序会当场崩溃而不是悄悄算错\n");
    }

    /* ============================================================
       收尾
       ============================================================ */
    printf("\n=== 本节结束 ===\n");
    printf("记住三句话：\n");
    printf("  1) C 语言传参永远是拷贝，想改外面就得传地址。\n");
    printf("  2) a 是地址，*a 是地址里的东西，&a 是取地址。\n");
    printf("  3) malloc 回来的内存，free 之前一直占着，谁申请谁负责。\n");
    return 0;
}
