# 这章：端口转发 —— `-L` / `-R` / `-D` 到底把谁的哪个端口搬给了谁

> **一句话：端口转发不是魔法，就是「在你已经登录成功的那条加密连接上，再跑一个中转进程」。
> 这章要解决的是三个问题：门开在**谁**的机器上、冒号后面的地址由**谁**解析、门没开成功时**为什么没人告诉你**。**

本章所有输出都是**本机真跑出来的**（OpenSSH_10.2p1 Ubuntu-2ubuntu3.6，服务端 `127.0.0.1:22` 是真实的系统 sshd）。
完整落盘输出：`~/.cache/handbook-captures/ssh05.txt`（476 行，一条命令就能重生成，见文末「怎么跑这章的 demo」；
退出码 0，那次的沙箱是 `<沙箱>`，跑完自动删除）。**本章 10 个场景，每一处标 ✅/🔧 的输出都能在这份捕获里逐行找到**
—— 以前散在单独文件里的「补充实测」，现在全部并进了 `demo.sh` 的场景 10，
不再有「demo 之外另跑一份、读者无法复现」这种证据。

**引用约定**（本章所有围栏都按它写）：

- 围栏里 `$ ` 开头的行是**终端原文**；四空格缩进的是我手画的说明，不是跑出来的。
- `<沙箱>` = 那次 `mktemp -d` 出来的 `/tmp/ssh05-XXXXXX`，你重跑一定是别的名字。
- `<本机用户>` / `<家目录>` / `<主机名>` 是**脱敏占位符**：这几处是 ssh / shell 从本机展开出来的真实身份
  （本章就一处 —— `ls -l` 那个 socket 的属主/属组列），为了不印进书里统一替换成占位符；
  你重跑会看到你自己的名字。
- 远端用户名统一写 `learner`：本章每一个连接都按设计停在认证（`Permission denied (publickey,password).`），
  用一个本机不存在的用户名和一个真实登录名得到的报错、退出码、结论**完全一样**，
  而文档里因此不会印出跑脚本那个人的真实登录名（全书红线「屏幕上不出现真实身份」）。
- `[退出码 N —— 非 0 是预期的]`、`（这条按设计会失败…）` 是 demo 自己打的注解，不是 shell 的输出。
- 端口、PID、inode、时间戳每次跑都不同 —— **别抄数字，抄结构**。

⚠ **先说清楚本章的证据边界**（这条比内容重要，别跳过）：

本机按红线**不允许登录成功**（不装 `authorized_keys`、不猜密码），而 **ssh 的本地门是在认证成功之后才建的**。
所以本章的证明方式是：**用一个真跑的中转进程（`relay.py`），把 `ssh -L` 的机制在你眼前完整复现一遍**
—— 监听、搬字节、换上游、绑 0.0.0.0、端口被占、进程死了之后客户端看到什么，全是真的；
唯一没有真的是「那条加密连接本身」。每个代码块都会标是哪一种：

| 标记 | 含义 |
|---|---|
| ✅ **本机实跑** | 这段输出就是这台机器上真发生的，可在 `~/.cache/handbook-captures/ssh05.txt` 里逐行找到 |
| 🔧 **本机实跑到边界** | 真跑了 `ssh`，但停在认证那一步（看到的是认证报错，不是隧道效果） |
| ⚠ **需你自己的服务器验证** | 只给命令模板和结构，**没有编造任何成功输出** |

**实验红线**（demo 全程遵守，你自己玩隧道也照这个抄）：

- 只在 `127.0.0.1` / `localhost` / `0.0.0.0` 的本机监听上做实验，**绝不连任何外部主机**。
- 每条 `ssh` 都带 `-F /dev/null`（不读你真实 config）、`-o BatchMode=yes`（绝不弹口令）、
  `-o PreferredAuthentications=publickey`（绝不试密码）、沙箱 `-o UserKnownHostsFile=`、
  以及一个不存在的 `-o IdentityFile=` 当挡箭牌（免得 ssh 去探测你真实的 `~/.ssh/id_*`）。
- 每个后台进程（`http.server`、`relay.py`、`fake_socks5.py`）都登记 PID，`trap` 里统一 kill，
  退出前用 `ss -lnt` 三连自证没有遗留监听（本章历史上曾在 2222 端口留下过没关的进程）。
- 不重启/不停止系统 sshd，22 端口只做被动观察。

---

## 你会遇到的场景

| # | 场景 | 学完这章你能自己解释 |
|---|---|---|
| 1 | 「服务器只开了 22，我想看它内网那台机器的管理页面」 | `-L` 的两个地址分别属于谁（demo 场景 1–3） |
| 2 | `-L 8888:localhost:80` 里的 `localhost` 是我自己的吗 | **不是**，是服务器自己的 —— 本章唯一的难点，现场演给你看 |
| 3 | `ssh -L` 报 `bind [127.0.0.1]:8080: Address already in use` | 端口被占的真实成因与原文（demo 场景 5） |
| 4 | 隧道悄悄断了，我的程序卡在连接错误 | `ExitOnForwardFailure` / `ServerAliveInterval`（默认值本机实测） |
| 5 | `-g` 和 `GatewayPorts` 是不是一回事 | 一个是客户端绑 0.0.0.0，一个是服务端允许远端门被别人连 |
| 6 | 家里的机器在 NAT 后面，我要连它上面的服务 | `-R`：门开在**服务器**身上，方向反过来 |
| 7 | 想让浏览器整个走隧道，不想一个服务开一个门 | `-D` 动态 SOCKS 代理 |
| 8 | `-D` 之后 DNS 泄漏、或者「明明开了隧道还是解析失败」 | `--socks5` vs `--socks5-hostname`，代理日志里看得一清二楚（demo 场景 8） |
| 9 | 走隧道连不上本机的 `127.0.0.1` 服务，却连得上 `localhost` | `localhost` 被本地解析成 `::1`（demo 场景 8 铁证） |
| 10 | 想转 docker 的 socket（不是 TCP 端口） | unix socket 形态的门（demo 场景 6） |
| 11 | `-N`、`-f` 到底干什么，为什么不加会怎样 | 不建 shell、后台化；配合 `ExitOnForwardFailure` 才安全 |
| 12 | 服务端禁了转发，我怎么自查 | `AllowTcpForwarding` / `PermitOpen` / `GatewayPorts`（⚠ 需服务器验证） |

---

## 心智模型

### 第一块：门开在谁身上 —— 本章最重要的一张图

三种转发用的是**同一条已经建好的 SSH 连接**，区别只有一个：**监听（listen）发生在哪台机器上**。

```
                     你（笔记本）                        中转/服务器                    内网目标
                 192.168.130.130                    能看见内网目标                  127.0.0.1:47305
                 127.0.0.1                          的机器

  ┌─────────────────────────────────────────────────────────────────────────────────────────────┐
  │  -L 8080:127.0.0.1:47305   「本地转发」：门开在【你】身上，你去打远端的目标                    │
  │                                                                                               │
  │   你的程序 ──TCP──> 【听 127.0.0.1:8080】═══已有加密连接═══> 服务器 ──TCP──> 目标 47305        │
  │   (curl/浏览器)        门在客户端                            ▲                                │
  │                        ssh 进程里建的 listen                  │                                │
  │                                          「连目标」这一步发生在【服务器那侧】★                 │
  │                                          所以只有服务器能到达的目标，你也能碰到                │
  ├──────────────────────────────────────────────────────────────────────────────────────────────┤
  │  -R 9000:localhost:3000    「远端转发」：门开在【服务器】身上，别人/服务器打进来，流向你         │
  │                                                                                               │
  │   服务器上的程序 / 别的机器 ──TCP──> 【听 0.0.0.0:9000 或 127.0.0.1:9000】                     │
  │                                          ═══同一条加密连接，反着用═══> 你的机器 ──TCP──> 3000   │
  │                                          ▲                                                   │
  │              门是服务器建的，所以要经服务端 GatewayPorts / AllowTcpForwarding 点头             │
  ├──────────────────────────────────────────────────────────────────────────────────────────────┤
  │  -D 1080                  「动态转发」：门开在【你】身上，但目标不写死，每次由客户端现报         │
  │                                                                                               │
  │   浏览器 ──SOCKS5──> 【听 127.0.0.1:1080】═══加密连接═══> 服务器 ──TCP──> 任意目标             │
  │                        ▲                                                                      │
  │                        冒号右边什么都没有：目标地址在 SOCKS 请求里带过来（所以叫「动态」）      │
  │                        DNS 在哪一侧解析，取决于客户端把「域名」还是「IP」交给代理 ★             │
  └──────────────────────────────────────────────────────────────────────────────────────────────┘
```

**记忆法（可以背）**：

- **冒号左边 = 「听的地方」，冒号右边 = 「要去的地方」。**
- 听的地方在哪台机器，由**开关**决定：`-L`/`-D` 在客户端，`-R` 在服务器。
- 要去的地方，由**听的那台机器**去解析和连接 —— 于是 `-L 8080:localhost:80` 里的 `localhost`
  就是**服务器的 localhost**；`-R 9000:localhost:3000` 里的 `localhost` 才是**你的**。
  这一个是本章最难、也最容易在半年后救你命的一点。
- 方向口诀：`-L` = 我要**出去**（本地开门打远端）；`-R` = 别人**进来**（远端开门打本地）；
  `-D` = 我要出去且**目的地不固定**。

### 第二块：全景表（把上面的图压缩成一张可以贴在工位上的表）

| | `-L` 本地转发 | `-R` 远端转发 | `-D` 动态转发 |
|---|---|---|---|
| 监听开在 | **你的机器** | **服务器** | **你的机器** |
| 语法 | `-L [绑:门:]目标主机:目标端口` | `-R [绑:]门[:目标主机:目标端口]` | `-D [绑:]门` |
| 只给一个端口行不行 | **不行**，实测 `Bad local forwarding specification '8080'` | **行**，实测 `-R 80` 通过解析（含义：连回你本机的 80） | 不行（`-D 0` 也报 Bad） |
| 冒号右边的地址谁解析 | **服务器** | **你的机器** | 每次请求由客户端给，**由代理侧或客户端侧解析，看写法** |
| 谁能连这个门（默认） | 只有本进程/本机（`127.0.0.1`） | 服务器默认只绑 `127.0.0.1`（`GatewayPorts no`） | 只有本机（`127.0.0.1`） |
| 要开放给同网段 | 加 `-g` 或显式写 `0.0.0.0:` | 服务端 `GatewayPorts clientspecified\|yes` | 加 `-g` 或写 `0.0.0.0:` |
| 服务端总开关 | `AllowTcpForwarding`（`local` 管 -L） | `AllowTcpForwarding`（`remote` 管 -R） | 同 `-L`（本质是 SOCKS 化的 -L） |
| 目标能限定吗 | `PermitOpen`（服务端） | — | `PermitOpen` |
| 典型用途 | 访问内网 DB/管理页/本机 dev server | NAT 后面被反向连、临时演示、内网穿透 | 浏览器全套流量走隧道、绕过本机 DNS |
| 一句话风险 | 你把内网服务的一小块公开在自己机器上 | 你在**别人的机器**上开了一个口 | 你把服务器当成匿名出口，DNS 还可能外泄 |

（⚠ 表里凡带「服务端」的三行，本机都查不到实际值，需要 `sshd -T` 在自己的服务器上验证 —— 见实战 10。）

### 第三块：一条转发真正做的事 = 本地 listen + 双向搬字节

这一节是 demo 场景 1–2 的成果：**`ssh -L` 帮你写的就是一个中转进程**，本机用 30 行 Python 就能等价复现。
它的生命周期四步：① 在本地门 `bind()/listen()`（失败就是 `EADDRINUSE`，实战 5）→
② `accept()` 后顺着已加密的 SSH 连接发 channel → ③ **服务器侧代表你去 `connect(目标)`**
（★ 目标地址在服务器那侧被解析，实战 3）→ ④ 两头 pump 字节，一条 TCP 会话对应一个 SSH channel。
demo 里的 `relay.py` 干的正是 ①②④（把 ③ 换成「本机连本机」，所以整条链路都能真跑）。

### 第四块：三层「谁能决定这个门开不开」

```
   第 1 层  客户端参数解析        语法/端口范围            ✅ 本机实测（Bad ... specification / 99999 越界）
            └ 破了：进程连启动都启动不了，报错非常响
   第 2 层  本地 bind()           端口被占 / 权限 <1024    ✅ 本机实测（Errno 98 + 二进制里的 bind [%s]:%s 模板）
            └ 破了：⚠ 默认 ExitOnForwardFailure=no，连接照样「成功」，你毫无察觉（实战 7）
   第 3 层  服务端 sshd 点头      AllowTcpForwarding /     ⚠ 本机查不到，需服务器验证（实战 10）
            └ PermitOpen / GatewayPorts —— 破了就得到
              「Warning: remote port forwarding failed for listen port …」
```

第 2 层和第 3 层的差别就是本章最常见的事故：**「门开了」和「门有用」是两件事**。
`ss -lnt` 只能证明第一件；第二件要靠真发一个请求（实战 1）或看服务器侧文案（实战 10）。

## 场景逐个实战

下面每一节都标了对应的 **demo 场景 N**，贴的都是那一次真跑的原文（那次的沙箱就是 `<沙箱>`）。
**端口是脚本每次跑向内核现要的（`socket.bind(("127.0.0.1", 0))`），所以你跑出来的一定是别的数字
—— 别抄端口，抄结构。** 本轮：目标 A=47305、目标 B=55777（都是 `python3 -m http.server`，
扮演「内网里那台你连不到的机器」）、门=37575（等价 `ssh -L 37575:…` 的本地门）、
另一个门=59365（换上游用）、被占的门=51927（绑 `0.0.0.0` 用）、SOCKS=46393（`-D` 的门）。

---

### 实战 1：先把「内网那台机器」摆出来（demo 场景 1，✅ 本机实跑）

**想干什么**：造一个只监听 `127.0.0.1` 的服务，当作内网数据库/内部 API；后面所有「门」都往它转发。

**敲什么**

```bash
python3 -m http.server 47305 --bind 127.0.0.1 --directory <沙箱>/site &
ss -lnt | grep ':47305 '
curl -s --max-time 5 http://127.0.0.1:47305/hello.txt
```

**真实输出**

```text
$ python3 -m http.server 47305 --bind 127.0.0.1 --directory <沙箱>/site &        # 后台起【targetA】
  [PID 167637 已登记，脚本退出时会自动 kill]

--- 它只绑 127.0.0.1，所以 ss 里看到的是这一行：

$ ss -lnt | grep ':47305 ' || echo '  (没命中 = 它没在监听)'
LISTEN 0      5          127.0.0.1:47305      0.0.0.0:*   

--- 基准：直接访问它，等下好对比

$ curl -s --max-time 5 http://127.0.0.1:47305/hello.txt
你好，我是【目标服务 A】，监听在 127.0.0.1:47305（把它想成内网数据库/内部 API）
```

**逐行解读**

- `--bind 127.0.0.1` 就是「服务只在本机回环上开放」这件事的物证：`ss` 那一行的本地地址列写的是
  `127.0.0.1:47305`，而不是 `0.0.0.0:47305`。这一行是本章后面所有对比的**基准**。
- 先把「不经门直连」的输出记下来（`你好，我是【目标服务 A】…`）。**后面每一次穿门访问都拿它来对比** ——
  转发这件事唯一的可观测证据就是「我从 A 端口拿到了 B 的内容」。没有基准，你分不清「门错了」和
  「目标本来就长这样」。

---

### 实战 2：人肉版 `ssh -L` —— 门开在本地，字节搬到目标（demo 场景 2，✅ 本机实跑）

**想干什么**：在本地再开一个端口（37575），访问它却拿到 47305 的内容 —— 这就是 `-L` 的全部效果。

**敲什么**

```bash
python3 relay.py 127.0.0.1 37575 127.0.0.1 47305 relay.log &   # 人肉版 ssh -L
curl -s --max-time 5 http://127.0.0.1:37575/hello.txt
ss -lnt | grep -E ':(47305|37575) '
cat relay.log
```

**真实输出**

```text
$ python3 <沙箱>/relay.py 127.0.0.1 37575 127.0.0.1 47305 <沙箱>/relay.log &        # 后台起【relay_door】
  [PID 167673 已登记，脚本退出时会自动 kill]

--- 现在 curl 那个门（37575），拿回来的却是目标（47305）的内容：

$ curl -s --max-time 5 http://127.0.0.1:37575/hello.txt
你好，我是【目标服务 A】，监听在 127.0.0.1:47305（把它想成内网数据库/内部 API）

--- ss 一眼看清：两个端口都在监听，一个是目标、一个是门

$ ss -lnt | grep -E ':(47305|37575) '
LISTEN 0      5          127.0.0.1:37575      0.0.0.0:*   
LISTEN 0      5          127.0.0.1:47305      0.0.0.0:*   

--- 门自己的日志（铁证：是它在替你搬字节，还告诉你它连的是谁）

$ cat <沙箱>/relay.log
[relay 12:47:59] 监听 127.0.0.1:37575 -> 上游 127.0.0.1:47305
[relay 12:48:00]   ('127.0.0.1', 44266): 已连上上游 (127.0.0.1,47305)
[relay 12:48:00]   上游->本地 转发 295 字节后关闭
[relay 12:48:00]   本地->上游 转发 88 字节后关闭
```

**逐行解读**

- **两个 LISTEN 行**：`37575` 和 `47305` 都在听。你的程序连的是前者，服务在后者。
  「隧道存在」这件事在 `ss -lnt` 里就是这一行，**这也是你以后判断隧道有没有建起来的第一招**。
- `('127.0.0.1', 45406)` —— 门这边进来的连接，源端口是内核临时分配的 **45406**（客户端随机端口），
  它**不是**任何监听端口，别去 `ss -lnt` 里找它（`-l` 只列监听）。要看这种连接用 `ss -tnp`。
- `已连上上游 (127.0.0.1,47305)` —— 门自己去连了目标。**这一行等价于 ssh 在服务器侧做的 connect()**，
  也就是「第二个地址由谁解析」这个难点的现场（实战 3 放大它）。
- `本地->上游 转发 88 字节` / `上游->本地 转发 295 字节` —— 两个方向各一条。
  88 字节是那个 `GET /hello.txt HTTP/1.1 …` 请求头，295 字节是响应。**转发的本质就是这四行。**

**等价写法（⚠ 需你自己的服务器验证）**

```text
ssh -N -L 37575:127.0.0.1:47305 用户@服务器
curl -s http://127.0.0.1:37575/hello.txt
```

真实场景里唯一的区别：**「连目标 47305」这一步发生在服务器那侧**。
所以 47305 可以是服务器内网里任何一台机器的任何端口，你的防火墙根本不知道它存在。
demo 的收尾说明原话就是这件事：

```text
  ↑ 把上面这套东西换个壳子，就是： ssh -N -L 37575:127.0.0.1:47305 用户@服务器
    ssh 帮你写的就是这个『本地 listen + 顺着已有加密连接搬字节』的中转进程，
    区别只在于：真实场景里『连目标』这一步发生在【服务器那侧】，所以能穿过防火墙。
```

---

### 实战 3：门后面接谁是配置决定的 —— `-L` 的第二个地址由**服务器**解析（demo 场景 3，✅ 本机实跑）

**这一节是本章的难点，也是全章最值钱的四条输出。** 你要看清的是：**客户端一个字都没改，内容全换了。**

**① 同一扇门，换上游**

```text
$ python3 <沙箱>/relay.py 127.0.0.1 37575 127.0.0.1 55777 <沙箱>/relay2.log &        # 后台起【relay_door2】
  [PID 167687 已登记，脚本退出时会自动 kill]

$ curl -s --max-time 5 http://127.0.0.1:37575/hello.txt
你好，我是【目标服务 B】，监听在 127.0.0.1:55777（另一个内部服务）
```

**② 门接向本机的 22（sshd 自己）**

```text
$ python3 <沙箱>/relay.py 127.0.0.1 59365 127.0.0.1 22 <沙箱>/relay_ssh.log &        # 后台起【relay_ssh】
  [PID 167694 已登记，脚本退出时会自动 kill]

$ timeout 3 nc 127.0.0.1 59365 </dev/null | head -c 40; echo
SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
```

**③ 门接向本机的 631（打印服务 cups）**

```text
$ python3 <沙箱>/relay.py 127.0.0.1 59365 127.0.0.1 631 <沙箱>/relay_cups.log &        # 后台起【relay_cups】
  [PID 167717 已登记，脚本退出时会自动 kill]

$ printf 'GET / HTTP/1.0\r\n\r\n' | timeout 3 nc 127.0.0.1 59365 | head -2
HTTP/1.0 400 无效请求
Content-Language: zh_CN
```

**逐行解读**

- 三个例子用的是**同一个门**（37575 / 59365），客户端连门的方式一模一样（`curl 127.0.0.1:门`），
  拿回来的却是三种完全不同的东西：目标 B 的页面、sshd 的版本横幅、cups 的 400 响应。
- 结论只有一个变量：**「门后面接谁」是中转点上的配置决定的，不是客户端决定的。**
- ②那条 `SSH-2.0-OpenSSH_10.2p1` 尤其重要：它证明门**不认识协议**，它只是搬字节。
  所以「我以为我转的是 HTTP」这种直觉没有任何约束力 —— 门可以指向任何 TCP 服务，包括 sshd 自己。

demo 自己给的原话（就是本章那句结论）：

```text
  ↑ 结论（本章唯一的难点）：-L 里冒号后面那个地址，是【中转点那侧】的地址。
    所以 -L 8080:localhost:80 里的 localhost = 服务器自己的 localhost，不是你的。
```

**所以你会撞上的三种「明明配错了却没报错」**

| 你写的 | 你以为是 | 实际是 | 后果 |
|---|---|---|---|
| `-L 8080:localhost:80` | 我本机的 80 | **服务器**的 80 | 门开了，`curl` 拿到的是服务器的 web（或 404/拒绝） |
| `-L 8080:localhost:80`（服务器上没跑 web） | 同上 | 同上 | 门**照样开成功**，`curl` 才报 `Connection refused` —— 见实战 5 |
| `-L 8080:127.0.0.1:3306` | 我本机有 MySQL | **服务器**的 3306 | 连上了陌生库，或者 refused |
| `-L 8080:10.0.0.9:3306` | 需要我能解析/路由到 10.0.0.9 | 由**服务器**解析和路由 | 只要服务器能到，你就能到 —— 这正是隧道的价值 |

**为什么这么设计**：中转点必须知道往哪送；而「往哪送」的信息写在服务器上才有穿透防火墙的能力。
记法：**冒号右边那串，是要在服务器上跑的 `nc <host> <port>`，不是在你机器上。**

---

### 实战 4：绑定地址 —— 为什么 ssh 默认只听 `127.0.0.1`（demo 场景 4，✅ 本机实跑）

**想干什么**：并排看两种门 —— 只绑回环（ssh 的默认）和绑 `0.0.0.0`（等价于 `-g`）。

```text
$ python3 <沙箱>/relay.py 0.0.0.0 51927 127.0.0.1 47305 <沙箱>/relay_all.log &        # 后台起【relay_all】
  [PID 167726 已登记，脚本退出时会自动 kill]

--- 两种门并排看：左边只绑回环（ssh 默认），右边绑 0.0.0.0（等价于加了 -g）

$ ss -lnt | grep -E ':(37575|51927) '
LISTEN 0      5            0.0.0.0:51927      0.0.0.0:*   
LISTEN 0      5          127.0.0.1:37575      0.0.0.0:*   

$ ip -4 -o addr show | awk '{print "  本机地址:", $4}' | head -6 || true
  本机地址: 127.0.0.1/8
  本机地址: 192.168.130.130/24
```

**逐行解读**

- 两行的差别**只在本地地址列**：`127.0.0.1:37575` vs `0.0.0.0:51927`。
- 第二行下面紧跟着 `本机地址: 192.168.130.130/24` —— 这不是凑数的：**`0.0.0.0` 的门会出现在这个
  网段的每一次 ARP 里**。同一 WiFi 上的任何人都能 `curl 192.168.130.130:51927`，
  然后拿到内网目标 A 的内容。你辛苦转出来的那小块内网服务，就这样公开了。
- 所以 `ssh` 默认**只绑 127.0.0.1** 是刻意保护你。要放开只有两种写法：

```text
ssh -g -L 51927:127.0.0.1:47305 用户@服务器          # 所有 -L/-D 门都绑到非回环
ssh -N -L 0.0.0.0:51927:127.0.0.1:47305 用户@服务器   # 只放开这一个门（更推荐）
```

- `-g` 的完整语义（⚠ 效果需服务器验证，本机无法建门）：它等价于给所有本地/动态转发的门都
  允许绑到非回环地址上。demo 的原话是「ssh 要 `-g` 才肯这么绑，而且服务端还有 `GatewayPorts`
  管着远端那一侧」—— 这一句里的两个东西千万别混为一谈，见下表。

**`-g`（客户端）和 `GatewayPorts`（服务端）不是一回事**，这是本章第二容易混的点：

| | 管什么 | 在哪一侧 | 默认 |
|---|---|---|---|
| `-g` / 显式写 `0.0.0.0:` | 你本地的 `-L`/`-D` 门绑哪个地址 | 客户端 | 只绑 `127.0.0.1` |
| `GatewayPorts` | 你 `-R` 请求的门**在服务器上**能不能被别人连 | 服务端 sshd | `no`（只绑服务器回环） |

`GatewayPorts` 的客户端侧值可以在本机看到（✅ 实测）：

```text
$ ssh -F /dev/null -G learner@127.0.0.1 | grep -iE "forward|clearall|gateway|tunnel"
clearallforwardings no
exitonforwardfailure no
forwardx11 no
forwardx11trusted yes
gatewayports no
tunnel false
forwardx11timeout 1200
forwardagent no
tunneldevice any:any
```

（⚠ 一句老实话免得你误读：**客户端也认 `gatewayports` 这个键**，`-G` 才会把它 dump 出来；
但真正决定「远端门能不能被同网段连走」的是**服务器 `sshd_config` 里的 GatewayPorts**，
那个值本机查不到，只能在服务器上 `sshd -T | grep -i gatewayports`。别把 `-G` 里那行当服务端答案。）

---

### 实战 5：端口被占 —— 从内核报错到 ssh 报错原文（demo 场景 5，✅/🔧）

**想干什么**：让第二个门去抢已经在监听的 37575，看真实报错长什么样。

```text
$ python3 <沙箱>/relay.py 127.0.0.1 37575 127.0.0.1 47305 /dev/null 2>&1 | tail -2
  （这条按设计会失败，就为了看真实报错原文）
    ~~~~~~^^^^^^^^^^^^^^^^^^^
OSError: [Errno 98] Address already in use
```

（这里没有「退出码」那行是正常的：demo 用 `bash -c "… | tail -2"` 起子 shell 执行，
`set -o pipefail` 不会传进子 shell，那条管道的退出码是 `tail` 的 0，所以 `note` 的补提示分支没触发。
**看错误要看内容，别看退出码** —— 坑 #10。）

**逐行解读**：内核给的 errno 是 **98 = EADDRINUSE**，中文语境里你会看到的三种文案是同一件事：

| 你会看到 | 出自 |
|---|---|
| `[Errno 98] Address already in use` | Python（本节实测） |
| `bind [127.0.0.1]:37575: Address already in use` | **ssh 客户端**（模板已在本机二进制里查到，见下） |
| `Error: remote port forwarding failed for listen port 37575` | `-R` 时**服务器**回给你的（模板同样在二进制里） |

ssh 的文案模板本机查得到（✅ 实测，`strings` 翻二进制）：

```text
$ strings /usr/bin/ssh | grep -E '^bind \[%s\]:%s' | head -2 || echo '  (这条模板没找到)'
bind [%s]:%s: %.100s
```

同一段里还能翻出这些（✅ 实测原文，都是运行时真会打印的格式串；完整列表在 demo 场景 10 ④，
那一节把 `strings /usr/bin/ssh` 里所有带 `%s`/`%d`/`%u` 的转发文案全打了一遍）：

```text
Local connections to %.200s:%d forwarded to remote address %.200s:%d
Remote connections from %.200s:%d forwarded to local address %.200s:%d
Allocated port %u for remote forward to %s:%d
Error: remote port forwarding failed for listen port %d
Warning: remote port forwarding failed for listen port %d
Bad local forwarding specification '%s'
```

**读法**：`Local connections to …forwarded to remote address …` 和
`Remote connections from …forwarded to local address …` 这两条是 ssh 描述**方向**的原话
（本机没建过真门，所以没抓到它们的实际输出 —— 属于「文案已在二进制里查到、语义按 ssh(1) 推断」，
标记 ⚠）；`Allocated port %u for remote forward to …` 对应 `-R …:0` 让服务器随便分配端口的场景，
它会打印实际分配值；`Error:` 与 `Warning:` 那两行是 `-R` 建门失败的两种语气 ——
**Error 会终止进程，Warning 不会，所以千万别只按「有没有报错」判断成败**（坑 #6）。

**🔧 本机实跑到边界：为什么本章不能直接演 ssh 报 bind 失败**

```text
$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh -o IdentityFile=<沙箱>/keys/_no-default-probe -o ConnectTimeout=8 -N -L 37575:127.0.0.1:47305 learner@127.0.0.1 2>&1 | tail -2 || true
no such identity: <沙箱>/keys/_no-default-probe: No such file or directory
learner@127.0.0.1: Permission denied (publickey,password).
  ↑ 看到了吗：ssh 先死在认证上，压根走不到建门那一步。这就是为什么本章隧道效果需服务器验证。
```

**这条边界本身就是知识点**（02 章实战 5 的「钥匙出场顺序」在这里的镜像）：
**本地门是认证成功之后才建的**。所以：

- 你 `-L` 时先看到认证类报错 —— 别去查端口占用，先修认证。
- 认证过了才轮到 `bind […]:…: Address already in use`。
- 反过来，`-R` 的门是**服务器**建的，所以要认证成功 + 服务端 `GatewayPorts`/`AllowTcpForwarding` 都点头。

**怎么查「谁占了我的门」**（demo 场景 10 ③，本机非 root 真跑）：

```text
$ ss -lntp | grep -E ':(631|47305) '
LISTEN 0      4096       127.0.0.1:631        0.0.0.0:*                                        
LISTEN 0      5          127.0.0.1:47305      0.0.0.0:*    users:(("python3",pid=167637,fd=23))
LISTEN 0      4096           [::1]:631           [::]:*                                        

$ fuser 631/tcp; echo exit=$?
exit=1

$ ss -lntp | grep ':22 ' || echo '  (22 这一行没找到)'
LISTEN 0      4096         0.0.0.0:22         0.0.0.0:*                                        
LISTEN 0      4096            [::]:22            [::]:*                                        
```

**这里有个容易误判的点**：`-p` 那一列**只对你 OWN 的进程有内容**。上面第 1、3 行（cups 的 631）
Process 列**直接空着**，不是「没人监听」；只有第 2 行 —— 本章 demo 自己起的目标 A
（`python3 -m http.server 47305`）—— 才带出 `users:(("python3",pid=167637,…))`。
同理非 root 的 `fuser 631/tcp` **什么都不打印、退出码 1**（`exit=1` 就是它打的），
这不代表端口空闲。22 那一行也一样：`ss -lntp | grep ':22 '` 找得到 `LISTEN`，但没有 Process 列
—— 因为 sshd 属于 root。所以查占用的正确顺序是：先用 `ss -lnt`（不带 p，两列地址看仔细）确认**有没有**这一行，
再决定要不要 `sudo ss -lntp` / `sudo fuser -v 37575/tcp` 去问**是谁**。

**为什么默认端口会撞**：`-L 8080:…` 这种习惯写法最容易和
本地 dev server（8080/3000/5432）、别的隧道的遗留进程撞。

**换端口的办法只有「自己挑一个」—— 别指望让 ssh 随便分配**（本机实测这条不对称，
原文见 demo 场景 10 ① 的 `-L 127.0.0.1:0:…` / `-L :0:…` 两条）：

```text
$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh -o IdentityFile=<沙箱>/keys/_no-default-probe -o ConnectTimeout=8 -L 127.0.0.1:0:127.0.0.1:47305 learner@127.0.0.1
  （这条按设计会失败，就为了看真实报错原文）
Bad local forwarding specification '127.0.0.1:0:127.0.0.1:47305'
  [退出码 255 —— 非 0 是预期的]

$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh -o IdentityFile=<沙箱>/keys/_no-default-probe -o ConnectTimeout=8 -L :0:127.0.0.1:47305 learner@127.0.0.1
  （这条按设计会失败，就为了看真实报错原文）
Bad local forwarding specification ':0:127.0.0.1:47305'
  [退出码 255 —— 非 0 是预期的]
```

**`-L` 的门禁端口写 `0` 直接判语法错**（本地门不能交给系统分配）；
但 **`-R` 的远端端口写 `0` 能过解析**（实测 `-R 0:localhost:3000` 走到了认证，二进制里也有
`Allocated port %u for remote forward to %s:%d`）—— 「让服务器随便分配端口」是 `-R` 独有的能力，
⚠ 实际分配值要到你自己的服务器上看。

---

### 实战 6：unix socket 形态的门（demo 场景 6，✅ 本机实跑）

**想干什么**：门不一定是 TCP 端口，也可以是 socket 文件 —— 这是转发 docker socket 的原理。

```text
$ python3 <沙箱>/relay.py unix <沙箱>/tun.sock 127.0.0.1 47305 <沙箱>/relay_unix.log &        # 后台起【relay_unix】
  [PID 167748 已登记，脚本退出时会自动 kill]

--- socket 文件长这样（第一位 s = socket）：

$ ls -l <沙箱>/tun.sock
srwxrwxr-x 1 <本机用户> <本机用户> 0 Oct  5 12:48 <沙箱>/tun.sock

$ ss -xlp 2>/dev/null | grep tun.sock | head -2 || echo '  (没命中)'
u_str LISTEN 0      5                            <沙箱>/tun.sock 384922            * 0    users:(("python3",pid=167748,fd=24))                                 

--- 用 curl --unix-socket 走它，拿到的还是目标 A 的内容：

$ curl -s --max-time 5 --unix-socket <沙箱>/tun.sock http://localhost/hello.txt
你好，我是【目标服务 A】，监听在 127.0.0.1:47305（把它想成内网数据库/内部 API）
```

**逐行解读**

- `srwxrwxr-x` 第一个字母 `s` = socket 文件；`0` 字节是正常的（socket 不存内容）。
  ⚠ **`srwxrwxr-x` 是个坏权限**：同组/其他人都能连这个 socket。docker 官方 socket 是 `srw-rw---- root:docker`。
  你把转出来的 socket 放在共享目录（`/tmp`）时，等于又开了一次「绑定地址」的风险 —— 收尾删掉或收紧权限。
- `ss -xlp`：`-x` = unix socket，`-l` = 只列监听，`-p` = 带进程。这一行同时给出
  路径、inode 号（384922）和持有它的进程（`pid=167748`）。**`ss -lnt` 里找不到它**，
  这是最常见的「我的门呢？」——它在 unix 侧不在 TCP 侧。
- `curl --unix-socket <路径> http://localhost/hello.txt`：URL 里的主机名**只是给 HTTP 头用的**，
  真正决定连到哪的是 `--unix-socket`。理解这一点才能理解为什么 docker 的 socket 转发「换名字」不影响它。

**等价 ssh 写法**（⚠ 需你自己的服务器验证）

```text
ssh -N -L /tmp/m.sock:127.0.0.1:47305 用户@服务器      # 本地门是 unix socket
ssh -N -L /tmp/r.docker.sock:/var/run/docker.sock 用户@服务器   # 更常用：把远端 docker 的 socket 拿到本地
```

本机对**语法**能给证据（✅ 实测，demo 场景 10 ① 的 `-L <路径>.sock:…` 那条，注意它顺利过了参数解析、死在认证）：

```text
$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh -o IdentityFile=<沙箱>/keys/_no-default-probe -o ConnectTimeout=8 -L <沙箱>/a.sock:127.0.0.1:80 learner@127.0.0.1
  （这条按设计会失败，就为了看真实报错原文）
Pseudo-terminal will not be allocated because stdin is not a terminal.
no such identity: <沙箱>/keys/_no-default-probe: No such file or directory
learner@127.0.0.1: Permission denied (publickey,password).
  [退出码 255 —— 非 0 是预期的]
```

⚠ **转发 docker socket 的安全真相**（一句话，但请当红线记）：能连 `/var/run/docker.sock`
= 能创建特权容器挂你整块盘 = **等于 root**。`-L` 把它转到你笔记本上，是把服务器的 root 权限
搬到自己机器上放着；配 `socat`/`--security-opt` 之前，先想清楚你是否真的需要它。

### 实战 7：隧道进程死了，你的程序会看到什么（demo 场景 7，✅ 本机实跑）

**想干什么**：把门杀掉，看客户端侧「隧道悄悄断了」长什么样。

```text
$ ss -lnt | grep ':37575 ' || echo '  （门已经没了）'
  （门已经没了）

$ curl -sS --max-time 5 http://127.0.0.1:37575/hello.txt
  （这条按设计会失败，就为了看真实报错原文）
curl: (7) Failed to connect to 127.0.0.1 port 37575 after 0 ms: Could not connect to server
  [退出码 7 —— 非 0 是预期的]
```

demo 给的话：

```text
  ↑ 『隧道悄悄断了』在客户端就是一个普通连接错误；服务端那一侧不留痕，所以脚本里要加
    -o ExitOnForwardFailure=yes + ServerAliveInterval，见下一个场景的配置。
```

**逐行解读**

- 第一行是本章最实用的一条命令：**`ss -lnt | grep ':端口 '`**。门在不在，一秒出结果，
  不用去 `ps` 里翻 ssh 进程（ssh 进程可能活着但门早没了）。
- `curl: (7) … Could not connect to server` —— **没有「隧道断了」这种专属报错**。
  隧道断在客户端就是一个普通连接错误。所有依赖隧道的程序（psql、redis-cli、浏览器、docker）
  都只会报自己那一套连接错误，不会告诉你「问题在隧道」。
- `[退出码 7]` 是 curl 的 `CURLE_COULDNT_CONNECT`（7）。它和「目标服务 refused」是同一个码，
  所以**光看退出码区分不出门没了还是目标挂了** —— 区分方法就是上面的 `ss -lnt` 两条腿。

**两个必须加的生产选项，默认值本机可查（demo 场景 9，✅ 实测）**

```text
$ ssh -F /dev/null -G learner@127.0.0.1 | grep -E '^(exitonforwardfailure|serveraliveinterval|serveralivecountmax|tcpkeepalive) '
Pseudo-terminal will not be allocated because stdin is not a terminal.
exitonforwardfailure no
tcpkeepalive yes
serveralivecountmax 3
serveraliveinterval 0
  exitonforwardfailure no ← 默认『门没开成功也算连接成功』，这是最坑人的静默失败
  serveraliveinterval 0   ← 默认不发保活包，NAT/防火墙掐线后你毫无察觉
```

**逐行解读**

- 那行 `Pseudo-terminal will not be allocated…` 是 **ssh 的 stderr**，
  它出现在这里是因为管道里 `grep` 只过滤 stdout，stderr 直接漏到屏幕。
  和 `-G` 的结果无关，别把它读成「配置有问题」。（`-G` 不联网，也不建门。）
- `exitonforwardfailure no`：门 bind 失败时，ssh **照样把连接当成功**。
  于是 CI 脚本里 `ssh -L … && run_migration` 会一路绿灯跑到迁移，然后在第一个 SQL 上炸。
- `serveraliveinterval 0`：不发保活包。默认只有 `tcpkeepalive yes`，
  而 TCP keepalive 的间隔是内核级的（通常 2 小时）——**对「NAT 表项 5 分钟过期」这类问题毫无用处**。
  `ServerAliveInterval=30` + `ServerAliveCountMax=3` 才是「约 90 秒发现链路死了」的配置。

**推荐组合（demo 场景 9 的命令模板，⚠ 效果需你自己的服务器验证）**

```text
    ssh -f -N -o ExitOnForwardFailure=yes -o ServerAliveInterval=30 \
        -o ServerAliveCountMax=3 -L 37575:127.0.0.1:47305 用户@服务器
    验证门确实开了：ss -lnt | grep 37575    然后 curl -sS http://127.0.0.1:37575/
```

四个参数各干什么（这段是能背的）：

| 参数 | 作用 | 不加会怎样 |
|---|---|---|
| `-N` | **不请求 shell**，只做转发 | 你会得到一个空终端，看起来像卡住；脚本里就等于挂着不返回 |
| `-f` | 认证完成后**进后台** | 终端被占住；配合 `-N` 才是「后台隧道」 |
| `-o ExitOnForwardFailure=yes` | 门开不成功就**退出** | 静默失败：连接算成功，门不存在 |
| `-o ServerAliveInterval=30 -o ServerAliveCountMax=3` | 定期发保活包，超时就把进程收掉 | 隧道半死（NAT 掐线）时进程永远挂着，程序一直连一个死门 |

⚠ **`-f` 和 `ExitOnForwardFailure` 有个配合陷阱**：`-f` 在**认证成功之后**才 fork 到后台，
而门是在同一个阶段建的。若门失败，`-f` 模式下 ssh 的退出码未必能被上层脚本可靠拿到
（本机拿不到，因为走不到这一步）。**所以脚本里 `-f` 之后必须紧跟一条主动验证**：

```bash
ssh -f -N -o ExitOnForwardFailure=yes -L 37575:127.0.0.1:47305 用户@服务器 \
  && for i in $(seq 1 20); do ss -lnt | grep -q ':37575 ' && break; sleep 0.3; done \
  && ss -lnt | grep ':37575 '          # 拿不到就报错退出，别往下跑
```

（⚠ 这段是我给你的写法模板，本机没跑通过成功那一支 —— 因为建不了真门。逻辑与
`ss` 那两条命令是真跑过的。）

---

### 实战 8：`-D` 动态代理，以及 DNS 到底在谁那边解析（demo 场景 8，✅ 本机实跑）

**想干什么**：`-D` 开一个 SOCKS5 门（`ssh -D 1080`），**用一个真 SOCKS5 服务端**收请求，
把「客户端交给代理的是 IP 还是域名」打印出来。这是本节的全部价值，也是网上讲 `-D` 最常讲错的地方。

demo 在沙箱里写了个最小 SOCKS5 服务端（`fake_socks5.py`：解析问候+请求，把请求目标原样写日志，然后一律回 `0x05 = connection refused`），所以**所有请求都会失败**，但失败得很有信息量。

**① 写法一：`--socks5` / `socks5://` —— 本地先解析，再把 IP 交给代理**

```text
$ curl -sS --max-time 6 --socks5 127.0.0.1:46393 http://never-resolves-invalid.example/
  （这条按设计会失败，就为了看真实报错原文）
curl: (97) Could not resolve proxy: 127.0.0.1
  [退出码 97 —— 非 0 是预期的]
```

**② 那句 `Could not resolve proxy` 是 curl 的文案在骗人，`-v` 才看得见真相**

```text
$ curl -sS -v --max-time 6 --socks5 127.0.0.1:46393 http://never-resolves-invalid.example/ 2>&1 | grep -E 'Trying|Failed to resolve|Store negative|curl:'
  （这条按设计会失败，就为了看真实报错原文）
*   Trying 127.0.0.1:46393...
* Store negative name resolve for never-resolves-invalid.example:80
* Failed to resolve "never-resolves-invalid.example" for SOCKS5 connect.
curl: (97) Could not resolve proxy: 127.0.0.1
```

**③ 写法二：`--socks5-hostname` / `socks5h://` —— 域名原样交给代理，由代理解析**

```text
$ curl -sS --max-time 6 --socks5-hostname 127.0.0.1:46393 http://never-resolves-invalid.example/
  （这条按设计会失败，就为了看真实报错原文）
curl: (97) cannot complete SOCKS5 connection to never-resolves-invalid.example. (5)
  [退出码 97 —— 非 0 是预期的]
```

**④ 目标换成能解析的 `localhost`，两种写法对比**

```text
$ curl -sS --max-time 6 -x socks5://127.0.0.1:46393 http://localhost:47305/hello.txt
  （这条按设计会失败，就为了看真实报错原文）
curl: (97) cannot complete SOCKS5 connection to localhost. (5)
  [退出码 97 —— 非 0 是预期的]

$ curl -sS --max-time 6 -x socks5h://127.0.0.1:46393 http://localhost:47305/hello.txt
  （这条按设计会失败，就为了看真实报错原文）
curl: (97) cannot complete SOCKS5 connection to localhost. (5)
  [退出码 97 —— 非 0 是预期的]
```

**⑤ 铁证：代理那一侧收到的原始请求**

```text
$ cat <沙箱>/socks.log
[socks5] 已在 127.0.0.1:46393 监听
[socks5] 问候: VER=5 方法数=2 客户端会的方法=[0, 1]
[socks5] 问候: VER=5 方法数=2 客户端会的方法=[0, 1]
[socks5] 问候: VER=5 方法数=2 客户端会的方法=[0, 1]
[socks5] 请求: CMD=1(1=CONNECT) 目标类型=域名 目标=never-resolves-invalid.example:80
[socks5] 问候: VER=5 方法数=2 客户端会的方法=[0, 1]
[socks5] 请求: CMD=1(1=CONNECT) 目标类型=IPv6 目标=00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:01:47305
[socks5] 问候: VER=5 方法数=2 客户端会的方法=[0, 1]
[socks5] 请求: CMD=1(1=CONNECT) 目标类型=域名 目标=localhost:47305
```

**逐行解读（这一屏是本章最值钱的日志）**

- `问候: VER=5 方法数=2 客户端会的方法=[0, 1]` —— 客户端支持的方法号 0（无需认证）和 1（GSSAPI）。
  `ssh -D` 的 SOCKS5 服务端也走这一套；`[0,1]` 说明 curl 优先无认证。
- **②那次有 3 条「问候」没有对应「请求」**：curl 在**本地解析域名就失败了**，
  请求根本没发出去（`Failed to resolve … for SOCKS5 connect`），还顺手缓存了 negative 结果
  （`Store negative name resolve`）。而末尾那句 `Could not resolve proxy: 127.0.0.1`
  **是文案 bug**：proxy 明明是 `127.0.0.1`，它早就解析好了 —— 报的其实是「目标解析失败」。
  **凡是走 `--socks5`（不带 hostname）+ 域名目标，就可能撞上这条误导性文案。**
- **④第一种写法**：代理收到 `目标类型=IPv6 目标=…00:01:47305` —— 就是 `::1:47305`。
  说明 `socks5://` 在**本地**把 `localhost` 解析了，而且解析成了 **IPv6 的 `::1`**。
  这就是那种「明明开了隧道，为什么连不上 127.0.0.1 上的服务」的根因：
  目标只监听 `127.0.0.1`（IPv4），你递过去的却是 `::1`。**（本机的目标服务确实只绑了 127.0.0.1，见实战 1。）**
- **④第二种写法**：代理收到 `目标类型=域名 目标=localhost:47305` —— 域名原样递过去了，
  由**代理侧**解析。这才是 `-D` 想要的语义。
- 三条 `curl: (97) cannot complete SOCKS5 connection to … (5)`：结尾的 `(5)` 是 SOCKS 回复里的
  `0x05 = connection not allowed by ruleset`，也就是 demo 服务端故意回的拒绝 —— **不是隧道坏了**。

demo 自己的总结（压缩排版、措辞照它）：「写法一那次代理日志里只有『问候』没有『请求』
（本地解析就失败了）；写法一 + localhost 那次代理收到的目标类型是 IPv6（本地已解析、且解析成 `::1`）；
写法二代理收到的是域名字符串本身，DNS 发生在代理那一侧 —— 这才是 `-D` 想要的效果」。

**⑥ 门不存在时的报错（脚本已经把 SOCKS 进程杀了，所以这就是真实状态）**

```text
$ curl -sS --max-time 6 -x socks5h://127.0.0.1:46393 http://example.com/
  （这条按设计会失败，就为了看真实报错原文）
curl: (7) Failed to connect to example.com port 80 via 127.0.0.1 after 0 ms: Could not connect to server
  [退出码 7 —— 非 0 是预期的]
```

注意它的写法：`… port 80 via 127.0.0.1` —— **curl 明确告诉你它是「经过 127.0.0.1 这个代理」失败的**。
看到 `via <你的门>` 就是门没在听；看到 `Could not resolve proxy` 反而要怀疑是 DNS 那一侧。

**等价 ssh 写法与使用面（⚠ 需你自己的服务器验证）**

```text
ssh -f -N -o ExitOnForwardFailure=yes -D 127.0.0.1:1080 用户@服务器
浏览器： SOCKS5 host=127.0.0.1 port=1080，并勾选「SOCKS5 时也代理域名」（= 上面的 socks5h 语义）
命令行： export ALL_PROXY=socks5h://127.0.0.1:1080      # h 很关键
git：   git -c http.proxy=socks5h://127.0.0.1:1080 clone …
```

**为什么 `-D` 值得单独一节**：`-L` 是「一个门对一个固定目标」，`-D` 是「一个门对任意目标」，
代价是三条安全边界全变：① 服务器替你做 DNS（查询不泄漏，但**服务器的 DNS 视图**会骗你）；
② 服务器变成你的出口（它的出口日志会记下你的流量）；③ `-D` 的门**默认没有目标白名单**
（`-L` 至少还能用服务端 `PermitOpen` 限目标）。

---

### 实战 9：ssh 侧的参数与语法 —— 这一半能在本机真跑（demo 场景 9 + 补充实测，✅）

**① demo 场景 9 抓到的客户端语法报错原文**

```text
--- 这些是本机真跑出来的客户端报错（连不上都轮不到，纯语法）：
  ssh -L badspec -> Bad local forwarding specification 'badspec'
  ssh -D abc -> Bad dynamic forwarding specification 'abc'
  ssh -L 47305 -> Bad local forwarding specification '47305'
  ssh -L :80 -> Bad local forwarding specification ':80'
  ssh -L 0:127.0.0.1:80 -> Bad local forwarding specification '0:127.0.0.1:80'
```

**② `-L` 和 `-R` 的段数要求不一样**（demo 场景 10 ①：14 种写法连着真跑，同一个循环、同一套护栏参数。
下面贴前 6 条原文；后 8 条的结论都在紧接着的对照表里，表里每一条都真跑过。）

```text
$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh -o IdentityFile=<沙箱>/keys/_no-default-probe -o ConnectTimeout=8 -R badspec learner@127.0.0.1
  （这条按设计会失败，就为了看真实报错原文）
Bad remote forwarding specification 'badspec'
  [退出码 255 —— 非 0 是预期的]

$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh -o IdentityFile=<沙箱>/keys/_no-default-probe -o ConnectTimeout=8 -D 0 learner@127.0.0.1
  （这条按设计会失败，就为了看真实报错原文）
Bad dynamic forwarding specification '0'
  [退出码 255 —— 非 0 是预期的]

$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh -o IdentityFile=<沙箱>/keys/_no-default-probe -o ConnectTimeout=8 -D 127.0.0.1:abc learner@127.0.0.1
  （这条按设计会失败，就为了看真实报错原文）
Bad dynamic forwarding specification '127.0.0.1:abc'
  [退出码 255 —— 非 0 是预期的]

$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh -o IdentityFile=<沙箱>/keys/_no-default-probe -o ConnectTimeout=8 -D 37575 learner@127.0.0.1
  （这条按设计会失败，就为了看真实报错原文）
Pseudo-terminal will not be allocated because stdin is not a terminal.
no such identity: <沙箱>/keys/_no-default-probe: No such file or directory
learner@127.0.0.1: Permission denied (publickey,password).
  [退出码 255 —— 非 0 是预期的]

$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh -o IdentityFile=<沙箱>/keys/_no-default-probe -o ConnectTimeout=8 -L 8080 learner@127.0.0.1
  （这条按设计会失败，就为了看真实报错原文）
Bad local forwarding specification '8080'
  [退出码 255 —— 非 0 是预期的]

$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh -o IdentityFile=<沙箱>/keys/_no-default-probe -o ConnectTimeout=8 -L 8080:127.0.0.1:99999 learner@127.0.0.1
  （这条按设计会失败，就为了看真实报错原文）
```

**读法（这张对照是本节重点）**

| 写法 | 结果 | 为什么 |
|---|---|---|
| `-L 8080` | **Bad** | `-L` 必须知道「去连谁」，只给端口没意义 |
| `-R 80` | **通过解析**（走到认证才停） | `-R` 允许只给端口：含义是「远端 80 → 连回我本机 80」 |
| `-L 8080:127.0.0.1:99999` | **Bad** | 端口范围在**解析阶段**就查，不到 bind 阶段 |
| `-L 0.0.0.0:8080:127.0.0.1:47305` | 通过解析（实测停在认证） | 想放开绑定地址就写**四段**：绑址 + 门 + 目标 + 目标端口 |
| `-L [::1]:8080:127.0.0.1:47305` | 通过解析（实测停在认证） | 绑 IPv6 回环，方括号是必须的 |
| `-L 0:127.0.0.1:80` | **Bad** | 三段式的**第二段必须是端口号**，`0` 在这里被当主机名段读 |
| `-L /tmp/m.sock:127.0.0.1:80` | 通过解析（实测停在认证） | 第一段是路径 → 门就是 unix socket（实战 6） |
| `-L [::1]:8080:127.0.0.1:80` | 通过解析（本机实测停在认证） | 绑定 IPv6 用方括号 |
| `-D 127.0.0.1:37575` | 通过解析（实测停在认证） | `-D` 也允许带绑地址 |
| `-R 9000:localhost:3000` / `-R 0:localhost:3000` | 都通过解析（实测停在认证） | `-R` 的端口写 `0` = 让服务器随便分配 |
| `-D 0` / `-D 127.0.0.1:abc` | **Bad** | `-D` 必须是可解析的端口号 |

**推论**：`Bad … forwarding specification` 这类错**和网络、密钥、服务端配置都无关**，
是命令行本身写错了 —— 看到它就只该改命令行，别去翻 sshd。

上面那张表里所有「通过解析（实测停在认证）」的证据都是同一种形状（demo 场景 10 ① 里 `-L 0.0.0.0:8080:…` 那条的原文）：

```text
$ ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh -o IdentityFile=<沙箱>/keys/_no-default-probe -o ConnectTimeout=8 -L 0.0.0.0:8080:127.0.0.1:47305 learner@127.0.0.1
  （这条按设计会失败，就为了看真实报错原文）
Pseudo-terminal will not be allocated because stdin is not a terminal.
no such identity: <沙箱>/keys/_no-default-probe: No such file or directory
learner@127.0.0.1: Permission denied (publickey,password).
  [退出码 255 —— 非 0 是预期的]
```

判读规则：**看到 `Bad … specification` = 参数写错；看到 `Permission denied` = 参数写对了、
死在认证**。这两个报错的区别就是「语法关」和「认证关」的分界，本章所有排错都从这一步开始分类。
（`-R 9000:localhost:3000`、`-R 0:localhost:3000`、`-D 37575`、`-L [::1]:8080:…`、
`-L <路径>.sock:…` 的实测输出全是上面这一形状，都出自场景 10 ① 那一个循环。）

顺一条真实噪声：这 14 条要是**并发**打 22 端口，会偶发 `kex_exchange_identification: read:
Connection reset by peer` —— 那是本机 sshd 的 `MaxStartups` 限流，和 `-R` 无关；
01 章坑 8 讲的正是它。看到 **kex 之前**的 reset 就不要去查密钥和转发。
所以场景 10 ① 每条之间 `sleep 2`：本轮这 14 条的报错**全部**是 `Bad … specification`
或 `Permission denied`，一条 kex 噪声都没混进来 —— 这也是下面那张表敢逐行引用的原因。

**③ 配置文件里写转发是「两个参数」，不是命令行那一串**（demo 场景 10 ⑤）

```text
$ ssh -F <沙箱>/onearg -G t 2>&1 | tail -2
  （这条按设计会失败，就为了看真实报错原文）
<沙箱>/onearg line 2: Missing target argument.
<沙箱>/onearg: terminating, 1 bad configuration options

$ ssh -F <沙箱>/twoarg -G t | grep -icE 'bad|missing' || true
Pseudo-terminal will not be allocated because stdin is not a terminal.
0
```

对应到 `~/.ssh/config` 的正确写法：

```text
# ✗ 错（照抄命令行）：LocalForward 37575:127.0.0.1:47305
LocalForward 37575 127.0.0.1:47305         # ✓ 两段：先「听哪」，再「去哪」
RemoteForward 9000 localhost:3000
DynamicForward 1080
ExitOnForwardFailure yes
ServerAliveInterval 30
ServerAliveCountMax 3
```

`Missing target argument.` 这条报错值得背：**它整段终止**（`terminating, 1 bad configuration options`），
所以一个 Host 段里写错一行转发，**整个 config 都不生效**，症状像「所有 SSH 都忽然变慢了」。
03 章会专门讲这类「配置里一个坏选项打死全部」。

上面那一整套写法本机已验过语法（demo 场景 10 ⑤ 原文；`-G` 不会 dump 转发列表，
所以「接受」的判据是 bad/missing 计数为 0）：

```text
$ ssh -F <沙箱>/cfg -G t | grep -icE 'bad|missing' || true
Pseudo-terminal will not be allocated because stdin is not a terminal.
0

$ ssh -F <沙箱>/cfg -G t | grep -E '^(exitonforwardfailure|serveraliveinterval|serveralivecountmax) '
Pseudo-terminal will not be allocated because stdin is not a terminal.
exitonforwardfailure yes
serveralivecountmax 3
serveraliveinterval 30
```

**这一段的意义**：`exitonforwardfailure yes` + `serveraliveinterval 30` 写进 `~/.ssh/config` 之后，
坑 #3 和坑 #8 就都不存在了 —— 这是本章最划算的一次改动，一次配置、终身有效。

---

### 实战 10：`-R` 反向与三个服务端开关（⚠ 需你自己的服务器验证）

本机建不了真门（认证过不去），所以本节**不贴任何成功输出**，只给方向和命令模板。
但语法层面本机已验证（实战 9 的 `-R badspec` / `-R 80`），文案层面也已从二进制查到（实战 5 的 `strings`）。

**方向（把心智模型那张图压缩成一句）**：`-R` 的监听端口开在**服务器**上，
冒号右边的目标由**你的机器**去连。所以 `-R 9000:localhost:3000` 的意思是
「服务器上听 9000，谁连它就转到**我本机的 3000**」。

```text
# 场景：你在 NAT/家庭网络后面，运维在服务器上，他要临时看你本机跑的 demo
服务器（有公网）  ──── 你 ssh 上去时顺手带着 ────►  你的机器
      │                                                   │
      └─ 0.0.0.0:9000 或 127.0.0.1:9000（受 GatewayPorts 管）└─ localhost:3000

ssh -N -R 9000:localhost:3000 用户@服务器        # 服务器上多出一个门
ssh -N -R 0:localhost:3000 用户@服务器           # 让服务器随便分配端口，ssh 会打印实际值
curl -s http://127.0.0.1:9000/                   # 在【服务器上】执行这句才算验证
```

**为什么默认还是「只绑 127.0.0.1」**：`GatewayPorts no`（sshd 默认）时，
即使你写 `-R 0.0.0.0:9000:…`，服务器也只会绑在自己的回环上。
好消息是：**ssh 会明确告诉你它被降级了** —— 这句文案在本机二进制里查得到（✅ 实测原文，
来自 `strings /usr/bin/ssh` 的转发文案全表，原文在 demo 场景 10 ④）：

```text
Forwarding listen address "%s" overridden by server GatewayPorts
Local forwarding listening on path %s.
%s allowed port %d for forwarding to host %s port %d
```

三条各有用处：第一条就是你怀疑「`-R` 配了却连不进去」时要 grep 的那句；
第二条证明 unix socket 门（实战 6）在成功时会打印它听在哪个路径；
第三条是**服务端**允许与否的回复文案（`%s allowed port %d …`，出现在 sshd 侧/`-v` 里）。
⚠ 但三条的实际带值输出都要在真服务器上才能看到 —— 本机只证明了「这些字符串存在于这个版本的 ssh 里」。

三个服务端开关（都在 `sshd_config`，**本机查不到实际值**）：

```text
    sshd 侧：GatewayPorts no|clientspecified|yes   PermitTunnel no|yes|ethernet|ip
             AllowTcpForwarding yes|no|local|remote
    自查模板： ssh -G 用户@服务器 | grep -i forward   （客户端侧）
             服务器端 root： sshd -T | grep -iE 'gatewayports|allowtcpforward|permittunnel'
```

| 开关 | 管什么 | 值 | 你会看到的症状 |
|---|---|---|---|
| `GatewayPorts` | `-R` 的门在服务器上绑哪 | `no`（默认，只绑服务器回环）/ `clientspecified`（尊重你写的绑地址）/ `yes`（一律绑 0.0.0.0） | 配 `no` 时你从别的机器连不上那个 9000 |
| `AllowTcpForwarding` | 允不允许 -L/-R（`local` 只放行 -L，`remote` 只放行 -R） | 默认 `yes` | 被禁时：`-L` 会得到 channel 层的 `administratively prohibited`；`-R` 得到 `Error/Warning: remote port forwarding failed for listen port …` |
| `PermitOpen` / `PermitListen` | 精确限定「能转去哪些目标」/「-R 能听哪些端口」 | `none` / 列表 | 违规请求被拒，**日志在服务端** |
| `PermitTunnel` | `-w`（`ssh -w 10:10`）建 TUN/TAP 层隧道，**需要 root 权限** | 默认 `no` | 未开时 `Tunnel forwarding using interface …` 之后就是失败 |

demo 也直接给了这条自查命令，注意它的作用域（✅ 本机可跑，但只 dump 客户端侧的值）：

```bash
ssh -G 用户@服务器 | grep -i forward      # 客户端认为的转发配置；不是服务端答案
```

**`-R` 的三条实战提醒**（都不需要成功输出就能记）：

1. **`-R` 的失败默认不致命**：sshd 回 `Warning: remote port forwarding failed…` 时 ssh **继续跑**，
   你要盯 stderr。把它当「连上=配好」是 `-R` 最大的事故来源（坑 #6）。
2. **`-R` 是在别人机器上开口子**：用完 `ssh -O exit` / 关掉进程，并考虑在服务器侧
   `PermitListen none` 兜底。
3. **半死链路**：`-R` 的门跟着连接活；NAT 掐线后服务器上的门可能还在听（旧 channel 已死），
   这就是「门在、内容不对」的另一类成因。解法同实战 7 的 `ServerAliveInterval`。

⚠ **不属于本章的三个邻近概念**（免得你搜到这儿就走错了路）：
`-J`/`ProxyJump`（多跳登录，是**连接路径**不是端口转发，见 03 章）、
`-W host:port`（把 stdio 转成一个 TCP 连接，给 `ProxyCommand`/git 用）、
`ssh -w`（二层 TUN 隧道，要 `PermitTunnel` 且两端 root）。

## 命令速查表

标记：✅ 本机可跑（结果随手可查）／⚠ 效果要你自己的服务器。

| 我想干什么 | 命令 | |
|---|---|---|
| 开一个本地门（生产写法） | `ssh -f -N -o ExitOnForwardFailure=yes -o ServerAliveInterval=30 -L 8080:127.0.0.1:3306 用户@服务器` | ⚠ |
| **门到底开了没** | `ss -lnt \| grep ':8080 '` | ✅ |
| 门存在但内容对不对 | `curl -sS --max-time 5 http://127.0.0.1:8080/` | ✅（形态） |
| 谁占了我的端口 | `ss -lntp \| grep ':8080 '` | ✅ |
| unix socket 门 | `ss -xlp \| grep 名字`（`ss -lnt` 里**找不到它**） | ✅ |
| 反向门（别人连服务器 → 打我本机） | `ssh -N -R 9000:localhost:3000 用户@服务器` | ⚠ |
| 让服务器随便分配远端端口 | `ssh -N -R 0:localhost:3000 用户@服务器`（会打印 `Allocated port …`） | ⚠ |
| 动态 SOCKS 代理 | `ssh -f -N -D 127.0.0.1:1080 用户@服务器` | ⚠ |
| 用代理**且让远端解析域名** | `export ALL_PROXY=socks5h://127.0.0.1:1080`（`h` 是关键） | ✅（语义本机验证） |
| 让本机解析域名（对比用） | `curl --socks5 127.0.0.1:1080 …`（≡ `socks5://`） | ✅ |
| 只放开一个门给同网段 | `-L 0.0.0.0:8080:127.0.0.1:80`（四段式） | ⚠ |
| 把所有本地/动态门都放 0.0.0.0 | `-g` | ⚠ |
| 这次连接不要用配置文件里的转发 | `-o ClearAllForwardings=yes` | ⚠（只管配置文件，压不掉命令行 `-L`） |
| 配置文件里写转发 | `LocalForward 8080 127.0.0.1:3306`（**两个参数**，不是一个串） | ✅ |
| 查客户端侧转发相关默认值 | `ssh -G 主机 \| grep -iE 'forward\|gateway\|tunnel\|clearall'` | ✅ |
| 查服务端三道门禁 | 服务器 root：`sshd -T \| grep -iE 'gatewayports\|allowtcpforward\|permitopen\|permittunnel'` | ⚠ |
| 在已登录的复用会话上临时加/减门 | `ssh -O forward -S <sock> -L 8080:…`／`ssh -O cancel -S <sock> -L 8080:…`／`ssh -O check -S <sock>` | ⚠（机制见 04 章） |
| 收掉隧道进程 | `kill <pid>`（`-f` 模式先 `pgrep -af 'ssh.*-N.*-L 8080:'`），然后 `ss -lnt` 复核 | ✅ |
| 转发远端 docker | `ssh -N -L /tmp/r.docker.sock:/var/run/docker.sock 用户@服务器` + `DOCKER_HOST=unix:///tmp/r.docker.sock` | ⚠（安全见实战 6） |

---

## 这章的坑

1. **`-L 8888:localhost:80` 里的 `localhost` 是服务器的。** 冒号右边那串由**中转点**解析和连接
   （demo 场景 3 用三个不同上游演给你看了）。同理 `-R` 的冒号右边是你自己的机器 —— 方向相反。
2. **「门开了」≠「门有用」。** `ss -lnt` 那一行只能证明 `bind()` 成功；
   上游通不通要么真发一个请求，要么看 `-v`。实战 1 的「先记基准输出」就是为了这条。
3. **`ExitOnForwardFailure` 默认是 `no`**（本机实测 dump 出来了）。
   门 bind 失败，连接照样算成功 → CI 一路绿灯跑到业务代码才炸。**脚本里必加 `yes`。**
4. **`-L` 不给三段是语法错，`-R` 给一个端口却是合法的**（本机实测这条不对称）。
   照抄网上 `ssh -L 8080 host` 那种写法会直接 `Bad local forwarding specification`。
   同一条不对称还体现在端口 `0` 上：**`-L …:0:…` 一律 Bad，`-R 0:…` 能过解析**（让服务器分配端口
   是 `-R` 独有的能力）—— 所以本地门撞车时只能自己换端口，别指望 `-L` 自动分配（实战 5 实测原文）。
5. **`--socks5` 和 `--socks5-hostname` 是两个语义**：前者本机解析（DNS 泄漏 + 可能踩 `::1`），
   后者交给代理解析。代理日志实测证实了这条区别（demo 场景 8），别混用。
6. **`-R` 失败常常只是 `Warning:`。** 二进制里同时存在 `Error:` 和 `Warning:` 两版文案 ——
   Warning 那版不会让 ssh 退出。**「连上了」不等于「远端门建成了」**,要 grep stderr。
7. **`-g` / `-L 0.0.0.0:…`（客户端绑址）和 `GatewayPorts`（服务端门禁）不是一回事。**
   而且客户端 `-G` 也会 dump 出 `gatewayports no`，很容易误当成服务端答案。
8. **`tcpkeepalive yes` 不等于保活**。实测默认 `serveraliveinterval 0`（不发保活包）+ `tcpkeepalive yes`
   （内核级 keepalive，间隔默认很久）。NAT 5 分钟过期时，只有 `ServerAliveInterval` 能救你。
9. **忘了 `-N` 得到一个「像卡住的空终端」；忘了 `ExitOnForwardFailure` 加 `-f` 得到一个后台僵尸门。**
   `-N` = 不要 shell；`-f` = 后台。**`-f` 之后必须主动 `ss -lnt` 验证门**，因为退出码不可靠。
10. **管道的退出码会骗人**。demo 自己的 `… | tail -2` 那条失败命令就没有触发「退出码」提示行
    （退出码属于 `tail`）。脚本里判成败要么看输出内容，要么用 `${PIPESTATUS[0]}`。
11. **unix socket 门在 `ss -lnt` 里查不到**，要用 `ss -xlp`；而 demo 转出来的 socket 是
    `srwxrwxr-x`（同组/其他人可连）—— 权限比 TCP 门绑 127.0.0.1 更松，收尾要删。
12. **`localhost` 会被解析成 `::1`**（demo 场景 8 的 IPv6 行铁证）。目标服务只绑 `127.0.0.1` 时，
    走 `socks5://localhost:port` 就连不上；写 `127.0.0.1` 或用 `socks5h://` 交给代理解析。
13. **配置文件里转发写成一段 → `Missing target argument.`，而且是「terminating, 1 bad configuration
    options」整段终止**。症状是「所有 ssh 行为忽然回到默认」，不是「这一条 Host 不生效」。
14. **遗留监听是本章唯一会伤到别人的事。** 你转的是**内网服务**，一个没人认领的门就是给同网段留的后门。
    demo 的收尾三连自证（PID 回收 / `ss -lnt` 干净 / 专门查 2222 8098 8099 1080）就是这个动作，
    值得抄进你自己的脚本 —— 本章历史上真在 2222 端口漏过一次没关的进程。

---

## 自检问题

1. `-L 8080:localhost:80` 和 `-R 9000:localhost:3000`，两个 `localhost` 分别是谁的 localhost？
   **答**：前一个是**服务器**的（因为 `-L` 的目标由中转点连接），后一个是**你本机**的
   （`-R` 的门在服务器上，字节反向往你送）。记忆法：冒号右边的地址，由**门所在的那台机器**去连。

2. 你跑了 `ssh -f -N -L 8080:127.0.0.1:3306 用户@服务器`，`curl 127.0.0.1:8080` 报 refused。
   按本章的顺序，前两条该敲什么？
   **答**：① `ss -lnt | grep ':8080 '` —— 门在不在；
   ② 若在，说明是**服务器侧**连不上 3306（MySQL 没起 / 只绑了别的地址），
   要登录到服务器上验；若不在，多半是端口被占或认证根本没过去（`-f` 静默）。

3. 为什么 `-R` 建好之后，别人从同网段连不上那个 9000，而你在服务器上 `curl 127.0.0.1:9000` 却是通的？
   **答**：`GatewayPorts no`（服务端默认）会把远端门强制绑在服务器的回环上。
   需要服务端 `GatewayPorts clientspecified`（然后你写 `-R 0.0.0.0:9000:…`）或 `yes`。
   ssh 在这种情况下会打印 `Forwarding listen address "…" overridden by server GatewayPorts`。

4. `-D 1080` 之后 `curl --socks5 127.0.0.1:1080 http://内部名/` 报
   `Could not resolve proxy: 127.0.0.1`。代理明明是通的，问题出在哪？
   **答**：curl 的文案 bug。真实原因用 `-v` 才看得到：`--socks5`（不带 hostname）会**在本地**解析
   目标域名，解析失败就报错，而错误串错误地指向了 proxy 地址。改用 `--socks5-hostname` / `socks5h://`
   把域名交给代理解析（demo 场景 8 实测）。

5. `-N`、`-f`、`-o ExitOnForwardFailure=yes` 分别解决什么问题？少了哪个最危险？
   **答**：`-N` 不请求 shell（只转发）；`-f` 认证后进后台；`ExitOnForwardFailure=yes` 让门失败即退出。
   最危险的是**少了第三个还配了 `-f`**：门没开、进程却活在后台，你的程序对着空气报错。

6. `ss -lnt` 里 `127.0.0.1:8080` 和 `0.0.0.0:8080` 的差别，在本章意味着什么？
   **答**：意味着这个门是只有本进程/本机能连，还是**同网段所有人都能连**。
   demo 场景 4 把 `ip -4 -o addr show` 打在旁边就是为了这条：`0.0.0.0` 的门会挂在你实际网卡所在网段上。

---

## 怎么跑这章的 demo

```bash
cd ssh-handbook/05-port-forwarding
bash demo.sh          # 跑完自动 kill 所有自己起的进程、删沙箱、三连自证
bash demo.sh --keep   # 保留沙箱和进程，打印 PID/端口，你自己玩完记得 kill
```

一次真跑的规模：**476 行输出、退出码 0、10 个场景**（那次的沙箱 `<沙箱>`）。
重生成那份捕获只要一条命令（章目录 0 产物，副作用只进沙箱）：

```bash
cd ssh-handbook/05-port-forwarding
bash demo.sh > ~/.cache/handbook-captures/ssh05.txt 2>&1; echo EXIT=$?
```

10 个场景与本章实战一一对应：1→实战1（目标与 `ss` 基准）、2→实战2（人肉 `-L` 与 relay 日志）、
3→实战3（换上游：B / 22 / 631）、4→实战4（`127.0.0.1` vs `0.0.0.0` + `ip -4 -o addr show`）、
5→实战5（`Errno 98` + `strings` 里的 `bind [%s]:%s` + ssh 停在认证）、6→实战6（unix socket 门）、
7→实战7（门没了的 `curl: (7)`）、8→实战8（真 SOCKS5 + `--socks5` vs `--socks5-hostname` 的 ATYP 差异）、
9→实战9/10（`Bad … specification` 五条 + `-G` 的四个默认值 + 推荐组合与 sshd 开关清单）、
10→实战 5/6/9 的补充边界（`-L/-R/-D` 14 种写形的解析结果、`-G` dump 出来的转发相关键、
`ss -lntp` 查「谁占了门」、`strings` 里的转发文案全表、`LocalForward` 一段式 vs 两段式）。
证据强度：1–8 全 ✅，9、10 的语法/默认值/文案 ✅、隧道效果 ⚠。

围栏里的命令是 demo 源码里 `$SB`（沙箱）、`$PORT_*`（本轮随机端口）这些变量**展开后**的样子 ——
捕获文件里存的就是展开后的原文，所以「README 写 `python3 <沙箱>/relay.py 127.0.0.1 37575 …`、
脚本源码写 `python3 "$SB/relay.py" 127.0.0.1 $PORT_DOOR …`」是同一件事的两种写法，
不是「README 演了 demo 没演的命令」。

**本章 demo 里已验证的**：目标与门的 `ss` 形态、relay 双向搬字节、上游可换、`Errno 98`、
`bind [%s]:%s: %.100s` 等转发文案模板确实存在于这个 ssh 里、unix socket 门的 `ss -xlp`、
`-D` 的三种 curl 写法与代理侧收到的 ATYP、`-L/-R/-D` 的语法边界、
`exitonforwardfailure no` / `serveraliveinterval 0` / `tcpkeepalive yes` / `serveralivecountmax 3` 的默认值、
`-G` 对 `LocalForward` 一段式写法的报错、收尾无遗留监听。

**⚠ 需要你自己的服务器才能验证的（本章一个字都没编造成功输出）**：
`ssh -L` / `-R` / `-D` 真正打通后的端到端效果、
门建成时 ssh 的实际提示行（`Allocated port …`、`Local forwarding listening on path …`）、
`GatewayPorts` / `AllowTcpForwarding` / `PermitOpen` / `PermitTunnel` 的服务端实际值与被拒时的原文、
`-g` 后同网段真能连上、`-R` 半死链路的表现、docker socket 转发、
`-O forward/cancel` 在复用会话上动态加减门（见 04 章）、`-f` 的可靠退出码。
文中每一处都标了 ⚠ 并给了命令模板。

**安全自证**（demo 结尾三段原文）：

```text
[沙箱已删除] <沙箱>
[收尾自检 1] 本脚本起的进程还在不在：
[收尾自检 2] 有没有遗留监听端口（本章用过的端口都应消失）：
  干净：没有遗留监听
[收尾自检 3] 顺便确认没有当年那种 2222 僵尸监听：
  干净
```

9 个 PID（`167637 167673 167683 167687 167694 167717 167726 167748 167798`）全部打印「已回收」；
全程只在 `127.0.0.1` / `0.0.0.0` 的本机端口上做过实验，没连任何外部主机；
每条 `ssh` 都带 `-F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey`，
所以最后停在 `Permission denied (publickey,password).`（没有登录成功）；
系统 sshd（22 端口）只被被动观察，没有被重启或改配置。

**本章补充实测**（就是 demo 场景 10：同一套护栏参数、同一个沙箱、同一次跑）：
`-R badspec` / `-L 8080` / `-R 80` / `-D 0` / `-D 127.0.0.1:abc` /
`-L 0.0.0.0:8080:…` / `-L [::1]:8080:…` / `-L <路径>.sock:…` / `-R 9000:…` / `-R 0:…` 的解析结果、
`LocalForward` 一段式 vs 两段式、`ssh -G` dump 的转发相关键、
`strings /usr/bin/ssh` 里全部转发相关格式串（含 `Forwarding listen address "%s" overridden by server GatewayPorts`）。

---

## 下一步

- **04-agent-and-multiplexing**：`ControlMaster`/`ControlPath` 建好一条会话后，
  用 `ssh -O forward` / `-O cancel` **在不重新登录的情况下加减门** —— 这是本章那条
  「门是认证成功后才建的」的直接收益；也是 `ServerAliveInterval`、半死链路处理的正主。
- **03-config-file**：把 `-L 8080 127.0.0.1:3306`、`ExitOnForwardFailure`、`ServerAliveInterval`
  写进 `~/.ssh/config` 的 Host 段（注意坑 #13 的两段式写法和「一个坏选项打死全部」）。
- **06-troubleshooting**：`bind […] : Address already in use`、
  `remote port forwarding failed`、`administratively prohibited`、
  `kex_exchange_identification: … Connection reset`（本章实测撞过）逐条对照。
- **现在就能做的三个动作**：
  ① `ss -lnt` 看一遍你这台机器上现在有几个「没人认领的门」（`ssh -L` 的遗留一般长这样）；
  ② 把你最常用的那条隧道写成 `Host db` + `LocalForward` 两段式，加上 `ExitOnForwardFailure yes`；
  ③ 在 `~/.ssh/config` 里全局加 `ServerAliveInterval 30` + `ServerAliveCountMax 3`，
     从此不再有「终端挂着但其实早就断了」的隧道。



