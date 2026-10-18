# 第 03 章：字符串与文本（切片、f-string、UTF-8 字节 vs 字符）

这一章只回答一件事：**Python 的 `str` 里装的到底是什么，和你在 C 里那套「字符串 = `char*` + `\0` + 字节数」
差在哪里**。

写给会 C 和数据结构的你。你在 C 里那套直觉全都自洽：字符串就是一串字节、长度就是字节数、
`strlen` 扫到 `\0` 为止、`printf("%-10s")` 补十个字符宽、越界就是未定义行为。
本章前八节先把「`str` 不是字节串」这一条钉进地里（三个数字并排、é 的两种写法、六种 codec 的字节数表），
中间十二节把切片和 f-string 的脾气全部实测一遍，后九节处理「文本要落地成字节、要排版、要跟 `bytes` 打交道」
时真正会咬人的地方。

本章每一条结论都出自本机 **一次真实运行**：`bash demo.sh`，779 行输出，CPython 3.14.4 / Ubuntu。
跑不出来的东西一律不进正文，全部列在最后的「本章没演 / 没验证」里。
只用标准库，没 pip，没联网，没改任何配置文件。

> **关于引用输出里的随机后缀**：`demo.sh` 每次跑都 `mktemp -d` 一个随机沙箱，本章这次是
> `/tmp/py03-hjVjJH`。捕获里凡出现这个目录名的地方，你复现时会换成自己的随机后缀，其它字符逐字不变。
> 除此之外本章**不打印任何绝对路径**，所以输出里不会有用户名（第 24 节还顺手证明了目录里没留产物）。

## 你会遇到的场景

| 你的困惑 / 你听说的说法 | 本章哪节回答 | 对应 demo 节 |
| --- | --- | --- |
| `len(s)` 数的到底是字符还是字节？为什么中文 `len` 是 2 而文件里是 6 个字节 | 第 1、22 节 | 第 1、22 节 |
| 同样一个 `é`，为什么有时 `len` 是 1 有时是 2，`==` 还是 False | 第 2、3 节 | 第 2、3 节 |
| 一个字符到底占几个字节？`utf-8` / `gbk` / `latin-1` 谁装得下谁装不下 | 第 4、8 节 | 第 4、8 节 |
| `UnicodeEncodeError` 和 `UnicodeDecodeError` 那句话怎么读，`position` 是谁的下标 | 第 5、6 节 | 第 5、6 节 |
| `errors='ignore'` / `'replace'` 会发生什么；能不能靠乱码自救回原文 | 第 4、5、6 节 | 第 4、5、6 节 |
| 文件开头那个看不见的 `\ufeff` 是什么，`utf-8-sig` 又是什么 | 第 7 节 | 第 7 节 |
| 我写的编码名到底本机认不认（`utf8`？`cp936`？`mbcs`？） | 第 8 节 | 第 8 节 |
| `s[100]` 抛异常，`s[10:20]` 为什么什么都不说就给个空串 | 第 9、10 节 | 第 9、10 节 |
| `s[::-1]` 为什么能反转；`s[:0:-1]` 为什么少了第一个字符；`slice` 对象有啥用 | 第 10 节 | 第 10 节 |
| 为什么 `s[1:3] = "X"` 不行而 `lst[1:3] = [...]` 行 | 第 10 节 | 第 10 节 |
| `{x=}` 是什么鬼；`f"{'ab':*^10}"` 那串符号怎么读；`f"{'中':>6}"` 为什么补不齐 | 第 11、12 节 | 第 11、12 节 |
| 网上说 f-string 里不能放同种引号 / 不能放反斜杠，3.14 到底行不行 | 第 13 节 | 第 13 节 |
| `strftime('%A')` 打出英文星期，设了 `LC_ALL=zh_CN.UTF-8` 还是英文 | 第 14 节 | 第 14 节 |
| f-string / `.format` / `%` / `+` / `join` 到底该用哪个，各差在哪 | 第 15、16 节 | 第 15、16 节 |
| `split()` 和 `split(' ')` 结果不一样；`split('')` 为什么报错 | 第 17 节 | 第 17 节 |
| 一段文本里有 `\r\n`、`\x0b`、`\u2028`，`split('\n')` 切不开 —— `splitlines` 能 | 第 18 节 | 第 18 节 |
| 为什么 `split` 再 `join` 回去的字符串和原文不一样 | 第 19 节 | 第 19 节 |
| `'xyfooxy'.strip('xy')` 我以为是去前缀，怎么把 `m` 都快啃没了 | 第 20 节 | 第 20 节 |
| `'ß'.upper()` 怎么变成两个字符；土耳其的 `İ`/`ı` 又怎么回事 | 第 21 节 | 第 21 节 |
| `isdigit()` 为 True 为什么 `int()` 还是报错；汉字算 `isalpha` 吗 | 第 21 节 | 第 21 节 |
| 打印中文表格怎么都对不齐；`east_asian_width` 是干什么的 | 第 22 节 | 第 22 节 |
| `b'abc'[0]` 为什么给的是 97；`bytes` 和 `str` 为什么不能相加 | 第 23 节 | 第 23 节 |
| `for c in s` 拿到的 `c` 是什么类型；`'国中' in '我爱你中国'` 为什么 False | 第 23 节 | 第 23 节 |

## 一句话版本

1. **`str` 是码点序列，`bytes` 才是字节序列，两者是两个类型，`+` 不跨界**：
   `len()` 数码点，`len(s.encode('utf-8'))` 数字节，终端里占几格还得另算 —— 三把尺子互不换算
   （第 1、4、22、23 节）。
2. **Python 的 `==` 比的是码点序列逐个相同，不是「看起来一样」**：
   所以判 Unicode 相等之前要 `unicodedata.normalize('NFC', ...)`（第 2、3 节）。
3. **切片越界一律静默夹断，不报错；`str` 任何下标/切片都只能读不能写**：
   前者藏 bug，后者是第 02 章「不可变」的兑现（第 9、10 节）。

后面二十五节（含第 0 节环境体检）都是这三句话在各种角落的兑现。

## 引用约定

- 下面每个 ````text` 块里的**命令与输出**，都是从那次捕获里逐行抄下来的**原文**（捕获存在
  `~/.cache/handbook-captures/py03.txt`）。带 `$ ` 提示符的围栏里，每一行都在捕获中逐字存在
  （只允许沙箱随机路径归一化和连续空白折叠两种差异），由 `~/.local/bin/verify_quotes.py` 逐行核对，
  闸门是「未匹配行 0」。
- **要省略就明说**：想截断时关掉围栏，正文写清砍了哪几行，再开一个不带 `$ ` 的围栏继续抄。
  带 `$ ` 的围栏里绝对不出现省略号，也绝对不改写。
- **locale 设置（本章只动了一处）**：脚本运行前本机是 `LANG=zh_CN.UTF-8`、`LC_ALL` 未设；
  `demo.sh` 自己 `export LC_ALL=C.UTF-8`，目的是让报错文本能逐字比对（英文报错比中文报错更适合抄进书里），
  **不是为了改变 `str` 的行为** —— 第 21 节会现场证明 `str.upper/casefold` 在这两种 locale 下输出逐字相同。
  唯一真的跟 locale 纠缠的是 `strftime`（第 14 节）。
- **计时数字会抖**：`timeit` 那节（第 16 节）除了原始数字，还抄了 demo 自己打的 `[形状]` 行
  （把所有数字换成 `N`），形状行是可复现的。
- 命令一律写 `python3`（本机 PATH 里没有 `python`，第 01 章讲过）。
- 本章**不演示文件读写**：`open()` 时 `encoding` 该给什么、`newline` 怎么处理 `\r\n`，都留给第 08 章。
  这里只讲 `str` / `bytes` / `codec` 本身。

## 心智模型

```text
          Python 里有三把互相独立的尺子，量的是同一个「字符串」的三层东西

  ①  str   一列【码点】(code point)                 len(s) 数这一层
        '中'  '文'                       → 2
          │  .encode('utf-8')      每个码点按变长规则变成 1~4 个字节
          ▼
  ② bytes  一列【字节】0x00~0xFF                     len(b) 数这一层
        e4 b8 ad e6 96 87              → 6      ← C 的 char* 活在这一层
          │  送到终端 / 写进文件
          ▼
  ③ 显示格  终端网格里占几列                          只能自己估
        中(2 格) 文(2 格)               → 4      ← 排版对齐只看这一层

  ①↔② 的换算器叫 codec（utf-8 / gbk / latin-1 / utf-16 …），换不过去就抛
       UnicodeEncodeError（①→②，position 是【码点】下标）
       UnicodeDecodeError（②→①，position 是【字节】下标）   ← 两个方向坐标不同！
  ②→① 选错 codec 就是乱码：bytes 自己不带「我是哪种编码」的记号（第 4、6 节）
  ①↔③ 没有任何函数：宽度是终端和字体的事，Python 只给 unicodedata.east_asian_width 一个提示（第 22 节）

  还有一条藏在 ① 里的坑：「看起来一个字符」在 ① 层可能是 1 个或 2 个码点
  （é = e + 组合重音符，第 2 节）。所以 ① 层的「字符数」也不等于人眼看到的字数。
```

## 0. 环境体检：本机默认 locale、脚本设了什么、两个 encoding 各是什么（demo 第 0 节）

先把「在哪个解释器上、什么 locale 下测的」钉死。这两件事本章后面各有一次兑现
（报错文本能不能逐字比对、`strftime` 打中文还是英文）。

```text
[环境] 本脚本运行前：LANG=zh_CN.UTF-8  LC_ALL=（未设）
[环境] 脚本已 export LC_ALL=C.UTF-8（只为报错/消息可逐字比对；str 方法本身与 locale 无关）
```

解释器与「`python` 这个命令存不存在」：

```text
$ python3 --version
Python 3.14.4
[退出码 0]

$ command -v python python3
/usr/bin/python3
[退出码 0]
```

`command -v` 只打出一个路径 —— `python` 这个名字本机没有。

有两个都叫「encoding」的东西，含义完全不同，别混：

```text
$ python3 -c "import sys; print(sys.getdefaultencoding())"
utf-8
[退出码 0]

$ python3 -c "import locale; print(locale.getpreferredencoding())"
UTF-8
[退出码 0]
```

读法：`sys.getdefaultencoding()` 是 **`str`→`bytes` 的缺省编码**，恒为 `utf-8`，和 locale 无关，改不掉；
`locale.getpreferredencoding()` 是 **打开文件不给 `encoding` 时用什么**，跟 locale 走。
后者什么时候咬人是第 08 章的事，本章只需要记住：**它俩不是一回事**。

Unicode 数据的版本也要钉死 —— 本章所有「某字符算宽 / 算字母 / 有没有名字」的判定都受它支配：

```text
$ python3 -c "import unicodedata; print(unicodedata.unidata_version)"
16.0.0
[退出码 0]

$ python3 -X utf8 -c "import sys; print(sys.flags.utf8_mode)"
1
[退出码 0]
```

★ **结论（第 0 节）**：本机 = CPython 3.14.4 + Unicode 16.0.0 + `LC_ALL=C.UTF-8`（demo 自己设的）。
第 21、22 节那些「算不算字母」「占几格」的判定，换一份 Unicode 数据就可能变。

## 1. `str` 是码点序列，不是字节：三个数字并排 + `b"中文"` 的编译期拒绝（demo 第 1 节）

C 里 `char*` 一个概念兼两件事：一串字节、以及它们的个数。Python 把这两件事拆成两个类型：
`str`（码点）和 `bytes`（字节）。同一段中文，两个长度：

```text
$ python3 -c "s = '中文'; print(len(s), len(s.encode('utf-8')), len(bytes(s, 'utf-8')))"
2 6 6
[退出码 0]
```

`2 6 6`。第一个是码点数，后两个是字节数。CJK 基本区的汉字在 UTF-8 下每个占 3 字节，
所以 2 个码点 = 6 字节。差 3 倍不是巧合，是变长编码的算法规则。

想偷懒写个 `b"中文"`？编译期就拒绝你：

```text
$ python3 -c "print(len(b\"中文\"))"
  File "<string>", line 1
    print(len(b"中文"))
              ^^^^^
SyntaxError: bytes can only contain ASCII literal characters
[退出码 1]
```

注意这是 `SyntaxError` 不是 `ValueError` —— 程序根本没开始跑。`bytes` 字面量从语法上就只许放 ASCII，
这条硬规则本身就是「字节不是字符串」的最好证据。

把上面两条合成一个脚本再打一遍（`a_codepoints.py` 的第 1 部分）：

```text
$ python3 a_codepoints.py 1
====================================================================
一、三个数字并排：字符数 / UTF-8 字节数 / bytes 对象长度
====================================================================
len(s)              = 2          ← s 是 str，数的是【码点】
len(s.encode())     = 6           ← 先变 bytes，数的是【字节】
len(bytes(s,'utf-8')) = 6           ← 同上，构造器写法
repr(b)             = b'\xe4\xb8\xad\xe6\x96\x87'
b[0] 是码点吗？      = 228（0xe4），是第 1 个【字节】，不是任何汉字
'中' 的 UTF-8 字节   = b'\xe4\xb8\xad'（3 字节）
'a' 的 UTF-8 字节    = b'a'（1 字节）
→ 同一个 str，len 是 2；编码成 UTF-8 是 6 个字节。差 3 倍，不是巧合：
  CJK 基本区汉字每个码点在 UTF-8 里占 3 字节，ASCII 占 1 字节。
```

（这一段捕获里还没完：后面 5 行是「`b"中文"` 编译期拒绝」在脚本里的复述，上面砍掉了它。
逐字接抄在下面这个不带提示符的块里，末尾是这一段自己的退出码：）

```text
字面量 b"中文" 在写的时候就会被编译期拒绝：
  SyntaxError: bytes can only contain ASCII literal characters
  （发生在 1 行 5 列；bytes 字面量只许放 ASCII）
→ 想要「中文的字节」只有两条路：'中文'.encode('utf-8') 或 bytes('中文', 'utf-8')，
  或者手写转义 b'\xe4\xb8\xad\xe6\x96\x87'。没有第三种。
[退出码 0]
```

`b[0] = 228` 这一行值得多看一眼：`228`（`0xe4`）**不是任何汉字，也不是任何字符**，
它只是那个 3 字节序列的第一个字节。C 里 `s[0]` 至少还是个能看的 `char`；这里 `bytes[0]` 是个纯整数
（第 23 节再对照一次）。

★ **结论（第 1 节）**：**要字节先 `encode`，没有第三条路**。
以后写 `len(...)` 之前先问自己一句「这里要的是 2 还是 6」—— 问不出来的地方就是 bug。

## 2. é 的两种写法：看起来一模一样，`==` 却是 False（demo 第 2 节）

这一节是本章最反直觉的一条，也是把「字符」这个词彻底打碎的一节。

Unicode 里 é 有两个合法身份：一个预合成码点 `U+00E9`，或者 `e` + `U+0301`（组合用急性重音符）。
两种写法终端里画出来的字形完全相同：

```text
$ python3 a_codepoints.py 2
====================================================================
二、é 的两种写法：看起来一模一样，== 却是 False
====================================================================
写法一 a = "\u00e9"    → repr: 'é'   len: 1
写法二 b = "e\u0301"  → repr: 'é'   len: 2
（上面两行的引号里长得一样 —— 终端按字形画出来就是同一个 é）
a 的码点: ['0xe9']
b 的码点: ['0x65', '0x301']
a == b  → False
名字 a: LATIN SMALL LETTER E WITH ACUTE
名字 b: ['LATIN SMALL LETTER E', 'COMBINING ACUTE ACCENT']
类别: U+00E9 是 Ll；U+0301 是 Mn（Mn=组合记号，不单独成字）
→ str 的 == 比的是码点序列是否逐个相同，不是「看起来像不像」。
  所以搜索引擎/表格软件里算同一个词，在 Python 里默认算两个不同的键。
[退出码 0]
```

三个取证点连成一条锁链：

- `len` 一个 1 一个 2 —— 说明 `str` 的「长度」真的是**码点个数**，跟字形无关；
- `unicodedata.name()` 一个 `LATIN SMALL LETTER E WITH ACUTE`、一个 `... + COMBINING ACUTE ACCENT` ——
  说明它们是两个不同的 Unicode 字符，只是渲染结果重叠；
- `U+0301` 的类别是 `Mn`（Mark, nonspacing，组合记号）—— 它自己**不单独成字**，必须挂在前一个字母上。

这就是「人眼的一个字符」和「`str` 的一个元素」不是一回事的确凿证据。任何用 `len(s) <= 20` 限制
用户昵称长度的代码，碰上 `e\u0301` 都会算错。

★ **结论（第 2 节）**：`==` 只认码点序列。`a == b` 为 False 不是 bug，是它在忠实报告
「这真的不是同一串东西」。想「按人眼的意思比相等」，必须先归一 —— 下一节。

## 3. `unicodedata.normalize` 现场来回（NFC / NFD / NFKC）（demo 第 3 节）

「归一化」就是把上面两种写法统一成一种。常用的两种形式：

- **NFC**（合成形式）：`e` + 组合重音符 → `U+00E9`。人写的、Web 上传的、Windows/iPhone 打的，多数是这种。
- **NFD**（分解形式）：`U+00E9` → `e` + 组合重音符。macOS 的文件名系统就是这么存的。

```text
$ python3 a_codepoints.py 3
====================================================================
三、unicodedata.normalize 现场来回
====================================================================
NFC(b) == a → True
NFD(a) == b → True
来回一轮 NFC(NFD(a)) == a → True
len: a=1  NFC(b)=1  NFD(a)=2
```

（捕获里这个块还继续输出「café 两种形态」和 NFKC 的部分，共 15 行；先关掉围栏，
下面两段逐字接抄，中间只删了空行位置。）

```text
 café 两种形态：NFC 长度 4，NFD 长度 5，相等？False
字典里存了 NFC 版；用 NFD 版查能命中吗 → False（False 就意味着实际会抛 KeyError）
  （真实现场：macOS 的文件名走 NFD，你在 Windows/iPhone 打的 é 是 NFC —— 跨平台比对用户名/歌名时这就是个 bug。）
比对前先归一：NFC(NFD 版) in d → True
```

这一小段是**真正会发生在生产里的 bug**：字典里存了 NFC 版的键，用 NFD 版去查就是 `KeyError`，
而两个键在屏幕上长得一模一样。（块开头那个「空格 + café」是原文：两种形态并排打出来，
肉眼完全分不出，`len` 一个 4 一个 5。）

还有一个更狠的 **NFKC**（兼容分解），它不光拆组合记号，还把「长得像但历史上另有身份」的字符
全折成普通形式：

```text
顺手记两条：
本机 Unicode 数据版本 unicodedata.unidata_version = 16.0.0
NFKC（兼容分解，比 NFC 更狠）：'\u00bd'(½)→'1⁄2'，'\u00b2'(²)→'2'
NFKC 拆连字：'ﬁ'(U+FB01)→'fi'
→ 判「Unicode 等不等」的标准做法：unicodedata.is_normalize(
    'NFC', x) 先验，或两边都 normalize('NFC', …) 再 ==。
[退出码 0]
```

注意 NFKC 会把 `½` 变成 `1⁄2`（三个字符，中间是 U+2044 分数斜线）、把连字 `ﬁ` 变成 `fi` ——
**它会改变长度、改变外观含义**，所以别拿它当默认归一化用：做「用户名 / 检索词查重」用 `NFC`，
要「搜遍各种奇怪写法」才考虑 `NFKC`（或者两种都建索引）。

★ **结论（第 3 节）**：只要字符串来自外部（用户输入、文件名、复制粘贴、别的系统），
比较之前一律 `unicodedata.normalize('NFC', s)`；`is_normalize('NFC', s)` 是那个「需不需要归一」的判据。

## 4. 同一个字符在不同 codec 下占几个字节（demo 第 4 节）

「一个字符几个字节」这个问题没有独立于 codec 的答案。五个字符 × 六种 codec，本机实测：

```text
$ python3 b_codecs.py 1
====================================================================
一、同一个字符在不同 codec 下占几个字节
====================================================================
字符      utf-8        gbk    gb18030       big5    latin-1     utf-16
中             3          2          2          2        编不下          4
ü             2          2          2        编不下          1          4
ß             2        编不下          4        编不下          1          4
±             2          2          2          2          1          4
emoji          4        编不下          4        编不下        编不下          6
→ 同一行数字不同就是「编码」的全部意义：码点不变，字节数随 codec 变。
→ latin-1 只覆盖 U+0000~U+00FF，汉字直接编不下；gbk 连 ß 和 emoji 都不收，
  但 gb18030（国标强制版）全收 —— 收不收是 codec 的属性，不是 Python 的。
```

几个数字背后的道理：

- **UTF-8 是变长的**：ASCII 1 字节、拉丁扩展 2 字节、CJK 基本区 3 字节、emoji（`U+1F600` 这种）4 字节。
  码点越大字节越多，而且**自同步**（看字节高位就知道是首字节还是延续字节）——
  这正是第 6 节能精确报「哪个字节坏了」的原因。
- **latin-1 是恒等映射**：`U+0000~U+00FF` 一一对应，`ü`（`U+00FC`）就是 `0xfc` 一个字节，
  超出这个范围直接编不下。这也解释了本节末尾「乱码自救」为什么成立。
- **utf-16 是定长 2 字节单元**：BMP 内字符 2 字节；emoji 在 BMP 之外要用两个单元（代理对）→ 4 字节，
  表里那行是 6 字节，因为 `utf-16` 默认还自带 2 字节 BOM（见第 7 节）。代理对本章没细演，进末尾清单。

编不下的报错原文（捕获里就在下一段）：

```text
编不下的报错原文（一字不改）：
  UnicodeEncodeError: 'gbk' codec can't encode character '\xdf' in position 0: illegal multibyte sequence
  这条消息措辞很怪（illegal multibyte sequence），别被绕进去：
  它就是「这个字符该 codec 装不下」的意思。
```

（`\xdf` 就是 `ß`。注意这条是 **`encode` 方向**，所以 `position 0` 是 `str` 的字符下标。）

最后是本章最实用的一段：**乱码怎么自救**。

```text
往返与乱码：
  '中文'.encode('gbk') → b'\xd6\xd0\xce\xc4'；原样 decode('gbk') 回去 → '中文'
  UTF-8 的 3 字节按 latin-1 读 → 'ä¸\xad'（每个字节各配一个字符）
  再按 latin-1 编回去、按 UTF-8 解 → '中' ← 经典乱码的自救手法
[退出码 0]
```

原理：latin-1 是 0..255 的**恒等双射**，所以「按 latin-1 错误解码出来的怪字符串」再按 latin-1
编码回去，字节**一个不差**地还原了。于是 `s.encode('latin-1').decode('utf-8')` 能把那串怪字符
修回 `中`。看到网页标题里出现「ä½ å¥½」这种鬼画符，就是 UTF-8 字节被按 latin-1 读了。

★ **结论（第 4 节）**：字节数不是字符的属性，是 **（字符, codec）这对组合**的属性。
`gb18030` 全收、`gbk` 连 `ß` 都不收 —— 这是 codec 表决定的，跟 Python 无关。

## 5. 编码侧编不下：报错原文 + `errors=` 三种命运（demo 第 5 节）

`'中文'.encode('latin-1')` 的完整 traceback：

```text
$ python3 -c "'中文'.encode('latin-1')"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    '中文'.encode('latin-1')
    ~~~~~~~~~~~~~^^^^^^^^^^^
UnicodeEncodeError: 'latin-1' codec can't encode characters in position 0-1: ordinal not in range(256)
[退出码 1]
```

逐行读：`File "<string>", line 1` —— 因为用了 `-c`；`~~~~~~~~~~~~~^^^^^^^^^^^` 是 3.11+ 的
**列级标注**，波浪线指出这次调用的对象和参数（第 01 章拆过这个格式）；最后一行是异常类和消息。

消息里的 **`position 0-1` 是 `str` 的字符下标**（编码方向，输入是 `str`）。写成区间，因为
`中` `文` 两个都编不下。异常对象上还挂着可以直接读走的证据：

```text
$ python3 b_codecs.py 2
====================================================================
二、编码侧的三种命运：errors='strict' / 'replace' / 'ignore'
====================================================================
strict  → 抛异常：UnicodeEncodeError: 'latin-1' codec can't encode characters in position 0-1: ordinal not in range(256)
          e.encoding='latin-1' e.start=0 e.end=2 e.object='中文'
          e.reason='ordinal not in range(256)'（第一个编不下的是 '中'）
replace → b'??x'   ← 编不下的变 ?
ignore  → b'x'            ← 直接扔掉
backslashreplace → b'\\u4e2d'
xmlcharrefreplace → b'&#20013;'
namereplace → b'\\N{CJK UNIFIED IDEOGRAPH-4E2D}'
→ replace/ignore 是「数据无声消失」的开关，默认 strict 才是安全的。
[退出码 0]
```

`e.start=0 e.end=2` 是**半开区间**（Python 通用习惯），刚好对上消息里的 `position 0-1`。
六种 `errors=` 的代价完全不等：

| `errors=` | 本机实测结果 | 什么时候用 |
| --- | --- | --- |
| `strict`（默认） | 抛异常 | 默认就好，问题当场暴露 |
| `replace` | `b'??x'` | 只在乎「别崩」，例如日志里打一段未知文本 |
| `ignore` | `b'x'` | **危险**：汉字没了，字节长度也对不上 |
| `backslashreplace` | `b'\\u4e2d'` | 可逆、信息不丢，最安全的「不崩」方案 |
| `xmlcharrefreplace` | `b'&#20013;'` | 写 HTML / XML |
| `namereplace` | `b'\\N{CJK UNIFIED IDEOGRAPH-4E2D}'` | 需要人类可读字符名时 |

★ **结论（第 5 节）**：把 `errors='ignore'` 写进代码之前，先确认你能接受 `b'x'` 这种结果
（**内容没了、还是成功返回**）。要「不崩且不丢信息」，用 `backslashreplace`。

## 6. 解码侧字节不对：`UnicodeDecodeError` 完整原文（demo 第 6 节）

拿 GBK 的 4 个字节冒充 UTF-8：

```text
$ python3 -c "b'\xd6\xd0\xce\xc4'.decode('utf-8')"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    b'\xd6\xd0\xce\xc4'.decode('utf-8')
    ~~~~~~~~~~~~~~~~~~~~~~~~~~^^^^^^^^^
UnicodeDecodeError: 'utf-8' codec can't decode byte 0xd6 in position 0: invalid continuation byte
[退出码 1]
```

这条消息信息量很大，逐段拆：

- `'utf-8' codec` —— 谁在抱怨；
- `can't decode byte 0xd6` —— **具体哪个字节**出问题了（`0xd6` 是 2 字节序列的首字节形态，
  可它后面跟的 `0xd0` 不是 `10xxxxxx` 的延续字节形态）；
- `in position 0` —— **字节下标**（解码方向输入是 `bytes`，和上一节编码方向的字符下标正好相反！）；
- `invalid continuation byte` —— 失败的具体原因。

异常对象的料和几种命运：

```text
$ python3 b_codecs.py 3
====================================================================
三、解码侧：错字节的三种命运 + UnicodeDecodeError 的料
====================================================================
strict  → 抛异常：UnicodeDecodeError: 'utf-8' codec can't decode byte 0xd6 in position 0: invalid continuation byte
          e.start=0  e.end=1  ← 这是【字节位置】，不是字符位置
          e.object=b'\xd6\xd0\xce\xc4'（字节原样，共 4 个）
          e.reason='invalid continuation byte'
```

（捕获里紧接着的 5 行是 `replace` / `ignore` / `backslashreplace` / `xmlcharrefreplace` 的结果。
`replace` 那一行的输出里是**货真价实的 4 个 U+FFFD 替换字符**，抄进 Markdown 里等于塞进 4 个
不可见怪字符，所以那一行我**不复制、只描述**：`errors='replace'` 得到长度为 4 的串，
每个坏字节换一个 U+FFFD。其余四行逐字接抄在下面：）

```text
ignore  → ''   ← 长度 0，坏字节人间蒸发
backslashreplace → '\\xd6\\xd0\\xce\\xc4'
decode 用 xmlcharrefreplace → TypeError: don't know how to handle UnicodeDecodeError in error callback
  （那个 handler 只服务于编码方向，解码方向没有对应物）
```

最后那条 `TypeError` 是意外收获：**错误处理器分方向**，`xmlcharrefreplace` 只在 `encode` 侧存在，
拿到 `decode` 侧用会直接报「don't know how to handle UnicodeDecodeError in error callback」。

「位置到底是谁的下标」，本机又实测了两条容易按字符数错位的：

```text
位置报的是字节下标，这里最容易按字符数错位：
  前面有 2 个 ASCII 字节 → 报错说 'utf-8' codec can't decode byte 0xd6 in position 2: invalid continuation byte（字节位置 2，不是字符第 0 个）
  只留半个汉字：'utf-8' codec can't decode bytes in position 0-1: unexpected end of data
  → start=0 end=2 是【坏字节区间】，区间可以不止 1 字节
[退出码 0]
```

三条要点：

1. **解码方向的 `position` 是字节下标，编码方向的 `position` 是码点下标**。同一条消息两套坐标 ——
   按错一次，你在文件里定位 bug 就会永远偏。
2. **截断**（网络分包只收到半个汉字）报的是 `unexpected end of data`，区间 `0-1` 两个字节；
   说明 `e.start/e.end` 报的是**坏字节区间**，不一定只有 1 字节。
3. `errors='ignore'` 给**空串** —— 4 个字节全没了还返回成功。这是本章最好的反面教材：
   **不报错不等于没错**。

★ **结论（第 6 节）**：`UnicodeDecodeError` 不是「编码错了」四个字那么笼统，它给了你
**哪个字节、哪个位置、为什么不行**。三条信息足够定位任何乱码；`errors='ignore'` 会把它们全抹掉。

## 7. BOM：`EF BB BF`、U+FEFF、`utf-8-sig`（demo 第 7 节）

```text
$ python3 b_codecs.py 4
====================================================================
四、BOM：U+FEFF 与 utf-8-sig
====================================================================
'中文'.encode('utf-8')      = b'\xe4\xb8\xad\xe6\x96\x87'
'中文'.encode('utf-8-sig')  = b'\xef\xbb\xbf\xe4\xb8\xad\xe6\x96\x87'
BOM 三字节 EF BB BF 就是字符 U+FEFF 的 UTF-8 编码：b'\xef\xbb\xbf'
名字：U+FEFF 叫 ZERO WIDTH NO-BREAK SPACE（历史包袱，如今主要当 BOM 用）
用 utf-8 读带 BOM 的字节流：'\ufeff中文' → 首字符是 U+FEFF，混进正文了
用 utf-8-sig 读同一串：      '中文' → BOM 被吃掉
用 utf-8-sig 读不带 BOM 的：  '中文' → 也正常（sig 遇无则原样解）
utf-16 默认自带 BOM：'中'.encode('utf-16') = b'\xff\xfe-N'（FF FE = 小端标记）
→ Windows 记事本的 UTF-8 文件常带 BOM；拿 utf-8 读，第一个字符就多出来。
[退出码 0]
```

BOM（Byte Order Mark）在两件事之间摇摆：给 UTF-16 标字节序（`FF FE` 小端 / `FE FF` 大端），
以及在 UTF-8 里当「我是 UTF-8」的签名。**UTF-8 不需要字节序标记**（它字节序无关），
所以 UTF-8 的 BOM 纯属历史遗留，除了咬人没有别的作用：

- 拿 `utf-8` 读带 BOM 的内容 → `'\ufeff中文'`：第一个字符是零宽的 `\ufeff`。
  于是「第一个字符」其实是个零宽字符：`startswith('#')` 这类判断、CSV 的表头比对全会错位
  （这类后果是**推断**，本机没有为它们单独跑命令，见末尾清单）。
- 拿 `utf-8-sig` 读：**带 BOM 会吃掉，不带 BOM 也正常**。所以读「可能是 Windows 生成的 UTF-8」时
  用 `utf-8-sig` 更稳；**写的时候不要主动写 BOM**（那会让所有 `utf-8-sig` 之外的读者吃到 `\ufeff`）。
- `utf-16` 那行 `b'\xff\xfe-N'` 里 `-` 和 `N` 是**字节被当成 ASCII 打印出来的样子**
  （`0x2d 0x4e` 是「中」的 UTF-16LE 两个字节）—— 这是 repr 的常规把戏，别以为那是字符串内容。

★ **结论（第 7 节）**：看到「首字符莫名其妙多出一个不可见字符」「CSV 表头对不上」，
第一反应就是查 `EF BB BF`。判据：`b[:3] == b'\xef\xbb\xbf'`。

## 8. 本机有哪些 codec：`codecs.lookup` 逐个试，不猜（demo 第 8 节）

别背 codec 列表，直接问本机：

```text
$ python3 b_codecs.py 5
====================================================================
五、本机 codec 名录：逐个 codecs.lookup，不猜
====================================================================
  utf8             → utf-8
  UTF-8            → utf-8
  utf-8            → utf-8
  ascii            → ascii
  latin-1          → iso8859-1
  latin1           → iso8859-1
  iso-8859-1       → iso8859-1
  cp1252           → cp1252
  cp936            → gbk
  gbk              → gbk
  gb2312           → gb2312
  gb18030          → gb18030
  big5             → big5
  shift_jis        → shift_jis
  euc_jp           → euc_jp
  koi8-r           → koi8-r
  utf-16           → utf-16
  utf-16-le        → utf-16-le
  utf-8-sig        → utf-8-sig
  utf-32           → utf-32
  punycode         → punycode
  idna             → idna
  rot13            → rot-13
  hex              → hex
  base64           → base64
  unicode_escape   → unicode-escape
  string_escape    → LookupError: unknown encoding: string_escape
  mbcs             → LookupError: unknown encoding: mbcs
  oem              → LookupError: unknown encoding: oem
  不存在的东西           → LookupError: unknown encoding: 不存在的东西

alias 表里登记的别名条目数：333
→ 'utf8'/'UTF-8' 是同一 codec 的别名；cp936 就是 gbk 的马甲；
  'string_escape'、'mbcs' 是 Python 2 / Windows 的旧事，本机没有。
  中文 codec 靠 C 扩展 _codecs_cn（第 01 章的 lib-dynload 里能看到它）。
[退出码 0]
```

四个要点：

1. **名字不区分大小写，连字符/空格也忽略**：`utf8` / `UTF-8` / `utf-8` 都归到 `utf-8`，
   `latin1` 归到 `iso8859-1`。所以手写的编码名多半能被认出来；认不出来时 `lookup` 会明确告诉你
   它归到了谁 —— 这一列「名字 → 正身」的写法本身就是排错手段。
2. **`cp936` 就是 `gbk`**：Windows 的中文代码页和国标是同一个 codec 的两个名字。
3. **`rot13` / `hex` / `base64` 竟然也是 codec**（历史 API：把 `bytes`→`bytes` 的变换统一成了「编码」）。
   别拿它们当真编码用；`punycode` / `idna` 是域名编码，也在名录里。
4. **Python 2 的 `string_escape`、Windows 的 `mbcs` 本机都没有** —— 网上抄的老代码里那些编码名，
   第一个要怀疑的就是这类。

未注册 codec 的报错原文，单独跑一条：

```text
$ python3 -c "import codecs; codecs.lookup('mbcs')"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    import codecs; codecs.lookup('mbcs')
                   ~~~~~~~~~~~~~^^^^^^^^
LookupError: unknown encoding: mbcs
[退出码 1]
```

注意异常类是 **`LookupError`**，不是 `UnicodeError` 家族 —— 「编码名写错」和「编码过程中出错」
在 Python 的异常树里是两个分支，捕获时要分开写。

★ **结论（第 8 节）**：判断 codec 本机可不可用只有一条可靠办法：`codecs.lookup(名字)` 现场试。
333 条别名、有没有 `_codecs_cn`，都不是靠记忆能定的。

## 9. 切片越界：`s[100]` 抛 `IndexError`，`s[i:j]` 越界却安静给短串（demo 第 9 节）

先把两条命令放在一起看，反差就是本节全部内容：

```text
$ python3 -c "'hello'[100]"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    'hello'[100]
    ~~~~~~~^^^^^
IndexError: string index out of range
[退出码 1]
```

```text
$ python3 -c "print(repr('hello'[2:100]), repr('abc'[10:20]), repr('hello'[100:]), repr('hello'[-100:2]))"
'llo' '' '' 'he'
[退出码 0]
```

四条切片，全都越界，**全都一声不吭**。把两种越界摊开成表：

```text
$ python3 c_slicing.py 1
====================================================================
一、s[i] 越界就抛，s[i:j] 越界只给短串 —— 两种越界两种命运
====================================================================
s = 'hello world'（len=11）
s[4:100]   → 'o world'     ← 终点越界：安静截到结尾
s[100:]    → ''        ← 起点越界：安静给空串
s[4:2]     → ''        ← 起点在终点右边：空串，不报错
s[-100:2]  → 'he'     ← 起点负得过头：夹到 0，正常截
'abc'[10:20] → ''      ← 整段都在串外：还是空串
s[100]  → IndexError: string index out of range   ← 单下标越界就没这么客气
s[-100] → IndexError: string index out of range
→ 对照 C：arr[i] 越界是未定义行为（读到隔壁变量或段错误）；
  Python 把「单下标」变成必抛异常，把「切片」变成自动夹断 —— 两种都安全，
  但脾气不同：切片越界不报错这件事，藏 bug 的本事一流（见 README ★）。
[退出码 0]
```

对照 C：

| 场景 | C | Python `s[i]` | Python `s[i:j]` |
| --- | --- | --- | --- |
| 越界读 | 未定义行为（读到隔壁变量 / 段错误） | `IndexError` | 静默夹断给短串 |
| `i > j` | 取决于实现 | ——（没有这种写法） | 空串，不报错 |
| 负数下标 | 合法但指到数组前面（还是 UB） | 从尾部倒数，`-len..-1` 有效 | 负端点先加 `len` 再夹断 |

`'abc'[10:20]` → `''` 这一条要刻进脑子：**它和「切到的内容正好是空」完全无法区分**。
所以 `parts = line[10:20]` 这种代码，如果 `line` 短了，你拿到的是空串而不是错误；
`if parts:` 只能勉强拦住一半。**更好的做法是把「这里长度应该够」这个假设写成断言**，
别指望切片提醒你。

★ **结论（第 9 节）**：Python 把 C 的「越界 = 灾难」换成了「越界 = 静默夹断或异常」，安全多了；
代价是**切片越界不会告诉你任何事**。

## 10. 负索引、步长、`slice` 对象；切片赋值：list 行、str 不行（demo 第 10 节）

```text
$ python3 c_slicing.py 2
====================================================================
二、负索引、步长、slice 对象
====================================================================
s[-1]      → 'd'      s[-5:] → 'world'
s[-1] == s[len(s)-1] → True
s[::2]     → 'hlowrd'    ← 步长 2：隔一个取一个
s[::-1]    → 'dlrow olleh'  ← 步长 -1：整串反转
s[::-2]    → 'olh'（对 'hello'）  s[5:0:-2] → ' le'  s[:0:-1] → 'dlrow olle'
→ 负步长时省略的默认端点会互换（起点=尾、终点=头），所以 [:0:-1] 不含 s[0]。

slice 对象就是三个数打包：slice(1, 8, 2)
s[sl] == s[1:8:2] → True（冒号切片 = 构造 slice 再传进去）
sl.indices(11) → (1, 8, 2) ← 把越界/负数夹成合法三元组，切片内部就干这个
slice(-4).indices(11) → (0, 7, 1)

三、切片能不能赋值：list 能，str 不能
list 切片赋值 [1,2,3,4] 的 [1:3] ← ['a','b'] → [1, 'a', 'b', 4]（长度可变，左边右边不必等长）
零宽切片赋值（lst[1:1]=[x]）→ [1, '插', 'a', 'b', 4] ← 这就是「在索引 1 处插入」
赋空列表 = 删除 → ['a', 'b', 4]
str 切片赋值 → TypeError: 'str' object does not support item assignment
→ 呼应第 02 章：str 不可变。想「改」只能重建：'hXllo world'
[退出码 0]
```

四个值得单独记的点：

1. **`s[::-1]` 反转**是最常见的切片惯用法，C 里没有对应物（C 得自己写交换循环）。
2. **负步长时省略的默认端点会互换**：`s[:0:-1]` 是「从尾开始，走到索引 0 **之前**（不含 0）」，
   所以少了第一个字符；`s[5:0:-2]` → `' le'` 同理（终点 0 取不到）。
   `s[::-1]` 之所以能反转整个串，正是因为两端都省略、互换后变成「尾 → 头」。
3. **`slice` 是一等对象**：`s[1:8:2]` 就是 `s[slice(1, 8, 2)]`。
   `slice.indices(n)` 把「越界 / 负数」统一夹成合法三元组 —— `slice(-4).indices(11)` → `(0, 7, 1)`。
   这就是上一节「静默夹断」的实现机制。**想搞懂任何一条切片结果，`indices()` 现场算一遍即可**；
   真实用途是把常用切法存成常量复用（`TAIL = slice(-4, None)`）。
4. **切片能读不能写（对 `str`）**。原始 `TypeError` 单独跑一条：

```text
$ python3 -c "s = \"hello\"; s[1:3] = \"X\""
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    s = "hello"; s[1:3] = "X"
                 ~^^^^^
TypeError: 'str' object does not support item assignment
[退出码 1]
```

`list` 就没这个限制：`lst[1:3] = ['a','b']` 能换，`lst[1:1] = ['插']` 是插入，
`lst[1:3] = []` 是删除，而且**左右长度不必相等**（这是 list 和 str 的第二个根本差别，
第一个是可变性）。想「改」字符串只能重建整串。

★ **结论（第 10 节）**：`str` 不能原地改不是 Python 小气，是第 02 章「不可变对象」的必然结果 ——
不可变对象才能被共用（驻留、当 dict 键、跨线程传），一旦允许原地改，这些全塌。
要「可变的文本」，用 `list` 收集再 `join`（第 16、19 节）。

## 11. f-string：`{x=}` 调试语法、`!r` / `!s` / `!a`、花括号转义（demo 第 11 节）

```text
$ python3 d_fstring.py 1
====================================================================
一、调试语法 {x=}、转换 !r/!s/!a、花括号转义 {{ }}
====================================================================
f"{NAME}, {x=}"                        → '世界, x=42'  ← 等号=名字加值的 repr，调试神器
f"{x = }"（空格也保留）                       → 'x = 42'
f"{x=:.2f}"（等号后还能接格式）                  → 'x=42.00'
f"{DT.year=}, {DT.month=}"             → 'DT.year=2026, DT.month=10'  ← 左边整个是表达式
f"{1.5!s}"                             → '1.5'  ← !s 强制 str()
f"{' 文本 '!r}"                          → "' 文本 '"  ← !r 强制 repr()，空格现形
f"{'中'!a}"                             → "'\\u4e2d'"  ← !a 用 ascii()，非 ASCII 变 \uXXXX
f"{{{x}}}"（花括号双写）                      → '{42}'  ← 想输出 {42} 就这么写
→ 坑：f-string 里的字面花括号必须成对双写，单个 { 直接 SyntaxError。
[退出码 0]
```

三件有牙齿的东西：

- **`{x=}`（3.8+）等于 `f"x={x!r}"`，但名字是它自己认出来的。** 调试时少打一半字，
  而且 `f"{DT.year=}"` 说明 `=` 左边可以是**整个表达式**（属性、下标、调用都行）。
  `f"{x = }"` 里的空格会保留；`f"{x=:.2f}"` 说明 `=` 之后还能接格式规范。
- **`!r` / `!s` / `!a` 是三种转换**：`!r` 用 `repr`（空格、引号、不可见字符全部现形，
  调字符串问题时最有用）；`!s` 强制 `str`（默认就是它，写给读者看时用来抵消自定义 `__format__`）；
  `!a` 用 `ascii`（`'中'` → `'\u4e2d'`，日志要求纯 ASCII 时用它）。
- **字面花括号必须成对双写**：`f"{{{x}}}"` → `'{42}'`。三个 `{`：两个成对打出字面 `{`，
  一个开表达式。单个未配对的 `{` 直接 `SyntaxError`。

★ **结论（第 11 节）**：把 `{x=}` 当成默认调试打印，把 `!r` 当成「让空格和引号现形」的开关。
要拼 JSON / 模板这类含 `{}` 的文本，别硬用 f-string（双写极易错），改用 `.format_map` 或普通 `join`。

（上一块最后那句「单个 `{` 直接 `SyntaxError`」是 demo 打的**旁白**，不是跑出来的：
本机没有为它单独跑一条命令，所以那句按「未演」处理，见末尾清单。）

## 12. f-string 格式规范：填充 / 对齐 / 宽度 / 千分位 / 精度 / 符号 / 进制（demo 第 12 节）

冒号后面那串的完整语法是 **`[[fill]align][sign][#][0][width][,][.precision][type]`**。本机逐条实测：

```text
$ python3 d_fstring.py 2
====================================================================
二、格式规范速写：填充/对齐/宽度/千分位/精度/符号/进制
====================================================================
f"{'ab':*^10}"                         → '****ab****'  ← 填充字符 * + 居中 ^
f"{3.14159:>10.2f}"                    → '      3.14'  ← 右对齐宽 10，两位小数
f"{-3.14159:<+8.1f}"                   → '-3.1    '  ← 左对齐 + 强制符号
f"{1234567:,}"                         → '1,234,567'  ← 千分位
f"{1234567.5:,.2f}"                    → '1,234,567.50'  ← 千分位 + 精度
f"{42:b}" / f"{42:#010b}"              → '101010 / 0b00101010'  ← 二进制；# 带 0b 前缀
f"{255:#x}"                            → '0xff'  ← 十六进制带前缀
f"{0.256:.1%}"                         → '25.6%'  ← 百分数
f"{65:c}"                              → 'A'  ← 整数按码点出字符
f"{-42:+d}  f"{0:+d}"                  → '-42  +0'  ← 符号位
f"{-42:=05d}"                          → '-0042'  ← = 把符号留在最左、零填充跟在后面
f"{'x':>{w}}"（宽度现算）                    → '       x'
f"{'中':>6}A"                           → '     中A'  ← 宽度按【码点数】补，不是显示格数！
整数给小数精度 → ValueError: Precision not allowed in integer format specifier
→ 记忆：[[fill]align][sign][#][0][width][,][.precision][type]
[退出码 0]
```

怎么读这几条：

- `*^10` = 填充字符 `*` + 对齐 `^`（居中）+ 宽度 10。对齐只有 `<`（左）`>`（右）`^`（中）三种；
  **填字符写在对齐符前面**，`{:>10}` 不写 fill 时 fill 默认是空格。
- `.2f` 是精度，类型可以是 `b/x/o/c/d/s/%` 等；`#` 表示「带前缀」（`0x` / `0b`）。
- `=` 是最容易被忽略的一个：`f"{-42:=05d}"` → `-0042`，含义是「符号留在最左，零填充跟在符号后面」。
  这正好是 C 里 `printf("%05d")` 给你的样子。`{:05d}`（不带 `=`）的等价写法本章**没进捕获**，
  所以不在这儿下结论，记进末尾清单。
- **宽度可以运行时现算**：`f"{'x':>{w}}"`（`w` 是个变量）。按显示宽度对齐中文时必须要它（第 22 节）。
- `f"{'中':>6}A"` → `'     中A'`：**6 个字符宽 = 5 个空格 + 1 个汉字，可那个汉字在终端里占 2 格**。
  格式化根本不知道「格」这个概念 —— 第 22 节展开。
- 精度给整数会当场翻脸：`ValueError: Precision not allowed in integer format specifier`。

★ **结论（第 12 节）**：格式规范这段符号**位置敏感**，别背，要抄。
只要涉及对齐，先问一句「这里的宽度是码点数还是显示格数」 —— 第 22 节给了算显示格数的写法。

## 13. 嵌套引号在 3.14 上到底能怎么写（demo 第 13 节）

网上教程（3.11 时代写的）会说「f-string 里不能有反斜杠、不能复用同种引号」。**这条已经过期了**
（PEP 701，Python 3.12 起）。本机 3.14.4 六条全过：

```text
$ python3 d_fstring.py 3
====================================================================
三、嵌套引号在 3.14 上的实测（这台机器跑出来的，不是文档抄的）
====================================================================
f'{d["k"]}'（老规矩：外单内双）                  → 'v'
f"{d["k"]}"（同种引号复用）                    → 'v'  ← 3.12+ PEP 701，本机通过
f"{d["名字"]}"                           → '张三'  ← 中文键也照样
f"{chr(10).join(["a", "b"])}"（表达式带反斜杠） → 'a\nb'  ← 3.12+ 允许表达式里出现 \n
f"{f"内层{x}外"}"（f 套 f）                  → '内层42外'
f"{"a" if x else "b"}"（引号直接复用）         → 'a'
三引号包裹、内部再嵌 f 的多行写法：
2026 年 10 月 03 日  ← 这一行是三引号 f 串打出来的
第二行照打。
→ 结论：本机 3.14.4 上面六条全部能跑；3.11 及以前只有第一条能活。
  旧教程说「f-string 里绝对不能同种引号/反斜杠」——那是 3.11 的老规矩。
[退出码 0]
```

看第四条：表达式里出现了 `["a", "b"]`（和外面同种的双引号），而且用 `chr(10)` 造出换行 ——
这是 PEP 701（Python 3.12 起）把 f-string 交给**真正的递归解析器**之后的效果。

但**兼容性地雷还在**：上一块最后那行「3.11 及以前只有第一条能活」是 demo 打的旁白，
本机**没有旧解释器可验**（进末尾清单）。能确定的是「本机 3.14 全过」这一半。所以：

- 只在「明确跑 3.12+」的项目里复用引号；
- 库代码 / 要给别人跑的脚本，仍然写 `f'{d["k"]}'`（外单内双）—— 这条**在所有版本都对**；
- 「3.11 及以前的具体报错文本」本机没有旧解释器，**没验证**，进末尾清单。

★ **结论（第 13 节）**：f-string 的引号限制是**版本相关的实现细节**，不是语言规则。
别把老教程的禁令当真理，也别把新写法发到旧解释器上。

## 14. datetime 格式串；`strftime` 是本章唯一和 locale 有关的事（demo 第 14 节）

```text
$ python3 d_fstring.py 4
====================================================================
四、datetime 格式串；strftime 是唯一跟 locale 纠缠的地方
====================================================================
f"{DT:%Y-%m-%d %H:%M:%S}"              → '2026-10-03 14:05:09'
f"{DT:%F %T}"                          → '2026-10-03 14:05:09'  ← %F/%T 是 ISO 速记
f"{DT:%A %B %d}"                       → 'Saturday October 03'  ← LC_TIME=C → 英文星期月名
f"{DT:%j} 天 / 第 %U 周"                  → '276 天 / 第 39 周'
DT.isoformat()                         → '2026-10-03T14:05:09'  ← 想要机器可读就这个
—— 现场换 locale 再打同一张串 ——
不调 setlocale：Saturday October 03
setlocale(LC_TIME, "zh_CN.UTF-8") 后：星期六 十月 03
设回 C：Saturday October 03
→ 关键实测：外部环境变量 LC_ALL=zh_CN.UTF-8 起进程时 %A 仍是英文；
  Python 只把 LC_CTYPE 交给自己管，LC_TIME 默认停在 C，要代码里 setlocale。
  而 str.upper/casefold 完全不理会 locale（第 21 节现场对照）。
[退出码 0]
```

f-string 里 `%Y-%m-%d` 这种**直接写在冒号后面**（`f"{DT:%Y-%m-%d}"`）就是走 `strftime`，
不用 `DT.strftime(...)` 再套一层。

然后是那条会让人困惑很久的实测：**外部环境变量给成中文 locale，`%A` 照样是英文**：

```text
$ LC_ALL=zh_CN.UTF-8 python3 -c "from datetime import datetime; print(datetime(2026, 10, 3, 14, 5, 9).strftime('%A %d %B'))"
Saturday 03 October
[退出码 0]
```

而在代码里显式 `locale.setlocale(locale.LC_TIME, "zh_CN.UTF-8")` 之后才变成 `星期六`（上一块里那行）。

为什么？CPython 启动时只把 `LC_CTYPE` 这一类设进 C 运行时（为了让编码 / 字符判定正常工作），
`LC_TIME`、`LC_NUMERIC`、`LC_COLLATE` 停在 `"C"`。所以：

- 想让日期变中文 → **代码里 `setlocale(LC_TIME, ...)`**，只改环境变量不管用；
- `setlocale` 是**进程全局**的，会影响同进程里别的库的 C 调用 —— 别在库代码里随手调；
- 只是想要「中文日期格式」，更稳的做法是自己拼（`f"{DT.year}年{DT.month}月"`），不碰 locale。

★ **结论（第 14 节）**：**本章唯一一个 locale 真的掺和进来的地方就是 `strftime`**。
`str` 的方法（大小写、`is*`、切片）一律不理会 locale —— 下一节现场对照。

## 15. `str.format` / `%` / 加号 / `join` 同台对比（demo 第 15 节）

```text
$ python3 d_fstring.py 5
====================================================================
五、str.format、%、加号、join 同台
====================================================================
f-string（日常首选）：张三 得了 42 分
str.format：张三 得了 42 分
str.format：张三-42-张三
str.format：张三-42
format_map：张三-42
百分号老式写法：张三 得了 42 分
百分号配字典：张三 的分数 42
百分号里打一个字面 % 要写两个（这行的结尾就是）：% → %
加号拼接：ab3（数字必须手动 str()）
join 拼接：春眠，不觉晓 ← 分隔符在前，可迭代对象在后
→ 选型：日常 f-string；模板来自配置/用户的字符串才用 .format/format_map；
  % 只在老代码里认得它就行；成批拼接用 join，别用 +=（第 16 节量给你看）。
[退出码 0]
```

（输出里那三行 `str.format：…` 是同一套格式规范的三种喂法：位置参数、重复用同一个参数
（`{0}-{1}-{0}`）、关键字参数；`format_map` 那行直接吃了一个 dict。）

五条路各自的真实定位：

| 写法 | 什么时候必须用它 | 坑（本机实测到的） |
| --- | --- | --- |
| `f"...{x}"` | 日常一切：值就在作用域里、当场就要打 | 模板来自外部（配置 / 用户输入）时**不能用** —— f-string 在写下它的那一刻就求值了 |
| `tpl.format(a=1)` / `tpl.format_map(d)` | 模板是一个**字符串对象**（从配置、数据库来的） | 模板里出现的名字必须都传进去，少传会抛异常（异常原文本章没演）；`format_map` 直接吃 dict 最省事 |
| `"%s" % x` | 老代码里认得它即可 | 字面 `%` 要写成 `%%`（输出里那行就是演示）；右边必须是元组/字典，参数个数对不上会抛异常（原文没演）|
| `'sep'.join(seq)` | 成批拼接 | 分隔符在**前面**；只吃 `str` 序列（第 19 节报错原文） |
| `a + b` | 两三个片段 | 数字不会自动转（那行 `'ab' + 3` 必须先 `str(3)`）；循环里用是 O(n²)（第 16 节） |

★ **结论（第 15 节）**：**格式化模板如果来自外部，绝不能用 f-string**（要 `.format` / `format_map`）。
其余场景一律 f-string，理由是好读，不是快 —— 下一节把「快」这个借口量掉。

## 16. 循环里 `+=` 拼串的代价：timeit 实测（demo 第 16 节）

数字每次都会抖，所以每条后面都抄了 demo 自己打的 `[形状]` 行（所有数字换成 `N`，形状稳定）。
两边 setup 里造的是**同样 1000 个词**，只换拼接方式：

```text
$ python3 -m timeit -s "
words = ['词%d' % i for i in range(1000)]
def plus():
    out = ''
    for w in words:
        out += w
    return out
" "plus()"
10000 loops, best of 5: 27 usec per loop
[退出码 0]
[形状] N loops, best of N: N usec per loop
```

```text
$ python3 -m timeit -s "words = ['词%d' % i for i in range(1000)]" "''.join(words)"
50000 loops, best of 5: 5.44 usec per loop
[退出码 0]
[形状] N loops, best of N: N usec per loop
```

同样 1000 个词，这次 `join` 快 5 倍上下（27 vs 5.44 usec）。原因不是「Python 慢」，是算法：
`str` 不可变 → `out += w` 每次都**新建整串并逐字节复制**，n 轮下来是 O(n²)；
`''.join(words)` 先扫一遍求总长、一次分配、再一次填，O(n)。
第 02 章已经从语义上量过 `+=` 对 `str` 是「造新对象再贴标签」。实现里可能还有快路径，
但那是**实现细节、不可依赖**（本章没演），你的代码不该把性能押在它上面。

再量一下单次格式化的开销：

```text
$ python3 -m timeit -s "a, b = '张三', 42" "f'{a}-{b}'"
5000000 loops, best of 5: 62.3 nsec per loop
[退出码 0]
[形状] N loops, best of N: N nsec per loop
```

```text
$ python3 -m timeit -s "a, b = '张三', 42" "'{}-{}'.format(a, b)"
5000000 loops, best of 5: 92.1 nsec per loop
[退出码 0]
[形状] N loops, best of N: N nsec per loop
```

f-string 比 `.format` 快三分之一上下（这一趟 62.3 vs 92.1 nsec），但绝对差是**三十纳秒不到** —— 所以「用 f-string 因为快」
是个可笑的理由，**用 f-string 因为它好读**。第 15 节那条「外部模板只能用 `.format`」的结论，
速度上完全付得起。

★ **结论（第 16 节）**：**循环里拼串一律先 `append` 进 list，最后 `'sep'.join(parts)`**。
这是本章唯一有数量级代价的选型。（计时绝对值每次会抖，本章只引形状，不把数字当规则。）

## 17. `split` 家族的真脾气：`split()` vs `split(' ')`、`maxsplit`、`rsplit`、`partition`（demo 第 17 节）

```text
$ python3 e_split_join.py 1
====================================================================
一、split() 与 split(' ')：不是一回事
====================================================================
s = '  a  b\tc\nd '
s.split()   → ['a', 'b', 'c', 'd']
s.split(' ') → ['', '', 'a', '', 'b\tc\nd', '']
→ 无参 split()：把【所有连续空白】当一个分隔符，还扔掉首尾空白，结果无空串；
  split(' ')：只按【单个空格字符】切，连续空格/制表符/换行都照切，空串照留。
''  .split()   → []   ''.split(' ') → ['']
' '.split()    → []    ' '.split(' ') → ['', '']
  （空串 split() 给 []；split(' ') 给 ['']——差一个空元素，代码会崩在这）
全角空格也算空白：'a\u3000b'.split() → ['a', 'b']，split(' ') → ['a\u3000b']
split('') → ValueError: empty separator
[退出码 0]
```

（上面那行 `'a\u3000b'` 是 `repr()` 的转义形态，**它本身就是捕获里的原文**，逐字未改；
读的时候记住：U+3000 是全角空格，肉眼看着像空格，`split()` 认它、`split(' ')` 不认。）

这一节有**两条能直接崩代码**的差异：

1. **空串的命运**：`''.split()` → `[]`（长度 0），`''.split(' ')` → `['']`（长度 1）。
   同样一个空输入，两种切法给出的**列表长度不一样**。所以任何「切完按固定名字解包」
   （`a, b = s.split(sep)`）或「取 `parts[1]`」的代码，都会在空输入这个边界上崩，
   而崩的形式取决于切法 —— 解包失败的报错文本本章没演，见末尾清单。
2. **`split()` 认的是 Unicode 空白**：不只 `' ' \t \n \r \v \f`，连**全角空格 U+3000** 也算，
   而 `split(' ')` 认不出它。C 的 `isspace` 只认 ASCII 那几个 ——
   这是 Python 走 Unicode 表的结果，**与 locale 无关**。

`split('')` 的报错原文单独跑一条：

```text
$ python3 -c "'abc'.split('')"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    'abc'.split('')
    ~~~~~~~~~~~^^^^
ValueError: empty separator
[退出码 1]
```

（想「按每个字符」切，用 `list(s)`，不是 `split('')`。）

限量、从右切，以及「只切一刀」的专用工具：

```text
$ python3 e_split_join.py 2
====================================================================
二、maxsplit、rsplit、partition/rpartition
====================================================================
'a-b-c-d'.split('-')    → ['a', 'b', 'c', 'd']
'a-b-c-d'.split('-', 2) → ['a', 'b', 'c-d']   ← 最多切 2 刀，右边不再切
'a-b-c-d'.rsplit('-', 2) → ['a-b', 'c', 'd']  ← 从右边数 2 刀
'a-b-c-d'.split('-', -1) → ['a', 'b', 'c', 'd']   ← 负数=不限

'a=b=c'.partition('=')    → ('a', '=', 'b=c')  ← 只切第一刀，三段定死
'a=b=c'.rpartition('=')   → ('a=b', '=', 'c') ← 只切最后一刀
'abc'.partition('=')     → ('abc', '', '') ← 找不到时不是异常，是原串+两个空
三元组直接解包：k='a' sep='=' v='b=c'
→ 取「键=值」用 partition 比 split 稳：结果长度永远是 3，不会 IndexError。
[退出码 0]
```

**`partition` 是解析 `key=value` / 邮件头 / 文件名后缀的正确工具**：
`split('=', 1)` 在**没有 `=`** 时只给 1 个元素，解包成 3 个就崩；
`partition` 永远给 3 段（找不到就是 `('原串', '', '')`），**长度可预测**。
`split(sep, maxsplit)` 的 `maxsplit` 给**负数表示不限**；`rsplit` 的方向只在有 `maxsplit` 时才有效果
（无 `maxsplit` 时 `split` 和 `rsplit` 结果相同）。

split 家族速查：

| 方法 | 分隔符 | 返回 | 找不到分隔符时 | 典型用途 |
| --- | --- | --- | --- | --- |
| `s.split()` | 所有连续空白（含全角空格） | 无空串的列表 | ——（本来就不按分隔符找） | 按词切、按「空白」切 |
| `s.split(sep)` | 精确的 `sep`，可留空串 | 列表（可能含 `''`） | 单元素列表 `[s]` | 固定格式：CSV 行、参数串 |
| `s.split(sep, n)` | 同上，最多 n 刀 | 最多 n+1 段 | 同上 | 只切第一个字段 |
| `s.rsplit(sep, n)` | 从右数 n 刀 | 同上 | 同上 | 取最后一个字段（`a.b.c` 取 `c`） |
| `s.partition(sep)` | 精确 `sep` | **恒 3 元组** | `(s, '', '')` | `key=value`、URL query |
| `s.splitlines()` | 11 种「换行」 | 列表 | —— | 按行处理（第 18 节） |

★ **结论（第 17 节）**：默认 `s.split()`（按空白、不吃空串）；按**固定分隔符**才写 `s.split(sep)`；
`key=value` 一律 `partition`。判断该用哪个的问题只有一个：「输入里可能有连续空白 / 首尾空白吗」。

## 18. `splitlines` 认哪些「换行」：11 种逐个实测（demo 第 18 节）

按行切这件事，Python 认的「换行」比你想的多得多。逐个实测
（每行左边是 `splitlines()`，右边是 `split('\n')`，都作用在 `'A' + 分隔符 + 'B'` 上）：

```text
$ python3 e_split_join.py 3
====================================================================
三、splitlines 认哪些分隔符（split('\n') 只认一种）
====================================================================
名字           两列 = splitlines / split('\n')（都作用在 A+分隔符+B 上）
LF           ['A', 'B'] / ['A', 'B']
CR           ['A', 'B'] / ['A\rB']
CRLF         ['A', 'B'] / ['A\r', 'B']
VT 0x0b      ['A', 'B'] / ['A\x0bB']
FF 0x0c      ['A', 'B'] / ['A\x0cB']
FS 0x1c      ['A', 'B'] / ['A\x1cB']
GS 0x1d      ['A', 'B'] / ['A\x1dB']
RS 0x1e      ['A', 'B'] / ['A\x1eB']
NEL 0x85     ['A', 'B'] / ['A\x85B']
LSEP U+2028  ['A', 'B'] / ['A\u2028B']
PSEP U+2029  ['A', 'B'] / ['A\u2029B']
→ splitlines 一口气认 11 种「换行」；split('\n') 只认 LF，CR 老 Mac 风格照吞。
'a\r\nb'.splitlines(keepends=True) → ['a\r\n', 'b']
'a\n'.splitlines() → ['a']  'a\n'.split('\n') → ['a', '']  ← 尾部空元素差别
'\n'.splitlines() → ['']   ''.splitlines() → []
→ 提醒：splitlines 不收分隔符参数，也没有 maxsplit（想要限量就 split）。
[退出码 0]
```

（这两行右侧的 `\u2028` / `\u2029` 是 `repr()` 打出来的转义形态，捕获里就是这几个字符，逐字未改。
判据看左列：**splitlines 切开了**，`split('\n')` 没切开 —— 这正是本节的重点。）

三个现实后果：

- **`\x0b`（垂直制表）、`\x0c`（换页）也算换行**。从 PDF / 老式报表里抠出来的文本，
  `splitlines()` 会切得比你预期碎；只按 LF 切要用 `split('\n')`。
- **`U+2028`（行分隔符）、`U+2029`（段分隔符）、`U+0085`（NEL）也算**。
  前两个是 JS / JSON 里的经典坑（它们在 JS 里也是行终止符）。所以「把用户文本按行处理」之前
  要明确自己认几种换行 —— 不确认就用 `splitlines()`（认全），或者自己先 `replace` 归一。
- **尾部空元素行为不同**：`'a\n'.splitlines()` → `['a']`（不多给你空行），
  `'a\n'.split('\n')` → `['a', '']`；`'\n'.splitlines()` → `['']`（一个空行），
  `''.splitlines()` → `[]`（什么都没有）。这一组差别直接决定「空文件」和
  「只有一个换行的文件」在你的循环里各跑几轮。
- `keepends=True` 保留分隔符，**而且是原样保留**：`['a\r\n', 'b']` 里那个 `\r\n` 不会被拆。
  想统一 / 保留行尾就靠它。

★ **结论（第 18 节）**：「按行切」有两个不同的答案：`splitlines()`（认 11 种、不吃尾部空行）和
`split('\n')`（只认 LF、会留尾部空串）。混用就是「同一份文本我数 3 行你数 5 行」这种 bug 的来源。

## 19. `join` 的脾气与「split 再 join 不还原」最小反例（demo 第 19 节）

```text
$ python3 e_split_join.py 4
====================================================================
四、join 的脾气，和「split 再 join 不还原」
====================================================================
','.join([])      → ''     ← 空列表给空串，不报错
','.join(['x'])    → 'x'         ← 单元素不加尾巴逗号
'-'.join('abc')     → 'a-b-c'   ← 分隔符在前！把 'abc' 当可迭代对象拆开了
','.join([1, 2])  → TypeError: sequence item 0: expected str instance, int found
补救：'1,2' 或 map(str, ...)

不还原的最小反例：
原文 'a  b' → split() → ['a', 'b'] → ' '.join → 'a b' ≠ 原文（两个空格塌成一个）
原文 'a\r\nb' → splitlines() → '\n'.join → 'a\nb' ≠ 原文（CR 被没收）
原文 ' a ' → split() → ['a'] → ''.join → 'a' ≠ 原文（首尾空格和分隔全没了）
→ 想「拆开还能装回去」：用 split(同一个 sep) + sep.join；split()/splitlines() 是丢信息的。
对照：'x,y'.split(',') 再 ','.join → 'x,y'（无损）
[退出码 0]
```

`join` 最反 C 直觉的一点：**分隔符是调用者，序列是参数** —— `'-'.join('abc')` → `'a-b-c'`。
所以「对一个字符串做 join」会把它逐字符拆开（`str` 本身就是可迭代对象，第 23 节再证一次）。

`TypeError` 原文值得整段记住，因为它连**第几项**出错都告诉你：

```text
$ python3 -c "','.join([1, 2])"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    ','.join([1, 2])
    ~~~~~~~~^^^^^^^^
TypeError: sequence item 0: expected str instance, int found
[退出码 1]
```

补救永远是 `','.join(map(str, xs))`。

本节真正的教训是那三个反例：**`split` 和 `join` 不是互逆运算**（除非两边都用同一个显式 `sep`）。
`split()`（无参）和 `splitlines()` 都是**有损**的：连续空白塌成一个、`\r` 消失、首尾空白扔掉 ——
信息在切的那一刻就没了。所以任何「拆开处理完再拼回原文」的需求（格式化器、配置写回、代码改写）
必须 `split(sep)` + `sep.join(...)`，或者自己管住分隔符。

★ **结论（第 19 节）**：`split` ↔ `join` 只有在**两边 `sep` 相同、且 `sep` 是显式给出的**时候可逆。
「round-trip 不还原」是本章最容易静默损坏文本的一条，排在坑清单前列。

## 20. `strip` 删的是字符集合，不是前缀；`removeprefix` / `removesuffix` 才是砍前后缀（demo 第 20 节）

C 里没有「按集合从两端剥字符」的原语，所以从 C 过来的人几乎 100% 会把 `strip('xy')` 读成
「去掉前缀 `xy`」。它不是：

```text
$ python3 f_strip_case.py 1
====================================================================
一、strip(带参数) 删的是「字符集合」，不是前缀
====================================================================
'xyfooxy'.strip('xy')                    → 'foo'
'mississippi'.strip('isp')               → 'm'  ← 经典：几乎全被吃光
'banana'.strip('ban')                    → ''  ← 空串！每个字母都在集合里
'banana'.strip('bn')                     → 'anana'  ← 左端 b 进集合啃一个，右端 a 不进就停
'  hi \n'.strip()                        → 'hi'  ← 无参=删所有空白
'\t a \t'.lstrip()                       → 'a \t'  ← lstrip 只管左端
→ 规则：从两端往中间啃，只要【当前字符】落在参数集合里就啃掉，
  碰到第一个不在集合里的就停；两端各自独立。所以它没有「前缀」概念。

真要砍前缀/后缀：removeprefix / removesuffix（3.9+，整段匹配或原样返还）
'abcabc'.removeprefix('abc')             → 'abc'
'abcabc'.removeprefix('xyz')             → 'abcabc'  ← 不匹配→原样，不报错
'notes.txt'.removesuffix('.txt')         → 'notes'
'XfooX'.removeprefix('X')                → 'fooX'  ← 只动左端一个
'XfooX'.removesuffix('X')                → 'Xfoo'  ← 只动右端一个
[退出码 0]
```

`'mississippi'.strip('isp')` → `'m'` 是本节的高光：后面那一串字母全都在集合 `{i, s, p}` 里，
于是从左数第二个字符开始一路啃到底，只剩一个 `m`。**结果和「前缀 / 后缀」的意思毫无关系**。

再看 `'banana'.strip('ban')` → `''`（每个字母都在集合里，整根香蕉被啃光）和
`'banana'.strip('bn')` → `'anana'`（左端 `b` 在集合里，啃掉一个；下一个 `a` 不在，停；
右端 `a` 一开头就不在集合里，什么都不动）—— 这两条合起来证明了规则的全部三个要点：
**按字符判定、两端各自独立、不是按串匹配**。

而 `removeprefix` / `removesuffix`（3.9+）才是你想要的原语：**整段匹配才砍**，
不匹配就**原样返回、不报错**（比 `if s.startswith(p): s = s[len(p):]` 少两行且不会写错）。
注意它们**只砍一次**、而且**只砍一头**：`'XfooX'.removeprefix('X')` → `'fooX'`，
右端那个 `X` 不动；`removesuffix` 反之。

`strip` 家族全家福：

| 你想干什么 | 正确写法 | 常见错写 |
| --- | --- | --- |
| 去掉首尾空白 | `s.strip()` | `s.strip(' \n')`（漏了 `\t\r\v\f` 和全角空格） |
| 去掉固定前缀 | `s.removeprefix('pre_')` | `s.strip('pre_')`（会把正文里的 p/r/e/_ 全啃掉） |
| 去掉固定后缀 | `s.removesuffix('.txt')` | `s.split('.')[0]`（名字里带点就错，见第 17 节） |
| 只去左端 | `s.lstrip()` / `s.removeprefix(p)` | —— |
| 去掉两端的所有引号 | `s.strip('"\'')` | `removeprefix('"').removesuffix('"')`（不配对的也砍了） |
| 去掉缩进 | `s.lstrip()` 或 `textwrap.dedent`（本章没演） | `strip()`（连尾也去了） |

★ **结论（第 20 节）**：看到 `strip(` 后面带参数，脑子里立刻换成「**字符集合**」四个字。
凡「去掉这个前缀 / 后缀」的语义，一律 `removeprefix` / `removesuffix`。

## 21. 大小写（ß→SS、土耳其 İ）与 `isalpha` / `isdigit` / `isnumeric` 判定表（demo 第 21 节）

### 21.1 大小写：长度会变、一对多、locale 完全不掺和

```text
$ python3 f_strip_case.py 2
====================================================================
二、大小写：长度会变、一对多、locale 完全不掺和
====================================================================
'ß'.upper()                              → 'SS'  ← 一个字符变两个：len 1 → 2
'Straße'.upper()                         → 'STRASSE'
'ß'.casefold()                           → 'ss'  ← casefold 才是给「不分大小写比较」用的
U+0130 'İ'.lower()                       → 'i̇'  ← len=2，展开成 i + 组合上句点
    拆开看码点：['0x69', '0x307'], 第二个的名字：COMBINING DOT ABOVE
'ı'(小写无点 i).upper()                      → 'I'  ← 变 ASCII I —— 土耳其人看了摇头
'ﬀ'(连字).upper()                          → 'FF'  ← 一个码点变两个
'hello world'.title()                    → 'Hello World'
"they're ok".title()                     → "They'Re Ok"  ← 撇号后也大写：不是你要的别用 title
'hello world'.capitalize()               → 'Hello world'  ← 只首字母大写

不分大小写比较/排序的正确姿势：
sorted(['Banana', 'apple'])              → ['Banana', 'apple']  ← 默认按码点：大写在前
sorted(['Banana', 'apple'], key=str.casefold) → ['apple', 'Banana']
'ß'.casefold() == 'ss'                   → True
→ 上面这些都与 locale 无关。locale 掺和的是 strftime 那类（第 14 节）。
[退出码 0]
```

三条要点：

1. **大小写会改变长度**：`ß` → `SS`（len 1 → 2）、`İ`（U+0130）→ `i` + `U+0307`（len 1 → 2）、
   `ﬀ` → `FF`。所以「先 `upper()` 再限长」「`upper()` 之后再 `==`」都可能踩坑；
   第 2 节那条「`len` 是码点数」在这里再兑现一次 —— `İ.lower()` 的两个码点里那个 `U+0307`
   是**组合记号**，人眼看着还是一个字符。
2. **`casefold` 才是「不分大小写比较」该用的**：捕获里 `'ß'.casefold()` → `'ss'`、
   `'ß'.casefold() == 'ss'` → `True`（`'ß'.lower()` 的结果本章**没跑**，见末尾清单）。
   排序：`sorted(xs, key=str.casefold)` —— 默认 `sorted` 按码点，
   所以 `'Banana'` 排在 `'apple'` 前面（C 的 `strcoll` 会按 locale 词典序，Python 默认不会）。
3. **土耳其语的 i 问题，Python 不解决**：`'ı'`（无点小写 i）`.upper()` → ASCII `'I'`，
   而土耳其语正字法要求 `i/İ` 与 `ı/I` 两套配对。想要 locale 敏感的 casing，标准库没有
   （第 2 条里「与 locale 无关」在这里就是代价）。第三方方案本章没演。

现场对照「locale 到底影不影响 `str` 的大小写方法」—— 同一个表达式跑两遍，只有 locale 不同：

```text
$ LC_ALL=zh_CN.UTF-8 python3 -c "print('ß'.upper(), 'ß'.casefold(), len('ß'.upper()))"
SS ss 2
[退出码 0]
```

```text
$ python3 -c "print('ß'.upper(), 'ß'.casefold(), len('ß'.upper()))"
SS ss 2
[退出码 0]
```

两条输出**逐字相同**（都是 `SS ss 2`）。对照第 14 节 `strftime` 的表现，本章关于 locale 的核心结论就是：

> **`str` 的方法一律走 Unicode 表，不理会 locale；只有 `strftime` / `strcoll` 那一类
> `locale` 模块函数才看 locale，而且必须在代码里 `setlocale`。**

### 21.2 `isalpha` / `isdigit` / `isnumeric`：汉字、全角、上标、分数的判定

```text
$ python3 f_strip_case.py 3
====================================================================
三、isalpha / isdigit / isnumeric：汉字、全角、上标、分数的判定
====================================================================
字符   说明          isalpha  isdigit  isnumeric  isalnum isidentifier
中    汉字             True    False      False     True         True
a    ASCII 字母       True    False      False     True         True
１    全角数字 1        False     True       True     True        False
2    半角 2          False     True       True     True        False
²    上标 2          False     True       True     True        False
½    二分之一          False    False       True     True        False
五    汉字五            True    False       True     True         True
①    圆圈 1          False     True       True     True        False
Ⅷ    罗马数字八         False    False       True     True         True
_    下划线           False    False      False    False         True
     空格            False    False      False    False        False
```

（这个块到判定表结束。捕获里下面还有 13 行：三行「分工」结论、`int()` 的实测、以及三个零散判定，
上面砍掉了，逐字接抄在下面这个不带提示符的块里：）

```text
→ 分工：isdigit 认「能当十进制数位用」的（含全角/上标/圈数字）；
  isnumeric 最宽（连 ½、五、Ⅷ 都算「有数值」）；isalpha 管「字母类」（汉字也算）。

但 isdigit=True 不等于 int() 能用：
int('１２３')（全角）                           → 123
int('²') → ValueError: invalid literal for int() with base 10: '²'
int('½') → ValueError: invalid literal for int() with base 10: '½'
→ 判「能不能 int()」：ch.isdigit() 再 int() 兜住大多数，或者干脆 try/except；
  isnumeric 只说明『这是个数量』，不保证能转。
'中'.islower()                            → False  ← 无字母可判 → False（不是 True！）
'abc123'.isascii()                       → True
' '.isspace()                            → True
[退出码 0]
```

读那张表的三个层次：

- **`isalpha`：Unicode 类别是「字母」就算** —— 所以汉字 `中`（`Lo`）、汉字数字 `五`、罗马数字 `Ⅷ`（`Nl`）
  全是 `True`。拿 `isalpha()` 判「是不是英文字母」是错的，要 `s.isascii() and s.isalpha()`。
- **`isdigit` 是 `isnumeric` 的子集**：`²`（上标）、`①`（圈数字）、`１`（全角）都 `isdigit=True`
  （它们「能当十进制数位用」）；`½`、`五`、`Ⅷ` 只有 `isnumeric=True`（有数值，但不是数位）。
- **`isidentifier`**：能不能直接当 Python 变量名 —— `中` 可以（`'中'.isidentifier()` 为 True），
  `_` 可以，全角 `１` 不行。这是第 02 章「标识符」那条规则的正身。

然后是本节最阴的一条：**`isdigit()` 为 True 不代表 `int()` 能用**。
`int('１２３')`（全角）→ `123`（`int` 认十进制数位类的 Unicode 数字），
但 `int('²')` 抛 `ValueError: invalid literal for int() with base 10: '²'`。
判「能不能转整数」的可靠写法是 `try / except`，不是 `isdigit()`。

还有一条很容易答错：`'中'.islower()` → **`False`**（不是 `True`）——
串里**没有任何 cased 字符**时，`islower` / `isupper` / `istitle` 一律返回 `False`。
所以 `if not s.islower(): s = s.lower()` 这类写法，对纯中文 / 纯数字串会白跑一趟（无害）；
反过来 `if s.islower()` 判「已经是小写」则是**错的**（中文串永远进不去那个分支）。

★ **结论（第 21 节）**：`is*` 家族问的是 **Unicode 类别**，不是你的人类语义。
「是不是英文字母」「能不能转 int」这两问 `is*` 都答不了 —— 前者加 `isascii()`，后者用 `try/except`。

## 22. 显示宽度：`east_asian_width`、三个数字、CJK 表格对齐两种补法（demo 第 22 节）

这是本章最后一条、也是排版时最疼的一条：**`str` 里根本没有「占几格」这个信息**，
终端宽度是字体和终端的事。Python 能给的只有 Unicode 数据里的一个**建议**：

```text
$ python3 g_width.py
====================================================================
一、east_asian_width 的分类（本机逐个实测）
====================================================================
  U+0061 east_asian_width=Na  combining=  0  类别=Ll
  U+4E2D east_asian_width=W   combining=  0  类别=Lo
  U+FF30 east_asian_width=F   combining=  0  类别=Lu
  U+FF71 east_asian_width=H   combining=  0  类别=Lo
  U+00B1 east_asian_width=A   combining=  0  类别=Sm
  U+03A9 east_asian_width=A   combining=  0  类别=Lu
  U+2192 east_asian_width=A   combining=  0  类别=Sm
  U+2605 east_asian_width=A   combining=  0  类别=So
  U+3000 east_asian_width=F   combining=  0  类别=Zs
  U+1F600 east_asian_width=W   combining=  0  类别=So
  U+0301 east_asian_width=A   combining=230  类别=Mn
  U+200B east_asian_width=N   combining=  0  类别=Cf
→ W=宽（中日韩汉字）、F=全角、H=半角、Na=窄、N=中性、A=暧昧（看字体脸色）。
  U+3000 全角空格是 F：它吃掉两格，很多「多一个空格」的错位就是它。
```

六个类别的用法：`W`（宽，汉字）和 `F`（全角）算 2 格；`Na`（窄）、`H`（半角）、`N`（中性）算 1 格；
**`A`（Ambiguous，暧昧）取决于字体和终端配置** —— 传统中文终端里常按 2 格画，现代终端多是 1 格。
本章的估算把 `A` 按 1 格算，这一条是**假设、不是实测**（已进末尾清单）。

`U+0301` 那一行还给了第二个字段：`combining=230` —— 它是**不占格**的（类别 `Mn`），要算 0 格；
`U+200B`（零宽空格，类别 `Cf`）同理是 0 格。`combining()` 返回的是「相对基字的偏移量」，
非 0 就说明它是挂在别人身上的记号。

（上面这个块只到第一个 `→` 结论。捕获里同一个脚本还打了两部分：「三个数字」和「两种补法」。
关掉围栏说明一下：**下面两个块没有命令行，是同一个 `g_width.py` 运行的后续输出**，逐字照抄。）

```text
二、三个数字各管各的：同一个字符串的 码点数 / UTF-8 字节数 / 显示格数
====================================================================
  'hello'                len=5  utf8字节=5  显示格=5
  '中文'                   len=2  utf8字节=6  显示格=4
  '中文字符串'                len=5  utf8字节=15  显示格=10
  'ＰＹＴＨＯＮ'               len=6  utf8字节=18  显示格=12
  'café'                 len=4  utf8字节=5  显示格=4
  'café'                len=5  utf8字节=6  显示格=4
  '😀'                    len=1  utf8字节=4  显示格=2
```

倒数第三、二行那两行 `café` **在这个文档里长得一模一样**，但一个是 NFC（len 4 / 5 字节）、
一个是 NFD（len 5 / 6 字节）—— 第 2 节那条差异到了这一层彻底消失：**显示格都是 4**。
于是章首那三把尺子在一张表里凑齐了：`'中文字符串'` 是 **5 / 15 / 10**，`'😀'` 是 **1 / 4 / 2**，
三个数字两两不等，谁也不能替代谁。

```text
三、同一张表两种补法（对比右边那条竖线）
====================================================================
按 len() 补（错位版）：
  |中文字符串   | 数据 6
  |café    | 数据 6
  |hello   | 数据 6
  |ｱｲｳ     | 数据 6
  |±x      | 数据 6
按显示宽度补（对齐版）：
  |中文字符串| 数据 6
  |café    | 数据 6
  |hello   | 数据 6
  |ｱｲｳ     | 数据 6
  |±x      | 数据 6
→ len('中文字符串')=5 以为占 5 格，实际占 10 格：竖线就是这么歪的。
[退出码 0]
```

（如果你在自己的终端里看这两版**竖线一样齐**，那不是脚本错了 —— 是你的字体把 `±`（A 类）
按 2 格画了，或者你的 Markdown 渲染器用了等宽字体之外的规则。这恰好就是「A 类不可靠」的现场演示。
差异最稳的一行是 `|中文字符串`：按 `len` 补只给 5 格宽的位子，它实际要 10 格。）

对齐版用的宽度函数，就是本章给出的判据写法（`g_width.py` 里那份的骨架）：

```python
import unicodedata

def display_width(s: str) -> int:
    w = 0
    for ch in s:
        if unicodedata.combining(ch):            # 组合记号：0 格（U+0301 那一行）
            continue
        if unicodedata.east_asian_width(ch) in ('W', 'F'):
            w += 2
        else:                                     # Na / H / N / A：按 1 格（A 是假设！）
            w += 1
    return w

def pad(s: str, target: int) -> str:              # target 是「格」数
    return s + ' ' * max(0, target - display_width(s))   # 左对齐：补在后头

# 补完直接用：f"{pad(name, 12)}"
# 别再套第 12 节那种 `:>12`——格式规范的宽度数的是码点，不是「格」，套上等于又绕回 len()。
```

★ **结论（第 22 节）**：**排版对齐别信 `len`**。要按显示格对齐就自己写 `display_width`
（`W` / `F` 算 2、`combining` 算 0），并接受「`A` 类和 emoji 是估计值」这个事实。
要精确只能问终端或引第三方 `wcwidth`（本机没装，本章没演）。

## 23. `bytes` 与 `str` 的边界、`bytearray` 可变、`in` 与逐字符遍历（demo 第 23 节）

### 23.1 `bytes[i]` 给的是 int；`bytes + str` 报错原文

```text
$ python3 h_bytes_iter.py 1
====================================================================
一、bytes 的下标给出的是 int，不是「长度为 1 的 bytes」
====================================================================
b'abc'[0]                            → 97  ← int！对照 C：char 数组取下标是值不是指针
b'abc'[0:1]                          → b'a'  ← 切片才回到 bytes
list(b'abc')                         → [97, 98, 99]
chr(b'abc'[0])（字节数再变回字符）             → 'a'
b'abc' + 'abc' → TypeError: can't concat str to bytes
'abc' + b'abc' → TypeError: can only concatenate str (not "bytes") to str
→ bytes 和 str 是两个世界，+ 不跨界；要跨界就 encode/decode。
bytes([256]) → ValueError: bytes must be in range(0, 256)
'中'.encode('utf-8') == b'\xe4\xb8\xad' → True

bytearray：可变的字节；bytes：不可变（呼应第 02 章）
bytearray(b'ab') 改 [0]=65、append(99) → b'Abc'
b'ab'[0] = 65 → TypeError: 'bytes' object does not support item assignment
bytes(ba) 转回不可变                      → b'Abc'
hash(bytearray) → TypeError: unhashable type: 'bytearray'
hash(b'ab') 可用                       → True
[退出码 0]
```

C 的 `char` 是「既能当小整数又能当字符」的两面派；Python 把这两面拆干净了：

- **`bytes[i]` 是 `int`（0~255）**，不是「长度为 1 的 bytes」。想要字节对象得写 `b[i:i+1]`。
  这条对 C 来的人最反直觉，但 `list(b'abc')` → `[97, 98, 99]` 正好自洽。
  要回到字符是 `chr(b'abc'[0])` → `'a'` —— **只对 ASCII 那部分有意义**；
  对 `0xe4` 用 `chr` 只会得到一个毫无意义的私有字符，因为字节不能单独解成字符，必须整段 `decode`
  （第 1 节那个 `b[0]=228` 的续集）。
- **两个方向的 `+` 都不许，而且两句报错文本不一样**：从 `bytes` 侧发起是
  `can't concat str to bytes`，从 `str` 侧发起是 `can only concatenate str (not "bytes") to str`。
  读 traceback 时这两句能直接告诉你是**哪一侧**的 `+`。
- **`bytes([256])` 抛 `ValueError: bytes must be in range(0, 256)`** —— 半开区间写法，
  255 行、256 不行。C 的 `char` 会绕回去（截断成 0），Python 直接拒。

`bytearray` 是「能原地改的字节」：`ba[0] = 65`、`ba.append(99)` 都行；
代价是它**不可哈希**（`hash(bytearray)` → `TypeError: unhashable type: 'bytearray'`），当不了 dict 键；
`bytes` 可哈希。这正是第 02 章那条「可变 ⇒ 不可哈希 ⇒ 不能当键」规律的字节版。

`bytes([256])` 的原始 traceback 单独跑一条：

```text
$ python3 -c "bytes([256])"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    bytes([256])
    ~~~~~^^^^^^^
ValueError: bytes must be in range(0, 256)
[退出码 1]
```

### 23.2 `in` 判的是子串；`for c in s` 拿到的是长度 1 的 `str`

```text
$ python3 h_bytes_iter.py 2
====================================================================
二、in 判的是子串；for c in s 拿到的是长度为 1 的 str
====================================================================
s = '我爱你中国'
'中' in s                             → True
'国中' in s                            → False  ← 顺序反了就不是子串（in 要连续同序）
'love' in 'I love Python'            → True
→ str 的 in = 「是不是子串」，等价 C 的 strstr，不是「集合里有没有」。

逐字符遍历：每个 c 都是 str、长度 1 —— 不是 C 的 char
  c='中'  type=str  len=1  ord=20013 U+4E2D
  c='a'  type=str  len=1  ord=97 U+0061
换成遍历 bytes，元素就是 0~255 的整数：
  [0] 228 (0xe4)
  [1] 184 (0xb8)
  [2] 173 (0xad)
  [3] 97 (0x61)
→ 一个「中」字：str 视角 1 个元素；bytes 视角 3 个元素。这就是章节开头的延续。

enumerate / zip 两个常用式：
list(enumerate('abc'))               → [(0, 'a'), (1, 'b'), (2, 'c')]
list(zip('abc', [1, 2]))             → [('a', 1), ('b', 2)]  ← zip 默认配到短的为止
strict=True 长度不齐 → ValueError: zip() argument 2 is shorter than argument 1
[退出码 0]
```

三条：

- **`in` 对 `str` 是子串判断**（等价 C 的 `strstr`），对 `set` / `dict` 才是「成员」。
  `'国中' in '我爱你中国'` → `False` 说明它要**连续且同序**；
  想问「这几个字都出现过吗」得写 `set('国中') <= set(s)`。
- **`for c in s` 的 `c` 是长度为 1 的 `str`，不是字符**。Python 压根没有「字符」这个类型 ——
  一个字符就是一个长度为 1 的字符串；`ord(c)` 才拿到码点，`chr(n)` 反过来。
  而遍历 `bytes` 得到的是 `int`（`228 / 184 / 173` —— 一个「中」字的 3 个字节各算一个元素）。
  **同一份文本，`str` 视角走 5 轮、`bytes` 视角走 15 轮**（`'中文字符串'`），这就是第 1 节的延续。
- **`zip` 默认在最短处停下且不报错**，这是「两个列表长度不同就少跑几轮」的经典 bug 源。
  3.10+ 的 `zip(a, b, strict=True)` 会抛
  `ValueError: zip() argument 2 is shorter than argument 1` —— 处理外部数据时**默认加 `strict=True`**。

`b'abc' + 'abc'` 的 traceback 也单独跑一条，注意那条**单个** `^`（二元运算符只标运算符本身）：

```text
$ python3 -c "b'abc' + 'abc'"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    b'abc' + 'abc'
    ~~~~~~~^~~~~~~
TypeError: can't concat str to bytes
[退出码 1]
```

★ **结论（第 23 节）**：**在函数边界上就决定收 `str` 还是 `bytes`，中间不许偷偷转换**
（`b + s` 报错是在帮你，不是刁难）。文本用 `str`，协议 / 文件 / 哈希用 `bytes`；
跨界只在出入口做，而且**必须显式写 codec 名**。

## 24. 收尾：证明这一章没在手册目录里留下任何产物（demo 第 24 节）

本章八个示例脚本全都不写文件（只 `print`），但还是要自证：

```text
$ 〔在 03-strings-and-text 里〕 find . \( -name '__pycache__' -o -name '*.pyc' -o -name '*.pyo' -o -name '*.tmp' \) | wc -l
0
[退出码 0]
```

```text
$ find . -newer a_codepoints.py -name '*.pyc' | wc -l
0
[退出码 0]
```

第一条找所有编译产物和临时文件（**相对路径**，所以输出里不会有绝对路径），第二条找「比源码更新的 pyc」，
两个数都必须是 `0`。能做到是因为脚本全程用 `python3 xxx.py` 直接跑、从不 `import` 本章的示例模块
（第 01 章讲过：只有被 import 的模块才会写 pyc）。

捕获的最后两行，原样照抄（不带提示符）：

```text
[全部跑完] 上面每一段都是真实执行结果；README.md 引用的输出全部出自这一次运行。

[沙箱已删除] /tmp/py03-hjVjJH
```

★ **结论（第 24 节）**：`0` 和 `0`。你 clone 这个仓库跑 `bash demo.sh`，也不会多出一个文件。

## 对照表

### C 的 `char*` vs Python 的 `str`（全章汇总）

| 你在 C 里的概念 | Python 的真实情况 | 本章证据 |
| --- | --- | --- |
| `char*` 就是字节数组 | `str` 是码点序列；字节在 `bytes` 里，两个类型不混、不能相加 | 第 1、23 节 |
| `strlen(s)` 一个函数管长度 | 三把尺子：`len(s)` 码点 / `len(s.encode())` 字节 / 显示格自己算 | 第 1、22 节 |
| `char` 既是小整数也是字符 | `bytes[i]` 是 `int`；`str[i]` 是长度 1 的 `str`；`ord` / `chr` 才跨得过去 | 第 23 节 |
| `strcmp` 比字节 | `==` 比码点序列；Unicode 等价要先 `normalize` | 第 2、3 节 |
| `strchr` / `strstr` | `ch in s` 判子串（要连续同序）；`s.find(...)` 给下标 | 第 23 节 |
| `sprintf("%-10s", s)` 补 10 个字符宽 | `f"{s:<10}"` 补的是 **10 个码点**；中文要自己按显示格补 | 第 12、22 节 |
| `strcat` + 手动 `realloc` | `+` / f-string / `'sep'.join(...)`；循环里 `+=` 是 O(n²) | 第 15、16 节 |
| 越界读写 = 未定义行为 | 单下标 `IndexError`；切片静默夹断给短串 | 第 9 节 |
| 数组可以原地改 | `str` 不可变（`TypeError`）；要改就重建，或用 `list` / `bytearray` | 第 10、23 节 |
| `isalpha()` 跟着 locale 走 | `str.isalpha()` 走 Unicode 表，**与 locale 无关**；但汉字也算 True | 第 21 节 |
| `'中'` 是合法字符字面量 | `'中文'` 合法，`b"中文"` 是 `SyntaxError` | 第 1 节 |
| 读文件得到 `char*` | 读进来的是字节，**必须先选 codec** 才变 `str`；选错就是乱码（可 latin-1 自救） | 第 4、6 节 |
| 文本长度 = 占的格子数 | 完全无关：`'中文字符串'` 是 5 码点 / 15 字节 / 10 格 | 第 22 节 |

## 一页纸（考前扫这半页）

### 三张地图，各配一条判据

下面三条**不是捕获原文**，是把本章脚本里用过的判据抽出来给你现抄（本机都跑过同型命令）：

```bash
# 地图①【码点层 = str】：几个码点、各是谁
python3 -c "s='你的串'; print(len(s), [hex(ord(c)) for c in s])"
#   一句话：这里没有「字节」也没有「格」；== 逐码点比；不可变。

# 地图②【字节层 = bytes】：落地成字节是几个
python3 -c "s='你的串'; b=s.encode('utf-8'); print(len(b), repr(b))"
#   一句话：和外部世界（socket / 文件 / 哈希 / C 扩展）只在这一层见面；换层必须写 codec 名。

# 地图③【显示格 = 终端】：排版要对齐看这个（与第 22 节 display_width 同一条规则）
python3 -c "import unicodedata as u; s='你的串'; print(sum(0 if u.combining(c) else 2 if u.east_asian_width(c) in ('W','F') else 1 for c in s))"
#   一句话：A（暧昧）类和 emoji 是估计值；对齐不准先怀疑 len，别怀疑字体。
```

本机实测的三数对照：`'中文字符串'` = **5 / 15 / 10**，`'café'`（NFD）= **5 / 6 / 4**，
`'😀'` = **1 / 4 / 2**（第 1、22 节）。

### 三条铁律

1. **要字节先 `encode`，写清 codec，别裸着跨层。**
   `str + bytes` 一定 `TypeError`（第 23 节两句原文），`b"中文"` 连语法都过不了（第 1 节）。
   出入口各一次 `encode` / `decode`，中间永远只走 `str`。
2. **比 Unicode 等价就 `normalize`**：比较、当 dict 键、查重之前一律 `normalize('NFC', ...)`。
   é 有两种写法，`==` 是 False、`len` 是 4 或 5（第 2、3 节），macOS 存 NFD、Windows 传 NFC。
3. **排版对齐别信 `len`**：`f"{'中':>6}"` 补的是 6 个码点不是 6 格（第 12 节实测 `'     中A'`）；
   中文表格歪掉的原因永远是 `east_asian_width` 没参与（第 22 节）。

### 「我该用哪个方法」速查（最高频的 16 条）

| 想干什么 | 用这个 | 别用这个 | 本机证据 |
| --- | --- | --- | --- |
| 数「几个字」 | `len(s)`（码点数） | 当成字节数 / 当成显示格 | 第 1、22 节 |
| 数「几个字节」 | `len(s.encode('utf-8'))` | `len(s)` | 第 1、4 节 |
| 判两个 Unicode 串同不同意 | `normalize('NFC',a) == normalize('NFC',b)` | 直接 `==` | 第 2、3 节 |
| 判首字符是不是 BOM | `b[:3] == b'\xef\xbb\xbf'`；读时用 `utf-8-sig` | 拿 `utf-8` 读 Windows 文件 | 第 7 节 |
| 判某 codec 本机有没有 | `codecs.lookup(name)` 现场试 | 猜、或抄网上列表 | 第 8 节 |
| 允许坏字节继续跑 | `errors='backslashreplace'` | `errors='ignore'`（数据无声消失） | 第 5、6 节 |
| 反转一个串 | `s[::-1]` | 手写交换循环 | 第 10 节 |
| 复用一段切法 | `slice(a, b, step)` 存成常量 | 到处复制 `a:b:step` 字面量 | 第 10 节 |
| 收集可变文本 | `list` + `''.join(parts)` | `s += x` | 第 10、16 节 |
| 按空白切词 | `s.split()` | `s.split(' ')` | 第 17 节 |
| 切了还要装回去 | `s.split(sep)` + `sep.join(...)` | `split()` / `splitlines()` + join（有损） | 第 19 节 |
| 切 `key=value` | `s.partition('=')` | `s.split('=')` 再解包 | 第 17 节 |
| 按行切 | `s.splitlines()`（认 11 种） | `s.split('\n')`（只认 LF） | 第 18 节 |
| 去前缀 / 后缀 | `removeprefix` / `removesuffix` | `strip('pre_')`（那是字符集合） | 第 20 节 |
| 不分大小写比较 / 排序 | `.casefold()`、`key=str.casefold` | `.upper()`（漏 `ß`）、`sorted()`（大写在前） | 第 21 节 |
| 判「是不是英文字母」 | `s.isascii() and s.isalpha()` | `s.isalpha()`（汉字也 True） | 第 21 节 |
| 判「能不能转 int」 | `try: int(x)` | `x.isdigit()`（`'²'` 会骗你） | 第 21 节 |
| 调试打印名和值 | `f"{x=}"` | `print("x =", x)` | 第 11 节 |
| 让空格 / 引号现形 | `f"{s!r}"` | `f"{s}"` | 第 11 节 |
| 中文表格对齐 | 自写 `display_width` + 现算宽度 `{x:>{w}}` | `f"{x:<10}"` | 第 12、22 节 |
| 日期出中文 | 代码里 `setlocale(LC_TIME, ...)`，或自己拼 | 只改环境变量 | 第 14 节 |
| 配对遍历外部数据 | `zip(a, b, strict=True)` | `zip(a, b)`（悄悄截短） | 第 23 节 |

## 这章的坑（按代价从大到小）

1. **把 `len()` 当字节数或当显示宽度用**（第 1、22 节）。代价三连：按字节限长会拒掉合法输入
   （2 个汉字 6 字节）、按 `len` 对齐的表必歪、按 `len` 去切 UTF-8 字节串会把一个汉字切两半
   → `UnicodeDecodeError`。**本章代价最大的一条。**
2. **循环里用 `+=` 拼串**（第 16 节）：O(n²)，1000 词实测 `27 usec` vs `join` 的 `5.44 usec`，
   而且**没有任何报错**。
3. **`split()` / `splitlines()` 拆开再 `join` 回去**（第 19 节）：连续空白塌成一根、`\r\n` 掉一个 `\r`、
   首尾空白全没 —— **静默改坏文本**，格式化器 / 配置写回场景的致命 bug。
4. **以为 `'xyfooxy'.strip('xy')` 是去前缀**（第 20 节）：它按字符集合从两端啃，
   `'mississippi'.strip('isp')` → `'m'`、`'banana'.strip('ban')` → `''`。
   数据被啃穿，而现象只是「好像少了点东西」，很难被发现。
5. **不 `normalize` 就比 Unicode 相等 / 拿它当 dict 键**（第 2、3 节）：`KeyError`、查重失效、
   跨平台（macOS NFD 文件名）对不上。
6. **把 `errors='ignore'` / `'replace'` 当万能药**（第 5、6 节）：`b'x'` 和 `''` 都是「成功返回」，
   坏数据无声进下游。要「不崩且不丢」只有 `backslashreplace`。
7. **解码方向按字符数理解 `position`**（第 5 节 vs 第 6 节）：编码方向报**码点**下标，
   解码方向报**字节**下标。两套坐标混用，你就永远指错地方。
8. **切片越界不报错**（第 9 节）：`'abc'[10:20]` → `''`，和「正好是空」无法区分 → 藏 bug。
   C 里越界是 UB，你会立刻撞上；这里不会。
9. **`split(' ')` 与 `split()` 混用**（第 17 节）：`''.split()` → `[]` 而 `''.split(' ')` → `['']`，
   解包时一个报「不够」一个报「太多」。
10. **想原地改字符串**（第 10 节）：`s[1:3] = "X"` → `TypeError: 'str' object does not support item assignment`。
    正解是重建，或改用 `list` + `join`。
11. **`int()` 之前拿 `isdigit()` 把关**（第 21 节）：`'²'.isdigit()` 是 True，`int('²')` 抛 `ValueError`。
12. **拿 `isalpha()` 判「英文字母」、拿 `islower()` 判中文**（第 21 节）：`'中'.isalpha()` 也 True；
    `'中'.islower()` 是 False（无 cased 字符一律 False）。
13. **以为大小写不改长度**（第 21 节）：`'ß'.upper()` len 1→2、`'İ'.lower()` len 1→2、`'ﬀ'.upper()` → `'FF'`；
    `title()` 还会把 `they're` 变成 `They'Re`。
14. **`zip` 不加 `strict=True`**（第 23 节）：长度不齐时静默截断，少跑几轮没有任何提示。
15. **给 `join` 传字符串**（第 19 节）：`'-'.join('abc')` → `'a-b-c'`（分隔符在前，字符串也可迭代）。
16. **把 `bytes([256])` 当「会截断」**（第 23 节）：C 的 `char` 绕回去，Python 抛
    `ValueError: bytes must be in range(0, 256)` —— 想绕记得自己 `% 256`。
17. **指望环境变量把日期变中文**（第 14 节）：`LC_ALL=zh_CN.UTF-8` 实测 `%A` 仍是 `Saturday`；
    必须代码里 `setlocale(LC_TIME, ...)`，而它是进程全局的。
18. **把「f-string 里不能复用引号 / 不能有反斜杠」当真理**（第 13 节）：本机 3.14.4 六条全过，
    这条禁令已经不适用于 3.12+；反过来在旧版本上复用新写法行不行，本机没验证，
    但「发到别人机器上可能炸」这个风险是真实的。
19. **用 `split('.')[0]` 取文件名**（第 17、20 节）：`my.tar.gz`、`archive.` 都会错；
    用 `removesuffix`（`os.path.splitext` 留给第 08 章）。
20. **把 BOM 当正文**（第 7 节）：`'\ufeff'` 混进第一个字符，`startswith` / 表头比对 / `int()` 全错位；
    读的时候用 `utf-8-sig`。
21. **拿 `.format` / `format_map` 处理别人给的模板还不管缺键**（第 15 节）：模板里的名字没传够就抛异常
    （原文本章没演）；而拿 f-string 处理外部模板，等于把别人的字符串当代码执行。

## 自检问题

1. 同一段文本 `len(s) = 5`、`len(s.encode('utf-8')) = 15`、显示 10 格。这三个数各是什么的个数？
   本章实测过的哪个字符串正好是这组数？（第 1、22 节）
2. `a = "\u00e9"`、`b = "e\u0301"`。说出 `len(a)`、`len(b)`、`a == b`、`NFC(b) == a`、`NFD(a) == b`，
   以及 `b` 最后一个码点的类别。（第 2、3 节）
3. `UnicodeEncodeError` 和 `UnicodeDecodeError` 消息里的 `position` 分别是**谁**的下标？
   各举本章一条实测命令说明。（第 5、6 节）
4. 一段 UTF-8 字节被错误地按 latin-1 解成了怪字符串，怎么修回 `中`？为什么这招能成立？
   如果是被错误地按 GBK 解的，这招还成立吗？（第 4、6 节）
5. 不查文档，写出 `'a\r\nb\x0bc'.splitlines()` 和 `'a\n'.split('\n')` 的结果。（第 18 节）
6. `'mississippi'.strip('isp')`、`'banana'.strip('ban')`、`'abcabc'.removeprefix('xyz')`
   各是什么？三条各自说明什么规则？（第 20 节）
7. 要把用户名列表不分大小写排序，还要正确处理 `ß`。写出那一行。（第 21 节）
8. `'İ'.lower()` 的长度是多少？拆开是哪两个码点？`'ß'.upper()` 呢？（第 21 节）
9. `s = 'hello'` 时 `s[100]` 和 `s[10:20]` 各发生什么？为什么这个不对称对写代码的人很重要？
   `slice(-4).indices(11)` 给什么？（第 9、10 节）
10. 外部环境变量已经 `LC_ALL=zh_CN.UTF-8`，`strftime('%A')` 打出什么？怎么改才能打「星期六」？
    `'ß'.upper()` 会跟着 locale 变吗？（第 14、21 节）
11. 写出 `f"{3.14159:>10.2f}"`、`f"{-42:=05d}"`、`f"{1234567:,}"`、`f"{0.256:.1%}"` 的输出；
    `f"{5:.2f}"` 会怎样？（第 12 节）
12. 用 f-string 输出字面 `{42}` 怎么写？`f"{'ab':*^10}"` 输出什么？（第 11、12 节）
13. `b'abc'[0]`、`b'abc'[0:1]`、`list(b'abc')`、`'abc'[0]`、`type('abc'[0])` 各是什么？
    两句 `+` 的 `TypeError` 消息为什么长得不一样？（第 23 节）
14. 1000 次拼串，`+=` 和 `join` 哪个快、本机快多少、为什么是数量级差异？（第 16 节）
15. 「拆开还能原样装回去」—— `split(' ')` + `' '.join` 和 `split()` + `' '.join` 哪个满足？
    `splitlines()` + `'\n'.join` 呢？（第 19 节）
16. 本机 `sys.getdefaultencoding()` 和 `locale.getpreferredencoding()` 各是什么？
    一个管什么、一个管什么？（第 0 节）

答不上来就重跑 `bash demo.sh` 对应那节 —— 每节开头的 `══════` 标题就是索引。

## 本章没演 / 没验证（绝不编造，都在这）

| 想写但没在本机证明 / 没演的事 | 状态 |
| --- | --- |
| `A`（Ambiguous）类字符在**真实终端**里到底占 1 格还是 2 格；第 22 节 `display_width` 把它按 1 格算 | **假设，未实测**（demo 跑在管道里，没有终端可问） |
| `wcwidth` / `unicodedata2` 之类第三方库的宽度表 | 没装也不装（只用标准库），所以没演 |
| emoji 序列（ZWJ 家庭 emoji、变体选择符 U+FE0F、肤色修饰符）的码点数与显示格数 | 没演；第 22 节只演了单个 `U+1F600` |
| macOS 文件系统真把文件名存成 NFD 的现场（第 3 节那两句是**结论性说明**，本机是 Linux） | 没实机验证 |
| UTF-16 代理对（surrogate pair）的机制、`surrogatepass` / `surrogateescape` 两个错误处理器 | 没演；第 4 节只在 `utf-16` 那列出现了 4 / 6 字节 |
| `open()` 不写 `encoding` 到底用什么、`newline=` 怎么影响 `\r\n`、文本 / 二进制模式差异 | 没演，**全部留给第 08 章**；本章只给了两个本机值（第 0 节） |
| `sys.stdout` / `print` 在非 UTF-8 终端下的重编码行为（`-X utf8` 只演到 `sys.flags.utf8_mode` 变 1） | 没演完 |
| `str` 内部的 1 / 2 / 4 字节紧凑存储结构（PEP 393 的 `PyUnicodeObject` 布局）、`sys.getsizeof` 的串尺寸 | 没演。本章只从外部证明「`str` 数码点、不显字节数」 |
| `str` 里能不能出现 `\0`、它会不会像 C 那样截断 | 没演，**不下结论** |
| 3.11 及更早版本上那些新写法**到底能不能跑**、报错文本长什么样（含 demo 旁白那句「只有第一条能活」） | 本机无旧解释器，**未验证**；第 13 节能确定的只有「3.14.4 六条全过」 |
| `unicodedata.name()` 对无名码点抛什么（推测是 `ValueError`） | 没演 |
| locale 敏感的 `upper()`（土耳其语 I / İ 规则）在 Python 里怎么实现 | 没演。第 21 节只证明了标准方法**与 locale 无关** |
| `str.translate` / `str.maketrans`、`textwrap`、正则 `re`、`difflib` | 没演，本章刻意不碰（`re` 与文本切分的关系留给后面章节） |
| `f"{-42:05d}"`（不带 `=`）的输出、`'ß'.lower()` 的结果、f-string 里单个未配对 `{` 的报错原文 | 没演（第 12、21、11 节正文里都按「未演」处理，没当结论用） |
| `a, b = ''.split()` / `a, b = ''.split(' ')` 的解包报错原文；`.format` 缺键、`%` 参数不匹配的报错原文 | 没演：第 17、15 节只说了「会崩」，没引文案 |
| `startswith` / `endswith` / `find` / `index` / `count` / `center` / `ljust` / `zfill` / `replace` / `translate` 这些方法本章没碰 | 没演，不下结论（第 20 节只演了 `strip` 家族） |
| BOM 混进正文后 `startswith('#')` 失效、CSV 表头对不上、`int('\ufeff12')` 崩 这些**后果** | 没演，正文里标为推断；只有「首字符变成 `\ufeff`」是实测（第 7 节） |
| `find` / `rfind` / `index` / `rindex` / `count` / `startswith` / `endswith` / `center` / `ljust` / `rjust` / `zfill` / `replace` 这批方法 | **本章没碰、也没下结论**（它们和 `split` / `strip` 家族的区别只在「找」与「换」，规则一致）；异常原文留给第 07 章 |
| 网络收发时 `encode` 后长度受限（如按字节截断标题）的真实崩溃现场 | 没演（不联网），只说明了为什么按 `len(s)` 截字节会截出半个汉字 |
| `timeit` 的绝对值可复现性 | 刻意不承诺：只引 `[形状]` 行，数字每次会抖 |

## 怎么跑这一章

```text
cd python-handbook/03-strings-and-text
bash demo.sh                # 全跑一遍，退出码 0，约 780 行输出
bash demo.sh --keep         # 跑完保留沙箱并把目录名打给你，进去自己接着敲
```

想只看某一节：

```text
bash demo.sh 2>&1 | awk '/^══════ 22\./,/^══════ 23\./'
```

`demo.sh` 的行为约定（红线，代码里都写着）：

- 所有副作用只进 `mktemp -d "${TMPDIR:-/tmp}/py03-XXXXXX"` 出来的那个目录；
  `trap cleanup EXIT` 只删自己建的这一个目录，`--keep` 时不删、改成打一行提示。
- **不写、不改、不删仓库里任何其他文件**；`python-handbook/README.md` 的章节地图由主 agent 维护。
  本章八个示例脚本一律 `python3 xxx.py` 直接跑，从不 `import` 彼此，所以连 `__pycache__` 都不会出现
  （第 24 节用两条 `find | wc -l` 现场自证为 `0`）。
- 只用标准库：`unicodedata` / `codecs` / `locale` / `datetime` / `timeit`，不 pip、不联网、不改 shell 配置。
- 环境变量只在脚本自己和它的子 shell 里生效：`export LC_ALL=C.UTF-8`（为了报错文本可逐字比对），
  并 `unset` 掉 `PYTHONPATH` / `PYTHONUTF8` / `PYTHONIOENCODING`，避免本机残留影响结论。
- **故意不加 `set -e`**：本章有十几条命令是「注定失败」的（`'中文'.encode('latin-1')`、
  `'abc'.split('')`、`b'abc' + 'abc'`、`s[100]`…），加了就只看得到第一段。
- 每段前打 `$ 命令`、段末打 `[退出码 N]`，非零也继续跑。
- **全程不打印绝对路径**（只有随机沙箱名），报错 traceback 用 `python3 -c` 触发，
  所以 `File "<string>"` 而不是你的真实路径 —— 输出里不含用户名。
- 只在 `[ -t 1 ]`（真的接到终端）时上色，所以 `bash demo.sh > out.txt` 拿到的是干净纯文本。
- 复核引文：`bash demo.sh > ~/.cache/handbook-captures/py03.txt 2>&1` 之后
  `python3 ~/.local/bin/verify_quotes.py README.md ~/.cache/handbook-captures/py03.txt '/tmp/py03-[A-Za-z0-9]{6}'`
  → 期望末行 `未匹配行 0`。

## 术语小抄

| 术语 | 一句话解释（本章的用法） |
| --- | --- |
| 码点 (code point) | Unicode 表上的一个编号，写成 `U+00E9`。`str` 的元素就是它，`ord()` 取它，`chr()` 造它 |
| 字符 | 日常说法骗人：人眼「一个字」可能是 1 个码点（é）或 2 个码点（e + 组合重音符）|
| 组合记号 (Mn) | 不单独成字、要挂在前一个字母上的记号（`U+0301` 急性重音符）。`combining()` 非 0 |
| codec / 编码名 | `str` ↔ `bytes` 的换算器：`utf-8`、`gbk`、`latin-1`、`utf-16`…；`codecs.lookup` 查名录 |
| UTF-8 | 变长 1~4 字节的编码；ASCII 1、拉丁扩展 2、CJK 3、emoji 4；字节序无关 |
| latin-1 | U+0000~U+00FF 的**恒等映射**（每个码点 1 字节），所以拿它错读会「原样可逆」→ 乱码自救的基础 |
| BOM / U+FEFF | 字节序标记；UTF-8 里是 `EF BB BF`，历史包袱。`utf-8-sig` 会吃掉它 |
| NFC / NFD / NFKC | 归一化形式：合成 / 分解 / 兼容分解（NFKC 更狠，会把 `½`→`1⁄2`、`ﬁ`→`fi`）|
| `east_asian_width` | Unicode 给的宽度建议：`W` 宽、`F` 全角 = 2 格；`Na`/`H`/`N` = 1 格；`A` 暧昧看字体 |
| 切片 / `slice` 对象 | `s[a:b:step]` 是 `s[slice(a,b,step)]` 的糖；`slice.indices(n)` 给出夹断后的三元组 |
| f-string 格式规范 | `{表达式!转换:填充对齐宽度精度类型}`；宽度按**码点数**补，不是显示格 |
| PEP 701 | 3.12 起的新 f-string 解析器：允许复用同种引号、表达式里放反斜杠、f 套 f |
| `casefold` | 为「不分大小写比较」设计的归一化，比 `lower()` 狠（`ß` → `ss`）|
| `isdigit` / `isnumeric` | 前者认「能当十进制数位」（含 `²`、`①`、全角），后者更宽（还收 `½`、`五`、`Ⅷ`）|
| `str` 不可变 | 任何「改」都是造新串；因此可驻留、可哈希、可安全共用（第 02 章）|
| `bytes` / `bytearray` | 不可变字节序列 / 可变字节序列；`bytearray` 不可哈希，`bytes` 可 |
| 驻留 (interning) | 解释器把某些相同内容的字符串共用一个对象；和「内容相等」是两回事（第 02 章讲过）|

## 本章之后

- **第 04 章**（容器）：本章的 `'sep'.join(list)` 为什么比 `+=` 快、list 是指针数组这件事，
  接到「各容器的切片 / 拷贝 / 遍历代价表」上继续算。
- **第 05 章**（函数）：`str` 方法的完整签名（`split(sep=None, maxsplit=-1)` 这种默认值写法）、
  关键字参数与本章那些 `sep=` 用法。
- **第 07 章**（异常）：本章攒了一堆原文 —— `IndexError` / `TypeError` / `ValueError` /
  `UnicodeError` 家族 / `LookupError`。它们的继承关系、「该不该捕获」的判据在那一章结清。
- **第 08 章**（文件与路径）：本章刻意没碰的全部落地问题：`open()` 的 `encoding` / `newline=`、
  文本 vs 二进制模式、BOM 怎么进的文件、`surrogateescape` 处理怪文件名。
- **第 09 章 / 第 10 章**（环境与工具）：`-X utf8` 的完整效果、locale 与环境的排查手法。
- 正则（`re`）与更复杂的文本处理：本章有意不讲，避免和「切片 + `split`」混成两套心智模型。

**本章回指**：01 章讲过的「lib-dynload 里的 C 扩展」在第 8 节兑现（中文 codec 靠 `_codecs_cn`）；
01 章教的 traceback 读法（`^^^^` 列级标注、`File "<string>"`）在第 5、6、10、17、19、23 节反复用；
02 章「不可变对象」的三条后果（不能原地改 / 可哈希 / 可共用）分别落在第 10、23、10 节。
