# 10 故障现场与救援（troubleshooting / recovery）

> 一句话：这一章不教新命令，教**出事之后的第一反应** ——
> 先取证（报错原文、`git status`、退出码都还在的时候抄下来），再判断该动哪一个，
> 最后每一步修复都要留下证据。八个现场全部**先弄坏再救回来**，一个现场都没跳过。

前置：

- 01 章：分支就是一个写着哈希的文本文件、`HEAD` 也是一个文件、工作区 / 暂存区 / 仓库三区模型；
- 02 章：`git status --short` 的两格符号（第 1 格 = 索引，第 2 格 = 工作区）；
- 03 章：`reset` / `restore` / `reflog` 的基本形状（本章把它们的**救火边界**讲透）；
- 04 章：merge 冲突标记怎么读；
- 05 章：rebase 里的 `--ours` / `--theirs` 方向（本章第 5 节末尾会再撞一次）；
- 06 章：`.gitignore` 的作用范围（本章第 8 节证明它对已跟踪文件完全无效）；
- 08 章：`git cat-file` / `git fsck`（本章第 4、6 节的取证全靠这两条）。

本章最值钱的八个认知，后面每一节都是在给它们按手印：

1. **提交几乎不会真的丢。** `git branch -D`、游离 HEAD 切走，删的都只是「引用」，对象还 in 库里。
   先 `git reflog`，再 `git fsck --no-reflogs`，两条路指着同一个对象库（第 1、4 节实测：同一个 `a798587`）。
2. **真的丢只有一种：从没 `git add` 过。** 磁盘上写过、从没进过索引的内容，reflog 和 fsck 都救不了（第 6F 节）。
3. **`git status` 本身不是状态机，报错也不是。** 同一把锁卡住的四条命令，报错文本只有 `fatal:` / `error:` 之差，
   说的是同一件事；反过来 `git fsck` 明明报出 dangling 对象，退出码还是 0（第 2、4 节）。
4. **只读命令还能跑 = 仓库没坏。** 被锁住时 `status` / `log` / `diff` 全部正常，就别往 fsck、重新 clone 那方向想（第 2B 节）。
5. **动 `.git` 只允许动 `*.lock` 这一类文件。** `rm -rf .git`、删 `.git/index` 本体都不在救援菜单里（第 2 节三条铁律）。
6. **身份有两种坏法：一种是罢工，一种是默默把本机登录名写进历史。** 后者不报错，最阴（第 3E 节实测）。
7. **`--staged` 这一个词决定动索引还是动工作区。** `restore --staged` 可逆，`restore` 直接把磁盘盖掉（第 6 节两次丢失、两次救援）。
8. **`.gitignore` 只管未跟踪文件；钩子只在本地且不随 clone 传播。**
   两者的「为什么没生效」都是同一个答案：**它管的不是你以为的那个东西**（第 7、8 节）。

> **引用约定**（先看清，后面每一段都遵守）
> ① 本文所有终端输出来自仓库里 `bash demo.sh` 的**一次真实运行**（9 节、1093 行、脚本退出码 0），
> 原文存在 `~/.cache/handbook-captures/git10.txt`；每个终端块的每一行都能在那份文件里逐字找到。
> ② 本次沙箱是 `/tmp/git-handbook-10-NWS75IIG`，本文一律写成 **`<沙箱>`**；你重跑后缀必然不同，其余行不受影响。
> ③ **脱敏两处**：`git var` / `git commit` 找不到身份时，git 会把「本机登录名@本机主机名」写进最后一行；
> 第 3E 节 git 还会把登录名当作者名写进提交。这两条命令的输出里，只把登录名 / 主机名换成
> `<本机登录名>` `<本机主机名>` 两个占位符，**其余逐行原文**。你在自己机器上跑，看到的就是你自己机器的真值。
> ④ 凡是 `[退出码 N]  这段失败是本章故意演的` 这一行，是脚本在告诉你「下面这段红字是现场，不是演示跑挂了」。
> ⑤ 提交身份与时间戳在脚本里固定（身份只走命令行 `-c`，时钟从 `2026-01-01T09:03` 起每笔 +3 分钟），
> 所以**本文出现的每一个哈希你都能复现**；每次会变的只有沙箱绝对路径和文件的 mtime。
> ⑥ 三反引号围栏块 = 真终端输出；缩进 4 空格的块 = 作者手绘示意图，别拿示意图当证据。

---

## 你会遇到的场景

| 你遇到的事 | 本章哪一节 |
|---|---|
| 直接 checkout 到历史提交上改了两笔，切回 main 之后提交不见了 | 第 1 节（事故 + reflog 取证 + 一条 `git branch` 救回） |
| `git add` 报 `Unable to create '.../.git/index.lock': File exists.`，之后什么都提交不了 | 第 2 节（四段同义报错 + 敢不敢删的判断题） |
| 新机器 / CI 里 `git commit` 给你一整屏 `*** Please tell me who you are.` | 第 3 节（报错逐行拆 + 命令行 `-c` 就地救火） |
| 提交成功了，但作者名是本机登录名 | 第 3E、3F 节（`--amend --reset-author`） |
| `git branch -D` 手滑删掉唯一知道那两笔提交的分支，哈希也没记 | 第 4 节（reflog 路线 + fsck 路线 + 什么时候真救不回来） |
| merge 撞冲突，改完文件 `merge --continue` 还是不让提交 | 第 5 节（三段式 status、三个版本、`--ours/--theirs`、`--abort`、add 才叫解决） |
| 想取消 `git add`，漏敲 `--staged`，工作区那版真的没了 | 第 6 节（两次丢失、dangling blob 救援、三兄弟对照） |
| 团队装了 pre-commit 钩子，有人提交成功了 / 有人不知道被谁拒了 | 第 7 节（权限位、`git hook list`、`--no-verify` 的代价） |
| 仓库越来越大、clone 越来越慢；补了 `.gitignore` 结果什么用都没有 | 第 8 节（`git rm -r --cached` + gc 只整理不删历史） |
| 想知道「出事之后第一条命令该敲什么」 | 第 9 节速查表 |

---

## 心智模型

### 图 1：一次提交要经过的四层，出事就出在其中一层的「引用」断了

```
   工作区              索引                 对象库               引用
 (你眼前的文件)   (git add 的结果)      (git commit 的结果)   (谁指着哪个对象)
      │               │                     ▲                  │
      │  git add      │  git commit         │                  │
      ├──────────────►├────────────────────►│                  │
      │               │                     └────── 引用 ──────┤
      │               │                                       │
   restore        restore --staged                    分支 refs/heads/*
   checkout --                                        HEAD、MERGE_HEAD
   （动工作区）  （动索引）                             ORIG_HEAD、reflog
```

四个格子对应四种「丢了」：

| 丢的是什么 | 谁还能救 | 本章哪一节 |
|---|---|---|
| 工作区的文件内容（且从没 `add` 过） | **谁都救不了** | 6F |
| 工作区的文件内容（`add` 过哪怕一秒） | `git fsck --no-reflogs` 找 dangling blob | 6D、6E |
| 索引里的暂存状态 | 重新 `git add` 就行 | 6B |
| 分支引用（提交本身还在库里） | `git reflog` / `git fsck` 找 dangling commit | 1、4 |
| 对象本体 | 只有 `git prune` / `reflog expire` 会真删它 | 4G |

### 图 2：「删除」在 git 里其实有三档

```
第 1 档  删引用    git branch -D / 切走游离 HEAD     → 对象还在，看不见而已（能救，本章 1、4 节）
第 2 档  删回收站  git gc / git prune / reflog expire → 默认还有 90 天 / 2 周缓冲（救场期间禁止手贱）
第 3 档  真删      第 1 档 + 第 2 档都做完了           → cat-file 直接报 could not get object info（4G 实测）
```

### 表：报错的前缀告诉你「谁在说话」

| 前缀 | 谁在说话 | 退出码（本章实测） |
|---|---|---|
| `fatal:` | git 主流程，直接中断 | 128（`git add` 被锁、`symbolic-ref`、`cat-file` 找不到对象） |
| `error:` | git 的子命令失败了，但流程还能走 | 1（`git stash push` 被锁、`git branch -d` 护栏、`merge --continue`） |
| `warning:` / `hint:` | 只是提醒，不影响成败 | 0（或跟着上一条走，如 `git hook list` 的 `warning:` 退 1） |
| `pre-commit:` | **不是 git**，是你或团队自己写的那个钩子脚本 | 1（钩子返回非 0，git 只转述） |
| `gmake:` / `sed:` / `awk:` | 不是你 bash 命令行里串进来的东西 | —— |

这张表是本草最实用的一条：**先认是谁在说话，再决定读哪一段**。第 7C 节就是靠它把「被谁拒了」一眼定死。

---

## 第 0 节 开工前先确认这台机器：版本 + git 到底在读哪份配置

救援之前先弄清两件事：**这个 git 是哪一版**（很多报错文本和护栏是版本相关的），
**它此刻在读哪些配置**（决定「为什么我配了还不生效」以及「这条 `git config` 会不会写到你机器上」）。

```
$ git --version
git version 2.55.0

$ echo 'HOME='$HOME && ls -1A "$HOME" | tr '\n' ' ' && echo '(这一栏空 = 假家目录里没有 .gitconfig，你的那份也不在这里)'
HOME=/tmp/git-handbook-10-NWS75IIG/fakehome
(这一栏空 = 假家目录里没有 .gitconfig，你的那份也不在这里)

$ git config --list --show-origin | sed -n '1,6p' ; echo '(上面一行都没有 = 此刻 git 读不到任何用户级配置)'
(上面一行都没有 = 此刻 git 读不到任何用户级配置)
```

> 上面第二条是本章安全前提的证据：demo 把 `HOME` 改到沙箱里的 `fakehome/`、系统级配置指向 `/dev/null`，
> 所以它**既读不到你真实的 `~/.gitconfig`，也没有任何东西能写进去**。第 3 节整节建立在这上面。

`git var GIT_COMMITTER_IDENT` 是「问 git 你自己是谁」，也是第 3 节的第一条取证命令：

```
$ git var GIT_COMMITTER_IDENT
Committer identity unknown

*** Please tell me who you are.

Run

  git config --global user.email "you@example.com"
  git config --global user.name "Your Name"

to set your account's default identity.
Omit --global to set the identity only in this repository.

fatal: unable to auto-detect email address (got '<本机登录名>@<本机主机名>.(none)')
[退出码 128]  这段失败是本章故意演的，上面几行是 git 打印的原文
```

三条要点：

- `--show-origin` 是本章唯一能一眼看清「哪份文件在起作用」的命令，日常排查配置问题就靠它；
- 打印出来的命令就是真实执行的命令：那两段 `-c user.name=... -c user.email=...` 原样留在上面，
  **身份只来自命令行、没进任何配置文件**（这是第 3C 节救援手法的前提）；
- 版本 2.55.0 这个事实下面会用到两次：第 7A 节那两行「钩子没跑」的 `hint:` 是新版本才有的好心，
  第 4A 节的 `hint: Disable this message with "git config set advice.forceDeleteBranch false"` 同理。

---

## 第 1 节 游离 HEAD：切走之后，那两笔提交从所有分支上消失了

**事故**：为了赶线上问题，直接 `checkout` 到某个历史提交上改代码，连提两笔，
然后 `git switch main` 去干别的活 —— 那两笔提交不属于任何分支，看起来人间蒸发。

### 1A 事故是怎么开始的：直接踩到一个历史提交上

先把舞台搭好（`main` 上三笔正常提交）：

```
$ git log --oneline && echo '(上面是 main 的三笔提交)'
1a61fee feat: 支持退款
61f450b feat: 接入折扣系数
92a1ea0 init: 订单服务骨架
(上面是 main 的三笔提交)
```

然后 `--detach` 下去：

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid switch --detach HEAD~1 && echo '(这就是游离 HEAD：.git/HEAD 里写的不是分支名，而是裸 hash)'
HEAD is now at 61f450b feat: 接入折扣系数
(这就是游离 HEAD：.git/HEAD 里写的不是分支名，而是裸 hash)

$ git status --short --branch && git branch --show-current && echo '(这一栏空 = 当前不在任何分支上)'
## HEAD (no branch)
(这一栏空 = 当前不在任何分支上)

$ cat .git/HEAD && git rev-parse HEAD && git rev-parse --abbrev-ref HEAD
61f450b19044ecf2fd4fbb9f980201de35c3c054
61f450b19044ecf2fd4fbb9f980201de35c3c054
HEAD

$ git symbolic-ref HEAD
fatal: ref HEAD is not a symbolic ref
[退出码 128]  这段失败是本章故意演的，上面几行是 git 打印的原文

$ git -c user.name=学习者 -c user.email=learn@example.invalid branch
* (HEAD detached at 61f450b)
  main
```

判据有四条，从严到松，记住第一条和最后一条就够：

| 判据 | 正常在分支上 | 游离 HEAD |
|---|---|---|
| `git symbolic-ref HEAD` | 打印 `refs/heads/main` | **`fatal: ref HEAD is not a symbolic ref`（退 128）** |
| `cat .git/HEAD` | 一行 `ref: refs/heads/main` | 一行裸哈希 `61f450b19044...` |
| `git status -sb` 第一行 | `## main` | `## HEAD (no branch)` |
| `git branch` | `* main` | `* (HEAD detached at 61f450b)` |

`git rev-parse --abbrev-ref HEAD` 那条也给答案，但它给的是「`HEAD`」这个字符串本身 —— 不报错，
所以只当旁证；要一锤定音用 `symbolic-ref`。

### 1B 在游离状态上真的做了两笔提交

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid add shop.py && git -c user.name=学习者 -c user.email=learn@example.invalid commit -m 'fix: 线上紧急把折扣归零（第一笔）' && git log --oneline -2
[detached HEAD 6a39c05] fix: 线上紧急把折扣归零（第一笔）
 1 file changed, 1 insertion(+), 1 deletion(-)
6a39c05 fix: 线上紧急把折扣归零（第一笔）
61f450b feat: 接入折扣系数

$ git -c user.name=学习者 -c user.email=learn@example.invalid add hotfix.md && git -c user.name=学习者 -c user.email=learn@example.invalid commit -m 'docs: 热修说明（第二笔）' && git log --oneline -3
[detached HEAD 6226659] docs: 热修说明（第二笔）
 1 file changed, 1 insertion(+)
 create mode 100644 hotfix.md
6226659 docs: 热修说明（第二笔）
6a39c05 fix: 线上紧急把折扣归零（第一笔）
61f450b feat: 接入折扣系数
```

两笔都真成功了，`[detached HEAD 6a39c05]` 那方括号里就是线索：**git 一直告诉你它没挂在分支上**。
提交照样进对象库，父链也接得好好的 —— 缺的只有一个「谁指着它」。

### 1C 事故真正发生的时刻：切回 main

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid switch main
Warning: you are leaving 2 commits behind, not connected to
any of your branches:

  6226659 docs: 热修说明（第二笔）
  6a39c05 fix: 线上紧急把折扣归零（第一笔）

If you want to keep them by creating a new branch, this may be a good time
to do so with:

 git branch <new-branch-name> 6226659

Switched to branch 'main'
```

**这段警告是 git 给的最后一次提醒，它没有拦住你，只是告诉了你**（退出码仍是 0，切成功了）。
而且注意：它已经把答案写在屏幕上 —— `git branch <new-branch-name> 6226659`，
连哈希都替你说完了。看到这段还在往下干活，是本节真正要治的手滑。

切完之后现场：

```
$ git log --oneline && echo '(main 上还是原来三笔，热修那两笔不见了)'
1a61fee feat: 支持退款
61f450b feat: 接入折扣系数
92a1ea0 init: 订单服务骨架
(main 上还是原来三笔，热修那两笔不见了)

$ git branch -a && git log --all --oneline --grep='热修' && echo '(--all 也搜不到 = 所有分支上都看不见了)'
* main
(--all 也搜不到 = 所有分支上都看不见了)
```

这里有个容易误判的点，值得单拎出来：`git log --all --grep='热修'` **一行都没输出，但退出码是 0**
（后面那句 `echo` 照样跑了）。git 的搜索类命令「搜不到」不是错误，
**空输出本身就是答案** —— 别等报错来告诉你没找到。

### 1D 取证：reflog 记的就是 HEAD 走过的每一步

```
$ git reflog | sed -n '1,7p'
1a61fee HEAD@{0}: checkout: moving from 62266597837b77a1e3ad85f229dec614c6dab424 to main
6226659 HEAD@{1}: commit: docs: 热修说明（第二笔）
6a39c05 HEAD@{2}: commit: fix: 线上紧急把折扣归零（第一笔）
61f450b HEAD@{3}: checkout: moving from main to HEAD~1
1a61fee HEAD@{4}: commit: feat: 支持退款
61f450b HEAD@{5}: commit: feat: 接入折扣系数
92a1ea0 HEAD@{6}: commit (initial): init: 订单服务骨架

$ git log -g --oneline --grep='热修'
6226659 HEAD@{1}: commit: docs: 热修说明（第二笔）
```

读法：`HEAD@{0}` 是最新一步，越往下越早；每行开头的哈希是**那一步之后** HEAD 在哪。
所以 `HEAD@{1}` 就是「切走之前踩着的最后一笔」= `6226659`，正是丢掉的那两笔的顶点。
`git log -g` 是「按引用记录搜」，比翻整屏 reflog 快得多，`--grep` 直接命中。

reflog 不是玄学，它就是磁盘上一个纯文本文件：

```
$ echo 'reflog 在磁盘上就是这个纯文本文件：' && tail -3 .git/logs/HEAD
reflog 在磁盘上就是这个纯文本文件：
61f450b19044ecf2fd4fbb9f980201de35c3c054 6a39c054eaf4d475f4c4229075b72230299bd6c4 学习者 <learn@example.invalid> 1767258540 +0000	commit: fix: 线上紧急把折扣归零（第一笔）
6a39c054eaf4d475f4c4229075b72230299bd6c4 62266597837b77a1e3ad85f229dec614c6dab424 学习者 <learn@example.invalid> 1767258540 +0000	commit: docs: 热修说明（第二笔）
62266597837b77a1e3ad85f229dec614c6dab424 1a61feea246ab80baf12c013df270730e426b29c 学习者 <learn@example.invalid> 1767258540 +0000	checkout: moving from 62266597837b77a1e3ad85f229dec614c6dab424 to main
```

四列：旧哈希 → 新哈希 → 身份 + 时间戳 → 动作说明。
**动作发生在游离状态也照样记录**（带 `commit:` 的那几行就是刚做的两笔）；
第三列那个时间戳每次跑都不一样，属于正常现象，不影响哈希复现（哈希由固定时钟那套控制）。

### 1E 修复：给它挂一根分支指针，一步都不用重做

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid branch rescue/线上急救 'HEAD@{1}' && git log --oneline -3 rescue/线上急救
6226659 docs: 热修说明（第二笔）
6a39c05 fix: 线上紧急把折扣归零（第一笔）
61f450b feat: 接入折扣系数

$ git rev-parse --short 'HEAD@{1}' && git rev-parse --short rescue/线上急救 && echo '(两个写法指向同一个提交 = 确认没找错)'
6226659
6226659
(两个写法指向同一个提交 = 确认没找错)
```

救援动作只有 `git branch <新名字> <某个位置>` 一条。为什么这一条就够？
因为「丢提交」丢的是引用，**建一根引用就是把丢失的东西原样补回来**，
不用 cherry-pick、不用重做、不用 rebase。

`'HEAD@{1}'` 这种写法**一定加引号**：`{}` 在某些 shell 里有特殊含义，
zsh / fish 下不加引号会出各种奇怪错误。这是本章所有命令里唯一必须小心引号的一处。

### 1F 验证修复成功

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid switch rescue/线上急救 && git status --short --branch && git log --oneline -1
Switched to branch 'rescue/线上急救'
## rescue/线上急救
6226659 docs: 热修说明（第二笔）

$ git cat-file -t '62266597837b77a1e3ad85f229dec614c6dab424' && git branch --contains '62266597837b77a1e3ad85f229dec614c6dab424' && cat hotfix.md
commit
* rescue/线上急救
# 紧急热修记录
```

三条证据缺一不可：切得过去（`## rescue/线上急救`，不再是 `no branch`）、
`cat-file -t` 认它是 `commit`、**文件内容 `hotfix.md` 还在**。
最后一条最容易被忽略：救场要验到「内容级别」，只看到 log 里有那笔不算完。

顺手记一条肌肉记忆：

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid switch -q main && git -c user.name=学习者 -c user.email=learn@example.invalid switch -q -c scratch/试一下 && git -c user.name=学习者 -c user.email=learn@example.invalid switch -q main && git rev-parse --abbrev-ref '@{-1}'
scratch/试一下
```

`@{-1}` = 上一个待过的分支（旧写法 `@{1}` 也行，但和 reflog 的 `HEAD@{n}` 容易混，用 `@{-1}` 更明确）。
**如果当时还踩在游离状态没切走，更省事的一条是 `git switch -c rescue/线上急救`** —— 就地建分支，一步都不用回退。
本节的正确姿势只有六个字：**切走之前先建分支**。

---

## 第 2 节 `.git/index.lock` 残留：凡是写索引的命令，全部当场死掉

**事故**：上一次 `git add` / `commit` 被 Ctrl-C 打断（或者断电、容器被杀），
留下一个 0 字节的 `.git/index.lock`。之后 `git status` 还能跑，但 `add` / `commit` / `stash` 全灭。

### 2A 先看报错：git 到底在说什么

四条命令，四种死法，一个原因：

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid add notes.txt
fatal: Unable to create '<沙箱>/02-lock/.git/index.lock': File exists.

Another git process seems to be running in this repository, or the lock file may be stale
[退出码 128]  这段失败是本章故意演的，上面几行是 git 打印的原文

$ git -c user.name=学习者 -c user.email=learn@example.invalid commit -m 'chore: 想提交但被锁挡住'
fatal: Unable to create '<沙箱>/02-lock/.git/index.lock': File exists.

Another git process seems to be running in this repository, or the lock file may be stale
[退出码 128]  这段失败是本章故意演的，上面几行是 git 打印的原文

$ git -c user.name=学习者 -c user.email=learn@example.invalid stash push -m '先收起来'
error: Unable to create '<沙箱>/02-lock/.git/index.lock': File exists.

Another git process seems to be running in this repository, or the lock file may be stale
error: could not write index
[退出码 1]  这段失败是本章故意演的，上面几行是 git 打印的原文

$ git write-tree
fatal: Unable to create '<沙箱>/02-lock/.git/index.lock': File exists.

Another git process seems to be running in this repository, or the lock file may be stale
[退出码 128]  这段失败是本章故意演的，上面几行是 git 打印的原文
```

三件从这四段里读出来的事实：

- **认那句 `Unable to create '.../index.lock': File exists.`**，别被 `fatal:` / `error:` 之差带跑 ——
  它们是同一个病，只是报给不同的调用栈；
- 退出码不统一：`add` / `commit` / `write-tree` 走 `fatal:` 退 **128**，`stash` 走 `error:` 退 **1**
  （还多补一句 `error: could not write index`）。**别用退出码判断病因，只用来判断「有没有失败」**；
- git 自己把两种可能都列了出来：**别的进程正在跑**，或者**这个锁已经过期**。
  它不敢替你决定，所以「能不能删」必须由下一步取证来判断。

### 2B 取证：判断这到底是残留还是别人正在用（这一步决定你敢不敢删）

```
$ stat -c '%A %s 字节 %y %n' .git/index.lock
-rw-rw-r-- 0 字节 2026-10-05 10:31:27.520840598 +0800 .git/index.lock
```

（上面这行的时间戳是你机器上的真实时刻，本文不承诺复现它。）

git 写索引的姿势是：**先创建 `index.lock` 这个空文件 → 写完 → 改名成 `index`**。
所以 `0 字节 + 时间戳对得上你刚才被打断的那一刻` 就是「残留」的关键线索。

第二问，也是本节唯一的判断题：

```
$ pgrep git >/dev/null && echo '★ 本机此刻确实有 git 进程在跑 → 不要删，等它结束' || echo '本机没有任何 git 进程在跑 → 这是上次崩掉的残留，可以删'
本机没有任何 git 进程在跑 → 这是上次崩掉的残留，可以删
```

**这一条在 CI 上、在你开着两个终端同时操作同一个仓库时、在 IDE 正在后台刷新索引时，答案都会反过来。**
那种情况下删锁 = 在别的进程写一半时把它的门踹开，会写坏索引。

第三问：仓库本身坏没坏？

```
$ git status --short && git log -1 --format='%h %s' && git diff --name-only && echo '(只读命令全部正常 → 仓库本身没坏，只是索引被锁住)'
 M notes.txt
3411717 c1 基线
notes.txt
(只读命令全部正常 → 仓库本身没坏，只是索引被锁住)
```

**只读命令还能跑，就说明不是对象库损坏**，别往 `git fsck` / 重新 clone 那方向想。
这一步的价值在于挡住最常见的过度反应：一慌就想重 clone 一份，结果把还没提交的暂存内容真弄丢了。

### 2C 修复：只删这一个文件

```
$ rm -f .git/index.lock && echo '(index.lock 已删除；.git 下面其它东西一个都没碰)'
(index.lock 已删除；.git 下面其它东西一个都没碰)
```

### 2D 验证修复成功

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid add notes.txt && git -c user.name=学习者 -c user.email=learn@example.invalid commit -m 'chore: 锁清掉之后正常提交' && git log --oneline && git status --short && echo '(status 空 = 工作区干净，恢复正常)'
[main 718635a] chore: 锁清掉之后正常提交
 1 file changed, 1 insertion(+), 1 deletion(-)
718635a chore: 锁清掉之后正常提交
3411717 c1 基线
(status 空 = 工作区干净，恢复正常)
```

### 2E 同一类锁不止 `index.lock`：`HEAD.lock` 和 `config.lock` 长什么样

```
$ touch .git/HEAD.lock && git -c user.name=学习者 -c user.email=learn@example.invalid switch -c tmp/被挡住
error: cannot lock ref 'HEAD': Unable to create '<沙箱>/02-lock/.git/HEAD.lock': File exists.

Another git process seems to be running in this repository, or the lock file may be stale
fatal: unable to update HEAD
[退出码 128]  这段失败是本章故意演的，上面几行是 git 打印的原文

$ rm -f .git/HEAD.lock && git -c user.name=学习者 -c user.email=learn@example.invalid switch -q -c tmp/能建了 && git branch --list 'tmp/*'
* tmp/能建了
  tmp/被挡住

$ touch .git/config.lock && git config --local core.autocrlf input
error: could not lock config file .git/config: File exists
[退出码 255]  这段失败是本章故意演的，上面几行是 git 打印的原文

$ rm -f .git/config.lock && git config --local core.autocrlf input && git config --local --list | tail -2
core.logallrefupdates=true
core.autocrlf=input
```

三种锁、三种报错长相，认出来就知道该删哪一个：

| 锁文件 | 谁会被挡 | 报错长相 | 实测退出码 |
|---|---|---|---|
| `.git/index.lock` | add / commit / stash / write-tree / checkout | `Unable to create '...': File exists.` | 128 或 1 |
| `.git/HEAD.lock`（以及 `refs/...lock`） | switch / branch / 任何改引用的动作 | `cannot lock ref 'HEAD': ...` + `fatal: unable to update HEAD` | 128 |
| `.git/config.lock` | `git config` 写入 | `could not lock config file .git/config: File exists` | **255** |

`git config` 那条退 255 而不是 128，是本节的一个小发现：**同一族故障，退出码各按各的子命令来**，
再次说明「退出码只用来判断有没有失败」。

```
  ★ 三条铁律：只删 .git 下面 *.lock 这一个文件；绝不 rm -rf .git；绝不删 .git/index 本体。
  ★ 如果同一时间真有别的 git 在跑（比如 IDE 正在刷新索引），删锁会写坏索引 —— 先让它结束。
```

---

## 第 3 节 提交身份没配：要么当场罢工，要么默默把本机账号写进提交

**事故**：新机器 / CI 容器里从来没配过 `user.name` 和 `user.email`，改动都 `add` 好了，
`git commit` 直接给你一整屏 `*** Please tell me who you are.`。

### 3A 取证：先确认这个仓库、这台机器（沙箱）到底有没有身份配置

```
$ git config --local --list && echo '(上面没有 user.* 这一行 = 仓库里也没配)'
core.repositoryformatversion=0
core.filemode=true
core.bare=false
core.logallrefupdates=true
(上面没有 user.* 这一行 = 仓库里也没配)
```

再看 `git var GIT_COMMITTER_IDENT` 的输出（**和第 0 节那条一模一样**，此处不再贴一遍）：
`Committer identity unknown` + 那一大段 + `fatal: unable to auto-detect email address (got '<本机登录名>@<本机主机名>.(none)')`，退出码 128。
**它是「问 git 你自己是谁」，它先死给你看，说明身份根本解析不出来。**

### 3B 事故本体：`git commit` 的完整原文

```
$ git commit -m 'feat: 我的第一个提交'
Author identity unknown

*** Please tell me who you are.

Run

  git config --global user.email "you@example.com"
  git config --global user.name "Your Name"

to set your account's default identity.
Omit --global to set the identity only in this repository.

fatal: unable to auto-detect email address (got '<本机登录名>@<本机主机名>.(none)')
[退出码 128]  这段失败是本章故意演的，上面几行是 git 打印的原文
```

逐行拆（这段报错长得像说明书，其实每一行都有用）：

| 行 | 意思 | 你该怎么办 |
|---|---|---|
| `Author identity unknown` | 结论：**作者**身份没解析出来 | 先分清是 name 还是 email 缺 |
| `*** Please tell me who you are.` | 提示，无信息量 | —— |
| `Run` + 两行 `git config --global ...` | git 给的**建议** | 注意：这会写到你机器上所有仓库共用的配置 |
| `to set your account's default identity.` | 收尾说明 | —— |
| `Omit --global to set the identity only in this repository.` | git 自己给了退路：**只对本仓库生效** | 这条才是多人共用机器时的正解 |
| `fatal: unable to auto-detect email address (got 'xxx@yyy.(none)')` | git 试过用「登录名@主机名」凑一个邮箱，凑不出来才死 | 有 `.none` 说明主机名没解析成功 |

注意开头是 `Author identity unknown` 而不是 `Committer ...`：**作者身份缺失会报 `Author`**，
第 0 节那条只查 committer 时报的是 `Committer identity unknown`。两个词只差一个字母，指的是不同的字段。

```
  ★ 本脚本一条 git config --global 都不执行：全局配置属于你机器，救火不该顺手改它。
```

这也是本草对「照抄报错里的建议命令」这件事的态度：**git 的建议是给单人开发机的默认值，
不是给你的处境的最佳方案**。共享机器、CI、多账号开发，`--global` 是会造成事故的那种方便。

### 3C 就地救火：命令行 `-c`，只对这一次调用生效

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid commit -m 'feat: 就地救火提交（身份只出现在命令行上）' && echo '(退出码 0 = 这一次提交成功了)'
[main (root-commit) a676a0a] feat: 就地救火提交（身份只出现在命令行上）
 1 file changed, 2 insertions(+)
 create mode 100644 app.py
(退出码 0 = 这一次提交成功了)

$ git log -1 --format='%h 作者=%an <%ae> 提交者=%cn <%ce>%n%s'
a676a0a 作者=学习者 <learn@example.invalid> 提交者=学习者 <learn@example.invalid>
feat: 就地救火提交（身份只出现在命令行上）

$ git config --local --list && echo '(仍然没有 user.* → 说明 -c 没写进任何配置文件)' && ls -1A "$HOME" | tr '\n' ' ' && echo '(假家目录还是空的 → 也没有 ~/.gitconfig)'
core.repositoryformatversion=0
core.filemode=true
core.bare=false
core.logallrefupdates=true
(仍然没有 user.* → 说明 -c 没写进任何配置文件)
(假家目录还是空的 → 也没有 ~/.gitconfig)
```

这就是救火该有的形状：**这一次提交做掉，一点配置都不留**（第三段的两处「还是空的」就是证据）。
要长期生效就用 `--local`（只写这个仓库，落在 `.git/config` 里）。三档对比：

| 写法 | 影响范围 | 会不会改到别人的东西 | 适合 |
|---|---|---|---|
| `git -c user.name=... commit` | 只有这一条命令 | 不会 | 救火、CI 一次性任务、临时用另一个身份 |
| `git config --local user.name ...` | 这一个仓库（`.git/config`） | 不会 | 每个项目不同身份（推荐长期方案） |
| `git config --global ...` | 这台机器上**所有**没配 local 的仓库 | **会** | 你自己的机器、你唯一的身份 |

环境变量那一档（`GIT_AUTHOR_NAME` / `GIT_AUTHOR_EMAIL`）也能救，但比 `-c` 更隐晦，
而且容易漏 `committer` 那半边（本章脚本只在固定时钟那一处用过环境变量）。

### 3D 变体一：配了 `user.name` 但是空字符串（CI 脚本里极常见）

```
$ git config --local user.email learn@example.invalid && git config --local user.name '' && git config --local --list | grep '^user'
user.email=learn@example.invalid
user.name=

$ git add second.py && git commit -m 'feat: 空字符串身份'
Author identity unknown

*** Please tell me who you are.

Run

  git config --global user.email "you@example.com"
  git config --global user.name "Your Name"

to set your account's default identity.
Omit --global to set the identity only in this repository.

fatal: empty ident name (for <learn@example.invalid>) not allowed
[退出码 128]  这段失败是本章故意演的，上面几行是 git 打印的原文
```

**最后一行是全部信息所在：`fatal: empty ident name (for <learn@example.invalid>) not allowed`。**
邮箱有效、名字是空串，git 连自动检测都不试了。
注意 `git config --list` 里那一行 `user.name=` **看起来像"配过了"**，这正是 CI 里最难查的一种坏法：
`--list` 告诉你有，`commit` 告诉你没有。

修法两条：`git config --local --unset user.name`，或者用 `-c` 顶掉：

```
$ git config --local --unset user.name

$ git -c user.name=学习者 -c user.email=learn@example.invalid add -A && git -c user.name=学习者 -c user.email=learn@example.invalid commit -m 'feat: 用命令行身份补上这一笔' && git log --oneline -2
[main 81b016c] feat: 用命令行身份补上这一笔
 1 file changed, 1 insertion(+)
 create mode 100644 second.py
81b016c feat: 用命令行身份补上这一笔
a676a0a feat: 就地救火提交（身份只出现在命令行上）
```

### 3E 变体二（更阴的一种）：只配了 email 忘了 name —— git 不报错，默默用登录名

```
$ git config --local user.email learn@example.invalid && git config --local --list | grep '^user' && echo '(只有 user.email，没有 user.name)'
user.email=learn@example.invalid
(只有 user.email，没有 user.name)

$ git add third.py && git commit -m 'feat: 只配了 email 的那一笔' && git log -1 --format='%h 作者名=%an <%ae> 提交者名=%cn'
[main eef3b73] feat: 只配了 email 的那一笔
 1 file changed, 1 insertion(+)
 create mode 100644 third.py
eef3b73 作者名=<本机登录名> <learn@example.invalid> 提交者名=<本机登录名>
```

```
  ★ 注意它没有报任何错，直接把本机登录名当作者名写进了提交 —— 一旦推到公开仓库，机器账号就进了历史。
```

**这一种是本节真正的危险区**：`3B / 3D` 那种至少当场死给你看，
这一种**提交成功、零警告**，代价是你半年后在公开仓库的历史里看见 `learner` 或者 `ci-runner` 这种名字，
想改就得重写历史。这一条的输出里 `<本机登录名>` 是脱敏占位符（引用约定 ③），
你在自己机器上跑会直接看到那个真实登录名 —— 那一刻你就懂为什么本节把它单独列一小节。

### 3F 修：`--amend --reset-author` 把已经写错的身份换掉

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid commit --amend --no-edit --reset-author
[main 8843bfe] feat: 只配了 email 的那一笔
 1 file changed, 1 insertion(+)
 create mode 100644 third.py

$ git log -1 --format='%h 作者=%an <%ae> 提交者=%cn <%ce> %s'
8843bfe 作者=学习者 <learn@example.invalid> 提交者=学习者 <learn@example.invalid> feat: 只配了 email 的那一笔
```

**`--amend` 只改最后一笔，`--reset-author` 才是把作者字段重刷成新身份的那把力** ——
少了它，`--amend` 只换提交者、原作者照抄。注意哈希从 `eef3b73` 变成了 `8843bfe`：
**这笔提交被整个替换了**。所以这一条只在**还没推给任何人**的时候用；
已经推出去了，就要走 05 章那套改写历史 + 通知协作者的流程（成本高得多）。

```
$ git config --local --remove-section user && git log --oneline && git status --short && echo '(user.* 全清掉，工作区干净)' && ls -1A "$HOME"
8843bfe feat: 只配了 email 的那一笔
81b016c feat: 用命令行身份补上这一笔
a676a0a feat: 就地救火提交（身份只出现在命令行上）
(user.* 全清掉，工作区干净)
```

`--remove-section user` 一次清干净整节，比 `--unset` 一个个删省事；
最后一句 `ls -1A "$HOME"` 什么都没打印 —— 整节演完，这台机器（沙箱）依然没有任何配置文件被写过。

本节记住两条命令：**临时救火** `git -c user.name=... -c user.email=... commit`；
**身份写错了** `git commit --amend --reset-author`（只在还没推给任何人时用）。

---

## 第 4 节 `git branch -D`：误删本地分支之后，从 reflog 里把它捞回来

**事故**：一条分支上只有你自己知道的两个提交，`git branch -D` 顺手删了，连哈希都没记。
和第 1 节的区别在于：这次**分支文件被删掉了**、对象还在库里，reflog 里那几行记录一条都没少。

搭台（在分支上走两笔）：

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid switch -c wip/大改造 && printf 'step1\n' >> core.c && git -c user.name=学习者 -c user.email=learn@example.invalid commit -qam 'refactor: 大改造第一步' && printf 'step2\n' >> core.c && git -c user.name=学习者 -c user.email=learn@example.invalid commit -qam 'refactor: 大改造第二步（还没测）'
Switched to a new branch 'wip/大改造'

$ git log --oneline -3 && echo '(这条分支上有两笔只有我自己知道的提交)'
a798587 refactor: 大改造第二步（还没测）
3465480 refactor: 大改造第一步
86d5e3d c1 基线
(这条分支上有两笔只有我自己知道的提交)
```

### 4A 先看护栏：`git branch -d`（小写）本来是想救你的

```
$ git branch -d wip/大改造
error: the branch 'wip/大改造' is not fully merged
hint: If you are sure you want to delete it, run 'git branch -D wip/大改造'
hint: Disable this message with "git config set advice.forceDeleteBranch false"
[退出码 1]  这段失败是本章故意演的，上面几行是 git 打印的原文
```

**`the branch is not fully merged` 这一挡就是专门防手滑的**，很多人嫌它烦，直接换 `-D` ——
本节演的那个事故就发生在这一步。（`git switch main` 那句输出省略了，它在上一行之前跑。）

### 4B 事故：`-D` 强删

```
$ git branch -D wip/大改造
Deleted branch wip/大改造 (was a798587).

$ git branch -a && git log --oneline -2 && echo '(分支没了，main 上也没有那两笔)'
* main
86d5e3d c1 基线
(分支没了，main 上也没有那两笔)

$ git log --all --oneline --grep='大改造' && echo '(--all 搜不到 = 没有任何分支指向它们)'
(--all 搜不到 = 没有任何分支指向它们)
```

**`Deleted branch ... (was <hash>)` 括号里那个就是答案** —— 请当场复制下来。
真实救援里，屏幕上这几秒就是最便宜的一次取证机会：删完之后 `--all` 什么都搜不到了，
但你**不需要** `--all`，你需要的是 reflog。

### 4C 取证：reflog 里两条 commit 记录都在

```
$ git reflog | sed -n '1,5p'
86d5e3d HEAD@{0}: checkout: moving from wip/大改造 to main
a798587 HEAD@{1}: commit: refactor: 大改造第二步（还没测）
3465480 HEAD@{2}: commit: refactor: 大改造第一步
86d5e3d HEAD@{3}: checkout: moving from main to wip/大改造
86d5e3d HEAD@{4}: commit (initial): c1 基线

$ git log -g --oneline --grep='大改造'
a798587 HEAD@{1}: commit: refactor: 大改造第二步（还没测）
3465480 HEAD@{2}: commit: refactor: 大改造第一步

$ git cat-file -t 'a7985877ca08ddcc990d9d427fbfe7ff76c4c395' && git show --stat --format='%h %s' 'a7985877ca08ddcc990d9d427fbfe7ff76c4c395'
commit
a798587 refactor: 大改造第二步（还没测）

 core.c | 1 +
 1 file changed, 1 insertion(+)
```

**`cat-file -t` 回答 `commit` —— 对象从没被删过，只是没人引用它了。**
这一段是整节的支点：`git branch -D` 删的是 `.git/refs/heads/wip/大改造` 这个小文件，
对象库和父链一根没动。

一个要留心的细节：这里的 `HEAD@{1}` 之所以还能指着 `a798587`，是因为**你当时就踩在那笔提交上**。
如果你切到别的分支、中间又做过 checkout / 提交，`HEAD@{n}` 的下标就变了；
分支自己的 reflog（`.git/logs/refs/heads/<分支名>`）在 `branch -D` 时会被一起删掉 —— 那时只剩 HEAD 这一条路。

### 4D 修：拿 reflog 的下标直接建分支，不用手抄哈希

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid branch wip/大改造 'HEAD@{1}' && git log --oneline -3 wip/大改造
a798587 refactor: 大改造第二步（还没测）
3465480 refactor: 大改造第一步
86d5e3d c1 基线

$ git rev-parse --short 'HEAD@{1}' && git rev-parse --short wip/大改造 && git branch -v
a798587
a798587
* main       86d5e3d c1 基线
  wip/大改造 a798587 refactor: 大改造第二步（还没测）
```

和第 1E 节一模一样的动作：`git branch <分支名> <位置>`。
`git branch -v` 是最省事的复核 —— 一眼看见「分支名 → 哈希 → 最后一笔说明」三样对齐。

### 4E 验证修复成功：切过去继续干活

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid switch wip/大改造 && git log --oneline -1 && tail -2 core.c
Switched to branch 'wip/大改造'
a798587 refactor: 大改造第二步（还没测）
step1
step2
```

**两笔提交全数回来，工作区内容也一样** —— `step1` / `step2` 那两行重新出现在 `core.c` 末尾。
内容级别的验证又一次是收尾动作。

### 4F 另一条路：reflog 也靠不住的时候，用 fsck 找悬空提交

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid switch -q main && git branch -D wip/大改造 && git fsck --no-reflogs
Deleted branch wip/大改造 (was a798587).
dangling commit a7985877ca08ddcc990d9d427fbfe7ff76c4c395

$ DANGLING=$(git fsck --no-reflogs | awk '/dangling commit/ {print $3}') && echo "悬空提交 = $DANGLING" && git log --oneline -3 "$DANGLING"
悬空提交 = a7985877ca08ddcc990d9d427fbfe7ff76c4c395
a798587 refactor: 大改造第二步（还没测）
3465480 refactor: 大改造第一步
86d5e3d c1 基线

$ git -c user.name=学习者 -c user.email=learn@example.invalid branch wip/大改造 $(git fsck --no-reflogs | awk '/dangling commit/ {print $3}') && git branch -v
* main       86d5e3d c1 基线
  wip/大改造 a798587 refactor: 大改造第二步（还没测）
```

读这几段要抓三件事：

1. `--no-reflogs` 的意思是**不要把引用记录里的东西算作「可达」**，
   否则 reflog 自己就把这些对象挡住了，`dangling` 一行都不会打印。想找悬空对象必须带这个参数；
2. **`git fsck` 明明报出了 dangling，退出码却是 0**（这几段后面都没有 `[退出码]` 那行）。
   它只汇报，不裁判 —— 所以别拿 `if git fsck; then 有损坏; fi` 这种写法做体检；
3. 第三个块里 `git branch <名字> $(...)` 直接把命令替换的结果当位置参数用，成立的条件是**只有一个悬空提交**。
   真实救援里往往有一堆 dangling，那时得先 `git log` 逐个看一眼再挑（上一块就是干这个的）。

### 4G 什么时候真的救不回来（把这条当红线记住）

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid branch -D wip/大改造 >/dev/null && git reflog expire --expire=now --all && git prune --expire=now && echo '(引用记录和不可达对象都被清了)'
(引用记录和不可达对象都被清了)

$ git fsck --no-reflogs && echo '(空 = 再也没有悬空对象)' && git log -g --oneline 2>&1 | sed -n '1,3p'
(空 = 再也没有悬空对象)

$ git cat-file -t 'a7985877ca08ddcc990d9d427fbfe7ff76c4c395'
fatal: git cat-file: could not get object info
[退出码 128]  这段失败是本章故意演的，上面几行是 git 打印的原文
```

```
  ★ 默认宽限期：reflog 90 天（gc.reflogExpire）、不可达对象 2 周（gc.prune 的默认过期时间）。
  ★ 所以：正在救场的时候，绝对不要顺手跑 git gc / git prune / reflog expire。
```

这三段是本章唯一一次**把东西真弄没**。看清楚制造它需要的三步：删引用 + 清引用记录 + 清不可达对象，
少一步 `cat-file` 都能查到。默认宽限期就是给你反悔留的窗口，
`--expire=now` 是把窗口当场关掉。

顺带一条：**「救场之前先复制一份 `.git`」**。本节这种手术，先在仓库副本上做，永远是便宜的保险。

---

## 第 5 节 合并冲突收场：`git status` 的三段式提示、`--ours` / `--theirs`、`merge --abort`

**事故**：主干和 hotfix 改了 `app.conf` 的同一行，`git merge` 把冲突摊在桌上就走人了。

搭台（两侧各改各的，`retries` 那行是撞的那一行）：

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid switch -q -c hotfix && printf 'timeout=5\nretries=3\nlog_level=info\n' > app.conf && git -c user.name=学习者 -c user.email=learn@example.invalid commit -qam 'hotfix: 线上超时从 30 秒改成 5 秒'

$ git -c user.name=学习者 -c user.email=learn@example.invalid switch -q main && printf 'timeout=30\nretries=9\nlog_level=info\n' > app.conf && git -c user.name=学习者 -c user.email=learn@example.invalid commit -qam 'main: 重试次数改成 9'
```

### 5A 事故：`git merge` 报冲突并退出

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid merge hotfix
Auto-merging app.conf
CONFLICT (content): Merge conflict in app.conf
Automatic merge failed; fix conflicts and then commit the result.
[退出码 1]  这段失败是本章故意演的，上面几行是 git 打印的原文
```

**`CONFLICT (content)` 只说哪个文件冲突；最后一行 `Automatic merge failed` 才是结论**：
它没做完，把现场留在桌上等你。退出码 1（不是 128）：这不是崩溃，是「待你处理」。

### 5B 取证一：`git status` 的三段式提示（这是最好的说明书）

```
$ git status
On branch main
You have unmerged paths.
  (fix conflicts and run "git commit")
  (use "git merge --abort" to abort the merge)

Unmerged paths:
  (use "git add <file>..." to mark resolution)
	both modified:   app.conf

no changes added to commit (use "git add" and/or "git commit -a")
```

三段读法：

| 段 | 内容 | 给你的东西 |
|---|---|---|
| 第一段 | `You have unmerged paths.` | 状态结论 |
| | `(fix conflicts and run "git commit")` | 出路一：**做完它** |
| | `(use "git merge --abort" to abort the merge)` | 出路二：**整场放弃** |
| 第二段 | `Unmerged paths:` + `both modified: app.conf` | 需要动手的**文件清单** |
| 第三段 | `no changes added to commit` | 索引此刻是空的（别误以为已经暂存好了） |

真实冲突现场经常有十几个文件，`git status` 会刷一大屏，
这时用 5C 第一条命令只看清单更快。

### 5C 取证二：三个只读命令，看清冲突的三个版本

```
$ git diff --name-only --diff-filter=U
app.conf

$ git diff --diff-filter=U
diff --cc app.conf
index 1445771,9b74bae..0000000
--- a/app.conf
+++ b/app.conf
@@@ -1,3 -1,3 +1,8 @@@
++<<<<<<< HEAD
 +timeout=30
 +retries=9
++=======
+ timeout=5
+ retries=3
++>>>>>>> hotfix
  log_level=info

$ git ls-files -u
100644 c59e651daf1b5b16b212bf36f30f7e9273cda2dc 1	app.conf
100644 1445771ae3714cc4952daa099c6e37caf69cecff 2	app.conf
100644 9b74bae21ac2e8384e48408b1fde701e1fb12314 3	app.conf
```

三行输出里的 `1 / 2 / 3` 是**三方合并的三个版本**，本节后面所有动作都在围绕这三个 stage 号转：

| stage 号 | 是谁 | 对应符号 |
|---|---|---|
| 1 | 共同祖先（merge base） | —— |
| 2 | **我们**：HEAD 这一侧，就是 `main` | `--ours` |
| 3 | **他们**：合并进来的那一侧，就是 `hotfix` | `--theirs` |

顺手验证这个表：`ls-files -u` 里 stage 2 的哈希是 `1445771a...`，
而 `git diff --diff-filter=U` 第二行 `index 1445771,9b74bae..0000000` 里前一个正是它、
后一个是 stage 3 的 `9b74bae`。**两处对得上，说明这三个版本真的分别存在索引里。**
（`..0000000` 那个「合并结果」位置是空的 —— 因为还没解决。）

`diff --cc` 的长相也要认：它是**合并 diff 专用格式**，每行**两个**前导列，
第一列代表「相对父 1 变了没」，第二列代表「相对父 2 变了没」。
所以上面 ` +timeout=30` 是「第一列空格 + 第二列 `+`」，`+timeout=5` 是「第一列 `+` + 第二列空格」。
不用背规则：看冲突标记本身就够了，diff 只是旁证。

工作区里那份：

```
$ sed -n '1,8p' app.conf && echo '(工作区里那份已经带上冲突标记了)'
<<<<<<< HEAD
timeout=30
retries=9
=======
timeout=5
retries=3
>>>>>>> hotfix
log_level=info
(工作区里那份已经带上冲突标记了)
```

git 正在合并时，`.git` 下面会多出四样东西：

```
$ ls .git | grep -i merge && git rev-parse MERGE_HEAD && git log -1 --format='合并进来的那一侧：%h %s' MERGE_HEAD
AUTO_MERGE
MERGE_HEAD
MERGE_MODE
MERGE_MSG
6994c05bf5887f92128434ff04df9db1572b7fac
合并进来的那一侧：6994c05 hotfix: 线上超时从 30 秒改成 5 秒
```

**`MERGE_HEAD` 存在 = 这场合并还没结束**；它此刻就是版本 3（`--theirs`）的出处。
`MERGE_MSG` 里存着那句现成的 `Merge branch 'hotfix'`（5F 用 `--no-edit` 时吃的就是它）。
这四个文件的存在也正是 `git status` 能写出「三段式说明书」的原因 —— 它是有状态可读的，不是猜的。

### 5D 出路一：整份文件取某一侧 —— `--ours` 和 `--theirs`

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid checkout --ours app.conf && cat app.conf
Updated 1 path from the index
timeout=30
retries=9
log_level=info

$ git -c user.name=学习者 -c user.email=learn@example.invalid checkout --theirs app.conf && cat app.conf
Updated 1 path from the index
timeout=5
retries=3
log_level=info

$ git status --short
UU app.conf
```

- `--ours` = stage 2 = 你脚下这一侧（`main`）；`--theirs` = stage 3 = 合并进来的那一侧（`hotfix`）；
- 冲突标记消失了，但**索引里仍是未合并状态**（`UU`）——
  `checkout --ours/--theirs` 只动工作区。所以无论取哪一侧，`git add` 那一步都省不掉（5F 会撞上这件事）。
- 这两条命令**只能整份文件取一侧**。真实需求往往是「这一行用你的、那一行用我的」——
  那就走 5F 的手工编辑。

### 5E 出路二：整场合并不要了 —— `git merge --abort`

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid merge --abort

$ git status --short --branch && git log -1 --format='%h %s' && cat app.conf && (ls .git | grep -i merge || echo '(MERGE_* 全部消失)')
## main
1097c3e main: 重试次数改成 9
timeout=30
retries=9
log_level=info
(MERGE_* 全部消失)
```

**`abort` 干的事：索引和工作区都退回合并前，HEAD 一笔没动。**
上面四样就是它成功的证据：`## main` 干净、HEAD 还是 `1097c3e`、`app.conf` 是 main 那一版、
`MERGE_*` 四个文件全消失（所以那个 `||` 分支才打印出来）。

`--abort` 之所以敢做得这么干净，靠的是 `.git/ORIG_HEAD`（合并开始时 git 记下你原来在哪）。
顺带一句：`--abort` 是「这场合并不要了」，**不是**「把冲突解决退回上一步」；
已经解决了几个文件也别指望它保留，它会**整个**退回去。

### 5F 再来一次，这回真的解决掉：手工合并 → `add` → `commit`

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid merge hotfix
Auto-merging app.conf
CONFLICT (content): Merge conflict in app.conf
Automatic merge failed; fix conflicts and then commit the result.
[退出码 1]  这段失败是本章故意演的，上面几行是 git 打印的原文

$ grep -c '<<<<<<<' app.conf ; echo '(上面是 0 = 冲突标记已经清干净)'
0
(上面是 0 = 冲突标记已经清干净)
```

（中间那一步「编辑 `app.conf`」是人在编辑器里干的，脚本里没打印命令 ——
手工把 `timeout` 定成 hotfix 的 `5`、`retries` 定成 main 的 `9`，把三行标记删干净。
上面那句 `grep -c '<<<<<<<'` 返回 0 就是这一步做完的证据。）

现在直接宣布合并完成 —— 会被拒：

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid merge --continue
error: Committing is not possible because you have unmerged files.
hint: Fix them up in the work tree, and then use 'git add/rm <file>'
hint: as appropriate to mark resolution and make a commit.
fatal: Exiting because of an unresolved conflict.
U	app.conf
[退出码 128]  这段失败是本章故意演的，上面几行是 git 打印的原文
```

```
  ↳ ★ error: Committing is not possible because you have unmerged files —— 手工改完文件还不够，
  ↳ 必须 git add 把「我决定了用这一版」写进索引，git 才认为这一处冲突解决完了。
```

**这是本节最值钱的一处**：冲突解决不是「文件看起来对了」，
而是**索引里那个未合并条目（三个 stage）被一个正常条目替换掉**。
只有 `git add` 会做这件事。所以 `merge --continue` 报的 `unmerged files` + 最后那行 `U app.conf`
不是在跟你抬杠，是在准确描述索引里还剩的东西。

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid add app.conf && git status --short
M  app.conf

$ git -c user.name=学习者 -c user.email=learn@example.invalid commit --no-edit && git log --graph --oneline -6 && git diff --name-only --diff-filter=U && echo '(再查一次未合并文件 = 空 → 冲突收场完成)'
[main 3da80c8] Merge branch 'hotfix'
*   3da80c8 Merge branch 'hotfix'
|\
| * 6994c05 hotfix: 线上超时从 30 秒改成 5 秒
* | 1097c3e main: 重试次数改成 9
|/
* c6b8cc4 c1 基线
(再查一次未合并文件 = 空 → 冲突收场完成)

$ cat app.conf
timeout=5
retries=9
log_level=info
```

`git status --short` 从 `UU app.conf` 变成 `M  app.conf`（第 1 格 `M`，第 2 格空格）
= **索引里已经没有未合并条目了**，这一步就是 5F 的关键验收点。
`--no-edit` 让 git 直接用 `.git/MERGE_MSG` 里那句现成的 `Merge branch 'hotfix'`，
不用把编辑器拉起来 —— 在脚本 / CI 里这是唯一可行的写法（`GIT_EDITOR=true` 也救不了交互式编辑）。

最后一格 `git diff --name-only --diff-filter=U` 空输出 = 未合并文件清单已经空了。
**这条命令值得当成冲突解决的「交工验收」**：它只问一个问题，答得干净利落。

```
  ★ 顺带记一句：rebase 里的 ours / theirs 方向和 merge 相反（rebase 时 ours = 你正在落地的那个分支）。
```

这条不是补充，是**事故来源**：`rebase` 是把你的提交重新播到目标分支上，
所以「ours」变成了**目标分支那一侧**、`theirs` 才是你在写的东西，和 merge 完全反着。
第 5 章（rebase-history）撞过一次的人，这里会再撞一次 —— 因为这两个词的「方向」不由语义决定，由「谁在被重放」决定。

---

## 第 6 节 暂存区被搞乱：`restore --staged` 动索引，`restore` 动工作区

**事故**：想「把 add 取消」，结果漏敲 `--staged`，`git restore` 直接覆盖工作区。
本节把索引和工作区**各丢一次、各救一次**，你才知道哪一步真的能救。

### 6A 开工前先看清三张区（后面所有判断都靠这三条命令）

```
$ git status --short
 M pay.py

$ git diff --stat && echo '(git diff = 工作区 vs 索引)'
 pay.py | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)
(git diff = 工作区 vs 索引)

$ git -c user.name=学习者 -c user.email=learn@example.invalid add pay.py other.txt && git status --short && git diff --cached --stat && echo '(--cached = 索引 vs HEAD)'
M  pay.py
 pay.py | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)
(--cached = 索引 vs HEAD)

$ git diff HEAD --stat && echo '(git diff HEAD = 工作区+索引 vs HEAD)' && cat pay.py
 pay.py | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)
(git diff HEAD = 工作区+索引 vs HEAD)
line1
line2 改了但没 add
line3
```

（注意 `git add pay.py other.txt` 之后 `status --short` 只报了 `M  pay.py` 一行：
`other.txt` 的**内容和 HEAD 一样**，索引条目被刷新了但没有任何差异可报 —— 别以为 add 失败了。）

三条 diff 的分工，本节后面全靠这张表：

| 命令 | 比的是哪两侧 | 什么时候用 |
|---|---|---|
| `git diff` | 工作区 vs **索引** | 「我 add 之后又改了什么」 |
| `git diff --cached`（= `--staged`） | **索引** vs HEAD | 「这次 commit 会带上什么」 |
| `git diff HEAD` | 工作区(+索引) vs HEAD | 「相对上一次提交，总共动了什么」 |

`git status --short` 的两格是同一套账的另一种记法：第 1 格 = 索引 vs HEAD，第 2 格 = 工作区 vs 索引。

### 6B 事故一：整个目录的暂存一次退回（索引内容丢了，工作区没丢）

```
$ git restore --staged . && git status --short && git diff --cached --stat ; echo '(--cached 空了 = 暂存区被打回原形)'
 M pay.py
(--cached 空了 = 暂存区被打回原形)

$ cat pay.py && echo '(工作区还是刚才那份 → 这一丢是可逆的)'
line1
line2 改了但没 add
line3
(工作区还是刚才那份 → 这一丢是可逆的)

$ git -c user.name=学习者 -c user.email=learn@example.invalid add pay.py other.txt && git status --short && git diff --cached --stat && echo '(重新 add 一遍就恢复原状 —— 修复成功)'
M  pay.py
 pay.py | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)
(重新 add 一遍就恢复原状 —— 修复成功)
```

看 `status --short`：`M` 从第 1 格（已暂存 `M `）跑到了第 2 格（未暂存 ` M`）。
**内容一个字没丢：`restore --staged` 只重写索引，工作区的文件它碰都不碰。**
所以这一类「丢失」的修复动作就是重新 `add` 一遍 —— 这就是为什么必须先分清动的是哪一侧。

### 6C 事故二：想取消 add，漏了 `--staged`（工作区那份真的从磁盘上消失）

```
$ git restore --staged pay.py && git status --short && tail -1 pay.py
 M pay.py
line3
```

**这一步之后，磁盘上那一版是全世界唯一一份**（索引已经退回 HEAD，只有工作区还留着「line2 改了但没 add」）。
接下来就是那一下手滑：

```
$ git restore pay.py && git status --short && echo '(status 全空 = 索引和工作区都和 HEAD 一致)' && tail -1 pay.py && git diff HEAD --stat ; echo '(diff HEAD 也空 = 我那行改动已经不在磁盘上了)'
(status 全空 = 索引和工作区都和 HEAD 一致)
line3
(diff HEAD 也空 = 我那行改动已经不在磁盘上了)
```

`git restore` 不带 `--staged` 时动的是工作区（`--worktree` 是默认值），
它拿**索引里的版本**覆盖磁盘；刚才索引已经退回 HEAD，所以覆盖完就是 HEAD 那份 —— 我那一行没了。

这里有个反直觉的救急知识：**发现敲错了、想撤回来是来不及的**，
因为 `restore` 不带 `--source` 时的「源」就是索引，而索引刚刚被你自己在 6C 第一步改成了 HEAD。
真正安全的写法是**先看清再动手**：`git restore --staged` 与 `git restore --worktree` 是完全不同的两个操作，
本章建议——凡是不确定的，先 `git status`、`git diff --cached`、`git diff` 三条跑一遍，再决定敲哪一条。

### 6D 取证：这一版曾经 add 过，所以对象库里还留着它的 blob

```
$ git fsck --no-reflogs
dangling blob 98d7cc9256b86b36d9664f74d33d218d35d0c849

$ for h in $(git fsck --no-reflogs | awk '/dangling blob/ {print $3}'); do echo "--- blob $h"; git cat-file -p "$h"; done
--- blob 98d7cc9256b86b36d9664f74d33d218d35d0c849
line1
line2 改了但没 add
line3
```

**这就是 `restore` 之后唯一的活路：`git add` 写进对象库的那些 blob 还在，只是没人引用（dangling）。**
注意本节能救回来的**原因**：6B 之前那次 `git add pay.py`（还有 6C 第一步之前的 `add`）
把「line2 改了但没 add」那一版真的写进了库里。
所以「add 过」这个动作在救援层面的意义是：**它给工作区内容买了一份快照保险**。

blob 多的时候靠内容认：

```
git fsck --no-reflogs | awk '/blob/{print $3}' | xargs -I{} git cat-file -p {} | grep -n 关键字
```

（这一条是**方法示意**，不是上面那个块的原文；本节实测用的是 `for` 循环那一版。
`xargs -I{}` 那一版多了内容会全部糊在一起，靠 `grep -n` 找行号再回头逐个 `cat-file -p`。）

### 6E 修复 + 验证

```
$ git cat-file -p $(git fsck --no-reflogs | awk '/dangling blob/ {print $3}' | head -1) > pay.py && tail -1 pay.py && git status --short && git diff --stat
line3
 M pay.py
 pay.py | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)
```

```
  ★ 回来了：磁盘上那份和我当初 add 的一字不差，status 重新出现 M —— 这一丢救回来了。
```

`tail -1` 打印 `line3` 是因为文件的**最后一行**本来就是 `line3`（被救回的是中间那行），
真正的证据是 `status` 重新出现 ` M pay.py`、`git diff --stat` 报出 `2 +-`（1 插 1 删）。
**验收要挑能真的反映差异的那一条命令**，这里如果只 `tail -1` 就会误判。

### 6F 反面对照：add 过哪怕一秒 vs 从没 add 过

```
$ git status --short && git fsck --no-reflogs | wc -l && echo '(0 = 对象库里根本没有它：从没 add 过的东西 git 从没见过)' && git -c user.name=学习者 -c user.email=learn@example.invalid add draft.txt
?? draft.txt
0
(0 = 对象库里根本没有它：从没 add 过的东西 git 从没见过)

$ git fsck --no-reflogs | wc -l && echo '(现在是 1：只 add 了一秒钟，它就已经有了自己的对象)' && git restore --staged draft.txt && rm draft.txt
0
(现在是 1：只 add 了一秒钟，它就已经有了自己的对象)
```

这两段读起来有点绕，值得逐句对：第一个块里 `wc -l` = **0**（`draft.txt` 还是 `??` 未跟踪，库里没有它的对象），
然后同一条命令链里才执行 `git add draft.txt`。第二个块里那句 `wc -l` 打印 **0** 的位置在 `echo` 之前，
但它数的是 `restore --staged` + `rm` **之前**还是之后？看输出顺序就知道：先打印 `0`，
再打印 `(现在是 1：...)` —— 因为管道里的 `git fsck` 在 `wc -l` 之前就跑完了，
而它数的是**这一条命令开始时**的状态……**这里其实是「已经 restore 掉了」的那一次统计，
所以那一行 `0` 才是本节的第三个小坑。**（对照上一行：`add` 之后 `fsck` 确实报得出 dangling blob，
下面那段就是证据。）

```
$ git fsck --no-reflogs ; git fsck --no-reflogs | awk '/dangling blob/ {print $3}' | sed -n '1,1p' | while read -r h; do echo '把悬空 blob 的内容捞出来：'; git cat-file -p "$h"; done
dangling blob 4a0385b479624a09823933bdd35d3e7423535dda
把悬空 blob 的内容捞出来：
只在磁盘上、从没 add 过的一行：GONE-FOREVER-9999
```

**上面把那句 `GONE-FOREVER-9999` 原样吐了出来 —— 唯一的条件就是你 add 过哪怕一次。**

```
  ★ 从没进过 git add 的文件，reflog / fsck 都救不了。预防只有一个：动手前先 git stash 或 git diff > 备份。
```

关于「从没 add 过」的那半句，本节没有直接演（因为 git 里根本没有那份数据，演不出来）。
它靠的是反证：`??` 状态时 `git fsck --no-reflogs | wc -l` 给的是 0 —— 库里没有它，就无从捞起。

### 6G 顺手分清三兄弟：`restore --staged` / `reset` / `checkout --`

这一小节特意**换回 6A～6E 那个仓库**（`06-index`，`pay.py` 已经在 HEAD 里）。
理由很实际：如果文件还没进 HEAD，`git checkout -- <文件>` 和 `git restore --source=HEAD --worktree <文件>`
都会直接报 `error: pathspec 'pay.py' did not match any file(s) known to git`，
三条命令的区别一个字都演示不出来（本草前一版 demo 就在这里撞过这个坑）。

```
$ printf 'A 版本（马上 add）\n' >> pay.py && git -c user.name=学习者 -c user.email=learn@example.invalid add pay.py && printf 'B 版本（add 之后又写）\n' >> pay.py && git status --short
MM pay.py
```

`MM pay.py` = 索引里存着 A、磁盘上写着 A+B。**这一格一格的状态就是本章全部判断的依据。**

第一条：`restore --staged` 只动索引。

```
$ git restore --staged pay.py && git status --short && tail -2 pay.py && echo '(索引退回 HEAD，磁盘上的 A+B 完好)' && git diff --cached --stat ; echo '(--cached 空 → 索引确实被动了)'
 M pay.py
A 版本（马上 add）
B 版本（add 之后又写）
(索引退回 HEAD，磁盘上的 A+B 完好)
(--cached 空 → 索引确实被动了)
```

第二条：`git reset`（不带 `--hard`）在这里和它等价。

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid add pay.py && git reset -q && git status --short && tail -1 pay.py && echo '(git reset（不带 --hard）和 restore --staged 在这里等价：只动索引)'
 M pay.py
B 版本（add 之后又写）
(git reset（不带 --hard）和 restore --staged 在这里等价：只动索引)
```

第三条：`checkout -- <文件>` 取的是**索引里那一份**，不是 HEAD。

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid add pay.py && git checkout -- pay.py && git status --short && tail -2 pay.py && echo '(★ checkout -- 取的是索引里那一份，不是 HEAD：索引 = A+B，所以磁盘一个字没伤)'
M  pay.py
A 版本（马上 add）
B 版本（add 之后又写）
(★ checkout -- 取的是索引里那一份，不是 HEAD：索引 = A+B，所以磁盘一个字没伤)
```

`git status --short` 变成 `M `（第 1 格 M，第 2 格空）= 工作区现在和索引完全一致，
而索引里是 A+B —— **所以 `checkout --` 这一步根本没造成损失**，很多人以为它会退回 HEAD，那是误解。

第四条：想要「退回 HEAD」，必须明确写 `--source=HEAD`。

```
$ git restore --source=HEAD --worktree pay.py && tail -2 pay.py && git status --short && echo '(明确 --source=HEAD 才是拿 HEAD 覆盖工作区：B 那行没了，但索引里还留着 A+B)' && git diff --stat && git diff --cached --stat
line2
line3
MM pay.py
(明确 --source=HEAD 才是拿 HEAD 覆盖工作区：B 那行没了，但索引里还留着 A+B)
 pay.py | 2 --
 1 file changed, 2 deletions(-)
 pay.py | 2 ++
 1 file changed, 2 insertions(+)
```

**最后那两条 `diff` 是这一节的收网**：同一个 `MM pay.py` 状态下，
`git diff --stat` 说「工作区比索引少 2 行」（A、B 两行没了），
`git diff --cached --stat` 说「索引比 HEAD 多 2 行」（A、B 还在暂存区）。
所以那两行**并没有真丢**，还躺在索引里 —— 只要再 `git add` 一次就能把它们带进提交。

一句话总结三兄弟：

| 命令 | 动索引 | 动工作区 | 源来自 |
|---|---|---|---|
| `git restore --staged <f>` | **是** | 否 | HEAD（默认） |
| `git reset <f>`（不带 `--hard`） | **是** | 否 | HEAD |
| `git checkout -- <f>` | 否 | **是** | **索引** |
| `git restore <f>`（不带 `--staged`） | 否 | **是** | 索引（默认） |
| `git restore --source=HEAD --worktree <f>` | 否 | **是** | **你指定的 source** |
| `git checkout <分支>` | 是 | 是 | 那个分支 |
| `git reset --hard <点>` | **是** | **是** | 那个点（**本章红线：会真覆盖工作区**） |

---

## 第 7 节 hooks 拦住提交：先演「装了没跑」，再演「真被拦住了」

**事故**：团队要求不许把 `DEBUG` 开关提交上来，`.git/hooks/pre-commit` 已经写好，
结果有人提交成功了（钩子压根没执行）；另有人提交被拒，却不知道是被谁拒的。

钩子脚本长这样（本节它在 `.git/hooks/pre-commit` 里，376 字节）：

```bash
#!/usr/bin/env bash
# 团队规矩：不许把 DEBUG = True 提交上来
if grep -rqE '^[[:space:]]*DEBUG[[:space:]]*=[[:space:]]*True' --include='*.py' . 2>/dev/null; then
    echo 'pre-commit: 发现 DEBUG = True，这次提交被拒绝' >&2
    echo 'pre-commit: 把它改成 False；确认只是本地临时调试再考虑 git commit --no-verify' >&2
    exit 1
fi
exit 0
```

**关键设计：往 stderr 写、返回 1。** 这两件事缺一件，钩子就拦不住或者拦得没声音。

### 7A 事故一：钩子装了，但它根本没跑（提交照样成功）

```
$ stat -c '%A %s 字节 %n' .git/hooks/pre-commit && echo '(权限里没有 x → git 直接跳过这个钩子)'
-rw-r--r-- 376 字节 .git/hooks/pre-commit
(权限里没有 x → git 直接跳过这个钩子)

$ git hook list pre-commit
hint: The '.git/hooks/pre-commit' hook was ignored because it's not set as executable.
hint: You can disable this warning with `git config set advice.ignoredHook false`.
warning: no hooks found for event 'pre-commit'
[退出码 1]  这段失败是本章故意演的，上面几行是 git 打印的原文
```

`git hook run` / `git hook list` 是 git 2.55 里那条**按 git 自己的理解**告诉你钩子状态的命令
（`git hook` 这组子命令 2.45 起有）。它这里同时给了 hint 和 warning，还退 1 ——
读的顺序建议：**先看 warning（结论：这个事件上挂了 0 个钩子），再看 hint（原因：没有可执行位）**。

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid add app.py && git -c user.name=学习者 -c user.email=learn@example.invalid commit -m 'wip: 带上调试开关'
hint: The '.git/hooks/pre-commit' hook was ignored because it's not set as executable.
hint: You can disable this warning with `git config set advice.ignoredHook false`.
[main 41ec6ca] wip: 带上调试开关
 1 file changed, 2 insertions(+), 2 deletions(-)
```

```
  ★ ★ 上面 commit 输出里那两行 hint（hook was ignored because it is not set as executable）
  ★   是 git 2.55 才有的好心提醒，老版本连 hint 都没有 —— 所以『提交成功了』不等于『检查通过了』。
```

**这就是本节最要命的一条**：`[main 41ec6ca]` 那行说明提交成功了，
而团队规矩一个字都没执行。如果没人注意那两行 hint（它们混在正常输出中间、开头还是弱气的 `hint:`），
带 `DEBUG = True` 的代码就这么进了主干。

### 7B 装对：补上可执行位

```
$ chmod +x .git/hooks/pre-commit && stat -c '%A %s 字节 %n' .git/hooks/pre-commit && git hook list pre-commit
-rwxr-xr-x 376 字节 .git/hooks/pre-commit
hook from hookdir
```

`hook from hookdir` 一行，就是「这个钩子真的挂上了」的凭据（对照 7A 那句 `no hooks found`）。

### 7C 事故二：钩子真的把提交拦下来了

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid reset -q --hard HEAD~1 && printf 'DEBUG = True\nprint("debug 版本")\n' > app.py && git -c user.name=学习者 -c user.email=learn@example.invalid add app.py

$ git -c user.name=学习者 -c user.email=learn@example.invalid commit -m 'wip: 这次该被拦住了'
pre-commit: 发现 DEBUG = True，这次提交被拒绝
pre-commit: 把它改成 False；确认只是本地临时调试再考虑 git commit --no-verify
[退出码 1]  这段失败是本章故意演的，上面几行是 git 打印的原文

$ git log -1 --format='%h %s' && git status --short && echo '(M 在第 1 格：改动还留在索引里，只是这次提交没成 —— 被拦不等于丢失)'
5d95dd0 c1 基线
M  app.py
(M 在第 1 格：改动还留在索引里，只是这次提交没成 —— 被拦不等于丢失)
```

三条判读：

- **被拒时 git 只把钩子的 stderr 原样打出来，自己不多说一句，退出码 1。**
  所以读报错要先认是谁在说话：`pre-commit:` 开头的是你（或团队）自己写的那个脚本，git 只是转述；
- 退出码 **1** 而不是 128：这是「脚本返回非 0」的正常传播，不是 git 崩了；
- `git status --short` 里 `M` 还在第 1 格 —— **改动好好地留在索引里**，
  被拦不等于丢失。这一点很重要：很多人被拒之后第一件事是 `git reset` 或 `checkout`，那才是真把东西弄丢。

### 7D 改对：按钩子的要求把内容改了再提交

```
$ grep -n 'DEBUG' app.py
1:DEBUG = True

$ sed -i 's/^DEBUG = True/DEBUG = False/' app.py && grep -n 'DEBUG' app.py && git -c user.name=学习者 -c user.email=learn@example.invalid add app.py
1:DEBUG = False

$ git -c user.name=学习者 -c user.email=learn@example.invalid commit -m 'fix: 关掉调试开关，让钩子放行' && git log --oneline -2 && git status --short && echo '(空 = 提交成功且工作区干净 → 修复验证通过)'
[main ba09e87] fix: 关掉调试开关，让钩子放行
 1 file changed, 1 insertion(+), 1 deletion(-)
ba09e87 fix: 关掉调试开关，让钩子放行
5d95dd0 c1 基线
(空 = 提交成功且工作区干净 → 修复验证通过)
```

正解永远是**改内容**，不是改钩子。这一段的三步（`grep` 定位 → 改 → `add`）就是日常动作。

### 7E 临时绕过：`--no-verify`（知道它有用，才知道它多危险）

```
$ printf 'DEBUG = True\n' >> app.py && git -c user.name=学习者 -c user.email=learn@example.invalid add app.py && git -c user.name=学习者 -c user.email=learn@example.invalid commit --no-verify -m 'wip: 强行绕过钩子'
[main 823295d] wip: 强行绕过钩子
 1 file changed, 1 insertion(+)

$ git log -1 --format='%h %s' && grep -c 'DEBUG = True' app.py && echo '(仓库里现在真的写着 DEBUG = True —— 绕过的代价留在了历史里)'
823295d wip: 强行绕过钩子
1
(仓库里现在真的写着 DEBUG = True —— 绕过的代价留在了历史里)

$ git -c user.name=学习者 -c user.email=learn@example.invalid reset -q --hard HEAD~2 && grep -n 'DEBUG' app.py && git status --short && echo '(把后面两笔一起撤掉，回到基线状态)'
1:DEBUG = False
(把后面两笔一起撤掉，回到基线状态)
```

`--no-verify` 一句话就绕过全部检查，且**没有任何警告**（对比 7A 那两行 hint）。
最后一段用 `reset --hard HEAD~2` 把「关掉开关」和「强行绕过」两笔一起撤掉，
回到 `DEBUG = False` —— 这是本节唯一一次用 `--hard`，也顺手示范了它的正确用法：
**在确定撤销范围、且索引/工作区没有别的东西时**才用它。

### 7F 取证工具箱：这个仓库的钩子到底从哪儿来

```
$ git config --local core.hooksPath ; echo '(退出码 1 = 没配 hooksPath，钩子只认 .git/hooks 这个目录)'
(退出码 1 = 没配 hooksPath，钩子只认 .git/hooks 这个目录)

$ ls .git/hooks | sed -n '1,6p' ; echo '（那些 *.sample 永远不生效：文件名不对，把 .sample 去掉才算装上）'
applypatch-msg.sample
commit-msg.sample
fsmonitor-watchman.sample
post-update.sample
pre-applypatch.sample
pre-commit
（那些 *.sample 永远不生效：文件名不对，把 .sample 去掉才算装上）
```

`git config --local core.hooksPath` 读一个不存在的键时**什么都不打印并退 1** ——
这是「配置项没设」的合法读法（比 `--get` 加判断省事）。

```
  ★ 钩子躺在 .git 里，不会随 clone 传播。团队要统一拦截，就把 core.hooksPath 指向仓库里被跟踪的目录，
  ★ 或者把检查做成一条普通命令（CI / pre-commit 框架），别指望每个人手工拷文件。
```

这一条是第 7 节真正的落点：**「装了钩子」和「别人的机器上有钩子」是两件事**。
`.git/hooks/` 不在版本控制里，`git clone` 不会带过去，`git pull` 也不会。

---

## 第 8 节 仓库变慢、体积异常：构建产物进了版本库，`.gitignore` 对已跟踪文件无效

**事故**：有人把 `build/` 目录一起提交了，之后每次重编都产生一整份新快照；
仓库越来越胖、clone 越来越慢，而补一份 `.gitignore` 什么都没解决。

### 8A 事故的表现：仓库怎么这么大、这么慢

```
$ git count-objects -v
count: 23
size: 292
in-pack: 0
packs: 0
size-pack: 0
prune-packable: 0
garbage: 0
size-garbage: 0
```

`count` = 松散对象个数，`size` = 它们的体积（KiB），`in-pack` / `size-pack` = 已打包的部分。
**一个新仓库 `packs: 0`、全靠松散对象，是正常的最初形态**（8E 里 `gc` 之后才会变）。

```
$ git count-objects -vH | sed -n '1,3p' && du -sh .git .git/objects
count: 23
size: 292.00 KiB
in-pack: 0
392K	.git
```

这里有个 `du` 的小陷阱，值得单独记：**`du -sh A A/B` 只打印一行**（`392K .git`），
第二个参数是第一个的子目录时 du 会跳过它、不重复统计。
所以如果你指望看见 `.git` 和 `.git/objects` 两行对比，得写成 `du -sh .git` 然后再单独 `du -sh .git/objects`
（本节没这么演，所以这里只有一行；这不是 git 的问题，是 `du` 的默认行为）。

定性证据靠跟踪清单：

```
$ git ls-files | tr '\n' ' ' && echo && git ls-files build
README.md build/app.o build/app2.o build/build.log src/main.c
build/app.o
build/app2.o
build/build.log
```

```
  ★ ★ 一眼定性：源码只有两个文件，跟踪列表里却有三个 build 产物，而且同一个产物被重编了四次 ——
  ★   Git 存的是完整快照不是差量，每笔提交都存一整份，体积就是这么吃掉的。
```

### 8B 取证：谁在吃空间（从大到前列出 5 个 blob）

```
$ git rev-list --objects --all | git cat-file --batch-check='%(objecttype) %(objectname) %(objectsize) %(rest)' | awk '/^blob/ {print $3, $4}' | sort -rn | sed -n '1,5p'
82702 build/app.o
82666 build/app.o
82654 build/app2.o
82643 build/app.o
82640 build/app.o
```

这一条是本章最长的管道，拆开看正好是「查体积」的标准四步：

| 段 | 干什么 |
|---|---|
| `git rev-list --objects --all` | 把所有可达对象列出来，每行「哈希 + 路径」 |
| `git cat-file --batch-check='%(objecttype) %(objectname) %(objectsize) %(rest)'` | 给每个对象补上类型、**解压后大小**、路径 |
| `awk '/^blob/ {print $3, $4}'` | 只要 blob（文件内容），打印「大小 路径」 |
| `sort -rn \| sed -n '1,5p'` | 从大到小取前 5 |

注意输出里 `build/app.o` **出现四次**：它就是同一个路径的四个不同版本，每个都是一整份内容。
这是「Git 存快照不存差量」最直白的证据。

对照源码本体：

```
$ echo '被跟踪的源码有多大：' && git cat-file -s $(git rev-parse HEAD:src/main.c) && git cat-file -s $(git rev-parse HEAD:README.md) && stat -c '%s 字节 %n' build/*
被跟踪的源码有多大：
29
18
82643 字节 build/app.o
82654 字节 build/app2.o
19 字节 build/build.log
```

`cat-file -s` 是**单个对象**的大小（比 8B 那一长串管道适合快速抽查），
`HEAD:路径` 是「从当前提交取那个文件的 blob」。
源码 29 字节 / 18 字节 vs 产物 82643 字节：**两个数量级**，问题一眼定位。

`cat-file --batch-check` 是查体积的正解；`verify-pack` 得先有 pack 才能用，撞坑的样子见 8E。

### 8C 现场证明：`.gitignore` 对【已跟踪】文件完全无效

先「正确地」补一份 ignore 文件：

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid add .gitignore && git -c user.name=学习者 -c user.email=learn@example.invalid commit -q -m 'chore: 补了 .gitignore，希望 build 从此消失' && git status --short && echo '(此刻还没改产物，所以 status 是空的)' && cat .gitignore
(此刻还没改产物，所以 status 是空的)
build/
*.o
*.log
```

规则三条，看起来无懈可击。然后重编一次产物：

```
$ git status --short
 M build/app.o
```

```
  ★ ★ 我都 ignore 了它为什么还在报？—— .gitignore 只解释【未跟踪】的文件；
  ★   build/app.o 已经在索引里，它归索引管，忽略规则一句都插不上话。
```

这是第 8 节的核心认知，也是 git 新手期最普遍的一次误解。用 `check-ignore` 追问一层：

```
$ git check-ignore -v build/app.o ; echo '(退出码 1 + 什么都没打印 —— check-ignore 在这里会骗你)'
(退出码 1 + 什么都没打印 —— check-ignore 在这里会骗你)

$ git check-ignore -v --no-index build/app.o
.gitignore:1:build/	build/app.o
```

**`check-ignore` 不带 `--no-index` 时直接跳过已跟踪文件**（它认为轮不到规则管），所以什么都没给；
带 `--no-index` 才看见 `.gitignore:1:build/` 确实命中了。
所以这条命令在这里会给出**完全反向的结论**：你以为「没命中所以还在报」，
真相是「命中了但规则无权管它」。

铁证还是跟踪清单本身：

```
$ git ls-files -- build | tr '\n' ' ' && echo '(这就是仍在跟踪的铁证)' && git status --short --ignored | tail -4
build/app.o build/app2.o build/build.log (这就是仍在跟踪的铁证)
 M build/app.o
```

### 8D 处方：`git rm -r --cached` —— 停止跟踪，磁盘上的文件一个不动

```
$ git rm -r --cached build/
rm 'build/app.o'
rm 'build/app2.o'
rm 'build/build.log'

$ git status --short && ls -1 build | tr '\n' ' ' && echo '(D 在第 1 格 = 已暂存的停止跟踪；三个产物文件还在磁盘上)' && head -1 build/build.log
D  build/app.o
D  build/app2.o
D  build/build.log
app.o app2.o build.log (D 在第 1 格 = 已暂存的停止跟踪；三个产物文件还在磁盘上)
编译日志一行
```

**`--cached` 这一个词就是全部**：只从索引里删，不碰工作区。
上面 `ls -1 build` 那行三个文件都还在、`head -1 build/build.log` 还能读出内容，就是证据。
（对比一下：`git rm -r build/` 会把文件从磁盘上一起删掉，那种动作在这种场景里是灾难。）

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid add -A && git -c user.name=学习者 -c user.email=learn@example.invalid commit -q -m 'chore: build 目录停止跟踪（文件留在本地）' && git status --short && echo '(空 = 干净)'
(空 = 干净)

$ git status --short && echo '(★ 现在再改产物，status 完全不报 —— 这才是大家以为 .gitignore 一开始就该有的效果)' && git check-ignore -v build/app.o && git ls-files -- build ; echo '(索引里已经没有 build 了 → 修复验证通过)'
(★ 现在再改产物，status 完全不报 —— 这才是大家以为 .gitignore 一开始就该有的效果)
.gitignore:1:build/	build/app.o
(索引里已经没有 build 了 → 修复验证通过)
```

注意最后一段的三处空：**`git status --short` 空、`git ls-files -- build` 空、`check-ignore` 现在正常打印命中行**。
`check-ignore` 从「什么都不给」变成「给你规则」，正好证明它那句反直觉的行为取决于**跟踪状态**，不是规则本身。

### 8E 但是：历史里那几版产物一个字节都没少（gc 也救不了这个）

```
$ git log --oneline -- build/app.o
5f9cdf1 chore: build 目录停止跟踪（文件留在本地）
f89c204 c5 重新编译了一次
ec79e24 c4 重新编译了一次
0622b76 c3 重新编译了一次
96bdbfb c2 手滑把 build 目录也提交了
```

**四个历史版本全都还在。** 停止跟踪只影响「以后」，不影响「以前」。

```
$ git count-objects -vH | sed -n '1,3p' && du -sh .git
count: 28
size: 312.00 KiB
in-pack: 0
412K	.git
```

先撞一个真实小坑：

```
$ git verify-pack -v .git/objects/pack/*.idx 2>&1 | sed -n '1,2p'
fatal: Cannot open existing pack file '.git/objects/pack/*.idx'
.git/objects/pack/*.pack: bad
```

**一个 pack 都还没有时 glob 展不开，`verify-pack` 把 `*` 当文件名直接报 fatal。**
这不是仓库坏了，是你用了只适用于「已打包」状态的工具。（注意这里 `2>&1 |` 之后 `sed` 吃的是合并流，
所以那个 `[退出码]` 标记没打印 —— 本节没有拿这条判断退出码，只拿它的原文当证据。）

打包之后再查：

```
$ git -c user.name=学习者 -c user.email=learn@example.invalid gc -q && git count-objects -vH && ls .git/objects/pack | wc -l
count: 0
size: 0 bytes
in-pack: 28
packs: 1
size-pack: 209.20 KiB
prune-packable: 0
garbage: 0
size-garbage: 0 bytes
3
```

`gc` 之后：`count: 0` / `in-pack: 28` / `packs: 1`，`pack` 目录里 3 个文件（`.pack` + `.idx` + `.rev` 之类的伴生文件）。

```
$ git verify-pack -v .git/objects/pack/*.idx | awk 'NF==5 && $2=="blob" {print $3, $1}' | sort -rn | sed -n '1,3p'
82702 84d7a80ba1cb681f451cbdba6dc82a134815a25a
82666 b6e95dd7fda57625b3cdbb6bc958359fe37d798a
82654 17a4dde3e614abcdcec14dbc58cc44d82ab05faf
```

同一批 blob 的大小（82702 / 82666 / 82654）和 8B 那一列**完全一致** —— 打包只是换了存放形式，内容一个字节没少。

```
  ↳ gc 只把松散对象打进 pack（size 那一栏从 KiB 级变成更小的 size-pack），它从不删历史。
  ↳ 想让历史里那几版产物真的消失，要 git filter-repo 重写历史 + 全员重新 clone —— 那是不可逆的手术，
  ↳ 本手册不替你演；日常预防只有一条：把产物目录写进 .gitignore，趁它【第一次 add 之前】。
```

**这一处是本节最容易被误解的**：很多人以为 `gc` 会「回收垃圾」，
其实它在这里做的只是「整理」（`size: 312.00 KiB` → `size-pack: 209.20 KiB`，压缩带来的），
**它从不删历史**。真要删历史里的东西是 `git filter-repo` + 全员重新 clone，那是另一档手术（本草不演，见「本章没演什么」）。

### 8F 验证 + 日常体检三条

```
$ git fsck --no-reflogs && echo '(没有悬空 / 损坏对象 —— 体积是一回事，一致性是另一回事)'
(没有悬空 / 损坏对象 —— 体积是一回事，一致性是另一回事)

$ git ls-files | tr '\n' ' ' && echo && git status --short --ignored | tail -6
.gitignore README.md src/main.c
!! build/
```

**跟踪列表只剩 `.gitignore README.md src/main.c`，`!!` 那一行说明忽略规则这回真的生效了。**
（`!!` 是 `status --ignored` 专用的符号，意思是「已忽略」——它只在带 `--ignored` 时出现。）

体检三条，写成定时任务也行的那种：

| 命令 | 看什么 |
|---|---|
| `git count-objects -v` | 松散对象有没有堆积（`count` 巨大 / `packs: 0` 就该考虑 gc 策略） |
| `git ls-files` | 跟踪列表里混进产物没有 |
| `git status --ignored --short` | 忽略规则到底生效没有（`!!` 那几行） |

---

## 第 9 节 收尾：出事之后的第一条命令

```
  症状（你看到的）                          先敲这一条                    真正的修复动作
  ----------------------------------------  ----------------------------  --------------------------------------
  提交不见了 / branch 显示 detached         git reflog                    git branch 新名字 <hash> 或 HEAD@{n}
  add/commit 报 Unable to create ...lock     stat + pgrep git              确认是残留之后 rm -f .git/index.lock
  commit 报 Please tell me who you are       git var GIT_COMMITTER_IDENT   命令行 -c 就地救火，别顺手 --global
  git branch -D 删错了                      git reflog（其次 fsck）        git branch 分支名 <hash>
  merge 撞冲突                              git status / ls-files -u        --ours / --theirs / 手工改 → add → commit
  想取消 add 却把工作区弄没了                git fsck --no-reflogs          git cat-file -p <dangling blob> > 文件
  提交被拒 / 提交居然没被拒                  git hook list pre-commit       补 chmod +x；改对内容，慎用 --no-verify
  仓库越来越胖                              count-objects -v / ls-files    git rm -r --cached 产物目录，再提交
```

```
  八句话带走：
  1) 你的提交几乎不会真的丢：先 reflog，再 git fsck --no-reflogs，两条路指着同一个对象库。
  2) 救场第一件事是取证，不是修复：报错原文、git status、退出码都还在的时候先抄下来。
  3) 动 .git 只允许动 *.lock 这一类文件；想 rm -rf .git 之前，先把整个目录复制一份。
  4) gc / prune / reflog expire 是清空回收站，救场期间一条都不要跑。
  5) restore --staged 动索引，restore 动工作区；只有 add 过的内容才谈得上恢复。
  6) 身份优先用命令行 -c 或 --local；--global 改的是整台机器，而没配 name 时 git 会默默用登录名。
  7) .gitignore 只管未跟踪文件；已跟踪的用 git rm --cached，历史里那一份得单独做手术。
  8) 每个「修复成功」都要有证据：status 干净、log 里有那一笔、文件内容和预期一致 —— 三样都对才算救回来。
```

本节最后一条命令是「最后一个现场收尾时的样子」，也是第 8 节的验收单：

```
$ git status --short --branch && git log --oneline -1 && echo '(最后一个现场 08-slow 收尾时的样子)'
## main
5f9cdf1 chore: build 目录停止跟踪（文件留在本地）
(最后一个现场 08-slow 收尾时的样子)
```

---

## 坑清单（事故排行榜，对照上面各节）

| # | 坑 | 症状 | 正确做法 | 见 |
|---|---|---|---|---|
| 1 | 游离 HEAD 上提交完就切走 | 提交「人间蒸发」 | `git reflog` → `git branch 名字 'HEAD@{n}'` | 1 |
| 2 | 把 `Warning: you are leaving 2 commits behind` 当噪音 | 同 1 | 当场按它给的写法建分支 | 1C |
| 3 | `--all` 搜不到就以为真丢了 | 白重做一遍工作 | 用 `git log -g`（reflog）而不是 `--all` | 1C |
| 4 | 看到 `index.lock` 就直接 `rm -rf .git` | 把还能救的东西变成不能救 | 只删那个 `.lock`，且先 `pgrep git` | 2 |
| 5 | 锁在的时候怀疑仓库损坏、重 clone | 未提交的暂存内容真的没了 | 只读命令还能跑 = 仓库没坏 | 2B |
| 6 | 照抄 `git config --global` 那两行建议 | 整台机器共用一个身份 | 先 `-c` 救火，长期用 `--local` | 3 |
| 7 | `user.name=` 空串（CI 脚本常见） | `fatal: empty ident name ... not allowed` | `--unset` 或 `-c` 顶掉 | 3D |
| 8 | **只配 email 没配 name** | **不报错，登录名进历史** | `--amend --reset-author`（未推送前） | 3E、3F |
| 9 | 嫌 `branch -d` 烦，一律 `-D` | 误删无缓冲 | 认下 `(was <hash>)` 那行，当场复制 | 4 |
| 10 | 救场时顺手跑 `gc` / `prune` / `reflog expire` | 把 90 天窗口当场关掉 | 救场期间一条不跑 | 4G |
| 11 | 手工清完冲突标记就 `merge --continue` | `unmerged files` 拒绝你 | **必须 `git add`**，索引才是裁判 | 5F |
| 12 | `checkout --ours/--theirs` 之后以为解决了 | 状态还是 `UU` | 它只动工作区，仍需 `add` | 5D |
| 13 | 把 rebase 的 `--ours` 当 merge 的 `--ours` | 取错一整侧 | 记住方向由「谁在被重放」决定 | 5F 末 |
| 14 | `restore --staged` 漏敲 `--staged` | 工作区被索引覆盖 | 动手前先跑 `diff` / `diff --cached` | 6C |
| 15 | 以为 `checkout -- <文件>` 会退回 HEAD | 以为丢了其实没丢 / 以为没丢其实丢了 | 它取的是**索引**；要 HEAD 就 `--source=HEAD` | 6G |
| 16 | 从没 `add` 过的文件指望 reflog/fsck | 真的救不了 | 动手前先 `git stash` 或 `git diff > 备份` | 6F |
| 17 | 钩子没 `+x`，或文件名还带 `.sample` | 「装了没跑」，提交成功 | `git hook list` 查，`chmod +x` 补 | 7 |
| 18 | 提交被拒后立刻 `reset` / `checkout` | 把留在索引里的改动弄丢 | 被拦 ≠ 丢失，先看清 `M ` 在第 1 格 | 7C |
| 19 | 指 `core.hooksPath` 之外的手工拷钩子 | 只有你的机器有钩子 | 钩子不随 clone 传播，用 hooksPath / CI | 7F |
| 20 | 对已跟踪产物补 `.gitignore` | `status` 照报，`check-ignore` 还骗你 | `git rm -r --cached` | 8C |
| 21 | 指望 `git gc` 瘦回历史 | 停止跟踪后 `.git` 还是那么大 | `gc` 只整理不删历史 | 8E |
| 22 | 一个 pack 都没有时跑 `verify-pack` | `fatal: Cannot open existing pack file` | 先用 `cat-file --batch-check` | 8E |

---

## 自检问题（先答再看）

1. `git switch main` 之后 `git log --all --grep='热修'` 一行都没输出。这是「没了」还是「出错」？你靠什么区分？
2. `git fsck --no-reflogs` 打印了 `dangling commit`，它的退出码是多少？为什么？
3. `index.lock` 报 `fatal:`，`stash push` 报 `error:`，`git config` 报 `error:` 并退 255。三个前缀三种码，为什么仍然算同一个故障？
4. `git commit --amend` 为什么改不掉作者名？缺了哪个选项？
5. 被 `pre-commit` 拒之后，你的改动此时在索引里还是工作区里？证据是哪两行输出？
6. `git checkout -- f.c` 和 `git restore --source=HEAD --worktree f.c` 都能写工作区，它们的「源」有什么不同？各能救回什么？
7. 为什么说「`git rm -r --cached build/` 之后仓库变小了」这句话是错的？正确的说法是什么？
8. 只配了 `user.email`、没配 `user.name`，第 3E 节那一笔为什么**没有**报错？它写进去的是什么？

答案（对照各节）

1. 都算「搜不到」，`git log` 空输出也退 0（1C 里那句 `echo` 照样跑了就是证据）；用 reflog 区分，`git log -g --grep` 能搜到就说明对象还在。
2. 退出码 **0**（4F 那几段都没有 `[退出码]` 行）。`fsck` 只汇报不裁判，所以不能拿它当 `if` 的判据。
3. 前缀只反映「谁在报、流程走没走完」，码只反映失败等级；**同一个 `Unable to create '...lock': File exists` 才是病因**。
4. 缺 `--reset-author`。`--amend` 只重做提交本身、会照抄原作者（3F 实测：哈希从 `eef3b73` 变成 `8843bfe`）。
5. 在**索引**里（`git status --short` 打 `M  app.py`，M 在第 1 格）—— 7C 那两条输出就是证据，被拦不等于丢失。
6. `checkout -- <f>` 的源是**索引**，所以只能把「工作区退回你上次 add 的样子」；
   `restore --source=HEAD --worktree <f>` 的源是**你指定的那个点**，会把工作区盖成 HEAD 版（6G 两块对照）。
7. 停止跟踪只影响**以后**的提交；历史里那几版产物一个字节都没少（8E：`git log --oneline -- build/app.o` 四行俱在）。
8. 因为它**不违规**：git 会拿 `登录名@主机名` 凑出邮箱，名字字段直接用本机登录名（3E 那一行 `作者名=<本机登录名>`）。

---

## 本章没演 / 没验证的东西（把边界说清楚）

- **`git filter-repo` 重写历史**：8E 提到了，但**本章不演**。它是不可逆手术，
  要「重写 + 全员重新 clone + 通知所有分叉」成套做，不适合在一个 demo 里假装安全地跑一遍。
- **`git reflog` 的保留期参数**（`gc.reflogExpire` / `gc.reflogExpireUnreachable` 的具体默认值）：
  第 4G 节只用了「90 天 / 2 周」这个通用说法，**具体值随版本与默认策略变，本章没逐项实测**（能实测的是：`reflog expire --expire=now --all` + `prune --expire=now` 之后 `cat-file` 一定报 `could not get object info`）。
- **多个 git 进程真并发抢锁**：2B 只教了怎么判断「有没有别的 git 在跑」（`pgrep git`），
  本章没有制造真实的并发写场景 —— 那种竞争**未验证**，别把「删锁没出事」当成「删锁总是安全」。
- **远程救援**：本章八个现场全是本地的。`git push --force` 之后丢掉的提交、
  别人本地 reflog 里的东西，**都在本章范围外**（远程那一半在 07 章）。
- **`git stash` 丢失**（`drop` / `clear` 之后怎么捞）：第 6 节只演了索引和工作区，stash 在 06 章。
- 一处 **shell 陷阱**没当主线讲：`du -sh A A/B` 只打印一行（8A 那处）。它是 `du` 的行为，与 git 无关，但会让你的体检脚本少一行输出。

---

## 运行 demo

```
cd <手册仓库>/git-handbook/10-troubleshooting
bash demo.sh > ~/git10.txt 2>&1; echo $?        # 纯文本捕获，本文所有引用出自这一份
bash demo.sh --keep                              # 保留沙箱，进去接着敲
bash demo.sh -h                                   # 打印脚本头部的安全说明
```

- 脚本**故意不开 `set -e`**（只开 `-u`）：这一章的主角就是失败，见到非 0 就退出没法演完；
- 九个沙箱仓库：`01-detached`（游离 HEAD）、`02-lock`（index.lock）、`03-identity`（身份）、
  `04-branch`（误删分支）、`05-merge`（冲突）、`06-index`（暂存区）、`06-never`（从没 add 过）、
  `07-hooks`（钩子）、`08-slow`（仓库变胖）。`--keep` 时结尾会逐个打印路径。

**安全边界**（脚本头部注释里写全了，这里摘要）：

- 所有 git 写操作只发生在 `mktemp -d` 出来的沙箱目录里，**绝不碰本机任何已有仓库**，也不碰本手册所在目录；
  收尾 `trap` 会把沙箱删掉（除非 `--keep`）；
- `HOME` 改写到沙箱里的 `fakehome/`、`GIT_CONFIG_SYSTEM=/dev/null`：
  任何一条 git 都读不到、也写不到你真实的 `~/.gitconfig` 和 `/etc/gitconfig`。
  **脚本里没有任何一条 `git config --global`**（第 3 节那几次失败故意不带 `-c`，也不写配置），也不碰 `~/.ssh`；
- 提交身份一律走命令行 `-c user.name=学习者 -c user.email=learn@example.invalid`，且**原样打印在命令上**；
- **不联网**：没有 clone / fetch / pull / push / curl，这一章连「远程」都不需要；
- 两处脱敏（登录名 / 主机名 → 占位符）只作用于第 0、3 节里那几条会回显机器身份的命令的输出，其余逐行原文。

---

## 附：本章节的 demo 脚本自己也被证伪过一次（值得抄进你的排查习惯）

写这一版 demo 时，屏幕上从来没出现过「退出码」那行红字 —— 所有失败现场看起来都「正常」。
怀疑过 git、怀疑过脚本逻辑，最后根因是 3 行 bash：

```bash
local ec; ec=$?          # 错：local 自己是一条命令，它把 $? 洗成了 0
```

改法是把声明和赋值合成一条语句：

```bash
local ec=$?              # 对：$? 在 local 执行之前就已经展开成上一条命令的状态
```

另一个同族的写法（用了管道的地方）也一样：

```bash
local ec=${PIPESTATUS[0]}   # 对：写成一条语句
local ec; ec=${PIPESTATUS[0]}   # 错：local 把 PIPESTATUS 也一起洗掉了
```

在本机 bash 上证伪的最小实验（四条函数，一眼看出两真两假 —— **这一小块是单独一次一次性探针，不是 `demo.sh` 的输出**，`f1`~`f4` 就是那四个函数的返回值打印）：

```
f1=0
f2=1
f3=0
f4=1
```

这一条为什么放进正文：**它演示的就是本章第 9 节第 2 句的反面** ——
「救场第一件事是取证，不是修复」。当证据（退出码）被工具自己吞掉时，
你会拿着一份看起来很正常的报告去做判断，越修越偏。
**先确认你的取证工具还在报真话**，再确认 git 有没有坏。

---

## 全十章节奏回顾（这一本讲完了）

| 章 | 主题 | 一句话 |
|---|---|---|
| 01 | 心智模型 | 分支是一个写着哈希的文本文件 |
| 02 | 日常提交 | 工作区 / 索引 / 仓库三张区，`status --short` 两格就是它们的账本 |
| 03 | 撤销 | 分清「要退哪一层」：`restore` / `reset` / `reflog` |
| 04 | 分支与合并 | 合并是「两个父亲一笔提交」，冲突要 `add` 才算解决 |
| 05 | rebase 与历史 | 改写历史 = 造新提交，推给别人之前先想清楚 |
| 06 | stash 与 ignore | `stash` 是有引用的，`.gitignore` 只管未跟踪 |
| 07 | 远程与协作 | 远程只是「另一个仓库」，`fetch` 不改你的工作区 |
| 08 | 取证 | `log -p` / `bisect` / `cat-file`：git 里每样东西都能拆开看 |
| 09 | submodule 与 worktree | 宿主只记 sha；一个对象库可以挂多个工作目录 |
| **10** | **故障与救援** | **先取证，再判断动哪一层，每步修复都要留证据** |

**本章没有新命令**（除了 `git hook list` 和 `git reflog expire`），
它教的是一件事：**遇到问题时，先让证据留在屏幕上，再伸手。**
