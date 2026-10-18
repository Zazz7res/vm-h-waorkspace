# 这章：排错 —— 报错原文不是提示，是证词

> **一句话：SSH 的每一条报错，都是「它已经走到哪一步」的证词。**
> 你不需要背报错清单，你只需要知道一条 `ssh` 命令必然经过**三段路**：
> **① TCP 连上了没有 → ② 密钥交换 + 主机指纹过了没有 → ③ 认证过了没有。**
> 报错原文里的主语和位置，会告诉你是死在第几段；确定段之后，能排除的原因就一大片，能查的方向就只剩一小片。
> 本章按【报错原文】组织，一共 13 个场景，**每一条原文都是这台机器上真跑出来的**。

本章的写法和其它章不太一样：别处是「我要干什么」，这里是「我看见了这句话」。
因为排错的入口永远是**你屏幕上那行字**，不是手册的目录。

**版本与本轮现场**（所有输出都出自这一轮）：

```text
沙箱：<沙箱>
OpenSSH：OpenSSH_10.2p1 Ubuntu-2ubuntu3.6, OpenSSL 3.5.5 27 Jan 2026
目标：127.0.0.1:22（本机真实 sshd）+ 现搭的假服务器
本轮端口：没人听=47857 队列打满=45357 假服务器=34295/33509/47481/53987
（本脚本一开始就 unset 了 SSH_AUTH_SOCK，所以用的绝不是你桌面上的 agent）
```

> ⚠ **沙箱路径、端口、指纹每轮都会变**（脚本向内核要空闲端口、现造密钥）。
> 所以你重跑时数字对不上是**正常的**，别对数字，对**结构**：哪一行报错、报在管道的哪一段。
> 本章引用的每一行都可以在 `/tmp/ssh06.txt`（`bash demo.sh > /tmp/ssh06.txt 2>&1`）里 `grep -F` 命中；
> 小节里的代码块是**逐行节选**（每行原文未改，块内可能跳过中间几行）。

**能本机验证 / 不能本机验证（先摊开，免得你照着 README 找不到东西）**

| 本机真跑出来了（demo 有原文） | 需要你自己有服务器 / 需要 root，标「未验证」 |
|---|---|
| `refused` / `timed out` / `Could not resolve` / `bind` 四类 TCP 死法 | 真实公网防火墙 DROP 造成的 `timed out` |
| `-v / -vv / -vvv` 三层日志的差别与三段路读法 | 服务端 `sshd` 侧日志原文（`journalctl`） |
| `Permission denied (publickey)` + `Offering`/`Authentications that can continue` | `Server accepts key` 之后的 `agent refused operation` |
| `Too many authentication failures`（真被本机 sshd 掐线） | `Bad owner or permissions on ~/.ssh/authorized_keys` |
| `WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!` 全文 | `Timeout, server not responding.`（要登录成功后） |
| `StrictHostKeyChecking` 四值真实行为、`ssh-keygen -R/-F/-H` | `sshd -d` 前台调试输出的完整链路 |
| 私钥四种坏法：太开 / 读不了 / 格式错 / 口令错 | SELinux 上下文导致 key 失效 |
| `Bad SSH2 cipher spec` vs `Unable to negotiate ... Their offer:` | 老服务器（OpenSSH 5.3/7.0）真实指纹核对 |
| 假服务器造的 `kex_exchange_identification` / `Connection closed` | `Banner`、`PerSourcePenalties` 的服务端配置 |
| `ssh -G` 的 first-match-wins / `Include` 顺序 / `~` 展开规则 | 需要多网卡/VPN 的源地址绑定实况 |
| git 的 `GIT_SSH_COMMAND` argv 取证、`ssh 变体 'simple'` | GitHub/GitLab 真实仓库的认证差异 |

---

## 你会遇到的场景

| # | 你在屏幕上看到的（或经历的） | 它其实只说明了这一件事 | 去看 |
|---|---|---|---|
| 1 | `Connection refused` / `Connection timed out` / `Could not resolve hostname` | 还没到 SSH 协议，**跟密钥一点关系都没有** | 实战 1 |
| 2 | `Permission denied (publickey)` | 第 3 段死了，但**不知道为什么**死 —— 必须 `-vv` | 实战 2 |
| 3 | `Permission denied (password)`，或者它压根不问我要密码 | `BatchMode` / 服务端只开了 publickey | 实战 3 |
| 4 | `Too many authentication failures` | 不是密码错，是你**牌太多**，对的牌排在后面 | 实战 4 |
| 5 | `WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!` | 三种成因长得一样，**其中一种要你停手** | 实战 5 |
| 6 | `Load key ...: invalid format / error in libcrypto / too open` | 私钥文件本身坏了或权限不对，**还没出门就死了** | 实战 6 |
| 7 | `agent refused operation` / `The agent has no identities.` | 你连的根本不是那个 agent，或者它被锁了 | 实战 7 |
| 8 | `Unable to negotiate ... no matching key exchange method found.` | 两边算法表没有交集，`Their offer` 就是答案 | 实战 8 |
| 9 | `kex_exchange_identification: read: Connection reset by peer` | 对面在**握手阶段**就把你踢了，不是你认证失败 | 实战 9 |
| 10 | `Warning: Permanently added ...` | 这是**通知**，不是错误。别为了消它关掉校验 | 实战 10 |
| 11 | git 报 `Permission denied (publickey)` | git 用的可能不是你那个 ssh，先问它调了谁 | 实战 11 |
| 12 | 「我明明写了配置，怎么没生效」 | first-match-wins：`ssh -G` 是唯一权威 | 实战 12 |
| 13 | 以上全都不像，我就是连不上 | 万能顺序：`-G` → `ping` → `nc` → `ss` → `-vv` | 实战 13 |

---

## 心智模型

### 第一块：一条 `ssh` 命令只有三段路

```
 ┌── 第 1 段：TCP ──────────────────────────────────────────────┐
 │  DNS 解析 → connect() → 三次握手                             │
 │  日志锚点：Connecting to X [IP] port 22. / Connection established.
 │  死在这段的报错：Could not resolve / refused / timed out / bind
 │  ★ 这一段能排除掉：密钥、指纹、算法、服务端配置。全都与它们无关。
 └─────────────────────────────────────────────────────────────┘
 ┌── 第 2 段：KEX（密钥交换 + 主机指纹）────────────────────────┐
 │  换版本横幅 → 谈 kex / host key / cipher / MAC → 验 known_hosts
 │  日志锚点：kex: algorithm: ... / kex: host key algorithm: ...
 │  死在这段的报错：REMOTE HOST IDENTIFICATION HAS CHANGED /
 │                 Unable to negotiate ... no matching XXX /
 │                 kex_exchange_identification: ...（半路断）
 │  ★ 这一段才有「指纹」的事；你的用户密钥、agent 还没登场。
 └─────────────────────────────────────────────────────────────┘
 ┌── 第 3 段：认证 ────────────────────────────────────────────┐
 │  问「服务器接受哪些方式」→ 逐个递 key / 口令 → 通过或拒绝    │
 │  日志锚点：Authentications that can continue: ... / Offering public key: ...
 │  死在这段的报错：Permission denied (publickey) / (publickey,password) /
 │                 Too many authentication failures / agent refused operation
 │  ★ 只有这一段归「你的 key」管。
 └─────────────────────────────────────────────────────────────┘
```

**这条顺序是本章所有判断的根**：报错出现在第几段，就只可能是那一段的原因。
反过来用更省时间 —— 你先确定「死在第几段」，能一次性排除另外两段的**全部**可能性。
实战 2 会把这三段用 `-vv` 的真实行号打出来，实战 13 用 `grep -n` 把锚点行号钉死。

### 第二块：`-v / -vv / -vvv` 是放大镜倍数，不是「越详细越好」

本机实测三级日志的**行数**（同一目标、同一份配置，见「补跑取证」附录 A）：

```text
$ ssh -v -o IdentityAgent=none … learner@127.0.0.1 true > 沙箱里的日志（屏幕只数行数）
  ssh -v    总行数= 50  debug1= 47  debug2=  0  debug3=  0

$ ssh -vv -o IdentityAgent=none … learner@127.0.0.1 true > 沙箱里的日志（屏幕只数行数）
  ssh -vv   总行数= 95  debug1= 47  debug2= 46  debug3=  0

$ ssh -vvv -o IdentityAgent=none … learner@127.0.0.1 true > 沙箱里的日志（屏幕只数行数）
  ssh -vvv  总行数=129  debug1= 47  debug2= 46  debug3= 34
```

读法（这三行数字比任何解释都直白）：

- **`-v` = 只看 `debug1`**。三段路的锚点行**全是 `debug1`** —— 也就是说，
  **90% 的排错 `-v` 就够了**，而且它总共 50 行（其中 47 行是 `debug1`），一眼看完。
- **`-vv` 多出 46 行 `debug2`**：`local client KEXINIT proposal` 和**双方的完整算法清单**
  （`KEX algorithms:` / `host key algorithms:` / `ciphers ctos:` / `MACs ctos:`）。
  → 只有「算法谈不拢」（实战 8）才需要它，因为你要看见**整张表**才知道该挑哪个。
- **`-vvv` 再多 34 行 `debug3`**：`send packet: type 20` / `receive packet: type 20` /
  `set_sock_tos` / `timeout: 8000 ms remain after connect` / `record_hostkey: found key type ...`。
  → 这是**报文级 / 文件级 / 计时级**日志，用来回答「卡在等哪一个包」，日常排错用不上；
    而且它会打印 `Started with: ssh ...`（你的完整命令行）和远端横幅，
    **贴问题到网上时最容易不小心泄露环境信息**（见附录 A）。
- **`debug1` 前面的行号本身就是线索**：demo 实战 13 打出的是
  `4: Connecting to` → `7: Connection established` → `46: kex: algorithm` → `87: Authentications`。
  行号之间间隔突然变大 = 中间在等什么东西（超时、慢 DNS、重试）。

### 第三块：报错原文的**主语**决定你去哪台机器上查

| 报错长这样 | 主语是谁 | 该去查 |
|---|---|---|
| `ssh: connect to host 127.0.0.1 port 47857: Connection refused` | **本机内核**（客户端进程） | 你自己的网络视图：IP、端口、路由、VPN |
| `kex_exchange_identification: read: Connection reset by peer` | **对面**发的 RST | 对端机器：防火墙、sshd 状态、限速 |
| `Unable to negotiate with 127.0.0.1 port 22: no matching cipher found.` | **双方协商**的结果 | 先 `ssh -Q` 看自己，再照 `Their offer` 看对方 |
| `learner@127.0.0.1: Permission denied (publickey,password).` | **对端 sshd** | 括号里那张表 + 你的 key/agent |
| `Bad owner or permissions on /home/user/.ssh/authorized_keys` | **服务端**进程 | 服务器上的文件权限（但见附录 B：客户端也会报这句） |
| `command-line line 0: Bad SSH2 cipher spec 'cast128-cbc'.` | **本机 ssh 的解析器** | 你的算法名，**连门都没出**，跟服务器无关 |
| `Permissions 0644 for '...' are too open.` | **本机** ssh-add/ssh-keygen | 本地文件权限（`ssh` 客户端自己不说这句，实战 6 实测） |
| `致命错误：ssh 变体 'simple' 不支持设置端口` | **git**，不是 ssh | 见实战 11 |

> 一句话记法：**先看句号前面那个词是谁。** `ssh:` 开头是你自己，`用户@主机:` 开头是对面，
> `Unable to negotiate with 主机` 是协商结果，`command-line line 0:` 是本地解析，`致命错误：` 是 git。

### 第四块：分层定位 —— 每层只有一个工具配一个问题

```
 问题：这台机器在不在？          → ping -c 1 -W 1 主机
    ⚠ 很多服务器禁 ICMP：ping 不通 ≠ 服务不在；ping 通 ≠ SSH 能连。只能当第一步，不能当结论。
 问题：这个名字解出来是谁？      → getent hosts 主机 / ssh -G 主机 | grep '^hostname'
 问题：这个端口有没有人听？      → nc -vz 主机 端口（refused / succeeded / 一直不返回，三种答案）
 问题：听的真是 sshd 吗？        → nc 主机 22 看横幅，或 ssh -vv 里的 remote version string
 问题：是谁在听、绑在哪个地址？  → ss -lnt | grep ':端口 '（本机侧，还能看到 backlog 与并发）
 问题：ssh 协议答没答？          → ssh -v，看有没有 kex: algorithm 这一行
 问题：握手过了认证过没过？      → ssh -vv，看 Authentications that can continue / Offering
 问题：配置到底生效了没？        → ssh -G 主机（不发一个网络包，毫秒级，纯本地，最快）
 问题：转发出来的门通不通？      → curl -v / 用真实协议问它（05 章），别只看 ss
```

**关键分界：`nc -vz` 和 `ssh -v` 之间就是「网络世界」和「SSH 世界」的边界。**
`nc` 都能连上而 `ssh` 报错 → 从这一刻起，**所有**网络层的解释都该退休了。
`curl` 在本章不出场（本章的目标只有 22 端口上的 ssh 协议），它属于 05 章「验门后面接的是谁」。

### 第五块：`ssh -G` 是唯一权威 —— 它打印的是「最终生效值」，不是「你写了什么」

```
  配置来源（优先级从高到低，但真正的规矩是：同一次解析里【同一个关键字第一次出现赢】）
      命令行 -o XXX      ← 最靠前写的那一个赢（实测，见实战 12②）
      命令行 -F 文件
      ~/.ssh/config      ← 不存在就跳过，不报错
      /etc/ssh/ssh_config（本机第 19 行 Include /etc/ssh/ssh_config.d/*.conf）
  ssh -G <host> 把上面全部合成一张「你真正会用到的表」打出来，**不发一个网络包**。
```

所以它是本章最便宜的工具：**毫秒级、无副作用、能证明「覆盖顺序」**。
实战 12 用它证明了四件事：重复 `Host` 只有第一次生效、`-o` 也是第一次赢、
`*` 写在前面会吃掉后面的具体 `Host`、`Include` 的位置决定谁赢。

两个实测的小脾气（都会咬人，见实战 3、9）：

- `ssh -G` **不打印** `preferredauthentications` 和 `connectiontimeout`（本机 `grep -c` = 0），
  因为「没有默认值的键」它干脆不列。**`grep` 不到 = 没配置**，**不等于**「值不对」。
- 输出里会混进一句 `Pseudo-terminal will not be allocated because stdin is not a terminal.`
  —— `-G` 根本不建连接，这句是 10.2 的噪音（实战 10 专门讲它）。

### 第六块：为什么本章有一半标了「本机未验证」—— 两条红线

```
 红线 1：绝不尝试登录成功。
   做法：每条 ssh 都挂 -F /dev/null + BatchMode=yes + 一条【不存在】的 IdentityFile
         + 沙箱 UserKnownHostsFile + PreferredAuthentications=publickey。
   代价：所有「认证成功之后」的现象（agent refused operation、Server accepts key、
         Timeout, server not responding、authorized_keys 的服务端报错）本机拿不到。
 红线 2：绝不动系统 sshd（22 端口是真实服务，只能被动观察）。
   代价：服务端的限速 / 防火墙 / Banner / MaxStartups 只能观察，不能改配置来验证。
```

所以本章的诚实分工是：**能本机造的现场就贴原文**（假服务器、known_hosts、坏私钥文件、
`-G`、算法表、git 包装层……这些都在客户端侧，随便造）；
**只能服务端侧看的就给完整命令模板 + 标未验证**。
另外，本章所有场景一律只用 `127.0.0.1`/`localhost`，脚本从头到尾没有 `cat` 过任何私钥。

---

## 场景逐个实战

### 实战 1（demo 第 1 节）：「连不上」的四种说法，每一种各自排除了哪一类原因

```text
$ nc -vz 127.0.0.1 47857
  （这条按设计会失败，就为了看真实报错原文）
nc: connect to 127.0.0.1 port 47857 (tcp) failed: Connection refused
  [退出码 1 —— 非 0 是预期的]

$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -p 47857 learner@127.0.0.1 true
  （这条按设计会失败，就为了看真实报错原文）
ssh: connect to host 127.0.0.1 port 47857: Connection refused
  [退出码 255 —— 非 0 是预期的]
```

```text
$ timeout 15 ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -p 45357 learner@127.0.0.1 true
  （这条按设计会失败，就为了看真实报错原文）
ssh: connect to host 127.0.0.1 port 45357: Connection timed out
  [退出码 255 —— 非 0 是预期的]
```

```text
$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=5 learner@nosuchhost.invalid true
  （这条按设计会失败，就为了看真实报错原文）
ssh: Could not resolve hostname nosuchhost.invalid: Name or service not known
  [退出码 255 —— 非 0 是预期的]

$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -b 10.10.10.10 learner@127.0.0.1 true
  （这条按设计会失败，就为了看真实报错原文）
bind 10.10.10.10: Cannot assign requested address
ssh: connect to host 127.0.0.1 port 22: failure
```

```text
$ ss -lnt | grep ':22 '
LISTEN 0      4096         0.0.0.0:22         0.0.0.0:*
LISTEN 0      4096            [::]:22            [::]:*

$ nc -vz 127.0.0.1 22
Connection to 127.0.0.1 22 port [tcp/ssh] succeeded!
```

**这四种死法怎么分（本章第一个必须带走的东西）**

| 原文 | 内核层面发生了什么 | 排除了什么 | 该往哪查 |
|---|---|---|---|
| `Could not resolve hostname` | DNS 都没解出来，**connect() 没发生** | 一切服务端因素 | 主机名拼写、内网 DNS、`/etc/hosts`、VPN 没连 |
| `Connection refused` | 包到了对面，对面**回了 RST** | 防火墙 DROP、密钥、指纹、算法 | 端口写错、sshd 没起、不是 22、被 NAT/LB 接了 |
| `Connection timed out` | SYN 发出去**石沉大海**（既不 ACK 也不 RST） | 「端口后面没人听」（那会立刻 refused） | 防火墙 DROP、路由黑洞、对端并发超限后随机丢包 |
| `bind ...: Cannot assign requested address` | 你要求**源地址**，本机没有这个地址 | 对端的一切 | VPN 掉线、多网卡选错、容器里 `-b`/`BindAddress` 失效 |

**demo 为了造出 `timed out` 干的事很值得学**：它起了一个 `fullbacklog.py`，
`listen(0)` 之后自己把 accept 队列占满、**永不 accept**。于是新连接只能静静排队 →
`ConnectTimeout=8` 到点报 `timed out`。这就是一次本地证明：

> **`refused` = 到了但没人接；`timed out` = 没到，或者到了被故意不理。**
> 两者排查方向**完全相反**：refused 去找进程，timed out 去找防火墙 / 链路 / 负载。

`-b 10.10.10.10` 那条还有个读法上的坑：它打了两行，**第二行
`ssh: connect to host 127.0.0.1 port 22: failure` 是 ssh 的收尾废话**
（只表示「我连接失败了」，不带原因），真正的信息在**第一行的 strerror**。
网上大量帖子只抄最后一行，于是你搜 `failure` 什么也搜不到 —— 看到 `bind` 那一行再动手。

最后那两行 `ss` 是本章的**参照组**：`0.0.0.0:22` 和 `[::]:22` 说明本机 sshd 真的在听
（v4/v6 各一个监听），`nc` 的 `succeeded!` 是同一件事的第二次确认。
后面所有「连上了但被拒」的场景，都以这两行为前提。**先看 `ss`，再看 `ssh`**，
这是排错省时的第一条纪律。

### 实战 2（demo 第 2 节）：`Permission denied (publickey)` —— 用 `-vv` 把三段路走一遍

不加 `-v`，你能得到的全部信息就一行半：

```text
$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh/real learner@127.0.0.1 true
  （这条按设计会失败，就为了看真实报错原文）
Warning: Permanently added '127.0.0.1' (ED25519) to the list of known hosts.
no such identity: <沙箱>/keys/_no-default-probe: No such file or directory
learner@127.0.0.1: Permission denied (publickey,password).
  [退出码 255 —— 非 0 是预期的]
```

**同一条命令加 `-vv`**（demo 还把它 `tee` 进 `vvv.txt`，后面几节继续 grep 它）：

```text
debug1: Connecting to 127.0.0.1 [127.0.0.1] port 22.
debug1: Connection established.
debug1: kex: algorithm: mlkem768x25519-sha256
debug1: kex: host key algorithm: ssh-ed25519
debug1: Authentications that can continue: publickey,password
debug1: Offering public key: plain@sb ED25519 SHA256:Ae47YlA/WzUk+qiJUaAlMFA2ON6D0WF73OFFlBenV4g agent
debug1: Authentications that can continue: publickey,password
debug1: No more authentication methods to try.
learner@127.0.0.1: Permission denied (publickey,password).
```

**逐行对号入座**：

- `Connecting to 127.0.0.1 [127.0.0.1] port 22.` —— 第 1 段开始；方括号里是**解析后的 IP**。
  「名字对但 IP 不对」的坑（`/etc/hosts` 残留、内网 DNS、云上同名实例）就在这两行现形。
- `Connection established.` —— **TCP 通了**。这一行一旦出现，网络层的原因全部出局。
- `kex: algorithm: mlkem768x25519-sha256` —— **KEX 最终选中的算法**（不是候选表！）。
  10.2 的默认第一优先是后量子混合算法 `mlkem768x25519-sha256`。
- `kex: host key algorithm: ssh-ed25519` —— **服务器主机密钥的类型**。
  实战 5 的指纹校验就发生在这行紧后面。
- `Authentications that can continue: publickey,password` —— **第 3 段的分水岭**。
  注意它出现了**两次**：第一次是服务器告诉你「我还肯接受哪些方式」；
  第二次是它拒绝你的 key 之后**再说一遍**，内容一模一样。
  → 「`Offering` 之后紧跟同样的 `Authentications that can continue`」
    就是「这把 key 它不认」的标准节奏，**不是**网络问题、**不是**算法问题。
- `Offering public key: ... agent` —— 你把这张牌亮出去了。**行尾的标签是钥匙**：

```text
debug1: identity file <沙箱>/keys/_no-default-probe type -1
debug1: Will attempt key: plain@sb ED25519 SHA256:Ae47YlA/WzUk+qiJUaAlMFA2ON6D0WF73OFFlBenV4g agent
debug1: Will attempt key: <沙箱>/keys/_no-default-probe  explicit
debug1: Offering public key: plain@sb ED25519 SHA256:Ae47YlA/WzUk+qiJUaAlMFA2ON6D0WF73OFFlBenV4g agent
```

**`Will attempt key` 和 `Offering public key` 的差别，就是「打算用」和「真的用了」**：
前者是清单，后者才是发出去的包。而 `identity file ... type -1` 的意思是
「这个文件我看了，认不出格式（或者根本不存在）」。标签含义：

| 行尾标签 | 这张牌从哪来 |
|---|---|
| `agent` | 从 ssh-agent 拿的（内存里那份，**和文件权限无关**，见实战 6①） |
| `explicit` | 命令行 `-i` 或配置里 `IdentityFile` 明确指定的 |
| 只有 `Will attempt` 没有 `Offering` | 文件不存在 / 读不了 / 解析失败 → **牌根本没打出去** |
| 两行都没有 | 客户端**一张牌都没有** → `Permission denied` 完全合理 |

最后一种情形 demo 用 `-o IdentityAgent=none` 造（把 agent 这条路也掐死）。
那次真跑的输出被 sshd 的握手限速截断了（实战 3 会看到限流长什么样），
所以本手册补跑了一次，拿到干净尾部（**附录 C**）：`Will attempt key` 只剩那条不存在的
`explicit`，接着直接 `No more authentication methods to try.`，**一行 `Offering` 都没有**。

> **这是本章最重要的一个区分**：报错原文一模一样（`Permission denied (publickey,password)`），
> 但根因可能是「你没给 key」，也可能是「你给了、它不认」。**两种情况的修法完全相反**，
> 而唯一能区分它们的就是 `-vv` 里有没有 `Offering public key` 这一行。

**三个隐形杀手（都在服务端，本机只能给自查命令 —— 需服务器验证）**：

1. `StrictModes`：服务器嫌弃 `~/.ssh` 或 `authorized_keys` 权限太开。
   症状：同一把 key 在别的机器好使、在这台不行；客户端只看到 `Permission denied (publickey)`。
2. **SELinux/AppArmor**：权限数字全对，但安全上下文不对，sshd 读不到那个文件。
   症状：从别的目录 `cp` 过来的 `authorized_keys` 失效（或 `mv` 过来的失效），**改内容没用**。
   → 这条最阴，因为 `ls -l` 看起来完美无缺。
3. `AuthorizedKeysFile` 被改过：你写的文件根本不是 sshd 在读的那个文件。
   症状：怎么加 key 都不生效，而且服务器日志里连「尝试过这把 key」都没有。

自查模板（都在服务器上用 root 跑，**本手册一条都没执行**）：

```bash
sshd -T | grep -iE 'authorizedkeysfile|strictmodes'
namei -l ~youruser/.ssh/authorized_keys    # 逐级看权限，比 ls -l 好用
grep -i sshd /var/log/auth.log             # RHEL 系是 /var/log/secure
getenforce; ausearch -m avc -ts recent | grep -i ssh
/usr/sbin/sshd -D -d -p 2222               # 前台单连接调试；用完必须 kill
```

### 实战 3（demo 第 3 节）：`Permission denied (password)` 与「根本不给我输口令的机会」

```text
$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o PreferredAuthentications=password -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh/real learner@127.0.0.1 true
  （这条按设计会失败，就为了看真实报错原文）
kex_exchange_identification: read: Connection reset by peer
Connection reset by 127.0.0.1 port 22
  [退出码 255 —— 非 0 是预期的]
```

**先给这条打个假**：这一句**不是** `PreferredAuthentications=password` 造成的，
它是**本机 sshd 在前面一连串认证失败之后把你限流了**（demo 每条真连接的命令之间 `sleep 10`
就是为了绕开这个，偶尔还是会被打到；同一现象见实战 9 与附录 A）。
**这就是排错最经典的一类假象**：你改了参数 A，看到的却是 B 类报错。判别方法只有两条 ——

1. 换一条**完全无关**的命令（比如 `nc -vz 127.0.0.1 22`）也失败 = 对端在限流你；
2. **等 30~60 秒**再跑同一条，报错变了 = 之前那个不是你的参数造成的。

正常情况，这两句才是本节的主角：

```text
$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o PubkeyAuthentication=no -o PreferredAuthentications=password,keyboard-interactive -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh/real learner@127.0.0.1 true
  （这条按设计会失败，就为了看真实报错原文）
learner@127.0.0.1: Permission denied (publickey,password).
  [退出码 255 —— 非 0 是预期的]
```

> **括号里列的是【服务器此刻还肯接受的方式】，不是你能选的方式。**
> 这是 `Permission denied` 后面那个括号唯一的含义，也是它最值钱的地方。

由此分三种走法：

- 括号里**只有 `publickey`** → 服务器写了 `PasswordAuthentication no`，别再试密码了，
  去修 key/agent（实战 2、4、7）。这时原文长这样：`user@example.com: Permission denied (publickey).`
- 括号里**有 `password` 但你就是登不上** → 先怀疑大小写 / 输入法 / 复制带空格
  （尤其从聊天工具里粘的），再怀疑服务端 PAM 与访问限制：
  `AllowUsers`、`DenyUsers`、`Match Address`、`faillock`（多次失败后锁定）。
- 出现 `keyboard-interactive` → **它常常是双因子**：弹两次框很正常，**第二次才是口令**，
  别在第一次（往往是「请输入 PIN / 认证码」）乱填密码。

`BatchMode=yes` 的真实含义：**把需要人回答的环节全部掐掉**
（除 `gssapi-keyex` / `gssapi-with-mic` / `batch`），这就是「脚本里它为什么不问我要密码」的答案。
本机查默认值：

```text
$ ssh -F /dev/null -o BatchMode=yes -G learner@127.0.0.1 | grep -E '^(preferredauthentications|batchmode) '
Pseudo-terminal will not be allocated because stdin is not a terminal.
batchmode yes

$ ssh -F /dev/null -G learner@127.0.0.1 | grep -E '^(pubkeyauthentication|passwordauthentication|kbdinteractiveauthentication|numberofpasswordprompts|authenticationmethods) '
Pseudo-terminal will not be allocated because stdin is not a terminal.
kbdinteractiveauthentication yes
passwordauthentication yes
pubkeyauthentication true
numberofpasswordprompts 3
```

**这段输出里三个坑，全是本机实测**：

1. `preferredauthentications` **一行都没有** —— 不是 grep 写错了，是 `ssh -G` 对「没有默认值的键」
   干脆不打印（`authenticationmethods`、`connectiontimeout` 同理，实战 9 再验一次）。
   → `-G` 的 grep 结果为**空是合法状态**，别把它当成「配置被吃掉了」的证据。
2. `Pseudo-terminal will not be allocated ...` 混进了 `-G` 的输出。`-G` 根本不建连接，
   这句属于**噪音**（详见实战 10）。脚本里 `ssh -G | grep` **不要用 `head -1` 当第一条配置行**。
3. `pubkeyauthentication true` 但 `passwordauthentication yes` —— 同一个 `-G` 里两种写法。
   这是 OpenSSH 自己的历史遗留，**别当成两种不同语义**；
   脚本里做判断要同时容忍 `yes`/`true`。

### 实战 4（demo 第 4 节）：`Too many authentication failures` —— agent 里 key 太多

demo 往沙箱 agent 里塞了 8 把 junk key（一共 9 把），于是真被本机 sshd 掐了线：

```text
$ ssh-add -l | wc -l
9

$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh/real -v learner@127.0.0.1 true 2>&1 | grep -E 'Offering public key|Received disconnect|Disconnected' | tail -4
  （这条按设计会失败，就为了看真实报错原文）
debug1: Offering public key: junk-4@sandbox ED25519 SHA256:fgXmhQHfQNBSlcOKsj0Z4S+Cai9+xFA7+5D3lY4U5cQ agent
debug1: Offering public key: junk-5@sandbox ED25519 SHA256:ysZ5JBPLIJWbq0dwiYtyBDgtSQUmFjqFhfolRe4RoSE agent
Received disconnect from 127.0.0.1 port 22:2: Too many authentication failures
Disconnected from 127.0.0.1 port 22
```

**读这两行的差别**：实战 2 是 `Authentications that can continue` 一次次回来
（服务器**逐个拒绝**你的牌）；这里是 `Received disconnect ... :2: Too many authentication failures`
（服务器**直接挂电话**）。`:2` 是 SSH 的 disconnect reason code，
看到它就知道「不是这一把 key 的问题，是**次数**爆了」。

根因一句话：**客户端把 agent 里的 key 挨个亮了一遍，服务器数到第 6 次就把线掐了。**
`MaxAuthTries` 默认 6，本机配置实测：

```text
$ grep -E '^#?(MaxAuthTries|MaxStartups|PubkeyAuthentication|PasswordAuthentication)' /etc/ssh/sshd_config || true
#MaxAuthTries 6
#PubkeyAuthentication yes
#PasswordAuthentication yes
#MaxStartups 10:30:100
```

（只读展示：这几行**都被注释着**，所以用的就是默认值 `6` / `10:30:100`。）

> 本地现象经常是「**我明明有对的 key**」—— 因为对的那把按 `ssh-add` 的顺序排在第 7 个，
> 永远轮不到它。「明明有」+「永远轮不到」这个组合，是 agent 类排错里最难自己想到的。

**修法：只递一把**（`IdentitiesOnly=yes` + 显式 `-i`）：

```text
$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh/real -o IdentitiesOnly=yes -i <沙箱>/keys/j3 -v learner@127.0.0.1 true 2>&1 | grep -E 'Offering public key|Received disconnect|Permission denied'
  （这条按设计会失败，就为了看真实报错原文）
debug1: Offering public key: <沙箱>/keys/j3 ED25519 SHA256:m4BuFljDfcMPsPvxq/8ajx+glkBE9V7gBIYc49c960s explicit agent
learner@127.0.0.1: Permission denied (publickey,password).
```

注意 `Offering` 只剩**一行**，而且没有 `Received disconnect` 了 —— 计数没爆，
于是拿到的是正常的「这把 key 服务器不认」。**这就是判断「修好了没有」的标志**：
从「掐线」变成「拒一张牌」，说明次数问题已经解除，剩下的才是 key 本身的问题。
（顺便看清 `explicit agent` 这个双标签：`-i` 指定的那把**同时也在 agent 里**，两个来源合并成一张牌。）

另一条路是把不要的 key 摘掉：`ssh-add -d <pub>` 删一把、`ssh-add -D` 全清（04 章）。
demo 收尾就是这么做的：

```text
$ ssh-add -D >/dev/null; ssh-add <沙箱>/keys/id_plain >/dev/null; ssh-add -l
All identities removed.
Identity added: <沙箱>/keys/id_plain (plain@sb)
256 SHA256:Ae47YlA/WzUk+qiJUaAlMFA2ON6D0WF73OFFlBenV4g plain@sb (ED25519)
```

### 实战 5（demo 第 5 节）：`WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!`

现场是：known_hosts 里先写一把**假**的 `127.0.0.1` 指纹，再连真 sshd。真实原文（整块，一字未改）：

```text
$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o PreferredAuthentications=publickey -o StrictHostKeyChecking=yes -o UserKnownHostsFile=<沙箱>/kh/changed learner@127.0.0.1 true
  （这条按设计会失败，就为了看真实报错原文）
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@    WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!     @
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
IT IS POSSIBLE THAT SOMEONE IS DOING SOMETHING NASTY!
Someone could be eavesdropping on you right now (man-in-the-middle attack)!
It is also possible that a host key has just been changed.
The fingerprint for the ED25519 key sent by the remote host is
SHA256:ZiBpW4VGzrtnDYcX80tWmYPtY6K9jw4OzMVPXmDRk4w.
Please contact your system administrator.
Add correct host key in <沙箱>/kh/changed to get rid of this message.
Offending ED25519 key in <沙箱>/kh/changed:1
  remove with:
  ssh-keygen -f '<沙箱>/kh/changed' -R '127.0.0.1'
Host key for 127.0.0.1 has changed and you have requested strict checking.
Host key verification failed.
  [退出码 255 —— 非 0 是预期的]
```

**这段吓人的报错里其实写了三个答案**，多数人只读第一行就 `-R` 了：

| 原文行 | 它给你的信息 |
|---|---|
| `The fingerprint ... is SHA256:ZiBp...` | **对方现在这把的指纹** —— 这是你要去核对的东西 |
| `Offending ED25519 key in ...:1` | **旧记录在哪个文件的第几行** —— 精确到行 |
| `remove with: ssh-keygen -f '...' -R '127.0.0.1'` | **它把命令直接写给你了**（连路径都带引号，可复制） |

**三种成因怎么分（本章的判断法）**：

- **A) 服务器真的换了**（重装 / 换机 / 换了 host key）——
  新指纹**能从可信渠道核对**（管理员、控制台里 `ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub`、工单）
  → 那就 `-R` 删旧记录后重连。
- **B) 只是访问方式变了**（IP vs 域名、换了端口、换了跳板路径）——
  known_hosts 里本来就是**按主机名分别存**的，看着像「变了」，其实**那条压根没写过**。
  用 `ssh-keygen -F` 分别查两种写法就能区分。
- **C) 中间人**：既不是 A 也不是 B，又**无法从可信渠道核对新指纹** ——
  **停手**。别 `-R`，更别用 `-o StrictHostKeyChecking=no` 把它盖过去。

区分 A/B 的工具（哈希过的也能查，这是 `-F` 独有的能力）：

```text
$ printf '  真指纹那份（场景 2 里 accept-new 自动写进去的）：\n'; ssh-keygen -F 127.0.0.1 -f <沙箱>/kh/real | head -2
  真指纹那份（场景 2 里 accept-new 自动写进去的）：
# Host 127.0.0.1 found: line 1
127.0.0.1 ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIJ8COfYZlWVtvsITvesDuL0e7PzZiQCCKeFi9lLjMMhY

$ printf '  假指纹那份（现在就长这样）：\n'; cat <沙箱>/kh/changed
  假指纹那份（现在就长这样）：
127.0.0.1 ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAILZEqjOitQSU5bsq4EV/HIhY8X+aWDiNnsx1uFv21+u2
```

（对比这两行就能看出「变」在哪：同一台机器、同一种 key 类型，**base64 完全不同**。
`ssh-keygen -F` 那句 `# Host ... found: line 1` 是它找到的证据行号。）

修法一：删掉旧记录（**自动留 `.old` 备份**，方便回滚，很多人不知道）：

```text
$ ssh-keygen -R 127.0.0.1 -f <沙箱>/kh/todelete
# Host 127.0.0.1 found: line 1
<沙箱>/kh/todelete updated.
Original contents retained as <沙箱>/kh/todelete.old

$ ls <沙箱>/kh
changed
real
todelete
todelete.old

$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh/todelete learner@127.0.0.1 true 2>&1 | head -2
  （这条按设计会失败，就为了看真实报错原文）
Warning: Permanently added '127.0.0.1' (ED25519) to the list of known hosts.
no such identity: <沙箱>/keys/_no-default-probe: No such file or directory
```

修法二：`StrictHostKeyChecking` 四个值各自的真实行为。demo 逐条真跑，每条换一份**空的** known_hosts：

```text
$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o PreferredAuthentications=publickey -o StrictHostKeyChecking=no -o UserKnownHostsFile=<沙箱>/kh/v_no learner@127.0.0.1 true 2>&1 | tail -1
learner@127.0.0.1: Permission denied (publickey,password).

$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o PreferredAuthentications=publickey -o StrictHostKeyChecking=yes -o UserKnownHostsFile=<沙箱>/kh/v_yes learner@127.0.0.1 true 2>&1 | head -2
No ED25519 host key is known for 127.0.0.1 and you have requested strict checking.
Host key verification failed.

$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o PreferredAuthentications=publickey -o StrictHostKeyChecking=ask -o UserKnownHostsFile=<沙箱>/kh/v_ask learner@127.0.0.1 true 2>&1 | head -2
Host key verification failed.
```

> ⚠ **这一节差点被限速骗过去**：同一份配置，在本机 22 端口刚被前面几十条失败连接打累的时候，
> `yes` / `ask` 两条真跑拿到的是 `Connection closed by 127.0.0.1 port 22`（那是 sshd 的**握手限速**，
> 见实战 9），**不是** strict checking 的判决。等 sshd 冷却后重跑，才得到上面这两句干净的原文。
> 这件事本身就是本章最大的坑之一：**判定之前先等 30 秒。**

四个值的正确结论（`no` 的 demo 原文 + 附录 C 补跑的 `yes`/`ask` + 实战 10 的 `accept-new`）：

| 值 | 首次见到（known_hosts 里没有） | **见过之后变了** | 适用 |
|---|---|---|---|
| `no` | 直接连，**一句提示都没有**就走到认证（实测） | **照样放行** ← 这才是真正危险的地方 | 谁都不该无脑用 |
| `ask`（默认） | 弹终端提示；**没有终端时** `Host key verification failed.` | **拦下**，给出上面那一整块 WARNING | 交互式使用 |
| `yes` | **没有终端时**：`No ED25519 host key is known for X and you have requested strict checking.` | **拦下** | 安全要求高的脚本 |
| `accept-new` | 自动收下并打印 `Warning: Permanently added ...` | **拦下**（和 `yes`/`ask` 一样） | **CI 的推荐值** |

**为什么不该无脑 `=no`（这是红线，值得单独一段）**：
`no` 的含义不是「跳过首次确认」，而是「**指纹这个机制整个关掉**」——
它把「见过之后又变了」这一类也放行。而「变了」正是中间人攻击**唯一会暴露**的时刻。
换句话说：`=no` 之后，known_hosts 这个文件对你**不再有任何保护作用**，留着它只是心理安慰。
`accept-new` 是 `no` 和 `yes` 之间那个正确的折中：**只在从来没见过时放行，见过之后再变照样拦**。
它满足了「首次要自动化」这个真实需求，同时**保留了防中间人的那一次校验**。
所以：CI / 自动化用 `accept-new`，永远不要用 `no`；
`no` 只该出现在「我明确知道我在做什么、而且这台机器不可能被中间人」的临时命令上
（比如连自己刚建的虚拟机 —— 而且用完就把这条 Host 的指纹收进 known_hosts）。

`HashKnownHosts`：隐私更好，但**你的 grep 会当场失效**（真实踩坑，demo 现场）：

```text
$ ssh-keygen -H -f <沙箱>/kh/hash/known_hosts 2>&1 | tail -3
Delete this file to ensure privacy of hostnames
<沙箱>/kh/hash/known_hosts updated.
Original contents retained as <沙箱>/kh/hash/known_hosts.old

$ grep -c '127.0.0.1' <沙箱>/kh/hash/known_hosts
  （这条按设计会失败，就为了看真实报错原文）
0
  [退出码 1 —— 非 0 是预期的]

$ printf '  但 ssh-keygen -F 照样找得到：\n'; ssh-keygen -F 127.0.0.1 -f <沙箱>/kh/hash/known_hosts | head -2
  但 ssh-keygen -F 照样找得到：
# Host 127.0.0.1 found: line 1
|1|QKDa8ZtXAvYuuswYbsvY/ETtXqM=|BNeQ1LlBWdk7l0Y/OncFA5ctOrg= ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAILZEqjOitQSU5bsq4EV/HIhY8X+aWDiNnsx1uFv21+u2
```

> CI 脚本里想用 `grep` 判断「这台主机是否已知」，在哈希过的 `known_hosts` 上**永远返回 0**，
> 于是每次都白装一遍 key。能用的工具只有 `ssh-keygen -F`
> （它拿你的候选主机名去算 HMAC 再比对，所以哈希了也认得出）。
> 顺带：`-H` 那句 `Delete this file to ensure privacy of hostnames` 提醒的是 **`.old` 里还是明文** ——
> 哈希只保护主文件，不保护备份。

### 实战 6（demo 第 6 节）：私钥文件本身的错（权限 / 格式 / 口令）

**① 权限太开：`ssh-add` 和 `ssh-keygen` 会吼，`ssh` 客户端【一声不吭】**

```text
$ ssh-add <沙箱>/keys/id_plain
  （这条按设计会失败，就为了看真实报错原文）
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@         WARNING: UNPROTECTED PRIVATE KEY FILE!          @
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
Permissions 0644 for '<沙箱>/keys/id_plain' are too open.
It is required that your private key files are NOT accessible by others.
This private key will be ignored.
  [退出码 1 —— 非 0 是预期的]

$ ssh-keygen -y -f <沙箱>/keys/id_plain
  （这条按设计会失败，就为了看真实报错原文）
Permissions 0644 for '<沙箱>/keys/id_plain' are too open.
It is required that your private key files are NOT accessible by others.
This private key will be ignored.
Load key "<沙箱>/keys/id_plain": bad permissions
  [退出码 255 —— 非 0 是预期的]
```

然后 demo 把**同一把 0644 的 key**（`IdentityAgent=none` + `IdentitiesOnly=yes` + 显式 `-i`）交给 `ssh`：

```text
  （这条按设计会失败，就为了看真实报错原文）
debug1: Offering public key: <沙箱>/keys/id_plain ED25519 SHA256:Ae47YlA/WzUk+qiJUaAlMFA2ON6D0WF73OFFlBenV4g explicit
learner@127.0.0.1: Permission denied (publickey,password).
```

> **这里要纠正 demo.sh 注释里的一句话。** 脚本注释写的是「ssh 只是【不再用这把 key】」，
> 但**真实输出是它照样把这把 key 递了出去**（`Offering ... explicit`，一句抱怨都没有）。
> 所以本机实测的正确结论更强：`ssh(1)` 对 `-i` 指定的、权限太开的私钥**既不警告也不拒绝**。
> 检查私钥权限的是 `ssh-add` / `ssh-keygen`（以及服务端的 sshd），**不是你的 ssh 客户端**。
> 结论不变的那一半是：**「我的 key 明明就在这里，为什么没生效」的第一反应仍然是 `ls -l` 看权限**，
> 因为坏法不止一种（②③④），而 `ssh` 侧对这些坏法**基本都是静默的**。
> 另外：如果这把 key 恰好也在 agent 里，它一定被递出去（用的是 agent 内存里那份，跟文件权限无关）——
> 这就是同一份 key「有时好有时坏」的来源（04 章也讲过这条）。

**②③ 五种坏文件，五种原文**（demo 各真跑一次 `ssh-keygen -y -f`）：

```text
Load key "<沙箱>/keys/id_unreadable": Permission denied
Load key "<沙箱>/keys/id_pub_as_priv": error in libcrypto
Load key "<沙箱>/keys/id_badpem": invalid format
Load key "<沙箱>/keys/id_trunc": error in libcrypto
Load key "<沙箱>/keys/id_empty": error in libcrypto
```

对应的文件分别是：`chmod 000` 的、拿**公钥**当私钥的、PEM 头在但身体是假 base64 的、
`head -c 200` 截断后又补了 END 行的、空文件。

- **`invalid format`** = PEM 头**在**，但身体按那个类型解析不出来。
  常见：格式转换工具把文件转坏了、把证书当私钥、复制时掉了字符。
- **`error in libcrypto`** = 按 **OpenSSH 自有格式**解析失败：截断、空文件、口令块坏。
  本机实测的意外：**把公钥当私钥喂进去也是 `error in libcrypto`**（不是 `invalid format`），
  所以「invalid format = 格式不对」这个直觉是错的，**要按解析器分，不按「坏法」分**。
- **`Permission denied`**（句首大写那句）= 文件**根本读不了**，连解析都没开始。
  → 只有这一种是权限问题，而且和 `too open` / `bad permissions` 是三件不同的事。

两条万能自救：

```bash
ssh-keygen -y -f 你的私钥            # 能打出公钥就说明文件本身是好的，问题在别处
ssh-keygen -p -m PEM -f 你的私钥     # 给只认老格式的程序（部分 PHP/JS 库、老 Paramiko）用
```

**④ 被问口令却没有终端**（这把 key 带口令，本机没有 ssh-askpass）：

```text
$ ssh-keygen -y -f <沙箱>/keys/id_locked
  （这条按设计会失败，就为了看真实报错原文）
ssh_askpass: exec(/usr/bin/ssh-askpass): No such file or directory
Load key "<沙箱>/keys/id_locked": incorrect passphrase supplied to decrypt private key

$ env SSH_ASKPASS_REQUIRE=never ssh-keygen -y -f <沙箱>/keys/id_locked
  （这条按设计会失败，就为了看真实报错原文）
Enter passphrase for "<沙箱>/keys/id_locked": Load key "<沙箱>/keys/id_locked": incorrect passphrase supplied to decrypt private key
```

对比这两条就是「**为什么它不问我要口令**」的完整答案：有 TTY 时它是 `Enter passphrase ...:`，
没 TTY（cron / CI / 管道里）就变成 `ssh_askpass: exec(...): No such file or directory` + `incorrect passphrase`。
**看到 `ssh_askpass` 那一行，不要以为是「口令错了」**，它说的是「我找不到问口令的窗口」。

改口令只动私钥，公钥和服务器上的 `authorized_keys` 一个字都不用改：`ssh-keygen -p -f 私钥`。

**⑤ 别抄到过期的报错字符串** —— 拿本机二进制逐个查（`strings /usr/bin/ssh`）：

```text
  Bad owner or permissions on            二进制里命中 1 次
  invalid format                         二进制里命中 1 次
  error in libcrypto                     二进制里命中 1 次
  agent refused operation                二进制里命中 1 次
  unsupported cache option               二进制里命中 0 次
  sign_and_send_pubkey                   二进制里命中 1 次
  kex_exchange_identification            二进制里命中 1 次
  ssh_exchange_identification            二进制里命中 0 次
  Connection closed by remote host       二进制里命中 1 次
```

- `unsupported cache option` **命中 0**：那是 macOS keychain / 老版本相关，这台 Ubuntu 的 10.2 不会给你这句。
- `ssh_exchange_identification` **命中 0**：这个旧前缀在 10.2 客户端里**根本没有**，
  现在给你的是 `kex_exchange_identification`（含义相同，见实战 9）。
  → 在网上抄到带 `ssh_exchange_identification` 的帖子，第一件事是对人家那台客户端的版本（`ssh -V`）。

**⑥ `Bad owner or permissions` 会不会是客户端报的？—— demo 实测：`-F` 文件它不检查**

```text
$ printf 'Host *\n  ServerAliveInterval 42\n' > <沙箱>/cfg/loose; chmod 666 <沙箱>/cfg/loose; ssh -F <沙箱>/cfg/loose -G learner@127.0.0.1 2>/dev/null | grep -c serveraliveinterval
  （这条按设计会失败，就为了看真实报错原文）
1
```

世界可写的 `-F` 配置**照样生效**（返回 1），世界可写的 `known_hosts` 也照样能连 —— 客户端不管。
但**这句报错在客户端侧是可以复现的**，条件非常特别，而且它正好解释了「为什么本章每条命令都带 `-F /dev/null`」：
**被 `Include` 进来的文件要查权限，`-F` 顶层文件不查**（本手册补跑取证，见**附录 B**）。

### 实战 7（demo 第 7 节）：agent 类报错 —— 「我加了 key 为什么还要口令」

**排查三件套，顺序不能换**（门牌号 → 里面有没有牌 → 是不是被重做过）：

```text
$ echo "第一步 门牌号：SSH_AUTH_SOCK=${SSH_AUTH_SOCK:-（空）}"
第一步 门牌号：SSH_AUTH_SOCK=<沙箱>/agent.sock

$ ssh-add -l | head -2; printf '  （沙箱 agent 里现在有 %s 把）\n' "$(ssh-add -l | wc -l)"
256 SHA256:Ae47YlA/WzUk+qiJUaAlMFA2ON6D0WF73OFFlBenV4g plain@sb (ED25519)
  （沙箱 agent 里现在有 1 把）
```

**第二步的四种原文，对应四种完全不同的走法**（这一段是本章最省时间的一张表）：

| `ssh-add -l` 的原文 | 真实含义 | 走法 |
|---|---|---|
| `Could not open a connection to your authentication agent.` | **门牌号是空的**：agent 没起，或环境变量没传过来 | 起 agent / `eval $(ssh-agent -s)`（04 章） |
| `Error connecting to agent: No such file or directory` | 门牌号有，但 **socket 文件没了** | 重启过 / 换机器 / 进容器 —— 重新拿门牌号 |
| `Error connecting to agent: Connection refused` | socket 文件在，但**进程早死了**（残留） | `ssh-add -D` 没用，得重起 agent |
| `The agent has no identities.` | agent 好好的，只是**里面是空的** | `ssh-add 你的key`，别再查别的 |

前三种 demo 都在本机真跑了（第三种最反直觉 —— **文件在不等于进程在**）：

```text
$ env -u SSH_AUTH_SOCK ssh-add -l
  （这条按设计会失败，就为了看真实报错原文）
Could not open a connection to your authentication agent.
  [退出码 2 —— 非 0 是预期的]

$ env SSH_AUTH_SOCK=<沙箱>/nope.sock ssh-add -l
  （这条按设计会失败，就为了看真实报错原文）
Error connecting to agent: No such file or directory
  [退出码 2 —— 非 0 是预期的]

$ env SSH_AUTH_SOCK=<沙箱>/notasocket ssh-add -l
  （这条按设计会失败，就为了看真实报错原文）
Error connecting to agent: Connection refused
  [退出码 2 —— 非 0 是预期的]
```

（第三条那个 `notasocket` 就是一个随便写了 `x` 的**普通文件** ——
内核对着普通文件 connect 就是 `Connection refused`。
所以「socket 文件存在」和「有进程在听」是两回事，这句报错分得开它们。）

**第三步：凡是穿过 `sudo` / `su` / `docker exec` / `systemd` / `cron` 的，环境都会被重做一遍**

```text
$ env -i PATH=$PATH HOME=$HOME bash -c 'echo "  干净子进程里的 SSH_AUTH_SOCK=[${SSH_AUTH_SOCK:-空}]"; ssh-add -l'
  （这条按设计会失败，就为了看真实报错原文）
  干净子进程里的 SSH_AUTH_SOCK=[空]
Could not open a connection to your authentication agent.
  [退出码 2 —— 非 0 是预期的]
```

本机 `sudo` 需要交互认证跑不动，所以 demo 用 `env -i` 演**同一件事**（环境清空 → 门牌号没了）。
能 sudo 的机器上自己对比这三条，这是「sudo 把 agent 洗掉」的铁证：
`ssh-add -l` / `sudo ssh-add -l` / `sudo -E ssh-add -l`。
（只有 `-E` 会保留 `SSH_AUTH_SOCK`；而保留了也不一定连得上 —— socket 属主是你，
root 反而可能被拒。**这就是 `ForwardAgent yes` 比 `sudo -E` 靠谱的原因**，见 04 章。）

**agent 上锁之后长什么样**（`ssh-add -x` 锁，`-X` 解；解锁口令走 `SSH_ASKPASS`）：

```text
$ SSH_ASKPASS=<沙箱>/askpass.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0 ssh-add -x
Agent locked.

  （这条按设计会失败，就为了看真实报错原文）
debug1: get_agent_identities: bound agent to hostkey
debug1: get_agent_identities: ssh_fetch_identitylist: agent contains no identities
debug1: No more authentication methods to try.

$ SSH_ASKPASS=<沙箱>/askpass.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0 ssh-add -X >/dev/null; printf '  解锁了：\n'; ssh-add -l
Agent unlocked.
  解锁了：
256 SHA256:Ae47YlA/WzUk+qiJUaAlMFA2ON6D0WF73OFFlBenV4g plain@sb (ED25519)
```

**`agent contains no identities` 这一行是「锁了」的指纹**：
客户端**能连上** agent（没有 `Could not open`），但清单是空的 ——
于是你 `ssh-add -l` 明明看到 key，`ssh` 却说一张牌都没有。
这就是「我加了 key 为什么还要口令 / 为什么说不认我的 key」的一种真解，
而且它**只在 `-vv` 里现形**（实战 2 那张「标签表」的最后一行）。

**④ `sign_and_send_pubkey: signing failed for ...: agent refused operation` —— ⚠ 本机拿不到**

这句必须**服务器接受了你的公钥**才会出现：签名发生在 `Offer` **之后**
（服务器先说「这把 key 我有记录」，客户端才去让 agent 签）。
本手册不允许装 `authorized_keys`、不可能登录成功，所以**拿不到，需服务器验证**。
在有服务器的机器上取证：

```bash
ssh -vv -o IdentitiesOnly=yes -i ~/.ssh/id_ed25519 用户@服务器 true 2>&1 \
  | grep -E 'Server accepts key|sign_and_send_pubkey|agent refused'
```

常见成因（按概率排）：
① agent 是**别的用户 / 别的容器**的（你连的根本不是同一个 socket）；
② agent 被 `ssh-add -x` 锁了（就是上面那一节）；
③ confirm 模式（`ssh-add -c`）下你在另一个终端按了拒绝；
④ `gpg-agent` 接管了 `SSH_AUTH_SOCK` 但那把 key 不在智能卡里；
⑤ `ssh-add -t` 设的寿命到点了。

两步自救：`ssh-add -l` 看到 key 吗 → 看到就 `ssh-add -D` 再干净地重新 `ssh-add`（不带 `-c`/`-t`）。

### 实战 8（demo 第 8 节）：算法谈不拢 —— `no matching key exchange method / host key type / cipher`

**先看本机到底支持什么（`ssh -Q` 是唯一权威，别抄网上的清单）**：

```text
$ for q in kex key sig cipher mac; do printf '  %-6s 本机支持 %3s 种，前 3 个：%s\n' "$q" "$(ssh -Q $q | wc -l)" "$(ssh -Q $q | head -3 | tr '\n' ' ')"; done
  kex    本机支持  15 种，前 3 个：diffie-hellman-group1-sha1 diffie-hellman-group14-sha1 diffie-hellman-group14-sha256
  key    本机支持  14 种，前 3 个：ssh-ed25519 ssh-ed25519-cert-v01@openssh.com sk-ssh-ed25519@openssh.com
  sig    本机支持  10 种，前 3 个：ssh-ed25519 sk-ssh-ed25519@openssh.com ecdsa-sha2-nistp256
  cipher 本机支持  10 种，前 3 个：3des-cbc aes128-cbc aes192-cbc
  mac    本机支持  16 种，前 3 个：hmac-sha1 hmac-sha1-96 hmac-sha2-256

    cast128-cbc  : cipher=0 mac=0 key=0
    3des-cbc     : cipher=1 mac=0 key=0
    hmac-md5     : cipher=0 mac=1 key=0
    ssh-dss      : cipher=0 mac=0 key=0
    ssh-rsa      : cipher=0 mac=0 key=1
```

`-Q` 的五个类别要分清：`kex`（密钥交换）、`key`（**主机**密钥类型）、`sig`（用户签名算法）、
`cipher`（对称加密）、`mac`（完整性）。第二段是「顺手验梗」：
`cast128-cbc`、`ssh-dss` 在 10.2 里**已经没有了**（全 0），
`3des-cbc` 还在 cipher 表里、`hmac-md5` 还在 mac 表里、`ssh-rsa` 还在 key 表里 ——
**「有没有」和「默认肯不肯用」是两个问题**，下面 ②③ 就是这个差别。

**① 客户端压根不支持的算法，连门都出不了**：

```text
$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o Ciphers=cast128-cbc -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh/real learner@127.0.0.1 true 2>&1 | head -1
  （这条按设计会失败，就为了看真实报错原文）
command-line line 0: Bad SSH2 cipher spec 'cast128-cbc'.
```

`Bad SSH2 cipher spec` 是**本地解析失败** —— 前缀 `command-line line 0:` 就在告诉你
「死在你的命令行参数上」，跟服务器一点关系都没有。看到这句就回去 `ssh -Q`，**别去改服务器**。

**② 客户端支持、但本机这台新 sshd 不肯用 —— 真正的协商失败（三句真实原文）**：

```text
Unable to negotiate with 127.0.0.1 port 22: no matching key exchange method found. Their offer: mlkem768x25519-sha256,sntrup761x25519-sha512,sntrup761x25519-sha512@openssh.com,curve25519-sha256,curve25519-sha256@libssh.org,ecdh-sha2-nistp256,ecdh-sha2-nistp384,ecdh-sha2-nistp521,ext-info-s,kex-strict-s-v00@openssh.com

Unable to negotiate with 127.0.0.1 port 22: no matching host key type found. Their offer: rsa-sha2-512,rsa-sha2-256,ecdsa-sha2-nistp256,ssh-ed25519

Unable to negotiate with 127.0.0.1 port 22: no matching cipher found. Their offer: chacha20-poly1305@openssh.com,aes128-gcm@openssh.com,aes256-gcm@openssh.com,aes128-ctr,aes192-ctr,aes256-ctr
```

三句同一个模板：`Unable to negotiate with <IP> port <N>: no matching XXX found.`
**`Their offer:` 后面那一串是【对方】支持的清单，比任何攻略都权威**：照它挑一个就行。
（注意第一条：`diffie-hellman-group1-sha1` 明明在**本机** `ssh -Q kex` 的 15 种里，
但服务端不接 —— 这就是前面说的「本地支持」≠「能谈成」。）
`HostKeyAlgorithms` 那次还可能长成另一句：
`It is also possible that old OpenSSH versions (< 7.0) are not supported.`

**③ 反例：`MACs=hmac-md5` 在这台机器上【居然谈成了】—— 别把攻略当定律**

```text
$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o PreferredAuthentications=publickey -o StrictHostKeyChecking=no -o UserKnownHostsFile=<沙箱>/kh/neg -o MACs=hmac-md5 learner@127.0.0.1 true 2>&1 | tail -1
  （这条按设计会失败，就为了看真实报错原文）
learner@127.0.0.1: Permission denied (publickey,password).
```

`hmac-md5` 是所有教程里「一定会谈崩」的算法，结果本机服务端答应了，一路走到认证。
**同样的降级配置，换一台服务器结论就反过来。永远以 `Their offer` 为准。**

**④ `=` 和 `+` 的区别：`=` 是【整表替换】，`+` 是【追加】**（`ssh -G` 现场证明）：

```text
  默认 kex 列表（截断）：
kexalgorithms mlkem768x25519-sha256,sntrup761x25519-sha512,sntrup761x25519-sha512@openssh.com,curve2

  = 覆盖成只剩一个：
kexalgorithms diffie-hellman-group14-sha1

  + 追加（默认整表都留着）：
kexalgorithms mlkem768x25519-sha256,sntrup761x25519-sha512,sntrup761x25519-sha512@openssh.com,curve2
```

再拿真连接验证这个差别的后果：

```text
$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o PreferredAuthentications=publickey -o StrictHostKeyChecking=no -o UserKnownHostsFile=<沙箱>/kh/neg -o 'KexAlgorithms=+diffie-hellman-group14-sha1' learner@127.0.0.1 true 2>&1 | tail -1 || true
learner@127.0.0.1: Permission denied (publickey,password).
```

> 同一条配置：**用 `=` 死在 kex（见 ②），用 `+` 顺利走到认证。**
> 这就是网上「加个 `+` 就好了」的老方子有时管用、有时更糟的原因。
> 还有第三种 `-hmac-md5`（从表里**删掉**）—— `=` / `+` / `-` 三种语义混用是配置事故高发区，
> 改完一律用 `ssh -G | grep '^kexalgorithms'` 复述一遍再决定。

连老服务器的正解顺序：
`ssh -Q` 看自己有什么 → 只把**对方支持的**用 `=` 顶上 →
并且**一定写在 `Host 老IP` 作用域里**，别让全局降级传染到你的新机器（这是安全事项，不只是好用）。
新客户端连老服务器的另一个坑：host key 是 `ssh-rsa` 时，
`HostKeyAlgorithms=+ssh-rsa` 和 `PubkeyAcceptedAlgorithms=+ssh-rsa` **两个都要加**，漏一个照样死
（前者管「服务器是什么 key」，后者管「我的用户 key 能签成什么」，见上面 `-Q` 的五个类别）。

### 实战 9（demo 第 9 节）：半路断线 —— `kex_exchange_identification` / `Connection closed` / reset

这一节四种死法全部用**手搭的假服务器**造（`127.0.0.1` 上独立端口，完全不碰系统 sshd）：
`rst` 用 `SO_LINGER=0` 发 RST、`fin` 正常关、`garbage` 回一段 HTTP 400、
`banner` 发一个像样的版本横幅再关。

```text
$ python3 <沙箱>/fakeserver.py rst 34295 &        # 后台起【fake_rst】
  [PID 134732 已登记，脚本退出时会自动 kill]

$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=6 -o IdentityFile=<沙箱>/keys/_no-default-probe -o StrictHostKeyChecking=no -o UserKnownHostsFile=<沙箱>/kh/fake -p 34295 learner@127.0.0.1 true
  （这条按设计会失败，就为了看真实报错原文）
kex_exchange_identification: read: Connection reset by peer
Connection reset by 127.0.0.1 port 34295
  [退出码 255 —— 非 0 是预期的]

$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=6 -o IdentityFile=<沙箱>/keys/_no-default-probe -o StrictHostKeyChecking=no -o UserKnownHostsFile=<沙箱>/kh/fake -p 33509 learner@127.0.0.1 true
  （这条按设计会失败，就为了看真实报错原文）
Connection closed by 127.0.0.1 port 33509
  [退出码 255 —— 非 0 是预期的]

$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=6 -o IdentityFile=<沙箱>/keys/_no-default-probe -o StrictHostKeyChecking=no -o UserKnownHostsFile=<沙箱>/kh/fake -p 47481 learner@127.0.0.1 true
  （这条按设计会失败，就为了看真实报错原文）
Connection closed by 127.0.0.1 port 47481
  [退出码 255 —— 非 0 是预期的]

$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=6 -o IdentityFile=<沙箱>/keys/_no-default-probe -o StrictHostKeyChecking=no -o UserKnownHostsFile=<沙箱>/kh/fake -p 53987 learner@127.0.0.1 true
  （这条按设计会失败，就为了看真实报错原文）
Connection closed by 127.0.0.1 port 53987
  [退出码 255 —— 非 0 是预期的]
```

**怎么读这几句**（注意第三个 `garbage`：端口后面是个 **Web 服务**，给出的也是 `Connection closed by`）：

| 原文 | 对面做了什么 | 可能原因 |
|---|---|---|
| `kex_exchange_identification: read: Connection reset by peer` | 发了 **RST** | 防火墙 REJECT、端口后面根本不是 sshd、fail2ban 踢你、**sshd 并发超限随机丢** |
| `Connection closed by 127.0.0.1 port N` | 发了 **FIN**（正常关） | 版本横幅还没换完就关了：sshd 不想理你（版本、限速）、**端口打错后面是别的服务** |
| 完全没有 `kex` / `identification` 字样，只有 `connect to host ...` | 停在 **TCP** | 回到实战 1，**别在这里查密钥** |
| `ssh_exchange_identification: ...`（旧前缀） | 同上 RST | 那是 8.x 及更早客户端的写法，同一件事（实战 6⑤ 已证明本机二进制里没这串） |

**这一类和实战 1 的分界就在 `Connection established.` 有没有出现**：
出现了才轮得到 KEX。`kex_exchange_identification` 是**版本横幅交换阶段**的失败 ——
它既不是网络不通（TCP 已经通了），也不是认证失败（还没到认证），
所以「在这里查密钥」是 100% 白费。

**本机现场还多送了一条真实的限流证据**（附录 A，本手册补跑撞上的）：
`debug1: kex_exchange_identification: banner line 0: Not allowed at this time` 然后
`Connection reset by 127.0.0.1 port 22`。这不是参数问题，是**同机 sshd 在几十次失败认证之后
主动拒绝握手**。处置办法不是改配置，而是**等 30~60 秒再判**：
如果重试后同一条命令的报错变了，那你刚才看到的根本不是你要找的那个原因（实战 3、5 都吃过这个亏）。

**服务端默认值里能本机查的这一半**（`Timeout, server not responding.` 需要登录成功后）：

```text
$ ssh -F /dev/null -G learner@127.0.0.1 | grep -E '^(serveraliveinterval|serveralivecountmax|tcpkeepalive|connectiontimeout) '
Pseudo-terminal will not be allocated because stdin is not a terminal.
tcpkeepalive yes
serveralivecountmax 3
serveraliveinterval 0

$ grep -E '^#?MaxStartups' /etc/ssh/sshd_config || true
#MaxStartups 10:30:100
```

四个值连起来读，就是「隧道 / 长连接跑着跑着断了，日志一个字都没有」的根因：

- `serveraliveinterval 0` → 客户端**默认不发保活**。空闲链路被中间设备静默丢弃时，ssh 完全不知情。
- `serveralivecountmax 3` 但 `interval 0` → 保活这套判断根本没启动（次数乘 0 还是 0）。
- `tcpkeepalive yes` → **别指望它救你**：那是内核级 TCP keepalive，Linux 默认间隔在 2 小时量级，
  而且只有对端**回 RST** 才知道，链路被静默黑洞化时它什么都测不出来。
  → 正确写法：`-o ServerAliveInterval=30 -o ServerAliveCountMax=3`（≈ 90 秒无回应就判定断开）。
- `connectiontimeout` **一行都没有** —— 只在显式设置时才打印（实战 3 那条规律的第二次验证）。
  端口级超时用 `-o ConnectTimeout`，它是**连接阶段的总预算**（附录 A 的
  `timeout: 8000 ms remain after connect` 就是这句话的实证据）；
  `ServerAliveInterval` 是**会话阶段**的，两者别混。
- `MaxStartups 10:30:100`（默认注释着 = 生效值）：未认证连接超过 10 条开始**按 30% 概率随机丢**，
  到 100 条全丢。**这就是「并发脚本偶尔连不上、单条手动又正常」的官方解释。**
  demo 默认不跑这场（要 `HAMMER=1` 才跑），因为它是**对本机真实 sshd 施压**：
  修法不是写重试循环，而是**降并发** —— 上连接池（04 章 `ControlMaster`）或者让脚本串行。

本机拿不到、需服务器验证的两句（**别把模板当实测**）：

- `kex_exchange_identification: Connection closed by remote host` —— 字符串在二进制里
  （实战 6⑤ 已证明命中 1），但 10.2 在这条路上给你的是 `Connection closed by <IP> port N`
  这句更具体的形式，含义一样。
- `Timeout, server not responding.` —— 只出现在**已登录成功、会话进行中**：
  `ServerAliveInterval` 连续 `CountMax` 次没回应。自查模板：
  `ssh -o ServerAliveInterval=15 -o ServerAliveCountMax=4 用户@服务器 'sleep 5; echo ok'`

### 实战 10（demo 第 10 节）：`Warning: Permanently added ...` —— 这根本不是错误

```text
$ rm -f <沙箱>/kh/fresh; ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh/fresh learner@127.0.0.1 true 2>&1 | head -1 || true
Warning: Permanently added '127.0.0.1' (ED25519) to the list of known hosts.
```

含义：首次见这台服务器，**指纹已经记下了**。它是 WARNING 级别，连接照常继续。
很多人为了让 CI 日志干净把它「消掉」，结果顺手关掉了整章最重要的安全机制 ——
**正确做法是降日志级别，不是关校验**：

```text
$ ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR -o UserKnownHostsFile=<沙箱>/kh/quiet learner@127.0.0.1 true 2>&1 | head -2
  （这条按设计会失败，就为了看真实报错原文）
learner@127.0.0.1: Permission denied (publickey,password).

$ printf '  quiet 那份 known_hosts 里确实写进去了（校验照样做了）：\n'; head -c 120 <沙箱>/kh/quiet; echo ' ...'
  quiet 那份 known_hosts 里确实写进去了（校验照样做了）：
127.0.0.1 ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIJ8COfYZlWVtvsITvesDuL0e7PzZiQCCKeFi9lLjMMhY
```

**`LogLevel=ERROR` 之后 WARNING 不见了，但文件里那一行实实在在写进去了** ——
这就是「让日志干净」和「让校验失效」的区别，demo 用 `head -c 120` 把它钉死了。

同一类「看着像报错其实不是」的还有：

```text
     Pseudo-terminal will not be allocated because stdin is not a terminal.   （没终端却要不要 pty）
     Received disconnect from ... 2: Too many authentication failures          （那是真报错，见场景 4）
     no such identity: /path  : No such file or directory                      （-i 指向了不存在的文件）
```

本章里 `Pseudo-terminal ...` 出现得非常**离谱**：它出现在 `ssh -G` 的输出里（实战 3、9 都撞上了）。
`-G` 根本不建连接、不该有 pty，这句属于 10.2 的噪音。所以：
**`ssh -G | grep ...` 之后不要用「第一行」判断任何东西**，也别在脚本里 `head -1` 取值。
`no such identity:` 同理 —— demo 那条挡箭牌 `IdentityFile=.../_no-default-probe`
每次都会打它一行，**那是设计出来的**（用来保证不可能登录成功），不是故障。

### 实战 11（demo 第 11 节）：git 报的 `Permission denied (publickey)` —— 多半 git 用的不是你那个 ssh

**① 先问：git 调的是哪个 ssh？（PATH 里可能不止一个）**

```text
$ which -a ssh; printf '  git 版本：%s\n' "$(git --version)"
/usr/bin/ssh
/bin/ssh
  git 版本：git version 2.55.0
```

（本机这两个是同一个文件的两个路径，`/bin` 是 `/usr/bin` 的软链；
 但 WSL / Git Bash / IDE 内置终端里它们**经常真是两个不同的 ssh**，
 版本不同、读的配置文件不同 —— 那是「普通终端行、IDE 不行」的头号原因。）

**② 真跑一次 git**（本机 sshd 照样把认证挡回去，这就是你熟悉的那几句）：

```text
$ GIT_SSH_COMMAND='ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/keys/_no-default-probe -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh/real' GIT_TERMINAL_PROMPT=0 git ls-remote git@127.0.0.1:/nope.git 2>&1 | head -4
  （这条按设计会失败，就为了看真实报错原文）
no such identity: <沙箱>/keys/_no-default-probe: No such file or directory
git@127.0.0.1: Permission denied (publickey,password).
致命错误：无法读取远程仓库。
```

**git 后面那句「Please make sure you have the correct access rights」其实一点信息都没给**。
真正的线索永远在它调用的那条 `ssh` 上，所以排错入口就一句：**把 `-vv` 塞给 git 用的那个 ssh**
（实战 2 的三段定位立刻能用）：

```bash
GIT_SSH_COMMAND='ssh -vv' git ls-remote ...
```

**③ 用假 ssh 把 git 传下去的 argv 打出来**（理解 `GIT_SSH_COMMAND` 的最短路径）：

```text
  git 实际执行的 ssh 参数（一行一个 argv）：
    -G
    -o
    SendEnv=GIT_PROTOCOL
    git@127.0.0.1
    git@127.0.0.1
    git-upload-pack 'nope.git'
```

看到 `git-upload-pack '...'` 那一段就通了：**git 是把子命令当成【远端要执行的文件】交给 ssh 的**。
所以 `GIT_SSH_COMMAND` 里**只能放 ssh 自己的选项**；要放 shell 逻辑（变量、`if`、管道）必须包一层脚本。
（另外注意第一行是 `-G`：git 会先跑一次 `ssh -G` 来探测远端环境 —— 这就是
「你看到的 `-G` 输出」和「git 看到的 `-G` 输出」必须是同一份配置的原因。）

**④ 一个真实坑：`GIT_SSH_COMMAND` 会让 git 用「simple」ssh 变体，于是 `ssh://` 带端口就不干活**

```text
$ GIT_ARGV_LOG=<沙箱>/g0.log GIT_SSH_COMMAND=<沙箱>/bin/fake-ssh git ls-remote ssh://git@127.0.0.1:22/nope.git 2>&1 | head -2
  （这条按设计会失败，就为了看真实报错原文）
致命错误：ssh 变体 'simple' 不支持设置端口
```

**这句和 SSH 一点关系都没有，是 git 自己拒绝的**（主语判别法，见心智模型第三块）。
修法任选其一：改用 scp 式地址 `git@host:path`（端口靠 ssh 配置里 `Host` 段的 `Port` 指定），
或者显式告诉 git 这是个真 ssh：`GIT_SSH_VARIANT=ssh`。demo 验了后者确实通了：

```text
  （这条按设计会失败，就为了看真实报错原文）
Warning: Permanently added '127.0.0.1' (ED25519) to the list of known hosts.
no such identity: <沙箱>/keys/_no-default-probe: No such file or directory
git@127.0.0.1: Permission denied (publickey,password).
```

（对照上一条：加了 `GIT_SSH_VARIANT=ssh` 之后**报错换成了 ssh 的**，说明 git 这关放行了，
现在轮到 SSH 世界。这种「报错前缀换了一个世界」的位移，就是修复生效的证据。）

**⑤ 三种指定方式，优先级从高到低（都让假 ssh 把 argv 打出来当证据）**：

```text
  a) GIT_SSH_COMMAND（临时，优先级最高，推荐）：
致命错误：无法读取远程仓库。
     实际传参：-o BatchMode=yes -G
  b) core.sshCommand（仓库长期，优先级低于环境变量）：
致命错误：无法读取远程仓库。
     仓库里存的：<沙箱>/bin/fake-ssh -F /dev/null -o BatchMode=yes
     实际传参：-F /dev/null -o
     再套一层环境变量（变量赢）：
致命错误：无法读取远程仓库。
     实际传参：-o Visual=yes -G
  c) GIT_SSH（老变量：只认一个可执行文件路径，不能带参数）：
致命错误：无法读取远程仓库。
     实际传参：-F /dev/null -o
```

读法：`a` 传下去的是环境变量里那串参数（`-o BatchMode=yes`）；
`b` 传下去的是仓库配置里的（`-F /dev/null`）；而 `b` 的仓库**同时**存在 `GIT_SSH_COMMAND` 时，
传下去的是**变量的** `-o Visual=yes` —— **环境变量赢**。
`c` 最有意思：`GIT_SSH` 只接受**一个可执行文件路径**，你跟在后面的参数会被丢掉，
于是它退化成用仓库配置里的参数 —— 这就是「不要用 `GIT_SSH` 传选项」的实测理由。

**一张对症表**（demo 里那张的完整版）：

| 症状 | 真因 | 取证命令 |
|---|---|---|
| GitHub 好使、`ssh -T` 也好使，只有某个仓库不行 | `core.sshCommand` / 部署 key / 该仓库的 Host 段 | `git config --show-origin --get-all core.sshCommand` |
| WSL / Git Bash / IDE 内置终端不行，普通终端行 | PATH 里有**两个不同的 ssh** | `which -a ssh; ssh -V`（在**每个**终端里各跑一次） |
| cron / CI runner 里不行，手动行 | 环境变量与 `HOME` / agent 没跟过去 | 实战 7 三件套 + 实战 12⑥（`HOME` 换不掉 key） |
| 报错里永远是 `git@主机` 而不是你的登录名 | 那是**远端用户**，正常，不是配置错 | —— |
| 端口非 22 时怎么都不通 | scp 式简写**不带端口**（会骗人） | 写全 `ssh://git@主机:端口/path`，或配 `Host` 段的 `Port` |
| git 的报错看不懂 | 那多半是 git 的话，不是 ssh 的 | `GIT_SSH_COMMAND='ssh -vv'` 重新跑 |
| 加了 `GIT_SSH_COMMAND` 后带端口的地址突然不能用 | git 切到 `simple` 变体 | `GIT_SSH_VARIANT=ssh`（实测有效） |

### 实战 12（demo 第 12 节）：原因其实在客户端配置 —— first-match-wins、`Include`、两个不存在的环境变量

**① 同一个 `Host` 模式写了两遍：只有第一次生效**

```text
$ ssh -F <沙箱>/cfg/dup -G dup 2>/dev/null | grep -E '^(hostname|port|user) '
user first-wins
hostname 10.0.0.1
port 2200
```

配置文件里 `Host dup` 写了两遍（第二遍是 `10.0.0.2` / `2222` / `second-ignored`）。
`-G` 的裁决：**三个值全来自第一个块**。注意规则不是「后块覆盖前块」，而是
**「同一个关键字第一次出现就定了，后面静默忽略」** —— 这个区别决定了下面所有现象。

**② 命令行 `-o` 也守同一条规矩：重复的关键字【第一次赢】**

```text
  先 no 后 yes -> 生效的是哪一个：
stricthostkeychecking false
  先 yes 后 no -> 生效的是哪一个：
stricthostkeychecking true
```

> **这是本章对「脚本里明明设了护栏却无效」最硬的一条解释。**
> 把默认护栏写在一个变量里（`S="ssh -o StrictHostKeyChecking=no ..."`），
> 然后在具体命令后面再补一个 `-o StrictHostKeyChecking=accept-new` 想「覆盖」它 ——
> **覆盖不了，前面那个赢，而且是静默的。**
> demo.sh 的 `$S` 前缀因此只放**不会被单条命令覆盖的东西**
> （`-F /dev/null`、`BatchMode`、`ConnectTimeout`、挡箭牌 `IdentityFile`），
> 而把 `UserKnownHostsFile` / `StrictHostKeyChecking` / `PreferredAuthentications`
> 写在**每条命令自己身上**，脚本注释里就是这个原因。这是写 ssh 包装脚本的必修课，
> 也是本章里少数「你会在 CI 代码里一眼认出来」的反模式。

**③ 通配 `*` 写在前面会吃掉后面所有具体 `Host`（反直觉但正确）**

```text
$ ssh -F <沙箱>/cfg/star -G myserver 2>/dev/null | grep -E '^(user|port) '
user eaten-by-star
port 2205
```

`Host *` 块在前（`User eaten-by-star`），`Host myserver` 在后（`User never-mind` + `Port 2205`）。
结果 `user` 是 `*` 里的，`port` 却是具体块里的 —— **`Host` 块不是「整段匹配」，是「逐键第一次赢」**：
`*` 里没写 `Port`，所以 `Port` 仍然从后面的具体块拿到了。
所以：`*` 块里只放**所有主机都需要、而且不会被覆盖**的东西；例外要写在 `*` **之前**。

**④ `Include` 的位置决定谁赢**

```text
$ ssh -F <沙箱>/cfg/inc -G inc 2>/dev/null | grep -E '^(port|user) '
user from-include
port 1000

$ printf '  把 Include 挪到最后：\n'; ssh -F <沙箱>/cfg/inc2 -G inc 2>/dev/null | grep -E '^(port|user) '
  把 Include 挪到最后：
user from-include
port 3000
```

片段目录里有 `10-base`（`Port 1000` + `User from-include`）和 `20-override`（`Port 2000`），
主文件自己还有 `Port 3000`。`Include` 写在**最前** → `10-base` 赢（`port 1000`），
`20-override` 和主文件的 `3000` **都被忽略**；`Include` 挪到**最后** → 主文件赢（`port 3000`），
而 `User` 因为只在 include 里出现过，所以**两处都还是** `from-include`。

> 想把发行版 / 系统级配置当**兜底**，就把 `Include` 放在自己文件的**最下面**；
> 想让它们**强制生效**（比如安全基线），就放最上面。
> 文件名前的 `10-` / `20-` 只在**同一个目录内部**排序，跨目录 / 跨文件一律按 Include 的位置算。
> （`/etc/ssh/ssh_config` 第 19 行就是 `Include /etc/ssh/ssh_config.d/*.conf`，同一条规矩。）

**⑤ 传说中存在的环境变量 `SSH_OPTS` / `SSH_CONFIG_FILE` —— 根本不存在（本机实测）**

```text
  作为独立字符串出现在二进制里 SSH_CONFIG_FILE    : 0 次
  作为独立字符串出现在二进制里 SSH_OPTIONS        : 0 次
  作为独立字符串出现在二进制里 SSH_OPTS           : 0 次
  作为独立字符串出现在二进制里 SSH_CONFIG         : 0 次

  设了 SSH_CONFIG_FILE 也没用 —— 下面这条生效的仍是 -F 指定的那个文件（user=eaten-by-star）：
user eaten-by-star
hostname myserver

  ssh 客户端真正会读的环境变量（本机 strings 抓的，权威）：
SSH_ASKPASS SSH_ASKPASS_PROMPT SSH_ASKPASS_REQUIRE SSH_AUTH_SOCK SSH_SK_HELPER
```

**没有任何变量能指定配置文件，能换文件的只有 `-F`**；也没有变量能注入公共选项。
那份 `strings` 抓出来的清单就是全部（5 个）。
所以你在网上看到的 `export SSH_OPTS="-o StrictHostKeyChecking=no"` 十有八九是从别的工具
（`GIT_SSH_COMMAND`、`DOCKER_OPTS`）串台来的 —— 它**不会报错，只是完全没用**，
这类「静默无效」比报错更危险，因为你以为护栏在。

**⑥ 那换个 `HOME` 能换掉默认 key 吗？本机实测：不能**

```text
  正常 HOME 下 ssh 找的是：
no such identity: <家目录>/.ssh/__probe_does_not_exist__: No such file or directory
  HOME 换成沙箱 fakehome 之后：
no such identity: <家目录>/.ssh/__probe_does_not_exist__: No such file or directory
```

两次展开出来的路径**一模一样**：config 里的 `~` 按**密码本（passwd）里的家目录**展开，**不看 `$HOME`**。
所以「给 cron / 容器设个 `HOME` 就换了一套 key」这个方子在这台 OpenSSH 上**无效**。
有效做法只有两条：每条命令 `-F 配置文件`；`IdentityFile` 写**绝对路径**。

（本手册就是靠这条规律才敢在沙箱里建一个 `fakehome/.ssh` —— 它根本不会被读到，
 你真实的 `~/.ssh` 全程没有被访问。这也意味着：你在容器里把 `HOME` 指到挂载卷，
 `ssh` 依然按 uid 对应的 passwd 家目录去找 `~/.ssh`，
 所以「容器里 ssh 找不到我的 key」通常是这个原因，而不是卷没挂上。）

### 实战 13（demo 第 13 节）：万能排查顺序 —— 每步都真跑，并写明它排除了哪一类

**第 0 步 先确认你到底连的是谁**（九成怪案死在这里；`-G` 把最终生效值全打出来）：

```text
$ ssh -F <沙箱>/cfg/dup -G dup 2>/dev/null | grep -E '^(hostname|port|user|identityfile|controlmaster|forwardagent) '
user first-wins
hostname 10.0.0.1
port 2200
controlmaster false
identityfile ~/.ssh/id_rsa
identityfile ~/.ssh/id_ecdsa
identityfile ~/.ssh/id_ecdsa_sk
identityfile ~/.ssh/id_ed25519
identityfile ~/.ssh/id_ed25519_sk
forwardagent no
```

这一步花 5 毫秒、不发包，却能把「你以为在连 A，其实在连 B」一次性解决。
`identityfile` 是**有序列表**：5 条默认值 + 你写的，谁在前面先递谁 ——
实战 4 那个「对的 key 永远轮不到」的排序问题就在这张表里现形。
（注意这里 `-F` 的是 `dup`，那份配置里没有 `IdentityFile`，所以列出的 5 条是内置默认清单；
 实战 2⑤ 里 `-F /dev/null` + `IdentityAgent=none` 时**没有** `Offering` 行，
 因为那几个默认路径下**没有文件**。表里有 ≠ 牌堆里有，这个区分很重要。）

**第 1、2 步 机器在不在 / 端口有没有人听**：

```text
$ ping -c 1 -W 1 127.0.0.1 2>&1 | tail -2
  （这条按设计会失败，就为了看真实报错原文）
1 packets transmitted, 1 received, 0% packet loss, time 0ms
rtt min/avg/max/mdev = 0.018/0.018/0.018/0.000 ms

$ nc -vz 127.0.0.1 22 2>&1 | tail -1
Connection to 127.0.0.1 22 port [tcp/ssh] succeeded!

$ nc -vz 127.0.0.1 47857 2>&1 | tail -1
  （这条按设计会失败，就为了看真实报错原文）
nc: connect to 127.0.0.1 port 47857 (tcp) failed: Connection refused
```

`ping` 那条被脚本标成「按设计会失败」，是因为管道尾段（`tail`）返回 0 把真实退出码吃掉了
（见本节最后一条纪律），而 `ping` 本身是成功的。**它通不通都不能给你结论**，理由见心智模型第四块。
`nc` 的两条一 succeed 一 refused，就是实战 1 那张表的两端，
**并且这一步同时排除了「密钥、指纹、算法」三类**：端口都没通，那些还轮不到。

**第 3 步 服务端确实在跑 sshd**（本机可查；服务器上要 root）：

```text
$ ss -lnt | grep ':22 '; printf '  sshd 进程：%s\n' "$(pgrep -a sshd | head -2 | tr '\n' '; ')"
LISTEN 0      4096         0.0.0.0:22         0.0.0.0:*
LISTEN 0      4096            [::]:22            [::]:*
  sshd 进程：72964 sshd: /usr/sbin/sshd -D [listener] 0 of 10-100 startups;
```

这三行是三合一证据：`0.0.0.0:22` = 绑在所有地址上（不是只绑回环）、
`4096` = accept backlog、`0 of 10-100 startups` = **`MaxStartups` 的实时读数**
（当前未认证连接 0 条，阈值 10~100 —— 和实战 9 的配置值对上了）。
`ss -lnt` 各列：`LISTEN`=状态、`0`=当前排队数、`4096`=上限、第 4 列才是本地地址:端口。
在有 systemd 的服务器上自己跑（**本手册不动系统服务，这里只给模板**）：
`systemctl is-active ssh`（Debian/Ubuntu 叫 `ssh`，RHEL 系叫 `sshd`）、`ss -lnt | grep -E ':(22|2222) '`。

**第 4 步 `ssh -vv`（或 `-vvv`）三段定位**（本机真跑，`grep -n` 把锚点行号也打出来）：

```text
4:debug1: Connecting to 127.0.0.1 [127.0.0.1] port 22.
7:debug1: Connection established.
46:debug1: kex: algorithm: mlkem768x25519-sha256
47:debug1: kex: host key algorithm: ssh-ed25519
87:debug1: Authentications that can continue: publickey,password
94:debug1: Offering public key: plain@sb ED25519 SHA256:Ae47YlA/WzUk+qiJUaAlMFA2ON6D0WF73OFFlBenV4g agent
96:debug1: Authentications that can continue: publickey,password
101:learner@127.0.0.1: Permission denied (publickey,password).
```

把上面那 100 多行压成四句话（**这张表是我画的，不是终端输出**）：

```
  停在 Connection established 之前   -> 实战 1（网络/端口）
  停在 kex 两行之前                 -> 实战 5（指纹）/ 8（算法）/ 9（半路断）
  停在 Offering/Permission denied   -> 实战 2/3/4/6/7（key、agent、权限）
  一路顺利却没有你要的效果           -> 实战 12（配置没生效）或 05 章（转发/服务端开关）
```

这四个行号（4 / 7 / 46 / 87）就是三段路的**里程碑**：
7 到 46 之间是版本横幅交换 + KEX（卡住 = 实战 9 那一类），46/47 之后才轮到主机指纹，
87 之后才是认证。行号差值异常大 = 中间在等（慢 DNS、重试、超时）。

**第 5 步 服务器侧**（需 root，需服务器验证 —— 本手册**一条都没执行**，只给模板）：

```bash
journalctl -u sshd -n 50 --no-pager   # 或 /var/log/auth.log、/var/log/secure
/usr/sbin/sshd -D -d -p 2222          # 前台单连接调试，服务器侧看到的一切都会打出来
sshd -T | grep -iE 'authorizedkeysfile|permitrootlogin|passwordauth|pubkeyauth|maxauthtries|allowtcpforward'
# 用完记得 kill 掉临时 sshd，别留监听。
```

**第 6 步 还查不出来就换客户端 / 换机器**：`ssh -V` 记版本，同一份 config 在别的机器试一次 ——
这一步能区分「配置写错了」和「这台机器的 OpenSSH 太老 / 太新，不支持某个算法」
（实战 6⑤ 的字符串命中数、实战 8 的 `ssh -Q`，都在教你这一步为什么值得做）。

```text
$ printf '  本机客户端版本：%s\n' "$(ssh -V 2>&1)"
  本机客户端版本：OpenSSH_10.2p1 Ubuntu-2ubuntu3.6, OpenSSL 3.5.5 27 Jan 2026
```

**最后一条排错纪律**：以上 6 步的顺序**不能换**。
理由不是「这样整齐」，而是**每一步都会让下一步的假设成立或不成立**：
没确认端口有人听就去查密钥，你看到的 `Permission denied` 可能是限流假象（实战 3 现场演示过）；
没确认 `-G` 的生效值就去改配置文件，改的是根本没被读到的那个块（实战 12）；
没确认 `Connection established.` 出现就去读 `kex`，你会把网络问题当成算法问题（实战 9）。

---

## 补跑取证（demo 的**场景 14**，专门跑正文引用的那三组数字）

> 前面正文里引用的行数阶梯、`StrictHostKeyChecking=yes/ask` 的干净判决、以及限流现场，
> 都出自 demo 的场景 14，捕获文件和别处一样在 `~/.cache/handbook-captures/ssh06.txt`。
> 之所以单独立一节：这几条要么和 demo 主体的连接顺序相互干扰（22 端口会被前面的失败连接限速），
> 要么需要额外加护栏（`IdentityAgent=none`）才能让数字可比。
> 每条依旧是 `-F /dev/null` + 沙箱 `UserKnownHostsFile` + 不存在的 `IdentityFile`（+ `BatchMode`），
> 不可能登录成功、不弹口令、不读写 `~/.ssh`，只观察真实失败输出，用完立刻删沙箱。

### 附录 A：`-v / -vv / -vvv` 的行数阶梯，以及一次真实的限流

```text
--- ① 附录 A：-v / -vv / -vvv 的行数阶梯（同一目标、同一套护栏，只差 -v 的个数）
   （加 IdentityAgent=none 是为了让行数可比：不然每次会多出若干条 agent 里那张牌的尝试行）
$ ssh -v -o IdentityAgent=none … learner@127.0.0.1 true > 沙箱里的日志（屏幕只数行数）
  ssh -v    总行数= 50  debug1= 47  debug2=  0  debug3=  0

$ ssh -vv -o IdentityAgent=none … learner@127.0.0.1 true > 沙箱里的日志（屏幕只数行数）
  ssh -vv   总行数= 95  debug1= 47  debug2= 46  debug3=  0

$ ssh -vvv -o IdentityAgent=none … learner@127.0.0.1 true > 沙箱里的日志（屏幕只数行数）
  ssh -vvv  总行数=129  debug1= 47  debug2= 46  debug3= 34
```

`-v` 的最后 7 行（**这一次撞上了 22 端口的限流**，正好是实战 9 讲的现场）：

```text
debug1: no pubkey loaded from <沙箱>/nokey
debug1: identity file <沙箱>/nokey type -1
debug1: no identity pubkey loaded from <沙箱>/nokey
debug1: Local version string SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
debug1: kex_exchange_identification: banner line 0: Not allowed at this time
kex_exchange_identification: read: Connection reset by peer
Connection reset by 127.0.0.1 port 22
```

`-vv` 独有的 `debug2`（**双方完整的算法提案表**，这就是实战 8 必须用 `-vv` 的原因）：

```text
debug2: resolve_canonicalize: hostname 127.0.0.1 is address
debug2: local client KEXINIT proposal
debug2: KEX algorithms: mlkem768x25519-sha256,sntrup761x25519-sha512,sntrup761x25519-sha512@openssh.com,curve25519-sha256,curve25519-sha256@libssh.org,ecdh-sha2-nistp256,ecdh-sha2-nistp384,ecdh-sha2-nistp521,diffie-hellman-group-exchange-sha256,diffie-hellman-group16-sha512,diffie-hellman-group18-sha512,diffie-hellman-group14-sha256,ext-info-c,kex-strict-c-v00@openssh.com
debug2: ciphers ctos: chacha20-poly1305@openssh.com,aes128-gcm@openssh.com,aes256-gcm@openssh.com,aes128-ctr,aes192-ctr,aes256-ctr
debug2: MACs ctos: umac-64-etm@openssh.com,umac-128-etm@openssh.com,hmac-sha2-256-etm@openssh.com,hmac-sha2-512-etm@openssh.com,hmac-sha1-etm@openssh.com,umac-64@openssh.com,umac-128@openssh.com,hmac-sha2-256,hmac-sha2-512,hmac-sha1
```

（`debug2` 里还有一行 `host key algorithms:`，17 个条目，最长的那行；
它和上面 `KEX algorithms` 的区别就是「主机密钥类型」和「密钥交换算法」的差别 —— 实战 8 的 `-Q key` / `-Q kex`。）

`-vvv` 独有的 `debug3`（**报文级 / 文件级 / 计时级**）：

```text
debug3: Running on Linux 7.0.0-14-generic #14-Ubuntu SMP PREEMPT_DYNAMIC Mon Apr 13 11:09:53 UTC 2026 x86_64
debug3: Started with: ssh -vvv -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=<沙箱>/nokey -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh/k learner@127.0.0.1 true
debug3: ssh_connect_direct: entering
debug3: set_sock_tos: set socket 3 IP_TOS 0xb8
debug3: timeout: 8000 ms remain after connect
debug3: record_hostkey: found key type ED25519 in file <沙箱>/kh/k:1
debug3: load_hostkeys_file: loaded 1 keys from 127.0.0.1
debug3: order_hostkeyalgs: have matching best-preference key type ssh-ed25519-cert-v01@openssh.com, using HostkeyAlgorithms verbatim
debug3: send packet: type 20
```

`debug3` 有三件独有能力，值得单独记：

1. `Started with: ssh ...` —— **它把你的完整命令行打了出来**（贴网上之前先看这行有没有泄露路径 / 用户名）。
2. `timeout: 8000 ms remain after connect` —— 证明 `ConnectTimeout` 是**整个连接阶段的总预算**，
   不是每次 `read` 单独计时（实战 9 那个概念的实证据）。
3. `send packet: type 20` / `receive packet: type 20` —— 「卡在等哪一个包」只有 `-vvv` 能回答
   （`type 20` 属于 KEX 刚完成、准备进认证的那道交界）。

对照结论：**`-v` 12 行足够定「第几段」；`-vv` 才看得见「整张算法表」；`-vvv` 才看得见「等哪个包」。**
顺序就按这个来，别一上来 `-vvv` 然后在 150 行里找线索。

### 附录 B：`Bad owner or permissions` 在客户端侧**能**复现，条件是 `Include`

实战 6⑥ 说 `-F` 的文件不查权限（demo 实测 `chmod 666` 照样生效）。
本手册补跑了四种组合，把这条报错的**真实成因**钉死了：

```text
A1: 含 Include(666) 的 -F 配置，stderr 全文：
   ssh 退出码=255
   stderr=[Bad owner or permissions on <沙箱>/d/10-x]
   serveraliveinterval 行=[无]
A2: 把 Include 的文件权限改回 600：
serveraliveinterval 99
A3: 直接 -F 666 文件（对照组）：
serveraliveinterval 42
D: known_hosts 666 会不会被客户端嫌弃： 0 次提及 bad owner
E: ssh 二进制里的原文： Bad owner or permissions on %s
```

结论四条（都是本机实测）：

1. **`-F` 指向的顶层配置文件：不查权限**（A3：666 照样读，值照样生效）。
2. **被 `Include` 进来的文件：查！** 一旦它 666，`ssh` 在**本地当场退出 255**，
   报 `Bad owner or permissions on <那个文件>`，而 `serveraliveinterval` **一行都没打印出来** ——
   也就是**整条命令死在配置解析阶段，一个网络包都没发**。
3. `known_hosts` 权限太开：客户端**不管**（D）。所以这句报错**不是** known_hosts 引起的。
4. 这句字符串确实在 `ssh` 二进制里（E，格式串 `Bad owner or permissions on %s`），
   所以它**既可能是服务端 sshd 报的，也可能是你客户端的 Include 链报的**。

**这一条对本手册最重要的一层意义**：`/etc/ssh/ssh_config` 第 19 行是
`Include /etc/ssh/ssh_config.d/*.conf`（本机实测，只读）。
现在那一份是 `root:root 644`（`/usr/lib/systemd/ssh_config.d/20-systemd-ssh-proxy.conf`），
**所以本机不带 `-F` 也不会触发**（本手册真跑过一次 `ssh -o BatchMode=yes -G 127.0.0.1`，正常输出）。
但只要有一天那个目录里出现一个**属主不对 / group-writable** 的 drop-in
（某些配置管理工具、第三方安装脚本会这么干），
**你机器上每一条不带 `-F` 的 `ssh` / `scp` / `rsync` / `git` 都会在本地当场死掉**，
而报错长得像「服务端权限问题」，把人往服务器方向引。

> 这就是全书硬规矩「**每条 ssh 都带 `-F /dev/null` 或沙箱配置**」的真正原因，
> 它同时解决三件事：不受系统配置污染、不被 Include 链炸死、不碰你真实的 `~/.ssh`。
> 顺带说明：`~/.ssh/config` 本身也属于**被检查**的那一类（这就是「改完 config 突然全挂」的来源），
> 但本手册不读写你的 `~/.ssh`，所以**那一条本机未验证**。

### 附录 C：`StrictHostKeyChecking=yes` / `ask` 冷却后的**干净判决**原文

demo 那两条被限流打断了（实战 5 里诚实标注过）。本手册等 sshd 冷却后各补跑一次：

```text
=== 客户端立刻试一次 StrictHostKeyChecking=yes（演示刚结束，看是否有罚时残留）===
No ED25519 host key is known for 127.0.0.1 and you have requested strict checking.
Host key verification failed.
=== 等 50s 再试同一条 ===
No ED25519 host key is known for 127.0.0.1 and you have requested strict checking.
Host key verification failed.

=== ask + BatchMode=yes + stdin 关掉（模拟脚本/cron）===
Host key verification failed.
=== ask + 不带 BatchMode（同一条，仍然不可能登录成功）===
ssh_askpass: exec(/usr/bin/ssh-askpass): No such file or directory
Host key verification failed.
=== no + 首次（对照：一句提示都没有，直接走到认证）===
Warning: Permanently added '127.0.0.1' (ED25519) to the list of known hosts.
no such identity: <沙箱>/nokey: No such file or directory
learner@127.0.0.1: Permission denied (publickey,password).
```

**这批补跑把实战 5 那张表的两个空格填上了**：
`ask` 在没有终端时给的是**光秃秃一句** `Host key verification failed.`（不告诉你为什么）；
把 `BatchMode` 摘掉，它才暴露出**真正的原因**是 `ssh_askpass: exec(...): No such file or directory`
—— 也就是「我想问你，但我找不到问你的窗口」。
而 `yes` 多给你一句判决理由：`No ED25519 host key is known for 127.0.0.1 and you have requested strict checking.`
（**同一件事，`ask` 的报错比 `yes` 更难查**，因为少了那一行说明。这是「报错信息量」差异的一个具体例子。）

补跑还顺手把实战 2⑤「一张牌都没有」的干净尾部拿到了：

```text
debug1: Next authentication method: publickey
debug1: Will attempt key: <沙箱>/nokey  explicit
debug1: Trying private key: <沙箱>/nokey
no such identity: <沙箱>/nokey: No such file or directory
debug1: No more authentication methods to try.
learner@127.0.0.1: Permission denied (publickey,password).
```

对照 demo 场景 2⑤（`IdentityAgent=none`）那条的**空输出**，这里补两条排错纪律：

- **`-F /dev/null` 之后把 agent 一关，就真的「一张牌都没有」**：`Will attempt key` 只剩那条不存在的
  `explicit`，连 `Offering` 都没有一行。实战 2 标签表最后一行说的就是这种状态。
- **管道会把退出码吃掉**。demo 里几条标了「按设计会失败」却**没有打印退出码行**的，
  全都是因为管道最后一段（`head` / `grep`）返回 0 —— **管道的退出码 ≠ ssh 的退出码**。
  （附录 C 第一条里那个 `rc=0` 就是这么来的：ssh 其实失败了，`head` 赢了。）
  写排错脚本要么 `set -o pipefail`，要么读 `${PIPESTATUS[0]}`；
  看到「什么都没打印 + 没有退出码标记」，**第一反应是去查那条 ssh 是不是中途失败了**，
  而不是「ssh 没报错」。

---

## 命令速查表

每一条都是本章真跑过的（最后一行是服务端模板，本机只读到配置文件，没改、没重启，标了未验证）。

| 我想干什么 | 命令 | 只认这一行 / 注意 |
|---|---|---|
| 先确定死在三段路的哪一段 | `ssh -v <平时的命令> 2>&1 \| tail -25` | 三句锚点：`Connection established.` / `kex: algorithm:` / `Authentications that can continue:` |
| 看最后谈成了哪个算法 | `ssh -vv <…> 2>&1 \| grep -E '^debug1: kex:'` | `kex: algorithm: mlkem768x25519-sha256`、`kex: host key algorithm: ssh-ed25519` |
| 看双方「愿意谈什么」的完整清单 | `ssh -vv <…> 2>&1 \| grep -E 'KEX algorithms\|client ciphers\|MACs'` | 只有 `-vv` 才有；左边那列是**本机**能力，别当成对面的 |
| 看包级细节（谁先动的手） | `ssh -vvv <…> 2>&1 \| grep -E 'send packet\|Started with'` | ⚠ 先确认 `Started with:` 那行就是你以为的那条命令 |
| 看最终生效配置（权威） | `ssh -G <用户@主机>` | 小写键、**第一个匹配赢**；没有默认值的键根本不打印 |
| 只查几项 | `ssh -G <…> \| grep -E '^(user\|identityfile\|port\|stricthostkeychecking\|serveraliveinterval) '` | 值里 `yes`/`true` 混写（实测 `pubkeyauthentication true` 但 `passwordauthentication yes`），别拿字符串比 |
| 绕开 ssh 单验 TCP | `nc -vz 127.0.0.1 22` | 只有 refused / succeeded 两种答案，最能切割问题 |
| 端口到底有没有人听 | `ss -lnt \| grep ':22 '` + `pgrep -a sshd` | `grep` 空 = 没进程在听 → 后面一切报错都是结果不是原因 |
| 本机支持哪些算法 | `ssh -Q kex`、`ssh -Q key`、`ssh -Q cipher`、`ssh -Q mac` | 用来判断是「对面太老」还是「我填错了」；填错会得到 `Bad SSH2 cipher spec` |
| 猜这句报错是谁说的 | `strings /usr/bin/ssh \| grep -c 'unsupported cache option'` | 命中 0 = **不是本机 ssh 说的**，去查版本/别的工具 |
| 指纹在不在 known_hosts | `ssh-keygen -F <主机>` | `-H` 哈希过之后 `grep 主机名` 永远 0，只有 `-F` 认 |
| 主机 key 真的换了 | `ssh-keygen -R <主机>` | 会留 `<file>.old` 明文备份，别顺手删 |
| 首次连接免交互但仍要记录 | `-o StrictHostKeyChecking=accept-new` | 不要用 `no`：`no` 是**整个机制**关掉 |
| 把噪音压掉只留真错误 | `-o LogLevel=ERROR` | 消掉 `Pseudo-terminal will not be allocated…` 和 `Warning: Permanently added…` |
| 私钥文件本身坏没坏（不经 ssh） | `ssh-keygen -y -f <key>` | 能吐公钥 = 可读 + 格式对；`Load key …: invalid format` 在这一步就能复现 |
| 老格式私钥 / 改口令 | `ssh-keygen -p -m PEM -f <key>` | 先备份；这一步改的是文件，不是 agent 里那张牌 |
| agent 现在有几张牌 | `ssh-add -l` | 空的时候是 `The agent has no identities.`，**不是**「agent 没起」 |
| agent 在不在 | `echo "SSH_AUTH_SOCK=$SSH_AUTH_SOCK"` | 空 = 没 agent；`env -i` 起的子 shell 会把它丢掉（实战 7 实测） |
| agent 被锁了 | `ssh-add -x` / `ssh-add -X` | 锁着签名的原文是 `sign_and_send_pubkey: … agent refused operation` |
| 只递这一把 key | `-o IdentitiesOnly=yes -i <key>` | 治 `Too many authentication failures`（服务端 `MaxAuthTries 6`，第 7 把轮不到） |
| 半死链路 | `-o ServerAliveInterval=30 -o ServerAliveCountMax=3` | 默认是 `0` 和 `3`：**根本不发保活包**；`tcpkeepalive yes` 救不了 |
| 建 TCP 阶段的超时 | `-o ConnectTimeout=10` | 只管到「连上」，认证和之后的卡它不管 |
| git 用的是哪个 ssh | `which -a ssh`；`GIT_SSH_COMMAND='ssh -vv' git ls-remote …` | argv 取证最直白；`ssh 变体 'simple' 不支持设置端口` 是 **git** 说的 |
| 让 git 用带端口的 ssh:// | `GIT_SSH_VARIANT=ssh` | 或者退回 scp 式地址 `git@host:path` |
| 服务端并发/限流模板 | `grep -E '^#?MaxStartups' /etc/ssh/sshd_config` | 本机读到 `#MaxStartups 10:30:100`；**效果本机未验证**（不动 sshd） |

---

## 这章的坑（20 条，除了明写「未验证」的，全部本机复现或二进制取证）

1. **`Connection refused` 和 `Connection timed out` 的排查方向是相反的。**
   refused = 包到对面了、只是没人听（查端口/IP/sshd）；timed out = 包半路被黑洞（查防火墙/安全组/路由）。
   把 timed out 当成「sshd 没起」去重启服务，是本章最常见的白工。
2. **`Permission denied (publickey)` 有两种完全相反的根因**，只看最后那句会打成平手。
   分界线是 `-vv` 里有没有 `Offering public key:`：有 = 递了牌被拒（key 不对 / 服务端不认）；
   没有 = **压根没递牌**（agent 空、path 不匹配、`IdentityFile` 指错、私钥读不出来）。
3. **`Too many authentication failures` 是「牌太多」不是「密码错」**。服务端 `MaxAuthTries`（本机
   `/etc/ssh/sshd_config` 里是 `#MaxAuthTries 6`）数的是**尝试次数**，agent 一有 8 把 key 就死在
   第 7 把之前。这时候改密码、重新生成 key 都没用，加 `-o IdentitiesOnly=yes -i <那把>` 才有用。
4. **本机 sshd 的限流会插造报错**。demo 里三条不同场景跑出过 `kex_exchange_identification: read:
   Connection reset by peer` 和 `Connection closed by 127.0.0.1 port 22` —— 那不是配置问题，是连太快。
   **判定前先等 30~60 秒**，并用 `nc -vz` 交叉验证；`HAMMER=1 bash demo.sh` 是故意演这一场。
5. **`StrictHostKeyChecking=no` 不是「跳过一次确认」，是把整个机制关掉。**
   它会静默接受任意 key，中间人无从发现；需要免交互就用 `accept-new`（只免首见、变更照样拦）。
   实测四种取值只在「首见」和「变更」两种局面下有差别 —— 见实战 5 的判决表。
6. **`ask`（交互默认值）在**没有终端**的时候退化成 `Host key verification failed.`，一句不说为什么。**
   想看到 `No ED25519 host key is known for … and you have requested strict checking.`，
   得摘掉 `BatchMode` 且带真终端（附录 C 实测）；CI 里那句光秃秃的 `verification failed` 就是这个。
7. **`ssh-keygen -H` 之后你自己的 `grep known_hosts` 永远返回 0**，看起来像「指纹丢了」。
   真相是 `HashKnownHosts` 把主机名加密了，只有 `ssh-keygen -F` 认；而且它会留 `<file>.old` **明文**备份，
   做隐私的下一步是删掉那个 `.old`（或删除时加 `-H` 前先想清楚）。
8. **`ssh(1)` 对「权限太开的私钥」是静默的**（本机实测：`0644` 的 key 照样被 `Offering public key`）。
   严格检查的是 **sshd 侧**和某些 `Load key` 分支。别照着网上「客户端也会拒绝太开的 key」去排错。
9. **`invalid format` / `error in libcrypto` / `Permission denied` 三句 `Load key` 是分层的**：
   第一句 = 解析器压根认不出这是私钥文件（多半复制粘贴少了行、或本来是公钥）；
   第二句 = 认出结构但解密/解码失败（口令错、截断、KDF 不支持）；
   第三句 = **文件读不了**（权限/属主/目录不可遍历），跟 key 内容一点关系都没有。
10. **`ssh_askpass: exec(/usr/bin/ssh-askpass): No such file or directory` 不等于口令错。**
    它只说明「有人要口令但没终端可问」。要么 `-o NumberOfPasswordPrompts=0` 关掉，要么把 `SSH_ASKPASS` 指真东西。
11. **过期字符串会把你带到别的时代**：`ssh_exchange_identification`（现在叫 `kex_exchange_identification`）、
    `unsupported cache option` 这些在本机二进制里 `strings | grep -c` 命中 0。搜到教程里这句而你机器上不是这句，
    先比版本，再按你的原文重排。
12. **「半路断」和「连不上」的分界是 `Connection established.`**。这句出现过，就不是路由/防火墙的锅，
    去查服务端（sshd 活着但拒绝握手、`MaxStartups` 丢包、TCP wrapper、fail2ban）。
13. **`ssh -G` 里 grep 不到 ≠ 配置坏了**。`ConnectTimeout` 之类**没有默认值的键根本不打印**（实测 `-G` 输出里没有它）。
    要确认它生效，看 `-v` 里的 `timeout: 8000 ms remain after connect`（补跑实测）。
14. **first-match-wins 对所有来源都成立，包括 `-o`**（实测：同一个关键字写两次，第一条赢）。
    所以「`Host *` 写在最前面」会把后面所有 Host 段吃掉；`Include` 的位置决定谁赢；
    你在 shell alias 里塞的 `-o` 也可能盖掉配置文件里的期望。
15. **`SSH_OPTS` / `SSH_CONFIG_FILE` 这两个「常见教程里的环境变量」不存在**（`strings /usr/bin/ssh` 命中 0）。
    真实存在的只有 `SSH_ASKPASS`、`SSH_ASKPASS_PROMPT`、`SSH_ASKPASS_REQUIRE`、`SSH_AUTH_SOCK`、`SSH_SK_HELPER`。
    同理，改 `HOME` 也换不掉默认 key 的查找路径 —— 它按 passwd 走（实战 12 实测）。
16. **git 报的 `Permission denied (publickey)` 多半和 ssh 无关**：git 用的可能不是你那个 ssh
    （`which -a ssh` 本机有 `/usr/bin/ssh` 和 `/bin/ssh` 两条），或 `GIT_SSH_COMMAND` 让它选了
    `simple` 变体而拒绝带端口。先用 `GIT_SSH_COMMAND='ssh -vv'` 把 argv 和日志抓出来再动手。
17. **管道的退出码会骗人**（本章踩在自己 demo 上）：`ssh … | head -3` 失败时，
    `||` 那一支不触发，因为没有「[退出码 255]」行。**看输出内容判成败**，要判就 `set -o pipefail` 或 `${PIPESTATUS[0]}`。
18. **`Bad owner or permissions` 基本是服务端的话**（本机实测：世界可写的 `-F` 顶层配置照样生效）。
    但**被 `Include` 进来的文件是检查的** —— 补跑用 `chmod 666` 的被包含文件复现出
    `Bad owner or permissions on …`、退出码 255、且**一个包都没发**。`~/.ssh/config` 同属被检查类（本机未验证，
    因为要动真 `~/.ssh/`，本章红线不碰）。
19. **同一份配置在限流前后会给出不同报错**，所以「我照 README 改了配置怎么报错变了」不一定是改坏了。
    排错前先把「这台机器现在能不能握手」用 `nc -vz` + 一次 `ssh -v … true` 定住。
20. **`Warning: Permanently added …` 不是错误**，是首见记录成功的通知；CI 日志里把它当报错处理是常见误伤。
    真正要盯的是同一次输出里后面的那句判决（`Permission denied` / `Host key verification failed`）。

---

## 自检问题

1. 你看到 `ssh: connect to host 127.0.0.1 port 2222: Connection timed out`。
   这句话已经**排除**了哪些原因？你的第一条命令是什么？
   **答**：排除了密钥、known_hosts、算法、认证方式 —— 那些都得先连上才有资格报错；
   也排除了「sshd 起没起在别的端口」这类本机服务问题（refused 才是那一类）。
   第一条命令是 `nc -vz 127.0.0.1 2222`（或直接 `ss -lnt | grep ':2222 '`）：把 ssh 从等式里拿掉。

2. `-v` 尾部是 `learner@127.0.0.1: Permission denied (publickey,password).`。
   只看这一句能定论吗？该看 `-vv` 的哪一行？
   **答**：不能。分界是**有没有 `Offering public key: …`**：有 = 牌递出去被拒（服务端不认这把）；
   没有 = 牌没递出去（agent 空 / `IdentityFile` 不存在 / 私钥没读出来 / `IdentitiesOnly` 挡了）。
   两条路的修法完全相反（实战 2、实战 4）。

3. `Received disconnect from 127.0.0.1 port 22:2: …` 和
   `Authentications that can continue: publickey,password` 谁走得更远？
   **答**：后者远得多。`Authentications that can continue` 说明 TCP、KEX、主机指纹**三段全过了**，
   死在认证；`Received disconnect … :2:` 通常死于 KEX/协议阶段（对面嫌你算法或嫌你太吵），
   还没到递牌。看到 `:2:` 去改 key 是徒劳。

4. `StrictHostKeyChecking` 的 `ask` 和 `yes` 在无终端时报错信息量差在哪？`no` 的真实危险是什么？
   **答**：`yes` 会给出 `No ED25519 host key is known for 127.0.0.1 and you have requested strict checking.`
   加一句 `Host key verification failed.`；`ask` 在没有终端时只剩 `Host key verification failed.`，
   把原因吞了（附录 C 实测）。`no` 的危险不是「少一次确认」，而是**指纹变更也照样接受**，
   中间人替换 host key 时你毫无提示 —— 所以要 `accept-new`。

5. `ssh -G` 里 grep 不到你写的 `ConnectTimeout`，是配置坏了吗？怎么证明它生效？
   **答**：不是，`-G` 不打印没有默认值的键。证明要跑一次 `ssh -v … true`，
   看 `timeout: <毫秒> ms remain after connect`（本机实测 8000 ms）。
   同理 `-G` 里值的小写/`yes|true` 混写也别用字符串比较。

6. 你在容器/CI 里把 `HOME` 指到别处想换一套 key，为什么默认 key 还是没换？
   **答**：默认身份文件的查找是按 passwd 记录里的家目录走的（实战 12 实测：改 `HOME` 后
   `identityfile` 那行没变）。要换就显式 `-i` + `IdentitiesOnly=yes`，或 `-F` 一份自己的配置，
   别指望 `HOME`。

7. `Include` 放配置文件最前和最后，效果差在哪？被包含的文件权限太开会怎样？
   **答**：同一关键字**第一条赢**，所以 `Include` 放最前 = 被包含者优先，放最后 = 主文件优先
   （`-o` 也遵守同一条规则）。被 `Include` 的文件权限太开会让客户端**在发包前**退出：
   `Bad owner or permissions on …`，退出码 255；而顶层 `-F` 指定的文件本身不检查（附录 B 实测）。

8. CI 日志里同时出现 `Warning: Permanently added 'git.xxx' (ED25519) to the list of known hosts.`
   和 `Permission denied (publickey)`。先处理哪条？
   **答**：第一条不是问题（它只说明首见已记录，`accept-new` 的正常输出）；
   处理第二条。而第二条的第一手证据是**有没有 `Offering public key`**，
   其次查 agent（`ssh-add -l` 是否 `The agent has no identities.`）、再查 git 用的是哪个 ssh（坑 #16）。

9. 为什么本章有一半场景标了「本机未验证」？这样写对你排错有什么用？
   **答**：因为红线不允许——不动系统 sshd、不碰真 `~/.ssh/`、不连外机。
   标出来是告诉你**这条要到你自己的机器上验**，并且给了验证命令和期望原文；
   没标「未验证」的每一句都能在本章 demo 输出里逐字对上。

---

## 怎么跑这章的 demo

```bash
cd ssh-handbook/06-troubleshooting
bash demo.sh              # 13 个场景跑完自动删沙箱、kill 自己起的进程、四项收尾自证
bash demo.sh --keep       # 保留沙箱和进程，打印路径/PID/端口（自己记得收尾）
HAMMER=1 bash demo.sh     # 额外加演「并发打满被随机丢包」：对 localhost:22 连 14 次
```

一次真跑的规模：**938 行输出、退出码 0、13 个场景**（沙箱 `<沙箱>`，
本轮端口 `没人听=47857 队列打满=45357 假服务器=34295/33509/47481/53987`），
耗时几分钟（每节之间 `sleep 10`，故意给本机 sshd 解限流的冷却时间）。
依赖：`python3`（假服务器/满队列，纯标准库）、`nc`、`ss`、`strings`、`git`、`ssh-keygen`。

| demo 场景 | 本文实战 | 证据强度 |
|---|---|---|
| 1 连不上的四种死法 | 实战 1 | ✅ 本机 |
| 2 `-v/-vv` 三段路走一遍 | 实战 2 | ✅ |
| 3 密码认证与「不给输的机会」 | 实战 3 | ✅（①被限流插造，见原文标注） |
| 4 agent 牌太多 / `IdentitiesOnly` | 实战 4 | ✅ |
| 5 known_hosts 变更与四种取值 | 实战 5 | ✅（干净判决在附录 C） |
| 6 私钥权限/格式/口令 | 实战 6 | ✅（含纠正 demo 注释一句） |
| 7 agent 类报错 | 实战 7 | ✅（`agent refused operation` 需真 agent 锁，本机演的是可控版） |
| 8 算法谈不拢 | 实战 8 | ✅（本地解析 + 对 localhost 的真实协商失败） |
| 9 半路断线 / 四种假服务器 | 实战 9 | ✅ |
| 10 `Permanently added` 与噪音 | 实战 10 | ✅ |
| 11 git 层 | 实战 11 | ✅（argv 取证用 `bin/fake-ssh`） |
| 12 客户端配置优先级 | 实战 12 | ✅（`Include` 权限复现在附录 B） |
| 13 万能排查顺序 | 实战 13 | ✅ |
| 服务端 `MaxStartups` 效果、`AllowUsers`、`sshd` 侧日志 | 实战 9/13 | ⚠ 本机未验证（只读配置，不动服务） |

跑完你会在末尾看到这段（这是本章最该被抄进你自己脚本的东西）：

```text
[沙箱已删除] <沙箱>
[收尾自检 1] 本脚本起的进程是否都回收了：
  PID 121554 已回收
  PID 134732 已回收
  PID 134735 已回收
  PID 134748 已回收
  PID 134752 已回收
[收尾自检 2] 有没有遗留监听端口（本章用过的都应消失）：
  干净：没有遗留监听
[收尾自检 3] 有没有本脚本留下的 ssh-agent：
  干净：没有遗留 agent
[收尾自检 4] 顺便确认没有当年那种 2222 僵尸监听：
  干净
```

安全边界（脚本自己保证了这些，你读输出时可以放心，也可以照着抄成自己的排错模板）：

- **只连 `127.0.0.1`**，一个外部地址都不碰；所有「对面」都是脚本自己起的假服务器或本机已有服务。
- 脚本一开始就 `unset SSH_AUTH_SOCK SSH_AGENT_PID`，**不会把你真实 agent 里的 key 递出去**。
- 每条 `ssh` 都带 `-F /dev/null` + 沙箱里的 `UserKnownHostsFile` + `BatchMode=yes`
  + 一把不存在的 `IdentityFile` —— 所以**没有任何一条命令能登录成功**，全部是设计好的失败。
- **不写、不读 `~/.ssh/`，不改 `/etc/ssh/`，不重启/不停用 sshd 与 ssh.socket**：`/etc/ssh` 只被 `grep`/`head`
  读过；22 端口只被动观察（`ss -lnt`、`nc -vz`）。
- **私钥全程没有被 `cat` 过**，输出里出现的只有指纹、注释和路径。
- 所有后台进程 PID 先登记、`trap cleanup EXIT` 统一回收，再用 `ss -lnt` 复核端口。

本手册为了补齐三处「demo 被限流污染 / 未演」另外做了补跑（附录 A/B/C）。那些补跑用的沙箱
同样已经 `rm -rf`，端口复核过：`ss -lnt | grep -E ':(2222|47857|45357|34295|33509|47481|53987) '`
没有任何输出，`pgrep -af 'fullbacklog|fakeserver'` 也没有残留。

---

## 下一步

- **05-port-forwarding**：实战 13 那条「先 `ss -lnt` 看门在不在」的顺序就是从 05 章「隧道不通」
  的排错里抽出来的通用版；反过来，05 章的 `bind: Address already in use` /
  `remote port forwarding failed` 用本章的三段路模型读一遍就清楚了。
- **04-agent-and-multiplexing**：本章 agent 类报错（`The agent has no identities.` /
  `agent refused operation` / 牌太多）的正主；`ControlMaster` 复用一条会话还能顺手
  把 `MaxStartups` 那类并发限流降下来。
- **03-config-file**：`ssh -G` 只是「读」，03 章教你「写」：Host 段顺序、`Include` 位置、
  两段式 `IdentityFile` + `IdentitiesOnly`。本章的坑 #14/#15/#18 全部以 03 章的规则为根据。
- **01-how-it-works**：三段路（TCP → KEX+指纹 → 认证）的**原理版**在那里；
  想知道 `-vvv` 里那几十个包为什么按那个顺序飞（`send packet: type 20` 之后发生什么），回到 01 章。
- **一句话带走**：出错了先花 20 秒确定「死在第几段」，再动手。
  改 key、改配置、重启服务、换网络 —— 这四件事分别属于四个不同的段，
  **一次只可能有一件是对的**，而 `ssh -v` 的最后 20 行已经告诉你是哪件了。
