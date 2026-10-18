#ifndef BLOOM_H
#define BLOOM_H

/*
 * ============================================================================
 * 布隆过滤器 Bloom Filter —— 本文件的"菜单"
 * ============================================================================
 *
 * 【一句话】
 *   一条位数组 + k 个哈希下标。它只回答一个问题：
 *      "这个键【肯定没在里面】，还是【也许在里面】？"
 *   说"肯定没有" ⇒ 100% 可信（不会说谎）。
 *   说"也许有"   ⇒ 有一定概率是假的（误判），这个概率在设计时就定好了。
 *
 * 【为什么值得存在】
 *   哈希表/Set 想判"在不在"，必须把键【本身】存下来：5000 个字符串就要 5000 份
 *   字符串副本 + 5000 个节点。布隆过滤器把键揉成 k 个下标，只在位数组上"打点"，
 *   键本身一个字节都不留 —— 空间是【位数】级别的，跟键多长、有多少个键几乎无关。
 *   代价：会误判，而且不能删。所以它的正确用法是"挡在真存储前面的门神"：
 *   它说没有 ⇒ 直接拒绝，一次数据库都不用查；它说有 ⇒ 再去查真表确认。
 *
 * 【本节的三个技术要点】
 *   1) 位数组：一个 unsigned char 数组当 m 个开关用，靠 /8、%8、移位、或运算定位。
 *      这是本节最容易看错的三行代码，bits_set / bits_test 里有逐字解释。
 *   2) k 个哈希 → 只写【一个】64 位哈希，再用 h1 + i*h2 组合出 k 个下标。
 *      这是工程上的标准做法，bloom_derive_slots 上方讲清了它为什么够用。
 *   3) m 和 k 是从 (n, p) 反解出来的：m = -n·ln p / (ln2)²，k = (m/n)·ln2。
 *      本仓库编译命令【没有 -lm】，所以 ln 是自己用级数手搓的（bloom_ln）。
 *
 * 【键的类型】
 *   本节按仓库铁律：只支持 char * 和 int 两种键，存的值一律是 int（其实布隆连值都不存）。
 *   不搞 void * 泛型，不搞函数指针比较器 —— 想要别的类型就照抄一份改哈希函数，C 就这样。
 *
 * 【删除】
 *   标准布隆过滤器【不支持删除】：清空一个键的 k 个位，会顺手清掉别人的位 ⇒ 假阴性
 *   ⇒ 破坏了"说没有就一定没有"这条唯一的命根子。
 *   真要删就用计数布隆（Counting Bloom），本节也实现了：counting_bloom_* 一组函数。
 */

/* 一个字节里的位数。C 标准只保证 CHAR_BIT >= 8，本机是 8；
   bloom.c 里用 _Static_assert 在编译期把它钉死，不然后面的 /8 %8 全是空中楼阁。 */
#define BLOOM_BITS_PER_BYTE 8

/* 计数布隆每个计数器占 1 字节（无符号），最大计到 255。
   为什么不用 4 bit：4 bit 要处理"半个字节"的奇偶对齐，代码翻倍；
   本节优先把"计数"这件事讲清楚，空间账在 README 里明算（1 字节 = 比标准版贵 8 倍）。 */
#define BLOOM_COUNT_MAX 255u

/* ============================================================================
 * 结构体
 * ============================================================================ */

/* 标准布隆过滤器：只有位数组，没有键、没有计数，所以不能删。 */
typedef struct {
    unsigned char *bits;          /* 位数组本体：m 个位挤在 bytes 个字节里 */
    unsigned long long m;         /* 位数（不是字节数！）—— 公式算出来的那个 m */
    unsigned long long bytes;     /* 字节数 = (m + 7) / 8，向上取整 */
    int k;                        /* 每个键打几个点（哈希个数） */
    unsigned long long inserts;   /* 调用过多少次 bloom_add（重复键也计数，见 README 坑 6） */
} Bloom;

/* 计数布隆：位数组换成"每格一个 0..255 的计数器"，能加能删。 */
typedef struct {
    unsigned char *cnt;           /* cnt[i]：第 i 位当前被几个键占用 */
    unsigned long long m;         /* 计数格数（等价于标准版的位数） */
    unsigned long long bytes;     /* 字节数 = m（一格一字节，贵 8 倍） */
    int k;
    unsigned long long inserts;
    unsigned long long deletes;
    int counter_overflowed;       /* 一旦有格子撞到 255 就置 1 并且不复位：此后删除不再安全 */
} CountingBloom;

/* ============================================================================
 * 1. 位数组三件套：本节最容易看错、也最有教学价值的三行代码
 * ============================================================================
 * 把"第 idx 个开关"落到内存上，只需要想清楚两件事：
 *   idx / 8  ⇒ 这个开关住在第几个字节里（一字节装 8 个开关）
 *   idx % 8  ⇒ 进去之后它是这个字节的第几位
 * 然后：置位用 |= ，测试用 &= 后看非零，清位用 &= ~。
 */
void bits_set(unsigned char *bits, unsigned long long idx);   /* 把第 idx 位按成 1（开灯） */
void bits_clear(unsigned char *bits, unsigned long long idx); /* 把第 idx 位按成 0（关灯）—— 危险，见第 8 幕 */
int  bits_test(const unsigned char *bits, unsigned long long idx); /* 1 = 亮着，0 = 灭着 */

/* 打印若干字节的二进制长相（MSB 在左），用来"亲眼看见"1u << (idx%8) 到底点了哪一格 */
void bits_dump(const unsigned char *bits, unsigned long long n_bytes, const char *tag);

/* ============================================================================
 * 2. 哈希：把键揉成一个 64 位无符号整数
 * ============================================================================
 */

/* char * 键：64 位 FNV-1a。返回 0 也可以是一个合法哈希值，所以不设错误码。 */
unsigned long long bloom_hash_str(const char *key);

/* int 键：先把整数"搅散"（乘大素数 + 右移异或），见 bloom.c 里那段
   "为什么要搅散：不搅散的话 k 个点会全撞在一起，一个键只占 1 位"的实测说明。 */
unsigned long long bloom_hash_int(int key);

/*
 * 从一个 64 位哈希出发，算出 k 个下标写进 out[]。
 *   h     ：bloom_hash_str / bloom_hash_int 出来的原始哈希
 *   m     ：位数组的位数
 *   k     ：要几个下标
 *   out   ：接收数组，长度至少 out_cap
 *   out_cap：out 实际能装几个（不足 k 就只填前 out_cap 个）
 * 组合方式：a = h % m；b = 把 h 再搅一遍后 % m、并强制成奇数；slot_i = (a + i*b) % m。
 * 也就是"在 Z_m 上走一条等差数列"。
 * 这就是"双重哈希当 k 个独立哈希用"，为什么够用、为什么先把种子取模到 m 以内，
 * 请读 bloom.c 第 3 节的大段注释。
 */
void bloom_derive_slots(unsigned long long h, unsigned long long m, int k,
                        unsigned long long out[], int out_cap);

/* ============================================================================
 * 3. 参数推导：给定"要放 n 个元素、最多容忍 p 的误判率"，反解 m 和 k
 * ============================================================================
 */

/* 自己实现的 ln（自然对数），x <= 0 时返回 0 并由调用方挡住。
   为什么不用 <math.h>：本仓库的编译命令没有 -lm，log() 会链接失败。
   只用加减乘除，级数收敛到 1e-16 量级，对"算 m 该取几"绰绰有余。 */
double bloom_ln(double x);

/* 反解公式：
   m = ceil( -n · ln p / (ln2)² )   （位数）
   k = round( m / n · ln2 )         （哈希个数），至少 1
   任何一个是 NULL 就跳过那个输出；参数非法时返回 -1。 */
int bloom_params(unsigned long long n, double p,
                 unsigned long long *m_out, int *k_out);

/* ============================================================================
 * 4. 生命周期
 * ============================================================================
 */

/* 造一个"容量 n、目标误判率 p"的过滤器。p 必须在 (0,1) 开区间，例如 0.01 = 1%。
   失败返回 NULL（参数非法或内存申请失败，两种情况都会打印一行说明）。 */
Bloom *bloom_new(unsigned long long n, double p);
void   bloom_free(Bloom *bf);          /* 谁 malloc 谁 free：唯一的还债机会 */

CountingBloom *counting_bloom_new(unsigned long long n, double p);
void           counting_bloom_free(CountingBloom *cb);

/* ============================================================================
 * 5. 四个动作
 * ============================================================================
 */

/* 登记一个键：把它 k 个位全按成 1。返回 0 成功 / -1 参数非法。 */
int bloom_add(Bloom *bf, const char *key);
int bloom_add_int(Bloom *bf, int key);

/* 查询：返回 1 = 【可能存在】（k 个位全亮），0 = 【肯定没有】（至少一位是灭的），
   -1 = 参数非法。注意"1"不代表真的有，这是本节的核心。 */
int bloom_maybe_contains(const Bloom *bf, const char *key);
int bloom_maybe_contains_int(const Bloom *bf, int key);

/* 统计"1 的占比"（= 已点亮的位 / m），它是误判率的直接来源：占比^k ≈ 误判率。 */
unsigned long long bloom_ones(const Bloom *bf);
double bloom_density(const Bloom *bf);

/* 打印一行体检报告：m / k / 字节 / 插入次数 / 1 的占比。 */
void bloom_describe(const Bloom *bf, const char *tag);

/* ---------- 计数布隆：唯一能删的版本 ---------- */
/* 返回值：0 成功；-1 参数非法；1 成功但【有计数器饱和】（此后删除可能造假阴性）。 */
int counting_bloom_add(CountingBloom *cb, const char *key);
int counting_bloom_remove(CountingBloom *cb, const char *key); /* 0 删了 / 1 删了但曾饱和 / -1 非法 / -2 本来就不在，没动 */
int counting_bloom_maybe_contains(const CountingBloom *cb, const char *key);

/* ---------- 反面教材专用：标准布隆过滤器的"朴素删除" ----------
 * 它就是把 k 个位清零。bloom.c 里演示完它会立刻产生假阴性，所以这个函数
 * 存在的意义只有一个：让你亲眼看到【不该这么写】。正式代码里没有安全用法。 */
void bloom_naive_remove(Bloom *bf, const char *key);

#endif /* BLOOM_H */
