# 这章：SSH 到底在干什么

> **一句话：把「我输了 `ssh user@host` 之后到弹出密码框之前，网络上来回了哪几句话」彻底搞清楚。**
> 搞清楚了这一件事，后面所有章节（密钥、配置、隧道、排错）都是在往这条线上挂零件。

本章所有输出都是**本机真跑出来的**（OpenSSH_10.2p1，客户端和服务端都是它，服务端在 `127.0.0.1:22`）。
唯一演不了的是「登录成功之后的画面」—— 因为按实验规矩，我们不会往服务器上装任何 authorized_keys。
凡是这种情况，文中会写清楚「这段需要你自己有服务器时验证」。

---

## 你会遇到的场景

| # | 场景 | 学完这章你能自己解释 |
|---|---|---|
| 1 | `ssh -vvv` 打了一屏 `debug1/debug2/debug3`，不知道哪行重要 | 这章就是逐行标注那份日志 |
| 2 | 第一次连服务器，弹出一句 `Are you sure you want to continue connecting (yes/no/[fingerprint])?`，慌忙按了回车 | 这叫 TOFU，按下去就等于把信任签字画押了 |
| 3 | 某天突然弹一大屏 `WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!` | 99% 是服务器重装/换了密钥，1% 是真的被人中间人了 |
| 4 | 明明密码输对了，却报 `Unable to negotiate ... no matching key exchange method found` | 你的新版客户端和它的老服务器算法对不上 |
| 5 | 想用 `ssh-keyscan` 批量把一堆机器的 known_hosts 生成好，又听说「不安全」 | 这章会说清它到底不安全在哪、什么时候能用 |
| 6 | 一直以为「登录就是用私钥加密一段东西发过去」 | **这个理解是错的**，本章专门开一节纠正 |

---

## 心智模型

### 第一块：整条链路只有两次「换锁」

新手最大的困惑是：SSH 里到底有几把钥匙？答案简单得有点反直觉 —— **一次连接里，非对称加密只出现在开头那一小段，之后所有数据都走对称加密**。

为什么要这样？因为两件事互相矛盾：

| | 非对称（RSA / Ed25519 / 曲线25519） | 对称（AES-GCM / ChaCha20） |
|---|---|---|
| 快不快 | 慢（大数运算/曲线运算） | 极快（有 CPU 硬件指令，能跑到几个 GB/s） |
| 需不需要提前共享密钥 | **不需要**，_public_ 谁都能拿到 | **需要**，双方得先有一模一样的那把钥匙 |
| 适合干什么 | 建立信任、协商秘密 | 搬数据 |

所以 SSH 的做法是：**用非对称解决「互不相识的两台机器怎么在没有窃听者能看懂的通道上，先弄出一个共同的秘密」，然后用这个秘密去开对称锁搬数据。**

非对称在这条链路上只干三件事，**不多不少**：

1. **证明「这台服务器就是它声称的那台」** —— 服务器用自己的**主机私钥**对握手数据签名，客户端拿 known_hosts 里的**主机公钥**验证。
2. **安全地协商出一个对称密钥** —— 用 (EC)DH 类算法，双方各出一半、拼出同一个秘密，窃听者看到两边公开的部分也算不出来。
   （严格说 DH 本身不是非对称加密，但它属于「不需要预先共享秘密就能建立秘密」这一类，思路和公钥密码同源。）
3. **证明「你就是你」** —— 登录时你的**用户私钥**对服务器给的挑战签名，服务器用 `authorized_keys` 里的公钥验。

**数据传输一行都不走非对称。** 你在 SSH 里敲的每个字符、`scp` 传的每个字节，都是对称加密的。

### 第二块：一次完整握手的时序图（两条泳道）

```
   客户端 (ssh)                                            服务端 (sshd)
        |                                                        |
   ─────+--- ① TCP 连上 22 端口 --------------------------------->+
        |                                                        |
        |<──── ② SSH-2.0-OpenSSH_10.2p1 ...  (明文，就一行) ──────|
        |─── ③ SSH-2.0-OpenSSH_10.2p1 ...    (明文，就一行) ─────>|   版本交换
        |                                                        |   （双方看对方是谁，
        |                                                        |     顺便决定要不要开某些
        |                                                        |     兼容老实现的黑历史开关）
   ─────+── ④ SSH2_MSG_KEXINIT --------------------------------->|
        |    "我支持这些 KEX / 主机密钥类型 / 对称算法 / MAC / 压缩" |
        |<──────────────────────── ⑤ SSH2_MSG_KEXINIT ───────────+   算法协商
        |    服务端也报一遍自己的清单                              |   （全明文！）
        |                                                        |
        |   双方各自取"我列表里第一个也在你列表里的" → 成交          |
   ─────+── ⑥ SSH2_MSG_KEX_ECDH_INIT ---------------------------->|
        |    客户端的 DH 公钥份额（临时生成，用完就扔）              |
        |<──── ⑦ SSH2_MSG_KEX_ECDH_REPLY ────────────────────────+
        |    · 服务器的主机公钥 blob                                |
        |    · 服务器的 DH 公钥份额                                 |
        |    · 用【主机私钥】对上面所有内容的【签名】  ← 非对称第 1 次 |
        |                                                        |
        |   客户端此时做两件独立的事：                               |
        |     A. 用 DH 双方份额算出对称密钥 K    （机密性）           |
        |     B. 用 known_hosts 里的主机公钥验签名（真实性）          |
        |     ⚠ A 和 B 都必须成功；B 失败就是那句 REMOTE HOST        |
        |       IDENTIFICATION HAS CHANGED，或第一次连接问你 yes/no  |
   ─────+── ⑧ SSH2_MSG_NEWKEYS --------------------------------->|
        |<──────────────────────── ⑨ SSH2_MSG_NEWKEYS ───────────+   换锁完成
        |                                                        |
        |========= 从这一行往下，所有包都被 K 对称加密 =============|
        |                                                        |
        |<──────── ⑩ SSH2_MSG_EXT_INFO: server-sig-algs ─────────+
        |    "我接受这几种签名算法"（决定你的 RSA key 用 SHA-256     |
        |     还是 SHA-512 签；老服务器不认 rsa-sha2-* 会踩坑）      |
   ─────+── ⑪ SSH2_MSG_SERVICE_REQUEST "ssh-userauth" ---------->|
        |<──────────────────── ⑫ SERVICE_ACCEPT ─────────────────+   进入认证阶段
        |                                                        |
        |── ⑬ 询问："这台机器允许哪些认证方法？"                     |
        |<── ⑭ Authentications that can continue:                |
        |        publickey,password                               |
        |                                                        |
        |   客户端挑一个方法。假设是 publickey：                     |
        |── ⑮ "我有这把公钥：<blob>，要不要？"  ← 先只递公钥！        |
        |<── ⑯ 要 → SSH2_MSG_USERAUTH_PK_OK                      |
        |     不要 → 直接告诉你继续换别的（省流量，也省你的私钥口令）  |
        |                                                        |
        |   服务器说"要"，客户端才：                                 |
        |── ⑰ 用【用户私钥】对（会话哈希 + 服务器主机密钥指纹）签名     |
        |     ← 非对称第 2 次；这一步才是"证明我持有私钥"             |
        |                                                        |
        |<── ⑱ 成功 → 开 shell / 执行命令                          |
        |     失败 → "Authentications that can continue: ..."     |
        |            换下一个 key / 下一个方法                      |
        |                                                        |
        |   沙箱演示里所有 key 都不在任何 authorized_keys 上，       |
        |   而且用的是一个本机不存在的用户名 learner，              |
        |   所以走到这里必然是：                                    |
        |     learner@127.0.0.1: Permission denied (publickey,password).
```

### 第三块：known_hosts 到底在防谁

很多人以为 known_hosts 是「记一下这台机器，免得每次问我」。这只说对了一半，而且是不要紧的那一半。

**known_hosts 是 SSH 唯一的「服务器身份凭据」。** 没有它，非对称加密的第 1 件事（证明服务器是谁）就完全失效了。

设想没有 known_hosts 的世界，中间人（能改你网络路径的人，比如咖啡厅路由器、被搞了的交换机、恶意 ARP）这样打你：

```
     你                                        真服务器
     |                                              ^
     |========== SSH 握手 ==========================>|
     |                                              |
     中间的盒子
     |  它同时和你、和服务器各建一条 SSH 连接，         |
     |  两边都用【它自己的】主机密钥签名。              |
     |  对它自己：签名是真的，验得过。                 |
     |  对你：你没有任何参照物知道"真服务器该长什么样"， |
     |  于是你也验过了。                              |
     |                                              |
     |  结果：它同时持有你和它的对称密钥，              |
     |  你的密码、你的命令、你 scp 的文件全是明文。      |
```

known_hosts 存在的唯一目的，就是给「真服务器该长什么样」提供一个**不经过这条网络的**参照物。

那第一次连的时候哪来的参照物？没有。这就是 **TOFU（Trust On First Use，第一次用即信任）**：

```
   第一次连接：没有任何可比对的东西 → ssh 把指纹念给你听，让你【用眼睛/用别的渠道】核对
              → 你说 yes → 这条凭据才落进 known_hosts
   之后每次：拿网络那端递来的主机公钥 和 known_hosts 里存的 比对，不一致就断
```

所以「第一次连接随手按回车」的实际含义是：**你把防中间人的能力外包给了「那一刻你没在偷听者眼皮底下」这个假设**。
家里自己的路由器 + 你自己的手机热点，风险可接受；机场 Wi-Fi 上第一次连一台重要服务器，值得先通过另一个渠道（服务器控制台、云厂商面板、同事）问一句指纹。

**怎么核对指纹？**（这一步不需要连上服务器，任何能读到公开信息的渠道都行）

```bash
# 服务器上执行（root 权限），列出它自己的主机公钥指纹：
ssh-keygen -lf /etc/ssh/ssh_host_ed25519_key.pub

# 你本地核对：known_hosts 里那一行算出来的指纹应该完全一样
ssh-keygen -lf ~/.ssh/known_hosts
```

> 说明：上面第一条**需要你有服务器才能验证**（要登录上去执行）。本机演示不能读 `/etc/ssh/`，
> 但第二条的等价物本章跑过了：把已知的一条 known_hosts 记录用 `ssh-keygen -lf` 算指纹，
> 和 `-vvv` 日志里服务端自报的指纹逐字符相同 —— 见「实战 4」。

### 第四块：`ssh -Q` 就是「我这台机器支持哪些算法」的官方查询口

```bash
ssh -Q kex      # 密钥交换（决定怎么协商出对称密钥）
ssh -Q key      # 主机密钥 / 身份密钥的类型
ssh -Q cipher   # 对称加密
ssh -Q mac      # 完整性校验
ssh -Q sign     # ← 这个不存在！很多人记错了
```

注意：`ssh -Q` 打印的是「**编译进来支持的**」，握手时默认只报其中一部分。差集就是被新版 OpenSSH 主动关掉的（SHA-1 系、`diffie-hellman-group1-sha1`、`3des-cbc`、`hmac-md5` 等）。这个差集本章在实战 2 里现场算给你看。

---

## 场景逐个实战

### 实战 1：先只看两行版本号（真的不用 ssh 命令）

**什么情况**：想确认「这个端口后面是不是 SSH」，或者想理解协议最开头。

**敲什么**

```bash
timeout 5 bash -c 'exec 3<>/dev/tcp/127.0.0.1/22; head -n 1 <&3'
```

（`/dev/tcp/<ip>/<port>` 是 bash 内置的伪设备，不用装 nc；只读一行就退出，不做任何认证。）

**真实输出**

```text
SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
```

**为什么**：SSH 协议最开头是「版本交换」，规定格式是 `SSH-协议版本-软件版本[注释]`，
**完全明文，不加密，不认证**。客户端看到 `OpenSSH` 就会去查一张「兼容开关表」，
决定要不要为这个具体实现打开某些历史包袱 —— 这一步在 `-vvv` 里叫 `compat_banner`。

**再进一步（这个输出很值得看一眼）**：版本行之后紧跟着的第一个包就是算法协商包，**它是明文的**：

```bash
timeout 5 bash -c "exec 3<>/dev/tcp/127.0.0.1/22; head -n 1 <&3 >/dev/null; printf 'SSH-2.0-DemoClient\r\n' >&3; head -c 128 <&3 | strings"
```

真实输出：

```text
mlkem768x25519-sha256,sntrup761x25519-sha512,sntrup761x25519-sha512@openssh.com,curve25519-sha256,curv
```

**这告诉你一件重要的事**：协商阶段（谁支持什么算法）**没有任何机密性**，链路上的旁观者能清楚看到你的客户端支持哪些算法 ——
这也就是为什么「你用什么 SSH 客户端/版本」是可以被网络指纹识别的。真正的机密性从 `NEWKEYS` 之后才开始。

**常见翻车**

- 用工具连着读、又不发自己的版本行 → 服务端等超时，日志里出现 `Timeout before protocol identifier`。
- 看到 `SSH-1.99-...` 以为版本不对 → 1.99 的意思是「我两个版本都支持」，不是版本号写错了。
- 连一个非 SSH 端口（比如 80）→ 拿到一堆 HTTP 头，`Remote protocol version reading failed`。

---

### 实战 2：完整读一遍 `ssh -vvv`（本章主菜）

**敲什么**

```bash
ssh -F /dev/null -o IdentityAgent=none \
    -o UserKnownHostsFile=<沙箱>/known_hosts \
    -o StrictHostKeyChecking=accept-new \
    -vvv -o BatchMode=yes -o IdentitiesOnly=yes \
    -i <沙箱>/keys/id_demo learner@127.0.0.1 'echo hi' </dev/null
```

- `-F /dev/null`：不读你真实的 `~/.ssh/config` 和系统配置（沙箱实验的**第一条铁律**）
- `-o IdentityAgent=none`：不借用你真实的 ssh-agent（**第二条铁律**，见「坑 1」）
- `-o UserKnownHostsFile=`：known_hosts 指到沙箱，不污染真的那份
- `-o BatchMode=yes`：绝不弹密码框（没有 tty 也不会挂在那里等）
- `-o IdentitiesOnly=yes -i <沙箱>key`：只用这把沙箱钥匙
- `learner@`：一个本机**不存在**的中性用户名 —— 这本书的每一脚都故意登录失败，
  用假名比用你的真登录名干净，结论完全一样（见下面第 ⑰ 行附近的说明）

**真实输出（124 行，完整日志在 demo 沙箱里叫 `full_vvv.log`），逐段标注：**

```text
  1  debug1: OpenSSH_10.2p1 Ubuntu-2ubuntu3.6, OpenSSL 3.5.5 27 Jan 2026
  2  debug3: Running on Linux 7.0.0-34-generic ...
  3  debug3: Started with: ssh -F /dev/null -o IdentityAgent=none ... learner@127.0.0.1 "echo hi"
  4  debug1: Reading configuration data /dev/null
```
↑ 本地准备阶段。`Started with` 那行很有用：它把**最终**展开的命令行原样打了一份，排查时先确认这里。
`Reading configuration data` 出现几次，就代表读了几个配置文件 —— 这一行是判断「配置到底有没有被读到」的最快方法。

```text
  8  debug1: Connecting to 127.0.0.1 [127.0.0.1] port 22.
 10  debug1: Connection established.
 11  debug1: loaded pubkey from <沙箱>/keys/id_demo: ED25519 SHA256:dlFPNzZBheCvaTtV/AIezF1+5HwokOFdxQbpG51D7pI
 12  debug1: identity file <沙箱>/keys/id_demo type 2
 14  debug1: Local version string SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
 15  debug1: Remote protocol version 2.0, remote software version OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
 16  debug1: compat_banner: match: OpenSSH_10.2p1 ... pat OpenSSH* compat 0x04000000
```
↑ TCP 通了 → 先把要用的身份公钥读进内存（注意：**这里读的是公钥，还没碰私钥**）→ 版本交换。
`compat_banner` 那行说明「对方是 OpenSSH，兼容开关设成 0x04000000」。你连老古董服务器时这行会变成别的 pattern，
那些古怪的 compat 位就是「为了迁就某个实现的 bug 而特意打开的行为差异」。

```text
 18  debug1: Authenticating to 127.0.0.1:22 as 'learner'
```
↑ 注意这个位置：**在 KEX 之前**，ssh 就已经把「我打算用哪个用户名登录」定下来了。
所以 `Permission denied (publickey,password)` 里那个用户名，跟认证方式无关，先看清是不是**用户名本身不对**
（`ssh 127.0.0.1` 不加 user@ 时用的是你本地用户名，这是新手最常见的第一个错）。

这一行的 `learner` 是 demo 特意写的**中性用户名**（本机不存在这个用户）：不写 `user@` 时 ssh 会拿你的
登录名去登，那等于把你的真实姓名印进日志。换成一个不存在的用户，本章结论一个字都不变（照样停在
`Permission denied`，KEX 阶段连用户名都用不上）。**你跑 demo 会在这一行看到 `learner`，但你自己在终端
不加 user@ 跑同一条命令，这里显示的就是你自己的登录名** —— 这正是要教的那个坑。

```text
 22  debug3: send packet: type 20
 23  debug1: SSH2_MSG_KEXINIT sent
 24  debug3: receive packet: type 20
 25  debug1: SSH2_MSG_KEXINIT received
 26  debug2: local client KEXINIT proposal
 27  debug2: KEX algorithms: mlkem768x25519-sha256,sntrup761x25519-sha512,...,ext-info-c,kex-strict-c-v00@openssh.com
 28  debug2: host key algorithms: ssh-ed25519-cert-v01@openssh.com,...,ssh-ed25519,ecdsa-sha2-nistp256,...,rsa-sha2-512,rsa-sha2-256
 29  debug2: ciphers ctos: chacha20-poly1305@openssh.com,aes128-gcm@openssh.com,aes256-gcm@openssh.com,aes128-ctr,aes192-ctr,aes256-ctr
 ...
 39  debug2: peer server KEXINIT proposal
 40  debug2: KEX algorithms: mlkem768x25519-sha256,sntrup761x25519-sha512,...,ext-info-s,kex-strict-s-v00@openssh.com
 41  debug2: host key algorithms: rsa-sha2-512,rsa-sha2-256,ecdsa-sha2-nistp256,ssh-ed25519
```
↑ 算法协商。读法：

- `ctos` = client to server，`stoc` = server to client。两方向可以不一样（服务器常给自己那侧留更强的算法）。
- 清单的**顺序就是偏好顺序**，第一个双方都在的算法定成交价。所以「想让某算法优先」= 把它放前面；
  `-o KexAlgorithms=...` 会**整张覆盖**，`+` 才是追加，`-` 是排除。
- `ext-info-c` / `ext-info-s` 是伪算法名，作用只是「我支持 EXT_INFO，等下会给你发扩展信息」。别去 `ssh -Q` 里找它。
- `kex-strict-c-v00@openssh.com` 也出现在 KEX 列表里，看着别扭，但这是**协议设计**：strict kex 必须
  「双方都在 KEX 清单里声明了它」才启用，否则老实现对包序号的处理会立刻把连接判成攻击。
  下一行 `debug3: kex_choose_conf: will use strict KEX ordering` 才是启用判决的地方。
- 客户端的 host key 列表里全是 `*-cert-v01@openssh.com` 排在前面 —— 那是「如果对方拿证书来我也收」；
  服务端列表里没有 cert，因为这台 sshd 没配 CA 签的主机证书。**这就是证书方案在两边各是哪几行的样子。**

```text
 52  debug3: kex_choose_conf: will use strict KEX ordering
 53  debug1: kex: algorithm: mlkem768x25519-sha256
 54  debug1: kex: host key algorithm: ssh-ed25519
 55  debug1: kex: server->client cipher: chacha20-poly1305@openssh.com MAC: <implicit> compression: none
 56  debug1: kex: client->server cipher: chacha20-poly1305@openssh.com MAC: <implicit> compression: none
```
↑ **成交结果，四行看完就懂整场协商。** 三个值得注意的点：

1. `mlkem768x25519-sha256` 被选中了 —— 这是**后量子混合 KEX**（ML-KEM/kyber + X25519 各出一半）。
   也就是说你这台 10.2 的 OpenSSH 已经在默认走抗量子密钥协商了。
   它排在客户端和服务端两清单的最前面，所以第一顺位就成交。
2. `MAC: <implicit>` 不是「没配 MAC」。chacha20-poly1305 和 *-gcm 这类是 **AEAD 算法**（认证加密），
   加密和防篡改是**同一套数学一起做的**，所以没有独立的 MAC。
   只有 CBC/CTR 这种只加密不防篡改的算法才需要外挂 `hmac-sha2-256` 之类的独立 MAC。
   这一行是理解「为什么现在的默认列表里几乎没有 HMAC」的钥匙。
3. `compression: none` 在最前面。别急着加 `-C`：加密过的数据没有冗余可压，压缩带来的收益小，
   而且历史上出过 `CRIME`/`BREACH` 这一类「压缩 + 攻击者可控明文」的泄露攻击。
   只在「传纯文本且带宽真的很差」时才开压缩。

```text
 57  debug3: send packet: type 30
 58  debug1: expecting SSH2_MSG_KEX_ECDH_REPLY
 59  debug3: receive packet: type 31
 61  debug1: Server host key: ssh-ed25519 SHA256:ZiBpW4VGzrtnDYcX80tWmYPtY6K9jw4OzMVPXmDRk4w
 62  debug1: load_hostkeys: fopen /etc/ssh/ssh_known_hosts: No such file or directory
 64  debug3: hostkeys_find_by_key_hostfile: trying user hostfile "<沙箱>/known_hosts"
 65  debug3: hostkeys_foreach: reading file "<沙箱>/known_hosts"
 66  Warning: Permanently added '127.0.0.1' (ED25519) to the list of known hosts.
```
↑ 服务端递来了「主机公钥 + DH 份额 + 用主机私钥签的名」（type 31 一个包里全含）。
`Server host key:` 这行打出的**就是服务端自报的公钥指纹**，`known_hosts` 要比对的是它。
注意 62~65 的查找顺序：**先看系统级 `/etc/ssh/ssh_known_hosts`（没有）**，再看用户级。
（这个顺序对排查很有用：公司统一分发主机密钥就是走系统级那份。）

如果**没找到任何可比对的记录**，行为由 `StrictHostKeyChecking` 决定，共三种：

| 取值 | 行为 | 场景 |
|---|---|---|
| `ask`（默认） | 交互式问你要不要记；非交互环境（CI、脚本）里问不到就**直接失败** | 人肉用 |
| `accept-new` | 不认识就自动记，**认识但不一致仍然报致命错误** | 自动化脚本的正确选择 |
| `no` | 不认识就自动记，**不一致也只是警告然后照连** | ⚠ 关闭了服务器身份验证，只适合内网玩具机 |

> 这里有个很多人不知道的差别：`accept-new` 是 `no` 的安全替代版，它保留了「变了就断」的能力。
> 想自动化又不敢关安全的，永远用 `accept-new`，不要用 `no`。

```text
 65  debug3: send packet: type 21
 66  debug1: ssh_packet_send2_wrapped: resetting send seqnr 3
 68  debug1: rekey out after 134217728 blocks
 69  debug1: SSH2_MSG_NEWKEYS sent
 71  debug1: expecting SSH2_MSG_NEWKEYS
 75  debug1: SSH2_MSG_NEWKEYS received
 77  debug1: rekey in after 134217728 blocks
```
↑ **换锁点。** 从这里开始，双方都改用刚协商出的对称密钥。
`resetting send seqnr 3` 是在重置包序号计数（序号本身不能作为明文泄露给窃听者）。
`rekey ... after 134217728 blocks` = 2³¹ 块（每块 64KB）后自动重新做一次握手换密钥 —— 这是密码学上的
「同一把密钥加密的数据量不能超过某个界限」的要求，不是性能设置。会话很久时你会在日志里再看到一轮 KEX，
那叫 **rekey（重新密钥）**，是完全正常的，不要当成断线。

```text
 82  debug1: Sending SSH2_MSG_EXT_INFO
 92  debug3: receive packet: type 7
 93  debug1: SSH2_MSG_EXT_INFO received
 94  debug3: kex_input_ext_info: extension server-sig-algs
 95  debug1: kex_ext_info_client_parse: server-sig-algs=<ssh-ed25519,ecdsa-sha2-nistp256,ecdsa-sha2-nistp384,
      ecdsa-sha2-nistp521,sk-ssh-ed25519@openssh.com,sk-ecdsa-sha2-nistp256@openssh.com,rsa-sha2-512,rsa-sha2-256>
 95  debug3: kex_input_ext_info: extension publickey-hostbound@openssh.com
 97  debug3: kex_input_ext_info: extension ping@openssh.com
```
↑ **`server-sig-algs` 这一行，是「我的 RSA key 为什么登录不了老服务器」这个千古谜题的答案所在。**

它的意思是：**「在用户认证阶段，我（服务端）接受下面这几种签名算法」**。
注意它和 KEX 里的 host key 清单**是两张不同的表**：一张管「服务器怎么证明自己是服务器」，
一张管「用户怎么证明自己是用户」。别混。

对 RSA 用户密钥的影响最实际：RSA 密钥本身只有一种（`ssh-rsa`），但**签名用的哈希**可以是 SHA-1（老的 `ssh-rsa`）
或 SHA-256/SHA-512（`rsa-sha2-256` / `rsa-sha2-512`）。OpenSSH 8.8 起客户端**不再用 SHA-1 签**，
改为看这个 `server-sig-algs` 列表挑一个双方都支持的。于是：

- 现代服务器 ↔ 现代客户端：列表里有 `rsa-sha2-256/512` → 一切正常。
- **老 OpenSSH（< 7.2）服务器**：压根不发 EXT_INFO → 客户端**看不到这个扩展** →
  对 RSA key 只能退回 `ssh-rsa`(SHA-1)，而你的新版客户端又拒绝 SHA-1 → 症状是
  「公钥明明装了，却一句提示都没有就 `Permission denied`」。
  **解法不是去开 SHA-1，是换一把 ed25519 key**（见 02 章）。

`publickey-hostbound@openssh.com=<0>` 是较新扩展，作用是把用户签名和「这条具体 TCP 会话」绑死，
防止你的 agent 被别的进程滥用（host-bound 公钥认证）。看到 `<0>` 表示版本 0。

```text
 99  debug3: receive packet: type 6
100  debug2: service_accept: ssh-userauth
101  debug1: SSH2_MSG_SERVICE_ACCEPT received
108  debug1: Authentications that can continue: publickey,password
109  debug3: start over, passed a different list publickey,password
110  debug3: preferred publickey
113  debug3: authmethod_is_enabled publickey
114  debug1: Next authentication method: publickey
115  debug1: Will attempt key: <沙箱>/keys/id_demo ED25519 SHA256:dlFPNzZBheCvaTtV/AIezF1+5HwokOFdxQbpG51D7pI explicit
117  debug1: Offering public key: <沙箱>/keys/id_demo ED25519 SHA256:dlFPNzZBheCvaTtV/AIezF1+5HwokOFdxQbpG51D7pI explicit
118  debug3: send packet: type 50
119  debug2: we sent a publickey packet, wait for reply
120  debug3: receive packet: type 51
121  debug1: Authentications that can continue: publickey,password
122  debug2: we did not send a packet, disable method
123  debug1: No more authentication methods to try.
124  learner@127.0.0.1: Permission denied (publickey,password).
```
↑ 认证阶段，几个必须理解的细节：

- 客户端会**主动问一遍**「你支持哪些方法」，服务端答 `publickey,password`。这行是**排错第一优先要看的东西**：
  它说的是**服务器允许什么**，不是你的客户端能做什么。
  比如服务器上 `PasswordAuthentication no` 时，这里就只剩 `publickey`，你就算想输密码也没地方输。
- `110 debug3: preferred publickey` ← 这行是**你的客户端本地**决定的尝试顺序（来自 `PubkeyAuthentication`
  / `PasswordAuthentication` / `KbdInteractiveAuthentication` 三个开关 + 内置优先级）。
  「服务端允许」和「你本地愿意」两个集合取交集，才是真正会走的路径。
- `115 Will attempt key` vs `117 Offering public key`：**Will attempt 是排进队列，Offering 是真发出去了。**
  中间可能还夹着 `Trying private key: ... / no such identity`（文件不存在，静默跳过）。
  这个区分在「配了 5 把 key 结果一把都没递出去」的排查里非常关键。
- **`Offering public key` 只递公钥，不递私钥、也不签名。** 服务器说「这把我不认」（回 type 51），
  这一把的开销几乎为零 —— 所以 agent 里有 100 把 key 的真正代价不是算力，而是**认证尝试次数**
  （服务端 `MaxAuthTries` 默认 6，超了直接踢，见 03 章的 `IdentitiesOnly`）。
- `explicit` 这个尾巴表示「这把 key 是你用 `-i` 或 `IdentityFile` 明确指定的」；
  另一种常见尾巴是 `agent`（从 ssh-agent 拿到的）和 `config line N`。

**⚠ 需要你有真实服务器才能验证的部分**：上面 115→118 之后，正常应当出现

```text
debug1: Server accepts key: /path/id_ed25519 ED25519 ...
debug1: Signing challenge using ... key ...
debug1: send_pubkey_test: publickey packet sent
debug1: Authentication succeeded (publickey).
```

**这两行（`Server accepts key` / `Authenticated to ...`)在本机演示里拿不到**，因为我们不会往任何机器装公钥。
你连自己服务器时应该能看到它们 —— 如果你只看到 `Offering public key` 然后就 `Permission denied`，
说明服务器**在第一步就拒了这把公钥**，那要去查的是 `authorized_keys` 内容/权限/`StrictModes`，
而不是你的私钥。（这是「服务器根本没认这把钥匙」和「认了但签名过程失败」的分界线，价值极高。）

**常见翻车**

- 只看最后一行 `Permission denied` 就断定「密码错了」。正确做法：先看 `Authenticating to ... as 'xxx'` 那行的**用户名**。
- `-vvv` 一屏找不到重点：先 grep 四个锚点
  `grep -nE 'kex: algorithm|Server host key:|Authentications that can continue|Offering public key' 日志`
- 在 CI 里跑 `-vvv` 忘了 `-o BatchMode=yes` → 进程卡在密码提示上，表现为「超时」而不是「认证失败」，非常误导。

---

### 实战 3：现场算出「编译支持但默认禁用」的算法差集

**敲什么**

```bash
ssh -Q kex                                                   # 编译支持的
grep -m1 '^debug2: KEX algorithms' full_vvv.log              # 本次实际报价的
comm -13 <(报价表 | tr ',' '\n' | sort) <(ssh -Q kex | sort) # 差集
```

**真实输出**

demo 里这几条是 `ssh -Q <类> | tr '\n' ' '`，所以**每一类的输出在屏幕上都是一整行**；
README 下面照原样保留长行，不为了排版折行（折了就和捕获对不上了）。

```text
$ ssh -Q kex | tr '\n' ' '
diffie-hellman-group1-sha1 diffie-hellman-group14-sha1 diffie-hellman-group14-sha256 diffie-hellman-group16-sha512 diffie-hellman-group18-sha512 diffie-hellman-group-exchange-sha1 diffie-hellman-group-exchange-sha256 ecdh-sha2-nistp256 ecdh-sha2-nistp384 ecdh-sha2-nistp521 curve25519-sha256 curve25519-sha256@libssh.org sntrup761x25519-sha512 sntrup761x25519-sha512@openssh.com mlkem768x25519-sha256
```

```text
$ ssh -Q cipher | tr '\n' ' '
3des-cbc aes128-cbc aes192-cbc aes256-cbc aes128-ctr aes192-ctr aes256-ctr aes128-gcm@openssh.com aes256-gcm@openssh.com chacha20-poly1305@openssh.com
```

```text
$ ssh -Q mac | tr '\n' ' '
hmac-sha1 hmac-sha1-96 hmac-sha2-256 hmac-sha2-512 hmac-md5 hmac-md5-96 umac-64@openssh.com umac-128@openssh.com hmac-sha1-etm@openssh.com hmac-sha1-96-etm@openssh.com hmac-sha2-256-etm@openssh.com hmac-sha2-512-etm@openssh.com hmac-md5-etm@openssh.com hmac-md5-96-etm@openssh.com umac-64-etm@openssh.com umac-128-etm@openssh.com
```

```text
$ ssh -Q key | tr '\n' ' '
ssh-ed25519 ssh-ed25519-cert-v01@openssh.com sk-ssh-ed25519@openssh.com sk-ssh-ed25519-cert-v01@openssh.com ecdsa-sha2-nistp256 ecdsa-sha2-nistp256-cert-v01@openssh.com ecdsa-sha2-nistp384 ecdsa-sha2-nistp384-cert-v01@openssh.com ecdsa-sha2-nistp521 ecdsa-sha2-nistp521-cert-v01@openssh.com sk-ecdsa-sha2-nistp256@openssh.com sk-ecdsa-sha2-nistp256-cert-v01@openssh.com ssh-rsa ssh-rsa-cert-v01@openssh.com
```

实际报价（本机握手）：

```text
$ printf '本次握手客户端实际报价：\n'; grep -m1 '^debug2: KEX algorithms' <沙箱>/full_vvv.log | sed 's/^debug2: KEX algorithms: //' | tr ',' '\n' | grep -vE 'ext-info|kex-strict' | tr '\n' ' '; echo
本次握手客户端实际报价：
mlkem768x25519-sha256 sntrup761x25519-sha512 sntrup761x25519-sha512@openssh.com curve25519-sha256 curve25519-sha256@libssh.org ecdh-sha2-nistp256 ecdh-sha2-nistp384 ecdh-sha2-nistp521 diffie-hellman-group-exchange-sha256 diffie-hellman-group16-sha512 diffie-hellman-group18-sha512 diffie-hellman-group14-sha256
```

差集那一条（demo 里是 `comm -13 <(报价表|tr ',' '\n'|sort) <(ssh -Q kex|sort)`）：

```text
$ printf '\n编译支持但默认不报价的：\n'; comm -13 <(grep -m1 '^debug2: KEX algorithms' <沙箱>/full_vvv.log | sed 's/^debug2: KEX algorithms: //' | tr ',' '\n' | sort) <(ssh -Q kex | sort) | tr '\n' ' '; echo

编译支持但默认不报价的：
comm: file 1 is not in sorted order
comm: file 2 is not in sorted order
comm: input is not in sorted order
diffie-hellman-group14-sha1 diffie-hellman-group1-sha1 diffie-hellman-group-exchange-sha1 diffie-hellman-group-exchange-sha256
```

⚠ 上面这三行 `comm: ... not in sorted order` **不是噪音，是这一节的第二个知识点**：
`comm` 要求两个输入都按 `LC_ALL=C` 的字节序排好，而 `sort` 默认按 locale 排序，
两边顺序不一致时 `comm` 的结果就不该全信 —— 输出里因此混进了
`diffie-hellman-group-exchange-sha256`（它其实**在**报价表里，是个假阳性）。
真要看差集，写 `comm -13 <(... | LC_ALL=C sort) <(ssh -Q kex | LC_ALL=C sort)`。

**结论**：`diffie-hellman-group1-sha1`、`diffie-hellman-group14-sha1`、`diffie-hellman-group-exchange-sha1`
在 `-Q` 里查得到，但**默认不报价**。对称算法那边，`3des-cbc` 和所有 `*-cbc` 同理；
MAC 那边 `hmac-md5*`、`hmac-sha1*`（非 -etm 版）默认也不在列表里。
**这就是「老服务器连不上」的数学根源**：不是网络问题，是双方的清单交集变成了空集。

**常见翻车**：拿 `ssh -Q` 的输出当「我的机器允许用这些」→ 不是，那是「编译支持的」。

---

### 实战 4：known_hosts 的三种真结果（全部本机可跑）

#### 4a. 第一次连接会问你（TOFU 原话）

```text
The authenticity of host '127.0.0.1 (127.0.0.1)' can't be established.
ED25519 key fingerprint is: SHA256:ZiBpW4VGzrtnDYcX80tWmYPtY6K9jw4OzMVPXmDRk4w
This key is not known by any other names.
Are you sure you want to continue connecting (yes/no/[fingerprint])?
```

**这一屏是本章最重要的东西。** 拆读：

- `127.0.0.1 (127.0.0.1)`：前一个是**你在命令行敲的名字**，括号里是**解析出来的地址**。
  两个不一样时值得警惕（DNS 劫持 / hosts 文件被动过）。
- `ED25519 key fingerprint is: SHA256:...`：只给你看**指纹**，不给你看公钥全文 —— 因为人核对不了 68 个字符的 base64。
- `(yes/no/[fingerprint])?`：第三种答案允许你**直接输入期望的指纹**，它会自动比对；
  也就是说：如果云厂商在你另一个终端里贴了 `SHA256:ZiBpW4...`，你可以把指纹当成答案打进去，
  一致就记下来 —— 这比「盲按 yes」强，因为核对动作被强制做了。
- `This key is not known by any other names.`：在提示你「这个主机可能有别的名字（别名/CNAME/IP），
  它们各自的记录都没有」。如果你用多个别名连同一台机器，这条就会出现。

回答 `no` 之后，真实结果：那份 known_hosts（demo 里叫 `kh_first`）**仍然是 0 字节**：

```text
$ wc -c < <沙箱>/kh_first
0
```

#### 4b. `accept-new` 写下记录，指纹和服务端自报的一致

```text
$ ssh -F /dev/null -o IdentityAgent=none -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh_first learner@127.0.0.1 true
Warning: Permanently added '127.0.0.1' (ED25519) to the list of known hosts.
learner@127.0.0.1: Permission denied (publickey,password).

$ cat <沙箱>/kh_first
127.0.0.1 ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIJ8COfYZlWVtvsITvesDuL0e7PzZiQCCKeFi9lLjMMhY

$ ssh-keygen -lf <沙箱>/kh_first
256 SHA256:ZiBpW4VGzrtnDYcX80tWmYPtY6K9jw4OzMVPXmDRk4w 127.0.0.1 (ED25519)
```

对照 `-vvv` 里的 `Server host key: ssh-ed25519 SHA256:ZiBpW4VGzrtnDYcX80tWmYPtY6K9jw4OzMVPXmDRk4w` —— **一模一样**。
`ssh-keygen -lf` 能吃 known_hosts，很多人不知道。

known_hosts 一行的结构：

```
127.0.0.1    ssh-ed25519   AAAAC3Nza...
└ 主机匹配名  └ 密钥类型     └ 公钥 blob(base64)
```

#### 4c. known_hosts 和服务器实际密钥不一致 —— 那个吓人的框

我在沙箱 known_hosts 里放了一把**不属于这台服务器**的公钥，然后连：

```text
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@    WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!     @
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
IT IS POSSIBLE THAT SOMEONE IS DOING SOMETHING NASTY!
Someone could be eavesdropping on you right now (man-in-the-middle attack)!
It is also possible that a host key has just been changed.
The fingerprint for the ED25519 key sent by the remote host is
SHA256:ZiBpW4VGzrtnDYcX80tWmYPtY6K9jw4OzMVPXmDRk4w.
Please contact your system administrator.
Add correct host key in <沙箱>/kh_wrong to get rid of this message.
Offending ED25519 key in <沙箱>/kh_wrong:1
  remove with:
  ssh-keygen -f '<沙箱>/kh_wrong' -R '127.0.0.1'
Host key for 127.0.0.1 has changed and you have requested strict checking.
Host key verification failed.
```

**读法（这三行信息量最大）**：

- `The fingerprint ... sent by the remote host is SHA256:ZiBp...` —— **新的**是什么
- `Offending ED25519 key in <文件>:1` —— **旧的**记录在哪个文件的第几行
- `remove with: ssh-keygen -f ... -R ...` —— **ssh 已经把修复命令替你写好了**，复制执行即可

**执行它，一条就活过来**（这是真的，本机跑过）：

```text
$ ssh-keygen -f <沙箱>/kh_wrong -R 127.0.0.1 | tail -3
# Host 127.0.0.1 found: line 1
<沙箱>/kh_wrong updated.
Original contents retained as <沙箱>/kh_wrong.old

$ cat <沙箱>/kh_wrong
```

`cat` 那一行**没有任何输出** —— 那条记录被删掉了，下次连接会重新走一遍 TOFU。

**判断是「服务器换密钥」还是「真被中间人了」**：

| 线索 | 更像换密钥（ benign ） | 更像中间人（ malignant ） |
|---|---|---|
| 最近有没有重装/换镜像/重建云实例 | 有 → 正常 | — |
| 只有一台机器报，还是**一批**都报 | 一台 | ⚠ 一批同时报（说明你的网络路径被改了） |
| 换过的指纹能不能在**另一条渠道**上确认 | 能（面板/控制台/同事） | 不能 |
| 有没有同时出现别的算法异常 | 无 | ⚠ 有（比如突然降级到老算法） |
| 你此刻在什么网络 | 家里/公司有线 | ⚠ 陌生 Wi-Fi / 代理 |

不确定时的正确动作：**换个网络（手机热点）再连一次**，看指纹是否和之前一致；不要用 `-o StrictHostKeyChecking=no` 「先把活干完」。

---

### 实战 5：`ssh-keyscan` —— 它抓到的东西没有任何信任含量

**敲什么**

```bash
ssh-keyscan -t ed25519,rsa,ecdsa 127.0.0.1
```

**真实输出**

```text
# 127.0.0.1:22 SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
127.0.0.1 ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABgQCv0L/nyt70qt48pb8NnYSiqad3b2hC98Tu7...
# 127.0.0.1:22 SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
127.0.0.1 ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBA...
# 127.0.0.1:22 SSH-2.0-OpenSSH_127... (略)
127.0.0.1 ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIJ8COfYZlWVtvsITvesDuL0e7PzZiQCCKeFi9lLjMMhY
```

以 `#` 开头的是**注释行**（它顺便把版本行打出来了，方便你识别设备类型），可以直接 `grep -v '^#'` 丢掉。

**它做的事情，用一句话说**：开一条 TCP 连接，做一次 KEX 的**前半段**（拿到服务器自报的公钥），
然后**不做任何校验**地把这个公钥按 known_hosts 的格式打印出来。

**所以「把 keyscan 结果直接写进 known_hosts」= 放弃了 SSH 的整个服务器身份保障**：

```text
$ ssh -F /dev/null -o IdentityAgent=none -o BatchMode=yes -o UserKnownHostsFile=<沙箱>/kh_scan learner@127.0.0.1 true
learner@127.0.0.1: Permission denied (publickey,password).
```

（`<沙箱>/kh_scan` 就是上面那条 `ssh-keyscan -t ed25519 127.0.0.1` 的产物 ——
demo 里把它包进了带重试的 `ksc`，所以捕获里那行写的是 `ksc ed25519 <沙箱>/kh_scan`，见章末说明。）

看到没：**服务器身份这一关无声通过了**，直接走到认证阶段 —— 因为凭据是我在「同一条可能被劫持的通道」上自己抄来的。
keyscan 和正常连接用的是**同一条网络路径**，中间人递什么它就记什么，而且它连指纹都不给你看一眼的机会。

**那 keyscan 到底什么时候能用？**

| 用法 | 安全吗 | 说明 |
|---|---|---|
| 内网自建、机器可控、只求「别问我 yes/no」 | 可接受 | 风险≈0（同二层内网），CI 里常见 |
| `keyscan` + 事后用**独立渠道**核对指纹 | 好 | 记下来，但拿真实凭据验证过 |
| 直接 keyscan 公网机器 + 不核对 | **别** | 等价于 `StrictHostKeyChecking=no`，只是伪装成了"我做了 known_hosts" |
| `keyscan >> known_hosts` 当日常操作 | **别** | 每次抓都覆盖一遍，等于永远不会有「变了就报警」的保护 |

#### 顺手把 `SHA256:` 指纹、`SSHFP`、哈希 known_hosts 三件事讲清

**① `SHA256:` base64 指纹**

`ssh-keygen -lf` 打出来的就是它：**公钥 blob 的 SHA-256 摘要，做 base64（去掉 `=` 填充），前面加 `SHA256:`**。
它是「一把公钥的短名字」，用来**人眼比对**，不是拿来当身份凭据用的（不能从指纹反推公钥）。

```text
$ while read -r h type blob; do printf '%-24s' "$type"; printf '%s %s\n' "$type" "$blob" > <沙箱>/one.pub; ssh-keygen -lf <沙箱>/one.pub | awk '{print $2}'; done < <沙箱>/scan_all.txt
ssh-rsa                 SHA256:ibzNAzta/QjJNuVM2RQGy3c4uS05qeQPzemT1k1vkm0
ecdsa-sha2-nistp256     SHA256:wVKJVKNo+aN5wg7EESi5pHk57rucFS0908iFRR6zHuc
ssh-ed25519             SHA256:ZiBpW4VGzrtnDYcX80tWmYPtY6K9jw4OzMVPXmDRk4w
```

一台机器可以有**多把主机密钥**（rsa / ecdsa / ed25519 各一把），也就是**多个合法指纹**。
所以核对时先确认对方给你的是哪种类型的指纹，别拿 RSA 的去比 Ed25519 的然后判定"被攻击了"。

**② SSHFP（DNS 里的 SSH 指纹记录，RFC 4255）**

思路：把主机公钥的指纹作为 DNS 记录发布（类型 `SSHFP`），客户端配 `VerifyHostKeyDNS yes`，
用 DNS 作为**第二条独立渠道**来验证主机密钥，这样第一次连接也能自动校验。

```text
# 记录格式（需要你在真实 DNS 上配，本机无法验证）：
<hostname>. IN SSHFP <算法> <指纹类型> <十六进制指纹>
        算法:     1=RSA 2=DSS 3=ECDSA 4=Ed25519
        指纹类型: 1=SHA-1 2=SHA-256
# 生成要贴到 DNS 的那几行（ssh-keygen 本地就能出，这一步可离线验证）：
ssh-keygen -r <主机名>            # 默认 SHA-1
ssh-keygen -r <主机名> -f /etc/ssh/ssh_host_ed25519_key.pub -a 2   # 用 SHA-256
```

> ⚠ **这段需要你自己有服务器 + 自建 DNS 才能验证。** 关键陷阱一句话：
> **SSHFP 只有在 DNS 被 DNSSEC 签名保护时才有安全价值**。没有 DNSSEC 的 SSHFP 是
> 「把可被伪造的数据放到另一个同样可被伪造的通道上」，等于没做。
> 所以 SSHFP 在实践里很少用，公司环境更常见的是**证书（`@cert-authority`）**方案。

**③ 哈希 known_hosts（`HashKnownHosts yes` / `ssh-keygen -H`）**

known_hosts 是明文，里面就是一份「你访问过哪些机器」的清单 ——
这在多用户机器/被取证场景里算隐私泄露。哈希后的样子：

```text
$ cp <沙箱>/kh3 <沙箱>/kh_hash; ssh-keygen -H -f <沙箱>/kh_hash; head -2 <沙箱>/kh_hash
WARNING: <沙箱>/kh_hash.old contains unhashed entries
Delete this file to ensure privacy of hostnames
<沙箱>/kh_hash updated.
Original contents retained as <沙箱>/kh_hash.old
# 127.0.0.1:22 SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
|1|cOapg+mV45K6fjmJT765ZonkzpE=|Le++3wUomBhr+PA6ZMIYfYejPsY= ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIJ8COfYZlWVtvsITvesDuL0e7PzZiQCCKeFi9lLjMMhY
```

> 来源说明：**本章 demo 没跑 `-H`**（本章只管握手），上面这段是**第 02 章 demo 场景 7** 的真跑原文
> （`~/.cache/handbook-captures/ssh02.txt`），路径里的沙箱名折成了 `<沙箱>`。
> 那一行 `|1|…|…=` 里的两段 base64（盐 + HMAC）**每次跑都不一样**，所以它对本章捕获文件来说是「跨章 + 随机」两类各占一半。

- `|1|<盐 b64>|<HMAC-SHA1(盐, 主机名) b64>` —— 两个字段都 base64（这就是 `SSHfp`/`SHA256:` 之外的第三个 base64 场景区别）
- 哈希的是**主机名**，不是公钥。所以：**它防的是"别人读你的 known_hosts 看出你连过哪些机器"，
  不防中间人**（比对逻辑一样，只是无法反向推出主机名）。
- 副作用一：**你不能再 `grep 主机名 known_hosts` 了**，要用 `ssh-keygen -f known_hosts -F 主机名`。
  同一份哈希文件仍查得到（02 章 demo 里紧跟着跑的那条就是证据）：

```text
$ ssh-keygen -F 127.0.0.1 -f <沙箱>/kh_hash | head -2
# Host 127.0.0.1 found: line 2 
|1|cOapg+mV45K6fjmJT765ZonkzpE=|Le++3wUomBhr+PA6ZMIYfYejPsY= ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIJ8COfYZlWVtvsITvesDuL0e7PzZiQCCKeFi9lLjMMhY
```

- 副作用二：**每行只对应一个主机名**。同一台机器有 IP 又有域名时，会写成同一行里多个 `|1|...|...` 用逗号分隔；
  工具（Ansible 之类）直接读写 known_hosts 时特别容易踩这个。
- 上面 `-H` 时还打印了一条很实用的警告（就是围栏里那两行 `WARNING`）：
  哈希了主文件，但**备份文件 `<沙箱>/kh_hash.old` 还是明文的**，等于没哈希 —— 这条警告经常被忽略，
  它的意思就是「想彻底打码，把 `.old` 也删掉」。

---

### 实战 6：公钥加密 ≠ 签名（这一节专门纠正那个误区）

**误区长这样**：「登录时我的客户端用私钥加密一段密码/凭据，发过去，服务器用我的公钥解密，于是就知道是我了。」

**这句话有四个错**：
1. 登录时**没有任何密码被发送**（publickey 方式下）。
2. 私钥不是用来「加密要发给别人的内容」的。
3. 公钥能验签，**不代表谁能解密你的数据**。
4. 方向反了 —— 真实过程是**服务器出题，你答题**。

**正确的方向**：

```
   你要证明的命题：「我手里有私钥」
   你不能用的做法：把私钥发出去（那就不是证明，是送人）

   于是：
   服务器 ──"会话哈希 H"──> 你          （H 里含本次会话独有的一堆数据 +
   你                                      known_hosts 里那把主机公钥的哈希）
   你：用私钥对 H 签名 ─────────────> 服务器
                                        服务器：用 authorized_keys 里的公钥验签
                                        验过了 = 你确实持有私钥，
                                                而且这个签名只对【本次会话】有效
```

**"只对这次会话有效"这一点是精髓**：签名内容 H 里绑进了本次握手的随机数与主机密钥，
所以窃听者就算抓到你的一次签名，**也不能拿它去登录下一次**（重放无效）。
如果 SSH 真是"发一段固定的加密凭据"，那被抓一次就永久失陷 —— 那种设计叫
*"password-equivalent"*，是被刻意避免的。

**不用连服务器也能亲手体验一遍这个方向**（本机实跑，全程离线）：

```bash
ssh-keygen -t ed25519 -C mine -f ./mine  -N ''    # 造两把 key
ssh-keygen -t ed25519 -C other -f ./other -N ''
printf 'session-log line 1\ntransfered 42 files\n' > payload.txt
ssh-keygen -Y sign -n file-verify -f ./mine payload.txt       # 私钥签
ssh-keygen -Y check-novalidate -n file-verify -f ./mine.pub -s payload.txt.sig < payload.txt
```

真实输出，四个实验一次看完「签名」的性质：

```text
# ① 正确的私钥签 + 正确的公钥验
Good "file-verify" signature with ED25519 key SHA256:q4H1d9ua2dKDFNkAEiNw1AgVT+jIygk7ZQ6WUt9kO+U

# ② 内容改一个字节，再用正确的公钥验（payload 里 line 1 → line X）
Signature verification failed: incorrect signature
Could not verify signature.

# ③ 内容不动，把用途标签从 file-verify 换成别的
Couldn't verify signature: namespace does not match
Could not verify signature.

# ④ 用【别人的公钥】问同一份签名 —— 注意，它说 Good！
Good "file-verify" signature with ED25519 key SHA256:q4H1d9ua2dKDFNkAEiNw1AgVT+jIygk7ZQ6WUt9kO+U
```

- ② 说明签名保的是**完整性 + 来源**，一个比特都不能改。
- ③ 说明签名**绑定了用途**（namespace）。SSH 登录用的 namespace 是 `"server auth"` / `"client auth"`，
  所以"给服务器签名用的东西"不能被搬到"给文件签名"的场景里复用 —— 这就是防跨场景滥用的机制。
- ④ **最重要的一个坑**：`check-novalidate` 顾名思义「不做身份验证」，它只检查签名自不自洽（签名 blob 里
  本来就带着公钥），**不检查这把公钥是不是你指定的那把**。
  真正要查「是谁签的」必须用 `ssh-keygen -Y verify` + `allowed_signers` 名单：

```text
$ printf 'mine-id  ssh-ed25519 %s\nother-id ssh-ed25519 %s\n' "$(cut -d' ' -f2 mine.pub)" "$(cut -d' ' -f2 other.pub)" > allowed_signers

$ cut -c1-58 allowed_signers
mine-id  ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIIV6ShbPW5oBT
other-id ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIOX8S86oHyA9T

$ ssh-keygen -Y verify -f allowed_signers -I mine-id  -n file-verify -s payload.txt.sig < payload.txt
Good "file-verify" signature for mine-id with ED25519 key SHA256:q4H1d9ua2dKDFNkAEiNw1AgVT+jIygk7ZQ6WUt9kO+U

$ ssh-keygen -Y verify -f allowed_signers -I other-id -n file-verify -s payload.txt.sig < payload.txt
Could not verify signature.
```

> 名单里的两行 base64 是 demo **每次跑现造的两把钥匙**，所以这里的前缀（截到 58 列是为了不刷屏）
> 只对得上本次捕获；结构和行为不变。

**这套「验签 + 查身份名单」就是 SSH 登录时服务器干的事**（名单 = `authorized_keys`）。
`-Y sign/-Y verify` 还能给 git tag、给文件、给 CI 制品签名 —— 属于同一套数学，理解一次赚三次。

**一句话记法**：**私钥签名，公钥验签；公钥加密，私钥解密。** 登录走的是前一半。

---

## 主机密钥 vs 用户密钥（必须分清的两种"公钥/私钥对"）

它们**格式完全相同**（都是 `算法 base64 [注释]` 一行），区别**全在存在哪、被谁信、用来证明什么**。
本机实跑对比：前两条命令看的是**主机密钥**（`hostkey.pub`，keyscan 下来的），
后两条看的是**用户密钥**（`keys/id_demo.pub`，沙箱里现造的）。

```text
$ ssh-keygen -lf <沙箱>/hostkey.pub
256 SHA256:ZiBpW4VGzrtnDYcX80tWmYPtY6K9jw4OzMVPXmDRk4w 127.0.0.1 (ED25519)

$ cut -c1-60 <沙箱>/hostkey.pub
127.0.0.1 ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIJ8COfYZlWVtvs

$ ssh-keygen -lf <沙箱>/keys/id_demo.pub
256 SHA256:dlFPNzZBheCvaTtV/AIezF1+5HwokOFdxQbpG51D7pI sandbox-demo-key (ED25519)

$ cut -c1-60 <沙箱>/keys/id_demo.pub
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIOwsUATpFIMCABCghIoN4jLX
```

> 左边那条 `127.0.0.1` 是主机密钥公钥行**多出来的第一个字段**（主机名）；用户密钥的公钥行没有它，
> 结尾多一个注释。除这两点外两者逐字段同构 —— 这正是这张对比表的前提。
> 用户密钥的 base64 和指纹是 demo 每次跑现造的，只对得上本次捕获。

| | **主机密钥** Host key | **用户密钥** User/identity key |
|---|---|---|
| 谁拥有 | 一台**服务器**（每个机器一份） | 一个**人**（可以一人多把） |
| 私钥存在哪 | 服务器本机 `/etc/ssh/ssh_host_*_key`（root/600） | 你的机器 `~/.ssh/id_*`（600） |
| 公钥存在哪 | 所有客户端的 `~/.ssh/known_hosts`（或系统级 `ssh_known_hosts`） | 服务器的 `~/.ssh/authorized_keys`（或 CA 签的证书） |
| 用途 | 证明「**这台服务器**是它声称的那台」 | 证明「**我这个人**有权登录」 |
| 在握手哪一步用到 | KEX 阶段（`Server host key:` 那行） | 认证阶段（`Offering public key:` 那行） |
| 签名算法在哪张表里 | KEX 的 `host key algorithms` 清单 | EXT_INFO 的 `server-sig-algs` 清单 |
| 丢了 / 泄了会怎样 | **丢**：所有客户端报 `REMOTE HOST IDENTIFICATION HAS CHANGED`，每台都要 `-R` 一次 | **丢（泄露）**：别人能登录你所有装了这把公钥的机器 —— 所以必须有 passphrase + agent 兜着 |
| | | **丢（忘了备份）**：你进不去，得靠控制台/带外方式登录再装新公钥 |
| 常见数量 | 一台机器 3 把（rsa/ecdsa/ed25519）同时在场 | 一个人 1~N 把 |

**新手最典型的困惑**："我把服务器加进 known_hosts 了，为什么还让我配公钥？"
因为那是**两把毫不相干的钥匙**：known_hosts 是「你验服务器」，authorized_keys 是「服务器验你」。方向相反，位置相反。

---

## 命令速查表

| 我想干什么 | 命令 |
|---|---|
| 看完整握手过程 | `ssh -vvv <host>` （`-vv` 通常就够了；`-vvv` 才有包类型） |
| 把 debug 存文件（别污染 stderr） | `ssh -vvv -E /tmp/ssh-debug.log <host>` |
| 只看四个关键锚点 | `ssh -vvv ... 2>&1 \| grep -E 'kex: algorithm\|Server host key:\|Authentications that can continue\|Offering public key'` |
| 查本机支持的 KEX / 密钥 / 对称 / MAC | `ssh -Q kex` / `ssh -Q key` / `ssh -Q cipher` / `ssh -Q mac` |
| 查证书类算法 | `ssh -Q key-cert` |
| 只看服务端自报的端口横幅 | `timeout 5 bash -c 'exec 3<>/dev/tcp/HOST/22; head -1 <&3'` |
| 抓主机公钥（**不带任何信任**） | `ssh-keyscan -t ed25519 <host>` |
| 算任意公钥/私钥/known_hosts 的指纹 | `ssh-keygen -lf <文件>`（known_hosts 也能直接喂） |
| 要老式 MD5 冒号指纹 | `ssh-keygen -lf <文件> -E md5` |
| 第一次连接自动记账（安全版） | `-o StrictHostKeyChecking=accept-new` |
| 删掉一条 known_hosts（服务器换密钥后） | `ssh-keygen -f ~/.ssh/known_hosts -R <主机名>` |
| 查一条 known_hosts（哈希后也能查） | `ssh-keygen -f ~/.ssh/known_hosts -F <主机名>` |
| 把 known_hosts 全哈希 | `ssh-keygen -H -f ~/.ssh/known_hosts`（记得删 `.old`） |
| 体验一次"私钥签、公钥验" | `ssh-keygen -Y sign -n demo -f ./key file` → `ssh-keygen -Y check-novalidate -n demo -f ./key.pub -s file.sig < file` |
| 只看配置解析结果，不连接 | `ssh -G <host>`（03 章的支柱） |

---

## 这章的坑（≥5，全是本机踩过/复现过的）

1. **忘了 `-o IdentityAgent=none`，把你真实的钥匙递给了陌生服务器。**
   本 demo 第一版就犯了这个错，`-vvv` 里赫然出现
   `debug1: Offering public key: <你的真实注释> ED25519 SHA256:... agent`。
   递公钥本身不等于登录，但在陌生机器上暴露「我有这么一把 key、它长这样」是没必要的信息泄露。
   任何做实验的脚本都该带上 `-F /dev/null -o IdentityAgent=none -o UserKnownHostsFile=<沙箱文件>`。

2. **把 `StrictHostKeyChecking=no` 当"自动化标配"。** 它不是"跳过第一次提问"，它是**永久关闭服务器身份验证**
   （密钥变了也照连）。自动化要用 `accept-new`：第一次自动记账，**变了照样拦你**。

3. **用 `ssh-keyscan` 批量生成 known_hosts，以为自己做的是"固定凭据"。**
   实战 5 现场演示了：keyscan 抄来的公钥，写进 known_hosts 后校验**100% 通过**，
   因为它抄的路径和连接的路径是同一条。这等于零保障，只是文件里多了几行。

4. **把 `ssh -Q` 当成"我允许用这些"。** 它打印的是编译支持的，握手默认只报一部分。
   两个坑方向都有：以为旧算法能用（其实默认关了）、以为新算法不支持（其实有，只是排在后面）。

5. **看到 `REMOTE HOST IDENTIFICATION HAS CHANGED` 就无脑 `-R` 删了重装。**
   正确顺序：先在**另一条渠道**（云控制台、带外管理、同事、换手机热点再连一次）确认新指纹，
   再 `-R`。直接删掉 = 主动放弃了那次唯一能发现中间人的机会。

6. **`KexAlgorithms=xxx` / `Ciphers=xxx` 写成了"整张替换"，结果把唯一可用的算法挤掉了。**
   症状是 `Unable to negotiate ... no matching ...`。
   改老服务器**永远用 `+`**：`-o KexAlgorithms=+diffie-hellman-group14-sha1`。
   （本机两种写法的真实报错都跑出来了，见"坑"末尾的报错对照表。）

7. **在 CI / 管道里跑 ssh，忘加 `-o BatchMode=yes`。**
   没有 tty 时 ssh 会去试 `ssh_askpass`，失败后报一句 `Host key verification failed.` 或者
   `read_passphrase: can't open /dev/tty`。这句话**长得像**认证失败，其实是「它想问你问题但问不到」。
   真实输出片段（本机跑 ProxyJump 时看到的）：
   ```text
   debug1: read_passphrase: can't open /dev/tty: No such device or address
   ssh_askpass: exec(/usr/bin/ssh-askpass): No such file or directory
   Host key verification failed.
   ```

8. **本机 sshd 被并发握手淹了会主动踢你，报的话很像"配置坏了"。** 真实输出：
   ```text
   kex_exchange_identification: banner line 0: Not allowed at this time
   Connection reset by 127.0.0.1 port 22
   ```
   这是 `MaxStartups` 未完成认证连接数超限（默认 10 起限流）。脚本里并发跑很多 ssh 时非常常见 ——
   **重试就行**，不要改配置。（本 demo 的 `wait_sshd` / `rssh` 就是为了这个。）

9. **以为 `MAC: <implicit>` 是没配好。** 见实战 2 的注解 ②：AEAD 算法天生自带完整性，不需要独立 MAC。

10. **把 `server-sig-algs` 和 KEX 里的 host key 清单当同一张表。**
    一张管"服务器证明自己"，一张管"用户证明自己"。老服务器不发 EXT_INFO 时，
    出问题的是**用户侧 RSA 签名**，你却在改 `HostKeyAlgorithms` —— 方向错了，见实战 2。

---

## 自检问题

1. 一次 SSH 连接里，非对称加密一共用了几次、分别用来干什么？数据传输用不用非对称？
   **答**：非对称只用于「服务器身份验证（主机私钥签名）」和「用户身份验证（用户私钥签名）」这两处
   （DH 类协商属于"无需预共享秘密即可建立秘密"这一类）。传输全部走对称（AEAD/CTR + MAC）。
   见「心智模型第一块」。

2. 你说 `no` 拒绝第一次连接的指纹确认，known_hosts 里会留下什么？
   **答**：什么都不留。本机实测 `wc -c < known_hosts` = `0`。TOFU 的记录动作只在你**接受之后**才发生。

3. `WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED` 报错里，哪三行是你排错真正要看的？
   **答**：① `The fingerprint for the ED25519 key sent by the remote host is SHA256:...`（新的是什么）；
   ② `Offending ED25519 key in <文件>:<行号>`（旧记录在哪）；③ `remove with: ssh-keygen -f ... -R ...`（修复命令它已替你写好）。

4. `ssh-keyscan 主机 >> ~/.ssh/known_hosts` 和 `-o StrictHostKeyChecking=no`，哪个更安全？为什么？
   **答**：**基本一样不安全**，而且前者还更容易给人错觉。两者的信任建立都走"可能被劫持的同一条通道"，
   keyscan 连让你看一眼指纹的机会都没有。安全性来自**独立渠道核对指纹**，不来自"文件里有没有记录"。

5. 登录时你的私钥"发出去"了吗？服务端拿到什么才认定你是你？
   **答**：私钥一步都没出本地。先递的是**公钥**（`Offering public key`），服务器说认这把之后，
   你才用私钥对「本次会话独有的哈希（含服务器主机公钥哈希）」做**签名**。服务器用 authorized_keys
   里的公钥验签。签名被会话数据绑定，所以抓包重放无效。

6. 你有一把 RSA-3072 用户密钥，装好了公钥，本地 `ssh -vv` 看到 `Offering public key` 之后直接
   `Permission denied`，也看到对方 `Authentications that can continue: publickey`。最可能是什么？
   **答**：先确认 `authorized_keys` 的权限/内容/`StrictModes`（服务器在"要不要这把公钥"这一步就拒了）。
   如果日志里连 `server-sig-algs` 都没有（老服务），那是 RSA 签名哈希协商不上 ——
   换 ed25519 最省事。区分标志：**有没有出现 `Server accepts key` 这一行**（见实战 2 末尾）。

---

## 运行 demo

```bash
cd ssh-handbook/01-how-it-works
bash demo.sh            # 跑完自动删沙箱
bash demo.sh --keep     # 保留沙箱并打印路径，进去 cat full_vvv.log 慢慢对照本章
```

demo 会依次演示 9 组场景：配置读取隔离 → 裸版本号交换 → 完整 `-vvv`（含"卡在半路"和"走完认证"两份日志）
→ `ssh -Q` 算法查表与差集 → TOFU 提示原文（用 python 起假终端，回答 `no`）→ `accept-new` 记账与指纹比对
→ **主机密钥不匹配的完整报错 + `-R` 修复** → `ssh-keyscan` 及其信任空洞 → 签名/验签离线实验 →
主机密钥 vs 用户密钥 → 四种算法协商报错。

**本机实跑验证过的**（demo 里直接可见）：全部握手/KEX 协商、`Server host key` 指纹与 known_hosts 一致性、
TOFU 提示、host key mismatch 全流程、keyscan 结果可用性与指纹、`ssh -Q` 各表、签名/验签四个实验、
四种算法报错。

**需要你自己有服务器时才能验证的**（demo 里演不了，README 也都标了）：
① 认证成功路径的那几行（`Server accepts key` / `Authentication succeeded`）；
② `/etc/ssh/ssh_host_*_key.pub` 的指纹核对（要登录服务器执行）；
③ SSHFP + DNSSEC（要自建签名 DNS）。

**安全**：本 demo 不启动后台进程、不监听端口、不读写 `~/.ssh`、不修改 `/etc/ssh`，
所有命令都带 `-F /dev/null -o IdentityAgent=none -o UserKnownHostsFile=<沙箱>`。
结尾会自动打印「监听端口 / 遗留进程 / 沙箱文件清单」三项自证。

---

## 围栏里的占位符 = 脱敏约定（读之前先看这条）

本章每一行输出都取自 `bash demo.sh` 的一次真跑（捕获存在
`~/.cache/handbook-captures/ssh01.txt`），只做了三类替换：

| 围栏里写的 | 屏幕上实际出现的 | 为什么这么写 |
|---|---|---|
| `<沙箱>` | `/tmp/ssh01-XXXXXX`（每次跑都换一个） | 临时目录名，和身份无关，只是没必要印 |
| `learner@127.0.0.1` | `learner@127.0.0.1`（**demo 里就是这么写的**，没替换） | 远端用户名用中性名。本机不存在 `learner`，本章每次连接都停在 `Permission denied`，结论和用真名完全一致 —— 而你自己不加 `user@` 跑同一条命令，报错里印的就是你的登录名，这正是要教的那个坑 |
| `<主机名>` | 你填的任意 host | 只出现在「需要你自己有服务器」的**命令模板**里，不是输出 |

另一类是**shell/程序从本机环境展开、demo 拦不住**的：`ssh -G` 的 `user` 行（没配 `User` 时 = 你的登录名）、
`%u` / `%L` 的展开、`ControlPath` 里嵌的真实用户名、`ls -l` 的属主列、`/home/<你>/.ssh/...` 这种路径。
这一类在 README 里一律写成 `<本机用户>` / `<家目录>` / `<主机名>`（02 章就有 8 行这种，章末也写了同一条约定）。
**本章 README 引用到的围栏恰好一条都没沾上**：`-G` 那条命中的是沙箱配置里的 `User sandbox-user`，
没有把 `ls -l` 的属主列抄进来。捕获文件里其实有 2 行真名字（场景 2 那句 `ls -l <沙箱>/keys` 的属主列），
README 没有用它 —— **你跑 demo 会在那一行看到你自己的登录名，是正常的**，它只活在你本机的捕获里。

## 本章围栏和捕获文件的差异（逐条，共 8 行）

1. **`rssh` / `ksc` 是 demo 的重试包装，README 展示的是展开后的等价命令。**
   sshd 的 `MaxStartups` 在并发握手时会临时拒绝新连接（`Not allowed at this time` / RST），
   而裸 `ssh-keyscan` 撞上这个**既不报错也不重试、直接返回空**，整段演示就塌了。
   所以 demo 把每条要碰 22 端口的命令包进 `rssh`（ssh）/ `ksc`（keyscan）/ `wait_sshd`（探一下 banner）。
   函数体里没有新逻辑，捕获里那些 `$ rssh 'ssh -F /dev/null …'` / `$ ksc ed25519 …` 行，
   README 都写成你手敲等价的那条 —— 这是**呈现差异**，不是没跑过。
2. **实战 5 的 ③「哈希 known_hosts」两条围栏（8 行）来自 02 章 demo 场景 7 的真跑**（`ssh02.txt`）：
   本章 demo 不跑 `ssh-keygen -H`（本章只管握手），已经在该节就地标注了来源。
   同时 `|1|<盐>|<HMAC>|` 两段 base64 里的盐是每次随机生成的，天然对不上别次的捕获。
3. **随机值**（不算差异，只是提醒）：沙箱里现造密钥的指纹、`allowed_signers` 里那两行 base64 前缀、
   agent 的 socket 名与 PID、`ssh -Q` 的算法表（随 OpenSSH 版本变）。
   本章围栏按当次捕获原样抄，其中指纹/路径这类被校验脚本折成占位符，base64 前缀那两行是逐字符对齐的。

---

## 下一步

- `02-keys`：把「用户密钥」这一侧讲透 —— 生成、passphrase、权限模型实测、指纹比对、部署公钥。
- `03-config-file`：把上面那些 `-o` 长串变成 `~/.ssh/config` 里的三行字（效率提升最大的一章）。
- 排错时永远先回到本章那张泳道图：**问自己"卡在第几步"**。
  卡在 ②③ = 网络/端口；卡在 ④~⑦ = 算法协商；卡在 ⑦ 的 B = 主机密钥/known_hosts；
  卡在 ⑬~⑱ = 认证。这个二分法能解决你 90% 的 SSH 问题。
