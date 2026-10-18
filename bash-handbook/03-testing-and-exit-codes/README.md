# 判断与退出码：`[ ]` vs `[[ ]]`、`&& || !`

> 这一章只干一件事：让你**亲眼看到**「一个判断到底是怎么得出真/假的，以及那个假是从哪个数字来的」。
> 下面每一段输出都是这台机器（bash 5.3.9 + dash 0.5.12，GNU grep 3.12 / diffutils 3.12 / sed 4.9，
> awk = mawk 1.3.4，coreutils 是 uutils Rust 版）上真实跑出来的，出自同一次
> `bash demo.sh 2>&1 | tee ~/.cache/handbook-captures/bash03.txt`（1711 行，退出码 0）。
> 每次重跑只有两处会变：**沙箱路径** `/tmp/bash03-XXXXXX` 和文件测试那节里的**时间戳/inode 号**，
> 除此之外逐字一致。
> 引用格式：`$ ...` 是真敲的命令，紧跟其后是它的真实输出，最后一行 `[退出码 N]` 是 demo 打的总结。
> 助手脚本（`exitcodes.sh` 等）内部的输出带两格缩进，那是它们自己打印的，不是我加的。

---

## 你会遇到的场景

先对上号，下面每一条都是本章实测过的现场。看看哪些是你已经踩过的：

1. `if [ $x = "abc" ]`，x 恰好是空的时候报 `unary operator expected`，你加了 `2>/dev/null` 让它「不响了」，
   然后这个分支永远走 `else`，你以为是逻辑写错了。
2. `[ $x ]` 看起来人畜无害，x=`b c` 时却报 `binary operator expected`；x 为空时又不报错，只是**永远为假**。
3. `[ 010 -eq 10 ]` 返回真。你以为 `-eq` 会按八进制读 `010`，其实它只按十进制读，`010` 就是 10。
4. `[ $f = v[0-9]* ]` 永远不成立 —— 你写惯了 `[[ ]]`，忘了 `[ ]` 的右边**不是模式**，而且还要先做文件名展开。
5. `[ a < b ]` 不报语法错，而是安安静静把 `b` 当成输入文件，然后告诉你 `b: No such file or directory`。
6. 一个脚本 `set -e` 加了等于没加：中间某条命令失败了，脚本却一路跑到最后并返回 0。
7. `cmd1 && cmd2 || cmd3`，`cmd2` 失败时 `cmd3` 也跑了 —— 你把它当成了 if-else。
8. `if [[ 1 == 1 ]]` 从 `#!/bin/sh` 的脚本里跑出来，什么都没做，退出码还是 0，看起来「成功了」。
9. 管道 `grep xxx file | wc -l`，`grep` 没匹配到你不知道；`$?` 永远是 `wc` 的 0。
10. `exit 300` 之后 `$?` 是 44，你以为是 bug。
11. `[[ $v =~ ^v([0-9]+) ]]` 好好写着，某天你「保险起见」给正则加了引号，从此永远匹配不上。
12. `case` 里写了 `a.c)`，它匹配的是 `abc` 还是 `a.c`？（答案：只匹配 `a.c`，模式是 glob 不是正则。）

这 12 个现场全部来自同一个心智模型的三层结构。**记住这三层，你以后遇到任何 bash 判断怪事都会先问一句：
「现在是哪一层在决定真假？」**

---

## 一句话版本

* bash 里**没有布尔值**。`if`、`&&`、`||`、`!`、`case` 看的只有一个东西：**上一条命令的退出码是不是 0**。
* `[ ]` 是一个**真正的程序**（名字叫 `[`，是 `test` 的同名别名），所以它的参数在进入之前就已被「拆词 +
  文件名展开」改过一遍 —— 这一层是 90% 的 `[ ]` 事故来源。
* `[[ ]]` 是 **bash 的语法**（不是命令），里面不拆词、不展开，还能直写 `&&`/`||`、glob、`=~` 正则。
  代价是它不是 POSIX，`sh`（本机是 dash）里根本不认识它。
* `A && B || C` **不是** if-else：`B` 失败时 `C` 也会跑。
* 退出码只在 0–255，`exit 300` 会回绕成 44；非零**没有大小含义**，`0/1/2/126/127/128+信号号`
  只是**命令作者**给你的约定，不是 shell 的语义。

---

## 引用约定

* **本章所有输出都是一次 `bash demo.sh` 跑出来的**，围栏里每一行都是那次运行的原文（未改写、未省略；
  需要截断时我会**关掉代码块、用正文说明省略了哪些行、再另起一个不含 `$` 的块**）。
* `[退出码 N]` 这一行由 demo 的 `runc` 助手打印，含义是「上面那条命令的 `$?`」。
* 助手脚本里的输出行前面有两格缩进，末尾的 `rc=N` 是脚本自己打印的（不是 `runc` 打的）。
* 沙箱路径统一形如 `/tmp/bash03-6个随机字符`，本章**只在这个目录里写文件**，跑完 `rm -rf`。
* 报错原文里出现的文件名都是沙箱里的相对路径（比如 `bad_double.sh: line 2: ...`），
  因为 demo 会先把助手脚本复制进沙箱再执行。

---

## 心智模型：判断是怎么得出真/假的（三层）

```text
你写的这一行：      if [ $x = "abc" ]; then ...

 ┌─ 第 1 层：展开（bash 在你按下回车后立刻做，第 02 章讲过展开与引号）
 │   $x  →  （空）        ← 拆词（word splitting）
 │   未加引号的 * ? [ ]    →  可能变成文件名（pathname expansion）
 │   结果：[  = abc ]   ← 「传给 [ 这个程序的参数」到这里已经定下来了
 │
 ├─ 第 2 层：某个程序/关键字执行判断，产生一个 0~255 的整数
 │   [ 是程序（内建，外部还有 /usr/bin/[）  → 它数了数自己的参数，按 POSIX 规则解释 → 2
 │   [[ 是 bash 的保留字                    → 它自己解析，不拆词不展开 → 0 或 1
 │   grep / diff / 任何外部命令              → 自己决定返回几
 │
 └─ 第 3 层：控制结构只看「是不是 0」
        if / while / until  → 0 走 then，非 0 走 else（**非 0 是几，if 不管**）
        A && B              → A 为 0 才跑 B
        A || B              → A 非 0 才跑 B
        ! A                 → 把 0 和非 0 对调
        整条的结果           → 又是下一个 0~255，继续被下一个控制结构读走
```

关键认知：

* **第 1 层出错，第 2 层就收到一堆畸形参数**。这就是「`[` 里忘了加引号」的全部危害 ——
  不是「不优雅」，而是**参数个数变了，程序换了套解释规则**。
* **第 3 层只看是不是 0，所以 1、2、127、137 在控制流里完全等价**。
  想区分它们只能自己去读 `$?` 的数值，而 `$?` 又只活一条命令（本章第 6 节实测）。
* 第 2 层的「谁」是可以换的：`[` 换成了 `[[`，第 1 层就不做了 —— 这正是 `[[ ]]` 存在的理由。

---

## 谁在说话：本章每个角色的真实身份

先把「谁在判断」钉死。下面是本机实测（`type -a` 会把内建和外部程序**两份都列出来**）：

```
$ type -a [ test
[ is a shell builtin
[ is /usr/bin/[
[ is /bin/[
test is a shell builtin
test is /usr/bin/test
test is /bin/test
[退出码 0]
```

```
$ type -a "[["
[[ is a shell keyword
[退出码 0]
```

外部那份 `/usr/bin/[` 是**另一个实现**（本机它是指向 uutils coreutils 的符号链接）：

```
$ stat -c "%s 字节  %F  %n" /usr/bin/[; readlink -f /usr/bin/[
28 字节  symbolic link  /usr/bin/[
/usr/lib/cargo/bin/coreutils/[
[退出码 0]
```

这三行说明三件事，后面所有结论都建立在它们之上：

| 角色 | 本机身份 | 是不是命令 | 第 1 层展开会不会影响它 |
|---|---|---|---|
| `test` | shell 内建 + `/usr/bin/test` | 是（程序） | **会**：它只收到参数 |
| `[` | shell 内建 + `/usr/bin/[`（uutils） | 是（程序，`test` 的同名别名） | **会**：同上，且必须自己以 `]` 结尾 |
| `[[` | `is a shell keyword` | **不是**（bash 语法） | 不会（左边不拆词、不展开；只有右边是模式） |
| `$?` | 变量（不是命令） | —— | 只保留**上一条**命令的码 |
| `${PIPESTATUS[@]}` | 数组变量 | —— | 只保留**上一条管道**每段的码 |
| `if/while/until` | 保留字 | —— | 只看 0 / 非 0 |
| `&& \|\| ;& ;;` | 控制运算符 | —— | 只看 0 / 非 0，且**从左往右** |
| `case` | 保留字 | —— | 用 **glob** 匹配模式，不看退出码 |
| `set -e` | shell 选项 | —— | 条件位里的失败**被豁免**（第 11 节实测） |

本机工具实现也顺手记下来，因为「报错原文」和「退出码约定」都是**这些实现**给的，不是 bash 给的：

```
$ bash --version | head -2
GNU bash, version 5.3.9(1)-release (x86_64-pc-linux-gnu)
Copyright (C) 2025 Free Software Foundation, Inc.
[退出码 0]
```

```
$ grep --version | head -1
grep (GNU grep) 3.12
[退出码 0]

$ diff --version | head -1
diff (GNU diffutils) 3.12
[退出码 0]

$ sed --version | head -1
sed (GNU sed) 4.9
[退出码 0]

$ command -v awk; awk -W version 2>&1 | head -1
/usr/bin/awk
mawk 1.3.4 20260129
[退出码 0]

$ command -v shellcheck || echo "shellcheck 本机没有：本章每个写法都是靠真跑验证的，没有静态检查器的意见"
shellcheck 本机没有：本章每个写法都是靠真跑验证的，没有静态检查器的意见
[退出码 0]
```

> ★ `[` 有内建和外部两份，`[[` 只有语法一份。
> 所以「`[` 里为什么必须有空格」这类问题，答案不在 bash 手册的「条件表达式」章节里，
> 而在「**一个程序是怎么数自己的参数的**」这件事里。本章第 8 节全部在讲这件事。

---

## 6. 退出码：唯一的判据

### 6.1 `$?` 是什么、活多久

```
$ true; echo $?
0
[退出码 0]

$ false; echo $?
1
[退出码 0]
```

`0` 是真、`1` 是假 —— 但注意上面两条的输出：`echo $?` 本身也是一条命令，
它打印完就把 `$?` 改成了自己的 0。所以：

```
$ false; echo "紧跟 false 的 \$? = $?"
紧跟 false 的 $? = 1
[退出码 0]

$ false; echo 打断一下 >/dev/null; echo "中间插了一条 echo 之后 \$? = $?"
中间插了一条 echo 之后 $? = 0
[退出码 0]

$ false; :; echo "中间插了一条冒号（内建空命令）之后 \$? = $?"
中间插了一条冒号（内建空命令）之后 $? = 0
[退出码 0]

$ false; v=1; echo "纯赋值 v=1 之后 \$? = $?"
纯赋值 v=1 之后 $? = 0
[退出码 0]

$ false; a=(1 2); echo "数组赋值 a=(1 2) 之后 \$? = $?"
数组赋值 a=(1 2) 之后 $? = 0
[退出码 0]

$ false; v=$((5-5)); echo "算术赋值 v=\$((5-5)) 之后 \$? = $?"
算术赋值 v=$((5-5)) 之后 $? = 0
[退出码 0]
# ↑ 结论：想拿退出码就得**紧贴着取**，或者干脆别经过 $? —— 直接把命令写成 if/&& 的条件。
# 「非 0 一律为假」是唯一判据；数值大小不参与判断：
```

这六条是本节唯一需要背下来的东西：

| 中间插了什么 | `$?` 变成 | 为什么 |
|---|---|---|
| `echo ...` | 0 | echo 成功了 |
| `:`（空命令） | 0 | `:` 的**定义**就是返回 0 |
| `v=1` 纯赋值 | 0 | 赋值本身就是一条命令 |
| `a=(1 2)` 数组赋值 | 0 | 同上 |
| `v=$((5-5))` 算术赋值 | 0 | 展开（`5-5`=0）不产生码，赋值产生 0 |
| 什么都没插 | 上一条的码 | `$?` 只活一条命令的距离 |

> ★ 想拿退出码，只有两条安全路：**紧贴着取 `$?`**，或者**干脆别经过 `$?`** ——
> 直接把命令本身写成 `if`/`&&`/`||` 的条件。第 12 节会看到「中间插一句话就把判据作废」的现场复现。

### 6.2 「非 0」没有大小：8 个不同的数字，同一个分支

```
$ for n in 0 1 2 3 126 127 128 255; do if bash -c "exit $n"; then echo "  exit $n → if 走 then（真）"; else echo "  exit $n → if 走 else（假）"; fi; done
  exit 0 → if 走 then（真）
  exit 1 → if 走 else（假）
  exit 2 → if 走 else（假）
  exit 3 → if 走 else（假）
  exit 126 → if 走 else（假）
  exit 127 → if 走 else（假）
  exit 128 → if 走 else（假）
  exit 255 → if 走 else（假）
[退出码 0]
# ↑ 2 和 255 和 1 在 if 眼里一模一样。「错误严重到什么程度」不是 shell 的语义，
#   只是**命令作者给人的约定**：0=成功，1=一般失败，2=用法/严重错误，126 127 128+n 有固定来历。
#   这条约定不属于 shell，属于每个命令自己 —— 第 2 节把它一条条跑出来。
# 取值只有 0-255，超出按 mod 256 回绕（第 2 节里 ( exit 300 ) 之后是 44，不是 300）：
```

**2 和 255 和 1 在 `if` 眼里一模一样。** 「错误有多严重」不是 shell 的语义，而是**每个命令的作者**
给自己定的约定。这个约定只对「读脚本的人」和「写上层封装的人」有用，对 shell 本身没有用。

### 6.3 取值只有 0–255，超出按 mod 256 回绕

```
$ bash -c "exit 256"; echo "exit 256 → $?"
exit 256 → 0
[退出码 0]

$ bash -c "exit 257"; echo "exit 257 → $?"
exit 257 → 1
[退出码 0]

$ bash -c "exit -1"; echo "exit -1 → $?"
exit -1 → 255
[退出码 0]
```

`exit 256` → 0，`exit 257` → 1，`exit -1` → 255。第 7 节里 `( exit 300 )` 给出 **44**，
因为 `300 mod 256 = 44`。这不是 bash 的怪癖，是**退出状态只有 8 位**这个 Unix 事实。

---

## 7. 常见退出码约定：逐条现跑

下面这一整块是 `exitcodes.sh` 的输出（`rc=` 后面是每条命令自己的 `$?`）。

```
──── A. grep：0=找到了，1=没找到（正常答复），2=没法干活
  grep -q alpha ec_a.txt      命中                 rc=0
  grep -q zzz   ec_a.txt      没命中              rc=1
  grep alpha ec_MISSING.txt   文件不存在
        grep: ec_MISSING.txt: No such file or directory
        → rc=2
  grep "[" ec_a.txt           正则写错
        grep: Invalid regular expression
        → rc=2
```

`grep` 的三段约定：**0 找到了 / 1 没找到 / 2 没法干活**。特别注意倒数第二条：

```
  grep -c zzz ec_a.txt        没命中时打印 0
        0
        → rc=1
  grep -q alpha ec_a.txt ec_MISSING.txt 一半好一半坏 rc=0
  grep -q alpha ec_dir        参数是个目录     rc=2
```

这里 `grep -c` **打印了 `0`**（这是正确答案：匹配行数是 0），退出码却是 **1**。
所以「输出对不对」和「退出码是不是 0」是两件事；`grep -c` 的 1 不是错误，是「没找到」这个正常答复。
最后一条也值得记：`grep -q` 只要**第一个文件**命中就返回 0，后面的文件读不到它也不管（`2` 被吞掉了）。

`diff` 用的是同一套三段约定：

```
──── B. diff：0=一样，1=不一样（正常答复），2=没法干活
  diff -q ec_a.txt ec_a.txt   完全相同           rc=0
  diff -q ec_a.txt ec_b.txt   有差异
        Files ec_a.txt and ec_b.txt differ
        → rc=1
  diff -q ec_a.txt ec_MISSING.txt  文件不存在
        diff: ec_MISSING.txt: No such file or directory
        → rc=2
  cmp -s ec_a.txt ec_b.txt    有差异（-s 静音） rc=1
```

而 `test`/`[` 的三段是本章最重要的一组数字 —— **0 真 / 1 假 / 2 你用错了**：

```
──── C. test / [ ]：0=真，1=假，2=用错了（注意 2 不是「假得厉害」）
  test 1 -eq 1              真                      rc=0
  test 1 -eq 2              假                      rc=1
  test -z ""                真                      rc=0
  test 1 -zz 2              运算符不存在
        exitcodes.sh: line 36: test: -zz: binary operator expected
        → rc=2
  test -eq 1                一元运算符缺左操作数
        exitcodes.sh: line 36: test: -eq: unary operator expected
        → rc=2
  [ 1 -eq 2 -zz ]           多余参数
        exitcodes.sh: line 36: [: syntax error: `-zz' unexpected
        → rc=2
```

> ★ 看最后两条的报错原文：`test: -zz: binary operator expected` 和
> `[: syntax error: '-zz' unexpected`。这些**看起来像语法错误**的信息，其实来自 `test`/`[` 这两个
> 程序在**运行时**的抱怨（所以它带的是 `脚本名: 行号:` 前缀，而不是 bash 解析期的报错）。
> 区分这两种错误是第 8.9 节的主题。

`exit` 自己的取值域（每一行都是一个**子 shell**，所以 `exit` 死掉的是子进程，demo 继续跑）：

```
──── D. exit 的取值域：只保留低 8 位（mod 256）
  ( exit 0 )                         →         rc=0
  ( exit 7 )                         →         rc=7
  ( exit 255 )                       →         rc=255
  ( exit 256 )                       →         rc=0
  ( exit 300 )                       →         rc=44
  ( exit 511 )                       →         rc=255
  ( exit 999 )                       →         rc=231
  ( exit -1 )                        →         rc=255
  ( exit abc )                       → exitcodes.sh: line 84: exit: abc: numeric argument required
        rc=2
```

`exit 300 → 44`、`exit 999 → 231`、`exit -1 → 255`，全部是 mod 256 的结果；
`exit abc` 不是数字，bash 直接给 **2**（这是 bash 自己的用法错误码，不是命令的返回码）。

126 / 127 这两个数字，新手几乎每次都记反：

```
──── E. 126 家族（找到但执行不了）与 127（根本没找到）
  nosuch_program_03（不存在）   → exitcodes.sh: line 84: nosuch_program_03: command not found
        rc=127
  ./ec_dir（是目录）            → exitcodes.sh: line 84: ./ec_dir: Is a directory
        rc=126
  ./ec_noexec.sh（没有 +x）      → exitcodes.sh: line 84: ./ec_noexec.sh: Permission denied
        rc=126
  ./ec_bad.sh（shebang 指错）    → exitcodes.sh: ./ec_bad.sh: /bin/no_such_interp_03: bad interpreter: No such file or directory
        rc=126
  bash ec_noexec.sh（读就行）   → 我其实能跑
        rc=0
```

**127 = 这个名字在 PATH 里根本找不到；126 = 找到了，但执行不了**（是目录、没 +x、shebang 指向的解释器不存在）。
倒数第二条特别说明问题：同一个文件，`./ec_noexec.sh` 报 `Permission denied`（126），
而 `bash ec_noexec.sh` 正常打印内容 —— 因为 bash 只需要**能读**这个文件。

被信号杀死时，退出码 = `128 + 信号号`：

```
──── F. 被信号杀死：退出码 = 128 + signum（反查用 kill -l）
  kill -HUP   signum=1   退出码=129  128+signum=129  反查名=HUP
  kill -INT   signum=2   退出码=130  128+signum=130  反查名=INT
  kill -KILL  signum=9   退出码=137  128+signum=137  反查名=KILL
  kill -SEGV  signum=11  退出码=139  128+signum=139  反查名=SEGV
  kill -PIPE  signum=13  退出码=141  128+signum=141  反查名=PIPE
  kill -USR1  signum=10  退出码=138  128+signum=138  反查名=USR1
  kill -TERM  signum=15  退出码=143  128+signum=143  反查名=TERM
```

这一列的用处在第 10 节：看到 137 你应当立刻反推出 9（SIGKILL），看到 141 是 13（SIGPIPE），
看到 130 是你自己按了 Ctrl-C。`kill -l` 提供反查表；第 12 节还会看到管道里某段被信号杀死时，
只有 `PIPESTATUS` 留得住这件事。

最后一组把「非 0 没有大小」钉死 —— 同样是 1，含义天差地别：

```
──── G. 同样是非 0，含义完全不同（没有「大小」可言）
  false                     约定俗成的 1        rc=1
  grep -q zzz ec_a.txt      没找到 → 1          rc=1
  diff -q ec_a.txt ec_b.txt 不一样 → 1          rc=1
  grep alpha ec_MISSING.txt 干不了活 → 2       rc=2
  被 SIGKILL(9) 杀死 →                    rc=137（=128+9）
  ( exit 200 )              自己定的 200         rc=200
  ↑ grep -c 打印的「0」是正确答案，退出码却是 1：输出对 ≠ 退出码 0
[退出码 0]
```

---

## 8. `[ ]` 是一个程序：空格是语法，参数个数决定解释

这是本章最容易返工的一节，所以每一小段都有实测。

### 8.1 它的身份：程序，而且是两个

```
════ 1. `[` 的程序身份 ════
  $ type -a [
      [ is a shell builtin
      [ is /usr/bin/[
      [ is /bin/[
      ↪ rc=0

  $ command -v [; echo "↑ 只打印一个 [ ：命令名解析到的是内建，所以不带路径"
      [
      ↑ 只打印一个 [ ：命令名解析到的是内建，所以不带路径
      ↪ rc=0

  $ env /usr/bin/[ x = x ]; echo "env 强制调用外部那份 [ → rc=$?"
      env 强制调用外部那份 [ → rc=0
      ↪ rc=0

  $ type -a test
      test is a shell builtin
      test is /usr/bin/test
      test is /bin/test
      ↪ rc=0

  $ type -a "[["
      [[ is a shell keyword
      ↪ rc=0

  $ ls -l /usr/bin/[
      lrwxrwxrwx 1 root root 28 Mar 31  2026 /usr/bin/[ -> ../lib/cargo/bin/coreutils/[
      ↪ rc=0

  $ /usr/bin/[ x = x ]; echo "外部 [ 也能跑 → rc=$?"
      外部 [ 也能跑 → rc=0
      ↪ rc=0

  $ /usr/bin/[ -v HOME ]; echo "外部 [ 支持 -v 吗 → rc=$?"
      [: extra argument 'HOME'
      外部 [ 支持 -v 吗 → rc=2
      ↪ rc=0

  $ /usr/bin/[ a == a ]; echo "外部 [ 支持 == 吗 → rc=$?"
      外部 [ 支持 == 吗 → rc=0
      ↪ rc=0

  $ /usr/bin/[ 010 -eq 10 ]; echo "外部 [ 怎么读 010 → rc=$?"
      外部 [ 怎么读 010 → rc=0
      ↪ rc=0

  $ /usr/bin/[ x; echo "外部 [ 少 ] → rc=$?"
      [: missing ']'
      外部 [ 少 ] → rc=2
      ↪ rc=0
```

七个要点：

1. `type -a [` 报了**两份**：内建 + `/usr/bin/[`（后者又是 uutils coreutils 的符号链接）。
2. `command -v [` 只打印一个 `[` —— 内建优先，所以没有路径。
3. `type -a "[["` 说的是 `shell keyword`：**`[[` 不是命令**，这是后面一切差别的根源。
4. 外部那份 `/usr/bin/[` 能独立跑（`env` 强制调用它，返回 0）。
5. 外部那份**不认 `-v`**：`[: extra argument 'HOME'`（rc=2）。所以 `-v` 只能用在内建 `[` 里；
   脚本用 `#!/usr/bin/env bash` 才安全。
6. 外部那份**认 `==`**（rc=0）、**也把 `010 -eq 10` 判成 0**：这台机器的 uutils 实现和 bash 内建
   在这两点上行为一致（别处不一定，见「本章没演/没验证」）。
7. 外部那份少写 `]` 时报 `[: missing ']'` rc=2 —— 再次证明 `]` 是**参数**，不是语法括号。

### 8.2 空格就是语法：少一个空格 = 换一条命令

`[` 是程序，那它的名字就必须**自己成一个词**。这条规则的 consequences 全部在这里：

```
════ 2. 空格就是语法（少一个空格 = 换一条命令）════
  $ [ -z x ]; echo "[ -z x ] 正常写法 → rc=$?"
      [ -z x ] 正常写法 → rc=1
      ↪ rc=0

  $ [-z x]; echo "上面报完错，这一行照样跑 → rc=$?"
      bash: line 1: [-z: command not found
      上面报完错，这一行照样跑 → rc=127
      ↪ rc=0

  $ [ -z x]; echo "同上 → rc=$?"
      bash: line 1: [: missing `]'
      同上 → rc=2
      ↪ rc=0

  $ x=y; [ "$x"=y ]; echo "粘成一个参数 [y=y] → rc=$?"
      粘成一个参数 [y=y] → rc=0
      ↪ rc=0

  $ x=y; [ "$x" =y ]; echo "= 右边少空格（其实还是两个词）→ rc=$?"
      bash: line 1: [: y: unary operator expected
      = 右边少空格（其实还是两个词）→ rc=2
      ↪ rc=0

  $ x=y; [x = y]; echo "左边少空格 → rc=$?"
      bash: line 1: [x: command not found
      左边少空格 → rc=127
      ↪ rc=0
```

逐条拆开：

| 你写的 | bash 分成几个词 | 实际执行的命令 | 结果 |
|---|---|---|---|
| `[ -z x ]` | 4 | `[`，参数 `-z` `x` `]` | 正常，rc=1（x 非空） |
| `[-z x]` | 2 | `[-z`（找不到这个命令） | `command not found`，rc=**127** |
| `[ -z x]` | 2 | `[`，参数 `-z` `x]` | `[: missing ']'`，rc=**2** |
| `[ "$x"=y ]` | 2 | `[`，参数 `y=y` | **单个参数 → 非空即真**，rc=**0**（最阴的一条） |
| `[ "$x" =y ]` | 3 | `[`，参数 `y` `=y` | `[: y: unary operator expected`，rc=**2** |
| `[x = y]` | 3 | `x`（找不到这个命令） | rc=**127** |

> ★ 第四行是本章最贵的一条：**它不报错**。`[ "$x"=y ]` 因为少了一个空格，
> 变成「一个非空参数」，于是**永远为真**。你以为是字符串比较，其实它连比较都没做。
> 报错的坑好查（有原文），不报错的坑难查（只表现为「这个分支怎么每次都进」）。

### 8.3 参数个数决定 `[ ]` 怎么解释同一串字

`[`（以及外部的 `test`）不看语法树，它**按参数个数切换解释模式**。这台机器实测：

```
════ 3. 参数个数决定 [ ] 怎么解释同一串字 ════
  $ [ x ]; echo "1 个参数：非空字符串就是真 → rc=$?"
      1 个参数：非空字符串就是真 → rc=0
      ↪ rc=0

  $ [ "" ]; echo "1 个参数：空串 → rc=$?"
      1 个参数：空串 → rc=1
      ↪ rc=0

  $ [ 0 ]; echo "1 个参数：0 被当字符串看 → rc=$?"
      1 个参数：0 被当字符串看 → rc=0
      ↪ rc=0

  $ [ -n ]; echo "1 个参数：-n 被当成普通字符串 → rc=$?"
      1 个参数：-n 被当成普通字符串 → rc=0
      ↪ rc=0

  $ [ -z ]; echo "1 个参数：-z 同理 → rc=$?"
      1 个参数：-z 同理 → rc=0
      ↪ rc=0

  $ [ -v ]; echo "1 个参数：-v 同理 → rc=$?"
      1 个参数：-v 同理 → rc=0
      ↪ rc=0

  $ [ ! x ]; echo "2 个参数 → rc=$?"
      2 个参数 → rc=1
      ↪ rc=0

  $ [ a = b ]; echo "3 个参数：三元比较 → rc=$?"
      3 个参数：三元比较 → rc=1
      ↪ rc=0

  $ [ a b c ]; echo "3 个参数：中间那个不是运算符 → rc=$?"
      bash: line 1: [: b: binary operator expected
      3 个参数：中间那个不是运算符 → rc=2
      ↪ rc=0

  $ [ a b c d ]; echo "4 个参数 → rc=$?"
      bash: line 1: [: too many arguments
      4 个参数 → rc=2
      ↪ rc=0

  $ [ a -a b ]; echo "3 个参数：-a 是「与」 → rc=$?"
      3 个参数：-a 是「与」 → rc=0
      ↪ rc=0

  $ [ a -a b -a c ]; echo "5 个参数：POSIX 只规定到 4 → rc=$?"
      5 个参数：POSIX 只规定到 4 → rc=0
      ↪ rc=0
```

规则（本机能对上的都列在这里）：

| `[` 收到的参数个数（不含 `[` 和 `]`） | 解释方式 | 实测例子 |
|---|---|---|
| 0 | `[ ]` → 恒真 | （见 8.4：`[ $x ]` x 为空时就是这条） |
| 1 | 单参数形式：**字符串非空即为真** | `[ x ]`→0、`[ "" ]`→1、`[ 0 ]`→**0**、`[ -n ]`→0、`[ -v ]`→0 |
| 2 | `! 表达式` 或一元运算符 | `[ ! x ]`→1（x 非空 → 取反为假） |
| 3 | 三元比较 `a op b`，或一元+`]` | `[ a = b ]`→1、`[ a b c ]`→`b: binary operator expected` rc=2 |
| 4 | 只允许 `[ a op b ]` 的变形 | `[ a b c d ]`→`too many arguments` rc=2 |
| 5+ | bash 允许 `-a`/`-o` 继续串 | `[ a -a b -a c ]`→0（POSIX 只规定到 4 个参数，5 个以上行为未定义） |

三条最要命的：

1. **`[ 0 ]` 是真**（rc=0）。因为单参数走「字符串非空」规则，`0` 是非空字符串。
   你以为写 `[ $n ]` 是在判「n 不等于 0」，其实只有在 n 是**空**时才为假。
   想按数字判要写 `[ "$n" -ne 0 ]`，或者 `(( n ))`。
2. **`[ -n ]` 是真**（rc=0）。`x` 忘了加引号且为空时，`[ -n $x ]` 就退化成 `[ -n ]` —— 于是「空串」被判成「非空」。
   这正是 8.4 的第一条事故。
3. `[ a -a b ]` 里 `-a` 是「与」，`a`、`b` 各自又按单参数规则判真 —— 所以整条是 0。

### 8.4 经典事故现场：变量没加引号

同一个语句 `[ $x = a ]`，只换 `x` 的值，得到三种完全不同的结局：

```
════ 4. 变量没加引号：同一个语句，x 取三种值 ════
  $ x="";    [ $x = a ]; echo "x 为空 → rc=$?"
      bash: line 1: [: =: unary operator expected
      x 为空 → rc=2
      ↪ rc=0

  $ x="b c"; [ $x = a ]; echo "x 含空格 → rc=$?"
      bash: line 1: [: too many arguments
      x 含空格 → rc=2
      ↪ rc=0

  $ x="abc"; [ $x = a ]; echo "x 是单个词（不报错，只是判假）→ rc=$?"
      x 是单个词（不报错，只是判假）→ rc=1
      ↪ rc=0

  $ x="-n";  [ $x = a ]; echo "x 以 - 开头 → rc=$?"
      x 以 - 开头 → rc=1
      ↪ rc=0

  $ x="";    [ $x ]; echo "[ \$x ] x 为空 → rc=$?"
      [ $x ] x 为空 → rc=1
      ↪ rc=0

  $ x="b c"; [ $x ]; echo "[ \$x ] x 含空格 → rc=$?"
      bash: line 1: [: b: unary operator expected
      [ $x ] x 含空格 → rc=2
      ↪ rc=0

  $ x="";    [ -n $x ]; echo "[ -n \$x ] x 为空 → rc=$?"
      [ -n $x ] x 为空 → rc=0
      ↪ rc=0

  $ x="有值"; [ -n $x ]; echo "[ -n \$x ] x 有值 → rc=$?"
      [ -n $x ] x 有值 → rc=0
      ↪ rc=0

  $ x="a b"; [ -z $x ]; echo "[ -z \$x ] x 含空格 → rc=$?"
      bash: line 1: [: a: binary operator expected
      [ -z $x ] x 含空格 → rc=2
      ↪ rc=0

  $ x="a b"; [ -z "$x" ]; echo "加引号的正确写法 → rc=$?"
      加引号的正确写法 → rc=1
      ↪ rc=0
```

**事故 1：`x` 是空串。**

* 错法原文：`[ $x = a ]`
* 展开后：`[ = a ]` —— 于是 `[` 收到两个参数 `=` 和 `a`，它按「一元运算符 + 操作数」解释，
  结果 `=` 不是合法的一元运算符，报 **`[: =: unary operator expected`**，rc=**2**。
* 对法：`[ "$x" = a ]` → 三个参数，正常比较，rc=1。
* 为什么：拆词（第 1 层）把空变量变成了「什么都没有」，参数个数从 3 掉到 2，解释规则就换了。

**事故 2：`x` 里带空格（`b c`）。**

* 错法原文：`[ $x = a ]`
* 展开后：`[ b c = a ]` —— 4 个参数，报 **`[: too many arguments`**，rc=**2**。
* 对法与为什么：同事故 1。

**事故 3：`x` 恰好是 `-n`。**

* 错法原文：`[ $x = a ]`，x=`-n`
* 展开后：`[ -n = a ]` —— 三个参数，`-n` 被当一元运算符吃掉，`=` 和 `a` 剩两个……
  本机实测：**不报错**，rc=1。这类「值刚好长成运算符」的情况，输出比报错更难查。
  （所以老代码里会看到 `[ "x$opt" = x ]` 这种加前缀的土办法。）

**事故 4：`[ $x ]`（老写法，判断「变量是否有值」）。**

* `x=""` → `[ ]` → 0 个参数 → 恒假，rc=1。**看起来对了**。
* `x="b c"` → `[ b c ]` → 2 个参数 → **`[: b: unary operator expected`**，rc=2。
* 对法：`[ -n "$x" ]`，或者 `[[ -n $x ]]`、`[[ $x ]]`。

**事故 5：`[ -z $x ]`。**

* `x=""` → `[ -z ]` → 单参数 `-z` → **真**（rc=0，「它是空」判对了，但是靠意外判对的）。
* `x="a b"` → `[ -z a b ]` → 报 **`[: a: binary operator expected`**，rc=2。
* 而 `[ -n $x ]` 在 x 为空时是 `[ -n ]` → **rc=0，把空判成非空**（这一条最危险，因为它把「空」判成「有值」）。

**事故 6：`x="a b"` 时 `[ -z "$x" ]` 返回 rc=1。**

对的：加了引号之后它是长度为 3 的字符串 `a b`，`-z` 判它非空 → 假。**这是全部五条里唯一「加了引号就正确」的示范。**

### 8.5 报错归报错，`if` 整条还是 0

这是「为什么你 `2>/dev/null` 之后永远查不出来」的现场：

```
════ 5. 报错归报错：if 整条还是 0，set -e 抓不到 ════
  $ x=""; if [ $x = a ]; then echo 走then; else echo 走else; fi; echo "整条 if 的 rc=$?"
      bash: line 1: [: =: unary operator expected
      走else
      整条 if 的 rc=0
      ↪ rc=0

  $ bash -e -c "x=; [ \$x = a ]; echo 报错之后脚本还在跑"
      bash: line 1: [: =: unary operator expected
      ↪ rc=2

  $ bash -e -c "x=; if [ \$x = a ]; then echo t; fi; echo if 之后还活着"
      bash: line 1: [: =: unary operator expected
      if 之后还活着
      ↪ rc=0
```

* 第 1 段：`[ $x = a ]` 报 `unary operator expected`（rc=2），但它在 `if` 的**条件位**，
  于是走 `else`，**整条 `if` 语句的退出码是 0**。脚本继续，谁也看不见那个 2。
* 第 2 段：同样的语句裸在 `bash -e` 里 —— **脚本当场终止，rc=2**。因为 `set -e` 管的是「非条件位的命令」。
* 第 3 段：`if` 里的失败被 `set -e` **豁免**，所以 `if` 之后那行 `echo` 照样执行。
  这一条是第 11 节 `!` 与 `set -e` 互相拆台的同一套规则。

### 8.6 `-eq` vs `=`：带前导零的惊喜

`-eq` 比较**整数**，`=` 比较**字符串**。而 `[ ]` 的整数解释**不是八进制**：

```
════ 6. 数字 -eq vs 字符串 =：带前导零的惊喜 ════
  $ [ 010 -eq 10 ]; echo "[ 010 -eq 10 ] → rc=$?"
      [ 010 -eq 10 ] → rc=0
      ↪ rc=0

  $ [ 010 -eq 8 ];  echo "[ 010 -eq 8 ] → rc=$?"
      [ 010 -eq 8 ] → rc=1
      ↪ rc=0

  $ [ 010 = 10 ];   echo "[ 010 = 10 ] → rc=$?"
      [ 010 = 10 ] → rc=1
      ↪ rc=0

  $ [ 010 = 010 ];  echo "[ 010 = 010 ] → rc=$?"
      [ 010 = 010 ] → rc=0
      ↪ rc=0

  $ (( 010 == 8 )); echo "(( 010 == 8 )) → rc=$?"
      (( 010 == 8 )) → rc=0
      ↪ rc=0

  $ echo "\$((010)) = $((010))"
      $((010)) = 8
      ↪ rc=0

  $ printf "printf %%d 010 → %d\n" 010
      printf %d 010 → 8
      ↪ rc=0

  $ [ 008 -eq 8 ];  echo "[ 008 -eq 8 ]（真八进制里没 8）→ rc=$?"
      [ 008 -eq 8 ]（真八进制里没 8）→ rc=0
      ↪ rc=0

  $ [ 0x10 -eq 16 ]; echo "[ 0x10 -eq 16 ] → rc=$?"
      bash: line 1: [: 0x10: integer expected
      [ 0x10 -eq 16 ] → rc=2
      ↪ rc=0

  $ (( 0x10 == 16 )); echo "(( 0x10 == 16 )) → rc=$?"
      (( 0x10 == 16 )) → rc=0
      ↪ rc=0

  $ [ abc -eq 1 ];  echo "[ abc -eq 1 ] → rc=$?"
      bash: line 1: [: abc: integer expected
      [ abc -eq 1 ] → rc=2
      ↪ rc=0

  $ [[ abc -eq 1 ]]; echo "[[ abc -eq 1 ]] → rc=$?"
      [[ abc -eq 1 ]] → rc=1
      ↪ rc=0

  $ [ 10 -gt 9 ]; [[ 10 -gt 9 ]]; echo "两版 -gt 都真 → $?"
      两版 -gt 都真 → 0
      ↪ rc=0
```

八条实测结论，逐条对上：

1. `[ 010 -eq 10 ]` → **rc=0（真）**。`[` 的 `-eq` 按十进制读 `010` → 10，所以 10 == 10。
2. `[ 010 -eq 8 ]` → rc=1（假）。想按八进制要写 `(( 010 == 8 ))` → **rc=0**（`(( ))` 才认八进制前缀），
   `$((010))` 打印 **8**，`printf %d 010` 也打印 **8**。
3. `[ 010 = 10 ]` → rc=1（字符串比较，`010` ≠ `10`）；`[ 010 = 010 ]` → rc=0。
4. `[ 008 -eq 8 ]` → rc=0。**真八进制里根本没有 8**，`[` 却照样把它读成十进制 8 ——
   这正好说明 `[ ]` 根本没在做八进制解析。
5. `[ 0x10 -eq 16 ]` → **rc=2**，原文 `[: 0x10: integer expected`；`(( 0x10 == 16 ))` → rc=0。
   十六进制只有算术上下文认。
6. `[ abc -eq 1 ]` → rc=2（`[: abc: integer expected`）；`[[ abc -eq 1 ]]` → **rc=1，且不报错**。
   这是两者又一处行为分歧：`[[ ]]` 里比较失败只给「假」，`[ ]` 给「用法错误 2」。
7. `[ 10 -gt 9 ]` 和 `[[ 10 -gt 9 ]]` 都是真。
8. 记住这张小表：

| 比较什么 | 用哪个 | 反例后果 |
|---|---|---|
| 字符串 | `=`（`[[ ]]` 里也可用 `==`） | `-eq` 遇到非数字 → rc=2 报错 |
| 整数 | `-eq -ne -lt -le -gt -ge` | `=` 会按字符串比：`[ 010 = 10 ]` → 假 |
| 八进制/十六进制数字 | `(( ))` / `$(( ))` | `[ 010 -eq 8 ]` → 假 |
| 版本号这种「看着像数字」的串 | 字符串比较或正则 | 别用 `-eq`（`1.2` 会 rc=2） |

### 8.7 `=` 与 `==`：`[ ]` 里比字面，`[[ ]]` 里右边是模式

```
════ 7. `=` 与 `==`：[ ] 里比字面，[[ ]] 里右边是模式 ════
  $ [ abc = abc ];  echo "[ abc = abc ] → rc=$?"
      [ abc = abc ] → rc=0
      ↪ rc=0

  $ [ abc == abc ]; echo "[ abc == abc ] → rc=$?"
      [ abc == abc ] → rc=0
      ↪ rc=0

  $ [ abc = a* ];   echo "[ abc = a* ]（[ ] 不做模式匹配）→ rc=$?"
      [ abc = a* ]（[ ] 不做模式匹配）→ rc=1
      ↪ rc=0

  $ [ abc == a* ];  echo "[ abc == a* ]（bash 的 [ ] 也不匹配）→ rc=$?"
      [ abc == a* ]（bash 的 [ ] 也不匹配）→ rc=1
      ↪ rc=0

  $ [[ abc == a* ]]; echo "[[ abc == a* ]]（右边是 glob）→ rc=$?"
      [[ abc == a* ]]（右边是 glob）→ rc=0
      ↪ rc=0

  $ [[ abc == "a*" ]]; echo "右边加引号 → 变字面比较 → rc=$?"
      右边加引号 → 变字面比较 → rc=1
      ↪ rc=0

  $ [[ abc = a* ]];   echo "[[ abc = a* ]]（= 与 == 等价）→ rc=$?"
      [[ abc = a* ]]（= 与 == 等价）→ rc=0
      ↪ rc=0

  $ dash -c "[ abc == abc ]; echo dash的== → rc=\$?"
      dash: 1: [: abc: unexpected operator
      dash的== → rc=2
      ↪ rc=0

  $ dash -c "[ abc = abc ]; echo dash的= → rc=\$?"
      dash的= → rc=0
      ↪ rc=0
```

四条结论：

1. `[ abc = abc ]` 和 `[ abc == abc ]` 在 **bash 的内建 `[`** 里都返回 0 —— `==` 在 `[ ]` 里只是 `=` 的别名。
2. 但 **`[` 永远不做模式匹配**：`[ abc = a* ]` → rc=1，`[ abc == a* ]` → rc=1。
   而 `[[ abc == a* ]]` → rc=**0**。右边是不是模式，取决于你在哪一边。
3. `[[ abc == "a*" ]]` → rc=1：**引号把模式变成字面量**（第 10 节 `=~` 上会再撞一次）。
4. `[ abc == abc ]` 换到 dash 里 → **`dash: 1: [: abc: unexpected operator`，rc=2**。
   `==` 不是 POSIX。`[ ]` 里请写 `=`。

### 8.8 `[ ]` 里的 `<`：不转义是重定向，转义才是比较

这一段是「`[` 是程序」最直观的证据：**`<` 在 shell 的第 1 层就被当成重定向吃掉了**：

```
════ 8. `[ ]` 里的 `<`：不转义是重定向，转义才是比较，且不受 locale 影响 ════
  $ [ a < b ]; echo "[ a < b ] 没转义 → rc=$?"
      bash: line 1: b: No such file or directory
      [ a < b ] 没转义 → rc=1
      ↪ rc=0

  $ [ a \< b ]; echo "[ a \< b ] 转义后 → rc=$?"
      [ a \< b ] 转义后 → rc=0
      ↪ rc=0

  $ [[ a < b ]]; echo "[[ a < b ]] 不用转义 → rc=$?"
      [[ a < b ]] 不用转义 → rc=0
      ↪ rc=0

  $ LC_ALL=C bash -c "[[ b < B ]]"
      ↪ rc=1

  $ LC_ALL=zh_CN.UTF-8 bash -c "[[ b < B ]]"
      ↪ rc=0

  $ LC_ALL=C bash -c "[ b \< B ]"
      ↪ rc=1

  $ LC_ALL=zh_CN.UTF-8 bash -c "[ b \< B ]"
      ↪ rc=1

  $ LC_ALL=C bash -c "[ Z \< a ]"
      ↪ rc=0

  $ LC_ALL=zh_CN.UTF-8 bash -c "[ Z \< a ]"
      ↪ rc=0

  $ LC_ALL=C bash -c "[[ Z < a ]]"
      ↪ rc=0

  $ LC_ALL=zh_CN.UTF-8 bash -c "[[ Z < a ]]"
      ↪ rc=1

  $ echo "排序证据："; printf "Z\na\n" | LC_ALL=C sort | tr "\n" " "; echo; printf "Z\na\n" | LC_ALL=zh_CN.UTF-8 sort | tr "\n" " "; echo
      排序证据：
      Z a 
      a Z 
      ↪ rc=0
```

* `[ a < b ]` → 报错原文 `b: No such file or directory`，rc=1。
  展开后它其实是 `[ a ]`（标准输入重定向自不存在的文件 `b`）—— `[` 收到的参数少了一个，
  而那个「文件」根本没打开成功。
* `[ a \< b ]` → rc=0。转义（或写成 `"<"`）之后 `<` 才作为**参数**传进去，成为字符串比较运算符。
* `[[ a < b ]]` → rc=0，不用转义（`[[ ]]` 里面第 1 层不做重定向解析）。
* 最后那 8 行是**字典序的 locale 矩阵**，这是 `[[ ]]` 和 `[ ]` 最大的行为分歧：

| 表达式 | `LC_ALL=C` | `LC_ALL=zh_CN.UTF-8` | 用的是哪套序 |
|---|---|---|---|
| `[[ b < B ]]` | rc=1（假） | rc=**0（真）** | `[[ ]]` 调 libc 的 `strcoll` → **受 locale 影响** |
| `[[ Z < a ]]` | rc=0（真） | rc=**1（假）** | 同上 |
| `[ b \< B ]` | rc=1 | **rc=1（不变）** | `[ ]` 的 `\<` 按**字节/码位**比 → 不受 locale 影响 |
| `[ Z \< a ]` | rc=0 | **rc=0（不变）** | 同上 |

紧跟的 `sort` 两行是同一件事的独立印证：`Z a`（C 序）vs `a Z`（中文序）。

> ★ 为什么这事重要：`[[ $a < $b ]]` 换台机器、换个 `LC_ALL` 就可能反过来。
> 需要稳定顺序时用 `[ "$a" \< "$b" ]`（字节序），或者在脚本开头钉住 `LC_ALL=C`（本章 demo 就是这么做的）。

### 8.9 `-a` / `-o` / `!` 的优先级：一题三写法三个答案

```
════ 9. -a / -o / ! 的优先级：一题三写法三个答案 ════
  $ a=1; b=9; c=9; [ "$a" = 1 -o "$b" = 2 -a "$c" = 3 ]; echo "不加括号（意图 (a或b)且c=假）→ rc=$?"
      不加括号（意图 (a或b)且c=假）→ rc=0
      ↪ rc=0

  $ a=1; b=9; c=9; [ \( "$a" = 1 -o "$b" = 2 \) -a "$c" = 3 ]; echo "加 \\( \\) 分组 → rc=$?"
      加 \( \) 分组 → rc=1
      ↪ rc=0

  $ a=1; b=9; c=9; [[ ( $a == 1 || $b == 2 ) && $c == 3 ]]; echo "[[ ]] 里括号无需转义 → rc=$?"
      [[ ]] 里括号无需转义 → rc=1
      ↪ rc=0

  $ a=1; b=9; c=9; [ "$a" = 1 -o \( "$b" = 2 -a "$c" = 3 \) ]; echo "把 -a 括起来 → rc=$?"
      把 -a 括起来 → rc=0
      ↪ rc=0

  $ x=""; y=""; [ ! -z "$x" -a -n "$y" ]; echo "意图 !(x空 且 y非空)=真 → rc=$?"
      意图 !(x空 且 y非空)=真 → rc=1
      ↪ rc=0

  $ x=""; y=""; [ ! \( -z "$x" -a -n "$y" \) ]; echo "显式分组 → rc=$?"
      显式分组 → rc=0
      ↪ rc=0

  $ x=""; y=""; [[ ! ( -z "$x" && -n "$y" ) ]]; echo "[[ ]] 版 → rc=$?"
      [[ ]] 版 → rc=0
      ↪ rc=0

  $ a=1; [ ( $a = 1 ) ]; echo "[ ] 里裸括号（不转义）→ rc=$?"
      bash: -c: line 1: syntax error near unexpected token `$a'
      bash: -c: line 1: `a=1; [ ( $a = 1 ) ]; echo "[ ] 里裸括号（不转义）→ rc=$?"'
      ↪ rc=2

  $ a=1; b=9; [[ $a == 1 && ( $b == 2 || $b == 9 ) ]]; echo "[[ ]] 里嵌套分组 → rc=$?"
      [[ ]] 里嵌套分组 → rc=0
      ↪ rc=0
```

同一句「(a=1 或 b=2) 且 c=3」，`a=1, b=9, c=9`：

* `[ "$a" = 1 -o "$b" = 2 -a "$c" = 3 ]` → **rc=0（真）**。你期望假。
  因为 `test` 的 `-a` 比 `-o` **结合更紧**：它实际算的是 `a=1 OR (b=2 AND c=3)` → 真。
* `[ \( "$a" = 1 -o "$b" = 2 \) -a "$c" = 3 ]` → **rc=1（假）**。加了 `\( \)` 才得到你要的意思。
* `[[ ( $a == 1 || $b == 2 ) && $c == 3 ]]` → **rc=1（假）**。同样结果，但不用转义、不用给每个变量加引号。

`[ ! -z "$x" -a -n "$y" ]` 是同一件事的第二种形态：意图是「!(x空 且 y非空)」= 真，
实测 **rc=1**；而显式分组 `[ ! \( -z "$x" -a -n "$y" \) ]` → **rc=0**。
`[[ ! ( -z "$x" && -n "$y" ) ]]` → **rc=0**。

最后两条裸括号的对比也别忘了：`[ ( $a = 1 ) ]`（不转义）→ bash **当场语法错误**，rc=2；
`[[ $a == 1 && ( $b == 2 || $b == 9 ) ]]` → rc=0，括号在 `[[ ]]` 里就是分组。

> ★ 结论表：**永远不要在 `[ ]` 里混用 `-a`/`-o`/`!`**。要么写成 `[[ ]]` 用 `&&`/`||`/`!`，
> 要么把每个子判断拆成独立的 `[[ ]]`，再用 `&&`/`||` 串起来（第 14 节给推荐写法）。
> POSIX 自己都说 `-a`/`-o` 的表达式「超出 4 个参数时行为未定义」。

### 8.10 `[[ ]]` 的错误是语法错误：会把脚本当场掐掉

这条决定了很多事故「为什么后面几行没跑」：

```
════ 10. [[ ]] 的错误是「语法错误」：会把脚本当场掐掉 ════
  $ printf "%s\n" "echo 第一行" "[[ 1 -eq ]]" "echo 第三行" > bad_double.sh; bash bad_double.sh; echo "脚本 rc=$?"
      第一行
      bad_double.sh: line 2: unexpected argument `]]' to conditional binary operator
      bad_double.sh: line 2: syntax error near `]]'
      bad_double.sh: line 2: `[[ 1 -eq ]]'
      脚本 rc=2
      ↪ rc=0

  $ printf "%s\n" "echo 第一行" "[ 1 -eq 2 -zz ]" "echo 第三行" > bad_single.sh; bash bad_single.sh; echo "脚本 rc=$?"
      第一行
      bad_single.sh: line 2: [: syntax error: `-zz' unexpected
      第三行
      脚本 rc=0
      ↪ rc=0
[退出码 0]
# 再从 demo 这边补三条「[ 就是个命令」的直接证据（同一个判断三种写法）：
```

* `[[ 1 -eq ]]` 这种写法是 **bash 解析期**的错误 → 整份脚本在第 2 行终止，脚本退出码 **2**，
  第三行的 `echo` **根本没执行**。
* `[ 1 -eq 2 -zz ]` 是 `test` 程序**运行时**的抱怨 → 打印错误，脚本继续，第三行照跑，脚本退出码 **0**。

> ★ 这就是为什么「加了 `[[ ]]` 的脚本改一半语法就跑不动」，而 `[ ]` 的脚本会「报个错继续跑，
> 把半成品写到磁盘上」。第 12 节和第 14 节会给出对应的兜底手段（`bash -n` 只解析不执行，第 01 章讲过）。

demo 末尾再补三条「`[` 就是命令」的直接证据（三种写法同一个判断）：

```
$ "[" -z "" ]
[退出码 0]
# ↑ 上一条说明 [ 就是命令名：加引号也能调用，但它仍然是「以 ] 结尾的一串参数」。

$ [ -z "" ]; echo "[ 的退出码就是 test 的退出码：$?"
[ 的退出码就是 test 的退出码：0
[退出码 0]

$ test -z ""; echo "写成 test 一字不差：$?"
写成 test 一字不差：0
[退出码 0]
```

第一条把 `[` 用引号包起来照样能调用，就是因为它**是个命令名**。
第三条写成 `test` 一字不差 —— `[` 就是 `test` 的别名，`test` 结尾不需要 `]`，`[` 结尾必须有 `]`。

---

## 9. 文件测试运算符全表：每个都用真对象测

`-e -f -d -L -r -w -x -s -p -S -b -c -u -g -k -t -nt -ot -ef -O -G` 这一堆开关，
手册通常只写「`-f` 判断是不是文件」。真正会咬人的是三件事，全部实测在下面：

1. 它们**几乎全都跟随符号链接**（只有 `-L` 不跟）。断链的符号链接 `-e`/`-f` 都是**假**。
2. `-r/-w/-x` 判的是「**我这个进程能不能**」，不是「权限位里有没有那一格」。
3. `-nt/-ot` 只看 mtime，`-ef` 比的是「同一个文件」（设备号 + inode），跟内容无关。

`file_tests.sh` 先在沙箱 `objs/` 里把对象全造出来（`ln -s`、`ln`、`mkfifo`、
`python3` 绑一个 AF_UNIX socket 文件、`chmod 4755/2755/1777/2750/000/400/755/644`、
`sleep 2` 造出两个差 2 秒的 mtime），然后逐个测。开头那段**对象清单**是 `stat -c` 打的
（用 `stat` 而不是 `ls -l`，因为 `ls -l` 会把属主名印到屏幕上）：
```
$ bash file_tests.sh

──── 0. 沙箱里造出来的对象（stat 证据）
  -rw-rw-r-- 2 大小=12  inode=218849  mtime=2026-10-05 13:44:21.282510198 +0800  hello.txt
  -rw-rw-r-- 1 大小=0  inode=218850  mtime=2026-10-05 13:44:21.282510198 +0800  empty.txt
  drwxrwxr-x 2 大小=40  inode=218852  mtime=2026-10-05 13:44:21.282923730 +0800  adir
  lrwxrwxrwx 1 大小=5  inode=218855  mtime=2026-10-05 13:44:21.284923875 +0800  link_ok
  lrwxrwxrwx 1 大小=14  inode=218856  mtime=2026-10-05 13:44:21.286924020 +0800  link_broken
  -rw-rw-r-- 2 大小=12  inode=218849  mtime=2026-10-05 13:44:21.282510198 +0800  hardlink
  prw-rw-r-- 1 大小=0  inode=218857  mtime=2026-10-05 13:44:21.289924237 +0800  afifo
  srwxrwxr-x 1 大小=0  inode=218858  mtime=2026-10-05 13:44:21.303925249 +0800  asock
  -rwsr-xr-x 1 大小=0  inode=218859  mtime=2026-10-05 13:44:21.309925683 +0800  suid_file
  -rwxr-sr-x 1 大小=0  inode=218860  mtime=2026-10-05 13:44:21.313925972 +0800  sgid_file
  drwxrwxrwt 2 大小=40  inode=218854  mtime=2026-10-05 13:44:21.282923730 +0800  stickydir
  drwxr-s--- 2 大小=40  inode=218853  mtime=2026-10-05 13:44:21.282923730 +0800  giddir
  ---------- 1 大小=2  inode=218861  mtime=2026-10-05 13:44:21.318926334 +0800  no_read
  -r-------- 1 大小=2  inode=218862  mtime=2026-10-05 13:44:21.320869698 +0800  ro_file
  -rwxr-xr-x 1 大小=2  inode=218863  mtime=2026-10-05 13:44:21.321926551 +0800  exe_file
  -rw-r--r-- 1 大小=2  inode=218864  mtime=2026-10-05 13:44:21.322926623 +0800  no_exe
  -rw-rw-r-- 1 大小=0  inode=218865  mtime=2026-10-05 13:44:21.328927057 +0800  old_file
  -rw-rw-r-- 1 大小=0  inode=218866  mtime=2026-10-05 13:44:23.333071982 +0800  new_file
  crw-rw-rw- 1 大小=0  inode=5  mtime=2026-10-05 10:07:45.300061652 +0800  /dev/null
  brw-rw---- 1 大小=0  inode=125  mtime=2026-10-05 10:07:45.619339476 +0800  /dev/loop0
```

### 9.1 存在性与类型：谁跟随符号链接

```
──── 1. 存在性 / 类型（注意谁跟随符号链接）
  [ -e plain ]                             rc=0
  [ -e link_ok ]                           rc=0
  [ -e link_broken ]                       rc=1
  [ -f plain ]                             rc=0
  [ -f adir ]                              rc=1
  [ -f link_ok ]                           rc=0
  [ -f link_broken ]                       rc=1
  [ -d adir ]                              rc=0
  [ -d plain ]                             rc=1
  [ -L link_ok ]                           rc=0
  [ -L link_broken ]                       rc=0
  [ -L plain ]                             rc=1
  [ -e hello.txt ]                         rc=0
```

* `-e link_broken` → **rc=1**。断链「存在」这件事取决于用哪个开关：`-e` 问的是**目标**。
* `-L link_broken` → **rc=0**。`-L` 问的是**链接本身**。所以「这个符号链接是不是坏的」=
  `[ -L "$f" ] && [ ! -e "$f" ]`。
* `-f adir` → 1、`-d plain` → 1：`-f` 只认普通文件，目录要 `-d`。
* `-f link_ok` → 0：跟随链接之后它是普通文件。

### 9.2 权限三兄弟：判的是「我这个进程能不能」

```
──── 2. 权限三兄弟：判的是「我这个进程能不能」
  [ -r hello.txt ]                         rc=0
  [ -r no_read ]                           rc=1
  [ -w no_read ]                           rc=1
  [ -r ro_file ]                           rc=0
  [ -w ro_file ]                           rc=1
  [ -x exe_file ]                          rc=0
  [ -x no_exe ]                            rc=1
  [ -x adir ]                              rc=0
  [ -r /etc/shadow ]                       rc=1
```

`-r no_read`（权限 000）→ 1，`-w no_read` → 1；`-r ro_file`（400）→ 0 但 `-w` → 1；
`-x exe_file`（755）→ 0，`-x no_exe`（644）→ 1；`-x adir`（目录 775）→ 0 —— 目录的 `-x` 是「能不能进」。
最后一条 `-r /etc/shadow` → **1**：我们不是 root，即使权限位显示 `root shadow` 可读也一样。
**这套判断走的是 `access(2)`**，所以它会看真实 uid/gid，也可能被 ACL 或 capabilities 影响 ——
它给你的答案是「我能不能」，不是「chmod 写了什么」。

### 9.3 大小、时间戳、同一个文件

```
──── 3. 大小 / 时间戳 / 同一文件
  [ -s hello.txt ]                         rc=0
  [ -s empty.txt ]                         rc=1
  [ -s oneline.txt ]                       rc=0
  [ new_file -nt old_file ]                rc=0
  [ old_file -nt new_file ]                rc=1
  [ old_file -ot new_file ]                rc=0
  [ plain -nt plain ]                      rc=1
  [ hello.txt -ef hardlink ]               rc=0
  [ hello.txt -ef link_ok ]                rc=1
  [ link_ok -ef plain ]                    rc=0
  [ hello.txt -ef plain ]                  rc=1
  [ -O hello.txt ]                         rc=0
  [ -G hello.txt ]                         rc=0

──── 3b. -nt 的精度实测：两条紧挨着的 touch 也分得出新旧
  stat 证据：t_a mtime=1791179063.397076600   t_b mtime=1791179063.399076700
  [ t_b -nt t_a ]                          rc=0
  [ t_a -nt t_b ]                          rc=1
  stat 证据：t_c mtime=1791179063.405077200   t_d mtime=1791179063.405077200（一模一样）
  [ t_d -nt t_c ]                          rc=1
  [ t_d -ot t_c ]                          rc=1
  stat 证据：backdated mtime=1577836800.000000000（比 hello.txt 旧）
  [ backdated -nt hello.txt ]              rc=1
```

* `-s` 判「**字节数 > 0**」：`hello.txt`(12B)→0，`empty.txt`(0B)→1，一行 `y\n` → 0。
  注意它跟「有没有内容」不同：一个只含空行的文件 `-s` 也是真。
* `-nt/-ot`：`new_file -nt old_file` → 0，反过来 → 1；`old_file -ot new_file` → 0。
  `plain -nt plain` → **1**（同一个文件不比新，时间相等就是「不更新」）。
* `-ef`：`hello.txt -ef hardlink` → **0**（同一 inode），
  `hello.txt -ef link_ok` → 1（`link_ok` 指向 `plain`，是另一个 inode），
  `link_ok -ef plain` → **0**（跟随链接后是同一个 inode）。
  这就是 `-ef` 的用途：判断「这两个路径是不是同一个东西」，而不是「内容是否相同」。
* `-O/-G` hello.txt → 都是 0：属主和属组就是我当前进程（组也是我的补充组时）。

**3b 是 `-nt` 的精度实测**（很多手册说「`-nt` 只能比秒」，这台机器上不是这样）：

两条紧挨着的 `touch`（间隔约 4 毫秒）就分得出新旧：`t_b -nt t_a` → 0，`t_a -nt t_b` → 1；
用 `touch -r` 把 mtime 造成**逐纳秒相同**的两个文件：`t_d -nt t_c` → 1 且 `t_d -ot t_c` → 1（**两个都假**，
时间相等时 `-nt`/`-ot` 双双为假）；把 mtime 改到 2020 年再比：`backdated -nt hello.txt` → 1。

> ★ 结论：本机的 `-nt` 精度是**纳秒级**（stat 显示的 `1791175861.119514700` 这种 9 位小数），
> 但**跨文件系统/跨机器不要依赖这个**：NFS 只保证秒级、有些挂载点会截断时间戳。
> 需要可靠「谁新」时，比较 `stat -c %Y` 的数值或者用构建系统。

### 9.4 特殊文件类型

```
──── 4. 特殊文件类型（每一行都是沙箱里的真对象或 /dev 里的真设备）
  [ -p afifo ]                             rc=0
  [ -p hello.txt ]                         rc=1
  [ -S asock ]                             rc=0
  [ -S afifo ]                             rc=1
  [ -c /dev/null ]                         rc=0
  [ -b /dev/null ]                         rc=1
  [ -b /dev/loop0 ]                        rc=0
  [ -c /dev/loop0 ]                        rc=1
  [ -L /dev/stdin ]                        rc=0
  [ -e /dev/stdin ]                        rc=0
  [ -c /dev/stdin ]                        rc=0
```

`-p afifo` → 0（命名管道），`-S asock` → 0（unix socket），
`-c /dev/null` → 0（字符设备）、`-b /dev/null` → 1，反过来 `-b /dev/loop0` → 0、`-c /dev/loop0` → 1。
这三类是「管道/socket/设备」，普通脚本里最常用的是 `-p`（判断有没有人用管道喂 stdin）。
最后三条关于 `/dev/stdin`：`-L` → 0（它是个符号链接，指向 `/proc/self/fd/0`）、`-e` → 0、`-c` → 0
—— 但这只是**这一次**的 stdin 状态，第 9.6 小节会换三种 stdin 再测一遍，答案完全不同。

### 9.5 三个「位」：setuid / setgid / sticky

```
──── 5. 三个「位」：setuid / setgid / sticky
  [ -u suid_file ]                         rc=0
  [ -u plain ]                             rc=1
  [ -g sgid_file ]                         rc=0
  [ -g giddir ]                            rc=0
  [ -g plain ]                             rc=1
  [ -k stickydir ]                         rc=0
  [ -k plain ]                             rc=1
  [ -k /tmp ]                              rc=0
```

`-u suid_file`（4755）→ 0，`-g sgid_file`（2755）→ 0，`-g giddir`（2750 目录）→ 0（目录也能有 setgid，
作用是「新建文件的组继承目录的组」），`-k stickydir`（1777）→ 0，`-k /tmp` → **0**（`/tmp` 就是 sticky 位）。
`-u plain`、`-g plain`、`-k plain` 都是 1。

> ★ `suid_file` 只是**造出来判位**的，本章从不执行它；`chmod 4755` 也只发生在沙箱里。

### 9.6 `-t`：这个 fd 是不是终端（脚本里几乎永远是假）

```
──── 6. -t：这个 fd 是不是终端（脚本里几乎永远是假）
  [ -t 1 ]                                 rc=1
  [ -t 0 ]                                 rc=1
  [ -t 2 ]                                 rc=1
```

三条全是 1 —— 因为 demo 的输出被接到了管道（`tee`）里，标准输入也不是终端。
把同一句放进**真 pty** 里跑，答案立刻反过来：

```
$ script -qec "[ -t 1 ] && echo \"-t 1 在 pty 里为真（标准输出是终端）\" || echo \"假\"; [ -t 0 ] && echo \"-t 0 在 pty 里为真（标准输入是终端）\" || echo \"假\"; true" /dev/null
-t 1 在 pty 里为真（标准输出是终端）
-t 0 在 pty 里为真（标准输入是终端）
[退出码 0]
# ↑ 这就是为什么脚本里 `if [ -t 1 ]; then 上色; fi` 有用：被重定向时它是假，不污染文件。
#   本 demo 开头那句 `if [ -t 1 ]` 用的正是这条规则，所以捕获文件里没有任何转义序列。
# /dev/stdin 到底是哪种类型，取决于它**接到什么**上 —— 写死一个答案就是错的。
# 造一个探针脚本，分别在 stdin=/dev/null、stdin=管道、stdin=普通文件 三种情况下跑：
```

这就是脚本里 `if [ -t 1 ]; then 上色; fi` 有效的原理：
**`-t` 判的是「这个 fd 接到什么」，跟脚本长什么样无关。**
demo.sh 开头那句 `if [ -t 1 ]` 用的正是这条规则，所以捕获文件里一个转义序列都没有（你可以拿
`grep -c $'\033' bash03.txt` 自己验）。

### 9.7 单参数形式的三个高频误用

```
──── 7. 顺带三个高频误用：单参数形式的陷阱
  （现在 xe 是空串）
  [ -n $xe ]                               rc=0
  [ -n "$xe" ]                             rc=1
  [ -z $xe ]                               rc=0
  [ -z "$xe" ]                             rc=0
  （现在 xe 是「有值」）
  [ -n $xe ]                               rc=0
  [ -z $xe ]                               rc=1
  （单参数的通用规则：只要那一个词非空就是真）
  [ 0 ]                                    rc=0
  [ "0" ]                                  rc=0
  [ -o ]                                   rc=0
  [ -v ]                                   rc=0
[退出码 0]
# 上面第 6 组里 [ -t 1 ] 全是假，因为 demo 的输出被接到了管道（tee）里。
# 把同一句放进**真 pty** 再看一次，才知道 -t 判的是「这个 fd 接到什么」而不是「脚本长什么样」：
```

这八行是 8.3 那条「单参数 = 非空即真」规则的完整版：

* `x=""` 时 `[ -n $x ]` → **rc=0**（把空判成非空，最危险）；`[ -n "$x" ]` → rc=1（正确）。
* `x=""` 时 `[ -z $x ]` → **rc=0**（判对了，但是**靠意外判对的**：`[ -z ]` 是单参数真）。
  `x="有值"` 时 `[ -z $x ]` → rc=1。看起来两个都对 —— 直到 `x="a b"`（8.4 事故 5 当场 rc=2）。
* `[ 0 ]` → 0、`[ "0" ]` → 0、`[ -o ]` → 0、`[ -v ]` → 0：单参数只看「这个词是不是空串」。
  所以 `[ "$x" ]` 这种「省事的写法」判的其实是「**非空**」，不是「真」。

### 9.8 `/dev/stdin` 到底是什么类型：换三种 stdin 再测

探针脚本 `stdin_probe.sh` 内容就 7 行（`for op in -p -c -f -r -s; do [ $op /dev/stdin ]; ...`），
分别让 stdin 接 `/dev/null`、接管道、接普通文件：

```
$ echo "──── 情况1：stdin 接到 /dev/null"; bash stdin_probe.sh < /dev/null
──── 情况1：stdin 接到 /dev/null
  [ -p /dev/stdin ] rc=1
  [ -c /dev/stdin ] rc=0
  [ -f /dev/stdin ] rc=1
  [ -r /dev/stdin ] rc=0
  [ -s /dev/stdin ] rc=1
  [ -t 0 ] rc=1
[退出码 0]
```

```
$ echo "──── 情况2：stdin 接到管道"; echo hi | bash stdin_probe.sh
──── 情况2：stdin 接到管道
  [ -p /dev/stdin ] rc=0
  [ -c /dev/stdin ] rc=1
  [ -f /dev/stdin ] rc=1
  [ -r /dev/stdin ] rc=0
  [ -s /dev/stdin ] rc=1
  [ -t 0 ] rc=1
[退出码 0]
```

```
$ echo "──── 情况3：stdin 接到普通文件"; bash stdin_probe.sh < stdin_probe.sh
──── 情况3：stdin 接到普通文件
  [ -p /dev/stdin ] rc=1
  [ -c /dev/stdin ] rc=1
  [ -f /dev/stdin ] rc=0
  [ -r /dev/stdin ] rc=0
  [ -s /dev/stdin ] rc=0
  [ -t 0 ] rc=1
[退出码 0]
```

| 表达式 | stdin=/dev/null | stdin=管道 | stdin=普通文件 |
|---|---|---|---|
| `-p /dev/stdin` | 1 | **0** | 1 |
| `-c /dev/stdin` | **0** | 1 | 1 |
| `-f /dev/stdin` | 1 | 1 | **0** |
| `-r /dev/stdin` | 0 | 0 | 0 |
| `-s /dev/stdin` | 1 | 1 | **0** |
| `-t 0` | 1 | 1 | 1 |

> ★ 结论：`/dev/stdin` 是个符号链接，**它的类型跟着 stdin 实际接的东西变**。
> 「判断有没有人用管道喂我」的正确写法是 `[ -p /dev/stdin ]`，
> 而不是背一句「`/dev/stdin` 是字符设备」—— 上一表里它只在 stdin 接 `/dev/null` 时才是 `-c`。

---

## 10. `[[ ]]` 为什么存在：同一份变量喂给两边

规则只有一句：**`[ ]` 的参数在送进程序之前就已经被改过；`[[ ]]` 里面不改。**
所以 `[[ ]]` 不是「语法糖」，它是把第 1 层的两类事故（拆词、文件名展开）**整类消灭掉**。

先看目录条件。demo 在沙箱里造了 `g1`（一个文件）和 `g2`（两个文件）：

```
$ mkdir -p g1 g2; touch g1/onlyfile g2/aaa g2/bbb; ls g1; ls g2
onlyfile
aaa
bbb
[退出码 0]
# ① 变量里是「a b」：[ ] 拆成两个词，语句变形；[[ ]] 原样当一个词。
```

### 10.1 变量里有空格

```
$ x="a b"; [ $x = "a b" ]; echo "[ ] 未加引号 rc=$?"
bash: line 1: [: too many arguments
[ ] 未加引号 rc=2
[退出码 0]

$ x="a b"; [ "$x" = "a b" ]; echo "[ ] 加引号   rc=$?"
[ ] 加引号   rc=0
[退出码 0]

$ x="a b"; [[ $x == "a b" ]]; echo "[[ ]] 不加引号 rc=$?"
[[ ]] 不加引号 rc=0
[退出码 0]
# ② 变量里是 glob 字符 *：最阴的一种 —— 不报错，答案是「对」，但判的根本不是你以为的东西。
```

同一个 `x="a b"`：`[ ]` 未加引号 → `[: too many arguments` rc=2；加引号 → rc=0；
`[[ ]]` **不加引号也一样 rc=0**。这就是「在 `[[ ]]` 里忘了加引号不会死，在 `[ ]` 里忘了加引号会死」。

### 10.2 变量里是 glob 字符 `*`（最阴的一种）

```
$ cd g1; x="*"; [ $x = onlyfile ]; echo "目录里只有一个文件时 [ ] rc=$?"
目录里只有一个文件时 [ ] rc=0
[退出码 0]

$ cd g1; x="*"; [[ $x == onlyfile ]]; echo "同一个目录里 [[ ]] rc=$?"
同一个目录里 [[ ]] rc=1
[退出码 0]

$ cd g2; x="*"; [ $x = "*" ]; echo "目录里有两个文件时 [ ] rc=$?"
bash: line 1: [: too many arguments
目录里有两个文件时 [ ] rc=2
[退出码 0]

$ cd g2; x="*"; [[ $x == "*" ]]; echo "同样是两个文件的目录 [[ ]] rc=$?"
同样是两个文件的目录 [[ ]] rc=0
[退出码 0]
# ↑ 结论：用 `[ ]` 时漏一个引号，判据可能**悄悄变真**；`[[ ]]` 把这一类整类消灭掉了。
# ③ `[[ ]]` 的独门武器：&&/|| 直写、== 走 glob、=~ 走正则、-v 判断变量是否已设。
```

* 目录里**只有一个文件**时：`[ $x = onlyfile ]` → **rc=0，为真**。
  因为 `*` 被展开成了 `onlyfile`，于是这句变成 `[ onlyfile = onlyfile ]`。
  **它不报错、答案还是「对」，但判断的根本不是你写的那个比较。**
* 同一目录、同一变量：`[[ $x == onlyfile ]]` → rc=1。`*` 就是字符串 `*`。
* 目录里有**两个文件**时：`[ $x = "*" ]` → `[: too many arguments` rc=2（`*` 展开成两个词）。
* 同一情况：`[[ $x == "*" ]]` → rc=0。

> ★ 这一条值得单独背：**`[ ]` 里的未引用变量如果内容长得像 glob，它会静默地变成「不报错的错误答案」**。
> 报错的坑你有机会发现，静默变真的坑你可能查一晚上。

### 10.3 `[[ ]]` 的四个独门武器

```
$ a=1; b=2; [[ $a == 1 && $b == 2 ]]; echo "[[ ]] 里直写 && rc=$?"
[[ ]] 里直写 && rc=0
[退出码 0]

$ a=1; b=2; [ $a = 1 && $b = 2 ]; echo "[ ] 里直写 && rc=$?（上一行就是报错原文）"
bash: line 1: [: missing `]'
[ ] 里直写 && rc=2（上一行就是报错原文）
[退出码 0]
```

`[ ]` 里直写 `&&` 的报错原文值得背下来：**`[: missing ']'`** ——
`&&` 被当成了参数（其实 `&&` 之后的部分整个被 shell 拆成另一条命令），
所以 `[` 只收到 `$a = 1`，看不到 `]`。

```
$ f=v1.2.3; [[ $f == v[0-9]* ]]; echo "[[ == 走 glob rc=$?"
[[ == 走 glob rc=0
[退出码 0]

$ f=v1.2.3; [ $f = v[0-9]* ]; echo "[ ] 里同样的写法 rc=$?（右边是字面量，而且还要先做文件名展开）"
[ ] 里同样的写法 rc=1（右边是字面量，而且还要先做文件名展开）
[退出码 0]
```

`[[ == ` 的右边是 **glob 模式**：`v[0-9]*` 命中 `v1.2.3`。
`[ ]` 里同样的字面量返回 rc=1：右边是**字面量**（而且还要先做文件名展开，这里恰好展开后还是它自己）。

```
$ v=1; [[ -v v ]]; echo "[[ -v v ]]（v 已设）rc=$?"
[[ -v v ]]（v 已设）rc=0
[退出码 0]

$ unset -v never_set_03; [[ -v never_set_03 ]]; echo "[[ -v 未设的 ]] rc=$?"
[[ -v 未设的 ]] rc=1
[退出码 0]

$ unset -v never_set_03; [ -v never_set_03 ]; echo "[ ] 里 -v rc=$?（bash 的内建 [ 支持，POSIX/dash 不支持，见第 8 节）"
[ ] 里 -v rc=1（bash 的内建 [ 支持，POSIX/dash 不支持，见第 8 节）
[退出码 0]

$ arr=(7 8); [[ -v arr[1] ]]; echo "[[ -v arr[1] ]] rc=$?"
[[ -v arr[1] ]] rc=0
[退出码 0]

$ arr=(7 8); [[ -v arr[5] ]]; echo "[[ -v arr[5] ]] rc=$?"
[[ -v arr[5] ]] rc=1
[退出码 0]
# ④ < 与 > 的字典序受 locale 影响，这是 [[ ]] 和 [ ] 的 \< 最大的分歧点。
#    必须把判断写进**文件**，再用带 locale 的子 bash 去跑它；在命令行上给 [[ 前面贴 LC_ALL= 是无效的写法。
```

`-v` 判「变量**有没有被设过**」，这和「值是不是空」是两件事：
`[[ -v v ]]`（v=1）→ 0；`[[ -v never_set_03 ]]` → 1；bash 的内建 `[ -v ... ]` → 同样 1（但 dash 不认，见第 13 节）。
数组元素也能问：`[[ -v arr[1] ]]` → 0、`[[ -v arr[5] ]]` → 1。
（这个开关在数据结构上很实用：区分「键不存在」和「键存在但值为空」。）

### 10.4 `<`/`>` 的字典序受 locale 影响（`[[ ]]` 独有）

必须把判断**写进文件**，再让带 locale 的子 bash 去跑它（在 `[[` 前面贴 `LC_ALL=` 是无效写法，
因为那样会让 `[[` 不再被当成保留字）：

```
$ LC_ALL=C bash cmp_bB.sh; echo "[[ b < B ]] 在 LC_ALL=C 下 rc=$?"
[[ b < B ]] 在 LC_ALL=C 下 rc=1
[退出码 0]

$ LC_ALL=zh_CN.UTF-8 bash cmp_bB.sh; echo "[[ b < B ]] 在中文 locale 下 rc=$?"
[[ b < B ]] 在中文 locale 下 rc=0
[退出码 0]

$ LC_ALL=C bash cmp_Za.sh; echo "[[ Z < a ]] 在 LC_ALL=C 下 rc=$?"
[[ Z < a ]] 在 LC_ALL=C 下 rc=0
[退出码 0]

$ LC_ALL=zh_CN.UTF-8 bash cmp_Za.sh; echo "[[ Z < a ]] 在中文 locale 下 rc=$?"
[[ Z < a ]] 在中文 locale 下 rc=1
[退出码 0]

$ LC_ALL=zh_CN.UTF-8 bash cmp_sq.sh; echo "[ ] 里 \\< 在中文 locale 下 rc=$?（不受 locale 影响；完整矩阵见 bracket_spaces.sh 第 8 组）"
[ ] 里 \< 在中文 locale 下 rc=0（不受 locale 影响；完整矩阵见 bracket_spaces.sh 第 8 组）
[退出码 0]
# 再用 sort 印证 locale 真的改排序（第 02 章讲过展开与引号，这里是它对「比较结果」的影响）：
```

四条实测：

* `[[ b < B ]]`：`LC_ALL=C` → **1（假）**；中文 locale → **0（真）**。
* `[[ Z < a ]]`：`LC_ALL=C` → **0（真）**；中文 locale → **1（假）**。
* `[ Z \< a ]`（中文 locale）→ **0**，和 C 下一样 —— `[ ]` 的 `\<` 走字节序，不受 locale 影响。
* 后面 `sort` 两行是独立印证：**同一份输入，C 序是 `Z a`，中文序是 `a Z`**。

```
$ printf "Z\na\n" | LC_ALL=C sort | tr "\n" " "; echo "| 这是 C 排序"
Z a | 这是 C 排序
[退出码 0]

$ printf "Z\na\n" | LC_ALL=zh_CN.UTF-8 sort | tr "\n" " "; echo "| 这是中文排序"
a Z | 这是中文排序
[退出码 0]
# 正则的坑全部集中在「右边加了引号」这一件事上，见 regex_pitfalls.sh：
```

### 10.5 `=~` 正则：右半边**绝对不能加引号**

下面整块是 `regex_pitfalls.sh` 的输出（每条都带 `rc=`）：

```
$ bash regex_pitfalls.sh

════ 1. 不加引号 = 正则：捕获组进 BASH_REMATCH ════
  $ s="v1.2.3"
[[ $s =~ ^v([0-9]+)\.([0-9]+) ]]
echo "rc=$?  组0=${BASH_REMATCH[0]}  组1=${BASH_REMATCH[1]}  组2=${BASH_REMATCH[2]}"
echo "整个数组=[${BASH_REMATCH[*]}] 元素个数=${#BASH_REMATCH[@]}"
      rc=0  组0=v1.2  组1=1  组2=2
      整个数组=[v1.2 1 2] 元素个数=3
      ↪ rc=0
```

`BASH_REMATCH` 是这次匹配的全部真相：`组0=v1.2`（整段命中）、`组1=1`、`组2=2`，
数组 `[v1.2 1 2]`、元素个数 3。注意 `${BASH_REMATCH[@]}` 只在 `[[ =~ ]]` 之后有效，
中间隔任何一条命令都可能被下次匹配覆盖。

```
════ 2. 加引号 = 字面量：同一条正则，答案反过来 ════
  $ s="v1.2.3"
[[ $s =~ "^v([0-9]+)" ]]
echo "加引号版 rc=$?"
      加引号版 rc=1
      ↪ rc=0

  $ s="v1.2.3"
[[ $s =~ ^v([0-9]+) ]]
echo "不加引号版 rc=$?"
      不加引号版 rc=0
      ↪ rc=0

  $ [[ "axxb" =~ a.x ]];  echo "点号不加引号（任意字符）rc=$?"
      点号不加引号（任意字符）rc=0
      ↪ rc=0

  $ [[ "axxb" =~ "a.x" ]]; echo "点号加引号（字面点）rc=$?"
      点号加引号（字面点）rc=1
      ↪ rc=0

  $ [[ "a.c"  =~ "a.c" ]]; echo "字面 a.c 命中 a.c rc=$?"
      字面 a.c 命中 a.c rc=0
      ↪ rc=0
```

这一组是**本章最重要的引号实验**，两条同一条正则、答案相反：

| 写法 | 结果 | 为什么 |
|---|---|---|
| `[[ $s =~ ^v([0-9]+) ]]` | **rc=0** | 右边不加引号 → 按 ERE 解释（`(` `)` 是捕获组、`+` 是量词） |
| `[[ $s =~ "^v([0-9]+)" ]]` | **rc=1** | 右边加引号 → 变成**字面量**：要求串里真的出现 `^v([0-9]+)` 这几个字符 |
| `[[ "axxb" =~ a.x ]]` | rc=0 | `.` 是「任意一个字符」 |
| `[[ "axxb" =~ "a.x" ]]` | rc=1 | 引号版要求字面 `a.x`，而串里是 `axxb` |
| `[[ "a.c" =~ "a.c" ]]` | rc=0 | 只有当值里**真有那个点**时，引号版才对 |

```
════ 3. 模式里有空格：不加引号是语法错误 ════
  $ [[ "a b c" =~ a b c ]]; echo "不该走到这里"
      bash: -c: line 1: syntax error in conditional expression: unexpected token `b'
      bash: -c: line 1: syntax error near `b'
      bash: -c: line 1: `[[ "a b c" =~ a b c ]]; echo "不该走到这里"'
      ↪ rc=2

  $ [[ "a b c" =~ "a b c" ]]; echo "整体加引号 rc=$?"
      整体加引号 rc=0
      ↪ rc=0
```

模式里有空格时，**不加引号直接是语法错误**（rc=2）；把整个模式加引号反而对（rc=0）。
工程上的标准做法是**模式放进变量**、引用变量时不加引号：

```
  $ re="a b c"; [[ "a b c" =~ $re ]]; echo "模式放变量、引用变量时不加引号 rc=$?"
      模式放变量、引用变量时不加引号 rc=0
      ↪ rc=0
```

最后一行是推荐写法：`re="a b c"`，然后 `[[ $s =~ $re ]]`。
变量里的空格不会拆词（`[[ ]]` 里面不拆词），而变量的**内容**仍然按正则解释 —— 两全。

```
════ 4. 这是 ERE，不是 PCRE ════
  $ [[ "a1" =~ \d ]]; echo "\\d 匹配数字吗 rc=$?"
      \d 匹配数字吗 rc=1
      ↪ rc=0

  $ [[ "xd" =~ \d ]]; echo "那它到底匹配了什么 rc=$?"
      那它到底匹配了什么 rc=0
      ↪ rc=0

  $ [[ "a1" =~ [0-9] ]]; echo "正确写法：字符类 rc=$?"
      正确写法：字符类 rc=0
      ↪ rc=0

  $ [[ "ab" =~ ^[a-c]+$ ]]; echo "量词 + 锚定 rc=$?"
      量词 + 锚定 rc=0
      ↪ rc=0

  $ [[ "ABC" =~ [a-z] ]]; echo "默认区分大小写 rc=$?"
      默认区分大小写 rc=1
      ↪ rc=0
```

`[[ "a1" =~ \d ]]` → **rc=1**：bash 的 `=~` 是 **ERE**，没有 PCRE 的 `\d`。
`[[ "xd" =~ \d ]]` → **rc=0**：`\d` 被当成「字面量 d」，所以它匹配的是字母 d。
这两条一起看最好：`\d` 不是「永远不工作」，而是「**悄悄换了意思**」，这比报错危险得多。
正确写法是字符类 `[0-9]`（rc=0）、`^[a-c]+$`（rc=0）；另外默认**区分大小写**（`[[ ABC =~ [a-z] ]]` → rc=1，
要忽略大小写用 `shopt -s nocasematch`）。

```
════ 5. 不锚定 \$ 就是「找子串」 ════
  $ [[ "v1x" =~ ^v[0-9] ]]; echo "不写尾巴锚 rc=$?"
      不写尾巴锚 rc=0
      ↪ rc=0

  $ [[ "v1x" =~ ^v[0-9]$ ]]; echo "写了尾巴锚 rc=$?"
      写了尾巴锚 rc=1
      ↪ rc=0

  $ [[ "版本 v12 发布" =~ ^v[0-9]+$ ]]; echo "整串必须全是版本号 rc=$?"
      整串必须全是版本号 rc=1
      ↪ rc=0

  $ [[ "版本 v12 发布" =~ v[0-9]+ ]]; echo "只要求出现一次 rc=$?"
      只要求出现一次 rc=0
      ↪ rc=0
```

不锚定 `$` 就是**找子串**：`^v[0-9]` 对 `v1x` → rc=0，加了尾巴锚 `^v[0-9]$` → rc=1。
`版本 v12 发布` 用 `^v[0-9]+$` → 1、用 `v[0-9]+` → 0。
（做版本号校验时这一步最容易忘：`^v([0-9.]+)$` 要两头都锚住。）

```
════ 6. == 是 glob，=~ 是正则：同一个串两种答案 ════
  $ [[ "abc" == a*c ]]; echo "== 用 glob：* 是任意串 rc=$?"
      == 用 glob：* 是任意串 rc=0
      ↪ rc=0

  $ [[ "abc" =~ a.c ]]; echo "=~ 用正则：. 只顶一个字符 rc=$?"
      =~ 用正则：. 只顶一个字符 rc=0
      ↪ rc=0

  $ [[ "abbc" =~ a.c ]]; echo "abbc 对 a.c（要正好一个中间字符）rc=$?"
      abbc 对 a.c（要正好一个中间字符）rc=1
      ↪ rc=0

  $ [[ "abbc" =~ a.*c ]]; echo "正则里要 a.*c 才等价 rc=$?"
      正则里要 a.*c 才等价 rc=0
      ↪ rc=0

  $ f=app.log; [[ $f == *.log ]]; echo "按扩展名分派就用 == glob rc=$?"
      按扩展名分派就用 == glob rc=0
      ↪ rc=0

  $ f=app.log; [[ $f =~ \.log$ ]]; echo "换成正则要转义点并锚定 rc=$?"
      换成正则要转义点并锚定 rc=0
      ↪ rc=0
```

`==` 和 `=~` 的分工背这张表：

| 需求 | 用 | 注意 |
|---|---|---|
| 按扩展名/前缀分派 | `[[ $f == *.log ]]` → 0 | glob：`*` 吃任意长度，`?` 吃一个字符 |
| 同一需求写成正则 | `[[ $f =~ \.log$ ]]` → 0 | 点必须转义、必须锚 `$` |
| 数字/格式校验 | `[[ $v =~ ^v[0-9]+(\.[0-9]+)*$ ]]` | 要取片段就读 `BASH_REMATCH` |

`abbc` 对 `a.c` → **rc=1**（`.` 只顶一个字符），对 `a.*c` → 0（`.*` 才顶任意长度）。

```
════ 7. 引号是把刀：模式侧加了引号就不再是模式 ════
  $ [[ "aXb" == a*b ]];   echo "右边裸着写：* 是模式 rc=$?"
      右边裸着写：* 是模式 rc=0
      ↪ rc=0

  $ [[ "aXb" == "a*b" ]]; echo "右边加引号：* 是字面星号 rc=$?"
      右边加引号：* 是字面星号 rc=1
      ↪ rc=0

  $ [[ "a*b" == "a*b" ]]; echo "值真的叫 a*b 时才靠引号版 rc=$?"
      值真的叫 a*b 时才靠引号版 rc=0
      ↪ rc=0

  $ pat="a*b"; [[ "aXb" == $pat ]];   echo "模式存进变量、引用时不加引号 → 仍然是模式 rc=$?"
      模式存进变量、引用时不加引号 → 仍然是模式 rc=0
      ↪ rc=0

  $ pat="a*b"; [[ "aXb" == "$pat" ]]; echo "模式存进变量、引用时加引号 → 变成字面 rc=$?"
      模式存进变量、引用时加引号 → 变成字面 rc=1
      ↪ rc=0

  $ x="*"; [[ $x == "*" ]]; echo "[[ ]] 左边不做文件名展开：x 就还是一个星号 rc=$?"
      [[ ]] 左边不做文件名展开：x 就还是一个星号 rc=0
      ↪ rc=0
```

这六行把「引号是把刀」钉死：模式侧加引号 = 交出模式能力。
`x="*"; [[ $x == "*" ]]` → 0 是最后一块拼图：`[[ ]]` 的**左边**不做文件名展开，
所以 `x` 就还是那个星号字符本身。

```
════ 8. 括号：@( ) 可以，裸 ( ) 是语法错误 ════
  $ a=dog; [[ $a == @(cat|dog) ]]; echo "extglob @() 不用 shopt 就能用 rc=$?"
      extglob @() 不用 shopt 就能用 rc=0
      ↪ rc=0

  $ a=dog; [[ $a == (cat|dog) ]]; echo "裸括号：不该走到这里"
      bash: -c: line 1: unexpected argument `(' to conditional binary operator
      bash: -c: line 1: syntax error near `(c'
      bash: -c: line 1: `a=dog; [[ $a == (cat|dog) ]]; echo "裸括号：不该走到这里"'
      ↪ rc=2

  $ a=dog; shopt -s extglob; [[ $a == @(cat|dog) ]]; echo "显式开 extglob 之后 rc=$?"
      显式开 extglob 之后 rc=0
      ↪ rc=0

  $ [[ "aaa" == +(a) ]]; echo "+() 一个以上 rc=$?"
      +() 一个以上 rc=0
      ↪ rc=0

  $ [[ "" == ?(a) ]]; echo "?() 零或一个 rc=$?"
      ?() 零或一个 rc=0
      ↪ rc=0

  $ [[ "abc" =~ ^(a|b) ]]; echo "正则里的 ( ) 是捕获组 rc=$?"; echo "  BASH_REMATCH=[${BASH_REMATCH[*]}]"
      正则里的 ( ) 是捕获组 rc=0
        BASH_REMATCH=[a a]
      ↪ rc=0
```

括号的两副面孔：

* 在 `[[ == ` 后面，`@(cat|dog)`（extglob）**不用 `shopt -s extglob` 就能用** → rc=0；
  但**裸括号** `(cat|dog)` → 语法错误 rc=2（原文 `unexpected argument '(' to conditional binary operator`）。
* `+(a)`（一个以上）、`?(a)`（零或一个）都能用 → rc=0。
* 在 `[[ =~ ` 后面，`( )` 是**正则捕获组**：`^(a|b)` 命中，`BASH_REMATCH=[a a]`。

```
════ 9. 两种错误：解析期语法错误 vs 运行期正则错误 ════
  $ [[ 1 -eq ]]; echo "不该走到这里"
      bash: -c: line 1: unexpected argument `]]' to conditional binary operator
      bash: -c: line 1: syntax error near `;'
      bash: -c: line 1: `[[ 1 -eq ]]; echo "不该走到这里"'
      ↪ rc=2

  $ [[ "abc" =~ a[b ]]; echo "正则里 [ 不闭合 rc=$?"
      bash: line 1: [[: invalid regular expression `a[b': Unmatched [, [^, [:, [., or [=
      正则里 [ 不闭合 rc=2
      ↪ rc=0

  $ printf "%s\n" "echo 第一行" "[[ 1 -eq ]]" "echo 第三行" > bad_double.sh; bash bad_double.sh; echo "脚本 rc=$?"
      第一行
      bad_double.sh: line 2: unexpected argument `]]' to conditional binary operator
      bad_double.sh: line 2: syntax error near `]]'
      bad_double.sh: line 2: `[[ 1 -eq ]]'
      脚本 rc=2
      ↪ rc=0

  $ printf "%s\n" "echo 第一行" "[[ abc =~ a[b ]]" "echo 第三行（正则错误不中断脚本）" > bad_re.sh; bash bad_re.sh; echo "脚本 rc=$?"
      第一行
      bad_re.sh: line 2: [[: invalid regular expression `a[b': Unmatched [, [^, [:, [., or [=
      第三行（正则错误不中断脚本）
      脚本 rc=0
      ↪ rc=0
[退出码 0]

══════ 7. && || ! 与 if / case（and_or_trap.sh + case_dispatch.sh） ════════════════════════════════
# if 后面跟的是**任意命令**，不是「布尔表达式」；case 的模式是 glob 不是正则；
# A && B || C 不是 if-else；! 会让 set -e 的保护失效；三种终止符 ;; ;& ;;& 走法不同。
```

两类错误终于对上号了（对照 8.10）：

| 错误 | 阶段 | 原文 | 脚本会不会继续 | 退出码 |
|---|---|---|---|---|
| `[[ 1 -eq ]]` | **解析期**语法错误 | `unexpected argument ']]' to conditional binary operator` | **不继续** | 2 |
| `[[ abc =~ a[b ]]` | **运行期**正则错误 | `[[: invalid regular expression 'a[b': Unmatched [ …` | 继续 | 1（判断为假） |
| `[ 1 -eq 2 -zz ]` | 运行期 `test` 用法错误 | `[: syntax error: '-zz' unexpected` | 继续 | 2（但整条脚本 0） |

---

## 11. `&& || !` 与 `if` / `case`

### 11.1 `if` 后面跟的是**任意命令**

新手最大的概念错位就在这里：`if` 不接「条件表达式」，它接一条**命令**，
然后判断「它的退出码是不是 0」。下面六条都是命令：`grep`、不存在的文件、`bash -c "exit 3"`、
函数、`:`。

```
$ bash and_or_trap.sh

════ 1. if 后面跟的是任意命令（条件 = 退出码 = 0 吗）════
  $ printf "name=alice\nport=8080\n" > rp_conf.ini
if grep -q "name=alice" rp_conf.ini; then echo "grep 当条件：找到了 → then"; else echo "没找到 → else"; fi
      grep 当条件：找到了 → then
      ↪ rc=0

  $ printf "name=alice\n" > rp_conf.ini
if grep -q "name=bob" rp_conf.ini; then echo then; else echo "bob 不在配置里 → else（grep 返回 1）"; fi
      bob 不在配置里 → else（grep 返回 1）
      ↪ rc=0

  $ printf "name=alice\n" > rp_conf.ini
if grep -q alice rp_MISSING.ini; then echo then; else echo "grep 返回 2（干不了活）也算条件不成立 → else"; fi
      grep: rp_MISSING.ini: No such file or directory
      grep 返回 2（干不了活）也算条件不成立 → else
      ↪ rc=0

  $ if bash -c "exit 3"; then echo then; else echo "子命令退出码 3 ≠ 0 → else；整条 if 的 rc=$?"; fi
      子命令退出码 3 ≠ 0 → else；整条 if 的 rc=3
      ↪ rc=0

  $ ok(){ return 0; }; if ok; then echo "函数也能当条件"; fi
      函数也能当条件
      ↪ rc=0

  $ if :; then echo "连冒号（什么都不做）都能当条件，它的退出码是 0"; fi
      连冒号（什么都不做）都能当条件，它的退出码是 0
      ↪ rc=0
```

四条要点：

1. `if grep -q ...` 才是「用返回值判断」的正规写法 —— 不经过 `$?`，也就没人能改写它。
2. `grep` 找不到**文件**时返回 2，`if` 一样走 `else`：**在 if 眼里 2 和 1 没区别**。
3. `if bash -c "exit 3"` 走 `else`；注意打印出来的 `$?` 是 **3**：
   `else` 分支里的 `$?` 就是刚才那个条件的退出码（这是本章唯一能「拿到」条件码的窗口）。
4. 函数和 `:` 都能当条件：条件位判断的是**它们的返回值**。

### 11.2 经典反例：`A && B || C` **不是** if-else

```
════ 2. 经典反例：A && B || C 里 B 失败也会跑 C ════
  $ A(){ echo "  A 成功"; return 0; }
B(){ echo "  B 开始干活…然后失败"; return 1; }
C(){ echo "  C（兜底）被调用"; }
echo "── 写法一：A && B || C"
A && B || C
echo "── 写法二：等价于 if-else 吗？看 if 版"
if A; then B; else C; fi
echo "  ↑ if 版里 C 没有被调用，&& || 版里 C 被调用：两者不等价"
      ── 写法一：A && B || C
        A 成功
        B 开始干活…然后失败
        C（兜底）被调用
      ── 写法二：等价于 if-else 吗？看 if 版
        A 成功
        B 开始干活…然后失败
        ↑ if 版里 C 没有被调用，&& || 版里 C 被调用：两者不等价
      ↪ rc=0
```

同一段 `A`（成功）、`B`（失败）：

* `A && B || C` → C **跑了**。因为链子是从左往右算的：`A&&B` 整体为假（B 失败），于是 `|| C` 成立。
* `if A; then B; else C; fi` → C **没跑**。`else` 只在 `A` 失败时执行。

> ★ 结论：只有当 **B 保证成功**（或者 B 失败也无所谓）时，`A && B || C` 才勉强等价于 if-else。
> 你没法保证这一点，所以**别把它当 if-else 用**。这一条能解释很多「兜底逻辑莫名其妙执行了」的 bug。

### 11.3 同一件事两种写法，在 `set -e` 下结局不同

```
════ 3. 同一件事，两种写法在 set -e 下的结局 ════
  $ cat > ao_chain.sh <<'SCRIPT'
set -e
A(){ return 0; }
B(){ return 1; }
C(){ echo "  C 跑了"; }
A && B || C
echo "  && || 版：脚本还活着（整条链最后是 C 的 0）"
SCRIPT
bash ao_chain.sh; echo "  ao_chain.sh 退出码=$?"
        C 跑了
        && || 版：脚本还活着（整条链最后是 C 的 0）
        ao_chain.sh 退出码=0
      ↪ rc=0

  $ cat > ao_if.sh <<'SCRIPT'
set -e
A(){ return 0; }
B(){ return 1; }
C(){ echo "  C 跑了"; }
if A; then B; else C; fi
echo "  if 版：不该走到这里"
SCRIPT
bash ao_if.sh; echo "  ao_if.sh 退出码=$?（B 失败直接终止脚本）"
        ao_if.sh 退出码=1（B 失败直接终止脚本）
      ↪ rc=0
```

* `&& ||` 版：`set -e` 下**活到最后**，脚本退出码 0 —— 因为 `A && B` 站在 `||` 左边，属于条件位，被豁免。
* `if` 版：`B` 失败**直接终止脚本**，退出码 1 —— `then` 分支不是条件位，`set -e` 照常生效。

> ★ 这条很多人记反：`set -e` 对 `A && B || C` 里的 `B` **不管**，对 `if…then B; fi` 里的 `B` **管**。
> 所以「加了 set -e 就更安全」这句话，取决于你写成哪种形式。

### 11.4 `&&` 和 `||` 是从左往右依次执行，没有优先级

```
════ 4. && 和 || 是从左往右依次执行，没有优先级 ════
  $ true || false && echo "X 被打印（说明 && 不比 || 优先，纯左结合）"
      X 被打印（说明 && 不比 || 优先，纯左结合）
      ↪ rc=0

  $ false && true || echo "Y 被打印"
      Y 被打印
      ↪ rc=0

  $ echo 1 && echo 2 || echo 3 && echo 4
      1
      2
      4
      ↪ rc=0

  $ echo 1 && false || echo 3 && echo 4
      1
      3
      4
      ↪ rc=0

  $ false && echo A || echo B && echo C
      B
      C
      ↪ rc=0
```

`true || false && echo X` 打印了 X。如果 `&&` 优先级更高，就会先算 `false && echo X`（不打印），
再让 `true || 不打印` → 什么都不输出。**实测打印了** → 说明没有优先级，只有从左往右：
先算 `true || false` 得到「真」（`false` 被短路跳过），链子的当前值是 0，于是 `&& echo X` 执行。

后三条链子是同一件事的三个变体，输出逐字对比：

| 写法 | 打印 | 说明 |
|---|---|---|
| `echo 1 && echo 2 \|\| echo 3 && echo 4` | 1、2、4 | 全左结合 |
| `echo 1 && false \|\| echo 3 && echo 4` | 1、3、4 | `false` 让下一段换成 `\|\|` 分支 |
| `false && echo A \|\| echo B && echo C` | B、C | A 没跑（短路），B 跑了，链子又为真 → C 也跑 |

### 11.5 `!` 取反：判据对了，但 `set -e` 的保护没了

```
════ 5. ! 取反：判据对了，但 set -e 的保护没了 ════
  $ printf "%s\n" "set -e" "false" "echo 不该走到这里" > plain_e.sh; bash plain_e.sh; echo "  plain_e.sh 退出码=$?"
        plain_e.sh 退出码=1
      ↪ rc=0

  $ printf "%s\n" "set -e" "! true" "echo 被 ! 保护：还活着" "echo 到这里退出码=\$?" > bang_e2.sh; bash bang_e2.sh; echo "  bang_e2.sh 退出码=$?"
      被 ! 保护：还活着
      到这里退出码=0
        bang_e2.sh 退出码=0
      ↪ rc=0

  $ bash -ec "if ! grep -q zzz rp_conf.ini; then echo 活着; fi; echo if 里用 ! 也照样不受 set -e 影响"
      活着
      if 里用 ! 也照样不受 set -e 影响
      ↪ rc=0

  $ cat > bang_e.sh <<'SCRIPT'
set -e
worker(){ echo "  worker 第 1 步"; false; echo "  worker 第 3 步（本来不该执行）"; }
if ! worker; then echo "  走了 ! 分支"; fi
echo "  脚本活着，退出码=$?"
SCRIPT
bash bang_e.sh; echo "  bang_e.sh 退出码=$?"
        worker 第 1 步
        worker 第 3 步（本来不该执行）
        脚本活着，退出码=0
        bang_e.sh 退出码=0
      ↪ rc=0

  $ cat > nobang_e.sh <<'SCRIPT'
set -e
worker(){ echo "  worker 第 1 步"; false; echo "  worker 第 3 步"; }
if worker; then echo "  走了 then"; fi
echo "  不该走到这里"
SCRIPT
bash nobang_e.sh; echo "  nobang_e.sh 退出码=$?"
        worker 第 1 步
        worker 第 3 步
        走了 then
        不该走到这里
        nobang_e.sh 退出码=0
      ↪ rc=0

  $ if ! grep -q zzz rp_MISSING.ini 2>/dev/null; then echo "! 之后：grep 失败被取反 → 走 then（注意 grep 的 2 也被吃掉了）"; fi
      ! 之后：grep 失败被取反 → 走 then（注意 grep 的 2 也被吃掉了）
      ↪ rc=0
```

这五条一起读才有意义：

1. 裸 `false` 在 `set -e` 下 → 脚本终止，rc=1。
2. `! true` 在 `set -e` 下 → **「! 的右边」被当成条件位**，所以豁免，脚本继续。
3. `if ! grep -q …` → 也一样继续（`if` 的条件位 + `!` 的条件位，双重豁免）。
4. `bang_e.sh`：`if ! worker` 里 `worker` 函数中间那条 `false` **没能终止脚本**，
   函数一路跑到 `第 3 步`，整个脚本退出码 0。**这是 `set -e` 最常见的失效现场**：
   一旦某个函数被放进条件位（`if f`、`! f`、`f && …`、`f || …`），它**函数体内部**的每一条
   都不再受 `set -e` 保护。
5. `nobang_e.sh` 更反直觉：`if worker`（不带 `!`）同样让函数跑到第 3 步，
   而且因为最后一行 `echo` 成功，**`worker` 返回 0**，于是走了 `then`，脚本还是 0。

> ★ 结论：`set -e` 不是「出错就停」，而是「**非条件位的命令**出错才停」。
> 别把关键校验放进条件位；或者在函数里自己写 `|| return 1` 之类的显式判断（第 14 节给写法）。

```
════ 6. if ! cmd 与 if cmd; then…else…：真值表相同，读法不同 ════
  $ has(){ grep -q alice rp_conf.ini 2>/dev/null; }
if ! has; then echo "! 版：没有 alice"; fi
if has; then echo "正版：有 alice"; else echo "正版：没有 alice"; fi
echo "  两条结论一致；差别在 else 之后还能不能继续判断（推荐正着写）"
      正版：有 alice
        两条结论一致；差别在 else 之后还能不能继续判断（推荐正着写）
      ↪ rc=0

  $ if ! [[ -e rp_MISSING.ini ]]; then echo "![[ ]] 与 [[ ! ]] 都能用"; fi
if [[ ! -e rp_MISSING.ini ]]; then echo "  两种否定写法等价"; fi
      ![[ ]] 与 [[ ! ]] 都能用
        两种否定写法等价
      ↪ rc=0
```

`if ! cmd` 与 `if cmd; then…else…` 的真值表完全相同，`![[ ]]` 与 `[[ ! ]]` 也完全相同；
但**读法**不同：`if !` 要读者在脑中做一次反演，双重否定（`if ! not_x`）尤其难读。

### 11.6 短路之后 `$?` 是谁的

```
════ 7. 短路之后 $? 是谁的 ════
  $ false && true; echo "false && true → rc=$?"
      false && true → rc=1
      ↪ rc=0

  $ true || false; echo "true || false → rc=$?（false 根本没跑）"
      true || false → rc=0（false 根本没跑）
      ↪ rc=0

  $ false || true; echo "false || true → rc=$?"
      false || true → rc=0
      ↪ rc=0

  $ false && false; echo "false && false → rc=$?（第二个 false 没跑，码是第一个的）"
      false && false → rc=1（第二个 false 没跑，码是第一个的）
      ↪ rc=0

  $ true && false || true && false; echo "长链 → rc=$?"
      长链 → rc=1
      ↪ rc=0

  $ ! true; echo "! true → rc=$?"
! false; echo "! false → rc=$?"
      ! true → rc=1
      ! false → rc=0
      ↪ rc=0
[退出码 0]
```

背这张表（全部实测）：

| 表达式 | `$?` | 为什么 |
|---|---|---|
| `false && true` | 1 | `true` 根本没跑，码是 `false` 的 |
| `true \|\| false` | 0 | `false` 根本没跑，码是 `true` 的 |
| `false \|\| true` | 0 | 跑了 `true`，码是它的 |
| `false && false` | 1 | 第二个 `false` 没跑 |
| `true && false \|\| true && false` | 1 | 从左往右：真→假→真→假 |
| `! true` / `! false` | 1 / 0 | 取反只换 0/非 0，不产生别的值 |

### 11.7 `case`：模式是 glob，三种终止符三条轨迹

```
$ bash case_dispatch.sh

════ 1. 三种终止符：同一段代码，三种轨迹 ════
  输入 1.0.0：
    [命中 1.*]
  输入 2.5.1：
    [命中 2.*]
    [执行 2.9 的体（因为上面是 ;&）]
  输入 2.9：
    [命中 2.*]
    [执行 2.9 的体（因为上面是 ;&）]
  输入 9.9：
    [命中 [0-9].*]
  输入 xyz：
    [兜底 *]
  ── 换成 ;;&（每个后续分支都仍然要测试模式）──
  v1 error here → [error][slow]
  v2 slow → [slow]
  error slow → [error][slow]
  clean → （无标签）
```

* `;;` 正常结束分支（第一个分支命中就停）。
* `;&` **无条件落穿**到下一条分支的**命令体**（不再测模式）：输入 `2.5.1` 命中 `2.*`，
  然后把 `2.9)` 的体也跑了一遍。
* `;;&` **继续测试**后面的每一条分支（fallthrough but keep matching）：
  所以 `v1 error here` 同时拿到 `[error]` 和 `[slow]` 两个标签，
  而 `error slow`（没有版本号）只有 `[error][slow]`，`clean` 什么标签都不打。

> ★ `;&`/`;;&` 是 bash 的扩展（非 POSIX，dash 没有）。它们的用处是「**多标签打标**」——
> 用 `;;&` 写出来的分类器比一串 `if` 短得多，但如果你的脚本要 `#!/bin/sh`，就完全不能用。

```
════ 2. 模式是 glob 不是正则 ════
  模式「a.c」匹配「abc」→ 不命中
  模式「a.c」匹配「a.c」→ 命中
  模式「a*c」匹配「abc」→ 命中
  模式「a*c」匹配「ac」→ 命中
  模式「[0-9]*」匹配「9 lives」→ 命中
  模式「[0-9]*」匹配「nine」→ 不命中
  模式「v[0-9]」匹配「v1」→ 命中
  模式「*.log」匹配「app.log」→ 命中
```

`case` 的模式**是 glob，不是正则**：模式 `a.c` 匹配 `a.c` 而**不匹配** `abc`
（`case` 里 `.` 就是点，`*` 才是任意串，`[0-9]` 是字符类）。
想在 `case` 里做正则校验，只能先用 `[[ =~ ]]` 提取，或者用 glob 能表达的形式（`v[0-9]` 命中 `v1`）。

```
════ 3. | 或、*) 兜底位置、模式里的空格 ════
  a → 命中 a|x
  x → 命中 a|x
  带 空格 → 命中带引号的模式
  带 空格2 → 命中反斜杠转义的空格
  z → 兜底
  ── 把 *) 放到第一位会怎样 ──
  specific → 被第一位的 *) 吞掉，后面的分支永远轮不到
```

`|` 是「或」；模式里的空格要**转义或加引号**（`带\ 空格` / `"带 空格"`），否则模式会被拆词。
最后一行是一个真事故：把 `*)` 写在**第一位**，后面的分支永远轮不到 ——
`case` 是从上往下第一次命中即停（用 `;;` 时）。

```
════ 4. 版本分派：case 版 vs if-elif 版（同一组输入，结论逐字一致）════
  输入 1.0
  case 版：
    v1：只支持 legacy 接口
  if   版：
    v1：只支持 legacy 接口
  输入 2.3
  case 版：
    v2.0-2.4：legacy + new 双轨
  if   版：
    v2.0-2.4：legacy + new 双轨
  输入 2.7
  case 版：
    v2.5+：默认 new 接口
  if   版：
    v2.5+：默认 new 接口
  输入 3.1
  case 版：
    v3：只有 new 接口
  if   版：
    v3：只有 new 接口
  输入 v2.7
  case 版：
    带 v 前缀的写法：去掉 v 再来一次
  if   版：
    带 v 前缀的写法：去掉 v 再来一次
  输入 abc
  case 版：
    无法识别的版本号
  if   版：
    无法识别的版本号
```

这一组是同一段逻辑的 `case` 版和 `if-elif` 版，六组输入（`1.0`/`2.3`/`2.7`/`3.1`/`v2.7`/`abc`）
**结论逐字一致**。写 `case` 的理由不是「快」，是：

| 维度 | `case` | `if-elif` 链 |
|---|---|---|
| 版本号/后缀分派 | 模式写在行首，一眼看完 | 每行都要重复 `[[ $v == … ]]` |
| 前缀归一化（`v2.7`→`2.7`） | 单独一条分支处理即可 | 同样，但排版更散 |
| 多标签打标 | `;;&` | 只能多个 `if` |
| 可移植性 | POSIX（除 `;&`/`;;&`） | POSIX |
| 范围判断（`2.5+` 这种） | 需要拆成 `2.[5-9]*`/`[3-9]*` | 用字符串/数字比较更直观 |

```
════ 5. 后缀分派 + nocasematch ════
  access.log → 日志：走日志轮转
  data.csv → 表格：走 csv 解析
  archive.tar.gz → 压缩包（注意 * 能吃掉整串）
  notes.txt → 有后缀但没专门处理
  README → 没有后缀
  noext.LOG → 日志：走日志轮转
  ── 开 nocasematch 之后（同一个 case，不用把大小写都列出来）──
  noext.LOG → 只写小写 *.log 也命中（nocasematch=on）
  app.Log → 只写小写 *.log 也命中（nocasematch=on）
```

后缀分派的关键是**顺序**：`*.tar.gz` 必须写在 `*.gz` 之前（否则 `archive.tar.gz` 会被 `*.gz` 先吃掉，
虽然两者都算「压缩包」，但你的分支体可能不一样）。`*.*` 和 `*` 分别兜住「有后缀但没专门处理」和「没有后缀」。
最后两行是 `shopt -s nocasematch`：开了之后同一份 `case` 里只写 `*.log` 就能命中 `noext.LOG`、`app.Log`。

```
════ 6. case 的退出码来自哪一支 ════
  没命中任何分支（只有 1) 这条）→ $? = 0（空跑的 case 永远 0）
  命中且分支体最后一条失败 → $? = 1
  命中且分支体成功 → $? = 0
  ;& 落穿到下一条体，最后一条是 true → $? = 0
[退出码 0]
```

`case` 语句自己的退出码：**没命中任何分支时是 0**（空跑的 `case` 永远成功）；
命中时是**分支体最后一条命令**的码。这一条很容易搞错，很多人以为「没匹配到」应该返回非 0。

---

## 12. 退出码的传播与丢失

下面整块来自 `status_chain.sh`，它只做一件事：**把每一步的 `$?` 当场打印出来**。

```
$ bash status_chain.sh

──── 1. $? 只活到「下一条命令」为止
  false 之后立刻取                    $? = 1
    （中间插了一句 echo）
  同上，再取                          $? = 0（echo 成功了，码被换成 0）
  false 之后紧跟 : （空操作）     $? = 0（: 自己固定返回 0）
  false 之后跑纯赋值 x=5             $? = 0（赋值本身也算一条命令）
  false 之后跑一个假的 [[ ]]        $? = 1
  false 之后跑一个假的 [ ]          $? = 1
```

（第 1 组已在 6.1 展开过，这里接第 2 组：几个容易看走眼的改写者。）

```
──── 2. 几个容易看走眼的改写者
  ( exit 3 ) 子 shell                     $? = 3
  ( false; true ) 组里最后一条赢    $? = 0
  ( exit 3; true )：exit 之后没人跑  $? = 3
  false 之后跑 if true; then :; fi      $? = 0（then 分支改写）
  false 之后 case 一个分支都没命中 $? = 0（空跑也算成功）
  false 之后跑一个空 for 循环      $? = 0
  (( 1 )) 算术结果非零               $? = 0
  (( 0 )) 算术结果为零               $? = 1（这就是「失败」）
  v=$(( 5 - 5 )) 只是展开              $? = 0（展开不产生码，赋值产生 0）
```

* `( exit 3 )` → 3：子 shell 的码就是 `exit` 的码。
* `( false; true )` → 0：组里**最后一条**赢。
* `( exit 3; true )` → **3**：`exit` 当场结束子 shell，后面的 `true` 根本没机会跑
  （很多人以为组里最后是 `true` 就是 0，这条正是反例）。
* `if true; then :; fi` → 0：`then` 分支的 `:` 改写了码。
* 一个分支都没命中的 `case`、空 `for` 循环 → **0**（空跑算成功）。
* `(( 1 ))` → 0；`(( 0 ))` → **1**。算术表达式的规则是「结果为 0 → 退出码 1」，反过来才成立。
  这是 `(( ))` 作为条件时最容易踩的坑：`(( x-- ))` 在 x 从 1 变 0 时返回 1（`set -e` 下会炸）。
* `v=$((5-5))` → 0：`$(( ))` 只是**展开**，它不产生码；产生码的是赋值。

```
──── 3. 命令替换 $( ) 吃退出码
  false; a=$(true)                         $? = 0
  false; b=$(false)                        $? = 1（替换里的失败会传给赋值）
  同一行从左到右：0  ←    →  1
  ↑ 左边的数字是替换**之前**的码，右边的是替换**之后**的：$( ) 一跑完，$? 就换人了
  把失败包进 echo：   ← 这一句里替换失败了
  上面那条 echo 整条的退出码     $? = 0（echo 自己成功 = 0，替换里的 1 被吞）
```

命令替换吃退出码的两种姿势，以及**同一行从左到右**的取证：

* `true; echo "$? $(false) $?"` → 打印 `0 ←  → 1`：**左边的数字是替换之前的码**，
  右边的数字是替换之后的。`$( )` 一跑完，`$?` 当场换人。
* `echo "把失败包进 echo：$(false)"` → 整条 echo 的退出码是 **0**，替换里的 1 被吞掉了。

```
──── 4. local / export / declare 一律返回 0（set -e 的头号帮凶）
    函数里 local v=$(false) 之后      存下来的 rc = 0
  f_local 整个函数的退出码         $? = 0（local 把替换的 1 换成了 0，return 拿到的就是 0）
    函数里分开写之后               存下来的 rc = 1
  g_split 整个函数的退出码         $? = 1（分开写才保住了替换的 1）
  false 之后 export NAME=1               $? = 0（export 永远 0）
  false 之后 declare -a …              $? = 0
```

`local` / `export` / `declare` **一律返回 0**，这是 `set -e` 的头号帮凶：

| 写法 | 函数拿到的码 | 结论 |
|---|---|---|
| `local v=$(false)` | **0** | local 把替换的 1 换成了 0 |
| `local v` 然后 `v=$(false)` | **1** | 声明和赋值分开，码才保得住 |
| `export NAME=1` | 0（永远） | 前面 `false` 的码被抹掉 |
| `declare -a ARR=(1)` | 0（永远） | 同上 |

```
──── 5. 函数的退出码 = 函数体最后执行的那条命令
  h_mid(){ false; true; }                  $? = 0（中间的 false 没人记得）
  h_tail(){ true; false; }                 $? = 1（最后一条决定一切）
    （h_echo 里打印了这一行）
  h_echo(){ false; echo …; }             $? = 0（echo 才是最后一行）
  h_ret(){ false; return 9; }              $? = 9（return 是显式判据）
  h_if(){ if true; then false; fi; }       $? = 1
  h_if2(){ if false; then :; fi; }         $? = 0（条件假、没 else → 整条 if 是 0）
```

函数的退出码 = **函数体最后执行的那条命令**：

| 函数 | `$?` | 说明 |
|---|---|---|
| `h_mid(){ false; true; }` | 0 | 中间的 `false` 没人记得 |
| `h_tail(){ true; false; }` | 1 | 最后一条决定一切 |
| `h_echo(){ false; echo …; }` | 0 | **`echo` 才是最后一行** ← 你写日志时每天都在制造这个 |
| `h_ret(){ false; return 9; }` | 9 | `return` 是显式判据 |
| `h_if(){ if true; then false; fi; }` | 1 | if 语句的码 = 执行过的分支的最后一条 |
| `h_if2(){ if false; then :; fi; }` | **0** | 条件假、没有 else → **整条 if 是 0** |

最后一条要重点记：「什么都没做」在 bash 里就是成功。
所以 `if [ 条件 ]; then 干活; fi` 里 `干活` 失败，函数会返回非 0；但**条件不成立**时返回 0。

```
──── 6. 管道：$? 只是最后一段的，全部真相在 PIPESTATUS
  false | true                             $?=0  PIPESTATUS=[1 0]
  true | false                             $?=1  PIPESTATUS=[0 1]
  grep 命中 | grep 不命中             $?=1  PIPESTATUS=[0 1]
status_chain.sh: line 110: nonexistent_prog_03: command not found
  真命令 | 不存在的命令 | true    $?=0  PIPESTATUS=[0 127 0]
  在子 shell 里开 pipefail 再 false|true $? = 1
  在当前 shell 里用 { } 开 pipefail  $? = 1（括号组不 fork，开关留下来了）
  再跑一次 false|true（没再开开关） $? = 1（证明 pipefail 还在生效）
  set +o pipefail 之后再跑             $? = 0（回到「只看最后一段」）
```

管道是退出码丢失的重灾区，五行取证：

1. `false | true` → `$?` = **0**，`PIPESTATUS=[1 0]`。
2. `true | false` → `$?` = **1**，`PIPESTATUS=[0 1]`。
   → **`$?` 只是最后一段的码**，前面几段全在 `PIPESTATUS` 里。
3. `grep 命中 | grep 不命中` → `[0 1]`，`$?`=1 —— 这次恰好「看起来对了」，纯属最后一段就是失败那段。
4. `sleep 0.1 | nonexistent_prog_03 | true` → `PIPESTATUS=[0 127 0]`，`$?`=**0**。
   中间一段**命令根本不存在**（127），管道整体还是成功。这就是管道里「静默失败」的完整现场。
5. `pipefail` 的三段实验：子 shell 里 `set -o pipefail` 只影响那个子 shell；
   `{ set -o pipefail; false | true; }` 因为**不 fork**，开关**留了下来**，
   于是下一条 `false | true` 变成 **1**；`set +o pipefail` 之后又回到 0。

> ★ 想在一个开了 `pipefail` 的脚本里知道「哪一段失败」，只有 `${PIPESTATUS[@]}` 这一条路，
> 而且必须**紧贴管道**读（它和 `$?` 一样只活一条命令）。第 15 节的 capstone 就是这么用的。

```
──── 7. 「if cmd」和「if [ $? -eq 0 ]」的实测差别
  ① 紧贴着写：走到 else（$?=1，判对了）
  （中间就插了这么一句人畜无害的 echo）
  ② 隔了一句再判：走到 then ← 判据已被上面的 echo 改写成 0
  ③ if grep -q …：走到 else ← 推荐：条件就是命令本身
[退出码 0]
```

最后这七行是本章最值得抄走的一条经验：

* ① `grep -q zzz file` 之后**紧贴着** `[ $? -eq 0 ]` → 判对了，走 `else`。
* ② 同样两句之间**插了一句 `echo`** → 走 `then`：判据已被那句 `echo` 改写成 0，
  而**没有任何报错**。
* ③ 换成 `if grep -q zzz file` → 走 `else`，结论和 ① 一样，但它**不怕中间插话**。

> ★ 所以规则是：**能用命令本身当条件，就不要经 `$?`。**
> `if cmd; then`、`cmd || fail`、`[[ ]]` 都属前者；`cmd; if [ $? -eq 0 ]` 属后者，是脆的。

---

## 13. `test` / `[` / `[[` 三者关系：什么时候**只能**用 `[ ]`

> 先把编号对上，免得跟 demo 说不上话：本章 `## 6.`~`## 12.` 依次对应 demo 的第 1、2、4、5、6、7、3 节
> （demo 第 3 节「退出码的传播与丢失」被挪到最后讲，因为它要用到前面所有零件）。
> 往后是：章 13 ← demo 8、章 14 ← demo 9、章 15 ← demo 10。
> 完整的「章 ↔ demo 节号 ↔ 捕获行号」对照表在章末「怎么跑这章的 demo」里。

前面 12 节全是在 bash 里跑的，判据的「谁」一直固定。这一节换一个变量：**这段代码由谁读**。
`[[ ]]`、`=~`、`-v` 在 bash 里是语法，在 POSIX shell 里根本不存在 —— 不存在的时候不是「报错」这么简单，
它会**改变你走的分支**。这一节的五条实测就是这条链子。

### 13.1 本机的 `/bin/sh` 是 dash：`[[` 当场降级成「命令找不到」

```
$ readlink -f /bin/sh; stat -c "%F %n" /bin/sh
/usr/bin/dash
symbolic link /bin/sh
[退出码 0]
```

`/bin/sh` 是指向 `/usr/bin/dash` 的符号链接。所以「脚本第一行写 `#!/bin/sh`」
在这台机器上等于「把这份文件交给 dash 读」，而 dash 是纯 POSIX 实现，不认识 bash 的语法糖。

把 `[[` 当**独立命令**跑，下场很响亮：

```
$ sh -c '[[ 1 == 1 ]]'
sh: 1: [[: not found
[退出码 127]
```

`sh: 1: [[: not found` —— 请把它读成一句话：**dash 去 PATH 里找一个名叫 `[[` 的程序**，没找到。
第 8.1 节 `type -a "[["` 说的是 `shell keyword`，那是 bash 的答复；换一个读者，
`[[` 就从「语法」掉回「一条命令」，于是拿到第 7 节实测过的 **127 = 名字找不到**。
这一条好，因为它响了。

真正阴的是**同一句站在 `if` 的条件位上**：

```
$ sh -c 'if [[ 1 == 1 ]]; then echo 走了then; else echo 走了else; fi'
sh: 1: [[: not found
走了else
[退出码 0]

$ bash -c 'if [[ 1 == 1 ]]; then echo 走了then; else echo 走了else; fi'
走了then
[退出码 0]
```

三个立场对着读：

| `[[ … ]]` 站在哪 | 本机实测 | 危险性 |
|---|---|---|
| 独立命令（`sh -c '[[ 1 == 1 ]]'`） | `sh: 1: [[: not found`，退出码 **127** | 低：响了，而且 127 有明确来历 |
| `if` 的条件位（dash） | 报一行 `[[: not found`，**走 `else`**，整条 if 的退出码 **0** | 高：静默换分支，脚本「跑完了，没报错」 |
| `if` 的条件位（bash） | 走 `then` | —— |

第二行就是本章开头「场景 8」的完整答案，机制是两层已知事实的乘法：

1. 第 6.2 节实测过：**非 0 一律为假，shell 不看非 0 是几**。
   于是「`[[` 这个程序不存在」（127）和「`[[ … ]]` 判出来是假」（1）在条件位上**同一个待遇**。
2. 第 12 节实测过：`if` 的退出码 = **执行过的那个分支的最后一条命令**。`else` 里的 `echo` 成功 → 整条 if 是 0。

拿 C 作对照：`strcoll()` 只保证返回值的**符号**、不保证大小，所以你只能问「谁大谁小」，
不能拿那个整数当分数用。这里更极端 —— shell 只问「是不是 0」，
所以「判据不存在」可以完美伪装成「判据为假」。区别只藏在你不去读的 `$?` 里。

> ★ 所以「`[[ ]]` 在 sh 里不可用」最典型的表现**不是**一堆语法错误，
> 而是**你的 `else` 分支永远在跑，而脚本看起来一切正常**。

### 13.2 只用 POSIX 判据的脚本：四种跑法，结论一字不差

demo 在沙箱里用 heredoc 现写了两个小脚本（源码在 `demo.sh` 里，跑完随沙箱删掉）：

* `sh_only.sh`：`#!/bin/sh` + 只用 `[ ! -f "$f" ]` 和 `[ -s "$f" ]`，成功退 0、空文件退 1、不是普通文件退 2。
* `bash_only.sh`：`#!/usr/bin/env bash` + `[[ -f $f ]]`、`[[ $f == *.log ]]`、`[[ v9.1 =~ ^v([0-9]+) ]]`、`${BASH_REMATCH[1]}`。

先准备材料（`data.txt` 是一行非空文本，`zero.txt` 是 0 字节）：

```
$ printf "line\n" > data.txt; : > zero.txt
[退出码 0]
```

同一个 `sh_only.sh`，四种调用方式：

```
$ sh         sh_only.sh data.txt; echo "  sh rc=$?"
非空普通文件
  sh rc=0
[退出码 0]

$ dash       sh_only.sh data.txt; echo " dash rc=$?"
非空普通文件
 dash rc=0
[退出码 0]

$ bash       sh_only.sh data.txt; echo " bash rc=$?"
非空普通文件
 bash rc=0
[退出码 0]

$ bash --posix sh_only.sh data.txt; echo " bash --posix rc=$?"
非空普通文件
 bash --posix rc=0
[退出码 0]
```

四行输出逐字相同，四个 `rc` 都是 0。这才是「可移植判据」的定义：
**不是加了某个 flag，而是你写的每一个字符四种调用方式都认识、都给出同一个数**。
`sh`、`dash` 是两个名字同一个程序（13.1 那个符号链接），`bash` 和 `bash --posix` 读它也没意见 ——
POSIX 子集是这几个解释器的**交集**。

### 13.3 换成 bash 专属脚本：换读者就换分支，而且报错文案会说谎

```
$ bash      bash_only.sh data.txt; echo "  bash rc=$?"
版本号（正则，组1=9）
  bash rc=0
[退出码 0]

$ sh        bash_only.sh data.txt; echo "    sh rc=$?"
bash_only.sh: 4: [[: not found
用法错误：data.txt 不是普通文件
    sh rc=2
[退出码 0]

$ dash      bash_only.sh data.txt; echo "  dash rc=$?"
bash_only.sh: 4: [[: not found
用法错误：data.txt 不是普通文件
  dash rc=2
[退出码 0]
```

中间那条（`sh bash_only.sh data.txt`）是本章最阴的一条，逐行拆：

1. `bash_only.sh` 的第 4 行是 `[[ -f $f ]] || { echo "用法错误：$f 不是普通文件"; exit 2; }`。
2. 在 sh 里 `[[ -f $f ]]` 是「命令找不到」→ 非 0（127）。
3. 它站在 `||` **左边** = 条件位 → 于是 `||` 右边的兜底块**执行了**。
4. 打印「用法错误：data.txt 不是普通文件」，脚本 `exit 2`。

**可是 `data.txt` 就是普通文件** —— 上一组刚用 `[ -f ]` 在同一个文件上判出 0。
报错文案是脚本作者写的，它只能解释「判据为假」，不知道自己的兜底其实是被「判据不存在」拉进来的。
你会拿着这句话去查文件权限、查路径、查 `cd`，而真正的原因是**这份文件不该由 sh 来读**。

三条实测立场排一下危险度：

| 立场 | 实测到的表现 | 你查错方向的概率 |
|---|---|---|
| 独立命令 | 127 + 一行 `not found` | 低 |
| `if` 条件位 | 走 `else`，整条 if 退 0，只留一行 `not found` | 高（分支换了，码还是 0） |
| `\|\|` / `&&` 左边 | 兜底分支执行，**并且打印一句误导性的错误文案** | 最高（它主动骗你） |

`&&` 左边的镜像情形 demo 没单跑，本章只钉住上面这三条实测过的。

### 13.4 `bash --posix` 不是护身符：它不把 `[[ ]]`、`=~`、数组关掉

```
$ bash --posix -c '[[ 1 == 1 ]]'; echo "bash --posix 里的 [[ ]] rc=$?"
bash --posix 里的 [[ ]] rc=0
[退出码 0]

$ bash --posix -c '[[ v9 =~ ^v([0-9]) ]]; echo "组1=${BASH_REMATCH[1]}"'; echo "bash --posix 里的 =~ rc=$?"
组1=9
bash --posix 里的 =~ rc=0
[退出码 0]

$ bash --posix -c 'a=(1 2); echo "元素数=${#a[@]}"'; echo "bash --posix 里的数组 rc=$?"
元素数=2
bash --posix 里的数组 rc=0
[退出码 0]
```

三条全是 **rc=0**：`bash --posix` 里 `[[ ]]` 照用、`=~` 连 `${BASH_REMATCH[1]}` 都照给出 `组1=9`、
数组 `a=(1 2)` 的 `${#a[@]}` 照样是 2。

所以「我加了 `--posix`，这脚本就 POSIX 了」是错觉。`--posix` 调整的是**少数几条 bash 扩展的行为**，
它不会把你已经写下去的语法糖变成非法。（拿 C 编译器类比一下：`-std=` 关掉的是语言规则里那部分，
编译器扩展通常照旧可用 —— 这是类比，不是本机实测。）
真正的门只有一个：**哪个解释器读这个文件**。13.1/13.3 已经给了两把钥匙：
`/bin/sh` 是 dash；`sh`/`dash` 读 bash 糖 → 127 → 换分支。

### 13.5 dash 缺的四件东西，和一件**没缺**的

```
$ dash -c '((1))'
dash: 1: 1: not found
[退出码 127]

$ dash -c 'a=(1 2)'
dash: 1: Syntax error: "(" unexpected
[退出码 2]

$ dash -c '[ -v HOME ]'
dash: 1: [: -v: unexpected operator
[退出码 2]

$ dash -c '[ 010 -eq 10 ]'
[退出码 0]
```

1. `dash -c '((1))'` → `dash: 1: 1: not found`，rc=**127**。dash 没有 `(( ))` 这条算术语法，
   于是 `(` 被当成**子 shell 的开始**，`((1))` 变成「子 shell 里再开子 shell，里面跑命令 `1`」，
   报错报的是那个 `1`。一行线索都没多给。
2. `dash -c 'a=(1 2)'` → `Syntax error: "(" unexpected`，rc=**2**：这是**解析期**错误，
   和第 8.10 节 `[[ 1 -eq ]]` 同族 —— 整条 `-c` 当场死，后面的句子根本不会被执行。
3. `dash -c '[ -v HOME ]'` → `dash: 1: [: -v: unexpected operator`，rc=**2**。
   这条要和第 8.1 节那条并起来看：bash **内建** `[` 认 `-v`；**外部** `/usr/bin/[`（本机是 uutils 那份）
   报 `[: extra argument 'HOME'` rc=2；dash 报 `unexpected operator` rc=2。
   **三家实现三种原文，结论一致：`-v` 只能在 bash 里用。** 这就是第 10.3 节那句「dash 不认」的现场。
4. `dash -c '[ 010 -eq 10 ]'` → rc=**0**，一声不响。**这条没缺**：dash 的 `test` 和 bash 内建一样
   把 `010` 读成十进制 10（第 8.6 节实测的同一件事）。
   「前导零 = 八进制」只住在 `(( ))` 和 `$(( ))` 里，换 shell 不会改这个结论。

第 4 条的价值是当**对照组**：前三条是「换读者就换答案」，第四条是「换读者也不换」。
写脚本时你要的正是这种确定性 —— 判据要么落在交集里（第 4 条那种），要么你就把读者钉死成 bash。

### 13.6 三者关系收口：什么时候你**只能**用 `[ ]`

一句话身份（第 8.1 节的 `type -a` + 这一节的换读者，两边合起来才是完整的）：

| 写法 | 身份 | 受第 1 层（拆词/文件名展开）摆布吗 | 换 shell 还能用吗 |
|---|---|---|---|
| `test` | 程序（内建 + 外部两份） | **受** | 受 POSIX 保护，`test` 本身到处都在 |
| `[ ]` | `test` 的同名别名（程序），`]` 必须是最后一个**参数** | **受**（第 8 节整节在讲这个） | POSIX 子集到处都在；扩展开关不一定（`-v` 三条实测） |
| `[[ ]]` | **bash 的保留字**（不是命令） | 不受（第 10.1/10.2 节） | **不能**：13.1/13.3 现场 |

那么「只能用 `[ ]`」的场景是哪些？本章能钉住的只有两类：

1. **这份文件会被 POSIX shell 读**：shebang 写 `#!/bin/sh`，或者调用方是 `sh script.sh` / `dash script.sh`
   （13.3 就是这个现场）。这时 `[[ ]]`、`=~`、`(( ))`、数组、`-v` 都是**禁用项**。
2. **你控制不了读者**：命令串交给别人的解释器执行。C 里 `system(s)` 按标准就是「起一个 sh 读这段字符串」，
   本机的 sh 是 dash —— 所以 `system("[ a = a ]")` 能用，`system("[[ a == a ]])` 走的是 13.1 那条链
   （**注意：`system()` 这条路本章没在本机演，属于「本章没演」清单**，这里只作方向提示）。

反过来，其余场合（shebang 是 bash 的脚本、交互 shell）本章的建议是第 10 节那套：**默认 `[[ ]]`**，
因为 10.1/10.2 实测过它把「漏引号 → 参数个数变 → 换套解释规则」这一整类事故**消灭**了，
而 `[ ]` 只能靠你每次记得加引号。

> ★ 三个身份背下来：`test` 是程序，`[` 是它的同名别名（所以空格是语法），`[[` 是 bash 的语法。
> 前两个受拆词和文件名展开摆布（第 8 节），第三个只受「谁来读这个文件」摆布（这一节）。

---

## 14. 判据写法取舍：同一件事的几种写法，实测差在哪

第 8.9 节结尾欠了一条「推荐写法」，第 11.5 节结尾欠了一条「显式判断的写法」，
第 8.10 节结尾欠了一条「语法错误的兜底手段」—— 这一节把三笔账一起还。
demo 第 9 节实测的就是三组「同一件事、几种写法」，我先按原文摆出来，再给结论。

### 14.1 嵌套括号四条写法：第 2 条不是「判据为假」，是「这条命令行没解析过去」

同一道题：`a=1`、`b=99`，意图「a 等于 1 **或** b 等于 99」。

```
$ a=1; b=99; if [ \( "$a" = 1 -o "$b" = 99 \) ]; then echo "1a"; else echo "1b"; fi
1a
[退出码 0]

$ a=1; b=99; if [ ( "$a" = 1 -o "$b" = 99 ) ]; then echo "2a"; else echo "2b"; fi
bash: -c: line 1: syntax error near unexpected token `"$a"'
bash: -c: line 1: `a=1; b=99; if [ ( "$a" = 1 -o "$b" = 99 ) ]; then echo "2a"; else echo "2b"; fi'
[退出码 2]

$ a=1; b=99; if [[ ( $a == 1 || $b == 99 ) ]]; then echo "3a"; else echo "3b"; fi
3a
[退出码 0]

$ a=1; b=99; if [[ $a == 1 ]] || [[ $b == 99 ]]; then echo "4a"; else echo "4b"; fi
4a
[退出码 0]
```

| # | 写法 | 实测 | 为什么 |
|---|---|---|---|
| 1 | `[ \( "$a" = 1 -o "$b" = 99 \) ]` | `1a`，rc=0 | 能跑。括号**必须**转义成 `\( \)`：`(`/`)` 对 shell 是语法字符，而 `[` 是**程序**，它的参数里不能出现裸括号 |
| 2 | `[ ( "$a" = 1 -o "$b" = 99 ) ]` | `bash: -c: line 1: syntax error near unexpected token` ，rc=**2**；`2a`、`2b` **一个都没打印** | shell 在解析 `(` 的那一刻就失败了，整条 `bash -c` 的字符串没读完 |
| 3 | `[[ ( $a == 1 || $b == 99 ) ]]` | `3a`，rc=0 | 括号在 `[[` 里就是**分组**，不用转义；变量也不用加引号（10.1） |
| 4 | `[[ $a == 1 ]] \|\| [[ $b == 99 ]]` | `4a`，rc=0 | 拆成两个独立 `[[ ]]`，用控制运算符串起来（左结合，11.4） |

第 2 条必须和第 8.10 节对照着读，那里实测过同一件事的另两种形态：

* `[[ 1 -eq ]]` → **解析期**错误 → 整份脚本在第 2 行终止，脚本退出码 2，第三行的 `echo` 没执行。
* `[ 1 -eq 2 -zz ]` → `test` **运行时**抱怨 → 打印错误、脚本继续、第三行照跑，脚本退出码 0。
* 本节的裸括号 → 也是**解析期**错误，而且早到「连 `if` 这个结构都没成型」。

所以三条的**可发现性**是一条降序的：解析期错误最响（rc=2 + 分支消失，一眼看出没跑）>
运行期错误（有原文，但脚本继续，可能写出半成品）。
`[退出码 2]` 这一行是 runc 打的整条 `bash -c` 的码 —— 它是 2 而不是 0，
这就是「判据没成型」和「判据成型但是假」在退出码上的可见差别（前提：你去读它）。

取舍（四条都实测了，排序按「出事时你多快能发现」）：

1. **优先 4**：每个子判断独立成 `[[ ]]`，词数不可能被变量值改掉（10.1），
   `||` 是控制运算符、短路行为第 11.4 节实测过。多写十来个字符，换一个「不会静默」的结构。
2. **嵌套深了用 3**：`[[ $a == 1 && ( $b == 2 || $b == 9 ) ]]` → rc=0，第 8.9 节最后一条实测过。
3. **必须 POSIX 时才用 1**：`\( \)` 能跑，但它依赖 `-o`/`-a`，而第 8.9 节实测
   `-a` 比 `-o` **结合更紧**（同一句不加括号 → rc=0，你的意图是假），POSIX 自己说超过 4 个参数行为未定义。
4. **永远不要 2**：裸括号。它不给你「假」，它给你「没跑」。

### 14.2 `if ! cmd` 与 `if cmd; then…else…`：真值表相同，`set -e` 下的**存活面也相同**

demo 现写三个脚本，同一句判据三种放法，每个都开 `set -e`：

```
$ printf "%s\n" "set -e" "worker() { if ! false; then echo 走了then; fi; echo 函数第3步还在; }" "worker" "echo 脚本还在" > bang.sh
[退出码 0]

$ bash bang.sh; echo "  带 ! 的这版 rc=$?"
走了then
函数第3步还在
脚本还在
  带 ! 的这版 rc=0
[退出码 0]

$ printf "%s\n" "set -e" "worker() { if false; then echo x; else echo 走了else; fi; echo 函数第3步还在; }" "worker" "echo 脚本还在" > nobang.sh
[退出码 0]

$ bash nobang.sh; echo "  不带 ! 的这版 rc=$?"
走了else
函数第3步还在
脚本还在
  不带 ! 的这版 rc=0
[退出码 0]

$ printf "%s\n" "set -e" "false" "echo 这行永远不执行" > plain.sh
[退出码 0]

$ bash plain.sh; echo "  裸 false 在 set -e 下 rc=$?"
  裸 false 在 set -e 下 rc=1
[退出码 0]
```

三个 `rc`：0、0、**1**。

| 脚本 | 判据怎么放 | 实测 |
|---|---|---|
| `bang.sh` | `worker(){ if ! false; then echo 走了then; fi; echo 函数第3步还在; }` | `走了then` → `函数第3步还在` → `脚本还在`，**rc=0** |
| `nobang.sh` | `worker(){ if false; then echo x; else echo 走了else; fi; echo 函数第3步还在; }` | `走了else` → `函数第3步还在` → `脚本还在`，**rc=0** |
| `plain.sh` | 顶层一行裸 `false` | 下一行的 `echo` **没跑**，**rc=1** |

这一组和 11.5 是同一个结论的两次取证，但这里多出一条信息：**`!` 在这里不额外改变任何东西**。
带 `!` 和不带 `!` 的两个版本，函数后续步骤照跑、脚本退出码都是 0。
所以「用 `!` 还是用 `else`」是**可读性**问题，不是保护强度问题；
保护强度只由一件事决定 —— **这条命令有没有站在条件位**。

站在条件位 = 三种位置（第 11.3/11.5 节实测）：`if`/`while`/`until` 的判断、`!` 的右边、
`&&`/`||` 的左边。进了这三格，`set -e` 就地失效，连函数体内部每一行一起失效（11.5 第 4、5 条）。
`plain.sh` 之所以死，就因为它把 `false` 放在了**顶层、独立一行**：不在任何条件位里。

于是「想失败就真的停」的写法只有一种形状：**让失败自己成为一个动作，而不是一个判断**。
本章实测到的对应零件是：

* `cmd || { echo "cmd 失败"; exit 2; }` —— 13.3 那条 `bash_only.sh` 就是它的现场（`||` 右边**真的会跑**，
  并且脚本以 2 结束）。注意副作用：`cmd` 本身被豁免了，所以**别再指望 `set -e` 帮你拦它**。
* 函数里 `check || return 1` —— 依据是第 12 节实测的「函数退出码 = 函数体最后执行的那条命令」
  （`h_ret(){ false; return 9; }` → 9）。**这条组合本章没单独跑过**，它是上面两条实测规则的直接拼接，
  照抄前请自己跑一次。
* 可读性上的取舍：`if ! has; then echo 没有; fi` 和 `if has; then echo 有; else echo 没有; fi`
  真值表相同（11.5 最后一条实测过两者一致）。后者不用在脑子里做反演，
  双重否定（`if ! not_x`）尤其难读；**两者都不给你更多 `set -e` 保护**。

### 14.3 取反的三个位置：文件真不存在时三种写法等价，变量是空串时差一档

```
$ f=nope.txt; ! [[ -e $f ]]; echo "! [[ ]] rc=$?"
! [[ ]] rc=0
[退出码 0]

$ f=nope.txt; [[ ! -e $f ]]; echo "[[ ! ]] rc=$?"
[[ ! ]] rc=0
[退出码 0]

$ f=nope.txt; [ ! -e $f ]; echo "[ ! ]  rc=$?"
[ ! ]  rc=0
[退出码 0]

$ f=""; [ ! -e $f ];   echo "f 是空串时，不加引号的那条 rc=$?"
f 是空串时，不加引号的那条 rc=1
[退出码 0]

$ f=""; [ ! -e "$f" ]; echo "f 是空串时，加引号的那条   rc=$?"
f 是空串时，加引号的那条   rc=0
[退出码 0]
```

前三条（`f=nope.txt`，文件真的不存在）：`! [[ -e $f ]]`、`[[ ! -e $f ]]`、`[ ! -e $f ]` **全是 rc=0**，
三种取反位置在这里没有任何行为差别。

后两条把 `f` 换成空串，**同一个源语句** `[ ! -e $f ]` 得到 **1**，加了引号的 `[ ! -e "$f" ]` 得到 **0**。
差别不在逻辑，在第 8.3 节那张「参数个数决定解释」的表上：

* `f=""` 不加引号 → 展开后是 `[ ! -e ]`，**2 个参数** → 走表里第 2 行「`! 表达式`」，
  表达式只剩 `-e` 一个词 → 按**单参数规则**（字符串非空即真，第 8.3 节实测 `[ -n ]`→0、`[ -v ]`→0）为**真** →
  取反 → **假** → rc=1。
* `f=""` 加引号 → `[ ! -e "" ]`，**3 个参数** → 「`!` + 一元 `-e` + 空串」→ `-e ""` 判的是
  「空字符串这个名字存不存在」→ **假** → 取反 → **真** → rc=0。

把 rc=1 送回第 6.2 节（非 0 一律为假），现场后果是：

```text
f=""; if [ ! -e $f ]; then 建新文件; fi   ← 不建，而且一句错都不报
f=""; if [ ! -e "$f" ]; then 建新文件; fi  ← 会建
```

**同一句源码，一个建一个不建。** 这是第 8.4 节「漏一个引号」事故的另一副面孔：
前一次是「空值被判成非空」，这一次是「不存在被判成存在」。两次都不是逻辑写错，
都是**参数个数变了，程序换了套解释规则**（心智模型的第 1 层）。

取舍：

* 否定判断优先写进 `[[ ]]`：`[[ ! -e $f ]]`。10.1 实测过 `[[ ]]` 左边不拆词
  （`x="a b"` 时 `[[ $x == "a b" ]]` 不加引号也 rc=0），空串同样保住，取反位置在内在外行为一致（本节前三条）。
* 必须用 `[ ]`（POSIX 场合，13.6）时，引号是**硬要求**：`[ ! -e "$f" ]`。
* 「判符号链接是不是坏的」用第 9.1 节那条结论的写法：`[ -L "$f" ] && [ ! -e "$f" ]`
  （依据是 9.1 实测的 `-L link_broken`→0 与 `-e link_broken`→1；这个两步组合本身 demo 没单独跑过一条命令，
  跑过的是它两个判据各自的那个 rc）。
  它正好是「拆分式」的样板：两个独立 `[ ]`，各自的参数个数都是可预期的 3 和 4。

### 14.4 本章推荐写法汇总

每一行右边都指着本章实测过它的节；**没实测的组合一律不上这张表**。

| 你要判的东西 | 推荐写法 | 实测依据 | 别写（会出什么事） |
|---|---|---|---|
| 字符串是否为空 | `[[ -z $v ]]` / `[ -z "$v" ]` | 8.4 事故 5、6 | `[ -z $v ]`：空值时退化成单参数 `-z` → **靠意外判对**，值里有空格就 rc=2 报 `binary operator expected`（8.4 事故 5 实测） |
| 字符串是否非空 | `[[ -n $v ]]` / `[ -n "$v" ]` | 8.4 事故 5 | `[ -n $v ]`：空值时是 `[ -n ]` → **rc=0，把空判成有值**（8.4 事故 5 最后一条，八条里最危险的一条） |
| 数字是否为 0 | `[ "$n" -ne 0 ]` / `(( n ))` | 8.3 结尾三条、12 第 2 组 | `[ $n ]` → `0` 是非空字符串，**恒真**（8.3 实测 `[ 0 ]`→0） |
| 变量有没有被设过 | `[[ -v name ]]` / `[[ -v arr[1] ]]` | 10.3 | `[ -v … ]`：bash 内建能用，外部 `/usr/bin/[` 报 `[: extra argument 'HOME'`（8.1），dash 报 `[: -v: unexpected operator`（13.5 第 3 条），两个都是 rc=2 |
| 文件是否存在 / 类型 | `[[ -f $p ]]`，POSIX 场合作 `[ -f "$p" ]` | 9.1、10.2、13.2 | 裸 `[ -f $p ]` 且 `p` 的内容长得像 glob → **静默换成别的答案**（10.2 实测 rc=0） |
| 「不存在才动手」 | `[[ ! -e $f ]]` | 14.3 前三条 | `[ ! -e $f ]`（不加引号）→ `f` 是空串时 rc 反过来，14.3 后两条实测 1 vs 0 |
| 两个条件相或 | `[[ A ]] \|\| [[ B ]]`，其次 `[[ ( A \|\| B ) ]]` | 14.1 第 3、4 条 | `[ ( A -o B ) ]` → 解析期死亡（14.1 第 2 条）；`[ \( A -o B \) ]` 能用但掉进 `-a/-o` 优先级（8.9、14.1 第 1 条） |
| 正则匹配 | `[[ $s =~ $pat ]]`，`pat` 存进变量 | 10.5（含 `BASH_REMATCH`） | `=~ "带引号的正则"` → **永远匹配不上**（10.5 实测） |
| 命令本身成没成 | `if cmd; then` / `cmd || { …; exit 2; }` | 11.1、13.3、12 第 7 组 | `cmd; if [ $? -eq 0 ]` → 中间插一句就作废（12 第 7 组实测 ②走 then） |
| 管道里哪一段失败 | `ps=("${PIPESTATUS[@]}")` **紧贴**管道之后 | 12 第 6 组 | 只看 `$?` → 只剩最后一段（12 第 6 组实测 `[0 127 0]` 而 `$?`=0） |
| 一串后缀/版本分派 | `case` + glob，`*)` 兜底放最后 | 11.7 | 在 `case` 模式里写正则（11.7 实测 `a.c)` 只匹配字面 `a.c`） |
| 让失败真的终止脚本 | 失败动作单独成行，不进条件位 | 14.2（`plain.sh` rc=1） | 把关键校验塞进 `if`/`!`/`&&` 左/`||` 左 → `set -e` 全部失效（11.5、14.2） |

最后把 8.10 欠的那条兜底手段还上：**改完脚本先 `bash -n 脚本名`**。
它只解析不执行，所以 14.1 第 2 条那种「裸括号」、8.10 那种 `[[ 1 -eq ]]` 都会在 `bash -n` 阶段被挑出来，
而你一行副作用都不会有（这条机制在第 01 章实测，**本章的捕获里没有它的输出**，所以这里不贴）。

> ★ 这张表背后的统一逻辑只有一句：**`[ ]` 的可靠性靠你控制参数个数，`[[ ]]` 的可靠性靠控制读者**。
> 前者靠引号、靠不混用 `-a/-o`；后者靠 shebang 和「这份文件由谁读」。
> 两条都不靠运气，也都不靠 `set -e`。

---

## 15. capstone：`decide.sh` —— 40 行把本章判据串成一个真工具

demo 第 10 节在沙箱里用 heredoc 现写了一个 40 行脚本 `decide.sh`（跑完随沙箱一起删掉，
源码就在 `demo.sh` 的 `<<'DECIDE'` 里）。它做的事很无聊：数一个文件里有多少行非空行，够不够门槛。
但它值钱在两点：**一条执行路径上连用六种本章判据**，以及**它把自己的结论翻译成 0/1/2 三档退出码**。
它故意**不开 `set -e`** —— 14.2 刚实测过「条件位一踩，保护全消」，
这个脚本里到处是 `[[ ]]` 和 `||`，开了 `set -e` 只会让你看不出到底谁失败了。

### 15.1 先看清这条路径上有几种判据，每种出自哪一节

下表左边几列是脚本里的写法（源码文本，本章捕获里没打印过它，所以不贴进代码块）：

| 脚本里那一行 | 用的判据 | 本章实测在哪 |
|---|---|---|
| `while [ $# -gt 0 ]; do` | `[ ]` 里的 `-gt`：数字比较走运算符，不走字典序 | 8.6 |
| `case "$1" in -v) … ;; -*) … ;; *) break ;; esac` | glob 模式分派 + `;;` 结束分支；`-*` 是 glob 不是「任意连字符」 | 11.7 |
| `if [ $# -lt 1 ] \|\| [ $# -gt 2 ]; then … exit 2` | **拆分式**：两个独立 `[ ]` 用控制运算符 `\|\|` 串起来 | 14.1 第 4 条、11.4 |
| `if ! [[ $min =~ ^[0-9]+$ ]]; then … exit 2` | `=~` 右边**不加引号**才是正则；`!` 站在 `[[ ]]` 外面 | 10.5、14.3 |
| `if [[ ! -f $target ]]; then … exit 2` | 文件类型判断；变量裸写在 `[[ ]]` 里不被拆词 | 9.1、10.1 |
| `lines=$(grep -c '[^[:space:]]' "$target"); grep_rc=$?` | 命令替换**之后立刻**读 `$?`；`grep -c` 的「打印 0 但 rc=1」 | 12 第 3 组、7 第 A 组 |
| `if [ "$grep_rc" -gt 1 ]; then … exit 2` | 拿退出码的**数值**分档（`if` 只会看 0/非 0，这里故意自己读 `$?`） | 6.2、7 第 A 组 |
| `ps=("${PIPESTATUS[@]}")`（紧跟 `grep … \| wc -l`） | 管道分段码，必须**紧贴**管道取 | 12 第 6 组 |
| `if [ "$lines" -ge "$min" ]; then … exit 0; fi` + `exit 1` | 数字比较 + 三档退出码的最后一跳 | 8.6、7、6.3 |
| `min=${2-1}` | 「第 2 个参数没给就用 1」，脚本开头是 `set -u` | 语法本身属第 02 章的参数展开；**它的结果在本节第 9 档实测到**（打印了「>= 门槛 1」） |

`decide.sh` 的用法与约定（demo 的注解行写的，含义由下面九档实测支撑）：

```text
用法: decide.sh [-v] <文件> [最小非空行数]
退出码: 0 = 达标 / 1 = 内容不达标 / 2 = 用法或文件问题
```

### 15.2 九档真实结果：一个字符都没改，全出自那次捕获

材料先在沙箱里造好（`ok.log` 三行、`zero.log` 空、`name with space.log` 文件名带空格）：

```
$ printf "a\nb\nc\n" > ok.log; : > zero.log; printf "x\n\n" > "name with space.log"
[退出码 0]
```

**0 档与 1 档**（内容达标 / 内容不达标）：

```
$ bash decide.sh ok.log 2; echo "  达标           → rc=$?"
PASS ok.log：3 行 >= 门槛 2
  达标           → rc=0
[退出码 0]

$ bash decide.sh ok.log 9; echo "  门槛调到 9     → rc=$?"
FAIL ok.log：3 行 < 门槛 9
  门槛调到 9     → rc=1
[退出码 0]

$ bash decide.sh zero.log 1; echo "  空文件         → rc=$?"
FAIL zero.log：0 行 < 门槛 1
  空文件         → rc=1
[退出码 0]
```

**2 档**（用法或文件问题，五种触发方式）：

```
$ bash decide.sh nosuchfile 1; echo "  文件不存在     → rc=$?"
文件不存在或不是普通文件: [nosuchfile]
  文件不存在     → rc=2
[退出码 0]

$ bash decide.sh ok.log abc; echo "  门槛不是数字   → rc=$?"
门槛必须是纯数字，收到的是 [abc]
  门槛不是数字   → rc=2
[退出码 0]

$ bash decide.sh; echo "  没给参数       → rc=$?"
用法: decide.sh [-v] <文件> [最小非空行数]
  没给参数       → rc=2
[退出码 0]

$ bash decide.sh ok.log 1 2 3; echo "  参数过多       → rc=$?"
用法: decide.sh [-v] <文件> [最小非空行数]
  参数过多       → rc=2
[退出码 0]

$ bash decide.sh -x ok.log; echo "  未知选项       → rc=$?"
未知选项: -x
用法: decide.sh [-v] <文件> [最小非空行数]
  未知选项       → rc=2
[退出码 0]
```

**最后一档**：`-v` 打开管道版计数，文件名带空格：

```
$ bash decide.sh -v "name with space.log"; echo "  带空格的文件名+-v → rc=$?"
管道版计数：1
PIPESTATUS=[0 0]  ps[0]=0(grep) ps[1]=0(wc)
PASS name with space.log：1 行 >= 门槛 1
  带空格的文件名+-v → rc=0
[退出码 0]
```

九档汇总（前两列是上面原文里的）：

| # | 调用 | 脚本打印 | `rc` | 这一档在说什么 |
|---|---|---|---|---|
| 1 | `decide.sh ok.log 2` | `PASS ok.log：3 行 >= 门槛 2` | **0** | 达标 |
| 2 | `decide.sh ok.log 9` | `FAIL ok.log：3 行 < 门槛 9` | **1** | 正常答复：不达标 |
| 3 | `decide.sh zero.log 1` | `FAIL zero.log：0 行 < 门槛 1` | **1** | 正常答复：不达标（数出来的就是 0 行） |
| 4 | `decide.sh nosuchfile 1` | `文件不存在或不是普通文件: [nosuchfile]` | **2** | 没法干活：文件问题 |
| 5 | `decide.sh ok.log abc` | `门槛必须是纯数字，收到的是 [abc]` | **2** | 没法干活：参数格式不对 |
| 6 | `decide.sh` | `用法: decide.sh [-v] <文件> [最小非空行数]` | **2** | 没法干活：参数太少 |
| 7 | `decide.sh ok.log 1 2 3` | 同上用法行 | **2** | 没法干活：参数太多 |
| 8 | `decide.sh -x ok.log` | `未知选项: -x` + 用法行 | **2** | 没法干活：选项不认识 |
| 9 | `decide.sh -v "name with space.log"` | `管道版计数：1` / `PIPESTATUS=[0 0] …` / `PASS …1 行 >= 门槛 1` | **0** | 达标（走了管道版计数） |

> ⚠ **读这张表前先躲开一个坑**：上面四个代码块的每一段末尾都是 `[退出码 0]`，
> 连 rc=2 的那五段也是 0。那是 runc 打的，它测的是**整条 `bash -c` 命令行**的码，
> 而那条命令行的最后一条是 `echo "  … → rc=$?"` —— 第 12 节第 5 组的 `h_echo(){ false; echo …; }`
> 实测过「`echo` 才是最后一行，它把码换成 0」。
> **这个脚本真正的判决在 `→ rc=N` 那一行，不在 `[退出码 N]` 那一行。**
> 只盯 `[退出码 0]` 的人会得出「九条全成功」，其中包括「文件不存在」那条。

### 15.3 为什么退出码要分 0 / 1 / 2 这三档

先承认一件反直觉的事：**这三档在 shell 的控制流里完全没有区别**。
第 6.2 节实测过 `exit 1`、`exit 2`、`exit 255` 在 `if` 眼里一模一样。
所以 `decide.sh ok.log 9 || echo 不行` 和 `decide.sh nosuchfile 1 || echo 不行`
走的是同一个分支，`&&`/`||`/`if` 谁也分不清 1 和 2。

那分档是给谁看的？两个读者：

1. **人**：`echo "  文件不存在     → rc=2"` 这一行就是给人眼看的分档标签。
2. **上层脚本**：只有它**自己把 `$?` 当数值读**时，档位才有意义 ——
   `rc=$?`（紧贴调用）然后 `case $rc in 0)…1)…2)…esac`。
   这正是 `decide.sh` 内部对 `grep` 做的事：`grep_rc=$?` 之后 `[ "$grep_rc" -gt 1 ]`，
   它没有用 `if grep …`（那样只能拿到 0/非 0），而是**故意绕开控制流去读数值**。

然后是分档的标准。不是「严重程度」，是**答案的层级**：

| 档 | 含义 | 谁该负责 | 上层该干什么 |
|---|---|---|---|
| 0 | 我正常干完活，答案是「达标」 | —— | 继续往下走 |
| 1 | 我正常干完活，答案是「不达标」 | **数据** | 报警 / 记录 / 让人看，但别当故障重试 |
| 2 | 我**没能给出答案**（用法错、文件不对、读不了） | **调用方或环境** | 修命令行、修权限；这次结论根本不存在 |

把 1 和 2 塞进同一档会失去什么？第 2 档和第 4 档的 rc 都是非 0，
但一个是「这批日志确实行数不够」（要人去查日志），一个是「你把文件名敲错了」（要人去查脚本）。
上层分不开，就只能要么把所有 FAIL 当故障重试（然后一直失败），
要么把所有 rc=2 当业务结论吞掉（然后永远发现不了参数写错）。
反过来把 2 报成 0 更糟：CI 里 `decide.sh "$f" && echo OK` 会对着一个不存在的文件打印 OK。

为什么**偏偏是 0/1/2 这三个数字**？因为它不是这个脚本发明的：

* `grep`：0 找到 / 1 没找到 / 2 没法干活（第 7 节 A 组，`grep -c` 没命中时**打印 0 而 rc=1** 也实测了）。
* `diff`：0 一样 / 1 不一样 / 2 没法干活（B 组）。
* `test` / `[` 自己：0 真 / 1 假 / 2 用法错（C 组，`[: -zz: binary operator expected` → rc=2）。
* `126`、`127`、`128+signum` 这三段已经被 shell 征用（E、F 组实测）：
  你把「文件不存在」定成 127，它就撞上「命令找不到」；定成 137 就撞上 SIGKILL；定成 2 才安全，
  因为 2 已经是这一族「用法/故障」的通用编号。
  换句话说：**留给业务码的小数字只有 0、1、2、3…，而 0/1/2 这一组已经有现成约定，占了它等于免费。**

拿 C 对照最贴切：这就是 `errno` 的分段逻辑 —— 你自定义错误码时不能随手挑一个正数当「文件不存在」，
因为调用方要靠**数字本身**决定是重试、是报警、还是直接退出；同理 `grep` 的 1 之所以不是错误，
就像 `strcoll` 的返回值大小没有含义、只有符号有含义一样：**约定只保证区分，不保证度量**。

最后一档细节值得单独钉：脚本里写的是 `[ "$grep_rc" -gt 1 ]`，不是 `[ "$grep_rc" -ne 0 ]`。
差别就在第 3 档那个现场：`decide.sh zero.log 1` 里 `grep -c` 打印 `0`、退出码是 `1`，
脚本判 `1 -gt 1` 为假 → **不把「空文件」当故障**，继续按数字比门槛，最后 `exit 1`。
要是写成 `-ne 0`，所有空文件都会退 2 报「读文件失败」——
那是把「业务上的否」当「技术上的坏」，是这一族 bug 里最常见的一种。

### 15.4 第 9 档里藏着本章最核心的一句结论

第 9 档那条命令行是 `bash decide.sh -v "name with space.log"`。
文件名叫 `name with space.log`，**带空格**。进了脚本之后，同一个值被两种写法对待
（下面两行是 `decide.sh` 的**源码**，箭头后面是我的注解；源码是 demo 用 heredoc 现写进沙箱的，
不在那次捕获里，所以这里不可能是运行输出块）：

```text
if [[ ! -f $target ]]; then          ← 变量裸用，不加引号
lines=$(grep -c '[^[:space:]]' "$target")   ← 外部命令的参数，必须加引号
```

* `[[ ! -f $target ]]` 不加引号也对：第 10.1 节实测过 `[[ ]]` 的左边不拆词
  （`x="a b"` 时 `[[ $x == "a b" ]]` 不加引号 rc=0）。这里它判 `name with space.log` 是普通文件 → rc=0 → 不进兜底分支。
* `grep -c '[^[:space:]]' "$target"` 的引号**不能省**：那是把值交给**外部程序**，
  参数在进入 `grep` 之前就已经被第 1 层拆好了（第 8.4 节的全部事故都在这一层）。
  省掉引号，`grep` 会收到两个参数 `name` 和 `with`、`space.log`，于是报「No such file or directory」。

> ★ 这一行就是整章的落点：**同一个变量，在 `[[ ]]` 里可以裸，在外部命令前必须裹**。
> 不是风格偏好，是「谁在解析它」的差别 —— `[ ]`/外部命令受第 1 层摆布，`[[ ]]` 不受（第 8.1 节 `type -a`）。

第 9 档另外两行打印也各有一条实测依据：

* `管道版计数：1` —— `grep … | wc -l` 的输出只有 `wc` 的码进 `$?`（第 12 节第 6 组：`true | false` → `$?`=1，
  `false | true` → `$?`=0，`$?` 只是**最后一段**的）。
* `PIPESTATUS=[0 0]  ps[0]=0(grep) ps[1]=0(wc)` —— 两段的码都在数组里；
  `ps=("${PIPESTATUS[@]}")` 紧跟在管道后面一句（第 12 节第 6 组实测过 `[0 127 0]` 这种中间段炸掉而 `$?`=0 的现场）。
  本次实测到的是「两段都为 0」这个组合；「grep 那段返回 1」的组合在第 12 节第 6 组里测过（`[0 1]`）。

还有一点：`-v` 必须写在**文件参数之前**。因为选项分派是 `while [ $# -gt 0 ]` + `case` + `*) break`，
遇到第一个不以 `-` 开头的参数就停下来把它当文件名。
捕获里只跑了 `-v` 在前这一种（第 9 档）；`decide.sh name.log -v` 这种写法**没跑过**，
按 `case` 的实测行为它会被当成第三个参数，从而掉进第 7 档（参数过多 → rc=2）——
这一句是推导，不是实测，请自己跑一次再信。

### 15.5 这个 capstone 仍然留着的三个缺口（别当成已验证）

1. **`grep_rc -gt 1` 那一档从没被触发过**。九档里没有任何一次是「文件存在但读不了」——
   沙箱里造过权限 000 的 `no_read`（第 9.2 节实测 `-r no_read` → rc=1），
   但 `decide.sh` 没拿它跑过。所以脚本里那条 `printf '读文件失败（grep rc=%s）'` 是**没被执行过的代码**。
2. **`-h` / `--help` 这条分支没跑过**（只跑了未知选项 `-x`）。
   而且它把「看帮助」也归到 2 档，这和 GNU 系统里「`--help` 成功退 0」的惯例相反 ——
   本章没有对这条惯例做任何实测，见「本章没演 / 没验证」。
3. **`[[ ! -f $target ]]` 对目录、对断链符号链接走同一句错误文案**。
   判据本身在第 9.1 节实测过（`-f adir` → rc=1、`-f link_broken` → rc=1），
   但 `decide.sh 某目录 1` 这样的调用本章没跑过，所以「目录也会退 2 并打印那句文案」是**从判据推出来的**，
   不是实测到的。

捕获的最后两行是 demo 自己收尾（跑完删沙箱；`--keep` 那条分支本章的捕获里没有）。
屏幕上那一行印的是完整的 `/tmp/bash03-随机6位`，按全书脱敏约定把整条路径折成 `<沙箱>` ——
那个随机后缀每跑必变，本章不拿它当证据：

```
[全部跑完] 上面每一段都是真实执行；README.md 引用的输出全部出自这次运行（同一份捕获）。

[沙箱已删除] <沙箱>
```

> ★ 一个 40 行的小工具就能看出本章全部规则不是清单体：
> 判据选错（`[ ]` 裸变量）会在**第 1 层**坏掉，写法选错（`-a/-o`、裸括号）会在**解析层**坏掉，
> 读者选错（把 bash 糖交给 sh）会在**分支层**坏掉，而退出码分档选错会在**约定层**坏掉 ——
> 四层里只有约定层是给人看的，另外三层都是机器决定的。

