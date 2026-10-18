#!/usr/bin/env python3
# ============================================================================
# 这个文件在演什么：一边遍历一边改容器，到底会发生什么 ——
#   remove 只删第一个匹配、del/pop 按下标 vs remove 按值、
#   正向遍历时删元素的真实「残留结果」、dict 边遍历边删的报错原文、
#   以及容器上的 == 与 is 到底比的是什么。
# 为什么演：
#   * 你在 C 里写链表删除时是靠「prev->next = cur->next」，指针在手，删谁都一样。
#     Python 的 list 是数组，删一个元素要把后面整段往前挪（见 pop_insert_probe.py 量的数），
#     而 `for x in a` 内部是**一个按下标递增的迭代器**：你把数组改短了，
#     它的下标还在往前走 —— 于是「跳过下一个元素」。这不是文档里的警告，是算术。
#   * 本文件每一种错法都打出**改完之后容器里还剩什么**，而不是只说「别这么写」。
#     残留结果比道理有用：你下次见到那个怪结果能立刻认出病因。
#   * RuntimeError 那两条是「dict / deque 边遍历边改」的报错，字符串要逐字记住，
#     它们和「list 边遍历边改」完全不同 —— list 那份**不报错**，只是安静地给你错答案。
#
# 只用标准库（collections.deque）。每段后面用 → 给结论。
# ============================================================================
from collections import deque

SEP = "=" * 76


def show(tag, obj):
    print(f"  {tag:<34} {obj}")


print(SEP)
print("一、remove(值)：只删**第一个**匹配，删不到就 ValueError")
print(SEP)
a = [7, 3, 7, 5, 7]
show("原始 a", a)
a.remove(7)
show("a.remove(7) 之后", a)
print(f"  a.count(7) = {a.count(7)}   ← 三个 7 变两个，一次只干掉一个")
print("  remove 的返回值：", repr(a.remove(3)), "  ← None！它只改你手上这个 list，不给你新的")
print("  → 想「删掉所有 7」不能靠反复 remove(7) 配一个已知次数，得先数：")
n = a.count(7)
for _ in range(n):
    a.remove(7)
show(f"  循环 {n} 次之后", a)
try:
    a.remove(99)
except ValueError as e:
    print(f"  再删一个不存在的值 → ValueError: {e}")
print("→ 报错原文逐字就是 `list.remove(x): x not in list`（小写 l 开头，因为它是方法名，")
print("  不是类名；而且它**不告诉你**要删的值是什么 —— 想 debug 得自己把值打进日志里）。")
print("  对法（如果你根本不知道有几个匹配）：推导式重新造一个，见第四节。")

print(SEP)
print("二、按下标删 vs 按值删：del / pop / remove 三种签名与返回值")
print(SEP)
b = [10, 20, 30, 40]
show("原始 b", b)
x = b.pop()
show("b.pop()（不带参数）→ 返回", f"{x!r}   b = {b}")
x = b.pop(0)
show("b.pop(0) → 返回", f"{x!r}   b = {b}")
del b[0]
show("del b[0] → 没有返回值", f"b = {b}")
print("  → 三句话：pop 给你删掉的那个元素；del 什么都不给；remove 按值找、也是什么都不给。")
print("  → pop(0) 和 del b[0] 都是 O(n)（要把后面整段前移），pop() 和 del b[-1] 才是 O(1)。")
print("    数字在 pop_insert_probe.py，本节只管语义。")
try:
    b.pop(9)
except IndexError as e:
    print(f"  下标越界 → IndexError: {e}")
show("b[99:100]（切片越界）", b[99:100])
print("→ 注意这行对比：按下标越界立刻 IndexError，切片越界给你空 list，一句不报。")
print("  （切片那套越界规矩在 slicing_probe.py 第一节。）")

print(SEP)
print("三、错法：正向遍历 list 的同时删元素 —— 看真实残留")
print(SEP)
print("  先给你一个「侥幸对」的版本，再给你一个「当场错」的版本。侥幸对才是最坑的。")
lst = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
for x in lst:
    if x % 2 == 0:
        lst.remove(x)
show("删偶数，跑完剩下", lst)
show("本该剩下", [1, 3, 5, 7, 9])
print("  → 这次**居然对了**。原因见下面的逐帧表：迭代器确实每一步跳过一个元素，")
print("    但在「1,2,3,4,…」这种奇偶交替的排列里，被跳过的恰好全是奇数，没伤着目标。")
print("    换个排列就不一样了 ↓")

lst = [2, 4, 6, 8, 10]
for x in lst:
    lst.remove(x)
show("全删 [2,4,6,8,10]，跑完剩下", lst)
show("本该剩下", [])
print("  → 还剩俩。这就是「跳过」暴露出来的样子。")

lst = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
for x in lst:
    if x <= 5:
        lst.remove(x)
show("删所有 <=5，跑完剩下", lst)
show("本该剩下", [6, 7, 8, 9, 10])
print("  → 残留里躺着 2 和 4：它们**满足条件却没被删**。")
print("    更糟的是这个残留每次都一样，你不会怀疑它 —— 直到某天数据换了个顺序。")

print("  逐帧表（下面这个 while 是 list 迭代器「下标 +1」行为的等价复刻，")
print("  最后一行拿它的结果和真 for 循环比，两者必须一致才算讲得对）：")
src = [2, 4, 6, 8, 10]
work = list(src)
i = 0
while i < len(work):
    x = work[i]
    i += 1                     # 迭代器：先取 work[i]，再把下标 +1（它不知道数组变短了）
    work.remove(x)
    print(f"    取 work[{i - 1}] = {x:>2}  删掉 → work = {work}   下一步会取下标 {i}")
show("逐帧表跑完", work)
lst = list(src)
for x in lst:
    lst.remove(x)
print(f"  真 for 循环跑完 = {lst}")
print(f"  两种走法一致吗：{work == lst}")
print("→ 病因就一句话：**删完数组变短，游标还在往前走**，于是每删一个就漏看下一个。")
print("  对 C 的直觉修正：C 里 `list_for_each_safe` 是先存 `next` 再摘节点，")
print("  所以删除不影响遍历；Python 的 list 迭代器只存下标，不存「下一个元素是谁」。")
print("  deque 那边就不这么客气了 —— 它和 dict 一样当场拦住你（本机原文）：")
dq = deque([1, 2, 3])
try:
    for x in dq:
        dq.append(x)          # 只是 append，既没删也没换对象
except RuntimeError as exc:
    print(f"    for x in dq 时 dq.append(x) → RuntimeError: {exc}")
    print(f"    报错之后 dq = {list(dq)}   ← 它拦住了遍历，但那次 append 已经真的进去了")
print("  → 三个容器三种性格，这张表就是本节的全部结论：")
print("      list 边遍历边改：不报错，安静给你错答案（第一到第三节）；")
print("      dict 边遍历边改大小：RuntimeError: dictionary changed size during iteration（第五节）；")
print("      deque 边遍历边改：RuntimeError: deque mutated during iteration（上面这两行）。")

print(SEP)
print("四、对法四种，全部真跑一遍")
print(SEP)
data = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
p1 = [x for x in data if x % 2]
show("① 推导式造新 list", p1)
lst = list(data)
lst[:] = [x for x in lst if x % 2]
show("② 切片赋值原地改", lst)
lst = list(data)
for x in [v for v in lst if v % 2 == 0]:
    lst.remove(x)
show("③ 遍历另一份快照", lst)
lst = list(data)
for i in range(len(lst) - 1, -1, -1):
    if lst[i] % 2 == 0:
        del lst[i]
show("④ 倒着按下标删", lst)
print("→ ①造新对象（原 list 还在，别的引用不受影响）；②③④改的是原对象（所有标签一起看到）。")
print("  ②用切片赋值的原因见 copy_ladder.py：`lst = [...]` 只是换标签，函数外的调用方看不见。")

print(SEP)
print("五、错法：边遍历 dict 边改大小 —— 报错原文逐字看")
print(SEP)
d = {"a": 1, "b": 2, "c": 3, "d": 4}
try:
    for k, v in d.items():
        if v % 2 == 0:
            del d[k]
except RuntimeError as e:
    print(f"  RuntimeError: {e}")
show("崩之前 d 已经是", d)
show("d 的长度", len(d))
print("→ 注意三件事：")
print("  ① list 那份「安静地给错答案」，dict 这份「直接 RuntimeError」—— 因为 dict 的")
print("     遍历顺序依赖索引数组，扩容/重排会让游标失去意义，解释器选择当场拦住你。")
print("  ② 报错信息里没有键名、没有第几步，只有 changed size 这四个词。")
print("  ③ 上面 d 里 'b' 已经被删掉了 —— 报错发生在**下一次迭代时**（解释器发现版本号变了），")
print("     不是发生在 del 那一刻。所以你看到的是「改到一半崩了」的中间状态，数据是真的脏了。")
print("     字典**只改值不增删键**是不报错的（下一行验证）：")
d2 = {"a": 1, "b": 2}
for k in d2:
    d2[k] = d2[k] * 10
show("  只改值跑完了", d2)
print("  → 所以这条报错的准确触发条件是「size 变了」，也就是 insert / del / pop / setdefault。")
print("    最后那个 setdefault：键已存在时它不新增，所以不报错；键不存在时它**会**新增，就报错。")
print("  对法：")
show("  ① 造一份新 dict（原 d 不动）", {k: v for k, v in d.items() if v % 2})
d3 = {"a": 1, "b": 2, "c": 3, "d": 4}
for k in [k for k, v in d3.items() if v % 2 == 0]:
    del d3[k]
show("  ② 先拿键的快照再原地 del", d3)

print(SEP)
print("六、== 与 is 用在容器上：一个比「值逐格比」，一个比「同一个对象」")
print(SEP)
p = [1, 2, 3]
q = [1, 2, 3]
show("p == q", p == q)
show("p is q", p is q)
show("id(p) == id(q)", id(p) == id(q))
r = p
show("r = p 之后 r is p", r is p)
show("r = p 之后 r == p", r == p)
m = {"x": 1, "y": 2}
n = {"y": 2, "x": 1}
show("键序不同的两个 dict，m == n", m == n)
show("m is n", m is n)
s1 = {1, 2, 3}
s2 = {3, 2, 1}
show("顺序打乱的两个 set，s1 == s2", s1 == s2)
t1 = (1, 2)
t2 = (1, 2)
show("同样写法的两个 tuple，t1 == t2", t1 == t2)
show("t1 is t2", t1 is t2)
print("→ tuple 这一格量出来是 True —— 不是「tuple 比了值」，而是这两个 `(1, 2)` 在同一个")
print("  代码对象里被编译成了**同一个常量**，所以本来就是同一个对象。换一种写法就露馅：")
built = tuple([1, 2])
show("  t1 == tuple([1, 2])", t1 == built)
show("  t1 is tuple([1, 2])", t1 is built)
print("  → 结论：`is` 的结果由「对象从哪来」决定，跟你想比较的东西没关系。")
print("    这正是第 02 章第 8 节「别用 is 比值」那条规矩在容器上的续集：")
print("    list / dict / set 每次字面量都新建（上面 p is q = False），tuple 有可能被复用。")
print("  判据永远写 == / != ；is 只用来和 None 比，或者明确要判断别名关系时（本文件第一到第四节）。")

print(SEP)
print("七、容器 == 是「逐格递归比」，所以共享内层对象会串味")
print(SEP)
row = [0, 0]
shared = [row, row]
own = [[0, 0], [0, 0]]
show("shared == own", shared == own)
print("  → 两个结构 == 成立（值全对得上），但改法完全不同（第三行开始露馅）：")
shared[0][1] = 9
own[0][1] = 9
show("  各改一格后 shared", shared)
show("  各改一格后 own", own)
show("  改完还相等吗", shared == own)
print("→ `==` 只看「此刻的值的形状」，不看「谁和谁共用」。alias_pitfalls.py 那套 id 判据")
print("  是唯一能把这两件事分开的办法。")

print(SEP)
print("八、顺手一条：普通 for 的循环变量会留在作用域里，推导式的不会")
print(SEP)
for loop_var in [10, 20]:
    pass
show("for 之后 loop_var 还在", loop_var)
out = [y for y in [1, 2, 3]]
show("[y for y in [1, 2, 3]] 的结果", out)
print("  推导式跑完后访问它的内部变量：", end="")
try:
    print("  y =", y)  # noqa: F821  故意引用「推导式里用过」的名字
except NameError as e:
    print(f"NameError: {e}")
print("→ 对比你写的 C：C 里 `for (int i = 0; ...)` 的 i 出了大括号就没了；")
print("  Python 的 for 循环**没有**这层作用域，变量留在函数/模块里；")
print("  而推导式有自己的作用域，泄漏不出来。谁泄漏谁不泄漏，见 comprehension_basics.py 第五节。")
