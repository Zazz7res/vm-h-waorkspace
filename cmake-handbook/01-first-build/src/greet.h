/* greet.h —— 这个小库对外只暴露一个函数 */
#ifndef GREET_H
#define GREET_H

#include <stddef.h>   /* size_t：函数签名里用了它，就必须自己 include，别靠传递包含 */

/*
 * 把 "你好, <name>!" 写进 buf（最多 n 个字节，含结尾 '\0'）。
 * 返回值：本该写入的长度（**不是**实际写入的长度）—— 这就是 snprintf 的语义，
 * 第 01 章 README 里"截断怎么判断"那一段讲的正是它。
 */
int greet_build(char *buf, size_t n, const char *name);

#endif /* GREET_H */
