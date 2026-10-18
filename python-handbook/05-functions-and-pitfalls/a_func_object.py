#!/usr/bin/env python3
# ============================================================================
# a_func_object.py —— 第 05 章第 2 节：`def` 造出来的到底是个什么东西
#
# 一句话：函数不是「一段代码」，是一个**能被调用的对象**。
# 它和 42、"文本"、[1, 2] 一样：能存进变量、能塞进列表、能当参数传、能从函数里返回。
# 本文件把这些「一样」逐条跑给你看，顺便把函数对象身上的属性全列一遍 ——
# 后面所有坑（默认值只算一次、闭包共享 cell、装饰器换名字）都住在这张属性表里。
# ============================================================================

def greet(name, times=1):
    """跟 name 打 times 次招呼。"""
    return f"你好，{name}！" * times


print("=== 1. 函数是个对象：type 打得出来、callable 认得出 ===")
print("type(greet) =", type(greet))
print("greet 是实例吗（isinstance(greet, object)）：", isinstance(greet, object))
print("callable(greet) =", callable(greet))
print("greet.__class__.__name__ =", greet.__class__.__name__)
print("调一下：greet('小明', 2) =", greet("小明", 2))

print()
print("=== 2. 函数对象身上的属性（本章后面全靠这几张身份证）===")
for attr in ("__name__", "__qualname__", "__module__", "__doc__",
             "__defaults__", "__kwdefaults__", "__annotations__",
             "__dict__", "__closure__", "__code__"):
    value = getattr(greet, attr)
    # __code__ 只打类型，它的内容第 3 节单独拆（打全了会带文件路径）
    shown = type(value).__name__ if attr == "__code__" else repr(value)
    print(f"  {attr} = {shown}")

print()
print("=== 3. 函数对象能改进自己：__dict__ 是可写的（类字典是只读，对比第 06 章）===")
greet.version = 3
greet.__dict__["owner"] = "学习者"
print("  greet.__dict__ =", greet.__dict__)
print("  加进去的属性直接能用：greet.version =", greet.version, "｜ greet.owner =", greet.owner)
print("  写 __dict__ 和 setattr 是一回事：", "version" in greet.__dict__)

print()
print("=== 4. 一等公民的四条用法（存、传、返、比）===")
table = {"g": greet, "u": str.upper}
print("  存进字典再调：table['g']('小红') =", table["g"]("小红"))


def call_twice(fn, arg):          # 参数位置收一个函数
    return [fn(arg), fn(arg)]


print("  当参数传进去：call_twice(greet, 'A') =", call_twice(greet, "A"))
print("  当返回值出来：", call_twice(lambda s: s.strip(), "  B  ") == ["B", "B"])
print("  放进列表按下标调：[greet, str.lower][1]('MiXeD') =", [greet, str.lower][1]("MiXeD"))
print("  能比：greet == table['g'] →", greet == table["g"], "｜ 和另一个函数比 →", greet == str.upper)
print("  能进 set（函数可哈希，默认按身份）：{greet, table['g'], str.upper} 的元素个数 =",
      len({greet, table["g"], str.upper}))

print()
print("=== 5. 同名 def 写两遍：名字被换走，旧对象没死（回指第 02 章「赋值是贴名字」）===")
first = greet                     # first 和 greet 现在贴着同一个函数对象


def greet(name, times=1):         # 第二次 def：造一个**新对象**，把名字 greet 揭下来贴过去
    return f"[新版] 你好，{name}" * times


print("  first is greet →", first is greet)
print("  新对象 greet('老王', 1) =", greet("老王", 1))
print("  旧对象被 first 拽着还能调：first('老王', 2) =", first("老王", 2))
print("  旧对象的 __doc__ 还在：", repr(first.__doc__))
del greet
print("  del 掉名字 greet 之后，first 照样能调：first('收尾') =", first("收尾"))

print()
print("=== 6. 一个反直觉的小账：def 跑一次 = 造一个新对象 ===")
made = []


def factory():
    def inner():                  # 每次跑 factory，这一行都造一个新的函数对象
        return 1
    return inner


a, b = factory(), factory()
print("  a is b →", a is b, "（同一段源码，两个对象）")
print("  但它们的 __code__ 是同一个：", a.__code__ is b.__code__, "（字节码复用，包壳各造一个）")
made.append(a)
print("  函数对象的开销：每次 def 都新造一个壳，别在热循环里现写 def（第 22 节有计时）")
