/* util/util.h —— 一个极小的“工具库”，本章专门用它来演示“使用要求怎么传播” */
#ifndef UTIL_H
#define UTIL_H

/*
 * 把 x 夹进 [lo, hi]。
 * 这个函数本身不重要，重要的是：vector.c 和 vstats.c 都要 #include 本文件、
 * 都要调用它 —— 于是“谁能看见 util.h、谁看不见”就变成了本章的实验对象。
 */
int u_clamp(int x, int lo, int hi);

/* 扩容时单次最多翻到多少（一个刻意随便定的上限，用来让 u_clamp 真的被用上） */
#define U_MAX_GROWTH 1024

#endif /* UTIL_H */
