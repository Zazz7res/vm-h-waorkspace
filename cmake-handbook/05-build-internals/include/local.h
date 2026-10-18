/* local.h —— 和 src/local.h **同名**，但这份不在 main.c 的目录里，
   只有尖括号 #include <local.h>（跳过"包含者所在目录"那一步）才会解析到这里。
   包含宏故意和 src/local.h 不同名，否则会被那边的防重复包含挡掉。 */
#ifndef INC_LOCAL_H
#define INC_LOCAL_H
#define LOCAL_BRACKET "尖括号 <local.h> → include/local.h（同目录那一步被跳过）"
#endif
