#ifndef CH07_UTIL_H
#define CH07_UTIL_H

/* util_stamp() 打印的是"编译 util.c 那一刻编译器看到的 CH07_STAMP 宏"。
 * 它是本章另一个探针：老写法 add_definitions(-DCH07_STAMP=...) 是全局的，
 * 所以 calc、util、app、greet、notepad 五个 target 会拿到同一个值；
 * 换成 target_compile_definitions 之后，只有被点名的那个 target 变。
 * 于是"谁吃到了这个宏"这件事，不用猜 flags.make，跑一下程序就看得见。 */
void util_stamp(void);

#endif /* CH07_UTIL_H */
