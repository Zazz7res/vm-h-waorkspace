"""第 02 章第 3 节：赋值编译成什么，那张「名字表」在哪。

跑法：python3 c_assign_dis.py

第 2 节说了「赋值只改指向」，这一节把它的实现摊开：
  * 字节码里是一条 STORE_NAME / STORE_FAST，没有任何「把值搬进盒子」的指令；
  * 模块层的那张名字表是真实存在的一个 dict —— 就是 `globals()` / `__dict__`。
"""
import dis
import sys


def same(x, y):
    return id(x) == id(y)


print("=" * 68)
print("一、`a = 1` 编译出来就两条指令")
print("=" * 68)
print("源码： a = 1")
dis.dis(compile("a = 1", "<演示>", "exec"))
print("读法： LOAD_SMALL_INT 1  →  把已经存在的对象 1 压上栈（没有造盒子）")
print("      STORE_NAME 0 (a)  →  在名字表里把名字 a 指向那个对象")
print("注意指令名叫 STORE_NAME 而不是 STORE_TO_BOX：赋值从来不搬值，只改指向。")

print("=" * 68)
print("二、那张名字表就是模块的 __dict__，可以拿在手里看")
print("=" * 68)
answer = 42
print("globals() 的类型：", type(globals()).__name__)
print("名字表里有 answer 这个键吗？", "answer" in globals())
print("globals()['answer'] 和直接写 answer 是同一个对象吗？", same(globals()["answer"], answer))
print("globals() 和 vars() 在模块顶层是同一个 dict 吗？", id(globals()) == id(vars()))
print("模块对象自己的 __dict__ 就是 globals() 吗？",
      sys.modules["__main__"].__dict__ is globals())
print("所以「给变量赋值」= 「往 dict 里加一个键」，这就是为什么能用字符串当名字：")
globals()["字符串当名字"] = 7
print("  globals()['字符串当名字'] =", globals()["字符串当名字"])
del globals()["字符串当名字"]
print("  删掉键之后 '字符串当名字' in globals() →", "字符串当名字" in globals())
print("→ C 的变量名编译完就没了；Python 模块层的名字活到进程结束，还就是一个 dict 的键。")
print("  代价：每次读全局名都要做一次哈希查找（01 章场景 2 埋的那句话，在这里兑现）。")

print("=" * 68)
print("三、函数里的名字不查这个 dict：STORE_FAST")
print("=" * 68)
dis.dis(compile("def f():\n    a = 1\n    return a\n", "<演示>", "exec"))
print("读法：局部名字是 STORE_FAST/LOAD_FAST —— 存在帧里的固定槽位，按索引取，不查 dict。")
print("所以同一个名字 a，在模块层是 dict 的键，在函数里是栈槽；")
print("这个区别就是第 18 节 UnboundLocalError 的全部原因（编译期就定了它归哪一层）。")

print("=" * 68)
print("四、类的方法体里读名字，先看局部、再看类体外面——不看类体")
print("=" * 68)


class Holder:
    base = 10

    def read(self):
        return "base" in locals(), "base" in globals()


print("方法里 'base' 在 locals 吗 / 在 globals 吗：", Holder().read())
print("（类体自己是一层名字空间，但那层不会传给方法 —— 方法里必须写 self.base。")
print("  这已经是第 06 章的内容了，这里只把边界摆出来。）")
