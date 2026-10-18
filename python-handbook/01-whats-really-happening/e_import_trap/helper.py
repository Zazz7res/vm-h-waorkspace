"""helper.py —— 一个「邻居模块」，专门用来演示 import 到底按哪个目录找人。

它只有一个函数，跑起来什么都不做，就是为了让你能用 `import helper` 去试各种启动方式。
"""


def hello():
    return "找到 helper.py 了"


def where_am_i():
    import os
    return os.path.abspath(__file__)
