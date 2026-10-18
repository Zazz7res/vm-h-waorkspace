# 06 stash 与忽略规则（stash / .gitignore / git clean）

> 一句话：这一章解决两种「东西明明在，但 Git 装作没看见」的局面 ——
> 改到一半的活儿怎么**临时挪开、还能原样取回**（`git stash`），
> 编译产物和本地配置怎么**永远不进仓库**（`.gitignore`），
> 以及为什么「写进 .gitignore 了它怎么还在」的正解是 `git rm --cached` 而不是继续改规则。

前置：01 章的「分支就是一个写着哈希的文本文件」和工作区/暂存区/仓库三区模型；
02 章 `git status --short` 的两格符号（第 1 格 = 暂存区，第 2 格 = 工作区）；
03 章的 `reset --hard`、reflog 与 clean 真删的现场——本章第 3 节的清理、第 7 节的救援、第 11 节的 dry-run 都建立在那之上。
本章留下的尾巴（历史里那份泄露的 token 怎么清）由 10 章接走。

本章最值钱的三个认知，先放在这里：

1. **stash 不是剪贴板，是正儿八经的 Git 对象。** 一次 `git stash push` 造出一个**有三个父亲的 commit**：
   `^1` 是你 stash 那一刻 HEAD 所在的基线、`^2` 是暂存区长什么样、`^3` 是未跟踪文件（要 `-u`/`-a` 才有）。
   `refs/stash` 只是指向这个 commit 的一行文本——和 01 章的分支指针是同一种东西。第 1 节全部拍在桌上给你看。
2. **pop 撞车不吞东西。** 记住这句英文：`The stash entry is kept in case you need it again.`。
   只要它出现，stash 还躺在栈里，工作区的烂摊子随便你收拾。但**先确认 stash 还在，再动手清理**——顺序反了就是自己砸自己（第 3 节整节讲这件事）。
3. **.gitignore 只是一个「未跟踪文件的过滤器」。** 已被跟踪的文件它一个字都管不着（第 10 节现场）；
   规则有四层来源、会互相打架，别背口诀，用 `git check-ignore -v` 直接问「这条是谁在起作用」（第 8、9 节，全部实测）。

> 本章所有终端输出都来自仓库里 `bash demo.sh` 的一次真实运行（11 节、1623 行、退出码 0，原文存在 `/tmp/git06.txt`），
> 本文只是把它切开、逐段讲解，每个终端块（含 `[退出码 N]` 与 `↳` 注释行）都能在那份输出里逐字找到。
> 沙箱路径是 `mktemp` 随机生成的（本次是 `/tmp/git-handbook-06-stash-ignore-XhUYQQwQ`），
> 你重跑一次后缀一定和你自己的不一样，**这不影响其余任何一行**；提交时间戳和身份在脚本里是固定的
> （`学习者 <learn@example.com>`，从 `2026-01-01 10:01` 起逐条递增），所以**本文里的每一个提交哈希就是你复现时的哈希**。
> 带 `★ 上面这段失败是本章故意让你看的` 标注的非 0 退出码，是脚本刻意制造的错误现场，不是演示跑挂了。

---

## 你会遇到的场景

| 你遇到的事 | 本章哪一节 |
|---|---|
| 改到一半，线上着火必须切分支，这一堆改动怎么办 | 第 6 节（完整流程：半成品在手，线上着火） |
| `git stash` 到底把东西「藏」在哪了 | 第 1 节（物理证据：一根 ref + 三个 commit） |
| apply 和 pop 选哪个，差别在哪 | 第 2 节（apply 保守、pop = apply + drop） |
| pop 撞了冲突，我的 stash 会不会丢 | 第 3 节（★ 保命条款 + 两条出路） |
| 新建的文件 / 被忽略的文件，stash 收不收得到 | 第 4 节（默认只收已跟踪，`-u` 与 `-a`） |
| 三个文件只想收其中两个 | 第 5 节（`-- <路径>`、`--keep-index`） |
| drop / clear 手滑，一整天的活儿没了 | 第 7 节（`git fsck --unreachable` 捞回，附 gc 红线） |
| 同一个文件名四份规则文件都能管，到底谁说了算 | 第 8 节（四层来源与实测优先级） |
| `dir/` 加 `!dir/sub/` 为什么救不回来 | 第 9 节（9F，★ 一行规则省半小时） |
| 写进 .gitignore 了它还在 status 里 / 它怎么还在仓库里 | 第 10 节（`git rm --cached` 与历史残留） |
| 我的文件「不见了」，是不是被 Git 吃了 | 第 11 节（`status --ignored`、clean 的 dry-run） |

---

## 心智模型

### 图 1：一次 stash = 三个父亲 + 一根指针

这是第 1 节用真实对象验证过的形状（图本身来自 demo 的输出）：

```
      stash 的物理形状（三个普通 commit + 一根指着它的 ref）：

      .git/refs/stash ─── 文本文件，写着下面这个 commit 的 hash
             │
             ▼
      stash@{0}  commit "On main: 登录页改到一半，先让开位置"
        ├─ ^1 ── 你 stash 时 HEAD 所在的提交          ← 干净基线（参照物）
        ├─ ^2 ── index on main: ...                  ← 暂存区长什么样
        └─ ^3 ── untracked files on main: ...        ← 未跟踪文件（要 -u / -a 才有）

      .git/logs/refs/stash ── 栈的流水账（git stash list 读的就是它）
```

看懂这张图，本章一半的问题都不用背：「stash 的东西在哪」——在对象库里，是一个 commit；
「pop 为什么会冲突」——拿 `^1` 当参照物做三方合并；「误删的 stash 为什么能捞回来」——
drop 只是不动 ref 和流水账，对象本身 Git 从没删过。

### 图 2：apply / pop / drop / clear 各动谁

| 动作 | 工作区 | 暂存区 | 栈里那条 | 失败时 |
|---|---|---|---|---|
| `apply` | 恢复改动（默认全按未暂存放回） | 不动 | **还在** | 不影响栈 |
| `apply --index` | 恢复改动 | **连暂存状态一起还原**（MM 形状回来） | 还在 | 不影响栈 |
| `pop` | 恢复改动 | 同 apply（要暂存状态用 `pop --index`） | **成功后才删** | ★ 不删，留着并提示 kept |
| `drop` | 不动 | 不动 | 删（打印 hash！） | 空栈时报错、啥也不丢 |
| `clear` | 不动 | 不动 | 整栈清空（连 ref 带流水账） | 同上，hash 是唯一后悔药 |

判据只有一句：**拿不准就 apply，确认了再 drop。** pop 是「apply + drop」的快捷键，它省的那一步恰恰是不可逆的那一步。

### 图 3：.gitignore 是过滤器，不是删除键

```
   文件在磁盘上 ──▶ Git 看 status 时问一句：它被跟踪了吗？
                        │
        是（在索引里）  │  否（未跟踪）
            ▼           │           ▼
     .gitignore 管不着   │   逐层查忽略规则（子目录 .gitignore
     改它照样被报出来     │   → 仓库根 → exclude → 全局），
     正解是 rm --cached  │   命中「忽略」→ status 里隐身（!!）
                        │   没命中 → 出现在 ?? 里
```

四层规则谁赢是**实测**出来的（8E）：子目录 `.gitignore` > 仓库根 `.gitignore` > `.git/info/exclude` > 全局 `core.excludesFile`。
这个顺序和很多人的直觉相反——仓库私有的 exclude 并不比团队的 `.gitignore` 大，详见 8C。

---

## 第 1 节 stash 到底是什么：一根 ref + 三个 commit，全部拍在桌上给你看

**场景**：你在改登录页，a.txt 改了没 add、b.txt 改了并且 add 过、还新建了一个未跟踪的 pay-draft.txt。
现在要「把改动收起来」。收起来不是变魔术——先把 stash 之前的现场拍清楚，再看每个字节去了哪。

```bash
$ git init -q -b main '/tmp/git-handbook-06-stash-ignore-XhUYQQwQ/s1-anatomy'
[退出码 0]
  ↳ 现场：基线已提交；改了 2 个文件（其中一个 add 过）；还新建了 1 个未跟踪文件
  ↳ 已提交：42ab5f1 c1 基线：三个文件

$ git status --short
 M a.txt
M  b.txt
?? pay-draft.txt
[退出码 0]
```

逐行读这三行（02 章的两格符号学第一次实战）：`M a.txt` 的 M 在**第 2 格**——改了没 add，改动只在工作区，`git diff` 能看到它；
`M  b.txt` 的 M 在**第 1 格**——add 过了，改动进了暂存区，要用 `git diff --cached` 才看得见；
`?? pay-draft.txt` 未跟踪，两个 diff 都不归它管。这三行就是 stash 马上要「吃掉」的东西的全家福。
**为什么要分这么细**：stash 不是一个袋子把一切塞进去——它给工作区和暂存区**分别**拍快照，
你现在记住「哪个改动在第几格」，1F 就能对上号。

### 1B 存起来：唰地干净

```bash
$ git stash push -u -m '登录页改到一半，先让开位置'
Saved working directory and index state On main: 登录页改到一半，先让开位置
[退出码 0]

$ git status --short
[退出码 0]

$ git stash list
stash@{0}: On main: 登录页改到一半，先让开位置
[退出码 0]
```

`Saved working directory **and index** state`——提示语自己就说了：工作区和暂存区**都**被存了。
之后 status 是空的：连 pay-draft.txt 也不见了（`-u` 的功劳，第 4 节专讲）。
★ 这不是「藏起来」的假象：你的改动被做成了**真的 Git 对象**。`git stash list` 里 `stash@{0}` 读作「栈里第 0 条」，也就是最后存的那条。

### 1D 物理证据一：refs/stash 就是一个 41 字节的文本文件

demo 在这里跑了 `ls -l .git/refs`，屏幕上能看见 `stash` 是个**只有 41 字节的普通文件**，和 `heads/` 目录并排躺着
（属主列是本机用户名，成书略去不贴）。41 字节 = 40 位哈希 + 一个换行——和 01 章的分支指针**一模一样**的东西。直接看内容：

```bash
$ cat .git/refs/stash
257543fd962826212b2c9ccd4dc63c31cc251e2b
[退出码 0]

$ git cat-file -t '257543fd962826212b2c9ccd4dc63c31cc251e2b'
commit
[退出码 0]
```

`cat-file -t` 回答 `commit`——**stash 指向的是一个提交对象**，不是什么魔法缓冲区。
区别只有一个：分支指针在 `refs/heads/` 下、指向「这条线的最新提交」；`refs/stash` 在 `refs/` 根下、指向「栈顶那条 stash」。

### 1E 物理证据二：stash@{0} 本身是一个 commit，而且不止一个父亲

```bash
$ git log -1 --format='hash：%H%n父亲们：%P%n说明：%s' 'stash@{0}'
hash：257543fd962826212b2c9ccd4dc63c31cc251e2b
父亲们：42ab5f15be45a1d51bf3f75917cca219710834e4 5f4c9aaab5be3fc7b062cc5acca06581baadacf6 b021aea3e3136c5d1c1d20d4af4e2aaf77f9976c
说明：On main: 登录页改到一半，先让开位置
[退出码 0]
```

`%P` 打印所有父亲——这一行出现了**三个**哈希，说明这是「多亲提交」。demo 接着跑了 `git show --stat 'stash@{0}'`：
输出头部一行 `Merge: 42ab5f1 5f4c9aa b021aea`——三个短哈希正是上面 `%P` 的三兄弟；stat 是 `a.txt | 1 +` 加 `b.txt | 1 +`——
**这个 commit 的 tree 就是你 stash 那一刻【工作区】的样子**：「a 没 add、b add 过」这个区分在这一层被合并成
「工作区相对基线的总改动」。

### 1F 三个父亲分别是谁（本节最该记住的三行）

```bash
$ git show --stat 'stash@{0}^'
commit 42ab5f15be45a1d51bf3f75917cca219710834e4
Author: 学习者 <learn@example.com>
Date:   Thu Jan 1 10:01:00 2026 +0800

    c1 基线：三个文件

 README.md | 1 +
 a.txt     | 1 +
 b.txt     | 1 +
 3 files changed, 3 insertions(+)
[退出码 0]

$ git show --stat 'stash@{0}^3'
commit b021aea3e3136c5d1c1d20d4af4e2aaf77f9976c
Author: 学习者 <learn@example.com>
Date:   Thu Jan 1 10:01:00 2026 +0800

    untracked files on main: 42ab5f1 c1 基线：三个文件

 pay-draft.txt | 1 +
 1 file changed, 1 insertion(+)
[退出码 0]
```

三行总结（背下来不如理解）：**^1 参照物，^2 暂存区，^3 未跟踪。**
- `^1`（写 `^` 不带数字就是 `^1`）= 你 stash 时 HEAD 所在的提交——干净基线。pop 时 git 就是拿它当参照物做三方合并
  （第 3 节冲突标记的两端，分别对应「基线动过的结果」和「stash 里的改动」）。
- `^2` = 暂存区快照（demo 单独对它跑过 `git show --stat 'stash@{0}^2'`：说明行是
  `index on main: 42ab5f1 c1 基线：三个文件`，stat 里只有 b.txt——正是 1A 里 add 过的那个。
  **「add 过的东西一点不丢」的证据就在这**；2D 还会再见一次 ^2）。
- `^3` 的说明行是 `untracked files on main: ...`，里面住着 pay-draft.txt。**只有带 `-u`/`-a` 时才有这个父亲**（第 4 节对比）。

### 1G 栈的流水账：git stash list 其实就是 git log -g refs/stash

```bash
$ git log -g stash --oneline
257543f stash@{0}: On main: 登录页改到一半，先让开位置
[退出码 0]
```

`-g` 读 reflog。对 stash 这根 ref 用 `log -g`，得到的就是 `git stash list`——两者不是「碰巧长得像」，是**同一份数据的两种呈现**。
原始文件 `.git/logs/refs/stash` 每行四个字段：旧 hash、新 hash、身份和时间、最后是你那句 `-m` 说明
（demo 里当场 `cat` 了这份文件，首条记录以 40 个 0 开头表示「从无到有」）。**栈的顺序就是靠这个文件维持的**——
这也预告第 7 节的坑：`clear` 把 ref 和流水账一起抹掉时，`stash@{n}` 这套坐标整个失效。

### 1H 不 pop 也能先看内容：git stash show

demo 跑了 `git stash show --stat`，输出正是 ` a.txt | 1 +`、` b.txt | 1 +` 加一行总计。
`show` 就是「拿 stash 的 tree 和 `^1` 比 diff」，不放回改动也能审阅。不带参数默认看 `stash@{0}`，看别的条目：
`git stash show -p 'stash@{1}'`。demo 还跑了 `git stash show -p`，a、b 两个文件的改动都列了出来——
再次印证：这一层是「工作区总改动」，**不再区分 add 与否**（区分在 ^2 快照里）。

**常见翻车**：把 stash 当剪贴板，担心「东西被哪个程序同步走了」。它就是本仓库 `.git` 里的对象，只在这台机器的这个仓库里
（「stash 不随 push 走」这句常言本章没有实测，但由「对象就在 .git 里」可直接推出——6B 的图上 `refs/stash` 挂在本地，远程交互是 07 章的事）。

---

## 第 2 节 apply 和 pop 差一个字：pop 会顺手把栈里那条删掉

**场景**：干净小仓库（基线 `7d376b3 c1 基线`，三个已跟踪文件，先只改一个），存一条 stash，
把 apply 和 pop 的全部区别（含手滑现场）一次演完。

```bash
$ git init -q -b main '/tmp/git-handbook-06-stash-ignore-XhUYQQwQ/s2-apply-pop'
[退出码 0]
  ↳ 现场：三个已跟踪文件，先只改一个
  ↳ 已提交：7d376b3 c1 基线

$ git stash push -m '先只改一半'
Saved working directory and index state On main: 先只改一半
[退出码 0]
```

### 2A apply：改动回到工作区，栈里那条【还在】

apply 的输出就是帮你跑了一遍 `git status`（`On branch main` + `Changes not staged for commit:` 那一段，
明细行 `\tmodified:   base.txt`；demo 另跑了一次 `git status --short` 确认是 ` M base.txt`）；而 **`stash list` 里那条还在**。
同一份改动此刻同时存在于工作区和栈里——所以 **apply 是安全的那个**：弄错了还能重来，栈里永远有一份底。

### 2A-2 手滑现场：apply 完又 pop，被 Git 当场拒绝

新手最常走的一步棋：apply 完忘了自己 apply 过，又把终端历史里的 `git stash pop` 翻出来敲。

```bash
$ git stash pop
error: Your local changes to the following files would be overwritten by merge:
	base.txt
Please commit your changes or stash them before you merge.
Aborting
On branch main
Changes not staged for commit:
  (use "git add <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)
	modified:   base.txt

no changes added to commit (use "git add" and/or "git commit -a")
The stash entry is kept in case you need it again.
[退出码 1]  ★ 上面这段失败是本章故意让你看的
```

逐行读：`error: Your local changes ... would be overwritten by merge`——工作区里已经有一份**和 stash 内容相同的改动**
（刚才 apply 留下的），pop 再叠一遍会把它们盖掉，Git 拒绝开工；`Aborting` 之后的 status 是现场快照；
★ 最后一行 `The stash entry is kept in case you need it again.`——**pop 失败不吞东西**。这是「保命条款」的第一次亮相，第 3 节整节展开。

### 2B pop：先把工作区退回干净，再看 pop 真正做的两件事

```bash
$ git restore base.txt
[退出码 0]

$ git stash pop
On branch main
Changes not staged for commit:
  (use "git add <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)
	modified:   base.txt

no changes added to commit (use "git add" and/or "git commit -a")
Dropped refs/stash@{0} (e40184285afbd64e16345e1f8cb0cf360e801214)
[退出码 0]

$ git stash list
[退出码 0]
```

先用 `git restore` 把工作区退干净（2A-2 的拒绝就发生在「工作区不干净」时）。
成功 pop 的输出 = apply 的那坨 status（上面这屏）+ 一行 `Dropped refs/stash@{0} (<完整 hash>)`；最后 list 空了。
**pop = apply + drop，一条命令两步。** ★ 请养成习惯扫一眼 Dropped 括号里的完整哈希（本次是 `e401842...`）——
第 7 节你会知道，这是 drop 类动作唯一的后悔药。

### 2C 空栈上再 pop / drop：手滑按第二次上箭头时的画面

```bash
$ git stash pop
No stash entries found.
[退出码 1]  ★ 上面这段失败是本章故意让你看的

$ git stash apply stash@{3}
error: stash@{3} is not a valid reference
[退出码 1]  ★ 上面这段失败是本章故意让你看的
```

（demo 还用同样方式试了 `git stash drop`，提示一样是 `No stash entries found.`。）
三条都退出码 1，但**什么都没丢**——栈本来就是空的，空栈上 pop/drop 无害，别慌。
第三条顺便立规矩：`stash@{n}` 的 n 是**栈内位置**，越界直接 `is not a valid reference`。
和第 7 节对照：裸哈希在这里什么都不是，`stash@{n}` 才是 pop/apply 认的坐标。

### 2D 进阶：暂存区也想原样回来，用 apply --index

换个现场：f1.txt 先改了一版并 add，工作区又改了第二版——02 章的 `MM` 形状。

```bash
$ git status --short
MM f1.txt
 M f2.txt
[退出码 0]
  ↳ MM f1.txt = 暂存区里存着一版、工作区又改了一版。stash 会怎么收这种现场？

$ git stash push -m '暂存区和工作区各有一版'
Saved working directory and index state On main: 暂存区和工作区各有一版
[退出码 0]

$ git show --stat 'stash@{0}^2'
commit dd656aaa700121ccc409f990532901f9a7e53ec0
Author: 学习者 <learn@example.com>
Date:   Thu Jan 1 10:02:00 2026 +0800

    index on main: 7d376b3 c1 基线

 f1.txt | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)
[退出码 0]
```

`^2` 那份快照里只有 f1 的**第一版**（add 过的那版）——**「暂存区也被一起存下来了」的铁证**，1F 的 `^2 = 暂存区` 完成第二次验证。

**apply 默认【不】还原暂存状态**：demo 接着跑 `git stash apply`，`git status --short` 变成两行 ` M f1.txt`、` M f2.txt`——
`MM` 被抹平成全是「M 在第 2 格」。内容一点没丢（工作区那版都在），丢的只是「哪些改动在暂存区」这个**布局**。

**换成 `--index` 就原样回来**：先 `git restore .` 退干净，再 `git stash apply --index`，这次输出里
`Changes to be committed`（f1.txt）和 `Changes not staged for commit`（f1.txt + f2.txt）两段同时出现，
status 回到 `MM f1.txt` / ` M f2.txt`。**要连暂存状态一起恢复就得带 `--index`；pop 同理 `git stash pop --index`**
（后者是 demo 注释给的规则，本章没单独实跑）。

**常见翻车**：apply 完又 pop（2A-2，好在 kept 条款兜底）；精心排好的 MM 布局被 apply 默认抹平（2D-1，用 `--index`）；
把 `stash@{3}` 当「第 3 次 push 的」（它是位置不是编号，越界报错，2C）。

---

## 第 3 节 pop 撞车现场：★ stash 不会被丢弃（保命条款，务必看完）

**场景**：你改了 app.txt 第 2 行并 stash（基线 `537f12f c1 基线`），然后同一行又被「上游」提交了新版本
（`447e8d7 c2 上游改了第 2 行`——04 章的 fetch 场景，这里用本地提交直接模拟，不联网）。现在 pop。

```bash
$ git init -q -b main '/tmp/git-handbook-06-stash-ignore-XhUYQQwQ/s3-conflict'
[退出码 0]
  ↳ 现场：你改了第 2 行并 stash，然后同一行又被上游提交了新版本
  ↳ 已提交：537f12f c1 基线

$ git stash push -m '本地实验先收起来'
Saved working directory and index state On main: 本地实验先收起来
[退出码 0]
  ↳ 已提交：447e8d7 c2 上游改了第 2 行
```

### 3A pop：内容冲突 —— git 把栈里那条留着了

```bash
$ git stash pop
Auto-merging app.txt
CONFLICT (content): Merge conflict in app.txt
On branch main
Unmerged paths:
  (use "git restore --staged <file>..." to unstage)
  (use "git add <file>..." to mark resolution)
	both modified:   app.txt

no changes added to commit (use "git add" and/or "git commit -a")
The stash entry is kept in case you need it again.
[退出码 1]  ★ 上面这段失败是本章故意让你看的

$ git stash list
stash@{0}: On main: 本地实验先收起来
[退出码 0]
```

★★ 本章最重要的一屏。`Auto-merging` + `CONFLICT (content)` 说明 pop 内部做的是一个**真正的三方合并**（04 章的词汇全部复用；
参照物就是 1F 的 `^1`）。而最后一句 **`The stash entry is kept in case you need it again.`** 加上复查的 `git stash list`：那条还在。

> 请记住这句英文：**The stash entry is kept in case you need it again.**
> pop 失败时 Git 不会吞掉你的东西。这就是「pop 撞冲突」和「pop 成功」唯一的区别——
> 成功 → `Dropped ...`；失败 → 内容已经摊在你工作区（带冲突标记），栈里那条原封不动。

### 3B 冲突现场长这样：标记两端都写着来源

```bash
$ git status --short
UU app.txt
[退出码 0]

$ cat app.txt
line1
<<<<<<< Updated upstream
line2 上游把这一行改了
=======
line2 我改的（本地实验）
>>>>>>> Stashed changes
line3
line4
[退出码 0]
```

`UU` = 未解决冲突。打开文件，标记和 04 章的合并冲突**长得不一样**：不是分支名，而是
`<<<<<<< Updated upstream`——你现在脚下这份内容（已叠上 c2 的 HEAD 工作区）；
`>>>>>>> Stashed changes`——stash 里那份。对照 1F：三方合并的参照物是 `^1`，
「Updated upstream」是基线动过的结果，「Stashed changes」是你存进去的改动。

### 3C 千万别在没解决之前再 pop 一次

demo 在冲突没解决时又敲了一次 `git stash pop`：`error: could not write index` + `app.txt: needs merge`，退出码 1，
末尾照旧 kept；换 `git stash apply` 一样被拒。**工作区还带着未完成的合并（04 章的多 stage 暂存区状态），索引写不动，先解决，别硬来。**
好消息从坏消息里透出来：kept 条款怎么折腾都丢不了栈。

### 3D 出路一：整份文件取 stash 那一侧 → add → 确认后再 drop

```bash
$ git checkout --theirs app.txt
Updated 1 path from the index
[退出码 0]

$ cat app.txt
line1
line2 我改的（本地实验）
line3
line4
[退出码 0]
```

整份文件都要 stash 那一版时，`git checkout --theirs` 从暂存区取「theirs 侧」。在 stash 现场，**theirs = Stashed changes**
（对照 3B 的标记；和 04 章「<<<<<<< HEAD 那半是 ours」同一套语言，换了个名字而已）。
`git add app.txt` 把文件报告为已解决——demo 复查 status，`UU` 变 `M  app.txt`，04 章冲突收尾的同一个机制。
此刻**改动已经在工作区，栈里那条只是多余副本**——demo 确认无误后才跑 `git stash drop`，
屏幕上照例留下 `Dropped refs/stash@{0} (dbad6943cd70728575d3b84ba0ffa9816516fe4f)`，list 归零。
注意顺序：先取回、先 add、先核对内容（`cat app.txt` 那步不是走过场），**最后**才 drop。

### 3E 出路二：退回干净状态，把 stash 留在栈里改天再说

今天不想处理了。demo 再演一次撞车（`git stash push -m '再收一次'` → `git stash pop` → 同样的 CONFLICT + kept），
然后按正确顺序全身而退：

```bash
$ git stash list
stash@{0}: On main: 再收一次
[退出码 0]
  ↳ 先看这条 stash list：东西还在。【确认还在之后】才谈清理工作区：

$ git reset -q --hard HEAD
[退出码 0]

$ git status --short
[退出码 0]

$ git stash list
stash@{0}: On main: 再收一次
[退出码 0]
```

★ 顺序很重要：**先确认 stash 还在，再 `reset --hard`**。反过来就是自己砸自己（03 章讲过 reset --hard 砸的是工作区+暂存区，
它够不着栈；但如果你连栈里那份也清过，就真没了）。清完之后工作区干净、栈里那条完好，改天在任何分支上再处理。
另一条更稳的路是 6D 的 `git stash branch`——回到 stash 自己的基线上再解决冲突，比在别的分支上硬 pop 舒服得多。

### 3F 另一种翻车：未跟踪文件撞名，pop 直接不干活

现场：栈里是一条带 `-u` 的 stash，里面有个未跟踪的 draft.txt；而工作区此刻已经有一个同名 draft.txt（别的流程新建的）。

```bash
$ git stash pop
Already up to date.
draft.txt already exists, no checkout
error: could not restore untracked files from stash
On branch main
Untracked files:
  (use "git add <file>..." to include in what will be committed)
	draft.txt

nothing added to commit but untracked files present (use "git add" to track)
The stash entry is kept in case you need it again.
[退出码 1]  ★ 上面这段失败是本章故意让你看的
```

逐行读：`Already up to date.` 是已跟踪部分没啥可合的；真正的问题在
`draft.txt already exists, no checkout` + `error: could not restore untracked files from stash`——
**Git 不肯覆盖工作区那个同名文件，整个 apply 直接失败**。`cat` 证明磁盘上那份还是「别人新建的同名文件」，
你 stash 里的那份没落盘；而 kept 条款第三次出现：demo 复查 `git stash list`，那条仍在。
解法：先把工作区那个 draft.txt 改名或删掉（确认它不是你要的那份！），再 pop。
这个坑和第 4 节 `-a` 的风险连着：把不属于版本控制的文件卷进 stash 再卷出来，撞名概率最高。

---

## 第 4 节 -u 和 -a：默认只收「Git 认识的文件」

**场景**：一个仓库同时有四种文件——已跟踪且改过（src.txt）、未跟踪新文件（scratch.sh）、
被 .gitignore 忽略的产物（build/、run.log）。看 stash 在不同参数下各收谁。基线 `a535e10 c1 基线（含 .gitignore）`。

### 4A 普通 stash：未跟踪的和被忽略的都留在原地

```bash
$ git status --short
 M src.txt
?? scratch.sh
[退出码 0]

$ git stash push -m '只收已跟踪的'
Saved working directory and index state On main: 只收已跟踪的
[退出码 0]

$ git status --short
?? scratch.sh
[退出码 0]
```

对比上面两屏：`?? scratch.sh` 纹丝不动。stash 默认只管 **Git 认识的文件**（已跟踪的）；被忽略的 build/、run.log 同理不动
（它们连 status 都不展示）。这意味着「stash 完就切分支」有暗雷：scratch.sh 还躺在工作区，切分支它跟着你走——
第 6 节的正确流程用 `-u` 正是为了这个。

### 4B 反例：只剩未跟踪文件时，普通 stash 什么都不做（但退出码是 0）

```bash
$ git stash push -m '我以为收走了'
No local changes to save
[退出码 0]

$ git stash list
[退出码 0]
```

★★ 这一屏是给所有写脚本的人看的：已跟踪文件全干净、只剩未跟踪文件时，`git stash push` 打印
`No local changes to save`，**退出码却是 0**，而 list 是空的——**压根没存**。
**脚本里别拿退出码判断「stash 成功了吗」**，要判断就去看 list / 看 status。交互使用也一样：
以为收走了就开始切分支、改文件，等发现时现场已经混了。

### 4C -u：连未跟踪文件一起收走（第三个父亲就是这么来的）

```bash
$ git stash push -u -m '连未跟踪一起收'
Saved working directory and index state On main: 连未跟踪一起收
[退出码 0]

$ git status --short
[退出码 0]

$ ls -1
build
run.log
src.txt
[退出码 0]

$ git log -1 --format='父亲们：%p' 'stash@{0}'
父亲们：a535e10 a3f7bc0 fe8fb9e
[退出码 0]

$ git show --stat 'stash@{0}^3'
commit fe8fb9ecbc9bd744c1d2399737c1f1e51310f929
Author: 学习者 <learn@example.com>
Date:   Thu Jan 1 10:05:00 2026 +0800

    untracked files on main: a535e10 c1 基线（含 .gitignore）

 scratch.sh | 1 +
 1 file changed, 1 insertion(+)
[退出码 0]
```

`-u`（--include-untracked）之后，scratch.sh **从磁盘上消失了**（`ls -1` 里再没有它）。它没被删：
它被写进了 `stash@{0}^3` 那个对象里。当场验血统：三个父亲齐了（基线 a535e10 + index 快照 a3f7bc0 + untracked 快照 fe8fb9e），
而 `^3` 的 stat 正是 scratch.sh——1F 那张 ASCII 图的第三次实物对照。**不带 `-u` 时 stash commit 只有两个父亲**，
这条可以拿 4A 那次 push 的自己数（demo 没单独跑，结论来自第 1 节 ^3 只在 -u/-a 下存在的实测）。

### 4D -a：连被 .gitignore 掉的也一起收（build/ 和 run.log 都没了）

```bash
$ git stash push -a -m '连 gitignore 掉的也收'
Saved working directory and index state On main: 连 gitignore 掉的也收
[退出码 0]

$ ls -1
src.txt
[退出码 0]

$ git show --stat 'stash@{0}^3'
commit 1b952a3f4a2edec726f7438e68fb766a79c4ad40
Author: 学习者 <learn@example.com>
Date:   Thu Jan 1 10:05:00 2026 +0800

    untracked files on main: a535e10 c1 基线（含 .gitignore）

 build/app.o | 1 +
 run.log     | 1 +
 scratch.sh  | 1 +
 3 files changed, 3 insertions(+)
[退出码 0]
```

`-a`（--all）把被忽略的编译产物也塞进 `^3`：`ls -1` 只剩 src.txt，build/ 和 run.log 都进了对象库。
demo 对它的判语值得原样引用：**「这个选项用处小、风险大：回来时容易和你重新产出的文件撞名（见 3F）」。**
最后 demo 把这条 pop 回来，`ls -1` 里四样全在（`build run.log scratch.sh src.txt`）——东西确实没丢，只是换了个地方住。
（怎么把被忽略这批隐身文件揪出来看是 `git status --ignored`，8C/11A 细讲，那里能看到 `!!` 前缀的名单。）

**常见翻车**：默认 stash 不收未跟踪 → 切分支时被 scratch 文件跟过来（4A，用 `-u`）；
退出码 0 骗过脚本（4B，信 list 不信退出码）；无脑 `-a` → pop 时和新生成的产物撞名（3F 机制），还白白把大文件塞进 .git。

---

## 第 5 节 只想收其中几个文件：git stash push -- <路径>

**场景**：同时改着 app.js、model.js、view.js（外加未跟踪的 extra.conf），但只想给其中两个「让开位置」。基线 `bcbded0 c1 三个文件`。

### 5A -- 后面列路径：只收 app.js 和 model.js

```bash
$ git stash push -m '只收 app 和 model' -- app.js model.js
Saved working directory and index state On main: 只收 app 和 model
[退出码 0]

$ git status --short
 M view.js
?? extra.conf
[退出码 0]
```

`--` 是命令与路径的分界（git 通用约定：`--` 之后全是路径，不再被当成选项）。
view.js 的改动和 extra.conf 都还在原地——这就是「部分让位」。demo 用 `git stash show --stat` 也印证：
栈里这条只含 app/model（` app.js   | 2 +-`、` model.js | 2 +-` 两行）。

### 5B 路径 + -u：把某个未跟踪文件也一起带走

```bash
$ git stash push -u -m '把 extra.conf 和 view.js 带走' -- extra.conf view.js
Saved working directory and index state On main: 把 extra.conf 和 view.js 带走
[退出码 0]

$ git stash list
stash@{0}: On main: 把 extra.conf 和 view.js 带走
stash@{1}: On main: 只收 app 和 model
[退出码 0]
```

`-u` 叠加路径：连未跟踪的 extra.conf 也能点名带走，工作区彻底干净。现在栈里有两条。
**编号从栈顶往下数，每多 push 一条全都往后挪一格**——所以跨条目操作（apply/drop `stash@{1}`）前
先 `git stash list` 对一眼再动手；demo 就是用 `git stash show --stat 'stash@{1}'` 先验了 5A 那条的内容才继续的。

### 5C 先把两条都取回来、落一条提交，再演示按目录收

demo 在这里把两条 stash 依次 pop、把工作区提交成 `2818299 c2 上面这些改动我先提交掉`，又造了
`d3e9d2e c3 新增 lib 目录`（中间输出静音，为的是给新改动腾地方），现场变成 lib 里两个文件加 view.js 都有改动。然后：

```bash
$ git stash push -m '整个 lib 目录先让开' -- lib/
Saved working directory and index state On main: 整个 lib 目录先让开
[退出码 0]

$ git status --short
 M view.js
[退出码 0]
```

路径参数可以是**目录**：`-- lib/` 把 lib 下所有已跟踪文件的改动一起收走，view.js 不受影响。
（想带走未跟踪文件要在路径之外再配上 `-u`，如 5B 的组合——本章没演示「只给路径不加 -u 点名未跟踪文件」的写法。）

### 5D --keep-index：暂存区留着，把工作区那部分收走

```bash
$ git status --short
MM app.js
 M lib/io.js
 M lib/util.js
 M view.js
[退出码 0]

$ git stash push --keep-index -m '只把工作区那部分收走'
Saved working directory and index state On main: 只把工作区那部分收走
[退出码 0]

$ git status --short
M  app.js
[退出码 0]
```

现场 app.js 是 `MM`（暂存区一版 A3、工作区又改一版 A4）。`--keep-index` 的效果：A3 留在原地（`M  app.js`，M 在第 1 格），
A4 进了 stash。典型用法是「拿暂存区这份干干净净地跑一遍测试再提交」。
★ 但最后一屏要瞪大眼睛：只剩 `M  app.js`——**lib/ 和 view.js 那些没暂存的改动也被一并收走了**。
demo 原话：**`--keep-index` 只是「暂存区那份别动」，不是「只 stash 这个文件」；只挑文件要用 `-- <路径>`。**
两个维度互不干扰：`--` 管「收哪些文件」，`--keep-index` 管「暂存区动不动」。

---

## 第 6 节 完整流程：半成品在手，线上着火

**场景**：你在 feature/pay 上改到一半（pay.js 已 add、login.js 改了没 add、reconcile.sh 未跟踪——三种状态全齐），
线上出 bug 必须马上切回 main 修。整章所有零件的一次总装配。基线 `f0623e8 c1 项目起步`。

```bash
$ git status --short
 M login.js
A  pay.js
?? reconcile.sh
[退出码 0]
```

### 6A 第一步：全部收干净（用 -u，那个未跟踪脚本也得带走）

```bash
$ git stash push -u -m 'feature/pay 半成品：支付对账'
Saved working directory and index state On feature/pay: feature/pay 半成品：支付对账
[退出码 0]

$ git stash list
stash@{0}: On feature/pay: feature/pay 半成品：支付对账
[退出码 0]
```

**工作区干净了才谈得上安全切分支。** 03 章的 `reset --hard` 也能清空，但它把未提交的改动**真砸掉**；
stash 是挪开，不是砸掉（第 1 节的对象证据摆在那）。`-m` 写清楚「哪条线 + 半成品是啥」——
将来 `stash list` 里一排 On 记录，全靠这行说明认人。

### 6B 第二步：切回 main，开 hotfix 修线上

demo 先 `git switch main`（屏幕回执 `Switched to branch 'main'`，复查 `git status --short` 全空），
在 hotfix/order 上提交 `ac35a2e hotfix: 修超时配置`，切回 main 收下修复成 `e426e5a c2 main 收下这次修复`。
现在看全局形状：

```bash
$ git log --oneline --decorate --graph --all
* e426e5a (HEAD -> main) c2 main 收下这次修复
| * ac35a2e (hotfix/order) hotfix: 修超时配置
|/  
| *   183f0a3 (refs/stash) On feature/pay: feature/pay 半成品：支付对账
|/|\  
| | * 7cbe11d untracked files on feature/pay: f0623e8 c1 项目起步
| * 1ebbfb2 index on feature/pay: f0623e8 c1 项目起步
|/  
* f0623e8 (feature/pay) c1 项目起步
[退出码 0]
```

★★ 这张图是本章最值钱的一屏：`refs/stash` 那一坨就是第 1 节 ASCII 图的**实物版**——
一个 merge commit（看 `|/|\` 那行的三条边），下面挂着 index 和 untracked files 两个快照，根上连着 feature/pay 那条线。
逐行指认：`183f0a3 (refs/stash)` 是栈顶本体；`1ebbfb2 index on feature/pay: f0623e8` 是 ^2；
`7cbe11d untracked files on feature/pay: f0623e8` 是 ^3（reconcile.sh 就住在里面）；^1 直接画在图上——feature/pay 指着的 f0623e8。
而此刻你人在 main，修完 hotfix，**半成品安安静静挂在对象库里，谁也没妨碍**。

### 6C 第三步：回到 feature/pay，把半成品取回来

```bash
$ git stash pop
On branch feature/pay
Changes to be committed:
  (use "git restore --staged <file>..." to unstage)
	new file:   pay.js

Changes not staged for commit:
  (use "git add <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)
	modified:   login.js

Untracked files:
  (use "git add <file>..." to include in what will be committed)
	reconcile.sh

Dropped refs/stash@{0} (183f0a3192374e90f25c583c57a71083598edcff)
[退出码 0]
```

对照 6A 之前那一屏：` M login.js`、`A  pay.js`、`?? reconcile.sh` **一模一样地回来了**（demo 复查 status 原样列出这三行）。
改动回来、stash 清空（Dropped + list 空），接着干。整条路上你没弄脏任何一条提交。
demo 在这里补了一条观察，值得多想十秒钟：pop 之后 pay.js 还保持在暂存区（A 在第 1 格），
**因为 stash 里 `^2` 那份快照记得它 add 过**。而 2D-1 演示过 apply 默认抹平暂存状态——
这一处没抹平、那一处抹平，差别细节本章不展开；**操作规则只记一条：在乎暂存区布局就显式用
`apply --index` / `pop --index`（2D-2 实测过 MM 原样还原），不在乎就怎么都行。**

### 6D 加分招：git stash branch —— 这个半成品其实该另开一条线做

存了很久的 stash、基线早就物是人非的 stash，硬 pop 会撞一堆冲突（第 3 节的痛苦）。Git 备了一条正路：
在 **stash 自己的基线**上开新分支来处理。demo 先把 refactor.md 单独 stash（`git stash push -u -m '这块要单开分支慢慢做' -- refactor.md`），
确认 HEAD 还在 `f0623e8 c1 项目起步`，然后一条命令：

```bash
$ git stash branch wip/refactor 'stash@{0}'
Switched to a new branch 'wip/refactor'
M	login.js
A	pay.js
On branch wip/refactor

Dropped stash@{0} (c61f98bdf091c6877033500a9dba0301b2099c98)
[退出码 0]
```

（本屏略去了 `On branch wip/refactor` 之后的 status 长格式三段，capture 里完整：`new file: pay.js` 在暂存段、
`modified: login.js` 在未暂存段、`reconcile.sh` 和 **refactor.md** 在未跟踪段。）

`git stash branch <新分支名> <stash>` 一条命令干四件事（demo 注释原样拆解）：
**用这条 stash 的基线（^1）建分支、切过去、apply、再 drop。**
输出怎么读：`Switched to a new branch 'wip/refactor'` 是建+切；紧跟着裸列表的 `M	login.js` / `A	pay.js`
是换分支时把没带走的改动**列出来给你看**；随后跟上的 status 长格式（上屏已略，见注）里，注意 `?? refactor.md`（这条 stash 的主体）
已和 reconcile.sh 一起回到未跟踪段；最后 `Dropped stash@{0} (c61f98b...)` 收尾。
demo 复查 `git branch`：`* wip/refactor` 已在 feature/pay、hotfix/order、main 之旁多出来，`git stash list` 空了。
**「在它自己的起点上解决冲突，比在别的分支上硬 pop 舒服得多」**——长期抗战用它。

---

## 第 7 节 drop / clear 之后：用 git fsck --unreachable 把 stash 捞回来

**场景**：你手滑 `git stash clear`，一整条重要的 stash 从列表里消失了。本章把它演成一部三集救援片。
先立主角：基线 `b75587c c1 基线`，主角 stash 是 `c908c3d4f4b8615880f9ee99d154c6b3b4034047`（★★★ 一天的活儿）。

```bash
$ git stash push -u -m '★★★ 一天的活儿，千万别丢'
Saved working directory and index state On main: ★★★ 一天的活儿，千万别丢
[退出码 0]
  ↳ 先记下栈顶 hash（正常开发时不用记，drop 那一行会打印给你）：c908c3d4f4b8615880f9ee99d154c6b3b4034047
```

### 7A 先看 drop 这个动作：它会打印 hash，那是你唯一的后悔药

demo 又压了一条「第二条：稍后被误 drop」在栈顶（此时 list 里 `{0}` 是新的、`{1}` 是主角），然后：

```bash
$ git stash drop
Dropped refs/stash@{0} (e0faff76b6386d22abff51f0ce9a5bfab042f873)
[退出码 0]
```

`Dropped refs/stash@{0} (<完整 hash>)` 括号里那一串——**请立刻复制，别让它划走**。
drop 只是让 refs/stash 指向下一格；被丢的那条从「有 ref 指着」变成「没人引用」，**对象本身还在库里**（这是本节能救的根由）。

### 7B 然后手滑：clear 把整栈全清了

```bash
$ git stash list
stash@{0}: On main: ★★★ 一天的活儿，千万别丢
[退出码 0]

$ git stash clear
[退出码 0]

$ git stash list
[退出码 0]
```

列表空了、工作区也干净了，看起来一天白干了。别急着搜「git stash clear 恢复」，先按 03 章的条件反射试 reflog——然后撞上一次 fatal：

```bash
$ git reflog show stash
fatal: ambiguous argument 'stash': unknown revision or path not in the working tree.
Use '--' to separate paths from revisions, like this:
'git <command> [<revision>...] -- [<file>...]'
[退出码 128]  ★ 上面这段失败是本章故意让你看的
```

★ `clear` 干得比 drop 绝——**refs/stash 这根 ref 本身已经没了**，连 stash 的 reflog 都查不到（1G 的流水账随 ref 一起蒸发）。
这条路断了，换 fsck。

### 7C 先试一手：拿 hash 直接 pop —— 不行，这条要记牢

7A 那种「打印出来的完整 hash」总该能用吧：

```bash
$ git stash pop c908c3d4f4b8615880f9ee99d154c6b3b4034047
error: 'c908c3d4f4b8615880f9ee99d154c6b3b4034047' is not a stash reference
[退出码 1]  ★ 上面这段失败是本章故意让你看的
```

`error: '<hash>' is not a stash reference`——**pop / apply 参数里的 `stash@{n}` 是【reflog 位置】坐标，裸 hash 不算 stash 引用，pop 拒收。**
和第 2C 条（越界位置报错）是同一套规矩的两面。**但 apply 收 hash**——看 7F，先别慌。

### 7D 救援第一步：确认对象还在库里（Git 从没删过它，只是没人引用了）

```bash
$ git cat-file -t c908c3d4f4b8615880f9ee99d154c6b3b4034047
commit
[退出码 0]

$ git log -1 --format='%h %s' c908c3d4f4b8615880f9ee99d154c6b3b4034047
c908c3d On main: ★★★ 一天的活儿，千万别丢
[退出码 0]
```

`cat-file -t` 还在、`log -1` 能认。demo 还跑了完整的 `git show --stat c908c3d...`（头一行
`Merge: b75587c f69600c 7580b37`——**三父亲形状完好如初**）和 `git show --stat c908c3d...^3`：
未跟踪的 notes-day1.md 就躺在第三个父亲的快照里。这就是「drop ≠ 删除」的实物证明：Git 从没删过它，只是没人引用了。

### 7E 救援第二步：git fsck --unreachable 把没人引用的对象全列出来

hash 丢了（没复制 7A 那一行）怎么办？让 fsck 挨个点名：

```bash
$ git fsck --unreachable
unreachable blob 00b0852ce5f3022987738928713e50c3c8304ab8
unreachable commit e0faff76b6386d22abff51f0ce9a5bfab042f873
unreachable tree a8ab5f06ab3994f5c60bba2ef7c90813cdb6e476
unreachable commit c908c3d4f4b8615880f9ee99d154c6b3b4034047
unreachable blob 4b5d6eaa6ad28ef4e90b5c7980bf0e528a0a0422
unreachable tree 4e8a6c05377313930c80c93bb55c5ea4ca55a626
unreachable commit b0ef21239ec4687a798ae1681defbc53d94a909c
unreachable tree d0185dde4b0d8cc70b16513163f9543b267e85bb
unreachable commit 7580b37c0b3468ba4c6ec0153d0977004caa8f3a
unreachable commit f69600c93555a3db79a043e6ad7da3560b3ed388
unreachable blob dbe5fd73f52537ed87b607f1e111232b9b0ceb15
[退出码 0]
```

blob / tree / commit 三种混在一起，输出很长——**只盯 `unreachable commit` 那几行**。把 commit 挑出来、配上说明行认人：

```bash
$ git fsck --unreachable | grep "unreachable commit" | cut -d" " -f3 | sort | while read h; do echo "$h | $(git log -1 --format=%s "$h")"; done
7580b37c0b3468ba4c6ec0153d0977004caa8f3a | untracked files on main: b75587c c1 基线
b0ef21239ec4687a798ae1681defbc53d94a909c | untracked files on main: b75587c c1 基线
c908c3d4f4b8615880f9ee99d154c6b3b4034047 | On main: ★★★ 一天的活儿，千万别丢
e0faff76b6386d22abff51f0ce9a5bfab042f873 | On main: 第二条：稍后被误 drop
f69600c93555a3db79a043e6ad7da3560b3ed388 | index on main: b75587c c1 基线
[退出码 0]
```

看说明就能认出哪条是你的：`On main: ★★★ 一天的活儿，千万别丢`。
（`untracked files` / `index on` 说明的也是快照 commit——^2/^3 自己同样是普通 commit，1F 又说了一遍。）

```bash
$ git fsck
dangling commit e0faff76b6386d22abff51f0ce9a5bfab042f873
dangling commit c908c3d4f4b8615880f9ee99d154c6b3b4034047
[退出码 0]
```

不带参数的 `git fsck` 只报 **dangling commit**（两条被丢弃的「栈顶」都在：drop 掉的 e0faff76、clear 掉的 c908c3d），够认 hash；
`--unreachable` 连 ^2/^3 这些中间快照一起列，看得更全。认人时 dangling 更清爽。

### 7F 救援第三步：用 hash apply，改动整个回来

```bash
$ git stash apply c908c3d4f4b8615880f9ee99d154c6b3b4034047
On branch main
Changes not staged for commit:
  (use "git add <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)
	modified:   core.txt

Untracked files:
  (use "git add <file>..." to include in what will be committed)
	notes-day1.md

no changes added to commit (use "git add" and/or "git commit -a")
[退出码 0]

$ cat core.txt
写了整整一天的改动
[退出码 0]
```

已跟踪文件的改动（core.txt）+ 未跟踪文件（notes-day1.md，来自 ^3）**都回来了**（demo 用 `cat notes-day1.md`
验了内容是「同一天的未跟踪笔记」）。7C 那个 pop 的错不是因为 hash 错了，而是 **pop 只认 `stash@{n}` 坐标，apply 认 hash**。
救援口诀：**fsck 找 hash → apply 用 hash。**

### 7G 顺手把找回来的东西重新挂回栈里（可选）

demo 最后把工作区重新 `git stash push -u -m '找回来了，重新存一次'`，list 里就有了新的一条。
为什么不用 `git update-ref` 硬挂回 refs/stash？demo 给了答案：`git stash list` 读的是 `.git/logs/refs/stash` 那份流水账（1G！），
手工 update-ref 只改 ref、不改流水账，**list 里看不见它**。老老实实 push 一条新的最省心。

### 7H 边界说明（本章不真跑，03 章已经演示过）

```bash
  ↳ 上面整套救援的前提是「对象还没被 gc 真删」。
  ↳ 如果你已经跑过 git reflog expire --expire=now --all + git gc --prune=now，
  ↳ 或者 stash 的保留期过了（gc.reflogExpire，默认 90 天），那就是真没了，神仙难救。
  ↳ ★ 所以：stash 里有货的时候，永远不要顺手跑 gc。
```

90 天是默认值不是保险箱，gc 一跑物理清除。**stash 里有货的时候永远不要顺手跑 gc**——这条直接抄进肌肉记忆。

---

## 第 8 节 .gitignore 的四层来源：仓库级、目录级、仓库私有 exclude、还有全局那份

从这节开始换主角：stash 管「临时挪开」，.gitignore 管「永远别看」。
先交代一个安全事实（demo 在屏幕上也打了）：

> 本节讲到的『全局配置』只存在于沙箱假家目录（HOME 在脚本开头就被指到沙箱的 fakehome/）。
> 脚本里没有、也不会有任何一条 git config --global；你真实的 ~/.gitconfig 一个字节都没动。

「全局忽略」这一层照样演示了，用的那份假 `.gitconfig` 是 `cat` 手写的纯文本（连 `git config` 都没用），
里面只有一条 `core.excludesFile`。沙箱仓库是 `s8-layers`，现场：同一类文件，四份规则文件都能管它——得知道谁在起作用。

### 8A 第一层：仓库根 .gitignore（会被提交，全项目共享）

```bash
$ git check-ignore -v --no-index root-build.log
.gitignore:2:*.log	root-build.log
[退出码 0]

$ git check-ignore -v --no-index keep-me.log
.gitignore:3:!keep-me.log	keep-me.log
[退出码 0]
```

本章新工具第一次亮相：`git check-ignore -v --no-index <文件>`，专治「这文件到底被哪条规则管着」。
命中来源格式是 **`文件:行号:规则 →制表符→ 被问的文件`**：第一行读作「仓库根 .gitignore 第 2 行的 `*.log` 命中了 root-build.log」。
**以后不用再猜，直接问它。** 第二行 `keep-me.log` 命中的是带 `!` 的**反向排除**规则——注意它命中了、退出码也是 0，
但含义恰好相反（8E/9G 展开）。demo 复查 `git status --short`：root-build.log 不在 ?? 名单里（被忽略），
keep-me.log 大大方方在（被 `!` 救回）。

### 8B 第二层：目录内的 .gitignore（只管它那棵子树）

```bash
$ git check-ignore -v --no-index src/special.dat
src/.gitignore:2:special.dat	src/special.dat
[退出码 0]

$ git check-ignore -v --no-index special.dat
[退出码 1]  ★ 上面这段失败是本章故意让你看的
```

同一条 `special.dat` 规则：写进 `src/.gitignore` 就管得到 `src/special.dat`，**管不到仓库根那个 special.dat**（第二条退出码 1、无输出）。
**作用域 = 这份文件所在目录以下**。把子目录当私有垃圾桶乱写规则之前，先想清楚这点。

### 8C 第三层：.git/info/exclude（只对这个仓库生效，而且提交不上去）

```bash
$ tail -3 .git/info/exclude
# exclude patterns (uncomment them if you want to use them):
# *.[oa]
# *~
[退出码 0]

$ git check-ignore -v --no-index exclude-only.txt
.git/info/exclude:7:exclude-only.txt	exclude-only.txt
[退出码 0]

$ git status --ignored --short
?? .gitignore
?? data.gz
?? from-global.txt
?? keep-me.log
?? special.dat
?? src/
!! exclude-only.txt
!! root-build.log
!! src/special.dat
[退出码 0]
```

exclude-only.txt 谁都没写在 .gitignore 里，只靠 `.git/info/exclude:7` 就被忽略了（`!!` 名单里能看到它）——
这是「**我自己的破文件不想污染团队的 .gitignore**」的正确去处：`.git` 里的文件，天然提交不上去、只对本仓库生效。
（`tail -3` 那屏是它的出厂模板注释，你的仓库里也是这三行开头。）

★★ 然后是实测优先级里最反直觉的一条：demo 顺手在 exclude 里也写了 keep-me.log 相关条目，再问
`git check-ignore -v --no-index keep-me.log`，命中的**还是** `.gitignore:3:!keep-me.log`，status 里它依然是 `??`。
demo 原话：**实测优先级：.gitignore 压过 .git/info/exclude。exclude 不是「管理员特权」，它只是排在 .gitignore 后面的一层。**

### 8D 第四层：全局 core.excludesFile（你机器上所有仓库都生效）—— 只在沙箱里演示

```bash
$ git config --get core.excludesFile
/tmp/git-handbook-06-stash-ignore-XhUYQQwQ/fakehome/gitignore-global
[退出码 0]

$ git check-ignore -v --no-index data.gz
/tmp/git-handbook-06-stash-ignore-XhUYQQwQ/fakehome/gitignore-global:1:*.gz	data.gz
[退出码 0]

$ git check-ignore -v --no-index from-global.txt
/tmp/git-handbook-06-stash-ignore-XhUYQQwQ/fakehome/gitignore-global:2:from-global.txt	from-global.txt
[退出码 0]
```

`core.excludesFile` 指向一个规则文件，对你机器上**所有**仓库生效——这就是「所有仓库自动忽略 .DS_Store / IDE 目录」那类配置的实现方式。
check-ignore 的命中来源直接打到那份全局文件的完整路径；data.gz 和 from-global.txt 这两个「仓库自己没写规则」的文件就是被这层忽略的
（8A 时 data.gz 还是 ??，因为全局那层那时还没挂上；8E 起四层凑齐）。
再说一遍：这里的「全局」是假 HOME 里的假 .gitconfig；真实机器上对应的操作是
`git config --global core.excludesFile ...`——**那条命令本章没有实测，也不会出现在你的手册示例里**。

### 8E 四层互相打架时谁赢（全部实测，别背口诀）

四层全挂上后的名单（demo 的 `git status --ignored --short`）：!! 一行一个——
`data.gz`、`exclude-only.txt`、`from-global.txt`、`root-build.log`、`src/special.dat`；其中 **from-global.txt 和 data.gz 全靠沙箱那份全局规则**，
仓库自己的文件里谁都没写它们。然后三组对决：

**第一组：exclude 反排压全局。** 对 from-global.txt 问两次 check-ignore（第二次是补了规则之后）：

```bash
$ git check-ignore -v --no-index from-global.txt
/tmp/git-handbook-06-stash-ignore-XhUYQQwQ/fakehome/gitignore-global:2:from-global.txt	from-global.txt
[退出码 0]

$ git check-ignore -v --no-index from-global.txt
.git/info/exclude:9:!from-global.txt	from-global.txt
[退出码 0]

$ git status --short
?? .gitignore
?? from-global.txt
?? keep-me.log
?? special.dat
?? src/
[退出码 0]
```

exclude 里补一条 `!from-global.txt` → 命中换成 `.git/info/exclude:9`，文件从 !! 变回 ??。**第 3 层压过第 4 层。**

**第二组：仓库根 .gitignore 反排压全局。** demo 先 `tail -5 .gitignore` 给出规则现场（`# 仓库根 .gitignore` / `*.log` / `!keep-me.log` / `!*.gz`），
然后问 data.gz：

```bash
$ git check-ignore -v --no-index data.gz
.gitignore:4:!*.gz	data.gz
[退出码 0]
```

全局那条 `*.gz` 被 `.gitignore:4:!*.gz` 压掉，demo 复查 status 里 data.gz 又变回未跟踪。**第 1 层压过第 4 层。**
（顺带能从 tail 数出行号：`!*.gz` 是第 4 行，后补的 `!special.dat` 是第 5 行。）

**第三组：子目录 .gitignore 管自己的子树，根上的反排手伸不进去。**

```bash
$ git check-ignore -v --no-index special.dat
.gitignore:5:!special.dat	special.dat
[退出码 0]

$ git check-ignore -v --no-index src/special.dat
src/.gitignore:2:special.dat	src/special.dat
[退出码 0]
```

根 .gitignore 的 `!special.dat` 救回了仓库根那个；但 `src/special.dat` 命中的还是 `src/.gitignore:2`——
**越靠近这个文件的规则越优先，根上的反排除管不到子目录。** demo 最后复查 `--ignored` 名单：src/special.dat 仍 !!，其余被救的都回到 ??。

实测优先级（高 → 低），demo 原话：**子目录 .gitignore > 仓库根 .gitignore > .git/info/exclude > 全局 core.excludesFile。**

### 8F 一眼看清每条配置来自哪个文件

```bash
$ git config --list --show-origin | grep -i excludesfile
file:/tmp/git-handbook-06-stash-ignore-XhUYQQwQ/fakehome/.gitconfig	core.excludesfile=/tmp/git-handbook-06-stash-ignore-XhUYQQwQ/fakehome/gitignore-global
[退出码 0]
```

`--show-origin` 把来源文件写在每行前面（system / 全局 / 仓库本地一目了然）。这里 HOME 已是假家目录，所以全是沙箱路径——
在真机器上，这一条就是排查「我明明没写过忽略规则」的第一把钥匙。

### 本章四层速查表

| 层 | 文件 | 生效范围 | 会提交给团队吗 | 适合放什么 |
|---|---|---|---|---|
| 1 | 仓库根 `.gitignore` | 全仓库 | **会**（它是源码） | 全项目共享的产物规则（*.o、build/、*.log） |
| 2 | 目录内 `.gitignore` | 该目录子树 | 会 | 子项目/子模块自己的规则 |
| 3 | `.git/info/exclude` | 仅本仓库 | 不会（在 .git 里） | 你个人的临时垃圾，别污染第 1 层 |
| 4 | 全局 `core.excludesFile` | 你机器上所有仓库 | 不会 | 操作系统/编辑器杂物（.DS_Store、IDE 目录） |

**常见翻车**：把自己的临时垃圾写进仓库根 .gitignore 提交给全团队（该用第 3 层）；
把团队规则只写进自己的第 3/4 层（别人照样把产物提交进来，该用第 1 层）；
用「越私有越优先」的直觉口诀猜优先级（8C 实测打了脸：exclude 输给仓库根 .gitignore）。

---

## 第 9 节 规则怎么写：/ ** ! 尾斜杠，每条都用 check-ignore -v 验一遍

**场景**：一份把常用写法全用上的 .gitignore（沙箱 `s9-rules`），16 行、每条都有对应的靶文件，逐条过堂。

```bash
$ grep -n '' .gitignore
1:### 本章的规则示范（下面逐条用 check-ignore 验）
2:*.o
3:build/
4:cache/
5:/toplevel-only.txt
6:*.md
7:!important.md
8:docs/**/draft.md
9:src/**/*.tmp
10:/dist/
11:logs/
12:nested/
13:secret/
14:!secret/public/
15:keys/*
16:!keys/public/
```

### 9A 尾斜杠只匹配目录；不带斜杠的穿透任意层

```bash
$ git check-ignore -v --no-index build/app.o
.gitignore:3:build/	build/app.o
[退出码 0]

$ git check-ignore -v --no-index cache
[退出码 1]  ★ 上面这段失败是本章故意让你看的

$ git check-ignore -v --no-index logs/app.log
.gitignore:11:logs/	logs/app.log
[退出码 0]
```

规则里有 `cache/`（带尾斜杠），而 `cache` 是个**文件** → 匹配不上（退出码 1）。**尾斜杠 = 只当目录用**；目录里的文件才归它管
（`build/app.o` 命中 `build/`、`logs/app.log` 命中 `logs/`）。

### 9B 开头的 / = 锚定到这份 .gitignore 所在目录，不往下递归

```bash
$ git check-ignore -v --no-index toplevel-only.txt
.gitignore:5:/toplevel-only.txt	toplevel-only.txt
[退出码 0]

$ git check-ignore -v --no-index src/toplevel-only.txt
[退出码 1]  ★ 上面这段失败是本章故意让你看的
```

第一条命中、第二条不命中：`/toplevel-only.txt` 只管仓库根那一个。**开头的 `/` 是锚点**：锚在这份 .gitignore 所在目录，一层不多管。

### 9C ** = 跨任意层目录（可以是 0 层）

```bash
$ git check-ignore -v --no-index docs/draft.md
.gitignore:8:docs/**/draft.md	docs/draft.md
[退出码 0]

$ git check-ignore -v --no-index docs/a/b/draft.md
.gitignore:8:docs/**/draft.md	docs/a/b/draft.md
[退出码 0]

$ git check-ignore -v --no-index src/sub/t.tmp
.gitignore:9:src/**/*.tmp	src/sub/t.tmp
[退出码 0]
```

`docs/draft.md`（中间没有额外目录层）也归 `docs/**/draft.md` 管——**`**` 可以匹配 0 层**。
demo 同样验证了 `src/t.tmp` 被 `src/**/*.tmp` 命中（直属层也抓到）。

### 9D 一条规则忽略了一个目录之后，Git 根本不会走进去

```bash
$ git check-ignore -v --no-index dist
.gitignore:10:/dist/	dist
[退出码 0]

$ git check-ignore -v --no-index dist/inner.txt
.gitignore:10:/dist/	dist/inner.txt
[退出码 0]
```

`/dist/` 命中目录**本身**，里面的文件跟着一起没了。★ 记住这句机制——**Git 不会走进被忽略的目录里看**——9F 那个著名的坑就是它的直接推论。

### 9E ! 反向排除：同一份文件里后面的规则赢

```bash
$ git check-ignore -v --no-index notes.md
.gitignore:6:*.md	notes.md
[退出码 0]

$ git check-ignore -v --no-index important.md
.gitignore:7:!important.md	important.md
[退出码 0]
```

demo 复查 `git status --short`：important.md 出现在 ?? 里（notes.md 这类没被 ?? 列出来——它在别的方式下被忽略/不存在）。
**important.md 命中的是写在 `*.md` 后面的 `!important.md` → 它没被忽略。同一份文件里，后面的规则赢。**
顺序就是语义：先画大圈（`*.md`），再抠例外（`!important.md`）。

### 9F ★★ 最著名的坑：父目录被整个忽略时，子目录反排不回来

看同一份文件里的第 13/14 行（错误写法）与第 15/16 行（正确写法），现场对比：

```bash
$ git check-ignore -v --no-index secret/public/readme.txt
.gitignore:13:secret/	secret/public/readme.txt
[退出码 0]

$ git check-ignore -v --no-index keys/public/readme.txt
[退出码 1]  ★ 上面这段失败是本章故意让你看的
```

`secret/` 整个被忽略 → **`!secret/public/` 这条【完全没用】**——Git 根本不会走进 secret/ 里面，
你的反排规则连被求值的机会都没有（9D 的机制在这兑现；demo 也验了 `secret/key.txt`，同样打在 `:13:secret/` 上，第 14 行全程隐身）。
正确版本 `keys/*` + `!keys/public/`：`keys/key.txt` 被 `:15:keys/*` 忽略，而 `keys/public/readme.txt` **退出码 1 = 没被忽略，真的救回来了**。
差别在机制：`keys/*` 没有把 keys/ **目录本身**判死，Git 还会走进 keys/，`!keys/public/` 才有机会翻盘。
demo 复查 status：`?? keys/` 大大方方在册。规则只要改一行：**忽略目录的【内容】(`dir/*`) 而不是目录本身 (`dir/`)**。
**这一条能省掉无数人半小时。**

### 9G check-ignore 的退出码语义（很容易读错）

```bash
$ git check-ignore -v --no-index important.md
.gitignore:7:!important.md	important.md
[退出码 0]

$ git check-ignore --no-index important.md
[退出码 1]  ★ 上面这段失败是本章故意让你看的

$ git check-ignore -v --no-index Makefile
[退出码 1]  ★ 上面这段失败是本章故意让你看的
```

三问对照：

| 问法 | 退出码 | 真实含义 |
|---|---|---|
| `check-ignore -v` 命中带 `!` 的规则 | 0 | **有条规则命中了**（但那是反向排除 → 其实没被忽略） |
| 同一文件，`check-ignore` 不带 `-v` | 1 | **真的没被忽略** |
| `check-ignore -v` 问没规则沾的文件（Makefile） | 1 | 一条规则都没命中 |

**带 `-v` 的 0 = 「有一条规则命中」，命中的是 `!` 规则时其实【没被忽略】；不带 `-v` 时 0 = 「真的被忽略」、1 = 没被忽略。
想知道到底忽略没忽略，用不带 -v 的那条。** demo 还补了关键约定：**`--no-index` = 「已跟踪的文件也按规则判」**；
不加它时 check-ignore 会直接**跳过已跟踪文件**——第 10 节整节就是这个坑的现场。

### .gitignore 语法速查表（每条对应上面的实测）

| 写法 | 含义 | 实测 |
|---|---|---|
| `*.o` | 不带斜杠：匹配**任意层**的同名模式 | 9A（build/app.o） |
| `build/` | 带尾斜杠：只匹配**目录**（及其内一切） | 9A（cache 文件不命中 `cache/`） |
| `/name` | 开头的 /：锚定到本文件所在目录，**不递归** | 9B |
| `a/**/b` | `**` 跨任意层，**含 0 层** | 9C |
| `*.md` 后接 `!important.md` | 同文件内**后面的规则赢**，`!` = 反向排除 | 9E |
| `secret/` + `!secret/public/` | ★ 无效！父目录整个被忽略，Git 不进去 | 9F |
| `keys/*` + `!keys/public/` | ★ 正确姿势：忽略**内容**而非目录本身 | 9F |
| `# 开头` | 注释行（那份示范文件第 1 行 `###` 全程没人命中） | 9 节开头 |

**常见翻车**：用 `dir/` + `!dir/sub/` 想「整个目录忽略、里面留白名单」（9F，改 `dir/*`）；
`check-ignore -v` 退出码 0 就当「被忽略了」（9G，`!` 规则也是 0）；
给已跟踪文件写完规则、用不带 `--no-index` 的 check-ignore 一验什么都没打印，就以为规则写错了（10B）。

---

## 第 10 节 已提交的文件写进 .gitignore 为什么没用 —— 正解是 git rm --cached

**场景**：手滑把带 token 的本地配置 `config.local.json` 提交了（基线 `fbc9b42 c1 手滑把本地配置也提交了`），
现在补一条 .gitignore 想让它「消失」。每个人都会踩一次的节。

### 10A 现在才想起来加 .gitignore —— 什么也没发生

```bash
$ git status --short
?? .gitignore
[退出码 0]
  ↳ 列表里没有 config.local.json 那一行 —— 因为它【已经在仓库里了】。gitignore 只管未跟踪的文件。

$ git status --short
 M config.local.json
?? .gitignore
[退出码 0]
```

第一屏：加了规则，status 里只有新建的 .gitignore 自己。config.local.json 没消失，但也**完全不受影响**——它已经在仓库里了。
第二屏是故意改了一下它之后的复查：**改它照样被报出来**（M 在第 2 格）。这就是「.gitignore 对已跟踪文件无效」的现场。
回到图 3 那棵树：已跟踪分支根本不查忽略规则。

### 10B check-ignore 在这里会骗你：不带 --no-index 时它压根不看这条规则

```bash
$ git check-ignore -v config.local.json
[退出码 1]  ★ 上面这段失败是本章故意让你看的

$ git check-ignore -v --no-index config.local.json
.gitignore:1:config.local.json	config.local.json
[退出码 0]
```

不带 `--no-index`：退出码 1、什么都不打印。**不是规则写错了**，而是 Git 认为「这文件我在跟踪，轮不到忽略规则」
（9G 埋的伏笔在这兑现）。带上 `--no-index` 才肯按规则判一遍，打印出确实有一条 `.gitignore:1`。
两条输出合起来读才是完整真相：**规则在，但管不着。**

### 10C 正解：git rm --cached <文件> —— 从索引里摘掉，磁盘上的文件原样留着

```bash
$ git rm --cached config.local.json
rm 'config.local.json'
[退出码 0]

$ git status --short
D  config.local.json
?? .gitignore
[退出码 0]

$ cat config.local.json
{"token":"换了一个 token"}
[退出码 0]

$ git check-ignore -v config.local.json
.gitignore:1:config.local.json	config.local.json
[退出码 0]
```

逐行读：`rm '...'` 是它唯一的回执；status 里 `D  config.local.json`——第一格 D = **已暂存的「从仓库里删除」**；
`cat` 证明**磁盘上文件还在、内容还在**（demo 另用 `ls -1 config.local.json` 点了名；`--cached` 只动索引不动硬盘）。
一个精彩的细节：为什么 status 里没有多出 `?? config.local.json`？demo 答：**因为它已经命中 .gitignore 了——
未跟踪 + 被忽略 = 在 status 里隐身**（第 11 节教怎么揪出来看）。
最后一问最妙：不带 `--no-index` 的 check-ignore 现在也能命中了——**说明它已经变成未跟踪文件，归 .gitignore 管了**。
「无效 → 有效」的分界线，就是跟踪状态翻面的那一瞬间。

### 10D 提交这次「停止跟踪」，之后就彻底清净

```bash
$ git add -A
[退出码 0]

$ git commit -q -m 'c2 把本地配置移出仓库跟踪（文件留在本地）'
[退出码 0]

$ git status --short
[退出码 0]
```

demo 在这里又改了一次文件内容再跑第二条 status——还是空的。**工作区里随便改，它都不再出现在 status 里**——
这才是大家以为 .gitignore 一开始就该有的效果。注意 `add -A` 会把 ?? .gitignore 一起提交掉，从此规则跟着仓库走。

### 10E ★★ 但是：仓库历史里那一份还在

```bash
$ git log --oneline
cdb47a1 c2 把本地配置移出仓库跟踪（文件留在本地）
fbc9b42 c1 手滑把本地配置也提交了

$ git show HEAD~1:config.local.json
{"token":"这个不该进仓库"}
[退出码 0]
```

（demo 还跑了 `git show --stat --oneline HEAD~1`，stat 里明晃晃一行 `config.local.json | 1 +`。）
`git show HEAD~1:config.local.json` 一敲，**旧 token 原形毕露**。`git rm --cached` 只是「从现在起不再跟踪」，
之前那条提交里的内容一个字都没少。demo 的处置意见原样引用：**真泄露了 token：立刻吊销换新；要清历史得用 filter-repo（10 章）。别指望 .gitignore。**
顺序也别记反：吊销不依赖清历史，清历史也救不回已被别人看到的 token——**先吊销，历史的事慢慢来**。

### 10F 顺带看一个现场：被忽略的文件也能强行提交进去

```bash
$ git add new-ignored.log
The following paths are ignored by one of your .gitignore files:
new-ignored.log
hint: Use -f if you really want to add them.
hint: Disable this message with "git config set advice.addIgnoredFile false"
[退出码 1]  ★ 上面这段失败是本章故意让你看的

$ git add -f new-ignored.log
[退出码 0]
```

`git add` 挡住了被忽略的路径并给出 hint（这个拒绝带退出码 1）；`-f` 强行照加，demo 提交成
`c3 明知被忽略，还是用 -f 提交了`，此后 status 干净——文件变成「已跟踪」，又回到 10A 的世界：**规则在，管不着**。
demo 补刀：**「团队里出现『我这儿没报，你那儿报了』，八成就是有人这么干过。」**
排查方向多一条：status 干干净净但行为不一致时，查有没有被 `-f` 硬塞进来的文件。

---

## 第 11 节 看清被忽略的东西 + git clean 只报名单（本章不真删）

### 11A git status --ignored：把「隐身」的那批也拉出来

接着 10 节的仓库：工作区很干净，但 config.local.json 明明躺在磁盘上——它只是隐身了。

```bash
$ git status --short
[退出码 0]

$ git status --ignored --short
!! config.local.json
[退出码 0]

$ git status --ignored
On branch main
Ignored files:
  (use "git add -f <file>..." to include in what will be committed)
	config.local.json

nothing to commit, working tree clean
[退出码 0]
```

`--short` 里 `!!` 前缀就是被忽略的；长格式在 `Ignored files:` 那一段，还顺手提示你可以 `git add -f`（10F 的伏笔）。
排查「我的文件怎么不见了」时，**`git status --ignored` 应该是第一个敲的命令**——十有八九它没丢，只是隐身了。

### 11B–11C 造几个未跟踪文件，然后 git clean -nd：只报名单，什么都不删（★ 永远先跑这条）

demo 造了未跟踪的 `tmpwork/`、`try1.sh`、`try2.sh`（status 里三行 ??），然后：

```bash
$ git clean -nd
Would remove tmpwork/
Would remove try1.sh
Would remove try2.sh
[退出码 0]

$ ls -1 | tr "\n" " "
app.py config.local.json new-ignored.log tmpwork try1.sh try2.sh 
[退出码 0]
```

输出全是 `Would remove ...` 虚拟语气，文件一个没动——demo 复查 status 三行 ?? 原样，`ls` 点名全在。
`-n` = dry-run，`-d` = 连未跟踪的**目录**一起看。clean 是 stash 的阴暗面：stash 挪开 Git 认识的文件，
clean **删除** Git 不认识的文件——而且 03 章说过：被 clean 删掉的未跟踪文件 Git 从没见过，**reflog 也救不回来**。

### 11D -n 不加 d：只列文件不列目录，名单会短一截

```bash
$ git clean -n
Would remove try1.sh
Would remove try2.sh
[退出码 0]
```

对比 11C：**少了 `tmpwork/`**。少了 d 就看不见整个目录——最容易在这里低估名单长度。

### 11E -nxd：连被 .gitignore 忽略的一起进名单（这才是核弹预览）

```bash
$ git clean -nxd
Would remove config.local.json
Would remove tmpwork/
Would remove try1.sh
Would remove try2.sh
[退出码 0]
```

★★ 名单里多出了 **config.local.json**——那是你**唯一一份**的本地配置文件（10 节刚辛苦把它移出跟踪）。
这正是 `-x` 最狠的地方：它把「被忽略」的也当「垃圾」扫进名单。把 `-n` 换成 `-f`，你机器上所有 .env / 本地配置 / 构建缓存一起蒸发。
demo 原话：**这份名单和真删时的名单一字不差，所以：先把 -n 在脑子里换成 -f 读一遍，再决定敲不敲。**

### 11F 本章【不执行】git clean -fd / -fdx

```bash
  ↳ 真删的现场和后果 03 章已经完整演示过：被 clean 删掉的未跟踪文件 Git 从没见过，reflog 也救不回来。
  ↳ 本章只强调一件事 —— clean 的 dry-run 必须先跑。所以这个脚本跑到现在，没删掉任何一个文件。
```

真删为什么比 pop 撞冲突可怕，第 7 节正好给了反面参照：stash 至少把东西做成对象（fsck 能捞）；
clean 掉的文件**对象库根本没存过**，神仙难救。所以本章从头到尾没有 `-f`。

### clean 参数速查表

| 命令 | 列出什么 | 删什么 |
|---|---|---|
| `git clean -n` | 未跟踪**文件**（不含目录） | 什么都不删 |
| `git clean -nd` | 未跟踪文件 + 未跟踪目录 | 什么都不删 ★ 永远先跑这条 |
| `git clean -nxd` | 上面全部 + **被 .gitignore 忽略的** | 什么都不删（核弹预览） |
| `git clean -fd` / `-fdx` | —— | 真删，不可恢复（03 章演示过） |

---

## 本章记住五句话（demo 收官时打印在屏幕上的原文）

```
  1) stash 不是剪贴板：push 造的是真 commit（^1 基线 / ^2 暂存区 / ^3 未跟踪），refs/stash 只是栈顶指针。
  2) 拿不准就 apply，确认了再 pop；pop 失败时 Git 会把 stash 留着（The stash entry is kept...），不吞你的东西。
  3) 误 drop / clear：git fsck --unreachable 找 hash，git stash apply <hash> 全数复活 —— 前提是你没跑 gc。
  4) .gitignore 有四层来源（仓库根 / 目录内 / .git/info/exclude / 全局 excludesFile），
     谁在起作用用 git check-ignore -v 问，别猜；反排除子目录要写 dir/* + !dir/sub/，而不是整个忽略 dir/。
  5) .gitignore 管不了已跟踪文件：要停止跟踪用 git rm --cached，历史里那一份得单独处理。
```

## 坑清单（事故排行榜，对照上面各节）

1. **退出码 0 ≠ 存进去了**（4B）：`No local changes to save` 也是 0。判断看 `git stash list`。
2. **apply 完又 pop**（2A-2）：`Your local changes ... would be overwritten`，好在 kept 条款兜底。习惯：apply 后先看 list。
3. **冲突没解决就再 pop 一次**（3C）：`could not write index / needs merge`，越急越乱。先解决，或走 3E 退干净、6D 换基线。
4. **先 `reset --hard` 后想起确认 stash**（3E）：★ 顺序永远是「先确认栈里还在，再清理工作区」。
5. **未跟踪文件撞名，pop 罢工**（3F）：`draft.txt already exists, no checkout`，把磁盘上那份改名/删掉再 pop。
6. **无脑 `-a`**（4D）：被忽略的产物也进 stash，pop 时和新生成的产物撞名。`-u` 是常态，`-a` 是特种作战。
7. **drop/clear 后拿裸 hash pop**（7C）：`is not a stash reference`。口诀：**fsck 找 hash → apply 用 hash**（7F）。
8. **stash 里有货顺手 gc**（7H）：`reflog expire` + `gc --prune=now` 会让 90 天保险箱当场失效。
9. **`dir/` + `!dir/sub/`**（9F）：Git 不走进被忽略的目录，反排永远够不着。改一行 `dir/*` 就活。
10. **把 check-ignore -v 的退出码 0 当「已忽略**」（9G）：命中 `!` 规则也是 0。问「到底忽略没」用不带 -v 的。
11. **给已跟踪文件加 .gitignore**（10A/10B）：无效，且不带 --no-index 的 check-ignore 会骗你。正解 `git rm --cached`（10C）。
12. **`git clean -n` 不带 d**（11D）：看不见未跟踪目录，名单短一截；`-fdx` 的名单里躺着你的唯一版 config.local.json（11E）。

## 自检问题（先答再看）

**Q1：一次 `git stash push -u` 之后，你的改动以什么形态存在？三个父亲分别是什么？**
<sub>真 commit（对象库里），refs/stash 指向它：^1 = stash 时 HEAD 所在提交（三方合并的参照物）、
^2 = index 快照（暂存区长什么样）、^3 = untracked 快照（未跟踪文件）。第 1 节 1D/1E/1F 的 cat-file、log %P、show 实测。</sub>

**Q2：apply 和 pop 的本质区别？想把 `MM`（暂存区一版 + 工作区一版）的现场原样恢复，该敲什么？**
<sub>pop = apply + drop：apply 完栈里那条还在（2A），pop 成功后 Dropped（2B），失败时 kept 不删（2A-2/3A）。
默认 apply 会把暂存状态抹平成全未暂存（2D-1）；要原样恢复用 `git stash apply --index`（2D-2 实测 MM 回来），pop 同理 `--index`。</sub>

**Q3：`git stash pop` 撞了冲突，stash 会丢吗？屏幕上靠哪句话判断？之后两条出路是什么？**
<sub>不丢。看最后一句 `The stash entry is kept in case you need it again.` + `git stash list` 复查（3A）。
出路一：`checkout --theirs` 取 stash 侧 → `add` 标记解决 → 确认后再 `drop`（3D）；
出路二：先确认 list 里还在，再 `reset --hard` 退干净、改天处理（3E）；长期抗战用 6D 的 `git stash branch`。</sub>

**Q4：`git stash`、`stash push -u`、`stash push -a` 各收哪些文件？「工作区只剩一个未跟踪文件」时不带 -u 敲 stash 会发生什么？**
<sub>默认只收已跟踪（4A）；-u 加收未跟踪（^3 出现，4C）；-a 连被 .gitignore 忽略的也收（4D，风险见坑 6）。
只剩未跟踪时不带 -u：打印 `No local changes to save`、退出码 0、**什么都没存**（4B）。</sub>

**Q5：`git stash clear` 之后 reflog 为什么也查不到？救援三步是什么？为什么 pop 用 hash 不行而 apply 行？**
<sub>clear 连 refs/stash 这根 ref 一起删，流水账随 ref 消失（7B 的 fatal）。三步：`git cat-file` 确认对象还在（7D）→
`git fsck --unreachable` 列无人引用对象、按说明行认人（7E）→ `git stash apply <hash>` 复活（7F）。
pop 只认 `stash@{n}` 这种 reflog 位置，裸 hash 被拒（7C）。</sub>

**Q6：.gitignore 四层来源的实测优先级是什么？「仓库私有」的 .git/info/exclude 是不是最大？**
<sub>子目录 .gitignore > 仓库根 .gitignore > .git/info/exclude > 全局 core.excludesFile（8E 三组实测：
exclude 压全局、根 .gitignore 压全局、子目录压根）。exclude 不是管理员特权，它输给仓库根 .gitignore（8C 实测 keep-me.log）。</sub>

**Q7：想让 secret/ 里只忽略私货、保住 secret/public/，错误写法和正确写法分别是什么？为什么？**
<sub>错误：`secret/` + `!secret/public/`（9F：父目录整个被忽略，Git 不进去，反排永无机会）；
正确：`keys/*` + `!keys/public/`（忽略内容而非目录本身；keys/public/readme.txt 的 check-ignore 退出码 1 实锤救回）。
机制在 9D：一条规则忽略目录本身后，Git 根本不会走进去。</sub>

**Q8：把 config.local.json 写进 .gitignore 后它还在 status / 还在仓库，分别是什么原因？正解？历史里那份怎么办？**
<sub>还在 status：已跟踪文件不受忽略规则影响，改动照样报 M（10A）；check-ignore 不带 --no-index 会假装没规则（10B）。
正解：`git rm --cached` 停止跟踪（10C），提交后它在 status 里彻底清净（10D）。
历史里那份还在（10E）：立刻吊销 token，清历史去 10 章 filter-repo——`.gitignore` 和 `rm --cached` 都救不了旧提交。</sub>

## 运行 demo

```bash
cd git-handbook/06-stash-and-ignore
bash demo.sh          # 全跑一遍（11 节），跑完自动删沙箱；输出 1623 行，退出码 0
bash demo.sh --keep   # 跑完保留沙箱并打印路径，可以进去自己敲命令
```

脚本约定（本章的安全边界，demo.sh 开头注释里逐条写着）：

- 所有 git 写操作只发生在 `mktemp -d` 的临时沙箱里，**绝不碰你机器上的任何真实仓库**。
- 脚本一开始就把 `HOME` 指进沙箱的 `fakehome/`：任何 git 命令都读不到、更写不到你真实的 `~/.gitconfig`；
  演示「全局忽略文件」用的那份假 `.gitconfig` 是 `cat` 手写的——**全脚本没有任何一条 `git config --global`**。
- 提交身份只在命令行给（`-c user.name=学习者 -c user.email=learn@example.com`），不写进任何配置文件；
  屏幕上为可读性把这两个 `-c` 隐掉了，真实执行的命令里它们一直在。
- 不联网：本章没有 clone/push/fetch，「上游提交」都是本地直接造（第 3 节）。
- 时间戳从 `2026-01-01T10:01:00 +0800` 逐条递增固定，所以**哈希可复现**；唯一会变的是沙箱绝对路径那几行（每次随机）。
  输出固定英文（`LC_ALL=C.UTF-8`），git 提示语你在自己机器上看到的可能被语言配置翻译过，内容同义。
- 脚本**故意不加 `set -e`**：这一章要看的就是命令失败长什么样，非 0 退出码处都打了
  `★ 上面这段失败是本章故意让你看的`。
- `git clean` 全程只跑 dry-run（-n / -nd / -nxd），脚本没有删掉任何一个文件（11F 自证）。

`--keep` 之后，推荐进沙箱把本章最容易记反的三处亲手再验一遍：

```bash
cd "$SB/s1-anatomy"   && git show 'stash@{0}^2'                              # 三个父亲到底长什么样
cd "$SB/s3-conflict"  && git stash list                                      # pop 冲突后那句 kept 的实物
cd "$SB/s8-layers"    && git check-ignore -v --no-index keep-me.log          # exclude 输给 .gitignore 的反直觉现场
```

本章的引用校验以 `/tmp/git06.txt`（本次 `bash demo.sh` 的完整输出）为唯一事实来源；
正文每个终端块（含 `[退出码 N]` 与 `↳` 注释行）都逐字来自它。

## 下一章预告

**07-remote-collab**：本章的对象和规则都只活在你自己的机器上；下一章把「远程」接进来——
`clone / fetch / push / pull`，以及那个你迟早要问的问题：`The stash entry is kept` 里的 stash，
和同事的 `origin/main` 到底谁看得见谁。本章第 3 节用本地提交模拟的「上游改了同一行」，在 07 章将真的从 `git fetch` 里长出来。

另外两处本章刻意留的口子：03 章的 `clean -f` 真删现场和 `reset --hard` 威力（3E/11F 都指回了它）；
10 章的 `git filter-repo`——把 10E 那个还躺在 `HEAD~1` 里的 token 从历史中彻底挖走。
