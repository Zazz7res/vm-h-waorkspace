# 第 05 章 · 函数与坑：`def` 只跑一次，调用才查名字

> 本章所有输出都是本机 `Python 3.14.4` 真跑出来的，一条都没编。
> 跑法：`cd python-handbook/05-functions-and-pitfalls && bash demo.sh`（25 节，全跑一遍约 2 秒，跑完自动删沙箱）。
> 想自己拨开关：`bash demo.sh --keep`，然后进打印出来的那个 `/tmp/py05-XXXXXX` 里翻。
>
> 本章的每条命令都长这样：`$ python3 某个文件.py` 或 `$ python3 -c "..."`，
> 全部只读写 `mktemp -d` 出来的沙箱，不碰你的 `~/.config`、不联网、不装包。

---

## 第 00 节 · 本章在讲什么，以及为什么全章只钉两句话

前面四章把「解释器在干什么」「变量其实是标签」「字符串」「容器」四件事过了一遍。
到函数这一章，大多数人学到的是**一套口诀**：可变默认值别用、`*args **kwargs`、lambda 是匿名函数、
闭包就是环境变量、装饰器就是包装。这些口诀都对，但它们背后其实只有两句话：

> **第一句：`def` 这一行只跑一次。默认值的表达式、装饰器、函数体里 `def` 出来的名字，
> 全部在 `def` 那一刻定死；函数体里的每一行才是每次调用现跑的。**
>
> **第二句：名字是调用那一刻才查的。函数体里出现的每个名字，都按 LEGB 现找；
> 传参数只是「给那个名字做一次赋值」，所以你原地改对象外面看得见，重新赋值外面看不见。**

本章 25 节就是拿可运行的证据把这两句话钉死，再把它们的所有下游坑一次性排干净：

| 你想搞清楚的东西 | 本章用哪一节回答 | 一句话答案 |
|---|---|---|
| 函数到底是不是「东西」 | 第 2、3 节 | 是普通对象，能存能传能返能进 set；它身上的 `__defaults__`/`__closure__` 就是账本 |
| `def f(x=time.time())` 为什么三次一样 | 第 4 节 | 默认值的表达式属于 `def`，只算一次；账本 `__defaults__` 里那个数就是它 |
| `def f(xs=[])` 为什么越攒越多 | 第 5 节 | 默认值存的是**引用**不是快照，那个 `list` 全程序只有一个 |
| 这条坑会不会报错 | 第 5 节 | **一个字都不报**，代码按你写的做了，只是不是你想要的 |
| 三种正解哪种该用 | 第 6 节 | `None` 哨兵 / 关键字专用必填 / 工厂函数；判「没传」必须写 `is None` |
| 函数里改 `xs`，外面脏不脏 | 第 7 节 | 只有「原地改」脏；`xs = ...`、`xs + [...]` 一律看不见，`xs +=` 看得见 |
| `f(1, b=2)`、`f(*xs)`、`f(**d)` 哪种合法 | 第 8、9、10 节 | 位置在前关键字在后；写反是 `SyntaxError`，数量不对是 `TypeError` |
| `*` `/` `**` 六个符号各管什么 | 第 9 节 | 五种参数筐的顺序表，`/` 之前只能按位置，`*` 之后只能按名字 |
| `zip(*rows)` 为什么能转置 | 第 11 节 | `*` 是「摊开」：把行的 list 摊成 `zip` 的多个参数 |
| 「返回多个值」是怎么回事 | 第 12 节 | `return` 只交一个东西，多个值是**逗号**造的元组 |
| 闭包到底是什么 | 第 13 节 | 内层函数出门时带的那几个**盒子**（`cell`），共享、装当前值 |
| `[lambda: i for i in ...]` 为什么全是最后一个 | 第 14 节 | 晚绑定：`i` 是调用时才查的名字；三条修法都在「当场把值变成函数自己的」 |
| `UnboundLocalError` 凭什么 | 第 15 节 | 编译器只看「这个函数里有没有出现过对它赋值」，不看会不会执行到 |
| 为什么 `locals()` 写了不算 | 第 16 节 | 模块级是一个真字典（能写），函数级是帧里的槽位（`locals()` 只是快照） |
| `lambda` 什么时候该用 | 第 17 节 | 只是 `def` 的表达式版；一旦需要名字/文档/多条语句就换 `def` |
| `key=`、`map`、`filter`、`any/all` | 第 18 节 | 收的都是函数；`map/filter/zip` 是惰性且一次性的；`list.sort()` 返回 `None` |
| `partial`、`lru_cache` 在补什么 | 第 19 节 | 一个省参数，一个用「参数必须可哈希」换「不重复算」 |
| 递归能吃多少层 | 第 20 节 | 限额是解释器数的（本机 1000）；**尾递归不会变成循环**，深递归改循环 + 自己造栈 |
| 为什么局部变量比全局快 | 第 21、22 节 | 字节码里局部是数组下标（`LOAD_FAST`），全局是查名字（`LOAD_GLOBAL`）；实测倍率 |
| 装饰器是不是新语法 | 第 23 节 | 不是：`@X` ≡ `f = X(f)`，所以它在 `def` 那一刻就跑了一次 |
| 这些零件拼起来长什么样 | 第 24 节 | 一个「重试 + 记账 + memo」的装饰器，每一行都能回指到某节 |
| 怎么证明本章没留垃圾 | 第 25 节 | `find` 数 `__pycache__`/`*.pyc` = `0`，章目录只有源文件 |

**读法建议**：第 4、5、6、7 节是本章的地基和代价最大的坑，务必按顺序读；
第 8~12 节是「调用一个函数到底要写什么」，可以当参考页查；
第 13~16 节是作用域这条线，`UnboundLocalError` 和晚绑定都在这里，是很多人工作三年还在踩的一段；
第 17~20 节是工具箱；第 21~22 节是「为什么」和「值多少钱」；第 23、24 节把全章收口。
第 25 节是取证纪律，不信本章时可以跳到最后再看一遍。

> 本章和第 02 章是一对：那章说「赋值是给对象贴名字，不是拷贝盒子」，
> 本章讲的是这条规律在**函数边界上**的所有后果。第 06 章（类）里那些坑，
> 有一半（可变默认值、`self.x += 1`、方法查找）就是把本章的规律换了一层皮。

---

## 第 01 节 · 环境体检：本章结论只在这么一个解释器上验过

函数和作用的细节是**版本敏感**的：`/` 这个「仅位置参数」分隔符是 3.8 才有的，
`nonlocal` 是 3.0，报错文案（尤其「cannot access local variable」这句）这几年改过好几轮，
3.13 起官方还明确写了「函数里改 `locals()` 不被保证生效」。所以第一步照例把解释器身份钉死。

本章 21 个取证脚本 + `demo.sh`，demo 会把它们整份复制进沙箱再跑 —— 章目录里**不会**留下任何产物：

```
$ ls
a_func_object.py
b_default_once.py
c_mutable_default.py
d_default_fixes.py
demo.sh
e_arg_passing.py
f_param_kinds.py
g_param_errors.py
h_unpacking.py
i_return.py
j_closure.py
k_latebinding.py
l_scope.py
m_scope_dicts.py
n_lambda.py
o_higher.py
p_functools.py
q_recursion.py
r_bytecode.py
s_call_cost.py
t_decorator.py
u_capstone.py
[退出码 0]
```

解释器和它报出的自己：

```
$ python3 --version && python3 -c "import sys; print(sys.version)"
Python 3.14.4
3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]
[退出码 0]
```

本章只用四个标准库模块，一句话验齐：

```
$ echo "LC_ALL=$LC_ALL  PYTHONHASHSEED=$PYTHONHASHSEED"; python3 -c "
import sys, functools, inspect, dis, timeit
print(\"只用标准库这四个 + 内置：\", {\"functools\", \"inspect\", \"dis\", \"timeit\"} <= sys.stdlib_module_names)
print(\"前缀:\", sys.prefix, \"｜ 浮点精度:\", sys.float_info.epsilon)
"
LC_ALL=C.UTF-8  PYTHONHASHSEED=0
只用标准库这四个 + 内置： True
前缀: /usr ｜ 浮点精度: 2.220446049250313e-16
[退出码 0]
```

核数记在这儿，因为第 22 节的计时只能读倍率：

```
$ python3 -c "import os; print(\"os.cpu_count() =\", os.cpu_count())"
os.cpu_count() = 4
[退出码 0]
```

三条要点：

1. **`functools` / `inspect` / `dis` / `timeit` 全在 `sys.stdlib_module_names` 里** —— 也就是说
   `{"functools", "inspect", "dis", "timeit"} <= sys.stdlib_module_names` 这一行答 `True`，
   **你不需要装任何东西**。这三行判据本身值得抄走：任何时候想知道「这是标准库还是要 pip」，
   一行 `sys.stdlib_module_names` 就够了。
2. **`PYTHONHASHSEED=0`** 是 demo 显式设的。本章要看 `set`、`dict` 的打印顺序
   （第 5 节的 `{'a': 2, 'b': 1}`、第 11 节的解包结果），不固定种子的话同一份代码两次跑顺序可能翻，
   本章就没法拿它当证据。
3. **`os.cpu_count() = 4`**：第 22 节所有的绝对纳秒数都受这台机器当时的 CPU 占用影响，
   **只有倍率可读**（脚本里已经写了：`timeit` + 3 轮取最快）。

---

## 第 02 节 · 函数是个对象：`def` 造了什么、它身上有哪些身份证

先把最反直觉的一条放前面：**`def` 不是「声明一个函数」，它是「执行一条赋值」。**
执行完，内存里多出来的是一个**普通的、可变、可哈希、能改进自己**的对象，
然后把名字 `greet` 贴上去 —— 和 `x = 1` 是同一类语句，只是造出来的东西复杂一点。

这个对象身上挂着一串属性，本章后面每一节都在翻这几本账，所以先把清单看全：

```
$ python3 a_func_object.py
=== 1. 函数是个对象：type 打得出来、callable 认得出 ===
type(greet) = <class 'function'>
greet 是实例吗（isinstance(greet, object)）： True
callable(greet) = True
greet.__class__.__name__ = function
调一下：greet('小明', 2) = 你好，小明！你好，小明！

=== 2. 函数对象身上的属性（本章后面全靠这几张身份证）===
  __name__ = 'greet'
  __qualname__ = 'greet'
  __module__ = '__main__'
  __doc__ = '跟 name 打 times 次招呼。'
  __defaults__ = (1,)
  __kwdefaults__ = None
  __annotations__ = {}
  __dict__ = {}
  __closure__ = None
  __code__ = code

=== 3. 函数对象能改进自己：__dict__ 是可写的（类字典是只读，对比第 06 章）===
  greet.__dict__ = {'version': 3, 'owner': '学习者'}
  加进去的属性直接能用：greet.version = 3 ｜ greet.owner = 学习者
  写 __dict__ 和 setattr 是一回事： True

=== 4. 一等公民的四条用法（存、传、返、比）===
  存进字典再调：table['g']('小红') = 你好，小红！
  当参数传进去：call_twice(greet, 'A') = ['你好，A！', '你好，A！']
  当返回值出来： True
  放进列表按下标调：[greet, str.lower][1]('MiXeD') = mixed
  能比：greet == table['g'] → True ｜ 和另一个函数比 → False
  能进 set（函数可哈希，默认按身份）：{greet, table['g'], str.upper} 的元素个数 = 2

=== 5. 同名 def 写两遍：名字被换走，旧对象没死（回指第 02 章「赋值是贴名字」）===
  first is greet → False
  新对象 greet('老王', 1) = [新版] 你好，老王
  旧对象被 first 拽着还能调：first('老王', 2) = 你好，老王！你好，老王！
  旧对象的 __doc__ 还在： '跟 name 打 times 次招呼。'
  del 掉名字 greet 之后，first 照样能调：first('收尾') = 你好，收尾！

=== 6. 一个反直觉的小账：def 跑一次 = 造一个新对象 ===
  a is b → False （同一段源码，两个对象）
  但它们的 __code__ 是同一个： True （字节码复用，包壳各造一个）
  函数对象的开销：每次 def 都新造一个壳，别在热循环里现写 def（第 22 节有计时）
[退出码 0]
```

三件事必须记住：

1. **函数是普通对象。** 能存进 dict、当参数传、当返回值交出来、当列表元素按下标调、
   能进 `set`（可哈希，默认按身份）。第 18 节 `key=` 收函数、第 23 节装饰器收函数，
   用的全是这一条。
2. **`def` 跑一次 = 造一个新对象。** 第 6 小段那个 `a is b → False` 就是这么来的：
   同一段源码写两遍就是两个函数对象，但它们的 `__code__` 是同一个 —— 字节码复用，壳各造一个。
   所以别在热循环里现写 `def`。
3. **名字被后一次 `def` 抢走，旧对象只要还有人拽着就活着。** 这就是第 02 章
   「赋值是贴名字」的原文重演，也是第 23 节 `f = X(f)` 的全部机制。

`__defaults__`、`__kwdefaults__`、`__closure__`、`__code__` 这四行是本章的仪表盘：
以后每一次「它为什么会这样」，答案都能在这四个属性上找到实物。

---

## 第 03 节 · 编译期的账本：`__code__` 里那几个 `co_` 字段与 `__annotations__`

上一节列了身份证，这一节把账本翻开。这些字段全是**编译期**就定好的，
也就是说：它们描述的是「你写的这段 `def` 长什么样」，和调用时传了什么无关。

```
$ python3 -c "
def make(base=10, *rest, tag=\"无\", **kw):
    def inner():
        return base + tag
    return inner
f = make()
code = make.__code__
print(\"外层 co_argcount（位置筐个数）=\", code.co_argcount)
print(\"外层 co_varnames（局部名字表）=\", code.co_varnames)
print(\"外层 co_cellvars（被内层盯上的名字）=\", code.co_cellvars)
print(\"内层 co_freevars（出门带的名字）=\", f.__code__.co_freevars)
print(\"内层 __closure__ 长度 =\", len(f.__closure__), \"｜ 盒子里装的是 =\", [c.cell_contents for c in f.__closure__])
print(\"外层的默认值账本 __defaults__ =\", make.__defaults__, \"｜ 关键字筐 __kwdefaults__ =\", make.__kwdefaults__)
"
外层 co_argcount（位置筐个数）= 1
外层 co_varnames（局部名字表）= ('base', 'tag', 'rest', 'kw', 'inner')
外层 co_cellvars（被内层盯上的名字）= ('base', 'tag')
内层 co_freevars（出门带的名字）= ('base', 'tag')
内层 __closure__ 长度 = 2 ｜ 盒子里装的是 = [10, '无']
外层的默认值账本 __defaults__ = (10,) ｜ 关键字筐 __kwdefaults__ = {'tag': '无'}
[退出码 0]
```

再看注解 —— 它在这一节只需要回答一个问题：「注解在运行时到底是什么」：

```
$ python3 -c "
def annotated(a: int, b: str = \"x\", *args: float, c: bool = False, **kw: object) -> None:
    return None
print(\"__annotations__ =\", annotated.__annotations__)
print(\"注解只是存起来的字典，不改运行行为：annotated(1) =\", annotated(1))
print(\"取出来用：__annotations__[a] is int →\", annotated.__annotations__[\"a\"] is int)
"
__annotations__ = {'a': <class 'int'>, 'b': <class 'str'>, 'args': <class 'float'>, 'c': <class 'bool'>, 'kw': <class 'object'>, 'return': None}
注解只是存起来的字典，不改运行行为：annotated(1) = None
取出来用：__annotations__[a] is int → True
[退出码 0]
```

四个字段用大白话对一遍（这一份对照表后面反复要用）：

| 账本字段 | 大白话 | 本章哪一节靠它 |
|---|---|---|
| `__defaults__` | 位置筐的默认值**元组**，从右往左对齐参数 | 第 4、5、6 节：坑本体就写在这里 |
| `__kwdefaults__` | 关键字专用筐的默认值**字典** | 第 9 节：`note='无'` 在这儿，不在 `__defaults__` |
| `co_cellvars` / `co_freevars` | 外层「被盯上的名字」/ 内层「出门带的名字」 | 第 13、14 节：闭包的两本对账 |
| `co_varnames` | 这一层的**全部局部名字**（参数 + 函数体里赋过值的） | 第 15 节：判「这个名字算不算局部」的铁证 |
| `__closure__` | 带进来的盒子本体（`cell` 对象元组） | 第 13、14 节：`cell_contents` 直接看到当前值 |

现抄的查账四行（**这几行不是捕获原文**，是本章反复用的取证动作）：

```python
f.__defaults__, f.__kwdefaults__          # 默认值到底是哪个对象（第 5 节的坑一眼看穿）
f.__closure__ and [c.cell_contents for c in f.__closure__]   # 闭包盒子里现在装的是什么
f.__code__.co_varnames                    # 编译器把哪些名字判成了局部（第 15 节）
inspect.signature(f)                      # 五种筐的完整形状（第 24 节用它自检）
```

> 注解那一段里 `'return': None` 那个值有点绕：`def ... -> None:` 存进字典的是**对象 `None` 本身**，
> 不是字符串。注解是第 10 章（类型注解与工具）的主题，本章只把「它在运行时只是一个字典、
> 解释器一行都不遵守」这条钉住 —— `annotated(1)` 少传参数照样跑就是证据。

---

## 第 04 节 · ★全章地基：`def` 只跑一次，默认值的表达式在 `def` 那一刻就被算掉了

这是本章最重要的一节，后面所有坑都是它的下游。先看实验：把一个**会变的表达式**当默认值，
然后用一个计数器证明它只被算了一次。

```
$ python3 b_default_once.py
=== 1. 用一个会变的表达式当默认值：它是几就是几，不再变 ===
def 跑完之后 built = 1 （表达式被调用了一次）
连调三次 with_default()： 1 1 1
三次之后 built = 1 ← 还是 1，说明函数体里**没有**再算默认值

=== 2. 拿时间做同一个实验：睡 0.3 秒也不会让默认值变新 ===
两次调用相等吗（==）： True ｜ 是同一个对象吗（is）： True
def 之后过了 0.3 秒，默认值还是那个旧时刻吗： True
证据在账本上：stamp.__defaults__ 里那个数 == 第一次调用的返回值 → True

=== 3. 账本可以直接改：__defaults__ 是个元组，赋值就换默认值 ===
改之前 with_default.__defaults__ = (1,)
改之后 with_default() = 777 ｜ 账本 = (777,)
（这招能救命：调试时想看「默认值到底是哪个对象」，直接 print f.__defaults__）

=== 4. def 那一刻算不出来，就在 def 那行报错（不是调用时）===
调一个还没定义的东西当默认值： NameError | name 'helper' is not defined
换成调用时才找名字就没事：
  （函数体里的名字是**调用时**才查的，这正和第 15 节 LEGB 对上）

=== 5. 默认值 vs 函数体里读全局：一个拍快照，一个每次现读 ===
改了全局 LIMIT 之后：pick() = 3 ｜ pick_live() = 99
  pick 的账本： (3,) ← 99 根本没进来

=== 6. 同一条规律的另外两个变体 ===
默认值写名字，拿到的是 def 那一刻绑定的对象： [1, 2]
之后往 BASE 里塞东西，默认值那个对象跟着变吗： [1, 2, 99] （同一个 list，当然跟着变）
  这行就是第 5 节那个坑的另一半：默认值不是快照**值**，是快照**引用**。

=== 7. 一句话收口 ===
判据：看这行代码属于 def 还是属于调用。
  属于 def（只跑一次）：默认值表达式、装饰器、函数体里 def 出来的名字绑定；
  属于调用（每次都跑）：函数体里的每一行、参数表达式的求值（f(make_default()) 每次都会 +1）。
对照：def 的默认值 1 次 vs 调用处传参每次都算： 跑完两次之后 built 从 2 变成 3
[退出码 0]
```

把这一节的规律抽出来，就是一张「属于 `def` 还是属于调用」的分类表：

| 写在哪 | 什么时候跑 | 本章证据 |
|---|---|---|
| 默认值的表达式（`def f(x=build())`） | **`def` 那一刻，一次** | 第 1、2 小段：`built` 停在 1，`stamp.__defaults__` 里那个数不变 |
| 装饰器（`@X`） | **`def` 那一刻，一次** | 第 23 节第 7 小段：还没调用，注册表已经满了 |
| 函数体里的每一行 | 每次调用 | 第 1 小段第 3 行之后 `built` 没动 |
| 函数体里的**名字** | 每次调用现查 | 第 4 小段：默认值里用未定义的名字当场 `NameError`，函数体里用则调用时才报 |
| 调用处传的参数表达式（`f(build())`） | 每次调用 | 第 7 小段对照：跑完两次 `built` 从 2 变 3 |

两个立刻能用的推论：

1. **`def` 那一刻算不出来，就在 `def` 那行报错。** 第 4 小段用 `exec` 造了一个
   `def f(x=helper())`，`NameError` 是在 `def` 那一行列出来的 —— 不是等你调用才炸。
   这条对写代码很友好：默认值里的错误永远是**导入期**暴露。
2. **想看「默认值到底是哪个对象」，直接 `print(f.__defaults__)`。** 第 3 小段现场把
   `__defaults__` 改成 `(777,)`，函数行为立刻跟着变 —— 它不是什么私有副本，就是你能改的元组。
   调试第 5 节那种坑时，这一行比打印十次返回值都快。

> 第 5 小段那条「默认值 vs 函数体里读全局」的对比（`pick() = 3` / `pick_live() = 99`）
> 是很多「明明改了配置为什么函数不生效」的答案：**默认值是快照，函数体里的名字是直播。**

---

## 第 05 节 · ★可变默认参数陷阱：现场复现（而且它一个字都不报错）

上一节说了默认值只在 `def` 时算一次。把这条套到 `[]` 上，就是 Python 面试八股里
最出名、也最容易「背了还会犯」的那条坑。这里不复述结论，直接跑：

```
$ python3 c_mutable_default.py
=== 1. 坑本体：三次调用，账越攒越多 ===
bad_append(1) = [1]
bad_append(2) = [1, 2] ← 1 为什么还在？
bad_append(3) = [1, 2, 3]
第四次调用之前，账本 bad_append.__defaults__ = ([1, 2, 3],)

=== 2. 换个名字调也一样：因为脏的是那个 list，不是名字 ===
alias(4) = [1, 2, 3, 4]
两条路拿到的是同一个容器：bad_append(5) == alias(5) → True
再印账本： ([1, 2, 3, 4, 5, 5],)

=== 3. 返回值就是那个默认容器本身：外面拿到的是活的引用 ===
mine = [1, 2, 3, 4, 5, 5, '新东西']
外面往 mine 里塞一个，账本也跟着变： ([1, 2, 3, 4, 5, 5, '新东西', '外面塞的'],)
下次调用带着这个脏底开始：bad_append('下一次') = [1, 2, 3, 4, 5, 5, '新东西', '外面塞的', '下一次']

=== 4. 报错是零：这就是它贵在哪 ===
整段跑到现在没有一条 TypeError/ValueError —— 代码「按写的做」了，只是不是你想要的。
典型生产事故形态：缓存函数、累积日志、递归收集器。下面这条就是递归版：
  第一次 collect(3) = [3, 2, 1]
  第二次 collect(2) = [3, 2, 1, 2, 1] ← 上一次的 3 还在，而且这次又攒了一遍
  账本： ([3, 2, 1, 2, 1],)

=== 5. 哪些默认值会中枪：只有「可变的」那几种 ===
dict 版三次： {'a': 2, 'b': 1} {'a': 2, 'b': 1} {'a': 2, 'b': 1}
set  版三次： [3] [1, 3] [1, 3]
bytearray 版三次长度： 1 3 4
账本们： ({'a': 2, 'b': 1},) ({1, 3},) (bytearray(b'xyyz'),)
连类本身都能当默认值（可变的是它管的东西，坑更大）：
  bad_class 三次： [1, 1, 1] [1, 1, 1] [1, 1, 1]

=== 6. 安全的一半：不可变默认值不会中枪 ===
head 两次： >>甲 >>乙
count_to 两次： 3 4
pair 两次： (1, 2) (3, 4)

=== 7. 唯一的例外：不可变的壳 + 可变的芯 ===
bad_inner 三次： ['a', 'b', 'c'] ['a', 'b', 'c'] ['a', 'b', 'c']
POISON 本体已经被写脏： (['a', 'b', 'c'],)
  判据修正：不是「默认值类型不可变就安全」，而是「**没有任何一条路径能原地改它**」才安全。
[退出码 0]
```

三个必须看清的地方：

1. **脏的是那个 `list`，不是名字。** 第 2 小段换了个名字 `alias` 调用，账本一模一样 ——
   因为 `alias` 和 `bad_append` 贴的是同一个函数对象，那个默认 `list` 也只有一个。
2. **返回值就是那个默认容器本身。** 第 3 小段：外面拿到 `mine` 之后往里塞一个，
   `__defaults__` 跟着变，下次调用直接带着这个脏底开始。也就是说
   **调用方能污染函数**，函数也在污染调用方，双向都漏。
3. **整段零报错。** 第 4 小段专门数了一遍：一个 `TypeError`/`ValueError` 都没有。
   这就是这条坑贵在哪 ——「代码按写的做了」和「代码按你想要的做了」是两件事，
   而解释器只保证前者。生产事故里最常见的三个形态本章都演了：缓存函数、累积日志、递归收集器
   （`collect(3)` → `[3, 2, 1]`，`collect(2)` → `[3, 2, 1, 2, 1]`）。

第 5、6、7 小段把「什么情况下会中枪」的判据收干净了：

- 会中枪：`list` / `dict` / `set` / `bytearray` / **任何自定义可变对象**（连类本身都能当默认值，
  它 `append` 到类属性上就是全局共享）。
- 不会中枪：`int` / `float` / `str` / `tuple` / `frozenset` / `bytes` / `None` / 函数对象。
- **唯一的例外要警惕**：第 7 小段那个 `POISON = ([],)` —— **不可变的壳 + 可变的芯**。
  元组本身改不了，但 `POISON[0].append(...)` 改得动，而默认值拿到的正是那同一个元组。
  所以判据要修正成：**「没有任何一条路径能原地改它」才安全**，不是「类型不可变就安全」。

---

## 第 06 节 · 三种正解：`None` 哨兵 / 关键字专用必填 / 工厂函数

坑认清了，这一节给三条出路，并说清各自什么时候用最合适。

```
$ python3 d_default_fixes.py
=== 正解 A：None 哨兵（最常用，也是标准库自己的写法）===
连调三次： [1] [2] [3]
账本里没有 list（None 是不可变的，随便共用）： (None,)
显式传表时照样能原地改（这是特性：调用方想收集就给一个）：
  good_append('A', mine) = ['A'] ｜ mine = ['A']
  good_append('B') 会不会污染 mine： ['B'] ｜ mine = ['A']
为什么必须写 `is None` 而不是 `if not box`：
  sloppy('新', given) = [1, 2, '新'] （这次运气好，given 非空）
  sloppy('新', empty_given) = ['新'] ｜ 传进来的 empty_given = []
  → 传进来的空表被**扔掉**了，调用方以为自己在收集，实际拿到的是另一个对象。

=== 正解 B：keyword-only 必填，逼调用者自己交出容器 ===
正常用法：strict_append(1, box=[]) = [1]
再调一次（新表）： [2]
想偷懒不传 box： TypeError | strict_append() missing 1 required keyword-only argument: 'box'
  → 坑变成**当场报错**，这是最好的结局：写代码的人立刻被拦下。

=== 正解 C：工厂函数（每调用一次现造一个），类里就是 default_factory ===
build() 三次： ['表头'] ['表头'] ['表头']
账本（存的是函数对象，不是产物）： (<class 'list'>, '表头')
传别的工厂也通：build(lambda: ["先放一行"]) = ['先放一行', '表头']
  同一招在 dataclass 里叫 field(default_factory=list)（第 06 章第 10 节实测过那条 ValueError）。

=== 什么时候不用改：默认值本来就该是「共享的那个」===
连续注册三个： ['a'] ['a', 'b'] ['a', 'b', 'c']
外部直接看那份注册表： ['a', 'b', 'c']
  → 共享是**意图**时，写清注释并接受它；坑只在于「你以为每次是新的」。

=== 自查这条坑的两条硬办法 ===
扫一遍三个函数，报出「默认值里有可变对象」的： ['with_list', 'with_dict']
  真跑时用 `fn.__defaults__` 一眼就能看到那个共享容器；
  代码评审时用 `grep -n 'def .*=\[\]' *.py`（或 ={} / =set()）也能一网打尽。
[退出码 0]
```

三条正解的取舍：

| 正解 | 写法 | 什么时候选它 | 代价 |
|---|---|---|---|
| **A. `None` 哨兵** | `def f(x, box=None): if box is None: box = []` | 默认情况最多、「想收集时才传表」的接口，也是标准库自己的写法 | 要多写两行；判据**必须**是 `is None` |
| **B. 关键字专用必填** | `def f(x, *, box)` | 你根本不想替调用方决定容器，逼他自己交 | 调用变啰嗦；好处是坑变成**当场 `TypeError`** |
| **C. 工厂函数** | `def f(x, make=list): box = make()` | 需要「每次现造」而且造的东西可能变 | 多一层调用；类里对应 `field(default_factory=list)`（第 06 章第 10 节） |

第 1 小段里那条 **`is None` vs `if not box`** 值得单独钉一遍，因为它是「正解写错半步」的常见形态：

```python
def sloppy(item, given=None):
    box = [] if not given else given   # 错：传进来的空 list 会被扔掉
    box.append(item)
    return box
```

调用方传了一个空表，本意是「请用这个表收集」，结果拿到的是**另一个对象**（捕获里那行
`sloppy('新', empty_given) = ['新'] ｜ 传进来的 empty_given = []` 就是这个事故现场）。
`if not x` 把 `[]`、`{}`、`0`、`""` 全判成「没传」。所以判「没传」只能写 `is None`。

最后一小段给了两条**自查**手段，日常能直接用：

- 跑起来查：`[f for f in funcs if any(isinstance(d, (list, dict, set, bytearray)) for d in (f.__defaults__ or ()) + tuple((f.__kwdefaults__ or {}).values()))]`
  —— 捕获里那条筛出的是 `['with_list', 'with_dict']`。
- 评审时扫文本：`grep -n 'def .*=\[\]' *.py`（再顺手扫 `={}`、`=set()`），一网打尽。

> 「什么时候不用改」那一小段也得记住：**共享是意图时，写清注释、接受它。**
> 坑只在于「你以为每次是新的」。注册表、插件列表这类东西本来就该是一份。

---

## 第 07 节 · 传参到底传的什么：一次赋值，贴名字；原地改才看得见

Python 既不叫「传值」也不叫「传引用」，官方说法是 **call by object / call by assignment**：
调用那一刻做的就是你写 `xs = 那个对象` 时做的事。这节用六种写法把这件事摊开。

```
$ python3 e_arg_passing.py
=== 1. 六种写法：函数体里那一行到底是什么操作，决定外面脏不脏 ===
函数              外面的表跑完后               返回值                   返回值是同一个对象吗
rebind          [1, 2, 3]             [9, 9, 9]             False
mutate          [1, 2, 3, 9]          [1, 2, 3, 9]          True
index_assign    [9, 2, 3]             [9, 2, 3]             True
concat          [1, 2, 3]             [1, 2, 3, 9]          False
augmented       [1, 2, 3, 9]          [1, 2, 3, 9]          True
pop_front       [2, 3]                1                     False

=== 2. 只有两条规律，上面六行全能推出来 ===
  名字 = anything        → 只动局部那个名字，外面一律看不见（rebind / concat）
  名字.append / [i]= / += → 动的是那个对象本体，外面全看得见（mutate / index_assign / augmented）
  判据就一个：这一行有没有出现「名字 =」这种单纯赋值。

=== 3. 传参没有拷贝：函数里那个名字和外面贴的是同一个对象 ===
  probe(OUTER) → True
  probe(sample)（另一个同名不同值的表）→ False
  所以「传引用」也不准确：传的是**那个对象的绑定**，一次赋值而已。
```

上面那张表是本章最该背下来的表之一，因为它把「脏不脏」这个模糊问题变成了
「函数体里那一行是什么操作」这个机械问题。第 2 小段给了推导它的两条规律：
**只要出现「名字 = 值」这种纯赋值，外面一律看不见；只要出现 `.append` / `[i] = ` / `+=`，
动的是对象本体，外面全看得见。**

最容易猜错的一格是 `augmented`：`xs += [9]` 看着像赋值（左边是名字），
其实它走 `list.__iadd__`，是原地扩展 —— 所以外面脏了，而且返回值和传进去的是同一个对象。

```
=== 4. 不可变对象：函数里怎么折腾都改不动外面（不是魔法，是没得改）===
  bump(x) = 11 ｜ x 还是 = 10
  s 里改第一个字符： TypeError | 'str' object does not support item assignment
  想要结果只能靠返回值：x = 10 → 接回来才变： x 现在是 11

=== 5. 不可变的壳 + 可变的芯：照样改得动 ===
  传进去的 container = (['干净'],)
  poke 之后 = (['干净', '脏了'],) ｜ 外面看同一个： (['干净', '脏了'],)
  外层是不是同一个对象： True （tuple 没被换，只是芯被改了）

=== 6. 想「不收脏」就在函数里自己拷贝一层（第 04 章那六级梯子在这儿用）===
  naive 版返回： [1, 2, 3, '尾巴'] ｜ 调用方的 shared 变成： [1, 2, 3, '尾巴']
  polite 版返回： [1, 2, 3, '尾巴'] ｜ 调用方的 shared2 没动： [3, 1, 2]
  浅拷贝只保护外层槽位：内层还是同一个对象（第 04 章 copy 那一节的结论，这里再验一次）
  改拷贝里的内层，原件跟着脏： [[3, 99], [1]]
```

第 3 小段那两行（`probe(OUTER) → True` / 换一个同名不同值的表就 `False`）是「传参没有拷贝」的
直接证据：函数里那个名字和外面贴的是**同一个对象**。所以「传引用」也不准确 ——
传的是那个对象的**绑定**，一次赋值而已，赋值完函数里那个名字就可以被重新贴走（`rebind` 那一行）。

第 4、5 小段是同一个规律的两面：不可变对象（`int`/`str`）想改只能造新的，所以函数永远改不动外面
—— 这不是魔法，是**没得改**；而「不可变壳 + 可变芯」（`(['干净'],)`）照样改得动。
注意 `str` 那条的报错原文：`TypeError: 'str' object does not support item assignment`，
这是很多人第一次写 `s[0] = 'X'` 时会撞上的。

```
=== 7. 可变/不可变在这里的**唯一**实际差别 ===
  不可变对象（int/str/tuple/frozenset/bytes/None）：想改只能造新的 → 函数永远改不动外面；
  可变对象（list/dict/set/bytearray/自定义类实例）：能原地改 → 函数改得动外面。
  所以第 5 节那个坑只会出现在可变默认值上，和第 15 节的作用域规律是同一族。
   list：能原地追加 → 函数改得动外面
   dict：根本没有 append 这个方法 → 只能造新的
   set：根本没有 append 这个方法 → 只能造新的
   bytearray：有 append，但这一类值不收（'str' object cannot be interpreted as an integer）
   tuple：根本没有 append 这个方法 → 只能造新的
   str：根本没有 append 这个方法 → 只能造新的
   int：根本没有 append 这个方法 → 只能造新的
   NoneType：根本没有 append 这个方法 → 只能造新的
   frozenset：根本没有 append 这个方法 → 只能造新的

=== 8. 一句话收口 ===
  函数改不改得动外面 = 你有没有原地改那个对象；
  函数返回值能不能救回来 = 你接不接。两条都和第 02 章「赋值是贴名字」同一条规律。
[退出码 0]
```

第 7 小段那张「九种类型能不能原地 append」的判定表值得看一眼，因为它把
「可变 vs 不可变」从背诵变成了可执行的判据（`list` 有 `append`、`dict`/`set` 根本没有这个方法、
`bytearray` 有 `append` 但只收整数 —— 那条 `'str' object cannot be interpreted as an integer`
就是传了字符串过去的现场）。

> 第 6 小段那条「浅拷贝只保护外层槽位」是第 04 章复制六级梯子的结论在这里的复现：
> `polite` 版不脏调用方，但改到内层，原件跟着脏。**想不收脏，就得在函数里自己拷一层**；
> 深拷贝的代价见第 04 章。

---

## 第 08 节 · 调用形态：位置、关键字、混着写，哪种合法哪种不合法

同一个函数，六种写法。先跑合法的：

```
$ python3 -c "
def two(a, b):
    return (a, b)
print(\"two(1, 2)          →\", two(1, 2))
print(\"two(a=1, b=2)      →\", two(a=1, b=2))
print(\"two(b=2, a=1)      →\", two(b=2, a=1), \"（关键字之间顺序无所谓）\")
print(\"two(1, b=2)        →\", two(1, b=2), \"（位置在前、关键字在后：合法）\")
print(\"two(*[1, 2])       →\", two(*[1, 2]))
print(\"two(**字典)        →\", two(**{\"a\": 1, \"b\": 2}))
"
two(1, 2)          → (1, 2)
two(a=1, b=2)      → (1, 2)
two(b=2, a=1)      → (1, 2) （关键字之间顺序无所谓）
two(1, b=2)        → (1, 2) （位置在前、关键字在后：合法）
two(*[1, 2])       → (1, 2)
two(**字典)        → (1, 2)
[退出码 0]
```

再看三种不合法的，注意它们分属两类错误：

```
$ python3 -c "
def two(a, b):
    return (a, b)
for label, src in [(\"关键字后面跟位置\", \"two(b=2, 1)\"), (\"同一个名字写两遍\", \"two(a=1, a=2)\")]:
    try:
        value = eval(src)
        print(label, \"→ 竟然跑通了：\", value)
    except SyntaxError as e:
        print(label, \"→ SyntaxError |\", e.msg)
try:
    two(1, 2, b=3)
except TypeError as e:
    print(\"同一个参数喂两遍 →\", \"TypeError |\", e)
"
关键字后面跟位置 → SyntaxError | positional argument follows keyword argument
同一个名字写两遍 → SyntaxError | keyword argument repeated: a
同一个参数喂两遍 → TypeError | two() got multiple values for argument 'b'
[退出码 0]
```

这一节的核心只有那条分工：

- **`SyntaxError` = 这一行根本没法编译。**「关键字后面跟位置」「同一个名字写两遍」
  都在写的时候就被判死，IDE、`python3 -m py_compile`、语法检查都能当场拦下。
- **`TypeError` = 编译过了但调用形状对不上。**「同一个参数喂两遍」这种要等到真跑起来
  才知道（因为参数是运行期才知道值的）。

第 10 节把两族的报错原文一次收齐，这里先把判据立住：**看到 `SyntaxError` 改写法，
看到 `TypeError` 改数量/名字。**

---

## 第 09 节 · 五种参数是同一条流水线上的五个筐（含 `/` 与 `*` 两个分隔符）

`def` 一行里最多能出现五种参数筐。这一节一次填满它们，再把编译器的账本翻出来对照。

```
$ python3 f_param_kinds.py
=== 1. 一次填满五个筐 ===
    salt（位置必填）    = 一勺盐
    oil（位置可默认）   = 一大勺油
    extras（多出的位置）= ('辣椒', '葱花') ｜类型 tuple
    sugar（关键字专用） = 两勺糖
    note（关键字带默认）= 无
    leftover（没对上的）= {'记录': 'A', '备注': 'B'} ｜类型 dict

=== 2. 只给必填的：其余全走默认/空筐 ===
    salt（位置必填）    = 一勺盐
    oil（位置可默认）   = 半勺
    extras（多出的位置）= () ｜类型 tuple
    sugar（关键字专用） = 两勺糖
    note（关键字带默认）= 无
    leftover（没对上的）= {} ｜类型 dict
    （extras 是空 tuple、leftover 是空 dict，**不是 None**）
```

第 3 小段那个「同一个参数两种写法效果一样」是 `POSITIONAL_OR_KEYWORD` 的定义；
而 `/` 左边那几个只能是位置，`*` 右边那几个只能是关键字。

```
=== 3. oil 既能按位置也能按名字：它是「位置或关键字」筐 ===
    salt（位置必填）    = 一勺盐
    oil（位置可默认）   = 按位置给的油
    extras（多出的位置）= () ｜类型 tuple
    sugar（关键字专用） = 糖
    note（关键字带默认）= 无
    leftover（没对上的）= {} ｜类型 dict
    salt（位置必填）    = 一勺盐
    oil（位置可默认）   = 按名字给的油
    extras（多出的位置）= () ｜类型 tuple
    sugar（关键字专用） = 糖
    note（关键字带默认）= 无
    leftover（没对上的）= {} ｜类型 dict
    同一个参数两种写法，效果一样 —— 除了 / 之前那几个（第 6 节现场看报错）。

=== 4. 整批传：*列表 灌位置筐，**字典 灌名字筐 ===
    salt（位置必填）    = 一勺盐
    oil（位置可默认）   = 一桶油
    extras（多出的位置）= ('花椒',) ｜类型 tuple
    sugar（关键字专用） = 三勺糖
    note（关键字带默认）= 无
    leftover（没对上的）= {'自定义键': 'Z'} ｜类型 dict
    args 前两个填 salt/oil，剩下的进 extras；字典整份进关键字筐。

=== 5. 关键字筐是**一个 dict**：能被 ** 展开，也能被塞进别的关键字参数 ===
  forward('盐', sugar='糖') 直接转发：
    salt（位置必填）    = 盐
    oil（位置可默认）   = 半勺
    extras（多出的位置）= () ｜类型 tuple
    sugar（关键字专用） = 糖
    note（关键字带默认）= 无
    leftover（没对上的）= {} ｜类型 dict
```

顺序口诀（背这一条就够，写错时按它改列）：

```text
位置必填 → 位置默认 → / → 剩下的位置 → *args → 关键字专用 → **kwargs
```

第 5 小段演示了实战里最常见的一招：**`**kwargs` 是一个 dict，所以能整批转发**
（`logged(name, **kwargs)` 里直接 `f(*args, **kwargs)`）。第 23 节的装饰器就是这么写的。

编译器把五个筐记在哪？这是本章后面反复要翻的一本账：

```
=== 6. 编译器的账本：这五个筐在函数对象上怎么记 ===
  co_argcount（位置筐总个数，含带默认的）= 2
  co_posonlyargcount（/ 左边的个数）    = 0
  co_kwonlyargcount（* 右边的个数）     = 2
  co_varnames（局部名字表，前几个就是参数）= ('salt', 'oil', 'sugar', 'note', 'extras', 'leftover')
  __defaults__（位置筐的默认值元组）    = ('半勺',)
  __kwdefaults__（关键字筐的默认值字典）= {'note': '无'}
  co_flags = 0xf → ['CO_OPTIMIZED', 'CO_NEWLOCALS', 'CO_VARARGS', 'CO_VARKEYWORDS']
  （CO_VARARGS/CO_VARKEYWORDS 两位亮起来，就说明这个函数有 *args / **kwargs）

=== 7. 五种筐的最小样本：一行一个，看清边界 ===
  只有位置必填             → 1
  带默认（可省）            → 2
  仅位置 /              → 3
  关键字专用 *            → 4
  *args + **kwargs   → ((5, 6), {'k': 7})

=== 8. 实战最常见的一种：*args, **kwargs 转发（写包装函数就靠它）===
    调用 add：位置=(1,) 关键字={}
    → 返回 11
    调用 add：位置=(1, 2) 关键字={}
    → 返回 3
    调用 add：位置=(1,) 关键字={'b': 3}
    → 返回 4
  → 有了 *args/**kwargs 转发，包装函数不需要知道被包函数的签名（第 23 节装饰器就这么写）。
[退出码 0]
```

三行补充：

1. **空筐是空容器，不是 `None`。** `*args` 没收到时是 `()`，`**kwargs` 没收到时是 `{}`。
   写 `if args is not None` 永远为真，要写 `if args`。
2. **`co_flags` 那两位（`CO_VARARGS` / `CO_VARKEYWORDS`）** 一亮，就表示这个函数有 `*args` / `**kwargs`。
   读第三方库源码时，这一行能一句话回答「它到底吃不吃额外参数」。
3. **`*args` 后面还能继续写名字**（第 7 小段的最小样本，以及第 10 节那条常被误判的写法）：
   那些名字从此只能按关键字传。这个形状在只读 API 里很好用 —— 强迫调用方写明参数名。

---
## 第 10 节 · 报错原文集：六种 `TypeError` + 九种 `SyntaxError`

上一节立了判据（`SyntaxError` 改写法、`TypeError` 改数量），这一节把两族的**原文**一次收齐。
以后撞上时你能一眼认出是哪一族，这比记忆报错文字有用得多。

### 调用形状对不上：六种 `TypeError`

```
$ python3 g_param_errors.py
=== 1. 调用形状对不上：六种 TypeError 原文 ===
  位置参数多给一个             → TypeError | two() takes 2 positional arguments but 3 were given
  必填少给一个               → TypeError | two() missing 1 required positional argument: 'b'
  名字不认识                → TypeError | two() got an unexpected keyword argument 'c'
  同一个参数喂两遍             → TypeError | two() got multiple values for argument 'b'
  关键字专用没给              → TypeError | kwonly() missing 1 required keyword-only argument: 'need'
  仅位置参数按名字给            → TypeError | pos_only() got some positional-only arguments passed as keyword arguments: 'a'

读法：前五条都是「数量/名字」问题；最后一条 pos_only 是**传法**问题 —— 同一个值换个写法就不报错（pos_only(1, 2) 正常）。
  对照： (1, 2) ｜ (1, 2) ｜ 多给 → pos_only() takes 2 positional arguments but 3 were given
```

前五条都是「数量 / 名字」问题，最后一条 `pos_only` 是**传法**问题：
同一个值换成按位置传就没事（`pos_only(1, 2)` 正常）。这条区分很重要 —— 它决定你是改调用还是改签名。

### `def` 那一行写错顺序：六种 `SyntaxError`

```
=== 2. def 那一行写错顺序：六种 SyntaxError 原文（用 compile 单独抓，不让它带走整个文件）===
  默认参数排在必填前面                 → SyntaxError | parameter without a default follows parameter with a default
  参数没有值（等号后面空着）              → SyntaxError | expected default value expression
  **kwargs 不是最后一位            → SyntaxError | arguments cannot follow var-keyword argument
  / 排在 * 后面                  → SyntaxError | / must be ahead of *
  两个 *args                   → SyntaxError | * argument may appear only once
  关键字专用排在 / 前面（顺序反了）         → SyntaxError | / must be ahead of *
  一个**合法**但常被误判的写法：*args 后面还能跟名字，只是它从此只能按关键字传：
    star_then_name(1, 2, b=3) = ((1, 2), 3) ｜ 不给 b 的名字： TypeError | star_then_name() missing 1 required keyword-only argument: 'b'
```

注意第 2 小段是用 `compile()` 单独编译抓的：`SyntaxError` 如果在正常脚本里出现，
它发生在**编译阶段**，会把整个文件一起带走，一行都跑不到。
这是本章取证的一个技巧，也是你调试时的一个常识 —— 一个语法错 = 全文件不执行。

顺带把那条「常被误判成写错了」的合法写法钉住：`def f(a, *args, b)` 里的 `b` 不但不报错，
还自动变成**关键字专用**参数（第 2 小段最后那行实测）。

### 调用处的语法错误 + `**` 展开的边界

```
=== 3. 调用处的语法错误（这三条也是 SyntaxError，不是 TypeError）===
  同一个关键字写两遍                → SyntaxError | keyword argument repeated: a
  位置参数排在了关键字参数后面           → SyntaxError | positional argument follows keyword argument
  def 里裸 * 后面什么都没有         → SyntaxError | invalid syntax

=== 4. ** 展开时字典里装了什么：三条边界 ===
  键是 int： TypeError | keywords must be strings
  键是带空格的字符串： TypeError | two() got an unexpected keyword argument 'b c'
  键多了个不认识的（能跑，进 **kwargs 才不报错；这里 two 没有 kwargs）： TypeError | two() got an unexpected keyword argument 'c'
```

第 4 小段这三条是 `**字典` 的边界：**键必须能当参数名**。
键是 `int` 时报的是那句很短的 `keywords must be strings`；键带空格、键多了不认识的，
都会变成 `got an unexpected keyword argument`。第 06 章 `**` 解包配置字典时天天遇到。

### 最阴的一条：不报错，但一定错

```
=== 5. 一个「能跑但一定错」的隐蔽形状：参数名和局部名撞了 ===
  shadowed('  去空格  ') = '去空格'
  这条不报错，但和第 15 节 UnboundLocalError 只差一个 if：
  almost 的局部表里有 value 吗： True
  （因为参数本来就是局部名字，所以参数名被自己遮蔽时**不会**报 UnboundLocalError，只会静悄悄变成参数原值 —— 比第 15 节那个更难查。）

=== 6. 收口：三类错误各自的修法 ===
  SyntaxError（def 行/调用行写坏）→ 按第 9 节那条顺序改列；
  TypeError（形状对不上）→ 看报错里的**名字**，它总告诉你缺的是哪个；
  静悄悄错（本节第 5 条）→ 只能靠 inspect.signature 或读 __code__.co_varnames（第 3 节的账本）。
[退出码 0]
```

第 5 小段是本章我最想让你记住的隐蔽坑：**参数名和函数体里的局部名撞了**。

```python
def almost(value):
    value = value.strip() if False else value   # 不报错，只是那行赋值永远不生效
    return value
```

因为参数本来就在局部名字表里，所以「遮蔽」它不会触发 `UnboundLocalError`（第 15 节那个），
只会**静悄悄拿参数原值**。查这种问题的手段只有两条：`inspect.signature` 对一遍，
或者读 `__code__.co_varnames`（第 3 节那本账）。

---

## 第 11 节 · 解包：`*` 和 `**` 的反方向用法，含 `zip(*rows)` 转置

同一个星号，在 `def` 行上是「收」，在调用和赋值左边是「摊」。这三种位置必须分开记。

```
$ python3 h_unpacking.py
=== 1. 一个符号，两个方向 ===
  不写星号：整个 list 当成**一个**参数塞进去
    args = ([1, 2, 3],) （tuple）
    kw   = {} （dict）
  写一个星号：摊成三个位置参数
    args = (1, 2, 3) （tuple）
    kw   = {} （dict）
  字典配两个星号：整份灌进关键字筐
    args = () （tuple）
    kw   = {'名字': '学习者', '层级': 1} （dict）
```

第 1 小段是全部内容的对照基准：不写星号是整个 `list` 当**一个**参数，
写一个星号是摊成三个位置参数，字典配两个星号整份灌进关键字筐。

```
=== 2. 解包用在任何可迭代对象上（不限 list）===
  max(*[3, 9, 1]) = 9 ｜ 对比 max([3, 9, 1]) = 9
  dist(*[0, 0, 3, 4]) = 5.0
  拆两段一起传：dist(*[0, 0], *[3, 4]) = 5.0
  字符串也能摊：probe(*'ab') →     args = ('a', 'b') （tuple）
    kw   = {} （dict）
  range 也能摊： [0, 1, 2, 3, 4]

=== 3. 星号接收（解包用法的第三种出现位置：赋值左边）===
  first, *rest = 1 [2, 3, 4] （rest 恒为 list）
  *a, last = ['h', 'e', 'l', 'l'] 'o'
  只剩空表时也照样是 list（不是 None）： 42 [] 0
  交换不用临时变量（右边先打包成元组再摊开）：p, q = 2 1
```

两个要点：`*` 能吃**任何可迭代对象**（`list`、`str`、`range`、生成器都行），
而且可以在一次调用里写好几段（`dist(*[0, 0], *[3, 4])`）。

第 3 小段那个「星号接收」是很多人只会用不会讲的东西，规则很硬：
**`rest` 恒为 `list`**（哪怕只剩一个元素、甚至是空 —— 空时是 `[]` 而不是 `None`），
一个赋值式里只能有一个裸 `*`。最后一行的交换 `p, q = q, p` 之所以能成立，
是因为右边先打包成元组再摊开。

### `zip(*rows)`：本章最实用的一招

```
=== 4. zip(*rows) 转置：本章最实用的一招 ===
  rows 有 3 行，每行 3 列
  zip(*rows) 展开成： [('名字', '代号', '年龄'), (18, 20, 22), ('北京', '上海', '广州')]
  想要「按列」遍历就靠它： [['名字', '代号', '年龄'], [18, 20, 22], ['北京', '上海', '广州']]
  手写等价式：zip(rows[0], rows[1], rows[2]) → True

=== 5. 字面量里的解包（3.5+）：合并容器不靠额外函数 ===
  [*a1, *a2, 5] = [1, 2, 3, 4, 5]
  {**{'x': 1}, 'y': 2} = {'x': 1, 'y': 2}
  set 里也行：{*a1, *a2} = {1, 2, 3, 4}
  3.9+ 的 | 也能合字典（第 04 章讲过 dict + 会 TypeError）： {'x': 1, 'y': 2}
  函数调用里传合并后的参数：dist(*[*[0, 0], *[3, 4]]) = 5.0
```

把「行」的列表摊开交给 `zip`，`zip` 收到的就是三个参数（三行），
它按位置横着收 —— 于是输出变成「列」。手写等价式那一行 `zip(rows[0], rows[1], rows[2]) → True`
就是在说这件事。你在第 04 章见过的「行式数据转列式」到这里就一句话：`zip(*rows)`。

```
=== 6. 解包只摊一层：槽位复制的是引用（第 04 章那条规律再验一次）===
  外层是两个不同的 list：copy is src → False
  内层是同一个 list：copy[0] is src[0] → True
  改 copy[0] 里面的东西，src 跟着脏： [[1, 99], [2]]

=== 7. 三个高频用途收尾 ===
  ① 打印时摊开：print(*rows, sep=';') → 1+2+3 ｜ ② 把可迭代对象变 tuple：tuple(zip(*rows)) 的元素数 = 3
  ③ 配置字典驱动调用：call_with_config(dist, {'x1':0,'y1':0,'x2':6,'y2':8}) = 10.0
[退出码 0]
```

第 6 小段那条要记住：**解包只摊一层，槽位复制的是引用**。
`[[1, 2], [2]]` 摊成新的外层 `list`，外层不是同一个对象（`is` 为 `False`），
但里层还是同一个（`copy[0] is src[0]` 为 `True`），改里层原件跟着脏 ——
这正是第 04 章「浅拷贝只保护外层」那条规律的再一次复现。

最后第 7 小段给了三个高频用途：`print(*rows, sep=';')`、`tuple(zip(*rows))`、
以及**配置字典驱动调用**（`call_with_config(dist, {'x1': 0, ...})` 里 `f(**config)`）。
第三条是从配置文件到函数参数之间最短的一座桥。

---

## 第 12 节 · `return` 的真相：没写就是 `None`，多个值是逗号造的元组

「Python 能返回多个值」是本手册最想拆掉的一句口诀。它不是错，但它把因果说反了。

```
$ python3 i_return.py
=== 1. 三种「没给返回值」的写法，结果都是 None ===
    （只打印，不 return） HI
  only_print 的返回值 = None （NoneType）
  bare_return(-1) = -1 ｜ bare_return(5) = None
  falls_off_end(0) = None ｜ falls_off_end(3) = 3
  → 所以 `if f(x):` 在 f 返回 None 时是假；想区分「没结果」和「结果为 0」必须显式返回。
```

三种「没给返回值」的写法 —— 不写 `return`、写裸 `return`、走到函数末尾 —— 结果都是 `None`。
后果就是 `if f(x):` 在「函数只打印不返回」时**永远为假**，
而 `bare_return(0)` 和「真的没有结果」这两件事在调用方看来一模一样。

```
=== 2. 「返回多个值」的真相：逗号造元组 ===
  不拆开接：packed = (3, 1) （tuple）
  拆开接：q, r = 3 1
  星号接剩余：q2 = 2 tail = [1]
  它俩完全等价：return (a, b) → False

=== 3. 逗号自己就会造元组（和函数没关系，回指第 02 章）===
  x = 1,      → (1,) （tuple）← 单元素元组必须带那个逗号
  y = 1, 2, 3 → (1, 2, 3)
  括号只是可选的：(1, 2, 3) == y → True
  函数返回三个值也是这一个 tuple： (1, 2, 3) 3
```

`return q, r` 里那个逗号是**造元组的运算符**，和函数没关系：第 3 小段现场给你看
`x = 1,` 就得到 `(1,)`（单元素元组必须带那个逗号，这条在第 02 章也钉过）。
所以准确说法是：**`return` 只交一个东西，这次交的是一个元组；接的时候拆开而已。**

```
=== 4. return 提前收工：后面一行都不跑 ===
  early(10) = 3 （0+1+2=3，走到 i==3 就交了）
  对比 continue 只跳过本次：
  skips(6) = [0, 2, 4]

=== 5. 返回值只能有一个「出口」：想同时给状态和数据就打包 ===
  parse('42') → 成功=True 值=42
  parse('四十二') → 成功=False 值=None
  反面写法（只返回数据，出错时返回 None）看着省一步，调用方就没法区分「值是 None」和「失败」：
    parse_lossy('0') = 0 ｜ 用 `if not x` 判断就完了（第 02 章假值那一节）

=== 6. 生成器的 return 是另一回事（第 10 章），这里只留个对照 ===
  type(gen(1)) = generator ｜ 它不是 tuple 也不是值，是个迭代器
  取出： 1 ｜第二次 next 抛 StopIteration，value = '收尾' （return 的值藏在异常里）
```

第 4 小段区分了 `return`（整个函数收工）和 `continue`（只跳过本次循环），
第 5 小段是本节的实用建议：想把「状态 + 数据」一起交出去，就**显式打包成元组或 dict**，
别用「出错时返回 `None`」那种省事写法 —— 因为 `parse_lossy('0') = 0` 和失败都是假值，
调用方用 `if not x` 判断就完了（第 02 章假值那一节在这里咬人）。

第 6 小段是生成器的对照（`return` 的值藏在 `StopIteration.value` 里）。

> 一处勘误要提前说清：这一小节脚本里打的那句「第 10 章」是本章脚本写作时的旧编号，
> 本书的生成器 / `map` / `filter` 在**第 04 章第 17 节**；捕获原文照录，不改。

```
=== 7. 收口 ===
  return 只管「交一个东西 + 结束函数」；几个值、返回什么形状，全是逗号和括号决定的。
[退出码 0]
```

> 第 06 章第 5 节那条「`__init__` 返回非 `None` 当场 `TypeError`」，用的正是本节的规律：
> `__init__` 被规定只能返回 `None`，你 `return self` 就是在返回一个「多出来的东西」。

---

## 第 13 节 · 闭包：内层函数出门时带的那几个盒子（`cell`）

「闭包」这个词吓人的程度和它的实际内容完全不成比例。内容就一句：
**内层函数用了外层的名字，那个名字就被装进一个盒子，跟着内层函数走。**

```
$ python3 j_closure.py
=== 1. 最小闭包：计数器 ===
  c1 三次： 1 2 3
  c2 独立（另一个盒子）： 1
  每个盒子归谁管：c1 和 c2 的 cell 是同一个吗 → False

=== 2. 编译器把闭包记在账本哪里 ===
  co_varnames（自己的局部表）= ()
  co_freevars（从外面带进来的名字）= ('count',)
  make_counter 那一侧：co_cellvars（被内层盯上的名字）= ('count',)
  __closure__ 是 tuple ，长度 = 1
  盒子里现在装的值：c1.__closure__[0].cell_contents = 3
  没有闭包名的函数 __closure__ 是 None：hit 之外看 greet 例子 → None
```

第 1 小段是两个计数器各自数各自的：`c1` 和 `c2` 是两次 `make_counter()` 的产物，
各自的盒子（`cell`）不是同一个（`is → False`），所以互不干扰。这就是闭包最常见的用途：
**给每个函数一份自己的私有状态**。

第 2 小段是账本对照，务必看清这三行：

- `co_freevars = ('count',)` —— 内层「出门带的名字」；
- `make_counter` 那侧的 `co_cellvars = ('count',)` —— 外层「被内层盯上的名字」；
- `__closure__` 是一个 **`tuple`**，长度等于带的盒子数，没带时是 `None`（不是空元组）。

`cell_contents` 这个属性名值得记：调试闭包时，它是唯一能直接看到「盒子里现在装了什么」的入口。

```
=== 3. 盒子是共享的，不是快照：外面改一下里面立刻看见 ===
  外层 def 之后又赋值，内层读到的是： 改过了

=== 4. 两个内层函数共用同一个盒子 ===
  plus() = 11 ｜ show() = 10
  两个函数的盒子是同一个： True

=== 5. 盒子可以被写（nonlocal），也可以被整个换掉 ===
  flip 四次： True False True False
  盒子里当前： False

=== 6. 不用 nonlocal 的等价写法：把账本装进可变对象（第 7 节那条规律）===
  三次： 1 2 3
  盒子里装的是那个 list： [3]
  两种写法对比：nonlocal 走「换内容」，可变对象走「原地改」；都能存状态，
  但 nonlocal 版本一眼能看出这里有共享状态，list 版本要看两遍 —— 可读性算账也要记。
```

第 3、4 小段合起来是一条：**盒子是共享的，不是快照。**
外层后来改了那个名字，内层立刻看见（因为它们是同一个盒子）；
两个内层函数也能共用同一个盒子（`plus` 改了，`show` 看到的就是新的）。
这半句是第 14 节那个坑的全部原因，先在这儿立住。

第 5、6 小段给出「往盒子里写」的两条路：

- `nonlocal` —— 换盒子里的内容（声明「这个名字是封闭层那个，不是我的新局部」）；
- 把账本装进可变对象（`box = []` 然后 `box.append`）—— 原地改，**不需要声明**。

两条都能存状态，脚本里那段对比值得琢磨：`nonlocal` 版本一眼能看出这里有共享状态，
`list` 版本要看两遍。**可读性也算账**，这是第 07 节「只有原地改才看得见」在闭包里的回声。

```
=== 7. 闭包 vs 类实例：同一件事的两种写法（回指第 06 章）===
  类版三次： 1 2 3
  闭包版三次： 1 2 3
  状态个数少、只要一个操作 → 闭包轻；状态多、操作多、要看一眼全貌 → 类。
  内存账：闭包函数对象 + 一个 cell vs 实例 + __dict__（第 06 章 10 万实例那张表可类比）。

=== 8. 一句话判据 ===
  看到「内层函数用了外层的名字」就先问：这个名字有没有被**赋值**？
    没赋值（只读）→ 直接读盒子；
    要赋值 → 必须 nonlocal（内层）或 global（模块级），否则就是第 15 节的坑。
[退出码 0]
```

第 7 小段把闭包和类实例摆在一起对了一次账（同一件事的两种写法，回指第 06 章）：
状态少、操作一个 → 闭包轻；状态多、操作多、要看全貌 → 类。
第 24 节的 capstone 就是这个对照的实战版（`attempts` 用闭包记，而不是用类）。

第 8 小段的判据请原样抄走：**看到「内层函数用了外层的名字」，先问这个名字有没有被赋值。**
没赋值 → 直接读盒子；要赋值 → 必须 `nonlocal`（封闭层）或 `global`（模块级），否则就是下一节的坑。

---

## 第 14 节 · ★晚绑定：`[lambda: i for i in range(3)]` 为什么三个都是 2

面试八股里和可变默认值并列的那一条。它的原因和第 4 节完全同源，修法也是。

```
$ python3 k_latebinding.py
=== 1. 现场复现（函数里、模块里都一样）===
  函数里造三个 lambda，跑出来： [2, 2, 2]
  模块级造三个 lambda，跑出来： [2, 2, 2]
  三个函数是同一个盒子的三个引用： True
  那个盒子现在装的： 2

=== 2. 为什么是 2：lambda 里那个 i 是「调用时才去查的名字」 ===
  对比第 4 节：默认值属于 def（当场算），函数体里的名字属于调用（事后查）。
  这条规律在 for 循环版上更明显：
  for 版跑出来： [2, 2, 2] ｜ 循环变量 i 还活着 = 2
  （for 的循环变量会泄漏到外面，推导式里的不会 —— 第 6 节专门量这个差别）
```

三个 lambda 是**同一个盒子的三个引用**（第 1 小段那个 `is → True`），
而那个盒子最后装的是 `2`。原因在第 2 小段那句对照里：

> 默认值属于 `def`（当场算一次），函数体里的名字属于调用（每次现查）。

`lambda: i` 里的 `i` 是函数体里的名字，所以三个 lambda 都在**被调用那一刻**去查同一个 `i`，
而那时循环早就跑完了，`i` 是最后一个值。`for` 版和推导式版结果一样，
差别只在「循环变量会不会泄漏到外面」（第 6 小段专门量）。

三条修法，本质都是**把当时的值变成每个函数自己拥有的东西**：

```
=== 3. 修法 A：默认参数拍快照（最短，最常见）===
  跑出来： [0, 1, 2]
  快照存在每个函数自己的账本里：三个 __defaults__ 分别是 [(0,), (1,), (2,)]
  有没有闭包盒子了： None
  原理：`i=i` 的右边属于 def 那一刻（第 4 节），把当时那个值塞进默认值，之后各不相干。

=== 4. 修法 B：functools.partial（语义最清楚）===
  跑出来： [0, 1, 2]
  partial 把值存在对象上： [(0,), (1,), (2,)]
  代价：拿到的是 partial 对象，不是函数（没有 __name__，第 19 节有账）。

=== 5. 修法 C：每轮新建一层闭包（工厂函数）===
  跑出来： [0, 1, 2]
  这次的盒子是三个不同的盒子： True ｜ 内容分别 = [0, 1, 2]
  关键：外层函数的局部名字 value 每调一次 make_getter 就新造一个盒子。
```

- **修法 A（`i=i` 默认参数）**最短也最常见：`i=i` 的右边属于 `def` 那一刻（第 4 节），
  当场把值塞进默认值。副作用是这三个函数**不再有闭包盒子**（`__closure__ → None`），
  三个 `__defaults__` 分别变成 `(0,) (1,) (2,)` —— 账本上能直接看到快照。
- **修法 B（`functools.partial`）**语义最清楚（一眼就知道「这个值是被固定住的」），
  代价是拿到的是 `partial` 对象而不是函数（没有 `__name__`，第 19 节有账）。

```
=== 6. 顺带把「泄漏」量清楚：for 泄漏、推导式不泄漏、class 体另算（第 06 章）===
  for 循环之外还能看到 leaked = 2
  推导式变量出不来： NameError | name 'comp_var' is not defined
  class 体里的循环变量在类里可见：Leaky.picked = 2
  但 class 体的名字不会进模块：'cls_var' 在 globals 里吗 → False

=== 7. 同一族坑的实战形态：回调/信号里现取循环变量 ===
  错法：handlers['a']() = b （本该是 'a'）
  修法：handlers_ok['a']() = a

=== 8. 判据一句话 ===
  闭包/lambda 里出现的每个名字，都是**调用那一刻**去查的；
  想要「那一刻之前的值」，就得在 def 行上把它变成默认参数，或者每次新建一层作用域。
[退出码 0]
```

- **修法 C（工厂函数，每轮新建一层闭包）**最通用：每次调用 `make_getter` 都会新造一个局部名字
  `value`，也就是新造一个盒子 —— 于是三个函数各有一个盒子，内容分别是 `0 1 2`。
  这一条和第 13 节第 1 小段那两个独立计数器是同一件事。

第 6 小段那三条「泄漏」实测是很多人没底的地方，直接列结论：

| 写法 | 循环/体内的名字在外面可见吗 | 捕获证据 |
|---|---|---|
| `for leaked in ...`（模块级） | **可见**，还是最后一个值 | `leaked = 2` |
| `[comp_var for comp_var in ...]` 推导式 | **不可见**，`NameError` | `name 'comp_var' is not defined` |
| `class` 体里的 `for` | 类里可见（`Leaky.picked = 2`），但**不进模块** `globals()` | 两条都实测了 |

第 7 小段是这条坑在真实项目里的样子：**回调/信号处理器里现取循环变量**
（`handlers['a']()` 返回了 `'b'`）。修法同上，用默认参数或工厂。

---

## 第 15 节 · LEGB 与 ★`UnboundLocalError`：编译器只看有没有赋值

名字查找的顺序是 **L**ocal → **E**nclosing → **G**lobal → **B**uiltins。
但这一节真正的重点不是顺序，而是那句话：**编译一个函数时，编译器只看「这个函数里有没有对它赋值」，
完全不看会不会执行到那一行。**

```
$ python3 l_scope.py
=== 1. 四层各有一个同名东西：看谁赢 ===
   函数级（L） / inner 读到：函数级（L） ｜ 外面这份还是： 模块级（G）
  内置那层（B）什么时候被读到：只在全局、封闭、局部都没有这个名字时，例如 len、print、int。
   4

=== 2. ★只读不算写，一赋值就变局部：UnboundLocalError 现场 ===
  bad_add： UnboundLocalError | cannot access local variable 'counter' where it is not associated with a value
  这条异常还能直接问它缺谁：e.name = None ｜ 它是 NameError 的子类吗 → True
  good_global 三次： 1 2 3 ｜ 模块级 counter = 3

=== 3. 编译器不看会不会执行，只看有没有赋值 ===
  never_runs： UnboundLocalError | cannot access local variable 'flag' where it is not associated with a value
  证据（账本，第 3 节那套）：flag 在 never_runs 的局部表里吗 → True
  只读的函数就不受影响：only_reads() = 全局值 ｜ 它的局部表 = ()
```

第 1 小段确认顺序（四层各放一个同名东西，函数级赢，且不改动外面那份）；
第 2 小段是本节的坑本体：

`bad_add` 里只写了 `counter += 1`，没写 `global counter`。
于是编译器看到「这个函数里对 `counter` 赋过值」，把 `counter` 判成局部名字；
运行时那行要读 `counter`，局部的槽位还没装过东西 —— `UnboundLocalError`。

两个细节值得记：

1. 这个异常是 `NameError` 的**子类**（`issubclass → True`），所以 `except NameError` 能接住它；
2. 报错文字在 3.11 之后改成了 `cannot access local variable 'counter' where it is not associated with a value` ——
   这句比老版本的 `local variable ... referenced before assignment` 友好得多，很多老教程里贴的原文已经不是本机原文。
   本章捕获里就是新文案。

```
=== 4. 内层要改外层的值：nonlocal（模块级用 global）===
  闭包里不写 nonlocal： UnboundLocalError | cannot access local variable 'total' where it is not associated with a value
  写了 nonlocal： 1 3 6
  nonlocal 不能指向模块级（那是 global 的活），这条连编译都过不去：
   SyntaxError | no binding for nonlocal 'counter' found
  反过来，global 也管不到封闭函数那一层（global 只认模块级），所以两种声明不能互换。

=== 5. 同一层里同名遮蔽：参数名、循环变量、推导式 ===
  param_and_loop([1,2,3]) 返回 = 2 （已经不是那张表了）
  换个名字就干净：cleaner([1,2,3]) = [2, 4, 6]

=== 6. class 那一层不在 LEGB 里（第 06 章那条规律在这边的样子）===
  from_module → 模块级
  from_class  → NameError | name 'shared' is not defined
  via_self    → 类里
```

第 3 小段是这条坑最阴的形态：赋值写在**永远跑不到的分支**里（`if False:`），
照样让这个名字整层变成局部。证据就在账本上：
`"flag" in never_runs.__code__.co_varnames → True`。
也就是说这是**编译期**的判定，和执行路径无关 —— 想验证任何「编译器怎么判的」，
`co_varnames` 都是铁证，第 3 节那本账在这儿兑现。

第 4 小段把跨层写的三种手段排了一遍：封闭层 `nonlocal`、模块级 `global`、对象属性 `self.x`（第 06 章）。
其中那条「`nonlocal` 不能指向模块级」是**编译期** SyntaxError（`no binding for nonlocal 'counter' found`），
这一条是脚本用 `compile()` 单独抓的 —— 写在正常文件里整个文件都不会执行。
反过来 `global` 也管不到封闭函数那一层，两种声明**不能互换**。

```
=== 7. 判据三句话 ===
  ① 读名字：L→E→G→B，class 那一层**不算一层**；
  ② 这个函数里只要出现过 `名字 = `、增量赋值、for/with/def/import 的绑定目标，
     这个名字整层就是局部（编译期判定，不看执行路径）；
  ③ 想跨层写：模块级 global、封闭层 nonlocal、对象属性 self.x（第 06 章第 17 节那个遮挡物）。
[退出码 0]
```

第 5 小段（参数名被循环变量遮蔽：`param_and_loop([1,2,3])` 返回 `2`）是第 10 节第 5 小段那个
「不报错但一定错」的又一形态；第 6 小段那条 `class` 那一层**不在 LEGB 里**是第 06 章反复强调的规律，
这里的证据更直接：类方法里裸写 `shared` → `NameError`，只有 `self.shared` / `Cls.shared` 拿得到。

三条判据（本节原话，抄走）：

1. 读名字：L→E→G→B，**`class` 那一层不算一层**；
2. 这个函数里只要出现过 `名字 = `、增量赋值、`for`/`with`/`def`/`import` 的绑定目标，
   这个名字整层就是局部（**编译期判定，不看执行路径**）；
3. 想跨层写：模块级 `global`、封闭层 `nonlocal`、对象属性 `self.x`。

---

## 第 16 节 · 作用域其实是字典：`globals()` 能写、`locals()` 写了不算

第 15 节说「编译器把名字分到槽位里」，这一节解释另一半：**模块级是一个真字典，函数级不是。**
这条不对称解释了三件事：`exec`/`eval` 怎么注入变量、调试器为什么要特殊手段、
以及为什么 `locals()` 只能用来「看」。

```
$ python3 m_scope_dicts.py
=== 1. 函数里的 locals() 是一份快照 ===
    locals() 的键 = ['local_a', 'local_b']
    'module_var' 在 locals 里吗 → False ｜ 在 globals 里吗 → True
    globals() 里能直接取到模块级那份： 模块级
  函数返回后拿到的那份： ['local_a', 'local_b'] ｜ 它还能读，但改它不会影响任何真实变量

=== 2. 往 locals() 里写：不创建变量 ===
   NameError | name 'injected' is not defined
  （3.13 起官方明确：函数里对 locals() 的修改**不被保证生效**，别依赖它。）

=== 3. 往 globals() 里写：真生效 ===
  直接按名字读： 字典写出来的模块级名字
  也能覆盖已有的：先读 = 模块级 ｜ 改完再读 = 被字典改过了
  这就是 exec/eval 注入变量的通道（第 09 章 import 机制也走这条路）。
```

三个小段一条线：函数里的 `locals()` 是一份**快照**（第 1 小段：函数返回后你手里那份还能读，
但它和真实的栈帧已经没有关系）；往里写**不创建变量**（第 2 小段那条 `NameError`）；
`globals()` 里写则**真生效**（第 3 小段，包括覆盖已有名字）。

```
=== 4. 模块级那两层其实是同一个字典 ===
  locals() is globals()（模块帧）→ True
  sys.modules[__name__].__dict__ is globals() → True
  vars() 不带参数 = locals()：vars() is locals() → True
  vars(模块) 拿到的是那个 __dict__： dict

=== 5. 谁住在模块字典里（本章文件自己的账本，节选）===
  本文件里非双下划线的名字共 7 个，前 12 个： ['made_by_dict', 'module_var', 'show_snapshot', 'snap', 'sys', 'write_into_globals', 'write_into_locals']
  函数对象也在同一层：'show_snapshot' in globals() → True
  del 掉名字，对象才可能回收（第 2 节那个 first 拽着旧函数就是同一件事）：
    赋值前后：'temp' in globals() → True
    del 之后：'temp' in globals() → False

=== 6. eval/exec 的两个字典参数：这就是「人为造一层作用域」 ===
  eval 收不到外面的局部名（因为局部名不在字典里，在帧里）：
     42
  但 eval 能收到全局名： 被字典改过了
  把局部值显式喂进去就行： 42
  exec 传第二个参数＝给它一个独立字典，不会污染本模块：
    sandbox 的键 = ['__builtins__', 'f'] ｜ 调用： 只在沙盒里
    'f' 有没有漏进本模块 globals → False
```

第 4 小段那三行 `is` 比较是本节的精华，把「模块级 = 一个字典」钉死：

```python
locals() is globals()                      # 模块帧里 True —— 同一本字典
sys.modules[__name__].__dict__ is globals() # True —— 模块的 __dict__ 就是它
vars() is locals()                          # True —— vars() 不带参数就是 locals()
```

第 5 小段顺手把「谁住在模块字典里」数了一遍（本文件非双下划线的名字 7 个）。
注意最后那条：`del` 掉名字，对象才可能被回收 —— 这正是第 2 节里 `first` 拽着旧函数不放的另一面。

第 6 小段是这套机制最实用的地方：

- `eval` **收不到**外面的局部名（因为局部名在帧里、不在字典里），但能收到全局名；
- 想让它看见局部值，就**显式喂进去**（`eval("x * 2", {"x": v})`）；
- `exec` 传第二个参数（一个独立字典）= 给它一个沙箱，**不会污染本模块**
  （第 6 小段实测：`'f'` 没漏进 `globals()`）。

这三条就是第 09 章 `import` 机制的底层通道：`import` 做的事情之一就是把名字写进模块字典。

```
=== 7. 一句话收口 ===
  模块级 = 一个真字典（能读能写能注入）；函数级 = 帧里的槽位（locals() 只是快照，写不进去）。
  第 15 节的 UnboundLocalError 是「编译期把这个名字分给了局部槽位」，
  这一节解释了为什么运行时也没法用字典改回来。
[退出码 0]
```

一句话收口：**模块级 = 一个真字典（能读能写能注入）；函数级 = 帧里的槽位（`locals()` 只是快照）。**
所以「运行时想给函数注入一个局部变量」这条路根本不存在 —— 第 15 节的 `UnboundLocalError`
是编译期分槽位，这一节解释了为什么运行期也没法用字典改回来。

---

## 第 17 节 · `lambda`：`def` 的表达式版，没有别的神通

先给结论：`lambda` 是**表达式**版的 `def`，仅此而已。它造出来的对象和 `def` 造的是同一类东西。

```
$ python3 n_lambda.py
=== 1. lambda 是个对象，和 def 造的完全同类 ===
  type(square) = <class 'function'>
  square(7) = 49 ｜ square_def(7) = 49
  __name__ 分别是： '<lambda>' 和 'square_def'
  __code__.co_name： '<lambda>' 和 'square_def'
  签名信息也在账本上：co_varnames = ('x',) ｜ __defaults__ = None

=== 2. lambda 能用 def 能用的全部参数语法 ===
  带默认值： 30
  收 *args/**kwargs： ((1, 2), {'k': 3})
  仅位置参数： 10
  关键字专用： 42
  嵌套（闭包）： 42
```

第 1 小段用身份证清单证明「同类」：`type` 都是 `<class 'function'>`，
`__code__.co_varnames`、`__defaults__` 一应俱全 —— 唯一区别是 `__name__` 是 `'<lambda>'`。
第 2 小段更重要：**`lambda` 能用 `def` 的全部参数语法**（默认值、`*args/**kwargs`、
仅位置 `/`、关键字专用 `*`、闭包），别以为它只能写 `lambda x: x`。

```
=== 3. 唯一的语法限制：里面只能有一个表达式 ===
  赋值语句           → SyntaxError | cannot assign to lambda
  import 语句      → SyntaxError | invalid syntax
  return 关键字     → SyntaxError | invalid syntax
  带分号的复合         → SyntaxError | invalid syntax
  想「表达式的副作用」可以用三元/元组/推导式绕，但一旦绕就说明该写 def 了：
   5 ← 这种三元是合法的（它是表达式）

=== 4. 赋值给名字的代价：traceback 里那一行变成 <lambda> ===
  lambda 版的最深那一帧： f_lambda = lambda x: 1 / 0       # 故意的：只看报错里那一行长什么样
    对应源码行： ~~^~~
  def 版的最深那一帧： return 1 / 0
    对应源码行： ~~^~~
  区别就在帧名上：一个写 in <lambda>（三个 lambda 长得一模一样），一个写 in named。
```

唯一的限制就是「里面只能有一个表达式」。第 3 小段收了四种撞墙原文，
其中 `cannot assign to lambda` 是这一族独有的。最后一行那句提醒很有用：
**一旦你需要用三元/元组去绕「副作用」，就说明该写 `def` 了。**

第 4 小段是「别把 `lambda` 赋值给名字」这条规矩的全部理由：
traceback 里那一帧写的是 `in <lambda>`，而源码行也只是 `lambda x: 1 / 0` ——
三个 lambda 长得一模一样，出事后你分不清是哪个。赋给名字的 `def` 版则直接给你 `in named`。

```
=== 5. lambda 真正合适的场合：一次性、当参数交出去的小函数 ===
  排序按第二项： [('香蕉', 1), ('桃子', 2), ('苹果', 3)]
  换成 def 版：
   [('香蕉', 1), ('桃子', 2), ('苹果', 3)] ← 结果一样，只是要多写三行
  赋值给名字（不推荐）： ['a', 'b']
  同一件事的推导式写法（更推荐，第 04 章讲过）： ['a', 'b']

=== 6. 一句话判据 ===
  lambda 只在「这里要一个函数、而且就一行」时用；
  一旦它需要名字、需要文档、需要多条语句、需要出现在 traceback 里被认出，就换 def。
[退出码 0]
```

第 5 小段给了它真正的用武之地：`sorted(key=lambda t: t[1])` 这种
「一次性、当参数交出去的小函数」。同一段最后还给了「推导式更合适」的对照
（要造一个新列表时用推导式，而不是 `list(map(lambda ...))`）。

判据一句话：**需要名字、需要文档串、需要多条语句、需要出现在 traceback 里被认出 —— 就换 `def`。**

---

## 第 18 节 · 把函数当参数交出去：`sorted(key=)`、`map`/`filter`、`zip`、`any`/`all`

这一节是第 2 节「函数是一等公民」的应用现场。三条最容易记错的先放在前面：
**`key` 收的是函数；`map`/`filter`/`zip` 是惰性且一次性的；`list.sort()` 返回 `None`。**

```
$ python3 o_higher.py
=== 1. key= 收函数：同一个表四种比法 ===
  默认按整个元组（比第一项，再比第二项…）： [('桃子', 2, 'pink'), ('苹果', 3, 'red'), ('香蕉', 1, 'yellow')]
  按第二项（数量）： [('香蕉', 1, 'yellow'), ('桃子', 2, 'pink'), ('苹果', 3, 'red')]
  按第三项长度： [('苹果', 3, 'red'), ('桃子', 2, 'pink'), ('香蕉', 1, 'yellow')]
  内置函数直接当 key： ['香蕉', '苹果', '桃子']
  降序： [('苹果', 3, 'red'), ('桃子', 2, 'pink'), ('香蕉', 1, 'yellow')]
  稳定排序：数量相同的不改原顺序（第 04 章 Counter 那条「同计数按插入顺序」同源）：
    sorted(同数量) = [('丙', 0), ('甲', 1), ('乙', 1)]
  key 也可以是有名字的函数： [3, 2, 1]
  坑点：key 里写 `t[1]` 的方括号是取值，写 `key=1` 就 TypeError（要的是函数不是值）：
    TypeError | 'int' object is not callable
```

`key=` 收一个「元素 → 用来比的值」的函数。第 1 小段用同一张表演了四种比法，
顺手收了两个要点：**排序是稳定的**（数量相同的 `丙/甲/乙` 不改原顺序，
和第 04 章 `Counter` 那条「同计数按插入顺序」同源），以及那条坑：
`key=1` 会报 `TypeError: 'int' object is not callable` —— 因为它要的是函数，不是值。

```
=== 2. map/filter 是惰性对象，不是结果 ===
  m = map(double, [1,2,3]) —— 这一步一行都没跑
  type(m) = map
  问长度： TypeError | object of type 'map' has no len()
  list(m) 才开始算：
      （真的算了） 1
      （真的算了） 2
      （真的算了） 3
    → [2, 4, 6]
  再 list(m) 一次（已经空了）： [] ← map 是一次性的，第 10 章生成器同族
      （真的算了） 1
      （真的算了） 2
      （真的算了） 3
  推导式版没有这个问题： [2, 4, 6]
  filter 收「判真假的函数」，None 表示用元素本身的真假：
    filter(None, [0,1,'','a',[],[1]]) = [1, 'a', [1]]
    filter(lambda n: n % 2, range(6)) = [1, 3, 5]
```

第 2 小段把「惰性」演得很直白：`m = map(double, [1,2,3])` 那一步**一行都没跑**；
`len(m)` 直接 `TypeError: object of type 'map' has no len()`；
`list(m)` 才开始算，而且算完就空 —— 再 `list(m)` 一次得到 `[]`。
这就是「一次性」的实物，和第 04 章生成器那一节同族（旧编号在第 10 章，见第 12 节那条勘误）。

`filter(None, ...)` 那一行也是常考点：**`None` 表示用元素本身的真假来筛**，
所以 `filter(None, [0,1,'','a',[],[1]])` 得到 `[1, 'a', [1]]` —— 这是第 02 章假值表的一次现场调用。

```
=== 3. sorted vs list.sort：一个给新表一个给 None ===
  sorted(data) = [1, 2, 3] ｜ 原表 data = [3, 1, 2]
  data.sort() 返回 = None ｜ 原表变成 = [1, 2, 3]
  把 data.sort() 直接传给下一步 = 传了 None：
    TypeError | object of type 'NoneType' has no len()
  想要「排完还拿到表」： [1, 2, 3] 或者 (data.sort(), data)[1] 这种丑写法不如上一行

=== 4. zip 也是惰性的（第 04 章讲过），配 map 时最常踩 ===
  type(zip([1,2],[3,4])) = zip
  list(zip(...)) = [(1, 3), (2, 4)]
  长度不齐时按短的： [(1, 'a')]
  想按长的补默认值： [(1, 'a'), (2, '-'), (3, '-')]
```

第 3 小段是本节代价最大的一条坑：`data.sort()` **返回 `None`**，原地改 `data`。
所以 `for x in data.sort()`、`json.dump(data.sort())`、`len(data.sort())` 这类写法
都会把 `None` 传给下一步，报的是 `object of type 'NoneType' has no len()` 这种「莫名其妙」的错。
想要「排完还拿到表」就用 `sorted(data)`。这条和第 05 节（可变默认值）、第 07 节（原地改 vs 重绑定）
是同一条规律的三种外衣。

```
=== 5. any/all 收「判真假的函数」吗？不，收的是可迭代对象 ===
  any(n > 2 for n in nums) = True （有个真的就 True，且是惰性的）
  all(n > 0 for n in nums) = True
  any(nums) = True ｜ all([]) = True （空表：any=False / all=True，是数学约定）
  误写成函数会怎样：
    TypeError | any() takes exactly one argument (2 given)
  any/all 短路：第一个真的出现就不再往下算 —— 用生成器传参才有这效果：
    any(probe(n) for n in [1, 2, 3, 4]) = True ｜ 实际只算了 [1, 2, 3] 这几个

=== 6. 收口：什么时候用哪个 ===
  只是「取每个元素」→ 推导式（第 04 章）；需要「把已有函数原样套上去」→ map；
  需要「按条件筛」→ 推导式带 if 或 filter；判断「有没有/全都」→ any/all + 生成器（能短路）。
[退出码 0]
```

第 5 小段纠正一个高频误解：**`any`/`all` 收的是可迭代对象，不是函数**。
所以 `any(n > 2 for n in nums)`（生成器）合法，写成 `any(nums, ...)` 就是
`TypeError: any() takes exactly one argument (2 given)`。
另外两个要点：空表时 `any([]) = False`、`all([]) = True`（数学约定，`all` 是「没有反例」）；
以及**短路只在传生成器时发生** —— 第 5 小段用 `probe` 数了一遍，
`any(probe(n) for n in [1,2,3,4])` 实际只算了 `[1, 2, 3]`，如果传的是
`[probe(n) for n in ...]`（列表推导式）就会四个全算完。

第 6 小段那张「什么时候用哪个」的取舍表，抄走：
只是取每个元素 → 推导式；把**已有函数**原样套上去 → `map`；按条件筛 → 推导式带 `if` 或 `filter`；
判断「有没有 / 全都」→ `any`/`all` + 生成器（能短路）。

---

## 第 19 节 · `functools` 三件套：`partial`、`lru_cache`、`reduce`

```
$ python3 p_functools.py
=== 1. partial：固定一部分参数 ===
  square(9) = 81
  type(square) = partial
  square.func is power → True
  square.args = () ｜ square.keywords = {'exp': 2}
  没有 __name__： AttributeError | 'functools.partial' object has no attribute '__name__'

=== 2. 位置参数从左头塞进去，调用时的参数接在后面 ===
  cube = partial(power, 2) → cube(10) = 1024 （等价 power(2, 10)）
  调用时再给位置参数：cube(3, 4) → TypeError | power() takes 2 positional arguments but 3 were given
  关键字能覆盖固定的值：square(9, exp=3) = 729
  也能整批转发：functools.partial(power)(3, 2) = 9

=== 3. partial vs lambda：两条路，账不一样 ===
  结果一样： True
  partial 是对象不是函数（inspect 看到的是 __call__）；lambda 是真函数，有 __name__： '<lambda>'
  调试时 partial 少一层栈帧；写回调/注册表时两种都常见 —— 记住报错里名字会丢就行。
```

`partial` 就是「固定一部分参数，造一个新对象」。第 2 小段那条**位置参数从左头塞进去、
调用时的参数接在后面**是它最容易记反的地方：`partial(power, 2)` 固定的是**第一个**参数，
所以 `cube(10)` 等价 `power(2, 10)` 而不是 `power(10, 2)`；关键字参数则能被覆盖
（`square(9, exp=3) = 729`）。

第 1 小段那条 `AttributeError: 'functools.partial' object has no attribute '__name__'`
和第 14 节修法 B 提到的代价是同一件事：它不是函数。第 3 小段给了「`partial` vs `lambda`」的账：
结果一样，但 `partial` 少一层栈帧、`lambda` 有 `__name__`。

### `lru_cache`：用「参数可哈希」换「不重复算」

```
=== 4. lru_cache：给递归函数加记忆，看命中账本 ===
  fib(30) = 832040 ｜ 函数体真跑了 31 次（不加缓存是 269 万次级别）
  再要一次 fib(30) = 832040 ｜ 函数体又多跑了吗 → False
  命中账本： CacheInfo(hits=29, misses=31, maxsize=None, currsize=31)
  参数表： {'maxsize': None, 'typed': False}
  它是被包过的函数：type(fib).__name__ = _lru_cache_wrapper ｜ __name__ 还在吗 → fib
  cache_clear() 之后： CacheInfo(hits=0, misses=0, maxsize=None, currsize=0) ｜ 函数体跑的总次数（含上一轮）= 31

=== 5. lru_cache 的门票：参数必须可哈希（第 06 章 __hash__ 在这儿咬人）===
  传 list： TypeError | unhashable type: 'list'
  换 tuple 就行： 2 2 ｜ 账本 = CacheInfo(hits=1, misses=1, maxsize=128, currsize=1)
  自定义类没写 __eq__ 时默认可哈希（按身份），写了 __eq__ 就不可哈希 —— 第 06 章第 7 节那条：
    TypeError | unhashable type: 'WithEq'
  （这就是「写 __eq__ 忘 __hash__」最典型的爆炸点：缓存/集合/dict 键。）
```

第 4 小段是本章最漂亮的一个数字：`fib(30)` 不加缓存要跑 269 万次级别的函数体，
加了缓存**函数体只跑了 31 次**，`cache_info()` 直接给出
`CacheInfo(hits=29, misses=31, maxsize=None, currsize=31)`。
这三个数字（hits / misses / currsize）就是「你的缓存到底有没有在起作用」的仪表盘。
另外注意 `type(fib).__name__ = _lru_cache_wrapper`：它是被包过的函数，
但 `__name__` 还在（因为它内部用了 `wraps`，第 23 节讲这件事）。

```
=== 6. maxsize 会淘汰：演示「算了三次但缓存里只留两个」 ===
    square_cached(1) =       （算了） 1
1
    square_cached(2) =       （算了） 2
4
    square_cached(3) =       （算了） 3
9
    square_cached(1) =       （算了） 1
1
  最后账本： CacheInfo(hits=0, misses=4, maxsize=2, currsize=2)

=== 7. reduce：把二元操作折叠成一个值（第 04 章讲过 itertools，这条归 functools）===
  reduce(lambda a, b: a * b, [1,2,3,4]) = 24
  带初值（空表也不炸）： 100
  空表不带初值： TypeError | reduce() of empty iterable with no initial value
  实际项目里更常用 sum()/math.prod()/最大公约定制函数，reduce 读起来最费劲。
```

第 5 小段是本节和第 06 章的接口，也是最容易咬人的地方：**参数必须可哈希**。

- 传 `list` → `TypeError: unhashable type: 'list'`；
- 换成 `tuple` → 正常，账本立刻出现 `hits=1`；
- 自定义类**没写 `__eq__`** 时按身份可哈希，能进缓存；**写了 `__eq__` 忘 `__hash__`** →
  `TypeError: unhashable type: 'WithEq'`，和第 06 章第 7 节「进 set、当 dict 键」是同一条报错。

第 6 小段把 `maxsize` 的淘汰语义演清楚了：`maxsize=2` 时算 `1 2 3` 三个，再回来问 `1`
已经不在缓存里（`hits=0, misses=4, currsize=2`）。
所以缓存不是「越大越保险」，`maxsize=None` 才是全留（代价是永不释放，
第 20 节递归 + 缓存的组合里这点要考虑）。

```
=== 8. 收口 ===
  partial = 少写几个参数的语法；lru_cache = 用「参数可哈希」换「不重复算」；
  两者都是「造一个新对象包住原函数」，原函数不受影响（第 23 节装饰器讲的正是这个套路的语法糖）。
[退出码 0]
```

`reduce` 放在最后，是因为它在真实项目里最不该被用：可读性差，
`sum()` / `math.prod()` / 一个 `for` 循环通常都更直白。
这里只收它三条边界：带初值时空表不炸、不带初值时空表
`TypeError: reduce() of empty iterable with no initial value`。

收口那两行是全章的一条主线：**`partial` 和 `lru_cache` 都是「造一个新对象包住原函数」，
原函数不受影响。** 第 23 节的装饰器讲的就是这件事的语法糖。

---

## 第 20 节 · 递归：限额是解释器数的，尾递归不会变成循环

你在 C 里写递归遍历二叉树时，「能递归多深」是**栈大小**决定的（`ulimit -s`，通常 8 MB）。
Python 里改由**解释器数帧**决定：`sys.getrecursionlimit()`，本机默认 `1000`。

```
$ python3 q_recursion.py
=== 1. 默认限额与它的形状 ===
  sys.getrecursionlimit() = 1000
  depth(500) = 500 （离限额还远）
  depth(1500)： RecursionError | maximum recursion depth exceeded
  RecursionError 是谁的子类： ['RecursionError', 'RuntimeError', 'Exception', 'BaseException']

=== 2. 调高限额能跑更多层（但这是借来的栈空间，不是免费的）===
  新限额 = 4000
  depth(2000) = 2000
  跑完恢复 = 1000 （脚本里临时改完一定要改回去）

=== 3. 无限递归：解释器数到第几层喊停 ===
  最深跑到第 998 层就停了（限额 = 1000 ，还要扣掉外面已有的帧）
  报错原文 = maximum recursion depth exceeded
  注意：抛出来之后所有帧**瞬间退光**，except 块里再数栈就只剩外面那一两层了。
```

第 1 小段：`depth(500)` 好得很，`depth(1500)` 当场 `RecursionError`；
第 2 小段证明这个限额**可以调**（调到 4000 后 `depth(2000)` 跑通），
脚本里也照例把它**改回 1000** —— 临时改了不还原，是那种会把自己坑到的写法。
第 3 小段有个容易被忽略的细节：无限递归最深跑到第 **998** 层而不是 1000，
因为限额算的是**总帧数**，外面本来就已经有 `sys.setrecursionlimit` / `try` / `main` 那几层了。
还有一句要记住：**异常抛出后所有帧瞬间退光**，所以你在 `except` 块里再数栈，只剩外面那一两层。

```
=== 4. 尾递归不会被优化：累加版照样炸 ===
  sum_rec(100) = 5050
  sum_rec(100000) → RecursionError | maximum recursion depth exceeded
  循环版 sum_loop(100000) = 5000050000 （不吃栈）

=== 5. 「自己造栈」的模板：这就是你 C 里迭代遍历二叉树那套 ===
  walk(50) → 处理个数 / 栈峰值 = (50, 1)
  递归版同样规模要 50 层帧；这里栈峰值只有 1（线性结构）—— 换成树才是队列/栈的用武之地。

=== 6. 递归里最贵的其实是「每层一个新帧 + 局部变量表」===
  layers(5) 在最底层数到的帧数 = 7 （5 层递归 + 解释器和 demo 自带的那几层）
  depth.__code__.co_varnames = ('n',) ← 每层帧都要开这一份
  第 21 节 dis 里能看到这些名字全靠 LOAD_FAST/STORE_FAST 搬进搬出，层数一多就是真金白银。
  递归深度大时先问两句：能不能改循环？能不能带备忘录（第 19 节 lru_cache）？
```

第 4 小段是本节最重要的一条：**Python 没有尾调用优化。**
`sum_rec` 写成累加器形式（看起来是尾递归）照样在 100000 层炸，
而循环版 `sum_loop(100000)` 轻松给出 `5000050000`。
「尾递归在别的语言里会被编译器变成循环」这个预期在 CPython 上**不成立**，
这不是实现偷懒，是指明「要深迭代就显式用循环」的设计取向。

第 5 小段给了那个模板 —— **自己造栈**：

```python
def walk(n):
    stack, done = [n], 0
    while stack:
        cur = stack.pop()
        done += 1
        if cur > 1:
            stack.append(cur - 1)
    return done
```

这就是你在 C 里迭代遍历二叉树那套（`malloc` 一个栈、`push`/`pop`）的 Python 版。
捕获里那句 `walk(50) → 处理个数 / 栈峰值 = (50, 1)` 是它的账：50 个节点，栈峰值只有 1
（线性结构本来就不需要更多）；**换成树才是显式栈的用武之地**，那时峰值就是树高。

```
=== 7. 一句话收口 ===
  递归在 Python 里是「表达清晰」的工具，不是「性能」工具；
  深度可预测、几十到几百层 → 放心写递归；深度可能上千 → 直接上循环 + 显式栈。
[退出码 0]
```

第 6 小段把「递归贵在哪」说清楚：每层一个新帧，每层都要开一份局部变量表
（`layers(5)` 在最底层数到 7 帧 = 5 层递归 + 解释器和 demo 自带的几层；
`depth.__code__.co_varnames = ('n',)` 就是每层都要开的那一份）。
所以深度大时先问两句：**能不能改循环？能不能带备忘录（第 19 节 `lru_cache`）？**

判据一句话：**深度可预测、几十到几百层 → 放心写递归；深度可能上千 → 直接上循环 + 显式栈。**

---

## 第 21 节 · 字节码视角：局部变量是数组下标，全局是查名字，闭包开盒子

第 15、16 节说的「编译期分槽位」不是比喻。`dis` 把它摊开给你看：
同一份算法，变量写在不同层，生成的**指令族就不一样**。

```
$ python3 r_bytecode.py
=== 1. 三种写法对应三种指令（只看关键那几行）===
  局部累加（use_local）：
    STORE_FAST     total
    STORE_FAST     i
    STORE_FAST     total
    LOAD_FAST_BORROW total
  全局累加（global_accum）：
    LOAD_GLOBAL    GLOBAL_TOTAL
    LOAD_FAST_BORROW n
    STORE_GLOBAL   GLOBAL_TOTAL
    LOAD_GLOBAL    GLOBAL_TOTAL
  闭包累加（use_deref）：
    STORE_FAST     total
    LOAD_FAST_BORROW total
    LOAD_DEREF     captured
    STORE_FAST     total
    LOAD_FAST_BORROW total

=== 2. 指令计数（整份字节码，不看循环次数）===
  use_local   *_FAST=  3  *_GLOBAL=  1  *_NAME=  0  *_DEREF=  0  总指令=17
  global_accum *_FAST=  0  *_GLOBAL=  3  *_NAME=  0  *_DEREF=  0  总指令=7

=== 3. 闭包那一层：LOAD_DEREF ===
  use_deref  *_FAST=3  *_DEREF=1  总指令=19
  co_varnames（自己的局部表）= ('n', 'total', 'i')
  co_freevars（从外面带的名字）= ('captured',) ← 这些走 *_DEREF
  同一份算法，只是把 total 换成全局/闭包变量，指令就换一族：
    局部 → LOAD_FAST（数组下标）｜全局 → LOAD_GLOBAL（查字典，带缓存）｜闭包 → LOAD_DEREF（打开盒子）
```

三族指令的对应关系（这张表是这一节的全部）：

| 变量住在哪 | 读它的指令 | 实际动作 | 谁负责记账 |
|---|---|---|---|
| 局部（含参数） | `LOAD_FAST` / `STORE_FAST` | **数组下标**，编译期就点好数 | `co_varnames` |
| 模块全局 | `LOAD_GLOBAL`（模块内还有 `LOAD_NAME`） | **查字典**（带缓存） | `globals()` 那个 dict |
| 闭包 | `LOAD_DEREF` / `STORE_DEREF` | **打开盒子**（`cell`） | `co_freevars` / `__closure__` |

```
=== 4. 局部变量表是编译期就点好数的数组 ===
  co_argcount = 2 ｜ co_varnames = ('a', 'b', 'c', 'd', 'e')
  前 co_argcount 个是参数，后面全是函数体里被赋值出来的名字（c/d/e），一个都不多一个都不少
  co_stacksize（操作数栈峰值，也是编译期算的）= 2
  整份字节码（只有 8 条，逐行读一遍就懂「局部变量=数组下标」）：
    RESUME       
    LOAD_FAST_BORROW_LOAD_FAST_BORROW a, b
    BINARY_OP    +
    STORE_FAST   c
    LOAD_FAST_BORROW c
    LOAD_SMALL_INT 
    BINARY_OP    *
    STORE_FAST   d
    LOAD_FAST_BORROW d
    LOAD_SMALL_INT 
    BINARY_OP    -
    STORE_FAST   e
    LOAD_FAST_BORROW e
    RETURN_VALUE 
  对照 global_accum（把同一件事写在全局上）：
    RESUME       
    LOAD_GLOBAL  GLOBAL_TOTAL
    LOAD_FAST_BORROW n
    BINARY_OP    +=
    STORE_GLOBAL GLOBAL_TOTAL
    LOAD_GLOBAL  GLOBAL_TOTAL
    RETURN_VALUE 

=== 5. 一个真实推论：函数里少用全局、多用手递参数 ===
  global_list_sum() = 19999900000
  param_list_sum(LOOKUPS) = 19999900000
  两个函数体指令差异（循环头那一处）：
    global_list_sum    LOAD_GLOBAL=1  LOAD_FAST=0
    param_list_sum     LOAD_GLOBAL=0  LOAD_FAST=0
  循环头只查一次全局名，所以差距不大；**在循环体里反复读全局名**才真的贵（第 22 节量的是那种）。
[退出码 0]
```

第 4 小段那两份 `dis` 全文建议**逐行读一遍**，这是理解「局部变量为什么快」最短的路：

- `three(a, b)` 里 `c`、`d`、`e` 全走 `STORE_FAST` / `LOAD_FAST_BORROW`，
  而且 `co_varnames = ('a', 'b', 'c', 'd', 'e')` —— 前 `co_argcount`（2）个是参数，
  后面全是函数体里被赋值出来的名字，**一个都不多一个都不少**，这就是那个数组；
- `global_accum(n)` 干的是同一件事（累加），但每一轮都要 `LOAD_GLOBAL GLOBAL_TOTAL` +
  `STORE_GLOBAL GLOBAL_TOTAL` —— 一次字典查找。
- `co_stacksize`（操作数栈峰值）也是编译期算出来的，这里等于 2，正好对应「`a b +` 那一瞬栈里两个值」。

第 5 小段给了一条**真实推论**：把全局值当参数传进函数会更快
（`param_list_sum(LOOKUPS)` vs `global_list_sum()`，循环头一个是 `LOAD_GLOBAL=1` 一个是 `0`）。
但脚本也诚实写了：这个例子里循环头只查一次全局名，所以差距不大 ——
**真的贵是在循环体里反复读全局名**，那种情形第 22 节量了。

> 3.11 之后 CPython 给全局查找加了专门的缓存，`LOAD_GLOBAL` 比老版本快了不少；
> 但「数组下标 vs 查字典」这个**相对**形状没变，所以本节结论仍然成立。
> 具体指令名（比如这里出现的 `LOAD_FAST_BORROW`、`LOAD_SMALL_INT`）是 3.14 的写法，
> 换个版本会变名，别把指令名当知识背，要背的是那三族的分工。

---

## 第 22 节 · 计时：一次调用值多少钱、包一层装饰器值多少钱

这一节的数字**每跑必抖**，只有倍率可读（口径写在脚本里：`timeit` + 3 轮取最快）。
下面这份是捕获那一刻的那一轮，绝对值别抄进任何报告。

```
$ python3 s_call_cost.py
=== 1. 一次空函数调用：这就是「把三行代码包成函数」的税 ===
  空函数 f()                        13.8 ns/次（跑了 500000 次，取 3 轮里最快的一轮）
  什么都不做的循环（对照）                    4.8 ns/次（跑了 500000 次，取 3 轮里最快的一轮）
  → 一次调用的税 ≈ 9.0 ns（2.9 倍）

=== 2. 参数个数与打包方式 ===
  f()（无参）                        13.7 ns/次（跑了 500000 次，取 3 轮里最快的一轮）
  f(x)（一个位置参数）                   15.9 ns/次（跑了 500000 次，取 3 轮里最快的一轮）
  f(x, y, z)                     18.6 ns/次（跑了 500000 次，取 3 轮里最快的一轮）
  f(*xs)（先解包 list）               41.0 ns/次（跑了 500000 次，取 3 轮里最快的一轮）
  f(**kw)（先解包 dict）              76.8 ns/次（跑了 500000 次，取 3 轮里最快的一轮）
  def f(*a, **k) 被直接调用           57.3 ns/次（跑了 500000 次，取 3 轮里最快的一轮）
  → 三个参数比一个贵 1.17 倍；**kw 解包比直接给关键字贵 4.82 倍（要现造 dict 再摊开）
```

先看清一件事：**一次函数调用的税只有约 9 纳秒**（空函数 13.8 ns vs 什么都不做的循环 4.8 ns）。
也就是说「为了可读性把三行代码包成一个函数」这件事，成本是纳秒级 ——
别为了省这点钱把逻辑糊成一坨。参数打包那边才是有形状的差异：
三个参数比一个贵约 1.17 倍，而 `**kw` 解包比直接给关键字贵约 4.82 倍（要现造 dict 再摊开）。

```
=== 3. 第 21 节那条规律的价钱：局部 vs 全局 vs 闭包 ===
  累加进局部变量                       816.1 ns/次（跑了 20000 次，取 3 轮里最快的一轮）
  累加进全局变量                      1517.6 ns/次（跑了 20000 次，取 3 轮里最快的一轮）
  累加进闭包变量（nonlocal）            1026.5 ns/次（跑了 20000 次，取 3 轮里最快的一轮）
  → 同一件事：全局比局部贵 1.86 倍，闭包比局部贵 1.26 倍（每轮多一次 LOAD/STORE_DEREF）

=== 4. 包一层装饰器的税（第 23 节讲的正是这个 wrapper）===
  直接调 plain(1)                   23.3 ns/次（跑了 500000 次，取 3 轮里最快的一轮）
  调一层 wrapper(1)                 35.2 ns/次（跑了 500000 次，取 3 轮里最快的一轮）
  → 多一层函数调用 ≈ 贵 1.51 倍；热循环里包三层就是三倍的税

=== 5. 内联 vs 拆函数：什么时候拆得不划算 ===
  sum(data) 内联                 3562.2 ns/次（跑了 2000 次，取 3 轮里最快的一轮）
  套一层自己的函数                     2617.5 ns/次（跑了 2000 次，取 3 轮里最快的一轮）
  → 这层封装的税 = -944.7 ns，相对总耗时只占 -26.5%（循环体够长时，函数边界这点钱根本不值一提）

=== 6. 怎么读这一节 ===
  ① 函数边界很便宜（几十纳秒级），**为了可读性尽管拆**；
  ② 真要抠的是循环体里的名字来源（局部最快）和每轮都做的解包；
  ③ 上面这些数字只能当**量级**看，别把小数位抄进任何报告 —— 换一台机器、换个 CPU 占用率就全变，但倍率形状基本稳定。
[退出码 0]
```

第 3 小段是第 21 节那条规律的**标价**：同一件累加的事，
全局比局部贵约 1.86 倍，闭包（`nonlocal`）比局部贵约 1.26 倍（每轮多一次 `LOAD/STORE_DEREF`）。
第 4 小段给装饰器标价：多包一层 wrapper ≈ 贵 1.51 倍 ——
**热循环里包三层就是三倍的税**，这就是第 23 节最后那条「别在热点上叠装饰器」的依据。

第 5 小段最有意思：给一个 1000 元素的循环套上自己的函数，
这层封装的税算出来是**负的**（-26.5%）。原因就在脚本的注释里：
内联那段是「在模块级跑一个 for」，而函数版本每次读 `data` 都是 `LOAD_FAST`。
所以真实结论不是「函数有开销所以别拆」，而是：

1. **函数边界很便宜（几十纳秒级），为了可读性尽管拆；**
2. **真要抠的是循环体里的名字来源（局部最快）和每轮都做的解包；**
3. 这些数字只能当**量级**看，倍率形状基本稳定，绝对值换台机器就全变
   （本机 4 核，第 1 节已记录）。

---

## 第 23 节 · 装饰器：`@X` 就是 `f = X(f)`，没有新语法

把这一章第 2 节那条「`def` 是赋值」和这一节连起来，装饰器就完全没有神秘感了：

> `@X` 那一行做的事，等价于在 `def` 块后面补一句 `f = X(f)`。
> 因为 `def` 只跑一次（第 4 节），所以**装饰器也只跑一次**，就在定义那一刻。

```
$ python3 t_decorator.py
=== 1. `@X` 只是 `hello = X(hello)` 的缩写 ===
  hello('world') = HELLO, WORLD
  名字 hello 现在绑的是谁： wrapper （wrapper，不是 hello 了）
  文档串也没了： None
  手写等价版：
    plain = shout(plain) → HELLO, WORLD ｜ 一模一样

=== 2. 被吞掉的不只是名字：一份对照表 ===
  装饰前        __name__=original     __doc__='原始说明。'        签名=(text)  模块=__main__
  装饰后        __name__=wrapper      __doc__=None           签名=(text)  模块=__main__
  → 名字、文档、签名全变成 wrapper 的了；调试时这是灾难，所以有下一节。

=== 3. functools.wraps 把身份证贴回来 ===
  wraps 后    __name__=fixed        __doc__='原始说明。'        签名=(text)  模块=__main__
  还多送一个反向指针：fixed.__wrapped__ is fixed → False ｜ __wrapped__.__name__ = fixed
  （functools.WRAPPER_ASSIGNMENTS = ('__module__', '__name__', '__qualname__', '__doc__', '__annotate__', '__type_params__') ）
```

第 1 小段两种写法（`@shout` 和手写 `plain = shout(plain)`）产出完全一样。
第 2 小段是一张**代价清单**：装饰后 `__name__`、`__doc__` 全变成 wrapper 的了 ——
签名看着还在（这里恰好同为 `(text)`），但文档串没了、名字变了。
**调试时这是灾难**：日志、traceback、`help()`、IDE 提示全都指着 `wrapper`。

```
=== 4. 带参数的装饰器 = 三层函数 ===
  roll('骰子') = ['掷 骰子', '掷 骰子', '掷 骰子']
  展开式：roll = repeat(3)(roll) —— 先调用 repeat(3) 拿到 deco，再用 deco 包函数
  分两步验证：
    repeat(3) 返回的是哪个函数： deco
    再拿它包一个函数：step2('一次') = ['掷 一次', '掷 一次', '掷 一次']

=== 5. 忘括号的两种现场 ===
  正确：@needs_args(2)
    ok() = 跑完了
  错误：把 @needs_args 直接挂上去（times 收到的是函数本身）：
    wrong() → TypeError | needs_args.<locals>.deco() missing 1 required positional argument: 'fn'

=== 6. 多个装饰器：离 def 近的**先**包（里层），远的在外层 ===
  @open_tag 在上、@bold_tag 在下 → <[hi]>
  反过来写 → [<hi>]
  执行顺序：装饰（自下而上）与调用（自上而下）是**两件事**，别混。
```

`functools.wraps` 就是修这张身份证的（第 3 小段）：`__name__`/`__doc__`/`__module__`/`__qualname__`
等一串名字从原函数贴回 wrapper。它还**白送一条反向指针** `__wrapped__`：

```python
fixed.__wrapped__          # 拿到被包住的原函数
fixed.__wrapped__.__defaults__   # 第 4 节那本账穿过 wrapper 也能翻到
```

第 4 小段是带参数装饰器的结构，一句话记法：**多写一层「收参数的函数」**。

```python
def repeat(times):        # 收装饰器的参数
    def deco(fn):         # 收被装饰的函数
        @wraps(fn)
        def wrapper(*a, **k):
            return [fn(*a, **k) for _ in range(times)]
        return wrapper
    return deco

@repeat(3)                # ≡ roll = repeat(3)(roll)：先 repeat(3) 拿到 deco，再用 deco 包
```

捕获里那个「分两步验证」把 `repeat(3)` 返回的确实是 `deco`、`deco` 包出来的确实是
能跑三次的函数，两步都实测了 —— 顺序搞混是这类装饰器最常见的 bug。
第 5 小段是这条规律的必然结果：**忘写括号**时 `times` 收到的是函数本身，
于是炸在 `deco() missing 1 required positional argument: 'fn'`。
看到这句报错就去找括号。

第 6 小段把叠多个装饰器的顺序钉死：**离 `def` 近的先包（里层），远的在外层**；
而**调用时**顺序相反（外层先进）。所以「装饰自下而上、调用自上而下」是两件事，别混。
`<hi>` / `[hi]` 那两行就是这一条的现场。

```
=== 7. 装饰器在 def 那一刻就跑了一次（呼应第 4 节默认值）===
  还没调用任何函数，注册表已经满了： ['tool_a', 'tool_b']
  这就是第 19 节 lru_cache、第 06 章 @property、@dataclass 能在「定义时」动手的原因。

=== 8. 用装饰器踩一遍本章所有坑的合体版 ===
  调用两次： [1, 2] [1, 2]
  日志： ['append_log(1,)[]', 'append_log(2,)[]']
  坑还在，而且现在连 __defaults__ 都得穿过 wrapper 才看得到： ([1, 2],)
  （wraps 帮你留了 __wrapped__ 这条后门。）
[退出码 0]
```

第 7 小段呼应第 4 节：**还没调用任何函数，注册表已经满了**（`['tool_a', 'tool_b']`）。
这就是第 19 节 `lru_cache`、第 06 章 `@property` / `@dataclass` 能在「定义时」动手的全部原因 ——
它们在 `def`/`class` 那一刻就被执行了。

第 8 小段是本章的收口实验：把可变默认值（第 5 节）和日志装饰器（第 23 节）叠在一起，
坑照旧存在，而且**现在连 `__defaults__` 都要穿过 wrapper 才看得到**。
调试这类代码时，`fn.__wrapped__.__defaults__` 是你的回马枪。

> 到这里，第 06 章里 `@property` / `@dataclass` / `@abstractmethod` 都褪掉了神秘感：
> 它们都是「在 `class` 语句执行时对名字做一次赋值」的函数，没有任何新机制。

---

## 第 24 节 · capstone：把本章零件装成一个真工具（重试 + 记账 + memo）

这一节不引入任何新语法，只把前面 23 节的零件拼成一个你在真实项目里会写的东西：
一个「失败重试 + 尝试次数记账 + 结果 memo」的装饰器。每一步都能回指到某节。

```
$ python3 u_capstone.py
=== 1. 先让工具自己交代签名（第 9 节的五个筐，一眼看全）===
  flaky 的签名：(threshold, *, note='无')
    threshold 类型=POSITIONAL_OR_KEYWORD    默认=必填
    note     类型=KEYWORD_ONLY             默认='无'
  retry 的签名：(times=3, exceptions=(<class 'ValueError'>,))
    times    类型=POSITIONAL_OR_KEYWORD    默认=3
    exceptions 类型=POSITIONAL_OR_KEYWORD    默认=(<class 'ValueError'>,)

=== 2. 重试：前三次失败、第四次成功 ===
    第 1 次失败：ValueError: 才第 1 次，还没到 4
    第 2 次失败：ValueError: 才第 2 次，还没到 4
    第 3 次失败：ValueError: 才第 3 次，还没到 4
  flaky(4, note='走网络') → 第 4 次成功（备注：走网络）
  累计尝试次数（装饰器自己记的账）： 4 ｜ 前两笔 = ['flaky', 'flaky']
  身份证还在吗：__name__ = flaky ｜ __wrapped__ = flaky
```

第 1 小段用 `inspect.signature` 让工具自己交代形状（第 9 节那五个筐一眼看全：
`threshold` 是 `POSITIONAL_OR_KEYWORD`、`note` 是 `KEYWORD_ONLY`）——
这是自检签名最短的一条路，比读 `co_*` 那堆字段更适合人看。
第 2 小段是主流程：前三次失败、第四次成功，`attempts` 那本账由装饰器自己用闭包记（第 13 节），
身份证也在（`__name__ = flaky`，`__wrapped__` 也在，第 23 节）。

```
=== 3. 重试全部失败：把最后一次的异常原样抛出去 ===
    第 1 次失败：ValueError: 才第 1 次，还没到 99
    第 2 次失败：ValueError: 才第 2 次，还没到 99
    第 3 次失败：ValueError: 才第 3 次，还没到 99
    第 4 次失败：ValueError: 才第 4 次，还没到 99
  抛回来了：ValueError | 才第 4 次，还没到 99 ｜ 一共试了 8 次

=== 4. partial 把「常用配置」定下来（第 19 节）===
    第 1 次失败：ValueError: 才第 1 次，还没到 3
    第 2 次失败：ValueError: 才第 2 次，还没到 3
  always_three() → 第 3 次成功（备注：无）
  partial 对象没有 __name__，但有 func/keywords： flaky {}

=== 5. memo：给纯函数加缓存（第 19 节 lru_cache 的手工版，顺便看清门票）===
  score('ab') = 8
  再来一次同样的： 8 8 ｜ 函数体跑了几次： 1
  传 list 当参数 → 缓存门票都没了：
    TypeError | unhashable type: 'list'
  账本： CacheInfo(hits=2, misses=1, maxsize=32, currsize=1)
```

第 3 小段是这条设计的关键取舍：**全部失败时把最后一次的异常原样抛出去**，
而不是吞掉或者换成一个新异常（异常链的正式话题在第 07 章）。
第 4 小段用 `partial` 把「常用配置」定成 `always_three`（第 19 节），
并顺手复现了那个代价：`partial` 对象没有 `__name__`。
第 5 小段是 memo：`lru_cache` 的手工版，`score('ab') = 8` 第二次直接命中
（函数体只跑 1 次），传 `list` 当参数则**连门票都没了** ——
`TypeError: unhashable type: 'list'`，第 19 节第 5 小段那条又出现了。

```
=== 6. 记账装饰器：默认参数那条坑在这里会怎么咬人（第 5 节回马枪）===
  one.log = ['one', 'one', 'two']
  two.log = ['one', 'one', 'two'] ← 两本账合成了一本（两个函数共享同一个 list）
  证据： True
  正解（第 6 节）：把默认值换成 None 哨兵，或者让 log 待在**每次装饰新建的闭包**里：
  three.log = ['three', 'three'] ｜ four.log = ['four'] ｜ 同一本吗： False

=== 7. 本章零件清单（工具里每一行都能回指到某节）===
  函数对象可挂属性                           ← 第 2 节
  inspect.signature / __code__ 当账本   ← 第 3 节
  默认值只在 def 时求值                      ← 第 4 节
  可变默认参数坑 + None 哨兵                  ← 第 5、6 节
  原地改 vs 重绑定（state 字典）               ← 第 7 节
  五种参数筐与 *args/**kwargs 转发           ← 第 9 节
  报错原文认形状                            ← 第 10 节
  闭包盒子做记账本                           ← 第 13 节
  global / nonlocal                  ← 第 15 节
  partial / lru_cache / wraps        ← 第 19 节
  @ 就是赋值                             ← 第 23 节
[退出码 0]
```

第 6 小段是本章最好的一击：**记账装饰器的默认参数坑**。

```python
def naive_log(fn, log=[]):     # 错：log 在 def 那一刻定死，所有被装饰函数共用一本
    ...
```

结果 `one.log` 和 `two.log` 合成了一本（第 6 小段那个 `证据： True`）。
正解是第 6 节那两条：`None` 哨兵，或者**让 `log` 待在每次装饰新建的闭包里** ——
`three.log` 和 `four.log` 就此各自一本（`同一本吗： False`）。

第 7 小段那张 11 行的回指表是这一章的目录：**工具里每一行都能指回某节的编号。**
如果只能记住这张表，本章的目的就达到了。

---

## 第 25 节 · 自证：沙箱里的事只留在沙箱，章目录一个字都没多

demo 的写操作全部发生在 `mktemp -d` 的沙箱里；此刻的当前目录就是沙箱 `work/`
（示例源码是 `cp -R` 进去的）。

```
$ echo "沙箱 work/ 里的项数=$(ls | wc -l)，前 12 个："; ls | head -12
沙箱 work/ 里的项数=22，前 12 个：
a_func_object.py
b_default_once.py
c_mutable_default.py
d_default_fixes.py
demo.sh
e_arg_passing.py
f_param_kinds.py
g_param_errors.py
h_unpacking.py
i_return.py
j_closure.py
k_latebinding.py
[退出码 0]
```

Python 一旦被 `import` 就会造 `__pycache__`，所以这才是本章真正要检查的东西：

```
$ echo "__pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l)"
__pycache__ 个数=0  *.pyc 个数=0
[退出码 0]
```

章目录里应该只有源文件（21 个示例 `.py` + `demo.sh`），没有任何 demo 产物：

```
$ 〔在 05-functions-and-pitfalls 里〕 find . -maxdepth 1 -type f | sort
./a_func_object.py
./b_default_once.py
./c_mutable_default.py
./d_default_fixes.py
./demo.sh
./e_arg_passing.py
./f_param_kinds.py
./g_param_errors.py
./h_unpacking.py
./i_return.py
./j_closure.py
./k_latebinding.py
./l_scope.py
./m_scope_dicts.py
./n_lambda.py
./o_higher.py
./p_functools.py
./q_recursion.py
./r_bytecode.py
./s_call_cost.py
./t_decorator.py
./u_capstone.py
[退出码 0]
```

```
$ 〔在 05-functions-and-pitfalls 里〕 echo "子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  总项数=$(ls | wc -l)  main.c 个数=$(find . -name main.c | wc -l)"
子目录数=0  总项数=22  main.c 个数=0
[退出码 0]
```

这一章一条网络命令都没用、没起过任何监听端口，也没留下后台进程：

```
$ if command -v ss >/dev/null 2>&1; then ss -lnt | head -4; else echo "ss 本机没有"; fi
State  Recv-Q Send-Q Local Address:Port  Peer Address:Port
LISTEN 0      511        127.0.0.1:39721      0.0.0.0:*   
LISTEN 0      4096   127.0.0.53%lo:53         0.0.0.0:*   
LISTEN 0      4096       127.0.0.1:631        0.0.0.0:*   
[退出码 0]
```

```
$ pgrep -a sleep | head -3; echo "（上面这一行为空 = 系统里没有 demo 留下的后台进程）"
（上面这一行为空 = 系统里没有 demo 留下的后台进程）
[退出码 0]
```

关于上面那两个「22」：**它们是捕获那一刻的计数。** 当时章目录里还没有 `README.md`
（正文是捕获之后才写的）。今天你再跑，`ls | wc -l` 和沙箱里的项数都会是 `23`，
多出来的那一项就是 `README.md` —— 它只是被 `cp -R` 复制进沙箱，不会在章目录里产生任何新东西。
**结论不变：产物计数仍是 `0`。**

> 本章的取证纪律（写在脚本头部，不是空话）：临时目录 `mktemp -d "${TMPDIR:-/tmp}/py05-XXXXXX"`，
> `trap cleanup EXIT` 只删自己建的这一个；`--keep` 时不删、改成打一行提示。
> 屏幕上不主动打印绝对路径，只有 `runc_at` 会打目录的 basename。

---

## 对照表

### C 里的概念 vs Python 的函数（全章汇总）

| 你在 C 里的概念 | Python 的真实情况 | 本章证据 |
|---|---|---|
| 函数名 = 代码段地址，编译期定死 | `def` 是**运行期的一次赋值**，跑一次造一个新对象 | 第 2 节 |
| `int (*cmp)(const void*, const void*)` 函数指针 | 函数是普通对象，能存 dict、当参数、当返回值 | 第 2、18 节 |
| 头文件里的默认值 | Python 没有默认值的「声明」；`__defaults__` 是对象身上的元组 | 第 3、4 节 |
| `static` 局部变量 | 闭包的 `cell`，或可变默认参数（**全体共享，这是坑**） | 第 5、13 节 |
| 传 `struct*` 进去能改到外面 | 一样：只有原地改可见；重新赋值改的是自己的局部名字 | 第 7 节 |
| 传 `int` 改不到外面 | 不可变对象改不动 —— 不是魔法，是没得改 | 第 7 节 |
| 栈帧由 OS/栈大小限制 | 帧数由**解释器数**：`sys.getrecursionlimit()`，本机 1000 | 第 20 节 |
| `-foptimize-sibling-calls` 把尾递归变循环 | CPython **不做尾调用优化**，写成尾递归照样炸 | 第 20 节 |
| 手写显式栈做迭代遍历 | 同一套模板在 Python 里就是 `while stack:` + `list.append/pop` | 第 20 节 |
| 局部变量 = `ebp` 偏移，全局 = 查符号 | 局部 = `LOAD_FAST`（数组下标），全局 = `LOAD_GLOBAL`（查字典） | 第 21 节 |
| 宏 / `inline` 省调用开销 | 函数调用约 9 ns，可读性优先；真贵的是循环体里的名字来源 | 第 22 节 |
| 备忘录表（自写 `memo[n]`） | `functools.lru_cache` 一行，门票是**参数可哈希** | 第 19、24 节 |
| 回调注册：函数指针 + `void* ctx` | 闭包（函数 + `cell`）或带 `__call__` 的实例（第 06 章） | 第 13、24 节 |
| `__attribute__((deprecated))` / 宏包装 | 装饰器 `@X ≡ f = X(f)`，在定义那一刻执行一次 | 第 23 节 |

## 一页纸（考前扫这半页）

### 两句话，覆盖本章全部结论

```text
一、def 只跑一次：默认值表达式、装饰器、体内 def 出来的名字 → 定义那一刻定死；
    函数体的每一行、调用处传的参数表达式 → 每次调用现跑。
二、名字是调用时才查的：LEGB（class 那一层不算一层）；传参 = 一次赋值，
    只有原地改（.append / [i]= / +=）外面才看得见。
```

### 五条现抄判据（**这些不是捕获原文**，是本章脚本里反复用过的取证动作）

```python
f.__defaults__; f.__kwdefaults__            # 默认值是哪个对象（第 5 节的坑一眼看穿）
f.__code__.co_varnames                      # 编译器把谁判成了局部（第 15 节）
f.__closure__ and [c.cell_contents for c in f.__closure__]   # 盒子里现在装什么（第 13、14 节）
inspect.signature(f)                        # 五种筐的完整形状（第 24 节）
dis.dis(f)                                  # 名字来源：*_FAST / *_GLOBAL / *_DEREF（第 21 节）
```

### 参数声明顺序（写 `def` 时按这条排）

```text
位置必填 → 位置默认 → / → 剩余位置 → *args → 关键字专用 → **kwargs
                     ↑                 ↑
              左边只能按位置      右边只能按名字
空筐永远是 () 和 {}，不是 None；判「没传」只写 `is None`，别写 `if not x`。
```

### 可变默认值三条正解怎么选

| 场景 | 选它 |
|---|---|
| 「默认每次新表，但允许调用方交表进来」 | `def f(x, box=None)` + `if box is None: box = []` |
| 「绝不替调用方决定容器」 | `def f(x, *, box)` —— 坑变成当场 `TypeError` |
| 「每次现造，而且造什么可配」 | `def f(x, make=list)` + `box = make()`（类里 = `field(default_factory=list)`） |
| 「共享本来就是意图」（注册表、插件表） | 不改，写清注释 |

## 这章的坑（按代价从大到小）

1. **可变默认参数**（第 5 节）：`def f(xs=[])` 那个 `list` **全程序只有一个**，
   三次调用账越攒越多，而且**一个字都不报错**。生产事故形态：缓存函数、累积日志、递归收集器。
   判据修正版：不是「类型不可变就安全」，而是「**没有任何一条路径能原地改它**」才安全
   （`POISON = ([],)` 那种「不可变壳 + 可变芯」照样脏）。
2. **`UnboundLocalError`**（第 15 节）：函数里只要出现过对这个名字赋值（哪怕在 `if False:` 里），
   它整层就是局部。报错原文是 `cannot access local variable 'x' where it is not associated with a value`，
   它是 `NameError` 的子类。修法：`global` / `nonlocal` / 换个名字。
3. **晚绑定**（第 14 节）：`[lambda: i for i in range(3)]` 三个都是 `2`，因为 `i` 是调用时才查的名字，
   三个函数共用**同一个盒子**。三条修法（默认参数快照 / `partial` / 工厂函数）本质相同。
4. **正解写错半步：`if not box`**（第 6 节）：把调用方传来的**空表**当成「没传」扔掉，
   调用方以为自己在收集，实际拿到另一个对象。判「没传」必须写 `is None`。
5. **`list.sort()` 返回 `None`**（第 18 节）：把它当参数传给下一步就是传了个 `None`，
   报的是 `object of type 'NoneType' has no len()` 这种和真实原因无关的错。
6. **装饰器吞掉身份证**（第 23 节）：`__name__`/`__doc__`/签名全变 wrapper 的，
   调试时灾难。写 `@functools.wraps(fn)`，留 `__wrapped__` 这条后门。
7. **带参数装饰器忘括号**（第 23 节）：`@needs_args` 而不是 `@needs_args(2)`，
   报错是 `deco() missing 1 required positional argument: 'fn'` —— 看到这句就去找括号。
8. **`map`/`filter`/`zip` 当成列表用**（第 18 节）：`len(m)` 报 `no len()`，
   而且它们是**一次性**的 —— 第二次 `list(m)` 得到 `[]`。要重复用就 `list(...)` 存下来或用推导式。
9. **`lru_cache` 收到不可哈希参数**（第 19、24 节）：`list` 当场 `unhashable type: 'list'`；
   自定义类**写了 `__eq__` 忘 `__hash__`** 也进不去（第 06 章那条规律在这里咬人）。
10. **以为尾递归会被优化**（第 20 节）：CPython 不做尾调用优化，累加版照样在约 1000 层炸；
    临时调高 `setrecursionlimit` 是借栈空间，用完**必须改回去**。
11. **参数名被局部赋值遮蔽**（第 10 节第 5 小段、第 15 节第 5 小段）：
    `def f(value): value = ...` 不报错也不报 `UnboundLocalError`，
    只会静悄悄拿参数原值 —— 本章最难查的一条，只能靠 `inspect.signature` / `co_varnames`。
12. **把 `locals()` 当可写字典**（第 16 节）：函数里往里写**不创建变量**（3.13 起官方明确不保证生效）；
    能写的是 `globals()`。

## 自检问题

1. `def f(x=time.time()): return x`，连调三次返回什么？账本 `f.__defaults__` 是什么？为什么？（第 4 节）
2. 写出「默认值只在 `def` 时求值」和「函数体里的名字调用时才查」各对应的一条报错原文，并说明报在**哪一行**。（第 4 节）
3. `def bad(item, acc=[])` 连调三次分别是什么？换成 `alias = bad` 再调，账本会不会更脏？说出判据。（第 5 节）
4. `POISON = ([],)` 作默认值安全吗？给出判据的修正版。（第 5 节第 7 小段）
5. 三条正解各自的写法、适用场景和代价是什么？为什么判「没传」必须是 `is None`？（第 6 节）
6. `xs = [...]` / `xs.append(...)` / `xs += [...]` / `xs = xs + [...]` 四种写法，
   哪几种改得动调用方那个表？为什么 `+=` 最常被猜错？（第 7 节）
7. 「`f(1, b=2)` 合法、`f(b=2, 1)` 不合法」——后者报的是 `SyntaxError` 还是 `TypeError`？
   说出一条只报 `TypeError` 的形状错误。（第 8、10 节）
8. 按顺序写出五种参数筐和两个分隔符；`*args` 没收到时是什么值？`**kwargs` 呢？（第 9 节）
9. `zip(*rows)` 为什么能转置？手写等价式是什么？解包会摊几层？（第 11 节）
10. `return a, b` 返回了几个值？`return a,` 返回什么类型？`x = 1,` 是什么？（第 12 节）
11. 说出 `co_cellvars`、`co_freevars`、`__closure__`、`cell_contents` 各自描述什么。
    两个内层函数能共用同一个盒子吗？怎么验证？（第 13 节）
12. `[lambda: i for i in range(3)]` 为什么全是 `2`？给出三条修法，并说出它们共同的原理。（第 14 节）
13. `for` 循环变量、推导式变量、`class` 体里的循环变量，三者的「泄漏」分别是什么结果？（第 14 节第 6 小段）
14. 为什么 `if False: flag = 1` 也能让读 `flag` 变成 `UnboundLocalError`？
    用哪一行代码当场证明编译器怎么判的？（第 15 节）
15. `nonlocal` 能指向模块级吗？`global` 能指向封闭函数吗？各是什么阶段的错？（第 15 节第 4 小段）
16. `locals()` 和 `globals()` 在「能不能写」上的不对称是什么？`eval` 为什么看不见局部名？（第 16 节）
17. `lambda` 的唯一语法限制是什么？写出四条撞墙的原文。为什么不该把 `lambda` 赋值给名字？（第 17 节）
18. `sorted(data, key=len)` 里 `key` 收的是值还是函数？`data.sort()` 的返回值是什么？（第 18 节）
19. `any(probe(n) for n in xs)` 和 `any([probe(n) for n in xs])` 差别在哪？空表时 `any`/`all` 各是什么？（第 18 节第 5 小段）
20. `partial(power, 2)` 固定的是第几个参数？它为什么没有 `__name__`？（第 19 节）
21. `lru_cache` 的门票是什么？`fib(30)` 加了缓存后函数体跑了几次？`cache_info()` 三个数字各是什么含义？（第 19 节）
22. 本机递归限额多少？无限递归实际跑到第几层、为什么不是整限额？尾递归会被优化成循环吗？（第 20 节）
23. 三族指令 `*_FAST` / `*_GLOBAL` / `*_DEREF` 分别对应变量住在哪一层？哪一族最快？（第 21 节）
24. `@repeat(3)` 的等价展开式是什么？多个装饰器叠加时，装饰顺序和调用顺序各是怎样的？（第 23 节）
25. 装饰器在什么时候执行？用哪一节哪一行的实验证明「还没调用就已经注册好了」？（第 23 节第 7 小段）

## 本章没演 / 没验证（绝不编造，都在这）

| 想写但没在本机证明 / 没演的事 | 状态 |
|---|---|
| 把递归限额调到几千后真的**耗尽 C 栈**（segfault）会发生什么 | 没演。本机只演到 `RecursionError`，**不去试崩解释器** |
| `sys.set_int_max_str_digits`、`threading.stack_size` 之类的其它解释器上限 | 没演 |
| 类装饰器（装饰 `class`）、装饰器装饰类里的方法、`@staticmethod`+`@property` 叠加顺序 | 没演，本章只演函数装饰器 |
| 装饰器带自己的状态/参数校验、`contextlib.decorator`、`functools.cached_property`、`singledispatch` | 没演 |
| `async def` / 协程 / `await` 与闭包的关系 | 没演，全书未涉及 |
| 生成器 `yield` 的完整机制与 `send` / `throw` | 第 04 章第 17 节只演了惰性和一次性；本章第 12 节只留了 `StopIteration.value` 一个对照 |
| `nonlocal` 跨**多层**（三层嵌套）绑到哪一层 | 没演 |
| `locals()` 在 3.13 之前是否「改了有时生效」 | 没验证，本机只有 3.14.4；正文那句「3.13 起官方明确不保证」是文档口径，非本机实测 |
| `LOAD_FAST_BORROW`、`LOAD_SMALL_INT` 这些 3.14 指令名的确切语义与借用计数细节 | 没逐条验证，本章只用它们说明「局部 = 数组下标」这一族 |
| 第 22 节那些纳秒数的**绝对值**可复现性 | 未验证也不可复现，只有倍率形状当结论用（本机 4 核） |
| 多线程下共享闭包 / 可变默认值的竞态 | 没演，留给第 11 章（GIL 与并发） |
| `dis` 的完整 opcode 表、`co_stacksize` 的精确算法 | 没演 |
| `inspect.signature` 的绑定与 `Parameter.kind` 全部六种取值 | 第 24 节只用了两种（`POSITIONAL_OR_KEYWORD` / `KEYWORD_ONLY`） |
| 用 AST（`ast` 模块）静态抓「可变默认值」 | 没演，本章只给了 `__defaults__` 和 `grep` 两条自查 |
| `partial` 的 `func.args` 与调用参数的完整合并规则（位置覆盖位置的关键字规则） | 只演了常见三种，边界未穷尽 |

## 怎么跑这一章

```text
cd python-handbook/05-functions-and-pitfalls
bash demo.sh                # 全跑一遍，25 节，退出码 0，约 2 秒，1276 行输出
bash demo.sh --keep         # 跑完保留沙箱并把目录名打给你，进去自己接着敲
bash demo.sh --help         # 只认 --keep / --help，其它参数当场报错退出
```

想只看某一节（bash 手册第 01 章那套 `awk` 区间法在这里同样好用）：

```text
bash demo.sh 2>&1 | awk '/^══════ 第 14 节/,/^══════ 第 15 节/'
```

想单独跑本章任一取证脚本（进沙箱更省事，`--keep` 之后 `work/` 里就是全套源码）：

```text
python3 k_latebinding.py
```

`demo.sh` 的行为约定（红线，代码里都写着）：

- 所有副作用只进 `mktemp -d "${TMPDIR:-/tmp}/py05-XXXXXX"` 出来的那个目录；
  `trap cleanup EXIT` 只删自己建的这一个，`--keep` 时不删、改成打一行提示。
- **示例源码整份复制进沙箱再运行**（`cp -R` 到 `$SB/work`），所以章目录不会留下
  `__pycache__`/`*.pyc` —— 第 25 节用 `find` 现场数过，两个都是 `0`。
- **不写、不改、不删仓库里任何其他文件**；`python-handbook/README.md` 的章节地图由主 agent 维护。
- 只用标准库：`functools`、`inspect`、`dis`、`timeit`、`sys`、`math`、`os`，
  **不 pip、不联网、不改任何配置**（`sys.stdlib_module_names` 那一行就是这句话的证据）。
- 环境变量只在脚本自己和它的子 shell 里生效：`export LC_ALL=C.UTF-8 LANG=C.UTF-8`
  （让 **bash** 那一层的报错语言可预测，Python 自己的输出恒为英文），
  并 `unset PYTHONPATH PYTHONSTARTUP PYTHONOPTIMIZE PYTHONINSPECT PYTHONWARNINGS`，
  避免本机残留影响结论；`PYTHONHASHSEED=0` 只为了让 `set`/`dict` 的打印顺序在两次跑之间一致。
- **故意不加 `set -e`**：本章近一半是「注定失败」的现场（可变默认参数、参数形状报错、
  `UnboundLocalError`、`nonlocal` 指向模块级、`lru_cache` 收到 `list`、递归超限、装饰器少写括号…），
  加了就只能看到第一段。
- **故意不加 `set -o pipefail`**：`cmd | head -3` 这类截取会因为下游提前关管道报 141，
  屏幕上会冒出一堆和教学无关的退出码。
- 每段前打 `$ 命令`、段末打 `[退出码 N]`，非零也继续跑；最后打 `[全部跑完]` 与 `[沙箱已删除]`。
- 只在 `[ -t 1 ]`（真的接到终端）时上色，所以 `bash demo.sh > out.txt` 拿到的是干净纯文本。
- 复核引文：本章 README 的每条输出都由 `@@R 起-止@@` 占位符**从定稿捕获里机械搬出**，
  所以**捕获文件定稿后不能再重跑 `demo.sh`**（重跑行号会漂，占位符指向的内容就错了）。
  要校验：`python3 ~/.local/bin/expand_fences.py README.md ~/.cache/handbook-captures/py05.txt`
  → `python3 ~/.local/bin/verify_quotes.py README.md ~/.cache/handbook-captures/py05.txt '/tmp/py05-[A-Za-z0-9]{6}'`
  → 期望末行 `未匹配行 0`。

## 术语小抄

| 术语 | 一句话解释（本章的用法） |
|---|---|
| `def` 是赋值 | `def f(): ...` 执行的是「造一个函数对象 + 把名字 `f` 贴上去」，跑一次造一个新的 |
| `__defaults__` | 位置筐默认值的**元组**，从右往左对齐参数；可变默认值的坑就写在这里 |
| `__kwdefaults__` | 关键字专用筐默认值的**字典**（`*` 右边那几个） |
| `__code__` / `co_*` | 编译期账本：`co_varnames`（局部名字表）、`co_cellvars`（被盯上的）、`co_freevars`（出门带的）、`co_argcount`、`co_flags` |
| 仅位置参数（`/`） | `/` 左边的参数**只能按位置**传，按名字传报 `got some positional-only arguments passed as keyword arguments` |
| 关键字专用（`*`） | `*` 或 `*args` 右边的参数**只能按名字**传，没给报 `missing 1 required keyword-only argument` |
| `*args` / `**kwargs` | 多出来的位置 → `tuple`；没对上的名字 → `dict`。空时是 `()` / `{}`，**不是 `None`** |
| 解包（`*` / `**`） | 调用时的反方向：把可迭代对象摊成位置参数 / 把 dict 整份灌进关键字筐。只摊**一层** |
| 星号接收 | 赋值左边的 `a, *rest = ...`，`rest` 恒为 `list` |
| 一等公民 | 函数能存、传、返、比、进 `set`（默认按身份哈希） |
| 晚绑定 | 函数体里的名字在**调用那一刻**才查；循环结束后再调用，查到的就是最后一个值 |
| 闭包 / `cell` | 内层函数带着的「盒子」。共享、装当前值；`cell_contents` 能直接看 |
| `nonlocal` / `global` | 声明「这个名字是封闭层 / 模块层的，不是我新造的局部」。两者**不能互换** |
| LEGB | 读名字的顺序：Local → Enclosing → Global → Builtins，**`class` 那一层不算一层** |
| `locals()` / `globals()` | 前者在函数里是**快照**（写了不算），后者是模块那个**真字典**（写了真生效） |
| `lambda` | 表达式版 `def`，里面只能有一个表达式；`__name__` 是 `<lambda>`，所以别赋值给名字 |
| 一等函数参数（`key=`） | 收「元素 → 用来比的值」的函数；收值就报 `'int' object is not callable` |
| 惰性迭代器 | `map` / `filter` / `zip`：创建时不算、`len()` 报错、只能用一遍 |
| `partial` | 固定一部分参数的对象（**不是函数**，没有 `__name__`） |
| `lru_cache` | 参数为键的记忆表。门票＝参数可哈希；`cache_info()` 给 hits/misses/currsize |
| 尾调用优化 | 「尾递归变循环」—— **CPython 不做**，限额是解释器数帧数（本机 1000） |
| `RecursionError` | `RuntimeError` 的子类；抛出一瞬间所有帧已退光，`except` 里数不到深度 |
| `LOAD_FAST` / `_GLOBAL` / `_DEREF` | 三族取名字指令：数组下标 / 查字典（带缓存）/ 开盒子 |
| 装饰器 | `@X ≡ f = X(f)`，在 `def` 那一刻执行一次；带参数的装饰器是三层函数 |
| `functools.wraps` | 把原函数的 `__name__`/`__doc__` 等贴回 wrapper，并留 `__wrapped__` 后门 |

## 本章之后

- **第 06 章**（类与 `dataclass`）：本章把「函数是对象、`def` 只跑一次、原地改才看得见」讲完了，
  第 06 章就是这三条在「类字典 + 实例字典」上的重演：`@property`、`@dataclass`、
  `self.x += 1` 造遮挡物、可变类变量全体脏。那一章第 10 节还会看到
  `dataclass` 把本章第 5 节那条坑变成**定义那一刻的 `ValueError`**。
- **第 07 章**（异常与 `with`）：本章攒了一整章的报错原文 —— `TypeError` 家族
  （`missing 1 required keyword-only argument` / `unexpected keyword argument` / `unhashable type` /
  `object of type 'map' has no len()`）、`SyntaxError` 家族（`/ must be ahead of *`、
  `positional argument follows keyword argument`）、`NameError` 家族（`UnboundLocalError`）、
  以及 `RecursionError`。它们的继承关系、`try/except/else/finally` 的取舍、
  第 24 节那条「把最后一次的异常原样抛出去」背后的异常链，都在那里结清。
- **第 09 章**（模块与 venv）：第 16 节那条「往 `globals()` 里写是真生效」正是 `import` 机制的通道；
  模块级代码「只在导入那一刻跑一次」是本章第 4 节规律的放大版。
- **第 10 章**（类型注解与工具）：第 3 节只证明了「注解在运行时只是一个字典」；
  `typing` 到底能做什么、mypy 本机没装时怎么活，都留给那里。
- **第 11 章**（并发）：第 13 节那些共享的 `cell`、第 5 节那个共享的默认 `list`，
  在多线程下会怎样？那里给答案（本机 GIL 也会实测）。
- **数据结构侧的接口**：你在 C 里写的递归（树、图 DFS）到这里要重新判断一次「能不能递归」——
  第 20 节那张「限额 1000 / 无尾调用优化 / 自己造栈」就是决策依据；
  你写的每个 ADT 的 `cmp` / 遍历回调，到这里就是第 18 节的 `key=` 和第 13 节的闭包。

**本章回指**：第 02 章「赋值是贴名字、不是拷贝」在本章兑现了三次
（第 2 节同名 `def` 覆盖、第 7 节 `rebind` vs `mutate`、第 16 节 `del` 掉名字对象才回收）；
第 04 章「浅拷贝只保护外层槽位」在第 7 节第 6 小段和第 11 节第 6 小段各复现一次；
第 04 章「容器打印用元素 `repr`」和推导式那条规律在第 11、17、18 节反复被用到；
第 03 章的编码/切片在本章没出现（本章只碰函数边界）。
