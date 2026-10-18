# 07 远程与多人协作（remote / fetch / pull / push / tag）

> 一句话：这一章解决「你电脑上的仓库」和「别人电脑上的仓库」怎么交换东西 —— 为什么 `fetch` 不会改你的代码、`pull` 为什么莫名其妙多了个合并提交、`push` 被拒之后三条出路各自的后果。

前置：已学过 01 章的「三个区（工作区 / 暂存区 / 仓库）」、02 章的「commit 是快照 + 父指针」、06 章的「分支就是一行文本指针」。本章会大量用到「指针就是一段文本」这个认知。

本章所有输出都是在本机临时沙箱里真跑出来的（git 2.55.0，Linux）：`bash demo.sh` 按 8 个场景跑完是 **792 行输出、退出码 0**，本文每个代码块里的每一行都能在那份输出里逐字找到。

为了让它真的可复现，`demo.sh` 里做了三件事：① 每条提交固定身份 `学习者 <learn@example.com>` 和同一个时间 `2026-01-01T10:00:00 +0800`；② 设了 `GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null`，**你机器上的 `~/.gitconfig` 不会混进这份输出**（不隔离的话，你如果配过 `pull.rebase=false`，场景 4 里那条 `git pull` 会在你机器上直接 merge 成功，根本不报下面那个错）；③ 所有仓库（包括当「服务器」的 bare 仓库）都显式 `-b main`。「服务器」是本地 bare 仓库，全程没有联网，没有 clone/push 任何外部地址。

⚠ 每次跑**一定会变**的只有两样：沙箱目录名（形如 `/tmp/tmp.7pWRLdLodU`，你自己跑会换成别的），以及**merge 提交的哈希** —— git 给 pull 自动生成的合并信息里带远程 URL（`Merge branch 'main' of <URL>`），URL 里就有那个随机目录名。所以那种提交、以及依赖它的 `describe` 短哈希每跑必变（本章的 describe 演示因此特意放在一个没有 merge 提交的小仓库里跑）。除这两样以外，连跑两次的输出逐字节相同（已核对）。

⚠ 屏幕上打印的命令省略了 `-c user.name=学习者 -c user.email=learn@example.com` 这两个身份参数（真实执行的是带它们的版本）。除了这一点，`$ ` 后面那一行就是当场敲下去的那一行；输出里出现的路径、退出码、报错原文都是真的。

⚠ 关于报错语言：`demo.sh` 里设了 `LC_ALL=C`，所以 git 的提示是**英文原文**（网上搜索、查文档都按英文报错搜最准）。如果你的终端是中文 locale，同一条信息会显示成中文，例如 `ahead/behind` → `领先/落后`、`Updates were rejected because the remote contains work...` → `更新被拒绝，因为远程仓库包含您本地尚不存在的提交。` 是同一件事，不是两种错误。

⚠ `git pull --rebase` 那两段输出里的 `Rebasing (1/1)` 和 `Successfully rebased...` 在原始输出里是**同一个物理行**：中间是 git 进度行用的回车符（CR），不是换行。所以你在终端上看到的是前一句被后一句盖掉。

**本章没演 / 没验证的**（别处看到的说法，这里不敢替你背书）：
- 真实的远程服务器：GitHub / GitLab / `git://` / SSH 地址一个都没连，认证、`credential.helper`、密钥、代理全没跑 —— 本章的「远程」永远是本地 bare 仓库。
- `git fetch --prune` / `fetch.prune`、`git fetch --tags`、`git push --delete` 之外的 refs 清理（比如 `--prune-tags`）：只在散文里提了一句，**没实跑**。
- `--force-with-lease=<ref>:<expect>` 的显式写法、`--force-if-includes`：没跑（本章只跑了不带参数的 `--force-with-lease`）。
- `git bundle`、`git daemon`、浅克隆 `--depth`、`--filter` 部分克隆、`git clone --reference`：没跑。
- GPG 签名 tag（`git tag -s`）与签名验证：没跑，只说了注解 tag 可以被签名这一事实。
- 平台侧的 PR 行为（CI、审批、squash 按钮、分支保护）：全是描述，没有实跑；本章只跑了它们在 git 层面的等价命令。

---

## 你会遇到的场景

| 场景 | 本章哪一节救你 |
|---|---|
| 「公司仓库改名 / 搬家了，我的本地仓库怎么办」 | 场景 1（`remote set-url`） |
| 「我 fetch 了，代码怎么没更新？是不是坏了」 | 场景 2（fetch 的真相） |
| 「`branch -vv` 里那个 `ahead 2, behind 1` 到底啥意思」 | 场景 3 |
| 「我就想同步一下，怎么凭空多了个 Merge 提交，历史变乱了」 | 场景 4 |
| 「`git push` 报 `Updates were rejected...`，我该怎么办」 | 场景 7（重点） |
| 「`git push` 不带参数到底推的是哪个分支？会不会推到别人分支上」 | 场景 5（`push.default`） |
| 「我删了的远程分支怎么又回来了 / 打了 tag 同事说没有」 | 场景 5、6 |
| 「公司要 PR、要 squash，这玩意儿在 git 层面到底是啥」 | 场景 8 |

---

## 心智模型

### 图 1：一个远程仓库其实是「另一个完整仓库」，你们之间只靠传对象 + 挪指针

很多人以为远程仓库是「主仓库」，本地是「副本」。**不对。** 远程那一份也是一个完整的 git 仓库，也有自己的分支指针；你的仓库里有全套对象。两者之间没有实时连接，只有「你主动 fetch/push 的那一刻」才传数据。

```
        你的仓库                              服务器上的仓库（bare：只有对象和指针，没有工作区）
 ┌───────────────────────────────┐        ┌────────────────────────────────┐
 │ 工作区  README.md  report.md  │        │ objects/  ← 全部快照内容         │
 │ 暂存区  index                │        │ refs/heads/main  ──→ f4af96e    │ ← 同事推上去之后
 │ refs/heads/main      ──→ 8e8f0d3│      │ refs/heads/develop ──→ ...      │   服务器自己的指针
 │ refs/remotes/origin/main ─→ f4af96e│   └────────────────────────────────┘
 │ objects/  ← 包括同事的对象      │                    ▲
 └───────────────────────────────┘                    │
            │  fetch = 下载对象 + 挪这根快照指针 ────────┘
            │  push  = 上传对象 + 请求把服务器那根指针挪到我的位置（只能快进，除非 -f）
            ▼
```

### 图 2：`refs/remotes/origin/main` 是「我上次看到的对方状态」，不是对方的真实状态

这是本章**最关键**的一条认知。它和分支指针是同一种东西 —— 磁盘上一个 41 字节的文本文件，里面写着一个 commit 哈希：

```
        真实世界的时间线
   ────────────────────────────────────────────────►
   同事推了 X   同事推了 Y   同事推了 Z
                          ▲
                          │  服务器上 main = Z（真实状态）
                          │
   你这边 origin/main = Y （你上次 fetch 时的快照，之后一直没更新）
   你这边 main        = A （你自己的提交）
```

由此推出四条铁律，本章后面全部依赖它们：

1. **`fetch` 不改工作区、不改暂存区、不改你的分支。** 它只做两件事：把对方的对象下载进你的对象库、把 `refs/remotes/origin/*` 往前挪。
2. **`ahead/behind` 的对比对象是你本地那根快照指针**，所以「不 fetch 就永远看不到同事新推的东西」，`behind 0` 不代表对方真的没提交。
3. **`pull` = `fetch` + （merge 或 rebase）**，后半段才是真的动你的代码。
4. **`push` 默认只允许「快进」**：服务器会检查「你要我指过去的那个提交，是不是包含了我当前历史的全部内容」。不是，就拒绝 → 这就是 `non-fast-forward` 报错的来源。

### 图 3：为什么叫 origin？—— 它只是个别名

```
 .git/config 里真实存在的东西：
   [remote "origin"]                      ← "origin" 是这个名字随便起的
           url = /tmp/.../origin.git      ← 真正的地址
           fetch = +refs/heads/*:refs/remotes/origin/*   ← 对方的 main 抓回来放哪儿
   [branch "main"]
           remote = origin                ← 这就是 "upstream"
           merge  = refs/heads/main       ← 具体对应远程的哪个分支
```

`git clone` 时 git 默认把源仓库起名 `origin`，仅此而已。一个仓库可以有多个 remote（`origin` + `upstream` + `mirror`），互不打架。

---

## 场景逐个实战

### 场景 1：仓库搬家 / 想把 origin 改名 / 想看看 remote 到底是什么

**什么情况**：接手一个仓库，想知道 `origin` 指向哪；或者公司服务器换了域名、从 HTTP 换成 SSH；或者觉得 `origin` 这个名字不直观想改成 `upstream`。

**敲哪几条命令**

```bash
git remote -v                       # 列别名 + URL（fetch/push 各一行，可以不同）
git remote show origin              # 看对方有哪些分支、我的跟踪关系、pull/push 会怎么动
git remote get-url origin           # 只要 URL
git remote set-url origin <新地址>   # 换地址，别的一律不动
git remote rename origin upstream   # 改名（连配置和 refs/remotes/* 一起改）
git remote remove origin            # 只删别名
```

**为什么是这几条**：remote 只是配置里的一个节（见图 3），所以「改名/换地址/删别名」都是纯配置操作，不碰对象也不碰提交。`remote show` 比 `-v` 有用得多 —— 它会**真的连一次对方**去问分支列表，所以能在不改动任何东西的前提下看清对方状态。

**真实输出**（沙箱里 `origin` 指向一个本地 bare 仓库）

```text
$ cd /tmp/tmp.7pWRLdLodU/seed && git remote add origin /tmp/tmp.7pWRLdLodU/origin.git && git remote -v
origin	/tmp/tmp.7pWRLdLodU/origin.git (fetch)
origin	/tmp/tmp.7pWRLdLodU/origin.git (push)

$ cd /tmp/tmp.7pWRLdLodU/seed && git push -qu origin main

$ cd /tmp/tmp.7pWRLdLodU/seed && git remote show origin
* remote origin
  Fetch URL: /tmp/tmp.7pWRLdLodU/origin.git
  Push  URL: /tmp/tmp.7pWRLdLodU/origin.git
  HEAD branch: main
  Remote branch:
    main tracked
  Local branch configured for 'git pull':
    main merges with remote main
  Local ref configured for 'git push':
    main pushes to main (up to date)
```

改名会连带远程跟踪引用一起改名（注意 `refs/remotes/upstream/main`）：

```text
$ cd /tmp/tmp.7pWRLdLodU/seed && git remote rename origin upstream && git remote -v && git show-ref
upstream	/tmp/tmp.7pWRLdLodU/origin.git (fetch)
upstream	/tmp/tmp.7pWRLdLodU/origin.git (push)
4a494a87255595c29217d418aba53eb2c6d9100d refs/heads/main
4a494a87255595c29217d418aba53eb2c6d9100d refs/remotes/upstream/main
```

删别名不会丢任何东西：

```text
$ cd /tmp/tmp.7pWRLdLodU/seed && git remote remove origin && git remote -v && echo '(别名没了)'
(别名没了)

$ cd /tmp/tmp.7pWRLdLodU/seed && git log --oneline -1 && echo '(提交还在)'
4a494a8 init: 建仓库
(提交还在)
```

**常见翻车**
- 只 `remote remove` 就以为「和服务器断开了」：`refs/remotes/origin/*` 和 `branch.*.remote` 可能残留，重新 `remote add` 后行为怪。彻底重来：`git remote remove origin` 会把该 remote 的跟踪引用一起清掉，但 `branch.*.remote` 里指向它的分支会失去跟踪（`branch -vv` 方括号消失）。
- `remote -v` 看不到对方分支就以为对方是空的 —— 要用 `remote show origin` 或 `git ls-remote`。
- 在 CI 里 clone 之后 `remote show origin` 卡住：那是 shallow/单分支 clone，跟踪 refspec 被限制了（见「这章的坑」第 5 条）。

---

### 场景 2：「我 fetch 了，代码怎么没变？」—— 把 fetch 的真面目看清

**什么情况**：同事推了新东西，你 `git fetch` 完，`ls` 一看文件没变，`git status` 说 `working tree clean`，你以为 fetch 失败了。

**敲哪几条命令**

```bash
git fetch                              # 下载对方对象 + 更新 refs/remotes/origin/*
git log --oneline -3                   # 你的分支：不动
git log --oneline -3 origin/main       # 远程快照：前进了
git log --oneline HEAD..origin/main    # 对方比你多哪些提交（只读！）
git diff HEAD origin/main --stat       # 具体差在哪些文件
git merge origin/main                  # 这一步才真的改工作区
```

**为什么是这几条**：`HEAD..origin/main` 这种「两点」写法读作「从 origin/main 里减去 HEAD 能到达的提交」，也就是「他有我没有」。它和 `git merge` 用的是同一套图论，所以先看清楚再 merge 是零风险的（只读）。

**真实输出**（同事 bob 已经推了一个提交，你在 alice 里）

```text
$ cd /tmp/tmp.7pWRLdLodU/bob && git push -q origin main && echo '(同事推送成功)'
(同事推送成功)

$ cd /tmp/tmp.7pWRLdLodU/alice && git log --oneline -3
4a494a8 init: 建仓库

$ cd /tmp/tmp.7pWRLdLodU/alice && git status && ls
On branch main
Your branch is up to date with 'origin/main'.

nothing to commit, working tree clean
README.md

$ cd /tmp/tmp.7pWRLdLodU/alice && git fetch
From /tmp/tmp.7pWRLdLodU/origin
   4a494a8..7193afa  main       -> origin/main

$ cd /tmp/tmp.7pWRLdLodU/alice && git log --oneline -3 && echo '↑ 你的 main 一点没动'
4a494a8 init: 建仓库
↑ 你的 main 一点没动

$ cd /tmp/tmp.7pWRLdLodU/alice && git log --oneline -3 origin/main && echo '↑ 远程快照已经前进'
7193afa 同事: 加了 report.md
4a494a8 init: 建仓库
↑ 远程快照已经前进
```

注意 `fetch` 那行输出 `4a494a8..7193afa  main -> origin/main`：**箭头右边是 `origin/main`，不是 `main`**。它自己就把「我只动快照」说明白了。

那根快照在磁盘上就是个文本文件，跟分支指针完全同一种东西（06 章讲过）：

```text
$ cd /tmp/tmp.7pWRLdLodU/alice && git show-ref
4a494a87255595c29217d418aba53eb2c6d9100d refs/heads/main
7193afa866f1934d4d2adb7ca65dc458c3d98959 refs/remotes/origin/HEAD
7193afa866f1934d4d2adb7ca65dc458c3d98959 refs/remotes/origin/main

$ cat /tmp/tmp.7pWRLdLodU/alice/.git/refs/remotes/origin/main
7193afa866f1934d4d2adb7ca65dc458c3d98959
```

真正动工作区的是 merge（这次是快进，所以连 merge 提交都不产生）：

```text
$ cd /tmp/tmp.7pWRLdLodU/alice && git merge origin/main && ls && git log --graph --oneline -3
Updating 4a494a8..7193afa
Fast-forward
 report.md | 1 +
 1 file changed, 1 insertion(+)
 create mode 100644 report.md
README.md
report.md
* 7193afa 同事: 加了 report.md
* 4a494a8 init: 建仓库
```

**常见翻车**
- 以为 `git pull` = 「刷新一下」就天天敲，结果历史里塞满 merge 提交（见场景 4）。
- 看到 `git status` 说 `up to date with 'origin/main'` 就以为同事没改 —— 这句话只对比你本地那根**可能已经过期**的快照。
- 想撤销 fetch：**根本不需要撤销**，fetch 什么都没改（只多了些对象和一根快照指针）。真嫌快照太新，`git fetch -p`（prune）或 `git update-ref -d refs/remotes/origin/xxx`。

---

### 场景 3：`branch -vv` 里 `ahead 2, behind 1` 怎么读，数字是怎么算出来的

**什么情况**：`git push` 前想确认自己状态；或者 CI 脚本要判断分支是否落后。

**敲哪几条命令**

```bash
git branch -vv                              # 方括号里就是 upstream + ahead/behind
git status -sb                              # 同样信息的极简版（脚本里常用）
git config --local --get-regexp '^branch\.' # upstream 关系的真身：两行配置
git rev-list --left-right --count origin/main...main   # 自己算一遍
git log --oneline origin/main ^main         # behind 的那些提交具体是谁
```

**为什么是这几条**：`ahead/behind` 不是 git 记的账，是**每次现场算**的对称差集：`ahead = 我有他没有的提交数`，`behind = 他有我没有的提交数`（"他"= 本地那根快照）。既然能现场算，就能自己数一遍验证。

**真实输出**（你本地提交了一个，快照上有同事的一个）

```text
$ cd /tmp/tmp.7pWRLdLodU/alice && git branch -vv
* main 9381fb3 [origin/main: ahead 1] alice: 加 todo.md

$ cd /tmp/tmp.7pWRLdLodU/alice && git rev-list --left-right --count origin/main...main
0	1

$ cd /tmp/tmp.7pWRLdLodU/alice && echo -n 'ahead: ' && git rev-list --count main ^origin/main && echo -n 'behind: ' && git rev-list --count origin/main ^main
ahead: 1
behind: 0
```

读法（务必按这个顺序念）：`[origin/main: ahead 1, behind 1]`
- `origin/main` = **我跟踪的那根远程引用**（upstream）。裸 `git pull` / `git push` 就是和它打交道。
- `ahead 1` = **我的分支比它多 1 个提交** → 这 1 个还没推上去，我 `push` 有活干。
- `behind 1` = **它比我多 1 个提交** → 这通常是同事推的，我 `pull` 有活干。
- 两者同时非 0 = **分叉了（diverged）** → push 会被拒，见场景 7。

关键陷阱：`behind` 是按**本地快照**算的。同事又推了一个，你 `branch -vv` 数字纹丝不动：

```text
$ cd /tmp/tmp.7pWRLdLodU/bob && echo '同事又改了' > extra.md && git add -A && git commit -qm '同事: 又改了一次' && git push -q origin main && echo '(同事又推了一个，但你没 fetch)'
(同事又推了一个，但你没 fetch)

$ cd /tmp/tmp.7pWRLdLodU/alice && git branch -vv && echo '↑ 数字完全没变，因为它压根不知道'
* main 9381fb3 [origin/main: ahead 1] alice: 加 todo.md
↑ 数字完全没变，因为它压根不知道

$ cd /tmp/tmp.7pWRLdLodU/alice && git fetch -q && git branch -vv && echo '↑ fetch 之后 behind 才对上'
* main 9381fb3 [origin/main: ahead 1, behind 1] alice: 加 todo.md
↑ fetch 之后 behind 才对上
```

`-u`（`--set-upstream`）到底干了什么 —— 就是写那两行配置：

```text
$ cd /tmp/tmp.7pWRLdLodU/alice && git push -u origin feature/login
To /tmp/tmp.7pWRLdLodU/origin.git
 * [new branch]      feature/login -> feature/login
branch 'feature/login' set up to track 'origin/feature/login'.

$ cd /tmp/tmp.7pWRLdLodU/alice && git config --local --get-regexp '^branch\.feature'
branch.feature/login.remote origin
branch.feature/login.merge refs/heads/feature/login

$ cd /tmp/tmp.7pWRLdLodU/alice && git branch -vv
* feature/login 062a706 [origin/feature/login] alice: 登录骨架
  main          9381fb3 [origin/main: ahead 1, behind 1] alice: 加 todo.md
```

**常见翻车**
- `ahead/behind` 数字不对 → 99% 是忘了 fetch。想只看远程真实状态又不想动本地：`git ls-remote origin`（只读、不下载对象）。
- `git branch -u origin/main` 可以给**已有分支**补跟踪关系（不用重新 push）。想取消：`git branch --unset-upstream`。
- 把 upstream 理解成「必须同名」：它记录的是 `refs/heads/xxx` 这个具体路径，名字可以和本地不一样，但那会引出 `push.default=simple` 的拒绝（场景 5）。

---

### 场景 4：`fetch` vs `pull` vs `pull --rebase` —— 同一份分叉，三种历史形状

**什么情况**：「我就想同步一下代码，怎么凭空多了一个 Merge 提交，历史变成一坨麻花了？」

**敲哪几条命令**

```bash
git pull                       # 默认会先问你怎么合（2.34 起）
git -c pull.rebase=false pull  # 显式 merge 路线 → 产生 merge 提交（菱形）
git pull --rebase              # rebase 路线 → 一条直线
git fetch && git log ... && git merge origin/main   # 最稳：看清楚再动
```

**为什么是这几条**：分叉时只有两种落地方式 —— 造一个有两个父亲的提交把两边接起来（merge），或者把你那几个提交摘下来重新贴到对方后面（rebase）。前者不改写历史但形状变菱形，后者形状最干净但**改写了你自己的提交哈希**。

**先认清分叉现场**（`s4/alice` 有你的 1 个提交，远程快照有同事的 1 个）

```text
$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git fetch -q && git log --oneline --graph --all -5
* 8e8f0d3 alice: 我的功能提交
| * f4af96e bob: 同事的紧急修复
|/  
* 4a494a8 init: 建仓库

$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git branch -vv
* main 8e8f0d3 [origin/main: ahead 1, behind 1] alice: 我的功能提交
```

**形状 A：pull 走 merge 路线 —— 多出一个 merge 提交**

```text
$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git -c pull.rebase=false pull
Merge made by the 'ort' strategy.
 bob.txt | 1 +
 1 file changed, 1 insertion(+)
 create mode 100644 bob.txt

$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git log --graph --oneline -5
*   e362901 Merge branch 'main' of /tmp/tmp.7pWRLdLodU/s4/origin
|\  
| * f4af96e bob: 同事的紧急修复
* | 8e8f0d3 alice: 我的功能提交
|/  
* 4a494a8 init: 建仓库

$ cd /tmp/tmp.7pWRLdLodU/s4/alice && echo '这个 merge 提交有两个 parent，这就是『合并痕迹』：' && git cat-file -p HEAD | head -5
这个 merge 提交有两个 parent，这就是『合并痕迹』：
tree 26069d4d8f7235a285008db7f34330086c646c0f
parent 8e8f0d3c107d675dd6e9b5e0ab8dca67cc092137
parent f4af96e8fa141f00be96e5a37aded2e4f982deca
author 学习者 <learn@example.com> 1767232800 +0800
committer 学习者 <learn@example.com> 1767232800 +0800
```

**这就是「莫名其妙的 merge commit」的完整成因**：`git pull` = `git fetch` + `git merge`；只要 `fetch` 之后发现两边分叉（不是快进），`merge` 就必须造一个**两个 parent** 的提交（上面 `cat-file -p` 里两行 `parent`）。提交信息里那句 `Merge branch 'main' of <URL>` 的 `of <URL>` 就是它来自 pull 的签名 —— 纯本地 `git merge xxx` 不会带 `of` 部分。

**形状 B：pull --rebase —— 一条直线，但你的提交哈希变了**

```text
$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git pull --rebase
Rebasing (1/1)
Successfully rebased and updated refs/heads/main.

$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git log --graph --oneline -5
* eb410ed alice: 我的功能提交
* f4af96e bob: 同事的紧急修复
* 4a494a8 init: 建仓库

$ cd /tmp/tmp.7pWRLdLodU/s4/alice && echo '重贴前的提交：' && git log --oneline -1 8e8f0d3c107d675dd6e9b5e0ab8dca67cc092137 && echo '重贴后的提交：' && git log --oneline -1 HEAD && git reflog -4
重贴前的提交：
8e8f0d3 alice: 我的功能提交
重贴后的提交：
eb410ed alice: 我的功能提交
eb410ed HEAD@{0}: pull --rebase (finish): returning to refs/heads/main
eb410ed HEAD@{1}: pull --rebase (pick): alice: 我的功能提交
f4af96e HEAD@{2}: pull --rebase (start): checkout f4af96e8fa141f00be96e5a37aded2e4f982deca
8e8f0d3 HEAD@{3}: reset: moving to 8e8f0d3c107d675dd6e9b5e0ab8dca67cc092137
```

`8e8f0d3 → eb410ed`：改动内容（tree）一模一样，只因为 parent 从 `4a494a8` 换成了 `f4af96e`，所以**是另一个提交**。这就是「rebase 改写历史」的最小实例，也是「已经推出去的提交不要 rebase」这条规矩的由来（05 章详述）。

**形状 C：fetch 之后自己 merge —— 和 A 结果同形，但你多了一次「看清楚」**

```text
$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git reset -q --hard 8e8f0d3c107d675dd6e9b5e0ab8dca67cc092137 && git merge origin/main
Merge made by the 'ort' strategy.
 bob.txt | 1 +
 1 file changed, 1 insertion(+)
 create mode 100644 bob.txt

$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git log --graph --oneline -5
*   c2a0e9b Merge remote-tracking branch 'origin/main'
|\  
| * f4af96e bob: 同事的紧急修复
* | 8e8f0d3 alice: 我的功能提交
|/  
* 4a494a8 init: 建仓库
```

**那能不能什么都不配置，直接 `git pull`？** 新版 git 会停下来问你，而不是替你决定：

```text
$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git reset -q --hard 8e8f0d3c107d675dd6e9b5e0ab8dca67cc092137 && git pull
hint: You have divergent branches and need to specify how to reconcile them.
hint: You can do so by running one of the following commands sometime before
hint: your next pull:
hint:
hint:   git config pull.rebase false  # merge
hint:   git config pull.rebase true   # rebase
hint:   git config pull.ff only       # fast-forward only
hint:
hint: You can replace "git config" with "git config --global" to set a default
hint: preference for all repositories. You can also pass --rebase, --no-rebase,
hint: or --ff-only on the command line to override the configured default per
hint: invocation.
fatal: Need to specify how to reconcile divergent branches.
[命令退出码 = 128 —— 上面的报错是故意让它发生的]
```

**选择建议**

| 你的情况 | 建议 |
|---|---|
| 团队协作、想少踩坑、不在意历史里的菱形 | `git config pull.rebase false`（= merge 路线），最保守，不会改写任何提交 |
| 个人项目 / 讨厌菱形 / 团队接受「只 rebase 自己的未推送提交」 | `git config pull.rebase true`，历史是一条线，bisect / revert 都更舒服 |
| 只允许快进，分叉就报错让我自己处理（很多公司的推荐姿势） | `git config pull.ff only`，然后需要时手动 `git rebase origin/main` |
| 什么都还没配 | 新版 git 会 `fatal` 让你表态（上面的输出），这其实比旧版自动 merge 更安全 |

配置写在**仓库级** `.git/config`（`git config pull.rebase true`）比 `--global` 更稳，因为有的仓库确实需要不同策略。

**常见翻车**
- 配了 `pull.rebase=true` 之后，在有**未提交改动**时 pull → rebase 会中途卡住要求处理冲突，很多人这时会以为「代码坏了」。记住 rebase 是**逐个提交**重放，一个坏了就停下（10 章有急救）。
- 用 `pull --rebase` 但分支已经推给同事了 → 你之后必须 `push --force-with-lease`，同事会被迫重新对齐。团队规矩：**共享分支不 rebase**。
- `git stash` + `git pull --rebase` + `git stash pop` 是常用组合，但 pop 也可能冲突，别在 pull --rebase 失败没收拾干净时 pop。

---

### 场景 5：push 的各种形态 —— 不带参数时到底推了什么？

**什么情况**：新人最慌的一件事：`git push` 会不会推错分支？怎么删远程分支？`--all` 能不能用？

**敲哪几条命令**

```bash
git push                                  # 按 upstream + push.default 决定推哪
git push origin main                      # 显式：推本地 main 到 origin 的 main
git push origin main:backup               # 冒号左边本地、右边远程，可以不同名
git push -u origin feature/x              # 第一次建立跟踪关系
git push --all origin                     # 所有本地分支（团队仓库慎用）
git push --tags origin                    # 所有本地 tag
git push origin --delete feature/x        # 删远程分支（长写法，推荐）
git push origin :feature/x                # 同一件事（冒号左边空 = 把「空」推上去）
git ls-remote --heads origin              # 只读确认远程到底有几条分支
```

**真实输出**：先看你本地和远程各在哪

```text
$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git reset -q --hard 8e8f0d3c107d675dd6e9b5e0ab8dca67cc092137 && git log --oneline -2 && echo '↑ 你本地；下面看远程：' && git --git-dir=/tmp/tmp.7pWRLdLodU/s4/origin.git log --oneline -2
8e8f0d3 alice: 我的功能提交
4a494a8 init: 建仓库
↑ 你本地；下面看远程：
f4af96e bob: 同事的紧急修复
4a494a8 init: 建仓库

$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git -c pull.rebase=false pull -q && git push 2>&1 | tail -3
To /tmp/tmp.7pWRLdLodU/s4/origin.git
   f4af96e..e362901  main -> main
```

`f4af96e..e362901  main -> main` 读法：**远程 main 从 f4af96e 快进到 e362901**（左边是旧位置、右边是新位置）。

指定远程分支名（不同名也可以）：

```text
$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git push origin main:main-backup && git ls-remote --heads origin
To /tmp/tmp.7pWRLdLodU/s4/origin.git
 * [new branch]      main -> main-backup
e3629017027e4fad016efb92fe225e7c60eb8bbd	refs/heads/main
e3629017027e4fad016efb92fe225e7c60eb8bbd	refs/heads/main-backup
```

`--all` 的事故现场 —— 本地那几个随手开的 wip 分支全被甩上服务器了：

```text
$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git branch && echo '↑ 本地有这些分支，然后看 --all 会把它们全甩出去：'
* main
  wip/另一件事
  wip/随手试验
↑ 本地有这些分支，然后看 --all 会把它们全甩出去：

$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git push --all origin
To /tmp/tmp.7pWRLdLodU/s4/origin.git
 * [new branch]      wip/另一件事 -> wip/另一件事
 * [new branch]      wip/随手试验 -> wip/随手试验

$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git ls-remote --heads origin && echo '↑ wip 垃圾分支已经躺在服务器上了'
bfbd69da97d674af088fdded835e8fafd85c48db	refs/heads/main
bfbd69da97d674af088fdded835e8fafd85c48db	refs/heads/main-backup
96d43e93a2be06cdff6084a2c59911eb20a2981e	refs/heads/wip/另一件事
bfbd69da97d674af088fdded835e8fafd85c48db	refs/heads/wip/随手试验
↑ wip 垃圾分支已经躺在服务器上了

$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git push --tags origin 2>&1 | tail -3
To /tmp/tmp.7pWRLdLodU/s4/origin.git
 * [new tag]         v5.0-只在这台机器上 -> v5.0-只在这台机器上
```

两种删除写法（结果完全一样，只是可读性差别）：

```text
$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git push origin --delete main-backup
To /tmp/tmp.7pWRLdLodU/s4/origin.git
 - [deleted]         main-backup

$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git push origin --delete wip/随手试验 && echo '↑ 长写法，推荐'
To /tmp/tmp.7pWRLdLodU/s4/origin.git
 - [deleted]         wip/随手试验
↑ 长写法，推荐

$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git push origin :wip/另一件事 && echo '↑ 短写法：冒号左边空 = 删除远程同名分支'
To /tmp/tmp.7pWRLdLodU/s4/origin.git
 - [deleted]         wip/另一件事
↑ 短写法：冒号左边空 = 删除远程同名分支
```

⚠ **删远程分支是破坏性操作**：如果你的平台允许「已合并的分支自动删除」，别人在 PR 里引用过这条分支名也会失效。沙箱里随便试，真实仓库里先确认「这条分支的提交已经进了主干」。

**push.default 三个值实测**。故意造一个怪现场：本地分支叫 `mywork`，它的 upstream 却是 `origin/other-name`：

```text
$ cd /tmp/tmp.7pWRLdLodU/pdx && git branch -vv && echo '↑ 本地 mywork 的 upstream 是 origin/other-name（名字不一致）'
  main   51ebadf [origin/main] start
* mywork 7801ce1 [origin/other-name] mywork 提交

$ cd /tmp/tmp.7pWRLdLodU/pdx && git -c push.default=simple push
fatal: The upstream branch of your current branch does not match
the name of your current branch.  To push to the upstream branch
on the remote, use

    git push origin HEAD:other-name

To push to the branch of the same name on the remote, use

    git push origin HEAD

To avoid automatically configuring an upstream branch when its name
won't match the local branch, see option 'simple' of branch.autoSetupMerge
in 'git help config'.
[命令退出码 = 128 —— 上面的报错是故意让它发生的]

$ cd /tmp/tmp.7pWRLdLodU/pdx && git -c push.default=current push && git ls-remote --heads origin
To /tmp/tmp.7pWRLdLodU/pd.git
 * [new branch]      mywork -> mywork
51ebadfeb8037ca3459757f864a421dceeb14487	refs/heads/main
7801ce1743b7d3f5364ccf2b729db17da922bb8b	refs/heads/mywork
7801ce1743b7d3f5364ccf2b729db17da922bb8b	refs/heads/other-name

$ cd /tmp/tmp.7pWRLdLodU/pdx && git -c push.default=upstream push
Everything up-to-date
```

| 值 | 一句话规则 | 危险点 |
|---|---|---|
| `simple`（现在的默认） | 只允许「本地分支 → 同名 upstream 分支」，名字不一致或没有 upstream 就拒绝 | 几乎没有。它存在的意义就是防误推 |
| `current` | 本地分支叫什么，就推到远程同名分支，**完全不看 upstream** | 会不断在服务器上长出新分支 |
| `upstream` | 顺着 upstream 推，哪怕远程分支名不同 | 古老默认值。历史上 `git push` 会把所有同名分支都推上去，误伤风险大 |
| `nothing` | 拒绝一切裸 push，必须写全 | CI 里防手滑的好选择 |

为什么现在默认是 `simple`：旧默认 `matching`（不在这四个里，是更早的默认）会「把所有本地分支推到远程同名分支」，一个人手一抖能改一堆别人的分支。`upstream` 又太容易把你推到名字相近的别人分支上。`simple` 是「只有你明确跟踪的那一条、且名字一致才推」，最小副作用。

另外两个有用的写法：`git push origin HEAD`（推当前分支，不用记分支名，脚本里最稳）、`git push --force-with-lease`（见场景 7）。

**关于 `--force-with-lease`，只点一句（详见 05 章）**：它的判据是「远程那根指针必须还停在我上次 fetch 到的位置」。如果你 fetch 之后同事又推了，git 就会拒绝你 —— 这正是它比裸 `-f` 安全的地方：裸 `-f` 是「不管远程有什么，一律按我说的改」，`-f` + lease 是「按我说的改，但如果有人在我之后动过，就别改」。

---

### 场景 6：tag —— 轻量 vs 注解，`cat-file -t` 直接给出铁证

**什么情况**：发版要打 tag；别人说「你那个 tag 没有说明信息」；`git describe` 出来的字符串看不懂。

**敲哪几条命令**

```bash
git tag v1.0                        # 轻量 tag：就是个引用，直接指向 commit
git tag -a v1.0 -m "第一个正式版"     # 注解 tag：独立对象，有作者/时间/说明
git cat-file -t v1.0                # ← 证明两者不是同一种对象
git tag -n                          # 列表 + 注解内容
git show v1.0                       # 注解 tag 会先显示 tag 对象本身
git push origin v1.0                # push 默认不带 tag，必须显式推
git push origin --delete v1.0       # 删远程 tag
git tag -d v1.0                     # 删本地 tag
git describe --tags                 # tag名-落后提交数-g短哈希
```

**真实输出**

```text
$ cd /tmp/tmp.7pWRLdLodU/s4/alice && echo '### 对象类型：轻量 tag = 一个直接指向 commit 的引用；注解 tag = 独立的 tag 对象' && git cat-file -t v0.1-light && git cat-file -t v1.0
### 对象类型：轻量 tag = 一个直接指向 commit 的引用；注解 tag = 独立的 tag 对象
commit
tag
```

这就是硬证据：轻量 tag 这个引用**直接指向 commit 对象**，注解 tag 指向一个**独立的 tag 对象**，那个 tag 对象再指向 commit。tag 对象里存的东西：

```text
$ cd /tmp/tmp.7pWRLdLodU/s4/alice && echo '### 注解 tag 对象里存的内容（谁、什么时候、说了什么、指向谁）：' && git cat-file -p v1.0
### 注解 tag 对象里存的内容（谁、什么时候、说了什么、指向谁）：
object eaa67a05754cb2f6f8087b0c8b1848deb04a3b84
type commit
tag v1.0
tagger 学习者 <learn@example.com> 1767232800 +0800

第一个正式版
```

因为是两种对象，所以 `rev-parse` 的行为不一样（`^{}` 的作用是「一层层解引用到 commit」）：

```text
$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git rev-parse v0.1-light v1.0 v1.0^{} && echo '↑ 注解 tag 要加 ^{} 才拿到 commit；轻量 tag 两个写法一样'
eaa67a05754cb2f6f8087b0c8b1848deb04a3b84
620e827645484816b487ae086e4c278e3692a246
eaa67a05754cb2f6f8087b0c8b1848deb04a3b84
↑ 注解 tag 要加 ^{} 才拿到 commit；轻量 tag 两个写法一样
```

一次看全所有 tag 的类型（脚本里排障很好用）：

```text
$ cd /tmp/tmp.7pWRLdLodU/s4/alice && echo '### 一次看全：名字 / 对象类型 / 指向的 commit' && git tag --format='%(refname:short) %(objecttype) -> %(*objectname:short)'
### 一次看全：名字 / 对象类型 / 指向的 commit
v0.1-light commit -> 
v1.0 tag -> eaa67a0
v5.0-只在这台机器上 commit -> 
```

推送与删除：

```text
$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git push origin v1.0 && git ls-remote --tags origin
To /tmp/tmp.7pWRLdLodU/s4/origin.git
 * [new tag]         v1.0 -> v1.0
620e827645484816b487ae086e4c278e3692a246	refs/tags/v1.0
eaa67a05754cb2f6f8087b0c8b1848deb04a3b84	refs/tags/v1.0^{}
eaa67a05754cb2f6f8087b0c8b1848deb04a3b84	refs/tags/v5.0-只在这台机器上

$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git push origin --delete v0.1-light
To /tmp/tmp.7pWRLdLodU/s4/origin.git
 - [deleted]         v0.1-light

$ cd /tmp/tmp.7pWRLdLodU/s4/alice && git tag -d v0.1-light
Deleted tag 'v0.1-light' (was eaa67a0)
```

`describe` 的用途 = 「用最近的注解 tag 描述我现在在哪」，CI 打版本号、给崩溃日志标版本全靠它：

```text
$ cd /tmp/tmp.7pWRLdLodU/dtag && git log --oneline -3 && echo '↑ v1.0 打在最底下那个提交上，上面还有 2 个提交'
d9782df tag 后的第二个提交
fdfaeb4 tag 后的第一个提交
2aa5a45 初始版本
↑ v1.0 打在最底下那个提交上，上面还有 2 个提交

$ cd /tmp/tmp.7pWRLdLodU/dtag && git describe --tags
v1.0-2-gd9782df

$ cd /tmp/tmp.7pWRLdLodU/dtag && git describe --tags --abbrev=0 && echo '↑ --abbrev=0 只要 tag 名，CI 里取版本号常用'
v1.0
↑ --abbrev=0 只要 tag 名，CI 里取版本号常用

$ cd /tmp/tmp.7pWRLdLodU/dtag && echo '临时改动' >> two.c && git describe --tags --dirty && git checkout -q -- two.c
v1.0-2-gd9782df-dirty
```

`v1.0-2-gd9782df` 读法：最近的 tag 是 `v1.0` → 距离它 `2` 个提交 → 当前提交短哈希 `d9782df`，前面的 `g` 是「git」的意思，不属于哈希。`--abbrev=0` 只回 tag 名；`--dirty` = 工作区有未提交改动（上面那条命令改完 `two.c` 立刻用 `git checkout -q -- two.c` 还原了）。

轻量 tag 默认**不被 describe 认账**（这也是「为什么推荐注解 tag」最实在的理由之一）：

```text
$ cd /tmp/tmp.7pWRLdLodU/desc && echo '### 这个仓库只有轻量 tag v0-light：' && git describe
### 这个仓库只有轻量 tag v0-light：
fatal: No annotated tags can describe 'd09398e30b1a95a4c91d64de83eb030733729af6'.
However, there were unannotated tags: try --tags.
[命令退出码 = 128 —— 上面的报错是故意让它发生的]

$ cd /tmp/tmp.7pWRLdLodU/desc && git describe --tags && echo '↑ 加 --tags 才肯用轻量 tag'
v0-light-1-gd09398e
↑ 加 --tags 才肯用轻量 tag

$ cd /tmp/tmp.7pWRLdLodU/desc && git tag -a v0-annotated -m '注解版' && git describe && echo '↑ 有注解 tag 之后 describe 直接就能用了'
v0-annotated
↑ 有注解 tag 之后 describe 直接就能用了
```

**为什么发版要用注解 tag**：① 带作者和时间，能审计「谁在什么时候决定这就是 1.0」；② 可以被 GPG 签名（`-s`）；③ `describe` 默认只认它；④ 轻量 tag 只是个引用，容易被误挪位置，而 tag 对象内容写进了哈希，改了就不是同一个 tag。

**常见翻车**
- `git push` 不带 tag → 同事那边 `git tag` 看不到。要么 `git push origin v1.0`，要么 `push --tags`（但 `--tags` 会把本地所有 tag 甩出去，包括试验的）。
- 已经推出去的 tag 不要随便改名/挪位置：别人的 clone 里那个旧 tag 还在，会出现「同一个 tag 名两个含义」。
- 本地 tag 不会因为 `git fetch` 自动更新（除非配了 `remote.origin.fetch` 带 tags 或用 `git fetch --tags`）。`git fetch --prune-tags` 才能删掉本地多余的 tag。

---

### 场景 7（本章重点）：双人协作撞上 `Updates were rejected...`，三条出路各自的后果

**现场**：`origin` 是一个 bare 仓库模拟的服务器，`alice` = 你，`bob` = 同事。两人从同一份 clone 出发：bob 推了他的提交，你在本地也有一个没推的提交。

#### 7.0 被拒的报错原文

```text
$ cd /tmp/tmp.7pWRLdLodU/s1/alice && git push
To /tmp/tmp.7pWRLdLodU/s1/origin.git
 ! [rejected]        main -> main (fetch first)
error: failed to push some refs to '/tmp/tmp.7pWRLdLodU/s1/origin.git'
hint: Updates were rejected because the remote contains work that you do not
hint: have locally. This is usually caused by another repository pushing to
hint: the same ref. If you want to integrate the remote changes, use
hint: 'git pull' before pushing again.
hint: See the 'Note about fast-forwards' in 'git push --help' for details.
[命令退出码 = 1 —— 上面的报错是故意让它发生的]
```

（中文 locale 下同一件事的提示是：`更新被拒绝，因为远程仓库包含您本地尚不存在的提交。`）

**它在人话里说什么**：「你要我把 `main` 指到你的提交，但你的历史里没有我现在指着的这个提交（`f4af96e`）。如果我照办，f4af96e 就从 main 上掉了 —— 那不是你写的那些提交，我不敢。」

对照图 2 的分叉图就明白了：push 只被允许「快进」，而分叉状态下你的提交不是远程的下一代。

#### 出路一：`git pull`（merge 路线）—— 同事的东西保住了，代价是多一个菱形

```text
$ cd /tmp/tmp.7pWRLdLodU/s1/alice && git -c pull.rebase=false pull 2>&1 | tail -5
   4a494a8..f4af96e  main       -> origin/main
Merge made by the 'ort' strategy.
 bob.txt | 1 +
 1 file changed, 1 insertion(+)
 create mode 100644 bob.txt

$ cd /tmp/tmp.7pWRLdLodU/s1/alice && git log --graph --oneline -5
*   2ef92cb Merge branch 'main' of /tmp/tmp.7pWRLdLodU/s1/origin
|\  
| * f4af96e bob: 同事的紧急修复
* | 8e8f0d3 alice: 我的功能提交
|/  
* 4a494a8 init: 建仓库

$ cd /tmp/tmp.7pWRLdLodU/s1/alice && git push 2>&1 | tail -2
To /tmp/tmp.7pWRLdLodU/s1/origin.git
   f4af96e..2ef92cb  main -> main
```

直接从 bare 仓库看服务器真实状态（最诚实的视角）：

```text
$ cd /tmp/tmp.7pWRLdLodU/s1/alice && echo '### 服务器上的历史（直接从 bare 仓库看，最诚实的视角）：' && git --git-dir=/tmp/tmp.7pWRLdLodU/s1/origin.git log --graph --oneline -5
### 服务器上的历史（直接从 bare 仓库看，最诚实的视角）：
*   4a77e3b Merge branch 'main' of /tmp/tmp.7pWRLdLodU/s1/origin
|\  
| * f4af96e bob: 同事的紧急修复
* | 8e8f0d3 alice: 我的功能提交
|/  
* 4a494a8 init: 建仓库
```

同事 fetch 之后：

```text
$ cd /tmp/tmp.7pWRLdLodU/s1/bob && git fetch -q && echo '### 同事 fetch 之后看到的远程（他的本地 main 一点没动，status 里 behind 2）：' && git status -sb && git log --graph --oneline origin/main -5
### 同事 fetch 之后看到的远程（他的本地 main 一点没动，status 里 behind 2）：
## main...origin/main [behind 2]
*   4a77e3b Merge branch 'main' of /tmp/tmp.7pWRLdLodU/s1/origin
|\  
| * f4af96e bob: 同事的紧急修复
* | 8e8f0d3 alice: 我的功能提交
|/  
* 4a494a8 init: 建仓库
```

后果：安全、零改写，但每个「撞上分叉」都会往历史里加一个只有两个父亲、不带任何新内容的提交。人多了历史就成麻花，`git log --graph` 难读、`bisect` 会遇到「合并提交两边都试」的麻烦（08 章讲）。

#### 出路二：`git pull --rebase` —— 同事的东西保住了，历史还是一直线

```text
$ cd /tmp/tmp.7pWRLdLodU/s2/alice && git pull --rebase 2>&1 | tail -4
From /tmp/tmp.7pWRLdLodU/s2/origin
   4a494a8..f4af96e  main       -> origin/main
Rebasing (1/1)
Successfully rebased and updated refs/heads/main.

$ cd /tmp/tmp.7pWRLdLodU/s2/alice && git log --graph --oneline -4
* eb410ed alice: 我的功能提交
* f4af96e bob: 同事的紧急修复
* 4a494a8 init: 建仓库

$ cd /tmp/tmp.7pWRLdLodU/s2/alice && git push 2>&1 | tail -2
To /tmp/tmp.7pWRLdLodU/s2/origin.git
   f4af96e..eb410ed  main -> main

$ cd /tmp/tmp.7pWRLdLodU/s2/alice && echo '### 服务器上的历史：没有菱形' && git --git-dir=/tmp/tmp.7pWRLdLodU/s2/origin.git log --graph --oneline -4
### 服务器上的历史：没有菱形
* eb410ed alice: 我的功能提交
* f4af96e bob: 同事的紧急修复
* 4a494a8 init: 建仓库
```

注意这次 push **不需要** `-f`：因为 rebase 之后你的提交是远程提交的直接后代，是快进。只有当你 rebase 了**已经推出去过的**提交时才需要 `--force-with-lease`。

后果：历史干净；代价是你的提交哈希变了（`8e8f0d3 → eb410ed`），如果这些提交已经推给别人、别人正在基于它们工作，你会逼他们重新对齐。**规矩：只 rebase 自己没推过的提交。**

#### 出路三：`git push -f` —— 把同事的提交从远程历史上抹掉（事故现场）

```text
$ cd /tmp/tmp.7pWRLdLodU/s3/alice && echo '### 先试安全的 --force-with-lease：它要求远程还停在我上次 fetch 到的位置' && git push --force-with-lease
### 先试安全的 --force-with-lease：它要求远程还停在我上次 fetch 到的位置
To /tmp/tmp.7pWRLdLodU/s3/origin.git
 ! [rejected]        main -> main (stale info)
error: failed to push some refs to '/tmp/tmp.7pWRLdLodU/s3/origin.git'
[命令退出码 = 1 —— 上面的报错是故意让它发生的]

$ cd /tmp/tmp.7pWRLdLodU/s3/alice && echo '### 现在换成裸 -f：git 不再拦你，同事的提交当场从远程历史里消失' && git push -f 2>&1 | tail -3
### 现在换成裸 -f：git 不再拦你，同事的提交当场从远程历史里消失
To /tmp/tmp.7pWRLdLodU/s3/origin.git
 + f4af96e...8e8f0d3 main -> main (forced update)

$ cd /tmp/tmp.7pWRLdLodU/s3/alice && echo '### 远程 main 现在长这样：' && git --git-dir=/tmp/tmp.7pWRLdLodU/s3/origin.git log --oneline -4
### 远程 main 现在长这样：
8e8f0d3 alice: 我的功能提交
4a494a8 init: 建仓库
```

`f4af96e`（同事的紧急修复）**已经从远程 main 上消失了**：

```text
$ cd /tmp/tmp.7pWRLdLodU/s3/alice && echo "### 同事的提交 f4af96e8fa141f00be96e5a37aded2e4f982deca 还在远程 main 上吗：" && git --git-dir=/tmp/tmp.7pWRLdLodU/s3/origin.git merge-base --is-ancestor f4af96e8fa141f00be96e5a37aded2e4f982deca refs/heads/main && echo '还在' || echo '不在了（退出码非 0）—— 他被覆盖了'
### 同事的提交 f4af96e8fa141f00be96e5a37aded2e4f982deca 还在远程 main 上吗：
不在了（退出码非 0）—— 他被覆盖了
```

同事自己的视角（他本地还留着，但对远程一无所知）：

```text
$ cd /tmp/tmp.7pWRLdLodU/s3/bob && echo '### 同事那边的视角：' && git fetch -q && git status -sb && git log --graph --oneline -4
### 同事那边的视角：
## main...origin/main [ahead 1, behind 1]
* f4af96e bob: 同事的紧急修复
* 4a494a8 init: 建仓库
```

**为什么 git 拦不住你**（也是最值得记住的一句）：因为你本地**连同事那个对象都没有** —— 你从没 fetch 过它，git 无从知道远程那份历史的重要性。

```text
$ cd /tmp/tmp.7pWRLdLodU/s3/alice && echo '### 为什么 git 拦不住你？因为你本地连同事那个对象都没有（你从没 fetch 过它）：' && git cat-file -t f4af96e8fa141f00be96e5a37aded2e4f982deca
### 为什么 git 拦不住你？因为你本地连同事那个对象都没有（你从没 fetch 过它）：
fatal: git cat-file: could not get object info
[命令退出码 = 128 —— 上面的报错是故意让它发生的]
```

而 **`--force-with-lease` 为什么能救同事**：它多问一句「远程现在的位置，是不是还等于我本地快照记录的位置」。你的快照还停在 `4a494a8 init`，远程真实已经在 `f4af96e`（同事推的），两者不符 → lease 不成立 → git 拒绝。报错里的 `(stale info)` 就是「你的信息过期了」。**结论：真要强推，先 `fetch` 把快照刷新到真实位置，再 `--force-with-lease`；它等价于「我知道远程现在有什么，我确认要覆盖」。**

**怎么救**（对象没被物理删除，所以能救）：让「东西被抹掉的同事」基于当前远程重贴自己的提交，再推上去：

```text
$ cd /tmp/tmp.7pWRLdLodU/s3/bob && git rebase origin/main 2>&1 | tail -3 && git log --graph --oneline -4 && git push -q origin main && echo '(同事重新推上去了)'
Rebasing (1/1)
Successfully rebased and updated refs/heads/main.
* da792b5 bob: 同事的紧急修复
* 8e8f0d3 alice: 我的功能提交
* 4a494a8 init: 建仓库
(同事重新推上去了)

$ cd /tmp/tmp.7pWRLdLodU/s3/alice && git fetch -q && echo '### 你这边 fetch 之后就能看到同事的提交又回到远程了：' && git log --graph --oneline origin/main -4
### 你这边 fetch 之后就能看到同事的提交又回到远程了：
* da792b5 bob: 同事的紧急修复
* 8e8f0d3 alice: 我的功能提交
* 4a494a8 init: 建仓库
```

**服务端能不能自己捞回来？取决于有没有开 reflog**（`git init --bare` 默认不开）。开了的话：

```text
$ git --git-dir=/tmp/tmp.7pWRLdLodU/serverlog.git reflog | head -6
8e8f0d3 HEAD@{0}: push
f4af96e HEAD@{1}: push

$ cd /tmp/tmp.7pWRLdLodU/s3/alice && echo '### 但 reflog 里 main@{1} 就是被覆盖掉的那个提交：' && git --git-dir=/tmp/tmp.7pWRLdLodU/serverlog.git rev-parse 'main@{1}' && echo "### 它正好等于同事的 f4af96e8fa141f00be96e5a37aded2e4f982deca" && echo '### 于是运维一条命令就能把服务器指回去'
### 但 reflog 里 main@{1} 就是被覆盖掉的那个提交：
f4af96e8fa141f00be96e5a37aded2e4f982deca
### 它正好等于同事的 f4af96e8fa141f00be96e5a37aded2e4f982deca
### 于是运维一条命令就能把服务器指回去

$ git --git-dir=/tmp/tmp.7pWRLdLodU/serverlog.git update-ref refs/heads/main 'main@{1}' && git --git-dir=/tmp/tmp.7pWRLdLodU/serverlog.git log --oneline -3
f4af96e bob: 同事的紧急修复
4a494a8 init: 建仓库
```

反过来，本章那个默认创建的 bare 仓库 `reflog` 是空的 —— 什么都查不到。**所以：真实工作里 `push -f` 到共享分支前，务必 `git fetch` 并确认 `--force-with-lease` 能通过。平台（GitHub/GitLab）上的保护分支 + PR 流程，本质就是替你堵死这条路。**

---

### 场景 8：工作流和 PR 到底是什么（全本地模拟，不需要网络）

**主干开发 / GitHub Flow**：长期分支只有 `main`；其他分支活不过一两天；改动通过 PR 评审后进主干；主干随时可发布。

**PR/MR 在 git 层面的本质**：就两步 —— ① `git push -u origin feature/xxx`（把你的分支推到远程）② 请人评审。评审通过后落地那个动作，还是你已经学过的 `merge`（`--no-ff`）或 `squash`。平台只是在这两步之间加了网页讨论、CI、审批和权限。

```text
$ cd /tmp/tmp.7pWRLdLodU/floww2 && git push -u origin feature/dark-mode 2>&1 | tail -3
To /tmp/tmp.7pWRLdLodU/flow.git
 * [new branch]      feature/dark-mode -> feature/dark-mode
branch 'feature/dark-mode' set up to track 'origin/feature/dark-mode'.

$ cd /tmp/tmp.7pWRLdLodU/floww2 && git fetch -q && echo '### 评审者（这里你自己扮演）在本地看到的：' && git log --oneline main..origin/feature/dark-mode && git diff main...origin/feature/dark-mode
### 评审者（这里你自己扮演）在本地看到的：
a236459 fix: 改 css 笔误
21b55ea feat: 加开关
6b3938f feat: 深色模式骨架
diff --git a/theme.c b/theme.c
new file mode 100644
index 0000000..64ac37b
--- /dev/null
+++ b/theme.c
@@ -0,0 +1 @@
+dark + toggle + css
```

（`main...分支` 的三个点是「从两者的共同祖先到分支」，正是评审想看的：这条分支带来了什么，不含主干上别人的改动。）

**落地方式一：merge --no-ff（保留分支形状）**

```text
$ cd /tmp/tmp.7pWRLdLodU/floww2 && git checkout -q main && git merge --no-ff -m 'Merge branch feature/dark-mode (#128)' origin/feature/dark-mode 2>&1 | tail -3 && git log --graph --oneline -6
 theme.c | 1 +
 1 file changed, 1 insertion(+)
 create mode 100644 theme.c
*   e39964d Merge branch feature/dark-mode (#128)
|\  
| * a236459 fix: 改 css 笔误
| * 21b55ea feat: 加开关
| * 6b3938f feat: 深色模式骨架
|/  
* 11360cc main: 初始版本
```

**落地方式二：squash（整条分支压成 1 个提交）**

```text
$ cd /tmp/tmp.7pWRLdLodU/floww2 && git merge --squash origin/feature/dark-mode 2>&1|tail -2 && git commit -qm 'feat: 深色模式 (#128)' && git log --graph --oneline -4
 1 file changed, 1 insertion(+)
 create mode 100644 theme.c
* 1f4d758 feat: 深色模式 (#128)
* 11360cc main: 初始版本

$ cd /tmp/tmp.7pWRLdLodU/floww2 && git push -q origin main && echo '### 服务器上的主干：干净得像一条直线' && git --git-dir=/tmp/tmp.7pWRLdLodU/flow.git log --oneline -3
### 服务器上的主干：干净得像一条直线
1f4d758 feat: 深色模式 (#128)
11360cc main: 初始版本
```

**为什么很多公司强制 squash**
1. 主干历史 = 需求/缺陷清单：一个提交 = 一件事，`git log --oneline main` 直接能当变更日志读。
2. **回滚极干净**：`git revert 1f4d758` 一条命令就能撤掉整个功能；如果是 merge 路线，revert 一个 merge 提交要处理一堆中间提交，还常常引发「后来再合并时改动不回来」的经典坑。
3. 噪音不入库：分支里那些「改笔误」「加了个 print」「删掉 print」永远留在你的分支（和 PR 页面）里，主干看不到。
4. PR 编号写进提交信息，`git log --grep '#128'` 能反查到评审记录和讨论。
5. 代价（要诚实说）：squash 后远程分支与主干**失去了父子关系**，所以「同一个分支二次合并」会冲突 —— 平台的做法是 squash 之后自动删源分支。别在 squash 策略下反复 merge 同一条长期分支。

**GitFlow**：`main`（生产）+ `develop`（日常集成）两条长期分支，加上临时的 `feature/*`、`release/*`、`hotfix/*`。适合「有明确版本发布节奏、需要同时维护多个版本」的产品（比如客户端软件），不适合每天上线的 Web 服务。

```text
$ cd /tmp/tmp.7pWRLdLodU/gf2 && git log --graph --oneline --all -16
*   2cff64e hotfix 1.0.1
|\
| * b62b609 fix: 修一个线上崩溃
|/
*   a0b9ae8 Release 1.0.0
|\
| * a95b689 chore: 版本号 1.0.0
| * 994fb0b Merge feature/search into develop
|/|
| * d98b5f3 feat: 建索引
| * a390b9b feat: 搜索
|/
* f84157c main: 起点
```

三种工作流对比：

| | 长期分支 | 一次发版怎么做 | 适合 |
|---|---|---|---|
| 主干开发（Trunk Based） | 只有 main | main 直接打 tag 发布 | 小团队、天天上线、有开关/特性标志 |
| GitHub Flow | 只有 main | 同上，改动必须走 PR | Web 服务、持续部署 |
| GitFlow | main + develop | 拉 `release/x.y`，修完合回 main + develop 再打 tag | 版本节奏慢、要多版本并行维护 |

**共同铁律**（不管哪种流程）：共享分支上**只有快进**；`main` 保护起来；自己的未推送提交可以 rebase，推出去的就别动了。

---

## 命令速查表

```bash
# —— remote 别名 ——
git remote -v                      # 别名 + URL
git remote show <name>             # 连一次对方，看分支/跟踪/pull-push 配置
git remote get-url <name>
git remote add <name> <url>
git remote set-url <name> <url>    # 换地址，别的一律不动
git remote rename <old> <new>
git remote remove <name>           # 只删别名
git ls-remote origin               # 只读：远程真实有哪些引用
git remote prune origin            # 删掉本地那些「远程已经没有」的跟踪引用

# —— 下载 ——
git fetch                          # 只更新快照，不动工作区
git fetch --all --prune            # 所有 remote，并清理消失的分支
git pull                           # fetch + (merge/rebase，2.34 起会问)
git pull --rebase                  # fetch + rebase，历史一条线
git pull --ff-only                 # 只允许快进，分叉就报错（最保守）

# —— 看差异（全部只读） ——
git log --oneline HEAD..origin/main      # 他有我没有
git log --oneline origin/main..HEAD      # 我有他没有
git rev-list --left-right --count origin/main...main   # behind / ahead
git diff HEAD origin/main --stat
git branch -vv ; git status -sb

# —— 上传 ——
git push                                   # 按 upstream + push.default
git push origin main
git push origin 本地分支:远程分支
git push -u origin 新分支                   # 第一次推 + 建立跟踪
git push --all / --tags                    # 慎用
git push origin --delete 分支名             # 删远程分支（破坏性）
git push origin :分支名                      # 同一件事的短写法
git push --force-with-lease                 # 唯一允许的强推姿势（05 章详述）
git push origin HEAD                        # 推当前分支，脚本里最稳

# —— tag ——
git tag                                  # 列表
git tag -a v1.0 -m "说明"                 # 注解 tag（发版用这个）
git tag v1.0                             # 轻量 tag（只是引用）
git cat-file -t v1.0                     # commit / tag —— 一眼看穿类型
git show v1.0 ; git tag -n
git push origin v1.0                     # push 默认不带 tag！
git push origin --delete v1.0
git tag -d v1.0
git describe --tags --dirty --abbrev=0

# —— 工作流 / PR 本质 ——
git push -u origin feature/x             # ① 提 PR = 把分支推上去
git log --oneline main..origin/feature/x # ② 评审：看提交
git diff main...origin/feature/x         # ② 评审：看改动
git merge --no-ff origin/feature/x -m 'Merge ... (#128)'   # ③ 落地：保留形状
git merge --squash origin/feature/x && git commit -m 'feat: xxx (#128)'  # ③ 落地：压一个
git revert <sha>                         # squash 之后回滚只这一条
```

---

## 这章的坑

1. **以为 `fetch` 会更新代码** —— 不会（场景 2 已实测）。fetch 之后 `git status` 会明确告诉你差多少（中文 locale 实测原文）：
   ```text
   位于分支 main
   您的分支落后 'origin/main' 共 2 个提交，并且可以快进。
     （使用 "git pull" 来更新您的本地分支）
   ```
   英文 locale 是 `Your branch is behind 'origin/main' by 2 commits, and can be fast-forwarded.`。看到这行才说明你还需要 merge/rebase。记住 `pull = fetch + 动你的代码`。
2. **`ahead/behind` 看着放心** —— 它是拿**本地快照**算的，你多久没 fetch 它就多假。养成习惯：判断状态前，先 `git fetch`，或者用 `git ls-remote` 直接问服务器。
3. **`git push` 不带参数推去哪，全凭 `push.default` + upstream** —— 新人最容易的两件事：① 新分支裸 push 直接 `fatal: The current branch xxx has no upstream branch`；② `push.default=current` 会不断在服务器上生出新分支（我们实测它当场建了 `refs/heads/mywork`）。日常建议保持默认 `simple`。
4. **`git push` 默认不带 tag** —— 「我打了 tag，同事说没有」百分百是这个。要么显式 `git push origin v1.0`，要么 `--tags`（会全推，包括试验 tag）。
5. **`push -f` 到共享分支** —— 本章场景 7.3 实测：同事的提交直接从远程历史上消失，而且 git 拦不住你（你本地连那个对象都没有）。真需要强推（自己 rebase 之后）只用 `--force-with-lease`，并且**先 fetch**。
6. **`--force-with-lease` 不是免死金牌** —— 它的保护来自「远程必须还在我上次 fetch 的位置」。如果你自己刚 fetch 过（快照已经更新到包含同事的提交），lease 会成立，你照样能把不该抹的东西抹掉。所以：**lease + 确认历史里包含别人的提交，两者都要。**
7. **删远程分支不是「撤销」** —— `push origin --delete` 之后别人本地那份分支还在，而且他下次 push 会把分支重新创建回来。要全团队清干净得靠平台设置（分支保护、自动删合并后的源分支）。
8. **squash 合并后不要 reuse 同一条分支** —— squash 不建立父子关系，再 merge 一次同样的改动会冲突；需要继续做就重新切一条新分支。
9. **`git pull` 在有未提交改动时可能把 merge 搞成半途中断状态** —— 冲突卡在 index 里（10 章有 `--abort/--continue/--quit` 的急救）。稳妥做法：push/pull 前先 `git status`，或 `git stash` 起来。

---

## 自检问题

1. **`git fetch` 之后 `git status` 说 `Your branch is up to date with 'origin/main'`，同事却说他已经推了。谁在骗你？**
   答：都没有。`status` 对比的是**你本地**的 `refs/remotes/origin/main` 快照；你还没 fetch 到他的新提交时，这句话就仍然成立。要么是你 fetch 之前看的，要么是同事推到了别的分支/别的 remote。验证方法：`git ls-remote origin`（直接问服务器）或再 `git fetch` 一次看有没有 `... -> origin/main` 的行。

2. **`branch -vv` 显示 `[origin/main: ahead 2, behind 1]`，`ahead/behind` 这两个数分别是谁比谁多？git 是怎么算的？**
   答：`ahead 2` = 你的本地分支比**本地那根 origin/main 快照**多 2 个提交（你有、它没有）；`behind 1` = 快照比你多 1 个（它有、你没有，通常来自同事）。算法是对称差集两头计数，等价于 `git rev-list --count main ^origin/main` 和 `git rev-list --count origin/main ^main`，也可用 `git rev-list --left-right --count origin/main...main` 一次拿到（顺序是 behind 在前、ahead 在后）。

3. **为什么 `git pull` 会造出一个「什么都没改」的 merge 提交？怎么避免？**
   答：`pull` = `fetch` + `merge`。分叉时 merge 必须造一个两个父亲的提交来连接两边（本章用 `git cat-file -p HEAD` 看到了两行 `parent`）。避免：用 `pull --rebase`（历史直线，但改写你自己的提交），或 `pull --ff-only`（拒绝分叉），或 `fetch` + 自己看完再决定。

4. **`push --force-with-lease` 什么时候会拒绝你？拒绝说明什么？**
   答：当「远程当前指向的位置」≠「你本地快照记录的位置」时拒绝（报错 `! [rejected] ... (stale info)`）。这说明你 fetch 之后又有人推了 —— 它拒绝的正是「你会覆盖别人」这次操作。看到 stale info 就 fetch 重新对齐，别改回 `-f`。

5. **`git tag v1.0` 和 `git tag -a v1.0 -m "x"` 打出来的东西，在对象库里的差别是什么？用什么命令证明？**
   答：轻量 tag 是一个直接指向 commit 的引用（`git cat-file -t v1.0` 输出 `commit`）；注解 tag 是独立的 tag 对象（输出 `tag`），里面存 object/type/tag 名/tagger（谁、何时）/说明文本。连带差别：`rev-parse v1.0^{}` 才能拿到 commit；`describe` 默认只认注解 tag；`git tag -n` 里轻量 tag 没有内容。

6. **「提 PR」用命令行能不能等价做出来？squash 落地和 merge --no-ff 落地，回滚难度差在哪？**
   答：能。① `git push -u origin feature/x`（= 打开 PR）② 评审者用 `git log main..origin/feature/x`、`git diff main...origin/feature/x` 看内容（= PR 页面的 Commits / Files changed）③ `git merge --no-ff` 或 `git merge --squash && git commit`（= 平台的 Merge 按钮）。回滚：squash 后一个功能 = 一个提交，`git revert <那一个>` 就够；merge --no-ff 后回滚整个功能要 revert 那个 merge 提交，容易连带丢掉后续从主干同步的改动。

---

## 运行 demo

```bash
cd git-handbook/07-remote-collab
bash demo.sh          # 跑完自动删沙箱
bash demo.sh --keep   # 保留沙箱并打印路径，可以进去自己翻
```

`demo.sh` 会按本章 8 个场景依次实跑（8 个场景 = 脚本里的 1→8 段），屏幕上每次都以 `$ 命令` 的形式打印正在执行什么，然后原样打印输出，故意失败的命令后面会补一行 `[命令退出码 = N —— 上面的报错是故意让它发生的]`。它自己建 `mktemp -d` 沙箱、用 `trap` 清理、可以在任意目录运行；不联网、不 clone 外部仓库、**不读也不写你的 `~/.gitconfig`**（脚本里 `GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_SYSTEM=/dev/null`；提交一律带 `-c user.name=学习者 -c user.email=learn@example.com`）。跑完是 792 行输出、退出码 0，章目录里不留任何文件。

**怎么核对本文的围栏不是编的**：把 demo 的输出留成一个文件，再拿本文任一条 `$` 命令去里面找。

```bash
bash demo.sh > /tmp/git07.txt 2>&1      # 本文所有 ```text 围栏的原料就是这一份
grep -n "git push --force-with-lease" /tmp/git07.txt
```

本文代码块里凡是带 `$` 提示符的行，都必须能在 `/tmp/git07.txt` 里逐字找到
（只有沙箱目录名、以及默认合并信息里带 URL 的那类 merge 提交哈希是每次重跑都会变的例外）。找不到就是写错了。

想单独看协作现场：

```bash
bash demo.sh --keep
cd <打印出来的路径>/s3/alice
git log --graph --oneline --all
git --git-dir=../origin.git reflog        # 默认 bare 仓库：什么都没有
```
