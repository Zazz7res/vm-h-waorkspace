#include <stdio.h>
#include <stdlib.h>
#include "deque.h"

/*
 * main.c —— 双端队列的六幕演示
 *
 * 重点是第 2 幕：C 语言里负数取模会算出 -1，所以 front/rear 倒退一律写
 * (x - 1 + capacity) % capacity。每一步都打印 front/rear，绕圈的那一刻会特别说明。
 *
 * 编译运行（在项目根目录）：
 *   make one DIR=04-queue/deque
 */

static void scene(const char *title)
{
    printf("\n========================================\n");
    printf("%s\n", title);
    printf("========================================\n");
}

/* 四个「操作 + 打印」小助手，demo 里反复用 */
static void push_back_and_show(Deque *d, int value)
{
    printf("\n  push_back(%d)   // 尾部进（和队列 enqueue 一样）\n", value);
    if (deque_push_back(d, value)) {
        deque_print(d, "现在：");
    } else {
        printf("    → 失败，见上面的 [失败]\n");
    }
}

static void push_front_and_show(Deque *d, int value)
{
    printf("\n  push_front(%d)  // 头部进 ← front 要【倒退】：(front-1+cap)%%cap\n", value);
    if (deque_push_front(d, value)) {
        deque_print(d, "现在：");
    } else {
        printf("    → 失败，见上面的 [失败]\n");
    }
}

static void pop_front_and_show(Deque *d)
{
    int v = 0;
    printf("\n  pop_front()\n");
    if (deque_pop_front(d, &v)) {
        printf("    从头端弹出 %d\n", v);
        deque_print(d, "现在：");
    } else {
        printf("    → 失败（空），见上面的 [失败]\n");
    }
}

static void pop_back_and_show(Deque *d)
{
    int v = 0;
    printf("\n  pop_back()    // 尾端出 ← 和栈 pop 一模一样\n");
    if (deque_pop_back(d, &v)) {
        printf("    从尾端弹出 %d\n", v);
        deque_print(d, "现在：");
    } else {
        printf("    → 失败（空），见上面的 [失败]\n");
    }
}

int main(void)
{
    /* ============================================================
       第 1 幕：四端都能进能出
       ============================================================ */
    scene("第 1 幕：四端操作 —— 头进、头出、尾进、尾出，全是 O(1)");

    Deque d;
    deque_init(&d, 6);
    deque_print(&d, "刚初始化（capacity=6）：");
    printf("    front=rear=size=0，两个指针重合 = 空\n");

    push_back_and_show(&d, 20);
    push_back_and_show(&d, 30);
    push_front_and_show(&d, 10);   /* front 从 0 倒退 → (0-1+6)%6 = 5，落到最后一格 */
    push_back_and_show(&d, 40);

    printf("\n  ★ 现在队里逻辑顺序是 10 → 20 → 30 → 40，但格子是【断成两段】的：\n");
    printf("    data[5]=10 是队头，data[0..2]=20 30 40 紧随其后（绕回开头接上的）。\n");
    printf("    这就是双端队列必须用循环数组的原因：两头都能加人，下标会两头跑。\n");

    printf("\n  两头各看一眼（不删）：\n");
    int h = 0, t = 0;
    deque_peek_front(&d, &h);
    deque_peek_back(&d, &t);
    printf("    队头 = %d（data[%d]），队尾 = %d（data[%d]）\n", h, d.front, t,
           (d.rear - 1 + d.capacity) % d.capacity);

    pop_front_and_show(&d);      /* 10 */
    pop_back_and_show(&d);       /* 40 ← 这一端是"栈口" */

    printf("\n  现在队里剩 20 → 30：两头各拿走一个之后，剩下的还是原来那个顺序。\n");

    /* ============================================================
       第 2 幕：本节最难的一行 —— 负数取模
       ============================================================ */
    scene("第 2 幕：⚠ 为什么倒退必须写 (front - 1 + capacity) % capacity");

    printf("  C 语言的 %% 是【向零截断】，不是数学同余，两个式子的结果完全不同：\n");
    printf("    ( 0 - 1 ) %% 6            = %d   ← 负数！拿它当数组下标就是 data[-1] 越界\n",
           (0 - 1) % 6);
    printf("    ( 0 - 1 + 6 ) %% 6        = %d   ← 正确：环上的最后一格\n", (0 - 1 + 6) % 6);
    printf("    ( 0 - 1 ) %% 6 == -1 ？  %s\n", ((0 - 1) % 6 == -1) ? "true（这就是坑）" : "false");
    printf("  记忆方法：凡是下标可能变负的减法，一律先 +capacity 再 %%capacity。\n");
    printf("  加法不用担心：(5 + 1) %% 6 = %d，正数取模永远是 0..capacity-1。\n", (5 + 1) % 6);

    printf("\n  连续从头部塞人，把环填满，看看 front 一路倒退的轨迹：\n");
    Deque f;
    deque_init(&f, 4);
    deque_print(&f, "刚初始化（capacity=4）：");
    push_front_and_show(&f, 100);   /* front 0 → 3 */
    push_front_and_show(&f, 200);   /* front 3 → 2 */
    push_front_and_show(&f, 300);   /* front 2 → 1 */
    push_front_and_show(&f, 400);   /* front 1 → 0，正好填满 */
    printf("\n  再塞一个 500 —— 这回是【真满】（size==capacity==4）：\n");
    push_front_and_show(&f, 500);
    printf("    注意装 4 个元素【一格都没浪费】：因为判满用 size，没有「少用一个格子」的约定。\n");

    /* ============================================================
       第 3 幕：同一个结构，既能当栈也能当队列
       ============================================================ */
    scene("第 3 幕：只用两个口子，双端队列就退化成了栈 / 队列");

    int nums[3] = {1, 2, 3};

    Deque as_stack;
    deque_init(&as_stack, 4);
    Deque as_queue;
    deque_init(&as_queue, 4);

    printf("  同样把 1、2、3 放进去（都从尾部进），区别只在【从哪一头取】：\n");
    for (int i = 0; i < 3; i++) {
        deque_push_back(&as_stack, nums[i]);
        deque_push_back(&as_queue, nums[i]);
    }
    deque_print(&as_stack, "两个队列内容一样：");

    printf("\n  取的时候：\n    ");
    printf("当栈用（push_back + pop_back，后进先出 LIFO）：");
    while (!deque_is_empty(&as_stack)) {
        int v = 0;
        deque_pop_back(&as_stack, &v);
        printf("%d ", v);
    }
    printf("\n    当队列用（push_back + pop_front，先进先出 FIFO）：");
    while (!deque_is_empty(&as_queue)) {
        int v = 0;
        deque_pop_front(&as_queue, &v);
        printf("%d ", v);
    }
    printf("\n");
    deque_usage_hint("一句话：栈和队列都是双端队列的「只开一个口」特例 —— "
                     "所以本节这一个实现，把前两章的两种结构都覆盖了。");

    /* ============================================================
       第 4 幕：pop_back / peek_back 的「先退再读」
       ============================================================ */
    scene("第 4 幕：pop_back 是「先退再读」，pop_front 是「先读再退」（顺序别记反）");

    Deque m;
    deque_init(&m, 4);
    deque_push_back(&m, 7);
    deque_push_back(&m, 8);
    deque_print(&m, "装好 7、8：");
    printf("    front=%d 指着 7（真实元素），rear=%d 指着【空位】（下一个 push_back 的位置）\n",
           m.front, m.rear);
    printf("    → 所以 pop_front 先读 data[front] 再挪；pop_back 先把 rear 退一格再读。\n");

    pop_back_and_show(&m);        /* 8 */
    pop_back_and_show(&m);        /* 7 */
    printf("\n  现在 front=%d、rear=%d（重合，空）。再 push_back 两个人，让 rear 走到 0 之后：\n",
           m.front, m.rear);
    deque_push_back(&m, 9);
    push_back_and_show(&m, 10);
    push_back_and_show(&m, 11);
    push_back_and_show(&m, 12);   /* size 到 4，满了 */
    printf("\n  现在 rear 绕回 0。此时 pop_back 要把 rear 从 0 【倒退】→ 又用到 +capacity：\n");
    pop_back_and_show(&m);
    printf("    ★ 这一行就是两个知识点撞在一起：队尾元素在 data[3]，"
           "而 rear 站在 0；不倒退就取不到，倒退不加 capacity 就越界。\n");

    /* ============================================================
       第 5 幕：真实应用 —— 滑动窗口最大值（单调队列）
       ============================================================ */
    scene("第 5 幕：应用：滑动窗口最大值（双端队列 + 一条「保持递减」的纪律）");

    /*
     * 题目：给一排高度，宽度为 k 的窗口从左往右滑，每一步报告窗口里的最大值。
     * 暴力做：每个窗口扫一遍 = O(n·k)。
     * 单调队列：队里【存下标】，并且始终保证「从队头到队尾对应的值递减」。
     *   ① 尾端淘汰：新来的人比队尾高，队尾那些"矮又新"的永远不可能当最大值 → 从尾部 pop_back 掉
     *      （这一步用的正是 pop_back —— 栈口）
     *   ② 头端过期：窗口往前滑，队头那个下标掉出窗口了 → pop_front
     *   ③ 新下标 push_back 进队
     *   ④ 队头 deque_peek_front 就是当前窗口最大值的下标
     * 每个下标最多进队一次、出队一次 → 总体 O(n)，和单调栈一个道理。
     */
    int heights[7] = {9, 5, 3, 7, 2, 8, 1};
    int n = 7;
    int k = 3;

    Deque w;
    deque_init(&w, n);      /* 队里存下标，最多 n 个，一次到位不扩容 */

    printf("  高度数组：");
    for (int i = 0; i < n; i++) {
        printf("%d ", heights[i]);
    }
    printf("   窗口宽度 k=%d\n  下面逐格滑动，队里放的是【下标】（括号里是对应高度）：\n", k);

    for (int i = 0; i < n; i++) {
        printf("\n  ── i=%d，新元素 heights[%d]=%d ──\n", i, i, heights[i]);

        /* ① 尾端淘汰：把"比新元素矮且比它新"的都请出去 */
        int dropped = 0;
        while (!deque_is_empty(&w)) {
            int tail_idx = 0;
            deque_peek_back(&w, &tail_idx);
            if (heights[tail_idx] <= heights[i]) {
                int gone = 0;
                deque_pop_back(&w, &gone);
                printf("    ① pop_back 下标 %d（高度 %d ≤ 新来的 %d，它这辈子当不了最大值了）\n",
                       gone, heights[gone], heights[i]);
                dropped++;
            } else {
                break;
            }
        }
        if (dropped == 0) {
            printf("    ① 尾端不用淘汰（新元素比队尾还矮，留着它有用）\n");
        }

        /* ③ 新下标进队（② 放在进队后判断更直观：队头过期就从头端踢） */
        deque_push_back(&w, i);
        printf("    ③ push_back 下标 %d\n", i);

        /* ② 头端过期：下标 <= i-k 的已经不在窗口 [i-k+1, i] 里了 */
        int head_idx = 0;
        deque_peek_front(&w, &head_idx);
        if (head_idx <= i - k) {
            int gone = 0;
            deque_pop_front(&w, &gone);
            printf("    ② pop_front 下标 %d：它已经滑出窗口 [%d, %d]\n",
                   gone, i - k + 1, i);
        } else {
            printf("    ② 队头下标 %d 还在窗口 [%d, %d] 内，不踢\n",
                   head_idx, i - k + 1, i);
        }

        /* 打印此刻队里有什么（队里存的是【下标】，所以要把环上的格子读出来再查高度）*/
        printf("    队里（头→尾，高度递减）：");
        for (int p = 0; p < deque_size(&w); p++) {
            int cell = (w.front + p) % w.capacity;      /* 环上的第 p 个格子 */
            int idx  = w.data[cell];                     /* 格子里存的下标 */
            printf("[%d:%d] ", idx, heights[idx]);
        }

        if (i + 1 >= k) {
            int best_idx = 0;
            deque_peek_front(&w, &best_idx);
            printf(" → 窗口最大值 = heights[%d] = %d\n", best_idx, heights[best_idx]);
        } else {
            printf(" → 窗口还没凑满 %d 个，不输出\n", k);
        }
    }

    printf("\n  ★ 为什么是 O(n)：每个下标最多【进队一次、出队一次】，"
           "所以 while 循环的总执行次数 ≤ n，不是一次都不算。\n");
    printf("    这就是 03-stack/monotonic-stack 里同样的「摊还」论证 —— "
           "只不过那里只能从栈顶弹，这里两头都能弹，所以能顺便踢掉过期的队头。\n");

    /* ============================================================
       第 6 幕：边界情况
       ============================================================ */
    scene("第 6 幕：边界情况都安全（空队四端取、真满两端塞、非法 init）");

    printf("  空队列上四种取法都只报错、不崩：\n");
    Deque e;
    deque_init(&e, 3);
    deque_pop_front(&e, NULL);
    deque_pop_back(&e, NULL);
    int tmp = 0;
    if (!deque_peek_front(&e, &tmp)) {
        printf("    peek_front 失败，tmp 仍是 %d（没被写脏）\n", tmp);
    }
    deque_peek_back(&e, &tmp);

    printf("\n  填满之后两端都塞不进（判的都是 size==capacity）：\n");
    Deque g;
    deque_init(&g, 3);
    deque_push_back(&g, 1);
    deque_push_back(&g, 2);
    deque_push_back(&g, 3);
    deque_print(&g, "满了：");
    deque_push_back(&g, 4);
    deque_push_front(&g, 0);

    printf("\n  非法初始化（capacity=0）：\n");
    Deque bad;
    deque_init(&bad, 0);
    deque_push_back(&bad, 1);
    deque_push_front(&bad, 1);
    deque_pop_back(&bad, NULL);

    /* ============================================================
       收尾：谁 malloc 谁 free
       ============================================================ */
    scene("收尾：五个队列全部 destroy");

    deque_destroy(&d);
    printf("  d    已 destroy：data=%p, size=%d\n", (void *)d.data, d.size);
    deque_destroy(&f);
    printf("  f    已 destroy\n");
    deque_destroy(&as_stack);
    printf("  as_stack 已 destroy（这一幕结束时它已经是空的了，但格子还得还回去）\n");
    deque_destroy(&as_queue);
    printf("  as_queue 已 destroy\n");
    deque_destroy(&m);
    printf("  m    已 destroy\n");
    deque_destroy(&w);
    printf("  w    已 destroy（单调队列那 8 个格子的数组）\n");
    deque_destroy(&e);
    printf("  e    已 destroy\n");
    deque_destroy(&g);
    printf("  g    已 destroy（还满着就被销毁：destroy 会整块 free，不用先清空）\n");
    deque_destroy(&bad);
    printf("  bad  已 destroy（本来就没申请内存，free(NULL) 是合法空操作）\n");

    printf("\n=== 本节结束 ===\n");
    printf("记住四句话：\n");
    printf("  1) 双端队列 = 循环数组 + front/rear 各自正反挪一格；四端全 O(1)，一格不浪费。\n");
    printf("  2) 倒退一律 (x - 1 + capacity) %% capacity：C 里 (0-1)%%6 = -1，直接当下标就越界。\n");
    printf("  3) pop_front 先读后挪（front 在真实元素上）；pop_back 先挪后读（rear 在空位上）。\n");
    printf("  4) 只用尾端两个口 = 栈；只用头进尾出 = 队列；两头配合 = 单调队列（滑动窗口 O(n)）。\n");

    return 0;
}
