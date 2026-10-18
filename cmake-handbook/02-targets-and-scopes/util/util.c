/* util/util.c —— 编进静态库 util */
#include "util.h"

int u_clamp(int x, int lo, int hi)
{
    /* 调用者把区间写反了（lo > hi）时不猜意图，直接返回 lo —— 教学代码也要有明确行为 */
    if (lo > hi) {
        return lo;
    }
    if (x < lo) {
        return lo;
    }
    if (x > hi) {
        return hi;
    }
    return x;
}
