#ifndef CH07_FOO_H
#define CH07_FOO_H

/* foo_tag() 由 libfoo.a 提供 —— 而沙箱里同时存在两份 libfoo.a（一份返回 1、一份返回 2），
 * 名字一模一样、路径不一样。谁赢，由链接命令里 -L 的出现顺序决定（第 2 部分整节都在演它）。 */
int foo_tag(void);

#endif /* CH07_FOO_H */
