# Bash 大白话手册

> 一本「先跑起来、再懂为什么」的 bash 手册。每一章都是：**你遇到了什么现场 → 该敲哪几条命令 →
> 屏幕上真实出现了什么 → 背后的机制是什么**。
> 全书所有输出都是这台机器（Ubuntu 26.04 LTS + bash 5.3.9 + dash 0.5.12）上真跑出来的，没有一行是手写的。

---

## 这本手册解决什么问题

绝大多数人 bash 卡住，不是因为「记不住选项」，而是因为**脑子里没有一张正确的运行模型图**。典型现场：

| 你的现场 | 真正的原因（本手册的哪一章） |
|---|---|
| `./x.sh` 报 `Permission denied`，`bash x.sh` 却能跑 | 执行权限 + shebang（01） |
| 脚本里明明改了变量，一出管道就没了 | 管道每一段都是子 shell（01） |
| `sh x.sh` 报错、`bash x.sh` 正常 —— 以为 `sh` 就是 bash | `/bin/sh` 在本机是 **dash**（01） |
| `$*` 和 `$@` 看着一样，加引号后行为不同 | 词分割与 IFS（02） |
| `[ "$x" = yes ]` 在 `x` 带空格时炸掉 | `[ ]` 是命令，不是语法（03） |
| 管道前半段失败了，`echo 成功` 照样打印 | 默认只看最后一段退出码（04） |
| `for f in *.tmp` 遇到带空格的文件名就散架 | 通配与 IFS（02、05） |
| 函数 `return 1` 之后外面拿不到结果 | bash 函数只能返回退出码或输出（06） |
| 脚本半夜跑挂，第二天不知道死在哪一行 | `set -euo pipefail` + `trap`（08、09） |
| `sed -i` 把文件改坏了，备份都没留 | 三剑客的危险开关（07） |
| 终端里好用的 `alias`，写进 `.sh` 完全不生效 | 非交互 shell 不读 rc（01） |

一句话：**bash 是一门「先展开、再执行」的语言，而几乎所有坑都来自「你以为在 A 层，其实在 B 层」**
（哪个进程、哪个解释器、哪一层展开）。所以本手册的章节顺序就是按层的复杂度排的。

---

## 全书章节地图

```text
01 shell-vs-script    交互 shell / 脚本 / 子 shell：谁在跑、谁看得见谁          ← 地基，必须先跑    ✅ 已交付
     ↓
02 words-and-expansion  一行命令被切成几段：引号、${}、IFS、通配               ← bash 的「语法层」  ✅ 已交付
     ↓
03 testing-and-exit-codes  判断与退出码：[ ] vs [[ ]]、&& || !                ← 「决策层」        ✅ 已交付
04 pipes-and-redirection   数据怎么流：管道、fd 重定向、here-doc、进程替换      ← 「流动层」        ✅ 已交付
     ↓
05 loops-and-control    for/while/until/select + read 的坑                     ← 「重复层」        ✅ 已交付
06 functions-and-returns   函数「返回值」的真相、local、递归、信号               ← 「组织层」        ✅ 已交付
     ↓
07 text-three-musketeers   grep / sed / awk 实战：单位、方言、码                ← 外部工具          ✅ 已交付
08 robust-scripts    set -euo pipefail、trap、mktemp、getopts、锁与 TOCTOU        ← 写成能上生产      ✅ 已交付
09 when-things-break  bash -x/PS4/BASH_XTRACEFD、trap DEBUG、-n/-u、time、strace   ← 出事怎么查        ✅ 已交付
     ↓
10 real-recipes      批量重命名 / 日志轮转 / 目录校验 / 把交互操作固化成脚本      ← 综合演练          ✅ 已交付
11 cheatsheet        全书速查：按场景查写法，每行带出处 + 一页纸 22 条            ← 日常翻            ✅ 已交付
```

依赖关系是硬性的：**02 必须在 03/04 之前**（不先讲清「展开」，`[ ]` 和重定向的报错无法解释）；
**01 必须在所有之前**（不先建立「子 shell」概念，`set -e`、管道、变量可见性全是玄学）。

### 为什么保持 11 章、没有增删

按规划表落地，只在两处做了**归并**，理由写在这里：

1. **作业控制（`&`、`jobs`、`wait`、`disown`、`kill`）不单开一章**，拆给 **05**
   （`until` 等待 + 后台作业的循环写法）和 **08**（并发锁、幂等）。
   理由：这几条命令单独讲会变成「进程管理手册」；它们真正的用途是「脚本里等一个活干完」，
   和循环、健壮性绑在一起才有意义。另外本手册的安全红线要求后台作业必须收尾，不适合当独立练习章。
2. **数组 / 关联数组不单开一章**，放进 **02**。
   理由：数组的坑 90% 是展开的坑（`"${arr[@]}"` 为什么必须带引号、`set -u` 下空数组怎么炸），
   和引号、IFS 讲在同一节里才不会重复两遍。

其余章节按原计划。第 09 章确实受了本机环境限制（**这台机器没有 `shellcheck`**，见下面「本机环境」），
交付时的处理是：**不演、不编、不抄别人的输出** —— 静态检查那一层全用 bash 自己做得动的三件套
（`-n`、`-o noexec`、`-u`）实测，`shellcheck` 在本章只出现在「它到底在不在」那两处点名里
（第 1 节、第 9 节第 4 格的同族工具在场名单），**一次都没被真正调用过**，
并在章尾「本章没演」那节里点名它连**安装途径都没验证过**（本章没跑过任何安装命令）。

---

## 怎么用

每一章一个目录，里面固定是：**若干 `.sh` 示例 + 一份 `demo.sh` + 一份 `README.md`**。

```bash
cd bash-handbook/01-shell-vs-script
bash demo.sh              # 按 README 小节顺序把全章证据跑一遍，跑完自动删沙箱
bash demo.sh --keep       # 保留沙箱并打印路径，进去自己接着敲
bash demo.sh > out.txt    # 重定向时自动关颜色，得到和 README 一模一样的纯文本
```

`demo.sh` 的设计约定（全书统一）：

- 每段先打 `$ 你要敲的命令`，再打真实输出，最后打 `[退出码 N]` —— 退出码也是教学内容。
- 所有写操作只发生在 `mktemp -d` 沙箱里，默认跑完 `rm -rf` 掉（只删自己刚建的那一个）。
- **故意不加 `set -e`**：第 01～03 章要专门演示失败现场，加了 `-e` 会在第一个错误就退出，
  屏幕上就看不到「报错原文 + 后面继续跑」的完整因果。第 08 章再讲为什么生产脚本必须加。
- `export LC_ALL=C.UTF-8` 固定语言 —— 这台机器默认是 `zh_CN.UTF-8`，bash 的报错会变成中文，
  README 就无法逐行核对了（这个差异本身在第 01 章场景 1 有现场）。

看书的顺序建议：**跑 demo → 对着 README 找「哦，这行是这个意思」→ 做章末自检问题**。
不要先读 README 再跑 demo，会失去「亲眼看到」的效果。

---

## 本机环境（全部是实跑输出，抄自 `bash demo.sh` 第 0 节）

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
[退出码 0]

$ echo "登录 shell 环境变量 \$SHELL = [$SHELL]"
登录 shell 环境变量 $SHELL = [/bin/bash]
[退出码 0]

$ /bin/sh --version 2>&1 | head -1
/bin/sh: 0: Illegal option --
[退出码 0]

$ type -a echo
echo is a shell builtin
echo is /usr/bin/echo
echo is /bin/echo
[退出码 0]

$ shopt | head
array_expand_once   	off
assoc_expand_once   	off
autocd              	off
bash_source_fullpath	off
cdable_vars         	off
cdspell             	off
checkhash           	off
checkjobs           	off
checkwinsize        	on
cmdhist             	on
[退出码 0]

$ shopt | wc -l
59
[退出码 0]
```

外部命令「由谁实现」同样是现场探测出来的，不是猜的（也抄自第 0 节）：

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
[退出码 0]
```

连 `echo` 这种最基础的命令，本机三个实现给的都不是同一个结果：

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

逐条说明它决定了你能怎么写：

| 事实 | 后果 |
|---|---|
| `bash` 是 **5.3.9**（发行版包 5.3-2ubuntu1） | `mapfile -d`、`${var@Q}`、`&>`、`**` globstar、`EPOCHREALTIME` 都能用 |
| **`/bin/sh -> dash`**（0.5.12-12ubuntu3，描述里写明 `POSIX-compliant shell`） | `sh x.sh` 会掉进 POSIX shell：数组、`[[ ]]`、`<<<`、`$'..'` 全部报错。第 01 章有完整现场 |
| `dash` 不认 `--version` | 那句 `Illegal option --` 就是「`sh` 不是 bash」的物证，不用查文档 |
| **`dash` 认 `PS4` 这个变量**（第 09 章实测，推翻本章早期说法） | `PS4='〔行号〕' sh -x f` 的首行前缀真的变成了 `〔行号〕a=1`；可 `PS4` 里塞 `${LINENO}`/`${FUNCNAME}`/`${BASH_SOURCE}` 全是**空串**，`BASH_XTRACEFD` 对它一点作用都没有。所以「换 shell 之后 trace 变了」不是「PS4 没了」，是**壳还在、名字空了** —— 不失败的降级比失败难查 |
| `dash` 里没有 `trap … DEBUG`、没有 `set -T`、`time` **不是关键字** | 前两条当场报错（`trap: DEBUG: bad trap` / `Illegal option -T`）；第三条最阴：`time :` 被当成「找一个叫 time 的程序」，找到 `/usr/bin/time` 却因为没有这个文件而 `cannot run` |
| 本机 `hostname` 的输出与 `$USER` **是同一个串** | 脱敏时「用户名」和「主机名」这两条必须**各自**折成 `<用户>`／`<主机名>`，不能指望一条正则替另一条兜底 —— 第 09 章的隐私扫描就是按四个占位符分别数的 |
| `$SHELL` 是 `/bin/bash` | 注意：`$SHELL` 只是「登录时用的那个 shell」这个**环境变量**，不代表当前进程。它骗了很多人 |
| `shopt` 一共 **59** 个开关 | 同一个脚本在不同开关下行为不同（`nullglob`、`lastpipe`、`expand_aliases` 是三大坑位） |
| 系统 Ubuntu 26.04 LTS，但 `ls/env/tr/date/wc/head/tail/sort/seq/uniq/cut/paste/sleep/timeout/stdbuf` 来自 **uutils coreutils 0.8.0**（Rust 实现），不是 GNU coreutils；而 `find`/`xargs` 是 **GNU findutils 4.10.0**，`grep` 3.12、`sed` 4.9 都是 GNU，`awk` 是 mawk | 「GNU 专属所以本机没有」这条直觉在这儿不好使：当场实测 `date -d`、`find -printf`、`sort -R --random-source` 都能跑。真会咬人的是 `wc -l` 数**换行符个数**、`tr` 按**字节**切中文。所以每章都按「先探一次再用」的规矩写，第 01 章已实测本机 `ls` 输出与 GNU 版一致 |
| `sed`/`grep` 是 GNU（4.9 / 3.12），`awk` 却是 **mawk 1.3.4** 而不是 gawk | `sed -i`、`grep -oP`（PCRE）可用；但 gawk 独有特性（`gensub`、`asort`、`BEGINFILE`）不能假设存在，第 07 章按 mawk 的实际能力选型 |
| `echo` 三个实现行为不同 | bash 内建 `echo` **默认不**解释 `\n`（要 `-e`），dash 内建默认解释，`/usr/bin/echo`（uutils）又不解释。**全书脚本一律用 `printf`**，这条在第 01 章场景 1 有现场对比 |
| 默认 locale 是 `zh_CN.UTF-8` | bash 报错默认是**中文**（例如 `未找到命令`）。手册里所有英文输出都来自显式 `LC_ALL=C.UTF-8` |

### 相关工具在场情况（这决定第 09 章能演示什么，**本次一个都没装**）

```
$ command -v shellcheck || echo "shellcheck: 本机没有（第 09 章给出安装途径，本次没有安装）"
shellcheck: 本机没有（第 09 章给出安装途径，本次没有安装）
[退出码 0]

$ command -v xdg-open strace
/usr/bin/xdg-open
/usr/bin/strace
[退出码 0]
```

- **`shellcheck`：本机没有。** 第 09 章已按这条交付：全章**没有一个 shellcheck 输出**，
  也不许你照抄别人的输出；静态那一层改用 `bash -n` / `-o noexec` / `-u` 三件套（都有实测）。
  安装途径（`sudo apt install shellcheck`，或从 GitHub Release 下载静态二进制）**本身也标成「本机未验证」**——
  本章没跑过任何安装命令，包名、源、装完能不能跑都没验过，按自己发行版的方式装、装完自己判。
- **`strace`：有（6.19）**，第 09 章真跑了 26 处，全部是 `strace ... bash -c '...'` 这种**自己起、自己等**的形状；
  **`-p`（挂别人的进程）一次没用过**，章里为此配了一个先喂假违规、再报真实值的计数器（第 18 节 ②，分子 0／分母 26）。
  可复现性是分层的：单进程日志抹干净能**逐行对拍**，带 `-f` 的多进程日志**天生不可复现**，
  能进 CI 的只有**调用行计数**与**存在性**（第 13 节 9）。
- **GNU `Time`：本机包没把版本号编进去**（`/usr/bin/time` 打印的版本串是 `UNKNOWN`），
  所以本章只报「本机这一份认哪些格式字母」，不写成「GNU Time 都这样」。
- 因此全书有**两支章宿主 + 三支取证脚本**带真实测量的抖点，承载它们的脚本顶部各写了一行豁免标记
  `# CHECK: RUN=非复现 理由：…`：第 09 章是 `timing.sh`、`tracewho.sh`、`demo.sh`，
  第 10 章是 `logrotate.sh` 和它的宿主 `demo.sh`（后者的抖动**全部**来自它并进整章的第 5 节）。
  **这行标记只免「两遍逐字相同」这一条**：rc、残留文件数、隐私扫描、守卫能否弄响、每格自己的断言照旧从严 ——
  它不是「这一格不用判」的许可证。
  第 10 章还把「不复现到底有多大」量到行号：整份捕获连跑两遍只有 5 行不同（3 行 pid、2 行竞态末值），
  于是该章的对拍口径写成**「两遍 diff 的差异行必须全部落在豁免清单里」** ——
  豁免必须**能被违反**，写成「这一章不用逐字比」就和没写一样。
  反过来，标着 `RUN=复现` 的脚本（第 10 章 `rename.sh`、`dircheck.sh`）就真的一行都不许变；
  而**什么标记都不写**（第 10 章 `interact.sh`）意思是**不豁免、照旧从严**，不是「忘了」。
- **`xdg-open`：有**（`/usr/bin/xdg-open`），但它是「用桌面默认程序打开文件」的命令，无桌面会话时会失败。
  这条原先写的是「第 10 章会在『跑完自动打开结果』的场景里提一句，并把失败输出如实贴出来」——
  第 10 章交付时没有兑现，因为它对得上该章的取舍：**一台没有桌面会话的机器上贴一条失败输出不算证据**。
  现在改成实话：`xdg-open` 在全书只出现在第 01 章 demo 的一条 `command -v xdg-open strace` 在场探测里，
  那一格的输出连第 01 章 README 都没引用，**没有任何一章真的跑过它**。
- 另外实测 `command -v ltrace` 无输出（这条是写第 09 章之前单独探测的，不在 demo.sh 里）。
  第 09 章自己跑的在场名单（第 9 节第 4 格）把同一族工具一并问了：
  `shellcheck=无 shfmt=无 ltrace=无 valgrind=无 strace=/usr/bin/strace gdb=/usr/bin/gdb` ——
  所以章里没有格式化对照、没有库调用层（`ltrace`）对照、没有任何内存错误的实测证据；
  `gdb` 虽然**在场**但本章一次都没打开它，「断点感」只给了 DEBUG 陷阱 + `extdebug` 这一种土办法。

---

## 全书安全约定（每一章的 `demo.sh` 都遵守，不是摆设）

1. **绝不碰用户的配置。** 不改 `~/.bashrc`、`~/.bash_profile`、`~/.profile`、`~/.config`，不改 `/etc` 下任何东西。
   需要「rc 文件生效」的效果时，一律在沙箱里写一个临时 rc，用 `bash --rcfile <沙箱文件>` 演示
   （第 01 章场景 9 就是这么做的）。
2. **所有写操作只发生在 `mktemp -d` 沙箱。** `rm -rf` 只允许作用于 `demo.sh` 自己刚创建的那一个目录，
   绝不作用于任何已存在的路径；`--keep` 时会打印沙箱路径并跳过删除。
   手册目录里那些 `.sh` 示例**永远是被复制进沙箱后才加工**的，不会被 `chmod` 改写。
3. **不杀别人起的进程。** 需要演示信号、后台作业、`trap` 时，只对 `$!` 记录的、本脚本自己 fork 出来的
   子进程发信号，并在退出前 `wait` 到它结束。绝不用 `pkill`/`killall` 扫全局。
4. **不起常驻监听端口。** 需要「等待/超时」场景时用文件轮询或 `timeout`，不 `nc -l`。
5. **不联网、不装包、不猜密码。** 需要外部输入时一律现场生成假文件；缺工具就如实说明。
6. **不编造输出。** 任何跑不出来的场景在文档里明确标注「本机未验证」，并说明为什么。
7. **代码块内容与源文件一字不差。** README 引用的每一行都能在对应那次 `demo.sh` 的真实输出里
   `grep -F` 到（第 01 章交付前逐条核对过 361 行，本文件自己核对过 82 行，全部命中）。
8. **屏幕上不出现真实身份。** `ls -l` 的属主列、`$HOME` 展开出来的家目录、`uname -a` 里的主机名，
   README 里一律写成 `<用户>` / `<家目录>` / `<主机名>`。这三样是 shell 自己展开的，demo 拦不住，
   只能事后替换 —— 这也是全书**唯一**对真输出做改动的一处，校验器因此会在这些行各报 1 行「未匹配」，
   属预期，看到 `<用户>` / `<家目录>` 开头的未匹配行可以直接放过。

---

## 目录结构

```text
bash-handbook/
├── README.md                  ← 你在这里
├── 01-shell-vs-script/        ✅ 已完成
│   ├── README.md              本章讲解（904 行，含真实输出）
│   ├── demo.sh                一键复现全章证据（第 0～8 节共 9 节，644 行输出）
│   ├── pids.sh                三种跑法对比：谁在跑、变量看得见吗
│   ├── bashonly.sh            只用 bash 独有语法（dash 会炸给你看）
│   ├── subshell.sh            子 shell 三个来源 + 管道里变量丢失
│   ├── posargs.sh             $0 $1 $# $@ $* 与 set --
│   ├── envcheck.sh            shell 变量 vs 环境变量 + env -i 干净环境
│   ├── rcprobe.sh             alias/shopt 在脚本里到底有没有
│   ├── trace_demo.sh          给 bash -x 用的最小样本（自定义 PS4）
│   ├── errors.sh              运行时错误：命令不存在、set -u 未定义变量
│   └── broken_syntax.sh       故意的语法错误：给 bash -n 用
├── 02-words-and-expansion/    ✅ 已完成
│   ├── README.md              本章讲解（2062 行，12 个 `##` 大节，含 `$` 的引文 703 行逐行核过）
│   ├── demo.sh                一键复现全章证据（444 行脚本 → 1017 行纯文本输出）
│   ├── seg.sh                 本章核心取证工具：把「展开并分词之后到底剩几个词」打印出来
│   ├── args_matrix.sh         `$*` 与 `$@` 的四格矩阵逐项实测（带引号 / 不带引号 × 有无 IFS）
│   ├── passing.sh             同一个值在四种写法下的「传参变形记」
│   ├── vars.sh                脚本内部的自动变量实测
│   ├── unbound.sh             变量名拼错：默认安静当成空串 vs `set -u` 当场报错退出
│   ├── qmark.sh               `${v:?提示语}` 在脚本里的真实行为（报错原文 + 当场结束）
│   └── safeloop.sh            遍历文件名时的四种写法对照（只读文件名，不写不删）
├── 03-testing-and-exit-codes/ ✅ 已完成
│   ├── README.md              本章讲解（3059 行，第 6～15 节 + 章尾固定段，含 `$` 的引文 1041 行逐行核过）
│   ├── demo.sh                一键复现全章证据（436 行脚本 → 1711 行纯文本输出）
│   ├── exitcodes.sh           「退出码是每条命令自己定的约定」实测表
│   ├── bracket_spaces.sh      `[ ]` 到底是个什么东西：程序身份、空格语法、经典事故
│   ├── file_tests.sh          文件测试运算符全表，每个都用沙箱里真造出来的对象实测
│   ├── regex_pitfalls.sh      `[[ ]]` 的 `==`（glob）与 `=~`（正则）：引号一加，含义全变
│   ├── and_or_trap.sh         `&& || !` 与 if/elif：`A && B || C` 为什么不是 if-else
│   ├── status_chain.sh        `$?` 是怎么被「传下来」又「半路丢掉」的
│   └── case_dispatch.sh       case 的模式匹配与三种终止符（版本/后缀分派的主战场）
├── 04-pipes-and-redirection/  ✅ 已完成
│   ├── README.md              本章讲解（2103 行，26 节 + 章尾固定段，含 `$` 的引文 721 行逐行核过）
│   ├── demo.sh                一键复现全章证据（608 行脚本 → 1178 行纯文本输出）
│   ├── slowpipe.sh            管道缓冲实测：中间那一段「多久才把字吐给下游」
│   ├── heredoc.sh             here-doc 取证：`<<EOF` / `<<'EOF'` / `<<-` 三兄弟
│   ├── herestring.sh          here-string（`<<<`）取证：和 here-doc 差在哪
│   ├── execsave.sh            `exec` 改的是这个 shell 自己的 fd：永久重定向、保存、恢复
│   └── mkweird.sh             在当前目录造出「名字里有空白字符」的文件，给 xargs 两节用
├── 05-loops-and-control/      ✅ 已完成
│   ├── README.md              本章讲解（2555 行，22 节 + 章尾固定 8 段，含 `$` 的引文 393 行逐行核过）
│   ├── demo.sh                一键复现全章证据（357 行脚本 → 957 行纯文本输出，22 个节标题）
│   ├── loopsnap.sh            词表快照：循环里新建/删掉的文件，本轮根本看不见
│   ├── loopspeed.sh           循环贵在哪里：同一个循环体，词表形状不同就少跑/多跑
│   ├── mapfill.sh             `mapfile` 全开关实测（含 CRLF 的 `-t` 只砍 `\n` 不砍 `\r`）
│   ├── jobwait.sh             作业控制：`&` / `$!` / `jobs` / `wait` / `disown` / `kill -0`
│   ├── selectmenu.sh          `select` 的真实语义：菜单、stdin 驱动、非法输入变空串
│   ├── readlast.sh            最后一行没有换行符就被 `read` 吞掉
│   ├── readsteal.sh           循环体偷吃 stdin：内层 `read` 把外层的行喝干了
│   ├── untilretry.sh          `until` 重试环：「等什么」+「最多等多久」两半
│   └── parlimit.sh            限并发三写法：一把全扔 / 数闸门轮询 / `wait -n`
├── 06-functions-and-returns/  ✅ 已完成
│   ├── README.md              本章讲解（2906 行，24 节 + 章尾固定 8 段，含 `$` 的引文 502 行逐行核过）
│   ├── demo.sh                一键复现全章证据（538 行脚本 → 1305 行纯文本输出，24 个节标题）
│   ├── retcode.sh             `return` 后面能写什么、低 8 位怎么回绕（7 格）
│   ├── outcapture.sh          命令替换的四刀：尾换行、多值、诊断、子 shell（6 格）
│   ├── errmix.sh              三条通道怎么分：码走 `$?`、数据走 stdout、话走 stderr（7 格）
│   ├── trapreturn.sh          `return` 与 `exit` 在 trap 里、在 source 里的不同下场（7 格）
│   ├── localscope.sh          动态作用域：谁调了它才决定它改的是谁（8 格）
│   ├── recursion.sh           递归靠什么成立、代价怎么量（`FUNCNEST`、进入次数）（7 格）
│   ├── functype.sh            一个名字有几种身份：`type -t/-a` 与三条夺回路（7 格）
│   ├── exportfun.sh           函数能不能跨进程：`export -f` 的形状、代价与负对照（8 格）
│   └── argpass.sh             函数的参数从哪来：`$#`、`"$*"` 与 `"$@"`、`set --`（7 格）
├── 07-text-three-musketeers/  ✅ 已完成
│   ├── README.md              本章讲解（3003 行，25 节 + 章尾固定 8 段，含 `$` 的引文 480 行逐行核过）
│   ├── demo.sh                一键复现全章证据（489 行脚本 → 1339 行纯文本输出，25 个节标题）
│   ├── grepdialect.sh         同一个正则在三套方言里是三份结果（7 格）
│   ├── greppat.sh             grep 手里只有「一整行」：`-o` / `-c` / 锚点 / locale（8 格）
│   ├── grepfix.sh             用 grep 做工程判断：说没有 / 说出错 / 把码吃掉（8 格）
│   ├── sedflow.sh             sed 手里永远只有一行：模式空间、地址、`g`/数字（8 格）
│   ├── sedinplace.sh          `sed -i` 不是「就地改」：新建、写完、再挪名字（8 格）
│   ├── pipe3.sh               三环串成管道时「单位错位」到底错在哪（8 格）
│   ├── threeway.sh            同一件小事三家各做一次：谁够得着，谁只是看着像（7 格）
│   ├── awkjoin.sh             awk 的关联数组：拿小表当索引，两份文件拼成一份（7 格）
│   ├── awkframe.sh            awk 拿的是「一行里的字段」，而且字段可以赋值（7 格）
│   └── awkmawk.sh             本机 awk 是 mawk 1.3.4：会叫的缺件与不会叫的缺件（7 格）
├── 08-robust-scripts/          ✅ 已完成
│   ├── README.md              本章讲解（2360 行，18 节 + 章尾固定 8 段，含 `$` 的引文 429 行逐行核过）
│   ├── demo.sh                一键复现全章证据（492 行脚本 → 1381 行纯文本输出，18 个节标题；骨架 skel.sh 由它现场写进沙箱）
│   ├── strictfour.sh          四件衣服一件一件穿：`-e` / `-u` / `-o pipefail` / `-E` 各挡什么（11 格，29 条 bash -c）
│   ├── errescue.sh            `-e` 的四位豁免：`&&` `||` `if` `!`，以及条件位把函数体内关灯（11 格，25 条）
│   ├── trapsafe.sh            陷阱的三个时间差：定义时展开、触发时吞码、清理被 -TERM 打断留半截（11 格，26 条）
│   ├── argscheck.sh           参数三道闸：`$#`、getopts 两种 optstring、`--` 会被带值选项吃掉、`./`（9 格，23 条）
│   ├── filesafe.sh            碰文件之前的刀：判断跟链接走、mktemp 权限、set -C、flock 跟 fd 走（9 格，18 条）
│   └── weirdnames.sh          4 个怪名字走完整条链：4 个变 7 段、`rc=0` 却少删一个（8 格）
├── 09-when-things-break/       ✅ 已完成
│   ├── README.md              本章讲解（3601 行，18 节 + 章尾固定 8 段，含 `$` 的引文 304 行逐字对上、0 行走模糊兜底）
│   ├── demo.sh                一键复现全章证据（721 行脚本 → 1237 行纯文本输出，18 个节标题；顶部一行 `# CHECK: RUN=非复现`）
│   ├── xtrace.sh              `bash -x` 到底说什么、走哪条流、跟不跟得进各种容器（10 格）
│   ├── ps4fd.sh               PS4 在**哪一刻**展开、`BASH_XTRACEFD` 三档、fd 是谁开的才算（9 格）
│   ├── debugtrap.sh           `trap DEBUG` 的时机与次数、`-T`／`extdebug`、调用栈三数组的口诀（11 格）
│   ├── staticcheck.sh         不跑起来也能查：`-n`／`-o noexec`／`-u`、报错行四段、同族工具在场名单（7 格）
│   ├── timing.sh              `time` 的三重身份、TIMEFORMAT 52 个字母逐个试、这把尺能不能进 CI（8 格）
│   └── tracewho.sh            一个名字被谁接走、strace 的口径／隐私／可复现性与「不用 `-p`」那条红线（10 格）
├── 10-real-recipes/            ✅ 已完成
    ├── README.md              本章讲解（2634 行，13 节 + 章尾固定 8 段，带提示符的引文 100 行逐字对上、0 行走模糊兜底）
    ├── demo.sh                一键复现全章证据（1153 行脚本 → 896 行纯文本输出，13 个节标题、87 个大格；顶部 `# CHECK: RUN=非复现`，第 7~13 节的取证脚本正文内嵌在它里面）
    ├── rename.sh              批量重命名：名单是哪来的，坑就是哪来的（10 格，`# CHECK: RUN=复现`）
    ├── dircheck.sh            目录校验：一盏绿灯只覆盖清单上那几行（9 格，`# CHECK: RUN=复现`）
    ├── logrotate.sh           日志轮转：换的是名字，那只正在写字的手没被换（11 格，`# CHECK: RUN=非复现`——屏上当场印 pid 与竞态末值）
    ├── interact.sh            把交互操作固化成脚本：手敲时 shell 白送的那四样，脚本里一样都没有（10 格，**不写 RUN 标记＝不豁免**）
    └── redline.tsv            第 13 节红线扫描的数据文件：1 行说明 + 10 条「类别／ERE／样本」，样本只用来验模式真能命中，本章一行都不执行
└── 11-cheatsheet/              ✅ 已完成（全书速查，唯一一章不取证、只自查）
    ├── README.md              456 行：10 张按场景查的表（全页 13 张表、154 行表格，每行带「两位章号 §节号」出处）+ 四段能直接抄的骨架 + 一页纸 22 条 + 章尾固定 7 段
    └── demo.sh                417 行脚本 → 261 行纯文本输出，八节自查：出处逐条解析（123 条，断链 0）、每句「屏上」回前十章的现场围栏里找（9538 行屏幕底本，判 25 段、站不住 0）、页面结构四条守卫、四段骨架过 `bash -n`、8 条硬事实当场重跑、隐私扫描先响再报 0
                               顶部 `# CHECK: RUN=就地`：它要翻开前十章的 README，拷进沙箱就没有真结果
```

- 上面这张 11 章的地图已经走完了。第 11 章**故意不照前十章的结构**：它没有取证脚本，
  `demo.sh` 的枪口对准这一页自己（出处断链、骨架语法、8 条硬事实、隐私、红线自证）。
- **当前进度：01～11 十一章全部已交付**（每章都是「取证脚本 → demo 一键复现 → 定稿捕获 → README 按行号搬原文 → 闸门全绿」；第 11 章跳过取证那一步，因为它不取证）。
- `broken_syntax.sh` 是唯一**故意**不能通过 `bash -n` 的文件，别当成 bug 去「修好」它。
- 各章的引文保真度是同一个口径：README 里凡带 `$` 的围栏，都由 `@@R 起-止@@` 占位符
  从**定稿捕获文件**（`~/.cache/handbook-captures/bashNN.txt`）里机械搬出，再用
  `~/.local/bin/expand_fences.py` 展开、`~/.local/bin/chapter_gate.sh` 判章、`~/.local/bin/audit_all.sh` 扫全书 ——
  第 02、03、04、05、06、07、08、09、10、11 章当前均为「未匹配行 0」。
  因此**已交付章的 demo 不能随手重跑**（重跑行号会漂，占位符指向的内容就错位了）。
  真要改脚本，就整章重做一遍闭环：跑两遍比对、`~/.local/bin/capture_index.py` 重生成行号索引、
  重刷 md5、重拼 README、重过闸门。第 06 章就这么走过一次 —— 输出从 1279 行变成 1305 行，
  起因是第 22 节有一句「env 里多了一个键名很怪的条目：」后面被 `sed -n` 少了个 `p` 而什么都没打印，
  以及第 24 节有两道守卫标着「能响」却当场印 0。**承诺印在纸上，就必须有断言撑着。**
  第 07 章这么走过**两次**（记录在 `~/.cache/handbook-captures/bash07-改判-2026-10-14.md`）：
  第一次是 `grepdialect.sh` 的取证助手 `cell` 支持末尾传方言开关，而 16 个调用点**只把 `ERE -E` 写在标签里、
  开关一个都没传** —— 屏幕上那些标着 ERE/PCRE 的行其实全是 BRE 的结果；修法不是补数字，而是让 `cell`
  把**真正传进命令的那串开关**单独印成一列，标签写错就在屏幕上自己露出来。
  第二次是 `awkjoin.sh` 第 16 节第 5 格那句「同一个键吗 → 是」，判据其实是 `length(a)==2`
  这个「少了一个元素」的**代理**。两条同族：**「rc=0 + 两遍 diff 为空」证明的是可复现，不是内容对**。
  第 08 章同样留了一份改判记录（`~/.cache/handbook-captures/bash08-改判-2026-10-15.md`）：
  13 条里最值钱的三条是「dash 其实**实现了** `pipefail`（我把断言写反了）」、
  「`$?` 被紧跟其后的 `cd ..` 顶掉，于是第 14 节四个读数全是 0」，
  以及「隐私守卫自己的正则也会假阳性 —— 它会命中本章的片段文件名 `bash08-readme-partA.md`」。
  第 09 章留了同族记录（`~/.cache/handbook-captures/bash09-改判-2026-10-16.md`）：
  捕获定稿、八段片段拼完之后又抓到两处「屏上是一回事、正文是另一回事」，都付了整章重冻结的代价 ——
  最贵的一条是**「dash 其实认 `PS4`」（我把断言写反了，和第 08 章 `pipefail` 同族）**：
  它认这个变量，只是 `${LINENO}`／`${FUNCNAME}`／`${BASH_SOURCE}` 在 dash 里全是空串、
  `BASH_XTRACEFD` 对它一点作用都没有 —— 「壳还在、名字全空」这种**不失败的降级**比报错难查得多。
  为此第 16 节重写成四问、捕获从 1227 行重冻结成 1237 行、19 个行号区间用脚本机械搬迁。
  同一次还钉了两件事：**格标题也是承诺**（`ps4fd.sh` 第 2 格标题写「那三种空」而屏上只有两格空），
  以及**正文里引用抖点必须写「这一趟印的是 X、判据是 Y」**（`time` 那几个不同值在本章几份留档里印过 2、3、4、5）。
  第 10 章留了同族记录（`~/.cache/handbook-captures/bash10-改判-2026-10-16.md`），六条，屏幕一行没动，
  动的全是**承诺与工具**。最贵的一条是「两遍逐字相同」这一条**许不起**：`demo.sh` 顶部原本写 `RUN=复现`，
  而它把 `logrotate.sh` 的输出并进整章，屏上必然带 5 行由调度决定的抖动（3 处 pid、2 处竞态末值），
  `script_check.py` 两遍真跑当场判 FAIL。修法不是把比对放宽（那等于拆守卫），是把标记改成真话、
  并把豁免**量到行号**（第 413、417、424、473、475 行），其余四条 —— rc、残留文件数、隐私、每格自己的断言 —— 照旧从严。
  另外三条同族教训值得每章都过一遍：**工具漏拷数据文件会造出「章少印一块」的假阳性**（第 13 节那份 `redline.tsv`）、
  **被正文引用的工具不能放在会被清理的暂存目录**（那把数字对账尺已提到 `~/.local/bin/check_part.py`）、
  **只改注释也会把三笔现读数刷掉**（章目录 `.sh` 字节总和当场从 175173 漂到 175241，
  所以那三处的判据一律写成「跑前与跑后相等」而不是「等于几」）。

---

## 下一步

**直接进第 01 章：[`01-shell-vs-script/README.md`](01-shell-vs-script/README.md)。**
那一章你会亲眼看到三件很多人到死都搞混的事：

1. `bash x.sh`、`./x.sh`、`source x.sh` 是**三种不同的进程关系**，变量可见性完全不同
   （实测：`source` 时 `$$` 和外层一模一样，另两种是子进程且 `$PPID` 指向外层）；
2. `sh` 在这台机器上**根本不是 bash**（`/bin/sh -> dash`），同一个文件换个解释器就 `Syntax error: "(" unexpected`；
3. 管道右侧的 `n=$((n+1))` **改的是子 shell**，外面永远看到 0 —— bash 新手第一大坑。

跑法：

```bash
cd bash-handbook/01-shell-vs-script
bash demo.sh
```

已经跑完 01 的话，按这个顺序往下读，每一章都只解决一件事：

| 章 | 一句话 | 你会拿到的判据 |
| --- | --- | --- |
| [`02-words-and-expansion`](02-words-and-expansion/README.md) | 一行命令先被切成**词**，才轮到执行 | 「为什么引号一丢，一个文件名变成两个参数」 |
| [`03-testing-and-exit-codes`](03-testing-and-exit-codes/README.md) | 判断的唯一出口是**退出码**，`[ ]` 本身只是个程序 | 「为什么 `[ $x = yes ]` 在 x 为空时炸」 |
| [`04-pipes-and-redirection`](04-pipes-and-redirection/README.md) | 管道是**进程间的接线**，重定向是**启动前改 fd 指向** | 「为什么 `2>&1 >f` 没把报错收进文件」 |
| [`05-loops-and-control`](05-loops-and-control/README.md) | 循环的三本账：**名单、进程、收尾** | 「为什么 `for f in *.tmp` 里新建的文件本轮看不见」 |
| [`06-functions-and-returns`](06-functions-and-returns/README.md) | 函数只做三件事：**交回一个码、动名字表、留可见性** | 「为什么 `return 256` 等于成功、`inner` 改的是 `outer` 的 `x`」 |
| [`07-text-three-musketeers`](07-text-three-musketeers/README.md) | 三家唯一相同的是**按行吃饭**，其余三本账（单位 / 能不能就地改 / 码表不表结果）各记各的 | 「为什么抄来的 `-E` 写 `\d+` 不报错却给你**错的行**、`sed -i` 一个字没改也退 0、awk 改一列结果 diff 炸一整行」 |
| [`08-robust-scripts`](08-robust-scripts/README.md) | 健壮性不是一句 `set -e`，是**四件衣服一件一件穿**（`-e` / `-u` / `-o pipefail` / `-E`），外加陷阱的三个时间差 | 「为什么 `A && B` 里 `-e` 关灯、`trap … EXIT` 的清理会被 `kill -TERM` 打断留半截、`flock` 跟着 fd 走而不是跟着文件走」 |
| [`09-when-things-break`](09-when-things-break/README.md) | 出事怎么查：`-x` 到底说什么、走哪条流、`trap DEBUG` 什么时候响、不跑起来能查什么 | 「为什么 `PS4` 里的 `${LINENO}` 在 dash 里是**空串**（壳还在、名字全空）、`time` 在 dash 里根本不是关键字而是外部程序」 |
| [`10-real-recipes`](10-real-recipes/README.md) | 四件真活各跑一整章：批量改名 / 目录校验 / 日志轮转 / 把交互操作固化成脚本 —— 每条主线一本账 | 「为什么 `mv` 换了名字而那只 fd 还在写同一个 inode、`sha256sum -c` 的绿灯只覆盖清单上那几行、你手敲时 shell 白送的四样脚本里一样都没有」 |
| [`11-cheatsheet`](11-cheatsheet/README.md) | 全书速查：10 张按场景查的表（每行带出处）+ 四段能直接抄的骨架 + 一页纸 22 条。本章不取证，`demo.sh` 自查这一页自己 | 「为什么这一页每条出处都要当场解析一遍才算数、`bash -n` 过得了的四段骨架仍然只是形状、速查表里哪三条判据明确说了不许拿它当证据」 |

第 04 章会把第 01 章那句「管道右侧改的是子 shell」量到进程表上（`$$` vs `$BASHPID`），
再顺手讲清 `PIPESTATUS`、`pipefail`、`exec` 改 fd、here-doc、`<( )` 进程替换和管道缓冲 ——
01 里那个「外面永远看到 0」的坑，到这一章才有完整的账。
