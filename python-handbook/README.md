# Python 大白话手册

> 一句话：这本手册不讲「Python 语法有哪些」，只讲**你敲下 `python3 xxx.py` 之后，
> 机器上到底发生了什么**——解释器读了哪些路径、编译出了什么、缓存写在哪、
> 报错原文的每一行是什么意思。每一个结论都有一份能跑的代码和一段真实终端输出撑着。

---

## 这本手册解决什么问题

你会写 `for` 循环、会用 `dict`，但下面这些事一直是「玄学」：

| 你的现场 | 别处怎么糊弄你 | 本手册给什么 |
|---|---|---|
| `import xxx` 有时成功、换个目录跑就 `ModuleNotFoundError` | 「加个 `sys.path.append` 就好了」 | 把 `sys.path` 逐条打出来，讲清**第 0 项到底是谁塞进来的**（01 章实测 4 种启动方式） |
| 目录里莫名多出 `__pycache__/`，不敢删 | 「缓存而已」 | 直接解析 `.pyc` 的 16 字节头部和里面的 marshal 数据（01 章） |
| `python` 找不到、`python3` 找得到；`pip` 又不见了 | 「你 PATH 没配好」 | `which` / `sys.executable` / `sys.prefix` 三方对照，本机实测（01 章） |
| `print` 到管道里就「卡住不吐字」 | 「加 flush 就行」 | 伪终端 vs 管道，同一份代码数「0.5 秒时已经到达几行」的对照实验（01 章） |
| `def f(a=[])` 第二次调用居然带着上次的元素 | 「别用可变默认值」 | 真实复现，并解释**默认值只在 `def` 执行那一次求值**（05 章） |
| `a is b` 和 `a == b` 结果时对时错，`256 is 256` 真 `257 is 257` 假 | 「别用 is 比数字」 | 小整数缓存的实现原因和边界（02 章） |
| 网上说「Python 慢」，但 list 求和比 numpy 还快？ | 「解释器就是这样」 | 字节码层面看**每条指令在干什么**，再用 `timeit` 实测（01/10 章） |
| 加了多线程，CPU 反而只有一个核在跑 | 「有 GIL，别用多线程」 | 本机真实测量 GIL 的影响，并给出 threading / multiprocessing / asyncio 的分工表（11 章） |
| 报错一大坨 traceback，不知道看哪一行 | 「从下往上看」 | 把 `NameError` / `ModuleNotFoundError` / `IndentationError` 的**完整原文**逐行拆给你看（01 章） |
| 不知道 `if __name__ == "__main__"` 到底在防什么 | 「惯例，写上准没错」 | 两种启动方式的真实差异（09 章） |
| 想装个包，`pip install` 直接报 EXTERNALLY-MANAGED | 「加 --break-system-packages」 | 本机实测这条路为什么会失败，以及 venv 存在的理由（09 章） |

共同点：这些都不是语法问题，是**运行机制问题**。搞懂机制，命令不用背。

---

## 全书主线：一条命令下去，经过这几站

```
    你敲：python3 pkg/main.py arg1
                │
                ▼
   ┌────────────────────────────┐   01 章：python3 是谁？sys.executable / PATH
   │ 1. shell 找到解释器并起进程 │
   └─────────────┬──────────────┘
                 ▼
   ┌────────────────────────────┐   01 章：sys.path 怎么拼出来的
   │ 2. 算出 sys.path，跑 site  │      （脚本所在目录 ≠ 当前目录！）
   └─────────────┬──────────────┘
                 ▼
   ┌────────────────────────────┐   01 章：compile() → 字节码 → __pycache__
   │ 3. 源码 → AST → 字节码      │      （.pyc 头部 16 字节里存了什么）
   └─────────────┬──────────────┘
                 ▼
   ┌────────────────────────────┐   02/04/05/06 章：一切皆对象，
   │ 4. 逐条执行字节码，操作对象 │      变量是「贴在对象上的标签」
   └─────────────┬──────────────┘
                 ▼
   ┌────────────────────────────┐   07 章：异常不是「返回错误码」，
   │ 5. 出错 → traceback → 退出码│      是往栈上扔对象 + 现场快照
   └────────────────────────────┘
```

后面每一章都挂在这条线上：02~06 是第 4 站的细节，07 是第 5 站，08 是「和磁盘打交道」，
09 回到第 2 站讲包与虚拟环境，10 讲「怎么自己看见这些」（工具），11 讲「多个人同时跑第 4 站会怎样」。

---

## 全书章节地图

| 章节 | 目录名 | 一句话 | 状态 |
|---|---|---|---|
| 01 | `01-whats-really-happening/` | 解释器到底做了什么：`-c` vs 脚本、字节码、`__pycache__` 内部、`sys.path` 查找顺序、缓冲与管道、报错原文、`-O`/`-B`/`PYTHONPATH` 留下的证据、`timeit` 实测 | ✅ **已交付** |
| 02 | `02-types-and-variables/` | 变量是标签不是盒子；int/str/bool/None；`is` vs `==`；小整数缓存这个真实陷阱 | ✅ **已交付** |
| 03 | `03-strings-and-text/` | 切片、f-string、split/join；UTF-8 字节 vs 字符；`unicodedata` | ✅ **已交付** |
| 04 | `04-collections/` | list/dict/set/tuple 的槽位、扩容台阶与复杂度实测；复制六级梯子；四族经典坑；推导式与生成器；`collections` 四兄弟 | ✅ **已交付** |
| 05 | `05-functions-and-pitfalls/` | `def` 只跑一次（默认值/装饰器都在定义那一刻）；可变默认参数陷阱真实复现 + 三种正解；传参＝一次赋值；五种参数筐与报错原文全集；闭包与晚绑定；`UnboundLocalError`；`partial`/`lru_cache`；递归限额与无尾调用优化；`*_FAST`/`*_GLOBAL`/`*_DEREF` 与 `timeit` 实测；`@X ≡ f = X(f)` | ✅ **已交付** |
| 06 | `06-classes-and-dataclasses/` | 魔术方法、`@dataclass`、`__slots__`、属性 vs 方法 | ✅ **已交付** |
| 07 | `07-errors-and-context/` | `try/except/else/finally` 走位图；异常家谱与 `except Exception` 接不住的那三个；`__cause__` vs `__context__`；traceback 怎么读、怎么程序化拿；11 族报错原文现场构造；自定义异常；`with` 协议、`@contextmanager`、`contextlib` 四件、`ExitStack`；退出码/`atexit`/`os._exit`/线程里的静默 | ✅ **已交付** |
| 08 | `08-files-and-pathlib/` | 编码 / 换行 / 路径是三套独立规则：`open` 的四个模式维度（`w` 打开即截断）、缓冲与 `flush`、`newline=` 读写两侧、默认编码的四个来源与 BOM、`errors=` 五种结局、`Path` 的「纯算 vs 问内核」、`os.path` 逐个对照、`mkdir/unlink` 的幂等写法与「删文件看父目录权限」、`glob` 的三个不保证、`mkstemp` 为什么是 0600、`resolve` vs `absolute` vs `normpath`、csv 管什么不管什么、json 为什么不保存类型、原子写的唯一姿势、大文件的 `seek` cookie、`tracemalloc` 比大小 | ✅ **已交付** |
| 09 | `09-modules-and-venv/` | 一次 `import` 背后是三本账：`sys.modules`（命中就什么都不做）、`sys.path`（四层拼成、顺序即优先级、`sys.path[0]` 的形状由入口决定、空串跟着 `chdir` 漂、内置/冻结层遮蔽不了）、`__pycache__`（头部只有两个数、失效判据、四种「别写缓存」）；包 = 带 `__init__.py` 的目录、命名空间包与 `__path__`、`from pkg import *` 与 `__all__`、循环导入炸的是模块体先后顺序而非「有环」、同一环两种入口两种生死、`importlib` 把 `import` 还原成可编程协议、`reload` 不换对象；venv 只换 `sys.path` 这一本账（骨架三样、`bin/` 全是符号链接、`pyvenv.cfg` 一个开关换掉整本路径账）、`site` 才是填账本的那只手（`.pth` 是被 `exec` 的代码）、`-P`/`-I`/`-s`/`-S`/`-E` 四种「关」关的不是同一件事 | ✅ **已交付** |
| 10 | `10-typing-and-tools/` | 3.14 的注解是**一段还没执行的代码**：`def g(x: Widget)` 定义照样成功、第一次读 `__annotations__` 才 `NameError`、`__annotate__`、`annotationlib` 的四种读法；运行时到底拦不住什么（`Final` 不拦、`Protocol` 的 `isinstance` 只查方法名不查签名、`get_origin/get_args` 是唯一真干活的那批）；本机 **mypy/pyright/ruff/black/flake8/pylint/pytest 全缺席、连 `pip` 都没有**（发行版另外带了 `requests`/`rich`/`setuptools`/`yaml`，但没有一个管类型，名单见第 12 章第 1 节），本章用得上的第三方库只有 `typeguard` 一个 → 「没装静态检查器时怎么用标准库自己搭验证」 | ✅ **已交付** |
| 11 | `11-concurrency/` | GIL 管的是「一次让谁跑字节码」，不是「能不能并发」：等待时它放手（并发排法下同时挂在等待里的峰值 4，串行排法只有 1，总时长短过串行的六成）、算死时它收税（4 线程干 4 份反而不比 1 线程干 1 份快）。`threading` 的两副面孔与第三副「退出」（daemon 被硬切）；`+=` 丢更新三联 —— 本机 4 线程 × 20 万次**纯 `+=` 一条不丢**，改成「读→让出→写」才必丢，加锁才谈得上「一条不丢」；`multiprocessing` 本机默认 **forkserver**，它把主模块整份重新 import（没有 `if __name__` 守卫 → 当场 `RuntimeError` 原文，函数藏在守卫里面 → `AttributeError: module '__mp_main__' has no attribute ...`），`get_start_method()` 只**问过一次**全局默认就再也 set 不动（`force=True` 才掰得开），进程之间只共享「复印件」，要么 `Value` 要么 `Manager`；`asyncio` 里 `f()` 不等于跑了（`coroutine ... was never awaited` 原文），量「循环被冻住」只能数拍子不能数秒（同步 `sleep` 那一刻 0 拍、总拍数照样满 20）；`concurrent.futures` 的 `map` 保投递序、`as_completed` 保完成序、异常攒到 `.result()` 才抛、`max_workers` 只是**上限**；`queue` 收尾不塞哨兵就是永久等待；收口表把同一件活（等 / 算）在四种写法上各跑一遍 —— 屏幕上**一个秒数、一个 pid 都没有**，因为那些数每跑必变 | ✅ **已交付** |
| 12 | `12-cheatsheet/` | 全书速查，出活时每天翻的那一页：**11 张表 × 8 行**（每行都是「直觉以为什么 → 屏幕上实际是什么 → 出处」）、**四段能直接抄的骨架**（脚本开头与退出码 / 读 JSON 与原子写 / 最小 CLI / 并发收口）、**一页纸 20 条**。★ 这一章不新产证据，它把枪口对准**这一页本身**：157 次出处逐条解析（认四种标题形状，断链 0、覆盖 11 章）、页面结构当场数（表数 11、每表 4~8 行、列数、空格子、一页纸 20 条）、四段骨架在内存里过 `ast.parse` + `compile()`、12 组 51 条硬事实用同一个解释器当场重判、隐私五类全 0、章目录只读自证 —— 每条守卫都先被人为弄响一次。本机名单也在这里钉死：`pip`/`mypy`/`ruff`/`black`/`pytest` 全缺席，在场的是发行版带来的 `requests`/`rich`/`setuptools`/`typeguard`/`yaml` | ✅ **已交付** |

### 对规划表的三处微调（理由）

1. **09 章的 venv 部分必须改成「先讲清本机坏在哪，再讲正确姿势」。**
   实测 `python3 -m venv <dir>` 在这台机器上**直接失败退出码 1**，因为发行版没带 `ensurepip`
   （见文末「本机环境」里的原样报错）；`--without-pip` 才建得出来。所以这一章不能按
   「venv 一键就好用」写，要按「venv 是什么 → 本机为什么缺件 → 缺件时还能干什么 →
   `apt install python3.14-venv` 是需要你自己在有网的机器上做的动作」来写，并且**手册本身绝不执行任何安装**。
2. **10 章的 mypy 改成「类型注解 + 本机可用的替代验证」，主线换成 3.14 的惰性注解。**
   `mypy` 未安装、`pip` 不存在，硬写「装个 mypy 跑一下」就是编造。实测缺席的还有
   `pyright`/`ruff`/`black`/`flake8`/`pylint`/`pytest`（`importlib.util.find_spec` 逐个探过，全是 `None`），
   唯一在场的第三方类型相关库是 `typeguard`。而 3.14 把注解改成了**惰性求值**：
   `def g(x: Widget)`（`Widget` 不存在）**定义照样成功**，函数上多一个 `__annotate__`，
   真正炸的是第一次读 `__annotations__` 那一下（`NameError`，traceback 里那帧就叫 `__annotate__`），
   `annotationlib` 这个新模块给四种读法（`VALUE`/`FORWARDREF`/`STRING`/`VALUE_WITH_FAKE_GLOBALS`）。
   所以这一章按「注解是一段**还没执行的代码** → 谁去读它、读到什么 → 运行时到底拦不住什么
   （`Final` 不拦、`Protocol` 的 `isinstance` 只看方法名） → 没有静态检查器时用标准库自己搭验证」来写，
   并**绝不假设读者装了任何工具**。取证契约与全部实测事实见
   `~/.cache/handbook-captures/py10-SCRIPT-CONTRACT.md`。
   本机真能用的验证手段是 `py_compile` / `compile()` / `ast` / `annotationlib` / `warnings`，
   `timeit` 与 `pdb` 只讲怎么用、不拿耗时数字当结论；mypy 部分只给「装好之后长这样」的
   **待办说明并标注本机未验证**。
3. **01 章把「stdout 缓冲」和「报错原文」提升为独立场景。**
   这两个是新手最高频、又最没人讲机制的坑（管道里看不到进度、traceback 看不懂），
   而且都能在本机做出**可复现的量化实验**，所以放第 01 章而不是塞到最后。

---

## 怎么用

**每章一个目录，目录里一份 `README.md` + 若干示例 + 一份 `demo.sh`。**

```bash
cd python-handbook/01-whats-really-happening
bash demo.sh              # 一键复现本章 README 里的全部输出（全程在 mktemp 沙箱里，跑完自动删）
bash demo.sh --keep       # 保留沙箱并打印路径，进去自己接着敲
bash demo.sh > /tmp/py01.txt 2>&1   # 收全量纯文本；重定向时脚本自动不上色
```

推荐学习动作，和 DS 手册一样的节奏：

1. 先读章节 README 的「你会遇到的场景」，找到自己的现场；
2. 看「心智模型」那张 ASCII 图，先建立猜测；
3. 打开对应的小示例源码（每个文件都能单独 `python3 xxx.py` 跑）；
4. `bash demo.sh --keep`，进沙箱把 README 里的命令自己再敲一遍，**对照输出是否逐字一致**；
5. 不一致就是你环境不同（版本 / PATH / locale），这本身就是最有用的信息。

约定：章节 README 里**所有 ```text 代码块都是终端输出的原文节选**，不是示意图。
每一节标了「demo 第 N 节」，可以按编号回 `demo.sh` 找那段命令。
沙箱路径里的随机后缀（`/tmp/py01-XXXXXX`）每次都不同，除此之外的行应当可复现。

---

## 本机环境（下面每一条都是这台机器真跑出来的，不是印象）

```text
$ python3 --version
Python 3.14.4
$ which python3
/usr/bin/python3
$ type -a python3
python3 is /usr/bin/python3
python3 is /bin/python3
$ ls -l /usr/bin/python3
lrwxrwxrwx 1 root root 10 Mar 21  2026 /usr/bin/python3 -> python3.14
$ ls -l /usr/bin/python3.14
-rwxr-xr-x 1 root root 7477160 Aug 20 18:41 /usr/bin/python3.14
$ python --version
/bin/bash: 行 1: python: 未找到命令
$ echo $SHELL
/bin/bash
$ bash --version | head -1
GNU bash，版本 5.3.9(1)-release (x86_64-pc-linux-gnu)
$ uname -a
Linux <主机名> 7.0.0-14-generic #14-Ubuntu SMP PREEMPT_DYNAMIC Mon Apr 13 11:09:53 UTC 2026 x86_64 GNU/Linux
$ echo $PATH
<家目录>/.qoder/bin:<家目录>/.qoder/plugins/cache/qoderapp-bundler/qoder-context/1.0.72.34cf9e2f/bin:<家目录>/.qoder/bin:<家目录>/.qoder/entry:<家目录>/.local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin
```

解释器自己的说法：

```text
$ python3 -c "import sys; print(sys.executable); print(sys.version); print(sys.prefix); print(sys.base_prefix)"
/usr/bin/python3
3.14.4 (main, Aug 20 2026, 10:41:58) [GCC 15.2.0]
/usr
/usr
```

`sys.path` 的默认内容（在 `/tmp` 下跑，第 0 项是 `''`，代表当时把什么放进去了）：

```text
$ python3 -c "import sys; print(sys.path)"
['', '/usr/lib/python314.zip', '/usr/lib/python3.14', '/usr/lib/python3.14/lib-dynload', '/usr/local/lib/python3.14/dist-packages', '/usr/lib/python3/dist-packages']
```

第三方工具在这台机器上的真实状态（**决定了哪些章节不能怎么写**）：

```text
$ python3 -m pip --version
/usr/bin/python3: No module named pip
[退出码 1]
$ command -v mypy || echo "mypy: 未安装"
mypy: 未安装
$ ls -l /usr/lib/python3.14/EXTERNALLY-MANAGED
-rw-r--r-- 1 root root 645 Aug 20 18:41 /usr/lib/python3.14/EXTERNALLY-MANAGED
$ python3 -m venv /tmp/probe-venv        # 在本机建 venv 的真实下场
The virtual environment was not created successfully because ensurepip is not
available.  On Debian/Ubuntu systems, you need to install the python3-venv
package using the following command.

    apt install python3.14-venv

You may need to use sudo with that command.  After installing the python3-venv
package, recreate your virtual environment.

Failing command: /tmp/probe-venv/bin/python3
[退出码 1]
$ python3 -m venv --without-pip /tmp/probe-venv2 && ls /tmp/probe-venv2/bin
Activate.ps1
activate
activate.csh
activate.fish
python
python3
python3.14
𝜋thon
$ rm -rf /tmp/probe-venv /tmp/probe-venv2
```

由此得出的硬性写法约束（全书遵守）：

- **没有 `python` 这个命令，只有 `python3`。** 所有示例统一写 `python3`。
  （顺带一个有意思的点：`python` 命令在 venv 里反而存在，那是 venv 自己建的符号链接。）
- **没有 pip、没有 mypy，绝不安装任何东西。** 手册只用**标准库**；
  需要第三方包的场景一律改成本机可用的标准库等价物，或者明确标注「本机未验证」。
- **`-m venv` 默认会失败**（缺 `ensurepip`）。09 章按这个真实情况写。
- 终端消息的语言会变：Python 自己的输出恒为英文，但 `command not found` 这类是 **bash 的翻译**。
  上面 `python --version` 那条你大概率看到中文的「未找到命令」；demo.sh 里把 `LC_ALL`/`LANG`
  设成 `C.UTF-8`，为的就是整份 transcript 可以逐字比对。

---

## 全书安全约定

| 约定 | 为什么 |
|---|---|
| **只用标准库，绝不 `pip install`、绝不联网** | 本机根本没有 pip；而且手册要能在任何机器上「照着敲就复现」，不能依赖下载 |
| **一切有副作用的操作都在 `mktemp -d` 沙箱里** | 手册示例会写文件、建 venv、造 `__pycache__`、造垃圾目录。仓库必须干净 |
| **不碰 `~/.bashrc` / `~/.profile` / `~/.config` / `~/.local`** | 演示 `PATH`、`PYTHONPATH`、venv 激活时只改**当前子 shell** 的环境变量，绝不动你的配置文件 |
| **不卸载、不覆盖、不 `rm -rf` 仓库里既有的东西** | 清理只针对自己刚在 `/tmp` 里建的那份；`trap cleanup EXIT` 保证异常退出也清 |
| **`demo.sh` 故意不加 `set -e`** | 这一本手册有一半内容是「演示失败现场」（`ModuleNotFoundError`、venv 建不起来、缩进错误）。加了 `set -e` 就演不了，脚本会在第一个故意失败处直接停住 |
| **每条命令后打 `[退出码 N]`** | 光看报错文本分不清「这是正常的失败」还是「脚本自己崩了」；退出码就是判据 |
| **只在输出到终端时上色**（`[ -t 1 ]`） | 重定向到文件时必须是干净纯文本，README 才能逐行引用、`grep -qF` 才能核对 |
| **不猜密码、不连外部主机、不打印任何私钥** | 同本仓库其他手册的红线 |
| **屏幕上不出现真实身份**：`ls -l` 的属主列、`/home/<用户>` 这类家目录路径、`uname -a` 里的主机名，README 里统一写成 `<用户>` / `<家目录>` / `<主机名>` | 这几样是 shell 自己展开的，demo 拦不住；一本公开的手册不该带着你的登录名。**这是全书唯一对真输出的替换**，所以引文校验器会在这些行各报 1 行「未匹配」，属预期，看到 `<用户>` / `<家目录>` 开头的未匹配行可以直接放过 |

**和仓库根 `Makefile` 的关系**：它只 `find` 恰好三层深的 `main.c` 来编译数据结构章节。
本手册全是 `.py` / `.sh` / `.md`，不参与也不影响那套构建；
本手册的示例也**绝不**出现 `<手册>/<章节>/main.c` 这种三层路径。
`__pycache__/`、`*.pyc` 一律只出现在 `/tmp` 沙箱里，仓库目录里不会留下它们。

---

## 下一步

- 从 [`01-whats-really-happening/`](01-whats-really-happening/README.md) 开始。它一章就把
  「解释器读哪、编译成什么、缓存放哪、报错怎么写」四件事全部做实，后面 11 章都默认你有这张图。
- 01 章的入口：`cd python-handbook/01-whats-really-happening && bash demo.sh`。
- **当前进度：01~12 十二章全部已交付**（每章都是 `demo.sh` + 7~25 个取证脚本 + 逐字保真的 README；
  第 12 章是全书收口的速查页，它自己不产证据，只把前 11 章的出处、结构、硬事实逐条重核对一遍）。
  全书的转折点是第 05 章
  （[`05-functions-and-pitfalls/`](05-functions-and-pitfalls/README.md)，25 节）：
  「`def` 只跑一次、调用才查名字」这两句话在后面每一章都会被引用 ——
  第 06 章的 `@dataclass`/`@property`、第 07 章的异常原文、第 20 节的递归与栈，
  全部建立在这两条上。**装饰器**（`@X ≡ f = X(f)`）在第 05 章第 23 节正式讲完，
  第 06 章只是拿现成的用。
- 第 07 章（[`07-errors-and-context/`](07-errors-and-context/README.md)，21 节 / 19 个脚本）是全书
  第一次把「报错」当控制流讲：每一族报错都在本机现场构造、原文抄下来，
  并且把「异常在哪四种入口被咽掉」钉成一张表 —— 这一章的 `s_checklist.py` 还会数自己写了几节。
- 第 08 章（[`08-files-and-pathlib/`](08-files-and-pathlib/README.md)，22 节 / 19 个脚本 / 3351 行，
  含 `$` 的引文 399 行逐行核过）是全书第一次把代码写到**进程外面**：一旦落到磁盘，你就同时踩在
  编码、换行、路径三套规则上，而这三套经常被当成一套。本章最贵的一条是 `open(p, 'w')` —— 截断发生在
  `open()` 返回之前，进了 `with` 块再反悔已经晚了，而且全程没有一条报错提醒你。
  第 20 节的对账工具当场**造五个坏样本让五条规矩各响一次**，第 21 节自证沙箱没在章目录留东西。
- 第 09 章（[`09-modules-and-venv/`](09-modules-and-venv/README.md)，22 节 / 9 个取证脚本 / 2893 行 / 89 段围栏，
  含 `$` 的引文 604 行逐行核过、未匹配 0）把「一个 `.py` 怎么变成一个可导入的名字」做实：
  本章的主张是 **`import` 查的是三本账，而 venv 只改了其中一本** —— `sys.modules` 决定跑不跑、
  `sys.path` 决定从哪跑、`__pycache__` 决定用不用新的。最贵的一格是第 4、5 节：
  `sys.path[0]` 的形状由**启动方式**决定（脚本给脚本目录、`-m` 给当前目录、`-c` 给空串、`-P`/`-I` 连空串都不给），
  所以「我这能跑、他那跑不了」通常不是语法问题，而是根在哪的问题；新建一个 `json.py` 能让标准库消失也是同一件事。
  第 16 节还诚实地演了本机没有 `ensurepip` 时 `python3 -m venv` 的**真实失败原文**，
  然后把 venv 骨架（`bin/` 全是符号链接 + 一份 `pyvenv.cfg`）手工搭出来讲机制。
- 第 10 章（[`10-typing-and-tools/`](10-typing-and-tools/README.md)，14 节 / 7 个取证脚本 / 2165 行 / 79 段围栏，
  含 `$` 的引文 267 行逐字核过、未匹配 0，七支脚本判据合计 233/233）把注解还原成**一段还没执行的代码**：
  `def g(x: Widget)`（`Widget` 不存在）定义照样成功，炸的是第一次读 `__annotations__` 那一下，
  而且「求值成功才缓存、失败不缓存」是用计数器量出来的，不是背出来的。
  本章最贵的一格是第 04 节：`annotationlib.Format` 四把钥匙插进同一支坏注解的函数，
  给出 `NameError` / `ValueError` / `ForwardRef` 对象 / 源码文本四种完全不同的东西 ——
  而 `get_annotations()` 的**默认**那把恰好是会炸的那把，所以「凡读注解必先 `try`」是唯一走得通的写法。
  第 12~13 节诚实交代本机没有任何静态检查器（连 `pip` 都没有），
  于是把 `typeguard` 的插桩边界和「自己用 `ast` 搓一把尺」都摊开跑给你看。
- 第 11 章（[`11-concurrency/`](11-concurrency/README.md)，15 节 / 7 个取证脚本 / 2237 行 / 92 段围栏，
  定稿捕获 844 行，含 `$` 的引文 230 行逐字核过、未匹配 0，七支脚本各自的判据 34/33/51/44/42/43/2 全齐）
  把「并发」这个词拆成四本**互相看不见**的账：GIL 只管「一次让谁跑字节码」，`threading` 的
  `active_count()` 只是它自己的名册，`multiprocessing` 的默认 `forkserver` 会**重新 import 一遍你的主模块**，
  `asyncio` 里「不 await 就一行都不执行」。本章最贵的两格都在**否掉一句到处都在抄的话**：
  第 04 节第 5 格 —— 本机 4 线程 × 20 万次「纯 `box["n"] += 1`」一条都不丢（默认节拍和 `1e-6` 两种口径都不丢），
  只有改成「读 → 让出 → 写」才必丢，所以「GIL 下 `+=` 一定丢更新」在本章写作「本机演不出来」；
  第 07 节第 8 格与第 10 节第 2 格 —— 4 个进程给不出 4 倍速，屏幕上只留「明显快过串行吗 → True」
  「够到 4 倍速了吗 → False」这两个布尔，倍数本身的读数一个字都不上屏。
  因为「快了多少」每跑必变，本章的收口表（第 13 节）**一个秒数、一个 pid 都没有**，
  选型判据被压成一句「你的时间花在等，还是花在算」。第 14 节把两套对账（七支脚本的地板必须只有一个值、
  屏幕上七类禁串必须全 0）都先**故意弄响一次**再报 0，包括一张假的 `rc=124` 和一份假的 `43/44`。
- 第 12 章（[`12-cheatsheet/`](12-cheatsheet/README.md)，887 行 / 26 段围栏 / 定稿捕获 522 行：三批各连跑两趟以上、
  每批内部字节一致 —— 改之前读数 157，按 88 行逐行审核的意见改完那 5 行变成 158，再在章尾补一段审核记录变成 160（已冻结）。
  两次变化都只差那 4 行「读数：出处出现 …」，因为动出去的只有两处章号引用，去重 119 条与断链 0 从头到尾没动。
  含 `$` 的引文 234 行逐字核过、未匹配 0，
  正文数字断言回捕获核对 `DIFF 0 / NOKEY 0`）是全书的**收口件**：它自己不产新证据，而是把前 11 章压成
  11 张表（每表 8 行、三列「直觉以为什么 / 屏幕上实际是什么 / 出处」）、四段骨架和 20 条一页纸。
  本章的 `demo.sh`（八节 / 1426 行 / 沙箱里现写 10 支核验脚本 + 一支 12 组 51 条判据的探针合集）
  把枪口对准**这一页本身**：出处当场解析（160 次、去重 119 条、断链 0、覆盖 11 章，认四种标题形状）、
  页面结构当场数（表数 11 且编号连续、每表落在 4~8 行的硬区间、列数与空格子、一页纸声明数 = 围栏实数）、
  四段骨架在内存里过 `ast.parse` + `compile()`（少一个冒号两道都炸，模块层塞 `return` 只有后者拦得下）、
  11 章每章至少一组硬事实**当场重判**、隐私五类全 0、章目录跑前跑后清单一模一样。
  ★ 本章还诚实订正了第 10 章的一句过头话：「唯一在场的第三方库是 `typeguard`」只成立于它当场探的那几个名字，
  一份 18 个名字的名单切开后，`dist-packages` 里躺着的是 `requests`/`rich`/`setuptools`/`typeguard`/`yaml` 五个 ——
  被否掉的是「装东西」这条路（本机没有 `pip`），不是「本机一个第三方包都没有」。
- **全书 12 章已交付。** 剩下的路在这一页之外：`mypy`/`pytest` 这类工具要先在**有网的机器**上装了才谈得上用，
  本书从头到尾一条都没假设你装了它们。用法写在第 12 章末尾：挑一个你真的在跑的脚本，按骨架给它补
  `if __name__` 与退出码、按表 8 换掉 `open(p, 'w')`、按表 9 排查一次 `ModuleNotFoundError`、
  按骨架 4 把并发收尾补齐；回看顺序建议 **08 → 09 → 05 → 11 → 07**（按出事代价从大到小）。
