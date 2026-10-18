# 交互 shell、脚本、子 shell：到底谁在跑

> 这一章只干一件事：让你**亲眼看到**「你敲的那一行，是被哪个进程、用哪套语法执行的」。
> 下面每一段输出都是这台机器（bash 5.3.9 + dash 0.5.12，Ubuntu 26.04 LTS）上真实跑出来的，
> 出自一次 `bash demo.sh > /tmp/bash01.txt 2>&1`（644 行，退出码 0）。
> 唯一每次会变的是**进程号**和 `mktemp` 的沙箱路径 `/tmp/bash01-XXXXXX`，除此之外逐字一致。
> 引用格式：`$ ...` 是真敲的命令，紧跟其后是它的真实输出，最后一行 `[退出码 N]` 是 demo 打的总结。

---

## 你会遇到的场景

先对上号，这些是不是你的现场：

1. `./x.sh` 报 `Permission denied`，改成 `bash x.sh` 就好了 —— 你以为文件坏了，其实只是少 `chmod +x`。
2. 网上抄的脚本第一行是 `#!/bin/sh`，里面却用了数组，一跑就 `Syntax error: "(" unexpected`。
3. 你在脚本里赋了个变量，紧接着的子 shell 里 `echo` 它却是空的；或者反过来，
   在脚本里赋的值，`source` 之后终端里能看见、直接跑就看不见。
4. `cat file | while read` 里累加的计数器，循环一结束变成 0 —— 明明刚才还在打印 1、2、3。
5. 终端里好用的 `alias`，写进 `.sh` 里完全无效。
6. `sh x.sh` 和 `bash x.sh` 结果不一样，你以为 `sh` 就是「bash 的安静模式」。
7. 脚本跑到一半炸了，你分不清这是**语法错误**还是**运行时错误**，也不知道为什么前面几行已经执行了。

这 7 个现场，全部来自同一个心智模型的四个层次。记住它，你以后遇到任何 bash 怪事都会先问一句：
**「现在是哪一层、哪个进程？」**

---

## 心智模型：一条命令要过的四层

```text
你敲下 ./x.sh 或 bash x.sh 或 source x.sh
   │
   ├─ 层 1：谁来做「解释器」？
   │     ./x.sh        → 内核读文件第 1 行的 shebang（#!/usr/bin/env bash）
   │     bash x.sh     → 你指定了 bash，shebang 被**完全无视**
   │     sh  x.sh      → 你指定了 dash（本机 /bin/sh -> dash），shebang 同样被无视
   │     无 shebang 的 ./x.sh → 内核 execve 失败(ENOEXEC)，由**调用方** shell 兜底
   │
   ├─ 层 2：要不要 fork 新进程？
   │     bash x.sh / ./x.sh   → fork 一个子进程，环境变量「单向继承」，回不来
   │     source x.sh / . x.sh → 不 fork，逐行塞进**当前** shell 执行
   │
   ├─ 层 3：当前 shell 内部，哪些写法又开了子 shell？
   │     ( ... )          子 shell
   │     a | b            管道的**每一段**都是子 shell（默认）
   │     $( ... )         命令替换是子 shell
   │     { ...; }         只是分组，**不** fork（但放在管道右侧仍然是子 shell）
   │
   └─ 层 4：变量分两种，中间隔着一次 fork
         shell 变量（`V=1`）        → 只有当前这个 shell 看得见
         环境变量（`export V=1`）   → fork 出去的子进程才看得见；且只能往下传
```

三张对照表，比任何解释都管用 —— **下面每一格都能在 `demo.sh` 第 0/1/7 节里找到对应原文**：

| 跑法 | 是否新进程 | 脚本里 `$0` | 脚本里赋的变量，外面看得见吗 |
|---|---|---|---|
| `bash x.sh` | ✅ 新 bash 进程 | `x.sh` | ❌ 看不见 |
| `./x.sh` | ✅ 新进程（解释器由 shebang 决定） | `./x.sh` | ❌ 看不见 |
| `source x.sh` / `. x.sh` | ❌ 就在当前 shell 里 | 继承当前 shell 的 `$0`（实测是 `bash`） | ✅ 看得见 |

| bash 独有写法 | dash（本机 `/bin/sh`）的真实报错原文 |
|---|---|
| 数组 `arr=(a b)` | `dash_probe.sh: 1: Syntax error: "(" unexpected` |
| `[[ 1 == 1 ]]` | `dash: 1: [[: not found` |
| `cat <<< hi` | `dash: 1: Syntax error: redirection unexpected` |
| `$'a\nb'`（ANSI-C 引用） | 不报错，但**不展开**：拿到的值是字面 `$a\nb` |

| | 交互式 shell | 非交互（脚本） |
|---|---|---|
| 读 `~/.bashrc` / `/etc/bash.bashrc` | ✅ | ❌ 一个都不读（只认 `BASH_ENV`） |
| `alias` 条数 | 有（实测装了 rc 之后是 1 条） | **0 条** |
| `shopt expand_aliases` | `on` | `off` |
| `$-` 里有没有 `i` | 有（实测 `hiBHc`） | 没有（实测 `hB`） |

---

## 场景逐个实战

### 场景 1：先承认这台机器上有两个 shell，两套外部命令（demo 第 0 节）

```
$ bash --version | head -3
GNU bash, version 5.3.9(1)-release (x86_64-pc-linux-gnu)
Copyright (C) 2025 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
[退出码 0]

$ which bash sh
/usr/bin/bash
/usr/bin/sh
[退出码 0]

$ ls -l /bin/sh
lrwxrwxrwx 1 root root 4 Feb  3  2026 /bin/sh -> dash
[退出码 0]

$ readlink -f "$(which sh)"
/usr/bin/dash
[退出码 0]

$ dpkg -l bash dash 2>/dev/null | grep ^ii
ii  bash           5.3-2ubuntu1     amd64        GNU Bourne Again SHell
ii  dash           0.5.12-12ubuntu3 amd64        POSIX-compliant shell
```

**`/bin/sh -> dash` 这一行是本章一切麻烦的源头。** 网上大量教程写 `sh x.sh`，
作者本人的机器上 `sh` 常是 bash 的软链接，换到你这台就报错。

再看两个「看起来一样、其实不一样」的证据：

```
$ /bin/sh --version 2>&1 | head -1
/bin/sh: 0: Illegal option --
[退出码 0]

$ type -a echo
echo is a shell builtin
echo is /usr/bin/echo
echo is /bin/echo
```

- dash 不认 `--version`，bash 认。这句 `Illegal option` 本身就是「它不是 bash」的物证。
- `type -a echo` 告诉你：`echo` 同时是**内建命令**和外部程序，bash 默认用内建。
  这条规律适用于一大票你以为是程序的命令：`test`、`[`、`printf`、`read`、`cd`、`alias`。
  这不是学术问题，同一条 `echo` 三种实现的实测结果就不同：

```
$ echo "a\nb"
a\nb
[退出码 0]

$ echo -e "a\nb"
a
b
[退出码 0]

$ /usr/bin/echo "a\nb"
a\nb
[退出码 0]

$ dash -c 'echo "a\nb"'
a
b
[退出码 0]

$ printf "a\nb\n"
a
b
[退出码 0]
```

看清四件事：**bash 的内建 `echo` 默认不解释反斜杠**（要 `-e`）；**dash 的内建 `echo` 却解释**；
**外部 `/usr/bin/echo` 又是第三种行为**；只有 `printf` 在谁手里都一致。
所以手册全书的规矩是：**脚本里要换行、要转义，一律用 `printf`，不要用 `echo`。**
（很多人「本地好好的、换台机器就少一行」就是这里。）

最后确认一件很多人误解的事：

```
$ echo "登录 shell 环境变量 \$SHELL = [$SHELL]"
登录 shell 环境变量 $SHELL = [/bin/bash]
```

`$SHELL` 只是**一个环境变量**，内容是「你登录时用的是哪个 shell」。
它不代表你此刻这个进程是什么 shell —— 别用它做判断，用 `$BASH_VERSION` 是否非空，或 `$-`。

本机外部命令的实现身份也一并打出来，因为它决定后面几章能写什么：

```
$ ls --version | head -1
ls (uutils coreutils) 0.8.0
[退出码 0]

$ sed --version | head -1
sed (GNU sed) 4.9
[退出码 0]

$ awk --version | head -1
mawk 1.3.4 20260129
[退出码 0]

$ grep --version | head -1
grep (GNU grep) 3.12
```

**`ls`/`env`/`tr`/`date`/`wc`/`head` 这些是 uutils coreutils（Rust 实现），不是 GNU coreutils；
`awk` 是 mawk，不是 gawk。** 这条事实的直接后果：第 07 章只能用 POSIX/mawk 公共子集的 awk 特性
（gawk 专属的 `gensub`、`asort`、`--ri` 一概不写），第 04 章讲 `date -d`、`tr -d` 时也必须先跑一遍确认。

### 场景 2：shebang 的真实作用 —— 同一个文件体，加不加第一行（demo 第 1 节）

沙箱里用同一个文件做了两个版本（`sb_yes.sh` 保留 shebang，`sb_no.sh` 把第一行删掉）：

```
$ echo "=== sb_yes.sh 第 1 行 ==="; head -1 sb_yes.sh; echo "=== sb_no.sh 第 1 行 ==="; head -1 sb_no.sh
=== sb_yes.sh 第 1 行 ===
#!/usr/bin/env bash
=== sb_no.sh 第 1 行 ===
# ============================================================================
[退出码 0]

$ ./sb_yes.sh | head -4
  数组第 2 个元素 ${arr[1]} = beta
  [[ ]] 模式匹配生效：dash 以 d 开头
  从 <<< 里读到一行：[来自 here-string 的第一行]
  从 <<< 里读到一行：[来自 here-string 的第二行]
[退出码 0]
```

现在把同一个没有 shebang 的文件分别交给 bash 和 dash：

```
$ ./sb_no.sh | head -4
  数组第 2 个元素 ${arr[1]} = beta
  [[ ]] 模式匹配生效：dash 以 d 开头
[退出码 0]

$ sh -c "./sb_no.sh" | head -2
./sb_no.sh: 14: Syntax error: "(" unexpected
[退出码 0]
```

没有 shebang 时，`./sb_no.sh` 居然能跑 —— 因为内核 `execve` 失败（`ENOEXEC`）后，
**调用方 bash 兜底，用「自己」来解释这个文件**。从 dash 里调用同一份文件，兜底的就是 dash，立刻炸。
结论一句话：**没有 shebang，「谁调用你」决定你用哪种语法，脚本行为不可复现。**

最反直觉的一条，是「显式指定解释器时 shebang 被完全无视」：

```
$ sh sb_yes.sh | head -2
sb_yes.sh: 15: Syntax error: "(" unexpected
[退出码 0]

$ dash sb_yes.sh | head -2
sb_yes.sh: 15: Syntax error: "(" unexpected
[退出码 0]

$ bash sb_no.sh | head -2
  数组第 2 个元素 ${arr[1]} = beta
  [[ ]] 模式匹配生效：dash 以 d 开头
[退出码 0]
```

`sb_yes.sh` 明明写着 `#!/usr/bin/env bash`，但 `sh sb_yes.sh` 就是「让 dash 去读这个文件」，
shebang 一行都不作数。反过来 `bash sb_no.sh`（没 shebang）也照样跑得漂亮。
**所以：shebang 只在「让内核去 exec」时生效，也就是 `./xxx` 这种方式。**

顺带注意上面几段结尾都是 `[退出码 0]` —— 因为管道最后一段是 `head`，
`head` 成功了整条管道就报成功，dash 的退出码 2 被你吃掉了。这是第 04 章 `pipefail` 的引子。

#### bash 到底比 dash 多了什么（逐个隔离实测）

上面只看到「数组」一个失败点，那是因为 dash 一遇语法错误就中断整个文件。
把四种写法**分开**试，报错原文一目了然：

```
$ dash dash_probe.sh
dash_probe.sh: 1: Syntax error: "(" unexpected
[退出码 2]

$ bash dash_probe.sh
数组可用
[退出码 0]

$ dash -c 'if [[ 1 == 1 ]]; then echo "[[ 可用"; fi'
dash: 1: [[: not found
[退出码 0]

$ bash -c 'if [[ 1 == 1 ]]; then echo "[[ 可用"; fi'
[[ 可用
[退出码 0]

$ dash -c 'cat <<< hi'
dash: 1: Syntax error: redirection unexpected
[退出码 2]

$ bash -c 'cat <<< hi'
hi
[退出码 0]
```

`[[ 1 == 1 ]]` 在 dash 里的表现特别有迷惑性：**它不是语法错误**，dash 把 `[[` 当成一个**命令**去找，
于是报 `[[: not found`。注意上面那段的退出码是 **0** 而不是 127 —— 单独跑才拿得到 127：

```
$ dash -c '[[ 1 == 1 ]]'
dash: 1: [[: not found
[退出码 127]

$ dash -c 'if [[ 1 == 1 ]]; then echo 走了then; else echo 走了else; fi'
dash: 1: [[: not found
走了else
[退出码 0]
```

放在 `if` 条件里时，「命令不存在」只是让条件为假，脚本不炸、**永远走 else**，整条命令以 0 收工。
这类「不报错、只是结果不对」的坑比语法错误危险得多。

第四个差异是 ANSI-C 引用 `$'...'`（同一个文件，两个解释器各跑一次）：

```
$ dash ansi_probe.sh
  $'a\nb' 展开结果：[$a\nb]
[退出码 0]

$ bash ansi_probe.sh
  $'a\nb' 展开结果：[a
b]
[退出码 0]
```

dash 把 `$` 原样留下、`\n` 也不换行（值是字面 `$a\nb`）；bash 才真的展开成一个换行。

### 场景 3：忘了 `chmod +x` 时的报错原文（demo 第 1 节 ⑧）

```
$ chmod -x sb_no.sh
[退出码 0]

$ ./sb_no.sh
bash: line 1: ./sb_no.sh: Permission denied
[退出码 126]

$ bash sb_no.sh | head -2
  数组第 2 个元素 ${arr[1]} = beta
  [[ ]] 模式匹配生效：dash 以 d 开头
[退出码 0]
```

三个数字要背下来，因为它们指向完全不同的原因：

| 退出码 | 含义 | 本例 |
|---|---|---|
| **126** | 找到了，但**执行不了**（没执行权限 / 它是目录） | `./x.sh` 但没 `chmod +x` |
| **127** | **找不到这个命令** | 敲 `x.sh` 没加 `./` |
| **2** | bash 自己的**语法错误** | `broken_syntax.sh` |

`bash x.sh` 不需要执行权限 —— bash 只要**能读**这个文件就行。这解释了为什么
「`bash x.sh` 能跑、`./x.sh` 报 126」完全正常，也正是 CI 里「本地能跑、流水线报 126」的标准答案
（git 只记 `+x` 位，被人 `cp` 出来或解压丢权限就会这样）。

### 场景 4：`bash x.sh` / `./x.sh` / `source x.sh` 的进程关系（demo 第 2 节）

`pids.sh` 会报出自己的 `$0`、`$$`、`$BASHPID`、`$PPID` 并给 `SHELLVAR` 赋值。
外层 shell 也报自己的 pid，父子关系当场就清楚（数字每次跑都会变，**关系**不变）：

```
$ echo "外层 pid=$$"; bash pids.sh; echo "外层 pid=$$ 还在；外面看到 SHELLVAR=[$SHELLVAR]"
外层 pid=192811
  0(名字)      = [pids.sh]
  $$(PID)     = [192812]      $BASHPID = [192812]
  PPID(父进程) = [192811]
  BASH_VERSION = [5.3.9(1)-release]
  交互性       = 非交互（$- 不含 i）
  赋值完成     = SHELLVAR=[脚本里赋的值]
外层 pid=192811 还在；外面看到 SHELLVAR=[]
[退出码 0]
```

`bash pids.sh`：脚本 pid=192812，父进程正是外层的 192811 → **确实是新进程**，
所以脚本里赋的 `SHELLVAR` 外面**看不见**。`./pids.sh` 唯一差别是 `$0` 带上 `./`：

```
  0(名字)      = [./pids.sh]
  $$(PID)     = [192814]      $BASHPID = [192814]
  PPID(父进程) = [192813]
外层 pid=192813 还在；外面看到 SHELLVAR=[]
```

`source` 就把上面一切反转了：

```
$ echo "外层 pid=$$"; source pids.sh; echo "外层 pid=$$ 还在；外面看到 SHELLVAR=[$SHELLVAR]"
外层 pid=192815
  0(名字)      = [bash]
  $$(PID)     = [192815]      $BASHPID = [192815]
  PPID(父进程) = [192714]
  BASH_VERSION = [5.3.9(1)-release]
  交互性       = 非交互（$- 不含 i）
  赋值完成     = SHELLVAR=[脚本里赋的值]
外层 pid=192815 还在；外面看到 SHELLVAR=[脚本里赋的值]
[退出码 0]
```

看清四个地方：**`$$` 没变**（192815 = 外层）、**`$0` 变成了 `bash`**（不再是文件名）、
**`$BASHPID` 也没变**（根本没 fork）、**`SHELLVAR` 外面看得见**。
`. pids.sh` 的实测输出和 `source` 完全同构（`$0 = [bash]`、`SHELLVAR` 可见）—— POSIX 点命令，写脚本用它更保险。

副作用对比才是 source 的真正用途：

```
$ printf "cd /tmp\nGUESS=from-source\n" > cdtest.sh; echo "source 前目录=$(pwd)"; source ./cdtest.sh; echo "source 后目录=$(pwd) GUESS=[$GUESS]"
source 前目录=/tmp/bash01-yLhur0/work
source 后目录=/tmp GUESS=[from-source]
[退出码 0]

$ echo "全新的 shell 里：目录=$(pwd) GUESS=[${GUESS-（没有：上一段 source 的效果不跨进程）}]"
全新的 shell 里：目录=/tmp/bash01-yLhur0/work GUESS=[（没有：上一段 source 的效果不跨进程）]
[退出码 0]
```

`source` 里的 `cd` 真的把当前 shell 带走了 —— 这就是 `~/.bashrc` **必须**被 source 而不能被执行的
原因（rc 文件的全部工作就是「修改当前 shell」：设变量、定义函数、改开关）。
反过来，**能被 source 的脚本不能有副作用**：不能依赖 `$1`（source 时位置参数是外面的），
不能 `exit`（会把你的终端关掉）。

### 场景 5：子 shell 的三个来源，和「管道里改的变量外面看不到」（demo 第 3 节）

`subshell.sh` 的完整输出（这是本章最核心的一屏）：

```
$ bash subshell.sh
  顶层：$$=192822  $BASHPID=192822
  ① ( ) 里面：$$=192822  $BASHPID=192823   ← $$ 没变，BASHPID 变了
  ① 括号里看 INNER_VAR = [只在括号里存在]
  ① 括号外看 INNER_VAR = [（没定义：括号里赋的值出不来）]
  ② 管道右侧数到第 1 个：apple   （$BASHPID=192825）
  ② 管道右侧数到第 2 个：banana   （$BASHPID=192825）
  ② 管道右侧数到第 3 个：cherry   （$BASHPID=192825）
  ② 管道结束后外面的 COUNT = [0]   ← 坑：刚才明明加到 3 了！
  ② 改用 for 循环（没有管道）外面的 COUNT2 = [3]
  ③ 命令替换里 echo 出来的文本 RESULT = [被改过了]
  ③ 外面的 OUTER 还是 = [原来的值]   ← 赋值没传出来，只有文本传出来了
  ④ 用 { } 分组：SUM=60   $BASHPID=192828   ← 分组救不了管道右侧
  ④ 外面的 SUM = [0]
[退出码 0]
```

三件事同时被证明了：

1. **判据只能用 `$BASHPID`。** `$$` 在 `( )` 里面纹丝不动（192822 → 192822），
   因为 bash 故意让 `$$` 保持「当初那个 shell」的 pid。用 `$$` 判断子 shell 的人永远看不出问题。
2. **管道右侧是子 shell。** 三行打印的 `$BASHPID` 完全相同（192825 = 只有一个子进程），
   `COUNT` 却在外面归零 —— 它改的是自己那份副本。这是 bash 新手第一大坑。
3. **命令替换里只有「文本」能传出来。** `RESULT=[被改过了]` 是 echo 的文本；
   `OUTER=[原来的值]` 说明赋值留在子 shell 里烂掉了。

`{ }` 那段是配套的认知修正：**分组 ≠ 不 fork**。`{ ...; }` 自己不创建子 shell，
但它待在管道右侧，照样是子 shell（`SUM` 外面还是 0）。

三种真解法，demo 全跑了一遍：

```
$ n=0; printf "a\nb\nc\n" | while IFS= read -r line; do n=$((n+1)); done; echo "管道右侧累加，外面 n=[$n]"
管道右侧累加，外面 n=[0]
[退出码 0]

$ shopt -s lastpipe; m=0; printf "a\nb\nc\n" | while IFS= read -r line; do m=$((m+1)); done; echo "开了 shopt -s lastpipe，外面 m=[$m]"
开了 shopt -s lastpipe，外面 m=[3]
[退出码 0]

$ k=0; while IFS= read -r line; do k=$((k+1)); done < <(printf "a\nb\nc\n"); echo "改用进程替换喂 while（第 04 章细讲），外面 k=[$k]"
改用进程替换喂 while（第 04 章细讲），外面 k=[3]
[退出码 0]

$ printf "a\nb\nc\n" > f.txt; j=0; while IFS= read -r line; do j=$((j+1)); done < f.txt; echo "改成从文件重定向读，外面 j=[$j]"
改成从文件重定向读，外面 j=[3]
[退出码 0]
```

推荐度：**从文件/重定向读 > 进程替换 > `shopt -s lastpipe`**。
`lastpipe` 只在「作业控制关闭」时生效（非交互脚本里正好成立），一旦被人在交互式终端里跑就可能失效，
所以不要把它当唯一方案。

### 场景 6：脚本里的 `$0 $1 $# $@ $*`（demo 第 4 节）

调用方式是 `bash posargs.sh alpha "beta gamma" delta`：

```
$ bash posargs.sh alpha "beta gamma" delta
  $0 = [posargs.sh]        ← 脚本名（怎么被叫起来，它就写什么）
  $1 = [alpha]  $2 = [beta gamma]  $3 = [delta]  $4 = []   ← 第 4 个根本没传
  $# = [3]        ← 参数个数。**不含** $0！这是最多人记错的一条
  $@ = [alpha beta gamma delta]
  $* = [alpha beta gamma delta]
  "$@" 加引号，用 | 连接每个词：
    |alpha    |beta gamma    |delta|
  "$*" 加引号，是一个整词：
    |alpha beta gamma delta|
  逐个拆开看：
    $[1] = [alpha]
    $[2] = [beta gamma]
    $[3] = [delta]
```

先记住三条硬事实：**`$#` 不含 `$0`**；**`$@`/`$*` 不加引号时长得一模一样**（都散了）；
**加引号才见分晓** —— `"$@"` 是「每个参数一个词」，`"$*"` 是「全部拼成一个词」。
第 02 章会把这个差别放到真实文件名场景里炸给你看。

同一条脚本换个调用方式，`$0` 立刻变：

```
$ ./posargs.sh one two
  $0 = [./posargs.sh]        ← 脚本名（怎么被叫起来，它就写什么）
  $1 = [one]  $2 = [two]  $3 = []  $4 = []   ← 第 4 个根本没传
  $# = [2]        ← 参数个数。**不含** $0！这是最多人记错的一条
```

`set --` 和 `shift`（`posargs.sh` 最后四行，两种调用方式下输出完全相同）：

```
  ---- 下面用 set -- 把位置参数整组换掉 ----
  set -- 之后：$# = [3]
  set -- 之后：$1 = [换进去的第一个]  $2 = [第二个带 空格]  $3 = [第三个]
  shift 前：$# = [3]  $1 = [alpha]
  shift 一次后：$# = [2]  $1 = [beta gamma]
```

`set --` 就是「把位置参数整组换掉」，注意 `"第二个带 空格"` 依然是一个参数 ——
它走的是和命令行传参**完全一样的**词分割规则。`shift` 是丢掉 `$1`、全体前移、`$#` 同步减 1。

### 场景 7：新手最需要背下来的报错原文（demo 第 5 节）

```
$ bash errors.sh
  【报错 1】命令不存在
  敲的命令：no_such_program_abc --version
errors.sh: line 14: no_such_program_abc: command not found
  ← 报错原文见上一行。退出码 = [127]   ← 127 = 「命令没找到」
  脚本**没有停**，还在往下跑。这就是 bash 的默认行为。

  【报错 2】赋值写成了「等号两边带空格」（第一号新手手误）
  敲的命令：my_count = 5
errors.sh: line 21: my_count: command not found
  ← 退出码 = [127]。bash 把它读成「运行命令 my_count，参数是 = 和 5」，
    所以报的是「command not found」，**而不是**语法错误。赋值必须写 my_count=5。

  【报错 3】把脚本当命令敲，但忘了写 ./
  敲的命令：pids.sh（当前目录里的文件，没加 ./）
errors.sh: line 28: pids.sh: command not found
```

`my_count = 5` 报的是 `command not found` 而不是语法错误 —— 这条几乎每个新手都会中，
而报错信息里完全没有「赋值」这个词，所以搜都搜不到。
如果你看到的报错是「`xxx: command not found`」而 `xxx` 是你心里的变量名，就是它。

报错 3 也值得记：**bash 不在当前目录找命令**。这是安全设计（否则你在下载目录里随手敲 `ls`
就可能执行到别人放的同名脚本），代价是必须写 `./x.sh`。

最有教学价值的是最后两段：

```
  【报错 4】条件里把字符串丢给 -eq 做数值比较
errors.sh: line 36: [: abc: integer expected
  ← 报错原文见上一行，退出码 = [0]
    这里有两层教训：① [ ] 是个**命令**（type -a test 能看到它是内建），
    所以它的报错是运行时错误，脚本继续跑；② 但 if 整条语句的退出码是 **0**（假分支不执行
    也不算失败），所以「判定条件写错了」这种事**根本不会**被 set -e 抓到 —— 它只会静默走错路。

  【报错 5】set -u 下引用未定义变量（这一段会让脚本当场结束）
  已经 set -u，现在敲「echo [$UNDEFINED_AT_ALL]」 ——
errors.sh: line 48: UNDEFINED_AT_ALL: unbound variable
[退出码 1]
```

**`[ ]` 是命令，不是语法** —— 它就是 `test`（`type -a test` 显示 `test is a shell builtin`）。
既然是命令，它的失败就是「运行时」的：报错、判定为假、脚本继续。而 `if` 语句整体返回 0，
所以条件写反 / 类型写错时 `set -e` 完全不救，脚本会安静地走错分支跑到天亮。
最后一段是 `set -u` 的价值：**变量名写错 → 当场停（退出码 1），而不是带着空值继续跑。**

语法错误则是另一个世界：

```
$ bash broken_syntax.sh
  这行在报错**之前**，所以它真的打印出来了 → 证明 bash 是边解析边执行的
broken_syntax.sh: line 23: syntax error near unexpected token `fi'
broken_syntax.sh: line 23: `fi'
[退出码 2]
```

`broken_syntax.sh` 里就是 `if [ -f "$CONFIG_FILE" ]` 后面**漏写了 `then`**。
注意三件事：报错第二行会把出问题那行原样回显（`fi`），定位很快；
**报错之前的 echo 已经执行了**（bash 是边解析边执行，不是「全文检查通过才跑」）；
退出码固定是 **2**，和运行时错误区分得干干净净。

### 场景 8：shell 变量 vs 环境变量（demo 第 6 节）

```
$ bash envcheck.sh
  本层看到：PRIVATE_VAR=[只在bash里]  PUBLIC_VAR=[会被继承]
  export -p（列出所有环境变量）里能不能找到它们两个：
declare -x PUBLIC_VAR="会被继承"
  ---- 往下开一个子 bash，只看环境 ----
    子进程看到：PRIVATE_VAR=[]  PUBLIC_VAR=[会被继承]
  ---- 子进程里改 PUBLIC_VAR，能传回来吗 ----
    子进程里现在是 [子进程改的]
    父进程这边还是：PUBLIC_VAR=[会被继承]   ← 传不回来
  ---- 变量前缀写法：只给「这一条命令」加环境变量 ----
    那条命令看到：PUBLIC_VAR=[只给这一条命令]
    前缀写法之后，父进程：PUBLIC_VAR=[会被继承]   ← 也只影响那一条命令
  ---- printenv 看单个变量到底在不在环境里 ----
    printenv PRIVATE_VAR → 找不到（退出码非 0），它真的不在环境里
    printenv PUBLIC_VAR → 会被继承
```

`export -p | grep` 那一行只匹配到 `PUBLIC_VAR` —— 这就是「两个变量、两种身份」的直接物证。
方向也是**单向**的：子进程改环境变量，父进程永远看不见；
唯一的回流通道是「子进程打印 → 父进程用命令替换接住」（场景 5 来源 ③）。

`env -i` 造一个干净环境，三条反直觉的事实一起出来：

```
  ---- env -i：把环境清空，看看还剩什么 ----
    我这个 shell 的环境变量条数：85
    env -i 之后 PATH=[/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin]
    env -i 之后 PUBLIC_VAR=[]
    env -i 之后 BASH_VERSION=[5.3.9(1)-release]
    三条合起来的结论：PATH 是 bash **自己造的默认值**（不是继承来的，env -i 里根本没有 PATH），
    PUBLIC_VAR 消失（证明继承只走环境），BASH_VERSION 还在（它是 bash 启动后自己设的 shell 变量，
    不在环境里 —— 用 printenv BASH_VERSION 是查不到的）。
    现场验证：printenv BASH_VERSION → 查不到，它确实不在环境里。
[退出码 0]
```

- `env -i` 之后 `PATH` 还在：**那是 bash 自己填的默认值**，不是你继承来的。
- `BASH_VERSION` 还在但 `printenv` 查不到：**它是 bash 启动时自造的 shell 变量，不进环境**。
  所以「子进程里读不到 `$BASH_VERSION`」这类 bug，多半是你把脚本交给 `sh` 跑了。
- 这里数到 85、外面 demo 数到 84，差的正好是刚 `export` 的那个 `PUBLIC_VAR`：

```
$ echo "当前环境里的变量条数：$(env | wc -l)"
当前环境里的变量条数：84
[退出码 0]

$ env -i /usr/bin/env | wc -l
0
[退出码 0]
```

最后一行是最干净的一眼：`env -i` 之后连 `env` 自己都打印不出任何东西。

### 场景 9：`alias` 和 `shopt`：交互 shell 里有，非交互脚本里没有（demo 第 7 节）

沙箱里造一个 rc 文件（**没有碰你的 `~/.bashrc`**），里面设一个 alias 和两个开关：

```
$ printf 'alias mytestalias="echo ALIAS 生效了"\nshopt -s nullglob\nshopt -s cdable_vars\n' > myrc; cat myrc
alias mytestalias="echo ALIAS 生效了"
shopt -s nullglob
shopt -s cdable_vars
[退出码 0]
```

同一份 `rcprobe.sh`，两种打开方式，结果完全两样：

```
$ bash rcprobe.sh
  $-            = [hB]          ← 含 i 才是交互式（这里 hB / hiBHc 的差别就是它）
  $0            = [rcprobe.sh]
  当前 alias 条数 = [0]
  alias mytestalias 不存在（rc 没生效）
  shopt expand_aliases  = off
  shopt nullglob        = off
  shopt extglob         = off
  shopt cdable_vars     = off
  shopt histappend      = off
  BASH_ENV       = [（没设）]
  本机 PATH 里第一个 bash = [/usr/bin/bash]   ← shebang 写 env bash 时找到的就是它
[退出码 0]

$ bash --rcfile ./myrc -ic "source ./rcprobe.sh"
bash: cannot set terminal process group (192710): Inappropriate ioctl for device
bash: no job control in this shell
  $-            = [hiBHc]          ← 含 i 才是交互式（这里 hB / hiBHc 的差别就是它）
  $0            = [bash]
  当前 alias 条数 = [1]
  alias mytestalias 存在 → [alias mytestalias='echo ALIAS 生效了']
  shopt expand_aliases  = on
  shopt nullglob        = on
  shopt extglob         = off
  shopt cdable_vars     = on
  shopt histappend      = off
```

那两行 `cannot set terminal process group` 不是结论的一部分，是 `-i` 在没有真终端
（这里输出被接到管道）时的正常抱怨，忽略即可。

把 rc 文件的干扰排除掉（`--norc --noprofile`），「交互式 vs 非交互」本身**只**差一个开关：

```
$ diff <(bash -c "shopt | sort") <(bash --norc --noprofile -ic "shopt | sort" 2>/dev/null) || echo "（上面就是全部差别）"
23c23
< expand_aliases      	off
---
> expand_aliases      	on
（上面就是全部差别）
```

所以：**`alias` 是交互 shell 的糖**。终端里能用的 alias 写进 `.sh` 就是不生效，这不是 bug 是规则。
脚本里要复用命令就写**函数**；实在要用 alias 必须 `shopt -s expand_aliases`，而且 alias 必须定义在
使用之前（展开发生在解析期，函数体里的 alias 定义顺序会咬人）。

顺手抓到一个真坑（`shopt -p` 的退出码跟着开关状态走）：

```
$ shopt -p expand_aliases; echo "shopt -p 的退出码 = [$?]（开关明明是正常关闭的）"
shopt -u expand_aliases
shopt -p 的退出码 = [1]（开关明明是正常关闭的）
```

它**正常打印了** `shopt -u expand_aliases`，退出码却是 `1`。于是
`shopt -p x || echo "查不到"` 这种写法，会在「开关正常为 off」时也走兜底分支。
查开关状态请用 `shopt -q 名字`（只给退出码：0=on，1=off）。

### 场景 10：`bash -n` 和 `bash -x`（demo 第 8 节）

`-n` = 只解析不执行，专门抓语法错误：

```
$ bash -n pids.sh && echo "pids.sh 语法检查通过（一条命令都没执行）"
pids.sh 语法检查通过（一条命令都没执行）
[退出码 0]

$ bash -n errors.sh && echo "errors.sh 语法检查通过 —— 可它一跑就连报 5 个错：-n 管不到运行时错误"
errors.sh 语法检查通过 —— 可它一跑就连报 5 个错：-n 管不到运行时错误
[退出码 0]

$ bash -n broken_syntax.sh
broken_syntax.sh: line 23: syntax error near unexpected token `fi'
broken_syntax.sh: line 23: `fi'
[退出码 2]
```

中间那行最有价值：`errors.sh` 里全是运行时错误，`bash -n` 却给它发通行证。
所以 `-n` 只能当**语法闸门**用（提交前、CI 里跑一遍），不能当测试用。

`-x` 是「把每一行**展开之后**真正执行的命令打出来」。默认提示符 `PS4` 就是一个 `+ `：

```
$ bash -x pids.sh 2>&1 | head -14
+ echo '  0(名字)      = [pids.sh]'
  0(名字)      = [pids.sh]
+ echo '  $$(PID)     = [192896]      $BASHPID = [192896]'
  $$(PID)     = [192896]      $BASHPID = [192896]
+ echo '  PPID(父进程) = [192895]'
  PPID(父进程) = [192895]
+ case "$-" in
+ echo '  交互性       = 非交互（$- 不含 i）'
  交互性       = 非交互（$- 不含 i）
+ SHELLVAR=脚本里赋的值
+ EXPORTEDVAR=脚本里导出的值
+ export EXPORTEDVAR
```

读法：`+` 后面**不是你写的那一行**，而是 bash 真正执行的形态 —— 变量已替换、通配已展开、引号已剥掉。
`echo` 的参数被单引号整体包住（`'  0(名字)      = [pids.sh]'`），那是 bash 在告诉你
「这一整块是**一个词**，里面的空格不算分隔」。缩进相同、没有 `+` 的那些行才是命令自己的 stdout
（跟踪走 stderr，所以必须 `2>&1` 才会混在一起）。

自定义 `PS4` 之后能带上行号和函数名（`trace_demo.sh` 开头设的
`PS4='+ [L$LINENO ${FUNCNAME[0]:-顶层}] '`）：

```
$ bash -x trace_demo.sh 2>&1 | head -26
+ PS4='+ [L$LINENO ${FUNCNAME[0]:-顶层}] '
+ [L25 顶层] total=0
+ [L34 顶层] for round in 1 2 3
+ [L35 顶层] add_up 1
+ [L29 add_up] local step=1
+ [L30 add_up] total=1
+ [L31 add_up] echo '  第 1 轮：加上 1，total=1'
  第 1 轮：加上 1，total=1
+ [L41 顶层] missing_program_xyz --help
trace_demo.sh: line 41: missing_program_xyz: command not found
+ [L43 顶层] echo '  最后一条命令的退出码 = [127]'
  最后一条命令的退出码 = [127]
+ [L46 顶层] label='带 空格 和'\''引号'
+ [L47 顶层] echo '  打印 label：[带 空格 和'\''引号]'
```

最后一行值得停一下：`label='带 空格 和'\''引号'` 这种看着难受的写法，
正是 bash 在**忠实地**表示「值里有一个单引号」。这类「中文 + 空格 + 引号」的 bug，
只看 `-x` 一眼就能定位；不看 `-x` 你会怀疑人生。

关于 `+` 的个数，本机实测的真相和很多教程写的不一样：

```
$ set -x; v=$(printf hi); echo "v=$v"
++ printf hi
+ v=hi
+ echo v=hi
v=hi
[退出码 0]

$ set -x; f() { local a=1; echo "$a"; }; f; ( g=2; echo "$g" ); { h=3; echo "$h"; }
+ f
+ local a=1
+ echo 1
1
+ g=2
+ echo 2
2
+ h=3
+ echo 3
3
```

**函数调用、`( )`、`{ }` 都只有一个 `+`；只有命令替换 `$( )` 变成 `++`。**
所以别拿「+ 的个数」判断有没有进函数 —— 它判断的是**有没有真的 fork**。

---

## 命令速查表

| 我想…… | 命令 | 备注 |
|---|---|---|
| 看 `/bin/sh` 是谁 | `ls -l /bin/sh` | 本机 `-> dash`，这是 sh/bash 之争的终点 |
| 看命令是真程序还是内建 | `type -a echo` | `test`、`[`、`read`、`printf`、`cd`、`alias` 都是内建 |
| 用 bash 跑脚本（最稳） | `bash x.sh` | 别写 `sh x.sh` 来「试」bash 脚本 |
| 让 `./x.sh` 能跑 | `chmod +x x.sh` + 写对 shebang | 缺一个就是 126 / 用错解释器 |
| 把脚本内容装进当前 shell | `source x.sh`（或 `. x.sh`） | rc 文件、公共函数库都用它 |
| 判当前进程真实 pid | `echo $BASHPID` | `$$` 在子 shell 里不变，别用它判 fork |
| 判交互 / 非交互 | `case $- in *i*) ... ;; esac` | 非交互实测 `$- = hB` |
| 看某个 shopt 开关 | `shopt -q nullglob && echo on` | 别用 `shopt -p x \|\| ...`，它的退出码会骗你 |
| 看所有环境变量 | `export -p` | 只列导出的；`set` 会连 shell 变量一起列 |
| 单个变量在不在环境里 | `printenv NAME` | 退出码非 0 = 不在 |
| 干净环境跑一次 | `env -i /bin/bash -c '...'` | 注意 PATH 会被 bash 自己补上 |
| 只查语法不执行 | `bash -n x.sh` | 退出码 2 = 语法错误；运行时错误它不管 |
| 跟踪执行全过程 | `bash -x x.sh` | 跟踪走 stderr，`2>&1` 才和 stdout 混在一起 |
| 让跟踪带行号函数名 | `PS4='+ [L$LINENO ${FUNCNAME[0]:-顶层}] '` | 写进脚本开头，别写进 `~/.bashrc` |
| 把管道最后一段留在本 shell | `shopt -s lastpipe` | 只在作业控制关闭（非交互）时生效 |
| 换掉位置参数整组 | `set -- a "b c" d` | 规则与命令行传参完全相同 |
| 丢掉第一个参数 | `shift` | `$#` 同步减 1 |
| 要转义/换行 | `printf 'a\nb\n'` | 别用 `echo`：bash 要 `-e`、dash 不用、uutils 又不一样 |

## 这章的坑

1. **`sh x.sh` 不是「bash 的安静模式」。** 本机 `/bin/sh -> dash`：数组 `arr=(a b)` 和 `<<<` 是
   **语法错误**（退出码 2），`[[ ]]` 是**命令找不到**（127，放进 `if` 就被吃掉变 0），
   `$'..'` **不报错但静默不展开**。四种坏法都不一样，别只记「会炸」。
   要么 shebang 写 `#!/usr/bin/env bash` 并用 `./x.sh`，要么显式 `bash x.sh`。
2. **显式指定解释器时 shebang 无效。** `sh x.sh` 会**无视**文件里 `#!/usr/bin/env bash` 那一行。
3. **没有 shebang 也能跑**，但解释器由调用方兜底决定，换个 shell / 换台机器就变卦。永远写第一行。
4. **`[[ ]]` 在 dash 里不报语法错**，它报 `[[: not found` 然后安静地走 else —— 比语法错误危险得多。
   更阴的是：单独跑退出码 127，放进 `if` 条件里整条命令退出码变 **0**，CI 一律判成功。
5. **126 和 127 不是一回事**：126 = 找到了执行不了（多半没 `+x`），127 = 找不到命令（多半忘了 `./`）。
6. **`$$` 在子 shell 里不变。** 判 fork 只能用 `$BASHPID`。
7. **管道每一段都是子 shell**，里面 `n=$((n+1))` 外面全丢；`{ }` 分组救不了它。
8. **`bash -x` 里 `++` 只来自真的 fork（命令替换）**，函数调用不加深 —— 网上老教程的说法本机不成立。
9. **`alias` 和 rc 文件在非交互脚本里根本不加载。** 脚本要复用逻辑就写函数。
10. **`shopt -p NAME` 的退出码跟着开关走**，开关 off 时返回 1（哪怕输出完全正常）。查状态用 `shopt -q`。
11. **`my_count = 5` 报的是 command not found**（127），不是语法错误 —— 报错里一个「赋值」都没有。
12. **语法错误之前的代码已经执行了。** bash 边解析边执行，所以「跑到第 23 行才报语法错」意味着
    前面 22 行的写盘操作可能已经生效。上线前的脚本一律先 `bash -n`。
13. **`[ ]` 报错不代表 `if` 失败。** `[: abc: integer expected` 之后 `if` 语句退出码仍是 0，
    `set -e` 抓不到写错的判定 —— 它只会静默走错分支。
14. **别用 `echo` 打转义。** bash 内建默认不解释、dash 内建解释、uutils `/usr/bin/echo` 又不解释；
    要可复现就用 `printf`。
15. **别用 `$SHELL` 判断当前 shell**，它只是登录时的环境变量；用 `$BASH_VERSION` 是否非空，或看 `$-`。
16. **`source` 的脚本不能有副作用**：`$1` 是外面的参数，`exit` 会关掉你的终端。

## 自检问题

答不上就回到对应场景：

1. `bash x.sh`、`./x.sh`、`source x.sh` 三种跑法，脚本里 `$0` 分别是什么？哪个不 fork？（场景 4）
2. 为什么 `sh x.sh` 会无视 `#!/usr/bin/env bash`？什么情况下 shebang 才起作用？（场景 2）
3. `./x.sh` 报 126、`x.sh` 报 127，各是为什么？为什么 `bash x.sh` 两个都不会报？（场景 3）
4. 管道右侧 `while` 里累加的变量，外面为什么是 0？给出三种解法并说明各自限制；
   `lastpipe` 在什么情况下会失效？（场景 5）
5. `$#` 含不含 `$0`？`"$@"` 和 `"$*"` 的区别什么时候才显现？（场景 6）
6. `bash -n` 能抓出「命令不存在」吗？能抓出「少了 `then`」吗？（场景 10）
7. 你终端里定义的 alias 为什么在脚本里失效？脚本里想要相似能力的正解是什么？（场景 9）
8. 子进程里改了环境变量，父进程怎么拿到值？（场景 8 —— 只有一条路：让它打印出来，用命令替换接住）
9. 同一个脚本，`bash -x` 里某一行前面出现两个 `+` 说明什么？（场景 10）

## 怎么跑这章的 demo

```bash
cd bash-handbook/01-shell-vs-script
bash demo.sh              # 全程在 mktemp 沙箱里跑，跑完自动删
bash demo.sh --keep       # 保留沙箱并打印路径，进去自己接着敲
bash demo.sh > out.txt    # 重定向时自动关掉颜色，得到和本章一模一样的纯文本
```

`demo.sh` 的几条设计说明：

- **故意不加 `set -e`**（只 `set -u`）。本章要的就是「报错原文 + 脚本继续往下跑」的现场；
  加了 `-e` 会在第一个错误处退出，你反而看不见「bash 默认不会停」这条最重要的事实。
  第 08 章再讲为什么生产脚本必须加回来。
- **`export LC_ALL=C.UTF-8`**：这台机器默认 `LANG=zh_CN.UTF-8`，bash 报错会是中文（`未找到命令`），
  README 就无法逐行核对。改动只发生在本脚本及其子进程，**不动你的任何配置文件**。
- **只在 `[ -t 1 ]`（输出接终端）时上色**，重定向到文件就是干净纯文本。
- **只用 `mktemp -d` 沙箱**：`chmod`、`cp`、生成 `sb_yes.sh`/`sb_no.sh`/`myrc`/`dash_probe.sh`/`ansi_probe.sh`
  全在沙箱里；手册目录里那几个 `.sh` 一个字节都不会被改。`rm -rf` 只作用于刚创建的那一个沙箱目录。
- **不改 `~/.bashrc`、`~/.profile`、`~/.config`、`/etc`**：需要 rc 效果时，沙箱里写 `myrc`，
  用 `bash --rcfile ./myrc -ic` 演示（场景 9）。
- **不起后台进程、不 kill 任何进程、不监听端口、不联网、不装包**：所以缺的工具就如实打印缺
  （`shellcheck: 本机没有（第 09 章给出安装途径，本次没有安装）`）。
- 退出码分布（79 段真实执行，本次运行的非 0 段逐个对得上命令）：
  `2` × 4 —— `dash dash_probe.sh`、`dash -c 'cat <<< hi'`（dash 语法错误）、
  `bash broken_syntax.sh`、`bash -n broken_syntax.sh`；
  `126` × 1 —— `./sb_no.sh` 没有执行权限；`127` × 1 —— `dash -c '[[ 1 == 1 ]]'`（`[[` 被当成命令）；
  `1` × 1 —— `bash errors.sh` 里 `set -u` 命中未定义变量；其余 72 段为 0，脚本本身最终退出码 0。

## 下一步

- **第 02 章《切词与展开》**：本章你已经看见 `"$@"` 和 `"$*"` 输出不一样、`$'..'` 只在 bash 里展开、
  `echo` 在不同实现下行为不同。下一章把「一行命令被切成几段」讲透：三种引号、`${}` 全家桶、
  `set -u`、通配与 `nullglob`、IFS。这是 bash 真正的语法层，也是 90%「莫名其妙少了一个词」的根因。
- 想把「子 shell」这条线一次走完：直接看第 04 章（管道与重定向），那里的 `pipefail`
  就是本章「`sh x.sh | head -2` 明明失败却报退出码 0」的正式解法。
