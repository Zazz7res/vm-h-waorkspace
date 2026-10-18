# random.py —— 抢名字的假模块（本章最重要的一颗雷，只放在沙箱里演示）
#
# 你自己写了个文件叫 random.py / json.py / email.py / test.py……
# 只要它出现在 sys.path 的**前面**（脚本目录、当前目录、PYTHONPATH 都排在标准库前面），
# `import random` 拿到的就是它，标准库那份根本不会被打开。
#
# 这个假模块故意只提供 roll()，不提供 randint()，
# 于是报错会以「AttributeError: module 'random' has no attribute 'randint'」的形式
# 出现在**你完全没改过的第三方代码里** —— 这就是最难查的那种现场。
#
# 注意：本文件里所有解释都写成 # 注释而不是文档字符串，
# 因为注释不会进字节码（对比 demo_module.py 的 co_consts 就能看到差别）。

def roll():
    return 4          # 掷骰子，保证随机（笑）


def whoami():
    import os
    return os.path.abspath(__file__)
