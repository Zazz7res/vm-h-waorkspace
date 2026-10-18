"""第 02 章第 16 节：del 删的是名字，不是对象；引用计数一格一格数给你看。

跑法：python3 o_refcount_del.py
（本节证据就是这段脚本的输出，demo.sh 里有对应小节。）
"""
import gc
import sys
import weakref


class Thing:
    """一个只有名字开销的普通类，用来当被观察的对象。"""


def rc(obj):
    """把 obj 交给 sys.getrefcount：注意函数参数本身也是一个持有者。"""
    return sys.getrefcount(obj)


print("=" * 68)
print("一、每多一个持有者，计数加 1（看增量，别看绝对值）")
print("=" * 68)
t = Thing()
base = rc(t)
print("1 个名字：  增量", 0, "绝对值", base)
u = t
print("加 1 个名字：增量", rc(t) - base)
bag = [t]
print("塞进 list： 增量", rc(t) - base)
shelf = {"k": t}
print("塞进 dict： 增量", rc(t) - base)
print("绝对值现在是", rc(t))
del u, bag, shelf
print("删掉三个持有者之后：增量", rc(t) - base)
print("→ 每行都恰好 +1：名字、list 的槽、dict 的值，全是对同一个对象的引用。")
print("  绝对值里含解释器自己的临时引用，所以只能看差值；")
print("  顺带一个现象：同一段代码在函数里测，比在模块顶层测少 1（本机如此，本章不解释谁多持有一份）。")
print("  对照：函数里的局部对象：", end=" ")


def local_count():
    v = Thing()
    return rc(v)


print(local_count())

print()
print("=" * 68)
print("二、del 之后名字没了，对象还在（还有别人指着）")
print("=" * 68)
keep = [1, 2, 3]
also = keep
del keep
print("del keep 之后 also =", also)
try:
    print(keep)
except NameError as err:
    print("报错原文：", err)
print("→ NameError 说的是「这个名字不存在」，不是「这个东西被删了」。")
print("  对象活得好好的，因为 also 还指着它。")

print()
print("=" * 68)
print("三、最后一个名字删掉 → 立刻回收（weakref 当证人）")
print("=" * 68)
w = Thing()
watcher = weakref.ref(w)          # 弱引用：指着但不计数
print("有名字时，证人看得见对象吗？", watcher() is not None)
del w
print("del 之后，证人还看得见吗？", watcher() is not None)
print("→ 是 False：计数归 0，对象当场释放，不用等 GC。（这也是为什么 del 之后 id 会被复用）")

print()
print("=" * 68)
print("四、del 还能删别的东西：下标、属性、切片、以及 del 一个不存在的名字")
print("=" * 68)
data = [1, 2, 3, 4]
del data[1]
print("del data[1] →", data)
del data[1:]
print("del data[1:] →", data)
holder = Thing()
holder.tag = 1
del holder.tag
print("del 属性之后 hasattr？", hasattr(holder, "tag"))
print("del 一个不存在的名字：", end=" ")
try:
    del never_existed
except UnboundLocalError as err:
    print("UnboundLocalError:", err)
except NameError as err:
    print("NameError:", err)
