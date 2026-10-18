#!/usr/bin/env python3
"""报错原文解剖（四）：TabError —— tab 和空格混用，Python 直接拒绝翻译。

这个文件里的缩进是**真的混用**了制表符和空格（不是只在注释里说说）。
想看证据：

    cat -A i_errors/tab_error.py | sed -n '12,17p'      # ^I 就是制表符

为什么从网上/Word/别处复制一段代码进来就会炸：那段代码用的是空格，
而你的文件里已经有一批用制表符写的行，两者的「列」一旦撞上，解释器就报 TabError，
并且**不会**帮你猜哪边是对的。

跑法：python3 i_errors/tab_error.py
"""


def roll(dice):
	if dice > 3:
		return "大"
        return "小"          # ← 这一行用 8 个空格；上面的 if 用 1 个制表符，列正好撞上


print(roll(5))
