# 03 撤销与回退

> 一句话：这一章解决「我刚才敲错了 / 这个提交不想要 / 文件被我改烂了 / 甚至整个分支被我冲没了」，
> 并且教你怎么在真的搞砸之后把东西捞回来。

**全书最高价值的一章就是这章。** 前面两章教你怎么往前走，这一章教你怎么往回走 ——
而往回走的命令是 Git 里唯一会真的吃掉数据的命令。
所以本章每一条命令都会明确写出：**它动了哪几个区、会不会丢东西、丢了还能不能救。**

> ⚠️ 阅读顺序建议：先把「心智模型」那张图和「撤销全景表」看两遍，再往下看具体场景。
> 后面凡是带 ★ 的命令，本章都会提前告诉你它吃数据。**在你没看清对应小节之前，不要往自己的真实仓库上敲。**

---

## 你会遇到的场景

| 你遇到的事 | 本章哪一节 |
|---|---|
| 代码写着写着改烂了，想回到上次提交时的样子 | 场景 ① |
| `git add` 加错了文件，还没提交 | 场景 ② |
| 提交说明打错字了 | 场景 ③ |
| 提交完发现漏了一个文件 | 场景 ④ |
| 提交拆得太碎 / 提交到错误的分支上了，想整条撤了重做 | 场景 ⑤ |
| 目录里全是试过的脚本和编译产物，想清干净 | 场景 ⑥ |
| 别的分支上有个提交我也想要 | 场景 ⑦-a |
| 提交已经推到远程、同事已经拉走了，现在要撤销 | 场景 ⑦-b |
| `reset --hard` 敲错，几条提交连改动全没了 | **场景 ⑧（本章的保命符）** |

---

## 心智模型

### 撤销全景表：先看这张表，再学命令

这张表是本章的目录，也是你以后随手要查的东西。
「动了哪几个区」里的三个区，就是 01 章讲的**工作区 / 暂存区 / 仓库**。

| 我想干什么 | 用哪条命令 | 动工作区 | 动暂存区 | 动仓库(历史) | 会不会丢东西 | 丢了能救吗 |
|---|---|:--:|:--:|:--:|---|---|
| 这个文件我改烂了，退回上次提交的样子 | `git restore <文件>` | ✔ | ✘ | ✘ | **★ 工作区改动直接没** | 救不了（Git 从来没见过它） |
| 同上（老写法） | `git checkout -- <文件>` | ✔ | ✘ | ✘ | **★ 同上** | 救不了 |
| 我 add 加错了，退回没 add 的状态 | `git restore --staged <文件>` | ✘ | ✔ | ✘ | 不丢 | — |
| 同上（老写法） | `git reset HEAD <文件>` | ✘ | ✔ | ✘ | 不丢 | — |
| 提交说明写错了 | `git commit --amend -m "..."` | ✘ | ✔ | ✔（只改最后一条） | 不丢内容，但 hash 变 | reflog |
| 提交漏了个文件 | `git add 漏的` + `git commit --amend --no-edit` | ✘ | ✔ | ✔ | 不丢 | reflog |
| 撤销提交，改动我要留着重新组织 | `git reset --soft HEAD~n` | ✘ | ✘ | ✔ | 不丢 | — |
| 撤销提交，改动退回未暂存 | `git reset HEAD~n`（默认 --mixed） | ✘ | ✔ | ✔ | 不丢 | — |
| 撤销提交，改动也别要了 | `git reset --hard HEAD~n` | ✔ | ✔ | ✔ | **★ 真丢** | **reflog 能救** |
| 清掉所有未跟踪的新文件 | `git clean -nd` → `git clean -fd` | ✔ | ✘ | ✘ | **★ 真丢** | **救不了**（Git 没见过） |
| 把别的分支某一条提交搬过来 | `git cherry-pick <hash>` | ✔ | ✔ | ✔（新增一条） | 不丢 | — |
| 已经共享出去的提交要作废 | `git revert <hash>` | ✔ | ✔ | ✔（往后加一条） | 不丢 | — |
| 撤销一条合并提交 | `git revert -m 1 <merge 的 hash>` | ✔ | ✔ | ✔ | 不丢 | — |
| 玩坏了，找回被冲掉的提交 | `git reflog` → `git reset --hard <hash>` | ✔ | ✔ | ✔ | 这是救援，不是破坏 | — |

三个规律，读完这张表你就该记住：

1. **Git 只救它见过的东西。** 凡是 `git add` 过的，都在仓库里有快照，能救。
   从没 add 过的（未跟踪文件、工作区里改了但没提交的中间状态），Git 手里没有副本，**删了就是真没了**。
2. **凡是动了「仓库」那一列的命令，都会让 hash 变。** hash 一变，别人手里的副本就错位。
   于是有本章最重要的一句判据（下面会反复出现）：
   > **这段历史别人拉走过吗？拉走过就不要改写，只能用 `revert` 往后加。**
3. **`reset --hard` 之后不是终点。** 只要没执行过 `git gc --prune=now`，
   你被冲掉的东西基本都还在，靠 `reflog` 能捞回来。场景 ⑧ 现场演示一遍你就信了。

### 撤销的物理过程：一条链、一个指针、一堆没引用的旧快照

这一章的命令看起来五花八门，其实物理上只做两件事：**挪指针**和**按快照覆盖文件**。
把下面这张图记住，本章所有命令都不用背。

```
          历史是一条链，每条提交存的是「那一刻整个项目的快照」，不是差异

  ... 提交1 ──→ 提交2 ──→ 提交3 ──→ 提交4 ──→ 提交5   ← main 指这里
                                          ▲
                                          │
                                        main

  你敲 git reset --hard HEAD~2：

  ... 提交1 ──→ 提交2 ──→ 提交3 ──→ 提交4 ──→ 提交5
                                 ▲
                                 │
                               main              ← 只是这一个文本文件里的 hash 被改了

  ★ Git 干的事：把 main 这个文本文件里的 hash 从「提交5」改成「提交3」
  ★ Git 没干的事：没有删任何提交对象。提交4、提交5 还完整地躺在 .git 里
  ★ Git 额外干的事：把「工作区 + 暂存区」改成「提交3 那份快照」的样子 ——
                    你感觉「文件被回退了」，其实是被快照覆盖了一遍

  那提交4、提交5 什么时候真没？

  HEAD 的 reflog（记录了 HEAD 最近走过去的每一步）：
     HEAD@{0}: reset: moving to HEAD~2      ← 现在
     HEAD@{1}: commit: 提交5                ← 你被冲掉之前在这
     HEAD@{2}: commit: 提交4
     ...
  只要你还没跑 git gc --prune=now，用 HEAD@{1} 或者那条提交的 hash 就能整个回来。
```

配合 01 章那句「分支就是一个文本文件」：
`reset` 从来不动历史，**它只改那个文本文件里写的 hash**。
「历史被改写」是给人的错觉，物理上是**你离开了这条链，走到更前面的节点去了**，
旧节点还挂在链条上，只是没有一个分支指着它了而已。

### 撤销之前，先分清三个区

所有撤销命令都在回答同一个问题：**你要退回的是哪个区？**

```
   工作区              暂存区               仓库
  （你的编辑器）      （下次提交的草稿）    （已归档的快照）
       │                  │                  │
   改了但没 add      add 了但没 commit     commit 过了
       │                  │                  │
       └─ restore <文件> ──┘                  │     ← 拿仓库版本覆盖工作区
                              └─ restore --staged <文件> ─┘ ← 只退回草稿，草稿内容不动
                                              │
                                    reset --soft/--mixed/--hard ← 移动归档位置
```

- 改坏了 → 只涉及「工作区 vs 仓库」这一层 → 场景 ①
- add 错了 → 只涉及「暂存区」这一层 → 场景 ②
- commit 了 → 涉及仓库 → 场景 ③④⑤⑦，并且**只要 commit 过，撤销就可能影响别人**

---

## 场景逐个实战

> 下面每一段的所有输出都是真跑出来的，跑法见最后的「运行 demo」。
> 本手册所有沙箱都用固定时间戳（`2026-01-01T10:0x:00 +0800`）和固定身份，所以你能复现出一模一样的 hash。
> 输出里的中文报错取决于你的系统语言；如果你的 Git 是英文界面，对照 `LC_ALL=C` 的英文原文看即可，含义一致。

### ① 工作区的文件改坏了，想退回上一次提交时的样子

**什么情况**
你正在改 `note.txt`，改了半天发现方向错了，想回到「上次提交时它长什么样」。
关键前提：**这些改动从没 `git add` 过**，只存在于工作区。

**敲哪些命令**

```bash
git status                      # 先看：到底改了哪些文件
git restore note.txt            # ★ 会丢：把这个文件的工作区版本覆盖成仓库里的版本
```

老写法 `git checkout -- note.txt`，效果和上面这条完全一样。

**为什么是这几条**
`restore` 的意思是「从某个来源恢复文件内容」。来源默认是仓库（上一次提交），
所以它做的事就是：把仓库里那份快照的内容，重新写回你硬盘上的这个文件。
它只碰这一个文件，别的文件、暂存区、历史都不动。

**真实输出**

```text
$ git status
位于分支 main
尚未暂存以备提交的变更：
  （使用 "git add <文件>..." 更新要提交的内容）
  （使用 "git restore <文件>..." 丢弃工作区的改动）
	修改：     note.txt

修改尚未加入提交（使用 "git add" 和/或 "git commit -a"）
$ git diff --stat
 note.txt | 3 ++-
 1 file changed, 2 insertions(+), 1 deletion(-)

$ git restore note.txt
$ git status
位于分支 main
无文件要提交，工作区干净
$ cat note.txt
第一行：目标
第二行：做法
第三行：注意事项

$ git checkout -- note.txt
$ cat note.txt
第一行：目标
第二行：做法
第三行：注意事项
```

注意 git 自己在提示里就写了 `（使用 "git restore <文件>..." 丢弃工作区的改动）` ——
它把「丢弃」两个字直接印在你脸上，这条命令确实就是丢弃。

**为什么有两个名字做同一件事：`restore` / `checkout` / `switch`**

早期的 Git 里 `git checkout` 一条命令干三件完全不同的事：

```
git checkout <分支>        # 切分支
git checkout -- <文件>     # 丢弃某文件的改动
git checkout <提交>        # 让 HEAD 脱离分支，指向某个提交
```

同一个命令、三种语义，其中最危险的是第二个：
你以为在切分支，敲了个文件名，改动就没了。
所以新 Git 把这三件事拆成三个命令，**名字即语义**：

| 新命令 | 干的事 | 对应的老写法 |
|---|---|---|
| `git switch <分支>` | 只切分支 | `git checkout <分支>` |
| `git restore <文件>` | 只丢弃/恢复文件内容 | `git checkout -- <文件>` |
| `git switch --detach <提交>` | 让 HEAD 指向某个提交 | `git checkout <提交>` |

看到别人的教程里全是 `checkout`，不用怀疑，就是同一个动作。**新代码用 restore/switch，读旧文档认识 checkout。**

**常见翻车**

- **未跟踪的文件 restore 不掉。** 新建的、从没 `git add` 过的文件，Git 手里没有它的「仓库版本」，
  所以它根本不在 restore 的管辖范围：

  ```text
  $ git status --short
  ?? scratch.txt
  $ git restore scratch.txt
  错误：路径规格 'scratch.txt' 未匹配任何 Git 已知文件
  ```

  这类文件要么手动 `rm`，要么用场景 ⑥ 的 `git clean`。
- **`restore` 不问你确认。** 它覆盖就覆盖了，没有 `-i` 之类的温柔选项。改坏之前先 `git diff` 看清。
- **想恢复「上一次提交之前」是不存在的。** 你改坏了、又提交了、再 restore ——
  restore 拿的是你刚提交的那份，等于什么都没撤销。这时候要用场景 ⑤。

---

### ② 已经 `git add` 了又反悔：退回工作区状态，但改动必须留着

**什么情况**
你 `git add` 了 `a.txt`，但仔细一想这个不该进这次提交。
**注意你的诉求**：改动我要留着，只是不想让它出现在「下次提交」里。

**敲哪些命令**

```bash
git restore --staged a.txt      # 只把暂存区退回仓库版本，工作区一个字不动
```

老写法是 `git reset HEAD a.txt`，效果一样。

**为什么是这条**
`git add` 干的事是「把工作区当前内容拍个快照，放进暂存区」。
`--staged` 就是在说：**这次操作的对象是暂存区那一层，不是工作区**。
于是暂存区的内容被换回成仓库里那份，而你硬盘上的文件 Git 根本没碰。

**真实输出**

```text
$ git add a.txt
$ git status --short
M  a.txt
 M b.txt

$ git restore --staged a.txt
$ git status --short
 M a.txt
 M b.txt
$ cat a.txt
A 原始
A 的新改动，我还想留着
```

**先学会读 `git status --short` 的两个格子**，这是本章的实用基本功：

```
 第1格  第2格   含义
  M      ·     已暂存（改了并 add 过），工作区相对暂存区没再改
  ·      M     未暂存（改了但没 add）
  M      M     已暂存，而且暂存之后又改了
  A      ·     新文件，已暂存
  ?      ?     未跟踪 → ??
```

上面输出里 `M  a.txt`（M 在第1格）是「已暂存」，` M b.txt`（M 在第2格）是「没暂存」。
`restore --staged` 之后，a.txt 的 M 从第 1 格跳到了第 2 格 —— 改动还在，只是退出了草稿箱。

再补一个铁证：撤销前后 `git diff` 的两个方向也对上了。

```text
$ git add a.txt
$ git status --short
M  a.txt
 M b.txt
$ git reset HEAD a.txt
重置后取消暂存的变更：
M	a.txt
M	b.txt
$ git status --short
 M a.txt
 M b.txt
$ cat a.txt
A 原始
A 又改了
```

**如果你连工作区的改动也不想要了**

那就再加一条不带 `--staged` 的 restore。**这一条会真丢东西。**

```text
$ git restore a.txt
$ cat a.txt
A 原始
$ git status --short
 M b.txt
```

顺序是：**先 `--staged` 退出草稿箱，再不带参数丢弃** —— 反过来的话你会少看一次改动内容。

**常见翻车**

- **`git reset --hard` 想用来取消 add：不行。** `--hard` 会连工作区一起砸掉，
  你以为只退暂存区，实际上文件内容也回退了。取消 add 用 `--staged` / `git reset HEAD <文件>`。
- **`git restore --staged <文件>` 对「新 add 的文件」的效果是让它变成未跟踪**，不是删掉它。
  这很好：文件还在，你可以继续改或者手动删。
- **想整体退回上一个提交状态但保留所有改动** → 那是场景 ⑤ 的 `git reset --mixed HEAD~1`。

---

### ③ 提交说明写错了：`git commit --amend` 就地重写最后一条

**什么情况**
刚提交完，发现说明写成了 `fixd 登录`（应该是 `fix: 修复登录超时`）。
内容是对的，只有说明错了。**而且这条还没推给别人。**

**敲哪些命令**

```bash
git log --oneline                  # 确认你要改的就是最后一条
git commit --amend -m "fix: 修复登录超时"
```

**为什么是这几条**
`--amend` 的物理动作是：**把最后一条提交撤掉，把「暂存区此刻的内容 + 新说明」重新打包成一条新提交。**
既然是新打包，就是一个新对象，**hash 一定变**（01 章讲的：提交对象不可变，改任何东西 = 造新的）。
这就是为什么 amend 只在「还没共享」的时候安全。

**真实输出**

```text
$ git log --oneline
535d54d fixd 登录
9954ff0 初始

$ git commit --amend -m 'fix: 修复登录超时'
[main 0addcbd] fix: 修复登录超时
 Date: Thu Jan 1 10:04:00 2026 +0800
 1 file changed, 1 insertion(+)
$ git log --oneline
0addcbd fix: 修复登录超时
9954ff0 初始
```

hash 对照（脚本里真取出来的完整 hash）：

```text
  改之前 535d54d7a492b559f9acc0032b65526d2acd5217
  改之后 0addcbd1597110d6c5a83b151a9b79c9c981731b
```

旧提交去哪了？reflog 里有记录：

```text
$ git reflog | head -5
0addcbd HEAD@{0}: commit (amend): fix: 修复登录超时
535d54d HEAD@{1}: commit: fixd 登录
9954ff0 HEAD@{2}: commit (initial): 初始
```

`commit (amend)` 是 git 给这次操作打的标签。
**旧提交没有消失，只是没人引用它了** —— 这正是场景 ⑧ 能把东西捞回来的原因。

**常见翻车**

- **忘了 `--amend` 结果变成新提交**：`git commit -m "改一下说明"` 会老老实实加一条新提交，历史里留下两条。
- **amend 时把工作区所有改动一起吞进去**：`--amend` 打包的是**暂存区**，不是整个工作区。
  如果你习惯 `commit -a`，或者之前 `git add -A` 过，很容易把不该进这条提交的东西一起带进去。
  所以 amend 之前先看一眼 `git status`。
- **对已经推出去的提交 amend** → 见场景 ④ 的翻车演示，push 会直接被拒。

---

### ④ 提交完发现漏了一个文件：`add` 之后 `--amend` 并进同一条

**什么情况**
提交了「feat: 加登录功能」，回头一看 `README.md` 忘了 add。
你不希望历史里出现一条「补 README」的碎提交。

**敲哪些命令**

```bash
git add README.md
git commit --amend --no-edit        # --no-edit = 保留原来的提交说明
```

**为什么是这几条**
和 ③ 同一套机制：`add` 把漏的内容放进暂存区，`--amend` 把「暂存区全部内容」重打包成一条提交。
`--no-edit` 只是让你不用重打一遍说明。

**真实输出**

```text
$ git show --stat --oneline HEAD
9e2b62a feat: 加登录功能
 app.js | 1 +
 1 file changed, 1 insertion(+)

$ git add README.md
$ git commit --amend --no-edit
[main 18c8dcb] feat: 加登录功能
 Date: Thu Jan 1 10:06:00 2026 +0800
 2 files changed, 2 insertions(+)
 create mode 100644 README.md
 create mode 100644 app.js
$ git show --stat --oneline HEAD
18c8dcb feat: 加登录功能
 README.md | 1 +
 app.js    | 1 +
 2 files changed, 2 insertions(+)
```

文件从 1 个变 2 个，说明还是那句 `feat: 加登录功能`，历史里也没有多出一条脏提交。

**★ 危险边界：这条提交已经推给别人了怎么办**

沙箱里用一个本地 bare 仓库冒充 `origin`，把这条推出去，再 amend，再 push：

```text
$ git remote add origin ../s4-origin.git
$ git push -u origin main
To ../s4-origin.git
 * [new branch]      main -> main
分支 'main' 设置为跟踪 'origin/main'。
```

到这一步，那条提交已经是**公共历史**了。接下来故意手滑：再 amend 一次，补一个文件进去。

```text
$ git add extra.js
$ git commit --amend --no-edit
[main 00cb6bb] feat: 加登录功能
 Date: Thu Jan 1 10:06:00 2026 +0800
 3 files changed, 3 insertions(+)
 create mode 100644 README.md
 create mode 100644 app.js
 create mode 100644 extra.js
$ git log --oneline
00cb6bb feat: 加登录功能
$ git push origin main
To ../s4-origin.git
 ! [rejected]        main -> main (non-fast-forward)
错误：无法推送一些引用到 '../s4-origin.git'
提示： 更新被拒绝，因为您当前分支的最新提交落后于其对应的远程分支。
提示： 如果您希望先与远程变更合并，请在推送前执行 'git pull'。
提示： 详见 'git push --help' 中的 'Note about fast-forwards' 小节。
```

最后这条 `git push` 的退出码是 **1**（demo 在那一行后面打了 `↳ 退出码 1` 的注解，
注解不是 git 的输出，所以没抄进围栏）。

Git 在这里拦你，是**在保护你的同事**。你 amend 出来的 `00cb6bb` 和远程那条不是同一个对象，
远程认为你「落后了」，因为你的新历史里没有它认得的那条提交。

你有两条路：

1. **老实做法（推荐）**：不再动这条提交，往后加一条新提交把漏的文件补上。历史里多一条碎提交，但没人受伤。
2. **确实要改写**：用 `git push --force-with-lease`（**不是**裸 `-f`），第 05 章会专门用两个沙箱克隆
   演示裸 `-f` 怎么吃掉同事的提交，以及带保险绳的版本怎么挡住。

> **本章判据第一次出场：这段历史别人拉走过吗？拉走过就不要改写。**

**常见翻车**

- **amend 到一半想放弃**：`--amend` 是立即生效的，没有「编辑中」状态可退出。
  要退回 amend 之前的样子，用 `git reset --hard ORIG_HEAD`（`ORIG_HEAD` 是 git 在危险操作前偷偷记下的位置），
  或者按场景 ⑧ 用 `git reflog` 找那条 `commit (amend)` 之前的 hash。
- **把 amend 当成 `--no-verify` 的后门**：amend 会重新跑一遍钩子，如果钩子拦你，说明真有问题。

---

### ⑤ 撤销提交本身：`reset --soft` / `--mixed` / `--hard` 三兄弟

**什么情况**
你连做了 5 条提交，越看越不对劲：拆得太碎、提交到了错误的分支、或者压根就不该提交。
想把「提交」这一步撤销掉。

**先看清楚：三种模式动的区不一样**（★ = 会真丢东西）

```
  ┌──────────────┬────────────┬────────────┬──────────────────┐
  │              │  --soft    │  --mixed   │  --hard          │
  ├──────────────┼────────────┼────────────┼──────────────────┤
  │ 分支指针     │ 移动       │ 移动       │ 移动             │
  │ 暂存区       │ 不动       │ 重置       │ 重置             │
  │ 工作区       │ 不动       │ 不动       │ 覆盖 ★           │
  └──────────────┴────────────┴────────────┴──────────────────┘
```

不带参数时默认就是 `--mixed`，所以 `git reset HEAD~2` 等于 `git reset --mixed HEAD~2`。

**怎么选**

- 想**重新组织提交**（把 5 条并成 2 条、或者换个拆分方式）→ `--soft`，改动全都排在暂存区，直接 `git commit` 就行
- 想**回到「还没 add」的状态**，重新决定要提交什么 → `--mixed`（默认，最常用）
- 想**彻底不要这几条提交的改动** → `--hard` ★
  > ⚠️ **这条命令会真丢东西，敲之前先看清本节和场景 ⑧。** 它是本章最容易造成事故的一条。

**统一现场**

三个沙箱是完全相同的现场：5 条提交（`提交 1`…`提交 5`，每条往 `a.txt` 加一行、并新增一个 `docN.md`），
外加一条**没提交的工作区改动**和一个**未跟踪文件** `draft.txt`。
每条都执行 `git reset --<模式> HEAD~2`。

**真实输出 A：`--soft`（撤销提交，改动全部退回暂存区）**

```text
$ git log --oneline
e2ddd3e 提交 5
081a011 提交 4
a894dbd 提交 3
362cc91 提交 2
1e21687 提交 1
$ git reset --soft HEAD~2
$ git log --oneline
a894dbd 提交 3
362cc91 提交 2
1e21687 提交 1
$ git status
位于分支 main
要提交的变更：
  （使用 "git restore --staged <文件>..." 以取消暂存）
	修改：     a.txt
	新文件：   doc4.md
	新文件：   doc5.md

尚未暂存以备提交的变更：
  （使用 "git add <文件>..." 更新要提交的内容）
  （使用 "git restore <文件>..." 丢弃工作区的改动）
	修改：     a.txt

未跟踪的文件:
  （使用 "git add <文件>..." 以包含要提交的内容）
	draft.txt
$ cat a.txt
第1行：骨架
第2行：新增
第3行：新增
第4行：新增
第5行：新增
第6行：还没提交的工作区改动
```

`提交 4`、`提交 5` 从 log 里消失了，但它们带来的改动**整整齐齐排在「要提交的变更」里** ——
你现在只需要一条 `git commit -m "提交 4+5 合并版"` 就能重新组织。
注意 `修改：a.txt` 同时出现在「要提交」和「尚未暂存」两栏：前 5 行改动已进暂存区，
第 6 行是你 reset 之前就存在的工作区改动，它没进暂存区。**这正是 --soft 不动暂存区的证明。**

**真实输出 B：`--mixed`（默认，改动退回工作区）**

```text
$ git reset --mixed HEAD~2
重置后取消暂存的变更：
M	a.txt
$ git log --oneline
91e596a 提交 3
aa96425 提交 2
0415318 提交 1
$ git status
位于分支 main
尚未暂存以备提交的变更：
  （使用 "git add <文件>..." 更新要提交的内容）
  （使用 "git restore <文件>..." 丢弃工作区的改动）
	修改：     a.txt

未跟踪的文件:
  （使用 "git add <文件>..." 以包含要提交的内容）
	doc4.md
	doc5.md
	draft.txt
```

a.txt 的 6 行改动仍然全在，但全部退到「尚未暂存」；
`doc4.md` / `doc5.md` 因为本来是被提交「新增」的文件，reset 后它们变成了**未跟踪** ——
注意这里有个容易误解的点：`--mixed` **不会删文件**，只是 git 不再认识它们了。
如果你以为「撤销了就不在了」而去做 `git clean`，那就会真没了。

**真实输出 C：`--hard`（★ 真丢）**

```text
$ git reset --hard HEAD~2
HEAD 现在位于 867addb 提交 3
$ git log --oneline
867addb 提交 3
aa035db 提交 2
fdc9826 提交 1
$ git status
位于分支 main
未跟踪的文件:
  （使用 "git add <文件>..." 以包含要提交的内容）
	draft.txt

提交为空，但是存在尚未跟踪的文件（使用 "git add" 建立跟踪）
$ cat a.txt
第1行：骨架
第2行：新增
第3行：新增
```

对比一下就明白了：

| | `--soft` | `--mixed` | `--hard` |
|---|---|---|---|
| `a.txt` 行数 | 6 行全在 | 6 行全在 | **只剩 3 行**（第 4、5、6 行没了） |
| `doc4.md` / `doc5.md` | 还在，且已暂存 | 还在，未跟踪 | **文件被 git 删掉了** |
| `git status` | 一屏内容 | 一屏内容 | 几乎干净 |
| 你的工作区改动（第 6 行） | 保留 | 保留 | **一起没了，你没提交的东西也砸了** |

**★ 这里有个巨大的坑，必须单独立一句**：
`--hard` 覆盖的是「**git 认识的文件**」。那个从未 `git add` 过的 `draft.txt`，
`--hard` 拿它没办法，三份沙箱里它都活着：

```text
$ ls -1 "$SB"/s5-*/draft.txt
/tmp/git-handbook-03-undo-Tzw9c7ds/s5-hard/draft.txt
/tmp/git-handbook-03-undo-Tzw9c7ds/s5-mixed/draft.txt
/tmp/git-handbook-03-undo-Tzw9c7ds/s5-soft/draft.txt
```

两个结论：
1. `reset --hard` 之后 `git status` 里如果还有「未跟踪」那一段，别看到「干净」二字就放心。
2. 想把 `reset --hard` 没删掉的未跟踪文件一起清掉，得再补一条 `git clean -fd`（下一场景）——
   **这两条合起来才是「彻底回到过去」，也才是真正危险的两连击。**

**常见翻车**

- **`git reset --hard HEAD`（想「刷新一下」）**：这一条的意思是「把工作区和暂存区全部砸回当前提交」，
  你所有未提交的改动瞬间清零。有人以为它是「重载」，其实是格式化。
- **撤销之后想再撤销**：`reset` 移走了指针，`HEAD~2` 的含义也跟着变了。要找回上一个位置用 reflog，别猜。
- **`--soft` 之后立刻 `git commit`，结果提交是空的**：`--soft` 保留暂存区内容，但如果原本暂存区是空的，
  你会得到「无文件要提交」，此时改动其实在工作区 —— 那是 `--mixed` 的结果，回去看模式。

---

### ⑥ 一堆没用的新文件和构建产物：`git clean`

**什么情况**
试了半天，目录里留下一堆 `try1.py`、`TEMP.log`，还有编译出来的 `build/`。
这些都是未跟踪文件，`restore` / `reset` 管不了它们（原因见 ① 和 ⑤）。

**敲哪些命令**

```bash
git clean -nd      # ★ 保命第一步：-n = 只报名单，不删。看清楚再往下
git clean -fd      # 确认无误后，真删
```

**为什么是这几条**
`-n`（dry-run）只打印「将删除 …」，什么都不动；`-f` 才强制删；`-d` 才连带删目录。
Git 故意把 `clean` 设计成**必须显式加 `-f`** 才动手，就是为了让你有机会停下来。

**真实输出**

```text
$ git status --short
?? TEMP.log
?? try1.py
?? try2.py

$ git clean -nd
将删除 TEMP.log
将删除 try1.py
将删除 try2.py

$ git clean -fd
正删除 TEMP.log
正删除 try1.py
正删除 try2.py
$ echo -n "  现在目录里有："; ls -1 | tr "\n" " "; echo
  现在目录里有：app.py build
```

**被 `.gitignore` 忽略的文件是另一档。** 沙箱里 `.gitignore` 写了 `build/`，
所以它连 `-nd` 的名单都不进 —— 这是好事，保护你的编译缓存：

```text
$ git clean -nxd
将删除 TEMP.log
将删除 build/
将删除 try1.py
将删除 try2.py
```

要连忽略文件一起清，需要显式加 `-x`：

```text
$ git clean -fdx
正删除 build/
$ echo -n "  现在目录里有："; ls -1 | tr "\n" " "; echo
  现在目录里有：app.py
```

**读措辞：`-n` 说「将删除」，真删时说「正删除」。看到「正删除」人就已经在坑里了。**

**常见翻车**

- **不加 `-n` 直接 `-fd`**：这是本章事故排行榜第一名。`clean` 删掉的东西 **Git 从来没见过，reflog 也救不了**，
  比 `reset --hard` 更彻底 —— 因为 `reset --hard` 至少还有个旧快照在对象库里。
- **`git clean -fdx` 顺手清掉了你本地的 `.env`、`node_modules`**：`-x` 会把所有被忽略的文件一起删。
  删依赖是小事（重装就行），**被忽略的本地配置文件删了才是大事**，那往往是你唯一的副本。
  用 `-x` 之前务必先跑 `-nxd` 读一遍名单。
- **`-e <规则>` 可以排除**：`git clean -fd -e .vscode` 能在名单里放过某个路径，比事后后悔便宜。

---

### ⑦-a 只想把某一条提交搬过来：`git cherry-pick`

**什么情况**
同事在 `hotfix` 分支上做了一个小修复，你也想要，但你**不想要整条 `hotfix` 分支**
（那上面还有半成品）。
这时候 merge 就太重了，正确工具是把那一条提交复制过来。

**敲哪些命令**

```bash
git log --oneline hotfix          # 找到你要的那条的 hash
git cherry-pick <hash>            # 把它的内容重新打包成一条新提交，落在当前分支顶端
```

**为什么是这几条**
`cherry-pick` 干的事：取出那条提交「相对它父亲」的差异，把这个差异应用到你当前分支顶端，
然后**新建一条提交**。所以它和 `rebase` 是一家人 —— 都是「重新造提交」。
既然是重新造，hash 就变。

**真实输出**

现场：`main` 和 `hotfix` 在 `c2` 之后分叉，`hotfix` 上有一条只改 `cache.conf` 的提交。

```text
$ git log --oneline --decorate --graph --all
* 9fec911 (HEAD -> main) c3 main 自己的改动
| * 8a92fd7 (hotfix) hotfix: 给缓存加上限
|/  
* 4dfa2aa c2 加 readme
* 21c66ec c1 加缓存配置
```

搬运：

```text
$ git cherry-pick 8a92fd7c5953017d6e46c69716200405f8c6b9a4
[main 7310886] hotfix: 给缓存加上限
 Date: Thu Jan 1 10:28:00 2026 +0800
 1 file changed, 1 insertion(+)
$ git log --oneline --decorate --graph --all
* 7310886 (HEAD -> main) hotfix: 给缓存加上限
* 9fec911 c3 main 自己的改动
| * 8a92fd7 (hotfix) hotfix: 给缓存加上限
|/  
* 4dfa2aa c2 加 readme
* 21c66ec c1 加缓存配置
$ cat cache.conf
ttl=60
max=100
```

**内容搬过来了，hash 变了：`8a92fd7` → `7310886`。**
为什么变？因为提交对象里记着「我的父亲是谁」。原来那条的父亲是 `c2`，
新造这条的父亲是 `c3` —— 父亲不一样，内容再像也是两个不同对象。
**同一份改动在两个分支上有两个 hash，这是 Git 里很多「明明是一样的改动为什么还冲突」的答案。**

**翻车：搬过来的改动和你当前分支改了同一行**

```text
$ git cherry-pick 0210a72222be2060235297b7a532688a4131f350
自动合并 readme.md
冲突（内容）：合并冲突于 readme.md
错误：无法应用 0210a72... hotfix: readme 补一句
提示： 解决所有冲突之后，用 "git add/rm <路径规格>" 标记它们，
提示： 然后执行 "git cherry-pick --continue"。您也可以执行
提示： "git cherry-pick --skip" 命令跳过这个提交。如果想要中止执行并回到
提示： 执行 "git cherry-pick" 之前的状态，执行 "git cherry-pick --abort"。
提示： Disable this message with "git config set advice.mergeConflict false"

$ git status --short
UU readme.md
$ cat readme.md
第一行
<<<<<<< HEAD
第二行（main 在这里加了另一句）
=======
第二行（hotfix 在这里加了说明）
>>>>>>> 0210a72 (hotfix: readme 补一句)
第三行
```

三条出路，和 merge/rebase/revert 是同一套（04 章细讲冲突，这里先记住形状）：

- 手工把文件改对 → `git add readme.md` → `git cherry-pick --continue`
- 这条我不要了 → `git cherry-pick --skip`
- 整个当没发生 → `git cherry-pick --abort`

```text
$ git cherry-pick --abort
$ git log --oneline
6c82183 main: readme 也补一句
7310886 hotfix: 给缓存加上限
9fec911 c3 main 自己的改动
4dfa2aa c2 加 readme
21c66ec c1 加缓存配置
$ git status --short
```

最后那条 `git status --short` 一行输出都没有 —— 工作区又干净了，`--abort` 把撞车现场整个收了回去。

**常见翻车**

- **搬完又 merge 了原分支** → 同一份改动以两个 hash 各进一次，通常会「干净地重复一遍」或者撞冲突。
  经验：**要么 cherry-pick 单条，要么 merge 整条，别对同一批提交两样都干。**
- **cherry-pick 了合并提交** → 直接报 `错误：提交 xxx 是一个合并提交但未提供 -m 选项`，
  和 revert 的规则一样（见 ⑦-b）。
- **搬完 hash 变了就以为「这不是同一个修复」** → 是同一个改动，不是同一个对象。这是正常且必然的。

---

### ⑦-b 历史已经共享出去了：别改写，用 `revert` 加一条反向提交

**什么情况**
「提交 3」上线了，也已经 `git push` 出去、同事也拉走了。
现在要撤销它。

**为什么不能 `reset`**
`reset` 会让历史变短。你本地的 `main` 变成 3 条，远程是 4 条，
下次你 push 就会被拦（见 ④ 的输出），硬推上去就是把同事的工作基础抽掉。
**已经共享的历史只能往前追加，不能缩短。**

**敲哪些命令**

```bash
git log --oneline              # 找到要撤销的那条的 hash
git revert <hash>              # 生成一条「内容正好相反」的新提交
git revert --no-edit <hash>    # 用默认说明，不进编辑器
```

**为什么是这条**
`revert` = 「取这条提交相对它父亲的差异，反向应用一遍，做成一条新提交」。
历史长度 +1，不是 -1。原来的「提交 3」仍然在 log 里，后面多了一条 `Revert "提交 3"`。
这就是它的价值：**留痕、可追溯、对别人无害。**

**真实输出**

```text
$ git log --oneline
9dbe650 提交 4
0311be5 提交 3
0b43d5d 提交 2
21d88a9 提交 1

$ git revert --no-edit HEAD~1
[main 1f7f146] Revert "提交 3"
 Date: Thu Jan 1 10:35:00 2026 +0800
 1 file changed, 1 deletion(-)
 delete mode 100644 f3.txt
$ git log --oneline
1f7f146 Revert "提交 3"
9dbe650 提交 4
0311be5 提交 3
0b43d5d 提交 2
21d88a9 提交 1
```

文件层面：

```text
$ [ -e f3.txt ] && echo "  f3.txt 还在" || echo "  f3.txt 已经不存在了"
  f3.txt 已经不存在了
```

历史层面：**「提交 3」还在 log 里**，只是后面多了一条 Revert。
而且历史是往前长不是往后缩，所以 push 一路绿灯：

```text
$ git push origin main
To ../s7b-origin.git
   9dbe650..1f7f146  main -> main
```

**翻车 A：要撤销的那行改动，后来又被别人改过了**

沙箱里：`提交A` 把第 2 行改成「提交A把它改坏了」，随后 `提交B` 又在这一行叠了别的改动。
现在撤销 A：

```text
$ git revert --no-edit b386140777a140757c21a8b526b302a2e85942f7
自动合并 config.txt
冲突（内容）：合并冲突于 config.txt
错误：无法还原 b386140... 提交A：改了第2行
提示： 解决所有冲突之后，用 "git add/rm <路径规格>" 标记它们，
提示： 然后执行 "git revert --continue"。您也可以执行
提示： "git revert --skip" 命令跳过这个提交。如果想要中止执行并回到
提示： 执行 "git revert" 之前的状态，执行 "git revert --abort"。
提示： Disable this message with "git config set advice.mergeConflict false"

$ git status --short
UU config.txt
$ cat config.txt
第1行 别碰
<<<<<<< HEAD
第2行 提交B又在这行叠了新改动
=======
第2行 本来是好的
>>>>>>> parent of b386140 (提交A：改了第2行)
第3行 别碰
第4行 别碰
```

这条 `git revert` 的退出码是 **1**（demo 在那一行后面打了 `↳ 退出码 1` 的注解，
注解不是 git 的输出，所以没抄进围栏）。

认一下这个形状：`>>>>>>> parent of b386140 (提交A：改了第2行)` ——
看到 **`parent of`** 就说明这是 revert 撞冲突了（普通合并冲突这里写的是分支名/提交标题）。
下半段（`=======` 到 `>>>>>>>`）是「撤销之后应该变成的样子」，也就是 A 动手之前的旧内容。

出路和 cherry-pick 一样：改对 → `git add` → `git revert --continue`；不想撤了 → `git revert --abort`。

```text
$ git revert --abort
$ git status --short
$ git log --oneline
0e10025 提交B：在第2行上又改了一次
b386140 提交A：改了第2行
8e84658 加配置文件
1f7f146 Revert "提交 3"
9dbe650 提交 4
0311be5 提交 3
0b43d5d 提交 2
21d88a9 提交 1
```

`--abort` 之后历史一个字没动 —— revert 失败不留任何痕迹，这点很友好。

**翻车 B：撤销一条合并提交**

合并提交有**两个父亲**，git 不知道你想沿哪条线退回，直接拒绝：

```text
$ git log --oneline --graph
*   dc9af91 Merge branch feat
|\  
| * 30fe816 c2 加 feat.txt
* | bf6c41a c3 main 自己的改动
|/  
* 0c903f9 c1 起点

$ git revert --no-edit HEAD
错误：提交 dc9af9170961588c6a32ee9ce581b50067639a34 是一个合并提交但未提供 -m 选项。
致命错误：还原失败
```

`-m 1` 的意思是「**保留第 1 个父亲那条线**（也就是主线），把第 2 个父亲带进来的东西撤掉」：

```text
$ git revert --no-edit -m 1 HEAD
[main c90d8dd] Revert "Merge branch feat"
 Date: Thu Jan 1 10:43:00 2026 +0800
 1 file changed, 1 deletion(-)
 delete mode 100644 feat.txt
$ git log --oneline --graph
* c90d8dd Revert "Merge branch feat"
*   dc9af91 Merge branch feat
|\  
| * 30fe816 c2 加 feat.txt
* | bf6c41a c3 main 自己的改动
|/  
* 0c903f9 c1 起点
```

`feat.txt` 没了，但 `c2` 和那条 Merge 提交都还在历史里。

**一个必须知道的坑（进阶，但很常见）**：
`revert -m 1` 撤销了一次合并，如果你后来把同一个分支**再合并一次**，
Git 会认为「这些提交我早就有了」，于是**原来那些改动不会回来**。
想撤掉一个被 revert 掉的合并，正确做法是 **revert 掉那条 Revert**：
`git revert <那条 Revert 的 hash>`。

**常见翻车**

- **`revert` 一个还没共享的提交**：能用，但历史会留下「做了又撤」两条痕迹。
  本地还没推的提交，用 `reset` 更干净。
- **`git revert HEAD~3..HEAD` 这种范围写法**：会把范围内每条提交各生成一条 Revert，
  顺序是从最旧的开始（Git 会先弹一句 `正在还原 ...`）。撤销一批时这是对的，
  但别指望它只生成一条。

---

### ⑧ 真玩坏了：`reset --hard` 把 5 条提交冲掉，用 `reflog` 捞回来

**这一节是本章的保命符。请务必在沙箱里亲手做一遍。**

**什么情况**
沙箱现场：5 条提交 `辛苦做的提交 1…5`。
你本来想 `git reset --hard HEAD~1`，敲成了 `HEAD~4`。

**第一反应：停手。**
`reflog` 里的位置是**相对现在**的，你每多敲一条会动 HEAD 的命令，
刚才记下的 `HEAD@{1}` 就变了含义。所以：先查，别猜，别连按上箭头重复执行。

**真实输出**

```text
$ git log --oneline
6112255 辛苦做的提交 5
a0a7799 辛苦做的提交 4
bae1ab9 辛苦做的提交 3
0ce3017 辛苦做的提交 2
9507790 辛苦做的提交 1

$ git reset --hard HEAD~4
HEAD 现在位于 9507790 辛苦做的提交 1
$ git log --oneline
9507790 辛苦做的提交 1
$ cat a.txt
第1行内容
$ echo -n "  目录里有："; ls -1 | tr "\n" " "; echo
  目录里有：a.txt note1.md
```

四条提交、五个文件里只剩两个 —— 看起来全没了。现在查后悔药：

```text
$ git reflog
9507790 HEAD@{0}: reset: moving to HEAD~4
6112255 HEAD@{1}: commit: 辛苦做的提交 5
a0a7799 HEAD@{2}: commit: 辛苦做的提交 4
bae1ab9 HEAD@{3}: commit: 辛苦做的提交 3
0ce3017 HEAD@{4}: commit: 辛苦做的提交 2
9507790 HEAD@{5}: commit (initial): 辛苦做的提交 1
```

**怎么读这个表**
`reflog` 是「**HEAD 自己去过的每一个位置**」的流水账，`reset`、`commit`、`checkout`、`rebase`、`merge`
全都会往里写一行。所以它记的是**你的操作轨迹**，而不是「提交历史」。
左边是那次操作之后 HEAD 落在哪个提交，中间是 `HEAD@{n}`（相对现在的第 n 个位置），右边是那次操作的描述。

`HEAD@{1}` 那行 `commit: 辛苦做的提交 5`，就是你作死之前的位置。

**先确认内容真的还在对象库里**（这一步能让你从恐慌恢复到平静）：

```text
$ git show --stat --oneline 6112255f4dce4a29fb837dee121530f733220fcf
6112255 辛苦做的提交 5
 a.txt    | 1 +
 note5.md | 1 +
 2 files changed, 2 insertions(+)
```

**捞回来（方式一，推荐：先建保险分支）**

```text
$ git branch rescue 6112255f4dce4a29fb837dee121530f733220fcf
$ git reset --hard 6112255f4dce4a29fb837dee121530f733220fcf
HEAD 现在位于 6112255 辛苦做的提交 5
$ git log --oneline
6112255 辛苦做的提交 5
a0a7799 辛苦做的提交 4
bae1ab9 辛苦做的提交 3
0ce3017 辛苦做的提交 2
9507790 辛苦做的提交 1
$ cat a.txt
第1行内容
第2行内容
第3行内容
第4行内容
第5行内容
$ echo -n "  目录里有："; ls -1 | tr "\n" " "; echo
  目录里有：a.txt note1.md note2.md note3.md note4.md note5.md
$ git branch -D rescue
已删除分支 rescue（原为 6112255）。
```

为什么先建分支？因为你如果不确定捞的对不对，先给那个 hash 起个名字挂住它，
之后随便怎么试都丢不了。**这是本章最实用的一个习惯。**

**捞回来（方式二：直接引用 `HEAD@{n}`）**

```text
$ git reset --hard HEAD~2
HEAD 现在位于 bae1ab9 辛苦做的提交 3
$ git reflog | head -6
bae1ab9 HEAD@{0}: reset: moving to HEAD~2
6112255 HEAD@{1}: reset: moving to 6112255f4dce4a29fb837dee121530f733220fcf
9507790 HEAD@{2}: reset: moving to HEAD~4
6112255 HEAD@{3}: commit: 辛苦做的提交 5
a0a7799 HEAD@{4}: commit: 辛苦做的提交 4
bae1ab9 HEAD@{5}: commit: 辛苦做的提交 3
$ git reset --hard 'HEAD@{1}'
HEAD 现在位于 6112255 辛苦做的提交 5
```

注意 `HEAD@{1}` 的编号在两次操作之间**已经漂移到别的位置**了：
上面 `head -6` 的输出里，`提交 5` 在 `HEAD@{1}` 和 `HEAD@{3}` 都出现过，因为中间又多了一条操作。
**所以 `HEAD@{n}` 只能当场用，不能写进脚本、笔记、CI 当长期名字。长期要用的是 hash。**

**顺手记一个更强的视角：`git log -g`（把 log 画在 reflog 上）**

```text
$ git log -g --oneline | head -8
9507790 HEAD@{0}: reset: moving to HEAD@{1}
bae1ab9 HEAD@{1}: reset: moving to HEAD~2
6112255 HEAD@{2}: reset: moving to 6112255f4dce4a29fb837dee121530f733220fcf
9507790 HEAD@{3}: reset: moving to HEAD~4
6112255 HEAD@{4}: commit: 辛苦做的提交 5
a0a7799 HEAD@{5}: commit: 辛苦做的提交 4
bae1ab9 HEAD@{6}: commit: 辛苦做的提交 3
0ce3017 HEAD@{7}: commit: 辛苦做的提交 2
```

分支也有自己的 reflog：`git reflog show <分支名>`、`git log -g <分支名>`。
被删掉的分支在它的 reflog 里还留着一段，这是找回「删掉的分支」的办法。

**★ reflog 的边界：`gc` 是真的终点**

沙箱里再造一个最坏情况：提交只做在一条分支上，然后把分支删了。

```text
$ git branch -D doomed
已删除分支 doomed（原为 586aacb）。
$ git log -1 --format='%h %s' 586aacbd8151fcfc5f519612475b1886c236c589
586aacb doomed 分支的独苗
```

没有分支指向它了，但对象还在 —— 直接 `git fsck` 时它甚至不报警，因为 reflog 还兜着：

```text
$ git fsck 2>&1
```

注意这个围栏里命令后面**一行都没有** —— `git fsck` 什么都不输出，这就是「不报警」：
reflog 还兜着，那个提交在 git 看来仍然可达。

把 reflog 也清掉（**正常情况下你永远不该敲这条**，这里只是为了看清边界）：

```text
$ git reflog expire --expire=now --all
$ git fsck --lost-found
悬空 commit 586aacbd8151fcfc5f519612475b1886c236c589
```

「悬空 commit」= 没人引用、但还没被删的提交。用 hash 照样能捞：

```text
$ git reset --hard 586aacbd8151fcfc5f519612475b1886c236c589
HEAD 现在位于 586aacb doomed 分支的独苗
$ echo -n "  目录里有："; ls -1 | tr "\n" " "; echo
  目录里有：base.txt only-doomed.txt
```

**真正的终点是这一步**：

```text
$ git reset --hard HEAD~1
HEAD 现在位于 0815793 c1 起点
$ git reflog expire --expire=now --all
$ git gc --prune=now
$ git cat-file -t 586aacbd8151fcfc5f519612475b1886c236c589
致命错误：git cat-file: could not get object info
```

对象真被删了，神仙难救。

所以：**`git gc --prune=now` 之前，你几乎总能救回来；之后就不行了。**
正常开发中 Git 会自动 gc，默认保留 2 周（`gc.reflogExpire` 一般是 90 天），
所以「第二天醒来发现东西没了」这种事很少发生 —— 但「手动跑了 gc 才发现自己早就该备份」是会发生的。

**本章最值钱的一句话**：

```bash
git branch backup-20260101     # 动手改历史之前，先给当前位置起个名字挂住
```

一条 `git branch` 只要 0.001 秒，事后后悔的代价是重新写三天的代码。**先挂保险，再动手。**

---

## 命令速查表

```bash
# ── 看清现场（永远先做这一步）──────────────────────────────
git status
git status --short
git diff                    # 工作区 vs 暂存区
git diff --cached           # 暂存区 vs 仓库
git log --oneline --graph
git reflog                  # ★ 出事之后的第一反应

# ── 工作区（★ 会丢未提交的改动）────────────────────────────
git restore <文件>                    # 用仓库版本覆盖工作区这个文件
git restore .                         # 整个目录都覆盖（★）
git checkout -- <文件>                # 同上，老写法
git clean -nd                         # 未跟踪文件：只报名单
git clean -fd                         # 真删（★）
git clean -nxd / -fdx                 # 连被 .gitignore 忽略的一起（★，先读名单）

# ── 暂存区（安全，不动工作区）──────────────────────────────
git restore --staged <文件>           # 退出草稿箱，改动留着
git reset HEAD <文件>                 # 同上，老写法

# ── 最后一条提交 ──────────────────────────────────────────
git commit --amend -m "新说明"        # 改说明（hash 变）
git add <漏的> && git commit --amend --no-edit   # 补文件，不改说明

# ── 提交历史（★ 只能在「没被别人拉走过」时用）──────────────
git reset --soft  HEAD~n              # 撤提交，改动留在暂存区
git reset         HEAD~n              # 撤提交，改动退回工作区（默认 --mixed）
git reset --hard  HEAD~n              # 撤提交，改动也别要（★）
git revert        <hash>              # 加一条反向提交（共享历史的安全做法）
git revert -m 1  <merge 的 hash>      # 撤销一条合并提交
git cherry-pick   <hash>              # 搬一条提交过来

# ── 后悔药 ────────────────────────────────────────────────
git reflog                            # HEAD 走过的每一步
git log -g --oneline                  # 把 log 画在 reflog 上
git branch rescue <hash>              # 先挂住，再试
git reset --hard <hash>               # 捞回来
git fsck --lost-found                 # reflog 也没了：找悬空提交
git ORIG_HEAD /  ORIG_HEAD            # git 在危险操作前记的位置：git reset --hard ORIG_HEAD

# ── 不可逆（禁止随手跑）───────────────────────────────────
git reflog expire --expire=now --all  # ★ 把后悔药本身销毁
git gc --prune=now                    # ★ 把对象真删掉，到此为止
```

---

## 这章的坑

**1. `git reset --hard` 顺手「刷新一下」，其实把未提交的改动全砸了。**
很多人以为 `git reset --hard HEAD` 是重载，实际是**把整个工作区砸回当前提交**。
带不带 `HEAD~n` 都危险，因为**你没提交过的那部分改动也一起没了**（场景 ⑤ 的 `a.txt` 第 6 行）。
→ 敲之前先看 `git status`，有未提交改动的就先 `git stash`（06 章）或者 `git branch` 挂住。

**2. `git clean -fd` 不加 `-n` 就先跑了，这是本章事故榜第一名，而且救不了。**
`reset --hard` 至少还有个旧快照躺在对象库里，reflog 能捞；
`clean` 删的是**未跟踪文件**，Git 从来没见过它们，**对象库里根本没有副本，reflog 也查不到**。
`-x` 更狠，会顺手删掉你唯一的 `.env` 副本。
→ 铁律：**`clean` 必须先 `-n`，读一遍名单再动手。**

**3. `restore` / `checkout` 对未跟踪文件完全无效，会让人误以为「撤销不了 = 只能手删」。**
真实报错是 `错误：路径规格 'scratch.txt' 未匹配任何 Git 已知文件`。
看到这条别慌，它说明这个文件 Git 压根不认识，属于「场景 ⑥」的地盘，不是场景 ①。

**4. `git add` 加错了，用 `git restore <文件>` 想取消暂存 —— 结果连改动一起丢了。**
`restore` 不带 `--staged` 时动的是**工作区**，等于「丢弃改动」。
取消暂存要的是 `git restore --staged <文件>` 或 `git reset HEAD <文件>`。
两个命令只差 8 个字符，区别是「改动还在」和「改动没了」。

**5. amend 之后 `git push` 被拒，情急之下敲了裸 `git push -f`。**
被拒的那一刻，Git 正在告诉你「这条历史别人已经拿走了」（场景 ④ 的真实报错原文就是 `non-fast-forward`）。
裸 `-f` 会**把同事已经拉走的提交从远程抽掉**，他下次 pull 会一脸茫然，
更糟的是他那边的工作可能基于那条提交。
→ 判据：**这段历史别人拉走过吗？拉走过 → 只能 revert；本地没推过 → 才允许 amend/reset。**
→ 确实要改写（比如刚推出去 30 秒就发现错了、群里喊一声没人拉）：用 `--force-with-lease`，05 章有实验。

**6. 出事之后连续试了好几条命令，把 reflog 的位置冲乱了。**
`HEAD@{n}` 的 n 是**相对现在**算的，每敲一条动 HEAD 的命令它漂移一格（场景 ⑧ 里 `提交 5` 同时出现在
`HEAD@{1}` 和 `HEAD@{3}`）。你在慌乱中按记忆敲 `HEAD@{2}`，很可能砸到别的地方去。
→ 出事后的正确动作：**先 `git reflog` 看清楚，再选 hash，先 `git branch rescue <hash>` 挂住，然后才 reset。**

**7. `revert -m 1` 撤了一次合并，之后重新合并同一分支时发现「改动没回来」。**
因为那些提交对象本身已经在历史里了，Git 认为它们「不需要再合并一次」。
→ 想恢复被 revert 掉的合并，**revert 掉那条 Revert**：`git revert <Revert 的 hash>`。

**8. 以为 `git reset --hard` 会删掉未跟踪文件，于是看到 status 里还有 `??` 就放心了。**
实际不会删（`--hard` 只管 Git 认识的文件）。反过来说更常见：
**有人以为「status 干净了 = 我的东西都没了」，其实那个未跟踪文件还在**。
这两个方向都记一句：`--hard` 管 Git 认识的，`clean` 管 Git 不认识的。

---

## 自检问题

**Q1：我改坏了 `a.txt`（改完还没 `git add`），同时 `b.txt` 是刚新建的、也从没 add 过。
想只把 `a.txt` 退回、`b.txt` 留着继续改，敲什么？**

**答：** `git restore a.txt`。restore 只碰 Git 认识的文件，`b.txt` 是未跟踪文件，
它既不在 restore 的管辖范围，也不在 `--hard` 的覆盖范围，所以它安然无恙。
别用 `git restore .`（那会把整个目录的已跟踪文件全砸掉），也别用 `git clean`（那会把 `b.txt` 删掉）。

**Q2：`git reset --soft`、`--mixed`、`--hard` 分别在什么场景用？怎么一句话记住？**

**答：** 记住「**越往右越狠**」：soft → mixed → hard。
`--soft` = 只把提交退回草稿箱（改动全在暂存区，随时可以再 commit），用来**重新组织提交**；
`--mixed`（默认）= 提交和暂存都退回，改动留在工作区，用来**重新决定要提交什么**；
`--hard` = 三个区一起砸回那个提交，★ **真丢**，用来**彻底不要这些改动**。
共同点：三条都只移动分支指针，都不会删历史对象，所以 hard 之后仍可用 reflog 救。

**Q3：`git restore --staged a.txt` 和 `git restore a.txt` 差一个参数，差别有多大？**

**答：** 差别是「**改动还在**」和「**改动没了**」。
前者只把暂存区退回仓库版本（相当于取消 `git add`），你硬盘上的文件一个字节都不动；
后者把**工作区**文件用仓库版本覆盖，未提交的改动直接消失。
`git status --short` 里那个 `M` 从第 1 格跳到第 2 格，就是前者；两格都空，就是后者。

**Q4：提交已经 `git push` 给同事了，现在要撤销它。该用 `reset` 还是 `revert`？为什么？**

**答：** **`revert`。** 判据就一句：**这段历史别人拉走过吗？拉走过就不要改写。**
`reset` 让历史变短，你 push 时会被拒（`! [rejected] ... (non-fast-forward)`），
强行 `-f` 就是把同事的工作基础抽掉。`revert` 是往后加一条内容相反的提交，
历史只增长、不缩短，push 一路畅通，而且 log 里看得见「做过又撤了」，可追溯。

**Q5：我 `git reset --hard HEAD~4` 把四条提交冲没了，文件也看不到了。现在该怎么办？**

**答：** **停手，别敲任何命令**（尤其不要重复按上箭头），然后：
1. `git reflog` 找那条 `commit: <你被冲掉的顶端提交>`，抄下它左边的 hash；
2. `git show --stat <那个 hash>` 确认内容还在；
3. `git branch rescue <那个 hash>` 先挂住保险；
4. `git reset --hard <那个 hash>` 回到它。
只有当你已经跑过 `git gc --prune=now`（或 reflog 已过期）才救不回来 —— 那才是真没了。

**Q6：`git checkout -- file`、`git checkout <branch>`、`git switch <branch>`、`git restore file`
这四条里哪两条是同一件事的现代/旧写法？为什么要拆？**

**答：** `git checkout -- file` ≡ `git restore file`；`git checkout <branch>` ≡ `git switch <branch>`。
拆的原因是老 `checkout` 一条命令干三件事（切分支 / 丢文件改动 / 让 HEAD 脱离分支），
最容易出的事故是「以为在切分支，实际在丢改动」。
新命令让名字即语义：**switch 切分支、restore 恢复文件**，读旧文档时认识 `checkout` 即可。

---

## 运行 demo

```bash
cd git-handbook/03-undo
bash demo.sh          # 跑完自动删除沙箱
bash demo.sh --keep   # 跑完保留沙箱并打印路径，你可以进去自己敲命令
```

`demo.sh` 会自己 `mktemp -d` 建沙箱，**绝不碰你现有的仓库**，全程不联网（需要「远程」时用本地 bare 仓库冒充）。
脚本按本章 ①→⑧ 的顺序演示，每一步都会先打印**正在演示什么**和**即将执行的命令**，
输出读起来就是一份讲义。想只看某一节，可以 `bash demo.sh --keep` 跑完后 `cd` 进沙箱里的那个目录自己复现。

沙箱结构（`--keep` 后你会看到）：

| 目录 | 对应场景 | 里面留着什么 |
|---|---|---|
| `s1/` | ① 工作区改坏 | `note.txt` 已还原，还有一个未跟踪 `scratch.txt`（restore 不掉的那个） |
| `s2/` | ② 取消暂存 | `a.txt` / `b.txt` 的最终状态 |
| `s3/` | ③ amend 改说明 | reflog 里能看到 `commit (amend)` |
| `s4/` | ④ amend 补文件 + 已推送的边界 | 它的 `origin` 是沙箱根上的兄弟目录 `../s4-origin.git`（本地 bare 冒充的远程），不在 `s4/` 里面 |
| `s5-soft/` `s5-mixed/` `s5-hard/` | ⑤ 三兄弟对比 | **同一份现场，三种结局**，最值得逐个 `git status` 对比 |
| `s6/` | ⑥ clean | 已经被清干净了 |
| `s7a/` | ⑦-a cherry-pick | `main` 上那条搬来的提交，reflog 里还能看到 abort |
| `s7b/` `s7c/` | ⑦-b revert | `s7b` 有 Revert + revert 撞冲突；`s7c` 有 `revert -m 1` |
| `s8/` `s8b/` | ⑧ reflog 救援 | `s8b` 跑过 `gc --prune=now`，对象真没了（这就是边界的样子） |

> 时间戳是固定的（`2026-01-01T10:01:00 +0800` 起逐条递增），
> 所以你在自己机器上跑，hash 会和本文一模一样。
> 如果你的 Git 输出是英文，`export LC_ALL=zh_CN.UTF-8` 再跑一遍就能对上本文。

**下一章**：04 分支与合并冲突 —— 撤销搞的是「一个分支上的过去」，下一章开始搞「两条分支的现在」，
包括那个坑过无数人的 `--ours` / `--theirs` 在 merge 和 rebase 里含义互换。
