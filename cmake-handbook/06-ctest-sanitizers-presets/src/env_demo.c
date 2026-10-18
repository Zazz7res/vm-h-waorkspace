#include <stdio.h>
#include <stdlib.h>

/* env_demo —— 把环境变量打印出来就退 0。
   它是 ENVIRONMENT 测试属性的显影剂：
   * 不加属性跑它     → 打印 (未设置)
   * 加 ENVIRONMENT "CH06_TAG=..." 再跑 → 打印那个值
   两条用例、同一个程序，屏幕上就能看出属性确实注进了测试进程的环境。 */
int main(void)
{
    const char *tag = getenv("CH06_TAG");
    printf("env_demo: CH06_TAG = %s\n", tag != NULL ? tag : "(未设置)");
    return 0;
}
