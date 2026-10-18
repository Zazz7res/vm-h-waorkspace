/*
 * 第 02 章的对照组：C 语言里「变量是盒子」到底是什么体验。
 *
 * 编译运行（demo.sh 第 1 节里就是这条命令）：
 *     gcc -O0 -Wall -o a_c_box a_c_box.c && ./a_c_box
 *
 * 看点有三个：
 *   1) int b = a; 是**把盒子里的值倒一份**到新盒子，之后 a 怎么改都跟 b 无关；
 *   2) 盒子的大小在编译期就钉死了（sizeof(int)），装不下就溢出；
 *   3) 想让两个名字指向同一块内存，C 得靠指针——也就是说 C 默认没有「标签」，
 *      你得自己造一个标签出来。Python 反过来：默认全是标签，想要盒子得自己拷贝。
 */
#include <stdio.h>
#include <limits.h>

int main(void)
{
    int a = 1;
    int b = a;          /* 倒一份值：b 是一个独立的盒子 */
    a = 2;              /* 改 a 这个盒子，b 里的东西纹丝不动 */
    printf("一、赋值 = 倒盒子：a = %d, b = %d\n", a, b);

    int *p = &a;        /* p 才是 C 里的「标签」：它指向 a 那个盒子 */
    *p = 99;
    printf("二、通过指针改：a = %d（p 和 a 是同一块内存，不是两份值）\n", a);

    printf("三、盒子大小是编译期钉死的：sizeof(int) = %zu 字节, INT_MAX = %d\n",
           sizeof(int), INT_MAX);

    /* 溢出：这是 C 的日常，在 Python 里根本不存在（本章第 15 节实测 2**200）。
       有符号整数溢出在 C 标准里是未定义行为，这里只演示「不会自动变大」这件事。 */
    int big = INT_MAX;
    int overflowed = big + 1;
    printf("四、INT_MAX + 1 = %d（它没有长成一个更大的整数，而是绕回去了）\n", overflowed);

    return 0;
}
