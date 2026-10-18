#ifndef DYNAMIC_MEMORY_H
#define DYNAMIC_MEMORY_H

/*
 * ===================================================================
 * 本节是"内存四件套"的实弹演习：malloc / calloc / realloc / free。
 * 上一节我们学会了"盒子长什么样"（结构体 + 自引用），
 * 这一节学"盒子从哪儿来、怎么换大的、怎么还回去"。
 *
 * 三个必须先记住的结论（后面每一章都会用到）：
 *   1) 堆内存不会自己消失。谁 malloc，谁负责 free；不 free 就是泄漏。
 *   2) realloc 可能原地扩大，也可能整块搬到别处 —— 所以【必须接收返回值】。
 *   3) 函数想改调用者的指针，参数就得是指针的指针（Node **）。
 *      链表/树的插入删除接口全是 Node **，根子就在这一条。
 * ===================================================================
 */

/*
 * 链表节点：上一节的雏形，这一节开始用它练 malloc。
 * 每个节点单独 malloc 一块（16 字节），它们可以散落在堆的任何角落。
 */
typedef struct Node {
    int val;
    struct Node *next;
} Node;

/* ---------- 1. 三种"要一块内存"的方式，逐一对比 ---------- */
int *arr_new_malloc(int n, int fill_value);  /* 要多少字节，内容随机，必须自己填 */
int *arr_new_calloc(int n);                  /* 两个参数(个数,每个多大)，自动全 0 */
int *arr_new_malloc_memset(int n);           /* malloc + memset 手工清零，效果和 calloc 一样 */

/* 用来演示"malloc 不清零"：申请→写脏→故意归还，让下一次 malloc 极大概率捡到同一块脏内存 */
void dirty_block_setup(int n, int dirty_value);
int *arr_new_malloc_untouched(int n);           /* 只 malloc 不写，让 main 打印出"垃圾值" */

/* ---------- 2. realloc：换一块更大的（可能搬家） ---------- */
/*
 * 返回新地址；失败时返回原地址（原内存依然有效，长度不变）。
 * ⚠ 调用者必须写成 p = array_resize(p, n); 绝不能把返回值丢掉。
 */
int *array_resize(int *arr, int new_n);

/* ---------- 3. 二级指针：正反例两幕（风格照抄 pointer-basics 的 swap） ---------- */
void push_front_broken(Node *head, int val); /* 反面教材：改的是头指针的拷贝 */
void push_front_ok(Node **head, int val);    /* 正确写法：改到调用者的头指针 */
int  pop_front_ok(Node **head, int *value_out); /* 弹头：能把头指针改成 NULL，返回 0 成功 / -1 空链 */

/* ---------- 4. 建链 / 打印 / 拆链 ---------- */
Node *build_list(int n, int start_value);
int   list_length(const Node *head);
void  print_list(const char *tag, const Node *head);
void  free_list(Node *head);

/* ---------- 5. 泄漏演示（默认不跑，命令行传参数才跑） ---------- */
void print_int_array(const char *tag, const int *arr, int n);
void leak_demo(void);   /* 故意漏掉两块内存：只有在 ASan 下、且传了命令行参数才会被报出来 */

#endif /* DYNAMIC_MEMORY_H */
