#include <stdio.h>
#include <stdlib.h>   /* malloc / free */
#include <string.h>   /* strlen */
#include "expr.h"

/*
 * ===================================================================
 * expr.c —— 括号匹配 + 中缀转后缀 + 后缀求值
 * ===================================================================
 * 两张图先把栈的形状记住。
 *
 * 【图一：括号匹配】扫描 "([)]" —— 这就是为什么必须用栈，不能用队列
 *
 *   读到 '(' → 入栈       栈: (
 *   读到 '[' → 入栈       栈: ( [        ← 栈顶是「最近还没闭合的开括号」
 *   读到 ')' → 看栈顶是 '['，不是 '(' → ❌ 类型不符，交叉括号，直接判不匹配
 *
 *   如果用队列（先进先出），第一个要配对的会是 '('，')' 正好能配上 '('，
 *   于是 "([)]" 会被误判成合法 —— 而它在数学上根本是错的写法。
 *   ★「最近的一个最先被闭合」= 后进先出 = 栈。这一句就是本节的全部动机。
 *
 * 【图二：调度场算法】"(3+4)*5-6" → "34+5*6-"
 *
 *   读到的字符 │ 输出串(后缀) │ 运算符栈 │ 干了什么
 *   ---------┼-------------┼---------┼--------------------------------
 *      (     │             │ (       │ 左括号直接进栈，它是"分隔线"
 *      3     │ 3           │ (       │ 数字直接进输出
 *      +     │ 3           │ ( +     │ 栈顶是 '(' → 不弹，压入
 *      4     │ 34          │ ( +     │
 *      )     │ 34+         │         │ 一路弹到 '(' 为止：弹出 '+' 写进输出，'(' 丢弃
 *      *     │ 34+         │ *       │ 栈空 → 压入
 *      5     │ 34+5        │ *       │
 *      -     │ 34+5*       │ -       │ 栈顶 '*' 优先级(2) >= '-'(1) → 弹出 '*'，再压 '-'
 *      6     │ 34+5*6      │ -       │
 *    结束     │ 34+5*6-     │ (空)    │ 把栈里剩下的运算符全倒进输出
 *
 *   后缀 "34+5*6-" 求值（栈里放"还没被运算符用掉的数"）：
 *     '3' 入栈 → 栈: 3
 *     '4' 入栈 → 栈: 3 4
 *     '+' 先弹出 4（右），再弹出 3（左）→ 3+4=7 → 栈: 7      ★先弹的是右操作数，顺序别反
 *     '5' 入栈 → 栈: 7 5
 *     '*' 弹 5、弹 7 → 7*5=35 → 栈: 35
 *     '6' 入栈 → 栈: 35 6
 *     '-' 弹 6、弹 35 → 35-6=29 → 栈: 29  → 最终答案 29
 *
 *   对照中缀 (3+4)*5-6 = 7*5-6 = 35-6 = 29 ✔
 *
 *   ★ 后缀为什么不需要括号？
 *     中缀要靠「括号 + 优先级」两套外部规则才能确定"谁先算"；
 *     后缀把"谁先算"直接写进了串里：每个运算符都排在它自己的两个操作数【之后】，
 *     从左往右一遍扫下来，见到运算符就拿栈顶两个数算，算完压回去 —— 没有任何选择余地，
 *     也就没有歧义。括号在中缀里干的活，后缀用"位置"就干完了，所以括号自然消失。
 * ===================================================================
 */

/* ===================================================================
   第一部分：CharStack —— 本目录自带的小字符栈
   同时存「字符」和「它在原串里的位置」：位置用来报错时指出是第几个字符。
   =================================================================== */

/*
 * 初始化：两次 malloc（字符数组 + 位置数组）。
 * ⚠ 边界情况：如果第二次 malloc 失败，必须把第一次的 free 掉再退出 ——
 *   不然 chars 那块内存永远找不回来（泄漏）。这种"申请了两样、第二样失败"的清理
 *   是 C 里最容易漏的一类，写完就顺手检查一遍。
 */
void cst_init(CharStack *s, int capacity)
{
    s->chars = NULL;
    s->positions = NULL;
    s->capacity = 0;
    s->top = -1;                       /* 流派 A：空栈 top = -1 */

    if (capacity <= 0) {
        printf("      [警告] cst_init 容量非法(%d)，建成空壳\n", capacity);
        return;
    }

    s->chars = malloc((size_t)capacity * sizeof(char));
    if (s->chars == NULL) {
        printf("      [错误] malloc 字符栈失败（capacity=%d）\n", capacity);
        return;
    }
    s->positions = malloc((size_t)capacity * sizeof(int));
    if (s->positions == NULL) {
        printf("      [错误] malloc 位置栈失败（capacity=%d）\n", capacity);
        free(s->chars);                /* ★ 把已经到手的那块还回去，避免泄漏 */
        s->chars = NULL;
        return;
    }
    s->capacity = capacity;
}

void cst_destroy(CharStack *s)
{
    if (s == NULL) {
        return;
    }
    free(s->chars);                    /* free(NULL) 合法，所以 init 失败也能安全 destroy */
    free(s->positions);
    s->chars = NULL;
    s->positions = NULL;
    s->capacity = 0;
    s->top = -1;
}

bool cst_push(CharStack *s, int position, char ch)
{
    if (s->chars == NULL || s->top + 1 >= s->capacity) {
        printf("      [失败] 字符栈满/未初始化（capacity=%d, top=%d）\n", s->capacity, s->top);
        return false;
    }
    s->top += 1;                       /* 先挪指针 */
    s->chars[s->top] = ch;             /* 再写内容 —— 两个数组是【平行】的，下标必须同步 */
    s->positions[s->top] = position;
    return true;
}

bool cst_pop(CharStack *s, char *out_ch, int *out_position)
{
    if (s->top < 0) {
        return false;                  /* 空栈：什么都不写 */
    }
    char ch = s->chars[s->top];
    int position = s->positions[s->top];
    s->top -= 1;
    if (out_ch != NULL) {
        *out_ch = ch;
    }
    if (out_position != NULL) {
        *out_position = position;
    }
    return true;
}

bool cst_peek(const CharStack *s, char *out_ch, int *out_position)
{
    if (s == NULL || s->top < 0 || s->chars == NULL) {
        return false;
    }
    if (out_ch != NULL) {
        *out_ch = s->chars[s->top];
    }
    if (out_position != NULL) {
        *out_position = s->positions[s->top];
    }
    return true;
}

bool cst_is_empty(const CharStack *s)
{
    return s == NULL || s->top < 0;
}

/* 教学打印：把栈里现在的字符连同位置一起摊开 */
void cst_print(const CharStack *s, const char *label)
{
    printf("        %s  size=%d：栈(底→顶) ", label, (s->top + 1 > 0 ? s->top + 1 : 0));
    if (cst_is_empty(s)) {
        printf("(空)\n");
        return;
    }
    for (int i = 0; i <= s->top; i++) {
        printf("[%d:'%c'] ", s->positions[i], s->chars[i]);   /* [原串位置:'字符'] */
    }
    printf("\n");
}

/* ---------- 括号判断的三个小工具：写成函数是因为 main/校验里都要用 ---------- */

static bool is_open_bracket(char ch)
{
    return ch == '(' || ch == '[' || ch == '{';
}

static bool is_close_bracket(char ch)
{
    return ch == ')' || ch == ']' || ch == '}';
}

/* 配对关系表：闭括号 ch 应该配哪个开括号；不认的字符返回 0 */
static char matching_open(char close_ch)
{
    switch (close_ch) {
    case ')': return '(';
    case ']': return '[';
    case '}': return '{';
    default:  return 0;   /* 不是闭括号 */
    }
}

/* ===================================================================
   第二部分：应用① 括号匹配校验
   ===================================================================
   一步步：
     ① 从左到右扫每个字符
     ② 开括号 → 压栈（连同它的位置一起压，方便报错说"第几个字符没闭合"）
     ③ 闭括号 → 栈空？就是「多余的闭括号」；不空但栈顶不是它该配的那个？就是「类型不符 / 交叉」；
                配上了就弹栈，并打印"第 a 个 X 与第 b 个 Y 配对"
     ④ 其它字符 → 直接忽略（不进栈！栈里只放"还没闭合的开括号"）
     ⑤ 扫完后栈里还有剩 → 「没闭合」，把剩下的位置和字符都报出来
   边界情况：空串算合法（没有任何括号 = 没有任何错误）；
             整串没有任何括号也算合法；
             一个闭括号在开头出现 → 立刻失败在③的"栈空"分支。
   =================================================================== */
bool expr_check_brackets(const char *expr)
{
    if (expr == NULL) {
        printf("  [错误] 传入的是空指针\n");
        return false;
    }

    int len = (int)strlen(expr);
    printf("  待检查（长度 %d）: \"%s\"\n", len, expr);

    /* 栈里最多同时压 len 个开括号，所以容量给 len 绝对够（len 为 0 时给 1 避免 0 容量）*/
    CharStack st;
    cst_init(&st, (len > 0) ? len : 1);
    if (st.chars == NULL) {
        printf("  [错误] 内存申请失败，跳过本次检查\n");
        return false;
    }

    bool ok = true;
    const char *reason = "";           /* 不匹配时的原因，最后打印出来 */
    int bad_pos = -1;

    for (int i = 0; i < len; i++) {
        char ch = expr[i];

        if (is_open_bracket(ch)) {
            cst_push(&st, i, ch);
            printf("    第 %d 个字符 '%c' 是【开括号】→ 入栈，它正在等着被闭合\n", i, ch);
            cst_print(&st, "入栈后");
        } else if (is_close_bracket(ch)) {
            printf("    第 %d 个字符 '%c' 是【闭括号】→ 该找「最近一个还没闭合的开括号」配对了\n", i, ch);

            if (cst_is_empty(&st)) {
                ok = false;
                bad_pos = i;
                reason = "闭括号出现时栈里没有任何待闭合的开括号 → 多余的闭括号";
                cst_print(&st, "此时");
                printf("      ❌ 就在这里失败了\n");
                break;
            }

            char top_ch = 0;
            int top_pos = 0;
            cst_peek(&st, &top_ch, &top_pos);      /* 只看，先不弹，因为可能配对失败 */

            if (top_ch != matching_open(ch)) {
                ok = false;
                bad_pos = i;
                reason = "最近的开括号类型和这个闭括号不配对 → 括号交叉/写错了";
                printf("      栈顶是第 %d 个字符 '%c'，而 '%c' 应该配 '%c' → 不匹配\n",
                       top_pos, top_ch, ch, matching_open(ch));
                cst_print(&st, "此时");
                printf("      ❌ 就在这里失败了\n");
                break;
            }

            cst_pop(&st, NULL, NULL);              /* 只要"丢掉"，值我不用 → 两个输出参数都传 NULL */
            printf("      ✔ 配对成功：第 %d 个 '%c' 与第 %d 个 '%c' 闭合，弹出栈顶\n", top_pos, top_ch, i, ch);
            cst_print(&st, "弹出后");
        } else {
            printf("    第 %d 个字符 '%c' 是普通字符 → 跳过，不进栈（栈里只放待闭合的开括号）\n", i, ch);
        }
    }

    /* ⑤ 收尾检查：前面都对了，但可能有开括号一直没闭上 */
    if (ok && !cst_is_empty(&st)) {
        ok = false;
        reason = "扫描结束时栈里还有开括号没被闭合 → 括号不完整";
        printf("    扫描结束，但栈里还剩 %d 个开括号没闭合：", st.top + 1);
        for (int i = 0; i <= st.top; i++) {
            printf("第 %d 个字符 '%c'、", st.positions[i], st.chars[i]);
        }
        printf("\n");
    }

    printf("  结论：");
    if (ok) {
        printf("✅ 括号匹配合法（串里的每个开括号都被最近的正确闭括号关掉了）\n");
    } else {
        printf("❌ 括号不匹配 —— %s（出问题的位置：第 %d 个字符）\n", reason, bad_pos);
    }

    cst_destroy(&st);                  /* 两次 malloc，两次 free */
    return ok;
}

/* ===================================================================
   第三部分：IntStack —— 本目录自带的小 int 栈（后缀求值用）
   =================================================================== */

void ist_init(IntStack *s, int capacity)
{
    s->data = NULL;
    s->capacity = 0;
    s->top = -1;
    if (capacity <= 0) {
        capacity = 1;                  /* 至少给 1 格，免得后面到处判空 */
    }
    s->data = malloc((size_t)capacity * sizeof(int));
    if (s->data == NULL) {
        printf("      [错误] malloc int 栈失败（capacity=%d）\n", capacity);
        return;
    }
    s->capacity = capacity;
}

void ist_destroy(IntStack *s)
{
    if (s == NULL) {
        return;
    }
    free(s->data);
    s->data = NULL;
    s->capacity = 0;
    s->top = -1;
}

bool ist_push(IntStack *s, int value)
{
    if (s->data == NULL || s->top + 1 >= s->capacity) {
        printf("      [失败] int 栈满/未初始化\n");
        return false;
    }
    s->top += 1;
    s->data[s->top] = value;
    return true;
}

bool ist_pop(IntStack *s, int *out_value)
{
    if (s->top < 0) {
        return false;
    }
    int v = s->data[s->top];
    s->top -= 1;
    if (out_value != NULL) {
        *out_value = v;
    }
    return true;
}

bool ist_is_empty(const IntStack *s)
{
    return s == NULL || s->top < 0;
}

void ist_print(const IntStack *s, const char *label)
{
    printf("        %s  size=%d：数栈(底→顶) ", label, (s->top >= 0 ? s->top + 1 : 0));
    if (ist_is_empty(s)) {
        printf("(空)\n");
        return;
    }
    for (int i = 0; i <= s->top; i++) {
        printf("%d ", s->data[i]);
    }
    printf("\n");
}

/* ---------- 运算符优先级与结合性 ---------- */

/*
 * 优先级数字：越大越先算。'(' 单独给 0，因为它靠"分隔线"规则处理，不参与比较。
 *   + -  => 1
 *   * /  => 2
 *    0   => 不是运算符
 */
static int precedence(char op)
{
    switch (op) {
    case '+': case '-': return 1;
    case '*': case '/': return 2;
    case '(': case ')': return 0;
    default:            return -1;   /* 非法字符 */
    }
}

/*
 * 把「当前输出串 + 运算符栈」打成一行。
 * 抽成一个函数是因为转换过程每个字符都要打一次这两样东西，
 * 写在每个分支里会变成 7 份几乎一样的 printf —— 一改就漏改，教学代码最忌讳这个。
 */
static void print_conversion_state(const char *out, const CharStack *ops)
{
    printf("            输出串=\"%s\"   运算符栈(底→顶)=", out);
    if (cst_is_empty(ops)) {
        printf("(空)");
    } else {
        for (int k = 0; k <= ops->top; k++) {
            printf("%c ", ops->chars[k]);
        }
    }
    printf("\n");
}

/*
 * 调度场算法的内部实现（verbose 决定打不打印过程）。
 *
 * 核心规则只有三条：
 *   ① 数字：直接写进输出串（后缀里的操作数顺序就是它出现的顺序）
 *   ② 运算符：把栈里【优先级 >= 自己】的运算符先弹光（它们要更先算），再压自己
 *   ③ 右括号：把栈一路弹到左括号为止，左括号本身丢弃（它只是分隔线，不进输出）
 *
 * 为什么②里比较用 `>=`（而不是 `>`）？
 *   因为 + - * / 全是【左结合】：`8-3-2` 要算成 `(8-3)-2`。
 *   遇到第二个 '-' 时，栈顶的 '-' 优先级【相等】，必须先把前者输出，
 *   后缀才会变成 "83-2-"（读作 (8-3)-2）；若只在"更大"时才弹，会得到 "832--"（读作 8-(3-2)），
 *   结果完全不同 —— 这就是 `>=` 和 `>` 的区别，一个字符决定结合性。
 * 为什么遇到 '(' 就停？
 *   '(' 在栈里是一堵墙：墙外的运算符绝不能比墙里的先输出，
 *   所以弹栈必须停在 '('，而 '(' 本身最后被丢弃、不进输出。
 */
static char *to_postfix_impl(const char *expr, bool *ok_out, bool verbose)
{
    if (expr == NULL) {
        if (verbose) {
            printf("  [错误] 传入的是空指针\n");
        }
        if (ok_out != NULL) {
            *ok_out = false;
        }
        return NULL;
    }

    int len = (int)strlen(expr);
    if (len == 0) {
        printf("  [错误] 表达式是空串\n");
        if (ok_out != NULL) {
            *ok_out = false;
        }
        return NULL;
    }

    /* 输出串最多和输入一样长（括号会被丢掉，所以一定 <= len）；多留 1 格给结尾 '\0' */
    char *out = malloc((size_t)len + 1);
    if (out == NULL) {
        printf("  [错误] malloc 后缀串失败（要 %d 字节）\n", len + 1);
        if (ok_out != NULL) {
            *ok_out = false;
        }
        return NULL;
    }

    CharStack ops;
    cst_init(&ops, len);               /* 运算符栈最多同时压 len 个 */
    if (ops.chars == NULL) {
        free(out);                     /* ★ 已经拿到手的 out 必须还掉，否则这块就泄漏了 */
        if (ok_out != NULL) {
            *ok_out = false;
        }
        return NULL;
    }

    int out_len = 0;
    out[0] = '\0';

    if (verbose) {
        printf("  中缀: \"%s\"   （限制：只有个位数 0-9、+ - * /、( )，空格忽略）\n", expr);
        printf("  规则：① 数字 → 直接进输出  ② 运算符 → 先弹光栈里优先级 >= 自己的，再压自己  "
               "③ ')' → 一路弹到 '(' 为止\n\n");
    }

    for (int i = 0; i < len; i++) {
        char ch = expr[i];

        if (ch == ' ' || ch == '\t') {
            if (verbose) {
                printf("    第 %d 个字符是空白 → 忽略，什么都不做\n", i);
            }
            continue;
        }

        if (ch >= '0' && ch <= '9') {
            /* 规则①：操作数直接进输出 */
            out[out_len++] = ch;
            out[out_len] = '\0';      /* 每次补上结尾 0，纯粹是为了能直接用 "%s" 打印出来给你看 */
            if (verbose) {
                printf("    第 %d 个字符 '%c'：数字 → 规则①，直接写进输出串\n", i, ch);
                print_conversion_state(out, &ops);
            }
            continue;
        }

        if (ch == '(') {
            cst_push(&ops, i, ch);
            if (verbose) {
                printf("    第 %d 个字符 '('：左括号 = 一堵墙 → 直接压栈当分隔线（墙外的运算符"
                       "绝不能比墙里的先输出）\n", i);
                print_conversion_state(out, &ops);
            }
            continue;
        }

        if (ch == ')') {
            /* 规则③：一路弹到 '(' 为止，'(' 自己丢弃 */
            bool found_open = false;
            if (verbose) {
                printf("    第 %d 个字符 ')'：右括号 → 规则③，把墙里的运算符全倒进输出\n", i);
            }
            while (!cst_is_empty(&ops)) {
                char top_ch = 0;
                int top_pos = 0;
                cst_peek(&ops, &top_ch, &top_pos);

                if (top_ch == '(') {
                    cst_pop(&ops, NULL, NULL);      /* 丢弃左括号，它不进输出串 */
                    found_open = true;
                    if (verbose) {
                        printf("        看到第 %d 个字符 '('：它就是这堵墙的墙头，弹掉丢弃，不再输出\n",
                               top_pos);
                    }
                    break;
                }
                /* 用临时变量接住，再写进 out，顺便打印"这一步弹了谁" */
                char popped = 0;
                int popped_pos = 0;
                cst_pop(&ops, &popped, &popped_pos);
                out[out_len++] = popped;
                out[out_len] = '\0';
                if (verbose) {
                    printf("        弹出 '%c'（原串第 %d 个字符）→ 它比墙内的一切都要先算，写进输出\n",
                           popped, popped_pos);
                    print_conversion_state(out, &ops);
                }
            }

            if (!found_open) {
                printf("    ❌ 第 %d 个字符 ')' 找不到配对的 '(' → 括号不匹配，转换失败\n", i);
                if (ok_out != NULL) {
                    *ok_out = false;
                }
                cst_destroy(&ops);
                free(out);             /* ★ 失败路径也要释放 out：返回 NULL 但绝不泄漏 */
                return NULL;
            }
            if (verbose) {
                if (!found_open) {
                    printf("        一直弹到栈空都没见到 '(' —— 说明这个 ')' 是多余的\n");
                }
                print_conversion_state(out, &ops);
            }
            continue;
        }

        if (precedence(ch) >= 1) {
            /* 规则②：把栈里优先级 >= 自己的都弹进输出，再压自己 */
            if (verbose) {
                printf("    第 %d 个字符 '%c'：运算符（优先级 %d）→ 规则②，先把栈里优先级 >= %d 的都倒进输出\n",
                       i, ch, precedence(ch), precedence(ch));
            }
            while (!cst_is_empty(&ops)) {
                char top_ch = 0;
                int top_pos = 0;
                cst_peek(&ops, &top_ch, &top_pos);

                if (top_ch == '(') {
                    if (verbose) {
                        printf("        栈顶是 '('（一堵墙）→ 停止弹栈，墙里的东西属于另一个子表达式\n");
                    }
                    break;
                }
                if (precedence(top_ch) < precedence(ch)) {
                    if (verbose) {
                        printf("        栈顶 '%c'(优先级%d) < 新来的 '%c'(优先级%d) → 栈顶更晚算，"
                               "按规矩不能现在输出它 → 停止弹栈\n",
                               top_ch, precedence(top_ch), ch, precedence(ch));
                    }
                    break;
                }
                char popped = 0;
                int popped_pos = 0;
                cst_pop(&ops, &popped, &popped_pos);
                out[out_len++] = popped;
                out[out_len] = '\0';
                if (verbose) {
                    printf("        弹出 '%c'（优先级%d >= 新来的%d，而且 + - * / 都是左结合，"
                           "它必须先算）→ 写进输出\n",
                           popped, precedence(popped), precedence(ch));
                    print_conversion_state(out, &ops);
                }
            }
            cst_push(&ops, i, ch);
            if (verbose) {
                printf("        然后把 '%c' 压栈（它要等右边的操作数读齐、且没有更优先的才输出）\n", ch);
                print_conversion_state(out, &ops);
            }
            continue;
        }

        /* 其它字符：本节目的里只允许 数字 运算符 括号 空格 */
        printf("    ❌ 第 %d 个字符 '%c' 是本实现不支持的字符（只支持 0-9 + - * / ( ) 和空格）\n", i, ch);
        if (ok_out != NULL) {
            *ok_out = false;
        }
        cst_destroy(&ops);
        free(out);
        return NULL;
    }

    /* 扫完了：把运算符栈里剩下的全部倒进输出（此时不可能有 '('，除非没闭合） */
    if (verbose) {
        printf("    输入读完，把运算符栈里剩下的全部倒进输出：\n");
    }
    while (!cst_is_empty(&ops)) {
        char popped = 0;
        int popped_pos = 0;
        cst_pop(&ops, &popped, &popped_pos);
        if (popped == '(') {
            printf("    ❌ 第 %d 个字符 '(' 一直没有闭合 → 括号不匹配，转换失败\n", popped_pos);
            if (ok_out != NULL) {
                *ok_out = false;
            }
            cst_destroy(&ops);
            free(out);
            return NULL;
        }
        out[out_len++] = popped;
        out[out_len] = '\0';
        if (verbose) {
            printf("        倒出 '%c'（输入都读完了，栈里剩下的运算符全都比它们右边的数字更晚输出）\n",
                   popped);
            print_conversion_state(out, &ops);
        }
    }

    cst_destroy(&ops);

    if (out_len == 0) {
        printf("    ❌ 转换结果是空串（输入里没有任何数字）\n");
        if (ok_out != NULL) {
            *ok_out = false;
        }
        free(out);
        return NULL;
    }

    if (verbose) {
        printf("  ✅ 后缀表达式 = \"%s\"（长度 %d）\n", out, out_len);
    }
    if (ok_out != NULL) {
        *ok_out = true;
    }
    return out;                        /* ★ 这块内存归调用者，必须有人 free */
}

/*
 * 外壳：带打印版（第 2 幕用）
 */
char *expr_infix_to_postfix(const char *expr, bool *ok_out)
{
    return to_postfix_impl(expr, ok_out, true);
}

/*
 * 后缀求值的内部实现（verbose 决定打不打印每一步出栈计算过程）。
 *
 * 规则：数字压栈；运算符弹出【两个】数，算完把结果压回。
 * ⚠ 弹出顺序是本节最容易写反的一行：
 *     先弹出的是【右】操作数 b（它是最后压进去的），后弹出的是【左】操作数 a。
 *     写成 a 先 b 后，减法除法就全错：8-3 会算成 3-8，8/2 会算成 2/8=0（整数除法还特别隐蔽）。
 *     加乘满足交换律看不出问题 —— 所以测试一定要包含 - 和 /。
 * 边界：
 *   · 运算符前面不足两个数 → 表达式不合法（比如 "3+"、"3++4"）
 *   · 除数为 0 → 明确报错（整数除 0 会当场崩，绝不能让它走到那一步）
 *   · 扫完栈里不止一个数 → 说明运算符太少（"34"）或者结构不对
 *   · 扫完栈是空的 → 空表达式或全是运算符
 */
static bool eval_postfix_impl(const char *postfix, int *result_out, bool verbose)
{
    if (postfix == NULL || postfix[0] == '\0') {
        printf("  [错误] 后缀串是空的\n");
        if (result_out != NULL) {
            *result_out = 0;
        }
        return false;
    }

    int len = (int)strlen(postfix);
    IntStack st;
    ist_init(&st, len);                /* 数栈最多同时装 len 个操作数 */
    if (st.data == NULL) {
        if (result_out != NULL) {
            *result_out = 0;
        }
        return false;
    }

    bool ok = true;
    const char *why = "";

    if (verbose) {
        printf("  后缀: \"%s\"   求值过程（栈里放「还没被运算符用掉的数」）：\n", postfix);
    }

    for (int i = 0; i < len; i++) {
        char ch = postfix[i];

        if (ch >= '0' && ch <= '9') {
            int v = ch - '0';          /* 字符 '7' → 整数 7：减去 '0' 的 ASCII 码，这是 C 的固定套路 */
            ist_push(&st, v);
            if (verbose) {
                printf("    读到数字 '%c' → 压入 %d\n", ch, v);
                ist_print(&st, "压入后");
            }
            continue;
        }

        if (precedence(ch) >= 1) {
            int b = 0;
            int a = 0;

            /* 先弹右操作数 */
            if (!ist_pop(&st, &b)) {
                ok = false;
                why = "运算符前面缺少数（栈已经空了）";
                if (verbose) {
                    printf("    读到运算符 '%c' → ❌ 栈是空的，哪来的右操作数？表达式不合法\n", ch);
                }
                break;
            }
            /* 再弹左操作数 */
            if (!ist_pop(&st, &a)) {
                ok = false;
                why = "运算符前面只有一个数（一元负号本实现不支持）";
                if (verbose) {
                    printf("    读到运算符 '%c' → 只弹到右操作数 %d，左操作数没有了 → 表达式不合法\n", ch, b);
                }
                break;
            }

            if (verbose) {
                printf("    读到运算符 '%c' → 先弹出右操作数 b=%d，再弹出左操作数 a=%d\n", ch, b, a);
            }

            int r = 0;
            if (ch == '/') {
                if (b == 0) {
                    ok = false;
                    why = "除数是 0";
                    printf("    ❌ %d / %d：除数为 0！整数除 0 会直接让程序崩，这里提前挡住\n", a, b);
                    break;
                }
                r = a / b;
            } else if (ch == '*') {
                r = a * b;
            } else if (ch == '+') {
                r = a + b;
            } else {                   /* '-' */
                r = a - b;
            }

            if (verbose) {
                printf("      计算 a %c b = %d %c %d = %d，把结果 %d 压回栈\n", ch, a, ch, b, r, r);
            }
            ist_push(&st, r);
            if (verbose) {
                ist_print(&st, "算完后");
            }
            continue;
        }

        printf("    ❌ 后缀串里出现不支持的字符 '%c'\n", ch);
        ok = false;
        why = "后缀串里有非法字符";
        break;
    }

    int final_result = 0;
    if (ok) {
        if (ist_is_empty(&st)) {
            ok = false;
            why = "扫完了但栈里一个数都没有";
        } else {
            ist_pop(&st, &final_result);
            if (!ist_is_empty(&st)) {
                ok = false;
                why = "表达式结构不对：最后栈里还剩多个数，说明运算符太少";
                if (verbose) {
                    ist_print(&st, "剩下的");
                }
            }
        }
    }

    ist_destroy(&st);

    if (ok) {
        if (result_out != NULL) {
            *result_out = final_result;
        }
        if (verbose) {
            printf("  ✅ 结果 = %d\n", final_result);
        }
    } else {
        if (result_out != NULL) {
            *result_out = 0;
        }
        printf("  ❌ 求值失败：%s\n", why);
    }
    return ok;
}

bool expr_eval_postfix(const char *postfix, int *result_out)
{
    return eval_postfix_impl(postfix, result_out, true);
}

/*
 * 一步到位：中缀 → 后缀 → 求值（不打过程，只报结论）。
 *
 * ★ 内存责任链，看清楚：
 *     to_postfix_impl 内部 malloc 出后缀串 → 返回给本函数
 *     本函数用它算完 → 【立刻 free】，因为"这里 malloc 到手的是我"。
 *   如果忘了这一句 free，每调用一次就漏掉一串内存；ASan 会直接报 leak。
 */
bool expr_eval_infix(const char *expr, int *result_out)
{
    bool ok = false;
    char *postfix = to_postfix_impl(expr, &ok, false);
    if (!ok || postfix == NULL) {
        printf("  \"%s\" → ❌ 转换失败\n", expr);
        free(postfix);                 /* free(NULL) 是合法空操作，所以这里不用先判空 */
        if (result_out != NULL) {
            *result_out = 0;
        }
        return false;
    }

    bool eval_ok = eval_postfix_impl(postfix, result_out, false);
    printf("  \"%s\"  →  后缀 \"%s\"  →  ", expr, postfix);
    if (eval_ok) {
        printf("%d ✅\n", (result_out != NULL) ? *result_out : 0);
    } else {
        printf("失败 ❌\n");
    }

    free(postfix);                     /* ★ 上面那句 malloc 的归还处 */
    postfix = NULL;
    return eval_ok;
}
