# 第 08 章 · 文件与 pathlib：字节、字符、路径，是三件不同的事

> 本章所有输出都是本机 `Python 3.14.4` 真跑出来的，一条都没编、一条都没改。
> 跑法：`cd python-handbook/08-files-and-pathlib && bash demo.sh`（21 节，跑完自动删沙箱）。
> 想自己拨开关：`bash demo.sh --keep`，然后进打印出来的那个 `/tmp/py08-XXXXXX/work` 里接着敲。
>
> 本章的每条命令都长这样：`$ python3 某个文件.py` 或 `$ python3 -c "..."`，
> 全部只读写 `mktemp -d` 出来的沙箱，不碰你的 `~/.config`、不联网、不装包、不开任何监听端口。
> 第 4、18 节用 `tracemalloc` 量内存：**峰值绝对值每跑必抖，所以那两节只印「谁比谁大」这类大小关系，不印字节数**；
> 其它节的输出是确定的 —— 同一份脚本跑两遍，`diff` 出来是空的（第 20 节的对账工具就是这么要求的）。

---

## 第 00 节 · 本章在讲什么，以及为什么把三件事拆开讲

前七章讲的都是**内存里的 Python**：类型、字符串、容器、函数、类、异常、`with`。
这一章跨到进程外面去：**一旦东西要落到磁盘上、要从磁盘上回来，你就同时踩在三套规则上**，
而这三套规则经常被当成一套：

> **一、编码（encoding）管「字节 ↔ 字符」。** 文件里没有字符，只有字节；
> `utf-8` / `gbk` / `latin-1` 是三本不同的翻译词典。翻错了不一定报错 —— `latin-1` 永远翻得动，
> 只是翻出来是乱码。**「没报错」和「读对了」是两件事。**
>
> **二、换行（newline）管「行 ↔ 字符」。** 文本层会偷偷把你写的 `\n` 换成平台的行结束符，
> 也会把读到的 `\r\n` 换成 `\n`。`csv` 要求你写 `newline=""`，不是风格洁癖，是因为它自己已经写了 `\r\n`，
> 再让文本层翻译一次就变成 `\r\r\n`。
>
> **三、路径（pathlib / os.path）管「路径 ↔ 操作」。** 这里有一条最容易混淆的界线：
> `parts / name / suffix / parent` 是**纯字符串运算**，对一条根本不存在的路径照样算得出来、一个异常都不抛；
> `exists / stat / open / resolve` 是**问内核**，路径不存在就当场给你 `FileNotFoundError`。
> 想清楚自己在哪一侧，pathlib 的坑一半就填平了。

这三件事各自成套，所以本章的排版也是三条线：
第 1~7 节走**编码与字节**这条线（模式、缓冲、换行、编码、`errors=`）；
第 8~14 节走**路径与对象**这条线（`Path` 的纯算、和 `os.path` 逐个对照、建目录删文件、遍历与 `stat`、
`read_text`、临时文件、符号链接）；
第 15~18 节走**格式与落盘**这条线（`csv`、`json`、原子写、大文件与 `seek`）；
第 19 节把三条线拧成一个真工具，第 20~21 节是本章自己给自己做的对账与自证。

| 你想搞清楚的东西 | 本章用哪一节回答 | 一句话答案 |
|---|---|---|
| `open()` 那些模式字母到底改了什么 | 第 2 节 | 三件事：能不能读、能不能写、打开那一刻游标在哪 + 要不要截断。`w` 是**打开就截断**，还没等你写，旧内容已经没了 |
| 为什么管道里看不到进度 | 第 3 节 | 接终端是行缓冲、接管道是块缓冲；`flush()` 才是边界，`close()` 才是终点。日志要在管道里实时可见就 `print(..., flush=True)` |
| 不 `close` 会怎样 | 第 4 节 | 引用计数归零时 GC 替你关，但这不是保证；文件对象还是**一次性迭代器**，第二次 `list(f)` 是空的而且不报错 |
| `newline=` 读写两侧各管什么 | 第 5 节 | 读侧决定 `\r\n` 要不要变成 `\n`，写侧决定 `\n` 要不要变成 `os.linesep`；写 `csv` 必须 `newline=""` |
| 不写 `encoding=` 时用的是哪个编码 | 第 6 节 | 来自 `locale.getpreferredencoding()`，不来自你的文本内容；`PYTHONUTF8` / `PYTHONIOENCODING` 能整条改写它（本章开开关关对比过） |
| BOM 为什么会毁掉 `startswith('#')` | 第 6 节 | `utf-8` 读带 BOM 的文件，首字符是 `\ufeff`；读自己写的文本用 `utf-8-sig` 最省事 |
| `errors=` 有哪些结局 | 第 7 节 | `strict` 响、`ignore` 悄悄扔字节、`replace` 换 `\ufffd`、`surrogateescape` 先留着且**写回去还能原样**、`backslashreplace` 改内容但看得见 |
| `Path` 和字符串到底差在哪 | 第 8 节 | 差在「纯算」和「问内核」这条线上：`parts/name/suffix/parent` 对不存在的路径全算得出，`stat()` 当场抛 |
| `os.path` 那一堆函数还要不要学 | 第 9 节 | 要 —— 老代码里全是它。本章把两派逐个并排对照，顺带钉一条必须亲眼看到的规矩：`os.path.join("/a", "/b")` 会把 `/a` 丢掉 |
| 建目录 / 删文件的正确写法 | 第 10 节 | `mkdir(parents=True, exist_ok=True)`、`unlink(missing_ok=True)`；权限那一半最反直觉：**删文件看的是父目录的写权限，不是文件自己的** |
| `glob` / `rglob` 的结果为什么老变 | 第 11 节 | 它不保证顺序（本章全部 `sorted()` 过）、`*` 不匹配点开头的文件、`*` 不跨 `/`；`st_size` 还不等于占盘大小 |
| `read_text()` / `write_text()` 能不能无脑用 | 第 12 节 | 小文件、整份读、整份写才用它；`write_text` 是**截断写**，配置文件不能用它（第 17 节给正确写法） |
| 临时文件为什么不能随手 `open('/tmp/x','w')` | 第 13 节 | 名字可预测 = 可被抢注。`mkstemp` 用 `O_CREAT\|O_EXCL` 且权限 0600，普通 `open` 是 0644 —— 这是安全差异不是风格差异 |
| `resolve()` / `absolute()` / `normpath` 选哪个 | 第 14 节 | `resolve()` 问内核并展开符号链接，`normpath` 只折叠 `..` 的字符串，`absolute()` 只拼 cwd、**不折叠**。相对符号链接相对的是**链接自己所在的目录** |
| `csv` 替我处理了什么 | 第 15 节 | 引号与分隔符（`split(",")` 遇到 `"预算, 超了"` 就切错列）；它不管编码、不管类型，读出来全是 `str` |
| `json` 为什么不保存类型 | 第 16 节 | 它是**值**的格式：`tuple` 回来变 `list`、`set`/`bytes` 当场 `TypeError`、字典的 int 键回来变 str。`JSONDecodeError` 带 `lineno/colno/pos` |
| 「写完」什么时候真的落到磁盘上 | 第 17 节 | `write` 只到用户态缓冲，`flush` 只到内核，`fsync` 才是「请求内核别丢」；配置文件的唯一安全写法是**同目录临时文件 + `os.replace`** |
| 大文件怎么读才不炸内存 | 第 18 节 | 逐行或分块；文本模式的 `seek()` 只认 `tell()` 给你的**不透明 cookie**，随手塞一个字节数就撞原文报错 |
| 这些零件拼起来长什么样 | 第 19 节 | 一个「安全读配置、规范化产出」的小工具，每一行都能回指本章某一节（对照表在它自己最后一段里） |
| 本章的「19 个脚本 / N 段」是谁数的 | 第 20 节 | 一个对账脚本数的，而且它当场**造五个坏样本让五条规矩各响一次** —— 不响的检查等于没检查 |
| 怎么证明这些实验没在你机器上留垃圾 | 第 21 节 | 每条计数都先被**故意弄响一次**再报 0：恒 0 的检查比没有检查更糟 |

**读法建议**：只想知道 `open()` 该写哪些参数，看第 2、5、6、7 节，这四节凑成「读一个文件」的完整参数表；
在做文件管理脚本的人重点看第 8~14 节；
被「进度不刷新」「CSV 多出空行」「配置文件写坏」困扰过的人直接看第 3、15、17 节。
第 1 节是地基（四个「默认编码」+ `umask`），后面第 6、13 节要回来引用它，别跳过。
第 19 节是本章的综合演练，建议读完 1~18 再去看；第 20、21 节是取证纪律本身，
它解释的是「这本书为什么可信」，你可以最后读，但别完全跳过。

**和相邻章节的接缝**：异常的家谱、`with` 协议、`errno` 分派在第 07 章；
本章大量用到「`with` 只保证 `close`，不保证事务」这条边界，并在第 17 节把它做实。
字符串本身的编码细节（`str` 是码点序列、`normalize`）在第 03 章；
子进程与管道在第 11 章。本章只管一件事：**东西怎么出去、怎么回来，中间别坏**。

---

## 第 01 节 · 环境体检：这一章的每一条结论都挂在这几个数字上

文件这一章比前面几章更吃**环境**：`open()` 不写 `encoding=` 时用哪本翻译词典，取决于 locale；
新建文件是什么权限，取决于 `umask`；临时文件落在哪个目录，取决于 `tempfile.gettempdir()`；
而「`\n` 会不会被换成平台行结束符」这件事在这台机器上**根本看不出来** —— 因为 `os.linesep` 就是 `'\n'`。
所以第一步照例把解释器的身份和这几个旋钮钉死。

本章 19 个 `.py`（18 个取证脚本 + 第 20 节那个对账工具）+ `demo.sh`，demo 把它们整份复制进沙箱再跑 ——
章目录里**不会**留下任何产物（第 21 节当场自证）：

```
$ ls
README.md
a_open_modes.py
b_buffering.py
c_close_and_iter.py
d_newline.py
demo.sh
e_encoding_default.py
f_errors_param.py
g_pathlib_basics.py
h_pathlib_vs_ospath.py
i_mkdir_unlink.py
j_glob_stat.py
k_read_write_text.py
l_tempfile_perms.py
m_symlink_resolve.py
n_csv.py
o_json.py
p_atomic_write.py
q_bigfile_seek.py
r_cli_capstone.py
s_checklist.py
[退出码 0]
```

这一列是**字节序**，不是「人以为的顺序」：大写排在前面，所以 `README.md` 在 `a_open_modes.py` 之前；
`d` 那一格里 `d_newline.py` 在 `demo.sh` 之前，因为 `_`（0x5F）比 `e`（0x65）小。
第 11 节 `glob` 的排序讲的是同一件事 —— 列文件时想稳定，就得自己 `sorted()`。

解释器和它报出的自己：

```
$ python3 --version && python3 -c "import sys; print(sys.version)"
Python 3.14.4
3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]
[退出码 0]
```

接下来这一格是本章的地基。四个「默认编码」是四个**职责不同**的东西，只是恰好在这台机器上取值一样，
所以看不出差别的时候最容易被记成一条：

```
$ python3 -c "
import sys, locale, os, stat
print(\"sys.getdefaultencoding()      =\", sys.getdefaultencoding())
print(\"sys.getfilesystemencoding()   =\", sys.getfilesystemencoding())
print(\"locale.getpreferredencoding() =\", locale.getpreferredencoding(False))
print(\"sys.stdout.encoding           =\", sys.stdout.encoding)
print(\"locale.getlocale()            =\", locale.getlocale())
"
sys.getdefaultencoding()      = utf-8
sys.getfilesystemencoding()   = utf-8
locale.getpreferredencoding() = UTF-8
sys.stdout.encoding           = utf-8
locale.getlocale()            = ('C', 'UTF-8')
[退出码 0]
```

- `sys.getdefaultencoding()`：`str` ↔ `bytes` 在**内存里**互相翻译时的默认（`'中文'.encode()` 不带参数走它）。
  它和磁盘上的文件没有直接关系 —— 这是最常被误当成「文件默认编码」的那一个。
- `sys.getfilesystemencoding()`：把**路径字符串**编成字节交给内核时用的编码。第 8~9 节那些带中文的文件名靠它才进得去。
- `locale.getpreferredencoding(False)`：`open()` 不写 `encoding=` 时**真正的来源**。第 6 节整节在拆这一条。
- `sys.stdout.encoding`：`print` 的落地编码，和 `open()` 各走各的 —— 所以「终端里看着正常、写进文件就乱码」
  是完全可能同时发生的两件事。
- `locale.getlocale() = ('C', 'UTF-8')`：这一行说明 demo 已经把语言环境钉成了 `C.UTF-8`
  （这台机器的终端本来是 `LANG=zh_CN.UTF-8`），并且 `unset` 掉 `PYTHONUTF8` / `PYTHONIOENCODING` /
  `PYTHONCOERCECLOCALE` 三个环境变量。上面四个数因此是**纯 locale 推导**出来的，不是环境变量顶上去的 ——
  第 6 节比的就是把它们一个个拨动之后各变成什么。

权限那一维，本章满屏的 `0644` / `0755` / `0600` 都挂在这一行上：

```
$ um=$(umask); printf "umask=%s ｜ 新建文件默认 %04o ｜ 新建目录默认 %04o\n" "$um" "$((0666 & ~(8#$um)))" "$((0777 & ~(8#$um)))"
umask=0022 ｜ 新建文件默认 0644 ｜ 新建目录默认 0755
[退出码 0]
```

`umask=0022` 的意思是「新建文件谁都能读、只有你能写；新建目录谁都能进、只有你能写」。
第 13 节要拿它当对照组：`tempfile.mkstemp()` 建出来是 `0600`，**不是** `0644` ——
它不接受 umask 施舍的那点读权限，这是安全差异，不是风格差异。

临时文件落在哪：

```
$ python3 -c "import tempfile, os; p=tempfile.gettempdir(); print(\"tempfile.gettempdir() =\", p); print(\"是 /tmp 吗 →\", p == \"/tmp\")"
tempfile.gettempdir() = /tmp
是 /tmp 吗 → True
[退出码 0]
```

`/tmp` 是全机共享、别人也能在里面建名字的目录。这正是「用可预测的文件名（`/tmp/myapp.lock`）当临时文件」
会被抢注的那个场景 —— 第 13 节的 `O_CREAT|O_EXCL` 就为此而生。

行结束符这一维在 Linux 上看着像废话，但它决定了第 5 节的实验怎么设计：

```
$ python3 -c "import os, sys; print(\"os.linesep =\", repr(os.linesep), \"｜ 平台 =\", sys.platform)"
os.linesep = '\n' ｜ 平台 = linux
[退出码 0]
```

`os.linesep` 就是 `'\n'`，所以「文本模式写侧把 `\n` 换成平台行结束符」这条规矩在这台机器上**换不出任何差别**：
想亲眼看到 `\r\n`，必须显式写 `newline='\r\n'`（第 5 节和第 15 节的 `csv` 都这么做）。
反过来说，同一份脚本在 Windows 上写出的字节和这里不一样 —— 那是本机没法验证的另一半，章里如实标注。

最后确认本章没有任何第三方依赖：

```
$ python3 -c "
import sys
need = {\"pathlib\", \"csv\", \"json\", \"tempfile\", \"shutil\", \"stat\", \"os\", \"tracemalloc\", \"warnings\", \"pty\"}
print(\"本章用到的模块全在标准库里 →\", need <= sys.stdlib_module_names)
print(\"缺的：\", sorted(need - sys.stdlib_module_names) or \"无\")
"
本章用到的模块全在标准库里 → True
缺的： 无
[退出码 0]
```

`sys.stdlib_module_names` 是解释器自己带的一份名单，拿集合做 `<=` 就是「这些名字全在标准库里吗」的一句话答案。
本章不 `pip install`、不联网、不开端口，所有读写都在 `mktemp -d` 的沙箱里，跑完由脚本自己删干净（第 21 节）。

**这一节带走：**

- 四个「默认编码」各管一件事：内存翻译 / 路径字节 / `open()` 的默认 / `print` 的输出。
  这台机器上它们取值都是 utf-8，但**取值相同不等于职责相同**。
- `open()` 的默认编码来自 `locale.getpreferredencoding()`，不来自文件内容；换 locale 或设 `PYTHONUTF8` 就换答案。
- 本章所有权限数字都读自 `umask=0022` 这台机器；换了 umask，`0644` 那一列要跟着重算，`mkstemp` 的 `0600` 不用。
- `os.linesep = '\n'` 意味着「换行翻译」在本机不可观测，第 5 节靠显式 `newline=` 才把它逼出来。
- 结论的适用范围写死在本节：`Python 3.14.4` + Linux。这不是脚注，是这一章的一部分。

---

## 第 02 节 · `open()` 的模式字符到底改了哪三件事

模式串看着像一个权限申请：要读写 `r`，要改写 `w`，要追加写 `a`。真跑一遍会发现它同时改了三件**互相独立**的事：

1. 这个句柄**能不能读、能不能写**（`+` 管这一维）；
2. `open()` 返回的那一刻**游标停在哪儿**（`a` 和别的模式不一样）；
3. **要不要动旧内容**（`w` 会动，而且动在 `open()` 里；`x` 干脆拒绝打开）。

再加第四维：读写的**单位**是字节还是字符（`b` 管这一维）。这四件事谁也不替谁担保，所以背公式没用，得一格一格撞。

先把全景表摆出来。实验设计很朴素：每个模式都先造一份**同样的** 72 字节内容，只用这个模式 `open()`，然后什么都不写就关掉，再量「旧内容还剩多少」。

```
$ python3 a_open_modes.py
=== 1. 模式全景表：每个格子都是拿真文件撞出来的 ===
  初始内容统一是同一份记账文本 = 72 字节（'旧的记账内容…' × 4 轮）：
  模式   可读  可写  打开后 tell()  只 open 不写就 close：旧内容剩多少   自报(读,写)
  r      ✓     ✗     0              72 字节                      (True, False)
  r+     ✓     ✓     0              72 字节                      (True, True)
  w      ✗     ✓     0              0 字节                       (False, True)
  w+     ✓     ✓     0              0 字节                       (True, True)
  a      ✗     ✓     72             72 字节                      (False, True)
  a+     ✓     ✓     72             72 字节                      (True, True)
  x      ✗     ✗     —              72 字节（不让打开）                —
```

三个能当场读出来的结论：

- `r` 和 `r+` 的游标起点都是 0，旧内容都剩 72 字节 —— 这两个模式的区别**只在能不能写**，跟位置无关。
- `a` / `a+` 一打开 `tell()` 就是 72，也就是**文件末尾**；`r`/`w` 那一列是 0。
- 最后一列是句柄自己报的 `(readable(), writable())`。`w` 是 `(False, True)`、`a` 是 `(False, True)`：想知道手里的句柄能干什么，问这两个方法，别拿模式串猜。

### `w` 的截断发生在你想清楚之前

这是全章最贵的一条。脚本分三步演：

```
=== 2. ★本章最贵的一条：'w' 在 open() 那一刻就截断，不等 write ===
  ① 先写一份账本：72 字节，第一行 = '旧的记账内容旧的记账内容旧的记账内容旧的记账内容'
  ② 只执行 open(p,'w')，一个字节都还没写：磁盘上剩 0 字节（fh.closed=False）
  ③ 立刻 close（全程没 write）：0 字节，读回来 = ''
  → 截断发生在 open() 返回**之前**。等你想清楚「还是别写了」，旧内容已经没了，
    而且没有任何一条报错提醒你 —— 第 07 章第 16 节说的「不报错的错」就是这个形状。
  每个模式各开一次、都一个字不写就关掉，看谁真的动了磁盘：
    'r ' 72 → 72 字节 ｜ 旧内容没动
    'r+' 72 → 72 字节 ｜ 旧内容没动
    'w ' 72 → 0 字节 ｜ ★当场截断
    'w+' 72 → 0 字节 ｜ ★当场截断
    'a ' 72 → 72 字节 ｜ 旧内容没动
    'a+' 72 → 72 字节 ｜ 旧内容没动
    'x ' 72 → 72 字节 ｜ 拒绝打开，旧内容原封不动
  → 掉到 0 的只有 'w' 和 'w+'；'x' 是直接拒绝打开。这就是「宁可失败也不覆盖」的写法。
  ★ 想让「打开即截断」不发生只有两条路：用 'x'，或者先 'r' 读进来、决定要写了再 'w'。
```

关键在于第 ② 步和后面那七行逐模式实测（`'w ' 72 → 0 字节 ｜ ★当场截断` 那一格）：**「只 `open` 不写」这件事本身就是一次破坏**。`with open(p, 'w') as fh:` 这行执行完，旧内容已经是 0 字节了；接下来就算你在块里反悔、一个字都不写、甚至直接 `raise`，那份账本也回不来 —— 而且全程没有一条报错提醒你。第 07 章讲的「不报错的错」，这是最典型的一个形状。

掉到 0 的只有 `w` 和 `w+`，`a` / `a+` / `r` / `r+` 都原封不动，`x` 直接拒绝打开（`72 字节（不让打开）`）。所以「存在就别碰」没有第二种写法：要么用 `x`，要么先 `r` 读进来、真决定要写了再换 `w`。

### 三种「打不开」的原文长什么样

```
=== 4. 三种「打不开」的报错原文：一个字都别改地抄下来 ===
  'r' 对不存在的文件 → FileNotFoundError: [Errno 2] No such file or directory: 'open-modes-demo/查无此文件.txt'
  'w' 对不存在的父目录 → FileNotFoundError: [Errno 2] No such file or directory: 'open-modes-demo/没有的目录/子/x.txt'
  'x' 对已存在的文件 → FileExistsError: [Errno 17] File exists: 'open-modes-demo/已存在.txt'
  'xb' 对已存在的文件 → FileExistsError: [Errno 17] File exists: 'open-modes-demo/已存在.txt'
  'x+' 对已存在的文件 → FileExistsError: [Errno 17] File exists: 'open-modes-demo/已存在.txt'
  → 前两条是同一个类同一条 strerror（只差路径），因为内核只知道「这一级路径不存在」；
    父目录不存在时 'w'/'a'/'x'/'r' 全给 FileNotFoundError —— 建目录不归模式串管，
    归 os.makedirs，本章第 10 节。这条错最容易被误读成「没权限」。
  → 看 errno 别看中文语感：No such file or directory = 2，File exists = 17；
    两者都是 OSError 的儿子（第 07 章第 3 节那棵树），except OSError 一网能打尽。
```

这段值得逐字看一遍，因为它纠正一个很普遍的误读：

- `open(p, 'w')` 失败**不一定是文件的错**，也可能是父目录不存在 —— 报的还是 `FileNotFoundError: [Errno 2]`，跟「文件不存在」一模一样。`w` / `a` / `x` 都不会替你把目录建出来，建目录是 `os.makedirs` 的事（本章第 10 节）。
- `x` 那三条（`x` / `xb` / `x+`）报的是 `FileExistsError: [Errno 17] File exists`。同一个「不允许」，errno 是 17 不是 2。
- 两条 `Errno 2`、两条 `Errno 17` 都是 `OSError` 的儿子，所以 `except OSError` 一网能打尽；要分流就按 `errno` 分派，别按报错的中文语感分派。

### `b` 换的是单位，不是编码

```
=== 5. 文本还是二进制：write 收什么、read 给什么 ===
  'w' + write('第一行\n') → 成功，write 返回 4（文本模式的计数单位是**字符**）；
      磁盘上实际是 10 字节（4 个汉字 × 3 + 换行 1）。
  'w' + write(str) → ✓
  'wb' + write('中文') → ✗ TypeError: a bytes-like object is required, not 'str'
  'r' read(3) → str：'第一行'（3 个**字符**）
  'rb' read(3) → bytes：b'\xe7\xac\xac'（3 个**字节** = 刚好一个「第」字）
  'rb' read(2).decode('utf-8') → UnicodeDecodeError: 'utf-8' codec can't decode bytes in position 0-1: unexpected end of data
  → 字节、字符、路径是三件不同的事：'b' 换的是**单位**，不是「换个编码读」的开关；
    没有 'b' 才有 encoding= 可谈。截在半个字上，就是上面那条 UnicodeDecodeError。
  → 'wb' 这类二进制句柄的 tell()/seek() 单位、以及能不能 seek 到任意字节，见本章第 18 节。
```

这一格是「字节 ↔ 字符」这条线的起点，两个数字必须记住：

- 文本模式 `write('第一行\n')` 返回 **4**，磁盘上却是 **10** 字节（4 个汉字各 3 字节，加一个换行）。`write` 的返回值从来不是字节数。
- 同一个 `read(3)`：文本层给你 3 个**字符**（`'第一行'`），二进制层给你 3 个**字节**（`b'\xe7\xac\xac'`，刚好一个「第」字）。二进制层照着字节数切，切在半个字上，`read(2).decode('utf-8')` 就当场抛 `UnicodeDecodeError`，消息尾巴是 `unexpected end of data`。

所以 `b` 不是「换个编码来读」的开关，它换的是**计量单位**；只有不带 `b` 的时候，`encoding=` 才有意义。而 `encoding=` 和 `errors=` 这两个文本层旋钮，分别在第 06 节和第 07 节撞开。

**这一节带走：**

- 模式串四个维度各自独立：`+` 加方向、`b` 换单位、`a` 改起点、`w` 加截断。别把「`a` 不能读」和「`a` 起点在末尾」混成一条。
- `w` / `w+` 的截断在 `open()` 返回之前完成（本机实测 72 字节 → 0 字节）。写之前先确认自己是不是在拿 `w` 覆盖别人的数据。
- `x` 是唯一「已存在就拒绝打开」的模式，且拒绝时一个字节都没碰（旧内容仍 72 字节）。
- 报错按 `errno` 读：2 = 路径不存在，17 = 已存在；两者同属 `OSError`。
- `readable()` / `writable()` 是句柄自报的，比模式串可靠；`write` 的返回值在文本层是字符数。

---

## 第 03 节 · 缓冲：为什么管道里看不到进度

同一份 `print` 循环，在自己的终端里一行一行往外冒，接到管道、`tee`、CI 日志里就一声不吭，等进程退出才一口气涌出来。代码没变，变的是 `sys.stdout` 那个 fd 接到的是什么。这一节用 `pty` 在本机造了个假终端，把这件事量出来——不联网、不起交互 shell。

```
$ python3 b_buffering.py
=== 1. 同一段 print 循环：接伪终端 / 接管道 / 接管道但 flush=True ===
  子进程跑的源码就这几行（三个格子共用，第 3 格只多一个 flush=True）：
      import os, sys, time
      FLUSH = sys.argv[2] == 'flush'
      for i in range(20):
          print(i, flush=FLUSH)
      ready, go = sys.argv[1], sys.argv[3]
  采样方式：等子进程留下「打印完了」的哨兵，再 select 0.3 秒一次；此时子进程一定还没退出。
  伪终端 pty ｜ 打印循环跑完了吗 → True ｜ 采样时子进程还活着 → True ｜ 退出前就看得见输出吗 → True
  退出前读到的量 → 有 ｜ 退出时才涌出来的量 → 0 条 ｜ 子进程自报：isatty=True line_buffering=True 二进制层=BufferedWriter
  管道 PIPE ｜ 打印循环跑完了吗 → True ｜ 采样时子进程还活着 → True ｜ 退出前就看得见输出吗 → False
  退出前读到的量 → 0 条 ｜ 退出时才涌出来的量 → 有 ｜ 子进程自报：isatty=False line_buffering=False 二进制层=BufferedWriter
  管道 PIPE + flush=True ｜ 打印循环跑完了吗 → True ｜ 采样时子进程还活着 → True ｜ 退出前就看得见输出吗 → True
  退出前读到的量 → 有 ｜ 退出时才涌出来的量 → 0 条 ｜ 子进程自报：isatty=False line_buffering=False 二进制层=BufferedWriter
  ★ 屏幕上没有精确字节数：tty 那一侧一次给一行还是两行本来就不固定，写死进 README 就会假。
  → 判据（本机实测）：接 tty 退出前 True、接管道退出前 False、加 flush=True 退出前 True。
    同一段代码、同一个解释器，差别只在 stdout 接到的是什么 fd —— 这就是 tail -f 看不到进度的全部原因。
  → 附带一条：tty 那次的字节里带 \r 吗 → True（终端行尾翻译加的，管道那次是 False）。换行翻译归本章第 5 节，这里只登记它存在。
```

三个格子的差别只在「stdout 那头是谁」：

- 接伪终端：`isatty=True`，`line_buffering=True`，退出前就看得见输出。
- 接管道：`isatty=False`，`line_buffering=False`，退出前**一条都读不到**，全部攒到进程退出才涌出。
- 接管道 + `flush=True`：`isatty` 还是 `False`、行缓冲还是 `False`，但退出前就看得见了 —— 因为你每次写都自己把边界画出来了。

注意第二列那个「二进制层 = BufferedWriter」在三格里都一样：行缓冲是**文本层**的开关，底下的二进制层始终是块缓冲。屏幕上（`★` 那一行）也故意不打精确字节数，因为 tty 那边一次给你一行还是两行本来就不固定，写死进书里就会变成假数据。附带登记一条：pty 那次的字节里带 `\r`（终端的行尾翻译加的），换行这件事归第 05 节。

### 三个旋钮分别拿到什么对象

```
=== 2. 三个缓冲旋钮：buffering=0 / 1 / -1 分别拿到什么对象 ===
  io.DEFAULT_BUFFER_SIZE = 131072（二进制层的默认块，本机实测的常量）
  open(mode='r', buffering=-1) → TextIOWrapper → BufferedReader ｜ f.name='buffering-demo/旋钮.txt' ｜ f.buffer.name='buffering-demo/旋钮.txt' ｜ 没有公开 buffer_size 属性 → True
  open(mode='r', buffering= 1) → TextIOWrapper → BufferedReader ｜ f.name='buffering-demo/旋钮.txt' ｜ f.buffer.name='buffering-demo/旋钮.txt' ｜ 没有公开 buffer_size 属性 → True
  open(mode='rb', buffering=-1) → BufferedReader ｜ f.name='buffering-demo/旋钮.txt' ｜ f.buffer.name='没有 buffer 这一层' ｜ 没有公开 buffer_size 属性 → True
  open(mode='rb', buffering= 0) → FileIO ｜ f.name='buffering-demo/旋钮.txt' ｜ f.buffer.name='没有 buffer 这一层' ｜ 没有公开 buffer_size 属性 → True
  open(mode='rb', buffering= 8) → BufferedReader ｜ f.name='buffering-demo/旋钮.txt' ｜ f.buffer.name='没有 buffer 这一层' ｜ 没有公开 buffer_size 属性 → True
  open(mode='wb', buffering=-1) → BufferedWriter ｜ f.name='buffering-demo/旋钮.txt' ｜ f.buffer.name='没有 buffer 这一层' ｜ 没有公开 buffer_size 属性 → True
  open(mode='wb', buffering= 8) → BufferedWriter ｜ f.name='buffering-demo/旋钮.txt' ｜ f.buffer.name='没有 buffer 这一层' ｜ 没有公开 buffer_size 属性 → True
  open(mode='a', buffering= 1) → TextIOWrapper → BufferedWriter ｜ f.name='buffering-demo/旋钮.txt' ｜ f.buffer.name='buffering-demo/旋钮.txt' ｜ 没有公开 buffer_size 属性 → True
  open(mode='r', buffering=0) → ValueError: can't have unbuffered text I/O
  → buffering=0 只许用在二进制模式：文本层必须缓存，它得攒够一个完整的字符才敢解码。
  → buffering=1 的那个「1」不是 1 字节，是「行缓冲」开关；文本层底下的 f.buffer 仍是默认块缓冲。
  缓冲区多大没法问属性，只能量行为 —— 把 buffering 设成 8，一段一段看它什么时候才交：
    累计写了  5 字节 → 另一个 open() 只看到 0 字节（差额 5 字节躺在用户态缓冲区里）
    累计写了  8 字节 → 另一个 open() 只看到 0 字节（差额 8 字节躺在用户态缓冲区里）
    累计写了  9 字节 → 另一个 open() 只看到 8 字节（差额 1 字节躺在用户态缓冲区里）
    close() 之后盘上 9 字节 = tell（close 里含 flush，这才是 with 保证的那件事）
  → 攒满 8 字节那次都没往下写，第 9 个字节才把前 8 个推下去：边界是缓冲区，不是「我写完了」。
```

- `buffering=0` 只许用在二进制模式。文本模式给 0 立刻 `ValueError: can't have unbuffered text I/O` —— 因为文本层必须攒够一个完整的字符才敢解码（下面第一条 `→` 说的就是这个）。
- `buffering=1` 的那个「1」不是 1 字节，是「行缓冲」开关：文本模式给 1，拿到的还是 `TextIOWrapper`，底下的 `f.buffer` 仍是 `BufferedReader`。
- 默认 `-1` 就是块缓冲，块大小是 `io.DEFAULT_BUFFER_SIZE = 131072`。
- 缓冲区多大**没法问属性**（上面表里那一整列都写着「没有公开 buffer_size 属性 → True」），只能量行为：围栏最后那段把 `buffering` 设成 8，写到 5 字节、写到 8 字节，另一个 `open()` 都看到 0 字节；写到**第 9 个字节**才把前 8 个推下去。边界是缓冲区满不满，跟「我这一句 write 写完了」没有半点关系。

### 文本层的两个开关：`line_buffering` 与 `write_through`

```
=== 3. 文本层的两个开关：line_buffering 与 write_through ===
  open(p,'w') → line_buffering=False write_through=False
    写完这两行（第二行不带换行）的那一刻盘上 0 字节 ｜ close 后 19 字节
  open(p,'w', buffering=1) → line_buffering=True write_through=False
    写完这两行（第二行不带换行）的那一刻盘上 19 字节 ｜ close 后 19 字节
  TextIOWrapper(FileIO, write_through=True) → line_buffering=False write_through=True
    写完这两行（第二行不带换行）的那一刻盘上 19 字节 ｜ close 后 19 字节
  二进制层对象 BufferedReader 有这两个开关吗 → False / False（它只认 flush）
  → line_buffering=True 那次连没带换行的「第二行」也一起到了盘上：它遇到换行是**整块**推下去，
    不是只推到换行为止。write_through=True 更干脆：文本层不存货，每次 write 都直接交给底层。
```

这里有个反直觉的细节：`line_buffering=True` 那次，盘上落的是 **19 字节**，也就是连**没带换行的第二行**一起落下去了。行缓冲的含义是「遇到换行就整块推下去」，不是「只推到换行为止」。`write_through=True` 更干脆：文本层不存货，每次 `write` 直接交给底层。二进制层对象这两个开关都没有，它只认 `flush()`。

### `flush` 才是边界，`close` 才是终点

```
=== 4. flush 才是边界：写完不 close 不 flush，另一个 open() 读到什么 ===
  写了 12 个字符（36 字节的量），没 flush 没 close，fh.closed=False
    此时另一个 open(p) 读到的字符数 = 0 ｜ 磁盘字节数 = 0
    fh.flush() 之后读到的字符数 = 12 ｜ 磁盘字节数 = 36
    fh.close() 之后再读 = 12 个字符 ｜ 磁盘 36 字节
  顺带一条本机实测的坑：文本模式里 f.tell() 自己就会 flush —— 问之前磁盘 0 字节，问完变 36 字节
  → write 只到用户态缓冲区：同一进程里另一句 open() 都看不见你写的东西，更别说别的进程和 tail -f；
    而「我先 tell 一下看写了多少」会顺手把这条边界抹掉 —— 上面那行就是证据，别把它当玄学。
  ★ flush 也只是「交给操作系统」，不是「落到磁盘」；断电不丢要 fsync，那是本章第 17 节。
```

写了 12 个字符（36 字节的量），既不 `flush` 也不 `close` 的时候，**同一个进程里另开一句 `open(p)` 都读到 0 字节** —— 更别说别的进程、`tail -f`、或者监控脚本。`flush()` 之后立刻变成 12 个字符 / 36 字节。围栏里那句「顺带一条本机实测的坑」是同一台机器上量到的：文本模式里 `f.tell()` 自己就会 flush —— 问之前磁盘 0 字节，问完变 36 字节 —— 所以「我先 tell 一下看写了多少」会顺手把这条边界抹掉，把你的实验做脏。

还要划清一层：`flush` 只是「交给操作系统」，不是「落到磁盘」。断电不丢要 `fsync`，那是本章第 17 节的事。

**这一节带走：**

- 可见性不是程序的性质，是它和 stdout 之间那根 fd 的性质（tty 行缓冲 / 管道块缓冲，实测 True 与 False）。
- 日志、进度条、CI 输出想要实时可见，就 `print(..., flush=True)` 或 `sys.stdout.flush()`。
- `write` 只到用户态缓冲区，`flush` 到内核，`close` 交出 fd 并顺带 flush —— 这正是 `with` 保证的那件事。
- 旋钮只有三个：默认块 131072、`buffering=1` 换行缓冲、`buffering=0` 只许二进制。
- 文本模式里 `tell()` 会顺手 flush，做「谁什么时候看得见」的实验时别踩它。

---

## 第 04 节 · `close`、一次性的文件迭代器、四种读法的内存账

这一节回答三个经常被合并成一个的问题：句柄归谁关？`for line in f` 为什么第二次是空的？`read` / `readlines` / 逐行 / 分块各花多少内存？

### `f.closed` 是唯一可信的字段

```
$ python3 c_close_and_iter.py
=== 1. f.closed 的三个时刻，和关完之后每一句话的报错原文 ===
  ① open 之后：f.closed=False ｜ 拿这个号问内核还开着吗 → True
  ② 自己 close() 之后：f.closed=True ｜ 同一个号问内核 → False
  再 close 一次 → 不报错吗 True（close 幂等，重复关不炸；f.closed 仍是 True）
  关完再碰它，每个方法的原文一个字符都没改（二进制那两行用的就是上面刚建好又刚关掉的那个句柄）：
    文本 read()    → ValueError: I/O operation on closed file.
    文本 write()   → ValueError: I/O operation on closed file.
    文本 tell()    → ValueError: I/O operation on closed file.
    文本 seek(0)   → ValueError: I/O operation on closed file.
    文本 flush()   → ValueError: I/O operation on closed file.
    文本 fileno()  → ValueError: I/O operation on closed file
    二进制 read(1)   → ValueError: read of closed file
    二进制 close()   → 居然没报错 → None
  ★ 那个二进制句柄拿到的号，就是刚被关掉的旧号 → True；拿旧号去 fstat，量到的是**新**文件的
     字节数 → True。号是内核的整数，用完立刻复用，别存下来当「我还开着」的证据。
  ③ with 退出之后：块体内 closed=False → 块体外 closed=True ｜ 同一个号问内核 → False
  → 唯一可信的字段是 f.closed：文本层五句共用一条消息、fileno() 只少一个句号，二进制层按方法各写一条 → 接异常请写 except ValueError，别去匹配消息字符串。
  → close 幂等 + 关完再碰才炸，就是第 07 章第 10 节「__exit__ 重复调用也安全」的实测版。
```

三件事：`close` 是**幂等**的（再关一次不报错），关完之后**碰它才炸**，而且炸出来的消息文本层与二进制层不一样 —— 文本层那五句共用 `I/O operation on closed file.`，`fileno()` 只少一个句号，二进制层按方法各写一条（`read of closed file`）。所以接异常请写 `except ValueError`，别去匹配消息字符串。

★ 那条更阴：那个二进制句柄拿到的号，正好是刚被关掉的旧号 —— 拿旧号去 `fstat` 照样问得出结果，量到的却是**另一个新文件**的字节数。号是内核的整数，用完立刻复用，绝不能存下来当「我还开着」的证据。

### 你没写 `close` 的时候，是谁在关

```
=== 2. ★你没写 close 的时候是谁在关：引用计数 / 循环引用 / 被异常扣住 / with ===
  A 只 del 名字、一次 close 都没写 → fd 还开着吗 False（是 dealloc 替你关的）；屏幕上有过任何提示吗 → False（默认过滤级别对 ResourceWarning 是 ignore）
     把过滤拨到 always 才看得见这一条：["ResourceWarning: unclosed file <_io.TextIOWrapper name='close-iter-demo/账本.txt' mode='r' encoding='utf-8'>"]
  B 函数返回、局部句柄出作用域（全程没 close）→ fd 还开着吗 False
  C 异常对象被我存下来 → 它那一帧里的句柄还开着吗 True；放掉异常之后 False
  D 句柄被循环引用扣住：del 之后 fd 还开着吗 True → gc.collect() 之后 False
  → A/B 那种「忘了 close 也没出事」，是 CPython 引用计数恰好补了刀：C 的异常把整帧扣住、D 的环
    要等分代回收、模块级名字更不会归零 —— 这三种形状下 fd 真的在漏，不是保证。
  with 的三条合同逐条实测（第 07 章第 10 节的协议落在文件上）：fh.__enter__() is fh → True；fh.__exit__(None, None, None) → 返回 None（假值 = 不吞异常）｜ 此时 closed=True
     块体抛异常，外面接住：RuntimeError: 块体里崩了
     异常路径上 wf.closed → True ｜ 磁盘上留下 12 字节：'写了一半'
  → with 只保证 close（close 里含 flush），不保证写完、更不保证落盘：半截文件不违反合同；要「要么
    全有要么全无」得走临时文件 + os.replace，本章第 17 节。
```

A 和 B 是「忘了 close 也没出事」的两种形状 —— 那是 CPython 引用计数替你补了一刀，不是保证。C（异常对象被存下来，它那一帧里的句柄跟着活）和 D（句柄被循环引用扣住，要等分代回收）这两种形状下 **fd 真的在漏**。而且屏幕上一个字都不会提示：A 那一格「屏幕上有过任何提示吗 → False」，`ResourceWarning` 在默认过滤级别是被忽略的，拨到 `always` 才看得见那条原文。

围栏最后几行是 `with` 的三条合同逐条实测：`__enter__` 返回自己、`__exit__` 返回假值（不吞异常）、块体抛异常照样关。但注意「异常路径上」那一行：`wf.closed → True` 的同时磁盘上留下 12 字节的 `'写了一半'` —— **`with` 只保证 close，不保证写完、更不保证落盘**。半截文件不违反合同，要「要么全有要么全无」得走临时文件 + `os.replace`（第 17 节）。

### 文件对象就是它自己的迭代器

```
=== 3. ★文件对象就是它自己的迭代器：第二次 list(f) 是空的，而且不报错 ===
  现场：close-iter-demo/五行.txt = 35 字节 / 5 行。第一次 list(f) 拿到 5 行（首行 'line-0\n'）
  读完这一刻 tell()=35，正好等于文件字节数 → True
  同一个句柄第二次 list(f) → []，长度 0，全程没接任何异常 ← ★这一格
  现场恢复：seek(0) 返回 0（新的绝对位置），重扫一遍又拿到 5 行 → 游标真被挪回开头
  它是迭代器的三条硬证：iter(f) is f → True ｜ type(iter(f)).__name__ → TextIOWrapper ｜ hasattr(f, '__len__') → False
  readlines() 给的是容器：type=list、5 个元素，一次抱完；f 自己不是容器（__len__ 那格 False），它只在你要求时才往前走一步 —— 两本账的差在第 4 段量
  游标只有一根，readline 和 list 共用：先 readline 拿走 'line-0\n'，接着 list 只剩 4 行（'line-1\n' 开头）
  只迭代过一行就问 tell() → OSError: telling position disabled by next() call；seek(0) 之后再问 → 居然没报错 → 0（一次 seek 拿回许可）
  → 拒答是因为底层已经替你读过头：拿到第 1 行后 f.buffer.raw.tell()=35，整个文件才 35 字节
  → 「配置文件读两遍」的翻车形状：第二遍得到 []、长度 0、不报错、循环一次都不进，程序带着空配置
    跑到底。要重读要么 seek(0)，要么第一遍就 list 住 —— 那就付 readlines 的账。
```

这是本章最容易静默翻车的一格。35 字节 / 5 行的文件，第一次 `list(f)` 拿到 5 行；同一个句柄第二次 `list(f)` 拿到 `[]`，长度 0，**全程没有任何异常**。三条硬证是 `iter(f) is f → True`、`type(iter(f)).__name__` 还是 `TextIOWrapper`、`hasattr(f, '__len__') → False`。`readlines()` 给的是容器（`type=list`、5 个元素一次抱完），`f` 自己不是容器 —— 两本账的差在第 4 段量。

「游标只有一根」那两行再补两刀：`readline` 和 `for` / `list` 共用同一根游标，先 `readline` 拿走 `'line-0\n'` 之后 `list` 只剩 4 行；而且只迭代过一行就问 `tell()`，会得到 `OSError: telling position disabled by next() call` —— 因为底层已经替你整个读过头了（`f.buffer.raw.tell()` 已经是 35）。一次 `seek(0)` 就能拿回许可。

落到现实里就是「配置文件读两遍」：第二遍得到 `[]`、不报错、循环一次都不进，程序带着**空配置**跑到底。要重读要么 `seek(0)`，要么第一遍就 `list` 住 —— 后者就是 `readlines` 的账。

### 四种读法：只印大小关系，不印绝对值

```
=== 4. 四种读法的内存账：只印大小关系，不印任何绝对值 ===
  现造样本：一万行.txt = 630000 字节 / 10000 行 / 每行 63 字节（本段唯一的确定数字）
  read()             峰值 ≥ 文件大小 → True ｜ 峰值 < 文件大小 → False
  readlines()        峰值 ≥ 文件大小 → True ｜ 峰值 < 文件大小 → False
  for line in f      峰值 ≥ 文件大小 → False ｜ 峰值 < 文件大小 → True
  read(8192) 分块      峰值 ≥ 文件大小 → False ｜ 峰值 < 文件大小 → True
  for 但用列表留住         峰值 ≥ 文件大小 → True ｜ 峰值 < 文件大小 → False
  两两比：read > 逐行 → True ｜ readlines > 逐行 → True ｜ read > 分块 → True ｜ 留住 > 不留 → True
  顺手拆一条传闻：readlines 峰值 > read 峰值 → False（本机同档，都不在「省」那一侧）
  内存绝对值每跑必抖，本节不拿它当证据 —— 只读方向
  → 分档只看一条线：读完就丢（逐行 / 分块）是常数级，整块抱住（read / readlines）在文件量级之上。
  ★ 「逐行省内存」的前提是你**不留它**：最后一档 [line for line in f] 和 readlines() 同档（「留住 > 不留」那一格实测），写列表推导的那一刻账已经付掉了。
```

样本是现造的 630000 字节 / 10000 行 / 每行 63 字节。这一段的输出形式值得学：**内存峰值的绝对值每跑必抖，所以脚本一个字节数都不印，只印「谁比谁大」**。方向就一条线 —— 读完就丢（逐行、分块）是常数级，整块抱住（`read`、`readlines`）在文件量级之上。

最狠的是最后一档和 `★` 那句：`[line for line in f]` 和 `readlines()` **同档**（「留住 > 不留」那一格实测为 `True`）。所谓「逐行省内存」的前提是你不留它；写下列表推导的那一刻，账已经付掉了。顺手还拆了一条传闻：`readlines 峰值 > read 峰值 → False`，本机同档，两个都不在「省」那一侧。

**这一节带走：**

- `close` 只做三件事：flush、释放 fd、幂等。`f.closed` 可信，存下来的 fd 号不可信。
- 「没 close 也没事」是引用计数的巧合；异常帧、循环引用、模块级名字这三种形状下 fd 真漏。`with` 不是风格，是唯一的合同，而合同的边界只到 close。
- 文件对象是一次性迭代器：第二次 `list(f)` 是空的而且不报错。重读用 `seek(0)`，或者第一遍就 `list` 住（然后付 `readlines` 的账）。
- 读法分两档：逐行 / 分块是常数级，`read` / `readlines` 在文件量级之上；逐行但要留住 = `readlines`。

---

## 第 05 节 · `newline=` 读写两侧各管什么

「一行」这个词有两个意思：**磁盘上的字节边界**，和**字符串里的 `\n`**。文本模式会在两者之间做一次翻译，`newline=` 决定谁向谁翻译、翻不翻。读写两侧的规矩还不一样，所以这一节先钉字节，再分别量读侧和写侧。

### 第一步：先把三份真实字节钉在磁盘上

```
$ python3 d_newline.py
=== 1. 先把三份真实字节形状钉在磁盘上：wb 不过文本层，写下去就是字节 ===
  LF    写入 b'a\nb\n'      hex=610a620a       磁盘字节数=4
        读回逐字节相同 → True ｜ 这一份的来历：Unix / 现代文本，行边界是 \n
  CRLF  写入 b'a\r\nb\r\n'  hex=610d0a620d0a   磁盘字节数=6
        读回逐字节相同 → True ｜ 这一份的来历：Windows / Excel 导出的 CSV，行边界是 \r\n
  CR    写入 b'a\rb\r'      hex=610d620d       磁盘字节数=4
        读回逐字节相同 → True ｜ 这一份的来历：老 Mac（System 9 之前），行边界是 \r
  为什么第一步必须用 wb：文本模式的写侧也会翻译一次，字节形状就不由你定了。
  对照：write_text('a\r\nb\r\n')（默认 newline=None）本机磁盘 = b'a\r\nb\r\n'
  → 本机 os.linesep='\n'，只翻译 \n，所以这一格恰好没变；
    在 os.linesep='\r\n' 的机器上它会变成 'a\r\r\nb\r\r\n'（本机未验证，别当事实引用）。
  ★ 结论：想「钉住字节」就只有 wb / read_bytes 一条路，t 模式的字节形状是算出来的。
```

必须先 `wb`，因为文本模式的写侧也会翻译一次，字节形状就不由你定了。三份的来历也标清楚了：`\n` 是 Unix / 现代文本，`\r\n` 是 Windows 与 Excel 导出的 CSV，`\r` 是老 Mac。

### 读侧第一层：整份 `read()` 时，只有 `None` 会改内容

```
=== 2. 读侧 3×5 表：同一份字节，newline 五种取值各读回什么字符串 ===
  （看 repr 就知道 \r 有没有被翻译掉。）
  最后一列是 splitlines() 的行数 —— 那是 str 自己的切法，和 newline 无关，摆这儿当对照。
  文件 LF.txt，磁盘 = b'a\nb\n'
    newline=None    → repr='a\nb\n'       splitlines 行数=2  ← 不写 newline 时的默认值
    newline=''      → repr='a\nb\n'       splitlines 行数=2
    newline='\n'    → repr='a\nb\n'       splitlines 行数=2
    newline='\r'    → repr='a\nb\n'       splitlines 行数=2
    newline='\r\n'  → repr='a\nb\n'       splitlines 行数=2
  文件 CRLF.txt，磁盘 = b'a\r\nb\r\n'
    newline=None    → repr='a\nb\n'       splitlines 行数=2  ← 不写 newline 时的默认值
    newline=''      → repr='a\r\nb\r\n'   splitlines 行数=2
    newline='\n'    → repr='a\r\nb\r\n'   splitlines 行数=2
    newline='\r'    → repr='a\r\nb\r\n'   splitlines 行数=2
    newline='\r\n'  → repr='a\r\nb\r\n'   splitlines 行数=2
  文件 CR.txt，磁盘 = b'a\rb\r'
    newline=None    → repr='a\nb\n'       splitlines 行数=2  ← 不写 newline 时的默认值
    newline=''      → repr='a\rb\r'       splitlines 行数=2
    newline='\n'    → repr='a\rb\r'       splitlines 行数=2
    newline='\r'    → repr='a\rb\r'       splitlines 行数=2
    newline='\r\n'  → repr='a\rb\r'       splitlines 行数=2
  → 整份 read() 时唯一会变的是 newline=None：\r\n 和 \r 都被翻译成 \n，三份文件读回来一模一样。
  → newline='' 完全不翻译，磁盘上有什么字符串里就有什么 \r —— 想让 \r 留在字符串里就靠它。
  → 指定某个串（'\n' / '\r' / '\r\n'）时也不翻译；指定值只影响「按行读」，所以本表这一列全是 2 行，看不出区别 —— 区别在第 3 段。
  → 不写 newline 和写 newline=None 是同一件事（本机实测相等 → True）。
```

这张 3×5 表的信息量全在 `repr` 里：`newline=None`（也就是不写 `newline`）那一格，`CRLF.txt` 和 `CR.txt` 读回来都是 `'a\nb\n'` —— `\r\n` 和 `\r` 都被翻译成 `\n`，三份不同的文件读回**一模一样**的字符串。而 `newline=''` 完全不翻译，磁盘上有什么，字符串里就有什么 `\r`。指定 `'\n'` / `'\r'` / `'\r\n'` 这三种也不翻译。

那张表下面还有一句容易被跳过去：「不写 `newline`」和「写 `newline=None`」是同一件事（本机实测相等 → `True`）。

### 读侧第二层：逐行迭代时，行数才露出差别

```
=== 3. 读侧的真正效果：逐行迭代时，行数才会露出差别 ===
  LF    newline=None    → 迭代到 2 行 ['a\n', 'b\n']
  LF    newline=''      → 迭代到 2 行 ['a\n', 'b\n']
  LF    newline='\n'    → 迭代到 2 行 ['a\n', 'b\n']
  LF    newline='\r'    → 迭代到 1 行 ['a\nb\n']
  LF    newline='\r\n'  → 迭代到 1 行 ['a\nb\n']
  CRLF  newline=None    → 迭代到 2 行 ['a\n', 'b\n']
  CRLF  newline=''      → 迭代到 2 行 ['a\r\n', 'b\r\n']
  CRLF  newline='\n'    → 迭代到 2 行 ['a\r\n', 'b\r\n']
  CRLF  newline='\r'    → 迭代到 3 行 ['a\r', '\nb\r', '\n']
  CRLF  newline='\r\n'  → 迭代到 2 行 ['a\r\n', 'b\r\n']
  CR    newline=None    → 迭代到 2 行 ['a\n', 'b\n']
  CR    newline=''      → 迭代到 2 行 ['a\r', 'b\r']
  CR    newline='\n'    → 迭代到 1 行 ['a\rb\r']
  CR    newline='\r'    → 迭代到 2 行 ['a\r', 'b\r']
  CR    newline='\r\n'  → 迭代到 1 行 ['a\rb\r']
  → 三格最有信息量：CR 文件配 newline='\n' 读出 **1 行**（整篇粘成一块）；
    CRLF 文件配 newline='\r' 读出 **3 行**（切在 \r 上，剩下一堆以 \n 开头的碎行）。
  → splitlines() 永远按「所有已知行边界」切，所以它数出来恒是 2 行 —— 
    别拿它当「我按这个 newline 能读到几行」的证据，两者不是一回事。
  ★ 代价：行数对不上时先怀疑 newline，别怀疑文件坏了；同一个文件配不同 newline 迭代，行数可以从 1 变到 3。
```

上面那张表整份读的时候看不出指定值的区别，因为指定值只影响**按行读**。一到逐行，差别立刻炸开：

- `CR.txt` 配 `newline='\n'`：整篇粘成 **1 行** `['a\rb\r']` —— 你写 `for line in f` 只进一次循环。
- `CRLF.txt` 配 `newline='\r'`：切在 `\r` 上，读出 **3 行** `['a\r', '\nb\r', '\n']`，多出的是以 `\n` 开头的碎行。
- 同一份文件，配不同的 `newline`，行数从 1 到 3。所以「行数对不上」先怀疑 `newline`，别怀疑文件坏了。

最后一列 `splitlines()` 是**对照项，不是证据**：`str.splitlines()` 永远按所有已知行边界切，恒数 2 行，跟 `newline` 一点关系都没有。别拿它当「我按这个 `newline` 能读到几行」。

### 写侧：只有 `\n` 会被换掉，串里现成的 `\r` 一个都不动

```
=== 4. 写侧 5 格：你写的 \n 落地时变成了什么 ===
  待写入的字符串恒为 'a\nb\n'（两个 \n），落盘字节逐个用 read_bytes 验：
  newline=None    → 磁盘 = b'a\nb\n'        hex=610a620a 字节数=4  ← 默认值这一格和 newline='' 完全一样（本机）
  newline=''      → 磁盘 = b'a\nb\n'        hex=610a620a 字节数=4
  newline='\n'    → 磁盘 = b'a\nb\n'        hex=610a620a 字节数=4
  newline='\r'    → 磁盘 = b'a\rb\r'        hex=610d620d 字节数=4
  newline='\r\n'  → 磁盘 = b'a\r\nb\r\n'    hex=610d0a620d0a 字节数=6
  本机 os.linesep = '\n'，磁盘字节数=1。newline=None 那一格写出的还是 \n，和 newline='' 一模一样。
  ★ 这一格本机验不出跨平台差别：os.linesep='\n' 时「翻译成 os.linesep」这句话没有可观察的后果。
    它只在本机成立，别当已验证的跨平台结论引用。
  → newline='' 与 newline='\n' 都把 \n 原样写下去（本机）；newline='\r' 把每个 \n 换成 \r；newline='\r\n' 把每个 \n 换成 \r\n。
  → 写侧的规则只有一句：**只有 \n 会被换掉**，字符串里现成的 \r 一个都不动 —— 看第 5 段。

=== 5. 写侧的连环翻译：串里本来就带 \r\n 时会变成 \r\r\n（CSV 空行的根）===
  待写入字符串 = 'x,y\r\nz,w\r\n'（\r 和 \n 都在字符串里了）
  newline=None    → 磁盘 = b'x,y\r\nz,w\r\n'
  newline=''      → 磁盘 = b'x,y\r\nz,w\r\n'
  newline='\r\n'  → 磁盘 = b'x,y\r\r\nz,w\r\r\n'
  → newline='\r\n' 时字符串里那个 \n 又被换成 \r\n，于是每个行尾变成 \r\r\n；
    Excel / 文本编辑器把这个多余的 \r 当成一个单独的行边界，屏幕上就多了空行。
  → 本机 newline=None 这一格恰好没变（os.linesep 就是 \n）；
    在 os.linesep='\r\n' 的机器上它同样会变成 \r\r\n（本机未验证）。
  → 所以 csv 模块的官方要求是**开文件时 newline=''**：让 csv 自己决定字节，文本层别插手。
    为什么必须这样、以及不这样会打出什么字节，见第 15 节 n_csv.py。
  ★ 本节只做文本层翻译的对照，不碰 csv 的引号与分隔符 —— 那是第 15 节的事。
```

待写入的字符串恒为 `'a\nb\n'`，落盘字节 `newline='\r'` 变成 `b'a\rb\r'`、`newline='\r\n'` 变成 `b'a\r\nb\r\n'`（6 字节）。本机 `os.linesep` 就是 `'\n'`，所以 `None` 那一格和 `''`、`'\n'` 三格完全一样 —— 这一格的跨平台差别本机验不出来，`★` 那两行老老实实标了「别当已验证的跨平台结论」，我也照原样提醒你。

真正的杀器在同一个围栏的后半段（`=== 5.`）：字符串里**本来就已经带 `\r\n`**（`csv` 就是这种），再配 `newline='\r\n'`，那个 `\n` 又被换一次，落盘成 `b'x,y\r\r\nz,w\r\r\n'`。Excel 和文本编辑器把多出来的 `\r` 当成一个单独的行边界，屏幕上就多了空行 —— 这就是「CSV 莫名其妙多空行」的机制。反过来，`csv` 模块要求你**开文件时写 `newline=''`**，让 csv 自己决定字节，文本层别插手（为什么必须这样、不这样打出什么字节，第 15 节整节在演它）。

**这一节带走：**

- 读侧只有 `newline=None`（默认）会改字符串内容：把 `\r\n`、`\r` 统一成 `\n`；其余四种磁盘上是什么就读回什么。
- 想让自己数行、自己解析、或者交给 `csv`，就 `newline=''` —— 它的含义是「别替我翻译」，不是「按这个切行」。
- 写侧翻译目标由 `os.linesep` 决定的只有 `None` 那一格；其余几格换成的就是你填的串，而且**只换 `\n`**。
- 行数对不上：先查 `newline`，再查最后一个字节是不是换行符；`splitlines()` 的结果不能当 `newline` 的证据。

---

## 第 06 节 · 默认编码是从哪来的，BOM 是怎么回事

不写 `encoding=` 的时候，`open()` 用的是**这台机器的配置**，不是**这个文件的编码**。这句话解释了大半「在我机器上好好的」类事故。先把四个「默认」各自管哪个方向钉死。

```
$ python3 e_encoding_default.py
=== 1. 四个「默认」到底是谁，本机实测值 + 各管哪个方向 ===
  sys.getdefaultencoding()          = 'utf-8'          管 str.encode()/bytes.decode() 不写编码时，**和文件无关**
  sys.getfilesystemencoding()       = 'utf-8'          管文件名 / argv / env 的 str↔bytes
  locale.getpreferredencoding(False)= 'UTF-8'          管 open() 不写 encoding 时
  sys.stdout.encoding               = 'utf-8'          管 print() 到屏幕，errors='surrogateescape'
  locale.getlocale()                = ('C', 'UTF-8')
  open(p, encoding=None).encoding    = 'UTF-8'（errors='strict'）—— 实测，不是照抄
  → open() 用的是 getpreferredencoding(False)：相等 → True；和 getdefaultencoding() 的字符串相等吗 → False。
  → 但 codecs.lookup 之后两者是同一个 codec → True（'UTF-8' 是 locale 给的写法，'utf-8' 是解释器给的写法，比的是**出处**）
  ★ 判据第一条：四个默认各管一个方向。搞混了就出现「文件名能建、内容读不出」这类怪事。
```

四个默认各管一个方向，互不替补：`sys.getdefaultencoding()` 管 `str.encode()` / `bytes.decode()` 不写编码时（和文件无关），`sys.getfilesystemencoding()` 管文件名 / argv / env，`locale.getpreferredencoding(False)` 管 `open()`，`sys.stdout.encoding` 管 `print()`。围栏里那两个 `→` 是重点：`open()` 用的是 `getpreferredencoding(False)`（相等 → `True`），跟 `getdefaultencoding()` 连字符串写法都不一样（`'UTF-8'` vs `'utf-8'`，大小写就是出处）—— 但 `codecs.lookup` 之后是同一个 codec，所以比的是**出处**，不是值。

### 把开关现场拨一遍

```
=== 2. 开关现场对比：同一个子进程小程序，六套 env，结果打一张表 ===
  子进程命令统一是：python3 -c <同一段 9 行小程序>，只有 env 不同（屏幕上不出现解释器路径）
  编号  defaultenc filesystem preferredencoding      stdout:errors          getlocale        utf8mode open
  A     utf-8     utf-8     UTF-8                  utf-8:surrogateescape  ('C','UTF-8')    0        UTF-8    正常结束=True
  B     utf-8     utf-8     utf-8                  utf-8:surrogateescape  ('C','UTF-8')    1        utf-8    正常结束=True
  C     utf-8     utf-8     UTF-8                  utf-8:replace          ('C','UTF-8')    0        UTF-8    正常结束=True
  D     utf-8     utf-8     utf-8                  utf-8:surrogateescape  (None,None)      1        utf-8    正常结束=True
  E     utf-8     utf-8     utf-8                  utf-8:surrogateescape  (None,None)      1        utf-8    正常结束=True
  F     utf-8     ascii     ANSI_X3.4-1968         ascii:surrogateescape  (None,None)      0        ANSI_X3.4-1968 正常结束=True
  每套 env 的含义：
    · A 基线：LC_ALL=C.UTF-8，三个开关都不设（就是本条 demo 命令拨好的位置）
    · B 加 PYTHONUTF8=1 → 强制开 PEP 540 UTF-8 模式
    · C 加 PYTHONIOENCODING → 只管标准流的编码和 errors
    · D 换 LC_ALL=C（POSIX locale）
    · E LC_ALL=C ＋ 关掉 PEP 538 的 locale 自动改造
    · F LC_ALL=C ＋ 把 UTF-8 模式也按住
  → sys.getdefaultencoding() 六格全不变（3.7 起编译期就固定成 utf-8）—— 这正好证明它**不是** open() 的默认编码。
  → 只有 locale.getpreferredencoding(False) 跟着动，而 open 那一列和它逐格相等：open() 的默认编码就是这个函数的返回值。
  → 大小写能认出来源：'UTF-8'（大写）是 locale 报的，'utf-8'（小写）是 UTF-8 模式给的；
    D、E 两格 pref=utf-8 且 utf8mode=1 —— **LC_ALL=C 会自动打开 UTF-8 模式**，
    所以「POSIX locale 就是 ascii」这句老话在本机默认不成立。
  → 只有 F（把 UTF-8 模式也按住）才让 default 真变成 ANSI_X3.4-1968，也就是 ascii。
  → sys.stdout.errors 基线就是 'surrogateescape'，这是 print 出坏字节不报错的根；C 那一格被 PYTHONIOENCODING 改成了 replace。
```

六套 env、同一个 9 行子进程小程序。三点观察：

- `sys.getdefaultencoding()` **六格全是 `utf-8` 一动不动**（3.7 起编译期就固定了）。这一列的存在就是为了反证：它**不是** `open()` 的默认编码。
- 只有 `preferredencoding` 那一列跟着 locale 动，而 `open` 那一列和它**逐格相等**。
- 大小写能当指纹：`'UTF-8'` 是 locale 报的，`'utf-8'` 是 PEP 540 UTF-8 模式给的。D、E 两格 `pref=utf-8` 且 `utf8mode=1` —— 说明 `LC_ALL=C` 会**自动**打开 UTF-8 模式，所以「POSIX locale 就是 ascii」这句老话在本机默认不成立。只有 F（把 UTF-8 模式也按住）才真的落到 `ANSI_X3.4-1968`，也就是 ascii。

### F 那套 env 下 print 一个中文，以及四种 codec 的字节账

```
=== 3. 现场撞一次：F 那套 env 下 print 一个中文会怎样 ===
  子进程 python3 -c "print('\\u4e2d')" 正常结束吗 → False（退非 0）
  屏幕上打出的字节数 → 0（一个字都没出去）
  异常原文：UnicodeEncodeError: 'ascii' codec can't encode character '\u4e2d' in position 0: ordinal not in range(128)
  同一句在基线 A 下：输出 = '中\n'，正常结束 = True
  ★ 代价：这段代码一个字都没改，改的只有环境变量。「我这台机器能跑」在这件事上一点证据都不是。

=== 4. 同一串文本，四种 codec 各给几个字节；latin-1 永不报错 ≠ 读对了 ===
  编码方向：'中文字幕'（4 个汉字）
    utf-8     字节数=12  hex 前 12=e4b8ade69687 字节=b'\xe4\xb8\xad\xe6\x96\x87\xe5\xad\x97\xe5\xb9\x95'
    gbk       字节数=8   hex 前 12=d6d0cec4d7d6 字节=b'\xd6\xd0\xce\xc4\xd7\xd6\xc4\xbb'
    gb18030   字节数=8   hex 前 12=d6d0cec4d7d6 字节=b'\xd6\xd0\xce\xc4\xd7\xd6\xc4\xbb'
    latin-1   UnicodeEncodeError: 'latin-1' codec can't encode characters in position 0-3: ordinal not in range(256)
  再加一个 emoji '🙂'：utf-8=4 字节、gbk=编不下、gb18030=4 字节、latin-1=编不下（逐个真试过，不是背的）
  解码方向才是重点：把上面那串 **UTF-8 字节**当 latin-1 读：
    这串字节共 12 个：e4b8ade69687e5ad97e5b995
    latin-1 解出 'ä¸\xadæ\x96\x87å\xad\x97å¹\x95'
    12 个字符，**没报任何错** —— 每个坏字节都领到了一个合法字符
    再 latin-1 编回去能还原字节吗 → True（latin-1 是 0x00~0xFF 的一一映射）
  ★ latin-1 永远不会报错，它只是把乱码给你：0x00~0xFF 每个字节都有对应字符，所以「读成功了」和「读对了」是两件事。
  反方向才有响：把 latin-1 的字节当 utf-8 解 ——
    'café'.encode('latin-1') = 636166e9 → UnicodeDecodeError: 'utf-8' codec can't decode byte 0xe9 in position 3: unexpected end of data
    这就是「文件明明存在，一读就 UnicodeDecodeError」的机制：字节是对的，codec 是错的。
```

上半段（`=== 3.`）的分量在于：**代码一个字都没改，只改环境变量**，`print` 一个中文就从「输出 `'中\n'`、正常结束 = `True`」变成「屏幕上打出的字节数是 0、退非 0」，异常原文是 `UnicodeEncodeError`，消息里那句 `'ascii' codec can't encode character '\u4e2d'` 和尾巴 `ordinal not in range(128)` 都写得很明白。「我这台机器能跑」在这件事上一点证据都不是。

下半段（`=== 4.`）是同一串 `'中文字幕'` 的字节账：`utf-8` 给 12 字节、`gbk` 和 `gb18030` 各 8 字节、`latin-1` 直接编不下。加个 emoji 之后 `gbk` 也编不下、`gb18030` 还能给 4 字节。真正的问题是**解码方向**：把那 12 个 UTF-8 字节当 `latin-1` 读，得到 12 个字符的乱码，而且**一个错都不报** —— `latin-1` 是 0x00~0xFF 的一一映射，每个坏字节都领得到合法字符，还能原样编回那串字节。所以「读成功了」和「读对了」是两件事。反方向才有响：把 `latin-1` 的字节（`'café'.encode('latin-1')` = `636166e9`）当 utf-8 解，`UnicodeDecodeError` 直接点名 `can't decode byte 0xe9 in position 3` —— 这就是「文件明明存在，一读就炸」的机制：字节是对的，codec 是错的。

### BOM：三个字节让首行判断当场失手

```
=== 5. BOM：三个字节让首行判断当场失败 ===
  同一份文本，utf-8 编出 21 字节，utf-8-sig 编出 24 字节，多的那三个 hex = efbbbf
  而 '\ufeff'.encode('utf-8') = efbbbf —— BOM 就是 U+FEFF 的 UTF-8 编码本身
  encoding='utf-8'     读到 '\ufeff# 配置说明\n值=1\n' ← 首字符是 U+FEFF
  encoding='utf-8-sig' 读到 '# 配置说明\n值=1\n' ← 头三个字节被吃掉；没有 BOM 的文件用它读也正常 → True
  现场撞一次：跳过注释行的那段代码，两种读法各跑一遍：
    encoding=utf-8      首行 startswith('#') = False  → 当成配置的行数 = 2（正确应是 1）
    encoding=utf-8-sig  首行 startswith('#') = True   → 当成配置的行数 = 1（正确应是 1）
  → 首行那条注释没被认出来，配置里凭空多出一行垃圾；报错没有，警告没有。
  UTF-16 的 BOM 与字节序（同一份文本，只打头四字节）：
    utf-16     头四字节 hex = fffe2300   全串字节数 = 24
    utf-16-le  头四字节 hex = 23002000   全串字节数 = 22
    utf-16-be  头四字节 hex = 00230020   全串字节数 = 22
    ff fe = 小端标记、fe ff = 大端标记；utf-16 带标记而 -le/-be 不带 —— 所以不带 BOM 的 UTF-16 只能靠约定猜字节序。
```

同一份文本，`utf-8` 编出 21 字节，`utf-8-sig` 编出 24 字节，多出来的三个是 `efbbbf`，而 `'\ufeff'.encode('utf-8')` 正好也是 `efbbbf` —— BOM 不是什么协议头，它就是 U+FEFF 这个字符本身的 UTF-8 编码。用 `encoding='utf-8'` 读带 BOM 的配置，首字符是 `\ufeff`，那句 `startswith('#')` 返回 `False`，注释行被当成配置，**凭空多出一行垃圾**（本机实测：当成配置的行数 2，正确应是 1），没有报错也没有警告。`utf-8-sig` 会吃掉头三个字节，而且**没有 BOM 的文件用它读照样正常**，所以读 Windows 记事本 / Excel 导出的文本就用它最省事。

底下 UTF-16 那三行顺手解释了字节序：`utf-16` 带 `ff fe` 小端标记，`utf-16-le` / `utf-16-be` 不带 —— 不带标记的 UTF-16 只能靠约定猜字节序。

**这一节带走：**

- `open()` 的默认编码 = `locale.getpreferredencoding(False)`，由进程 locale 和 `PYTHONUTF8` 决定；**文件内容里没有任何信息能改变它**。
- 四个默认各管一个方向，谁也不替补另一个。「文件名能建、内容读不出」就是混用的结果。
- `encoding=` 永远显式写。不写的脚本读的是这台机器的配置，换机器、换容器、换 CI、加一个环境变量就变，而且是悄悄变。
- 「没报错」不是「读对了」的证据：`latin-1` 解码永不报错，它只是把乱码给你。
- 带 BOM 的 UTF-8 用 `utf-8` 读会留下 `\ufeff`，首行判断当场失手；用 `utf-8-sig` 读，没 BOM 也能读。

---

## 第 07 节 · `errors=` 的五种结局和它的代价

`encoding=` 挑错会当场炸（第 06 节）。还有一种更阴的情况：**codec 挑对了，但字节里混进了这个 codec 不认的东西**。那一刻怎么办，由 `errors=` 决定。五个取值里只有一个会响，另外四个都在改内容 —— 改完还送你一个「读取成功」。

先造一份明知不合法的字节，把 hex 打出来，你可以自己复现：

```
$ python3 f_errors_param.py
=== 1. 先造一份「明知不合法」的字节，把 hex 打出来给读者复现 ===
  构造式：'正常abc'.encode('utf-8') + b'\xff\xfe\x81' + '尾巴'.encode('utf-8')
  落盘 hex = e6ada3e5b8b8616263fffe81e5b0bee5b7b4
  字节数 = 18 ｜ 其中坏的是第 9~11 号（从 0 数）
  逐字节看这三位：0xff, 0xfe, 0x81 —— UTF-8 里 0xff/0xfe 从来不是任何字符的首字节，0x81 是续字节却站在开头。
  → 这一份字节**没有**任何修复余地：不是编码选错了，是这三位根本不是 UTF-8。
  ★ 所以本节全部对照都用同一份 bytes，只换 errors=，别的都不动。
```

18 个字节，坏的是第 9~11 号那三位：`0xff`、`0xfe` 在 UTF-8 里从来不是任何字符的首字节，`0x81` 是续字节却站在开头。**这三位根本不是 UTF-8**，不是「编码选错了」，没有任何修复余地。整节的所有对照都只用这一份 bytes，只换 `errors=`。

### 读侧：同一份字节，五种结局

```
=== 2. 读侧五种结局：同一份字节，errors= 换一个，屏幕上就换一种东西 ===
  errors='strict'            → 抛异常（原文）：UnicodeDecodeError: 'utf-8' codec can't decode byte 0xff in position 9: invalid start byte
      结构化字段：位置 9~10（字节下标）、原因 'invalid start byte'
  errors='ignore'            → 字符数=7   repr='正常abc尾巴'
  errors='replace'           → 字符数=10  repr='正常abc���尾巴'
  errors='surrogateescape'   → 字符数=10  repr='正常abc\udcff\udcfe\udc81尾巴'
  errors='backslashreplace'  → 字符数=19  repr='正常abc\\xff\\xfe\\x81尾巴'
  → 同一份 18 字节的文件：ignore 给 7 个字符、replace 和 surrogateescape 各给 10 个、
    backslashreplace 给 19 个；strict 那格没有字符，只有异常。
  → ignore 的 7 个字符看着「干净」，其实是被切掉了三块；replace 用 U+FFFD 顶位，坏字节至少留了个占位符。
  → surrogateescape 把每个坏字节塞成一个 \udcXX 代理码点 —— 它不是字符，是**临时座位**。
  → backslashreplace 把坏字节写成 '\xff' 这样的可见文本，字符数反而最多。
  ★ 只有 strict 会响。剩下四个都在**改内容**，改完还给你一个「读取成功」。
```

- `strict`：没有字符，只有异常，而且带着结构化字段 —— 位置 9~10、原因 `'invalid start byte'`。
- `ignore`：7 个字符，看着「干净」，其实被切掉三块。
- `replace`：10 个字符，每个坏字节换成 `U+FFFD` 这个替换字符，坏字节至少留了个占位符。
- `surrogateescape`：10 个字符，坏字节各塞进一个 `\udcXX` 代理码点 —— 它**不是字符**，是那个字节的临时座位。
- `backslashreplace`：19 个字符，把坏字节写成 `'\xff'` 这种可见文本，字符数反而最多。

18 字节的文件，读出 7 / 10 / 10 / 19 个字符四种答案。这就是「读成功」这个词有多空。

### `ignore` 到底丢了几克数据

```
=== 3. 数一数 ignore 到底丢了几克数据：字节账 + 警告账 + 退出账 ===
  原文件 18 字节 → ignore 读出 7 个字符 → 写回磁盘 15 字节
  被悄悄扔掉的字节数 = 18 - 15 = 3（正好是 b'\xff\xfe\x81' 那三个）
  期间捕获到的警告条数 = 0（simplefilter('always') 已经把该报的都报了）
  同一个操作交给子进程 python3 -c 跑：正常结束 → True ｜ 它往 stderr 打了字吗 → False
  子进程自己写回磁盘的那份 = 15 字节 —— 和上面那笔账对得上
  ★ 没有一条警告，程序还退 0 ——「ignore 没报错」这件事本身，就是它最贵的地方。
  → 差额 3 字节听起来不多，但这是一次**不可见的删除**：同样写法跑在 8GB 日志上，少掉的东西没人能补回来。

=== 4. surrogateescape 能原样回去，ignore 之后回不去（sha256 逐字对比）===
  读进来 10 个字符：'正常abc\udcff\udcfe\udc81尾巴'
  用 errors='surrogateescape' 写回去：b'\xe6\xad\xa3\xe5\xb8\xb8abc\xff\xfe\x81\xe5\xb0\xbe\xe5\xb7\xb4'
  原文件 sha256 前 16 位 = 5a96ac0ef47f2f0a ｜ 回写后 = 5a96ac0ef47f2f0a ｜ 逐字节相同 → True
  对照：ignore 的字符串再怎么写都造不回原文件 —— 回写 sha256 前 16 位 = 0ef8819bdb542688 ｜ 相同 → False
  那这些代理码点能不能用默认 errors 写出去？不能：
    → UnicodeEncodeError: 'utf-8' codec can't encode characters in position 5-7: surrogates not allowed
  → 判据：surrogateescape 是**成对**的。读的时候用它，写回去也必须用它，
    中间那步换了 strict 就当场响（这是好事），换了 ignore 就悄悄丢。
```

这笔账分三本：字节账 —— 原文件 18 字节，`ignore` 读出 7 个字符，写回磁盘 15 字节，被悄悄扔掉的是 `18 - 15 = 3` 个字节，正好是那三个坏字节。警告账 —— `simplefilter('always')` 已经把该报的都报了，期间 **0 条警告**。退出账 —— 同一个操作交给子进程跑，正常结束 `True`，往 stderr 打了字吗 `False`，它写回磁盘的也是 15 字节，和上面那笔账对得上。

**没有一条警告，程序还退 0。** 「`ignore` 没报错」这件事本身，就是它最贵的地方。3 个字节听着不多，但这是一次**不可见的删除**：同样的写法跑在 8GB 日志上，少了哪一段没人能补回来。

围栏后半段（`=== 4.`）是 `surrogateescape` 的价值所在：读进来 10 个字符，用同一个 `errors` 写回去，回写文件和原文件的 sha256 前 16 位**逐字节相同**（`5a96ac0ef47f2f0a`）；而 `ignore` 的那个字符串无论怎么写都造不回原文件（`0ef8819bdb542688`，相同 → `False`）。反过来，那些代理码点**不能**用默认 `errors` 写出去 —— 立刻抛 `UnicodeEncodeError`，消息尾巴是 `surrogates not allowed`。所以 `surrogateescape` 是**成对**的：读的时候用它，写回去也必须用它。中间那一步换成 `strict` 会当场响（这是好事），换成 `ignore` 就悄悄丢。

### 写侧：五种 `errors` 落到磁盘上的真实字节

```
=== 6. 写侧五种 errors：落到磁盘上的真实文本，这一屏就是对照表 ===
  encoding=gbk（回读时按 gb18030 解，只为了给眼睛看；磁盘字节以 repr 为准）
    errors='backslashreplace'   磁盘字节数=18   磁盘=b'\xd5\xfd\xb3\xa3\\U0001f642\xce\xb2\xb0\xcd'
        按 gb18030 回读 → '正常\\U0001f642尾巴'
    errors='xmlcharrefreplace'  磁盘字节数=17   磁盘=b'\xd5\xfd\xb3\xa3&#128578;\xce\xb2\xb0\xcd'
        按 gb18030 回读 → '正常&#128578;尾巴'
    errors='namereplace'        磁盘字节数=33   磁盘=b'\xd5\xfd\xb3\xa3\\N{SLIGHTLY SMILING FACE}\xce\xb2\xb0\xcd'
        按 gb18030 回读 → '正常\\N{SLIGHTLY SMILING FACE}尾巴'
    errors='ignore'             磁盘字节数=8    磁盘=b'\xd5\xfd\xb3\xa3\xce\xb2\xb0\xcd'
        按 gb18030 回读 → '正常尾巴'
    errors='replace'            磁盘字节数=9    磁盘=b'\xd5\xfd\xb3\xa3?\xce\xb2\xb0\xcd'
        按 gb18030 回读 → '正常?尾巴'
  encoding=latin-1（回读时按 latin-1 解，只为了给眼睛看；磁盘字节以 repr 为准）
    errors='backslashreplace'   磁盘字节数=34   磁盘=b'\\u6b63\\u5e38\\U0001f642\\u5c3e\\u5df4'
    errors='xmlcharrefreplace'  磁盘字节数=41   磁盘=b'&#27491;&#24120;&#128578;&#23614;&#24052;'
    errors='namereplace'        磁盘字节数=145  磁盘=b'\\N{CJK UNIFIED IDEOGRAPH-6B63}\\N{CJK UNIFIED IDEOGRAPH-5E38}\\N{SLIGHTLY SMILING FACE}\\N{CJK UNIFIED IDEOGRAPH-5C3E}\\N{CJK UNIFIED IDEOGRAPH-5DF4}'
    errors='ignore'             磁盘字节数=0    磁盘=b''
    errors='replace'            磁盘字节数=5    磁盘=b'?????'
  → backslashreplace 留下 '\U0001f642'、xmlcharrefreplace 留下 '&#128578;'、
    namereplace 留下 '\N{SLIGHTLY SMILING FACE}' —— 三种都是**可读的替身**，
    坏掉那个字符是谁，回文件里还查得回来。
  → replace 打成一个 '?'、ignore 什么都不留：
    latin-1 + ignore 写出来是 0 字节文件，整句中文凭空蒸发，而且不报错。
  → 反过来，xmlcharrefreplace 用在**解码**方向会当场撞：TypeError: don't know how to handle UnicodeDecodeError in error callback
    所以写侧和读侧能用的 handler 名单不一样，不是同一套五个随便挑：
    surrogateescape 只在解码方向有意义，namereplace / xmlcharrefreplace 只在编码方向。
```

这两组写的是同一句中文夹一个表情符号（围栏开头注明：回读时按 `gb18030` 解只是给眼睛看，磁盘字节以 `repr` 为准）。`gbk` 那一组里，`backslashreplace` 留下 `\U0001f642`、`xmlcharrefreplace` 留下 `&#128578;`、`namereplace` 留下 `\N{SLIGHTLY SMILING FACE}` —— 三种都是**可读的替身**，坏掉的那个字符是谁，回文件里还查得回来。`replace` 只打成一个 `?`，`ignore` 什么都不留。`latin-1` 那一组更极端：`ignore` 写出来是 **0 字节文件**，整句中文凭空蒸发（磁盘 `b''`），`replace` 给 5 个 `?`，`namereplace` 把每个汉字都展开成 `\N{CJK UNIFIED IDEOGRAPH-6B63}` 这种全名，磁盘字节数冲到 145。

最后那两条 `→` 容易被忽略：这些 handler **读写两侧不是同一套名单**。`xmlcharrefreplace` 用在解码方向当场 `TypeError: don't know how to handle UnicodeDecodeError in error callback`；`surrogateescape` 只在解码方向有意义，`namereplace` / `xmlcharrefreplace` 只在编码方向。别以为五个名字随便挑。

**这一节带走：**

- 五个取值里只有 `strict` 会响；`ignore` / `replace` / `surrogateescape` / `backslashreplace` 都在**改内容**，改完还回你一个「读取成功」。
- 永远不要把 `ignore` 当宽容：它是删除，本机称出的重量是 3 字节、0 条警告、退码 0。
- 要「先留着、以后原样写回去」只有 `surrogateescape`，而且读写必须成对用同一个 `errors`。
- 要「人眼还能看出这里坏过」用 `replace` 或 `backslashreplace`；写侧的 `namereplace` / `xmlcharrefreplace` 会留下可查的字符全名或数字引用。
- `errors=` 是文本层的旋钮，读写两侧可用名单不同；二进制模式想容错得自己 `decode(errors=...)`。

---

## 第 08 节 · `Path` 是对象，不是字符串

到这里前六节一直在「字节 ↔ 字符」和「行 ↔ 字符」这两条线上：模式、缓冲、换行、编码、`errors=`。从这一节起换第三条线：**路径 ↔ 操作**。pathlib 的困惑几乎全部来自同一条界线没分清 —— 哪些方法是在**算字符串**，哪些方法是在**问内核**。本节先认类型，再拿一条本机根本不存在的路径，把两侧各问一遍。

```
$ python3 g_pathlib_basics.py
=== 1. 先认清手里的东西是什么类型（它不是 str）===
  type(FAKE) → PosixPath ｜ MRO： PosixPath → Path → PurePosixPath → PurePath → object
  isinstance(FAKE, str) → False ｜ type(FAKE) is str → False
  → 判据：类树分两支 —— PurePath 一支只做字符串活，Path 一支多出 exists/stat/mkdir 那些问内核
    的方法。本机是 POSIX，实现类是 PosixPath；同一份代码在 Windows 上拿到的是 WindowsPath。

=== 2. ★道具：一个本机根本不存在的路径，十个属性一次全算出来 ===
  道具路径（它不存在，也永远不会被创建）：'/no/such/dir/报告 v2.tar.gz'
  parts         → ('/', 'no', 'such', 'dir', '报告 v2.tar.gz')
  anchor        → '/'
  drive         → ''
  root          → '/'
  name          → '报告 v2.tar.gz'
  stem          → '报告 v2.tar'
  suffix        → '.gz'
  suffixes      → ['.tar', '.gz']
  parent        → PosixPath('/no/such/dir')
  is_absolute() → True
  十项里抛异常的个数 = 0 ｜ 一个 syscall 都没发起
  parents 这个对象   → 类型 _PathParents，它是生成器吗 → False；iter(parents) 才是生成器 → True
  list(parents)      → [PosixPath('/no/such/dir'), PosixPath('/no/such'), PosixPath('/no'), PosixPath('/')]（能下标、能 len 的只读序列，越往右越靠近根）
  ★这就是 pathlib 所有困惑的总根源：上面每一行都是字符串活 ——
    不需要文件存在、不需要目录存在、不需要任何权限，也永远不会因为「不存在」而报错。
    两个顺手实测：drive 在 POSIX 上恒为 ''（那是留给 Windows 盘符的位）；stem 只去掉**最后一个**后缀，所以 '报告 v2.tar' 里那个 .tar 还留着。
```

`Path(...)` 拿到的是 `PosixPath`，MRO 分两支：`PurePosixPath` / `PurePath` 那一支只做字符串活，`Path` 这一支多出 `exists` / `stat` / `mkdir` 这些要问内核的方法。而 `isinstance(FAKE, str) → False`：它不是字符串，也不是「延迟的字符串」。

★ 那一格是本节的轴心：道具路径 `/no/such/dir/报告 v2.tar.gz` **不存在，也永远不会被创建**，可 `parts`、`anchor`、`drive`、`root`、`name`、`stem`、`suffix`、`suffixes`、`parent`、`is_absolute()` 十项全部算得出来，抛异常个数 0，一个 syscall 都没发起。两个顺手量到的细节也值得记：`stem` 只去掉**最后一个**后缀，所以是 `'报告 v2.tar'`，里面那个 `.tar` 还留着；`parents` 那个对象本身不是生成器（`_PathParents`，能下标能 `len`），`iter(parents)` 才是，列出来从最深的父目录一路排到 `/`。

### 跨过那条线去问内核

```
=== 3. 跨过那条线：同一个路径去问内核，三个 False 和一条原文 ===
  exists()   → False
  is_file()  → False
  is_dir()   → False
  FAKE.stat()                → FileNotFoundError: [Errno 2] No such file or directory: '/no/such/dir/报告 v2.tar.gz'
  → FileNotFoundError 的父亲是 OSError；exists/is_file/is_dir 问的也是内核，但它们**把失败咽成 False**，stat 原样抛给你。
  ★同一个「不存在」，两种答法，而且咽掉答案那种会骗人：父目录没「进入」权限时，真实存在的文件也答 False。
    先问一遍：exists() → True ｜ 列这一层能列到它 → ['在里面.txt']
    把 挡/ 改成 0o600 之后（此刻文件还在，一个字节都没少）：
      exists() → False ｜ is_file() → False   ← 两个都是假的 False
      target.stat()              → PermissionError: [Errno 13] Permission denied: 'pathlib-demo/挡/在里面.txt'
      → 判据：exists() 的 False 只证明「我查不到」，不证明「它不存在」；要分辨这两种情况，
        就去 stat() 并接住 OSError，看 errno 是 2（真没有）还是 13（不让查）。
```

同一个「不存在」，两种答法：`exists()` / `is_file()` / `is_dir()` 把失败咽成 `False`，`stat()` 原样抛 `FileNotFoundError: [Errno 2] No such file or directory`。咽掉答案那种**会骗人**：把父目录 `挡/` 的权限改成 `0o600` 之后，里面的文件还在、一个字节都没少，`exists()` 却答 `False`、`is_file()` 也答 `False`，而 `stat()` 给的是 `PermissionError: [Errno 13] Permission denied`。

判据就落在 errno 上：**2 是「真没有」，13 是「不让查」**。所以 `exists()` 的 `False` 只证明「我查不到」，不证明「它不存在」；要分辨这两种情况只能 `stat()` 并接住 `OSError` 看 `errno`。

### `/` 不是除法，但它也不校验任何东西

```
=== 4. 拼接：/ 不是除法，但它也不校验任何东西 ===
  'data/reports' / 'sub' / 'x.txt' → 'data/reports/sub/x.txt'（类型 PosixPath）
  'data/reports' / Path('a')       → 'data/reports/a'
  'data/reports' / '/abs/x'        → '/abs/x'   ← 前面整段被丢掉
    规则：只要某一段是绝对路径（POSIX 上以 / 开头），前面全部作废 —— 和 os.path.join 一模一样。
    ★「路径来自用户输入」时这一步就是越界：用户传一个绝对路径，你拼出来的前缀当场消失。
  BASE / 3                   → TypeError: unsupported operand type(s) for /: 'PosixPath' and 'int'   ← / 只认 str 或有 __fspath__ 的对象，int 连试都不试
  和字符串的 + 摆一起：'data/reports' + 'sub' → 'data/reportssub' ｜ BASE / 'sub' → 'data/reports/sub'
  → 判据：str 的 + 只管首尾相接，分隔符是你的责任；Path 的 / 才是「按路径语义接一段」。
```

`/` 不是除法，是重载出来的拼接运算符：`'data/reports' / 'sub' / 'x.txt'` 一路接下去，结果类型还是 `PosixPath`；右边接 `Path` 对象也认（`'data/reports' / Path('a')` → `'data/reports/a'`）。三条规矩：

- 只要某一段是绝对路径（POSIX 上以 `/` 开头），前面整段作废 —— `'data/reports' / '/abs/x'` 得到的就是 `'/abs/x'`。**「路径来自用户输入」时这一步就是越界**：用户传一个绝对路径，你拼好的前缀当场消失，而且不响。这条规矩和 `os.path.join` 一模一样。
- `BASE / 3` 直接 `TypeError`：`/` 只认 `str` 或者有 `__fspath__` 的对象，`int` 连试都不试。
- 跟 `str` 的 `+` 摆一起才看得清分工：`'data/reports' + 'sub'` 给你 `'data/reportssub'`（分隔符是你的责任），`BASE / 'sub'` 给你 `'data/reports/sub'`（按路径语义接一段）。

### 落盘前后：A 类一字不变，B 类当场翻转

```
=== 7. 同一个字符串，落盘前后：纯算一字不变，问内核当场翻转 ===
  前面所有道具路径都没落盘；现在真写一个：3 个字符「报告x」｜ 落盘前 exists() → False
  落盘后 exists() → True ｜ is_file() → True ｜ is_dir() → False
  落盘后 stat() → st_size=7 字节（字符数 3，汉字在 utf-8 下 3 字节一个）
  A 类那一组（10 项）变了吗 → 没变，逐项相等
  删掉之后 exists() → False ｜ A 类那一组还相等吗 → True
  ★结论：文件在不在、能不能读，只改变 B 类的答案；A 类从头到尾只看那串字符。

=== 8. Path 不能当 str 用，但 os.path.* 收得到它：靠 __fspath__ ===
  str(BASE) == BASE → False ｜ BASE == 'data/reports' → False
    → 内容一样、类型不一样，== 就是 False；在 dict 的 key 里 Path('a') 和 'a' 是两个不同的坑。
  hasattr(BASE, 'startswith') → False：
    BASE.startswith('data')    → AttributeError: 'PosixPath' object has no attribute 'startswith'
  可 open() 和 os.path.* 偏偏收得下它：
    open(BASE)                 → FileNotFoundError: [Errno 2] No such file or directory: 'data/reports'
  os.path.join(BASE, 'x.txt') → 'data/reports/x.txt' ｜ 返回类型 str（吐回 str）；同一段用 pathlib → PosixPath
  os.fspath(BASE) → str ｜ str(BASE) → str ｜ 两者相等 → True
  → 判据：os.path.* 和 open() 能收 Path，不是因为它们认识 Path，而是因为 Path 实现了 __fspath__，
    接口层当场把它换成字符串 —— 一过 os.path.join 这道口，类型就退回 str 了。
    混用两套 API 时「谁是 str 谁是 Path」要按**边界**记，别按习惯记（第 9 节整节在对照这个）。
```

前面那些道具路径都没真落过盘；这一段真写一个 3 个字符的文件。写之前 `exists() → False`，写之后 `exists() / is_file()` 翻成 `True`、`is_dir()` 还是 `False`，`stat()` 给 `st_size=7` 字节（3 个字符，汉字在 utf-8 下一个 3 字节 —— 这条正好接回第 02 节 `write` 返回 4 而盘上 10 字节那个坑）。而 A 类那 10 项**逐项相等，一个都没变**；删掉之后 `exists()` 回到 `False`，A 类还是相等。结论就一句：**文件在不在、能不能读，只改变 B 类（问内核）的答案；A 类（纯算）从头到尾只看那串字符。**

同一个围栏的后半段（`=== 8.`）讲的是两套 API 的接缝。`Path` 不能当 `str` 用：`str(BASE) == BASE` 是 `False`（内容一样、类型不一样，`==` 就是 `False`，所以在 `dict` 的 key 里 `Path('a')` 和 `'a'` 是两个不同的坑）；它也没有 `startswith`，一问就 `AttributeError: 'PosixPath' object has no attribute 'startswith'`。可 `open()` 和 `os.path.*` 偏偏收得下它 —— 不是因为它们认识 `Path`，而是因为 `Path` 实现了 `__fspath__`，接口层当场把它换成字符串。所以一过 `os.path.join` 这道口，返回类型就退回 `str` 了。混用两套 API 时「谁是 str 谁是 Path」要按**边界**记，别按习惯记（第 09 节整节在对照这个）。

**这一节带走：**

- 名字是**名词**的（`name` / `suffix` / `parts` / `parent`）几乎全是字符串活，对不存在的路径照样算得出、一个异常都不抛；动词（`stat` / `open` / `mkdir`）全是内核活，会抛 `OSError` 那一族。
- `exists()` 的 `False` 只证明「查不到」：父目录没进入权限时真实存在的文件也答 `False`。别在 `open()` 前面加 `exists()` 把关 —— 它不可靠，而且查完到打开之间还有一次竞态；直接 `open`、按 `errno`（2 / 13）分派，这才是第 07 章立的 EAFP。
- `/` 遇到绝对路径段会把前缀整段丢掉且不报错，用户输入必须自己盯；`Path` 和 `str` 之间唯一的合法通道是 `os.fspath()` / `str()`。
- `with_suffix` / `relative_to` / `match` 这些变形也全在纯算那一侧（本节的脚本还演了它们「会响」与「不响」的几种情况，包括不响但答案是错的那种），真要改名得用 `rename()`，那是问内核。

---

## 第 09 节 · os.path 的字符串函数逐个对照

学 pathlib 的人普遍卡在一个问题上：`os.path` 那一堆函数还要不要学？已经会 pathlib 的人又常以为
pathlib 只是把 `os.path` 换了个对象写法。这一节把两边逐个并排，排完你会发现真正的分界线不在函数名上，
而在**这一步有没有碰内核**：`parts / name / suffix / parent` 是纯字符串运算，路径不存在也照样算得出来、
一个异常都不抛；`exists / stat / open / resolve / mkdir / unlink` 是问内核，路径不在当场给你
`FileNotFoundError`。这条线贯穿本章第 09 到第 14 节，后面每一节都会回到它。

先看真身：`os.path` 是个模块（本机 POSIX 挑到的是 `posixpath`，Windows 上会被换成 `ntpath`），
`Path` 是个工厂函数（本机返回 `PosixPath`），而且 `issubclass(Path, PurePath) → True`、
MRO 是 `Path → PurePath → object`。`os.path` 之所以能直接吃一个 `Path`，是因为它内部问 `__fspath__`。
下面这张表的最后一列就是本节的主轴，先看名字怎么拆：

```
$ python3 h_pathlib_vs_ospath.py
=== 1. 先认两套 API 的真身：一个是模块，一个是类 ===
  os.path 其实是 → module 模块 posixpath（本机 POSIX 挑到的实现）；在 Windows 上它会被换成 ntpath
  pathlib 的 Path → pathlib.Path（工厂函数，实际返回 PosixPath）
  两边的入口类型：os.path.split(str) → tuple 里装 str；Path(s).parent → PosixPath
  两支的父子关系：issubclass(Path, PurePath) → True ｜ Path 的 MRO：Path → PurePath → object
  收 Path 时 os.path 靠什么活：os.path.basename(P) → '季度 报告 v2.tar.gz'（str）
  → 判据：os.path 的每个函数都是「字符串进、字符串出」，它不需要你懂类型，
    代价是返回值之间没有结构，dirname 和 basename 拼回去要你自己加斜杠。

=== 2. 拆名字：split / dirname / basename ↔ parent / name / parts（全标纯字符串）===
  表头：函数（同一批输入）→ os.path 的答案 ‖ pathlib 的写法 → 答案 ｜ 这一格归谁
    os.path.split(s)        → ('data/reports', '季度 报告 v2.tar.gz')    ‖ p.parent, p.name    → ('data/reports', '季度 报告 v2.tar.gz')   ｜ 纯字符串
    os.path.dirname(s)      → 'data/reports'                         ‖ str(p.parent)       → 'data/reports'   ｜ 纯字符串
    os.path.basename(s)     → '季度 报告 v2.tar.gz'                      ‖ p.name              → '季度 报告 v2.tar.gz'   ｜ 纯字符串
    os.path.split('a/b/')   → ('a/b', '')                            ‖ Path('a/b/').name   → 'b'   ｜ 纯字符串
    os.path.basename('a')   → 'a'                                    ‖ Path('a').parent    → '.'   ｜ 纯字符串
    os.path.splitdrive(s)   → ('', 'data/reports/季度 报告 v2.tar.gz')   ‖ p.drive, p.root     → ('', '')   ｜ 纯字符串
    （os.path 里没有）           → "只能自己 split('/')"                      ‖ p.parts             → ('data', 'reports', '季度 报告 v2.tar.gz')   ｜ 纯字符串
    （os.path 里没有）           → '只能自己切后缀'                              ‖ p.suffixes          → ['.tar', '.gz']   ｜ 纯字符串
  尾斜杠那条值得多看一眼：os.path 眼里 'a/b/' 的 basename 是 ''（空串），
    pathlib 眼里 Path('a/b/') == Path('a/b') 是 True —— 它先把尾斜杠吃掉再比较。
    而单文件名 'a'：dirname 给你 ''（空串），Path('a').parent 给你 '.'（当前目录）
  → 判据：这两套「空值」语义不同。os.path 用空串表示「没有父目录」，
    pathlib 用 Path('.') 表示；把 dirname 的结果直接喂给下一层 join 之前，得想清楚是哪一种。
```

表里三处「同一件事、两种说法」值得停下来：尾斜杠那条 `os.path.split('a/b/')` 给的 basename 是**空串**，
而 `Path('a/b/').name` 给 `'b'`（它先把尾斜杠吃掉再算，所以 `Path('a/b/') == Path('a/b')` 是 `True`）；
单文件名那条 `dirname('a')` 给空串，`Path('a').parent` 给 `.`（当前目录）。两边的「没有父目录」根本不是
一个值 —— 把 `dirname` 的结果直接喂给下一层拼接之前，得先想清楚你手里是哪一个空。
另外 `p.parts` 和 `p.suffixes` 这两格 os.path 里压根没有对应函数，只能自己 `split('/')`、自己切后缀。

接下来这一条是本章少数「必须亲眼看到一次」的规矩，因为它能直接变成安全漏洞：

```
=== 3. ★join：绝对片段会把前面整段吞掉，两个 API 一样狠 ===
  os.path.join('/a', '/b')        → '/b'   ← '/a' 被丢掉，不是拼成 '/a/b'
  str(Path('/a') / '/b')          → '/b'   ← pathlib 一模一样
  对比正常片段：os.path.join('/a', 'b') → '/a/b' ｜ Path('/a') / 'b' → '/a/b'
  ★这就是「把用户给的路径 join 进自己的目录」时的越界写法：只要那一段以 / 开头，
    你的前缀当场作废。两个 API 都不挡这条路，所以必须自己先拒绝绝对路径（第 10 节 p.is_absolute()）。
  中间给空串：os.path.join('a', '', 'b') → 'a/b' ｜ Path('a') / '' / 'b' → 'a/b'
  join 收 None 的原文：
    os.path.join('/a', None) → TypeError: join() argument must be str, bytes, or os.PathLike object, not 'NoneType'
    Path('/a') / None      → TypeError: unsupported operand type(s) for /: 'PosixPath' and 'NoneType'
  → 判据：两边的报错措辞不同，但都是 TypeError 不是 ValueError —— 传错类型就是一次编程错误，
    别用 except Exception 把它和「文件不存在」混成一锅（呼应第 07 章那棵类树）。
```

`os.path.join('/a', '/b')` 的答案是 `'/b'`，不是 `'/a/b'` —— 只要拼进去的那一段以 `/` 开头，
你前面的整段前缀当场作废。`Path('/a') / '/b'` 一模一样，两个 API 都不挡这条路。所以「把用户给的路径
拼进自己的目录」这件事必须先自己拒绝绝对路径（`p.is_absolute()` 就是干这个的），指望 join 帮你把关是没有的。
右边那两段 `TypeError` 原文措辞不同，但都是 `TypeError` 而不是 `ValueError`：传错类型是编程错误，
别用 `except Exception` 把它和「文件不存在」搅成一锅（第 07 章那棵类树）。

后缀这一格也是并排着才看得清：`os.path.splitext('.bashrc')` 给 `('.bashrc', '')`，它认为第一个点属于名字，
所以这个文件「没后缀」；`Path('.bashrc').suffix` 也给 `''`、`.name` 给 `'.bashrc'` —— 答案一致，含义不同。
多点后缀 `报告.tar.gz`：`splitext` 只切最后一个点（`('报告.tar', '.gz')`），`Path.suffix` 同样只给 `'.gz'`，
但 `suffixes` 会把整串列出来（`['.tar', '.gz']`）。想按后缀给文件分类，看 `suffixes`，别把 `splitext` 的
第二段当成「扩展名」。

归一化这一组最容易记错，因为它一次给你五条不同的答案：

```
=== 5. normpath：一个折叠一个不折叠；PurePath 还能在非本平台上算 ===
  输入 'a//b/./c/../d'
    os.path.normpath → 'a/b/d'   ← 折叠：// 和 ./ 消失，c/.. 抵消   ｜ 纯字符串
    Path(s).parts    → ('a', 'b', 'c', '..', 'd')   ← 不折叠 '..'（但 // 和 ./ 被切段时消化了）｜ 纯字符串
    str(Path(s))     → 'a/b/c/../d'   ← pathlib 有自己的归一化，和 normpath 不等价    ｜ 纯字符串
    os.path.abspath  → '<当前目录>/a/b/d'   ← 折叠 + 拼 cwd（<当前目录> 是占位符）｜ 要一次 getcwd，不查这个路径
    Path.absolute()  → '<当前目录>/a/b/c/../d'   ← 只拼 cwd，**不折叠** '..'      ｜ 同上
    os.path.realpath → '<当前目录>/a/b/d'   ← 折叠 + 解符号链接      ｜ 问内核
    Path.resolve()   → '<当前目录>/a/b/d'   ← 和 realpath 同一条路   ｜ 问内核
  证明上面 mask 过的那几行确实以 cwd 开头（否则替换就是假的）→ True；resolve 和 absolute 的区别一个布尔：True
  跨平台算路径（本机是 Linux，一个字节的 Windows 文件系统都没碰）：
    PureWindowsPath('C:/x/y.txt').parts → ('C:\\', 'x', 'y.txt') ｜ drive → 'C:' ｜ name → 'y.txt'   ｜ 纯字符串
    同一个字符串交给 PosixPath：('C:', 'x', 'y.txt')（盘符被当成一段目录名）
  PurePath 和 Path 的差别只在有没有 IO 方法：PureWindowsPath 没有 exists() → False；Path 有 → True
  而带 IO 的那一支**不许你在错的平台上造**（这是本机原文，注意异常类型不是 OSError）：
    WindowsPath('C:/x') → UnsupportedOperation: cannot instantiate 'WindowsPath' on your system
  → 判据：算「另一种平台」的路径就用 Pure*Path —— 它不碰内核，所以既不需要那台机器存在，
    也不会因为「这机器上没有」而报错；想对它做 IO，解释器先拦你。
```

同一个输入 `'a//b/./c/../d'`：`os.path.normpath` 折叠成 `'a/b/d'`（`//`、`./` 消失，`c/..` 抵消），
而 `str(Path(s))` 给的是 `'a/b/c/../d'` —— pathlib 有自己的归一化，**和 normpath 不等价**，
`parts` 里那个 `'..'` 也还活着。`Path.absolute()` 只拼 cwd、不折叠 `..`；要折叠得 `os.path.abspath`。
真正会展开符号链接的只有右边两条（`realpath` / `resolve`），它们要问内核。屏幕上那几行的开头被写成
`<当前目录>`，那是 `os.getcwd()` 的占位符，脚本还专门断言了一句
「证明上面 mask 过的那几行确实以 cwd 开头（否则替换就是假的）→ True」，替换不是假的。

最后一段跨平台算路径很值得记住：本机是 Linux，一个字节 Windows 文件系统都没碰，但
`PureWindowsPath('C:/x/y.txt').parts` 照样算得出 `('C:\\', 'x', 'y.txt')`、`drive` 是 `'C:'`；
同一个字符串交给 `PosixPath` 就变成 `('C:', 'x', 'y.txt')`，盘符被当成一段普通目录名。
`PureWindowsPath` 没有 `exists()` 方法，所以它连「这机器上没有 Windows 路径」这件事都不会报错；
而带 IO 的那一支 `WindowsPath('C:/x')` 当场 `UnsupportedOperation`（注意异常类型不是 `OSError`）。
**要算另一种平台的路径就用 `Pure*Path`** —— 这正是「纯字符串」那一侧的用处。

现在把问内核那一半也并排一次：

```
=== 6. 问内核那一组：exists / isfile / isdir / getsize ↔ is_file / is_dir / stat ===
  表头：函数 → os.path 的答案 ‖ pathlib 的写法 → 答案 ｜ 这一格归谁
    os.path.exists(p)  → True         ‖ p.exists()          → True   ｜ 问内核
    os.path.isfile(p)  → True         ‖ p.is_file()         → True   ｜ 问内核
    os.path.isdir(p)   → False        ‖ p.is_dir()          → False   ｜ 问内核
    os.path.isdir(目录)  → True         ‖ p.is_dir()          → True   ｜ 问内核
    os.path.getsize(p) → 10           ‖ p.stat().st_size    → 10   ｜ 问内核
    os.path.islink(p)  → False        ‖ p.is_symlink()      → False   ｜ 问内核
    os.path.exists(s)  → False        ‖ P.exists()          → False   ｜ 问内核
  getsize 和 stat().st_size 是同一个数的两条路 → True（getsize 内部就是 os.stat 那一次调用，只多包了一层取 st_size）
  对不存在的文件，两边的原文（一个字都不改）：
    os.path.getsize(s) → FileNotFoundError: [Errno 2] No such file or directory: 'data/reports/季度 报告 v2.tar.gz'
    Path.stat()        → FileNotFoundError: [Errno 2] No such file or directory: 'data/reports/季度 报告 v2.tar.gz'
  → 判据：这一组里 os.path 和 pathlib 是同一个 syscall 的两层皮，报错也一样 ——
    所以「exists 换 is_file」不会让程序更稳，只是换个写法（别为改而改）。

=== 7. relpath / isabs：一个不响一个响，这是最容易被换错的一对 ===
  输入：完整路径 'ospath-demo/真文件.txt'（相对沙箱），基准 'ospath-demo'
    os.path.relpath(p, start) → '真文件.txt'   ｜ 纯字符串（但要一次 getcwd 当默认基准）
    p.relative_to(基准)       → '真文件.txt'   ｜ 纯字符串，返回 PosixPath
    两者相等吗 → True
  ★换个不相关的基准，两边立刻分道扬镳：
    os.path.relpath(p, 'other') → '../ospath-demo/真文件.txt'   ← 它不响，给你一串 ../
    p.relative_to('other')  → ValueError: 'ospath-demo/真文件.txt' is not in the subpath of 'other'   ← 它只承认「真的在我里面」
  os.path.isabs(s) → False / os.path.isabs(A) → True ｜ P.is_absolute() → False / Path(A).is_absolute() → True   ｜ 纯字符串
  → 判据：relpath 是「算出怎么走过去」（可能全是 ../，也可能是错的），relative_to 是「断言它在我下面」。
    拿 relpath 的结果去当「安全前缀校验」是漏洞；要用 relative_to / is_relative_to（不响的那个）。
```

`os.path.getsize(p)` 和 `p.stat().st_size` 打出来的都是 `10`，因为 `getsize` 内部就是那一次 `os.stat`，
只多包了一层取字段；对不存在的文件，两边的报错原文**一字不差**
（两条都是 `FileNotFoundError: [Errno 2] No such file or directory:`，后面跟的是同一个路径字符串）。
所以「把 exists 换成 is_file」不会让程序更稳，只是换个写法。

最容易被换错的是最后一对：`os.path.relpath(p, 'other')` 不响，给你一串 `'../ospath-demo/真文件.txt'`；
`p.relative_to('other')` 直接 `ValueError: 'ospath-demo/真文件.txt' is not in the subpath of 'other'`。
差别不是脾气，是语义：
`relpath` 是「算出怎么走过去」（可能全是 `../`），`relative_to` 是「断言它真的在我下面」。
拿 `relpath` 的结果当安全前缀校验是漏洞，要用会响的那一个（`relative_to` / `is_relative_to`）。

**带走什么**：`os.path` 是「字符串的几何」，pathlib 是「带着类型的路径」，而它们唯一共享的地基是那组
syscall。判断一段代码该不该改，只看它在问什么：算名字（`parts / name / suffix / parent / splitext / join /
normpath / relpath`）在哪边都是纯字符串，路径不存在也照算；问东西在不在、是什么类型（`exists / isfile /
getsize / realpath / resolve`）在哪边都是问内核，照样抛 `FileNotFoundError`。分清这两层，
换不换 API 就只是风格问题；分不清，换 API 只是把坑挪了个位置。顺带两条钉子：
`join` 会把绝对片段前面的整段吞掉，`os.fspath(p)` 比 `str(p)` 严格得多（写收路径参数的函数，
入口第一件事是 `os.fspath(p)`）。

---

## 第 10 节 · 幂等建目录、缺失不删炸，以及权限位怎么挡你

部署脚本跑第二遍就炸，多半是这节前两行的事：`mkdir` 默认不幂等、`unlink` 默认不容错。
但这一节真正难的是后半段：为什么 `mode=0o777` 建出来的目录是 `0o755`，为什么一个 `0o444` 的只读文件
照样删得掉。这两件事都归 Unix 的权限模型管，而它管的东西跟直觉差着一层。
还有一条主轴要在这里兑现：`mkdir / unlink / rmdir / open` 全都属于「问内核」那一侧，
所以它们抛的不是「统一的一个错」，而是 `OSError` 下面几个**平行儿子**，各挂一个 errno。

```
$ python3 i_mkdir_unlink.py
=== 1. mkdir：默认不幂等，三层靠 parents，缺父级当场炸 ===
  已存在时再 one.mkdir()          → FileExistsError: [Errno 17] File exists: 'mkdir-demo/一层'｜errno 17 = EEXIST（File exists）
                               类树 FileExistsError → OSError → Exception
  mkdir(exist_ok=True)       → 静默成功（没抛，一层/ 还在 → True）
  mkdir(parents=True) 三层     → 建成：'mkdir-demo/甲/乙/丙'（4 段路径，一次调用）
  不带 parents 建同样三层           → FileNotFoundError: [Errno 2] No such file or directory: 'mkdir-demo/单/层/深'｜errno 2 = ENOENT（No such file or directory）
                               类树 FileNotFoundError → OSError → Exception
  → 判据：parents=True 就是「缺哪层补哪层」，内部是对自己 parent 递归调用；下面是 pathlib 真源码：
      os.mkdir(self, mode) self.parent.mkdir(parents=True, exist_ok=True)
  exist_ok=True 会不会顺手放过一个同名**文件**？（先把这个名字占上）
  文件已存在时 mkdir()             → FileExistsError: [Errno 17] File exists: 'mkdir-demo/一个文件.txt'｜errno 17 = EEXIST（File exists）
                               类树 FileExistsError → OSError → Exception
  文件已存在时 mkdir(exist_ok=True) → FileExistsError: [Errno 17] File exists: 'mkdir-demo/一个文件.txt'｜errno 17 = EEXIST（File exists）
                               类树 FileExistsError → OSError → Exception
    → 判据：exist_ok 的判据是「已经是个目录」（源码里那句 not self.is_dir()），不是「名字被占了」。
```

四行原文把 `mkdir` 的脾气说完了：已存在时再 `mkdir()` 是 `FileExistsError`（errno 17 = EEXIST）；
`exist_ok=True` 静默成功；`parents=True` 一次调用建成 `mkdir-demo/甲/乙/丙` 这种四段路径；
不带 `parents` 建同样的三层就是 `FileNotFoundError`（errno 2 = ENOENT）。脚本还把 `parents=True` 的
pathlib 真源码贴了出来 —— 内部就是对 `self.parent` 递归调用 `mkdir(parents=True, exist_ok=True)`，
没什么魔法。最后一问最容易踩：`exist_ok=True` **不会**放过一个同名普通文件，照样 `FileExistsError`。
因为 `exist_ok` 的判据是源码里那句 `not self.is_dir()` ——「已经是个目录」才算成功，「名字被占了」不算。

权限位这一屏是本章被问最多的问题（「脚本建出来的目录别人为什么写不了」）的根因：

```
=== 2. ★mode= 打不过 umask：本机 0o777 建出来的目录照样是 0o755 ===
  进脚本时的 umask = 0o22（各人 shell 不同），脚本已把它钉成 0o022 —— 下面这几个八进制数才敢写进教材
  mkdir()                    → 0o755
  mkdir(mode=0o777)          → 0o755   ← 和上一行一模一样：777 里那两个写位被 umask 减掉了
  mkdir(mode=0o700)          → 0o700   ← 这条守住了：umask 0o022 只减组和其他的写位，不碰 owner
  write_text 新建文件            → 0o644   ← 文件模板 0o666、目录模板 0o777，都要再减一次 umask
  mkdir(mode=0o777) 之后再 chmod → 0o777   ← 想要 777 只有这条路
  → 判据：mode= 是「你想要的位」，umask 是「一定要减的位」，两者是**与非**关系（mode & ~umask）。
  ★所以「脚本建出来的目录别人写不了」这类问题，根因常常在 umask（cron、systemd、Docker 构建层里
    umask 常常是 0o077 或 0），不在 mkdir 那一行 —— 要么显式 chmod，要么别依赖别人能写。
```

脚本进门前先把 `umask` 钉成 `0o022`（各人 shell 里那个值不一样，不钉住这些八进制数就不敢写进教材），
然后：`mkdir()` → `0o755`，`mkdir(mode=0o777)` → **还是** `0o755`（777 里那两个写位被 umask 减掉了），
`mkdir(mode=0o700)` → `0o700`（umask 0o022 只减组和其他的位，不碰 owner，所以这条守住了），
`write_text` 新建文件 → `0o644`（文件模板 0o666、目录模板 0o777，都要再减一次 umask），
`mkdir(mode=0o777)` 之后再 `chmod` → `0o777`（想要 777 只有这条路）。
关系是**与非**：`mode & ~umask`。`mode=` 是「你想要的位」，`umask` 是「一定要减的位」，
umask 只会往上收、不会放开。cron、systemd、Docker 构建层里的 umask 常常是 `0o077` 或 `0`，
跟你的交互 shell 不是一个值 —— 同一段脚本换个地方建出不同权限，根因就在这行上，不在 `mkdir` 那一行。

下面这三行是给「`except OSError: pass`」准备的反例：

```
=== 3. parents=True 撞在一个同名普通文件上：本机答案不是 FileExistsError ===
  现场：'mkdir-demo/挡住.txt' 是一个普通文件，要建 'mkdir-demo/挡住.txt/子/孙'（三个 errno 各是一条不同的路）
  blocker.mkdir()（名字被占）      → FileExistsError: [Errno 17] File exists: 'mkdir-demo/挡住.txt'｜errno 17 = EEXIST（File exists）
                               类树 FileExistsError → OSError → Exception
  behind.mkdir()（不带 parents） → NotADirectoryError: [Errno 20] Not a directory: 'mkdir-demo/挡住.txt/子/孙'｜errno 20 = ENOTDIR（Not a directory）
                               类树 NotADirectoryError → OSError → Exception
  behind.mkdir(parents=True) → NotADirectoryError: [Errno 20] Not a directory: 'mkdir-demo/挡住.txt/子/孙'｜errno 20 = ENOTDIR（Not a directory）
                               类树 NotADirectoryError → OSError → Exception
  → 判据：EEXIST(17) 说「这个名字已经有东西了」，ENOTDIR(20) 说「中间有个不是目录的东西」；
    上面三行的类树里没有一条是另一条的儿子 —— FileExistsError 和 NotADirectoryError 是
    OSError 的两个平行儿子。只知道「mkdir 会抛 OSError」就写 except OSError: pass，等于把
    「改个名」和「先删掉占位的文件」两种完全不同的处置，压成了一次静默失败。
```

现场是一个叫 `mkdir-demo/挡住.txt` 的普通文件，你要在它底下建 `挡住.txt/子/孙`。
`blocker.mkdir()`（名字本身被占）→ `FileExistsError` errno 17；`behind.mkdir()`（不带 parents）→
`NotADirectoryError` errno 20 = ENOTDIR；加了 `parents=True` → **还是** `NotADirectoryError` errno 20。
三个 errno 各是一条路，而且类树里 `FileExistsError` 和 `NotADirectoryError` 是 `OSError` 的两个平行儿子，
互相不是父子。所以「mkdir 会抛 OSError 所以我 `except OSError: pass`」把「换个名」和
「先删掉占位的文件」两种完全不同的处置压成了一次静默失败。

删这一侧的规则差不多，但多一个坑：返回值不可信，`is` 也不可信。

```
=== 4. 删：unlink 三格、rmdir 两格，外加三个名字是不是同一件事 ===
  正常 unlink()                → 之前 exists True ｜ 之后 exists False ｜ 返回值 = None（别拿返回值判断，要么看 exists 要么接异常）
  对不存在的 unlink()             → FileNotFoundError: [Errno 2] No such file or directory: 'mkdir-demo/没这个.txt'｜errno 2 = ENOENT（No such file or directory）
                               类树 FileNotFoundError → OSError → Exception
  unlink(missing_ok=True)    → 静默成功，而且没有顺手把文件建出来 → False
  对非空目录 rmdir()              → OSError: [Errno 39] Directory not empty: 'mkdir-demo/非空'｜errno 39 = ENOTEMPTY（Directory not empty）
                               类树 OSError → Exception → BaseException
  对不存在的目录 rmdir()            → FileNotFoundError: [Errno 2] No such file or directory: 'mkdir-demo/根本没这个目录'｜errno 2 = ENOENT（No such file or directory）
                               类树 FileNotFoundError → OSError → Exception
  → 判据：rmdir 没有 exist_ok、也没有 force —— POSIX 的 rmdir(2) 本来就只允许删空目录。
    想递归删是 shutil.rmtree，那是另一个决定（它会一路删到你给的这个名字），本章第 12 节讲。
  三个名字是不是同一件事（本机实测，跟很多人以为的不一样）：
    os.unlink is os.remove → False   ← ★False！两个是**不同的 builtin 对象**；
      但 doc 互相指认：'Remove a file (same as unlink()).' / 'Remove a file (same as remove()).'
    Path.unlink 的源码里那一行：os.unlink(self)
    行为实测 os.unlink(...) → 文件还在吗 False   ← 两条路都是 unlink(2)，结果必然相同
    行为实测 os.remove(...) → 文件还在吗 False   ← 两条路都是 unlink(2)，结果必然相同
  → 判据：is 比的是**对象身份**，不是「做的事」；想知道是不是同一件事，看 errno 和效果，别看 is。
```

`unlink()` 正常删掉的返回值是 `None` —— 别拿返回值判断，要么之后看 `exists`，要么直接接异常。
对不存在的文件 `unlink()` 是 `FileNotFoundError`（errno 2），`missing_ok=True` 静默成功，
而且脚本专门验了「没有顺手把文件建出来 → False」。`rmdir` 只有两格：非空目录 →
`OSError: [Errno 39] Directory not empty`，不存在的目录 → `FileNotFoundError`；
POSIX 的 `rmdir(2)` 本来就只允许删空目录，所以 `rmdir` 既没有 `exist_ok` 也没有 force。
想递归删是 `shutil.rmtree`，那是另一个决定（第 13 节清理临时目录时就会撞见它）。
最有意思的是那行 `os.unlink is os.remove → False`：它俩是**不同的 builtin 对象**，
但 doc 互相指认「same as unlink() / same as remove()」，`Path.unlink` 源码里那一行是 `os.unlink(self)`，
行为实测两条路都把文件删掉了。判据：`is` 比的是对象身份，不是「做的事」。

★ 然后是本章最反直觉的一屏，它把「只读位就保护得住文件」这个常识打掉了一半：

```
=== 5. ★本章最反直觉：删文件看的是父目录的写权限，不是文件自己的 ===
  secret.txt 自己 chmod 成 0o444（只读，任何写都不许）｜父目录 locked/ 是 0o755
  删得掉吗 → True   ← 删的是**目录里那条名字映射**，不是文件的字节
  把这条钉死：同一个不可写的目录里放三个不同 mode 的文件，一次全撞：
    现在 locked/ 是 0o500（文件自己的位见左列）：
    unlink 位644.txt            → PermissionError: [Errno 13] Permission denied: 'mkdir-demo/locked/位644.txt'｜errno 13 = EACCES（Permission denied）
                               类树 PermissionError → OSError → Exception
    unlink 位777.txt            → PermissionError: [Errno 13] Permission denied: 'mkdir-demo/locked/位777.txt'｜errno 13 = EACCES（Permission denied）
                               类树 PermissionError → OSError → Exception
    unlink 位0.txt              → PermissionError: [Errno 13] Permission denied: 'mkdir-demo/locked/位0.txt'｜errno 13 = EACCES（Permission denied）
                               类树 PermissionError → OSError → Exception
    目录 chmod 回 0o700 之后三个全删掉 → True
    看不见/ 改成 0o300 之后：列目录当场响，按已知名字却删得掉：
      os.listdir(harder)         → PermissionError: [Errno 13] Permission denied: 'mkdir-demo/看不见'｜errno 13 = EACCES（Permission denied）
                               类树 PermissionError → OSError → Exception
      inside.unlink()            → 没抛（还在吗 → False）
      → 读位（r）管「能不能列出名字」，写位（w）管「能不能改目录项」，两条互不相干
  → 判据：Unix 上「删除」是对**目录**的写操作，所以 chmod 0o444 保护的是内容不被改，
    它保护不了「这个文件还存在」；要防删得动父目录，或者用只读文件系统/不可写挂载点。

=== 6. open() 的三种目录相关失败：三条原文、三个 errno、各自是谁的儿子 ===
  open(一个目录)                 → IsADirectoryError: [Errno 21] Is a directory: 'mkdir-demo'｜errno 21 = EISDIR（Is a directory）
                               类树 IsADirectoryError → OSError → Exception
  open(路径中间是文件)              → NotADirectoryError: [Errno 20] Not a directory: 'mkdir-demo/中间是文件.txt/子/x.txt'｜errno 20 = ENOTDIR（Not a directory）
                               类树 NotADirectoryError → OSError → Exception
  open(父目录不存在)               → FileNotFoundError: [Errno 2] No such file or directory: 'mkdir-demo/没这个目录/x.txt'｜errno 2 = ENOENT（No such file or directory）
                               类树 FileNotFoundError → OSError → Exception
  → 判据：三个 errno 各不相同，但三个类都是 OSError 的儿子，而且互相不是父子 ——
    所以 except FileNotFoundError 接不住 IsADirectoryError，写成 except OSError 又太宽。
  ★「路径中间是文件」（ENOTDIR 20）和「父目录不存在」（ENOENT 2）是同一个位置上的两种失败：
    看到 ENOENT 就下结论「文件被删了」，很可能其实是上游某个目录变成了文件。这三个都发生在
    open() 那一刻而不是读的那一刻 —— 打开阶段的错和读取阶段的错（编码），except 别写在一起。
```

`secret.txt` 自己 `chmod` 成 `0o444`（只读，任何写都不许），父目录 `locked/` 是 `0o755` ——
**删得掉吗 → True**。因为删除动的是「目录里那条名字映射」，不是文件的字节。
为了钉死这条，脚本在同一个不可写的目录（`locked/` 收成 `0o500`）里放了三个文件，
名字把各自的 mode 写了进去（`位644.txt`、`位777.txt`、`位0.txt`），一次全撞：
三个全是 `PermissionError` errno 13，跟文件自己的位**完全无关**；把目录 `chmod` 回 `0o700`，三个全删掉 → True。
再来一层：目录收成 `0o300`（没有读位）之后 `os.listdir()` 当场 `PermissionError`，
但拿着已知名字 `inside.unlink()` 却成功了 —— 读位（r）管「能不能列出名字」，写位（w）管「能不能改目录项」，
两条互不相干。所以 `chmod 0o444` 保护的是内容不被改，它保护不了「这个文件还存在」；要防删得动父目录，
或者用只读文件系统 / 不可写挂载点。

最后半屏是 `open()` 的三种目录相关失败，三个 errno 各不相同，但三个类都是 `OSError` 的儿子且互不为父子：
`open(一个目录)` → `IsADirectoryError` errno 21（EISDIR）；
`open('mkdir-demo/中间是文件.txt/子/x.txt')` →
`NotADirectoryError` errno 20；`open(父目录不存在)` → `FileNotFoundError` errno 2。
所以 `except FileNotFoundError` 接不住 `IsADirectoryError`，写成 `except OSError` 又太宽。
特别提醒中间那条：**看到 ENOENT 就下结论「文件被删了」，很可能其实是上游某个目录变成了文件**，
同一个位置上的两种失败而已。这三个都发生在 `open()` 那一刻，不是读的那一刻 ——
打开阶段的错和读取阶段的错（编码）别写进同一个 `except`。

**带走什么**：幂等三件套 —— ① 建目录 `mkdir(parents=True, exist_ok=True)`；② 删文件
`unlink(missing_ok=True)`；③ 删目录只有 `rmdir()`（非空必响）。三件套的共同语义是
「已经是我想要的样子」算成功，不算异常，重跑部署脚本不炸就靠它。「先 `if exists` 再动手」看着稳，
其实不如直接 `try`：查完到动手之间有一次竞态，而且多一条分支就多一条没被测过的路。
按 errno 分派比按异常类名分派精确，因为 errno 是整数、不随本机 `strerror` 的语言变：
`2` ENOENT（少一层 / 不存在）、`17` EEXIST（名字被占，可能是目录也可能是文件）、`20` ENOTDIR
（中间有个不是目录的东西）、`39` ENOTEMPTY（`rmdir` 非空）、`13` EACCES（权限位挡的，
要改的是父目录不是文件）、`21` EISDIR（把目录当普通文件开，回去查 `parts`）。
呼应第 07 章：`except` 匹配的是 `isinstance`，抓父亲就抓到全部儿子。

---

## 第 11 节 · 遍历、匹配与 stat 的字段

「`glob` 的结果为什么老变」「`*` 到底管多大范围」「`stat` 那一堆字段哪个能信」，这三个问题共用一个答案：
遍历顺序是文件系统给的，不是算法给的；`*` 的射程只有一个路径段；`stat` 里有的字段是身份、有的只是快照。
这一节的输出全部 `sorted()` 过 —— 不是好看，是因为**只有排过序的结果才能被逐字印进书里**。
脚本先造一棵固定的树（8 个文件 + 2 个子目录：`sub`、`sub/deeper`，文件名里带空格、带中文、
带点开头、带大小写两种后缀），把它的样子先打出来当基准：

```
$ python3 j_glob_stat.py
=== 1. 先造一棵固定的树（8 个文件 + 2 个子目录），列一遍确认它真是这个样子 ===
  写入的文件（sorted 后逐个打一行）：
    .env
    a.txt
    sub/b.txt
    sub/deeper/c.txt
    x.log
    y.LOG
    两个 词.txt
    数据.csv
  子目录: ['sub'] ｜ 全口径条目数（rglob('*')，含目录） = 10 ｜ 纯文件数 = 8

=== 2. 五种「列一下」一次打全：iterdir / glob('*') / glob('*.txt') / glob('**/*.txt') / rglob('*.txt') ===
  iterdir()          : ['.env', 'a.txt', 'sub', 'x.log', 'y.LOG', '两个 词.txt', '数据.csv']
  glob('*')          : ['.env', 'a.txt', 'sub', 'x.log', 'y.LOG', '两个 词.txt', '数据.csv']
  glob('*.txt')      : ['a.txt', '两个 词.txt']
  glob('**/*.txt')   : ['a.txt', 'sub/b.txt', 'sub/deeper/c.txt', '两个 词.txt']
  rglob('*.txt')     : ['a.txt', 'sub/b.txt', 'sub/deeper/c.txt', '两个 词.txt']
  → iterdir 和 glob('*') 的结果**一模一样**（都是「这一层的全部条目」，含目录本身）。
  → glob('**/*.txt') 和 rglob('*.txt') 也一模一样：rglob 就是 glob('**/' + pattern)。
  根级的 a.txt 在 '**/*.txt' 结果里吗 → True （本机实测：`**` 匹配**零层或多层**目录，不是「必须往下走一层」）
```

五种「列一下」里，`iterdir()` 和 `glob('*')` 的结果**一模一样**（都是这一层的全部条目，含目录本身 `sub`），
`glob('**/*.txt')` 和 `rglob('*.txt')` 也一模一样（`rglob` 就是 `glob('**/' + pattern)`）。
注意根级那个 `a.txt` 出现在 `'**/*.txt'` 的结果里（脚本专门验了一次 → `True`）：
`**` 匹配的是**零层或多层**目录，不是「必须往下走一层」。这条和很多人写的第一版直觉相反。

★ 匹配规矩有两条，第一条在两个同名 API 之间是**相反**的：

```
=== 3. ★ 匹配规矩一：点开头文件——两个 glob 在这里是**相反**的 ===
  pathlib 的 Path.glob('*')     : ['.env', 'a.txt', 'sub', 'x.log', 'y.LOG', '两个 词.txt', '数据.csv'] ｜ 里面有 .env 吗 → True
  glob 模块的 glob.glob('*')    : ['a.txt', 'sub', 'x.log', 'y.LOG', '两个 词.txt', '数据.csv'] ｜ 里面有 .env 吗 → False
  只写点号才拿得到：Path.glob('.*') → ['.env'] ｜ glob.glob('.*') → ['.env']
  ★ 这条要按本机事实纠正常见说法：「glob 的 `*` 不匹配点文件」只对 **glob 模块**成立；
    pathlib 的 Path.glob 实测**会**把 .env 一起给你。同一个 `*`，两套规矩。
  → 写「扫目录」的代码之前先问清楚用的是哪个 glob：漏文件（glob 模块）和多文件（pathlib）都是事故，而且方向相反。

=== 4. ★ 匹配规矩二：`*` 不跨越 `/`；顺手把大小写也实测了 ===
  glob('*')  条数 = 7  ｜ rglob('*')  条数 = 10
  glob('*.txt') 条数 = 2  ｜ rglob('*.txt') 条数 = 4
  glob('*/*.txt') : ['sub/b.txt']  ← 一个 `*` 只吃掉**一层**（sub）
  glob('*/*/*.txt'): ['sub/deeper/c.txt']  ← 想够到 deeper 就得再写一个 `*`
  → 上面这四行是同一个事实的四种说法：`*` 匹配「一个路径段里的任意字符」，路径分隔符 `/` 不在它的射程里；跨层只有 `**` 和 rglob 办得到。
  大小写实测（树里放了 x.log 和 y.LOG 两个文件）：
    glob('*.log')                        : ['x.log']
    glob('*.log', case_sensitive=False)  : ['x.log', 'y.LOG']
    glob('*.LOG')                        : ['y.LOG']
  → 默认 `case_sensitive=None` 跟随平台：本机 Linux/ext4 实测**区分大小写**，x.log 和 y.LOG 是两个不同的匹配结果。
  ★ 别写「.log 和 .LOG 是同一个后缀」的代码：Linux 上它俩就是两个文件；真要大小写无关，必须显式传 case_sensitive=False（3.13 起才有这个关键字）。

=== 5. os.walk 和 rglob 走同一棵树：条数相等，顺序**不保证** ===
  os.walk  走到的文件数 = 8  ｜ rglob 走到的文件数 = 8
  sorted 之后两份列表相等吗 → True
  sorted(os.walk) : ['.env', 'a.txt', 'sub/b.txt', 'sub/deeper/c.txt', 'x.log', 'y.LOG', '两个 词.txt', '数据.csv']
  rglob('*') 里的目录条目（walk 不进 files）: ['sub', 'sub/deeper']
  ★ 两边**原始迭代顺序**都不保证：os.walk 靠 readdir、rglob 靠 scandir，顺序由文件系统内部哈希决定，跟字母序、创建序都没关系。
    所以本脚本刻意不把原始顺序打在屏幕上——打出来就等于在赌「下次还这个顺序」，这一页明年就错了。要顺序，自己 sorted()。
```

同样是 `glob('*')`：`pathlib` 的 `Path.glob` 会把 `.env` 一起给你（→ `True`），
`glob` 模块的 `glob.glob` 不给你（→ `False`）。教科书上那句「glob 的 `*` 不匹配点文件」
**只对 glob 模块成立**，两边都得只写 `'.*'` 才拿得到 `.env`。写扫目录的代码之前先问清楚用的是哪个 glob：
漏文件（glob 模块）和多文件（pathlib）都是事故，而且方向相反。
第二条规矩：`*` 不吃 `/`。`glob('*')` 7 条、`rglob('*')` 10 条，`glob('*.txt')` 2 条、
`rglob('*.txt')` 4 条；`glob('*/*.txt')` 只有 `['sub/b.txt']`（一个 `*` 只吃掉一层），
够到 `deeper` 就得再写一个 `*`。大小写也当场测了：`glob('*.log')` → `['x.log']`，
加了 `case_sensitive=False` → `['x.log', 'y.LOG']`（这个关键字 3.13 起才有）。
本机 Linux/ext4 默认就是区分大小写，`.log` 和 `.LOG` 是两个文件 —— 别写「它们是同一个后缀」的代码。
最后，`os.walk` 和 `rglob` 走到的文件数都是 8，`sorted` 之后两份列表相等 → `True`；
但脚本**刻意没把原始迭代顺序打在屏幕上**：`os.walk` 靠 `readdir`、`rglob` 靠 `scandir`，
顺序由文件系统内部哈希决定，跟字母序、创建序都没关系。把原始顺序印出来就等于在赌「下次还这个顺序」，
这一页明年就错了。要顺序，自己 `sorted()`。

`stat` 那一屏把三种对象并排打（普通文件 / 目录 / 符号链接），只有链接那一行是真知识点：

```
=== 6. stat 字段屏：三种对象并排（普通文件 / 目录 / 符号链接）===
  普通文件   a.txt
    权限   stat → 0o644   filemode → '-rw-r--r--'   ｜ lstat → 0o644   filemode → '-rw-r--r--'
    类型   stat (REG,DIR,LNK) → (True, False, False) ｜ lstat (REG,DIR,LNK) → (True, False, False)
    链接数 st_nlink stat=1 ｜ lstat=1   ｜  大小 st_size stat=1 ｜ lstat=1
  目录     sub
    权限   stat → 0o755   filemode → 'drwxr-xr-x'   ｜ lstat → 0o755   filemode → 'drwxr-xr-x'
    类型   stat (REG,DIR,LNK) → (False, True, False) ｜ lstat (REG,DIR,LNK) → (False, True, False)
    链接数 st_nlink stat=3 ｜ lstat=3   ｜  大小 st_size stat=None(目录不打) ｜ lstat=None(目录不打)
  符号链接   link
    权限   stat → 0o644   filemode → '-rw-r--r--'   ｜ lstat → 0o777   filemode → 'lrwxrwxrwx'
    类型   stat (REG,DIR,LNK) → (True, False, False) ｜ lstat (REG,DIR,LNK) → (False, False, True)
    链接数 st_nlink stat=1 ｜ lstat=1   ｜  大小 st_size stat=1 ｜ lstat=5
  → 符号链接那一行是重点：stat() 跟着链接走，看到的是**目标**（0o644、st_size=1，就是 a.txt 那个字节）；
    lstat() 不跟，看到的才是链接自己（0o777、lrwxrwxrwx、st_size=5 = 目标名字 'a.txt' 的字节数）。
  → 目录的 st_nlink = 3，不是「谁指着它」，而是 2 + 直接子目录数（. 和 .. 各算一个）—— 这个字段对目录没有日常意义。
  ★ 目录的 st_size 上面故意没打值：它是「目录项存储占的字节数」，本机实测同一条路径会因为里面多了几条记录从 60 跳到 1060。任何把它当大小用的代码都是错的。
  ★ 后缀名不能当类型用，现场做三个都叫 *.txt 的东西给你看：
    a.txt     S_ISREG=True  S_ISDIR=False S_ISLNK=False filemode='-rw-r--r--'
    dir.txt   S_ISREG=False S_ISDIR=True  S_ISLNK=False filemode='drwxr-xr-x'
    link.txt  S_ISREG=False S_ISDIR=False S_ISLNK=True  filemode='lrwxrwxrwx'
    两个 词.txt  S_ISREG=True  S_ISDIR=False S_ISLNK=False filemode='-rw-r--r--'
    → 判类型永远用 stat.S_ISREG / S_ISDIR / S_ISLNK 查 st_mode 的比特位，不看后缀。
  时间字段本段一个都没打。它们就在那儿（只打名字，不打值）： ['st_atime', 'st_mtime', 'st_ctime'] —— 读一次文件 atime 就可能变，把值写进书里第二天就对不上。
```

`link` 这一行两条路给出两个完全不同的东西：`stat()` 跟着链接走，看到的是**目标**
（`0o644`、`-rw-r--r--`、`st_size=1`，就是 `a.txt` 那一个字节）；`lstat()` 不跟，看到的才是链接自己
（`0o777`、`lrwxrwxrwx`、`st_size=5` = 目标名字 `'a.txt'` 的字节数）。
目录的 `st_nlink = 3` 不是「谁指着它」，而是 `2 + 直接子目录数`（`.` 和 `..` 各算一个），这个字段对目录
没有日常意义。目录的 `st_size` 上面故意没打值：它是「目录项存储占的字节数」，本机实测同一条路径会因为
里面多了几条记录从 `60` 跳到 `1060` —— 任何把它当大小用的代码都是错的。
下面那四个都叫 `*.txt` 的东西（普通文件、目录、符号链接、带空格中文名）证明后缀不能当类型：
判类型永远用 `stat.S_ISREG / S_ISDIR / S_ISLNK` 查 `st_mode` 的比特位。
时间字段这一节一个值都没打，只打了名字 `['st_atime', 'st_mtime', 'st_ctime']` ——
读一次文件 `atime` 就可能变，把值写进书里第二天就对不上。

```
=== 7. st_size 和「占了多少盘」是两个数：现场造一个稀疏文件 ===
  写了几个字节 = 1 ｜ st_size = 1048577 ｜ st_blocks*512 = 4096
  占盘 / 逻辑大小 = 0.00391 ｜ 差值 = 1044481 字节
  注意 st_blocks 的单位是**固定的 512 字节**（POSIX 规定），不是本文件系统的块大小；本机 statvfs 的 f_frsize = 4096 字节
  → 所以 `du` 和 `ls -l` 能对不上号：ls 报的是 st_size，du 报的是 st_blocks*512。
  ★ 用 st_size 估「这份文件要占多少空间/拷多久」是错的：稀疏文件、带洞的数据库镜像、`cp --sparse=always` 的产物都能让两个数差几个量级。

=== 8. 是不是同一个文件：samefile 和 == 给出相反的答案 ===
  两个硬链接名   str 相等（p1 == p2）→ False  ｜ p1.samefile(p2) → True
  链接数证明     h1.stat().st_nlink = 2  ｜ unlink 掉 h2 之后 = 1
  只打布尔，不打 inode 数值：st_ino 相等 → True ｜ st_dev 相等 → True
  符号链接指过来  link.samefile(a.txt) → True  ｜ 字符串相等 → False
  两个真不同的文件 samefile → False  ｜ st_ino 相等 → False
  → samefile 判的是 (st_dev, st_ino) 这一对，不看路径字符串；Windows 上这对换成「卷序列号 + 文件 ID」，**本机未验证**，别当事实引用。
  Path.lstat 本机 3.14 存在吗 → True ；link 的 lstat().st_mode 是 LNK 吗 → True （老版本没有它就用 os.lstat）
```

稀疏文件这一段把「大小」和「占盘」彻底拆开：只写了 `1` 个字节，`st_size` 是 `1048577`，
而 `st_blocks*512` 只有 `4096`，占盘 / 逻辑大小 = `0.00391`，差值 `1044481` 字节。
`st_blocks` 的单位是**固定的 512 字节**（POSIX 规定），不是本文件系统的块大小（本机 `statvfs` 的
`f_frsize = 4096`）。这就是 `du` 和 `ls -l` 能对不上号的原因：`ls` 报 `st_size`，`du` 报 `st_blocks*512`。
用 `st_size` 估「这份文件占多少空间 / 拷多久」是错的，稀疏文件、带洞的数据库镜像、
`cp --sparse=always` 的产物都能让这两个数差几个量级。

`samefile` 和 `==` 给出相反的答案：两个硬链接名 `p1 == p2` → `False`，`p1.samefile(p2)` → `True`；
证据是 `st_nlink` 从 `2` 掉到 `1`（`unlink` 掉第二个名字之后）。这一段只打布尔不打 inode 数值，
因为 `st_ino` 每次跑都可能不同，但它「相等 → True」这个判断是稳定的。符号链接指过来 `samefile` 也是 `True`。
⚠️ 脚本在这里明写了一条边界：`samefile` 判的是 `(st_dev, st_ino)` 这一对，Windows 上这对换成
「卷序列号 + 文件 ID」，**这条本机未验证**，别当事实引用。`Path.lstat` 在本机 3.14 存在吗 → `True`
（老版本没有它就用 `os.lstat`）。

**带走什么**：四条判据。① `glob` / `rglob` / `os.walk` 都不保证顺序，要可复现就得自己 `sorted()`；
② `*` 不吃 `/`（跨层要 `**` 或 `rglob`），而点文件归不归 `*` 管，glob 模块和 pathlib 的答案相反，
先确认用的是哪个 glob 再谈行为；③ 类型看 `st_mode` 的比特位，不看后缀；链接要看这个数字是 `stat` 到的
还是 `lstat` 到的，两者的 `st_size` 不是一个东西；④ 大小 ≠ 占盘（`st_size` vs `st_blocks*512`），
是不是同一个文件看内核身份 `(st_dev, st_ino)`，不看路径字符串 —— 扩展名、路径字符串、`ls -l` 的字节数，
三样都靠不住。

---

## 第 12 节 · read_text / write_text 的最短路径和它的三个坑

`p.write_text(s)` 一行、`p.read_text()` 一行，这是本章最短的读写路径。短到没有参数可写，
就意味着它替你做了三个决定，而这三个决定各有代价：**追加没了、编码外包给环境变量、句柄谁来关没人管**。
这一节把这三个坑各演一次现场。顺便注意主轴上的一格：`write_text` 返回的是**字符数**，
这个数字经常被当成字节数用。

```
$ python3 k_read_write_text.py
=== 1. 最短路径就两行：write_text 写完，read_text 读回；先看它返回的是什么 ===
  write_text('内容\n') 的返回值 = 3 ｜ 类型 = int
  read_text(encoding='utf-8') 读回 = '内容\n' ｜ 字符数 = 3
  ★ 中文那一次实测：write_text('中文abc') 返回 = 5 个字符 ｜ 但磁盘上真实落的是 9 字节
  → 返回值是**字符数**（len(data)），不是写进磁盘的字节数；两者只在纯 ASCII 时相等。
    纯中文时字节数是字符数的三倍（UTF-8 一个汉字 3 字节）——这个数不能拿去核对字节。

=== 2. ★ 坑 1：write_text 是截断写，不是追加写 ===
  第一次写 30 个字符 → 磁盘 30 字节 ｜ 第二次写 1 个字符 → 磁盘 3 字节
  现在文件里剩下的内容 = '短' ｜ 长度 = 1
  → 前 30 个字符**一个字都没剩**。write_text 内部就是 open(p, 'w')：'w' 在打开那一刻 truncate。
  本机 3.14 的签名（inspect 实测，一字不改）：write_text (self, data, encoding=None, errors=None, newline=None)
  里面有 'mode' 这个参数吗 → False ｜ 里面有 'append' 吗 → False
  硬塞一个 mode= 试试 → TypeError: Path.write_text() got an unexpected keyword argument 'mode'
  ★ 本机事实：3.14 的 write_text **没有** mode= 也没有 append=。
    谁在代码里写 `p.write_text(s, mode='a')`，它不会变成追加，只会当场 TypeError。

=== 3. 想追加只有一条路：自己 open(..., 'a')（同一个文件，接着上一节的状态）===
  open(p,'a') 写 1 个字符前后：磁盘字节数 3 → 6 ｜ 现在的内容 = '短续'
  再 write_text 一次：磁盘字节数 6 → 15 ｜ 内容 = '重来的内容' ← 「续」又没了
  → 追加语义在 open 的模式字符里，不在 write_text 的参数里。要追加就自己管 with。
```

`write_text('内容\n')` 的返回值是 `3`、类型 `int`，`read_text(encoding='utf-8')` 读回 `'内容\n'`，
字符数也是 `3`。看着像字节数，但中文那次实测露馅了：`write_text('中文abc')` 返回 `5` 个字符，
磁盘上真实落的是 `9` 字节（UTF-8 一个汉字 3 字节）。返回值是 `len(data)`，只在纯 ASCII 时才和字节数相等，
**这个数不能拿去核对字节**。

坑 1：`write_text` 是截断写，不是追加写。先写 30 个字符（磁盘 30 字节），再写 1 个字符（磁盘 3 字节），
文件里剩下的只有 `'短'`（长度 1）—— 前面 30 个字符一个字都没剩。因为 `write_text` 内部就是 `open(p, 'w')`，
而 `'w'` 在**打开那一刻**就 truncate，还没等你写。脚本还用 `inspect` 把本机 3.14 的签名一字不改打出来：
`write_text(self, data, encoding=None, errors=None, newline=None)` —— 里面没有 `mode`（→ `False`）、
没有 `append`（→ `False`），硬塞一个 `mode=` 就是 `TypeError: Path.write_text() got an unexpected
keyword argument 'mode'`。所以谁在代码里写 `p.write_text(s, mode='a')`，它不会变成追加，只会当场炸。
想追加只有一条路：自己 `open(p, 'a')` 并管好 `with`。脚本接着上一段的状态试了：`open(p,'a')` 写 1 个字符，
磁盘从 `3` 变 `6`，内容 `'短续'`；再 `write_text` 一次，磁盘 `6` 变 `15`，「续」又没了。
**追加语义在 `open` 的模式字符里，不在 `write_text` 的参数里。**

坑 2 是最隐蔽的一个，因为本机它「侥幸是对的」：

```
=== 4. ★ 坑 2：不写 encoding 时，默认值从哪来（本机能证明，换 locale 立刻变）===
  本进程的默认值：sys.getfilesystemencoding() = utf-8 ｜ locale.getpreferredencoding(False) = UTF-8
  write_text('中文') 不写 encoding → 磁盘字节 = e4b8ade69687
  read_text() 不写 encoding 读回来相等吗 → True
  → 本机这条侥幸是对的，因为默认编码恰好是 UTF-8。**侥幸不是判据**，所以下面把 locale 换掉：
  ① 什么都不改：
  [子进程] pref = utf-8
  [子进程] wrote bytes = e4b8ade69687
  ② LC_ALL=C + 关掉 UTF-8 模式与 locale 强制：
  [子进程] pref = ANSI_X3.4-1968
  [子进程] UnicodeEncodeError: 'ascii' codec can't encode characters in position 0-1: ordinal not in range(128)
  ★ 同一段代码、同一个 write_text，换个 locale 就换一种结局（UTF-8 字节 vs UnicodeEncodeError）。
    判据：文本进出的**边界上必须写死 encoding='utf-8'**。省略它 = 把行为外包给环境变量。
```

不写 `encoding=` 时，默认值来自 `sys.getfilesystemencoding()`（本机 `utf-8`）和
`locale.getpreferredencoding(False)`（本机 `UTF-8`），所以 `write_text('中文')` 落的是
`e4b8ade69687`，`read_text()` 读回来相等 → `True`。但脚本立刻把 locale 换掉重跑：
同一段代码、同一个 `write_text`，在 `LC_ALL=C` 且关掉 UTF-8 模式的子进程里，
首选编码变成 `ANSI_X3.4-1968`，当场 `UnicodeEncodeError: 'ascii' codec can't encode characters
in position 0-1: ordinal not in range(128)`。**本机这条侥幸成立，是因为默认编码恰好是 UTF-8；
侥幸不是判据。** 结论落在一个字符上：文本进出的边界上必须写死 `encoding='utf-8'`，
省略它等于把行为外包给环境变量。

坑 3 是「不 `close`」的代价，平时它连一声都不响：

```
=== 5. ★ 坑 3：read_text 会替你 close，但 open(p).read() 没人替你 close ===
  默认过滤器里 ResourceWarning 的位置： [('ignore', 'ResourceWarning')] ← 第 0 位是动作，'ignore' 就是它平时看不见的原因
  默认过滤器下：这次捕获到的警告条数 = 0 （屏幕上一片安静 —— 它被吃掉了）
  simplefilter('always') 之后，stderr 上真的打出这几行（原文，已抹掉行号前缀）：
    ResourceWarning: unclosed file <TextIOWrapper name='text-demo/no_close.txt' mode='r' encoding='utf-8'>
    ResourceWarning: Enable tracemalloc to get the object allocation traceback
  同一个动作连做两遍，清洗后的输出相等吗 → True （消息里本机**没有**内存地址，只有 name/mode/encoding，所以能稳定复现）
  结构化一点看：w.category.__name__ = ResourceWarning ｜ 稳定部分 = unclosed file <TextIOWrapper name='text-demo/no_close.txt' mode='r' encoding='utf-8'>
  → 平时它不打字不是「没有泄漏」，是默认过滤器把 ResourceWarning 设成了 ignore。
    想让它响：`python -X dev`、`PYTHONWARNINGS=always`，或者上面那行 simplefilter('always')。
```

`read_text` 会替你 close，但 `open(p).read()` 没人替你 close。默认过滤器里 `ResourceWarning` 的位置是
`[('ignore', 'ResourceWarning')]` —— 第 0 位是动作，`'ignore'` 就是它平时看不见的原因。
脚本实测默认状态下捕获到的警告条数 = `0`（屏幕一片安静），换 `simplefilter('always')` 之后 stderr 上
真的打出 `ResourceWarning: unclosed file <TextIOWrapper name='text-demo/no_close.txt' mode='r'
encoding='utf-8'>`。这条警告里**没有内存地址**，只有 `name/mode/encoding`，所以同一个动作连做两遍
清洗后的输出相等（→ `True`），能稳定复现。平时它不打字不是「没有泄漏」，是被默认过滤器吃掉了；
想让它响：`python -X dev`、`PYTHONWARNINGS=always`，或者 `simplefilter('always')`。

三种必然失败的原文，errno 一起打：

```
=== 6. 三种必然失败，原文一字不改（errno 一起打）===
  read_text 读一个不存在的路径        → FileNotFoundError: [Errno 2] No such file or directory: 'text-demo/没有.txt' ｜ errno=2
  write_text 写进一个不存在的父目录     → FileNotFoundError: [Errno 2] No such file or directory: 'text-demo/没有的目录/x.txt' ｜ errno=2
  read_text 读一个目录            → IsADirectoryError: [Errno 21] Is a directory: 'text-demo' ｜ errno=21
  → 前两条都是 FileNotFoundError(errno 2)，但**原因不同**：一个缺文件、一个缺目录，原文里的路径字符串能告诉你差在哪。第三条是 IsADirectoryError(errno 21) —— 它不是 PermissionError，权限没挡你，是类型不对。

=== 7. read_text 只有一个动作：把整份文件变成一个 str 放进内存 ===
  写了 200 组「中文字符」 → read_text 返回类型 = str ｜ len(str) = 800 个字符 ｜ 磁盘上 = 2400 字节
  → 两个数字并排看：UTF-8 下一个汉字占 3 字节，所以 800 个字符 = 2400 字节。「文件多大」和「读进来多长」永远分两栏算。
  → 更关键的：这一句没有中间状态，整个 str 一次性在内存里；
    200 MB 的日志也能「读成功」，直到内存吃紧。怎么分块读、什么时候必须分块，见第 18 节。
```

`read_text` 读不存在的路径 → `FileNotFoundError` errno 2；`write_text` 写进不存在的父目录 →
**同样**是 `FileNotFoundError` errno 2，但原因不同（一个缺文件、一个缺目录，报错原文里的路径字符串
能告诉你差在哪）；`read_text` 读一个目录 → `IsADirectoryError` errno 21，它不是 `PermissionError`，
权限没挡你，是类型不对。最后一段是内存那一栏：写了 200 组「中文字符」之后，`read_text` 返回类型是 `str`、
`len(str) = 800` 个字符、磁盘上 `2400` 字节。「文件多大」和「读进来多长」要分两栏算 ——
更要紧的是 `read_text` 没有中间状态，整个 `str` 一次性在内存里，`200 MB` 的日志也能「读成功」，
直到内存吃紧（怎么分块读见第 18 节）。

**带走什么**：这两个方法的适用面就一句话 —— **整份读、整份写、文件小**（配置、模板、几十个字的中间结果）。
脑子里一旦冒出这四个问题，就得退回 `open()` 自己管：要追加？（`write_text` 没有 append，第 2、3 段实测）；
编码谁定的？（默认跟 locale 走，边界上必须写死 `utf-8`）；文件会有多大？（整份进内存）；
要不要边写边 flush、要不要读一半就停？（这两个方法没有游标）。
`open(p).read()` 这种「不 `with`」的写法不会报错，只会漏文件描述符，而且默认连一声警告都不给你。
★ 配置文件**不能**用 `write_text`：它是截断写，进程在「已经清空、还没写新内容」那个窗口里被断电 / OOM /
Ctrl-C，配置文件就永久没了。写配置要走临时文件 + 原子 `rename`，见第 17 节。

---

## 第 13 节 · 临时文件的四种粒度与权限位

这一节回答两个真实事故：「别人为什么能在 `/tmp` 里抢先占掉我的文件名」，以及
「我的临时文件为什么被同机器上的另一个用户读走了」。这里有一条安全线必须先划清：
**`mkstemp` 建出来是 0600，普通 `open` 建出来是 0644。这不是风格差异，也不是「谁更干净」，
差别就是「别的用户能不能在你 `/tmp` 里抢先做手脚」。** 名字要**抢**、不能**算**：
从「算出一个名字」到「打开它」之间那段空档，别的进程可以先占。

先看地基：临时目录是哪个、本机有哪几个 API、随机名字长什么形状。

```
$ python3 l_tempfile_perms.py
=== 1. 地基：tempdir 是哪个目录、本机有哪几个 API、名字长什么样 ===
  tempfile.gettempdir() = /tmp ｜ 它等于 /tmp 吗 → True
  tempfile.gettempprefix() = 'tmp' ｜ 随机段固定 8 位（字母数字下划线）
  本机这六个都在吗 → {'mktemp': True, 'mkstemp': True, 'mkdtemp': True, 'NamedTemporaryFile': True, 'TemporaryFile': True, 'TemporaryDirectory': True}
  → 粒度四种：mkstemp（fd+路径，自己管删）、NamedTemporaryFile（有名字，自动删）、TemporaryFile（连名字都没有）、TemporaryDirectory（一整份目录）。mktemp 不算粒度，它是反面教材，第 2 段专门演它。

=== 2. ★ mktemp：本机还留着它，而且一声不响——这才是它危险的原因 ===
  hasattr(tempfile, 'mktemp') → True
  真调一次：返回类型 = str ｜ 名字形状 = tmp+8位随机 ｜ 这个路径上已有文件吗 → False ← 它只算名字，不建文件
  docstring 原话（不是我们的评价）： THIS FUNCTION IS UNSAFE AND SHOULD NOT BE USED.
  本机有没有 DeprecationWarning → 警告条数 = 0 （3.14 实测：一条都没有，既没被移除也没被警告）
  ★「不报警告」不等于「能用」：从它算出名字到你 open 它，中间别的进程可以先占，这个空档叫竞态；第 5 段用 O_EXCL 现场把空档堵死。
```

`tempfile.gettempdir()` 是 `/tmp`，前缀 `gettempprefix()` 是 `'tmp'`，随机段固定 `8` 位
（字母数字下划线）。本机六个 API 全在（那个字典里六个 `True`），但**粒度只有四种**：
`mkstemp`（给 fd + 路径，删的责任在你）、`NamedTemporaryFile`（有名字、自动删）、
`TemporaryFile`（连名字都没有）、`TemporaryDirectory`（一整份目录）。
`mktemp` 不算第五种粒度，它是反面教材：本机 `hasattr(tempfile, 'mktemp')` 还是 `True`，
真调一次返回的是 `str`、形状是 tmp+8 位随机，而「这个路径上已有文件吗 → False」—— 它**只算名字，
不建文件**。docstring 原话（不是评价）：`THIS FUNCTION IS UNSAFE AND SHOULD NOT BE USED.`
更妙的是本机 `DeprecationWarning` 条数 = `0`：既没被移除也没被警告。**「不报警告」不等于「能用」**，
它危险恰恰藏在这份安静里 —— 从它算出名字到你 `open` 它，中间的空档足够别人先占。

粒度一，以及那组把安全线钉死的对照实验：

```
=== 3. 粒度一：mkstemp —— 返回 (fd, path)，创建那一刻就已经是 0600 ===
  解包出来：type(fd) = int ｜ type(path) = str ｜ 名字形状 = tmp+8位随机.tmp ｜ 是 tmp+8位随机+后缀吗 → True
  权限：从 fd 上 stat = 0o600 ｜ 从 path 再 stat = 0o600 ｜ fd 与 path 是同一个文件吗 → True
  它此刻在磁盘上吗 → True ← mkstemp **真的建了文件**，这点和 mktemp 正相反
  os.close(fd) 之后还在吗 → True ｜ 自己 unlink 之后 → False
  ★ 退出作用域不替你删：mkstemp 给你的是裸 fd，删除责任 100% 在自己手上。

=== 4. ★ 兑现第 1 节那行 umask 对照组：同一目录，mkstemp 0600、普通 open 0644 ===
  本进程入口继承到的 umask = 0o22 （外层 shell 给的，各机器不同）；对照组显式钉在 0022/0077 两档，好跟第 1 节那行 printf 同一口径：
  umask=0022：普通 open = 0o644 ｜ mkstemp = 0o600 ｜ 两者相等吗 → False ← 0644 是全机器可读，0600 才是「只有我能读」
  umask=0077：普通 open = 0o600 ｜ mkstemp = 0o600 ｜ 两者相等吗 → True ← 普通 open 跟着变了，mkstemp **不吃 umask**
  → mkstemp 无视 umask 的原因：内部是 os.open(..., O_CREAT|O_EXCL|O_RDWR, 0o600)，权限是第三个参数**给定**的，umask 只能往上收、不能放开。别指望 umask 替你保密：umask 是调用者当时的心情，0600 是 API 的承诺。
  ★ 已恢复：现在生效的 umask = 0o22 ｜ 入口原值 = 0o22 ｜ 相等吗 → True （上面只改**本进程**的 umask：不写配置文件、不改 shell、不影响别的进程。）

=== 5. O_EXCL 就是那个「抢注」动作：撞一次，看竞态是怎么被堵死的 ===
  先让「别人」把这个名字占掉：存在 → True
  再以 O_CREAT|O_EXCL 打开同一个名字 → FileExistsError: [Errno 17] File exists: 'temp-demo/抢注.txt' ｜ errno = 17
  别人的内容被改掉了吗 → True ← 撞失败是**原子**的：既没打开也没截断，一个字节没动
  反证：不带 O_EXCL、对同一个名字连开两次 → 两次都成功、落在同一个文件上 = True
  → 没有 O_EXCL，内核就不替你判断「这个名字刚才有没有被占」，两次都算成功。mkstemp 的安全就建在 O_EXCL 上：它让「确认不存在」和「创建」成为同一次内核操作，所以没有「先算名字、后创建」的空档——这就是它和 mktemp 的全部区别。
```

`mkstemp` 解包出来 `type(fd) = int`、`type(path) = str`，名字是 tmp+8 位随机+后缀；
权限从 fd 上 stat 是 `0o600`、从 path 再 stat 也是 `0o600`（两者是同一个文件 → `True`），
「它此刻在磁盘上吗 → True」—— 这点和 `mktemp` 正相反。`os.close(fd)` 之后文件还在（→ `True`），
自己 `unlink` 之后才没了：**退出作用域不替你删**，裸 fd 的删除责任 100% 在自己手上。

★ 兑现本节开头那条：同一目录、同一时刻，脚本把 umask 显式钉在 `0022` 和 `0077` 两档测了一遍。
`umask=0022` 时普通 `open` 建出来 `0o644`、`mkstemp` 建出来 `0o600`，两者相等吗 → `False`；
把 umask 收到 `0077`，普通 `open` 跟着变成 `0o600`，这时两者相等 → `True`。
也就是说：**普通 `open` 的权限取决于调用者当时的心情（umask），`mkstemp` 的 0600 是 API 的承诺。**
原因写在原文里：`mkstemp` 内部是 `os.open(..., O_CREAT|O_EXCL|O_RDWR, 0o600)`，
权限是第三个参数**给定**的，umask 只能往上收、不能放开。脚本还当场核对了自己把 umask 还回了 `0o22`
（入口原值也是 `0o22`，相等 → `True`，只改本进程，不写配置文件、不改 shell、不影响别的进程）。
`0o644` 落在 `/tmp` 意味着全机器可读 —— 别指望 umask 替你保密。

`O_EXCL` 就是那个「抢注」动作，它把空档彻底堵死：脚本先让「别人」把名字占掉（存在 → `True`），
再以 `O_CREAT|O_EXCL` 打开同一个名字 → `FileExistsError: [Errno 17] File exists`；
关键是**别人的内容一个字节都没被改**（→ `True`），撞失败是原子的：既没打开也没截断。
反证更说明问题：不带 `O_EXCL`、对同一个名字连开两次 → 两次都成功、落在同一个文件上（→ `True`）。
没有 `O_EXCL`，内核就不替你判断「这个名字刚才有没有被占」。`mkstemp` 的安全就建在它上面：
它让「确认不存在」和「创建」成为**同一次内核操作**，所以没有「先算名字、后创建」的空档 ——
这就是它和 `mktemp` 的全部区别。

```
=== 6. 粒度二：NamedTemporaryFile（默认 delete=True）—— 名字是它的，不是你借的 ===
  对象类型 = _TemporaryFileWrapper ｜ 名字形状 = tmp+8位随机.txt ｜ 权限 = 0o600 ｜ 在磁盘上吗 → True ｜ 有 .name 吗 → True
  ★ 拿它的名字**再 open 一次**读得到吗（Linux 实测）→ True ｜ 读到 = '已经写进去了'
  close() 之后还在吗 → False ← delete=True 就是在这一瞬间删的

=== 7. 粒度二·续：delete_on_close=False；粒度三 TemporaryFile：它根本没有名字 ===
  本机参数表 = ['mode', 'buffering', 'encoding', 'newline', 'suffix', 'prefix', 'dir', 'delete', 'errors', 'delete_on_close'] ｜ 有 delete_on_close 吗 → True （3.12 起才有）
  delete_on_close=False：close() 后还在吗 → True ｜ 名字形状 = tmp+8位随机 ｜ 内容已经落盘了吗 → True
  → close 只关句柄，**删**这个动作留给你自己：os.unlink(obj.name)。包装对象自己没有 unlink 方法，写 `ntf2.unlink()` 本机直接 AttributeError（这一条是踩出来的）。
  TemporaryFile：对象类型 = BufferedRandom ｜ .name 的类型 = int ← 是个**文件描述符整数**，不是路径
  当路径用 → TypeError: expected str, bytes or os.PathLike object, not int
  /proc/self/fd/<n> 指的东西末尾带 (deleted) 吗 → True ← 名字已从目录里摘掉，只剩句柄
  → 没有名字 = 别人撞不上，你也没机会「忘了删」；代价是不能用路径访问它（不能交给子进程、不能 mmap、也不能在另一个 open() 里读）。close 即删，所以不用登记。

=== 8. 粒度四：TemporaryDirectory（0700），和 0500 子目录会不会把清理炸掉 ===
  对象类型 = TemporaryDirectory ｜ 名字形状 = tmp+8位随机 ｜ 权限 = 0o700 （0700 的执行位 = 能进去，别的用户连 ls 都 ls 不了里面）
  inner 收成 0500 后交给 shutil.rmtree → PermissionError: errno=13, strerror='Permission denied'（str(exc) 带沙箱绝对路径，只打这三样）
  chmod 回 0700 之后 cleanup() → 目录还在吗 → False
  ★ 反直觉的一半：形状完全一样，只把 rmtree 换成 td2.cleanup() → 不抛异常 ｜ 目录还在吗 → False
    证据在源码里：它的 _rmtree 挂了 onexc，遇 PermissionError 先 _resetperms 再重试 = True
  → 别据此写「反正 cleanup 会自愈」：它自愈的只有**它自己建的**那棵树；你自己的 0500 项目目录交给 shutil.rmtree，照样是上面那条 PermissionError。
```

`NamedTemporaryFile` 默认 `delete=True`：对象类型是 `_TemporaryFileWrapper`、权限 `0o600`、
在磁盘上 → `True`、有 `.name` → `True`，而**拿它的名字再 open 一次读得到**（→ `True`，读到
`'已经写进去了'`）。名字是它的，不是你借的。`close()` 之后还在吗 → `False`，`delete=True` 就在这一瞬间删。
参数表里 `delete_on_close` 存在（→ `True`，3.12 起才有）；`delete_on_close=False` 时 `close()` 之后
文件还在（→ `True`）、内容已落盘（→ `True`），**删这个动作留给你自己**：`os.unlink(obj.name)`。
包装对象自己没有 `unlink` 方法，写 `ntf2.unlink()` 本机直接 `AttributeError`（这一条是踩出来的）。
`TemporaryFile` 更彻底：对象类型 `BufferedRandom`，`.name` 的类型是 `int` —— 那是个文件描述符整数，
不是路径，当路径用就 `TypeError: expected str, bytes or os.PathLike object, not int`；
`/proc/self/fd/<n>` 指向的东西末尾带 `(deleted)`（→ `True`）：名字已从目录里摘掉，只剩句柄。
没有名字 = 别人撞不上、你也没机会「忘了删」，代价是不能用路径访问它（不能交给子进程、不能 mmap、
也不能在另一个 `open()` 里读）。
`TemporaryDirectory` 是 `0o700`（执行位让你能进去，别的用户连 `ls` 都 `ls` 不了里面）。
反直觉的那一半在这里：往里面放一个收成 `0500` 的子目录，交给 `shutil.rmtree` →
`PermissionError: errno=13, strerror='Permission denied'`；但形状完全一样、只把 `rmtree` 换成
`td.cleanup()` → **不抛异常**，目录照样没了（→ `False`）。证据在源码里：它的 `_rmtree` 挂了 `onexc`，
遇到 `PermissionError` 先 `_resetperms` 再重试（→ `True`）。别据此写「反正 cleanup 会自愈」——
它自愈的只有**它自己建的那棵树**；你自己的 `0500` 项目目录交给 `shutil.rmtree`，
照样是上面那条 `PermissionError`。

临时目录到底从哪来，以及一条诚实声明：

```
=== 9. gettempdir() 从哪来：TMPDIR / TEMP / env= 全在子进程里改，本进程一动不动 ===
  ① 三个变量全清掉          → 子进程只报形状：IS_TMP ｜ 命中的是我们指的那个吗 → False
  ② TMPDIR=我们的目录     → 子进程只报形状：自选目录 ｜ 命中的是我们指的那个吗 → True
  ③ TEMP=我们的目录       → 子进程只报形状：自选目录 ｜ 命中的是我们指的那个吗 → True
  ④ 给一个没用的变量         → 子进程只报形状：IS_TMP ｜ 命中的是我们指的那个吗 → False
  本进程自己的 gettempdir() 仍然是 = /tmp （上面全是子进程的事，父进程的环境一个字没改）
  → 候选顺序：TMPDIR → TEMP → TMP → 内置（/tmp、/var/tmp、/usr/tmp）→ 当前目录；结果还被缓存在模块里，所以环境变量必须在**进程起来之前**设好才有效——subprocess 的 env= 正是这个用法。
  ★ 判据：要把临时文件关进自己的沙箱，传 dir= 最稳，改子进程环境其次，靠默认值最危险——默认值是 /tmp，那是公共地盘。

=== 10. ★ 未验证：Windows 语义，这台 Linux 机器一条都复现不了 ===
  下面几条是文档/常识里的 Windows 行为，本机**无法验证**，别当事实引用：
    · NamedTemporaryFile 在 Windows 上通常不能拿同一个名字二次 open（共享模式不同）；
    · delete=True 的删除时机受句柄释放影响，close() 之后能否立刻重建同名文件不保证；
    · 0600 在 Windows 上是「只有当前用户有 ACL」的近似，不是 POSIX 的那 9 个比特位；
    · 临时目录候选在 Windows 上一般命中 %LOCALAPPDATA%\Temp，而不是 /tmp。
  → 第 6 段那句「再 open 一次读得到 = True」**只在 Linux 成立**。要跨平台拿到「有名、别人撞不上」的临时文件，标准写法是 mkstemp + os.close(fd) + 自己 unlink，别依赖 NamedTemporaryFile。
```

四条子进程实验只报形状不报绝对路径：三个变量全清掉 → 命中 `/tmp`（不是我们指的那个 → `False`）；
`TMPDIR=` 我们的目录 → 命中自选目录（→ `True`）；`TEMP=` 我们的目录 → 也命中（→ `True`）；
给一个没用的变量 → 回到 `/tmp`（→ `False`）。而父进程自己的 `gettempdir()` 仍然是 `/tmp`，
环境一个字没改。候选顺序是 `TMPDIR → TEMP → TMP → 内置（/tmp、/var/tmp、/usr/tmp）→ 当前目录`，
结果还被**缓存在模块里**，所以环境变量必须在进程起来之前设好才有效 —— `subprocess` 的 `env=` 正是这个用法。
要把临时文件关进自己的沙箱，传 `dir=` 最稳，改子进程环境其次，靠默认值最危险（默认值是 `/tmp`，公共地盘）。
★ 围栏最后那段是本节唯一一条「听说」：Windows 语义这几条**本机无法验证，别当事实引用** ——
`NamedTemporaryFile` 在 Windows 上通常不能拿同一个名字二次 open（共享模式不同）、`delete=True` 的删除时机
受句柄释放影响、`0600` 在 Windows 上只是「当前用户有 ACL」的近似而非 POSIX 那 9 个比特位、
临时目录一般命中 `%LOCALAPPDATA%\Temp` 而不是 `/tmp`。所以第 6 段那句「再 open 一次读得到 = True」
**只在 Linux 成立**；要跨平台拿到「有名、别人撞不上」的临时文件，标准写法是
`mkstemp` + `os.close(fd)` + 自己 `unlink`，别依赖 `NamedTemporaryFile`。

**带走什么**：① 名字要抢不能算 —— `mkstemp` 内部 `O_CREAT|O_EXCL`，撞了就换下一个；
`mktemp` 只算不抢，中间有空档，而本机它还在、一声不响，正因如此更不能当粒度用。
② 权限要 API 给、不能环境给 —— `mkstemp` 建出来是 `0o600`、`TemporaryDirectory` 建出来是 `0o700`，
与继承来的 umask 无关；
普通 `open` 的 `0644` 落在 `/tmp` 就是全机器可读。③ `delete` 的边界背清楚 ——
`NamedTemporaryFile` 默认 `delete=True`，在 `close()` 那一刻删；`delete_on_close=False` 只关不删（得自己 unlink）；
`TemporaryFile` 连名字都没有；`mkstemp` 谁都不替你删；`cleanup()` 会自愈 `0500`，但只管它自己建的那棵树。
④ 要保密 → `O_EXCL` + `0600` + 尽早 close；要可控 → 传 `dir=`。
这两条本机实测过，Windows 那一段**没测过** —— 读者必须分得清哪条是证据、哪条是听说。

---

## 第 14 节 · 符号链接、resolve() 和「我是谁」

一个符号链接摆在面前，「它是谁」这个问题有好几条不同的算法，而它们给的答案互相不一样。
主轴在这里最锋利：`normpath` 是**纯字符串**运算，`absolute()` 只拼 cwd，
`abspath` 折叠字符串，只有 `resolve()` 是**问内核** —— 所以只有最后一条知道某个目录名其实是链接。
这一节还有一条能让备份脚本变成删除脚本的规矩：删链接不碰真身，但**写链接会写穿真身**。

```
$ python3 m_symlink_resolve.py
=== 1. 造现场，然后看「真身 / 文件链接 / 目录链接」的四个布尔 ===
  现场（全是相对路径）：real/a/hidden.txt 是真身；real/a/link-to-txt → hidden.txt；real/b/link-dir → ../a
  对象           is_symlink  exists   is_file  is_dir
  hidden.txt   is_symlink=False  exists=True   is_file=True   is_dir=False
  link-to-txt  is_symlink=True   exists=True   is_file=True   is_dir=False
  link-dir     is_symlink=True   exists=True   is_file=False  is_dir=True
  → 三个对象的 exists() 都是 True，但只有第一个 is_symlink() 是 False：exists() 会**顺着链接走过去**看目标。
  → 目录链接 link-dir 的 is_dir() 也是 True：所以 `if p.is_dir()` 挡不住「它其实是个指向别处的链接」。
  → 迭代给的是目录项本身，不是链接的目标：sorted(a/ 里的名字) = ['hidden.txt', 'link-to-txt']

=== 2. readlink() 拿到的是什么，resolve() 拿到的又是什么 ===
  readlink 给「链接里存的原文」，一个字符都不改（相对就还是相对）：
      symlink-demo/real/a/link-to-txt → readlink() = PosixPath('hidden.txt')
      symlink-demo/real/b/link-dir → readlink() = PosixPath('../a')
  resolve() 给绝对路径 —— 所以本脚本**不打**它，只打能确定身份的三样：
      是绝对路径吗 → True ｜ is_symlink() → False（走到底就不是链接了）
      尾巴匹配：str(rp).endswith('/real/a/hidden.txt') → True
      对沙箱根取相对形态（这就是屏幕上唯一允许的形态）→ 'symlink-demo/real/a/hidden.txt'
      目录链接 resolve 的尾巴 → 'symlink-demo/real/a'（../a 被内核从 b/ 起算，落到了 real/a）
  → 判据：readlink 是「问链接存了什么」，resolve 是「问内核最终是谁」；两个问题别混。
```

三个对象的四个布尔并排看：真身 `hidden.txt` 是 `is_symlink=False  exists=True  is_file=True  is_dir=False`；
文件链接 `link-to-txt` 是 `is_symlink=True   exists=True   is_file=True   is_dir=False`；
目录链接 `link-dir` 是 `is_symlink=True   exists=True   is_file=False  is_dir=True`。
三个 `exists()` 都是 `True`，只有第一个 `is_symlink()` 是 `False` —— 因为 `exists()` 会**顺着链接走过去**看目标；
而 `link-dir` 的 `is_dir()` 也是 `True`，所以 `if p.is_dir()` 挡不住「它其实是指向别处的链接」。
迭代给的永远是目录项本身，不是链接目标（`sorted(a/ 里的名字)` = `['hidden.txt', 'link-to-txt']`）。
`readlink()` 拿到的是「链接里存的原文」，一个字符都不改，相对就还是相对：
`PosixPath('hidden.txt')`、`PosixPath('../a')`。`resolve()` 给的是绝对路径，
所以脚本**不把它整个打出来**，只打能确定身份的三样：是绝对路径 → `True`、`is_symlink()` → `False`
（走到底就不是链接了）、尾巴 `endswith('/real/a/hidden.txt')` → `True`，
以及相对形态 `'symlink-demo/real/a/hidden.txt'`。目录链接那条 resolve 的尾巴是 `'symlink-demo/real/a'`
—— `'../a'` 被内核从 `b/` 起算，落到了 `real/a`。

同一个带 `..` 的相对路径，四条算法各给什么，外加当场验掉一个常见误区：

```
=== 3. 同一个带 .. 的相对路径，四条算法各给什么（★顺带验 resolve 查不查存在）===
  ① 原样 p                        → 'symlink-demo/real/b/../b/link-dir/hidden.txt'（parts 里有 '..' 吗 → True）
  ② p.absolute()（只拼 cwd）       → '<cwd>/symlink-demo/real/b/../b/link-dir/hidden.txt'
      拼完之后 parts 里还有 '..' 吗 → True  ★absolute() **不折叠 ..**
  ③ os.path.normpath(str(p))       → 'symlink-demo/real/b/link-dir/hidden.txt'（纯字符串折叠，link-dir 还当成普通目录名）
  ④ os.path.abspath(p)             → '<cwd>/symlink-demo/real/b/link-dir/hidden.txt' ＝ abspath 是「拼 cwd + 折叠」，仍然没问内核链接
  ⑤ p.resolve()（问内核）          → 尾巴 'symlink-demo/real/a/hidden.txt' ｜ endswith('/real/a/hidden.txt') = True
      ★ 只有 ⑤ 知道 link-dir 是链接：normpath 那行的结果是 real/b/link-dir/hidden.txt，
        拼出来的字符串看着对，**指到的东西不一定对**。
  ★ 再当场验掉一个最常见的误区：resolve() 会不会检查存在？现造一个悬空链接：
  dangling → nowhere.txt（真身不存在）： is_symlink=True exists=False os.path.exists=False
  默认 p.resolve() 没抛异常，照样给结果 → 尾巴 'symlink-demo/real/a/nowhere.txt'
    结果存在吗 → False  ← resolve 只负责「把链接走通」，不负责「保证有东西」
  resolve(strict=True) 才会炸，原文：
      → FileNotFoundError: [Errno 2] No such file or directory: '<cwd>/symlink-demo/real/a/nowhere.txt'
  真去读它时才炸：FileNotFoundError: [Errno 2] No such file or directory: 'symlink-demo/real/a/dangling'（filename 是相对形态，原样保留）
  → 判据：默认 resolve() **不检查存在**；要「必须存在否则报错」得自己写 strict=True。
```

① 原样 `'symlink-demo/real/b/../b/link-dir/hidden.txt'`（`parts` 里有 `'..'` → `True`）；
② `p.absolute()` 只拼 cwd（屏幕上那行开头是 `<cwd>` 占位符），拼完 `parts` 里还有 `..` → `True`，
**`absolute()` 不折叠 `..`**；③ `os.path.normpath(str(p))` 给
`'symlink-demo/real/b/link-dir/hidden.txt'`，纯字符串折叠，`link-dir` 还当成普通目录名；
④ `os.path.abspath(p)` ＝「拼 cwd + 折叠」，仍然没问内核链接；
⑤ `p.resolve()` 尾巴是 `'symlink-demo/real/a/hidden.txt'` —— **只有 ⑤ 知道 link-dir 是链接**。
`normpath` 那行拼出来的字符串看着对，**指到的东西不一定对**（`real/b/...` vs `real/a/...`）。
误区是「resolve 会保证文件在」：脚本现造一个悬空链接 `dangling → nowhere.txt`
（`is_symlink=True exists=False os.path.exists=False`），默认 `p.resolve()` 没抛异常，
照样给结果，尾巴落在 `'symlink-demo/real/a/nowhere.txt'`，而「结果存在吗 → False」。
要「必须存在否则报错」得自己写 `resolve(strict=True)`，那条原文是
`FileNotFoundError: [Errno 2] No such file or directory`。真去读它的时候才炸，
而且 `filename` 保持你给的相对形态 `'symlink-demo/real/a/dangling'`。

★ 相对符号链接相对的是**链接自己所在的目录**，不是你的 cwd —— 这条是运维现场最常见的坑，脚本真换了 cwd 跑：

```
=== 4. ★相对符号链接相对的是**链接自己所在的目录**：换 cwd 真跑一遍 ===
  同一条相对路径 'link-to-txt'，只换 cwd，不换路径：
      在 a/ 里开 'link-to-txt' → 成功，内容='真身原始内容\n'
      在 b/ 里开同一条 → FileNotFoundError: [Errno 2] No such file or directory: 'link-to-txt'
      但在 b/ 里开 'link-dir/hidden.txt' 是成功的 —— 内容='真身原始内容\n'
  → 因为内核处理 ../a 时，起点是**链接所在的 b/**，不是你的 cwd。
    这解释了为什么「脚本里用相对链接」换目录跑就会挂，而绝对链接不会。
  cwd 已回到原处 → True

=== 5. 删与改：unlink 只删链接；但 'w' 会**穿过链接写进真身** ===
  删之前：真身 exists=True 内容='真身原始内容\n'
  link.unlink() 之后：链接 is_symlink=False（没了） ｜ ★真身 exists 仍是 True，内容='真身原始内容\n'（一个字没掉）
  对链接用 'w' 写完 → 链接还是链接：is_symlink=True ｜ ★真身内容变成 '穿过链接改过的内容\n'
  → ★「打开链接写」= 改链接指向的那个文件，而且 w 的截断当场生效（第 1 节那条）。
    备份脚本里一个 `open(link,'w')` 就能把生产文件清成 0 字节，屏幕上还一切正常。
  shutil.copy(src, dst) 两种跟随，复制完当场验：
      copy-follow  is_symlink=False  exists=True
      copy-keep    is_symlink=True   exists=False readlink=PosixPath('hidden.txt')
        ★在新目录里读它 → FileNotFoundError: [Errno 2] No such file or directory: 'symlink-demo/dst/copy-keep'（'hidden.txt' 在这儿不存在）
  → 打包/备份的分水岭就在这：跟链接（默认）会把真身拷进包里；不跟（follow_symlinks=False）只拷链接，链接换个窝就悬空。
```

同一条相对路径 `'link-to-txt'`，在 `a/` 里 open 成功（内容 `'真身原始内容\n'`），
在 `b/` 里 open 同一条就是 `FileNotFoundError: [Errno 2] No such file or directory: 'link-to-txt'`；
但在 `b/` 里 open `'link-dir/hidden.txt'` 是成功的。原因：内核处理 `'../a'` 时起点是**链接所在的 `b/`**，
不是你的 cwd。脚本验完立刻把 cwd 放回原处（→ `True`）。这解释了为什么「脚本里用相对链接」
换目录跑就会挂 —— 脚本搬家、cron 换目录、容器换 `WORKDIR` 都是这一条在咬人。

删与改那半屏是本节最该抄走的两行：`link.unlink()` 之后链接没了（`is_symlink=False`），
而真身 `exists` 仍是 `True`、内容一个字没掉；但对链接用 `'w'` 写完，链接还是链接
（`is_symlink=True`），**真身内容变成 `'穿过链接改过的内容\n'`**。
「打开链接写」= 改链接指向的那个文件，而且 `'w'` 的截断当场生效（第 12 节那条）。
备份脚本里一个 `open(link, 'w')` 就能把生产文件清成 0 字节，屏幕上还一切正常。
`shutil.copy` 两种跟随当场验：跟（默认）→ `copy-follow is_symlink=False exists=True`（真身被拷进来了）；
不跟（`follow_symlinks=False`）→ `copy-keep is_symlink=True exists=False readlink=PosixPath('hidden.txt')`，
在新目录里读它立刻 `FileNotFoundError`（`'hidden.txt'` 在这儿不存在）。
打包/备份的分水岭就在这：跟链接会把真身拷进包里，不跟只拷链接、链接换个窝就悬空。

```
=== 6. samefile / os.path.realpath 与 resolve() 的实测对照 ===
  os.path.samefile(link, 真身) → True ｜ Path(link).samefile(真身) → True
  realpath 与 resolve 结果相同吗 → True ｜ 尾巴分别是 'symlink-demo/real/a/hidden.txt' / 'symlink-demo/real/a/hidden.txt'
  realpath 是纯字符串算的吗 → 不是：它和 resolve 一样要逐个 lstat，会展开链接；
  差别只在「查不查存在」这一条边界上，realpath 也有 strict 开关（本机实测）：
      → FileNotFoundError: [Errno 2] No such file or directory: '<cwd>/symlink-demo/real/a/nowhere.txt'
  → 判据：`samefile` 比的是「同一个文件对象」（内核身份），`normpath` 比的是字符串 ——
    路径字符串相等 ≠ 同一个文件；要判断「会不会写坏同一个东西」只能问内核。

=== 7. 收口：这四条以后当判据用 ===
  ① resolve() 问内核、会展开链接；normpath 纯算字符串、什么链接都不知道；
     absolute() 只拼 cwd、连 .. 都不折叠（要折叠得 abspath 或 normpath）。
  ② 默认 resolve(strict=False) **不检查存在**：悬空链接照样返回一个绝对路径，
     「拿到路径」和「文件在那儿」是两件事。
  ③ 相对符号链接的目标相对的是**链接自己所在的目录**：换 cwd 会换掉结果，
     脚本搬家、cron 换目录、容器换 WORKDIR 都是这一条在咬人。
  ④ 删链接不碰真身；**写链接会写穿真身**；备份跟不跟链接决定你会不会把 /etc 拷进包里。
     ★ 所以：打包用 follow_symlinks=False / copytree(symlinks=True)，
       写之前先 is_symlink() 判一次，别信 open() 不会改到别人。

  清理：symlink-demo/ 已删除（连里面的链接一起）｜ 还在吗 False ｜ 当前目录里 *-demo 残留 = 无
```

`os.path.samefile(link, 真身)` 和 `Path(link).samefile(真身)` 都是 `True` —— 它们比的是内核身份，
不是字符串。`realpath` 与 `resolve` 的结果相同（→ `True`），尾巴都是 `'symlink-demo/real/a/hidden.txt'`。
别以为 `realpath` 是纯字符串算法：它和 `resolve` 一样要逐个 `lstat`，会展开链接，
差别只在「查不查存在」这条边界上，而且 `realpath` 也有 strict 开关（本机实测那条
`FileNotFoundError` 就是它抛的）。
围栏末尾那几行是脚本自己收口的四条判据，和下面这段是一条条对得上的。

**带走什么**：① `resolve()` 问内核、会展开链接；`normpath` 纯算字符串、什么链接都不知道；
`absolute()` 只拼 cwd、连 `..` 都不折叠（要折叠得 `abspath` 或 `normpath`）。
② 默认 `resolve(strict=False)` **不检查存在**：悬空链接照样返回一个绝对路径，
「拿到路径」和「文件在那儿」是两件事。③ 相对符号链接的目标相对的是**链接自己所在的目录**，换 cwd 会换掉结果。
④ 删链接不碰真身、**写链接会写穿真身**；备份跟不跟链接决定你会不会把 `/etc` 拷进包里 ——
所以打包用 `follow_symlinks=False` / `copytree(symlinks=True)`，写之前先 `is_symlink()` 判一次，
别信 `open()` 不会改到别人。路径字符串相等 ≠ 同一个文件；要判断「会不会写坏同一个东西」只能问内核。

---

## 第 15 节 · csv：它到底替你处理了什么

「不就是逗号分隔吗，`split(',')` 一下就行」—— 这句话在真数据上必错。
csv 格式的规则其实只有一条：**字段里出现的分隔符、引号、换行，必须先被引号保护起来**，
读的时候再按同一套规矩反解。这一节把「替你处理了什么」和「没替你处理什么」分开：
它管引号和分隔符，**编码、类型、行尾都不归它**。

```
$ python3 n_csv.py
=== 1. 先演「不用 csv 会怎么死」：同一行数据，split 和 reader 并排 ===
  原文 '张三,"预算, 超了"' → split(',') 给 ['张三', '"预算', ' 超了"']（3 列）‖ csv.reader 给 ['张三', '预算, 超了']（2 列）★列数错了
  原文 '李四,"他说""好"""' → split(',') 给 ['李四', '"他说""好"""']（2 列）‖ csv.reader 给 ['李四', '他说"好"']（2 列）列数都是 2，内容却不同
  原文 '"第一行\n第二行",备注' → split(',') 给 ['"第一行\n第二行"', '备注']（2 列）‖ csv.reader 给 ['第一行\n第二行', '备注']（2 列）列数都是 2，内容却不同
  → split 的错不抛异常：它把「预算, 超了」切成两列，整张表当场错位，
    要到下游按列名取值时才炸 —— 那时候已经不知道是第几行的锅了。

=== 2. csv 的引号规则实测：三种麻烦字段 × quoting 四档 × escapechar ===
  含逗号      → 写成 '字段,"预算, 超了"\n'
  含双引号     → 写成 '字段,"他说""好"""\n'
  含换行      → 写成 '字段,"第一行\n第二行"\n'
  → 规则只有两条：需要保护才加引号（QUOTE_MINIMAL）；字段里的 " 写成 ""（doubling）。
    读的时候反过来：**只有被引号包住的换行才算字段内容** —— 这是第 6 段那个坑的根源。
  QUOTE_MINIMAL     → '甲,1,"有,逗号","有""引""号"\r\n'
  QUOTE_ALL         → '"甲","1","有,逗号","有""引""号"\r\n'
  QUOTE_NONNUMERIC  → '"甲",1,"有,逗号","有""引""号"\r\n'
  QUOTE_NONE        → ★故意失败：Error: need to escape, but no escapechar set
  escapechar=反斜杠（仍 QUOTE_MINIMAL）→ '甲,1,"有,逗号","有""引""号"\r\n'
  escapechar=反斜杠 + QUOTE_NONE       → '甲,1,有\\,逗号,有\\"引\\"号\r\n'
  → 实测：QUOTE_MINIMAL 下 escapechar **完全不起作用**（上一行和 QUOTE_MINIMAL 那行逐字一样，
    引号照旧 doubling）；只有 QUOTE_NONE「不用引号」时它才接手。
  默认 lineterminator = '\r\n' ｜ os.linesep = '\n' ｜ 本机自带 dialect 名 ['excel', 'excel-tab', 'unix']
  → 这两个默认值不一致：csv 硬写 \r\n，本机文本层写的是 \n。第 3 段演的就是它俩打架。
  → QUOTE_NONE 是「我不用引号」的宣言：字段里一旦有分隔符它只能报错 —— 宁可报错，
    也不产出一份看着正常、实际错位的文件（这条原文就是上面那句 need to escape）。
```

并排三行就是全部论证。`'张三,"预算, 超了"'`：`split(',')` 给 3 列
`['张三', '"预算', ' 超了"']`，`csv.reader` 给 2 列 `['张三', '预算, 超了']` —— **列数错了**，
而且 split 不抛异常，整张表当场错位，要到下游按列名取值时才炸，那时已经不知道是第几行的锅。
第二、三行更阴：列数碰巧都是 2，内容却不同（`'他说"好"'` 的引号还在 split 的结果里；
带换行那个字段 split 也没解）。
写侧规则：含逗号 → `'字段,"预算, 超了"\n'`；含双引号 → `'字段,"他说""好"""\n'`（引号 doubling）；
含换行 → `'字段,"第一行\n第二行"\n'`。`quoting` 四档实测：`QUOTE_MINIMAL` 只保护需要的字段，
`QUOTE_ALL` 全加引号，`QUOTE_NONNUMERIC` 那一行里数字 `1` 是唯一没被引号包住的
（`"甲",1,"有,逗号","有""引""号"`），
`QUOTE_NONE` 是故意失败的：`Error: need to escape, but no escapechar set` ——
宁可报错，也不产出一份看着正常、实际错位的文件。
还有一条常见误解被当场打掉：`escapechar=反斜杠` 配 `QUOTE_MINIMAL` 的结果和 `QUOTE_MINIMAL` 那行
**逐字一样**（引号照旧 doubling），escapechar 完全不起作用；只有 `QUOTE_NONE`「不用引号」时它才接手。
最底下两行是后面那个大坑的种子：默认 `lineterminator = '\r\n'`，而 `os.linesep = '\n'`，
csv 硬写 `\r\n`、文本层默认写 `\n`，两个默认值天生不一致（本机自带 dialect 名 `['excel', 'excel-tab', 'unix']`）。

★ 主菜：写 CSV 为什么必须 `newline=""`。两份文件的真实字节对着看：

```
=== 3. ★主菜：写 CSV 为什么必须 newline="" —— 两份文件的真实字节对着看 ===
  newline=""        nl-blank.csv      字节=64  \r\n ×3 其中 \r\r\n ×0
      repr = b'\xe5\xa7\x93\xe5\x90\x8d,\xe5\xa4\x87\xe6\xb3\xa8\r\n\xe5\xbc\xa0\xe4\xb8\x89,"\xe9\xa2\x84\xe7\xae\x97, \xe8\xb6\x85\xe4\xba\x86"\r\n\xe6\x9d\x8e\xe5\x9b\x9b,"\xe4\xbb\x96\xe8\xaf\xb4""\xe5\xa5\xbd"""\r\n'
  newline=None 默认   nl-none.csv       字节=64  \r\n ×3 其中 \r\r\n ×0
      repr = b'\xe5\xa7\x93\xe5\x90\x8d,\xe5\xa4\x87\xe6\xb3\xa8\r\n\xe5\xbc\xa0\xe4\xb8\x89,"\xe9\xa2\x84\xe7\xae\x97, \xe8\xb6\x85\xe4\xba\x86"\r\n\xe6\x9d\x8e\xe5\x9b\x9b,"\xe4\xbb\x96\xe8\xaf\xb4""\xe5\xa5\xbd"""\r\n'
  本机 os.linesep='\n' → 写侧翻译是 \n→\n，所以这两份**在这台机器上字节相同**，
  而且两份都带着 csv 自己写的 \r\n。差别不是假的，只是本机看不见 —— 那就把翻译层真的动
  起来：显式 newline='\r\n' 等价于把 os.linesep 换成 \r\n（Windows 的默认值就是这个形状）。
  ★ Windows 本身本机验不了，这一行按「机制实测 + 平台未验证」读，别当跨平台已验证结论。
  newline='\r\n' 模拟 nl-fake-win.csv   字节=67  \r\n ×3 其中 \r\r\n ×3
      repr = b'\xe5\xa7\x93\xe5\x90\x8d,\xe5\xa4\x87\xe6\xb3\xa8\r\r\n\xe5\xbc\xa0\xe4\xb8\x89,"\xe9\xa2\x84\xe7\xae\x97, \xe8\xb6\x85\xe4\xba\x86"\r\r\n\xe6\x9d\x8e\xe5\x9b\x9b,"\xe4\xbb\x96\xe8\xaf\xb4""\xe5\xa5\xbd"""\r\r\n'
      读回来（newline=''）→ [['姓名', '备注'], [], ['张三', '预算, 超了'], [], ['李四', '他说"好"'], []]
      ★ 空记录 [] 出现 3 次：\r\r\n 把一行劈成两半，csv 只能认为
        遇到了空行。列数没错、编码没错，行数却翻倍 —— 最难查的就是这种。
  读侧两条世界线，读同一份 nl-blank.csv：
      open 默认 newline 读到的 str → '姓名,备注\n张三,"预算, 超了"\n李四,"他说""好"""\n'
      open newline=''  读到的 str → '姓名,备注\r\n张三,"预算, 超了"\r\n李四,"他说""好"""\r\n'
  字段里带裸 \r          bare-cr.csv       字节=16  \r\n ×2 其中 \r\r\n ×0
      repr = b'x\r\n"\xe8\xa1\x8c1\r\xe8\xa1\x8c2"\r\n'
      newline='' 读 → [['x'], ['行1\r行2']] ｜ 默认 newline 读 → [['x'], ['行1\n行2']]
      ★ 那个 \r 被悄悄换成 \n：字段内容变了，一个异常都没有。
  → 判据落在一行：csv 自己写 \r\n，文本层默认会再翻译一次 → \r\r\n；
    所以读写两侧都要 newline=""，让 csv 自己判断哪里是行尾、哪里是字段内容。
```

本机 `newline=""` 那份 64 字节、`\r\n` ×3、`\r\r\n` ×0；`newline=None`（默认）那份也是 64 字节、
`\r\r\n` ×0 —— 因为本机 `os.linesep='\n'`，写侧翻译是 `\n→\n`，**这两份在这台机器上字节相同**。
差别不是假的，只是本机看不见。于是脚本把翻译层真的动起来（`newline='\r\n'`，等价于把 `os.linesep`
换成 `\r\n`，也就是 Windows 的默认形状）：那份立刻变成 67 字节、`\r\r\n` ×3，
读回来变成 `[['姓名', '备注'], [], ['张三', '预算, 超了'], [], ['李四', '他说"好"'], []]` ——
**空记录 `[]` 出现 3 次**：`\r\r\n` 把一行劈成两半，csv 只能认为遇到了空行。
列数没错、编码没错，行数却翻倍，最难查的就是这种。脚本在这一行旁边明写了：
Windows 本身本机验不了，这条按「机制实测 + 平台未验证」读，别当跨平台已验证结论。
读侧也有两条世界线，读同一份 `nl-blank.csv`：默认 `newline` 读到的 str 里行尾是 `\n`，
`newline=''` 读到的是 `\r\n`。最后一个样本：字段里带裸 `\r` 时（磁盘 16 字节），
`newline=''` 读出 `['行1\r行2']`，默认 `newline` 读出 `['行1\n行2']` ——
那个 `\r` 被悄悄换成 `\n`，字段内容变了，**一个异常都没有**。
判据落在一行：csv 自己写 `\r\n`，文本层默认会再翻译一次 → `\r\r\n`；所以**读写两侧都要 `newline=""`**，
让 csv 自己判断哪里是行尾、哪里是字段内容。

```
=== 4. 读侧 API：reader 是一次性迭代器，DictReader 的 key 从哪来 ===
  csv.reader(...) 的类型 = reader ｜ 是迭代器本身吗 → True
      next(it) → ['姓名', '备注']（拿到的是 list，不是 dict）
      list(it) → [['张三', '预算, 超了'], ['李四', '他说"好"']]（游标已经走过第一行了）
      再 list(it) → []  ★空且不报错 —— 呼应第 04 章「迭代器只能走一遍」
  数字列读回来 → ['数量', '单价'] 元素类型 ['str', 'str']
  ★ csv 不认识类型：那两列看着是数字，回来还是 str，int()/float() 必须你自己补一刀。
  空文件：list(reader) = []（不是 None、不是报错，就是零行 —— 对它 next() 只会拿到 StopIteration）
  只有表头：fieldnames=['a', 'b'] ｜ 数据行数=0（表头不算数据行，但它是 key 的唯一来源）
  列多于表头 → 多出来的挂在 key None 下：{'姓名': '张三', '年龄': '20', None: ['多余A', '多余B']}
  列少于表头 → 缺的补 None：{'姓名': '张三', '年龄': None, '城市': None}（None 和空串在 csv 里永远分不出来）
  DictWriter 多余键（extrasaction='raise'）→ ValueError: dict contains fields not in fieldnames: 'c'
    ★ 它抛的是 ValueError 不是 csv.Error —— 查文档不如认家谱（第 07 章那棵树）
  extrasaction='ignore' → 磁盘上只剩 b'1,2\r\n'（'c' 悄悄没了，和删列一样安静）

=== 5. dialect：分隔符 / 行终止符，以及 Sniffer 各猜出什么 ===
  delimiter=';'       → 'a;b;"c;d"\r\n'
  lineterminator=换行 → 'a,b\n'
  Sniffer 看 comma.csv  → delimiter=',' lineterminator='\r\n' quotechar='"'
  Sniffer 看 semi.csv   → delimiter=';' lineterminator='\r\n' quotechar='"'
  猜不出来时不是退回默认值，是报错：Error: Could not determine delimiter
  → Sniffer 是**猜**（数哪个字符出现得最规律），不是解析。生产代码把分隔符写死；
    猜错那次不一定报错，可能只是静默把整表读成一列。
```

`csv.reader(...)` 本身就是迭代器（→ `True`），`next(it)` 拿到的是 `list` 不是 `dict`；
`list(it)` 之后游标已经走过，再 `list(it)` 得到 `[]` —— **空且不报错**（呼应第 04 章「迭代器只能走一遍」）。
数字列读回来的元素类型是 `['str', 'str']`：csv 不认识类型，那两列看着是数字，回来还是 `str`，
`int()` / `float()` 必须你自己补一刀。空文件是 `[]`（不是 `None`、不是报错）；
只有表头时 `fieldnames=['a', 'b']`、数据行数 0（表头不算数据行，但它是 key 的唯一来源）。
列多于表头 → 多出来的挂在 key `None` 下（`{'姓名': '张三', '年龄': '20', None: ['多余A', '多余B']}`）；
列少于表头 → 缺的补 `None`（`None` 和空串在 csv 里永远分不出来）。
`DictWriter` 的多余键在 `extrasaction='raise'` 时抛的是 `ValueError: dict contains fields not in
fieldnames: 'c'` —— **不是 `csv.Error`**，查文档不如认家谱（第 07 章那棵树）；
`extrasaction='ignore'` 时磁盘上只剩 `b'1,2\r\n'`，那个键悄悄没了，和删列一样安静。
dialect 侧：`delimiter=';'` 写出 `'a;b;"c;d"\r\n'`，`lineterminator` 改换行写出 `'a,b\n'`；
`Sniffer` 对逗号文件和分号文件都能报对 `delimiter`，但猜不出来的时候**不是退回默认值，是报错**
（`Error: Could not determine delimiter`）。记住 Sniffer 是**猜**（数哪个字符出现得最规律），不是解析：
生产代码把分隔符写死，猜错那次不一定报错，可能只是静默把整表读成一列。

```
=== 6. 两个坑的原文：裸换行没加引号，和字段大到超出限制 ===
  字段里的换行没被引号包住时，磁盘上就长这样：'姓名,备注\n张三,第一行\n第二行\n'
      读出来 3 行 → [['姓名', '备注'], ['张三', '第一行'], ['第二行']]
      ★ 你以为写了 2 行数据，csv 读到 3 行：换行没被引号包住就是**行尾**，
        它没有任何办法知道那是字段内容 —— 这是把 CSV 写坏的唯一方式。
      同样内容交给 csv.writer：'张三,"第一行\n第二行"\n' → 文本 2 行，记录却是 1 条 ← 这两个数从此不是一回事
  csv.field_size_limit() 当前值 = 131072（进程级常量，能直接打数字）
  现场把限制调到 10 再读一个 100 字节的字段 → Error: field larger than field limit (10)
  调回去 = 131072 ｜ 调完照样读小字段 → [['ok', '1']]
  → 这条是**内存防护**（防一个畸形字段撑爆你），不是格式规则；真要读大字段就调它，
    但它是全局状态：在库代码里改它，等于改了同进程所有人读 CSV 的规则。

=== 7. 收口：csv 管引号和分隔符，编码、类型、行尾都不归它 ===
  ① 它管「引号与分隔符」这一套转义：逗号、双引号、换行都能进字段，
     前提是必须让它自己写引号 —— 手工 split(',') 在这套规则面前必错（第 1 段）。
  ② 它不管编码：encoding= 你自己给；Excel 导出的文件常带 BOM，用 utf-8-sig 才不被吃掉。
  ③ 它不管类型：读回来**全是 str**（第 4 段那行类型名单），int() 得自己来；
     None 与空串它也不分 ——「这格没填」和「这格填了 null 两个字」在 CSV 里同一个样。
  ④ 它不管行尾：行尾归 newline=。csv 写 \r\n + 文本层再翻译 = \r\r\n（第 3 段的字节为证）。
  → 读 CSV 的正确三件套：
      with open(p, encoding="utf-8-sig", newline="") as fh:
          for row in csv.DictReader(fh):            # 表头当 key，顺序不靠猜
              n = int(row["数量"])                   # csv 不给类型，这一刀必须你补
  ★ 最后一句：字段里出现「没被引号包住的换行」，文件本身就已经坏了，
    读它的人只会默默错位 —— 校验 CSV 时数的应该是**记录数**，不是行数。

  清理：csv-demo/ 已删除 → 还在吗 False ｜ 当前目录里 *-demo 残留 = 无
```

最后一个坑的原文：字段里的换行没被引号包住时，磁盘上就长这样 `'姓名,备注\n张三,第一行\n第二行\n'`，
读出来是 **3 行**（`['第二行']` 单独成了一条记录）。你以为写了 2 行数据，csv 读到 3 行 ——
换行没被引号包住就是**行尾**，它没有任何办法知道那是字段内容。这是把 CSV 写坏的唯一方式。
同样内容交给 `csv.writer` 出来是 `'张三,"第一行\n第二行"\n'`：文本 2 行，记录却是 1 条，
**这两个数从此不是一回事**。另一半是 `csv.field_size_limit()` 当前 `131072`（进程级常量，能直接打数字），
现场把它调到 `10` 再读一个 100 字节的字段 → `Error: field larger than field limit (10)`，调回去照样读
（`[['ok', '1']]`）。这条是**内存防护**（防一个畸形字段撑爆你），不是格式规则；
而且它是全局状态：在库代码里改它，等于改了同进程所有人读 CSV 的规则。

围栏最后那几段是脚本自己的收口，里面那三行 `with open(p, encoding="utf-8-sig", newline="") as fh:`
加 `csv.DictReader` 加 `int(row["数量"])`，就是以后读 CSV 该照抄的形状 —— 三个零件各管一件事，
少一个就掉回上面某个坑里。

**带走什么**：① 它管「引号与分隔符」这套转义 —— 逗号、双引号、换行都能进字段，前提是必须让它自己写引号，
手工 `split(',')` 在这套规则面前必错；② 它不管编码 —— `encoding=` 你自己给，Excel 导出的文件常带 BOM，
要用 `utf-8-sig` 才不被吃掉；③ 它不管类型 —— 读回来全是 `str`，`None` 与空串它也不分；
④ 它不管行尾 —— 行尾归 `newline=`，csv 写 `\r\n` + 文本层再翻译 = `\r\r\n`。
最后一句：字段里出现「没被引号包住的换行」，文件本身就已经坏了，读它的人只会默默错位 ——
校验 CSV 时数的应该是**记录数**，不是行数。

---

## 第 16 节 · json：类型表、报错原文，以及它不保存类型

「配置写成 json 总不会错吧」——这句话漏了最关键的一层：**json 存的是一串值，不是你的类型**。
`None` 和 `null` 恰好对得上，是因为那七种类型 json 里也有同名位置；`tuple`、`set`、`date`、你自己的类
在 json 的世界里根本没有格子可放。于是只有两种结局：**静默换个形状**，或者**当场 TypeError**。
静默那种更贵，因为它要到下游才算总账。这一节把这条界线逐类型量一遍，再把 json 读失败时那三个能救命的字段捞出来。

先看七种「进得去也回得来」的，和第八个假装成功的：

```
$ python3 o_json.py
=== 1. 类型往返表：进得去也回得来的，只有这七种 ===
  进 NoneType  None       → null         → 出 NoneType  None         相等=True
  进 bool      True       → true         → 出 bool      True         相等=True
  进 int       1          → 1            → 出 int       1            相等=True
  进 float     1.5        → 1.5          → 出 float     1.5          相等=True
  进 str       's'        → "s"          → 出 str       's'          相等=True
  进 list      [1, 2]     → [1, 2]       → 出 list      [1, 2]       相等=True
  进 dict      {'a': 1}   → {"a": 1}     → 出 dict      {'a': 1}     相等=True
  ★ tuple：进 tuple (1, 2) → [1, 2] → 出 list [1, 2] 相等=False
  → 判据：tuple 这一行**不报错**，但类型换了 —— 比报错危险，你到算错都不知道出错。
```

前七行是你唯一能指望的部分：进去什么类型、出来什么类型，`相等=True`。第八行才是本节的主题。
`tuple` 那一条**不报错、不警告、`dumps` 一路绿灯**，`(1, 2)` 出来是 `[1, 2]`，类型从 `tuple` 变成 `list`。
为什么这比 TypeError 危险：JSON 规范里只有一种「有序集合」，叫数组；Python 的 `list` 和 `tuple` 在它眼里
是同一个东西，谁进去都是数组，出来一律按 `list` 造。所以如果你的代码指着元组的「不可变」当哈希键、
或者靠 `isinstance(x, tuple)` 分辨「一条记录」和「一个列表」，`loads` 之后那条分支就静静走了另一路。
反过来说也成立：**`loads` 永远给不出 tuple**，你想拿回来只能自己造（见本节最后那两个钩子）。

接下来这份名单是「会响」的，响的都比不响的好办：

```
=== 2. 撞 TypeError 的名单，以及「键必须是字符串」这条硬规矩 ===
  进 set        {1, 2}                     → ★TypeError: Object of type set is not JSON serializable
  进 bytes      b'ab'                      → ★TypeError: Object of type bytes is not JSON serializable
  进 bytearray  bytearray(b'x')            → ★TypeError: Object of type bytearray is not JSON serializable
  进 complex    (3+4j)                     → ★TypeError: Object of type complex is not JSON serializable
  进 date       datetime.date(2026, 10, 3) → ★TypeError: Object of type date is not JSON serializable
  进 Point      Point(1, 2)                → ★TypeError: Object of type Point is not JSON serializable
  进 dict 键是 int：{"1": "\u7532", "2": "\u4e59"} → 回来 {'1': '甲', '2': '乙'} 键的类型 ['str', 'str']
  → ★能序列化，但键回来看是 str：int 键不报错，这是最容易被当成「没事」的一条。
  进 dict 键是 tuple {(1, 2): '键'} → ★TypeError: keys must be str, int, float, bool or None, not tuple
  同一个加 skipkeys=True（不排序）→ {"1": "int\u952e"}  ← 不认识的键**直接消失**
  但 skipkeys=True + sort_keys=True 在同一份数据上 → ★TypeError: '<' not supported between instances of 'int' and 'tuple'
    ← 键排序发生在写之前，混合类型的键先撞死在 sorted() 上（这条报错和 json 无关）
  → 判据：dumps 只接受 str/int/float/bool/None 五种键（后三种写出来都是字符串）；
    超出的一律 TypeError。skipkeys=True 是「把不能表达的悄悄扔掉」，别拿它当兜底。
```

`set`、`bytes`、`bytearray`、`complex`、`date`、自定义类，六个当场 `TypeError`，句式完全一样、只有类型名在变：
`Object of type set is not JSON serializable` —— 这一句直接抄进日志就能定位到是哪个字段。
中间那行 `int` 键才是本段最阴的一条：写出去是 `{"1": "\u7532", "2": "\u4e59"}`，回来变成 `{'1': '甲', '2': '乙'}`，
`键的类型` 打出来是 `['str', 'str']` —— 不报错、不警告，键的类型就这么换掉了；后面再拿 `1` 去取那个值，取不到，
而 `dumps` 那一步一个字都没抱怨过。为什么会这样，另一条报错里其实说了：
`keys must be str, int, float, bool or None` —— 它**接受** `int` 键，但 json 的对象只有字符串键这个位置，
所以写出去的时候 `1` 就成了 `"1"`；`bool` 和 `None` 键同理，落盘也都是字符串。
下面三行是同一条规矩的三个方向：`tuple` 键连写都写不出去；`skipkeys=True` 不是报错，是
**把表达不了的键直接删掉**，那份 dict 悄悄少一项，谁都不通知你；而 `skipkeys=True + sort_keys=True`
在同一份数据上反而炸了，炸点是 `sorted()` 拿 `int` 和 `tuple` 比大小 —— 一条跟 json 半点关系没有的报错，
出现的位置却是 `dumps` 里面。判据就一句：`skipkeys` 是「把不能表达的悄悄扔掉」，别拿它当兜底。

写侧的开关决定两件事：中文怎么落盘，以及这份文件的 diff 好不好看：

```
=== 3. ensure_ascii：默认全 \uXXXX，ASCII 安全，代价是长度 ===
  默认（True）      → {"\u540d\u5b57": "\u5f20\u4e09", "\u5907\u6ce8": "\u9884\u7b97"}
  ensure_ascii=False → {"名字": "张三", "备注": "预算"}
  字符数 len()：64 vs 24 ｜ 落盘字节数 encode('utf-8')：64 vs 40
  两份 loads 回来相等吗 → True（\u5f20 和「张」是同一个字符）
  → 判据：ensure_ascii=True 让输出**只用 ASCII 字符**，塞进任何不明管道都不会变乱码；
    代价就在上面那行数字里：一个中文写成 \uXXXX 是 6 个 ASCII 字符。文件自己已经写了 encoding=，落盘用 False；
    只有「不知道下游什么编码」的场合（邮件头、老 HTTP 头、日志采集）才值回 True。

=== 4. 排版三开关：indent / sort_keys / separators（同一份 dict）===
  ① 默认 → '{"meta": {"a": 2, "z": 1}, "name": "\\u53d6\\u8bc1", "tags": ["b", "a"]}'
  ② separators=(',',':') → '{"meta":{"a":2,"z":1},"name":"\\u53d6\\u8bc1","tags":["b","a"]}'   ← 最紧凑，机器间传输用
  ③ indent=2 + sort_keys=True（人读的那种写法，键按 sorted 落定，diff 才不会假变更）：
      {
        "meta": {
          "a": 2,
          "z": 1
        },
        "name": "\u53d6\u8bc1",
        "tags": [
          "b",
          "a"
        ]
      }
  sort_keys=False 的键序 = 插入序（b 先进去）→ '{"b": 1, "a": 2}' ｜ True → '{"a": 2, "b": 1}'
  ★ indent 会顺手改写 separators 的默认值：不带 indent 时项之间是 ', '，带了就只剩 ','：
    不带 indent → '{"a": 1, "b": 2}'
    带 indent=2 → '{\n  "a": 1,\n  "b": 2\n}'   ← 冒号后那个空格没变，变的是逗号后面
    所以风格要固定就显式写 separators=，别指望两处默认值一致（第 07 章那条：默认值是别人的决定）。
```

同一份 dict 三种写法：默认那行最刺眼的是键序 —— `sort_keys=False` 时键序就是**插入序**（`b` 先进去就 `b` 在前）。
你的程序这次先算 `b`、下次先算 `a`，文件内容没变、git 里红一片，这就叫假变更。
`sort_keys=True` 把键序交给 `sorted()`，两次生成才可比。第二件事更容易被忽略：
`indent` 一开，**separators 的默认值跟着变了** —— 不带 `indent` 时项之间是 `', '`，带了就只剩 `','`，
而冒号后面那个空格两边都不动。所以「风格要稳定」的唯一办法是显式写 `separators=`，
别指望两处默认值互相一致（第 07 章那条规矩在这里落地：默认值是别人的决定）。
`ensure_ascii` 那两个数很直白：同一份 dict，默认那份 64 个字符、`False` 那份 24 个；
落盘字节数 64 对 40 —— 因为一个中文写成 `\u5f20` 这种形式就是 6 个 ASCII 字符。
而两份 `loads` 回来仍然相等（`\u5f20` 和「张」是同一个字符），所以坏的从来不是内容，只是体积和可读性。
`True` 的好处也真：输出只用 ASCII 字符，塞进任何不明管道都不会变乱码；但文件自己已经写了 `encoding=`，
落盘没理由继续用它 —— 只有「不知道下游什么编码」的场合（邮件头、老 HTTP 头、日志采集）才值回 `True`。

报错这一侧 json 做得相当好，好到没有借口只打一句「读不到」：

```
=== 6. 三样报错原文 + JSONDecodeError 的家谱（lineno / colno / pos 是三个字段）===
  多余逗号   输入 '{"a": 1,}'
      JSONDecodeError: Illegal trailing comma before end of object: line 1 column 8 (char 7)
      e.msg='Illegal trailing comma before end of object' ｜ e.lineno=1 e.colno=8 e.pos=7 ← pos 直接拿去切片就能指出错在哪一格
  用了单引号  输入 "{'a': 1}"
      JSONDecodeError: Expecting property name enclosed in double quotes: line 1 column 2 (char 1)
      e.msg='Expecting property name enclosed in double quotes' ｜ e.lineno=1 e.colno=2 e.pos=1 ← pos 直接拿去切片就能指出错在哪一格
  文档被截断  输入 '{"a":'
      JSONDecodeError: Expecting value: line 1 column 6 (char 5)
      e.msg='Expecting value' ｜ e.lineno=1 e.colno=6 e.pos=5 ← pos 直接拿去切片就能指出错在哪一格
  issubclass(json.JSONDecodeError, ValueError) → True
  __mro__ → JSONDecodeError → ValueError → Exception → BaseException → object
  → 所以本脚本第 1、2 段那些 one()/back() 用 except (TypeError, ValueError) 就够接住它，
    但也**只**接住它：except Exception 会顺手把 MemoryError 之外的一整棵树都咽掉（第 07 章）。
  顺带一条：json 不认识 Python 的字面量。loads('null') → None，loads('None') → '★JSONDecodeError: Expecting value: line 1 column 1 (char 0)' ← 配置文件里手打 None 是最常见的「读不进来」
```

三份坏输入、三句原文，每句都自带 `line 1 column 8 (char 7)` 这样的位置；`e.msg` 是那句人话，
`e.lineno` / `e.colno` / `e.pos` 是三个字段 —— 注意 `colno` 从 1 数、`pos` 从 0 数，
所以 `pos` 可以直接拿去切片。原文每行末尾都缀了同一句判据：「pos 直接拿去切片就能指出错在哪一格」。
家谱那两行是本章反复用的工具：`issubclass(json.JSONDecodeError, ValueError) → True`，
MRO 是 `JSONDecodeError → ValueError → Exception → BaseException → object`。
结论有两半：接住它只要 `except (TypeError, ValueError)` 就够；但 `except Exception`
会把一整棵树连坐咽掉，把「配置写歪了」和「程序有 bug」压成同一次沉默（第 07 章那棵树）。
最后一行是新手命中率最高的一条：json 只认 `null` 这个写法，配置文件里手打 Python 的 `None`，
报的就是那句 `Expecting value: line 1 column 1 (char 0)`。

值和类型的最后一层，藏在「dump/load 吃的是流」和「数值精度与三个 inf 雷」这两段里：

```
=== 7. dump/load 吃的是流：指针前移，第二次 load 不是重读一遍 ===
  第一次 json.load → {'a': 1, 'b': [1, 2]}  ｜ 读完 fh.tell()=21（指针已经到末尾）
  第二次 json.load（同一个句柄）→ JSONDecodeError: Expecting value: line 1 column 1 (char 0)
  seek(0) 之后第三次 → {'a': 1, 'b': [1, 2]}  ← 内容没坏，坏的是你以为指针会自己回去
  → 判据：'Expecting value: line 1 column 1 (char 0)' 出现在**第二次** load，
    不是文件坏了，是读到了末尾。要么 seek(0)，要么 load 一次存进变量。
  分工一句话：json.dump(obj, fh) 写流 ｜ json.dumps(obj) 返 str ｜ 本机这份文件 21 字节 = dumps 的 21 字符（纯 ASCII 时两者才相等，含中文就别当同一回事）

=== 8. 数值精度与三个 inf 雷 ===
  大整数 4611686018427387904 → 4611686018427387904 类型 int ｜ 相等=True（没被变成 float）
  带不带指数就不一样：loads '100' → 100 int ｜ loads '1e2' → 100.0 float
  1e999（超出 float 上限）：loads → inf ｜ math.isinf → True  ← Python 不报错，直接给你一个 inf
  Python 认常量的名字：loads 'NaN' → nan ｜ loads 'Infinity' → inf
  ★ 反向就出事：dumps(inf) → Infinity ｜ dumps(nan) → NaN ← 这两个**不是合法 JSON**，别的语言一读就崩
  parse_constant 是唯一的拦截口：loads '[NaN, Infinity]' 带钩子 → [None, None]（钩子收到常量名，返回你允许的替身）
  浮点原样进原样出：dumps(0.1+0.2) → 0.30000000000000004 ｜ loads 回来 == 0.1+0.2 → True
  → 判据：NaN/Infinity 是 Python 的宽容，不是 JSON 的功能；跨语言传数就先把 inf 挡在门外。
```

第 7 段那个坑几乎每个人都踩过一次：第一次 `json.load` 读完，`fh.tell()=21`，指针已经在末尾；
第二次 `load` 拿到的是**和第 6 段一字不差的** `Expecting value: line 1 column 1 (char 0)` ——
文件没坏、内容没变，坏的是「你以为指针会自己回去」。要么 `seek(0)`，要么 load 一次存进变量。
`dump`/`dumps` 的分工就一句话：前者写流、后者返 `str`；那份文件 21 字节等于 `dumps` 的 21 字符，
是**纯 ASCII 才成立**的巧合，含中文就别把这两个数当同一回事。
第 8 段管数值：`4611686018427387904` 回来还是 `int`，json 不强制走浮点；但类型由**写法**决定 ——
`100` 是 `int`，`1e2` 就是 `100.0`。三颗雷是同一件事的两面：`1e999` 在 `loads` 阶段就变成 `inf`，
Python 不报错、直接给你一个无穷大；反方向 `dumps(inf)` 写出 `Infinity`、`dumps(nan)` 写出 `NaN`，
而这两个名字**不是合法 JSON**，别的语言一读就崩。唯一的拦截口是 `parse_constant`
（实测那份 `[NaN, Infinity]` 带钩子回来是 `[None, None]`，钩子收到常量名、返回你允许的替身）。
浮点本身倒是很规矩：`0.1 + 0.2` 写出 `0.30000000000000004`，读回来 `== 0.1 + 0.2` 还是 `True` ——
原样进原样出，别指望 json 顺手帮你四舍五入。

还有一类上面几段没演的东西：自定义类型不是没办法，写侧给 `default=`、读侧给 `object_hook=`，
一边拆成 json 认识的形状、一边装回来。但**那个用来标记类型的键名和字段是你在造协议**，
JSON 规范一个字都没规定；换一门语言读这份文件的人，不读你的文档就只会拿到一个怪 dict。协议要当接口管：写进文档、加版本。

**带走什么**：① json 是**值**的格式，不是**类型**的格式 —— 七种类型能原样回来，`tuple` 静默降级成 `list`，
`set`/`bytes`/`date`/自定义类当场 `TypeError`，`int` 键回来变 `str`；② 报错带 `msg`/`lineno`/`colno`/`pos`
四个可用的结构化字段，而且 `JSONDecodeError` 是 `ValueError` 的儿子，接它就用 `except (TypeError, ValueError)`，
别升格成 `except Exception`；③ 三个雷：`ensure_ascii` 的体积、`load` 两次不 `seek` 必失败、
`inf`/`nan` 能被 Python 写出去但那是坏 JSON。真要跨语言校验、又要保类型，就别硬塞 json ——
除非你接受类型丢失，并且把 `default=` / `object_hook=` 那套自造的协议当接口管起来。

---

## 第 17 节 · 怎么把「写完」变成磁盘上的事实

`write()` 返回了、`with` 块走完了、程序自己打了「已保存」——这三件事和「磁盘上有这份数据」之间
隔着**用户态缓冲区、内核页缓存、目录项**三层。「写完」「落盘」「原子」是三件不同的事，
各有各的写法，混用一次就要付一次半截文件的账。这一节把三层各自量一遍，顺便老实标出哪些结论本机验不了。

```
$ python3 p_atomic_write.py
=== 1. 写完 ≠ 落盘：不 close 不 flush 时另一个句柄读到多少（0 / 部分 / 全部都有）===
  本机 io.DEFAULT_BUFFER_SIZE = 131072 字节（①②③ 的分界线就是它）
  ① 一次 write 200 字节（小于缓冲区）    ：不 flush=0 ｜ flush 后=200 ｜ fsync 后=200 ｜ 应有=200
  ② 分 200 次各写 1000 字节         ：不 flush=131000 ｜ flush 后=200000 ｜ fsync 后=200000 ｜ 应有=200000
  ③ 一次 write 200000 字节（大于缓冲区） ：不 flush=200000 ｜ flush 后=200000 ｜ fsync 后=200000 ｜ 应有=200000
  → 三个数并排：flush 买到「用户态缓冲区 → 内核」，fsync 买到「请求内核别丢」；从另一个句柄读，
    这两笔数字一模一样 —— read 走的是内核页缓存，压根没下磁盘。
  ★「fsync 之后拔电会不会丢」本机验不了（无可控掉电手段），这条**未验证**：fsync ≠ 已物理写入介质。
```

`io.DEFAULT_BUFFER_SIZE = 131072` 就是那三条曲线的分界线，三个数字对着看才明白「写完」有多靠不住：
① 一次写 200 字节，缓冲区装得下，另一个句柄读到 **0** —— 你那句 `write` 的返回值只保证「已交给自己的缓冲」。
② 分 200 次各写 1000 字节，读到的是 **131000**：既不是 0 也不是 200000。这个数字很讲道理 ——
缓冲区装到装不下的那一刻才往外倒一次，倒出去的正好是 131 个整包（131000 字节），剩下 69000 还攥在你进程里。
「部分可见」比「完全看不见」危险得多，因为校验代码太容易写成「长度大于零就认」。
③ 一次写 200000 字节，超过缓冲区，当场 200000 —— 装不下就必须往下倒。
所以「别人什么时候能看到我的写」取决于你写了多大、什么时候溢出去，跟你的函数有没有 return 无关。
再往下一层：`flush` 买到的是「用户态缓冲区 → 内核」，`fsync` 买到的是「请求内核别丢」；而从另一个句柄读，
这两笔数字**一模一样**，因为 `read` 走的是内核页缓存，压根没下磁盘 —— 换句话说 `fsync` 之后数据在
「交给页缓存」这一档，再往下就没有本机可控的手段了。
★ 最后一行必须照抄进你的判断里：`fsync` 之后拔电会不会丢，本机没有可控掉电手段，这条**未验证**。
`fsync` ≠ 已物理写入介质，谁跟你说「加了 fsync 就掉电不丢」，让他拿出断电实测来。

半截文件长什么样，脚本用一次可控崩溃给你演了：

```
=== 2. 半截文件事故：open(p,'w') 之后崩在写入中间，旧数据就没了 ===
  接住的异常：RuntimeError: 序列化到一半崩了 ｜ 原文件字节数 41 → 172
  磁盘上剩下的尾巴：b'42,43,44,45,46,47,48,49,'
    → JSONDecodeError: Expecting value: line 1 column 167 (char 166)
  → with 只保证 close，不保证写全：事故不是「新的没写成」，是**旧的也没了** —— 没备份、没事务、不报错。

=== 3. 同目录临时文件 + os.replace：读者只会看到完整旧版或完整新版 ===
  旧版本长度=30000 新版本长度=70000 ｜ 三次现场探测读到的长度 = (30000, 30000, 70000)
  → 每次都严格等于旧或新 → True ｜ 读到中间值的次数 = 0
  replace 之后目录里的条目 = ['buf.bin', 'conf.json', 'settings.txt']　← .tmp 已经没了
  → 判据就印在屏幕上：读者能看到的长度只允许是「旧长度」或「新长度」之一，出现第三种长度就是有
    人在原文件上就地写。
```

第 2 段：`open(p,'w')` 在打开那一刻就把原文件清空了，然后代码崩在序列化中间 ——
接住的是自己抛的 `RuntimeError`，磁盘上只剩 172 字节的尾巴 `b'42,43,44,45,46,47,48,49,'`，读它就是那句
`Expecting value: line 1 column 167 (char 166)`。事故的形状不是「新的没写成」，是**旧的也没了**：
没备份、没事务、不报错，`with` 只保证 `close`，它从来不保证写全。
第 3 段是解药：新内容写进同目录的 `.tmp`，写完再 `os.replace`。读者被探测三次，
看到的长度是 `(30000, 30000, 70000)` —— 每次都严格等于旧长度或新长度，读到中间值的次数是 **0**，
`replace` 之后目录里连 `.tmp` 都不剩。判据就印在屏幕上：读者能看到的长度只允许是「旧」或「新」之一，
出现第三种长度，就是有代码在原文件上就地写。

权限位这件事看起来跟原子性无关，实际上它是同一个 `rename` 的副作用：

```
=== 4. ★mkstemp 的权限会跟着文件走：os.replace 之后它还是 0600 ===
  tmp 名 .perm????????.tmp（8 个随机字符不打）replace 前没 chmod       ：建好时=0o600 ｜ replace 之后=0o600 ｜ 等于本该有的 0o644 吗：False
  tmp 名 .perm????????.tmp（8 个随机字符不打）replace 前补 chmod(0o644)：建好时=0o600 ｜ replace 之后=0o644 ｜ 等于本该有的 0o644 吗：True
  → rename(2) 只改目录项、不改 inode：临时文件什么权限，转正之后就是什么权限。mkstemp 故意给 0600
    （防别人抢注），所以它**不能**直接当配置文件的 tmp：要么 replace 前 chmod，要么改用普通 open() 建 tmp —— 那才是 umask 0o022 给你的 0644。
```

两行只差一句 `chmod`，结局差一层用户：`mkstemp` 建的 tmp 是 `0o600`，不 `chmod` 直接 `replace`，
转正之后**还是 0o600**；`replace` 之前补一句 `os.chmod(tmp, 0o644)`，转正才是 0o644。
原因写在第三行：`rename(2)` 只改目录项、不改 inode —— 权限、属主、inode 号全都跟着那个临时文件走，
名字换了而已。而 `mkstemp` 故意给 `0600`（防别人抢注你的临时文件名），
所以它**不能**直接拿来当配置文件的 tmp：要么接受每次 `replace` 前 `chmod`，
要么改用普通 `open()` 建 tmp，那才是 `umask 0o022` 给你的 0644。
漏掉那次 `chmod` 的后果不是报错，而是转正之后的文件只有你自己读得到，同组和其他用户被挡在外面。

并发写同一个目标时，两种写法的差别是「谁变脏」：

```
=== 5. 崩溃一致性对比：A 组就地写 vs B 组 tmp + replace（各两个子进程）===
  A 组 两个子进程（python3 -c …，全 capture）都正常退出 → True ｜ 最终字节数 = 200000 ｜ 甲的完整长度 = 200000 ｜ 乙的完整长度 = 40000
  → 内容等于甲的完整 payload → False ｜ 等于乙的完整 payload → False
     顺序被两个标志文件钉死，结果 =「乙的整段 + \x00 空洞 + 甲的后半」，长度 200000 恰好等于甲的 200000。
  → 「长度对得上」根本不能当完整性判据：等于事故串 → True ｜ 有 \x00 空洞 → True
  B 组 两个子进程都正常退出 → True ｜ 最终字节数 = 40000（只允许 200000 或 40000）｜ 落在候选之一 → True
  → 最终内容 = 乙（标志文件排在后面的那个）完整 payload → True ｜ = 甲的完整 payload → False
  → 出现半截 / 混合 / 空洞 → False ｜ replace 之后残留 .tmp 个数 = 0
  → B 组无论谁赢，输的那一方脏的都是**自己那个 tmp**，碰不到目标文件：结果集只有 {完整甲, 完整乙}，没有第三种形状 —— 这就叫原子。

=== 6. rename / replace 的边界：同目录才谈原子，跨设备直接失败 ===
  本机 os.rename 到「已存在的普通文件」→ 成功，目标现在='源'
    → os.rename 撞已存在的目录：IsADirectoryError: [Errno 21] Is a directory
    → Path.rename 撞已存在的目录：IsADirectoryError: [Errno 21] Is a directory
  → 本机是 POSIX：rename/replace、Path.rename/Path.replace 都落到同一个 rename(2)，对已存在的**普通
    文件**都会替换；「rename 报错、只有 replace 强制覆盖」是 Windows 语义，本机验不了、**标未验证**。
  Path.replace 到已存在普通文件 → 成功；q1 还在吗=False s2 内容='pathlib 源2'
  借道另一台文件系统（basename=shm，与 atomic-demo/ 同设备吗=False）
    → OSError: [Errno 18] Invalid cross-device link ｜ errno.EXDEV=18
      os.EXDEV 实测：AttributeError: module 'os' has no attribute 'EXDEV'
  ★所以临时文件必须和被覆盖的文件在**同一个目录**（连文件系统都不用比了）：跨设备时
    rename/replace 都抛 EXDEV，只能退回「复制 + 改名」，那时就再也不原子了。
```

A 组就地写：两个子进程都正常退出（**退出码 0 不等于内容对**，这是本段最实用的一句），
最终 200000 字节 —— 恰好等于甲的完整长度，但内容既不是甲也不是乙（两个判据都是 `False`），
真实形状是「乙的整段 + \x00 空洞 + 甲的后半」。那个空洞是免费午餐：写偏到远处时，
中间没人写过的字节读出来就是 0。于是本段的判据被打成反例：「长度对得上」根本不能当完整性判据，
屏幕上那两句是「等于事故串 → True」「有 \x00 空洞 → True」。
B 组各自写自己的 tmp 再 `replace`：最终长度只在 `200000` 和 `40000` 里选，落在候选之一 → `True`，
内容 = 完整乙（标志文件排在后面的那个）→ `True`，出现半截/混合/空洞 → `False`，残留 `.tmp` 个数 = `0`。
谁赢都可能，但输的那一方脏的是**自己那个 tmp**，碰不到目标文件 —— 结果集只有 {完整甲, 完整乙}，
没有第三种形状，这就叫原子。
第 6 段划边界：本机是 POSIX，`os.rename` / `Path.rename` / `os.replace` / `Path.replace`
都落到同一个 `rename(2)`，对已存在的**普通文件**都会替换（撞已存在的目录则一律 `IsADirectoryError: [Errno 21]`）；
「rename 报错、只有 replace 强制覆盖」是 Windows 语义，本机验不了、**标未验证**。
跨设备那一档才是真正的硬约束：借道另一台文件系统（本机挑的是 `shm`，和 demo 目录不同设备），
`rename` 和 `replace` 都抛 `OSError: [Errno 18] Invalid cross-device link`，`errno.EXDEV=18` ——
只能退回「复制 + 改名」，那时就再也不原子了。顺带一条小的：`os.EXDEV` 这个写法在本机不存在
（`AttributeError`），要拿这个数得从 `errno` 模块里取。
★ 所以「临时文件必须和被覆盖的文件在**同一个目录**」不是洁癖，它是原子性的前提；同目录连文件系统都不用比了。

脚本自己在第 7 段把上面这些压成四行，顺序就是它们出现的顺序：

```
=== 7. 收口：配置文件的安全写法只有这四行 ===
  ① tmp 与目标**同目录**：tmp = path.with_name(path.name + '.tmp')（第 3 段这句；顺带堵死 EXDEV）
  ② 一次写全 → f.flush() → os.fsync(fd) → 再 close（fsync 只到「交给内核」为止，掉电未验证）。
  ③ os.chmod(tmp, 0o644) → os.replace(tmp, path)（第 4 段：权限跟 inode 走，不跟名字走）。
  ④ 任何一步崩了都要 tmp.unlink(missing_ok=True)，别让 .tmp 留在目录里等下次咬人。
  → with 只保证 close，不保证事务；「写完」「落盘」「原子」是三件不同的事，各有各的写法。
  → 有没有留半截别靠感觉：清点 .tmp 个数（第 5 段 B 组 = 0），并**故意留一个再数一次**，响过才算数。
```

**带走什么**：配置文件的安全写法只有四行 ——
① tmp 与目标**同目录**（顺手堵死 EXDEV）；
② 一次写全 → `f.flush()` → `os.fsync(fd)` → 再 close，且清楚 `fsync` 只买到「请求内核别丢」（掉电未验证）；
③ `os.chmod(tmp, 0o644)` 然后 `os.replace(tmp, path)`，权限跟 inode 走、不跟名字走；
④ 任何一步崩了都要 `tmp.unlink(missing_ok=True)`，别让 `.tmp` 留在目录里等下次咬人。
再加一条自我要求：有没有留下半截，别靠感觉去猜，去清点 `.tmp` 的个数 —— 而且要**先造一个残留再数一次**，
响过才算数 —— 这条在第 19 节被做成了一个真守卫，在第 20 节被做成了清单。

---

## 第 18 节 · 大文件、分块、seek/tell 的真实形状

「往 `seek` 里塞的那个数是不是就是字节偏移」——在文本模式下这件事有三种落点，其中只有一种**当场报错**，
另外两种一个不响：一种等 `read` 才炸，一种连 `read` 都不炸。
这一节先造一个「账算得出来」的 2MB 文件，把四种读法的字节账钉死，再分别拿二进制和文本去试 `seek`/`tell`，
最后把 `tell()` 到底是什么东西说清楚：它是**凭据**，不是字符数。

```
$ python3 q_bigfile_seek.py
=== 1. 先造一份「账算得出来」的 2MB 文件 ===
  行数=32000 每行字节=63 总字节=2016000 ｜ 用行数×每行算出来=2016000 ｜ 两者一致 → True
  内容指纹（md5 前 8 位）= 8d900bdb ｜ 文件不超过 4MB，不把磁盘和 IO 打爆
  ★摘要是拿来**证明四种读法拿到同一份字节**的，不是校验和教学；它由确定内容算出，所以每跑一样。

=== 2. 四种读法的字节账：块数、总字节、指纹，一个都不能错 ===
  read() 一次读完                 ：块数=1      期望=1      总字节=2016000  指纹=8d900bdb 逐字节相同=True
  read(65536) 分块循环            ：块数=31     期望=31     总字节=2016000  指纹=8d900bdb 逐字节相同=True
  iter(lambda: f.read(n), b'')：块数=31     期望=31     总字节=2016000  指纹=8d900bdb 逐字节相同=True
  逐行迭代 for line in f          ：块数=32000  期望=32000  总字节=2016000  指纹=8d900bdb 逐字节相同=True
  → 四种读法的总字节只有一个值 → True（2016000）｜指纹只有一个值 → True｜逐字节相同全是 True → True
    分块的块数是 ceil(总字节/块大小)（最后一块不满），逐行迭代的「块」正好就是行；
    iter(可调用, 哨兵) 和 while 手写判空是同一件事的两种写法，下面的内存方向也一致。

=== 3. 内存只看方向，不看数字（tracemalloc 峰值只用来比大小）===
  峰值(一次读完) > 峰值(分块) → True　差在一口气攥着多少字节
  峰值(一次读完) > 峰值(逐行) → True
  峰值(分块) > 峰值(逐行) → True　块越大峰值越高，跟文件总大小无关
  峰值(iter 哨兵) 与峰值(read(n) 分块) 落在同一档 → True
  ★内存绝对值每跑必抖，本节只读方向、不读数字；上面用的都是倍数级别的差距，
    贴边的比较（1.0x 那种）本机也不会印出来 —— 那是掷硬币，不是结论。
```

为什么要先造一个能算的文件：后面要比四种读法，比的前提是**它们拿到同一份字节**。
32000 行 × 每行 63 字节 = 2016000，行数×每行和实际字节数一致，指纹 md5 前 8 位 `8d900bdb`，
尺寸压到 2MB（不超过 4MB）—— 够把缓冲区别摊开，又不至于把磁盘和 IO 打爆。
四种读法的块数是 1 / 31 / 31 / 32000：一次读完 1 块；`read(65536)` 分块 31 块，
因为块数 = ceil(总字节/块大小)，最后一块不满（2016000 - 30×65536 = 49920，所以「31 块」里有一块是零头）；
`iter(lambda: f.read(n), b'')` 和它同块数、同指纹 —— 这两者本来就是同一件事的两种写法，
`iter(可调用, 哨兵)` 只是把 `while` 手写判空收进了一行；逐行迭代的「块」正好就是行，所以 32000。
四个方向的内存只用比大小：一次读完 > 分块 > 逐行，`iter` 哨兵和 `read(n)` 分块同一档。
注意本节**不印绝对值**：`tracemalloc` 的峰值每跑必抖，只有倍数级别的差距才敢印到屏幕上，
贴边的 1.0x 那种比较就算印出来也是掷硬币 —— 这条纪律比任何一条结论都值钱。

二进制模式的 `seek`/`tell` 是最诚实的，因为那里偏移就是字节：

```
=== 4. 二进制的 seek/tell：三种 whence 各自把游标变成什么 + 按偏移随机读第 N 行 ===
  seek(0, 0) ← 回到文件头，tell()=0
  seek(2000, 1) ← 从当前位置往后 2000，tell()=2000
  seek(0, 1) ← 问现在在哪，tell()=2000
  seek(-63, 2) ← 从末尾退一整行，tell()=2015937
  seek(0, 2) ← 跳到末尾，tell()=2016000
  自己扫一遍建「行号 → 字节偏移」索引：{1: 0, 16000: 1007937, 32000: 2015937}（每行 63 字节，所以是等差数列）
    seek(0) 后 readline() 的前 12 字节 = b'000001 | \xe4\xb8\xad' 行号对不对 → True
    seek(1007937) 后 readline() 的前 12 字节 = b'016000 | \xe4\xb8\xad' 行号对不对 → True
    seek(2015937) 后 readline() 的前 12 字节 = b'032000 | \xe4\xb8\xad' 行号对不对 → True
  → 大文件随机访问的正确姿势：**先建偏移索引，再用偏移读**；用「读 N 行」定位是 O(N)。
```

`whence` 三种各自把游标变成什么，五行讲完：`seek(0, 0)` 回文件头 → tell=0；`seek(2000, 1)` 从当前往后
2000 → 2000；`seek(0, 1)` 什么也不动、纯粹「问现在在哪」（这就是 `tell()`）；`seek(-63, 2)` 从末尾退一整行
→ 2015937 = 2016000 - 63；`seek(0, 2)` 跳到末尾 → 2016000。二进制里这三档全可靠。
下半段是「按偏移随机读第 N 行」的正确做法：先扫一遍建「行号 → 字节偏移」索引，
这里因为每行定长 63，索引是等差数列 `{1: 0, 16000: 1007937, 32000: 2015937}`（16000 行那个偏移正好
= (16000-1)×63 = 1007937，两个数互相核对得上）。然后 `seek` 到那个偏移、再 `readline()`，三次都拿到对的行内容 ——
`b'016000 | \xe4\xb8\xad'` 里那串 `\xe4\xb8\xad` 就是「中」的 utf-8 三字节，二进制模式给你的是字节、不翻译。
真实日志行不等长，索引必须自己扫出来存好。结论一句：**先建索引再用偏移读**；
用「读 N 行」来定位是 O(N)，而且行号不是地址。

文本模式是这一节的主菜，也是大多数人第一次撞上就以为是 bug 的那条：

```
=== 5. ★★文本模式的 seek：tell() 发的是凭据，不是字符数 ===
  utf-8   读完第 1 行 tell()=31  ｜该行字符数=11｜文件总字节=78  ｜把 31 当字节偏移去二进制里切，切出来正好是这一行 → True
  utf-16  读完第 1 行 tell()=24  ｜该行字符数=11｜文件总字节=58  ｜把 24 当字节偏移去二进制里切，切出来正好是这一行 → True
  utf-32  读完第 1 行 tell()=48  ｜该行字符数=11｜文件总字节=116 ｜把 48 当字节偏移去二进制里切，切出来正好是这一行 → True
  ★规范只保证「同一个流」认这张凭据。本机三种编码它恰好等于字节偏移，但别依赖：解码器手里攥着
    半个字符时 CPython 会把内部状态并进去；换文件、换编码都不保证还是这个数。
  seek(cookie=31) 再读一遍：两次内容完全一致 → True ｜ 从那里开始的头 9 个字符 = '第二行中文尾巴在这'
  ③ 不取凭据、自己塞一个字节数进去（含中文的文件；本机实测，报错原样打印）：
    utf-8   seek(0) → 返回 0，read(4) 读到 '第一行中'　★没报错 ≠ 落点就是你要的那个
    utf-8   seek(3) → 返回 3，read(4) 读到 '一行中文'　★没报错 ≠ 落点就是你要的那个
    utf-8   seek(7) → 返回 7（**seek 没报错**），read 才炸：UnicodeDecodeError: 'utf-8' codec can't decode byte 0xa1 in position 0: invalid start byte
    utf-16  seek(2) → 返回 2，read(4) 读到 '第一行中'　★没报错 ≠ 落点就是你要的那个
    utf-16  seek(4) → 返回 4，read(4) 读到 '一行中文'　★没报错 ≠ 落点就是你要的那个
    utf-16  seek(7) → 返回 7（**seek 没报错**），read 才炸：UnicodeDecodeError: 'utf-16-le' codec can't decode bytes in position 10-11: illegal UTF-16 surrogate
    utf-32  seek(4) → 返回 4，read(4) 读到 '第一行中'　★没报错 ≠ 落点就是你要的那个
    utf-32  seek(7) → 返回 7（**seek 没报错**），read 才炸：UnicodeDecodeError: 'utf-32-le' codec can't decode bytes in position 0-3: code point not in range(0x110000)
    utf-8   seek(-5) → 当场 ValueError: negative seek position -5
    utf-8   seek(999999) → 返回 999999，read(4) 读到 ''　★没报错 ≠ 落点就是你要的那个
    utf-16 每字符 2 字节 + 2 字节 BOM，utf-32 每字符 4 字节 + 4 字节 BOM：所以 utf-16 里 seek(2) 落在
    第 1 个字符**之前**，utf-32 里 seek(4) 才落在第 1 个字符上；落在半个码元上的直接抛。
  → 三种失败姿态本机全实测到了：负数当场 ValueError、切到字符中间的等 read 才 UnicodeDecodeError、
    越过末尾的（999999）不报错也不提醒，read 只给空串。报错里的解码器名是 utf-16-le/utf-32-le ——
    那是 BOM 探测之后才定下来的，你写的明明是 utf-16。
    seek(0, 1) → 返回 0（tell=0）
    seek(5, 1) → UnsupportedOperation: can't do nonzero cur-relative seeks
    seek(0, 2) → 返回 78（tell=78）
    seek(10, 2) → UnsupportedOperation: can't do nonzero end-relative seeks
    seek(-5, 2) → UnsupportedOperation: can't do nonzero end-relative seeks
  → 文本模式只有 whence=0 的两种用法可靠：seek(0) 和 seek(当初 tell() 拿到的 cookie)；
    非零的 cur/end 相对定位直接 io.UnsupportedOperation，随手塞的字节数要么炸要么静默错位。
```

先看第 5 段开头三行：utf-8 / utf-16 / utf-32 读完同一行（11 个字符）之后 `tell()` 分别是 31 / 24 / 48，
文件总字节分别是 78 / 58 / 116 —— 三个 `tell()` 拿去二进制里切都「正好是这一行」。
这就是陷阱本身：**本机恰好等于字节偏移，规范却不保证**。规范只保证「同一个流」认这张凭据。
CPython 在解码器手里攥着半个字符时，会把内部状态并进那个数；换个文件、换种编码，它就不再是偏移。
所以唯一该做的事是把 `tell()` 的返回值当**不透明 cookie** 存下来，原样还给同一个流的 `seek()` ——
`seek(cookie=31)` 之后重读，两次内容完全一致，从那里开始的头 9 个字符是 `第二行中文尾巴在这`。
③ 是不取凭据、自己塞字节数的十种落点，本机三种失败姿态全出现了：
- **负数当场就响**：`seek(-5)` → `ValueError: negative seek position -5`。这是最幸运的一种。
- **切到字符中间的，`seek` 不响、`read` 才炸**：`utf-8 seek(7)` 返回 7（文件都打开着、`seek` 还成功了），
  等到 `read` 才 `UnicodeDecodeError: 'utf-8' codec can't decode byte 0xa1 in position 0: invalid start byte`；
  utf-16 那一档炸在 `illegal UTF-16 surrogate`、utf-32 炸在 `code point not in range(0x110000)`。
  注意报错里的解码器名字是 `utf-16-le` / `utf-32-le` —— 那是 BOM 探测之后才定下来的，你写的明明是 `utf-16`。
- **越过末尾的不响也不提醒**：`seek(999999)` 返回 999999，`read(4)` 给一个空串 `''`，安静得像读完了。
最难发现的是中间那一档「没报错」的静默错位：`utf-8 seek(0)` 读到 `第一行中`、`seek(3)` 读到 `一行中文` ——
两个都成功、两个都不是你要的落点，因为偏移按**字节**算，而你心里数的是字符，中间这一层没人替你换算。
`utf-16` 那一档两种结局都出现过：`seek(2)` 和 `seek(4)` 都安静地给你内容（每字符 2 字节 + 2 字节 BOM，
`seek(2)` 落在第 1 个字符之前），`seek(7)` 切进半个码元才炸；`utf-32` 要 `seek(4)` 才正好落在第 1 个字符上。
最后五行的 whence 实验把结论钉死：文本模式 `seek(0, 1)`、`seek(0, 2)` 还能问位置（本机返回 0 和 78），
只要偏移**非零**就直接 `UnsupportedOperation: can't do nonzero cur-relative seeks` /
`can't do nonzero end-relative seeks`。所以文本模式只有两种用法可靠：`seek(0)`，
和 `seek(当初 tell() 拿到的 cookie)`；要相对定位、要从末尾倒着算，就开二进制模式。

并发追加这一档，可证的部分和不可证的部分分得很干净：

```
=== 7. 追加与并发：'a' 模式两个子进程各写 40 行 ===
  两个子进程（python3 -c …）都正常退出 → True
  行数 = 80（应为 80）｜ 每行都以 甲-/乙- 开头且完整 → True
  → 两行都在、没有任何半截行（集合完全相同）→ True
     谁先谁后本机不可预测，所以**不打顺序**；只打「全都在、都没被切开」这条判据。
  ★O_APPEND 让每个 write() 系统调用原子地接在当时末尾，所以单条 write 不会被别人的字节切开；
    但「一次 Python write 等于一次系统调用」只在跨过缓冲区时才成立 —— PIPE_BUF 那类具体阈值
    本机没有可控的并发调度手段去测，**标未验证**（第 11 章讲并发时再量）。
```

两个子进程各写 40 行，最后 80 行、每条都以 `甲-` 或 `乙-` 开头且完整，半截行 0 条（两个集合完全相同）。
脚本**不打顺序** —— 谁先谁后本机不可预测，打出来就是把噪声当结论。能证的机制是 `O_APPEND`：
它让每个 `write()` 系统调用原子地接在当时的末尾，所以单条 write 不会被别人的字节切开。
但成立条件写得很清楚：「一次 Python write 等于一次系统调用」只在**跨过缓冲区**时才成立
（回想第 17 节那次测量：小于一整个缓冲区的写，压根还没下到内核）。`PIPE_BUF` 那类具体阈值
本机没有可控的并发调度手段去测，**标未验证**。

**带走什么**：① 大文件别一次 `read()`，分块或逐行才不炸内存，一次读完 = 整个文件常驻；
② 读法可以换，**字节账必须对得上**：块数、总字节、指纹、逐字节相同四项一致才算读全；
③ 文本模式的 `seek` 只认 `tell()` 发回来的 cookie，自己塞字节数的三种结局是「负数当场 ValueError」、
「切到字符中间的等 read 才 UnicodeDecodeError」、越过末尾那种则是「不报错也不提醒」—— 没报错不等于对了；
④ cur/end 相对定位只要偏移非零就直接 `io.UnsupportedOperation`，要按相对量或末尾定位就开二进制模式；
⑤ 行号不是地址，随机访问靠偏移索引；⑥ `'a'` + 一个句柄做顺序写，并发追加只保证「单条 write 不会被别人的字节切开」，不保证顺序。

---

## 第 19 节 · capstone：把本章零件装成一个真工具（安全读配置、规范化产出）

前 18 节每节量一个零件，这一节把它们装成一个叫 `cfgnorm` 的命令行工具：吃一堆配置 json，
吐 `<名>.normalized.json` 和 `<名>.audit.csv`，全程只走同目录 tmp + `os.replace`。
它自己跑六段自证，最后打一张对照表，把工具里**每一处写法**回指本章第几节 —— 对读者来说，
读那张表比读工具源码划算；对写工具的人来说，那张表是「我知道我为什么这么写」的凭证。

```
$ python3 r_cli_capstone.py
=== 1. 工具亮相：用法、契约、现场布局（幂等建目录 + Path 纯算拆名字）===
  cfgnorm —— 安全读配置、规范化产出（本章 1~18 节的总成）
    用法：python3 r_cli_capstone.py --in <配置.json> --out <目录> [--check-only]
    产出：<目录>/<名>.normalized.json（UTF-8、键 sorted、indent=2、LF）
          <目录>/<名>.audit.csv      （体检单：每个契约键一行，写清落的是什么、为什么）
    约定：产出只走同目录 tmp + os.replace；开工前先扫上一版崩留下的 .tmp 并报警
    失败即不产出：读不到 / JSON 坏了 / 参数认不出，一律一个字节都不往目标上写

  in/ 连建两次都没抛 → True ｜ out/ 还没人建，exists=False（main() 会自己补上）
  Path 纯算（第 8、9 节：对**根本不存在**的名字照样算得出，一个异常不抛）：
    str='capstone-demo/in/app.json' ｜ parts=('capstone-demo', 'in', 'app.json') ｜ name='app.json' ｜ suffix='.json' ｜ stem='app'
    同一行输入交给 os.path：split=('capstone-demo/in', 'app.json') ｜ 两派算出来的 stem 相同吗 → True
    父级不存在时 mkdir()                     → FileNotFoundError: [Errno 2] ENOENT（No such file or directory）
    名字被普通文件占着时 mkdir()                 → FileExistsError: [Errno 17] EEXIST（File exists）
    中间是普通文件时 mkdir(parents=True)       → NotADirectoryError: [Errno 20] ENOTDIR（Not a directory）
  → 三个 errno 三条处置路径（第 10 节）：ENOENT 少一层、EEXIST 名字被占、ENOTDIR 中间被文件挡住；
    写成 except OSError: pass 等于把三种事故压成一次静默失败。
  现场布置：in/ 放了 6 份配置（现造现删）
    [('app.json', 88), ('bom.json', 96), ('broken.json', 27), ('dirty.json', 100), ('gbk编码.json', 33), ('wrongtypes.json', 75)]
    ← 六个字节形状各不同的输入：干净 / 类型错 / BOM+CRLF / 坏字节 / 语法坏 / gbk
```

契约写在用法里，三条值得单独划出来：产出**只**走同目录 tmp + `os.replace`；
开工前先扫上一版崩留下的 `.tmp` 并报警；失败即不产出 —— 读不到 / JSON 坏了 / 参数认不出，
一律一个字节都不往目标上写。这三条是第 17 节那四行长成了一个工具。
上半段是第 8、9、10 节的现场版：`Path` 那一路纯算（`parts / name / suffix / stem`）
对 `capstone-demo/in/app.json` 这个**根本不存在**的名字照样算得出、一个异常不抛，
和 `os.path` 那一派算出来的 `stem` 相同；而 `mkdir` 是问内核，于是三种现场失败各带一个 errno：
父级不存在 → `FileNotFoundError: [Errno 2] ENOENT`（少一层）、名字被普通文件占着 →
`FileExistsError: [Errno 17] EEXIST`（名字被占）、中间是普通文件还硬要 `parents=True` →
`NotADirectoryError: [Errno 20] ENOTDIR`（中间被挡）。三条 errno 是三条不同的处置路径，
写成 `except OSError: pass` 等于把三种事故压成一次静默失败。
六份输入故意是六个不同的字节形状：`app.json` 88 / `bom.json` 96 / `broken.json` 27 / `dirty.json` 100 /
`gbk编码.json` 33 / `wrongtypes.json` 75 —— 干净 / BOM+CRLF / 语法坏 / 坏字节 / 别的编码 / 类型错。

读侧第一步是把「猜」全部关掉：

```
=== 2. 读侧：encoding 与 errors 都写死，BOM 和 \r\n 先用 read_bytes 钉住 ===
  基线（第 6 节）：locale.getpreferredencoding(False)='UTF-8' ｜ sys.stdout.encoding='utf-8' —— 工具一个都不靠它们
  app.json       字节=88    字符=80    BOM=False  磁盘 \r\n=0    读回 '\r'=0    U+FFFD=0    句柄 encoding='utf-8-sig' errors='replace'
  bom.json       字节=96    字符=81    BOM=True   磁盘 \r\n=4    读回 '\r'=0    U+FFFD=0    句柄 encoding='utf-8-sig' errors='replace'
  broken.json    字节=27    字符=25    BOM=False  磁盘 \r\n=0    读回 '\r'=0    U+FFFD=0    句柄 encoding='utf-8-sig' errors='replace'
  dirty.json     字节=100   字符=86    BOM=False  磁盘 \r\n=0    读回 '\r'=0    U+FFFD=2    句柄 encoding='utf-8-sig' errors='replace'
  gbk编码.json     字节=33    字符=31    BOM=False  磁盘 \r\n=0    读回 '\r'=0    U+FFFD=7    句柄 encoding='utf-8-sig' errors='replace'
  wrongtypes.json 字节=75    字符=61    BOM=False  磁盘 \r\n=0    读回 '\r'=0    U+FFFD=0    句柄 encoding='utf-8-sig' errors='replace'
  → 判据一：同一份文件字节数 ≠ 字符数（一个中文 3 字节）；dirty 那一行的 U+FFFD 就是**被改掉的
    位置**，工具把它当质量信号报出来，而不是当成「读取成功」。
  → 判据二：bom 那一行磁盘上有 \r\n、读回来 \r 剩 0 个 —— 是文本层翻译的（第 5 节），不是文件坏了。
  open(p) 不写 encoding → 句柄 encoding='UTF-8'（来自 locale，不来自文件内容）
    → UnicodeDecodeError: 'utf-8' codec can't decode byte 0xc0 in position 10: invalid start byte
  同一份字节显式 encoding='gbk' → '{"name": "老机器存的", "port": 2}' ← 读通了，中文一个都没坏
```

基线那行是本章反复引用的那两个默认值：`locale.getpreferredencoding(False)='UTF-8'`、
`sys.stdout.encoding='utf-8'` —— 工具一个都不靠它们，句柄一律写死 `encoding='utf-8-sig' errors='replace'`。
六行表里三组数字各有各的讲法：
**字节 ≠ 字符**（`app.json` 88 字节 / 80 字符，一个中文 3 字节）；
**磁盘形状 ≠ 读回来的形状**（`bom.json` 磁盘上 `\r\n` 有 4 个、读回来 `'\r'` 剩 0 个，
这是文本层翻译的，第 5 节，不是文件坏了；BOM 那份 `BOM=True` 之所以不脏数据，是 `utf-8-sig` 吃掉了它）；
**「读成功」≠「读对了」**（`dirty.json` 的 `U+FFFD=2`、`gbk编码.json` 的 `U+FFFD=7` ——
`errors='replace'` 不是宽容，是**有人替你改了内容**，那两个和那七个替换符就是被改掉的位置。
工具把它当质量信号报出来，而不是当「读取成功」）。
最后两行是整个故事的根：`open(p)` 不写 `encoding`，句柄那个 `'UTF-8'` 来自 locale、不来自文件内容，
读那份 gbk 的旧机器文件当场炸在 `UnicodeDecodeError: 'utf-8' codec can't decode byte 0xc0 in position 10: invalid start byte`；
同一份字节显式 `encoding='gbk'` 就一句 `{"name": "老机器存的", "port": 2}` 全读通，中文一个都没坏。

校验这一层的价值全在「报错够不够具体」：

```
=== 3. 校验：契约就是 json 的那张类型表，报错必须带 lineno/colno/pos ===
  broken.json    判失败 → True ｜ 工具的原始判词：
    JSONDecodeError: Illegal trailing comma before end of object: line 1 column 24 (char 23) ｜ msg='Illegal trailing comma before end of object' lineno=1 colno=24 pos=23 ｜ 错在这一行：'{"name": "甲", "port": 1,}'
  没这份.json       判失败 → True ｜ 工具的原始判词：
    ENOENT：读不到 没这份.json → 不产出（FileNotFoundError）
  → 判据：JSONDecodeError 是 ValueError 的儿子，msg/lineno/colno/pos 直接指出「哪一格」；只打一句
    「配置读失败」的工具，出了事还得人再开一遍文件。语法坏 = 不产出，半个字都不写。
  契约校验（wrongtypes.json，--check-only 不落盘）判通过 → True：
    键      来的是            契约要     落的是       处置
    debug  1              bool    false     ★类型不符（来的是 int）→ 退回默认值
    name   <缺失>           str     "app"     补默认值
    port   "8080"         int     8080      数字字符串 → int
    ratio  <缺失>           float   1.0       补默认值
    tags   {"a": 1}       list    []        ★类型不符（来的是 dict）→ 退回默认值
    备注     "契约外的键"        <契约里没有> <丢弃>      ★契约外的键：丢掉，不写进产出
  → 三条最容易放过的：tuple 会**降级成 list 且不报错**、字典的 int 键回来变 str、
    bool 是 int 的儿子所以 isinstance(True, int) 为真 —— 契约表必须先单独挡 bool 再谈 int。
    loads 永远给不出 tuple，但内存对象会走到这一步：dumps((1, 2)) → [1, 2]
    loads 永远给不出 set，但内存对象会走到这一步：dumps({1, 2}) → TypeError: Object of type set is not JSON serializable
  Python 的字面量 json 不认：loads('null') → None ｜ 配置里手打 None → JSONDecodeError: Expecting value: line 1 column 1 (char 0)
```

`broken.json` 的判词把第 16 节那三字段用满了：`msg='Illegal trailing comma before end of object'`、
`lineno=1 colno=24 pos=23`，后面直接贴上出错那一行的原文 `{"name": "甲", "port": 1,}`；
缺失那份则走 `ENOENT` → 不产出。判据一句：只打「配置读失败」的工具，出了事还得人再开一遍文件。
下半段那张契约表是本节最好读的东西，逐行看处置就明白「契约」两个字怎么写：
`port` 来的是 `"8080"`、契约要 `int`、落的是 `8080`，这是**允许且写进契约的**转换（数字字符串 → int）；
`name`、`ratio` 缺失 → 补默认值；`备注` 是契约外的键 → 丢掉，不写进产出。
两行 ★ 才是真功夫：`debug` 来的是 `1`，契约要 `bool`，落的是 `false`，处置写得很直白 ——
「类型不符（来的是 int）→ 退回默认值」；`tags` 来的是 `{"a": 1}`，契约要 `list`，退回默认值 `[]`。
为什么 `1` 不能当 `True`？因为 `bool` 是 `int` 的儿子，`isinstance(True, int)` 为真。
这条家谱关系直接决定契约表的写法：**先单独挡 `bool`，再谈 `int`**。
如果表里只写「要 `int` 就 `isinstance(x, int)`」，那 `True`/`False` 会被当合法整数收进端口号；
反过来「要 `bool` 就 `isinstance(x, bool)`」看着对，一旦有人图省事写成 `in (0, 1)` 又漏回去。
底下两行是第 16 节那两条在真工具里的样子：`dumps((1, 2)) → [1, 2]`（tuple 降级、不报错）、
`dumps({1, 2}) → TypeError: Object of type set is not JSON serializable`；
最后一行是 json 和 Python 字面量的那条界线：`loads('null') → None`，配置里手打 `None` → `JSONDecodeError`，一字之差。

★ 这一段是全章最值钱的：产出走 tmp + `replace`，而「有没有留 .tmp」那笔账**当场被弄响一次**。

```
=== 4. ★产出：同目录 tmp + fsync + os.replace，而「有没有留 .tmp」这笔账当场响一次 ===
  旧版长度=30 新版长度=74 ｜ 三个探测点的（读者看到的长度, 此刻 .tmp 在不在）= [(30, False), (30, True), (74, False)]
  → 长度只落在 (30, 74) 里吗 → True ｜ 出现第三种长度（= 有人在目标上就地写）的次数 = 0
  umask 0o077 下（第 1、13 节）：tmp 建出来 0o600 → 不 chmod 就 replace → 产出 0o600 ｜ replace 前补一句 chmod(0o644) → 产出 0o644
  → rename(2) 只改目录项、不改 inode：临时文件什么权限，转正之后就是什么权限。漏掉那次 chmod，
    别人（组/其他）就读不到你刚「发布」的配置 —— 而工具自己一路判成功，什么都不报。
  对账第一次（现场真有残留）：工具用的 iterdir 版抓到 2 个 = ['.audit.tmp', 'app.normalized.json.tmp']
    残留原文的尾部 = '半截配置", "po' ← 这就是「崩在半路」的形状
  同一个现场换两种数法：glob 模块 → 1 个 = ['app.normalized.json.tmp'] ｜ Path.glob → 2 个 = ['.audit.tmp', 'app.normalized.json.tmp']
    ★glob 模块漏掉的那一个 = ['.audit.tmp']（第 11 节：glob 的 `*` 不匹配点开头的名字，Path.glob 却匹配 —— 同一个 `*` 两套规矩，本机实测）
    → 对账要是写成 glob 模块那一版，它对 .audit.tmp 这种残留就永远少报一个。漏报比不查更糟：
      屏幕上那个数字看着像有人盯着，其实被盯住的那一半根本不存在。工具用 iterdir 版，两个都抓到。
  工具自己扫过一遍（开工先清残留）→ 这次抓到 2 个 ｜ 现在 iterdir 版 = [] ｜ glob 版 = [] ｜ 两个都归 0 → True
  产出：app.normalized.json.tmp 转正为 app.normalized.json（113 字节，权限 0o644）｜ 判成功 → True
  → 这一段的判据：目标文件从头到尾只被 rename(2) 换过一次名；fsync 买到的是「请求内核别丢」，
    不是「已经在介质上」（本机没有可控断电手段，掉电行为**未验证**）。
```

前三行是第 17 节第 3 段的现场版：旧版 30、新版 74，三个探测点看到
`（读者看到的长度, 此刻 .tmp 在不在）` = `(30, False), (30, True), (74, False)` —— 写 tmp 的途中读者仍然是完整旧版，
`replace` 之后读者直接是完整新版，第三种长度出现次数 0。
第 4 行那两栏是权限那一课的第二遍，但这次是 `umask 0o077`：tmp 建出来 `0o600`，
不 `chmod` 就 `replace` → 产出 `0o600`；补一句 `chmod(0o644)` → 产出 `0o644`。
后面那句判词请照抄到你的 code review 清单上：漏掉那次 `chmod`，别人（组/其他）读不到你刚「发布」的配置，
**而工具自己一路判成功，什么都不报**。
真正的主角是「对账第一次」那三行：现场**真有**残留，工具用的 `iterdir` 版抓到 **2 个**
= `['.audit.tmp', 'app.normalized.json.tmp']`，那个残留原文的尾部 `半截配置", "po`
就是「崩在半路」的形状。先让守卫抓到东西，它后面的 0 才有意义。
紧接着同一份现场被三种数法数了一遍：`glob` 模块 → **1 个**、`Path.glob` → 2 个、`iterdir` → 2 个。
差的那一个就是 `['.audit.tmp']` —— glob 模块的 `*` **不匹配点开头的名字**，`Path.glob` 却匹配，
同一个 `*` 两套规矩（第 11 节那条，这里本机实测）。这条差别的后果很具体：
如果「有没有留 .tmp」的对账写成 glob 模块那一版，它对 `.audit.tmp` 这一类残留**永远少报一个**，
屏幕上那个数字看着像有人盯着，其实被盯住的那一半根本不存在 —— 这就是恒真的假守卫，
比没守卫更坏，因为它买了你的信任。工具用 `iterdir` 版，两个都抓到，扫完 `iterdir` 版和 glob 版一起归 0。
然后才是那句平淡的成功：`app.normalized.json.tmp` 转正为 `app.normalized.json`，113 字节、权限 `0o644`。
最后一行仍然把话说老：`fsync` 买到的是「请求内核别丢」，不是「已经在介质上」，掉电行为**未验证**。

第 5 段是边界（配置整份读、日志逐行读、`seek` 只认 cookie），第 6 段是那张贴身的对照表：

```
=== 6. 收口：这张对照表 —— 工具的每一处写法回指本章第几节 ===
  open(p, encoding="utf-8-sig", errors="replace")    → 第 6、7     节：不写 encoding 用的是 locale 的首选编码；BOM 会变成首字符 \ufeff；errors 里只有 strict 会响，其余四个都在改内容
  path.read_bytes() 先把字节钉住                           → 第 12、18   节：字节 ≠ 字符（一个中文 3 字节）；read_text/write_text 只配「整份 + 小文件」，而 write_text 是截断写
  读回的 "\r" 个数、写 tmp 时那句 newline="\n"                 → 第 5       节：读侧默认把 \r\n 翻成 \n，写侧把 \n 换成 os.linesep：磁盘形状和字符串形状是两回事
  p.parts / name / suffix / stem / parent            → 第 8、9     节：纯字符串运算，对不存在的名字照样算得出、一个异常不抛；os.path 那一派是同一条界线，老代码里全是它
  mkdir(parents=True, exist_ok=True) / unlink(missing_ok=True) → 第 10      节：幂等三件套；ENOENT / EEXIST / ENOTDIR 是三条不同的处置路径，except OSError: pass 把它们压成一次静默
  csv.writer(buf) + open(..., newline="")            → 第 5、15    节：csv 自己已经写了 \r\n，再让文本层翻译一次就成了 \r\r\n；它不管编码也不管类型，读出来全是 str
  json.loads + JSONDecodeError 三字段；dumps(sort_keys=True, ensure_ascii=False) → 第 16      节：json 是**值**的格式不是类型的格式（tuple 降级、set 当场 TypeError）；msg/lineno/colno/pos 一行定位到「哪一格」；默认 ensure_ascii 让中文体积翻倍
  open(out, "w") 只用在 tmp 上，绝不用在目标上                   → 第 2、12    节：'w' 在打开那一刻就 O_TRUNC：事故不是「新的没写成」，是**旧的也没了** —— 没备份、不报错
  同目录 .tmp → f.flush() → os.fsync(fd) → os.replace   → 第 3、17    节：flush 才是缓冲区边界；「写完」「落盘」「原子」是三件事；fsync 只到「交给内核」为止（掉电本机未验证）
  os.chmod(tmp, 0o644) 与新建文件的权限                      → 第 1、13    节：新建文件位 = 0o666 & ~umask；mkstemp 故意给 0600；权限跟 inode 走不跟名字走，所以 chmod 必须赶在 replace 之前
  sweep_stale_tmp() 用 iterdir，而不是 glob 模块的 `*`       → 第 11      节：glob 模块的 `*` 不匹配点开头的名字（Path.glob 却匹配）→ 那一版永远少报；计数守卫要先被故意弄响一次再报 0
  with open(...) / for line in fh / p.resolve()      → 第 4、14    节：with 只保证 close 不保证事务，文件对象还是一次性迭代器；resolve 问内核并展开符号链接，absolute 只拼 cwd、不折叠 ..
  对照表 12 行 ｜ 1..18 全对上号了吗 → True ｜ 没对上的节 = 无
  → 三条带走：① 「读成功」不等于「读对了」、「写完了」不等于「别人看得懂」——中间每一步都有自己的
    默认值，所以全部写死；② 产出只有一个安全形状：同目录 tmp 写全 → fsync → chmod → replace，
    任何一步崩了就 unlink(missing_ok=True)，绝不让半截东西顶在目标位置上；③ 这个工具最值钱的一行
    不是写文件那段代码，是**先抓到一个残留 .tmp、再报那个 0**：没有那一次响，后面的 0 一文不值。
  本机 Python 3.14.4 ｜ 全程只写 capstone-demo/ 下的相对路径 ｜ umask 钉在 0o022 ｜ 无绝对路径、无时间戳、无随机名，跑两遍逐字一致。
```

这张表 12 行，读法不是「看完就过」，而是**拿它当 code review 检查单**用。三行示范怎么读：
`open(p, encoding="utf-8-sig", errors="replace")` 回指第 6、7 节 —— 不写 `encoding` 用的就是 locale 首选编码，
BOM 会变成首字符 `\ufeff`，`errors` 五个取值里只有 `strict` 会响、其余四个都在改内容；
`open(out, "w") 只用在 tmp 上，绝不用在目标上` 回指第 2、12 节 —— `'w'` 在打开那一刻就 `O_TRUNC`，
事故不是「新的没写成」，是**旧的也没了**；
`sweep_stale_tmp() 用 iterdir，而不是 glob 模块的 *` 回指第 11 节 —— 就是上面那一幕，
点开头的那个名字会被漏数，而「计数守卫要先被故意弄响一次再报 0」。
中间那行 `os.chmod(tmp, 0o644)` 回指第 1、13 节，讲的还是「权限跟 inode 走，不跟名字走，
所以 `chmod` 必须赶在 `replace` 之前」；`csv.writer(buf) + open(..., newline="")` 回指第 5、15 节，
说的是 csv 自己已经写了 `\r\n`，再让文本层翻译一次就成了 `\r\r\n`。
最底下那行统计是这条链的验收：`对照表 12 行 ｜ 1..18 全对上号了吗 → True ｜ 没对上的节 = 无` ——
本章第 1 到第 18 节没有一个零件被装进工具之后又忘掉来历。
脚本自己的三条带走也印在表下面，第二条是全部产出的形状（同目录 tmp 写全 → `fsync` → `chmod` → `replace`，
任何一步崩了就 `unlink(missing_ok=True)`，绝不让半截东西顶在目标位置上），
第三条就是本节那一句：**这个工具最值钱的一行不是写文件那段代码，是先抓到一个残留 `.tmp`、再报那个 0；
没有那一次响，后面的 0 一文不值。**
末尾那行自证也顺手做了第 7 节该做的事：本机 Python 3.14.4、全程只写 `capstone-demo/` 下的相对路径、
`umask` 钉在 `0o022`、无绝对路径无时间戳无随机名，**跑两遍逐字一致** —— 一个工具如果每次输出的字节都不同，
它就没法被校验；「跑两遍逐字一致」是本章所有脚本的公共契约。

**带走什么**：① 「读成功」不等于「读对了」，「写完了」不等于「别人看得懂」—— 中间每一步都有自己的默认值，
所以 `encoding` / `errors` / `newline` / `indent` / `sort_keys` / 权限全部写死；
② 产出只有一个安全形状：同目录 tmp 写全 → `flush` → `fsync` → `chmod` → `replace`，崩了就删 tmp；
③ 契约表里 `bool` 要单独挡在前面，因为 `isinstance(True, int)` 为真；
④ 任何报 0 的计数守卫，先在眼前让它响一次，那个 0 才有资格被相信。

---

## 第 20 节 · 对账：这 19 个脚本是不是真按规矩写的，以及一条守卫怎么证明它会响

前面 19 个脚本每个都自称：编好了号、给了收口、建过目录就清理。
这种话最容易变成习惯性的空头承诺：脚本会删段、会改编号、会加一个不干净的例子。
`s_checklist.py` 就干这件事 —— 去源码里核。但它没有停在扫一遍报个 0，
而是先造五个坏样本**证明自己会响**，因为一条不会响的检查等于没检查。

规矩先写在屏幕上，再拿它去扫：

```
$ python3 s_checklist.py
=== 1. 先把规矩写在屏幕上，再拿它去扫（五条，每一条都可能失败）===
  ① 编号连续 ② 末段收口 ③ 至少一段 ④ 建过就得收 ⑤ 不联网
  ① 分段编号必须 1..N 连续 —— 跳号说明有一段被删了、末段编号没跟着改；
  ② 最后一段的标题里必须有「收口」—— 没有就说明它只演了现象，没把判据交给你；
  ③ 至少得有一条分段 —— 一句分段都没有的文件不配进这一章的清单（demo.sh 就撞在这条上）；
  ④ 建过目录/临时文件就必须有清理动作 —— 本章每个脚本都会在沙箱里真建文件，不清理就是污染；
  ⑤ 不许 import 网络模块 —— 本章全部结论都应该在断网时成立。
  注意这里**没有**的几条，免得你以为它们被检查了：
  · 没查脚本能不能跑通（跑通与否看第 2..19 节的退出码）；
  · 没查每段标题的内容对不对、有没有灌水（只看标题行，正文写三行还是三十行它不管）；
  · 没查屏幕上有没有绝对路径（④ 只查「源码里写了清理」，异常路径有没有真执行到，由第 21 节 find 兜）。
```

五条各有各的失败方式，逐条看才不觉得是走过场：① 编号必须 `1..N` 连续（跳号说明有一段被删了、
末段编号没跟着改）；② 最后一段标题里必须有「收口」（没有就说明它只演了现象、没把判据交给你）；
③ 至少要有一条分段（一句分段都没有的文件不配进清单 —— `demo.sh` 就撞在这条上）；
④ 建过目录/临时文件就必须有清理动作（本章每个脚本都会在沙箱里真建文件，不清理就是污染）；
⑤ 不许 import 网络模块（本章全部结论都该在断网时成立）。
下半段那三条「注意这里**没有**的」比五条规矩更值得学：不查脚本能不能跑通（跑通与否看第 2..19 节的退出码）、
不查每段标题内容对不对或灌不灌水（只看标题行，正文三行还是三十行它不管）、
不查屏幕上有没有绝对路径（④ 只查「源码里写了清理」，异常路径有没有真执行到，由第 21 节 `find` 兜）。
一条检查最坏的失败不是漏报，而是它的**说明夸大了自己的覆盖面** —— 于是这节把不查的东西也印出来。

然后开扫，19 个真脚本一个都不放过（包括它自己）：

```
=== 2. 开扫：本章每个脚本的段数、编号、末段标题 ===
  a_open_modes.py          段数=6   编号 1..6 连续 ｜ 末段「收口：一张「模式 → 不存在时 / 存在时 / 起点 / 截断 / 读 / 写」判据表」
  b_buffering.py           段数=6   编号 1..6 连续 ｜ 末段「收口：tty 行缓冲 / 管道块缓冲 / flush 才是边界 / close 才是终点」
  c_close_and_iter.py      段数=6   编号 1..6 连续 ｜ 末段「收口：close 归谁、迭代器一次性、内存只看方向」
  d_newline.py             段数=6   编号 1..6 连续 ｜ 末段「收口：读侧翻译谁决定、写侧翻译谁决定，一张表 + 三句话」
  e_encoding_default.py    段数=7   编号 1..7 连续 ｜ 末段「收口：默认编码来自 locale，不来自你的文本」
  f_errors_param.py        段数=8   编号 1..8 连续 ｜ 末段「收口：哪个会响 / 哪个会改内容 / 哪个能原样回去」
  g_pathlib_basics.py      段数=9   编号 1..9 连续 ｜ 末段「收口：一张「纯算 / 问内核」判据表 + 三条能带走的话」
  h_pathlib_vs_ospath.py   段数=8   编号 1..8 连续 ｜ 末段「收口：什么时候值得用 Path，以及 os.fspath(p) 与 str(p) 的区别」
  i_mkdir_unlink.py        段数=7   编号 1..7 连续 ｜ 末段「收口：幂等三件套，和「按 errno 分派」这条第 07 章的规矩」
  j_glob_stat.py           段数=9   编号 1..9 连续 ｜ 末段「收口：这一节四条判据」
  k_read_write_text.py     段数=8   编号 1..8 连续 ｜ 末段「收口：什么时候能用这两个方法，什么时候必须自己 open」
  l_tempfile_perms.py      段数=11  编号 1..11 连续 ｜ 末段「收口：临时文件的权限为什么是安全属性」
  m_symlink_resolve.py     段数=7   编号 1..7 连续 ｜ 末段「收口：这四条以后当判据用」
  n_csv.py                 段数=7   编号 1..7 连续 ｜ 末段「收口：csv 管引号和分隔符，编码、类型、行尾都不归它」
  o_json.py                段数=9   编号 1..9 连续 ｜ 末段「收口：一张类型表 + 三个字段 + 三个雷」
  p_atomic_write.py        段数=7   编号 1..7 连续 ｜ 末段「收口：配置文件的安全写法只有这四行」
  q_bigfile_seek.py        段数=8   编号 1..8 连续 ｜ 末段「收口」
  r_cli_capstone.py        段数=6   编号 1..6 连续 ｜ 末段「收口：这张对照表 —— 工具的每一处写法回指本章第几节」
  s_checklist.py           段数=4   编号 1..4 连续 ｜ 末段「收口：一条检查的价值 = 它能失败的那几种方式」
  —— 合计：脚本 19 个 ｜ 小节 139 段 ｜ 不合格 0 个
  → README 里「19 个脚本 / N 段」这两个数字的出处就是上面这一行，不是人手写的。
  本脚本自己（s_checklist.py）也在这份清单里 → 它同样得过这五条，不然它就是一盏只照别人的灯。
```

这张表就是本章 README 里那两个数字的出处：`合计：脚本 19 个 ｜ 小节 139 段 ｜ 不合格 0 个` ——
数字是脚本数出来的，不是人写的（段数从 `a_open_modes.py` 的 6 到 `l_tempfile_perms.py` 的 11，
19 行加起来正好 139）。末段标题那一列顺手把每节的收口句抄了一遍，想跳读本章的人可以拿它当目录。
最后一行是这类工具的可信度关键：`s_checklist.py` 自己也在清单里，段数 4、末段标题正是本节那句
「收口：一条检查的价值 = 它能失败的那几种方式」—— 它也过了这五条，不然它就是一盏只照别人的灯。

「不合格 0 个」这句话值多少钱，取决于另外五个 ★ 响过没有：

```
=== 3. 现场造五个坏样本 + 一个好样本：证明这五条能响，也不误报 ===
  bad1_numbering.py      段数=3   ★不合格（1 条）：① 编号不连续：实际是 [1, 2, 4]
  bad2_no_closure.py     段数=3   ★不合格（1 条）：② 末段标题是「最后一演」，没写收口 = 演了一半没给判据
  bad3_no_section.py     段数=0   ★不合格（1 条）：③ 一条分段标题都没有，它不是取证脚本
  bad4_leaks_dir.py      段数=3   ★不合格（1 条）：④ 建了目录（mkdir/makedirs）却全程没有 rmtree/rmdir
  bad5_network.py        段数=3   ★不合格（1 条）：⑤ 出现联网/跨进程模块：import socket
  good_sample.py         段数=3   编号 1..3 连续 ｜ 末段「收口：三条判据」
  → 五条规矩各自被一个样本触发过一次（响过的编号：①②③④⑤），
    第六个是真合格的样本、没被冤枉 —— 它建了目录也 rmtree 了，所以 ④ 不误报。
    所以第 2 段那句「不合格 0 个」是有判断力的结论，不是一句恒真的废话。
  收尾自证：check-demo/ 还在吗 → False ｜ 当前目录里 .py 个数 = 19（坏样本没留下尸体）
```

五个坏样本各撞一条：`bad1_numbering.py` 的编号实际是 `[1, 2, 4]`（①）；
`bad2_no_closure.py` 末段标题是「最后一演」、没写收口（②）；
`bad3_no_section.py` 段数=0、一条分段标题都没有（③）；
`bad4_leaks_dir.py` 有 `mkdir`/`makedirs` 却全程没有 `rmtree`/`rmdir`（④）；
`bad5_network.py` 出现 `import socket`（⑤）。
第六个 `good_sample.py` 是真合格的：3 段、编号 `1..3` 连续、末段写了「收口：三条判据」，
而且它**建了目录也 `rmtree` 了**，所以 ④ 不误报 —— 只有会响、也不冤枉好人，
上面第 2 段那句「不合格 0 个」才是有判断力的结论，不是一句恒真的废话。
最后一行照例自证：`check-demo/` 已经不在，当前目录 `.py` 个数 = 19，**坏样本没留下尸体**。

它什么时候退非 0？往参数里塞一个**不是取证脚本**的文件，闸门当场就响：

```
$ out=$(python3 s_checklist.py demo.sh); echo "退出码 = $?（非 0 = 能当 CI / make check 用）"; grep -F demo.sh <<<"$out"
退出码 = 1（非 0 = 能当 CI / make check 用）
  ③ 至少得有一条分段 —— 一句分段都没有的文件不配进这一章的清单（demo.sh 就撞在这条上）；
  demo.sh                  段数=0   ★不合格（1 条）：③ 一条分段标题都没有，它不是取证脚本
```

`python3 s_checklist.py demo.sh` 的退出码是 **1**，`grep` 出来的正是那两行：③ 那条规矩的原文，
和 `demo.sh 段数=0 ★不合格（1 条）`。合格退 0、不合格退 1，这个脚本才可能进 `make check` / CI；
只会往屏幕上打「看起来没问题」的工具，人不在的时候价值是零。

它自己的收口讲的是「一条检查的价值 = 它能失败的那几种方式」，这五条不只对本章有用：

```
=== 4. 收口：一条检查的价值 = 它能失败的那几种方式 ===
  ① 写「每个脚本都分段、都自清理」这种承诺之前，先问：有没有一条命令能把它证伪？
     没有 → 这条承诺三个月后一定和现实不符，而且没人知道。
  ② 守卫必须**当场演示过一次触发**（第 3 段五个样本）：否则你不知道它是不是恒真。
     本章第 12 节那条「没人替你 close」、第 17 节那条「有没有留 .tmp」都是同一个道理。
  ③ 结论要能被上游接住：合格退 0、不合格退 1，这个脚本才可能进 make check / CI；
     只会往屏幕上打「看起来没问题」的工具，人不在的时候价值是零。
  ④ 判据要**只写你真的查了的东西**（第 1 段那三条「注意这里没有的」）：
     检查工具最坏的失败不是漏报，是它的说明夸大了自己的覆盖面。
  ⑤ 两个工具互补比一个工具万能可信：本脚本查「源码里写了清理」，第 21 节查「磁盘上真没留下」；
     第 07 章那套逐字校验查「README 引的每一行真跑出来过」。三条都响不了的问题，就不该印在书里。
```

**带走什么**：① 写「每个脚本都分段、都自清理」这种承诺之前，先问一句：有没有一条命令能把它证伪？
没有 → 这条承诺三个月后一定和现实不符，而且没人知道。
② 守卫必须**当场演示过一次触发**，否则你不知道它是不是恒真 —— 本章第 12 节那条「没人替你 close」、
第 17 节那条「有没有留 .tmp」、第 19 节那个抓到 2 个残留的计数，是同一个道理的三种写法。
③ 结论要能被上游接住：退 0 / 退 1。
④ 判据只写你真的查了的东西，把「没查的」也印出来。
⑤ 两个工具互补，比一个工具自称万能可信：本脚本查「源码里写了清理」，第 21 节查「磁盘上真没留下」。

---

## 第 21 节 · 自证：沙箱里的事只留在沙箱，章目录一个字都没多

本章每个脚本都往磁盘上写过东西：一个 2MB 的文件、几份 `.tmp`、五个坏样本、一个 gbk 的配置文件。
「它们都被清干净了」这句话如果只能靠读源码来相信，那它就还是句承诺。
所以最后这一节交给 shell 闸门，而且按本章自己的规矩来：**先把式子写成能判的，再故意弄响一次，最后才报 0**。

demo 的写操作全部发生在一个临时造出来的沙箱目录里，此刻的当前目录就是那个 `work/`：

```
$ echo "沙箱 work/ 里的项数（不含 README.md）=$(ls | grep -vc "^README.md$")，前 12 个："; ls | grep -v "^README.md$" | head -12
沙箱 work/ 里的项数（不含 README.md）=20，前 12 个：
a_open_modes.py
b_buffering.py
c_close_and_iter.py
d_newline.py
demo.sh
e_encoding_default.py
f_errors_param.py
g_pathlib_basics.py
h_pathlib_vs_ospath.py
i_mkdir_unlink.py
j_glob_stat.py
k_read_write_text.py
```

`ls` 去掉 `README.md` 之后数到 **20** 项：除了源码就是那份 demo 脚本本身，正好是第 20 节那张表的清单，
一个沙箱子目录都没有 —— 每个脚本自己收了。`grep -v "^README.md$"` 这一手是本节的语法点：
清单里显式排除 `README.md`，这样 README 写完之后再重跑这一章，得到的才是同一份输出；
不然这一章的自证会被写 README 这件事本身弄响，变成永远红着的假警报。
`head -12` 只截前 12 个文件名到屏幕上，但计数用的是 `grep -vc`，所以 20 这个数是全量算出来的。

三个计数各管一种泄漏形状，脚本先**故意造出**它们：

```
$ mkdir -p 残留-demo; touch 残留.tmp; echo "塞进一个假产物之后：残留子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  非源码产物数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l)  残留 .tmp 个数=$(find . -name "*.tmp" | wc -l) ← 三个都会响"; rm -rf 残留-demo; rm -f 残留.tmp
塞进一个假产物之后：残留子目录数=1  非源码产物数=1  残留 .tmp 个数=1 ← 三个都会响
```

`mkdir -p 残留-demo; touch 残留.tmp` 之后，三个 `find` 一次数出来：残留子目录数=**1**、
非源码产物数=**1**、残留 `.tmp` 个数=**1** —— 三个都会响。三个式子各查一类东西，写法有讲究：
子目录那条是 `find . -mindepth 1 -maxdepth 1 -type d`（`-mindepth 1` 排掉 `.` 自己，
`-maxdepth 1` 只看一层，不然深处的合法子目录会被算进来）；
产物那条靠一串 `! -name` 把 `.py`、`demo.sh`、`README.md` 排除掉，剩下的才算「非源码产物」；
`.tmp` 那条是 `-name "*.tmp"`，全目录递归数。用 `find` 而不是 shell 通配，是有意的：
第 19 节刚演示过 shell 那套 `*` 的脾气会让你少报东西，这里换成 `find` 就没这个盲区。

响过之后，才轮到那三个 0：

```
$ echo "清掉之后：残留子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  非源码产物数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l)  残留 .tmp 个数=$(find . -name "*.tmp" | wc -l)"
清掉之后：残留子目录数=0  非源码产物数=0  残留 .tmp 个数=0
```

同三条 `find`、同一个屏幕上，删掉那个假产物之后三项全是 **0**。顺序不能反：先 1、后 0。
本章整份输出里之所以能出现这三个 0，靠的是两件事叠在一起：每个脚本自己在末段清干净，
demo 收尾那道退出时统一执行的清理再把异常路径兜住 —— 那些**故意**触发的失败既在屏幕上响了，
又没把沙箱留在盘上。
同一条「先弄响再归零」的纪律还用在解释器的编译缓存上：只有 `import` 会让 Python 把字节码写进缓存目录，
本章全程直接跑脚本、脚本之间不互相 import，所以这一对缓存从设计上就不该出现 ——
脚本还是先用一条 `import` 探针把它造出来（闸门当场响），删掉，再报本章跑完之后的计数。

最后回到章目录本体（源文件就住在这里），数一遍产物：

```
$ 〔在 08-files-and-pathlib 里〕 echo ".py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)（这 19 个是源码本身，不是产物）  非源码产物数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l)  子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)"
.py 个数=19（这 19 个是源码本身，不是产物）  非源码产物数=0  子目录数=0
```

`.py 个数=19` 这一项要先说明白：**这 19 个是源码本身，不是产物**，把源码数出来当成污染是最常见的假阳性，
所以式子里 `-name "*.py"` 单独数一列而不是排除它。真正当判据的是后两项：非源码产物数=**0**、子目录数=**0**。
再加两条取证纪律：这一章一条网络命令都没用、没起过任何监听端口（这一项屏幕上只打印条数 ——
端口号和本章无关，不该写进教材）；沙箱本身在全部段落跑完之后也被整份删掉，demo 在收尾打了一行确认。

**带走什么**：① 「清干净了」不是一种感觉，是三条能判的式子：残留子目录数、非源码产物数、`.tmp` 个数；
② 每一条都先被故意弄响一次，再报 0；③ 排除清单要显式写（`README.md`、源码本身），
否则闸门会在你写文档、加脚本的时候误报，误报三次之后就没人信它了；
④ 编译缓存那一档要单独解释：只有 `import` 会让解释器写字节码缓存，本章直接跑脚本所以根本不写 ——
这条不写出来，闸门上那个数就没有解释力，读者会以为是运气。

---

## 自检问题

1. `open(p, 'w')` 和 `open(p, 'a')` 都能写。只把 `p` 打开、一个字都不写、再关掉，两种方式各剩下什么？
（答：`'w'` 旧内容全没、文件变 **0 字节**；`'a'` 一个字节都没动。截断发生在 open 那一刻、不在 close，
所以「进块里反悔不写」救不回来 —— 第 2 节：72 字节那份被 `'w'` 和 `'w+'` 清成 0，`'x'` 撞
`FileExistsError` 时旧内容仍是 72 字节。）

2. `f.write('第一行\n')` 返回 4，磁盘上却是 10 字节。哪一步把这两个数错开的？
（答：返回的是**字符数**，磁盘上是**字节数**，`'第一行'` 三个汉字 9 字节 + `\n` 1 字节。同一条线上还有
`read(3)`：文本模式读 3 个字符，二进制 `read(3)` 读出 `b'\xe7\xac\xac'` —— 半个「第」，拿去解码就是
`unexpected end of data`（第 2 节）。）

3. `cat file | python3 x.py` 和 `python3 x.py | less`，哪个 stdout 是行缓冲？`buffering=8` 时第几个字节才看得见？
（答：接 `less` 那个（`isatty()` 为真）是行缓冲，接 `cat` 那个是块缓冲，进程不 flush 就什么都看不到；
`buffering=8` 要写到第 **9** 个字节才把前 8 个推下去。判据是 `f.line_buffering` 这个属性，不是猜（第 3 节）。）

4. 一个句柄交给 GC 时「有没有真的关掉」，你拿什么证明？为什么「没报错」不算证明？
（答：看 `f.closed`，再把那条 `ResourceWarning` 逼出来：默认过滤器里它是 `ignore`，所以「没报错」本来就不是
证据 —— 第 4 节把过滤拨到 `always` 才打出原文，第 12 节把 `[('ignore', 'ResourceWarning')]` 那张表打出来过。
真漏是 C/D 两种形状：异常对象被存下来、它那一帧里的句柄跟着活；句柄被循环引用扣住，要等分代回收。）

5. 同一份字节，`read()` 给 2 行、逐行给 3 行、`splitlines()` 给 2 行。什么时候必须逐行读？
（答：文件大到装不进内存时只能逐行，而行数取决于 `newline` 翻不翻译。三份字节（LF/CRLF/CR）里只有
`newline=None` 会改内容：通用换行把 `\r\n` 和 `\r` 都翻译成 `\n`；显式给 `'\n'` 或 `''` 就不改。
CR 那份最阴 —— 行数从 2 变 3，`splitlines()` 恒 2（第 5 节）。）

6. 拿 `latin-1` 读一份 UTF-8 文本，会报错吗？读出来几个字符？这算「读对了」吗？
（答：一个错都不报，12 字节的「第一行\n中文字幕\n」照 latin-1 解出 12 个字符，而且还能原样编回同样的 12 字节
—— latin-1 是 0..255 的一一映射，永远合法。反过来把 `'café'` 的 latin-1 字节当 utf-8 解才响：
`can't decode byte 0xe9 in position 3`（第 6 节）。）

7. `mkdir(mode=0o777)` 建出来的目录是几？为什么？`0o444` 的文件能不能删？`0o500` 的目录里呢？
（答：`0o755` —— 落盘是 `mode & ~umask`，本机 `umask=0022`；显式 `0o700` 不被削所以守得住。
删文件看的是**父目录的写位**：`0o444` 的文件照删；`0o500`（有 x 没 w）目录里删什么都 `PermissionError 13`；
`0o300` 目录 `listdir` 当场响，但按已知名字 `unlink` 成功（第 10 节）。）

8. `Path('/tmp') / '/etc/passwd'` 得到什么？`os.path.relpath` 和 `relative_to` 谁能当安全前缀校验？
（答：`/etc/passwd`，前缀被整个丢掉且不报错，`os.path.join` 两条路一样。
`relpath('/etc/passwd', '/tmp')` 照样返回一个看起来像相对路径的东西，拿它当「在这目录里」的判据是安全洞；
要用不响的那一个 `relative_to`（第 9 节）。）

9. `glob('*')` 和 `iterdir()` 列同一目录，谁能拿到 `.env`？`rglob('*.txt')` 等价于什么？结果顺序能信吗？
（答：`iterdir` 和 `Path.glob` 含 `.env`，stdlib `glob` 不含；`rglob('*.txt')` 与 `glob('**/*.txt')` 相等，
`**` 匹配零层或多层，`*` 不跨 `/`。顺序不保证 —— `os.walk` 靠 readdir、`rglob` 靠 scandir，
脚本刻意不把原始顺序打在屏幕上，要可复现自己 `sorted()`（第 11 节）。）

10. 临时文件为什么不能 `tempfile.mktemp()` + `open()`？`mkstemp()` 的权限为什么和 `open()` 不一样？
（答：`mktemp` 只算名字不创建，返回到 `open` 之间有时间窗，docstring 原文
`THIS FUNCTION IS UNSAFE AND SHOULD NOT BE USED.`，本机 `DeprecationWarning` 计数 0；
`mkdtemp` 内部才是「真创建」。`mkstemp` 硬编码 0o600 并带 `O_CREAT|O_EXCL`，`open` 走 `0o666 & ~umask` → 0o644，
同一 umask 下两者相等这条打出 `False`，`umask=0077` 才是 `True`。不带 `O_EXCL` 两次都成功，
带上的那次当场 `FileExistsError 17`、别人的内容一个字节没动（第 13 节）。）

11. `with` 关掉了 `ResourceWarning`，它关掉「半截文件」这个风险吗？
（答：不关，`with` 只保证 `close()`。第 17 节那台状态机把三档打出来：0 字节 / 131000 字节
（`io.DEFAULT_BUFFER_SIZE`）/ 200000 字节全文 —— 前两种死法都留半截文件。
原子写法是同目录 tmp → `flush` → `fsync` → `chmod` → `os.replace`（第 07 章第 18 节说过协议不管事务，这里做实）。）

12. 一份 JSON 写坏会抛什么？把它读成什么都会抛吗？
（答：坏的一侧抛 `JSONDecodeError`（`ValueError` 的儿子），带 msg / lineno / colno / pos，
`Expecting value: line 1 column 167 (char 166)` 这种坐标可直接定位；合法的一侧一个错都不报 ——
tuple 静默变 list、dict 的 int 键回来变 str，而 set / bytes / date / 自定义对象抛 `TypeError`。
类型信息在 dumps 那一步就没了（第 16 节）。）

13. 「写完了」「进内核了」「落盘了」是同一件事吗？各自的边界在哪？
（答：三档。`write()` 只进用户态 buffer；`flush()` 只是把内容交给内核，内核没承诺落盘；`fsync()` 才是那句
「请求内核别丢」。第 17 节两档崩溃现场：A 组两个 200000 的文件互撞，长度还是 200000，内容是混的 + `\x00` 空洞；
B 组只会得到 `{200000, 40000}` 两种全文，残留 `.tmp` 数 **0**。`fsync` 之后拔电会不会丢，本机没有可控掉电手段，未验证。）

14. 文本模式 `seek(30)` 之后再 `read()`，读到几个字符？报不报错和什么有关？
（答：看 codec。utf-8 落在字符边界 → 3 个；落在多字节中间 → 不响、不报错，直接给你半个字符；
utf-16-le 报 `illegal UTF-16 surrogate`、utf-32-le 报 `code point not in range(0x110000)`；越界 → `''` 且不报错。
所以 seek 的坐标只能来自 `tell()` 返回的 cookie，`seek(31)` 回去重读逐字节一致；
负数和 cur/end 带偏移直接 `OSError: can't do nonzero cur-relative/end-relative seeks`（第 18 节）。）

15. 本章跑完，`work/` 里剩下两个 `.tmp`。这是泄漏吗？
（答：是，而且是故意留的证据：第 19 节 `cfgnorm` 用一个假 owner 锁演一次「被抢锁退出」，那条路径不删锁文件，
屏幕上写出「`iterdir` 2 个 `['.audit.tmp', 'app.normalized.json.tmp']`」和「glob 模块 1 个」的差 ——
同一条式子里 `*` 不吃 `.` 开头。第 21 节数的那三个 0 是删掉假产物之后重数的：先 1、后 0，顺序不能反。）

16. 「不合格 0 个」怎么证明它是能响的闸门？`s_checklist.py` 什么时候退非 0？
（答：第 20 节现场造五个坏样本，各撞一条规矩（①编号 `[1, 2, 4]` 不连续、②末段标题是「最后一演」没写收口、
③段数 0、④建了目录却没 rmtree/rmdir、⑤出现 `import socket`），再加一个真合格的 `good_sample.py`——它建目录也
`rmtree` 了，所以 ④ 不误报；`s_checklist.py` 自己也在被检的那 19 个里。把非取证脚本喂进去就响：
`python3 s_checklist.py demo.sh` 退 **1**，能当 CI / make check 的判据。）

---

## 本章没演 / 没验证（绝不编造，都在这）

**字节、编码、文本层**

| 内容 | 状态 |
|---|---|
| `errors=` 其余档位 | 读侧演了 `strict`/`ignore`/`replace`/`surrogateescape`/`backslashreplace`，写侧演了 `namereplace`/`xmlcharrefreplace`；`surrogatepass`、`passthrough`、`codecs.register_error` 自定义 handler 一个没碰 |
| 流式 / 增量解码 | 第 2 节只演了「切在半字符上会响」，没演 `codecs.getincrementaldecoder` 怎么把两半续上 |
| 其它 codec | 演过 utf-8 / utf-8-sig / utf-16(le/be) / gbk / gb18030 / latin-1 / ascii；cp936、big5、shift_jis、cp1252 一次没出现 |
| 区域设置 | demo 只钉了 `C.UTF-8` 和 `LC_ALL=C`；`locale.setlocale`、`sys.stdout.reconfigure`、`os.fsencode`/`fsdecode` 都没演，`getfilesystemencoding` 只测了本机那一个值 |
| BOM 的其余形状 | 只演了 UTF-8 的 `efbbbf`（21 vs 24 字节）；utf-16/32 的 BOM 与字节序、`utf-16` 不带 le/be 时靠 BOM 定序，都没往下追 |

**路径与内核**

| 内容 | 状态 |
|---|---|
| Windows 侧 | `WindowsPath` 实例化只演了「当场 `UnsupportedOperation`」；盘符与 UNC、`\` 分隔、NTFS ACL、`NamedTemporaryFile` 二次 open、`rename` 覆盖语义：第 13 节第 10 段整段**本机未验证**，`samefile` 的「卷序列号 + 文件 ID」也标了**未验证**（第 11 节） |
| 时间戳 | `st_atime`/`st_mtime`/`st_ctime` 只打了字段名；谁改谁、`noatime`/`relatime`、精度，都没演 |
| 权限位的另一半 | `setuid`/`setgid`/`sticky`、`S_IMODE`/`S_ISUID`/`S_ISGID`/`S_ISVTX`：第 10 节只用 `S_IMODE` 反读过一次，其余没演 |
| ACL / xattr | 只演 9 个 POSIX 位，扩展属性与访问控制列表没碰 |
| `os.scandir` / `DirEntry` | 第 11 节只点名 rglob 底层靠 scandir；`DirEntry` 缓存 stat 省 syscall 没演 |
| `Path.walk()` | 正文一次没出现，只用过 `os.walk` |
| `Path.is_relative_to()` | 第 9 节结尾只被点名「要用会响的那一个」；本目录没有任何脚本跑过它 |
| `expanduser` / `expandvars` / `full_match` | `~` 与 `$VAR` 进 pathlib 会怎样，没演（第 8 节只演了 `PurePath('~')` 原样保留） |
| 硬链接的其它入口 | 第 11 节只用 `os.link` 造过一次；`Path.hardlink_to` 没跑 |
| `shutil` 高级 | `copytree` 只当「拷目录树归它管」点了一句；`ignore=` 过滤、`copy2` 保元数据、`shutil.move` 跨设备回退、`disk_usage` 都没演 |
| 零拷贝 / 容量 | `copy_file_range`、`sendfile`、`os.fstatvfs`（磁盘剩余）、`os.utime`（改时间）都没演 |
| TOCTOU 真实竞态 | 第 10 节给了判据（别用 `exists()` 把关，查完到打开之间有一次竞态），**没演过真实竞态** —— 本机没有可控的并发调度手段 |
| 真实网络文件系统 | NFS / CIFS 上的 rename、稀疏、锁语义：没演 |

**格式与落盘**

| 内容 | 状态 |
|---|---|
| json 自定义与钩子 | `default=`、`object_hook`、`object_pairs_hook`、`JSONEncoder` 子类都没演；`parse_*` 里只有 `parse_constant` 跑过（把 `Infinity`/`NaN` 换成 `None`） |
| json 的数与 NaN | `allow_nan=False`（生产端拦 `Infinity`）没演；`1e999` → `inf`、`0.30000000000000004` 只演了现象，没讲 IEEE-754 |
| csv 方言内部 | `excel` / `excel-tab` / `unix` 三个名字列出来了，但 `doublequote`、`QUOTE_RAW`、`skipinitialspace`、`strict`、`has_header`（3.13+）各自怎么改解析，没逐字段验 |
| csv 报错定位 | `reader.line_num` 没演 |
| 其它格式 | `tomllib` / `configparser` / `pickle` / `zipfile` / `sqlite3` 一字未提 |
| `fsync` 的目录那一半 | rename / replace 要让目录项也落盘，得对**目录 fd** 再 fsync 一次；本章只 fsync 文件，这一半没演 |
| `O_SYNC` / `O_DSYNC` / `fdatasync` | 没演 |
| 文件锁 | `flock` / `lockf` / `fcntl` 没演；第 19 节那把锁用 `os.open(O_CREAT\|O_EXCL)` 造，语义只到「能挡一次抢锁」 |
| `PIPE_BUF` 数值 | 第 18 节两子进程各写 40 行共 80 行，只打「全在、没被切开」，不打顺序；`PIPE_BUF` 那个阈值本机**未验证** |
| `mmap` | 第 13 节只提过一次「没名字的临时文件不能 mmap」这个限制，`mmap` 本身没演；第 18 节四种读法只比大小关系，没加 `mmap` 这一档 |
| 异步与别的入口 | `asyncio` 文件 IO、`aiofiles`、`os.popen`、子进程 fd 继承，全没碰 |

**三条已经在正文里标好的口子**（不是这里新加的，是指回原文那三行）：
- `fsync` 之后立刻掉电会不会丢：第 17 节原文标 **未验证**（无可控掉电手段）。
- `os.replace` 撞并发读能不能读到完整的旧版：第 17 节演了「rename 只改目录项、不改 inode」，没跑真实并发读写。
- Windows 全部语义：第 9 节只用 `PureWindowsPath` 演了**纯算法**那一半，文件系统那一半在第 13 节标了未验证。

---

## 怎么跑这一章

```bash
cd python-handbook/08-files-and-pathlib
bash demo.sh                          # 21 节连着跑，屏幕上按节号分段
bash demo.sh --keep > /tmp/08-run.log 2>&1   # 跑完把沙箱留一份，日志另存
ls work/                              # --keep 把沙箱内容拷进本目录的 work/，20 个 demo 子目录
python3 s_checklist.py                # 不带参数：给这 19 个脚本对账
python3 s_checklist.py demo.sh        # 故意喂错东西：闸门响，退出码 1
```

- 屏幕上的分节编号 `1..21` 就是 README 的「第 01..第 21 节」；「第 00 节 · 你想搞清楚的东西」是目录，
  它没有对应的 banner，别去找。
- `demo.sh` 不打印绝对路径：路径只报到最后一级目录（`/tmp/py08-XXXXXX` 在屏幕上就是 `py08-XXXXXX`），
  所以照着屏幕抄命令不会抄到一个不存在的目录。
- demo 每次都在 `mktemp -d`（前缀 `py08-`）里跑，`trap cleanup EXIT` 收尾整份删；
  `--keep` 是唯一让它把内容拷进 `work/` 的口子，拷完仍然删沙箱；`work/` 用完 `rm -rf work/`。
- `work/` 里有两个 `.tmp` 是第 19 节故意留的（假 owner 锁那条退出路径不删锁文件），不是 bug；
  第 21 节数出来的三个 **0**（残留子目录、非源码产物、`.tmp`）是删掉那个假产物之后重数的。
- 想单看某一节：直接 `python3 j_glob_stat.py`。19 个脚本都把东西建在当前目录的 `<名字>-demo/` 里
  （`glob-demo/`、`temp-demo/`、`csv-demo/`……），跑完自己 rmdir —— 这正是第 21 节那条
  「子目录数=0」在源文件层面成立的原因。单跑前记得 `cd` 到本章目录，别在家目录跑。
- demo 只在这几处拨了开关，其余全是解释器默认：开头一次 `export LC_ALL=C.UTF-8`、
  `unset PYTHONUTF8 PYTHONCOERCECLOCALE PYTHONIOENCODING`、`umask 022`（满屏的 0644/0755 都挂在这个数上）；
  第 6 节在**子进程**里试六套 env（含 `PYTHONUTF8=1`、`LC_ALL=C`、`PYTHONCOERCECLOCALE=0`），
  第 12 节在子进程里试 `LC_ALL=C` 关掉 UTF-8 模式那一档，第 13 节试 `TMPDIR`/`TEMP`/`TMP`；
  改 umask 的是第 10 节（钉 0o022）、第 13 节（0o022 与 0o077 两档对照）、第 19 节（临时 0o077 模拟 cron），
  三处都在脚本内恢复并当场核对「相等吗 → True」，不动配置文件。
  自己复现前记一句 `umask`，别顺手带着 `PYTHONUTF8=1` 跑 —— 带着它跑，第 6 节那张表就不成立了。
- 没有 `set -e`、没有 `set -o pipefail`：有些脚本**必须**非零退出，`set -e` 会让 demo 停在那里、
  后面十几节就再也跑不到 —— 闸门靠逐字比对和退出码断言，不靠中途炸掉。
- 屏幕上不出现时间戳、PID、内存绝对值：第 4、18 节那两格只印「谁比谁大」这一种关系，
  第 18 节的并发段只报「80 行全在、没被切开」而不报顺序。
- 「逐字一致」这条不是形容词：第 19、21 节各把同一段跑了两遍，`diff` 输出空才算过。
- 对账工具检查的是**脚本怎么写**（五条规矩），不是脚本跑没跑过：不带参数跑，本机结果是
  `合计：脚本 19 个 ｜ 小节 139 段 ｜ 不合格 0 个`。
- 安全边界：全程不联网、不装包、不起监听端口；第 21 节收尾只打印本机 LISTEN 的**条目数**，不打印端口号
  （端口和本章无关，不该写进教材）。

---

## 术语小抄

| 术语 | 一句话 |
|---|---|
| `bytes` vs `str` | 字节是磁盘上的东西，`str` 是码点序列；`+` 不跨界，`len()` 数的是码点不是字节 |
| 截断（`O_TRUNC`） | `'w'`/`'w+'` 在 **open 那一刻**就把文件清成 0 字节，不是 close 时 |
| 通用换行 | 读侧 `newline=None`：`\r\n` 和 `\r` 都翻译成 `\n`；只有 `None` 会改内容 |
| BOM | 就是 U+FEFF 这个字符本身（UTF-8 下 `efbbbf`）；`utf-8-sig` 读时吃掉它、写时不加 |
| `errors=` | 字节↔字符对不上时的策略：读侧 `ignore` 会**静默丢字节**，`surrogateescape` 能原样回写 |
| surrogateescape | 把坏字节藏进 `U+DC80..U+DCFF`，留着让你写回去；默认解码不用它 |
| `line_buffering` | 一个可以打印的属性，不是玄学：接 tty 为真、接管道为假 |
| `flush` vs `fsync` | flush 只是「交给内核」，内核没承诺落盘；fsync 才是那句「请求内核别丢」（掉电语义本机未验证） |
| `tell()` 的返回值 | 文本模式下是不透明 cookie，只能原样还给 seek，不是字节偏移 |
| 一次性迭代器 | 文件对象就是它自己的迭代器，`iter(f) is f`；走过就空，且不报错 |
| 纯算法 vs 问内核 | `parts`/`name`/`parent` 只切字符串、零 syscall；`exists`/`stat`/`glob` 每问一次都要 syscall |
| `UnsupportedOperation` | 在 POSIX 上实例化 `WindowsPath` 抛它 —— 注意它**不是** `OSError` 的儿子 |
| `lstat` vs `stat` | `stat` 跟着链接看真身，`lstat` 看链接本身（`st_size` 是链接字符串的长度） |
| `st_size` vs 占盘 | `st_blocks * 512` 才是实占；稀疏文件让两个数差几个量级 |
| `samefile` | 判 `(st_dev, st_ino)` 这一对，不看路径字符串 |
| `EEXIST` / `ENOTDIR` / `ENOTEMPTY` | 17 / 20 / 39；mkdir、rmdir 的三种响法各对应一种现场 |
| `O_CREAT \| O_EXCL` | 「我还没建过，建不出来就报错」：`mkstemp` 靠它，`mktemp` 没有它 |
| csv 的 `newline=''` | 交给 csv 自己管行尾；显式 `\r\n` 会和文本层翻一遍叠成 `\r\r\n` |
| JSON 的 round-trip | 值能回来，**类型不能**：tuple→list、int 键→str、set/bytes/date→当场 `TypeError` |
| `JSONDecodeError` | `ValueError` 的儿子，带 msg/lineno/colno/pos，坐标可直接定位 |
| 原子替换 | 同目录 tmp → flush → fsync → chmod → `os.replace`；跨设备退 18（`EXDEV`） |
| EAFP | 直接 `open`、按 `errno`（2/13）分派；别用 `exists()` 把关 |

---

## 本章之后

第 09 章讲 `import` 这条线，正好接在本章第 21 节那句话后面：只有 `import` 会让解释器写字节码缓存，
本章直接跑脚本所以根本不写 —— 下一章把那个「写」字打开：`__pycache__` 与 `.pyc` 里到底存了什么、
`__name__ == '__main__'` 那个分岔什么时候必须写、包和 `__init__.py` 到底改变了解析的哪一步，
以及 `python3 -m venv` 在这台机器上为什么会失败（本机没有 `ensurepip`，第 09 章会照着报错原文重演一遍，
不是「venv 不能用」）。第 11 章再回来还本章欠的两笔账：`PIPE_BUF` 那个真实阈值，
和第 17 节只演了单进程版本的并发写 —— 那两件事都需要真的把进程排开才测得准。

---

## 本章回指

| 本章哪里用到 | 出处 |
|---|---|
| `'w'` 一 open 就把旧账本清成 0 字节，全程没有一条报错提醒 | 第 07 章第 16 节「静悄悄杀手：那些不报错的错」—— 本章第 2 节把它落到文件上 |
| `FileNotFoundError`（2）和 `PermissionError`（13）分开接，别用 `except Exception` 一锅咽 | 第 07 章第 03 节那棵异常树；第 07 章第 08 节「`OSError` 带 `errno`/`strerror`/`filename`，`args` 是两个元素」 |
| I/O 一律 EAFP：直接 open、按 `errno` 分派，不在 open 前面加 `exists()` 把关 | 第 07 章第 18 节「EAFP + 按 `errno` 分派」 |
| `close` 幂等、`__exit__` 重复调用也安全、with 的三条合同 | 第 07 章第 10 节手写 `__enter__`/`__exit__`；本章第 4 节把它落到文件句柄上 |
| `with` 只管 close、不管事务；原子替换的形状是同目录 tmp + `os.replace` | 第 07 章第 18 节；本章第 17 节把这句话做实（三档崩溃现场 + A/B 两组） |
| 「默认值是别人的决定」 | 第 07 章那条规矩；本章第 16 节在 `separators` 上落地 —— 一个 `indent` 就把默认分隔符换掉，两处默认互不一致 |
| 「迭代器只能走一遍」 | 第 04 章第 17 节（生成器的惰性、一次性）；本章第 15 节文件迭代器第二次 `list(it)` 给 `[]`，空且不报错 |
| `str` 是码点序列、`len()` 数码点；判 Unicode 相等要先 `normalize` | 第 03 章；本章第 2 节把同一件事换成字节尺子 |
| 逐字校验：README 引的每一行都要真跑出来过 | 第 07 章第 20、21 节那套闸门；本章第 20 节用同一个工具对账（19 脚本 / 139 段 / 不合格 0） |
| 恒真守卫不算守卫：先把它故意弄响一次，再报 0 | 第 07 章第 20 节；本章第 20 节五个坏样本、第 21 节三个计数先 1 后 0 |
| 报错原文集：看到什么往哪查 | 第 07 章第 08 节；本章把新增的原文（`surrogates not allowed`、`field larger than field limit`、`Illegal trailing comma`）就地演完，不另开字典 |
---
