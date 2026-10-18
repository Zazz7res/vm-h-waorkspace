#ifndef ASTK_H
#define ASTK_H

/* 数组栈这一节：栈就是"一个数组 + 一个栈顶下标"。 */

/* 该给栈开多大：初始容量。真正的增长策略写在 leaf.c 里。 */
static inline int astk_init_cap(void)
{
    return 4;
}

#endif /* ASTK_H */
