/* main.c —— 演示程序本体。它只依赖 greet.h，不知道 greet.c 的存在（链接期才见面） */
#include <stdio.h>
#include "greet.h"

int main(void)
{
    /* 容量够：返回值 = 实际写入的字节数（不含 '\0'） */
    char big[32];
    int used = greet_build(big, sizeof(big), "CMake 学习者");
    printf("正常写入：%s\n", big);
    printf("snprintf 返回 %d，缓冲区容量 %u → %s\n", used, (unsigned)sizeof(big),
           (used < (int)sizeof(big)) ? "没截断" : "**被截断了**");

    /* 容量不够：故意只给 4 字节。
     * 为什么是 4？"你" 在 UTF-8 里正好占 3 字节，再加结尾 '\0' 是 4 ——
     * 于是截断点恰好落在字符边界上，终端能干净地显示出一个 "你"。
     * 如果给 5 或 6，截断点会切进 "好" 的中间，屏幕上就是一个乱码方块 ——
     * 这也是中文程序常见的坑：按字节截断不看字符边界。 */
    char tiny[4];
    int used2 = greet_build(tiny, sizeof(tiny), "CMake 学习者");
    printf("\n小缓冲区：\"%s\"\n", tiny);
    printf("snprintf 返回 %d，缓冲区容量 %u → %s\n", used2, (unsigned)sizeof(tiny),
           (used2 < (int)sizeof(tiny)) ? "没截断" : "**被截断了**（返回值告诉你本来需要几字节）");

    return 0;
}
