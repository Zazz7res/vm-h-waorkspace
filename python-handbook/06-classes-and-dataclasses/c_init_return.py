"""第 06 章第 5 节：__init__ 返回非 None —— 让它当场崩给你看。

跑法：python3 c_init_return.py（本脚本注定以退出码 1 结束，这就是目的）

__init__ 的协议是「初始化并返回 None」。Return 了别的东西，
Python 不忽略、不警告，直接 TypeError 把你拦住（3.14 原文见最后一行）。
"""


class Greeter:
    def __init__(self, name):
        self.name = name
        return f"你好，{name}"      # 故意返回字符串：构造器不是工厂函数


print("定义没问题，import 阶段一切正常；崩在调用的那一刻：")
Greeter("小安")
print("这一行永远执行不到")
