#ifndef STATIC_ARRAY_H
#define STATIC_ARRAY_H

/*
 * 这个头文件自己就要用到 bool，所以自己负责 include <stdbool.h>。
 * 规矩：头文件需要的东西自己带上，别指望调用者帮你带 —— 单独 #include 本文件
 * 也必须能编译通过，这叫"头文件自包含"。
 */
#include <stdbool.h>

/*
 * ===================================================================
 * static-array.h —— 静态数组（顺序表）的「菜单」
 * ===================================================================
 * 这个文件只有声明，没有实现。像饭店门口的菜单：先看有哪些菜，
 * 想知道怎么炒的再去翻 static-array.c。
 *
 * 【静态数组到底"静"在哪？】
 *   长度在【编译期】就写死在代码里了，比如：
 *         int a[5];
 *   编译器看完这行，就在二进制里记下"这块要 20 字节"，
 *   运行时没有任何"要不要再大一点"的商量余地。
 *   想改成 6 个？改源码 → 重新编译。这就是"定死"两个字的代价。
 *
 * 【和动态数组（下一节）的根本区别，只有两条】
 *   1) 内存来源：静态数组的格子住在【栈】上（如果是局部变量）或【全局区】，
 *      是编译器早就圈好的固定地盘；动态数组住【堆】上，是 malloc 现租的。
 *   2) 生命周期：静态数组随所在作用域自动生灭，不用你 free，
 *      也【不能】free（free 一个栈地址 = 非法释放，程序当场 abort）；
 *      动态数组谁 malloc 谁 free，忘了就是泄漏。
 *
 * 【这一节真正要练的东西】
 *   数组的"看家本事"是 O(1) 按下标访问 —— 因为它连续；
 *   数组的"致命短板"也是连续 —— 插入/删除要把后面的格子整排挪动，O(n)。
 *   本文件把这两个特性都做成了可以打印出来的函数：
 *   insert/remove 各写了两版，一版用 memmove（工程写法），
 *   一版手写循环、每挪一格就打印一次（教学写法），
 *   跑一遍就能亲眼看见"挪格子"到底挪了多少下。
 *
 * 【为什么这里把数组包进结构体？】
 *   光有 `int data[8]` 只知道"有 8 个格子"，不知道"我用了几个"。
 *   所以必须再带一个 length 记账（这是所有顺序表写法的第一条铁律）：
 *        data[]  = 房子有几间
 *        length  = 现在住了几个人
 *   漏了 length，后面的插入删除全都无从判断边界。
 */

/* 编译期常量：格子总数。用 #define 而不是变量，就是为了"编译期定死"这件事。
 * 注意它是【格子总数】，不是元素个数；元素个数由 length 记着。 */
#define SARRAY_CAPACITY 8

/*
 * 在数组"出生的地方"算长度，这是 C 语言唯一的官方套路：
 *      sizeof(a)      = 整个数组占多少字节（这里是 5*4=20）
 *      sizeof(a[0])   = 一个元素占多少字节（这里是 4）
 *      相除 = 5 个元素
 * ⚠ 这个宏【只能】用在真正的数组变量上（还没退化时）。
 *   一旦 a 是函数参数（`void f(int a[])`，实际是 `int *a`），
 *   sizeof(a) 就变成指针大小 8，算出来的商是 2（在 int=4 字节的机器上），
 *   错得离谱却一个字都不报 —— 见 static-array.c 里 sizeof_inside_function 的实测。
 *   外层 (int) 是强转：sizeof 的类型是 size_t（无符号），
 *   不转的话拿去和 int 循环变量比较会踩"有符号/无符号"的坑。
 */
#define STATIC_ARRAY_LEN(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))

/*
 * 静态数组 + 长度记账 = 教科书里说的"顺序表"。
 * 关键：data 是【内嵌数组】，不是指针！这一条就定义了"静态"两个字：
 *   sizeof(StaticArray) = 8*4(data) + 4(length) + 4(moved_cells) = 40 字节，
 *   编译期就锁死，整块内存连着结构体自己一起被分配/回收，一个 malloc 都不用。
 */
typedef struct {
    int data[SARRAY_CAPACITY];  /* 格子本体：住在结构体内部（结构体在哪它就在哪） */
    int length;                 /* 当前实际有几个元素，合法范围 0..SARRAY_CAPACITY */
    int moved_cells;            /* 上一次插入/删除挪动了多少格，纯粹为了把 O(n) 打印出来 */
} StaticArray;

/* ---------- 生命周期 ---------- */

/*
 * 初始化：length 归零，格子全填成 -999 这种"一眼看得出是垃圾"的值。
 * 为什么要特意填垃圾值？因为静态数组不会自动清零（栈上是脏的），
 * 而插入删除演示里"没被用过的格子"和"值为 0 的格子"必须一眼能分开，
 * 否则挪完一格你根本看不出到底是数据动了还是本来就有数。
 * 返回值用 bool：申请失败这种事在静态数组里根本不存在，它一定成功。
 */
void static_array_init(StaticArray *sa);

/*
 * 灌一批初值进去（一次性把 length 定好）。
 * values 是调用者的数组，n 是元素个数（必须连长度一起传，见第 00 节退化那一课）。
 * 装不下（n > 容量）时返回 false 并且一个都不写 —— 不做"写多少算多少"的半截事，
 * 半截状态会让后面的演示看不懂。
 */
bool static_array_fill(StaticArray *sa, const int *values, int n);

/*
 * "销毁"一个静态数组。
 * ⚠ 它内部【没有】free —— 这不是偷懒，而是静态数组的本质：
 *   没有任何一块堆内存归它管，所以没有任何东西需要归还。
 *   这里做的事只是把账面抹平（length=0、格子涂回垃圾），
 *   让"结构体还在但已经没内容"这件事可见。
 *   接口故意保留成和动态数组同名套路（xxx_destroy），
 *   这样你以后换用动态数组时，main.c 结尾那一行都不用改。
 */
void static_array_destroy(StaticArray *sa);

/* ---------- 只有读，永远 O(1) 或 O(n)，且都不碰内存边界 ---------- */

int  static_array_size(const StaticArray *sa);      /* 现在有几个元素 */
int  static_array_capacity(void);                   /* 能装几个（编译期常量，不需要 sa 也能问） */
bool static_array_is_empty(const StaticArray *sa);
bool static_array_is_full(const StaticArray *sa);

/*
 * 按下标读。返回 bool 表示"这次读合法吗"，值走 value_out 带出来。
 * 为什么要这么啰嗦？因为数组下标越界在 C 里【不会】报错，只会静悄悄读走别人的内存。
 * 把检查写进唯一的出入口，是静态数组唯一的自我保护手段：
 *   index < 0  || index >= length → false，value_out 原封不动。
 * 这正是 O(1) 随机访问的代价反面：语言不检查，就得自己检查。
 */
bool static_array_get(const StaticArray *sa, int index, int *value_out);

/* 按下标写。同样挡越界，防止把结构体后面的内存写花。 */
bool static_array_set(StaticArray *sa, int index, int value);

/*
 * 按值找第一个匹配的下标，找不到返回 -1。
 * 这是 O(n) 的：数组只保证"连续"，不保证"有序"，所以只能一格一格看。
 * （如果数组是排好序的，就能二分 O(log n) —— 见 11-searching/binary-search。）
 */
int  static_array_find(const StaticArray *sa, int value);

/* ---------- 插入 / 删除：这一节的主角，全是 O(n) ---------- */

/*
 * 工程写法：用 memmove 一次挪完。
 * index 允许等于 length（表示插在最末尾），必须 0 <= index <= length。
 * 容量满了返回 false —— 静态数组这时候【无计可施】，
 * 因为它不能像动态数组那样"再租一块大的"。这是静态数组最现实的痛点。
 * 为什么是 memmove 不是 memcpy？因为源和目标会重叠（见 .c 里的图解），
 * memcpy 遇到重叠是未定义行为，memmove 才是"重叠也保证搬对"的那个。
 */
bool static_array_insert_at(StaticArray *sa, int index, int value);

/*
 * 教学写法：和上面功能完全一样，但每搬一格就打印一次整排格子。
 * 用它跑一次"往第 0 格插"，就能亲眼看到 O(n) 到底是 n 次什么。
 */
bool static_array_insert_at_stepwise(StaticArray *sa, int index, int value);

/*
 * 删除第 index 格，被删掉的值通过 value_out 带出来（可能要用它做统计）。
 * 必须 0 <= index < length，空表直接 false。
 */
bool static_array_remove_at(StaticArray *sa, int index, int *value_out);
bool static_array_remove_at_stepwise(StaticArray *sa, int index, int *value_out);

/* 尾部追加：静态数组里唯一不用挪格子的插入，真正的 O(1)。 */
bool static_array_push_back(StaticArray *sa, int value);

/* ---------- 打印 ---------- */

/*
 * 一行值 + 账面。8 个格子【全部】打印，中间用一个 ‖ 标出"元素到哪里为止"：
 *      [  15|  99|  20 ‖ -1099| -1096 ...]  元素 3 / 格子 8  上次挪 3 格
 * ‖ 右边的格子还在但不是元素（里面是残留值/垃圾值）——
 * "格子总数"和"元素个数"必须一眼分得开，这是顺序表第一课。
 */
void static_array_print(const StaticArray *sa, const char *tag);

/*
 * 内存地图：五行对齐打印 —— 下标 / 地址 / 相对偏移 / 数值 / 身份（元素还是空位）。
 * 这是本节的"看图识字"环节：看完就明白 a[i] 就是"从起点往前走 i 格"，
 * 以及"格子存在"和"格子里是有效元素"为什么是两件事。
 * 还会顺手打印 sa 和 &sa.data[0]，证明"结构体第一个成员的地址就是结构体地址"。
 */
void static_array_print_memory_map(const StaticArray *sa, const char *tag);

/* ---------- 专门用来演示"退化 / sizeof 陷阱"的教学函数 ---------- */

/*
 * 故意把数组（退化后）当参数收进来，然后在函数体里打印 sizeof(arr)。
 * 输出会让你看到 8（指针大小）而不是 20/32（整个数组的字节数）—— 这就是退化。
 *
 * 这里参数【写成】 `const int *arr` 而不是 `const int arr[]`，两者编译出来是同一个东西，
 * 但写成 [] 时 gcc 会额外给一条警告：
 *     warning: 'sizeof' on array function parameter 'arr' will return size of 'const int *'
 * 这条警告本身就是在说"你以为你是数组，其实你早就只是指针了"。
 * 本仓库要求零警告编译，所以我们诚实地写成指针，教训一个字都不少。
 */
void sizeof_inside_function(const int *arr);

/* 两种遍历写法，结果必须一样：证明 a[i] 就是 *(a+i) 的语法糖。 */
int sum_by_index(const int *arr, int n);
int sum_by_pointer_walk(const int *arr, int n);

#endif /* STATIC_ARRAY_H */
