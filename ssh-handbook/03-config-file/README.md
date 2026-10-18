# 03 SSH 配置文件：一次配好，之后只敲别名

> 一句话：`~/.ssh/config` 是「给每个主机名预设一串命令行参数」的文件；
> 它的规则只有一条真正重要 —— **同一个关键字，第一个拿到的值生效，后面的静默作废**。
> 本章所有结论都用 `ssh -G 主机名` 离线打出来：它只读配置、不建连接、不碰私钥，所以可以放心敲。

本章对应的复现脚本是 [`demo.sh`](demo.sh)。下面每一段终端输出都是**同一次真跑**的原文，
来自 `bash demo.sh > ~/.cache/handbook-captures/ssh03.txt 2>&1`（1342 行，退出码 0），沙箱目录 `/tmp/ssh03-LUWym7`，
三个假端口 `59323`（空着）/ `60565`（只发一行横幅）/ `59679`（接了不说话）。
你自己跑一遍，路径和端口会变，**内容和行序不会变**。
凡是需要「能登录成功的服务器」才能看到的输出，本章一律标 **未验证**（见文末清单）。
还有一处**全书统一的脱敏替换**：`ssh -G` 会把「没配 `User` 时的默认用户名」「`%u` / `%L` / `%l` 展开出来的本机身份」
`%d` 展开出来的家目录，还有 `ls -l` 的属主列原样打印到屏幕上。这些是 ssh 从**口令表**里取的，demo 拦不住、
也没法用假名跑（`-l` 只改远端用户，改不了本机用户）。所以本章把这几处一律写成
**`<本机用户>` / `<主机名>` / `<家目录>`**，其余围栏逐行原样。你重跑那几行会看到你自己的名字。

---

## 你会遇到的场景

| 症状 / 需求 | 本章给你的答案 | 在第几节 |
| --- | --- | --- |
| 每次都要 `ssh -p 2222 -i ~/.ssh/id_work user@10.0.0.7` | 写成别名，之后只敲 `ssh work` | 场景 9-1 |
| 配置明明写了，却没生效，也不报错 | first-obtained-wins：被前面某个块抢走了 | 场景 2 |
| `Host *` 写在文件开头，结果所有别名用户名都错了 | 通配先抢，具体别名永远轮不到 | 场景 2d / 2e |
| 想「除了这台，其余都走跳板」 | `Host * !prod` 或 `Match !host prod` | 场景 3b / 4a |
| 不知道最终到底用了哪个用户/哪把钥匙/哪个跳板 | `ssh -G 别名 \| grep -E '^(user\|identityfile\|proxyjump) '` | 场景 0 / 5 |
| 内网机不开公网端口，只能经跳板机 | `ProxyJump`（别再手写 `ProxyCommand`） | 场景 9-2 |
| scp / rsync / git clone 也要走同一条路 | 配置里的 `ProxyJump` 会被这些工具一起继承 | 场景 9-2 |
| 反复登录同一台机，握手很慢 | `ControlMaster/ControlPath/ControlPersist` | 场景 9-3 |
| 配置越写越长，多人改同一个文件老冲突 | `Include ~/.ssh/config.d/*` 分片 | 场景 7 |
| `Bad owner or permissions` 一声炸 | 被 Include 的文件其他人可写就会被拒 | 场景 8 |
| 改一行配置就报 `Bad configuration option` | 关键字拼错 = 整个文件作废，不是跳一行 | 场景 10 |

---

## 心智模型

### 1）配置从哪儿来，谁压谁

```
     优先级高 ────────────────────────────────► 优先级低

  ┌──────────────┐   ┌──────────────────┐   ┌────────────────────────┐
  │ 命令行 -o / -l│   │ ~/.ssh/config     │   │ /etc/ssh/ssh_config     │
  │ / -p / -F    │   │ （或 -F 指定的文件）│   │ + /etc/ssh/ssh_config.d │
  └──────────────┘   └──────────────────┘   └────────────────────────┘
        ▲                    ▲                        ▲
        │                    │                        │
   每次都赢            只有它没被抢时   Debian/Ubuntu 在这里塞了
   （手册第 1 条）      才轮到它        SendEnv / HashKnownHosts 等
```

手册原文（`man ssh_config` 开头，场景 1 里真跑打印）：

```
       ssh(1) obtains configuration data from the following sources in the fol‐
       lowing order:

	     1.	  command-line options
	     2.	  user's configuration file (~/.ssh/config)
	     3.	  system-wide configuration file (/etc/ssh/ssh_config)
```

**`-F` 是把整条读取链换掉**，不是「加一个文件」：一旦给了 `-F`，`~/.ssh/config` 和
`/etc/ssh/ssh_config` 都不再读。所以本章（和所有安全实验）都用 `-F <沙箱文件>`，
这样你机器上的真实配置不会被牵连，也不会牵连别人。

```
$ ssh -F /dev/null -vvv -G box-a 2>&1 | grep 'Reading configuration data'
debug1: Reading configuration data /dev/null
```
一次都没有 `~/.ssh/config`、没有 `/etc/ssh/ssh_config` —— 这就是隔离的全部原理。

### 2）first-obtained-wins：本章唯一需要背下来的规则

手册第二句原文：

```
       Unless  noted  otherwise,  for  each parameter, the first obtained value
       will be used.
```

「每个参数只取第一个拿到的值」，配完一整天的坑 90% 都能用这一句解释。

```
文件从上往下读，遇到一个「匹配当前主机名」的块就把它的关键字往下灌：

   Host *                     ← 写在前面 = 它先抢
     User wildcard-first          user  ← 已被设上
   Host box-a                 ← 后到的同名关键字，静默丢弃
     User box-user                  ✗ 作废（不报错、不警告）
     HostName 10.0.0.1        ← Host * 没写过 HostName，所以这条生效
                                  ✓ hostname 10.0.0.1

   ⇒ 同一个块里「一半生效一半不生效」，这就是最难查的那种症状
```

关键字分三类（本章全部实测）：

| 类型 | 行为 | 代表 |
| --- | --- | --- |
| 覆盖型（手册说的 "Unless noted otherwise"） | 第一个生效，后面静默丢 | `User` `HostName` `Port` `ProxyJump` `ControlPath` `ServerAliveInterval` |
| **叠加型** | 写几条列几条，顺序就是使用顺序 | `IdentityFile`（试钥匙顺序）、`LocalForward`/`RemoteForward`、`SendEnv` |
| **可追加型**（值前面带符号） | `+` 追加到默认列表尾部、`-` 从默认删、`^` 放头部 | `Ciphers` `MACs` `PubkeyAcceptedAlgorithms` |

```
$ ssh -G -F /tmp/ssh03-LUWym7/cfg4e anyhost 2>/dev/null | grep -E '^(user|hostname|ciphers) '
user final-user
hostname anyhost
ciphers chacha20-poly1305@openssh.com,aes128-gcm@openssh.com,aes256-gcm@openssh.com,aes128-ctr,aes192-ctr,aes256-ctr,3des-cbc
```
`3des-cbc` 落在**列表末尾**，默认协商永远轮不到它 —— 「追加」不等于「优先」。

### 3）一个主机名进来，ssh 做了什么

```
  你在命令行敲：  ssh box-a
                    │
                    ▼
        ┌───────────────────────┐
        │ 第一遍解析（连接之前） │
        │ 逐行读文件，从上到下   │
        └───────────┬───────────┘
                    │  每一行看两件事：
                    │  ① 在哪个块里？块的头一行是 Host / Match
                    │  ② 这个块匹配当前主机名吗？
                    ▼
   Host 行：对你【敲的名字】做 glob 匹配（* ? ! ，多个模式用空格分开）
   Match 行：按条件求值（host / user / localuser / exec / all / final / canonical / !xxx）
                    │
                    │  匹配 ⇒ 块内关键字逐个尝试「第一个拿到就锁定」
                    ▼
        ┌───────────────────────┐
        │ 需要连接时才算的条件   │  Match addr / localnetwork  ← -G 阶段算不出来
        │ CanonicalizeHostname？ │  是 ⇒ 把短名补成 FQDN，再解析一遍
        └───────────┬───────────┘
                    ▼
        ┌───────────────────────┐
        │ 第二遍解析（final 遍） │  Match final / canonical 只在这遍成立
        └───────────┬───────────┘
                    ▼
       最终值 = 每个关键字第一次拿到的那个（叠加型 = 全部，按顺序）
```

`ssh -G` 就是把这个过程的产物打印出来就退出。所以：

```
$ printf 'final 段被求值时读文件次数 = ' ; ssh -F /tmp/ssh03-LUWym7/cfg4e -vvv -G anyhost 2>&1 | grep -c 'Reading configuration data'
final 段被求值时读文件次数 = 2
```

两遍解析在日志里数得出来 —— 这也是「为什么 `Match final` 里改 `HostName` 没用」的根因（实测见场景 4e）。

### 4）`Host` 和 `Match` 的区别，一句话

| | `Host` | `Match` |
| --- | --- | --- |
| 匹配对象 | **你在命令行敲的那个名字** | 一堆条件（主机名、用户、本机用户、命令退出码…） |
| 多模式分隔 | **空格** | **逗号**（pattern-list） |
| 结束条件 | 遇到下一个 `Host` / `Match` | 遇到下一个 `Host` / `Match` |
| 会不会报错 | 模式看不懂就是普通字符，不报错 | 条件写错直接 `Bad Match condition`，整个文件作废 |

---

## 场景逐个实战

> 每一节的标题都对得上 `demo.sh` 里的 `scene "N：..."`，输出都取自 `/tmp/ssh03.txt` 同一次跑。
> 命令里的 `/tmp/ssh03-LUWym7` 是当次沙箱目录；`59323` 是当次「空端口」（什么都不监听）。

### 场景 0 · 先认识武器：`ssh -G 主机名`

想干什么：**不改任何东西**，就问一句「这个主机名最终会变成什么」。

```
$ ssh -G -F /dev/null demo.example 2>&1 | head -5
Pseudo-terminal will not be allocated because stdin is not a terminal.
host demo.example
user <本机用户>
hostname demo.example
port 22
```

逐行解读：
- 第一行 `Pseudo-terminal ...` 来自 **stderr**，是 `ssh -G` 的固有噪音（stdin 不是 tty 就来一句），
  跟配置无关。脚本里用 `2>&1 | grep -v '^Pseudo'` 保留真报错、只滤掉它；你手敲时忽略即可。
- `host demo.example` = 你在命令行敲的那个名字（`Host` 匹配的就是它，不是 `HostName`）。
- `user <本机用户>` = 没配 User 时的默认值 = 你的操作系统用户名。
  这里打印的是 ssh 从本机口令表里取出来的**真实登录名**，为了不把它印进书里，本章统一写成 `<本机用户>`；
  你重跑这一行会看到你自己的名字（同理：`<家目录>` 是 `%d` / `/home/<你>` 那一列，`<主机名>` 是 `%l` / `%L`）。
- `hostname demo.example` = 没配 HostName 时，真连的地址就是你敲的名字。
- `port 22` = 默认端口。

它一共打 86 行（`ssh -G -F /dev/null demo.example 2>/dev/null | wc -l` → `86`），
里面几乎包含 ssh 认识的每一个关键字 —— 所以它是「最终生效值」的唯一权威视图。

**为什么说它安全**（这是本章敢让你随便敲的原因）：

```
$ ssh -F /dev/null -vvv -G demo.example 2>&1 | grep -E 'Reading configuration data|loaded pubkey|identity file'
debug1: Reading configuration data /dev/null
```

只有一行 `Reading configuration data`：**没有 `loaded pubkey`、没有 `identity file`** ——
`-G` 连私钥文件的门都不摸。真去连接时日志马上会出现 `debug1: identity file ...` 这种行，
所以本章把「会连接」的实验全部关在 `127.0.0.1` 的高端口上（见场景 6）。

还有一条容易慌的：`-G` 的输出里会出现你**真实 `~/.ssh` 的路径字符串**：

```
$ ssh -G -F /dev/null demo.example 2>/dev/null | grep -E '^(hostname|user|userknownhostsfile|identityfile) '
user <本机用户>
hostname demo.example
identityfile ~/.ssh/id_rsa
identityfile ~/.ssh/id_ecdsa
identityfile ~/.ssh/id_ecdsa_sk
identityfile ~/.ssh/id_ed25519
identityfile ~/.ssh/id_ed25519_sk
userknownhostsfile <家目录>/.ssh/known_hosts <家目录>/.ssh/known_hosts2
```

那是**编译进去的默认值被打印出来**，不是去读了它们（上面 `-vvv` 已经证明只读了一个文件）。

### 场景 1 · 配置文件在哪、谁压谁

想干什么：确认「命令行 > 配置文件 > 系统配置」这个顺序，并确认 `-F` 真的把你自己的配置隔开了。

配置文件（沙箱主文件，场景 1a 生成）：

```
     1	# 沙箱主配置（本 demo 唯一被 -F 指向的主文件）
     2	Host box-a
     3	  HostName 127.0.0.1
     4	  Port 59323
     5	  User box-user
     6	  IdentityFile /tmp/ssh03-LUWym7/keys/id_demo
     7	  IdentitiesOnly yes
     8	  UserKnownHostsFile /tmp/ssh03-LUWym7/kh
```

① `-o` 永远赢：

```
$ ssh -F /tmp/ssh03-LUWym7/config -G box-a 2>/dev/null | grep -E '^(host|hostname|port|user|identitiesonly) '
host box-a
user box-user
hostname 127.0.0.1
port 59323
identitiesonly yes

$ ssh -F /tmp/ssh03-LUWym7/config -o User=from-command-line -G box-a 2>/dev/null | grep -E '^user '
user from-command-line
```

② `-F` 一指定，读取链整条被换掉（数一下读文件的次数）：

```
$ printf '沙箱配置跑一遍，读文件次数 = ' ; ssh -F /tmp/ssh03-LUWym7/config -vvv -G box-a 2>&1 | grep -c 'Reading configuration data'
沙箱配置跑一遍，读文件次数 = 1

$ ssh -F /dev/null -vvv -G box-a 2>&1 | grep 'Reading configuration data'
debug1: Reading configuration data /dev/null
```

③ `/etc/ssh/ssh_config` 只读观察（本章一个字都不写它）。它自己的 `Host *` 会改默认值：

```
$ grep -vE '^[[:space:]]*#|^[[:space:]]*$' /etc/ssh/ssh_config | head -14
Include /etc/ssh/ssh_config.d/*.conf
Host *
    SendEnv LANG LC_* COLORTERM NO_COLOR
    HashKnownHosts yes
    GSSAPIAuthentication yes
```

```
$ ssh -G -F /etc/ssh/ssh_config box-a 2>/dev/null | grep -E '^(hashknownhosts|gssapiauthentication|sendenv|stricthostkeychecking) '
gssapiauthentication yes
hashknownhosts yes
stricthostkeychecking ask
sendenv LANG
sendenv LC_*
sendenv COLORTERM
sendenv NO_COLOR

$ ssh -G -F /tmp/ssh03-LUWym7/config box-a 2>/dev/null | grep -E '^(hashknownhosts|gssapiauthentication) '
gssapiauthentication no
hashknownhosts no
```

**同一个主机名，两份配置给出相反答案** —— 这就是「发行版给你的默认值」和「上游默认值」的差距，
也是「为什么我这台 `HashKnownHosts` 是 yes 别人是 no」的答案。

顺手能看到系统里塞的机关（`/etc/ssh/ssh_config.d/` 里那个 systemd 分片）：

```
$ ssh -G -F /etc/ssh/ssh_config 'unix/run/ssh-unix-local/socket' 2>/dev/null | grep -E '^(proxycommand|proxyusefdpass|stricthostkeychecking|userknownhostsfile|checkhostip) '
checkhostip no
proxyusefdpass yes
stricthostkeychecking false
userknownhostsfile /dev/null
proxycommand /usr/lib/systemd/systemd-ssh-proxy %h %p
```

主机名写成 `unix/…`、`machine/…`、`vsock/…` 时走本地 socket，而且**自带 `StrictHostKeyChecking no` +
`UserKnownHostsFile /dev/null`** —— 这解释了「为什么有人连 `machine/foo` 从来不问 yes/no」。
想关掉它，就在你自己的 `~/.ssh/config` 里覆盖（你的文件优先级更高）。

> 不加 `-F` 时是三层都读（你的 + 系统的 + 系统分片）。**本手册红线不读你真实的 `~/.ssh/config`，
> 所以这一条【不跑】**；你想亲眼看到三行 `Reading configuration data`，自己在终端敲
> `ssh -vvv -G somehost 2>&1 | grep "Reading configuration data"`。

### 场景 2 · first-obtained-wins（全章最重要的一节）

① 同一个关键字写两次，第二句**静默消失**：

```
$ printf 'Host dup\n  User first-user\n  User second-user\n  Port 2200\n  Port 2201\n' > /tmp/ssh03-LUWym7/cfg2a

$ ssh -G -F /tmp/ssh03-LUWym7/cfg2a dup 2>/dev/null | grep -E '^(user|port) '
user first-user
port 2200
```

没有 warning、没有报错，退出码 0。**这是「我明明写了却不生效」的第一答案。**

② 规则是**按关键字**，不是按块：两个同名 `Host dup` 块，各写不同关键字，全都生效。

```
$ printf 'Host dup\n  User first-user\n\nHost dup\n  HostName 10.0.0.9\n  Port 2200\n' > /tmp/ssh03-LUWym7/cfg2b

$ ssh -G -F /tmp/ssh03-LUWym7/cfg2b dup 2>/dev/null | grep -E '^(user|hostname|port) '
user first-user
hostname 10.0.0.9
port 2200
```

③ `IdentityFile` 是**叠加**的（唯一你会天天遇到的例外）：

```
$ ssh -G -F /tmp/ssh03-LUWym7/cfg2c addi 2>/dev/null | grep -E '^(identityfile|identitiesonly) '
identitiesonly yes
identityfile /tmp/ssh03-LUWym7/keys/id_first
identityfile /tmp/ssh03-LUWym7/keys/id_second
```

列出顺序 = 递钥匙顺序。配合 `IdentitiesOnly yes` 才「只递这些把」，否则 agent 里的钥匙照样会被端出去。

④ 典型错法：`Host *` 写在文件**最前面**

```
$ printf 'Host *\n  User wildcard-first\n\nHost box-a\n  User box-user\n  HostName 10.0.0.1\n' > /tmp/ssh03-LUWym7/cfg2d

$ ssh -G -F /tmp/ssh03-LUWym7/cfg2d box-a 2>/dev/null | grep -E '^(user|hostname) '
user wildcard-first
hostname 10.0.0.1
```

`user` 被通配抢走，`hostname` 却生效了（因为 `Host *` 那块没写 HostName）。
**一半生效一半不生效**，是这一章最难查的症状长相：你以为别名块在起作用，其实用户名早就被前面定死了。

⑤ 正解：具体别名在前，`Host *` 兜底在最后

```
$ printf 'Host box-a\n  User box-user\n  HostName 10.0.0.1\n\nHost *\n  User wildcard-last\n' > /tmp/ssh03-LUWym7/cfg2e

$ ssh -G -F /tmp/ssh03-LUWym7/cfg2e box-a 2>/dev/null | grep -E '^(user|hostname) '
user box-user
hostname 10.0.0.1
```

手册那句 `more host-specific declarations should be given near the beginning of the file, and general
defaults at the end.` 说的就是这件事（场景 1 里已打印原文）。

⑥ 「给某台机器开小灶」也一样：先设的把后面的挡住。

```
$ ssh -G -F /tmp/ssh03-LUWym7/cfg2h longline 2>/dev/null | grep -E '^(serveraliveinterval|forwardagent) '
serveraliveinterval 0
forwardagent no

$ ssh -G -F /tmp/ssh03-LUWym7/cfg2h2 longline 2>/dev/null | grep -E '^(serveraliveinterval|forwardagent) '
serveraliveinterval 60
forwardagent yes
```

两份文件内容完全一样，只是**块顺序相反**：`Host *` 在前的那份把 `longline` 的小灶全挡掉了。

⑦ 机制层证据：`-vvv` 给每个命中的块打一行，还带**行号**。

```
$ ssh -F /tmp/ssh03-LUWym7/config -vvv -G box-a 2>&1 | grep -E 'Reading configuration|Applying options|checking match' | head -5
debug1: Reading configuration data /tmp/ssh03-LUWym7/config
debug1: /tmp/ssh03-LUWym7/config line 2: Applying options for box-a
```

排查「到底哪一块在生效」就靠这两行：只看 `Applying options for ...` 出现的**顺序**，
第一个出现的块就是把值抢走的那个。

⑧ 关键字拼错不是「跳过一行」，是**整个文件作废**：

```
$ printf 'Host typo\n  Usermame learner\n' > /tmp/ssh03-LUWym7/cfg2g; ssh -G -F /tmp/ssh03-LUWym7/cfg2g typo 2>&1 | grep -v '^Pseudo'
/tmp/ssh03-LUWym7/cfg2g: line 2: Bad configuration option: usermame
/tmp/ssh03-LUWym7/cfg2g: terminating, 1 bad configuration options

$ ssh -G -F /tmp/ssh03-LUWym7/cfg2g typo >/dev/null 2>&1; echo "真实退出码=$?"
真实退出码=255
```

注意报错里关键字被**转成全小写**（`usermame`），拼写检查时别只看驼峰。
退出码 255 = ssh 直接罢工，连 `-G` 都不给你结果 —— 所以改完配置第一件事是 `ssh -G`，
让它在**不影响任何连接**的情况下把这种致命错先撞出来。

### 场景 3 · `Host` 的模式匹配：`*`、`?`、`!`、空格、逗号

模式语法手册里只有三个符号（`man ssh_config` PATTERNS 段，场景 3a 打印的原文）：

```
       A pattern consists of zero or more  non-whitespace  characters,	‘*’  (a
       wildcard	 that matches zero or more characters), or ‘?’ (a wildcard that
       matches exactly one character).
```

① `?` 一个字符、`*` 任意个：

```
$ printf 'Host web?\n  User one-char\nHost web*\n  User star\n' > /tmp/ssh03-LUWym7/cfg3a

$ for h in web1 web12 webx web; do printf '%-8s ' "$h"; ssh -G -F /tmp/ssh03-LUWym7/cfg3a "$h" 2>/dev/null | grep -m1 '^user '; done
web1     user one-char
web12    user star
webx     user one-char
web      user star
```

两个块都命中 `web1`（`?` 和 `*` 都匹配它），但 `user one-char` 赢了 —— 又是 first-wins。

② `!` 取反：

```
$ printf 'Host * !secret-www\n  User wildcard-except\nHost secret-www\n  User secret-user\n' > /tmp/ssh03-LUWym7/cfg3b

$ for h in secret-www secret-db laptop; do printf '%-12s ' "$h"; ssh -G -F /tmp/ssh03-LUWym7/cfg3b "$h" 2>/dev/null | grep -m1 '^user '; done
secret-www   user secret-user
secret-db    user wildcard-except
laptop       user wildcard-except
```

「除了这台全都走」的正确写法是 `Host * !例外`，**正项 `*` 不能省**：

```
$ printf 'Host !h1 !h2\n  User neg-only\n' > /tmp/ssh03-LUWym7/cfg3b2; for h in h1 h3; do printf '%-4s ' "$h"; ssh -G -F /tmp/ssh03-LUWym7/cfg3b2 "$h" 2>/dev/null | grep -m1 '^user '; done
h1   user <本机用户>
h3   user <本机用户>

$ printf '>>> 补一个 * 之后（Host 行里的多个模式是空格分隔的）：\n'; printf 'Host !h1 !h2 *\n  User neg-plus-star\n' > /tmp/ssh03-LUWym7/cfg3b3; for h in h1 h3; do printf '%-4s ' "$h"; ssh -G -F /tmp/ssh03-LUWym7/cfg3b3 "$h" 2>/dev/null | grep -m1 '^user '; done
>>> 补一个 * 之后（Host 行里的多个模式是空格分隔的）：
h1   user <本机用户>
h3   user neg-plus-star
```

全是 `!` 的一行永远不成立（手册原话：`a negated match will never produce a positive result by itself`），
而且不报错 —— `Host` 块写了跟没写一样。

③ 逗号在 `Host` 行里**不是分隔符**（这是抄别人配置片段时最容易带进来的 bug）：

```
$ printf 'Host a1 a2 a3\n  User spaced\n' > /tmp/ssh03-LUWym7/cfg3c

$ for h in a1 a2 a3 a4; do printf '%-4s ' "$h"; ssh -G -F /tmp/ssh03-LUWym7/cfg3c "$h" 2>/dev/null | grep -m1 '^user '; done
a1   user spaced
a2   user spaced
a3   user spaced
a4   user <本机用户>
```

```
$ printf 'Host c1,c2\n  User comma-is-literal\n' > /tmp/ssh03-LUWym7/cfg3c2; for h in c1 c2; do printf '%-4s ' "$h"; ssh -G -F /tmp/ssh03-LUWym7/cfg3c2 "$h" 2>/dev/null | grep -m1 '^user '; done; printf '>>> 那直接把逗号敲进主机名呢：\n'; ssh -G -F /tmp/ssh03-LUWym7/cfg3c2 'c1,c2' 2>&1 | grep -v '^Pseudo'
c1   user <本机用户>
c2   user <本机用户>
>>> 那直接把逗号敲进主机名呢：
hostname contains invalid characters
```

`Host c1,c2` 是一条叫 `c1,c2` 的模式；主机名里又不允许出现逗号 ⇒ **这一条永远配不上，还一声不吭**。
再看 `Host b1, b2`（逗号后带空格）就更清楚了：

```
$ printf 'Host b1, b2\n  User two-patterns-one-broken\n' > /tmp/ssh03-LUWym7/cfg3c3; for h in b1 b2; do printf '%-4s ' "$h"; ssh -G -F /tmp/ssh03-LUWym7/cfg3c3 "$h" 2>/dev/null | grep -m1 '^user '; done
b1   user <本机用户>
b2   user two-patterns-one-broken
```

被空格切成两个模式 `b1,` 和 `b2`：`b2` 命中，`b1` 配不上（它要找的是带逗号的 `b1,`）。
**结论：`Host` 行多个模式只用空格；`Match host` 多个模式只用逗号（场景 4a）。**

④ `*.example.com` 不匹配裸 `example.com`（`*` 要求至少有一个点前的内容）：

```
$ for h in example.com a.example.com a.b.example.com; do printf '%-18s ' "$h"; ssh -G -F /tmp/ssh03-LUWym7/cfg3d "$h" 2>/dev/null | grep -m1 '^user '; done
example.com        user <本机用户>
a.example.com      user subdomain-only
a.b.example.com    user subdomain-only
```

想连裸域名一起罩住就写 `Host example.com *.example.com`。

⑤ 语法宽容度：关键字大小写不敏感、可以用 `=`、缩进随意（Tab 也行）、值能加引号：

```
$ for h in case eq tab quoted; do printf '%-8s ' "$h"; ssh -G -F /tmp/ssh03-LUWym7/cfg3e "$h" 2>/dev/null | grep -m1 '^user '; done
case     user LowerKey
eq       user eq-user
tab      user tab-user
quoted   user 带空格的用户
```

⑥ 最容易被懵：**`Host` 匹配的是你敲的名字，不是 `HostName` 指向的真名**。

```
$ printf 'Host myalias\n  HostName real.server.name\n\nHost real.server.name\n  User only-if-you-type-the-real-name\n' > /tmp/ssh03-LUWym7/cfg3f

$ printf '>>> 敲别名 myalias:\n'; ssh -G -F /tmp/ssh03-LUWym7/cfg3f myalias 2>/dev/null | grep -E '^(host|hostname|user) '
>>> 敲别名 myalias:
host myalias
user <本机用户>
hostname real.server.name

$ printf '>>> 敲真名 real.server.name:\n'; ssh -G -F /tmp/ssh03-LUWym7/cfg3f real.server.name 2>/dev/null | grep -E '^(host|hostname|user) '
>>> 敲真名 real.server.name:
host real.server.name
user only-if-you-type-the-real-name
hostname real.server.name
```

同一个文件、同一台机器，敲别名和敲真名拿到**两套不同的配置**。
推论很实用：`known_hosts` 记的是**真名**（`HostName`），所以改别名不影响主机密钥校验；
改 DNS 指向才会触发 `REMOTE HOST IDENTIFICATION HAS CHANGED`。

### 场景 4 · `Match`：条件块（以及它会真的执行你的命令）

可用的条件关键字，手册一次性列全（场景 4a 打印的原文）：

```
	       matches.	 The available criteria keywords are: canonical, final,
	       exec,  localnetwork,  host, originalhost, tagged, command, user,
	       localuser, and version.	The all criteria must appear  alone  or
	       immediately  after  canonical  or  final.  Other criteria may be
	       combined arbitrarily.  All  criteria  but  all,	canonical,  and
	       final  require an argument.  Criteria may be negated by prepend‐
	       ing an exclamation mark (‘!’).
```

① `Match host` 用**逗号**分隔多个模式：

```
$ for h in w1 w2 w3 prod.db.internal; do printf '%-18s ' "$h"; ssh -G -F /tmp/ssh03-LUWym7/cfg4a "$h" 2>/dev/null | grep -m1 '^user '; done
w1                 user match-host-list
w2                 user match-host-list
w3                 user <本机用户>
prod.db.internal   user db-user
```

写成空格就直接 fatal（注意报错里还带行号）：

```
$ printf 'Match host w1 w2\n  User space-is-wrong\n' > /tmp/ssh03-LUWym7/cfg4a2; ssh -G -F /tmp/ssh03-LUWym7/cfg4a2 w1 2>&1 | grep -v '^Pseudo'
/tmp/ssh03-LUWym7/cfg4a2 line 1: missing argument for Match 'w2'
/tmp/ssh03-LUWym7/cfg4a2 line 1: Bad Match condition
/tmp/ssh03-LUWym7/cfg4a2: terminating, 1 bad configuration options
```

② `Match user` 看**登录用的用户名**（会被 `User` 或 `-l` 改写）；`Match localuser` 才看你的操作系统用户名：

```
$ printf '>>> -l alice: '; ssh -G -F /tmp/ssh03-LUWym7/cfg4b -l alice h 2>/dev/null | grep -m1 '^forwardagent '
>>> -l alice: forwardagent yes

$ printf '>>> -l bob  : '; ssh -G -F /tmp/ssh03-LUWym7/cfg4b -l bob h 2>/dev/null | grep -m1 '^forwardagent '
>>> -l bob  : forwardagent no
```

```
$ printf 'Match localuser %s\n  User by-localuser\n' "$(id -un)" > /tmp/ssh03-LUWym7/cfg4b2; printf '>>> 不带 -l，本机用户 %s 命中: ' "$(id -un)"; ssh -G -F /tmp/ssh03-LUWym7/cfg4b2 h 2>/dev/null | grep -m1 '^user '
>>> 不带 -l，本机用户 <本机用户> 命中: user by-localuser

$ printf 'Match localuser somebody-who-is-not-me\n  User by-localuser\n' > /tmp/ssh03-LUWym7/cfg4b3; printf '>>> localuser 写别人（不命中，退回默认）: '; ssh -G -F /tmp/ssh03-LUWym7/cfg4b3 h 2>/dev/null | grep -m1 '^user '
>>> localuser 写别人（不命中，退回默认）: user <本机用户>
```

`localuser` 和 `user` 只差中间那个字母：不写 `-l` 时两者恰好相等，很容易误以为是一回事。
上面那两条里 `本机用户 <本机用户>` 与 `退回默认）: user <本机用户>` 两处，是 ssh 从**本机口令表**展开出来的真实登录名：
`Match localuser` 只能用你自己那个名字才配得上（OpenSSH 拿 `getpwuid` 比对，`-l` 只改远端用户、改不了本机用户），
所以这一类行**没法用假名跑**，本章的处理办法是原样重跑、再把名字换成占位符 `<本机用户>`，你重跑会看到你自己的名字。
再加一条边界：

```
$ printf 'Match localuser %s\n  User by-localuser\n' "$(id -un)" > /tmp/ssh03-LUWym7/cfg4b4; printf '>>> 命令行 -l bob 优先级最高，Match 抢不过它: '; ssh -G -F /tmp/ssh03-LUWym7/cfg4b4 -l bob h 2>/dev/null | grep -m1 '^user '
>>> 命令行 -l bob 优先级最高，Match 抢不过它: user bob
```

命令行的值最先被「拿到」，所以 `Match` 块里改 `User` 永远抢不过 `-l`。
两个条件别混：`Match user` 判定的是**最终登录名**（会被 `-l` / `User` 改写，上面 alice/bob 两组已演示），
`Match localuser` 判定的是**你操作系统账号**（`-l` 改不动它）。

③ `Match all` 无条件成立，`Match !host xxx` 是「不是这台」；它们一样要排队等 first-wins：

```
$ printf 'Host m1\n  User target-user\nMatch host m1\n  ForwardAgent yes\nMatch !host m1\n  User fallback-user\n' > /tmp/ssh03-LUWym7/cfg4c

$ for h in m1 m2; do printf '>>> %s : ' "$h"; ssh -G -F /tmp/ssh03-LUWym7/cfg4c "$h" 2>/dev/null | grep -E '^(user|forwardagent) ' | tr '\n' ' '; printf '\n'; done
>>> m1 : user target-user forwardagent yes
>>> m2 : user fallback-user forwardagent no
```

④ **`Match exec` 会在你本地真的执行命令** —— 本章最重要的一条安全提醒：

```
$ printf 'Match exec "exit 7"\n  User should-not-appear\nMatch exec "true"\n  User by-exec\n' > /tmp/ssh03-LUWym7/cfg4d; printf 'exec "exit 7" 不匹配、exec "true" 匹配 -> '; ssh -G -F /tmp/ssh03-LUWym7/cfg4d anyhost 2>/dev/null | grep -m1 '^user '
exec "exit 7" 不匹配、exec "true" 匹配 -> user by-exec
```

```
$ printf 'Match exec "touch /tmp/ssh03-LUWym7/EXEC_RAN_BY_MINUS_G"\n  User exec-wrote-a-file\n' > /tmp/ssh03-LUWym7/cfg4d2; rm -f /tmp/ssh03-LUWym7/EXEC_RAN_BY_MINUS_G; ssh -G -F /tmp/ssh03-LUWym7/cfg4d2 anyhost 2>/dev/null | grep -m1 '^user '; ls -l /tmp/ssh03-LUWym7/EXEC_RAN_BY_MINUS_G
user exec-wrote-a-file
-rw-r--r-- 1 <本机用户> <本机用户> 0 Oct  5 12:43 /tmp/ssh03-LUWym7/EXEC_RAN_BY_MINUS_G
```

看清楚：**没有任何连接、没有登录，只是 `ssh -G`**，`touch` 就执行了、文件就出现了。
exec 还能吃 token，把「你敲的名字」传进本地命令：

```
$ printf 'Match exec "echo %%n=%%h:%%p >> /tmp/ssh03-LUWym7/EXEC_TOKENS_LOG"\n  User by-exec-token\n' > /tmp/ssh03-LUWym7/cfg4d3; rm -f /tmp/ssh03-LUWym7/EXEC_TOKENS_LOG; for h in box-a box-b; do ssh -G -F /tmp/ssh03-LUWym7/cfg4d3 "$h" >/dev/null 2>&1; done; cat -n /tmp/ssh03-LUWym7/EXEC_TOKENS_LOG
     1	box-a=box-a:22
     2	box-b=box-b:22
```

现实用法是 exec「问一个本地脚本：这台该不该走代理」。代价与风险（务必背下来）：
每一条 `ssh`（哪怕只是 `-G`）都会跑一次它；exec 的内容就是**本机代码执行入口** ——
别人发你的 config 片段、仓库里 `git clone` 下来的 `.ssh/config`，都不要直接 `Include`。

⑤ 两遍解析 + `Match final`：`-G` 会把文件读**两遍**，第二遍才求值 `final`：

```
$ printf 'final 段被求值时读文件次数 = ' ; ssh -F /tmp/ssh03-LUWym7/cfg4e -vvv -G anyhost 2>&1 | grep -c 'Reading configuration data'
final 段被求值时读文件次数 = 2

$ ssh -G -F /tmp/ssh03-LUWym7/cfg4e anyhost 2>/dev/null | grep -E '^(user|hostname|ciphers) '
user final-user
hostname anyhost
ciphers chacha20-poly1305@openssh.com,aes128-gcm@openssh.com,aes256-gcm@openssh.com,aes128-ctr,aes192-ctr,aes256-ctr,3des-cbc
```

同一份 `Match final all` 块里写了 `User` 和 `HostName`：`User` 改成功了，**`HostName` 没改**（还是 `anyhost`）。
这就是「第二遍已经太晚」的实测证据：连哪儿、走哪个代理在第一遍就定死了。
**把 `HostName` / `ProxyCommand` 写进 `Match final` 是无效配置** —— 这条本章实测到了，不用等真服务器。

⑥ `CanonicalizeHostname` + `Match canonical`：把短名补成 FQDN 后**再解析一遍配置**。

```
$ getent ahosts web01.localhost | head -2
::1             STREAM web01.localhost
::1             DGRAM

$ ssh -G -F /tmp/ssh03-LUWym7/cfg4f web01 2>/dev/null | grep -E '^(hostname|canonicalizehostname|canonicaldomains|user) '
user via-raw
hostname web01.localhost
canonicalizehostname true
canonicaldomains localhost
```

`web01` 被补成 `web01.localhost`（本机 `.localhost` 由 systemd-resolved 直接本地应答 `::1`，**没出网**；
换成你自己的域名才是真用法）。注意 `user` 落的是 `via-raw`：第一遍（还不 canonical）时
`Match !canonical` 先把 User 设上了，第二遍 `Match canonical` 再命中也轮不到 —— first-wins 的活教材。
想要 canonical 后的值生效，就把 `Match canonical` 写在**文件最前面**。

⑦ `CanonicalizeMaxDots`：点数超过它就跳过规范化（默认 1），「短名补域名、FQDN 别动」的开关：

```
$ ssh -F /tmp/ssh03-LUWym7/cfg4g -vvv -G web01.dc.corp 2>&1 | grep -E 'resolve_canonicalize|Canonicalized' | head -3
debug3: resolve_canonicalize: not canonicalizing hostname "web01.dc.corp" (max dots 1)
```

⑧ `Match addr` 在这个版本的客户端配置里已经不是可用条件了（对比上面 ①的可用列表，没有 `addr`），直接 fatal：

```
$ printf 'Match addr 127.0.0.1\n  User by-addr\n' > /tmp/ssh03-LUWym7/cfg4h; ssh -G -F /tmp/ssh03-LUWym7/cfg4h 127.0.0.1 2>&1 | grep -v '^Pseudo'
Unsupported Match attribute addr
/tmp/ssh03-LUWym7/cfg4h line 1: Bad Match condition
/tmp/ssh03-LUWym7/cfg4h: terminating, 1 bad configuration options
```

⑨ `Match` 段不会「吃掉」后面的 `Host` 块：下一个 `Host` 行重新起算（很多人以为会一直延续）：

```
$ printf 'Match host w1\n  User in-match\n\nHost w2\n  User w2-user\n' > /tmp/ssh03-LUWym7/cfg4i

$ for h in w1 w2 w3; do printf '>>> %-4s ' "$h"; ssh -G -F /tmp/ssh03-LUWym7/cfg4i "$h" 2>/dev/null | grep -m1 '^user '; done
>>> w1   user in-match
>>> w2   user w2-user
>>> w3   user <本机用户>
```

### 场景 5 · 常用关键字逐个看最终值（一份文件，五个块，全部指向 127.0.0.1 高端口）

这一节是「关键字字典 + `-G` 验收」，文件本体（`cat -n` 的原文，行号后面还要用）：

```
     1	# 沙箱主配置：本章所有 -G / 连接演示都走这份文件
     2	Host box-a
     3	  HostName 127.0.0.1
     4	  Port 59323
     5	  User box-user
     6	  IdentityFile /tmp/ssh03-LUWym7/keys/id_box
     7	  IdentitiesOnly yes
     8	  UserKnownHostsFile /tmp/ssh03-LUWym7/kh
     9	  StrictHostKeyChecking accept-new
    10	
    11	Host jumper
    12	  HostName 127.0.0.1
    13	  Port 59323
    14	  User jump-user
    15	  IdentityFile /tmp/ssh03-LUWym7/keys/id_jump
    16	  IdentitiesOnly yes
    17	  UserKnownHostsFile /tmp/ssh03-LUWym7/kh
    18	  ServerAliveInterval 30
    19	  ServerAliveCountMax 4
    20	  ConnectTimeout 10
    21	
    22	Host behind-jump
    23	  HostName 127.0.0.1
    24	  Port 59323
    25	  User inner-user
    26	  IdentityFile /tmp/ssh03-LUWym7/keys/id_demo
    27	  IdentitiesOnly yes
    28	  UserKnownHostsFile /tmp/ssh03-LUWym7/kh
    29	  ProxyJump jumper
    30	
    31	Host ssh-cat
    32	  HostName 127.0.0.1
    33	  Port 59323
    34	  User cat-user
    35	  IdentityFile /tmp/ssh03-LUWym7/keys/id_demo
    36	  IdentitiesOnly yes
    37	  UserKnownHostsFile /tmp/ssh03-LUWym7/kh
    38	  ProxyCommand ssh -F /tmp/ssh03-LUWym7/config -W %h:%p jumper
    39	
    40	Host mux
    41	  HostName 127.0.0.1
    42	  Port 59323
    43	  User mux-user
    44	  IdentityFile /tmp/ssh03-LUWym7/keys/id_demo
    45	  IdentitiesOnly yes
    46	  UserKnownHostsFile /tmp/ssh03-LUWym7/kh
    47	  ControlMaster auto
    48	  ControlPath /tmp/ssh03-LUWym7/cm/%r@%h:%p
    49	  ControlPersist 10m
    50	  ForwardAgent yes
```

① `HostName` / `Port` / `User` —— 别名的全部秘密就这三行：

```
$ ssh -G -F /tmp/ssh03-LUWym7/config box-a 2>/dev/null | grep -E '^(host|hostname|port|user|connecttimeout) '
host box-a
user box-user
hostname 127.0.0.1
port 59323
connecttimeout none
```

`connecttimeout none` = 没设，走内核默认。

② `IdentityFile` + `IdentitiesOnly` —— 只递这一把，别把 agent 里所有钥匙端上去（01 章 `MaxAuthTries` 的解药）：

```
$ ssh -G -F /tmp/ssh03-LUWym7/config box-a 2>/dev/null | grep -E '^(identityfile|identitiesonly) '
identitiesonly yes
identityfile /tmp/ssh03-LUWym7/keys/id_box

$ printf 'Host noio\n  IdentityFile /tmp/ssh03-LUWym7/keys/id_demo\n' > /tmp/ssh03-LUWym7/cfg5io; ssh -G -F /tmp/ssh03-LUWym7/cfg5io noio 2>/dev/null | grep -E '^(identityfile|identitiesonly) '
identitiesonly no
identityfile /tmp/ssh03-LUWym7/keys/id_demo
```

不写就是 `no`：`IdentityFile` 只是「额外加上这把」，agent 里其它钥匙照样会被递出去。

③ `ProxyJump` —— `-G` 里只是干干净净一行；真跑时 ssh 会把它展开成一条 `ProxyCommand`（场景 6e 实测）：

```
$ ssh -G -F /tmp/ssh03-LUWym7/config behind-jump 2>/dev/null | grep -E '^(host|hostname|port|user|proxyjump|proxycommand|proxyusefdpass) '
host behind-jump
user inner-user
hostname 127.0.0.1
port 59323
proxyusefdpass no
proxyjump jumper
```

注意 `proxycommand` 那一行**根本没出现** —— 展开是运行时才做的，`-G` 只告诉你「配了 ProxyJump」。
跳板机自己的参数要**单独查**，这是最常见的排错死角：

```
$ ssh -G -F /tmp/ssh03-LUWym7/config jumper 2>/dev/null | grep -E '^(host|hostname|port|user|identityfile|serveralive[a-z]+|connecttimeout) '
host jumper
user jump-user
hostname 127.0.0.1
port 59323
serveralivecountmax 4
serveraliveinterval 30
identityfile /tmp/ssh03-LUWym7/keys/id_jump
connecttimeout 10
```

④ `ProxyCommand` —— 里面的 `%h %p` 在 `-G` 里**原样不展开**（和下面的 `ControlPath` 正好相反）：

```
$ ssh -G -F /tmp/ssh03-LUWym7/config ssh-cat 2>/dev/null | grep -E '^(proxycommand|proxyusefdpass) '
proxyusefdpass no
proxycommand ssh -F /tmp/ssh03-LUWym7/config -W %h:%p jumper
```

⑤ `ServerAliveInterval` / `ServerAliveCountMax` / `TCPKeepAlive` —— 探测节奏三件套：

```
$ ssh -G -F /tmp/ssh03-LUWym7/config jumper 2>/dev/null | grep -E '^(serveraliveinterval|serveralivecountmax|tcpkeepalive) '
tcpkeepalive yes
serveralivecountmax 4
serveraliveinterval 30
```

节奏：空闲满 30 秒才发第一次探测，每 30 秒一次，连续 4 次没回音（≈120 秒）ssh 自己断开；
`ServerAliveInterval 0` = 关掉。`TCPKeepAlive` 是另一回事（靠内核探针，只能告诉你「管道断了」，
不能发现「对端进程卡死但内核还活着」），所以长跑会话靠 `ServerAlive*`。

⑥ `ControlMaster` / `ControlPath` / `ControlPersist` —— 这里 `%r %h %p` **被 `-G` 展开成真值**，`10m` 归一化成 600 秒：

```
$ ssh -G -F /tmp/ssh03-LUWym7/config mux 2>/dev/null | grep -E '^(controlmaster|controlpath|controlpersist|forwardagent) '
controlmaster auto
controlpath /tmp/ssh03-LUWym7/cm/mux-user@127.0.0.1:59323
forwardagent yes
controlpersist 600
```

`ssh -O`（复用控制命令）就是靠 `ControlPath` 找套接字的，没配它它直接告诉你没得查：

```
$ timeout 10 ssh -O check -F /tmp/ssh03-LUWym7/config box-a 2>&1 | head -2
No ControlPath specified for "-O" command

$ timeout 10 ssh -O check -F /tmp/ssh03-LUWym7/config mux 2>&1 | head -2
Control socket connect(/tmp/ssh03-LUWym7/cm/mux-user@127.0.0.1:59323): No such file or directory
```

第二条那句 `No such file or directory` = **还没有主连接**的标准样子（复用真正生效的输出见场景 9-3，那部分【未验证】）。

⑦ `StrictHostKeyChecking` / `UserKnownHostsFile` —— 把 known_hosts 钉在沙箱，绝不污染你真实那份：

```
$ ssh -G -F /tmp/ssh03-LUWym7/config box-a 2>/dev/null | grep -E '^(stricthostkeychecking|userknownhostsfile|hashknownhosts|updatehostkeys) '
hashknownhosts no
stricthostkeychecking accept-new
updatehostkeys false
userknownhostsfile /tmp/ssh03-LUWym7/kh
```

`accept-new` 是 7.4 之后推荐的折中：第一次见自动记下，之后一旦密钥变了**照样硬失败**；
`no` 是把中间人当不存在，别在生产上写。

⑧ token 在 `-G` 里的两副面孔（场景 6f 实测）：

```
$ printf 'Host tk\n  HostName 127.0.0.1\n  Port 59323\n  IdentityFile /tmp/ssh03-LUWym7/keys/id_%%r\n  UserKnownHostsFile /tmp/ssh03-LUWym7/kh/%%n@%%h:%%p_%%r_%%l_%%i_%%C_%%u_%%d\n  ControlPath /tmp/ssh03-LUWym7/cm/%%r@%%h:%%p\n  ProxyCommand nc %%h %%p\n' > /tmp/ssh03-LUWym7/cfg6f

$ ssh -F /tmp/ssh03-LUWym7/cfg6f -vvv -G tk 2>&1 | grep -E 'expanded'
debug3: expanded UserKnownHostsFile '/tmp/ssh03-LUWym7/kh/%n@%h:%p_%r_%l_%i_%C_%u_%d' -> '/tmp/ssh03-LUWym7/kh/tk@127.0.0.1:59323_<本机用户>_<本机用户>_1000_139afd57aa173e28826ec0538a112e9b1762fa24_<本机用户>/<家目录>'

$ ssh -G -F /tmp/ssh03-LUWym7/cfg6f tk 2>/dev/null | grep -E '^(identityfile|userknownhostsfile|controlpath|proxycommand) '
controlpath /tmp/ssh03-LUWym7/cm/<本机用户>@127.0.0.1:59323
identityfile /tmp/ssh03-LUWym7/keys/id_%r
userknownhostsfile /tmp/ssh03-LUWym7/kh/tk@127.0.0.1:59323_<本机用户>_<本机用户>_1000_139afd57aa173e28826ec0538a112e9b1762fa24_<本机用户>/<家目录>
proxycommand nc %h %p
```

- `UserKnownHostsFile`：有官方 `debug3: expanded ...` 一行，并且 `-G` 里已经是展开后的值。
- `ControlPath`：`-G` 里也展开了，但**不打那行日志**。
- `IdentityFile` / `ProxyCommand`：**原样留着 `%r` / `%h %p`** —— 它们要到真正用的那一刻才展开。
  所以别用 `-G` 判断这两类的值对不对，得用 `-v` 真跑（6d 里 `Executing proxy command: exec ... --127.0.0.1 59323` 就是那一刻）。
- 那个 `tk@…` 长串里 `<本机用户>_<主机名>` 和结尾的 `<本机用户>/<家目录>` 四处，依次是 `%r %l %u %d` 展开出来的**本机真实身份**，
  本章按脱敏约定换成了占位符；中间那串 40 位十六进制 `%C` **没有换**，它就是这次真跑打印的原值。
  `%C` = SHA1(`%l%h%p%r%j`)，本例没配 `User`、也没配 `ProxyJump`，所以四个输入是
  「本机名 + `127.0.0.1` + `59323` + 本机登录名 + 空串」；你在自己机器上把 `<主机名>` `<本机用户>` 换成真名字符串再取 SHA-1，
  就能复算出同一串（`printf '%s' "<主机名>127.0.0.159323<本机用户>" | sha1sum`）。
  派生值只能靠重跑得到，本章不手改它。

token 手册原文（`man ssh_config` TOKENS 段整段，场景 6f 打印），常用的这几个够用：

```
	     %%	   A literal ‘%’.
	     %C	   Hash of %l%h%p%r%j.
	     %d	   Local user's home directory.
	     %h	   The remote hostname.
	     %i	   The local user ID.
	     %j	   The contents of the ProxyJump option, or the empty string if
		   this option is unset.
	     %L	   The local hostname.
	     %l	   The local hostname, including the domain name.
	     %n	   The original remote hostname, as given on the command line.
	     %p	   The remote port.
	     %r	   The remote username.
	     %u	   The local username.
```

`%h`（`HostName` 的真名）和 `%n`（你敲的名字）不是一回事，做「按真名分文件」时用 `%h`，
做「按别名分文件」时用 `%n`。`%j` 实测（把跳板串直接拼进路径）：

```
$ printf 'Host tj\n  HostName 127.0.0.1\n  ProxyJump jumper\n  UserKnownHostsFile /tmp/ssh03-LUWym7/kh/j=[%%j]\n' > /tmp/ssh03-LUWym7/cfg6f2; ssh -G -F /tmp/ssh03-LUWym7/cfg6f2 tj 2>/dev/null | grep -E '^(userknownhostsfile|proxyjump) '
userknownhostsfile /tmp/ssh03-LUWym7/kh/j=[jumper]
proxyjump jumper
```

⑨ `-G` 打出来的是**全小写关键字**，和配置里的驼峰一一对应：

```
IdentityFile          ->  identityfile
StrictHostKeyChecking ->  stricthostkeychecking
ServerAliveCountMax   ->  serveralivecountmax
ControlPersist        ->  controlpersist
ProxyJump             ->  proxyjump
```

所以验收动作就是 `ssh -G 别名 | grep -E '^(你要查的关键字小写) '`。

### 场景 6 · 让 `-G` 和真实报错对上（三个【预期失败】）

**下面每一条都会失败，失败本身就是证据**：报错里的地址、端口、命令行 = 配置里的值。
端口是脚本刚向内核要的空闲端口，跟 22、跟外部网络毫无关系；每条会连接的命令都带同一组护栏
（`-o BatchMode=yes -o IdentityAgent=none -o IdentitiesOnly=yes -o IdentityFile=<沙箱key> -o UserKnownHostsFile=<沙箱文件>`）。

① 端口空着 ⇒ `Connection refused`，看 `Connecting to` 那行的端口是不是配置里那个：

```
$ timeout 10 ssh -F /tmp/ssh03-LUWym7/config -v -o BatchMode=yes -o IdentityAgent=none -o IdentitiesOnly=yes -o IdentityFile=/tmp/ssh03-LUWym7/keys/id_demo -o UserKnownHostsFile=/tmp/ssh03-LUWym7/kh box-a true 2>&1 | grep -E 'Applying options|Connecting to|connect to host' | head -4
debug1: /tmp/ssh03-LUWym7/config line 2: Applying options for box-a
debug1: Connecting to 127.0.0.1 [127.0.0.1] port 59323.
ssh: connect to host 127.0.0.1 port 59323: Connection refused
```

三行连起来就是完整因果：第 2 行的块被应用了 → 所以按 `HostName 127.0.0.1` + `Port 59323` 去连 → 端口空着所以 refused。
如果你看到的是 `port 22`，那就说明配置**没**生效（别名写错 / 块没匹配 / 被前面抢了）。

② 假服务端只发一行横幅 ⇒ 版本号被打印、`compat_banner` 匹配不上、连接被关：

```
$ timeout 12 ssh -F /tmp/ssh03-LUWym7/cfg6b -v -o BatchMode=yes -o IdentityAgent=none -o IdentitiesOnly=yes -o IdentityFile=/tmp/ssh03-LUWym7/keys/id_demo -o UserKnownHostsFile=/tmp/ssh03-LUWym7/kh fake-banner true 2>&1 | grep -E 'Applying options|Connecting to|Local version|Remote protocol|compat_banner|KEXINIT sent|Connection closed' | head -7
debug1: /tmp/ssh03-LUWym7/cfg6b line 1: Applying options for fake-banner
debug1: Connecting to 127.0.0.1 [127.0.0.1] port 60565.
debug1: Local version string SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
debug1: Remote protocol version 2.0, remote software version FakeDemoServer
debug1: compat_banner: no match: FakeDemoServer
debug1: SSH2_MSG_KEXINIT sent
```

对面（`nc`）抓到的客户端横幅，证明另一端确实是本机 ssh、端口就是配置里那个：

```
$ sed -n '1p' /tmp/ssh03-LUWym7/nc_fake_capture.txt | tr -d '\r'; printf '整个抓包文件 %s 字节：第一行之后是二进制 KEXINIT 包，不该往终端打\n' "$(wc -c < /tmp/ssh03-LUWym7/nc_fake_capture.txt)"
SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
整个抓包文件 1610 字节：第一行之后是二进制 KEXINIT 包，不该往终端打
```

`compat_banner: no match: FakeDemoServer` 这行很有用：现实中它出现在连老设备/奇异实现时，
后面的 `SSH2_MSG_KEXINIT sent` 说明客户端已经把算法列表发出去了 —— 卡在这之后的失败基本都是算法协商问题。

③ 服务端接了连接但一个字都不发 ⇒ banner exchange 超时，`ConnectTimeout` 管的就是这一段：

```
$ time timeout 12 ssh -F /tmp/ssh03-LUWym7/cfg6c -v -o BatchMode=yes -o IdentityAgent=none -o IdentitiesOnly=yes -o IdentityFile=/tmp/ssh03-LUWym7/keys/id_demo -o UserKnownHostsFile=/tmp/ssh03-LUWym7/kh hang-server true 2>&1 | grep -E 'Connecting to|Local version|timed out' | head -4
debug1: Connecting to 127.0.0.1 [127.0.0.1] port 59679.
debug1: Local version string SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
Connection timed out during banner exchange
Connection to 127.0.0.1 port 59679 timed out

real	0m3.028s
user	0m0.007s
sys	0m0.007s
```

配置里 `ConnectTimeout 3`，实际 3.028 秒退出 —— **`ConnectTimeout` 覆盖 TCP 连接和 banner 交换两段**，
不是只管 `connect()`。这条对排查「端口通但服务卡死」很关键。

④ `ProxyCommand` 的执行证据（故意写一个不存在的程序，日志已经把 `%h %p` 换成真值）：

```
$ timeout 10 ssh -F /tmp/ssh03-LUWym7/cfg6d -v -o BatchMode=yes -o IdentityAgent=none -o IdentitiesOnly=yes -o IdentityFile=/tmp/ssh03-LUWym7/keys/id_demo -o UserKnownHostsFile=/tmp/ssh03-LUWym7/kh bad-proxy true 2>&1 | grep -E 'Executing proxy|No such|Connection|proxy command' | head -4
debug1: Executing proxy command: exec /nonexistent-binary --127.0.0.1 59323
/bin/bash: line 1: /nonexistent-binary: No such file or directory
kex_exchange_identification: Connection closed by remote host
Connection closed by UNKNOWN port 65535
```

`--127.0.0.1 59323` 就是 `%h %p` 展开的结果。第二行是**子进程自己的 stderr** 冒上来的 ——
`ProxyCommand` 写错时报错会长这样，别被 `kex_exchange_identification` 误导成「服务器有问题」。

⑤ `ProxyJump` 的真实展开：先打印它替你造的 `ProxyCommand`，子进程**继承 `-F`**所以沙箱隔离没破：

```
$ timeout 12 ssh -F /tmp/ssh03-LUWym7/config -v -o BatchMode=yes -o IdentityAgent=none -o IdentitiesOnly=yes -o IdentityFile=/tmp/ssh03-LUWym7/keys/id_demo -o UserKnownHostsFile=/tmp/ssh03-LUWym7/kh behind-jump true 2>&1 | grep -E 'Applying options|implicit ProxyCommand|Executing proxy|Connecting to|connect to host' | head -6
debug1: /tmp/ssh03-LUWym7/config line 22: Applying options for behind-jump
debug1: Setting implicit ProxyCommand from ProxyJump: ssh -F /tmp/ssh03-LUWym7/config -v -W '[%h]:%p' jumper
debug1: Executing proxy command: exec ssh -F /tmp/ssh03-LUWym7/config -v -W '[127.0.0.1]:59323' jumper
debug1: /tmp/ssh03-LUWym7/config line 11: Applying options for jumper
debug1: Connecting to 127.0.0.1 [127.0.0.1] port 59323.
ssh: connect to host 127.0.0.1 port 59323: Connection refused
```

看点有三个：`line 22`（目标块）和 `line 11`（跳板块）**都被应用了**；`implicit ProxyCommand` 里自动带上了
`-F` 和 `-v`（这就是能用 `ProxyJump` 就别手写 `ProxyCommand` 的硬理由）；跳板那段也是 127.0.0.1 高端口 ⇒ 预期 refused。
在真服务器上，这里会是两次 `Connection established`（那部分【未验证】，本章红线不制造登录成功）。

### 场景 7 · `Include` 与配置分片

手册原文一次性看全（`Include` 的四条规则，场景 7a 打印）：

```
	       home directories.  Wildcards will be expanded and  processed  in
	       lexical	order.	 Files without absolute paths are assumed to be
	       in ~/.ssh if included in a user configuration file  or  /etc/ssh
	       if  included from the system configuration file.	 Include direc‐
	       tive may appear inside a Match or Host block to	perform	 condi‐
	       tional inclusion.
```

① 结构：入口只留一行 `Include`，分片按**词法顺序**加载，用文件名编号控制先后：

```
$ printf 'Host box-a\n  User shard-10-user\n  HostName 10.0.0.10\n  IdentityFile /tmp/ssh03-LUWym7/keys/id_shard10\n' > /tmp/ssh03-LUWym7/config.d/10-aliases.conf

$ printf 'Host *\n  User shard-90-default\n  IdentityFile /tmp/ssh03-LUWym7/keys/id_default\n' > /tmp/ssh03-LUWym7/config.d/90-catchall.conf

$ printf 'Include %s/config.d/*\n' /tmp/ssh03-LUWym7 > /tmp/ssh03-LUWym7/cfg7; cat -n /tmp/ssh03-LUWym7/cfg7; ls -l /tmp/ssh03-LUWym7/config.d/
     1	Include /tmp/ssh03-LUWym7/config.d/*
total 8
-rw-rw-r-- 1 <本机用户> <本机用户> 102 Oct  5 12:43 10-aliases.conf
-rw-rw-r-- 1 <本机用户> <本机用户>  80 Oct  5 12:43 90-catchall.conf

$ ssh -F /tmp/ssh03-LUWym7/cfg7 -vvv -G box-a 2>&1 | grep 'Reading configuration data'
debug1: Reading configuration data /tmp/ssh03-LUWym7/cfg7
debug1: Reading configuration data /tmp/ssh03-LUWym7/config.d/10-aliases.conf
debug1: Reading configuration data /tmp/ssh03-LUWym7/config.d/90-catchall.conf

$ ssh -G -F /tmp/ssh03-LUWym7/cfg7 box-a 2>/dev/null | grep -E '^(user|hostname|identityfile) '
user shard-10-user
hostname 10.0.0.10
identityfile /tmp/ssh03-LUWym7/keys/id_shard10
identityfile /tmp/ssh03-LUWym7/keys/id_default
```

两个结论：**10 号分片在 90 号前面 ⇒ `user`/`hostname` 由 10 号抢下**（词法顺序就是优先级，
所以 `config.d` 用 `00/10/90` 前缀，和 systemd 一个套路）；**`identityfile` 出现两行，顺序 = 加载顺序**
—— 叠加关键字不受 first-wins 管，这就是「分片里各放各的钥匙」能成立的原因。

② 推荐写法（真实机器上的样子）：

```
     1	Include ~/.ssh/config.d/*
     2	
     3	# 想临时全线关掉连接复用，就在 config.d/99-emergency.conf 里写：
     4	# Host *
     5	#   ControlMaster no
```

入口文件一年不改一次；业务改动全落在分片里，`git diff` 干净、多人不打架、出事就删一个文件。

③ 两个静默坑（都不报错）：通配没匹配到任何文件 / `Include` 写得比目标块晚。

```
$ printf 'Include %s/no-such-dir/*\n' /tmp/ssh03-LUWym7 > /tmp/ssh03-LUWym7/cfg7c2; printf '>>> 目录不存在时：'; ssh -G -F /tmp/ssh03-LUWym7/cfg7c2 box-a 2>/dev/null | grep -m1 '^user '
>>> 目录不存在时：user <本机用户>

$ printf 'Include %s/config.d/*.conf\n' /tmp/ssh03-LUWym7 > /tmp/ssh03-LUWym7/cfg7c3; printf '>>> 用绝对路径 + 通配（本 demo 的正解写法）：'; ssh -G -F /tmp/ssh03-LUWym7/cfg7c3 box-a 2>/dev/null | grep -E '^(user|hostname|identityfile) '
>>> 用绝对路径 + 通配（本 demo 的正解写法）：user shard-10-user
hostname 10.0.0.10
identityfile /tmp/ssh03-LUWym7/keys/id_shard10
identityfile /tmp/ssh03-LUWym7/keys/id_default
```

**这就是「我明明 Include 了怎么没生效」的第一答案**：路径拼错 / 后缀没匹配上，一声不吭。

第三个坑（相对路径按 `~/.ssh` 解析）本章**只抄手册不实测** —— 见上面手册那句
`Files without absolute paths are assumed to be in ~/.ssh`。跑它等于去读你真实的 `~/.ssh`，
本章红线不允许，所以结论标注为**【未验证】**：你在自己机器上验证时，写一个绝对路径的 Include 最保险。

④ `Include` 的位置**就是**优先级：放前面 = 分片先抢，放后面 = 入口块先抢（规则完全不变）。

```
$ printf 'Include %s/config.d/90-catchall.conf\nHost box-a\n  User entry-wins\n' /tmp/ssh03-LUWym7 > /tmp/ssh03-LUWym7/cfg7d; cat -n /tmp/ssh03-LUWym7/cfg7d; printf '>>> Include 行在 Host 块【之前】（分片先被读到）：'; ssh -G -F /tmp/ssh03-LUWym7/cfg7d box-a 2>/dev/null | grep -E '^(user|hostname) '
     1	Include /tmp/ssh03-LUWym7/config.d/90-catchall.conf
     2	Host box-a
     3	  User entry-wins
>>> Include 行在 Host 块【之前】（分片先被读到）：user shard-90-default
hostname box-a

$ printf 'Host box-a\n  User entry-wins\n\nInclude %s/config.d/90-catchall.conf\n' /tmp/ssh03-LUWym7 > /tmp/ssh03-LUWym7/cfg7d2; cat -n /tmp/ssh03-LUWym7/cfg7d2; printf '>>> 把入口 Host 块挪到 Include【之前】：'; ssh -G -F /tmp/ssh03-LUWym7/cfg7d2 box-a 2>/dev/null | grep -E '^(user|hostname) '
     1	Host box-a
     2	  User entry-wins
     3	
     4	Include /tmp/ssh03-LUWym7/config.d/90-catchall.conf
>>> 把入口 Host 块挪到 Include【之前】：user entry-wins
hostname box-a
```

`Include` 不是「引用」，是**就地展开**：展开位置在文件里的位置，就是它在优先级队列里的位置。

### 场景 8 · 配置文件权限：门限实测（沙箱里造，绝不碰真实 `~/.ssh`）

① 被 `Include` 进来的文件「其他人可写」 ⇒ 整条配置罢工：

```
$ printf '>>> 644 正常读：'; ssh -G -F /tmp/ssh03-LUWym7/cfg8 permprobe 2>/dev/null | grep -m1 '^user '
>>> 644 正常读：user perm-user

$ chmod 666 /tmp/ssh03-LUWym7/inc_perm.conf; printf '>>> 666 之后：'; ssh -G -F /tmp/ssh03-LUWym7/cfg8 permprobe 2>&1 | grep -v '^Pseudo' | head -3
>>> 666 之后：Bad owner or permissions on /tmp/ssh03-LUWym7/inc_perm.conf
```

② 门限到底卡在哪：逐档实测（组可写放行，**其他人可写**才拒）：

```
$ for m in 600 640 604 644 664 620 666 646 444; do chmod $m /tmp/ssh03-LUWym7/inc_perm.conf; printf '分片 %-4s => ' "$m"; ssh -G -F /tmp/ssh03-LUWym7/cfg8 permprobe 2>&1 | grep -m1 -E 'perm-user|Bad owner'; done; chmod 644 /tmp/ssh03-LUWym7/inc_perm.conf
分片 600  => user perm-user
分片 640  => user perm-user
分片 604  => user perm-user
分片 644  => user perm-user
分片 664  => user perm-user
分片 620  => user perm-user
分片 666  => Bad owner or permissions on /tmp/ssh03-LUWym7/inc_perm.conf
分片 646  => Bad owner or permissions on /tmp/ssh03-LUWym7/inc_perm.conf
分片 444  => user perm-user
```

规则不是「必须 600」，而是 **「不能被组外的人写」**：`664`、`620` 都能读，`666`、`646` 立刻被拒。
所以「`644` 的配置被拒绝」这种传闻要分清是哪一档：多半是 `666`/`646` 或者属主不对。

③ 反直觉的一条：`-F` 指定的**主文件本身不查权限** —— 别把安全感放在这上面：

```
$ chmod 666 /tmp/ssh03-LUWym7/cfg8; printf '>>> 主文件 666：'; ssh -G -F /tmp/ssh03-LUWym7/cfg8 permprobe 2>&1 | grep -m1 -E 'perm-user|Bad owner'; chmod 600 /tmp/ssh03-LUWym7/cfg8
>>> 主文件 666：user perm-user
```

权限门只装在「被 Include 进来的文件」和默认的 `~/.ssh/config` 路径上。
写自动化脚本时别以为 `chmod 666` 会被拦下来 —— 它照读。

④ 两个更难查的 `-F` 边界：

```
$ mkdir -p /tmp/ssh03-LUWym7/adir; ssh -G -F /tmp/ssh03-LUWym7/adir box-a >/dev/null 2>&1; printf '>>> 退出码=%s（居然成功）\n' "$?"; ssh -G -F /tmp/ssh03-LUWym7/adir box-a 2>&1 | grep -E '^(hostname|user) ' | head -2
>>> 退出码=0（居然成功）
user <本机用户>
hostname box-a

$ ssh -F /tmp/ssh03-LUWym7/nope.cfg -G box-a 2>&1 | grep -v '^Pseudo'
Can't open user config file /tmp/ssh03-LUWym7/nope.cfg: No such file or directory

$ ssh -F /tmp/ssh03-LUWym7/nope.cfg -G box-a >/dev/null 2>&1; echo "真实退出码=$?"
真实退出码=255
```

`-F` 指到**目录**：退出码 0，静默当成「什么都没配」—— 比报错难查十倍。
`-F` 指到**不存在的文件**：明确报错 + 退出码 255。脚本里要判断配置文件路径写得对不对，就用后者。

⑤ 你真实的 `~/.ssh/config` 权限不对时会报 `Bad owner or permissions on /home/<you>/.ssh/config`，
本 demo **【不验证】**（红线：不读写 `~/.ssh`）。顺带说明为什么不能作弊：这些路径来自口令表，不来自 `HOME`：

```
$ HOME=/tmp/ssh03-LUWym7/fakehome ssh -G -F /dev/null demo.example 2>/dev/null | grep -m1 '^userknownhostsfile '
userknownhostsfile <家目录>/.ssh/known_hosts <家目录>/.ssh/known_hosts2
```

换了 `HOME` 打印的还是你家目录（这条只打印路径，不打开文件）——
所以造不出「假 `~/.ssh/config`」来演同一件事。结论照手册：目录 `700`、文件 `600`、其他人绝对不可写。
自查（在你自己机器上跑，别塞进脚本）：`ls -ld ~/.ssh ~/.ssh/config && ssh -G somehost | grep ^hostname`。

---

## 引用约定（这一章的围栏是怎么从 demo 输出搬进文档的）

- **一次真跑**：`bash demo.sh > ~/.cache/handbook-captures/ssh03.txt 2>&1` → 1342 行、退出码 0，
  沙箱 `/tmp/ssh03-LUWym7`，三个沙箱端口 `59323`（空着）/ `60565`（只发一行横幅）/ `59679`（接了不说话）。
  本章每一段围栏都取自这一份捕获，逐行原样；围栏里的 `$ ` 就是当时的命令行。
- **demo 的输出助手**（`demo.sh` 第 131-134 行的 `scene` / `say` / `raw` / `mayfail`）只负责打印：
  `raw "..."` 与 `mayfail "..."` 会先把引号里那串**原样打印成一行 `$ 命令`**，再交给 `bash -c` 执行。
  里面的 `$SB`、`$DEADPORT`、`$CS` 在打印之前就已经被 shell 展开成真值，所以你在本章看到的是
  **展开后的等价命令**（`/tmp/ssh03-LUWym7/cfg2a`，不是 `$SB/cfg2a`）—— 可以直接复制去敲，
  只是路径和端口要换成你自己那一次的。`mayfail` 后面那句 `（退出码 N —— 这条就是要看它失败/报错）`
  是助手加的注解，不是 ssh 打的。
- **脱敏替换（本章唯一对真输出动的两处手）**：
  ① `ssh -G` 从本机口令表展开出来的 `<本机用户>`（没配 `User` 时那行 `user`、`%r` `%l` `%u`、
  `Match localuser` 的命中提示）、`<家目录>`（`/home/<你>/...`、`%d`）、`<主机名>`（`%l` / `%L`），
  以及 `ls -l` 的属主/属组两列，统一写成占位符；这几处**没法用假名跑**
  （OpenSSH 拿 `getpwuid` 比对本机用户，`-l` 只改**远端**用户），所以只能原样重跑、再事后替换。
  ② 配置里当值用的用户名（场景 2h 那个 `Usermame` 拼错示例）换成了中性名 `learner`，重跑取真输出。
  除此之外围栏逐行原样，**派生值（`%C` 的 40 位哈希、`%r@%h:%p` 拼出来的 socket 名）一个都没手改**。
- **这么做的代价**（引文校验器会报的「未匹配」）：
  `python3 ~/.local/bin/verify_loose.py README.md ~/.cache/handbook-captures/ssh03.txt` → `含$行=359 未匹配=2`。
  剩下这 2 行都在场景 5 ⑧ 那个 token 展开围栏里，是同一内容出现两次
  （一次 `debug3: expanded ...`、一次 `userknownhostsfile ...`）：那条长路径把 `<本机用户>` `<主机名>` `<家目录>`
  用**下划线**粘在一起，校验器只折得掉独立成词的名字，折不掉粘连形态，于是把捕获里的真名和本章的占位符
  当成了不同内容 → 各算 1 行未匹配。**属预期内，不是编造**；同一段里那串 `%C` 哈希（`139afd57aa173e28826ec0538a112e9b1762fa24`）
  是本次真跑的原值，按场景 5 ⑧ 给的 `sha1sum` 式子就能复算出来。
- **收尾证据**（demo 在退出前自己打印的，见捕获末尾）：`[收尾 1]` 记下的后台 PID 全部 kill、系统上剩余 `nc` 进程数 0；
  `[收尾 2]` `ss -lnt` 的监听集合与开工前基线 `diff` 无输出 —— 三个沙箱端口一个都没留下，
  `0.0.0.0:22` 与 `[::]:22` 仍是那台真实 sshd 在听（本章只被动观察、没重启没改配置），
  其余高端口都是别的进程原有的；最后 `[沙箱已删除] /tmp/ssh03-LUWym7`，章目录 0 产物。
- **本章标注【未验证】的三条**（都需要一台你能登录成功的服务器，本章红线不制造登录成功）：
  ① 复用真正生效时的 `ControlSocket ... already exists, disabling multiplexing` 日志；
  ② 你真实 `~/.ssh/config` 权限不对时的 `Bad owner or permissions on ~/.ssh/config`；
  ③ 相对路径 `Include` 落到 `~/.ssh/...` 的实际效果（只抄手册，没在你的目录上跑）。
