# 09 子模块与 worktree（submodule / worktree / subtree）

> 一句话：这一章解决两类「一个仓库不够用」的局面 ——
> **同一个仓库，我想同时开出第二个工作目录**（`git worktree`），
> 以及 **一个仓库里要套着另一个仓库，还要说清楚「我必须用哪一版」**（`git submodule`）。

前置：01 章「分支就是一个写着哈希的文本文件」+ 工作区/暂存区/仓库三区模型；
02 章 `git status --short` 的两格符号；03 章的 reflog（第 11 节的救援全靠它）；
07 章的 remote / push / fetch（本章所有「远程」都是本地 bare 仓库冒充的）。

本章最值钱的五个认知，后面每一节都是在给它们按手印：

1. **一个仓库 = 对象库 + 工作目录，两者可以拆开。** `git worktree` 只多一个工作目录、不多一份历史：
   新工作区里那个叫 `.git` 的东西**是一个 66 字节的文本文件**，不是目录（第 4 节实测）。
2. **宿主对 submodule 只记一条 `160000` 的 gitlink —— 一个 sha，不记任何文件内容。**
   所以宿主的对象库里根本拿不到那个 commit 对象（第 8 节那条 `could not get object info` 就是铁证）。
3. **`git clone` 不会把子模块的文件给你。** 目录是空的、`git status` 还是干净的，
   只有 `git submodule status` 前面那个 `-` 说了实话；`--init` 干的是「注册」，不能省（第 10 节）。
4. **子模块里的库一 checkout 出来就是 detached HEAD**（宿主只认 sha），
   代价是你在这里提交的东西一开始不属于任何分支（第 11 节：事故 + reflog 救援 + 推送顺序）。
5. **子模块那行的符号有五种，意思完全不同：`-` / `+` / ` M` / ` m` / ` ?`。**
   前两个是「sha 对不上」，后三个只是「库里面不干净」，宿主记的 sha 压根没动（第 9、10 节）。

> 本章所有终端输出都来自仓库里 `bash demo.sh` 的**一次真实运行**（14 节、1132 行、退出码 0，原文存在 `/tmp/git09.txt`），
> 每个终端块（含 `$ 命令`、`[退出码 N]`、`↳` 注释行）都能在那份输出里逐字找到。
> **引用约定**：① 沙箱路径本次是 `/tmp/git-handbook-09-GVecYD`，本文一律写成 **`<沙箱>`**，你重跑后缀必然不同，
> 这不影响其余任何一行；② 凡带**文件属主列**的 `ls -l` / `ls -ld` 输出本文**不引用**（那里面是跑脚本那台机器的用户名，
> 与 git 行为无关），证明「`.git` 是文件」改用 `file` + `cat`，证据一样硬；③ 提交身份与时间戳在脚本里固定
> （`学习者 <learn@example.com>`，从 `2026-01-05 09:03` 起每笔 +3 分钟），所以**本文每个哈希你都能复现**；
> ④ 标了 `★ 这条是故意失败的` 的非 0 退出码是脚本制造的错误现场，不是演示跑挂了；
> ⑤ 本文里**三反引号围栏块 = 真终端输出**，**缩进 4 空格的块 = 作者手绘示意图**，别拿示意图当证据。
> ⑥ 版本相关的两条：本次是 `git version 2.55.0`，脚本还在开工时探测了 `git-subtree` 存在（所以第 13 节是真跑）。

---

## 你会遇到的场景

| 你遇到的事 | 本章哪一节 |
|---|---|
| 正在 main 上写半成品，想同时看另一个分支，又不想 stash 来 stash 去 | 第 1 节（问题）+ 第 2 节（`worktree add`） |
| 为了「再看一眼另一个分支」又 clone 了一份，磁盘多出一整个历史 | 第 1 节 `du` / `count-objects` 实测差额 |
| `fatal: 'main' is already used by worktree at ...` | 第 3 节（★ 一个分支只能被一个工作区检出） |
| worktree 目录被我 `rm -rf` 了，`git worktree list` 里还挂着一条 | 第 5 节（`prunable` + `git worktree prune`） |
| CI 要一份干净的旧版本代码跑测试，不想占用分支名 | 第 6 节（`--detach` + `git -C`） |
| `git submodule add` 报 `fatal: transport 'file' not allowed` | 第 7 节（★ 2.38.1 起的安全限制） |
| clone 下来子模块目录是空的，`git status` 还告诉我「干净」 | 第 10 节（`update --init`，以及 `-` 前缀） |
| 子模块里报 `(modified content)`，可我改的文件宿主怎么看不到 | 第 9 节（三种脏状态 + 宿主根本带不走） |
| `-Subproject commit abc` / `+Subproject commit def` 到底在比什么 | 第 8 节（160000）+ 第 9 节（三种 gitlink diff） |
| 在子模块里提交完一切换，提交不见了 | **第 11 节（★ 本章保命符：detached HEAD + reflog）** |
| push 宿主被拒：`changes that cannot be found on any remote` | 第 11 节（正确顺序：先推库，再推宿主） |
| 想彻底删掉一个 submodule，删完发现还有残留 | 第 12 节（`deinit -f` → `git rm` → 提交 → `rm -rf .git/modules/<name>`） |
| 有人说「子模块太难用，用 subtree 吧」，subtree 到底干了什么 | 第 13 节（两种都真跑了） |

---

## 心智模型

### 图 1：一个仓库可以拆成「对象库」和「工作目录」

    ┌──────────── repo（一个 .git 对象库） ────────────┐
    │ objects/ 所有历史  refs/ 所有分支标签  config    │
    └──────┬───────────────┬───────────────┬──────────┘
      工作目录 A       工作目录 B      工作目录 C
      HEAD=main        HEAD=hotfix      HEAD=（游离）
      index 一份        index 一份        index 一份

    共享：对象库、所有 refs、config、remote、stash 列表、hooks
    各自：HEAD（在哪个分支）、工作目录内容、index、.git/worktrees/<名字> 那份登记

这句话解释了前 6 节的所有现象：**多开工作目录很便宜，所以「再 clone 一份」是浪费；
但 refs 是共享的，一个分支被两个工作区同时检出就会互相踩指针 —— 于是 git 直接禁止**（第 3 节实测）。

### 图 2：submodule 不是「把库拷进来」，是「留一张字条」

    宿主 host                                库 math（有自己的远程 math.git）
      .gitmodules  ← url = ../math.git          calc.c
      libs/math    ← 索引里只有一行：            README.md
      README.md       160000 f3e4bf7d…          历史 add → mul → divi → sq
      src/...
                                               宿主的对象库里【没有】f3e4bf7
      字条内容：「libs/math 必须正好是 f3e4bf7」    （第 8 节实测：cat-file 直接 fatal）

    库的 git 目录藏在哪？不在 libs/math/.git 里，在宿主的 .git/modules/libs/math。
    libs/math/.git 只是一个 gitdir: 指针文件 —— 和第 4 节 worktree 是同一套机制。

### 表：`submodule status` 前缀 与 `git status --short` 符号对照

| 看到的东西 | 出自哪条命令 | 含义 | 宿主记的 sha | 该敲什么 |
|---|---|---|:--:|---|
| `-fa2513… libs/math` | `git submodule status` | 还没初始化，库是空的 | 没变 | `git submodule update --init` |
| ` fa2513… libs/math (heads/main)` | `git submodule status` | 检出值 = 索引值，同步 | 没变 | 什么都不用 |
| `+a2cd92… libs/math (remotes/origin/HEAD)` | `git submodule status` | **检出值 ≠ 索引值**，工作区跑在前面 | 索引没变 | 宿主 `git add libs/math` + 提交 bump，或 `git submodule update` 退回 |
| ` M libs/math` | `git status --short` | 库里有**新提交**（`new commits`） | **这才是宿主要记的** | 决定 bump 还是回退 |
| ` m libs/math` | `git status --short` | 库里**已跟踪文件**被改，没提交 | 没变，宿主管不着 | 进库里提交（两步），或 `git -C libs/math checkout -- <文件>` |
| ` ? libs/math` | `git status --short` | 库里多了**未跟踪文件** | 没变 | 进库里处理，或 `--ignore-submodules=untracked` |

`+` 和 ` M` 总是同时出现（同一件事的两种说法）；`m` / `?` 只说明「库里面不干净」，
跟宿主的历史**一点关系都没有** —— 这一句就是第 9 节的全部内容。

---

## 第 0 节 开工前先看清这台机器

```text
$ echo "HOME=$HOME"; git config --global --list 2>&1 | head -3
HOME=<沙箱>/fakehome
fatal: unable to read config file '<沙箱>/fakehome/.gitconfig': No such file or directory
[退出码 0]
  ↳ HOME 已经指到沙箱里的 fakehome，所以最后这条一定是「找不到文件」级别的报错 ——
  ↳ 这正是我们要的：脚本没有任何一条命令能读到你真实的全局配置，更别说写它。
```

**这行在说什么**：HOME 被改写到沙箱里，所以那条 `fatal: unable to read config file ... No such file or directory`
是**好消息**：你机器上的真实全局配置在脚本眼里不存在。本章没有一条 `git config --global`；
出现的 `git config` 要么带 `--local`（第 12 节），要么带 `-f .gitmodules`（第 7 节没用到），要么只在一个 `-c` 里出现一次。
最后一行 `[退出码 0]` 值得单独记：**管道结尾的 `head -3` 成功了，所以整条命令退出码是 0 ——
管道里 git 的 fatal 不一定会变成非 0**（08 章还会撞一次）。

**为什么**：先确认版本、以及「这些命令写不到你真实配置」，后面 14 节才敢跟着敲。

---

## 第 1 节 为什么需要 worktree：一个仓库只有一个工作目录

**场景**：`app` 仓库正在 `main` 上干活（有一处没提交的改动），你想同时看 `hotfix` 分支的代码。
脚本给它造了 40 个数据文件、改写 6 轮，`main` 和 `hotfix` 从 `8b863e3` 分叉 ——
工作目录大小不变、历史一份份堆起来，这才是真实项目的样子（不然第 14 节的体积对比没有说服力）。

```text
$ echo "main 上正在写的半成品，还没打算提交" >> src/part02.txt; git status --short
 M src/part02.txt
[退出码 0]
  ↳ 工作目录已经被 main 分支 + 这份未提交改动占着。想同时看 hotfix，最直觉的做法是再 clone 一份：

$ git clone --no-local -q . ../app-clone && echo "clone 完成"
clone 完成
[退出码 0]
  ↳ 这里特意加了 --no-local：本地路径的 clone 默认会【硬链接】对象库里的 pack，du 甚至看不出成本。
  ↳ 真实场景是从远端服务器 clone，对象必须重新落一份盘，所以用 --no-local 模拟出那个真实代价。

$ cd .. && echo "=== 整目录体积 ==="; du -sh app app-clone
       echo "=== 各自的对象库 .git（第二份历史就是这里的差额）==="; du -sh app/.git
       echo "（clone 的 .git 单独量：）"; du -sh app-clone/.git
=== 整目录体积 ===
7.8M	app
7.8M	app-clone
=== 各自的对象库 .git（第二份历史就是这里的差额）===
6.1M	app/.git
（clone 的 .git 单独量：）
6.1M	app-clone/.git
[退出码 0]

$ cd .. && echo "--- 主仓库对象数：" && git -C app count-objects -v | head -3
       echo "--- clone 副本的对象数（一条不少，全抄了一遍）：" && git -C app-clone count-objects -v | head -3
--- 主仓库对象数：
count: 0
size: 0
in-pack: 309
--- clone 副本的对象数（一条不少，全抄了一遍）：
count: 0
size: 0
in-pack: 309
[退出码 0]
  ↳ clone 副本 = 工作文件 + 又一份完整历史；而且两份 .git 之后要各自 fetch、各自 gc。
  ↳ worktree 干的是另一件事：只多一个工作目录，对象库和分支全部共用。第 2 节建一个，第 14 节量给你看。
```

**这行在说什么**
- ` M src/part02.txt` 落在第 2 格 = 工作区相对暂存区有改动。**就是这一行挡住了你切分支**：
  切去 `hotfix` 得先 stash 或提交，回来还得 pop。
- 体积：整目录 7.8M 里 `.git` 占 6.1M —— **历史才是大头**；副本 7.8M 里 `.git` 又占 6.1M，
  两个数**一模一样**。为了「看一眼另一个分支」多花 6.1M，而这 6.1M 里没有一个字节是新内容。
- 对象数：`count:` 是松散对象条数、`in-pack:` 是打包进 pack 的条数。主仓库 0 + 309 = **309**，
  副本 0 + 309 = **309** —— **两个数一模一样**，这就是「一条不少，全抄了一遍」的算术证明。
  两边 `count` 都是 0，是因为副本由 clone 直接落成 pack、主仓库刚被脚本显式 `git gc` 过一次。

> **这两屏为什么敢写成固定数字**（我第一版写的时候它们其实每次都在变）：`du -sh` 和 `count-objects`
> 量的都是「磁盘上此刻有几个 pack」。而 `git commit` 会在背后偷偷起一个 `git gc --auto`，
> **它有没有跑完是竞态** —— 同一条脚本连跑两次，主仓库的 `.git` 能从 6.1M 变成 13M、`in-pack` 从 309 变成 525
> （多出来的是 gc 顺手收进来的不可达对象）。所以 demo 脚本在开头用环境变量把 `gc.auto` 压成 0
> （`GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=gc.auto GIT_CONFIG_VALUE_0=0`，不落任何配置文件，也不影响那条显式 `git gc`）。
> 加上这一行之后，同一台机器连跑两次，上面这两屏逐字节相同。
> **换一台机器 / 另一种文件系统（块大小、目录哈希方式不同）时这几个数还是会变**，
> 但「副本 ≈ 原仓库、worktree 只多一个工作目录」这个结论不变。

**为什么**：`clone` 复制的是「整个仓库」，你想要的只是「另一个工作目录」，成本差了 6.1M 和两份要各自维护的历史。

---

## 第 2 节 `git worktree add`：给同一个仓库再开一个工作目录

```text
$ git worktree add ../app-hotfix hotfix
Preparing worktree (checking out 'hotfix')
HEAD is now at 0d8552d hotfix: 改一处
[退出码 0]
  ↳ 上面这几行是原样输出：Preparing worktree 那行、HEAD is now at 那行，都是 git 自己打的。
```

**这行在说什么**：括号里是**动作 + 分支名**；`HEAD is now at` 和 `git checkout` 后那句是同一句话。
紧接着脚本用 `git -C ../app-hotfix log --oneline -3` 在新工作区里问历史，
顶端是 `0d8552d hotfix: 改一处`，下面两行是 `8b863e3 第 6 轮…`、`219f7fc 第 5 轮…` ——
**`main` 那边的提交在这儿看得见**，因为对象库是同一个（第 4 节证明）。
`git -C <目录> <子命令>` = 「先去那儿，再跑这条 git 命令」，比 `cd && git` 少一次 shell 状态变化，脚本全程用它。

在新工作区里改一笔（往 `src/part03.txt` 追加一行）并提交：

## 第 3 节 `git worktree list` 与「一个分支不能被两个工作区同时 checkout」

```text
$ git worktree list
<沙箱>/app        f04660c [main]
<沙箱>/app-hotfix 7df58fc [hotfix]
[退出码 0]

$ git worktree add ../app-main2 main
Preparing worktree (checking out 'main')
fatal: 'main' is already used by worktree at '<沙箱>/app'
[退出码 128]
  ↳ ★ 这条是故意失败的。规矩：一个分支最多只能被一个工作区检出，否则两边同时提交会互相踩指针。
  ↳   想在同一份代码上来回切，用 --detach（第 6 节），或者再开一个分支。

$ ls -d ../app-main2 2>&1; echo "（失败后没留下半个目录，不用手工收拾）"
ls: cannot access '../app-main2': No such file or directory
（失败后没留下半个目录，不用手工收拾）
[退出码 0]

$ git worktree add ../app-typo does-not-exist
fatal: invalid reference: does-not-exist
[退出码 128]
  ↳ ★ 这条也是故意失败的：第二个参数必须是已存在的 ref（分支 / 标签 / commit sha），写错就 fatal。
```

**这行在说什么**
- `git worktree list` 三列 = 工作区绝对路径、当前 commit、`[分支名]`；换成 `(detached HEAD)` 就是没挂分支的工作区（第 5、6 节）。
- 第一条 fatal 是**先打印 `Preparing worktree` 再失败**，说明检查发生在中途而不是解析参数时，
  所以人会以为留下了半个目录 —— 紧跟那条 `ls` 量过了：**失败是干净的，不用手工收拾**。
- 两条 fatal 症状完全不同，别混：`already used by worktree at` = 分支被占用（改 `--detach` 或新建分支）；
  `invalid reference` = 你说的东西不存在（拼错了，或还没 fetch 下来）。

想开第二份又不占用 `main`，就地新建分支：

## 第 4 节 worktree 里的 `.git` 是【文件】不是目录

「共享」两个字的物理证据全在这一节。

```text
$ file ../app-hotfix/.git; echo "--- 内容就一行："; cat ../app-hotfix/.git
../app-hotfix/.git: ASCII text
--- 内容就一行：
gitdir: <沙箱>/app/.git/worktrees/app-hotfix
[退出码 0]
  ↳ 一个 60 字节左右的文本指针，指向主仓库 .git/worktrees/<名字>。工作区自己不带 objects/。

$ echo "--- 这个登记处里装的是该工作区私有的东西："; ls .git/worktrees/app-hotfix
COMMIT_EDITMSG
HEAD
ORIG_HEAD
commondir
gitdir
index
logs
refs
```

**这行在说什么**：`file` 说它是 `ASCII text`（是目录的话会打印 `directory`），内容只有一行 `gitdir: <路径>`，
指向的是**登记处**而不是对象库。（脚本还跑过 `ls -ld .git ../app-hotfix/.git`，那行显示 worktree 的 `.git` 是 66 字节的
普通文件；按引用约定不抄属主列。）登记处里只有**这个工作区私有的状态**：
`HEAD`（在哪个分支）、`index`（暂存区）、`COMMIT_EDITMSG`、`logs`（**它自己的 reflog**）、
`gitdir`（指回那个 `.git` 文件）、`commondir`（指回公共目录）。**没有 `objects/`** —— 对象库确实只有一份。

## 第 5 节 用完怎么收尾：`worktree remove` 与 `worktree prune`

```text
$ rm -rf ../app-bad && echo "已经 rm -rf 掉工作区目录，但没走 git worktree remove"
已经 rm -rf 掉工作区目录，但没走 git worktree remove
[退出码 0]

$ git worktree list
<沙箱>/app        f04660c [main]
<沙箱>/app-bad    f04660c (detached HEAD) prunable
<沙箱>/app-hotfix 7df58fc [hotfix]
[退出码 0]

$ git worktree list --verbose
<沙箱>/app        f04660c [main]
<沙箱>/app-bad    f04660c (detached HEAD)
	prunable: gitdir file points to non-existent location
<沙箱>/app-hotfix 7df58fc [hotfix]
[退出码 0]
  ↳ ↑ list 已经把这条标成 prunable；--verbose 多打一行原因。这些残留登记就躺在 .git/worktrees/ 里：

$ git worktree prune --dry-run --verbose
Removing worktrees/app-bad: gitdir file points to non-existent location
[退出码 0]

$ git worktree prune --verbose; echo "--- prune 之后："; git worktree list; ls .git/worktrees/
Removing worktrees/app-bad: gitdir file points to non-existent location
--- prune 之后：
<沙箱>/app        f04660c [main]
<沙箱>/app-hotfix 7df58fc [hotfix]
app-hotfix
[退出码 0]
  ↳ prune 只做一件事：把「目录已经不存在」的登记删掉。它不会替你删任何真实存在的目录。
```

**这行在说什么**：`prunable` = 「这条登记可以清掉」，多打的那行说清原因：登记还在、它指向的目录没了
（那个 `.git` 文件随目录一起被 rm 了）。`(detached HEAD)` 同时告诉你它是 `--detach` 建的、**没占用任何分支**。
`prune --dry-run --verbose` 是「报名单不动手」；最后那屏把 `ls .git/worktrees/` 也一起量了 ——
删登记之前那个目录里是 `app-bad` + `app-hotfix` 两行，prune 之后只剩 `app-hotfix` 一行（同一次运行的输出顺序见 `/tmp/git09.txt`）。

**为什么**：`rm -rf` 是绕过 git 的动作，prune 是它留下的后遗症处理程序。习惯：**能用 `worktree remove` 就别 `rm -rf`**。

---

## 第 6 节 `--detach` 与 `git -C`：CI / code review 场景

```text
$ git worktree add --detach ../app-review main~1
Preparing worktree (detached HEAD 8b863e3)
HEAD is now at 8b863e3 第 6 轮：40 个数据文件全部改写
[退出码 0]
  ↳ 主工作区正在 main 上写着半成品；这里用 --detach 检出「main 的上一个提交」，HEAD 不挂任何分支，
  ↳ 既不占用分支，也绝不会误把分支指针推走 —— CI 拉一份代码跑测试、review 一眼旧版本，都是这个套路。

$ git -C ../app-review log --oneline -1; git -C ../app-review status | head -3
8b863e3 第 6 轮：40 个数据文件全部改写
Not currently on any branch.
nothing to commit, working tree clean
[退出码 0]
```

**这行在说什么**：括号里给的是 sha 不是分支名，这就是「不占用分支」的写法；`main~1` = main 的前一个提交（05 章的祖先语法）。
`Not currently on any branch.` 是**游离 HEAD 的标准自我介绍**（第 11 节子模块里会再撞一次，那次是被动撞上的）。
脚本还顺手用这个工作区做了两次只读侦察：`git -C ../app-review ls-files | head -3` 列出 `README.md`、`src/part01.txt`、`src/part02.txt`；
`git -C ../app-review diff --stat main main~1` 报 `README.md | 1 -`、`1 file changed, 1 deletion(-)` ——
**只读比较两个提交根本不需要切任何东西**。

游离状态下想留下东西，就地开分支：

## 第 7 节 submodule 起步：库仓库、库的假远程、宿主仓库

**场景设定（后半章都在这套目录里）**：

    <沙箱>/sm/
      math/      库作者的工作副本（普通仓库，main 分支）
      math.git/  库的「远程」= bare 仓库
      host/      宿主仓库，把库挂在 libs/math 下
      host.git/  宿主的「远程」= bare 仓库

```text
$ cd "$SB/sm" && echo "--- 假远程 math.git 的分支：" && git -C math.git log --oneline
--- 假远程 math.git 的分支：
f3e4bf7 math: mul()
e9c2cc0 math: add()
[退出码 0]
  ↳ ↑ 库这边：math = 库作者的工作副本，math.git = 假远程（bare），HEAD 现在指着含 mul() 的提交。

$ git submodule add ../math.git libs/math
Cloning into '<沙箱>/sm/host/libs/math'...
fatal: transport 'file' not allowed
fatal: clone of '<沙箱>/sm/math.git' into submodule path '<沙箱>/sm/host/libs/math' failed
[退出码 128]
  ↳ ★ 上面这条是故意失败的。git 从 2.38.1 的安全修复起，默认不允许「子进程」用 file 协议克隆仓库，
  ↳   而 submodule 的克隆正是父进程 spawn 出来的子进程，于是被拦。
  ↳   真实项目用 https://... 的 URL 不会有这个问题；本章全程本地路径，所以每条都补一个
  ↳   -c protocol.file.allow=always（只对本次调用生效，不写任何配置）。

$ git -c protocol.file.allow=always submodule add ../math.git libs/math
Cloning into '<沙箱>/sm/host/libs/math'...
done.
[退出码 0]
  ↳ ↑ 这次成功。注意 ../math.git 是相对宿主仓库写的 —— .gitmodules 里存的是相对路径（下一行就能看到）。

$ git status --short
A  .gitmodules
A  libs/math
[退出码 0]

$ echo "--- .gitmodules 的全部内容："; cat .gitmodules
--- .gitmodules 的全部内容：
[submodule "libs/math"]
	path = libs/math
	url = ../math.git
[退出码 0]
```

**这行在说什么**
- `transport 'file' not allowed` 是**安全策略**，不是你 URL 写错了：克隆时 git 会跑目标仓库自带的 hooks，
  恶意仓库可以借此执行命令，2.38.1 起把**子进程**的 file 协议默认关掉。用 GitHub/GitLab 的 https URL 永远碰不到。
  补救：命令上带 `-c protocol.file.allow=always`（只对本次生效，不落配置）。
- 成功那次只多一行 `done.`（失败那次连 `done.` 都没有，只有两个 fatal）。
- `submodule add` 只留下**两个新增项**：`.gitmodules`（配置）和 `libs/math`（那条 gitlink，下一节解剖）。
  **库里的文件一个都不在宿主的暂存区里** —— 这是本章最重要的一个「看不到」。
- `.gitmodules` 是**要提交的**文件（上一屏那个 `A`），一个 submodule 一节；`url = ../math.git` 是相对宿主仓库根算的，
  想让别人 clone 后也能找到库，这一行必须对**他们**可达。

## 第 8 节 宿主记的不是内容，是一条 `160000` 的 gitlink

```text
$ git ls-files -s
100644 51a278ab85ad5d798c949ff205589f639ac7fca4 0	.gitmodules
100644 c01d5770c5819cdbc2b1fc5a3f023310f8b5834c 0	README.md
160000 f3e4bf7cad8092828dabeb6fa4ee9a1832f0b2bd 0	libs/math
[退出码 0]
  ↳ ↑ 看模式列：README.md / .gitmodules 是 100644（普通文件），libs/math 是 160000。
  ↳   160000 是一种特殊的索引项：只存一个 commit sha，不存任何文件内容，术语叫 gitlink。

$ echo "--- diff --cached 里它的样子（new file mode 160000 + Subproject commit）："; git diff --cached -- libs/math
--- diff --cached 里它的样子（new file mode 160000 + Subproject commit）：
diff --git a/libs/math b/libs/math
new file mode 160000
index 0000000..f3e4bf7
--- /dev/null
+++ b/libs/math
@@ -0,0 +1 @@
+Subproject commit f3e4bf7cad8092828dabeb6fa4ee9a1832f0b2bd
[退出码 0]
```

**这行在说什么**：`git ls-files -s` 三列 = 模式 / sha / 暂存槽号。`100644` 后面跟的是**文件内容**的 blob sha，
`160000` 后面跟的是**另一个仓库的 commit** sha（`120000` 是符号链接、`040000` 是目录，`160000` 是这几类里唯一的「非文件」）。
第二段就是**「`+Subproject commit def` 到底在比什么」的出处**：diff 没有别的办法表达「一个不是文件的条目」，
于是伪造出一行文本 `Subproject commit <完整 sha>`。**它不是文件内容**。
`index 0000000..f3e4bf7` 里的 `0000000` = 这一项之前不存在（配合上一行 `new file mode 160000`）。

```text
$ echo "--- 宿主自己能不能拿出那个 commit 对象？"; git cat-file -t "$(git rev-parse HEAD:libs/math)"
--- 宿主自己能不能拿出那个 commit 对象？
fatal: git cat-file: could not get object info
[退出码 128]
  ↳ ★ 这条是故意失败的，而且失败得非常有说服力：宿主的对象库里【根本没有】那个 commit 对象，
  ↳   索引里那行 160000 只存了一个 40 字节的 sha 数字而已。

$ echo "--- 同一个 sha，问库自己："; git -C libs/math cat-file -t "$(git rev-parse HEAD:libs/math)"
       echo "--- 库的文件内容只在库的对象库里："; git -C libs/math cat-file -p HEAD:calc.c | head -2
--- 同一个 sha，问库自己：
commit
--- 库的文件内容只在库的对象库里：
int add(int a,int b){return a+b;}
int mul(int a,int b){return a*b;}
[退出码 0]
  ↳ 所以：宿主记的是「libs/math 必须正好是这个 commit」，文件内容由库自己的仓库负责存。
```

**这行在说什么**：`$(git rev-parse HEAD:libs/math)` 在宿主里只是**读树里的一个数字**，不需要对象存在；
换成 `git -C libs/math cat-file` 到**库**里问，答案才是 `commit`。同一条命令换个目录就活 —— 差的是对象库，不是 sha。

**为什么**：记住「一条 sha 而已」，后面五个符号、四种失败、两步推送顺序全都推得出来。

---

## 第 9 节 三种脏状态：`(untracked content)` / `(modified content)` / `(new commits)`

宿主已经记下 `f3e4bf7`。现在去库的工作目录里做三种「不干净」，看宿主分别怎么反应。
干净时 `git status --short` 一行不输出，`git submodule status` 前缀是**一个空格**：
` f3e4bf7cad8092828dabeb6fa4ee9a1832f0b2bd libs/math (heads/main)`。

### ① 库里只多了未跟踪文件

```text
$ echo "临时草稿，没跟踪" > libs/math/NOTE.md
       echo "=== ① 库里只多了未跟踪文件 ==="; git status --short; git status | tail -4
=== ① 库里只多了未跟踪文件 ===
 ? libs/math
  (commit or discard the untracked or modified content in submodules)
	modified:   libs/math (untracked content)

no changes added to commit (use "git add" and/or "git commit -a")
[退出码 0]
  ↳ ↑ (untracked content)：短状态第二位是 ?。宿主的 gitlink 一点没变。

$ echo "--- 宿主的 diff 对未跟踪内容一个字都不说："; git diff -- libs/math; echo "[上面真的没有输出]"
--- 宿主的 diff 对未跟踪内容一个字都不说：
[上面真的没有输出]
[退出码 0]
```

**这行在说什么**：`? ` 落在**第 2 格**（工作区那格）；长状态里那句 `(commit or discard the untracked or modified content in submodules)`
是 git 在明说「要处理请进子模块」。而 `git diff -- libs/math` **一个字节都没输出** ——
这就是「**`git status` 为什么看不见子模块内部的改动**」的第一半答案：
diff 比的是两侧那个 sha 数字，数字没变就是空；未跟踪文件从来不在任何 diff 的范围里（02 章）。

### ② 改了库里【已跟踪】的文件

```text
$ rm libs/math/NOTE.md
       echo "int untracked_helper(void){return 0;}" >> libs/math/calc.c
       echo "=== ② 改了库里【已跟踪】的文件 ==="; git status --short; git status | tail -4
=== ② 改了库里【已跟踪】的文件 ===
 m libs/math
  (commit or discard the untracked or modified content in submodules)
	modified:   libs/math (modified content)

no changes added to commit (use "git add" and/or "git commit -a")
[退出码 0]
  ↳ ↑ (modified content)：短状态第二位是 m。这两种都只是「库里面不干净」，宿主记录的 sha 没变。

$ echo "--- 这时 diff 会出现一个假 sha（后面挂了 -dirty）："; git diff -- libs/math
       echo "--- 想看清是什么，用 --submodule 只要一句话摘要："; git diff --submodule -- libs/math
--- 这时 diff 会出现一个假 sha（后面挂了 -dirty）：
diff --git a/libs/math b/libs/math
--- a/libs/math
+++ b/libs/math
@@ -1 +1 @@
-Subproject commit f3e4bf7cad8092828dabeb6fa4ee9a1832f0b2bd
+Subproject commit f3e4bf7cad8092828dabeb6fa4ee9a1832f0b2bd-dirty
--- 想看清是什么，用 --submodule 只要一句话摘要：
Submodule libs/math contains modified content
[退出码 0]
  ↳ 顺带一提：那个 -dirty 不是真 sha，只是 diff 用来表达「工作区比这个 sha 脏」的写法。
```

**这行在说什么**：**左右两个 sha 是同一个 `f3e4bf7…`**，只有右边多了 `-dirty`。
这是「`-Subproject commit abc` / `+Subproject commit def` 在比什么」的**反面教材**：
两个 sha 相同 + 带 `-dirty` = **内容脏了、宿主什么都没变**，该进库里去看，宿主这边无可提交。
`--submodule` 那句摘要 `contains modified content` 才是人话。嫌噪音烦可以用 `git status --ignore-submodules=dirty`
（实测那一屏 `git status --short` 输出为空）或 `=untracked`（实测照样报 ` m`，因为改的是已跟踪文件）；
取值还有 `all`。要长期生效就写宿主仓库的 **local** 配置，绝不用 `--global`。

### ★ 宿主根本带不走库里的文件改动

```text
$ echo "int divi(int a,int b){return a/b;}" >> libs/math/calc.c
             git -C libs/math commit -q -am "math: divi()"
             echo "=== ③ 库里 HEAD 前进了一个提交 ==="; git -C libs/math log --oneline -2
             echo "=== 宿主这时看它："; git status --short; git status | tail -4
=== ③ 库里 HEAD 前进了一个提交 ===
fa25134 math: divi()
f3e4bf7 math: mul()
=== 宿主这时看它：
 M libs/math
  (use "git restore <file>..." to discard changes in working directory)
	modified:   libs/math (new commits)

no changes added to commit (use "git add" and/or "git commit -a")
[退出码 0]
  ↳ ↑ (new commits)：短状态变成 " M"。只有这一种才是宿主要记录的东西。

$ echo "--- 未暂存的 gitlink diff：index 行带 160000，一个 sha 换成另一个 sha"; git diff -- libs/math
--- 未暂存的 gitlink diff：index 行带 160000，一个 sha 换成另一个 sha
diff --git a/libs/math b/libs/math
index f3e4bf7..fa25134 160000
--- a/libs/math
+++ b/libs/math
@@ -1 +1 @@
-Subproject commit f3e4bf7cad8092828dabeb6fa4ee9a1832f0b2bd
+Subproject commit fa251344595075ab7266fd28965a75d814f035e7
[退出码 0]
```

**这行在说什么**：符号从 ` m` 变成 **` M`**、尾巴从 `(modified content)` 变成 **`(new commits)`**；
而这次 diff 里 **`f3e4bf7..fa25134` 是两个不同的 sha** —— 这才是「`-Subproject commit abc` / `+Subproject commit def` 真的在比版本」的样子。
行尾那个 `160000` 再次确认它是 gitlink。想看库里的真实内容差异用 `--submodule`（默认档只列 `> math: divi()` 这样的提交清单）、
`--submodule=diff`（实测把 `int divi(int a,int b){return a/b;}` 那一行完整展开进宿主的 diff）。

### 按规矩收尾：先推库，再 bump 宿主

## 第 10 节 clone 下来子模块是空的：`update --init` / `--depth 1` / `--remote`

**场景**：另一个人（脚本里叫 `fresh`）第一次 clone 宿主，**没加** `--recurse-submodules`。
`ls -A libs/math | wc -l` 量出来是 **0** —— 一个**空目录**，连 `.git` 指针文件都没有：

```text
$ cd fresh && git status --short; echo "--- 状态是干净的，可库里就是没东西"; git submodule status
--- 状态是干净的，可库里就是没东西
-fa251344595075ab7266fd28965a75d814f035e7 libs/math
[退出码 0]
  ↳ ↑ 前缀 - 表示「还没初始化」。宿主里那条 160000 只是一个 sha，文件内容得靠 submodule 命令去拉。
```

**这行在说什么**：**`git status --short` 完全没有输出** —— 状态「干净」，但库里空的。
这就是「为什么 `git status` 看不见子模块内部」的**第二半答案**：宿主只比 sha，`fresh` 的 sha 和索引一致，所以它觉得一切正常。
**空目录不是脏，是没注册。** 唯一说实话的是 `submodule status` 的前缀 `-`。

### 为什么要 `--init`：一次失败把两件事拆开了

```text
$ cd fresh && git -c protocol.file.allow=always submodule update --init
Cloning into '<沙箱>/sm/fresh/libs/math'...
done.
Submodule path 'libs/math': checked out 'fa251344595075ab7266fd28965a75d814f035e7'
[退出码 0]

$ cd fresh && ls libs/math; git submodule status; git status --short; echo "--- 拉下来的文件是真的："; cat libs/math/calc.c
README.md
calc.c
 fa251344595075ab7266fd28965a75d814f035e7 libs/math (heads/main)
--- 拉下来的文件是真的：
int add(int a,int b){return a+b;}
int mul(int a,int b){return a*b;}
int divi(int a,int b){return a/b;}
[退出码 0]
```

**这行在说什么**：这次没有 `registered` 那行（上一轮已注册），直接克隆 + 检出。
`checked out 'fa2513…'` **打印的是 sha 而不是分支名** —— 这一句在预告第 11 节。
之后前缀从 `-` 变空格、`calc.c` 里三个函数都在；`cat libs/math/.git` 在新克隆里同样是
`gitdir: ../../.git/modules/libs/math`（第 7 节那两个细节原样复现）。
CI 里最常见的是一次到位：`git clone --recurse-submodules`（脚本用 `one-shot` 目录验了一次，
它等价于 clone + `submodule update --init --recursive`，本地路径同样要带 `-c protocol.file.allow=always`）。

### `--depth 1`：这一条**本章没能验证**

## 第 11 节 子模块的 detached HEAD：提交差点丢掉

第 10 节末尾那条 `update` 把库放回了宿主记的 sha，于是 `fresh/libs/math` 里：

```text
$ git status | head -3
HEAD detached at fa25134
nothing to commit, working tree clean
[退出码 0]
  ↳ ↑ HEAD detached at <sha>：update --init 检出的是宿主记录的那个 commit 对象，不是任何分支。
  ↳   这是设计如此（宿主只认 sha），但它意味着：你在这里 commit 的东西一开始不属于任何分支。

$ git branch -a; echo "--- 当前 HEAD："; git rev-parse --short HEAD
* (HEAD detached at fa25134)
  main
  remotes/origin/HEAD -> origin/main
  remotes/origin/main
--- 当前 HEAD：
fa25134
[退出码 0]
```

**这行在说什么**：**「游离 HEAD 是怎么回事」的完整答案就这一屏**：分支 `main` 存在、远程跟踪分支存在，
但 HEAD 谁也不挂，直接指着 commit `fa25134`。为什么：宿主只记 sha，git 无法确定该挂哪个分支（`main` 可能已经跑到别处）。

事故来了 —— 手快提交，然后「觉得不对劲」：

```text
$ git reflog | head -6
fa25134 HEAD@{0}: checkout: moving from b690a27bdc97c4e7f70c186e00dba4a4ceb841ce to main
b690a27 HEAD@{1}: commit: math: mod()（手快，在 detached HEAD 上提交了）
fa25134 HEAD@{2}: checkout: moving from a2cd9271dd2677bd9a67fabe411d43dab39a8189 to fa251344595075ab7266fd28965a75d814f035e7
a2cd927 HEAD@{3}: checkout: moving from fa251344595075ab7266fd28965a75d814f035e7 to a2cd9271dd2677bd9a67fabe411d43dab39a8189
fa25134 HEAD@{4}: checkout: moving from main to fa251344595075ab7266fd28965a75d814f035e7
fa25134 HEAD@{5}: clone: from <沙箱>/sm/math.git
[退出码 0]
```

**这行在说什么**：`HEAD@{1}` 那行 `commit: math: mod()（手快…）` 就是被冲掉的那笔，左边 hash `b690a27`。
这条 reflog 还顺手把第 10 节发生过什么全交代了：`HEAD@{3}` 是 `--remote` 拉到 `a2cd927`、
`HEAD@{2}` 是那条 `update` 退回 `fa25134` —— **两次 `checkout: moving from` 都不是你手工切的，是 `submodule update` 切的**。
这就是子模块 reflog 常让人惊呼「我什么时候切过分支」的原因。
救援只要一步：`git branch 救回来的-mod b690a27`（脚本还 `git checkout 救回来的-mod` 确认了 `On branch 救回来的-mod`）。

第②步回宿主 bump，然后**故意只做这一步就去 push**：

```text
$ echo "--- 现在直接推宿主（它会带上一个库的、还没推出去的 sha）："; git push --recurse-submodules=check origin main
--- 现在直接推宿主（它会带上一个库的、还没推出去的 sha）：
The following submodule paths contain changes that can
not be found on any remote:
  libs/math

Please try

	git push --recurse-submodules=on-demand

or cd to the path and use

	git push

to push them to a remote.

fatal: Aborting.
fatal: the remote end hung up unexpectedly
[退出码 128]
  ↳ ★ 故意失败：--recurse-submodules=check 发现 gitlink 指的 sha 在任何远程上都找不到，于是拒绝推送。
  ↳   它拦的就是「推上去别人 clone 不出来的宿主」这种事故。补救只有第 ① 步：把库也推上去。
```

**这行在说什么**：`changes that can not be found on any remote: libs/math` 检查的是
**宿主记的这个 sha 在库的远程上有没有**，此刻 `b690a27` 只在你磁盘上。
中间缩进的两行命令是 git **抄给你的补救办法**，不是报错内容。两条 fatal 叠在一起：
`Aborting.` 是本地检查拦的、`the remote end hung up unexpectedly` 是随后收尾时报的 —— **别被第二条误导成网络问题**。
补救后（`git -C libs/math push -q origin 救回来的-mod`，`branch -r` 里就多了 `origin/救回来的-mod`）再检查就过：
`8fdbd4b..58a9dad  main -> main`（`..` = 快进），并且脚本直接 `git -C ../host.git log --oneline -1` 问「假远程」，
答 `58a9dad host: libs/math 升到带 mod() 的提交` —— 服务端确实收到了。

**为什么**：submodule 的依赖是**跨仓库的 sha 引用**；引用能解析的前提是那个 sha 在某个远程上永久存在。
所以顺序永远是**先库后宿主**，`--recurse-submodules=check` 就是这条纪律的看门狗
（`on-demand` 会顺手替你推库，本章宁可手动一步，让你知道推了什么；`on-demand` 本身没跑过）。

---

## 第 12 节 `foreach` / `submodule.recurse` / `deinit` 与彻底移除

```text
$ git submodule deinit libs/math
error: the following file has local modifications:
    libs/math
(use --cached to keep the file, or -f to force removal)
fatal: Submodule work tree 'libs/math' contains local modifications; use '-f' to discard them
[退出码 128]
  ↳ ★ 故意失败：库的工作区里还有未提交改动，deinit 不肯替你扔。要么先提交/清掉，要么明确加 -f。

$ git submodule deinit -f libs/math; echo "--- deinit 之后："; ls -A libs/math | wc -l; echo "（0 = 工作区被清空了）"
Cleared directory 'libs/math'
Submodule 'libs/math' (../math.git) unregistered for path 'libs/math'
--- deinit 之后：
0
（0 = 工作区被清空了）
[退出码 0]
```

**这行在说什么**：deinit 的报错有**两层**（`error:` 是 `git rm` 那一路的话、`fatal:` 是 submodule 层的判断）。
加 `-f` 之后两件事都做了：`Cleared directory`（`ls -A | wc -l` = 0）+ `unregistered`
（实测 `git config --local --get-regexp "^submodule\."` 返回空、打印 `(空)`）。
但 **`.gitmodules` 一个字没动** —— 那是要提交的文件，git 不替你改版本库里的东西。
而 `.git/modules/libs/math` 里的 `objects`/`refs` 也还在（第 7 节埋的那句话在这儿兑现：deinit 不会丢库的历史）。

```text
$ git rm libs/math; echo "--- rm 之后索引里还有吗："; git ls-files -s; echo "--- .gitmodules 现在的内容："; cat .gitmodules; echo "（这一版 git 把那一节也一起删了）"
rm 'libs/math'
--- rm 之后索引里还有吗：
100644 e69de29bb2d1d6434b8b29ae775ad8c2e48c5391 0	.gitmodules
100644 c01d5770c5819cdbc2b1fc5a3f023310f8b5834c 0	README.md
--- .gitmodules 现在的内容：
（这一版 git 把那一节也一起删了）
[退出码 0]
```

**这行在说什么**：`160000` 那一行**从索引里消失了**。`.gitmodules` 现在是**空文件** ——
blob sha `e69de29…` 是 git 里**「空内容」的固定指纹**，看到它以 `e69de29` 开头就知道那文件 0 字节。
**这一版 git（2.55）在 `git rm <submodule 路径>` 时顺手把 `.gitmodules` 里那一节也删了**；
老版本不会删、要手工编辑再提交 —— 这就是网上教程互相矛盾的原因。
`git status --short` 此时是 `M  .gitmodules` + `D  libs/math`（`git rm` 已经帮你暂存了），提交即可。

第四步是可选的清扫：`rm -rf .git/modules/libs`（删完后 `ls -A .git/modules` 空、`ls -A libs/math` 报 No such file —— 后者早在 `git rm` 那步就没了）。
然后脚本又加回来一次，用一条证据确认第四步真的生效了：

## 第 13 节 submodule vs subtree：把库真正长进宿主里

`git-subtree` 不是内建命令（contrib 脚本，很多发行版随 `git-all` / `git-subtree` 包安装），第 0 节探测到了所以真跑，
**`add` / `pull` / `--squash` 三种都演了**。

```text
$ git -c protocol.file.allow=always subtree add --prefix third_party/math ../libcopy main
git fetch ../libcopy main
From ../libcopy
 * branch            main       -> FETCH_HEAD
Added dir 'third_party/math'
[退出码 0]
  ↳ ↑ subtree add 干的就是一次 fetch + 一次 merge：打印出来的第一行就是它替你跑的命令。

$ ls third_party/math; echo "--- 这些是宿主的【真实文件】，不是 160000："; git ls-files -s third_party/math
README.md
calc.c
--- 这些是宿主的【真实文件】，不是 160000：
100644 5c14c63a2e96b7a85d2f1f2a00fe0636695bbec2 0	third_party/math/README.md
100644 5fe881fff554cd2860da049f558c480c7e9ef65c 0	third_party/math/calc.c
[退出码 0]
```

**这行在说什么**：第一屏把**它替你跑的命令**打印出来了（`git fetch ...`）—— subtree 没有魔法。
第二屏**和第 8 节逐字对照着看**：模式是 `100644`、后面是**内容 blob 的 sha**、**每个文件一行**。
subtree 的世界里根本不存在「另一个仓库」，只有你的文件。

## 第 14 节 收尾自检：体积对比 / `git fsck` / 沙箱里没留垃圾

```text
$ echo "=== 同一份代码，三种做法的磁盘成本 ==="; du -sh app app-clone app-hotfix
=== 同一份代码，三种做法的磁盘成本 ===
7.9M	app
7.8M	app-clone
1.8M	app-hotfix
[退出码 0]

$ echo "--- 各自的 .git（worktree 那个只是个几十字节的指针文件）："; du -sh app/.git
       echo "    clone 副本的 .git（一整份历史，这才是真金白银）："; du -sh app-clone/.git
       echo "    worktree 工作区里的 .git：第一列 - = 文件，中间那个数字是字节数"; find app-hotfix/.git -maxdepth 0 -printf "%y %s %p\n"
--- 各自的 .git（worktree 那个只是个几十字节的指针文件）：
6.2M	app/.git
    clone 副本的 .git（一整份历史，这才是真金白银）：
6.1M	app-clone/.git
    worktree 工作区里的 .git：第一列 - = 文件，中间那个数字是字节数
f 66 app-hotfix/.git
[退出码 0]

$ echo "--- worktree 的登记元数据一共占这么点："; du -sh app/.git/worktrees; ls app/.git/worktrees
--- worktree 的登记元数据一共占这么点：
28K	app/.git/worktrees
app-hotfix
[退出码 0]
```

**这行在说什么**：**1.8M vs 7.8M** —— worktree 只有工作文件，它的 `.git` 是一个 **66 字节的文本文件**
（`find -printf "%y %s %p"` 那行：第一列 `f` = 普通文件，第二列就是字节数，里面只有一行 `gitdir:` 指向主仓库的登记目录）；
再 clone 一份要多花 6.1M 一整份历史。全部工作区登记元数据（HEAD、index、reflog 那些）**一共 28K**。
`app` 从第 1 节的 7.8M 变 7.9M，是因为期间在 hotfix 工作区又提交了几笔（对象进的还是同一个库）。

下一屏是 submodule / subtree 的磁盘账，脚本这次**每个路径单独调用一次 `du`**：

```text
$ echo "--- submodule 宿主的体积拆解（每个路径单独量：du 会把嵌套的参数跳过，一次列多个只出一行）"; du -sh sm/host
       echo "    其中 ① 库的工作区文件（历史不在这里）："; du -sh sm/host/libs
       echo "    其中 ② 宿主的整个 .git："; du -sh sm/host/.git
       echo "    其中 ② 里的 .git/modules = 被搬进来的库历史："; du -sh sm/host/.git/modules
--- submodule 宿主的体积拆解（每个路径单独量：du 会把嵌套的参数跳过，一次列多个只出一行）
444K	sm/host
    其中 ① 库的工作区文件（历史不在这里）：
12K	sm/host/libs
    其中 ② 宿主的整个 .git：
424K	sm/host/.git
    其中 ② 里的 .git/modules = 被搬进来的库历史：
224K	sm/host/.git/modules
[退出码 0]

$ echo "--- 库的假远程（真正的历史在这里）："; du -sh sm/math.git
       if [ -d st ]; then
         echo "--- subtree 的做法（库内容 + 库全部历史都进了宿主）："; du -sh st/app
         echo "    其中 bring 进来的目录："; du -sh st/app/third_party
       else
         echo "（第 13 节没跑，没有 st 目录）"
       fi
--- 库的假远程（真正的历史在这里）：
204K	sm/math.git
--- subtree 的做法（库内容 + 库全部历史都进了宿主）：
256K	st/app
    其中 bring 进来的目录：
8.0K	st/app/third_party
[退出码 0]
```

`sm/host` 444K 里只有 12K 是库的**工作文件**，剩下 424K 全是宿主自己的 `.git`，其中 224K 是被搬进去的库历史
（`.git/modules`）—— **submodule 贵的不是那几个文件，是又养了一份历史**。
对比 subtree：`st/app` 256K，`third_party` 只占 8.0K，但库的全部提交已经进了宿主的 pack。
（`du` 是文件系统视角，不是 git 语义；**绝对数字换一台机器就会变**，能复用的是「谁占大头」这个结构。
第一版这里写成「一次 `du -sh 父 子` 只报父目录」，那是旧脚本的写法，现在每个路径单独量，结论不再依赖那个行为。）

```text
$ echo "--- 从 worktree 里数对象：和主仓库一模一样（没有第二份历史）"; git -C app-hotfix count-objects -v | head -3
--- 从 worktree 里数对象：和主仓库一模一样（没有第二份历史）
warning: garbage found: <沙箱>/app/.git/worktrees/app-hotfix/refs
count: 4
size: 36
in-pack: 309
[退出码 0]
```

**这行在说什么**：`in-pack: 309` 和第 1 节主仓库那个 **一模一样** —— pack 只有一个。
`count: 4 / size: 36` 是第 2 到第 13 节期间新提交出来的几个松散对象，它们躺在**同一个** `.git/objects` 里，
worktree 这边一个都没多。
那条 `warning: garbage found: .../worktrees/app-hotfix/refs` 是**真实噪音**：第 4 节 `ls` 过登记处里那个 `refs`（空目录），
`count-objects` 期望它是文件，于是叫 garbage。**它不是数据损坏**，证据是紧接着的三条 `git fsck`
（`app`、`sm/host`、`sm/fresh` 各一条）**全部无输出、退出码 0** —— `fsck` 没输出就是没坏（08 章教你读它）。
脚本最后还对四个仓库各跑一次 `git worktree list`，每个都只剩主工作区（`app` 是两条：主 + `app-hotfix`），
**没有任何 prunable 残留** —— 第 5 节讲的收尾纪律在这儿兑现。

**为什么**：每次实验完问三件事 —— `fsck` 干净吗？`worktree list` 有 prunable 吗？沙箱里有没有多出来的目录？
前两条只读，在真实仓库上同样适用。

---

## 一张表：worktree vs submodule vs subtree

**worktree 管「一个仓库多份工作目录」，submodule / subtree 管「多个仓库合起来用」，它们不是同一层的竞品**；
submodule 和 subtree 才是真竞品。每格都标了本章是否演过。

| 维度 | worktree | submodule | subtree | subtree `--squash` |
|---|---|---|---|---|
| 解决什么 | 一个仓库同时开多份工作目录 | 仓库套仓库，锁定到某个 commit | 把库变成自己的文件 + 带上游历史 | 同上，但只留内容 |
| 磁盘上多什么 | 一份工作文件 + 28K 登记（实测） | **一整个库仓库**（宿主 `.git/modules` 里） | 库的文件和提交都进宿主 pack | 库的文件，历史压成一条 |
| 索引里长什么样 | 不涉及 | `160000 commit <sha>`（第 8 节实测） | `100644 <blob sha>` 逐文件（第 13 节实测） | 同左 |
| 别人 clone 之后 | 不随 clone 传播（工作区在各人机器上） | **目录是空的**，要 `update --init`（第 10 节实测） | 文件立刻全在（第 13 节实测） | 同左 |
| 升级依赖版本 | — | 库提交 → 推库 → 宿主 `add` + 提交（两步） | `subtree pull` 一次 + 一个 merge | `pull --squash` + 一条 Squashed |
| 改这些代码回推上游 | 不适用（本来就是同一个仓库） | 能，但必须两步 + 警惕 detached HEAD | 能（`subtree push`，**未演 → 未验证**） | **不行**（上游历史压平了，未验证） |
| 历史观感 | 完全不影响 | 干净，库里的事不出现在宿主 log | `git log` 被上游 + merge 拉长（实测） | 干净，但 `blame` 到不了上游 |
| 典型场景 | 主目录写一半 + 同时构建/评审另一版；CI | 依赖只读、紧跟上游、不想污染历史 | 拿来就当自己代码用 | 同左，且不想背上游历史 |
| 本章验证状态 | ✔ 第 1~6、14 节全程真跑 | ✔ 第 7~12 节（`recurse` 那格除外，见第 12 节） | ✔ 第 13 节（`push` 未演） | ✔ 第 13 节 |

**一句话选型**：同仓库多目录 → **worktree**；依赖只读、要紧跟上游、不想污染自己历史 → **submodule**；
「拿来就当自己代码用」、不想让别人为 submodule 操心 → **subtree**。三个可以并存。

---

## 速查表：按症状查

| 症状（屏幕上的原文片段） | 意思 | 立刻敲 |
|---|---|---|
| `fatal: 'main' is already used by worktree at ...` | 分支被别的工作区检出 | `git worktree list` 看在哪；或 `add --detach`；或新建分支 |
| `fatal: invalid reference: xxx` | 第二个参数不是任何存在的 ref | 核对分支/标签/sha，或先 fetch |
| `git branch` 里出现 `+ 分支名` | 该分支正被另一个工作区占用 | 别 checkout 它，去那个工作区干活 |
| `prunable` / `gitdir file points to non-existent location` | 工作区被手工 `rm -rf` 了 | `git worktree prune --dry-run --verbose` 再 `prune` |
| `contains modified or untracked files, use --force` | 工作区里有没提交的改动 | 先进去提交/清掉；确认不要了才 `--force` |
| `Not currently on any branch.` | 这个工作区是 `--detach` 检出的 | 要留下东西就 `checkout -b 新分支` |
| `fatal: transport 'file' not allowed` | 本地路径 + 子进程克隆被安全策略拦 | 该命令加 `-c protocol.file.allow=always`（别 `--global`） |
| `git status` 干净但子模块目录是空的 | 没注册、没克隆 | `git submodule update --init --recursive` |
| `git submodule status` 前缀 `-` | 未初始化（**唯一**显式线索） | 同上 |
| `git submodule status` 前缀 `+` | 检出 sha ≠ 宿主记的 sha | 要留：`git add <path>` + 提交；不要：`git submodule update <path>` |
| `? libs/math` + `(untracked content)` | 库里多了未跟踪文件，**宿主 sha 没变** | `git -C <path> status` 进去看；或 `--ignore-submodules=untracked` |
| `m libs/math` + `(modified content)` + diff 里 `-dirty` | 库里改了已跟踪文件，**宿主 sha 没变** | 进库里提交（两步），或 `git -C <path> checkout -- <文件>` |
| `M libs/math` + `(new commits)` | 库里 HEAD 前进了，**这才是宿主要记的** | 先推库，再 `git add <path>` + 提交 |
| `git commit -a` 报 `no changes added to commit`（退出码 1） | 库里的内容改动宿主带不走 | 去库里提交，回宿主只 bump gitlink |
| `HEAD detached at <sha>`（在子模块里） | 设计如此：宿主只认 sha | 想改先 `git checkout main` 或 `git switch -c 新分支` |
| `git branch --contains <sha>` 没输出 | 那笔提交成了孤儿 | `git reflog` 找 sha → `git branch 名字 <sha>` |
| `changes that can not be found on any remote` | 宿主记的 sha 在库的远程上不存在 | 先进库 `git push`，再推宿主 |
| `error: ... use --cached to keep the file, or -f to force removal` | deinit 时库工作区有未提交改动 | 清掉改动，或 `git submodule deinit -f <path>` |
| `warning: --depth is ignored in local clones` | 本地路径克隆不吃 `--depth` | 要用 `file://` 或 https（本章未验证） |
| `fsck` 没输出 | 对象库完整 | 好消息，什么都不用做 |
| `warning: garbage found: .../worktrees/<名字>/refs` | worktree 元数据被挑剔，非数据损坏 | 对照 `git fsck`（干净就没事） |

---

## 坑清单（事故排行榜，对照上面各节）

1. **以为「clone 完就有子模块内容」**（第 10 节）：clone 只带你 clone 的那个仓库；
   判据是 `submodule status` 的 `-` 前缀，**这时 `git status` 会骗你说干净**。
2. **只写 `update` 不写 `--init`**（第 10 节）：新 clone 里它无事发生 —— update 只服务已注册的 submodule。
   那次 `registered` 成功、`clone` 被拦的失败正好把两件事拆开了。
3. **在子模块里直接 commit 然后 checkout 走**（第 11 节 ★）：提交变孤儿。
   习惯：**进库先看 `git status`，见到 `HEAD detached` 就 `git switch -c` 再动手**。
4. **只做第②步就去 push 宿主**（第 11 节）：推上去一个别人 clone 不出来的宿主。
   `--recurse-submodules=check` 是刹车，不是故障；别把 `the remote end hung up unexpectedly` 读成网络问题。
5. **把 `-Subproject commit abc` / `+Subproject commit def` 当文件差异读**（第 8、9 节）：它比的是 sha。
   **两边 sha 相同、右边带 `-dirty` = 库里内容脏、宿主没变**（第 9 节 ②）。
6. **`git commit -a` 想连库的改动一起提交**（第 9 节 ★）：退出码 1、`no changes added to commit`。两步走，别抄近路。
7. **`--ignore-submodules=untracked` 以为能压掉一切**（第 9 节）：改的是已跟踪文件时照样报 ` m`。要压掉得用 `dirty`。
8. **`rm -rf` 掉 worktree 目录就完事**（第 5 节）：`.git/worktrees/` 留一条 prunable。正解 `worktree remove`，补救 `prune`。
9. **给 `worktree remove` 顺手加 `--force`**（第 5 节）：那里面可能有你今天写的全部工作。先 `git -C <wt> status`。
10. **以为 `worktree remove` 会删分支**（第 5、6 节）：不会。分支留在 refs 里，要另外 `git branch -d`。
11. **脚本里用 `--git-dir` 定位仓库**（第 4 节）：那是登记处，不是公共目录，缓存会各存一份。一律用 `--git-common-dir`。
12. **删 submodule 只 `git rm`**（第 12 节）：`.git/config` 还注册着、`.git/modules` 还存着整个库历史。四步各擦一块。
13. **照旧教程手工删 `.gitmodules` 那一节**（第 12 节）：这一版 `git rm` 已顺手删了（`.gitmodules` 变空文件 `e69de29…`）。
    **先 `cat` 一眼再动手。**
14. **指望 `--depth 1` 省流量**（第 10 节）：本地路径下它被忽略（原文 warning + `3` vs `3` + `false`）。
15. **把第 12 节那两屏当 `submodule.recurse` 的证据**：搭建失败、两屏一模一样，**这个结论没验证**。
    同理 `push.recurseSubmodules`、`subtree push`、`submodule sync` 都没跑，别从本章推。
16. **看到 `warning:` 就以为仓库坏了**（第 14 节）：`garbage found: .../worktrees/<名字>/refs` 之后三条 `fsck` 全干净。
    先定位它是哪一层的警告，再决定紧不紧张。

---

## 自检问题（先答再看）

**Q1：`git worktree add ../w hotfix` 之后，`../w/.git` 是文件还是目录？哪些东西共享、哪些各自独立？**
<sub>文件：`file` 报 ASCII text，内容只有一行 `gitdir: <主仓库>/.git/worktrees/w`，66 字节（第 4 节）。
共享：对象库、所有 refs、config、remote、stash、hooks。各自：HEAD、工作目录内容、index、
`.git/worktrees/<名字>` 那份登记（HEAD/index/logs/COMMIT_EDITMSG/gitdir/commondir/refs，里面没有 objects/）。
判据命令：`--git-dir` 两个工作区不同、`--git-common-dir` 相同。</sub>

**Q2：为什么 `git worktree add ../app-main2 main` 会 fatal？两条出路是什么？`git branch` 里的 `+` 是什么意思？**
<sub>refs 共享，两个工作区同时提交同一个分支指针必然互相踩（第 3 节 `already used by worktree at`）。
出路：`--detach`（不占用分支，第 6 节 CI 套路）或 `-b 新分支`。`+` = 该分支被别的 worktree 检出，现在 checkout 不上去。</sub>

**Q3：`list` 里出现 `prunable` 说明什么？`remove` / `prune` / `rm -rf` 三者的关系？**
<sub>登记还在但它指的工作目录已不存在（`gitdir file points to non-existent location`），典型成因是手工 `rm -rf`。
`remove` 删目录 + 删登记（脏时拒绝，`--force` 才强删）；`prune` 只删「目录已不存在」的登记，绝不删真实目录。
`remove` 不删分支。</sub>

**Q4：宿主索引里 `libs/math` 那条和其它条目有什么本质区别？怎么用一条命令证明宿主没存库的内容？**
<sub>模式 `160000`（gitlink）而不是 `100644`：后面跟的是**另一个仓库的 commit sha**，不含任何文件内容（第 8 节 `ls-files -s`）。
证明：`git cat-file -t "$(git rev-parse HEAD:libs/math)"` 在宿主里 `fatal: could not get object info`（退出码 128），
同一条命令 `git -C libs/math cat-file -t …` 才答 `commit`。`git ls-tree HEAD libs/math` 的类型也明写 `commit`。</sub>

**Q5：为什么 clone 完子模块目录是空的，`git status` 却说干净？`update --init` 里 `--init` 省不掉的原因是什么？**
<sub>宿主只比 sha：索引里的 `160000` 和工作区那个 gitlink 一致，所以 status 无输出；空目录不是「脏」，是**没注册**（第 10 节）。
唯一线索是 `git submodule status` 的前缀 `-`。`--init` 干的是把 `.gitmodules` 的 url 注册进宿主 `.git/config`；
`update` 只处理已注册的 submodule，所以不 `--init` 会无事发生 —— 那次 `registered` 成功、`clone` 被 file 协议拦的失败正好把两步拆开。</sub>

**Q6：`git diff` 里两次看到 `-Subproject commit f3e4bf7…` / `+Subproject commit …`，一次是内容脏、一次是真的升了版本。怎么区分？**
<sub>看右边的 sha：① 左右是**同一个** `f3e4bf7…` 且右边带 `-dirty` → 库里工作区脏、宿主记的 sha 没变（第 9 节 ②，
配套 `git status --short` 是 ` m`、长状态写 `(modified content)`）；② 右边是**另一个** sha（`f3e4bf7..fa25134 160000`）→
库里 HEAD 前进了（` M` + `(new commits)`），这才是宿主该记的东西。`-dirty` 不是真 sha，只是 diff 的表达方式。</sub>

**Q7：在子模块里提交完、切回 `main`，那笔提交不见了。发生了什么？怎么救？正确的提交/推送顺序是什么？**
<sub>子模块 checkout 出来就是 `HEAD detached at <sha>`（宿主只认 sha），提交只被 HEAD 指着；`checkout main` 之后没人指它 → 孤儿
（`git branch --contains <sha>` 无输出）。救：`git reflog` 找到 `commit: <说明>` 那行左边的 sha → `git branch 名字 <sha>`。
正确顺序永远两步：① 库里提交并把库的分支推到**库的**远程；② 回宿主 `git add <path>` + 提交 bump，再推宿主。
第 ② 步做完就推会被 `--recurse-submodules=check` 拦下（`can not be found on any remote`）。</sub>

**Q8：要彻底删掉一个 submodule，网上说法不一。本章实测的完整步骤是什么？每一步各清掉哪一处残留？**
<sub>四步：`git submodule deinit -f <path>`（清工作区 + 注销 `.git/config` 里的注册；`.gitmodules` 不动、库历史不动）→
`git rm <path>`（删索引里的 gitlink；这一版 git 还顺手把 `.gitmodules` 那一节删空，`cat` 一下就知道）→
`git commit` → `rm -rf .git/modules/<name>`（删留在宿主里的库历史）。
证明第 4 步生效的证据：再 `submodule add` 时会重新打印 `Cloning into … done.`，说明那份 git 目录确实没了。</sub>

---

## 本章没讲什么

- **`--depth 1` 到底省多少流量**：本地路径下 `--depth` 被忽略（第 10 节原文 warning），没验证。要看真效果得用 `file://` 或 https。
- **`submodule.recurse` 的行为差异**：脚本搭建失败了，两屏输出一样（第 12 节原样交代了）。只验证了配置能写能读。
- **`git submodule sync` / `absorbgitdirs` / `update --merge --rebase` / `branch = ` 字段 / `push.recurseSubmodules` 配置 /
  `--recurse-submodules=on-demand`**：全部没跑，只出现过命令名。
- **`git subtree push`（把本地改动推回上游）、subtree 的冲突处理**：没演。第 13 节只演了 `add` / `pull` / `--squash`。
- **`git worktree move` / `lock` / `repair`、多工作区并发提交的安全性**：没演。
- **子模块 + CI 的完整实践、嵌套子模块（孙辈）、`fetchRecurseSubmodules`**：`--recursive` 只在命令里出现过，没造孙辈仓库。
- **monorepo 替代方案（sparse-checkout + `skip-worktree`）**：本章没提。
- **历史里的泄露文件怎么清**：还是 10 章的 `filter-repo`。

---

## 运行 demo

## 下一章预告

**第 10 章《故障现场》**（正在做）：本章你已经见过好几次「命令失败长什么样」了 ——
`fatal: transport 'file' not allowed`、`already used by worktree at`、`could not get object info`、
`no changes added to commit`、`the remote end hung up unexpectedly`。
下一章把这些收拢成一套排查流程：**先分清是仓库坏了、还是索引/引用/工作区不一致**，
`fsck` 的输出怎么读、`dangling` 和 `unreachable` 有什么区别、`index.lock` 怎么办、
`git restore` 救不回来时还剩哪几条路。你在这里学到的「一条 sha 而已」和「共享对象库」，
到故障现场会变成最好用的两条判据。
