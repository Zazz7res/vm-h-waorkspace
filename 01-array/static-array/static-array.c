#include <stdio.h>
#include <string.h>   /* memmove 来自这里。注意：不是 <stdlib.h>，很多人第一次都写错 */
#include "static-array.h"

/*
 * ===================================================================
 * 0. 一个内部小工具：往格子里涂"一眼可辨的垃圾"
 * ===================================================================
 * 为什么要涂垃圾而不填 0？
 *   填 0 的话，"这一格还没被用过"和"这一格存着数值 0"就分不清了。
 *   插入删除会把格子来回挪，看不清哪格是搬来的、哪格是从空位变出来的，
 *   教学演示就白做了。所以统一涂 -(1000+i)，每格都不一样，挪到哪都认得出。
 * static 修饰的函数只有本文件能用（相当于"私有方法"），
 * 放在 .c 里不写进 .h，是为了让菜单保持干净。
 */
static void poison_cells(int *cell, int count)
{
    for (int i = 0; i < count; i++) {
        cell[i] = -(1000 + i);   /* -1000, -1001, -1002 ... 每格都不同 */
    }
}

/*
 * ===================================================================
 * 1. 初始化：不申请任何内存，只清账面
 * ===================================================================
 * 静态数组没有 malloc，所以也没有"申请失败"这种状态：
 * 结构体躺在哪（栈上/全局区）格子就躺在哪，编译器早就圈好了地盘。
 * 这里唯一要做的正经事是把 length 归零 —— 忘了这一步，
 * 后面所有操作都会拿着一堆脏格子当"已有元素"来处理。
 */
void static_array_init(StaticArray *sa)
{
    if (sa == NULL) {
        return;                     /* 传进来空指针就别硬写，正式项目这里该 abort */
    }
    poison_cells(sa->data, SARRAY_CAPACITY);
    sa->length = 0;
    sa->moved_cells = 0;
    printf("      static_array_init：格子总数（编译期定死）=%d，元素个数=0；一个字节都没 malloc\n",
           SARRAY_CAPACITY);
}

/*
 * ===================================================================
 * 2. 灌初值：一次把 length 定好
 * ===================================================================
 * values 是"外面那个数组"，退化成指针后长度就丢了，
 * 所以必须连 n 一起传 —— 这是第 00 节 pointer-basics 讲过的老规矩，
 * 到这里已经变成每个函数的默认要求了。
 * 装不下就整体失败、一个不写：宁可什么都不做，也不做"写一半"的半截状态。
 */
bool static_array_fill(StaticArray *sa, const int *values, int n)
{
    if (sa == NULL || values == NULL || n < 0) {
        printf("      [失败] 参数不合法（sa/values 为空或 n 是负数）\n");
        return false;
    }
    if (n > SARRAY_CAPACITY) {
        /* 这就是静态数组的天花板：想多装一个都做不到，只能改代码重编译 */
        printf("      [失败] 想灌 %d 个，但编译期就定死只有 %d 格 —— 静态数组帮不了你，\n",
               n, SARRAY_CAPACITY);
        printf("              要么改 #define 重编译，要么换下一节的动态数组。\n");
        return false;
    }
    for (int i = 0; i < n; i++) {
        sa->data[i] = values[i];    /* values[i] 和 *(values + i) 完全等价 */
    }
    poison_cells(&sa->data[n], SARRAY_CAPACITY - n);  /* 没用到的格子继续涂垃圾 */
    sa->length = n;
    sa->moved_cells = 0;
    return true;
}

/*
 * ===================================================================
 * 3. "销毁"：重点在于【没有 free】
 * ===================================================================
 * 静态数组的格子内嵌在结构体里，结构体是调用者的局部变量（住在栈上）。
 * 函数一返回，整块 40 字节自动被回收 —— 这是编译器替你做掉的。
 * 如果在这里写 free(sa->data) 会怎样？
 *   free 只能归还 malloc/calloc/realloc【当前】返回的堆地址。
 *   sa->data 是栈地址，free 它 = 非法释放（free 了不是堆的指针），
 *   ASan 会直接报 "attempting free on address which was not malloc'd"，
 *   glibc 则多半 abort。这是 C 初学者死亡率最高的错误之一。
 * 所以本函数只做账面清理，并在打印里把"为什么没有 free"说清楚。
 */
void static_array_destroy(StaticArray *sa)
{
    if (sa == NULL) {
        return;
    }
    int gone = sa->length;
    sa->length = 0;
    sa->moved_cells = 0;
    poison_cells(sa->data, SARRAY_CAPACITY);
    printf("      static_array_destroy：清掉了账面（原本有 %d 个元素）。\n", gone);
    printf("      ⚠ 这里【故意没有 free】：格子内嵌在结构体里，跟着结构体一起活、一起死；\n");
    printf("         写 free(sa->data) 就是释放栈地址，是非法释放，程序会当场 abort。\n");
}

/* ---------- 4. 只读查询：四个一句话的小函数 ---------- */

int static_array_size(const StaticArray *sa)
{
    return (sa != NULL) ? sa->length : 0;
}

/*
 * 容量为什么不需要 sa 参数？
 * 因为它是【编译期常量】，跟"哪一个数组"毫无关系 —— 问天问地问谁都答 8。
 * 动态数组的 capacity 就必须传结构体了，因为每个数组的容量可以不一样。
 * 这一个参数之差，就是"静态"和"动态"最直观的照面。
 */
int static_array_capacity(void)
{
    return SARRAY_CAPACITY;
}

bool static_array_is_empty(const StaticArray *sa)
{
    return (sa == NULL) ? true : (sa->length == 0);
}

bool static_array_is_full(const StaticArray *sa)
{
    return (sa == NULL) ? true : (sa->length == SARRAY_CAPACITY);
}

/*
 * ===================================================================
 * 5. get / set：越界检查写在这里，而不是指望语言帮你
 * ===================================================================
 * C 语言对 a[i] 只做一件事：算出地址 (起点 + i*4)，然后照办。
 * 它不会问"i 超没超"，因为：
 *   1) 数组变量里【没有】长度信息 —— 编译器压根不知道你有几格
 *      （只有 &a 那种"数组类型"才知道，而一当参数就退化没了）；
 *   2) 每次访问都比一遍 i<n 要有额外开销，C 的哲学是"为爽利，不付钱"。
 * 结果就是：越界不报错，只是读/写到了隔壁的内存 —— 未定义行为。
 * 所以我们把检查收进唯一的出入口：
 *   注意上界用 length 而不是 SARRAY_CAPACITY：
 *   下标在 [length, CAPACITY) 之间虽然是"合法的格子"，但里面装的是垃圾，
 *   当作元素读出来一样是 bug。这两层要分得清。
 */
bool static_array_get(const StaticArray *sa, int index, int *value_out)
{
    if (sa == NULL || value_out == NULL) {
        return false;
    }
    if (index < 0 || index >= sa->length) {
        printf("      [拒绝] index=%d 不在 [0, %d) 里 —— 语言不拦你，函数拦你\n",
               index, sa->length);
        return false;
    }
    *value_out = sa->data[index];
    return true;
}

bool static_array_set(StaticArray *sa, int index, int value)
{
    if (sa == NULL) {
        return false;
    }
    if (index < 0 || index >= sa->length) {
        printf("      [拒绝] index=%d 越界（现有 %d 个元素），不写\n", index, sa->length);
        return false;
    }
    sa->data[index] = value;
    return true;
}

/*
 * ===================================================================
 * 6. find：O(n) 线性扫描
 * ===================================================================
 * 数组按下标访问是 O(1)，但"按值找"是 O(n) —— 这两个 O 特别容易被混。
 * 区别在于：下标是"我直接告诉你第几格"，值要"一格一格比对才知道"。
 * 返回第一个匹配的下标，找不到返回 -1（用 -1 而不是 0，因为 0 是合法下标）。
 */
int static_array_find(const StaticArray *sa, int value)
{
    if (sa == NULL) {
        return -1;
    }
    for (int i = 0; i < sa->length; i++) {
        if (sa->data[i] == value) {
            return i;               /* 命中就立刻收工，平均要扫一半 */
        }
    }
    return -1;
}

/*
 * ===================================================================
 * 7. 插入（memmove 版）—— O(n) 的现场解剖
 * ===================================================================
 * 想插到第 index 格，就必须先把 [index, length) 这排格子整体往右挪一格。
 *
 *   插入前（length=5，往 index=1 插 99）：
 *     下标   0     1     2     3     4    (5)   (6)   (7)
 *          ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
 *          │ 10  │ 20  │ 30  │ 40  │ 50  │ 垃  │ 垃  │ 垃  │
 *          └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
 *                            ↑ 从这里开始，全部右移一格
 *   挪动过程（【必须从右往左】搬，反了就完蛋）：
 *     data[5] = data[4];  // 50
 *     data[4] = data[3];  // 40
 *     data[3] = data[2];  // 30
 *     data[2] = data[1];  // 20
 *     ┌─────┬─────┬─────┬─────┬─────┬─────┐
 *     │ 10  │ 20  │ 30  │ 40  │ 50  │ 50  │   ← 1 号位还是旧值，先别动它
 *     └─────┴─────┴─────┴─────┴─────┴─────┘
 *   最后 data[1] = 99：
 *     ┌─────┬─────┬─────┬─────┬─────┬─────┐
 *     │ 10  │ 99  │ 20  │ 30  │ 40  │ 50  │   length 变成 6
 *     └─────┴─────┴─────┴─────┴─────┴─────┘
 *
 * 【为什么 memmove 而不是 memcpy？】
 *   源 [1,5) 和目标 [2,6) 是【重叠】的。memcpy 明确不处理重叠，
 *   它内部可能正着抄：先把 data[1]=20 抄到 data[2]，接着又把 data[2]（已经是 20）
 *   抄到 data[3] → 结果全是 20，数据毁掉。
 *   memmove 保证重叠也搬对（它自己会判断该倒着抄还是正着抄，或用临时缓冲）。
 *   口诀：【区间可能重叠就用 memmove，绝不许重叠才用 memcpy。】
 *
 * 【挪动次数 = length - index】：插到最前面要挪 length 次，插到最后挪 0 次。
 *   平均 length/2，所以插入是 O(n)；把它记进 moved_cells，好让 main 打印出来。
 */
bool static_array_insert_at(StaticArray *sa, int index, int value)
{
    if (sa == NULL) {
        return false;
    }
    if (sa->length >= SARRAY_CAPACITY) {
        /* 静态数组在这里就没有退路了：不能扩容。这正是下一节动态数组存在的全部理由。 */
        printf("      [失败] 已满（%d/%d），静态数组不会自己变大，插入无处安放\n",
               sa->length, SARRAY_CAPACITY);
        sa->moved_cells = 0;
        return false;
    }
    if (index < 0 || index > sa->length) {
        /* 注意上界是 length 而不是 length-1：index == length 是"插到末尾"，合法 */
        printf("      [失败] index=%d 不在 [0, %d] 之内\n", index, sa->length);
        sa->moved_cells = 0;
        return false;
    }

    int tail_cells = sa->length - index;              /* 要挪几格 */
    size_t bytes = (size_t)tail_cells * sizeof(int);   /* memmove 的单位是字节！不是格数 */

    /*
     * memmove(目标起点, 源起点, 字节数)
     *   目标 = &data[index+1]：整排右移一格后的落点
     *   源   = &data[index]   ：现在住在 index 的那格开始
     *   tail_cells == 0 时（插到末尾）memmove 搬 0 字节，合法且什么都不做，
     *   所以这里不需要为"插到末尾"特判。
     */
    memmove(&sa->data[index + 1], &sa->data[index], bytes);

    sa->data[index] = value;      /* 空出来的那格终于写上主角 */
    sa->length++;                 /* 记账：多了一个元素 —— 忘了这行是顺序表第一大 bug */
    sa->moved_cells = tail_cells;
    return true;
}

/*
 * ===================================================================
 * 8. 插入（逐格打印版）—— 把上面那一次 memmove 拆成看得见的循环
 * ===================================================================
 * 和 static_array_insert_at 唯一的区别是：手写循环、每搬一格打印一次整排格子。
 * 这个循环【故意从右往左】写：`i = length-1 递减到 index`。
 * 为什么必须倒着？正着抄的话，data[i] 刚被写成的新值，下一轮又被当成源抄走，
 * 于是一个值像病毒一样传染了后面所有格子（打印出来会看到一串重复数字）。
 * 这也是 memmove 内部在重叠场景要做的事：它检测到"目标在源右边"，就倒着抄。
 */
bool static_array_insert_at_stepwise(StaticArray *sa, int index, int value)
{
    if (sa == NULL || sa->length >= SARRAY_CAPACITY) {
        printf("      [失败] 结构体为空或已经满了，逐格演示无法开始\n");
        return false;
    }
    if (index < 0 || index > sa->length) {
        printf("      [失败] index=%d 不合法\n", index);
        return false;
    }

    printf("      逐格演示：往 index=%d 插 %d，需要挪 %d 格（倒着搬！）\n",
           index, value, sa->length - index);
    static_array_print(sa, "        挪动前");

    int moved = 0;
    for (int i = sa->length - 1; i >= index; i--) {
        sa->data[i + 1] = sa->data[i];      /* 就这一行，搬一格 */
        moved++;
        printf("        第 %d 步：data[%d] = data[%d] = %d\n", moved, i + 1, i, sa->data[i + 1]);
        static_array_print(sa, "          现在");
    }

    sa->data[index] = value;
    sa->length++;
    sa->moved_cells = moved;
    static_array_print(sa, "        写新值并 length++ 后");
    return true;
}

/*
 * ===================================================================
 * 9. 删除（memmove 版）—— 整排左移一格
 * ===================================================================
 *   删除前（length=6，删 index=1）：
 *     ┌─────┬─────┬─────┬─────┬─────┬─────┐
 *     │ 10  │ 99  │ 20  │ 30  │ 40  │ 50  │
 *     └─────┴─────┴─────┴─────┴─────┴─────┘
 *                 ↑ 这个要消失
 *   删完后尾巴多出一个"重复的 50"，那格已经不算元素了（length 变 5），
 *   但它里面残留旧值 —— 所以逻辑上必须靠 length 划界，不能靠"这格是不是垃圾"。
 *     ┌─────┬─────┬─────┬─────┬─────┬─────┐
 *     │ 10  │ 20  │ 30  │ 40  │ 50  │ 50残│   ← 尾巴残留值，通常再涂个垃圾方便调试
 *     └─────┴─────┴─────┴─────┴─────┴─────┘
 *   挪动次数 = length - index - 1；删最后一个挪 0 次，删第一个挪 length-1 次。
 *   插入/删除的平均代价都是 n/2 → O(n)。这两行加在一起，
 *   就是"数组适合读多写少，不适合频繁在头部增删"的全部理由。
 */
bool static_array_remove_at(StaticArray *sa, int index, int *value_out)
{
    if (sa == NULL) {
        return false;
    }
    if (sa->length == 0) {
        printf("      [失败] 空表，没有东西可删\n");
        sa->moved_cells = 0;
        return false;
    }
    if (index < 0 || index >= sa->length) {
        printf("      [失败] index=%d 不在 [0, %d) 之内\n", index, sa->length);
        sa->moved_cells = 0;
        return false;
    }

    int removed_value = sa->data[index];      /* 先把要删的值存下来：一挪就被覆盖了 */
    int tail_cells = sa->length - index - 1;
    memmove(&sa->data[index], &sa->data[index + 1], (size_t)tail_cells * sizeof(int));

    sa->length--;                             /* 记账：少了一个元素（顺序表第二大 bug 就是忘这行） */
    sa->data[sa->length] = -(1099);           /* 尾巴残留涂成 -1099，让它一眼看得出"这不是元素" */
    sa->moved_cells = tail_cells;

    if (value_out != NULL) {
        *value_out = removed_value;           /* 输出参数：被删值从指针带出去 */
    }
    return true;
}

/* 删除（逐格打印版）：左移时【必须正着搬】，和插入的朝向正好相反。
 * 为什么反了？这次目标在源的【左边】（data[i] = data[i+1]），
 * 正着抄时读的位置永远比写的位置靠右，不会被自己覆盖；反过来倒着抄就白删了。
 * 结论很好记：搬家的方向永远是"往不会踩到自己新家的方向搬"。 */
bool static_array_remove_at_stepwise(StaticArray *sa, int index, int *value_out)
{
    if (sa == NULL || sa->length == 0) {
        printf("      [失败] 结构体为空或表已空\n");
        return false;
    }
    if (index < 0 || index >= sa->length) {
        printf("      [失败] index=%d 不合法\n", index);
        return false;
    }

    int removed_value = sa->data[index];
    printf("      逐格演示：删 index=%d（值 %d），需要左挪 %d 格（正着搬！）\n",
           index, removed_value, sa->length - index - 1);
    static_array_print(sa, "        删除前");

    int moved = 0;
    for (int i = index; i + 1 < sa->length; i++) {
        sa->data[i] = sa->data[i + 1];
        moved++;
        printf("        第 %d 步：data[%d] = data[%d] = %d\n", moved, i, i + 1, sa->data[i]);
        static_array_print(sa, "          现在");
    }

    sa->length--;
    sa->data[sa->length] = -(1099);
    sa->moved_cells = moved;
    static_array_print(sa, "        length-- 之后");
    if (value_out != NULL) {
        *value_out = removed_value;
    }
    return true;
}

/*
 * ===================================================================
 * 10. push_back：数组唯一"不用搬家"的插入
 * ===================================================================
 * 往末尾加一格，挪动次数 0 —— 这是 O(1)。
 * 记住这个事实：数组的"尾部"是它最便宜的位置，
 * 下一节动态数组的整个设计（容量翻倍）就是围绕"只从尾部加"这一件事建立的。
 */
bool static_array_push_back(StaticArray *sa, int value)
{
    if (sa == NULL || sa->length >= SARRAY_CAPACITY) {
        if (sa != NULL) {
            printf("      [失败] 已满（%d/%d）：静态数组到此为止\n", sa->length, SARRAY_CAPACITY);
        }
        return false;
    }
    sa->data[sa->length] = value;   /* 先写在"下一个空位" */
    sa->length++;                   /* 再承认它 —— 顺序不能颠倒，颠倒后 length 指着没数据的格子 */
    sa->moved_cells = 0;
    return true;
}

/* ---------- 11. 打印 ---------- */

/*
 * 一行值 + 账面。
 * ⚠ 这里【把 8 个格子全打印出来】，用一个 ‖ 分隔「元素区」和「空位区」。
 *   ‖ 右边的格子还在，但它们不属于本表（length 没把它们算进去），
 *   里面装的多半是插入/删除留下的【残留值】或初始化时的垃圾值。
 * 为什么费事把空位也打出来？因为"格子存在"和"格子里是有效元素"是两回事：
 *   格子有 8 个（编译期定死），元素只有 length 个 —— 这两个数字一旦混起来，
 *   后面学动态数组的 size / capacity 就永远学不明白。
 *   顺序表靠 length 划界，而这条边界是看不见的 —— 现在让它看得见。
 */
void static_array_print(const StaticArray *sa, const char *tag)
{
    if (sa == NULL) {
        printf("%s: (NULL)\n", tag);
        return;
    }
    printf("%s: [", tag);
    for (int i = 0; i < SARRAY_CAPACITY; i++) {
        /* 先打印分隔符再打印数值：第 length 格是第一个"不是元素"的格子，
         * 所以边界画在它【左边】。length == 0 时边界就在最开头，
         * 一眼看出"整排都还是空位"；length == 容量时没有空位，自然没有 ‖。 */
        if (i == sa->length) {
            printf(" ‖ ");
        } else if (i > 0) {
            printf("|");
        }
        printf("%4d", sa->data[i]);
    }
    printf("]  元素 %d / 格子 %d  上次挪 %d 格\n",
           sa->length, SARRAY_CAPACITY, sa->moved_cells);
}

/*
 * 内存地图：五行严格对齐 —— 下标 / 地址 / 相对偏移 / 数值 / 是否元素。
 * 目的只有一个：让你亲眼看见 "a[i] 的地址 = 首地址 + i*4"。
 * 每格宽度固定 15 字符（地址本身就是 14 字符，窄了会糊成一片看不清）。
 * 地址每次运行都变（系统开了地址随机化），但【相邻两格永远差 4 字节】——
 * 会变的是数字，不变的是规律，看的是规律。
 */
void static_array_print_memory_map(const StaticArray *sa, const char *tag)
{
    if (sa == NULL) {
        printf("%s: (NULL)\n", tag);
        return;
    }
    printf("  %s  ——  结构体地址 sa（=&sa）= %p，首格地址 &sa.data[0] = %p\n",
           tag, (void *)sa, (void *)&sa->data[0]);
    printf("            两者数值相等：C 保证成员按声明顺序排、第一个成员贴紧结构体开头\n");

    printf("        下标: ");
    for (int i = 0; i < SARRAY_CAPACITY; i++) {
        printf("%-15d", i);
    }
    printf("\n        地址: ");
    for (int i = 0; i < SARRAY_CAPACITY; i++) {
        printf("%-15p", (void *)&sa->data[i]);
    }
    printf("\n      相对偏移: ");
    for (int i = 0; i < SARRAY_CAPACITY; i++) {
        printf("+%-14d", i * (int)sizeof(int));   /* 距首格多少字节 = i*sizeof(int) */
    }
    printf("\n        数值: ");
    for (int i = 0; i < SARRAY_CAPACITY; i++) {
        printf("%-15d", sa->data[i]);
    }
    printf("\n        身份: ");
    for (int i = 0; i < SARRAY_CAPACITY; i++) {
        /* 「元素」/「空位」= 6 字节但只有 4 个显示列，宽度要给 17（=15+2）才和上面的数值行对齐 */
        printf("%-17s", (i < sa->length) ? "元素" : "空位");
    }
    printf("\n        （图例：元素 = length 之内的真实数据；空位 = 长度之外的格子，里面是残留垃圾值）\n");
    printf("        说明: 相邻两格地址相差 %zu 字节 = sizeof(int)，这就是 data+i 走一格的物理含义；\n",
           sizeof(int));
    printf("              标着「空位」的那些格子里躺的是垃圾或搬剩下的旧值，它们【不是】元素，读了就是 bug。\n");
}

/*
 * ===================================================================
 * 12. sizeof 陷阱现场：退化就发生在这里
 * ===================================================================
 * 参数无论写 `const int arr[]` 还是 `const int *arr`，编译器都当成后者处理
 * （[] 在参数列表里只是个"提示"，不携带长度）。
 * 于是在这个函数体里：
 *      sizeof(arr)   = 指针大小（x86-64 是 8）
 * 而在数组出生的那一层作用域里：
 *      sizeof(data)  = 整个数组的字节数（5 个 int = 20）
 * 为什么长度信息会"掉"？因为传参时只有"首地址"这一个数字被拷进栈/寄存器，
 * 数组到底几格这件事从来没被带上路 —— C 的数组参数根本不携带长度。
 * 所以：任何处理数组的函数都必须多收一个 n。这就是本文件每个函数要么带 length
 * 字段、要么干脆用结构体把长度一起打包的原因。
 */
void sizeof_inside_function(const int *arr)
{
    /*
     * 先把两个 sizeof 的结果存进变量，再相除。
     * 为什么不直接写 sizeof(arr)/sizeof(arr[0])？
     * 因为 gcc 认得这个"经典错误姿势"，会当场警告（这条警告其实是好事，它在替你把关）：
     *     warning: division 'sizeof (指针) / sizeof ('行解引用'); 
     *              它算不出元素个数 [-Wsizeof-pointer-div]
     * 本仓库要求零警告，所以这里把两个值先落到变量上再除 —— 数值一模一样，教训一个字没少。
     */
    size_t bytes_of_param = sizeof(arr);       /* 指针大小：8 */
    size_t bytes_of_cell  = sizeof(arr[0]);    /* 元素大小：4 */

    /* 用 printf("%p") 需要 void *，这里顺手演示一次"指针只当数据打印，不解引用" */
    printf("      [函数内部] 收到的地址 = %p（它其实就是 main 里那个数组的首地址）\n", (void *)arr);
    printf("      [函数内部] sizeof(arr)      = %zu  ← 这是【指针】的大小，不是数组的\n",
           bytes_of_param);
    printf("      [函数内部] sizeof(arr[0])   = %zu  ← 元素大小还是准的（它只看那一格）\n",
           bytes_of_cell);
    printf("      [函数内部] 用它算出的\"长度\" = %d  ← 8/4=2，看着像模像样，其实完全错\n",
           (int)(bytes_of_param / bytes_of_cell));
    printf("      结论：STATIC_ARRAY_LEN 这种宏只能在数组出生的作用域里用，出了就废。\n");
}

/*
 * 两种遍历：a[i] 是 *(a+i) 的语法糖，跑出来结果必然相同。
 * 这里用 long long 累加（而不是 int）：数组一大、值一大就溢出，
 * 溢出在 C 里是有符号数的未定义行为，别在求和这一步翻车。
 * 返回值仍是 int：本节的数组很小，够用；真要防溢出就返回 long long。
 */
int sum_by_index(const int *arr, int n)
{
    long long total = 0;
    for (int i = 0; i < n; i++) {
        total += arr[i];            /* 人话：第 i 个 */
    }
    return (int)total;
}

int sum_by_pointer_walk(const int *arr, int n)
{
    long long total = 0;
    const int *cursor = arr;        /* 抄一份地址自己走，arr 本身不动（它是 const 指向，可随便移） */
    for (int i = 0; i < n; i++) {
        total += *cursor;           /* 机器视角：先取当前格，再往前走一格 */
        cursor++;                   /* cursor++ 一次跳 sizeof(int)=4 字节，不是跳 1 字节 —— 指针加法的"单位"是元素 */
    }
    return (int)total;
}
