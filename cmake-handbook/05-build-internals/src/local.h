/* local.h —— 这一份和 src/main.c 在**同一个目录**里。
   用途：证明 #include "local.h"（双引号）会先搜"包含者自己所在的目录"，
   这一步排在所有 -I 前面。所以不管 -I 怎么排，这一份永远赢。
   对比 include/local.h：同名，但只有尖括号才拿得到它（第 11 节现场演）。
   注意：两份的**包含宏故意不同名**，否则第二份会被第一份的防重复包含挡掉，
   就看不出区别了。 */
#ifndef SRC_LOCAL_H
#define SRC_LOCAL_H
#define LOCAL_QUOTED "双引号 \"local.h\" → src/local.h（同目录优先，-I 抢不过）"
#endif
