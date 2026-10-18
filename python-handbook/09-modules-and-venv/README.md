# 第 09 章 · 模块与 venv：import 查的是三本账，venv 只改了其中一本

> 本章所有输出都是本机 `Python 3.14.4` 真跑出来的，一条都没编、一条都没改。
> 跑法：`cd python-handbook/09-modules-and-venv && bash demo.sh`（22 节，跑完自动删沙箱）。
> 想自己拨开关：`bash demo.sh --keep`，然后进打印出来的那个 `/tmp/py09-XXXXXX/work` 里接着敲。
>
> 本章的命令只有三类：`$ python3 某个文件.py`、`$ python3 -c "..."`、`$ python3 -m venv --without-pip ...`，
> 全部只读写 `mktemp -d` 出来的沙箱。**不联网、不装包、不开任何监听端口，也不动 `~/.local`、`~/.config`、`~/.bashrc`**。
> 本机没有 `ensurepip`，所以本章**建不出带 pip 的 venv** —— 这条失败没有跳过，第 16 节把原文照打当证据用了。
>
> 会变的量不进围栏：沙箱随机名、pid、毫秒一律不印在屏幕上，路径里的家目录折成 `<家目录>`、
> 安装前缀折成 `<py>`、当前目录折成 `<cwd>`，能判定的都改成了「相等吗 / 在第几项 / 几个 / 有没有」。
> 同一份 `demo.sh` 连跑两遍 `diff` 为空（本章定稿前实测过），第 22 节是它给自己的自证。

---

## 第 00 节 · 本章在讲什么：一次 `import` 背后有三本账

前八章讲的是「一个文件里的 Python」。这一章开始讲**跨文件**：一个名字怎么从磁盘上的某个 `.py`
变成你手里能点出属性的对象；以及为什么同一份代码，在你机器上能跑、在同事机器上就 `ModuleNotFoundError`。

这一章的主干是一句话：

> **`import x` 不是「打开文件」，是查三本账。**
>
> **一、`sys.modules` —— 模块账本。** 命中就直接把现成的对象给你，模块体**一行都不跑**。
> 「改了 `.py` 再导入一次没生效」拦在这里，不在文件系统。
>
> **二、`sys.path` —— 搜索路径。** 账本没命中才轮着它。它是**一个普通 list**，顺序就是优先级；
> 而它的第 0 项放的是什么，由**你怎么把 Python 叫起来的**决定，不由代码决定。
>
> **三、`__pycache__` —— 编译缓存。** 找到文件之后，parse/compile 的结果落一份盘；
> 它省掉的是「重新翻译」，**不省掉「重新执行」**。
>
> —— 而 `venv` 从头到尾只碰第二本账：换掉 `sys.path` 里那几行，解释器一个字节都没复制。

三条主线就按这三本账排：

- **主线①** 一次 `import` 查的三本账（第 2~7、11~15、19~20 节）：`a/b/c/d/e` 五篇各证一本，
  `f/g` 证「谁来查、能不能编程地查」，`h/i` 证「这本账是谁在解释器启动那一刻填进来的」。
- **主线②** `sys.path[0]` 的形状由启动方式决定（第 4、5 节）：同一份文件、一个字没改，
  换一种跑法就能「旁边那个模块拿得到 / 拿不到」。日常 90% 的「我这能跑」都出在这条线上。
- **主线③** venv 只是换了 `sys.path` 这本账（第 16~20 节）：`bin/` 里全是相对符号链接，
  `pyvenv.cfg` 里那一行开关就能整本换掉路径账 —— 魔法为零。

| 你想搞清楚的东西 | 本章用哪一节回答 | 一句话答案 |
|---|---|---|
| 改了 `.py` 再 `import` 为什么不生效 | 第 2、3 节 | 拦在 `sys.modules` 这本账上，跟文件系统无关：账本命中时**连文件都不打开**。删掉账本里那条或 `reload` 才会重跑，而 `reload` 不换对象 |
| `import a.b.c` 之后我手里多了什么 | 第 2 节 | 只多一个名字 `a`；模块本体照样被完整执行并进了账本。`del x` 删的是你这一格的绑定，不是模块 |
| 模块身上那些 `__name__` / `__file__` / `__builtins__` 谁塞的 | 第 2 节 | 全是 `__dict__` 里的普通键（`vars(m) is m.__dict__` 为真），执行模块体那一刻由解释器填。`__builtins__` 的**形状**不保证，别直接下标用它 |
| 改一个模块属性就能换掉它的行为，凭什么 | 第 2 节 | 函数体查全局名走的就是那份 `__dict__`；但 `def` 那一刻冻住的默认值改不动 —— 这就是 monkeypatch 能成立和会失效的分界 |
| 「我这能跑，他那跑不了」是怎么来的 | 第 4、5 节 | `sys.path[0]` 的形状由入口决定：脚本跑法给**脚本所在目录**、`-m` 给**当前目录**、`-c`/管道给**空串**、`-P`/`-I` 连空串都不给 |
| `ModuleNotFoundError` 为什么不告诉我它翻过哪些目录 | 第 6 节 | 报错原文里只有那个名字，一条搜索路径都不给。「import 不到」九成不是语法问题，是「根在哪」的问题 |
| 为什么 `os.chdir` 一下，同一份 `sys.path` 就换了根 | 第 6 节 | 空串不是启动时定死的目录，是**每次 import 的那一刻**拿 `os.getcwd()` 去解析的 |
| 新建一个 `json.py` 怎么会把标准库搞不见 | 第 5 节 | 它所在目录正好是 `sys.path[0]`，排在标准库之前。连锁错长这样：`module 'json' has no attribute 'dumps'` 加 `'json' is not a package` |
| 包和普通目录到底差在哪一个文件 | 第 7 节 | 差一个 `__init__.py`：有它 = 一份会执行的模块体 + 一条把同名目录切断的界线；没它 = 多个根拼起来的命名空间包（`__file__` 是 `None`） |
| `pkg.sub` 凭什么写得出来 | 第 7 节 | `import pkg.sub.mod` 除了跑模块体，还沿父链 `setattr`。所以只 `import pkg` 永远不带子模块 |
| 相对导入为什么一点 IDE 的「运行当前文件」就炸 | 第 7 节 | `.` 和 `..` 只认 `__package__` / `__spec__.parent` 这个**名字**，不看文件在哪；脚本跑法里它是 `None` |
| `python3 -m 包名` 跑的是谁 | 第 7 节 | 跑 `<包>.__main__`，缺它就是那句 `'pkgr' is a package and cannot be directly executed`；而 `-m json.tool` 跑的是包里的 `tool.py`，跟 `__main__` 无关 |

**读法建议**：只想知道「我这个 `import` 为什么找不到东西」，先读第 4、5、6 节，那是一整套排查顺序；
写过 `from x import *`、或者被人问过「`__init__.py` 里该写什么」的人重点读第 7 节；
准备用 venv 的人可以直接跳到第 16~20 节，但第 5 节的主线②是它的前置 —— 不懂 `sys.path` 怎么拼出来的，
就只能把 venv 当咒语念。第 1 节是地基（这台机器上解释器归谁、四个环境变量开关在哪），
第 3、6、10、18 这些标着「小证据」的节是把上一节的结论推到极端单独再看一遍，
可以跳，但跳了以后第 2 节那句「只跑一次」你会一直将信将疑。

**和相邻章节的接缝**：报错怎么接、`except ImportError` 接得住谁，家谱在第 07 章（本章第 2 节第 6 段用它）；
「默认值在 `def` 那一刻冻住」这条在第 05 章，本章第 2 节第 5 段拿它解释 monkeypatch 为什么有边界；
第 08 章讲的行缓冲和 stdout/stderr 顺序，在本章第 7 节的两次 `-m` 现场里会再撞一次
（那两格的 `[INIT]` 就排在 traceback 后面）。本章不管第三方安装，也不管虚拟环境外面该怎么组织项目 ——
那是第 10 章（typing 与工具链）和第 11 章（子进程与并发）的事；
本章只回答一件事：**一个名字是怎么变成对象的，以及它为什么没变成**。

---

## 第 01 节 · 环境体检：三本账的起点，和这台机器上解释器的归属

跨文件的结论最容易「换了台机器就不成立」，所以第一步把这台机器上解释器的身份、四个能改写 `import`
行为的环境变量、以及 `sys.path` 的初始形状钉死。这一节没有新结论，它的用处是：**后面每一节的结论
都能回指到这里的某一格**。

版本号先钉住，因为本章大量结论（`-P`、`-I` 的行为、`.pyc` 文件名里的 `314`、报错里那句
`consider renaming …`）都是版本相关的：

```
$ python3 --version && python3 -c "import sys; print(sys.version)"
Python 3.14.4
3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]
[退出码 0]
```

然后是「我到底站在哪个解释器上」。判据只看一个布尔：

```
$ python3 -c "import sys, os; print(\"sys.executable =\", sys.executable); print(\"sys.executable 的 basename =\", os.path.basename(sys.executable))"
sys.executable = /usr/bin/python3
sys.executable 的 basename = python3
[退出码 0]

$ python3 -c "import sys; print(\"sys.prefix 和 sys.base_prefix 相等吗 →\", sys.prefix == sys.base_prefix); print(\"sys.prefix 等于 sys.executable 所在目录的上一层吗 →\", sys.prefix == os.path.dirname(os.path.dirname(sys.executable)) if False else sys.prefix == os.path.dirname(os.path.dirname(os.path.realpath(sys.executable))))"
Traceback (most recent call last):
  File "<string>", line 1, in <module>
    import sys; print("sys.prefix 和 sys.base_prefix 相等吗 →", sys.prefix == sys.base_prefix); print("sys.prefix 等于 sys.executable 所在目录的上一层吗 →", sys.prefix == os.path.dirname(os.path.dirname(sys.executable)) if False else sys.prefix == os.path.dirname(os.path.dirname(os.path.realpath(sys.executable))))
                                                                                                                                                                                                                                                        ^^
NameError: name 'os' is not defined. Did you forget to import 'os'?
sys.prefix 和 sys.base_prefix 相等吗 → True
[退出码 1]
```

三行读法：

- 第一段是 `sys.executable`，本机是 `/usr/bin/python3` —— 一个系统包管理器管着的解释器，**本章所有 venv 都是从它长出来的**。
- 中间那段 `Traceback` 是这一格里第二个 `print` 用了 `os.path` 却忘了 `import os`，所以它当场翻了车。
  这是 demo 的 bug，不是设计出来的现场，我们不把它说成故意的；留着它有两个理由：
  一是 `[退出码 1]` 老老实实记下了这次翻车（第 22 节的自证就是同一个原则：错要错得能看见），
  二是改它就得重跑整章，22 节的行号整体漂移，这一节真正依赖的判据并不在它上面。
- 第三行 `sys.prefix 和 sys.base_prefix 相等吗 → True` 才是这一格要留的东西：**相等 = 我在系统解释器上，没泡在 venv 里**。
  第 16、18 节反复用的就是这一条判据，只看布尔，不印一堆带版本号和安装前缀的路径。

下面四个开关是本章结论的总闸。demo 一开场就把它们拨到「未设」，所以四行都该印 `（未设）`：

```
$ printf "PYTHONPATH=%s\nPYTHONHOME=%s\nPYTHONNOUSERSITE=%s\nPYTHONSAFEPATH=%s\n" "${PYTHONPATH:-（未设）}" "${PYTHONHOME:-（未设）}" "${PYTHONNOUSERSITE:-（未设）}" "${PYTHONSAFEPATH:-（未设）}"
PYTHONPATH=（未设）
PYTHONHOME=（未设）
PYTHONNOUSERSITE=（未设）
PYTHONSAFEPATH=（未设）
[退出码 0]
```

**你手敲时如果这里看到别的值，那说明你的 shell 配置正在改写 `import` 的行为** —— 本章每个脚本起子进程时
都自带一份干净 env，就是为了不让这些值混进结论。开关的下场由解释器自己报：

```
$ python3 -c "import sys, site; print(\"sys.flags.no_user_site =\", sys.flags.no_user_site); print(\"sys.flags.safe_path  =\", sys.flags.safe_path); print(\"site.ENABLE_USER_SITE     =\", site.ENABLE_USER_SITE); print(\"→ 前两个是解释器开关的下场，第三个是 site 自己声称要不要开家目录那条通道（和它真加没加是两件事，见 i 篇）\")"
sys.flags.no_user_site = 0
sys.flags.safe_path  = False
site.ENABLE_USER_SITE     = True
→ 前两个是解释器开关的下场，第三个是 site 自己声称要不要开家目录那条通道（和它真加没加是两件事，见 i 篇）
[退出码 0]
```

这格埋的是第 19 节的伏笔：`site.ENABLE_USER_SITE = True` 是 site **声称**要不要打开家目录那条通道，
和它**真的**加没加进去是两件事 —— 别把这两件事当一件。

再来是本章最重要的隐私红线，也是主线②的起点 —— `sys.path` 长什么样：

```
$ python3 -c "
import sys, os
h = os.path.expanduser(\"~\")
print(\"家目录前缀折成 <家目录> 之后的 sys.path：\")
print([p.replace(h, \"<家目录>\") for p in sys.path])
print(\"其中含家目录那一项吗 →\", any(\"<家目录>\" in p.replace(h, \"<家目录>\") for p in sys.path))
print(\"空串（＝当场查当前目录）在不在账上 →\", \"\" in sys.path, \"｜ 在第几项 →\", sys.path.index(\"\") if \"\" in sys.path else \"不在\")
"
家目录前缀折成 <家目录> 之后的 sys.path：
['', '/usr/lib/python314.zip', '/usr/lib/python3.14', '/usr/lib/python3.14/lib-dynload', '/usr/local/lib/python3.14/dist-packages', '/usr/lib/python3/dist-packages']
其中含家目录那一项吗 → False
空串（＝当场查当前目录）在不在账上 → True ｜ 在第几项 → 0
[退出码 0]
```

四件事一次看清楚：

- 打印前先把家目录前缀折成 `<家目录>`，所以屏幕上永远不会出现 `/home/…` 或登录名。
- 折算之后**含家目录那一项 → False**：本机 user site 那条通道虽然 `ENABLE_USER_SITE=True`，却没真进账（第 19 节演的是它）。
- 一共 6 项，**空串在第 0 项**。空串不是路径，是「每次 `import` 现查当前目录」，第 6 节专门把它推到极端。
- 这台机器的包目录叫 **`dist-packages`**（`/usr/local/lib/python3.14/dist-packages`、`/usr/lib/python3/dist-packages`），
  不叫 `site-packages`。第 20 节里「朴素地数 `site-packages` 出现几次」会数出 0，就是被这一行埋的。

最后两格交代本章的道具都在标准库里，顺带演示一个「判据自己会骗人」的坑：

```
$ python3 -c "
import sys
need = {\"importlib\", \"importlib.util\", \"site\", \"sysconfig\", \"struct\", \"marshal\", \"dis\", \"venv\", \"shutil\", \"subprocess\", \"tempfile\", \"ast\"}
print(\"本章用到的模块全在标准库里吗 →\", need <= sys.stdlib_module_names)
print(\"缺的：\", sorted(need - sys.stdlib_module_names) or \"无\")
print(\"venv 模块自带吗 →\", \"venv\" in sys.stdlib_module_names, \"｜ 但本机能不能建出带 pip 的 venv 是另一件事（见 h 篇）\")
"
本章用到的模块全在标准库里吗 → False
缺的： ['importlib.util']
venv 模块自带吗 → True ｜ 但本机能不能建出带 pip 的 venv 是另一件事（见 h 篇）
[退出码 0]
```

`本章用到的模块全在标准库里吗 → False`，缺的那一项是 `importlib.util`。但本章并没有少装东西：
`sys.stdlib_module_names` 是**顶层名字**的集合（里面有 `importlib`），点子包 `importlib.util` 不在里面，
所以这个 `False` 是判据写错，不是结论。`venv` 自己在标准库里 = True —— 而「模块在」和「本机建得出带 pip 的
venv」是两件事（第 16 节就撞在这上面）。同一节还数了 `~/.local/lib` 和 `~/.config` 的条目数当基线，
第 22 节再数一遍做对比：屏幕上只有计数和一个布尔，没有你家目录里的任何文件名。

**这一节带走：**

- 判「我在不在 venv 里」只用 `sys.prefix != sys.base_prefix`，别去比对一串路径 —— 路径里全是每换机器就变的噪音。
- `PYTHONPATH` / `PYTHONHOME` / `PYTHONNOUSERSITE` / `PYTHONSAFEPATH` 这四个能整条改写 `import` 的行为；
  做对照实验之前先把它们拨到未设，否则你看到的「哪一层给的」是上一行命令留下的幻觉。
- `site.ENABLE_USER_SITE` 是**声称**，`sys.path` 里有没有那条家目录才是**事实**（第 19 节实测这两者可以不一致）。
- 本机包目录叫 `dist-packages`，`sys.path` 第 0 项是空串 —— 后面所有「找不到」「找错」都从这两行出发。
- 判据本身也会写错：`importlib.util not in sys.stdlib_module_names` 是集合口径问题，不代表环境缺件。

---

## 第 02 节 · a 篇完整跑：`import x` 那一刻到底发生了什么（模块体只跑一次）

这一节把主线① 的第一本账 `sys.modules` 演透。道具 `a_import_basics.py` 在沙箱里现造一个 `once.py`，
它的模块体第一行就往 `run.log` 追加一个 `R`，再 `print` 一句标记 —— 于是「模块体跑了几遍」这件事
从「你信我」变成「屏幕上数」。

先跑第一段：同一个进程里 `import once` 三次。

```
$ python3 a_import_basics.py
=== 1. 模块体只跑一次：跑几遍 import，计数器只涨一格 ===
  现场：import-basics-demo/app/once.py —— 模块体第一行就往 run.log 追加一个 R，再 print 一句标记。
  子进程 = python3（只打 basename）+ cwd=沙箱 + 干净 env；下面 │ 开头的都是它的原始输出
  │ [once.py 的模块体正在执行]
  │ ① 同一进程里 import 三次之后，run.log 的 R 个数 = 1
  │ ② once is sys.modules['once'] → True
  │ ③ vars(once) is vars(sys.modules['once']) → True
  │ ④ 改一次，两边同时看到：once.VALUE = 99 | sys.modules 那份 = 99
  │ [once.py 的模块体正在执行]
  │ ⑤ 把 sys.modules 里那条删掉再 import → R 个数 = 2
  │ ⑥ 模块体重跑把 VALUE 重置回 42 （第 ④ 步的 99 被冲掉了）
  │ [once.py 的模块体正在执行]
  │ ⑦ 显式 reload 也重跑模块体 → R 个数 = 3
  │ ⑧ 但 reload 不换对象：还是同一个模块吗 → True
  └ 退出码 = 0
  跑完计数文件实际内容 = 'RRR'（3 个 R）
```

从上往下读这九行：

- `[once.py 的模块体正在执行]` 在整个屏幕的这一段里**只出现一次**，而 `import` 写了三次。
- ① `run.log` 的 `R` 个数 = 1：计数文件是最硬的证据，模块体只跑过一遍。
- ②③ `once is sys.modules['once']` 和 `vars(once) is vars(sys.modules['once'])` 都是 `True`：
  你手里那个名字和账本里那条是**同一个对象**，不是两份拷贝。
- ④ 改 `once.VALUE = 99`，两边同时看到 99 —— 这是「单例」的直接后果，也是第 5 段 monkeypatch 能生效的原因。
- ⑤ 把 `sys.modules['once']` 删掉，再 `import` 一次：`R` 个数变 2，模块体又跑了一遍。**同一个文件、同一个进程**，
  唯一变的是账本。这一行把「只跑一次」的原因从文件系统上摘下来，安到 `sys.modules` 上。
- ⑥ 重跑把 `VALUE` 冲回 42 —— 第 ④ 步那个 99 没了。
- ⑦⑧ 显式 `reload` 同样重跑（`R` = 3），但「还是同一个模块吗 → True」：重跑的是**内容**，换的不是**对象**。
- 最底下 `跑完计数文件实际内容 = 'RRR'` 是这段全部三次执行的对账。

「只跑一次」的准确说法是**每个进程一次**，不是每个文件一次。换一个进程立刻重来：

```
=== 2. 换一个进程：同一个文件，模块体又从头跑一遍 ===
  │ [once.py 的模块体正在执行]
  │ ⑨ 全新进程第一次 import → run.log 的 R 个数 = 4
  └ 退出码 = 0
```

⑨ 之后 `R` 个数 = 4（前面三遍加这一遍）。所以想让状态跨进程共享，模块变量一定不是答案，
必须是文件、数据库、环境变量这类进程外的东西。

接下来是四种写法各自往**谁的**命名空间里放了**什么**：

```
=== 3. import x 与 from x import y：往谁的命名空间里放了什么 ===
  │ [once.py 的模块体正在执行]
  │ ① import once                → 本地新增 ['once']
  │ ② from once import VALUE     → 本地新增 ['VALUE']
  │ ③ import pk.sub.leaf         → 本地新增 ['pk']
  │    但 sys.modules 里进了三个: ['pk', 'pk.sub', 'pk.sub.leaf']
  │ ④ import once as od          → 本地新增 ['od']
  │ ⑤ del once 之后写 once.VALUE → NameError: name 'once' is not defined
  │ ⑥ 'once' 还在 sys.modules 里吗 → True | 从账本里拿值：sys.modules['once'].VALUE = 42
  │ ⑦ from 进来的那个名字不受 del 影响：VALUE = 42 | 它是 int 吗 → int
  └ 退出码 = 0
```

- ①②④ 三种写法各绑一个名字：`import once` → `once`、`from once import VALUE` → `VALUE`、`import once as od` → `od`。
- ③ 是这条线里最容易记错的一条：`import pk.sub.leaf` 本地只新增 `['pk']`，但 `sys.modules` 里进了**三个**键。
  `b`、`c` 没进你本地，是挂在父包对象上的属性（第 7 节第 2 段专门演这件事）。
- ⑤⑥ `del once` 之后写 `once.VALUE` 给 `NameError`，可 `'once'` 还在 `sys.modules` 里，从账本照样取到 42。
  **`del` 删的是你那根线，不是模块。**
- ⑦ `from` 进来的那个名字不受 `del` 影响：`VALUE` 还是 42，类型是 `int`。
  这就是「为什么我的补丁没生效」的另一半答案：你 `from` 拿到的是当时的引用，不是实时视图。

再看模块对象本身 —— 那些双下划线的字段是从哪来的：

```
=== 4. 模块对象身上的字段都是从哪来的 ===
  │ ① type(money) = module | isinstance(money, types.ModuleType) → True
  │ ② money.__name__ = 'money' | __package__ = '' | 有 __path__ 吗 → False
  │    同一个键在包里就不一样：pk.__package__ = 'pk' | pk.sub.leaf.__package__ = 'pk.sub' | leaf.__name__ = 'pk.sub.leaf'
  │ ③ money.__doc__ = 'money.py：默认值写成模块级的账本，第 5 段拿它做「改全局状态」的实验。'
  │    只有「第一条语句是字符串」才算 doc：赋值之后 → None
  │    注释不顶位置：注释后那条字符串 → '注释后面的这条字符串才是第一条语句 → 它就是 __doc__'
  │ ④ money.__file__ = import-basics-demo/app/money.py
  │    money.__cached__ = import-basics-demo/app/__pycache__/money.cpython-314.pyc
  │    __spec__.origin = import-basics-demo/app/money.py | __spec__.submodule_search_locations = None
  │ ⑤ __loader__ 的类型 = SourceFileLoader | __spec__ 的类型 = ModuleSpec
  │ ⑥ money.__dict__ 类型 = dict | vars(money) is money.__dict__ → True
  │    __dict__ 的键（排序） = ['RATE', '__builtins__', '__cached__', '__doc__', '__file__', '__loader__', '__name__', '__package__', '__spec__', 'fee_fixed', 'fee_live', 'total']
  │ ⑦ 模块里的 __builtins__ = dict | is builtins.__dict__ → True | is builtins 模块 → False
  │    __main__（脚本本体）里的 __builtins__ = module | is builtins 模块 → True
  │    exec(源码, {}) 里的 __builtins__ = dict | len = 159
  │    这个键不是脚本自己写的：money.py 里一个字没提它，⑥ 的键表里却有 → 执行模块体那一刻由解释器塞进 __dict__
  │ ⑧ __pycache__ 里落下的文件 = ['doc_after_comment.cpython-314.pyc', 'doc_after_stmt.cpython-314.pyc', 'money.cpython-314.pyc', 'once.cpython-314.pyc']
  │    sys.flags.dont_write_bytecode = 0 | pyc 的名字跟着实现+版本走，所以它不是可移植的产物
  └ 退出码 = 0
```

- ① `type(money) = module`，`isinstance(money, types.ModuleType) → True`：模块是普通对象，没有隐藏的存储。
- ② 顶层模块的 `__package__` 是空串，包里的模块是包名（`pk.sub.leaf.__package__ = 'pk.sub'`）；
  普通模块**没有** `__path__`，这个键是包才有的（第 7 节就靠这三个键区分三种东西）。
- ③ `__doc__` 只认「第一条**语句**是字符串」：赋值之后的那条不算，而注释不顶位置 ——
  注释后面那条字符串才是第一条语句，所以它就是 `__doc__`。
- ④ `__file__` / `__cached__` / `__spec__.origin` 三个值互相指：源码路径由 finder 算出来，
  缓存路径是从它推出来的（`.pyc` 那个名字的机制在第 11 节）。
- ⑤⑥ `__loader__` 是 `SourceFileLoader`、`__spec__` 是 `ModuleSpec`，`vars(money) is money.__dict__ → True`。
  ⑥ 那份键表里有一个 `__builtins__`，而 `money.py` 里一个字没提它 —— 它是执行模块体那一刻被塞进去的。
- ⑦ 这一格最该记住：**同一个键的形状不保证**。被 `import` 的模块里 `__builtins__` 是 `dict`（而且
  `is builtins.__dict__` 为 `True`），`__main__` 里它却是 builtins **模块**，`exec(源码, {})` 的裸 dict 里又是 `dict`
  （那份有 159 个名字）。所以别写 `__builtins__['len']`，要用就 `import builtins` 再 `builtins.len`。
- ⑧ `__pycache__` 里落下 4 个 `.cpython-314.pyc`，`sys.flags.dont_write_bytecode = 0` —— 文件名跟着**实现+版本**走，
  所以它不是可移植产物。

模块是对象这件事，实用价值全在下面这格：

```
=== 5. 模块就是对象：getattr 取得到函数，赋值改得动全局状态 ===
  │ ① 函数就是个属性：getattr(m, 'fee_live')(100) = 5.0
  │    getattr(m, 'RATE') = 0.05
  │ ② m.fee_fixed.__defaults__（def 那一刻冻住的） = (0.05,)
  │ ③ 赋值 m.RATE = 0.99 之后：fee_fixed(100) = 5.0 | fee_live(100) = 99.0
  │    模块内部的 total(100)（它自己调 fee_live） = 104.0
  │ ④ 把 m.fee_live 换成假的，total(100) = 5.0 ← monkeypatch 的全部原理
  │    手动还原后 total(100) = 104.0
  │ ⑤ 赋值就是改 __dict__：m.__dict__['RATE'] = 0.99 | 另一个引用看到的也是新值 → 0.99 | 同一对象 → True
  └ 退出码 = 0
```

- ① `getattr(m, 'fee_live')(100) = 5.0`：函数就是模块上的一个普通属性，取得到、也换得掉。
- ② `m.fee_fixed.__defaults__ = (0.05,)` —— 这是 `def` 那一刻冻住的默认值（第 05 章那条规矩在这里落地）。
- ③ 改 `m.RATE = 0.99` 之后：`fee_live(100)` 从 5.0 变 **99.0**，模块**内部**的 `total(100)` 也跟着变 104.0；
  而 `fee_fixed(100)` 纹丝不动还是 5.0。**改得动的是「运行时现查的全局名」，改不动的是「定义时就绑好的默认值」。**
- ④ 把 `m.fee_live` 换成假的，`total(100)` 立刻变 5.0 —— 「这就是 monkeypatch 的全部原理」；手动还原后回到 104.0。
  能被替换的前提是模块内部**通过全局名调用它**，而不是 `def` 时就抓走了。
- ⑤ `m.__dict__['RATE'] = 0.99` 和 `m.RATE = 0.99` 是同一件事，另一个引用看到的也是新值，而且 `is` 同一个对象。

最后一格把四种「导不到」的报错原文一个字不改地抄下来 —— 半夜看到它们时，能直接反推出原因的那种：

```
=== 6. 四种「导不到」的报错原文（一个字都别改地抄下来）===
  │ ① True ← ModuleNotFoundError 是 ImportError 的儿子（第 07 章那棵树）
  │ ② ModuleNotFoundError: No module named 'totally_absent_xyz'
  │    exc.name = 'totally_absent_xyz' | exc.path = None | exc.msg = "No module named 'totally_absent_xyz'"
  │ ③ ModuleNotFoundError: No module named 'pk.sub.absent_leaf' | exc.name = 'pk.sub.absent_leaf'
  │ ④ ImportError: cannot import name 'NOT_THERE' from 'pk' (import-basics-demo/app/pk/__init__.py)
  │    exc.name = 'pk' | exc.path = 'import-basics-demo/app/pk/__init__.py' ← 注意 name 装的是「从谁那儿导入」，不是缺的那个名字
  │ ⑤ ImportError: cannot import name 'also_missing' from 'money' (import-basics-demo/app/money.py)
  │ ⑥ ImportError: attempted relative import with no known parent package
  │ ⑦ AttributeError: module 'starbad' has no attribute 'ghost' ← __all__ 里写了不存在的名字
  │    这次 exec 之后 ns 里有东西吗 → ['present'] ← 炸之前那半个已经绑上了
  │ ⑧ ImportError: cannot import name 'missing_one' from 'money' (import-basics-demo/app/money.py)
  │    多名字的 from 失败后 ns 里留下的是 → ['RATE']
  └ 退出码 = 0
```

- ① `ModuleNotFoundError 是 ImportError 的儿子` 为 `True`，所以 `except ImportError` 一网打得住 ②③④⑤⑥。
- ②③ 都是「没这个模块」，区别在报错写的是**全名**：`'pk.sub.absent_leaf'` 而不是最后那段 `absent_leaf`，
  只盯尾巴会查错方向。
- ④⑤ 是**模块存在、名字不存在**：`ImportError: cannot import name 'NOT_THERE' from 'pk' (…/__init__.py)`。
  注意 `exc.name` 装的是「从谁那儿导」，`exc.path` 才是那个文件 —— 读反了白查半小时。
- ⑥ 在没有父包的地方写相对导入：`attempted relative import with no known parent package`（机制在第 7 节第 5 段）。
- ⑦ `__all__` 里写了一个不存在的名字，炸出来的是 **`AttributeError`**，`except ImportError` 接不住它。
- ⑦⑧ 共同的代价：失败**之前**已经绑上的名字留在原地（`ns` 里剩 `['present']` / `['RATE']`）。
  **一个失败的 import 能给你留下半套状态。**

**这一节带走：**

- 模块体「只跑一次」的开关是 `sys.modules`，不是文件系统：删掉那条立刻重跑，`reload` 也重跑但**不换对象**。
  已经 `from x import y` 拿到旧引用的名字不会跟着更新 —— 热重载框架存在的全部理由就在这三行里。
- 「一次性」的准确说法是**每进程一次**；跨进程共享状态别用模块变量。
- `import x` 只在你本地绑一个名字（`import a.b.c` 只绑 `a`），`del` 只断本地这根线，模块对象照旧活着。
- 模块 = 一个 `types.ModuleType` 对象 + 一个 `dict`，`vars(m) is m.__dict__` 为真；
  所以「改模块属性」= 改它的运行时默认值 = **污染全局**，测试里必须成对地存旧值再还原。
- 报错分两族：没这个模块 / 没这个名字（带文件位置）/ 没这个父包，都能用 `except ImportError` 接；
  只有 `from x import *` 撞上谎报的 `__all__` 会给 `AttributeError`。

---

## 第 03 节 · 小证据：第一本账 sys.modules —— 命中就什么都不做，撕掉就立刻重跑

第 2 节用计数文件证明「模块体只跑一遍」。这一格把它推到极端：**账本命中的时候，连文件都不打开**。
手法很直接 —— 手工造一个同名假模块塞进 `sys.modules`，看 `import` 会不会发现它是假的。

```
$ mkdir -p ledger-check && cd ledger-check
printf "V = %s\nprint(%s)\n" "\"from-file\"" "\"[BODY] shadow.py 的模块体跑了一遍\"" > shadow.py
python3 -c "
import sys, os
name = \"shadow\"
mod = type(sys)(name); mod.V = \"凭空塞的假模块\"; mod.__file__ = None
sys.modules[name] = mod
import shadow
print(\"① 账本里先塞一个同名假模块 → import 直接拿到它吗 →\", shadow.V == \"凭空塞的假模块\")
print(\"   模块体跑过了吗 →\", os.path.exists(\"[BODY] 这行没出现就是没跑\") or \"没跑（屏幕上没有 [BODY] 那一行）\")
del sys.modules[name]
import shadow
print(\"② 把账本里这一条撕掉 → 同一个名字立刻按 sys.path 找到文件、重跑模块体：V =\", shadow.V)
import shadow
print(\"③ 第二次 import 又什么都不做（账本又命中了）→ V 还是\", shadow.V)
m = sys.modules[\"shadow\"]
del shadow
print(\"④ 只 del 本地名字：名字没了吗 →\", \"shadow\" not in globals(), \"｜ 账本里模块对象还活着吗 →\", \"shadow\" in sys.modules, \"｜ 是同一个对象吗 →\", sys.modules[\"shadow\"] is m)
print(\"   再 import 一次（账本命中）→ 模块体又跑了吗 →\", \"屏幕上没有第二遍 [BODY] 就是没跑\")
"
cd ..; rm -rf ledger-check
① 账本里先塞一个同名假模块 → import 直接拿到它吗 → True
   模块体跑过了吗 → 没跑（屏幕上没有 [BODY] 那一行）
[BODY] shadow.py 的模块体跑了一遍
② 把账本里这一条撕掉 → 同一个名字立刻按 sys.path 找到文件、重跑模块体：V = from-file
③ 第二次 import 又什么都不做（账本又命中了）→ V 还是 from-file
④ 只 del 本地名字：名字没了吗 → True ｜ 账本里模块对象还活着吗 → True ｜ 是同一个对象吗 → True
   再 import 一次（账本命中）→ 模块体又跑了吗 → 屏幕上没有第二遍 [BODY] 就是没跑
[退出码 0]
```

- ① `import 直接拿到它吗 → True`：`import shadow` 拿到的正是我们凭空 `type(sys)(name)` 造出来那个，
  磁盘上真有一个 `shadow.py` 它看都不看。
- 「模块体跑过了吗」那行下面**没有** `[BODY]` —— 这才是真证据：不是脚本说没跑，是屏幕上确实少那一行。
- ② `del sys.modules[name]` 之后，`[BODY] shadow.py 的模块体跑了一遍` 当场冒出来，`V = from-file`：
  撕掉账本那一条，同一个名字立刻按 `sys.path` 找到文件、重跑模块体。
- ③ 再 `import` 一次又什么都不做（屏幕上没有第二个 `[BODY]`），`V` 还是 `from-file` —— 账本重新填好了。
- ④ 只 `del shadow`（本地名字）：名字没了 `True`、账本里对象还活着 `True`、而且**是同一个对象** `True`。

④ 这一行是「为什么我的补丁没生效」的标准答案，也是第 2 节第 5 段那条的镜像：
**改模块属性 = 改全局状态**（所有引用者同时看到），
**`del` 一个名字 = 只删你这一格的绑定**（账本里的对象谁都能再拿回来，`import` 一次就复活）。

**这一节带走：**

- `import` 的第一入口是 `sys.modules` 查表，命中就**连文件都不打开** —— 连模块体里那句 `print` 都不会出现。
- 想强制重跑只有两条路：删掉账本里那一条，或者 `reload`（对象不换，只重跑内容）。
- 「名字没了」和「模块没了」是两件事，前者归 `globals()`，后者归 `sys.modules`，两个地方要分别查。
- 这个手法本身就是打桩的最低成本实现：不需要真的写文件，一个 `ModuleType` 对象就能占住一个 import 名字。

---

## 第 04 节 · 小证据：主线② —— sys.path[0] 的形状由启动方式决定

第 5 节有一整张七行的实测表。这一节只把最容易被忽略的那一件单独钉死：
**「根在哪」这个问题，七种入口给出四种形状**，而代码可以一个字都没改。

先在沙箱里造一个 `pkgx/x.py`，它打印两件事：`sys.path[0]` 折成什么、以及它指不指向脚本自己所在的目录：

```
$ mkdir -p shape-check/pkgx && cd shape-check
cat > pkgx/x.py <<"PYEOF"
import os, sys
def fold(p):
    return str(p).replace(os.getcwd(), "<cwd>").replace(os.environ["SB"], "<沙箱>")
tag = sys.argv[1] if len(sys.argv) > 1 else "?"
print("[入口 %-24s] path0 = %r" % (tag, fold(sys.path[0])))
print("                     它指向脚本所在目录吗 →", os.path.samefile(sys.path[0], os.path.dirname(os.path.abspath(__file__))))
PYEOF
python3 pkgx/x.py "python3 pkgx/x.py"
python3 -m pkgx.x "python3 -m pkgx.x"
python3 -c "import sys; print(\"[入口 %-24s] path0 = %r\" % (\"python3 -c\", sys.path[0])); print(\"                     是空串吗 →\", sys.path[0] == \"\", \"｜ 空串不是路径，是「每次 import 现查当前目录」\")"
python3 - <<"PYEOF"
import sys
print("[入口 %-24s] path0 = %r" % ("python3 - 走 stdin", sys.path[0]))
print("                     是空串吗 →", sys.path[0] == "")
PYEOF
python3 -P -c "import sys; print(\"[入口 %-24s] path0 = %r | 空串已经不在账上了吗 →\" % (\"python3 -P -c\", sys.path[0]), \"\" not in sys.path)"
python3 -I -c "import sys; print(\"[入口 %-24s] path0 = %r | 空串已经不在账上了吗 →\" % (\"python3 -I -c\", sys.path[0]), \"\" not in sys.path)"
python3 <<"PYEOF"
import sys
print("[入口 %-24s] path0 = %r" % ("python3 无参数（stdin 当脚本）", sys.path[0]))
print("                     是空串吗 →", sys.path[0] == "")
PYEOF
cd ..; rm -rf shape-check
```

七条命令就是七种入口：`python3 pkgx/x.py`、`python3 -m pkgx.x`、`python3 -c`、`python3 -` 走 stdin、
`python3 -P -c`、`python3 -I -c`、`python3` 无参数把 stdin 当脚本。`fold()` 把当前目录折成 `<cwd>`、
把沙箱前缀折成 `<沙箱>`，所以屏幕上没有随机名。

```
[入口 python3 pkgx/x.py       ] path0 = '<cwd>/pkgx'
                     它指向脚本所在目录吗 → True
[入口 python3 -m pkgx.x       ] path0 = '<cwd>'
                     它指向脚本所在目录吗 → False
[入口 python3 -c              ] path0 = ''
                     是空串吗 → True ｜ 空串不是路径，是「每次 import 现查当前目录」
[入口 python3 - 走 stdin       ] path0 = ''
                     是空串吗 → True
[入口 python3 -P -c           ] path0 = '/usr/lib/python314.zip' | 空串已经不在账上了吗 → True
[入口 python3 -I -c           ] path0 = '/usr/lib/python314.zip' | 空串已经不在账上了吗 → True
[入口 python3 无参数（stdin 当脚本）  ] path0 = ''
                     是空串吗 → True
[退出码 0]
```

四种形状，一条一条看：

- `python3 pkgx/x.py` → `path0 = '<cwd>/pkgx'`，「指向脚本所在目录吗 → True」。**根跟着文件走**。
- `python3 -m pkgx.x` → `path0 = '<cwd>'`，同一个问题答 **False**。**根是当前目录**，不是文件目录。
  这两行并排就是主线② 的全部：同一份 `x.py`，跑法一换，它能看见的兄弟模块集合就换了。
- `-c`、`-`（管道喂代码）、无参数这三种入口给的都是**空串** `''`。
  空串不是一条路径，是「每次 `import` 现查当前目录」—— 第 6 节把这句话推到极端。
- `-P -c` 和 `-I -c` 两行一模一样：`path0 = '/usr/lib/python314.zip'`，而且「空串已经不在账上了吗 → True」。
  `-P`（等价于 `PYTHONSAFEPATH=1`）是把入口那一项**整个不给**，于是第 0 项直接落到解释器自带的 zip 上；
  `-I` 从 3.11 起自带 `-P`，所以它的结果和 `-P` 一致。这也正是第 1 节要先把 `PYTHONSAFEPATH` 拨到未设的原因。

**这一节带走：**

- 「旁边那个模块能不能 import 到」由**你怎么启动**决定，不由代码决定 —— 七种入口四种形状。
- 脚本跑法的根是**文件所在目录**，`-m` 的根是**当前目录**：这是日常「IDE 里能跑、命令行不能跑」的分水岭。
- `-c` 和管道给的是空串；空串会跟着 `chdir` 漂（第 6 节）。不想要它，`-P` 或 `PYTHONSAFEPATH=1` 一整项删掉。
- `-I` 不是「更严格的隔离」的新东西，它就是 `-P`（3.11 起）加上忽略若干环境变量的组合。

---

## 第 05 节 · b 篇完整跑：sys.path 是四层拼出来的，同名遮蔽按层发生

这是本章最长的一篇取证（`b_sys_path_shape.py` 的七个段），回答主线① 的第二本账：
`sys.path` 里的每一项是**谁**在**什么时候**放进去的，以及「找不到」和「找错」为什么是同一张表的两面。

第一段先认清对象 —— 它不是一个只读的配置，是一个能被现场改写的普通 list：

```
=== 1. 先认清对象：sys.path 是一个能被现场改写的普通 list ===
  本机 Python 3.14.4（只打版本号）｜子进程解释器 = python3（只打 basename）。
  · sys-path-demo/                           ← ① 入口给的：脚本所在目录 / 当前目录（绝对路径）
  · <py>/lib/python314.zip                   ← ③ zip 位：标准库的打包入口（磁盘上根本没有也照样列着）
  · <py>/lib/python3.14/                     ← ③ 标准库 .py（安装时定好的默认项）
  · <py>/lib/python3.14/lib-dynload/         ← ③ 标准库的 C 扩展目录（.so）
  · <py>/local/lib/python3.14/dist-packages/ ← ③+ 这一项是 site 加的（下面第 2 段用 -S 单独证）
  · <py>/lib/python3/dist-packages/          ← ③+ 这一项是 site 加的（下面第 2 段用 -S 单独证）
  一共 6 项，全是字符串；顺序就是查找顺序，**前面的赢**。
  注意这张列表里**找不到第 ④ 层**（内置/冻结）：它不在 sys.path 里，第 2 段单独演。
  │ 起点：类型 = list | 项数 = 6
  │ ① append 一个相对目录之后 import rootmod → A/rootmod.py 那一份
  │    rootmod.__file__ = sys-path-demo/A/rootmod.py
  │ ② 把它移出 sys.path 并清掉 sys.modules → ModuleNotFoundError: No module named 'rootmod'
  │ ③ insert(0, 'B')（B 排在 A 前无所谓，A 已经移出去了）→ B/rootmod.py 那一份
  │ ④ 再 insert(0, 'A') → A/rootmod.py 那一份 ← 谁在前谁赢
  └ 退出码 = 0
```

- 开头那六行是**这一篇看到的账本形状**：`sys-path-demo/`（① 入口给的）、`<py>/lib/python314zip` 那三项（③ 标准库）、
  两项 `dist-packages`（③+ site 加的）。注意这里安装前缀被折成了 `<py>`，所以它和第 1 节那行
  `/usr/lib/python314.zip` 是同一个东西的两种写法，不是两份路径。
- 「一共 6 项，全是字符串；顺序就是查找顺序，**前面的赢**」，而且这张列表里**找不到第 ④ 层**（内置/冻结）——
  它不在 `sys.path` 里，第 2 段单独演。
- ①②③④ 四步是现场改写：`append` 一个相对目录进去，`import rootmod` 立刻拿到 `A/rootmod.py`；
  把它移出 `sys.path` **并清掉 `sys.modules`**，才变成 `ModuleNotFoundError`；
  `insert(0,'B')` → 拿 B；再 `insert(0,'A')` → 拿 A。**谁排前面谁赢**。
- 代价写在最后两行：运行时改 `sys.path` 是全局副作用，而且**子进程不继承**这件事 ——
  「我这能跑」不能跨进程外推。

第二层的账最容易背错：`PYTHONPATH` 的项到底插在哪、谁赢。四格对照：

```
  ② PYTHONPATH：同一份 pp.py，只换 env 里这一项（相对值也认，按当前目录解析）
    · PYTHONPATH=A:B                 前 4 项 = 'sys-path-demo' | 'sys-path-demo/A' | 'sys-path-demo/B' | '<py>/lib/python314.zip'
                                     赢的是 A/rootmod.py 那一份 ｜ 文件 = sys-path-demo/A/rootmod.py
    · PYTHONPATH=B:A                 前 4 项 = 'sys-path-demo' | 'sys-path-demo/B' | 'sys-path-demo/A' | '<py>/lib/python314.zip'
                                     赢的是 B/rootmod.py 那一份 ｜ 文件 = sys-path-demo/B/rootmod.py
    · PYTHONPATH=.（当前目录）             前 4 项 = 'sys-path-demo' | 'sys-path-demo' | '<py>/lib/python314.zip' | '<py>/lib/python3.14'
                                     赢的是 ModuleNotFoundError: No module named 'rootmod' ｜ 文件 = （没导到）
    · PYTHONPATH=A:A（同一项写两遍）         前 4 项 = 'sys-path-demo' | 'sys-path-demo/A' | '<py>/lib/python314.zip' | '<py>/lib/python3.14'
                                     赢的是 A/rootmod.py 那一份 ｜ 文件 = sys-path-demo/A/rootmod.py
```

- `A:B` 和 `B:A` 那两行换的就是赢家：`PYTHONPATH` 的项排在**第 0 项之后、标准库之前**，顺序照抄你写的，第一项赢。
- `PYTHONPATH=.` 那行有个容易忽略的细节：`.` 被展开成当前目录的**绝对路径**，于是前两项
  `'sys-path-demo'` 和 `'sys-path-demo'` **一模一样地重复** —— 去重只发生在 `PYTHONPATH` 自己内部，跨层不去重。
- `A:A` 那行则相反：同一个目录写两遍只留一份。两条规则不是一套，别把它们合成一句「会去重」。

第三层是 site 加的，也是 venv 动手的那一层。同一个探针，加不加 `-S`：

```
  ③ site 那一层：同一个探针，加与不加 -S
    · 正常：6 项 ｜ 'site' in sys.modules = True
    · 加 -S：4 项 ｜ 'site' in sys.modules = False
      -S 一关就少掉：<py>/local/lib/python3.14/dist-packages/
      -S 一关就少掉：<py>/lib/python3/dist-packages/
    · sys.meta_path 正常 = DistutilsMetaFinder,BuiltinImporter,FrozenImporter,PathFinder
      sys.meta_path -S   = BuiltinImporter,FrozenImporter,PathFinder
      → site 这一层不只是往 sys.path 里加目录：本机 dist-packages 里有个 .pth 文件，
        它还往 sys.meta_path 里塞了一个 finder（上面多出来的那个名字）。第三方能改查找过程本身。
```

- 正常 6 项、`-S` 之后 4 项，少掉的正是那两条 `dist-packages`；`'site' in sys.modules` 从 `True` 变 `False`。
- 更值得记的是最后两行：`sys.meta_path` 正常是四项，`-S` 之后少一个 `DistutilsMetaFinder`。
  **site 这一层不只是往 `sys.path` 里塞目录，它还往查找流程里塞 finder** ——
  第三方能改的是「怎么找」这件事本身，不只是「在哪找」。这条在第 19 节会被放大成 `.pth` 的安全面。

第四层完全不在 `sys.path` 上，所以它是最容易被误当成「我遮蔽成功了」的一层：

```
  ④ 内置/冻结那一层：造假 time.py 放进 PYTHONPATH，看它能不能冒充内置的 time
  │ IN_BUILTIN=True
  │ BUILTIN_N=64 个，样例 _abc,_ast,_bisect,_blake2,_codecs,_collections
  │ FAKE_WON=False
  │ TIME_ORIGIN=built-in
  │ TIME_FILE=AttributeError: module 'time' has no attribute '__file__'
  │ ZIP=1 项 zip，磁盘上真存在的 0 项
  │ FROZEN=frozen | 它的 __name__ = _frozen_importlib | 有 __file__ 吗 = False
```

假 `time.py` 就躺在 `sys.path[1]`（`PYTHONPATH` 给的位置，排在标准库之前），结果：
`FAKE_WON=False`、`TIME_ORIGIN=built-in`、`TIME_FILE` 是 `AttributeError: module 'time' has no attribute '__file__'`。
**内置层排在所有路径之前，`x in sys.builtin_module_names` 为 True 的名字遮蔽不了**，内置模块连 `__file__` 都没有。
别在「覆盖 `sys`」这件事上耗时间。（顺带一条：`ZIP=1 项 zip，磁盘上真存在的 0 项` —— 账本上那条 zip 是列着的，
哪怕这台机器上根本没有那个文件。）

第 3 段是主线② 的整张实测表，七种入口 × 五个观察位：

```
=== 3. 四种入口往 sys.path[0] 里放的不是同一个东西（实测表）===
  入口                                      sys.path[0]（掩码后）      argv[0]                     __package__  import neighbor
  · python3 sroot/entry.py                  sys-path-demo/sroot/     sroot/entry.py            None         → sroot 里的邻居模块
  · python3 -m sroot.entry                  sys-path-demo/           sys-path-demo/sroot/entry.py 'sroot'      ★ ModuleNotFoundError: No module named 'neighbor'
  · python3 -c <同一段代码>                      ''（空串）                   -c                        None         ★ ModuleNotFoundError: No module named 'neighbor'
  · python3 - < sroot/entry.py              ''（空串）                   -                         None         ★ ModuleNotFoundError: No module named 'neighbor'
  · python3 entry.py（cwd 换成 sroot/）         sys-path-demo/sroot/     entry.py                  None         → sroot 里的邻居模块
  · python3 -P sroot/entry.py               <py>/lib/python314.zip   sroot/entry.py            None         ★ ModuleNotFoundError: No module named 'neighbor'
  · PYTHONSAFEPATH=1 python3 sroot/entry.py <py>/lib/python314.zip   sroot/entry.py            None         ★ ModuleNotFoundError: No module named 'neighbor'
  · python3 -P -m sroot.entryfix → 退出码 1，当前目录整个不给了：
    │ python3: Error while finding module specification for 'sroot.entryfix' (ModuleNotFoundError: No module named 'sroot')
```

读最后一列 `import neighbor`（同目录的兄弟模块能不能拿到）就知道这表为什么重要：

- 第 1 行 `python3 sroot/entry.py` → 根是 `sroot/`，**拿得到**；第 5 行把 cwd 换成 `sroot/` 再跑，根还是 `sroot/`，
  照样拿得到 —— 证明脚本跑法的根**跟着文件走**。
- 第 2 行 `python3 -m sroot.entry` → 根变成 `sys-path-demo/`，`import neighbor` 变 `ModuleNotFoundError`。
- 第 3、4 行 `-c` 和 `-`（stdin）给空串，同样拿不到（它们查的是**进程当前目录**，不是文件目录）。
- 第 6、7 行 `-P` 与 `PYTHONSAFEPATH=1` 是同一件事的两种写法，结果一致。
- 最后一行最狠：`python3 -P sroot/entryfix` 连包都找不着了 ——
  `Error while finding module specification for 'sroot.entryfix' (ModuleNotFoundError: No module named 'sroot')`，
  退出码 1。**`-P` 关掉的是「当前目录」这一项**，所以用 `-m` 找顶层包之前，先确认根还在不在。

第 5 段把「同一份文件，两种跑法」单独钉一遍，因为它是日常事故的真形状：

```
=== 5. 同一份文件，两种跑法：兄弟模块一个拿得到、一个拿不到 ===
  磁盘上只有三个文件：sroot/entry.py、sroot/neighbor.py、A/rootmod.py（A/ 不在任何路径里）。
  · python3 sroot/entry.py     根 = sys-path-demo/sroot/
                               import neighbor → sroot 里的邻居模块
                               import rootmod  → ModuleNotFoundError: No module named 'rootmod'
  · python3 -m sroot.entry     根 = sys-path-demo/
                               import neighbor → ModuleNotFoundError: No module named 'neighbor'
                               import rootmod  → ModuleNotFoundError: No module named 'rootmod'
  · python3 -m sroot.entryfix  换成两种「写全」的写法，退出码 0：
    │ PATH0=sys-path-demo
    │ PKG='sroot'
    │ FULL=sroot 里的邻居模块
    │ DOT=sroot 里的邻居模块
```

- 磁盘上只有三个文件：`sroot/entry.py`、`sroot/neighbor.py`、`A/rootmod.py`（`A/` 不在任何路径里）。
- 脚本跑法：`import neighbor` 成功、`import rootmod` 失败。`-m` 跑法：两个都失败。
  **代码一个字没改**，换的只是入口。
- `-m` 报错写的是 `No module named 'neighbor'` 这种**光板名字**，不是「找不到 `sroot` 这个包」——
  这说明当前目录已经是根了，只是根里没有叫 `neighbor` 的文件。这一句读法能省掉半小时误诊。
- 最后一行给了两种当场跑通的修法：`import sroot.neighbor`（写全包名）和 `from . import neighbor`（相对导入），
  屏幕上 `FULL=` 和 `DOT=` 都拿到了同一份邻居模块。相对导入为什么只在后者成立，第 7 节第 5 段回答。

第 6 段是全章踩坑率最高的一格：一份 `json.py` 抢走标准库，**两种抢法、三层下场**。

```
=== 6. ★ 同名遮蔽：一份 json.py 抢走标准库，两种抢法、三层下场 ===
  基线（干净 env，什么都不加）：json.__file__ = <py>/lib/python3.14/json/__init__.py ｜ 它是包吗 → True
  · 抢法一（② 那一层）：PYTHONPATH=shad，入口脚本 jsoncheck.py 和假 json **不在**同一个目录
  │ FILE=sys-path-demo/shad/json.py
  │ WHO=沙箱里的假 json：它没有 dumps
  │ IS_PACKAGE=False
  │ HAS_DUMP=False
  │ DUMPS=AttributeError: module 'json' has no attribute 'dumps'
  │ SUB=ModuleNotFoundError: No module named 'json.tool'; 'json' is not a package
  └ 退出码 = 0
  · 抢法二（① 那一层）：假 json.py 一个字没挪，只是把入口脚本放进**同一个目录**：python3 peek.py
  │ FILE=sys-path-demo/shad/json.py
  │ WHO=沙箱里的假 json：它没有 dumps
  │ IS_PACKAGE=False
  │ HAS_DUMP=False
  │ DUMPS=AttributeError: module 'json' has no attribute 'dumps' (consider renaming 'sys-path-demo/shad/json.py' since it has the same name as the standard library module named 'json' and prevents importing that standard library module)
  │ SUB=ModuleNotFoundError: No module named 'json.tool'; 'json' is not a package
```

- 基线先钉住：`json.__file__ = <py>/lib/python3.14/json/__init__.py`，「它是包吗 → True」。
- 抢法一（走 `PYTHONPATH`，入口脚本和假 `json.py` **不在**同一个目录）：`FILE` 已经是假的、
  `IS_PACKAGE=False`、`HAS_DUMP=False`，`DUMPS` 给 `AttributeError: module 'json' has no attribute 'dumps'`，
  子模块给 `ModuleNotFoundError: No module named 'json.tool'; 'json' is not a package`。
- 抢法二（假文件一个字没挪，只是把**入口脚本放进同一个目录**）：前面四行一模一样，
  但 `DUMPS` 那条后面多了一长串括号：`consider renaming '…/shad/json.py' since it has the same name as
  the standard library module named 'json' and prevents importing that standard library module`。
  这条提醒是 3.12 起才有的，**而且只在被抢的文件躺在主脚本所在目录时出现** —— 抢法一（`PYTHONPATH`）没这个提醒，更难查。
- 两种抢法拿到的都是假的，连锁原因是同一个：假的那个是**模块**不是**包**（没有 `__path__`），
  所以真 `json` 的所有子模块一起没了。

再往下两格，说明「同一个遮蔽，换个入口连报错都不一样」，以及最狠的那一档：

```
  · 连标准库自己的入口都换了下场：python3 -m json.tool 退出码 1
    │ python3: Error while finding module specification for 'json.tool' (ModuleNotFoundError: __path__ attribute not found on 'json' while trying to find 'json.tool')
  · 最狠的一档：遮蔽**解释器启动阶段**就要用的模块（boom/encodings.py 里只有一句 raise）
    python3 -c 的退出码 = 1（开机就炸，你的代码一个字都没执行）：
    │   File "sys-path-demo/boom/encodings.py", line 1, in <module>
    │ RuntimeError: 沙箱里的假 encodings 模块被执行了
```

- `python3 -m json.tool` 退出码 1，报的是 `ModuleNotFoundError: __path__ attribute not found on 'json' while trying to find 'json.tool'`。
  和第 7 节那句 `'json' is not a package` 措辞很像，指向的原因不同：这一句是**找子模块时**发现父的 `__path__` 不存在。
- 遮蔽**解释器启动阶段**就要用的模块（`boom/encodings.py` 里只有一句 `raise`）：`python3 -c` 退出码 1，
  你的代码一个字都没执行就炸了，屏幕上只有 `File "sys-path-demo/boom/encodings.py", line 1` 和那句 `RuntimeError`。
  这条是「同名文件」能造成的最坏后果的下界。

本篇末尾还有一张「按层逐句问」的排查表（0 层查 `sys.modules`、1 层查入口、1b 查空串、2 查 `PYTHONPATH`、
3 查 site、3b 查同名遮蔽、4 查内置、5 查是不是包），每一行右边都标了本篇哪一段演过它 —— 原文没重贴，
`bash demo.sh` 跑到第 5 节最后一段就能看到。

**这一节带走：**

- `sys.path` 的来源就四层：**入口项**（第 0 项，形状由启动方式决定）、`PYTHONPATH`（插在入口之后、标准库之前）、
  标准库 + site、内置/冻结。顺序 = 优先级，所以「找不到」和「找错」是同一张表的两面。
- 内置/冻结那层不在 `sys.path` 上、排在所有路径之前，**遮蔽不了**；别拿它当试验田。
- 自己的脚本/目录名和标准库重名（`json`、`csv`、`random`、`site`、`platform`…）是最贵的一种 bug：
  截胡发生在 `sys.path[0]`，连 `PYTHONPATH` 都不用配。一秒自查：`python3 -c "import 名字; print(名字.__file__)`。
- 出「`module 'xxx' has no attribute …`」这种怪错，第一件事是打 `xxx.__file__` 看它到底是谁。
- 运行时改 `sys.path` 只影响当前进程，`PYTHONPATH` 却能穿透子进程 —— 所以对照实验必须每个子进程发一份干净 env。

---

## 第 06 节 · 小证据：第二本账 sys.path —— 顺序即优先级，空串还会跟着 chdir 换根

这一节只有两格，但它们是「import 不到」这件事的两个最底层事实：**顺序就是优先级**，
以及**报错不会告诉你任何一条它翻过的路径**。

先造两个同名不同内容的模块，把它们按不同顺序摆进 `sys.path`：

```
$ mkdir -p order-check/front order-check/back && cd order-check
printf "TAG = %s\n" "\"front\"" > front/winner.py
printf "TAG = %s\n" "\"back\""  > back/winner.py
python3 -c "
import sys
sys.path.insert(0, \"front\"); sys.path.insert(1, \"back\")
import winner
print(\"① front 排在 back 之前 → 拿到的是\", winner.TAG)
sys.path.remove(\"back\"); sys.path.insert(0, \"back\")
del sys.modules[\"winner\"]
import winner
print(\"② 换成 back 排最前，并撕掉账本那一条 → 拿到的是\", winner.TAG, \"（顺序即优先级，不是谁名字短谁赢）\")
sys.path.insert(0, \"front\")
import winner
print(\"③ 再把 front 挪回最前，但账本不撕 → 仍然是\", winner.TAG, \"（命中账本时连 sys.path 都不查——两本账的先后关系就在这）\")
print(\"④ 报错原文里只有名字，不给你任何一条搜索路径：\")
try:
    import no_such_mod_here
except ModuleNotFoundError as e:
    print(\"   \", type(e).__name__, str(e), \"｜ 提到模块名吗 →\", \"no_such_mod_here\" in str(e), \"｜ 提到任何一条搜索路径吗 →\", any(p in str(e) for p in sys.path if p))
"
cd ..; rm -rf order-check
① front 排在 back 之前 → 拿到的是 front
② 换成 back 排最前，并撕掉账本那一条 → 拿到的是 back （顺序即优先级，不是谁名字短谁赢）
③ 再把 front 挪回最前，但账本不撕 → 仍然是 back （命中账本时连 sys.path 都不查——两本账的先后关系就在这）
④ 报错原文里只有名字，不给你任何一条搜索路径：
    ModuleNotFoundError No module named 'no_such_mod_here' ｜ 提到模块名吗 → True ｜ 提到任何一条搜索路径吗 → False
[退出码 0]
```

- ① `front` 排在 `back` 之前 → 拿到 `front`。
- ② 换成 `back` 排最前，**并撕掉账本那一条** → 拿到 `back`。两个动作必须一起做，这正是 ③ 要教的东西。
- ③ 再把 `front` 挪回最前，但**账本不撕** → 仍然是 `back`。屏幕上那句判定就是两本账的先后关系：
  **命中账本时连 `sys.path` 都不查**。第 3 节把这条推到极端（连文件都不打开），这里把它接到优先级上。
- ④ 是这一节最实用的一条：`ModuleNotFoundError No module named 'no_such_mod_here'`，
  「提到模块名吗 → True」，「提到任何一条搜索路径吗 → **False**」。
  它不告诉你找过哪些目录，所以「import 不到」九成不是语法问题，而是「根在哪」的问题 ——
  得自己把 `sys.path` 摊开逐层问（第 5 节末尾那张表）。

第二格把「空串」这个键推到它的极限：**它不是启动时定死的当前目录**。

```
$ mkdir -p chdir-check/here chdir-check/there && cd chdir-check
printf "X = %s\n" "2" > there/somemod.py
python3 -c "
import os, sys
sys.path.insert(0, \"\")
root = os.getcwd()
print(\"① 空串这一项不是启动时定死的：chdir 之前，当前目录折成 <cwd> =\", os.getcwd().replace(root, \"<cwd>\"))
os.chdir(\"there\")
print(\"   os.chdir 之后 =\", os.getcwd().replace(root, \"<cwd>\"), \"｜ sys.path 一个字没改，账本却整个换根了\")
import somemod
print(\"   现在 import 到新根里的文件了吗 →\", somemod.X == 2)
os.chdir(\"..\")
try:
    del sys.modules[\"somemod\"]; import somemod
    print(\"   回到旧根再 import（撕过账本）→ 还能拿到吗 →\", somemod.X == 2)
except ModuleNotFoundError:
    print(\"   回到旧根再 import（撕过账本）→ 换根了，旧根下没有这个文件\")
"
cd ..; rm -rf chdir-check
① 空串这一项不是启动时定死的：chdir 之前，当前目录折成 <cwd> = <cwd>
   os.chdir 之后 = <cwd>/there ｜ sys.path 一个字没改，账本却整个换根了
   现在 import 到新根里的文件了吗 → True
   回到旧根再 import（撕过账本）→ 换根了，旧根下没有这个文件
[退出码 0]
```

- `sys.path.insert(0, "")` 之后，`os.chdir("there")` 一次，`sys.path` **一个字没改**，
  但屏幕上 `<cwd>` 变成了 `<cwd>/there` —— 整本账换了根。
- 「现在 import 到新根里的文件了吗 → True」：`somemod` 是从 `there/` 里拿到的，而代码里从来没有 `there` 这个词。
- 最后一步是它的逆命题：`chdir` 回旧根 + 撕掉账本再 import → 「换根了，旧根下没有这个文件」。
  四行合起来才构成完整证据链：同一个进程、同一份 `sys.path`，只改 `cwd`，同一个名字**先炸后成**再炸。
- 这里必须撕账本，否则第 3 节那条「命中就什么都不做」会把现象盖掉 ——
  这也是为什么这两格是本章最费笔墨的两本账的交叉点。

**这一节带走：**

- 顺序 = 优先级，而**账本在路径之前**：改了 `sys.path` 却撕了 `sys.modules` 才见效，这不是玄学，是第 3 节那条。
- `ModuleNotFoundError` 只给名字不给路径，所以排查的第一步永远是「把 `sys.path` 打印出来按层问」，不是改代码。
- 空串是在**每次 import 的那一刻**拿 `os.getcwd()` 去解析的：`-c` / 管道 + `chdir` = 同一份代码在不同目录下行为不同。
  IDE 里「运行选中的代码」忽好忽坏、CI 换个 workspace 就 `ModuleNotFoundError`，多半是这一条。
- 不想要这个漂移，就用 `-m`（写全包名），或者 `-P` / `PYTHONSAFEPATH=1` 把入口项整个关掉。

---

## 第 07 节 · c 篇完整跑：包 = 带 __init__.py 的目录，__init__ 就是包的模块体

第 2 节那句「模块体只跑一次」对整个包同样成立，而包多出一件事：**父包的 `__init__.py` 也是模块体**。
`c_packages_and_init.py` 第一段就把执行顺序直接印到屏幕上 —— 三个文件开头各印一行标记。

```
=== 1. 包 = 带 __init__.py 的目录：__init__ 的执行顺序直接上屏幕 ===
  现场：ord/pkg/__init__.py、ord/pkg/sub/__init__.py、ord/pkg/sub/mod.py ——
  三个文件开头各印一行标记，于是「谁被执行、什么顺序」是看得见的（cwd 都是 ord/）。
  · import pkg.sub.mod
    │ [INIT] pkg/__init__.py 被执行
    │ [INIT] pkg/sub/__init__.py 被执行
    │ [BODY] pkg/sub/mod.py 被执行
    │   sys.modules 里 pkg 开头的键 → ['pkg', 'pkg.sub', 'pkg.sub.mod']
  · from pkg.sub import mod
    │ [INIT] pkg/__init__.py 被执行
    │ [INIT] pkg/sub/__init__.py 被执行
    │ [BODY] pkg/sub/mod.py 被执行
    │   sys.modules 里 pkg 开头的键 → ['pkg', 'pkg.sub', 'pkg.sub.mod']
  · import pkg.sub
    │ [INIT] pkg/__init__.py 被执行
    │ [INIT] pkg/sub/__init__.py 被执行
    │   sys.modules 里 pkg 开头的键 → ['pkg', 'pkg.sub']
  · import pkg
    │ [INIT] pkg/__init__.py 被执行
    │   sys.modules 里 pkg 开头的键 → ['pkg']
```

- `import pkg.sub.mod` 打出三行：`[INIT] pkg/__init__.py` → `[INIT] pkg/sub/__init__.py` → `[BODY] pkg/sub/mod.py`。
  **从最外层往里**一层层跑，最后才轮到模块体。
- `from pkg.sub import mod` 打出**完全一样的三行**：想拿到子模块，父包必须先成为真模块。
- `import pkg.sub` 只有两行 `[INIT]`，`import pkg` 只有一行 —— 你写到哪一层，就跑进哪一层。
  「只 import 到 `pkg.sub` 时，`mod` 的模块体一行都没跑」这条是按需导入能省启动的根据。
- 右边那列 `sys.modules 里 pkg 开头的键` 是另一本账：`['pkg', 'pkg.sub', 'pkg.sub.mod']`，
  三级全进了账本，而你本地只有一个名字（第 2 节 ③）。

顺手把常规包身上的三个键打出来，第 4 段的命名空间包就是在这三个键上全变：

```
  · 顺手把常规包身上的三个键打出来（import pkg, pkg.sub）：
    │ [INIT] pkg/__init__.py 被执行
    │ [INIT] pkg/sub/__init__.py 被执行
    │ pkg.__file__ = pkg-init-demo/ord/pkg/__init__.py
    │ pkg.__path__ = ['pkg-init-demo/ord/pkg'] | 类型 list
    │ pkg.sub.__file__ = pkg-init-demo/ord/pkg/sub/__init__.py
    │ pkg.__package__ = 'pkg' | pkg.sub.__package__ = 'pkg.sub'
```

`__file__` 指向 `__init__.py`、`__path__` 是**一个目录列表**（这个包自己的搜索根）、`__package__` 是自己的名字。
普通模块没有 `__path__`（第 2 节 ②），这一项就是「包」和「模块」的分水岭。

第二段解释一件事：`pkg.sub` 这个写法凭什么能成立。

```
=== 2. 父包对象上被绑了属性：这就是 pkg.sub 写得出来的原因 ===
  · import pkg.sub.mod 之后
    │ [INIT] pkg/__init__.py 被执行
    │ [INIT] pkg/sub/__init__.py 被执行
    │ [BODY] pkg/sub/mod.py 被执行
    │ hasattr(pkg, "sub") → True
    │ hasattr(pkg.sub, "mod") → True
    │ pkg 上非下划线的名字 → ['PKG_MARK', 'sub']
  · 只 import pkg，然后写 pkg.sub
    │ [INIT] pkg/__init__.py 被执行
    │ AttributeError: module 'pkg' has no attribute 'sub'
    │ 'pkg.sub' 在 sys.modules 里吗 → False
  · from pkg import sub，然后写 sub.mod
    │ [INIT] pkg/__init__.py 被执行
    │ [INIT] pkg/sub/__init__.py 被执行
    │ AttributeError: module 'pkg.sub' has no attribute 'mod'
    │ 'pkg.sub.mod' 在 sys.modules 里吗 → False
  · pkgb：它的 __init__.py 里写了 from . import subb2
    │ hasattr(pkgb, "subb2") → True
    │ pkgb.subb2.X → subb2 的 X
    │ 'pkgb.subb2' 在 sys.modules 里吗 → True
```

- 第一格：`import pkg.sub.mod` 之后 `hasattr(pkg, "sub") → True`、`hasattr(pkg.sub, "mod") → True`，
  而 `pkg` 上非下划线的名字只有 `['PKG_MARK', 'sub']`。
  所以 `pkg.sub` 写得出来**不是路径魔法，是属性访问** —— `import` 子模块时顺手沿父链做了一次 `setattr`。
- 第二格：只 `import pkg` 然后写 `pkg.sub` → `AttributeError: module 'pkg' has no attribute 'sub'`，
  而且 `'pkg.sub' in sys.modules → False`。父包**永远不带**子模块。
- 第三格：`from pkg import sub` 之后写 `sub.mod` → `AttributeError: module 'pkg.sub' has no attribute 'mod'`，
  `'pkg.sub.mod' in sys.modules → False`。拿到了中间层，也不带下一层。
- 第四格是唯一「能带出来」的写法：`pkgb/__init__.py` 里写了 `from . import subb2`，
  于是 `hasattr(pkgb, "subb2") → True`、`pkgb.subb2.X` 拿得到、`'pkgb.subb2'` 进了账本。
  **「API 在 `__init__.py` 收口」的实现方式就是这一行**，代价是 `import` 变慢、模块体副作用提前兑现，两件事都不可逆。

第三段处理 `from pkg import *`：`__all__` 无 / 有 / 谎报，三种一起跑。

```
=== 3. __all__ 对 from pkg import * 的影响：无 / 有 / 谎报，三种一起跑 ===
  三个包都在 star/ 下：plainpkg 没有 __all__（另有子模块 suba）；
  alledpkg 有 __all__ = ['PUBLIC', 'fn', 'subb']（另有 subb、subc 两个子模块）；
  ghostpkg 的 __all__ = ['ghost']，可 ghost 根本不存在。
  · A：plainpkg（没有 __all__）
    │ A: from plainpkg import * → 绑到本地的名字 ['PUBLIC', 'Thing']
    │         sys.modules 里 plainpkg. 开头的键 → []
  · B：alledpkg（有 __all__）
    │ B: from alledpkg import * → 绑到本地的名字 ['PUBLIC', 'fn', 'subb']
    │         sys.modules 里 alledpkg. 开头的键 → ['alledpkg.subb']
  · C：ghostpkg（__all__ 撒谎）
    │ AttributeError: module 'ghostpkg' has no attribute 'ghost'
    │ 本地绑到的名字（一个都没有 = 全句作废）→ []
  └ 退出码 = 0
```

- A（没有 `__all__`）：绑到本地的名字是 `['PUBLIC', 'Thing']`，而 `sys.modules` 里 `plainpkg.` 开头的键是 **`[]`**。
  `import *` 拿的是父包命名空间里**非下划线的现成名字**，它**不替你 import 子模块** —— 子模块 `suba` 既没绑上也不在账本里。
- B（有 `__all__ = ['PUBLIC', 'fn', 'subb']`）：绑到的是 `['PUBLIC', 'fn', 'subb']`，而且账本里出现了 `['alledpkg.subb']`。
  写了 `__all__` 就是换了一份名单：**名单里的子模块名会被真的导入**，同目录没进名单的 `subc` 没进账本。
- C（`__all__ = ['ghost']` 撒谎）：`AttributeError: module 'ghostpkg' has no attribute 'ghost'`，
  本地绑到的名字是 `[]` —— **全句作废**。这和第 2 节第 6 段 `from m import RATE, missing` 留下半套（`['RATE']`）正好相反：
  星号导入是「先验名单、再一次性绑」，所以要么全成要么全不成立。

第四段是命名空间包（PEP 420），也是本章最容易出「查不出来的错」的一格：
同一个包名 `merged`，两个根都没有 `__init__.py` 时会发生什么。

```
=== 4. 命名空间包：没有 __init__.py 的目录也能 import，三个键全变了 ===
  现场：ns/root1/merged/ 和 ns/root2/merged/ 都**没有** __init__.py，ns/root3/merged/ 有；
  root1 里有 extra.py、root2 里有 only2.py，两边都有 leaf.py。
  · PYTHONPATH=root1:root2
    │ type → module | repr → <module 'merged' (namespace) from ['pkg-init-demo/ns/root1/merged', 'pkg-init-demo/ns/root2/merged']>
    │ 有 __file__ 这个键吗 → True | merged.__file__ = None
    │ __path__ 类型 → _NamespacePath | 几项 → 2
    │ __path__ 内容 → ['pkg-init-demo/ns/root1/merged', 'pkg-init-demo/ns/root2/merged']
    │ __spec__.origin → None
    │ from merged import * → 绑到的名字 []
    │ merged.leaf.WHERE → root1 的 merged/leaf.py
    │ merged.extra → root1 独有的 merged/extra
    │ merged.only2 → root2 独有的 merged/only2
  └ 退出码 = 0
  · PYTHONPATH=root3:root1:root2（把带 __init__.py 的根排第一）
    │ [INIT] root3 的 merged/__init__.py —— 有它就不是命名空间包了
    │ type → module | repr → <module 'merged' from 'pkg-init-demo/ns/root3/merged/__init__.py'>
    │ 有 __file__ 这个键吗 → True | merged.__file__ = 'pkg-init-demo/ns/root3/merged/__init__.py'
    │ __path__ 类型 → list | 几项 → 1
    │ __path__ 内容 → ['pkg-init-demo/ns/root3/merged']
    │ __spec__.origin → 'pkg-init-demo/ns/root3/merged/__init__.py'
    │ from merged import * → 绑到的名字 []
    │ merged.leaf.WHERE → root3 的 merged/leaf.py
    │ merged.extra → ModuleNotFoundError: No module named 'merged.extra'
    │ merged.only2 → ModuleNotFoundError: No module named 'merged.only2'
  └ 退出码 = 0
```

- 第一跑（`PYTHONPATH=root1:root2`，两边都**没有** `__init__.py`）：
  `repr` 里写着 `(namespace) from […两个目录…]`、`merged.__file__ = None`（键存在，值是 `None`）、
  `__path__` 的类型是 `_NamespacePath`、有**两项**、`__spec__.origin` 是 `None`。
  `from merged import *` 绑到的名字是 `[]` —— 命名空间包**没有模块体**，所以什么都没有。
  `merged.leaf.WHERE → root1 的 merged/leaf.py`：**同名的 leaf 只有第一个根那份被看到**，另一份被静默压住；
  但两边**不同名**的 `extra`（root1 独有）和 `only2`（root2 独有）**都拿得到**。
- 第二跑（`PYTHONPATH=root3:root1:root2`，把带 `__init__.py` 的那个根排第一）：屏幕上第一行就是
  `[INIT] root3 的 merged/__init__.py`，然后 `__file__` 变成那个文件、`__path__` 类型退回 `list` 且只剩**一项**、
  `extra` 和 `only2` 双双 `ModuleNotFoundError`。**一个 `__init__.py` 就把另外两个根整个切断。**
- 隐患压在这条分界线上：少一个 `__init__.py`，你的包和别人装的同名包会被并成同一个命名空间，
  出现「import 到 A 的模块、却找不到 B 的名字」这种查不出来的错；多一个又反过来把别人的目录切断。
  排查一行就够：`print(包.__path__)`，看这个包到底由哪几个目录拼成。

第五段回答「相对导入为什么这么挑跑法」。同一个文件 `rel/pkgr/modr.py`，第一句实质代码就是 `from . import helper`：

```
=== 5. 相对导入：. 和 .. 只看 __package__ / __spec__.parent，不看文件在哪 ===
  同一个文件 rel/pkgr/modr.py，它的第一句实质代码就是 from . import helper：
  · python3 pkgr/modr.py（脚本跑法） → 退出码 1
    │ __name__ = __main__
    │ __package__ = None
    │ __spec__ 这个键在吗 = True | 它的值是 None 吗 = True | parent = '（没有可问的 __spec__）'
    │ Traceback (most recent call last):
    │   File "pkg-init-demo/rel/pkgr/modr.py", line 7, in <module>
    │     from . import helper
    │ ImportError: attempted relative import with no known parent package
  · python3 -m pkgr.modr（模块跑法） → 退出码 0
    │ [INIT] pkgr/__init__.py
    │ __name__ = __main__
    │ __package__ = 'pkgr'
    │ __spec__ 这个键在吗 = True | 它的值是 None 吗 = False | parent = 'pkgr'
    │ 相对导入 . 成功 → pkgr/helper.py
  · 手搓一个 __package__ 再 exec 同一份源码（既不是脚本跑法也不是 -m）：
    │ __name__ = pkgr.modr
    │ __package__ = 'pkgr'
    │ __spec__ 这个键在吗 = False | 它的值是 None 吗 = True | parent = '（没有可问的 __spec__）'
    │ [INIT] pkgr/__init__.py
    │ 相对导入 . 成功 → pkgr/helper.py
    │ （文件位置一个字没动，只是命名空间里给了包名）
  └ 退出码 = 0
```

- 脚本跑法 `python3 pkgr/modr.py`：`__name__ = __main__`、`__package__ = None`、`__spec__` 的值也是 `None`，
  退出码 1，`ImportError: attempted relative import with no known parent package`。
  「当前包」这个概念在这个入口下**根本不存在**。
- 模块跑法 `python3 -m pkgr.modr`：`__package__ = 'pkgr'`、`__spec__.parent = 'pkgr'`，`[INIT] pkgr/__init__.py` 先跑，
  然后 `相对导入 . 成功 → pkgr/helper.py`，退出码 0。**同一个文件，唯一变的是它是「作为包的一部分」被导入的。**
- 第三格是决定性的一格：既不是脚本跑法也不是 `-m`，只是手工给一个命名空间塞上 `__package__` 再 `exec` 同一份源码 ——
  `__spec__` **这个键都不存在**，相对导入照样成功。
  屏幕上那句括号「文件位置一个字没动，只是命名空间里给了包名」就是这一段的全部结论：
  **`.` 只看名字，不看文件在哪。**

`..` 和越界的两格，把「级数」这条限制钉死：

```
  · 子包里的 from .. import helper（python3 -c 'import pkgr.sub.up'） → 退出码 0
    │ [INIT] pkgr/__init__.py
    │ [INIT] pkgr/sub/__init__.py
    │ __package__ = 'pkgr.sub' | __spec__.parent = 'pkgr.sub'
    │ .. 找到的是 → pkgr/helper.py
    │ .  找到的是 → pkgr/sub/sibling.py
  · 越界的 from ... import os（python3 -c 'import pkgr.sub.over'） → 退出码 1
    │ Traceback (most recent call last):
    │   File "<string>", line 1, in <module>
    │     import pkgr.sub.over
    │   File "pkg-init-demo/rel/pkgr/sub/over.py", line 1, in <module>
    │     from ... import os
    │ ImportError: attempted relative import beyond top-level package
    │ [INIT] pkgr/__init__.py
    │ [INIT] pkgr/sub/__init__.py
```

- `pkgr/sub/up.py` 里写 `from .. import helper`：`__package__ = 'pkgr.sub'`，
  `..` 找到 `pkgr/helper.py`（上一级），`.` 找到 `pkgr/sub/sibling.py`（同包）。两个答案同时印出来，界线就清楚了。
- `pkgr/sub/over.py` 里写 `from ... import os`：退出码 1，
  `ImportError: attempted relative import beyond top-level package` —— 级数不能越过顶层包。
- 这一格里有个顺序陷阱，屏幕上两行 `[INIT]` 排在 traceback **后面**。那只是这个入口把 stdout 留到退出才冲出来，
  真实执行顺序仍是 `__init__.py` 先跑完才轮到 `over.py` 那句 `from ... import os`（第 08 章讲的行缓冲在这里咬人）。

第六段是 `python3 -m 包名` 到底跑谁。先连跑三个**没有** `__main__.py` 的包 —— 常规包、常规子包、命名空间包：

```
=== 6. python3 -m 包名：跑的是 pkg.__main__，缺它就是这一句 ===
  先连跑三个「没有 __main__.py」的包：常规包 pkgr、常规子包 pkgr.sub、命名空间包 merged。
  · python3 -m pkgr → 退出码 1
    │ python3: No module named pkgr.__main__; 'pkgr' is a package and cannot be directly executed
    │ [INIT] pkgr/__init__.py
  · python3 -m pkgr.sub → 退出码 1
    │ python3: No module named pkgr.sub.__main__; 'pkgr.sub' is a package and cannot be directly executed
    │ [INIT] pkgr/__init__.py
    │ [INIT] pkgr/sub/__init__.py
  · PYTHONPATH=root1:root2 python3 -m merged → 退出码 1
    │ python3: No module named merged.__main__; 'merged' is a package and cannot be directly executed
    （顺序提醒：上面三跑把报错印在 [INIT] 前面，是因为这条命令走的是「直接写 stderr 就退出」的路径，
      子进程的 stdout 还压在缓冲区里，退出时才吐。真实顺序是包的 __init__.py 先跑完、才去找
      __main__ —— 下面成功那一跑里 [INIT] 就老老实实排在第一行。）
  · 往 pkgr/ 里现写一个 __main__.py（里面照常用 from . import helper），同一条命令再跑：
    │ [INIT] pkgr/__init__.py
    │ __main__.py 被 -m 跑起来了：__name__ = __main__ | __package__ = 'pkgr'
    │         __spec__.name = 'pkgr.__main__' | __spec__.parent = 'pkgr'
    │ 在 __main__.py 里相对导入照样可用 → pkgr/helper.py
  └ 退出码 = 0
```

- 三跑都是退出码 1，措辞同一个模板：`No module named pkgr.__main__; 'pkgr' is a package and cannot be directly executed`
  （子包和命名空间包只是把名字换成 `pkgr.sub.__main__` / `merged.__main__`）。
  **能 `import` 不等于能 `-m`。**
- 然后往 `pkgr/` 里现写一个 `__main__.py`（里面照常用 `from . import helper`），同一条命令再跑：
  `[INIT] pkgr/__init__.py` 排在最前面，接着 `__name__ = __main__ | __package__ = 'pkgr'`、
  `__spec__.name = 'pkgr.__main__' | __spec__.parent = 'pkgr'`，相对导入可用，退出码 0。
  所以 `-m 包名` 找的是 `<包>.__main__`，把它的 `__name__` 设成 `'__main__'`、`__package__` 设成包名 ——
  这解释了为什么 `__main__.py` 里既能用相对导入、又能当命令行入口。
- 上面三跑把报错印在 `[INIT]` 前面，走的是「直接写 stderr 就退出」的路径，子进程 stdout 还压在缓冲区里。
  真实顺序是 `__init__.py` 先跑完才去找 `__main__` —— 下面成功那一跑里 `[INIT]` 就老老实实排在第一行。

本篇最后是一张三列对照表：普通模块 / 常规包 / 命名空间包，在 `__file__`、`__path__`、
有没有可跑的模块体、`import *` 拿什么、`..` 能不能用、`-m` 会怎样这六个问题上并排给出答案，
每一格都标了本篇的段号。原文没重贴，跑 `bash demo.sh` 到第 7 节最后一段可见。

**这一节带走：**

- 包就是「带 `__init__.py` 的目录」，`__init__.py` 就是这个包的**模块体**：首次 `import` 时从最外层往里一层层执行，
  只跑到你写的那一层（`import pkg.sub` 不碰 `mod.py`）。
- `pkg.sub` 写得出来是因为 `import` 子模块时沿父链 `setattr`；所以父包 `__init__.py` 是唯一能替你 `import` 子模块的地方 ——
  API 收口写在这里，代价是启动税 + 副作用提前兑现。
- `from x import *` 在没有 `__all__` 时**不导入子模块**；写了 `__all__` 名单里的子模块会被真的导入；
  名单撒谎给 `AttributeError`，且整句作废、一个名字都不留。
- 命名空间包（没有 `__init__.py`）：`__file__` 是 `None`、`__path__` 是 `_NamespacePath` 且可多项、多个根会**合并**；
  任何一个根放了 `__init__.py` 就退化成常规包并**切断**其它根。查这类错只问一句：`print(包.__path__)`。
- 相对导入的锚点是 `__package__` 这个**名字**，不是文件位置：所以带相对导入的文件不能当脚本跑，
  正道只有 `-m` 和「把入口写到包外面」两条。在文件顶部 `sys.path.append(...)` 会让同一个文件有两个名字，
  于是两份对象两份状态（第 2 节的单例在这里反噬）。
- `-m 包名` 跑 `<包>.__main__`，而 `-m pkg.tool` 跑的是包里的模块 `tool.py`，两句原文很像、原因完全不同 ——
  第 5 节的假 `json.py` 那次报的是 `__path__ attribute not found`，别看错。

---

## 第 08 节 · 小证据：`import 子模块` 会把父包的 __init__ 跑一遍，`python 包/子.py` 不会

这一节在主线① 和主线② 的接缝上：同一个 `probe.py`，三种启动方式在屏幕上打出三种身份，
而最底下那一格把主线① 放大给你看 —— 导入 `pkgdir.leaf` 时，父包 `pkgdir` 会被连带记进 `sys.modules` 并挂上属性。
沙箱里现造一个 `pkgdir/`：`__init__.py` 是空文件，`leaf.py` 用相对导入 `from . import sibling` 去拿兄弟。

```
$ mkdir -p import-check/pkgdir && cd import-check
printf "" > pkgdir/__init__.py
cat > pkgdir/leaf.py <<"PYEOF"
from . import sibling
print("[leaf] 相对导入 succeeded, TAG =", sibling.TAG)
PYEOF
cat > pkgdir/sibling.py <<"PYEOF"
TAG = "sibling 被相对导入到了"
PYEOF
cat > pkgdir/probe.py <<"PYEOF"
import sys
print("[probe] __name__ =", __name__, "| __package__ =", repr(__package__), "| 有 __path__ 吗（我是不是个包）→", hasattr(sys.modules[__name__], "__path__"))
PYEOF
python3 -c "import sys; sys.path.insert(0, \"pkgdir\"); import probe"
python3 pkgdir/probe.py
python3 -m pkgdir.probe
python3 -c "
import sys, os
sys.path.insert(0, \".\")
import pkgdir.leaf
print(\"★ import 三层里的 pkgdir.leaf：pkgdir/__init__.py 是空文件，但它仍然被执行了（屏幕上没有报错就是执行成功）\")
print(\"   pkgdir 在账本里吗 →\", \"pkgdir\" in sys.modules, \"｜ pkgdir.sibling 也在吗 →\", \"pkgdir.sibling\" in sys.modules, \"｜ 父包对象上有 sibling 这个属性吗 →\", hasattr(sys.modules[\"pkgdir\"], \"sibling\"))
"
cd ..; rm -rf import-check
[probe] __name__ = probe | __package__ = '' | 有 __path__ 吗（我是不是个包）→ False
[probe] __name__ = __main__ | __package__ = None | 有 __path__ 吗（我是不是个包）→ False
[probe] __name__ = __main__ | __package__ = 'pkgdir' | 有 __path__ 吗（我是不是个包）→ False
[leaf] 相对导入 succeeded, TAG = sibling 被相对导入到了
★ import 三层里的 pkgdir.leaf：pkgdir/__init__.py 是空文件，但它仍然被执行了（屏幕上没有报错就是执行成功）
   pkgdir 在账本里吗 → True ｜ pkgdir.sibling 也在吗 → True ｜ 父包对象上有 sibling 这个属性吗 → True
[退出码 0]
```

先读 `probe` 那三行，同一个文件三种跑法打出三种 `__name__` / `__package__`：`-c` 里把 `sys.path` 直接插到 `pkgdir` 时它是顶层模块，`__name__` 打成 `probe`、`__package__` 打成空串；
`python3 pkgdir/probe.py` 时它是 `__main__` 且 `__package__` 是 `None`；只有 `python3 -m pkgdir.probe` 才把 `__package__` 打成 `pkgdir`。
这三行的含义在第 13 节会再钉一遍：相对导入只看这两个名字，不看文件在磁盘上位于哪里 —— 这是主线②「启动方式决定 `sys.path[0]` 形状」的另一面。
最后一格才是本节正题：`leaf` 里那句「相对导入成功」能印出来，说明空文件 `pkgdir/__init__.py` 被执行了（屏幕上没有报错就是执行成功）；
屏幕上底下那一行连着给了三个 True：父包在账本里、`pkgdir.sibling` 也在账本里、父包对象上挂着 `sibling` 这个属性。
这就是「`from pkg.sub import name` 有时能用、有时炸」的根源：账本里有没有那一格，取决于父包被连带执行到哪一步。

**这一节带走：**

- 导入 `包.子` 会先把父包的 `__init__` 跑一遍，哪怕它是个空文件；`python 包/子.py` 直接当脚本跑则没有这一层。
- 空 `__init__.py` 也照样执行，只是没内容可执行 —— 屏幕上不报错就等于跑了。
- 导入子模块时，`sys.modules` 里会同时多出父包和子模块两条键，父包对象上还挂着子模块属性。
- 同一个文件的 `__name__` / `__package__` 由启动方式决定（`-c` / 路径 / `-m` 三种各不相同），相对导入只认这两个名字。

---

## 第 09 节 · d 篇完整跑：循环导入炸的不是「有环」，是模块体的先后顺序

这一节在主线① 上，而且是最硬的一格：报错不是因为「图上有个环」，是因为某个模块体只跑到一半就被别人当成了成品。
沙箱里 `pkg/from_a.py` 第一行 `import pkg.from_b`，`pkg/from_b.py` 第一行 `from pkg.from_a import Y` —— 环就在这。

```
$ python3 d_circular_import.py
=== 1. 先造一个真循环：包内 from-import 撞上半成品，报错原文长这样 ===
  沙箱 circular-demo/ 里现造的文件数 = 37（其中 pkg/ 里 17 个）
  pkg/from_a.py 第 1 行 import pkg.from_b，pkg/from_b.py 第 1 行 from pkg.from_a import Y —— 环就在这
  子进程 `python3 drv_from.py`（cwd=circular-demo/）：rc=1
    Traceback (most recent call last):
      File "circular-demo/drv_from.py", line 1, in <module>
        import pkg.from_a
      File "circular-demo/pkg/from_a.py", line 1, in <module>
        import pkg.from_b                    # 先让 b 跑起来（下面还要被 b 回头 import）
        ^^^^^^^^^^^^^^^^^
      File "circular-demo/pkg/from_b.py", line 1, in <module>
        from pkg.from_a import Y             # ← 事故点：a 还在跑，Y 还没有
        ^^^^^^^^^^^^^^^^^^^^^^^^
    ImportError: cannot import name 'Y' from partially initialized module 'pkg.from_a' (most likely due to a circular import) (circular-demo/pkg/from_a.py)
  → 最后一行才是判据：cannot import name ... **from partially initialized module** ...
    「partially initialized」= from_a 已经在 sys.modules 里、但模块体只跑到第 1 行，Y 那一行还没执行。
  ★ 这句里连「most likely due to a circular import」都替你写好了 —— 解释器直接说出它怀疑什么，
    但它只在**包内的模块**上给这句（见第 2 节实测的第四种）。
```

屏幕最后一行是解释器自己写的那条 `ImportError`：它点明 `from_a` 是个**半成品模块**（已经在 `sys.modules` 里、但模块体只跑到第 1 行，`Y` 那一行还没执行），
顺带把「多半是循环导入」这个怀疑也写进了同一句里 —— 但它只在**包内的模块**上补这一句。
紧接着四种取法撞同一个环，屏幕上给出四种不同的报错原文：

```
=== 2. 同一个环、四种取法、四种原文：措辞由「怎么取」决定，不由环决定 ===
  取法 A  from pkg.from_a import Y（事故在 pkg/from_b.py）｜ rc=1
    原文 → ImportError: cannot import name 'Y' from partially initialized module 'pkg.from_a' (most likely due to a circular import) (circular-demo/pkg/from_a.py)
  取法 B  import pkg.attr_a as a → a.X（事故在 pkg/attr_b.py）｜ rc=1
    原文 → AttributeError: partially initialized module 'pkg.attr_a' from 'circular-demo/pkg/attr_a.py' has no attribute 'X' (most likely due to a circular import)
  取法 C  import pkg.dot_a → pkg.dot_a.X（事故在 pkg/dot_b.py）｜ rc=1
    原文 → AttributeError: cannot access submodule 'dot_a' of module 'pkg' (most likely due to a circular import)
  取法 D  一模一样的 from-import，但两个文件**不在包里**（circular-demo/top/）｜ rc=1
    原文 → ImportError: cannot import name 'Y' from 'top_a' (consider renaming 'circular-demo/top/top_a.py' if it has the same name as a library you intended to import)
  → A/B/C 三种都写着「most likely due to a circular import」；D 写的是「consider renaming」：
    partially initialized 那句提示走的是「父包 + 子模块」那条判定路径（本机 3.14.4 实测）。
  → 真正的差别是**取名字的时刻**：`from a import x` 在模块体里当场要 x；`import a` 只从
    sys.modules 拿到半成品模块对象，等到 `a.x` 那一刻才发现属性还没写上。
  ★ 措辞会随小版本变，跨版本/跨平台未验证；能当事实的是 traceback 里的**文件名 + 行号**。
```

A/B/C 三种原文都在句尾点了「多半是循环导入」这个怀疑；D 却换成一句「建议把文件改名」式的提示 —— 因为 D 那两个文件不在包里，走的是另一条判定路径。
真正的差别在**取名字的时刻**：`from a import x` 在模块体里当场要 `x`，`import a` 只是从账本里拿到半成品模块对象，等到 `a.x` 那一刻才发现属性还没写上。
把「半成品」三个字拍到脸上看，是下面这一格：

```
=== 3. 半成品时刻：在 a 的模块体里看 b 的命名空间，只有部分东西 ===
  驱动 `import pkg.half_b`：rc=0（同一个环，这一对却活下来了 —— 取法不撞时刻）
    b 的模块体前半段：hasattr(self, 'Y') → False
    a 的模块体里：hasattr(b, 'Y') → False
    a 的模块体里：b 的非 dunder 名字 → ['sys']
    a 的模块体里：b 的 dunder 名字（只打名字）→ ['__builtins__', '__cached__', '__doc__', '__file__', '__loader__', '__name__', '__package__', '__spec__']
    a 的模块体里：b.__spec__._initializing → True
    b 的模块体后半段：a 跑完之后，自己才写下 Y
    全部跑完之后：hasattr(pkg.half_b, 'Y') → True
    全部跑完之后：pkg.half_b.__spec__._initializing → False
  → b 的模块体最后一行才写 Y，而 a 的体是在倒数第二行那句 `import pkg.half_b` 里跑完的，
    所以 a 眼里的 b 只剩那几个 dunder 名字 + 一个 `sys`（b 自己在第 1 行 import 的），
    `hasattr(b, 'Y')` 当场 False —— 这就是「半成品」四个字的字面意思。
  → `_initializing` 这个私有字段就是那句报错的开关：模块体跑完才翻成 False（上面两行可对照）。
```

同一个环，这一对（`half_a`/`half_b`）却活了下来，`rc` 是 0。屏幕上 `hasattr(b, 'Y')` 给出 False、b 的非 dunder 名字只剩一个 `sys`、`__spec__` 上那个 `_initializing` 还是 True。
`_initializing` 这个私有字段就是那句报错的开关：模块体跑完才翻成 False，最底下两行可对照。这就是「半成品」四个字的字面意思。
那怎么修？d 篇给了三条出路，每条一个真子进程：

```
=== 4. 三条出路逐个实测（每条一个真子进程）：延迟 import / 只 import 模块 / 抽公共模块 ===
  出路① 延迟到调用时（from-import 挪进函数体）｜ rc=0 → 成功
    lazy_a.use() 拿到 lazy_b 的 Y
    lazy_b.use() 拿到 lazy_a 的 X
    → 环还在（pkg.lazy_a ↔ pkg.lazy_b 互相引用），只是模块体里谁也不当场取对方的名字。
  出路② 只 import 模块，不 from 模块 import 名字｜ rc=0 → 成功
    mo_a.use() 拿到 mo_b 的 Y
    mo_b.use() 拿到 mo_a 的 X
    → 环还在（pkg.mo_a ↔ pkg.mo_b 互相引用），只是模块体里谁也不当场取对方的名字。
  出路③ 之前：pkg3a 节点 ['x', 'y']、边 ['x→y', 'y→x'] ｜ 有环 → True ｜ 子进程 rc=1 → 失败
    原文 → ImportError: cannot import name 'DESC' from partially initialized module 'pkg3a.x' (most likely due to a circular import) (circular-demo/pkg3a/x.py)
  出路③ 之后：把共同依赖（常量 Color + 函数 stamp）抽到 pkg3b/common.py
    pkg3b 节点 ['common', 'x', 'y']、边 ['x→common', 'y→common'] ｜ 有环 → False ｜ 子进程 rc=0 → 成功
    x 盖了 红 的章 ｜ y 盖了 红 的章
  → 第三条出路不是技巧，是**改图**：三方图变成 DAG，谁先 import 都活得下来。
```

三条出路的 `rc` 全是 0。前两条环还在，只是模块体里谁也不当场取对方的名字；第三条干脆改图 —— 把共同依赖抽到 `common.py`，节点从两个变成三个（多了 `common`）、「有环」那一格翻成 False，谁先 import 都活得下来。
最常见的误诊是拿 `reload` 去救循环：

```
=== 5. 常见误诊：importlib.reload 救不了循环 ===
  驱动 `drv_rl.py`：rc=0；下面是子进程自己的输出（含报错原文）
    BODY-rr_a
    BODY-rr_b
    此刻 sys.modules 里 pkg.rl* 的先后 → ['pkg.rl_a', 'pkg.rl_b']
    第一次 import → 失败
       此刻 sys.modules 里有 pkg.rl_a 吗 → False ｜ 有 pkg.rl_b 吗 → False ｜ 有父包 pkg → True
       对半成品做 reload → KeyError ：sys.modules 里已经没有它，reload 连模块对象都拿不到
    BODY-rr_a
    BODY-rr_b
    此刻 sys.modules 里 pkg.rl* 的先后 → ['pkg.rl_a', 'pkg.rl_b']
    重新 import 一次 → 失败
       此刻 sys.modules 里有 pkg.rl_a 吗 → False ｜ 有 pkg.rl_b 吗 → False ｜ 有父包 pkg → True
    BODY-rr_a
    BODY-rr_b
    此刻 sys.modules 里 pkg.rl* 的先后 → ['pkg.rl_a', 'pkg.rl_b']
    reload 父包之后再来一次 → 失败
       此刻 sys.modules 里有 pkg.rl_a 吗 → False ｜ 有 pkg.rl_b 吗 → False ｜ 有父包 pkg → True
    第一、第二次的报错原文一模一样吗 → True
    原文｜ImportError: cannot import name 'X' from partially initialized module 'pkg.rl_a' (most likely due to a circular import) (circular-demo/pkg/rl_a.py)
  计数：rl_a 的模块体在这一个进程里被执行了 3 次、rl_b 的 3 次（每 import 一次就跑一遍体）
  「半成品进 sys.modules 的先后」这一行出现了 3 次，三次内容是否完全相同 → True；顺序就是 ['pkg.rl_a', 'pkg.rl_b'] —— a 永远排在 b 前面
  → reload 只做一件事：**把模块体再执行一遍**。它不改 sys.modules 里谁先谁后，
    于是同一条 from-import 再撞一次同一个半成品，报错文本一字不差（上面已比对：相等 True）。
  ★ 更麻烦的是失败之后半成品被从 sys.modules 摘掉了（上面那两行 False）——「reload 那个半成品」
    连模块对象都拿不到；反复 reload 只会让模块体多跑几遍，副作用也跟着多来几遍。
```

屏幕上三次尝试都标着失败，而第一、第二次的报错原文比对结果是「相同」为 True。原因是 `reload` 只做一件事：把模块体再执行一遍，它不改 `sys.modules` 里谁先谁后。
更麻烦的是失败之后半成品被从账本里摘掉了（那两行 `False`），连 `reload` 的目标模块对象都拿不到；反复 reload 只会让模块体多跑几遍、副作用多来几遍。
最后收口表告诉你看到这个报错先查哪三处，屏幕也标了本机三条出路的战绩：

```
=== 6. 第四条真实出路：TYPE_CHECKING —— 只给类型检查器看的那条边不跑 ===
  驱动 `drv_tc.py`：rc=0 → 成功
    import 完 tc_a 之后，sys.modules 里有 pkg.tc_b 吗 → False
    运行时 typing.TYPE_CHECKING 的值 → False
    手动真的 import tc_b 之后 → True ｜ tc_a.describe → tc_a 指向 B
  → 现代代码里的循环十有八九是**类型注解**造成的：A 标注要引 B、B 又要引 A。
    把那条 import 放进 `if TYPE_CHECKING:`（运行时 False），环在运行时就地消失；注解本身
    写成字符串，或整份文件加 `from __future__ import annotations`，交给检查器去解析。
  ★ 这条路只对「只读类型」有效：运行时要真用对方的函数，还是回到第 4 节那三条。

=== 7. 收口：看到这个报错先查哪三处 ===
  查哪一处              │ 怎么查（看证据，别猜）                                       │ 查到之后怎么办
  ① traceback 的 File 顺序 │ 从最后一行往上数：是不是 a.py 引 b.py、b.py 又回头引 a.py        │ 承认这是环，选一条出路
  ② 炸掉那一行的取法       │ from a import x → ImportError；import a 之后 a.x → AttributeError │ 把「取名字」的时刻往后挪
  ③ 模块体里谁当场用了对方 │ 常量、装饰器、类继承、注册表调用都算；函数体内的调用不算        │ 挪进函数体 / 抽公共模块
  三条出路的本机战绩：① 延迟 import rc=0 ｜ ② 只 import 模块 rc=0 ｜ ③ 抽公共模块 rc=0（见第 4 节）
  一条没救的：importlib.reload —— 报错原文完全相同，只是把模块体多跑了一遍（计数见第 5 节）
  ★ 全章反复出现的那句话在这儿最硬：import 是**执行**，不是声明；顺序问题只能用顺序解决。
  未验证：其它 Python 版本 / 其它平台的报错措辞 —— 别当事实引用。
```

d 篇还演了第四条出路：把只给类型检查器用的那条 import 放进 `if TYPE_CHECKING:`（运行时它是 `False`），环在运行时就地消失 —— 但它只对「只读类型」有效。
全章那句话在这儿最硬：import 是**执行**，不是声明；顺序问题只能用顺序解决。

**这一节带走：**

- 循环导入炸的不是「有环」，是「某个模块体只跑了一半就被当成成品」—— 同一个环换种取法就能活。
- 报错措辞由「怎么取名字」决定，不由环决定；`from a import x` 当场响，`import a` 用到时才响。
- 能当事实的是 traceback 里的文件名 + 行号，报错措辞和小版本绑定（本机 3.14.4，其它版本未验证）。
- 三条出路（延迟 import / 只 import 模块 / 抽公共模块变 DAG）本机各一个真子进程跑通；`reload` 一条都救不了。
- 失败会把半成品从 `sys.modules` 摘掉，所以「reload 那个半成品」连对象都拿不到。

---

## 第 10 节 · 小证据：同一个环、两种入口 = 两种生死（这一格是 d 篇结论的最小复现）

这一节在主线① 上，是第 09 节结论的最小复现：`cyc/a.py` 和 `cyc/b.py` 互相 `from . import`，环一个字没动，只换入口取法。

```
$ mkdir -p cycle-check/cyc && cd cycle-check
printf "%s\n" "print(\"[a 的模块体开跑]\")" "from . import b" "print(\"[a 的模块体跑完了]\")" "A_VALUE = \"a 里的东西\"" > cyc/a.py
printf "%s\n" "print(\"[b 的模块体开跑]\")" "from . import a" "print(\"[b 的模块体跑完了]\")" "B_VALUE = \"b 里的东西\"" > cyc/b.py
printf "%s\n" "print(\"[__init__ 被执行]\")" > cyc/__init__.py
echo "—— 入口 1：先 import a（a 的模块体里问 b 要 a 的名字，此刻 a 还是个半成品）"
python3 -c "
import sys; sys.path.insert(0, \".\")
try:
    import cyc.a
except Exception as e:
    print(\"   \", type(e).__name__, \"|\", str(e).splitlines()[-1])
    print(\"   此刻账本里 cyc.a 还在吗 →\", \"cyc.a\" in sys.modules, \"｜ 它身上有 A_VALUE 吗 →\", getattr(sys.modules.get(\"cyc.a\"), \"A_VALUE\", \"（没有：模块体没跑完）\") != \"（没有：模块体没跑完）\")
"
echo "—— 入口 2：把「要名字」换成「只拿模块」，同一个环立刻跑通"
printf "%s\n" "print(\"[a 的模块体开跑]\")" "from . import b as _b" "print(\"[a 的模块体跑完了]\")" "A_VALUE = \"a 里的东西\"" "def use_b():" "    from . import b" "    return b.B_VALUE" > cyc/a.py
printf "%s\n" "print(\"[b 的模块体开跑]\")" "from . import a" "print(\"[b 的模块体跑完了]\")" "B_VALUE = \"b 里的东西\"" > cyc/b.py
del_ok=1
python3 -c "
import sys; sys.path.insert(0, \".\")
import cyc.a
print(\"    import cyc.a 成功了吗 →\", True, \"｜ a.use_b() 拿到的是 →\", cyc.a.use_b())
"
cd ..; rm -rf cycle-check
—— 入口 1：先 import a（a 的模块体里问 b 要 a 的名字，此刻 a 还是个半成品）
[__init__ 被执行]
[a 的模块体开跑]
[b 的模块体开跑]
[b 的模块体跑完了]
[a 的模块体跑完了]
—— 入口 2：把「要名字」换成「只拿模块」，同一个环立刻跑通
[__init__ 被执行]
[a 的模块体开跑]
[b 的模块体开跑]
[b 的模块体跑完了]
[a 的模块体跑完了]
    import cyc.a 成功了吗 → True ｜ a.use_b() 拿到的是 → b 里的东西
[退出码 0]
```

屏幕上两个入口都把 `__init__`、`a`、`b` 的模块体五句 print 全跑到了底，没有一行异常。
入口 1 用 `from . import b` / `from . import a`：这是「只拿模块对象」，拿到的哪怕是个半成品也不当场向对方要名字，所以环跑通了 —— 这一格没走 except 分支（try 里只有 `import cyc.a`，成功就什么都不多印）。
入口 2 更进一步：模块体里只 `from . import b as _b`，真正把 `B_VALUE` 这个**名字**的索取挪进 `use_b()` 函数体，所以屏幕最底下那行确认 import 成功、`use_b()` 取到了 b 里写的那个字符串。
两个入口的差别只有「什么时候取名字」这一行，环没拆 —— 所以修循环导入的着力点从来不是把环拆开，而是别让一个模块在另一个的模块体里被当成品用。
真正「死」的那一种（模块体里当场向对方要名字）在第 09 节已经演过：屏幕上这里两个入口都是「生」。

**这一节带走：**

- 同一个环，`from . import <模块>` 只拿模块对象、不当场取名，就能活；当场要名字才会撞半成品。
- 修循环导入改的是「取名字的时机」，不是「把环剪断」。
- 屏幕上模块体 print 的嵌套顺序（a 开跑、b 开跑、b 跑完、a 跑完）就是 `sys.modules` 里那个半成品窗口有多宽的直接证据。
- 本节两个入口都跑通；报错原文的措辞跟小版本绑定（本机 3.14.4），别跨版本引用。

---

## 第 11 节 · e 篇完整跑：__pycache__ 的形状、失效判据，和四种「别写缓存」

这一节是主线① 的第三本账：缓存的键不是内容哈希，而是「源码 mtime + 源码字节数」两个数。缓存命中时 Python 连源码都不读。
先看文件名和 `__pycache__` 长在哪：

```
$ python3 e_pyc_and_cache.py
=== 1. 文件名形状：xxx.cpython-314.pyc 是谁决定的，__pycache__ 长在哪 ===
  子进程 `python3 -c 'import pkg.mod, solo'`（cwd=pycache-demo/）：rc=0 ｜ 模块体打印 → BODY-solo
  沙箱里现在的 .pyc（sorted，相对路径）：共 3 个
    __pycache__/solo.cpython-314.pyc
    pkg/__pycache__/__init__.cpython-314.pyc
    pkg/__pycache__/mod.cpython-314.pyc
  tag 的来源：sys.implementation.cache_tag = cpython-314 ｜ importlib.util.MAGIC_NUMBER = 2b0e0d0a ｜ 主.次版本 = 3.14
  把 main_script.py 当**脚本**直接跑：rc=0 ｜ 输出 → BODY-solo ; main 自己 import 了 solo
  它自己有没有留下 main_script.*.pyc → False（被 import 的 solo 却仍然有缓存：True）
  → 只有**被 import 的模块**写缓存；`__main__`（你直接敲进去的那个文件）永远不写 .pyc。
  → 文件名 = 模块名 + tag + .pyc；tag = 实现名-主.次版本（本机 cpython-314）。
    debug 构建 / free-threaded 构建 / Windows 上的 tag 形状，本机之外未验证，别当事实引用。
  ★ 缓存**不集中**：pkg/ 里那份和沙箱根上那份在不同目录 —— 删缓存要按目录删，find 一遍再删。
```

屏幕上 `.pyc` 一共 3 个，`solo` 那份在根目录的 `__pycache__/` 里、`pkg` 那两份在 `pkg/__pycache__/` 里 —— 缓存不集中，删要按目录删。
`main_script.py` 当脚本直接跑，屏幕上问它自己有没有留下 pyc 时答 False，而被它 import 的 `solo` 却有：只有被 import 的模块写缓存，`__main__` 永远不写。
文件名 = 模块名 + tag + `.pyc`，tag 由 `sys.implementation.cache_tag` 决定（本机 `cpython-314`）。头部那 16 字节里放的是谁：

```
=== 2. 头部 16 字节：缓存键不是内容哈希，而是「源码 mtime + 源码字节数」 ===
  magic 和当前解释器期望的一致 → True
  flags = 0（0 = 时间戳失效法；1 = unchecked-hash；3 = checked-hash —— ④ 现场把它切成 3）
  第 9~12 字节 == 源码 mtime（整秒）→ True
  第 13~16 字节 == 源码字节数 → True
  这份头部里有没有「源码哈希」字段 → False（flags=0 时那 8 个字节被 mtime 和 size 占了）
  → 判据照抄：缓存键是 (mtime, size)。改了内容却没动这两个数，缓存就还「有效」。
```

`flags` 是 0，也就是「按源码 mtime+size 失效」这一档；第 9~12 字节等于源码 mtime（整秒），第 13~16 字节等于源码字节数；那份头部里有没有独立的哈希字段，屏幕上答 False。
判据照抄下来：缓存键是 (mtime, size)。于是有了下面这四种改法、四种结局，每一格都是真子进程：

```
=== 3. 失效判据实测：四种改法四种结局，每一格都是真子进程跑出来的行为 ===
  源码一直是 `TAG = "V1"`；改内容就是把它改成 V2（两行长度完全一样，字节数不变）。
  ① 只把 mtime 拨后 3 秒（内容/字节数一个没动）：现在 头部 mtime == 源码 mtime → False
     再 import：rc=0 ｜ 输出 TAG = V1 ｜ .pyc 被重写过 → True（头部 mtime 重新对上源码：True）
     → mtime 一变，缓存当场作废、重编译。这是大家都熟悉的方向。
  ★ 方向别记反（这一页两种改法各测过一次）：动 mtime → 缓存**失效**；只动内容而 mtime 和字节数不动 →
    缓存**照旧命中**。所以「只改内容会不会被发现」的答案是「不一定」，而不是「一定会」。
  ② 改内容 V1→V2：**字节数不变** → True，mtime 拨回原值 → True
     下一次 import：rc=0 ｜ 输出 → TAG = V1   ← 读到的是 V1，源码明明写着 V2
     .pyc 一个字节都没被重写 → True：它压根没去读源码，只比了头部那两个数
  ★ 真实的坑就在这儿：`cp -p`、`rsync -a`、`git checkout` 之后只要 mtime 和字节数没变，
    Python 继续跑**旧字节码**。「我改了代码怎么没生效」有一半案例是这一条，不是编译器抽风。
  ③ 同样的内容，把 mtime 拨后 5 秒：rc=0 ｜ 输出 → TAG = V2   ← 这次才读到 V2
  ④ `python3 -m compileall -q -f --invalidation-mode checked-hash pkg`：rc=0 ｜ 现在头部 flags = 3（非 0 = 哈希失效法；`-f` 是强制重写，别让它跳过）
     再演一遍「只改内容 + mtime 拨回 + 字节数不变」（size 同 True / mtime 同 True）
     下一次 import：rc=0 ｜ 输出 → TAG = V2   ← 抓到了：哈希模式**会**现读源码算哈希
  → 默认模式（flags=0）省掉的是「读源码」这一步，代价就是②那个坑；checked-hash 用一次读源码换确定性。
```

把方向记牢（这一页两种改法各测过一次）：① 动 mtime（内容/字节数一个没动）—— 缓存当场作废、重编译；② 改内容 V1→V2 但字节数不变、mtime 拨回原值 —— 下一次 import 屏幕上取到的还是 V1，`.pyc` 一个字节都没被重写。
这就是真实的坑：`cp -p`、`rsync -a`、`git checkout` 之后只要 mtime 和字节数没变，Python 继续跑**旧字节码** —— 「我改了代码怎么没生效」有一半案例是这一条。③ 把 mtime 拨后 5 秒，这次才读到 V2；④ 切成 checked-hash 后，只改内容也能被抓到，因为哈希模式会现读源码算哈希。
想「别让缓存写出来」，e 篇演了四种开关，屏幕上只给布尔不给路径：

```
=== 4. 四种「别写缓存」各演一遍：屏幕上只有布尔，没有路径 ===
  默认（什么都不加）｜ rc=0 ｜ 源码旁边出现 __pycache__ → True
  -B（命令行开关）｜ rc=0 ｜ 源码旁边出现 __pycache__ → False
  PYTHONDONTWRITEBYTECODE=1｜ rc=0 ｜ 源码旁边出现 __pycache__ → False
  sys.dont_write_bytecode = True｜ rc=0 ｜ 源码旁边出现 __pycache__ → False
  -X pycache_prefix=pcp（sys.pycache_prefix 的命令行版）｜ rc=0
    源码旁边有 __pycache__ → False ｜ prefix 目录里有 .pyc → True
    prefix 里 solo 的缓存只此一份 → True ｜ 它的路径 == prefix + 源码绝对路径去掉开头的 / → True
    用了 prefix 就**没有 __pycache__ 这一层**（.pyc 直接躺在镜像目录里）→ True
    ★ 它把标准库的缓存也一起搬走了：prefix 里除 solo 之外还有 9 份 .pyc，其中落在系统目录（usr/… 开头）下的 9 份 —— 所以 prefix 别设到别人的目录里
  PYTHONPYCACHEPREFIX（同一个开关的环境变量版）｜ rc=0 ｜ 源码旁边有 __pycache__ → False ｜ prefix 目录里有 .pyc → True
  ★ 补一刀「只写不读」这半边：已有缓存 + 改内容（size 同 True、mtime 拨回 True）
     带 -B 再 import → rc=0 ｜ 输出 BODY-solo ; TAG = V1   ← 还是 V1：-B 拦得住写，拦不住读
     不带 -B 再 import 一次 → rc=0 ｜ 输出 BODY-solo ｜ .pyc 仍与源码「对得上」→ True
  → 三个开关只管**写**。想让陈旧缓存彻底闭嘴只有两条路：删 .pyc，或者让 mtime/字节数真的变一变。
```

`-B`、`PYTHONDONTWRITEBYTECODE=1`、`sys.dont_write_bytecode=True` 三个都能让源码旁边不出现 `__pycache__`（屏幕上那一格都是 False）；但补的那一刀说明它们只管写不管读 —— 带着 `-B` 再 import 陈旧缓存，屏幕上取到的还是 V1。
`-X pycache_prefix` 把缓存搬到别处，连标准库那份都一起搬走（prefix 里除 solo 外还有 9 份落在系统目录），所以别把 prefix 设到别人的目录里。
那条「删源码只留 .pyc 还能跑」的传闻，e 篇当场证伪：

```
=== 5. 删掉源码只留 .pyc：那个「大家都以为能跑」的传闻，实测 ===
  第一步：正常 import 一次，让它把缓存写出来 → rc=0 ｜ 有 __pycache__ → True
  第二步：**只删 .py**，__pycache__ 原样留着（现在带 solo 的文件：['__pycache__/solo.cpython-314.pyc']）
  第三步：再 import → rc=1 ｜ 原文：ModuleNotFoundError: No module named 'solo'
  → 传闻证伪：Python 3 不会因为 __pycache__ 里有个 .pyc 就认它 —— 定位模块找的是 `solo.py`。
  第四步：把那个 .pyc **改名成同级的 solo.pyc**（老式布局，__pycache__ 还留着）→ ['__pycache__/solo.cpython-314.pyc', 'solo.pyc']
  第五步：再 import → rc=0 ｜ 输出 → BODY-solo ; loader = SourcelessFileLoader | TAG = V1
  → 这才能跑：认的是「模块名.pyc」这个同级老式名字（SourcelessFileLoader），不是 __pycache__ 里带 tag 的那个。
  ★ 只把 .pyc 头部的 magic 改坏 1 个字节 → rc=1 ｜ 原文：ImportError: bad magic number in 'solo': b'\xd4\x0e\r\n'
     （本机期望的 magic 是 2b0e0d0a，头部第一个字节被我不小心改成了 0xd4 —— 这就是「拷 .pyc 去别的机器」必响的原因）
     其它 Python 版本上的原文未验证，别当事实引用；能确定的是：magic 绑解释器小版本。
```

只删 `.py` 再 import，`rc=1`，屏幕上是一条 `ModuleNotFoundError`；Python 3 定位模块找的是 `solo.py`，不认 `__pycache__` 里带 tag 的那份。
把 `.pyc` 改名成同级的 `solo.pyc`（老式布局）才跑通，接管它的 loader 变成 `SourcelessFileLoader`；而把头部的 magic 改坏 1 个字节，屏幕上立刻是一条 `ImportError` 说 magic 对不上 —— magic 绑解释器小版本。
最后一格把「.pyc 到底是什么」和「缓存到底省了什么」一起收口：

```
=== 6. 编译这件事看得见：.pyc 的正文是字节码 + 常量，不是机器码，更不是加密 ===
  compile('A = 1', ...) → code 对象；co_code 是 bytes → True
  dis 拆出来的前三条指令（本机 3.14 实测；别的小版本指令名会变）：['RESUME', 'LOAD_SMALL_INT', 'STORE_NAME']
  marshal.loads(pyc[16:]) → code：co_name = <module>，co_filename 只剩 basename = mod.py，co_consts = ('V1', None)
  → 一句话：.pyc 的正文就是一个 marshal 序列化的 code object —— **字节码**，CPU 一行都不认，
    仍然由解释器逐条解释执行。缓存跳过的只有 parse/compile，不是执行（第 7 节用计数证明）。
  源码里的字符串字面量在不在 .pyc 里 → ASCII「canary」：True ｜ 中文「金丝雀」（UTF-8）：True
  源码文本本身（'GUESS = ' 这一段）在不在 .pyc 里 → False
  ★ 「只发 .pyc 就能保护源码」是自欺：常量、名字、行号全在文件里，反编译几乎是原样。

=== 7. 收口：缓存买到的是「省掉 parse/compile」，不是「省掉执行」 ===
  同一个进程里 import cnt 四次 → 模块体打印次数 = 1 ｜ 这是第 1 讲那条规律的镜像：一个进程内模块体只跑一遍（sys.modules 说了算）
  缓存已经在了 → True（下面三个进程每一个都能命中它）
  有缓存：连开 3 个进程各 import 一次 → 模块体执行次数 = 3 ｜ rc 序列 [0, 0, 0]
  无缓存（每轮删掉 .pyc）：同样 3 个进程 → 模块体执行次数 = 3 ｜ rc 序列 [0, 0, 0]
  → 两组计数相等 → True：有没有 .pyc，模块体该跑几次还是几次。
  → 第 3 节②已经证明「命中缓存时连源码都不读」，所以省下的只有读源码 + 解析 + 编译这三步；
    模块体里的 print、建连接、写注册表、发请求，一次都不会少。本节一个毫秒都没打，
    因为「省了多少时间」每跑必抖，不是这一页能保证的事实。
  收口表（下面每一条都是本机 3.14.4 实测；跨版本 / 跨平台未验证，别当事实引用）
  你看到的现象             │ 本机实测的判据                                    │ 该怎么办
  改了代码没生效            │ 缓存键 = (源码 mtime, 字节数)；两个都没变 → 旧码继续跑 │ 删 __pycache__，或让那两个数真的变
  目录里多出 __pycache__    │ 只有被 import 的模块写缓存，__main__ 永远不写        │ 可以删，下次 import 会长回来
  想让它别写缓存            │ -B / PYTHONDONTWRITEBYTECODE / sys.dont_write_bytecode │ 三个都只管写，不管读
  缓存想放到别处            │ sys.pycache_prefix 镜像的是**绝对路径**              │ 别把 prefix 设到别人家的目录
  源码不想给你              │ __pycache__/*.pyc 加载不了；同级 xxx.pyc 才行，且绑 magic │ 这是分发格式，不是加密格式
```

`marshal.loads(pyc[16:])` 解出来是一个 code object，正文是**字节码**（`RESUME/LOAD_SMALL_INT/STORE_NAME`），CPU 一行都不认，源码里的字符串字面量原样躺在里面 —— 所以「只发 .pyc 保护源码」是自欺。
收口表下面那几行计数是硬证：同一进程 import 四次 → 模块体打印次数 = 1；有缓存 vs 无缓存各连开 3 进程 → 模块体执行次数都是 3，两组计数相等。缓存买到的只是「省掉 parse/compile」，不是「省掉执行」，所以 e 篇一个毫秒都没打（每跑必抖）。

**这一节带走：**

- 缓存键 =（源码 mtime, 源码字节数）两个数，不是内容哈希；两个数都没变 → 继续跑旧字节码。
- 只有被 import 的模块写 `.pyc`，`__main__` 永远不写；缓存散在各目录，删要按目录删。
- `-B` / `PYTHONDONTWRITEBYTECODE` / `sys.dont_write_bytecode` 三个只管写不管读；想让陈旧缓存闭嘴只能删 `.pyc` 或让那两个数真变。
- 删源码只留 `__pycache__/*.pyc` 加载不了；同级老式 `xxx.pyc` 才行，且 magic 绑解释器小版本。
- 缓存省的是 parse/compile，模块体照样执行、副作用照样发生。

---

## 第 12 节 · 小证据：缓存头部只有两个数 —— 当场拆 16 字节给你看

这一节在主线① 的第三本账上，是第 11 节判据的现场解剖：把 `.pyc` 头部的 16 字节用 `struct` 拆成四个字段，逐字对给你看。

```
$ mkdir -p header-check && cd header-check
python3 -c "
import importlib.util, os, struct, subprocess, sys
open(\"hcheck.py\", \"w\").write(\"A = 1\n\")
env = {k: v for k, v in os.environ.items() if not k.startswith(\"PYTHON\")}
env[\"PYTHONDONTWRITEBYTECODE\"] = \"\"
subprocess.run([sys.executable, \"-c\", \"import hcheck\"], capture_output=True, env=env)
cache = importlib.util.cache_from_source(\"hcheck.py\")
print(\"① 形状：源码 hcheck.py → 缓存 basename =\", os.path.basename(cache), \"｜ 它是 sys.implementation.cache_tag 拼出来的，tag =\", sys.implementation.cache_tag)
print(\"   缓存在 __pycache__ 目录里吗 →\", os.path.basename(os.path.dirname(cache)) == \"__pycache__\")
head = open(cache, \"rb\").read(16)
magic, flags, f3, f4 = struct.unpack(\"<4sIII\", head)
st = os.stat(\"hcheck.py\")
print(\"② 头部取前\", len(head), \"个字节，四个字段：\")
print(\"   字段① 版本标签字节（长度 =\", len(magic), \"）｜ 字段② flags =\", flags, \"→ flags==0 就是「按源码 mtime+size 失效」那一档吗 →\", flags == 0)
print(\"   字段③ 是源码 mtime：当场对得上吗 →\", f3 == int(st.st_mtime))
print(\"   字段④ 是源码字节数：当场对得上吗 →\", f4 == st.st_size)
print(\"③ ★ 整份判据只有这两个数，没有源码内容的哈希 →\", (f3, f4) == (int(st.st_mtime), st.st_size))
print(\"   所以「内容改了但 mtime 和字节数都没变」= 缓存命中 = 跑旧代码（e 篇第 3 段现场复现）\")
print(\"④ 换了 cache_tag 就是两份互不相干的缓存：文件名里带 tag →\", sys.implementation.cache_tag in os.path.basename(cache))
"
cd ..; rm -rf header-check
① 形状：源码 hcheck.py → 缓存 basename = hcheck.cpython-314.pyc ｜ 它是 sys.implementation.cache_tag 拼出来的，tag = cpython-314
   缓存在 __pycache__ 目录里吗 → True
② 头部取前 16 个字节，四个字段：
   字段① 版本标签字节（长度 = 4 ）｜ 字段② flags = 0 → flags==0 就是「按源码 mtime+size 失效」那一档吗 → True
   字段③ 是源码 mtime：当场对得上吗 → True
   字段④ 是源码字节数：当场对得上吗 → True
③ ★ 整份判据只有这两个数，没有源码内容的哈希 → True
   所以「内容改了但 mtime 和字节数都没变」= 缓存命中 = 跑旧代码（e 篇第 3 段现场复现）
④ 换了 cache_tag 就是两份互不相干的缓存：文件名里带 tag → True
[退出码 0]
```

屏幕上四行读数：形状那行 `hcheck.cpython-314.pyc`，tag 来自 `sys.implementation.cache_tag`，缓存确实在 `__pycache__` 目录里（True）。
头部取前 16 个字节、拆成四个字段：字段① 是版本标签（长度 4），字段② `flags` 是 0，屏幕上确认这正是「按 mtime+size 失效」那一档，字段③ 当场对得上源码 mtime、字段④ 当场对得上源码字节数。
最关键那行 `③ ★` 把判据收成一个布尔：`(f3, f4) == (int(st.st_mtime), st.st_size)` 是 True —— 整份判据只有这两个数，没有源码内容的哈希。
所以「内容改了但 mtime 和字节数都没变」= 缓存命中 = 跑旧代码；方向反过来也同理：拷文件、`git checkout`、Docker COPY 都可能把 mtime 设成新值而内容没变。缓存买到的是省掉 parse/compile，不是省掉执行。

**这一节带走：**

- `.pyc` 头部就 16 字节：4 字节 magic + 4 字节 flags + 一个 mtime + 一个字节数，没有内容哈希位。
- `flags==0` 意味着失效判据就是那两个数，命中即不读源码。
- 文件名里的 tag 来自 `sys.implementation.cache_tag`；换 tag 就是两份互不相干的缓存。
- 「改了没生效」和「没改却重编译」都是这两个数在起作用，不是编译器抽风。

---

## 第 13 节 · f 篇完整跑：__name__ / guard / 星号导入与 __all__

这一节在主线② 上：`__name__` 和 `__package__` 是解释器在启动那一刻写好的两个变量，它们替你决定了这份源码是谁、能看见谁。
先看同一个文件四种启动方式打出的四种身份（真子进程，原文照打）：

```
$ python3 f_main_guard_and_dunder.py
=== 1. 同一个文件，四种启动方式打出四种 __name__ / __package__（真子进程，原文照打） ===
  python3 solo.py｜ rc=0
      solo | __name__='__main__' __package__=None __spec__是None=True __file__=solo.py argv0=solo.py sys.path[0]是cwd=True sys.path[0是本文件目录=True
  python3 -c 'import solo'｜ rc=0
      solo | __name__='solo' __package__='' __spec__是None=False __file__=solo.py argv0=-c sys.path[0]是cwd=True sys.path[0是本文件目录=True
  python3 -m pk.mod｜ rc=0
      BODY-pk__init__
      pk.mod | __name__='__main__' __package__='pk' __spec__是None=False __file__=mod.py argv0=mod.py sys.path[0]是cwd=True sys.path[0是本文件目录=False
  python3 pk/mod.py｜ rc=0
      pk.mod | __name__='__main__' __package__=None __spec__是None=True __file__=mod.py argv0=mod.py sys.path[0]是cwd=False sys.path[0是本文件目录=True
  → 脚本模式和 -m 模式都把 __name__ 打成 '__main__'，区别全在 __package__：
    脚本模式 None（这文件不属于任何包 → 相对导入必炸），-m 模式 'pk'（有包 → 相对导入可用）。
  → 被 import 时 __name__ 就是模块名、__package__ 是空串、__spec__ 不是 None。
  → 最后两列顺带解释了「同一条 import 换个目录就找不到」：脚本模式 sys.path[0] 是**脚本所在目录**，
    -m / -c 模式是**当前工作目录**。同一个模块名，在两种跑法下能是两个不同的东西。
```

脚本模式 `python3 solo.py` 里 `__name__` 打成 `__main__`、`__package__` 打成 None；被 `-c` 里的 `import solo` 时 `__name__` 是 `solo`、`__package__` 是空串；`-m pk.mod` 也把 `__name__` 打成 `__main__`，但 `__package__` 是 `pk`。
最后两列顺带解释了「同一条 import 换个目录就找不到」：脚本模式 `sys.path[0]` 是脚本所在目录，`-m` / `-c` 是当前工作目录。
没有 guard 会怎样，用计数说话而不是用嘴说话：

```
=== 2. 没有 guard 会怎样：别人 import 你一下，你整个脚本跑一遍（计数说话） ===
  python3 nog.py（当脚本）｜ rc=0 ｜ 模块体执行=1 次 ｜ 主逻辑执行=1 次 ｜ 副作用文件建了吗 → True
  python3 -c 'import nog'（当模块）｜ rc=0 ｜ 模块体执行=1 次 ｜ 主逻辑执行=1 次 ｜ 副作用文件建了吗 → True
  python3 has.py（当脚本）｜ rc=0 ｜ 模块体执行=1 次 ｜ 主逻辑执行=1 次 ｜ 副作用文件建了吗 → True
  python3 -c 'import has'（当模块）｜ rc=0 ｜ 模块体执行=1 次 ｜ 主逻辑执行=0 次 ｜ 副作用文件建了吗 → False
  同一进程里 import nog 两遍｜ rc=0 ｜ 模块体执行=1 次 ｜ 主逻辑执行=1 次 ｜ 副作用文件建了吗 → True
  → 两件事分得很清：guard 管「当脚本还是当模块」，sys.modules 管「一个进程里跑几遍」。
    没有 guard 时，任何一次 import 都把主逻辑和副作用全量执行一遍 —— 别人用你是无意的，事故是有意的。
  ★ 加了 guard 也不等于安全：模块体那一半照样执行（BODY-has=1）。import 是执行，不是声明。
```

屏幕上四行对比把两件事分得很清：`nog` 无 guard，被 `import nog` 时主逻辑照样执行 1 次、副作用文件也建了；`has` 有 guard，被 import 时主逻辑 0 次、文件没建，但模块体那半边照样执行 1 次。
guard 管「当脚本还是当模块」，`sys.modules` 管「一个进程里跑几遍」—— 加了 guard 也不等于安全，import 是执行不是声明。
包里的东西到底该怎么起，差别就写在 `__package__` 上：

```
=== 3. 包内相对导入：-m 与 路径/x.py 的差别就写在 __package__ 上 ===
  `python3 -m pk.use`｜ rc=0
      BODY-pk__init__
      pk.mod | __name__='pk.mod' __package__='pk' __spec__是None=False __file__=mod.py argv0=use.py sys.path[0]是cwd=True sys.path[0是本文件目录=False
      相对导入成功，拿到的是 pk.mod
      pk.use | __name__='__main__' __package__='pk' __spec__是None=False __file__=use.py argv0=use.py sys.path[0]是cwd=True sys.path[0是本文件目录=False
    包的 __init__ 跑了吗（BODY-pk__init__ 计数）→ 1
  `python3 pk/use.py`（同一份文件，用路径起）｜ rc=1
      Traceback (most recent call last):
        File "guard-demo/pk/use.py", line 15, in <module>
          from . import mod                          # 包内相对导入：靠的是 __package__
          ^^^^^^^^^^^^^^^^^
      ImportError: attempted relative import with no known parent package
    这一跑里 __package__ 是 None（见第 1 节），所以相对导入当场被拒 —— 报错原文就是上面那行。
  `python3 pk/use_abs.py`（改成绝对导入兄弟模块）｜ rc=0 ｜ 输出 → pk.mod | __name__='mod' __package__='' __spec__是None=False __file__=mod.py argv0=use_abs.py sys.path[0]是cwd=False sys.path[0是本文件目录=True ; 绝对导入成功，mod.__name__ = mod
  `python3 -m pk.use_abs`（同一行代码，换 -m 起）｜ rc=1 ｜ 原文 → ModuleNotFoundError: No module named 'mod'
  `python3 -m pk`（跑包的 __main__.py）｜ rc=0
      BODY-pk__init__
      pk.__main__ | __name__='__main__' __package__='pk' __spec__是None=False __file__=__main__.py argv0=__main__.py sys.path[0]是cwd=True sys.path[0是本文件目录=False
      pk.mod | __name__='pk.mod' __package__='pk' __spec__是None=False __file__=mod.py argv0=__main__.py sys.path[0]是cwd=True sys.path[0是本文件目录=False
      入口拿到的是 pk.mod
  → 两套查找规则别混：相对导入问 __package__，绝对导入问 sys.path。
    脚本模式下 sys.path[0] 恰好是 pk/，所以 `import mod` 能成 —— 但它拿到的是**另一个模块**
    （__name__='mod'，不是 'pk.mod'）：同一份源码在进程里存在两份实例，第 1 行就看得出来。
  ★ 结论只有一句：住在包里的东西用 `-m` 起，别用路径起。
```

`python3 -m pk.use` 走通，屏幕上相对导入成功、拿到的是 `pk.mod`，包的 `__init__` 计数是 1；`python3 pk/use.py` 同一份文件用路径起却是 `rc=1`，报错是一条关于「相对导入没有已知父包」的 `ImportError`（因为 `__package__` 是 None）。
更阴的是绝对导入那对：`pk/use_abs.py` 用路径起能成功，但它拿到的是 `__name__` 为 `mod` 的**另一个模块实例**，不是 `pk.mod`；换 `-m pk.use_abs` 反而 `ModuleNotFoundError`（那个只有裸名的模块找不到）。结论只有一句：住在包里的东西用 `-m` 起。
星号导入的家底，默认过滤器先看：

```
=== 4. from x import *：默认只放「不以下划线开头」的名字 ===
  驱动 `star_check.py`（先 import m_pub 看家底，再星号导入看进来了什么）：rc=0
      m_pub 里的公开名字 → ['gamma', 'pi']
      m_pub 里以下划线开头、归作者自己的名字 → ['__beta', '_alpha']
      from m_pub import * 实际给进来的 → ['gamma', 'pi']
  → _alpha、__beta 一个都没进来：默认过滤器就是「下划线开头一律不给」，一个字节都不商量。
    在 `import *` 这一条规则里，前导下划线是**语法**，不是「我自己看的注释」。

=== 5. __all__ 一旦出现就是唯一白名单：私有能放进来，公开能被挡在外面 ===
  m_all.py 里写着 __all__ = ['pi', '_hidden']｜`from m_all import *` 之后本地名字 → ['_hidden', 'pi']
  对照（白名单只管星号，不管 getattr）→ 模块里其实有： ['pi', 'public_thing'] ; dir() 里还有 public_thing 吗： True
  ★ 白名单里写了不存在的名字 → rc=1 ｜ 原文 → AttributeError: module 'm_missing' has no attribute 'nope'
  ★ 白名单类型写错（__all__ = 5）→ rc=1 ｜ 原文 → TypeError: 'int' object does not support indexing
  → 所以 __all__ 不是 README，是 `*` 的定义域：写了它就要对每个名字负责，漏一个就是运行时响。
```

星号实际给进来的只有 `gamma`、`pi` 两个，`_alpha`、`__beta` 一个都没进来 —— 在 `import *` 这条规则里，前导下划线是语法不是注释。
`__all__` 一旦出现就是唯一白名单：它里面同时写了公开名和私有名之后，私有名字能进来，而模块里那个公开名字被挡在星号外面（但 `dir()` 里还在，白名单只管星号不管 getattr）。
白名单里写了不存在的名字或把类型写成 `5`，屏幕上各响一条。星号还有两条硬边界：

```
=== 6. 星号的两条硬边界：它会无声覆盖同名对象，而且进不了函数体 / 类体 ===
  驱动 `star_local.py`（本地先有 pi = 9.99，再 from m_pub import *）：rc=0
      导入前本地公开名字 = ['pi']
      导入后本地公开名字 = ['gamma', 'pi']
      pi 现在是 → 3.14159 （原来是 9.99）
      新增的名字（排序）→ ['gamma']
      __name__ 还在吗、有没有被冲掉 → __main__
      _alpha 进来过吗 → False ｜ __beta 呢 → False
  → pi 被覆盖成模块里那个 3.14159，屏幕上不会有任何警告；双下划线名字反而安全（默认过滤器不给）。
  函数体里用星号（`python3 star_fn.py`）｜ rc=1 ｜ 这个文件一行标准输出都没有 → True
        File "guard-demo/star_fn.py", line 2
          from m_pub import *                    # 函数体里用星号
                            ^
      SyntaxError: import * only allowed at module level
  类体里用星号（`python3 star_cls.py`）｜ rc=1 ｜ 这个文件一行标准输出都没有 → True
        File "guard-demo/star_cls.py", line 2
          from m_pub import *                    # 类体里用星号
                            ^
      SyntaxError: import * only allowed at module level
  → 本机 3.14.4 给的是**编译期** SyntaxError：文件里一行代码都没执行就死了（注意上面 rc=1、无 stdout）。
    原因很实在：函数体/类体的名字集合要到运行时才知道，而星号要求编译期就把命名空间定下来。
  ★ 更早的版本在这里是 SyntaxWarning 还是 SyntaxError —— 未验证，别当事实引用。
```

本地先把 `pi` 设成 9.99，`from m_pub import *` 之后 `pi` 被换成了模块里那个 3.14159 —— 同名对象被**无声**覆盖，屏幕上不会有任何警告；双下划线名字反而安全（默认过滤器不给）。
函数体和类体里用星号，本机 3.14.4 给的是**编译期**的 `SyntaxError`：`rc=1` 且这个文件一行标准输出都没有，因为名字集合要到运行时才知道，而星号要求编译期就把命名空间定下来。
最后收口表把「你敲的东西 → 本机字段 → 后果」并排列出来：

```
=== 7. 收口：guard 不是仪式，它决定「这个文件被当脚本还是被当模块」 ===
  你敲的东西               │ 本机实测的字段（第 1 节原文缩写）              │ 后果
  python3 x.py             │ __name__='__main__' __package__=None          │ 相对导入必炸；guard 里的主逻辑跑
  python3 -m pkg.x         │ __name__='__main__' __package__='pkg'         │ 相对导入可用；pkg/__init__ 先跑一遍
  import x / import pkg.x  │ __name__='x' / 'pkg.x' __package__='' / 'pkg' │ guard 里的主逻辑一次都不跑
  计数小结（本机）：无 guard 被 import → 主逻辑 1 次 + 副作用文件建了；有 guard 被 import → 主逻辑 0 次、文件没建、但模块体仍执行 1 次
  星号小结（本机）：默认过滤下划线开头 ｜ __all__ 是唯一白名单（私有进、公开挡）｜ 同名对象被无声覆盖 ｜ 函数体与类体里直接 SyntaxError
  三条判据：
  ① 这个文件只要可能被人 import，主逻辑就必须待在 guard 里 —— 不写就是把执行权交出去。
  ② 包里的东西用 `-m` 起，别用路径起：__package__ 决定相对导入的生死（第 3 节两条原文互为对照）。
  ③ 库要暴露 API 就显式写 __all__ 并保证每个名字都对得上；应用代码里别写星号 —— 覆盖是无形的。
  ★ 落点：__name__ / __package__ 不是玄学标签，是解释器在启动那一刻写好的两个变量，它们替你决定了这一份源码是谁、能看见谁。
  收尾自证：guard-demo/ 还在吗 → False ｜ 残留条目 0 个 ｜ 本次起真子进程 22 个 ｜ 沙箱里的 __pycache__ 随它一起删除
  当前目录的 __pycache__ 有没有被本脚本新增 → False
  输出自检：出现绝对路径 / 临时目录 / 登录名的行数 = 0
```

三行判据浓缩：`python3 x.py` 时 `__package__` 是 None、相对导入必炸；`-m pkg.x` 时 `__package__` 是 `pkg`、相对导入可用且父包 `__init__` 先跑；被 `import x` 时 guard 里的主逻辑一次都不跑。

**这一节带走：**

- `if __name__ == "__main__":` 不是仪式：没有它，别人 import 你一下你就把整个脚本（主逻辑 + 副作用）跑一遍，本节用计数证明。
- `__name__` 和 `__package__` 由启动方式决定；脚本模式 `__package__=None`（相对导入必炸），`-m` 模式有包名（相对导入可用）。
- 包里的东西用 `-m` 起，别用路径起：路径起会让 `sys.path[0]` 变成脚本目录，绝对导入拿到的是另一个模块实例。
- 星号默认只放不以下划线开头的名字；`__all__` 一旦出现就是唯一白名单（私有能进、公开能挡）。
- 星号会无声覆盖同名对象，且在函数体 / 类体里直接编译期 SyntaxError。

---

## 第 14 节 · g 篇完整跑：把 import 语句还原成可以编程调用的协议（查、载、重载）

这一节在主线① 的「谁在查」那一侧：`find_spec` 查的是账本之外那条 finder 链，`exec_module` 才真的执行模块体，`reload` 只是再跑一遍。
先造现场：一个普通包、一个命名空间包、一个不在 `sys.path` 里的插件文件，屏幕上打出 `sys.meta_path` 上的四个 finder：

```
$ python3 g_importlib_and_spec.py
=== 1. 造现场：一个包、一个命名空间包、一个不在 sys.path 里的插件文件 ===
  沙箱 importlib-demo/ 里的文件（排序，相对当前目录）：
    importlib-demo/pkgs/nspkg/leaf.py
    importlib-demo/pkgs/pkg/__init__.py
    importlib-demo/pkgs/pkg/sub.py
    importlib-demo/pkgs/rload.py
    importlib-demo/plugins/dyn.py
  三个角色的区别就在有没有 `__init__.py`：
    pkg/    有 __init__.py → 普通包；nspkg/  有 __init__.py 吗 → False（所以它是命名空间包）
    plugins/dyn.py  所在目录在 sys.path 里吗 → False —— 只能按路径载
  已把 importlib-demo/pkgs 插到 sys.path[0]（这是 importlib 的用法演示，不是给读者抄的常规操作）
  现在 sys.meta_path 上的 finder：['DistutilsMetaFinder', 'BuiltinImporter', 'FrozenImporter', 'PathFinder']
  → 查一个名字时，依次问过这些 finder；importlib.util.find_spec 就是这条链的可调用入口。
```

`nspkg/` 没有 `__init__.py`（所以它是命名空间包），`plugins/dyn.py` 所在目录不在 `sys.path` 里（只能按路径载）。查一个名字时依次问过这四个 finder，`find_spec` 就是这条链的可调用入口。
第一个反直觉点在返回值上 —— `import_module` 和 `__import__` 载进来的东西一样，给你的却不一样：

```
=== 2. import_module 和 __import__：载进来的东西一样，给你的返回值不一样 ===
  importlib.import_module('pkg.sub')  → 返回对象的 __name__ = 'pkg.sub'
  __import__('pkg.sub')               → 返回对象的 __name__ = 'pkg'   ★顶层包，不是 pkg.sub
  __import__('pkg.sub', fromlist=[…])  → 返回对象的 __name__ = 'pkg.sub'
  import_module 的返回值和 fromlist 版 __import__ 是同一个对象吗 → True
  import_module 的返回值和裸 __import__ 是同一个对象吗 → False
  但 sys.modules 里留下的键（排序）：['pkg', 'pkg.sub']
  顶层包对象上能不能直接摸到子模块 → hasattr(via_builtin, 'sub') = True
  → `import pkg.sub` 这条**语句**等价于裸 __import__：只把 pkg 绑进当前命名空间，所以 `sub.value` 用不了、必须写全 `pkg.sub.value`；这不是语法挑剔，是返回值本来就是 pkg。
  → 判据：名字是动态的（配置里读出来的、用户传的）就用 import_module，它给的对象和你想的一样。
```

屏幕上 `import_module('pkg.sub')` 那个返回对象的 `__name__` 是 `pkg.sub`，而裸 `__import__('pkg.sub')` 给的是 `pkg`（顶层包）；两者留在 `sys.modules` 里的键都是 `pkg` 和 `pkg.sub` 两条。
`import pkg.sub` 这条**语句**等价于裸 `__import__`：只把 `pkg` 绑进命名空间，所以 `sub.value` 用不了、必须写全 —— 这不是语法挑剔，是返回值本来就是 pkg。判据：名字是动态的就用 `import_module`。
`find_spec` 的三种结局加第四种脾气：

```
=== 3. find_spec 的三种结局 + 第四种脾气（它是唯一「查而不载」的入口）===
  ① 标准库 textwrap：origin → <base>/lib/python3.14/textwrap.py（basename = textwrap.py）｜ submodule_search_locations → None
     对比包内模块 pkg.sub：origin basename = sub.py｜ cached 是 .pyc 吗 → True｜ 相对位置 importlib-demo/pkgs/pkg/sub.py
  ② 查不到的名字 no_such_module_xyz：find_spec → None   ★返回 None，不抛异常（这是它和 import_module 最大的性格差别）
     同一个名字交给 import_module → 抛 ModuleNotFoundError: No module named 'no_such_module_xyz'
  ③ 命名空间包 nspkg（目录里没 __init__.py）：origin → None ｜ loader → None
     但 submodule_search_locations 有值：类型 _NamespacePath、长度 1、位置 ['importlib-demo/pkgs/nspkg']
     → 判据：origin 是 None 而 search_locations 非 None，就是命名空间包；「一个包被拆在多个目录里」靠的就是这个（PEP 420）。
  ④ ★第四种脾气：find_spec('no_such_parent.child') 不返回 None，抛 ModuleNotFoundError: No module named 'no_such_parent'
     对比：父包存在、子模块不存在时它又回到返回 None —— find_spec('pkg.no_sub') → None
  → 所以拿 find_spec 做「存在性检查」必须写成两层：整个调用包 try，返回值再判 None。
```

① 查得到就给一个 spec；② 查不到的名字 `find_spec` 返回 None 且**不抛异常**（同一个名字交给 `import_module` 就抛 `ModuleNotFoundError`）；③ 命名空间包那一格 `origin` 是 None，而 `submodule_search_locations` 有值。
第四种脾气最坑：父包本身就不存在时，`find_spec` 不返回 None，而是直接抛 `ModuleNotFoundError`；只有父包在、子模块缺的时候才回到返回 None。所以拿它做存在性检查得写成两层（调用包 try、返回值再判 None）。
从任意路径把一个文件装成模块，是三件套，外加 `__name__` 你说了算：

```
=== 4. 从任意路径把一个文件装成模块：三件套，外加 __name__ 是你说了算 ===
  文件在 importlib-demo/plugins/dyn.py；先确认按名字根本找不到它：find_spec('dyn') → None
  spec_from_file_location("alpha", …) → name='alpha'、loader=SourceFileLoader、origin=importlib-demo/plugins/dyn.py
  module_from_spec 之后还没执行它：模块里已有的名字 → （空）   ★此时 STATE/bump 都不存在，少了 exec_module 就是拿到一个空壳
  exec_module 之后：__name__ = 'alpha' ｜ bump() → 1 ｜ STATE = {'count': 1}
  ★文件名是 dyn.py，__name__ 却是 'alpha' —— 这个名字是**我传给 spec_from_file_location 的**，跟文件名无关；`if __name__ == '__main__'` 那道闸门在这里由你决定放不放行。
  exec 完了 sys.modules 里有 'alpha' 吗 → False   ★手加载**不会**自动注册，注册是你自己的活（下一行就手动补上）
  手动注册后：sys.modules['alpha'] is mod_a → True
  → 不注册会踩的坑：模块内部第二次 import 自己会再执行一遍模块体（拿到第二份常量），dataclass/enum/异常类这些「按类对象认身份」的代码会当场精神分裂。
```

`spec_from_file_location("alpha", …)` → `module_from_spec` → `exec_module`。屏幕上文件名是 `dyn.py`、`__name__` 却是 `'alpha'`，因为这个名字是你传给 `spec_from_file_location` 的。
少了 `exec_module` 就是个空壳；`exec_module` 之后 `sys.modules` 里 `有 'alpha' 吗 → False` —— 手加载**不会**自动注册，注册是你自己的活。不注册会踩的坑：模块内部第二次 import 自己会再执行一遍模块体，拿到第二份常量。
同一个文件、两个名字 = 两份独立状态，用计数证明：

```
=== 5. 同一个文件、两个名字 = 两份独立状态（用计数证明，不是嘴上说）===
  两次 exec 的是同一个文件 importlib-demo/plugins/dyn.py，只是名字不同。
  mod_a.bump() → 2
  mod_a.bump() → 3
  mod_b.bump() → 1   ★还是 1：它数的是自己那份 STATE
  STATE 是同一个对象吗 → False ｜ bump 是同一个函数对象吗 → False
  各自的 STATE 现在：alpha={'count': 3} beta={'count': 1}
  sys.modules 里这两个键都在：['alpha', 'beta']
  → 判据：import 的**身份是名字（sys.modules 的键），不是文件**。同一个 .py 被两个名字引进来就是两个实例，模块级单例、注册表、计数器全部各算各的。
  ★ 这也是「同一个包既被绝对导入又被相对导入」时全局状态凭空分裂的根因，插件框架最容易在这里漏一次初始化。
```

两次 exec 的是同一个 `dyn.py`，只是名字不同。`mod_a.bump()` 数到 3，`mod_b.bump()` 还是 1（各数自己那份 `STATE`）；`STATE 是同一个对象吗 → False`。判据：import 的身份是名字（`sys.modules` 的键），不是文件。
最后是 reload 的真实语义（第 15 节还会单独放大）：

```
=== 6. reload 的真实语义：模块体重跑，旧引用一个都不换 ===
  reload 前：VERSION=1、模块体已跑 1 次、greet.__defaults__=('before',)、greet() → 'hi before'
  reload 返回的对象就是原来那个模块 → True ｜ sys.modules['rload'] 还是它吗 → True
  reload 后：VERSION=2、模块体跑了 2 次   ★RUN_COUNT 能累加，说明它是**在原 __dict__ 上就地重跑**，不是新建模块
  模块属性 rmod.greet 现在指向新的函数对象（旧的是 old_greet）→ True
  但旧函数还是旧的：old_greet.__defaults__=('before',)、old_greet() → 'hi before'
  新函数：rmod.greet.__defaults__=('after',)、rmod.greet() → 'HI after'
  ★旧实例的 isinstance 当场失效：isinstance(old_instance, rmod.Widget) → False（它的 __class__ 还是 reload 前那个 Widget 类对象）
  已经 in-flight 的默认参数、已经建好的实例、别处 `from rload import greet` 拿到的名字，reload 一个都不会替它们换掉。
  → 判据：reload 的作用域只有「模块自己重跑一遍」。它不清缓存、不改引用、不重启已运行的代码；想真正干净，只有新进程这一条路。
```

`reload 返回的对象就是原来那个模块 → True`、`RUN_COUNT` 从 1 累加到 2，说明它在原 `__dict__` 上就地重跑。但模块属性指向了新函数，旧的 `old_greet` 还是旧的默认参数，`isinstance(old_instance, rmod.Widget) → False` —— 已绑定的引用一个都不换。
什么时候才该动 `importlib`？收口段说只有三类（插件注册、按路径载单文件、测试里要一份干净状态），它的**第一代价**是「你自己接管了失败」：`find_spec` 只给 None、`exec_module` 抛的是模块体里的原始异常、缓存与注册全归你。

**这一节带走：**

- `import_module('pkg.sub')` 返回 `pkg.sub`，裸 `__import__('pkg.sub')` 返回顶层包 `pkg`；`sys.modules` 里两者都留下完整两条键。
- `find_spec` 是「查而不载」：查不到返回 None 不抛，但父包本身不存在时又抛 —— 同一函数两种脾气，存在性检查得防两层。
- 手加载三件套里 `__name__` 由你定，且 `exec_module` 不会自动注册到 `sys.modules`，不注册会让模块体多跑一遍。
- import 的身份是名字不是文件：同一份 `.py` 被两个名字引进来就是两份独立状态。
- 走 `importlib` 换来「名字/路径/时机」三个自由度，付出去的是失败路径全部自己写。

---

## 第 15 节 · 小证据：reload 只重跑模块体，你手里的旧引用一个都不换

这一节在主线① 上，是第 14 节 reload 那格的独立复现：`rmod.py` 里 `VER = 1` 和一个类 `K`，用一个新进程把它换掉再 reload。

```
$ rm -rf reload-check && mkdir -p reload-check && cd reload-check
cat > rmod.py <<PYA
VER = 1
class K:
    pass
PYA
cat > rmod_v2.py <<PYB
VER = 2
print("  [这一行证明模块体又跑了一遍]")
class K:
    pass
PYB
cat > probe.py <<PYC
import importlib, os, shutil, sys
import rmod
old_ver, old_k, old_obj = rmod.VER, rmod.K, rmod.K()
print("① 模块对象的身份：reload 之前 sys.modules 里那条就是它吗 →", sys.modules["rmod"] is rmod)
before = os.stat("rmod.py")
shutil.copyfile("rmod_v2.py", "rmod.py")
after = os.stat("rmod.py")
print("   换掉源码：mtime 变了吗 →", after.st_mtime != before.st_mtime, "｜ 字节数也变了吗 →", after.st_size != before.st_size, "（两个判据都动了，缓存必然重做）")
loaded = importlib.reload(rmod)
print("② reload 的返回值就是你手里那个模块对象吗 →", loaded is rmod, "｜ __dict__ 里的 VER 更新了吗 →", rmod.VER == 2)
print("③ 但你手里的旧引用一个都不换：old_ver 还是", old_ver, "｜ rmod.K 和旧 K 是同一个类吗 →", rmod.K is old_k)
print("   旧实例的 isinstance 还认吗 →", isinstance(old_obj, rmod.K), "← 这一条就是「reload 之后到处冒幽灵报错」的根因")
print("④ 结论：reload 改的是那份 __dict__，不接管你已经绑定走的名字；要拿新东西得重新取属性")
PYC
python3 probe.py
cd .. && rm -rf reload-check
① 模块对象的身份：reload 之前 sys.modules 里那条就是它吗 → True
   换掉源码：mtime 变了吗 → True ｜ 字节数也变了吗 → True （两个判据都动了，缓存必然重做）
  [这一行证明模块体又跑了一遍]
② reload 的返回值就是你手里那个模块对象吗 → True ｜ __dict__ 里的 VER 更新了吗 → True
③ 但你手里的旧引用一个都不换：old_ver 还是 1 ｜ rmod.K 和旧 K 是同一个类吗 → False
   旧实例的 isinstance 还认吗 → False ← 这一条就是「reload 之后到处冒幽灵报错」的根因
④ 结论：reload 改的是那份 __dict__，不接管你已经绑定走的名字；要拿新东西得重新取属性
[退出码 0]
```

屏幕第一行先钉死身份：`reload 之前 sys.modules 里那条就是它吗 → True`。中间换源码那行 `mtime 变了吗 → True ｜ 字节数也变了吗 → True`，两个判据都动了，缓存必然重做，所以那句 `[这一行证明模块体又跑了一遍]` 才印得出来。
第二行 `reload 的返回值就是你手里那个模块对象吗 → True`，且 `__dict__` 里的 `VER` 更新成了 2。真正扎手的是第三行：`old_ver 还是 1`（旧引用不换），`rmod.K 和旧 K 是同一个类吗 → False`（类被重新定义了）。
最底下那行 `旧实例的 isinstance 还认吗 → False` —— 这一条就是「reload 之后到处冒幽灵报错」的根因。结论写在屏幕最后：reload 改的是那份 `__dict__`，不接管你已经绑定走的名字；要拿新东西得重新取属性。
这也顺带解释了第 09 节为什么「reload 救不了循环」：环里缺的是别的模块的模块体跑完，而你手里那批函数、类、实例的引用在 reload 之后还是旧对象 —— 它只重跑，不接管。

**这一节带走：**

- reload 原地重跑模块体：`VER` 变了、类 `K` 换成了新对象、模块体里那句 print 再响一次。
- reload 返回的就是你手里那个模块对象（身份不变），`sys.modules` 那条键也没换。
- 你早先绑定的旧引用（`old_ver`、旧 `K`、旧实例）一个都不换，旧实例的 `isinstance` 当场 False。
- 想拿到新东西必须重新取属性；要让代码真正干净，只有起新进程这一条路。

---

## 第 16 节 · h 篇完整跑：venv 到底改了什么（本机没有 ensurepip，这条失败也当证据演）

结论先摆这儿：venv 的全部魔法就是「一个目录 + 一份 cfg + 一堆相对链接」换来的 `sys.path` 改写。
它既不复制解释器，也不装任何包。而本机这台 Ubuntu 连 pip 引导的零件都没给，于是那条失败本身也被当证据演了一遍。

```
$ python3 h_venv_mechanics.py
=== 1. 本机前提：ensurepip 不在，所以只有 --without-pip 建得出来 ===
  子进程 `python3 -c "import ensurepip"` → 退出码 1
  报错原文（最后一行）：ModuleNotFoundError: No module named 'ensurepip'
  python3 -m venv --without-pip <目标> → 退出码 0 ｜ 输出行数 0
  python3 -m venv --without-pip --system-site-packages <目标> → 退出码 0 ｜ 输出行数 0
  → 建出来的骨架顶部（排序 + 类型）：[('.gitignore', 'file'), ('bin', 'dir'), ('include', 'dir'), ('lib', 'dir'), ('lib64', 'link'), ('pyvenv.cfg', 'file')]
  → 判据：venv 的「骨架」和「pip」是两个独立步骤。骨架是纯文件操作（建目录、连链接、写 cfg），pip 引导要靠 stdlib 里的 ensurepip 模块 —— 发行版可以只给你前者。
```

本机前提就这四行：`import ensurepip` 的子进程退 1，报错原文一行；两条带 `--without-pip` 的建法都退 0、输出 0 行，
骨架顶部六项一个不少 —— 两个文件（`.gitignore`、`pyvenv.cfg`）、三个目录（`bin`、`include`、`lib`）、一条 `lib64` 链接。
所以「venv 建不出来」和「venv 的机制不灵」是两件事：骨架是纯文件操作，pip 引导要靠 stdlib 里那个模块，发行版可以只给你前者。

```
=== 2. 拆骨架：bin/ 里全是相对符号链接，pyvenv.cfg 是唯一配置 ===
  venv-demo/demo1/bin 里的名字（排序）：['Activate.ps1', 'activate', 'activate.csh', 'activate.fish', 'python', 'python3', 'python3.14', '𝜋thon']
  逐个判「是链接还是文件」——只报 basename，不报绝对路径：
    Activate.ps1 islink=False  普通文件，行数=248
    activate     islink=False  普通文件，行数=76
    activate.csh islink=False  普通文件，行数=27
    activate.fish islink=False  普通文件，行数=69
    python       islink=True   readlink=python3              目标写得绝对吗=False
    python3      islink=True   readlink=<base>/bin/python3   目标写得绝对吗=True
    python3.14   islink=True   readlink=python3              目标写得绝对吗=False
    𝜋thon        islink=True   readlink=python3              目标写得绝对吗=False
  → 注意 python3 这一行：它直接链到基座里的绝对路径，另外三个（python / python3.14 / 𝜋thon）链的是同目录的 python3。★所以 bin/ 里「相对」和「绝对」两种链接混着，搬目录不一定坏，换机器一定坏（第 7 段实测）。
  lib 底下（排序相对路径）：['venv-demo/demo1/lib/python3.14', 'venv-demo/demo1/lib/python3.14/site-packages']
  ★ venv 自己的 site-packages（venv-demo/demo1/lib/python3.14/site-packages）此刻是空的：是目录吗 → True ｜ 里面有几项 → 0
  include/ 里有什么 → []（本机实测：空占位）
  pyvenv.cfg 逐行读（绝对路径一律折成 <base>/ 或 <cwd>/）：
    home                         = <base>/bin                                     绝对路径吗=True
    include-system-site-packages = false                                          绝对路径吗=False
    version                      = 3.14.4                                         绝对路径吗=False
    executable                   = <base>/bin/python3.14                          绝对路径吗=True
    command                      = <base>/bin/python3 -m venv --without-pip <cwd>/venv-demo/demo1 绝对路径吗=True
  → 判据：这个文件里写死了 home / executable / command 三条绝对路径 —— 它就是「venv 不能跨机器搬」的物证（第 7 段回收）。
```

直觉以为 `bin/` 里全是相对链接，屏幕上不是。八个名字里四个激活脚本是普通文件（`activate` 那一份 76 行）；
另外四条 `python*`（连一个 𝜋 开头的都在）确实是链接，可只有 `python3` 这一条 readlink 到基座的绝对路径，
其余三条链的是同目录的 `python3`。venv 自己的 `site-packages` 建完 0 项，`include/` 是空占位。
`pyvenv.cfg` 全文五行，其中 `home`、`executable`、`command` 三条写着绝对路径 —— 「不能跨机器搬」的物证就是这几行。

```
=== 3. 关键判据：venv 不复制解释器，同一个 Python 换个归属 ===
  观察项                        python3（基座）                 venv-demo/demo1/bin/python
  sys.prefix == sys.base_prefix True                            False
  sys.prefix 折成相对           <base>                          venv-demo/demo1
  sys.base_prefix 折成相对      <base>                          <base>
  sys.executable 折成相对       <base>/bin/python3              venv-demo/demo1/bin/python
  realpath(sys.executable)      <base>/bin/python3.14           <base>/bin/python3.14
  tuple(sys.version_info)       (3, 14, 4, 'final', 0)          (3, 14, 4, 'final', 0)
  site.ENABLE_USER_SITE         True                            False
  两边 realpath 是同一个文件吗 → True ｜ inode 相同吗 → True ｜ 字节数相同吗 → True
  version_info 相等吗 → True；bin/ 下 4 个 python* 链接的 realpath 都指向同一个文件 → True
  → 判据：venv 里没有任何「第二个解释器」。它只是在 sys.executable 的 realpath 上方两层放了个 pyvenv.cfg，Python 启动时读到它，就把 sys.prefix 改成那个目录（site.venv() 干的）。
  ★ 代价：同一份标准库、同一份 lib-dynload。venv 隔离的是**第三方包目录**，标准库版本要变只能换解释器（pyenv/conda/换发行版那一类，本机未验证，别当事实引用）。

=== 4. sys.path 被换成了什么 + include-system-site-packages 开关实测 ===
  计数项                    基座 / demo1 / demo2（依次三个数）
  len(sys.path)           6 / 5 / 7
  含 site-packages 的条数 0 / 1 / 1
  含 dist-packages 的条数 2 / 0 / 2
  包目录总条数            2 / 1 / 3
  标准库目录还在吗        True / True / True
  demo1 的包目录（排序，折叠）：['venv-demo/demo1/lib/python3.14/site-packages']
  demo2 的包目录（排序，折叠）：['<base>/lib/python3/dist-packages', '<base>/local/lib/python3.14/dist-packages', 'venv-demo/demo2/lib/python3.14/site-packages']
  venv 自己的 site-packages 排在包目录最前吗 → demo1=True demo2=True
  ★ 数 'site-packages' 这个朴素判据在本机**失灵**：demo1 和 demo2 都只数到 1 条，差别全在 dist-packages 那 2 条上 —— Debian/Ubuntu 把系统包目录改名成了 dist-packages，标准库以外的路径也照这个名字装。
  → 真正的开关效果要这样读：demo1 的 dist-packages 条数 0 → demo2 的 2；开了系统包后，基座里已装的包在 venv 里就直接可见（隔离等于没做）。
  顺带实测：venv 里 ENABLE_USER_SITE 是 False（基座是 True）—— venv 顺手把家目录那个 ~/.local 通道关了，免得用户级安装包搅浑环境。
```

第 3 段是关键判据：两边 `realpath(sys.executable)` 是同一个文件、inode 相同、字节数相同、`version_info` 全等，
唯一变的是 `sys.prefix == sys.base_prefix` 从 True 翻到 False。venv 里没有第二个解释器，
它只是在 `sys.executable` 的 realpath 往上两层放了一份 cfg，Python 启动读到它就把 `sys.prefix` 改成那个目录（`site.venv()` 干的）。
代价跟着来：标准库和 `lib-dynload` 是同一份，标准库版本要变只能换解释器。
第 4 段那张计数表要这样读：`len(sys.path)` 是 6 / 5 / 7，真实包目录是 2 / 1 / 3，而「数一下 `site-packages`」这个朴素判据
两条 venv 都只数到 1 —— 本机系统包目录叫 `dist-packages`，差别全藏在那两条里。`ENABLE_USER_SITE` 基座 True、venv False。

```
=== 5. activate 只干了几行 export：手工设 env 起子进程，魔法为零 ===
  bin/activate 共 76 行，其中真正改环境的就这些（去重排序，绝对路径已折）：
    PATH="$VIRTUAL_ENV/"bin":$PATH"
    PATH="${_OLD_VIRTUAL_PATH:-}"
    VIRTUAL_ENV=$(cygpath <cwd>/venv-demo/demo1)
    _OLD_VIRTUAL_PATH="$PATH"
    _OLD_VIRTUAL_PS1="${PS1:-}"
    _OLD_VIRTUAL_PYTHONHOME="${PYTHONHOME:-}"
    export PATH
    export PS1
    export PYTHONHOME
    export VIRTUAL_ENV
    export VIRTUAL_ENV=<cwd>/venv-demo/demo1
    export VIRTUAL_ENV_PROMPT
  （其中 `$(cygpath …)` 那行在 Cygwin 分支里，本机走不到；剩下的就是备份旧值 → 设新值 → deactivate 时还原，三件事。）
  实验 A：`venv-demo/demo1/bin/python` 在 PATH 里没有它、VIRTUAL_ENV 也没设的情况下 → sys.prefix = venv-demo/demo1
  实验 B：基座 python3，但手工塞好 VIRTUAL_ENV 和指向 demo1/bin 的 PATH → sys.prefix = <base>、sys.path 里多了 venv 的 site-packages 吗 → False ★prefix 没变，环境变量本身不制造 venv
  实验 C：`python3` 这个词命中哪个文件 —— 不设 PATH → <base>/bin/python3；把 demo1/bin 放最前 → <cwd>/venv-demo/demo1/bin/python3
  同法测 `python`（注意本机基座里根本没有 python 这个名字）：不设 → None；设了 → <cwd>/venv-demo/demo1/bin/python
  → 判据：激活 = 改 PATH（让 python 这个词命中 venv/bin 里那个链接）+ 设 VIRTUAL_ENV/PROMPT（给提示符和外部工具看）。真正让 sys.path 换掉的是解释器**自己被从哪个目录启动**，不是这些变量。
  ★ 代价：`which python3` 对了不代表环境对了 —— 子 shell、nohup、cron、systemd、IDE 的解释器路径都不走你这个 shell 的 PATH，「忘了激活」的报错永远长得像「包没装」。
```

`activate` 看着神秘，真改环境的只有围栏里那十几行：备份旧值、设 `PATH` / `VIRTUAL_ENV` / `VIRTUAL_ENV_PROMPT` /
（清空后的）`PYTHONHOME`、`deactivate` 时还原；带 `cygpath` 的那行在 Cygwin 分支里，本机走不到。
三个实验才是骨头：A 直接跑 venv 里的 `bin/python`，`sys.prefix` 就是它；B 拿基座解释器手工塞好那两个变量，
`sys.prefix` 不动、venv 的 `site-packages` 也没进账（False）—— 环境变量本身不制造 venv；
C 只回答「`python3` 这个词命中哪个文件」，顺带暴露本机基座根本没有 `python` 这个名字。
于是有个扎人的推论：`which python3` 对了不代表环境对了，子 shell、nohup、cron、systemd、IDE 都不走你这个 shell 的 PATH。

```
=== 6. 诚实交代：本机的 pip 从哪一步断的（原文照打，不装、不修）===
  venv-demo/demo1/bin/python -m pip --version → 退出码 1
    <cwd>/venv-demo/demo1/bin/python: No module named pip
  体检表里那条 PIP_IMPORTABLE：基座=False、demo1=False → 两边都没有 pip（这台机器就没装 pip）
  现在试**带 pip 引导**的 `python3 -m venv venv-demo/broken` → 退出码 1，原文：
    The virtual environment was not created successfully because ensurepip is not
    available.  On Debian/Ubuntu systems, you need to install the python3-venv
    package using the following command.
    
        apt install python3.14-venv
    
    You may need to use sudo with that command.  After installing the python3-venv
    package, recreate your virtual environment.
    
    Failing command: <cwd>/venv-demo/broken/bin/python3
  ★ 这段原文走的是 **stdout**：把 stdout 丢进 /dev/null 后 stderr 只剩 0 行 → CI 里只截 stderr 会看不到它。
  失败之后磁盘上留下了什么（排序相对路径）：
    venv-demo/broken/.gitignore                    file
    venv-demo/broken/bin                           dir
    venv-demo/broken/bin/python                    link
    venv-demo/broken/bin/python3                   link
    venv-demo/broken/bin/python3.14                link
    venv-demo/broken/bin/𝜋thon                     link
    venv-demo/broken/include                       dir
    venv-demo/broken/lib                           dir
    venv-demo/broken/lib/python3.14                dir
    venv-demo/broken/lib/python3.14/site-packages  dir
    venv-demo/broken/lib64                         link
    venv-demo/broken/pyvenv.cfg                    file
  bin/activate 建出来了吗 → False ｜ 骨架建到一半就退了 → 这也是「先判断 rc、再判断目录存在」不能省的原因。
  → 本机断的是发行版没打包 python3-venv（ensurepip 被剥掉了），**不是 venv 的机制问题**；补法是在有网的机器上 `apt install python3.14-venv` —— 那是读者的动作，本章脚本绝不执行任何安装。
```

第 6 段是本机的诚实交代：venv 里 `-m pip --version` 退 1，基座和 venv 两边都 import 不到 pip —— 这台机器根本没装。
带 pip 引导的建法退 1，整段原文在围栏里，它点名要装的那个包本章不装也不修。
两个细节值得抄进教训：这段原文走 **stdout**（把 stdout 丢进 `/dev/null` 之后 stderr 只剩 0 行，CI 只截 stderr 就看不见它）；
失败之后磁盘上还留着 12 个条目，但 `bin/activate` 没建出来 —— 「先判断 rc、再判断目录存在」不能省。

```
=== 7. 收口：venv 做的三件事、不做的三件事（含搬目录实测）===
  把 demo1 整份搬到 venv-demo/moved 后，`moved/bin/python` 的 sys.prefix → venv-demo/moved
  解释器侧照样能用吗 → True（链接是相对的、cfg 里的 home 还在，所以**同一台机器上挪目录碰巧不坏**）
  可 moved/bin/activate 里：还写着 demo1 的老地址吗 → True；改成新地址了吗 → False ★激活脚本是建环境那一刻写死的，搬完的 venv 一激活就废
  pyvenv.cfg 的 command 那行也还记着建它时的绝对路径（第 2 段已打出），换一台机器、换一个前缀目录、Windows 换盘符，这些行全都对不上。
```

第 7 段拿搬目录收口：同一台机器上把骨架整份搬走，`sys.prefix` 跟着新目录变、解释器照样能用（链接是相对的、cfg 里 `home` 还在），
可 `activate` 里仍写着老地址、没改成新地址 —— 搬完的 venv 一激活就废。
「做的三件事」：自己的 `site-packages`（空着也占住包目录第一位）、自己的 `bin/`（激活后排 `PATH` 最前）、
`VIRTUAL_ENV` 与 `VIRTUAL_ENV_PROMPT`。「不做的三件事」：不隔离标准库、不复制解释器也不装包、不能跨机器搬。
捕获自己也划了线：Windows 的 `Scripts\` 布局、macOS framework 版的路径形态、装了 python3-venv 之后 pip 引导成功的完整输出，
本机一条都没测到 —— 这三样只能当交代，不能当证据。

**这一节带走：**

- venv 里没有第二个解释器：`realpath`、inode、字节数、`version_info` 四条全等，变的只有 `sys.prefix` 这个归属标记。
- `pyvenv.cfg` 是唯一配置，`home`/`executable`/`command` 三条绝对路径就是它搬不动的物证；`bin/` 里相对与绝对链接混着用。
- 激活只是改 `PATH`：手工设变量起子进程，魔法为零。真正换 `sys.path` 的是解释器从哪个目录被启动。
- 隔离只针对第三方包目录，标准库一条没少；本机包目录叫 `dist-packages`，只数 `site-packages` 会数漏。
- 本机的可跑形态只有 `--without-pip`。带 pip 引导的失败原文和它留下的半截骨架，都是本章留下来的证据。

---

## 第 17 节 · 现场格：venv 骨架就那三样，bin/ 里全是符号链接（沙箱里现建现删）

这一格不谈道理，只把骨架摊在桌面上数一遍：建它只要一条命令，家当只有三样，`bin/` 里那一堆链接就是全部，
建完立刻删 —— 它是沙箱里的临时物，章目录和你家目录都不参与。

```
$ python3 -m venv --without-pip venv-check/vv
[退出码 0]
↑ 这条命令跑在沙箱里（当前目录就是 work/），跑完由下一格删掉；章目录和你家目录都不参与。
```

`--without-pip` 是这台机器上唯一建得出来的形态（原因在第 16 节）。退 0，一个字节都不打。

```
$ 〔在 work 里〕 python3 -c "
import os, sys
root = \"venv-check/vv\"
missing = [p for p in (\"pyvenv.cfg\", \"bin\", \"lib\") if not os.path.exists(os.path.join(root, p))]
print(\"① 骨架三样齐了吗 →\", not missing, \"｜ 缺的：\", missing or \"无\")
print(\"   pyvenv.cfg 里只有键名（值里带绝对路径，一律不上屏）：\", sorted({ln.split(\"=\")[0].strip() for ln in open(os.path.join(root, \"pyvenv.cfg\")) if \"=\" in ln}))
b = sorted(os.listdir(os.path.join(root, \"bin\")))
print(\"② bin/ 里列一遍（只留 basename）：\", b)
rel = sum(1 for n in b if os.path.islink(os.path.join(root, \"bin\", n)) and not os.readlink(os.path.join(root, \"bin\", n)).startswith(\"/\"))
abso = sum(1 for n in b if os.path.islink(os.path.join(root, \"bin\", n)) and os.readlink(os.path.join(root, \"bin\", n)).startswith(\"/\"))
print(\"   符号链接里指向同目录的相对链接有几条 →\", rel, \"｜ 指向系统解释器的绝对链接有几条 →\", abso, \"（★ 诚实交代：不是一条不差的「全是相对」）\")
print(\"   激活脚本有没有 →\", sorted(n for n in b if \"activate\" in n))
lib = os.path.join(root, \"lib\")
sub = os.path.join(lib, next(x for x in os.listdir(lib) if x.startswith(\"python\")))
sp = os.path.join(sub, \"site-packages\")
print(\"③ venv 自己的包目录叫什么：\", os.path.basename(sub), \"/\", os.path.basename(sp), \"｜ 里面此刻有几项 →\", len(os.listdir(sp)), \"（建完是空的：venv 不装东西，它只是多开一个目录）\")
print(\"   它有没有复制一份标准库进去 →\", any(x.startswith(\"os.py\") or x.startswith(\"json\") for x in os.listdir(sp)))
"; rm -rf venv-check
① 骨架三样齐了吗 → True ｜ 缺的： 无
   pyvenv.cfg 里只有键名（值里带绝对路径，一律不上屏）： ['command', 'executable', 'home', 'include-system-site-packages', 'version']
② bin/ 里列一遍（只留 basename）： ['Activate.ps1', 'activate', 'activate.csh', 'activate.fish', 'python', 'python3', 'python3.14', '𝜋thon']
   符号链接里指向同目录的相对链接有几条 → 3 ｜ 指向系统解释器的绝对链接有几条 → 1 （★ 诚实交代：不是一条不差的「全是相对」）
   激活脚本有没有 → ['activate', 'activate.csh', 'activate.fish']
③ venv 自己的包目录叫什么： python3.14 / site-packages ｜ 里面此刻有几项 → 0 （建完是空的：venv 不装东西，它只是多开一个目录）
   它有没有复制一份标准库进去 → False
[退出码 0]
```

四行输出逐个读。① 骨架三样齐了，缺的列表是「无」；cfg 只上屏键名，五个：`command`、`executable`、`home`、
`include-system-site-packages`、`version`（值里带绝对路径，一律不上屏）。
② `bin/` 八个名字，链接分成两堆：3 条指同目录的相对链接、1 条指系统解释器的绝对链接 —— ★这一格自己就把「全是相对」这个说法否掉了。
③ venv 自己的包目录是 `python3.14/site-packages`，此刻 0 项。最后两个布尔是主线③ 的物理证据：
建完是空的、里面也没有标准库副本 —— venv 不装东西，它只是多开一个目录，把系统那份挡在账本外面。

顺带一个读数陷阱：② 那行「激活脚本有没有」抓到三个（`activate`、`activate.csh`、`activate.fish`），
而上面整份清单里是四个激活脚本 —— 那个大写开头的 `Activate.ps1` 没被匹配上（判据是大小写敏感的子串）。
「抓到三个」不等于「只有三个」，这种筛子在排查时得知道它漏了什么。

**这一节带走：**

- 一条 `python3 -m venv --without-pip 目标` 就把骨架建全：`pyvenv.cfg` + `bin/` + `lib/`。
- `bin/` 里 3 条相对链接 + 1 条绝对链接，别把「全是符号链接」听成「全是相对链接」。
- 新 `site-packages` 是 0 项、没有标准库副本：venv 只多开一个目录，不搬任何东西。
- 这一格现建现删：同一个命令串里 `rm -rf`，屏幕上不留绝对路径，也不给后面留残留。

---

## 第 18 节 · 小证据：一条 pyvenv.cfg 开关换掉整本路径账 —— include-system-site-packages

两个 venv，同一个解释器、同一套骨架，建的时候只差一个 `--system-site-packages`。
这一格把主线③ 钉死：变的只有 `sys.path` 这本账，而改写它的就是 cfg 里那一行。

```
$ mkdir -p switch-check && cd switch-check
python3 -m venv --without-pip off && python3 -m venv --without-pip --system-site-packages on
for v in off on; do
  printf "\n—— venv %s ——\n" "$v"
  ./$v/bin/python3 -c "
import os, sys
def fold(p):
    return str(p).replace(os.environ[\"SB\"], \"<沙箱>\").replace(os.getcwd(), \"<cwd>\")
switch = [ln.split(\"=\")[1].strip() for ln in open(os.path.join(sys.prefix, \"pyvenv.cfg\")) if ln.startswith(\"include-system\")]
print(\"① 我在 venv 里吗 →\", sys.prefix != sys.base_prefix, \"｜ prefix 折成 <沙箱> =\", fold(sys.prefix), \"｜ base_prefix 折成 <沙箱> =\", fold(sys.base_prefix))
print(\"② sys.path 条数 =\", len(sys.path), \"｜ 朴素数 site-packages =\", sum(1 for p in sys.path if \"site-packages\" in p), \"｜ 真实包目录（site-packages 或 dist-packages）=\", sum(1 for p in sys.path if \"site-packages\" in p or \"dist-packages\" in p))
print(\"③ cfg 里的开关 =\", switch[0] if switch else \"（没有这一行）\")
print(\"④ venv 自己的包目录在账上第几项 →\", max([i for i, p in enumerate(sys.path) if p.endswith(\"site-packages\")] or [-1]), \"｜ 家目录那条（ENABLE_USER_SITE =\", __import__(\"site\").ENABLE_USER_SITE, \"）在账上吗 →\", any(\".local\" in p for p in sys.path))
"
done
cd ..; rm -rf switch-check

—— venv off ——
① 我在 venv 里吗 → True ｜ prefix 折成 <沙箱> = <沙箱>/work/switch-check/off ｜ base_prefix 折成 <沙箱> = /usr
② sys.path 条数 = 5 ｜ 朴素数 site-packages = 1 ｜ 真实包目录（site-packages 或 dist-packages）= 1
③ cfg 里的开关 = false
④ venv 自己的包目录在账上第几项 → 4 ｜ 家目录那条（ENABLE_USER_SITE = False ）在账上吗 → False

—— venv on ——
① 我在 venv 里吗 → True ｜ prefix 折成 <沙箱> = <沙箱>/work/switch-check/on ｜ base_prefix 折成 <沙箱> = /usr
② sys.path 条数 = 7 ｜ 朴素数 site-packages = 1 ｜ 真实包目录（site-packages 或 dist-packages）= 3
③ cfg 里的开关 = true
④ venv 自己的包目录在账上第几项 → 4 ｜ 家目录那条（ENABLE_USER_SITE = True ）在账上吗 → False
[退出码 0]
```

同一份探针跑在 `off` 和 `on` 里，四行对照：

- ① 两边「我在 venv 里吗」都是 True，`prefix` 各在自己的沙箱目录，`base_prefix` 两边都是 `/usr` —— 同一个基座，没有第二个解释器。
- ② `sys.path` 条数 5 → 7；朴素数 `site-packages` 两边都是 **1**；真实包目录（`site-packages` 或 `dist-packages`）1 → **3**。
  ★只数 `site-packages` 的话，你会以为这个开关没生效 —— 本机系统包目录叫 `dist-packages`，两种数法必须同时给。
- ③ cfg 里那一行：`false` / `true`。开关住在文件里，不在环境变量里，也不在激活脚本里。
- ④ venv 自己的包目录两边都在账上第 4 项 —— 它的位置不动，动的只是别人。

④ 后半段还留了一个耐看的组合：家目录那条「在账上吗」两边都是 False，可括号里那个 `ENABLE_USER_SITE` 在 `off` 是 False、
在 `on` 反倒成了 True。这一格只给值不给原因（本章没演它为什么跟着这个开关翻），所以只记两件事：布尔会变，账上没有。
`off` 那个 False 就是第 16 节 demo1 看到的形态 —— 隔离环境顺手把 `~/.local` 那条缝堵上；True 却没路径，原因留给第 20 节。

**这一节带走：**

- venv 的隔离感全部来自这一件事：关掉时系统那份包目录整个不在账上，打开就回来。
- 判开关别用「数 site-packages」，本机要数「site-packages 或 dist-packages」；条数 5 / 7 才是真相。
- cfg 里一行文本就是开关本体，`sys.prefix` 和骨架完全没变。
- 布尔（`ENABLE_USER_SITE`）和账上有没有路径是两件事，第 20 节专门演这一刀。

---

## 第 19 节 · i 篇完整跑：site 模块才是填账本的那个手，.pth 是它塞进来的代码

前面所有关于 `sys.path` 的结论都挂在一个人身上：解释器启动时偷偷 import 的 `site`。
它干三件事：加包目录、处理 `.pth`、决定家目录那条通道开不开。
这一节最要紧的是第 5、6 两段 —— `.pth` 不是配置文件，它以 `import` 开头的行是**被执行**的代码。

```
$ python3 i_site_and_userdir.py
=== 1. site 报的几个值：本机的包目录到底叫什么 ===
  子进程（`python3 -c 体检表`）退出码 0，stderr 为空
  sys.prefix == sys.base_prefix   True
  len(sys.path)                   6
  site.getsitepackages() 条数     3
  其中有几条真实存在              2
  site.ENABLE_USER_SITE           True
  家目录那个目录存在吗            False
  它在 sys.path 里吗              False
  sys.path 里带 'packages' 的条目（排序）：['<base>/lib/python3/dist-packages', '<base>/local/lib/python3.14/dist-packages']
  getsitepackages() 给的路径（排序，折叠）：['<base>/lib/python3.14/dist-packages', '<base>/lib/python3/dist-packages', '<base>/local/lib/python3.14/dist-packages']
  user site 的形状（家目录折成 <HOME>，只打印形状）：<HOME>/.local/lib/python3.14/site-packages
  → 判据 ①：本机所有包目录都叫 **dist-packages**，一条 site-packages 都没有 —— Debian/Ubuntu 打的补丁。别处抄来的「去 site-packages 里找」的命令在这台机器上会扑空。
  → 判据 ②：非 venv 进程里 sys.prefix 和 sys.base_prefix 相等（这一行就是「我没在 venv 里」的判据，09 章 h 节里两者不等）。
  ★ 代价：getsitepackages() 报 3 条、实际存在 2 条 —— API 的返回值不等于磁盘现实，site 自己也是先 `isdir` 再决定的，所以排查时要以 sys.path 为准，不是以 site 的 API 为准。
```

先认清本机的名字：账上带 `packages` 的两条全是 `dist-packages`，一条 `site-packages` 都没有 ——
别处抄来的「去 site-packages 里 ls」在这台机器上必然扑空。
`getsitepackages()` 报 3 条、磁盘上真存在 2 条：★API 的返回值不等于磁盘现实，site 自己也是先 `isdir` 再决定的，
所以排查要以 `sys.path` 为准。user site 的形状是家目录下 `.local/lib/python3.14/site-packages`（这一处反倒叫 `site-packages`），
目录不存在、也不在账上。

```
=== 2. user site：开关是 True，路径却没进 sys.path（源码那行现场抠出来）===
  证据来自解释器自带的 <base>/lib/python3.14/site.py：
       375  def addusersitepackages(known_paths):
       386  if ENABLE_USER_SITE and os.path.isdir(user_site):
  实测对照：默认 sys.path 条数 6 → 加 `-s` 之后 6（ENABLE_USER_SITE 变成 False）
  → 判据：这台机器上两个数字**一样**，因为 <HOME>/.local/lib/python3.14/site-packages 这个目录压根不存在（USER_SITE_EXISTS=False），`-s` 自然无事可关。
  ★ 这条不是「本机特殊」，是排查价值：谁哪天执行过一次 `pip install --user`，那个目录被创建出来，你的包就会**绕开 venv** 出现在所有环境里 —— h 节实测过 venv 里 ENABLE_USER_SITE 被硬性关掉（False），就是为了堵这条缝。
  未验证：`pip install --user` 之后的真实效果（本机没有 pip，绝不安装，也不碰家目录）。
```

为什么开关 True 而路径不在账上，源码那行现场抠出来了：`ENABLE_USER_SITE and os.path.isdir(user_site)` 两个条件是「与」，
目录不存在就什么都不加。于是本机默认 6 条、加 `-s` 之后还是 6 条 —— 两个数字一样，`-s` 无事可关。
这条不是本机特殊，是反面价值：谁哪天装过一次用户级包，那个目录被创建出来，你的包就会**绕开 venv** 出现在所有环境里
（第 16 节 demo1 里 `ENABLE_USER_SITE=False` 就是堵这条缝的）。用户级安装的真实效果本机没 pip，演不了。

```
=== 3. python -S：跳过 site 之后少掉的是什么（连一个第三方 finder 都掉了）===
  len(sys.path)：默认 6 → -S 4
  默认有、-S 没有的包目录条目（排序）：['<base>/lib/python3/dist-packages', '<base>/local/lib/python3.14/dist-packages']
  （上面那一行是把两边的 PKG_DIRS 当集合做差；两边都是同一套打码规则下的相对/<base> 形态，所以差集只反映「site 加了哪几条」。）
  -S 之后还剩这些包目录：[]
  sys.meta_path：默认 ['DistutilsMetaFinder', 'BuiltinImporter', 'FrozenImporter', 'PathFinder']
               -S   ['BuiltinImporter', 'FrozenImporter', 'PathFinder']
  -S 之后 site.ENABLE_USER_SITE = None（None：算这个值的代码就是 site.main() 自己，跳过它它就没被算过）
  → 判据：`-S` 砍掉的正是 site 加的那几条包目录；标准库那条链一条不少 —— 再次印证「venv/site 只管第三方包目录」。
  ★ 注意 meta_path 第一项：默认进程里有个 DistutilsMetaFinder，它不属于 importlib，是系统里这个 .pth 带来的 → ['distutils-precedence.pth']。-S 一开它就没影了：启动时执行的一个 .pth 文件改写了导入机制本身。
```

`-S` 一刀砍掉的正中 site 加的这两条包目录：6 → 4，剩下 4 条全是标准库，包目录清空。
最扎的是 `sys.meta_path`：默认四条里有个 `DistutilsMetaFinder`，`-S` 之后只剩三条，它消失了 ——
它不属于 importlib，是系统里 `distutils-precedence.pth` 带来的。启动时执行的一个 `.pth` 文件改写了导入机制本身。

```
=== 4. 建一个 venv，往它的 site-packages 放 .pth：加路径这一半机制 ===
  `python3 -m venv --without-pip site-demo/venv` → 退出码 0（本机只能这么建，原因见 h 节）｜ site-packages 在 site-demo/venv/lib/python3.14/site-packages
  venv 里现在能找到 pthmod 吗 → False ｜ sys.path 里有 pth-target 吗 → False
  写 site-demo/venv/lib/python3.14/site-packages/demo.pth，两行内容（都相对 .pth 自己所在目录）：
    ../../../../pth-target      ← 指向沙箱里真实存在的 site-demo/pth-target
    ../../../../no-such-dir   ← 指向根本不存在的 site-demo/no-such-dir
  再问一次：pthmod 找到了吗 → True ｜ 它的 __file__（相对当前目录） → site-demo/pth-target/pthmod.py
  sys.path 条数 5 → 6；pth-target 进 sys.path 了吗 → True；带 packages 的条目 → ['site-demo/venv/lib/python3.14/site-packages']
  那个不存在的目录被加进 sys.path 了吗 → False   ★site 对 .pth 的每一行先做 `os.path.exists` 才加，不存在就**静默丢掉**，一句都不报。
  → 判据：`.pth` 里的普通行 = 相对 **本文件所在目录**（不是你的 cwd！）解析出来的路径，逐条 append 到 sys.path。这就是 `-e` 开发式安装（egg-link / __editable__）背后的全部魔法。
```

加路径这一半机制：往 venv 的 `site-packages` 放一个两行的 `demo.pth`，两行都写成相对**本文件所在目录**（不是你的 cwd），
一行指真目录、一行指根本不存在的目录。放完 `pthmod` 就找到了，`sys.path` 5 → 6；
那个不存在的目录 False —— ★site 对每一行先做 `exists` 再加，不存在就**静默丢掉**，一句都不报。
开发式安装背后的全部魔法就这么点。

```
=== 5. ★ 安全面实测：.pth 里 import 开头的行是被 exec 的，而且本机一次启动跑两遍 ===
  源码就在 <base>/lib/python3.14/site.py 里，两条分支泾渭分明：
       218  if line.startswith(("import ", "import\t")):
       219  exec(line)
  往 venv 的 site-packages 放 execdemo.pth，唯一一行是：
    import os; open(os.path.join(os.getcwd(), 'site-demo/exec-count.txt'), 'a').write('x')
  它相对 cwd 往 site-demo/exec-count.txt 追加一个字符 —— 无害，但性质是「任意代码」。
  只起一个进程、一句 `print` 都没碰到 pthmod：计数器字符数 = 2（跑了几遍就是几）｜ 子进程退出码 0，stdout 只有它自己那行
  ★ 两遍，不是一遍。原因在 site.py 里（同一份源码，行号现场抠出来）：
         659  addsitepackages(known_paths, [sys.prefix])
         728  known_paths = addsitepackages(known_paths)
    第一行在 site.venv() 内部：认出 pyvenv.cfg 之后马上扫一遍 venv 自己的 site-packages；
    第二行在 site.main() 里：随后 addsitepackages() 按 getsitepackages() 再扫一遍，
    而虚拟环境里 getsitepackages() 的第一项**还是**那个目录 → 同一个目录扫两遍、
    同一份 .pth 读两遍、里面的代码被执行两遍。
    ★ 这条「几遍」的计数依赖本机这份 site.py 的实现，换 Python 版本/发行版未验证 —— 但「.pth 里的代码会在每次启动时被执行」这件事本身与版本无关。
  → 判据：.pth 里的 import 行**必须幂等**（可重复执行、无副作用累积）；装一个包 = 授权它在**每次** python 启动时执行代码，这不是比喻，是第 5 段这个计数器。
  对照组：把同样的 .pth 放进一个 **PYTHONPATH** 目录 → 计数器字符数增量 = 0（0 = 一行都没执行）；pthmod 找到吗 → False
  → 判据：.pth 只在「被 site 当成包目录扫描」的目录里生效（sys.path 里那些包目录、以及 .pth 自己所在的目录）；PYTHONPATH 里的目录只是普通路径项，.pth 当普通文件被忽略。
  顺带给出第 ④ 步的现成查法（扫 sys.path 每个目录里的 *.pth，只报 basename）：把本段建的 demo .pth 删掉之后 → []；第 3 段基座里抓到的是 ['distutils-precedence.pth']。
```

安全面这一段，围栏里给了源码两行：以 `import ` 或 `import` + 制表符开头的行，走 `exec(line)`。
实验放的 `execdemo.pth` 只有唯一一行，内容是一个 `import` 开头、往一个文件追加一个字符的表达式 —— 无害，但性质是「任意代码」。
屏幕上：一个进程、一句 `print` 都没碰它，计数器字符数 = **2**。两遍，不是一遍。
原因也是从同一份源码现场抠出来的两个调用点：`site.venv()` 认出 cfg 之后先扫一遍 venv 自己的 `site-packages`，
随后 `site.main()` 按 `getsitepackages()` 再扫一遍，而虚拟环境里 `getsitepackages()` 的第一项**还是**那个目录 →
同一个目录扫两遍、同一份 `.pth` 读两遍、里面的代码执行两遍。
★这条「几遍」的计数依赖本机这份 `site.py`，换版本换发行版未验证；但「`.pth` 里的代码每次启动都被执行」这件事与版本无关。
对照组把同样的内容放进 `PYTHONPATH` 目录：增量 0，模块也找不到 —— `.pth` 只在被 site 当包目录扫描的地方生效。

**这就是为什么包目录里的 `.pth` 是攻击面**：装一个包 = 授权它在**每次** `python` 启动时执行代码，
这不是比喻，是那个计数器；而它落在你的项目之外（在解释器的目录里），你 `ls` 项目根本看不见它。
反过来，写 `.pth` 的人必须让那行 `import` 幂等 —— 它在一次启动里就不止跑一遍。

```
=== 6. 一条坏 .pth 的现场：报错很响，退出码是 0，后面的行全部作废 ===
  broken.pth 三行：../../../../bad1（存在）／ import no_such_module_from_pth（不存在）／ ../../../../bad2（存在）
  子进程退出码 → 0   ★启动成功。坏 .pth 不会拦住任何人，它只在 stderr 上喊一嗓子。
    Error processing line 2 of <cwd>/site-demo/venv/lib/python3.14/site-packages/broken.pth:
    
      Traceback (most recent call last):
        File "<frozen site>", line 219, in addpackage
        File "<string>", line 1, in <module>
      ModuleNotFoundError: No module named 'no_such_module_from_pth'
    
    Remainder of file ignored
    Error processing line 2 of <cwd>/site-demo/venv/lib/python3.14/site-packages/broken.pth:
    
      Traceback (most recent call last):
        File "<frozen site>", line 219, in addpackage
        File "<string>", line 1, in <module>
      ModuleNotFoundError: No module named 'no_such_module_from_pth'
    
    Remainder of file ignored
  实际加进 sys.path 的情况：坏行之前的 bad1 进来了吗 → True ｜ 坏行之后的 bad2 进来了吗 → False
  ★ 数一下上面那坨出现了几遍：2 遍 ——不是终端抽风，是第 5 段那个「一次启动扫两遍」的同一个原因，报错也翻倍。
  → 判据：site 对每一行 try/except，异常时打印上面那坨然后 **break** —— 「Remainder of file ignored」是字面意思，坏行**后面**的路径一条都不会加。
  ★ 代价：这类损坏极难发现，因为退出码是 0、报错在 stderr、而且每次启动都重复打；CI 里 `set -o pipefail` 或把 stderr 当日志翻的人才会撞见它。排查时 `python -X importtime` 不管用，要 `-v` 或直接看包目录里的 *.pth。
```

一条坏 `.pth`（三行：存在的路径 / `import` 一个不存在的模块 / 存在的路径）的现场：子进程退出码 **0**。
★直觉以为启动会被它拦住，屏幕上它是成功的：坏 `.pth` 不拦任何人，只在 stderr 上喊一嗓子。
然后数一喊几嗓子 —— 那坨 traceback 在围栏里出现**两遍**，不是终端抽风，就是上一段那个「一次启动扫两遍」。
最后一行那两个布尔把后果钉死了：坏行之前的 bad1 进了账（True），坏行之后的 bad2 没进（False）。
site 对每一行 try/except，异常时打完那坨就 **break** —— 围栏里原文末尾那句「忽略余下内容」是字面意思：
坏行**后面**的路径一条都不会加。
这类损坏极难发现：退出码 0、报错在 stderr、每次启动都重复打，CI 里只有开了 `pipefail` 或专门翻 stderr 的人才撞得到。
排查时 `-X importtime` 不管用，要么 `-v`，要么直接去包目录里看 `*.pth`。

收口那五步不另贴证据，全指向前面六格：① 先问「我在哪个解释器上」（`sys.executable`、`sys.prefix == sys.base_prefix`）；
② 再摊开 `sys.path` 看有什么（脚本所在目录 ≠ 当前目录）；③ 再看包目录叫什么名字；
④ 然后扫包目录里的 `*.pth`（它可能在你不知情时加过路径，也可能一行坏代码把后面的全吞了）；
⑤ 最后才怀疑用户级安装 / 装错环境。别拿「往 `sys.path` 里 append 绝对路径」治本，
那只把「环境不对」变成「只有你这台机器能跑」；正解是让解释器自己找到对的目录。

**这一节带走：**

- 填 `sys.path` 的手是 `site`：加包目录、处理 `.pth`、决定家目录通道开不开；`-S` 一次砍掉它加的那两条。
- 本机包目录叫 `dist-packages`；`getsitepackages()` 报 3 条而磁盘只有 2 条，排查以 `sys.path` 为准。
- `.pth` 的普通行 = 相对**它自己所在目录**的路径，不存在的静默丢掉；`import` 开头的行 = 被 `exec` 的代码。
- venv 里一次启动把同一份 `.pth` 跑了两遍（本机 3.14.4 实测），所以那行代码必须幂等。
- 坏 `.pth`：退出码 0、traceback 打两遍、坏行之后的路径全部作废 —— 这是本章最难查、也最值钱的一格。
- 一个 `.pth` 能往 `sys.meta_path` 里塞 finder：启动时的配置文件改动了导入机制本身。

---

## 第 20 节 · 小证据：-s / -S / -E / PYTHONNOUSERSITE= 空值，四种「关」法关的不是同一件事

四个都叫「关」，关的是三层不同的东西：`-s` 只关家目录那条通道，`-S` 把整个 `site` 关掉（包目录和 `.pth` 一起没了），
`-E` 关的是「读环境变量」这一层，`PYTHONNOUSERSITE` 只是 `-s` 的环境变量写法 —— 而且**空值不算关**。

```
$ python3 -c "
import os, site, sys, sysconfig
h = os.path.expanduser(\"~\")
def fold(p):
    return str(p).replace(h, \"<家目录>\")
venv_p = sysconfig.get_paths(scheme=\"venv\", vars={\"base\": sys.base_prefix})
user_p = sysconfig.get_paths(scheme=\"posix_user\")
cand = [fold(x) for x in dict.fromkeys(list(venv_p.values()) + list(user_p.values()))
        if x.endswith(\"site-packages\") or x.endswith(\"dist-packages\")]
onpath = set(fold(x) for x in sys.path)
print(\"① 「如果是 venv / 如果是 pip install --user」两种布局下的包目录候选（家目录折成 <家目录>）：\", cand)
print(\"   有几条落在家目录 →\", sum(1 for x in cand if \"<家目录>\" in x),
      \"｜ 有几条此刻就在 sys.path 上 →\", sum(1 for x in cand if x in onpath))
print(\"   本机 default scheme 真正在用的那条 →\", fold(sysconfig.get_paths()[\"purelib\"]))
print(\"   → 上面三条互不相同：venv 那条只在你建了 venv 之后才生效，user 那条目录压根不存在，\")
print(\"     平时 import 到的其实是 default 那条 —— 这就是「同一个 import 语句在不同环境下指向不同目录」的字面形状\")
print(\"② 此刻 sys.path 上：朴素数 site-packages =\", sum(1 for p in sys.path if \"site-packages\" in p),
      \"｜ 真实包目录（含 dist-packages）=\", sum(1 for p in sys.path if \"site-packages\" in p or \"dist-packages\" in p))
print(\"   → 两个数不一样就是本机那处改名（Debian 把包目录叫成 dist-packages），h/i 两篇的诚实交代都在这\")
print(\"③ 家目录那条 user site 在 sys.path 里吗 →\", site.getusersitepackages() in sys.path,
      \"｜ ENABLE_USER_SITE =\", site.ENABLE_USER_SITE,
      \"｜ 那个目录本身存在吗 →\", os.path.isdir(site.getusersitepackages()))
print(\"   → 开关是开着的但目录不存在，site 只往存在的目录里加：这就是「ENABLE_USER_SITE=True 而路径不在账上」的原因\")
"
① 「如果是 venv / 如果是 pip install --user」两种布局下的包目录候选（家目录折成 <家目录>）： ['/usr/lib/python3.14/site-packages', '<家目录>/.local/lib/python3.14/site-packages']
   有几条落在家目录 → 1 ｜ 有几条此刻就在 sys.path 上 → 0
   本机 default scheme 真正在用的那条 → /usr/local/lib/python3.14/dist-packages
   → 上面三条互不相同：venv 那条只在你建了 venv 之后才生效，user 那条目录压根不存在，
     平时 import 到的其实是 default 那条 —— 这就是「同一个 import 语句在不同环境下指向不同目录」的字面形状
② 此刻 sys.path 上：朴素数 site-packages = 0 ｜ 真实包目录（含 dist-packages）= 2
   → 两个数不一样就是本机那处改名（Debian 把包目录叫成 dist-packages），h/i 两篇的诚实交代都在这
③ 家目录那条 user site 在 sys.path 里吗 → False ｜ ENABLE_USER_SITE = True ｜ 那个目录本身存在吗 → False
   → 开关是开着的但目录不存在，site 只往存在的目录里加：这就是「ENABLE_USER_SITE=True 而路径不在账上」的原因
[退出码 0]
```

① 那一段是「同一个 `import` 在不同环境下指向不同目录」的字面形状：`sysconfig` 按 venv 布局算出来的那条
只在你真建了 venv 之后才生效；按用户级布局算出来的那条落在家目录（候选里 1 条），而此刻 0 条落在 `sys.path` 上；
本机 default 真正在用的是第三条（一个 `dist-packages`）。三条互不相同，谁也不代表谁。
② 两个数 0 和 2，又是那处改名。
③ 最要紧的布尔组合：家目录那条不在账上、`ENABLE_USER_SITE` 却是 True、目录本身 False ——
开关是开着的，但 site 只往**存在的**目录里加。「ENABLE_USER_SITE=True 而路径不在账上」的原因就在这。

```
$ python3 -c "import sys, site; print(\"对照（四个开关都没碰）→ ENABLE_USER_SITE =\", site.ENABLE_USER_SITE, \"｜ sys.path 条数 =\", len(sys.path))"
对照（四个开关都没碰）→ ENABLE_USER_SITE = True ｜ sys.path 条数 = 6
[退出码 0]

$ python3 -s -c "import sys, site; print(\"-s   → ENABLE_USER_SITE =\", site.ENABLE_USER_SITE, \"｜ sys.path 条数 =\", len(sys.path), \"｜ 只关家目录那条通道，其它照旧\")"
-s   → ENABLE_USER_SITE = False ｜ sys.path 条数 = 6 ｜ 只关家目录那条通道，其它照旧
[退出码 0]

$ python3 -S -c "import sys; print(\"-S   → 整个 site 都不跑：sys.path 条数 =\", len(sys.path), \"｜ 比上面少掉的几条就是 site 加的\")"
-S   → 整个 site 都不跑：sys.path 条数 = 4 ｜ 比上面少掉的几条就是 site 加的
[退出码 0]

$ python3 -E -c "import sys; print(\"-E   → PYTHONPATH/PYTHONHOME 这类环境变量全被忽略：条数 =\", len(sys.path), \"｜ 它关的是「读环境」，不是「读 site」\")"
-E   → PYTHONPATH/PYTHONHOME 这类环境变量全被忽略：条数 = 6 ｜ 它关的是「读环境」，不是「读 site」
[退出码 0]
```

四行对照：对照 True / 6 条；`-s` False / 6 条（条数一点没变 —— 目录本来不存在，它只关家目录那条通道，其它照旧）；
`-S` 4 条（比上面少的那两条正是 site 加的）；`-E` 6 条。
`-E` 这个 6 和对照那格的 6 相同，不是 `-E` 没用：这一格本来就没有 `PYTHONPATH` 之类可被忽略
（第 1 节体检那格四个变量全是「未设」），它关的是「读环境」，不是「读 site」。
再回看 `-S`：那一刀砍掉的不止路径，第 19 节 `sys.meta_path` 上那个第三方 finder 也跟着消失。

```
$ PYTHONNOUSERSITE= python3 -c "import sys; print(\"PYTHONNOUSERSITE= 空值 → flags.no_user_site =\", sys.flags.no_user_site, \"｜ 空值＝不关：环境变量只看是不是空，不是只看存不存在\")"
PYTHONNOUSERSITE= 空值 → flags.no_user_site = 0 ｜ 空值＝不关：环境变量只看是不是空，不是只看存不存在
[退出码 0]

$ PYTHONNOUSERSITE=1 python3 -c "import sys; print(\"PYTHONNOUSERSITE=1      → flags.no_user_site =\", sys.flags.no_user_site, \"｜ 这才叫关\")"
PYTHONNOUSERSITE=1      → flags.no_user_site = 1 ｜ 这才叫关
[退出码 0]
```

最后两行和直觉正相反：直觉以为「变量存在就关掉」，屏幕上 `PYTHONNOUSERSITE=` 空值的
`flags.no_user_site` 是 **0**（不关），给了 1 才是 **1**（这才叫关）。
所以写 CI 或 Dockerfile 想堵家目录那条通道，要么给 1，要么直接用 `-s`，别只 `export` 一个空值 ——
后者会给你一个「以为关了、其实没关」的环境。

**这一节带走：**

- `-s` 关家目录那条通道；`-S` 关整个 site（路径 + `.pth` + 被 `.pth` 塞进来的 finder）；`-E` 关环境变量这一层。三者互不替代。
- 环境变量看的是**空不空**，不是**有没有**：`PYTHONNOUSERSITE=` 空值等于没设。
- 条数这类「每跑必对照」的数字要成对读：`-s` 前后都是 6，恰恰证明本机那条通道本来就是空的。
- `sysconfig` 按不同 scheme 算出来的包目录只是候选，落不落在账上得当场问 `sys.path`。

---

## 第 21 节 · 跨脚本小结：三本账的合并视图（sys.modules / sys.path / 缓存）

把 a~i 九篇合成一张账：`import x` 要走三步 —— 谁在（账本① `sys.modules`）、去哪儿找（账本② `sys.path`）、
用哪份编译好的（账本③ 缓存）。三格屏幕上都给计数和布尔，路径里每跑必变的部分一律折成标记。

```
$ python3 -c "
import sys
print(\"【账本① sys.modules】此刻已有\", len(sys.modules), \"条（这个数字随解释器小版本变，README 只引用下面那几条布尔）\")
import json
print(\"  规矩：命中就什么都不做 →\", all(n in sys.modules for n in (\"json\", \"os\", \"sys\")), \"｜ 账本里的对象就是你 import 到的那个 →\", sys.modules[\"json\"] is json)
print(\"  名字相同就是同一个模块，和它在磁盘上叫什么路径无关 →\", \"os\" in sys.modules and sys.modules[\"os\"] is __import__(\"os\"))
print(\"  副作用：同一个文件用两个名字加载 = 两份互不相干的活状态（g 篇第 5 段用计数演过）\")
"
【账本① sys.modules】此刻已有 39 条（这个数字随解释器小版本变，README 只引用下面那几条布尔）
  规矩：命中就什么都不做 → True ｜ 账本里的对象就是你 import 到的那个 → True
  名字相同就是同一个模块，和它在磁盘上叫什么路径无关 → True
  副作用：同一个文件用两个名字加载 = 两份互不相干的活状态（g 篇第 5 段用计数演过）
[退出码 0]
```

账本① 的条数（那格第一个数字）随解释器小版本和入口变，README 不引用它；引用的是三条布尔：
命中就什么都不做、账本里的对象就是你 `import` 到的那个（`sys.modules["json"] is json`）、
名字相同就是同一个模块 —— 和它在磁盘上叫什么路径无关。副作用一句：同一个文件用两个名字加载 = 两份互不相干的活状态。

```
$ python3 -c "
import os, sys
h = os.path.expanduser(\"~\")
print(\"【账本② sys.path】此刻\", len(sys.path), \"条（-c 入口，所以第 0 项是空串）\")
print(\"  规矩：顺序即优先级 → 谁在前面谁赢（本 demo 第 6 节用同名双份文件当场换过胜负）\")
print(\"  入口项折成 <家目录> 之后，账上有含家目录的条数 →\", sum(1 for p in sys.path if h in p), \"｜ 空串在不在 →\", \"\" in sys.path)
print(\"  内置/冻结那层根本不在这本账里，却排在所有路径之前 → 同名文件遮蔽不了它（b 篇第 6 段）\")
"
【账本② sys.path】此刻 6 条（-c 入口，所以第 0 项是空串）
  规矩：顺序即优先级 → 谁在前面谁赢（本 demo 第 6 节用同名双份文件当场换过胜负）
  入口项折成 <家目录> 之后，账上有含家目录的条数 → 0 ｜ 空串在不在 → True
  内置/冻结那层根本不在这本账里，却排在所有路径之前 → 同名文件遮蔽不了它（b 篇第 6 段）
[退出码 0]
```

账本②：这一格是 `-c` 入口，所以第 0 项是空串；「含家目录的条数」给的是 0，这是隐私检查的形状而不是结论
（家目录前缀已经先折成标记）。规矩两条：顺序即优先级，谁在前面谁赢；
★内置和冻结那两层根本不在这本账里，却排在所有路径之前 —— 所以同名文件遮蔽不了它们。

```
$ python3 -c "
import sys
print(\"【账本③ 编译缓存】cache_tag =\", sys.implementation.cache_tag, \"｜ 它是 .pyc 文件名的来源\")
print(\"  此刻 sys.dont_write_bytecode =\", sys.dont_write_bytecode, \"｜ 它只管写不管读（e 篇第 4 段四种「别写」各演过一遍）\")
print(\"  sys.pycache_prefix 是 None 吗 →\", sys.pycache_prefix is None, \"｜ 非 None 时缓存整体搬到别处，搬的是绝对路径的镜像\")
print(\"  失效判据：源码 mtime + 源码字节数（本 demo 第 12 节当场从 16 字节头部读出来对过）\")
print(\"  ★ 只有 import 才产 __pycache__；python3 路径/x.py 直接跑脚本不产 —— 所以本 demo 第 22 节的计数才有意义\")
"
【账本③ 编译缓存】cache_tag = cpython-314 ｜ 它是 .pyc 文件名的来源
  此刻 sys.dont_write_bytecode = False ｜ 它只管写不管读（e 篇第 4 段四种「别写」各演过一遍）
  sys.pycache_prefix 是 None 吗 → True ｜ 非 None 时缓存整体搬到别处，搬的是绝对路径的镜像
  失效判据：源码 mtime + 源码字节数（本 demo 第 12 节当场从 16 字节头部读出来对过）
  ★ 只有 import 才产 __pycache__；python3 路径/x.py 直接跑脚本不产 —— 所以本 demo 第 22 节的计数才有意义
[退出码 0]
```

账本③：`cache_tag` 就是 `.pyc` 文件名中间那一段；`sys.dont_write_bytecode` 此刻 False，而且它**只管写不管读**；
`sys.pycache_prefix` 是 None，非 None 时缓存整体搬到别处、搬的是绝对路径的镜像；失效判据是「源码 mtime + 源码字节数」，
不是内容哈希。最后一句是本章的机关：**只有 `import` 才产 `__pycache__`，直接跑脚本不产** —— 下一节的计数才有意义。

```
$ python3 -c "
import sys
print(\"【三本账的先后】import x 走这条路，一步都不跳：\")
print(\"  第 1 步 问账本①：x 在 sys.modules 里吗 →\", \"json\" in sys.modules, \"｜ 在 → 直接把那个对象绑给你，文件不打开、模块体不跑\")
print(\"  第 2 步 问账本②：按 sys.path 从前往后找 spec（find_spec 只查不载，g 篇）\")
print(\"  第 3 步 问账本③：.pyc 里那两个数对得上源码吗 → 对得上就跳过 parse/compile，但模块体照样跑（e 篇收口）\")
print(\"  第 4 步 执行模块体 → 绑名字进你的命名空间 → 顺手在父包对象上挂属性（a/c 篇）\")
print(\"  任何一步想不通，报错原文里都不会告诉你它找过哪些目录 → 只能自己摊开 sys.path 逐层问（b 篇第 7 段那张表）\")
"
【三本账的先后】import x 走这条路，一步都不跳：
  第 1 步 问账本①：x 在 sys.modules 里吗 → False ｜ 在 → 直接把那个对象绑给你，文件不打开、模块体不跑
  第 2 步 问账本②：按 sys.path 从前往后找 spec（find_spec 只查不载，g 篇）
  第 3 步 问账本③：.pyc 里那两个数对得上源码吗 → 对得上就跳过 parse/compile，但模块体照样跑（e 篇收口）
  第 4 步 执行模块体 → 绑名字进你的命名空间 → 顺手在父包对象上挂属性（a/c 篇）
  任何一步想不通，报错原文里都不会告诉你它找过哪些目录 → 只能自己摊开 sys.path 逐层问（b 篇第 7 段那张表）
[退出码 0]
```

先后四步一步都不跳：问账本① → 按 `sys.path` 从前往后找 spec（`find_spec` 只查不载）→
核对 `.pyc` 头部那两个数 → 执行模块体、把名字绑进你的命名空间、顺手在父包对象上挂属性。
最下面那句才是排查的实底：任何一步想不通，**报错原文里都不会告诉你它找过哪些目录**，只能自己摊开 `sys.path` 逐层问。

还有一处值得盯着看：第 1 步那句问的是 `json` 在不在账本① 里，屏幕上给的是 False，
而上面账本① 那一格里同一个表达式是 True。原因就在两个围栏里：上面那段 `import` 过 `json`，这段只有 `import sys`，
而两次 `-c` 是两个进程 —— 账本① 每个进程从零长起。这正是「模块体只跑一次」的另一面：只跑一次，是在**这一个进程里**。

**这一节带走：**

- 三本账按顺序查：命中 → 路径 → 缓存；报错不会替你摊开路径，得自己印。
- 账本① 认名字不认路径；账本② 认顺序不认「哪个更合理」；账本③ 认 mtime + 字节数，不认内容哈希。
- 内置/冻结层不在 `sys.path` 里，却先于它 —— 遮蔽那一路的天花板。
- 「只有 import 才写缓存」这句话同时解释了第 22 节为什么能拿文件计数来自证。

---

## 第 22 节 · 自证：沙箱里的事只留在沙箱，章目录一个字都没多

这一节是本章的取证纪律本身，规矩只有一句：**先把「该有什么」写成能判的式子，再故意让它响一次，最后才报 0**。
一条恒为 0 的检查比没有检查更糟 —— 它会让人以为查过。

```
$ echo "沙箱 work/ 里的 .py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)（这 9 个是章目录副本，不是产物）  非 .py 文件数=$(find . -maxdepth 1 -type f ! -name "*.py" | wc -l)"
沙箱 work/ 里的 .py 个数=9（这 9 个是章目录副本，不是产物）  非 .py 文件数=0
[退出码 0]
```

此刻的当前目录就是沙箱 `work/`。第一格给的是「该有什么」：`.py` 9 个 —— 那是章目录的副本，不是产物；非 `.py` 文件 0 个。

```
$ mkdir -p 残留-demo; touch 残留.tmp; printf "y = 1\n" > probe_mod.py; python3 -c "import probe_mod"; echo "塞进假产物之后：子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  非 .py 文件数=$(find . -maxdepth 1 -type f ! -name "*.py" | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l)  ← 四项都会响"
塞进假产物之后：子目录数=2  非 .py 文件数=1  __pycache__ 个数=1  *.pyc 个数=1  ← 四项都会响
[退出码 0]

$ rm -rf 残留-demo 残留.tmp probe_mod.py __pycache__; echo "清掉之后：子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  非 .py 文件数=$(find . -maxdepth 1 -type f ! -name "*.py" | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)  *.pyc 个数=$(find . -name "*.pyc" | wc -l)"
清掉之后：子目录数=0  非 .py 文件数=0  __pycache__ 个数=0  *.pyc 个数=0
[退出码 0]
```

接着现场造三样残留：一个子目录、一个非 `.py` 文件、还有一次真 `import` 产生的缓存。
屏幕上四个数全跳起来了：子目录 2、非 `.py` 文件 1、`__pycache__` 1、`.pyc` 1 —— ★四项都会响。
下一行同一个式子清掉之后全归 0。先响再归零，这两个 0 才是有判断力的结论；
直接贴一个 0，你无法区分「真干净」和「这条 `find` 写错了」。
那一次「响」本身还是本章主题的现场复演：造出 `__pycache__` 的是那句 `import`，
而九个取证脚本跑的是 `python3 名字.py`，它们自己 import 出去的东西全在各自的 demo 目录里，收尾整份删。

```
$ echo "沙箱里此刻还有 venv 骨架吗 → pyvenv.cfg 个数=$(find . -name pyvenv.cfg | wc -l)  bin/activate 个数=$(find . -name activate | wc -l)（第 17、18 节现建的那两个已删）"
沙箱里此刻还有 venv 骨架吗 → pyvenv.cfg 个数=0  bin/activate 个数=0（第 17、18 节现建的那两个已删）
[退出码 0]
```

venv 那两样也数一遍：`pyvenv.cfg` 0、`bin/activate` 0 —— 第 17、18 节现建的那两个已经删了。

```
$ python3 -c "
import os
def n(p):
    p = os.path.expanduser(p)
    return len(os.listdir(p)) if os.path.isdir(p) else \"目录不存在\"
print(\"~/.local/lib 条目数 =\", n(\"~/.local/lib\"), \"｜ ~/.config 条目数 =\", n(\"~/.config\"), \"｜ ~/.bashrc 存在吗 →\", os.path.isfile(os.path.expanduser(\"~/.bashrc\")))
"
~/.local/lib 条目数 = 目录不存在 ｜ ~/.config 条目数 = 31 ｜ ~/.bashrc 存在吗 → True
[退出码 0]
```

家目录那两处是「demo 没往家目录写过东西」的凭证：同一格在第 1 节也数过一次，两处屏幕上这三个值逐字相同 ——
家目录下那条 lib 干脆连目录都没有、`.config` 是一个固定条目数、`.bashrc` 在。
条目数是这台机器自己的属性、不是本章造成的，所以「两处相同」才是结论，那个数字本身不是。

```
$ 〔在 09-modules-and-venv 里〕 ls
a_import_basics.py
b_sys_path_shape.py
c_packages_and_init.py
d_circular_import.py
demo.sh
e_pyc_and_cache.py
f_main_guard_and_dunder.py
g_importlib_and_spec.py
h_venv_mechanics.py
i_site_and_userdir.py
[退出码 0]

$ 〔在 09-modules-and-venv 里〕 echo ".py 个数=$(find . -maxdepth 1 -name "*.py" | wc -l)  demo.sh 个数=$(find . -maxdepth 1 -name "demo.sh" | wc -l)  README 以外的其它文件数=$(find . -maxdepth 1 -type f ! -name "*.py" ! -name "demo.sh" ! -name "README.md" | wc -l)  子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  __pycache__ 个数=$(find . -name __pycache__ | wc -l)"
.py 个数=9  demo.sh 个数=1  README 以外的其它文件数=0  子目录数=0  __pycache__ 个数=0
[退出码 0]
```

然后是章目录本体（demo 全程只读它）：10 个文件 —— 9 个取证脚本加 `demo.sh`，
`.py` 9、`demo.sh` 1、README 以外的其它文件 0、子目录 0、`__pycache__` 0。
清单显式排除 `README.md`，所以「README 写完之后重跑」得到的还是同一份输出 —— 写书这件事不会污染证据。

```
$ if command -v ss >/dev/null 2>&1; then echo "本机 LISTEN 条目数=$(ss -lnt | grep -c LISTEN)"; else echo "ss 本机没有（那就更没得起）"; fi
本机 LISTEN 条目数=7
[退出码 0]

$ v=$(pgrep -c -f "[v]env --without"); q=$(pgrep -c -f "[p]ip install"); echo "还在跑的 venv 建环境进程=$v ｜ 还在跑的 pip 安装进程=$q（本章只可能起这两类，全前台同步，跑到这一格时必然都归零）"
还在跑的 venv 建环境进程=0 ｜ 还在跑的 pip 安装进程=0（本章只可能起这两类，全前台同步，跑到这一格时必然都归零）
```

最后两条是「没起后台、没开端口」。监听只打条数（7 条全是这台机器上别的东西，端口号和本章无关，不该进教材）；
进程只数本章真可能留下的那两类名字，两个计数都是 0 —— 全程前台同步，跑到这一格时必然归零。

两条交代必须写清楚。**为什么不数 `jobs`**：demo 一个 `&` 都没起过，而跑它的执行器是用 `bash -c` 一条一条起
一次性子进程的 —— 在那种进程里问作业表，量到的永远是空表，那个 0 是假的，它连「查过」都算不上。
**`pgrep` 的模式为什么各带一个方括号**：`pgrep -f` 匹配完整命令行，而这条命令行里就写着模式本身；
不加方括号就是自己匹配自己。更糟的是上一版这里用的是宽模式，屏幕上当场冒出三行别人音频服务的进程号 ——
既泄了本机隐私，又把「有没有留下东西」判成了「这台机器上一共有多少含这三个字母的进程」。
模式收紧到「本章真可能起的那两条」，0 才对应一个有意义的命题。

这一章的取证纪律收在这儿：所有「故意失败」都由脚本自己接住并原样打印（报错措辞跟版本绑定，只有本机 3.14.4 验过）；
临时目录全在 `mktemp -d` 出来的 `py09-XXXXXX` 里，由 `trap cleanup EXIT` 整份删掉；9 个取证脚本一个字没改。

**这一节带走：**

- 守卫必须当场演示过一次触发，再报 0；恒为 0 的检查比没有检查更糟。
- 能判的式子 = 具体判据：子目录数、非 `.py` 文件数、`__pycache__` 与 `.pyc` 数、`pyvenv.cfg` 与 `activate` 数。
- 「两次一样」比「一个绝对数字」可信：家目录那两处和第 1 节对照，条目数是机器的属性，相同才是凭证。
- 计数要问对本机有意义的东西：`jobs` 在一次性子进程里永远是空表，宽 `pgrep` 模式量到的是整台机器。
- 端口只报条数、路径一律折叠、隐私不进正文 —— 教材引用的是判断，不是这台机器的清单。

---

## 自检问题

答里给的是节号，不是行号；想不起答案就回那一节读围栏。

1. 你把 `mymod.py` 改了第三行，在同一个 REPL 里再 `import mymod` 一次，改动没出现。为什么？
（`sys.modules` 命中就**连文件都不打开**：改的是磁盘、查的是账本。第 3 节拿同名假模块证明了「不打开」这一半。
强制重跑只有两条路 —— `del sys.modules['mymod']` 或 `importlib.reload`，后者重跑内容但**不换对象**。第 2、3、15 节。）

2. `reload` 之后，之前那句 `from mymod import VALUE` 拿到手的 `VALUE` 会跟着更新吗？
（不会。`from` 绑的是当时的引用，`reload` 改的是模块 `__dict__` 里的键，动不到你命名空间那根线。第 2 节⑦、第 15 节。）

3. `import a.b.c` 执行完，你本地多了几个名字？`sys.modules` 里多了几条？
（本地只有 `a`；账本里 `a`、`a.b`、`a.b.c` 三条全进。`b`、`c` 是挂在父对象上的属性。第 2 节③、第 7 节第 2 段。）

4. 同一个 `x.py`，`python3 pkgx/x.py` 与 `python3 -m pkgx.x` 的 `sys.path[0]` 各是什么？哪个能拿到同目录的兄弟模块？
（前者是**脚本所在目录**、后者是**当前目录**；只有前者拿得到，后者报的是光板名字 `No module named 'neighbor'`。第 4 节、第 5 节第 3、5 段。）

5. 什么时候 `sys.path[0]` 是空串？空串指的是哪个目录，是启动那一刻定死的吗？
（`-c`、`-` 走 stdin、无参数这三种入口给空串。不是定死的：**每次 import 现拿 `os.getcwd()`**，
所以 `os.chdir` 一次，整本账换根而 `sys.path` 一个字没改。第 4、6 节。）

6. 你写了个 `select.py` 放在项目根，`import select` 拿到的是自己的文件；那 `time` 呢？为什么不一样？
（磁盘上的标准库模块（`json`、`csv`、`select`…）能被排在它前面的 `sys.path[0]` 抢走；
`time`、`sys` 这些**内置/冻结**层压根不在 `sys.path` 上、排在所有路径之前，遮蔽不了 ——
实测 `FAKE_WON=False`、`TIME_ORIGIN=built-in`、它连 `__file__` 都没有。第 5 节第 2 段④、第 6 段。）

7. `ModuleNotFoundError: No module named 'foo'` 这条报错里，有没有它查过的目录？
（没有：「提到模块名吗 → True ｜ 提到任何一条搜索路径吗 → False」。所以第一步是把 `sys.path` 摊开按层问，不是改代码。第 6 节④、第 5 节末尾那张表。）

8. 常规包的 `__file__` / `__path__` / `__package__` 各是什么？命名空间包这三个键有什么不同？
（常规包：`__file__` 指向 `__init__.py`、`__path__` 是**一项**的 `list`、`__package__` 是自己的名字。
命名空间包：`__file__` 是 `None`、`__path__` 是 `_NamespacePath` 且**可多项**、`__spec__.origin` 也是 `None`。第 7 节第 1、4 段。）

9. 两个根里都有 `merged/` 且都没有 `__init__.py`；再往排第一的根放一个 `__init__.py`，另一个根的东西还看得见吗？
（合并时：不同名的子模块两边都拿得到，**同名**的只认第一个根那份；
一旦排前的根有了 `__init__.py`，立刻退化成常规包，其它根的 `merged/` 内容**整个不可见**。
查这类错一行就够：`print(包.__path__)`。第 7 节第 4 段。）

10. `from pkg import *` 什么时候会替你 `import` 子模块？`__all__` 里写了个不存在的名字，炸出来的是什么异常？
（没有 `__all__` 时一个子模块都不导，只搬非下划线的现成名字；写了 `__all__` 就是换了一份名单，
名单里的**子模块名会被真的导入**。撒谎那格炸的是 `AttributeError`，`except ImportError` 接不住，而且整句作废、一个名字都不留。第 7 节第 3 段、第 2 节⑦。）

11. 为什么 `python3 pkgr/modr.py` 里一句 `from . import helper` 就报
`attempted relative import with no known parent package`？把文件挪个地方能好吗？
（挪没用。`.` 只看 `__package__` / `__spec__.parent` 这个**名字**，脚本跑法里它是 `None`、`__spec__` 也是 `None`。
正道是 `-m` 或把入口写到包外面；手工往命名空间里塞一个 `__package__` 也能成 —— 第 7 节第 5 段第三格演的正是这个。）

12. `python3 -m pkgr` 报 `No module named pkgr.__main__`，`python3 -m json.tool` 却跑起来了。差别在哪？
（`-m 包名` 找的是 `<包>.__main__`，没有 `__main__.py` 就只能得到那句「is a package and cannot be directly executed」；
`-m json.tool` 跑的是**包里的一个模块** `tool.py`，跟 `__main__` 无关。第 7 节第 6 段。）

13. venv 建好之后，`bin/` 里的 `python3` 是复制出来的吗？判断「我在不在 venv 里」最省事的一行是什么？
（不是复制，`bin/` 里全是**相对符号链接**；判据是 `sys.prefix != sys.base_prefix`。
venv 改写的只有 `sys.path` 这本账，解释器一个字节都没动。第 16、17 节，判据在第 1 节。）

14. `pyvenv.cfg` 里哪一行决定「venv 看不看得见系统包」？本章在不装任何包的前提下怎么把它演出来？
（`include-system-site-packages`。同一份骨架建**两个** venv（都 `--without-pip`，一个开一个关），
比的是两边 `sys.path` 里包目录的**条数**、以及系统那条 `dist-packages` 在不在账上 —— 全程没装过一个包。第 18 节。）

15. 本机 `python3 -m venv vv`（不带 `--without-pip`）会成功吗？本章怎么处理这件事？
（不会，本机没有 `ensurepip`。这条失败没有跳过，原文照打当证据用了（第 16 篇第 1、6 段）。
所以本章**没有任何**「装完包之后 import 会怎样」的结论 —— 见下面「本章没演」。）

16. `python -s`、`-S`、`-E` 关掉的是同一件事吗？各关什么？
（不是。`-s` 只关家目录那条 user site；`-S` 整个 `site` 都不执行，于是少掉两条系统包目录**并且** `sys.meta_path` 少一个 finder；
`-E` 忽略 `PYTHONPATH` / `PYTHONHOME` 这类环境变量。第 20 节那六行对照表。）

17. 往 venv 的 `site-packages` 放一个 `.pth`，除了加路径还有别的后果吗？安全面在哪？
（有：以 `import ` 开头的行是被 **`exec`** 的，启动时就执行代码，而且本机一次启动会跑**两遍**；
一条坏 `.pth` 的现场是「报错很响、退出码 0、它后面的行全部作废」。第 19 篇第 4、5、6 段。）

18. `.pyc` 的失效判据是内容哈希吗？缓存到底买到了什么？
（不是哈希，头部那两个数是**源码 mtime + 源码字节数**，四种改法四种结局逐格实测过（第 11 篇第 2、3 段）。
它买到的是「省掉 parse/compile」，**不是**「省掉执行」。第 11、12 节。）

19. 本机 `-c` 入口下 `sys.path` 有 6 项，为什么「数里面有几项带 `site-packages`」会数出 0？
（这台机器的包目录叫 **`dist-packages`**。本章按「`site-packages` 或 `dist-packages`」一起数，真实包目录是 2 条；
朴素数法数出 0 —— 这就是「未匹配先怀疑口径」在书里的一次落地。第 1 节、第 20 节②。）

20. 循环导入炸的到底是「有环」还是别的？`importlib.reload` 能救吗？
（炸的是**模块体的先后顺序**：同一个环、四种取法给四种不同原文；「半成品时刻」在 a 的模块体里看 b 的命名空间只有部分东西。
`reload` 救不了（第 9 篇第 5 段专门把它当误诊演）。三条出路各有实测 —— 延迟 import、只 `import 模块` 不 `from`、抽公共模块；
第四条是 `TYPE_CHECKING`，只给类型检查器看的那条边运行时不跑。第 9、10 节。）

## 本章没演 / 没验证（绝不编造，都在这）

写书最容易骗人的地方，是把「合理」写成「跑过」。下面这些本章没有证据，正文里也不该当结论用：

- **没装过任何一个包**。本机没有 `ensurepip`，所有 venv 都是 `--without-pip` 建的，`pip` 一次都没运行过。
  于是「装进 venv 之后 import 顺序怎么变」「`pip install -e` 往 `site-packages` 写了什么」「依赖冲突」全没演。
- **只在这台机器、这一个解释器上跑过**：CPython 3.14.4 / Ubuntu / 包目录叫 `dist-packages`。
  别的版本只在正文里作为**交代**出现（`-P` 是 3.11+、`consider renaming …` 的提醒是 3.12+、`-I` 自 3.11 起自带 `-P`、
  PEP 420 是 3.3+），**没有在那些版本上实测**；Windows、macOS、别的发行版一律未验证。
- **没写过自己的 `sys.meta_path` finder**。只在 `-S` 那一格看见本机 `dist-packages` 里有个 `.pth`
  往 `meta_path` 塞进一个 `DistutilsMetaFinder`（第 5 节第 2 段③），以及 `importlib` 的查/载协议（第 14 节）。
  自定义 finder、让 `find_spec` 返回自己的 loader，本章没演。
- **`.pyc` 只拆了头部 16 字节和文件名形状**（第 11、12 节）。说「正文是字节码 + 常量」用的是 `marshal` / `dis`
  当场看得见的东西，没做完整格式解剖；`-O` / `-OO` 会产出什么形状的缓存也没演。
- **`zipimport` 没有真打包验证**。账本上那条 `…/python314.zip` 在磁盘上并不存在
  （数的是「1 项 zip，磁盘上真存在的 0 项」），那是「列着 ≠ 存在」的证据，不是 zipimport 工作的证据。
- **没做并发导入实验**：多线程 / 多进程同时 import 同一个模块会怎样、`sys.modules` 的锁在哪一层，本章没碰。
- **venv 搬目录只测了一次**「整个挪走还能不能用」（第 16 篇收口那格），
  符号链接、`PYTHONHOME`、跨文件系统移动的组合没系统测。
- **没联网、没监听端口、没写你的家目录**。第 22 节的自证只报计数：家目录那两个位置的条目数在第 1 节与第 22 节各数一遍且一致，
  章目录的 `.py` 个数 / `demo.sh` 个数 / 其它文件数 / 子目录数 / `__pycache__` 个数全部对账，
  端口只看 `ss -lnt` 的条数、进程守卫只数 `pgrep -c` 的个数，并且这些计数**先被故意弄响过一次**才归零。

## 怎么跑这一章

前置只有一个：**先读第 1 节**。本章结论吃环境（四个 `PYTHON*` 开关、包目录叫 `dist-packages`、
本机没有 `ensurepip`），第 1 节把这些钉死了，后面每一节都回指它。

```
cd python-handbook/09-modules-and-venv
bash demo.sh              # 22 节一路跑完，跑完自动删沙箱
bash demo.sh --keep       # 跑完保留沙箱并打印路径，进去自己接着敲
```

`demo.sh` 把章目录那 9 个取证脚本（`a_import_basics.py` … `i_site_and_userdir.py`）整份复制进 `mktemp -d`
出来的沙箱再跑，**章目录本身从头到尾不参与写**（第 22 节自证）。每格用 `banner` 起头、`runc` 执行、`note` 收尾，
所以屏幕形状和本 README 的围栏一一对应。

想单点复现某一节，看标题就知道去哪找：

- 标着「**x 篇完整跑**」的节（第 2、5、7、9、11、13、14、16、19 节）= 跑一个 `.py` 脚本，
  例如 `python3 b_sys_path_shape.py`，脚本自己造子目录、自己收尾。
- 标着「**小证据**」「**现场格**」的节（第 3、4、6、8、10、12、15、17、18、20 节）= 一段 `python3 -c` 或几条 shell，
  围栏里连命令一起贴全了，可以直接抄着敲。
- 第 21 节是把三本账并成一张合并视图的跨脚本小结，第 22 节是本章给自己的对账。

自己敲的时候有两件事跟屏幕不一样，是**正常**的：

1. **沙箱名不同**。`mktemp` 的随机后缀每跑必变，所以本章从来不印它，路径一律折成 `<沙箱>` / `<cwd>` / `<家目录>` / `<py>`。
   第 5 节那条 `<py>/lib/python314.zip` 和第 1 节那条 `/usr/lib/python314.zip` 是同一个东西的两种写法，不是两份路径。
2. **你机器上的包目录可能叫 `site-packages`**。那第 20 节「真实包目录 2 条 / 朴素数 `site-packages` 数出 0」这两格
   在你那儿会数成一样，这不影响主线 —— 主线是「`sys.path` 这本账被 `pyvenv.cfg` 那一行改写」，不是具体条数。

本章的每条引文都由脚本从定稿捕获按行号搬出（占位符标行号 + `expand_fences.py` 搬出原文），交付闸门是一条命令：

```
bash ~/.local/bin/chapter_gate.sh README.md ~/.cache/handbook-captures/py09-final.txt \
     '/tmp/py09-[A-Za-z0-9]{4,8}'
```

它判六件事：占位符残留 0、围栏成对、含提示符的行逐字对上捕获、无真实身份、片段与 README 同步、收尾节齐。

## 术语小抄

| 术语 | 一句话 | 本章哪节 |
|---|---|---|
| `sys.modules` | 进程内的模块账本，`import` 的第一入口；命中就**什么都不做** | 2、3 |
| 模块体 | 一个 `.py` 从第一行执行到最后一行这件事本身；`import` 跑的是它，不是「定义」 | 2、7 |
| `reload` | 重跑模块体、**不换对象**；手里的旧引用一个都不更新 | 2、15 |
| `sys.path` | 能被现场改写的普通 list，顺序 = 优先级，且只在当前进程生效 | 5、6 |
| `sys.path[0]` | 「根」，形状由**启动方式**决定：脚本目录 / 当前目录 / 空串 / 不给 | 4、5 |
| 空串项 | 不是路径，是「每次 `import` 现拿 `os.getcwd()`」，会跟着 `chdir` 漂 | 4、6 |
| `-P` / `PYTHONSAFEPATH` | 把入口那一项整个不给（3.11+）；`-I` 从 3.11 起自带 `-P` | 4、5 |
| `-s` / `-S` / `-E` | 分别关 user site、整个 `site`、`PYTHON*` 环境变量 —— 关的**不是**同一件事 | 20 |
| `dist-packages` | Debian/Ubuntu 上包目录的名字，所以本章按两种名字一起数 | 1、20 |
| 四层来源 | 入口项 → `PYTHONPATH` → 标准库 + site → 内置/冻结（最后一层不在 `sys.path` 上，遮蔽不了） | 5 |
| `sys.meta_path` | 「谁去找」的 finder 队列；`site` 能往里塞东西，于是改的是**查找过程本身** | 5、19 |
| `__init__.py` | 包的模块体：首次 `import` 从最外层往里执行；也是把同名目录切断的那条界线 | 7 |
| `__path__` | 包自己的搜索根（常规包 1 项 `list`，命名空间包 `_NamespacePath` 可多项） | 7 |
| 命名空间包 | 没有 `__init__.py` 的目录：`__file__` 是 `None`，多个根合并成同一个包 | 7 |
| `__all__` | `from x import *` 的唯一白名单；列子模块名会被**真的导入**；撒谎给 `AttributeError` | 7、13 |
| `if __name__ == '__main__'` | 决定「这个文件被当脚本还是被当模块」，不是仪式 | 13 |
| `__package__` / `__spec__.parent` | 相对导入唯一的锚点：**名字**，与文件在哪无关 | 7 |
| `__pycache__` / `.pyc` | 编译缓存，名字跟着实现+版本走（`cpython-314`），不可移植 | 11、12 |
| 缓存头部两个数 | 失效判据是**源码 mtime + 字节数**，不是内容哈希 | 12 |
| `find_spec` / `import_module` | 把 `import` 语句还原成可编程调用的协议：查、载、重载 | 14 |
| `spec_from_file_location` 三件套 | 从任意路径把一个文件装成模块，`__name__` 由你说了算 | 14 |
| 循环导入 | 炸的是模块体先后顺序，不是「有环」本身；四条出路各有实测 | 9、10 |
| `TYPE_CHECKING` | 只给类型检查器看的那条 import 边，运行时不跑 | 9 |
| venv | 只换 `sys.path` 这本账：`bin/` 全是相对符号链接 + 一个 `pyvenv.cfg`，解释器不复制 | 16、17 |
| `pyvenv.cfg` | venv 唯一的配置文件；`include-system-site-packages` 一行换掉整本路径账 | 18 |
| `activate` | 就是几行 `export`：手工设环境变量起子进程，效果一样，魔法为零 | 16 |
| `site` 模块 | 启动时填 `sys.path` 的那只手；`.pth` 是它塞进来的代码（`import ` 开头的行被 `exec`） | 19 |
| user site | `~/.local` 那条通道；`ENABLE_USER_SITE=True` 是**声称**，在不在账上才是**事实** | 1、19、20 |

## 本章之后

第 10 章讲 **typing 与工具链**：`mypy` / `ruff` / `pytest` 这类工具装在哪、配置文件读的是哪一份、
虚拟环境和它们怎么配合 —— 它的前提正是本章第 16~20 节：**环境是 `sys.path` 的一层，不是「另一个 Python」**。
第 11 章讲 **子进程与并发**：本章反复用「干净 env 起子进程」（`sys.path` 的运行时改动**不跨进程**，
`PYTHONPATH` 却能穿透），那套 `subprocess` 的形状在第 11 章正式展开；
本章留在桌上的那个问题 —— 同一进程里并发 `import` 时锁在哪一层 —— 也是第 11 章才碰得到。

## 本章回指

- 第 01 章「解释器把源码交给谁」在这里落成具体的一本账：`import` 查 `sys.modules` → 查 `sys.path` → 读文件 → 编译 → 执行。
- 第 02 章「变量是名字，不是盒子」在本章第 2 节③④⑤⑥ 反复兑现：`del` 删名字不删对象，`from` 绑的是引用。
- 第 05 章「默认值在 `def` 那一刻冻住」是第 2 节第 5 段的边界：改 `m.RATE` 改得动运行时全局名，改不动 `__defaults__`。
- 第 07 章的异常家谱在这里用了一次：`ModuleNotFoundError ⊂ ImportError`，而 `__all__` 谎报给的是 `AttributeError` —— 那个它接不住。
- 第 08 章的缓冲与 stdout/stderr 顺序，在第 7 节第 5、6 段咬了两次人：两行 `[INIT]` 会排在 traceback 后面，
  真实顺序仍是 `__init__.py` 先跑完。
