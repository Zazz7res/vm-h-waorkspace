#include <stdio.h>

/* GNU 的 constructor 属性：程序进 main 之前自动跑这个函数。
   把它当成"这份文件到底有没有参与编译"的探针 ——
   参与了，屏幕上就多一行；没参与，什么都不发生，而且**一句错都不报**。
   （第 20 节还同时看 link.txt 里有没有它的 .o，两条证据互相对质。） */
__attribute__((constructor)) static void announce(void)
{
    printf("[ctor.c 参与了]\n");
}
