#ifndef SLIST_NUM_H
#define SLIST_NUM_H

/* 单链表这一节的"该建几个节点头"。
   故意写成 static inline：别的目录只要 include 到这份头，
   连函数体都能拿到 —— 于是跨目录 include 可以一路绿灯到链接都不响。
   第 12、13 节就拿这份头当"邻居的东西"。 */
static inline int slist_count(void)
{
    return 3;
}

#endif /* SLIST_NUM_H */
