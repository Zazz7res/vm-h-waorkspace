# 第 06 章 · 类与 dataclass：把「对象」拆成字典、查找链和一份生成的代码

> 本章所有输出都是本机 `Python 3.14.4` 真跑出来的，一条都没编。
> 跑法：`cd python-handbook/06-classes-and-dataclasses && bash demo.sh`（25 节，按小节顺序跑完约 15 秒，跑完自动删沙箱）。
> 想自己拨开关：`bash demo.sh --keep`，然后进打印出来的那个 `/tmp/py06-XXXXXX` 里翻。
>
> 本章的每条命令都长这样：`$ python3 某个文件.py` 或 `$ python3 -c "..."`，
> 全部只读写 `mktemp -d` 出来的沙箱，不碰你的 `~/.config`、不联网、不装包。

---

## 0. 本章在讲什么，以及为什么从「字典」开始

前面五章把变量、类型、字符串、容器、函数都过了一遍。到这一步，很多人学「面向对象」是这样学的：
背一套名词 —— 封装、继承、多态、构造器、装饰器 —— 然后发现**这些名词在 Python 里全都对应不到一条
语言级的新机制**。这一章就是要把它反过来讲：

> **Python 的「对象」没有任何隐藏机关。类是一个字典，实例是一个字典，
> 方法就是放在类字典里的一个普通函数，属性访问就是「先翻实例字典，再翻类字典」这条查找链，
> `dataclass` 则是官方替你往类字典里塞了三个函数。**

这一句话里的每个成分，本章都用可运行的证据钉死：

| 你想搞清楚的东西 | 本章用哪一节回答 | 一句话答案 |
|---|---|---|
| 类到底存在哪 | 第 2 节 | 存在 `A.__dict__` 里，类型是 `mappingproxy`（只读门面） |
| 为什么改 `ann.company` 不影响别的实例 | 第 3 节 | 你写进的是**实例字典**，它只是遮住了类属性 |
| `self` 是什么魔法 | 第 4 节 | 没有魔法：`obj.f(x)` 就是 `A.f(obj, x)`，绑定的只是「函数 + 实例」这一对 |
| `__init__` 能 return 东西吗 | 第 5 节 | 不能，而且报错原文写得很清楚（3.14 的原文本章照录） |
| `print(obj)` 和 `f"{obj}"` 走哪个方法 | 第 6 节 | 三条路各有各的走法，容器里只认 `__repr__` |
| 自定义 `__eq__` 之后为什么进不了 set | 第 7 节 | 定义 `__eq__` 会把 `__hash__` 设成 `None` —— 这是语言规定，不是 bug |
| `@property` 到底生成了什么 | 第 8 节 | 一个 descriptor 住在类字典里；getter/setter/deleter 三件套各有独立入口 |
| `@dataclass` 是不是黑魔法 | 第 9~13 节 | 它只是替你写 `__init__`/`__repr__`/`__eq__`，并且**默认值检查比你自己写还严** |
| `__slots__` 省多少内存 | 第 14 节 | 10 万实例实测：13,600,000 → 4,800,000 字节 |
| 继承时到底按什么顺序找方法 | 第 15~16 节 | 按 `类.__mro__` 这份名单，`super()` 找的是名单里的**下一名**，不是「我爸」 |
| 类变量什么时候变成「全局共享」 | 第 17 节 | 可变类变量全体共享；`+=` 会悄悄造出遮挡物 |
| `isinstance` 和 `type()` 的区别 | 第 18 节 | 一个认户口（含继承），一个认现形 |
| 定义子类那一刻能拦截吗 | 第 19 节 | `__init_subclass__` 能；`abc.ABC` 只是把这个钩子拿来用 |
| `__private` 的真名 | 第 20 节 | 字典里躺的是 `_类名__private`，改写发生在编译期 |
| 实例为什么能带括号调用 | 第 21 节 | `f(x)` 走的是 `type(f).__call__(f, x)` |
| `__getattr__` 和 `__getattribute__` | 第 22 节 | 兜底 vs 全量拦截，后者写错就是死循环（本章真跑给你看） |
| 上下文管理器 | 第 23 节 | `__enter__` / `__exit__` 两个函数，第 07 章正式接手 |
| 怎么自己查这些东西 | 第 24 节 | 本章用到的标准库入口汇总 |

**读法建议**：第 2~4 节是地基，顺序别打乱；第 5~8 节是「常见 dunder」，可以跳着查；
第 9~14 节是 `dataclass` 专区 —— 如果你只想快速会写数据类，看这三节就够；
第 15 节往后是继承与元编程，日常写业务代码用得少，但读第三方库时天天遇到。

---

## 1. 第 1 节 · 环境体检：本章结论只在这么一个解释器上验过

对象模型这种东西，版本差异比字符串那章还凶：`dataclass(slots=True)` 是 3.10 才有的选项，
`__init_subclass__` 是 3.6，报错文案这几年一直在改。所以第一步先把解释器的身份钉死。

本章的取证脚本一共 20 个 `.py` 文件，demo 会把它们整份复制进沙箱再运行 —— 章目录里**不会**留下任何产物：

```
$ ls
a_class_dict.py
b_self_param.py
c_init_return.py
d_repr_str.py
demo.sh
e_eq_hash.py
f_property_trio.py
g_dataclass_intro.py
h_dc_mutable_default.py
i_dc_frozen.py
j_dc_eq_order.py
k_dc_slots.py
l_slots_cost.py
m_mro_super.py
n_shared_classvar.py
o_isinstance_type.py
p_subclass_hooks.py
q_mangling.py
r_callable.py
s_getattr.py
t_context_manager.py
[退出码 0]
```

解释器和它报出的自己：

```
$ python3 --version && python3 -c "import sys; print(sys.version)"
Python 3.14.4
3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]
[退出码 0]
```

```
$ echo "LC_ALL=$LC_ALL  LANG=$LANG  LANGUAGE=$LANGUAGE  PYTHONHASHSEED=$PYTHONHASHSEED"; python3 -c "
import sys, dataclasses, inspect, abc
print(\"标准库三件套齐天：\", {\"dataclasses\", \"inspect\", \"abc\"} <= sys.stdlib_module_names)
print(\"前缀:\", sys.prefix, \"｜ 编码:\", sys.getfilesystemencoding(), sys.stdout.encoding)
"
LC_ALL=C.UTF-8  LANG=C.UTF-8  LANGUAGE=C  PYTHONHASHSEED=0
标准库三件套齐天： True
前缀: /usr ｜ 编码: utf-8 utf-8
[退出码 0]
```

三条要点：

1. **`PYTHONHASHSEED=0`** 是 demo 显式设的，不是环境给的。第 7 节要看「自定义 `__eq__` 之后 set 拒绝收你」，
   那一段的输出里没有哈希值参与，设它只是为了让本章里偶尔出现的 `dict`/`set` 打印顺序在两次跑之间完全一致 ——
   否则同一份代码两次跑，`{'a': 1, 'b': 2}` 这种打印顺序可能翻，本章就没法拿它当证据。
2. **`LC_ALL=C.UTF-8`** 是为了让 **bash** 的报错语言可预测（本章偶尔要看 bash 那一层的原文），
   和 Python 的属性查找、`dataclass` 生成没有半点关系。
3. **`sys.stdlib_module_names`** 这一手值得记住：它能一句话回答「`dataclasses` 是标准库还是要 `pip install`」。
   本章用到的 `dataclasses` / `inspect` / `abc` 三个都答 `True` —— 也就是说**你不需要装任何东西**。

> 本章不碰文件名编码（那是第 03 章的事）。`sys.getfilesystemencoding()` 那行只是顺手留个底牌。

---

## 2. 第 2 节 · 类就是个字典：`A.__dict__` 里到底有什么

先把最反直觉的一条放前面：**在 Python 里，「类」本身就是一个对象，而它的内容存在一个字典里。**
你写下的 `kind = "A 家族"` 和 `def hello(self)`，最后都变成这个字典里的两个键。

```
$ python3 -c "
class A:
    kind = \"A 家族\"
    def hello(self):
        return \"你好\"
print(\"type(A.__dict__).__name__ =\", type(A.__dict__).__name__)
print(\"普通 key（不含双下划线）：\", sorted(k for k in A.__dict__ if not k.startswith(\"__\")))
print(\"vars(A) is A.__dict__ 内容相同：\", dict(vars(A)) == dict(A.__dict__))
"
type(A.__dict__).__name__ = mappingproxy
普通 key（不含双下划线）： ['hello', 'kind']
vars(A) is A.__dict__ 内容相同： True
[退出码 0]
```

第二条：这个字典是**只读门面**。想直接往里塞东西？

```
$ python3 -c "
class A:
    kind = \"A 家族\"
try:
    A.__dict__[\"extra\"] = 1
except TypeError as e:
    print(\"type(e).__name__ =\", type(e).__name__)
    print(\"原文：\", e)
"
type(e).__name__ = TypeError
原文： 'mappingproxy' object does not support item assignment
[退出码 0]
```

注意这条报错的分寸感：`A.__dict__["extra"] = 1` 不行，但 `A.extra = 1` 完全可以。
差别不在「能不能改类」，而在**改的方式必须走属性赋值那条正门**，不能翻窗户进投影字典。
这正是 `mappingproxy` 这个类型的唯一职责：给你看，不让你直接动。

把这套东西放进一个像样的类里，看它的全貌：

```
$ python3 a_class_dict.py
====================================================================
一、Employee 的类字典里到底有什么
====================================================================
vars(Employee) 的键（按写入顺序）：
['__module__', '__firstlineno__', '__doc__', 'company', '__init__', 'greet', '__static_attributes__', '__dict__', '__weakref__']
普通名字（不带双下划线）只有： ['company', 'greet']
company 在类字典里： True
greet 在类字典里： True → 它是个 function

====================================================================
二、实例有自己的字典；类字典只是只读投影
====================================================================
vars(ann) = {'name': '小安'}
ann.__dict__ is vars(ann)： True
Employee.__dict__ 的类型： mappingproxy （mappingproxy = dict 的只读门面）

====================================================================
三、实例存同名属性 → 遮住类属性；del 掉 → 又露出来
====================================================================
遮挡前：ann.company = Acme 制造 ｜ id(ann.company) == id(Employee.company)： True
ann.company = '邪恶集团' 之后：
  ann.company      = 邪恶集团 ← 查的是实例字典，查到就用它
  Employee.company = Acme 制造 ← 类属性一根手指头都没被碰
  vars(ann)        = {'name': '小安', 'company': '邪恶集团'} ← 遮挡物就是实例字典里的这个键
  bob.company      = Acme 制造 ← 别的实例不受影响，照样落到类属性
del ann.company 之后：
  ann.company      = Acme 制造 ← 实例字典里没这个键了，查找继续往下落到类
  vars(ann)        = {'name': '小安'}
greet() 读的也是同一条查找链： 您好，我是 小安，来自 Acme 制造
[退出码 0]
```

逐段读：

- **`vars(Employee)` 的键按写入顺序**排出来是
  `['__module__', '__firstlineno__', '__doc__', 'company', '__init__', 'greet', '__static_attributes__', '__dict__', '__weakref__']`。
  这里有两个你在教程里很少见到的名字：
  - `__firstlineno__`（3.10+）：这个类在源文件里从第几行开始。
  - `__static_attributes__`（3.12+）：编译器扫出来的「这个类的代码里静态引用过的属性名」清单。
    它们的存在说明一件事：**类字典不只装你写的东西，解释器自己也往里记账。**
- **`greet` 在类字典里，而且它是个 `function`** —— 这是全章最关键的一行。
  「方法」不是另一种东西，它就是躺在类字典里的一个普通函数对象。第 4 节把这条用到极致。
- **`company` 也在类字典里**，但 `Employee("小安", ...)` 造出来的 `ann` 的 `vars()` 里只有 `{'name': '小安'}`。
  类属性和实例属性**住在两个不同的字典**，这就是下面第 3 节的全部内容。
- **第三段那个 `del`**：`ann.company = '邪恶集团'` 之后，`ann.company` 读到的是实例字典里那个键，
  `Employee.company` 一根手指头都没被碰，`bob.company` 照样是 `Acme 制造`；
  而 `del ann.company` 之后 `ann.company` 立刻变回 `Acme 制造` —— 因为遮挡物被删掉了，查找继续往下落。

---

## 3. 第 3 节 · 属性查找：先翻实例字典，再翻类字典 —— `del` 掉就露出来

把上面那个故事压缩成三条命令，这是全章最该背下来的一段：

```
$ python3 -c "
class A:
    tag = \"类上的\"
a = A()
print(\"没遮挡时 a.tag：\", a.tag)
a.tag = \"实例上的\"
print(\"遮挡后 a.tag：\", a.tag, \"｜ A.tag：\", A.tag)
print(\"实例字典里现在有：\", sorted(vars(a)))
del a.tag
print(\"del 之后 a.tag：\", a.tag, \"｜ vars(a)：\", sorted(vars(a)))
"
没遮挡时 a.tag： 类上的
遮挡后 a.tag： 实例上的 ｜ A.tag： 类上的
实例字典里现在有： ['tag']
del 之后 a.tag： 类上的 ｜ vars(a)： []
[退出码 0]
```

一段话讲完这条查找链：

> 读 `a.tag` 时，Python 先看 `vars(a)` 里有没有 `tag` 这个键；有就用它，没有再去看 `vars(A)`；
> 再没有才去 `A` 的父类（第 15 节的 MRO），全都没有才抛 `AttributeError`。
> **写 `a.tag = ...` 则永远只写实例字典**，它从不回头看类里有什么。

「写只写实例」这件事有个直接后果，也是新手最常见的困惑来源 —— 改一个实例，别的实例和类本身毫无反应：

```
$ python3 -c "
class A:
    tag = \"类上的\"
one, two = A(), A()
one.tag = \"只有 one 改\"
print(\"two.tag 被带了吗：\", two.tag)
print(\"A.tag 被带了吗：\", A.tag)
print(\"one 的实例字典：\", sorted(vars(one)), \"｜ two 的实例字典：\", sorted(vars(two)))
"
two.tag 被带了吗： 类上的
A.tag 被带了吗： 类上的
one 的实例字典： ['tag'] ｜ two 的实例字典： []
[退出码 0]
```

两条实战推论：

1. **想在类层面改所有实例的默认值**，只能动类属性本身：`A.tag = "新的"`。
   已经写了同名实例属性的那些对象**不会**跟着变，因为它们的实例字典里有遮挡物。
2. **想判断「这个值是这个对象自己的还是类给的」**，只看 `vars(obj)`：
   `sorted(vars(one))` 是 `['tag']`，`sorted(vars(two))` 是 `[]` —— 一眼分明。

> ⚠ 上面这套「先实例后类」的说法对**普通属性**成立。数据描述符（`property`、`__slots__`、类方法）
> 排在实例字典**前面**，第 8 节和第 14 节会各撞一次这个例外 —— 那时不要以为本章骗了你。

---

## 4. 第 4 节 · `self` 就是普通第一个参数：`obj.f()` 与 `A.f(obj)` 等价

Python 没有「this 关键字」。`self` 只是一个**名字**，约定俗成地当作方法的第一个参数。
`c.add(5)` 和 `Calc.add(c, 5)` 是同一次调用的两种写法：

```
$ python3 -c "
class Calc:
    def __init__(self, start=0):
        self.value = start
    def add(self, n):
        self.value += n
        return self.value
c1, c2 = Calc(10), Calc(10)
print(\"Calc.add(c1, 5) 返回：\", Calc.add(c1, 5))
print(\"c2.add(5)    返回：\", c2.add(5), \"← 一模一样的两条路，只是写法不同\")
print(\"c1 和 c2 状态同步吗：\", c1.value == c2.value)
print(\"c1.add.__func__ is Calc.add：\", c1.add.__func__ is Calc.add)
print(\"c1.add.__self__ is c1：\", c1.add.__self__ is c1)
print(\"vars(Calc)[\\\"add\\\"] 的类型：\", type(vars(Calc)[\"add\"]).__name__, \"← 类字典里躺的就是普通函数\")
"
Calc.add(c1, 5) 返回： 15
c2.add(5)    返回： 15 ← 一模一样的两条路，只是写法不同
c1 和 c2 状态同步吗： True
c1.add.__func__ is Calc.add： True
c1.add.__self__ is c1： True
vars(Calc)["add"] 的类型： function ← 类字典里躺的就是普通函数
[退出码 0]
```

看最后三行，这是「绑定方法」的真实构造：

- `c1.add.__func__ is Calc.add` → `True`
- `c1.add.__self__ is c1` → `True`
- `vars(Calc)["add"]` 的类型 → `function`

也就是说：**每次你写 `c1.add`，Python 现场造一个 `(函数, 实例)` 的小对子给你**，
`type(c1.bump).__name__` 叫 `method`。它不是存在类字典里的东西 —— 类字典里那个始终是裸 `function`。

把这条推到极限，会得到本章信息量最大的一份取证：

```
$ python3 b_self_param.py
====================================================================
一、两条路调用同一个函数体
====================================================================
Counter.bump(c1, 5) = 5 （类上取函数，手动把实例当第一个参数传）
c2.bump(5)          = 5 （语法糖，干的同一件事）
两次调用后状态一致： True

====================================================================
二、拆开绑定方法：它就是 (函数, 实例) 一对
====================================================================
type(c1.bump).__name__ = method
b.__func__ is Counter.bump： True
b.__self__ is c1： True
b(3) = 8 ｜ c1.n 从 5 推进到 8 ← 动的正是 __self__ 那个实例

====================================================================
三、把函数当值传出去 —— 它收不收 self 一眼看清
====================================================================
把绑定方法 c3.bump 传进去（每次自动带 c3）： [1, 2, 3]
c3.n 现在是： 3
把类上的裸函数 Counter.bump 传进去：没人塞实例，第一个位置被参数 1 占了，
1.n 当场崩，报错原文：
  原文： 'int' object has no attribute 'n'
正确姿势：裸函数自己补第一参数 → [4, 5] （连推两步，c3.n = 5 ）

====================================================================
四、反证：函数住在实例字典（不是类字典）时，self 不会自动塞
====================================================================
c4.handy() 报错原文： standalone() missing 1 required positional argument: 'counter_obj'
↑ 注意：如果 handy 是「方法」，counter_obj 会被自动塞好；这里没有，
  说明自动塞只发生在「名字从类字典里查出来」那条路上。
手动把实例补上就能跑：c4.handy(c4, 9) = 9 ｜ c4.n = 9
对照：c4.bump(1) 不用补 self，因为 bump 在类字典里 → 10
结论：方法 = 恰好放在类字典里的函数；其余都是普通函数。
[退出码 0]
```

四个层次，逐条说清：

1. **第一段**：`Counter.bump(c1, 5)` 和 `c2.bump(5)` 得到同一个结果，且两次调用后状态一致（`True`）。
   语法糖而已。
2. **第二段**：拆开拓扑 —— `b.__func__` 是函数、`b.__self__` 是实例，`b(3)` 动的是 `__self__` 那个实例的 `n`（5 → 8）。
   **绑定方法的「状态」全在 `__self__` 上，方法对象自己什么数据都不存。**
3. **第三段（这才是坑）**：把「绑定方法」当值传给一个循环，它每次自动带上实例，结果是 `[1, 2, 3]`；
   把「类上的裸函数」传进去，**没人塞第一个参数**，于是第一个位置被真正的业务参数占了 ——
   那条路上传的是 `1`，于是 `1.n` 当场崩，报错原文：

   ```text
   原文： 'int' object has no attribute 'n'
   ```

   注意这条报错有多坑：它不提 `bump`，不提参数个数，只告诉你「整数没有 `.n`」。
   你会盯着自己的业务代码找 `.n`，而真正的问题是你传了个没绑定的函数。
   正确姿势在原文最后：**裸函数自己补第一参数**（`lambda n: Counter.bump(c3, n)` 这类写法），
   于是 `[4, 5]`，`c3.n = 5`。
4. **第四段是反证**，也是「方法到底是什么」的判定实验：把一个普通函数塞进**实例字典**，
   然后 `c4.handy()` 调用它 —— 崩了，原文：

   ```text
   standalone() missing 1 required positional argument: 'counter_obj'
   ```

   `counter_obj` 没被自动塞。手动 `c4.handy(c4, 9)` 就能跑（返回 9）。
   对照：`c4.bump(1)` 不用补，因为 `bump` 在**类字典**里 → 得到 10。

   > 结论一句话：**方法 = 恰好放在类字典里的函数。** 自动塞 `self` 只发生在
   > 「名字从类字典里查出来」这条路上，跟函数长什么样、有没有用装饰器都无关。

---

## 5. 第 5 节 · `__init__` 返回非 `None`：3.14 的报错原文

第 05 章讲过：`return` 是函数自己的事，返回值类型不受限制。那 `__init__` 呢？
它长得像函数，所以真的有人以为可以在里面 `return` 一个对象「交货」。这条实验的价值在于**分两步看**：

```
$ python3 -u c_init_return.py
定义没问题，import 阶段一切正常；崩在调用的那一刻：
Traceback (most recent call last):
  File "/tmp/py06-AF72VU/work/c_init_return.py", line 17, in <module>
    Greeter("小安")
    ~~~~~~~^^^^^^^^
TypeError: __init__() should return None, not 'str'
[退出码 1]
```

- **定义阶段完全正常**：脚本能 import、类体能跑完，Python 一句意见都不提。
- **崩在调用的那一刻**（`Greeter("小安")`），退出码 1。
- 最后一行原文值得逐字读：

  ```text
  TypeError: __init__() should return None, not 'str'
  ```

  `should return None` 而不是 `must return None`，是 CPython 源码里的措辞，意思是
  「协议要求你返回 `None`，你返回了别的」。这条**不是**类型注解层面的检查，
  而是解释器在 `type.__call__` 里写死的一步：造完对象、调完 `__init__`，
  发现返回值不是 `None` 就抛 `TypeError`。
- 也就是说，构造器只负责「填房间」（把 `self` 这个已经造好的对象填上字段），
  不负责「交货」。**真要控制「造出来的是哪个对象」，那是 `__new__` 的活**，本章没演（见末尾清单）。

> 顺带一句和第 05 章的接驳：普通函数漏写 `return` 返回 `None`，`__init__` 恰恰**必须**这样。
> 所以 `__init__` 的签名写 `-> None` 不是风格建议，是协议要求。

---

## 6. 第 6 节 · `__repr__` vs `__str__`：`print`、f-string、容器里各走哪个

两个方法都管「把这个对象说成一句话」，区别是**说给谁听**：
`__str__` 给人看，`__repr__` 给程序员和调试器看，理想情况下 `eval(repr(x))` 能造回一个 `x`。
哪个场合走哪条路，是背不住的 —— 所以本章直接把六条路一次跑齐：

```
$ python3 -c "
class T:
    def __str__(self): return \"str 给的\"
    def __repr__(self): return \"repr 给的\"
t = T()
print(\"print(t) →\", t)
print(\"f\\\"{t}\\\" →\", f\"{t}\")
print(\"f\\\"{t!r}\\\" →\", f\"{t!r}\")
print(\"[t] →\", [t])
print(\"(t,) →\", (t,))
print(\"{1: t} →\", {1: t})
"
print(t) → str 给的
f"{t}" → str 给的
f"{t!r}" → repr 给的
[t] → [repr 给的]
(t,) → (repr 给的,)
{1: t} → {1: repr 给的}
[退出码 0]
```

六行三句话：**`print` / `str()` / 裸 f-string 走 `__str__`；`!r`、`repr()`、容器打印走 `__repr__`。**

其中最容易咬人的是最后三条：`[t]`、`(t,)`、`{1: t}` 里看到的都是 `repr 给的`。
**容器打印元素时永远用 `repr`，一次都不叫 `__str__`。** 这条会解释第 9 节里
`dataclass` 为什么把 `__repr__` 写得那么长 —— 因为你多半是要把对象塞进列表里看的。

把这条规则用在三个「只定义其中一个」的类上，能问出另外两个方向的问题：

```
$ python3 d_repr_str.py
====================================================================
一、Book 同时有 __str__ 和 __repr__：四种场合
====================================================================
1. print(b) →
   实际打出：《Python 手册》共 420 页
2. str(b) / f"{b}" → 《Python 手册》共 420 页 | 《Python 手册》共 420 页
3. repr(b) / f"{b!r}" → Book(title='Python 手册', pages=420) | Book(title='Python 手册', pages=420)
4. 塞进容器再 print： [Book(title='Python 手册', pages=420)] | (Book(title='Python 手册', pages=420),)
   ↑ list/tuple/dict 打印元素时用的全是元素的 repr，所以看到的是 __repr__

====================================================================
二、只有 __repr__（Stamp）：str() 退回来
====================================================================
print(s) → Stamp('加急') 　← __str__ 没定义，str() 退到 __repr__
f"{s!r}" → Stamp('加急')

====================================================================
三、只有 __str__（Memo）：repr() 不退回来
====================================================================
print(m) → 便签内容 　← 走 __str__
repr(m) 的开头： <__main__. …结尾： > ← 默认那串带地址的 <...object at 0x...>，地址每次跑都不同所以只截形状
repr(m) 等于 __str__ 吗： False ← False：repr 永远不叫 __str__
把 Memo 塞进 list 打印： [ <__main__. … ] ← 容器用的是 repr，给人看的 __str__ 根本没上场

====================================================================
四、f-string 的转换码就这两个：!s 走 str，!r 走 repr
====================================================================
f"{b!s}" → 《Python 手册》共 420 页
f"{b!r}" → Book(title='Python 手册', pages=420)
小结：写日志给人看用 str/print，写调试输出和「能复制回去重跑」的东西用 repr。
[退出码 0]
```

四段各自的要点：

1. **`Book`（两个都有）**：`print` 给「《Python 手册》共 420 页」，
   `repr` 给「`Book(title='Python 手册', pages=420)`」。第四行验证了容器那条：
   `print([b])` 出来的是 `[Book(title='Python 手册', pages=420)]`，**不是**给人看的那句。
2. **`Stamp`（只有 `__repr__`）**：`print(s)` 也能打出 `Stamp('加急')` —— 因为
   **`object.__str__` 的默认实现就是回头调 `repr(self)`**。所以「只写 `__repr__`」是安全的，
   很多库就是这么干的。
3. **`Memo`（只有 `__str__`）**：反过来**不成立**，这是本节唯一的方向性 asymmetry：
   `repr(m)` 拿到的是默认那串 `<__main__. … object at 0x…>`，
   `repr(m) == str(m)` 判出 `False`，塞进列表打印时你看到的是那串带地址的默认货色。
   （那串里的地址每次跑都不同，所以 demo 自己就把形状截出来打了，只留 `<__main__. …` 这个轮廓 ——
   这一行不是本章的证据行，是 demo 的现场处理。）
   > 结论值得背：**只写 `__repr__` 两个场合都能用；只写 `__str__` 只有两个场合之一。**
   > 拿不定主意时，写 `__repr__`。
4. **f-string 的转换码只有两个**：`!s` 走 `str`、`!r` 走 `repr`（`!a` 是 ASCII 版，本章没演）。

---

## 7. 第 7 节 · `__eq__` 的代价：`__hash__` 被没收，进不了 set / 当不了 dict 键

这条是 Python 里最「看不见」的一条语言规定，也是很多人第一次给类写 `__eq__` 时撞的墙。
裸现场（自定义判等，然后把实例放进 set）：

```
$ python3 -c "
class P:
    def __eq__(self, other): return True
p = P()
{p}
"
Traceback (most recent call last):
  File "<string>", line 5, in <module>
    {p}
TypeError: cannot use 'P' as a set element (unhashable type: 'P')
[退出码 1]
```

原文：`TypeError: cannot use 'P' as a set element (unhashable type: 'P')`。
**没有一句提到「你定义了 `__eq__`」** —— 这就是它难查的原因。完整故事在这里：

```
$ python3 e_eq_hash.py
====================================================================
一、__eq__ 本身是好的：字段相等就相等
====================================================================
p1 == p2 → True ｜ p1 == p3 → False

====================================================================
二、代价：__hash__ 被自动设成 None
====================================================================
Point.__hash__ is None → True
（注意不是「没继承到」，是解释器在类创建时主动塞的 None）

====================================================================
三、三种踢法，三种报错原文
====================================================================
放进 set： TypeError: cannot use 'Point' as a set element (unhashable type: 'Point')
当 dict 键： TypeError: cannot use 'Point' as a dict key (unhashable type: 'Point')
直接 hash()： TypeError: unhashable type: 'Point'

====================================================================
四、补 __hash__ 之后：set / dict 都活了
====================================================================
set 里有几个： 2 ← h1、h2 判等又同哈希，被当成同一个，去重成功
用另一个等值新实例去查 dict： 坐标 1,2 的备注 ← 靠 __eq__ + __hash__ 配合命中
哈希值相等吗： True ← 这就是「相等必须同哈希」约定的兑现

====================================================================
五、对照：完全不写 __eq__ 的类，默认可哈希（按对象身份）
====================================================================
q1 == q2 → False ｜ hash(q1) == hash(q2) → False
放进 set： 2 个 ← 两个不同对象，谁也去重不了谁
结论：默认的「相等」是「是不是同一个对象」；你一改判等规则，就必须同批改哈希规则。
[退出码 0]
```

拆开讲：

- **第一段**证明 `__eq__` 本身工作正常：`p1 == p2` 是 `True`（字段相同），`p1 == p3` 是 `False`。
- **第二段**是病根，也是本章反复强调的那类「解释器主动做的手术」：
  `Point.__hash__ is None` → `True`，而且原文括号里那句很重要 ——
  **不是「没继承到」，是解释器在类创建那一刻主动塞进去的 `None`。**
  规则原文：一个类如果定义了 `__eq__` 而没有定义 `__hash__`，`__hash__` 被设为 `None`。
- **第三段**给出三种踢法的三种原文（这三句长得不一样，背下来能省很多搜索）：

  ```text
  放进 set： TypeError: cannot use 'Point' as a set element (unhashable type: 'Point')
  当 dict 键： TypeError: cannot use 'Point' as a dict key (unhashable type: 'Point')
  直接 hash()： TypeError: unhashable type: 'Point'
  ```

- **第四段**是正解：补一个和 `__eq__` 口径一致的 `__hash__`，set 和 dict 立刻都活了
  （`set 里有几个： 2` 是因为 `h1`、`h2` 既判等又同哈希，被合成一个），
  并且 `hash(p1) == hash(p2)` → `True` 兑现了那条约定：
  **相等的对象必须哈希相等。** 反过来不要求（哈希相等但判等不等是允许的，那叫冲突）。
- **第五段**是对照组：完全不写 `__eq__` 的类，`q1 == q2` 是 `False`、哈希也不等，
  放进 set 剩 2 个 —— 因为**默认的判等是「是不是同一个对象」（身份），默认哈希也是按身份算的**。

> 一句话记忆：**你一改「怎么算相等」，就必须同批改「怎么算哈希」。**
> 第 11 节会让你看到 `dataclass` 对这条规则守得比你自己写还严。

---

## 8. 第 8 节 · `@property`：getter/setter/deleter 三件套与两种报错原文

`@property` 的作用：让「调用一个计算」长得像「读一个字段」。它的代价是两类新报错。先撞最常见的一个：

```
$ python3 -c "
class R:
    @property
    def v(self): return 1
r = R()
r.v = 2
"
Traceback (most recent call last):
  File "<string>", line 6, in <module>
    r.v = 2
    ^^^
AttributeError: property 'v' of 'R' object has no setter
[退出码 1]
```

原文 `AttributeError: property 'v' of 'R' object has no setter`，
而且注意 traceback 里那行 `r.v = 2` 底下有个 `^^^` 指着 `r.v` —— 3.14 的报错会把出事的**目标**标出来。

三件套的完整面貌、property 在类字典里的真身，以及两个新手事故现场：

```
$ python3 f_property_trio.py
====================================================================
一、只有 getter：读 OK，写/删各自报什么原文
====================================================================
r.serial = 2026 ← 读没问题
赋值 r.serial = 999 → AttributeError: property 'serial' of 'ReadOnly' object has no setter
删除 del r.serial → AttributeError: property 'serial' of 'ReadOnly' object has no deleter

====================================================================
二、property 在类字典里是个什么物件
====================================================================
type(vars(ReadOnly)['serial']).__name__ = property
vars(ReadOnly)['serial'].getter 是函数吗： getter

====================================================================
三、Product 三件套：setter 挡脏数据，deleter 清账
====================================================================
读 mp.price = 10
mp.price = 25 之后读： 25 （setter 里我们改的是 _price）
实例字典： {'_price': 25} ← 对外名字是 price，内部存的是 _price
mp.price = -1 → ValueError: 价格不能为负，收到 -1
（deleter 被调用：把价格清账，回到默认 0）
del mp.price 之后： 0

====================================================================
四、调用方写法差别：obj.area vs obj.size
====================================================================
rc.area 是什么类型： method ← 不带括号只是拿到绑定方法，没算
rc.area() = 12 ← 方法要括号才执行
rc.size = 12 ← property 不带括号，直接就是算好的值
误写成 rc.size() 会怎样：
  原文： 'int' object is not callable
误写成 print(rc.area)（忘加括号）：打印出来的是 method 对象而不是数，
这是新手最常见的「property 与方法混用」事故现场。
[退出码 0]
```

四段各自要点：

1. **第一段：写和删报的是两句不同的原文** —— `has no setter` / `has no deleter`。
   也就是说 `@property` 只给了 getter 时，这个属性**读得到、写不进、删不掉**，三条路各自有独立开关。
2. **第二段回答「property 到底住在哪」**：`vars(ReadOnly)['serial']` 的类型是 `property`，
   它自己带着 `.getter`（一个函数）。结合第 2 节：**它就是类字典里的一个物件**，不是实例上的东西。
   再结合第 3 节那条警告：`property` 是**数据描述符**，读它的优先级**排在实例字典前面** ——
   这就是为什么你不能靠 `obj.serial = ...` 绕过 setter。
3. **第三段**是正经用法：setter 挡脏数据（`mp.price = -1` → `ValueError: 价格不能为负，收到 -1`），
   deleter 清账（`del mp.price` 之后回到默认 0）。
   这里还有一个值得看的细节：`实例字典： {'_price': 25}` ——
   **对外的名字是 `price`（类字典里的 property），实际数据存在 `_price`（实例字典里的键）**。
   这两个名字必须是两个，否则 setter 里写 `self.price = value` 会无限递归（第 22 节有一个同构的死循环现场）。
4. **第四段是「property vs 方法」的混淆事故**，本章最实用的一段对照：
   - `rc.area` 的类型是 `method` —— **不带括号只是拿到绑定方法，根本没算**；
   - `rc.area()` = 12 —— 方法要括号才执行；
   - `rc.size` = 12 —— property 不带括号，直接就是算好的值；
   - 反过来误写 `rc.size()` 会崩，原文 `'int' object is not callable`（又是第 4 节那种「主语失踪」的报错）。

   > 所以：把 `print(rc.area)` 打成不带括号的形式，输出里会看见 `<bound method ...>` 而不是数。
   > 这是代码评审里一眼能抓的事故：**property 和方法混用时，忘了括号的那一方永远不报错，只是给你一个对象。**

---

## 9. 第 9 节 · `@dataclass` 现形（一）：它只是替你写进类字典的三个函数

`dataclass` 的名声很差，因为教程都在背「加个 `@dataclass` 就自动生成 `__init__`」，
没人告诉你生成的东西**长在哪、是什么类型**。这一节和第 2 节严丝合缝：

```
$ python3 -c "
from dataclasses import dataclass
@dataclass
class D:
    x: int
    y: str = \"hi\"
print(\"类字典里的普通名字（y 是字段声明留下的默认值）：\", sorted(k for k in vars(D) if not k.startswith(\"__\")))
print(\"生成三件套都在：\", all(n in vars(D) for n in (\"__init__\", \"__repr__\", \"__eq__\")))
import inspect
print(\"__init__ 的签名：\", inspect.signature(D.__init__))
print(\"D(1) 的 repr →\", repr(D(1)))
"
类字典里的普通名字（y 是字段声明留下的默认值）： ['y']
生成三件套都在： True
__init__ 的签名： (self, x: int, y: str = 'hi') -> None
D(1) 的 repr → D(x=1, y='hi')
[退出码 0]
```

四行输出对应四条事实：类字典里的普通名字只有 `['y']`（`x` 没默认值，所以类字典里根本没有 `x` 这个键）；
`__init__` / `__repr__` / `__eq__` 三件套都在；`inspect.signature` 看得到生成的参数表
`(self, x: int, y: str = 'hi') -> None`；`repr(D(1))` → `D(x=1, y='hi')`。

完整的现形过程，包括「它替你写的东西和你手写的东西并排跑是不是一样」：

```
$ python3 g_dataclass_intro.py
====================================================================
一、装饰完，类字典里多了什么
====================================================================
三件套都在类字典里： ['__init__', '__repr__', '__eq__']
它们都只是普通函数： ['function', 'function', 'function']
inspect 看得到签名（这就是它替你写的 __init__ 参数表）：
   (self, x: int, y: int, label: str = 'v') -> None

====================================================================
二、fields()：字段清单，ClassVar 不在其中
====================================================================
字段名： ['x', 'y', 'label']
  x: type=int 默认值='MISSING' default_factory=无
  y: type=int 默认值='MISSING' default_factory=无
  label: type=str 默认值='v' default_factory=无
count（ClassVar）在字段清单里吗： False

====================================================================
三、实例上存的东西：字段老老实实进 vars(实例)
====================================================================
vars(Vec(1, 2)) = {'x': 1, 'y': 2, 'label': 'v'}
count 在实例字典里吗： False ← ClassVar 只住类字典： True

====================================================================
四、和手写版并排跑：行为一致
====================================================================
repr 对比： Vec(x=1, y=2, label='v') ｜ Vec(x=1, y=2, label='v')
判等对比： (True, True) ← 都是「字段全相等才相等」
字段访问对比： True
缺参数报错对比 —— dataclass 版： Vec.__init__() missing 1 required positional argument: 'y'
                          手写版： HandVec.__init__() missing 1 required positional argument: 'y'
结论：@dataclass = 读注解，把上面这些函数生成好塞进类字典，仅此而已。
[退出码 0]
```

一段一段读：

- **第一段**：三件套 `['__init__', '__repr__', '__eq__']` 都在类字典里，而且
  它们的类型 `['function', 'function', 'function']` —— **全是普通函数**，不是某种生成器对象。
  这就是「`@dataclass` = 往类字典里塞函数」的全部含义。
  签名那行 `(self, x: int, y: int, label: str = 'v') -> None` 也是真函数的签名。
- **第二段**：`dataclasses.fields()` 是字段清单的**权威来源**（比翻 `__annotations__` 准）。
  三个字段各自的 `type` / 默认值 / `default_factory` 都列了出来，
  其中没有默认值的字段打的是 `'MISSING'` —— 那是 `dataclasses.MISSING` 这个哨兵对象，
  **不是字符串 `"MISSING"`，也不是 `None`**。用 `None` 当「没有默认值」的标记是不行的，
  因为 `None` 本身可能就是一个合法默认值。
  同一段还有个关键点：`count（ClassVar）在字段清单里吗： False` ——
  **`ClassVar` 注解的东西不算字段**，dataclass 直接跳过它。
- **第三段**：`vars(Vec(1, 2))` = `{'x': 1, 'y': 2, 'label': 'v'}` —— 字段老老实实进实例字典（第 3 节兑现）；
  `count` 不在实例字典里，只在类字典里。
- **第四段是和手写版的并排实验**，这一段是「dataclass 不是黑魔法」的正面证据：
  `repr` 两边逐字相同、判等两边 `(True, True)`、字段访问 `True`，
  连**缺参数的报错原文都只是类名不同**：

  ```text
  dataclass 版： Vec.__init__() missing 1 required positional argument: 'y'
  手写版：       HandVec.__init__() missing 1 required positional argument: 'y'
  ```

  这句原文和 `__init__` 真的是个普通函数完全一致 —— 参数表不匹配就是标准的函数调用错误。

> 第 05 章讲函数时说过：默认值在 `def` 那一刻求值一次。`@dataclass` 也守这条，于是有了下一节。

---

## 10. 第 10 节 · `@dataclass` 现形（二）：可变默认值它当场拒绝你

`items: list = []` 这一行在 `dataclass` 里**根本通不过定义**：

```
$ python3 -c "
from dataclasses import dataclass
@dataclass
class Draft:
    items: list = []
"
Traceback (most recent call last):
  File "<string>", line 3, in <module>
    @dataclass
     ^^^^^^^^^
  File "/usr/lib/python3.14/dataclasses.py", line 1442, in dataclass
    return wrap(cls)
  File "/usr/lib/python3.14/dataclasses.py", line 1432, in wrap
    return _process_class(cls, init, repr, eq, order, unsafe_hash,
                          frozen, match_args, kw_only, slots,
                          weakref_slot)
  File "/usr/lib/python3.14/dataclasses.py", line 1066, in _process_class
    cls_fields.append(_get_field(cls, name, type, kw_only))
                      ~~~~~~~~~~^^^^^^^^^^^^^^^^^^^^^^^^^^
  File "/usr/lib/python3.14/dataclasses.py", line 917, in _get_field
    raise ValueError(f'mutable default {type(f.default)} for field '
                     f'{f.name} is not allowed: use default_factory')
ValueError: mutable default <class 'list'> for field items is not allowed: use default_factory
[退出码 1]
```

这份 traceback 很长，但**只有最后一行是判决**：

```text
ValueError: mutable default <class 'list'> for field items is not allowed: use default_factory
```

中间那几帧（`dataclasses.py` 里的 `_process_class` → `_get_field`）值得看一眼就跳过，
它们告诉你两件实用的事：**报错发生在 `@dataclass` 装饰的那一瞬间（`line 3, in <module>` + `@dataclass ^^^^^`），
而不是任何一次实例化**。所以这类错误在导入模块时就会炸，不会潜伏到运行时 —— 这是它友好的地方。

为什么要有这条禁令？因为同样的坑在手写版里安静得要命。手写的病根（第 05 章那个坑搬进类）：

```
$ python3 h_dc_mutable_default.py
====================================================================
一、手写版：两个实例共用同一个默认列表（第 05 章的病根搬进类）
====================================================================
n1.add 之后 n1.items = ['苹果']
没动过的 n2.items = ['苹果'] ← 见鬼了，它怎么也有苹果
两个实例的 items 是同一个对象吗： True
真凶在函数对象上：NaiveCart.__init__.__defaults__ = (['苹果'],)

====================================================================
二、dataclass 的拒绝原文（捕获版；裸 traceback 在 demo 上一条命令里）
====================================================================
ValueError: mutable default <class 'list'> for field items is not allowed: use default_factory

====================================================================
三、field(default_factory=list)：每建一个实例现造一个列表
====================================================================
s1.add 之后：s1.items = ['苹果'] ｜ s2.items = []
items 同源吗： False ← False，各用各的
fields(SafeCart) 里 items 的默认值：MISSING（对，字段本身没默认值）
default_factory 上存的是： list ← 构造时调用它拿新列表
显式传参照常： SafeCart(items=['现成的'], owner='张三')
[退出码 0]
```

三段对照：

1. **第一段（手写版的事故）**：`n1.add` 之后，**没动过的 `n2.items` 里也有 `['苹果']`**，
   而且「两个实例的 `items` 是同一个对象吗」答 `True`。
   最狠的是最后那行**真凶定位法**：`NaiveCart.__init__.__defaults__ = (['苹果'],)` ——
   默认值挂在**函数对象**上，两个实例共用它，所以改一个等于改全部。
   这一手（看 `函数.__defaults__`）是第 05 章教的，本章再兑现一次。
2. **第二段**：`dataclass` 的拒绝原文（就是上面那条 `ValueError`，这里是被脚本捕获后的一行版）。
3. **第三段（正解）**：`field(default_factory=list)` —— 每建一个实例**现造**一个列表，
   `s1.items = ['苹果']` 而 `s2.items = []`，`items 同源吗： False`。
   还有个容易看漏的细节：`fields(SafeCart)` 里 `items` 的默认值是 `MISSING`，
   **字段本身没有默认值**，默认值是构造时调 `default_factory`（这里就是 `list` 这个类型本身）现取的。
   显式传参照常：`SafeCart(items=['现成的'], owner='张三')`。

> 顺带一条实用结论：`dict`、`set`、`list` 这三种可变默认值都会被这条禁令拦住；
> 但 `field(default_factory=...)` 也能接任何**无参数可调用**的东西，
> 比如 `default_factory=lambda: MyType(...)` 这种带参数的写法是合法的（本章没演）。

---

## 11. 第 11 节 · `@dataclass(frozen=True)`：赋值现场与 `FrozenInstanceError` 原文

`frozen=True` 把实例变成「写不进去的字典」，报错也自成一派：

```
$ python3 -c "
from dataclasses import dataclass
@dataclass(frozen=True)
class Cfg:
    host: str
c = Cfg(\"localhost\")
c.host = \"evil\"
"
Traceback (most recent call last):
  File "<string>", line 7, in <module>
    c.host = "evil"
    ^^^^^^
  File "<string>", line 15, in __setattr__
dataclasses.FrozenInstanceError: cannot assign to field 'host'
[退出码 1]
↑ 退出码 1。注意异常类是 dataclasses.FrozenInstanceError，它是 AttributeError 的儿子（脚本里验证）。
```

两个细节：

- 异常类是 **`dataclasses.FrozenInstanceError`**，不是 `AttributeError` 的字面名字，
  但 traceback 里有一帧 `File "<string>", line 15, in __setattr__` —— 说明它是**生成的 `__setattr__` 主动抛的**。
- 它是 `AttributeError` 的儿子（脚本里验证，见下面的 `True`），
  所以 `except AttributeError` 能接住它 —— 这条在写异常处理时很有用。

它和白送哈希之间的关系，正好把第 7 节的规则照了一遍：

```
$ python3 i_dc_frozen.py
====================================================================
一、赋值报什么原文（捕获版，和裸 traceback 同一行字）
====================================================================
FrozenInstanceError: cannot assign to field 'host'
删除也一样： cannot delete field 'port'
FrozenInstanceError 是 AttributeError 的儿子吗： True

====================================================================
二、frozen 反而白送 __hash__：能进 set；非 frozen 的 eq 类不能
====================================================================
Config(1,1) == Config(1,1)： True
放进 set 去重后剩： 2 个
对照组 Mutable.__hash__ is None： True ← 第 7 节那条规则，dataclass 也守
把 Mutable 放进 set → TypeError: cannot use 'Mutable' as a set element (unhashable type: 'Mutable')

====================================================================
三、frozen 的正确「改法」：replace 造新物件
====================================================================
原 c = Config(host='localhost', port=8000) 　新 c2 = Config(host='localhost', port=9000)
c2 是新对象吗： True ｜ 旧对象字段没被动： 8000

====================================================================
四、真要写内部缓存怎么办：正路是不进字段（或 ClassVar），歪路给你看一眼
====================================================================
歪路：object.__setattr__ 能硬写（c.port 现在是 1234 ）——这是逃生舱不是日常 API
[退出码 0]
```

四段要点：

1. **第一段**：两条原文，一赋值一删除 —— `cannot assign to field 'host'` / `cannot delete field 'port'`。
2. **第二段（这一节最值钱）**：`frozen=True` **反而白送你 `__hash__`**，
   因为字段不可变 ⇒ 按字段算哈希是安全的，于是 `Config` 的实例能进 set（去重后剩 2 个，
   说明 `(1,1)` 与 `(1,1)` 被认成同一个）。
   对照组：非 frozen 但带 `__eq__` 的 `Mutable.__hash__ is None` → `True`，
   放进 set 直接 `TypeError: cannot use 'Mutable' as a set element (unhashable type: 'Mutable')`。
   **第 7 节那条语言规定，`dataclass` 生成器自己完全守着** ——
   这就是「用官方生成器比自己写安全」的实证例子。
3. **第三段（frozen 的正确改法）**：不要改，要**换**。`dataclasses.replace(c, port=9000)` 造一个新对象，
   旧对象字段纹丝不动（`8000`），`c2 是新对象吗： True`。
4. **第四段**：真要往 frozen 实例里塞内部缓存，正路是**别把它做成字段**（或者用 `ClassVar`）；
   歪路 `object.__setattr__(c, 'port', 1234)` 确实能硬写（写进去了，`c.port` 现在是 1234），
   但那是逃生舱、不是日常 API —— 它绕过的正是你声明「不可变」的那个承诺。

---

## 12. 第 12 节 · `@dataclass` 现形（三）：`eq` 怎么比、`order` 才有的 `<`、`repr` 长什么样

第 9 节已经证明 `__init__` / `__repr__` / `__eq__` 就是被塞进类字典的三个普通函数。
这一节把它们**里面写了什么**逐条演出来，先演最容易被当成 bug 的一条：dataclass 给了你 `==`，
却不给你 `<`。

```
$ python3 -c "
from dataclasses import dataclass
@dataclass
class V:
    a: int
V(1) < V(2)
"
Traceback (most recent call last):
  File "<string>", line 6, in <module>
    V(1) < V(2)
TypeError: '<' not supported between instances of 'V' and 'V'
[退出码 1]
```

demo 在那条命令下面打了一句「不写 `order=True` 就没有 `__lt__`」，报错原文就在上面：
`TypeError: '<' not supported between instances of 'V' and 'V'`。
不是「比较结果为假」，是**这个运算符根本没被实现**，所以 Python 只能报 `TypeError`。

```
$ python3 j_dc_eq_order.py
====================================================================
一、生成的 __eq__：逐字段 + 必须同类
====================================================================
Version(1,2) == Version(1,2)： True
Version(1,2,0) == Version(1,2)： True ← patch 也是字段，默认 0 也参与比
Version(1,2) == SubVersion(1,2)： False ← 字段全同也不给过：不是同一个类
对照组手写 isinstance 版： True （它管你是不是子类）
比不过就返回 NotImplemented 让位： False ← 对面类接不住才 False

====================================================================
二、order=True 才有 < ：字段元组字典序
====================================================================
Version(1,2) < Version(1,10)： True ← (1,2,0)<(1,10,0) 逐位比，不是字符串比
Version(1,9,1) < Version(1,10,0)： True
sorted 直接可用： [Version(major=1, minor=9, patch=0), Version(major=1, minor=10, patch=0), Version(major=2, minor=0, patch=0)]
对照组 Plain（没开 order）的类字典里有 __lt__ 吗： False → Plain(1) < Plain(2) 就是 demo 上一条命令里那句 TypeError

====================================================================
三、repr：按字段声明顺序，name=value，str 用带引号的 repr
====================================================================
repr(Version(1,2)) → Version(major=1, minor=2, patch=0)
repr(Version(1,2,'rc1')) 这种字符串字段 → Version(major=1, minor=2, patch='rc1')
塞进 list： [Version(major=1, minor=0, patch=0), Version(major=0, minor=1, patch=0)] ← 容器打印走 repr，和第 6 节呼应
[退出码 0]
```

四段要点：

1. **生成的 `__eq__`：逐字段比 + 必须同类**。
   `Version(1,2) == Version(1,2)` → `True`；`Version(1,2,0) == Version(1,2)` 也是 `True`
   —— `patch` 本身也是字段，默认值 `0` 一样参与比，别以为「省着写的那个字段不算」。
   而 `Version(1,2) == SubVersion(1,2)` → `False`：字段一个不差也不给过，因为**不是同一个类**。
   对照手写 `isinstance` 版是 `True`（它管你是父是子）。
   顺带一条实现细节：判定不通过时生成的是 `NotImplemented` 而不是 `False`，
   所以对面类有机会接这一比（`比不过就返回 NotImplemented 让位： False` 说的就是「对面也接不住，才落到 False」）。
2. **`order=True` 才有 `<`，比的是「字段元组」的字典序**。
   `Version(1,2) < Version(1,10)` → `True`，因为比的是 `(1,2,0) < (1,10,0)`，逐位按 `int` 比。
   这一点对你现在写版本号特别重要：如果 `major/minor` 是 `str`，那就是字符串比，
   `"9" < "10"` 这种结果会反过来坑你 —— **字段类型决定排序对不对**。
   开了 `order` 之后 `sorted(...)` 直接可用，不用传 `key=`。
   没开的对照组 `Plain` 的类字典里连 `__lt__` 都没有，所以它一比较就是 demo 上一条命令里那句 `TypeError`。
3. **`repr` 的形状**：按**字段声明顺序**输出 `name=value`，字符串字段用带引号的 `repr`
   （`patch='rc1'`），不是裸文本。
4. **和第 6 节呼应**：把 dataclass 实例塞进 list 再打印，看到的还是这个 `repr`
   （`[Version(major=1, minor=0, patch=0), ...]`）—— 容器不跟你讲情面，它只调 `__repr__`。

## 13. 第 13 节 · `@dataclass(slots=True)`：3.10+ 的选项，真把 `__dict__` 去掉了吗

`slots=True` 是 3.10 才加的参数（本机 3.14 可用，第 24 节现场验过）。
值得单独演一次的原因是：它**不是「建议解释器省点内存」的提示**，而是真的把类重建了一遍。

```
$ python3 -u -c "
from dataclasses import dataclass, fields
@dataclass(slots=True)
class P:
    x: int
p = P(1)
print(\"实例有 __dict__ 吗：\", hasattr(p, \"__dict__\"))
print(\"类字典里的 __slots__：\", P.__slots__)
print(\"fields 照常：\", [f.name for f in fields(P)])
p.z = 3
"
实例有 __dict__ 吗： False
类字典里的 __slots__： ('x',)
fields 照常： ['x']
Traceback (most recent call last):
  File "<string>", line 10, in <module>
    p.z = 3
    ^^^
AttributeError: 'P' object has no attribute 'z' and no __dict__ for setting new attributes
[退出码 1]
```

四行三句话：`hasattr(p, "__dict__")` → `False`（实例上那个字典真的没了）、
`P.__slots__` → `('x',)`（选项被翻译成了标准的类级 `__slots__`）、
`fields(P)` 照常给出 `['x']`（字段清单不受影响，工具链照用），
最后 `p.z = 3` 当场崩 —— 报错原文比「不能加属性」说得细：
`AttributeError: 'P' object has no attribute 'z' and no __dict__ for setting new attributes`。

```
$ python3 k_dc_slots.py
====================================================================
一、__dict__ 真的没了；字段变成了类上的槽描述符
====================================================================
hasattr(p, '__dict__') → False
Point.__slots__ → ('x', 'y', 'tag')
类字典里 x 这个位置是什么： member_descriptor
但字段清单照常： ['x', 'y', 'tag']
实例数据照样读写：p.x = 1 ；p.tag = pt

====================================================================
二、加新属性：当场拒绝，原文
====================================================================
AttributeError: 'Point' object has no attribute 'z' and no __dict__ for setting new attributes

====================================================================
三、和手写 __slots__ 打架的原文
====================================================================
TypeError: Both already specifies __slots__

====================================================================
四、frozen + slots 可以叠：既不能改也不能加
====================================================================
没有 __dict__： False
改 → FrozenInstanceError: cannot assign to field 'id'
加 → TypeError: super(type, obj): obj (instance of Lock) is not an instance or subtype of type (Lock).
  （这是 3.14 上 frozen+slots 的一个真实毛边：frozen 生成的 __setattr__ 拿的还是重建类之前的旧 cls 去 super()，报的不是 FrozenInstanceError 而是 super 检查失败；结论照旧——改也不行加也不行，只是加的报错比较怪）
repr 正常： Lock(id=7)
[退出码 0]
```

要点：

1. **字段变成了类上的 `member_descriptor`**（`类字典里 x 这个位置是什么： member_descriptor`）。
   这就是 3.14 上 `slots=True` 的实现方式：把「注解」变成「描述符」，实例不再有 `__dict__`，
   但读写 `p.x` 的体验完全不变（`实例数据照样读写：p.x = 1 ；p.tag = pt`）。
2. **和手写 `__slots__` 会打架**：`TypeError: Both already specifies __slots__`。
   也就是说要么交给 dataclass 生成，要么自己写，**不能两头都写**。
   （注意这条不是「哪个赢」的规则，是它直接拒绝你。）
3. **`frozen` + `slots` 能叠，但在 3.14 上「加新属性」的报错很怪**：
   改 → `FrozenInstanceError: cannot assign to field 'id'`（正常）；
   加 → `TypeError: super(type, obj): obj (instance of Lock) is not an instance or subtype of type (Lock).`
   捕获里那段括号说明是 demo 自己写的结论：这是 `frozen+slots` 的一个**真实毛边** ——
   `frozen` 生成的 `__setattr__` 拿的还是重建类之前的旧 `cls` 去 `super()`，
   于是报的不是 `FrozenInstanceError` 而是 `super` 检查失败。
   **结论照旧：改也不行，加也不行**，只是「加」的这条错误消息不值得你照着查文档。
   `repr 正常： Lock(id=7)` —— 两个开关叠起来不影响 `repr`。

## 14. 第 14 节 · `__slots__` 的账：省多少内存（10 万实例实测）与三个代价

上一节的选项背后是同一个机制。先把「有没有 `__dict__`」这件事看明白 ——
**实例本体两边一样大**，差别全在挂不挂那坨字典上。

```
$ python3 -c "
import sys
class Loose:
    def __init__(self, x, y):
        self.x = x; self.y = y
class Tight:
    __slots__ = (\"x\", \"y\")
    def __init__(self, x, y):
        self.x = x; self.y = y
a, b = Loose(1, 2), Tight(3, 4)
print(\"Loose 实例有 __dict__ 吗：\", hasattr(a, \"__dict__\"), \"｜ Tight 实例：\", hasattr(b, \"__dict__\"))
print(\"实例本体都是\", sys.getsizeof(a), \"字节；差额全在挂不挂 __dict__ 上（详见下一条脚本的 10 万实例总账）\")
"
Loose 实例有 __dict__ 吗： True ｜ Tight 实例： False
实例本体都是 48 字节；差额全在挂不挂 __dict__ 上（详见下一条脚本的 10 万实例总账）
[退出码 0]
```

`Loose` 实例有 `__dict__`（`True`），`Tight` 没有（`False`），而 `sys.getsizeof` 两个都是 48 字节。
所以「`__slots__` 省内存」的正确说法是：**省下的是每个实例额外挂着的那个小字典**，
不是实例本身变小。单实例看不出名堂，量大了才显形 —— 这就是下一条 10 万实例的总账。

```
$ python3 l_slots_cost.py
====================================================================
一、100,000 个实例的总账（批量口径，见脚本 docstring）
====================================================================
Loose（无 slots）合计：13,600,000 字节，摊到每实例 136 字节
Tight（有 slots）合计：4,800,000 字节，摊到每实例 48 字节
__slots__ 省了：8,800,000 字节（约为 Loose 的 64%，每实例省 88 字节）
对照单实例：本体都是 48 字节；Loose 多挂着的那坨就是它的 __dict__（dict），Tight 压根没有：hasattr(b, '__dict__') → False
```

接下来这段里有一行「造 10 万个的耗时」，**每次跑都会抖**，所以本章只把它的字节数当证据，
时间那行只当量级看（围栏里不含 `$` 提示符，是刻意的截断口径）：

```
二、造 10 万个的耗时（每次会抖，只看量级，README 别引具体数）
Loose：12 毫秒 ｜ Tight：8 毫秒
```

三个代价，每个都是原文（这三行属于上面那个脚本的输出尾部）：

```
====================================================================
三、三个代价（每个都是原文）
====================================================================
代价 1 加不上新属性 → 'Tight' object has no attribute 'extra' and no __dict__ for setting new attributes
代价 2 vars() 用不了 → TypeError: vars() argument must have __dict__ attribute
代价 3 父类没写 __slots__，子类写了也白干：hasattr(c, '__dict__') → True （c.anything = 2 照样塞得进去）
[退出码 0]
```

1. **省了多少（确定值）**：100,000 个实例，无 `slots` 合计 `13,600,000` 字节（每实例 136），
   有 `slots` 合计 `4,800,000` 字节（每实例 48），**省 `8,800,000` 字节，约 64%，每实例省 88 字节**。
   你写 DS 的时候最有感觉的场景：一个「图节点」对象要造几万个，
   每个字段固定、运行时不加新属性 —— 那就是 `__slots__` 的主场。
2. **代价 1**：加不上新属性。`'Tight' object has no attribute 'extra' and no __dict__ for setting new attributes`。
   任何「随手往对象上挂个缓存/标记」的写法都会当场崩。
3. **代价 2**：`vars()` 用不了 → `TypeError: vars() argument must have __dict__ attribute`。
   这个更阴：本章很多取证手法（第 2、3 节的 `vars(实例)`）在 `__slots__` 类上**全部失效**，
   调试时得改用 `getattr` 或 `dataclasses.asdict()`。
4. **代价 3（最容易白干的一条）**：父类没写 `__slots__`，子类写了也白干 ——
   `hasattr(c, '__dict__') → True`，`c.anything = 2` 照样塞得进去。
   因为 `__dict__` 是从父类继承来的，子类那份 `__slots__` 管不到它。
   **要用就得整条继承链都用**。

## 15. 第 15 节 · 钻石继承：`__mro__` 是名单，调用链照着名单走

到这一节为止，前面讲的都是「一个类 + 它的实例」。从这节开始讲继承，
而 Python 的多继承只有一件事需要真正搞懂：**方法查找沿 `__mro__` 这份名单往下走**。
其余全是这份名单的推论。

```
$ python3 m_mro_super.py
====================================================================
一、名单本体
====================================================================
D.__mro__ → ['D', 'B', 'C', 'A', 'object']
D.__bases__ → ['B', 'C'] ← 亲爹只有 B、C 两个
B.__mro__  → ['B', 'A', 'object']

====================================================================
二、真实调用链（打印顺序就是执行顺序）
====================================================================
D().greet():
  D.greet：先干自己的，再 super()
  B.greet：我该喊 super() 了 —— 注意接下来不是 A！
  C.greet：轮到我兜底了
  A.greet 被调（名单终点站）
↑ 四行按 D→B→C→A 出现：C 抢在 A 前面，因为它是名单里 B 的下一家。
  这就是「super 不是调用父类，是沿 MRO 往后走」的铁证。

====================================================================
三、同一份 B.greet，站的位置不同，super() 找的人就不同
====================================================================
super(B, d_inst).greet.__func__ is C.greet → True
super(B, b_inst).greet.__func__ is A.greet → True
↑ 同一段 super() 代码，实例是 D 就去找 C，实例是 B 就去找 A：
  找谁 = 实例所属类的 MRO 里 B 的下一家，与「写在哪个类里」无关。
[退出码 0]
```

三段里最值钱的是第二段。`D.__mro__` 是 `['D', 'B', 'C', 'A', 'object']` ——
注意 `D` 有两个亲爹（`D.__bases__ → ['B', 'C']`），但名单把 `A` 排在了最后，`B` 和 `C` 都不会重复出现。
然后看真实执行顺序：`D.greet` → `B.greet` → `C.greet` → `A.greet`。

**关键反直觉点：`B.greet` 里写的是 `super()`，可它接的是 `C`，不是 `B` 的亲爹 `A`。**
`B` 自己的名单（`B.__mro__ → ['B', 'A', 'object']`）里 `B` 的下一名确实是 `A`，
但这一轮跑的实例是 `D`，`D` 的名单里 `B` 的下一名是 `C` —— 所以 `C` 抢在 `A` 前面兜底。
demo 打的旁白就是这一条的铁证：「super 不是调用父类，是沿 MRO 往后走」。

第三段把这件事说成了可测的式子：`super(B, d_inst).greet.__func__ is C.greet → True`，
`super(B, b_inst).greet.__func__ is A.greet → True`。
**同一段 `super()` 代码，换个实例就换个去处。**

## 16. 第 16 节 · `super()` 找谁，由「实例的 MRO」定，不由「写在哪」定

这一节把上面第三段单独拎出来，用最少的类再证一次 —— 因为这是读别人库代码时最容易踩空的地方。

```
$ python3 -c "
class A:
    def hi(self): return \"A\"
class B(A):
    def hi(self): return \"B\"
class C(A):
    def hi(self): return \"C\"
class D(B, C):
    pass
d = D()
print(\"D.__mro__ →\", [c.__name__ for c in D.__mro__])
print(\"super(B, d).hi.__func__ 落在哪：\", super(B, d).hi.__func__(d),
      \"← B 的下一家是 C，不是亲爹 A\")
print(\"super(C, d).hi.__func__ 落在哪：\", super(C, d).hi.__func__(d))
b = B()
print(\"换 B 自己的实例：super(B, b).hi 落在哪：\", super(B, b).hi.__func__(b))
"
D.__mro__ → ['D', 'B', 'C', 'A', 'object']
super(B, d).hi.__func__ 落在哪： C ← B 的下一家是 C，不是亲爹 A
super(C, d).hi.__func__ 落在哪： A
换 B 自己的实例：super(B, b).hi 落在哪： A
[退出码 0]
```

四行读法：

- `D.__mro__` → `['D', 'B', 'C', 'A', 'object']`。`D` 里什么都没写（`pass`），名单是自动算出来的。
- `super(B, d).hi.__func__` 落在 **`C`**：`B` 的下一家是 `C`，不是亲爹 `A`。
- `super(C, d).hi.__func__` 落在 `A`：名单里 `C` 后面就是 `A`。
- 换成 `B` 自己的实例 `b`，`super(B, b)` 立刻改口去找 `A`。

`super(X, obj)` 的语义就是「**从 `type(obj).__mro__` 里 `X` 的下一名开始找**」。
它压根不关心 `X` 和 `obj` 有没有父子关系，也不关心你这段代码写在哪个类的文件里。
推论（也是为什么协作式多继承能成立）：只要每层都老实地 `super()` 往后传，
名单上的每一家就都会被叫到一次，且 `A`（共用祖先）只被叫一次 —— 钻石不会重复打洞。

## 17. 第 17 节 · 类变量在继承里：`+=` 悄悄建遮挡物，可变类变量全体一起脏

第 3 节讲过「实例字典遮住类属性」。这一节把它推到继承上，得到两条会咬人的规则。

```
$ python3 n_shared_classvar.py
====================================================================
一、ViaClass：全类一张计数器
====================================================================
两个实例 + 一个子类实例构造后：ViaClass.n = 3
子类没自己的 n，抬头读到同一个：ViaSub.n = 3
vars(ViaSub) 里有 n 吗： False ← 子类字典没添过东西

====================================================================
二、ViaSelf：+= 在实例上顺手建了遮挡物
====================================================================
ViaSelf.n = 0 ← 类上那份从头到尾没动过
s1 的实例字典： {'n': 1} ｜ s2 的： {'n': 1}
s1.n = 1 （读的是自己的），s2.n = 1 （也读自己的）
拆步骤看真相：self.n += 1 拆成「读类上的 → 写实例的」：
  刚 new 出来 vars(fresh) = {'n': 1} （__init__ 里那次 += 已经写了它自己的 n）

====================================================================
三、SharedBox：可变类变量，append 不建遮挡物，全体一起脏
====================================================================
只给 b1 塞了一发：b1.items = [999] ｜ b2.items = [999]
b1.items is b2.items is SharedBox.items → True ← 三个名字一块字典里的同一个 list
（和第 10 节可变默认值一个病根：共享的是对象，不是名字。第 02 章的老账。）
[退出码 0]
```

三段各一条规则：

1. **`ViaClass`：全类一张计数器**。两个实例 + 一个子类实例构造完，`ViaClass.n = 3`。
   子类 `ViaSub` 抬头读到的是同一个（`ViaSub.n = 3`），而且 `vars(ViaSub)` 里**根本没有 `n`** ——
   子类字典没添过东西，它只是顺着继承链找到了父类那份。
2. **`ViaSelf`：`self.n += 1` 会在实例上顺手建一个遮挡物**。
   类上那份从头到尾是 `0`（`ViaSelf.n = 0`），可每个实例自己变成了 `1`：
   `s1 的实例字典： {'n': 1}`。拆步骤看穿它：`+=` 是**「读类上的 → 写实例的」**，
   写入永远写实例字典。demo 里那句 `刚 new 出来 vars(fresh) = {'n': 1}` 就是当场抓到的证据。
   这条的杀伤力在于：**读的时候看不出区别**（`s1.n` 就是 `1`，你以为读的是类变量），
   只有拿 `vars()` 一比才露馅。
3. **`SharedBox`：可变类变量 + `append` = 全体一起脏**。
   只给 `b1` 塞了一发，`b2.items` 也变 `[999]`，因为
   `b1.items is b2.items is SharedBox.items → True` —— 三个名字指向同一个 `list`。
   `append` 是「就地改」，不产生任何写入，所以**不会**建遮挡物。
   和第 10 节可变默认值、第 02 章「共享的是对象不是名字」是同一个病根的第三次发作。

一句话总结：**赋值（含 `+=`）建遮挡物；就地修改（`append`/`[...]=`）穿过遮挡物改到共享的那个对象上。**

## 18. 第 18 节 · `isinstance` 认户口，`type()` 认现形

前面用了两节的继承，这一节给「你是不是某个类」这件事把两个工具分清楚。

```
$ python3 -c "
class Animal: pass
class Dog(Animal): pass
d = Dog()
print(\"isinstance(d, Animal)：\", isinstance(d, Animal), \"｜ type(d) is Animal：\", type(d) is Animal)
print(\"type(d).__name__：\", type(d).__name__)
print(\"isinstance(d, (Animal, int))：\", isinstance(d, (Animal, int)), \"← 元组一并问\")
"
isinstance(d, Animal)： True ｜ type(d) is Animal： False
type(d).__name__： Dog
isinstance(d, (Animal, int))： True ← 元组一并问
[退出码 0]
```

```
$ python3 o_isinstance_type.py
====================================================================
一、三行看清差别
====================================================================
isinstance(d, Animal)      → True ← 认户口：曾祖是 Animal 就算
type(d) is Animal          → False ← 只认现形：你本体是 ToyDog
type(d) is ToyDog          → True ｜ isinstance(d, ToyDog) → True

====================================================================
二、isinstance 支持元组（一条顶三条）
====================================================================
[True, True, True]
↑ 第二参数是元组时，「是其中任何一家的户口」都算 True

====================================================================
三、type 的另一副面孔：三参数工厂（动态造类）
====================================================================
type(Dyn).__name__ = type ｜ isinstance(obj, Animal) → True
obj.sound() → 叮
↑ class Dyn(Animal): def sound... 编译完就是这个三元调用；「class 是字典」的另一半证据

====================================================================
四、dataclass 也认：isinstance / type 照继承链
====================================================================
isinstance(dv, Base) → True ｜ type(dv) is Base → False
但判等不认户口！Base(1) == Derived(1, 2) → False （第 12 节：生成的 __eq__ 要求同类）
[退出码 0]
```

四段要点：

1. **一句话分辨**：`isinstance(d, Animal)` → `True`（认户口：曾祖是 `Animal` 就算），
   `type(d) is Animal` → `False`（只认现形：你本体是 `ToyDog`）。
   默认选 `isinstance` —— 它是「按继承链问话」的那个。
2. **`isinstance` 支持元组**：第二参数给元组时，「是其中任何一家的户口」都算 `True`
   （`[True, True, True]`）。`isinstance(d, (Animal, int))` 一条顶三条。
3. **`type` 的另一副面孔：三参数工厂**。`type(名字, 基类元组, 类字典)` 能**动态造类**
   （`type(Dyn).__name__ = type`、`isinstance(obj, Animal) → True`、`obj.sound() → 叮`）。
   这一条是第 2 节「类是字典」的另一半证据：`class Dyn(Animal): ...` 编译完就是这个三元调用。
4. **`dataclass` 也认这套**：`isinstance(dv, Base) → True`，`type(dv) is Base → False` ——
   户口照继承链。**但判等不认户口**：`Base(1) == Derived(1, 2) → False`
   （回指第 12 节：生成的 `__eq__` 要求同类）。
   所以「`isinstance` 说是，`==` 说不」在子类 dataclass 上是**正常现象**，不是 bug。

## 19. 第 19 节 · `__init_subclass__` 与 ABC：定义子类那一刻的钩子

「有人继承我，我要做点事」—— Python 确实给了这个钩子，而且它住的还是类字典里（第 2 节的延伸）。

```
$ python3 -c "
import abc
class Shape(abc.ABC):
    @abc.abstractmethod
    def area(self): ...
Shape()
"
Traceback (most recent call last):
  File "<string>", line 6, in <module>
    Shape()
    ~~~~~^^
TypeError: Can't instantiate abstract class Shape without an implementation for abstract method 'area'
[退出码 1]
```

先演最普及的那个用法：抽象类不许实例化。`TypeError: Can't instantiate abstract class Shape without an implementation for abstract method 'area'` ——
3.14 的这句原文比早期版本的「abstract」含糊说法长得多，直接告诉你**缺哪个方法**。

```
$ python3 p_subclass_hooks.py
====================================================================
一、__init_subclass__：class 语句执行的那一刻就被钩
====================================================================
registry = ['导入器', 'Exporter'] ← 没人调用登记函数，定义子类时自动进表
跑起来： Importer 在跑 ｜ Exporter 在跑
__init_subclass__ 也住在类字典里，类型是 classmethod ← 它自己也不过是类字典里的一个物件（第 2 节的延伸）

====================================================================
二、ABC：抽象类不许实例化（捕获版原文；裸 traceback 见 demo 下一条命令）
====================================================================
TypeError: Can't instantiate abstract class Shape without an implementation for abstract method 'area'
补了 area 就能生：Square(3).area() = 9
isinstance(Square(1), Shape) → True
[退出码 0]
```

两段要点：

1. **`__init_subclass__` 自动登记**：`registry = ['导入器', 'Exporter']` ——
   **没人调用过任何登记函数**，定义子类的那一刻父类钩子就把它塞进表了。
   插件式代码（第 1 章讲过的「注册表」写法）在 Python 里就靠这个，不用在每个子模块末尾手写一行 `register(X)`。
   然后 `Importer 在跑 ｜ Exporter 在跑` 说明表真能用。
   细节：`__init_subclass__` 也住在类字典里，类型是 `classmethod` —— 它自己也不过是类字典里的一个物件。
2. **`abc` 就是把这个钩子拿来用**：补了 `area` 就能生（`Square(3).area() = 9`），
   并且 `isinstance(Square(1), Shape) → True`。
   也就是说「抽象」这件事**没有语言级机关**：它是 `ABCMeta`（一个元类）在实例化那一刻做的检查，
   报错也是普通 `TypeError`（第 24 节汇总里能看到 `abc.ABC` / `abc.abstractmethod` 两个公开件）。

## 20. 第 20 节 · 名字改写：`__private` 在字典里的真名是 `_类名__private`

两个前导下划线、结尾不到两个下划线的名字，编译器会**在编译期**把它改名。
这一节把「改成了什么、跟着谁改」全部现场取证。

```
$ python3 q_mangling.py
====================================================================
一、实例字典里的真名
====================================================================
vars(v) = {'_Vault__secret': '库房钥匙'}
写 self.__secret，存的键其实是： ['_Vault__secret']

====================================================================
二、类字典里的真名；协议名原样保留
====================================================================
相关键： ['_Vault__max', '_Vault__peek', '__init__']
__max 变成 _Vault__max 在类字典： True ｜ 原名 __max 还在吗： False
__init__ 改写了吗： True ← 前后都双下划线 = 协议名，豁免

====================================================================
三、外面想用「私」名字：字面名撞 AttributeError，真名倒是随便拿
====================================================================
v.__secret → 'Vault' object has no attribute '__secret'
  （这句写在模块里，没被改写，它真去找一个叫 '__secret' 的键 —— 而那个键从没存在过）
在别的类体里写 obj.__secret → 'Vault' object has no attribute '_Nosy__secret'
  （报错自报 _Nosy__secret：改写跟着「写代码的类」走，不是跟着被访问的对象）
真名直取：v._Vault__secret → 库房钥匙 ← 拿到，mangling 不是安全盒
v.show()（类体内正规通路）→ 库房钥匙

====================================================================
四、改写的真实目的：父子撞名互不干扰
====================================================================
Vault.__max 的真身： 100 ｜ SubVault 的同名 __max 真身： 777
_Vault__max 出现在 SubVault 自己的类字典里吗： False ← 各存各的键，这就是改写的全部意义
[退出码 0]
```

四段要点：

1. **实例字典里的真名**：`vars(v) = {'_Vault__secret': '库房钥匙'}` ——
   你写 `self.__secret`，存进去的键是 `'_Vault__secret'`。`__secret` 这个键**从来没存在过**。
2. **类字典同理，但协议名豁免**：`相关键： ['_Vault__max', '_Vault__peek', '__init__']`。
   `__max` → `_Vault__max`（原名不在了），而 `__init__` 原样保留 ——
   **前后都是双下划线**的名字属于协议名，不参与改写。
3. **报错信息会出卖改写规则（这一条最有用）**：
   - 在模块里写 `v.__secret` → `'Vault' object has no attribute '__secret'`：这行没被改写，它真去找 `__secret`。
   - 在**别的类体**里写 `obj.__secret` → `'Vault' object has no attribute '_Nosy__secret'`：
     报错自己报出了 `_Nosy__secret`，**改写跟着「写代码的那个类」走，不跟着被访问的对象走**。
   - 真名直取：`v._Vault__secret → 库房钥匙` —— 拿到。**所以 mangling 不是安全盒，从来不是。**
   - 正规通路 `v.show()`（类体内写 `self.__secret`）→ `库房钥匙`。
4. **改写的真实目的：父子撞名互不干扰**。
   `Vault.__max` 真身 `100`，`SubVault` 的同名 `__max` 真身 `777`，
   而 `_Vault__max` **不在** `SubVault` 自己的类字典里 —— 各存各的键。
   这就是改写的全部意义：防「子类无意覆盖父类的内部名字」，**不是**防外部访问。

给你的实操结论：想表达「别在外部用」，写**一个**下划线 `_price`（本章第 8 节就是这么写的）；
`__` 只在「父类内部实现、怕被子类撞名」时才用。

## 21. 第 21 节 · `__call__`：实例为什么能带括号 —— `f(x)` 原来是 `type(f).__call__(f, x)`

函数对象能 `f(x)`，实例也能 —— 只要类里写了 `__call__`。这一节把「调用」这个动作拆成一次协议查找。

```
$ python3 r_callable.py
====================================================================
一、实例直接带括号调用
====================================================================
m2(10) → 20 ｜ m5(10) → 50
等价写法 type(m2).__call__(m2, 10) → 20
顺手塞进 map：list(map(m2, [1, 2, 3])) → [2, 4, 6]

====================================================================
二、有状态的调用对象：替代闭包的另一种写法
====================================================================
t() ×3 → [1, 2, 3] ｜ t.hits = 3
isinstance(m2, Callable)（collections.abc）→ True
普通实例呢：isinstance(42, Callable) → False ｜ isinstance(t, Callable) → True
三参数 type() 造类时给上 __call__ 也能用：
  Invoker()() → 我也能被调用
[退出码 0]
```

三段要点：

1. **等价写法实测**：`m2(10) → 20`，`type(m2).__call__(m2, 10) → 20`。
   和第 4 节 `obj.f(x)` ≡ `A.f(obj, x)` 是同一个套路 —— **运算符/调用都是「去类的字典里找那个函数」**。
   注意找的是 `type(m2)` 的字典，不在实例字典里翻（隐式协议查找的规矩，第 5 节已经见过一次）。
2. **能直接当函数用**：`list(map(m2, [1, 2, 3])) → [2, 4, 6]`。
   凡是收「可调用对象」的地方（`map`、`sorted(key=...)`、回调）都能塞实例。
3. **有状态的调用对象 = 闭包的另一种写法**：`t() ×3 → [1, 2, 3] ｜ t.hits = 3`。
   计数器那种「每次调用记住上回」的逻辑，闭包用 `nonlocal`，这里用字段 ——
   字段版本**可以被外面读、可以重置、可以加第二个方法**，这是闭包给不了的。
4. **`collections.abc.Callable` 判的就是这个**：`isinstance(m2, Callable) → True`，
   `isinstance(42, Callable) → False`。
   最后一行还有个回环：用第 18 节的三参数 `type()` 造类时给上 `__call__`，
   造出来的实例照样能被调用（`Invoker()() → 我也能被调用`）——
   「类是字典」和「调用是协议」两条线在这里接上了。

## 22. 第 22 节 · `__getattr__` 是兜底，`__getattribute__` 是全量拦截（写错死循环）

第 3 节的查找链是「先实例字典，再类字典，再往上继承链」。
这一节给你两个**插入这条链**的口子，并现场演一次写错会怎样。

```
$ python3 -u -c "
class Good:
    def __init__(self):
        self.data = {\"x\": 1}
    def __getattribute__(self, name):
        print(\"    拦截到：\", name)
        return super().__getattribute__(name)   # 正确姿势：把活交回给 object
g = Good()
print(\"g.data →\", g.data)
"
    拦截到： data
g.data → {'x': 1}
[退出码 0]
```

这是**正确姿势**的对照：`__getattribute__` 每次属性访问都被叫（`拦截到： data`），
里面必须用 `super().__getattribute__(name)` 把活交回给 `object`，才不至于自己拦死自己。

```
$ python3 -u s_getattr.py
====================================================================
一、__getattr__：只在「查无此人」时被叫
====================================================================
读已有的 sp.real → 正主属性 （__getattr__ 没出声，正常链路直接命中）
    （__getattr__ 被叫了：'ghost' 走正常查找没找到）
读不存在的 sp.ghost → 伪造的 ghost
    （__getattr__ 被叫了：'number42' 走正常查找没找到）
读不存在的 sp.number42 → 伪造的 number42

====================================================================
二、__getattribute__ 写错的死循环现场（接下来崩）
====================================================================
b = Boom() 构造本身不崩（赋值走 __setattr__，不经过 __getattribute__），
崩的是下一句读属性：
现在执行 b.x，看它自己咬自己：
Traceback (most recent call last):
  File "/tmp/py06-AF72VU/work/s_getattr.py", line 46, in <module>
    print(b.x)
          ^^^
  File "/tmp/py06-AF72VU/work/s_getattr.py", line 35, in __getattribute__
    return self.__dict__[name]      # ← 取 self.__dict__ 本身也要走 __getattribute__！
           ^^^^^^^^^^^^^
  File "/tmp/py06-AF72VU/work/s_getattr.py", line 35, in __getattribute__
    return self.__dict__[name]      # ← 取 self.__dict__ 本身也要走 __getattribute__！
           ^^^^^^^^^^^^^
  File "/tmp/py06-AF72VU/work/s_getattr.py", line 35, in __getattribute__
    return self.__dict__[name]      # ← 取 self.__dict__ 本身也要走 __getattribute__！
           ^^^^^^^^^^^^^
  [Previous line repeated 996 more times]
RecursionError: maximum recursion depth exceeded
[退出码 1]
```

三段要点：

1. **`__getattr__` 只在「查无此人」时被叫**：读 `sp.real` 时它一声不吭（正常链路直接命中）；
   读 `sp.ghost`、`sp.number42` 时才出声（`__getattr__ 被叫了：'ghost' 走正常查找没找到`）。
   适合做「动态代理 / 属性名映射 / 友好的 AttributeError」。
2. **`__getattribute__` 是每次都要过**：所以它里面**任何**一次 `self.任何东西` 都会再进它自己。
   错误版写的是 `return self.__dict__[name]` —— 光看像「直接掏字典，绕开查找」，
   其实**取 `self.__dict__` 这一步本身就是一次属性访问**，于是又回到 `__getattribute__`。
3. **死循环现场**（demo 特意让脚本自己崩掉，好让你看到 traceback 长什么样）：
   traceback 里同一行 `line 35` 连打三次，然后 `[Previous line repeated 996 more times]`，
   最后 `RecursionError: maximum recursion depth exceeded`，退出码 1。
   另外注意 demo 前面那句提醒：**构造本身不崩**（赋值走 `__setattr__`，不经过 `__getattribute__`），
   崩的是下一句读属性 —— 所以这类 bug 的现场往往离真正的错处隔了一行。
   两条活路：`super().__getattribute__(name)`，或 `object.__getattribute__(self, name)`。

## 23. 第 23 节 · 半个上下文管理器：`__enter__` / `__exit__`（第 07 章的钩子）

`with` 你已经在前面的章节里当资源回收用过了。这里先把它的**两个函数**摊开，
异常怎么传、返回值怎么用留给第 07 章正式讲。

```
$ python3 t_context_manager.py
====================================================================
一、正常走完
====================================================================
[前门] __enter__：开门，欢迎光临
  屋里干活，手里的 as 对象是 Door 实例吗：True
[前门] __exit__：平安出门，锁门

====================================================================
二、屋里出异常：__exit__ 收到三元组，然后异常继续飞
====================================================================
[后门] __enter__：开门，欢迎光临
[后门] __exit__：摔了个跤（RuntimeError: 屋里水管爆了），照样锁门
  外面还是接住了：RuntimeError: 屋里水管爆了

====================================================================
三、__exit__ 返回 True：异常就地消失（危险，演示用）
====================================================================
[Silence] 本来有个 ValueError，被我 return True 咽了
跑到这说明 ValueError 真被咽了 —— 生产代码别这么干，至少打个日志
[退出码 0]
```

三段就是三个问题：

1. **正常走完**：`__enter__` 先进（开门），`as` 拿到的就是 `Door` 实例本身（`手里的 as 对象是 Door 实例吗：True`），
   出门时 `__exit__` 后到（锁门）。
2. **屋里出异常**：`__exit__` 收到异常三元组，所以它照样执行 ——
   `摔了个跤（RuntimeError: 屋里水管爆了），照样锁门`，然后**异常继续往外飞**
   （`外面还是接住了：RuntimeError: 屋里水管爆了`）。
   这就是「用 `with` 而不是手写 `try/finally`」的全部理由：清理必然发生，且不吞异常。
3. **`__exit__` 返回 `True` = 就地咽掉异常**：`本来有个 ValueError，被我 return True 咽了`，
   下一行「跑到这说明 ValueError 真被咽了」。**危险，只是演示** ——
   生产代码真要吃异常，至少得打日志，否则 bug 会彻底消失在任何观测里。

## 24. 第 24 节 · 取证手段汇总：这一章用到的标准库入口

本章没有任何第三方依赖。把用过的入口列一遍，方便你自己复用这套手法。

```
$ python3 -c "
import dataclasses, inspect, abc, collections.abc, sys
print(\"dataclasses 公开件：\", [n for n in (\"dataclass\", \"field\", \"fields\", \"replace\", \"FrozenInstanceError\") if hasattr(dataclasses, n)])
print(\"inspect.signature / isclass：\", hasattr(inspect, \"signature\"), hasattr(inspect, \"isclass\"))
print(\"abc.ABC / abstractmethod：\", hasattr(abc, \"ABC\"), hasattr(abc, \"abstractmethod\"))
print(\"collections.abc.Callable：\", hasattr(collections.abc, \"Callable\"))
"
dataclasses 公开件： ['dataclass', 'field', 'fields', 'replace', 'FrozenInstanceError']
inspect.signature / isclass： True True
abc.ABC / abstractmethod： True True
collections.abc.Callable： True
[退出码 0]
```

```
$ python3 -c "
from dataclasses import dataclass
@dataclass(frozen=True, slots=True)
class DC:
    x: int
print(\"slots=True 是 3.10 加的参数，本机 3.14 可用：\", DC.__slots__)
"
slots=True 是 3.10 加的参数，本机 3.14 可用： ('x',)
[退出码 0]
```

`slots=True 是 3.10 加的参数，本机 3.14 可用： ('x',)` —— 这一条是给第 13 节补的版本号证据。

本章反复用到的取证动作，一共四类：

| 想知道 | 用的入口 | 本章哪一节用它证明了什么 |
| --- | --- | --- |
| 类里到底有什么 | `vars(类)` / `vars(实例)` | 第 2、3、8、9、17、20 节：方法在类字典、`+=` 在实例字典建遮挡物、真名是 `_Vault__secret` |
| 生成的函数长什么样 | `inspect.signature` | 第 9 节：`dataclass` 替你写的 `__init__` 参数表 |
| 字段清单（含默认值来源） | `dataclasses.fields` | 第 9、10 节：`ClassVar` 不在字段里；`default_factory` 存的是 `list` |
| 沿哪条链找方法 | `类.__mro__` / `类.__bases__` / `super(X, obj).__func__` | 第 15、16 节：`super` 找的是名单下一名 |

## 25. 第 25 节 · 收尾：证明这一章没在手册目录里留下任何产物

本章的取证脚本是 20 个 `.py` 文件，demo 把它们**整份复制进沙箱**再运行（`cp -R` 到 `$SB/work`），
所以章目录应该永远是干净的。

```
$ 〔在 06-classes-and-dataclasses 里〕 find . \( -name '__pycache__' -o -name '*.pyc' \) | wc -l
0
[退出码 0]
```

```
$ 〔在 06-classes-and-dataclasses 里〕 ls | wc -l
21
[退出码 0]
```

```
$ ls
a_class_dict.py
b_self_param.py
c_init_return.py
d_repr_str.py
demo.sh
e_eq_hash.py
f_property_trio.py
g_dataclass_intro.py
h_dc_mutable_default.py
i_dc_frozen.py
j_dc_eq_order.py
k_dc_slots.py
l_slots_cost.py
m_mro_super.py
n_shared_classvar.py
o_isinstance_type.py
p_subclass_hooks.py
q_mangling.py
r_callable.py
s_getattr.py
t_context_manager.py
[退出码 0]
```

```
$ ls __pycache__
ls: cannot access '__pycache__': No such file or directory
[退出码 2]
```

**`0` 这个数字必须是 0**：`__pycache__` / `.pyc` 一个都没生出来 ——
因为本章全程 `python3 xxx.py` 直接跑脚本，**从没 `import` 过章目录里的示例模块**
（`dataclasses` 是标准库，缓存生在 `/usr/lib` 且只读）。
最后一条退出码 2 是**故意的**：`ls __pycache__` 报 `No such file or directory`，
即沙箱里同样没有缓存目录 —— 它是「没有」的证据，不是失败。

**关于那个 `21`（诚实说明，别照抄成今天的数）**：`ls | wc -l` 跑在章目录里，
`21` = 20 个取证脚本 + `demo.sh`；上面那份 `ls` 是沙箱 `work` 目录（同样 21 项，`cp -R` 进去的家当）。
这两个数都是**捕获那一刻**的：当时章目录里**还没有 README.md**（正文是捕获之后才写的）。
今天你再跑，`ls | wc -l` 会得到 `22`，多出来的那一项就是 `README.md` ——
它是被复制进沙箱的，不会在章目录里产生任何新东西。**结论不变：产物计数仍是 `0`。**

---

## 对照表

### C 里的概念 vs Python 的类（全章汇总）

| 你在 C 里的概念 | Python 的真实情况 | 本章证据 |
| --- | --- | --- |
| `struct` + 一组函数 | 类是**一个字典**（`mappingproxy`），函数（方法）就放在这个字典里 | 第 2 节 |
| `s.x` 是内存偏移，编译期定死 | `s.x` 是**一次运行期查找**：实例字典 → 类字典 → 继承链 | 第 3、22 节 |
| `foo(&s)` 手动传结构体指针 | `self` 就是普通第一个参数，`obj.f(x)` ≡ `A.f(obj, x)` | 第 4 节 |
| 构造函数 `init(struct*)`，可返回错误码 | `__init__` **必须返回 `None`**，返回别的当场 `TypeError` | 第 5 节 |
| `printf("%s")` 自己写格式化 | `print`/`str()`/裸 f-string 走 `__str__`；`%r`、容器打印走 `__repr__` | 第 6 节 |
| 比较函数 `int cmp(const void*, const void*)` | 写 `__eq__` 就**自动失去 `__hash__`**，进不了 set、当不了 dict 键 | 第 7 节 |
| getter/setter 手写两个函数 | `@property`：读起来像字段，其实是类字典里的一个 descriptor | 第 8 节 |
| 手写 `struct` 的初始化函数 | `@dataclass` 替你写 `__init__`/`__repr__`/`__eq__`，仅此而已 | 第 9~12 节 |
| 默认值是「调用时现造的」 | 可变默认值只造一次（第 05 章的老坑）；`dataclass` 在**定义那一刻**就 `ValueError` 拒绝 | 第 10 节 |
| `const struct` | `@dataclass(frozen=True)`：`__setattr__` 被替换，`FrozenInstanceError`（`AttributeError` 的儿子） | 第 11 节 |
| `qsort` 里比较大小 `<` | dataclass 默认**不给** `<`（要 `order=True`），比的是字段元组字典序 | 第 12 节 |
| 结构体字段固定、无多余开销 | 普通实例每多一个 `__dict__`：10 万实例实测 136 → 48 字节/个 | 第 13、14 节 |
| 函数指针表 / vtable 派发 | `super()` 沿 `type(obj).__mro__` 往后走，**与代码写在哪个类无关** | 第 15、16 节 |
| 全局变量共享 | 可变类变量全体共享；`self.x += 1` 会**悄悄建实例遮挡物** | 第 17 节 |
| `sizeof` 判类型、靠约定区分多态 | `isinstance` 认户口（含继承），`type()` 认现形 | 第 18 节 |
| `static` / 类方法 | `__init_subclass__` 是「定义子类那一刻」的钩子，`abc` 只是它的用户 | 第 19 节 |
| 文件内 `static` 的「私有」 | `__priv` 只是**改名**成 `_类名__priv`，真名随时取得到；不是安全边界 | 第 20 节 |
| 函数名即地址（函数指针） | 实例带括号 ≡ `type(f).__call__(f, x)` | 第 21 节 |
| 手动 `free` / goto 统一清理出口 | `__enter__`/`__exit__`：清理必然执行；`__exit__` 返回 `True` 会**吞掉异常** | 第 23 节 |

## 一页纸（考前扫这半页）

### 三条判据，覆盖本章全部结论

下面三段**不是捕获原文**（是本章脚本里用过的判据，抽出来给你现抄，本机都跑过同型命令）：

```python
vars(SomeClass)                 # 类到底有什么：方法、property、@classmethod 全在这里
vars(some_instance)             # 实例自己有什么：字段 / 被 += 造出来的遮挡物；有 __slots__ 时 TypeError
SomeClass.__mro__               # super() 照着这份名单走；查属性也是这个顺序
```

```python
# 「它到底可不可哈希 / 能不能比」，一行看穿
cls.__hash__ is None            # 写了 __eq__ 就会是 True —— set/dict 立刻拒你
cls.__dict__.get('__lt__')      # dataclass 只有 order=True 才有这个东西
dataclasses.fields(cls)         # 字段清单：ClassVar 不在，default_factory 在这里看
```

```python
# 属性访问的两个插入点，记一句话就够
# __getattr__      = 查无此人时才叫（兜底）
# __getattribute__ = 每次必叫，里面只能用 super()/object.__getattribute__ 交回，否则 RecursionError
```

### dataclass 开关速查（本章只演了这三个 + 一个 `default_factory`）

| 写法 | 本章演过 | 实测到的效果 |
| --- | --- | --- |
| `@dataclass`（默认） | 第 9、12 节 | 生成 `__init__`/`__repr__`/`__eq__`；**无** `<`；**不可哈希**（因为 `__eq__`） |
| `@dataclass(order=True)` | 第 12 节 | 补 `<`（及一族比较），按字段元组字典序；`sorted` 直接可用 |
| `@dataclass(frozen=True)` | 第 11 节 | 赋值/删除报 `FrozenInstanceError`；**白送 `__hash__`**，能进 set；改法用 `replace()` |
| `@dataclass(slots=True)` | 第 13 节 | 真删掉 `__dict__`，字段变 `member_descriptor`；加新属性当场 `AttributeError`；与手写 `__slots__` 互斥 |
| `field(default_factory=list)` | 第 10 节 | 每建实例现造一个；`fields()` 里 `default=MISSING`、`default_factory` 存的是 `list` |

`frozen` 与 `slots` 可以叠（第 13 节实测：改也不行加也不行，只是「加」的报错文案在 3.14 上很怪）。

## 这章的坑（按代价从大到小）

1. **写了 `__eq__` 忘了 `__hash__`**（第 7 节）：解释器主动把 `__hash__` 设成 `None`
   （不是「没继承到」）。实例**进 set、当 dict 键全部当场 `TypeError`**，
   而且报错有三种不同原文。代价最大的一条：通常要等到你把对象放进缓存/去重集合那天才炸。
2. **以为可变类变量属于这个类**（第 17 节）：`SharedBox.items.append(...)` 全体一起脏，
   `is` 一比三个名字同一个 `list`。**读的时候完全看不出问题**，只有对比才露馅。
3. **`self.x += 1` 想改类变量**（第 17 节）：改不动，只在实例上建了个遮挡物；
   类上那份永远是 `0`。要改类变量得写 `Cls.x += 1`。
4. **把 `__repr__` / `__str__` 的效果记反**（第 6 节）：只有 `__repr__` 时 `str()` 会退回它；
   **只有 `__str__` 时 `repr()` 绝不退回它**（还是那串带地址的 `<...object at 0x...>`），
   于是 `[t]`、`{1: t}` 里看到的一律是默认 repr。调试日志最容易踩。
5. **把 `@dataclass` 当「不可变」或「有排序」用**（第 11、12 节）：默认没有 `<`（`TypeError`），
   默认不 frozen（能改字段）；而 frozen 只挡字段**赋值**，字段指向的 list 仍然能 `append`（本章未演，见「本章没演」）。
6. **`__init__` 里 `return self` / `return 别的东西`**（第 5 节）：`TypeError: __init__() should return None, ...`，
   而且是从 `Cls(...)` 那行报出来的，容易误以为构造函数没跑成 —— 它跑完了，只是返回值不合规。
7. **`__getattribute__` 里写 `self.__dict__[name]`**（第 22 节）：自己咬自己，
   `RecursionError: maximum recursion depth exceeded`，traceback 里 `[Previous line repeated 996 more times]`。
8. **`slots=True` 之后还用 `vars(实例)` 调试**（第 14 节）：`TypeError: vars() argument must have __dict__ attribute`；
   改用 `dataclasses.asdict()`。另外父类没写 `__slots__`，子类写了**也白干**（`__dict__` 继承下来了）。
9. **以为 `__private` 是访问控制**（第 20 节）：`v._Vault__secret` 直接拿到。
   它只防父子撞名。跨类访问时那条 AttributeError 里的名字是 `_Nosy__secret`，跟着「写代码的类」改名。
10. **`__exit__` 里 `return True`**（第 23 节）：异常就地消失，「跑到下一行」成了唯一现场证据。
    这是本章里唯一一条**完全没有报错**的坑，所以最难查。
11. **忘传 `default_factory`，写 `items: list = []`**（第 10 节）：好消息是 `dataclass` 在定义那一刻
    就 `ValueError: mutable default <class 'list'> for field items is not allowed: use default_factory`。
    手写 `def __init__(self, items=[])` 可没人拦你 —— 那就是第 05 章那个「默认值只造一次」的坑。

## 自检问题

1. 一个类只写了 `__eq__`。说出 `Cls.__hash__` 的值，以及 `{obj}`、`{obj: 1}`、`hash(obj)`
   三条各自的报错原文。（第 7 节）
2. `@dataclass(frozen=True)` 的实例：为什么它**能**进 set，而不 frozen 且带 `__eq__` 的 dataclass 不能？
   要改一个字段，正确写法是什么？（第 11 节）
3. `class T: def __str__: ...`（只有 `__str__`）。写出 `print(t)`、`repr(t)`、`str([t])` 三者分别是什么。（第 6 节）
4. 为什么 `@dataclass` 里 `items: list = []` 会当场报错，而 `items: list = field(default_factory=list)` 不会？
   用 `fields()` 看到的 `default` 各是什么？（第 10 节）
5. `D.__mro__ = ['D','B','C','A','object']`。`B.greet` 里的 `super().greet()` 调到谁？
   把实例换成 `B` 自己的再答一次。说出判据。（第 15、16 节）
6. `s.n += 1`（`n` 是类变量）之后，`vars(s)` 和 `Cls.n` 各是什么？为什么「读」看不出问题？（第 17 节）
7. `isinstance(x, Animal)` → `True` 但 `type(x) is Animal` → `False`，两种情况各举一例。
   `type` 的第二副面孔是什么？写出三参数形式。（第 18 节）
8. 在别的类里写 `obj.__secret`，报错里的属性名是 `__secret` 还是 `_Nosy__secret`？为什么？（第 20 节）
9. `m2(10)` 的等价展开式是什么（用 `type` 和 `__call__` 写）？`isinstance(42, Callable)` 是 True 还是 False？（第 21 节）
10. `__getattr__` 和 `__getattribute__` 各在什么时机被调用？后者里为什么不能用 `self.__dict__`？（第 22 节）
11. `__exit__` 返回 `True` 和返回 `None`，对 `with` 块里的异常分别是什么后果？（第 23 节）
12. `@dataclass(slots=True)` 之后，第 14 节里三个代价哪几个立刻生效？给出各自报错原文。（第 13、14 节）

## 本章没演 / 没验证（绝不编造，都在这）

| 想写但没在本机证明 / 没演的事 | 状态 |
| --- | --- |
| `frozen=True` 的字段是 list 时 `append` 能不能改（推测：能，frozen 只挡赋值） | 没演，**不下结论** |
| `dataclass` 其余开关：`init=False`、`repr=False`、`eq=False`、`unsafe_hash`、`kw_only`、`match_args`、`weakref_slot` | 没演。开关名字只在你自己的报错 traceback 里被列出来过（第 10 节那条 `_process_class(...)` 行） |
| `field()` 的 `init=` / `compare=` / `hash=` / `repr=` 逐条 | 没演，本章只用了 `default_factory` |
| `__post_init__`（生成完 `__init__` 的挂钩点） | 没演 |
| dataclass 继承时字段的**顺序**规则与「默认字段不能排在无默认字段前面」 | 没演 |
| `__new__`、`__init__` 与 `__del__` 的分工；对象生命周期末尾的回收 | 没演（`__init__` 只演了返回值约束） |
| 描述符协议本身（手写 `__get__`/`__set__`/`__set_name__`）；第 8 节只证明了 property 是 descriptor | 没演 |
| 元类 `metaclass=`：`abc` 那节只在报错说明里出现「元类」这个词，没有手写元类 | 没演 |
| `ABCMeta.register` / `__subclasshook__`（虚拟子类）、自定义 `__instancecheck__` | 没演 |
| `__slots__` 与 `__weakref__`、`weakref_slot`；`slots` 对继承链的具体布局影响 | 没演 |
| 10 万实例总账在**别的解释器 / 别的版本**上的字节数（PyPy、3.11…） | 未验证，本机只有 3.14.4；两组字节数是这一台机器的实测值 |
| 第 14 节那行「造 10 万个的耗时」 | 每跑必抖，本章只当量级看，**没当证据引用** |
| `sys.getsizeof` 对 `__dict__` 本身的精确大小（脚本用的是 10 万实例总账口径） | 没演 |
| `__exit__` 收到的三元组具体形状、异常链 `__context__` | 没演，**全部留给第 07 章** |
| `contextlib.contextmanager`、`@contextmanager`、可重入 `with` / `ExitStack` | 没演，留给第 07 章 |
| `__setattr__`/`__delattr__` 自定义、`__dir__`、`__slots__` 与 `property` 共存 | 没演 |
| 名字改写在嵌套函数 / 模块级 / 子目录包里的表现 | 没演；本章只演了模块、类体、别的类体三种位置 |
| 多继承时 MRO 算不出来（`TypeError: inconsistent MRO`）长什么样 | 没演 |
| `type()` 三参数造的类能不能被 `dataclass` 装饰、能不能 pickle | 没演 |
| `__call__` 与 `functools.partial`、`operator` 模块的配合 | 没演 |

## 怎么跑这一章

```text
cd python-handbook/06-classes-and-dataclasses
bash demo.sh                # 全跑一遍，25 节，退出码 0，约 15 秒，1023 行输出
bash demo.sh --keep         # 跑完保留沙箱并把目录名打给你，进去自己接着敲
```

想只看某一节：

```text
bash demo.sh 2>&1 | awk '/^══════ 14\./,/^══════ 15\./'
```

`demo.sh` 的行为约定（红线，代码里都写着）：

- 所有副作用只进 `mktemp -d "${TMPDIR:-/tmp}/py06-XXXXXX"` 出来的那个目录；
  `trap cleanup EXIT` 只删自己建的这一个目录，`--keep` 时不删、改成打一行提示。
- **示例源码整份复制进沙箱再运行**（`cp -R` 到 `$SB/work`），所以章目录里不会留下任何产物 ——
  第 25 节用 `find`（`__pycache__`/`*.pyc` → `0`）和 `ls | wc -l`（`21`，捕获那一刻的项数，
  今天再跑是 `22`，多的就是 README.md；见第 25 节末尾的说明）现场自证。
- **不写、不改、不删仓库里任何其他文件**；`python-handbook/README.md` 的章节地图由主 agent 维护。
- 只用标准库：`dataclasses`、`inspect`、`abc`、`collections.abc`、`sys`，不 pip、不联网、不改任何配置。
- 环境变量只在脚本自己和它的子 shell 里生效：`export LC_ALL=C.UTF-8`（让 bash 的报错语言可预测），
  并 `unset PYTHONPATH PYTHONSTARTUP PYTHONOPTIMIZE PYTHONINSPECT PYTHONWARNINGS`，
  避免本机残留影响结论。本章不碰文件名编码。
- **故意不加 `set -e`**：本章一半以上是「注定失败」的现场（`__init__` 返回字符串、unhashable、
  `frozen` 赋值、`property` 无 setter、`dataclass` 可变默认值、`slots` 加属性、`<` 未实现、
  `__getattribute__` 死循环、抽象类实例化…），加了就只能看到第一段。
- 每段前打 `$ 命令`、段末打 `[退出码 N]`，非零也继续跑；最后打 `[全部跑完]` 与 `[沙箱已删除]`。
- 只在 `[ -t 1 ]`（真的接到终端）时上色，所以 `bash demo.sh > out.txt` 拿到的是干净纯文本。
- 复核引文：本章 README 的每条输出都由 `@@R 起-止@@` 占位符**从定稿捕获里机械搬出**，
  所以**捕获文件定稿后不能再重跑 `demo.sh`**（重跑行号会漂，占位符指向的内容就错了）。
  要校验：`python3 ~/.local/bin/expand_fences.py README.md ~/.cache/handbook-captures/py06.txt`
  → `python3 ~/.local/bin/verify_quotes.py README.md ~/.cache/handbook-captures/py06.txt '/tmp/py06-[A-Za-z0-9]{6}'`
  → 期望末行 `未匹配行 0`。

## 术语小抄

| 术语 | 一句话解释（本章的用法） |
| --- | --- |
| 类字典 | `vars(cls)`，类型是 `mappingproxy`（只读门面）。方法、property、类变量全住在这 |
| 实例字典 | `vars(obj)`，`__dict__`。你赋值出来的字段住在这；有 `__slots__` 时**根本没有这个字典** |
| 属性查找链 | 读 `obj.x`：实例字典 → 类字典（含 MRO 上的每个类）→ `__getattr__` 兜底 |
| 遮挡物 (shadow) | 实例字典里和类属性同名的键。它**遮住**类上那份，读的时候看不出来 |
| 绑定方法 | `obj.f` 是「函数 + 实例」这一对；`obj.f(x)` ≡ `A.f(obj, x)`，`self` 无魔法 |
| dunder | 双下划线包围的名字（`__init__`、`__eq__`…），是「运算符/内置函数的协议名」 |
| `__str__` vs `__repr__` | 给人看 vs 给调试看。`print`/`str()`/裸 f-string 走前者；容器、`!r`、`repr()` 只走后者 |
| 哈希约定 | 「`a == b` 必须 `hash(a) == hash(b)`」。写 `__eq__` 时解释器把 `__hash__` 设成 `None` 来保护这条 |
| descriptor（描述符） | 类字典里一个「拦读写」的对象，`property` 和 `__slots__` 的字段都是它 |
| `@dataclass` | 读类型注解，替你生成 `__init__`/`__repr__`/`__eq__`（+ 可选 `<`、`__setattr__`、`__slots__`）塞进类字典 |
| 字段 (field) / `ClassVar` | `fields(cls)` 列出的才是字段；`ClassVar` 标注的是类变量，不进字段清单、不进实例字典 |
| `default_factory` | 「每次构造现造一个」的函数。可变默认值的唯一正解 |
| `frozen=True` | 生成 `__setattr__` 直接抛 `FrozenInstanceError`（`AttributeError` 的子类）；改法用 `dataclasses.replace()` |
| `__slots__` | 类上的名字元组，字段变 `member_descriptor`，实例不再有 `__dict__` |
| MRO | `cls.__mro__`，方法查找的名单（C3 线性化）。`super()` 找的是名单里当前类的**下一名** |
| `super()` | `super(X, obj)` = 「从 `type(obj).__mro__` 里 `X` 的下一名开始找」，与代码写在哪个类无关 |
| `__init_subclass__` | 定义子类那一刻自动执行的钩子（住在类字典里，是个 `classmethod`） |
| ABC / 抽象方法 | `abc.ABC` + `@abstractmethod`：缺实现就拒绝实例化，报 `TypeError` |
| 名字改写 (mangling) | `__priv` 编译期改成 `_类名__priv`，跟着「写代码的类」走；不是访问控制 |
| `__call__` | 让实例能带括号；`f(x)` ≡ `type(f).__call__(f, x)` |
| `__getattr__` / `__getattribute__` | 兜底（查无此人才叫）/ 全量拦截（每次必叫，写错就 `RecursionError`） |
| `__enter__` / `__exit__` | `with` 的两个函数；`__exit__` 返回 `True` = 咽掉异常 |
| `mappingproxy` | `cls.__dict__` 的类型：只读，往里写 `AttributeError`/`TypeError` |

## 本章之后

- **第 07 章**（异常与 `with`）：本章攒了一整章的报错原文 —— `TypeError` 家族
  （unhashable / 抽象类 / `Both already specifies __slots__` / `'<' not supported`）、
  `AttributeError` 家族（`property ... has no setter` / `FrozenInstanceError` / `'P' object has no attribute 'z'`）、
  `ValueError`（可变默认值）、`RecursionError`。它们的继承关系、`try/except/else/finally` 的取舍、
  `__exit__` 的三元组与异常链，都在那一章结清；`contextlib` 也从那里开始。
- **第 08 章**（文件与路径）：`dataclasses.asdict()` 存 JSON、`__repr__` 写进日志的实际场景。
- **第 09 章**（模块与 venv）：`__init__.py`、包的名字空间，和「类字典 / 实例字典」是同一套
  字典查找思路的延续；第 21 节的「有状态调用对象」到那一章会跟闭包、生成器并排比一次代价。
  （迭代器与生成器本身在**第 04 章第 17 节**已经演过：惰性、一次性、`len()` 报错。）
- **装饰器正式讲（已交付）**：本章把 `@property`、`@dataclass`、`@staticmethod`、`@classmethod`、
  `@abstractmethod` 一律当「已知的现成工具」用。它们到底是什么 —— `@X ≡ f = X(f)`、
  在 `def`/`class` 那一刻执行一次、`functools.wraps` 贴回身份证、带参数装饰器是三层函数 ——
  全部在**第 05 章第 23 节**讲完，第 24 节还用一个「重试 + 记账 + memo」装饰器把零件拼成了真工具。
- **数据结构侧的接口**：你在 C 里写的每个 ADT（动态数组、链表、堆、图）到这里都能变成
  `@dataclass(order=..., frozen=...)` + `__eq__`/`__hash__` 的组合；
  下一层要处理的是「节点对象要不要 `__slots__`」—— 本章第 14 节那张 10 万实例的账就是决策依据。

**本章回指**：第 02 章的「赋值是贴名字、不是拷贝」在本章兑现了三次
（第 3 节遮挡物、第 17 节可变类变量、第 20 节 `self.__secret` 的写入）；
第 05 章的「可变默认值只造一次」被 `dataclass` 变成了定义期 `ValueError`（第 10 节）；
第 04 章的「容器打印用元素 `repr`」在第 6、12 节两次现场复现。
