/* lib/vector.h —— 对外只暴露这 5 个函数（和第 10 章动态数组一节同构，故意写小） */
#ifndef VECTOR_H
#define VECTOR_H

#include <stddef.h>   /* size_t：签名里用了它就得自己 include，不靠传递包含 */

typedef struct {
    int *data;
    size_t len;
    size_t cap;
} Vec;

void   vec_init(Vec *v);
int    vec_push(Vec *v, int x);      /* 0 = 成功，-1 = 分配失败 */
int    vec_get(const Vec *v, size_t i);   /* 越界返回 -1（元素都是正数，够用） */
size_t vec_len(const Vec *v);
void   vec_free(Vec *v);

#endif /* VECTOR_H */
