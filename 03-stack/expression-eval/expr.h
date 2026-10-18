#ifndef EXPR_H
#define EXPR_H

/*
 * ===================================================================
 * expr.h —— 栈的两大经典应用（括号匹配 + 中缀转后缀并求值）
 * ===================================================================
 * 这一节不谈新的数据结构，只用栈解决两个真实问题，目的是让你明白：
 *   「后进先出」这个看起来很土的规矩，到底凭什么能算表达式。
 *
 * 应用①：括号匹配校验
 *   混合 ()[]{} 三种括号，判断一段文本里的括号是否合法配对。
 *   为什么用栈？因为「最近的一个还没闭合的开括号」必须最先被闭合 ——
 *   这正是后进先出。用队列就会把 ([)] 这种交叉错误判成合法。
 *
 * 应用②：中缀 → 后缀（调度场算法 Shunting-yard）+ 后缀求值
 *   中缀：人看的写法  (3+4)*5-6     ← 需要括号 + 优先级两套规则才能确定运算顺序
 *   后缀：机器好算的写法 34+5*6-    ← 从左往右扫，见到运算符就把前面两个数拿来计算，
 *                                        顺序天然被"运算符出现的位置"锁死，所以不需要括号
 *   「后缀为什么不需要括号」一句话解释：
 *     后缀表达式里每个运算符都排在【它所作用的两个数】之后，
 *     从左往右读的时候，任何时候栈里存的都是"还没被运算符用掉的数"，
 *     运算符一出现，它要的那两个操作数刚好就是栈顶两个 —— 不可能有歧义。
 *     括号在中缀里干的活就是"人为制造这个先后关系"，后缀已经把先后关系写进串本身了，
 *     所以括号自然消失。
 *
 * 本节目的限制（刻意做小，好把机制看清楚）：
 *   · 操作数只支持【个位数】0-9（不做多位数解析，不写 atoi 那套字符攒数字的代码）
 *   · 运算符只有 + - * /（整数除法）
 *   · 括号只有 ( )，另外括号匹配那一半支持 ()[]{} 三种
 *   · 空格会被跳过
 *
 * ⚠ 本目录完全自包含：下面两个小栈（字符栈、int 栈）都是就地写的，
 *   没有 #include 兄弟目录的 astack.h / lstack.h —— Makefile 按目录独立编译，
 *   跨目录 include 编不过。栈在这里只是三五十行的辅助小结构，复制一份最省事。
 */

#include <stdbool.h>

/*
 * CharStack —— 极小字符栈（括号匹配 & 运算符栈都用它）
 *   chars[]     ：存的字符
 *   positions[] ：和 chars 平行的「它在原串里的第几个字符」，报错时才能指出位置
 *   top         ：栈顶元素下标，空栈 = -1（和 array-stack 的流派 A 一致）
 */
typedef struct {
    char *chars;
    int  *positions;
    int   capacity;
    int   top;
} CharStack;

void cst_init(CharStack *s, int capacity);
void cst_destroy(CharStack *s);
bool cst_push(CharStack *s, int position, char ch);
bool cst_pop(CharStack *s, char *out_ch, int *out_position);   /* 两个输出参数都可以传 NULL */
bool cst_peek(const CharStack *s, char *out_ch, int *out_position);
bool cst_is_empty(const CharStack *s);
void cst_print(const CharStack *s, const char *label);         /* 教学打印：栈里现在有哪些括号/运算符 */

/*
 * IntStack —— 极小 int 栈（后缀求值用，栈里放「还没被运算符用掉的数」）
 */
typedef struct {
    int *data;
    int  capacity;
    int  top;
} IntStack;

void ist_init(IntStack *s, int capacity);
void ist_destroy(IntStack *s);
bool ist_push(IntStack *s, int value);
bool ist_pop(IntStack *s, int *out_value);
bool ist_is_empty(const IntStack *s);
void ist_print(const IntStack *s, const char *label);

/* ---------- 应用①：括号匹配 ---------- */

/*
 * 检查 expr 里的括号是否合法配对（支持 ()[]{} 三种混用）。
 * 返回 true = 匹配；false = 不匹配。
 * 无论成功失败，都会：
 *   · 逐个字符打印扫描过程 + 当前栈内容
 *   · 最后打印结论和【不匹配的具体原因】（多余的闭括号 / 类型不对 / 没闭合 / 非法字符）
 */
bool expr_check_brackets(const char *expr);

/* ---------- 应用②：中缀转后缀 + 后缀求值 ---------- */

/*
 * 调度场算法：中缀 → 后缀。
 * 返回【调用者负责 free】的字符串（malloc 出来的）；失败返回 NULL，原因会打印出来。
 * ok_out 如果不为 NULL，会把成功/失败写进去（C 语言只有一条 return 通道时的老手法，
 * 现在 C23 有 [[nodiscard]]，但我们只用 C11 的标准写法）。
 *
 * 每一步都打印：当前读到的字符、做了什么决策、输出串长到现在是什么、运算符栈里是什么。
 */
char *expr_infix_to_postfix(const char *expr, bool *ok_out);

/*
 * 后缀表达式求值（整数除法；除数为 0 会明确报错并返回 false）。
 * 每读一个字符都打印：做了什么、数字栈变化、如果是运算符就把这次计算的两个操作数和结果写出来。
 */
bool expr_eval_postfix(const char *postfix, int *result_out);

/*
 * 一步到位：中缀 → 后缀 → 求值。
 * 内部会把 expr_infix_to_postfix 申请的字符串 free 掉（谁 malloc 谁 free，这里 malloc 的是它，
 * 所以 free 也必须在它作用域里做完，否则就是泄漏）。
 */
bool expr_eval_infix(const char *expr, int *result_out);

#endif /* EXPR_H */
