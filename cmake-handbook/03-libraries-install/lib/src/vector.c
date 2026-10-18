/* vector.c —— 库的实现。注意它 include 了两个外界看不见的东西：
 *   "vector_internal.h"  → 来自 lib/src/，只有 target 自己拿到 -I（PRIVATE）
 *   "util.h"             → 来自 util 这个内部库，能不能拿到 -I 取决于 VECTOR_DEP_SCOPE */
#include <stdlib.h>

#include "vector.h"
#include "vector_internal.h"
#include "util.h"

int vec_init(Vec *v)
{
    if (v == NULL) {
        return -1;
    }
    v->data = NULL;
    v->len = 0;
    v->cap = 0;
    return 0;
}

int vec_push(Vec *v, int x)
{
    if (v == NULL) {
        return -1;
    }
    if (v->len == v->cap) {
        size_t want = (v->cap == 0) ? (size_t)VEC_DEFAULT_CAP : v->cap * 2;
        /* 内部工具函数：把新容量夹进 [VEC_DEFAULT_CAP, U_MAX_GROWTH] */
        size_t cap_new = (size_t)u_clamp((int)want, VEC_DEFAULT_CAP, U_MAX_GROWTH);
        int *grown = realloc(v->data, cap_new * sizeof *grown);
        if (grown == NULL) {
            return -1;              /* 失败时保持原状：绝不把 data 变成 NULL */
        }
        v->data = grown;
        v->cap = cap_new;
    }
    v->data[v->len++] = x;
    return 0;
}

int vec_get(const Vec *v, size_t i)
{
    if (v == NULL || i >= v->len) {
        return -1;
    }
    return v->data[i];
}

long vec_sum(const Vec *v)
{
    if (v == NULL) {
        return 0;
    }
    long s = 0;
    for (size_t i = 0; i < v->len; i++) {
        s += v->data[i];
    }
    return s;
}

size_t vec_len(const Vec *v)
{
    return (v == NULL) ? 0 : v->len;
}

void vec_free(Vec *v)
{
    if (v == NULL) {
        return;
    }
    free(v->data);
    v->data = NULL;
    v->len = 0;
    v->cap = 0;
}
