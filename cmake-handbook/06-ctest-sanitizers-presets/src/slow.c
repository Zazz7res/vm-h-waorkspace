#include <stdio.h>
#include <unistd.h>

/* slow —— 用来把 TIMEOUT 属性演成真的 ***Timeout。
   它睡 3 秒。用例上挂 TIMEOUT 1，于是 ctest 在第 1 秒就把它掐了。

   为什么用 sleep 而不是忙等：忙等在 -O2 下可能被优化没了，
   睡 3 秒跟优化级别无关，超时这条证据在任何构建类型下都稳定。 */
int main(void)
{
    printf("slow: 开始睡 3 秒（用例的 TIMEOUT 设成 1 秒，等不到我跑完）\n");
    fflush(stdout);
    sleep(3);
    printf("slow: 睡完了，但我大概已经被掐掉了\n");
    return 0;
}
