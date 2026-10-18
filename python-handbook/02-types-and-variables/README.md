# 第 02 章：变量是标签不是盒子（类型与变量）

这一章只回答一件事：**`a = 1` 这行代码，在内存层面到底发生了什么，和你在 C 里学的「变量是一块内存」
差在哪里**。差在哪不是重点，重点是每一种差法都用一条命令当场拍下来。

写给正在学 C 和数据结构的人。你脑子里那套「变量 = 一个盒子，赋值 = 把值倒进盒子，
`sizeof` 编译期钉死，溢出会绕回去」全都对 —— 本章第一节就现编现跑把它证实了。
后面二十三节全是它的反面：Python 的名字不装东西，它只是一张贴在对象上的标签。

本章每一条结论都出自本机 **一次真实运行**（`bash demo.sh`，1379 行输出），
跑不出来的东西一律不进正文，写在最后的「本章没演 / 没验证」里。
只用标准库，没 pip，没联网，没改任何配置文件。

> **关于引用输出里的随机后缀**：`demo.sh` 每次跑都 `mktemp -d` 一个随机沙箱，本章这次是
> `/tmp/py02-OQI9dh`。捕获里凡出现这个目录名的地方，你复现时会换成自己的随机后缀，其它字符逐字不变。
> 除此之外本章**不打印任何绝对路径**，所以不会有用户名泄进输出。

## 你会遇到的场景

| 你的困惑 / 你听说的说法 | 本章哪节回答 | 对应 demo 节 |
| --- | --- | --- |
| `a = 1; b = a; a = 2`，`b` 到底是 1 还是 2？和 C 一样吗 | 第 2 节 | 第 2 节 |
| 为什么我改了一个 list，另一个「看起来无关」的 list 也变了 | 第 2、4 节 | 第 2、4 节 |
| 可变 / 不可变到底管到哪一层？tuple 里装 list 还算不可变吗 | 第 4、6 节 | 第 4、6 节 |
| `+=` 对 list 和 str 行为不一样，是不是 bug | 第 5 节 | 第 5 节 |
| `a = b = []` 为什么是同一个对象，`a, b = b, a` 凭什么不用临时变量 | 第 15 节 | 第 15 节 |
| `is` 和 `==` 差在哪？为什么运行时算出的 `256` 是同一个对象、`257` 就不是 | 第 7、8 节 | 第 7、8 节 |
| 为什么两个内容一样的字符串有时 `is` 成立有时不成立 | 第 9 节 | 第 9 节 |
| `1 is True`？`0 == False`？bool 凭什么能参加算术 | 第 10 节 | 第 10 节 |
| `None` 和 `NaN` 谁更「空」；`x != x` 什么时候为真 | 第 11、12 节 | 第 11、12 节 |
| `0.1 + 0.2 == 0.3` 为什么 False，而 `1 == 1.0` 为什么 True | 第 13 节 | 第 13 节 |
| Python 是弱类型，那 `type` 到底挂在名字上还是对象上 | 第 14 节 | 第 14 节 |
| Python 的 int 为什么不溢出？代价是什么？真能无限大吗 | 第 14 节 | 第 14 节 |
| `del x` 是删对象还是删名字？对象什么时候真的死 | 第 16 节 | 第 16 节 |
| 循环引用引用计数数不出来，Python 怎么收 | 第 17 节 | 第 17 节 |
| `UnboundLocalError` 那句话怎么读，为什么加一行赋值就炸 | 第 18 节 | 第 18 节 |
| `global` / `nonlocal` 是干什么的（深入留第 05 章） | 第 19 节 | 第 19 节 |
| 想复制一份，到底写 `copy` / `deepcopy` / 切片 / `list()` | 第 20 节 | 第 20 节 |
| 我照着网上抄的结论，在本机对不对 | 第 21 节 | 第 21 节 |

## 心智模型

**C 的世界（第 1 节现场证实）：变量是盒子，赋值是倒值。**

```text
   C: int a = 1;  int b = a;  a = 2;

   名字 a ──▶ [ 1 ]  ──▶ 变成 ──▶ [ 2 ]      ← 盒子里的内容被覆盖
   名字 b ──▶ [ 1 ]                         ← 倒过来的一份，与 a 无关

   · 盒子的尺寸编译期钉死（sizeof(int) = 4）
   · 想「两个名字指同一块内存」必须自己写指针 int *p = &a;
   · 装不下的数不会自动换个大盒子，它绕回去（INT_MAX + 1 = 最小负数）
```

**Python 的世界（第 2 节起反复证实）：名字不装东西，名字是贴标签。**

```text
   Python: a = 1;  b = a;  a = 2;

   一开始：  a ──┐
                 ├──▶ (对象 int 1)        ← b = a 是把标签贴到同一个对象
            b ──┘

   a = 2 后：a ────▶ (对象 int 2)
            b ────▶ (对象 int 1)          ← 撕标签重贴，对象一个都没动

   可变对象才是咬人的地方：
            xs ──┐
                 ├──▶ [list 1,2]   xs.append(3)  → 对象本体变了，两条标签都看见
            alias┘
            xs ──┐
                 ├──▶ [list 1,2,3]   xs = [9,9]  → 只换 xs 这张标签，alias 留在原地
            alias┘

   · 类型挂在对象上，不挂在名字上（第 14 节）
   · 赋值从不搬值，只改指向：字节码叫 STORE_NAME，不叫 STORE_TO_BOX（第 3 节）
   · 「深拷贝」要明说（第 20 节），「删对象」也轮不到你直接做（第 16 节）
```

## 一句话版本

1. **名字是标签，对象才是东西**：`b = a` 让两张标签贴同一个对象；`b.append()` 改对象（谁都看得见），
   `b = ...` 只换标签（别的标签看不见）。第 2 节四条 `-c` 命令就是这一句。
2. **可变 / 不可变只描述「对象本体能不能被改」，不描述「标签能不能换」**：
   tuple 不可变，但 tuple 里那个 list 照样能长（第 6 节，而且是「报错了副作用还留下」那种狠）。
3. **`==` 比值，`is` 比身份；身份问题在 CPython 里受缓存与编译期优化影响，所以永远别用 `is` 比值**
   （第 7、8 节：边界是自己扫出来的，不是抄来的）。

后面所有节都是这三句话在各种角落的兑现。

## 0. 环境体检：本章结论只在这么一个解释器上验过（demo 第 0 节）

先把解释器钉死。小整数缓存的边界、字符串驻留、`+=` 的原地性，全是**实现细节**，
不是语言规范；换一台机器、换一个版本，本节里打星号的地方都可能变。

```text
$ python3 --version
Python 3.14.4
[退出码 0]

$ python3 -c "import sys; print(sys.version)"
3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]
[退出码 0]

$ python3 -c "import sys; print(sys.prefix, sys.base_prefix)"
/usr /usr
[退出码 0]

$ echo "LC_ALL=$LC_ALL  LANG=$LANG  PYTHONHASHSEED=$PYTHONHASHSEED"
LC_ALL=C.UTF-8  LANG=C.UTF-8  PYTHONHASHSEED=0
[退出码 0]

$ python3 -c "import sys; print(sys.getfilesystemencoding(), sys.stdout.encoding)"
utf-8 utf-8
[退出码 0]
```

`sys.prefix` 和 `base_prefix` 相同，说明不在虚拟环境里，测的就是系统那个 CPython。
`PYTHONHASHSEED=0` 是 demo 自己导的，只为让含 str 的 dict/set 打印顺序可复现；
本章不靠哈希顺序得结论（第 9 节里会看到它没起作用的地方）。

两个数字后面还会反复用到，先摆出来：

```text
$ python3 -c "import sys; print(sys.int_info)"
sys.int_info(bits_per_digit=30, sizeof_digit=4, default_max_str_digits=4300, str_digits_check_threshold=640)
[退出码 0]

$ python3 -c "import sys; print(sys.getsizeof(0), sys.getsizeof(1), sys.getsizeof(2**30))"
28 28 32
[退出码 0]
```

读法：CPython 的 int 不是「一个 64 位格子」，而是 **每格 30 bit、每格 4 字节** 的数组。
`0` 和 `1` 都是 28 字节（对象头 + 一格），跨过 `2**30` 就变成 32 字节（多一格）。
`28` 这个数比 C 的 `sizeof(int)=4` 大 7 倍 —— 这就是「无限精度」的标价，第 14 节继续算。

## 1. 对照组：C 语言里变量真是盒子（demo 第 1 节）

这一节不是为了怀旧，是为了给全章一个**能对照的基准**。gcc 现编现跑：

```text
$ gcc -O0 -Wall -o a_c_box a_c_box.c
[退出码 0]

$ ./a_c_box
一、赋值 = 倒盒子：a = 2, b = 1
二、通过指针改：a = 99（p 和 a 是同一块内存，不是两份值）
三、盒子大小是编译期钉死的：sizeof(int) = 4 字节, INT_MAX = 2147483647
四、INT_MAX + 1 = -2147483648（它没有长成一个更大的整数，而是绕回去了）
[退出码 0]
```

四行输出对应四条 C 直觉，本章要用四条 Python 现场把它们逐条推翻：

| C 直觉（第 1 节证实） | Python 反面（哪节推翻） |
| --- | --- |
| 赋值 = 倒一份值 | 第 2 节：赋值 = 贴标签 |
| 名字绑定一块固定大小的内存 | 第 3 节：名字是 dict 的键 / 帧里的槽 |
| 一个变量只装一种类型 | 第 14 节：类型挂在对象上，名字随便换 |
| 整数装不下就绕回去 | 第 1 节末尾 + 第 14 节：Python 的 int 直接变长 |

最后一条先在这里给个甜头，第 14 节再算账：

```text
$ python3 -c "import sys; print(\"C 的 int 32 位装不下 2**63；Python 的 int 想多大多大：\", 2**63, 2**100)"
C 的 int 32 位装不下 2**63；Python 的 int 想多大多大： 9223372036854775808 1267650600228229401496703205376
[退出码 0]
```

★ **结论（第 1 节）**：C 的模型完全正确，只是它描述的是「名字那一侧」；
Python 把「名字那一侧」退化成了查表，所有物理都挪到了「对象那一侧」。本章就是在给你重贴这套坐标系。

## 2. 场景 1：`a = 1; b = a; a = 2` —— `b` 是什么？（demo 第 2 节）

先看四条一行式命令，它们合起来就是本节全部内容：

```text
$ python3 -c "a = 1; b = a; a = 2; print(b)"
1
[退出码 0]

$ python3 -c "a = 1; b = a; a = 2; print(id(a) == id(b))"
False
[退出码 0]

$ python3 -c "a = [1]; b = a; a.append(2); print(b, id(a) == id(b))"
[1, 2] True
[退出码 0]

$ python3 -c "a = [1]; b = a; a = [1, 2]; print(b, id(a) == id(b))"
[1] False
[退出码 0]
```

逐条读：

- 第 1 条：`b` 是 1。**这一条 C 的直觉答对了，但答对的原因错了** —— 不是因为「倒了一份值」，
  而是因为 `1` 是不可变对象，`a` 换标签时没法带走 `b`。
- 第 2 条：`id(a) == id(b)` 是 False，说明 `a = 2` 之后两张标签贴的**不是同一个对象**了。
- 第 3 条：换 list 场景。`a.append(2)` 之后 `b` 变成了 `[1, 2]`，而且 `id` 还是相等 ——
  **两张标签从头到尾贴着同一个对象，是那个对象自己变了**。C 的直觉在这里彻底失效。
- 第 4 条：`a = [1, 2]` 之后 `b` 还是 `[1]`，`id` 不相等 —— 换标签只影响换的那一张。

★ **结论（第 2 节）**：区分两件事 ——
**改对象**（`append` / `[i]=` / `extend`，所有标签都看得见）和
**换标签**（`= `，只有被换的那张标签受影响）。
你在 C 里只有「改内存」和「改指针」两种动作，Python 的名字一律属于后者。

完整取证在 `b_name_object.py`，它把四种情形和 `id()` 的两个陷阱一起打出来：

```text
$ python3 b_name_object.py
====================================================================
一、a = 1; b = a; a = 2 —— b 到底是什么
====================================================================
贴标签之后：a = 1  b = 1
a 和 b 贴的是同一个对象吗？ True
a = 2 之后：  a = 2  b = 1  ← b 没被 a 带着变
现在 a 和 b 还是同一个对象吗？ False
b 的类型是 int ，值还是 1
```

脚本自己接下来说明了这次「巧合」的性质（下面这段是同一份输出的第 103–105 行，
中间那行空行省略了）：

```text
C 里 int b = a; 是倒一份值，所以 b 永远是 1；
Python 里 b = a 是把标签也贴到同一个对象上，之后 a 换对象跟 b 无关。
两种写法这一次结果一样，但原因不一样 —— 区别在可变对象上会炸开（第 4 节）。
```

然后是第二个 block「可变对象上，「贴同一张标签」和「倒一份值」立刻分道扬镳」的输出：

```text
alias is xs 吗？ True
改 xs 之后 alias = [1, 2, 3, 4]  ← 它不是复制品，它就是同一个东西
换标签之后 alias = [1, 2, 3, 4] ，xs = [9, 9]
现在 alias is xs 吗？ False
```

注意第 2 行和第 3 行的对比：`append` 之后 `alias` 跟着变（`[1,2,3,4]`），
`xs = [9, 9]` 之后 `alias` **还是** `[1,2,3,4]` —— 同名的两行代码，一个动本体一个动标签。

第三个 block 是本章对后面章节最重要的埋伏：函数参数。

```text
  函数里 id(参数) == id(外面那个对象)？ True
  参数指向的对象类型： list
所以函数里改列表，外面看得见 —— 传的是同一个对象（第 05 章可变默认参数那个坑的根）。
```

第四个 block 讲 `id()` 本身怎么读，这个必须讲清楚，否则整章的取证方法会被误用：

```text
del 之后新建一个同类型的 list，地址撞上原来那个了吗？ True
撞上了也别奇怪：id 就是内存地址，地址会被复用。
结论：id() 只用来当**同一时刻**的对照证据，别把它的数值存下来当身份。
顺手记两个尺寸（本机 3.14 / 64 位）：sys.getsizeof([1,2,3]) = 88 ；sys.getsizeof(()) = 48
```

★ **结论（第 2 节续）**：`id()` 是「这一刻它在哪个地址」，不是「它的身份证号」。
对象释放后地址会被回收给新对象（本机第 4 block 真的撞上了）。
所以本章所有 `id` 证据都是 `id(x) == id(y)` 这种**同时刻比较**，从不记录裸数值。

## 3. 场景 1 续：赋值编译成什么，那张名字表在哪（demo 第 3 节）

「标签」不是比喻，它在字节码里就写着。`a = 1` 编译出来只有两条指令：

```text
$ python3 -c "import dis; dis.dis(compile(\"a = 1\", \"<s>\", \"exec\"))"
  0           RESUME                   0

  1           LOAD_SMALL_INT           1
              STORE_NAME               0 (a)
              LOAD_CONST               1 (None)
              RETURN_VALUE
[退出码 0]
```

`LOAD_SMALL_INT 1` 没有创建盒子，它只是把一个**已经存在**的对象压上栈；
`STORE_NAME 0 (a)` 才是赋值本体 —— 在名字表里把键 `a` 指过去。
指令名叫 `STORE_NAME` 而不是 `STORE_TO_BOX`，这个命名不是巧合。

名字表是真的能拿在手里的 dict：

```text
$ python3 -c "x = 5; print(type(globals()).__name__, \"x\" in globals(), globals()[\"x\"])"
dict True 5
[退出码 0]

$ python3 -c "x = 5; print(globals() is vars())"
True
[退出码 0]

$ python3 -c "x = 5; del x; print(\"x\" in globals())"
False
[退出码 0]
```

第三条预告了第 16 节：`del` 删的是这个 dict 里的**键**。

`c_assign_dis.py` 把「名字表 = 模块 `__dict__`」钉死，并且演示了字符串当名字这种 C 做不到的事：

```text
$ python3 c_assign_dis.py
====================================================================
一、`a = 1` 编译出来就两条指令
====================================================================
源码： a = 1
  0           RESUME                   0

  1           LOAD_SMALL_INT           1
              STORE_NAME               0 (a)
              LOAD_CONST               1 (None)
              RETURN_VALUE
读法： LOAD_SMALL_INT 1  →  把已经存在的对象 1 压上栈（没有造盒子）
      STORE_NAME 0 (a)  →  在名字表里把名字 a 指向那个对象
注意指令名叫 STORE_NAME 而不是 STORE_TO_BOX：赋值从来不搬值，只改指向。
```

第二个 block（同份输出，`====================================================================` 那行分隔符照抄）：

```text
globals() 的类型： dict
名字表里有 answer 这个键吗？ True
globals()['answer'] 和直接写 answer 是同一个对象吗？ True
globals() 和 vars() 在模块顶层是同一个 dict 吗？ True
模块对象自己的 __dict__ 就是 globals() 吗？ True
所以「给变量赋值」= 「往 dict 里加一个键」，这就是为什么能用字符串当名字：
  globals()['字符串当名字'] = 7
  删掉键之后 '字符串当名字' in globals() → False
→ C 的变量名编译完就没了；Python 模块层的名字活到进程结束，还就是一个 dict 的键。
  代价：每次读全局名都要做一次哈希查找（01 章场景 2 埋的那句话，在这里兑现）。
```

★ 这里有个能直接拿来用的判据：**模块层的变量 = 该模块 `__dict__` 的键**。
所以 `getattr(mod, name)` / `setattr(mod, name, v)` / `mod.__dict__[name]` 三种写法是同一件事，
也所以「运行时给模块塞一个全局变量」在 Python 里合法、在 C 里没有对应物。

函数里的名字不查这个 dict，脚本打出来的还是 `-c` 那段 `def f(): a = 1` 的字节码，
只是文件名换成了 `<演示>`，其中的 `<code object f at 0x77cbfed72090, ...>` 地址每次进程都不同 ——
**本章刻意不把带 `0x…` 的行放进带 `$` 的围栏**，你复现时这一行的十六进制一定和这里不一样，其余逐字相同。

同一份输出里可复现的那几行是函数体：`STORE_FAST 0 (a)` / `LOAD_FAST_BORROW 0 (a)`，
和上面 `-c` 那条一模一样，所以这里不再重复引用，只留脚本自己的读法（原文照抄）：

```text
读法：局部名字是 STORE_FAST/LOAD_FAST —— 存在帧里的固定槽位，按索引取，不查 dict。
所以同一个名字 a，在模块层是 dict 的键，在函数里是栈槽；
这个区别就是第 18 节 UnboundLocalError 的全部原因（编译期就定了它归哪一层）。
```

`dis` 顶层命令里也能看到同样的结构（demo 第 3 节的 `-c`）：

```text
$ python3 -c "import dis; dis.dis(compile(\"def f():\n    a = 1\n    return a\", \"<s>\", \"exec\"))"
  0           RESUME                   0

  1           LOAD_CONST               0 (<code object f at 0x79fc7698e090, file "<s>", line 1>)
              MAKE_FUNCTION
              STORE_NAME               0 (f)
              LOAD_CONST               1 (None)
              RETURN_VALUE
```

（同上：`0x79fc7698e090` 是那次运行的地址，不可复现。这条命令后面紧跟的
`Disassembly of <code object f ...>` 段里就是 `STORE_FAST 0 (a)` 和 `LOAD_FAST_BORROW 0 (a)`
两行 —— 它们同样带着不可复现的 `Disassembly of <code object … at 0x…>` 抬头行，所以本章只引用脚本
那份输出里的读法，不重复贴这两行。）

`c_assign_dis.py` 最后一个 block 把边界摆到类体上（细节留给第 06 章）：

```text
方法里 'base' 在 locals 吗 / 在 globals 吗： (False, False)
（类体自己是一层名字空间，但那层不会传给方法 —— 方法里必须写 self.base。
  这已经是第 06 章的内容了，这里只把边界摆出来。）
```

★ **结论（第 3 节）**：Python 有**两套**名字机制 —— 模块层查 dict（慢一点，但可以被字符串、
被运行时改写），函数层用栈槽（快，但编译期就固定了）。第 18 节那个「离奇」的
`UnboundLocalError`，根子在「编译期就定了它归哪一层」。

## 4. 场景 2：可变 vs 不可变（demo 第 4 节）

先把「不可变」的官方说法用报错原文钉住：

```text
$ python3 -c "xs = [1, 2]; ys = xs; xs.append(3); print(ys)"
[1, 2, 3]
[退出码 0]

$ python3 -c "xs = [1, 2]; ys = xs; ys[0] = 9; print(xs)"
[9, 2]
[退出码 0]

$ python3 -c "s = \"abc\"; print(s.upper() == s, id(s.upper()) == id(s))"
False False
[退出码 0]

$ python3 -c "s = \"abc\"; s[0] = \"X\""
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    s = "abc"; s[0] = "X"
               ~^^^
TypeError: 'str' object does not support item assignment
[退出码 1]
```

第三条值得停一下：`s.upper() == s` 是 False（内容不一样），
`id(s.upper()) == id(s)` 也是 False（连对象都不是同一个）。
str 的每个方法都**造新对象再返回**，这不是低效，是「不可变」的定义本身。
第四条是「不可变」在解释器里的原话，报错原文值得逐字读：
`'str' object does not support item assignment` —— 它说的是「不支持**下标赋值**」，
不是「str 不能变」。这个区别在第 6 节会咬人。

`d_mutable.py` 的 int/str 部分补了一个容易踩空的细节：

```text
$ python3 d_mutable.py
====================================================================
一、list：原地改，对象没换
====================================================================
alias 看到的： [99, 2, 3, 4]
xs 看到的：   [99, 2, 3, 4]
alias 和 xs 还是同一个对象吗？ True
→ 这就是 Python 里最容易被咬的一步：改一个，另一个跟着变。
```

（中间隔着一个「二、int / str：想「原地改」根本不存在」的分隔，同份输出接着打：）

```text
n = 11 ；n + 1 生成的新对象和旧对象是同一个吗？ False
（其实 10 和 11 都在小整数缓存里，这里比的必须是变量：见第 7 节）
字符串拼接后，标签指向的还是原来那个 str 吗？ False
直接按下标改字符串，报错原文： 'str' object does not support item assignment
→ str 的每个方法都返回新对象，这是「不可变」的代价与好处：
  好处是它可以当 dict 的键、可以被安全共享；代价是循环里拼字符串很贵（10 章实测）。
```

★ **结论（第 4 节）**：**可变 / 不可变是对象的属性，不是变量的属性。**
不可变对象能当 dict 键、能安全共享、能做哈希缓存的前提；
可变对象带来「一处改全局变」的便利，代价是本章剩下的所有坑。
第 2 行那句括号很重要：证明「n+1 是新对象」时**必须用变量**，
因为 `10 is 10` 这种字面量比较会被小整数缓存成全 —— 第 7 节专讲这件事。

## 5. 场景 3：`+=` 对 list 和 str 是两种命运（demo 第 5 节）

```text
$ python3 -c "xs = [1, 2]; before = id(xs); xs += [3]; print(id(xs) == before)"
True
[退出码 0]

$ python3 -c "s = \"ab\"; before = id(s); s += \"c\"; print(id(s) == before, s)"
False abc
[退出码 0]

$ python3 -c "t = (1, 2); before = id(t); t += (3,); print(id(t) == before, t)"
False (1, 2, 3)
[退出码 0]
```

同一个 `+=` 号，三种命运：list 原地（`id` 没变）、str 换对象、tuple 换对象。
字节码层面它们**写的是同一条指令**，所以差别一定在运行时：

```text
$ python3 -c "import dis; dis.dis(compile(\"xs += [3]\", \"<s>\", \"exec\"))"
  0           RESUME                   0

  1           LOAD_NAME                0 (xs)
              LOAD_SMALL_INT           3
              BUILD_LIST               1
              BINARY_OP               13 (+=)
              STORE_NAME               0 (xs)
              LOAD_CONST               1 (None)
              RETURN_VALUE
[退出码 0]
```

读法：`BINARY_OP 13 (+=)` 之后紧跟 `STORE_NAME 0 (xs)` —— **注意它一定会重新贴标签**。
所以「`+=` 是不是原地」这件事根本不由语法决定，而由这个对象有没有 `__iadd__` 决定：
有，`BINARY_OP` 就调它并（可能）返回自己；没有，就退化成 `x = x + y` 造新对象。
（这里 `BINARY_OP 13 (+=)` 的编号是本机 3.14 打出来的，别照抄网上写法的编号。）

`e_iadd.py` 把协议讲透，还用自定义类当探针：

```text
$ python3 e_iadd.py
====================================================================
一、+= 对 list 和 str 是两种命运（用 __iadd__ 解释）
====================================================================
list 走 += 之后对象换了吗？ False
  → 没换：list.__iadd__ 原地吸收，返回的还是自己
str 走 += 之后对象换了吗？ True
  → 换了：str 没有真正的原地加法，只能造一个新 str 再贴标签
tuple 走 += 之后对象换了吗？ True
  → 也换了：tuple 不可变，+= 等价于 tpl = tpl + (3,)
```

（同份输出接着的两段）

```text
两个写法看着一样，语义完全不同：
  lst += [3]   等价于 lst.__iadd__([3])，改的是 lst 指向的那个对象本体
  txt += 'c'   等价于 txt = txt + 'c'，造新对象并把标签撕过来
所以：多个标签贴着同一个 list 时，+= 会影响所有标签；贴着 str 时不会。

自己写一个类来验证这个协议（谁实现了 __iadd__，+= 才可能原地）：
InPlace：+= 之后对象换了吗？ False ；第二个标签看到的 hits = 8
NewObject：+= 之后对象换了吗？ True ；r.tag = ab
```

★ **结论（第 5 节）**：`+=` 是**语法糖**，不是**语义**。想知道它对某个类型做什么，
问三个问题：这个类实现了 `__iadd__` 吗？它返回的是 `self` 还是新对象？
自定义类时你可以自己选（`InPlace` 返回 self 所以 `hits` 被第二个标签看见；
`NewObject` 返回新对象所以只有 `r.tag` 变了）。这也是第 21 节「一页纸取证」里的常客。

## 6. 场景 4：tuple 里装 list —— 不可变管到哪一层（demo 第 6 节）

```text
$ python3 -c "t = (1, [10]); inner = id(t[1]); t[1].append(20); print(t, id(t[1]) == inner)"
(1, [10, 20]) True
[退出码 0]

$ python3 -c "t = (1, [10]); t[1] += [20]; print(t)"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    t = (1, [10]); t[1] += [20]; print(t)
                   ~^^^
TypeError: 'tuple' object does not support item assignment
[退出码 1]
```

第二条是**本章最容易被误读的一行输出**，所以值得完整读一遍：

- 退出码 1，`print` 根本没跑（`t` 到底变成什么了，这条命令没回答）。
- 报错是 `TypeError` 而不是 `ValueError`，指的位置是 `t[1] += ` 的 `^^^`（下标赋值这一步）。
- 但 `+=` 对 list 那一半**已经执行成功了**（第 5 节刚证实它是原地吸收）。

`f_tuple_inner.py` 把这条命令回答不了的问题补齐：

```text
$ python3 f_tuple_inner.py
====================================================================
一、tuple 里装 list：「不可变」到底管到哪一层
====================================================================
往 tuple 里那个 list 追加元素之后： (1, [10, 20, 30])
tuple 这个对象本身换了吗？ False
里面的 list 还是原来那个对象吗？ True
→ tuple 的「不可变」只管**里面每一格指向哪个对象**，不管那个对象自己变不变。
  这叫 shallow immutability（浅层不可变）。所以 tuple 不能当「深」的常量用。
```

（同份输出接着的 hash 对照，这段顺便把 dict 键的规则一起证了）

```text
顺带一个能当场把 hash 规则证出来的对照：
hash((1, 2)) 能不能算？ 能
hash((1, [2])) 能不能算？ 不能： unhashable type: 'list'
→ 因为 dict 的键/set 的元素必须永远不变，可变的 list 装进来就破坏哈希表，
  所以解释器直接拒绝给含 list 的 tuple 算哈希。

最后一行是本节最狠的一个现场：
执行 t2[1] += [30] 之前： (1, [10, 20])
TypeError 报错原文： 'tuple' object does not support item assignment
执行完（虽然是失败的一行）之后： (1, [10, 20, 30])
→ 报了错，但 list 真的被改了：+= 先原地吸收（成功），再往 tuple 的第 1 格贴标签（被禁止）。
  半路失败、副作用留下。这一条是本章「不可变」最常被误解的地方。
```

`-c` 那两条命令和脚本里这一段是同一个现场的两种问法：报错的那一行是**原子操作**吗？
答案不是 —— Python 的语句级原子性根本不存在，`t[1] += [20]` 是「算 → 原地改 → 存回下标」三步，
第三步被禁止，前两步已经落地。

★ **结论（第 6 节）**：
1. **不可变是浅层的**：tuple/frozenset 只保证「每格指向谁」不变，不保证里面那层的对象不变。
   想要真常量，得自己保证内层也可变对象不存在（或用 `deepcopy` 之后再冻结，第 20 节）。
2. **hash 规则和不可变是绑在一起的**：`hash((1, [2]))` 直接 `TypeError: unhashable type: 'list'`，
   因为可变对象进哈希表会让表失效。
3. **失败的语句可以留下副作用**。写 `try` 包住一行「应该没副作用」的代码时，这条尤其重要。

## 7. 场景 5：小整数缓存，边界自己扫出来（demo 第 7 节）

网上流传的「Python 缓存 -5 到 256」这句话，本章不接受抄来的数字，自己扫。
但**先要避开一个无效实验**：

```text
$ python3 -c "n = 100; print(n + 0 is n + 0)"
True
[退出码 0]

$ python3 -c "n = 300; print(n + 0 is n + 0)"
False
[退出码 0]

$ python3 -c "n = 300; print(n + 0 == n + 0)"
True
[退出码 0]

$ python3 -c "n = -6; print(n + 0 is n + 0, n + 0 == n + 0)"
False True
```

（最后一条的 `[退出码 0]` 与紧跟的 demo 提示行在原输出里连在一起，这里把提示行放到正文：）

> demo 里这一节的提示原文是「必须用运行时算出来的整数：把 300 直接写成字面量，编译器就折成同一个常量了（第 8 节）」。

为什么非要 `n + 0`？因为 `300 is 300` 这种写法里，编译器会把同一份代码内的两个相同字面量
**合并成常量表里的同一个常量**，`is` 当然成立，但**什么都没测到**。
`+ 0` 强制它走一次运行时算术，才会碰到「这个数从缓存发还是新建」。

`g_small_ints.py` 用 `mk(n) = n + 0` 当探针，在 `-300 .. 400` 上扫边界：

```text
$ python3 g_small_ints.py
====================================================================
一、直接比字面量是无效实验（3.14 还会骂你）
====================================================================
demo 第 8 节里能看到这条警告原文：
    SyntaxWarning: "is" with 'int' literal. Did you mean "=="?
本机实测：写成字面量的 `256 is 256` 和 `257 is 257` 都是 True —— 两个字面量被折成同一个常量。
→ 网上那句「257 is 257 是 False」在这台 3.14 上直接复现不了，必须换成运行时值。
```

★ 这一条本身就是一个值得记住的结论：**「257 is 257 是 False」这句广为流传的话，
在本机 3.14 上复现不了**。网上那些例子多半写成字面量或写法过时。

扫描结果（本机硬证据）：

```text
扫描区间： -300 .. 400
命中缓存的区间： -5 .. 256   共 262 个
这段区间是不是连续不断？ True
上边界那一个： 256 → mk(n) is mk(n) = True
上边界再往上一个： 257 → False
下边界那一个： -5 → True
下边界再往下一个： -6 → False
→ 本机边界实测结论：闭区间 -5 到 256
```

`262` 这个数是交叉验证：从 -5 数到 256（含两端）正好 262 个，和「连续不断 True」一起说明
扫出来的不是一堆散点，是一个区间。

第三、四个 block 把「这是实现细节」钉死：

```text
7 和 70-63（都过运行时）是同一个对象吗？ True
2**31（缓存外）两次运算结果同一个对象吗？ False ；但相等： True
→ 缓存外 == 成立、is 不成立。所以「用 is 判数值」永远是错的：
  它在小整数区间里刚好对，出了区间就翻脸 —— 这种「时对时错」最难查。
```

（分隔线之后）

```text
sys.int_info = sys.int_info(bits_per_digit=30, sizeof_digit=4, default_max_str_digits=4300, str_digits_check_threshold=640)
语言参考里没有任何一处保证整数缓存的区间大小；换解释器（PyPy）或换版本都可能不同。
顺带看看别的内置单例走没走同一条路：
  bool(1) is True ： True
  None is None    ： True
  sys.intern(那个字符串) is 同一个变量 ： True
```

★ **结论（第 7 节）**：本机 CPython 3.14.4 的小整数缓存是**闭区间 -5..256，共 262 个**。
这个数字**只在这台机器这一次运行里成立**，所以正确用法是：把它当「为什么 `is` 会时灵时不灵」的证据，
不当规则。规则只有一条 —— 判数值用 `==`。

## 8. 场景 6：`is` vs `==` —— 3.14 直接警告你（demo 第 8 节）

第 7 节说「字面量比 `is` 是无效实验」，3.14 的解释器自己就是这么认为的，而且会骂人：

```text
$ python3 -c "print(256 is 256, 257 is 257)"
<string>:1: SyntaxWarning: "is" with 'int' literal. Did you mean "=="?
<string>:1: SyntaxWarning: "is" with 'int' literal. Did you mean "=="?
True True
[退出码 0]

$ python3 -c "print(\"abc\" is \"abc\", \"a b\" is \"a b\")"
<string>:1: SyntaxWarning: "is" with 'str' literal. Did you mean "=="?
<string>:1: SyntaxWarning: "is" with 'str' literal. Did you mean "=="?
True True
[退出码 0]

$ python3 -W error -c "print(256 is 256)"
  File "<string>", line 1
SyntaxError: "is" with 'int' literal. Did you mean "=="?
[退出码 1]
```

三条读法：

1. 两个警告是**一条一个字面量对**打的（同一行有两个 `is`，所以两行警告）。
2. 输出依然是 `True True` —— 警告不改行为，只标你写法。第 7 节那个「网上结论复现不了」在这里再次现身。
3. `-W error` 把 `SyntaxWarning` 升级成 `SyntaxError`，退出码 1，**代码根本不会跑**。
   所以「`is` 用错」在严格 CI 配置下是构建失败，不是风格问题。

真正的判据（`==` 可以自反，`is` 也自反，但两者问的问题不同）：

```text
$ python3 -c "import sys; xs = [1]; print(id(xs) == id(xs), xs == xs)"
True True
[退出码 0]

$ python3 -c "print(1 == True, 1 is True, None is None)"
<string>:1: SyntaxWarning: "is" with 'int' literal. Did you mean "=="?
True False True
[退出码 0]
```

第二条命令同时给了三个答案和一个警告，非常划算：`1 == True` 是 True（值相等，第 10 节解释），
`1 is True` 是 False（不是同一个对象），`None is None` 是 True（单例）。
警告针对的正是其中的 `1 is True` —— 解释器认为你对 int 字面量用了 `is`。

★ **结论（第 8 节）**：`is` 的合法用法只有三类 ——
**比 `None`**、**比单例**（`Ellipsis`、模块自己、哨兵对象）、**问「是不是同一个东西」**（缓存是否命中、
回调是否同一个函数对象、修改是否真的落在同一个容器上）。其它场合写 `==`。
这条是 01 章索引里点名的那个场景（「`is` vs `==`」），本章在这里把它结掉。

## 9. 场景 7：字符串驻留 —— 哪些字符串会被共用（demo 第 9 节）

```text
$ python3 -c "a = \"foo_bar\"; b = \"foo\" + \"_bar\"; print(a == b, id(a) == id(b))"
True True
[退出码 0]

$ python3 -c "import sys; parts = [\"foo\", \"_bar\"]; a = \"\".join(parts); b = \"foo_bar\"; print(sys.intern(a) is b)"
True
[退出码 0]

$ python3 -c "import sys; print(sys.intern(\"a b\") is sys.intern(\"a b\"))"
True
[退出码 0]
```

第一条又是个陷阱秀：`"foo" + "_bar"` 是**编译期折常量**，所以和字面量同一个对象；
如果拼接发生在运行时（比如 `"".join(parts)`），默认就**不是**同一个对象，要靠 `sys.intern`。
所以「字符串到底共不共用」有三个不同机制掺在一起，`h_intern.py` 把它们分开测。

第一个 block 是关键：每一行的两个字符串**来自两段互相独立的 `compile()`**，
这样才排除掉「常量表去重」的干扰：

```text
$ python3 h_intern.py
====================================================================
一、哪些字面量被自动驻留（每行都是两段独立编译的代码产物）
====================================================================
'foo_bar'    两段独立代码给出同一个对象？ True
'abc_def'    两段独立代码给出同一个对象？ True
'1abc'       两段独立代码给出同一个对象？ True
'_x'         两段独立代码给出同一个对象？ True
'foo bar'    两段独立代码给出同一个对象？ False
'Foo.Bar'    两段独立代码给出同一个对象？ False
'café_ok'    两段独立代码给出同一个对象？ False
'a b'        两段独立代码给出同一个对象？ False
```

（同份输出的规则总结）

```text
本机测出来的规则：全是 ASCII 字母 / 数字 / 下划线 → 驻留；
带空格、带点、含非 ASCII 字母 → 不驻留。
（空串 "" 也恒 True，但那是因为它自己是个单例，不能当驻留规则的证据。）
```

对照 C 的直觉：编译器按「像不像标识符」决定要不要把字符串存进 intern 表，
因为它主要为了给**属性名/键**的 dict 查找提速，不是为了帮你省内存。

第二到第五个 block 依次是运行时拼接、常量去重、属性名、用处：

```text
运行时拼出来的和字面量同一个对象吗？ False
两个值相等吗？ True
sys.intern 之后同一个对象了吗？ True
sys.intern 是幂等的：再驻留一次还是它自己吗？ True
```

（「三、同一份代码里两个相同字面量：那是常量表去重，不是驻留」）

```text
同一份代码里两个 'a b' 值相等吗： True
同一份代码里两个 'a b' 是同一个对象吗： True
两段独立代码里的 'a b' 是同一个对象吗： False
→ 差别就在这：常量表去重只在一份代码内有效；跨代码还共用的，才是被驻留过的。
```

这一段值得单独划线：**同一个 False 和 True 的差，就是「去重」和「驻留」的分界线**。
很多人（包括各种文章）把这两件事混为一谈，于是得出「Python 字符串全都共用」的错误结论。

```text
h.__dict__ 里那个键： 'foo_bar'
它和字面量 'foo_bar' 是同一个对象吗？ True
→ 别在业务代码里靠这个：它是 CPython 的实现细节，不是语言保证。
```

（最后一个 block）

```text
同一个 list 里三个 'ok' 是同一个对象吗？ True
读文件 / input() / 算出来的字符串一律不驻留，所以：
  判内容一律写 ==；is 只留给 None 和「就要问是不是同一个东西」的场合。
```

★ **结论（第 9 节）**：字符串共用是**三件事的叠加** ——
编译期常量折叠、常量表去重（一份代码内）、自动驻留（像标识符的 ASCII 串，跨代码）。
你测出来的 `is True` 属于哪一件，取决于实验怎么搭。实践结论和第 8 节一样：**判内容写 `==`**，
`sys.intern` 只在「拿身份换速度」的场合用（大量重复键），它不改变任何语义。

## 10. 场景 8：`bool` 是 `int` 的儿子（demo 第 10 节）

```text
$ python3 -c "print(issubclass(bool, int), isinstance(True, int), type(True) is int)"
True True False
[退出码 0]

$ python3 -c "print(True + 1, sum([True, False, True]), True == 1)"
2 2 True
[退出码 0]

$ python3 -c "one_is = 1; print(one_is is True, one_is == True)"
False True
[退出码 0]

$ python3 -c "d = {1: \"甲\", True: \"乙\"}; print(d, len(d))"
{1: '乙'} 1
[退出码 0]

$ python3 -c "d = {1: \"甲\", 1.0: \"乙\"}; print(d, len(d))"
{1: '乙'} 1
[退出码 0]
```

第一条命令是整节的核心三件套，三个答案刚好排成一条线：
`issubclass(bool, int)` True（继承关系）、`isinstance(True, int)` True（`True` 是个 int），
`type(True) is int` **False**（`type()` 拿到的是 `bool`）。
第三条正是第 14 节要讲的「`isinstance` vs `type() is`」的预演。

第四、第五条是同一个陷阱的两个皮：`{1: '甲', True: '乙'}` 打印出来是 `{1: '乙'}`，长度 1 ——
`1` 和 `True` 在 dict 眼里是**同一个键**（`hash` 相同、`==` 相同），后者把前者的值盖掉了；
`1.0` 也一样。这跟 C 的「整数就是整数」完全不同：Python 的 dict 判键用的是 `==`。

`i_bool.py` 打全了继承链，注意它开头的警告（这一行带沙箱路径，你复现时后缀会换）：

```text
$ python3 i_bool.py
/tmp/py02-OQI9dh/work/i_bool.py:22: SyntaxWarning: "is" with 'int' literal. Did you mean "=="?
  print("True == 1 =", True == 1, "；1 is True =", 1 is True, "← 值相等，但不是同一个对象")
====================================================================
一、bool 是 int 的儿子
====================================================================
issubclass(bool, int) = True
bool.__mro__ = bool -> int -> object
type(True) = bool ；isinstance(True, int) = True
type(True) is int 吗？ False ← 类型判断用 type() is 就把 bool 排除了
True + 1 = 2 ；False * 10 = 0 ；sum([True,True,False]) = 2
True == 1 = True ；1 is True = False ← 值相等，但不是同一个对象
True.bit_length() = 1 ← bool 直接能用 int 的方法，说明它真是 int
int 有没有 bool 的方法？ hasattr(int, 'bit_length') = True
```

（同份输出第二个 block，讲「被算术吞掉」）

```text
len([t for t in flags if t]) = 2
sum(flags) = 2 （当计数用很方便，但类型上它是 int 不是 bool）
把 2 转成 bool：bool(2) = True ；2 == True = False
字典里 1 和 True 是同一个键： 1 个键
字典里 1 和 1.0 也算同一个键： 1 个键
→ 因为键的判据是 ==，而 1 == True == 1.0；后面的赋值把前面的值盖掉了。
```

脚本自己在第 22 行用 `1 is True` 触发了警告，这是**故意**的 —— 它要用现场警告原文说明
「连写这种探针都会被骂」，输出前两行就是证据。

★ **结论（第 10 节）**：`bool` 是 `int` 的子类，`True` 就是 `1` 的另一种写法，所以它能算术、
会被 `sum` 吞、会在 dict 里和 `1` 撞键。
`0 == False` 成立、`1 is True` 不成立。
写「是不是布尔」时用 `isinstance(x, bool)`（能认出子类），
写「是不是整数且只可能是 bool」时 `type(x) is bool`；两者不等价，见第 14 节。

## 11. 场景 9：`None` 是单例，而且「空」不等于 `None`（demo 第 11 节）

```text
$ python3 -c "print(None is None, type(None).__name__, bool(None))"
True NoneType False
[退出码 0]

$ python3 -c "print(None == 0, None == False, None == \"\")"
False False False
[退出码 0]

$ python3 -c "import copy; print(copy.copy(None) is None)"
True
[退出码 0]
```

第二条命令是这节的核心：`None` 和一切「空」都**不相等**。这是和 JS 的 `==` 最大的分野，
也是 C 里没有的东西（C 的 `NULL` 就是个 0，`NULL == 0` 成立）。
第三条更狠：`copy.copy(None)` 返回的还是 `None` 自己 —— 单例不参与拷贝语义。

还有一条 demo 用的多行 `-c`（它本身就是完整命令，`$` 围栏里逐字照抄，包括换行）：

```text
$ python3 -c "
def forget():
    pass
print(repr(forget()), forget() is None)
"
None True
[退出码 0]
```

`j_none.py` 把「只有一个」量化了：

```text
$ python3 j_none.py
====================================================================
一、None：整个进程里只有一个
====================================================================
type(None) = NoneType
None is None = True
两个 None 变量同一个对象吗？ True
bool(None) = False ；None == 0 = False ；None == False = False
copy.copy(None) is None = True
copy.deepcopy(None) is None = True
sys.getsizeof(None) = 16 字节（它不携带任何数据，本身就是个记号）
```

然后是那张「假值 vs None」对照表，它决定了你以后怎么写参数默认值：

```text
None 和「空」是两件事，判空必须分开写：
  []      为假值 True   is None False
  ''      为假值 True   is None False
  {}      为假值 True   is None False
  0       为假值 True   is None False
  False   为假值 True   is None False
  None    为假值 True   is None True
→ `if not x:` 会把 0 / '' / [] / None 一视同仁；要区分「没传」和「传了空」必须写 is None。
```

第二个 block 补了最常见的事故现场（忘记 `return`）：

```text
ret = None ；type(ret) = NoneType
报错原文： 'NoneType' object has no attribute 'bit_length'
```

★ **结论（第 11 节）**：`None` 是「什么都没有」这个**事实**的唯一记号，16 字节，不携带数据，
全进程一个，`copy`/`deepcopy` 都返回它自己。
它和「空」的关系只是「两者都是假值」。
所以判 `None` 只能写 `is None`（第 8 节允许的三类 `is` 之首），
判「空不空」写 `if not x`，**永远不要把 `== None` 当判空**。

## 12. 场景 10：NaN 跟谁都不相等（包括它自己）（demo 第 12 节）

```text
$ python3 -c "x = float(\"nan\"); print(x == x, x != x, id(x) == id(x))"
False True True
[退出码 0]

$ python3 -c "import math; print(math.isnan(float(\"nan\")), math.isnan(0.0))"
True False
[退出码 0]

$ python3 -c "x = float(\"nan\"); print([x].count(x), x in [x])"
1 True
[退出码 0]

$ python3 -c "print(float(\"nan\") == float(\"nan\"))"
False
[退出码 0]
```

第一条是本章最反直觉的一行输出：同一个变量 `x`，`x == x` 是 **False**，
`x != x` 是 **True**，但 `id(x) == id(x)` 是 **True**。
三个答案放在一起才说得通：`==` 走的是 IEEE 754 的数值比较规则（NaN 与任何值比较都是「无序」），
`is` 走的是地址比较（同一个对象当然相等）。**`==` 比的是数值规则，`is` 比的是身份，两者不必一致。**

第三条又翻一层：`[x].count(x)` 是 1、`x in [x]` 是 True —— 明明 `x == x` 是 False，
为什么容器找到了？因为 CPython 的容器查找先做**同一性短路**（`is` 成立就直接算命中），
根本轮不到 `==`。这条差异在 `k_nan.py` 里被打得更细。

```text
$ python3 k_nan.py
====================================================================
一、NaN：按 IEEE 754，它跟谁都不相等，包括它自己
====================================================================
nan == nan  → False
nan != nan  → True ← 唯一满足 x != x 的值
nan is nan  → True ← 但对象还是同一个：is 比地址，== 比数值规则
math.isnan(nan) → True
两个不同的 NaN 对象也互不相等： False
```

（同份输出接着的坑说明）

```text
坑：`if x != x:` 判 NaN 能用，但 list 里找 NaN 走的是 is 短路 —— 
  [nan].count(nan) = 1 ；nan in [nan] = True
→ 同一性短路让容器操作「看起来正常」，排序/聚合时 NaN 会一路传染。
  sorted 遇 NaN 不报错但顺序未定义： [3, nan, 1]
```

（注意上面第一行行尾在捕获里是带一个空格的，照抄。）

第二个 block 把 float 相等判定的另外几个边界一次列全，这一段其实提前把第 13 节的「数值塔」引出来了：

```text
inf == inf → True ；inf > 10**1000 → True
-0.0 == 0.0 → True ；但符号还在： -1.0
于是 dict 里 -0.0 和 0.0 是同一个键： 1
hash(1) == hash(1.0) → True ← 相等的数必须同哈希，这是哈希表的合同
hash(True) == hash(1) → True
→ 这套「跨类型相等 + 同哈希」是 Python 数值塔的规则，
  C 里 int/float 比较靠隐式转换，2^53+1 那种撞车是无声无息的。
```

`inf > 10**1000` 是 True 这件事值得一提：`10**1000` 是个几百位的 int，Python 拿它跟 `inf` 比
**没有把它先转成 float**（那会溢出），而是按精确语义判定。这是下一节的正题。
`-0.0 == 0.0` 但符号还在（`1/-0.0 = -1.0`），并且它们在 dict 里是同一个键 —— 第 10 节
`1`/`True` 撞键的同一套规则。

★ **结论（第 12 节）**：`==` 不是「内容一样」的同义词，它是「调用类型的相等规则」。
float 的规则由 IEEE 754 定，于是产生 `nan != nan` 这个唯一能自反失败的普通值。
判 NaN 用 `math.isnan()`，别用 `x != x`（能用但只有你能看懂）；
更要记住的是容器走 `is` 短路，所以「NaN 在 list 里找得到、但在比较里永远不相等」这种
自相矛盾的观感是**两个机制叠加**的结果，不是 bug。

## 13. 场景 11：`0.1 + 0.2 != 0.3`，以及 `1 == 1.0` 为什么成立（demo 第 13 节）

```text
$ python3 -c "print(0.1 + 0.2 == 0.3)"
False
[退出码 0]

$ python3 -c "print(format(0.1 + 0.2, \".20f\"))"
0.30000000000000004441
[退出码 0]

$ python3 -c "import math; print(math.isclose(0.1 + 0.2, 0.3, rel_tol=1e-09))"
True
[退出码 0]

$ python3 -c "import decimal; print(decimal.Decimal(\"0.1\") + decimal.Decimal(\"0.2\") == decimal.Decimal(\"0.3\"))"
True
[退出码 0]

$ python3 -c "print(1 == 1.0, id(1) == id(1.0))"
True False
[退出码 0]

$ python3 -c "print(float(2**53 + 1) == float(2**53), 2**53 + 1 == float(2**53))"
True False
```

前四条是一个完整的处理链：现象（False）→ 原因（`0.30000000000000004441`）→
工程解法一（容差 `math.isclose`）→ 工程解法二（十进制精确 `decimal`）。
第 5 条把「值相等」和「同一对象」彻底分开：`1 == 1.0` True，`id` 不同 False。

第 6 条是**本章和 C 最实在的一条分工**（demo 里紧跟的提示行原文是
「最后两条的差别是本章和 C 最实在的分工：先转 float 会撞车，int 直接比不撞。」）：

- `float(2**53 + 1) == float(2**53)` → **True**：`2**53+1` 转成 float 时被舍入成 `2**53`，两边撞车。
- `2**53 + 1 == float(2**53)` → **False**：int 和 float 直接比时**不做有损转换**，按精确值判定。

C 里 `2^53+1 == (double)(2^53)` 只能得到第一个答案，因为比较时 int 一定会被隐式转成 double。
`l_float_tower.py` 把「0.1 的真身」挖到最底层：

```text
$ python3 l_float_tower.py
====================================================================
一、0.1 + 0.2 != 0.3：不是 Python 的 bug，是二进制小数的必然
====================================================================
0.1 + 0.2 == 0.3  → False
0.1 + 0.2 精确到 20 位： 0.30000000000000004441
0.3   精确到 20 位： 0.29999999999999998890
0.1 真实存的值（53 位二进制还原成十进制）： 0.1000000000000000055511151231257827021181583404541015625
用 Decimal 看 0.1 的真身： 0.1000000000000000055511151231257827021181583404541015625
正确姿势一：比较用容差 True
正确姿势二：要精确就用 Decimal（十进制精确，标准库自带） True
错误姿势：把 float 直接当钱数累加 —— 0.1 加十次： 0.9999999999999999 ；等于 1 吗？ False
```

`0.1` 存的是 `0.1000000000000000055511151231257827021181583404541015625` ——
不是「近似值存丢了」，而是**这就是它精确存储的那个数**，二进制小数里根本没有 0.1 这个位置。
最后一行「0.1 加十次 = 0.9999999999999999」是给「拿 float 当钱」的人准备的现场证据。

第二个 block 讲跨类型比较凭什么成立：

```text
1 == 1.0 → True ；type(1) = int type(1.0) = float
它们是同一个对象吗（is）→ False ← 不同类型必然是两个对象
int 和 float 能比，是因为它们都在 numbers 塔的「精确值」语义下比较：
  isinstance(1, numbers.Integral) = True
  isinstance(1.0, numbers.Integral) = False
  isinstance(True, numbers.Integral) = True ← bool 也算整数
关键：int 和 float 直接比时，Python 不偷懒把 int 转成 float，它按精确值比 ——
  float 只能精确表示 2^53 以内那一档连续整数。先把两边都转成 float 再比：
    float(2**53 + 1) == float(2**53) → True  ← 转 float 就撞车了（精度不够）
  再让 int 直接和 float 比（不转换）：
    2**53 + 1 == float(2**53) → False  ← 仍然 False：这是精确比较的结果
  对照纯 int 之间：2**53 + 1 == 2**53 → False
→ C 里 int 和 float 比较靠隐式转换，撞车是无声无息的；Python 这条路上不会撞。
```

★ **结论（第 13 节）**：
1. **`float` 是二进制小数，不是「带误差的十进制」**。钱、精确计数一律 `decimal.Decimal`（标准库，无需 pip）。
2. **跨类型比较走「数值塔」**（`numbers.Integral` / `numbers.Rational` / …），
   语义是数学值相等；`hash` 也必须跟着一致（`hash(1) == hash(1.0) == hash(True)`，第 12 节已实测），
   否则 dict 就坏了。这是「`1 == True`、`1.0 == 1` 却 `1 is not True`」的完整解释。
3. 和 C 的分工：C 的比较靠隐式转换（有损、无声），Python 的比较靠精确语义（无损）。
   学 C 时养成的「比较就是比位模式」的直觉，在这里要换成「比较是调用类型的规则」。

## 14. 场景 12：类型挂在对象上；混合 list；int 无限精度（demo 第 14 节）

先证明「类型不挂在名字上」：同一个名字，两次赋值之后 `type()` 换了。

```text
$ python3 -c "x = 1; print(type(x).__name__); x = \"换\"; print(type(x).__name__)"
int
str
[退出码 0]

$ python3 -c "print(type(1) is int, isinstance(True, int), type(True) is int)"
True True False
[退出码 0]

$ python3 -c "import sys; print([sys.getsizeof(v) for v in (0, 1, 2**30, 2**60)])"
[28, 28, 32, 36]
[退出码 0]

$ python3 -c "print(2**200)"
1606938044258990275541962092341162602522202993782792835301376
[退出码 0]

$ python3 -c "xs = [1, \"甲\", 3.0, None]; print([type(v).__name__ for v in xs])"
['int', 'str', 'float', 'NoneType']
[退出码 0]

$ python3 -c "print(2**53 + 1 > 2**53, 2**200 + 1 != 2**200)"
True True
```

最后一条在 demo 里跟的提示是：「最后一条对照 C：int64 加一在 C 里要么溢出要么未定义，这里永远是真。」
`2**53 + 1 > 2**53` 在 C 里用 `int64_t` 勉强能对、用 `int` 直接错、`2**200` 根本写不出来；
Python 里它是个普通表达式。

`m_types_int.py` 第一个 block 顺手把「类型注解」的运行时地位也钉了：

```text
$ python3 m_types_int.py
====================================================================
一、变量没有类型，对象才有
====================================================================
x = 1 → type(x) = int
换一次赋值：x = '现在我是字符串' → type(x) = str
再换一次：type(x) = list
→ 变量身上没有任何地方写着「x 是 int」。写 x: int = 1 也只是注解，运行时不拦你：
   先 y: int = 1 再 y = '换了'，结果 y = '换了'
   注解只是被存起来没人执行：f.__annotations__ = {'a': <class 'int'>, 'return': <class 'str'>}
```

★ 这一段解决初学 C 的人最常见的疑问：「Python 是弱类型吗？」
准确说法是**动态强类型**：类型在运行时属于对象（动态），
但解释器不会替你偷偷把 str 当 int 用（强）。类型注解 `x: int = 1` 在运行时**完全不执行**，
它只是一个被存进 `__annotations__` 的字典项。

第二个 block 是数据结构课的对照点：混合 list 到底贵在哪。

```text
mixed = [1, '二', 3.0, None, True]
每个元素的类型： ['int', 'str', 'float', 'NoneType', 'bool']
list 里存的不是值，是指向对象的指针（本机 64 位，每个槽 8 字节）
sys.getsizeof(mixed) = 104 ← 只是外壳 + 指针数组，不含元素本体
把每个本体的大小加起来 = 156
→ 代价一：每个元素单独占一块内存（装箱），比 C 的 int 数组费得多；
  代价二：遍历时每次都得先问类型才能干活，这就是「Python 慢」的一大半原因；
  好处：写起来自由。想要 C 那种紧凑数组，标准库自带 array：
  array('i', ...) 的 sys.getsizeof = 100 ，元素类型统一，溢出会报错： OverflowError: signed integer is greater than maximum
```

`104` 和 `156` 这两个数放一起就是结论：`sys.getsizeof(list)` **不含元素本体**，
它量的是「指针数组 + 头部」。`array('i', ...)` 那一行是标准库里的 C 风格数组，
溢出直接 `OverflowError` —— 和 C 的绕回去不同，它是显式报错。

第三个 block 把第 10 节埋的「两种类型检查」结清：

```text
type(1) is int     → True
isinstance(1, int) → True
type(True) is int  → False ← 严格：bool 被排除
isinstance(True, int) → True ← 宽容：bool 是 int 的儿子
isinstance(SubInt(5), int) → True
type(SubInt(5)) is int     → False
→ 一般写 isinstance（允许子类，也允许 numbers 那种虚拟子类）；
  只有「我就要精确这一种类型」才写 type() is。
```

第四个 block 是全章数值部分的收尾，`getsizeof` 一档一档跳，并把「每 30 位加 4 字节」自己核算过：

```text
C 的 int 是 32 位（本机 sizeof 见第 1 节），INT_MAX = 2147483647；
Python：2**31 = 2147483648 ；2**63 = 9223372036854775808
2**200 = 1606938044258990275541962092341162602522202993782792835301376
2**200 的十进制位数 = 61
2**1000 的十进制位数 = 302
50! = 30414093201713378043612608166064768844377641568960512000000000000 ；位数 = 65
```

（同份输出的档位表，12 行全部带自校验）

```text
它是「怎么长大」的？看 sys.getsizeof 一档一档跳：
（本机 sys.int_info：每 30 位一个 digit，一个 digit 4 字节）
  值为 0 的对象占 28 字节；去掉 digit 的纯头部是 24 字节
  2**0     占   28 字节 →   1 个 digit（1 个二进制位需要 1 个 30 位 digit）  ✓ 对得上
  2**1     占   28 字节 →   1 个 digit（2 个二进制位需要 1 个 30 位 digit）  ✓ 对得上
  2**29    占   28 字节 →   1 个 digit（30 个二进制位需要 1 个 30 位 digit）  ✓ 对得上
  2**30    占   32 字节 →   2 个 digit（31 个二进制位需要 2 个 30 位 digit）  ✓ 对得上
  2**31    占   32 字节 →   2 个 digit（32 个二进制位需要 2 个 30 位 digit）  ✓ 对得上
  2**59    占   32 字节 →   2 个 digit（60 个二进制位需要 2 个 30 位 digit）  ✓ 对得上
  2**60    占   36 字节 →   3 个 digit（61 个二进制位需要 3 个 30 位 digit）  ✓ 对得上
  2**61    占   36 字节 →   3 个 digit（62 个二进制位需要 3 个 30 位 digit）  ✓ 对得上
  2**90    占   40 字节 →   4 个 digit（91 个二进制位需要 4 个 30 位 digit）  ✓ 对得上
  2**200   占   52 字节 →   7 个 digit（201 个二进制位需要 7 个 30 位 digit）  ✓ 对得上
  2**1000  占  160 字节 →  34 个 digit（1001 个二进制位需要 34 个 30 位 digit）  ✓ 对得上
  2**10000 占 1360 字节 → 334 个 digit（10001 个二进制位需要 334 个 30 位 digit）  ✓ 对得上
→ 28 → 32 → 36 …… 每 30 个二进制位加 4 字节。C 里这叫「得换个更大的类型」，
  Python 里是对象自己变长，标签（变量名）完全不知道也不关心。
```

每行末尾那个 `✓ 对得上` 是脚本自己算的：用 `sys.getsizeof` 的字节数反推 digit 数，
再和「二进制位数 ÷ 30 向上取整」对比。12 行全对，才敢说「每 30 位 4 字节」。

最后一件事：**无限精度有唯一一处限速**，而且限的不是「算」是「打印」：

```text
唯一会拦你的是「把它转成字符串」这一步（防止有人拿它烧 CPU）：
ValueError: Exceeds the limit (4300 digits) for integer string conversion; use sys.set_int_max_str_digits() to increase the limit
默认上限 sys.int_info.default_max_str_digits = 4300
算出来不受限：2**100000 这个 int 建成了，getsizeof = 13360 字节；
len(str(2**1000)) = 302 ，只有超过 4300 位才报。
```

★ **结论（第 14 节）**：
1. **类型属于对象，不属于名字**；要判类型用 `isinstance`（认子类、认虚拟子类），
   只有明确要「精确这个类」才用 `type() is`。
2. **无限精度的价钱是「每 30 个二进制位 4 字节 + 每次运算按 digit 走」**，
   所以 `2**10000` 只占 1360 字节，而 `int→str` 有 4300 位的安全阀（`ValueError`）。
3. **混合 list = 指针数组 + 一堆散落的对象**（`getsizeof` 104 不含本体 156），
   这就是「好写但慢」的物理解释；要紧凑数组用 `array`。

## 15. 场景 13：赋值语义全家桶（多重赋值 / 交换 / 解包）（demo 第 15 节）

```text
$ python3 -c "a = b = []; a.append(1); print(b, a is b)"
[1] True
[退出码 0]

$ python3 -c "a, b = 1, 2; a, b = b, a; print(a, b)"
2 1
[退出码 0]

$ python3 -c "import dis; dis.dis(compile(\"a, b = b, a\", \"<s>\", \"exec\"))"
  0           RESUME                   0

  1           LOAD_NAME                0 (b)
              LOAD_NAME                1 (a)
              SWAP                     2
              STORE_NAME               1 (a)
              STORE_NAME               0 (b)
              LOAD_CONST               0 (None)
              RETURN_VALUE
[退出码 0]

$ python3 -c "import dis; dis.dis(compile(\"a = b\nb = a\", \"<s>\", \"exec\"))"
  0           RESUME                   0

  1           LOAD_NAME                0 (b)
              STORE_NAME               1 (a)

  2           LOAD_NAME                1 (a)
              STORE_NAME               0 (b)
              LOAD_CONST               0 (None)
              RETURN_VALUE
[退出码 0]
```

这两段 `dis` 摆在一起是本节的核心证据：

- `a, b = b, a`：`LOAD_NAME b`、`LOAD_NAME a` **先把两个值都压到栈上**，
  然后 `SWAP 2`，再依次 `STORE_NAME`。值一旦上了栈就和名字脱钩，所以不会互相覆盖。
- `a = b; b = a`（C 风格的「以为能交换」写法）：第一条 `STORE_NAME 1 (a)` 已经把 `a` 原来的值
  从名字表里挤掉了，第二条读到的 `a` 是新值 —— 两个名字最后指向同一个对象。

还有一件 3.14 才看得出来的事：**交换根本没建元组**。上面字节码里没有 `BUILD_TUPLE`，
只有 `SWAP`，这就是证据（`n_assign_pack.py` 里也重复了这条结论）。

解包部分：

```text
$ python3 -c "first, *rest = [1, 2, 3, 4]; print(first, rest)"
1 [2, 3, 4]
[退出码 0]

$ python3 -c "*head, last = (1, 2, 3); print(head, last)"
[1, 2] 3
[退出码 0]

$ python3 -c "x, y = 1, 2, 3"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    x, y = 1, 2, 3
    ^^^^
ValueError: too many values to unpack (expected 2, got 3)
[退出码 1]

$ python3 -c "x, y = 5"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    x, y = 5
    ^^^^
TypeError: cannot unpack non-iterable int object
[退出码 1]

$ python3 -c "*a, *b = [1, 2]"
  File "<string>", line 1
SyntaxError: multiple starred expressions in assignment
[退出码 1]
```

四条错误分属三个层级，值得背下来：数量不匹配是 `ValueError`（运行时才知道），
右边不可迭代是 `TypeError`（也是运行时），两个星号是 `SyntaxError`（编译期就拒，退出码 1 且没跑）。

`n_assign_pack.py` 的完整取证：

```text
$ python3 n_assign_pack.py
====================================================================
一、a = b = []：一个对象，两张标签（本章第二大坑）
====================================================================
a = []  b = []
a is b 吗？ True ← 两个名字贴的是同一个 list
往 a 追加之后，b = [1] ← 它不是复制品，它就是同一个东西
对照正确写法（各贴各的）：
c = [1]  d = [] ；c is d = False
再对照一个「长得一样但不会咬人」的：
整数版：e = 1  f = 0 ← int 不可变，e + 1 造了新对象，f 还贴在 0 上
→ 这个坑只在可变对象上咬人；和第 4 节是同一件事的两面。
```

（第二个 block「二、a, b = b, a 是怎么做到的（看字节码）」——它打的字节码与上面 `-c` 那条逐字相同，
这里不再重复，只留脚本自己的读法和那条 C 风格对照，原文照抄）

```text
读法：先把 b、a 都 LOAD 上栈（值已经在栈上，和名字脱钩），
      SWAP 2 把栈顶两个换位置，然后依次 STORE_NAME 给 a、b。
      因为两个值都先被读进栈里，所以不存在「先把 a 覆盖掉再读」的问题。

对照 C 风格的错误写法（右边先覆盖了左边）：
不用元组：p = 2  q = 2 ← 两个都变成 2，原来的 1 丢了
用 a, b = b, a：  p = 2  q = 1

顺带：3.14 里这个「元组」其实没被建出来 —— 上面那段字节码里没有 BUILD_TUPLE，
只有 SWAP，这就是证据。编译器看穿了你只是要交换。
```

第三个 block 把星号解包的边角料凑齐，包括那个「一元素元组必须带逗号」的老坑：

```text
first = 10  rest = [20, 30, 40] ；rest 的类型 = list
*head = [10, 20]  last = 30
low = 0  mid = [1, 2, 3]  high = 4
注意那个逗号：one, = [7] → one = 7 （写成 one = [7] 的话 one 是个 list）
星号目标可以空手：x, *y, z = (1, 2) → x = 1  y = []  z = 2
```

（同一份输出里的失败现场表，`-c` 只演示了两条，脚本把四条凑全）

```text
失败现场（这三条报错原文值得背下来）：
  x, y = 1, 2, 3   → ValueError: too many values to unpack (expected 2, got 3)
  x, y, z = 1, 2   → ValueError: not enough values to unpack (expected 3, got 2)
  *a, *b = [1, 2]  → SyntaxError: multiple starred expressions in assignment (<string>, line 1)
  x, y = 5         → TypeError: cannot unpack non-iterable int object
→ 解包要求「可迭代」且「数量对得上」。最后一条说明 int 根本不是可迭代对象。
```

★ 注意 `*a, *b` 那条在脚本里是**编译**它才拿到的报错，所以括号里带了 `(<string>, line 1)`，
和前面 `-c` 直接跑的那条格式不同 —— 同一个 `SyntaxError`，两种呈现，因为一个是 `compile()` 一个是真跑。

第四个 block 把链式赋值和增强赋值收进同一套解释：

```text
m = n = o = 5 → 三个名字同一个对象吗？ True
xs1 += [3] 之后 xs2 = [1, 2, 3] ← 原地吸收，两张标签都看见
```

（它后面打的字节码和第 5 节 `-c` 那条 `xs += [3]` 逐字相同，唯一区别是名字换成 `xs1`，
所以这里不重复贴；同一份输出的收尾两行是本章 `+=` 结论的官方版本：）

```text
→ 本机 3.14 打出来是 BINARY_OP 13 (+=)：普通加法和原地加法共用一条 BINARY_OP，
  靠后面那个编号区分该用哪套槽位方法（先试 __iadd__，没有才退回 __add__）。
```

★ **结论（第 15 节）**：所有赋值形式（链式、元组解包、星号、增强）在字节码层面都是
**「先把右边的值全准备好，再逐个 STORE_NAME」**。
由此三条推论：`a = b = []` 必然是一个对象两张标签（想各来一个就写 `a = []; b = []` 或
`[[] for _ in range(n)]`）；`a, b = b, a` 不需要临时变量，因为值先进栈；
`+=` 之后一定会 `STORE_NAME`，所以「原地还是换对象」只能由 `__iadd__` 决定（第 5 节）。

## 16. 场景 14：`del` 删的是名字，不是对象（demo 第 16 节）

```text
$ python3 -c "import sys; xs = [1]; print(sys.getrefcount(xs))"
2
[退出码 0]

$ python3 -c "import sys; xs = [1]; ys = xs; print(sys.getrefcount(xs))"
3
[退出码 0]

$ python3 -c "xs = [1]; ys = xs; del xs; print(ys)"
[1]
[退出码 0]

$ python3 -c "xs = [1]; del xs; print(xs)"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    xs = [1]; del xs; print(xs)
                            ^^
NameError: name 'xs' is not defined
[退出码 1]
```

第三条是结论的一半：`del xs` 之后 `ys` 照样打印出 `[1]` —— **对象没被删**。
第四条是另一半：报错是 `NameError: name 'xs' is not defined`，
它抱怨的是「这个名字不存在」，不是「这个东西被销毁了」。
第一条那个 `2` 也是常客：多出来的那 1 份是 `getrefcount` 自己的参数引用，
所以 `getrefcount` **只能看增量，不能看绝对值**。

`o_refcount_del.py` 把增量一格一格数出来，并用 `weakref` 当「对象死没死」的证人：

```text
$ python3 o_refcount_del.py
====================================================================
一、每多一个持有者，计数加 1（看增量，别看绝对值）
====================================================================
1 个名字：  增量 0 绝对值 2
加 1 个名字：增量 1
塞进 list： 增量 2
塞进 dict： 增量 3
绝对值现在是 5
删掉三个持有者之后：增量 0
→ 每行都恰好 +1：名字、list 的槽、dict 的值，全是对同一个对象的引用。
  绝对值里含解释器自己的临时引用，所以只能看差值；
  顺带一个现象：同一段代码在函数里测，比在模块顶层测少 1（本机如此，本章不解释谁多持有一份）。
  对照：函数里的局部对象： 1
```

★ 「函数里比模块顶层少 1」这一行是诚实记录：本机确实测出这个差，但**本章没能证明多出来的那份
是谁持有的**，所以只描述现象、不给解释（相关猜测列在最后的「没演/没验证」清单里）。

```text
del keep 之后 also = [1, 2, 3]
报错原文： name 'keep' is not defined
→ NameError 说的是「这个名字不存在」，不是「这个东西被删了」。
  对象活得好好的，因为 also 还指着它。
```

（第三 block，`weakref` 证人）

```text
有名字时，证人看得见对象吗？ True
del 之后，证人还看得见吗？ False
→ 是 False：计数归 0，对象当场释放，不用等 GC。（这也是为什么 del 之后 id 会被复用）
```

这两行是**整章取证方法的支点**：`weakref` 不增加引用计数，所以它能在不改动物件寿命的前提下
回答「对象还在不在」。计数归零 → 立刻释放，这是引用计数的确定性，也是 Python 里
「没有析构延迟」的来源；第 2 节里 `id` 撞上旧地址，原因就在最后括号那句。

（第四 block：`del` 的其它四种目标）

```text
del data[1] → [1, 3, 4]
del data[1:] → [1]
del 属性之后 hasattr？ False
del 一个不存在的名字： NameError: name 'never_existed' is not defined
```

★ **结论（第 16 节）**：`del` 永远是**移除一个绑定**，作用对象是「名字/槽位/键/属性」这一层；
对象只在引用计数归零那一刻释放。
把 `del xs` 理解成「删掉那个 list」是本章第一号误区 —— 正确说法是「把 `xs` 这张标签撕掉，
没人贴了就回收」。

## 17. 场景 14 续：引用计数数不出环，靠分代 GC 兜底（demo 第 17 节）

```text
$ python3 -c "import gc; print(gc.get_threshold(), gc.isenabled())"
(2000, 10, 0) True
[退出码 0]
```

`gc.get_threshold()` 打出来的是三个数的元组，`gc.isenabled()` 是 True。
**这三个数各自的确切语义本章没有验证**（本机它们确实是 `(2000, 10, 0)`，仅此而已），
完整机制写进了文末「没演」清单；本节只用「GC 可以被整体关掉」这一件事来证明两套机制是两套。

接下来这条多行 `-c` 是本章最漂亮的一个实验：**先关掉 GC，让引用计数独自面对环**。

```text
$ python3 -c "
import gc, weakref
class Node: pass
gc.disable()                      # 先关掉分代 GC，只剩引用计数
a, b = Node(), Node()
a.peer, b.peer = b, a             # 互相指着
ra, rb = weakref.ref(a), weakref.ref(b)
del a, b
print('关了 gc、del 两个名字之后，环还活着吗：', ra() is not None or rb() is not None)
gc.enable()
gc.collect()
print('gc.collect() 之后没了？', ra() is None and rb() is None)
"
关了 gc、del 两个名字之后，环还活着吗： True
gc.collect() 之后没了？ True
[退出码 0]
```

读法：`a`、`b` 两个名字被 `del` 之后，两个节点**互相**还各持有对方一份，计数都是 1，
引用计数一个都释放不了 → 证人还看得见（True）。
`gc.collect()` 走一趟，环被拆掉，两个证人同时看不见（True）。
这就是「引用计数会漏，Python 用分代 GC 补漏」的完整现场。

```text
$ python3 -c "import gc; print(gc.collect() >= 0)"
True
[退出码 0]
```

★ **结论（第 17 节）**：Python 的内存回收是**两套机制叠着**：
引用计数负责「确定性释放」（计数归 0 立刻放，所以 `with` / 文件关闭不靠运气），
分代 GC 只负责「引用计数处理不了的循环引用」（所以时机不可预期）。
推论：`__del__` 的触发时机对**无环**对象是可预测的（第 16 节已经用 `weakref` 证到「当场释放」），
对**有环**对象不可预测 —— 这一半本章没演，见文末清单。

## 18. 场景 15：作用域最小版 —— `UnboundLocalError` 报错原文逐行读（demo 第 18 节）

```text
$ python3 -c "
count = 0
def bump():
    print(count)
    count = count + 1
bump()
"
Traceback (most recent call last):
  File "<string>", line 6, in <module>
    bump()
    ~~~~^^
  File "<string>", line 4, in bump
    print(count)
          ^^^^^
UnboundLocalError: cannot access local variable 'count' where it is not associated with a value
[退出码 1]
```

这段报错每一行都有含义，逐行拆：

| 那一行 | 含义 |
| --- | --- |
| `Traceback (most recent call last):` | 栈从外往内打印，最后一行是**出事的那一行** |
| `File "<string>", line 6, in <module>` | 外层是模块（`<module>`），在第 6 行调 `bump()`；文件名是 `<string>` 因为这是 `-c` |
| `~~~~^^` | `~~~` 标的是被调用的表达式，`^^` 标的是这次调用里出问题的那一小段（01 章第 7 节讲过这套记号） |
| `File "<string>", line 4, in bump` | 内层帧是函数 `bump`，第 4 行 |
| `print(count)` + `^^^^^` | 波浪号指向 `count` —— **读**它才炸，不是写它 |
| `UnboundLocalError: cannot access local variable 'count' where it is not associated with a value` | 全文见下方解读 |

那句话的中文直译是「不能访问局部变量 `count`，它还没有和任何值关联」。
三个关键点：

1. 它说 **local variable** —— 说明解释器已经认定 `count` 是局部的（尽管模块层真有一个 `count = 0`）。
2. 「not associated with a value」正是第 2 节的语言：名字是标签，这张局部标签此刻**还没贴上任何东西**。
3. 报错类型叫 `UnboundLocalError`，名字里的「unbound」正是第 2 节的语言：绑定（binding）不存在。
   它和 `NameError` 的继承关系本章**没演**（见文末清单），别在这里当结论用。

`p_scope_min.py` 把「为什么局部」用字节码钉死：

```text
$ python3 p_scope_min.py
====================================================================
一、只读全局：没问题
====================================================================
read_only() = 0
globals() 里有 count 吗？ True
函数对象自己不保存全局值，它在被调用的那一刻去模块的 globals dict 里取：
改了全局之后再调：read_only() = 42
```

（第二个 block，注意第一行：那行 print **是执行过的**）

```text
这一行会打印，下一行才炸
UnboundLocalError 原文（本节主角）：
   cannot access local variable 'count' where it is not associated with a value

换成带注解的写法，一模一样：
   cannot access local variable 'n' where it is not associated with a value
```

★ 常见误解纠正：**不是「执行到赋值语句才炸」**。函数从第一行开始，`count` 就已经是局部名字了，
所以任何「先读后写」都会炸；写在后面只是让你以为前面的读走的是全局。
带注解（`n: int` 那种）也不改变判定 —— 注解不参与作用域决策。

字节码层的证据（缩进与编号照抄本机输出）：

```text
字节码层看得最清楚：编译期就把 count 定成了 LOAD_FAST/STORE_FAST（局部槽），
而不是 LOAD_NAME/STORE_NAME（查模块 globals dict）：
 18           RESUME                   0

 20           LOAD_GLOBAL              1 (print + NULL)
              LOAD_CONST               1 ('这一行会打印，下一行才炸')
              CALL                     1
              POP_TOP

 21           LOAD_FAST_CHECK          0 (count)
              LOAD_SMALL_INT           1
              BINARY_OP                0 (+)
              STORE_FAST               0 (count)

 22           LOAD_FAST_BORROW         0 (count)
              RETURN_VALUE
```

第 21 行的 `LOAD_FAST_CHECK` 就是那句报错的生产者：它是「取局部槽，取到空值就抛
`UnboundLocalError`」的版本。对照第 3 节：模块层同名走 `LOAD_NAME`（查 dict，一定能查到全局）。

第三、四、五个 block 把边界补齐（深度留给后续章）：

```text
global 声明之后写全局名字：with_global() = 43 ；全局 count 现在是 43
nonlocal 声明之后写外层函数的名字：outer_demo() = 1
两个关键字都只是「别把这个名字当本地的」，不是「复制一份进来」。
```

（第四个 block 把第 16 节和这节接上）

```text
   cannot access local variable 'tmp' where it is not associated with a value
→ 和第 16 节连起来了：del 删名字；对象要是没人指着就当场释放。
```

第五个 block 只摆边界：

```text
InClass.doubled = 20 ← 类体里 base 那一刻可见
InClass().method() = 10 ← 方法里要靠 self，裸写 base 会 NameError
```

★ **结论（第 18 节）**：一个名字在函数里归哪一层，是**编译期**看整个函数体决定的，
规则只有一条：**函数体内任何位置对它赋值（含 `del`、增强赋值、注解不算），它就是局部名**。
于是「读局部名却还没贴标签」= `UnboundLocalError`。
这条规则和第 3 节的两套名字机制（dict vs 栈槽）是同一件事的两个视角。

## 19. 场景 15 续：`global` / `nonlocal` 只演示存在（demo 第 19 节）

```text
$ python3 -c "
count = 0
def good():
    global count
    count = count + 1
    return count
print(good(), count)
"
1 1
[退出码 0]

$ python3 -c "
def outer():
    seen = 0
    def inner():
        nonlocal seen
        seen += 1
        return seen
    return inner()
print(outer())
"
1
[退出码 0]
```

第一条的两个 `1` 是关键：函数返回 1，**外面的** `count` 也变成 1 —— `global` 改的就是模块 dict 里那个键。
第二条 `nonlocal` 改的是**外层函数的栈槽**（不是全局，也不是本地新副本）。
闭包、`nonlocal` 与自由变量的完整机制留第 05 章，这里只证明「这两个关键字确实改变绑定目标」。

同一节还演示了「不加声明就是另一回事」，以及一个诚实的坑：

```text
$ python3 -c "
total = 0
def f():
    print(total)
    total = 1
f()
" | tail -4
Traceback (most recent call last):
  File "<string>", line 6, in <module>
    f()
    ~^^
  File "<string>", line 4, in f
    print(total)
          ^^^^^
UnboundLocalError: cannot access local variable 'total' where it is not associated with a value
[退出码 0]
```

★ 注意最后那个 `[退出码 0]`：**这条命令走了管道，`[退出码]` 是 `tail` 的，不是 `python3` 的**
（demo 故意没开 `pipefail`）。想拿 python3 的退出码看第 18 节那条不带管道的，那里是 `[退出码 1]`。
这是「读 demo 输出」的一个通用陷阱，01 章第 8 节也踩过一次。

最后一条给出「绕过作用域」的第三种写法（能跑，但没人这么写）：

```text
$ python3 -c "count = 0; exec(\"count = count + 1\", globals()); print(count)"
1
[退出码 0]
```

★ **结论（第 19 节）**：想让函数改外面的名字，只有三条路 ——
`global`（改模块 dict）、`nonlocal`（改外层栈槽）、直接改对象本体（第 2、4 节的 `append`）。
第三条不需要任何关键字，也正因为如此它才是 Python 的默认风格：
**能不重新绑定就不重新绑定，要改就改对象**。

## 20. 场景 16：想复制的时候该写什么（`copy` / `deepcopy` / 切片）（demo 第 20 节）

```text
$ python3 -c "import copy; xs = [[1], [2]]; ys = copy.copy(xs); print(ys is xs, ys[0] is xs[0])"
False True
[退出码 0]

$ python3 -c "import copy; xs = [[1], [2]]; ys = copy.deepcopy(xs); print(ys[0] is xs[0])"
False
[退出码 0]

$ python3 -c "xs = [[1], [2]]; ys = xs[:]; ys[0].append(9); print(xs)"
[[1, 9], [2]]
[退出码 0]

$ python3 -c "import copy; print(copy.copy(5) is 5, copy.deepcopy(\"ab\") is \"ab\")"
<string>:1: SyntaxWarning: "is" with 'int' literal. Did you mean "=="?
<string>:1: SyntaxWarning: "is" with 'str' literal. Did you mean "=="?
True True
[退出码 0]
```

三条 `False True / False / [[1, 9], [2]]` 分别对应浅拷贝、深拷贝、切片：
`copy.copy` 换外壳不换内层，`deepcopy` 连内层一起换，**切片是浅拷贝**
（第三条里改的是 `ys` 的内层，`xs` 却变成了 `[[1, 9], [2]]` —— 内层是同一个 list）。
第四条说清「不可变对象复制就是返回自己」，所以 `copy` 一个 int 毫无意义（还白挨两个警告）。

`q_copy_table.py` 是这节的正片，第一个 block 把三种写法并排跑：

```text
$ python3 q_copy_table.py
====================================================================
一、三种「得到一个变量」的写法，结果完全不同
====================================================================
赋值来的 alias 是同一个对象吗？ True
浅拷贝 shallow 是新对象吗？ True
深拷贝 deep 是新对象吗？ True
但浅拷贝里面那层 list 还是共用的： True
深拷贝里面那层也是新的： True

改原件内层 → alias 看到： ['a', 'b', '改了原件的内层']
改原件内层 → shallow 也看到： ['a', 'b', '改了原件的内层'] ← 浅拷贝的痛点
改原件内层 → deep 不受影响： ['a', 'b']
外层换成新值 → alias = 99 ；shallow = 1 ；deep = 1
→ 外层换值是贴标签，只影响还指着旧对象的那些名字；内层就地改，所有共用者都看见。
```

第二、三个 block 有个**反直觉但实测如此**的角落：`frozenset` 和「含 list 的 tuple」。

```text
  1             copy 之后同一个对象？ True    deepcopy 之后同一个对象？ True
  'abc'         copy 之后同一个对象？ True    deepcopy 之后同一个对象？ True
  (1, 2)        copy 之后同一个对象？ True    deepcopy 之后同一个对象？ True
  None          copy 之后同一个对象？ True    deepcopy 之后同一个对象？ True
  True          copy 之后同一个对象？ True    deepcopy 之后同一个对象？ True
  3.5           copy 之后同一个对象？ True    deepcopy 之后同一个对象？ True
  frozenset()   copy 之后同一个对象？ True    deepcopy 之后同一个对象？ False
→ int / str / tuple / None / bool 这些不可变的，复制返回本体：白多一张标签，不浪费内存。
```

`frozenset()` 那行是全表唯一的 `deepcopy → False`，本机实测如此；
**为什么它 deepcopy 会造新对象，本章没验证**（写进文末清单），但「会造新的」这个事实是打出来的。

```text
copy.copy(tuple) 是新对象吗？ False
copy.deepcopy(tuple) 是新对象吗？ True
deepcopy 之后里面的 list 换了吗？ True
→ 壳虽然不可变，但里面元素被换了新对象，所以壳必须重造 —— 这就是两者的分界线。
```

第四个 block 是「我明明复制了二维数组」的事故现场：

```text
rows[:] 是新 list 吗？ True ；内层共用吗？ True
list(rows) 同理，内层共用吗？ True
改 sliced 的内层 → rows = [[1, 2, 99], [3, 4]]  ← 所谓「复制了二维数组」是假的
要真的独立：一层层列表推导 → 推出来的 = [[1, 2, 99], [3, 4]] ；原件 = [[1, 2, 99, -1], [3, 4]] （内层不共用了）
或者 deepcopy 之后改原件 → 快照 = [[1, 2, 99, -1], [3, 4]] ；原件 = [[1, 2, 99, -1, -1], [3, 4]]
```

第五个 block 把其它常用类型的「要一份独立副本」写法一次跑完（这几个数都是本机现测）：

```text
str 不可变，切片就是本体：st[:] is st → True
bytes 不可变，要改就转 bytearray： b'Abc' （原 bs 还是 b'abc' ）
array 的切片是独立副本吗？ True
set 的两种复制都给新外壳： True True
```

★ **结论（第 20 节）**：一张表就够 ——

| 你想干什么 | 写什么 | 本机证据 |
| --- | --- | --- |
| 只是换个名字指同一个东西 | `ys = xs` | 第 2 节：`id` 相同，`append` 双方都看得见 |
| 要新外壳、内层共用没关系 | `copy.copy(xs)` / `xs[:]` / `list(xs)` / `set(xs)` | 本节 block 4：内层照样共用 |
| 要一份完全独立 | `copy.deepcopy(xs)` | block 1：`deep 不受影响` |
| 一层嵌套、想要内层也独立 | `[[*row] for row in rows]` | block 4：「推出来的」那行 |
| 复制不可变对象 | 什么都不用写 | block 2：`copy`/`deepcopy` 返回本体（`frozenset` 的 `deepcopy` 例外，见清单） |

## 21. 一页纸取证：本章最常写错的八行，当场判对错（demo 第 21 节）

```text
$ python3 -c "x = 300; print(x + 0 is x + 0, x + 0 == x + 0)"
False True
[退出码 0]

$ python3 -c "
def append_to(target, item):
    target = target + [item]      # 换标签：外面看不见
    return target
xs = [1]
append_to(xs, 2)
print(xs)
"
[1]
[退出码 0]

$ python3 -c "
def append_inplace(target, item):
    target += [item]              # 原地吸收：外面看得见
xs = [1]
append_inplace(xs, 2)
print(xs)
"
[1, 2]
[退出码 0]

$ python3 -c "xs = [1, 2]; ys = xs; ys += [3]; print(xs)"
[1, 2, 3]
[退出码 0]

$ python3 -c "s = \"a\"; t = s; t += \"b\"; print(s)"
a
[退出码 0]
```

第二条和第三条是**本章所有内容的浓缩**：函数参数是贴标签（第 2 节 block 3），
所以 `target = target + [item]` 只是把函数里那张局部标签撕到新对象上，外面看不见；
`target += [item]` 走 `list.__iadd__`，改的是外面那个对象本体。
第四条把第 5 节的「+= 会影响所有标签」再验一遍，第五条同一件事在 str 上不成立。

```text
$ python3 -c "
def f(a=[]):
    a.append(1)
    return a
print(f(), f())
"
[1, 1] [1, 1]
[退出码 0]
```

`[1, 1] [1, 1]` —— 两次调用打印的是**同一个 list**（默认值只在 `def` 执行那一刻贴一次标签）。
这朵「最出名的花」的**根因属于本章**（默认值是绑在函数对象上的一个标签），
机制的完整展开在第 05 章。

```text
$ python3 -c "
key = [1, 2]
try:
    d = {key: 1}
except TypeError as e:
    print('TypeError:', e)
"
TypeError: cannot use 'list' as a dict key (unhashable type: 'list')
[退出码 0]

$ python3 -c "import sys; xs = [1]; xs.append(2); print(sys.getrefcount(xs) >= 1)"
True
[退出码 0]
```

第一条给的是 dict 键的官方报错（和第 6 节 `hash((1, [2]))` 那条是同一件事的两个入口，
文案不一样：这里 `cannot use 'list' as a dict key (unhashable type: 'list')`，
那里 `unhashable type: 'list'`）。第二条是 `getrefcount` 只能看差值的温和版本。

## 22. 取证手段汇总：这一章用到的标准库入口（demo 第 22 节）

```text
$ python3 -c "import sys, gc, dis, copy, math, weakref, numbers, decimal, array; print(\"标准库全部可用：\", all([sys, gc, dis, copy, math, weakref, numbers, decimal, array]))"
标准库全部可用： True
[退出码 0]

$ python3 -c "import sys; print(sys.getsizeof([1,2,3]), sys.getsizeof(\"abc\"), sys.getsizeof((1,2)), sys.getsizeof({1:2}))"
88 44 64 224
[退出码 0]

$ python3 -c "import sys; print(sys.version_info)"
sys.version_info(major=3, minor=14, micro=4, releaselevel='final', serial=0)
[退出码 0]

$ python3 -c "import dis; print([op.opname for op in dis.get_instructions(compile(\"a += b\", \"<s>\", \"exec\"))])"
['RESUME', 'LOAD_NAME', 'LOAD_NAME', 'BINARY_OP', 'STORE_NAME', 'LOAD_CONST', 'RETURN_VALUE']
[退出码 0]
```

四个空壳对象的 `getsizeof`（88 / 44 / 64 / 224）值得存下来当尺子：
list 外壳按槽位长（第 14 节 `mixed` = 104）、str 带缓存、tuple 定长、**dict 空表就 224 字节**。

最后一行是「用 `dis` 但不写整个反汇编」的写法：`dis.get_instructions()` 给结构化指令，
`op.opname` 取助记符（**注意：属性叫 `opname`，不是 `name`** —— 本章第一版在这里踩过 `AttributeError`）。

## 23. 收尾：证明这一章没在手册目录里留下任何产物（demo 第 23 节）

```text
$ 〔在 02-types-and-variables 里〕 find . \( -name '__pycache__' -o -name '*.pyc' -o -name 'a_c_box' -o -name '*.out' \) | wc -l
0
[退出码 0]

$ 〔在 02-types-and-variables 里〕 ls | wc -l
18
[退出码 0]
```

第一条是硬性红线：`0`。gcc 的可执行文件、`__pycache__`、`.pyc` 全生在沙箱里。
第二条的 `18` 是**这次捕获时**手册目录里的文件数（17 个示例源码 + `demo.sh`），
现在多了你正在读的这份 `README.md`，所以你自己跑会变成 19 —— 这条不是产物计数。

沙箱内部的 `ls` 长这样（有编译产物是正常的，它在 `/tmp` 里）：

```text
$ ls
a_c_box
a_c_box.c
b_name_object.py
```

（上面只引了前三项；这份输出共 19 项：17 个示例源码 + `demo.sh` + 现编出来的 `a_c_box`，
`demo.sh` 会把源码整份拷进沙箱再跑，所以目录名之外没有绝对路径出现在任何输出里。
余下 16 行按字母序是 `c_assign_dis.py` 到 `q_copy_table.py`。）

```text
$ ls __pycache__
ls: cannot access '__pycache__': No such file or directory
[退出码 2]
```

```text
$ python3 -c "import os; print(sorted(d for d in os.listdir(\".\") if \"pycache\" in d or d.endswith(\".pyc\")))"
[]
[退出码 0]
```

（demo 在末尾打的两行收尾信息，原文照抄；第一行带沙箱目录，后缀会随运行变化）

```text
沙箱位置：/tmp/py02-OQI9dh（默认跑完就删；想留着继续敲就用 bash demo.sh --keep）
[全部跑完] 上面每一段都是真实执行结果，README 引用的输出全部出自这一次运行。
```

★ **结论（第 23 节）**：`ls __pycache__` 的退出码 2 是**好事** —— 它证明本章全程用
`python3 xxx.py` 直接跑脚本，从没 `import` 过任何示例模块，所以解释器没有理由生成 pyc。

## 一页纸（考前扫这半页）

三句话：

1. **名字是标签，对象才是东西**：赋值只改指向，从不搬值（字节码叫 `STORE_NAME`）。
2. **可变 / 不可变是对象的属性，而且是浅层的**：改对象 → 所有标签看见；换标签 → 只有这张标签看见。
3. **`==` 问「按这个类型的规则相等吗」，`is` 问「是同一个对象吗」**；
   前者受 IEEE 754 / 数值塔规则支配（`nan != nan`、`0.1+0.2 != 0.3`、`1 == 1.0`），
   后者受 CPython 缓存与编译期优化支配（`-5..256`、像标识符的 str 字面量），**所以判值永远写 `==`**。

| 想干什么 | 正确写法 | 本机证据在哪 |
| --- | --- | --- |
| 想原地改 | `xs.append / xs[i] = / extend`，或自定义 `__iadd__` 返回 self | 第 2、4、5 节 |
| 想原地改但对象不可变 | 没有这条路：造新对象再 `=`，或换可变容器 | 第 4 节 `TypeError: 'str' object does not support item assignment` |
| 想换对象 | `x = 新值`（只影响这张标签） | 第 2 节第 4 条 `-c` |
| 想让两个名字各有一份 | 分别 `= []`；**别写** `a = b = []` | 第 15 节 block 1 |
| 想比较值 | `==`（浮点用 `math.isclose`，钱用 `Decimal`） | 第 13 节 |
| 想比较身份 / 判 None / 判单例 | `is` | 第 8、11 节 |
| 想复制一份外壳 | `copy.copy` / 切片 / `list()` / `set()` | 第 20 节 block 1、4 |
| 想复制一整棵树 | `copy.deepcopy` | 第 20 节 block 1「deep 不受影响」 |
| 想让对象当场释放 | 删掉最后一个引用（`del`），靠引用计数 | 第 16 节 block 3（`weakref` 证人变 False） |
| 想让类型注解生效 | 办不到：运行时不执行，交给外部检查器 | 第 14 节 block 1 |

## 这章的坑（按你会踩的顺序排）

1. **`a = b = []`**：一个对象两张标签。函数默认参数 `def f(a=[])` 是它的孪生兄弟（第 21 节实测 `[1, 1] [1, 1]`）。
2. **`+=` 的两种命运**：list 原地、str/tuple 换新（第 5 节）；`t[1] += [20]` 会**报错但已经改了内层**（第 6 节）。
3. **tuple 不是「深的常量」**：浅层不可变（第 6 节）。
4. **用 `is` 判数值**：小整数区间里对，出去就翻脸（第 7 节）；3.14 还会直接给你 `SyntaxWarning`，
   `-W error` 下变 `SyntaxError`（第 8 节）。
5. **网上抄的 `257 is 257 是 False` 在本机复现不了**：字面量被折成同一常量（第 7 节）。
6. **`if not x`** 把 `0 / '' / [] / None` 一视同仁（第 11 节）；要区分「没传」和「传了空」必须写 `is None`。
7. **切片/`list()`/`dict()` 是浅拷贝**：二维数组「复制了个寂寞」（第 20 节 block 4）。
8. **`1`、`True`、`1.0` 在 dict 里是同一个键**（第 10 节），`-0.0` 和 `0.0` 也是（第 12 节）。
9. **函数里「先读后写」= `UnboundLocalError`**，而且报错行是**读**那一行，不是赋值那一行（第 18 节）。
10. **`sys.getrefcount` 的绝对值不可信**（多算一个参数引用），**`id` 会复用**（第 16、2 节）。
11. **`gc.collect()` 的返回值含标准库自己的环**，每次不同，只能判非负（第 17 节）。
12. **走管道时 `[退出码 0]` 是管道末端命令的**，不是 python3 的（第 19 节）。

## 自检问题

1. `xs = [1, 2]; ys = xs; ys.append(3); print(xs)` 和 `ys = ys + [3]` 之后分别打印 `xs`，
   结果各是什么？用哪两条本章实测命令能证明你答对？（第 2、4 节）
2. 为什么 `x: int = 1` 拦不住 `x = "换"`？运行时那个 `int` 去哪了？（第 14 节 block 1）
3. `a = b = []; a.append(1)` 之后 `b` 是什么？把 `[]` 换成 `0`、`a = a + 1` 之后 `b` 是什么？
   两个答案为什么不一样？（第 15、5 节）
4. `2**53 + 1 == float(2**53)` 和 `float(2**53 + 1) == float(2**53)` 各是什么？为什么？（第 13 节）
5. 一句话解释 `nan != nan` 为真而 `nan in [nan]` 也为真。（第 12 节）
6. 为什么 `print(count)` 在 `count = count + 1` 之前也会炸？
   报错里哪个词已经告诉你它把 `count` 当什么了？（第 18 节）
7. `del xs` 之后 `ys`（原来 `ys = xs`）还能用吗？什么条件下对象当场释放，靠什么证明？（第 16 节）
8. `copy.deepcopy(frozenset()) is copy.deepcopy(frozenset())` 本章测出来是什么？
   这一条能不能当规则用？（第 20 节 + 下面的清单）

答不上来就重跑 `bash demo.sh` 对应那节 —— 每节开头的 `══════` 标题就是索引。

## 本章没演 / 没验证（绝不编造，都在这）

| 想写但跑不出来 / 没在本机证明的 | 状态 |
| --- | --- |
| `gc.get_threshold()` 三个数各自的确切语义（`(2000, 10, 0)` 只打了值） | 没演；手册章节地图里也没有 GC 专章，留作后续扩展 |
| 有循环引用时 `__del__` 的触发时机（只测了「GC 收掉环」，没测 `__del__` 何时跑） | 没演 |
| `frozenset` 为什么 `copy` 返回本体而 `deepcopy` 造新对象（只测出现象） | 原因没验证 |
| `UnboundLocalError` 与 `NameError` 的继承关系（`issubclass` 没跑） | 没演，留给第 07 章 |
| 「函数里 `getrefcount` 比模块顶层少 1」到底是哪多持了一份（只记录了现象） | 没验证，demo 里明写不解释 |
| `__slots__` 之后对象是不是就没有 `__dict__`（本章只在类体命名空间那节碰了 `__dict__`） | 没演，留给第 06 章 |
| 引用计数溢出的 `ob_ref_deleted`/延迟回收路径（`sys.getrefcount` 上限、`gc.freeze()`） | 没演 |
| 小整数缓存的**内存代价**（预分配数组在 `PyLongObject` 里的具体布局、`ob_digit` 字段） | 没演，只测了 `-5..256` 与 `getsizeof` |
| PyPy / GraalPy 上的缓存边界与驻留规则（本机只有 CPython 3.14.4） | 没环境，没法演 |
| 「强类型」这条语言层面的断言（第 14 节正文里那句「解释器不会偷偷把 str 当 int 用」）：本章**没有**跑 `1 + "1"` 之类的现场 | 没演，只作定性说明；`TypeError` 报错文本怎么读，第 01 章第 7 节演过 |
| `is` 比较在跨进程 / 序列化（pickle）后是否还成立 | 没演 |
| 「`is` 与 `==` 的性能差多少」 | 没演（计时类数字抖动大，本章不引） |
| `int` 加法溢出后如何**内部**决定进位（digit 数组的运算过程） | 没演，只测了尺寸跳档 |
| `dis` 输出里 `<code object ... at 0x...>` 的地址可复现性 | 刻意不引用（每次进程都不同） |

## 怎么跑这一章

```text
bash demo.sh                # 跑完自动删沙箱
bash demo.sh --keep         # 跑完保留沙箱，把目录名打给你，进去自己敲
```

`demo.sh` 的行为约定（红线，代码里都写了）：

- 只往 `mktemp -d "${TMPDIR:-/tmp}/py02-XXXXXX"` 里写东西，`trap cleanup EXIT` 只删那一个目录；
  `--keep` 时不删，改成打一行提示。
- 不写、不改、不删仓库里任何其他文件；`python-handbook/README.md` 的章节地图由主 agent 维护。
- 只用标准库，不 pip、不联网、不改 shell 配置；环境变量只在脚本自己（及其子 shell）里生效。
- 开头 `unset` 掉可能干扰结论的 `PYTHON*` 变量，并固定 `LC_ALL=C.UTF-8` / `PYTHONHASHSEED=0`。
- 全程不打印绝对路径（除随机沙箱名），所以输出里不含用户名。

想只看某一节：`bash demo.sh 2>&1 | awk '/^══════ 7\./,/^══════ 8\./'`（`7.` 换成分节标题里的节号）。

## 下一步

- **第 04 章**（容器与复杂度表）：本章的「list 是指针数组」（第 14 节 `getsizeof` = 104 / 本体 156）
  直接接上「所以切片、拷贝、遍历各是什么代价」，以及 dict 空表 224 字节那条尺子。
- **第 03 章**（字符串与文本）：第 9 节的驻留规则只测了「像不像标识符」这一层，
  编码/`unicodedata` 层面的共用行为不在本章。
- **第 05 章**（函数与坑）：`def f(a=[])` 那个 `[1, 1] [1, 1]`（第 21 节）、
  局部/全局判定的完整规则、闭包与 `nonlocal`（第 19 节只演示了存在）。
- **第 06 章**（类与 dataclass）：类体那层命名空间和方法里为什么看不见它（第 3 节 block 4、第 18 节 block 5）、
  `__slots__` 与 `__dict__`（本章没演）。
- **第 07 章**（异常与 with）：`UnboundLocalError` / `TypeError` / `ValueError` 的继承关系与正确捕获。
- **第 10 章**（类型注解与工具）：`timeit` 量「循环拼字符串很贵」（第 4 节脚本里预告的那条）；
  `mypy` 才是让 `x: int = 1` 有牙齿的东西（第 14 节证明运行时没有牙齿）。
- **GC 阈值与循环引用回收的完整机制**：手册章节地图里没有专章，本章只在第 17 节演到「能关掉、能手动收」。

**本章回指**：01 章场景 2 埋的「全局名要查 dict」在第三节兑现；
01 章索引里点名的「`is` vs `==`」在第七、八节结清。






