#ifndef STRUCT_AND_TYPEDEF_H
#define STRUCT_AND_TYPEDEF_H

/*
 * ===================================================================
 * 这个头文件是"菜单"：只声明，不实现。
 * 真正的代码在 struct-and-typedef.c 里。
 *
 * 本节解决的问题：
 *   链表节点、树节点、图的边……本质都是"一小块内存里装好几个字段，
 *   其中某个字段还装着'下一个节点的地址'"。
 *   把这句话拆开就是三个知识点：结构体 + 指针 + 自引用。
 *   本节把这三个零件分别练熟，下一节（dynamic-memory）和 02-linked-list 就直接能用。
 * ===================================================================
 */

/*
 * ---------- 1. 同一个结构体的两种写法（用来对比 typedef 省了什么） ----------
 *
 * 写法 A：不带别名。以后每次用它都必须写完整的 `struct TagPoint`。
 * 写法 B：用 typedef 起了个外号 Point。之后 `Point` 就是一个完整的类型名，
 *         前面不需要再加 struct 关键字。
 *
 * 为什么要省这个字？三个理由，后面 `.c` 里会一条条演示：
 *   1) 少打字，而且改类型时只改一处；
 *   2) `typedef` 能让"指针"写法读起来更顺：Point *p 比 struct TagPoint *p 清爽；
 *   3) 结构体数组、二级指针的声明会短一大截。
 */
struct TagPoint { int x; int y; };
typedef struct { int x; int y; } Point;

/*
 * ---------- 2. 故意做的一个"小结构体"，用来实测编译器补齐 ----------
 * char 占 1 字节，int 占 4 字节，加起来 5 字节。
 * 但 sizeof 出来的结果不是 5（本机是 8）。为什么？看 .c 里的内存图。
 */
typedef struct { char c; int n; } CharInt;

/*
 * ---------- 3. 结构体套结构体（嵌套） ----------
 * Rect 里面没有 int，只有两个 Point —— 这就是"把复杂数据一层层搭出来"。
 * Window 再往外套一层，用来演示访问要写成 w.frame.top_left.x 这种三级的路。
 */
typedef struct { Point top_left; Point bottom_right; } Rect;
typedef struct { Rect frame; int id; int layer; } Window;

/*
 * ---------- 4. 自引用结构：整节的重点，也是下一章的地基 ----------
 *
 * typedef struct Node { int val; struct Node *next; } Node;
 *         ↑                                         ↑     ↑
 *         结构体标签(tag)                          结构体标签  typedef 起的别名
 *
 * 为什么必须写成 `struct Node *next`，不能写 `Node *next`？
 *   因为执行到 `next` 这一行的时候，别名 Node 还没生效（typedef 要等整个
 *   结构体定义完才有），编译器不认识 Node 这个名字。
 *   但 `struct Node` 这个标签已经在上花括号那一刻被"挂上号"了，可以拿来用。
 *   而且只能用指针：如果写 `struct Node next;`，那 Node 里又装一个 Node，
 *   里面又装一个 Node……无限套娃，大小算不出来，编译器直接报错。
 *   指针的大小永远是 8 字节（64 位机），跟它指向谁无关，所以能算出来。
 *
 * 结论：这一行就是链表节点的全部秘密 —— 一个数据 + 一张写着"下一个节点地址"的纸条。
 */
typedef struct Node {
    int val;
    struct Node *next;
} Node;

/* ---------- 5. 打印：让内存里的东西看得见 ---------- */
void print_point(const char *tag, Point p);      /* 只读，按值传就够了 */
void print_point_array(const Point *pts, int n);
void print_rect(const char *tag, const Rect *r);
void print_node_chain(const Node *head);

/* ---------- 6. 改结构体：正反例对照（风格和第 1 节的 swap 一模一样） ---------- */
void move_point_by_value(Point p, int dx, int dy);      /* 失败：改的是拷贝 */
void move_point_by_pointer(Point *p, int dx, int dy);   /* 成功：直接改原件 */

/* ---------- 7. 大结构体：按值传 vs 按指针传的成本对比 ---------- */
long long rect_area_by_value(Rect r);        /* 每次调用整块拷贝 16 字节 */
long long rect_area_by_pointer(const Rect *r); /* 每次调用只拷贝 8 字节（一个地址） */

/* ---------- 8. 结构体之间赋值 == 整块字段拷贝 ---------- */
void swap_points(Point *a, Point *b);        /* 想交换原件就得传指针 */

/* ---------- 9. 结构体数组：一行一个点的批量初始化 ---------- */
void init_point_array(Point pts[], int n);   /* 第 i 个点填成 (i, 2*i) */

/* ---------- 10. 自引用结构：手工搭一条链，再量长度，再拆掉 ---------- */
Node *build_node_chain(int count, int start_value); /* 调用者负责 node_chain_free */
int   node_chain_length(const Node *head);
void  node_chain_free(Node *head);

/* ---------- 11. 实测 sizeof（补齐规则）。用 size_t 是因为 sizeof 天生就是它 ---------- */
size_t size_of_char_int(void);
size_t size_of_point(void);
size_t size_of_rect(void);
size_t size_of_node(void);

#endif /* STRUCT_AND_TYPEDEF_H */
