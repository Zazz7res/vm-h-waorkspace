/* lib/vector.c —— 编进静态库 vector。注意第 4 行：它依赖 util.h */
#include <stdio.h>
#include <stdlib.h>
#include "vector.h"
#include "util.h"      /* ← 整个本章的胜负手：这一行能不能成立，取决于 CMake 有没有给到 -I */

void vec_init(Vec *v)
{
    v->data = NULL;
    v->len = 0;
    v->cap = 0;
}

/*
 * 尾插。容量策略：0 → 4，之后每次翻倍，但用 u_clamp 卡住上限 U_MAX_GROWTH。
 * realloc 失败时保持原状并返回 -1（绝不把 v->data 变成 NULL —— 那是最常见的内存 bug 源头）。
 */
int vec_push(Vec *v, int x)
{
    if (v->len == v->cap) {
        size_t want = (v->cap == 0) ? 4 : v->cap * 2;
        /* u_clamp 收 int，这里容量本来就很小，转回来是安全的 */
        size_t cap_new = (size_t)u_clamp((int)want, 4, U_MAX_GROWTH);
        int *grown = realloc(v->data, cap_new * sizeof *grown);
        if (grown == NULL) {
            return -1;
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
    v->data = NULL;      /* free 之后立刻置 NULL：这是防止二次 free / 悬空读最便宜的办法 */
    v->len = 0;
    v->cap = 0;
}
