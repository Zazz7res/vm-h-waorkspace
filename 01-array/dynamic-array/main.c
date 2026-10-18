#include <stdio.h>
#include <string.h>   /* strcmp：解析命令行开关 */
#include "dynamic-array.h"

/* 小工具：打印一行分隔线（和第 00 节同款） */
static void section(const char *title)
{
    printf("\n----------------------------------------\n");
    printf("%s\n", title);
    printf("----------------------------------------\n");
}

int main(int argc, char **argv)
{
    /* ============================================================
       第 1 幕：size 和 capacity 是两个数字（本节最重要的图景）
       ============================================================ */
    section("第 1 幕：租了 4 间房，只住了 3 个人 —— size != capacity");

    {
        DynArray v;
        dynarray_init(&v, 4);
        printf("  连续 push 3 个元素（每次只是往空位写一个值，一次 realloc 都没发生）：\n");
        dynarray_push_back(&v, 10);
        dynarray_push_back(&v, 20);
        dynarray_push_back(&v, 30);
        dynarray_print(&v, "  现在          ");
        dynarray_print_memory_map(&v, "内存地图", 8);
        printf("  看图说话：\n");
        printf("    capacity=4 → 堆上真的有 4 个格子（4*%zu = %zu 字节），地址连续；\n",
               sizeof(int), (size_t)4 * sizeof(int));
        printf("    size=3     → 只有前 3 格是元素，第 3 格是空位，里面是涂过的垃圾值；\n");
        printf("    下次 push 只要 size < capacity，就【不租新房、不搬任何东西】—— 这就是 O(1)；\n");
        printf("    代价：多租了 %d 格没人住，白占 %zu 字节 —— 空间换时间，vector 的全部交易。\n",
               v.capacity - v.size, (size_t)(v.capacity - v.size) * sizeof(int));
        dynarray_destroy(&v);
    }

    /* ============================================================
       第 2 幕：满了就翻倍 —— 把每次 realloc 的地址打出来
       ============================================================ */
    section("第 2 幕：push 到装不下时，容量翻倍（realloc 现场直播）");

    {
        DynArray v;
        dynarray_init(&v, 4);
        printf("  起始 4 格，现在 push 到第 9 个元素，中间会撞上两次扩容：\n");
        for (int i = 1; i <= 9; i++) {
            printf("  push %d：", i * 10);
            if (i == 5 || i == 9) {
                printf("\n");      /* 这两次会触发扩容，先换行让日志不打横 */
            }
            dynarray_push_back(&v, i * 10);
            if (i != 5 && i != 9) {
                printf("  size=%d capacity=%d（空位够用，没碰 realloc）\n", v.size, v.capacity);
            }
        }
        dynarray_print(&v, "  9 个元素之后  ");
        dynarray_print_memory_map(&v, "16 格里只住了 9 个", 16);
        printf("  数一下这两次扩容抄走的元素：4 + 8 = 12 个，换来后面 7 次 push 全都白嫖空位。\n");
        printf("  ⚠ 观察日志里「原地扩大」和「整块搬家」两种结局：地址没变时一个字节都不用抄；\n");
        printf("     搬家时 realloc 帮你把老数据整块抄过去，旧地址当场作废 —— 所以必须\n");
        printf("     `p = realloc(p, n)` 用返回值覆盖自己，别留旧指针。（详见 00-c-foundation/dynamic-memory）\n");
        dynarray_destroy(&v);
    }

    /* ============================================================
       第 3 幕：为什么是翻倍而不是 +1 —— 真跑一遍对照实验
       ============================================================ */
    section("第 3 幕：翻倍 vs 每次 +1：把两个数字跑出来对比（不是嘴上说说的摊还）");

    {
        int total = 40;

        DynArray dbl;                       /* 实验组：容量翻倍 */
        DynArray pl1;                       /* 对照组：每次只 +1 格 */
        dynarray_init(&dbl, DYNARRAY_MIN_CAPACITY);
        dynarray_init(&pl1, DYNARRAY_MIN_CAPACITY);
        dynarray_set_growth_policy(&pl1, 1, 1);   /* 新容量 = 旧容量*1 + 1，就是"每次多一格" */

        /* 这两个实验只关心最后的统计数字，日志先关掉，否则 +1 组要刷屏 36 行 */
        dbl.log_enabled = 0;
        pl1.log_enabled = 0;

        for (int i = 1; i <= total; i++) {
            dynarray_push_back(&dbl, i);
            dynarray_push_back(&pl1, i);
        }

        printf("  同样 push %d 个元素，两组都从 %d 格起步（日志已关，只看账本）：\n",
               total, DYNARRAY_MIN_CAPACITY);
        printf("  ┌────────────┬──────────┬──────────┬──────────┬──────────────┐\n");
        printf("  │ 扩容策略   │ 扩容次数 │ 真搬家   │ 最终容量 │ 最坏抄动个数 │\n");
        printf("  ├────────────┼──────────┼──────────┼──────────┼──────────────┤\n");
        printf("  │ 翻倍 2x    │ %8d │ %8d │ %8d │ %12llu │\n",
               dbl.grow_count, dbl.grow_moved_count, dbl.capacity, dbl.copy_total);
        printf("  │ 每次 +1    │ %8d │ %8d │ %8d │ %12llu │\n",
               pl1.grow_count, pl1.grow_moved_count, pl1.capacity, pl1.copy_total);
        printf("  └────────────┴──────────┴──────────┴──────────┴──────────────┘\n");
        dynarray_print_grow_report(&dbl, "翻倍组");
        dynarray_print_grow_report(&pl1, "+1 组");
        printf("  结论（数字都是上面真跑出来的）：\n");
        printf("    翻倍：扩容次数是 log 级别的，抄动总量 < 2*%d，摊到每次 push 仍然是常数 → 摊还 O(1)；\n",
               total);
        printf("    +1  ：每 push 一个都要 realloc 一次、把整排老元素抄一次，总量是 %d*%d/2 级 → O(n^2)。\n",
               total, total);
        printf("  直觉版解释：翻倍意味着「第 k 次扩容之后，要走够 2^(k-1) 步才会撞上下一次扩容」，\n");
        printf("  搬家这种贵动作被后面一大串免费的空位填充分摊掉了；+1 则让贵动作每步都发生。\n");
        printf("  顺便一个工程事实：翻倍也不是越大越好 —— 涨得太快会多占内存（最坏空位接近一半），\n");
        printf("  所以真实库里的做法通常是「翻倍但有上限」或「按块增长」，思路一样：让贵动作尽量稀疏。\n");
        dynarray_destroy(&dbl);
        dynarray_destroy(&pl1);
    }

    /* ============================================================
       第 4 幕：中间插入 / 删除 —— 动态数组救不了 O(n)
       ============================================================ */
    section("第 4 幕：insert_at / remove_at 照样要挪格子（动态化只解决「装不下」）");

    {
        DynArray v;
        dynarray_init(&v, 4);
        for (int i = 1; i <= 5; i++) {
            dynarray_push_back(&v, i * 100);      /* 100 200 300 400 500，中间撞一次扩容 */
        }
        dynarray_print(&v, "  起始          ");

        printf("\n  往 index=1 插 999：\n");
        dynarray_insert_at(&v, 1, 999);
        dynarray_print(&v, "  插入后        ");
        printf("  挪了 %d 格（size - index = 5 - 1 = 4，插到越靠前挪得越多）。\n",
               v.size - 1 - 1);

        printf("\n  删 index=0：\n");
        int removed = 0;
        dynarray_remove_at(&v, 0, &removed);
        dynarray_print(&v, "  删除后        ");
        printf("  被删的值 = %d，后半排左移 %d 格。\n", removed, v.size - 1);

        printf("\n  按下标读两种姿势（这就是 C++ vector 里 operator[] 和 at() 的区别）：\n");
        int out = -1;
        if (dynarray_at(&v, 2, &out)) {
            printf("    at(2) 安全检查通过 → %d\n", out);
        }
        printf("    get(2) 不设防直读      → %d（快，但下标错了就是你自己的锅）\n",
               dynarray_get(&v, 2));
        if (!dynarray_at(&v, 100, &out)) {
            printf("    at(100) 被拦下，返回 false，out 仍是 %d —— 这就是检查版的作用\n", out);
        }
        printf("    find(400) 找到第 %d 格；找不存在的 7 返回 %d（不排序就只能线性扫，O(n)）\n",
               dynarray_find(&v, 400), dynarray_find(&v, 7));
        dynarray_destroy(&v);
    }

    /* ============================================================
       第 5 幕：缩容 —— 低于 1/4 才砍一半
       ============================================================ */
    section("第 5 幕：pop 太多会「把房子退掉一半」：1/4 阈值现场");

    {
        DynArray v;
        dynarray_init(&v, 4);
        for (int i = 1; i <= 33; i++) {
            dynarray_push_back(&v, i);       /* 4→8→16→32→64：撞 4 次扩容 */
        }
        dynarray_print(&v, "  装了 33 个    ");
        printf("  现在 size=%d / capacity=%d，利用率刚过一半 —— 空位太多，得能退回去。\n",
               v.size, v.capacity);

        printf("\n  判定式是 size * 4 <= capacity * 1，所以容量 %d 的阈值线画在 size=%d 那一格：\n",
               v.capacity, v.capacity / (DYNARRAY_DEFAULT_SHRINK_DENOMINATOR));
        int popped = 0;
        while (v.size > 17) {
            dynarray_pop_back(&v, &popped);   /* 一路 pop 到 17：全程没有一次 realloc，日志静悄悄 */
        }
        printf("  此刻 size=%d capacity=%d：%d*4=%d 不小于 %d*1=%d → 还差一格没压线，所以【不砍】。\n",
               v.size, v.capacity, v.size, v.size * v.shrink_denominator,
               v.capacity, v.capacity * v.shrink_numerator);
        printf("  这一格很关键：有空位 ≠ 该缩容，判定只看「压没压线」。\n");

        int cap_before = v.capacity;   /* 先把砍之前的容量存下来：等砍完再打，数字就已经是新的了 */
        dynarray_pop_back(&v, &popped);       /* size 16 → 16*4=64 <= 64*1 → 压线，缩容 */
        printf("  再 pop 一个：size=%d，%d*4=%d <= %d*1=%d → 压线触发缩容，看上面那行日志：\n",
               v.size, v.size, v.size * v.shrink_denominator,
               cap_before, cap_before * v.shrink_numerator);
        dynarray_print(&v, "  缩容之后      ");
        printf("  容量 %d → %d，归还 %d 格 = %zu 字节；砍完 size=%d 只有新容量 %d 的一半，\n",
               cap_before, v.capacity, cap_before - v.capacity,
               (size_t)(cap_before - v.capacity) * sizeof(int), v.size, v.capacity);
        printf("  这一半就是「缓冲带」：要再 push 满 %d 个才碰得到扩容线，中间每步都是 O(1)。\n",
               v.capacity - v.size);

        printf("\n  继续 pop 到空，看它停在哪儿：\n");
        while (v.size > 0) {
            dynarray_pop_back(&v, &popped);   /* 16→8→4 一路砍，最后撞上地板 */
        }
        dynarray_print(&v, "  全 pop 完     ");
        printf("  容量停在 %d，而 base_capacity=%d —— 它【不会】砍到 0：\n", v.capacity, v.base_capacity);
        printf("  否则「清空再填满」会在 0 格和 4 格之间反复 malloc/free，又是一轮抖动。\n");
        printf("  真要彻底归还内存，用的是 destroy，不是把 size 弹成 0 —— 这两个动作不是一回事。\n");
        dynarray_destroy(&v);
    }

    /* ============================================================
       第 6 幕：1/2 就砍 vs 1/4 才砍 —— 抖动实测
       ============================================================ */
    section("第 6 幕：为什么不是「掉到 1/2 就砍」—— 抖动（thrashing）实测");

    {
        int rounds = 20;

        DynArray good;      /* 阈值 1/4：本仓库默认 */
        DynArray bad;       /* 阈值 1/2：反面教材 */
        dynarray_init(&good, 4);
        dynarray_init(&bad, 4);
        dynarray_set_shrink_policy(&bad, 1, 2);   /* size*2 <= capacity*1，即 size <= capacity/2 就砍 */

        /* 两组做【完全相同】的动作搭出同一个起点：push 9 个 → size=9 / capacity=16。
         * 这个位置是精心挑的，两个条件都得满足：
         *   1) 1/2 组离阈值线只差 1 格（size 掉到 8 就压线），1/4 组离得远（要掉到 4）；
         *   2) capacity=16 明显高于 base_capacity=4，缩容不会被地板拦住 —— 否则砍一次就触底，
         *      抖不起来，实验就变成了「两组都安静」，什么也证明不了（这一点很容易踩，见下面的坑）。 */
        for (int i = 1; i <= 9; i++) {
            dynarray_push_back(&good, i);
            dynarray_push_back(&bad, i);
        }
        printf("  共同起点：size=%d capacity=%d base_capacity=%d（两组账面完全一样）\n",
               good.size, good.capacity, good.base_capacity);
        printf("  阈值线：1/4 组画在 size<=%d，1/2 组画在 size<=%d；现在 size=%d，两组都还没压线。\n",
               good.capacity * good.shrink_numerator / good.shrink_denominator,
               bad.capacity * bad.shrink_numerator / bad.shrink_denominator, bad.size);
        printf("  然后各做 %d 轮【pop 一下 push 一下】—— 最坏情况，故意来回摩擦：\n", rounds);

        good.log_enabled = 0;      /* 关日志，只看统计，否则 1/2 组要刷屏 40 行 */
        bad.log_enabled = 0;
        int good_reallocs_at_start = good.grow_count + good.shrink_count;
        int bad_reallocs_at_start = bad.grow_count + bad.shrink_count;
        unsigned long long good_copy_at_start = good.copy_total;
        unsigned long long bad_copy_at_start = bad.copy_total;
        for (int i = 0; i < rounds; i++) {
            dynarray_pop_back(&good, NULL);
            dynarray_push_back(&good, i);
            dynarray_pop_back(&bad, NULL);
            dynarray_push_back(&bad, i);
        }

        int good_hits = good.grow_count + good.shrink_count - good_reallocs_at_start;
        int bad_hits = bad.grow_count + bad.shrink_count - bad_reallocs_at_start;
        unsigned long long good_copied = good.copy_total - good_copy_at_start;
        unsigned long long bad_copied = bad.copy_total - bad_copy_at_start;
        printf("  ┌──────────┬────────────────┬──────────────┬──────────────┐\n");
        printf("  │ 缩容阈值 │ %d 轮内 realloc │ %d 轮内抄动 │ 扩容/缩容次数 │\n", rounds, rounds);
        printf("  ├──────────┼────────────────┼──────────────┼──────────────┤\n");
        printf("  │ 1/4 才砍 │ %14d │ %12llu │ %4d /%4d │\n",
               good_hits, good_copied, good.grow_count, good.shrink_count);
        printf("  │ 1/2 就砍 │ %14d │ %12llu │ %4d /%4d │\n",
               bad_hits, bad_copied, bad.grow_count, bad.shrink_count);
        printf("  └──────────┴────────────────┴──────────────┴──────────────┘\n");
        printf("  （前两列只算 %d 轮 pop/push 期间的新增；第三列是从头累计，起点那 9 次 push 两组都扩过 2 次容）\n", rounds);
        printf("  两组动作完全一样，差别只在阈值线画在哪儿：\n");
        printf("    1/4 组：%d 轮里 realloc 次数 = %d —— 缩容线和扩容线之间隔着一整段空位，来回碰不到；\n",
               rounds, good_hits);
        printf("    1/2 组：%d 轮里 realloc 次数 = %d，也就是每轮 %d 次（pop 触发缩一次 + push 触发扩一次）——\n",
               rounds, bad_hits, bad_hits / rounds);
        printf("             两条线贴在一起，一擦就着：容量在 %d 格和 %d 格之间反复横跳；\n",
               bad.capacity, bad.capacity / 2);
        printf("    抄动元素数：1/2 组 %llu 个 vs 1/4 组 %llu 个 —— 每次 pop/push 都变成 O(n)，\n",
               bad_copied, good_copied);
        printf("             %d 轮下来就是 %llu 次抄写；把 rounds 加大，这个数还会线性往上撞。\n",
               rounds, bad_copied);
        printf("  搬家实况：1/2 组 %d 次扩容里真搬家 %d 次，%d 次缩容里真搬家 %d 次。\n",
               bad.grow_count, bad.grow_moved_count, bad.shrink_count, bad.shrink_moved_count);
        printf("  （realloc 未必每次都搬家，但 %d 次调用本身就说明：本该 O(1) 的动作被换成了 O(n) 动作。）\n",
               bad_hits);
        printf("  记法：扩容线和缩容线【必须隔开一段缓冲带】，任何「来回切换大小」的资源都这规矩\n");
        printf("  （git 的 pack、JVM 的堆、内存分配器全都一样，不是 vector 的怪癖）。\n");
        dynarray_destroy(&good);
        dynarray_destroy(&bad);
    }

    /* ============================================================
       第 7 幕：懒启动 —— 空表可以一格内存都不占
       ============================================================ */
    section("第 7 幕：init(0) 懒启动：一张空表不该占任何堆内存");

    {
        DynArray v;
        dynarray_init(&v, 0);                /* 传 0 或负数 = 先不租 */
        dynarray_print(&v, "  刚 init(0)   ");
        dynarray_print_memory_map(&v, "此时没有内存地图可画", 8);
        printf("  读一下空表：is_empty=%d，at(0) 会返回 false —— 什么都没有，也就不会骗你。\n",
               dynarray_is_empty(&v));
        printf("  现在第一次 push_back(42)：\n");
        dynarray_push_back(&v, 42);
        dynarray_print(&v, "  push 一个之后");
        printf("  第一次扩容从 %d 格起租（DYNARRAY_MIN_CAPACITY），不是一次只租 1 格。\n",
               DYNARRAY_MIN_CAPACITY);
        dynarray_destroy(&v);
    }

    /* ============================================================
       第 8 幕（开关才触发）：get 越界会长什么样
       ============================================================ */
    section("第 8 幕：不设防的 get 越界 —— 语言真的不拦你");

    if (argc > 1 && strcmp(argv[1], "--show-get-oob") == 0) {
        printf("  你传了 --show-get-oob，现在【故意】用 dynarray_get 越界读，而且分两层：\n");
        DynArray v;
        dynarray_init(&v, 4);
        dynarray_push_back(&v, 7);
        dynarray_push_back(&v, 8);
        dynarray_print(&v, "  表里有 2 个元素");
        dynarray_print_memory_map(&v, "越界实验现场", 8);

        printf("\n  第 1 层：index=2 —— 越过了 size（只有 0/1 是元素），但还没越出 capacity（4 格真的租着）。\n");
        int spare = dynarray_get(&v, 2);
        printf("    dynarray_get(&v, 2) 拿到 %d：这是【空位里的残留值】（毒值 -2000+i，见 init 那一段）。\n",
               spare);
        printf("    危险就在这儿：ASan 【不会】报，程序照常跑完，但你拿到的根本不是元素 ——\n");
        printf("    它读的是自己的内存，只是内容是垃圾。这种 bug 只能靠 size/capacity 别搞混来防。\n");

        printf("\n  第 2 层：index=%d（= capacity）—— 这回越出的不只是逻辑边界，连堆上的格子都不存在了。\n",
               dynarray_capacity(&v));
        fflush(stdout);                         /* ASan 一报错就直接终止，先把要说的话说完 */
        volatile int bad_index = dynarray_capacity(&v);   /* 用 volatile 防止编译器把越界读优化掉 */
        int oob = dynarray_get(&v, bad_index);
        printf("    dynarray_get(&v, %d) 拿到 %d —— 这一行本来就不该打出来，ASan 会先拦下它。\n",
               bad_index, oob);
        dynarray_destroy(&v);
        printf("  ⚠ 上一行如果打出来了，说明这次运行没被 ASan 抓到（换个分配器场景就可能真的读到邻居内存）；\n");
        printf("     无论抓不抓得到，结论都一样：get 越界 = 未定义行为，只有 at 会替你把住门。\n");
    } else {
        printf("  dynarray_get 里一个 if 都没有，所以越界直接未定义行为；\n");
        printf("  dynarray_at 才是有检查的那一档。默认运行只演示 at 被拦下（第 4 幕）。\n");
        printf("  越界还分两层，两层都不好惹：\n");
        printf("    index 在 [size, capacity) 里 → 读到自己租的空位，不崩、不报错，拿到垃圾值（最阴）；\n");
        printf("    index >= capacity            → 越出堆上那块内存，ASan 报 heap-buffer-overflow。\n");
        printf("  想亲眼看第 2 层现场（ASan 会报错，进程非 0 退出）：\n");
        printf("      gcc -Wall -Wextra -Wshadow -std=c11 -g -fsanitize=address -static-libasan \\\n");
        printf("          01-array/dynamic-array/*.c -o /tmp/verify_arr_2 && /tmp/verify_arr_2 --show-get-oob\n");
        printf("  规矩很简单：写循环时下标上界一律用 size，别用 capacity；\n");
        printf("  要读空位就明说自己在读空位 —— 否则就是在读运气。\n");
    }

    /* ============================================================
       收尾
       ============================================================ */
    section("收尾：谁申请谁归还");
    printf("  本 main 里每一个 dynarray_init 都在同一个作用域结束时 dynarray_destroy 过了，\n");
    printf("  所以默认运行 ASan 必须报 0 泄漏（这就是交付前的验收闸门）。\n");
    printf("  对照一下上一节：静态数组连 malloc 都没有，所以永远不可能漏；\n");
    printf("  动态数组换来了「想多大就多大」，代价就是多了一条必须自己承担的责任。\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住五句话：\n");
    printf("  1) size 是「住了几个人」，capacity 是「租了几间房」，永远 size <= capacity；越界事故全在这俩数字被搞混。\n");
    printf("  2) push_back 摊还 O(1) 不是因为搬家便宜，而是因为搬家【很少发生】—— 翻倍让贵动作尽量稀疏。\n");
    printf("  3) realloc 可能原地也可能搬家，所以永远 `p = realloc(p, n)`；失败返回 NULL 时原内存照旧可用。\n");
    printf("  4) 缩容要留缓冲带：低于 1/4 才砍一半，砍到 base_capacity 就停手，否则 pop/push 一来回就抖动。\n");
    printf("  5) destroy = free + data=NULL + size=capacity=0，三件事一件都不能少；中间增删仍是 O(n)。\n");
    return 0;
}
