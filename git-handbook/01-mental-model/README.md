# 心智模型 mental-model

> 一句话：这一章教你把 Git 看成**三个区 + 一堆内容寻址的文件 + 两个指针**，
> 之后所有命令（包括你抄来的那些诡异的一行流）都能自己推导出来，不用背。

这一章不讲任何"高级技巧"。它只做一件事：**把 `.git` 目录拆开给你看**，
用真跑出来的命令输出当证据，证明"分支就是一个文本文件""提交就是一张快照""HEAD 就是一个指针"。
后面每一章遇到想不通的命令，回到这一章的图上看一眼就通了。

---

## 你会遇到的场景

1. 别人教程里说 "modified / staged / untracked / deleted / 已跟踪 / 未跟踪"，你分不清哪个词说的是哪个地方变了。
2. `git status` 让你 "use git restore --staged <file>"，你完全不知道它要你干什么。
3. 你以为 Git 存的是"改了什么"（差异），所以担心"改一行不会让仓库越来越大吗"。
4. 切分支时明明只花了 0.1 秒，你不懂它凭什么这么快。
5. `git checkout <一串哈希>` 之后 git 开始说一堆"detached HEAD"，你不敢动，怕把东西弄丢。
6. 想知道 `.git` 目录里那 7 个东西到底是干什么的，`HEAD`、`objects`、`refs`、`index` 分别是谁。

---

## 心智模型

### 图 1：三个区（这是所有 git 命令的坐标系）

```
        你直接摸得到的文件               git 的小抄                永久档案库
     ┌──────────────────────┐    ┌──────────────────────┐   ┌──────────────────────┐
     │  工作区 working dir   │    │  暂存区 index/staging │   │   仓库 repository     │
     │  （就是你自己编辑的那  │    │  = .git/index 这个    │   │   = .git/objects 里   │
     │   些真实文件）         │    │   二进制清单文件      │   │   的对象 + refs       │
     │                      │    │                      │   │                      │
     │  note.txt            │    │  100644 db887d6... 0 │   │  commit 对象          │
     │  todo.txt            │    │  100644 397b4a7... 0 │   │    └→ tree 对象        │
     │  junk.log  ←gitignored│   │  100644 a45cdf0... 0 │   │        └→ blob 对象    │
     └──────────┬───────────┘    └──────────┬───────────┘   └───────────▲──────────┘
                 │  git add <file>          │   git commit              │
                 │  （把"内容副本"抄进清单） │  （把清单装订成永久档案）   │
                 └──────────────────────────┘                           │
                                                                        │
                 git checkout / git restore ←───────────────────────────┘
                 （把档案里的某份内容写回工作区）
```

三条箭头就是日常 90% 的命令：

| 方向 | 命令 | 人话 |
|---|---|---|
| 工作区 → 暂存区 | `git add` | "下一次提交里请带上这个版本" |
| 暂存区 → 仓库 | `git commit` | "把现在的清单装订成永久档案" |
| 仓库 → 工作区 | `git checkout` / `git restore` | "把档案里的内容抄回来覆盖我的文件" |
| 仓库 → 暂存区 | `git restore --staged` | "清单里那条撤回，文件别动" |

**`git status` 的四类措辞，就是在报告"哪两个区之间不一致"**（本章场景 2 实测）：

| status 的措辞 | 短格式 | 说明的是哪两个区在打架 |
|---|---|---|
| `Changes to be committed`（要提交的变更） | 第一列有字母，如 `A ` `M ` `D ` | 暂存区 ≠ 仓库 |
| `Changes not staged for commit`（尚未暂存以备提交的变更） | 第二列有字母，如 ` M` ` D` | 工作区 ≠ 暂存区 |
| `Untracked files`（未跟踪的文件） | `??` | 工作区里有，暂存区清单里根本没这一行 |
| `nothing to commit, working tree clean` | 无输出 | 三个区一致 |

⚠ 短格式的两个字母**不是**"一种状态"，而是"两次比较的结果"：
`MM` = 暂存区比仓库多了点东西 **并且** 工作区又比暂存区多了点东西（第一次比较在第一列，第二次在第二列）。

### 图 2：`.git` 里面的东西（谁存内容、谁存名字）

```
.git/
├── HEAD              ← 一个文本文件。内容要么 "ref: refs/heads/main"，要么一串裸哈希
├── config            ← --local 配置。（git config --global 写的是 ~/.gitconfig，另一个文件）
├── description       ← 给 gitweb 看的说明，可以无视
├── hooks/            ← 14 个 .sample 模板，去掉 .sample 后缀才会生效（默认全部不生效）
├── info/exclude      ← 本机专用的 .gitignore（不会跟着仓库提交出去）
├── index             ← 暂存区本体：一张二进制清单，记录"下次提交要有哪些文件+哪个哈希"
├── objects/          ← 【仓库本体】所有内容的碎片，文件名就是内容的哈希前 2 位
│   ├── db/887d64...   ← 一个 blob = 一份文件内容（zlib 压缩后的原文）
│   ├── ee/76580b...   ← 一个 tree = 一张目录清单（文件名 → 哈希）
│   ├── b6/c8fd4...    ← 一个 commit = tree 指针 + parent 指针 + 作者 + 说明
│   └── pack/          ← 对象多了以后 git 打包瘦身放这里
└── refs/
    ├── heads/main     ← 【分支就是一个文本文件】里面只有一行：那串 commit 哈希
    ├── heads/experiment
    └── tags/          ← 标签同理，也是写哈希的文本文件
```

指针关系画出来是这样（**"移动分支"= 改一个文件里的一行字符串**）：

```
   HEAD（.git/HEAD）
     │  内容："ref: refs/heads/main"
     ↓
   refs/heads/main            ← 分支本体，就是个文本文件
     │  内容："125210120aefce636cb496eb0ab658b3be5fd9ed"
     ↓
   objects/12/52101...        ← commit 对象
     │  tree ee76580b...  parent 8cd504cd...
     ↓                          ↓
   objects/ee/76580b...      上一个 commit（就这样一路串成"历史"）
     │  blob db887d64... blob a45cdf0e...
     ↓
   objects/db/887d64...      ← 某一份文件的完整内容
```

### 图 3：commit 是快照，不是差异

```
你以为的 Git（差分存储）：          实际的 Git（全量快照 + 内容去重）：

  v1 ──+3行── v2 ──+1行── v3        提交1 → tree1 → [blobA1, blobB1]
  改一行      再改一行               提交2 → tree2 → [blobA2, blobB1]  ← B 没变，指针复用
  就要存一份补丁                     提交3 → tree3 → [blobA2, blobB1]  ← 内容相同 → 直接复用同一份 blob
                                     ↑
                            tree 对象很小（就几行"文件名+哈希"），
                            所以"多一次提交"≈"多两个小文件 + 变更的那份文件"
```

---

## 场景逐个实战

> 下面所有输出都是 `bash demo.sh` 真跑出来的（沙箱 `/tmp/git01-MDQtO9`，跑完自动删；
> 那串随机后缀每次都不同，你看到的是你自己的名字，不影响其他任何一行）。
> 时间戳、提交身份、文件内容全部固定，所以你跑出来的**哈希和这里一模一样**（本章连跑两次逐字节相同）。
> 凡是需要写提交身份的 `commit`，命令行里都会带上 `-c user.name="学习者" -c user.email="learn@example.com"`，
> 为的是**不碰你的全局配置**；`add`/`status`/`log` 这些不产生提交的命令就不用带。
> 输出里 git 说的是英文，因为脚本把语言固定成英文保证可复现；你手敲时大概率是中文，
> 本章末尾的"中英对照"就是给你翻译用的。

### 场景 1：新建仓库，先把 `.git` 拆开看

**什么情况**：`git init` 敲完，屏幕上只有一句 "Initialized empty Git repository"，你不知道它到底建了什么。

**敲哪几条**

```bash
git init -b main demo      # -b main 指定初始分支名，不加它老版本 git 会用 master
cd demo
ls .git                    # 顶层 7 个东西
find .git/objects .git/refs | sort
cat .git/HEAD              # ★ 最重要的一条
cat .git/config
ls .git/hooks
git rev-parse HEAD         # 问 HEAD 要哈希
```

**为什么是这几条**：`ls`/`find` 是"眼见为实"，`cat .git/HEAD` 直接证明**分支和 HEAD 都是文本文件**，
`git rev-parse HEAD` 则是故意失败一次，让你知道"HEAD 现在指向的分支还没有任何提交"。

**真实输出**

```text
$ git init -b main demo
Initialized empty Git repository in /tmp/git01-MDQtO9/demo/.git/

$ ls .git
HEAD
config
description
hooks
info
objects
refs

$ find .git/objects .git/refs | sort
.git/objects
.git/objects/info
.git/objects/pack
.git/refs
.git/refs/heads
.git/refs/tags

$ cat .git/HEAD
ref: refs/heads/main
```

`git init` 时 `objects/` 和 `refs/` 下面**一个对象、一个分支文件都没有**（只有空目录），
所谓"仓库"此刻只是个空架子：

```text
$ find .git/objects .git/refs | sort
.git/objects
.git/objects/info
.git/objects/pack
.git/refs
.git/refs/heads
.git/refs/tags
```

七个东西分别是什么：

| 路径 | 人话解释 |
|---|---|
| `HEAD` | "我现在在哪"。文本文件，内容要么是 `ref: 某个分支文件的路径`，要么是一串裸哈希（那就是 detached，见场景 6） |
| `config` | 这个仓库的私人设置。`git config --local xxx` 写的就是它；`--global` 写的是 `~/.gitconfig`（另一个文件） |
| `description` | 给老式网页界面 gitweb 显示的一句话介绍，个人项目基本用不到 |
| `objects/` | **仓库本体**。所有文件内容、目录清单、提交，全都以"哈希 → 内容"的形式躺在这里 |
| `refs/heads/` | 分支们。每个分支 = 一个文本文件，文件内容 = 一个 commit 哈希 |
| `index` | 暂存区本体（`git init` 时还没有，第一次 `git add` 才被创建）。二进制，别用 cat，用 `git ls-files -s` 读 |
| `hooks/` | 钩子脚本目录 |
| `info/exclude` | 只在本仓库生效、又不会提交出去的 .gitignore（第 06 章实测） |

hook 默认全部不生效，因为文件名都带 `.sample` 后缀：

```text
$ ls .git/hooks | head -4
applypatch-msg.sample
commit-msg.sample
fsmonitor-watchman.sample
post-update.sample
  （一共 14 个 .sample，去掉 .sample 后缀才会生效）
```

`config` 里的默认内容（注意：**没有任何身份设置**，所以提交必须靠 `-c` 或配置文件给身份）：

```text
$ cat .git/config
[core]
	repositoryformatversion = 0
	filemode = true
	bare = false
	logallrefupdates = true
```

还没有提交时问 HEAD 要哈希：

```text
$ git rev-parse HEAD   （这条会失败，就为了看 git 的报错措辞）
fatal: ambiguous argument 'HEAD': unknown revision or path not in the working tree.
Use '--' to separate paths from revisions, like this:
'git <command> [<revision>...] -- [<file>...]'
HEAD
```

这条报错的意思不是"HEAD 文件不存在"，而是"`HEAD` 这个 ref 存在，但它指向的分支还没有提交，
所以没法解析成一个版本"。

**常见翻车**

- 把 `.git` 当成"缓存目录"删掉 → 你删掉的是**全部历史**，工作区文件一份不少但所有提交没了。
- 看到 `hooks/` 里 14 个文件就以为"仓库配了检查脚本" → 它们带 `.sample`，一个都没跑。
- 在 Windows/某些编辑器里 `.git` 看不见，就以为"这目录不是仓库" → 它是隐藏目录，用 `ls -a` 或打开"显示隐藏文件"。

### 场景 2：`git status` 的每种措辞，分别指哪个区变了

**什么情况**：`git status` 说 "Changes to be committed"、"Changes not staged"、"Untracked files"，
你想确认"这句话=哪个区变了"，否则你永远在猜下一步该 add 还是该 restore。

**敲哪几条**（一次把 4 种状态全造出来）

```bash
printf '第一行\n第二行\n' > note.txt
printf '*.log\n' > .gitignore
echo '临时日志' > junk.log                 # 被 .gitignore 拦住，status 里根本不出现
git add note.txt .gitignore
git commit -m "提交1"
printf '第一行\n第二行改过了\n' > note.txt  # 工作区 ≠ 暂存区  → modified
echo '新增文件' > todo.txt                 # 工作区有、清单没有 → untracked
git rm --cached .gitignore                 # 暂存区 ≠ 仓库     → deleted
git status
git status -sb
```

**为什么是这几条**：`git rm --cached <file>` 是"从清单里删掉这一行，但工作区文件不动"，
这样仓库里有它、清单里没有它，`status` 就会报告 `deleted`。
配合另外两种（改文件 / 新建文件），三种措辞同时出现，一对比就清楚了。

**真实输出**

```text
$ git status
On branch main
Changes to be committed:
  (use "git restore --staged <file>..." to unstage)
	deleted:    .gitignore

Changes not staged for commit:
  (use "git add <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)
	modified:   note.txt

Untracked files:
  (use "git add <file>..." to include in what will be committed)
	.gitignore
	todo.txt
```

看清楚这段：`.gitignore` **同时出现在两个区里** ——
"要提交的变更：deleted"（清单里删了，仓库里还有）+ "未跟踪的文件"（工作区里那个文件还在磁盘上）。
这不是 bug，正是 `--cached` 的语义：**停止跟踪，但把文件留在你手上**。

短格式一目了然（第一列 = 暂存区 vs 仓库，第二列 = 工作区 vs 暂存区）：

```text
$ git status -sb
## main
D  .gitignore
 M note.txt
?? .gitignore
?? todo.txt
```

`git ls-files` 读的是**暂存区那张清单**，所以 `git rm --cached` 之后它眼里就只剩 `note.txt`：

```text
$ git ls-files
note.txt
```

`add` 之后 `todo.txt` 从 `??` 变成 `A`（进了清单）：

```text
$ git add todo.txt

$ git status -sb
## main
D  .gitignore
 M note.txt
A  todo.txt
?? .gitignore
```

**这里最值得记住的一点**：`??` 和 `A ` 的区别不是"内容变了"，而是**清单里有没有这一行**。
`git restore --staged todo.txt` 把清单里那一行撤掉，`todo.txt` 立刻变回 `??`，
文件内容一个字都没动：

```text
$ git restore --staged todo.txt

$ git status -sb
## main
D  .gitignore
 M note.txt
?? .gitignore
?? todo.txt
```

最后 `add -A` 全收 + commit，三方一致，`status` 就闭嘴了：

```text
$ git -c user.name="学习者" -c user.email="learn@example.com" commit -m "提交2：todo.txt 进仓库，.gitignore 停止跟踪"
[main 8cd504c] 提交2：todo.txt 进仓库，.gitignore 停止跟踪
 2 files changed, 2 insertions(+), 1 deletion(-)
 create mode 100644 todo.txt

$ git status
On branch main
nothing to commit, working tree clean
```

**中英对照**（你手敲时终端很可能是中文，短格式字母不变，只有长格式措辞变）

| 英文（本章输出） | 中文（你机器上可能看到） | 短格式 |
|---|---|---|
| `Changes to be committed` | `要提交的变更：` | 第一列字母 |
| `Changes not staged for commit` | `尚未暂存以备提交的变更：` | 第二列字母 |
| `Untracked files` | `未跟踪的文件:` | `??` |
| `modified` | `修改` | `M` |
| `deleted` | `删除` | `D` |
| `new file` | `新文件` | `A` |
| `nothing to commit, working tree clean` | `无文件要提交，干净的工作区` | 无输出 |

**常见翻车**

- 看到 `modified` 就以为"已经提交了"。**没有**。`modified` 只说明工作区变了，还没进清单。
- 用 `git status` 找被忽略的文件 → 找不到，被忽略的文件默认彻底不显示（要 `git status --ignored`，见 06 章）。
- 以为 `git add` 之后就不能反悔 → `git restore --staged <file>` 就是专门的撤销，文件内容不受影响。

### 场景 3：`git add` 到底往 `.git` 里写了什么

**什么情况**：老师说得把文件"加入暂存区"，但你不知道 `add` 是"记个文件名"还是"复制一份内容"。
这个区别直接决定"你后面又改了文件，`commit` 提交的是哪一份"。

**敲哪几条**

```bash
git ls-files -s                        # 看清单：模式 哈希 stage 文件名
find .git/objects -type f | sort       # add 之前有几个对象
git add note.txt
find .git/objects -type f | sort       # add 之后多了一个
git cat-file -p db887d64...            # 把清单里那串哈希直接 cat 出来
git cat-file -t db887d64...            # git 管它叫 blob
```

**为什么是这几条**：`git ls-files -s` 是唯一能直接看到 index 内容的正规姿势
（index 是二进制，`cat` 会吐乱码）。四列含义：

```
100644  db887d64505f6c3d33446ce117c08b45cfbb0647  0	note.txt
─────  ────────────────────────────────────────  ─  ────────
模式     内容哈希                                  阶段 文件名
```

- **模式**：`100644` = 普通文件（不可执行）、`100755` = 可执行文件、`120000` = 符号链接。
- **阶段 stage**：正常都是 `0`。只有合并冲突时才会出现 `1/2/3`（共同祖先/我方/对方）。

**真实输出**：清单里有三行，每行一个哈希：

```text
$ git ls-files -s
100644 397b4a7624e35fa60563a9c03b1213d93f7b6546 0	.gitignore
100644 923c186c587363ea3dac9770fd14485c86a76e5e 0	note.txt
100644 a45cdf0e1cc615394a769211bfc4eaccd6ac7c6e 0	todo.txt
```

哈希不是编号，是**内容算出来的**，所以顺着它能捞出文件内容本体（这里 cat 的是清单里 `note.txt` 那行的哈希）：

```text
$ git cat-file -p "923c186c587363ea3dac9770fd14485c86a76e5e"
第一行
第二行改过了
```

改文件内容但**先别 add**，objects 里是这些（8 个文件）：

```text
$ printf '第一行\n第二行改过了\n第三行\n' > note.txt

$ find .git/objects -type f | sort
.git/objects/39/7b4a7624e35fa60563a9c03b1213d93f7b6546
.git/objects/8c/d504cd193dfc7bedf6996423f56ffbb834e10b
.git/objects/92/3c186c587363ea3dac9770fd14485c86a76e5e
.git/objects/99/59d0a604b47423b77e808e5d78331af133bb93
.git/objects/a4/5cdf0e1cc615394a769211bfc4eaccd6ac7c6e
.git/objects/c1/76ff8aec10456c1762e82d2601b26ca801b35e
.git/objects/d2/c1cb5f6ad132d2ff0bb3bee13cffec3305d2fa
.git/objects/d9/b286aac9a1b0f3c6dac5a8e43c93ffdd2ee053
```

`git add` 之后**立刻多出一个文件**，而这次提交还没发生：

```text
$ git add note.txt

$ find .git/objects -type f | sort
.git/objects/39/7b4a7624e35fa60563a9c03b1213d93f7b6546
.git/objects/8c/d504cd193dfc7bedf6996423f56ffbb834e10b
.git/objects/92/3c186c587363ea3dac9770fd14485c86a76e5e
.git/objects/99/59d0a604b47423b77e808e5d78331af133bb93
.git/objects/a4/5cdf0e1cc615394a769211bfc4eaccd6ac7c6e
.git/objects/c1/76ff8aec10456c1762e82d2601b26ca801b35e
.git/objects/d2/c1cb5f6ad132d2ff0bb3bee13cffec3305d2fa
.git/objects/d9/b286aac9a1b0f3c6dac5a8e43c93ffdd2ee053
.git/objects/db/887d64505f6c3d33446ce117c08b45cfbb0647

$ git ls-files -s
100644 397b4a7624e35fa60563a9c03b1213d93f7b6546 0	.gitignore
100644 db887d64505f6c3d33446ce117c08b45cfbb0647 0	note.txt
100644 a45cdf0e1cc615394a769211bfc4eaccd6ac7c6e 0	todo.txt
```

前 8 行和 add 之前的清单完全一样，末尾多了 `.git/objects/db/887d6...` —— 这就是新写进去的 blob（9 个 vs 8 个）。
同时清单里 `note.txt` 那行的哈希也从 `923c186...` 换成了 `db887d6...`。

把新哈希 cat 出来，就是工作区那份三行内容；git 管这种对象叫 `blob`（binary large object，人话："一份文件内容"）：

```text
$ git cat-file -p db887d64505f6c3d33446ce117c08b45cfbb0647
第一行
第二行改过了
第三行

$ git cat-file -t db887d64505f6c3d33446ce117c08b45cfbb0647
blob
```

再把同样内容 `add` 一次，哈希一字不变 —— 这就是"内容寻址天然去重"：

```text
$ git add note.txt

$ git rev-parse :note.txt
db887d64505f6c3d33446ce117c08b45cfbb0647
```

**结论（本章最重要的一句话之一）**：
`git add` **不是**打标记，而是**把文件内容复制一份进 objects**。
所以 `add` 之后你再改文件，`commit` 提交的是"上次 add 那一刻的内容"，新改动不会进去
（这就是为什么 `git add` 之后又要改文件时必须再 `add` 一次）。

**加餐：手滑把 `.git/index` 删了会怎样**（demo 里真删了，就在场景 3 末尾）

```text
$ rm .git/index

$ git status -sb
## main
D  .gitignore
D  note.txt
D  todo.txt
?? .gitignore
?? note.txt
?? todo.txt
```

看清楚了：**每个文件都同时"被删除"和"未跟踪"** —— 因为清单没了，git 认为"下次提交将不包含任何文件"，
而工作区的文件全成了野文件。这时候如果你手快 `git commit`，就会提交出一个**空仓库级删除**。
解药是 `git reset`（默认 mixed 模式，只按 HEAD 重建清单，工作区一个字都不动）：

```text
$ git reset
Unstaged changes after reset:
M	note.txt

$ git status -sb
## main
 M note.txt
```

`M note.txt` 回来了，说明"工作区确实和清单不一样"这个真实状态被完整恢复，什么都没丢。

**常见翻车**

- "我 `add` 过了呀，怎么提交的文件还是旧的？" → `add` 之后又改了。再 `git add` 一次，或 `git commit -a`。
- 想直接 `cat .git/index` 看清单 → 二进制，用 `git ls-files -s`。
- 把 `100644` 里的 `644` 当成权限随便改 → 想保留可执行位用 `git update-index --chmod=+x <file>`，别手改清单。
- 看到满屏 `D` + `??` 就以为"我文件全丢了" → 先 `git reset` 重建清单再判断，别急着 commit。

### 场景 4：commit 是快照不是差异

**什么情况**：你担心"每次提交都存一整个项目副本，仓库不会爆炸吗？"
或者反过来，你以为 Git 存的是 patch，所以想不通"为什么 checkout 老版本能直接拿到完整文件"。

**敲哪几条**

```bash
seq 1 9 | sed 's/^/第 /;s/$/ 行/' > big.txt && git add big.txt && git commit -m "提交3"
git cat-file -p HEAD                  # 第 1 层：提交对象
git cat-file -p "HEAD^{tree}"         # 第 2 层：目录清单
git cat-file -p "$(git rev-parse HEAD:big.txt)"   # 第 3 层：文件内容
git count-objects -v                  # 对象总数
sed -i '5s/.*/第 5 行改了/' big.txt && git commit -qam "提交4"   # 只改一行
git count-objects -v                  # 看 count 增加了几个
```

**为什么是这几条**：`cat-file -p` 是"顺着指针往下钻"的显微镜；
`HEAD^{tree}` 意思是"HEAD 这个提交指向的那棵 tree"；
`count-objects -v` 的 `count` 字段就是"objects 里躺着多少个未打包对象"。
改一行前后各看一次，增量一目了然。

**真实输出**：提交对象本身就这五行，不含任何文件内容、也不含任何 diff：

```text
$ git cat-file -p HEAD
tree ee76580b57ea8335715b9f287c3bcfd42c4674d2
parent 8cd504cd193dfc7bedf6996423f56ffbb834e10b
author 学习者 <learn@example.com> 1767232800 +0800
committer 学习者 <learn@example.com> 1767232800 +0800

提交3：加上 9 行的 big.txt
```

注意两个细节：① 时间显示成 `1767232800`（Unix 秒），就是脚本里固定的 2026-01-01 10:00 +0800；
② `parent 8cd504cd...` 就是提交2 的哈希 —— 历史链条 physically 就是这一行串起来的。

那最底下的提交呢？它**连 `parent` 这一行都没有**（`git rev-list --max-parents=0 HEAD` 就是"找出没有父母的那个提交"）：

```text
$ git cat-file -p $(git rev-list --max-parents=0 HEAD)
tree d9b286aac9a1b0f3c6dac5a8e43c93ffdd2ee053
author 学习者 <learn@example.com> 1767232800 +0800
committer 学习者 <learn@example.com> 1767232800 +0800

提交1：note.txt 和 .gitignore 进仓库
```

这也是为什么第一次提交的输出里 git 会专门标注 `(root-commit)`（见场景 2）。

顺着 `tree` 钻第二层 —— 目录清单，一行一个文件，全是"哈希 → 名字"：

```text
$ git cat-file -p "HEAD^{tree}"
100644 blob 397b4a7624e35fa60563a9c03b1213d93f7b6546	.gitignore
100644 blob fe39308095e2c25d5daabfd68267f16c1056e458	big.txt
100644 blob db887d64505f6c3d33446ce117c08b45cfbb0647	note.txt
100644 blob a45cdf0e1cc615394a769211bfc4eaccd6ac7c6e	todo.txt
```

再钻第三层，拿到 `big.txt` 的完整内容本体：

```text
$ git cat-file -p "$(git rev-parse HEAD:big.txt)"
第 1 行
第 2 行
第 3 行
第 4 行
第 5 行
第 6 行
第 7 行
第 8 行
第 9 行
```

链条就是 `commit → tree → blob → 内容`。子目录会多一层 tree（`HEAD^{tree}` 里会出现 `tree xxxx  subdir/`），
道理完全一样。

现在**只改第 5 行**，前后各数一次对象（`find` 数的是散文件个数，`count` 字段是 git 自己数的，两者应当一致）：

```text
$ find .git/objects -type f | wc -l
12

$ git count-objects -v
count: 12
size: 48
in-pack: 0
packs: 0
size-pack: 0
prune-packable: 0
garbage: 0
size-garbage: 0

$ sed -i '5s/.*/第 5 行改了/' big.txt

$ git -c user.name="学习者" -c user.email="learn@example.com" commit -qam "提交4：只改第 5 行"

$ find .git/objects -type f | wc -l
15

$ git count-objects -v
count: 15
size: 60
in-pack: 0
packs: 0
size-pack: 0
prune-packable: 0
garbage: 0
size-garbage: 0
```

12 → 15，**正好多 3 个对象**，而且类型正好是 commit / tree / blob 各一个：

```text
$ git cat-file -t HEAD
commit

$ git cat-file -t "HEAD^{tree}"
tree

$ git cat-file -t HEAD:big.txt
blob
```

老对象一点没变大 —— 改之前那版 `big.txt` 90 字节，改之后 96 字节（就差几个汉字）：

```text
$ git cat-file -s HEAD~1:big.txt
90

$ git cat-file -s HEAD:big.txt
96
```

那"差异"从哪来？git 现算的，不是存的：

```text
$ git diff --stat HEAD~1 HEAD
 big.txt | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)
```

**终极证据**：把第 5 行**原样改回去**再提交一次。两次提交里 `big.txt` 的哈希完全相同，
说明一个"内容对象"都没新增；`count` 只从 15 涨到 16（只多一个 commit，连 tree 都复用了提交3 的那棵，
因为"文件名 → 哈希"这张清单的内容又变得和提交3 一模一样了）：

```text
$ sed -i '5s/.*/第 5 行/' big.txt

$ git -c user.name="学习者" -c user.email="learn@example.com" commit -qam "提交5：改回去了"

$ git rev-parse HEAD~2:big.txt
fe39308095e2c25d5daabfd68267f16c1056e458

$ git rev-parse HEAD:big.txt
fe39308095e2c25d5daabfd68267f16c1056e458

$ git count-objects -v
count: 16
size: 64
in-pack: 0
packs: 0
size-pack: 0
prune-packable: 0
garbage: 0
size-garbage: 0
```

**所以正确的心智模型是**：

- Git 存的是**每个提交时刻的全量快照**（内容层面没有 patch）。
- 但"快照"靠**共享未变动的 blob** 实现，所以只改一个文件时，仓库只多这一个文件的内容 + 两三个几十到几百字节的小对象。
- 真正省空间的打包（`git gc` 里的 packfile 才做增量压缩）是后话，不影响你的心智模型。

**常见翻车**

- 以为"Git 存 diff，所以删掉旧文件历史就没了" → 老 blob 仍在 objects 里，`git cat-file -p <老哈希>` 照样能捞出来。
- 用 `git diff A B` 以为是"读取存好的 patch"，其实每次都是现场对比两个快照算出来的。
- 以为"改一行 = 整个文件重新存一遍，仓库会迅速膨胀" → 只对**那一个文件**重存一份，其他文件全复用。

### 场景 5：分支是指针，HEAD 是"指针的指针"

**什么情况**：你切了 100 次分支，每次 0.01 秒；你不知道 Git 凭什么这么快。

**敲哪几条**

```bash
git branch --show-current
grep '' .git/refs/heads/*        # ★ 分支就是文本文件：直接把内容打出来（多文件时 grep 会带上文件名）
cat .git/HEAD
git branch experiment            # 建分支
ls .git/refs/heads               # 多了一个文件
git checkout experiment          # 切分支
cat .git/HEAD                    # 只有 HEAD 这一行变了
git add exp.txt
git commit -m "experiment 上的提交"
grep '' .git/refs/heads/*        # 只有 experiment 那行哈希变了
```

**真实输出**

```text
$ git branch --show-current
main

$ grep '' .git/refs/heads/*
125210120aefce636cb496eb0ab658b3be5fd9ed

$ cat .git/HEAD
ref: refs/heads/main
```

（只有一个分支文件时 `grep` 不加文件名前缀；下面有两个分支文件时就带上了 —— 这样更好读。）

建分支就是"写一个新文本文件，内容 = 当前提交的哈希"，两个分支文件此刻**内容完全相同**：

```text
$ git branch experiment

$ ls .git/refs/heads
experiment
main

$ grep '' .git/refs/heads/*
.git/refs/heads/experiment:125210120aefce636cb496eb0ab658b3be5fd9ed
.git/refs/heads/main:125210120aefce636cb496eb0ab658b3be5fd9ed
```

切分支只改 `HEAD` 那一行字符串（工作区文件会被换成那个提交的内容，但 ref 层面只有这一处变化）：

```text
$ git checkout experiment
Switched to branch 'experiment'

$ cat .git/HEAD
ref: refs/heads/experiment
```

在新分支上提交之后 —— 被改写的字符串只有 `experiment` 那一个文件，`main` 一动没动，`HEAD` 也一个字没变：

```text
$ echo 'experiment 专属' > exp.txt

$ git add exp.txt

$ git -c user.name="学习者" -c user.email="learn@example.com" commit -m "experiment 上的提交"
[experiment 1472bd8] experiment 上的提交
 1 file changed, 1 insertion(+)
 create mode 100644 exp.txt

$ grep '' .git/refs/heads/*
.git/refs/heads/experiment:1472bd8b74f54673471df5d9401f6aa2ce6be7bb
.git/refs/heads/main:125210120aefce636cb496eb0ab658b3be5fd9ed

$ cat .git/HEAD
ref: refs/heads/experiment
```

这就是全部机制。画成图（**"提交"就是同时造一个 commit 对象并把当前分支文件里的哈希改成它**）：

```
   提交前                                    提交后
   HEAD ──→ ref: refs/heads/experiment       HEAD ──→ ref: refs/heads/experiment
                                              （HEAD 一个字没改）
   experiment → 1252101  ──┐                  experiment → 1472bd8 ──┐ ← 只改了这一行
                           ↓                                        ↓
   main → 1252101 ──→ commit 1252101        main → 1252101 ──→ commit 1252101
                            │                                        │  ↑ parent 指回上一个
                            ↓                                        ↓
                        tree/blob                                  tree/blob（老的全复用）
                                                                     ↑
                                                        新 commit 1472bd8 → 新 tree → 新 blob
```

历史为什么是一条链？因为 commit 对象里有 `parent` 那一行；分支只是一个"入口书签"。
上面那段 `--graph` 输出就是这么来的：

```text
$ git log --oneline --decorate --graph --all
* 1472bd8 (HEAD -> experiment) experiment 上的提交
* 1252101 (main) 提交5：改回去了
* df066d9 提交4：只改第 5 行
* b6c8fd4 提交3：加上 9 行的 big.txt
* 8cd504c 提交2：todo.txt 进仓库，.gitignore 停止跟踪
* c176ff8 提交1：note.txt 和 .gitignore 进仓库
```

`(main)`、`(HEAD -> experiment)` 这些括号里的名字就是"哪个指针指在这个提交上"。

**常见翻车**

- 以为"分支里存着这个分支的文件副本" → 分支只是一个哈希指针，文件全在共享的 `objects/` 里。
- 以为"切分支要把整个目录重写一遍，很大项目要很久" → 只有被改过的文件才会被重写，所以是毫秒级。
- `git branch` 后面不接 `-a`/`--show-current`，看到"什么都不打印"以为失败了 → 裸 `git branch` 是列出本地分支并标 `*`（本章用 `--show-current` 更直白）。

### 场景 6：detached HEAD —— 你此刻"不在任何分支上"

**什么情况**：想看老代码 / 按教程 checkout 了一个哈希，git 开始长篇警告 `detached HEAD`，你怕一动就丢东西。

**敲哪几条**

```bash
git checkout "$(git rev-parse --short HEAD~1)"   # ★ 给哈希，不给分支名
cat .git/HEAD                                    # 里面变成裸哈希
git status                                       # 明说 HEAD detached at xxx
git add ... && git commit -m "游离提交"
git checkout experiment                          # 切走 → git 警告 "leaving 1 commit behind"
git branch --contains <那个哈希>                  # 空白 = 没有任何分支包含它
git reflog                                       # 找回哈希
git branch rescue <那个哈希>                      # 抢救：给它建分支
```

**为什么**：正常状态下 `HEAD` 写的是"去读哪个分支文件"，提交时会顺手更新那个分支文件。
detached 时 `HEAD` 里是**裸哈希**，没有分支文件可更新，于是新提交只被 `HEAD` 临时指着；
你一 Switch 走，`HEAD` 换内容，那个提交就没人指了 —— 术语叫"游离/orphan"，
但它**没被删**，在 `reflog` 里能捞 30~90 天。

**真实输出**（`git checkout <哈希>` 的完整警告，这段就是"人话解释"）

```text
$ git checkout 1252101
Note: switching to '1252101'.

You are in 'detached HEAD' state. You can look around, make experimental
changes and commit them, and you can discard any commits you make in this
state without impacting any branches by switching back to a branch.

If you want to create a new branch to retain commits you create, you may
do so (now or later) by using -c with the switch command. Example:

  git switch -c <new-branch-name>

Or undo this operation with:

  git switch -

Turn off this advice by setting config variable advice.detachedHead to false

HEAD is now at 1252101 提交5：改回去了
```

铁证 —— `HEAD` 里面不再是 `ref: `，而是裸哈希：

```text
$ cat .git/HEAD
125210120aefce636cb496eb0ab658b3be5fd9ed

$ git status
HEAD detached at 1252101
nothing to commit, working tree clean
```

（中文环境下这句是"您正处于分离头指针状态" / "HEAD 目前分离于 1252101"。）

在游离状态下提交，git 甚至把状态名写给你看（`[detached HEAD 84ecdc2]`）：

```text
$ git -c user.name="学习者" -c user.email="learn@example.com" commit -m "游离提交：没有任何分支指向它"
[detached HEAD 84ecdc2] 游离提交：没有任何分支指向它
 1 file changed, 1 insertion(+)
 create mode 100644 loose.txt
```

分支文件里的哈希**一点没动**，`HEAD` 里却是那个新提交的哈希（这就是"没人指它"的直接证据）：

```text
$ grep '' .git/refs/heads/*
.git/refs/heads/experiment:1472bd8b74f54673471df5d9401f6aa2ce6be7bb
.git/refs/heads/main:125210120aefce636cb496eb0ab658b3be5fd9ed

$ cat .git/HEAD
84ecdc21bc1744eaae77d80cce31c2bc4942772b
```

切回分支时 git 明确告诉你丢了什么，还顺手给了抢救命令：

```text
$ git checkout experiment
Warning: you are leaving 1 commit behind, not connected to
any of your branches:

  84ecdc2 游离提交：没有任何分支指向它

If you want to keep it by creating a new branch, this may be a good time
to do so with:

 git branch <new-branch-name> 84ecdc2

Switched to branch 'experiment'

$ git log --oneline -1
1472bd8 experiment 上的提交

$ git branch --contains 84ecdc21bc1744eaae77d80cce31c2bc4942772b
```

上面这条 `git branch --contains` **一行输出都没有** —— 一个分支都不包含它。
再看全仓库：`git log --all` 里也没有 `84ecdc2`（它在 `log` 意义上已经"不存在"了，只是磁盘上的对象还在）：

```text
$ git log --oneline --all
1472bd8 experiment 上的提交
1252101 提交5：改回去了
df066d9 提交4：只改第 5 行
b6c8fd4 提交3：加上 9 行的 big.txt
8cd504c 提交2：todo.txt 进仓库，.gitignore 停止跟踪
c176ff8 提交1：note.txt 和 .gitignore 进仓库
```

自救第 1 步，`reflog`（HEAD 的行程记录，`git checkout`、`git commit`、`git reset` 都会往里写）：

```text
$ git reflog
1472bd8 HEAD@{0}: checkout: moving from 84ecdc21bc1744eaae77d80cce31c2bc4942772b to experiment
84ecdc2 HEAD@{1}: commit: 游离提交：没有任何分支指向它
1252101 HEAD@{2}: checkout: moving from experiment to 1252101
1472bd8 HEAD@{3}: commit: experiment 上的提交
1252101 HEAD@{4}: checkout: moving from main to experiment
1252101 HEAD@{5}: commit: 提交5：改回去了
df066d9 HEAD@{6}: commit: 提交4：只改第 5 行
b6c8fd4 HEAD@{7}: commit: 提交3：加上 9 行的 big.txt
8cd504c HEAD@{8}: reset: moving to HEAD
8cd504c HEAD@{9}: commit: 提交2：todo.txt 进仓库，.gitignore 停止跟踪
c176ff8 HEAD@{10}: commit (initial): 提交1：note.txt 和 .gitignore 进仓库
```

`HEAD@{1}` 那行就是游离提交。自救第 2 步，用哈希建分支把它"钉住"（两种方式都能拿到那个哈希）：

```text
$ git branch rescue 84ecdc21bc1744eaae77d80cce31c2bc4942772b

$ git log --oneline -1 rescue
84ecdc2 游离提交：没有任何分支指向它

$ git branch --contains 84ecdc21bc1744eaae77d80cce31c2bc4942772b
  rescue

$ git rev-parse 'HEAD@{1}'
84ecdc21bc1744eaae77d80cce31c2bc4942772b

$ git branch rescue2 'HEAD@{1}'

$ git log --oneline -1 rescue2
84ecdc2 游离提交：没有任何分支指向它
```

从此这个提交有名字了，不会再丢。

**常见翻车**

- 在 detached 状态做了有用的工作，直接 `git checkout main` 走人 → 提交还在 reflog 里（默认 90 天），但**如果你不救、又跑过 `git gc --prune`，它就真没了**。正确姿势：立刻 `git switch -c 临时分支名`。
- 以为"游离 = 仓库坏了" → 它只是"你站在一个提交上而不是站在一个分支上"，看代码、做实验完全安全。
- 想关掉那一大段警告：`git config --local advice.detachedHead false`（只写 `.git/config`，别加 `--global`）。

---

## 命令速查表

| 命令 | 干什么 | 例子 |
|---|---|---|
| `git init -b main` | 建仓库并指定初始分支名 | `git init -b main myproj` |
| `git status` / `status -sb` | 报告三个区哪里不一致（`-b` 多一行分支信息） | `git status -sb` |
| `git ls-files -s` | 读暂存区清单：模式/哈希/stage/文件名 | `git ls-files -s src/` |
| `git add <file>` | 把工作区内容复制成 blob 并写进清单 | `git add note.txt` |
| `git restore --staged <file>` | 把清单里那条撤回，文件不动 | `git restore --staged todo.txt` |
| `git restore <file>` | 用清单里的内容覆盖工作区（丢弃未 add 的改动） | `git restore note.txt` |
| `git commit -m "..."` | 把清单装订成永久档案 | `git commit -m "fix: 修好空指针"` |
| `git cat-file -p <ref或哈希>` | 打印对象原文（顺着指针钻层） | `git cat-file -p HEAD` |
| `git cat-file -t <哈希>` | 只看对象类型：commit / tree / blob | `git cat-file -t HEAD` |
| `git cat-file -s <哈希>` | 看对象 uncompressed 大小（字节） | `git cat-file -s "HEAD:big.txt"` |
| `git rev-parse <东西>` | 把任何写法解析成完整哈希 | `git rev-parse HEAD~1`、`git rev-parse :note.txt` |
| `git rev-parse --short HEAD` | 只要短哈希 | `git checkout $(git rev-parse --short HEAD~1)` |
| `git count-objects -v` | 数对象个数/体积（`count` 字段最常用） | `git count-objects -v \| head -3` |
| `git log --oneline --decorate --graph --all` | 一条命令看全所有分支历史 | 建议起别名 `git lg` |
| `git branch <名字>` | 建分支 = 写一个 ref 文本文件 | `git branch experiment` |
| `git branch --show-current` | 只打印当前分支名（detached 时打印空） | `git branch --show-current` |
| `git branch --contains <哈希>` | 问哪些分支包含这个提交 | `git branch --contains 84ecdc2` |
| `git checkout <分支名>` | 改 HEAD 那一行 + 同步工作区 | `git checkout experiment` |
| `git checkout <哈希>` | 进入 detached HEAD | `git checkout 1252101` |
| `git reflog` | HEAD 的行程日志（救命的东西） | `git reflog \| head` |
| `git log -g --oneline` | 把 reflog 当 log 看 | `git log -g --oneline` |

---

## 这章的坑

1. **`git add` 是复制内容，不是打标记**。add 之后再改文件，`commit` 用的还是 add 那一刻的内容；
   忘了再 add 就会提交到"半个版本"。查法：`git diff`（工作区 vs 清单）还有输出就说明你漏了。
2. **`status` 的 `modified` ≠ 已提交**。它是"工作区 vs 暂存区"的结果；
   真正的"已提交"是 `git log` 里看得见。看到 ` M` 就以为安全了，是最常见的自我安慰。
3. **`.git/index` 不是文本，别 `cat`**。真手滑删了它（`rm .git/index`），`git status` 会当场骗你：
   所有文件同时显示成 `D`（已删除）+ `??`（未跟踪）。**这时候千万别 commit**，
   先 `git reset`（默认 mixed，只按 HEAD 重建清单，工作区不动），场景 3 的"加餐"里有完整实测。
4. **`HEAD` 里出现裸哈希不是"仓库坏了"**，是你 detached 了。但**别在 detached 下长期干活**：
   一 Switch 走，提交就没人指了（30~90 天后 `git gc` 会真删）。
5. **commit 对象里没有 diff，也没有文件名列表**。它只指向一棵 tree。
   所以"提交说明写清改了什么"很重要 —— git 不会替你记住意图。
6. **分支不是容器，是指针**。`git branch feature` 不会复制任何文件；
   删分支（`git branch -d`）只删那个文本文件，里面的提交对象往往还活着并被别的分支共享。
7. **`git init` 的初始分支名不一致会恶心你**：老 git 默认 `master`，新 git 默认 `main`，
   还取决于别人的 `init.defaultBranch` 配置。养成习惯：`git init -b main`，或克隆后先看 `git branch --show-current`。
8. **短格式第一列/第二列容易看反**。` M` 前面那个空格是"暂存区和仓库一致"，别当噪音删掉；
   复制 status 给别人看时**保持对齐**，空格本身就是信息。

---

## 自检问题

1. `git add note.txt` 之后我又改了 `note.txt`，然后 `git commit`。提交的是哪个版本？怎么在提交前确认？
   **答**：提交的是**上次 add 那一刻**的内容。确认方法：`git status -sb` 出现 `MM`，或 `git diff`（工作区 vs 清单）还有输出。补救：再 `git add` 一次。

2. `git rm --cached xxx` 之后，`git status` 里为什么会出现两次 `xxx`？
   **答**：一次是"要提交的变更：deleted"（清单里删了、仓库里还有），一次是"未跟踪的文件"（文件仍在磁盘上，但没人跟踪它）。这正是"停止跟踪但保留本地文件"的完整含义。

3. 一个仓库里两个提交的 `big.txt` 只差一行，objects 里多了几个对象？
   **答**：3 个 —— 新的 blob（变更的那份文件内容）+ 新的 tree（目录清单里那行的哈希变了）+ 新的 commit。其余未变文件的 blob 全部复用。

4. 把某文件改回和历史上某次提交完全一致的内容，再提交一次，会产生新 blob 吗？
   **答**：不会。内容一致 → 哈希一致 → 复用旧 blob（本章实测：`count` 只从 15 涨到 16，只多一个 commit 对象）。

5. `cat .git/HEAD` 得到 `ref: refs/heads/main` 和得到 `1252101...` 分别意味着什么？
   **答**：前者在分支上（HEAD → 分支文件 → 提交）；后者 detached，HEAD 直接指向提交，此时提交不会推进任何分支。

6. 你 `git checkout <哈希>` 看了老代码，改了半天还提交了，然后 `git checkout main`。工作白干了吗？怎么救？
   **答**：暂时没丢。`git reflog` 找到那条 `commit:` 记录（或用 `HEAD@{1}`），`git branch 保住它 <哈希>` 立刻钉住，之后想 merge/cherry-pick 再操作。

7. 为什么切分支几乎是瞬间完成的？
   **答**：切分支只改 `HEAD` 文件里的一行字符串，再按差异把工作区文件替换成目标提交的内容；文件内容本来就都在共享的 `objects/` 里，不需要搬运整个项目。

---

## 运行 demo

```bash
cd git-handbook/01-mental-model
bash demo.sh          # 全跑一遍 + 自动清理沙箱（输出约 590 行，边看边对照 README 约 20 分钟）
bash demo.sh --keep   # 跑完保留沙箱，并把路径打印给你，可以进去自己敲命令
```

脚本特性（三章的 demo.sh 都是同一套约定）：

- `set -euo pipefail`：任何一步真失败就立刻停下，不假装成功。
- 自己在 `mktemp -d` 出来的 `/tmp/git01-XXXXXX` 里 `git init`，**绝不碰你机器上的真实仓库**。
- 退出时 `trap` 自动清理沙箱；`--keep` 则打印沙箱路径并保留。
- 在任何目录下运行都可以（脚本内部一律用绝对路径）。
- 固定 `GIT_AUTHOR_DATE` / `GIT_COMMITTER_DATE` = `2026-01-01T10:00:00 +0800`，
  提交统一用 `git -c user.name="学习者" -c user.email="learn@example.com"`，
  所以**哈希可复现**（本章连跑两次输出逐字节一致）。
- 设置 `GIT_CONFIG_GLOBAL=/dev/null`、`GIT_CONFIG_SYSTEM=/dev/null`：
  不读也不写你的 `~/.gitconfig`，同时保证输出不受你的个人配置影响。
- 设置 `LC_ALL=C.UTF-8`：git 提示语固定英文。你手敲时想切回中文，把 `LANG=zh_CN.UTF-8` 用上去即可，
  本章的"中英对照"表覆盖了这个差异。

`--keep` 模式结尾长这样：

```text
# 这一小段来自 `bash demo.sh --keep` 那一次运行，所以沙箱名和上面不一样
[沙箱已保留] /tmp/git01-ecSnkH
  进去自己敲： cd "/tmp/git01-ecSnkH/demo" && git status
  看完自己删： rm -rf "/tmp/git01-ecSnkH"
```

进去之后推荐自己敲这几条，把本章再走一遍：

```bash
cat .git/HEAD
for r in .git/refs/heads/*; do echo "$(basename $r) -> $(cat $r)"; done
git cat-file -p HEAD
git ls-files -s
git count-objects -v
git reflog
```

---

## 下一步

- `02-daily-commit`：把这三个区跑成日常闭环（add / commit / log / diff / rm / .gitignore）。
- 之后是 `03-branch-and-merge`（分支与合并）、`06-stash-and-ignore`（被打断时怎么救急）。
- 这一章的图 1/图 2 请抄到便利贴上贴在显示器边上，比背命令有用。
