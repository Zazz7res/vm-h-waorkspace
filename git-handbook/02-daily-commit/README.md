# 02 日常提交闭环 daily-commit

> 一句话：这一章把 01 章那三个区跑成你每天要重复几十遍的闭环 ——
> **改 → 看现场 → 挑要提交的 → 复核 → 装订 → 回看**，每步用哪条命令、它读的是哪两个区、输出怎么认。

01 章给你的是坐标系（三个区 + 内容寻址 + 两个指针），这一章给的是**手上的动作**。
学完你应该能不看教程、不靠 IDE 按钮，纯命令行做出一条干净的提交，
并且在按回车之前就说清"这次提交里到底有什么"。本章所有输出都是 `bash demo.sh` 真跑出来的，一条没编。

---

## 你会遇到的场景

| 你遇到的事 | 本章哪一节 |
|---|---|
| `git init` 和 `git clone` 哪个才是"开始一个项目"？clone 出来的仓库多了什么？ | 场景 1 |
| `git status -s` 里 `MM` `AD` `D ` ` M` `R ` `??` 这些字母到底怎么读 | 场景 2 |
| 一个文件改了 20 处，只想提交其中 2 处（不想让调试代码进历史） | 场景 3 |
| `add -A` / `add -u` / `add .` 到底差在哪，我该用哪个 | 场景 3 |
| `git commit -am "..."` 之后新文件怎么还是没进去 | 场景 4 |
| 提交说明写错了 / 漏了文件，想就地换掉最后一条 | 场景 4 |
| `git commit` 不带 `-m` 弹出一堆东西，我提交成功了吗 | 场景 4 加餐 |
| `git diff`、`--staged`、`diff HEAD` 三条各比的是哪两个区 | 场景 5 |
| 想直接拿到"上个版本这个文件长什么样" | 场景 5 |
| `A..B` 和 `A...B` 有什么区别，`git log` 除了翻页还能干什么 | 场景 6 |
| 我领先/落后几条，什么时候能放心合 | 场景 6 |
| **`.env` 已经提交进仓库了，现在补 `.gitignore` 为什么没用** | 场景 7（本章最值钱的一节） |
| `git rm` / `git rm --cached` / `git mv` 分别动了哪几个区 | 场景 2、7 |
| `.gitignore` 写了 `build/`，为什么 `!build/keep.txt` 不生效 | 场景 8 |
| 想知道某个路径到底被哪条规则拦住 | 场景 8 |
| 想给长命令起短名，又怕污染全局配置 | 场景 9 |

---

## 心智模型

### 图 1：一天的闭环（六步，每步只做一件事）

```
   ①改            ②看现场             ③挑               ④复核              ⑤装订            ⑥回看
 编辑文件   →   git status -sb   →   git add -p    →  git diff --staged →  git commit    →  git log
     │               │                   │                  │                 │               │
     │               │                   │                  │                 └ 暂存区 → 仓库（造一个 commit 对象）
     │               │                   │                  └ 读「暂存区 vs 仓库」= 这次会带上什么
     │               │                   └ 工作区 → 暂存区（复制内容进 objects + 写进清单）
     │               └ 三个区两两比较，报告哪里不一致（不改任何东西）
     └ 只动工作区，Git 完全不知道

   整条链上只有两个地方真的写数据：③（objects + index）和 ⑤（objects + refs）。
   ②④⑥ 全是只读的"看一眼"，可以任意位置反复插进去看。
```

为什么④必须单独看一眼：`git commit` 装订的是**清单**，不是你硬盘上的文件（01 章场景 3 的铁证）。
跳过④最常见的结果就是"提交进去的版本是半个"，或者"调试打印跟着上线了"。

### 图 2：`git status -s` 的两个格子 = 两次比较，不是一个状态

```
        第1格                        第2格        文件名
      ┌───┐                       ┌───┐        ┌────────
      │   │← 暂存区 vs 仓库         │   │← 工作区 vs 暂存区
      └───┘                       └───┘
   非空 = "下次提交里有它"         非空 = "还有改动没进下次提交"

   MM  both.txt      = 暂存区比仓库多了点东西（第1格 M）**并且** 工作区又比暂存区多了点（第2格 M）
    M  modified.txt  = 前面那个空格不是噪音，它说的是"暂存区和仓库一致"，只有工作区变了
   ?? brandnew.txt   = 两格都不成立 —— 清单里根本没这一行
```

本章 demo 一次造出 9 种组合，全部真实出现在同一条 `git status -s` 里（场景 2）。

### 图 3：`diff` 全家福 —— 三条命令读三角形的三条边

```
              工作区                 ← 你正在写的东西
             /      \
      git diff        git diff HEAD
      （工作区 vs      （工作区 vs 仓库）
        暂存区）        ↑ 离上次提交总共改了多少
           /            \
        暂存区 ──── git diff --staged ───→ 仓库
                    （暂存区 vs 仓库）
                     ↑ 这次 commit 会带上什么

   再加一个维度：git diff <A> <B> = 比两个提交（两份归档快照）
   记法：**谁没出现在命令里，谁就是左边的基准。**
         git diff          → 基准是暂存区，看工作区
         git diff --staged → 基准是仓库，看暂存区
         git diff HEAD     → 基准是仓库，看工作区（跨过暂存区）
```

### 图 4：历史是一张有向图，`A..B` / `A...B` 是集合运算

场景 6 的现场（`loglab`，真实哈希）：`main` 和 `feature` 在 `A2` 之后**真的分叉过**。

```
   A1 ──→ A2 ──→ A3                        ← main
   9bd5835 e2b2a73 9e7bd7a
            │
             ──→ F1 ──→ F2                 ← feature
                   0db2a6b 5398305

   共同祖先（分叉点）：git merge-base main feature → e2b2a73（就是 A2）

   A..B  「从 B 出发能走到、但 A 走不到的提交」→ 左端点自己不算
        e2b2a73..main  = A3          main..feature = F2、F1        feature..main = A3
        想把 A2 也算进去 → 写它的父：e2b2a73^..main = A3、A2

   A...B 「两边的对称差」= 各自独有的混在一起（共同祖先两边都不包含）
        main...feature = A3、F2、F1   （加 --left-right 才分得清 <左 / >右）
```

★ **`log` 的点号和 `diff` 的点号意思不一样**，这是本章最容易记错的一处：

| 写法 | `git log` 里的意思 | `git diff` 里的意思 |
|---|---|---|
| `A..B` | B 独有的**提交列表** | 直接比 A、B 两端快照（等价 `git diff A B`） |
| `A...B` | 两边各自独有的提交混在一起 | 从**共同祖先**开始比，只看 B 这条线真正改了什么 |

### 全景表：想干什么 → 用哪条 → 动的是哪个区

| 我想干什么 | 命令 | 动工作区 | 动暂存区 | 动仓库 | 本章 |
|---|---|:--:|:--:|:--:|:--:|
| 让一个普通目录变成仓库 | `git init -b main` | ✘ | ✘ | 建 `.git` | 1 |
| 已有仓库，拉一个工作副本 | `git clone <地址> [目录]` | 写满 | 建 | 建 | 1 |
| 看三个区哪里不一致 | `git status -sb` | 只读 | — | — | 2 |
| 连被忽略的也列出来 | `git status -s --ignored` | 只读 | — | — | 2 |
| 把这一处放进下次提交 | `git add <文件>` | ✘ | ✔ | 写 blob | 3 |
| 已跟踪文件的改动+删除全进 | `git add -u` | ✘ | ✔ | 写 blob | 3 |
| 当前目录所有变化（含新文件） | `git add .` | ✘ | ✔ | 写 blob | 3 |
| 整个工作树所有变化全进 | `git add -A` | ✘ | ✔ | 写 blob | 3 |
| 一个文件只挑几块 | `git add -p` | ✘ | ✔（部分） | 写 blob | 3 |
| 暂存区整个退回，改动留着 | `git reset` | ✘ | ✔ | ✘ | 3 |
| 装订成永久档案 | `git commit -m "..."` | ✘ | 清空 | ✔ | 4 |
| 顺手把已跟踪的改动一起提交 | `git commit -am "..."` | ✘ | 自动 add | ✔ | 4 |
| 换掉最后一条提交 | `git commit --amend` | ✘ | ✔ | ✔（hash 变） | 4 |
| 我 add 之后又改了什么 | `git diff` | 只读 | — | — | 5 |
| 这次 commit 会带上什么 | `git diff --staged` | 只读 | — | — | 5 |
| 离上次提交总共改了多少 | `git diff HEAD` | 只读 | — | — | 5 |
| 取某个版本的某个文件 | `git show <提交>:<路径>` | 只读 | — | — | 5 |
| 看历史长什么样 | `git log --oneline --graph --decorate --all` | 只读 | — | — | 6 |
| 领先/落后几条 | `git rev-list --count A..B` | 只读 | — | — | 6 |
| 分叉点是谁 | `git merge-base A B` | 只读 | — | — | 6 |
| 停止跟踪但本地文件留着 | `git rm --cached <文件>` | ✘ | ✔ | ✔（下次提交删） | 7 |
| 删文件，提交里也删 | `git rm <文件>` | ✔ 删掉 | ✔ | ✔ | 7 |
| 改名且让 git 认成 rename | `git mv a b` | ✔ 改名 | ✔ | ✔ | 2 |
| 问谁把这个路径拦住了 | `git check-ignore -v <路径>` | 只读 | — | — | 8 |
| 只给本仓库配别名/开关 | `git config --local ...` | ✘ | ✘ | 写 `.git/config` | 9 |

三条规律，看完这张表就该记住：

1. **只有 `add` 和 `commit` 在写数据**，`status`/`diff`/`log`/`show`/`check-ignore` 全是只读的"看一眼"。
   本章没有一条危险命令 —— 危险的那几条（`reset --hard`、`clean -fd`、`push -f`）全在第 03、05 章。
2. **暂存区是"下次提交的草稿"**，所有 `add` 家族的差别只有一个问题：*这次往草稿里放多少东西*。
3. **`.gitignore` 只对"清单里没有的文件"生效**。已经在仓库里的文件写一百条 ignore 也没用 ——
   必须先 `git rm --cached`（场景 7，本章事故排行榜第一名）。

---

## 场景逐个实战

> 下面所有输出都是 `bash demo.sh` 真跑出来的（沙箱 `/tmp/git02-xY0rbf`，跑完自动删；
> 那串随机后缀每次都不同，你看到的是你自己的，不影响其他任何一行）。
> 时间戳、提交身份、文件内容全部固定，所以你跑出来的**哈希和这里一模一样**（本章连跑两次逐字节相同，只差沙箱名）。
> 凡是需要身份的 `commit`，命令行里都带 `-c user.name="学习者" -c user.email="learn@example.com"`，
> 为的是**不碰你的全局配置**；`add`/`status`/`log` 这些不产生提交的命令就不带。
> git 说英文是因为脚本把 `LC_ALL` 固定成 `C.UTF-8`；你手敲时大概率是中文，见场景 2 的中英对照。
> 标了「加餐」的小块是本文**额外在临时沙箱里跑**的命令（沙箱名写在块里），输出同样原文照贴；
> 个别地方为了省篇幅省略了与上文完全重复的几行，都会用一句话注明。

### 场景 1：建仓库的两条路 —— init 与（本地）clone

**想干什么**：手上有个目录想变成仓库（`init`），或者已有仓库想再拿一份能干活的地方（`clone`）。
你想知道两条路建出来的东西**差在哪** —— 这个差别决定了你以后 `status` 能不能告诉你和远程的差距。

**敲哪些命令**

```bash
git init -b main newrepo
git clone origin.git work        # 这里的 origin 是本地 bare 仓库冒充的，全程不联网
git status ; git remote -v ; cat .git/config ; git branch -a
```

**真实输出**

```text
$ git init -b main newrepo
Initialized empty Git repository in /tmp/git02-xY0rbf/newrepo/.git/

$ ls -a newrepo
.
..
.git
```

`init` 只造一个 `.git`，工作区一个文件都没动。作为对照，"远程"那边先有了内容：
沙箱里 `seed` 仓库提交出 `b2b6611 origin 的第一个提交`，再用 `git clone --bare seed origin.git` 复制成 bare 仓库。

**bare 仓库整个目录就是别人的 `.git`**：`ls origin.git` 打出来的是 `HEAD`、`config`、`description`、`hooks`、`info`、`objects`、`packed-refs`、`refs` —— 没有工作区、没有 `index`，也不该在里面写代码。
它专门用来"被别人 push/pull"；`packed-refs` 是 `clone --bare` 顺手打包引用时生成的，不影响任何语义。

现在从它 clone 一份工作副本：

```text
$ git clone origin.git work
Cloning into 'work'...
done.

$ ls -a
.
..
.git
app.txt

$ git status
On branch main
Your branch is up to date with 'origin/main'.

nothing to commit, working tree clean

$ git remote -v
origin	/tmp/git02-xY0rbf/origin.git (fetch)
origin	/tmp/git02-xY0rbf/origin.git (push)
```

`clone` = `init` + 记住远程地址 + 捞对象 + 建分支 + **把工作区文件铺出来**。
两处铁证：`ls -a` 里 `app.txt` 已经在（`init` 出来的仓库不会有），
`status` 多了一句 `Your branch is up to date with 'origin/main'.`（init 出来的仓库永远没这句，因为它不知道谁是远程）。

`.git/config` 里的差别最直白：

```text
$ cat .git/config
[core]
	repositoryformatversion = 0
	filemode = true
	bare = false
	logallrefupdates = true
[remote "origin"]
	url = /tmp/git02-xY0rbf/origin.git
	fetch = +refs/heads/*:refs/remotes/origin/*
[branch "main"]
	remote = origin
	merge = refs/heads/main
```

对比 01 章 `git init` 之后的 `.git/config`（只有 `[core]` 四行）：多出来的 `[remote "origin"]` 管"远程叫什么、在哪、
抓回来映射成 `refs/remotes/origin/*` 下的本地引用"；`[branch "main"]` 管"本地 `main` 跟踪谁"。有了第二段，`status` 才知道
要不要写那句 up to date，`git push`/`git pull` 才能不带参数。

```text
$ git log --oneline --decorate --all
b2b6611 (HEAD -> main, origin/main, origin/HEAD) origin 的第一个提交
```

`branch -a` 里 `* main` 之外全是 `remotes/origin/…`（`remotes/origin/HEAD -> origin/main`、`remotes/origin/main`）。
`origin/main` **不是远程机器上的东西**，它是你本地 `refs/remotes/origin/main` 这个文本文件，
内容是"上次和远程通讯时，远程 main 在哪个哈希"（01 章"分支就是文本文件"的远程版）。
`origin/HEAD` 记的是"远程默认分支是哪个"。`*` 标的是 HEAD 当前所在分支；`remotes/` 开头的全是只读快照，
别 checkout 上去干活（会进 detached HEAD，01 章场景 6）。

**加餐：那句 "up to date" 会怎么变脏**（demo 场景 1d：沙箱里再开一个 bare 仓库 `update.git` 冒充 origin，
本地工作副本 `w`、"别人"的工作副本 `w2`，全程不联网）

```text
$ git remote add origin /tmp/git02-jaX8xi/update.git

$ git push -u origin main   # 远程那边还没有 main，所以是 [new branch]
To /tmp/git02-jaX8xi/update.git
 * [new branch]      main -> main
branch 'main' set up to track 'origin/main'.
[退出码 0]

$ git status -sb
## main...origin/main
```

刚推完，本地 `main` 和快照 `origin/main` 指同一条，`##` 后面什么都不带。
本地再走一条提交（`echo '+本地第二条' >> note.txt` 然后 `commit -qam "W2 本地又走了一条"`）就变成：

```text
$ git status -sb
## main...origin/main [ahead 1]
```

再换成"别人先推了一步"：`git clone -q update.git w2`，在 `w2` 里提交 `O1 别人先推了一步`，
`git push -q origin main` 推上去（`-q` 静默，推成功时一个字都不打）。回到 `w` 看：

```text
$ git status -sb   # w 还没 fetch
## main...origin/main [ahead 1]

$ git fetch -q origin

$ git status -sb   # fetch 之后才看得见落后
## main...origin/main [ahead 1, behind 1]
```

两块之间**只隔了一条 `git fetch`**。`origin/main` 只是本地存的快照，不 fetch 就永远是旧的，而 Git 不会替你自动 fetch ——
"我本地明明干净怎么 push 被拒了"的根因基本都在这（第 07 章细讲）。Git 是分布式的，本地永远只存"我最后一次看到远程的样子"。

**常见翻车**

- 在 bare 仓库里 `git status` → 一脸茫然。bare **没有工作区**，别在 `xxx.git` 里写代码。
- 以为 `origin/main` 是远程的实时状态 → 它是缓存（见加餐）。
- 想改远程地址用 `git remote set-url origin <新地址>`，别手删 `.git/config`。

### 场景 2：`git status -s` 短格式，每个字母都要认得

**想干什么**：`git status` 输出两屏，你要 2 秒内知道"哪些进这次提交、哪些没进、哪些是野文件"。

**敲哪些命令**（一次把 9 种状态全造出来，全落在同一条输出里）

```bash
echo '+1' >> both.txt && git add both.txt && echo '+2' >> both.txt   # MM 暂存过又改
git rm del_staged.txt                                                # D  清单和工作区一起删
rm del_worktree.txt                                                  #  D 只删文件，git 不知情
echo '+工作区' >> modified.txt                                        #  M
echo '新文件' > fresh.txt && git add fresh.txt && rm fresh.txt        # AD 进了清单又被删
echo '+暂存' >> stagedonly.txt && git add stagedonly.txt              # M
git mv ren.txt ren_new.txt                                           # R
echo '没人管' > brandnew.txt                                          # ??
git status -s
```

**为什么是这几条**：两个格子来自**两次不同的比较**（图 2）。要证明这件事，
必须同时造出"只有第 1 格有字母"（`D ` / `M `）、"只有第 2 格有字母"（` D` / ` M`）和"两格都有"（`MM` / `AD`）。

**真实输出**（基线是 7 个文件一次提交，提交完 `status -s` 什么都不输出）

```text
$ git status -s
MM both.txt
D  del_staged.txt
 D del_worktree.txt
AD fresh.txt
 M modified.txt
R  ren.txt -> ren_new.txt
M  stagedonly.txt
?? brandnew.txt
```

**逐行解读**

| 输出 | 现场怎么造的 | 下次提交会带上什么 |
|---|---|---|
| `MM both.txt` | 改 → add → **又改** | 只带第一次 add 的内容（第 2 格那些没进） |
| `D  del_staged.txt` | `git rm`（顺手删文件并暂存） | 删掉它 |
| ` D del_worktree.txt` | 裸 `rm`，git 不知情 | **什么都不带**。你以为删了，历史里还在 |
| `AD fresh.txt` | `add` 成 A，随即 `rm` | 提交一个"新增"，可磁盘上没这文件（自相矛盾，必须处理） |
| ` M modified.txt` | 只改文件没 add | 不带 |
| `R  ren.txt -> ren_new.txt` | `git mv` | 改名（见场景 3 的 `(100%)`） |
| `M  stagedonly.txt` | 改完 add，之后没再动 | 带上 |
| `?? brandnew.txt` | 新建，从没 add 过 | 不带，而且 `restore`/`reset` 都管不了它 |

`AD` 值得多说一句：`A`（第 1 列）= 清单里它被标成新增，`D`（第 2 列）= 工作区里它没了。
git 不拦你这种自相矛盾，但带着 `AD` 提交 = 提交一个空新增，然后继续有个鬼文件。
要么 `git add fresh.txt` 承认删除，要么把文件写回来。

同一现场的长格式：

```text
$ git status
On branch main
Changes to be committed:
  (use "git restore --staged <file>..." to unstage)
	modified:   both.txt
	deleted:    del_staged.txt
	new file:   fresh.txt
	renamed:    ren.txt -> ren_new.txt
	modified:   stagedonly.txt

Changes not staged for commit:
  (use "git add/rm <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)
	modified:   both.txt
	deleted:    del_worktree.txt
	deleted:    fresh.txt
	modified:   modified.txt

Untracked files:
  (use "git add <file>..." to include in what will be committed)
	brandnew.txt
```

两个要点：① `both.txt` **在两个栏目里各出现一次**，这正是短格式 `MM` 的长格式说法（一次比较出一行）。
② 括号里 `(use "git restore --staged <file>..." to unstage)` 不是客套，是 git 把**下一步该敲的命令**写给你了 ——
新手最该练的就是照着它敲。

再加一条常被误解的：`git status -s --ignored` 会在最后多出一行 `!! junk.tmp` —— `!!` **只在这条命令里出现**
（`junk.tmp` 被基线里的 `*.tmp` 拦住）。平时它彻底隐身；怀疑某文件没被拦住 → `--ignored` 或 `git check-ignore -v <路径>`（场景 8）。

**中英对照**（本章输出是英文；中文这版是在同一现场的沙箱里用 `LC_ALL=zh_CN.UTF-8` 真跑出来的）

| 英文（本章输出） | 中文（你机器上大概率看到） | 短格式 |
|---|---|---|
| `On branch main` | `位于分支 main` | `## main` |
| `Changes to be committed` | `要提交的变更：` | 第 1 列字母 |
| `Changes not staged for commit` | `尚未暂存以备提交的变更：` | 第 2 列字母 |
| `Untracked files` | `未跟踪的文件:` | `??` |
| `modified:` | `修改：` | `M` |
| `deleted:` | `删除：` | `D` |
| `new file:` | `新文件：` | `A` |
| `renamed:` | `重命名：` | `R` |

字母全世界通用 —— **`??` 不会变成中文，所以练读短格式最划算**。

**常见翻车**

- 把两格当一格读：` M` 和 `M ` 是**两件完全相反的事**（还没 add / 已进草稿）。贴给别人时别删空格。
- 看到 ` D` 以为"历史里也没了" → 历史里还在，你得 `git add`/`git rm` 才算真删。
- 想 `restore` 掉 `??` 的文件 → restore/reset 管不着未跟踪文件，那是 `git clean` 的地盘（第 03 章）。

### 场景 3：add 家族 —— add / add -A / add -u / add -p

**想干什么**：把"这次要提交的东西"精确放进草稿箱。最难的一种：**同一文件里既有真修复又有随手加的调试打印**，只想提交前者。

**敲哪些命令**

```bash
seq 1 14 | sed 's/^/line /' > app.c && git add app.c && git commit -m "app.c 14 行"
sed -i '2s|.*|line 2 重要修复|;12s|.*|line 12 临时调试|' app.c
git diff -- app.c                          # 现在是两块
printf 'y\nn\n' | git add -p -- app.c      # 逐块确认（脚本喂答案；手敲是直接按键）
git diff --cached -- app.c                 # 暂存区只有第一块
git diff -- app.c                          # 工作区另一块还在
git commit -m "只提交第 2 行的重要修复"
```

**真实输出** —— 先给场景 2 的脏现场结账，这次提交把上面每个字母都兑现了：

```text
$ git add -A

$ git -c user.name="学习者" -c user.email="learn@example.com" commit -m "收拾场景 2 的现场"
[main ad24bb5] 收拾场景 2 的现场
 7 files changed, 5 insertions(+), 2 deletions(-)
 create mode 100644 brandnew.txt
 delete mode 100644 del_staged.txt
 delete mode 100644 del_worktree.txt
 rename ren.txt => ren_new.txt (100%)
```

`rename ren.txt => ren_new.txt (100%)` 是场景 2 那个 `R ` 的后续：**git 存的其实是"删一个 + 加一个"，
`(100%)` 是它按内容相似度事后推断出来的**。`git mv` 的全部意义就是"帮你把两步一次做完"，改名没有魔法。

现在改两处（第 2 行是真修复，第 12 行是调试打印）：

```text
$ git diff -- app.c
diff --git a/app.c b/app.c
index d4f93aa..8991395 100644
--- a/app.c
+++ b/app.c
@@ -1,5 +1,5 @@
 line 1
-line 2
+line 2 重要修复
 line 3
 line 4
 line 5
@@ -9,6 +9,6 @@ line 8
 line 9
 line 10
 line 11
-line 12
+line 12 临时调试
 line 13
 line 14
```

`add -p` 会先把上面这份 diff 原样再打一遍，然后**逐块问你**（`(1/2)` = 第 1 块，共 2 块；方括号是可用的键）：

```text
(1/2) Stage this hunk [y,n,q,a,d,k,K,j,J,g,/,e,p,P,?]? @@ -9,6 +9,6 @@ line 8
 line 9
 line 10
 line 11
-line 12
+line 12 临时调试
 line 13
 line 14
(2/2) Stage this hunk [y,n,q,a,d,K,J,g,/,e,p,P,?]? 
```

回答 `y`（暂存这块）、`n`（这块先不要），交互结束。复核：

```text
$ git diff --cached -- app.c
diff --git a/app.c b/app.c
index d4f93aa..d9810e1 100644
--- a/app.c
+++ b/app.c
@@ -1,5 +1,5 @@
 line 1
-line 2
+line 2 重要修复
 line 3
 line 4
 line 5

$ git status -s -- app.c
MM app.c
```

（`git diff -- app.c` 在这时打印的第二块和上面 `(1/2)` 之后那 6 行一字不差，就不重复贴了。）

**★ 三处 `index` 行的哈希，把整件事钉死了**

（下面三行是把上面三屏 `git diff` 的 `index` 行抄在一起对齐写的，不是又一次运行结果）

    git diff（add -p 之前）    index d4f93aa..8991395
    git diff --cached（之后）   index d4f93aa..d9810e1
    git diff（之后）           index d9810e1..8991395

`d4f93aa` = 仓库那版，`d9810e1` = 暂存区那版（只第 2 行变了），`8991395` = 工作区那版（两行都变）。
demo 里紧接着把这两点各问了一次：

```text
$ git cat-file -t d9810e1   # git diff --cached 的第二列：add -p 真写进去的
blob
[退出码 0]

$ git cat-file -t 8991395   # git diff 的第二列：工作区那版，diff 只是现场算给你看
fatal: Not a valid object name 8991395
[退出码 128]

$ git hash-object --stdin < app.c
8991395a9957c0318a74e32c2e6e97f41377f259
```

`d9810e1` 是真实对象（`add -p` 写进去的，所以 `cat-file -t` 说 `blob`）；而 `8991395` **此刻根本不在 objects 里** ——
报错的退出码是 128。但它的值没错：把当前工作区内容喂给 `git hash-object --stdin`（不带 `-w` 就只算哈希、不写盘），
得到的正是 `8991395a9957c0318a74e32c2e6e97f41377f259`，前 7 位就是 `8991395`。
**`git diff` 现场算哈希给你看，并不写盘；只有 `add` 才真的复制内容** ——
01 章场景 3 那条结论的又一次实证。

于是这次提交只带第 2 行：

```text
$ git -c user.name="学习者" -c user.email="learn@example.com" commit -m "只提交第 2 行的重要修复"
[main 9db3baa] 只提交第 2 行的重要修复
 1 file changed, 1 insertion(+), 1 deletion(-)

$ git status -s
 M app.c
```

`1 file changed, 1 insertion(+), 1 deletion(-)` —— 精确一块，调试那行留在工作区（` M` 一直挂着，
等真改完再处理，或者交给第 06 章的 stash）。

**加餐：`add -p` 的键不止 y/n**（demo 场景 3 的 `patchlab` 仓库：九行文件里改第 2、4、6 行）

先问一次 `?`，git 会把**这一轮真能按的键**连含义一起打出来 —— 这就是那张表的出处，不用背：

```text
$ printf "?\nq\n" | git add -p -- multi.txt
diff --git a/multi.txt b/multi.txt
index 1cfd117..27d644a 100644
--- a/multi.txt
+++ b/multi.txt
@@ -1,9 +1,9 @@
 L1
-L2
+L2 真修复
 L3
-L4
+L4 临时调试
 L5
-L6
+L6 也想留下
 L7
 L8
 L9
(1/1) Stage this hunk [y,n,q,a,d,s,e,p,P,?]? y - stage this hunk
n - do not stage this hunk
q - quit; do not stage this hunk or any of the remaining ones
a - stage this hunk and all later hunks in the file
d - do not stage this hunk or any of the later hunks in the file
s - split the current hunk into smaller hunks
e - manually edit the current hunk
p - print the current hunk
P - print the current hunk using the pager
? - print help
(1/1) Stage this hunk [y,n,q,a,d,s,e,p,P,?]? 

$ git status -s -- multi.txt
 M multi.txt
```

按 `q` 之后 `status -s` 只有一条 ` M`（第 2 格）—— 改动还在工作区，`q` 没替你暂存任何东西，
这就是"`q` 和 `n` 都不销毁内容"的实证。

三处改动挨得近，git 把它们并成了**一个** hunk（上面问句是 `(1/1)`，方括号里有 `s`）。
这一次喂 `s y n y`：`s` 拆块，再逐块决定。

```text
$ printf "s\ny\nn\ny\n" | git add -p -- multi.txt
diff --git a/multi.txt b/multi.txt
index 1cfd117..27d644a 100644
--- a/multi.txt
+++ b/multi.txt
@@ -1,9 +1,9 @@
 L1
-L2
+L2 真修复
 L3
-L4
+L4 临时调试
 L5
-L6
+L6 也想留下
 L7
 L8
 L9
(1/1) Stage this hunk [y,n,q,a,d,s,e,p,P,?]? Split into 3 hunks.
@@ -1,3 +1,3 @@
 L1
-L2
+L2 真修复
 L3
(1/3) Stage this hunk [y,n,q,a,d,k,K,j,J,g,/,e,p,P,?]? @@ -3,3 +3,3 @@
 L3
-L4
+L4 临时调试
 L5
(2/3) Stage this hunk [y,n,q,a,d,k,K,j,J,g,/,e,p,P,?]? @@ -5,5 +5,5 @@
 L5
-L6
+L6 也想留下
 L7
 L8
 L9
(3/3) Stage this hunk [y,n,q,a,d,K,J,g,/,e,p,P,?]? 
```

`Split into 3 hunks.` 之后 `(1/1)` 变成 `(1/3)`、`(2/3)`、`(3/3)`，每行问句后面跟着的就是那一块本身。
注意问句里的字母**每块都不一样**：拆完之后 `s` 没了、多出一批跳转键（`k`/`K`/`j`/`J`/`g`/`/`），
最后一块又少掉几个。**那些字母是 git 按"这块还能往哪跳、还能不能拆"现算出来的**，
所以别背问句，看不清就按 `?`。结果（`git add -p` 只动清单，所以复核还是那两条）：

```text
$ git diff --cached -- multi.txt
diff --git a/multi.txt b/multi.txt
index 1cfd117..259876d 100644
--- a/multi.txt
+++ b/multi.txt
@@ -1,9 +1,9 @@
 L1
-L2
+L2 真修复
 L3
 L4
 L5
-L6
+L6 也想留下
 L7
 L8
 L9

$ git status -s -- multi.txt
MM multi.txt
```

`L2 真修复` 和 `L6 也想留下` 进了草稿（暂存区那版是 `259876d`），
`L4 临时调试` 那一行在暂存区里变回了原来的 `L4`，留在工作区 → `status -s` 报 `MM multi.txt`。

手敲时最该记的键：`y` 暂存 / `n` 跳过 / `s` 拆块 / `e` 手工编辑这块 / `q` 不问了直接收工 / `a` 这块及其后全部暂存 /
`d` 这块及其后全部跳过 / `?` 列全部键。`q` 和 `n` 分别是"剩下的不看了"和"剩下的逐块说不"，中途按 `q` 不会丢任何东西。

**add 家族到底差在哪**（同一现场，逐条实测）

```text
$ echo '未跟踪的新文件' > untracked.txt
$ echo '+改动' >> app.c
$ rm -f stagedonly.txt

$ git status -s
 M app.c
 D stagedonly.txt
?? untracked.txt

$ git add -u

$ git status -s
M  app.c
D  stagedonly.txt
?? untracked.txt
```

`add -u`（update）只动**已跟踪**的：改动能进、删除能进，`?? untracked.txt` 它始终不收。
接着 `git add .` → 三行变成 `M ` / `D ` / `A `（新文件进来了）；`git reset -q` → 又退回
` M` / ` D` / `??`。`git reset -q`（= `--mixed`，不带 `--hard`）把草稿箱整个倒回工作区，
**一个字节的内容都没丢** —— 这是"我 add 多了"的全局解药（单个文件用 `git restore --staged <文件>`，第 03 章场景 ②）。
最后 `git add -A` + 提交收场：

```text
$ git -c user.name="学习者" -c user.email="learn@example.com" commit -m "把现场提交干净：untracked.txt 进仓库，stagedonly.txt 删掉"
[main 3633d9b] 把现场提交干净：untracked.txt 进仓库，stagedonly.txt 删掉
 3 files changed, 3 insertions(+), 3 deletions(-)
 delete mode 100644 stagedonly.txt
 create mode 100644 untracked.txt
```

`add -A` = 整个工作树的一切变化（改、删、新增，不限当前目录）；`add .` 带路径范围，在子目录敲就不影响上层。

| 命令 | 已跟踪的改动 | 已跟踪的删除 | 未跟踪的新文件 | 作用范围 |
|---|:--:|:--:|:--:|---|
| `git add <文件>` | ✔ | ✔ | ✔（首次 add = 开始跟踪） | 你点名的路径 |
| `git add -p` | ✔（挑块） | — | ✘ 不在 -p 里出现 | 指定文件 |
| `git add -u` | ✔ | ✔ | ✘ **不收** | 默认全仓库 |
| `git add .` | ✔ | ✔ | ✔ | **当前目录及子目录** |
| `git add -A` | ✔ | ✔ | ✔ | 整个工作树 |

日常推荐：**先 `git status -s`，再 `git add <你确认过的文件>`**。`add -A` 只在"这一堆确实都该一起提交"时用，
用完必须再看一眼 `git diff --staged`。

**常见翻车**

- `add` 之后又改了文件 → 提交的是**上次 add 那一刻**的内容。查法：`status -s` 出现 `MM`，或 `git diff` 还有输出。补救：再 add。
- 用 `git add -A` 把 `node_modules`/`.env` 一起收进仓库 → 后果见场景 7，本章最贵的坑。
- 想取消 add：`git restore --staged <文件>`（单个）或 `git reset`（整个暂存区）。
- `add -p` 手敲时别用管道喂答案（本文这么干纯粹为了让 demo 可复现），交互式按键更安全，因为你能看见每一块。

---

### 场景 4：commit 家族 —— `-m` / `-am` / `--amend`

**想干什么**：把清单装订成提交；以及刚装订完发现说明写错、或者漏了个文件，想就地换掉最后一条。

**敲哪些命令**

```bash
git commit -m "说明"            # 只装订清单，不看工作区一眼
git commit -am "说明"           # 先把「已跟踪且有改动」的文件补进清单，再装订
git commit --amend -m "新说明"  # 换掉最后一条：内容和说明都能改，哈希一定变
```

**真实输出** —— `-am` 的坑先用一次提交现场演示：

```text
$ echo '我是新文件' > brand_new.txt

$ echo '+改动' >> app.c

$ git status -s
 M app.c
?? brand_new.txt

$ git -c user.name="学习者" -c user.email="learn@example.com" commit -am "改了 app.c（本来还想带上 brand_new.txt，带不上）"
[main c4330cf] 改了 app.c（本来还想带上 brand_new.txt，带不上）
 1 file changed, 1 insertion(+)

$ git status -s
?? brand_new.txt
```

**逐行解读**
- 提交前 `status -s` 是两行：` M app.c`（已跟踪、有改动）和 `?? brand_new.txt`（从没进过清单）。
- `-a` 的作用域**只有已跟踪文件**。`brand_new.txt` 是 `??`：Git 的清单里没有它，`-a` 不知道要带它。
- 所以输出只有一句 `1 file changed, 1 insertion(+)` —— 数字对得上，是你没看住，不是 git 出错。
- 提交完再看一眼 `status -s`，那个 `??` 还杵在那儿。**这就是"我以为提交了"的标准现场**，
  修法只有一条：新文件先 `git add` 一次，之后它才归 `-am` 管。
- add 过之后普通提交就带上它了：`[main d491338] brand_new.txt 这次进来了`、` 1 file changed, 1 insertion(+)`、
  ` create mode 100644 brand_new.txt`，随后 `status -s` 完全空输出。`create mode` 是"这次多了个新文件"的官方
  说法 —— 01 章讲过，Git 不存"新增/删除"这种动作，它是事后从清单差异推出来的。

**`--amend`：把最后一条换掉**

```text
$ echo '刚才忘了写这句' >> brand_new.txt

$ git add brand_new.txt

$ git -c user.name="学习者" -c user.email="learn@example.com" commit --amend -m "brand_new.txt（补过内容，也改了说明）"
[main af87628] brand_new.txt（补过内容，也改了说明）
 Date: Thu Jan 1 10:00:00 2026 +0800
 1 file changed, 2 insertions(+)
 create mode 100644 brand_new.txt
```

- 想补的内容得**先 add 再 amend** —— `--amend` 同样只装订清单。这里 `1 file changed, 2 insertions(+)`
  比原来的 `1 insertion(+)` 多了一行，说明新内容真的进了替换后的那条。
- 只有 amend 会多打一行 `Date:`：那是**原作者时间**（沿用原提交）。而"提交者时间"被重写成现在，
  哈希因此一定变。本章把 `GIT_COMMITTER_DATE` 也钉死了所以看不出来，你平时 amend 完
  `git log --format='%h %ad %cd'` 就能看见"作者时间旧、提交时间新"这对矛盾 —— 那是 amend 的铁证。

```text
--- 提交数没变多，最后一条被换掉了 —— 但哈希变了

$ git log --oneline -3
af87628 brand_new.txt（补过内容，也改了说明）
c4330cf 改了 app.c（本来还想带上 brand_new.txt，带不上）
3633d9b 把现场提交干净：untracked.txt 进仓库，stagedonly.txt 删掉

--- 被换掉的那个提交没消失，在 reflog 里躺着

$ git reflog -3
af87628 HEAD@{0}: commit (amend): brand_new.txt（补过内容，也改了说明）
d491338 HEAD@{1}: commit: brand_new.txt 这次进来了
c4330cf HEAD@{2}: commit: 改了 app.c（本来还想带上 brand_new.txt，带不上）
```

`d491338` 从 `log` 里彻底消失了，但 `reflog` 那行 `commit (amend)` 还留着它。
这就是 03 章"amend 完还能救"的底层原因：**对象没被删，只是没名字指着它了**。

**加餐：没提交成功时 git 到底怎么说**（demo 场景 4 的 `nomit` 仓库。身份是 `git config --local`
写进这个沙箱仓库自己的 —— **这一节演的是"身份有了但提交没成"**；没有身份时 git 死得更早，
场景 9 那条 `Author identity unknown` 才是那种情况）

```text
$ echo 'a 的内容' > a.txt

$ git add a.txt

$ GIT_EDITOR=true git commit   # 进了编辑器，一个字没写就退出来
Aborting commit due to empty commit message.
[退出码 1]

$ git log --oneline
fatal: your current branch 'main' does not have any commits yet
[退出码 128]

$ git status -sb
## No commits yet on main
A  a.txt
```

`a.txt` 已经 `add` 过，所以 `status -sb` 给它 `A `（第 1 格）；空说明只让 commit 没改成，清单一个字没动。
`git log` 那条 `fatal` 值得单独记：**仓库里一条提交都没有时，`log` 直接罢工**，退出码 128。

接着演第二种脏：先把 `a.txt` 真提交一次（`git commit -qm "a.txt 第一次进仓库"`），
再改 `a.txt` 但**不** `add`，另外扔一个野文件 `b.txt` 进去：

```text
$ git status -sb
## main
 M a.txt
?? b.txt

$ git commit -m "改了 a.txt"   # 改了但没 add，而且没带 -a
On branch main
Changes not staged for commit:
  (use "git add <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)
	modified:   a.txt

Untracked files:
  (use "git add <file>..." to include in what will be committed)
	b.txt

no changes added to commit (use "git add" and/or "git commit -a")
[退出码 1]
```

同一条 commit 在补了 `git add a.txt` 之后就成功了：

```text
$ git add a.txt

$ git commit -m "改了 a.txt"
[main ed2907e] 改了 a.txt
 1 file changed, 1 insertion(+)

$ git status -sb
## main
?? b.txt
```

三句话记住三种失败：`Aborting commit due to empty commit message.`（说明是空的）、
`no changes added to commit`（清单是空的，改动都在工作区）、
`does not have any commits yet`（仓库里一条提交都没有，`log` 没法工作）。
**判据永远是：命令跑完立刻看退出码或再看一次 `status -s`**，别靠"好像没报错"。

**常见翻车**
- 用 `-am` 期望带上新文件 → 见上，新文件必须 `add` 一次。
- `--amend` 之后 `git push` 被拒 → 你换掉了历史，远程不认。已推送的提交**不要** amend（03 章细讲怎么处理）。
- amend 时忘了 `add` → 结果只改了说明，内容还是老那份；`1 file changed` 那行会告诉你差在哪。
- 想改的不是最后一条而是上上条 → `--amend` 改不了它，那是 03 章的 `rebase -i`。

---

### 场景 5：diff / show —— 三个区两两对比，一条都不能记错

**想干什么**：按回车之前把"这次提交会带上什么"看清楚；以及直接把某个历史版本的文件内容捞出来。

**敲哪些命令**（对应图 3 三角形的三条边）

```bash
git diff                # 工作区 vs 暂存区：我 add 之后又改了什么
git diff --staged       # 暂存区 vs 仓库：这次 commit 会带上什么（--cached 完全同义）
git diff HEAD           # 工作区 vs 仓库：离上次提交总共差了多少
git diff A B            # 任意两个版本之间
git show --stat HEAD    # 一个提交的元信息 + 差异
git show HEAD~1:路径     # 直接取那个版本的文件原文
```

**真实输出** —— 现场是 `note.md` 三行，提交一次后：把 L2 改了并 `add`，再把 L3 改了**不** `add`。

（先把三行进仓库：`printf` 三行 → `git add note.md` → 提交，输出是 `[main 2407de8] note.md 进仓库`、
` 1 file changed, 3 insertions(+)`、` create mode 100644 note.md`。然后制造本次现场：
把 L2 改了并 `add`，再把 L3 改了**不** `add`。）

```text

$ printf 'L1\nL2改并暂存\nL3\n' > note.md

$ git add note.md

$ printf 'L1\nL2改并暂存\nL3也改了\n' > note.md

$ git status -s -- note.md
MM note.md
```

`MM` = 两个格子都在改（暂存区比仓库多一处 L2，工作区比暂存区多一处 L3）。三条 diff 各自的输出：

```text
$ git diff -- note.md
diff --git a/note.md b/note.md
index f9adb40..972f82f 100644
--- a/note.md
+++ b/note.md
@@ -1,3 +1,3 @@
 L1
 L2改并暂存
-L3
+L3也改了

$ git diff --staged -- note.md
diff --git a/note.md b/note.md
index 516fad4..f9adb40 100644
--- a/note.md
+++ b/note.md
@@ -1,3 +1,3 @@
 L1
-L2
+L2改并暂存
 L3

$ git diff HEAD -- note.md
diff --git a/note.md b/note.md
index 516fad4..972f82f 100644
--- a/note.md
+++ b/note.md
@@ -1,3 +1,3 @@
 L1
-L2
-L3
+L2改并暂存
+L3也改了
```

**逐行解读**
- **`index X..Y` 这行是三条命令的身份证明**：`516fad4` = 仓库那版、`f9adb40` = 暂存区那版、
  `972f82f` = 工作区那版。三条 diff 用的是同样三个哈希，只是两两配对不同 —— 图 3 三条边一次跑通。
- `@@ -1,3 +1,3 @@` 读法：旧文件从第 1 行起 3 行，新文件从第 1 行起 3 行。**上下文行两边都算进这个数**，
  所以 `-L2 -L3 +L2改并暂存 +L3也改了` 是 4 行带符号、但计数还是 3。
- `git diff HEAD` 不等于"前两条拼起来"：如果暂存之后又把那行改回原样，
  `--staged` 和 `diff` 各看得见一次改动，而 `diff HEAD` 完全看不见（两端一模一样）。
  所以提交前这三条**都要看**，`--staged` 是那条不能省的。
- `-- note.md` 里的 `--` 是"后面全是路径"的分隔符。少了它，如果恰好有个分支也叫 `note.md`，git 就得猜。

**真实输出**（继续，把两行都提交掉，再比两个提交）

```text
$ git -c user.name="学习者" -c user.email="learn@example.com" commit -qam "note.md 两行都提交"
```

（`-q` 让 commit 什么都不输出 —— 想省那两行统计就这么用。）再比两个提交：

```text
$ git diff HEAD~1 HEAD -- note.md
diff --git a/note.md b/note.md
index 516fad4..972f82f 100644
```

（后面 8 行与上面 `git diff HEAD -- note.md` **逐字相同** —— 同一份差异，只是把两端换成了 `HEAD~1` 和 `HEAD`，
所以这里只贴到 `index` 行，它就足以证明两端的对象是同一对。）

```text
$ git show --stat --oneline HEAD
8d84e7f note.md 两行都提交
 note.md | 4 ++--
 1 file changed, 2 insertions(+), 2 deletions(-)
```

`note.md | 4 ++--` = 这个文件一共 4 行发生变化，右边两格是加、左边两格是删（`--stat` 的柱状图就这么读）。
`8d84e7f` 就是那句 `commit -qam` 造出来的提交（`-q` 让 commit 少打那两行统计，输出更干净）。

```text
$ git show HEAD~1:note.md
L1
L2
L3
```

`git show <版本>:<路径>` 是"给我那个时间点的这份文件"，不改动工作区、不留痕迹。
配合 `git diff <commit> -- 路径` 能回答"这行是谁什么时候改的"（05 章 `blame` 的前置动作）。

**常见翻车**
- 把 `git diff` 当成"这次要提交什么" → 它恰恰**不是**。要提交的内容用 `--staged` 看。
- 忘了 `--staged` 还有同名写法 `--cached`：`git diff --cached` 与 `git diff --staged` 输出完全一样，别被两个名字绕晕。
- 看 `git diff` 没输出就以为没改 → 已暂存的改动它不报；`git diff HEAD` 或 `status -s` 才算全。
- 想用 diff 看两个分支 → `git diff main feature`（比两端）和 `git diff main...feature`（从共同祖先比，
  等于 PR 页面显示的差异）不是一回事，差别见场景 6。

---

### 场景 6：log 家族 —— 历史怎么看，区间语法 `A..B` 与 `A...B`

**想干什么**：确认自己站在历史的哪个位置、和另一条线差几条；合并前知道"我落后多少、能不能放心合"。

demo 在沙箱里造了一个**真的分叉过**的历史（仓库 `loglab`）：`main` 上 A1/A2/A3，从 A2 岔出 `feature` 再走 F1/F2。
五个提交 A1 `9bd5835`（root）、A2 `e2b2a73`、F1 `0db2a6b`、F2 `5398305`、A3 `9e7bd7a`，
建的时候都是最普通的 `echo` + `add` + `commit`；这些哈希在下面每条输出里都会出现，可以逐条对上。
**敲哪些命令**

```bash
git log --oneline --graph --all     # 全分支拓扑，先看地形
git log --oneline -n 2              # 最近两条
git log -n 2 --stat --oneline       # 每条动了哪些文件、各几行
git log --oneline A..B              # B 有而 A 没有的（左端点不含）
git log --oneline A...B             # 两边各自独有的混在一起
git merge-base A B ; git rev-list --count A..B
```

**真实输出**

```text
$ git log --oneline --graph --all
* 5398305 F2 feature 第二次
* 0db2a6b F1 feature 第一次
| * 9e7bd7a A3 main 第三行
|/  
* e2b2a73 A2 main 第二行
* 9bd5835 A1 初始
```

读图：`*` 是一个提交，竖线是"这条线"，`|/` 是两条线汇合处 —— **共同祖先是 `e2b2a73`**，不用猜。
注意顺序：`log` 按时间倒序，所以 feature 的 F2 排在最上面（它是最晚写的），但它不在 `main` 上。

```text
$ git log --oneline -n 2
9e7bd7a A3 main 第三行
e2b2a73 A2 main 第二行

$ git log -n 2 --stat --oneline
9e7bd7a A3 main 第三行
 app.txt | 1 +
 1 file changed, 1 insertion(+)
e2b2a73 A2 main 第二行
 app.txt | 1 +
 1 file changed, 1 insertion(+)
```

**为什么 `--graph --all` 少了 `-n` 就什么都在**：`git log` 默认只沿 **HEAD 能到达**的提交走。
当前在 `main`，所以 feature 那两条完全不出现 —— 新手那句"我提交去哪了"九成是这个原因，
东西没丢，只是没往那条线上看。`--all` = 所有分支都走一遍。

**区间语法：两个点是减法，三个点是各说各话**

```text
$ git log --oneline e2b2a73..main
9e7bd7a A3 main 第三行

$ git log --oneline e2b2a73^..main
9e7bd7a A3 main 第三行
e2b2a73 A2 main 第二行

$ git log --oneline main...feature
9e7bd7a A3 main 第三行
5398305 F2 feature 第二次
0db2a6b F1 feature 第一次

$ git log --oneline --left-right main...feature
< 9e7bd7a A3 main 第三行
> 5398305 F2 feature 第二次
> 0db2a6b F1 feature 第一次
```

顺序也有意义：`git log --oneline feature..main` 只给 `9e7bd7a A3 main 第三行`，
反过来 `git log --oneline main..feature` 给 `5398305 F2 feature 第二次` + `0db2a6b F1 feature 第一次`
（这两条 demo 里有原文输出，此处省了；`--left-right` 那份留着是因为它把方向直接标了出来）

- `A..B` 读作**"B 里凡是 A 没有的"**，**左端点不含**。想含就写它的父 `e2b2a73^..main`（第二条多出了 `e2b2a73`）
  —— 这一条是 CI 脚本里最容易写错的地方。
- 顺序有意义：左边是"被减掉的"，右边是"要看的"。`feature..main` 给 A3，反过来给 F1/F2。
- `A...B` 是对称差，看不出方向；加 `--left-right` 后 `<` = 只有左边有、`>` = 只有右边有。
  上面 1 个 `<`、2 个 `>`，正好是"main 领先 1、feature 领先 2"。

**实战三连**

```text
$ git merge-base main feature
e2b2a7313bd2a7bd98bb78e1b17f8b72c564901a

$ git rev-list --count main..feature
2

$ git rev-list --count feature..main
1
```

`merge-base` 给全哈希 `e2b2a73…`，和图上 `|/` 那个位置互相验证。
跟人协作时报三个信息就够：**共同祖先 `e2b2a73`、我多 1 条、对面多 2 条**。

**★ `log` 的点号和 `diff` 的点号不是一回事**（本章最容易记反的一条）

```text
$ git diff --stat main...feature
 feat.txt | 2 ++
 1 file changed, 2 insertions(+)

$ git diff --stat main feature
 app.txt  | 1 -
 feat.txt | 2 ++
 2 files changed, 2 insertions(+), 1 deletion(-)
```

- `diff main...feature`：从**共同祖先**比到 `feature` → 只看见 feature 真正做的事（`feat.txt` +2）。
  这就是网页 PR 那个 "Files changed" 的算法。
- `diff main feature`（等价 `main..feature`）：直接比两端 → 多出 `app.txt | 1 -`。
  那不是 feature 删的，是 **main 自己加了、feature 线上当然没有**，比两端就显示成"删"。
- 同一个符号两套语义：`log` 里 `A...B` 是提交列表的对称差，`diff` 里是"从共同祖先比内容"。别串线。

**合并完再回头看一次**

```text
$ git -c user.name="学习者" -c user.email="learn@example.com" merge --no-edit -m "M 合并 feature" feature
Merge made by the 'ort' strategy.
 feat.txt | 2 ++
 1 file changed, 2 insertions(+)
 create mode 100644 feat.txt

$ git log --oneline --graph --all
*   d00588d M 合并 feature
|\  
| * 5398305 F2 feature 第二次
| * 0db2a6b F1 feature 第一次
* | 9e7bd7a A3 main 第三行
|/  
* e2b2a73 A2 main 第二行
* 9bd5835 A1 初始

$ git log --oneline e2b2a73..HEAD
d00588d M 合并 feature
9e7bd7a A3 main 第三行
5398305 F2 feature 第二次
0db2a6b F1 feature 第一次
```

`d00588d` 有两个父：`HEAD^1` = `9e7bd7a`（main 侧）、`HEAD^2` = `5398305`（feature 侧），
图上那个 `|\` 画的就是这件事。合并后 `main` 已经包含 `feature`，于是
`e2b2a73...HEAD` **退化**成和 `e2b2a73..HEAD` 一样的四条 —— 一边完全包含另一边时，对称差就等于减法，
这是集合性质不是 bug。

**加餐：为什么本章的图里没有 `(HEAD -> main)`**

demo 场景 6 的 `declab` 仓库，同一份历史（`A1 初始` → `main` 上 `A2 main 第二行`、`feature` 上 `F1 feature 第一次`），
三种接法。第一种用 `script` 造一个 pty 来模拟真终端（脚本里同时带 `-c color.ui=never`，
只为了别把 ANSI 转义符刷进捕获，装饰行为不受它影响）：

```text
$ script -qec "git --no-pager -c color.ui=never log --oneline --graph --all" /dev/null
* b6b7734 (feature) F1 feature 第一次
| * e2b2a73 (HEAD -> main) A2 main 第二行
|/  
* 9bd5835 A1 初始
```

```text
$ git --no-pager log --oneline --graph --all
* b6b7734 F1 feature 第一次
| * e2b2a73 A2 main 第二行
|/  
* 9bd5835 A1 初始

$ git --no-pager log --oneline --graph --decorate --all
* b6b7734 (feature) F1 feature 第一次
| * e2b2a73 (HEAD -> main) A2 main 第二行
|/  
* 9bd5835 A1 初始
```

规则：**输出连着终端才自动加装饰；被管道或重定向吞掉时不加**。本章 demo 全程往管道里写，
所以场景 6 的图没分支名。而场景 9 的 `git lg`（别名里显式写了 `--decorate`）在同样的重定向下
照样打出 `(HEAD -> main)` 和 `(feature)` —— 两处对照就是这条规则的完整证据，不需要额外信源。
**抓输出、写文档时想要装饰就显式写 `--decorate`**，别赌终端。

**常见翻车**
- "我的提交没了" → `git log --all --oneline` 或 `git reflog`，八成是分支没切过去。
- `git log A..B` 少了左端点还纳闷 → 记住**左边的不含**；含就写 `A^..B`。
- 卡在分页器里出不来 → 按 `q`；脚本里用 `git --no-pager log` 或把输出重定向。
- 想追一个改过名的文件 → `git log --oneline --follow -- 路径`，不加 `--follow` 改名前就断了。

---

### 场景 7：经典翻车 —— 文件已经提交了，才想起来要 ignore

**想干什么**：`.env` 已经进仓库了，过几天才想起来它不该进。现在补 `.gitignore`，然后把它请出去，
**但本地那份文件绝对不能删**（那是你机器上唯一一份配置）。本章最值钱的一节。

**敲哪些命令**

```bash
git rm --cached .env     # 从清单里除名，工作区文件保留
git add .gitignore       # 规则本身也得进仓库，否则保护不了别人
git commit -m "停止跟踪 .env"
```

**真实输出** —— 事故现场（沙箱仓库 `oops`）：

```text
$ printf 'DEBUG=1\nAPI_KEY=开发用的\n' > .env

$ echo 'code' > main.c

$ git add -A

$ git -c user.name="学习者" -c user.email="learn@example.com" commit -m "把 .env 也一起提交了（事故现场）"
[main (root-commit) 4a27e25] 把 .env 也一起提交了（事故现场）
 2 files changed, 3 insertions(+)
 create mode 100644 .env
 create mode 100644 main.c
```

`git add -A` 一把梭的代价：`.env` 在 `2 files changed` 里躺着。过了几天写规则、又改了一次 `.env`：

```text
$ printf '.env\n*.env\n' > .gitignore

$ printf 'SECRET=漏出去了\n' >> .env

--- 但 .env 已经被跟踪了，.gitignore 对它完全无效

$ git status -s
 M .env
?? .gitignore

--- check-ignore 还会'骗'你：它默认跳过已跟踪文件，所以什么都没输出

$ git check-ignore -v .env
[退出码 1]

--- 加 --no-index 才看见规则本来是命中它的

$ git check-ignore -v --no-index .env
.gitignore:2:*.env	.env
[退出码 0]
```

**逐行解读**
- ` M .env` 就是判决书：**已跟踪的文件还在被跟踪，`.gitignore` 对它一个字都不生效**。
  忽略规则只管"还没进清单的东西要不要提醒我"，不管"已经在清单上的要不要停止跟踪"。
- `check-ignore -v .env` 什么都不输出、退出码 1 —— 它**默认跳过已跟踪文件**。
  很多人在这儿以为"规则没写对"，其实是问错了工具。
- 加 `--no-index`（= 别管跟踪状态，纯按规则算）才看见命中。注意命中的是 **第 2 行 `*.env`** 而不是
  第 1 行 `.env`：两条都能匹配，git 报**最后一条命中的规则**。所以排查时"哪条规则生效"要看行号，别看内容像。

**修法**

```text
$ git rm --cached .env
rm '.env'

$ ls -a
.
..
.env
.git
.gitignore
main.c

--- 文件还在原地，但清单里已经没它了

$ git status -s
D  .env
?? .gitignore

$ git add .gitignore

$ git -c user.name="学习者" -c user.email="learn@example.com" commit -m "停止跟踪 .env，同时把 .gitignore 提交进仓库"
[main 6c7d106] 停止跟踪 .env，同时把 .gitignore 提交进仓库
 2 files changed, 2 insertions(+), 2 deletions(-)
 delete mode 100644 .env
 create mode 100644 .gitignore

$ printf 'SECRET=又加了一行\n' >> .env

--- 现在随便改 .env，status 一个字都不输出 = ignore 生效了

$ git status -s
```

- `ls -a` 那 6 行是这节最该记住的东西：**`--cached` 只动清单，不动硬盘**。
- `D  .env`：第 1 格 `D` = 暂存区比仓库少一个文件（待提交的删除），第 2 格是空格 = 工作区跟暂存区一致。
  文件明明还在硬盘上，那个 `D` 说的是"仓库里 will be deleted"，不是"我机器上没了"（对照场景 2 的两格读法）。
- 提交完再改 `.env`，`status -s` **完全空输出** —— 这一次 ignore 真的生效了，判据就是这一句。
- 为什么还要 `git add .gitignore` 并一起提交：未提交的 `.gitignore` **在你本机已经生效**，
  但它没进仓库，所以对同事、对 CI、对新克隆等于没有规则 —— 你修好的坑，别人会再踩一遍。

**★ 历史里那份还留着**（这条最容易被忽略，也是安全问题）

```text
$ git show HEAD^:.env
DEBUG=1
API_KEY=开发用的

--- 对比：git rm 不加 --cached，是连工作区文件一起删掉

$ git rm main.c
rm 'main.c'
```

这一次 `ls -a` 里就没有 `main.c` 了（demo 里紧跟着那次 `ls` 只剩 `.env`、`.git`、`.gitignore`），
`git status -s` 同样报 `D  main.c`。

`git rm --cached` 的 `D  `（第 1 格 D，删除已暂存）和不带 `--cached` 的 `D  ` 在 status 上长得一样，
区别只在 `ls`：前者文件还在、后者已经没了。另外注意 `git show HEAD^:.env` 一把就把密钥原文捞出来了 ——
**删除提交只是"从今天起不再跟踪"，历史里每个旧提交都还完整带着那份内容**。真漏了密钥：
密钥本身立刻作废重发，然后把旧历史当已泄露处理（07 章讲重写历史）。

**加餐：MM 状态下 `git rm --cached` 会拒绝干活**（demo 场景 7 的 `rmcache` 仓库：`cfg.txt` 先提交 v1，
再改成 v2 并 `add`，然后工作区改成 v3 —— 于是三个区各有一份不同的内容）

```text
$ git status -s
MM cfg.txt

$ git rm --cached cfg.txt   # 想把暂存区里那条撤掉
error: the following file has staged content different from both the
file and the HEAD:
    cfg.txt
(use -f to force removal)
[退出码 1]
```

报错的意思是："暂存区那份内容跟工作区不一样、也跟 HEAD 不一样，我删了它三份版本要丢一份，不敢自作主张"。
`git rm` **没执行** —— 这一条就是证据：

```text
$ git status -s   # 再问一次：还是 MM，证明上面那条 git rm 根本没执行
MM cfg.txt
```

解法不是 `-f`（那是"确认连工作区文件一起扔"），而是先 `git restore --staged cfg.txt` 退暂存区，
再 `git rm --cached cfg.txt`：

```text
$ git restore --staged cfg.txt

$ git status -s
 M cfg.txt

$ git rm --cached cfg.txt
rm 'cfg.txt'

$ git status -s
D  cfg.txt
?? cfg.txt

$ ls
cfg.txt
```

`git ls-files` 这时一个字都不输出（清单里已经没有 `cfg.txt` 了），而 `ls` 里它还在硬盘上。
第 1 格 `D` 说"要提交一个删除"，第 2 格 `??` 说"硬盘上它变成了野文件" —— 两个字母不矛盾，
正是"停止跟踪但保留文件"的准确状态。

**常见翻车**
- 只写 `.gitignore` 不 `git rm --cached` → `status` 照样报 ` M`，白写。
- 用了 `git rm`（忘了 `--cached`）→ 本地配置被删，还得从历史里 `git show <提交>:<路径> > 文件` 捞回来。
- 把 `.gitignore` 当成"机密文件保险箱" → 它只是"别提醒我"，**不是加密、不是不上云**。真机密放 `~/.ssh/` 这类仓库外的地方。
- 想取消忽略又已经跟踪了 → 见场景 8 的 `check-ignore -v`，先确认到底哪条规则在起作用。

---

### 场景 8：`.gitignore` 语法逐条实测 + `check-ignore -v` 排查

**想干什么**：规则写了不生效，或者生效了却把想留的文件吞了。要能**逐条问 git"这个路径凭什么被忽略"**。

沙箱仓库 `iolab` 里先造一批文件（`build/keep.txt`、`build/sub/other.txt`、`run.log`、`important.log`、
`docs/a/tmp/one.c`、`sub/tmp-only.txt`、`secretA.pem`、`secretAB.pem`、`notes/me.txt`、一个叫 `tmp` 的文件…），
然后放这 7 条规则：

```text
$ cat .gitignore
*.log
!important.log
build/*
!build/keep.txt
/tmp-only.txt
secret?.pem
docs/**/tmp/
```

**真实输出** —— 一次问 11 个路径，谁被哪条规则管全打出来：

```text
$ git check-ignore -v run.log important.log build/keep.txt build/sub/other.txt tmp-only.txt sub/tmp-only.txt secretA.pem secretAB.pem docs/a/tmp/one.c docs/a/b/tmp/two.c logs/nested.log
.gitignore:1:*.log	run.log
.gitignore:2:!important.log	important.log
.gitignore:4:!build/keep.txt	build/keep.txt
.gitignore:3:build/*	build/sub/other.txt
.gitignore:5:/tmp-only.txt	tmp-only.txt
.gitignore:6:secret?.pem	secretA.pem
.gitignore:7:docs/**/tmp/	docs/a/tmp/one.c
.gitignore:7:docs/**/tmp/	docs/a/b/tmp/two.c
.gitignore:1:*.log	logs/nested.log
[退出码 0]
```

**逐行解读**：格式是 `规则来自哪个文件:第几行:什么规则 <TAB> 命中的路径`（`<TAB>` 是真的制表符，
脚本里好切；你看到列对不齐就是这个原因）。这 9 行把语法全讲完了：

| 规则 | 命中 | 说明的语法 |
|---|---|---|
| `*.log` | `run.log`、`logs/nested.log` | `*` 跨不过 `/`，但**匹配任何一层**里的同名文件；`logs/` 本身没被忽略，里面的才忽略 |
| `!important.log` | `important.log` | `!` 是放行。**后写的规则赢**，所以取反要写在被取反的规则之后 |
| `!build/keep.txt` vs `build/*` | 前者放行 `keep.txt`，`build/*` 吞掉 `build/sub/other.txt` | `build/*` 匹配 `build` 的每个直接子项（含目录 `sub`），`sub/other.txt` 因为父目录被忽略也跟着没了 |
| `/tmp-only.txt` | 只命中根下那个 | **开头的 `/` = 只从仓库根算**，不递归。`sub/tmp-only.txt` 所以没出现 |
| `secret?.pem` | `secretA.pem`（`secretAB.pem` 没出现） | `?` = 恰好一个字符 |
| `docs/**/tmp/` | `docs/a/tmp/one.c`、`docs/a/b/tmp/two.c` | `**` 跨任意层；结尾 `/` 表示只匹配目录 |

**没被打印出来的路径 = 一条规则都没命中**，这一行输出（`sub/tmp-only.txt`、`secretAB.pem`）本身就是证据。
想让它们也显式列出来：

```text
$ git check-ignore -v -n run.log sub/tmp-only.txt secretAB.pem
.gitignore:1:*.log	run.log
::	sub/tmp-only.txt
::	secretAB.pem
[退出码 0]

$ git check-ignore -n run.log   （这条故意会失败，就为了看 git 怎么说）
fatal: --non-matching is only valid with --verbose
```

`-n` 让未命中也打一行，规则栏写成空的 `::`。**`-n` 必须配 `-v`**，否则 git 直接 fatal ——
这条报错本身也是排查时常见的一脚踩空。再看最终没被忽略的清单：

```text
$ git status -s --untracked-files=all
?? .gitignore
?? build/keep.txt
?? important.log
?? notes/me.txt
?? secretAB.pem
?? sub/tmp-only.txt
?? tmp
```

三个反例（都是真实踩坑）：

```text
$ printf 'build/\n!build/keep.txt\n' > .gitignore

$ git check-ignore -v build/keep.txt
.gitignore:1:build/	build/keep.txt
[退出码 0]

--- 换成 build/* 取反才生效

$ git check-ignore -v build/keep.txt build/sub/other.txt
.gitignore:2:!build/keep.txt	build/keep.txt
.gitignore:1:build/*	build/sub/other.txt
[退出码 0]
```

写成 `build/` 时**取反失效**：git 一旦判定某目录被忽略，**根本不会走进这个目录**，
里面的 `!build/keep.txt` 永远没机会被考虑。正确写法是 `build/*` 这种"忽略目录里的东西、留下目录本身"。
这是 `.gitignore` 最反直觉的一条，很多"我明明 ! 了"的问题都是它。

```text
$ printf 'tmp\n' > .gitignore

$ git check-ignore -v tmp docs/a/tmp/one.c
.gitignore:1:tmp	tmp
.gitignore:1:tmp	docs/a/tmp/one.c
[退出码 0]

$ printf 'tmp/\n' > .gitignore

$ git check-ignore -v tmp docs/a/tmp/one.c
.gitignore:1:tmp/	docs/a/tmp/one.c
[退出码 0]
```

不带结尾 `/` 时 `tmp` 同时打中那个**文件** `tmp` 和目录 `docs/a/tmp/` 里的内容；加了 `/` 只匹配目录，
`tmp` 文件就漏出去了（第二次输出里只剩一行）。想两者都盖住就写两行：`tmp` 和 `tmp/`。

```text
$ printf 'notes/ # 忽略笔记目录\n' > .gitignore


$ git check-ignore -v notes/me.txt
[退出码 1]

--- notes/me.txt 没被忽略；改成规规矩矩的一行才生效

$ printf 'notes/\n# 这行才是真正的注释\n' > .gitignore

$ git check-ignore -v notes/me.txt
.gitignore:1:notes/	notes/me.txt
[退出码 0]
```

**行尾注释不合法**：`notes/ # 说明` 整行被当成一条规则（`#` 只在行首才算注释），于是谁都不匹配，
退出码 1。规则越写越长时把说明单独起一行、行首加 `#`。

最后两条本章只点到：
- `--no-index` 是给**已跟踪**文件排查用的（场景 7 那句 `git check-ignore -v .env` 空输出就是同一坑的另一半）。
  demo 末尾对未跟踪的 `run.log` 用 `--no-index` 退出码 1，是因为此刻 `.gitignore` 已经被上面改成只剩
  `notes/` 和一行注释 —— 规则本身已经不认 `*.log` 了，跟 `--no-index` 无关，别把它读成"加了 `--no-index` 就失灵"。
- 忽略规则有三个来源：仓库里的 `.gitignore`（进仓库、给别人用）、`.git/info/exclude`（只你这台机器、不进仓库）、
  `core.excludesFile` 指的全局文件（本机所有仓库通用，第 06 章专讲，本章不配它免得动你的全局设置）。

**常见翻车**
- 以为 `.gitignore` 能撤掉已跟踪文件 → 见场景 7，必须 `git rm --cached`。
- 忽略整个目录后又想放行一个文件 → 用 `build/*` 不是 `build/`。
- 规则没生效先别怀疑 git：`git check-ignore -v <路径>` 一句就知道是谁在起作用、还是压根没规则命中。
- `status -s` 只看见 `dir/` 看不见里面的文件 → 默认把未跟踪目录折叠了，加 `-u`/`--untracked-files=all` 才摊开。


---

### 场景 9：别名与常用配置 —— 只用 `--local` 和 `-c`，绝不动你的全局

**想干什么**：`git status -sb`、`git log --oneline --graph --decorate --all` 这种长命令想敲短的，
但**不想把你本机的全局配置搞脏**（这是全书的硬规矩：本章所有配置只写 `.git/config` 或 `-c` 一次性传参）。

**敲哪些命令**

```bash
git -c alias.w='status -sb' w              # 写法一：只活在这一条命令里
git config --local alias.st 'status -sb'   # 写法二：只写进本仓库的 .git/config
git config --local --show-origin --get alias.st
```

**真实输出**（仓库还是场景 6 那个 `loglab`）

```text
$ git -c alias.w='status -sb' w
## main
[退出码 0]

$ git config --local alias.st 'status -sb'

$ git config --local alias.ci 'commit -m'

$ git config --local alias.lg 'log --oneline --graph --decorate --all'

$ grep -A4 '\[alias\]' .git/config
[alias]
	st = status -sb
	ci = commit -m
	lg = log --oneline --graph --decorate --all

$ echo '+一行改动' >> app.txt

$ git st
## main
 M app.txt
```

`grep` 那 4 行是**证据**：别名就是往 `.git/config` 里写几行文本，`git st` 展开成 `git status -sb`
—— 纯文本替换，git 不做任何聪明事（所以别名里想带参数就照常往后拼）。`[alias]` 下面是 Tab 缩进，
复制配置时别把它改成空格。

```text
$ git config --local --show-origin --get alias.st
file:.git/config	status -sb
[退出码 0]
```

`git config --local --list` 是**拍平**的写法：`alias.st=status -sb`、`alias.ci=commit -m`、`alias.lg=…`
再加 `core.repositoryformatversion` / `core.filemode` / `core.bare` / `core.logallrefupdates` 四行默认值 ——
跟 `.git/config` 里的 ini 长相不同，别看花。`--show-origin` 则是"我这配置到底在哪写的"的排查利器：
输出格式是 `文件来源<TAB>值`，一眼看出是 local 还是 global。

```text
$ git config --local --show-origin --get core.quotepath
file:.git/config	false
[退出码 0]
```

三个常开的本地开关（都只用 `--local` 写）：`core.quotepath false`（中文文件名不再显示成八进制转义）、
`pull.rebase false`（`pull` 用 merge 不用 rebase，初学阶段更安全）、`init.defaultBranch main`（只影响本仓库）。

**别名不带身份照样提交不了 —— 这正是全书每条 commit 都带 `-c` 前缀的原因**

```text
$ git config --local --get user.email
[退出码 1]

$ git -c user.name='学习者' -c user.email='learn@example.com' config --get user.email
learn@example.com
[退出码 0]

$ git ci '用别名 ci 提交的'
Author identity unknown

*** Please tell me who you are.

Run

  git config --global user.email "you@example.com"
  git config --global user.name "Your Name"

to set your account's default identity.
Omit --global to set the identity only in this repository.

fatal: unable to auto-detect email address (got '<本机登录名>@<本机主机名>.(none)')
[退出码 128]
[上面 fatal 里的 <本机登录名>@<本机主机名> 是脚本换掉的占位符，其余原文]

$ git add app.txt

$ git -c user.name="学习者" -c user.email="learn@example.com" ci '用别名 ci 提交的'
[main e34266d] 用别名 ci 提交的
 1 file changed, 1 insertion(+)

$ git st
## main
```

- `config --local --get user.email` 退出码 1（没配）—— 但 demo 每条提交都成功了，因为身份是 `-c` 传进去的，
  `-c` 的优先级高于配置文件。第二条命令正是拿 `config --get` 现场读出 `-c` 传进去的值。
- 那段 `Author identity unknown` 的提示**要你 `git config --global`**：本章（和全书）都不照它做。
  临时实验一律 `-c`，真要长期用就在**单个仓库**里 `git config --local user.email ...`。
- 最后那行 `fatal` 里 git 会把你本机自动猜测的身份打出来，那是**真实登录名和主机名**。
  demo 脚本在这条命令上套了一层脱敏（`mask_id`：只把登录名/主机名换成 `<本机登录名>` `<本机主机名>` 两个占位符，
  并在输出末尾补一行说明），所以**屏幕上和捕获里都不留真实机器信息**；你在自己机器上跑，看到的就是你自己机器的真值。
  本章其余输出都是原文照贴。
- `git ci`（别名 `commit -m`）加上 `-c` 前缀后照常可用 —— 别名和 `-c` 是两套互不干扰的机制。

```text
$ git lg
* e34266d (HEAD -> main) 用别名 ci 提交的
*   d00588d M 合并 feature
|\  
| * 5398305 (feature) F2 feature 第二次
| * 0db2a6b F1 feature 第一次
* | 9e7bd7a A3 main 第三行
|/  
* e2b2a73 A2 main 第二行
* 9bd5835 A1 初始
```

这行输出顺手把场景 6 的加餐闭环了：同样是重定向到文件，`git lg` 因为有显式 `--decorate`，
`(HEAD -> main)` 和 `(feature)` 都打出来了；而场景 6 里没写 `--decorate` 的那几条就没有。

**常见翻车**
- `alias.ci = commit -m` 之后 `git ci 没有说明` → 少写一个引号包参数；别名只是文本替换，参数照常往后接。
- 别名写在全局结果换台机器就失效（或者反过来，别人机器上有你猜不到的别名）→ 项目专用别名写 `--local`，
  需要跨机器共享的写进 dotfiles（06 章）。
- 想删掉别名：`git config --local --unset alias.st`；想看是谁写的：`--show-origin`。
- `git config --local --list` 看不到 `[remote]` 之类？那是因为这个仓库是 `init` 出来的、没有远程；
  克隆出来的仓库里能看到（对照场景 1 那份 `.git/config`）。

---

## 命令速查表

```bash
# ── ② 看现场（只读，随时插）────────────────────────────
git status                      # 长格式，带"下一步该敲什么"的提示
git status -s                   # 短格式：两格状态 + 文件名
git status -sb                  # 加一行 ## 分支...上游 [ahead n, behind n]
git status -s --ignored         # 连被忽略的也列出来（!! 开头）
git status -s --untracked-files=all   # 未跟踪目录不折叠，摊开到文件

# ── ③ 挑要提交的 ────────────────────────────────────
git add 路径                    # 整个文件进清单
git add -p 路径                 # 逐块挑：y n s(拆块) e(手改这块) q a d
git add -u                      # 已跟踪文件的改动全进（不含新文件）
git add .                       # 当前目录起，新文件也收（会尊重 .gitignore）
git add -A                      # 全仓库所有变化，包括删除 —— 慎用
git reset                       # 整个暂存区退回工作区（不动文件、不动历史）
git restore --staged 路径        # 只退一个文件

# ── ④ 复核（只读）───────────────────────────────────
git diff                        # 工作区 vs 暂存区：add 之后又改了什么
git diff --staged               # 暂存区 vs 仓库：这次 commit 会带上什么 ★
git diff HEAD                   # 工作区 vs 仓库：总共差多少
git diff A B                    # 任意两端；A...B = 从共同祖先比（= PR 那个 diff）

# ── ⑤ 装订 ──────────────────────────────────────────
git commit -m "说明"             # 只装订清单
git commit -am "说明"            # 顺手把已跟踪的改动进清单再装订（新文件带不上）
git commit --amend -m "新说明"   # 换掉最后一条，哈希必变；旧的那条在 reflog 里

# ── ⑥ 回看（只读）───────────────────────────────────
git log --oneline -n 5
git log --oneline --graph --decorate --all
git log -n 2 --stat --oneline
git log --oneline A..B          # B 有 A 没有（左端点不含）；含就写 A^..B
git merge-base A B ; git rev-list --count A..B
git show HEAD:路径               # 那个版本这份文件的原文
git reflog -5                   # 你这只手最近干过什么（含被 amend 掉的）

# ── 停止跟踪 / 删文件 ───────────────────────────────
git rm --cached 路径             # 除名，本地文件留（.env 事故的解药）
git rm 路径                     # 除名 + 本地文件也删
git rm -r --cached 目录          # 整目录除名（配合 .gitignore 用）

# ── 忽略规则排查 ────────────────────────────────────
git check-ignore -v 路径…        # 谁命中哪条规则：文件:行:规则 <TAB> 路径
git check-ignore -v --no-index 路径   # 已跟踪文件也要问（默认会跳过）
git check-ignore -v -n 路径…      # 未命中也列出来（-n 必须配 -v）

# ── 配置：本章只用这两种 ────────────────────────────
git -c alias.w='status -sb' w    # 一次性，命令结束就没了
git config --local alias.st 'status -sb'
git config --local --show-origin --get alias.st
git config --local --list
git config --local --unset alias.st
```

**这一章绝不出现的写法**：`git config --global …`（改你本机所有仓库）、`sudo`、任何往真实项目里
随手提交的命令。想试就 `mktemp -d` 一个沙箱，跑完 `rm -rf`。

---

## 这章的坑

1. **`.gitignore` 管不了已跟踪文件**。这是本章最贵的一个：`.env` 提交完再补规则，`status` 照报 ` M`。
   唯一解药是 `git rm --cached`，而且**历史里那份永远还在** —— 真密钥要立刻作废重发。
2. **`git commit -am` 带不上新文件**。`-a` 的作用域是"已跟踪"；`??` 的文件它连名字都不知道。
   新文件必须 `add` 一次，之后才归 `-am` 管。
3. **`git diff` 不是"这次要提交什么"**。它比的是工作区 vs 暂存区，恰恰**跳过**了要提交的那部分。
   提交前必看的是一条 `git diff --staged`。
4. **`add -A` 一把梭 = 把 `node_modules` 和 `.env` 一起送进历史**。先 `.gitignore`、再 `status -s` 过一遍，
   然后才 `add`。顺序不能反。
5. **`git log A..B` 的左端点不包含**。CI 脚本里漏掉一个提交的经典原因；要含就写 `A^..B`。
6. **`log` 的点号和 `diff` 的点号语义不同**。`log A...B` 是提交列表的对称差；
   `diff A...B` 是"从共同祖先比内容"（PR 页面用的就是这个）。两套记号，别串。
7. **`--amend` 之后不能直接 push**。你换掉了历史；已经推出去的提交要按 03 章的流程处理，别硬 `--force`。
8. **`.gitignore` 写 `build/` 之后 `!build/keep.txt` 失效**。git 判定目录被忽略就不再进入它，
   里面的取反没机会生效。写 `build/*`。
9. **行尾 `#` 不是注释**。`notes/ # 说明` 整行被当成规则，谁都不匹配。注释单独一行、行首 `#`。
10. **`git check-ignore -v` 对已跟踪文件默认沉默**，容易被读成"规则没写对"。加 `--no-index` 才是问规则本身；
    而它报的是**最后一条命中的规则**，看行号别看你觉得像的那条。
11. **`git rm --cached` 在 `MM` 状态下会拒绝**（暂存内容和工作区、HEAD 都不同）。
    解法是 `git restore --staged` 再 `git rm --cached`，不是 `-f`。
12. **`status -s` 两个格子是两次比较**，不是一个状态。`D  ` 和 ` D`、`MM` 和 ` M`、`A ` 和 `AM` 意思完全不同，
    读法见场景 2。

---

## 自检问题

**Q1：`git status -s` 输出 `MM note.md`，`git diff --staged -- note.md` 和 `git diff -- note.md` 会分别告诉你什么？**
答：`MM` = 暂存区比仓库有改动、工作区比暂存区还有改动。`--staged` 报"这次 commit 会带上什么"（L2 那处），
`diff` 报"add 之后又改了什么"（L3 那处）。两条都看，才知道工作区里有一半没进这次提交。
场景 5 里三条 diff 的 `index` 行用的是同样三个哈希（`516fad4`/`f9adb40`/`972f82f`），这就是最硬的验证。

**Q2：`git commit -am "..."` 成功返回了，为什么我的新文件还是 `??`？**
答：`-a` 只把**已跟踪**文件的改动补进清单。`??` 从没进过清单，`-a` 不知道它。先 `git add` 它一次；
此后它的改动就归 `-am` 管了（场景 4 的 `brand_new.txt` 就是这个流程）。

**Q3：怎么把 `app.c` 里 12 处改动中的 2 处提交，剩下 10 处留在工作区？**
答：`git add -p app.c` 逐块选（一块里改太多用 `s` 拆，`git add -p` 的 `?` 会打印全部键），
然后 `git commit -m "只提交第 2 行的重要修复"`（不带 `-a`！），`git diff --staged` 复核后再按回车。
判据：提交完 `git diff -- app.c` 还有输出、`status -s` 显示 ` M`，才说明剩下的确实留下了。

**Q4：`.env` 已经在仓库里了，现在写 `.gitignore` 有用吗？该怎么办？**
答：对**已经跟踪的** `.env` 完全没用（`.gitignore` 只管未跟踪文件要不要提醒你）。
`git rm --cached .env`（本地文件保留，用 `ls` 确认）→ `git add .gitignore` → 一起提交。
之后 `status -s` 对 `.env` 空输出才算成事。历史里那份还能 `git show HEAD^:.env` 捞出来，所以密钥要作废重发。

**Q5：`git log --oneline main...feature` 和 `git diff --stat main...feature` 的三个点含义相同吗？**
答：不同。前者是"两边各自独有的提交混在一起"（对称差，加 `--left-right` 才分方向）；
后者是"从共同祖先开始比内容"，只看 `feature` 真正改了什么（`feat.txt +2`），
不含 `main` 侧那些改动 —— 也就是 `diff main feature` 直接比两端时多出的那行 `app.txt | 1 -` 的来历。

**Q6：怎么知道你领先/落后上游几条？为什么 `status -sb` 有时说 "up to date" 其实远程已经变了？**
答：`git status -sb` 首行 `## main...origin/main [ahead 1, behind 1]`，或
`git rev-list --count origin/main..main`（领先）/ `main..origin/main`（落后）。
`origin/main` 只是**你上次 fetch 时**本地存的一张快照，不 fetch 就永远是旧的 ——
所以"up to date"可能只是"我还没去看"（场景 1 加餐实测：fetch 之前 `[ahead 1]`，之后才变 `[ahead 1, behind 1]`）。

**Q7：`git check-ignore -v .env` 什么都没有输出、退出码 1。是规则写错了吗？**
答：不一定，先问是不是已跟踪。`check-ignore` 默认跳过已跟踪文件，所以命中了也不报。
加 `--no-index` 再问一次；如果报了 `.gitignore:2:*.env`，说明规则没问题，是**跟踪状态**的问题（回到 Q4）。
另外它报最后命中的规则，所以行号 2 而不是 1 是正常的。

**Q8：想把 `git log --oneline --graph --decorate --all` 收成 `git lg`，怎么配才不会污染别的项目？**
答：`git config --local alias.lg 'log --oneline --graph --decorate --all'` —— `--local` 写的是
`.git/config`，只对当前仓库有效；`git config --local --show-origin --get alias.lg` 能验证来源确实写着
`file:.git/config`。临时试一次用 `git -c alias.lg='…' lg`。全局配置（`--global`）本章不碰。

---

## 运行 demo

```bash
cd 02-daily-commit
bash demo.sh              # 全程沙箱，跑完自动清理
bash demo.sh --keep       # 跑完保留沙箱，进去自己敲
```

脚本的护栏（都在 `demo.sh` 开头几行）：

- `set -euo pipefail`：任何一条命令非 0 就整体失败，不会静默半截。要看**失败长什么样**的命令走三个小工具：
  `grc`（打退出码）、`gf`（打退出码并写明"这条故意会失败"）、`grcm`（同 `grc`，但输出先过一层脱敏）。
  它们内部都是 `|| rc=$?`，所以脚本自己不会挂。
- `SB=$(mktemp -d "${TMPDIR:-/tmp}/git02-XXXXXX")` + `trap cleanup EXIT`：所有写操作只在这一个随机目录里，
  结束（含报错、Ctrl-C）都删干净。
- `export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null`：**读不到也不写你的全局配置**；
  提交身份统一 `-c user.name="学习者" -c user.email="learn@example.com"`，`git config` 一律 `--local`。
  唯一例外是 `nomit` 仓库：那一节要演的是"身份已经有了、提交却没成"，所以用 `--local` 把身份写进沙箱仓库自己。
- `GIT_AUTHOR_DATE` / `GIT_COMMITTER_DATE` 固定成 `2026-01-01T10:00:00 +0800`：内容 + 身份 + 时间都定死，
  所以**哈希可复现**（本章连跑两次，除了沙箱名逐字节相同）。
- `LC_ALL=C.UTF-8` 钉英文输出、`GIT_PAGER=cat` 关分页器、`GIT_MERGE_AUTOEDIT=no` 让 merge 不弹编辑器。
- 全程不联网：所谓"远程"是沙箱里 `git init --bare` 出来的 `origin.git` / `update.git`，
  `push`/`fetch` 都只在这台机器上走。

`--keep` 结尾长这样（沙箱名每次不同）：

```text
以上 9 个场景，就是 README 里 9 个小节的证据链。

[沙箱已保留] /tmp/git02-IfEd0c
  进去自己敲： ls "/tmp/git02-IfEd0c"，再 cd 进 daily / loglab / oops / iolab / w / w2 / patchlab / nomit / declab / rmcache 任意一个
  看完自己删： rm -rf "/tmp/git02-IfEd0c"
```

沙箱里你会得到这些目录，对应本章的场景：

| 目录 | 里面是什么 | 对应场景 |
|---|---|---|
| `newrepo` | `git init` 出来的空仓库（只有 `.git`） | 1 |
| `seed` → `origin.git` | 普通仓库 + 它的 bare 克隆，本地冒充远程 | 1 |
| `work` | `git clone` 出来的工作副本，带 `[remote]` 和 `origin/main` | 1 |
| `update.git` + `w` + `w2` | 加餐：`ahead` / `behind` 是怎么被 `fetch` 照出来的 | 1d |
| `daily` | 9 条提交的日常仓库，`status`/`add`/`commit`/`diff` 全在这儿演 | 2–5 |
| `patchlab` | 加餐：`add -p` 的 `?` 帮助屏和 `s` 拆块 | 3 |
| `nomit` | 加餐：三种"没提交成功"的原文报错 | 4 |
| `loglab` | 真分叉过再合并的历史（含 `--local` 别名） | 6、9 |
| `declab` | 加餐：终端里自动装饰 vs 管道里不装饰 | 6 |
| `oops` | `.env` 事故 + `git rm --cached` 补救 | 7 |
| `rmcache` | 加餐：`MM` 状态下 `git rm --cached` 拒绝干活 | 7 |
| `iolab` | 7 条 `.gitignore` 规则逐个实测的试验田 | 8 |

进去值得自己敲几条（都是只读的，不会把沙箱搞乱）：

```bash
cd /tmp/git02-XXXXXX/daily
git log --oneline                 # 应该看到 9 条，顶端是 8d84e7f
git show --stat HEAD              # 那 4 行 ++-- 是哪来的
git status -s                     # daily 最后是干净的，什么都不输出

cd ../oops
git ls-files                      # .env 已经不在清单里，main.c 也被 git rm 掉了
git show 4a27e25:.env             # 手痒验证"历史里还在"

cd ../loglab
git log --oneline --graph --decorate --all   # 显式 --decorate 才有 (HEAD -> main)
git cat-file -t HEAD; git cat-file -p HEAD | head   # 01 章的对象层，本章再接一次
git config --local --list                  # 别名就写在这里
```

看完记得 `rm -rf /tmp/git02-XXXXXX`（那个随机名结尾两行已经提示你了）。

---

## 下一步

闭环跑通了：能看现场、能挑内容、能复核、能装订、能回看，还能把误提交的 `.env` 请出去。
但这套动作只覆盖"一切顺利"的半天。真正让你晚上睡不着的是**敲错**：
`checkout` 错了分支、`reset --hard` 手滑、`clean -fd` 把没跟踪的临时文件全清了、
`pull` 把仓库搅成一团乱麻。

下一章 **03 撤销与找回**：所有"撤回"动作画在同一张三个区的图上，
讲清每条命令动哪几格、会不会丢、丢了怎么用 `reflog` 捞（场景 4 末尾那句
"`commit (amend)` 还躺在 reflog 里"就是给 03 章埋的门）。

顺手能练的（不用读新章就能做）：把你手上任意一个真实项目，
用 `git status -sb` + `git diff --staged` 的组合写出**一条有意义的提交说明**（第一行讲"为什么"，
空一行，正文讲"这次带了哪几处"）—— 本章六步闭环里唯一还剩下的手艺活就是它。
