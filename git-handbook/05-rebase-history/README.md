# 05 改写历史（rebase / 交互式 rebase / cherry-pick / amend / reflog）

> 一句话：这一章解决「历史这条线太难看 / 站错了位置 / 手滑弄丢了提交」三件事——
> `rebase` 到底对你的提交做了什么、为什么哈希全变了、
> **冲突停在半路时那三条出路怎么选**、没有图形编辑器怎么用 `rebase -i` 整理历史、
> 以及最值钱的：`rebase` 弄丢的提交**怎么用 `reflog` 一颗不落地捞回来**。

前置：01 章的「提交是快照 + 分支只是一个写着哈希的文件」、02 章的 `add`/`commit`、
03 章的 `reset --hard`/`reflog`、04 章的「冲突标记怎么读」和「`--ours`/`--theirs` 在 rebase 里会反过来」。
本章假定你已经知道合并冲突长什么样，只讲**变基独有的那部分**。

本章最重要的三个认知，先放在这里：

1. **`rebase` 不移动任何提交，它是「照着旧提交重做一批新提交，然后把分支指针挪过去」。**
   新对象的 `parent` 变了、`committer` 时间变了，所以哈希必然全变（场景 ③④ 用 `git cat-file -p`
   把搬家前后的两个对象逐行摆在一起）。旧对象**没被删**，还躺在 `.git/objects` 里 —— 这句话是场景 ⑳ 救命的依据。
2. **交互式 rebase 不需要任何图形编辑器。** `git rebase -i` 打开的所谓「编辑器」就是一个文本文件，
   里面是 `pick <哈希> <说明>` 的清单。用一个 `sed` 当 `GIT_SEQUENCE_EDITOR`，本章所有整理动作
   （`fixup`/`squash`/`reword`/`edit`/`drop`/重排顺序/`--autosquash`）都能无人值守跑完（场景 ⑪~⑮）。
3. **改写完发现不对，先停手，别先跑 `gc`。** 三条出路按「走得多远」排序：
   还没结束 → `git rebase --abort`；刚结束 → `ORIG_HEAD`；过了一阵 → `git reflog`（场景 ⑳~㉓ 把它们放在同一个仓库里逐个现场验证）。

> 本章输出全部是真跑出来的，一条都没编排：主体就是仓库里 `bash demo.sh` 的 **1635 行原文**（A→I 九段，退出码 0），
> 每小节引用的 15~40 行都能在这份输出里逐行找到（文末给了抽查方法）。
> 身份固定成 `学习者 <learn@example.com>`，时间戳从 `2026-01-01T10:01:00 +0800` 起逐条递增，
> 所以你复现出来的**哈希和本文完全一致**（已连跑两次，去掉沙箱目录名后逐字节相同）。
> 沙箱目录后缀每次随机（`/tmp/git-handbook-05-rebase-XXXXXXXX`），本文里出现这个路径的地方一律写成 `<沙箱>`，其余原样。
> git 的进度行（`Rebasing (2/5)`）原文结尾是原地回车 `\r`，`demo.sh` 只做了这一处整理：把 `\r` 换成真换行，
> 内容一字不改、退出码取的是 git 自己的退出码。

---

## 你会遇到的场景

| 你遇到的事 | 本章哪一节 |
|---|---|
| 主线走了一步、我这分支走了两步，到底谁基于谁？我该 rebase 谁？ | 场景 ①（`log --graph` + `merge-base` + `main..topic`） |
| `git rebase main` 敲下去一句 `up to date`，什么都没发生 | 场景 ②（★ rebase 搬的是**你脚下这条**分支） |
| rebase 完 `git log` 里的哈希全变了，我是不是把历史搞坏了？ | 场景 ③④（`cat-file -p` 逐行对照 + `patch-id`） |
| rebase 之后有人说「我那条提交怎么自己不见了」 | 场景 ⑤（`skipped previously applied commit` + `git cherry`）、⑤b（想硬留该怎么办） |
| rebase 停在半路，`CONFLICT` + 一堆 `hint:`，不知道手该往哪放 | 场景 ⑥（读现场：`status` / 文件本体 / `:1:2:3:`） |
| 解完冲突 `git rebase --continue` 说 `needs merge` —— 它为什么不理我 | 场景 ⑦（必须先 `git add`） |
| 这条提交我不要了 / 整个 rebase 我反悔了 | 场景 ⑧⑨（`--skip` / `--abort`，各跑一遍真输出） |
| 听说还有 `--quit`，和 `--abort` 有什么区别 | 场景 ⑩（★ 区别就是一句：状态删了、人留在半路） |
| 想把三条 WIP 压成一条 / 改一条提交说明 / 调顺序 —— 但这台机器没有图形编辑器 | 场景 ⑪⑫⑬（`GIT_SEQUENCE_EDITOR` + `sed`） |
| 每次修点小毛病都要 rebase 一次，太麻烦 | 场景 ⑭（`git commit --fixup` + `rebase -i --autosquash`） |
| 分支中间有两条提交是废的，只想去掉**那一段** | 场景 ⑮（`git rebase --onto <新底本> <分界线> <分支>`） |
| 只想把别人那一条提交拿过来，别的一条都别动 | 场景 ⑯⑰⑱（`cherry-pick` / `-x` / `-n` / 范围 / 合并提交要 `-m`） |
| `git commit --amend` 会不会把改动漏掉？能不能 amend 更早的提交？ | 场景 ⑲（含 `pathspec 'HEAD~1' did not match` 那条真报错） |
| **改了一条已经 push 出去的提交，push 被拒了** | 场景 ⑳（`non-fast-forward` 原文 + `--force-with-lease` 的代价） |
| **手滑把一条提交整行删掉了，git 还笑着回我 `Successfully rebased`** | ㉑㉒㉓（`reflog` 找哈希 → `reset --hard` 整条复原 / `cherry-pick` 只捞那一条） |
| `git fsck` 什么都没报，是不是说明还能找回？能找回的期限是多久？ | ㉔（reflog 让对象保持「可达」；`gc` 才是收圈的） |

---

## 心智模型

### 图 1：合并是「造一个菱形」，变基是「把这条线整个搬到那条线上」

```
   起点（04 章和本章共用的一张图）：

        main ──→ C1 ──→ M1                 ← 对方（主线）走了一步
                │
                └──→ T1 ──→ T2             ← 你（topic）走了两步

   ── merge（04 章）：造一个新的双亲提交，两条线都留在历史里 ──

        main ──→ C1 ──→ M1 ────────→ MM ← 合并提交，parent 写两行
                       ╱            ╲
              T1 ──→ T2 ─────────────╱

   ── rebase（本章）：把你的 T1/T2【重放】到 M1 后面，历史变一条直线 ──

        main ──→ C1 ──→ M1 ──→ T1' ──→ T2' ← topic

        ★ T1' 不是 T1。它是新造的对象：内容一样（补丁一样），parent 不一样，哈希不一样。
```

`git log --graph` 里 merge 的结果是**一个叉再合起来**，rebase 的结果是**一条直线**。
形状之外没有别的区别值得先记：一个是「记录发生过合并」，一个是「假装我是一路顺着主线写下来的」。

### 图 2：一次 rebase 的物理动作 —— 对象没被删，只有指针挪了

```
   rebase 前：topic ──→ T2 ──→ T1 ──→ C1（.git/objects 里就这四个提交）

   rebase 中：git 从 merge-base 开始，把 T1、T2 的【补丁】一条条按到新底本上：
              C1 ──→ M1 ──→ T1' ──→ T2'
                                ↑ 新对象，新哈希

   rebase 后：topic ──→ T2' ──→ T1' ──→ M1 ──→ C1
              T1、T2 仍在 .git/objects 里，只是【没有分支指向它们了】。
              谁还指着它们？—— .git/logs/HEAD（也就是 git reflog）。这就是㉑~㉓全部戏份的来源。
```

### 图 3：三颗后悔药，按「你走得多远」选

```
   git rebase main 开始的那一刻，git 就把「你原来在哪」抄了两个地方：
       .git/ORIG_HEAD                        ← 一个文件，一行哈希
       .git/rebase-merge/orig-head           ← 状态目录里，只在【还没结束】时存在

   ① 还停在半路（冲突 / edit 停住）
        git rebase --abort       → 完全复原：分支、工作区、暂存区全回到开始那一刻
   ② 已经跑完，结果不对
        git reset --hard ORIG_HEAD        → 整条分支回到 rebase 之前
        git reflog | head                 → 想知道「刚才那几步都干了什么」
   ③ 跑完又干了别的，ORIG_HEAD 已经被覆盖
        git reflog 一路往前翻，拿到旧哈希 → branch/reset/cherry-pick 三选一（㉑~㉓）

   ★ 唯一能真正把这圈救生绳剪断的动作：git gc / git reflog expire / git prune。
     发现丢了东西的第一件事是【停手】，不是「再试一把」。
```

---

## ① 先看清形状，再决定要不要 rebase

**什么情况**
你建了 `topic`，写了两条提交；期间 `main` 也走了一步。现在要么合并要么变基，
但你连「谁基于谁、会被搬走的到底是哪几条」都没看清就动手了 —— 这是 rebase 事故的主要来源。

**敲哪几条**

```bash
git log --oneline --graph --decorate --all   # 形状
git merge-base main topic                    # 分叉点
git log --oneline main..topic                # ★ 会被搬走的就是这几条
git log --oneline topic..main                # 对方的提交（它们会变成你的新底本）
git rev-list --count topic ^main             # 我领先几个
```

**真实输出**（沙箱 `a-shape/`，A→I 全部场景都由 `demo.sh` 跑出来，下同）

```text
$ git log --oneline --graph --decorate --all
* 8b40fae (HEAD -> main) m1：main 加了 README.md
| * 110e005 (topic) t2：改 notes.txt 第2行
| * 0f92ba8 t1：加 api.js（main 完全没碰这个文件）
|/  
* 54eff0e c1 共同的起点
[退出码 0]

$ git merge-base main topic
54eff0e8d05049280efca0d1a0836ed9ca19c7ed
[退出码 0]

$ git log --oneline main..topic
110e005 t2：改 notes.txt 第2行
0f92ba8 t1：加 api.js（main 完全没碰这个文件）
[退出码 0]

$ git log --oneline topic..main
8b40fae m1：main 加了 README.md
[退出码 0]

$ echo -n '我领先几个：' && git rev-list --count topic ^main && echo -n '我落后几个：' && git rev-list --count main ^topic
我领先几个：2
我落后几个：1
[退出码 0]
```

**逐行解读**

- 图形第三行 `|/` 就是**分叉点**。它上面没有竖线，是两条线共用的最后一提交 —— `54eff0e`，
  和 `git merge-base main topic` 打印的是同一个东西（一个短一个长，`--oneline` 只是缩写）。
- `main..topic` 读作「**从 main 走不到、但从 topic 走得到**」的提交 = `t1 t2`。
  这三个点就是「这次会被重放的清单」，rebase 之前**必须**先看它，没有第二个替代品。
- `topic..main` 只列出 `m1`，它是你即将落到的**新底本**，不会被搬走。
- 「领先 2 / 落后 1」这两个数字合起来才叫局面：只报「落后 1」就去 rebase，等于没看。

**常见翻车**

- `git log --graph` 不加 `--all`：只看到当前分支，然后以为「对方的提交没了」。
- 把 `..` 读反：`main..topic` 不是「main 到 topic」，是「topic 里减掉 main 里已经有的」。
- 用 `git status` 看局面：它只告诉你当前分支干净不干净，**完全不看另一条分支**。

---

## ② `git rebase main` 搬的是你脚下这条分支（方向性错误）

**什么情况**
明明想「把 topic 搬到 main 上」，人却站在 `main` 上敲了 `git rebase main`。
git 不报错，只回一句 `up to date`，你以为生效了，其实一步没动。

**敲哪几条**

```bash
git switch main          # ← 站错了
git rebase main          # ← 自己搬自己
git switch topic         # ← 正确姿势：先站到【要被搬的那条】上
git rebase main
```

**真实输出**

```text
$ git switch -q main && git rev-parse --abbrev-ref HEAD
main
[退出码 0]

$ git rebase main
Current branch main is up to date.
[退出码 0]

$ git switch -q topic && git rev-parse --abbrev-ref HEAD
topic
[退出码 0]

$ git rebase main
Rebasing (1/2)
Rebasing (2/2)
Successfully rebased and updated refs/heads/topic.
[退出码 0]

$ git log --oneline --graph --decorate --all
* 1c8ca34 (HEAD -> topic) t2：改 notes.txt 第2行
* 8357104 t1：加 api.js（main 完全没碰这个文件）
* 8b40fae (main) m1：main 加了 README.md
* 54eff0e c1 共同的起点
[退出码 0]
```

**逐行解读**

- `Current branch main is up to date.` 退出码是 **0**。它不是错误提示，是「我按你说的做了，
  你要搬的这条分支（= main）已经在自己头上，无需搬」。这是本章最阴的坑：**没有红字**。
- 正确的 rebase 只有一句口诀：**站到要被动的那条分支上，把要落到的底本当参数。**
- 成功后图形变成一条直线（对比 ①：`| *` 的叉没了），`topic` 的头从 `110e005` 变成了 `1c8ca34`。
- 不想记方向，就用显式写法（新版本 git）：`git rebase --onto main main topic` —— 三个位置写全，
  站哪儿都不影响结果（`--onto` 的读法见 ⑮）。

**常见翻车**

- 站错分支后**又补一句** `git rebase topic`：现在搬的是 main，把主线拖到了你那条线的尾巴上。
  发现不对立刻 `git rebase --abort`（如果还在半路）或 `git reset --hard ORIG_HEAD`（如果已经跑完，见 ㉓）。

---

## ③ 为什么 rebase 之后哈希全变了：`%h %p` 前后对照

**什么情况**
rebase 完一看 `git log`，每条提交的哈希都换了，心里一沉：是不是把历史改坏了？

**敲哪几条**

```bash
git log --format="%h %p %s" main..topic   # 搬家前跑一次，搬家后再跑一次
```

**真实输出**（同一个仓库、同一对提交，只是中间做了一次 rebase）

```text
$ git log --format="%h %p %s" main..topic
110e005 0f92ba8 t2：改 notes.txt 第2行
0f92ba8 54eff0e t1：加 api.js（main 完全没碰这个文件）
[退出码 0]
  ·  topic 现在 = 110e005，main 现在 = 8b40fae

$ git log --format="%h %p %s" main..topic
1c8ca34 8357104 t2：改 notes.txt 第2行
8357104 8b40fae t1：加 api.js（main 完全没碰这个文件）
[退出码 0]
```

**逐行解读**

- 格式 `%h %p %s` = 自己的短哈希、**父亲的短哈希**、说明。左右两栏比的是同一个字段。
- 说明文字**一模一样**（`t1：加 api.js…`），`%h` 却全变了 —— 因为它们**不是同一批对象**，
  是照着重做的副本。
- 看第二栏 `%p`：搬家前 `t1` 的父亲是 `54eff0e`（共同起点），搬家后是 `8b40fae`（main 的头 m1）。
  **父亲变了**，这就是 rebase 的全部目的；而哈希是对「父亲」这块内容算出来的，所以必然变。
- `·` 那行是 demo 打的提示：分支指针从 `110e005` 挪到了 `1c8ca34`。旧哈希 `110e005` 此刻仍然存在，
  只是没有分支指着它了 —— 这正是 ㉑~㉓ 能救回来的物理前提。

---

## ④ 打开提交对象看：什么变了、什么没变（还有「补丁没变」这件事）

**什么情况**
你想知道 rebase 到底改了对象的哪几个字段；或者有人告诉你「tree 也变了，那内容不就变了吗」。

**敲哪几条**

```bash
git cat-file -p <旧哈希>          # 对象原文
git cat-file -p <新哈希>
git diff --stat <旧> <新>         # 两个提交对象之间的差异
git show <提交> | git patch-id --stable
git hash-object -t commit --stdin # 手工造 commit 对象，看哈希对哪几个字段敏感
```

**真实输出 A：搬家前后两个对象原文**

```text
$ git cat-file -p 0f92ba8f74f21c131ae5d450e1c30bc2884723de
tree 2bffc9aa85cc1e1dee90e26a7eaa8482bdab4749
parent 54eff0e8d05049280efca0d1a0836ed9ca19c7ed
author 学习者 <learn@example.com> 1767232920 +0800
committer 学习者 <learn@example.com> 1767232920 +0800

t1：加 api.js（main 完全没碰这个文件）
[退出码 0]

$ git cat-file -p 835710466acfba20accd3736a316ff403282bbcf
tree 3a5f1923e5478ff5f622261dc6ef7093853f1a81
parent 8b40fae283ac9728f65ffd96fba0ecf44afca4c7
author 学习者 <learn@example.com> 1767232920 +0800
committer 学习者 <learn@example.com> 1767233100 +0800

t1：加 api.js（main 完全没碰这个文件）
[退出码 0]
```

**真实输出 B：逐行对照 + patch-id**

```text
$ echo "author     旧=$(git log -1 --format=%ad 0f92ba8f74f21c131ae5d450e1c30bc2884723de)"
author     旧=Thu Jan 1 10:02:00 2026 +0800
[退出码 0]

$ echo "author     新=$(git log -1 --format=%ad 835710466acfba20accd3736a316ff403282bbcf)"
author     新=Thu Jan 1 10:02:00 2026 +0800
[退出码 0]

$ echo "committer  新=$(git log -1 --format=%cd 835710466acfba20accd3736a316ff403282bbcf)"
committer  新=Thu Jan 1 10:05:00 2026 +0800
[退出码 0]

$ git diff --stat 0f92ba8f74f21c131ae5d450e1c30bc2884723de 835710466acfba20accd3736a316ff403282bbcf | cat
 README.md | 1 +
 1 file changed, 1 insertion(+)
[退出码 0]

$ git show 0f92ba8f74f21c131ae5d450e1c30bc2884723de | git patch-id --stable
c06f1ce6b3adca8acd63855b31ed9210557c5773 0f92ba8f74f21c131ae5d450e1c30bc2884723de
[退出码 0]

$ git show 835710466acfba20accd3736a316ff403282bbcf | git patch-id --stable
c06f1ce6b3adca8acd63855b31ed9210557c5773 835710466acfba20accd3736a316ff403282bbcf
[退出码 0]
```

**逐行解读**

- 提交对象一共五样东西：`tree`（整棵树快照的哈希）、`parent`、`author`、`committer`、空行后的说明。
- **`author` 连时间戳都原样保留**（`1767232920` 两行一样）；**`committer` 换成了新时间**（`1767233100`）——
  重放就是一次「重新提交」，所以提交者时间必然刷新。这也是 `git log --format=%ad` 和 `%cd` 要分两个字段的原因。
- `tree` 也变了（`2bffc9a` → `3a5f192`）。**别以为 rebase 只换 parent**：一条提交是**整个目录的全量快照**（01 章），
  新底本 `main` 多了一个 `README.md`，重放出来的快照里必然也带着它。`git diff --stat 旧 新` 只有 `README.md | 1 +`
  一行，恰好证明「这条提交自己做的事一点没变」。
- 所以「tree 变了」≠「你改的东西变了」。判断同一份改动用的是 **patch-id**：
  它只看加了哪些行、删了哪些行，忽略行号和上下文。上面两条的 patch-id 都是
  `c06f1ce6b3adca8acd63855b31ed9210557c5773` —— **同一个补丁**。
- git 就是靠 patch-id 认出「这份补丁上游已经有了」的（场景 ⑤），也是靠它知道重放前后是同一条提交。

**真实输出 C：只把 parent 挪一格，哈希就完全不同**

```text
$ printf 'tree %s\nparent %s\nauthor 学习者 <learn@example.com> 1767232920 +0800\ncommitter 学习者 <learn@example.com> 1767232920 +0800\n\n完全一样的说明\n' "$(git rev-parse main^{tree})" "$(git rev-parse main)" | git hash-object -t commit --stdin
3c46058f62c1871094cef1f456495184c9737fd9
[退出码 0]

$ printf 'tree %s\nparent %s\nauthor 学习者 <learn@example.com> 1767232920 +0800\ncommitter 学习者 <learn@example.com> 1767232920 +0800\n\n完全一样的说明\n' "$(git rev-parse main^{tree})" "$(git rev-parse main^)" | git hash-object -t commit --stdin
34a7b785cdfc2a06d9634a934614ab20fa9d74b7
[退出码 0]
```

- 两条命令的 `tree`、`author`、`committer`、说明**全同**，只有 `parent` 那一行不同 → 结果哈希毫无关系。
  内容寻址仓库**没有「就地修改」这种操作**，所谓改写历史 = 造新对象 + 挪指针，一次都没有例外。
- 顺带看一眼 rebase 留下的流水（`git reflog -8`，H 节靠它救人）：

```text
$ git reflog -8
1c8ca34 HEAD@{0}: rebase (finish): returning to refs/heads/topic
1c8ca34 HEAD@{1}: rebase (pick): t2：改 notes.txt 第2行
8357104 HEAD@{2}: rebase (pick): t1：加 api.js（main 完全没碰这个文件）
8b40fae HEAD@{3}: rebase (start): checkout main
110e005 HEAD@{4}: checkout: moving from main to topic
8b40fae HEAD@{5}: checkout: moving from main to main
8b40fae HEAD@{6}: commit: m1：main 加了 README.md
54eff0e HEAD@{7}: checkout: moving from topic to main
[退出码 0]
```

---

## ⑤ 「我那条提交怎么自己不见了」：上游已有同样的补丁 → 自动跳过

**什么情况**
你手工抄过一次（或者同事以另一条提交合进了同样的改动），再 rebase 时你那一条**被无声跳过**，
只剩一句 `warning:`。有人以为丢了，其实没有。

**敲哪几条**

```bash
git cherry -v main side         # 提前预告：减号=上游已有（会跳过），加号=会重放
git rebase main
git log --oneline --format="%h %s" --all
```

**真实输出**（沙箱 `c-dup/`：`side` 加了一个 `h.txt`，`main` 也加了一份**内容完全一样**的 `h.txt`）

```text
$ git cherry -v main side
- 3efea2ad7af9a0ca6fdc8be152ccbc929ae0b1fb side1：加 h.txt
[退出码 0]

$ git rebase main
warning: skipped previously applied commit 3efea2a
hint: use --reapply-cherry-picks to include skipped commits
hint: Disable this message with "git config set advice.skippedCherryPicks false"
Successfully rebased and updated refs/heads/side.
[退出码 0]

$ git log --oneline --format="%h %s" --all
b58c5cc m1：我自己也加了一份内容完全一样的 h.txt
6e1c5ab c1 起点
[退出码 0]
```

**逐行解读**

- `git cherry` 行首是**减号** = 这份补丁上游已经有了（比的就是 ④ 里的 patch-id）。它提前把结局告诉你了。
- `warning: skipped previously applied commit 3efea2a` —— 注意是 `warning` 不是 `error`，
  退出码仍然 0，rebase 正常完成。
- 最后一张图里 `side` 分支已经不见了：它**被快进到了 main 的位置**（重放后没有任何新内容可加），
  所以 `--all` 只剩 main 那条线。**不是丢提交，是「你的改动上游已经有了」。**
- 真想硬留那份重复历史，光加提示里教的 `--reapply-cherry-picks` **还不够**（真实输出见场景 ⑤ 后面那一段）。

---

## ⑤b 想硬留着那条重复提交：`--reapply-cherry-picks` 单独用不管事

**什么情况**
你就是要「哪怕上游已经有同样的改动，我这条提交也要留在历史里」（例如为了追溯谁什么时候做的决定）。

**真实输出**（沙箱 `c-dup-keep/`，和 ⑤ 一模一样的局面，只是换了开关）

```text
$ git rebase --reapply-cherry-picks main
Rebasing (1/1)
dropping d3448015c3ae3680191ba9278bc9a594da9832be side1：加 h.txt -- patch contents already upstream
Successfully rebased and updated refs/heads/side.
[退出码 0]

$ git reset -q --hard ORIG_HEAD && git log --format="%h %s" -2
d344801 side1：加 h.txt
e66b26f c1 起点
[退出码 0]

$ git rebase --reapply-cherry-picks --empty=keep main
Rebasing (1/1)
Successfully rebased and updated refs/heads/side.
[退出码 0]

$ git show --stat --format="%h %s" HEAD | cat
5368743 side1：加 h.txt
[退出码 0]
```

**逐行解读**

- 第一次：`--reapply-cherry-picks` 只是让它**不再预警「skipped previously applied」**，
  但补丁落到新底本上变成空改动，于是 git 直接回一句 `dropping … patch contents already upstream`。
- 第二次：`--reapply-cherry-picks --empty=keep` 两个一起用才留得下来 —— 最后一张图里
  `git show --stat` **一行文件都没列**，这就是一条真正的空提交（说明还在，改动为空）。
- 中间那句 `git reset --hard ORIG_HEAD` 顺带证明：rebase 完成后也能整条撤回（㉓ 讲透这颗药）。

---

## ⑥ rebase 撞车：先学会读现场（`UU` / 冲突标记 / `:1:2:3:`）

**什么情况**
`git rebase main` 停住了，一屏 `CONFLICT` + 五行 `hint:`。你不确定现在算什么状态、手该往哪放。

**现场怎么造**（沙箱 `c-conflict/`：topic 的 `t1 t2` 和 main 的 `m1` 都改了 `shared.txt` 第 2 行）

```text
$ git log --oneline --graph --decorate --all
* 6e6b291 (main) m1：第2行改成 MAIN
| * 7711deb (HEAD -> topic) t3：加 logo.png（main 没碰过，肯定不冲突）
| * cc4eb0b t2：第2行再改一次
| * 7621685 t1：第2行改成 TOPIC
|/  
* 0c2b475 c1 共同的起点
[退出码 0]

$ git rebase main
Rebasing (1/3)
Auto-merging shared.txt
CONFLICT (content): Merge conflict in shared.txt
error: could not apply 7621685... t1：第2行改成 TOPIC
hint: Resolve all conflicts manually, mark them as resolved with
hint: "git add/rm <conflicted_files>", then run "git rebase --continue".
hint: You can instead skip this commit: run "git rebase --skip".
hint: To abort and get back to the state before "git rebase", run "git rebase --abort".
hint: Disable this message with "git config set advice.mergeConflict false"
Could not apply 7621685... # t1：第2行改成 TOPIC
[退出码 1]
```

**读现场三眼**

```text
$ git status --short
UU shared.txt
[退出码 0]

$ cat shared.txt
第1行：起点
<<<<<<< HEAD
第2行：MAIN 改的
=======
第2行：TOPIC 改的
>>>>>>> 7621685 (t1：第2行改成 TOPIC)
[退出码 0]

$ git ls-files -u
100644 fac85697852e8f96349b5b682ce1bbab7f4984c8 1	shared.txt
100644 32ccb0004aa8b9f0819252ab2f4cd5a77cc54f2d 2	shared.txt
100644 63416383e5b27a37ec571f9d22e2f65fb5d61214 3	shared.txt
[退出码 0]

$ echo "== :2: = 底本（onto 那一侧，也就是 main）=="; git cat-file -p ":2:shared.txt"
== :2: = 底本（onto 那一侧，也就是 main）==
第1行：起点
第2行：MAIN 改的
[退出码 0]

$ echo "== :3: = 正在被重放的那条提交（你的 topic）=="; git cat-file -p ":3:shared.txt"
== :3: = 正在被重放的那条提交（你的 topic）==
第1行：起点
第2行：TOPIC 改的
[退出码 0]
```

**逐行解读**

- 退出码 1，报错的是 git，但它把**三条出路**原样写在 `hint:` 里：解决 → `git add` → `--continue`；
  这条不要 → `--skip`；整个作废 → `--abort`。三条都跑过，见 ⑦⑧⑨。
- `error: could not apply 7621685...` 里的哈希是**你原来那条老提交**（正在重放的原件），不是新提交。
  所以「同一个冲突在 rebase 里会以不同哈希反复出现」——因为每条被重放的提交各撞一次。
- ★ **和 merge 正好相反**：`<<<<<<< HEAD` 那一半是**你落到的底本（main）**，`>>>>>>> 7621685 (…)` 那一半才是**你自己那条提交**。
  因为 rebase 把你的改动当成「外来户」往 main 上重放（04 章把这个坑讲透过）。
  于是 `git checkout --ours` 在 rebase 里取的是**对方的内容**，`--theirs` 取的才是**你的** —— 名字反过来用一次就记住了。
- `git ls-files -u` 的三个 stage：`:1:` 共同祖先、`:2:` ours（此处=底本 main）、`:3:` theirs（此处=你那条提交）。
  想「原封不动拿某一边的版本」，`git cat-file -p ":2:文件名"` / `":3:文件名"` 比翻标记更准。

---

## ⑦ 结局一：手工解决 → `git add` → `git rebase --continue`

**什么情况**
冲突你来拍板。但你改完文件直接 `--continue`，git 回了句你看不懂的话。

**真实输出**（接 ⑥ 的现场，继续往下跑）

```text
$ git rebase --continue
shared.txt: needs merge
You must edit all merge conflicts and then
mark them as resolved using git add
[退出码 1]

$ cat shared.txt
第1行：起点
第2行：手工合并后的最终版
[退出码 0]

$ git add shared.txt
[退出码 0]

$ git status --short
M  shared.txt
[退出码 0]

$ git rebase --continue
[detached HEAD c4523bd] t1：第2行改成 TOPIC
 1 file changed, 1 insertion(+), 1 deletion(-)
Rebasing (2/3)
Auto-merging shared.txt
CONFLICT (content): Merge conflict in shared.txt
error: could not apply cc4eb0b... t2：第2行再改一次
hint: Resolve all conflicts manually, mark them as resolved with
hint: "git add/rm <conflicted_files>", then run "git rebase --continue".
hint: You can instead skip this commit: run "git rebase --skip".
hint: To abort and get back to the state before "git rebase", run "git rebase --abort".
hint: Disable this message with "git config set advice.mergeConflict false"
Could not apply cc4eb0b... # t2：第2行再改一次
[退出码 1]
```

**逐行解读**

- 第一次 `--continue` 被拒：`needs merge` / `You must edit all merge conflicts and then mark them as resolved using git add`。
  **这一步什么都没改变**，rebase 还停在原地 —— `git add` 不是可省的动作，它是「这个文件我处理过了」的**报告动作**。
- 第二次 `--continue` 才真的造出提交：`[detached HEAD c4523bd]` —— 此刻 HEAD 是**游离指针**，
  这条新提交还没被任何分支接住；等整个 rebase 收尾时 git 才会把 `topic` 挪过来（见 ⑧ 的 `branch -v`）。
- 注意它**没有停下来说「恭喜」**，而是径直往下重放，走到 `t2` 又撞了同一行。
  这就是 rebase 冲突的常态：**同一段代码可能让你解好几遍**（每条提交各解一次），不是你做错了什么。
- `UU` → `M` 这个变化值得盯一次：`git status --short` 第一格是暂存区、第二格是工作区，
  `UU` 表示两边都没拍板，`M ` 表示「我决定了，等着被提交」。

---

## ⑧ 结局二：这条提交我不要了 → `git rebase --skip`

**什么情况**
第 2 次撞车之后你意识到：`t2` 那条改动本来就是临时的，不要也罢。

**真实输出**（接 ⑦，正停在 `t2` 的冲突上）

```text
$ git log --format="%h %s" -3
c4523bd t1：第2行改成 TOPIC
6e6b291 m1：第2行改成 MAIN
0c2b475 c1 共同的起点
[退出码 0]

$ git rebase --skip
Rebasing (3/3)
Successfully rebased and updated refs/heads/topic.
[退出码 0]

$ git log --format="%h %s" -4
b0b2397 t3：加 logo.png（main 没碰过，肯定不冲突）
c4523bd t1：第2行改成 TOPIC
6e6b291 m1：第2行改成 MAIN
0c2b475 c1 共同的起点
[退出码 0]

$ git rev-parse --short ORIG_HEAD
7711deb
[退出码 0]

$ git branch -v | cat
  main  6e6b291 m1：第2行改成 MAIN
* topic b0b2397 t3：加 logo.png（main 没碰过，肯定不冲突）
[退出码 0]
```

**逐行解读**

- `--skip` 的字面意思是「这条不重放了」，不是「跳过冲突回头再补」。上面第二张 log 里 `t2` **整个消失**，
  `t3` 照常重放成 `b0b2397`，rebase 收尾并把 `topic` 指针挪过去。
- 如果你只是不想在这一行上做决定，**别用 `--skip`**，用 `--abort`（⑨）；`--skip` 是删提交级别的 operation。
- `ORIG_HEAD = 7711deb` 正是 rebase 开始前 `topic` 的位置 —— 想反悔，`git reset --hard ORIG_HEAD`（㉓ 现场演）。

---

## ⑨ 结局三：整个 rebase 作废 → `git rebase --abort`（完全复原，不是尽量还原）

**什么情况**
撞到一半发现局面看不懂，或者根本不该现在动。你想退回 rebase 之前，一丝痕迹不留。

**真实输出**（新仓库 `c-abort/`，同样的局面重演一次）

```text
$ git log --format="%h %s" main..topic
56d1f43 t3：加 logo.png（main 没碰过，肯定不冲突）
2924f32 t2：第2行再改一次
d285a90 t1：第2行改成 TOPIC
[退出码 0]

$ git rebase main
Rebasing (1/3)
Auto-merging shared.txt
CONFLICT (content): Merge conflict in shared.txt
error: could not apply d285a90... t1：第2行改成 TOPIC
hint: Resolve all conflicts manually, mark them as resolved with
hint: "git add/rm <conflicted_files>", then run "git rebase --continue".
hint: You can instead skip this commit: run "git rebase --skip".
hint: To abort and get back to the state before "git rebase", run "git rebase --abort".
hint: Disable this message with "git config set advice.mergeConflict false"
Could not apply d285a90... # t1：第2行改成 TOPIC
[退出码 1]

$ cat shared.txt
第1行：起点
<<<<<<< HEAD
第2行：MAIN 改的
=======
第2行：TOPIC 改的
>>>>>>> d285a90 (t1：第2行改成 TOPIC)
[退出码 0]

$ git rebase --abort
[退出码 0]

$ git rev-parse --short topic
56d1f43
[退出码 0]

$ git log --format="%h %s" main..topic
56d1f43 t3：加 logo.png（main 没碰过，肯定不冲突）
2924f32 t2：第2行再改一次
d285a90 t1：第2行改成 TOPIC
[退出码 0]

$ cat shared.txt
第1行：起点
第2行：TOPIC 又改了一次
[退出码 0]
```

**逐行解读**

- 冲突标记还在文件里的时候敲 `--abort`，之后：分支指针回到 `56d1f43`（和第一张 log 一模一样）、
  提交链原样恢复、**文件里连冲突标记都没了**（最后一张图 = rebase 前 `t2` 的工作区内容）。
- `--abort` 是**完全复原**，不是「尽量还原」。所以拿不准就 abort，把 `main..topic` 看清楚再来一次，成本几乎为零。
- 它只在 rebase **还没结束**时可用。跑完之后要用 `ORIG_HEAD` / `reflog`（㉓）。

---

## ⑩ 第四个选项 `--quit`：只删状态，把人留在半路（日常别用）

**什么情况**
你想「先退出来」，于是顺手敲了 `--quit`。结果 `git status` 说你游离在某个提交上，文件里还有冲突标记。

**真实输出**（新仓库 `c-quit/`，停在第一个冲突上）

```text
$ git rebase --quit
[退出码 0]

$ git status -sb | head -2
## HEAD (no branch)
UU shared.txt
[退出码 0]

$ ls .git/rebase-merge
ls: cannot access '.git/rebase-merge': No such file or directory
[退出码 2]

$ git rebase --abort
fatal: no rebase in progress
[退出码 128]
```

**逐行解读**

- `--abort` = 回到 rebase 之前；`--quit` = **只把状态目录删掉**，HEAD 留在半路上（`## HEAD (no branch)`），
  冲突标记原样留在文件里。上面第三、第四条命令就是后果：状态没了 → 想 abort 都 abort 不了。
- 它的正经用途只有一个：「我要用别的办法收尾」（自己 `git reset`、或者换个工具继续）。
  日常手滑用了它，剩下的只有懵 —— 所以这一节把它和 `--abort` 放在一起，让你**认得出这个事故**。
- 已经 `--quit` 了怎么办：`git rebase --abort` 失效，但 `ORIG_HEAD` 还在。
  `git reset --hard ORIG_HEAD`（先确认 `git log --oneline -3 ORIG_HEAD` 是你要的那个位置），
  然后 `git checkout topic` 把分支指针重新接上（分支指针在 rebase 半路时并没动，动的是 HEAD）。

---

## ⑪ `rebase -i` 打开的到底是什么（清单原文，逐字）

**什么情况**
你不敢用 `git rebase -i`：一是以为要有图形编辑器，二是不知道它到底让你改什么。

**先说清楚前提**：`git rebase -i` 会打开**两个**「编辑器」，是两件事：

| 环境变量 | 改的文件 | 什么时候用到 |
|---|---|---|
| `GIT_SEQUENCE_EDITOR` | 那份 `pick` 清单 | **每次** `rebase -i` 都打开 |
| `GIT_EDITOR` | 提交说明 | 只有 `squash` / `reword` / `fixup -c` / `edit` 时才打开 |

**真实输出**（新仓库 `d-todo/`，五条提交；`GIT_SEQUENCE_EDITOR` 指到一个「只打印不改」的沙箱脚本）

```text
$ git log --format="%h %s" main..topic
79bd58e 第5条：加配置项
9e8f468 第4条：加日志
0a4422d 第3条：加表单校验
c9999d8 第2条：改一处笔误
9586b4d 第1条：加登录骨架
[退出码 0]

$ GIT_SEQUENCE_EDITOR='<沙箱>/seq-show.sh' git rebase -i main

>>> 下面是 git rebase -i 打开的那个「编辑器」实际收到的文件内容（逐字）：
pick 9586b4d # 第1条：加登录骨架
pick c9999d8 # 第2条：改一处笔误
pick 0a4422d # 第3条：加表单校验
pick 9e8f468 # 第4条：加日志
pick 79bd58e # 第5条：加配置项

# Rebase fad2522..79bd58e onto fad2522 (5 commands)
#
# Commands:
# p, pick <commit> = use commit
# r, reword <commit> = use commit, but edit the commit message
# e, edit <commit> = use commit, but stop for amending
# s, squash <commit> = use commit, but meld into previous commit
# f, fixup [-C | -c] <commit> = like "squash" but keep only the previous
#                    commit's log message, unless -C is used, in which case
#                    keep only this commit's message; -c is same as -C but
#                    opens the editor
# x, exec <command> = run command (the rest of the line) using shell
# b, break = stop here (continue rebase later with 'git rebase --continue')
# d, drop <commit> = remove commit
# These lines can be re-ordered; they are executed from top to bottom.
#
# If you remove a line here THAT COMMIT WILL BE LOST.
#
# However, if you remove everything, the rebase will be aborted.
#
Successfully rebased and updated refs/heads/topic.
[退出码 0]
```

（上面这屏**从中间截了一段**：`# d, drop` 之后 git 还印了 label / reset / merge / update-ref 四条注释，
本章没搬进来 —— 搬进来这一屏就 40 行了。围栏里留下的每一行都是 demo 输出里逐字出现过的。）

**逐行解读**

- ★ **清单顺序和 `git log` 相反**：log 从新到旧（`第5条` 在最上面），清单从**旧到新**（`第1条` 在最上面）。
  「第 2 行」在两个视图里是不同的提交，这是 `-i` 事故榜第一名。
- 清单的每一行就是「这条提交怎么处理」。`pick` 原样重放，`reword` 只改说明，`edit` 停下让你改，
  `squash` 并进前一条并合并说明，`fixup` 并进前一条并**丢掉**自己的说明，`drop`（或直接删这一行）丢提交，
  `exec` 在那个位置跑一条命令。
- 两句注释必须背下来：`These lines can be re-ordered`（可以重排）、
  `If you remove a line here THAT COMMIT WILL BE LOST.`（**删一行 = 丢一条提交**）—— ㉑ 的事故就是这句的现场版。
- 这次一个字都没改，所以结尾还是 `Successfully rebased…`，五条提交的哈希**没变**（因为底本没变，
  `git log --format='%h %s' main..topic` 和动手前完全一致）。这就是「打开清单原样关掉」的真实后果：无事发生。

---

## ⑫ fixup / squash / reword：三种「并一条」的区别就在说明上

**什么情况**
三条 WIP（「加登录骨架」「改一处笔误」「加日志」「加配置项」）想合并；或者说明写得太烂想改。

**真实输出 A：fixup（第 2 条并进第 1 条，只留第 1 条的说明）**

```text
$ git log --format="%h %s" main..topic
13166fa 第5条：加配置项
b2659f6 第4条：加日志
fe2db8d 第3条：加表单校验
58c2836 第2条：改一处笔误
e2446b2 第1条：加登录骨架
[退出码 0]

$ GIT_SEQUENCE_EDITOR='sed -i "2s/^pick/fixup/"' git rebase -i main
Rebasing (2/5)
Rebasing (3/5)
Rebasing (4/5)
Rebasing (5/5)
Successfully rebased and updated refs/heads/topic.
[退出码 0]

$ git log --format="%h %s" main..topic
92e7e47 第5条：加配置项
45b9646 第4条：加日志
b62e603 第3条：加表单校验
04d3ac9 第1条：加登录骨架
[退出码 0]

$ git show --stat --format="%h %s" HEAD~3 | cat
04d3ac9 第1条：加登录骨架

 login.js | 2 ++
 1 file changed, 2 insertions(+)
[退出码 0]
```

**真实输出 B：squash（第 5 条并进第 4 条，两条说明拼在一起）**

```text
$ GIT_SEQUENCE_EDITOR='sed -i "5s/^pick/squash/"' git rebase -i main
Rebasing (5/5)
[detached HEAD 714d8ca] 第4条：加日志
 Date: Thu Jan 1 10:54:00 2026 +0800
 2 files changed, 2 insertions(+)
 create mode 100644 cfg.txt
 create mode 100644 log.js
Successfully rebased and updated refs/heads/topic.
[退出码 0]

$ git log --format="%h %s" main..topic
714d8ca 第4条：加日志
b3fb68e 第3条：加表单校验
d855bad 第2条：改一处笔误
47d867d 第1条：加登录骨架
[退出码 0]

$ git show -s --format=%B HEAD
第4条：加日志

第5条：加配置项
[退出码 0]
```

**真实输出 C：reword（只换说明，内容一行没动）**

```text
$ GIT_SEQUENCE_EDITOR='sed -i "1s/^pick/reword/"' GIT_EDITOR='sed -i "1s|.*|第1条：加登录骨架（reword 重写过的说明）|"' git rebase -i main
Rebasing (1/5)
[detached HEAD 42535db] 第1条：加登录骨架（reword 重写过的说明）
 Date: Thu Jan 1 10:57:00 2026 +0800
 1 file changed, 1 insertion(+)
 create mode 100644 login.js
Rebasing (2/5)
Rebasing (3/5)
Rebasing (4/5)
Rebasing (5/5)
Successfully rebased and updated refs/heads/topic.
[退出码 0]

$ git log --format="%h %s" main..topic
42ace3c 第5条：加配置项
5f43273 第4条：加日志
ee53aea 第3条：加表单校验
d42fe5a 第2条：改一处笔误
42535db 第1条：加登录骨架（reword 重写过的说明）
[退出码 0]
```

**逐行解读**

- **`sed -i "2s/^pick/fixup/"` 就是全部的魔法**：它改的正是 ⑪ 里那份清单文件，把第 2 行的命令字换掉。
  没有图形界面、没有交互，`rebase -i` 一样跑完。你手动操作时改的是同一个文件、同样的内容。
- fixup 的结果：`第2条` 从 log 里**彻底消失**，`04d3ac9`（第1条）的 `--stat` 里 `login.js | 2 ++` ——
  两次改动合在一条里，说明一点额外痕迹不留。适合「补自己上一条提交的漏」。
- squash 的结果：`714d8ca` 的说明是 `第4条：加日志` + 空行 + `第5条：加配置项`（`%B` = 完整说明）。
  这里 `GIT_EDITOR=true` 等于「直接用 git 预先写好的那份合并稿」；你手动操作时看到的是同一个待编辑的说明文件。
- reword：注意**一条命令里挂了两个编辑器**，`GIT_SEQUENCE_EDITOR` 管清单、`GIT_EDITOR` 管说明。
  结果只有 `第1条` 的说明换了，内容 `1 file changed, 1 insertion(+)` 原样。
- 三条场景的共同点：**改过的提交及其后所有提交的哈希都变了**（③④ 讲过为什么）。
  对比 A 组前后两栏：`第3/4/5条` 全换新哈希，哪怕它们自己的内容一个字没动 —— 祖先换了。

**手动操作时你其实只要记住这张表**

| 你想要 | 清单里写 | 会弹说明编辑器吗 |
|---|---|---|
| 把补漏的那条并进上一条，不留痕 | `fixup <哈希>` | 不弹 |
| 把两条并成一条，说明合并 | `squash <哈希>` | 弹（编辑合并稿） |
| 只改说明 | `reword <哈希>` | 弹（编辑这一条） |
| 停在这条上，改内容 + 改说明 | `edit <哈希>` | 不弹，改成停下来 |
| 丢掉这条 | `drop <哈希>`（或直接删行） | 不弹 |
| 在这个位置跑条命令 | `exec <命令>` | 不弹 |

---

## ⑬ 重排顺序，以及 `edit` 停下来的那一刻（amend 和 rebase 的接头处）

**什么情况**
「加配置项」这条其实该最先做；或者你发现第 1 条提交里少写了一个文件、说明也没写清。

**真实输出 A：把清单最后一条挪到第一行**（`GIT_SEQUENCE_EDITOR` 指到沙箱里一个 20 行的小脚本）

```text
$ git log --format="%h %s" main..topic
9ac5f02 第5条：加配置项
cd75441 第4条：加日志
371f6b8 第3条：加表单校验
c8dfd2c 第2条：改一处笔误
d05a724 第1条：加登录骨架
[退出码 0]

$ GIT_SEQUENCE_EDITOR='<沙箱>/seq-move-last-to-first.sh' git rebase -i main

>>> 我已经把清单最后一条挪到了第一行（其余顺序不变），git 会照新顺序重放
Rebasing (1/5)
Rebasing (2/5)
Rebasing (3/5)
Rebasing (4/5)
Rebasing (5/5)
Successfully rebased and updated refs/heads/topic.
[退出码 0]

$ git log --format="%h %s" main..topic
dea352f 第4条：加日志
d0f3530 第3条：加表单校验
8465386 第2条：改一处笔误
1b8c0f6 第1条：加登录骨架
8d82dfb 第5条：加配置项
[退出码 0]
```

**真实输出 B：`edit` 停下来，看状态目录，再 amend**

```text
$ GIT_SEQUENCE_EDITOR='sed -i "1s/^pick/edit/"' git rebase -i main
Rebasing (1/5)
Stopped at 9ea316a...  # 第1条：加登录骨架
You can amend the commit now, with

  git commit --amend 

Once you are satisfied with your changes, run

  git rebase --continue
[退出码 0]

$ git status | head -12
interactive rebase in progress; onto 27db184
Last command done (1 command done):
   edit 9ea316a # 第1条：加登录骨架
Next commands to do (4 remaining commands):
   pick 188527e # 第2条：改一处笔误
   pick 3326fb1 # 第3条：加表单校验
  (use "git rebase --edit-todo" to view and edit)
You are currently editing a commit while rebasing branch 'topic' on '27db184'.
  (use "git commit --amend" to amend the current commit)
  (use "git rebase --continue" once you are satisfied with your changes)

nothing to commit, working tree clean
[退出码 0]

$ echo "onto       = $(git rev-parse --short $(cat .git/rebase-merge/onto))"
onto       = 27db184
[退出码 0]

$ echo "orig-head  = $(git rev-parse --short $(cat .git/rebase-merge/orig-head))"
orig-head  = bdbfb31
[退出码 0]

$ echo "head-name  = $(cat .git/rebase-merge/head-name)"
head-name  = refs/heads/topic
[退出码 0]

$ git commit --amend -m "第1条：加登录骨架（在 rebase 中途顺手补全说明）"
[detached HEAD 92a59a6] 第1条：加登录骨架（在 rebase 中途顺手补全说明）
[退出码 0]

$ git rebase --continue
Rebasing (2/5)
Rebasing (3/5)
Rebasing (4/5)
Rebasing (5/5)
Successfully rebased and updated refs/heads/topic.
[退出码 0]
```

**逐行解读**

- A：`第5条` 跑到了 log **最底下**（`8d82dfb`），成了这条线最老的提交 —— 因为清单是从上往下执行的，
  它现在排在第一行。**顺序由清单决定，不由你以为的时间决定。**
- A 的代价：重排只有在提交**互不依赖**时才安全。要是第 5 条改的文件是第 4 条创建的，
  挪到前面就必然撞冲突（git 只能按顺序重放，它不会替你补中间步骤）。
- B：`Stopped at …` 退出码是 **0** —— 这不是错误，是 git 老实等你。`git status` 第一行
  `interactive rebase in progress; onto 27db184` 已经把局面说干净了。
- ★ `rebase` 的「进行中」状态**不是内存里的魔法，就是 `.git/rebase-merge/` 里这一堆文件**：
  `onto`（新底本）、`orig-head`（搬家前你在那）、`head-name`（要归还给哪条分支）、还有剩下的清单。
  想明白这点，你就明白为什么能在另一个终端里查、为什么中途断电/关终端也不会丢。
- 停下来时 HEAD 是**游离指针**（`[detached HEAD 92a59a6]`）。此刻 `git commit --amend` 只改这一条；
  `--continue` 之后 git 会**把后面的提交按新底本重新重放一遍**，所以第 2~5 条的哈希全变（对比 ⑫ 的输出）。
- `edit` 里你可以做比 amend 更多的事：`git add` 补文件、`git reset HEAD^` 把这条拆成两条、
  甚至直接改文件再提交 —— 但记住：**你能在这一步做的事，rebase 全程都能做**，它只是给你按了暂停键。

---

## ⑭ 日常最省事的组合：`git commit --fixup` + `rebase -i --autosquash`

**什么情况**
第 3 条提交写完，过了两天发现有个小毛病。你不想每次都为一个小补丁开一次交互式 rebase、
也不想手工数「这条该并到第几条后面」。

**真实输出**（新仓库 `d-autosquash/`，先造一条 `fixup!` 提交，再让 git 自己认）

```text
$ git rev-parse --short HEAD~2
4282f11
[退出码 0]

$ git commit -q --fixup=4282f11 && git log --format='%h %s' -3
7d5626c fixup! 第3条：加表单校验
37e9db3 第5条：加配置项
b501aea 第4条：加日志
[退出码 0]

$ GIT_SEQUENCE_EDITOR='<沙箱>/seq-show.sh' GIT_EDITOR=true git rebase -i --autosquash main

>>> 下面是 git rebase -i 打开的那个「编辑器」实际收到的文件内容（逐字）：
pick ae74f5d # 第1条：加登录骨架
pick 30274d9 # 第2条：改一处笔误
pick 4282f11 # 第3条：加表单校验
fixup 7d5626c # fixup! 第3条：加表单校验
pick b501aea # 第4条：加日志
pick 37e9db3 # 第5条：加配置项

# Rebase bcbd9d9..7d5626c onto bcbd9d9 (6 commands)
Rebasing (4/6)
Rebasing (5/6)
Rebasing (6/6)
Successfully rebased and updated refs/heads/topic.
[退出码 0]

$ git log --format="%h %s" main..topic
f9e3308 第5条：加配置项
0e3ed7d 第4条：加日志
e70dd4e 第3条：加表单校验
30274d9 第2条：改一处笔误
ae74f5d 第1条：加登录骨架
[退出码 0]
```

（中间那个注释块和 ⑪ 那一屏逐字相同，省掉没搬 —— 屏幕上只留「git 自己排好的那 6 行命令」，
因为它才是这一节要你看的东西。围栏里剩下的每一行都在 demo 输出里真出现过。）

**逐行解读**

- `git commit --fixup=4282f11` 帮你造的提交说明就是 **`fixup! 第3条：加表单校验`** ——
  这行字是 `--autosquash` 唯一的匹配依据（它按说明文字找目标，不是按血缘）。
- 看清单第 4 行：**`fixup 7d5626c` 已经被 git 自动插到第 3 条后面了**，你一行都不用动。
  这就是 `--autosquash` 干的事：把带 `fixup!`/`squash!` 前缀的提交挪到目标提交下面并改命令字。
- 结果 log 里 `第3条` 变成 `e70dd4e`（内容合并 + 说明保持第 3 条原样），
  `第4/5条` 也换了哈希（祖先变了），`第1/2条` **哈希没变**（`30274d9`/`ae74f5d` 原样）——
  因为它们在改动点之前，重放时 parent 和时间都没变。
- 想让 `fixup` 用自己的说明而不是前一条的：清单里写 `fixup -C`（保留本条说明）或 `fixup -c`（弹编辑器）。
- 嫌每次敲开关麻烦：`git config --global rebase.autosquash true`（**本章没这么做，也没验证**，
  它是 07/10 章会讲的全局配置；临时用 `git -c rebase.autosquash=true rebase -i main` 更安全）。

**drop 的合法用法 vs 事故用法**

```text
$ GIT_SEQUENCE_EDITOR='sed -i "4s/^pick/drop/"' git rebase -i main
Rebasing (5/5)
Successfully rebased and updated refs/heads/topic.
[退出码 0]

$ git log --format="%h %s" main..topic
9950374 第5条：加配置项
1473e85 第3条：加表单校验
0c3ed4b 第2条：改一处笔误
42ea0b9 第1条：加登录骨架
[退出码 0]
```

- 第 4 条被丢掉，这是你**主动**丢的，git 照办，一句错误都没有。
- 但如果丢的**不是**你想丢的那条呢？—— 这就是 ㉑ 的主角。先把这句记住：
  **清单里删一行和写 `drop` 是同一个动作，git 不会二次确认。**

---

## ⑮ `--onto`：只搬中间那一段（普通 rebase 做不到）

**什么情况**
分支里第 2、3 条提交是当时试验用的，现在觉得不该要，但**后面的第 4、5 条要留**。
`rebase -i` 也能删（写两个 `drop`），但如果那两条被后面的提交依赖着，你就会撞一堆冲突；
`--onto` 的用法是**精准摘掉一段**，或者**给分支换个底本**。

**真实输出 A：摘掉中间一段**（`t2 t3` 是废的）

```text
$ git log --format="%h %s" main..topic
e68f045 t5：还有这个
1f04f22 t4：另一个有用的改动
ba3bb5b t3：试验的后续（也不该要）
a91ea40 t2：试验（现在觉得不该要）
0404f10 t1：有用的改动
[退出码 0]

$ echo "新底本 t1 = $(git rev-parse --short topic~4)   分界线 t3 = $(git rev-parse --short topic~2)"
新底本 t1 = 0404f10   分界线 t3 = ba3bb5b
[退出码 0]

$ git rebase --onto $(git rev-parse topic~4) $(git rev-parse topic~2) topic
Rebasing (1/2)
Rebasing (2/2)
Successfully rebased and updated refs/heads/topic.
[退出码 0]

$ git log --format="%h %s" main..topic
569056f t5：还有这个
cad8ea0 t4：另一个有用的改动
0404f10 t1：有用的改动
[退出码 0]

$ git log --oneline --graph --decorate --all
* 569056f (HEAD -> topic) t5：还有这个
* cad8ea0 t4：另一个有用的改动
* 0404f10 t1：有用的改动
* 72803fa (main) c1 起点
[退出码 0]
```

**真实输出 B：分支挂错底本了（hotfix 其实该基于 main）**

```text
$ git log --oneline --graph --decorate --all
* bd510c1 (HEAD -> main) m2：主线往前走
| * 69c18bb (hotfix) hotfix：一个跟老功能无关的独立修复
| * 74c2f1b (old-feature) old2：老功能第二步
| * 2cdb18a old1：老功能第一步
|/  
* 979318f c1 起点
[退出码 0]

$ git rebase --onto main $(git rev-parse old-feature) hotfix
Rebasing (1/1)
Successfully rebased and updated refs/heads/hotfix.
[退出码 0]

$ git log --oneline --graph --decorate --all
* 1fe871f (HEAD -> hotfix) hotfix：一个跟老功能无关的独立修复
* bd510c1 (main) m2：主线往前走
| * 74c2f1b (old-feature) old2：老功能第二步
| * 2cdb18a old1：老功能第一步
|/  
* 979318f c1 起点
[退出码 0]

$ git log --format="%h %s" main..hotfix
1fe871f hotfix：一个跟老功能无关的独立修复
[退出码 0]

$ git log --format="%h %s" main..old-feature
74c2f1b old2：老功能第二步
2cdb18a old1：老功能第一步
[退出码 0]
```

**读法只有一个秘密**

```
git rebase --onto <接到哪（新底本）> <从哪开始不搬（分界线，不含）> <哪条分支>
                     ↑                    ↑                          ↑
                  t1 / main          t3 / old-feature            topic / hotfix

搬的是 (分界线 , 分支头】这一段；区间外的提交一条都不带。第三个参数不写 = 你脚下这条分支。
```

- A：`topic~4` 是 t1（要接到的位置），`topic~2` 是 t3（分界线，**它自己不被搬**）。
  于是被搬的是 t3 之后的 t4、t5 两条（`Rebasing (1/2)` 正好两条，这是验证写法对不对最快的方式），
  `t2 t3` 落在区间外被丢掉，`ls -1` 只剩 `core.txt keep1.txt keep2.txt keep3.txt` —— 试验文件没了。
- B：分界线写 `old-feature`（hotfix 的父提交 = old2），新底本写 `main` → 只搬 `hotfix` 那**一条**。
  对照两张图的 `old-feature` 分支：**一动没动**（还是 `74c2f1b`），只有 `hotfix` 挪到了 `bd510c1` 后面。
  这就是 `--onto` 的主场：普通 `git rebase main` 会把 hotfix **连同** old1/old2 一起搬过来。
- ★ 边界写错是最常见事故：分界线写成 t4 就少搬一条，写成 t2 就多搬一条。
  所以动手前先跑 `git log --oneline <分界线>..<分支>`，**列出来的就是会被搬走的**（① 的规矩在 `--onto` 里同样适用）。

---

## ⑯ cherry-pick：只抄那一条（含 `-x`、范围写法、抄出空提交）

**什么情况**
`hotfix` 分支上第三条正好是线上要的那个修复，前两条还在试验阶段，不能跟着走。

**真实输出**（新仓库 `f-cherry/`：`hotfix` 上 h1 h2 h3，`exp` 上 e1 e2，当前在 main）

```text
$ git log --oneline --graph --decorate --all
* 1c2b364 (HEAD -> main) m1：主线改动
| * 68345e5 (exp) e2：把 tools.c 改成 v2（这个要）
| * 2b66898 e1：试验性改动（别一起要）
| * 335bd1a (hotfix) h3：顺手改了样式
| * 9451888 h2：加工具函数 tools.c
| * 25abbce h1：修一个崩溃
|/  
* ca7cb0c c1 起点
[退出码 0]

$ git cherry-pick $(git rev-parse --short hotfix~1)
[main 2fc49f5] h2：加工具函数 tools.c
 Date: Thu Jan 1 11:44:00 2026 +0800
 1 file changed, 1 insertion(+)
 create mode 100644 tools.c
[退出码 0]

$ git cherry-pick -x $(git rev-parse --short exp~1)
[main 1a72ea1] e1：试验性改动（别一起要）
 Date: Thu Jan 1 11:46:00 2026 +0800
 1 file changed, 1 insertion(+)
 create mode 100644 experiment.c
[退出码 0]

$ git show -s --format=%B HEAD
e1：试验性改动（别一起要）

(cherry picked from commit 2b66898bee8526ac616d2b8cdc7966768d45b457)
[退出码 0]
```

**逐行解读**

- `[main 2fc49f5] h2：…` 这行就是新造的那条：说明照抄、内容照抄、**哈希是新的**（道理和 ③④ 一样：parent 换了）。
- cherry-pick 永远是**追加到你当前分支的尾巴上**，和原提交在哪条分支、排第几都无关。
  抄之前先 `git switch` 到目标分支 —— 这一步忘记，就抄到别的分支上去了（和 ② 同一类方向性错误）。
- `-x` 在说明末尾追加一行 `(cherry picked from commit <完整 40 位哈希>)`。
  跨分支搬修复**强烈建议加上**：将来 `git log` 一眼就知道这改动哪来的，还能用 `git cherry` 对账。
  不加 `-x` 就什么都没有，将来只能靠猜。
- 抄过来的提交**作者保留原人，提交者是你**（`Date:` 那行还是原来的作者时间）。

**真实输出：一次抄一串，以及「抄出空提交」的停法**

```text
$ git cherry-pick main~1..hotfix
[main e393f9f] h1：修一个崩溃
 Date: Thu Jan 1 11:43:00 2026 +0800
 1 file changed, 1 insertion(+)
 create mode 100644 bugfix.c
The previous cherry-pick is now empty, possibly due to conflict resolution.
If you wish to commit it anyway, use:

    git commit --allow-empty

Otherwise, please use 'git cherry-pick --skip'
On branch main
You are currently cherry-picking commit 9451888.
  (all conflicts fixed: run "git cherry-pick --continue")
  (use "git cherry-pick --skip" to skip this patch)
  (use "git cherry-pick --abort" to cancel the cherry-pick operation)

nothing to commit, working tree clean
[退出码 1]

$ git cherry-pick --skip
[main e7ff3bf] h3：顺手改了样式
 Date: Thu Jan 1 11:45:00 2026 +0800
 1 file changed, 1 insertion(+)
 create mode 100644 ui.css
[退出码 0]

$ git log --format="%h %s" -4
e7ff3bf h3：顺手改了样式
e393f9f h1：修一个崩溃
1a72ea1 e1：试验性改动（别一起要）
2fc49f5 h2：加工具函数 tools.c
[退出码 0]
```

- `main~1..hotfix` = 「hotfix 有、`main~1` 没有」= h1 h2 h3，git 按**从旧到新**逐个应用（第一行就是 h1）。
- 抄到 h2（`9451888`）**停住了**：h2 那份补丁在前面已经被我抄过一次，落到当前分支成了**空改动**。
  这就是 ④ 里 patch-id 那套机制的日常表现。
- 出路写在提示里：硬留一条空提交 → `git commit --allow-empty`；不要 → `git cherry-pick --skip`；
  整个序列作废 → `git cherry-pick --abort`。这里选 `--skip`，**sequencer 自己接着把剩下的 h3 抄完**，
  不需要你重新发起 —— 「cherry-pick 是一个序列」就是这个意思。

---

## ⑰ cherry-pick 的两个特殊场合：合并提交必须 `-m`，`-n` 只抄不提交

**真实输出 A：抄一个合并提交**

```text
$ git log --format='%h %p %s' -1
4c2ea3e e7ff3bf 335bd1a 把 hotfix 合进来
[退出码 0]

$ git cherry-pick 4c2ea3e
error: commit 4c2ea3e94716ee70e78df8bce75cb4f00f0910f3 is a merge but no -m option was given.
fatal: cherry-pick failed
[退出码 128]

$ git cherry-pick -m 1 4c2ea3e
The previous cherry-pick is now empty, possibly due to conflict resolution.
If you wish to commit it anyway, use:

    git commit --allow-empty

Otherwise, please use 'git cherry-pick --skip'
On branch merge-host
You are currently cherry-picking commit 4c2ea3e.
  (all conflicts fixed: run "git cherry-pick --continue")
  (use "git cherry-pick --skip" to skip this patch)
  (use "git cherry-pick --abort" to cancel the cherry-pick operation)

nothing to commit, working tree clean
[退出码 1]

$ git cherry-pick --abort
[退出码 0]
```

- `git log --format='%h %p %s'` 里 `%p` 打印了**两个**父亲（`e7ff3bf 335bd1a`）→ 这是个合并提交。
- 不带 `-m` 一定报错：合并提交有两个父亲，git 不知道「以哪个为底本算差异」。**这不是 bug，是它在逼你做决定。**
- `-m 1` = 以第 1 个父亲（当时你脚下那条线）为底本，只把第 2 个父亲带来的差异重放一遍；`-m 2` 反过来。
- 本例 `-m 1` 停在空提交上，因为这份差异早在 ⑯ 就被逐条抄过来了 —— 出路和 ⑯ 一样（这里用 `--abort` 收摊）。

**真实输出 B：`-n`（`--no-commit`）**（新仓库 `f-nocommit/`）

```text
$ git cherry-pick -n $(git rev-parse --short other)
[退出码 0]

$ git status --short
A  big.patch.txt
[退出码 0]

$ git diff --cached --stat | cat
 big.patch.txt | 2 ++
 1 file changed, 2 insertions(+)
[退出码 0]

$ git log --format="%h %s" -1
5e78be2 m1：主线
[退出码 0]

$ ls .git/CHERRY_PICK_HEAD
ls: cannot access '.git/CHERRY_PICK_HEAD': No such file or directory
[退出码 2]

$ git add extra.txt
[退出码 0]

$ git commit -m "把 o1 的改动和我自己的合成一条"
[main 7f9ed9b] 把 o1 的改动和我自己的合成一条
 2 files changed, 3 insertions(+)
 create mode 100644 big.patch.txt
 create mode 100644 extra.txt
[退出码 0]
```

- 三个关键信息：① 分支头还是 `5e78be2`（**没造新提交**）② 改动已经进了**暂存区**（`A ` 第一格）
  ③ 连 `CHERRY_PICK_HEAD` 都没创建。`-n` = 「东西给我，剩下的我自己决定」。
- 用途：别人那一条太大了，你只想借其中一半；或者你想把抄来的改动和自己的改动**合成一条**（上面就是这么干的）。
- ★ `-n` 之后**原提交的说明不会自动带上**（默认 cherry-pick 会带），得自己写；作者信息也不保留，提交的是你。

---

## ⑱ cherry-pick 撞车：还是那三条出路（和 rebase 同一套）

**真实输出**（新仓库 `f-conf/`：`side` 和 `main` 都改了 `tools.c` 同一行）

```text
$ git cherry-pick $(git rev-parse --short side)
Auto-merging tools.c
CONFLICT (content): Merge conflict in tools.c
error: could not apply 21ca9c2... s1：tools.c 改成 SIDE
hint: After resolving the conflicts, mark them with
hint: "git add/rm <pathspec>", then run
hint: "git cherry-pick --continue".
hint: You can instead skip this commit with "git cherry-pick --skip".
hint: To abort and get back to the state before "git cherry-pick",
hint: run "git cherry-pick --abort".
hint: Disable this message with "git config set advice.mergeConflict false"
[退出码 1]

$ git status --short
UU tools.c
[退出码 0]

$ cat .git/CHERRY_PICK_HEAD && echo "↑ 这个文件里记的就是正在被抄的那个提交"
21ca9c2509ac713ca03ff580c7c446b7a53ccff7
[退出码 0]

$ git cherry-pick --continue
error: Committing is not possible because you have unmerged files.
hint: Fix them up in the work tree, and then use 'git add/rm <file>'
hint: as appropriate to mark resolution and make a commit.
fatal: Exiting because of an unresolved conflict.
U	tools.c
[退出码 128]

$ git add tools.c
[退出码 0]

$ git cherry-pick --continue
[main 563623f] s1：tools.c 改成 SIDE
 Date: Thu Jan 1 11:59:00 2026 +0800
 1 file changed, 1 insertion(+), 1 deletion(-)
[退出码 0]
```

**逐行解读**

- 和 ⑦ 一模一样的话术：**没 `git add` 就 `--continue` → 当场拒绝**（`error: Committing is not possible…`）。
- 差别在 `hint:` 的措辞：cherry-pick 的提示里只有 `--continue / --skip / --abort` 三条（没有 `git add` 之外的选项），
  而且**没提 ours/theirs**；冲突现场的 `:1:2:3:` 三个 stage 同样在（读法见 ⑥）。
- `.git/CHERRY_PICK_HEAD` 这个文件里就是「正在被抄的那条」的完整哈希 ——
  和 rebase 的 `.git/rebase-merge/` 是**同一套设计**：状态写在 `.git` 目录里，所以断电、换终端都不丢。
- 记这张对照表：

| 操作 | 继续 | 跳过这一条 | 整个作废 | 状态目录 |
|---|---|---|---|---|
| `rebase`（含 `-i`） | `--continue` | `--skip` | `--abort` | `.git/rebase-merge/` |
| `cherry-pick` | `--continue` | `--skip` | `--abort` | `.git/CHERRY_PICK_HEAD` + sequencer 目录 |
| `merge` | `--continue` | **没有 skip** | `--abort` | `.git/MERGE_HEAD` |

---

## ⑲ `git commit --amend`：改说明、补漏文件，以及它只能改 HEAD

**真实输出**（新仓库 `g-amend/`，本地已有两条提交，`style.css`/`wip.txt` 是故意留着的未跟踪文件）

```text
$ git commit -q --amend -m "第二条：加 utils.js（说明改过了）" && git log --format='%h %p %s' -2
724e25c d4e62b9 第二条：加 utils.js（说明改过了）
d4e62b9  第一条：登录骨架
[退出码 0]

$ git status --short
?? style.css
[退出码 0]

$ git add style.css
[退出码 0]

$ git commit -q --amend --no-edit && git show --stat --format="%h %s" HEAD | cat
94907ef 第二条：加 utils.js（说明改过了）

 style.css | 1 +
 utils.js  | 1 +
 2 files changed, 2 insertions(+)
[退出码 0]

$ git status --short
?? wip.txt
[退出码 0]

$ git commit -q --amend --no-edit && git show --stat --format="%s" HEAD | cat
第二条：加 utils.js（说明改过了）

 style.css | 1 +
 utils.js  | 1 +
 2 files changed, 2 insertions(+)
[退出码 0]

$ git commit --amend -m "试图改更早的提交" HEAD~1
error: pathspec 'HEAD~1' did not match any file(s) known to git
[退出码 1]
```

**逐行解读**

- 只改说明，**哈希照样变**（`5b88cdd → 724e25c`）：说明本身参与哈希计算（④ 里 `hash-object` 那次实验就是它）。
- 漏文件的标准修法三步：**改 → `git add` → `git commit --amend --no-edit`**。
  `--no-edit` = 说明保持原样、不弹编辑器。上面第二次 amend 之后 `style.css` 进了提交。
- ★ amend 只带上**暂存区里的东西**：`wip.txt` 一直是 `??`（没 add），连续两次 amend 都没把它带走。
  这是好事 —— 它意味着你不用怕「amend 会不会把我正在改的半成品一起塞进提交」。
  反过来也要清醒：`git add -A` 之后再 amend 就真的会全带走，所以**别在 amend 前随手 `add -A`**。
- `git commit --amend HEAD~1` 报的是 `pathspec 'HEAD~1' did not match any file(s)` ——
  因为 `HEAD~1` 在这里被当成**文件路径**了。名字骗人：**amend 只认 HEAD 那一条**。
  要改更早的提交，用 ⑫⑬ 的 `rebase -i`（`reword` / `fixup` / `edit`）。
- 另外两条没在 demo 里演但值得记：`--amend --author="名字 <邮箱>"` 只改作者；
  已经 push 出去的提交属于「已发布历史」，见 ⑳。

---

## ⑳ ★ 改写「已经发布出去」的历史：push 被拒那一下，就是代价

**什么情况**
你把已经 push 的那条提交 amend 了（或者 rebase 了已经共享的分支），然后 `git push` 被拒。

**真实输出**（新仓库 `g-amend/`：`origin` 是沙箱里的 bare 仓库 `g-origin.git`，**全程没有联网**；
第一条提交已经 push 上去，然后用 `reset` 退回到那条已发布的提交上再 amend）

```text
$ git push -q -u origin main && echo "本地 main = $(git rev-parse --short main)"
本地 main = d4e62b9
[退出码 0]

$ git reset -q --hard d4e62b9 && git log --format='%h %s' -1
d4e62b9 第一条：登录骨架
[退出码 0]

$ git commit -q --amend -m "第一条：登录骨架（把已发布的也改了）" && git log --format="%h %s" -2
c5c1d27 第一条：登录骨架（把已发布的也改了）
[退出码 0]

$ echo "本地 HEAD     = $(git rev-parse --short HEAD)"
本地 HEAD     = c5c1d27
[退出码 0]

$ echo "origin/main   = $(git rev-parse --short origin/main)"
origin/main   = d4e62b9
[退出码 0]

$ git push
To ../g-origin.git
 ! [rejected]        main -> main (non-fast-forward)
error: failed to push some refs to '../g-origin.git'
hint: Updates were rejected because the tip of your current branch is behind
hint: its remote counterpart. If you want to integrate the remote changes,
hint: use 'git pull' before pushing again.
hint: See the 'Note about fast-forwards' in 'git push --help' for details.
[退出码 1]

$ git --git-dir=<沙箱>/g-origin.git log --format='%h %s' | cat
d4e62b9 第一条：登录骨架
[退出码 0]

$ git push --force-with-lease
To ../g-origin.git
 + d4e62b9...c5c1d27 main -> main (forced update)
[退出码 0]

$ git --git-dir=<沙箱>/g-origin.git log --format='%h %s' | cat
c5c1d27 第一条：登录骨架（把已发布的也改了）
[退出码 0]
```

**逐行解读**

- 被拒的原因是 **non-fast-forward**：你手上这条 `c5c1d27` 不再是远程那条 `d4e62b9` 的「下一代」，
  它是**兄弟**（同一个父亲的另一个孩子）。推上去就得把服务器那条挤掉，所以 git 替你把关、直接拒绝。
- `git --git-dir=…/g-origin.git log` 是**服务器视角**：拒绝之后服务器还稳稳停在 `d4e62b9`，
  什么都没被改掉。这是本章反复要你看的一个习惯 —— **改之前先想清楚对面看到的是什么。**
- `--force-with-lease` 之后：`+ d4e62b9...c5c1d27 main -> main (forced update)`，服务器指针被挪走，
  旧那条提交在服务器上不再属于任何分支（别人的本地仓库里还有，于是开始分叉）。
- `--force-with-lease` 比裸 `-f` 好的地方：它要求「远程还停在我上次看到的位置」，
  同事期间推过新东西它就会拒绝（那一幕在 07 章有完整现场）。
  但它**挡不住你改写自己的历史** —— 它防的是「覆盖别人的工作」，不防「毁掉别人的引用」。
- **真正的代价不是 git 报错**，而是：任何基于 `d4e62b9` 干活的同事，分支会瞬间和你对不上；
  CI 缓存、别人已经 review 的 diff、`git blame` 全部指向一个不存在的哈希。
- ★ 团队铁律（这一章唯一必须背下来的规矩）：
  **只有「没发布过」的提交可以随便 `amend` / `rebase`。发布过的用「往上叠新提交」解决。**
  非改不可（比如误提交密钥）时：先通知所有基于该分支的人，再改写 + `--force-with-lease`。

---

## ㉑ ★★ 事故现场：`rebase` 之后一条提交不见了，git 还回退出码 0

**剧情**（沙箱 `h-rescue/`）
整理历史时想删掉清单里的一行**注释**，`sed` 的模式写宽了，把一条真提交整行删了。
清单里删一行 = `drop`，而那句警告早就写在 ⑪ 的注释里：`If you remove a line here THAT COMMIT WILL BE LOST.`

**真实输出**

```text
$ git log --format="%h %s" main..feature
41f1e09 F3：也有用
6e5d0f3 F2：★ 被我手滑删掉的那条
5fe758a F1：有用的改动
[退出码 0]

$ GIT_SEQUENCE_EDITOR='sed -i "/F2/d"' git rebase -i main
Rebasing (2/2)
Successfully rebased and updated refs/heads/feature.
[退出码 0]

$ git log --format="%h %s" main..feature
36ca839 F3：也有用
5fe758a F1：有用的改动
[退出码 0]

$ git log --oneline --all | grep F2
[退出码 1]

$ git branch -a | cat
* feature
  main
[退出码 0]
```

**逐行解读**

- 看清楚：`Rebasing (2/2)` —— 只剩两条要重放（F1、F3），**F2 已经在清单阶段被删掉了**。
- 然后 rebase **成功完成、退出码 0、一句错误都没有**。git 只是照办：清单里没这行，那就不重放。
  这是本章最可怕的场景：**没有红字来救你**。
- `git log --all | grep F2` 没找到（`grep` 退出码 1）。`--all` 只走**引用**能到的提交，
  此刻确实没有任何引用指向 F2。
- ★ 但「看不见」不等于「不存在」。对象还在 `.git/objects` 里（④ 讲过 rebase 不删旧对象），
  只是暂时没有分支指着它。下面三步就是把它捞回来的完整动作。

---

## ㉒ `git reflog`：git 自己记的流水账，和两种救援路线

**路线 A：找到哈希 → 整条分支复原**

```text
$ git reflog -12
36ca839 HEAD@{0}: rebase (finish): returning to refs/heads/feature
36ca839 HEAD@{1}: rebase (pick): F3：也有用
5fe758a HEAD@{2}: rebase (start): checkout main
41f1e09 HEAD@{3}: commit: F3：也有用
6e5d0f3 HEAD@{4}: commit: F2：★ 被我手滑删掉的那条
5fe758a HEAD@{5}: commit: F1：有用的改动
6c72aa2 HEAD@{6}: checkout: moving from main to feature
6c72aa2 HEAD@{7}: commit (initial): c1 起点
[退出码 0]

$ git reflog | grep -n 'F2'
5:6e5d0f3 HEAD@{4}: commit: F2：★ 被我手滑删掉的那条
[退出码 0]

$ git log --format='%h %s' -3 $(git reflog | grep 'commit: F2' | head -1 | cut -d' ' -f1)
6e5d0f3 F2：★ 被我手滑删掉的那条
5fe758a F1：有用的改动
6c72aa2 c1 起点
[退出码 0]

$ git rev-parse --short ORIG_HEAD
41f1e09
[退出码 0]

$ git branch feature-snapshot ORIG_HEAD
[退出码 0]

$ git reset -q --hard $(git rev-parse feature-snapshot) && git log --format='%h %s' main..feature
41f1e09 F3：也有用
6e5d0f3 F2：★ 被我手滑删掉的那条
5fe758a F1：有用的改动
[退出码 0]

$ cat two.txt
★ 这条被我手滑删掉了（里面有重要代码）
[退出码 0]

$ git reflog -4
41f1e09 HEAD@{0}: reset: moving to 41f1e090b9dabf0311e52ea65d535be1f17c68b5
36ca839 HEAD@{1}: rebase (finish): returning to refs/heads/feature
36ca839 HEAD@{2}: rebase (pick): F3：也有用
5fe758a HEAD@{3}: rebase (start): checkout main
[退出码 0]
```

**读法（四列）**：`<哈希> <位置>@{序号}: <动作>: <说明>`

- `rebase (start)` = 搬家动作的开始，`rebase (pick)` = 每重放一条记一行，`rebase (finish)` = 结果。
  **再往下的 `commit: F3 / F2 / F1` 就是三条原始提交** —— 被删那条 `6e5d0f3` 就在里面。
  也就是说：reflog 不骗你，它老实记下了每一次指针变动。
- ★ 关键动作是**给对象找个新引用**：`git branch feature-snapshot ORIG_HEAD`（零成本保险），
  再 `git reset --hard feature-snapshot` 复原。`two.txt` 里的内容回来了 —— 这就是「真的救回来了」，
  不是只看见了。顺手留的那个快照分支**别急着删**，等确认整理结果没问题了再收拾。
- `ORIG_HEAD = 41f1e09` 是 rebase 开始前 `feature` 的位置，不用数 `@{几}` 就能核对。
  但它**只在改写类动作之后有效**：`rebase`/`merge`/`pull`/`reset --hard` 会写它，普通提交不写；
  而且你多做一次改写它就没了。所以救援现场不要「再试一把」。

**路线 B：整理结果我要保留，只把丢的那一条抄回来**（新仓库 `h-surgical/`）

```text
$ git log --format="%h %s" main..feature
5030c0b G3：也有用
c16f6b1 G1：有用的改动
[退出码 0]

$ git rev-list -n 1 --all --grep='G2'
[退出码 0]

$ git log -1 --format='%h %s %cd' 61e09f2
61e09f2 G2：★ 手滑删掉的那条 Thu Jan 1 12:19:00 2026 +0800
[退出码 0]

$ git cherry-pick 61e09f2
[feature 7aaf8fc] G2：★ 手滑删掉的那条
 Date: Thu Jan 1 12:19:00 2026 +0800
 1 file changed, 1 insertion(+)
 create mode 100644 two.txt
[退出码 0]

$ git log --format="%h %s" main..feature
7aaf8fc G2：★ 手滑删掉的那条
5030c0b G3：也有用
c16f6b1 G1：有用的改动
[退出码 0]
```

- `git rev-list -n 1 --all --grep='G2'` 打印了**空**（退出码还是 0）：`--all` 只看引用可达的对象，
  **reflog 不在引用里**，所以按说明搜也搜不到。→ 找丢失提交的哈希，**只有 reflog 这条路**。
- 拿到 `61e09f2` 之后 `git cherry-pick 61e09f2`：整理结果原样保留，只把丢的那一条贴回尾巴。
  注意它成了**最新**那条（顺序变了）—— 如果顺序要紧，就用 ⑮ 的 `edit`/`rebase -i` 把它插回原位。

**分支自己的 reflog（救分支时比 HEAD 的更好读）**

```text
$ git log -g --format="%h %gd %gs" feature | cat
7aaf8fc feature@{0} cherry-pick: G2：★ 手滑删掉的那条
5030c0b feature@{1} rebase (finish): refs/heads/feature onto 1027d066b28cfe3bfd15771790a88946a9703a4e
76fe3a7 feature@{2} commit: G3：也有用
61e09f2 feature@{3} commit: G2：★ 手滑删掉的那条
c16f6b1 feature@{4} commit: G1：有用的改动
1027d06 feature@{5} branch: Created from HEAD
[退出码 0]

$ tail -3 .git/logs/refs/heads/feature
61e09f274a9d1d08e46dc9c5ba210b4f1fc68f21 76fe3a7a5694eab1d2de65c87e68e92de354412f 学习者 <learn@example.com> 1767241200 +0800	commit: G3：也有用
76fe3a7a5694eab1d2de65c87e68e92de354412f 5030c0b7376c3f08e4ccd5030aedc4abbe03456e 学习者 <learn@example.com> 1767241260 +0800	rebase (finish): refs/heads/feature onto 1027d066b28cfe3bfd15771790a88946a9703a4e
5030c0b7376c3f08e4ccd5030aedc4abbe03456e 7aaf8fc9bdc60937e5d2e72769356c0415aea0b0 学习者 <learn@example.com> 1767241320 +0800	cherry-pick: G2：★ 手滑删掉的那条
[退出码 0]
```

- `HEAD@{n}` 记的是「你**脚下位置**」的流水 —— 切分支、`checkout` 都会掺进去；
  `feature@{n}` 只记「**这条分支指针**」的变动，救分支时更准（`git log -g <分支>` 或直接 `分支名@{1}`）。
- 最下面那张图是 `.git/logs/refs/heads/feature` 的**原文**：`<旧哈希> <新哈希> <名字> <邮箱> <时间戳>\t<动作>: <说明>`。
  reflog 不是什么黑魔法，就是一串纯文本追加日志 —— 也正因为它只是个日志，**它能被删**（㉔）。

---

## ㉓ 三颗后悔药放在一张图里：什么时候用哪颗

**真实输出**（新仓库 `h-pills/`，故意停在冲突半路）

```text
$ git rebase main
Rebasing (1/3)
Auto-merging shared.txt
CONFLICT (content): Merge conflict in shared.txt
error: could not apply ba7dbbc... t1：第2行改成 TOPIC
hint: Resolve all conflicts manually, mark them as resolved with
hint: "git add/rm <conflicted_files>", then run "git rebase --continue".
hint: You can instead skip this commit: run "git rebase --skip".
hint: To abort and get back to the state before "git rebase", run "git rebase --abort".
hint: Disable this message with "git config set advice.mergeConflict false"
Could not apply ba7dbbc... # t1：第2行改成 TOPIC
[退出码 1]

$ echo "rebase 开始前，topic = cf5af6d"
rebase 开始前，topic = cf5af6d
[退出码 0]

$ echo "状态文件 .git/rebase-merge/orig-head = $(cat .git/rebase-merge/orig-head)"
状态文件 .git/rebase-merge/orig-head = cf5af6d18b023aee01c22b2e65123042fd63dde6
[退出码 0]

$ echo "ORIG_HEAD                            = $(git rev-parse ORIG_HEAD)"
ORIG_HEAD                            = cf5af6d18b023aee01c22b2e65123042fd63dde6
[退出码 0]

$ git rebase --abort
[退出码 0]

$ git rev-parse --short topic
cf5af6d
[退出码 0]

$ git rev-parse --short ORIG_HEAD
cf5af6d
[退出码 0]
```

**一句话分清三颗药**

```
①还没结束（冲突 / edit 停住）        → git rebase --abort
   证据：.git/rebase-merge/ 这个目录还在，里面有 orig-head 文件
②已经结束，但还没干别的              → git reset --hard ORIG_HEAD
   证据：.git/ORIG_HEAD 里就是搬家前的哈希
③已经结束，又干了几步别的            → git reflog 往前翻，拿到哈希再 branch / reset / cherry-pick
   证据：.git/logs/... 里逐条记着每次指针变动
```

- 上面第一张和第三张是**同一个值**（`cf5af6d…`）写在两个地方：状态目录里的 `orig-head`
  是「这次 rebase 还没结束」的证据，`.git/ORIG_HEAD` 是「上次改写开始前在哪」。
- `--abort` 之后 `topic` 和 `ORIG_HEAD` 都还在原位 —— 双保险。
- 反过来，rebase **跑完**了再 `--abort` 会拿到 `fatal: no rebase in progress`（⑩ 里演过这条），
  这时候只有 ②③ 两颗药可用。
- ⑤b 里那句 `git reset -q --hard ORIG_HEAD` 就是第 ② 颗药的一次实战使用。

---

## ㉔ 为什么这次 `git fsck` 什么都没报（以及你能放心多久）

**真实输出**

```text
$ git fsck --unreachable
[退出码 0]

$ git config --get gc.reflogExpire; git config --get gc.reflogExpireUnreachable
[退出码 1]
```

**逐行解读**

- `git fsck --unreachable` 输出**空**（退出码 0）。很多人以为「fsck 没报悬空对象 = 东西还在」，
  其实这里因果关系刚好相反：**reflog 还指着那些旧提交，所以 git 认为它们「可达」**，
  于是 fsck 不把它们当悬空对象，`gc` 也不会删。
  —— 换句话说，救你的是 reflog 这道引用，不是对象本身还留着。
- 第二条命令两个查询都没输出、退出码 **1**：本仓库没配这两项，走 git 默认值
  （`gc.reflogExpire` 默认 90 天、`gc.reflogExpireUnreachable` 默认 30 天，**这两个数字来自 git 文档，
  本文没有实测**：在沙箱里把 90 天等到是 demo 做不到的事）。
- ★ 所以发现丢了东西的正确顺序是：**立刻停手 → `git reflog` 找哈希 → `branch` / `reset` / `cherry-pick` 抢救**。
  千万别先跑 `git gc` / `git reflog expire` / `git prune` —— 那才是真把救生圈戳破的动作。
  真要确认对象还在，可以用 `git cat-file -t <哈希>`（返回 `commit` 就是还在）。

---

## 红线清单：什么时候绝对不该 rebase / amend

| 情况 | 为什么 | 该做什么 |
|---|---|---|
| 这条分支别人也在用（已经 push） | 哈希全变（③④），他们的引用和 CI 全部指向不存在的对象 | 往上叠新提交；非改不可先通知，再 `--force-with-lease`（⑳） |
| `main` / 发布分支 / 长期共享分支 | 同上，影响面是整团队 | 用 merge 或 PR 的 squash，别在共享分支上 rebase |
| 已经 review 过的提交 | diff 链接、评论锚点全部失效 | 新提交叠上去，让 review 走增量 |
| 只为了「历史好看」而对别人分支 rebase | 收益是自己的，代价是所有人的 | 只整理自己没发布的那一段 |
| `pull` 之后不确定发生了什么 | `pull` = `fetch` + 合并/变基，可能已经改了本地历史 | 先 `git reflog` 看一眼，再动手（㉒） |
| 工作区还有未提交改动 | rebase 只处理提交，未提交的会被搅进来甚至丢 | 先 `git stash`（06 章）或先提交干净 |

补充三条容易被忽略的事实：

- **`git rebase` 的默认底本是当前分支的上游**（`@{up}`），不写参数时它会去读远程跟踪分支 —— 于是
  `pull --rebase` 的效果就是这么来的（`git rebase` ≈ `git rebase @{up}`）。本章所有例子都显式写出底本，
  就是为了不把行为交给配置。
- **rebase 会重写 `committer` 时间**（④），所以「按提交时间排序」的历史会 reorder；
  想保住时间线，看 `%ad`（作者时间）而不是 `%cd`。
- **合并过的历史 rebase 会被压平**（默认丢掉合并结构）。要保留菱形得用 `--rebase-merges`
  （**本章没演示**，07 章的协作场景里再碰更合适）。

---

## 速查表（这一章的命令 → 它动了什么）

| 命令 | 动了什么 | 现场在第几节 |
|---|---|---|
| `git log --oneline --graph --decorate --all` | 只读，看形状 | ① |
| `git merge-base A B` | 只读，算分叉点 | ① |
| `git log --oneline A..B` | 只读，「B 有 A 没有」=会被搬走的 | ① |
| `git rebase <底本>` | 重放当前分支的独有提交 → 新对象 + 挪指针 | ②③ |
| `git rebase --onto <新底本> <分界线> <分支>` | 只搬 (分界线,头】 那一段 | ⑮ |
| `git rebase -i <底本>` + `GIT_SEQUENCE_EDITOR` | 按清单逐条处理（pick/reword/edit/squash/fixup/drop/exec） | ⑪~⑭ |
| `git commit --fixup=<提交>` + `rebase -i --autosquash` | 造一条自动排到目标后面的补丁提交 | ⑭ |
| `git rebase --continue` | 报告冲突已处理（必须先 `git add`），继续重放 | ⑦ |
| `git rebase --skip` | **丢掉**当前这条不重放 | ⑧ |
| `git rebase --abort` | 完全回到 rebase 前 | ⑨ |
| `git rebase --quit` | 只删状态目录，HEAD 留在半路 | ⑩ |
| `git cherry-pick <提交>` / `-x` / `-n` / `A..B` / `-m 1` | 抄：追加到当前分支尾巴上 | ⑯⑰ |
| `git commit --amend [-m/-–no-edit/--author]` | 重写 **HEAD** 那一条（只带上暂存区的内容） | ⑲⑳ |
| `git reflog` / `git log -g <分支>` / `分支@{1}` | 读指针流水账，拿回旧哈希 | ㉑㉒㉓ |
| `git reset --hard <哈希>` / `git branch <名> <哈希>` | 把找回的哈希变成正式引用 | ㉒ |
| `git cherry -v A B` | 只读，按 patch-id 预告哪些会被跳过 | ⑤ |

---

## 怎么跑这章的 demo

```bash
cd git-handbook/05-rebase-history
bash demo.sh          # 全跑一遍（A→I 九段），跑完自动删沙箱；输出 1635 行，退出码 0
bash demo.sh --keep   # 跑完保留沙箱并打印路径，可以进去自己敲命令
```

脚本约定（这一章比前几章更严一点，逐条对上）：

- 所有仓库都建在 `mktemp -d` 出来的临时沙箱里（`/tmp/git-handbook-05-rebase-XXXXXXXX`），
  **不碰你机器上任何已有仓库、不碰你的工作目录**；结尾 `trap cleanup EXIT` 自动 `rm -rf`（`--keep` 除外）。
- **不访问网络**：需要「服务器上的仓库」时在沙箱里 `git init --bare` 冒充 `origin`，
  本章所有 `push` 都只推向那个沙箱 bare 目录（⑳ 用的就是它）。
- **不读也不写你的配置**：`GIT_CONFIG_GLOBAL=/dev/null`、`GIT_CONFIG_SYSTEM=/dev/null`（只是本进程不去读，
  不改你的任何文件）；全章**一次 `git config` 都没跑**（除了 ㉔ 那条只读的 `--get`）。
- 身份统一用环境变量给：`GIT_AUTHOR_NAME/EMAIL`、`GIT_COMMITTER_NAME/EMAIL` = `学习者 <learn@example.com>`，
  不落进任何配置文件。
- ★ **故意没有 `set -e`**：本章要看的就是失败现场（冲突、被拒的 push、报错的 cherry-pick）。
  每条命令都按 `$ 命令` → 原样输出 → `[退出码 N]` 的格式打印，非 0 的那些是专门给你看的教材。
- `export GIT_EDITOR=true` + 沙箱里的 `sed`/小脚本当 `GIT_SEQUENCE_EDITOR`，
  所以**这台机器没有图形编辑器也能一路跑完**，中途不会卡住等你打字。
- 只在输出打到终端时上色（`[ -t 1 ]` 判断），重定向到文件就是纯文本，方便和本文逐行对照。
- 时间戳从 `2026-01-01T10:01:00 +0800` 起逐条递增，**已连跑两次核对：除了沙箱目录名，输出逐字节相同**。

`--keep` 之后推荐自己进沙箱把关键几步敲一遍：

```bash
cd "$SB/c-conflict"   && git status --short && git ls-files -u   # 冲突现场的三个 stage
cd "$SB/d-todo"       && GIT_SEQUENCE_EDITOR="$SB/seq-show.sh" git rebase -i main
cd "$SB/h-rescue"     && git reflog | head -8                     # ★ 亲眼看到那条「已丢」的提交
cd "$SB/g-amend"      && git --git-dir="$SB/g-origin.git" log --oneline   # 服务器视角
```

本文引用的每一个 `text` 代码块都取自 `/tmp/git05.txt`（`bash demo.sh > /tmp/git05.txt 2>&1` 的原文）。
抽查任意一句是不是真跑出来的：

```bash
grep -qF 'dropping d3448015c3ae3680191ba9278bc9a594da9832be side1：加 h.txt -- patch contents already upstream' /tmp/git05.txt && echo 真
grep -qF '6e5d0f3 HEAD@{4}: commit: F2：★ 被我手滑删掉的那条' /tmp/git05.txt && echo 真
```

本文**没有实测、只引用官方文档或前几章结论**的地方（都已在正文里标注）：
`gc.reflogExpire=90 天 / gc.reflogExpireUnreachable=30 天`（㉔）、
`--force-with-lease` 在同事推过新东西时拒绝（⑳，完整现场在 07 章）、
`--rebase-merges` 保留菱形（红线清单）、`rebase.autosquash` 全局配置（⑭）、
`fixup -C/-c` 的差别（⑫，只在 ⑪ 的清单注释里引用了原文）。
真实远程仓库（GitHub 之类）上的 push 保护、PR 讨论区失锁等行为本文一律没测 —— 那是 07 章用本地 bare 仓库模拟的场景。

---

## 下一步

- `06-stash-and-ignore`：rebase 前工作区必须干净（红线清单最后一条），改到一半要切走就用 `git stash`。
- `07-remote-collab`：`pull --rebase` 就是「把 ⑳ 那条规矩每天用一次」，
  `--force-with-lease` 挡不住别人、`push` 被拒之后到底该怎么收尾，那一章有完整的两个人现场。
- `08-investigation`：`reflog` 拿到的哈希怎么继续用 —— `blame` / `bisect` / `-S` 找「这行谁写的」。
- `10-troubleshooting`：`HEAD` 游离了、`detached HEAD` 忘了收、误删分支 —— 全部回到本章 ㉒㉓ 的那三颗药。


