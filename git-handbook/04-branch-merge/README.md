# 04 分支与合并冲突（branch / merge / conflict）

> 一句话：这一章解决「两条线各走了一段，现在要合成一条」——
> 分支到底是个什么东西、合并为什么有时不留痕迹有时留一个菱形、
> **冲突那几行 `<<<<<<< ======= >>>>>>>` 到底在读什么**、以及合坏了怎么退回。

前置：01 章的「三个区 + 分支就是一个文本文件」、02 章的 `add`/`commit`、03 章的 `reset`/`revert`/`reflog`。
本章开始要处理**多个指针同时存在**的局面，所以每一条命令都请先用 `git log --graph` 看清楚形状再敲。

本章最重要的三个认知，先放在这里：

1. **分支不是容器，是一个写着哈希的文本文件。** 快进合并之所以 0 成本，就是因为它只改这一行字符串，
   连一个新对象都不产生（本章 场景 ④ 用 `find .git/objects | wc -l` 现场数给你看）。
2. **合并冲突不是 git 坏了，是它在等你做决定。** 它把两边内容都塞进同一个文件，
   并且把**共同祖先**也留在暂存区里（`:1:` `:2:` `:3:` 三个版本），你亲手删掉标记行、再 `git add`，
   才算说了「我决定了」。**`git add` 在冲突现场不是「暂存改动」，而是「报告这个文件我处理过了」。**
3. **`--ours` / `--theirs` 在 merge 和 rebase 里指的是相反的两条线。** 这是本章事故榜第一名，
   场景 ⑪ 用同一份冲突文件在两种情境下各取一次，输出并排给你看。

> 本章输出**全部来自仓库里的 `bash demo.sh`**（A→J 十段，1067 行输出，退出码 0），本文围栏里的每一行
> 都能在那份输出里逐字找到。想复核就 `bash demo.sh > /tmp/git04.txt 2>&1`，然后按「运行 demo」一节的办法对。
> 输出固定了身份和时间戳（`学习者 <learn@example.com>`，从 `2026-01-01T10:01:00 +0800` 逐条递增），
> 而且脚本里设了 `GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null`，
> 所以你机器上的 `~/.gitconfig`（别名、`pull.rebase`、`advice.*`）不会混进这份输出，
> 你复现出来的**哈希和本文一致**（已连跑两次核对，逐字节相同，只有沙箱目录的随机后缀会变）。
> ⚠ `demo.sh` 里设的是 `LANG=zh_CN.UTF-8`，所以本章输出是**中文**（和 01、07 章的英文输出不同）；
> 但 `Fast-forward`、`Merge made by the 'ort' strategy.` 这几句 git 没有中文翻译，
> 你在中文机器上看到的也是英文原句，别以为是两种不同的东西。
>
> **本章没演 / 没验证的**（别处看到的说法，这里不敢替你背书）：
> - `git rerere`（重复冲突自动复用上次的解决结果）—— 本章一行没跑。
> - `git mergetool` 只演了「外部命令改写文件」这个机制（用假工具），**任何真实的图形工具（meld / vimdiff / VS Code）都没跑过**。
> - octopus 合并（`git merge a b c`）、`--squash` 的冲突现场、`diff3` 冲突样式（本章只跑了 `zdiff3`）：没实跑。
> - `.git/objects` 的**打包后**形态（`git gc` 之后 `find` 数出来的就不是这个数了）：本章只数了刚提交完的松散对象。

---

## 你会遇到的场景

| 你遇到的事 | 本章哪一节 |
|---|---|
| 分支建了一堆，`git branch` 只有名字，完全看不出谁落后谁没合 | 场景 ①（`branch -vv` / `--merged` / `--no-merged` / `log --graph`） |
| 想删分支，`-d` 报错不给删；或者 `-d` 竟然放行，事后发现里面还有东西 | 场景 ②（`-d` 的真实判断标准 + 删错了怎么复活） |
| 别人一条分支摆在那儿，我想合进来，但**不知道该合的是什么** | 场景 ③（先看再说：`HEAD..分支`、三点两点、`--no-commit` 试合） |
| 合并完历史变成一条直线，看不出这里合过东西 | 场景 ④⑤（fast-forward 与 `--no-ff` 的取舍） |
| `git pull` 莫名其妙多了个 Merge 提交，想禁止 | 场景 ⑥（`--ff-only`） |
| 两边改了不同地方，git 自己合了 —— 它凭什么敢自己合？ | 场景 ⑦（三方合并 + `merge-base`） |
| **冲突文件里那几行标记怎么看？`HEAD` 那半是我改的还是他改的？** | 场景 ⑧（逐行读标记 + `:1:/:2:/:3:` + `zdiff3`） |
| 改完冲突文件，`git commit` 说 `error: Committing is not possible because you have unmerged files` | 场景 ⑨（为什么必须再 `git add`） |
| 合到一半不想合了，想退回合并前，一丝痕迹不留 | 场景 ⑩（`--abort` vs `--quit`） |
| 在 rebase 里取 `--ours`，结果取到了 main 的内容，一脸茫然 | 场景 ⑪（★ ours/theirs 含义互换） |
| 对方重命名了文件、我改了内容，冲突长得完全不一样 | 场景 ⑫（改名/删除/add-add，含改名阈值的真相） |
| 只想看「主线都合进来了哪些东西」 | 场景 ⑬（`--merges` / `--first-parent`） |
| 手滑把不该合的分支合进来了，而且已经生成合并提交 | 场景 ⑭（`ORIG_HEAD` / `reflog` / `revert -m 1`） |

---

## 心智模型

### 图 1：合并只有两种物理动作 —— 挪指针，或者造一个双亲提交

```
   起点：两条分支各自指着某个提交，objects 里的链只有一条（分叉前共用）

        main ──→ C1 ──→ C2 ──→ C3            ← feature 从 C3 分出去
                        │
                        └──→ F1 ──→ F2        ← feature

   ── 结局一：fast-forward（main 一步没动，feature 在它「正前方」）──

        物理动作：把 main 这个文本文件里的哈希从 C3 改成 F2。**没有新提交，没有新对象。**

        main ─┐
   feature ───┴──→ 都指向 F2

   ── 结局二 / 三：main 也动了 → 不能快进 → 造一个「有两个父亲的提交」──

        物理动作：新建 M，parent 写两行（先 C 后 F），tree 是三方合并算出来的结果快照。

                 ┌── C4 ────────┐
        C3 ──────┤              M ← main
                 └── F1 ── F2 ──┘        ← feature

   ★ 这个 M 就是「合并提交」。它不是补丁，也是一份完整快照（01 章讲的），
     只不过它有两个 parent，所以「上一个」这个说法从此不再唯一。
```

所以「合并」这个词骗人。git 从来没把东西**搬**到一起，它只是：
**算出一份新的目录快照（把两边的改动叠起来），造一条指向两份旧提交的记录，然后把你的分支指针挪到这条新记录上。**
叠不出唯一结果的那些行，就是冲突。

### 图 2：合并的三种结局（先记住这张表，再学命令）

| 形状 | 条件 | git 干什么 | 输出长什么样 | 新对象 | 典型用途 |
|---|---|---|---|---|---|
| **fast-forward**（快进） | 对方在你正前方，你一步没动 | 只改 `refs/heads/你的分支` 里那行哈希 | `更新 a88c7bb..8017b77` + `Fast-forward` | **0 个** | 个人分支随手并一下，历史保持直线 |
| **三方合并自动成功** | 两边分叉，但改的不是同一批行 | 造一个双亲合并提交 | `自动合并 doc.md` + `Merge made by the 'ort' strategy.` | 1 个 commit（tree 能复用就复用） | 默认结局；`--no-ff` 可以强行制造这种形状 |
| **冲突** | 两边改了**同一行**（或一边改名/删除、一边修改） | 把两边塞进工作区文件、在暂存区留 3 个版本，然后停手 | `冲突（内容）：合并冲突于 xxx` + 退出码 1 | 0 个（等你手工收尾） | 必须人来决定 |

一句判据（场景 ③ 现场验证）：**能不能快进，不是靠看，是靠 `git merge-base --is-ancestor A B` 的退出码。**

### 图 3：冲突的物理真相 —— 暂存区里有三个版本，工作区里那份是给你读的

正常时候暂存区每个文件只有一行、stage 号是 `0`。一旦冲突，同一个文件会变成**三行**
（沙箱 `i-merge-state/`，demo I1 撞出来的现场；`-u` = 只列未合并的）：

```text
$ git ls-files -u
100644 8050857d8e841257c3704f8155fcf188f44bfc07 1	config.ini
100644 baeaeb474ddbed91d578c5675750293c958fb200 2	config.ini
100644 af26ba9d7d5630fc5cdbadabb39c34441ce77b5a 3	config.ini
```

```
最后一列那个数字就是「阶段号」，它才是 ours/theirs 的真身：

  1  = :1: = 共同祖先（base）      「原来是什么」
  2  = :2: = ours   = 当前底本那一侧   「我这边站的地方改成了什么」
  3  = :3: = theirs = 这一次叠进来的那一侧 「对方改成了什么」

第一列 100644 是文件模式，第二列是那份内容在对象库里的 blob 号。
```

### 全景表：本章命令按「先看清 / 再决定 / 收尾 / 救火」排

| 阶段 | 命令 | 一句话 |
|---|---|---|
| 看清 | `git log --oneline --graph --decorate --all` | 本章的地基，形状只有画出来才看得懂 |
| 看清 | `git branch -vv` | 一行给出上游 + 领先/落后几个 |
| 看清 | `git branch --no-merged` | 哪些分支的内容还没进当前分支 |
| 看清 | `git log --oneline HEAD..feature` | 对方有、我没有的提交 |
| 看清 | `git log --oneline --left-right HEAD...feature` | 一行同时给出两边各有谁 |
| 看清 | `git diff --stat HEAD...feature` | 对方到底动了哪些文件（三点 = 相对共同祖先） |
| 决定 | `git merge feature` | 默认：能快进就快进 |
| 决定 | `git merge --no-ff feature` | 强行留菱形，整个功能可当成一个整体回退 |
| 决定 | `git merge --ff-only feature` | 只许快进，否则直接中止（`pull --ff-only` 同理） |
| 决定 | `git merge --no-commit --no-ff feature` | 试合：叠好但不提交，不满意 `--abort` 走人 |
| 收尾 | `git status` / `git ls-files -u` | 冲突现场还剩几个文件没决定 |
| 收尾 | `git checkout --ours\|--theirs <文件>` | 整份文件取一边（**不是**逐行取） |
| 收尾 | `git add <文件>` | 「这个文件的冲突我处理过了」 |
| 收尾 | `git commit` | 合并才算完成 |
| 救火 | `git merge --abort` | 退回合并前，`MERGE_*` 一起删掉 |
| 救火 | `git merge --quit` | ⚠ 只删 `.git/MERGE_*` 小抄，文件和暂存区都不还原 |
| 救火 | `git reset --hard ORIG_HEAD` | 已经提交了的合并（本地未 push） |
| 救火 | `git revert -m 1 <合并提交>` | 已经 push 出去的合并 |

### `git log --graph` 到底怎么读

本章每一条 `--graph` 输出都由四种字符组成，认全了就再也不用脑补：

```
* 6549f5d (HEAD -> main) c4 main 往前走了一步        ← 一个提交
| * 905ec49 (feature/login) c3 加表单校验            ← 另一条线上的提交
|/                                                    ← 两条线在这里并成一条（共同祖先那一行开始合流）
* 808e308 c1 初始提交

  ★ 竖直的 | 是「这条线还在往上（往过去）延伸」；
  ★ 斜线 / 和 \ 是「线的端点在合流/分叉」；
  ★ 菱形合并长这样（场景 ⑤ 的原文）：
        *   5178585 (HEAD -> main) 合并 feature 功能分支    ← 双亲提交
        |\                                                   ← 从这里分成两条父亲线
        | * ba738d0 (feature) c2 加 extra.txt                ← 第 2 个父亲（合进来的那条）
        * | df1f4f0 c3 main 也动了                           ← 第 1 个父亲（我原来站的线）
        |/
        * 8861495 c1 初始
  ★ 左边那条 * 一列 = 你的主线（first parent）；只要盯着最左列，
    就能把「主线历史」和「从别处合进来的支线」分开看（配合 --first-parent，场景 ⑬）。
```

`--decorate` 打印括号里的指针名（`HEAD -> main`、`origin/feature/login`）；
`--all` 才是「所有分支都列出来」——不加它，只显示当前分支可达的历史，分叉那条线**根本看不见**，
这是新手看到「历史怎么少了半截」的头号原因。

---

## 场景逐个实战

### ① 分支建了一堆，只看名字等于什么都没看

**什么情况**
项目里躺着 6 条分支，你 `git branch` 一看只有名字，不知道哪条已经合了、哪条和远程差多少、
哪条根本没上游。于是删错分支、合错分支、push 推到别人的分支上。

**敲哪几条**

```bash
git branch                    # 只有名字和 *
git branch -a                 # 把 remotes/origin/* 也列出来
git branch -vv                # ★ 推荐：一行同时给出上游和领先/落后
git log --oneline --graph --decorate --all   # ★ 形状
git branch --merged           # 已经并进当前分支的
git branch --no-merged        # 还没并进来的
```

**真实输出**（沙箱 `a-branches/`，注意这个仓库配了一个本地 bare 仓库冒充 `origin`）

```text
$ git branch
  feature/login
* main

$ git branch -a
  feature/login
* main
  remotes/origin/feature/login
  remotes/origin/main

$ git branch -vv
  feature/login 367a52f c3 加表单校验
* main          a07cfb0 [origin/main] c4 main 往前走了一步
```


**形状才是重点**

```text
$ git log --oneline --graph --decorate --all
* a07cfb0 (HEAD -> main, origin/main) c4 main 往前走了一步
| * 367a52f (origin/feature/login, feature/login) c3 加表单校验
| * 0124d6e c2 加登录页
|/  
* 1bed28d c1 初始提交
```

一行一行对上：`main` 和 `origin/main` 指在同一个提交 `a07cfb0` 上（所以方括号里不报领先落后）；
`feature/login` 和 `origin/feature/login` 指在 `367a52f` 上；
两条线在 `1bed28d`（c1 初始提交）合流 —— 这一行前面是 `*` 没有竖线，说明它是**唯一的共同祖先**。

**「谁已经合了」**

```text
$ git branch --merged
* main

$ git branch --no-merged
  feature/login
```

`--merged` 只列出了 `main` 自己：意思是「**这些分支的内容已经完全包含在当前分支里**，删了不会丢东西」。
`feature/login` 出现在 `--no-merged` 里 = 它有 2 条提交（`0124d6e`、`367a52f`）还没进 `main`。

**常见翻车**

- 用 `git log --oneline --graph` 不带 `--all`，只看到当前分支，然后以为「feature 的提交丢了」。
- 把 `remotes/origin/xxx` 当成远程的**实时**状态 —— 它是你上次 `fetch` 时的快照（第 07 章专讲这个坑）。
- `git branch -a` 看完以为分支很多。本地分支和远程跟踪分支是两套 ref，删本地的那条不会删远程的，反之也一样。

### ② 删分支：`-d` 真的保护你吗

**什么情况**
功能做完了想清理分支。你以为 `git branch -d` 是安全版、`-D` 是危险版，
但真实情况是：`-d` 有时候拒得莫名其妙，有时候**放行了本该让你后悔的内容**。

**真实输出：`-d` 拦住你的那一面**

```text
$ git branch --merged
* main
  topic

$ git branch -d topic
已删除分支 topic（原为 a07cfb0）。

$ git branch --no-merged
  feature/login-ui
  wip/experiment

$ git branch -d wip/experiment
错误：分支 'wip/experiment' 没有完全合并
提示： 如果您确认要删除它，执行 'git branch -D wip/experiment'
提示： Disable this message with "git config set advice.forceDeleteBranch false"

$ git branch -D wip/experiment
已删除分支 wip/experiment（原为 41414c6）。
```

`topic` 是**用当前 main 直接建的分支**（`git branch topic`，内容一模一样，所以它天然「已合并」），`-d` 秒删。
`wip/experiment` 上有一条只有它自己有的提交（`41414c6 c5 只在 wip/experiment 上的提交`），`-d` 拒绝。
这就是 `-d` 的本职：**替你看住「只有这条分支指着」的内容。**

**但 `-d` 不是「绝对安全」**：它的判断标准是「相对 **HEAD** **或**相对 **这条分支自己的上游**」已合并。
实测另一条 `feature/ghost` —— 它**没并进 main**，`-d` 却只警告一句就把分支删了（因为它已经推到了自己的上游）：

```text
$ git branch -d feature/ghost
警告：将要删除的分支 'feature/ghost' 已经被合并到
         'refs/remotes/origin/feature/ghost'，但未合并到 HEAD
已删除分支 feature/ghost（原为 c2c0196）。
```

所以删之前真正该数的是这一条（有几个提交还没进去，一眼看到）：
`git log --oneline main..feature/login-ui` → `367a52f c3 加表单校验` / `0124d6e c2 加登录页`。

**删错了的后悔药**就是那句「原为 <hash>」里的哈希（分支自己的 reflog 跟着分支一起没了）：

```text
$ git branch wip/experiment 41414c6e23d0776d36c28b7a3a0891866460d6e5
$ git show wip/experiment:secret.txt
这条改动只存在于 wip/experiment
```

`git show <分支>:<文件>` 很划算：**不用切分支**就能读那条分支上任意文件的内容
（分支删了照样能读 —— objects 里的快照一直在，只是没人引用它，这条线索第 08 章会放大成主角）。

### ③ 合并别人分支之前，先「看再说」：`HEAD..分支` 这一套

**什么情况**
同事（或上周的你）留了一条 `feature/login` 摆在那儿，说「合进来吧」。
你直接 `git merge`，结果合出一堆冲突、或者合进来一堆你根本没打算要的东西。
**合并前花 30 秒把「要合的到底是什么」问清楚，是本章性价比最高的习惯。**

> 这一节是 demo 的 **G 段**（沙箱 `g-checklist/`）：现场和 `a-branches/` 一样 ——
> `feature/login` 上有 2 条提交，`main` 上也往前走了 1 条，两边分叉。

**第 1～4 步：形状 + 谁有什么**

```text
$ git log --oneline --graph --decorate --all
* c3c7781 (HEAD -> main) c4 main 往前走了一步
| * bd1d521 (feature/login) c3 加表单校验
| * a5650be c2 加登录页
|/  
* 472f732 c1 初始提交

$ git branch --merged
* main

$ git branch --no-merged
  feature/login

$ git log --oneline HEAD..feature/login
bd1d521 c3 加表单校验
a5650be c2 加登录页

$ git log --oneline feature/login..HEAD
c3c7781 c4 main 往前走了一步
```

`A..B` 读作「**在 B 里、不在 A 里**的提交」。所以 `HEAD..feature/login` = 我合进来会新增哪些东西；
反过来 `feature/login..HEAD` = 对方缺我哪些（这决定了能不能快进）。
两个列表都非空 → **分叉了**，合并一定会产生合并提交（或者直接冲突）。

嫌两条命令麻烦就一条搞定（`<` 是我独有、`>` 是对方独有）：

```text
$ git log --oneline --left-right HEAD...feature/login
< c3c7781 c4 main 往前走了一步
> bd1d521 c3 加表单校验
> a5650be c2 加登录页
```

**第 6、7 步：三点与两点的区别（本章必考）**

```text
$ echo "共同祖先 = $(git rev-parse --short "$(git merge-base HEAD feature/login)")"
共同祖先 = 472f732

$ git diff --stat HEAD...feature/login
 login.js | 2 ++
 1 file changed, 2 insertions(+)

$ git diff --stat HEAD..feature/login
 login.js | 2 ++
 plan.md  | 1 -
 2 files changed, 2 insertions(+), 1 deletion(-)
```

同一条 `git diff --stat`，两点/三点结果不一样，差别就在 `plan.md`：

```
  HEAD..feature（两点）  = 直接比较两个顶端快照  → 把「你自己对 plan.md 的改动」算成对方「删掉了一行」
  HEAD...feature（三点） = 从 merge-base 出发比   → 只显示【对方相对共同祖先】真正动过的东西
```

**想知道「这次合并会给我带来什么」，永远用三点。**
`git diff HEAD...feature` 的语义正好等于 git 做三方合并时的「对方那一份差异」。
（这个两点/三点差别在 `git log` 里含义又略有不同，第 08 章「排查取证」里会再对比一次。）

**第 8 步：能不能快进，用退出码问**

```text
$ git merge-base --is-ancestor HEAD feature/login; echo "退出码=$?"
退出码=1
```

这条命令**什么都不打印**，答案全在退出码里：`0` = HEAD 是 `feature/login` 的祖先，也就是没分叉、能快进；
`1` = 分叉了，快进不可能。上面这个现场给的是 1。

同一个沙箱里另有一个「main 一步没动」的可快进现场（demo H1，`h-ffobjects/`），同一条命令给的就是 0：

```text
$ git merge-base --is-ancestor HEAD feature; echo "退出码=$?"
退出码=0
```

**第 9 步：试合（这是「先看再说」的终点）**

```text
$ git merge --no-commit --no-ff feature/login
自动合并进展顺利，按要求在提交前停止

$ git status
位于分支 main
所有冲突已解决但您仍处于合并中。
  （使用 "git commit" 结束合并）

要提交的变更：
	新文件：   login.js

$ git diff --cached --stat HEAD
 login.js | 2 ++
 1 file changed, 2 insertions(+)

$ git merge --abort
$ git log --oneline -1
c3c7781 c4 main 往前走了一步
$ git status --short
```

`--no-commit` 让 git 把改动叠好、放进暂存区，然后**停在提交之前**等你过目；
`git diff --cached --stat HEAD` 就是「如果我现在提交，会多出什么」。
看完不满意 → `git merge --abort`，现场完全复原（`git status --short` 输出为空，log 顶端还是 `c3c7781`）。

⚠ 那句 `所有冲突已解决但您仍处于合并中` 里的「冲突」二字是 git 的固定措辞，
**根本没有冲突时也这么说**（本次一个冲突都没有）。看到它就理解为「合并还没提交」即可。

**常见翻车**

- 用两点 `git diff HEAD..feature` 去估计「对方改了啥」，把**自己的**改动当成对方删掉了，越看越慌。
- 试合时忘了 `--no-commit`，合完才发现不想合，只能走 `reset --hard ORIG_HEAD`（场景 ⑭）。
- `merge-base` 有多个共同祖先时（合并过多次的分支）git 只报**最佳**那一个，这正常，别以为算错。

### ④ 结局一：fast-forward —— 指针挪一格，一个新对象都不产生

**什么情况**
你在功能分支上干了几天活，主线一步没动。回主线一合，历史变成一条直线，
连「这里合过一次」都看不出来。你怀疑合并没生效。

**现场与合并**（沙箱 `b-ff/`：main 停在 c1，feature 上有 c2、c3）

```text
$ git log --oneline --graph --decorate --all
* 8017b77 (feature) c3 改 doc.md
* 885bae5 c2 加 extra.txt
* a88c7bb (HEAD -> main) c1 初始

$ git merge feature
更新 a88c7bb..8017b77
Fast-forward
 doc.md    | 1 +
 extra.txt | 1 +
 2 files changed, 2 insertions(+)
 create mode 100644 extra.txt

$ git log --oneline --graph --decorate --all
* 8017b77 (HEAD -> main, feature) c3 改 doc.md
* 885bae5 c2 加 extra.txt
* a88c7bb c1 初始
```

三个必须看懂的点：

1. `更新 a88c7bb..8017b77`：这是两点写法，意思是「把 main 从 `a88c7bb` **挪到** `8017b77`」——
   它就是在描述一次指针赋值，没别的动作。
2. `Fast-forward` 这句是英文，且**没有新提交产生**（对比场景 ⑤ 的 `Merge made by the 'ort' strategy.`）。
3. 合并后 `* 8017b77 (HEAD -> main, feature)` —— 两个分支名并排出现在同一个提交上，
   这就是「两条线现在指着同一个哈希」。

**铁证：分支只是文本，快进不产生对象**（demo H 段，沙箱 `h-ffobjects/`，和上面同一份形状）

```text
$ grep "" .git/refs/heads/*
.git/refs/heads/feature:0bbda5554ee99273cd0496645da3cbf027a045f0
.git/refs/heads/main:c2e83afc8919bcc6c4b55e1c659848fab3928f8a

$ find .git/objects -type f | wc -l
9

$ git merge feature
更新 c2e83af..0bbda55
Fast-forward
 doc.md    | 1 +
 extra.txt | 1 +
 2 files changed, 2 insertions(+)
 create mode 100644 extra.txt

$ grep "" .git/refs/heads/*
.git/refs/heads/feature:0bbda5554ee99273cd0496645da3cbf027a045f0
.git/refs/heads/main:0bbda5554ee99273cd0496645da3cbf027a045f0

$ find .git/objects -type f | wc -l
9
```

`.git/refs/heads/main` 里那一串字符串从 `c2e83af…` 变成 `0bbda55…`（和 feature 完全相同），
而 `.git/objects` 下的文件数 **9 → 9，一个都没多**。
快进合并的全部成本 = 改写一个 41 字节的文本文件。这就是「合并为什么这么快」的最终答案。
（⚠ 这个 9 是**刚提交完、还没 `git gc`** 时的松散对象数；打包成 pack 之后 `find` 出来的就不是这个数了。）

### ⑤ `--no-ff`：技术上能快进，也强行留一个菱形

**什么情况**
功能分支上的 12 条碎提交（`wip`、`改一下`、`又改`、`typo`）你不想让它们混进主线历史里，
你想让「这个功能」在 log 里是**一个整体**，出问题能一次撤掉。

**demo 用的现场是「main 也动了 + --no-ff」**（沙箱 `b-noff2/`）

```text
$ git log --oneline --graph --decorate --all
* df1f4f0 (HEAD -> main) c3 main 也动了
| * ba738d0 (feature) c2 加 extra.txt
|/  
* 8861495 c1 初始

$ git merge --no-ff feature -m "合并 feature 功能分支"
Merge made by the 'ort' strategy.
 extra.txt | 1 +
 1 file changed, 1 insertion(+)
 create mode 100644 extra.txt

$ git log --oneline --graph --decorate --all
*   5178585 (HEAD -> main) 合并 feature 功能分支
|\  
| * ba738d0 (feature) c2 加 extra.txt
* | df1f4f0 c3 main 也动了
|/  
* 8861495 c1 初始
```

注意：**这个现场本来就必须造合并提交**（两边分叉了，快进不可能），所以 `--no-ff` 在这里没有额外效果。
真正体现 `--no-ff` 价值的是「**明明能快进**，我偏不要」——demo H3（沙箱 `h-noffobjects/`），
和场景 ④ 一模一样的形状，只是把 `git merge feature` 换成 `--no-ff`：

```text
$ find .git/objects -type f | wc -l
9

$ git merge --no-ff feature -m "合并 feature"
Merge made by the 'ort' strategy.
 doc.md    | 1 +
 extra.txt | 1 +
 2 files changed, 2 insertions(+)
 create mode 100644 extra.txt

$ find .git/objects -type f | wc -l
10

$ git cat-file -p HEAD
tree 70a5a707de30acb29d6295b7dd7896691633162e
parent 0d968d323d4cedcdb20b1f3589e2e7e874b3fb01
parent 83a427d60d0a693b43f5540446d4c72a5df35749
author 学习者 <learn@example.com> 1767235980 +0800
committer 学习者 <learn@example.com> 1767235980 +0800

合并 feature

$ echo "HEAD^1 = $(git rev-parse --short HEAD^1)  HEAD^2 = $(git rev-parse --short HEAD^2)"
HEAD^1 = 0d968d3  HEAD^2 = 83a427d
```

同一份现场，`--no-ff` 比快进多出来的就是**这 1 个对象**：一条有两个 `parent` 行的 commit
（tree `70a5a70…` 直接复用 feature 那份，所以只多 1 个而不是 2 个）。
两个父亲分别是 `0d968d3`（HEAD^1，你原来站的 main）和 `83a427d`（HEAD^2，合进来的 feature）。

**为什么功能分支常用 `--no-ff`**

1. 菱形在 `log --graph` 里看得见「这是一整个功能合进来的边界」。
2. 想反悔时 `git revert -m 1 <合并提交>` **一次撤掉整个功能**（03 章 ⑦-b 实测过；本章场景 ⑭ 再说时机）。
3. 主线上的碎提交噪音被关在菱形里面，`--first-parent` 一筛就干净（场景 ⑬）。

代价：历史不再是直线，`git log` 里要多读 `|\` 这些线。

**常见翻车**

- 以为 `--no-ff` 会「保留分支结构」→ 它只是**多造一个提交**，分支文件本身不受影响。
- 用了 `--no-ff` 却忘了它可能带冲突：`--no-ff` 只决定形状，不改变冲突判定。
- 团队约定用 `--no-ff`，但有人用 `git pull` 合回来了（默认能快进就快进）→ 形状不一致。
  治法：`git config --local pull.ff only`（写 `.git/config`，别 `--global`），或统一用 `git merge --no-ff` 显式合。

### ⑥ `--ff-only`：把「只许快进」变成硬约束

**什么情况**
你绝不想要 git 偷偷造合并提交（尤其是 `git pull` 的时候）。
你想表达的是：**能快进就合，不能就停下来告诉我。**

**真实输出**（沙箱 `b-ffonly/`，两边分叉）

```text
$ git log --oneline --graph --decorate --all
* 83c5f8e (HEAD -> main) c3 主线改动
| * 52ef9e8 (feature) c2 功能第一步
|/  
* 65fdccc c1 初始

$ git merge --ff-only feature
提示： 无法在偏离的分支上进行快进操作，您需要:
提示：
提示： 	git merge --no-ff
提示：
提示： 或者：
提示：
提示： 	git rebase
提示：
提示： Disable this message with "git config set advice.diverging false"
致命错误：无法快进，中止。
```

这一屏值得逐行读：

- **没有任何半吊子状态**：报完错，暂存区、工作区、分支指针全都没动。这就是 `--ff-only` 的价值 ——
  失败就是彻底失败，而不是「先合上再说」。
- 提示里给了两条出路：`git merge --no-ff`（承认要菱形）或 `git rebase`（把自己的提交搬到对方后面，第 05 章）。
- 最后一行 `致命错误：无法快进，中止。` 是**真正的错误**（退出码 128），
  脚本里能靠它判断成败；上面那些 `提示：` 只是建议，可以关掉。

**最常见的用法是 `git pull --ff-only`**：宁可拉取失败，也不要 pull 自作主张造一个 `Merge branch 'main' of ...` 提交。
很多人被这种「莫名多出来的合并提交」恶心过，根源就是 pull 的默认策略（07 章有完整对照）。

### ⑦ 结局二：三方合并自动成功 —— git 凭什么敢自己决定

**什么情况**
两边都改了同一个文件，git 一句话就合完了，连冲突都没报。你有点慌：它是不是偷偷丢了东西？

**真实输出**（沙箱 `b-auto/`，8 行的 `doc.md`，feature 改第 6 行、main 改第 2 行）

```text
$ git log --oneline --graph --decorate --all
* 87ef4f5 (HEAD -> main) c3 主线改了第2行
| * e7b9ab7 (feature) c2 功能分支改了第6行
|/  
* dea2ff4 c1 共同的起点

$ git merge feature
自动合并 doc.md
Merge made by the 'ort' strategy.
 doc.md | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)

$ git log --oneline --graph --decorate
*   dd260d1 (HEAD -> main) Merge branch 'feature'
|\  
| * e7b9ab7 (feature) c2 功能分支改了第6行
* | 87ef4f5 c3 主线改了第2行
|/  
* dea2ff4 c1 共同的起点
```

`自动合并 doc.md`（英文界面是 `Auto-merging doc.md`）= git 用三方对比算出了唯一结果。
`Merge made by the 'ort' strategy.` = 造了合并提交（`ort` 是当前默认策略，取代了老的 `recursive`；
它额外擅长处理改名，细节见场景 ⑫）。

**「三方」是哪三方**

```text
$ git log -1 --format='%h 的父亲们：%p'
dd260d1 的父亲们：87ef4f5 e7b9ab7

$ echo "  HEAD^1 = $(git rev-parse --short HEAD^1)  HEAD^2 = $(git rev-parse --short HEAD^2)"
  HEAD^1 = 87ef4f5  HEAD^2 = e7b9ab7

$ git log --oneline -1 HEAD^1
87ef4f5 c3 主线改了第2行

$ git log --oneline -1 HEAD^2
e7b9ab7 c2 功能分支改了第6行

$ echo "两条线的共同祖先 = $(git merge-base HEAD^1 HEAD^2)"
两条线的共同祖先 = dea2ff4011f6219a0f20bc46ec8595ac080519b2

$ git log --oneline -1 $(git merge-base HEAD^1 HEAD^2)
dea2ff4 c1 共同的起点
```

```
        三方 = ① 共同祖先 base（dea2ff4）   ← 参照物：「原来是什么」
               ② 我们 ours  （HEAD^1 = 87ef4f5）
               ③ 他们 theirs（HEAD^2 = e7b9ab7）

        判定规则（对每一段差异分别做）：
          base=A，ours=A，theirs=B   →  我们没改，对方改了 → 取 B（自动）
          base=A，ours=B，theirs=A   →  对方没改，我们改了 → 取 B（自动）
          base=A，ours=B，theirs=C   →  两边都改了且不同   → ★ 冲突，人来决定
          base=A，ours=A，theirs=A   →  没差异，跳过
```

「为什么敢自己合」的答案就在这里：**它比较的不是两个版本，而是三个版本**。
只要某一段只有一边动过，结果就是唯一的，git 就替你定；两边都动了同一行，它才停下。

### ⑧ 结局三：冲突到底长什么样（本章最重要的一节）

**什么情况**
两边改了同一行。git 停了。你现在面对一屏从没见过的符号，不敢动。

**现场**（沙箱 `b-conflict/`：feature 把 `线程` 改成 8 并加了 `最大连接`；main 把端口 3000→9090、`线程` 改成 4）

```text
$ git log --oneline --graph --decorate --all
* c4dd4d5 (HEAD -> main) main: 线程 = 4，顺手把端口也改了
| * 7318c7e (feature) feature: 线程 = 8
|/  
* 8c43cb7 c1 初始配置

$ git merge feature
自动合并 config.ini
冲突（内容）：合并冲突于 config.ini
自动合并失败，修正冲突然后提交修正的结果。
```

`自动合并 config.ini` 和 `冲突（内容）` **两句同时出现**，别以为矛盾：
git 是一行一段地判定的 —— 同一个文件里，能自动定的段已经定了（`端口 = 9090`、`超时 = 30`、`最大连接 = 100`），
只有「都改了且不同」的那一段留给你。

**`git status` 会直接告诉你三条出路**

```text
$ git status
位于分支 main
您有尚未合并的路径。
  （解决冲突并运行 "git commit"）
  （使用 "git merge --abort" 中止合并）

未合并的路径：
  （使用 "git add <文件>..." 标记解决方案）
	双方修改：   config.ini

修改尚未加入提交（使用 "git add" 和/或 "git commit -a"）
```

逐行读：
- `您有尚未合并的路径` = 仓库处于「合并进行中」状态（这个状态是磁盘上的文件，见场景 ⑩）。
- `（使用 "git merge --abort" 中止合并）` = **退路一直都在**，git 主动写给你了。
- `双方修改：config.ini` = 冲突类型是 both modified（英文 `both modified`），短格式记号是 `UU`。
- 那句「使用 git add 标记解决方案」就是场景 ⑨ 的核心：**add = 标记已解决**，不是暂存。

**冲突文件本体，逐行读**

```text
$ cat config.ini
端口 = 9090
超时 = 30
最大连接 = 100
<<<<<<< HEAD
线程 = 4
=======
线程 = 8
>>>>>>> feature
```

```
端口 = 9090          ← 标记之外的行：git 已经替你定好了（这里取了你这侧，因为只有你改了端口）
超时 = 30            ← 两边一样
最大连接 = 100        ← 只有对方加了，直接保留
<<<<<<< HEAD          ← ★ 冲突段开始。这一行到 ======= 之间 = :2: ours = 你脚下分支（main）的内容
线程 = 4
=======               ← 分界线
线程 = 8               ← ★ 这一行到 >>>>>>> 之间 = :3: theirs = 被合进来的 feature 的内容
>>>>>>> feature       ← ★ 冲突段结束。>>> 后面写的是「对方是谁」：这里是分支名 feature
```

三条必须记死的读法：

1. **`<<<<<<<` 到 `=======` 是「你的」，`=======` 到 `>>>>>>>` 是「对方的」**，
   判断依据是**你此刻站在哪条分支上**（这里 HEAD 在 main，所以上半是 main 的内容）。
2. 这些标记行**就是文件内容的一部分**（纯文本）。git 不会替你删，也不会替你选。
   你的工作 = 把不想留的那半**连同三行标记一起删掉**，或者改成两边合并后的新内容。
3. 如果标记忘了删就提交，git **不会拦你** —— 它会老老实实把 `<<<<<<< HEAD` 提交进历史。
   这也是为什么很多项目会在 CI 里加一条 `grep -rn '^<<<<<<< ' .` 的检查。

**三个版本分别从暂存区里捞出来对比**

```text
$ git ls-files -u
100644 8050857d8e841257c3704f8155fcf188f44bfc07 1	config.ini
100644 baeaeb474ddbed91d578c5675750293c958fb200 2	config.ini
100644 af26ba9d7d5630fc5cdbadabb39c34441ce77b5a 3	config.ini

$ echo "== :1: 共同祖先 =="; git cat-file -p ":1:config.ini"
== :1: 共同祖先 ==
端口 = 3000
超时 = 30

$ echo "== :2: ours（HEAD/我们自己） =="; git cat-file -p ":2:config.ini"
== :2: ours（HEAD/我们自己） ==
端口 = 9090
超时 = 30
最大连接 = 100
线程 = 4

$ echo "== :3: theirs（feature 那一侧） =="; git cat-file -p ":3:config.ini"
== :3: theirs（feature 那一侧） ==
端口 = 3000
超时 = 30
最大连接 = 100
线程 = 8
```

对一下场景 ⑦ 的判定规则，这次冲突为什么发生就一目了然：
`:1:` 只有两行（`端口 = 3000 / 超时 = 30`），`:2:` 和 `:3:` **各自都加了 `最大连接`/`线程` 两行，且 `线程` 的值不同** →
`base=A, ours=B, theirs=C` → 这一段没有唯一解 → 交给你。

> ⚠ 引号别丢：`git cat-file -p ":2:config.ini"` 里的引号是防 shell 解析 `:`。

**让标记里直接显示共同祖先：`zdiff3`（推荐）**

默认的 `<<<<<<<` 两段式**看不到 base**（base 只能靠 `:1:` 单独捞）。
git 有个专门解决这个痛点的开关：`git config merge.conflictStyle zdiff3`（写进 `.git/config`，只影响本仓库），
或只想这一次生效就 `git -c merge.conflictStyle=zdiff3 merge feature`。
demo J 段（沙箱 `j-zdiff3/`，和 `b-conflict/` 同一份现场）两种都演了 —— 先确认没配、再用 `-c` 跑一次、跑完再确认还是没配：

```text
$ git config --get merge.conflictStyle || echo "（没配，默认两段式）"
（没配，默认两段式）

$ git -c merge.conflictStyle=zdiff3 merge feature
自动合并 config.ini
冲突（内容）：合并冲突于 config.ini
自动合并失败，修正冲突然后提交修正的结果。

$ cat config.ini
端口 = 9090
超时 = 30
最大连接 = 100
<<<<<<< HEAD
线程 = 4
||||||| ee12acb
=======
线程 = 8
>>>>>>> feature

$ git config --get merge.conflictStyle || echo "（跑完仍然是没配：-c 只活在这一条命令里）"
（跑完仍然是没配：-c 只活在这一条命令里）
```

这次 `|||||||` 和 `=======` 之间**是空的** —— 空得有含义：**共同祖先那里根本没有这一段**，
是两边各自新增了内容（`最大连接`/`线程` 在 base 里都不存在，`:1:` 那份上面刚捞出来过，只有两行）。
于是你立刻知道「这不是谁改坏了谁，是两边都在往下加」，判断依据完全不一样。
（如果 base 里有原值，`|||||||` 下面就会照印那一行，你一眼看出「两边把同一行改成了两个值」。）

### ⑨ 解决冲突四步走，以及「为什么必须再 `git add`」

**什么情况**
标记看懂了，接下来要收尾。最常见的两种死法：改完文件直接 `git commit`（被拒），
或者用 `--ours` 想「保留这一行」结果整份文件被换掉。

**第 0 步：先决定「要不要逐行挑」**

```text
$ git checkout --ours config.ini
从索引区更新了 1 个路径

$ echo "--- 取 ours 之后 ---"; cat config.ini
--- 取 ours 之后 ---
端口 = 9090
超时 = 30
最大连接 = 100
线程 = 4

$ git checkout --theirs config.ini
从索引区更新了 1 个路径

$ echo "--- 取 theirs 之后 ---"; cat config.ini
--- 取 theirs 之后 ---
端口 = 3000
超时 = 30
最大连接 = 100
线程 = 8
```

★ 看清楚这两份内容：`--ours` 拿到的是 **`:2:` 那整份文件**，`--theirs` 拿到的是 **`:3:` 那整份文件**。
**它们不是「这一行选哪边」，而是「这个文件整份用哪一边」。**
取 theirs 之后连 `端口` 都变回 3000 了 —— 你只想解决 `线程` 那一行，结果把主线的端口改动也一起撤了。
要逐行挑，就只能自己编辑文件，把标记行删干净。

**第 1 步：手工把文件改成你决定的样子**

```text
$ cat config.ini
端口 = 9090
超时 = 30
最大连接 = 100
线程 = 8
```

端口取主线的 9090、线程取功能分支的 8 —— 这就是这次合并的**业务决定**，git 无权替你决定。
（上面的命令是 `cat`，实际动作是你用编辑器改；关键是**三行标记必须都删掉**。）

**第 2～4 步：`git add` → `git commit` → 看形状**

```text
$ git status --short
UU config.ini

$ git add config.ini

$ git status --short
M  config.ini

$ git commit -m "合并 feature：端口取 9090，线程取 8"
[main f801636] 合并 feature：端口取 9090，线程取 8

$ git log --oneline --graph --decorate
*   f801636 (HEAD -> main) 合并 feature：端口取 9090，线程取 8
|\  
| * 7318c7e (feature) feature: 线程 = 8
* | c4dd4d5 main: 线程 = 4，顺手把端口也改了
|/  
* 8c43cb7 c1 初始配置

$ git log -1 --format='合并提交 %h：%s  父亲有 %p'
合并提交 f801636：合并 feature：端口取 9090，线程取 8  父亲有 c4dd4d5 7318c7e
```

`UU` → `M ` 就是「已解决」的机器证据。为什么这一步非做不可？

```
git add config.ini   在冲突现场做的事 = 删掉暂存区里 config.ini 的 stage 1/2/3 三行，
                                       换成一条 stage 0 的新行（内容 = 你手上这份文件）
```

add 之前，仓库的账本上写着「这个文件有三个版本、没人决定用哪个」，
这种状态**不可能**被提交成历史，所以 `git commit` 会直接拒你
（`git status` 那句 `您有尚未合并的路径` + 短格式的 `UU` 就是同一个意思）。
**所以：`add` 在这里不是「暂存改动」，是「报告：这个冲突我处理过了」。**
（01 章讲的「add = 把内容复制成 blob 并写进清单」在这里依然成立，只是多了一层语义：清掉那三个 stage。）

### ⑩ 合到一半想放弃：`--abort` 与 `--quit` 差一个词，后果完全不同

**什么情况**
冲突太多不想处理了；或者你在别人的机器/CI 上跑错了合并，想整个退回。

**`git merge --abort`：整个退回**（demo `c-abort/`）

```text
$ git merge feature
自动合并 line.txt
冲突（内容）：合并冲突于 line.txt
自动合并失败，修正冲突然后提交修正的结果。

$ git status --short
UU line.txt

$ git log --oneline -1
66e39ae main 也改第8行

$ git merge --abort

$ git status --short
$ git log --oneline -1
66e39ae main 也改第8行

$ cat line.txt
共同的起点
第2行
第3行
第4行
第5行
第6行
第7行
main 把第8行改成别的了
```

`--abort` 之后：`git status --short` **空**（无 UU、无未跟踪）、log 顶端还是 `66e39ae`、
文件里一个标记行都没有。这就是它作为「退路」的全部含义。

**先搞清「合并进行中」在磁盘上是什么**（demo I1 段，沙箱 `i-merge-state/`，刚撞完冲突那一刻）

```text
$ ls .git
AUTO_MERGE
COMMIT_EDITMSG
HEAD
MERGE_HEAD
MERGE_MODE
MERGE_MSG
ORIG_HEAD
config
description
hooks
index
info
logs
objects
refs

$ cat .git/MERGE_HEAD
5d8c8490bada7ae9290f31d16a3737689bf7106d

$ echo "MERGE_HEAD = $(git rev-parse --short MERGE_HEAD)   feature = $(git rev-parse --short feature)   HEAD = $(git rev-parse --short HEAD)"
MERGE_HEAD = 5d8c849   feature = 5d8c849   HEAD = 6951d6a

$ cat .git/MERGE_MSG
Merge branch 'feature'

# Conflicts:
#	config.ini
```

- `MERGE_HEAD` = 「对方是谁」，内容就是 `feature` 的哈希（和 `git rev-parse feature` 完全一致）。
- `MERGE_MSG` = 「你提交时默认给你用的说明」，`# Conflicts:` 那两行是注释，列出哪些文件撞了。
- `MERGE_MODE` = 记 `--no-ff` 这类开关；`AUTO_MERGE` = ort 写的工作区结果。
  有这几个文件 = 你在合并中途；它们没了 = 没在合并中。**git 没有「记忆」，状态全靠这几个文件。**

**`git merge --quit`：只删小抄，别的都不还原** ★（还是那个 `i-merge-state/`，demo I2）

```text
$ git merge feature
自动合并 config.ini
冲突（内容）：合并冲突于 config.ini
自动合并失败，修正冲突然后提交修正的结果。

$ git merge --quit

$ ls .git | grep -i merge || echo "（MERGE_* 文件已删除）"
（MERGE_* 文件已删除）

$ git status --short
UU config.ini

$ git ls-files -u
100644 8050857d8e841257c3704f8155fcf188f44bfc07 1	config.ini
100644 baeaeb474ddbed91d578c5675750293c958fb200 2	config.ini
100644 af26ba9d7d5630fc5cdbadabb39c34441ce77b5a 3	config.ini

$ git merge --abort
致命错误：没有要中止的合并（MERGE_HEAD 丢失）。
```

对照 `--abort`：`--quit` **只删掉 `.git/MERGE_*`**，工作区的标记行、暂存区的三个 stage 全都留在原地。
于是你既退不回（`--abort` 已经不肯了：`没有要中止的合并（MERGE_HEAD 丢失）`），
也没法正常提交（`UU` 还在）。自救：

```text
$ git reset
重置后取消暂存的变更：
M	config.ini

$ git ls-files -u

$ git status --short
 M config.ini

$ cat config.ini
端口 = 9090
超时 = 30
最大连接 = 100
<<<<<<< HEAD
线程 = 4
=======
线程 = 8
>>>>>>> feature

$ git checkout -- config.ini
$ git status --short
```

`git reset`（默认 mixed，03 章讲过）把暂存区砸回 HEAD，三个 stage 消失（上面 `git ls-files -u` 输出为空）、
`UU` 变成 ` M`；但**工作区那份带标记的文件还在**（上面 `cat` 的最后四行），
得手工把标记行删掉，或者干脆 `git checkout -- config.ini` 丢弃改动（demo 选的是后者，所以最后一条
`git status --short` 输出为空 —— 干净了，但你**什么都没决定**，等于这次合并白撞了一回）。
> 结论：**想放弃就用 `--abort`。`--quit` 是给「我知道我在干什么」的人准备的逃生口**
> （比如你想在合并状态外单独处理某个文件），不是温和版的 abort。

### ⑪ ★★★ 本章最著名的坑：`--ours` / `--theirs` 在 merge 和 rebase 里是反的

**什么情况**
你在 `feature` 上，同一份冲突文件、同一句 `git checkout --ours shared.txt`：
`merge` 时取到的是自己的内容，`rebase` 时取到的却是 **main 的内容**。不知道这一点，
你在 rebase 里想「保留自己的改动」，结果把自己的代码全删了。

demo 的 D 段把**同一份冲突**在两种情境下各造了一次（`shared.txt` 第 2 行：main 侧写 `MAIN`、
feature 侧写 `FEATURE`，人站在 `feature` 上），`git ls-files -u` 的输出是最硬的证据：

**merge 情境**（站在 `feature` 上 `git merge main`）：

```text
$ git ls-files -u
100644 9e68baf59d581b428b46030eef295cb7220c05f8 1	shared.txt
100644 061bc349bf952ba98dc06e9bb6dd63d6dd509b68 2	shared.txt
100644 396626f392b19f6ca320a51cd69174de3288d798 3	shared.txt
```

**rebase 情境**（同一个现场，改成 `git rebase main`）：

```text
$ git ls-files -u
100644 9e68baf59d581b428b46030eef295cb7220c05f8 1	shared.txt
100644 396626f392b19f6ca320a51cd69174de3288d798 2	shared.txt
100644 061bc349bf952ba98dc06e9bb6dd63d6dd509b68 3	shared.txt
```

`:1:`（共同祖先 `9e68baf5…`）两边一模一样，但 `:2:`/`:3:` **整整齐齐对调了**。原因：
`ours/theirs` 的本义不是「我 vs 你」，而是 **stage 号** ——
`:2:`（ours）= **当前正在被写入的那个底本**，`:3:`（theirs）= **这一次被叠进来的那坨改动**。
`merge` 时底本是你脚下的分支；`rebase` 时 git 先把 HEAD 挪到**对方**（main）上，再把你的提交一条条重放上去，
底本换人了 → **名字没换、含义换了**。

> 口诀（背这个，别背直觉）：
> **merge：ours = 你当前所在分支；theirs = 被你合并进来的那个分支。**
> **rebase：ours = 你要落到的那个底本（通常是 main）；theirs = 你自己那条被重放的分支。**

两条辨认线索（都在 demo 的真实输出里）：

1. rebase 的标记尾巴写的是**提交**（`>>>>>>> bfe39f0 (feature: 写 FEATURE)`），merge 写的是**分支名**（`>>>>>>> main`）。
   看到哈希形式就知道自己在 rebase 里，此刻 `--ours` 十有八九不是你想要的。
2. 别信记性，现场验一次：`git ls-files -u <文件>` 看 `:2:`/`:3:` 的哈希 →
   `git cat-file -p ":2:<文件>"` 读出内容 → 和 `git rev-parse --short HEAD <对方分支>` 对一下，谁对上谁才是 ours。

（rebase 的那一屏冲突提示是**英文** —— `Resolve all conflicts manually...`，git 还没翻译这几句，别找「为什么两句不一样」。）

### ⑫ 冲突的其他形状：改名、删除、两边各加一份

**A. 对方改了文件名、你改了内容（默认：git 没认出改名）**（demo E1 段，沙箱 `e-rename/`）

```text
$ git merge mvline
冲突（修改/删除）：core.txt 在 mvline 中被删除，在 HEAD 中被修改。core.txt 的 HEAD 版本在树中被保留。
自动合并失败，修正冲突然后提交修正的结果。

$ git status --short
UD core.txt
A  heart.txt

$ cat heart.txt
第1行
第2行
第3行：改名之后这一行也被动过
第4行
第5行

$ grep -c "<<<<<<<" heart.txt
0
```

读法：`UD` = modified/**deleted**（你改了、对方把它删了），`A  heart.txt` = 新名字被当成**新增文件**。
`heart.txt` 里**一个冲突标记都没有**（`grep -c` 输出 0）—— 因为 git 这次**没把 `core.txt` 和 `heart.txt` 认成同一个文件**，
它看到的只是「你改了 core.txt、对方删了 core.txt、另外凭空多了个 heart.txt」。

**B. 为什么认不出：改名相似度是按字节现算的，小文件一改就掉到阈值以下**

`git mv` 在仓库里**就是**「删一个 + 加一个」，改名不是元数据 —— 这一点 `git show --stat` 直接摊给你看
（同一个文件写成两行，一删一加）：

```text
$ git show mvline~1:core.txt | wc -c
64

$ git show --stat mvline | tail -3
 core.txt  | 5 -----
 heart.txt | 5 +++++
 2 files changed, 5 insertions(+), 5 deletions(-)
```

「这是同一个文件」是 git 每次**现算**的相似度。这份文件总共只有 **64 字节**，改第 3 行中文就动了 30 多字节，
相似度掉到默认阈值 **50%** 以下。demo E1b 把阈值当变量拨了一遍：

```text
$ git diff -M --find-renames=50% --stat mvline~1 mvline
 core.txt  | 5 -----
 heart.txt | 5 +++++
 2 files changed, 5 insertions(+), 5 deletions(-)

$ git diff -M --find-renames=40% --stat mvline~1 mvline
 core.txt => heart.txt | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)
```

50% 认不出（还是 D + A 两行），40% 就认出来了（变成一行 `core.txt => heart.txt`）。
把阈值降到合并那边（`git merge -X rename-threshold=20 mvline`），**同一份现场立刻换一种冲突**：

```text
$ git merge -X rename-threshold=20 mvline
自动合并 heart.txt
冲突（内容）：合并冲突于 heart.txt
自动合并失败，修正冲突然后提交修正的结果。

$ git status --short
D  core.txt
UU heart.txt

$ cat heart.txt
第1行
第2行
<<<<<<< HEAD:core.txt
第3行：主线改成了另一句
=======
第3行：改名之后这一行也被动过
>>>>>>> mvline:heart.txt
第4行
第5行
```

标记两端这次带上了文件名：`<<<<<<< HEAD:core.txt` … `>>>>>>> mvline:heart.txt`
（看到 `分支名:文件名` 就说明这段涉及改名，`:` 后面是那条线上这个文件的旧名字）。
**改一个阈值就得到两种完全不同的冲突**（修改/删除 vs 内容冲突），要做的决定也不同：
前者是「留哪个文件名」，后者只是「留哪句话」。

**C. 其余几种形状，认字母就够了**（`git status --short` 的第一格 = 暂存区、第二格 = 工作区，U = Unmerged）

| 记号 | 中文说明 | demo 里的现场 | 你要决定的事 |
|---|---|---|---|
| `UD` + `A` | 修改/删除 | E1 段（没认出改名）、E3 段 `UD legacy.txt` | 留文件（`git add`）还是删（`git rm`）—— **git 不给默认值** |
| `AU` / `UA` / `DD` | 重命名/重命名 | E2 段：`old.txt` 被两边改成 `core.txt` 和 `heart.txt` | 两个新名字都留在工作区，选一个留、`git rm` 掉另一个（`DD` 那条也得报个决定） |
| `UU` | 双方修改 | 场景 ⑧（`b-conflict/`）、E1b 段（降阈值之后） | 逐行挑，或整份取一边 |
| `AA` | 添加/添加 | E4 段 `conflict-demo.txt`（`冲突（添加/添加）`，英文 `added in both`） | 同上，但暂存区里**没有 :1:** |

add/add 的铁证就在 `git ls-files -u` 里 —— 只有 2、3 两个阶段号，因为共同祖先里根本没这个文件：

```text
$ git merge tooltest
自动合并 conflict-demo.txt
冲突（添加/添加）：合并冲突于 conflict-demo.txt
自动合并失败，修正冲突然后提交修正的结果。

$ git ls-files -u
100644 857254abf297baeb1e34b76597210e6e2d6a0878 2	conflict-demo.txt
100644 c34c91a2664ca2b875913d037a962c1a1fd6895e 3	conflict-demo.txt

$ git status --short
AA conflict-demo.txt
```

**D. `git mergetool` 的本质：把一个外部命令插在冲突文件中间**

沙箱里没装图形工具，`git mergetool --tool-help` 只能报名单（34 个工具名，全部标着
`requires a graphical session`，这段 git 没翻译，是英文；没配 `merge.tool` 之前它根本不肯干活）。
demo 的 E4 段配了一个**假的**工具（`mergetool.mytool.cmd` 指向 `$SB/fake-merge.sh $MERGED`，
脚本内容就是把标记行 `grep -v` 掉），跑出来的结果很说明问题：

```text
$ git mergetool
Merging:
conflict-demo.txt

Normal merge conflict for 'conflict-demo.txt':
  {local}: created file
  {remote}: created file
[假工具] 我收到的冲突文件是：conflict-demo.txt
[假工具] 标记行已删掉，退出码 0

$ git status --short
M  conflict-demo.txt
?? conflict-demo.txt.orig
```

- 所谓 mergetool，**只是把冲突文件交给一个外部命令去改写**：`$MERGED` 是那个冲突文件，
  `$BASE`/`$LOCAL`/`$REMOTE` 分别是 `:1:`/`:2:`/`:3:` 落成的临时文件。meld、vimdiff、VS Code 干的事和这条 `grep` 一样。
- 工具退出码 0（+ `mergetool.mytool.trustExitCode true`）→ **git 就替你把这个文件 add 了**（上面那个 `M `）。
  可这次结果是错的：`甲`、`乙` 两行都留下了。**退出码不代表你解决对了。**
- `conflict-demo.txt.orig` 是 git 留的原始副本（不想要就 `git config mergetool.keepBackup false`）。
- mergetool **从不替你提交**：跑完 `git status` 仍然是「所有冲突已解决但您仍处于合并中」，还得自己 `git commit`。

### ⑬ 只看合并：`--merges` 与 `--first-parent`

**什么情况**
你只想知道「主线最近合进来了哪些东西」，不想看功能分支里那几十条碎提交。

```text
$ git log --oneline --merges
dd260d1 Merge branch 'feature'

$ git log --oneline --first-parent
dd260d1 Merge branch 'feature'
87ef4f5 c3 主线改了第2行
dea2ff4 c1 共同的起点
```

`--merges` = 只看双亲提交；`--first-parent` = **只沿着第 1 个父亲往回走**（`HEAD^1` 那条线，也就是主线），
合进来的整条支线被压成一个点。这就是 `--no-ff` 菱形（场景 ⑤）换来的收益：
主线历史一眼读完，功能内部的 12 条提交想细看再 `git log dd260d1^2` 进菱形里。

### ⑭ 合并合坏了：`ORIG_HEAD` / `reflog` / `revert -m 1`

**什么情况**
手滑 `git merge bad`，把不该上线的功能合进了主线，而且**合并提交已经生成了**
（demo 的 F 段专门演这一出：`git merge bad -m "手滑合并了 bad 分支"` → `Merge made by the 'ort' strategy.`，
`c.txt` 立刻出现在工作区）。

git 在合并前**偷偷记下了你原来的位置**，所以本地（没 push）最快的补救是两条：

```text
$ cat .git/ORIG_HEAD
a2f9a9bae9047bb9c5ef91e6acc721cf0e7a1378

$ git reset --hard ORIG_HEAD
HEAD 现在位于 a2f9a9b c4 main 也动了（让合并快进不了）

$ git reflog | head -5
a2f9a9b HEAD@{0}: reset: moving to ORIG_HEAD
30c80e7 HEAD@{1}: merge bad: Merge made by the 'ort' strategy.
a2f9a9b HEAD@{2}: commit: c4 main 也动了（让合并快进不了）
24dbe1f HEAD@{3}: checkout: moving from bad to main
6419c4b HEAD@{4}: commit: c3 不该合的功能
```

- 合并提交没了、`c.txt` 也没了。★ 再提醒一次：`reset --hard` 会连你**未提交的改动**一起砸掉（03 章 ⑤），
  敲之前先看 `git status`。
- **合并本身也会进 reflog**（`merge bad: Merge made by the 'ort' strategy.`），
  `HEAD@{2}` 就是合并之前的位置 —— 忘了 `ORIG_HEAD` 也能查回来。**出事先 reflog，别猜。**
- **这条合并如果已经 push 出去了，就不要 reset**，改用 03 章的做法：`git revert -m 1 <那条合并提交的 hash>`。
  `-m 1` = 保留第 1 个父亲（你原本的主线 `HEAD^1`），把第 2 个父亲带进来的东西整体撤掉
  （`revert -m` 的报错和实测见 03 章 ⑦-b）。这也回收了场景 ⑤ 的动机：**菱形留住了，一次 revert 才撤得掉一整个功能。**

---

## 命令速查表

```bash
# ── 看清局面（永远先做这一步）──────────────────────────────
git branch -vv                          # 上游 + 领先/落后
git branch --merged / --no-merged       # 相对 HEAD 谁已经合了 / 谁没合
git log --oneline --graph --decorate --all   # ★ 本章地基，--all 别省
git log --oneline HEAD..feature         # 对方有、我没有
git log --oneline feature..HEAD         # 我有、对方没有
git log --oneline --left-right HEAD...feature   # 两边一起看（< 我 / > 对方）
git diff --stat HEAD...feature          # 对方相对共同祖先动了哪些文件
git merge-base HEAD feature             # 共同祖先是哪个提交
git merge-base --is-ancestor A B; echo $?   # 0=A 是 B 的祖先（能快进）；1=不能
git cherry -v HEAD feature              # 按补丁内容比，+ 是新的、- 是已有等价改动

# ── 分支日常 ──────────────────────────────────────────────
git branch -m 旧名 新名                 # 改名
git branch -u origin/feature x          # 挂上游（挂错很坑）
git branch -d topic                     # 安全删（相对 HEAD 或上游已合并才放行）
git branch -D topic                     # 强制删；输出里「原为 <hash>」= 后悔药
git branch 名字 <hash>                  # 用那个 hash 原地复活分支
git show 分支名:文件名                   # 不切分支就读那条分支上的文件

# ── 合并 ─────────────────────────────────────────────────
git merge feature                       # 默认：能快进就快进
git merge --no-ff feature -m "..."      # 强行留菱形，整个功能可一次 revert
git merge --ff-only feature             # 只许快进，否则中止（pull 同理）
git merge --no-commit --no-ff feature   # 试合，停在提交前
git merge -X rename-threshold=20 feature  # 降低改名识别阈值（默认 50%）
git -c merge.conflictStyle=zdiff3 merge feature   # 一次性：冲突里显示共同祖先
git log --oneline --merges              # 只看合并提交
git log --oneline --first-parent        # 只看主线
git rev-parse --short HEAD^1 HEAD^2     # 合并提交的两个父亲

# ── 冲突现场 ─────────────────────────────────────────────
git status                              # 未合并路径 + 三条出路
git status --short                      # UU / UD / AU / UA / DD
git ls-files -u                         # ★ :1: base  :2: ours  :3: theirs
git cat-file -p ":2:文件名"              # 直接读三个版本之一
git diff                                # 组合差异（diff --cc，两列前缀，最难读）
git checkout --ours <文件>              # 整份文件取 ours（不是逐行！）
git checkout --theirs <文件>            # 整份文件取 theirs
git checkout -m -- <文件>               # 标记被改乱了？按 :1:/:2:/:3: 重新生成
git add <文件>                          # ★ 「这个冲突我处理过了」
git commit                              # 合并收尾（默认说明来自 .git/MERGE_MSG）
git mergetool                           # 把冲突文件交给外部命令改写

# ── 救火 ─────────────────────────────────────────────────
git merge --abort                       # 退回合并前，MERGE_* 一起删（★ 用这个）
git merge --quit                        # 只删 .git/MERGE_*，文件和暂存区不还原（⚠ 不是 abort）
git cat .git/ORIG_HEAD                  # git 在合并前记下的位置
git reset --hard ORIG_HEAD              # 合并合坏了且没 push（★ 会砸掉未提交改动）
git reflog | head -5                    # merge 也会进 reflog
git revert -m 1 <合并提交 hash>          # 已经 push 出去的合并
```

---

## 这章的坑

1. **`git log --graph` 不加 `--all` 就看不到另一条线。** 新手 90% 的「我的提交怎么没了」是这个。
   固定用 `git log --oneline --graph --decorate --all`。
2. **快进合并什么记录都不留。** `更新 a88c7bb..8017b77` + `Fast-forward` 之后，
   历史是一条直线（实测：对象数 9 → 9，只有 `.git/refs/heads/main` 里那行字符串变了）。
   事后你**看不出这里合过一个功能**，也没法一次撤掉整个功能 —— 要用 `--no-ff`。
3. **`git branch -d` 不是「绝对安全」。** 它的判断是「相对 **HEAD** 或相对 **这条分支的上游** 已合并」。
   本章实测：`feature/ghost` 没并进 main，但因为已经推到 `origin/feature/ghost`，`-d` 只警告一句就删了。
   → 删之前 `git log --oneline main..<分支>` 数一下还有几个提交没进去；
   → 删完那句「已删除分支 …（原为 41414c6）」里的 hash **一定要抄下来**，那是唯一的坐标。
4. **冲突状态下 `--ours` / `--theirs` 是「整份文件选一边」**，不是「这一行选一边」。
   实测：`git checkout --theirs config.ini` 之后连 `端口` 都从 9090 变回 3000 了。
   逐行挑就老实编辑文件。
5. **★ ours/theirs 在 rebase 里含义互换。** 同一个冲突文件，`:2:`/`:3:` 的哈希整整齐地对调
   （merge 时 `:2:`=`061bc349`(FEATURE)，rebase 时 `:2:`=`396626f3`(MAIN)）。
   在 rebase 里想「保住自己的改动」，取 `--theirs`；分不清就 `git ls-files -u` + `git cat-file -p ":2:文件"` 亲眼验一次。
6. **改完冲突文件不 `git add` 就 `git commit`，必然被拒。** `UU` 的第一格 U = Unmerged，
   只要还有一个带 U，git 就不提交。`add` 在这里的语义是「清掉暂存区里那三个 stage」，不是「暂存改动」。
7. **`git merge --quit` 不是温和版 abort。** 实测之后 `git merge --abort` 直接报
   `致命错误：没有要中止的合并（MERGE_HEAD 丢失）。`，而工作区的标记行、暂存区的三个 stage 都还在原地。
   想退回请只用 `--abort`。
8. **标记行忘了删就提交，git 不拦你。** 它会老老实实把 `<<<<<<< HEAD` 提进历史。
   提交前 `grep -rn '^<<<<<<< ' .`，或者在 CI 里加这一条。
9. **改名冲突的形状取决于相似度阈值，很脆弱。** 本章实测：64 字节的文件改一行中文，
   默认 50% 阈值认不出改名 → 得到「修改/删除」冲突；`-X rename-threshold=20` → 得到「内容」冲突。
   同一个业务问题，两次要做的决定完全不同。**先看 `git status --short` 的字母（UD / UU / AU / UA / DD）再动手。**
10. **上游（`-vv` 方括号里那个）挂错，pull/push 就会拉/推错分支。**
    实测挂成 `[origin/main: 领先 2，落后 1]` 的那条功能分支，随手 `git pull` 会把 main 合进来。
11. **别信 mergetool 的退出码。** 本章用一个只删标记行的假工具做实验：git 见退出码 0 就直接把文件 add 了，
    而文件里 `甲`、`乙` 两行都留下了 —— **这是一个错误结果，却被当成已解决。**
    工具只负责改写文件，对不对还是你的事；mergetool 也从不替你 commit。

---

## 自检问题

**Q1：`git merge` 之后历史是一条直线，`git log --graph` 里看不到任何「合并」的痕迹。可能吗？怎么做到的？**
**答**：可能，那是 fast-forward —— 你一步没动，对方在你正前方，git 只把分支文件里的哈希往前挪（实测对象数不变：9 → 9）。
要让合并留痕就用 `git merge --no-ff`，它强行造一个双亲提交，`--graph` 里出现菱形。

**Q2：怎么在不合并的前提下，判断「对方有、我没有」的提交和文件各是哪些？两条命令。**
**答**：`git log --oneline HEAD..feature`（提交列表）和 `git diff --stat HEAD...feature`（文件列表，**三点** = 相对共同祖先）。
两点版 `HEAD..feature` 会把**你自己的**改动也算进去，看起来像对方「删掉」了你加的东西（本章实测：三点只有 `login.js`，两点多出 `plan.md | 1 -`）。

**Q3：冲突文件里 `<<<<<<< HEAD` 到 `=======` 这一段，是「我改的」还是「对方改的」？依据是什么？**
**答**：是**你此刻脚下这条分支**（HEAD 那一侧，`:2:` ours）的内容；`=======` 到 `>>>>>>>` 是被合进来的那边（`:3:` theirs）。
依据是**你站在哪条分支上**，所以 rebase 时（HEAD 被挪到 main 上再重放你的提交）两段会互换。
想不靠记性就用 `git ls-files -u` 看 `:2:`/`:3:` 的哈希，再 `git cat-file -p ":2:文件"` 对一下。

**Q4：为什么冲突解决完必须 `git add`，不能直接 `git commit`？**
**答**：冲突时暂存区里那个文件有三行记录（stage 1/2/3，即 base/ours/theirs），`git commit` 拒绝提交「三个版本没人决定」的状态。
`git add` 的动作就是**删掉那三行、写回一条 stage 0 记录**（内容 = 你手上这份文件），也就是报告「我处理过了」。
标志位从 `UU` 变成 `M ` 就是成功了。

**Q5：合并到一半不想合了。`--abort` 和 `--quit` 有什么区别？各留下什么？**
**答**：`--abort` = 完全退回合并前：`.git/MERGE_*` 删掉、暂存区还原、工作区文件里的标记也没了（`git status --short` 空）。
`--quit` = **只删 `.git/MERGE_*` 那几个文件**，暂存区里的 1/2/3 三个 stage 和工作区带标记的文件都原样留着，
之后再敲 `--abort` 会报 `致命错误：没有要中止的合并（MERGE_HEAD 丢失）`。所以要放弃就用 `--abort`。

**Q6：同事的分支既改了文件名、又改了内容，而你改了同一个文件的同一行。会出现几种可能的冲突形状？怎么分辨？**
**答**：两种。git **认出改名** → 内容冲突，标记落在新文件名上（`UU heart.txt`，标记写 `<<<<<<< HEAD:core.txt` / `>>>>>>> mvline:heart.txt`）；
git **没认出改名**（相似度低于阈值，默认 50%）→ 修改/删除冲突（`UD core.txt` + `A heart.txt`，新文件里**没有标记**）。
分辨只看 `git status --short` 的字母；要换形状可以 `git merge -X rename-threshold=20`。

**Q7：你已经把不该合的分支合进 main 并提交了，但**还没 push**。接下来两条命令的顺序是什么？为什么不能直接 `revert -m 1`？**
**答**：`cat .git/ORIG_HEAD`（或 `git reflog`）确认合并前的位置 → `git reset --hard ORIG_HEAD`
（★ 先看 `git status`，它会把未提交的改动一起砸掉）。
还没共享的历史应该**缩短**而不是「往后加一条」，直接 `revert -m 1` 会在历史里留下「合了又撤」两条记录，
而且别人 pull 到你这条 revert 时看得一头雾水。**已经 push 出去才用 `revert -m 1`。**

---

## 运行 demo

```bash
cd git-handbook/04-branch-merge
bash demo.sh          # 全跑一遍（A→J 十段），跑完自动删沙箱；输出 1067 行，退出码 0
bash demo.sh --keep   # 跑完保留沙箱并打印路径，可以进去自己敲命令
```

脚本约定（和 01、03 章一致）：全部在 `mktemp -d` 的沙箱里做写操作，**绝不碰你机器上的任何真实仓库**；
不联网，需要「远程」时在沙箱里 `git init --bare` 冒充 `origin`；
提交身份写进**沙箱仓库的本地配置**（`git config user.name "学习者"`，不带 `--global`）；
`GIT_AUTHOR_DATE`/`GIT_COMMITTER_DATE` 从 `2026-01-01T10:01:00 +0800` 起逐条递增，所以哈希可复现；
`GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null` 已经在脚本里设好，
你机器上的 `~/.gitconfig`（`pull.rebase`、`init.defaultBranch`、`advice.*`、别名）不会混进这份输出。
`set -euo pipefail` 让任何一步真失败就停下；报错步骤用 `sh_fail` 打印退出码，是**故意给你看错误的**。
还有一处要知道：

- 沙箱目录后缀每次随机（`/tmp/git-handbook-04-branch-merge-XXXXXXXX`），本文两处出现这个路径的地方
  （开头「沙箱位置」和 E4 假工具路径）你自己的会不同，其余逐字节一致（已连跑两次核对）。

`--keep` 之后推荐自己进沙箱敲这几条，把本章再走一遍：

```bash
cd "$SB/a-branches"      && git branch -vv && git log --oneline --graph --decorate --all
cd "$SB/g-checklist"     && git log --oneline HEAD..feature/login   # 本章的检查清单
cd "$SB/b-conflict"      && git status && git ls-files -u
cd "$SB/i-merge-state"   && ls .git | grep -i merge   # 看还在合并中吗
cd "$SB/swap"            && git cat-file -p ":2:shared.txt"   # 亲眼确认 ours 是谁
cd "$SB/j-zdiff3"        && cat config.ini                    # 三方标记长什么样
```

**怎么核对本文每一行都是真跑出来的**：把 demo 的输出留成一个文件，再逐行对。

```bash
cd git-handbook/04-branch-merge
bash demo.sh > /tmp/git04.txt 2>&1      # 本文所有 ```text 围栏的原料就是这一份
```

本文代码块里凡是带 `$` 提示符的行，都必须能在 `/tmp/git04.txt` 里逐字找到
（只有沙箱后缀和每次重跑会变的提交哈希是例外）。找不到就是写错了。

---

## 下一步

- `05-rebase-history`：把「两条线合成一个菱形」换成「把自己的提交搬到别人后面」，历史保持直线。
  本章场景 ⑪ 已经预告了那个坑：rebase 里 `--ours` 指的是**对方的底本**。
- `06-stash-and-ignore`：改到一半要切分支，用 `git stash` 把工作区临时收起来（比 `merge --abort` 更适合「我只是要切走」）。
- `07-remote-collab`：`fetch`/`pull` 就是「别人那条分支怎么进到本地」，
  `pull` 莫名多出一个合并提交 —— 答案在本章场景 ⑥（`--ff-only`）。
- `08-investigation`：合错了、分支没了、想找「这行是谁什么时候弄的」—— 那一章按调查线索推进，
  会把本章的 `HEAD..分支`、两点/三点差异、`reflog` 当成基本功来用。


