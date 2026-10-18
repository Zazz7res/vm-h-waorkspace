#ifndef POINTER_BASICS_H
#define POINTER_BASICS_H

/*
 * 这是一个"头文件"。它只负责一件事：把函数长相（声明）告诉别人。
 * 真正的代码写在 pointer-basics.c 里。
 *
 * 为什么要分开？
 *   - 别的文件只要 #include "pointer-basics.h" 就能调用这些函数，
 *     不需要知道内部怎么实现的。
 *   - 编译器先看 .h 确认"参数个数、类型对不对"，链接时再去找 .c 里的实现。
 *
 * 外面那两行 #ifndef/#define 是"重复包含保护"：
 *   同一个头文件被 include 两次会报错，加上它，第二次就会被整段跳过。
 */

/* ---------- 1. 值传递 vs 指针传递：C 语言最重要的一课 ---------- */
void swap_by_value(int a, int b);      /* 交换失败，改的只是拷贝 */
void swap_by_pointer(int *a, int *b);  /* 交换成功，真的改到调用者的变量 */

/* ---------- 2. 指针当"输出参数"：一个函数想返回两个结果 ---------- */
int divide_with_remainder(int dividend, int divisor, int *remainder_out);

/* ---------- 3. 指针 + 数组：两种写法其实是同一件事 ---------- */
int sum_array(const int *arr, int n);

/* ---------- 4. 动态内存：在堆上造数组，用完必须还 ---------- */
int *make_array(int n, int fill_value); /* 调用者负责 free */
void  free_array(int *arr);

#endif /* POINTER_BASICS_H */
