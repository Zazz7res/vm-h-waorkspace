#include <stdio.h>
#include <stdlib.h>
#include <limits.h>     /* CHAR_BIT：确认"一字节 8 位"这个前提在编译期成立 */
#include "bloom.h"

/*
 * ============================================================================
 * 0. 两个数学常数 + 一个手搓的 ln —— 为什么这节要自己算对数
 * ============================================================================
 * 布隆过滤器的全部设计都靠这两个常数：
 *   ln 2 = 0.6931471805599453094172321214581765680755001343602552...
 *          （就是交错调和级数 1 - 1/2 + 1/3 - 1/4 + ... 的和，欧拉-马斯刻若尼无关，
 *            值是经典常数，任何对数表/计算器都能核对）
 *   (ln 2)^2 = 0.480453013918201424667102526326664...（就是上面那个数自己乘自己，
 *              这里直接写死乘好的结果，少一次乘法也少一次出错机会）
 *
 * 为什么不用 <math.h> 里的 log()？
 *   本仓库的编译命令是固定的，形如：
 *       gcc -Wall -Wextra -Wshadow -std=c11 -g -fsanitize=address -static-libasan 本目录所有C文件 -o xxx
 *   里面【没有 -lm】。要点区别：
 *     · 浮点的 + - * / 是编译器直接生成指令的，不需要任何库 ⇒ 用 double 四则运算绝对安全；
 *     · log / exp / sqrt / pow / ceil / floor / round 这些【函数】是 libm 的符号，
 *       老配置上不加 -lm 就是 undefined reference ⇒ 链接失败。
 *   （新 glibc 把 libm 并进了 libc，很多机器上不加 -lm 也能过，但那是在赌环境，不能赌。）
 *   所以下面 bloom_ln 用"缩范围 + 级数"手搓，只用加减乘除；
 *   ceil/round 也一律不用库函数，见 bloom_params 里那两行整数化写法。
 */

#define BLOOM_LN2     0.69314718055994530942
#define BLOOM_LN2_SQ  0.48045301391820142467

/* 编译期钉死前提：本节所有 /8、%8、(m+7)/8 都建立在"一字节 8 位"上。
   万一哪天搬到 CHAR_BIT != 8 的平台上，这行会直接编译报错，而不是悄悄算错下标。 */
_Static_assert(CHAR_BIT == BLOOM_BITS_PER_BYTE,
               "本节位数组的 /8 %8 写法假设 CHAR_BIT == 8");

double bloom_ln(double x)
{
    /* 约定：非法输入返回 0.0，由调用方挡住（布隆这里 p 一定在 (0,1) 内，不会走到）。
       为什么不用 -inf / NaN：一引入特殊值，后面的判断就要跟着写一堆特例。 */
    if (!(x > 0.0)) {
        printf("      [警告] bloom_ln 收到非法参数 x=%f，按 0 返回\n", x);
        return 0.0;
    }

    /* 第 1 步：把 x 缩到 [0.5, 1) 这个窄区间里。
       做法是不断乘 0.5 或 2，并用 e 记账"缩了几次 2"：x = v * 2^e
       ⇒ ln(x) = ln(v) + e·ln2。
       为什么非缩不可：下一步的级数只在 v 靠近 1 时才收敛得快。
       ln(0.01) 直接展开发 200 项都不一定够，缩完只要十几项。 */
    int e = 0;
    double v = x;
    while (v >= 1.0) {
        v *= 0.5;
        e += 1;
    }
    while (v < 0.5) {
        v *= 2.0;
        e -= 1;
    }

    /* 第 2 步：级数。普通泰勒展开 ln(1+u) = u - u²/2 + u³/3 ... 在 u→-1 时发散得厉害，
       所以换成这个"对称版"：令 y = (v-1)/(v+1)，则
           ln(v) = 2 · ( y + y³/3 + y⁵/5 + y⁷/7 + ... )
       v ∈ [0.5,1) ⇒ y ∈ [-1/3, 0)，|y| ≤ 1/3 ⇒ 每一项至少缩 9 倍，收敛飞快。
       （这个式子怎么来的：ln(v) = ln((1+y)/(1-y)) = 2(y + y³/3 + …)，
         把 y=(v-1)/(v+1) 反代回去正好得到 v，是数值分析课本的标准配方。） */
    double y = (v - 1.0) / (v + 1.0);
    double y2 = y * y;
    double term = y;          /* 当前项的分子：y^(2i+1)，每次乘 y2 */
    double sum = 0.0;
    for (int i = 0; i < 60; i++) {
        double add = term / (double)(2 * i + 1);
        sum += add;
        term *= y2;
        if (add > -1e-18 && add < 1e-18) {
            break;            /* 加进去已经等于没加，收工 */
        }
    }
    return 2.0 * sum + (double)e * BLOOM_LN2;
}

/*
 * ============================================================================
 * 1. 位数组三件套 —— 本节最该看懂的三行代码
 * ============================================================================
 * 我们要把"第 idx 个开关"（idx 可以是 0、1、2 ... 几十万个）存进内存。
 * 最省的办法：一个 unsigned char 数组，一个字节有 8 位 ⇒ 能当 8 个开关用。
 * 于是"第 idx 个开关"这件事必须拆成两个问题：
 *
 *      ① 它住在第几个字节里？        ⇒ idx / 8      （整除 = 每 8 个换一行）
 *      ② 进去之后是这个字节的哪一位？⇒ idx % 8      （余数 = 行内第几个）
 *
 *   类比：一栋楼每层 8 个房间，问"1005 号房在哪"——
 *        1005 / 8 = 125 ⇒ 第 125 层；1005 % 8 = 5 ⇒ 那层的第 5 间。
 *   这个"先算第几行、再算行内第几列"的拆分，是【所有】位图/位压缩代码的母题：
 *   堆分配器的空闲位图、操作系统的 CPU 亲和性掩码、素数筛、邻接矩阵压缩，全是这三行。
 *
 *   图示（idx = 11）：
 *
 *      idx = 11   ⇒   11 / 8 = 1（第 1 个字节，从 0 数）   11 % 8 = 3（字节内第 3 位）
 *
 *      bits[0]      bits[1]      bits[2]
 *      76543210     76543210     76543210      ← 字节内的位号（低位在右）
 *      00000000     00000000     00000000        把 bits[1] 的第 3 位按亮：
 *                       ↑↑↑↑↑↑↑↑↑↑↑↑
 *                 bits[1] |= (1u << 3)   →   00001000
 *                       └── 这一位整体代表全局下标 11 = 1*8 + 3
 *
 *   ★ 为什么写 1u 而不是 1：
 *     1) 严格说，这里 (idx % 8) 最大是 7，`1 << 7` = 128 还在 int 里，不会当场出事。
 *        但位操作的通用形状是 `1u << bit`，bit 可以到 31、63 ——
 *        有符号 int 的移位【不允许把 1 移进符号位】：`1 << 31` 在 C 里是
 *        未定义行为（结果不是 int 能表示的值 ⇒ 有符号溢出 ⇒ UB，编译器有权"假设它不发生"
 *        并据此优化）。`1u << 31` 则是【完全定义】的：无符号运算对 2^32 取模，值 = 2147483648。
 *        所以习惯是：位掩码一律无符号字面量，u 后缀 = "我承诺这块内存按位看，别按数值看"。
 *     2) `unsigned char |= 有符号int` 会先做整型提升和符号扩展，结果没错但绕了远路；
 *        两边都是无符号类型，读代码的人不用去查提升规则。
 *     3) 位号到了 32 以上就必须 `1ULL << bit`（无符号 long long），写成 `1u << 40`
 *        依然是 UB（移位量 >= 类型位宽，标准直接定义为未行为）。见 bloom_derive_slots
 *        里用 1ULL 的那几行 —— 同一套纪律。
 *
 *   ★ 三个操作的口诀：置位用 | 、测位用 & 、清位用 &= ~ 。
 *     为什么清位要 "&= ~mask" 而不是 "^= mask"：异或【翻转】，本来已经是 0 的位会被
 *     它按成 1；与上取反才是"只把这一位按成 0，别的位置一根手指头都不碰"。
 */
void bits_set(unsigned char *bits, unsigned long long idx)
{
    bits[idx / BLOOM_BITS_PER_BYTE] |= (unsigned char)(1u << (idx % BLOOM_BITS_PER_BYTE));
    /* 拆开看：idx/8 选字节 → (idx%8) 选位 → 1u<<位 造出只有那一位是 1 的掩码 → |= 按亮
       外面套 (unsigned char) 是明确告诉编译器"我知道要写回一个字节"，
       同时消掉 -Wconversion 类工具会叽歪的"整型降格"。语义完全不变。 */
}

void bits_clear(unsigned char *bits, unsigned long long idx)
{
    bits[idx / BLOOM_BITS_PER_BYTE] &=
        (unsigned char)~(1u << (idx % BLOOM_BITS_PER_BYTE));
    /* ~ 把掩码整个翻过来：00001000 → 11110111，与一下就把第 3 位按灭、其余 7 位保留。
       ⚠ 这个函数在标准布隆过滤器里【不该被用来删键】，第 8 幕就是它作妖的现场。 */
}

int bits_test(const unsigned char *bits, unsigned long long idx)
{
    return (bits[idx / BLOOM_BITS_PER_BYTE] >> (idx % BLOOM_BITS_PER_BYTE)) & 1u;
    /* 两种等价写法：
         (bits[i] & mask) != 0     ← 只看"那位是不是 1"
         (bits[i] >> 位) & 1       ← 把那位挪到最低再掐掉其余，正好是 0/1，能直接 return
       这里选后者，是因为它返回值天然是 int 0/1，不用再加 !=0。
       注意 bits[i] 会被整型提升成 int，右移的是非负值 ⇒ 一定是逻辑右移，不会补符号位。 */
}

void bits_dump(const unsigned char *bits, unsigned long long n_bytes, const char *tag)
{
    printf("      %s\n", tag);
    printf("        字节内位号：      7 6 5 4 3 2 1 0   （低位在右，所以下标是从右往左数）\n");
    unsigned long long show = (n_bytes > 6u) ? 6u : n_bytes;
    for (unsigned long long b = 0; b < show; b++) {
        printf("        bits[%llu] = ", b);
        for (int bit = BLOOM_BITS_PER_BYTE - 1; bit >= 0; bit--) {
            /* 从高位到低位逐个问"你亮吗"，打印出来的形状就和手写二进制一样 */
            printf("%d", (bits[b] >> bit) & 1u);
            if (bit > 0) {
                printf(" ");
            }
        }
        printf("   (0x%02X)\n", (unsigned)bits[b]);
    }
    if (n_bytes > show) {
        printf("        …… 后面 %llu 个字节省略\n", n_bytes - show);
    }
}

/*
 * ============================================================================
 * 2. 哈希函数：把键揉成【一个】64 位无符号整数
 * ============================================================================
 * 布隆需要的不是"哈希到 0..m-1"，而是"任意长度的键 → 一个看起来乱七八糟的 64 位串"，
 * 因为下标是后面用 h1 + i*h2 现算的（见第 4 节）。
 */
unsigned long long bloom_hash_str(const char *key)
{
    if (key == NULL) {
        return 0ULL;      /* 本节把 NULL 当成"空串"处理；调用方若要区分，自己先判 */
    }
    /* FNV-1a 64 位：
         初值（offset basis） = 14695981039346656037
         每一字节： h = (h ^ 字节) * 1099511628211     素数叫 FNV prime
       为什么先异或再乘（1a）而不是先乘再异或（1）：先异或能让"最低几位"立刻参与乘法，
       雪崩更快，短字符串也不至于撞成一坨。
       为什么这两个常数看着莫名其妙：它们是作者搜出来的经验值（素数 + 一个高位有戏的数）。
       为什么必须 unsigned long long：长键一定会让这个乘法溢出，
       无符号溢出 = 对 2^64 取模，标准【明确定义】；有符号溢出 = UB。
       为什么遍历用 unsigned char 而不是 char：char 在 x86 上默认有符号，
       中文字节是负数，直接进表达式会被符号扩展成 0xFF..xx，污染高位。 */
    unsigned long long h = 14695981039346656037ULL;   /* offset basis：一个都不能少写一位。
                                                        曾经手滑写成 1469598103934665603（少个 7），
                                                        编译不报错、跑得欢、哈希照样分散 —— 这种
                                                        常数抄错的 bug 只能靠对拍发现，所以要核对。 */
    for (const unsigned char *p = (const unsigned char *)key; *p != '\0'; p++) {
        h ^= (unsigned long long)*p;
        h *= 1099511628211ULL;                        /* FNV prime = 2^40 + 1 + ... 的著名素数 */
    }
    return h;
}

unsigned long long bloom_hash_int(int key)
{
    /*
     * ★ 为什么要"搅散"（mix / finalizer），这是本节第二大坑：
     *   如果偷懒直接 h = (unsigned long long)key，会发生两件坏事：
     *
     *   坏事 A（致命）：双重哈希要 h2 = "h 的另一半"。int 只有 32 位，高 32 位全是 0
     *                  ⇒ h2 = 0 ⇒ slot_i = (h1 + i*0) % m 对每个 i 都一样
     *                  ⇒ 所谓 k 个哈希点【全重合】，一个键只占 1 位而不是 7 位。
     *                    第 5 幕会实测这个坏版本的误判率，比正常版高一个数量级。
     *   坏事 B（常见）：key 本身是连续整数 0,1,2,...，它们只差最低几位。
     *                  % m 之后虽然也分散，但"键是 2 的幂的倍数"这类数据（例如内存地址、
     *                  分页 id、自增主键 *1024）会整整齐齐撞在同一个下标段上。
     *                  乘法 + 右移异或能把低位搅到高位、高位再搅回低位，两个方向都混。
     *
     *   下面这三行是 MurmurHash3 / splitmix64 的收尾混合（finalizer），业界标准件：
     *     x ^= x >> 33;  x *= 大奇数常数;  x ^= x >> 33;  ...
     *   原理一句话：乘法把低位信息"顶"到高位去（因为乘法是所有位参与的卷积），
     *   右移异或再把高位信息搬回低位 —— 一来一回，每一位都影响到了每一位。
     *   常数为什么长这样：作者搜出来的"每一位都不弱"的奇数，不必深究，记住要用无符号。
     *   先把 int 转 unsigned int 再升 64 位：这样 -1 变成 4294967295 参与混合，
     *   而不是先做符号扩展变成 0xFFFFFFFFFFFFFFFF 让所有负数都长得像。
     */
    unsigned long long x = (unsigned long long)(unsigned int)key;
    x += 0x9E3779B97F4A7C15ULL;      /* 黄金比例常数：连 0 也会被推开，避免"全 0 输入 → 全 0 输出" */
    x ^= x >> 30;
    x *= 0xBF58476D1CE4E5B9ULL;
    x ^= x >> 27;
    x *= 0x94D049BB133111EBULL;
    x ^= x >> 31;
    return x;
}

/*
 * ============================================================================
 * 3. 造下标：从"一个 64 位哈希"变出 k 个位置（双重哈希）
 * ============================================================================
 * 【为什么不真的写 k 个哈希函数】
 *   教科书说"用 k 个独立哈希函数 h1..hk"。真写出来意味着：
 *     · 每个键要把字符串扫 k 遍（k=7 就是 7 次遍历、7 倍指令数）；
 *     · 还得保证这 7 个函数彼此"独立" —— 大多数号称独立的组合其实相关性很强。
 *   工程上从不用 k 个哈希函数，而是 Kirsch & Mitzenmacher（2006）这招：
 *
 *          slot_i = (h1 + i * h2) mod m ,   i = 0, 1, ... , k-1
 *
 *   只用两个哈希（甚至只用一个 64 位哈希拆成两半），就得到 k 个下标。
 *
 * 【为什么够用】
 *   1) 理论：误判率的分析只需要这些下标"两两接近独立"（pairwise independent）。
 *      一次线性组合 h1 + i*h2 恰好给出两两独立的族 —— 论文证明按这个方式构造的
 *      布隆过滤器，其误判率和"真 k 个独立哈希"的版本在渐近意义上一模一样。
 *   2) 实践：我们的 h1、h2 来自 64 位混合良好的哈希，把 64 位看成"64 个真随机位"，
 *      取一半当 h1、另一半搅一遍当 h2，相关性已经低到测不出来。
 *   3) 省：字符串只扫【一遍】，k 个下标是 7 次加法 + 7 次除法，缓存也更友好。
 *
 * 【为什么 h2 必须是奇数（还要非 0）】
 *   序列是 h1, h1+h2, h1+2h2, ... 若 m 是 2 的幂而 h2 是偶数，
 *   这一串 mod m 只会落在【一半】的位上（低 log2(gcd) 位恒为 0），另一半永远点不到；
 *   极端情况 h2 ≡ 0 (mod m) 时 k 个点全重合 —— 就是上面"坏事 A"。
 *   强制奇数 ⇒ 与 2 的任何次幂互质 ⇒ 这 k 个点在 m 里散得最开。
 *   （我们的 m 一般不是 2 的幂，但奇数步长仍然是无害且必要的好习惯。）
 */
void bloom_derive_slots(unsigned long long h, unsigned long long m, int k,
                        unsigned long long out[], int out_cap)
{
    if (out == NULL || out_cap <= 0 || m == 0ULL || k <= 0) {
        return;                       /* 调用方给的容器不对：一个字节都不写，宁可不填 */
    }
    unsigned long long h1 = h;
    /* h2：把 h 再搅一遍（和第 2 节同一个 finalizer）。
       注意"同一个 64 位哈希"只被【读】了一次就分成了两个互不相干的种子 ——
       这正是"不用真的写 k 个哈希函数"的全部秘密。 */
    unsigned long long g = h ^ 0x9E3779B97F4A7C15ULL;
    g ^= g >> 30;
    g *= 0xBF58476D1CE4E5B9ULL;
    g ^= g >> 27;

    /* ★ 关键一步：先把两个种子都【降到 m 以内】，再做等差。
       为什么不在 64 位大数上直接算 (h1 + i*h2) % m？
         h2 是满 64 位的大数，i*h2 每加一次都会在 64 位里回绕一圈。
         回绕本身是定义良好的（对 2^64 取模），但 2^64 mod m 不等于 0，
         于是"相邻两个下标相差 h2 mod m"这条等差性质在 mod m 之后就看不出来了 ——
         结果【能用、分布也不差】，只是教学上没法一眼验证，而且白白吃一次 64 位溢出。
         先各自 % m，序列就变成 Z_m 上干净的等差数列：slot_i = a + i*b (mod m)，
         第 3 幕打印出来才能直接看出"等差 + 回绕"的样子。 */
    unsigned long long a = h1 % m;
    unsigned long long b = g % m;
    /* 步长 b 的处理，两条都要守住：
       1) b == 0 ⇒ k 个下标全重合（一个键只点 1 位）—— 绝对禁止；
       2) b 尽量取【奇数】：若 m 是 2 的幂而 b 是偶数，a + i*b 只会落在 m 的一半位置上，
          另一半永远点不到（等于白白浪费一半空间）。
       | 1 保证奇数；万一 m 是奇数、把 b 顶到了 m，就退 2（仍是奇数，且 m >= 8 所以退完 >= 1）。 */
    b |= 1ULL;
    if (b >= m) {
        b -= 2ULL;
    }

    for (int i = 0; i < k && i < out_cap; i++) {
        /* i*b <= (k-1)*m <= 31*m，远不会溢出 64 位，所以这里是纯安全的加法取模 */
        out[i] = (a + (unsigned long long)i * b) % m;
    }
}

/* ============================================================================
 * 4. m 和 k：从 (n, p) 反解
 * ============================================================================
 * 设计目标：n 个元素、误判率不超过 p。两个公式（推导见 README）：
 *      m = ceil( -n · ln p / (ln2)^2 )
 *      k = round( (m/n) · ln2 )
 * 直觉版：
 *   - 每个键点 k 个位，n 个键点完后 1 的占比约 1 - e^(-kn/m)；
 *   - 误判 ≈ (占比)^k。把占比记作 d，最小化 d^k 且满足 d^k = p，
 *     对 k 求导取极值 ⇒ 最优时 d = 1/2，即"位数组恰好一半亮一半灭"最省，
 *     解出 k = (m/n)·ln2，代回就得到 m。
 *   - 结论很硬：m/n = log2(1/p)，即"每个元素固定花 log2(1/p) 位"，
 *     p=1% ⇒ 每个元素 ≈ 10 位；p=0.1% ⇒ ≈ 14 位。这就是布隆的杀手锏。
 */
int bloom_params(unsigned long long n, double p,
                 unsigned long long *m_out, int *k_out)
{
    if (n == 0ULL) {
        printf("      [错误] bloom_params：预计元素数 n 必须是正数\n");
        return -1;
    }
    if (!(p > 0.0 && p < 1.0)) {
        printf("      [错误] bloom_params：期望误判率 p 必须在 (0,1) 开区间，收到 %f\n", p);
        return -1;
    }

    /* ln p 用我们自己写的 bloom_ln，不是 math.h 的 log() —— 见文件头"没有 -lm"的说明 */
    double ln_p = bloom_ln(p);
    double m_real = -(double)n * ln_p / BLOOM_LN2_SQ;

    /* 向上取整：不能调 ceil()（那是 libm 的符号！）。
       非负 double 先截断成整数（等价 floor），再判断"截断后是不是变小了"补 1。 */
    unsigned long long m_int = (unsigned long long)m_real;
    if ((double)m_int < m_real) {
        m_int += 1ULL;
    }
    if (m_int < (unsigned long long)BLOOM_BITS_PER_BYTE) {
        m_int = (unsigned long long)BLOOM_BITS_PER_BYTE;   /* 太小没意义，至少留一个字节 */
    }

    /* 四舍五入：+0.5 再截断。同样是"不用 round()"的理由。至少 1 个哈希。 */
    double k_real = (double)m_real / (double)n * BLOOM_LN2;
    int k_int = (int)(k_real + 0.5);
    if (k_int < 1) {
        k_int = 1;
    }
    if (k_int > 32) {
        /* 封顶纯属防御：p 小到离谱（比如 1e-20）时 k 会涨到几十，
           而本文件里 add/查询的下标容器是 slots[64]（见第 5 节）。
           与其让调用方悄悄越界写栈，不如在这里砍掉并且明说 —— 
           真要那么低的 p，正确做法是加大 n 而不是把 k 堆上天。 */
        printf("      [提示] 算出的 k=%d 太大已按 32 封顶（p 太小时应该多给 n）\n", k_int);
        k_int = 32;
    }

    if (m_out != NULL) {
        *m_out = m_int;
    }
    if (k_out != NULL) {
        *k_out = k_int;
    }
    return 0;
}

Bloom *bloom_new(unsigned long long n, double p)
{
    unsigned long long m = 0ULL;
    int k = 0;
    if (bloom_params(n, p, &m, &k) != 0) {
        return NULL;
    }

    Bloom *bf = malloc(sizeof(Bloom));
    if (bf == NULL) {
        printf("      [错误] bloom_new：申请 Bloom 结构体失败\n");
        return NULL;
    }

    /* 字节数 = 向上取整(m/8) = (m + 7) / 8。
       ★ 这个 "+7" 的技巧值得记：整数除法 /8 是向下取整，
         想向上取整就先把被除数加上 (除数-1)，即 (m + 8 - 1) / 8。
         位数组长度不是 8 的倍数时，最后那个字节会有几个位是"多出来的赠品"，
         它们永远不会被 % m 命中（下标范围是 0..m-1），但必须 allocate，否则越界。 */
    unsigned long long bytes = (m + (unsigned long long)(BLOOM_BITS_PER_BYTE - 1))
                               / (unsigned long long)BLOOM_BITS_PER_BYTE;

    /* 为什么这里用 calloc 不用 malloc：呼应 00-c-foundation/dynamic-memory 第 2 节 ——
       "数组当位图用"时，初值 0 就是算法的一部分（0 = 灯全灭 = 谁都没来过）。
       malloc 不保证清零，用 malloc 就必须再 memset 一次；calloc 顺手把溢出检查也做了。 */
    bf->bits = calloc((size_t)bytes, 1u);
    if (bf->bits == NULL) {
        printf("      [错误] bloom_new：申请 %llu 字节位数组失败\n", bytes);
        free(bf);                 /* 结构体已经拿到了，不能让它漏掉 —— 半件也要还 */
        return NULL;
    }
    bf->m = m;
    bf->bytes = bytes;
    bf->k = k;
    bf->inserts = 0ULL;
    return bf;
}

void bloom_free(Bloom *bf)
{
    if (bf == NULL) {
        return;                   /* free(NULL) 本身是合法空操作，但访问 bf->bits 会崩，所以先判 */
    }
    free(bf->bits);
    bf->bits = NULL;              /* 好习惯（dynamic-memory 第 5 节）：还不只是还，还要把纸条撕掉 */
    free(bf);
}

/* ============================================================================
 * 5. 加 & 查
 * ============================================================================
 * 两个动作合起来就是布隆的全部本体：
 *   add     : 算 k 个下标，全部 bits_set
 *   查询    : 算同样的 k 个下标，有一个没亮就敢说"肯定没有"；全亮只能说"也许有"
 *
 * 【为什么"说没有"绝对可信】（本节第一条性质，反证一遍）
 *   键 K 真被加过 ⇒ add 时它的 k 个位被按成 1 ⇒ 位数组只会 0→1、不会 1→0（标准版没有删除）
 *   ⇒ 现在去查这 k 个位，必然全亮 ⇒ 必然回答"也许有"。
 *   逆否命题就是："回答没有"（至少一位灭着）⇒ 这个键一定没被加过。
 *   ⚠ 一旦允许删除（把位清成 0），上面第二行的"只会 0→1"就断了，假阴性立刻出现 —— 第 8 幕。
 *
 * 【"说有"为什么会错】
 *   别人的键把我要看的 k 个位【分别】点亮了（不是我点的），我查的时候看到"全亮"就误判了。
 *   每多插一个键，亮着的位就多，这种"借花献佛"的概率就涨 ⇒ 越插越糊。
 */
int bloom_add(Bloom *bf, const char *key)
{
    if (bf == NULL || bf->bits == NULL || key == NULL) {
        printf("      [错误] bloom_add：参数里有 NULL\n");
        return -1;
    }
    unsigned long long slots[64];         /* k 实际只有个位数，64 格足够宽；为什么不用 VLA：
                                             C89/C11 的 VLA 在栈上不可控，教学代码宁可写死上界 */
    bloom_derive_slots(bloom_hash_str(key), bf->m, bf->k, slots, 64);
    for (int i = 0; i < bf->k; i++) {
        bits_set(bf->bits, slots[i]);
    }
    bf->inserts += 1ULL;                  /* 记账：重复加同一个键也会 +1，见坑 6 */
    return 0;
}

int bloom_add_int(Bloom *bf, int key)
{
    if (bf == NULL || bf->bits == NULL) {
        printf("      [错误] bloom_add_int：参数里有 NULL\n");
        return -1;
    }
    unsigned long long slots[64];
    bloom_derive_slots(bloom_hash_int(key), bf->m, bf->k, slots, 64);
    for (int i = 0; i < bf->k; i++) {
        bits_set(bf->bits, slots[i]);
    }
    bf->inserts += 1ULL;
    return 0;
}

/* 1 = 也许有；0 = 肯定没有；-1 = 参数非法 */
int bloom_maybe_contains(const Bloom *bf, const char *key)
{
    if (bf == NULL || bf->bits == NULL || key == NULL) {
        printf("      [错误] bloom_maybe_contains：参数里有 NULL\n");
        return -1;
    }
    unsigned long long slots[64];
    bloom_derive_slots(bloom_hash_str(key), bf->m, bf->k, slots, 64);
    for (int i = 0; i < bf->k; i++) {
        if (bits_test(bf->bits, slots[i]) == 0) {
            return 0;                     /* 一个灭的就够了：短路返回，剩下 k-1 个不用看 */
        }
    }
    return 1;
}

int bloom_maybe_contains_int(const Bloom *bf, int key)
{
    if (bf == NULL || bf->bits == NULL) {
        printf("      [错误] bloom_maybe_contains_int：参数里有 NULL\n");
        return -1;
    }
    unsigned long long slots[64];
    bloom_derive_slots(bloom_hash_int(key), bf->m, bf->k, slots, 64);
    for (int i = 0; i < bf->k; i++) {
        if (bits_test(bf->bits, slots[i]) == 0) {
            return 0;
        }
    }
    return 1;
}

void bloom_naive_remove(Bloom *bf, const char *key)
{
    if (bf == NULL || bf->bits == NULL || key == NULL) {
        printf("      [错误] bloom_naive_remove：参数里有 NULL\n");
        return;
    }
    unsigned long long slots[64];
    bloom_derive_slots(bloom_hash_str(key), bf->m, bf->k, slots, 64);
    for (int i = 0; i < bf->k; i++) {
        bits_clear(bf->bits, slots[i]);
    }
    /* 【反面教材，正式代码禁止调用】
       这 k 个位不是"我的"，只是"我路过时借用的"。清掉它们，等于把别人家的灯也拧灭，
       于是那个真在里面的人会被查成"肯定没有" —— 假阴性，布隆过滤器唯一的承诺被撕毁。
       第 8 幕会当场演示：删 "cat" 之后另一个键凭空消失。
       顺带一提：这里刻意【不动 bf->inserts】—— 因为动了它也无意义，
       "加了几个键"这个账目在允许删除的那一刻就已经和现实对不上了（unsigned 还会回绕成天文数字）。 */
}

unsigned long long bloom_ones(const Bloom *bf)
{
    if (bf == NULL || bf->bits == NULL) {
        return 0ULL;
    }
    unsigned long long ones = 0ULL;
    /* 数 1 的个数：逐字节用"打擦边球"的 klinger 式循环太绕，教学版本老老实实一位一位问。
       复杂度 O(m)，只在体检时调用，不参与 add/查询的热路径。 */
    for (unsigned long long i = 0; i < bf->m; i++) {
        ones += (unsigned long long)bits_test(bf->bits, i);
    }
    return ones;
}

double bloom_density(const Bloom *bf)
{
    if (bf == NULL || bf->m == 0ULL) {
        return 0.0;
    }
    return (double)bloom_ones(bf) / (double)bf->m;
}

void bloom_describe(const Bloom *bf, const char *tag)
{
    if (bf == NULL) {
        printf("      %s (NULL)\n", (tag != NULL) ? tag : "");
        return;
    }
    unsigned long long ones = bloom_ones(bf);
    printf("      %s m=%llu 位 / k=%d 个哈希 / %llu 字节（= m/8 向上取整）/ 已 add %llu 次 / 1 的占比 %.4f%%\n",
           (tag != NULL) ? tag : "",
           bf->m, bf->k, bf->bytes, bf->inserts,
           (bf->m > 0ULL) ? 100.0 * (double)ones / (double)bf->m : 0.0);
}

/* ============================================================================
 * 6. 计数布隆 Counting Bloom Filter —— 唯一能删的做法
 * ============================================================================
 * 思路：位数组的每一格从"1 bit 开关"换成"一个计数器"。
 *   add    : k 格各 +1        （原来：置 1）
 *   remove : k 格各 -1        （原来：不能动）
 *   查询   : k 格全部 > 0     （原来：全部 == 1）
 * 这样清掉某个键的贡献时，别人借用过的那一格还剩着正数，不会被误拧灭。
 *
 * 【空间账】每格 4 bit 就够（计到 15，实践中 k 通常 < 10，很少撞顶）⇒ 标准版的 4 倍；
 *   本节用 1 字节/格 ⇒ 8 倍，换来的是"不用处理半个字节的奇偶对齐"，代码能看。
 *   无论哪种，计数布隆都不再是"极致省空间"的那个东西了 —— 这是删除能力的价钱。
 *
 * 【新的坑：计数器饱和】一格到 255 再加就回绕到 0（无符号溢出是定义好的取模，
 *   但语义上"占用数"就错了）。回绕后 remove 会把这格减成 255，
 *   于是某个真的在里面的键被查到"某格为 0"⇒ 假阴性。所以：
 *     · add 时如果撞到 255 就【停在 255 不再加】，并把 counter_overflowed 置 1 永久留案；
 *     · 一旦留案，此后所有 remove 都不再保证正确 —— 本节会打印这个警告。
 * ============================================================================ */
CountingBloom *counting_bloom_new(unsigned long long n, double p)
{
    unsigned long long m = 0ULL;
    int k = 0;
    if (bloom_params(n, p, &m, &k) != 0) {
        return NULL;
    }
    CountingBloom *cb = malloc(sizeof(CountingBloom));
    if (cb == NULL) {
        printf("      [错误] counting_bloom_new：申请结构体失败\n");
        return NULL;
    }
    cb->cnt = calloc((size_t)m, 1u);      /* 一格一个字节 ⇒ 字节数就是 m，不除 8 */
    if (cb->cnt == NULL) {
        printf("      [错误] counting_bloom_new：申请 %llu 字节计数数组失败\n", m);
        free(cb);
        return NULL;
    }
    cb->m = m;
    cb->bytes = m;
    cb->k = k;
    cb->inserts = 0ULL;
    cb->deletes = 0ULL;
    cb->counter_overflowed = 0;
    return cb;
}

void counting_bloom_free(CountingBloom *cb)
{
    if (cb == NULL) {
        return;
    }
    free(cb->cnt);
    cb->cnt = NULL;
    free(cb);
}

int counting_bloom_add(CountingBloom *cb, const char *key)
{
    if (cb == NULL || cb->cnt == NULL || key == NULL) {
        printf("      [错误] counting_bloom_add：参数里有 NULL\n");
        return -1;
    }
    unsigned long long slots[64];
    bloom_derive_slots(bloom_hash_str(key), cb->m, cb->k, slots, 64);
    int saturated = 0;
    for (int i = 0; i < cb->k; i++) {
        if (cb->cnt[slots[i]] < BLOOM_COUNT_MAX) {
            cb->cnt[slots[i]] += 1;
        } else {
            saturated = 1;                /* 停在 255，绝不回绕成 0 */
        }
    }
    cb->inserts += 1ULL;
    if (saturated) {
        cb->counter_overflowed = 1;       /* 永久留案：之后 remove 不再保证正确 */
        return 1;
    }
    return 0;
}

int counting_bloom_maybe_contains(const CountingBloom *cb, const char *key)
{
    if (cb == NULL || cb->cnt == NULL || key == NULL) {
        printf("      [错误] counting_bloom_maybe_contains：参数里有 NULL\n");
        return -1;
    }
    unsigned long long slots[64];
    bloom_derive_slots(bloom_hash_str(key), cb->m, cb->k, slots, 64);
    for (int i = 0; i < cb->k; i++) {
        if (cb->cnt[slots[i]] == 0u) {
            return 0;                     /* 有一格是 0 ⇒ 肯定没有，承诺同样成立 */
        }
    }
    return 1;
}

int counting_bloom_remove(CountingBloom *cb, const char *key)
{
    if (cb == NULL || cb->cnt == NULL || key == NULL) {
        printf("      [错误] counting_bloom_remove：参数里有 NULL\n");
        return -1;
    }
    if (counting_bloom_maybe_contains(cb, key) == 0) {
        return -2;                        /* 本来就不在：一个计数器都不许动！
                                             乱减别人的一格，就是在制造假阴性 */
    }
    unsigned long long slots[64];
    bloom_derive_slots(bloom_hash_str(key), cb->m, cb->k, slots, 64);
    for (int i = 0; i < cb->k; i++) {
        if (cb->cnt[slots[i]] > 0u) {     /* 双保险：绝不让它减成负数（255 回绕） */
            cb->cnt[slots[i]] -= 1;
        }
    }
    cb->deletes += 1ULL;
    return cb->counter_overflowed;        /* 0 = 干净删除；1 = 删了但历史上饱和过，结果不可信 */
}
