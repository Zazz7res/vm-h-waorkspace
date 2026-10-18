# 04 ssh-agent 与连接复用：为什么第二次连接不再问口令

> 一句话心智模型：**ssh-agent 是本机一个「只肯签名、不肯交钥匙」的常驻小服务，靠一条 Unix socket 说话， 门牌号写在 `SSH_AUTH_SOCK` 里；连接复用则是把「一条已经认证完的 SSH 连接」挂在一个本地 socket 上， 后来的会话直接借道走 —— 借道的那一次，服务端根本看不到认证过程。**

这一章解决四个具体问题：

1. **不想再输私钥口令**：私钥有口令（第 02 章），每次 `ssh` 都要现解一遍，烦且危险。 `ssh-agent` 把「解开的私钥」放在内存里，之后所有 `ssh` 只跟它要签名。
2. **为什么新开的终端 / `sudo` / 容器里又要口令了**：门牌号（环境变量）没传过去。这是本章一半的篇幅。
3. **为什么第二次连同一台机器快得离谱、也不再问密码**：`ControlMaster`/`ControlPath`/`ControlPersist` 三件套把第一条连接做成了「母连接」，后来的 `ssh` 只是往本机一个 socket 里插一句话。
4. **`ssh -A` 到底把什么东西转 gave 远端**：转的不是私钥，也不是 socket 文件， 而是一条「回程通道」——远端连上本地新建的 socket，消息经加密通道回到你机器上的 agent。

---

## 证据来源（先说清楚这些输出是怎么来的）

本章全部终端输出都是 **`bash demo.sh` 一次真实运行的原文**，逐字摘录；唯一的改动是下面要交代的两类**身份占位符**（文末「引用约定」逐条列了），除此之外一个字没改：

- 命令：`cd ssh-handbook/04-agent-and-multiplexing && bash demo.sh > ~/.cache/handbook-captures/ssh04.txt 2>&1`
- 本次运行：**退出码 0**，capture **433 行**，9 个场景（场景 0 到场景 8）全部跑完， 结尾**四条**收尾自检都打印了（见文末「收尾检查」）。
- 沙箱目录是 `mktemp -d` 生成的随机名，本次是 `/tmp/ssh04-myhsZd`；**你重跑一定会得到另一个名字** （`ssh04-` + 6 位随机字符）。同理会变的东西还有：`ssh-agent` 的 PID、socket 的 inode 号、 文件时间戳、临时生成的 key 指纹、`ss -xl` 当刻在听的那几行。**结构与文字不变，数字与路径会变。**
- **本章的身份脱敏**（全书统一约定，也是这一章唯一对真输出动的两处手）：
  ① demo 要连的**远端用户**是一个中性名 `learner`（本机**没有**这个用户）。它照样每一步都失败在认证阶段，
     结论与用你的真名完全一致 —— 这是真取证不是造假。`%r`、`ControlPath` 展开出的 socket 文件名、`%C` 哈希
     都由这个用户名派生，本章里这些新值**全部来自重跑后的捕获，一个都没手改**。
  ② 另一类身份**只能来自本机**：`ls -l` 的属主/属组两列、`ssh -G` 里 `%u` / `%l` / `%L` / `%d` 展开出来的
     本机登录名、本机名、家目录。这几处没法用假名跑（OpenSSH 拿 `getpwuid` 比对本机用户， `-l` 只改**远端**用户），
     所以本章统一写成 **`<本机用户>` / `<主机名>` / `<家目录>`**。这一列是 ssh 从本机口令表里展开的真实登录名，
     为了不印进书里才换成占位符，你重跑会看到你自己的名字。
- 本机环境（capture 头三行）：

```text
沙箱目录：/tmp/ssh04-myhsZd
OpenSSH 版本：OpenSSH_10.2p1 Ubuntu-2ubuntu3.6, OpenSSL 3.5.5 27 Jan 2026
目标（本机真实 sshd；learner 在本机【不存在】，专门用来撞认证失败）：learner@127.0.0.1
```

跑之前先把红线交代清楚，这也是 `demo.sh` 从头守到尾的边界：

| 边界 | 具体做法 |
| --- | --- |
| 不碰你真实的 `~/.ssh` | 所有 key、config、known_hosts、control socket 都在 `mktemp` 沙箱里；每条 `ssh` 都带 `-F /dev/null`（连 `/etc/ssh/ssh_config` 一起跳过）与显式 `UserKnownHostsFile=<沙箱>/kh` |
| 绝不登录成功 | 每条 `ssh` 都带 `-o BatchMode=yes -o PreferredAuthentications=publickey`，永不提交口令；目标是本机 `learner@127.0.0.1:22`（`learner` 在本机不存在，也没装任何 `authorized_keys`），结局一定是 `Permission denied` |
| 不动系统 sshd | 22 端口只被「被动连接」，没有重启 / 改配置 / 起停 `ssh.socket`；沙箱里也没另起 sshd（本章不需要服务端配置） |
| 不连外部主机 | 全程只有 `127.0.0.1` |
| 不打印私钥 | 只有 `ssh-add -L` 输出的**公钥**，而且被 `cut -c1-60` 截断；capture 里没有任何私钥正文 |
| 不印真实身份 | 远端用户用中性名 `learner`；本机登录名 / 本机名 / 家目录只可能由 shell 自己展开，事后统一写成 `<本机用户>` / `<主机名>` / `<家目录>` |
| 自己起的 agent 自己关掉 | 记下 `MYAGENT_PID/MYAGENT_SOCK/AGENT_LAUNCHED_PID`，`EXIT` trap 里 `ssh-agent -k`，再打四条自检（残留 agent 进程 / 那支 agent 的 PID 已退出 / `ss -lnt` 与开工前基线一致 / `ss -xl` 里没有沙箱 socket）；`--keep` 才会留着 |

还有一处细节值得先讲：脚本里那条 `SSH_BASE` 末尾挂了一个 **根本不存在的 `IdentityFile`** （`keys/_no-default-probe`）。它的作用不是认证，而是**挡箭牌**——只要显式给了 `IdentityFile`， `ssh` 就不会再把 `~/.ssh/id_rsa`、`~/.ssh/id_ed25519` 这些默认文件名挨个探测一遍。 手册绝不需要、也绝不伸手去碰你的家目录。

---

## 你会遇到的场景（先对号入座）
| 症状 / 需求 | 本章给你的答案 | 在第几节 |
| --- | --- | --- |
| `ssh-add -l` 一句 `Could not open a connection to your authentication agent.` | 你压根没有 agent，或门牌号丢了 | 场景 0 / 场景 5 |
| `Error connecting to agent: No such file or directory` | 门牌号还在，但 socket 文件已经没了（agent 被杀 / 重启机器） | 场景 0 |
| `Error connecting to agent: Connection refused` | 那个路径上有东西，但它不是一个在听的 agent（抄了别人的路径、或残留文件） | 场景 0 |
| 加了 key 却没生效，`ssh` 还是要口令 | 十有八九是没 `eval "$(ssh-agent -s)"`，或换了进程树 | 场景 1 / 场景 5 |
| 临时给别人用一次，不想留钥匙在内存里 | `ssh-add -t 3600`（过期是真的会发生） | 场景 4 |
| `Too many authentication failures` | agent 里钥匙太多，服务端数到第 6 次翻脸 | 场景 6 |
| `ssh 主机A` 很快，`scp 主机B` 很慢，明明同一台 | 默认 `controlmaster false`，复用是关的 | 场景 7 |
| 配了 `ControlPath` 却毫无效果，也不报错 | 目录不存在 → OpenSSH **静默不复用**；`-vv` 才看得见 | 场景 7 |
| `Control socket connect(...): Connection refused` | 残留 socket / 那个位置上放了普通文件 | 场景 7 |
| `%h %p %l %r %C` 到底展开成什么 | 三条 `-O check` 的报错把它们原样打出来了 | 场景 7 |
| 走跳板机时每跳一次都要口令一次 | 给**跳板机的 Host 段**也配三件套（命令行 `-o` 到不了跳板机） | 场景 7 后半 |
| `ssh -A` 之后在远端 `ssh 别的机器` 不用密码，安全吗 | 转的是「使用权」不是「私钥」，风险仍然真实存在 | AgentForwarding 专章 |

---

## 心智模型：先把三张图刻进脑子

### 图 1：agent = 一个只监听本机 socket 的签名服务
```
   你的终端（shell 进程，环境里有一个 SSH_AUTH_SOCK=/…/agent.sock）
        │  fork/exec
        ▼
   ┌──────────┐   ①「你手里有哪些钥匙？」②「请用第 3 把签个名」   ┌────────────┐
   │   ssh    │ ────────────────────────────────────────────►  │  ssh-agent │ ← 常驻进程，内存里是【已解密】的私钥
   │  客户端  │ ◄────────────────────────────────────────────  │            │
   └──────────┘        公钥列表 / 签名结果                      └────────────┘
        │ TCP 22 → sshd                            ▲ 只有这一端能连
                                                   │ srw------- /…/agent.sock（0600，owner 是你）
   服务端永远只看到【公钥】和【签名】，私钥字节从不出本机（除非你 -A）。
```

你在 C 里写过的 socket 服务端就是这个形状：**一个 `AF_UNIX` 的 listening socket + 一个消息循环**。 `ssh-agent` 没有别的魔法：它 bind 一个路径、`listen`、然后 `accept` 若干连接， 每条连接上跑一个**长度前缀的二进制协议**（4 字节大端长度 + 1 字节消息类型）， 请求类型两种最常用：`SSH2_AGENTC_REQUEST_IDENTITIES`（要列表）和 `SSH2_AGENTC_SIGN_REQUEST`（要签名）。跟你在数据结构课里写的「客户端-服务端消息帧」是一回事。
### 图 2：连接复用 = 一条真连接（master）+ 若干个本地借道者（slave）
```
      第一次 ssh box                 第二次 ssh box            第三次 scp …
          │                             │                          │
          │ 没有 master → 自己当 master  │ ControlPath 有人在听！    │  同左
          ▼                             ▼                          ▼
   ┌────────────┐  TCP+KEX+认证  ┌──────────────────────────────────────┐
   │   sshd     │ ◄════════════ │ master（后台常驻的 ssh 进程）           │
   └────────────┘   一条加密连接 └──────────────────────────────────────┘
                                        ▲ 本机 Unix socket：srw------- cm-learner@…:22
                                          （= ControlPath 展开出来的那个文件）
                                   slave #1 / slave #2：连上只说「给我开一条 channel」，不过认证
```

**「第二次不再要密码」的真正原因**：slave 端**根本没有发生认证**。认证只在 master 建立那条 加密连接时做过一次；slave 只是在本机上向那个 socket 发了一个「开新会话」的请求， master 在它已有的加密连接上开一条新 channel 给服务端 —— 服务端在一条**已认证的**连接上 收到 `channel open`，它没有任何理由再问你是谁。
### 图 3：agent 转发转的是「回程通道」，不是钥匙
```
   你的笔记本                                远端服务器
   ┌──────────┐  ①channel request          ┌──────────────────────────┐
   │   ssh    │   agent-Forward@openssh.com │ sshd                     │
   │  客户端  │ ═══════════════════════════► │  ├─ 新建 /…/agent.<pid>  │
   └──────────┘                             │  └─ 给 shell 设           │
        ▲                                   │     SSH_AUTH_SOCK=那个路径 │
        │  ③转给本地 agent 签名              └──────────────────────────┘
   ┌──────────┐  ②远端某进程连上那个 socket，发一条「请签名」消息    ▲
   │ ssh-agent│ ◄═══ 消息经已认证的加密 channel 回来（签名结果原路返回）
   └──────────┘
   远端拿到的是「能让你的 agent 干活的能力」，不是私钥本身；
   但「能让它干活」已经足够以你的身份去认证任何一台你 agent 里有钥匙的机器。
```

---

## 场景 0：先把「agent 不在」演一遍 —— 三种长相，三种病因

这一节是所有诡异问题的起点。三条命令，三种报错，病因完全不同。
### 长相 1：环境变量压根没设
```text
$ env -u SSH_AUTH_SOCK ssh-add -l
  （这条按设计会失败，就为了看真实报错原文）
Could not open a connection to your authentication agent.
  [退出码 2 —— 非 0 是预期的]
```

`env -u SSH_AUTH_SOCK` 的意思是「把 `SSH_AUTH_SOCK` 从环境里抠掉再执行」。 `ssh-add` 找不到门牌号，于是打出这句老报错 —— 注意它的措辞是 **Could not open a connection to your authentication agent**， 它说的是「**连不上**」，而不是「agent 里没钥匙」。这句是新手最容易误读的报错。
### 长相 2：门牌号有，但那个文件不存在
```text
$ env SSH_AUTH_SOCK=/tmp/ssh04-myhsZd/这个文件不存在.sock ssh-add -l
  （这条按设计会失败，就为了看真实报错原文）
Error connecting to agent: No such file or directory
  [退出码 2 —— 非 0 是预期的]
```

这就是「**重启过 / 进过容器 / 换过终端**」的标准长相： 你的 shell 环境里还留着上次记下的路径，可那个 socket 文件早跟着 agent 进程一起没了。 报错来自 `connect()` 系统调用，`ENOENT` → 打印成人类语言 `No such file or directory`。
### 长相 3：那个位置有东西，但不是 agent
```text
$ env SSH_AUTH_SOCK=/tmp/ssh04-myhsZd/notasocket ssh-add -l
  （这条按设计会失败，就为了看真实报错原文）
Error connecting to agent: Connection refused
  [退出码 2 —— 非 0 是预期的]
```

脚本在这里先 `printf 'hello' > <沙箱>/notasocket` 造了一个**普通文件**，然后把门牌号指过去。 `connect()` 打到一个不是 socket 的路径上，Linux 返回的是 `ECONNREFUSED`， 所以你看到的是 **Connection refused**。这个报错还有另一个更常见的成因： socket 文件在、但**没有进程在监听**（agent 死了、文件没来得及 unlink）—— 也就是「残留 socket」， 和场景 7 里 `ControlPath` 的残留 socket 完全同一种症状。
### 三种报错的鉴别表
| 你看到的 | 内核返回 | 病因 | 一眼确认的办法 |
| --- | --- | --- | --- |
| `Could not open a connection to your authentication agent.` | 应用层自己打的 | `SSH_AUTH_SOCK` 未设（新终端、`sudo`、容器、cron） | `echo "$SSH_AUTH_SOCK"` 为空 |
| `Error connecting to agent: No such file or directory` | `ENOENT` | 变量指向的路径不存在：agent 已死 / 机器重启 / 路径抄错 | `ls -l "$SSH_AUTH_SOCK"` 也说不存在 |
| `Error connecting to agent: Connection refused` | `ECONNREFUSED` | 路径上有东西但不是活着的 socket：残留 socket、普通文件、别人机器的路径硬抄 | `ss -xl \| grep <那个路径>` 查不到 LISTEN |
| `The agent has no identities.`（退出码 1） | —— | 门牌号对、agent 活着，只是里面**是空的** | `ssh-add <key>` 装一把再看 |

> 最后一条严格说不是「连不上」，但新手同样会把它当成「agent 坏了」。 它的退出码是 **1**，前三种是 **2**，这也是一个可用的判据。

**常见翻车**：拿 `ssh-add -l` 的退出码写脚本时，把 `1`（空的）和 `2`（连不上）混为一谈， 于是「agent 空了」被当成「agent 挂了」去重启，反而把好好的 agent 换掉了。

---

## 场景 1：起一个真 agent，看清它到底是个什么东西

### 1.1 `ssh-agent -s` 只干两件事：起进程 + 打印「该怎么设变量」
```text
$ ssh-agent -s -a /tmp/ssh04-myhsZd/tmp-print.sock
SSH_AUTH_SOCK=/tmp/ssh04-myhsZd/tmp-print.sock; export SSH_AUTH_SOCK;
SSH_AGENT_PID=169701; export SSH_AGENT_PID;
echo Agent pid 169701;

$ env SSH_AUTH_SOCK=/tmp/ssh04-myhsZd/tmp-print.sock SSH_AGENT_PID=169701 ssh-agent -k
unset SSH_AUTH_SOCK;
unset SSH_AGENT_PID;
echo Agent pid 169701 killed;
  ↑ 看：ssh-agent -s 干的两件事 = 在后台起一个进程 + 打印几行『该怎么设环境变量』的文本。
    所以必须 eval 它，否则进程起来了而你的 shell 不知道门牌号 —— 这就是『加了 key 却没生效』的第一种形态。
```

逐字拆开这三行输出（这是 Bourne shell 语法，`-c` 会给 csh 版）：

| 行 | 内容 | 含义 |
| --- | --- | --- |
| 1 | `SSH_AUTH_SOCK=<路径>; export SSH_AUTH_SOCK;` | 门牌号：agent 监听的 Unix socket 路径 |
| 2 | `SSH_AGENT_PID=<数字>; export SSH_AGENT_PID;` | agent 的进程号：`ssh-agent -k` 靠它杀进程 |
| 3 | `echo Agent pid <数字>;` | 给人看的一句确认，纯字符串 |

**关键点：这三行是「打印出来」的，不是「生效」的。** 父进程没法直接改子进程的环境，所以 `ssh-agent` 只能把「你该怎么设」写成 shell 代码吐给你， 由 `eval` 在**当前 shell**里执行。于是就有了本章第一号翻车姿势：

| 写法 | 会发生什么 |
| --- | --- |
| `ssh-agent -s`（不 eval） | 进程起来了，但你的 shell 里 `SSH_AUTH_SOCK` 还是老样子（或压根没有） |
| `eval "$(ssh-agent -s)"` | **正确**：把打印出来的三行当成命令在当前 shell 里执行 |
| `ssh-agent -s \| cat > /dev/null` | 进程起来了、输出被扔掉 ⇒ 门牌号永久丢失，只能回头 `pgrep` 找 |

capture 里紧接着用 `env SSH_AUTH_SOCK=… SSH_AGENT_PID=169701 ssh-agent -k`（第 37 行） 把这个临时 agent 关掉，顺便演示 `-k` 的输出形状：它也是**打印三行 unset 代码**， 所以「关掉 agent」也要 `eval "$(ssh-agent -k)"` 才会把你的环境变量清干净。
### 1.2 正式起一个，并看清门牌号
```text
$ eval "$(ssh-agent -s -a /tmp/ssh04-myhsZd/agent.sock)"
Agent pid 169710
```

`-a <路径>` 显式指定 socket 绑在哪里 —— 脚本必须用它，否则默认路径在你的家目录里（见 1.5）。 `Agent pid 169710` 是 `eval` 执行到第三行 `echo` 的产物。

```text
$ echo "SSH_AUTH_SOCK=$SSH_AUTH_SOCK"
SSH_AUTH_SOCK=/tmp/ssh04-myhsZd/agent.sock

$ echo "SSH_AGENT_PID=$SSH_AGENT_PID"
SSH_AGENT_PID=169710
```

**一句话记住分工**：`SSH_AUTH_SOCK` 是「电话号」，`SSH_AGENT_PID` 只是「这台机器的主人是谁」的备忘； **所有实际通信只走 socket 路径**，PID 在认证路径上一个字节都不参与。
### 1.3 它是进程、是监听、也是一个文件：三种证据
```text
$ ps -o pid,ppid,cmd -p 169710
    PID    PPID CMD
 169710    6522 ssh-agent -s -a /tmp/ssh04-myhsZd/agent.sock
```

`ps` 那一行证明它就是个普通用户态进程：PPID 是 6522（脚本当时的父进程）， 命令行是 `ssh-agent -s -a <沙箱>/agent.sock`。`ssh-agent` 启动时 fork 到自己父进程之下， 所以**你关掉终端时它会跟着没**（父进程死了 → 某些环境下被 SIGHUP 带走），这正是场景 5 的一个成因。

```text
$ ss -xl | grep 'agent.sock' || echo '  (没命中)'
u_str LISTEN 0      4096          /run/user/1000/snapd-session-agent.socket 47185             * 0
u_str LISTEN 0      128                        /tmp/ssh04-myhsZd/agent.sock 393783            * 0
```

`ss -xl` 列出全机所有 **A**BSD 系 Unix socket 的监听项。四列关键信息： `u_str`（SOCK_STREAM 类型的 Unix socket）、`LISTEN`、`Recv-Q 0`、`Send-Q 128`（backlog 队列长度）， 然后是**路径**和一个数字（这次是 `393783`，即 socket inode；每次跑都不同）。

顺带看到一个不属于本脚本的：`/run/user/1000/snapd-session-agent.socket`（`47185` 那行）， snapd 的会话 socket，与 ssh 无关 —— 它只是因为名字里带 `agent` 才被 `grep` 捞进来， 顺带提醒一句：`grep agent` 不等于 `grep 你那支 agent`。

至于**「一台机器上可以同时有好几支 agent」**，本次运行里由文末「收尾自检 1」直接证了： `pgrep -a ssh-agent` 打出的三行全都是**不属于本脚本**的 —— 桌面环境那支 `gcr-ssh-agent`、 它启动的 `ssh-agent -D -a /run/user/1000/gcr/.ssh`，还有一支会话里残留的 `ssh-agent -D`。 于是本章后半部分反复出现的坑就在眼前： 你的 `SSH_AUTH_SOCK` 指的是哪一个？`ssh-add` 装进了哪一个？（场景 5、坑清单 #4）

```text
$ ls -l /tmp/ssh04-myhsZd/agent.sock
srw------- 1 <本机用户> <本机用户> 0 Oct  5 12:50 /tmp/ssh04-myhsZd/agent.sock
```

这行 `ls -l` 是本节最重要的一张证据：

```
srw------- 1 <本机用户> <本机用户> 0 Oct  5 12:50 <沙箱>/agent.sock
└┬┘└──┬──┘ └─┬─┘ └───┬───┘ └┬┘ └────┬────┘ └───────┬───────┘
│    │     │       │      │        │              │
类型   权限    链接数 属主     属组   大小    路径
=socket  0600          <本机用户> <本机用户>  0（没内容！）
```

三个要点：

1. **第一位是 `s`**：它是 socket，不是普通文件。`srw` 里那个 `w` 不是「可写内容」， 而是「可以 `connect()` 进去」——Unix socket 没有 read/write 语义，只有 `connect()`。
2. **`0600` = 只有属主能连**。内核在 `connect()` 时按**文件权限**检查（Unix socket 的访问控制就是文件系统权限）。 别的用户即使知道路径、即使 `/tmp` 是世界可读的，也连不上。
3. **大小 0**：socket 文件里没有一字节私钥。它只是命名空间里的一个「门牌」， 真正的数据在 agent 进程的**内存**里流动。
### 1.4 「socket 文件到底是谁的」——三层答案
| 层 | 谁创建 | owner / mode | 谁能连 | 结论 |
| --- | --- | --- | --- | --- |
| 本地 agent socket | `ssh-agent` 进程 `bind()` | 启动 agent 的那个 uid，默认 0600（`umask` 决定，OpenSSH 会主动收紧） | 同一用户 | 「谁的 agent」= 谁启动了这个进程；不是你 `ssh` 时随手敲的那个用户 |
| `ControlPath` 复用 socket | **master** `ssh` 客户端进程（认证成功之后才 bind） | 启动 master 的用户，同样靠目录+文件权限把关 | 同一用户（同用户即等于「拿到你的全部已认证会话」） | 所以手册强制要求：`ControlPath` 必须在**别的用户不可写**的目录里（`~/.ssh` 是 0700，正好） |
| 远端被转发的 agent socket | **远端 `sshd`**（收到 `agent-Forward@openssh.com` 之后创建） | 你在远端登录的那个用户 | 远端上同 uid 的任意进程（含别人替你起的进程）；有 root 的机器上是任何人 | 「你把钥匙借给了整台机器」——这就是 `-A` 的全部风险来源 |

一个必须记住的推论：**同机同用户 = 无隔离**。 如果你在一台多人共享的机器上用同一个 uid（或者你的 agent socket 目录被别人可写）， 别人就能把你的 agent 当签名机用。想在同机内做隔离，唯一办法是**一人一个 agent**， 即一人一个 `SSH_AUTH_SOCK`（场景 5 的「修法」那节给了具体做法）。
### 1.5 OpenSSH 10.2 的默认路径（本机未验证）

`man 1 ssh-agent` 本机原文（两条相关）：`-a bind_address` = “Bind the agent to the Unix-domain socket bind_address. The default is to create a socket at a random path matching $HOME/.ssh/agent/s.*.”； `-U` = “Instructs ssh-agent not to clean up stale agent sockets under $HOME/.ssh/agent/.”

也就是说：**你什么都不写时，新 OpenSSH 把 agent socket 放在 `~/.ssh/agent/s.<随机>`**， 而且启动时还会顺手清理该目录下的残留 socket。本章脚本用 `-a` 把路径全部钉在沙箱里， 所以这条默认行为**未在本机复现**（要验证它就得真的在家目录里操作，本手册不允许）。

**常见翻车**：`SSH_AUTH_SOCK` 被写死在 dotfile 里（`export SSH_AUTH_SOCK=/tmp/xxx`）， 换了机器 / 换了版本，路径规则不同，就成了场景 0 的长相 2、3。

---

## 场景 2：往里装钥匙

### 2.1 先造三把不同算法的钥匙（脚本注释里就写清了是谁的）
```text
$ ssh-keygen -q -t ed25519 -N '' -C key-alpha@sandbox -f /tmp/ssh04-myhsZd/keys/id_alpha

$ ssh-keygen -q -t rsa -b 2048 -N '' -C key-beta@sandbox -f /tmp/ssh04-myhsZd/keys/id_beta

$ ssh-keygen -q -t ecdsa -N '' -C key-gamma@sandbox -f /tmp/ssh04-myhsZd/keys/id_gamma

$ ls -l /tmp/ssh04-myhsZd/keys
total 24
-rw------- 1 <本机用户> <本机用户>  411 Oct  5 12:50 id_alpha
-rw-r--r-- 1 <本机用户> <本机用户>   99 Oct  5 12:50 id_alpha.pub
-rw------- 1 <本机用户> <本机用户> 1823 Oct  5 12:50 id_beta
-rw-r--r-- 1 <本机用户> <本机用户>  398 Oct  5 12:50 id_beta.pub
-rw------- 1 <本机用户> <本机用户>  513 Oct  5 12:50 id_gamma
-rw-r--r-- 1 <本机用户> <本机用户>  179 Oct  5 12:50 id_gamma.pub
```

`-N ''` = 私钥**不设口令**，这是脚本能自动化跑完的原因；你自己的钥匙当然应该设。 `-C` 只写进 `.pub` 文件末尾的注释字段。看第 113 行那三条 `ssh-add -l` 输出里的 `key-alpha@sandbox` —— 那个注释就是从 `.pub` 带来的，**不是** agent 自己编的。

`ls -l` 里两个权限值得停一下：私钥 `0600`、公钥 `0644`。这是标准答案： 公钥本来就是可以随便贴的东西（`ssh-add -L` 打印的就是它，直接进 `authorized_keys`）。
### 2.2 空的 agent 先给你一次「没钥匙」的错
```text
$ ssh-add -l
  （这条按设计会失败，就为了看真实报错原文）
The agent has no identities.
  [退出码 1 —— 非 0 是预期的]
```

`The agent has no identities.` + 退出码 1。连上了，但里面是空的。
### 2.3 `ssh-add` 干的事：把私钥交给那个进程保管
```text
$ ssh-add /tmp/ssh04-myhsZd/keys/id_alpha
Identity added: /tmp/ssh04-myhsZd/keys/id_alpha (key-alpha@sandbox)

$ ssh-add /tmp/ssh04-myhsZd/keys/id_beta
Identity added: /tmp/ssh04-myhsZd/keys/id_beta (key-beta@sandbox)

$ ssh-add /tmp/ssh04-myhsZd/keys/id_gamma
Identity added: /tmp/ssh04-myhsZd/keys/id_gamma (key-gamma@sandbox)
```

`Identity added: <路径> (<注释>)` 这一行里的路径是**你刚才给的源文件路径**， agent 只是记住了「这把身份来自这里」。**私钥内容被写进了 agent 进程的内存**， 不是复制成某个「agent 数据库文件」。你可以在 `ssh-add` 之后把私钥文件删了（别真这么干）， 只要 agent 还活着，认证照样能用 —— 这是「agent 里存的是明文私钥」最直接的证明。

有口令的私钥会在这里**唯一一次**问你要口令（或者从 `SSH_ASKPASS` 拿）。 之后每一次 `ssh` 都不再要，因为解密结果一直在 agent 内存里。
### 2.4 `-l`（指纹）与 `-L`（公钥全文）
```text
$ ssh-add -l
256 SHA256:9dUG+bPFcC13n2dibt38fCxCZACQW3kGXloDZfKWga0 key-alpha@sandbox (ED25519)
2048 SHA256:+DKN0YG/RhyQhXgJlmqErP6XNKvmxpWkGAOtmc1sK5o key-beta@sandbox (RSA)
256 SHA256:zZIU3KsJZQGvvglheenUAkMBGpwHs0wj0AG03iOhSN8 key-gamma@sandbox (ECDSA)
```

| 字段 | 例 | 怎么读 |
| --- | --- | --- |
| 位数 | `256` / `2048` / `256` | 密钥「强度」的粗略标示：ed25519 报 256、RSA 报 2048、ECDSA P-256 报 256 |
| 指纹 | `SHA256:9dUG+bPFcC13…` | 公钥的 SHA-256 摘要（base64、去掉 `=`）。认钥匙用的短 ID，不是秘密 |
| 注释 | `key-alpha@sandbox` | `.pub` 里的 comment，随你写 |
| 类型 | `(ED25519)` | 算法 |

**指纹不是加密「钥匙 ID」**：`ssh-add -d <文件>` 靠的是**文件路径**去匹配， `ssh-add -D` 是全清；想按指纹删可以用 `ssh-add -d` 配公钥文件。

```text
$ ssh-add -L | cut -c1-60
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIIdMfPwYWgN/oKn8SHq1eAq2
ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCHPNjZLBORfNtfva5KJw/f
ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlz
```

这三行是 `ssh-add -L | cut -c1-60` —— **公钥**（可以随便给人）， 而且脚本还截断了 60 字符。**本章 capture 与本文档里没有任何私钥正文。** `ssh-add -L` 的输出可以直接重定向成 `authorized_keys`，这是 agent 的一个正经用途。
### 2.5 删一把、全删、再全装回去
```text
$ ssh-add -d /tmp/ssh04-myhsZd/keys/id_beta
Identity removed: /tmp/ssh04-myhsZd/keys/id_beta RSA (key-beta@sandbox)

$ ssh-add -l
256 SHA256:9dUG+bPFcC13n2dibt38fCxCZACQW3kGXloDZfKWga0 key-alpha@sandbox (ED25519)
256 SHA256:zZIU3KsJZQGvvglheenUAkMBGpwHs0wj0AG03iOhSN8 key-gamma@sandbox (ECDSA)

$ ssh-add -D
All identities removed.

$ ssh-add -l
  （这条按设计会失败，就为了看真实报错原文）
The agent has no identities.
  [退出码 1 —— 非 0 是预期的]
```

注意 `-d` 的回报里带了算法：`Identity removed: <路径> RSA (key-beta@sandbox)`； `-D` 只回一句 `All identities removed.`（**不会**打印清单）。 最后 `ssh-add -l` 的退出码又回到 1。

脚本里还有一条「一条 `run` 里塞三句 `ssh-add` + 最后一句 `ssh-add -l`」的长命令（capture 第 139 行）： 输出就是三条 `Identity added` 后面紧跟三行列表 —— **顺序与装进去的顺序完全一致**（这条在后面场景 6 里很关键， 因为 `Offering` 的先后就是 agent 列表的先后）。想自己复现：`ssh-add <alpha> <beta> <gamma>` 三条连打，再接一条 `ssh-add -l`。

**常见翻车**

- 在 A agent 里 `ssh-add`，`ssh` 却连不上：因为当前 shell 的 `SSH_AUTH_SOCK` 指向 B agent。 `ssh-add -l` 看到什么，`ssh` 就用什么 —— 它们读同一个变量，所以这条自查永远有效。
- `ssh-add` 之后 `ssh` 还要口令：几乎一定是场景 5（变量没传过去）或场景 6（亮错钥匙）。
- 以为 agent 有「持久化」：OpenSSH 的 agent **不存盘**，重启就空。 想持久化只有两条路：桌面环境的 keyring（`gcr-ssh-agent`/`kwallet`）或 macOS 钥匙串，见场景 4。

---

## 场景 3：证明 ssh 用的是 agent 里的钥匙，而不是磁盘上的文件

这是全章最重要的一节：**把「黑箱」变成「可验证」**。手法只有一句话—— **故意不给任何 `-i`，也不设 `IdentitiesOnly`**，于是 ssh 唯一能拿到钥匙的地方就是 agent。
### 3.1 真实 `-vv` 输出
```text
$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/tmp/ssh04-myhsZd/kh -o IdentityFile=/tmp/ssh04-myhsZd/keys/_no-default-probe -o ConnectTimeout=8 -vv learner@127.0.0.1 true 2>&1 | tee /tmp/ssh04-myhsZd/vv_agent.txt | grep -E 'Permanently added|get_agent_identities|Will attempt key|Offering public key|Permission denied'
  （这条按设计会失败，就为了看真实报错原文）
Warning: Permanently added '127.0.0.1' (ED25519) to the list of known hosts.
debug1: get_agent_identities: bound agent to hostkey
debug1: get_agent_identities: agent returned 3 keys
debug1: Will attempt key: key-alpha@sandbox ED25519 SHA256:9dUG+bPFcC13n2dibt38fCxCZACQW3kGXloDZfKWga0 agent
debug1: Will attempt key: key-beta@sandbox RSA SHA256:+DKN0YG/RhyQhXgJlmqErP6XNKvmxpWkGAOtmc1sK5o agent
debug1: Will attempt key: key-gamma@sandbox ECDSA SHA256:zZIU3KsJZQGvvglheenUAkMBGpwHs0wj0AG03iOhSN8 agent
debug1: Will attempt key: /tmp/ssh04-myhsZd/keys/_no-default-probe  explicit
debug1: Offering public key: key-alpha@sandbox ED25519 SHA256:9dUG+bPFcC13n2dibt38fCxCZACQW3kGXloDZfKWga0 agent
debug1: Offering public key: key-beta@sandbox RSA SHA256:+DKN0YG/RhyQhXgJlmqErP6XNKvmxpWkGAOtmc1sK5o agent
debug1: Offering public key: key-gamma@sandbox ECDSA SHA256:zZIU3KsJZQGvvglheenUAkMBGpwHs0wj0AG03iOhSN8 agent
learner@127.0.0.1: Permission denied (publickey,password).
```

### 3.2 逐行精读

- `Warning: Permanently added '127.0.0.1' (ED25519) to the list of known hosts.` —— 沙箱 `kh` 是空的，`StrictHostKeyChecking=accept-new` 自动收下指纹并写进**沙箱文件**。 第一次连任何主机都会看到这句；它和第 04 章主题无关，但值得你确认自己的 known_hosts 路径对不对。
- `debug1: get_agent_identities: bound agent to hostkey` —— 这一行是**新 OpenSSH 才有的**。它对应 agent 协议里的 *host-bound agent* 扩展： 客户端在问 agent 之前，先把「我要连的是这台服务器」这件事绑进上下文（服务端主机密钥）。 意义：即使有个恶意服务器把你的 agent 转发链拉起来，也不能拿你的 agent 去认证**别的**主机。 你不需要理解它的密码学，只要知道：**这行出现 = agent 问答是按目标主机做上下文的**。
- `debug1: get_agent_identities: agent returned 3 keys` —— 一次 `SSH2_AGENTC_REQUEST_IDENTITIES` 往返，拿到 3 个「公钥+注释」。 注意：**这一步只拿到公钥**，私钥还在 agent 里没动过。
- `debug1: Will attempt key: <注释> <算法> <指纹> agent`（三行） —— 行尾那个 `agent` 是**出处标签**，本章反复用到。三种取值：

| 行尾标签 | 含义 |
| --- | --- |
| `agent` | 这把身份来自 agent |
| `explicit` | 来自命令行/config 显式给出的 `IdentityFile`（`-i` 或配置里的关键字） |
| （空白，什么都没有） | 来自默认文件名（`~/.ssh/id_*`）的自动探测 |

第 160 行 `Will attempt key: <沙箱>/keys/_no-default-probe  explicit` 行尾**只有空白没有标签类型**： 它就是我们那个「挡箭牌」，一个不存在的文件，所以 ssh 只是记了一笔「我要试它」，后面自然什么也没发生。
- `debug1: Offering public key: … agent`（三行） —— 提交公钥试探（`SSH2_MSG_USERAUTH_REQUEST`，method=`publickey`，`signature flag = FALSE`）。 **这一步只发公钥，不发签名。**服务端在 `authorized_keys` 里找这串公钥。
- `learner@127.0.0.1: Permission denied (publickey,password).` —— 三把公钥都不在（本机没装 authorized_keys），认证阶段结束。这是**设计好的结局**。
### 3.3 「成功那一行」本机拿不到（明确标注：未验证）
```text
$ wc -l /tmp/ssh04-myhsZd/vv_agent.txt; grep -c 'Server accepts key' /tmp/ssh04-myhsZd/vv_agent.txt || echo '  （0 次命中 = 本机拿不到，符合预期）'
110 /tmp/ssh04-myhsZd/vv_agent.txt
0
  （0 次命中 = 本机拿不到，符合预期）
```

`-vv` 全文 110 行，`Server accepts key` 命中 **0** 次。这条必须诚实标注为 **「本机未能复现」**。完整流程长这样（`<…>` 是未验证部分）：

```
Offering public key: ... agent
<Server accepts key: <注释> <算法> <指纹> agent>   ← 服务端说「这把我要」
<get_public_key / ssh_agent_get_identies: signing>  ← 客户端这时才发 SSH2_AGENTC_SIGN_REQUEST
<Authenticating with public key "…">                ← 签名结果提交，method=publickey, signature=TRUE
<Authenticated to …>
```

**「什么时候才真的用到私钥」**：只有在 `Server accepts key` 之后。 所以 agent 的签名请求是**一次一签**：私钥不出机器，出机器的是一个针对特定 「会话哈希（exchange hash）」的签名，换一次会话就得重签一次，签名不可复用。

给你自己的服务器补验证的完整命令（本章红线是**不往真实用户目录装 authorized_keys**，所以这几条请你在**你自己的测试机**上跑）：

```text
ssh-copy-id -i ~/.ssh/id_ed25519.pub 你的账号@你的测试机   # 或者手工把 ssh-add -L 的那行贴进 authorized_keys
ssh -vv 你的账号@你的测试机 true 2>&1 | grep -E 'Server accepts key|Authenticating with public key|Authenticated'
```

看到 `Server accepts key` 出现、而且它的行尾标签是 `agent`，就补上了本章缺的那一环。
### 3.4 对照实验 A：`IdentityAgent=none` 彻底绕开 agent
```text
$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/tmp/ssh04-myhsZd/kh -o IdentityFile=/tmp/ssh04-myhsZd/keys/_no-default-probe -o ConnectTimeout=8 -o IdentityAgent=none -v learner@127.0.0.1 true 2>&1 | grep -E 'get_agent_identities|Offering public key|No more auth|Permission denied'
  （这条按设计会失败，就为了看真实报错原文）
debug1: No more authentication methods to try.
learner@127.0.0.1: Permission denied (publickey,password).
```

一个 `get_agent_identities` 都没有、一条 `Offering` 都没有，直接 `No more authentication methods to try.` —— 因为客户端手里**一把公钥都没有**： 既不能问 agent（`IdentityAgent=none`），又不能读默认文件（那个挡箭牌 `IdentityFile` 挡住了自动探测）， 而 `IdentitiesOnly` 没开。这个实验干净地证明了「钥匙全部来自 agent」。

`IdentityAgent` 的三种写法（`man 5 ssh_config` 原文摘录）：

```text
       IdentityAgent
	       Specifies  the  Unix-domain  socket used to communicate with the
	       authentication agent.
	       This option overrides the SSH_AUTH_SOCK environment variable and
	       can be used to select a specific agent.  Setting the socket name
	       to none disables the use of an  authentication  agent. …
	（后面两句：填字符串 "SSH_AUTH_SOCK" 就回到读环境变量；值以 $ 开头则把该变量当路径。 摘录有删节）
```

三个实用姿势：

| 写法 | 用途 |
| --- | --- |
| `IdentityAgent none` | 调试 / 脚本里绝对不要误用你的 agent（比 `-a`（关转发）更彻底，`-a` 只管转发不管本地） |
| `IdentityAgent /path/to/socket` | **一台机器多个 agent** 的正解：给不同项目/不同身份各一个 socket |
| `IdentityAgent $MY_AGENT_SOCK` | 用环境变量选，方便在 config 里做「按目录切身份」 |
### 3.5 对照实验 B：`IdentitiesOnly=yes` + 只给一把 `-i`
```text
$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/tmp/ssh04-myhsZd/kh -o IdentityFile=/tmp/ssh04-myhsZd/keys/_no-default-probe -o ConnectTimeout=8 -o IdentitiesOnly=yes -i /tmp/ssh04-myhsZd/keys/id_alpha -v learner@127.0.0.1 true 2>&1 | grep -E 'Will attempt key|Offering public key|Permission denied'
  （这条按设计会失败，就为了看真实报错原文）
debug1: Will attempt key: /tmp/ssh04-myhsZd/keys/id_alpha ED25519 SHA256:9dUG+bPFcC13n2dibt38fCxCZACQW3kGXloDZfKWga0 explicit agent
debug1: Will attempt key: /tmp/ssh04-myhsZd/keys/_no-default-probe  explicit
debug1: Offering public key: /tmp/ssh04-myhsZd/keys/id_alpha ED25519 SHA256:9dUG+bPFcC13n2dibt38fCxCZACQW3kGXloDZfKWga0 explicit agent
learner@127.0.0.1: Permission denied (publickey,password).
```

看点全在**行尾标签**：`explicit agent` —— 两个标签同时出现。 含义：这把身份**既是**命令行显式给的（`explicit`），**又是** agent 里已有的（`agent`）。 OpenSSH 会做这样一件事：给了 `-i` 且 `IdentitiesOnly=yes` 时， 它仍然会问 agent「你有这把吗」；有就用 agent 里的那份（不用读私钥文件、不用口令）， 没有才去读磁盘文件。所以这一行是「`IdentitiesOnly` 并不会踢掉 agent，只是限定了候选集合」的铁证。

再注意 `Offering` **只出现 1 次**（场景 6 里出现了 6 次），这就是修法的雏形。

**常见翻车**

- 以为 `IdentitiesOnly=yes` = 「只用文件不用 agent」。错：它只是**限制候选**， agent 里匹配的那把仍然优先被使用（省口令）。
- 只写 `IdentitiesOnly=yes` 但**不给任何 `IdentityFile`**：那候选集就只有默认文件 + 挡箭牌， 你 agent 里的钥匙全被无视 —— 表现为「明明 `ssh-add -l` 有，却还问我要口令」（因为文件有口令）。

---

## 场景 4：`ssh-add` 的高级开关（这一段全部本机真跑）

### 4.1 `-t`：装进去的同时挂一个「自动过期」
```text
$ ssh-add -t 3600 /tmp/ssh04-myhsZd/keys/id_alpha
Identity added: /tmp/ssh04-myhsZd/keys/id_alpha (key-alpha@sandbox)
Lifetime set to 01:00:00
```

`Lifetime set to 01:00:00`。这个Lifetime 是**存在 agent 的内存里**的（每条身份各一个到期时刻）， 不是文件属性、也不是你 shell 的状态。
### 4.2 过期真的会发生：现场等一次
```text
$ ssh-add -D >/dev/null; ssh-add -t 1 /tmp/ssh04-myhsZd/keys/id_alpha; ssh-add -l
All identities removed.
Identity added: /tmp/ssh04-myhsZd/keys/id_alpha (key-alpha@sandbox)
Lifetime set to 00:00:01
256 SHA256:9dUG+bPFcC13n2dibt38fCxCZACQW3kGXloDZfKWga0 key-alpha@sandbox (ED25519)
```

装上时 `ssh-add -l` 还能看到那把（第 212 行）。然后：

```text
$ ssh-add -l
  （这条按设计会失败，就为了看真实报错原文）
The agent has no identities.
  [退出码 1 —— 非 0 是预期的]
```

**睡 3 秒之后，同一把钥匙自己没了。** 这条实验的价值在于： 以后你遇到「我的钥匙隔一会儿就没了」，先想 `ssh-add -t` / `SSHAGENT_DEFAULT_LIFETIME` / keyring 的过期策略，而不是怀疑 agent 崩了。 反过来，如果你希望「CI 里跑完这一轮就自动失效」，`-t` 正是最省事的实现（不需要脚本再去 `ssh-add -D`）。
### 4.3 `-x` / `-X`：给 agent 上锁、解锁

先把状态准备好：`ssh-add -D` 清空、`ssh-add <沙箱>/keys/id_alpha` 装一把、`ssh-add -l` 确认它在（capture 第 223-226 行，输出与 4.1/4.2 那两条同形状）。

上锁：

```text
$ SSH_ASKPASS=/tmp/ssh04-myhsZd/askpass.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0 ssh-add -x
Agent locked.
```

**锁上之后，连公钥列表都拿不到了**：

```text
$ ssh-add -L
  （这条按设计会失败，就为了看真实报错原文）
The agent has no identities.
  [退出码 1 —— 非 0 是预期的]
```

`ssh-add -L` 的报错和「agent 是空的」**一模一样**。这就是为什么你需要下面这条辨识符。
### 4.4 极具辨识度的一行：`agent contains no identities`
```text
$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/tmp/ssh04-myhsZd/kh -o IdentityFile=/tmp/ssh04-myhsZd/keys/_no-default-probe -o ConnectTimeout=8 -v learner@127.0.0.1 true 2>&1 | grep -E 'get_agent_identities|Offering|No more auth|Permission denied'
  （这条按设计会失败，就为了看真实报错原文）
debug1: get_agent_identities: bound agent to hostkey
debug1: get_agent_identities: ssh_fetch_identitylist: agent contains no identities
debug1: No more authentication methods to try.
learner@127.0.0.1: Permission denied (publickey,password).
```

对比场景 3 里 agent 正常时的两行：

| 情况 | `-v` 里 `get_agent_identities` 的输出 |
| --- | --- |
| agent 正常、有 3 把 | `get_agent_identities: agent returned 3 keys` |
| agent **被上锁** | `get_agent_identities: ssh_fetch_identitylist: agent contains no identities` |
| `IdentityAgent=none` | 这一条 debug 根本不出现 |

于是你有一个可靠的三分法。 （顺带说明：**为什么脚本能不用交互**：`SSH_ASKPASS=<脚本> SSH_ASKPASS_REQUIRE=force DISPLAY=:0`， 那个 `askpass.sh` 里就一句 `echo "sandbox-lock-phrase"` —— 口令是**沙箱里现造的锁口令**， 不是你的任何真实私钥口令，也不来自任何真实凭据。）

解锁 + 确认回来了：

```text
$ SSH_ASKPASS=/tmp/ssh04-myhsZd/askpass.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0 ssh-add -X; ssh-add -l
Agent unlocked.
256 SHA256:9dUG+bPFcC13n2dibt38fCxCZACQW3kGXloDZfKWga0 key-alpha@sandbox (ED25519)
```

`-x/-X` 的实际用途：笔记本借给人演示、或者你离开座位但不想关终端。 锁上之后连着的会话（尤其是场景 7 的复用 master）**不受影响**， 只是新的 `ssh` 拿不到身份了 —— 这一点很容易让人误判成「复用怎么坏了」。
### 4.5 钥匙串集成：本机只能验一半
```text
$ ssh -F /dev/null -o AddKeysToAgent=yes -G learner@127.0.0.1 2>/dev/null | grep -i addkeystoagent
addkeystoagent true
```

配置项能设上（`ssh -G` 的 dump 里 `addkeystoagent true`），这一半在本机验证了。 脚本紧接着把三行注释原样打了出来，我们照录（它们属于「需要你到自己环境里验证」的部分）：

```text
  Linux：config 里写 AddKeysToAgent yes（配合 askpass / gnome-keyring）；
  macOS：老写法 ssh-add -K 已废弃，现在是 ssh-add --apple-use-keychain + UseKeychain yes。
  ⚠ AddKeysToAgent 只有在【认证真的成功过】之后才会把 key 落进 agent。本机认证不可能成功，
     所以下面这条实验只能证明『认证失败时它不会加』，成功路径需你自己的服务器验证。
```

实验：清空 agent，然后用 `AddKeysToAgent=yes` + `IdentitiesOnly=yes` + `-i` 连一次（注定失败）：

```text
$ ssh-add -l 2>&1 | head -1
The agent has no identities.
```

```text
$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/tmp/ssh04-myhsZd/kh -o IdentityFile=/tmp/ssh04-myhsZd/keys/_no-default-probe -o ConnectTimeout=8 -o AddKeysToAgent=yes -o IdentitiesOnly=yes -i /tmp/ssh04-myhsZd/keys/id_alpha learner@127.0.0.1 true
  （这条按设计会失败，就为了看真实报错原文）
no such identity: /tmp/ssh04-myhsZd/keys/_no-default-probe: No such file or directory
learner@127.0.0.1: Permission denied (publickey,password).
  [退出码 255 —— 非 0 是预期的]
```

```text
$ ssh-add -l 2>&1 | head -1
The agent has no identities.
```

**两条结论**：

1. `no such identity: <沙箱>/keys/_no-default-probe: No such file or directory` 这条报错必须解释清楚： `IdentitiesOnly=yes` 会把候选集限定在 `IdentityFile` 列表里，而我们的挡箭牌文件**确实不存在**， 于是 ssh 提前抱怨一句。**这是挡箭牌的副作用，不是 bug**，也正是「显式给 IdentityFile = 不再自动探测」的代价。
2. 认证失败时 `AddKeysToAgent` **不会**把钥匙塞进 agent（第 273 行仍是空的）。 「认证成功之后才落进 agent」这半句 **本机未能复现**（不能登录成功是本章的红线）。

---

## 场景 5：「我明明加了 key 怎么还要口令」的头号原因 —— 门牌号传不过去

### 5.1 我的 shell 是对的
```text
$ echo "当前 shell:   SSH_AUTH_SOCK=$SSH_AUTH_SOCK"
当前 shell:   SSH_AUTH_SOCK=/tmp/ssh04-myhsZd/agent.sock
```
### 5.2 同一个用户、换一个干净的进程树，就没了
```text
$ env -i PATH=$PATH HOME=$HOME bash -c 'echo "  子 shell 里: SSH_AUTH_SOCK=${SSH_AUTH_SOCK:-（空）}"; ssh-add -l 2>&1 || echo "  => 这就是新开终端 / 进容器 / sudo 之后遇到的情形"'
  子 shell 里: SSH_AUTH_SOCK=（空）
Could not open a connection to your authentication agent.
  => 这就是新开终端 / 进容器 / sudo 之后遇到的情形
```

`env -i` 把环境清空，只带 `PATH` 和 `HOME`。于是子 shell 里 `SSH_AUTH_SOCK` 为空， `ssh-add -l` 回到场景 0 的**长相 1**。

这就是本质：**`SSH_AUTH_SOCK` 是「按进程树复制」的普通环境变量**。 它不会自动出现在别的终端、别的会话、别的用户身份里。任何一条会重建环境的路径都会把它洗掉。
### 5.3 `sudo`：本机跑不动，只给出真实报错（绝不猜密码）
```text
$ sudo -n env 2>&1 | head -1
  （这条按设计会失败，就为了看真实报错原文）
sudo: interactive authentication is required
  在你能 sudo 的机器上，请自己对比这三条（这才是『sudo 把 agent 洗掉』的真实证据）：
    ssh-add -l
    sudo ssh-add -l          # 大概率：Could not open a connection to your authentication agent.
    sudo -E ssh-add -l       # -E 保留环境变量，才可能延续同一个 agent
```

`sudo: interactive authentication is required` —— 这台机器的 sudo 需要交互口令， 脚本按红线**不猜密码**，所以这里只留下真实报错。 「`sudo` 把 agent 洗掉」这件事请你在**能 sudo 的机器上**自己对比这三条（脚本也照原样打出来了）：
这三条命令的输出 **本机未验证**；`-E` 保留环境，但很多 sudoers 里 `env_reset` + `env_keep` 白名单会把 `SSH_AUTH_SOCK` 剔掉，于是 `-E` 也不灵 —— 这时得显式 `sudo SSH_AUTH_SOCK="$SSH_AUTH_SOCK" <命令>`。）
### 5.4 谁会把你的门牌号洗掉（照着这张表排查）
| 场景 | 为什么丢 | 正确姿势 |
| --- | --- | --- |
| 新开一个终端窗口 | 每个终端是独立进程树，只有登录会话注入的那一份 | 桌面/发行版一般由登录会话自动给（本机就是 `gcr-ssh-agent`）；否则在 `.profile` 里做「没有就起一个，有就复用」 |
| tmux / screen 老 session 里的窗口 | server 进程是**当年**那个环境，新变量的值进不去 | `set-environment -g SSH_AUTH_SOCK "$SSH_AUTH_SOCK"`，或用固定 socket 路径的 systemd user agent |
| `sudo` / `su -` | `env_reset`、新 login shell | `sudo SSH_AUTH_SOCK="$SSH_AUTH_SOCK" …` 或 `sudo -E …`（受 sudoers 白名单限制） |
| `docker exec` / 进容器 | 容器是全新的环境，宿主变量传不进去 | `docker exec -e SSH_AUTH_SOCK=/agent.sock …` 并把 agent socket `-v` 挂进去；或干脆容器里 `ssh-add`（把私钥交出去，风险你清楚） |
| cron / systemd 服务 | 服务环境由 unit 文件定义，不继承你的登录会话 | `Environment=SSH_AUTH_SOCK=…`（不推荐跨用户），或用专门 agent 的 socket 路径 + `IdentityAgent` |
| 重启机器 | agent 进程死了、socket 文件被回收（场景 8 就是这个） | 用 keyring（gcr/kwallet/钥匙串）或登录时自动 `eval "$(ssh-agent -s)"` |
| 桌面自动给的 agent 被换了 | `SSH_AUTH_SOCK` 值变了（路径里的 PID 变了） | 每次操作前 `ssh-add -l` 确认「你正在跟哪个 agent 说话」 |

一个可以直接抄进 `~/.bashrc` 的「有则复用、无则新建」小段（OpenSSH 10.2 默认路径规则下的写法）：

```bash
# 没有 agent，或记录的 socket 已经不存在时，起一个；有就复用
if ! ssh-add -l >/dev/null 2>&1; then
  case "$(ssh-add -l 2>&1)" in
    *"Could not open a connection"*)
      eval "$(ssh-agent -s -a "$HOME/.ssh/agent/$(hostname)-$uid固定名" >/dev/null)" ;;
  esac
fi
```

（注意上面这段是本手册的**建议写法**，未在本机跑过 —— 它会往你的 `~/.ssh/agent/` 里建 socket， 而本章脚本一条命令都没有碰过你的家目录，所以也没法用它来验证。真要用请先在测试机上试。）

---

## 场景 6：agent 里钥匙太多 → `Too many authentication failures`（本机真能复现）

### 6.1 先把 agent 塞到 8 把
```text
--- 往 agent 里塞到 8 把（模拟『公司/个人/临时项目一共十几把 key』）
Identity added: /tmp/ssh04-myhsZd/keys/j1 (junk-1@sandbox)
Identity added: /tmp/ssh04-myhsZd/keys/j2 (junk-2@sandbox)
Identity added: /tmp/ssh04-myhsZd/keys/j3 (junk-3@sandbox)
Identity added: /tmp/ssh04-myhsZd/keys/j4 (junk-4@sandbox)
Identity added: /tmp/ssh04-myhsZd/keys/j5 (junk-5@sandbox)
Identity added: /tmp/ssh04-myhsZd/keys/j6 (junk-6@sandbox)
Identity added: /tmp/ssh04-myhsZd/keys/j7 (junk-7@sandbox)
Identity added: /tmp/ssh04-myhsZd/keys/j8 (junk-8@sandbox)

$ ssh-add -l | wc -l
8
```

`ssh-add -l | wc -l` 得 `8`。这里 `wc -l` 之所以能当计数用，是因为 agent 非空时每把钥匙正好一行； **agent 空时**输出是一行报错文本（`The agent has no identities.`），那时这个计数是 1，不是 0 （脚本在第 261 行的注释里专门点了这一点，见场景 4 那节）。
### 6.2 连一次：一把一把亮，服务端数到第 6 次翻脸
```text
$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/tmp/ssh04-myhsZd/kh -o IdentityFile=/tmp/ssh04-myhsZd/keys/_no-default-probe -o ConnectTimeout=8 -v learner@127.0.0.1 true 2>&1 | grep -E 'Offering public key|Received disconnect|Disconnected' | tail -5
  （这条按设计会失败，就为了看真实报错原文）
debug1: Offering public key: junk-4@sandbox ED25519 SHA256:zfleM1O5EQggsIb/cVCGqdLKiMqlUaC3IEHQ0pmcWE8 agent
debug1: Offering public key: junk-5@sandbox ED25519 SHA256:k3OQD/ubv5Iw6GSJef3RXJoZfIleHOTGjJQJxPSEZw4 agent
debug1: Offering public key: junk-6@sandbox ED25519 SHA256:PrEVrm0R47IFsgLAbJZ1EzuyEQxXOI+q072oRYPuX5Q agent
Received disconnect from 127.0.0.1 port 22:2: Too many authentication failures
Disconnected from 127.0.0.1 port 22
```

`tail -5` 只留了最后 5 行，所以你能看到 `junk-4`、`junk-5`、`junk-6` 三次 offer， 然后就是 `Received disconnect from 127.0.0.1 port 22:2: Too many authentication failures`。 **第 6 次 offer 之后连接被服务端主动断开**，连 `Permission denied` 都等不到 —— 这比失败更糟， 因为你根本不知道是哪一步坏的。
### 6.3 机制：`MaxAuthTries` 数的是「提交次数」，而不是「你试了几台机器」

服务端（`sshd`）对每个连接允许的身份验证尝试次数由 `MaxAuthTries` 控制 （`man 5 sshd_config` 的默认值是 6；发行版常改小，比如 Debian 系默认写成 3）。 OpenSSH 的 publickey 认证里，**每一次 `Offering public key`（只带公钥的试探）都算一次尝试**。 于是一条链就出来了：agent 里有 N 把钥匙（且没限制候选）→ 客户端最多能把 N 次 offer 打到服务端 → N ≥ MaxAuthTries 时， 你在「还没轮到正确的那把」之前就被踢了。

典型翻车现场：`~/.ssh/config` 里给 `Host *` 配了一个通用 agent，或者你 agent 里 存着「公司 / 个人 / 临时项目 / 老笔记本」共十几把 —— 于是**永远是前几把在挨枪**。

还有一个更隐蔽的放大因素：客户端在问 agent 之前，还会**自动探测 `~/.ssh/` 下的默认文件名** （`id_rsa`/`id_ecdsa`/`id_ed25519`…），这些也各算一次。本章用挡箭牌 `IdentityFile` 把它们全堵掉了，所以你只看到 agent 的 8 把在挨枪，没看到家目录的份。
### 6.4 修法：只给它一把
```text
$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/tmp/ssh04-myhsZd/kh -o IdentityFile=/tmp/ssh04-myhsZd/keys/_no-default-probe -o ConnectTimeout=8 -o IdentitiesOnly=yes -i /tmp/ssh04-myhsZd/keys/j1 -v learner@127.0.0.1 true 2>&1 | grep -E 'Offering public key|Received disconnect|Permission denied'
  （这条按设计会失败，就为了看真实报错原文）
debug1: Offering public key: /tmp/ssh04-myhsZd/keys/j1 ED25519 SHA256:Am48txjbeIf9+A8YxXON3+yeGNXzbcASD0i6wvzDeSU explicit agent
learner@127.0.0.1: Permission denied (publickey,password).
```

只出现 **1 次** `Offering`，服务端于是客客气气地回了 `Permission denied`（而不是 disconnect）。 **「被拒绝」比「被踢下线」有用得多**：前者告诉你这把钥匙不对，后者什么都没告诉你。

行尾标签这次是 `explicit agent` —— 又一次说明：`IdentitiesOnly=yes` 并不排斥 agent， 它排斥的是「你没显式列出来的那些候选」。

### 6.5 三档修法对照
| 档 | 写法 | 适合 |
| --- | --- | --- |
| 精确到一台 | `~/.ssh/config`：`Host prod` + `IdentityFile ~/.ssh/id_prod` + `IdentitiesOnly yes` | 日常主力，一台机器一把钥匙，报错可读 |
| 精确到一个项目 | 项目专用 agent：`SSH_AUTH_SOCK=<项目>/agent.sock`，或 config 里 `IdentityAgent <路径>` | agent 里钥匙多、又必须一次连 10 台机 |
| 减少候选但保留 agent | 只加 `IdentitiesOnly yes`，`IdentityFile` 给 1~2 把 | 临时排障 |
| 别动客户端，让服务端宽容点 | 服务端把 `MaxAuthTries` 调大 | **不推荐**：那是安全参数，不是舒适度参数 |

顺手记住另一条：`ssh -o NumberOfPasswordPrompts=0` 与 `BatchMode=yes` 是「让它别卡住」， 和「别挨枪」是两回事；`Too many authentication failures` 一定来自候选太多，不来自这两个开关。

---

## 场景 7：连接复用 `ControlMaster` —— 本机只能演「看得见的那一半」

先说清楚本节的能演与不能演：**认证失败就没有 master**，所以「复用真的生效了」那一半 （slave 秒连、`-O check` 报 `Master running`）**本机未能复现**； 但配置、默认值、记号展开、目录不存在、残留 socket、报错文案这些「看得见的那一半」全是真跑。
### 7.1 默认是关的（`ssh -G` 打最终生效值）
```text
$ ssh -F /dev/null -G learner@127.0.0.1 | grep -E '^(controlmaster|controlpath|controlpersist) '
Pseudo-terminal will not be allocated because stdin is not a terminal.
controlmaster false
controlpersist no
  ↑ 输出里没有 controlpath 这一行 = 压根没设（这是最常见的误读）
```

三个读法：

1. `Pseudo-terminal will not be allocated because stdin is not a terminal.` 这句是 **stderr**（脚本这一行没做 `2>&1`，所以它排在 stdout 之前）。它是 `ssh -G` 的固定噪音， 因为 `-G` 只是打印配置、不执行远命令、stdin 又不是 tty。**不是错误，和复用无关。** `2>/dev/null` 就能消掉（场景 4 里那条 `-G … 2>/dev/null | grep` 就是这么写的）。
2. `controlmaster false`、`controlpersist no` —— 复用默认关。
3. **没有 `controlpath` 这一行**。这是最常见的误读：它不是「空字符串」，而是 `ssh -G` 对「压根没设的、且没有默认值的选项」直接不打印。 同类的还有 `proxyjump`（不设就不出现）。**看到「没有这行」应该理解为「未设置」。**

配好之后的 dump 长什么样（**demo 之外的补充实测**，离线 `ssh -G`，不建连接；远端用户用 `-l learner` 显式给定， 免得把本机登录名打印进文档 —— 不加 `-l` 时这一行就是 `user <本机用户>`，见 7.3 的 `%r` 那格）：

```text
（补充实测：-F 一个沙箱 config，里面写 Host box + 三件套 + ForwardAgent no；命令是 ssh -F <沙箱>/cfg -l learner -G box）
user learner
hostname box
port 22
controlmaster auto
identitiesonly no
controlpath <沙箱>/cm/f48782c7e9818723b3ed957e60959904c487b220
forwardagent no
controlpersist 600
```

两个信息量很大的点：`controlpath` 里的 `%C` 已经被展开成 40 位十六进制； `ControlPersist 10m` 被**归一化成秒**（`600`）后打印 —— 你在 `-G` 里看到的是最终值，不是你写的原文。
### 7.2 推荐的三件套（以及为什么是这三件）

把 master 建起来只需要三行（脚本原样打印的就是这三行，逐行的理由见下表）：

| 关键字 | 为什么 |
| --- | --- |
| `ControlMaster auto` | `yes` 会让「第一个 ssh」强行当 master，并且如果它已经是 slave 会失败；`auto` 才是「能复用就复用，不能就自己当 master」，适合放进 `Host *` |
| `ControlPath ~/.ssh/cm-%r@%h:%p` | socket 放在**你自己的 0700 目录**里（man 明确要求目录不可被他人写）；名字里必须含 `%r@%h:%p` 或 `%C`，否则不同主机/不同用户/不同端口会撞同一个文件 |
| `ControlPersist 10m` | 没有它，最后一个客户端一退出 master 就死，复用只在「同时开着」时有效；`10m` 让 master 后台常驻 10 分钟，`git pull`/`scp`/`rsync` 连着敲就不用重认证 |

`ControlPersist` 的取值语义（`man 5 ssh_config` 原文摘要 + 本机 `-G` 实测归一化结果）：

| 你写的 | `-G` 打出来的 | 行为 |
| --- | --- | --- |
| `no`（默认） | `no` | 不进后台；初始客户端退出即关 master |
| `yes` | `yes` | 永久后台常驻，直到 `ssh -O exit` / 被杀 |
| `0` | `yes`（**注意不是 0**） | 同上：`0` 被当成正无穷用 |
| `10m` / `3600` | `600` / `3600`（秒） | 空闲（没有任何 client 挂着）超过这个时长就自动退出 |
| `never` | `Bad ControlPersist argument.` | **不是合法值**（`man` 里那些「forever」类词也不要用） |

（这张表最后两行来自 **demo 之外的补充实测**：`ssh -F /dev/null -o ControlPersist=<值> -G box` 逐个试。）
### 7.3 `%h %p %l %r` 这些记号展开成什么（用报错把它印出来）

`-O check` 的报错会把 **展开后的完整路径**原样吐出来，这是最省事的观察手法：

```text
$ ssh -O check -o ControlPath=/tmp/ssh04-myhsZd/cm/%r@%h:%p learner@127.0.0.1
  （这条按设计会失败，就为了看真实报错原文）
Control socket connect(/tmp/ssh04-myhsZd/cm/learner@127.0.0.1:22): No such file or directory
  [退出码 255 —— 非 0 是预期的]
```

```text
$ ssh -O check -o ControlPath=/tmp/ssh04-myhsZd/cm/%C learner@127.0.0.1
  （这条按设计会失败，就为了看真实报错原文）
Control socket connect(/tmp/ssh04-myhsZd/cm/d7ffceea9d6814181862a643c1719526adbe10c8): No such file or directory
  [退出码 255 —— 非 0 是预期的]
```

```text
$ ssh -O check -o ControlPath=/tmp/ssh04-myhsZd/cm/%n-%h-%p learner@127.0.0.1
  （这条按设计会失败，就为了看真实报错原文）
Control socket connect(/tmp/ssh04-myhsZd/cm/127.0.0.1-127.0.0.1-22): No such file or directory
  [退出码 255 —— 非 0 是预期的]
```

于是三条对照非常干净：同一个连接（远端用户 `learner`、主机 `127.0.0.1`、端口 `22`）， 三种模板展开成三个不同的文件名 —— `%r@%h:%p` → `learner@127.0.0.1:22`；`%C` → 40 位哈希； `%n-%h-%p` → `127.0.0.1-127.0.0.1-22`。

记号总表（`man 5 ssh_config` 的 TOKENS 段，`ControlPath` 接受 `% %%C %d %h %i %j %k %L %l %n %p %r %u`）。 **本次那列分两类**：远端那一头（`%h %p %r %n %C`）是 demo 真跑出来的原文，远端用户就是中性名 `learner`； 本机那一头（`%l %L %u %d`）只能由 ssh 从本机口令表展开、没法用假名跑，所以按本章约定写成 `<主机名>` / `<本机用户>` / `<家目录>` —— 你重跑这几格会看到你自己的名字：

| 记号 | 含义（man 原意） | 本次真实展开（连接 `learner@127.0.0.1:22`） | 说明 |
| --- | --- | --- | --- |
| `%h` | The remote hostname | `127.0.0.1` | 远端主机名（`-O check` 报错里可见） |
| `%p` | The remote port | `22` | 远端端口，`-p` 会给它赋值 |
| `%r` | The remote username | `learner` | 远端用户，`-l` / `user@` / 默认取本地用户名；demo 显式用中性名 `learner`，本机不存在这个人，所以每条连接都注定认证失败 |
| `%n` | The original remote hostname, as given on the command line | `127.0.0.1` | 命令行原始名；和 `%h` 的区别在 `CanonicalizeHostname` 改写 `%h` 之后才显现 |
| `%l` | The local hostname, including the domain name | `<主机名>` | 本机名（这台机器的 `hostname -f` 恰好与登录名同名、且没有域名后缀 —— 巧合，不是规律） |
| `%L` | The local hostname | `<主机名>` | 不含域名的版本 |
| `%u` | The local username | `<本机用户>` | ssh 从本机口令表取的，改不了 |
| `%i` | The local user ID | `1000` | uid |
| `%d` | Local user's home directory | `<家目录>` | 注意：这是把 socket 名指到家目录的写法，慎用 |
| `%j` | The contents of the `ProxyJump` option, or empty if unset | （未设，空） | **换跳板 → 换 socket** 的关键 |
| `%C` | Hash of `%l%h%p%r%j` | `d7ffceea9d6814181862a643c1719526adbe10c8` | 40 位 = SHA-1；适合「名字太长 / 路径含怪字符」时用 |
| `%%` | A literal `%` | `%` | |

`%C` 的复现（**本机手算命中 capture 的值**；前两项是 `<主机名>` 和远端用户 `learner`，可见字段拼起来就能重算）：

```text
SHA1( "<主机名>" + "127.0.0.1" + "22" + "learner" + "" ) = d7ffceea9d6814181862a643c1719526adbe10c8
```

自己复算就一条命令（把 `<主机名>` 换成你 `hostname -f` 的输出）： `printf '%s' "$(hostname -f)127.0.0.122learner" | sha1sum`。 和 capture 第 360 行一模一样 —— 也就是说 `%C` **没有任何秘密**，它只是四个可见字段的拼接哈希。 好处是长度固定、不含危险字符；坏处是**不可读**，排查「这个 socket 对应哪台机器」时不如 `%r@%h:%p` 直观。

`%j` 的作用（**demo 之外的补充实测**，离线 `-O check`，目标 `probe.invalid`、远端用户 `-l learner`， 所以这几格换成 `learner` 之后全部重跑过）：

| `ProxyJump` 的值 | 展开出的 `%C` |
| --- | --- |
| 未设 | `cd63bcceada4e89a8831a70b450daf6d5640e0dc` |
| `gate.example` | `5aa5cc7e39b9a5ee722c986fc83fa87b5eade488` |
| `gate.example:2222,a.example` | `95e75b4c5e215fa91b63223981215a30469adde4` |
| `none` | `cd63bcceada4e89a8831a70b450daf6d5640e0dc`（和未设一致） |

单跳那一条我按 `%l%h%p%r%j` 手算 SHA-1 命中（`SHA1("<主机名>probe.invalid22learnergate.example")` = `5aa5cc7e…`， `printf '%s' "$(hostname -f)probe.invalid22learnergate.example" | sha1sum` 就能复算）； 多跳（逗号列表）那条对不上原始字符串，说明 `%j` 里存的是**规整后的形式**（具体形式本机未确认）。 结论不受影响：**目标机相同、跳板链不同 → 控制 socket 不同 → 两条路径互不复用、互不污染。**
### 7.4 `ControlPath` 的目录不存在：静默不复用（最坑人的一处）
```text
$ rm -rf /tmp/ssh04-myhsZd/cm_noexist; ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=/tmp/ssh04-myhsZd/kh -o IdentityFile=/tmp/ssh04-myhsZd/keys/_no-default-probe -o ConnectTimeout=8 -v -o ControlMaster=auto -o ControlPath=/tmp/ssh04-myhsZd/cm_noexist/%r@%h:%p learner@127.0.0.1 true 2>&1 | grep -E 'auto-mux|Control socket|Permission denied'
  （这条按设计会失败，就为了看真实报错原文）
debug1: auto-mux: Trying existing master at '/tmp/ssh04-myhsZd/cm_noexist/learner@127.0.0.1:22'
debug1: Control socket "/tmp/ssh04-myhsZd/cm_noexist/learner@127.0.0.1:22" does not exist
learner@127.0.0.1: Permission denied (publickey,password).
```

只有两行 debug 说明真相：

- `auto-mux: Trying existing master at '<…>'` —— 它确实去找了。
- `Control socket "<…>" does not exist` —— 没找到。
- 然后**什么提示都没有**，直接按「不复用」这条路把连接走完（在 capture 里就是撞上 `Permission denied`）。

也就是说：**配置写错路径 = 完全没有症状，只是慢、只是每次认证**。 这就是为什么「复用为什么没生效」的第一诊断动作是 `ssh -vv` 看这两行，而不是看有没有报错。

顺带一条 `man` 原文，正好解释了「为什么可以不报错」：

```text
       If  the  ControlPath  cannot be opened, ssh(1) will continue without  con‐
       necting to a master instance.
```
### 7.5 残留 socket / 那个位置上放了普通文件：`-O check` 的真实症状
```text
$ ssh -O check -o ControlPath=/tmp/ssh04-myhsZd/cm/stale-regular learner@127.0.0.1
  （这条按设计会失败，就为了看真实报错原文）
Control socket connect(/tmp/ssh04-myhsZd/cm/stale-regular): Connection refused
  [退出码 255 —— 非 0 是预期的]
  已留下一个进程已死、但文件还在的 socket: /tmp/ssh04-myhsZd/cm/stale.sock
```

```text
$ ssh -O check -o ControlPath=/tmp/ssh04-myhsZd/cm/stale.sock learner@127.0.0.1
  （这条按设计会失败，就为了看真实报错原文）
Control socket connect(/tmp/ssh04-myhsZd/cm/stale.sock): Connection refused
  [退出码 255 —— 非 0 是预期的]
```

两条都是 **`Control socket connect(…): Connection refused`**，退出码 255。 这里有一个很有价值的内核细节：脚本放了两种东西在 `ControlPath` 上—— 一个是**普通文件**（内容 `not a socket`），一个是**已 bind 但进程已退出、没 unlink 的 socket 文件** （用 python `bind()` 后直接 `close()` 造出来，第 382 行就是那句提示）。 两者在 `connect()` 层面给出**同一个 errno（ECONNREFUSED）**，所以文案一模一样。 对照 7.4（目录都不存在 → `No such file or directory`），你手里就有三种可分辨的状态：

| `-O check` 的报错 | 路径上的东西 | 含义 |
| --- | --- | --- |
| `… No such file or directory` | 什么都没有（或目录都没有） | 从没建过 master，或目录路径写错 |
| `… Connection refused` | 普通文件 / 死掉的 socket 文件 | 有残留；也可能你连的其实是别人的旧文件 |
| `Master running (pid=…)` | 活的 master | 一切正常（**本机未能复现**，需要登录成功） |

**但真正的 ssh 客户端（不是 `-O check`）会自己动手清理**。下面是 **demo 之外的补充实测**： 同样造两个「脏路径」（普通文件 + 死 socket），然后用 `ControlMaster=auto` 连本机 `learner@127.0.0.1`（认证注定失败）：

```text
（补充实测 A：ControlPath=<沙箱>/cm/reg 上放一个普通文件）
debug1: auto-mux: Trying existing master at '<沙箱>/cm/reg'
debug1: Stale control socket <沙箱>/cm/reg, unlinking
learner@127.0.0.1: Permission denied (publickey,password).
→ 事后 ls -l 该路径：ls: cannot access '<沙箱>/cm/reg': No such file or directory（文件被 ssh 自己删了）

（补充实测 B：ControlPath=<沙箱>/cm/dying 上是「进程已死、文件还在」的 socket）
debug1: auto-mux: Trying existing master at '<沙箱>/cm/dying'
debug1: Stale control socket <沙箱>/cm/dying, unlinking
learner@127.0.0.1: Permission denied (publickey,password).
→ 同样被 unlink，事后 ls 也是 No such file or directory

（对照：同一个死 socket 路径，换 ssh -O check 只报 Connection refused，退出码 255；
 事后 ls -l 那个文件【还在】—— -O check 纯只读，不清理）
```

三个结论：

1. `ssh`（走 mux 的正常连接）会 **unlink 掉自己认为是残留的** control 路径，然后正常建新连接。
2. `ssh -O check` 是**纯只读**的：它只 connect、只报告，不清理。
3. 因此也有一条**真实的危险面**：如果你把 `ControlPath` 放进世界可写目录（`/tmp`）且文件名可预测， 别的用户可以先放一个文件在那个路径上，你的 ssh 客户端会**替你把那个文件删掉**（unlink 的是属主为他的文件也照删， 取决于目录权限）。这就是 `man` 里那句 **「it is recommended that any ControlPath … be placed in a directory that is not writable by other users」** 的硬核理由 —— 不只是「别人能连你的会话」，还有「别人能借你的手删文件」。
### 7.6 认证失败就不会有 master：把「本机给不了」讲清楚
```text
$ ls -A /tmp/ssh04-myhsZd/cm | grep -v stale || echo '  （没有 learner@127.0.0.1:22 这种复用 socket）'
  （没有 learner@127.0.0.1:22 这种复用 socket）
```

`ls -A` 之后只剩那两个 `stale-*`（被 `grep -v stale` 滤掉）， 所以 `<沙箱>/cm/` 里**没有** `learner@127.0.0.1:22` 这个 master socket。原因： **master 是「认证成功之后」才 bind 出来的**。认证阶段失败 → 进程直接退出 → 一次 bind 都没发生。

我给你一份完整模板（把 `box` 换成你的主机；这些命令的输出 **本机未验证**）：

```bash
mkdir -p ~/.ssh && chmod 700 ~/.ssh          # 目录权限是复用安全的前提
time ssh box true      # 第 1 次：完整 TCP 握手 + KEX + 认证，典型 0.3~1.5s
time ssh box true      # 10 分钟内第 2 次：复用，典型 0.02~0.08s（省掉握手与认证）
ls -l ~/.ssh/cm-*      # 应该看到一个 srw------- 的 socket，owner 是你
ssh -O check box       # Master running (pid=12345)
ssh -X box             # 复用会话里同样能带 agent 转发，但用的是 master 那一份（见 7.8）
ssh -O stop box        # 不再接受新的 slave；已挂着的继续用
ssh -O exit box        # 让 master 退出（会连带踢掉挂着的 slave）
```

「预期数字」怎么读：第二次的时间应该只跟本机 socket 往返 + 服务端开 channel 有关， **不应该再出现任何与 `Permission denied`/口令相关的输出**。如果第二次仍然要口令， 说明你压根没复用成功，回去看 7.4 那两行 debug。
### 7.7 `ControlMaster` 的取值（含 `ask`/`autoask`）

取值与 `-G` 显示形式见文末速查表 2（`-G` 逐个接受是 **demo 之外的补充实测**：`ssh -F /dev/null -o ControlMaster=<值> -G box`）：

`man 5 ssh_config` 的原文（值得读一遍，它是 `yes` 与 `auto` 分工的定义）：

```text
       ControlMaster
	       Enables the sharing of multiple sessions over a	single	network
	       connection.  When set to yes, ssh(1) will listen for connections
	       on  a  control  socket specified using the ControlPath argument.
	       Additional sessions can connect to this socket  using  the  same
	       ControlPath  with  ControlMaster set to no (the default).  These
	       sessions will try to reuse the master instance's network connec‐
	       tion rather than initiating new ones, but will fall back to con‐
	       necting normally if the control socket does not exist, or is not
	       listening.
	（摘录有删节：后一段讲 ask 需要 ssh-askpass、以及 ControlPath 打不开时继续正常连接）
```

**手册里最常被忽略的一句**就是上面摘录里的 `will fall back to connecting normally`： 复用不成时 slave 侧走回正常连接。 所以复用的失败模式**永远是静默降级**，不会给你任何错误 —— 想看到它必须 `-vv`。

`ask`/`autoask` 的现实用法很少：它们需要 `ssh-askpass`（GUI 或 7.4 那套环境变量）， 在没有图形助问器的机器上会变成「问不到 → 不建 master」，比 `auto` 更容易出问题。 除非你明确要「每次建 master 都由我批准」，否则一律用 `auto`。
### 7.8 复用与 agent 的交叉：`ForwardAgent` 属于 master

`man 5 ssh_config` 里紧接在后面的一段，正是本章两个主题的连接点：

```text
       X11  and	 ssh-agent(1) forwarding is supported over these multi‐
       plexed connections, however the display and agent forwarded will
       be the one belonging to the master connection  i.e.  it	is  not
       possible to forward multiple displays or agents.
```

翻译成大白话，三条后果：

1. **第一次连接时的 agent 状态会被「冻」进 master。** 如果 master 建立时你 agent 是空的（或你 `ssh -a` 关了转发）， 后面复用同一个 master 的会话**也拿不到转发** —— 哪怕你现在已经 `ssh-add` 好了。 表现就是「第二次连怎么不带 agent 了？」→ 解法：`ssh -O exit box` 关掉 master，重新连。
2. 反过来：你 `ssh-add -x` 上了锁、或 `ssh-add -D` 清空，**已经挂在 master 上的会话不受影响**； 而新建 slave 复用 master 时压根不走认证（场景 7 图 2），所以也不碰 agent。 于是「agent 明明锁了，怎么还能免密进去」= 你复用了 master，不是 agent 漏了。
3. **一条 master 只能带一份转发 agent**（`ForwardAgent` 是**连接级**属性，不是会话级）。 同一台远端上想同时用两把不同身份，只能建两条不同的 master（不同 `ControlPath`， 或不同 `SSH_AUTH_SOCK` 起两个 agent + 两套 Host 别名）。

`ssh -O` 的子命令一览（`man 1 ssh` 原文列出的合法值）：

| 命令 | 语义 | 本机能否验证 |
| --- | --- | --- |
| `-O check` | 查 master 活着没（活：`Master running (pid=…)`；否则报 connect 错误） | **只能验证失败分支**（7.3、7.5 的三条报错） |
| `-O forward` / `-O cancel` | 在不重新登录的前提下加减端口转发（第 05 章的正主） | 需要活的 master，**未验证** |
| `-O proxy` | 以 proxy 模式接一个已存在的 master（`ProxyJump` 内部会用得上） | **未验证** |
| `-O stop` | 让 master 不再接受新的复用请求（已挂着的继续） | **未验证** |
| `-O exit` | 让 master 退出 | **未验证** |
### 7.9 `ProxyJump` 怎么「复用同一连接」

`-J`/`ProxyJump` 的定义（`man 1 ssh` 原文）：

```text
       -J destination
	       Connect to the target host by first making an ssh connection to
	       the  jump  host described by destination and then establishing a
	       TCP forwarding to the ultimate destination from there.  …
	       …（中间略：多跳用逗号分隔、IPv6 用方括号、这是 ProxyJump 指令的简写）
	       Note   that  configuration  directives  sup‐
	       plied  on  the  command-line  generally  apply  to  the  destina‐
	       tion host and not any specified jump hosts.	 Use  ~/.ssh/config
	       for  jump hosts.
```

拆开看机制：

```
   ssh -J gate target
        │
        └─ 内部等价于一条 ProxyCommand：  ssh -W target:22 gate
                                         └ 这是一个独立的 ssh 进程（子进程），
                                           它会读 ~/.ssh/config（不读你给外层的 -o）
              ┌───────────────────────────┘
              ▼
        到 gate 的一条完整 SSH 连接（认证 gate）
              │  在 gate 上开一条 direct-tcpip channel，目的地 target:22
              ▼
        外层 ssh 把它的 SSH 协议跑在这条 socket 上 → 认证 target
```

于是**两级复用的关键是给跳板机也配三件套**（因为 `-o` 到不了子进程，只有 config 能）：

```sshconfig
Host gate
    ControlMaster auto
    ControlPath   ~/.ssh/cm-%r@%h:%p
    ControlPersist 10m

Host target
    ProxyJump gate
    # 这一层同样配三件套，第二次 ssh target 连 TCP 都不用重建
    ControlMaster auto
    ControlPath   ~/.ssh/cm-%r@%h:%p
    ControlPersist 10m
```

效果分三档（都**未在本机验证**，请在你的跳板环境里量）：

| 配置 | 第 2 次 `ssh target` 省掉了什么 |
| --- | --- |
| 只有 `Host target` 三件套 | 整条链路都不重建（连 gate 的那次认证也省了）——最省，但 master 死了会全部重来 |
| 只有 `Host gate` 三件套 | 省掉「到跳板机的握手与认证」；到 target 那一跳仍然重新认证 |
| 两级都配 | 第二次秒连；换 target 时也能复用 gate 那条 |

三个必须记住的细节：

1. **命令行 `-o` 到不了跳板机**（原文那句 `generally apply to the destination host`）。 所以 `ssh -J gate -o ControlPath=… target` 只配置了 target 侧；gate 侧要写进 `~/.ssh/config`。 （想显式控制内层 ssh，用 `-o ProxyCommand="ssh -F 你的config -o ControlPath=… -W %h:%p gate"`， `%h %p` 由外层展开。）
2. **`%C` 含 `%j`（7.3 实测）**：同一 target 走不同跳板链 = 不同 control socket， 不会出现「借了一条走另一条路建立的会话」这种串味问题。 反过来说：如果你手工用 `%r@%h:%p`（不含 `%j`），换跳板时会复用同一个 socket 名 —— 这在语义上是对的（同一 target 同一用户同一端口），但排查时别把两个 socket 弄混。
3. **跳板机的 `ControlPersist` 要够长**：gate 的 master 一旦过期， 所有「借它过去」的新 target 连接都要重新认证 gate，表现成「时快时慢」。

---

## AgentForwarding 专章：转的到底是什么

### 先把三个容易混淆的东西摆正
| 问题 | 答案 |
| --- | --- |
| 远端拿到的 `SSH_AUTH_SOCK` 的值，和你本地的相同吗？ | **不相同**。远端那个路径由**远端 sshd 新建**（常见形态 `/tmp/ssh-XXXXXX/agent.<pid>` 之类），是远端的本地路径 |
| socket 文件本身有被「传」过去吗？ | 没有。文件不出机器；被传的只有**消息字节**，走一条已认证的加密 channel |
| 远端能拿到我的私钥吗？ | 不能。agent 协议里没有任何「给我私钥字节」的请求类型（这正是 agent 存在的全部意义） |
| 那远端能干什么？ | 能让你本地 agent **以你的身份签名任意次**（无锁、无过期、无确认时）→ 等价于「拿你的身份去认证任何你 agent 里有钥匙的机器」 |

`man 1 ssh` 的 `-A` 原文把风险写得非常直白：

```text
       -A      Enables  forwarding  of  connections  from  an  authentication
	       agent such as ssh-agent(1).  …
	       （略：也可以按 Host 写进配置文件）
	       Agent  forwarding  should  be  enabled  with  caution.  Users with
	       the ability to bypass file permissions on the remote host  (for  the
	       agent's  Unix-domain  socket)  can  access  the  local  agent  through
	       the forwarded connection.  An attacker cannot obtain key  material
	       from  the  agent,  however  they  can perform operations on the
	       keys that enable them  to  authenticate  using  the  identities
	       loaded into the agent.  A  safer  alternative  may  be  to  use  a  jump
	       host (see -J).
```

**「A safer alternative may be to use a jump host」**这句是本章的收束： 大多数 `-A` 的真实需求，其实应该是 `-J`（或 `-L` 到内网）。
### 客户端开关 vs 服务端开关
| 层 | 关键字 / 选项 | 默认 | 本机验证状态 |
| --- | --- | --- | --- |
| 客户端 | `-A`（= `ForwardAgent yes`） | `forwardagent no`（`ssh -G` 可见） | 默认值可由 `-G` 读出（补充实测：`-G` 打 `forwardagent no`；`-o ForwardAgent=yes` 后打 `forwardagent yes`、`confirm` 打 `confirm`。注意它和 `addkeystoagent` 不一样 —— 后者被归一化成 `true`/`false`） |
| 客户端 | `-a` / `ForwardAgent no` | —— | 关掉，用于在 `Host *` 里对个别主机豁免 |
| 客户端 | `ForwardAgent confirm` | —— | 每次远端用你的 agent 都弹窗确认（配合 askpass） |
| **服务端** | **`AllowAgentForwarding`**（`sshd_config`） | `yes` | **本机未验证**（不读你的 `/etc/ssh/sshd_config`，不重启 sshd）；你自己查：`sudo sshd -T \| grep -i allowagentforwarding` |
| 服务端 | `StreamLocalBindMask`（默认 `0177`） | 影响转发 socket 的权限位 | 未验证 |
| 服务端 | `DisableForwarding` / `PermitOpen` | 一把闸 / 白名单 | 属于第 05 章 |

**一个真实踩点（本机实测到的）**：`AgentForwarding` 是 **sshd 侧**的关键字， 把它当客户端选项写会直接报错。**demo 之外的补充实测**：

```text
（补充实测：ssh -F /dev/null -o AgentForwarding=yes -G probe.invalid）
command-line: line 0: Bad configuration option: agentforwarding
```

也就是说：客户端 config 里要写 **`ForwardAgent yes`**；`AgentForwarding yes` 只能出现在 `/etc/ssh/sshd_config` 里。两个名字差一个字母，抄错就「整个配置项无效」（03 章讲过：坏选项会打死配置）。

`ssh -G` 里 agent 相关的三条默认值（本机实测）：`addkeystoagent false` / `forwardagent no` / （`identityagent` 不设时不打印）。
### 转发链上的 socket 是谁的：一个具体后果

`ForwardAgent` 一旦生效：远端 `sshd` 创建的那个 socket 的 owner 是**你登录的那个远端用户**。 于是：

- 远端上**同一个 uid** 的任何进程（包括别人替你跑的 cron、被你 `git clone` 下来的构建脚本、 被攻破的 web 服务）都能 `connect()` 它。
- 远端上**有 root 的人**能绕过文件权限（man 里那句 `bypass file permissions` 说的就是这个）。
- 转发 socket 的**残留**也是真实故障源：会话已经断了，`SSH_AUTH_SOCK` 还留在某个 shell 里， `ssh-add -l` 报 `Connection refused`（场景 0 长相 3）——这是远端机器上的，不是你本地 agent 坏了。

**如何在远端判断「我的 agent 是被转发的」**（这几条命令请在你自己登得上去的机器上跑，**本机未验证**）：

```bash
echo "$SSH_AUTH_SOCK"          # 指向 /tmp/ssh-*/agent.* 之类 => 十有八九是转发来的
ls -l "$SSH_AUTH_SOCK"         # owner 是自己，但路径在远端的临时目录里
ss -xlp | grep -F "$SSH_AUTH_SOCK"   # 谁在听
ssh-add -l                     # 列出来的是「你笔记本的 agent」里的钥匙，不是这台机器的
```

看到列表里出现「这台机器上根本没有的私钥」，就是转发的直接证据。 反过来，如果你不希望自己哪天不小心 `-A` 到别人机器上，把这条加进 config 里最省事：

```sshconfig
Host *
    ForwardAgent no
Host git.internal.example
    ForwardAgent yes      # 只在真正需要的那一台开
```
### agent 转发与本地 agent 的区别（对照表）
| 维度 | 本地 agent（`ssh-add` 装进去的那个） | 转发出来的 agent（远端看到的 `SSH_AUTH_SOCK`） |
| --- | --- | --- |
| 谁创建 | 你自己启动的 `ssh-agent` 进程（或 keyring 提供的兼容实现） | 远端 `sshd`（在你认证成功、且客户端请求了 `agent-Forward@openssh.com` 之后） |
| socket 位置 | 你机器上，OpenSSH 10.2 默认 `~/.ssh/agent/s.*`（本章用 `-a` 钉在沙箱） | 远端机器上，通常在远端用户的 `TMPDIR`（常见形态 `/tmp/ssh-XXXXXX/agent.<pid>`；**本机未验证**） |
| 属主 / 权限 | 你，0600（`srw-------`，场景 1 已实测） | 远端登录用户，权限由 `StreamLocalBindMask`（默认 0177）决定 |
| 生命周期 | 进程活着就活着；`ssh-agent -k` 或机器重启就没了（场景 8） | **跟着那条 SSH 连接走**：连接断了 socket 就废（文件可能残留） |
| 里面装了什么 | 你显式 `ssh-add` 的那几把 | **一个都不装**：它只是通往你本地 agent 的管道 |
| 能拿到私钥吗 | 进程内存里有明文私钥（本机 root 理论上能读内存 —— 这是 agent 的固有边界） | 不能，连内存都不在远端 |
| 谁能用 | 本机同 uid 的进程 | 远端同 uid 的进程 + 远端 root |
| 典型故障 | `Could not open…` / `No such file or directory`（场景 0） | `Connection refused`（残留）/ 复用 master 导致「不带 agent」（7.8） |
| 关掉它 | 杀进程 / `-k` | 会话结束；或客户端 `-a` / `ForwardAgent no`；或服务端 `AllowAgentForwarding no` |

---

## 场景 8：收尾 —— 亲手关掉 agent，看进程和 socket 一起消失

### 8.1 先确认它还在
```text
$ pgrep -a ssh-agent | grep -w 169710
169710 ssh-agent -s -a /tmp/ssh04-myhsZd/agent.sock
```

`pgrep -a ssh-agent | grep -w 169710`：`-w` 保证匹配的是完整 PID 而不是子串。
### 8.2 `-k` 关掉它
```text
$ SSH_AUTH_SOCK=/tmp/ssh04-myhsZd/agent.sock SSH_AGENT_PID=169710 ssh-agent -k
unset SSH_AUTH_SOCK;
unset SSH_AGENT_PID;
echo Agent pid 169710 killed;
```

注意 `-k` 也是**打印 shell 代码**（三行 unset + 一句 echo），所以日常用法是 `eval "$(ssh-agent -k)"`。脚本这里显式带上 `SSH_AUTH_SOCK/SSH_AGENT_PID` 两个变量， 是因为此时它已经不需要污染自己的 shell。
### 8.3 socket 文件被自己回收了
```text
$ ls -l /tmp/ssh04-myhsZd/agent.sock 2>&1 || echo '  （文件已不存在）'
ls: cannot access '/tmp/ssh04-myhsZd/agent.sock': No such file or directory
  （文件已不存在）
```

`ls` 说不存在 → agent **在退出时 unlink 了自己的 socket 文件**。这是「谁创建谁清理」的正例， 也解释了为什么残留 socket 通常出现在**异常退出**（进程被 `kill -9`、机器掉电）之后。
### 8.4 再问一次：回到场景 0 的长相 2
```text
$ ssh-add -l
  （这条按设计会失败，就为了看真实报错原文）
Error connecting to agent: No such file or directory
  [退出码 2 —— 非 0 是预期的]
```

`Error connecting to agent: No such file or directory`（退出码 2）。 注意这里**不是** `Could not open a connection…`：变量还在，文件才没了。 这条正是「**agent 被杀 / 机器重启后你的 shell 还留着上次的路径**」的标准症状， 把场景 0 的三种长相闭环了。
### 8.5 脚本自己的收尾与四条自检（原文）

本章会起 `ssh-agent`、会在沙箱里留 control socket，所以「**资源有没有释放**」必须自己证明。 脚本在 `EXIT` trap 里连做了四件事，capture 末尾原文照录（`/tmp/ssh04-myhsZd` 换成你自己那次的路径）：

```text
以上 8 个场景就是 README 里各小节的证据链。

[沙箱已删除] /tmp/ssh04-myhsZd

[收尾自检 1] 现在系统上还剩哪些 ssh-agent 进程
              （你桌面环境原本就有的那几支不属于本脚本；本脚本起的那支应当已消失）
  6813 /usr/libexec/gcr-ssh-agent --base-dir /run/user/1000/gcr
  6815 /usr/bin/ssh-agent -D
  13421 /usr/bin/ssh-agent -D -a /run/user/1000/gcr/.ssh
[收尾自检 2] 本脚本起过的那支 agent（pid=169710）还在吗：  已退出
[收尾自检 3] 监听端口和开工前基线对比（ss -lnt，只看第四列的地址:端口）：
  干净：一个新增监听都没有（2222/8098/8099/1080/9100 全空，0.0.0.0:22 与 [::]:22 仍是那台真实 sshd）
[收尾自检 4] 本脚本的沙箱路径下还有没有【在听的】Unix socket（ss -xl）：
  干净：ss -xl 里没有任何 /tmp/ssh04-myhsZd 下的 socket（agent.sock 随 -k 消失，cm/ 里那个残留测试 socket 随沙箱删掉）
```

读法：

- **自检 1** 列出收尾那一刻系统上所有的 `ssh-agent` 进程：那三行全都**不属于本脚本** —— `gcr-ssh-agent`（GNOME 的 agent 后端）、 它启动的 `ssh-agent -D -a /run/user/1000/gcr/.ssh`，以及桌面会话里原有的 `ssh-agent -D`。 本脚本那支 `169710` 已经不在列表里。（这也顺便证明：**一台机器上可以同时有多个 agent**， 你 `ssh-add` 到哪一支完全取决于 `SSH_AUTH_SOCK`。）
- **自检 2** 不是「看列表里有没有」，而是直接 `kill -0 169710` 问内核：这支我起的 agent 还在吗。 打印 `已退出` = `-k` 真的把进程带走了。`--keep` 模式下这条会改成提示你手工关。
- **自检 3** 是**和开工前的基线做集合对比**：脚本第一行就存了 `ss -lnt` 的地址:端口集合， 收尾再取一次，两者 `diff` 无输出才打印 `干净：一个新增监听都没有`。 这一条比「列几个端口名看看空不空」硬得多 —— 它连「你顺手起在某个冷门上」都抓得住。 22 端口那两行（`0.0.0.0:22`、`[::]:22`）从头到尾都在集合里，说明那台真实 sshd **没被重启、没被停、没被改**： 本章只被动连接它，集合不变就是证据。
- **自检 4** 用 `ss -xl` 查「本脚本沙箱路径下还有没有在听的 Unix socket」。 agent 的 `agent.sock` 随 `-k` 自己 unlink 了（8.3 实测），场景 7 那个 `cm/stale.sock` 是**bind 过但没人监听**的死 socket， `ss -xl` 本来就不列它，它随 `[沙箱已删除]` 一起消失。

跑完后我又手工复核了一次，结论一致：`ss -lnt` 里 `2222 / 8098 / 8099 / 1080 / 9100` 一个都没有， 剩下的都是这台机器**原本就有**的服务（`0.0.0.0:22`、`[::]:22` 是那台真实 sshd，`127.0.0.1:631`、 `[::1]:631` 是 CUPS，`127.0.0.53%lo:53`、`127.0.0.54:53` 是 systemd-resolved， 还有一两个 3 万段的高端口属于桌面/开发工具 —— 它们每次 `ss` 都可能不一样， 所以真正的证据是自检 3 那句「与开工前基线 `diff` 无输出」，而不是某个端口的名字）； `ss -xl | grep ssh04` 无输出，`/tmp/ssh04-*` 目录已全部消失，章目录里除了 `README.md` 和 `demo.sh` 没有任何产物。

---

## 本章速查表（汇总）

### 表 1：agent 相关环境变量
| 变量 | 谁负责写 | 谁读 | 值的样子 | 丢失/错误的症状 |
| --- | --- | --- | --- | --- |
| `SSH_AUTH_SOCK` | `ssh-agent` 打印 → `eval` 执行；或桌面 keyring / 远端 `sshd`（转发） | `ssh`、`ssh-add`、`scp`、`rsync`、`git`、`ssh-keygen`（要签名时） | Unix socket 路径（本次 `<沙箱>/agent.sock`） | `Could not open…`（未设）/ `No such file…`（路径没了）/ `Connection refused`（不是活 socket） |
| `SSH_AGENT_PID` | 同上 | 只有 `ssh-agent -k`、部分集成脚本 | 进程号（本次 `169710`） | `-k` 可能杀不掉；认证路径完全不受影响 |
| `SSH_ASKPASS` | 你 / 桌面环境 | `ssh`（要口令）、`ssh-add -x/-X`（要锁口令） | 可执行脚本路径 | 无 TTY 时拿不到口令，命令直接失败 |
| `SSH_ASKPASS_REQUIRE` | 你 | 同上 | `never` / `prefer` / `force` | 不设 = 老行为（有 TTY 就不用 askpass） |
| `DISPLAY` | 桌面 | 老版 askpass 判定 | `:0` | 纯 SSH 会话里为空，需 `SSH_ASKPASS_REQUIRE=force` 兜住 |
| `TMPDIR` | 远端 shell / `sshd` | 远端 `sshd` 决定转发 socket 建在哪 | 一般是 `/tmp` | 影响的是远端转发 socket 的位置（未在本机验证） |
| `SSH_CONNECTION` | 远端 `sshd` | 远端脚本判断来源 | 四个空格分隔的值 | 与本章无关，仅列出以免混淆 |
### 表 2：`ControlMaster` 取值
| 值 | `-G` 显示 | 建 master | 借 master | 失败时的行为 | 建议 |
| --- | --- | --- | --- | --- | --- |
| `no` | `false` | 不 | 会尝试借（残留/死掉的会被清理） | 静默新建普通连接 | 默认值 |
| `yes` | `true` | **一定自己建** | 不借 | `ControlPath` 打不开 → 继续、不复用（不报错） | 只在「这一条一定要当母连接」时用 |
| `auto` | `auto` | 没有就建 | 有就借 | 静默降级 | **`Host *` 里唯一该写的值** |
| `ask` | `ask` | 建之前问 | —— | 问不到就等价 `no` | 需要 askpass，日常不实用 |
| `autoask` | `autoask` | 同 `auto` 但问一句 | 同 `auto` | 同上 | 同上 |
| （`ControlPath none`） | —— | —— | —— | 彻底禁用 sharing | 给个别主机豁免时用 |

**`ControlPath` 选名原则**（记号全表见 7.3）：`%r@%h:%p`（可读，排查友好，但含 `@:`，在某些工具里要引号） 或 `%C`（不可读但绝对安全）。**两个都不能少**，即要么含 `%h %p %r`，要么含 `%C` —— man 的原话。 目录必须是别人不可写的（`~/.ssh` 0700 是标准答案；**不要**放 `/tmp`）。
---

## 坑清单（这 12 条最值钱）

1. **`ssh-agent -s` 忘 `eval`** —— 进程起来了，你不知道门牌号。症状：`ssh-add` 报 `Could not open a connection…`。修法：`eval "$(ssh-agent -s)"`，或直接用桌面/keyring 给的 agent。
2. **把 agent 当成有持久化的服务** —— 它只在内存里。重启机器就空。想持久化只有 keyring （`gcr-ssh-agent` / kwallet / macOS `--apple-use-keychain`）这一条路。
3. **`SSH_AUTH_SOCK` 在新终端 / `sudo` / 容器 / cron 里丢失** —— 环境变量按进程树复制，不跨会话。场景 5 那张表逐条对着查。`sudo -E` 也未必管用（`env_reset`）。
4. **一台机器多个 agent，你加错了支** —— `ss -xl | grep agent` 和 `pgrep -a ssh-agent` 会告诉你有几支（场景 1、收尾自检 1 都看到了）。 永远以 `ssh-add -l` 的当前输出为准；需要时用 `IdentityAgent <路径>` 明确指定。
5. **agent 里钥匙太多 → `Too many authentication failures`** —— 每次 `Offering` 都算一次认证尝试，`MaxAuthTries`（本机观察到第 6 次翻脸）到了就直接断连。 修法只有 `IdentitiesOnly=yes` + 精确 `IdentityFile`。
6. **`IdentitiesOnly=yes` 但不给 `IdentityFile`** —— 结果只剩默认文件，agent 被完全无视， 于是「明明加了 key 还要口令」。这是场景 3 两个对照实验合起来的结论。
7. **`AgentForwarding` 写进客户端 config** —— 那是 **sshd 侧**关键字，客户端写会 `Bad configuration option: agentforwarding`（本机实测）；客户端应该写 `ForwardAgent`。 而且一个坏选项会让整份配置出问题时，你只会看到「配置好像没生效」。
8. **`ControlPath` 指向不存在的目录 → 静默不复用** —— 没有报错，只有变慢和重复认证。 诊断只有一条路：`ssh -vv` 找 `auto-mux: Trying existing master` 与 `Control socket "…" does not exist` 这两行（7.4 实测）。
9. **`ControlPath` 用 `%h` 却不带 `%r`/`%p`** —— 同主机多账号 / 多端口撞同一个 socket， 表现是「连到了不该连的那条会话」。man 明确要 `%h %p %r`（或 `%C`）。
10. **`ControlPath` 放 `/tmp`** —— 除了「别人能连你的已认证会话」，还有一条更冷的：
客户端遇到残留会 **unlink 那个路径上的文件**（7.5 实测 `Stale control socket …, unlinking`），
于是别人可以拿你的客户端当删文件工具。目录必须不可被他人写。
11. **以为「复用之后还能换 agent / 换身份」** —— `ForwardAgent` 是**连接级**的，
属于 master；slave 会话拿的是 master 那一份（7.8 man 原文）。
想让新的 agent 生效：`ssh -O exit box` 后重连。
12. **`ControlPersist` 写成 `never` / 期望它「永不退出」还写 `0`** ——
`never` 直接 `Bad ControlPersist argument`（实测），`0` 会被归一化成 `yes`（实测）。
要「常驻」就写 `yes`；要「10 分钟」就写 `10m`（`-G` 里会看到 `600`）。

（再补三条小的：`ssh -G` 那行 `Pseudo-terminal …` 是 stderr 噪音、不是错误； 「`-G` 里看不到 `controlpath` 行」= 未设置，而不是空； `ssh-add -l | wc -l` 在 agent 空时得到 1 而不是 0，别拿它当「有没有钥匙」的判据。）

---

## 自检 8 题（先答再看折叠答案）

1. `ssh-add -l` 报 `Error connecting to agent: Connection refused`。 它和 `No such file or directory` 的病因差别是什么？你的下一步动作分别是什么？
2. 为什么「加了 key 之后新开一个终端又要口令」？用一句话讲清 `SSH_AUTH_SOCK` 的传播规则。
3. `-vv` 里 `Will attempt key: … agent` 行尾的 `agent` / `explicit` / （空白）分别表示什么？ `IdentitiesOnly=yes` 加一把 `-i` 时为什么会出现 `explicit agent` 两个标签？
4. 「第二次 `ssh` 同一台机器不再要密码」这件事，`ssh-agent` 和 `ControlMaster` 各自贡献了什么？ 如果 agent 已经被 `ssh-add -x` 锁了，为什么复用的会话还能进去？
5. `ControlPath` 写了但完全没效果，也不报错。最可能的两个原因是什么？怎么用一条命令看到真相？
6. `%C` 是怎么算出来的？为什么换一台跳板机之后 `%C` 会变？这条性质对 `ProxyJump` 复用意味着什么？
7. `ssh -A` 之后，远端用户能不能拿到你的私钥？他**具体**能做和不能做的分别是什么？ 三条能把风险压低的做法是什么？
8. 你要「临时把一把钥匙借给一次部署，一小时后自动失效」。写出最省事的命令组合， 并说明为什么它比「脚本里加一句 `ssh-add -D`」更稳。

<sub>**答案 1**：`Connection refused` = 路径上有东西但它不是一个在听的 socket（残留 socket / 普通文件 / 硬抄了别人机器的路径）；`No such file or directory` = 路径本身不存在（agent 已死 / 机器重启）。 下一步：前者 `ss -xl | grep <路径>` 看有没有 LISTEN，然后决定「重装 key」还是「换掉 `SSH_AUTH_SOCK`」； 后者直接 `ssh-agent -k` 清干净再 `eval "$(ssh-agent -s)"`（或重开终端 / 交给 keyring）。 证据：场景 0 三条命令的三种报错，以及场景 8 里 `-k` 之后回到 `No such file or directory`。</sub>

<sub>**答案 2**：`SSH_AUTH_SOCK` 只是一个普通环境变量，靠**进程树继承**传播：agent 起来时打印三行 shell 代码、由 `eval` 设在当前 shell 里；任何**不经过这条继承链**的新进程（另一个终端、`sudo`、 `su -`、`docker exec`、cron、systemd 服务、老的 tmux server）都不会拿到新值。 所以要么每个会话都注入（登录脚本 / keyring），要么把 socket 路径固定下来（`-a` + `IdentityAgent`）。 证据：`env -i PATH=… HOME=…` 之后子 shell 里 `SSH_AUTH_SOCK` 为空、`ssh-add -l` 报 `Could not open…`（场景 5.2）。</sub>

<sub>**答案 3**：`agent` = 身份来自 agent；`explicit` = 来自命令行/config 显式给的 `IdentityFile`； 空白（无类型标签）= 来自默认文件名的自动探测。`IdentitiesOnly=yes` 只是**限定候选集合**， 并不禁止用 agent：当 `-i` 给的那把在 agent 里也存在时，两个来源都成立， 于是标签写成 `explicit agent`（OpenSSH 优先用 agent 里那份，省掉口令）。 证据：场景 3.5 那次 `Offering` 只出现 1 次、行尾是 `explicit agent`。</sub>

<sub>**答案 4**：agent 的贡献是「私钥已在内存，签名不要口令」，所以每次**新连接**都能免口令认证； `ControlMaster` 的贡献更彻底——**复用会话根本不做认证**，slave 只在本机 socket 上请求开一条 channel， 服务端在已认证的 master 连接上看到 `channel open`，不问身份。 `ssh-add -x` 锁住 agent 后复用会话仍能进去，是因为那条会话压根不需要 agent（认证早就完成了）； 这正是「复用会绕过 agent 状态」的体现。若想让锁真的生效，得先 `ssh -O exit` 关掉 master。 证据：场景 7 图 2 + 场景 4.4（锁上时 `-v` 出现 `agent contains no identities`）。</sub>

<sub>**答案 5**：① `ControlPath` 的**目录不存在**（`ControlPath cannot be opened → ssh 继续、不复用`， man 原话，静默降级）；② socket 名不含 `%h %p %r`/`%C`，或被残留文件/别的用户占位，或压根没有活的 master （认证失败就不会有 master，见场景 7.6）。一条命令看真相： `ssh -vv 主机 true 2>&1 | grep -E 'auto-mux|Control socket'` —— 看到 `Trying existing master at …` + `Control socket "…" does not exist` 就是路径问题。 证据：场景 7.4 的两行 debug。</sub>

<sub>**答案 6**：`%C` = SHA1(`%l%h%p%r%j`)，即「本机名 + 远端主机 + 端口 + 远端用户 + ProxyJump 内容」拼接后取 SHA-1。 本次（远端用户 `learner`）`SHA1("<主机名>" + "127.0.0.1" + "22" + "learner" + "")` = `d7ffceea9d6814181862a643c1719526adbe10c8`，与 capture 完全一致， 用 `printf '%s' "$(hostname -f)127.0.0.122learner" | sha1sum` 就能自己复算。 `%j` 就是 `ProxyJump` 的内容，换跳板 → 拼接串变 → 哈希变（实测：`cd63bcc…` → `5aa5cc7e…`）。 对 `ProxyJump` 复用意味着：**同一目标、不同跳板链会各自拥有独立 control socket**， 不会串味；但如果你用 `%r@%h:%p`（不含 `%j`）命名的 socket，就要自己意识到这一点。</sub>

<sub>**答案 7**：**不能拿到私钥**（agent 协议没有「导出私钥」这种请求，man 也明说 `An attacker cannot obtain key material from the agent`）。 他**能**做的是：连上远端那个转发 socket，让你的本地 agent 无限次签名， 从而**以你的身份**去认证任何你 agent 里有钥匙的机器（`they can perform operations on the keys`）。 他能**不能**做的是：读私钥字节、把身份持久化到自己机器以外（转发 socket 随连接消失）。 三条降风险做法：`ssh-add -c` / `ForwardAgent confirm`（每签一次确认）、 `ssh-add -t <寿命>`（自动过期）、单独一个 agent + `IdentityAgent` 限定候选集合； 再加一条：只在需要的 Host 上 `ForwardAgent yes`，其余 `Host *` 关；服务端可 `AllowAgentForwarding no`。</sub>

<sub>**答案 8**：`ssh-add -t 3600 ~/.ssh/id_deploy`（一次装、一小时自动失效，`Lifetime set to 01:00:00`）， 并配 `-o IdentitiesOnly=yes -i ~/.ssh/id_deploy` 保证只亮这一把。 比「跑完再 `ssh-add -D`」更稳的原因：`-D` 依赖脚本**正常走到那一行**——中途 `exit`、被 `Ctrl-C`、 超时、`set -e` 触发，都会让钥匙永久留在 agent 里；而 `-t` 的过期时刻存在 **agent 进程内部**， 由 agent 自己到点回收（场景 4.2 里 `-t 1` + `sleep 3` 直接证明了「过期真的会发生」， 不需要任何后续命令）。到期即消失，与脚本是否成功无关。</sub>

---

## 未验证清单（本机拿不到 / 需要你到自己环境里补的）

诚实是这一章的骨架。以下结论**没有**一条来自「我推测应该这样」，都明确标了状态：

| 条目 | 状态 | 为什么 | 你可以怎么验 |
| --- | --- | --- | --- |
| `Server accepts key: … agent`（服务端接受公钥那一行） | **本机未能复现** | 不能在真实用户目录装 `authorized_keys` | 在你的测试机上 `ssh-copy-id` 后 `ssh -vv … \| grep 'Server accepts key'` |
| `AddKeysToAgent yes` 的成功路径（认证成功后自动入 agent） | 未验证 | 同上；本机只证了「失败时不加」 | 认证成功后 `ssh-add -L` 看是否多了那把 |
| master socket 真实存在（`Master running (pid=…)`） | 未验证 | 没有登录成功就没有 master | 三件套 + `ssh -O check` |
| 第二次连接的**具体**提速数字 | 未验证 | 同上 | `time ssh box true` 两次对比 |
| `-O forward` / `-O cancel` / `-O stop` / `-O exit` 的成功分支 | 未验证 | 同上（`-O check` 的失败分支已全部实测） | 活的 master 上逐条试 |
| `ProxyJump` 两级复用的实际效果 | 未验证 | 需要真跳板机；本手册不连外部主机 | 给 `Host gate` 加三件套，第二次 `ssh -J gate target` 看 `-vv` |
| 远端转发 socket 的确切路径（`/tmp/ssh-*/agent.<pid>` 形态） | 未验证 | 需要登录成功 + `-A` | 远端 `echo "$SSH_AUTH_SOCK"` |
| 服务端 `AllowAgentForwarding` 在本机的实际值 | **未验证（按红线不读 `/etc/ssh/sshd_config`，不碰 sshd）** | 只被动观察了 22 端口在听 | `sudo sshd -T \| grep -i allowagentforwarding` |
| `ssh-agent` 默认路径 `~/.ssh/agent/s.*` 与残留清理（`-U`） | 未验证 | 不能在家目录做实验 | 在测试机上 `ssh-agent -s` 不带 `-a`，看 `SSH_AUTH_SOCK` |
| `sudo ssh-add -l` vs `sudo -E ssh-add -l` | 未验证 | 本机 sudo 要交互口令（capture 里给了真实报错），不猜密码 | 在你 sudo 得动的机器上对比 |

---

## 收尾检查与本章安全边界回顾

- **capture**：`bash demo.sh > ~/.cache/handbook-captures/ssh04.txt 2>&1`，退出码 **0**，433 行，场景 0~8 全跑完， 结尾**四条**自检原文都打印了（见 8.5：残留 agent 只剩桌面那几支 / `kill -0` 证明本脚本那支 PID 已退出 / `ss -lnt` 与开工前基线一致 / `ss -xl` 里没有沙箱 socket）。
- **遗留端口与 socket**：脚本自检 + 我在 demo 结束后手工 `ss -lnt`、`ss -xl` 复核， `2222 / 8098 / 8099 / 1080 / 9100` 五个都没有，`ss -xl` 里也搜不到 `ssh04`； `0.0.0.0:22`、`[::]:22` 是那台**真实 sshd**，本章只被动连接它，没有重启、没有停 `ssh.socket`、没有改 `/etc/ssh/` 任何一个字节。
- **认证**：全程 `BatchMode=yes` + `PreferredAuthentications=publickey`， 结局一律是 `Permission denied (publickey,password)` 或 `Too many authentication failures`； 没有尝试登录成功，没有装 `authorized_keys`，没有猜口令，没有枚举用户名（`learner` 是本机不存在的用户名， 它只用来保证「文档里不印真名」，不改变任何一条结论）。
- **家目录**：没有读写 `<家目录>/.ssh/` 的任何内容（连默认文件名的自动探测都被 `IdentityFile=<沙箱>/keys/_no-default-probe` 挡掉了）。
- **外部主机**：只有 `127.0.0.1`（补充实测里的 `box` / `probe.invalid` 全部走 `-G` / `-O check`， 一条网络都不建）。
- **私钥**：capture 与本文档里**没有任何私钥正文**（`ssh-add -L` 只出公钥且被 `cut -c1-60` 截断）。
- **自己起的 agent**：`ssh-agent -k` 已关掉（场景 8 实测进程与 socket 一起消失，8.5 自检 2 用 `kill -0` 再证一次）。
- **沙箱**：默认模式跑完自动 `rm -rf`（`[沙箱已删除] /tmp/ssh04-myhsZd`）。 想留下来接着敲：`bash demo.sh --keep`（它会打印 PID、socket 和沙箱路径，并提醒你手工清理）。 章目录最终只剩 `README.md` + `demo.sh`，0 产物。

---

## 引用约定（这一章的围栏是怎么从 demo 输出搬进文档的）

- **一次真跑**：`bash demo.sh > ~/.cache/handbook-captures/ssh04.txt 2>&1` → 433 行、退出码 0， 沙箱 `/tmp/ssh04-myhsZd`，远端用户 `learner`。本章每一段围栏都取自这一份捕获，逐行原样； 围栏里的 `$ ` 就是当时的命令行。
- **demo 的输出助手**（`demo.sh` 里的 `scene` / `say` / `line` / `run` / `note`）只负责打印： `run "..."` 与 `note "..."` 会先把引号里那串**原样打印成一行 `$ 命令`**，再交给 `bash -c` 执行。 里面的 `$SB`、`$SSH_BASE`、`$SSH_TARGET` 在打印之前就已经被 shell 展开成真值，所以你在本章看到的是 **展开后的等价命令**（`/tmp/ssh04-myhsZd/keys/id_alpha`，不是 `$SB/keys/id_alpha`）—— 可以直接复制去敲， 只是路径要换成你自己那一次的。`note` 后面那句 `（这条按设计会失败，就为了看真实报错原文）` 和 `[退出码 N —— 非 0 是预期的]` 都是助手加的注解，不是 ssh 打的。
- **`<沙箱>` 这个写法**：凡是本章正文或围栏里出现 `<沙箱>`，它指的都是 `mktemp -d` 出来的那个随机目录 （本次 demo 是 `/tmp/ssh04-myhsZd`，7.1/7.3/7.5 那几条 **demo 之外的补充实测**用的是另一次 `mktemp -d`， 同样一律写作 `<沙箱>`）。用法固定两种：`<沙箱>/keys/id_alpha` 这种「路径里的一段」， 和 1.3 那张 `ls -l` 注解图里把整条路径缩成 `<沙箱>/agent.sock`。**所有带 `$ ` 的命令行都保留真实那次的完整路径**， 没有拿占位符冒充命令。
- **脱敏替换（本章唯一对真输出动的两处手）**：① 远端用户名 —— demo 直接改成中性名 `learner` 后**重跑**， `%r` 的值、`-O check` / `auto-mux` 报错里展开出的 socket 名（`learner@127.0.0.1:22`）、每一条 `Permission denied` 前面的 `learner@127.0.0.1`、 `%C` 的 40 位哈希全部是新捕获里的原值，**一个都没有手改**（`%C` 还能按 7.3 给的 `sha1sum` 式子复算）。 ② 只能来自本机的身份 —— `ls -l` 的属主/属组两列、`ssh -G` 里 `%u`/`%l`/`%L`/`%d` 的展开值、 `/home/<你>/.ssh/` 这类家目录路径，统一写成 `<本机用户>` / `<主机名>` / `<家目录>`； 这几处没法用假名跑（OpenSSH 拿 `getpwuid` 比对本机用户，`-l` 只改远端用户），只能原样重跑再事后替换。
- **%C 那 5 个哈希分别来自哪里**：`d7ffceea9d6814181862a643c1719526adbe10c8`（7.3 三条 `-O check` 里 `%C` 那条， capture 第 360 行）**在捕获里**；另外 4 个不在 —— `f48782c7e9818723b3ed957e60959904c487b220`（7.1 的 `-G box` dump） 和 `%j` 表里那三个（`cd63bcceada4e89a8831a70b450daf6d5640e0dc` / `5aa5cc7e39b9a5ee722c986fc83fa87b5eade488` / `95e75b4c5e215fa91b63223981215a30469adde4`）来自标注为 **demo 之外的补充实测** 的那几次离线运行， 命令原样附在两段正文里（都带 `-l learner`、都只读配置或查本地 socket，一条网络都不建）。 本次脱敏改造之后这 4 个值**重新跑过一遍**才写进来的，所以它们与 `learner` 自洽： `printf '%s' "$(hostname -f)box22learner" | sha1sum` → `f48782c7…`， `printf '%s' "$(hostname -f)probe.invalid22learnergate.example" | sha1sum` → `5aa5cc7e…`。 同理，7.5 那两段「stale socket 被 unlink」的补充实测也是重跑的（普通文件、死 socket 各一次， 外加一次 `-O check` 对照），它们的原文不在 `ssh04.txt` 里，本章因此把它们明确标成补充实测而不是 capture 摘录。
- **换用户名之后有没有改变结论？** 没有一条。本章每一条 `ssh` 都按设计失败在认证阶段， 远端用户是谁只影响「报错里印谁的名字」：`learner@127.0.0.1: Permission denied (publickey,password).` 与用真名时的失败是同一道关卡、同一个退出码； 场景 6 的 `Too many authentication failures` 数的是**公钥 offer 次数**，与用户名无关； 场景 7 的 socket 名换成 `learner@127.0.0.1:22` 之后，「目录不存在→静默不复用」「残留→Connection refused」两条一字未改。 唯一跟着变的是**派生值本身**（socket 文件名、`%C` 哈希），而它们本来就是重跑取的新真值。
- **这么做的代价**（引文校验器会报的「未匹配」）： `python3 ~/.local/bin/verify_loose.py README.md ~/.cache/handbook-captures/ssh04.txt` → `含$行=222 未匹配=0`。 这一章能清零是因为脱敏只动了**校验器本来就折得动**的那一类：`ls -l` 的属主/属组是**独立成词**的 `<本机用户>`， 捕获里的真名会被折成同一个占位符；而 `%u` / `%l` / `%L` / `%d` 的展开值本章只写在**表格和注解图**里， 那些块没有 `$ ` 行，本来就不参与逐行比对。对比之下，上一版对齐的是另一次运行（`含$行=223 未匹配=6`）， 那 6 行全是每次必变的 PID / socket inode / key 指纹漂移，加上当时本机 sshd 被并发连接限流、场景 3 的 `-vv` 只吐出半截输出。 这一版把每一个数都换成新捕获的原值（`169701` / `169710` / inode `393783` / 三把 key 的指纹 / `%C` = `d7ffceea9d6814181862a643c1719526adbe10c8`）， 所以没有遗留行需要解释。

---

## 下一步

- **05-port-forwarding（第 05 章）**：本章反复出现「复用会话共享 master 的转发属性」， 它的另一半就是端口转发。最值得先做的两件事： ① 用本章的三件套连上一台机器，再用 `ssh -O forward -L 8080:127.0.0.1:3306 主机` **在不重新认证的前提下加一道门**（`-O cancel` 收回）； ② 把 `ExitOnForwardFailure yes` + `ServerAliveInterval 30` 写进 `~/.ssh/config`， 从此不再有「门没开但连接还活着」的假成功。
- **03-config-file（第 03 章）**：本章所有 `-o` 都能沉淀成配置。注意两条已被本章实测的规矩： `AgentForwarding` 是 sshd 侧关键字（客户端要写 `ForwardAgent`），以及坏选项会让整份配置出问题； `ControlMaster/ControlPath/ControlPersist` 应该按 `Host` 段写，并且**跳板机的 Host 段要单独写** （命令行 `-o` 到不了跳板机）。
- **02-keys（第 02 章）**：`ssh-add -l` 里那些指纹/注释都来自那里； 「口令只在 `ssh-add` 那一刻被用过一次」这件事，值得回到那章再确认一遍。
- **06-troubleshooting（第 06 章）**：本章出现的 `Too many authentication failures`、 `Control socket connect(…): Connection refused`、`no such identity: …`、 `Bad configuration option`、`Pseudo-terminal will not be allocated…` 都会在排障章按「症状 → 命令 → 结论」再排一遍。
- **现在就能做的三个动作**： ① `ssh -F /dev/null -G <你常用的一台> | grep -E '^(controlmaster|controlpath|controlpersist|forwardagent) '` —— 看看自己现在到底有没有开复用（**没有 `controlpath` 行 = 没设**）； ② `pgrep -a ssh-agent` + `echo "$SSH_AUTH_SOCK"` —— 数一数你机器上有几支 agent，你正在跟哪一支说话； ③ 把 `ForwardAgent no` 写进 `Host *`，只在真正需要的那一台上开 —— 这是本章风险最高、也最容易一条配置就修好的地方。
