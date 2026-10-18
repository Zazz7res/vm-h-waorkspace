# 管道与重定向：`|`、`>`、`2>&1`、`>(…)` —— 给进程接线

> 这一章只干一件事：让你**亲眼看到**「数据在谁和谁之间流、哪一个瞬间被改了指向、
> 以及失败的那一段的退出码最后藏到哪里去了」。
> 下面每一段输出都是这台机器（bash 5.3.9(1)-release，coreutils 是 **uutils Rust 版 0.8.0**，
> `xargs`/`find` 是 GNU findutils 4.10.0，`sed` 4.9 / `grep` 3.12，`stdbuf` 有，`shellcheck` 没有）上
> 真实跑出来的，出自同一次 `bash demo.sh 2>&1 | tee ~/.cache/handbook-captures/bash04.txt`
> （1178 行，退出码 0，26 节）。
> 每次重跑只有这几类值会变：**进程号、毫秒数、自动分配的 fd 编号、时间戳、监听端口表**；
> 其余文字逐字一致。引用它们时请读**量级和形状**，别读小数位。

---

## 你会遇到的场景

先对上号。下面每一条都在本章里真跑过，而且**都给出了原文**：

1. `cat 文件 | grep 关键字 | wc -l`，中间 `grep` 没匹配到你完全不知道 —— `$?` 永远是 `wc` 的 `0`。
2. `while read` 循环里累加的计数器，循环一结束变成空/0。你以为循环没跑，其实它跑了，只是**跑在另一个进程里**。
3. `shopt -s lastpipe` 在你终端里加了等于没加。
4. `cmd >log.txt 2>&1` 和 `cmd 2>&1 >log.txt`：前者日志齐全，后者报错仍然在屏幕上刷，文件里只有一半。
5. `>&-` 关掉 1 号 fd 之后，同一个脚本里**后面所有** `echo` 一起哑掉，而且没法恢复。
6. `exec >log.txt` 忘了改回来，脚本后半段一个字都没打出来，你以为它没跑。
7. `2>/dev/null` 加上去之后，故障照样发生，只是**原因消失了**。
8. `tee /dev/stdout` 想「顺便再打一遍」，结果下游数到 **2 行**，其实你只喂了 1 行。
9. `cat 文件 | xargs rm` 删不掉带空格的名字，反而去删了几个**根本不存在**的片段；
   如果那个片段恰好撞上真实文件，就是无声误删。
10. `yes | head -1` 之后 `$?` 是 0，但 `PIPESTATUS` 里躺着 **141**。
11. 脚本开了 `set -euo pipefail`，`... | head -1` 这种天天在用的写法把整个脚本杀了。
12. `tail -f 日志 | grep xx` 完全不实时，你以为 grep 慢。
13. `diff a b` 想比「两个命令的结果」，于是写了两个临时文件；其实 `diff <(cmd1) <(cmd2)` 一句。
14. `wc -c <(echo hi)` 打出 `/dev/fd/63`，你以为那是个临时文件，其实是**匿名管道**。
15. `read x < <(...)` 写成 `read x <(...)` → `not a valid identifier`。
16. 手滑 `echo 一行 > 重要文件`：`set -C`（noclobber）能拦，但拦不住 `tee`。

---

## 一句话版本

> **管道 = 每条命令各自一个进程，中间接一根内核里有容量上限的缓冲区；
> 重定向 = 在命令启动**之前**改它的 fd 指向；
> `|` 的水从左往右流，同一行里的多个重定向**按你书写的先后顺序逐个生效**。**

所以本章所有问题都只有三个问法：

1. **「这一段跑在哪个进程里？」** —— 变量还在不在，全看这个。（第 3、4、24 节）
2. **「此刻这个 fd 指向谁？」** —— `2>&1` 复制的是**那一刻**的指向，不是永久绑定。（第 7~11、23 节）
3. **「每段各自的退出码去哪了？」** —— 默认只留最后一段，其余藏在 `PIPESTATUS` 里。（第 5、6、17~19 节）

---

## 引用约定

- **带 `$ ` 开头的代码块 = 本机实跑原文**，一字不改地抄自那一次捕获；块里最后一行
  `[退出码 N]` 是 demo 自己打的总结，不是 shell 的输出。
- **不带 `$ ` 的代码块 = 讲解用的片段**（模型图、口诀、你在别处会敲的写法），没在这一章跑过。
- demo 里所有会写的操作都发生在 `mktemp -d "${TMPDIR:-/tmp}/bash04-XXXXXX"` 里，
  退出时 `trap cleanup EXIT` 只删自己建的那一个；章目录里**只有源文件**（第 26 节用 `find` 自证）。
- 本章**不碰**你的 `~/.bashrc`、`~/.profile`、`/etc`，不改任何已有文件，不装任何包，
  **不起任何监听端口**（唯一的后台进程是几条 `sleep`，都在同一条命令里 `kill`/`wait` 收尾）。
- 报错文本统一在 `LC_ALL=C.UTF-8` + `LANGUAGE=C` 下取证 —— 为什么必须钉死，第 1 节有现场对比。
- `cat`/`ls`/`rm` 的报错措辞来自 **uutils coreutils**，和 GNU coreutils 不一样；
  本章引的每条外部命令报错都在本机现查过来源（第 2 节）。
- 本章**大量使用 `echo`**（不像前面几章统一用 `printf`）：因为要复现的就是网上常见的 `echo ... | ...` 写法，
  而且这里只用 bash 内建那一个 `echo`。
- **PID、毫秒、fd 编号**这三类数字每次跑都会变：围栏里的是捕获那一刻的真值，
  结论请读「谁比谁大、差几个数量级、哪个非零」。

---

## 心智模型：管道与重定向动的到底是什么（三层）

```text
第一层：进程
  A | B | C   →  bash fork 出三个进程，它们**都是子进程**，bash 自己在上面看着。
                 子进程拿到的是父进程内存的**复印件**：写复印件改不到原件。
                 → 所以「管道里改的变量」在管道外面看不见（第 3 节），
                   「子 shell 里改的变量」也在外面看不见（第 24 节）。
                 认进程只能用 $BASHPID；$$ 是继承来的字符串，不会更新（第 3 节实测）。

第二层：文件描述符（fd）
  0 = stdin（读）   1 = stdout（写）   2 = stderr（写）   3+ = 你自己接的私有水管
  每个进程有自己的一张 fd 表，表里每一格指向一个「打开的文件」（终端 / 磁盘文件 / 管道 / 设备）。

第三层：时机（本章最值钱的一层）
  重定向发生在**命令启动之前**，而且同一行里从左到右**逐个**生效：
      cmd >f 2>&1   →  先把 1 指向 f；再把 2 复制成「1 此刻的指向」= f。两个都进文件。
      cmd 2>&1 >f   →  先把 2 复制成「1 此刻的指向」= 屏幕；再改 1 指向 f。stderr 留在屏幕上。
  `n>&m` 是**复制指向**（dup），不是「建立一条永久联系」。这一条讲清楚了，第 7~10 节就不用背。

管道本身：一根有容量上限的内核缓冲区（不是无限大的传送带）
  写满 → 上游被堵住（阻塞，第 22 节实测「小数据瞬间写完 / 12 万行写到一半等下游」）
  读端已经关了还在写 → 内核发 SIGPIPE，默认动作终止进程 → 退出码 141 = 128+13（第 17 节）
```

---

## 本章地图

| 你要解决的问题 | 本章哪一节 |
| --- | --- |
| 这台机器的 bash / 外部命令各是谁家的，报错文本能不能对上 | 第 1、2 节 |
| 管道里改的变量为什么丢；三种修法（左边、`lastpipe`、同一段 `{ }`） | 第 3、4 节 |
| `$?` 为什么只看最后一段；`PIPESTATUS` 怎么读、为什么必须紧跟 | 第 5 节 |
| `set -o pipefail` 改的是哪一个数；和 `set -e` 配合的账单 | 第 6 节 |
| `>f 2>&1` vs `2>&1 >f`；`&>`、`>&2`、`1>&2`、两个流对调 | 第 7、8 节 |
| 关掉 fd：`>&-` 之后那三种各不相同的 `Bad file descriptor` 原文 | 第 9 节 |
| `exec` 不带命令 = 给这个 shell 换水管，而且是持久的 | 第 10 节 |
| `/dev/null` 藏掉了什么、没藏掉什么 | 第 11 节 |
| here-doc 四种写法与「终止符多一个空格」的事故 | 第 12 节 |
| `tee` 的双写；`tee /dev/stdout` 为什么会重复 | 第 13 节 |
| UUOC 的真实代价（用管道段数量化）；`cat` 什么时候不是废物 | 第 14 节 |
| `xargs` 分词：空格名、换行名、`-print0 \| xargs -0`、沙箱里真删对照 | 第 15、16 节 |
| SIGPIPE 与 141；谁会砍上游、谁不会 | 第 17 节 |
| 后台进程、`$!`、`wait`、`trap EXIT`；143/130 同一套记账 | 第 18 节 |
| `set -euo pipefail` 撞上 `head` 的假失败与三种解法 | 第 19 节 |
| 进程替换 `<( )` 给的是文件名；`< <( )` 才是 stdin；`/dev/fd/N` 是管道不是文件 | 第 20 节 |
| `diff <(sort a) <(sort b)`、`paste`；`>( )` 的异步坑 | 第 21 节 |
| 管道缓冲：块缓冲 vs 行缓冲、`stdbuf`、`sed -u`、`grep --line-buffered`、容量上限 | 第 22 节 |
| 私有 fd：`exec 3<`、`read -u 3`、`exec {fd}>` 让 bash 自己挑编号 | 第 23 节 |
| `{ }` 分组重定向 vs `( )` 子 shell 重定向 | 第 24 节 |
| `set -C`（noclobber）：`>` 拒绝覆盖、`>\|` 强绕、`>>` 与 `tee` 不受管 | 第 25 节 |
| 跑完没在仓库里留产物 | 第 26 节 |

**读法建议**：第 3~6 节是一条线（进程与退出码），第 7~11 节是一条线（fd 与指向），
第 12~16 节是「喂输入」与「传文件名」，第 17~19 节是信号与退出码记账，
第 20~23 节是 bash 的高级接口，第 24~25 节是收口。
赶时间只读：第 3、5、7、11、16、17、19、22 节 + 文末「这章的坑」和「一页纸」。

---

## 1. 先把这台机器的发言权确认一遍

本章大量依赖 bash **自己的**机制（`PIPESTATUS`、`lastpipe`、进程替换、自动 fd 分配），
它们全是版本相关的；而报错文本还跟**语言环境**有关。所以先把这两个变量钉死。

```
$ bash --version | head -1
GNU bash, version 5.3.9(1)-release (x86_64-pc-linux-gnu)
[退出码 0]
```

```
$ echo "demo 已把自己改成：LC_ALL=[$LC_ALL] LANG=[$LANG]（你原来的终端是 LANG=$ORIG_LANG，LC_ALL=$ORIG_LC_ALL）"
demo 已把自己改成：LC_ALL=[C.UTF-8] LANG=[C.UTF-8]（你原来的终端是 LANG=，LC_ALL=）
[退出码 0]
```

这一步不是走过场：本章有两条结论**长得一模一样但内容不同**，全看语言环境。
`cat` 找不到文件时报的是 `No such file or directory`（英文）还是「没有那个文件或目录」（中文）；
bash 内建命令写坏 fd 时报的是 `Bad file descriptor` 还是它的中文译名。
本章取证一律钉死在 `LC_ALL=C.UTF-8` + `LANGUAGE=C`，这样 README 里每一行报错原文都能被 `grep -qF` 核对。

```
$ nosuchcommand04
bash: line 1: nosuchcommand04: command not found
[退出码 127]
```

↑ 同一条命令，在中文终端里报「未找到命令」，在这里报 `command not found`。
**换 LANG 就等于换一本报错词典**，这是很多「照着教程敲却核对不上」的根源。

```
$ ls
a.txt
b.txt
demo.sh
execsave.sh
heredoc.sh
herestring.sh
keepme.txt
lines3.txt
mkweird.sh
slowpipe.sh
uuoc.txt
[退出码 0]
```

★ **带走第一条**：bash 的语义 + 外部命令的报错文本 = **两个独立的版本相关变量**。
换台机器、换个 `LANG`，本章某些字符串就得重新取一遍证。

---

## 2. 本章用到的外部命令都是谁家的

管道的**语义**在 bash 手里，但管道里跑的是**外部命令**；它们的报错措辞、选项集合各不相同，
所以每章第一步都是「先探一次再用」。

```
$ echo "外部命令的来路（本机 coreutils 是 uutils 那套，报错措辞和 GNU 版不同）：" && ls --version | head -1 && wc --version | head -1
外部命令的来路（本机 coreutils 是 uutils 那套，报错措辞和 GNU 版不同）：
ls (uutils coreutils) 0.8.0
wc (uutils coreutils) 0.8.0
[退出码 0]
```

本机 coreutils 是 **uutils（Rust 实现）**那一套，不是 GNU 那一套。
这条差别在本章会露面两次：`ls`/`wc`/`cat` 的报错文本、`stdbuf` 的行为细节都以本机实测为准。

```
$ echo "xargs 来自：$(xargs --version | head -1)"; echo "find 来自：$(find --version | head -1)"
xargs 来自：xargs (GNU findutils) 4.10.0
find 来自：find (GNU findutils) 4.10.0
[退出码 0]
```

```
$ echo "sed 来自：$(sed --version | head -1)"; echo "grep 来自：$(grep --version | head -1)"
sed 来自：sed (GNU sed) 4.9
grep 来自：grep (GNU grep) 3.12
[退出码 0]
```

`find` 和 `xargs` 是 **GNU findutils 4.10.0**，所以 `-print0`、`-0`、`-d` 全都有（第 15~16 节靠它们）。
换成 BSD（macOS）那一套：`xargs` 没有 `-d`，脚本直接翻脸 —— 这一点在正文里标出来，不在本机验证。

```
$ for t in cat tee yes head tail xargs find diff sort stdbuf tr shellcheck; do printf "  %-11s %s\n" "$t" "$(command -v "$t" || echo 本机没有)"; done
  cat         /usr/bin/cat
  tee         /usr/bin/tee
  yes         /usr/bin/yes
  head        /usr/bin/head
  tail        /usr/bin/tail
  xargs       /usr/bin/xargs
  find        /usr/bin/find
  diff        /usr/bin/diff
  sort        /usr/bin/sort
  stdbuf      /usr/bin/stdbuf
  tr          /usr/bin/tr
  shellcheck  本机没有
[退出码 0]
```

↑ `shellcheck` 本机**没有**（第 09 章才用它），本章只做实跑取证；`stdbuf` 有，第 22 节的缓冲实验全靠它。

★ **带走第二条**：`|` 左边的 bash 只是**接线员**，真正干活的是这些外部命令。
「管道怎么表现」问 bash，「某一段为什么这样报错」问**那一段自己的实现**。

---

## 3. 管道两侧是两个进程：赋值为什么会丢

一句话模型：`A | B` 是 fork 出两个进程、中间接一根内核管道，bash 自己在它们上面。
所以「在管道里改变量」改的是那个子进程的记忆，父进程一无所知 ——
**这不是 bash 的 bug，是 fork 的物理后果**：子进程拿到的是父进程内存的复印件，写复印件不会改原件。

bash 里有两个号码可以问，先把「真的是两个进程」摆出来：

```text
$$        = 当前脚本/交互 shell 启动时那个进程的 pid（fork 之后**不会**更新，是继承来的字符串）
$BASHPID  = 当前正在执行的那段代码**所属进程**的 pid（真的会变）
```

```
$ echo "这个 bash -c 自己：\$\$=[$$]  \$BASHPID=[$BASHPID]"
这个 bash -c 自己：$$=[11317]  $BASHPID=[11317]
[退出码 0]
```

```
$ echo x | { echo "管道右侧：\$\$=[$$]（没变，是从父进程继承的字符串）  \$BASHPID=[$BASHPID]（变了 = 真的换了一个进程）"; echo "     它从管道里读到了 $(cat)"; }
管道右侧：$$=[11318]（没变，是从父进程继承的字符串）  $BASHPID=[11320]（变了 = 真的换了一个进程）
     它从管道里读到了 x
[退出码 0]
```

```
$ echo "外层 bash -c 自己 BASHPID=[$BASHPID]"; { echo "第一段 BASHPID=[$BASHPID]"; } | { cat; echo "第二段 BASHPID=[$BASHPID]"; } | { cat; echo "第三段 BASHPID=[$BASHPID]"; }
外层 bash -c 自己 BASHPID=[11322]
第一段 BASHPID=[11323]
第二段 BASHPID=[11324]
第三段 BASHPID=[11325]
[退出码 0]
```

↑ 一次管道里**四个不同的进程号**：外层 `bash -c` 加上三段，每段都是独立进程（PID 每次跑都会变，
读的是「它们互不相同」这件事）。而它们打印的 `$$` 全一样 —— 那是继承来的字符串，
所以**认进程只能用 `$BASHPID`**。顺带看见：管道右侧那句 `$(cat)` 读到的正是左边的 `x`，
水确实是走管道倒过来的。

现在把「复印件」这件事变成看得见的赋值丢失 —— 两条最经典的翻车写法：

```
$ v=7; echo x | v=9; echo "① 管道右侧把 v 改成 9，左侧看到的 v=[$v]（还是 7）"; echo x | read w; echo "② 管道右侧 read 到的 w=[$w]（空字符串）"
① 管道右侧把 v 改成 9，左侧看到的 v=[7]（还是 7）
② 管道右侧 read 到的 w=[]（空字符串）
[退出码 0]
```

两条共同点：右侧那一段跑完就退出，它的 `v=9` / `w=x` 随进程一起没了。
`echo x | read w` 更狠：**`w` 连「曾经是什么」都没被外面的 shell 看见过**。
这就是「`while read` 循环里累加的计数器，循环结束后变成 0/空」的同一条病根（下一节治它）。

★ **带走第三条**：管道里每一段都是独立进程；想在管道之后接着用变量，**必须换写法**。

---

## 4. 把水接回来的三种写法：改左边、`lastpipe`、整段搬进同一进程

**改法①**：把「要留下来的东西」放进命令替换，让结果从左边的门进来。

```
$ v=""; v=$(echo 来自命令替换); echo "改法①：v=[$v]"; x=outer; y=$(x=inner; echo "$x"); echo "  附赠对照：命令替换里的赋值同样出不来（里面 x=[$y]，外面 x=[$x]）"
改法①：v=[来自命令替换]
  附赠对照：命令替换里的赋值同样出不来（里面 x=[inner]，外面 x=[outer]）
[退出码 0]
```

↑ 注意后半句：命令替换**也是子进程**，所以「把结果递出来」靠的是 stdout，不是变量。

**改法②**：`shopt -s lastpipe` —— 让**最后一段**回到当前 shell 里跑，不再另起进程。

```
$ echo "bash -c 起的这个进程的开关串 \$-=[$-]（没有 i → 非交互 → lastpipe 才有资格生效）"; v=""; shopt -s lastpipe; echo x | read v; echo "改法②（开着 lastpipe）：v=[$v]"; shopt -u lastpipe; echo x | read v; echo "  同一句关掉 lastpipe 再来：v=[$v]"
bash -c 起的这个进程的开关串 $-=[hBc]（没有 i → 非交互 → lastpipe 才有资格生效）
改法②（开着 lastpipe）：v=[x]
  同一句关掉 lastpipe 再来：v=[x]
[退出码 0]
```

`lastpipe` 有两个前提，缺一个就「加了等于没加」：

1. 这个 shell 必须**没有 job control** —— 交互终端里默认不生效，所以它**只在脚本里有用**。
   上面第一行打印的 `$-` = `hBc`，**没有 `i`**，正是「非交互」的凭据。
2. 只有**最后一段**回到当前 shell，前面的段依旧是独立进程 —— 在中间段累加的计数器照样丢。

**改法③**：把「循环」和「用结果的那句话」写进**同一对 `{ }`**，让它俩待在同一个进程里。

```
$ total=""; printf "a\nb\n" | { while IFS= read -r line; do total="$total$line"; done; echo "改法③：total=[$total]（大括号内赋值，大括号内就用掉了）"; }
改法③：total=[ab]（大括号内赋值，大括号内就用掉了）
[退出码 0]
```

```
$ printf "a\nb\nc\n" | while IFS= read -r line; do n=$((n+1)); done; echo "对照（循环和 echo 分家）：循环数到 3，管道外面 n=[$n]"; shopt -s lastpipe; n=0; printf "a\nb\nc\n" | while IFS= read -r line; do n=$((n+1)); done; echo "同一条写法开了 lastpipe：n=[$n]"
对照（循环和 echo 分家）：循环数到 3，管道外面 n=[]
同一条写法开了 lastpipe：n=[3]
[退出码 0]
```

↑ 这四行是本章最实用的一格：同一条 `while read` 循环，
循环和 `echo` 分家时 `n` 是**空字符串**（注意不是 0 —— 那个 `n` 从来没被赋过值，
累加发生在本 shell 看不见的进程里）；开了 `lastpipe` 立刻是 `3`。

★ **带走第四条（选型顺序，别记反）**：
能放左边就放左边（改法①最稳）→ 脚本里、只需要最后一段的结果用 `lastpipe`（改法②）→
结构复杂、想把状态圈在一小块里就同一段 `{ }`（改法③）。
**永远不要指望「管道外面那个变量」自己活过来。**

---

## 5. `PIPESTATUS`：整条管道的 `$?` 只看最后一段

造一条**中间真的会失败**的管道：`cat` 一个不存在的文件，后面接 `wc -l`。

```
$ cat nosuchfile404.txt | wc -l
cat: nosuchfile404.txt: No such file or directory
0
[退出码 0]
```

↑ 报错是 `cat` 的，退出码却是 `0` —— 因为 `wc` 成功了。**这是本章最重要的一条反直觉。**
bash 给了一条数组来记录「每段各自的退出码」：`PIPESTATUS`。

> **必须先读的一条规矩**：这些值只能在**紧跟着的那一条命令里**读。
> 中间只要多插一条命令（哪怕就是一个 `echo`），`PIPESTATUS` 就被那条命令重置成一位数组。
> 下面每个例子都只 `echo` 一次，就是在守这条。

```
$ cat nosuchfile404.txt | wc -l; echo "↑ 每段各自的退出码 PIPESTATUS=[${PIPESTATUS[@]}]，段数=${#PIPESTATUS[@]}，整条管道的 \$?=[$?]"
cat: nosuchfile404.txt: No such file or directory
0
↑ 每段各自的退出码 PIPESTATUS=[1 0]，段数=2，整条管道的 $?=[0]
[退出码 0]
```

↑ 数组里两个数：`cat` 退 `1`、`wc` 退 `0`。而 `$?` 就等于数组的**最后一个元素**。
换句话说：默认情况下 bash 只把「最后一段」当成整条管道的成败，**前面的失败一概不问**。

```
$ false | true; echo "两段 [${PIPESTATUS[@]}]  段数=${#PIPESTATUS[@]}  \$?=[$?]"; false | true | true; echo "三段 [${PIPESTATUS[@]}]  段数=${#PIPESTATUS[@]}  \$?=[$?]"
两段 [1 0]  段数=2  $?=[0]
三段 [1 0 0]  段数=3  $?=[0]
[退出码 0]
```

```
$ nosuchcmd04 | wc -l; echo "命令不存在的 127 也照样被吞：[${PIPESTATUS[@]}]  \$?=[$?]"
bash: line 1: nosuchcmd04: command not found
0
命令不存在的 127 也照样被吞：[127 0]  $?=[0]
[退出码 0]
```

↑ `127` 那个位置属于「bash 在子进程里报 `command not found`」，它也一样只在数组里露头。
`段数=${#PIPESTATUS[@]}` 顺手当「这条管道跑了几段」的探针 —— 第 14 节数 UUOC 的成本就靠它。

再给一条容易被当成「永远会挨刀」的写法：`grep -q`。它一命中就 exit，读端提前关门。

```
$ echo "① 5 行的短管道（塞得进管道缓冲，上游早写完了）："; seq 1 5 | grep -q 3; echo "   [${PIPESTATUS[@]}]"; echo "② 20 万行 + 第一行就命中（上游还在写，读端没了）："; seq 1 200000 | grep -q 1; echo "   [${PIPESTATUS[@]}]"; echo "③ 20 万行 + 最后一行才命中（上游其实已经写完）："; seq 1 200000 | grep -q 199999; echo "   [${PIPESTATUS[@]}"
① 5 行的短管道（塞得进管道缓冲，上游早写完了）：
   [0 0]
② 20 万行 + 第一行就命中（上游还在写，读端没了）：
   [141 0]
③ 20 万行 + 最后一行才命中（上游其实已经写完）：
   [0 0
[退出码 0]
```

↑ 同一个写法，只有**数据量 + 命中位置**不同，三个结果分别是 `0`、`141`、`0`。
所以「`grep -q` 会让上游死」**不是恒真的定律**，取决于命中那一刻上游有没有还在写。
（第三格末尾那个没闭合的 `[` 是 demo 的 `echo` 少打了一个 `]`，原文照录，不影响读数。）
`141` 这个数从哪来，第 17 节用 `yes` 和 `kill -l` 拆给你看。

---

## 6. `set -o pipefail`：让「中间那一段失败」变成整条管道的失败

`pipefail` 是本章唯一一个「先开再关」的开关，只在这几条命令内部有效。

```
$ echo "开之前：$(set -o | grep pipefail)"; set -o pipefail; echo "开之后：$(set -o | grep pipefail)"
开之前：pipefail       	off
开之后：pipefail       	on
[退出码 0]
```

现在把第 5 节那条**一模一样**的管道再跑一遍，只看 `$?` 变不变。

```
$ set -o pipefail; cat nosuchfile404.txt | wc -l; echo "pipefail 下：每段 PIPESTATUS=[${PIPESTATUS[@]}]（数组本身没变），整条管道的 \$?=[$?]，第 5 节不开时这里是 0"
cat: nosuchfile404.txt: No such file or directory
0
pipefail 下：每段 PIPESTATUS=[1 0]（数组本身没变），整条管道的 $?=[1]，第 5 节不开时这里是 0
[退出码 0]
```

★ **关键读数**：`PIPESTATUS` 数组**不会因为 `pipefail` 而改变**，
`pipefail` 只改「整条管道的 `$?`」。所以想看每段的细节，任何时候都得读数组；
`pipefail` 只是让默认的错误检查不再放过中间段。

多段各有成败时，`pipefail` 取的是**最后一个非零**的那一段（**不是第一个**）：

```
$ set -o pipefail; true | false | true; echo "三段 [${PIPESTATUS[@]}]  pipefail 后的 \$?=[$?]（最后一段是 true，但倒数第二个非零）"; set -o pipefail; false | true | false; echo "另一条 [${PIPESTATUS[@]}]  \$?=[$?]"
三段 [0 1 0]  pipefail 后的 $?=[1]（最后一段是 true，但倒数第二个非零）
另一条 [1 0 1]  $?=[1]
[退出码 0]
```

```
$ set -e -o pipefail; cat nosuchfile404.txt | wc -l; echo "这行永远执行不到"
cat: nosuchfile404.txt: No such file or directory
0
[退出码 1]
```

↑ 上面这条**只有报错、没有「这行永远执行不到」**，并且退出码是 `1`：
`set -e` 看到 `pipefail` 交上来的非零 `$?` 就当场把脚本杀了。这是两者配合的真实账单。

```
$ cat nosuchfile404.txt | wc -l >/dev/null; echo "这是一条全新的 bash -c（默认没开 pipefail），\$? 又是[$?]"
cat: nosuchfile404.txt: No such file or directory
这是一条全新的 bash -c（默认没开 pipefail），$? 又是[0]
[退出码 0]
```

★ **带走第五条**：`pipefail` 是「让 `&&` 和 `if` 对整条管道负责」的开关。
demo 里每一条都是全新的 `bash -c`，开关天然不串味；你自己的脚本里要么整份开，
要么用完 `set +o` 关掉，**别让它半开半关**（第 19 节会看到它和 `head` 打架）。

---

## 7. 重定向顺序：`>f 2>&1` 和 `2>&1 >f` 完全不是一回事

探针：一个同时往 stdout 和 stderr 各写一行的命令块，先把「两个都上屏」的样子立起来。

```
$ echo "探针本体（两个都上屏）："; { echo OUT-这是stdout; echo ERR-这是stderr >&2; }
探针本体（两个都上屏）：
OUT-这是stdout
ERR-这是stderr
[退出码 0]
```

重定向的两条规则，本章后面所有推断都从这里出发：

1. 同一行里的多个重定向**按书写顺序从左到右逐个生效**，不是同时生效。
2. `2>&1` 的意思是「把 2 号**复制**成 1 号**此刻**的指向」—— 复制的是那一刻的指向，**不是永久绑定**。

这条规则只有一处会咬人：就是 `>` 和 `2>&1` 的先后顺序。当场称两个版本：

```
$ rm -f ord1.txt; { echo OUT-这是stdout; echo ERR-这是stderr >&2; } >ord1.txt 2>&1; echo "===== 屏幕上只有这一行 echo，两句话都不见了 ====="; echo "----- cat ord1.txt -----"; cat ord1.txt
===== 屏幕上只有这一行 echo，两句话都不见了 =====
----- cat ord1.txt -----
OUT-这是stdout
ERR-这是stderr
[退出码 0]
```

```
$ rm -f ord2.txt; { echo OUT-这是stdout; echo ERR-这是stderr >&2; } 2>&1 >ord2.txt; echo "===== 上面屏幕中间混进来的那行 ERR，就是没进文件的 ====="; echo "----- cat ord2.txt -----"; cat ord2.txt; wc -l ord1.txt ord2.txt
ERR-这是stderr
===== 上面屏幕中间混进来的那行 ERR，就是没进文件的 =====
----- cat ord2.txt -----
OUT-这是stdout
 2 ord1.txt
 1 ord2.txt
 3 total
[退出码 0]
```

读法（严格按从左到右执行）：

- `>ord1.txt 2>&1` → 先把 1 指向文件；再把 2 复制成「1 此刻的指向」= 同一个文件 → **两行都进文件**。
- `2>&1 >ord2.txt` → 先把 2 复制成「1 此刻的指向」= 屏幕；再改 1 指向文件 → **stdout 进文件、stderr 留在屏幕上**。
- 所以 `ord1.txt` 两行、`ord2.txt` 一行 —— 那两行 `wc -l` 的收据就是这段推理的证明。

换用真正的报错命令再称一次，这次连退出码一起看：

```
$ ls nosuchdir404 >ord3.txt 2>&1; echo "标准写法：报错进了文件，屏幕上干净，退出码=[$?]"; cat ord3.txt
标准写法：报错进了文件，屏幕上干净，退出码=[2]
ls: cannot access 'nosuchdir404': No such file or directory
[退出码 0]
```

```
$ ls nosuchdir404 2>&1 >ord4.txt; echo "写反了：报错已经在上面屏幕上出来了；ord4.txt 行数=$(wc -l <ord4.txt)"
ls: cannot access 'nosuchdir404': No such file or directory
写反了：报错已经在上面屏幕上出来了；ord4.txt 行数=0
[退出码 0]
```

↑ 注意第 272 格那个 `退出码=[2]`：重定向**不改变命令的成败**，只改变你看得见多少（这条在第 11 节再加码）。

★ **带走第六条**：背下来没意义，理解「**复制发生在写下的那一刻**」就够了 ——
`2>&1` 必须写在 `>文件` **后面**，它才可能跟文件走；写在前面的 `2>&1` 只会跟住旧的 1 号。

---

## 8. `&>` / `&>>` / `2>&1` / `1>&2` / `>&2` 逐条真跑

```
$ rm -f amp.txt; ls nosuchdir404 &>amp.txt; echo "&> 一次收走两个流，屏幕上没有红字了；amp.txt："; cat amp.txt
&> 一次收走两个流，屏幕上没有红字了；amp.txt：
ls: cannot access 'nosuchdir404': No such file or directory
[退出码 0]
```

```
$ echo 追加的一行 &>>amp.txt; ls nosuchdir404 &>>amp.txt; echo "现在是 $(wc -l <amp.txt) 行："; cat amp.txt
现在是 3 行：
ls: cannot access 'nosuchdir404': No such file or directory
追加的一行
ls: cannot access 'nosuchdir404': No such file or directory
[退出码 0]
```

↑ `&>` / `&>>` 是 bash 的简写，等价于 `>文件 2>&1` 和 `>>文件 2>&1`；**POSIX sh 里没有这条**。
危险的是它**不会报错**：dash 把 `&` 当后台符、把 `>文件` 当成另一条空命令的重定向，
于是同一个字符串在两个 shell 里得到**完全不同的文件**。当场称字节数：

```
$ rm -f v_bash.txt v_dash.txt; bash -c "ls nosuchdir404 &>v_bash.txt"; dash -c "ls nosuchdir404 &>v_dash.txt"; sleep 0.2; echo "bash 写的文件=$(wc -c <v_bash.txt) 字节   dash 写的文件=$(wc -c <v_dash.txt) 字节"
ls: cannot access 'nosuchdir404': No such file or directory
bash 写的文件=60 字节   dash 写的文件=0 字节
[退出码 0]
```

↑ dash 那份是 **0 字节**：报错丢了、命令也确实「成功」了。要跨 shell 就写 `>文件 2>&1`。
（`/bin/sh -> dash` 这条事实在第 01 章已经钉过；这里只是它在本章的代价。）

```
$ echo "① >&2 和 1>&2 是同一件事（省略方向时默认就是 1）：两句都以报错流的形式出现在下面"; echo 甲 >&2; echo 乙 1>&2
① >&2 和 1>&2 是同一件事（省略方向时默认就是 1）：两句都以报错流的形式出现在下面
甲
乙
[退出码 0]
```

```
$ echo "② 反过来 2>&1 是把 stderr 并到 stdout：于是管道里的 tr 把两个流一起变成大写"; { echo go-stdout; echo go-stderr >&2; } 2>&1 | tr a-z A-Z
② 反过来 2>&1 是把 stderr 并到 stdout：于是管道里的 tr 把两个流一起变成大写
GO-STDOUT
GO-STDERR
[退出码 0]
```

↑ 如果分开写（只有 stdout 进管道），`tr` 就只能把第一行变大写 —— 这一条是「并流」的直接后果。

最后一条最容易被当成花活，其实是「把 stderr 存档、让 stdout 走报错通道」的标准做法：

```
$ echo "③ 把两个流对调（先给 stderr 留底，再让 stdout 走 stderr 的路）："; exec 3>&2 2>/dev/null; echo "这句写到了 fd3（=原来的 stderr，所以还能上屏）"; echo "这句本该上屏，但 2 号已经指向 /dev/null 了" >&2; exec 2>&3 3>&-; echo "恢复后再上屏的一句"
③ 把两个流对调（先给 stderr 留底，再让 stdout 走 stderr 的路）：
这句写到了 fd3（=原来的 stderr，所以还能上屏）
恢复后再上屏的一句
[退出码 0]
```

↑ 三步：`exec 3>&2` 先把 stderr 留底到 3 号 → `2>/dev/null` 把报错丢黑洞 →
写往 3 号的话还能上屏、写往 2 号的话消失 → `exec 2>&3 3>&-` 复原。
**「留底」这个动作是第 9、10 节的救命稻草。**

★ **带走第七条**：这些符号其实只有三种动作 ——
**改指向**（`>`、`>>`、`<`）、**复制指向**（`n>&m`、`n<&m`）、**丢进黑洞**（`>/dev/null`）。
`&>` 只是 bash 给的「两种一起改」快捷键，不是新概念。

---

## 9. 关闭 fd：`>&-` 与 `Bad file descriptor` 的原文

fd 不是用完自动就没了；显式关闭用 `n<&-`（读）/ `n>&-`（写）/ `exec n<&-`。
「关掉之后再写」是什么下场？下面三条分别对应三种写法，**报错原文各不相同**：

```
$ echo 这句写向已经关掉的1号 >&-
bash: line 1: echo: write error: Bad file descriptor
[退出码 1]
```

↑ 原文是 `bash: line 1: echo: write error: Bad file descriptor` ——
报错来自**内建的 `echo`**，不是外部命令：1 号已经不在进程表里，`write()` 返回 `EBADF`，bash 把它翻译给你看。

```
$ echo 这句在 >&- 之后又跟了一个 2>&1 试试 >&- 2>&1
bash: line 1: 1: Bad file descriptor
[退出码 1]
```

↑ 这条少了 `echo: write error`，变成 `bash: line 1: 1: Bad file descriptor`，
因为它连**建立重定向**这一步就失败了（要把 2 复制给 1，可 1 已经不存在），命令根本没跑起来。
同一行里的重定向还是从左到右生效（第 7 节那条规则）：**先关后复制就复制不到东西**。

```
$ exec 1>&-; echo "第一句：stdout 已经没了"; echo "第二句：也没有"; echo "第三句走 stderr，还能上屏" >&2; echo "最后这句也没有"
bash: line 1: echo: write error: Bad file descriptor
bash: line 1: echo: write error: Bad file descriptor
第三句走 stderr，还能上屏
bash: line 1: echo: write error: Bad file descriptor
[退出码 1]
```

↑ 上面这条是「**三句哑、一句响**」：`exec 1>&-` 影响**之后所有**命令，不是一句一次。
而且没有任何办法把它「恢复原样」—— 原地址没有事先 dup 出来就再也找不回了（第 10 节演示留底）。

读端关闭再读，报错原文又不一样（注意报的是编号，不是 `read`）：

```
$ exec 3<lines3.txt; echo "① 开着 3 号读到的第一行："; read -r l <&3; echo "   [$l]"; exec 3<&-; echo "② 关掉 3 号之后再拿它去读："; read -r l <&3; echo "   read 的退出码=[$?]"; echo "③ 编号会被复用：关掉再开，bash 还是给你 3 号"; exec 3<lines3.txt; cat <&3; exec 3<&-
① 开着 3 号读到的第一行：
   [fd3 的第一行]
② 关掉 3 号之后再拿它去读：
bash: line 1: 3: Bad file descriptor
   read 的退出码=[1]
③ 编号会被复用：关掉再开，bash 还是给你 3 号
fd3 的第一行
fd3 的第二行
[退出码 0]
```

↑ 读端的关闭报错是 `bash: line 1: 3: Bad file descriptor`，和外层命令无关（`read` 都没来得及跑）。
第三格还量到一件实用事：**编号会被复用** —— 关掉 3 号再开，bash 还是给你 3 号。

★ **带走第八条**：`>&-` 是「**这个编号从这个进程消失**」，不是「这一句不输出」。
要临时静音用 `>/dev/null`（编号还在，只是指向黑洞）；要退出后不留幽灵占用就显式关闭。

---

## 10. `exec` 不带命令时：改的是本 shell 的 fd，而且是持久的

普通重定向只影响它那一条命令；`exec >文件` 影响这个 shell **从这一刻起的所有命令**。

这段实验放在 `execsave.sh` 里跑 —— 它会把 stdout 带走，写在 `demo.sh` 主体里就把整章输出吞进文件了。
文件里五步：留底 → 带走 → 恢复 → 看看没留底就关掉的代价 → 最后 `ls` 一下落了什么。

```
$ bash execsave.sh
第 1 步：此刻 1 号 fd 还指向调用方（屏幕），所以这行你看得见。

第 2 步：先把 1 号 dup 到 9 号留底，再 exec >exec_log.txt —— 从这一瞬间起，
        本 shell 后面所有 stdout 都写进文件了。
但 stderr 没被动过 —— 下面这行是 >&2 打的，所以它还上屏

第 3 步：用留底的 9 号把 1 号接回来：exec 1>&9 9>&-
这句又上屏了 —— 说明 1 号 fd 真的换回去了

第 4 步：再看一次「没留底就关档」的后果（放在子 shell 里，不炸本脚本）：
execsave.sh: line 30: echo: write error: Bad file descriptor
↑ 上面那句报错了；我这句走 2 号 fd，还能上屏
   子 shell 的退出码=[0]

第 5 步：文件里到底落了什么：
--- exec_log.txt (2 行) ---
这句在 exec_log.txt 里，屏幕上看不见
这句也看不见，连 date 的输出也进了文件：2026-10-06 08:26
--- 本目录留下的文件 ---
exec_log.txt
[退出码 0]
```

↑ 五步的读法：

1. 第 2 步的 `exec 9>&1` 是**留底**（把 1 号此刻的指向复制给 9 号），第 3 步靠 `exec 1>&9 9>&-` 才接得回来。
   **没有事先 dup，就没有「恢复」这件事** —— 第 9 节那条 `exec 1>&-` 就是这个反面。
2. 第 2 步之后 `>&2` 打的那行照常上屏：只搬 1 号，**2 号一点没动**。
3. 第 4 步故意放进子 shell 里跑，所以 `Bad file descriptor` 只炸那一个子进程，
   本脚本活着 —— 这是演示「破坏性写法」的标准安全做法。
4. 第 5 步文件里那两行是**看不见的**：包括 `date` 的输出。
   那行 `2026-10-06 08:26` 是捕获那一刻的真实时刻，每次跑都会变 —— 别把它当常量核对。

最小版：只搬 stderr，看它和 stdout 互不干扰。

```
$ echo "最小版：exec 2>文件 只搬 stderr，stdout 一点不动"; exec 2>err_only.txt; echo "这句 stdout 还在屏幕上"; echo "这句 stderr 进了 err_only.txt" >&2; exec 2>&1; echo "把 2 号接回 1 号之后，这句 stderr 又上屏了" >&2; echo "----- err_only.txt 里留下的只有这一句：-----"; cat err_only.txt
最小版：exec 2>文件 只搬 stderr，stdout 一点不动
这句 stdout 还在屏幕上
把 2 号接回 1 号之后，这句 stderr 又上屏了
----- err_only.txt 里留下的只有这一句：-----
这句 stderr 进了 err_only.txt
[退出码 0]
```

↑ 注意最后那个 `exec 2>&1`：它把 2 号接回「1 号此刻的指向」，也就是恢复默认。
这个恢复动作能成立，全靠这一段里 **1 号从头到尾没被动过**；它要是也被改过，就得另留底。

`exec` 重定向的「持久」还有一层代价：**再写一次是截断，不是追加**。

```
$ exec >ov.txt; echo 第一次写; echo 第二次写; exec >ov.txt; echo 第三次写（前两次已经被截掉了）; exec >>ov.txt; echo 第四次写（追加）; exec >/dev/null; echo "这一行你看不见，它被丢进黑洞了"; cat ov.txt; echo "ov.txt 现在 $(wc -l <ov.txt) 行"
[退出码 0]
```

↑ 这一格故意跑成「屏幕上什么都不打」：`exec >ov.txt` 之后所有 stdout 都进了文件，
等到 `exec >/dev/null` 又把剩下的吞进黑洞 —— **整段既没上屏也没留下「第一次」「第二次」**。
读数：同一个 `exec >` 再写一次 = 重新截断，只剩「第三次」；`exec >>` 才把第四次接上。
所以脚本里用 `exec` 改道，**一定要记得改回来**。

```
$ echo "对照：单命令重定向 vs exec 持久改道，谁活得久："; { echo "A 只属于这一条命令" >one.txt; echo "B 这一句没带重定向，所以还上屏"; }; echo "----- one.txt -----"; cat one.txt
对照：单命令重定向 vs exec 持久改道，谁活得久：
B 这一句没带重定向，所以还上屏
----- one.txt -----
A 只属于这一条命令
[退出码 0]
```

★ **带走第九条**：想让改动只活一句话 → 把重定向贴在那句话后面，或包在 `{ }`/子 shell 里（第 24 节）；
用了 `exec` 就是给这个 shell 换水管，作用域直到「再 `exec` 一次」或进程结束。

---

## 11. `/dev/null`：把真实故障藏起来的现场复现

`/dev/null` 是一个「写多少都不留、一读就 EOF」的设备文件，它不参与管道，也不会失败。

```
$ echo "① 什么都不加，能看到报错原文："; grep 关键字 nosuchfile404.txt; echo "   退出码=[$?]"
① 什么都不加，能看到报错原文：
grep: nosuchfile404.txt: No such file or directory
   退出码=[2]
[退出码 0]
```

```
$ echo "② 只把 stderr 丢进 /dev/null：报错原文没了，退出码一个字节都没变："; grep 关键字 nosuchfile404.txt 2>/dev/null; echo "   退出码=[$?]"
② 只把 stderr 丢进 /dev/null：报错原文没了，退出码一个字节都没变：
   退出码=[2]
[退出码 0]
```

↑ **两条退出码都是 2**。`/dev/null` 不改命令的成败，只改你看得见多少 ——
排障时最坑的就是这个：脚本里挂了 `2>/dev/null`，日志里就只剩「失败」两个字，**原因全没了**。

```
$ echo "③ 想藏报错却顺手把正常输出也藏了（新手最常见的一次自伤）："; ls a.txt b.txt >/dev/null 2>&1; echo "   屏幕上什么都没显示，其实 ls 是成功的，退出码=[$?]"
③ 想藏报错却顺手把正常输出也藏了（新手最常见的一次自伤）：
   屏幕上什么都没显示，其实 ls 是成功的，退出码=[0]
[退出码 0]
```

```
$ echo "④ 正确姿势：只丢 2 号、1 号留着；下面这条里那个不存在的文件只贡献一行「消失的报错」"; ls a.txt b.txt nosuchfile404.txt 2>/dev/null; echo "   存在的两个照常打印，退出码=[$?]（ls 因为那个文件其实也报了非零）"
④ 正确姿势：只丢 2 号、1 号留着；下面这条里那个不存在的文件只贡献一行「消失的报错」
a.txt
b.txt
   存在的两个照常打印，退出码=[2]（ls 因为那个文件其实也报了非零）
[退出码 0]
```

↑ 这一格是新手最常犯的一对组合：想藏报错，顺手把正常输出也藏了（`>/dev/null 2>&1`）。
正确姿势是**只丢 2 号、1 号留着** —— 而且注意那条的退出码还是 `2`：
`/dev/null` 藏住了「为什么」，藏不住「失败了」。
想两者兼得就把 stderr 落到日志文件（`2>>log.txt`），或者用第 13 节的 `tee` 同时上屏和落盘。

```
$ echo "⑤ /dev/null 当输入用：它永远立刻给你 EOF"; wc -c </dev/null; read -r x </dev/null; echo "   read 的退出码=[$?]（读到 EOF 就算失败），x=[$x]"
⑤ /dev/null 当输入用：它永远立刻给你 EOF
0
   read 的退出码=[1]（读到 EOF 就算失败），x=[]
[退出码 0]
```

↑ `/dev/null` 当**输入**用是它的第二副面孔：永远立刻给你 EOF。
`read` 读到 EOF 就算失败（退出码 1）—— 这是「让命令安静地拿到空输入」的标准写法。

```
$ echo "⑥ 丢弃和落盘的差别（同一份数据两跑）："; head -c 100000 /dev/urandom >big_sandbox.bin; head -c 100000 /dev/urandom >/dev/null; echo "   落盘那份 $(wc -c <big_sandbox.bin) 字节，丢弃那份不占地方"; rm -f big_sandbox.bin; echo "   现在再 ls 它：$(ls big_sandbox.bin 2>&1)"
⑥ 丢弃和落盘的差别（同一份数据两跑）：
   落盘那份 100000 字节，丢弃那份不占地方
   现在再 ls 它：ls: cannot access 'big_sandbox.bin': No such file or directory
[退出码 0]
```

★ **带走第十条**：`2>/dev/null` 是「**我已经不打算看这件事的失败原因**」的书面声明。
要么别写，要么写了就顺手把 `||` 的补救分支也写上，别让它替你把 bug 藏三个月。

---

## 12. here-doc 与 here-string：`<<EOF`、`<<'EOF'`、`<<-`、`<<<` 四种写法

这一节交给两个独立文件跑：`<<-` 的证据必须是源码里**真实的 Tab 字符**，
而「终止符写错会吞掉后面全部内容」这种事故只能发生在独立文件里 ——
放进 `demo.sh` 主体就会把整章脚本一起吞掉。

```
$ bash heredoc.sh
== A. <<EOF（定界符不加引号）：正文里的 $变量、$(命令) 全都会展开 ==
这一行是真的 here-doc 正文：WHO=[张三]
命令替换也展开：年份字段 [2026]
变量紧贴普通字符：[张三的] —— 张三 后面直接跟汉字，仍然被认成变量

== B. <<'EOF'（定界符加引号）：一个字都不展开 ==
同样两行字：WHO=[$WHO]  命令替换 [$(date +%Y)]  算术 [$((1+1))]
↑ 这一段在屏幕上必须原样出现，因为定界符带了单引号

== C. <<- ：只剥每行行首的 Tab，空格一个都不剥 ==
   先看清源码里到底放了什么（本文件这一段，cat -A 里 ^I 就是 Tab，行尾 $ 是 cat -A 加的）：
   同一段正文，先用 <<INDENT（不带短横）跑一次 → Tab 原样留在输出里：
^ITabLine$
  SpaceLine$
^ITabLine2$
   再用 <<-INDENT 跑一次 → 行首 Tab 被剥掉，两个空格还在（SpaceLine 前面仍有两个空格）：
TabLine$
  SpaceLine$
TabLine2$

== D. 反斜杠在两种定界符下的差别 ==
不加引号的 here-doc 里 $WHO 被转义成字面量：$WHO，而 张三 照常展开
加引号的 here-doc 里连反斜杠都不特殊：\$WHO 原样两个字符都留下

== E. 终止符前面多一个空格 → bash 一路吞到结尾（真实警告原文） ==
bash: line 4: warning: here-document at line 1 delimited by end-of-file (wanted `EOF')
被吞掉的正文
EOF 
下一行也被吞
   ↑ 子 bash 的退出码=[0]：它把整段都当成 here-doc 正文，最后一行也没能被当命令执行

== F. here-doc 可以喂给任何读 stdin 的命令，包括 bash 自己 ==
这段代码是从 stdin 喂给一个新的 bash 的
它的 $0=[bash]  $#=[0]（命令行上一个参数都没有）
   内层 bash 的退出码=[0]

== G. here-doc 站在管道中间：它是「喂 stdin」，不是「当参数」 ==
   上游是 tr，here-doc 提供它的输入：
这行本来是小写英文字母 ABC
   here-doc 的内容也能同时进文件和下游（tee 接住）：
一份数据两条路
   tee 落到文件里的那份：
一份数据两条路

[heredoc.sh 跑完] 只在当前目录留了 hd_tee.txt，没碰别处
[退出码 0]
```

```
$ bash herestring.sh
== A. <<< 后面只接「一个词」：第二个词掉下去变成了命令行参数 ==
   跑 cat <<< 一 二 三 —— 只有「一」进了 stdin，「二」「三」成了 cat 要打开的文件名：
cat: 二: No such file or directory
cat: 三: No such file or directory
   ↑ 注意 cat 连「一」都没打印：一旦有文件名参数，它就压根不读 stdin 了。

== B. 末尾那个换行是 bash 补的 ==
   源字符串里没有换行，wc -l 却数出：1
   wc -c（含换行的字节数）：4

== C. <<< 的内容是字符串，不参与通配展开 ==
   当前目录里有 .txt 文件（列两个给你看：a.txt amp.txt ），但 <<< 里的星号原样输出：
*/*.txt
   换成 here-doc 也一样不展开成文件名（但变量会展开）：
*/*.txt

== D. 想要多行就得自己造真换行 ==
   双引号里的 "行1\n行2" 不是换行（反斜杠是字面字符），所以 wc -l 只数出 1：
1
   换成 bash 的 $'...' 语法才是真换行，wc -l 数出 2：
2
   或者干脆用命令替换造出来（这里喂的是三行）：
甲
乙
丙

== E. <<< 和管道不是一回事：左边没有命令，只有一个字符串 ==
   管道版（左边真起了一个 echo 进程）：
   from-echo
   <<< 版（没有左边进程，bash 自己把字符串塞进 stdin）：
from-here-string

== F. 变量里存着一段文本时，<<< 比 here-doc 方便 ==
2
   grep -c "" 数出的行数就是上面那个变量真实的行数（这里必须是真换行才行）

[herestring.sh 跑完] 本目录没多写任何文件
[退出码 0]
```

上面 heredoc.sh 的 A~G 七块，按顺序证的是：定界符加不加引号、正文里的 `$变量` 与 `$(( ))`
到底展不展开、`<<-` **只删 Tab 不删空格**（用 `cat -A` 把 `^I` 打出来看）、终止符多一个空格的事故。
`<<<`（here-string）那一半证的是：它只接**一个词**、末尾那个换行是 bash 补的、
正文**不参与通配展开**、以及它和管道不是一回事（左边没有进程）。

下面这两条是同一件事的**内联**版本，让你不看源文件也能对比引号的区别：

```
$ cat <<EOT
带引号的那句不会展开：$LC_ALL 和 $((6*7)) 保持原样写进来是不行的，这里定界符没加引号
所以这一行会展开成：LC_ALL=[$LC_ALL]，6×7=$((6*7))
EOT
带引号的那句不会展开：C.UTF-8 和 42 保持原样写进来是不行的，这里定界符没加引号
所以这一行会展开成：LC_ALL=[C.UTF-8]，6×7=42
[退出码 0]
```

```
$ cat <<'EOT'
同一份正文，定界符加上单引号：C.UTF-8 和 42 就一个都不展开了
这一行里的 $LC_ALL 保持字面，42 也保持字面
EOT
同一份正文，定界符加上单引号：C.UTF-8 和 42 就一个都不展开了
这一行里的 $LC_ALL 保持字面，42 也保持字面
[退出码 0]
```

↑ 两行一对比就明白：**加引号的定界符 = 「正文按原样喂给 stdin」**，等价于把整段用单引号包住。
规则和第 02 章的单/双引号是同一套，只是这里作用在「多行输入」上。

```
$ echo "终止符一行都没写的空 here-doc："; cat <<EOF
bash: line 1: warning: here-document at line 1 delimited by end-of-file (wanted `EOF')
终止符一行都没写的空 here-doc：
[退出码 0]
```

↑ 原文：`bash: line 1: warning: here-document at line 1 delimited by end-of-file (wanted 'EOF')`。
警告之后**什么都没有** —— 这就是「忘写终止符」的真实下场：**不报错、只是把后面的东西全吞掉**。
第 E 块里那条「终止符前多一个空格」是同一件事的进阶版：它把 `EOF` 后面所有内容都当正文，
连最后那行都能吞掉，而且子 shell 的退出码还是 `0`。

四条结论：

1. 定界符**加不加引号** = 正文要不要过一遍展开（和双引号/单引号同一套规则，第 02 章）。
2. `<<-` 只删行首的 **Tab**，空格一个都不删，所以要用 `<<-` 就得把缩进**统一成 Tab**
   （这一条和第 01 章「脚本用 Tab 还是空格」的争论在这里落地）。
3. 终止符必须**顶格**且完全一致（`<<-` 允许前面是 Tab），多一个空格就一路吞到文件尾。
4. 选型：一行文本用 `<<<`，多行文本用 `<<EOF`，多行又要缩进用 `<<-` + Tab。

---

## 13. `tee`：一份数据同时进文件和下游（含 `/dev/stdout` 那次「重复输出」）

`tee` 的语义：把 stdin 原样抄一份到每个参数文件，同时**继续往自己的 stdout 写**。
所以它是管道里唯一一个「既落盘又不断流」的段 —— 想边存边往下游送就用它。

```
$ rm -f t1.txt; printf "甲\n乙\n" | tee t1.txt; echo "----- tee 落到文件里的那份 -----"; cat t1.txt
甲
乙
----- tee 落到文件里的那份 -----
甲
乙
[退出码 0]
```

```
$ printf "11\n22\n33\n" | tee t2.txt | grep 2; echo "tee 自己占管道的一段，所以 PIPESTATUS=[${PIPESTATUS[@]}]"
22
tee 自己占管道的一段，所以 PIPESTATUS=[0 0 0]
[退出码 0]
```

↑ 段数=3：`tee` 是货真价实的一段（也是独立进程）。它下游那个 `grep` 只留一行，
文件里那份不受影响 —— **分流发生在 `tee` 内部，不是在管道上**。

网上很多脚本写 `tee /dev/stdout` 想「再打一遍」，它到底会不会重复？当场试：

```
$ printf "只有一行z\n" | tee /dev/stdout | cat -n
     1	只有一行z
     2	只有一行z
[退出码 0]
```

↑ **只喂了一行，下游 `cat -n` 却编到 2**。`/dev/stdout` 就是**这条管道自己**，
于是 `tee` 写「文件」（=管道）一次、再写自己的 stdout（=同一条管道）一次，两份都到了下游。

```
$ rm -f o.txt e.txt; { printf "w\n" | tee /dev/stderr | wc -l; } > o.txt 2> e.txt; echo "o.txt(stdout)=[$(cat o.txt)]  e.txt(stderr)=[$(cat e.txt)]"
o.txt(stdout)=[1]  e.txt(stderr)=[w]
[退出码 0]
```

↑ 这次 `/dev/stderr` 走的是另一条路（报错流），所以只多出一行 `w`，**没有**重复进 `wc`。

> ⚠ **本章现场踩出来的坑**：裸写 `printf w | tee /dev/stderr | wc -l` 并把整个 demo
> 重定向进文件（`demo.sh > 捕获 2>&1`）时，**捕获文件会被从头截断**。
> 原因和上面同一条：那一刻 `/dev/stderr` 就是那个文件，而 `tee` 打开输出文件默认带 `O_TRUNC`。
> 所以这里用 `{ ...; } > o.txt 2> e.txt` 把两个出口各自关进沙箱文件：现象一模一样，文件不坏。
> （第 22 节会让你把输出丢进 stdout 之前先想清楚「stdout 此刻是谁」—— 同一个教训。）

```
$ rm -f t3.txt; printf "第一版\n" | tee t3.txt >/dev/null; printf "第二版\n" | tee -a t3.txt >/dev/null; echo "tee 覆盖写 + tee -a 追加写，文件里最后是："; cat t3.txt; echo "----- 再来一次不关 stdout 的对照（屏幕上会再出现一遍）-----"; printf "第三版\n" | tee -a t3.txt
tee 覆盖写 + tee -a 追加写，文件里最后是：
第一版
第二版
----- 再来一次不关 stdout 的对照（屏幕上会再出现一遍）-----
第三版
[退出码 0]
```

★ **带走第十一条**：`tee` 的双写不是玄学，是「文件」和「stdout」两个出口各写一次；
当这两个出口是**同一个东西**（`/dev/stdout` 在管道里就是那条管道）时，你才会看到重复。

---

## 14. UUOC：`cat` 用管道其实是多起一个进程（用段数证明）

UUOC = Useless Use of cat。网上说它「没必要」，这里用 `PIPESTATUS` 的**段数**把代价量化：

```
$ wc -l <uuoc.txt; echo "重定向版：管道段数=${#PIPESTATUS[@]}  PIPESTATUS=[${PIPESTATUS[@]}]"
4
重定向版：管道段数=1  PIPESTATUS=[0]
[退出码 0]
```

```
$ cat uuoc.txt | wc -l; echo "cat 管道版：管道段数=${#PIPESTATUS[@]}  PIPESTATUS=[${PIPESTATUS[@]}]"
4
cat 管道版：管道段数=2  PIPESTATUS=[0 0]
[退出码 0]
```

↑ 两个结果一样，但下面那条**多了一个进程**（数组里多一位）。数据量小时完全感觉不到，
放进 `for` 循环里处理几万个文件就是白烧一倍的 fork。
顺便说清楚「段数」这个探针：管道里每多一个 `|` 就多一位数组元素，
`wc -l <文件` 的重定向版**根本不是管道**，所以段数是 `1`（就是那条命令自己）。

那 `cat` 什么时候**不是**废物？

```
$ echo "cat 真正有价值的场合：把多个文件拼成一个流，或者需要 - 表示 stdin 的时候"; echo "① 拼流：wc -l <a.txt 只能读一个文件；cat a.txt b.txt | wc -l = $(cat a.txt b.txt | wc -l) 行"; echo "② 有些命令只吃 stdin，不接文件名参数，这时必须用管道或 < 喂它"; printf abc | grep -q abc; echo "   printf abc | grep -q abc 的退出码=[$?]（grep 从管道读到了内容，命中）"
cat 真正有价值的场合：把多个文件拼成一个流，或者需要 - 表示 stdin 的时候
① 拼流：wc -l <a.txt 只能读一个文件；cat a.txt b.txt | wc -l = 6 行
② 有些命令只吃 stdin，不接文件名参数，这时必须用管道或 < 喂它
   printf abc | grep -q abc 的退出码=[0]（grep 从管道读到了内容，命中）
[退出码 0]
```

```
$ echo "而 grep 需要「哪个文件里的哪一行」时，把文件名交给它自己，别用管道："; grep -n an a.txt b.txt 2>/dev/null; echo "退出码=[$?]"
而 grep 需要「哪个文件里的哪一行」时，把文件名交给它自己，别用管道：
a.txt:1:banana
b.txt:2:banana
b.txt:3:durian
退出码=[0]
[退出码 0]
```

↑ 走管道（`cat a b | grep an`）就没有文件名了，只有内容 —— 这才是「少开一个进程」的**附带好处**。

★ **带走第十二条**：`cat 文件 | 命令` 该写成 `命令 <文件`；
但反过来，「命令只能吃 stdin」或「要把几个文件拼成一个流」时，`cat` 就不是废物而是必需品。

---

## 15. `xargs` 的分词陷阱（一）：名字里带空格

先把「名字里有空白」的文件造出来（全在沙箱里，仓库目录一个都没碰）：

```
$ bash mkweird.sh
造好了 4 个文件，字节名如下（每项一行，用 -print0 的 NUL 分隔再转成可见形式）：
  weird/tab	name.txt
  weird/nl
  name.txt
  weird/two words.txt
  weird/plain.txt

再用 sed -n l 打一遍（cat/sed 只会把 Tab 显示成 \t；名字里那个换行符会被 sed 当成行分隔，
所以一个文件被打印成两行 —— 这正是所有「按行读名字」的工具的死法）：
weird/tab\tname.txt$
weird/nl$
name.txt$
weird/two words.txt$
weird/plain.txt$

★ 关键数字：文件有 4 个，但按行数字节名会得到几行？
  文件真实个数 = 4
  find 正常输出后 wc -l 行数 = 5   ← 名字里带换行的那个文件被算成两行
[退出码 0]
```

↑ `mkweird.sh` 造了 4 个文件：普通名、带空格、带**真换行**、带 **真 Tab**。
那个关键数字先立住：**文件 4 个，按行数名字得到 5 行**。
`sed -n l` 那份输出里，带换行的名字自己就被打印成两行 —— 这正是所有「按行读名字」的工具的死法。

`find` 的输出用**换行**分隔，而 `xargs` 默认按 IFS 那套空白（空格/Tab/换行）**再切一刀**，
于是名字里的空格也被切成两个参数。用 `-t` 让 `xargs` 把真要执行的命令打出来：

```
$ find weird -type f | xargs -t echo 收到的参数是: 2>&1 | head -8
echo 收到的参数是: weird/tab name.txt weird/nl name.txt weird/two words.txt weird/plain.txt
收到的参数是: weird/tab name.txt weird/nl name.txt weird/two words.txt weird/plain.txt
[退出码 0]
```

↑ `-t` 先打印「即将执行的命令」，紧接着是 `echo` 自己打的那行，两份内容一模一样：
`weird/two words.txt` 变成了**两个**参数（`weird/two`、`words.txt`）。这里先用 `echo` 代替 `rm`，不下手。

顺手证明「切错了也不会自动删掉」——沙箱里的保护文件还在，因为 `xargs` 喂给 `rm` 的是被切碎的名字：

```
$ cp keepme.txt keep_copy.txt; ls -1 weird | wc -l; echo "（沙箱里 keep_copy.txt 还在：$(ls keep_copy.txt 2>&1)）"
5
（沙箱里 keep_copy.txt 还在：keep_copy.txt）
[退出码 0]
```

```
$ echo "换成只按换行分词："; find weird -type f | xargs -d "\n" -t echo 收到的参数是: 2>&1 | head -8
换成只按换行分词：
echo 收到的参数是: 'weird/tab'$'\t''name.txt' weird/nl name.txt 'weird/two words.txt' weird/plain.txt
收到的参数是: weird/tab	name.txt weird/nl name.txt weird/two words.txt weird/plain.txt
[退出码 0]
```

↑ `-d "\n"` 之后 `two words.txt` 是一个完整参数了（bash 的引号把 `\n` 解释成真换行交给 `xargs`）；
但名字里带**换行**的那个还是会翻车 —— 因为分隔符本身也被它用了，见第 16 节。

★ **带走第十三条**：`xargs` 默认的分词单位是「**空白**」，不是「行」；
只要名字可能有空白，就别用默认切法。

---

## 16. `xargs` 的分词陷阱（二）：换行 + `-print0` 修正 + 沙箱内真删

名字里带真换行的文件，让 `find` 的「一行一个」彻底失效：一个文件被读成两个。
这里不再用 `sed` 把 NUL 换成符号（那会把 NUL 本身混进输出，破坏纯文本捕获），
改成让 `xargs` 用 `printf` 把每个名字方括号包起来 —— 名字里的换行就原样露出来了：

```
$ echo "① 带换行的那个名字单独找出来（屏幕上它占两行，但它是一个文件）："; find weird -type f -name "nl*"; echo "② 四个名字各自的真面目（-print0 交给 xargs -0，再逐个 printf）："; find weird -type f -print0 | xargs -0 -I{} printf "名字=[%s]\n" "{}"
① 带换行的那个名字单独找出来（屏幕上它占两行，但它是一个文件）：
weird/nl
name.txt
② 四个名字各自的真面目（-print0 交给 xargs -0，再逐个 printf）：
名字=[weird/tab	name.txt]
名字=[weird/nl
name.txt]
名字=[weird/two words.txt]
名字=[weird/plain.txt]
[退出码 0]
```

↑ 上面那段是 **4 个文件、6 行输出**：带换行的名字自己就把两行占了，这正是 `find | wc -l` 会数错的原因。

```
$ echo "对同一批文件：find | wc -l 数出来=$(find weird -type f | wc -l)，真实个数=$(find weird -type f -print0 | tr -dc "\0" | wc -c)（数 NUL 的个数）"
对同一批文件：find | wc -l 数出来=5，真实个数=4（数 NUL 的个数）
[退出码 0]
```

↑ 数 NUL 的个数（`tr -dc "\0" | wc -c`）才是真个数：`5` vs `4`。

修法只有一个：让 `find` 用 **NUL 结尾**（`-print0`），让 `xargs` 按 **NUL** 分（`-0`）。

```
$ find weird -type f -print0 | xargs -0 -t echo 收到的参数是: 2>&1 | head -8
echo 收到的参数是: 'weird/tab'$'\t''name.txt' 'weird/nl'$'\n''name.txt' 'weird/two words.txt' weird/plain.txt
收到的参数是: weird/tab	name.txt weird/nl
name.txt weird/two words.txt weird/plain.txt
[退出码 0]
```

↑ 看清楚 `-t` 那行：四个名字各成一个参数，带 Tab/换行的被 bash 风格引号包起来了
（`'weird/nl'$'\n''name.txt'` 这种写法是 bash 的 `$'...'` 引号，第 02 章讲过）。

现在才**真的动手删**：先在沙箱里复制一份 `weird2/`，两条命令都只作用在复制品上。

```
$ rm -rf weird2; cp -a weird weird2; echo "复制了 $(find weird2 -type f -print0 | tr -dc "\0" | wc -c) 个文件，先跑错误切法的版本："; find weird2 -type f | xargs -t rm 2>&1 | head -6; echo "剩下的文件："; find weird2 -type f; echo "----- 换成 -print0 | xargs -0 之后再删 -----"; find weird2 -type f -print0 | xargs -0 -t rm; echo "还剩 $(find weird2 -type f -print0 | tr -dc "\0" | wc -c) 个文件"; rm -rf weird2; echo "沙箱里的复制品已清理"
复制了 4 个文件，先跑错误切法的版本：
rm weird2/tab name.txt weird2/nl name.txt weird2/two words.txt weird2/plain.txt
rm: cannot remove 'weird2/tab': No such file or directory
rm: cannot remove 'name.txt': No such file or directory
rm: cannot remove 'weird2/nl': No such file or directory
rm: cannot remove 'name.txt': No such file or directory
rm: cannot remove 'weird2/two': No such file or directory
剩下的文件：
weird2/tab	name.txt
weird2/nl
name.txt
weird2/two words.txt
----- 换成 -print0 | xargs -0 之后再删 -----
rm 'weird2/tab'$'\t''name.txt' 'weird2/nl'$'\n''name.txt' 'weird2/two words.txt'
还剩 0 个文件
沙箱里的复制品已清理
[退出码 0]
```

↑ 看 `rm` 的报错：它去删 `weird2/tab`、`weird2/nl`、`weird2/two` 这种**根本不存在**的名字 ——
带空格/换行的那个名字**一个都没被删掉**。反过来，如果某个切碎出来的片段**正好撞上真实存在的文件**
（比如当前目录里就有个叫 `name.txt` 的文件），那就是**无声误删**。
这一格是本章里唯一一次真删，删的是沙箱里的复制品，`rm -rf weird2` 收尾。

★ **带走第十四条**：只要文件名要跨进程传递，就用 `-print0 | xargs -0`；
「管道按行传名字」这件事**本身**就不安全，和你小心不小心没关系。

---

## 17. SIGPIPE：`yes | head -1` 之后那个退出码到底是什么

前面说过「管道里每一段都是独立进程」，这一节看两个进程之间**读端先关门**会发生什么。

```
$ yes | head -1; echo "PIPESTATUS=[${PIPESTATUS[@]}]  整条管道的 \$?=[$?]"
y
PIPESTATUS=[141 0]  整条管道的 $?=[0]
[退出码 0]
```

读数：`yes` 那一段是 **141**，`head` 那一段是 `0`，整条管道 `$?` 也是 `0`（默认只看最后一段，第 5 节）。
`head` 读满 1 行就 `exit(0)`，读端通道没人接了；`yes` 的下一次 `write` 触发内核信号 `SIGPIPE`，
默认动作是终止进程 → **128+13=141**。所以「141」不是 `head` 的错，是**写端被砍**的记账。

```
$ echo "把 141 拆开：141 = 128 + 13；本机 13 号信号的名字 = [$(kill -l 13)]"; echo "bash 里 128+信号号 是「被信号杀死」的统一记账方式，不是命令自己返回的"
把 141 拆开：141 = 128 + 13；本机 13 号信号的名字 = [PIPE]
bash 里 128+信号号 是「被信号杀死」的统一记账方式，不是命令自己返回的
[退出码 0]
```

换个下游、换个上游，刀还在不在？三条对照，全在同一台机器上跑出来：

```
$ yes | head -1 >/dev/null; echo "① 连 head 的输出都丢掉，两段还是 [${PIPESTATUS[@]}]（第一段照旧 141）"; seq 1 200000 | awk "NR==1{exit 0}"; echo "② awk 读完第一行就退：[${PIPESTATUS[@]}]"
① 连 head 的输出都丢掉，两段还是 [141 0]（第一段照旧 141）
② awk 读完第一行就退：[141 0]
[退出码 0]
```

```
$ seq 1 200000 | tail -1; echo "③ tail 必须读到最后一行才动手：上游 [${PIPESTATUS[@]}]（两个 0，没人挨刀）"; seq 1 200000 | sort | tail -3 >/dev/null; echo "④ sort 同理，它得把全部读完才输出：[${PIPESTATUS[@]}]"
200000
③ tail 必须读到最后一行才动手：上游 [0 0]（两个 0，没人挨刀）
④ sort 同理，它得把全部读完才输出：[0 0 0]
[退出码 0]
```

★ **带走第十五条**：判断「上游会不会 141」只看一件事 —— **下游退出时上游还有没有要写的**。
`head` / `grep -q` / `awk exit` 这类**提前收工** = 会砍；`tail` / `sort` / `wc` 这类**必须读完** = 不砍。
第 19 节会看到：一旦开了 `pipefail`，这条「上游 141」就会变成**整条脚本的失败理由**。

---

## 18. 后台进程与 `trap` 收尾：`143 = 128+15` 的同一套记账

本章唯一的后台进程就是这几条 `sleep`（无害、当场收干净）；**不起任何端口**。
为什么要在这里讲后台？因为第 17 节那套「128 + 信号号」的记账，脚本收尾时天天用到：
**你 kill 掉一个后台任务，它的退出码不是 0，是 143。**

```
$ sleep 30 & p=$!; echo "起了一个后台 sleep，\$! 记到的 pid=[$p]"; kill $p; wait $p; echo "被 SIGTERM(15) 杀掉后 wait 到的退出码=[$?]"
起了一个后台 sleep，$! 记到的 pid=[11646]
被 SIGTERM(15) 杀掉后 wait 到的退出码=[143]
[退出码 0]
```

↑ 读数：`$!` 是「最后一个后台任务的 pid」；`kill` **只是发信号**，必须 `wait` 才能拿到它的退出码。
`143 = 128 + 15`（SIGTERM），和第 17 节的 `141 = 128 + 13`（SIGPIPE）同一个公式。

```
$ sleep 1 & p=$!; trap "kill $p 2>/dev/null; echo \"[trap EXIT] 收尾：杀掉的后台 pid=$p\"; wait $p" EXIT; echo "这行跑完脚本就正常退出，看 trap 有没有替我们擦屁股"
这行跑完脚本就正常退出，看 trap 有没有替我们擦屁股
[trap EXIT] 收尾：杀掉的后台 pid=11648
[退出码 0]
```

↑ `trap EXIT` = 「这个 shell 无论怎么结束都执行这一段」；`demo.sh` 自己清理沙箱用的就是它。

```
$ sleep 0.2 & p=$!; echo "不写 trap、直接 exit 0 的话：后台 pid=[$p]（它还在睡）"; exit 0
不写 trap、直接 exit 0 的话：后台 pid=[11650]（它还在睡）
[退出码 0]
```

↑ 上一条打完 pid 就退了，`trap` 没写，那个 `sleep` 只能自己睡完（0.2 秒）；
真脚本里长期跑的后台任务必须 `trap EXIT` 收尾，否则留一堆孤儿进程。

```
$ echo "信号退出码这一家的对照表（全是 128+信号号，本机名字由 kill -l 现查）："; printf "  141 = 128 + 13 (%s)  ← 写端没人接，第 17 节\n" "$(kill -l 13)"; printf "  143 = 128 + 15 (%s) ← 被 kill 默认信号干掉，本节\n" "$(kill -l 15)"; printf "  130 = 128 + 2  (%s)   ← 你按 Ctrl-C\n" "$(kill -l 2)"
信号退出码这一家的对照表（全是 128+信号号，本机名字由 kill -l 现查）：
  141 = 128 + 13 (PIPE)  ← 写端没人接，第 17 节
  143 = 128 + 15 (TERM) ← 被 kill 默认信号干掉，本节
  130 = 128 + 2  (INT)   ← 你按 Ctrl-C
[退出码 0]
```

★ **带走第十六条**：`kill` 发的是信号，退出码要 `wait` 才拿得到；被信号杀死的进程一律 `128+信号号`。
看到 `141`/`143` 不要慌，先问「**谁给它发了哪个信号**」，答案就在数字里。

---

## 19. `pipefail` 撞上 `head`：`set -euo pipefail` 之后的假失败

第 17 节里 `yes | head -1` 是「head 成功、上游被砍」；把它放进 `pipefail` 的脚本里就是另一回事。

```
$ set -o pipefail; yes | head -1 >/dev/null; echo "pipefail 开着的整条管道 \$?=[$?]（head 明明成功了，141 从上游传上来了）"
pipefail 开着的整条管道 $?=[141]（head 明明成功了，141 从上游传上来了）
[退出码 0]
```

```
$ set -e -o pipefail; yes | head -1 >/dev/null; echo "这行永远执行不到：set -e 已经被 141 杀掉"
[退出码 141]
```

↑ 真实账单：屏幕上只有 `yes` 的那一行 `y` 被丢进 `/dev/null` 了，
「这行永远执行不到」**没打出来**，外层看到退出码 **141**。
第 17 节那条无害的写法，在 `set -e -o pipefail` 里就是**一次真失败**。

两种解法，各跑一遍：

```
$ set -e -o pipefail; yes | head -1 >/dev/null || true; echo "解法①（|| true 明确说我不在意上游）：脚本活下来了，\$?=[$?]"
解法①（|| true 明确说我不在意上游）：脚本活下来了，$?=[0]
[退出码 0]
```

```
$ set -e -o pipefail; if yes | head -1 >/dev/null; then echo "解法②：进 then（整条被判定成功）"; else echo "解法②：进 else —— pipefail 把整条判成失败，\$?=[$?]，但脚本还活着"; fi
解法②：进 else —— pipefail 把整条判成失败，$?=[141]，但脚本还活着
[退出码 0]
```

↑ 解法②值得多看一眼：`if` 的判断条件**不受 `set -e` 追杀**（这是第 03 章讲过的规则），
所以脚本活下来了，但**走进了 `else`** —— 一个明明成功的 `head` 被判成失败。

**解法③**（最干净）：在那几行**局部关掉** —— `set +o pipefail; …; set -o pipefail`，
或者干脆不用管道：`head -1 文件` 而不是 `cat 文件 | head -1`（第 14 节的 UUOC 在这里第二次有用）。

★ **带走第十七条**：`head` / `tail -n 1` / `grep -q` / `awk exit` 这类「提前收工」的下游，
和 `pipefail` **天生打架**。要么用 `|| true` / `if` 把它明确标成「我看过并且不在乎」，要么就局部关开关。

---

## 20. 进程替换 `<(…)`：它给的是一个**文件名**，不是 stdin

进程替换是本章最后一个大杀器：把「一条命令的输出」伪装成一个**文件名**塞进参数位置。

```
$ wc -c <(echo hi)
3 /dev/fd/63
[退出码 0]
```

↑ 注意 `wc` 打印的是 `/dev/fd/63` 这样的「文件名」：`<( )` 是把输出挂到一个 fd 上，
再把 `/dev/fd/N` 这个**名字**塞进**参数位置**。所以能用它的命令必须是**能吃文件名**的。

```
$ cat <(echo hi); echo "（cat 也是把它当文件名读出来的）"
hi
（cat 也是把它当文件名读出来的）
[退出码 0]
```

```
$ wc -c < <(echo hi); echo "换成 < <( ) 才是真的接到 stdin：这次没有文件名，只有数字"
3
换成 < <( ) 才是真的接到 stdin：这次没有文件名，只有数字
[退出码 0]
```

两个写法的区别不是风格，是**入口**不同：`cmd <( )` 走**参数**，`cmd < <( )` 走 **stdin**。

```
$ read -r x <(echo hi) </dev/null; echo "① read 只吃 stdin，参数位置给它就翻脸：退出码=[$?]"; exec 6< <(echo hi); if [ -p /dev/fd/6 ]; then echo "② 挂在 6 号上的那个名字，test -p 说它是【管道】，不是普通文件"; else echo "② test -p 说它不是管道"; fi; echo "   从它读到的内容：$(cat <&6)"; exec 6<&-
bash: line 1: read: `/dev/fd/63': not a valid identifier
① read 只吃 stdin，参数位置给它就翻脸：退出码=[1]
② 挂在 6 号上的那个名字，test -p 说它是【管道】，不是普通文件
   从它读到的内容：hi
[退出码 0]
```

↑ 第①格：`read` 只吃 stdin，你给它参数位置它就翻脸 ——
`bash: line 1: read: '/dev/fd/63': not a valid identifier`（它把那个文件名当成「要写入的变量名」了）。
第②格是本节最值钱的一条：挂在 6 号上的那个名字，`test -p` 说它是**管道**，不是普通文件。

`/dev/fd/N` 背后是**匿名管道**，不是磁盘上的临时文件。两个必须记住的后果：

1. 没有真实的 inode 大小 —— `wc -c` 读它是**读管道字节数**，`ls -l` 看不到内容长度；
2. 命令想「把这个文件打开两遍 / `seek` 回去」就会翻车 —— **管道不能回退**。

★ **带走第十八条**：能用 `<( )` 的前提是「那条命令接文件名参数」；
只吃 stdin 的命令（`read`、很多过滤器的默认行为）就得写 `< <( )`。

---

## 21. 进程替换 `>(…)` 和 `diff <(sort a) <(sort b)`

```
$ echo "两个文件各自的排序结果直接对比，中间不落任何临时文件："; echo "--- sort a.txt ---"; sort a.txt; echo "--- sort b.txt ---"; sort b.txt; echo "--- diff <(sort a.txt) <(sort b.txt) ---"; diff <(sort a.txt) <(sort b.txt); echo "diff 的退出码=[$?]（1 = 有差别）"
两个文件各自的排序结果直接对比，中间不落任何临时文件：
--- sort a.txt ---
apple
banana
cherry
--- sort b.txt ---
apple
banana
durian
--- diff <(sort a.txt) <(sort b.txt) ---
3c3
< cherry
---
> durian
diff 的退出码=[1]（1 = 有差别）
[退出码 0]
```

↑ 这就是 `<( )` 最实用的场合：`diff` / `sort` / `join` / `paste` 这些「要接文件名」的命令，
现在可以直接吃「另一条命令的结果」，**省掉 `mktemp` + 清理的那一段**。

```
$ paste -d+ <(printf "1\n2\n3\n") <(printf "a\nb\nc\n")
1+a
2+b
3+c
[退出码 0]
```

↑ 两个 `<( )` 占两个参数位，编号各是一个 `/dev/fd/N` —— 一次调用可以同时挂好几个。

`>( )` 是反方向：把「写进这个名字的东西」交给一条命令去消化。它有一个必须记住的坑：**异步**。

```
$ rm -f ps_async.txt; echo 这条走进程替换 > >(cat > ps_async.txt); echo "主命令立刻就退出了，此刻 ps_async.txt 里=[$(cat ps_async.txt 2>&1)]"; sleep 0.3; echo "等 0.3 秒再看：ps_async.txt 里=[$(cat ps_async.txt)]"
主命令立刻就退出了，此刻 ps_async.txt 里=[这条走进程替换]
等 0.3 秒再看：ps_async.txt 里=[这条走进程替换]
[退出码 0]
```

```
$ printf "x\ny\n" > >(wc -l > ps_count.txt); echo "不等的话立刻读：[$(cat ps_count.txt 2>&1)]"; sleep 0.3; echo "等一下再读：ps_count.txt=[$(cat ps_count.txt)] 行"
不等的话立刻读：[2]
等一下再读：ps_count.txt=[2] 行
[退出码 0]
```

↑ 这两格跑出来的是「**这一次**运气好」：`cat` 那句读到的已经是内容了。
但语义上 `>( )` 起的那个进程 **bash 不等它** —— 上一行还写着「此刻读到文件不存在的报错」的现场，
这一行就可能已经写完了。所以把它当成「**下一行马上去读，结果不保证**」来用：
写日志、边算边存很顺手；当作同步产出就是赌。
真需要同步就 `wait`（或像上面那样 `sleep` 一下），或者干脆别用 `>( )`，改成 `… > 文件` 之后再显式读那个文件。

★ **带走第十九条**：`<( )` 同步（对方打开时数据就在），`>( )` **异步**（收尾要自己管）。

---

## 22. 管道缓冲：为什么 `| grep` 的时候输出会「卡住」（`stdbuf` 本机有）

C 库的 stdout 有个规则：目标是**终端**时按行缓冲（一行一 flush），
目标是**文件或管道**时改成块缓冲（攒满 4K/8K 才吐一次）。
管道里每一段看到的 stdout 都不是终端，于是**中间那段**就把你的实时性吃掉了。

```
$ echo "stdbuf 在本机：$(command -v stdbuf)  版本 $(stdbuf --version)"
stdbuf 在本机：/usr/bin/stdbuf  版本 stdbuf (uutils coreutils) 0.8.0
[退出码 0]
```

这一组实验放在 `slowpipe.sh` 里：下游是 `while read`（一收到行就打印距开始的毫秒数），
上游是「A → 睡 0.35 秒 → B」。六段分别是「中间放什么」的对照：

```
$ bash slowpipe.sh
下游是 while read（一收到行就打印时刻），上游是「A → 睡 0.35 秒 → B」。
  --- ①  中间只有 cat：它不缓冲，来多少搬多少（中间段命令：cat）---
      收到 [A] 距开始 3 毫秒
      收到 [B] 距开始 357 毫秒
  --- ②  中间放 sed -n p：stdout 不是终端 → 整块攒满才吐（中间段命令：sed -n p）---
      收到 [A] 距开始 353 毫秒
      收到 [B] 距开始 354 毫秒
  --- ③  中间放 stdbuf -oL sed -n p：强制行缓冲（中间段命令：stdbuf -oL sed -n p）---
      收到 [A] 距开始 4 毫秒
      收到 [B] 距开始 355 毫秒
  --- ④  中间放 sed -u：sed 自己的行缓冲开关，效果同上（中间段命令：sed -u -n p）---
      收到 [A] 距开始 4 毫秒
      收到 [B] 距开始 361 毫秒
  --- ⑤  中间放 grep .：和 sed 一样块缓冲（中间段命令：grep .）---
      收到 [A] 距开始 353 毫秒
      收到 [B] 距开始 354 毫秒
  --- ⑥  中间放 grep --line-buffered .：grep 的自带开关（中间段命令：grep --line-buffered .）---
      收到 [A] 距开始 2 毫秒
      收到 [B] 距开始 356 毫秒

对照结论：②⑤ 的 A 都被推迟到 350ms 之后；①③④⑥ 的 A 几乎立刻到达。
这就是「明明上游在动，下游一个字都不出」的根源：缓冲在中间那一段，不在管道本身。
代价：行缓冲会让每一行都触发一次 write，长管道上会明显变慢（本节只看时序，不测吞吐）。

== 另一半真相：管道容量有限，下游不读的话上游会被堵住 ==
   下游一律先睡 0.6 秒再开始读；比较「上游写完的时刻」和「下游读完的时刻」。
   （两段时间都走 stderr 打印：如果走 stdout 就会被下一段的 cat 吞掉，屏幕上什么都看不见。）
   小数据(50 行, 约 150 字节)：上游（seq）写完于 +4 毫秒
   小数据(50 行, 约 150 字节)：下游（读完）于 +609 毫秒
   大数据(120000 行, 远超管道缓冲)：上游（seq）写完于 +609 毫秒
   大数据(120000 行, 远超管道缓冲)：下游（读完）于 +610 毫秒
   读法：小数据那两行时间差 ≈ 600 毫秒（上游一开机就写完了，剩下的时间在下游睡觉）；
         大数据那两行时间差 ≈ 0（上游被管道堵住，只能等下游开始读才写得完）。
   结论：管道是有容量的缓冲区，写满就堵；第 17 节的 SIGPIPE 就是「堵到没人接」的另一种死法。
[退出码 0]
```

关键读数（毫秒每跑必抖，读**分组**）：

- **②`sed -n p` 和 ⑤`grep .`**：A 被推迟到和 B 一起（都在 350ms 之后）—— **块缓冲**，
  明明上游在动，下游一个字都不出。这就是「`tail -f | grep` 不实时」的根源，**不是 grep 慢**。
- **①`cat`、③`stdbuf -oL sed`、④`sed -u`、⑥`grep --line-buffered`**：A 几乎立刻到达 —— 行缓冲/不缓冲。
  三个开关各自属于谁：`stdbuf -oL` 是外部命令的通用包装，`sed -u` 是 sed 自己的，
  `--line-buffered` 是 grep 自己的。**能用自己的开关就别用 stdbuf**（后者靠 `LD_PRELOAD`，别的实现未必吃）。
- 最后那组「小数据 50 行 vs 大数据 120000 行」：小数据上游 **+4ms 就写完**、下游 +609ms 读完
  （时间都花在下游睡觉）；大数据上游 **+609ms 才写完**、下游 +610ms 读完 ——
  **上游被管道堵住了**，只能等下游开始读才写得完。
  这就是「管道是有容量的缓冲区」的直接收据，也是第 17 节 SIGPIPE 的另一半背景。

行缓冲的代价，用同一批数据称一次：

```
$ to_ms(){ local t=$1; printf "%d" $(( ${t%.*} * 1000 + 10#${t#*.} / 1000 )); }; b=$(to_ms "$EPOCHREALTIME"); seq 1 200000 | sed -n p >/dev/null; m=$(to_ms "$EPOCHREALTIME"); echo "块缓冲 sed -n p 跑 20 万行：$((m-b)) 毫秒"; b=$(to_ms "$EPOCHREALTIME"); seq 1 200000 | sed -u -n p >/dev/null; m=$(to_ms "$EPOCHREALTIME"); echo "行缓冲 sed -u -n p 跑 20 万行：$((m-b)) 毫秒"
块缓冲 sed -n p 跑 20 万行：15 毫秒
行缓冲 sed -u -n p 跑 20 万行：241 毫秒
[退出码 0]
```

↑ 这就是「实时」的价格：行缓冲等于每行一次 `write` 系统调用，吞吐明显掉（本轮 15ms → 241ms，
读的是「差一个数量级」这件事，不是这两个数）。
**别在生产里无脑加 `--line-buffered`**，只有你真的需要边看边处理时才值得。

```
$ echo "内核给单根管道的容量也有上限，本机可调到的最大值："; cat /proc/sys/fs/pipe-max-size; echo "同一家族还有哪些旋钮："; ls /proc/sys/fs | grep -i pipe
内核给单根管道的容量也有上限，本机可调到的最大值：
1048576
同一家族还有哪些旋钮：
pipe-max-size
pipe-user-pages-hard
pipe-user-pages-soft
[退出码 0]
```

↑ `1048576` = 1MB 是「**允许**把一根管道调到多大」的上限，**不是默认容量**（默认由内核决定，量级是几十 KB）。
`slowpipe.sh` 最后那组对照就是默认容量的影子。

★ **带走第二十条**：管道不是无限大的传送带，它是一个**有限容量的缓冲区**，写满就堵、读空就等；
「谁读到了什么」取决于谁在什么时候写、谁在什么时候读。

---

## 23. `exec 3<` 文件 + `read -u 3`；以及 `{变量名}>` 让 bash 自己挑 fd

前面都是 0/1/2 三个公用水管；bash 还允许你自己接一根**私有水管**（3 号以上）。

```
$ exec 3<lines3.txt; n=0; while IFS= read -r -u 3 line; do n=$((n+1)); echo "  从 3 号读到第 $n 行：[$line]"; done; echo "读完（read 的退出码=[$?]），关掉 3 号"; exec 3<&-
  从 3 号读到第 1 行：[fd3 的第一行]
  从 3 号读到第 2 行：[fd3 的第二行]
读完（read 的退出码=[0]），关掉 3 号
[退出码 0]
```

↑ 为什么不直接 `while read`？因为 stdin 经常已经被管道占了；
3 号这种「私设水龙头」让你能**同时读两个来源**（第 05 章讲 `read` 循环的坑时还要靠它）。

硬编码 3 号有个隐患：这段代码可能被放进一个**已经用了 3 号**的环境里。
bash 4.1+ 给了自动分配：`exec {变量名}>文件`，编号由 bash 挑（从 10 往上找空位）。

```
$ echo "自动分配：exec {fd}>auto04.txt → 看 bash 本机给了几号"; exec {fd}>auto04.txt; echo "  拿到的编号 fd=[$fd]"; echo 写进去的一句 >&"$fd"; exec {fd}>&-; echo "  关掉后文件内容：$(cat auto04.txt)"
自动分配：exec {fd}>auto04.txt → 看 bash 本机给了几号
  拿到的编号 fd=[24]
  关掉后文件内容：写进去的一句
[退出码 0]
```

↑ 三个细节：① `{fd}` 那对大括号是**语法的一部分**，不是花括号展开；
② 用完的编号**回填进变量**（本机这一轮给了 `24` —— 这个编号每次跑都可能不同，
读的是「它是个两位数、由 bash 挑的」）；
③ 后面用的时候要写 `>&"$fd"`（把变量的**值**当编号）。
写成 `exec $fd>&-` 就翻脸了 —— **那是「执行一个叫 24 的命令」**。

```
$ echo "拿同一个变量再写一次（fd 已经关了）："; echo 还想写 >&"$fd"; echo "退出码=[$?]"
拿同一个变量再写一次（fd 已经关了）：
bash: line 1: "$fd": Bad file descriptor
退出码=[1]
[退出码 0]
```

↑ 关掉之后拿同一个变量再写：`bash: line 1: "$fd": Bad file descriptor`。
注意原文里 `$fd` 是**带着引号**出现在报错里的 —— 第 9 节那条报的是编号 `3`，这条报的是你写的那个词。

```
$ echo "读侧同理：exec {in}<lines3.txt"; exec {in}<lines3.txt; echo "  编号=[$in]"; read -r -u "$in" one; read -r -u "$in" two; echo "  读到：[$one] / [$two]"; exec {in}<&-; echo "  再读第三次（文件已经到 EOF）："; read -r -u "$in" three; echo "  退出码=[$?] three=[$three]"
读侧同理：exec {in}<lines3.txt
  编号=[24]
  读到：[fd3 的第一行] / [fd3 的第二行]
  再读第三次（文件已经到 EOF）：
bash: line 1: read: 24: invalid file descriptor: Bad file descriptor
  退出码=[1] three=[]
[退出码 0]
```

↑ 读侧同理，而且最后那一格的报错又换了一副面孔：
`bash: line 1: read: 24: invalid file descriptor: Bad file descriptor` ——
这次是 `read` 自己报的（它拿到了一个已经不在表里的编号）。
**同一类错误至少三种原文**，本章把它们逐条收齐了。

★ **带走第二十一条**：私有 fd 的完整生命周期是「**开 → 用 → 关**」，
开用 `exec {v}<文件`（别让编号写死），用 `read -u "$v"` / `>&"$v"`，关 `exec {v}<&-`。

---

## 24. `{ }` 分组重定向 vs `( )` 子 shell 重定向

一整段命令想共用一个重定向，就把它包起来。包法有两种，差别在「**起不起新进程**」。

```
$ v=1; { v=2; echo "组内 v=[$v]"; } >grp04.txt 2>&1; echo "组外 v=[$v]  ← { } 不起新进程，所以改动留住了"; echo "grp04.txt 里=[$(cat grp04.txt)]"
组外 v=[2]  ← { } 不起新进程，所以改动留住了
grp04.txt 里=[组内 v=[2]]
[退出码 0]
```

```
$ v=1; ( v=3; echo "子 shell 内 v=[$v]" ) >sub04.txt 2>&1; echo "外面 v=[$v]  ← 和第 3 节管道右侧同一个病"; echo "sub04.txt 里=[$(cat sub04.txt)]"
外面 v=[1]  ← 和第 3 节管道右侧同一个病
sub04.txt 里=[子 shell 内 v=[3]]
[退出码 0]
```

↑ 输出部分两者都能被重定向带走，区别全在**变量**上：
`{ }` 改的是本 shell 的变量（组外 `v=[2]`），`( )` 改的是复印件（外面 `v=[1]`）——
**和第 3 节管道右侧同一个病**。

```
$ echo "整段丢弃 / 整段捕获，两种写法都要一对括号或大括号："; { echo 甲; echo 乙; } >/dev/null; echo "  { } > /dev/null 之后屏幕照样有字（说明只是那一段被吞了）"; out=$( { echo 丙; echo 丁; } ); echo "  命令替换捕获版 out=[$out]"; { echo 戊; echo 己; } | wc -l
整段丢弃 / 整段捕获，两种写法都要一对括号或大括号：
  { } > /dev/null 之后屏幕照样有字（说明只是那一段被吞了）
  命令替换捕获版 out=[丙
丁]
2
[退出码 0]
```

↑ 注意 `$( { } )` 这个组合：命令替换**本身就是子 shell**，所以里面再怎么改变量都出不来。
想捕获输出就别改值，想留变量就别用 `$()`。
最后一格 `{ echo 戊; echo 己; } | wc -l` 顺手把第 4 节那条规矩再演一遍：
进了管道，`{ }` 也不再是本 shell 了。

```
$ echo "exec 在两种括号里的作用域差别："; ( exec >inner_exec.txt; echo "子 shell 里的 exec 只影响子 shell" ); echo "这句在外面，没被带走"; echo "----- inner_exec.txt -----"; cat inner_exec.txt
exec 在两种括号里的作用域差别：
这句在外面，没被带走
----- inner_exec.txt -----
子 shell 里的 exec 只影响子 shell
[退出码 0]
```

↑ `exec` 的重定向改道在子 shell 里**只影响那个子 shell** —— 这是「用 `( )` 隔离脏改动」的标准用法，
也是第 10 节那个「忘了改回来」问题的解药。

语法细节（踩过的人才懂）：`{ }` 的大括号是**保留字**，后面要有空格、最后一条命令要有分号、
`}` 要单独成词；`( )` 是**操作符**，括号前后不需要空格，里面也不用分号。

★ **带走第二十二条**：要把变量的修改留在本 shell（计数、赋值）→ `{ }`；要隔离脏改动 → `( )`。

---

## 25. `noclobber`：`>` 拒绝覆盖，`>|` 强制覆盖

本章最后一个开关：`set -C`（= `set -o noclobber`）让 `>` 变成「只许新建，不许覆盖」。

```
$ cp keepme.txt nc04.txt; echo "先造一个已经存在的文件："; cat nc04.txt
先造一个已经存在的文件：
需要被保护起来的原始文件
[退出码 0]
```

```
$ set -C; echo "第二版" >nc04.txt; echo "退出码=[$?]"; echo "文件还是原来的："; cat nc04.txt; set +C
bash: line 1: nc04.txt: cannot overwrite existing file
退出码=[1]
文件还是原来的：
需要被保护起来的原始文件
[退出码 0]
```

↑ 真实报错是 `bash: line 1: nc04.txt: cannot overwrite existing file`，
文件**一个字节都没动** —— 这就是它防的那件事：手滑把重要文件覆盖成一行字。

```
$ echo "用 >| 强制覆盖（bash 的逃生门）："; set -C; echo "第三版" >|nc04.txt; echo "退出码=[$?]"; set +C; cat nc04.txt
用 >| 强制覆盖（bash 的逃生门）：
退出码=[0]
第三版
[退出码 0]
```

```
$ echo ">& 这种写法同样被拦；>> 追加不受影响；tee 是程序自己 open 的，管不着："; set -C; echo 追加成功 >>nc04.txt; echo "  >> 退出码=[$?]"; echo 被拦住 >&nc04.txt; echo "  >& 退出码=[$?]"; printf "tee 写的一句\n" | tee nc04.txt >/dev/null; echo "  tee 退出码=[$?]"; set +C; echo "----- 现在 nc04.txt 里有三行：-----"; cat nc04.txt
>& 这种写法同样被拦；>> 追加不受影响；tee 是程序自己 open 的，管不着：
  >> 退出码=[0]
bash: line 1: nc04.txt: cannot overwrite existing file
  >& 退出码=[1]
  tee 退出码=[0]
----- 现在 nc04.txt 里有三行：-----
tee 写的一句
[退出码 0]
```

↑ 读数三件：`>>` 是「追加」不是「覆盖」，noclobber **不拦**；
`>&文件` 这种写法**同样被拦**；`tee` 根本没走重定向，它自己 `open()`，所以照样把文件改写了。
**别把 noclobber 当安全机制，它只是「挡住 `>` 这一只手滑」。**

★ **带走第二十三条**：脚本里临时开一下、用完 `set +C` 关掉（上面每条都自己关了）；
demo 每条命令都是全新的 `bash -c`，所以开关不会串到下一节。

---

## 26. 自证：沙箱里的事只留在沙箱，章目录一个字都没多

demo 的写操作全部发生在 `mktemp -d` 的沙箱里；此刻的当前目录就是沙箱的 `work/`，
本章跑出来的文件全在这里（跑完就整体删掉，**仓库目录从头到尾只读**）：

```
$ echo "沙箱 work/ 里现在的文件数=$(ls | wc -l)，前 12 个："; ls | head -12
沙箱 work/ 里现在的文件数=37，前 12 个：
a.txt
amp.txt
auto04.txt
b.txt
demo.sh
e.txt
err_only.txt
exec_log.txt
execsave.sh
grp04.txt
hd_tee.txt
heredoc.sh
[退出码 0]
```

↑ 这些名字（`ord*.txt`、`weird/`、`t1.txt`、`ps_*.txt`、`exec_log.txt`…）全是本章造的加工品，
它们跟着沙箱一起被 `trap cleanup EXIT` 删掉，一个都不会带回仓库。
`文件数=37` 是捕获那一刻沙箱里的项数（第 22 节还留了一个 `weird2/` 的删除现场），
这个数字会随章节内容变化，**不必核对**。

本章目录里应该**只有源文件**：`demo.sh` + 五个辅助 `.sh`，没有任何 demo 产生的文件：

```
$ 〔在 04-pipes-and-redirection 里〕 find . -maxdepth 1 -type f | sort
./demo.sh
./execsave.sh
./heredoc.sh
./herestring.sh
./mkweird.sh
./slowpipe.sh
[退出码 0]
```

再确认两件事：目录里没有子目录残留，也没有 C 源码混进来
（本章和 C 无关，那一层深度是你数据结构仓库别人的取证现场）：

```
$ 〔在 04-pipes-and-redirection 里〕 echo "子目录数=$(find . -mindepth 1 -maxdepth 1 -type d | wc -l)  main.c 个数=$(find . -name main.c | wc -l)"
子目录数=0  main.c 个数=0
[退出码 0]
```

本章一条网络命令都没用、没起过任何监听端口（后台只有几条 `sleep`，且都在同一条命令里收尾）：

```
$ echo "本章未起任何端口；复核一下此刻的监听列表："; if command -v ss >/dev/null 2>&1; then ss -lnt | head -5; else echo "ss 本机没有，改用 netstat：$(command -v netstat || echo 也没有)"; fi
本章未起任何端口；复核一下此刻的监听列表：
State  Recv-Q Send-Q Local Address:Port  Peer Address:Port
LISTEN 0      4096      127.0.0.54:53         0.0.0.0:*   
LISTEN 0      511        127.0.0.1:36869      0.0.0.0:*   
LISTEN 0      4096         0.0.0.0:22         0.0.0.0:*   
LISTEN 0      4096       127.0.0.1:631        0.0.0.0:*   
[退出码 0]
```

↑ 这张监听表是**此刻这台机器的真实状态**（`systemd-resolved` 的 53、CUPS 的 631、sshd 的 22 等），
不是本章起的；每次跑内容都可能不同。它在这里只承担一个证明：**里面没有任何 `bash04` 的痕迹**。

```
$ echo "此刻系统里还有没有 demo 留下的 sleep 进程（应当只剩你自己终端里的，或者干脆没有）："; pgrep -a sleep | head -5 || echo "pgrep 说没有（退出码 $?）"
此刻系统里还有没有 demo 留下的 sleep 进程（应当只剩你自己终端里的，或者干脆没有）：
[退出码 0]
```

↑ `pgrep -a sleep` 没有输出、退出码 0 —— demo 起过的那几条 `sleep` 全被 `kill`/`wait` 或 `trap` 收干净了。

---

## 一张表：管道与重定向的记账规则

| 你写下的 | 动的到底是什么 | 本章哪一节 | 实测收据 |
| --- | --- | --- | --- |
| `A \| B` | 两个独立进程 + 一根有容量上限的内核缓冲区 | 第 3、22 节 | 一次管道里四个不同 `$BASHPID`；12 万行时上游被堵住 |
| `… \| 改了一个变量` | 改的是子进程内存的复印件 | 第 3 节 | `v` 还是 7、`w` 是空 |
| `v=$(A \| B)` / `lastpipe` / 同段 `{ }` | 三条把结果接回本进程的路 | 第 4 节 | `v=[来自命令替换]`、`n=[3]`、`total=[ab]` |
| `$?` | **只有最后一段** | 第 5 节 | `cat 不存在 \| wc -l` → `$?` 0，`PIPESTATUS=[1 0]` |
| `${PIPESTATUS[@]}` | 每段各自的退出码；必须在紧跟的那条命令里读 | 第 5 节 | 段数 2/3 随管道长度变 |
| `set -o pipefail` | 只改整条管道的 `$?` = **最后一个非零**段；数组不变 | 第 6 节 | `[1 0]`→`$?` 1；`[0 1 0]`→1 |
| `cmd >f 2>&1` | 先 1→f，再把 2 复制成「1 此刻的指向」 | 第 7 节 | `ord1.txt` 2 行 |
| `cmd 2>&1 >f` | 先 2→屏幕（复制旧指向），再 1→f | 第 7 节 | `ord2.txt` 1 行 + 屏幕上一行 ERR |
| `cmd &>f` | bash 快捷键 = `>f 2>&1`；**dash 里语义完全不同** | 第 8 节 | bash 60 字节 vs dash 0 字节 |
| `n>&m` / `n<&m` | **复制指向**（dup），不是建立永久绑定 | 第 7~10 节 | `exec 3>&2` 留底再复原 |
| `>&-` / `<&-` | 让这个**编号**从进程表消失；之后所有命令都受影响 | 第 9 节 | 三句哑、一句响；无法恢复 |
| `exec >f`（不带命令） | 给**这个 shell** 换水管，直到再 exec 或进程结束 | 第 10 节 | 再 exec 一次是**截断**，`exec >>` 才追加 |
| `2>/dev/null` | 只改你**看得见**多少，不改成败 | 第 11 节 | 报错消失，退出码仍是 2 |
| `<<EOF` / `<<'EOF'` | 定界符加引号 = 正文不过一遍展开 | 第 12 节 | `$WHO` 展开 vs 原样 |
| `<<-` | **只剥行首 Tab**，空格一个都不剥 | 第 12 节 | `cat -A` 里 `^I` 的有无 |
| `<<< 词` | 只接**一个词**；末尾换行是 bash 补的；不做通配展开 | 第 12 节 | `cat <<< 一 二 三` 报两个文件不存在 |
| `tee f` | 两个出口各写一次（文件 + stdout） | 第 13 节 | `PIPESTATUS=[0 0 0]`（tee 占一段） |
| `tee /dev/stdout` | 「文件」和「stdout」是**同一个东西** → 重复 | 第 13 节 | 喂 1 行，`cat -n` 编到 2 |
| `cat f \| cmd` vs `cmd <f` | 差一个进程；段数可数 | 第 14 节 | 段数 2 vs 1 |
| `find \| xargs cmd` | 按**空白**分词，不是按行 | 第 15 节 | `two words.txt` 变两个参数 |
| `find -print0 \| xargs -0` | 按 **NUL** 分词 = 唯一安全写法 | 第 16 节 | 4 个名字各成一段 |
| 上游被读端关门砍死 | `SIGPIPE` → `128+13 = 141` | 第 17 节 | `yes \| head -1` → `[141 0]` |
| `kill` 一个后台任务 | 信号；退出码要 `wait` 才拿到 → `128+15 = 143` | 第 18 节 | `wait` 到 143 |
| `<( )` | 参数位置塞一个 `/dev/fd/N`（**匿名管道**） | 第 20 节 | `wc -c <(echo hi)` → `3 /dev/fd/63` |
| `< <( )` | 真接 stdin，给只吃 stdin 的命令用 | 第 20 节 | `read x <(…)` → not a valid identifier |
| `>( )` | 反方向，**异步**，bash 不等它 | 第 21 节 | 立刻读可能读到空 |
| 中间段 `sed`/`grep` | stdout 不是终端 → **块缓冲** | 第 22 节 | A 被推迟 350ms；行缓冲吞吐掉一个数量级 |
| `exec {v}>f` | bash 自己挑编号并回填进变量（10 往上） | 第 23 节 | 本轮给 24 |
| `{ …; } >f` vs `( … ) >f` | 不起新进程 vs 起子进程 | 第 24 节 | 组外 `v=[2]` vs 外面 `v=[1]` |
| `set -C` | `>` 拒绝覆盖；`>>`、`tee` 不受管 | 第 25 节 | `cannot overwrite existing file` |

### 三个数字家族（本章全部的退出码记账）

```text
命令自己返回的：       0 成功 / 1 通用失败 / 2 用法或参数错（grep、ls 常用）/ 127 命令不存在 / 126 不可执行
被信号杀死的：         128 + 信号号
   141 = 128 + 13 (SIGPIPE)  ← 写端没人接（第 17 节）
   143 = 128 + 15 (SIGTERM)  ← kill 默认信号（第 18 节）
   130 = 128 + 2  (SIGINT)   ← 你按了 Ctrl-C
管道整条的：           默认 = 最后一段；开 pipefail = 最后一个非零段（第 6 节）
```

---

## 一页纸（考前扫这半页）

```text
进程：A|B|C 每段都是独立进程；管道里改的变量外面看不见。认进程用 $BASHPID，不是 $$。
修法：能放左边放左边 v=$(…)；脚本里只要最后一段 shopt -s lastpipe（要求非交互，且只救最后一段）；
      复杂就把循环和用结果那句写进同一对 { }。
退出码：$? 只是管道最后一段。每段在 ${PIPESTATUS[@]} 里，必须在紧跟的那条命令里读。
pipefail：只改 $? = 最后一个非零段，数组不变。它和 head/grep -q/awk exit 打架 → || true 或局部 set +o。
顺序：重定向在同一行里从左到右逐个生效；2>&1 复制的是「1 号此刻的指向」。>f 2>&1 对，2>&1 >f 错。
&>：bash 快捷键，dash 里完全不同且不报错 → 跨 shell 一律写 >f 2>&1。
关 fd：>&- 是让编号消失，不是静音；之后所有命令一起哑且无法恢复。要静音用 >/dev/null。
留底：想恢复就得先 dup（exec 9>&1 … exec 1>&9 9>&-）。exec 不带命令 = 给本 shell 换水管，持久且再 > 会截断。
/dev/null：藏掉原因，藏不住失败（退出码一点没变）。写 2>/dev/null 就必须顺手写 || 补救。
here-doc：定界符加引号 = 不展开；<<- 只剥 Tab；终止符必须顶格，多一个空格一路吞到文件尾且不报错。
here-string：<<< 只接一个词，多一个词掉下去变成参数；末尾换行是 bash 补的；不做通配展开。
tee：两个出口各写一次；tee /dev/stdout 会重复（两个出口是同一条管道）；tee 自己占管道一段。
UUOC：cat f|cmd → 多一个进程（段数可数）。但拼多个文件成流、或命令只吃 stdin 时 cat 是必需品。
文件名跨进程：find -print0 | xargs -0，没有第二种安全写法。xargs 默认按空白切，不是按行。
信号：141=128+13 SIGPIPE（提前收工的下游会砍上游），143=128+15（kill 后台），130=128+2（Ctrl-C）。
kill 只是发信号，退出码要 wait。
<( )：给的是 /dev/fd/N 这个名字（匿名管道，不能 seek、不是磁盘文件），只能用给吃文件名的命令；
      只吃 stdin 的写 < <( )。>( ) 是异步的，下一行立刻读不保证有内容。
缓冲：中间段 stdout 不是终端 → 块缓冲 → tail -f|grep 不实时。行缓冲三把钥匙：stdbuf -oL、sed -u、
      grep --line-buffered；代价是每行一次 write，吞吐掉一个数量级。管道有容量，写满就堵上游。
私有 fd：exec {v}<文件 → bash 自己挑编号回填进变量；用 read -u "$v" / >&"$v"，关 exec {v}<&-。
分组：{ } 不起新进程（变量留住），( ) 起子 shell（隔离脏改动）。$() 本身也是子 shell。
noclobber：set -C 只拦 >（和 >&），不拦 >>，也管不住自己 open() 的 tee。
```

---

## 这章的坑（按代价从大到小）

1. **管道右侧那个循环/赋值跑在另一个进程里**（第 3、4 节）：`cat f | while read x; do n=$((n+1)); done`
   之后 `n` 是**空**（不是 0 —— 那个变量在本 shell 里从来没被赋过值）。**不报错，只是数字没了。**
   这是本章代价最大的一条：它通常出现在「统计行数/累加金额」的脚本里，跑起来安静得可怕。
2. **`2>&1` 写在 `>f` 前面**（第 7 节）：`cmd 2>&1 >f` 让报错**照样刷满屏幕**、文件里只有一半。
   判据是 `wc -l`：本机 `ord1.txt` 2 行、`ord2.txt` 1 行。规则只有一句：**复制的是那一刻的指向**。
3. **`find | xargs rm` 按行/按空白传名字**（第 15、16 节）：切碎出来的片段如果**正好撞上真实文件**就是无声误删。
   现场：`rm` 去删 `weird2/tab`、`name.txt` 这些不存在的名字，而带空格的那个**一个都没删掉**。
   唯一解：`-print0 | xargs -0`。
4. **`pipefail` + 提前收工的下游**（第 17、19 节）：`set -euo pipefail` 之下 `yes | head -1` 把脚本杀在 141，
   或让一个成功的 `head` 走进 `else`。`... | head -1` 是全世界脚本里最常见的写法之一，这条几乎人人会撞。
5. **`exec >f` 忘了改回来 / `exec 1>&-`**（第 9、10 节）：整个 shell 后面所有输出进文件或被丢掉；
   **`>&-` 关掉之后无法恢复**（没留底就找不回原指向）。现场是「脚本后半段什么都没打印，我以为它没跑」。
6. **`2>/dev/null` 把原因吞了**（第 11 节）：退出码一点没变（本机两条都是 `2`），**只有原因消失了**。
   排障时最费时间的就是这种「知道坏了、不知道为什么」。
7. **`&>` 在 `#!/bin/sh` 里**（第 8 节）：dash 不报错，只是把你的日志写成 **0 字节**。
   **静默错误 + 跨 shell 脚本**是最坏组合。写 `>f 2>&1`。
8. **`tee /dev/stdout` 想再打一遍**（第 13 节）：下游收到 **2 份**。
   更狠的是本章现场踩到的那次：把整个 demo 重定向进文件时 `tee /dev/stderr` 带 `O_TRUNC`，
   **把捕获文件从头截断**。规则：动 `/dev/std*` 之前先问「它此刻是谁」。
9. **`$?` 当成整条管道的成败**（第 5 节）：`cat 不存在 | wc -l` → `0`。中间段的失败**一概不问**。
   而且 `PIPESTATUS` 必须在紧跟的那条命令里读，插一个 `echo` 就变成一位数组。
10. **`grep -q` 让上游死**当成定律（第 5、17 节）：本机三格分别是 `0`、`141`、`0`，
    取决于命中那一刻上游还在不在写。**恒真的判据不存在**，这条要当「可能会」来记。
11. **`read x <(...)`**（第 20 节）：`not a valid identifier` —— `<( )` 给的是**参数**，`read` 要的是 stdin，
    得写 `read x < <( )`。少一个空格，语义完全不同。
12. **`tail -f | grep` 不实时以为是 grep 慢**（第 22 节）：是中间段**块缓冲**。
    解药 `--line-buffered` 有代价：本机 20 万行 15ms → 241ms（一个数量级）。
13. **`>( )` 当同步用**（第 21 节）：bash 不等那个进程，下一行立刻读可能读到空。
14. **`{ }` 与 `( )` 记反**（第 24 节）：`( )` 里的赋值出不来，`{ }` 里的能出来。
    还有那条语法：`{` 后要有空格、里面最后一条要有 `;`、`}` 要单独成词。
15. **`<<<` 后面写了好几个词**（第 12 节）：只有第一个进 stdin，其余变成**命令的参数**——
    `cat <<< 一 二 三` 打印了「一」了吗？**没有**，它因为有文件名参数就不读 stdin 了，直接报两个文件不存在。
16. **`noclobber` 当安全机制**（第 25 节）：`>>` 和 `tee` 都不受管。它只挡 `>` 这一只手滑。
17. **`exec $fd>&-`**（第 23 节）：那是「执行一个叫 24 的命令」。自动分配的编号要写 `exec {v}>&-`、
    使用时写 `>&"$v"`。

---

## 自检问题

1. `echo x | read w` 之后 `w` 是什么？用哪两个变量能**亲眼**证明管道每一段是独立进程？（第 3 节）
2. `lastpipe` 的两个前提是什么？为什么在交互终端里「加了等于没加」？（第 4 节）
3. `cat 不存在.txt | wc -l` 的 `$?` 是多少？`PIPESTATUS` 是什么？为什么必须在紧跟的那条命令里读它？（第 5 节）
4. 开了 `pipefail` 之后，`PIPESTATUS` 数组本身变不变？多段非零时 `$?` 取哪一段？（第 6 节）
5. 不看书写出 `cmd >f 2>&1` 和 `cmd 2>&1 >f` 各自的结果差别，并说出判据（本章用的是什么数字）。（第 7 节）
6. `&>f` 在 dash 里会发生什么？它是「报错」还是「静默给你不一样的文件」？（第 8 节）
7. `exec 1>&-` 和 `exec >/dev/null` 的区别是什么？哪一个还能恢复？怎么恢复？（第 9、10 节）
8. `2>/dev/null` 会不会改变命令的退出码？给出本章的实测数字。（第 11 节）
9. `<<-EOF` 能剥掉行首的两个空格吗？终止符前面多一个空格的后果是什么？会报错吗？（第 12 节）
10. `cat <<< 一 二 三` 打印了什么？为什么？（第 12 节）
11. `printf 一行 | tee /dev/stdout | wc -l` 数到几？解释为什么。（第 13 节）
12. `cat f | wc -l` 和 `wc -l <f` 用 `PIPESTATUS` 量出来的段数各是多少？`cat` 什么时候不是废物？（第 14 节）
13. `xargs` 默认按什么分词？`-d "\n"` 修好了哪种名字、还剩哪种没修好？（第 15 节）
14. 传文件名的唯一安全写法是什么？为什么「按行传」本身就不安全？（第 16 节）
15. `141` 是什么信号？写出它的算式。`yes | head -1` 和 `seq 1 200000 | tail -1` 谁的上游会挨刀，判据是什么？（第 17 节）
16. `kill` 一个后台任务之后，怎么拿到它的退出码？该用哪个变量记住 pid？（第 18 节）
17. `set -e -o pipefail` 的脚本里 `yes | head -1 >/dev/null` 会怎样？给出三种解法。（第 19 节）
18. `wc -c <(echo hi)` 打印的文件名是什么？它是磁盘上的文件吗？两条后果是什么？（第 20 节）
19. `read x <(echo hi)` 为什么报 `not a valid identifier`？正确写法是什么？（第 20 节）
20. `<( )` 和 `>( )` 在「同步/异步」上有什么区别？（第 21 节）
21. `tail -f f | grep x` 不实时，根源在哪里？三把「行缓冲」钥匙分别属于谁？代价量级是多少？（第 22 节）
22. 「小数据上游 4ms 写完、12 万行上游 609ms 才写完」这组对照说明了什么？（第 22 节）
23. 写出私有 fd 的完整生命周期（开、用、关）三种写法。`exec $fd>&-` 为什么危险？（第 23 节）
24. `{ …; } >f` 和 `( … ) >f` 的差别在哪个变量上？`$( { …; } )` 里改变量为什么出不来？（第 24 节）
25. `set -C` 之下 `>`、`>>`、`>&f`、`tee` 四种写法各自的后果？（第 25 节）

---

## 本章没演 / 没验证（绝不编造，都在这）

| 想写但没在本机证明的事 | 状态 |
| --- | --- |
| BSD/macOS 那一套 `find`/`xargs`（无 `-d`，`-print0` 行为细节不同） | 本机没有，**未验证**。正文只标注了「会翻脸」，没给结论 |
| `zsh` 的同名机制（`$pipestatus` 小写、进程替换的编号、`sh` 兼容模式） | 没演，本机只有 bash 5.3.9 和 dash 0.5.12 |
| `coproc`（bash 协同进程，双向管道） | 没演 |
| 命名管道 `mkfifo`、`/dev/fd` 之外的高级用法 | 没演（第 20 节只碰了匿名管道） |
| `flock` 文件锁与重定向的组合（`9>lock` + `flock 9`） | 没演 |
| `read` 的 `-n`/`-t`/`-d`/`-N` 各参数、`IFS` 对 `read` 的切分影响 | 没演，留给第 05 章 |
| `select`/`poll`/`epoll` 级别的多路复用（谁先可读） | 没演，超出本章范围 |
| 管道默认容量的**具体字节数**（本机只量到「小数据瞬间写完、12 万行被堵住」的形状） | 未量，只给了 `/proc/sys/fs/pipe-max-size` = 1MB 这个**上限** |
| `stdbuf` 对静态链接/非 glibc 程序是否生效（它靠 `LD_PRELOAD`） | 未验证 |
| 行缓冲对吞吐的影响只在**本机这一批数据**上量过一次（15ms vs 241ms） | 单次测量，不当常数 |
| `O_TRUNC` 截断捕获文件那次只用 `{ }` 复现了现象，没去读 `tee` 源码确认 `open` 标志 | 现象实测、原因按 `open(2)` 常规语义解释，**没在代码层验证** |
| `sed -u`、`grep --line-buffered` 之外的工具（`awk` 的 `fflush()`、`python -u`） | 没演 |
| `SIGPIPE` 被 `trap` 捕获后的行为差异（bash 内建 vs 外部命令） | 没演（第 18 节只演了 `trap EXIT`） |
| 信号 129~130 之外那一族（`SIGHUP`、`SIGQUIT`=131 之类） | 没演，本机只用 `kill -l` 现查了 2/13/15 三个 |
| `netstat`/`lsof` 看 fd 与管道对端（`ss` 只看了监听表） | 没演 |
| 第 1 节的中文报错只演了 `command not found` 一条；`Bad file descriptor` 的中文译名**没在本机验证** | 未验证，正文只说「跟着语言环境走」 |
| 上面所有 PID、毫秒、fd 编号、监听端口、时间戳在你机器上的复现 | 不保证。它们每次跑都变，本章只把**量级、分组、方向**当结论 |

---

## 怎么跑这一章

```text
cd bash-handbook/04-pipes-and-redirection
bash demo.sh                # 全跑一遍，26 节，退出码 0，约 4 秒
bash demo.sh --keep         # 跑完保留沙箱并把目录名打给你，进去自己接着敲
```

想只看某一节：

```text
bash demo.sh 2>&1 | awk '/^══════ 第 17 节/,/^══════ 第 18 节/'
```

本章一共 6 个文件：`demo.sh` + 五个辅助脚本 ——
`execsave.sh`（第 10 节，会把 stdout 带走，所以必须独立成文件）、
`heredoc.sh` / `herestring.sh`（第 12 节，`<<-` 需要源码里真实的 Tab，终止符事故只有独立文件敢演）、
`mkweird.sh`（第 15~16 节，造带空格/换行/Tab 的文件名，只用 `mktemp` 出来的目录）、
`slowpipe.sh`（第 22 节，缓冲时序实验，毫秒全走 stderr 打印）。

`demo.sh` 的行为约定（红线，代码里都写着）：

- 所有写操作只进 `mktemp -d "${TMPDIR:-/tmp}/bash04-XXXXXX"`；`trap cleanup EXIT` **只删自己建的那一个**。
  源文件被整份复制进 `$SB/work` 再运行，所以章目录里不会有任何产物（第 26 节 `find` 自证）。
- **不写、不改、不删仓库里任何其他文件**；`bash-handbook/README.md` 的目录树由主 agent 维护。
- **不起任何监听端口、不联网**；唯一的后台进程是几条 `sleep`，全部 `$!` 记录并当场 `kill`/`wait` 或 `trap EXIT` 收尾。
- 环境：`export LC_ALL=C.UTF-8 LANG=C.UTF-8 LANGUAGE=C`（报错原文可预测，第 1 节实测对比），
  `unset` 掉会影响 bash 行为的 `BASH_ENV`、`PROMPT_COMMAND`、`CDPATH`，
  并保存/恢复 `ORIG_LANG`/`ORIG_LC_ALL` 以便把「你原来的终端语言」打出来。
- **故意不加 `set -e`**：本章一半以上是「注定失败」的现场（`Bad file descriptor`、`cannot overwrite existing file`、
  `not a valid identifier`、`No such file or directory`、141/143 退出码…），加了就只能看到第一段。
  需要演示 `set -e` 本身时（第 6、19 节），是在**那条命令自己的 `bash -c` 里**临时开，不外溢。
- 每条命令都是**全新的 `bash -c`**，所以 `shopt`/`set -o`/`set -C` 这类开关不会串到下一节（第 6、25 节各自说明了为什么）。
- 每段前打 `$ 命令`、段末打 `[退出码 N]`，非零也继续跑；最后打 `[全部跑完]` 和 `[沙箱已删除]`。
- 只在 `[ -t 1 ]`（真的接到终端）时上色，所以 `bash demo.sh > out.txt` 拿到的是干净纯文本。
- **一次现场修正**：第 13 节那条 `tee /dev/stderr` 原本会把整份捕获文件从头部截断（`O_TRUNC` +
  「`/dev/stderr` 此刻就是那个文件」），所以改成把两个出口关进沙箱文件 `{ …; } > o.txt 2> e.txt`。
  现象一字未变，捕获不再受害 —— 围栏里读到的就是改过之后的版本。
- **复核引文**：本章 README 的每条输出都由 `@@R 起-止@@` 占位符**从定稿捕获里机械搬出**，
  所以**捕获文件定稿后不能再重跑 `demo.sh`**（重跑行号会漂，占位符指向的内容就错了）。
  要校验：`python3 ~/.local/bin/expand_fences.py README.md ~/.cache/handbook-captures/bash04.txt`
  → `python3 ~/.local/bin/verify_quotes.py README.md ~/.cache/handbook-captures/bash04.txt '/tmp/bash04-[A-Za-z0-9]{6}'`
  → 期望末行 `未匹配行 0`。

---

## 术语小抄

| 术语 | 一句话解释（本章的用法） |
| --- | --- |
| 段 (segment) | 管道里的一条命令。每段一个独立进程，`PIPESTATUS` 一位数组元素 |
| fd / 文件描述符 | 进程私有的小整数编号，每一格指向一个「打开的东西」：终端、磁盘文件、管道、设备 |
| 0 / 1 / 2 | stdin / stdout / stderr，每个进程出生就有的三根公用水管 |
| 私有 fd | 3 号以上，自己接的水管：`exec 3<文件` 或 `exec {v}<文件`（bash 挑编号） |
| dup（复制指向） | `n>&m` 的本质：把 m 此刻的指向**复制**给 n。复制完两者独立，之后各走各的 |
| 留底 | 改 1 号之前先 `exec 9>&1`，把原指向存到 9 号。没有留底就没有「恢复」 |
| 截断 (truncate) | `>f` 的动作之一：把 f 写成 0 字节再写。`>>` 才是追加；`tee` 默认也带 `O_TRUNC` |
| `2>&1` | 「把 2 复制成 1 此刻的指向」= 并流。写在 `>f` 之后才跟文件走 |
| `&>` / `&>>` | bash 快捷键，等价 `>f 2>&1` / `>>f 2>&1`。dash 里是另一回事 |
| `/dev/null` | 写多少都不留、一读就 EOF 的设备。只影响你**看得见**多少，不影响成败 |
| `/dev/stdout`、`/dev/stderr` | 「此刻这个 fd 指向的那个东西」的别名。在管道/重定向里它就是那条管道或那个文件 |
| `PIPESTATUS` |  bash 数组：每段各自的退出码。只能在**紧跟**的那条命令里读 |
| `pipefail` | 让整条管道的 `$?` = 最后一个非零段。不改变数组本身 |
| 均摊 / 段数探针 | `${#PIPESTATUS[@]}` 顺手量「这条管道跑了几段」，第 14 节用它证明 UUOC |
| here-doc / here-string | `<<EOF` 多行进 stdin；`<<< 词` 一个词进 stdin（末尾换行由 bash 补） |
| 定界符加引号 | `<<'EOF'`：正文一过都不展开。等价于把整段用单引号包住 |
| UUOC | Useless Use of cat：`cat f \| cmd` 该写 `cmd <f`，差一个进程 |
| NUL 分隔 | `-print0` / `xargs -0`：唯一能装下「名字里有空白」的分隔方式 |
| `SIGPIPE` | 读端已关还在写 → 内核终止写端。退出码 141 = 128+13 |
| `$!` / `wait` | 最后一个后台任务的 pid / 拿它的退出码（`kill` 只发信号，不返回退出码） |
| `trap EXIT` | 「这个 shell 无论怎么结束都执行这一段」，收尾后台进程和清理现场用 |
| 进程替换 | `<(cmd)` 把输出挂到 `/dev/fd/N` 并把这个**名字**塞进参数位；`>(cmd)` 是反方向且**异步** |
| 匿名管道 | `/dev/fd/N` 背后的东西：不能 seek、不是磁盘文件、没有 inode 大小 |
| 块缓冲 / 行缓冲 | stdout 目标是终端→行缓冲；是文件或管道→块缓冲（攒满才吐）。行缓冲=每行一次 write |
| `stdbuf -oL` | 通用包装，用 `LD_PRELOAD` 改 C 库缓冲策略；`sed -u`、`grep --line-buffered` 是各家自带开关 |
| noclobber | `set -C`：`>` 只许新建不许覆盖，报 `cannot overwrite existing file`；`>\|` 强绕，`>>` 与 `tee` 不受管 |

---

## 本章之后

- **第 05 章（变量、`read` 与循环）**：本章第 23 节那根「私有水龙头」（`exec {v}<文件` + `read -u "$v"`）
  是那里的主角 —— stdin 被管道占了之后怎么同时读两个来源，以及 `IFS`、`read -r` 的切分规则。
- **第 06 章（函数与脚本结构）**：本章第 10 节「`exec` 换水管会影响整个 shell」在函数里怎么隔离，
  以及 `local`、`return`、`$BASH_SOURCE` 那一层。
- **第 07 章（`set -e` 与错误处理）**：第 6、19 节已经把账单摆在桌上了 ——
  `set -e` 看 `pipefail` 交上来的非零就杀脚本、`if` 里的命令不受追杀。那一章结清整套规则。
- **第 08 章（作业控制与信号）**：第 17、18 节的 `128+信号号` 只用了 2/13/15 三个，
  `SIGHUP`、`nohup`、`disown`、进程组与 Ctrl-C 的广播都在那里。
- **第 09 章（工具与 `shellcheck`）**：本章 `shellcheck` 本机没有（第 2 节实测点名），
  那一章讲「没装时怎么活」以及 `strace`/`lsof` 看 fd 的手段。
- **回到你的 C 数据结构**：`04-queue` 里你自己写过「有容量上限的环形缓冲区，写满就堵」——
  本章第 22 节量到的「12 万行时上游被管道堵住、时间差归零」就是同一个模型在内核里的样子；
  第 17 节的 SIGPIPE 则是「往空队列里再塞元素被系统打断」的现实版本。

**本章回指**：第 01 章「`/bin/sh` 是 dash」在第 8 节兑现成 0 字节的日志；
第 02 章的「单引号跳过展开」在第 12 节的定界符上重演了一次，`$'...'` 引号在 `-t` 的输出里现出原形；
第 03 章的「`if` 里的命令不被 `set -e` 追杀」在第 19 节解法②里救了一次脚本。
