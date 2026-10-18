#include <stdio.h>
#include <stdlib.h>
#include "slist-num.h"

/* 第 02 节：单链表 —— **故意只 malloc 不 free**。
   建 slist_count() 个节点头，一个都没拆：
     · 退出码 0
     · 输出正常
     · -Wall -Wextra -Wshadow 零警告
   只有 LSan 看得见。第 15~17 节的 ASan 实验全靠它。
   3 次 malloc × sizeof(int)=4 字节 = 漏 12 字节。 */
int main(void)
{
    int total = 0;

    for (int i = 0; i < slist_count(); i++) {
        int *node = malloc(sizeof(int));        /* ★ 故意没有对应的 free */
        if (node == NULL) {
            return 1;
        }
        *node = i + 1;
        total += *node;
    }

    printf("slist total=%d\n", total);           /* 1+2+3 = 6 */
    return 0;
}
