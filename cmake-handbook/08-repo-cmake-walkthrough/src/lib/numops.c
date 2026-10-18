#include <stdio.h>

/* 第 22 节的道具：往工程里再加一个**静态库** target，
   然后看收尾那句"可执行 target 数量"会不会跟着涨。
   （BUILDSYSTEM_TARGETS 数的是"本目录里所有 buildsystem target"，
     库和可执行文件不分家，所以那句措辞在这里开始骗人。） */
int numops_report(void)
{
    printf("numops helper\n");
    return 0;
}
