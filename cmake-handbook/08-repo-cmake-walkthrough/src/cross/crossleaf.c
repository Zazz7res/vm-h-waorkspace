#include <stdio.h>
#include "slist-num.h"          /* ★ 这份头不在本 leaf 目录里，它在 02-linked-list/slist/ */

/* 跨目录 include 的探针：抄邻居的头。
   因为 slist-num.h 里是 static inline，这里连"链接不上邻居"都不会发生 ——
   能编译、能链接、能跑，约定的破坏完全无声。
   第 12 节用它证明 target_include_directories(... PRIVATE 本 leaf 目录) 那一行的价值。 */
int main(void)
{
    printf("crossleaf borrowed=%d\n", slist_count());
    return 0;
}
