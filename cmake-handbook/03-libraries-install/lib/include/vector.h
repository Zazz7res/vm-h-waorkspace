#ifndef VECTOR_H
#define VECTOR_H

/*
 * vector.h —— 这是**对外公开**的头文件，install 之后它落在 <前缀>/include/vector.h，
 * 也是消费者唯一能看见的头。
 *
 * 所以这里写的每一行都算「契约」：放进来一个东西，就等于承诺以后不随便删。
 * 第 02 章讲的 PUBLIC 就是这个意思 —— 传染给消费者的，必须是消费者看得懂的东西。
 * 反面例子：下面这个 struct 的三个字段一旦发布，「换实现」就不再是自由了
 * （想改成 C++ 风格、想把 cap 换成 size_t 之外的类型，都会让已经编译好的消费者行为错乱）。
 */

#include <stddef.h>

typedef struct {
    int    *data;
    size_t  len;
    size_t  cap;
} Vec;

/* 初始化一个空表。返回 0 表示成功，-1 表示参数非法（这里不会分配内存，所以几乎不会失败） */
int  vec_init(Vec *v);

/* 尾插一个整数。0 = 成功，-1 = 内存分配失败（失败时原内容保持不变） */
int  vec_push(Vec *v, int x);

/* 按下标取值；越界返回 -1（本表的元素约定为非负，所以 -1 可以当“取不到”用） */
int  vec_get(const Vec *v, size_t i);

/* 所有元素之和（溢出不管，教学用；真实库应该用 long 或返回错误码） */
long vec_sum(const Vec *v);

size_t vec_len(const Vec *v);

/* 释放内存并把结构体清回空表状态，可以安全地重复调用 */
void vec_free(Vec *v);

#endif /* VECTOR_H */
