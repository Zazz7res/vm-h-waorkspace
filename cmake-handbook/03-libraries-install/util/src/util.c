/* util.c —— 内部小库。它永远编成静态库，并打开 POSITION_INDEPENDENT_CODE，
 * 这样即便 BUILD_SHARED_LIBS=ON、vector 变成 .so，也能把它吸收进去。 */
#include "util.h"

int u_clamp(int x, int lo, int hi)
{
    if (lo > hi) {
        return lo;
    }
    return (x < lo) ? lo : ((x > hi) ? hi : x);
}
