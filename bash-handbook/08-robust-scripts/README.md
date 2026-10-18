# 第 08 章 · 写出炸不响的脚本 —— 四件衣服、陷阱的三个时间差、三道边界

> 这一章只管三件事：**`set -e / -u / -o pipefail / -E` 这四件衣服各自挡什么、互不顶班**、
> **陷阱（EXIT / ERR）在什么时刻读到什么值、清理跑到一半被打断会留下什么**、
> **参数与文件名从 `getopts` 走到 `rm` 的那条链上，哪一步会把事情悄悄改掉**。
> 下面每一行屏幕输出都不是人打的：它出自同一次 `bash demo.sh > ~/.cache/handbook-captures/bash08.txt`
> （1381 行、18 节），正文里每一格都由脚本从那份**已定稿的捕获**里按行号原样搬出来 ——
> 人只负责写正文和选区间，所以严格校验天然为 0。
> 这台机器的底细：bash 5.3.9(1)-release，GNU rm 9.7 / GNU sed 4.9 / GNU grep 3.12 / GNU findutils 4.10，
> 而 **cat、wc、tr、sort、head、paste 是 uutils coreutils 0.8.0**，awk 的真身是 **mawk 1.3.4**，
> `/bin/sh` 是 `/usr/bin/dash`，没装 `shellcheck`；
> demo 一进来就把 `LC_ALL`、`LANG`、`LANGUAGE` 三个一起钉成 C，所以**这份捕获里的报错全是英文原文**
> （第 1 节当场演给你看：只钉 `LC_ALL` 是钉不住的，`LANGUAGE` 排在它前面）。
>
> 怎么跑这一章：
>
> ```
> cd bash-handbook/08-robust-scripts
> bash demo.sh            # 18 节全跑一遍，跑完自动删沙箱
> bash demo.sh --keep     # 跑完保留沙箱，并打印进去接着敲的路径
> ```
>
> 安全边界：本章**不联网、不装包、不开监听端口、不碰 `~/.bashrc`**；
> 所有写操作只发生在 `mktemp -d` 出来的沙箱里（前缀 `bash08-`），由 `trap cleanup EXIT` 整体删掉，
> 六个取证脚本各自只在自己的子目录里造现场、跑完自己删，第 18 节当场自证一遍「沙箱外面一个条目都没多」。
> 输出是确定的：个数、行数、码、布尔判断每一跑都一样（两遍 diff 为空）。
> **唯一每跑必变的只有沙箱那个随机名**，所以它压根不上屏，正文里一律写成 `<沙箱>`。

---

## 本章在讲什么，以及为什么是这三条

前面七章你已经拿到三件工具：**每句话只有一个退出码**（第 03 章）、
**展开与词表是看不见的手**（第 02、04 章）、**函数只有码、名字是动态作用域**（第 06 章）。
把它们接到「写一个放到生产里跑的脚本」这件事上，就会长出三条反直觉：

> **一、`set -euo pipefail` 不是一句咒语，是四个互不相干的开关，而且每个都有豁免区。**
> `-e` 只管「普通位置上的失败」，写在 `&&`、`||`、`if`、`!` 后面的失败它一概不管；
> `-o pipefail` 只改**管道交出什么码**，它自己不会终止脚本；
> `-u` 只管「没定义过的名字被读」，而本机 bash 5.3 里**空数组整体展开它管不着**；
> `-E` 既不报错也不终止，只管一件事 —— ERR trap 进不进函数体。
> 于是「穿了 `-e` 却什么都没挡」不是玄学：管道前一棒失败了，最后一棒成功，
> 码在这一路上**就没变成非 0**，`-e` 无从判起（第 3、4 节）。
> 更贵的一条是第 5 节第 7 格：把函数放进 `if` 的条件位，**函数体内的 `-e` 整块关灯**，
> 里面失败了照样往下走、照样返回 0 —— 三件套穿满也拦不住。
>
> **二、陷阱的三个时间差：定义时、触发时、清理被中断的那一刻。**
> `trap "echo $?" EXIT` 里的 `$?` 在**写陷阱那一行**就被展开成了一个具体数字，
> 想留到执行时得写 `\$?`（第 7 节第 1 格）；
> 就算留到了执行时，陷阱体里第一句 `x=1` 会把退出码顶成 0，
> 所以清理函数第一句永远是 `__rc=$?`（第 7 节第 5 格、第 8 节整节）；
> 而清理跑到一半被第二个信号打断时，屏幕上会留下**一个真实的半截现场**：
> 陷阱进去了一条命令、没进第二条，`trapsafe/work/` 里剩 1 个文件（第 7 节第 9 格）。
> ERR 陷阱只负责「响」，`-e` 只负责「停」，两件互不替（第 7 节第 2 格）。
>
> **三、参数与名字这条链上，出错的那一步通常不报错。**
> `getopts` 的 `?` 分支里 `OPTARG` 在**非静默模式下根本没设**；`--` 会被带值选项当值吃掉；
> 第一个非选项之后 getopts 就停了，后面再写 `-b` 也不会被当选项（第 9 节）。
> 名字这边：4 个带空格/中文/换行/前导 `-` 的名字，无引号展开变 7 段，
> `find -print | xargs` 收到 7 条而 `-print0 | xargs -0` 收到 4 条，
> `rm -f -- $@` 那一跑**码 0、一声没出，却只删掉 1 个**（第 13、14 节）。
> 判断跟着链接走、写入也跟着链接走，「先测后开」中间那一步由别人替你选路径（第 11、12 节）。

三条主线各自的完整取证在：**第 3~5 节**（四件衣服与 `-e` 的豁免区）、
**第 7~8 节**（陷阱）、**第 9~16 节**（参数、文件、名字、骨架、dash）。
第 15 节把全部结论拼成一个能上线的骨架 `skel.sh`，第 16 节再把它交给 `/bin/sh` 撞一遍。

---

## 1. 先把这台机器的发言权确认一遍

本章每条结论都跟三样东西绑定：**哪个 bash**（决定有没有 `ERR` 陷阱、空数组那条边界）、
**哪个厂商的 rm/wc/awk**（决定报错长什么样）、**哪一套语言环境**（决定那句话用什么语言说）。
先把三样钉死。

```
$ bash --version | head -1
GNU bash, version 5.3.9(1)-release (x86_64-pc-linux-gnu)
[退出码 0]

$ echo "demo 已把自己改成：LC_ALL=[$LC_ALL] LANG=[$LANG] LANGUAGE=[$LANGUAGE]（你原来的终端是 LANG=$ORIG_LANG，LC_ALL=$ORIG_LC_ALL，LANGUAGE=$ORIG_LANGUAGE）"
demo 已把自己改成：LC_ALL=[C.UTF-8] LANG=[C.UTF-8] LANGUAGE=[C]（你原来的终端是 LANG=zh_CN.UTF-8，LC_ALL=（未设），LANGUAGE=zh_CN:zh）
[退出码 0]
```

外部命令的「谁家」这件事在本章不是 trivia：第 9 节第 8 格那条 `rm` 报错来自 GNU coreutils，
而 `cat` 报的是 uutils 的风格，同一件事两家话不一样；`wc -l` 数的是换行符这条账在第 13 节要用。

```
$ for c in rm cat wc tr sort head awk sed grep find xargs paste; do printf "  %-7s → %s\n" "$c" "$("$c" --version 2>&1 | head -1)"; done
  rm      → rm (GNU coreutils) 9.7
  cat     → cat (uutils coreutils) 0.8.0
  wc      → wc (uutils coreutils) 0.8.0
  tr      → tr (uutils coreutils) 0.8.0
  sort    → sort (uutils coreutils) 0.8.0
  head    → head (uutils coreutils) 0.8.0
  awk     → mawk 1.3.4 20260129
  sed     → sed (GNU sed) 4.9
  grep    → grep (GNU grep) 3.12
  find    → find (GNU findutils) 4.10.0
  xargs   → xargs (GNU findutils) 4.10.0
  paste   → paste (uutils coreutils) 0.8.0
[退出码 0]
```

然后是这一章最容易踩空的一步：**报错那句话的语言，不是由 `LC_ALL` 单独决定的**。
GNU 的 gettext 按 `LANGUAGE > LC_MESSAGES > LANG` 的优先顺序挑语言，
`LC_ALL` 能盖住后两个，却盖不住一个已经设好的 `LANGUAGE`。
下面这三行是**同一条命令**在三种组合下的原话（都是本机真实输出）：

```
$ mkdir -p loc && touch -- loc/-dash.txt
printf "  ① LC_ALL=C.UTF-8 + LANGUAGE=C（本章钉的就是这组）：%s\n" "$( (cd loc && env LC_ALL=C.UTF-8 LANGUAGE=C rm -f -dash.txt) 2>&1 | head -1)"
touch -- loc/-dash.txt
printf "  ② LC_ALL=zh_CN.UTF-8，把 LANGUAGE 撤掉：%s\n" "$( (cd loc && env -u LANGUAGE LC_ALL=zh_CN.UTF-8 rm -f -dash.txt) 2>&1 | head -1)"
touch -- loc/-dash.txt
printf "  ③ LC_ALL=zh_CN.UTF-8，但 LANGUAGE=C 还在：%s ← ★ 换语言没生效\n" "$( (cd loc && env LANGUAGE=C LC_ALL=zh_CN.UTF-8 rm -f -dash.txt) 2>&1 | head -1)"
touch -- loc/-dash.txt
printf "  bash 自己在 ② 那组下：%s\n" "$(env -u LANGUAGE LC_ALL=zh_CN.UTF-8 bash -c "while getopts \"a:b\" o; do :; done" _ -z 2>&1 | head -1)"
printf "  bash 自己在 ③ 那组下：%s\n" "$(env LANGUAGE=C LC_ALL=zh_CN.UTF-8 bash -c "while getopts \"a:b\" o; do :; done" _ -z 2>&1 | head -1)"
printf "  ★ 三次 rm 全都报错，那个文件还在吗：%s（错的就是没删成 —— 第 9、13 节各放大一次）\n" "$( [ -e loc/-dash.txt ] && echo 在 || echo 不在了 )"
printf "  你原来的终端是 LANG=%s、LANGUAGE=%s、LC_ALL=%s —— 所以单独跑本章脚本看到的是中文；\n" "$ORIG_LANG" "$ORIG_LANGUAGE" "$ORIG_LC_ALL"
printf "  demo 把 LC_ALL 与 LANGUAGE 一起钉住，这份捕获里才是英文。两种都是本机真实输出。\n"
rm -f -- loc/-dash.txt
rm -rf loc
  ① LC_ALL=C.UTF-8 + LANGUAGE=C（本章钉的就是这组）：rm: invalid option -- 'a'
  ② LC_ALL=zh_CN.UTF-8，把 LANGUAGE 撤掉：rm: 无效的选项 -- a
  ③ LC_ALL=zh_CN.UTF-8，但 LANGUAGE=C 还在：rm: invalid option -- 'a' ← ★ 换语言没生效
  bash 自己在 ② 那组下：_: 非法的选项 -- z
  bash 自己在 ③ 那组下：_: illegal option -- z
  ★ 三次 rm 全都报错，那个文件还在吗：在（错的就是没删成 —— 第 9、13 节各放大一次）
  你原来的终端是 LANG=zh_CN.UTF-8、LANGUAGE=zh_CN:zh、LC_ALL=（未设） —— 所以单独跑本章脚本看到的是中文；
  demo 把 LC_ALL 与 LANGUAGE 一起钉住，这份捕获里才是英文。两种都是本机真实输出。
[退出码 0]
```

看第 ② 与第 ③ 行：`LC_ALL` 一模一样是 `zh_CN.UTF-8`，只多了个 `LANGUAGE=C`，
中文就没了。所以「我把 locale 设了怎么还是英文」的答案在这儿 —— 不是没生效，是被 `LANGUAGE` 顶掉了。
最后那行 ★ 也别放过：**三次 rm 全部报错，那个文件一个都没删掉**。
「命令失败了但脚本还活着」和「命令失败了所以没做成」是两件事，第 9 节、第 13 节各放大一次。

```
$ printf "  /bin/sh 的真身=%s\n" "$(readlink -f /bin/sh)"; echo "  ★ 第 16 节要让本章的骨架去撞一下它"
  /bin/sh 的真身=/usr/bin/dash
  ★ 第 16 节要让本章的骨架去撞一下它
[退出码 0]

$ command -v shellcheck >/dev/null 2>&1 && echo "shellcheck 在" || echo "shellcheck 本机没有 —— 本章不装东西，脚本只能靠逐条真跑来验"
shellcheck 本机没有 —— 本章不装东西，脚本只能靠逐条真跑来验
[退出码 0]
```

`/bin/sh` 是 dash 这一条留到第 16 节结账；`shellcheck` 没有，意味着本章所有脚本只能靠**逐条真跑**来验，
这也正是本章的取证方式（每个脚本都被 `script_check.py` 跑两遍并要求两遍一致、零残留）。

```
$ for o in errexit nounset pipefail errtrace noclobber; do printf "  %-12s %s\n" "$o" "$(set -o 2>/dev/null | awk "\$1==\"$o\"{print \$2}")"; done
  errexit      off
  nounset      off
  pipefail     off
  errtrace     off
  noclobber    off
[退出码 0]

$ printf "  BASHOPTS=[%s] ｜ SHELLOPTS=[%s]\n" "${BASHOPTS-未设}" "${SHELLOPTS-未设}"
  BASHOPTS=[checkwinsize:cmdhist:complete_fullquote:extquote:force_fignore:globasciiranges:globskipdots:hostcomplete:interactive_comments:patsub_replacement:progcomp:promptvars:sourcepath] ｜ SHELLOPTS=[braceexpand:hashall:interactive-comments]
[退出码 0]

$ printf "  空数组在 set -u 下展开（第 3 节第 5 格要用）→ %s\n" "$(bash -c "set -u; a=(); echo \"[\${a[@]}]\"" 2>&1)"
  空数组在 set -u 下展开（第 3 节第 5 格要用）→ []
[退出码 0]
```

四件衣服的默认位置全是 off —— 这一屏是第 3 节的起点：**不写就等于没穿**。
最后一行是提前替第 3 节第 5 格埋的点：本机 bash 5.3 里 `set -u` 下展开空数组**不报错**，
老手册里「`set -u` 会因空数组炸掉」那条结论在这台机器上已经不成立。

---

## 2. 一张地图：三条主线各一条最小证据

```
# 先把三笔账各亮一次，后面 16 节都在把它们放大到「为什么会这样」。
# 主线①：四件衣服不互相顶班 —— 管道里前一棒失败，-e 单穿根本看不见。

$ bash -c "set -e; false | true; echo 走到这里，码=\$?"
走到这里，码=0
[退出码 0]
# 主线②：陷阱体里第一条赋值就把 $? 吞了 —— 清理代码写在前，退出码就变了。

$ trap "x=1; echo \"  EXIT 陷阱读到的码=[\$?]\"" EXIT; exit 7
  EXIT 陷阱读到的码=[0]
[退出码 7]
# 主线③：怪名字一路不报错，只在「个数」上露馅 —— 4 个名字，无引号展开变 7 段。

$ a=("有 空格.txt" "中文 名.txt" "$(printf "带\n换行.txt")" "-dash.txt"); c=0; for f in ${a[@]}; do c=$((c + 1)); done; printf "  数组元素 %s 个，无引号循环体走 %s 次\n" "${#a[@]}" "$c"
  数组元素 4 个，无引号循环体走 7 次
[退出码 0]
```

三条各 1~2 行，但形状就是后面 16 节的全部内容：
第一格 `-e` 单穿，管道里 `false` 那一棒失败了，最后一棒 `true` 交出 0，脚本照跑；
第二格陷阱体第一句 `x=1` 把 `$?` 吃掉，于是清理代码读到的退出码是 0 而不是 7；
第三格数组里明明是 4 个名字，去掉引号之后循环体走了 7 次 —— 而且**一声没出**。

---

## 3. strictfour.sh 完整跑：四件衣服各挡什么

这一节的脚本把 `-e`、`-u`、`-o pipefail`、`-E` 一件一件穿上，每格都是「穿前 / 穿后」并排，
屏幕上两行 `rc` 的差就是那件衣服的分工。它跑的是 29 条 `bash -c`，每条的码在最后第 11 格一起收账。

```
=== 1. 先把四件衣服的默认位置钉死：全是 off ===
  本机 bash 版本串：5.3.9(1)-release
    errexit        	off
    errtrace       	off
    nounset        	off
    pipefail       	off
  → 上面那块是四件衣服的学名与默认位置（-e=errexit、-u=nounset、-o pipefail、-E=errtrace）；不写就等于没穿。
  ★ 别把 -E 和 functrace 混 —— 那件管 DEBUG trap。下面每格都是穿前/穿后并排，两行 rc 的差就是分工。
```

```
=== 2. 衣服一 -e：挡的是「普通位置上的失败」 ===
  敲的是：false; echo "不穿：还活着"
    不穿：还活着
  rc=0
  敲的是：set -e; false; echo "穿了：这行印不出来"
  rc=1
  → 不穿 rc=0（失败没人认领），穿了 rc=1 且后半句没执行 —— 它只认「普通位置」，条件位一概不管（那半边归 errescue.sh）。
```

`-e` 的第一课不是它能挡什么，而是**它挡的那条线在哪**：条件位（`&&`、`||`、`if`、`!`）它一概不管，
那半边归第 5 节。

```
=== 3. 衣服二 -u：挡「没定义过的名字被读」，顺带看一件衣服怎么改掉另一件的码 ===
  敲的是：set -u; echo "读到=[$NOPE]"; echo "后面还在"
    bash: line 1: NOPE: unbound variable
  rc=127
  敲的是：set -u; echo "换成 \${NOPE-} 写法：读到=[${NOPE-}]"; echo "后面还在"
    换成 ${NOPE-} 写法：读到=[]
    后面还在
  rc=0
  敲的是：set -euo pipefail; echo "读到=[$NOPE]"; echo "后面还在"
    bash: line 1: NOPE: unbound variable
  rc=1
  → 只穿 -u：报错原文「unbound variable」＋码 127；换成 ${NOPE-} 就正常读出空串、码 0。
  → ★ 第三条把三件套穿满，同一条命令、同一句报错，码却从 127 变成 1 —— 报错由 -u 触发，退出码却由 -e 决定。
```

第 3 格那一行 ★ 是这一节最容易被写错的地方：同一个错误，**只穿 `-u` 时码是 127，穿满三件套时码是 1**。
报错由 `-u` 触发，退出码却由 `-e` 决定 —— 所以「`-u` 的码是 127」这句话不能孤立地背，
第 5 节和第 9 节还会再撞一次这两个码。

```
=== 4. 衣服二实战：读一个没传进来的位置参数 ===
  敲的是：set -u; echo "穿了：[$1]"; echo "后面还在"
    bash: line 1: $1: unbound variable
  rc=127
  敲的是：set -u; echo "换成 \${1-} 写法：[${1-}] 个数=$#"; echo "后面还在"
    换成 ${1-} 写法：[] 个数=0
    后面还在
  rc=0
  敲的是：set -u; printf "[%s]\n" "$@"; echo "一个参数都没转发，后面还在"
    []
    一个参数都没转发，后面还在
  rc=0
  → 报错原文里的名字是 $1（不是变量名打错），码还是 127 —— 新脚本最常见的第一次崩就是忘了传参。
  → ${1-} 与 "$@" 都活着（rc=0）：前者兜底成空串，后者空参数展开在本机不算未绑定（边界见第 5 格）。
```

新脚本第一次崩多半是这个形状：忘了传参，`$1` 在 `-u` 下当场死掉，
连「你没给参数」那句人话都来不及说。`${1-}` 与 `"$@"` 是两条活路。

```
=== 5. 衣服二的边界：本机 bash 5.3 里空数组展开不报错（老手册那条结论已过期） ===
  敲的是：set -u; a=(); echo "空数组长度=${#a[@]}"; printf "[%s]\n" "${a[@]}"; printf "[%s]\n" "${a[*]}"; echo "还在"
    空数组长度=0
    []
    []
    还在
  rc=0
  敲的是：set -u; echo "压根没声明过的 c：展开=[${c[@]}]，还在"
    压根没声明过的 c：展开=[]，还在
  rc=0
  敲的是：set -u; echo "压根没声明过的 c：长度=${#c[@]}"
    bash: line 1: c: unbound variable
  rc=1
  敲的是：set -u; b=(); echo "空数组取第 0 个=[${b[0]}]"
    bash: line 1: b[0]: unbound variable
  rc=127
  → 四条里两条活着、两条报错：空数组整体展开（[@] 与 [*]）和没声明过的名字整体展开都不报。
  → ★ 报的是「对没声明的名字求长度」（码 1）与「取具体下标」（码 127），两种码还不一样。
    所以「set -u 会因空数组炸掉」在本机不成立、别照抄，要防的是下标与 ${#…[@]} 这两种写法。
```

这一格专门用来推翻旧结论：四条里两条活着、两条报错，
而报错的两种**码还不一样**（求长度 1、取具体下标 127）。
要防的是 `${#c[@]}` 与 `${b[0]}`，不是空数组整体展开。

```
=== 6. 衣服三 -o pipefail：管道的码默认只算最后一棒 ===
  敲的是：false | true; echo "不穿：管道码=$?"
    不穿：管道码=0
  rc=0
  敲的是：set -o pipefail; false | true; echo "穿了：管道码=$?"
    穿了：管道码=1
  rc=0
  → 同一格：不穿 0、穿了 1。★ 两行的 rc 却都是 0 —— 衣服三只改变「管道交出什么码」，它自己不会终止脚本；
    要让它终止脚本得再穿一件 -e（第 8 格）。这是四件互不相干的第一个证据。
```

★ 这一格是本节最反直觉的一行：**两行的 rc 都是 0，但管道码从 0 变成了 1**。
`-o pipefail` 只改「管道交出什么码」，它自己不会终止脚本；
要让它真的终止，得再穿 `-e`（第 8 格）。

```
=== 7. 衣服三实战：grep 一行都没中时，码才第一次有意义 ===
  ★ 先证明这道计数会响：把探针 strict/probe.txt（含要找的 zzznomatch）一起算进来 → 命中 1 行
  撤掉探针 → 命中 0 行（响过一次，这个 0 才算数；下面管道里的 grep 就是这一次真没中）
  敲的是：grep zzznomatch strict/p.txt | cat; echo "不穿：管道码=$?"
    不穿：管道码=0
  rc=0
  敲的是：set -o pipefail; grep zzznomatch strict/p.txt | cat; echo "穿了：管道码=$?"
    穿了：管道码=1
  rc=0
  敲的是：set -o pipefail; grep gamma strict/p.txt | cat; echo "有匹配时穿不穿都一样：码=$?"
    gamma
    有匹配时穿不穿都一样：码=0
  rc=0
  → 真实脚本里最常见的形状就是「grep 一下再交给 cat/awk/sed」：不穿 pipefail 时 grep 一行没中（上面实测
    命中 0 行）也被报成 0 —— 「查无此事」和「查到了」在码上长得一模一样。
  → 第 3 行是对照：换成命中的 gamma，两种穿法都交 0，而且 gamma 那一行被 cat 原样打了出来。
```

`grep 一下再交给 cat/awk/sed` 是真实脚本里最常见的形状，
而不穿 pipefail 时「一行都没中」和「查到了」在码上长得一模一样。
注意这一格开头那两行 ★：**先把计数守卫弄响**（探针命中 1 行），再撤掉探针得 0 ——
否则后面那个「命中 0 行」等于没验。

```
=== 8. 衣服不互相顶班：-e 单穿挡不住管道里的失败 ===
  敲的是：set -e; grep zzznomatch strict/p.txt | cat; echo "只穿 -e：还在"
    只穿 -e：还在
  rc=0
  敲的是：set -o pipefail; grep zzznomatch strict/p.txt | cat; echo "只穿 pipefail：还在"
    只穿 pipefail：还在
  rc=0
  敲的是：set -e; set -o pipefail; grep zzznomatch strict/p.txt | cat; echo "两件都穿：这行印不出来"
  rc=1
  → 三行码是 0 / 0 / 1：-e 认的是「这条命令的码」，不穿 pipefail 时管道交出的永远是最后一棒的 0，于是 -e
    什么都没做错却放行了；只穿 pipefail 则码变 1 但没人终止脚本，线索照样躺在 $? 里。
  ★ 这两件得一起穿（写全就是 set -eo pipefail）：少一件就是漏的，四件套不是随便挑两件。
```

三行码 0 / 0 / 1：`-e` 单穿放行，不是它做错什么，而是**它拿到的码本来就是 0**；
pipefail 单穿改了码却没人终止，线索躺在 `$?` 里等你自己去读。
所以 `set -eo pipefail` 得写成一对，「四件套」不是随便挑两件。

```
=== 9. 衣服四 -E（errtrace）：只管一件事 —— ERR trap 进不进函数 ===
  敲的是：trap "echo 〔ERR〕响了" ERR; false; echo "还在"
    〔ERR〕响了
    还在
  rc=0
  敲的是：trap "echo 〔ERR〕响了" ERR; h(){ false; echo "体内"; }; h; echo "还在"
    体内
    还在
  rc=0
  敲的是：set -e; trap "echo 〔ERR〕响了" ERR; h(){ false; }; h; echo "还在"
  rc=1
  敲的是：set -e; set -E; trap "echo 〔ERR〕响了" ERR; h(){ false; }; h; echo "还在"
    〔ERR〕响了
  rc=1
  → 第 1、2 格：顶层 false 让 trap 响一次，同一个 false 搬进函数就一声不响 —— ERR trap 默认不进函数体。
  → 第 3、4 格只差一件 -E：不穿时一声不响、穿了响一次，而两格 rc 都是 1。★ 这件衣服既不报错也不终止，
    它只决定 trap 在函数里还活着不活着；想让函数里的失败留痕就得穿上它（细节归 trapsafe.sh）。
```

`-E` 是四件里最容易被误传的一件：它既不报错也不终止，只决定 ERR trap 在函数体里活着不活着。
第 3、4 格只差一件 `-E`，两格 rc 都是 1，区别只在屏幕上那一声〔ERR〕响没响。

```
=== 10. 三件套 set -euo pipefail 穿满，仍然挡不住这几类 ===
  敲的是：set -euo pipefail; f(){ false; }; if f; then echo "T"; else echo "F"; fi; echo "还在"
    F
    还在
  rc=0
  敲的是：set -euo pipefail; h(){ false; echo "体内第二行"; }; if h; then echo "T"; else echo "F"; fi; echo "还在"
    体内第二行
    T
    还在
  rc=0
  敲的是：set -euo pipefail; false || true; echo "还在"
    还在
  rc=0
  敲的是：set -euo pipefail; v=$(grep zzznomatch strict/p.txt) || echo "赋值失败被 || 接住了"; echo "还在"
    赋值失败被 || 接住了
    还在
  rc=0
  敲的是：set -euo pipefail; a=(); echo "空数组长度=${#a[@]}，还在"
    空数组长度=0，还在
  rc=0
  → 五条全 rc=0：条件位（if / ||）穿多少件衣服都是豁免区。★ 第 2 条最贵 —— 三件套穿满，函数体里的
    「体内第二行」照样打、照样走 T，和 errescue.sh 第 7 格那个形状一个字都没差。
  → 第 4 条：|| 一接，赋值型命令替换的失败也不致命；第 5 条：-u 管不到空数组展开。衣服是叠加的，不是覆盖的。
```

这一格是第 3 节的收口：**三件套穿满，五条照样全 rc=0**。
第 2 条最贵 —— 函数体里的「体内第二行」照样打、照样走 T，和第 5 节第 7 格同一个形状，一个字都没差。
衣服是**叠加**的，不是覆盖的：穿到第三件不会让第一件的豁免区消失。

```
=== 11. 收账：每条 bash -c 的真实码，和四件衣服的分工 ===
  run 出去的命令与它们的码（按出场顺序）：0 1 127 0 1 127 0 0 0 0 1 127 0 0 0 0 0 0 0 1 0 0 1 1 0 0 0 0 0
  条数=29 ｜ 码 0 的=20 ｜ 码 1 的=6 ｜ 码 127 的=3 ｜ 别的码=0 → 四个加起来等于条数吗：等于
  ★ 127 会不会是个恒零的假守卫？塞一条假码 127 进账本 → 从 3 变 4（响了）；撤掉后回到 3
  → 分工表（只报本机屏幕上真出现过的码）：-e 挡普通位置的失败（→1）；-u 挡没定义就读（→127：标量与下标，
    但空数组整体展开与 "$@" 它不管）；-o pipefail 管非最后一棒的码（→管道码 0 变 1）；-E 只管 ERR trap 进不进
    函数（→多一声，rc 不变）。0 / 1 / 127 三种码都在这块屏幕上出现过，所以「穿几件」=「挡几种」。
[退出码 0]
```

29 条命令、三种码（0 / 1 / 127）都在这块屏幕上真实出现过，所以「穿几件」等于「挡几种」。
最后那行 ★ 是本章每条「恒 0」计数的通用做法：**先塞一条假码进去，看这个数会不会动**。

---

## 4. 小证据：同一条管道，四种开关组合下的「码」与「停不停」

第 3 节第 6 格讲了「码」、第 8 格讲了「停」，这里把两件事并排放在同一条管道上，四格一次看全。

```
$ show(){ v=$(bash -c "$1" 2>&1); printf "    %-16s → %s\n" "$2" "${v:-（那行 echo 没跑到，脚本被掐了）}"; }
show "false | true; echo 码=\$?" 什么都不穿
show "set -e; false | true; echo 码=\$?" 只穿-e
show "set -o pipefail; false | true; echo 码=\$?" 只穿pipefail
show "set -eo pipefail; false | true; echo 码=\$?" 两件都穿
echo "  ★ 码由 pipefail 决定，停不停由 -e 决定；第 2 格与第 4 格的区别正好只有 pipefail。"
    什么都不穿  → 码=0
    只穿-e         → 码=0
    只穿pipefail   → 码=1
    两件都穿     → （那行 echo 没跑到，脚本被掐了）
  ★ 码由 pipefail 决定，停不停由 -e 决定；第 2 格与第 4 格的区别正好只有 pipefail。
[退出码 0]
# 反过来也一样：管道后棒成功、前棒失败，不加 pipefail 时 -e 根本没有非 0 可判 ——
#   于是「穿了 -e 却什么都没挡」不是玄学，是码在这一路上就没变成非 0。

$ bash -c "set -e; ls /no-such-dir-xyz | wc -l; echo 码=\$?" 2>&1 | tail -2
0
码=0
[退出码 0]
```

四行读下来，分工自己就浮出来了：**码由 pipefail 决定，停不停由 -e 决定**。
「只穿 -e」那格交出 0 并走到 echo，「只穿 pipefail」那格交出 1 却也走到 echo，
只有两件都穿才既改码又掐掉 —— 而第 2 格与第 4 格的区别正好只有 pipefail 这一件。
最后那行反过来验证：`ls` 一个不存在的目录，管道后棒 `wc -l` 成功，
不加 pipefail 时 `-e` 拿到的码就是 0，**它根本没有非 0 可判**。
---

## 5. errescue.sh 完整跑：`-e` 的四位豁免

第 3、4 节收的是「管道交出什么码」，这一节收的是「脚本到底停不停」。
`errescue.sh` 十一格，前四格就是把 `&&`、`||`、`if`、`!` 四个位置各撞一遍，
后七格是把「同一个函数换个摆法就换个下场」这件事钉死。跑之前先把结论摆在这：
**`-e` 只管「普通位置上的失败」这一句话，剩下的全是例外。**

先看第 1 格，直觉在这两格里是对的：

```
=== 1. 先给直觉留点面子：普通位置上的失败，set -e 确实会停 ===
  敲的是：false; echo "不开-e：这行照打"
    不开-e：这行照打
  rc=0
  敲的是：set -e; false; echo "开了-e：这行印不出来"
  rc=1
  → 直觉在这两格里是对的：不开 -e 时，失败只在 $? 里留了个 1，没人读它就等于没发生；开了才当场终止。
    但「当场终止」的条件很苛刻 —— 下面 2~5 格那四个位置，-e 一概不管，一个都不停。
```

不穿 `-e` 时那次失败只活在 `$?` 里，没人读就等于没发生；穿了才当场停。
但「当场停」的条件比想象中苛刻得多 —— 下面四个位置它一概不管。

```
=== 2. 豁免①：失败写在 && 的左边（条件位） ===
  敲的是：set -e; f(){ false; echo "体内第二行"; }; f && echo "右边"; echo "tail"
    体内第二行
    右边
    tail
  rc=0
  敲的是：set -e; f(){ false; }; f && echo "右边"; echo "tail"
    tail
  rc=0
  敲的是：set -e; true && false; echo "tail"
  rc=1
  → 三条都带 &&，屏幕上却是三种样子：第 1 条体内第二行照打（函数一进条件位，-e 在**函数体内部**
    整块关灯），且 f 交出的是最后一条 echo 的 0，所以右边也打了；第 2 条右边没打 —— ★ 那是 && 短路，
    不是 -e 停的，因为 tail 还在、rc=0；第 3 条 rc=1：豁免只管条件位那一条，**列表最后一条**失败 -e 照杀。

=== 3. 豁免②：失败写在 || 的左边，右边就是逃生门 ===
  敲的是：set -e; f(){ false; }; f || echo "右边接住了"; echo "还在"
    右边接住了
    还在
  rc=0
  敲的是：set -e; f(){ false; echo "体内第二行"; }; f || echo "右边接住了"; echo "还在"
    体内第二行
    还在
  rc=0
  敲的是：set -e; grep zzznomatch errescue/p.txt || echo "grep 没找到"; echo "还在"
    grep 没找到
    还在
  rc=0
  → 第 1 格：f 交出 1，右边接住、还在、rc=0 —— || 左边整条都算条件位，-e 全程闭嘴。
  → ★ 第 2 格只是往体内多塞一句 echo：体内第二行照打（体内 -e 已关灯），右边**却没接** —— f 交出的是
    最后一条 echo 的 0：函数返回什么，看的从来不是它中间失败过几次，而是它最后干了什么。
  → 第 3 格换成真命令：p.txt 里没有 zzznomatch，grep 码 1，逃生门打印「grep 没找到」，还在、rc=0。
```

这两格（`&&` 与 `||`）里最值得抄走的是三条：
**① 函数一进条件位，`-e` 在函数体内部整块关灯**，所以「体内第二行」照打；
**② `&&` 右边没打不等于脚本停了** —— 第 2 条是短路，`tail` 还在、`rc=0`；
**③ 函数最终交出的是它「最后一条命令」的码，不是它中间失败过几次** ——
第 3 节第 2 格里 `f` 一句 `echo` 都没干所以交出 1、右边接住了，
这里只是往体内多塞一句 `echo`，`f` 就变成 0、右边**没接**，屏幕上少了一行「右边接住了」。
还有一条容易背错：第 3 条 `true && false` 的 `rc=1`。豁免只管**条件位上那一条**，
列表的**最后一条**落在普通位置，`-e` 照杀。

```
=== 4. 豁免③：失败写在 if 的条件位 ===
  敲的是：set -e; f(){ false; }; if f; then echo "T"; else echo "F"; fi; echo "if 之后"
    F
    if 之后
  rc=0
  敲的是：set -e; if grep zzznomatch errescue/p.txt; then echo "T"; else echo "F"; fi; echo "if 之后"
    F
    if 之后
  rc=0
  → 走 F、if 之后还在、rc=0：写在条件位上的失败被当成「答案」，不是「事故」。
  → 第 2 格换成真命令：grep 没匹配上返回 1，-e 一样不管 —— if grep -q 是 -e 脚本里靠得住的探测写法。

=== 5. 豁免④：感叹号把整条列表推进条件位 ===
  敲的是：set -e; ! true; echo "还在，读到的码=$?"
    还在，读到的码=1
  rc=0
  敲的是：set -e; ! false; echo "还在，读到的码=$?"
    还在，读到的码=0
  rc=0
  敲的是：set -e; h(){ false; echo "体内"; }; ! h; echo "还在，读到的码=$?"
    体内
    还在，读到的码=1
  rc=0
  → 三格都「还在」且 rc=0：! 一戴上，整条列表就进了条件位，-e 不管它。★ 第 3 格最阴：体内第一条 false
    失败了第二行照样打，而下一行读 $? 拿到的是 1（h 本身返回 0，被 ! 翻成 1）—— 失败被翻译成了成功取反。
```

`if` 那一格是本章用得最多的一种写法：条件位上的失败被当成「答案」不是「事故」，
所以 `if grep -q …` 在穿满三件套的脚本里靠得住。
`!` 那三格里最阴的是第 3 条：`h` 自己返回 0（最后一条是 `echo`），被 `!` 翻成 1，
于是 `$?` 读到 1 —— **一次真实的失败被翻译成了「成功取反」**，
而 `-e` 全程闭嘴，因为 `!` 把整条列表推进了条件位。

```
=== 6. 「函数最后一条失败」根本不会自动终止一个不带 -e 的脚本 ===
  敲的是：f(){ false; }; f; rc=$?; echo "tail 照打"; echo "刚才 f 交出过的码=$rc"
    tail 照打
    刚才 f 交出过的码=1
  rc=0
  敲的是：set -e; f(){ false; }; f; echo "开了-e：tail 印不出来"
  rc=1
  → 同一个 f，差别只在脚本头上有没有 -e：不穿时脚本跑到尾、整体 rc=0，那次失败只活在 rc 这一行里；
    穿了就死在调用那一行（rc=1）。★ 「函数最后一条失败」bash 从来不替你记账：码进了 $? 就等被覆盖。
```

同一个 `f`，差别只在脚本头上有没有 `-e`：
不穿时脚本跑到尾、整体 `rc=0`，那次失败只活在 `rc=$?` 这一行里；穿了就死在调用那一行。
**bash 从来不替你记「函数最后一条失败」这笔账**，码进了 `$?` 就等着被下一条命令覆盖。

```
=== 7. ★ 全章最贵的一格：同一个函数，裸调用 vs 放进 if ===
  敲的是：set -e; h(){ false; echo "体内第二行"; }; h; echo "调用之后"
  rc=1
  敲的是：set -e; h(){ false; echo "体内第二行"; }; if h; then echo "T"; else echo "F"; fi; echo "if 之后"
    体内第二行
    T
    if 之后
  rc=0
  → 直觉：函数里第一条就失败了，两格都该停在 h 那一行、都该走 F。
  → 屏幕：裸调用那一格确实当场死（体内第二行没打、调用之后没打、rc=1）；放进 if 的这一格，
    体内第二行**照打**、走的是 **T**、if 之后还在跑、**rc=0**。
  ★ 条件位把函数体内的 errexit 整块关掉，函数返回的又是最后一条命令（echo，码 0）的状态，
    于是「体内明明失败了」被判成成功 —— 写 if 判成败的人，和写 set -e 图省心的人，
    会在同一行代码上拿到两个相反的答案。这一格是全章最贵的。
```

这一格是全章最贵的一格，值得单独背下来：
裸调用时死在 `h` 那一行（体内第二行没打、`调用之后` 没打、`rc=1`）；
放进 `if` 之后，**体内第二行照打、走的是 `T`、`if 之后` 还在跑、`rc=0`**。
两个条件叠在一起才出这个结果：条件位把函数体内的 `errexit` 整块关掉，
而函数返回的又是最后一条命令（`echo`，码 0）。
写 `if` 判成败的人，和写 `set -e` 图省心的人，会在同一行代码上拿到两个相反的答案。
第 3 节第 10 格那个「三件套穿满、五条照样全 `rc=0`」的第 2 条，跟这格是同一个形状，一个字都没差。

```
=== 8. 被 -e 掐死时，脚本交出的码不一定是你 return 的那个 ===
  敲的是：h(){ false; return 7; }; h; rc=$?; echo "tail 照打，h 交出过的码=$rc"
    tail 照打，h 交出过的码=7
  rc=0
  敲的是：set -e; h(){ false; return 7; }; h; echo "tail 印不出来"
  rc=1
  → 不穿 -e：h 老实交出 7（上面被 rc 接住了），脚本继续，整体 rc=0。穿 -e：屏幕上没有 tail，
    整体 rc=1 —— ★ 不是 7！被 errexit 终止时你 return 的数字根本没机会成为脚本的码，
    想把 7 留住只能靠 EXIT trap 去读（本章 trapsafe.sh 那一格）。

=== 9. 循环的两个位置：条件位豁免，循环体不豁免 ===
  敲的是：set -e; while false; do echo "体内"; done; echo "还在"
    还在
  rc=0
  敲的是：set -e; for i in 1 2; do false; echo "体内"; done; echo "还在"
  rc=1
  → while 的条件一进来就是假：体内一次都没打、还在、rc=0（条件位天然豁免，第 05 章那条「until 一轮
    都不跑」的同族）；for 的循环体是**普通位置**，第一次迭代就当场死，rc=1。
```

第 8 格要说清一件事：**被 `errexit` 终止时你 `return` 的数字根本没机会成为脚本的码**。
想对外交付 7，只能靠 `EXIT` 陷阱去读那个 `$?`（第 7 节第 5 格讲怎么读才读得到）。
第 9 格把循环的两个位置分开：`while` 的条件天然在条件位（一次都不跑、`rc=0`），
`for` 的循环体是普通位置（第一次迭代就当场死、`rc=1`）。

```
=== 10. 命令替换：写在赋值位会杀脚本，写在取值位不会 ===
  敲的是：set -e; v=$(grep zzznomatch errescue/p.txt); echo "活着 v=[$v]"
  rc=1
  敲的是：set -e; v=$(grep gamma errescue/p.txt); echo "活着 v=[$v]"
    活着 v=[gamma]
  rc=0
  敲的是：set -e; v=$(false; echo hi); echo "活着 v=[$v]"
    活着 v=[hi]
  rc=0
  敲的是：set -e; echo "结果=$(grep zzznomatch errescue/p.txt)"; echo "还在"
    结果=
    还在
  rc=0
  → 第 1 格 rc=1：赋值型命令替换（v=$(...)）的失败**会**终止脚本；第 2 格换成命中的 gamma 就活着，
    v=[gamma] 是 grep 真打出来的那一行。第 3 格说明 -e 只看替换串里**最后一条**命令：false 后面补一个
    echo 就变成 v=[hi]、rc=0（失败被最后一棒盖掉）；第 4 格没在赋值、只是嵌在字符串里，失败整个被咽掉。
```

命令替换这一格有四种下场，规则只有一条：**`-e` 只看替换串里最后一条命令**。
写在赋值位（`v=$(…)`）时失败会杀脚本；串里补一个成功的 `echo` 就变成 `v=[hi]`、`rc=0`；
而没在赋值、只是嵌在字符串里时，失败整个被咽掉，屏幕上只剩一个空 `结果=`。
第 2 格那个 `v=[gamma]` 是 `grep` 真从文件里打出来的那一行，不是人写的。

```
=== 11. 收账：上面每条 bash -c 的真实码都在台账里 ===
  run 出去的命令与它们的码（按出场顺序）：0 1 0 0 1 0 0 0 0 0 0 0 0 0 1 1 0 0 1 0 1 1 0 0 0
  条数=25 ｜ 码 0 的=18 ｜ 非 0 的=7 → 两个加起来等于条数吗：等于
  ★ 非 0 那个数会不会是恒零的假守卫？当场弄响一次：
    塞进一条假码 1 → 非 0 的从 7 变成 8（响了）；撤掉探针之后回到 7
  → 三条能带走的话：① -e 只管「普通位置」上的失败，&& || if ! 四个位置它一概不管；
    ② 函数一旦进了条件位，函数体内部的 -e 也一起关灯，它交出的是最后一条命令的码；
    ③ 所以别把 -e 当保险丝：成败要靠显式判码（if / rc=$? / || ），本章另一份 strictfour.sh
       把四件衣服穿满，也照样管不着这四位豁免 —— 两份脚本讲的是同一件事的两面。
  → 沙箱里只剩 p.txt 与台账 rcs.txt（探针 rcs.bak 已被 mv 吃掉），trap 退出时整目录删掉。
[退出码 0]
```

25 条命令、码 0 的 18、非 0 的 7，两个数加起来正好等于条数。
那行 ★ 是本章每条「恒 0」计数的通用做法：**塞一条假码 1 进台账，看这个 7 会不会变成 8** —— 它会。

---

## 6. 小证据：同一个 `f(){ false; }`，五种摆法五种下场

第 5 节的十一格太长，这一节把它压成五格并排，用的始终是同一个函数体 `f(){ false; }`。

```
$ f(){ false; }
f
printf "  不带 -e：函数里最后一条失败，外面码=%s，脚本照跑（这行印出来了）\n" "$?"
  不带 -e：函数里最后一条失败，外面码=1，脚本照跑（这行印出来了）
[退出码 0]

$ set -e; f(){ false; }; f; echo "  这行跑不到：裸调用 + -e → 当场结束"
[退出码 1]

$ set -e; f(){ false; }; if f; then :; else echo "  同一个 f 放进 if 条件位：-e 让路，脚本还活着"; fi
  同一个 f 放进 if 条件位：-e 让路，脚本还活着
[退出码 0]

$ set -e; f(){ false; }; g(){ f; }; g; echo "  到不了：失败藏在被调用者的函数体里，-e 照样管"
[退出码 1]

$ set -e; f(){ false; }; g(){ f; }; if g; then :; else echo "  把最外层 g 放进 if：整条调用链一起进条件位，-e 一路让路（这行印出来了）"; fi
  把最外层 g 放进 if：整条调用链一起进条件位，-e 一路让路（这行印出来了）
[退出码 0]
# ★ 第 4 与第 5 条是本章最常被误伤的形状：你的库函数在裸调用时能炸掉调用者，
#   在 if 里就只是「告诉调用者一声」。第 15 节那个骨架里所有失败都写成 if 判断，不靠 -e 兜。
```

读的顺序是：
**① 不穿 `-e`**，外面码=1、脚本照跑；
**② 穿了 `-e` 裸调用**，当场结束，`rc=1`；
**③ 放进 `if`**，`-e` 让路，那行 else 里的话印出来了；
**④ 失败藏在被调用者的函数体里**（`g(){ f; }`，然后裸调 `g`），`-e` 照样管、`rc=1`；
**⑤ 把最外层 `g` 放进 `if`**，整条调用链一起进条件位，`-e` 一路让路。
第 ④ 与第 ⑤ 条是真实项目里最常被误伤的形状：
你的库函数在裸调用时能炸掉调用者，放进 `if` 就只是「告诉调用者一声」。
所以第 15 节那个骨架里**所有失败都写成 `if` 判断，不靠 `-e` 兜**。

---

## 7. trapsafe.sh 完整跑：陷阱的三个时间差

「挂了 `trap` 就会收尾」这句话有三个时间差会把它拆穿：
**定义陷阱那一刻**（引号会不会把 `$?` 提前展开）、**触发那一刻**（陷阱体第一条语句会不会把码改掉）、
**清理跑到一半被第二个信号打断那一刻**（留下的是半截现场，而且没有任何证据）。
十一格按这三段排。

```
=== 1. 陷阱体里的 $? 什么时候被读：双引号当场展开，转义了才留到执行时 ===
  敲的是：trap "echo 〔EXIT〕码=$?" EXIT; exit 3
    〔EXIT〕码=0
  rc=3
  敲的是：trap "echo 〔EXIT〕码=\$?" EXIT; exit 3
    〔EXIT〕码=3
  rc=3
  敲的是：trap "echo 〔EXIT〕码=\$?" EXIT; false; echo tail
    tail
    〔EXIT〕码=0
  rc=0
  → 直觉以为「第一格会打印码=3」：屏幕上却是**码=0 而 rc=3**。因为写陷阱那一刻双引号里的
    $? 就已经被展开成一个具体数字（陷阱体里根本没有 $? 这东西）；把 \$? 留到执行时才有第 2 格。
  ★ 就算留到执行时（第 3 格），正常收尾读到的还是 0：中间那次 false 的码早被 echo 抹平了。
=== 2. ERR 陷阱在顶层响一声，响完照跑；停不停是 set -e 的事 ===
  敲的是：trap "echo 〔ERR〕" ERR; false; echo tail
    〔ERR〕
    tail
  rc=0
  敲的是：set -e; trap "echo 〔ERR〕" ERR; false; echo tail
    〔ERR〕
  rc=1
  → 直觉以为「挂了 ERR 陷阱就等于出错就停」：第一格响了〔ERR〕，tail 照打、rc=0。
  ★ ERR 只负责响，-e 只负责停；少挂 ERR 就没声音，少挂 -e 就不会停，两件事互不顶替。
```

第 1 格的直觉本该是「打印码=3」，屏幕上却是**码=0 而 `rc=3`**：
写 `trap "echo … $?"` 那一行，双引号里的 `$?` 就已经被展开成一个具体数字了，
陷阱体里根本没有 `$?` 这个东西 —— 想留到执行时得写 `\$?`。
第 3 格更阴：`false` 明明失败了，可 `$?` 走到陷阱时已经被后面那句 `echo tail` 抹平成 0。
第 2 格是「ERR 只负责响，`-e` 只负责停」：**少挂 ERR 就没声音，少挂 `-e` 就不会停**，
两件事互不顶替 —— 这条与第 3 节第 9、10 格是同一件事的两面。

```
=== 3. 四格并排：-E 有没有 × -e 有没有，函数里的失败到底响不响 ===
  敲的是：trap "echo 〔ERR〕" ERR; h(){ false; echo 体内; }; h
    体内
  rc=0
  敲的是：set -E; trap "echo 〔ERR〕" ERR; h(){ false; echo 体内; }; h
    〔ERR〕
    体内
  rc=0
  敲的是：set -e; trap "echo 〔ERR〕" ERR; h(){ false; }; h
  rc=1
  敲的是：set -e; set -E; trap "echo 〔ERR〕" ERR; h(){ false; }; h
    〔ERR〕
  rc=1
  → 直觉以为「陷阱挂了就会响」：不带 -E 的第 1、3 格**一声不响** —— ERR 陷阱不继承进函数。
  ★ 第 3 格最阴：脚本因为 -e 死掉了（rc=1），但你连一声响都没听到，日志里只剩一条不明所以的退出。
    加 -E 才响；第 2 格又说明 -E 并不管停不停（体内照打、rc=0）。两件衣服各挡各的事故。
=== 4. ERR 陷阱跟 -e 共享那四位豁免：&& 、|| 、if 里的失败不响 ===
  敲的是：trap "echo 〔ERR〕" ERR; false && echo y; echo tail
    tail
  rc=0
  敲的是：trap "echo 〔ERR〕" ERR; false || echo r; echo tail
    r
    tail
  rc=0
  敲的是：trap "echo 〔ERR〕" ERR; if false; then echo T; else echo F; fi
    F
  rc=0
  → 直觉以为「false 失败了就一定被陷阱抓到」：三格〔ERR〕一次都没响（tail/r/F 都打了）。
  ★ 条件上下文里的失败是「拿来判断的」不是「要报的」—— 别指望 ERR 陷阱替你兜底记一笔。
```

四格并排把 `-E` 与 `-e` 的分工切开：**不带 `-E` 时第 1、3 格一声不响**，ERR 陷阱不继承进函数体；
第 3 格是全节最危险的一行 —— 脚本因为 `-e` 死掉了（`rc=1`），但你连一声响都没听到，
日志里只剩一条不明所以的退出。
加 `-E` 才响，而第 2 格又说明 `-E` 完全不管停不停（体内照打、`rc=0`）。
第 4 格顺带把第 5 节那四位豁免搬过来：**条件位里的失败是「拿来判断的」，不是「要报的」**，
`&&`、`||`、`if` 三格里的 `false` 一次都没让 ERR 陷阱响。

```
=== 5. ★ 陷阱体里第一条赋值就把 $? 吞了（全章最贵的一格）===
  敲的是：trap "x=1; echo \"码=\$? x=\$x\"" ERR; false
    码=0 x=1
  rc=1
  敲的是：trap "__rc=\$?; echo \"码=\$__rc\"" ERR; false
    码=1
  rc=1
  → 直觉以为「陷阱体里读 $? 当然是失败的码」：第一格打的是 **码=0**（同时 x=1）——
    它前面那句 x=1 自己成功了，$? 当场被改写成 0，等 echo 去读已经晚了。
  ★ 修法只有一个动作：把 __rc=$? 写成陷阱体的**第一条语句**（第 2 格码=1）。顺序就是语义。
=== 6. trap - ERR 撤掉之后，同样的失败再也听不见 ===
  敲的是：trap "echo 〔ERR〕" ERR; false; trap - ERR; false; echo tail
    〔ERR〕
    tail
  rc=0
  → 两次 false 只响一声：第二声之前 trap - ERR 把陷阱摘了，tail 照打、rc=0。
  → 这是「这一段我不想让它响」的正解（临时静音），但摘除是全局的，容易忘了装回去。
```

这两格合起来讲「语义就是顺序」：
陷阱体里第一句写 `x=1`，它自己成功了，`$?` 当场被改写成 0，等 `echo` 去读已经晚了（打的是码=0）；
改成第一句 `__rc=$?` 才拿到码=1。
第 6 格是「这一段我不想让它响」的正解（`trap - ERR` 临时静音），
但它摘除的是全局的，容易忘了装回去 —— 真实脚本里更稳的写法是把静音区间包进一个函数。

```
=== 7. 被信号杀死的时候 EXIT 跑不跑：TERM / INT / KILL 三种死法 ===
  敲的是：trap "echo 〔EXIT〕" EXIT; kill -TERM $$; echo tail
    〔EXIT〕
  rc=143
  敲的是：trap "echo 〔EXIT〕" EXIT; kill -INT $$; echo tail
    〔EXIT〕
  rc=130
  敲的是：trap "echo 〔EXIT〕" EXIT; kill -9 $$; echo tail
  rc=137
  → 直觉以为「挂了 EXIT 就一定收得到尾」：TERM(143)、INT(130) 时 EXIT 还跑，但 tail 没跑（主线被拦腰截）；
    -9 那一格屏幕上**一个字都没有**。143=128+15、130=128+2：rc 大于 128 就是被信号干掉的。
  ★ SIGKILL 拦不住，所以 trap 一律作废 —— 清理不能只靠 trap EXIT，临时目录要能容忍「上一趟没扫干净」。
=== 8. 一旦给 TERM 挂了陷阱，这个信号就杀不动你了 ===
  敲的是：trap "echo 〔TERM〕" TERM; trap "echo 〔EXIT〕" EXIT; kill -TERM $$; echo tail
    〔TERM〕
    tail
    〔EXIT〕
  rc=0
  → 第 7 格 TERM 是拦腰截，这格〔TERM〕响完 tail 照打、EXIT 收尾、rc=0：陷阱顶掉了默认动作。
  ★ 看着安全其实更危险：管理器发 TERM 它不走，只能升级到 SIGKILL，然后就轮到第 7 格的「一个字都没有」。
    挂了 TERM 陷阱的脚本必须以 exit 收场（自己 exit 时 EXIT 陷阱照样收到那个码）。
```

第 7、8 两格必须一起读才看得出代价：
`TERM`(143)、`INT`(130) 时 `EXIT` 陷阱还跑，但主线上的 `tail` 没跑（被拦腰截），
`143=128+15`、`130=128+2` —— **`rc` 大于 128 就是被信号干掉的**；
`-9` 那一格屏幕上**一个字都没有**（`rc=137`），`SIGKILL` 拦不住，`trap` 一律作废。
第 8 格是很多人以为的「更安全」：一旦给 `TERM` 挂了陷阱，默认动作被顶掉，
〔TERM〕响完 `tail` 照打、`EXIT` 收尾、`rc=0` ——
看着安全其实更危险：**管理器发 TERM 它不走，只能升级到 SIGKILL，然后就轮到第 7 格那种「一个字都没有」**。
所以挂了 TERM 陷阱的脚本必须以 `exit` 收场（自己 `exit` 时 `EXIT` 陷阱照样收到那个码）。

```
=== 9. ★ 清理跑到一半被第二个信号打断：残留几个文件，当场数给你看 ===
  敲的是：cd trapsafe/work; trap "echo 清①删A; rm -f A; kill -TERM $BASHPID; echo 清②删B; rm -f B" EXIT; echo 主体
    主体
    清①删A
  rc=143
  被打断这趟之后 trapsafe/work/ 还剩 1 个：B
  → 屏幕上〔清①删A〕打了（陷阱确实进去了），〔清②删B〕没打 —— 半截清理，文件 B 还在原地。
  敲的是：cd trapsafe/work; trap "echo 清①一次删两个; rm -f A B" EXIT; echo 主体
    主体
    清①一次删两个
  rc=0
  改成一条 rm -f A B（一次外部命令，中间没有可插进去的间隙）之后还剩 0 个
  ★ 计数守卫先证明它会响：被打断那趟 = 1（不是恒零），收尾那趟 = 0。半截清理比没清理更难查，
    因为「trap 跑过了」在屏幕上有证据，而没跑完的那半截没有任何证据。
```

这一格是全章唯一在磁盘上留下真实半截现场的取证：
陷阱里删完 `A`、`echo` 完第二句之前，被第二个信号打断 ——
屏幕上〔清①删A〕打了（陷阱确实进去过），〔清②删B〕没打，`trapsafe/work/` 里**剩 1 个文件 `B`**。
改成一条 `rm -f A B`（一次外部命令，中间没有可插进去的间隙）之后剩 0 个。
那行 ★ 别跳过：**计数守卫先证明自己会响**（被打断那趟=1，不是恒零），
再说「半截清理比没清理更难查」—— 因为「trap 跑过了」在屏幕上有证据，
而没跑完的那半截**没有任何证据**。

```
=== 10. dash 认不认这一套（两种报错原文都照打）===
  敲的是（dash）：trap "echo 〔EXIT〕码=\$?" EXIT; exit 3
    〔EXIT〕码=3
  rc=3
  敲的是（dash）：trap "echo 〔ERR〕" ERR; false; echo tail
    trap: ERR: bad trap
    tail
  rc=0
  敲的是（dash）：set -o pipefail; false | true; echo 管道码=$?
    管道码=1
  rc=0
  敲的是（dash）：arr=(1 2); echo 到不了这里
    dash: 1: Syntax error: "(" unexpected
  rc=2
  敲的是（dash）：a=1; echo ${a[@]}
    dash: 1: Bad substitution
  rc=2
  → EXIT 陷阱 dash 认（码=3、rc=3）；ERR 陷阱 dash 报「trap: ERR: bad trap」，但 tail 照打、rc=0。
  ★ pipefail 在 dash 0.5.12 上**是认的**（管道码=1；不开时同一格是 0，见 strictfour.sh）；
    数组则是两样报法：声明 arr=(1 2) 当场「Syntax error: "(" unexpected」，光读 ${a[@]} 报
    「Bad substitution」（本格 rc=2）。「/bin/sh 就是阉割版 bash」这句老话两头都不能一概而论。
=== 11. 收账：上面每一条的真实退出码 ===
  按出场顺序的码：3 3 0 0 1 0 0 1 1 0 0 0 1 1 0 143 130 137 0 143 0 3 0 0 2 2
  条数=26 ｜ 码 0 的=12 ｜ 非 0 的=14 ｜ 其中 >128（被信号杀）的=4
  两个加起来等于条数吗：等于
  ★ 「>128」这个数会不会是恒零的假守卫？塞一条 143 进台账再撤掉：
    塞进去 → 从 4 变成 5（响了）；撤掉探针回到 4
  → 三条能带走的话：① 陷阱响不响看上下文（条件位不响、不带 -E 不进函数、-9 一律不响）；
    ② 陷阱体第一条就得把 $? 存下来，否则它被自己的赋值吃掉 —— 写陷阱时还得留神 $? 有没有被提前展开；
    ③ 清理要么一条命令做完，要么挂了 TERM 就必须 exit 收场：半截清理比没清理更难查。
[退出码 0]
```

第 10 格提前把第 16 节要用的 dash 结论摆在这：`EXIT` 陷阱它认（码=3、`rc=3`）、
ERR 陷阱它报 `trap: ERR: bad trap` 却照样往下跑（`rc=0`）、
`set -o pipefail` 它**不但收下还真实现**（同一格不开时是 0，这里交出 1）、
数组则是两样报法：`arr=(1 2)` 当场 `Syntax error: "(" unexpected`，光读 `${a[@]}` 报 `Bad substitution`（都 `rc=2`）。
**「`/bin/sh` 就是阉割版 bash」这句老话两头都不能一概而论。**
最后 26 条码的台账里，非 0 的 14、其中 `>128`（被信号杀）的 4；
那行 ★ 同样先把一条 143 塞进台账让 4 变成 5，再撤掉回到 4。

---

## 8. 小证据：同一个 `exit 9`，三种陷阱体，三种读法

第 7 节第 5 格给了结论，这里把「为什么」摊开成三行同一条命令。
`$?` 是**最后一次命令**的状态，而陷阱体里的第一条语句通常就是个赋值 —— 赋值自己成功了，码立刻变 0。

```
$ trap "echo \"  陷阱体第一条就是 echo（没人碰过退出码）：码=[\$?]\"" EXIT; exit 9
  陷阱体第一条就是 echo（没人碰过退出码）：码=[9]
[退出码 9]

$ trap "x=1; echo \"  陷阱体先写一句 x=1：码=[\$?]\"" EXIT; exit 9
  陷阱体先写一句 x=1：码=[0]
[退出码 9]

$ trap "__rc=\$?; x=1; echo \"  第一句把码抢进 __rc：[__rc=\$__rc] ｜ 但 echo 此刻再读退出码，已经是 [\$?]（被中间那句赋值顶掉了）\"" EXIT; exit 9
  第一句把码抢进 __rc：[__rc=9] ｜ 但 echo 此刻再读退出码，已经是 [0]（被中间那句赋值顶掉了）
[退出码 9]
# ★ 修法只有一条：陷阱体的第一句先 __rc=$?。第 15 节的骨架照这个写法钉死。
#   注意第 1 条里那几个反斜杠：双引号定义陷阱时，\$? 才会留到执行那一刻（第 7 节第 1 格）。
```

第 1 行没人碰退出码，读到 `[9]`；第 2 行前面多了一句 `x=1`，读到 `[0]`；
第 3 行是正解：`__rc=$?` 打头，`[__rc=9]` 保住了，
但同一行里再写 `$?` 读到的还是 `[0]` —— 中间那句 `x=1` 依然把码顶掉了。
**修法只有一个动作：清理函数第一句永远是 `__rc=$?`**，第 15 节的骨架照这个写法钉死。
第 1 行里那几个反斜杠也不是排版：双引号定义陷阱时，写 `\$?` 才会留到执行那一刻。
---

## 9. argscheck.sh 完整跑：进来的参数先过三道闸

这一节的链子是：**用户敲的那行命令 → `$#`/`$1` → `getopts` 的 case → `shift` → 交给外部命令**。
每一步都可能悄悄改写上一一步的结论，九格就是这九次改写。
先记住一句总纲：**闸门是 `$#`，不是 `getopts`**。

```
=== 1. 最外层那道闸是 $#：不是 getopts。顺手看 -u 下裸 $1 的下场 ===
  敲的是：if [ "$#" -lt 2 ]; then echo "用法：要 2 个参数（<目录> <模式>），你这趟给了 $# 个"; exit 2; fi; echo 收工
  参数是：[-a]
    用法：要 2 个参数（<目录> <模式>），你这趟给了 1 个
  rc=2
  敲的是：set -u; echo "第一个=$1"
  参数是：[]
    _: line 1: $1: unbound variable
  rc=127
  敲的是：set -u; echo "第一个=${1-<没给>}"
  参数是：[]
    第一个=<没给>
  rc=0
  → 直觉以为「有 getopts 就不用查参数个数」：第一格 $# 是 1（-a 也算一个），闸门必须先过。
  ★ 第二格是 bash 的原话（本机终端里是「未绑定的变量」，钉成 C 之后是 unbound variable）+ rc=**127**：
    脚本连「你没给参数」这句都来不及说就死了。
    读位置参数一律 ${1-} 或先查 $#。报错前缀打成 `_:` 是 try 把 $0 喂成了下划线 —— 屏幕上不出现真实路径。
```

第 1 格里 `-a` 也算一个参数，所以 `$#` 是 1 而不是 0 —— 用法检查必须先看个数，
`getopts` 只负责分选项，它不会因为「少了一个位置参数」而报错。
第 2 格是穿了三件套的新脚本最常见的死法：`$1` 在 `-u` 下当场死掉，
屏幕上那句 `_: line 1: $1: unbound variable`（本机终端里是「未绑定的变量」）之后，
**连「你没给参数」这句人话都来不及说**，而且码是 **127** 不是 1。
那个 `_:` 前缀是取证脚本把 `$0` 喂成下划线换来的（屏幕上不出现真实路径）。
第 3 格的正解只有一个字符：`${1-<没给>}`。

```
=== 2. getopts 最小骨架：-a 带值、-b 开关，剩下的操作数靠 shift $((OPTIND-1)) ===
  敲的是：while getopts "a:b" o; do case $o in a) echo "取到 a=[${OPTARG-}]";; b) echo "取到 b";; \?) echo "取到 ? OPTARG=[${OPTARG-未设}]";; :) echo "取到 : OPTARG=[${OPTARG-未设}]";; esac; done; shift $((OPTIND-1)); echo "OPTIND=$OPTIND 剩下[$*]"
  参数是：[-b][-a][foo][extra1][extra2]
    取到 b
    取到 a=[foo]
    OPTIND=4 剩下[extra1 extra2]
  rc=0
  敲的是：while getopts "a:b" o; do case $o in a) echo "取到 a=[${OPTARG-}]";; b) echo "取到 b";; \?) echo "取到 ? OPTARG=[${OPTARG-未设}]";; :) echo "取到 : OPTARG=[${OPTARG-未设}]";; esac; done; shift $((OPTIND-1)); echo "OPTIND=$OPTIND 剩下[$*]"
  参数是：[-a][foo]
    取到 a=[foo]
    OPTIND=3 剩下[]
  rc=0
  → OPTIND 是「下一个该读第几个参数」的指针，1 起算（$0 不算），所以 shift 的量是 OPTIND-1。
  → 第二格 -a foo 两个参数都被吃进 a，剩下为空 —— 带值选项一次消耗两个位置，这决定了 $# 的算法。
=== 3. 打包：-ab 不是「-a 和 -b」，是「-a 的值等于 b」 ===
  敲的是：while getopts "a:b" o; do case $o in a) echo "取到 a=[${OPTARG-}]";; b) echo "取到 b";; \?) echo "取到 ? OPTARG=[${OPTARG-未设}]";; :) echo "取到 : OPTARG=[${OPTARG-未设}]";; esac; done; shift $((OPTIND-1)); echo "OPTIND=$OPTIND 剩下[$*]"
  参数是：[-ab]
    取到 a=[b]
    OPTIND=2 剩下[]
  rc=0
  敲的是：while getopts "a:b" o; do case $o in a) echo "取到 a=[${OPTARG-}]";; b) echo "取到 b";; \?) echo "取到 ? OPTARG=[${OPTARG-未设}]";; :) echo "取到 : OPTARG=[${OPTARG-未设}]";; esac; done; shift $((OPTIND-1)); echo "OPTIND=$OPTIND 剩下[$*]"
  参数是：[-b][-a][b]
    取到 b
    取到 a=[b]
    OPTIND=4 剩下[]
  rc=0
  → 直觉以为「-ab 等价于 -a -b」：屏幕上第一格只有一句 a=[b]，b 这个开关**根本没出现**。
  ★ optstring 里 a 后面有冒号（要吃值），打包时它就把尾巴整段当值；想同时要 a 和 b，得写成两格
    或 -b -a b（第二格才有「取到 b」）。这是「少了一个开关还不报错」的现场。
```

这两格讲的是同一个坑的两面：**带值选项一次吃掉两个位置**。
`-a foo` 跑完 `OPTIND=3`、剩下为空，所以 `shift` 的量是 `OPTIND-1`（`OPTIND` 从 1 起算、`$0` 不算）。
第 3 格更值得贴墙上：`-ab` 在 `optstring="a:b"` 下**不是「`-a` 和 `-b`」，而是「`-a` 的值等于 `b`」** ——
屏幕上只有 `a=[b]`，`b` 那个开关根本没出现，而且 **rc=0、一声没出**。
「少了一个开关还不报错」就是这么发生的。

```
=== 4. 非静默模式：报错是 bash 自己打的，而 OPTARG 在 ? 分支里根本没设 ===
  敲的是：while getopts "a:b" o; do case $o in a) echo "取到 a=[${OPTARG-}]";; b) echo "取到 b";; \?) echo "取到 ? OPTARG=[${OPTARG-未设}]";; :) echo "取到 : OPTARG=[${OPTARG-未设}]";; esac; done; shift $((OPTIND-1)); echo "OPTIND=$OPTIND 剩下[$*]"
  参数是：[-a]
    _: option requires an argument -- a
    取到 ? OPTARG=[未设]
    OPTIND=2 剩下[]
  rc=0
  敲的是：while getopts "a:b" o; do case $o in a) echo "取到 a=[${OPTARG-}]";; b) echo "取到 b";; \?) echo "取到 ? OPTARG=[${OPTARG-未设}]";; :) echo "取到 : OPTARG=[${OPTARG-未设}]";; esac; done; shift $((OPTIND-1)); echo "OPTIND=$OPTIND 剩下[$*]"
  参数是：[-z]
    _: illegal option -- z
    取到 ? OPTARG=[未设]
    OPTIND=2 剩下[]
  rc=0
  敲的是：set -u; while getopts "a:b" o; do [ "$o" = "?" ] && echo "坏选项，OPTARG 是 [$OPTARG]"; done
  参数是：[-z]
    _: illegal option -- z
    _: line 1: OPTARG: unbound variable
  rc=127
  → 两格的报错原文都是 bash 打的，脚本一个字没写：本机终端（LANG=zh_CN.UTF-8、LANGUAGE=zh_CN:zh）里是「选项需要一个参数」
    「非法的选项」；demo 把 LC_ALL 与 LANGUAGE 一起钉成 C 之后，同一格是 option requires an argument / illegal option。
  ★ 同一格里 OPTARG=[未设] —— 本机 bash 5.3 在非静默模式下不给你 OPTARG；第三格接上 -u 之后
    直接 unbound 死掉（rc=127）。想拿那个坏字符只能走静默模式（下一格），别在 ? 分支读 $OPTARG。
  → 顺带一条：-u 单独穿时码是 127，同时穿了 -e 同一句报错就变成 1（对照 strictfour.sh 第 2 格）。
=== 5. 静默模式（optstring 开头写冒号）：两种事故只剩一个字符能分开 ===
  敲的是：while getopts ":a:b" o; do case $o in a) echo "静默:a=[${OPTARG-}]";; b) echo "静默:b";; \?) echo "静默:? OPTARG=[${OPTARG-未设}]";; :) echo "静默:: OPTARG=[${OPTARG-未设}]";; esac; done
  参数是：[-z]
    静默:? OPTARG=[z]
  rc=0
  敲的是：while getopts ":a:b" o; do case $o in a) echo "静默:a=[${OPTARG-}]";; b) echo "静默:b";; \?) echo "静默:? OPTARG=[${OPTARG-未设}]";; :) echo "静默:: OPTARG=[${OPTARG-未设}]";; esac; done
  参数是：[-a]
    静默:: OPTARG=[a]
  rc=0
  敲的是：while getopts "a:b" o; do case $o in a) echo "取到 a=[${OPTARG-}]";; b) echo "取到 b";; \?) echo "取到 ? OPTARG=[${OPTARG-未设}]";; :) echo "取到 : OPTARG=[${OPTARG-未设}]";; esac; done; shift $((OPTIND-1)); echo "OPTIND=$OPTIND 剩下[$*]"
  参数是：[-z]
    _: illegal option -- z
    取到 ? OPTARG=[未设]
    OPTIND=2 剩下[]
  rc=0
  → 静默模式下 bash 的报错原文**一声不出**（对比第三格同一条 -z 是有原话的），全看你 case 写没写
    「? 支」和「: 支」：-z 给 ?（OPTARG=[z]），-a 缺值给 :（OPTARG=[a]）—— 少写一支就是静默吞掉。
  ★ 选了静默模式，usage 就得自己印；屏幕上「什么都没发生」不是没出错，是没人报。
```

这两格配着读，因为它们是同一个选择的两端。
**非静默模式**（`optstring` 开头没冒号）：报错原文由 bash 自己打，脚本一个字没写，
但 `?` 分支里的 `OPTARG` **根本没设** —— 第 4 格第三行 `OPTARG=[未设]`，
再穿上 `-u` 去读它就是 `OPTARG: unbound variable` + **rc=127**（本格第 3 条）。
还有个反直觉：`-a` 缺值这种「看着该落到 `:`」的事故，非静默下**照样落到 `?`**。
**静默模式**（开头多写一个冒号）：bash 一声不出，两种事故只剩一个字符能分开 ——
`-z` 给 `?`（`OPTARG=[z]`）、`-a` 缺值给 `:`（`OPTARG=[a]`），少写一支就是静默吞掉。
所以：**想要那个坏字符就得开静默，开了静默 usage 就得自己印**。
（对照第 4 格第一行和第 5 格第三行：同一条 `-z`，一个有原话、一个只有你自己的 case。）

```
=== 6. 「--」只在「选项位」才结束解析：它可以被带值选项当值吃掉 ===
  敲的是：while getopts "a:b" o; do case $o in a) echo "取到 a=[${OPTARG-}]";; b) echo "取到 b";; \?) echo "取到 ? OPTARG=[${OPTARG-未设}]";; :) echo "取到 : OPTARG=[${OPTARG-未设}]";; esac; done; shift $((OPTIND-1)); echo "OPTIND=$OPTIND 剩下[$*]"
  参数是：[-a][--][-b][文件]
    取到 a=[--]
    取到 b
    OPTIND=4 剩下[文件]
  rc=0
  敲的是：while getopts "a:b" o; do case $o in a) echo "取到 a=[${OPTARG-}]";; b) echo "取到 b";; \?) echo "取到 ? OPTARG=[${OPTARG-未设}]";; :) echo "取到 : OPTARG=[${OPTARG-未设}]";; esac; done; shift $((OPTIND-1)); echo "OPTIND=$OPTIND 剩下[$*]"
  参数是：[--][-b][文件]
    OPTIND=2 剩下[-b 文件]
  rc=0
  → 直觉以为「看到 -- 后面全是操作数」：第一格里 -- 紧跟 -a，被当成 a 的值（a=[--]），后面的 -b
    照样被解析成开关。第二格 -- 落在选项位上才真的刹车，-b 和 文件 一起落到「剩下」。
  ★ 所以「用户的文件名可能以 - 开头」时，调用方要么把 -- 顶到选项位（-a -- x），要么写成 ./x。
=== 7. 第一个非选项之后就停：后面的 -b 永远不会被当成选项 ===
  敲的是：while getopts "a:b" o; do case $o in a) echo "取到 a=[${OPTARG-}]";; b) echo "取到 b";; \?) echo "取到 ? OPTARG=[${OPTARG-未设}]";; :) echo "取到 : OPTARG=[${OPTARG-未设}]";; esac; done; shift $((OPTIND-1)); echo "OPTIND=$OPTIND 剩下[$*]"
  参数是：[x][-b]
    OPTIND=1 剩下[x -b]
  rc=0
  敲的是：while getopts "a:b" o; do case $o in a) echo "取到 a=[${OPTARG-}]";; b) echo "取到 b";; \?) echo "取到 ? OPTARG=[${OPTARG-未设}]";; :) echo "取到 : OPTARG=[${OPTARG-未设}]";; esac; done; shift $((OPTIND-1)); echo "OPTIND=$OPTIND 剩下[$*]"
  参数是：[-b][x][-b]
    取到 b
    OPTIND=2 剩下[x -b]
  rc=0
  → 直觉以为「-b 在后面也会被认出来」：第一格 OPTIND=1、剩下[x -b]，循环一条都没进。
  ★ 第二格更阴：开头那个 -b 被认成开关（OPTIND 走到 2），停在 x 上，剩下的还是 [x -b] ——
    两格「剩下」一模一样，差别只在它前面有没有被解析掉的选项；屏幕上 OPTIND=1 对 2 就是证据。
```

第 6、7 两格把「`--` 到底在哪刹车」钉死。
直觉是「看到 `--`，后面全是操作数」，屏幕不是：`-a -- -b 文件` 里 `--` 紧跟在带值选项后面，
**被当成 `a` 的值**（`a=[--]`），后面的 `-b` 照样被解析成开关。
`--` 只有落在**选项位**上才真的刹车（第二格：`OPTIND=2`、`剩下[-b 文件]`，`-b` 和 `文件` 一起落到操作数）。
第 7 格是另一半：`getopts` 遇到**第一个非选项就停**，后面再写 `-b` 也永远不会被当选项 ——
`x -b` 那格 `OPTIND=1`、循环一条没进；`-b x -b` 那格开头那个 `-b` 被认掉了（`OPTIND=2`），
停在 `x` 上，**两格「剩下」一模一样**，唯一的证据就是屏幕上那个 `OPTIND` 是 1 还是 2。

```
=== 8. 以 - 开头的文件名交给外部命令：坏写法与两种正解 ===
  敲的是：cd argscheck/f; rm -f -dash.txt
  参数是：[]
    rm: invalid option -- 'a'
    Try 'rm ./-dash.txt' to remove the file '-dash.txt'.
    Try 'rm --help' for more information.
  rc=1
  敲的是：cd argscheck/f; rm -f -- -dash.txt
  参数是：[]
    （一声没出）
  rc=0
  敲的是：cd argscheck/f; rm -f ./-dash.txt
  参数是：[]
    （一声没出）
  rc=0
  敲的是：cd argscheck/f; cat -dash.txt
  参数是：[]
    error: unexpected argument '-d' found
    
      tip: to pass '-d' as a value, use '-- -d'
    
    Usage: cat [OPTION]... [FILE]...
    
    For more information, try '--help'.
  rc=1
  敲的是：cd argscheck/f; cat -- -dash.txt
  参数是：[]
    hi
  rc=0
  敲的是：rm --version | head -1; cat --version | head -1
  参数是：[]
    rm (GNU coreutils) 9.7
    cat (uutils coreutils) 0.8.0
  rc=0
  ★ 计数守卫自己会响：坏写法 rm 报 rc=1、文件还剩 1 个（它把 -dash.txt 当成一串选项了）；
    加 -- 之后剩 0 个、加 ./ 之后剩 0 个。-- 是「选项到此为止」，./ 是把名字改成不以 - 开头。
  → 屏幕上 rm 那句原话自己就教你写 ./（中文终端里是「尝试使用」，C.UTF-8 里是 try）；
    本机 rm 是 GNU coreutils，而 cat 是 uutils，
    同一件事 cat 报的是英文 unexpected argument（tip 里也自己写了 use -- -d），两套厂商风格并存。
```

这一格是三道闸的最后一道，也是本章唯一把两家厂商的报错并排放在一起的格：
`rm -f -dash.txt` 被 GNU rm 当成一串选项（`rm: invalid option -- 'a'`，`rc=1`，**文件还剩 1 个**），
而它的原话自己就教你写 `./`（中文终端里那句是「尝试使用」）；
`rm -f -- -dash.txt` 与 `rm -f ./-dash.txt` 两种正解都一声没出、`rc=0`、剩 0 个。
换成 `cat`（本机是 uutils）报的是另一套风格：`error: unexpected argument '-d' found`，
但 tip 里同样自己写了 `use '-- -d'` —— **两家话不一样，结论一样：`--` 或 `./`**。
那行 ★ 是本章通用做法：先把「坏写法还剩 1 个」这个数摆出来，守卫会响，后面的 0 才有意义。

```
=== 9. 收账：上面每一条的真实退出码 ===
  按出场顺序的码：2 127 0 0 0 0 0 0 0 127 0 0 0 0 0 0 0 1 0 0 1 0 0
  条数=23 ｜ 码 0 的=18 ｜ 非 0 的=5 ｜ 其中 2（用法错）的=1
  两个加起来等于条数吗：等于
  ★ 「码 2」会不会是恒零的假守卫？塞一条 2 进台账再撤掉：
    塞进去 → 从 1 变成 2（响了）；撤掉探针回到 1
  → 三条能带走的话：① 闸门是 $# 与「非 0 就 exit 并印 usage」，getopts 只负责分选项；
    ② 非静默模式下 ? 分支里 OPTARG 没设，想要坏字符就开静默，开了静默就得自己印 usage；
    ③ 交给外部命令之前把 -- 或 ./ 用上 —— 屏幕上 rm 那句原话就是它自己想让你做的（中英文同一句）。
[退出码 0]
```

23 条码、码 0 的 18、非 0 的 5、其中用法错（码 2）的 1；
「码 2」那个数先塞一条假码让它从 1 变成 2，再撤掉回到 1。

---

## 10. 小证据：OPTIND 不复位，以及两种 optstring 各怎么答

这一节三格都是同一个最小 `getopts` 骨架，只是 `optstring` 开头那一个冒号有和无。
另外提醒：这里的 `G` **故意不写 `shift`**，所以 `剩下[$*]` 打印的还是原始位置参数 ——
它的作用是让 `OPTIND` 和「第几个参数开始是操作数」在同一天里对照得起来。

```
$ G(){ while getopts "a:b" o; do case $o in
  a) echo "    取到 a=[${OPTARG-}]";;
esac; done; echo "    OPTIND=$OPTIND 剩下[$*]"; }
echo "  同一个进程里连调两次、中间没有 shift（写这一节时真踩到的坑）："
echo "  第一次："; G -a x
echo "  第二次："; G -a x
echo "  ★ 第二次那个 case 一声没出、OPTIND 还是 3 —— getopts 从第 OPTIND 个参数看起，"
echo "    上一次已经把它推到 3 了。函数里用 getopts，函数第一句得写 OPTIND=1。"
  同一个进程里连调两次、中间没有 shift（写这一节时真踩到的坑）：
  第一次：
    取到 a=[x]
    OPTIND=3 剩下[-a x]
  第二次：
    OPTIND=3 剩下[-a x]
  ★ 第二次那个 case 一声没出、OPTIND 还是 3 —— getopts 从第 OPTIND 个参数看起，
    上一次已经把它推到 3 了。函数里用 getopts，函数第一句得写 OPTIND=1。
[退出码 0]
```

第一格是本节写的时候真踩到的坑，也是函数里用 `getopts` 唯一必须的姿势：
**bash 不在同进程的两次 `getopts` 之间复位 `OPTIND`**。
`G -a x` 连调两遍，第二遍那个 `case` 一声没出、`OPTIND` 还是 3、`剩下[-a x]` 一个都没 shift ——
因为 `getopts` 从第 `OPTIND` 个参数看起，而上一次已经把它推到 3 了。
**函数里用 `getopts`，函数第一句得写 `OPTIND=1`。**

```
$ G(){ OPTIND=1; while getopts "a:b" o; do case $o in
  a) echo "    取到 a=[${OPTARG-}]";; b) echo "    取到 b";;
  \?) echo "    落到 ?，OPTARG=[${OPTARG-（根本没设）}]";;
  :) echo "    落到 :，OPTARG=[${OPTARG-（根本没设）}]";;
esac; done; echo "    OPTIND=$OPTIND 剩下[$*]"; }
echo "  非静默（optstring 不带冒号），输入 -z x："; G -z x
echo "  非静默，输入 -a x（-a 要值，这里 -a 拿到 x）："; G -a x
echo "  非静默，输入 -a（缺值）："; G -a
  非静默（optstring 不带冒号），输入 -z x：
bash: illegal option -- z
    落到 ?，OPTARG=[（根本没设）]
    OPTIND=2 剩下[-z x]
  非静默，输入 -a x（-a 要值，这里 -a 拿到 x）：
    取到 a=[x]
    OPTIND=3 剩下[-a x]
  非静默，输入 -a（缺值）：
bash: option requires an argument -- a
    落到 ?，OPTARG=[（根本没设）]
    OPTIND=2 剩下[-a]
[退出码 0]
```

非静默三格：报错原话由 bash 打（这里前缀是 `bash:`，第 9 节里是 `_:`，差别只在 `$0` 被喂成什么），
`?` 分支里 `OPTARG` 一律「根本没设」，
第三行那句 `bash: option requires an argument -- a` 后面落的也是 `?` 而不是 `:`。
所以那句 `${OPTARG-…}` 不是防御性写法，是**必需**的：穿上 `set -u` 少写这一笔就是第 9 节第 1 格的下场。

```
$ G(){ OPTIND=1; while getopts ":a:b" o; do case $o in
  a) echo "    取到 a=[${OPTARG-}]";; b) echo "    取到 b";;
  \?) echo "    落到 ?，OPTARG=[${OPTARG-（根本没设）}]";;
  :) echo "    落到 :，OPTARG=[${OPTARG-（根本没设）}]";;
esac; done; echo "    OPTIND=$OPTIND 剩下[$*]"; }
echo "  静默（optstring 开头写冒号），输入 -z x："; G -z x
echo "  静默，输入 -a x："; G -a x
echo "  静默，输入 -a（缺值）："; G -a
echo "  同一个函数连着调两遍（这一版每调一遍先复位一次）："; G -z x; G -a
  静默（optstring 开头写冒号），输入 -z x：
    落到 ?，OPTARG=[z]
    OPTIND=2 剩下[-z x]
  静默，输入 -a x：
    取到 a=[x]
    OPTIND=3 剩下[-a x]
  静默，输入 -a（缺值）：
    落到 :，OPTARG=[a]
    OPTIND=2 剩下[-a]
  同一个函数连着调两遍（这一版每调一遍先复位一次）：
    落到 ?，OPTARG=[z]
    OPTIND=2 剩下[-z x]
    落到 :，OPTARG=[a]
    OPTIND=2 剩下[-a]
[退出码 0]
# ★ 「（根本没设）」不是脚本写漏了：非静默模式下 bash 自己报完错，? 分支里的 OPTARG 是未定义的，
#   所以那句 ${OPTARG-…} 是必需的（穿了 set -u 时少写这一笔就是第 9 节第 1 格的下场）。
#   顺带看一眼非静默那组的第三行：连「选项缺值」这种看着该落到 : 的事故，也照样落到 ?，
#   因为静默模式（optstring 开头那个冒号）才是 : 分支唯一的入口。
```

静默三格：bash 的报错原文**一声不出**，两种事故靠一个字符分开 ——
`-z` 到 `?` 且 `OPTARG=[z]`，`-a` 缺值到 `:` 且 `OPTARG=[a]`。
第四行是同一函数连调两遍（这一版每调一遍先复位一次），两遍都如实答了，正好回证上面那条 `OPTIND=1`。
最后那两行 ★ 是本节的收口：**`:` 分支唯一的入口就是 `optstring` 开头那个冒号**；
选了静默，屏幕上「什么都没发生」不等于没出错，只是没人报。

---

## 11. filesafe.sh 完整跑：碰文件之前的那些刀

「先测一下再动手」这句话在文件上是**有洞的**，而且洞不报错。
九格里第 4、5 两格是全章最贵的两刀，第 6、7 格是它们的两条正解。

```
=== 1. 四种判断各自跟着谁：四个对象 × 六个判断，逐格真跑 ===
  对象    -e -f -L -s -d -r 
  plain     是 是 否 是 否 是 ← 它是链接吗：不是
  empty     是 是 否 否 否 是 ← 它是链接吗：不是
  to_dir    是 否 是 是 是 是 ← 它是链接吗：是
  dangling  否 否 是 否 否 否 ← 它是链接吗：是
  → 直觉以为「-e 存在、-f 是文件、够用了」：断链那行 -L=是 而 -e=否 —— 名字明明在那儿，-e 却说没有。
  ★ 因为 -e/-f/-s/-d/-r 全都**跟着链接走**，只有 -L 看链接本身；指向目录的链接 -f=否、-d=是。
    所以「我要的是真文件」写 -f，「我要拦符号链接」必须显式 -L，两件事不能互相顶替。
=== 2. mktemp 给的是什么权限（stat 真读；随机名字压根不上屏，只报权限与类型）===
  敲的是：f=$(mktemp); echo "默认无模板 → 权限=$(stat -c "%a %F" "$f")"; rm -f "$f"
    默认无模板 → 权限=600 regular empty file
  rc=0
  敲的是：d=$(mktemp -d); echo "-d → 权限=$(stat -c "%a %F" "$d")"; rmdir "$d"
    -d → 权限=700 directory
  rc=0
  敲的是：f=$(mktemp -t b08fs.XXXXXX); echo "-t 模板 → 权限=$(stat -c "%a" "$f")"; rm -f "$f"
    -t 模板 → 权限=600
  rc=0
  → 直觉以为「临时文件跟普通 > 建出来一样是 644/755」：mktemp 给的是 **600 文件 / 700 目录**。
  ★ 用 > 自己拼 /tmp/xxx 不但可预测（别人的进程能先占住这个名字），权限还是默认的宽权限 ——
    「名字不可猜 + 权限只给自己」这两条是 mktemp 一次买齐的，手写拼路径两条都丢。
```

第 1 格那张 4×6 的矩阵只用读一行：`dangling`（断链）那行 **`-L=是` 而 `-e=否`** ——
名字明明在那儿，`-e` 却说没有。原因是 `-e/-f/-s/-d/-r` **全都跟着链接走**，只有 `-L` 看链接本身；
指向目录的链接则是 `-f=否、-d=是`。
所以「我要真文件」写 `-f`、「我要拦符号链接」必须显式写 `-L`，两件事不能互相顶替。
第 2 格是 `mktemp` 的账：`stat` 真读出来的权限是 **600 文件 / 700 目录**，不是 `>` 建出来的 644/755；
而随机名字压根不上屏（只报权限与类型）。**「名字不可猜 + 权限只给自己」是 `mktemp` 一次买齐的两条**，
手写拼 `/tmp/xxx` 两条都丢。

```
=== 3. 模板里那串 X 的规则：不在末尾也能用，配 --suffix 却不行 ===
  敲的是：f=$(mktemp fs/XXXXXX.txt); echo "建出来了：后缀=$(printf %s "${f##*.}") 整串长度=${#f} X 有没有被换成随机串=$([ "$f" = "fs/XXXXXX.txt" ] && echo 没有 || echo 换了)"
    建出来了：后缀=txt 整串长度=13 X 有没有被换成随机串=换了
  rc=0
  敲的是：mktemp --suffix=.log fs/XXXXXXX.tmp
    mktemp: with --suffix, template 'fs/XXXXXXX.tmp' must end in X
  rc=1
  敲的是：mktemp fs/XX
    mktemp: too few X's in template 'fs/XX'
  rc=1
  → 直觉以为「X 必须在模板末尾」：第一格 fs/XXXXXX.txt **成功**（尾巴 .txt 留着），rc=0。
  ★ 第二格加了 --suffix 反而失败，原话是「模板 must end in X」rc=1（同一台机器上两条规则打架）；
    第三格 X 只有两个，报错也是原话 —— 记不住规则就别猜，让 mktemp 自己说 rc 和原因。
```

模板那串 `X` 有三条规则，屏幕上三条都撞过：`fs/XXXXXX.txt` **成功**（`X` 不在末尾也行，后缀 `txt` 留着），
加 `--suffix` 之后同一个形状反而失败（原话 `with --suffix, template … must end in X`，`rc=1`），
`X` 只写两个也是原话报错（`too few X's`）。
**记不住规则就别猜，让 `mktemp` 自己说 rc 和原因** —— 同一台机器上这两条规则是打架的。

```
=== 4. ★ 先测后开第一刀：断链让 -e 说「不存在」，写下去却凭空造出一个新文件 ===
  敲的是：cd fs; ln -sfn newtarget link1; if [ -e link1 ]; then echo "测到：存在"; else echo "测到：不存在，那就写"; fi; echo 秘密内容 > link1; echo "link1 还是链接吗=$([ -L link1 ] && echo 是 || echo 不是)"; echo "newtarget 出现了吗=$([ -e newtarget ] && echo 出现了 || echo 没有) 内容=$(cat newtarget 2>/dev/null)"
    测到：不存在，那就写
    link1 还是链接吗=是
    newtarget 出现了吗=出现了 内容=秘密内容
  rc=0
  → 直觉以为「测过不存在就能放心写」：-e 对断链回的是「不存在」，而 echo > 链接 把内容写进了
    链接**指向的那个名字** —— 屏幕上 newtarget 从无到有，我们从头到尾没写过它的名字。
  ★ 判断跟着链接走、写入也跟着链接走：测的是链接、落的却是别人替你选好的路径。
=== 5. ★ 先测后开第二刀：测完到开写之间链接被改指（这一步由我们替第三方演）===
  敲的是：cd fs; echo 原来的重要内容 > victimA; ln -sfn victimB swap; if [ -e swap ]; then echo "测到：存在，不写"; else echo "测到：swap 背后的名字不存在，可以放心写"; fi; ln -sfn victimA swap; echo 注入 > swap; echo "victimA 现在=[$(cat victimA)]"; echo "victimB 存在吗=$([ -e victimB ] && echo 存在 || echo 不存在)"
    测到：swap 背后的名字不存在，可以放心写
    victimA 现在=[注入]
    victimB 存在吗=不存在
  rc=0
  → 判定那一刻 swap 指向 victimB（不存在）→ 屏幕上走的正是「可以放心写」这一支；间隙里把 swap 改指
    victimA（里面有内容），于是写入落到**一个已存在、我们从来没测过的文件**上：victimA 被覆盖。
  ★ 这一格不假装抓到了并发：真实事故里换链接的是别人，这里由我们手动插一步 —— 证的不是概率，而是
    「测完再开 = 开的是别人说了算的那个路径」。正解见第 6、7 格。
```

这两格是本章的核心。
第 4 格：`link1` 是一条指向 `newtarget` 的**断链**，`[ -e link1 ]` 回「不存在」，
于是脚本按直觉「那就写」——`echo 秘密内容 > link1` 之后 `link1` 还是链接（`是`），
而**屏幕上 `newtarget 出现了吗=出现了 内容=秘密内容`**：
那个名字我们从头到尾没写过一次，内容是写进链接指向的路径里的。
第 5 格把时间差拉开：判定那一刻 `swap` 指向不存在的 `victimB`（走「可以放心写」这一支），
就在测完与开写之间把 `swap` 改指到有内容的 `victimA` —— 于是 `victimA 现在=[注入]`，
**一个已存在、我们从来没测过的文件被覆盖了**，全程 `rc=0`。
那行 ★ 说得很老实：**这一格不假装抓到了并发**，真实事故里换链接的是别人，这里由我们手动插一步；
它证的不是概率，而是「测完再开 = 开的是别人说了算的那个路径」。

```
=== 6. 正解一：set -C（noclobber）把「存在就报错」挪到打开的那一刻 ===
  敲的是：cd fs; echo 已有 > target0; ( set -C; echo 新的 > target0 ); echo "同名已存在 rc=$?"
    _: line 1: target0: cannot overwrite existing file
    同名已存在 rc=1
  rc=0
  敲的是：cd fs; ( set -C; echo 新的 > brandnew ); echo "新名字 rc=$? 内容=$(cat brandnew)"
    新名字 rc=0 内容=新的
  rc=0
  敲的是：cd fs; ( set -C; echo 强行 >| target0 ); echo ">| 之后 rc=$? 内容=$(cat target0)"
    >| 之后 rc=0 内容=强行
  rc=0
  → 直觉以为「noclobber 是交互 shell 的玩具」：第二格里它就是那条缝的封条 —— 判断不在脚本里做，
    由 open() 当场做（原话「无法覆盖已存在的文件」+ rc=1），中间没人能插一步。
  ★ 但 >| 一强行就破功（第三格 rc=0、内容被换）：封条只挡住「忘了」，挡不住「非要覆盖」。
```

正解一：`set -C`（noclobber）把「存在就报错」这件事**从脚本里挪到 `open()` 那一刻**，
中间没人能插一步（`cannot overwrite existing file`、`rc=1`），新名字则照常 `rc=0`。
但第三格是它的边界：`> |` 一强行就破功（`rc=0`、内容被换成「强行」）——
**封条只挡住「忘了」，挡不住「非要覆盖」**。

```
=== 7. 正解二：flock 的锁跟着「打开的文件描述」走，不跟着 fd 号、也不跟着文件名 ===
  敲的是：cd fs; exec 9>mylock; flock -n 9 && echo "第一次：拿到"; flock -n 9 && echo "第二次：也拿到（同一个 fd 不会自锁）" || echo "第二次：拿不到"
    第一次：拿到
    第二次：也拿到（同一个 fd 不会自锁）
  rc=0
  敲的是：cd fs; exec 9>mylock; flock -n 9; bash -c "exec 8>mylock; flock -n 8; echo \"子进程重新开一个 fd 去拿：rc=\$?\""
    子进程重新开一个 fd 去拿：rc=1
  rc=0
  敲的是：cd fs; exec 9>mylock; flock -n 9; bash -c "flock -n 9; echo \"子进程不重开、用继承来的同一个 fd：rc=\$?\""
    子进程不重开、用继承来的同一个 fd：rc=0
  rc=0
  → 直觉以为「flock 会挡住重复加锁的自己」：第一格同一个 fd 两次都拿到 —— 锁登记在「打开的文件描述」
    上，同一个描述再 flock 一次只是换个状态，不会自锁。
  ★ 第二格另一个进程**重新 open 一个 fd**（8）去拿同一个文件：rc=1 真的拿不到；第三格那个子进程不重开、
    直接用 fork 继承来的同一个 fd 9：rc=0 —— 它天生就带着父进程那把锁。所以「派出去的子任务」连着锁，
    而「另起一个实例重新开文件」才被挡住；只记「flock 防并发」这一句，这两件事一件也想不到。
```

正解二这一格反直觉程度最高：**`flock` 的锁跟着「打开的文件描述」走，不跟着 fd 号、也不跟着文件名**。
第一格同一个 fd 连 `flock -n 9` 两次都拿到（同一个描述不会自锁）；
第二格子进程**重新 `open` 一个 fd（8）**去拿同一个文件：`rc=1`，真的拿不到；
第三格子进程不重开、直接用 `fork` 继承来的同一个 fd 9：`rc=0` —— 它天生就带着父进程那把锁。
所以「派出去的子任务」是连着锁的，而「另起一个实例重新开文件」才被挡住。
只背「`flock` 防并发」这一句，这两件事一件也想不到。

```
=== 8. printf %q：把肉眼看不出来的名字变成能核对的字面 ===
  敲的是：cd fs; n="怪 名.txt"; printf "名字(s格式)=[%s] 名字(q格式)=[%q]\n" "$n" "$n"
    名字(s格式)=[怪 名.txt] 名字(q格式)=[怪\ 名.txt]
  rc=0
  敲的是：cd fs; printf "带引号的名字(q格式)=[%q]\n" "$(printf "带\x27引号\x27.txt")"
    带引号的名字(q格式)=[带\'引号\'.txt]
  rc=0
  敲的是：cd fs; printf "带制表符的名字(q格式)=[%q]\n" "$(printf "tab\t名.txt")"
    带制表符的名字(q格式)=[$'tab\t名.txt']
  rc=0
  敲的是：cd fs; q=$(printf "%q" "怪 名.txt"); eval "back=$q"; echo "粘回命令行之后名字相等吗=$([ "$back" = "怪 名.txt" ] && echo 相等 || echo 不相等) 字符数=$(printf %s "$back" | wc -m)"
    粘回命令行之后名字相等吗=相等 字符数=7
  rc=0
  → 直觉以为「echo 出来就能看出有没有空格」：同一行两种格式并排，s 那个 [怪 名.txt] 看不出边界在哪，
    q 那个在空格前面补了反斜杠 —— 同一个名字的两种说法并排，一比就知道差在哪。
  ★ 带引号的名字 q 给出「反斜杠+引号」，带制表符的名字 q 干脆换成美元加单引号的 ANSI-C 形状；
    最后一格才是 %q 的用途：粘回命令行还能得到同一个名字（相等、字符数一致）—— 但那一步是 eval，
    只对自己生成的字符串用；用户给的东西一律走数组和 "$@"，别拿 eval 接（见 weirdnames.sh）。
```

`%q` 这一格给的是「看得见」这件事：同一个名字并排两种格式，
`%s` 那个 `[怪 名.txt]` 看不出边界在哪，`%q` 那个 `[怪\ 名.txt]` 在空格前补了反斜杠；
带引号的给出「反斜杠+引号」，带制表符的干脆换成 `$'tab\t名.txt'` 这种 ANSI-C 形状。
最后一格才是它的用途：`eval "back=$q"` 之后名字**相等、字符数 7**。
但那一步是 `eval` —— **只对自己生成的字符串用；用户给的东西一律走数组和 `"$@"`，别拿 `eval` 接**（第 13、14 节正面处理）。

```
=== 9. 收账：上面每一条的真实退出码 ===
  按出场顺序的码：0 0 0 0 1 1 0 0 0 0 0 0 0 0 0 0 0 0
  条数=18 ｜ 码 0 的=16 ｜ 非 0 的=2 ｜ 其中 1 的=2
  两个加起来等于条数吗：等于
  ★ 「码 1」会不会是恒零的假守卫？塞一条 1 进台账再撤掉：
    塞进去 → 从 2 变成 3（响了）；撤掉探针回到 2
  → 三条能带走的话：① -e/-f/-s 全跟着链接走，只有 -L 看链接本身，拦链接得显式写；
    ② 先测后开等于把开哪个路径交给别人 —— 用 mktemp（不可猜+600）或 set -C 让 open 自己判；
    ③ flock 防的是别的进程，不防同一个 fd 的自己；名字看不清就用 %q，但别拿 eval 接用户输入。
[退出码 0]
```

18 条命令、码 0 的 16、非 0 的 2（两条都是 `mktemp` 的模板报错），
「码 1」那个数同样先塞假码让它从 2 变 3 再撤掉。

---

## 12. 小证据：把「改指」手动插在测与写之间

第 11 节第 5 格那一刀是脚本自己演的（第三方改不了你的时序），这里用两个普通文件把它放大到肉眼可见：
**测的那一刻为真，写的那一刻对象已经不是同一个了 —— 而且中间没有任何报错。**

```
$ mkdir -p t && cd t || exit 1
printf "%s\n" 原始内容 > important.txt
printf "%s\n" 别人的东西 > other.txt
ln -sf other.txt link
printf "  ① 测：[ -f link ] → %s ｜ [ -L link ] → %s\n" "$( [ -f link ] && echo 是 || echo 否)" "$( [ -L link ] && echo 是 || echo 否)"
printf "  ② 就在这一行与下一行之间，别人把 link 改指到 important.txt（本章手动插这一步）\n"
ln -sf important.txt link
printf "  ③ 脚本按 ① 的结论继续写：echo 新东西 > link\n"
echo 追加的内容 > link
printf "  结果 important.txt=[%s] ｜ other.txt=[%s]\n" "$(cat important.txt)" "$(cat other.txt)"
echo "  ★ 它写进了你「测过之后」才出现的那个对象，全程零报错。要么用 set -C / O_EXCL，要么别测了再开。"
cd .. && rm -rf t
printf "  现场已收拾：本目录还剩 %s 个多余目录\n" "$(ls -d */ 2>/dev/null | grep -vc ^lib/)"
  ① 测：[ -f link ] → 是 ｜ [ -L link ] → 是
  ② 就在这一行与下一行之间，别人把 link 改指到 important.txt（本章手动插这一步）
  ③ 脚本按 ① 的结论继续写：echo 新东西 > link
  结果 important.txt=[追加的内容] ｜ other.txt=[别人的东西]
  ★ 它写进了你「测过之后」才出现的那个对象，全程零报错。要么用 set -C / O_EXCL，要么别测了再开。
  现场已收拾：本目录还剩 0 个多余目录
[退出码 0]
```

`①` 测的时候 `link` 是链接、也确实是文件（`-f=是`、`-L=是`），脚本据此决定「可以写」；
`②` 就在这一行与下一行之间把 `link` 改指到 `important.txt`；
`③` 写入落地，屏幕上 `结果 important.txt=[追加的内容] ｜ other.txt=[别人的东西]` ——
**内容进了那个你「测过之后」才出现的目标，`other.txt` 一个字没动。**
最后一行是这一节唯一要带走的话：要么用 `set -C` / `O_EXCL` 让 `open()` 自己判，要么别测了再开。
那句「现场已收拾：本目录还剩 0 个多余目录」是同一道守卫在这里的读数，它在第 18 节 ② 被当场弄响过一次（造一个多余目录 → 1，撤掉 → 0）。
---

## 13. weirdnames.sh 完整跑：名字走完整条链

四个名字自始至终是同一批：**带空格**（`有 空格.txt`）、**中文里带空格**（`中文 名.txt`）、
**自带换行**（`带\n换行.txt`）、**以 `-` 开头**（`-dash.txt`）。
八格走的是同一条链：造出来 → 数组 → 循环 → 交给外部命令 → 跨进程 → 删掉 → 收账。

```
=== 1. 先造四个怪名字，并先证明「数出来几个」这件事本身数得对 ===
  造之前 wn/ 里 0 个文件，造之后 4 个 —— 计数函数会跟着事实变，不是恒零的摆设
  同一批换成 ls -A | wc -l 来数，得到 5 个
  → 直觉以为「4 和 5 里必有一个写错了」：两个都没错 —— wc -l 数的是**换行符**，
    而这四个名字里有一个自带换行，它一行变两行。要「几条记录」就得数 NUL。
  ★ 四个名字分别是：带空格、中文里带空格、自带换行、以 - 开头。下面每一格拿它们跑全链。
=== 2. 数组 → 循环：加引号走 4 次，去掉引号走 7 次 ===
  for f in "${arr[@]}"      → 循环体走 4 次（数组元素个数 ${#arr[@]} = 4）
  for f in ${arr[@]}（去引号）→ 循环体走 7 次，比上面多 3 段
  多出来的段长这样（循环体实际拿到的名字）：[有][空格.txt][中文][名.txt][带][换行.txt][-dash.txt]
  → 直觉以为「少个引号顶多多跑几次，无所谓」：拿到的是「有」「空格.txt」这种半截名字 ——
    它不报错，只是安静地对着并不存在的文件干活。
  ★ 元素里带空格还是换行都不影响 ${#arr[@]}（4）；只有**展开时没引号**才会被 IFS 和换行劈开。
```

第一格先解决一件更基础的事：**「数出来几个」本身会不会数错**。
同一批文件，数 NUL 得 4、`ls -A | wc -l` 得 5 —— 两个都没错，因为 `wc -l` 数的是**换行符**，
而那个自带换行的名字自己就占两行。要「几条记录」就得数 NUL。
第 2 格才是本章的主结论：`"${arr[@]}"` 走 4 次、`${arr[@]}` 走 7 次，
屏幕上把多出来的 7 段原样列出来了：`[有][空格.txt][中文][名.txt][带][换行.txt][-dash.txt]` ——
拿到的是**半截名字**，而它不报错，只是安静地对着并不存在的文件干活。
注意 ★ 那行：元素里带空格还是带换行都不影响 `${#arr[@]}`（还是 4），
**只有展开时没引号**才会被 IFS 和换行劈开。

```
=== 3. 交给外部命令：内层用 "$@" 接 与 用 $@ 接 ===
  内层敲的是：echo "内层参数个数 = $#"; for a in "$@"; do echo "  一个名字 →[$a]"; done
  交出去的名字：4 个 →|[有 空格.txt]|[中文 名.txt]|[带
换行.txt]|[-dash.txt]
    内层参数个数 = 4
      一个名字 →[有 空格.txt]
      一个名字 →[中文 名.txt]
      一个名字 →[带
    换行.txt]
      一个名字 →[-dash.txt]
  rc=0
  内层敲的是：k=0; for a in $@; do k=$((k + 1)); echo "  一段 →[$a]"; done; echo "切成 $k 段"
  交出去的名字：4 个 →|[有 空格.txt]|[中文 名.txt]|[带
换行.txt]|[-dash.txt]
      一段 →[有]
      一段 →[空格.txt]
      一段 →[中文]
      一段 →[名.txt]
      一段 →[带]
      一段 →[换行.txt]
      一段 →[-dash.txt]
    切成 7 段
  rc=0
  → 内层参数个数 4，可「一个名字 →[…]」在屏幕上占了 5 行 —— 那个自带换行的名字是**一个**参数。
  ★ 同一批名字、同一种交法，内层只把 "$@" 换成 $@，参数就当场变 7 段（父侧的引号白加了）。
```

这一格是父侧/内层的分工：父侧用 `"$@"` 交出去（4 个参数），
内层只要把 `"$@"` 换成 `$@`，**父侧加的引号一点用都没有**，当场变 7 段。
还有一处容易被眼睛骗过去：内层明明打印「参数个数 = 4」，
而「一个名字 →[…]」在屏幕上占了 **5 行** —— 那个自带换行的名字是**一个**参数、两行文字。
「行」和「参数」不是一回事，这条在第 15 节的骨架里变成 `wc -l` 的账。

```
=== 4. "$*" 与 "$@"：一个拼成一根串，一个永远是多个参数 ===
  默认 IFS 下 "${arr[*]}" =[有 空格.txt 中文 名.txt 带
换行.txt -dash.txt]
  把 IFS 改成冒号，同一条 "${arr[*]}" =[有 空格.txt:中文 名.txt:带
换行.txt:-dash.txt]
  同一个 IFS 下 "${arr[@]}" 交给 set --，参数个数还是 4
  默认 IFS 下**去掉引号** set -- ${arr[@]} → 7 个
  → "$*" 按 IFS 把名字**粘成一根串**：默认 IFS 里本来就含空格，所以第一格看着像对的，那是运气；
    IFS 一变（第二格的冒号），粘合剂跟着变，名字边界当场没了。
  ★ "$@" 不受 IFS 影响（第三格仍是 4 个）：只要这批名字还可能被再展开一次，就用 "$@"。
=== 5. 把 IFS 换成换行符的错觉：4 个名字照样数出 5 段，还顺手改掉了别的展开 ===
  list = 命令替换的结果（一行一个名字）。默认 IFS：for f in $list → 7 段
  改成 IFS=换行：同一条循环 → 5 段，而 wn/ 里真实文件数是 4 —— 两头都不对
  → 直觉以为「按行分就稳了」：那个自带换行的名字本身就跨两行，按行数必然多算一段。
  ★ 代价是**全局**的：同一串 sp（a b 两个空格 c），默认 IFS 下 set -- $sp → 3 个，
    设了 IFS=换行之后同一条 → 1 个。为一处改 IFS，脚本里后面所有未加引号的展开都跟着换规则。
```

`"$*"` 与 `"$@"` 的差别用 IFS 一试就现形：`"$*"` 按 IFS 把名字**粘成一根串**，
默认 IFS 里本来就含空格，所以第一格看着像对的（那是运气）；把 IFS 换成冒号，粘合剂跟着换，
名字边界当场没了。`"$@"` 不受 IFS 影响（第三格 `set --` 之后还是 4 个）。
第 5 格是「按行分总稳了吧」的错觉：`IFS=换行` 之后仍然数出 **5 段**（真实 4 个），
因为那个名字本身就跨两行；而代价是**全局**的 ——
同一串 `a b  c`，默认 IFS 下 `set -- $sp` 得 3 个，设了 `IFS=换行` 之后得 **1 个**。
为一处改 IFS，脚本里后面所有未加引号的展开都跟着换规则。

```
=== 6. find | xargs：不带 -0 收到 7 条，带 -0 收到 4 条 ===
  find -print  | xargs printf  → 外部命令收到 7 条，rc=0（它不报错，只是把名字改了）
  find -print0 | xargs -0      → 外部命令收到 4 条（wn/ 里真实文件数 = 4）
  xargs -t -n1 把它实际执行的每条命令原样吐到 stderr，实测 7 行：
    /bin/true wn/-dash.txt
    /bin/true wn/中文
    /bin/true wn/带
    /bin/true wn/有
    /bin/true 名.txt
    /bin/true 换行.txt
    /bin/true 空格.txt
  → 这 7 行里有 3 行的名字前面连 wn/ 前缀都没有了 —— 半截名指的不是「不存在」，是**另一个位置**。
  ★ 配对写：-print0 必须配 -0。靠「我这批名字里没有空格」这种自律，撑不过同事新建的一个目录。
```

跨进程那一格最扎眼的是那 7 行 `xargs -t` 的原样回显：
里面有 **3 行的名字前面连 `wn/` 前缀都没有了**（`名.txt`、`换行.txt`、`空格.txt`）——
半截名指的不是「不存在」，是**另一个位置**。
`find -print | xargs` 收到 7 条而 `rc=0`，`-print0 | xargs -0` 收到 4 条、与真实文件数一致。
**配对写：`-print0` 必须配 `-0`**；靠「我这批名字里没有空格」这种自律，撑不过同事新建的一个目录。

```
=== 7. 一路删干净：引号管「不被劈开」，-- 管「不被当选项」，两件事各自独立 ===
  删之前 wn/ 实测 4 个
  内层敲的是：cd wn && rm -f "$@"
  交出去的名字：4 个 →|[有 空格.txt]|[中文 名.txt]|[带
换行.txt]|[-dash.txt]
    rm: invalid option -- 'a'
    Try 'rm ./-dash.txt' to remove the file '-dash.txt'.
    Try 'rm --help' for more information.
  rc=1
  内层敲的是：cd wn && rm -f -- $@
  交出去的名字：4 个 →|[有 空格.txt]|[中文 名.txt]|[带
换行.txt]|[-dash.txt]
    （一声没出）
  rc=0
  内层敲的是：cd wn && rm -f -- "$@"
  交出去的名字：4 个 →|[有 空格.txt]|[中文 名.txt]|[带
换行.txt]|[-dash.txt]
    （一声没出）
  rc=0
  三跑的码：1 / 0 / 0，三跑之后的剩余文件数：4 → 3 → 0
  → 第一跑撞在 -dash.txt 上：rc=1，当场报错，一个也没删（看得见，所以好办）；
    第二跑 rc=0、一声没出，却只删掉 1 个，剩下 3 个原样躺着。
    被删掉的正是那个唯一没被劈开的名字 -dash.txt（现在：[已经没了]）—— 剩下三个带空格的一个没动。
  ★ 计数守卫：删之前 4 → 坏写法之后 3 → 正解之后 0，三个数全是现场量的，不是写死的。
```

这一格把「引号」和「`--`」两件事彻底分开：
第一跑 `rm -f "$@"` 撞在 `-dash.txt` 上 —— `rc=1`、当场报错、一个也没删（**看得见，所以好办**）；
第二跑 `rm -f -- $@` —— **`rc=0`、一声没出，却只删掉 1 个**，剩下 3 个原样躺着，
被删掉的正是那个唯一没被劈开的名字；
第三跑 `rm -f -- "$@"` 才是 `rc=0` 且剩 0 个。
三跑的码 `1 / 0 / 0`，三跑之后的剩余 `4 → 3 → 0`，三个数全是现场量出来的。

```
=== 8. 收账：上面每一次 run_arr 的真实退出码 ===
  按出场顺序的码：[0 0 1 0 0]
  条数=5 ｜ 码 0 的=4 ｜ 非 0 的=1 ｜ 两个加起来等于条数吗：等于
  → 5 次跑里非 0 的只有 1 次，其余 4 次全静默 —— 不报错正是它难查的原因。
    第 7 格中间那一跑就是这个形状：码 0、屏幕上啥也没说，事情没做成。
  ★ 能带走的一句话：名字从头到尾只该以「数组元素」的形态存在 —— "${arr[@]}" 交出去、"$@" 接住、
    循环里用 "$f"、外部命令前放 --、跨进程用 -print0 配 -0；中间任何一步退回字符串，
    就得赌这批名字里没有空白 —— 而这一格刚证明：赌输的时候不报错。
[退出码 0]
```

5 次 `run_arr` 里非 0 的只有 1 次，其余 4 次全静默。
最后那段 ★ 是本章关于名字的唯一总纲：**名字从头到尾只该以「数组元素」的形态存在** ——
`"${arr[@]}"` 交出去、`"$@"` 接住、循环里用 `"$f"`、外部命令前放 `--`、跨进程用 `-print0` 配 `-0`；
中间任何一步退回字符串，就得赌这批名字里没有空白，而第 7 格刚证明：**赌输的时候不报错**。

---

## 14. 小证据：`rm` 的四种写法排成真值表

第 13 节第 7 格跑的是「数组交出去」那条链，这里把四种写法并排，只看两个数：**码、还剩几个**。
现场每次都是两个名字：带空格的那个 + 以 `-` 开头的那个。

```
$ make(){ rm -rf w; mkdir -p w; touch -- "w/有 空格.txt" "w/-dash.txt"; }
try(){ make; o=$(cd w && eval "$1" 2>&1); r=$?; printf "    写法：%s\n" "$1"; printf "      → 码=%s ｜ 剩=%s ｜ %s\n" "$r" "$(find w -type f | wc -l)" "$( [ -n "$o" ] && echo 有报错 || echo 一声没出)"; }
echo "  每次造的现场都是两个名字：带空格的那个 + 以 - 开头的那个（这里用 eval 只是为了排成表，真实脚本写的是字面命令）"
try "rm -f 有 空格.txt -dash.txt"
try "rm -f -- 有 空格.txt -dash.txt"
try "rm -f \"有 空格.txt\" -dash.txt"
try "rm -f -- \"有 空格.txt\" \"-dash.txt\""
echo "  ★ 其中 rm -f -- 有 空格.txt -dash.txt 那一条：码=0、一声没出，却少删了一个 ——
    rc=0 证明的是「没报错」，不是「办成了」。"
rm -rf w
  每次造的现场都是两个名字：带空格的那个 + 以 - 开头的那个（这里用 eval 只是为了排成表，真实脚本写的是字面命令）
    写法：rm -f 有 空格.txt -dash.txt
      → 码=1 ｜ 剩=2 ｜ 有报错
    写法：rm -f -- 有 空格.txt -dash.txt
      → 码=0 ｜ 剩=1 ｜ 一声没出
    写法：rm -f "有 空格.txt" -dash.txt
      → 码=1 ｜ 剩=2 ｜ 有报错
    写法：rm -f -- "有 空格.txt" "-dash.txt"
      → 码=0 ｜ 剩=0 ｜ 一声没出
  ★ 其中 rm -f -- 有 空格.txt -dash.txt 那一条：码=0、一声没出，却少删了一个 ——
    rc=0 证明的是「没报错」，不是「办成了」。
[退出码 0]
```

四行读下来只有两个组合是「对的」，而**最难发现的是第二行**：
`rm -f -- 有 空格.txt -dash.txt` —— **码=0、一声没出，却少删了一个**。
它没报错，因为它确实删掉了它认出的那一个（`-dash.txt`），
而带空格的名字被劈成两段之后，`-f` 让「不存在」也不算错。
所以 ★ 那句必须钉住：**`rc=0` 证明的是「没报错」，不是「办成了」**；
要证明「办成了」，得像这块屏幕一样把剩余数**数出来**。

```
$ make(){ rm -rf w; mkdir -p w; touch -- "w/有 空格.txt" "w/-dash.txt"; }
make; rm -f -- "w/有 空格.txt" "w/-dash.txt"
printf "  同一个 make 之后按第 4 行写法删：剩 %s 个（判据是数出来的，不是猜的）\n" "$(find w -type f | wc -l)"
make; ( cd w && rm -f "有 空格.txt" "-dash.txt" ) 2>/dev/null; rc=$?
printf "  单引住第一个名字、不写 -- 的那一条：码=%s 剩 %s 个\n" "$rc" "$(find w -type f | wc -l)"
printf "  ★ 这一格写的时候真踩过坑：原来这里紧跟一句 cd ..，于是 \$? 读到的是 cd 的 0 —— 第 8 节讲的正是它。\n"
rm -rf w
  同一个 make 之后按第 4 行写法删：剩 0 个（判据是数出来的，不是猜的）
  单引住第一个名字、不写 -- 的那一条：码=1 剩 2 个
  ★ 这一格写的时候真踩过坑：原来这里紧跟一句 cd ..，于是 $? 读到的是 cd 的 0 —— 第 8 节讲的正是它。
[退出码 0]
# ★ 引号管「不被劈开」，-- 管「不被当选项」：两件事各自独立，缺一个就少删一片。
```

下面那块补两件事。第一行是判据本身：同一种写法换到带目录前缀的名字上，剩 0 个 ——
这个 0 是 `find | wc -l` 数出来的，不是猜的。
第二行是第 3 种写法（单引住第一个名字、不写 `--`）：`码=1 剩 2 个`，报错挡在前面，反而好办。
那行 ★ 是**写这一格时真踩到的坑**：原本这里紧跟一句 `cd ..`，
于是 `$?` 读到的是 `cd` 的 0 —— 第 8 节讲的正是它，这里被它咬了一口。

---

## 15. 综合演练：一个能上线的脚本骨架

下面这 52 行就是把前面所有结论拼起来的最小骨架：给一批文件各写一份「行数报告」。
它由 `demo.sh` 现场写进沙箱（`lib/skel.sh`）再挨个撞，正文里这份与沙箱里那份逐字一致。
每条规矩都能回指本章某一节，注释里就标着节号。

```bash
#!/usr/bin/env bash
# skel.sh —— 本章结论拼出来的最小可用骨架：给一批文件各写一份「行数报告」。
# 它故意不打印「一切正常」，只在最后给出实测计数；失败一律走 stderr + 显式退出码。
set -euEo pipefail                 # 四件衣服一次穿满（第 3 节）
__rc=0
work=''
cleanup() {
    __rc=$?                        # ★ 第一句先把码抢下来（第 8 节）
    if [ -n "$work" ]; then rm -rf -- "$work"; fi
    printf '[收尾] 退出码=%s ｜ 临时目录建在本目录，已删\n' "$__rc" >&2
}
trap cleanup EXIT
umask 077                          # 报告和临时目录默认不给别人读写

usage() { printf '用法: %s [-n 行数上限] [-q] 文件...\n' "$0" >&2; }

quiet=0 max=3
while getopts ':n:qh' o; do        # 首字符冒号 = 静默模式，报错自己写（第 10 节）
    case $o in
        n) max=$OPTARG ;;
        q) quiet=1 ;;
        h) usage; exit 0 ;;
        :) printf '选项 -%s 少了一个值\n' "$OPTARG" >&2; exit 2 ;;
        \?) printf '不认识的选项 -%s\n' "${OPTARG-（OPTARG 未设）}" >&2; exit 2 ;;
        *) printf '不该落到这里 -%s\n' "$o" >&2; exit 2 ;;
    esac
done
shift $((OPTIND - 1))

[ "$#" -gt 0 ] || { usage; exit 2; }   # ★ 最外层那道闸是 $#（第 9 节）

work=$(mktemp -d ./skel-XXXXXX)        # 建在当前目录，跑完自己删；随机名不进屏幕
done_n=0
skip_n=0
for f in "$@"; do                       # 名字全程保持「一个元素」的形态（第 13 节）
    if [ ! -f "$f" ]; then
        printf '跳过（不是普通文件）：%s\n' "$f" >&2
        skip_n=$((skip_n + 1))
        continue
    fi
    n=$(wc -l < "$f")
    if [ "$n" -gt "$max" ]; then
        printf '超过上限：%s（%s 行 > %s）\n' "$f" "$n" "$max" >&2
    fi
    dest="$work/r$done_n.rpt"
    ( set -C; printf '行数=%s\n来源=%s\n' "$n" "$f" > "$dest" )   # 不覆盖已存在的（第 11 节）
    done_n=$((done_n + 1))
    [ "$quiet" -eq 1 ] || printf '已报告：%s → %s 行\n' "$f" "$n"
done

printf '[完成] 收到 %s 个操作数 ｜ 写了 %s 份报告 ｜ 跳过 %s 个\n' "$#" "$done_n" "$skip_n"
[ "$skip_n" -eq 0 ] || exit 1           # 有跳过就报非 0，让调用方能判
```

四个设计决定值得单独指出：
**① 失败一律写成 `if` 判断**，不靠 `-e` 兜（第 6 节）—— 第 5 节第 7 格那种「条件位把体内关灯」
在这份骨架里不是风险，而是**被当作唯一的工作方式**：`-e` 穿着，但成败靠显式的码和计数。
**② `getopts` 用静默模式**，所以 `:` 支和 `?` 支两支都必须写（第 9、10 节），usage 自己印。
**③ 写入用 `( set -C; … )`**，不测了再开（第 11、12 节）。
**④ 最后交出的是实测计数**，且「有跳过就退 1」—— 调用方判的是码，而那个 `rc=0` 背后是一个数出来的 0，不是「没人报错」。

```
$ 〔在 . 里〕 touch -- "有 空格.txt" "-dash.txt"; bash skel.sh -- "有 空格.txt" "-dash.txt"
已报告：有 空格.txt → 0 行
已报告：-dash.txt → 0 行
[完成] 收到 2 个操作数 ｜ 写了 2 份报告 ｜ 跳过 0 个
[收尾] 退出码=0 ｜ 临时目录建在本目录，已删
[退出码 0]

$ 〔在 . 里〕 bash skel.sh "有 空格.txt" "-dash.txt"
已报告：有 空格.txt → 0 行
已报告：-dash.txt → 0 行
[完成] 收到 2 个操作数 ｜ 写了 2 份报告 ｜ 跳过 0 个
[收尾] 退出码=0 ｜ 临时目录建在本目录，已删
[退出码 0]

$ 〔在 . 里〕 bash skel.sh 有 空格.txt -dash.txt
跳过（不是普通文件）：有
跳过（不是普通文件）：空格.txt
已报告：-dash.txt → 0 行
[完成] 收到 3 个操作数 ｜ 写了 1 份报告 ｜ 跳过 2 个
[收尾] 退出码=1 ｜ 临时目录建在本目录，已删
[退出码 1]
```

三条怪名调用是这一节的核心对照：**带 `--`、不带 `--`、连引号也没了**。
前两条都是「收到 2 个操作数 ｜ 写了 2 份报告 ｜ 跳过 0 个 ｜ 退出码 0」——
**因为这个骨架从不把名字交给「会读选项」的命令**，`-dash.txt` 落到操作数位上没人把它当选项。
第三条引号一丢，两个名字被劈成三段：屏幕上 `收到 3 个操作数 ｜ 写了 1 份报告 ｜ 跳过 2 个`、
退出码 **1**。它这次会响，只是因为刚好有 `跳过` 这个计数项目。
换到第 14 节就不一样了：`rm` 会自己读选项，那边实测有一行是「码=0、一声没出，却少删了一个」。

```
$ 〔在 . 里〕 printf "a\nb\nc\n" > 三个.txt; printf "x\n" > 一个.txt; bash skel.sh -n 2 三个.txt 一个.txt
超过上限：三个.txt（3 行 > 2）
已报告：三个.txt → 3 行
已报告：一个.txt → 1 行
[完成] 收到 2 个操作数 ｜ 写了 2 份报告 ｜ 跳过 0 个
[收尾] 退出码=0 ｜ 临时目录建在本目录，已删
[退出码 0]

$ 〔在 . 里〕 bash skel.sh -q 一个.txt
[完成] 收到 1 个操作数 ｜ 写了 1 份报告 ｜ 跳过 0 个
[收尾] 退出码=0 ｜ 临时目录建在本目录，已删
[退出码 0]

$ 〔在 . 里〕 bash skel.sh
用法: skel.sh [-n 行数上限] [-q] 文件...
[收尾] 退出码=2 ｜ 临时目录建在本目录，已删
[退出码 2]

$ 〔在 . 里〕 bash skel.sh -x 一个.txt
不认识的选项 -x
[收尾] 退出码=2 ｜ 临时目录建在本目录，已删
[退出码 2]

$ 〔在 . 里〕 bash skel.sh -n
选项 -n 少了一个值
[收尾] 退出码=2 ｜ 临时目录建在本目录，已删
[退出码 2]

$ 〔在 . 里〕 bash skel.sh -n 1 -- 一个.txt
已报告：一个.txt → 1 行
[完成] 收到 1 个操作数 ｜ 写了 1 份报告 ｜ 跳过 0 个
[收尾] 退出码=0 ｜ 临时目录建在本目录，已删
[退出码 0]
# ★ 上面九条是骨架的全部输入面：怪名带 --、怪名不带 --、怪名连引号也没了，再是超限、安静、空、
#   坏选项、缺值、带值选项配 --。三条怪名三种下场：第 1、2 条的码都是 0，因为这个骨架从不把
#   名字交给「会读选项」的命令，-dash.txt 落到操作数位上没人把它当选项；第 3 条引号一丢，两个
#   名字被劈成三段（屏幕上实测：收到 3 个操作数、跳过 2 个），脚本自己交出 rc=1。
#   换到第 14 节就不一样了：rm 会自己读选项，那边实测有一行是「码=0、一声没出，却少删了一个」。
```

剩下六条把输入面撞完。`-n 2` 那格值得看清楚：超限**只警告不失败** ——
`超过上限：三个.txt（3 行 > 2）` 走 stderr，但照样 `已报告`、照样写报告、整体退出码 0。
所以骨架把「提醒」和「裁决」分开：警告只出声，**码由最后那句计数判断决定**（有跳过才退 1）。
`-q` 只留计数行；空参数、坏选项 `-x`、缺值 `-n` 三条都是 **usage/报错 + 退出码 2**
（这正是静默模式的代价：`bash` 那句原话不会替你说，得你自己写）。
`-n 1 -- 一个.txt` 则是「带值选项配 `--`」的合法形状（`--` 落在选项位上才刹车，第 9 节第 6 格）。

```
$ 〔在 . 里〕 printf "  跑完九条之后，本目录里残留的 skel-* 临时目录数：%s（守卫先响：下面故意造一个）\n" "$(ls -d skel-* 2>/dev/null | wc -l)"
mkdir skel-故意留下
printf "  故意造一个之后：%s ← 数得出来，所以上一条的 0 才算数\n" "$(ls -d skel-* 2>/dev/null | wc -l)"
rmdir skel-故意留下
printf "  撤掉探针之后：%s\n" "$(ls -d skel-* 2>/dev/null | wc -l)"
  跑完九条之后，本目录里残留的 skel-* 临时目录数：0（守卫先响：下面故意造一个）
  故意造一个之后：1 ← 数得出来，所以上一条的 0 才算数
  撤掉探针之后：0
[退出码 0]
```

最后是那道必须能响的守卫：跑完九条之后残留的 `skel-*` 临时目录数 = **0**，
紧接着 `mkdir skel-故意留下` 让它变 **1**，再 `rmdir` 撤掉回到 **0** ——
数得出来，上面那个 0 才算数。

---

## 16. 把这个骨架交给 `/bin/sh`（dash）撞一遍

`/bin/sh` 在本机是 dash。同一个文件、同一句调用，换 shell 就换一套语法 ——
先让 dash 真跑一遍本章骨架，再逐条撞它不认的东西。

```
$ 〔在 . 里〕 sh skel.sh 一个.txt
已报告：一个.txt → 1 行
[完成] 收到 1 个操作数 ｜ 写了 1 份报告 ｜ 跳过 0 个
[收尾] 退出码=0 ｜ 临时目录建在本目录，已删
[退出码 0]

$ 〔在 . 里〕 printf "l1\nl2\n" > "有 空格.txt"; touch -- "-dash.txt"; sh skel.sh -- "有 空格.txt" "-dash.txt"
已报告：有 空格.txt → 2 行
已报告：-dash.txt → 0 行
[完成] 收到 2 个操作数 ｜ 写了 2 份报告 ｜ 跳过 0 个
[收尾] 退出码=0 ｜ 临时目录建在本目录，已删
[退出码 0]

$ 〔在 . 里〕 sh -c "set -euEo pipefail; echo 走到这里"; printf "  码=%s ｜ -e -u -E -o pipefail 这四件 dash 全收下\n" "$?"
走到这里
  码=0 ｜ -e -u -E -o pipefail 这四件 dash 全收下
[退出码 0]
```

**这份骨架 dash 能整份跑完**（普通文件、怪名带 `--`，都是 `退出码=0`、报告一份没少），
而且 `-e -u -E -o pipefail` 这四件衣服 dash **全收下**（`码=0`）。
第二格还顺带证明了引号与 `--` 那套规矩在 dash 里同样成立。

```
$ 〔在 . 里〕 sh -c "false | true; echo 不加码=\$?; set -o pipefail; false | true; echo 加了pipefail=\$?"
不加码=0
加了pipefail=1
[退出码 0]

$ 〔在 . 里〕 sh -c "trap \"echo 〔dash 里 ERR 陷阱〕\" ERR; false"; printf "  码=%s ｜ 这就是第 7 节第 10 格那条原文\n" "$?"
trap: ERR: bad trap
  码=1 ｜ 这就是第 7 节第 10 格那条原文
[退出码 0]

$ 〔在 . 里〕 sh -c "a=1; echo [\${a[@]}]"; printf "  码=%s（bash 的数组展开在 dash 里是 Bad substitution）\n" "$?"
sh: 1: Bad substitution
  码=2（bash 的数组展开在 dash 里是 Bad substitution）
[退出码 0]

$ 〔在 . 里〕 bash -c "set -u; a=(); echo [\${a[@]}]"; printf "  同一个写法在 bash 5.3 里=%s（第 3 节第 5 格）\n" "$?"
[]
  同一个写法在 bash 5.3 里=0（第 3 节第 5 格）
[退出码 0]
# ★ 结论不是「别用 dash」，也不是「bash 写法一律不能移植」：这条骨架 dash 能整份跑完，
#   是因为它恰好一个 bash 专属特征都没用到（没有数组、没有 [[ ]]、没有 ERR 陷阱、没有 local -n）。
#   真正的分界就在上面那几行原文：set -o pipefail 它认，而且照样把管道的码改成 1
#   （第 4 节那张「码由 pipefail 决定」的表在 dash 里同样成立）；
#   trap ... ERR 它直接 bad trap —— 第 7 节整节的写法到 dash 里就没了。
#   要把脚本交给 /bin/sh，就得整份退回 POSIX 写法（第 01 章那条 shebang 的意义就在这）。
```

但收下不等于都实现，这里逐条实测：
`set -o pipefail` 它**不但认，还真实现** —— 同一条 `false | true`，不开时 `不加码=0`，开了 `加了pipefail=1`，
所以第 4 节那张「码由 pipefail 决定」的表在 dash 里同样成立。
`trap … ERR` 它直接 `trap: ERR: bad trap`（`码=1`）—— 第 7 节整节的写法到 dash 里就没了。
数组是两样死法：`${a[@]}` 报 `Bad substitution`（`码=2`），
而同一个写法在 bash 5.3 的 `set -u` 下**照常展开成 `[]`、码 0**（第 3 节第 5 格那条老结论的反面）。
最后那五行 ★ 是本节的结论：**不是「别用 dash」，也不是「bash 写法一律不能移植」** ——
这条骨架跑得通，是因为它恰好一个 bash 专属特征都没用到（没有数组、没有 `[[ ]]`、没有 ERR 陷阱、没有 `local -n`）；
真正的分界是那几条原文，**要把脚本交给 `/bin/sh`，就得整份退回 POSIX 写法**。

---

## 17. 对账：六个脚本各几格、合计几格、demo 自己几节

先看守卫能不能响：故意用一个匹配不到任何东西的模式（`=== ZZZ-NOT-THERE `）跑一遍，六个都是 0。

```
$ for f in strictfour errescue trapsafe argscheck filesafe weirdnames; do
  printf "  %-12s 假守卫计数=%s\n" "$f" "$(grep -c "=== ZZZ-NOT-THERE " "$f.sh")"
done
echo "  ★ 全 0 —— 所以下面这条真计数必须非 0，否则同样不可信"
  strictfour   假守卫计数=0
  errescue     假守卫计数=0
  trapsafe     假守卫计数=0
  argscheck    假守卫计数=0
  filesafe     假守卫计数=0
  weirdnames   假守卫计数=0
  ★ 全 0 —— 所以下面这条真计数必须非 0，否则同样不可信
[退出码 0]
```

**全 0 —— 所以下面那条真计数必须非 0，否则同样不可信。**
这就是本章所有「恒 0」结论的通用形：先证明这根探针能响，再报那个 0。

```
$ total=0
for f in strictfour errescue trapsafe argscheck filesafe weirdnames; do
  n=$(grep -c "^echo .=== " "$f.sh")
  printf "  %-12s %s 格\n" "$f" "$n"
  total=$((total + n))
done
printf "  ★ 六个脚本合计 %s 格（匹配式写成 ^echo .=== ：两种引号的分格标题都算，漏一种就会少数）\n" "$total"
  strictfour   11 格
  errescue     11 格
  trapsafe     11 格
  argscheck    9 格
  filesafe     9 格
  weirdnames   8 格
  ★ 六个脚本合计 59 格（匹配式写成 ^echo .=== ：两种引号的分格标题都算，漏一种就会少数）
[退出码 0]
```

`strictfour 11、errescue 11、trapsafe 11、argscheck 9、filesafe 9、weirdnames 8`，合计 **59 格**。
那行 ★ 顺便交代匹配式为什么写成 `^echo .===`：分格标题有两种引号写法，
**漏认一种就会少数** —— 这也是一个「先怀疑自己的正则，再怀疑被扫的对象」的例子。

```
$ printf "  demo 自己的节标题数（grep ^banner）：%s\n" "$(grep -c "^banner " demo.sh)"
printf "  沙箱 work 里除 demo 外的脚本数（含本章现造的骨架）：%s\n" "$(ls *.sh | grep -vc "^demo.sh$")"
  demo 自己的节标题数（grep ^banner）：18
  沙箱 work 里除 demo 外的脚本数（含本章现造的骨架）：7
[退出码 0]

$ printf "  源文件行数：demo=%s ｜ 骨架 skel.sh=%s ｜ 六脚本=%s\n" "$(wc -l < demo.sh)" "$(wc -l < skel.sh)" "$(cat strictfour.sh errescue.sh trapsafe.sh argscheck.sh filesafe.sh weirdnames.sh | wc -l)"
  源文件行数：demo=492 ｜ 骨架 skel.sh=52 ｜ 六脚本=825
[退出码 0]
# 这里数的「格」是脚本源码里的分格标题行；每一格都在屏幕上真实出现过（第 3~14 节逐条跑过）。
```

`demo` 自己的节标题 18 个（`grep ^banner`），沙箱里除 demo 外的脚本 7 个（六份取证 + 现造的骨架），
源文件行数 `demo=492`、`骨架 skel.sh=52`、`六脚本=825`。
最后一行那句提醒留着别跳：这里数的「格」是源码里的分格标题行，
**每一格都在屏幕上真实出现过**（第 3~14 节逐条跑过），不是清单上的规划。

---

## 18. 自证：沙箱里的事只留在沙箱

```
# ① 章目录（沙箱外面）开跑前后各列一次条目名，比对是否一模一样。
  开跑前 7 个条目 ｜ 现在 7 个条目 → 一模一样
# ② 每个取证脚本自己的现场子目录是否都删干净了（先故意造一个，让这道守卫响一次）。

$ mkdir -p zz-故意留下的目录
printf "  故意造一个多余目录之后，多余目录数=%s（★ 守卫响了）\n" "$(ls -d */ 2>/dev/null | grep -v "^lib/$" | wc -l)"
rmdir zz-故意留下的目录
printf "  删掉之后，多余目录数=%s（六个脚本的子目录都由它们自己的 trap 清掉了）\n" "$(ls -d */ 2>/dev/null | grep -v "^lib/$" | wc -l)"
  故意造一个多余目录之后，多余目录数=1（★ 守卫响了）
  删掉之后，多余目录数=0（六个脚本的子目录都由它们自己的 trap 清掉了）
[退出码 0]
# ③ 沙箱 work 里现在还剩什么（只印相对条目名）。

$ ls -A | sort | tr "\n" " "; echo
-dash.txt argscheck.sh demo.sh errescue.sh filesafe.sh lib skel.sh strictfour.sh trapsafe.sh weirdnames.sh 一个.txt 三个.txt 有 空格.txt 
[退出码 0]

$ printf "  lib/ 里剩：%s\n" "$(ls -A lib | sort | tr "\n" " ")"
  lib/ 里剩：skel.sh 
[退出码 0]
```

① 章目录（沙箱外面）开跑前后各列一次条目名：**7 个 → 7 个，一模一样**。
② 那道「多余目录」守卫先被 `mkdir zz-故意留下的目录` 弄响成 **1**，撤掉后回到 **0** ——
六个脚本的子目录都由它们自己的 trap 清掉了。
③ 沙箱里到最后一刻还留着 `-dash.txt`、`有 空格.txt`、`三个.txt` 这些测试文件
（还有 `lib/skel.sh` 和复制出来的 `skel.sh`），
**这恰恰是它们该在的地方**：它们跟着沙箱一起被 `cleanup` 删掉，一个都没漏到章目录里。

```
$ pat="curl|wget|nc -l|nohup|pip install|apt-get|yum install"
list=(strictfour.sh errescue.sh trapsafe.sh argscheck.sh filesafe.sh weirdnames.sh skel.sh lib/skel.sh)
printf "curl -s https://example.com\napt-get install x\n" > zz-probe.sh
printf "  ④-1 先弄响：把探针 zz-probe.sh 算进来，命中文件数=%s\n" "$(grep -lE "$pat" "${list[@]}" zz-probe.sh | wc -l)"
rm zz-probe.sh
printf "  ④-2 撤掉探针：同一道守卫命中文件数=%s（★ 响过一次，这个 0 才算数）\n" "$(grep -lE "$pat" "${list[@]}" | wc -l)"
  ④-1 先弄响：把探针 zz-probe.sh 算进来，命中文件数=1
  ④-2 撤掉探针：同一道守卫命中文件数=0（★ 响过一次，这个 0 才算数）
[退出码 0]

$ pat="curl|wget|nc -l|nohup|pip install|apt-get|yum install"
list=(strictfour.sh errescue.sh trapsafe.sh argscheck.sh filesafe.sh weirdnames.sh skel.sh lib/skel.sh)
printf "  ④-3 假阳性对照：把 demo.sh 也扫进来，命中文件数=%s\n" "$(grep -lE "$pat" "${list[@]}" demo.sh | wc -l)"
printf "     ↑ 多出来的那一个就是 demo.sh —— 禁词的字面文本就写在它自己那行 pat= 里。\n"
printf "     任何「扫源码找禁词」的守卫都会自匹配，所以扫描清单必须把持有模式串的那个文件挑出去。\n"
  ④-3 假阳性对照：把 demo.sh 也扫进来，命中文件数=1
     ↑ 多出来的那一个就是 demo.sh —— 禁词的字面文本就写在它自己那行 pat= 里。
     任何「扫源码找禁词」的守卫都会自匹配，所以扫描清单必须把持有模式串的那个文件挑出去。
[退出码 0]
```

④ 扫「有没有写过联网、装包、开监听端口的语句」，扫描对象是六份取证脚本 + 骨架（两份 skel 都算）。
三行读数缺一不可：**④-1 先把探针 `zz-probe.sh`（里面真的写着 `curl` 和 `apt-get`）算进来，命中 1**；
**④-2 撤掉探针，同一道守卫命中 0**；
**④-3 是假阳性对照：把 `demo.sh` 也扫进来，命中又变回 1** ——
多出来的那一个就是 `demo.sh` 自己，因为禁词的字面文本就写在它自己那行 `pat=` 里。
所以「扫源码找禁词」的守卫一定会自匹配，**扫描清单必须把持有模式串的那个文件挑出去**。

```
$ printf "  本 shell 的后台作业数=%s（本章没起过 & 作业）\n" "$(jobs -l | wc -l)"
  本 shell 的后台作业数=0（本章没起过 & 作业）
[退出码 0]
# ⑤ 屏幕上不该有真实身份。匹配串故意写成 /hom[e]/ 带方括号：
#   不带方括号时，它会命中「写着它自己的那一行」（永远非 0 的假阳性，④-3 就是活例子）；
#   带了方括号，模式文本不再自匹配，只有真实路径才算命中。同样先拿假想路径把守卫弄响。

$ list=(strictfour.sh errescue.sh trapsafe.sh argscheck.sh filesafe.sh weirdnames.sh skel.sh lib/skel.sh demo.sh)
printf "/hom%s/somebody/nowhere\n" "e" > zz-path-probe.txt
printf "  ⑤-1 先弄响：把含假想绝对路径的探针算进来，命中=%s 处\n" "$(grep -o "/hom[e]/[A-Za-z0-9._-]*" "${list[@]}" zz-path-probe.txt | wc -l)"
rm zz-path-probe.txt
printf "  ⑤-2 撤掉探针：连 demo.sh 自己一起扫，命中=%s 处（★ 响过一次，这个 0 才算数）\n" "$(grep -o "/hom[e]/[A-Za-z0-9._-]*" "${list[@]}" | wc -l)"
  ⑤-1 先弄响：把含假想绝对路径的探针算进来，命中=1 处
  ⑤-2 撤掉探针：连 demo.sh 自己一起扫，命中=0 处（★ 响过一次，这个 0 才算数）
[退出码 0]
# ⑤-3 这一节只印「命中几处」，不印命中的内容：grep -o 会把匹配到的路径原样送上屏幕，
#   数一遍就够 —— 想把内容印出来，等于亲手把家目录打到捕获里（所以第 3~17 节全用相对文件名跑）。
# ⑥ 真实身份的整屏扫描（用户名/主机名/家目录）由 README「怎么跑这一章」那道闸门在捕获外面做。
  demo 自己跑到这里，最后一节的退出码=0

[沙箱已删除] 前缀 bash08-（随机后缀不进屏幕；只有 --keep 时才印出完整路径）
```

⑤ 真实身份扫描同理：匹配串故意写成 `/hom[e]/[A-Za-z0-9._-]*` ——
不带方括号时它会命中「写着它自己的那一行」（永远非 0 的假阳性，④-3 就是活例子），
带了方括号，模式文本不再自匹配，只有真实路径才算命中。
先拿假想路径把守卫弄响（**⑤-1 命中 1 处**），再连 `demo.sh` 自己一起扫（**⑤-2 命中 0 处**）。
⑤-3 那句是这一节的方法论：**只印「命中几处」，不印命中的内容** ——
`grep -o` 会把匹配到的路径原样送上屏幕，想把内容印出来，等于亲手把家目录打进捕获里
（所以第 3~17 节全用相对文件名跑）。
⑥ 整屏的身份扫描（用户名/主机名/家目录）不在脚本里做，由下面「怎么跑这一章」那道闸门在捕获外面做。
最后一行 `demo 自己跑到这里，最后一节的退出码=0`，然后是那句沙箱已删除 ——
**随机后缀不进屏幕，只有 `--keep` 时才印出完整路径**。

---

## 一张表：本章全部开关与判断的分工

| 这一行 | 管什么 | 不管什么（实测出来的豁免） | 证据 |
|---|---|---|---|
| `set -e` | **普通位置**上失败就终止 | `&&` 左、`\|\|` 左、`if` 条件位、`!` 之后；函数一进条件位，**体内**也一起关灯 | 第 3 节 2、第 5 节 2~7 |
| `set -u` | 没定义过的名字被**读**就报错 | 本机 bash 5.3 下**空数组整体展开**不报错（`${#a[@]}`、`${a[0]}` 仍会） | 第 3 节 5、第 16 节末格 |
| `set -o pipefail` | 管道的**码**改成最后一个非 0 | **不会终止脚本**，得再穿 `-e`；dash 也认且真实现 | 第 3 节 6、8，第 4 节，第 16 节 |
| `set -E` | ERR 陷阱**进不进函数体** | 既不报错也不终止；不带它则函数里失败一声不响 | 第 3 节 9、10，第 7 节 3 |
| `trap … EXIT` | 正常收尾、TERM、INT 都跑 | `SIGKILL` 一律作废（屏幕上一个字都没有，rc=137） | 第 7 节 7 |
| `trap … ERR` | 失败时**响一声** | 停不停是 `-e` 的事；条件位不响；不带 `-E` 不进函数；dash 直接 bad trap | 第 7 节 2、4、10 |
| 陷阱体第一句 `__rc=$?` | 把退出码抢下来 | 晚了半句就被自己的赋值吃掉（码=0） | 第 7 节 5、第 8 节 |
| `-e` / `-f` / `-s` / `-d` / `-r` | 判断，**全跟着链接走** | 断链 `-e` 说「不存在」而名字明明在；拦链接必须显式 `-L` | 第 11 节 1 |
| `[ -e x ]` 之后再写 | —— | 等于把「开哪个路径」交给别人（TOCTOU） | 第 11 节 4、5，第 12 节 |
| `set -C`（noclobber） | 让 `open()` 当场判「已存在就报错」 | `>\|` 一强行就破功 | 第 11 节 6 |
| `flock` | 挡**别的进程重新 open 同一个文件** | 不挡同一个 fd 的自己；继承同一 fd 的子进程天生带着锁 | 第 11 节 7 |
| `mktemp` | 名字不可猜 **+** 600/700 权限 | 手写 `/tmp/xxx` 两条都丢；`--suffix` 时模板必须以 X 结尾 | 第 11 节 2、3 |
| `"$@"` / `"${arr[@]}"` | 保持「一个元素」的形态 | `"$*"` 会被 IFS 粘成一根串；去掉引号 4 个变 7 段 | 第 13 节 2~5 |
| `--` | 让外部命令别把名字当选项 | 紧跟带值选项时**被当值吃掉**；引号该加还得加 | 第 9 节 6，第 14 节 |
| `getopts` | 分选项 | 最外层闸门是 `$#`；非静默 `?` 支里 `OPTARG` 没设；`OPTIND` 不复位 | 第 9 节 1~5，第 10 节 |
| `rc=0` | 「没报错」 | **不等于「办成了」** —— 剩下的数要自己数 | 第 13 节 7，第 14 节 |

## 一页纸（考前扫这半页）

- 四件衣服是**叠加**不是覆盖：`-e` 管停、`-u` 管读未定义、`pipefail` 管码、`-E` 管 ERR 进不进函数。
- `-e` 的四位豁免：`&&` 左、`||` 左、`if` 条件位、`!`；**函数一旦进条件位，体内 `-e` 整块关灯**（全章最贵）。
- 函数交出的是**最后一条命令**的码，不是它中间失败过几次；`return 7` 在被 `-e` 掐死时留不下 7。
- 循环：条件位豁免（`while false` 不跑也不停），循环体是普通位置（第一次迭代就死）。
- 命令替换：赋值位会杀脚本，嵌在字符串里被咽掉；`-e` 只看串里**最后一条**。
- 陷阱三个时间差：定义时 `$?` 会被引号展开（要写 `\$?`）→ 触发时第一条赋值吞码（要先 `__rc=$?`）→ 清理被 `-TERM` 打断留半截现场。
- `ERR` 只响、`-e` 才停；`-9` 让所有 trap 作废；挂了 `TERM` 就必须自己 `exit` 收场。
- 参数：闸是 `$#`；`-ab` 是「a 的值等于 b」；非静默 `?` 支没有 `OPTARG`；静默 `:` 支唯一入口是开头那个冒号；函数里 `OPTIND=1`。
- `--` 只在选项位刹车，会被带值选项吃掉；名字交给会读选项的命令前放 `--` 或写 `./`。
- 名字只做四件事：数组存、`"$@"` 传、`"$f"` 用、`-print0 | xargs -0` 跨进程；`wc -l` 数换行，记录数要数 NUL。
- 文件：`-e/-f/-s/-r` 跟着链接走，`-L` 看链接本身；**别测了再开**，用 `mktemp` 或 `( set -C; … )`。
- `flock` 的锁跟着「打开的文件描述」走，不跟 fd 号、不跟文件名。
- 报错文本跟着 `LANGUAGE > LC_MESSAGES > LANG` 走（`LC_ALL` 盖不住已设好的 `LANGUAGE`）；**脚本里只按码分支**。
- 任何「恒 0」的计数，先塞一根注定响的探针；**`rc=0` 只证明没报错，剩下的数要自己数出来**。

## 这章的坑（按代价从大到小）

1. **把函数放进 `if` 条件位，同时指望 `set -e` 兜底。** 代价：体内失败照打、函数返回 0、走 `T`、
   `rc=0`（第 5 节第 7 格）——写 `if` 判成败的人和穿 `-e` 图省心的人在同一行拿到相反答案，
   三件套穿满也拦不住（第 3 节第 10 格第 2 条同形）。
2. **先测后开。** 代价：`-e` 对断链说「不存在」，`echo > 链接` 却凭空造出 `newtarget`（第 11 节第 4 格）；
   时序一拉开，写入落进一个**你从来没测过、已存在**的文件（第 11 节第 5 格、第 12 节），全程零报错。
3. **把「没报错」当成「办成了」。** 代价：`rm -f -- 有 空格.txt -dash.txt` **码=0、一声没出、少删一个**
   （第 14 节第 2 行）；`find -print | xargs` 收到 7 条还是 `rc=0`（第 13 节第 6 格）。
4. **清理函数第一句不是 `__rc=$?`。** 代价：陷阱体里任何一句赋值都把退出码改成 0，
   日志里的收尾行永远写着「退出码=0」，包括被 `-e` 掐死那一趟（第 7 节第 5 格、第 8 节）。
5. **以为挂了 ERR 陷阱就等于出错就停。** 代价：它只响（`tail` 照打、`rc=0`）；
   反过来不带 `-E` 时脚本因 `-e` 死掉却**一声不响**（第 7 节第 2、3 格）——
   你收到的可能是一条没有原因的退出。
6. **`-o pipefail` 单独穿。** 代价：它只改码不终止（第 3 节第 6 格、第 4 节第 2 格），
   线索躺在 `$?` 里没人读；`-e` 单独穿时它拿到的码本来就是 0（第 3 节第 8 格）。
7. **未加引号的 `$@` / `${arr[@]}`。** 代价：4 个名字变 7 段，拿到的是「有」「空格.txt」这种半截名字，
   **不报错**（第 13 节第 2、3 格）；父侧加引号救不了内层（同一格第二跑）。
8. **`find | xargs` 不配 `-print0/-0`。** 代价：7 条命令、其中 **3 条连目录前缀都丢了**，
   半截名指的不是「不存在」而是**另一个位置**（第 13 节第 6 格）。
9. **给 `TERM` 挂陷阱却不 `exit`。** 代价：默认动作被顶掉，脚本被叫不走（`rc=0`），
   管理器只能升级成 SIGKILL —— 于是从「少打一行日志」变成**屏幕上一个字都没有**（第 7 节第 7、8 格）。
10. **清理拆成多条命令。** 代价：被第二个信号打断时磁盘剩 1 个文件、屏幕上只有前半截证据
    （第 7 节第 9 格）。**半截清理比没清理更难查**：跑过的那半有证据，没跑的那半没有。
11. **`getopts` 在非静默的 `?` 分支里读 `$OPTARG`。** 代价：本机 bash 5.3 根本没设它，
    穿上 `-u` 直接 `unbound variable`、`rc=127`（第 9 节第 4 格第三行）。
12. **函数里用 `getopts` 不复位 `OPTIND`。** 代价：第二次调用**一声没出、`OPTIND` 还是上次的 3**，
    参数一个也没解析（第 10 节第 1 格）——本节写的时候真踩到。
13. **以为 `--` 一定结束解析。** 代价：紧跟带值选项时它成了值（`a=[--]`），后面的 `-b` 照样被当开关（第 9 节第 6 格）。
14. **以为 `-ab` 是两个开关。** 代价：`optstring="a:b"` 下它是「a 的值等于 b」，`b` 那个开关**消失了而 `rc=0`**（第 9 节第 3 格）。
15. **靠 `IFS=换行` 救按行切分。** 代价：自带换行的名字仍多数一段（5 vs 4），
    而且改动是**全局**的，后面所有未加引号的展开跟着换规则（第 13 节第 5 格）。
16. **用 `>` 自己拼 `/tmp/xxx`。** 代价：名字可预测（别人能先占）+ 默认宽权限，
    `mktemp` 一次买齐的两条一起丢（第 11 节第 2 格）。
17. **按报错文本做分支。** 代价：文本跟着 `LANGUAGE`/`LC_ALL` 变（第 1 节三行同命令三种语言），
    而**码不变** —— 只按 0/1/2/127/>128 分支。
18. **恒假守卫。** 代价：屏幕上那个「0」和「真干净」长得一模一样，比没有守卫更糟。
    本章每一个 0 前面都插了一根注定响的探针（第 3 节第 7 格、第 13 节第 7 格、第 15 节末、第 17、18 节）。

## 自检问题

1. `set -e` 下 `f(){ false; echo 体内; }; if f; then echo T; else echo F; fi` 打什么、码几？
   —— 打 `体内` 和 **`T`**，`rc=0`。条件位把体内 `-e` 关灯，函数返回最后一条 `echo` 的 0（第 5 节第 7 格）。
2. `set -e; false | true; echo hi` 会停吗？加 `pipefail` 呢？两件都加呢？
   —— 单穿 `-e`：不停（码本来就是 0）；单穿 `pipefail`：不停但码变 1；两件都穿：当场掐掉（第 3 节 6、8，第 4 节）。
3. 脚本里 `h(){ false; return 7; }; h` 被 `-e` 掐死时，脚本退出码是 7 吗？
   —— **是 1，不是 7**。`return` 的数字没机会成为脚本的码，想留住只能靠 EXIT trap 读 `$?`（第 5 节第 8 格）。
4. `while false; do …; done` 和 `for i in 1 2; do false; echo 体内; done` 在 `-e` 下谁死？
   —— while 的条件在条件位（一次不跑、`rc=0`）；for 的循环体是普通位置（第一次迭代就死、`rc=1`）（第 5 节第 9 格）。
5. `v=$(grep 没中 的文件)` 和 `echo "结果=$(grep 没中 的文件)"` 哪个杀脚本？
   —— 前者（赋值位）`rc=1`；后者失败被咽掉、`rc=0`（第 5 节第 10 格）。
6. `trap "echo 码=$?" EXIT` 打印的是退出码吗？
   —— 不是，**写陷阱那一行就被展开成 0 了**；要留到执行时得写 `\$?`（第 7 节第 1 格）。
7. 陷阱体第一句 `x=1` 之后读 `$?` 得到什么？
   —— 0。赋值自己成功了，把码顶掉；正解是首句 `__rc=$?`（第 7 节第 5 格、第 8 节）。
8. 挂了 ERR 陷阱但没挂 `-e`，脚本会停吗？挂了 `-e` 没挂 `-E`，函数里的失败会响吗？
   —— 都不：ERR 只响不停；不带 `-E` 时函数里的失败**一声不响**地终止脚本（第 7 节 2、3）。
9. `kill -9 $$` 时 EXIT 陷阱跑吗？`kill -TERM $$` 时呢？码分别是什么？
   —— `-9` 不跑，屏幕上一个字都没有，`rc=137`；TERM 会跑 EXIT，`rc=143`（=128+15），但主线被拦腰截（第 7 节第 7 格）。
10. `getopts "a:b"` 遇到 `-ab` 解析成什么？
    —— `a` 的值 = `b`，开关 `b` 没出现，`rc=0`（第 9 节第 3 格）。
11. 非静默模式下 `-z` 落到哪个分支、`OPTARG` 是什么？静默模式下 `-a`（缺值）落到哪？
    —— 都落 `?`，且非静默 `?` 支里 `OPTARG` **没设**；静默时缺值落 `:`、`OPTARG=[a]`（第 9 节 4、5，第 10 节）。
12. `getopts` 什么时候不再解析选项？`--` 什么时候不刹车？
    —— 遇到第一个非选项就停；`--` 紧跟带值选项时被当值吃掉，只有落在选项位才刹车（第 9 节 6、7）。
13. `[ -e link ]` 为假就能放心写吗？
    —— 不能。断链回「不存在」，写下去却在别人选好的路径上造出文件；正解是 `mktemp` 或 `( set -C; … )`（第 11 节 4~6，第 12 节）。
14. `flock -n 9` 在同一个 fd 上第二次调用拿得到吗？另一个进程重新 open 一个 fd 呢？
    —— 同一个 fd **拿得到**（不自锁）；别的进程重开 fd 拿不到；继承来的同一个 fd 天生带锁（第 11 节第 7 格）。
15. 本机 `mktemp` 建文件和目录的权限各是什么？
    —— **600 / 700**，不是 `>` 建出来的 644/755（第 11 节第 2 格）。
16. 四个名字（带空格、中文带空格、带换行、以 `-` 开头）为什么 `ls -A | wc -l` 数成 5？
    —— `wc -l` 数**换行符**，自带换行的那个名字占两行；要数记录得数 NUL（第 13 节第 1 格）。
17. `rm -f -- 有 空格.txt -dash.txt` 的码和剩余文件数？
    —— **码=0、剩 1 个**，一声没出。引号管「不被劈开」，`--` 管「不被当选项」，缺一个就少删一片（第 14 节）。
18. 同一个骨架交给 `sh`（dash），本章哪一条写法立刻失效？哪一条照样成立？
    —— `trap … ERR` 直接 `bad trap`、`${a[@]}` 是 `Bad substitution`；
    而 `-e -u -E -o pipefail` 四件全收下，`pipefail` 还真实现（第 7 节第 10 格、第 16 节）。

## 本章没演 / 没验证（绝不编造，都在这）

第 18 节屏幕上那几行是本章自己承认的边界，这里摊开并补齐：

- **flock 的另外两个反常**：只演了「同一个 fd 不自锁 / 重开 fd 才挡住 / 继承的 fd 带锁」三格，
  没演 `flock -w 超时` 的码、没演 `-x` 排他锁真的**阻塞**（那需要一个会等待的第二个进程）。
- **`-9` 之后的真实残留治理**：第 7 节只证了「SIGKILL 让 trap 一律作废」，
  没有演「下一趟启动时怎么把上一趟没扫干净的目录清掉」——那需要跨进程、跨时间的状态，本机演示不了。
- **`-r`、属主、ACL、只读挂载**：判断矩阵里的 `-r` 只演了「自己刚建的文件」；
  别人属主的文件、只读文件系统、immutable 位一律没碰（权限实验只在沙箱内自建目录，删前先改回来）。
- **TOCTOU 是手动插步**：第 11 节第 5 格和第 12 节都由脚本自己改链接，
  没有真并发；本章证的不是「概率多大」，而是「测完再开 = 开的是别人说了算的那个路径」。
- **`O_EXCL` 的直接写法**：只演了 `set -C` 和它被 `>|` 破功，没演 `> file` 之外的
  程序级 `open(..., O_EXCL)` 路径（本机没有 C 层演示），也没演 `set -C` 对 `>>` 追加、对 `tee` 这类外部命令**无效**。
- **`OPTIND` 复位坑的完整形状**：只演了同一函数连调两次；
  子 shell、`eval`、被 `trap` 打断后再进来这些路径下的 `OPTIND` 状态没验。
- **长选项**：只用 bash 内建 `getopts`（它天生不支持 `--help` 这种长选项，本章也就没写），
  外部 `getopt(1)` 的长选项写法与它的报错原文**没演**。
- **`shellcheck`**：本机没有（第 1 节屏幕上标了），所以本章所有脚本只靠**逐条真跑**验证，
  没有静态检查这一路；第 09 章会给出安装途径并如实标注「本机未验证」。
- **报错语言**：第 1 节钉的是 GNU 工具（rm/cat/bash 自身）的语言优先级；
  **dash 的报错原文在中文 locale 下会不会变**没测，所以第 7 节第 10 格、第 16 节那几句英文原话
  只在「demo 把 `LC_ALL`/`LANG`/`LANGUAGE` 钉成 C」这个前提下成立。
- **多字节名字的字节 vs 字符**：第 13 节那个 `字符数=7` 是本机 uutils `wc -m` 的读数，
  没和 GNU `wc`、`${#var}`（按字符）与 `awk length()`（mawk 按字节）逐一对齐。
- **性能**：本章样本都在几行以内，**没有任何耗时、内存、大文件**的测量，
  所以「`set -C` 慢不慢」「`-print0` 有没有开销」这类话一句都没写。
- **信号**：只演了 `TERM/INT/KILL` 三种死法，没演 `HUP`、`PIPE`、实时信号，
  也没演 `trap` 多个信号互相打断的完整矩阵（第 9 格那种「清理中途被插一刀」只演了一次）。

## 怎么跑这一章

前置（本机实测，第 1 节屏幕上逐条钉过）：bash **5.3.9(1)-release**、
GNU rm 9.7 / sed 4.9 / grep 3.12 / findutils 4.10，而 `cat`、`wc`、`tr`、`sort`、`head`、`paste` 是
**uutils coreutils 0.8.0**，`awk` 是 **mawk 1.3.4**，`/bin/sh` 指向 **dash**，**没有 `shellcheck`**。
第 1 节还钉了语言：demo 一进来就把 `LC_ALL`、`LANG`、`LANGUAGE` 三个一起钉成 C，
所以这份捕获里的报错全是英文原文 —— **在你自己的终端里跑，同一格可能是中文**（第 1 节演了为什么）。

```bash
cd bash-handbook/08-robust-scripts
bash demo.sh              # 18 节全跑一遍，跑完自动删沙箱
bash demo.sh --keep       # 跑完把沙箱留着，自己进去翻文件（会印出完整路径）
bash strictfour.sh        # 也可以只跑一份取证脚本：它自己 mktemp、自己 trap 清理
bash errescue.sh; bash trapsafe.sh; bash argscheck.sh
bash filesafe.sh; bash weirdnames.sh
```

`skel.sh` 不是章目录里的文件：它由 `demo.sh` 现场写进沙箱（`lib/skel.sh`，52 行）。
想拿它当模板，用 `--keep` 跑完从沙箱里取，或者直接抄本章第 15 节正文里那份 —— 两者逐字一致。

安全边界（第 18 节当场扫过并印在屏幕上）：本章六份取证脚本 + 骨架**没有**联网、装包、
开监听端口、`nohup`、后台 `&` 的语句（④ 那格先拿探针弄响一次，再报 0，并附了 `demo.sh` 自匹配的假阳性对照）；
屏幕上不出现真实身份（⑤ 那格用 `/hom[e]/` 形状扫过，命中 0 处），
随机名一律折成 `<沙箱>`；所有写操作都在 `mktemp -d` 起的沙箱里（前缀 `bash08-`），
由 `trap cleanup EXIT` 整体删掉；章目录本身只读，跑完多余目录 = **0**、条目数开跑前后一模一样。

想复现本书引文（README 里每一格都是从一次真实运行按行号搬出来的）：

```bash
bash demo.sh > ~/.cache/handbook-captures/新名字.txt 2> ~/.cache/handbook-captures/新名字.err
python3 ~/.local/bin/capture_index.py ~/.cache/handbook-captures/新名字.txt > 新名字-index.txt
python3 ~/.local/bin/expand_fences.py README.md ~/.cache/handbook-captures/新名字.txt
TAIL_TITLES='一张表|一页纸|这章的坑|自检问题|本章没演|怎么跑这一章|术语小抄|本章之后' \
  ~/.local/bin/chapter_gate.sh README.md ~/.cache/handbook-captures/新名字.txt '/bash08-[A-Za-z0-9]{6}' \
  bash08-readme-partA.md bash08-readme-partB.md bash08-readme-partC.md bash08-readme-partD.md
python3 ~/.local/bin/script_check.py *.sh      # 六份取证脚本九项全绿（demo 自身按惯例不入检）
```

沙箱正则这里写成 `'/bash08-[A-Za-z0-9]{6}'`（带那个斜杠）：`mktemp -d` 出来的名字永远是
`某目录/bash08-六个字符`，加上斜杠才是「一个真实路径」；不带斜杠时它会命中
`bash08-readme-partA.md` 这种片段文件名（`readme` 正好六个字母），那是假阳性 ——
第 18 节 ④-3 讲的正是这种「守卫扫到了自己」。

两遍跑把沙箱名抹掉之后 `diff` 为空，才算「这套证据每跑必同」。
本章定稿用的那份捕获是 `bash08.txt`（**1381 行、18 节、六脚本合计 59 格**），
七份脚本的 md5 记在 `bash08-scripts-md5.txt`；
我把哪几条断言写反了、每条守卫是怎么被弄响的，逐条记在 `bash08-改判-2026-10-15.md`
（其中第 11 条就是上面那个带斜杠的沙箱正则 —— 守卫也会假阳性）。

## 术语小抄

| 术语 | 一句话 | 哪一节 |
|---|---|---|
| 普通位置 | 既不在 `&&`/`\|\|` 左、也不在 `if`/`!` 里的命令位 —— `-e` 只管这儿 | 第 5 节 |
| 条件位 | 被拿去当「答案」的那些位置：`&&` 左、`\|\|` 左、`if` 条件、`!` 之后；函数进来了，体内的 `-e` 一起关灯 | 第 5、6 节 |
| `optstring` | `getopts` 那串字母，`x:` 表示 x 要值，**开头那个冒号是静默模式**（`: ` 分支唯一入口） | 第 9、10 节 |
| `OPTIND` / `OPTARG` | 下一个该读第几个参数（1 起算，`shift` 量是 `OPTIND-1`）/ 当前选项的值或坏字符（非静默 `?` 支里没设） | 第 9、10 节 |
| 静默模式 | `":…"`)：bash 一句原话不出，usage 得你自己印，两种事故靠 `?` 与 `:` 两个分支分开 | 第 9 节第 5 格 |
| ERR 陷阱 | 失败时响一声；**响 ≠ 停**，条件位不响，不带 `-E` 不进函数体，dash 直接 bad trap | 第 7 节 |
| EXIT 陷阱 | 正常收尾、TERM、INT 都跑；`SIGKILL` 一律作废；第一句必须先 `__rc=$?` | 第 7 节第 1、5、7 格 |
| `128+n` | 被信号干掉的退出码形状（TERM→143、INT→130、KILL→137） | 第 7 节第 7 格 |
| 跟着链接走 | `-e/-f/-s/-d/-r` 判的是**目标**，只有 `-L` 看链接本身；断链时 `-e` 说「不存在」 | 第 11 节第 1 格 |
| 断链（dangling） | 链接在、目标不存在：`-L=是`、`-e=否`，而写入会顺着它落在别人选的路径上 | 第 11 节第 4 格 |
| noclobber（`set -C`） | 把「已存在就报错」交给 `open()` 当场判，中间没人能插一步；`>\|` 破功 | 第 11 节第 6 格 |
| 打开的文件描述 | `flock` 锁的真正登记单位 —— 不是 fd 号，也不是文件名 | 第 11 节第 7 格 |
| TOCTOU | 「先测后开」：判定与动作之间的窗口由别人填（本章用手动插一步演示） | 第 11 节第 5 格、第 12 节 |
| `%q` | 把看不见的空格/引号/制表符变成能核对的字面；配 `eval` 才还原，所以别拿它接用户输入 | 第 11 节第 8 格 |
| NUL 分隔 | `-print0 \| xargs -0`、`read -d ''`：记录边界不能靠换行，因为名字里可以有换行 | 第 13 节第 1、6 格 |
| 词分裂 | 未加引号的展开被 IFS 劈开：4 个名字变 7 段，而 `rc=0` | 第 13 节第 2、3 格 |
| `LANGUAGE` 优先 | 报错语言按 `LANGUAGE > LC_MESSAGES > LANG` 挑；`LC_ALL` 盖不住已设好的 `LANGUAGE` | 第 1 节 |
| 计数守卫 | 每个「恒 0」之前先塞一根注定响的探针；**先证明它会响，后面的 0 才有意义** | 第 3 节第 7 格、第 15、17、18 节 |
| `<沙箱>` | 本章对 `mktemp -d` 随机名的统一写法：它每跑必变，所以压根不上屏 | 章头、第 18 节 |

## 本章之后

第 08 章管的是「脚本炸了会不会让你知道」。第 09 章接着问**另一半**：炸了之后怎么查 ——
`bash -x` 与 `PS4`（trace 里能看到展开**之后**的形状，也就是本章第 13 节那些半截名的现场）、
`time` 与 `strace -f -e trace=file`（本机有 `strace`，能真跑；它会当场映出第 11 节那条
「你以为写的是 `link`，其实 open 的是 `victimA`」）、`shellcheck`（本机没有，会如实标注并只给安装途径）。
写完不再靠「穿满四件衣服」求心安，而是能回答「它到底死在哪一行、开了哪个文件」。

再往后，第 10 章是批量重命名 / 日志轮转 / 目录校验这类真任务 ——
本章那份 52 行骨架就是它的起点，`skel.sh` 里那四个设计决定（失败写进 `if`、静默 `getopts`、
`( set -C; … )` 写入、最后交出实测计数）在每个任务里都会再用一遍。
第 11 章是全书速查，`set -euo pipefail`、`trap`、`"$@"`、`--`、`-print0 | xargs -0` 这几行会压缩成半页。

数据结构那边也别丢：`DS-in-C` 的 56 节全部绿灯之后，C 侧的「内存炸不响」（越界读、未初始化）
和本章的「shell 炸不响」其实是同一件事的两种语言版本 —— 都是**失败没有证据**。
