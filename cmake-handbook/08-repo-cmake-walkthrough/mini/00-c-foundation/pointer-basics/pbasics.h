#ifndef PBASICS_H
#define PBASICS_H

/* 指针地基这一节：只练"指针怎么读改写"，一个 malloc 都不碰。 */

/* 把"翻倍"这件事写成一份只给本 leaf 用的头。
   static inline：谁 include 谁在自己这一份可执行文件里得到一份，
   不需要额外的 .c，也就不需要跨目录链接别人。 */
static inline int twice(int x)
{
    return 2 * x;
}

#endif /* PBASICS_H */
