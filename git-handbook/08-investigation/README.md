# 08 排查取证

> 一句话：这一章教你向 Git 问**四个问题**，并且拿到**能复现、能截图、能贴进故障报告**的答案 ——
> 「这段时间里谁改了什么」「这一行是谁、哪次提交写的」「哪一次提交把它改坏了」「我以为已经删掉的东西还在不在」。

前面几章教你**往前走**（提交、分支、合并、撤销）。这一章教你**回头查**。
Git 最强的地方就在这里：**它把每一次改动的作者、时间、内容、前后关系全都存下来了，
而且提供了四套互不相同的检索入口**。出问题时，你不该靠"我记得大概是上周改的"，
而该靠下面这些命令给出的**证据**。

本章所有输出都是 `bash demo.sh` 在 `mktemp -d` 沙箱里真跑出来的。
demo 一共 8 节（`0`、`1`、`2`、`3`、`4`、`4b`、`5`、`6`），本章按这个顺序讲，
每个场景标题上都标了它对应 demo 的第几节。

---

## 你会遇到的场景

| 你遇到的事 | 本章哪一节 | 对应 demo |
|---|---|---|
| 线上炸了，想先看"最近两周到底谁动了什么" | 场景 2 ①② | 第 1 节 |
| 想知道"这个常量/函数什么时候被引进来、什么时候被改名的" | 场景 2 ③（`-S` vs `-G`） | 第 1 节 |
| 找"和某个需求号有关的提交"，要写周报 / 复盘 | 场景 2 ④⑧ | 第 1 节 |
| 只想知道某个文件（或目录）的改动史 | 场景 2 ⑤ | 第 1 节 |
| 文件改过名，`git log <新路径>` 只给一条，前面历史像断了 | 场景 2 ⑥（`--follow`） | 第 1 节 |
| 主干 log 里混进别人分支内部的一堆碎提交，看不过来 | 场景 2 ⑦（`--first-parent`） | 第 1 节 |
| 想统计"每个人写了多少行""哪个文件改动最频繁" | 场景 2 ⑨（`--numstat`） | 第 1 节 |
| 不想切分支、不想动工作区，只想看"三天前那个文件长什么样" | 场景 3 ①（`git show <版本>:<路径>`） | 第 2 节 |
| 分不清 `diff A B` / `A..B` / `A...B` / `HEAD^!` 各比的是什么 | 场景 3 ⑤⑥（三张区 + 四种目标写法） | 第 2 节 |
| 这行诡异代码是谁写的？为什么不删？ | 场景 4 ①②⑤ | 第 3 节 |
| 有人只改了缩进，`blame` 就把这行判给他了 | 场景 4 ③（`-w`） | 第 3 节 |
| 这段代码是从别的文件抄来的，`blame` 只报抄的那次提交 | 场景 4 ④（`-C`） | 第 3 节 |
| 内容明明来自最初那次提交，`blame` 却报了最近碰过这行的人 | 场景 4 ⑥ | 第 3 节 |
| 昨天还好好的，今天就不对了，24 个提交里有一个改坏了 | **场景 5（`bisect`）** | 第 4 节 |
| 有合并提交时 `bisect` 问到了从没上线过的版本，结论看不懂 | **场景 6（`--first-parent`）** | 第 4b 节 |
| 误删分支 / 误 `reset --hard`，连哈希都没记 | **场景 7（`reflog`）** | 第 5 节 |
| `@{yesterday}`、`@{u}`、`@{-1}` 这些写法到底指什么 | 场景 7 ③④⑤ | 第 5 节 |
| 悬空提交是什么？`git fsck` 输出怎么读？东西什么时候**真**没了 | **场景 8（`fsck` / `gc`）** | 第 6 节 |

---

## 心智模型

### 图 1：排查是「先粗后细」的漏斗，不是「一上来就 blame」

```
   你手里通常只有一个模糊线索：「大概上周坏的」/「这行不对劲」/「我刚才删错了」

   ┌──────────────────────────────────────────────────────────────────┐
   │ 第 1 步 缩小范围到「几条提交」                                     │
   │   git log 的五个阀门（可以叠加）：                                │
   │      时间 --since/--until   人 --author   说明 --grep             │
   │      内容 -S / -G           路径 -- <路径>（跨改名加 --follow）    │
   │   产出：3~10 个哈希                                               │
   └───────────────────────────┬──────────────────────────────────────┘
                               ↓
   ┌──────────────────────────────────────────────────────────────────┐
   │ 第 2 步 看清「那几条到底干了什么」（纯只读，不动工作区）            │
   │   git show <提交> -- <路径>     git show <版本>:<路径>             │
   │   git diff <A> <B>              git blame -L <行号> <文件>         │
   │   产出：一句话结论 ——「是 c17 把系数从 *10 改成 *10+3」            │
   └───────────────────────────┬──────────────────────────────────────┘
                               ↓
   ┌──────────────────────────────────────────────────────────────────┐
   │ 第 3 步 线索不够就交给机器（把「好/坏」写成退出码）                 │
   │   git bisect run ./test.sh          ← 24 个提交问 5 次就到位       │
   │   git bisect start --first-parent … ← 有合并提交时用它             │
   └───────────────────────────┬──────────────────────────────────────┘
                               ↓
   ┌──────────────────────────────────────────────────────────────────┐
   │ 第 4 步 数据疑似丢了：查回收站                                    │
   │   git reflog → git cat-file -t <哈希> → git fsck --lost-found      │
   │   终点：reflog expire + gc --prune=now 之后才是真没了               │
   └──────────────────────────────────────────────────────────────────┘
```

**为什么必须先粗后细**：`git blame` 是"给一个文件 + 一个行号"的工具，范围窄、开销大；
`git log` 是"给一堆条件"的工具，范围宽、开销小。
反过来用（对着 20 万行的文件 blame 全场）会慢到你以为卡死了。

### 图 2：`-S` 和 `-G` 的唯一区别 —— 一个数次数，一个看补丁

这两条是最被误用的组合，区别一句话说得完：

```
   某次提交的 diff（补丁）长这样：
        -    for i in range(MAX_RETRY):
        +    for i in range(ATTEMPTS):

   -G'MAX_RETRY'  问：diff 的任何一行里出现过 "MAX_RETRY" 吗？
                       ↑ 上面那行被删掉了，它本身就是 diff 的一行 → 命中

   -S'MAX_RETRY'  问：整个文件里 "MAX_RETRY" 出现的次数，这次提交前后变了吗？
                       ↑ 改之前 1 次、改之后 0 次 → 变了 → 也命中


   可是「把两个函数上下换个位置」这种提交：
       改之前 MAX_RETRY 出现 2 次
       改之后 MAX_RETRY 出现 2 次     ← 次数没变！
       但 diff 里确实有含 MAX_RETRY 的 + 行和 - 行

   → -G 命中，-S 看不见。本章实测里 5 条 vs 4 条的全部差别就在这里。

   记法：**找「谁写的 / 谁删的」用 -S，找「谁碰过」用 -G。**
```

### 图 3：`bisect` 的本质 = 把历史当成「前好后坏」的数组做二分

```
   24 个提交（c1 是打了标签的已知好版本，c24 是现在，已知坏）：

   c1 ── c2 ── ... ── c16 ── c17 ── c18 ── ... ── c24
   [好 ───────────── 好 ]     [ 坏 ──────────── 坏 ]
                              ↑ 真凶 = 第一个坏（c17，把 *10 改成 *10+3）

   只要「好/坏」在时间上单调（一旦坏了后面全坏），二分就成立：

   第 1 问 中点 c12 → GOOD   → 范围砍半，剩 c13..c24
   第 2 问 中点 c18 → BAD    → 真凶在 c13..c18
   第 3 问 中点 c15 → GOOD   → c16..c18
   第 4 问 中点 c17 → BAD    → c16..c17
   第 5 问 中点 c16 → GOOD   → 只剩 c17 → 结论

   log2(23) ≈ 4.6  →  实测正好问了 5 次。手工一条条看要 24 次。
```

**物理上"两端"存在哪**（本章第 4 节 `ls` 出来的就是这几个文件）：

```
   .git/refs/bisect/
   ├── bad                          ← 坏端（只有一个文件，存当前最紧的那个坏提交）
   ├── good-6e17a2b...              ← 好端：你标过的每个好提交各一个文件
   └── good-dd977ef...              ←   （本章 demo 里这两个是 c12 和 c1=v1.0）
```

所以 `git bisect terms` 打印的 "your current terms" 不是虚的，磁盘上真有这几个 ref 文件。
这也是为什么**中途退出必须 `git bisect reset`** —— 它删掉这些临时 ref，并把你送回开始二分之前的位置。

### 图 4：有合并提交时数组"不单调"了，所以普通 bisect 会问到你从没上线过的版本

```
   主干（main）                分支（experimental）
   m1 ─ m2 ─ ... ─ m12 ─┬─ m13 ─ ... ─ m22 ─┐
                        └ x1 ─ x2 ─ x3 ─────┴─ M（合并提交 = HEAD）
                              ↑ 真凶在这条线上，但它从来没在主干上跑过

   普通 git bisect start HEAD base_ok：
     它按拓扑序把 26 个提交摊平，于是可能先问 x1、再问 m18、再问 m21……
     ★ 问题：x1 / x3 是分支内部状态，线上从没跑过这个组合；
       它们更可能编译不过 → 脚本返回 125/127 → 二分的判据当场变脏。

   git bisect start --first-parent HEAD base_ok：
     只在主干那一列里二分：m12 → m17 → m20 → m22 → 结论停在合并提交 M
     ★ 这才是「坏代码什么时候进入主干」的正确答案。

   想知道具体是分支里哪一条改坏的，再来一击：
     git bisect start 'M^2' 'M^1'
     ↑ 坏端取合并的第二父亲（分支那侧 x3），好端取第一父亲（主干那侧 m22），
       git 会先逼你测一次两者的 merge base（m12），然后钻进分支给出 x2。
```

### 图 5：三层保险 —— reflog、对象库、gc 的到期时间轴

```
   你以为"删了"的东西，物理上被三层兜着：

   第 1 层  reflog（.git/logs/HEAD 和 .git/logs/refs/heads/<分支>）
            —— 纯文本流水账，记 HEAD / 分支指针走过的每一步。
               可达记录默认留 90 天（gc.reflogExpire），
               不可达的留 30 天（gc.reflogExpireUnreachable）
   第 2 层  对象库（.git/objects）
            —— 只要 add 过，内容就在里面。没人引用的叫 dangling / unreachable
   第 3 层  gc 的宽限期
            —— 不可达对象默认满 2 周才真的删（gc.prune = expire,expire-2-weeks）

   时间轴（demo 第 6 节实测的正是这条线）：

   reset --hard ──► fsck: 干净（reflog 还兜着，不算悬空）
        │
        ├─────────► fsck --no-reflogs: dangling commit 86bd29a   ← 还在！能救
        │
        └─────────► git reflog expire --expire=now --all
                    git gc --prune=now
                    └──► fsck 一片空白 + cat-file 报 fatal        ← 真没了，神仙难救

   ★ 本章的保命结论只有一句：**救场要趁早，别等 gc 跑过。**
   ★ 而 Git 唯一从没见过的东西 = 你从没 `git add` 过的文件（第 6 节 ⑦ 实测：搜遍全库 0 命中）。
```

### 一张表：问题 → 命令

| 你想问的 | 命令 | 本章实测编号 |
|---|---|---|
| 最近两周谁改了什么 | `git log --since='2 weeks ago' --format='%h %ad %an %s' --date=short` | 2 ① |
| 某人干了哪些活 | `git log --author='同事甲'` / `git shortlog -sne HEAD` | 2 ② |
| 谁引入了某个字符串 | `git log -S'字符串'` | 2 ③ |
| 谁碰过某个字符串（含改名、移动） | `git log -G'字符串'` | 2 ③ |
| 和 #103 有关的提交 | `git log --grep='#103'` | 2 ④ |
| 某个文件的改动史 | `git log -- <路径>` | 2 ⑤ |
| 某个文件改名前的历史 | `git log --follow -- <新路径>` | 2 ⑥ |
| 只看主干那一列 | `git log --first-parent` | 2 ⑦ |
| 机器可读的作者 / 行数报表 | `git log --format=… --numstat` | 2 ⑧⑨ |
| 老版本某个文件的内容 | `git show '<版本>:<路径>'` | 3 ① |
| 某提交只对某文件干了什么 | `git show <提交> -- <路径>` | 3 ② |
| 这行谁写的 | `git blame -L <a>,<b> <文件>` | 4 ①② |
| 忽略纯空白改动 | `git blame -w` | 4 ③ |
| 跨文件追拷贝来源 | `git blame -C` | 4 ④ |
| 某一段行的演化史 | `git log -L <起>,<止>:<文件>` | ★ demo 未跑（见速查表说明） |
| 揪出改坏的那次提交 | `git bisect start/bad/good/run` | 5 |
| 有合并时只按主干二分 | `git bisect start --first-parent` | 6 |
| 我刚才都干了什么 | `git reflog` / `git log -g` | 7 ①② |
| 昨天的 HEAD 是哪个 | `git rev-parse 'HEAD@{yesterday}'` | 7 ④ |
| 上一个待过的分支 | `git rev-parse --abbrev-ref '@{-1}'` | 7 ③ |
| 当前分支的远程跟踪点 | `git rev-parse --abbrev-ref '@{u}'` | 7 ③ |
| 有没有没人引用的提交 | `git fsck` / `--no-reflogs` / `--lost-found` | 8 ②③ |

---

## 场景逐个实战

> **读输出前的三条说明**（不看会以为自己有环境问题）：
>
> 1. **沙箱名**：每次跑 `mktemp -d` 出来的名字都不同，本章这一版是 `/tmp/tmp.95kGtyLeQ2`。
>    你看到的路径后缀会和这里不一样，其余内容一致。
> 2. **哈希的可复现性分两半**（这是本章和其他章最大的不同）。
>    demo 第 `0/1/2/3/5` 节故意把提交日期做成**相对运行当天**（`date -d "35 days ago"`），
>    否则 `--since='2 weeks ago'`、`@{yesterday}` 这类写法就查不到东西。
>    代价是**这几节的日期和哈希每次跑都会变** —— 本章连跑两次做对照：把两次的沙箱名换成同一个占位符后
>    `diff` 出 **728 行差异，100% 落在 `0/1/2/3/5` 节**（原始 diff 1130 行，多出来的全是沙箱名）。
>    第 `4/4b/6` 节用固定时间戳 `2026-01-01T10:00:00 +0800`，**哈希逐字节可复现**
>    （第 4/4b 节 241 行、第 6 节 89 行输出，两次跑 `diff` = **0 行**）。
>    **但不论哪天跑：每条命令命中几条提交、命中哪几条、二分问几次、结论落在哪个提交，全部一致** —— 这才是本章要学的东西。
> 3. **`$` 那一行是 demo 自己打印的真实执行命令**。
>    为了屏幕好读，demo 把提交身份 `-c user.name=学习者 -c user.email=learn@example.com`
>    从显示里抹掉了（真实执行时它是带上的）。你照抄屏幕上那行去敲，只读命令能直接跑通；
>    涉及提交的要自己补上身份，**不要去动全局配置**。
>    输出是英文，因为脚本 `export LC_ALL=C LANG=C`；你手敲时大概率是中文，本节末有中英对照。

### 场景 1：先把「调查现场」摆出来 —— 15 个提交、3 个作者、跨 5 周（demo 第 0 节）

**什么情况**：要练检索，得先有一个"像真实项目"的历史：多作者、跨时间、含改名、含合并、
含纯空白改动、含复制粘贴。demo 第 0 节就是在造这些"道具"，没有它们后面每一节都没有靶子。

**敲哪几条**（这一节只是造现场，最后一条才是看）

```bash
git log --graph --oneline --date=short --format='%h %ad %an %s'
```

**真实输出**

```text
* 320a44b 2026-10-03 同事甲 fix: 运费按重量加倍 (#205)
* 30fef87 2026-10-02 同事甲 feat: 给 total_price 加缓存
* 4a8560c 2026-09-30 学习者 docs: design.md 改名 architecture.md 并补一句
* 7d8a14d 2026-09-28 同事乙 chore: 删掉调试打印
* b7bfb91 2026-09-27 同事乙 wip: 临时调试打印
*   a616ae6 2026-09-25 学习者 Merge branch 'feature/export' (#103 落地)
|\  
| * 1fd8cd4 2026-09-23 同事甲 feat: 支持导出 CSV (#103)
* | 7bf241d 2026-09-24 学习者 chore: 主干上的一次普通改动
|/  
* 45e6d20 2026-09-22 学习者 refactor: 把运费逻辑抄一份到 src/ship.py
* 3a5c74e 2026-09-20 同事乙 style: 调了一下 count_items 的缩进（纯空白改动）
* d4d56a1 2026-09-17 同事甲 refactor: MAX_RETRY 统一改名 ATTEMPTS (#104)
* 505349d 2026-09-16 学习者 feat: 多处引用 MAX_RETRY（出现次数变多了）
* 5e88b21 2026-09-13 同事乙 refactor: retry_policy 和 connect 换个位置（MAX_RETRY 一次没增没减）
* 930dc06 2026-09-06 学习者 fix: 修登录页等级为负时的崩溃 (#102)
* 79f410b 2026-09-04 同事乙 feat: 给 net.py 加 MAX_RETRY 的引用 (#101)
* 1d03ac3 2026-08-30 同事甲 init: 项目骨架
```

**这张图里埋的六个道具，后面每一个都要用到**：

| 道具 | 提交 | 给后面哪一节当靶子 |
|---|---|---|
| 只是把两个函数上下换位置 | `5e88b21` | 场景 2 ③：`-G` 命中而 `-S` 不命中 |
| 把 `MAX_RETRY` 全量改名 `ATTEMPTS` | `d4d56a1` | 场景 2 ③：`-S`/`-G` 都命中 |
| 只把空格换成 Tab（纯空白） | `3a5c74e` | 场景 4 ③：`blame -w` |
| 把运费逻辑整份抄进 `src/ship.py` | `45e6d20` | 场景 4 ④：`blame -C` |
| `docs/design.md` 改名 `architecture.md` | `4a8560c` | 场景 2 ⑥ + 场景 4 ⑥：`--follow` |
| 一次 `--no-ff` 合并（第二父亲是 `1fd8cd4`） | `a616ae6` | 场景 2 ⑦ `--first-parent` + 场景 6 二分翻车 |

还有一条：`30fef87` 的提交正文里塞了 trailer（`Co-authored-by` / `Signed-off-by`），给场景 2 ⑧ 用。

**常见翻车**

- 别用裸 `git log` 看现场（默认分页 + 显示完整正文），一屏塞不下。
  排查阶段一律配 `--oneline --decorate`，或用 `--format` 自己挑字段。
- `--graph` 的 `|\` 和 `|/` 这两行**行尾带空格**。你把 log 重定向成文本发给同事时，
  某些编辑器会抹掉行尾空格，图就散了。要存档就用 `--oneline`，别存图形。
- 分支 `feature/export` 在合并后立刻被 demo 删了（`git branch -D`），
  所以现场里看不到它 —— 但它的提交 `1fd8cd4` 还在历史里，因为合并把它接进来了。

### 场景 2：`git log` 检索全家桶（demo 第 1 节）

`git log` 的过滤条件按"过滤对象"分五类：**时间、作者、提交说明、diff 内容、路径**。
它们可以随意叠加，但**语义不在同一层**，混用之前先想清楚自己在过滤哪一层。

#### ① 按时间：`--since` / `--until`（就是 `--after` / `--before`）

**什么情况**：线上上周开始不对劲，你只想看"最近两周"的提交。

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline --since='2 weeks ago' --format='%h %ad %an %s' --date=short
320a44b 2026-10-03 同事甲 fix: 运费按重量加倍 (#205)
30fef87 2026-10-02 同事甲 feat: 给 total_price 加缓存
4a8560c 2026-09-30 学习者 docs: design.md 改名 architecture.md 并补一句
7d8a14d 2026-09-28 同事乙 chore: 删掉调试打印
b7bfb91 2026-09-27 同事乙 wip: 临时调试打印
a616ae6 2026-09-25 学习者 Merge branch 'feature/export' (#103 落地)
7bf241d 2026-09-24 学习者 chore: 主干上的一次普通改动
1fd8cd4 2026-09-23 同事甲 feat: 支持导出 CSV (#103)
45e6d20 2026-09-22 学习者 refactor: 把运费逻辑抄一份到 src/ship.py
3a5c74e 2026-09-20 同事乙 style: 调了一下 count_items 的缩进（纯空白改动）
```

`--since` + `--until` 组一个区间，日期可以写"人话"：

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline --since='2026-01-01' --until='3 weeks ago' --format='%h %ad %s' --date=short
5e88b21 2026-09-13 refactor: retry_policy 和 connect 换个位置（MAX_RETRY 一次没增没减）
930dc06 2026-09-06 fix: 修登录页等级为负时的崩溃 (#102)
79f410b 2026-09-04 feat: 给 net.py 加 MAX_RETRY 的引用 (#101)
1d03ac3 2026-08-30 init: 项目骨架

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline --after='last monday' --format='%h %ad %s' --date=short
320a44b 2026-10-03 fix: 运费按重量加倍 (#205)
30fef87 2026-10-02 feat: 给 total_price 加缓存
4a8560c 2026-09-30 docs: design.md 改名 architecture.md 并补一句
7d8a14d 2026-09-28 chore: 删掉调试打印
```

`last monday`、`2 weeks ago`、`yesterday 00:00`、`2026-01-01` 都能吃 —— git 内置了一套
`approxidate` 日期解析器。**但粒度要小心**：`'yesterday'` = 昨天"这一刻"，不是昨天 0 点。
这条坑在场景 7 ⑤ 有完整实测。

★ `--since` 过滤的是**提交者日期（committer date）**，不是作者日期。
被 rebase 过的提交，作者日期是老的、提交日期是新的 —— 于是"按时间找不到老改动"这件事会真实发生。
要按作者日期筛，只能用 `--format='%ad'` 拿到之后再自己过滤。

#### ② 按人：`--author` 匹配「作者字段」，可以只写一部分

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline --author='同事甲' --format='%h %an %s'
320a44b 同事甲 fix: 运费按重量加倍 (#205)
30fef87 同事甲 feat: 给 total_price 加缓存
1fd8cd4 同事甲 feat: 支持导出 CSV (#103)
d4d56a1 同事甲 refactor: MAX_RETRY 统一改名 ATTEMPTS (#104)
1d03ac3 同事甲 init: 项目骨架

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline --author='yi@example.com' --format='%h %an %s'
7d8a14d 同事乙 chore: 删掉调试打印
b7bfb91 同事乙 wip: 临时调试打印
3a5c74e 同事乙 style: 调了一下 count_items 的缩进（纯空白改动）
5e88b21 同事乙 refactor: retry_policy 和 connect 换个位置（MAX_RETRY 一次没增没减）
79f410b 同事乙 feat: 给 net.py 加 MAX_RETRY 的引用 (#101)

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline --author='甲' --grep='CSV' --all-match --format='%h %an %s'
1fd8cd4 同事甲 feat: 支持导出 CSV (#103)
```

三个必须知道的细节：

1. `--author` 是在 `名字 <邮箱>` 这一整串上做**正则子串匹配**，所以 `甲`、`yi@example.com` 都能命中。
   它**不是**精确等于：写 `张三` 会把 `张三丰 <...>` 一起捞出来。
2. 多个 `--grep` 之间默认是"或"，多个 `--author` 之间也是"或"；
   想让**不同条件相与**必须加 `--all-match`（上面第三条就是"作者含甲"**且**"说明含 CSV"）。
3. `--author` 只匹配**作者（author）**，不匹配提交者（committer）。
   rebase / cherry-pick 之后 committer 变成动手的人、author 保持不变 —— 这正是"功劳记原作者"的机制。

**统计谁提交了多少条**（写周报第一件套）：`git shortlog`

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git shortlog -sne HEAD
     6	学习者 <learn@example.com>
     5	同事乙 <yi@example.com>
     5	同事甲 <jia@example.com>

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 只算最近两周（注意：shortlog 必须显式给一个范围，否则它会去等标准输入）：' && git shortlog -sne HEAD --since='2 weeks ago'
### 只算最近两周（注意：shortlog 必须显式给一个范围，否则它会去等标准输入）：
     4	学习者 <learn@example.com>
     3	同事乙 <yi@example.com>
     3	同事甲 <jia@example.com>
```

字母含义：`-s` 只出条数（summary）、`-n` 按条数从多到少排、`-e` 顺带列出邮箱
（这样"同名不同邮箱"的人能被分开）。
★ **`git shortlog -sne` 后面不给范围（不给 `HEAD`、也不给 `--since`）时，它会去等标准输入，
看起来像卡死了。** 这不是 bug，它设计成可以接管道：`git log --format='%an <%ae>' | git shortlog -sn`。

#### ③ 按改动内容：`-G` 看补丁，`-S` 数次数（本节最值钱的一条）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline -G'MAX_RETRY' --format='%h %s'
d4d56a1 refactor: MAX_RETRY 统一改名 ATTEMPTS (#104)
505349d feat: 多处引用 MAX_RETRY（出现次数变多了）
5e88b21 refactor: retry_policy 和 connect 换个位置（MAX_RETRY 一次没增没减）
79f410b feat: 给 net.py 加 MAX_RETRY 的引用 (#101)
1d03ac3 init: 项目骨架

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline -S'MAX_RETRY' --format='%h %s'
d4d56a1 refactor: MAX_RETRY 统一改名 ATTEMPTS (#104)
505349d feat: 多处引用 MAX_RETRY（出现次数变多了）
79f410b feat: 给 net.py 加 MAX_RETRY 的引用 (#101)
1d03ac3 init: 项目骨架
--- 两边差的那一条（换个位置那次）就是全部区别：-G 命中 5 条、-S 命中 4 条。证据在下面：

$ cd /tmp/tmp.95kGtyLeQ2/inv && git show HEAD~11 --format='%h %s' -- src/net.py
5e88b21 refactor: retry_policy 和 connect 换个位置（MAX_RETRY 一次没增没减）

diff --git a/src/net.py b/src/net.py
index c1c63d4..e2726c9 100644
--- a/src/net.py
+++ b/src/net.py
@@ -1,7 +1,8 @@
+def retry_policy():
+    return MAX_RETRY
+
 def connect(sock):
     for i in range(MAX_RETRY):
         if sock.ok():
             return True
     return False
-def retry_policy():
-    return MAX_RETRY
```

这次提交的 diff 里**确实**有含 `MAX_RETRY` 的 `+` 行和 `-` 行 → `-G` 命中。
但出现的**次数**一次没变（下面这条就是数给你看的）：

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo -n '改之前 MAX_RETRY 出现次数 = ' && git show 'HEAD~12:src/net.py' | grep -c MAX_RETRY && echo -n '改之后 MAX_RETRY 出现次数 = ' && git show 'HEAD~11:src/net.py' | grep -c MAX_RETRY
改之前 MAX_RETRY 出现次数 = 2
改之后 MAX_RETRY 出现次数 = 2
--- 次数没变 → -S 看不见它；diff 里确实有含 MAX_RETRY 的行 → -G 看得见
```

**怎么选**：

- 找"这个符号是谁引进来 / 谁删掉的" → **`-S`**（次数变化 = 增删）。
  查一个 feature flag 的生死、查一个函数还有没有调用者，这是最准的一把刀。
- 找"所有碰过这个字符串的改动，包括改名、移动、重写" → **`-G`**。
- `-G`/`-S` 匹配的是**补丁内容**，**不看提交说明**。要找说明里的字用 `--grep`。

`-S` 默认把参数当**纯文本**，括号就是括号；要用正则得显式打开 `--pickaxe-regex`：

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline -S'range(ATTEMPTS)' --format='%h %s'
d4d56a1 refactor: MAX_RETRY 统一改名 ATTEMPTS (#104)

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline -S'range\(ATTEMPTS\)' --pickaxe-regex --format='%h %s'
d4d56a1 refactor: MAX_RETRY 统一改名 ATTEMPTS (#104)
--- 上面第一条证明：-S 默认按『纯文本』理解，括号就是字符本身；要正则得加 --pickaxe-regex
```

两条给出同一条提交，是因为第一条把 `range(ATTEMPTS)` 当字面量、
第二条把 `\(` 当转义后的字面括号。**如果你不加 `--pickaxe-regex` 却写了正则元字符
（`.` `*` `(` `|`），git 不会报"正则写错了"，它只会安静地按字面量匹配，然后给你 0 条结果。**
"查不到但也不报错"是最难排查的一类问题，这条一定要记住。

顺手看一对"加了又删"的调试语句，`-G` 一次抓两条：

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline -G'debug' --format='%h %s'
7d8a14d chore: 删掉调试打印
b7bfb91 wip: 临时调试打印
```

#### ④ 按提交说明：`--grep`，加 `-E` 用正则，`--all-match` 相与

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline --grep='fix' --format='%h %s'
320a44b fix: 运费按重量加倍 (#205)
930dc06 fix: 修登录页等级为负时的崩溃 (#102)

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline -E --grep='(fix|feat).*#10[0-9]' --format='%h %s'
a616ae6 Merge branch 'feature/export' (#103 落地)
1fd8cd4 feat: 支持导出 CSV (#103)
930dc06 fix: 修登录页等级为负时的崩溃 (#102)
79f410b feat: 给 net.py 加 MAX_RETRY 的引用 (#101)

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline --grep='#' --grep='CSV' --all-match --format='%h %s'
1fd8cd4 feat: 支持导出 CSV (#103)

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline --grep='缓存' --author='甲' --format='%h %an %s'
30fef87 同事甲 feat: 给 total_price 加缓存
```

注意第二条：**合并提交 `a616ae6` 也被捞出来了**，因为它的标题里就写着 `#103`。
复盘需求单时这往往正是你想要的（"是这次合并把 #103 带进主干的"），
但如果你只想要真实改动，加 `--no-merges`。

`--grep` 匹配的是**标题 + 正文**整块，所以"正文里提了一句"的提交也会中。
反过来，**内容改了但说明没写关键字**的提交它一个也抓不到 —— 这就是为什么
"按内容找"必须用 `-S`/`-G`，两者互补，别只用一个。

#### ⑤ 按路径：`--` 之后才是路径

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline -- src/net.py
7d8a14d chore: 删掉调试打印
b7bfb91 wip: 临时调试打印
d4d56a1 refactor: MAX_RETRY 统一改名 ATTEMPTS (#104)
505349d feat: 多处引用 MAX_RETRY（出现次数变多了）
5e88b21 refactor: retry_policy 和 connect 换个位置（MAX_RETRY 一次没增没减）
79f410b feat: 给 net.py 加 MAX_RETRY 的引用 (#101)
1d03ac3 init: 项目骨架

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline -8 --format='%h %s' -- src/ docs/
320a44b fix: 运费按重量加倍 (#205)
30fef87 feat: 给 total_price 加缓存
4a8560c docs: design.md 改名 architecture.md 并补一句
7d8a14d chore: 删掉调试打印
b7bfb91 wip: 临时调试打印
1fd8cd4 feat: 支持导出 CSV (#103)
45e6d20 refactor: 把运费逻辑抄一份到 src/ship.py
3a5c74e style: 调了一下 count_items 的缩进（纯空白改动）
```

**为什么那个 `--` 不能省**：路径名和 ref（分支名 / 标签）可能撞车。
如果仓库里既有个分支叫 `README.md` 又有个文件叫 `README.md`，
`git log README.md` 会被当成"看这个 ref"，`git log -- README.md` 才是"看这个文件"。
`--` = "后面全是路径"的分隔符，写成习惯就好（01 章 `rev-parse` 的报错里也教过这一招）。

第二条还说明一件事：**路径过滤不受 `--first-parent` 约束** ——
`1fd8cd4`（分支内部的提交）照样出现在结果里，因为路径过滤问的是
"这个提交里 `src/` 或 `docs/` 有变化吗"，跟它在哪个分支上写的无关。

#### ⑥ `--follow`：跨改名追一个文件的前世今生

**什么情况**：`git log -- docs/architecture.md` 只给你 1 条，前面的历史像断了一样。

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline -- docs/architecture.md
4a8560c docs: design.md 改名 architecture.md 并补一句

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline --follow -- docs/architecture.md
4a8560c docs: design.md 改名 architecture.md 并补一句
45e6d20 refactor: 把运费逻辑抄一份到 src/ship.py
1d03ac3 init: 项目骨架
```

**为什么会断**：Git **只存"这个提交时刻的目录快照"，从不写"改名日志"**。
改名在对象库里就是"旧路径消失 + 新路径出现"。
`--follow` 是 git 现场做相似度检测，**猜**出来"这其实是同一个文件"。

看清楚它猜的过程（`--name-status` 把每次操作的字母打出来）：

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --follow --name-status --format='%h %s' -- docs/architecture.md
4a8560c docs: design.md 改名 architecture.md 并补一句

R060	docs/design.md	docs/architecture.md
45e6d20 refactor: 把运费逻辑抄一份到 src/ship.py

M	docs/design.md
1d03ac3 init: 项目骨架

A	docs/design.md
```

字母表（排查时读 `--name-status` 天天要用）：

| 字母 | 含义 | 后面那串数字 |
|---|---|---|
| `A` | 新增（Added） | — |
| `M` | 修改（Modified） | — |
| `D` | 删除（Deleted） | — |
| `R` | 改名（Rename） | `R060` = 相似度 60%，越高说明改动越少 |
| `C` | 复制（Copied，要 `--find-copies`） | 同理带百分比 |
| `T` | 类型变了（普通文件 ↔ 符号链接） | — |

`R060` 的意思是"新文件内容只有 60% 像旧文件" —— 这次改名同时补了一句话，相似度才掉到 60。
**相似度阈值默认 50%**，调严了改名就不被识别，`--follow` 会当场断在那里。

★ `--follow` 的三条硬限制：

1. **只能跟一个路径**。`git log --follow -- a.c b.c` 直接报错。
2. **它只沿当前这条线（第一父亲）跟**，跨很多次合并时可能中途跟丢。
3. **它跟的是"内容最像的那个文件"**，两个文件长得一样时能猜错。
   要绝对可靠的改名史，只能靠提交说明里写清楚，或者 `git log --name-status` 人肉读。

#### ⑦ `--first-parent`：只看主干那一列

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline -8 --format='%h %s'
320a44b fix: 运费按重量加倍 (#205)
30fef87 feat: 给 total_price 加缓存
4a8560c docs: design.md 改名 architecture.md 并补一句
7d8a14d chore: 删掉调试打印
b7bfb91 wip: 临时调试打印
a616ae6 Merge branch 'feature/export' (#103 落地)
7bf241d chore: 主干上的一次普通改动
1fd8cd4 feat: 支持导出 CSV (#103)

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline --first-parent -8 --format='%h %s'
320a44b fix: 运费按重量加倍 (#205)
30fef87 feat: 给 total_price 加缓存
4a8560c docs: design.md 改名 architecture.md 并补一句
7d8a14d chore: 删掉调试打印
b7bfb91 wip: 临时调试打印
a616ae6 Merge branch 'feature/export' (#103 落地)
7bf241d chore: 主干上的一次普通改动
45e6d20 refactor: 把运费逻辑抄一份到 src/ship.py
```

**逐行对比只有一处不同**：第 8 行的 `1fd8cd4 feat: 支持导出 CSV (#103)` 变成了 `45e6d20`。
`1fd8cd4` 是 `feature/export` 分支**内部**的提交，它被 `a616ae6` 合并进来了，但它不在主干那一列上。

`--first-parent` 的三个用途：

- **主干历史变成"一列"**，读起来就是"这个仓库上线过什么"，不会被别人分支里的碎提交淹没。
- 合并进来的整坨改动被压缩成一条合并提交；要看内容用 `git show -m <合并>` 或 `git diff <合并>^1 <合并>`。
- 场景 6 里它还有第二个身份：**让 bisect 的数组重新变得单调** —— 这是它最硬的价值。

#### ⑧ `--format`：写周报、做报表全靠它

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git log -3 --format='[%h] 全哈希=%H 作者=%an 邮箱=%ae 作者日期=%ad 提交日期=%cd 距今=%ar 指针=%D 说明=%s'
[320a44b] 全哈希=320a44b5a697a1e34cafc8a78b008762d84e79cd 作者=同事甲 邮箱=jia@example.com 作者日期=Sat Oct 3 10:10:34 2026 +0800 提交日期=Sat Oct 3 10:10:34 2026 +0800 距今=24 hours ago 指针=HEAD -> main 说明=fix: 运费按重量加倍 (#205)
[30fef87] 全哈希=30fef8741e25efd9639acb57daa7127bd99439e6 作者=同事甲 邮箱=jia@example.com 作者日期=Fri Oct 2 10:10:34 2026 +0800 提交日期=Fri Oct 2 10:10:34 2026 +0800 距今=2 days ago 指针= 说明=feat: 给 total_price 加缓存
[4a8560c] 全哈希=4a8560c282cba5e993686dd5175bc7786abbce6b 作者=学习者 邮箱=learn@example.com 作者日期=Wed Sep 30 10:10:34 2026 +0800 提交日期=Wed Sep 30 10:10:34 2026 +0800 距今=4 days ago 指针= 说明=docs: design.md 改名 architecture.md 并补一句

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log -2 --format='提交 %h：%s%n  正文（%b）  父亲们：%p  树对象：%T'
提交 320a44b：fix: 运费按重量加倍 (#205)
  正文（）  父亲们：30fef87  树对象：7475be1f7576510d2fd8814552515f863bc076e8
提交 30fef87：feat: 给 total_price 加缓存
  正文（Co-authored-by: 同事甲 <jia@example.com>
Signed-off-by: 同事乙 <yi@example.com>
）  父亲们：4a8560c  树对象：ba0a56ba96181173facc776fd30f208588d92d19
```

九个占位符一次记全：

| 占位符 | 内容 | 排查时的用途 |
|---|---|---|
| `%H` / `%h` | 完整 / 简短提交哈希 | 抄进故障单一律用 `%H`（短哈希会撞车） |
| `%T` | 它指向的 **tree** 对象哈希 | 01 章的 commit→tree→blob，取证时直接拿快照 |
| `%p` | **所有**父亲的哈希 | 合并提交这里会列出**两个** —— 场景 6 的 `^1`/`^2` 就是这个列表的序号 |
| `%an` / `%ae` | 作者名 / 作者邮箱 | 找人 |
| `%ad` / `%cd` | **作者**日期 / **提交者**日期 | 两者不同 = 被 rebase/cherry-pick 过（见下） |
| `%ar` | 距今（人话，如 `2 days ago`） | 粗略看时效，别拿它做精确判断 |
| `%D` | 指在这个提交上的**指针**（分支/标签/HEAD） | `HEAD -> main` 说明分支头就在这 |
| `%s` / `%b` | 标题 / 正文 | 正文 = 标题之后那一段 |
| `%n` | 换行 | `--format` 默认不给换行，多字段排版全靠它 |

两个值得停下来看的细节：

1. **`%p` 在合并提交上会给两个哈希**（场景 6 里那行 `Merge: e7a8ed1 cdec745` 是同一件事的另一面）。
   `HEAD^1` / `HEAD^2` 就是按这个列表数的，
   顺序永远是"**你合并当时所在那条线的父亲在前，被合进来的那个在后**"。
2. **`%ad` 和 `%cd` 相同**说明这段历史没被改写过。
   如果你在 log 里看到 `作者日期=2026-09-30 / 提交日期=2026-10-04`，
   那说明这条提交在 10 月 4 号被人 rebase / cherry-pick / amend 过（作者日期是原样继承的）。
   **"这段历史被人重写过了"最直接的取证方法就是比较这两个日期。**

trailer（提交正文末尾那几行 `键: 值`，`Signed-off-by` / `Co-authored-by` / `Fixes:` 都算）有专门的占位符：

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 只有带 trailer 的那次提交才看得出效果（这里看 HEAD~1）：' && git log -1 --format='%h %s%n--- trailers ---%n%(trailers:only=true)' HEAD~1
### 只有带 trailer 的那次提交才看得出效果（这里看 HEAD~1）：
30fef87 feat: 给 total_price 加缓存
--- trailers ---
Co-authored-by: 同事甲 <jia@example.com>
Signed-off-by: 同事乙 <yi@example.com>
```

`%(trailers:only=true)` = 只打 trailer、不打正文其余部分。
也能用 `--grep='Co-authored-by'` 捞，但那会把正文里普通提到这句话的提交一起捞出来，不精确。
要**写** trailer 用 `git interpret-trailers`，要**读**就上面这套。

#### ⑨ `--numstat`：机器可读的行数报表（以及它的空格坑）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git log -3 --numstat --format='COMMIT %h %s'
COMMIT 320a44b fix: 运费按重量加倍 (#205)

1	1	src/cart.py
COMMIT 30fef87 feat: 给 total_price 加缓存

1	0	src/cart.py
COMMIT 4a8560c docs: design.md 改名 architecture.md 并补一句

2	0	docs/{design.md => architecture.md}
```

三列，**用 TAB 分隔**：`新增行数`、`删除行数`、`文件名`。
`--format='COMMIT %h %s'` 是自己造的标题行（默认 `--numstat` 只给 git 那一大块 `commit …` 头，脚本不好切）；
`--pretty=tformat:` 则是**把标题行清空**，只留纯数据行，管道里最干净。

★ **上面那段输出里就藏着本章最阴的一个坑**：
`4a8560c` 那次改名的提交，路径被 git 缩写成 `docs/{design.md => architecture.md}` ——
**中间有两个空格**。你按空格切列，第三列就变成了 `docs/{design.md`：

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 每个文件总共被加/删了多少行：' && git log --pretty=tformat: --numstat | awk '{a[$3]+=$1; d[$3]+=$2} END {for (f in a) printf "%-18s +%s -%s\n", f, a[f], d[f]}' | sort
### 每个文件总共被加/删了多少行：
README.md          +5 -0
docs/design.md     +4 -1
docs/{design.md    +2 -0
src/calc.c         +12 -0
src/cart.py        +29 -3
src/config.py      +1 -0
src/net.py         +19 -7
src/ship.py        +5 -0
--- 看上面那行 docs/{design.md —— 这就是 numstat 的坑：改名会被写成 docs/{a => b}，中间有空格，按空格切列就断了
```

统计口径当场就脏了 —— 而且**一声不吭**。三条解法，第一条最省事：

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 想按列统计，就关掉改名检测（或者用 -z 自己解析）：' && git log --pretty=tformat: --numstat --no-renames | awk '{a[$3]+=$1; d[$3]+=$2} END {for (f in a) printf "%-18s +%s -%s\n", f, a[f], d[f]}' | sort
### 想按列统计，就关掉改名检测（或者用 -z 自己解析）：
README.md          +5 -0
docs/architecture.md +5 -0
docs/design.md     +4 -4
src/calc.c         +12 -0
src/cart.py        +29 -3
src/config.py      +1 -0
src/net.py         +19 -7
src/ship.py        +5 -0
```

`--no-renames` 之后路径里不再有 `=>`，列就齐了。
顺带解释数字为什么变：改名检测开着时 `design.md → architecture.md` 只算 `+2 -0`（只算差异）；
关掉之后它被算成"旧文件整个删掉 `-4` + 新文件整个加上 `+5`"，
于是 `docs/design.md` 从 `+4 -1` 变成 `+4 -4`，`docs/architecture.md` 冒出 `+5 -0`。
**结论：`--no-renames` 会把改名算成"整个重写"，做代码量统计时口径必须写清楚。**

另外两种正规解法：

- **`-z`**：`git log --numstat -z`，字段之间用 **NUL** 分隔，
  文件名里带空格、甚至带换行都不会断。解析稍麻烦，但绝对可靠。
- 在 awk 里用**列数守卫**（见下一条命令的做法），把不像样行直接丢掉。

**按人统计行数**（注意它是怎么防住上面那个坑的）：

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 每个人写了多少行（新增+删除，合并提交不算）：' && git log --no-merges --pretty=tformat:'AUTHOR %an' --numstat | awk '/^AUTHOR /{w=$2;next} NF==3{t[w]+=$1+$2} END{for (k in t) printf "%-8s %d\n", k, t[k]}' | sort -k2 -rn
### 每个人写了多少行（新增+删除，合并提交不算）：
同事甲 54
学习者 20
同事乙 12
```

拆解这条管道，**每一段都有存在理由，少一段口径就不对**：

| 片段 | 为什么必须有 |
|---|---|
| `--no-merges` | 合并提交会把别人所有的行数都记到"合并那个人"头上，数字立刻失真 |
| `--pretty=tformat:'AUTHOR %an'` | 造一个"下面这批 numstat 属于谁"的分隔行 |
| `--numstat` | 给出 `加 删 文件` 三列 |
| `awk '/^AUTHOR /{w=$2;next}` | 遇到分隔行就更新"当前作者"，然后跳过 |
| `NF==3{t[w]+=$1+$2}` | **`NF==3` 是关键的守卫**：只数正好三列的行，于是那个被切成 5 列的改名行被自动排除，宁可少算也不误算 |
| `sort -k2 -rn` | 按第二列数字倒序 |

**中英对照**（你手敲时终端很可能是中文；本章 demo 是英文）

| demo 里的英文（真实原文） | 本机 `LANGUAGE=zh_CN git …` 探到的中文 |
|---|---|
| `fatal: no upstream configured for branch 'tmp/scratch'` | `致命错误：尚未给分支 'main' 设置上游`（分支名随你的现场变） |
| `Deleted branch secret/大改造 (was 4f48d37).` | `已删除分支 secret/x（原为 0f6d405）。` —— 措辞是「原为」，不是「曾为」 |
| `dangling commit <哈希>`（fsck） | `悬空 commit <哈希>` —— 注意只翻了一半，commit 这个词留着 |
| `fatal: git cat-file: could not get object info` | 这句在我的 git 2.55 里**没有翻**，中文环境下照样是英文 |
| `fatal: your current branch 'main' does not have any commits yet` | `致命错误：您的当前分支 'main' 尚无任何提交` |

⚠ 右列**不是本章 demo 的输出**：demo 用 `LC_ALL=C` 强制英文。右列是我另外在
`mktemp -d` 沙箱里用 `LANGUAGE=zh_CN LC_ALL=zh_CN.UTF-8 git …` 逐条探出来的（哈希是
那次探针自己的现场，和 demo 无关）。你机器上有没有中文，取决于装没装 git 的中文语言包。


### 场景 3：不动工作区就能看清「那一次改了什么」——`git show` / `git diff` 的目标写法（demo 第 2 节）

**什么情况**：范围已经缩到两三条提交，接下来要"看内容"。此时最大的风险不是看不懂，
而是手滑 —— 想看一眼三个月前的某个文件，先把分支切过去，回来发现工作区一片红。
这一节的所有命令**都是只读的**：不 checkout、不改暂存区、不碰工作区一个字节。
看懂它们的口诀只有一句话：**git 眼里"版本"是个地址，`<版本>:<路径>` 就是这个地址里的门牌号。**

**敲哪几条**

```bash
git log --oneline --format='%h %ad %s' --date=short -- src/calc.c   # 这个文件被哪几次动过
git show 'HEAD~13:src/calc.c'                                       # 某个历史版本里的这个文件长啥样
git diff 'HEAD~13:src/calc.c' 'HEAD:src/calc.c'                     # 只比这一个文件，两个版本
git show HEAD --format='%h %s' -- src/cart.py                       # 某一次提交只对这个文件干了啥
git diff HEAD~2 HEAD                                                # 两个提交之间（= A..B）
git diff HEAD~3...HEAD                                              # 共同祖先 → HEAD（评审分支用）
git diff 'HEAD^!'                                                   # 只看这一个提交自己引入的改动
```

**真实输出**（① 先看 `src/calc.c` 这个文件被哪两次提交动过，再看 30 天前它长什么样）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline --format='%h %ad %s' --date=short -- src/calc.c
930dc06 2026-09-06 fix: 修登录页等级为负时的崩溃 (#102)
1d03ac3 2026-08-30 init: 项目骨架

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 30 天前那次提交（HEAD~13）里的 src/calc.c：' && git show 'HEAD~13:src/calc.c'
### 30 天前那次提交（HEAD~13）里的 src/calc.c：
#include <stdio.h>
/* 把等级换算成奖励分 */
int bonus(int level) {
    int v = level * 10;
    return v;
}

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 现在（HEAD）的 src/calc.c：' && git show 'HEAD:src/calc.c'
### 现在（HEAD）的 src/calc.c：
#include <stdio.h>
/* 把等级换算成奖励分 */
int bonus(int level) {
    int v = level * 10;
    return v;
}

/* 修一下负数等级导致的崩溃 */
int safe_bonus(int level) {
    if (level < 0) return 0;
    return bonus(level);
}
```

`git show 'HEAD~13:src/calc.c'` 里那个**冒号**是全部的关键：
冒号左边是"版本"，右边是"那个版本里的路径"。少了冒号就变成另一个意思（`git show HEAD~13` 是看那次提交的补丁）。
顺带注意一件事：`HEAD~13` 和 `HEAD` 两个版本的 `calc.c` **并不相同**，但 `git show 'HEAD:src/calc.c'`
和 `git show 'HEAD~13:src/calc.c'` 的差别只在文件末尾多了 6 行 —— 这正是第 1 节里 `930dc06`（`fix: 修登录页等级为负时的崩溃`）
加进去的 `safe_bonus`。**用 `:路径` 看快照，用 `diff A:路径 B:路径` 看差别**，两步就把"谁改的、改了什么"钉死了。

**真实输出**（② 一次提交只对某一个文件的效果；③ 那个"换个位置"的提交）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git show HEAD --format='%h %s' -- src/cart.py
320a44b fix: 运费按重量加倍 (#205)

diff --git a/src/cart.py b/src/cart.py
index b8a35d9..6ed1371 100644
--- a/src/cart.py
+++ b/src/cart.py
@@ -16,7 +16,7 @@ def shipping_fee(weight, zone):  # 这段运费逻辑已抄到 src/ship.py
     base = 8
     if zone > 3:
         base = base + 5
-    return base + weight
+    return base + weight * 2
 
 cache = {}
 def gift_wrap(items):

$ cd /tmp/tmp.95kGtyLeQ2/inv && git show HEAD~11 --stat --format='%h %s'
5e88b21 refactor: retry_policy 和 connect 换个位置（MAX_RETRY 一次没增没减）

 src/net.py | 5 +++--
 1 file changed, 3 insertions(+), 2 deletions(-)
```

第一段是**排查时最有用的姿势**：`git show <提交> -- <路径>`，标题先打印一次（`--format`），
下面只有这个文件的补丁，别的一个字都不给你看。第二段的 `5e88b21` 就是第 1 节里
`-S` 抓不到、`-G` 能抓到的那条"把两个函数上下换位置"，`--stat` 显示 `5 +++--`：
**动了 3 行、删了 2 行，但净增删为 1** —— 搬家类提交在统计里最容易骗人，看到 `--stat` 有增有减，
就点开 diff 确认一下，别只信行数。

**真实输出**（④ 直接比两个版本里的同一个文件，跳过"提交"这一层）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git diff 'HEAD~13:src/calc.c' 'HEAD:src/calc.c'
diff --git a/src/calc.c b/src/calc.c
index 10d8174..aa93cb9 100644
--- a/src/calc.c
+++ b/src/calc.c
@@ -4,3 +4,9 @@ int bonus(int level) {
     int v = level * 10;
     return v;
 }
+
+/* 修一下负数等级导致的崩溃 */
+int safe_bonus(int level) {
+    if (level < 0) return 0;
+    return bonus(level);
+}
```

**为什么是这几条**：`git diff A B` 比的是**两份快照**，`git show <提交>` 看的是**一次动作**。
排查时两个都要：先用 `show` 确认"这次提交干了什么"，再用 `diff 旧版本 新版本` 确认
"从那时到现在，这个文件一共漂移到什么样子"。后者不受中间提交数量影响，最适合回答
"上线的那个版本和现在的版本差在哪"。

**真实输出**（⑤ 三张区的 diff：工作区 / 暂存区 / HEAD —— 这几条命令也是理解全 Git 的地基）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && printf '临时改动\n' >> src/net.py && echo '### 工作区 vs 暂存区（git diff 默认）：' && git diff --stat
### 工作区 vs 暂存区（git diff 默认）：
 src/net.py | 1 +
 1 file changed, 1 insertion(+)

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 暂存区 vs HEAD（--staged，等价 --cached）：' && git add src/net.py && git diff --staged --stat
### 暂存区 vs HEAD（--staged，等价 --cached）：
 src/net.py | 1 +
 1 file changed, 1 insertion(+)

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 工作区+暂存区 vs HEAD：' && git diff HEAD --stat
### 工作区+暂存区 vs HEAD：
 src/net.py | 1 +
 1 file changed, 1 insertion(+)

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 收尾：把刚才的假改动清干净（第 03 章细讲）' && git reset -q && git checkout -- src/net.py && git status --short && echo '(status 输出为空 = 干净了)'
### 收尾：把刚才的假改动清干净（第 03 章细讲）
(status 输出为空 = 干净了)
```

把这三行背下来，`git diff` 从此不再靠猜：

| 敲什么 | 比的是哪两张区 | 记住的口诀 |
|---|---|---|
| `git diff` | 工作区 vs 暂存区 | "我还没 add 的是什么" |
| `git diff --staged`（= `--cached`） | 暂存区 vs HEAD | "我 add 了、还没 commit 的是什么" |
| `git diff HEAD` | 工作区+暂存区 vs HEAD | "和上次提交比，我总共动了什么" |

上面 demo 里三次输出看起来一样，是因为那次假改动**只有一行、而且 add 前后内容没变**；
这恰好是最容易误判的地方 —— **别用"输出一样"去推断"两种写法等价"**，
`add` 之后再改一次那个文件，三者立刻不同。收尾那条 `git reset -q && git checkout -- src/net.py`
把假改动清干净（细节在第 03 章）。

**真实输出**（⑥ `A B` / `A..B` / `A...B` / `^!` 四种目标写法）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 两个提交之间：A B 和 A..B 是一回事' && git diff --stat HEAD~2 HEAD
### 两个提交之间：A B 和 A..B 是一回事
 src/cart.py | 3 ++-
 1 file changed, 2 insertions(+), 1 deletion(-)

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 三个点 A...B = 从它俩的共同祖先到 B（评审分支时用这个，不算别人合进来的改动）：' && git diff --stat HEAD~3...HEAD
### 三个点 A...B = 从它俩的共同祖先到 B（评审分支时用这个，不算别人合进来的改动）：
 docs/{design.md => architecture.md} | 2 ++
 src/cart.py                         | 3 ++-
 2 files changed, 4 insertions(+), 1 deletion(-)

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### HEAD^! = 只看这一个提交自己引入的改动（不累计父辈）：' && git diff --stat 'HEAD^!'
### HEAD^! = 只看这一个提交自己引入的改动（不累计父辈）：
 src/cart.py | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)
```

- `git diff A B` 和 `git diff A..B` **完全一回事**（diff 命令里两个点不是"范围"的意思，范围是 log 家族的事）。
- `git diff A...B` 三个点 = **先找 A 和 B 的共同祖先（merge base），再比"祖先 → B"**。
  这在做代码评审时是**唯一正确**的写法：分支从你切出去之后主线又合了别人的东西，
  用两个点会把别人合进来的改动也算到你头上，三个点只看"这条分支自己带来的净改动"。
  看上面输出里的 `docs/{design.md => architecture.md}` —— 改名被 git 缩写成花括号形式，
  第 1 节 ⑨ 讲过它怎么把按空格切列的脚本弄断。
- `git diff 'HEAD^!'` = "只看 HEAD 这一次提交自己引入的改动，不累计父辈"。
  对普通提交等价于 `git diff HEAD^ HEAD`；对**合并提交**才有额外价值：它拿第一父亲做基准，
  于是你看到的是"这次合并从分支那边带进来了什么"。注意 `^!` 一定要加引号，
  bash 会把 `!` 当成历史展开（尤其是交互 shell 里双引号内），报了 `event not found` 就是它。

**常见翻车**

- **`git show 'HEAD~13:src/calc.c'` 不加引号**：`{}`、`!`、`**` 这类字符在部分 shell 里会被抢走，
  养成"rev 表达式一律加单引号"的习惯（本章 demo 全部这么写）。
- **想用 `git diff A B -- <路径>` 看单文件，结果用了 `git diff A B <路径>`**：少了 `--`，
  路径名一旦和分支/标签重名，git 就先按引用解析它，报错内容还特别误导人。
- **拿 `git diff` 去看历史版本**：`git diff HEAD~13 src/calc.c` 这种写法**不合法**，
  要么 `git diff 'HEAD~13:src/calc.c' 'HEAD:src/calc.c'`，要么 `git show 'HEAD~13:src/calc.c'`。
- **看快照时踩到"路径不存在"**：那次改名之前 `architecture.md` 根本不存在，
  `git show 'HEAD~13:docs/architecture.md'` 会报 fatal；用 `--follow` 先确认当时它叫什么。

---

### 场景 4：`git blame` —— 这一行到底是谁、哪次提交、什么时候写的（demo 第 3 节）

**什么情况**：手里只有一行可疑代码，连范围都没有。`blame` 是"从代码行反查提交"的唯一入口。
但它有个几乎所有人都会误解的地方：**它报的是"最后动过这一行的人"，不是"这行想法最早是谁的"**。
这一节把 blame 的六个开关（`-L` / `-w` / `-C` / 哈希前缀 `^` / `--porcelain` / `--since`）串成一条完整的追查链。

**敲哪几条**

```bash
git blame src/cart.py                  # 整份文件，每行前面挂上「提交 作者 日期 行号」
git blame -L 9,13 src/cart.py          # 只看 9~13 行（大文件必须配 -L，否则慢得离谱）
git blame -w -L 9,11 src/cart.py       # 忽略空白改动：把被"只调缩进"抢功的行还给原作者
git blame -C src/ship.py               # 跨文件追"这段是抄哪儿的"
git log -1 --format='%h | %an | %ad | %s' <哈希>   # 拿到哈希之后，看它到底是什么
git log -p -1 <哈希> -- src/cart.py                 # 再看它的 diff —— 这一步才叫理解意图
git blame -L 15,15 --porcelain src/cart.py          # 脚本要用它：字段一行一个，好解析
```

**真实输出**（① 整份 `src/cart.py`，26 行全贴，一行不省）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git blame src/cart.py
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800  1) def total_price(items, discount):
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800  2)     subtotal = 0
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800  3)     for it in items:
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800  4)         subtotal += it.price
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800  5)     if discount < 0:
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800  6)         raise ValueError('bad discount')
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800  7)     return subtotal * (1 - discount)
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800  8) 
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800  9) def count_items(items):
3a5c74e1 (同事乙 2026-09-20 10:10:34 +0800 10) 	    n = 0
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 11)     for it in items:
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 12)         n += 1
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 13)     return n
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 14) 
45e6d201 (学习者 2026-09-22 10:10:34 +0800 15) def shipping_fee(weight, zone):  # 这段运费逻辑已抄到 src/ship.py
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 16)     base = 8
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 17)     if zone > 3:
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 18)         base = base + 5
320a44b5 (同事甲 2026-10-03 10:10:34 +0800 19)     return base + weight * 2
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 20) 
30fef874 (同事甲 2026-10-02 10:10:34 +0800 21) cache = {}
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 22) def gift_wrap(items):
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 23)     return 3 * len(items)
1fd8cd46 (同事甲 2026-09-23 10:10:34 +0800 24) 
1fd8cd46 (同事甲 2026-09-23 10:10:34 +0800 25) def export_csv(rows):
1fd8cd46 (同事甲 2026-09-23 10:10:34 +0800 26)     return ",".join(rows)
```

**怎么读这一列**：`提交哈希(作者 日期 行号) 代码`。三件事立刻能看出来：

- 大量行挂在 `^1d03ac3` 上，**开头的 `^` 表示"追到了这次追溯的边界"**（默认是仓库的根提交）。
  `^` **不等于**"这就是作者"，它只说明"在我们看得见的历史里，这行没被人动过"。
  浅克隆（`--depth`）里边界是浅边界，`^` 出现得更多，这时候 blame 特别容易冤枉人。
- 第 10 行 `3a5c74e1 (同事乙 …) <TAB>    n = 0` —— 这行看起来是"同事乙改了内容"，
  其实那次提交只是把缩进从空格换成 Tab（`style: 调了一下 count_items 的缩进`），下一段 `-w` 立刻把它还原。
- 第 19 行 `320a44b5 (同事甲 2026-10-03 …)` 就是本次的"案发现场"：`return base + weight * 2`。
  第 24~26 行挂着 `1fd8cd4`，那是 `feature/export` 分支里的提交，被合并带进来的 ——
  blame 不区分"主干/分支"，它只看这行的历史链。

**真实输出**（② `-L` 只看一段；③ `-w` 把空白改动剔掉）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git blame -L 9,13 src/cart.py
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800  9) def count_items(items):
3a5c74e1 (同事乙 2026-09-20 10:10:34 +0800 10) 	    n = 0
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 11)     for it in items:
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 12)         n += 1
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 13)     return n
--- ③ -w：忽略空白改动。第 10 行的缩进被那次 style 提交动过，所以默认 blame 把它算给了那次提交

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 不加 -w：' && git blame -L 9,11 src/cart.py && echo '### 加 -w（回到真正写这行内容的人）：' && git blame -w -L 9,11 src/cart.py
### 不加 -w：
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800  9) def count_items(items):
3a5c74e1 (同事乙 2026-09-20 10:10:34 +0800 10) 	    n = 0
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 11)     for it in items:
### 加 -w（回到真正写这行内容的人）：
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800  9) def count_items(items):
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 10) 	    n = 0
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 11)     for it in items:
```

对比非常直白：同一个第 10 行，不加 `-w` 归给 `3a5c74e`（调缩进），加了 `-w` 归回 `^1d03ac3`（最初写它的人）。
**所以查"这行逻辑谁设计的"要加 `-w`；查"谁把格式搞乱了"要用默认的 blame。**
其他几个减少噪音的开关：`-b`（忽略行尾空白）、`-M`（认同一文件内的移动）、`-C`（认跨文件的移动）、
`-CC`（跨文件移动还检查目标文件的其他位置，最激进也最慢）、`-B`（拆开"移动+改动"的提交）。
如果团队里有人做了大范围格式化，把那次提交哈希写进仓库根的 `.git-blame-ignore-revs`，
然后 **在 CI 里** `git -c blame.ignoreRevsFile=.git-blame-ignore-revs blame …` ——
注意 `git config blame.ignoreRevsFile` 是仓库级配置，别在别人的仓库里随手写全局配置。

**真实输出**（④ `-C`：`src/ship.py` 整份是从 `cart.py` 抄的，加 `-C` 才看得见出生地）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 不加 -C：五行全归给抄的那次提交' && git blame src/ship.py
### 不加 -C：五行全归给抄的那次提交
45e6d201 (学习者 2026-09-22 10:10:34 +0800 1) def shipping_fee(weight, zone):
45e6d201 (学习者 2026-09-22 10:10:34 +0800 2)     base = 8
45e6d201 (学习者 2026-09-22 10:10:34 +0800 3)     if zone > 3:
45e6d201 (学习者 2026-09-22 10:10:34 +0800 4)         base = base + 5
45e6d201 (学习者 2026-09-22 10:10:34 +0800 5)     return base + weight

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 加 -C：文件名那一列变成了 cart.py —— 那才是代码的出生地' && git blame -C src/ship.py
### 加 -C：文件名那一列变成了 cart.py —— 那才是代码的出生地
^1d03ac3 src/cart.py (同事甲 2026-08-30 10:10:34 +0800 1) def shipping_fee(weight, zone):
^1d03ac3 src/cart.py (同事甲 2026-08-30 10:10:34 +0800 2)     base = 8
^1d03ac3 src/cart.py (同事甲 2026-08-30 10:10:34 +0800 3)     if zone > 3:
^1d03ac3 src/cart.py (同事甲 2026-08-30 10:10:34 +0800 4)         base = base + 5
^1d03ac3 src/cart.py (同事甲 2026-08-30 10:10:34 +0800 5)     return base + weight
```

**这两段是 `blame` 最容易被误读的地方**：不加 `-C`，五行全归给"抄的那次提交"（`45e6d20`，学习者），
看起来像学习者写了一个运费函数 —— 加 `-C`，**输出多出一列文件名 `src/cart.py`**，
哈希全部变回 `^1d03ac3`：真正写这段代码的是 `init` 那次提交的同事甲。
所以 blame 的输出**带文件名列 = 代码来自别的文件**，这是 `-C`/`-CC` 才有的信号。
反过来，看到"一整块代码同一次提交突然出现"，先怀疑是搬家/复制，再下结论。

**真实输出**（⑤ 完整追查链：blame 拿到哈希 → `log -1` 看是什么 → `log -p` 看为什么）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git blame -L 19,19 src/cart.py
320a44b5 (同事甲 2026-10-03 10:10:34 +0800 19)     return base + weight * 2

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo "第 19 行出自提交 320a44b5a697a1e34cafc8a78b008762d84e79cd，先看它是什么：" && git log -1 --format='%h | %an | %ad | %s' --date=short "320a44b5a697a1e34cafc8a78b008762d84e79cd"
第 19 行出自提交 320a44b5a697a1e34cafc8a78b008762d84e79cd，先看它是什么：
320a44b | 同事甲 | 2026-10-03 | fix: 运费按重量加倍 (#205)

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 再看它完整的 diff（这一步才叫理解意图）：' && git log -p -1 "320a44b5a697a1e34cafc8a78b008762d84e79cd" -- src/cart.py
### 再看它完整的 diff（这一步才叫理解意图）：
commit 320a44b5a697a1e34cafc8a78b008762d84e79cd
Author: 同事甲 <jia@example.com>
Date:   Sat Oct 3 10:10:34 2026 +0800

    fix: 运费按重量加倍 (#205)

diff --git a/src/cart.py b/src/cart.py
index b8a35d9..6ed1371 100644
--- a/src/cart.py
+++ b/src/cart.py
@@ -16,7 +16,7 @@ def shipping_fee(weight, zone):  # 这段运费逻辑已抄到 src/ship.py
     base = 8
     if zone > 3:
         base = base + 5
-    return base + weight
+    return base + weight * 2
 
 cache = {}
 def gift_wrap(items):
```

**为什么是这几条**：`blame` 只回答"哪一次"，**永远不回答"为什么"**。
"为什么"只有两个地方有：提交说明里的 `(#205)` 这种需求单号，以及 diff 本身。
上面这条链 —— `blame -L` → `log -1 --format` → `log -p -1 … -- 路径` —— 就是本章的核心动作，
三步之内从"一行可疑代码"走到"这次改动的意图和影响范围"。
顺带留意 `git log -p -1 <哈希> -- src/cart.py` 的 diff 头：
`@@ -16,7 +16,7 @@ def shipping_fee(weight, zone):  # 这段运费逻辑已抄到 src/ship.py`
`@@` 后面那串是 **hunk 上下文行**（git 从被改文件附近取一行当"锚点"，不是函数名解析），
所以你在 hunk 头里看到"`已抄到 src/ship.py`"这种注释是很正常的。

**真实输出**（⑥ 最容易被误解的一点：改名之后，第 1 行的"主人"换了）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 看 docs/architecture.md 第 1 行：内容来自 init 时同事甲写的，只是有人加了个尾巴，整行就换了主人：' && git blame -L 1,3 docs/architecture.md
### 看 docs/architecture.md 第 1 行：内容来自 init 时同事甲写的，只是有人加了个尾巴，整行就换了主人：
45e6d201 docs/design.md (学习者 2026-09-22 10:10:34 +0800 1) # 设计说明（已补充）
^1d03ac3 docs/design.md (同事甲 2026-08-30 10:10:34 +0800 2) 
^1d03ac3 docs/design.md (同事甲 2026-08-30 10:10:34 +0800 3) 模块划分：calc / net / cart。

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 这一行真正的原文（改名之前、HEAD~8 那个版本里的 docs/design.md）：' && git show 'HEAD~8:docs/design.md'
### 这一行真正的原文（改名之前、HEAD~8 那个版本里的 docs/design.md）：
# 设计说明

模块划分：calc / net / cart。
```

`45e6d201 docs/design.md (学习者 …) 1) # 设计说明（已补充）` —— 三个信息一次给全：
提交、**旧路径** `docs/design.md`（第二列！）、行号。内容是同事甲 `init` 时写的，
学习者只是在尾巴上加了"（已补充）"，这一行的 blame 就换人了。

**那能不能既承认"动过"又找到"最初的人"？** 能，blame 默认已经在跟改名了，剩下的是组合拳：

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 好消息：blame 默认会跟随改名（输出里第二列就是旧路径），组合拳是 log --follow 找旧路径 → blame <旧提交> -- <旧路径>：' && git log --follow --name-status --format='%h %s' -- docs/architecture.md
### 好消息：blame 默认会跟随改名（输出里第二列就是旧路径），组合拳是 log --follow 找旧路径 → blame <旧提交> -- <旧路径>：
4a8560c docs: design.md 改名 architecture.md 并补一句

R060	docs/design.md	docs/architecture.md
45e6d20 refactor: 把运费逻辑抄一份到 src/ship.py

M	docs/design.md
1d03ac3 init: 项目骨架

A	docs/design.md

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 用旧路径 + 改名之前的提交，原作者就回来了：' && git blame 'HEAD~8' -- docs/design.md
### 用旧路径 + 改名之前的提交，原作者就回来了：
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 1) # 设计说明
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 2) 
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 3) 模块划分：calc / net / cart。
```

`git log --follow --name-status` 那段输出里 `R060` 是关键：**R = Rename**，
`060` 是相似度 60%（低于这个阈值 git 就认为是"删一个 + 加一个"，`--follow` 也就断了）。
拿到旧路径 `docs/design.md` 之后，`git blame 'HEAD~8' -- docs/design.md`（旧提交 + 旧路径）
把三行全部还给 `^1d03ac3`，原作者回来了。

**真实输出**（⑦ 边界标记 `^`、机器可读的 `--porcelain`、`--since` 造边界）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### ^ 的样子（第一列哈希带 ^ = 追到了边界提交）：' && git blame -L 1,1 src/cart.py
### ^ 的样子（第一列哈希带 ^ = 追到了边界提交）：
^1d03ac3 (同事甲 2026-08-30 10:10:34 +0800 1) def total_price(items, discount):

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 要脚本处理就用 --porcelain（第一条记录里有 commit/author/time/边界等机器可读字段）：' && git blame -L 15,15 --porcelain src/cart.py | sed -n '1,5p'
### 要脚本处理就用 --porcelain（第一条记录里有 commit/author/time/边界等机器可读字段）：
45e6d2015d676609948df7e7ae8b7570b29dceea 15 15 1
author 学习者
author-mail <learn@example.com>
author-time 1790043034
author-tz +0800

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 只想看最近两周谁动过这两行：--since 直接把更早的历史当边界：' && git blame --since='2 weeks ago' -L 15,16 src/cart.py
### 只想看最近两周谁动过这两行：--since 直接把更早的历史当边界：
45e6d201 (学习者 2026-09-22 10:10:34 +0800 15) def shipping_fee(weight, zone):  # 这段运费逻辑已抄到 src/ship.py
^3a5c74e (同事乙 2026-09-20 10:10:34 +0800 16)     base = 8
```

`--porcelain` 第一条记录 `45e6d2015d676609948df7e7ae8b7570b29dceea 15 15 1`：
**全哈希 / 这条记录对应的原文件行号 / 结果文件行号 / 该行的记录条数**，
紧跟着 `author`、`author-mail`、`author-time`、`author-tz` 一行一个字段 ——
写报表只认这个格式，别去切默认输出的空格（默认输出里作者名带空格就断了）。
`--since='2 weeks ago'` 那一跑很值得注意：**第 15 行照常报出提交，第 16 行却变成 `^3a5c74e`** ——
`--since` 是给 blame **造一个追溯边界**，边界之前的历史一概不看，于是老代码全被打上 `^`。
它可以用来"只查最近谁动过"，**绝不能**当成"这些行的来源都确定了"。

**常见翻车**

- **对整份大文件跑 blame**：它要给**每一行**追一条历史线，几千行的文件能跑到几十秒。
  先 `-L`，或者 `-L /正则/,+20`（`git blame -L '/^def shipping_fee/,/^$/' src/cart.py` 也支持）。
- **把 blame 当"功劳簿"**：`blame` 是"改动归属"，不是"作者归属"。
  要论功，看 `git log --format='%an' | sort | uniq -c` 或者 `git shortlog -sne`（场景 2 ②）。
- **`^` 当成"不知道"**：`^` 只表示"追到边界"。在浅克隆里先 `git fetch --unshallow`，
  否则 blame 的结论没意义。
- **改名/搬家后不看第二列**：默认输出没有文件名列，一旦加了 `-C` 才有。
  看到"整块代码同一提交出现"却没有人名列，基本可以断定是**同文件搬家或复制**，
  这时候补 `-C`（甚至 `-CC -M -B`）再下结论。
- **想跨文件追却忘了 `-C` 只跟"移动/复制"**：如果对方是**手抄重写**（改了几个变量名），
  相似度不够，`-C` 也追不过去，只能回 `log -S`（场景 2 ③）。

---

### 场景 5：`git bisect` —— 24 个提交里问 5 次揪出真凶（demo 第 4 节）

**什么情况**：功能坏了，但没人知道是哪次提交坏的；手工从后往前切分支测，24 个版本要测到明天。
`bisect` 把这件事变成数学题：只要"好/坏"在时间上单调（一旦坏了后面全坏），二分就行，
`log2(23) ≈ 4.6` → **实测正好问 5 次**。这一节同时给出**手工版**和**自动版**，
因为自动版要求你先把"好坏"写成退出码 —— 这一步才是 bisect 真正的门槛。

**判定脚本的退出码约定**（`bisect-check.sh` 顶部注释就是这个，背下来）

| 退出码 | 含义 | 典型场景 |
|---|---|---|
| `0` | 这个版本**好** | 测试通过 |
| `1`~`49` | 这个版本**坏** | 断言失败（注意 49 以上不算坏） |
| `125` | **这个版本测不了，跳过** | 编译不过、依赖缺失 |
| 其它（`126`/`127`…） | **出事了，bisect 立刻停** | 脚本没 `chmod +x`、找不到文件 |

**敲哪几条**

```bash
./test.sh; echo "退出码 = $?"                 # 先自己确认"现在确实是坏的"
git bisect start HEAD v1.0                    # bad 在前（现在的版本），good 在后（已知好的标签）
git bisect good / git bisect bad              # 手工版：每切一次跑一次测试，标一次
git bisect terms                              # 当前两端记成了什么
git bisect log                                # 整个决策过程，能复制下来当笔记
git bisect run ./test.sh                      # 自动版：一路问到答案
git bisect reset                              # 一定要收尾：回到二分前的位置，清掉 refs/bisect/*
```

**真实输出**（① 现场：24 个提交；先自己跑一次判定脚本看退出码）

```text
$ cd /tmp/tmp.95kGtyLeQ2/bis && echo -n '提交总数 = ' && git rev-list --count HEAD && git log --oneline | sed -n '1,3p' && echo '...（中间省略）...'
提交总数 = 24
ed73ad6 c24: 日常改动
18879e8 c23: 日常改动
8301830 c22: 日常改动
...（中间省略）...
--- 先自己确认现在确实是坏的：跑一次判定脚本，看它的退出码

$ cd /tmp/tmp.95kGtyLeQ2/bis && ./test.sh; echo "退出码 = $?（约定：0=好，1=坏）"
BAD  bonus(7) = 73（期望 70）
退出码 = 1（约定：0=好，1=坏）
```

**顺序很重要**：先确认"现在坏"，再启动 bisect。
`bisect start` 需要两个端点，写反了（把好的写在前面）git 不会报错，
但它会把范围理解成"`v1.0` 是坏的"，然后一路朝错误方向取中点，最后给你一个荒腔走板的结论。
demo 里的 `test.sh` 是**第 1 个提交就带进仓库**的（`c1: 初始版本（检查脚本 test.sh 一起提交）`），
这不是巧合 —— bisect 会把工作区切到**每一个**历史版本去跑脚本，
如果脚本是最后一个提交才加的，前面的版本里根本没有这个文件，`run` 会当场以 127 报错。

**真实输出**（② 手工版：`start` 之后 git 已经帮你切到中点了）

```text
$ cd /tmp/tmp.95kGtyLeQ2/bis && git bisect start HEAD v1.0
Bisecting: 11 revisions left to test after this (roughly 4 steps)
[6e17a2b67c6cc1c52f9bccb31a0007ebd77ba80c] c12: 日常改动
--- 注意它已经把工作区切到『中间那个提交』了，并告诉你还剩几个候选、大概还要几步。自己测一下再决定标什么：

$ cd /tmp/tmp.95kGtyLeQ2/bis && ./test.sh
GOOD bonus(7) = 70

$ cd /tmp/tmp.95kGtyLeQ2/bis && git bisect good
Bisecting: 5 revisions left to test after this (roughly 3 steps)
[77e83632d9d4f37f4c62bd96510f3a79e9f6bee5] c18: 日常改动

$ cd /tmp/tmp.95kGtyLeQ2/bis && git bisect bad
Bisecting: 2 revisions left to test after this (roughly 2 steps)
[ae7a7613b915c7fa7268cb9d7aaabd602e953af9] c15: 日常改动
```

读一下这三行 `Bisecting: …`：`11 revisions left / roughly 4 steps` →
`5 / 3` → `2 / 2`。**候选数几乎每次都砍一半**，`roughly N steps` 是 git 在告诉你"还要问几次"。
每次 `bisect good/bad` 之后，HEAD 就进入"游离"状态（detached），
`git log --oneline` 看到的是**那个历史版本**的代码，所以能直接跑测试、能 `grep`、能编译。

**真实输出**（③ 两端存在哪、以及中途怎么退场）

```text
$ cd /tmp/tmp.95kGtyLeQ2/bis && git bisect terms && ls .git/refs/bisect/
Your current terms are 'good' for the old state
and 'bad' for the new state.
bad
good-6e17a2b67c6cc1c52f9bccb31a0007ebd77ba80c
good-dd977efc6e7eb393f8a1b1f64607d4ea71ed076c
--- 中途想放弃 / 想回到原来的位置：git bisect reset

$ cd /tmp/tmp.95kGtyLeQ2/bis && git bisect reset && echo '### 回到了二分之前待的那个提交：' && git log --oneline -1
Previous HEAD position was ae7a761 c15: 日常改动
Switched to branch 'main'
### 回到了二分之前待的那个提交：
ed73ad6 c24: 日常改动
```

`git bisect terms` 那两句 `'good' for the old state / 'bad' for the new state` 是**默认术语**。
术语可以换（比如查性能回归时想用"快/慢"）：`git bisect start --term-old=fixed --term-new=broken HEAD v1.0`，
之后就要用 `git bisect fixed` / `git bisect broken` 来标记，`git bisect terms` 也会跟着显示新术语
（我另外在沙箱里验过一遍，中文环境下它显示「您当前的术语：旧状态为 'fixed'，新状态为 'broken'。」）。
`.git/refs/bisect/` 下面：`bad` 只有一个文件（当前最紧的坏端），`good-<全哈希>` 每个好端一个 ——
所以你能从目录里直接数出"这次二分标过几个好版本"。
**`git bisect reset` 不是可选项**：不 reset，HEAD 会一直游离在某个历史提交上，
接下来的任何 `git commit` 都会写进一条游离分支里，只能靠 reflog 救（第 7 个场景）。

**真实输出**（④ 自动版：`bisect run` 一路问到答案，35 行全贴）

```text
$ cd /tmp/tmp.95kGtyLeQ2/bis && git bisect start HEAD v1.0
Bisecting: 11 revisions left to test after this (roughly 4 steps)
[6e17a2b67c6cc1c52f9bccb31a0007ebd77ba80c] c12: 日常改动

$ cd /tmp/tmp.95kGtyLeQ2/bis && git bisect run ./test.sh
running './test.sh'
GOOD bonus(7) = 70
Bisecting: 5 revisions left to test after this (roughly 3 steps)
[77e83632d9d4f37f4c62bd96510f3a79e9f6bee5] c18: 日常改动
running './test.sh'
BAD  bonus(7) = 73（期望 70）
Bisecting: 2 revisions left to test after this (roughly 2 steps)
[ae7a7613b915c7fa7268cb9d7aaabd602e953af9] c15: 日常改动
running './test.sh'
GOOD bonus(7) = 70
Bisecting: 0 revisions left to test after this (roughly 1 step)
[89aa375c54daf7724bb1b86c07c63ab224f5b84a] c17: 日常改动
running './test.sh'
BAD  bonus(7) = 73（期望 70）
Bisecting: 0 revisions left to test after this (roughly 0 steps)
[6c3efed61473fbc46f445d47dca701f618e9faa6] c16: 日常改动
running './test.sh'
GOOD bonus(7) = 70
89aa375c54daf7724bb1b86c07c63ab224f5b84a is the first 'bad' commit
commit 89aa375c54daf7724bb1b86c07c63ab224f5b84a
Author: 学习者 <learn@example.com>
Date:   Thu Jan 1 10:00:00 2026 +0800

    c17: 日常改动

 calc.c       | 4 +++-
 notes/n17.md | 1 +
 2 files changed, 4 insertions(+), 1 deletion(-)
 create mode 100644 notes/n17.md
bisect found first 'bad' commit
```

这段是本章最有画面感的输出：`running './test.sh'` → `GOOD`/`BAD` → 再切中点 → 再跑，
**5 次之后**给出 `89aa375c54daf7724bb1b86c07c63ab224f5b84a is the first 'bad' commit`，
并且顺手把这个提交的 `--stat` 打了出来（就是上面块结尾那 4 行：`calc.c` 改了 4 处、
新增了一个 `notes/n17.md`）。

真凶就是 `c17`：`calc.c` 里那个系数被动过（demo 用 `sed` 把 `int v = level * 10;`
改成 `int v = level * 10 + 3;`，于是 `bonus(7) = 73`）。
最后一行 `bisect found first 'bad' commit` 是 `run` 的收工信号；
**如果你的脚本在中途返回了 1~49 之外的"其它退出码"，这里会变成 `bisect run aborted`**，
结论不可信，得回去修判定脚本。

**真实输出**（⑤ 决策过程留档 + 一共问了几次）

```text
$ cd /tmp/tmp.95kGtyLeQ2/bis && echo '### 整个决策过程（可以复制下来当笔记）：' && git bisect log
### 整个决策过程（可以复制下来当笔记）：
# bad: [ed73ad6cdfef5ed3cc101857514c4ef40d146a2f] c24: 日常改动
# good: [dd977efc6e7eb393f8a1b1f64607d4ea71ed076c] c1: 初始版本（检查脚本 test.sh 一起提交）
git bisect start 'HEAD' 'v1.0'
# good: [6e17a2b67c6cc1c52f9bccb31a0007ebd77ba80c] c12: 日常改动
git bisect good 6e17a2b67c6cc1c52f9bccb31a0007ebd77ba80c
# bad: [77e83632d9d4f37f4c62bd96510f3a79e9f6bee5] c18: 日常改动
git bisect bad 77e83632d9d4f37f4c62bd96510f3a79e9f6bee5
# good: [ae7a7613b915c7fa7268cb9d7aaabd602e953af9] c15: 日常改动
git bisect good ae7a7613b915c7fa7268cb9d7aaabd602e953af9
# bad: [89aa375c54daf7724bb1b86c07c63ab224f5b84a] c17: 日常改动
git bisect bad 89aa375c54daf7724bb1b86c07c63ab224f5b84a
# good: [6c3efed61473fbc46f445d47dca701f618e9faa6] c16: 日常改动
git bisect good 6c3efed61473fbc46f445d47dca701f618e9faa6
# first 'bad' commit: [89aa375c54daf7724bb1b86c07c63ab224f5b84a] c17: 日常改动

$ cd /tmp/tmp.95kGtyLeQ2/bis && echo -n '24 个提交，二分实际问了几次：' && git bisect log | grep -cE '^git bisect (good|bad)'
24 个提交，二分实际问了几次：5

$ cd /tmp/tmp.95kGtyLeQ2/bis && git bisect reset
Previous HEAD position was 6c3efed c16: 日常改动
Switched to branch 'main'
```

`git bisect log` 的价值：**它本身就是一段可复制的命令序列**。
把它贴进 issue/群里，同事 `git bisect replay` 一下就能复现你的整个二分过程（不用再跑测试）。
最后那行 `24 个提交，二分实际问了几次：5` 是本节的落点 —— 手工 24 次 vs 自动 5 次，
这就是 bisect 的全部意义。

**常见翻车**

- **判定脚本不可靠**：测试本身有随机性/网络依赖，同一个版本一会 good 一会 bad，
  二分会**一路问错到底**且不报错。bisect 前先把脚本在同一版本上连跑三次，确认输出稳定。
- **忘了 `chmod +x`**：脚本没有执行权限 → 退出码 126 → bisect 立刻 abort；
  文件名写错 → 127。这两种都不是"版本坏"，别乱标。
- **用 `1`~`49` 之外的码表示坏**：例如 `exit 99` 会被当成"出事了"而中止，不是"坏"。
- **测试里 `cd`/`export` 污染了 bisect 环境**：`bisect run` 是在同一个 shell 会话里反复执行脚本的，
  脚本里改了环境变量、`cd` 跑了目录，下一轮就会在错误的地方跑。判定脚本要**只用绝对/相对当前仓库的路径**，
  退出前把临时文件清掉（`bisect-check.sh` 用 `/tmp/bisect-calc.$$` + `rm -f` 就是这个原因）。
- **中间有大段"编译不过"的历史**：给 `start` 之后加 `git bisect skip <范围>`，
  或者让脚本返回 125 让它自己跳过。跳过太多 git 会提示"无法确定唯一答案"，
  这时候它只能给你**一组候选提交**，别把它当结论直接发出去。
- **有合并提交时用普通 bisect**：这是下一个场景的全部内容 —— 也是本章最容易踩空的一脚。

### 场景 6：有合并提交时，bisect 为什么会翻车 —— `--first-parent` 怎么救（demo 第 4b 节）

**什么情况**：上一个场景的二分之所以成立，是因为**历史是一条直线**：一旦坏了，后面全都坏（单调）。
真实仓库里主干每天在合分支，历史变成一张网，"这个版本坏"这件事在网里**不再有明确意义** ——
分支内部的提交（`x1`）从来没和主干代码一起跑过，你测它，等于测一个**从没上线过的组合**。
这一节用 26 个提交的现场（主干 21 条 + 分支 3 条 + 一次合并，真凶藏在分支里的 `x2`）
把三种打法各跑一遍，看清它们各自回答的是什么问题。

**真实输出**（① 现场：合并刚发生，真凶在分支里）

```text
$ cd /tmp/tmp.95kGtyLeQ2/bm && git log --graph --oneline | sed -n '1,9p' && echo -n '提交总数 = ' && git rev-list --count HEAD
*   c78a66d Merge branch 'experimental'
|\  
| * cdec745 x3 feature 继续开发
| * ec9d350 x2 调整 bonus（真凶在这）
| * ec01e6f x1 新分支：feature.c
* | e7a8ed1 m22 主干上的普通改动
* | be897cd m21 主干上的普通改动
* | fb8bc01 m20 主干上的普通改动
* | 650da73 m19 主干上的普通改动
提交总数 = 26
```

图里的结构就是场景 2 ⑦（`--first-parent`）讲过的那张：**主干那一列是 `m1…m22`，右边那一竖是 `x1 x2 x3`**，
顶点 `c78a66d` 是合并提交。`base_ok` 是打在 `m1` 上的标签（已知好的起点）。

**真实输出**（② 普通 `git bisect run`：它第一次就问到了分支内部的 `x1`）

```text
$ cd /tmp/tmp.95kGtyLeQ2/bm && git bisect start HEAD base_ok && git bisect run ./test.sh
Bisecting: 12 revisions left to test after this (roughly 4 steps)
[ec01e6f32e12c3d6fc8555d1f0ca1035d49a0541] x1 新分支：feature.c
running './test.sh'
GOOD bonus(7) = 70
Bisecting: 6 revisions left to test after this (roughly 3 steps)
[e9b0e2cb6c358d5a876faff6c1995adbbac2a878] m18 主干上的普通改动
running './test.sh'
GOOD bonus(7) = 70
Bisecting: 3 revisions left to test after this (roughly 2 steps)
[be897cdc17a9e83a13be8c8781ffc69c77cc2a08] m21 主干上的普通改动
running './test.sh'
GOOD bonus(7) = 70
Bisecting: 1 revision left to test after this (roughly 1 step)
[cdec7459b07dbb76e57d6834e6018230e3b385d1] x3 feature 继续开发
running './test.sh'
BAD  bonus(7) = 73（期望 70）
Bisecting: 0 revisions left to test after this (roughly 0 steps)
[ec9d35035fa1efc4beb5d80db1c3c74f1eadb0f8] x2 调整 bonus（真凶在这）
running './test.sh'
BAD  bonus(7) = 73（期望 70）
ec9d35035fa1efc4beb5d80db1c3c74f1eadb0f8 is the first 'bad' commit
commit ec9d35035fa1efc4beb5d80db1c3c74f1eadb0f8
Author: 学习者 <learn@example.com>
Date:   Thu Jan 1 10:00:00 2026 +0800

    x2 调整 bonus（真凶在这）

 calc.c | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)
bisect found first 'bad' commit
```

**真实输出**（③ 它到底按什么顺序问的）

```text
$ cd /tmp/tmp.95kGtyLeQ2/bm && echo '### 它问过的顺序（主干和分支混在一起）：' && git bisect log | grep -E '^# (good|bad):' | sed 's/\[[0-9a-f]*\] //' && git bisect reset
### 它问过的顺序（主干和分支混在一起）：
# bad: Merge branch 'experimental'
# good: m1 起点
# good: x1 新分支：feature.c
# good: m18 主干上的普通改动
# good: m21 主干上的普通改动
# bad: x3 feature 继续开发
# bad: x2 调整 bonus（真凶在这）
Previous HEAD position was ec9d350 x2 调整 bonus（真凶在这）
Switched to branch 'main'
```

看清楚这个问的顺序：`x1`（分支里）→ `m18`（主干）→ `m21`（主干）→ `x3`（分支里）→ `x2`（分支里）。
**主干和分支混在一起跳**。这次 demo 的判据恰好干净（只有 `x2` 改了 `calc.c` 的系数），
所以普通 bisect 也给出了 `ec9d350 x2` —— 但它回答的问题是**「这行代码是谁写出来的」**。

而排查线上事故时你真正想问的是**「坏代码是什么时候进入主干的」**，两者不等价：
`x2` 写出来那天，主干上跑的一直是好的；是 `c78a66d` 那次合并把它带上线的。
更糟的是**判据会脏**：`x1`、`x3` 这类分支内部状态很可能编译不过（缺主干上的配套改动），
判定脚本返回 125 会被大量跳过，返回其它退出码则 bisect 直接中止，返回 1 会被误标成"坏"——
无论哪种，二分的单调前提就不成立了。

**真实输出**（④ 换成 `git bisect start --first-parent`：只在主干那一列里问）

```text
$ cd /tmp/tmp.95kGtyLeQ2/bm && git bisect start --first-parent HEAD base_ok && git bisect run ./test.sh
Bisecting: 10 revisions left to test after this (roughly 4 steps)
[0a3aa86ce9afd8e3a6933240846a3491cfa92893] m12 主干上的普通改动
running './test.sh'
GOOD bonus(7) = 70
Bisecting: 5 revisions left to test after this (roughly 3 steps)
[b3625eaad4d0507ca9c944412d8490ff780523bc] m17 主干上的普通改动
running './test.sh'
GOOD bonus(7) = 70
Bisecting: 2 revisions left to test after this (roughly 2 steps)
[fb8bc01958e8165201c4ecbf5e61a5e6ffb9ec5a] m20 主干上的普通改动
running './test.sh'
GOOD bonus(7) = 70
Bisecting: 0 revisions left to test after this (roughly 1 step)
[e7a8ed181d63c88cbfe712a24c9db42fe68bbb14] m22 主干上的普通改动
running './test.sh'
GOOD bonus(7) = 70
c78a66d482cf019aedf4cbc1bc02fd40f218b51c is the first 'bad' commit
commit c78a66d482cf019aedf4cbc1bc02fd40f218b51c
Merge: e7a8ed1 cdec745
Author: 学习者 <learn@example.com>
Date:   Thu Jan 1 10:00:00 2026 +0800

    Merge branch 'experimental'

 calc.c    | 2 +-
 feature.c | 2 ++
 2 files changed, 3 insertions(+), 1 deletion(-)
 create mode 100644 feature.c
bisect found first 'bad' commit
```

**真实输出**（⑤ 结论停在合并提交上）

```text
$ cd /tmp/tmp.95kGtyLeQ2/bm && echo '### 结论停在合并提交上（这才是符合直觉的答案：它就是『坏代码进入主干』的那一刻）：' && git bisect log | grep -E '^# (good|bad|first)' | sed 's/\[[0-9a-f]*\] //' && git bisect reset
### 结论停在合并提交上（这才是符合直觉的答案：它就是『坏代码进入主干』的那一刻）：
# bad: Merge branch 'experimental'
# good: m1 起点
# good: m12 主干上的普通改动
# good: m17 主干上的普通改动
# good: m20 主干上的普通改动
# good: m22 主干上的普通改动
# first 'bad' commit: Merge branch 'experimental'
Previous HEAD position was e7a8ed1 m22 主干上的普通改动
Switched to branch 'main'
```

这一版全程只问主干上的提交（`m12 → m17 → m20 → m22`），四个全是 GOOD，
于是"第一个坏"落在 **合并提交 `c78a66d`** 本身。这才是运维视角的正确答案：
**这次合并就是坏代码进入主干的那一刻**。而且 `--first-parent` 二分**不会脱离主干**，
它测的每一个版本都是"当时主干的真实状态"，判据天然单调。
注意 diff 那两行：`calc.c | 2 +-` 和 `feature.c | 2 ++`（`create mode 100644 feature.c`）——
合并提交在 `--stat` 下显示的是"相对第一父亲，这次合并带进来了什么"，一眼就能看到它把分支的改动吸了进来。

`git bisect start --first-parent` 需要较新的 Git（`git help bisect` 的 SYNOPSIS 里有 `--first-parent`
才算支持；本机 git 2.55.0 实测可用）。老版本的等价做法是把主干手动摊平：`git rev-list --first-parent main`
自己取中点测，或者干脆用 `git log --oneline --first-parent main` 找候选区间。

**真实输出**（⑥ 最后一击：再从合并提交钻进分支，找出具体哪一条）

```text
$ cd /tmp/tmp.95kGtyLeQ2/bm && git bisect start 'HEAD^2' 'HEAD^1' && git bisect run ./test.sh && git bisect reset
Bisecting: a merge base must be tested
[0a3aa86ce9afd8e3a6933240846a3491cfa92893] m12 主干上的普通改动
running './test.sh'
GOOD bonus(7) = 70
Bisecting: 0 revisions left to test after this (roughly 1 step)
[ec9d35035fa1efc4beb5d80db1c3c74f1eadb0f8] x2 调整 bonus（真凶在这）
running './test.sh'
BAD  bonus(7) = 73（期望 70）
Bisecting: 0 revisions left to test after this (roughly 0 steps)
[ec01e6f32e12c3d6fc8555d1f0ca1035d49a0541] x1 新分支：feature.c
running './test.sh'
GOOD bonus(7) = 70
ec9d35035fa1efc4beb5d80db1c3c74f1eadb0f8 is the first 'bad' commit
commit ec9d35035fa1efc4beb5d80db1c3c74f1eadb0f8
Author: 学习者 <learn@example.com>
Date:   Thu Jan 1 10:00:00 2026 +0800

    x2 调整 bonus（真凶在这）

 calc.c | 2 +-
 1 file changed, 1 insertion(+), 1 deletion(-)
bisect found first 'bad' commit
Previous HEAD position was ec01e6f x1 新分支：feature.c
Switched to branch 'main'
```

`git bisect start 'HEAD^2' 'HEAD^1'` 是本节最实用的一招，两个父亲各自的意义：

| 写法 | 指向 | 在这一节里是谁 |
|---|---|---|
| `M^1`（第一父亲） | 合并前**主干**的顶点 | `m22`（好） |
| `M^2`（第二父亲） | 合并前**分支**的顶点 | `x3`（坏） |

坏端给 `^2`、好端给 `^1`，就得到一条**只含这次合并带进来的提交**的直线，二分立刻收敛到 `x2`。
中间那句 `Bisecting: a merge base must be tested` 是 git 在说：
"你给的两端有个共同祖先（`m12`），我得先知道它是好是坏，不然无法二分" ——
**这不是报错**，测它、标它（demo 里标了 GOOD），它继续往下钻。

**所以标准打法是两步**：先 `--first-parent` 定位"哪次合并把坏了带进主干"，
再 `start 'M^2' 'M^1'` 钻进那条分支定位"具体哪一条提交"。一步到位的普通 bisect 只能给你其中一个答案，
而且可能是错的那个。

**常见翻车**

- **在合并密集的主干上跑普通 bisect**：结论落在某个分支内部提交上，
  而那个提交从没和主干一起跑过，看起来"找到了"，实际无法解释线上为什么坏。
- **把 `Bisecting: a merge base must be tested` 当成出错**：直接测它、标它即可。
- **`^1` / `^2` 记反**：记法很简单 —— **`^1` 是"你当时站着的那条线"（主干），`^2` 是"被合进来的那条线"**。
  记反了 git 不报错，但结论会指向"分支起点"这种无意义的答案。
- **合并用了 `-s ours` / 或者是个"空合并"**：`M^2` 那一侧带进来的净改动为 0，二分没有可测的差异，
  这时候要退回到"看这次合并实际改了什么"（`git show -m M` / `git diff M^1 M`）。

---

### 场景 7：`git reflog` —— HEAD 的流水账，以及误删分支的完整救场（demo 第 5 节）

**什么情况**：`reset --hard` 敲错、分支删了、rebase 到一半崩了，**而且你没记下任何哈希**。
这时唯一能救你的是 reflog —— 它是"HEAD 待过哪些地方"的本地流水账：
**只存本地、不推不拉、别人看不到**（同事机器上没有你的 reflog，所以别把它当备份）。

**真实输出**（① 流水账本体，以及它在磁盘上的样子）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git reflog | sed -n '1,14p'
320a44b HEAD@{0}: reset: moving to HEAD
320a44b HEAD@{1}: commit: fix: 运费按重量加倍 (#205)
30fef87 HEAD@{2}: commit: feat: 给 total_price 加缓存
4a8560c HEAD@{3}: commit: docs: design.md 改名 architecture.md 并补一句
7d8a14d HEAD@{4}: commit: chore: 删掉调试打印
b7bfb91 HEAD@{5}: commit: wip: 临时调试打印
a616ae6 HEAD@{6}: merge feature/export: Merge made by the 'ort' strategy.
7bf241d HEAD@{7}: commit: chore: 主干上的一次普通改动
45e6d20 HEAD@{8}: checkout: moving from feature/export to main
1fd8cd4 HEAD@{9}: commit: feat: 支持导出 CSV (#103)
45e6d20 HEAD@{10}: checkout: moving from main to feature/export
45e6d20 HEAD@{11}: commit: refactor: 把运费逻辑抄一份到 src/ship.py
3a5c74e HEAD@{12}: commit: style: 调了一下 count_items 的缩进（纯空白改动）
d4d56a1 HEAD@{13}: commit: refactor: MAX_RETRY 统一改名 ATTEMPTS (#104)

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 它在磁盘上就是一个纯文本文件：旧哈希 新哈希 身份 <时间戳> TAB 动作' && cat "/tmp/tmp.95kGtyLeQ2/inv/.git/logs/HEAD" | sed -n '1,4p'
### 它在磁盘上就是一个纯文本文件：旧哈希 新哈希 身份 <时间戳> TAB 动作
0000000000000000000000000000000000000000 1d03ac3ee6e9563046c096866019900fd800871a 同事甲 <jia@example.com> 1788055834 +0800	commit (initial): init: 项目骨架
1d03ac3ee6e9563046c096866019900fd800871a 79f410b51947e7a6de183c429ab6c3c412db2839 同事乙 <yi@example.com> 1788487834 +0800	commit: feat: 给 net.py 加 MAX_RETRY 的引用 (#101)
79f410b51947e7a6de183c429ab6c3c412db2839 930dc062fbbdc593db68b5bf1aab86cbba1ff819 学习者 <learn@example.com> 1788660634 +0800	commit: fix: 修登录页等级为负时的崩溃 (#102)
930dc062fbbdc593db68b5bf1aab86cbba1ff819 5e88b21bd888d182b04cd886109631aba130c050 同事乙 <yi@example.com> 1789265434 +0800	commit: refactor: retry_policy 和 connect 换个位置（MAX_RETRY 一次没增没减）
```

`git reflog` 打印的是"结果"：`哈希 HEAD@{n}: 动作: 描述`。
真正有意思的是 `.git/logs/HEAD` —— 它就是一行一条的**纯文本**，字段用**制表符**分隔：

```
旧哈希 空格 新哈希 空格 身份(name <email>) 空格 unix时间戳 空格 时区 <TAB> 动作: 描述
```

看到 `0000000000000000000000000000000000000000` 就知道那是仓库的第一条记录（之前什么都没有）。
因为是纯文本，`cat`、`grep`、`awk` 都能直接处理 —— 这也是它能当"最后防线"的原因：
哪怕 `git` 本身出问题，这个文件里的字还在。

**真实输出**（② `git log -g` = 同一份东西的 log 视图，`-g` 就是 `--walk-reflogs`）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git log -g --oneline | sed -n '1,8p'
320a44b HEAD@{0}: reset: moving to HEAD
320a44b HEAD@{1}: commit: fix: 运费按重量加倍 (#205)
30fef87 HEAD@{2}: commit: feat: 给 total_price 加缓存
4a8560c HEAD@{3}: commit: docs: design.md 改名 architecture.md 并补一句
7d8a14d HEAD@{4}: commit: chore: 删掉调试打印
b7bfb91 HEAD@{5}: commit: wip: 临时调试打印
a616ae6 HEAD@{6}: merge feature/export: Merge made by the 'ort' strategy.
7bf241d HEAD@{7}: commit: chore: 主干上的一次普通改动

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log -g --format='%h %gd %gs' | sed -n '1,4p'
320a44b HEAD@{0} reset: moving to HEAD
320a44b HEAD@{1} commit: fix: 运费按重量加倍 (#205)
30fef87 HEAD@{2} commit: feat: 给 total_price 加缓存
4a8560c HEAD@{3} commit: docs: design.md 改名 architecture.md 并补一句

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 带时间的写法（--date 控制 @{...} 里显示成什么）：' && git reflog --date=iso | sed -n '1,4p'
### 带时间的写法（--date 控制 @{...} 里显示成什么）：
320a44b HEAD@{2026-10-04 10:10:35 +0800}: reset: moving to HEAD
320a44b HEAD@{2026-10-03 10:10:34 +0800}: commit: fix: 运费按重量加倍 (#205)
30fef87 HEAD@{2026-10-02 10:10:34 +0800}: commit: feat: 给 total_price 加缓存
4a8560c HEAD@{2026-09-30 10:10:34 +0800}: commit: docs: design.md 改名 architecture.md 并补一句
```

为什么要用 `log -g`：`reflog` 命令只能整份列出来，而 `log -g` **能加 log 家族的全部阀门** ——
`--oneline`、`--grep='rebase'`、`-20`、`--since`，还能 `--format`。
`%gd`（reflog 下标）/ `%gs`（reflog 动作说明）这两个占位符只在 `-g` 模式下有意义。
`--date=iso` 那一段是排查利器：**它把 `HEAD@{n}` 换成"那一刻的时间戳"**，
于是"昨天下午 3 点我的 HEAD 在哪"这种问题能直接读出来。

**真实输出**（③ 索引语法：`HEAD@{n}`、`main@{n}`、`@{-1}`）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git rev-parse 'HEAD@{0}' 'HEAD@{1}' 'HEAD@{2}'
320a44b5a697a1e34cafc8a78b008762d84e79cd
320a44b5a697a1e34cafc8a78b008762d84e79cd
30fef8741e25efd9639acb57daa7127bd99439e6

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline -1 'HEAD@{2}'
30fef87 feat: 给 total_price 加缓存

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 分支自己的 reflog（磁盘上叫 .git/logs/refs/heads/main）：' && git log -g --oneline main | sed -n '1,5p'
### 分支自己的 reflog（磁盘上叫 .git/logs/refs/heads/main）：
320a44b main@{0}: commit: fix: 运费按重量加倍 (#205)
30fef87 main@{1}: commit: feat: 给 total_price 加缓存
4a8560c main@{2}: commit: docs: design.md 改名 architecture.md 并补一句
7d8a14d main@{3}: commit: chore: 删掉调试打印
b7bfb91 main@{4}: commit: wip: 临时调试打印

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### @{-1} = 上一个待过的分支（来回切的时候特别好用）：' && git switch -q -c tmp/scratch && git switch -q main && git rev-parse --abbrev-ref '@{-1}'
### @{-1} = 上一个待过的分支（来回切的时候特别好用）：
tmp/scratch
```

四条要点：

- `HEAD@{0}` 是**最新**，`HEAD@{1}` 是上一次，往旧的方向数。注意上面 `HEAD@{0}` 和 `HEAD@{1}`
  打印出**同一个哈希**（`320a44b…`）：因为 `@{0}` 是那次 `reset: moving to HEAD`，
  原地重置不改哈希 —— **reflog 记录条数 ≠ 提交个数**，别拿 `HEAD@{n}` 当 `HEAD~n` 用，两者完全不是一回事。
- `main@{n}` 是**这根分支指针自己的**历史（磁盘上 `.git/logs/refs/heads/main`），
  它比 `HEAD@{n}` 干净：只记分支指针的移动，不记你切到别的分支去干的事。
- `@{-1}` = **上一个待过的分支**，来回切分支时比 `git checkout -` 更好读（`-` 也是同一个意思）。
- 一定要**加引号**：`@{...}` 里有花括号，zsh 会把 `{...}` 当花括号展开，bash 历史展开还会吃掉 `!`。

**真实输出**（④ `@{u}`：必须先有跟踪关系；demo 现搭了一个**本地 bare 仓库**冒充远程）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 配好跟踪之后 @{u} 就能用了：' && git rev-parse --abbrev-ref '@{u}' && git status -sb | sed -n '1,1p'
### 配好跟踪之后 @{u} 就能用了：
origin/main
## main...origin/main

$ cd /tmp/tmp.95kGtyLeQ2/inv && git switch -q tmp/scratch && echo '### 没配跟踪的分支上问 @{u} 会怎样：' && git rev-parse '@{u}'
### 没配跟踪的分支上问 @{u} 会怎样：
fatal: no upstream configured for branch 'tmp/scratch'
[命令退出码 = 128 —— 上面的报错是故意让它发生的]

$ cd /tmp/tmp.95kGtyLeQ2/inv && git switch -q main
```

`origin/main` 那一步 demo 做的事是：`git init --bare "$SB/inv-origin.git"` → `git remote add origin <本地路径>` →
`git push -u origin main`。**全程在临时目录里，没有联网**。最后那条 `fatal: no upstream configured for branch 'tmp/scratch'`
是**故意**制造的错误，`[命令退出码 = 128 …]` 那行是 demo 自己补的注解 —— 本章所有"故意报错"都长这样，
看到就明白"这是教学重点，不是脚本坏了"。

**真实输出**（⑤ `@{...}` 里能写日期 —— 但它**不是时光机**）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && git reflog --date=iso | sed -n '1,6p' && echo "### 今天是 2026-10-04，上面这些时间点就是提交时间" && git rev-parse 'HEAD@{yesterday}'
320a44b HEAD@{2026-10-04 10:10:37 +0800}: checkout: moving from tmp/scratch to main
320a44b HEAD@{2026-10-04 10:10:37 +0800}: checkout: moving from main to tmp/scratch
320a44b HEAD@{2026-10-04 10:10:37 +0800}: checkout: moving from tmp/scratch to main
320a44b HEAD@{2026-10-04 10:10:37 +0800}: checkout: moving from main to tmp/scratch
320a44b HEAD@{2026-10-04 10:10:35 +0800}: reset: moving to HEAD
320a44b HEAD@{2026-10-03 10:10:34 +0800}: commit: fix: 运费按重量加倍 (#205)
### 今天是 2026-10-04，上面这些时间点就是提交时间
320a44b5a697a1e34cafc8a78b008762d84e79cd

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline -1 'HEAD@{yesterday}'
320a44b fix: 运费按重量加倍 (#205)

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 提醒：时间点超出 reflog 覆盖范围时 git 不报错，它直接把最新那条给你 —— 别把 @{...} 当时光机' && git rev-parse 'HEAD@{100 years ago}' && git rev-parse 'HEAD@{0}'
### 提醒：时间点超出 reflog 覆盖范围时 git 不报错，它直接把最新那条给你 —— 别把 @{...} 当时光机
320a44b5a697a1e34cafc8a78b008762d84e79cd
320a44b5a697a1e34cafc8a78b008762d84e79cd
```

`HEAD@{yesterday}` 的含义是：**"昨天那一刻，HEAD 在哪个提交上"**（在 reflog 里找时间戳最接近的那条）。
关键在最后一组：`HEAD@{100 years ago}` 超出了 reflog 的覆盖范围，
**git 不报错，它直接把最新那条给你** —— 于是你以为回到了一年以前，实际拿到的是现在的 HEAD。
这个静默行为是本章最阴的坑之一（超出 reflog 起点之外没有记录，git 只能给你"最早可见的那条"，
而在 `--date` 视图里它看起来完全正常）。**用 `@{日期}` 之前先 `git reflog --date=iso | tail -5` 看看流水账到哪一天为止。**

**真实输出**（⑥ `--since`/`--until` 里的人话日期，粒度是"这一刻"而不是"这天 0 点"）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 现在是 10:10:37，所以『昨天这一刻』比最后一次提交还晚一点点 → 空：' && git log --oneline --since='yesterday' --format='%h %ad %s' --date=short
### 现在是 10:10:37，所以『昨天这一刻』比最后一次提交还晚一点点 → 空：

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 想要『从昨天 0 点开始』，就写清楚时间：' && git log --oneline --since='yesterday 00:00' --format='%h %ad %s' --date=short
### 想要『从昨天 0 点开始』，就写清楚时间：
320a44b 2026-10-03 fix: 运费按重量加倍 (#205)

$ cd /tmp/tmp.95kGtyLeQ2/inv && git log --oneline --since='2026-01-01 00:00' --until='3 weeks ago' --format='%h %ad %s' --date=short | sed -n '1,4p'
5e88b21 2026-09-13 refactor: retry_policy 和 connect 换个位置（MAX_RETRY 一次没增没减）
930dc06 2026-09-06 fix: 修登录页等级为负时的崩溃 (#102)
79f410b 2026-09-04 feat: 给 net.py 加 MAX_RETRY 的引用 (#101)
1d03ac3 2026-08-30 init: 项目骨架
```

`--since='yesterday'` = 从**昨天的这一刻**开始，不是"昨天 0 点"。
demo 里最后一次提交是"昨天 10:10:34"，而这一刻是 10:10:37，于是那条提交差了 3 秒被排除，返回空。
想要"从昨天 0 点起"，写 `--since='yesterday 00:00'`（第三条 `--since='2026-01-01 00:00'` 也顺便说明：
**绝对日期配 `--until='3 weeks ago'` 这种相对日期是可以混着写的**）。
空输出不是错误 —— 但如果你不知道粒度，就会误判成"这个功能没人改过"。

**真实输出**（⑦ 硬核实战：误删分支 → 找回来 → 原地复活。这一段建议照着敲三遍）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 这条分支上有两个只有我自己知道的提交：' && git log --oneline -3
### 这条分支上有两个只有我自己知道的提交：
4f48d37 refactor: 大改造第二步（还没测）
e3cff98 refactor: 大改造第一步
320a44b fix: 运费按重量加倍 (#205)

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo "### 顶点哈希（真实场景你不会去记它，这里只是待会儿用来对照答案）：4f48d37266d976fbc3cb82bdbeb39a697dfdd8ca"
### 顶点哈希（真实场景你不会去记它，这里只是待会儿用来对照答案）：4f48d37266d976fbc3cb82bdbeb39a697dfdd8ca

$ cd /tmp/tmp.95kGtyLeQ2/inv && git switch -q main && git branch -D secret/大改造
Deleted branch secret/大改造 (was 4f48d37).
```

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 现在分支列表里没有它了（tmp/scratch 和 remotes/origin/main 是前面几步留下的）：' && git branch -a && echo '### main 的历史里也搜不到这两个提交：' && git log --oneline --grep='大改造' | sed -n '1,3p' && echo '(上面空 = 从分支上彻底看不见了)' 
### 现在分支列表里没有它了（tmp/scratch 和 remotes/origin/main 是前面几步留下的）：
* main
  tmp/scratch
  remotes/origin/main
### main 的历史里也搜不到这两个提交：
(上面空 = 从分支上彻底看不见了)

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 但 reflog 里全都记着（刚才那条 Deleted branch ... 提示本身就给了哈希）：' && git reflog | sed -n '1,8p'
### 但 reflog 里全都记着（刚才那条 Deleted branch ... 提示本身就给了哈希）：
320a44b HEAD@{0}: checkout: moving from secret/大改造 to main
4f48d37 HEAD@{1}: commit: refactor: 大改造第二步（还没测）
e3cff98 HEAD@{2}: commit: refactor: 大改造第一步
320a44b HEAD@{3}: checkout: moving from main to secret/大改造
320a44b HEAD@{4}: checkout: moving from tmp/scratch to main
320a44b HEAD@{5}: checkout: moving from main to tmp/scratch
320a44b HEAD@{6}: checkout: moving from tmp/scratch to main
320a44b HEAD@{7}: checkout: moving from main to tmp/scratch
```

到这一步的处境是：**分支列表里没有它了，`git log` 里也搜不到这两个提交**（`(上面空 = 从分支上彻底看不见了)`）。
在不知道 reflog 的人这里就是"两天工作没了"。但 reflog 里 4 条记录一条不少，
而且**`git branch -D` 的那句提示本身就把哈希告诉你了**：`Deleted branch secret/大改造 (was 4f48d37).`
—— 出事那一刻**先把终端上的这行抄下来**，比事后翻 reflog 快。

**真实输出**（⑧ 两种找法 + 确认对象还在 + 复活）

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 找法一：reflog 里按动作搜（-g 走的就是 reflog）' && git log -g --oneline --grep='大改造'
### 找法一：reflog 里按动作搜（-g 走的就是 reflog）
4f48d37 HEAD@{1}: commit: refactor: 大改造第二步（还没测）
e3cff98 HEAD@{2}: commit: refactor: 大改造第一步

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 找法二：直接翻流水账' && git reflog | grep '大改造'
### 找法二：直接翻流水账
320a44b HEAD@{0}: checkout: moving from secret/大改造 to main
4f48d37 HEAD@{1}: commit: refactor: 大改造第二步（还没测）
e3cff98 HEAD@{2}: commit: refactor: 大改造第一步
320a44b HEAD@{3}: checkout: moving from main to secret/大改造
```

```text
$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 顺手确认它真的还在对象库里：' && git cat-file -t 4f48d37266d976fbc3cb82bdbeb39a697dfdd8ca && git show --stat --format='%h %s' 4f48d37266d976fbc3cb82bdbeb39a697dfdd8ca | sed -n '1,6p'
### 顺手确认它真的还在对象库里：
commit
4f48d37 refactor: 大改造第二步（还没测）

 src/calc.c | 3 +++
 1 file changed, 3 insertions(+)

$ cd /tmp/tmp.95kGtyLeQ2/inv && git branch secret/大改造 4f48d37266d976fbc3cb82bdbeb39a697dfdd8ca && echo '### 复活了：' && git branch --list 'secret/*' && git log --oneline -3 secret/大改造
### 复活了：
  secret/大改造
4f48d37 refactor: 大改造第二步（还没测）
e3cff98 refactor: 大改造第一步
320a44b fix: 运费按重量加倍 (#205)

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 全程不记哈希也能救：reflog 的下标直接当起点用。上面写着 HEAD@{1}: commit: 大改造第二步，那就用它：' && git log --oneline -3 'HEAD@{1}'
### 全程不记哈希也能救：reflog 的下标直接当起点用。上面写着 HEAD@{1}: commit: 大改造第二步，那就用它：
4f48d37 refactor: 大改造第二步（还没测）
e3cff98 refactor: 大改造第一步
320a44b fix: 运费按重量加倍 (#205)

$ cd /tmp/tmp.95kGtyLeQ2/inv && echo '### 两种写法指向同一个提交（这才叫确认无误）：' && git rev-parse --short 'HEAD@{1}' && git rev-parse --short secret/大改造
### 两种写法指向同一个提交（这才叫确认无误）：
4f48d37
4f48d37
```

这一段里 `git log -g --grep='大改造'` 是最优雅的：**在 reflog 上用 log 的检索语法**（场景 7 ② 的能力在这里兑现）。
`git branch secret/大改造 <哈希>` 就是"把一根新指针挂到那个提交上"，两个提交连同它们的父链整体回来。
最后 `git rev-parse --short 'HEAD@{1}'` 与 `git rev-parse --short secret/大改造` 给出同一个短哈希 ——
**救场之后一定要做"两条独立路径指向同一个对象"这种交叉确认**，
别用"看起来文件回来了"当验收（文件可能回来但指针没挂对）。

顺手记住救场的**三步口径**：

```bash
git reflog | head -20                 # 1. 找到"我本来在哪"（抄哈希，或记住 HEAD@{n}）
git show --stat <哈希>                # 2. 确认内容真的是你要的那一份
git branch rescue <哈希>              # 3. 先挂个保险分支，再决定怎么挪回去
```

第 3 步为什么重要：挂上 `rescue` 之后，这个提交**就变成可达的了**，
即使后面你 `reset` 错了、或者 `gc` 跑了，它也删不掉 —— **先保命，再讲姿势**。
（同一个套路在第 03 章也出现过，那是"撤销"视角；这里是"取证"视角。）

**常见翻车**

- **误删之后继续敲命令**：`commit` / `checkout` / `reset` 每一条都在往 `.git/logs/HEAD` 顶上追加记录，
  虽然不会覆盖旧记录，但会让 `HEAD@{n}` 的下标全部位移。出事就**停手**，先 `git reflog` 抄哈希。
- **以为 reflog 会跟着 push 走**：它是纯本地文件，不在对象库里，**不会**被推走。
  别人（或另一台机器）删掉的分支，只能靠服务端保留的引用/对象或 `git log --all` 找回。
- **把 `HEAD@{3}` 当 `HEAD~3`**：前者是"第 3 条流水账记录"，后者是"往前第 3 个父提交"，毫无关系。
- **忘了分支也有自己的 reflog**：`HEAD` 的流水账里 checkout 记录很多会干扰眼睛，
  `git log -g main` / `git reflog show main` 只看分支指针自己的移动，往往一眼就找到。
- **`git reflog show` 后面跟了不存在的分支名**：git 会报 `fatal: ambiguous argument 'xxx': unknown revision`
  或回落到 HEAD 视图；先 `git branch -a` 确认真的名字（尤其是带 `/` 的）。
- **别用 `git reflog expire --expire=now --all`"清理空间"**：那是把后悔药本身销毁，
  下一个场景实测它接 `gc --prune=now` 之后对象就真没了。

---

### 场景 8：`fsck` / dangling / `gc` —— Git 为什么"很难真的丢东西"（demo 第 6 节）

**什么情况**：reflog 也过期了 / 同事的机器上没有记录 / 你想知道"git 到底还藏着什么"。
这一节把"能不能救"这条线**跑到尽头**：从 `reset --hard` 甩掉两个提交开始，
经 `cat-file`、`fsck`、`--lost-found`、`reflog expire`、`gc --prune=now`，
直到最后**真的救不回来**的那一步 —— 只有见过终点，才知道前面的宽限期有多宝贵。

**真实输出**（① 手滑之后，先问对象还在不在）

```text
$ cd /tmp/tmp.95kGtyLeQ2/gc && git log --oneline --format='%h %s'
86bd29a g3 又改了一点
a078652 g2: 我做了很久的工作
74f71f5 g1 起点

$ cd /tmp/tmp.95kGtyLeQ2/gc && echo '### 手滑：reset --hard 回到最早的提交，g2 和 g3 从分支上掉了' && git reset --hard "74f71f5bffd6bbf96ca7a26e568139c581d40849" && git log --oneline
### 手滑：reset --hard 回到最早的提交，g2 和 g3 从分支上掉了
HEAD is now at 74f71f5 g1 起点
74f71f5 g1 起点
--- ① 对象其实还在库里：直接问 cat-file

$ cd /tmp/tmp.95kGtyLeQ2/gc && echo "### g2（a078652) 还在不在？" && git cat-file -t "a078652f2dcf3141435fc046b3e259ae7cd6aa34" && git log -1 --oneline "a078652f2dcf3141435fc046b3e259ae7cd6aa34"
### g2（a078652) 还在不在？
commit
a078652 g2: 我做了很久的工作
```

`git reset --hard 74f71f5` 之后，`git log --oneline` 只剩 `g1 起点`，看起来 g2、g3 全没了。
但 `git cat-file -t <g2 全哈希>` 回答 `commit` —— **对象库根本不知道"分支指向哪"这回事**，
它只认"内容在不在"。分支指针挪走，只让对象**变不可达**，不让它消失。

**真实输出**（② `git fsck` 的默认行为，和 `--no-reflogs` 的区别 —— 这一条最容易被误解）

```text
$ cd /tmp/tmp.95kGtyLeQ2/gc && git fsck


$ cd /tmp/tmp.95kGtyLeQ2/gc && echo '### 加上 --no-reflogs（假装 reflog 不存在）才露出真身：' && git fsck --no-reflogs
### 加上 --no-reflogs（假装 reflog 不存在）才露出真身：
dangling commit 86bd29ad9d7741aaa2641f1190291470ab73bc97
--- ③ dangling = 悬空：没有任何指针指向它。--lost-found 会把这些提交的内容写成文件，方便逐个翻

$ cd /tmp/tmp.95kGtyLeQ2/gc && git fsck --lost-found --no-reflogs && ls .git/lost-found/commit/
dangling commit 86bd29ad9d7741aaa2641f1190291470ab73bc97
86bd29ad9d7741aaa2641f1190291470ab73bc97

$ cd /tmp/tmp.95kGtyLeQ2/gc && echo '### 注意只列出 1 个：fsck 只报链条顶端（g3），g2 是它的父亲，顺着 g3 就能找到' && for f in .git/lost-found/commit/*; do git log --oneline -3 "$(basename "$f")"; done
### 注意只列出 1 个：fsck 只报链条顶端（g3），g2 是它的父亲，顺着 g3 就能找到
86bd29a g3 又改了一点
a078652 g2: 我做了很久的工作
74f71f5 g1 起点
```

**第一次 `git fsck` 输出是空的**（`$ git fsck` 下面那两行空白就是真的空），
因为默认 fsck 把 **reflog 也算作"引用"**：reflog 还指着 g2/g3，所以它们不算悬空，fsck 一声不吭。
加上 `--no-reflogs`（"假装 reflog 不存在，只看正经引用"）才露出 `dangling commit 86bd29ad…`。
**这个差别决定了你能不能捞到东西**：

| 目的 | 用什么 |
|---|---|
| 检查仓库有没有坏（对象损坏、缺失父提交） | `git fsck`（别加参数） |
| 找"分支上没了但库里还在"的提交 | `git fsck --no-reflogs` / `--lost-found` |
| 找全部不可达对象（不止悬空顶端） | `git fsck --unreachable --no-reflogs` |

`--lost-found` 那一步还有两个细节：它**只报链条顶端**（g3），
g2 是 g3 的父亲，顺着 `git log` 就找到了 —— 所以"只列出 1 个"不代表"只丢了 1 个"；
同时它会把悬空提交的内容写成 `.git/lost-found/commit/<全哈希>` 文件，方便逐个 `cat`/`git show`。
★ 注意 `git fsck --lost-found` **会往 `.git/lost-found/` 里写文件**，
它不是纯只读命令；在生产仓库上跑之前想清楚（要只是看，用 `--no-reflogs` 就够了）。

**真实输出**（③ 救回来；顺带认识 dangling blob —— "add 过又没提交"的草稿）

```text
$ cd /tmp/tmp.95kGtyLeQ2/gc && git reset --hard "86bd29ad9d7741aaa2641f1190291470ab73bc97" && git log --oneline
HEAD is now at 86bd29a g3 又改了一点
86bd29a g3 又改了一点
a078652 g2: 我做了很久的工作
74f71f5 g1 起点
--- ⑤ 顺带认识一下 dangling blob：add 过、后来又没提交的内容，同样躺在对象库里

$ cd /tmp/tmp.95kGtyLeQ2/gc && printf '这段草稿后来没提交\n' > draft.txt && git add draft.txt && git reset -q draft.txt && rm draft.txt && git fsck --no-reflogs
dangling blob b0ef021f294159cadac9b9db9818887538e12cf2

$ cd /tmp/tmp.95kGtyLeQ2/gc && echo '### 把那个悬空 blob 的内容捞出来（这就是『找回没提交过的草稿』唯一的办法，前提是 add 过）' && git fsck --no-reflogs | awk '/^dangling blob/ {print $3}' | sed -n '1,1p' | xargs -r git cat-file -p
### 把那个悬空 blob 的内容捞出来（这就是『找回没提交过的草稿』唯一的办法，前提是 add 过）
这段草稿后来没提交
```

`git reset --hard <悬空提交>` 一步把两个提交全带回来。
后半段是另一类恢复：`printf … > draft.txt && git add draft.txt && git reset draft.txt && rm draft.txt`
—— 这个文件**从没被提交过**，但因为 `git add` 过，内容已经作为一个 **blob** 进了对象库，
`fsck --no-reflogs` 报出 `dangling blob b0ef021f…`，`git cat-file -p <那个 blob>` 就把原文捞回来了
（demo 用 `fsck | awk '/^dangling blob/{print $3}' | xargs -r git cat-file -p` 串成一条，
注意 `--no-reflogs` 的输出前面还带着 "dangling blob " 前缀，所以取**第 3 列**）。

**真实输出**（④ 90 天回收站：git 内置的三档宽限期）

```text
$ cd /tmp/tmp.95kGtyLeQ2/gc && echo '### 本仓库有没有配过 gc.* 选项：' && git config --local --list | grep -i '^gc' || echo '(一条都没有 → 全部用 git 内置默认值：gc.reflogExpire=90.days / gc.reflogExpireUnreachable=30.days / gc.prune=expire,expire-2-weeks)'
### 本仓库有没有配过 gc.* 选项：
(一条都没有 → 全部用 git 内置默认值：gc.reflogExpire=90.days / gc.reflogExpireUnreachable=30.days / gc.prune=expire,expire-2-weeks)

$ cd /tmp/tmp.95kGtyLeQ2/gc && echo '### 再手滑一次，把 g2/g3 从分支上甩掉：此时它们还活着（reflog + 2 周宽限期）：' && git reset --hard "74f71f5bffd6bbf96ca7a26e568139c581d40849" && git fsck --no-reflogs
### 再手滑一次，把 g2/g3 从分支上甩掉：此时它们还活着（reflog + 2 周宽限期）：
HEAD is now at 74f71f5 g1 起点
dangling commit 86bd29ad9d7741aaa2641f1190291470ab73bc97
dangling blob b0ef021f294159cadac9b9db9818887538e12cf2
```

`git config --local --list | grep '^gc'` 一条都没有 → 说明吃的是 git **内置默认值**：

| 配置 | 默认值 | 含义 |
|---|---|---|
| `gc.reflogExpire` | `90.days` | reflog 里**可达**的记录留 90 天 |
| `gc.reflogExpireUnreachable` | `30.days` | reflog 里**不可达**的记录留 30 天 |
| `gc.pruneExpire` | `2.weeks.ago` | `git gc` 内部就是 `prune --expire 2.weeks.ago`，不可达对象满 2 周才真删 |

（demo 那行注解把三档写成 `gc.reflogExpire=90.days / gc.reflogExpireUnreachable=30.days / gc.prune=expire,expire-2-weeks`，
意思是同一个"过期才删"；命令行的 `--prune=now` / `--expire=now` 就是把这个期限强行推到"现在"。）
所以真实项目里的时间窗是这样的：**分支上没了 → 立刻能靠 reflog 救；
30 天之后 → reflog 里的不可达记录会被 gc 清掉，只能靠 fsck；
2 周之内对象还在库里；谁跑了 `gc --prune=now` → 结束**。

**真实输出**（⑤ 走到终点：`reflog expire` + `gc --prune=now`，之后 `cat-file` 直接 fatal）

```text
$ cd /tmp/tmp.95kGtyLeQ2/gc && echo '### 强制倒垃圾（真实项目里别乱跑）：先让所有 reflog 立刻过期，再 gc --prune=now' && git reflog expire --expire=now --all && git gc --prune=now --quiet && echo '### 倒完之后 fsck 已经什么都找不出来：' && git fsck --no-reflogs && echo '(上面空 = 再也没有悬空对象了)' && git cat-file -t "a078652f2dcf3141435fc046b3e259ae7cd6aa34"
### 强制倒垃圾（真实项目里别乱跑）：先让所有 reflog 立刻过期，再 gc --prune=now
### 倒完之后 fsck 已经什么都找不出来：
(上面空 = 再也没有悬空对象了)
fatal: git cat-file: could not get object info
[命令退出码 = 128 —— 上面的报错是故意让它发生的]
--- 上面最后那个报错就是『真被删掉了』的证据 —— 所以救场要趁早，别等 gc 跑过。
```

`fatal: git cat-file: could not get object info` + `退出码 = 128` 就是"真被删掉了"的物证。
★ **这两条命令是本章唯一的"不可逆操作"**，在真实仓库里绝不要顺手跑：
`reflog expire --expire=now --all` 销毁后悔药，`gc --prune=now` 立刻物理删除所有不可达对象。
CI 里跑的 `git gc`（或 `git fetch --prune` + 自动 gc）会**在你不知情的时候**推进这两个期限 ——
所以"周一删的分支周三还能救"这件事，不能当成依赖。

**真实输出**（⑥ 唯一救不了的情况：从来没 `git add` 过的文件）

```text
$ cd /tmp/tmp.95kGtyLeQ2/gc && printf 'ONLY-ON-DISK-SECRET-42 这行从没进过对象库\n' > 只在磁盘上的文件.txt && git status --short && git -c core.quotepath=false status --short && git fsck --no-reflogs | wc -l
?? "\345\217\252\345\234\250\347\243\201\347\233\230\344\270\212\347\232\204\346\226\207\344\273\266.txt"
?? 只在磁盘上的文件.txt
0
--- 顺带一个小坑：中文文件名默认被转义成一串八进制（core.quotepath=true），加 -c core.quotepath=false 才看得舒服。

$ cd /tmp/tmp.95kGtyLeQ2/gc && git fsck --no-reflogs | wc -l
0

$ cd /tmp/tmp.95kGtyLeQ2/gc && rm -f 只在磁盘上的文件.txt && echo '### 删掉之后，在整个对象库里搜这串字符：' && git log --all -S'ONLY-ON-DISK-SECRET-42' --oneline | wc -l && git fsck --unreachable --no-reflogs | wc -l
### 删掉之后，在整个对象库里搜这串字符：
0
0

$ cd /tmp/tmp.95kGtyLeQ2/gc && echo '### 一条都没有 → 它确实没进过对象库，git 无能为力。结论：只有 add 过的内容才谈得上恢复' && git status --short && echo '(空 = 工作区已干净)'
### 一条都没有 → 它确实没进过对象库，git 无能为力。结论：只有 add 过的内容才谈得上恢复
(空 = 工作区已干净)
```

`?? "\345\217\252\345\234\250…"` 那一行顺便暴露一个小坑：**中文文件名默认被转义成八进制串**
（`core.quotepath=true` 是默认值），加 `-c core.quotepath=false` 才显示成人能读的样子 ——
用 `-c` 只对这一条命令生效，**不要去写全局配置**。
真正的结论在数字上：`git fsck --no-reflogs | wc -l` = `0`，
`git log --all -S'ONLY-ON-DISK-SECRET-42' --oneline | wc -l` = `0`，
`git fsck --unreachable --no-reflogs | wc -l` = `0` ——
**Git 从没见过的东西，它一点办法都没有。** 本章前面所有的"能救"，前提都是"你 add 过至少一次"。

**这章最后一条纪律**：**未提交的改动不是"已经存好了"**。
要么 `git add`（哪怕先扔进 `git stash`，第 06 章），要么就承认它有丢失风险、别放重要的东西。

**常见翻车**

- **`git fsck` 没报东西就以为没事**：默认它把 reflog 算作引用；查悬空要 `--no-reflogs`。
- **只看到 1 个 dangling commit 就以为只丢了 1 个**：fsck 只报顶端，往下走 `git log <顶端>` 看整条链。
- **在别人的生产仓库里跑 `git fsck --lost-found`**：它会往 `.git/lost-found/` 写文件。
- **靠"gc 两周宽限期"当备份**：`git gc --auto` 由别的操作触发，`expire` 与 `prune` 时间点你控制不了。
  **恢复窗口是运气，不是保证。**
- **想恢复"从没 add 过"的文件**：Git 无能为力，该找的是编辑器/文件系统/IDE 的本地历史
  （VS Code 的 Local History、`~/.local/share/Trash`、快照），别在 git 上浪费时间。
- **`git gc --prune=now` 之后又跑 `git fsck`**：`dangling` 全没了是正常的，
  不是"仓库被修好了"；真要说修好，那是 `repack` 的效果，和可达性无关。

---

## 命令速查表

```bash
# ── 缩小范围：git log 的五个阀门（可以叠加，叠加就是 AND）──────
git log --oneline --decorate --graph              # 永远先这样看现场
git log --since='2 weeks ago' --until='3 weeks ago'   # ① 时间（= --after/--before）
git log --author='同事甲' --format='%h %an %s'    # ② 人（正则子串，只匹配 author）
git log --grep='#103' -E --grep='(fix|feat).*#10[0-9]'   # ④ 说明（-E 才用正则）
git log -S'MAX_RETRY'  --oneline                  # ③ 内容：出现次数变了才算（谁写/谁删）
git log -G'MAX_RETRY'  --oneline                  # ③ 内容：diff 里出现过就算（谁碰过）
git log --follow -- <路径>                         # ⑤⑥ 路径（-- 之后才是路径，跨改名加 --follow）
git log --first-parent -8                          # ⑦ 只看主干那一列
git log --no-merges --pretty=tformat:'AUTHOR %an' --numstat | awk …   # ⑨ 行数报表
git shortlog -sne HEAD                             # 每人几条（★ 必须给范围，否则等 stdin）

# ── 看清内容：全是只读，不动工作区 ─────────────────────────
git show <提交>                                    # 一次提交的补丁
git show <提交> -- <路径>                          # 这次提交只对这个文件的部分
git show '<版本>:<路径>'                           # ★ 有冒号 = 看历史快照，不改工作区
git diff '<A>:<路径>' '<B>:<路径>'                 # 只比一个文件的两个版本
git diff A B  /  git diff A..B                     # 两份快照之间（两者等价）
git diff A...B                                     # ★ 共同祖先 → B（代码评审用这个）
git diff '<commit>^!'                              # 只看这一次提交自己引入的改动
git diff / --staged / HEAD                         # 工作区 vs 暂存区 / 暂存 vs HEAD / 全部 vs HEAD
git log -L 9,20:src/calc.c                         # 某几行的完整演化史（★ 本章 demo 没跑它，
                                                   #   但语法在临时沙箱里实测可用，比 blame 更详细）

# ── 谁写的这一行：blame ───────────────────────────────────
git blame <文件>                                   # 整份（大文件慢得离谱）
git blame -L 9,13 <文件>                           # 只这一段；-L '/正则/,+10' 也行
git blame -w -L 9,11 <文件>                        # ★ 忽略空白改动，把抢功还原
git blame -C <文件>                                # 跨文件追"从哪抄的"（-CC 更激进，也更快不来）
git blame -b -M -B <文件>                          # 行尾空白 / 同文件移动 / 拆开移动+改动
git blame --since='2 weeks ago' -L 15,16 <文件>    # 造边界：只看最近谁动过（^ 会变多）
git blame -L 15,15 --porcelain <文件>              # 脚本用：一行一个字段，别去切默认输出的空格
git -c blame.ignoreRevsFile=.git-blame-ignore-revs blame <文件>   # 跳过格式化提交

# ── 自动定位：bisect ──────────────────────────────────────
git bisect start <bad> <good>                      # bad 在前！写反了不报错但结论错
git bisect start --first-parent <bad> <good>       # ★ 有合并提交时用（需较新 git，见场景 6）
git bisect start '<M>^2' '<M>^1'                   # 从合并提交钻进分支
git bisect good / bad / skip                       # skip = 这个版本测不了
git bisect run ./test.sh                           # 0=好 1~49=坏 125=跳过 其它=中止
git bisect terms / log / reset                     # 当前两端 / 决策过程 / ★ 收尾必做
git bisect replay <保存的 bisect log>              # 把别人的二分过程直接复现

# ── 救场：reflog 与 fsck ──────────────────────────────────
git reflog | head -20                              # ★ 出事后的第一反应
git reflog --date=iso                              # 每笔流水账的绝对时间
git log -g --oneline --grep='大改造'               # 在 reflog 上用 log 的检索
git log -g --oneline main                          # 分支指针自己的历史
git rev-parse 'HEAD@{1}' 'main@{2}' '@{-1}' '@{u}' # ★ 全部加引号
git rev-parse 'HEAD@{yesterday}'                   # 昨天那一刻 HEAD 在哪（不是时光机）
git branch rescue <哈希>                           # 先挂住保险，再谈姿势
git cat-file -t <哈希>                             # 对象还在不在（commit/blob/tree/tag）
git fsck --no-reflogs                              # ★ 找悬空顶端（默认会把 reflog 当引用）
git fsck --lost-found                              # 把悬空提交写成文件（★ 会写盘）
git fsck --unreachable --no-reflogs                # 所有不可达对象
git log --all -S'<一段独有文本>' --oneline          # 用内容反查提交（--all 覆盖全部引用）
git -c core.quotepath=false status --short         # 中文文件名不被转义成八进制

# ── 不可逆（禁止随手跑）───────────────────────────────────
git reflog expire --expire=now --all               # ★ 销毁后悔药
git gc --prune=now                                 # ★ 物理删除所有不可达对象
```

---

## 这章的坑

1. **`-G` 和 `-S` 不是一回事，而且 `-S` 默认按字面量**。
   "把两个函数上下换位置"那次提交（demo 里 `5e88b21`），`-G'MAX_RETRY'` 命中、`-S'MAX_RETRY'` 不命中，
   因为**出现次数一次没变**（改前 2 次、改后 2 次，demo 里用 `git show 'HEAD~12:…' | grep -c` 数给你看了）。
   反过来 `-S` 加了正则元字符（`( ) . * |`）也不会报错，它按字面量匹配然后给你 0 条。要用正则必须 `--pickaxe-regex`。

2. **`--since='yesterday'` 是"昨天这一刻"，不是"昨天 0 点"**。
   demo 里差 3 秒就让那条"昨天 10:10:34"的提交查不出来（空输出）。要 0 点就写 `--since='yesterday 00:00'`。

3. **`git shortlog -sne` 不给范围会去等标准输入，看起来像卡死**。
   它是设计成能接管道的（`git log --format='%an <%ae>' | git shortlog -sn`），
   但你在终端裸敲时它就静静等键盘 —— 敲 `Ctrl-D` 或补个 `HEAD` 就活了。

4. **`--numstat` 按空格切列会被改名行弄断**。
   numstat 是 TAB 分隔的 `新增 删除 路径`，但改名行的第三列是
   `docs/{design.md => architecture.md}`（**这一列内部带空格**）——
   demo ⑨ 那条 awk 取 `$3` 当文件名，于是只拿到 `docs/{design.md`，统计表里就多出一条
   莫名其妙的 `docs/{design.md    +2 -0`。列数变了 → 脚本**悄悄**失真。解法：`-z`（NUL 分隔）自己 split，
   或者 `--no-renames` 换口径，或者像 demo 那样在 awk 里守卫 `NF==3`
   （**宁可少算也不误算**）。

5. **`--follow` 是"猜"的，不是记录的**。
   Git 只存快照，从不写改名日志。`--follow` 靠相似度（`R060` = 60%）现场猜同一个文件；
   改动太大（重写+改名）就断，报了 `fatal: --follow requires exactly one pathspec` 是你给了两个路径。
   而且它只跟**一条**线：改名之后再用 `--follow` 追到旧名，就得再用旧名跑一次 blame。

6. **`-- <路径>` 的那两个 dash 不是装饰**。
   `git log README` 如果同时存在一个叫 `README` 的分支，git 会先按引用解析，
   报错信息还特别偏（`ambiguous argument`）。写路径一律 `-- <路径>`。

7. **blame 报的是"最后动这行的人"，`^` 报的是"追到边界"**。
   第 10 行的缩进被 `3a5c74e` 动过一次，默认 blame 就把它归给同事乙，加 `-w` 才还给 `^1d03ac3`。
   `^` 前缀**不等于**"这就是作者"，浅克隆里尤其多。改名之后要 `blame '<旧提交>' -- <旧路径>` 才找回原作者。

8. **`--first-parent` 的 reflog 下标 ≠ 提交代数**（本章两条最容易混的点）。
   `HEAD@{0}` 和 `HEAD@{1}` 可以是同一个哈希（原地 `reset: moving to HEAD`）；
   `git reflog` 的条数被 checkout 灌水。要"往前第 3 个提交"用 `HEAD~3`，要"第 3 条记录"才用 `HEAD@{3}`。

9. **`HEAD@{很久以前}` 超出 reflog 覆盖范围时不报错，直接给你最新那条**。
   demo 用 `git rev-parse 'HEAD@{100 years ago}'` 和 `'HEAD@{0}'` 并排打印，两个哈希一模一样 ——
   静默骗人。**用 `@{日期}` 之前先看 `git reflog --date=iso | tail`。**

10. **普通 bisect 在有合并提交的主干上会把分支内部提交当候选**。
    demo 4b 里它第一次就问了 `x1`（那个组合从没在主干上跑过）。
    结论 `ec9d350 x2` 回答的是"谁写坏的"，`--first-parent` 的结论 `c78a66d`（合并提交）回答的才是
    "什么时候进的主干"。**运维视角要的是第二个。** 而且分支内部状态常常编译不过 → 退出码脏 → 二分前提崩掉。

11. **bisect 的退出码约定只有 0 / 1~49 / 125 三个安全档**。
    `exit 99`、脚本没 `chmod +x`（126）、文件名写错（127）都会让 bisect **中止**而不是"标成坏"。
    判定脚本必须在**第一个提交里就存在**（否则前面的版本没这个文件，`run` 立刻 127）。
    收尾 `git bisect reset` 不是客气，HEAD 会一直游离在历史提交上。

12. **`git fsck` 默认把 reflog 当引用，所以"什么都没找到"≠"没丢东西"**。
    查悬空要 `--no-reflogs`；`--lost-found` 只报链条顶端，下面的要靠 `git log <顶端>` 顺藤摸瓜。
    另：`--lost-found` 会往 `.git/lost-found/` **写文件**，不是纯只读命令。

13. **恢复窗口是运气，不是保证**。
    默认 `gc.reflogExpire=90.days` / `gc.reflogExpireUnreachable=30.days` / 不可达对象 2 周才删，
    但 `git gc --auto` 会被别的操作触发，谁跑一次 `reflog expire --expire=now --all && gc --prune=now`
    就直接归零（本文场景 8 ⑤ 实测：之后 `cat-file` 报 fatal）。**救场要趁早，别等 gc 跑过。**

14. **从没 `git add` 过的内容，Git 一律救不了**。
    本文场景 8 ⑥ 造了个只在磁盘上的文件，删掉之后 `log --all -S'…'`、`fsck --unreachable` **全是 0 命中** ——
    因为对象库里压根没有它。这条是全章的地板：上面所有"能救"都以"至少 add 过一次"为前提。

15. **本章 demo 的哈希分两类**：第 `4/4b/6` 节用固定时间戳，哈希**逐字节可复现**；
    第 `0/1/2/3/5` 节故意用"相对运行当天"的日期（否则 `--since='2 weeks ago'`、`@{yesterday}` 查不出东西），
    所以**每次跑哈希和日期都会变**，但"命中几条、命中哪几条、二分问几次、结论落在哪个提交"不变。
    照抄本文哈希时先确认你跑的是哪一节。

---

## 自检问题

**Q1：同事说"这行 `MAX_RETRY` 我没动过啊"，你想确认他有没有说过谎（不是）。该用 `-S` 还是 `-G`？为什么？**

**答：** 用 `-G'MAX_RETRY'`。`-G` 问的是"这次提交的 diff 里有没有含 `MAX_RETRY` 的行"，
所以"把两个函数上下换位置"这种**次数一次没变**的搬家提交也会被它抓住（demo 实测 `-G` 命中 5 条）；
`-S` 只认"出现次数变了"（命中 4 条），搬家类提交它天生看不见。
想抓"谁写的 / 谁删的"就用 `-S`（更准，噪音少）。口诀：**找"谁碰过"用 `-G`，找"谁改数量"用 `-S`**，
两者互补，只用一个一定漏。

**Q2：`git show HEAD~13:src/calc.c` 和 `git show HEAD~13 src/calc.c` 差别在哪？少那个冒号会怎样？**

**答：** 带冒号 = "看**那个版本里那个路径**的快照"（等价于一个对象地址 `<rev>:<path>`），输出文件内容。
不带冒号 = `git show HEAD~13 src/calc.c`，git 会把 `src/calc.c` 当成**路径过滤**用，
输出 `HEAD~13` 那次提交的补丁（只显示和这个文件有关的部分），**不给你文件内容**。
两个都不动工作区，这是本章反复强调的"只读地看历史"。
顺带记法：**冒号左边是版本，右边是路径**；`-- <路径>` 里的 `--` 是"后面全是路径"的分隔符，作用不同。

**Q3：blame 输出里一行开头是 `^1d03ac3`，同事说"前面带 `^` 说明这行不是我写的，是原作者"。他对吗？**

**答：** 不完全对，而且错的方向很危险。`^` 的真实含义是"**这次追溯到这里就到边界了**"
（默认边界是仓库的根提交；浅克隆是浅边界）。它只说明"在我看得见的这段历史里没人动过这行"，
**不代表"这就是写下这行想法的人"**。
浅克隆里 `^` 大量出现，这时候 blame 的结论几乎没参考价值 —— 先 `git fetch --unshallow` 再谈。
反过来说：把"缩进被动过"的行也算给改缩进的人，是**默认行为**，要还原给原作者得加 `-w`。

**Q4：主干上有 20 多次合并，现在线上坏了。你想用 bisect，第一步该改什么？**

**答：** 在 `start` 上加 `--first-parent`：`git bisect start --first-parent HEAD <已知好的主干标签>`。
这样只在主干那一列里取中点，**每一个被测版本都是"当时主干真实跑过的状态"**，判据单调；
合并进来的整块被当成一个单元，所以结论会停在**合并提交**上 —— 那正是"坏代码进入主干"的那一刻。
需要再精确到分支里哪一条，第二击 `git bisect start 'M^2' 'M^1'`（坏端=分支那侧，好端=主干那侧），
中途 git 会要求先测一次两者的 merge base（`Bisecting: a merge base must be tested`），标 GOOD 继续即可。

**Q5：你 `git reset --hard HEAD~5` 之后立刻 `git checkout main`，然后想找回那 5 条提交。
`HEAD@{1}` 能用吗？该用什么？**

**答：** **别用下标，用搜索。** `HEAD@{1}` 只是"上一条流水账"，你中间又 checkout 了一次，
它现在指向的是 checkout 那条记录，不是被甩掉的提交。正确做法：
`git reflog | head -20` 找到那条 `commit:` 记录抄哈希；或者
`git log -g --oneline --grep='<那次提交的关键词>'` 直接在 reflog 上搜；
再用 `git rev-parse '<你抄下的哈希>'` / `git show --stat <哈希>` 交叉确认；
最后 `git branch rescue <哈希>` 先挂住，再 `git reset --hard <哈希>`（或 `git merge rescue`）。
唯一"下标可用"的情况是你**出事之后没再敲任何移动 HEAD 的命令** —— 所以第一条建议永远是"停手先查"。

**Q6：`git fsck` 什么都没报，是不是说明我的提交没丢？**

**答：** 不是。默认 `fsck` 把 **reflog 也算作引用**，只要 reflog 还指着那个提交，它就不算悬空，fsck 沉默。
查"分支上没了但库里还在"要 `git fsck --no-reflogs`（demo 里正是这一步才冒出 `dangling commit 86bd29ad…`）；
要看更全就加 `--unreachable`。
另一个反向的坑：`--lost-found` **只报链条顶端**，只列出 1 个不代表只丢了 1 个，往下 `git log <顶端>` 才看到整条链。

**Q7：什么情况下 Git 一定救不回来？给出 demo 里的两个"终点"。**

**答：** 两个：
① **回收站被清空** —— `git reflog expire --expire=now --all && git gc --prune=now` 之后，
不可达对象被物理删除，`git cat-file -t <那个提交>` 直接 `fatal: git cat-file: could not get object info`（退出码 128）。
② **从来没 `git add` 过** —— 内容压根没进对象库，`fsck --unreachable` 和 `log --all -S'<独有字符串>'`
都是 0 命中，Git 无从谈起（这时候去找编辑器/IDE 的本地历史或文件系统快照）。
中间地带全靠时间：默认 reflog 可达 90 天 / 不可达 30 天 / 不可达对象 2 周才删，但这些都不是保证。

---

## 怎么跑这章的 demo

```bash
cd git-handbook/08-investigation
export PATH="$HOME/.local/bin:$PATH"     # 如果你的 git 装在用户目录里
bash demo.sh              # 跑完自动删沙箱，屏幕输出就是本章全部内容
bash demo.sh --keep       # 保留沙箱并打印路径，可以 cd 进去自己敲
```

`--keep` 跑完你会看到四个现场目录，逐个值得进去翻：

| 目录 | 对应场景 | 里面留着什么 |
|---|---|---|
| `inv/` | 1~4（场景 1~4） | 15 个提交的"像真实项目"现场：3 个作者、跨 5 周、含改名/合并/纯空白/复制粘贴；还有个本地 bare `../inv-origin.git` 冒充远程 |
| `bis/` | 场景 5 | 24 个提交的 C 小程序，`test.sh` 从第 1 个提交就在仓库里（这点是 `run` 能跑的前提） |
| `bm/` | 场景 6 | 26 个提交、一次 `--no-ff` 合并、真凶藏在分支里；`base_ok` 是打在 `m1` 上的标签 |
| `gc/` | 场景 8 | g1/g2/g3 三个提交，最后跑过 `reflog expire` + `gc --prune=now`，**对象真没了**（终点长这样） |

脚本本身的几个特征（照着改自己章节的 demo 也用得上）：

- `set -euo pipefail` + `trap cleanup EXIT`：中途出错也不会把沙箱留在 /tmp 里；`--keep` 才保留。
- 全程 `mktemp -d` 沙箱，**绝不碰你机器上任何真实仓库**；`export LC_ALL=C LANG=C` 所以输出是英文。
- 提交身份一律 `git -c user.name=学习者 -c user.email=learn@example.com`（脚本里收成 `$G`），
  **不读也不写你的全局配置**。屏幕上为了好读，`pretty()` 会把这串 `-c` 抹掉，真实执行是带上的。
- 每条演示命令由 `R()` 打印出来再执行，**故意制造的错误**后面会补一行
  `[命令退出码 = N —— 上面的报错是故意让它发生的]`；看到这句就是教学重点。
- 依赖 **GNU `date -d`**（造"相对运行当天"的提交日期）、**`cc`**（`bis/` 和 `bm/` 要编译 C 来判定好坏）、
  以及支持 `git bisect start --first-parent` 的 Git（`git help bisect` 的 SYNOPSIS 里有这个开关即可；
  本机 git 2.55.0 实测可用，`cc` 也可用）。
- 小依赖：脚本用 `git init -q`（没写 `-b main`），**要求你的 `init.defaultBranch` 是 `main`**，
  否则第 4/4b 节会因为找不到 `main` 而失败。想临时改而不污染全局配置，可以
  `git -c init.defaultBranch=main …` 或者写进沙箱里那个仓库的局部配置。

**关于哈希**：第 `4/4b/6` 节用固定时间戳 `2026-01-01T10:00:00 +0800`，你跑出来的哈希和本文一模一样
（本章连跑两次做过对照）；第 `0/1/2/3/5` 节是相对运行当天的日期，**哈希会变**，
本文贴的是 `/tmp/tmp.95kGtyLeQ2` 那一次的结果 —— 对照自己的输出时看**命令、命中条数和结论**，别看哈希。

**你机器上如果是中文输出**：本章 demo 里所有 git 消息都是英文（`LC_ALL=C`）。
本文只给了 5 组我实测过的中英对照（见场景 2 末尾），你换 `LANGUAGE=zh_CN` 跑时不一定每个词都有词条。

---

## 下一步

本章把"排查"闭环了：**log 缩范围 → show/diff/blame 看内容 → bisect 自动定位 → reflog/fsck 救场**。
真正动手改历史（不只是看）是第 05 章 `05-rebase-history`（改写历史与 rebase，本章末尾的
`author 不变 / committer 变` 在那里是主角）；把这套取证能力用到"多人协作"上是第 07 章
`07-remote-collab`（`@{u}`、`origin/main` 和跟踪关系那部分本章已经碰过）。

后面两章还没写 demo，但和本章咬得最紧：

- **09 子模块与 worktree（`09-submodule-worktree`）**：一个仓库里"多个工作区并存"，
  那时你会需要一个常驻的取证 worktree（`git worktree add ../debug <某个历史提交>`），
  而不用来回 checkout —— 本章的 `bisect` 会顺手帮你建（`bisect` 自己就在动 HEAD）。
- **10 排障与配置（`10-troubleshooting`）**：`git config` 分层（`--global`/`--local`/`--worktree`/`-c` 临时值）、
  `GIT_*` 环境变量（`GIT_DIR`/`GIT_WORK_TREE`/`GIT_AUTHOR_DATE`/`GIT_CONFIG_GLOBAL`）、
  `git bugreport`、索引锁 `index.lock` 之类"仓库为什么坏了"的问题 —— 本章用了两处
  （`-c core.quotepath=false`、`GIT_AUTHOR_DATE`），到那里会讲透。

再往回看时，建议顺序：`01-mental-model`（三区 + 指针的图，本章所有命令都建在它上面）→
`03-undo`（`reset`/`restore`/`reflog` 的操作视角，本章是它的取证视角）。



