# 这章：密钥 —— 生成、指纹、口令、权限、部署

> **一句话：把「一把 SSH 密钥从生出来到能被服务器认」这条路上的五个东西 —— 算法选择、公钥那一行、
> 指纹、口令、文件权限 —— 全部在本机真跑一遍，看清每一步的原文输出。**
> 这章是「你第一次真的要用 SSH 连服务器」时最先撞上的墙，也是 04（agent）、06（排错）两章的地基。

本章所有输出都是**本机真跑出来的**（OpenSSH_10.2p1 Ubuntu-2ubuntu3.6，客户端本机，服务端 `127.0.0.1:22`
上是真实的系统 sshd）。落盘的完整输出叫 `/tmp/ssh02.txt`（680 行），沙箱目录 `<沙箱>`，
跑完自动删除。

**实验红线**（demo 从头到尾遵守，你抄自己的实验也照这个抄）：

- 不读写 `~/.ssh/`、不碰 `/etc/ssh/`；所有 key、known_hosts、agent socket 都在 `mktemp -d` 的沙箱里。
- 每条 `ssh` 都带 `-F /dev/null`（不读你真实的 config）、`-o IdentityAgent=none` 或沙箱 agent、
  `-o UserKnownHostsFile=<沙箱文件>`，并且用一个不存在的 `-o IdentityFile=` 当挡箭牌，
  免得 ssh 去挨个探测 `~/.ssh/id_rsa`、`~/.ssh/id_ed25519`。
- **不制造登录成功**（不装 authorized_keys、不猜密码），所以每条真连接最后都停在
  `Permission denied (publickey,password).` —— 这正是要观察的东西。
- **私钥内容一个字都不打印**。要看「私钥里有什么」一律用 `ssh-keygen -y`（只吐公钥）。

凡是本机演不了的（需要你有第二台机器 / 需要登录成功），文中都明写「**需要你自己有服务器时验证**」。

---

## 你会遇到的场景

| # | 场景 | 学完这章你能自己解释 |
|---|---|---|
| 1 | 该生成哪种 key？`-t ed25519` 还是 `-t rsa -b 4096`？ | 三把真生成的 key + 真实耗时对比（demo 第 1 节） |
| 2 | `ssh-keygen -t ed25519 -b 4096` 为什么不报错也没生效 | `-b` 对 ed25519 是**静默忽略**，实测指纹仍是 256 |
| 3 | 公钥那一行 `ssh-ed25519 AAAA... me@laptop` 三段各是什么 | 逐字段拆开 + base64 解出内部 blob（demo 第 2 节） |
| 4 | 别人要我的「指纹」，我该给哪一条 | `SHA256:` / `MD5:` / `SHA512:` 三种都跑过（demo 第 3 节） |
| 5 | 第一次连服务器弹 `The authenticity of host ... can't be established`，按什么 | TOFU + 用 keyscan 独立算指纹核对（demo 第 3、7 节） |
| 6 | 私钥设了口令，每次连都要输，烦 | `ssh-keygen -p` 换口令不动公钥；口令交给 agent 只需输一次（demo 第 4、5 节） |
| 7 | 私钥设了口令，是不是拷走文件就没事 | 文件确实多了一层信封（399 → 464 字节），但**弱口令 = 零防护**（demo 第 4 节） |
| 8 | `WARNING: UNPROTECTED PRIVATE KEY FILE!` 到底在管什么权限 | 644 私钥的完整报错 + 一个反直觉现象：ssh 连的时候**不一定**会报它（demo 第 6 节） |
| 9 | `Bad owner or permissions on ...` 我没改任何 key 啊 | 那是**配置文件/Include** 的权限检查，门限是「别人不可写」（demo 第 6 节） |
| 10 | known_hosts 里怎么查/删/哈希，注释怎么改 | `-F` / `-R` / `-H` / `-c` 四条全跑（demo 第 7 节） |
| 11 | 公钥怎么装到服务器上去 | `ssh-copy-id -n` 的 dry-run 原文 + 另外两条路（demo 第 8 节） |
| 12 | `authorized_keys` 前面那些 `from=` / `command=` 是什么 | 一行一把钥匙，选项不能单独成行（demo 第 8 节） |
| 13 | 公司有几百台机器，不想每台贴公钥 | 证书：`ssh-keygen -s` 签一张，`-Lf` 读出有效期/principals（demo 第 9 节） |

---

## 心智模型

### 第一块：一把密钥其实是「三个东西 + 一个可选外壳」

```
   ssh-keygen 一次生成 ──────────────────────────────────────────────┐
                                                                    │
     ┌───────────────────────┐        ┌────────────────────────┐    │
     │ id_ed25519   （私钥） │        │ id_ed25519.pub（公钥） │ ◄──┘
     │ 权限 600，绝不外传     │───────►│ 权限 644，随便贴        │  ssh-keygen -y 单向导出
     │ 可能裹一层口令信封     │  只能   │ 一行三字段：            │  （公钥永远推不出私钥）
     └───────────────────────┘  正向   │ <算法> <blob> <注释>   │
                ▲                       └────────────────────────┘
                │ 口令（passphrase）                │
                │ 只是「文件加密的外壳」             │ 摘要
                ▼                                   ▼
     ┌───────────────────────┐        ┌────────────────────────┐
     │ 换了口令 ≠ 换了钥匙   │        │ 指纹 SHA256:KFI9m8A9… │ ← 给人眼核对用的短名字
     │ 公钥/指纹一个字都不变 │        │ 不是身份凭据，不能反推 │
     └───────────────────────┘        └────────────────────────┘

   谁信这三样东西：
     你的私钥  → 只有你的机器（+ 可选 agent 进程）持有
     你的公钥  → 服务器 ~/.ssh/authorized_keys（或 CA 签的证书）
     服务器公钥 → 你的 ~/.ssh/known_hosts（01 章讲的主机密钥，方向相反！）
```

**最容易搞混的一点**：口令（passphrase）保护的是**磁盘上那个文件**，不是密钥本身。
密钥的数学部分从来没被「锁」起来 —— 只要程序拿到口令解开信封，签名能力和裸私钥完全等价。

### 第二块：三种算法怎么选（不是信仰问题，是「谁会跟你握手」的问题）

| | **ed25519** | **ecdsa** | **rsa** |
|---|---|---|---|
| 一句话定位 | 默认就该用它 | 别主动用，除非有历史包袱 | 只留给连不动新算法的老系统 |
| 密钥长度可选吗 | 不可选，固定 256 位 | 256/384/521（曲线） | 位数随便给，但**默认 3072** |
| 本机实测生成耗时 | `real 0m0.011s` | （和 ed25519 同量级，demo 用 `-q` 没计时） | `real 0m1.320s`（4096） |
| 生成原理 | 曲线上的随机标量，快 | 曲线运算 | **要找两个大素数**，慢且吃 CPU |
| 指纹那行显示 | `256 SHA256:... (ED25519)` | `384 SHA256:... (ECDSA)` | `4096 SHA256:... (RSA)` |
| 签名的哈希 | 无哈希选择问题 | 无 | 有：SHA-1 的 `ssh-rsa` 早已被新版禁用，只剩 `rsa-sha2-256/512` |
| 什么时候必须用它 | — | 某些老硬件/老云面板只收 ecdsa | 对方 OpenSSH 太老、或第三方设备（打印机/老交换机）只认 rsa |
| 坑 | `-b` 写了也被忽略 | `nistp384/521` 有些老服务器不支持 | 别人报 `no matching key exchange`，你的新客户端已默认关掉它需要的老算法 |

**结论一句话**：新建一律 `-t ed25519`；只有「连不上老家伙」时才为那个老家伙单独配一把 RSA，
不要把主力 key 换成 RSA。

### 第三块：公钥为什么敢公开 —— 一行 base64 里到底装了什么

```
   id_ed25519.pub 这一行：
   ssh-ed25519   AAAAC3NzaC1lZDI1NTE5AAAAIIrNCvZB...   me@laptop
   └ 算法名        └ 公钥 blob 的 base64                └ 注释（自由文本，无安全含义）

   那个 blob 解码出来（demo 第 2 节的 xxd 原文）：
   00000000: 0000 000b 7373 682d 6564 3235 3531 3900  ....ssh-ed25519.
             └4字节长度=0x0b=11┘└── "ssh-ed25519"（11 字节）──┘│
   00000010: 0000 20 7e 18 07 00 75 68 50 ...         ← 又一个 4 字节长度(0x20=32) + 32 字节公钥本体
```

也就是说 SSH 公钥文件是 **SSH 自己的 wire 格式**（长度前缀 + 算法名 + 长度前缀 + 本体），不是 PEM。
这就是为什么它能直接贴进 `authorized_keys`、也能塞进 known_hosts —— 同一种编码，两个方向用。

**单向性**（demo 第 2 节实测）：从私钥算公钥 `real 0m0.009s`；反过来的问题叫「椭圆曲线离散对数」，
目前没有可行办法。所以：

- 贴公钥 = 安全；
- **泄露私钥 = 连公钥带签名能力一起丢**，而且服务器端完全看不出你泄露过（它只看到「签名合法」）。

---

## 场景逐个实战

> 下面每一节都标了对应的 **demo 第 N 节**，贴的都是那一次真跑的原文（沙箱 `<沙箱>`）。

### 实战 1：三种算法各生成一把（demo 第 1 节）

**敲什么**

```bash
ssh-keygen -t ed25519 -C 'me@laptop' -f ./id_ed25519 -N ''      # -N '' = 不设口令
ssh-keygen -t ecdsa -b 384 -C 'me@laptop' -f ./id_ecdsa -N '' -q
ssh-keygen -t rsa  -b 4096 -C 'compat@example.com' -f ./id_rsa -N '' -q
```

**真实输出（计时是重点）**

```text
$ cd <沙箱> && time ssh-keygen -t ed25519 -C 'me@laptop' -f <沙箱>/id_ed25519 -N ''
Generating public/private ed25519 key pair.
Your identification has been saved in <沙箱>/id_ed25519
Your public key has been saved in <沙箱>/id_ed25519.pub
The key fingerprint is:
SHA256:KFI9m8A9NHsMG4vFu4n/8KuMlf6Pzi2aqnbMb4bkiLw me@laptop
The key's randomart image is:
+--[ED25519 256]--+
|     .*          |
|   . *.O         |
|    = O.o        |
|   . ..B         |
|  . ..+oS        |
|   .o.o.         |
|.. * o+          |
|..o *=+=.o       |
| Eooo=B*O+o      |
+----[SHA256]-----+

real	0m0.004s

$ cd <沙箱> && time ssh-keygen -t rsa -b 4096 -C 'compat@example.com' -f <沙箱>/id_rsa -N '' -q

real	0m1.070s
```

**读法**

- 那一屏「randomart 随机艺术图」是**给人核对的辅助**：同一个指纹算出的图一样。理论上有意义，
  实践里没人真用它比对 —— 比对一律用 `SHA256:` 那行。
- `-q` 就是把这一整屏静音（老脚本里常见）。
- 1.07 秒 vs 0.004 秒：RSA 慢在**找素数**，不是慢在写文件。
  （`time` 后面还有 `user` / `sys` 两行，README 只留了 `real` 这一行 —— 每次跑都会变的耗时，
  抄三行没意义，见章末差异清单。）

**生成物的权限（这条很重要，实战 6 会炸给你看）**

```text
$ ls -l <沙箱> | awk '{print $1, $5, $9}'
total  
-rw------- 610 id_ecdsa
-rw-r--r-- 215 id_ecdsa.pub
-rw------- 399 id_ed25519
-rw-r--r-- 91 id_ed25519.pub
-rw------- 3381 id_rsa
-rw-r--r-- 744 id_rsa.pub
```

私钥 `600`、公钥 `644` —— 这就是 `ssh-keygen` 给你的标准答案，别自己改。

**指纹那行直接告诉你类型和位数**

```text
$ for f in id_ed25519 id_ecdsa id_rsa; do ssh-keygen -lf <沙箱>/$f.pub; done
256 SHA256:KFI9m8A9NHsMG4vFu4n/8KuMlf6Pzi2aqnbMb4bkiLw me@laptop (ED25519)
384 SHA256:8RA5E+CMpS8/cXlavYtbm6Wz46lHWneDYgaWfsZWT9M me@laptop (ECDSA)
4096 SHA256:n1isSpa+9CP9BMy+FwlI6ySAVi8FEB51rtVcy3CpOHM compat@example.com (RSA)
```

**两个位数陷阱（原文都跑出来了）**

```text
$ cd <沙箱> && ssh-keygen -t rsa -C 'default-bits' -f <沙箱>/id_rsadef -N '' -q && ssh-keygen -lf <沙箱>/id_rsadef.pub
3072 SHA256:gvKjGqwb1FxdWKd0IW/8ScbstdjafluPzUiWtEYAJfY default-bits (RSA)

$ cd <沙箱> && ssh-keygen -t ed25519 -b 2048 -C 'bit-ignored' -f <沙箱>/id_b2k -N '' -q; ssh-keygen -lf <沙箱>/id_b2k.pub
256 SHA256:iEP4btHZWt86WGDlBQysXbpDtfxPJkI23O5gtWiInpY bit-ignored (ED25519)

$ cd <沙箱> && ssh-keygen -t rsa -b 512 -f <沙箱>/id_tiny -N '' -q
Invalid RSA key length: minimum is 1024 bits
  （退出码 255 —— 这条就是要看它失败/报错）
```

- **RSA 不写 `-b` 是 3072，不是 4096。** 想 4096 必须自己写。
- **ed25519 的 `-b` 会被静默吃掉**，一声不响地给你 256 位。你以为加了强度，其实什么都没发生 ——
  这类「不报错的无效参数」是配置审查里最难发现的，**永远用 `ssh-keygen -lf` 复核一遍**。
- RSA 太短会直接拒绝（最低 1024，但 1024 也别用）。

---

### 实战 2：把公钥那一行拆开看（demo 第 2 节）

**敲什么 / 真实输出**

```text
$ cat <沙箱>/id_ed25519.pub
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIIrNCvZBjA9dBDA5KZ6wmDV/vQ7ckjjrKhPI91WOl+Sp me@laptop

$ ssh-keygen -y -f <沙箱>/id_ed25519
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIIrNCvZBjA9dBDA5KZ6wmDV/vQ7ckjjrKhPI91WOl+Sp me@laptop

$ diff <(ssh-keygen -y -f <沙箱>/id_ed25519) <沙箱>/id_ed25519.pub && echo '完全相同 —— 公钥就是私钥的公开部分，不是另一套秘密'
完全相同 —— 公钥就是私钥的公开部分，不是另一套秘密

$ awk '{print $2}' <沙箱>/id_ed25519.pub | base64 -d | xxd | head -4
00000000: 0000 000b 7373 682d 6564 3235 3531 3900  ....ssh-ed25519.
00000010: 0000 208a cd0a f641 8c0f 5d04 3039 299e  .. ....A..].09).
00000020: b098 357f bd0e dc92 38eb 2a13 c8f7 558e  ..5.....8.*...U.
00000030: 97e4 a9                                  ...
```

**`ssh-keygen -y` 是你该记住的「安全查看私钥」命令**：它只从私钥算出公钥，
不打印私钥的任何字节。要确认「这把私钥和那个 .pub 是不是配套」，用它 diff 一下就行。
（云面板/工单要你给「公钥」时，忘了 .pub 被删掉也能用 `-y` 现场再生成一个。）

**blob 结构**：`0000 000b` + `ssh-ed25519`（11 字节）+ `0000 20` + 32 字节本体。
这套「4 字节大端长度 + 内容」的拼法是整个 SSH 协议的通用编码，known_hosts、证书、
wire 上的包全是它。看懂一次，后面到处通用。

**手滑改错一个字符会怎样？（会报，不会静默错）**

```text
$ printf 'ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI改一个字符 x\n' > <沙箱>/broken.pub; ssh-keygen -lf <沙箱>/broken.pub
<沙箱>/broken.pub is not a public key file.
  （退出码 255 —— 这条就是要看它失败/报错）
```

所以「贴公钥时少复制了一个字符」这类事故，会在**加载那一步**就报出来，而不是悄悄连错机器。

**格式互转（RFC 4716，某些老面板要的那种）**

```text
$ ssh-keygen -e -f <沙箱>/id_ed25519.pub
---- BEGIN SSH2 PUBLIC KEY ----
Comment: "256-bit ED25519, converted by <本机用户>@<主机名> from OpenSSH"
AAAAC3NzaC1lZDI1NTE5AAAAIIrNCvZBjA9dBDA5KZ6wmDV/vQ7ckjjrKhPI91WOl+Sp
---- END SSH2 PUBLIC KEY ----

$ ssh-keygen -e -f <沙箱>/id_ed25519.pub > <沙箱>/id_ed25519.rfc4716 && ssh-keygen -i -m RFC4716 -f <沙箱>/id_ed25519.rfc4716
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIIrNCvZBjA9dBDA5KZ6wmDV/vQ7ckjjrKhPI91WOl+Sp
```

`-e` = export（OpenSSH → RFC 4716），`-i` = import（反过来）。
**注意一个实测细节**：`ssh-keygen -i` **不接受管道**（`... | ssh-keygen -i -m RFC4716 -f -` 会报
`ssh-keygen: -: No such file or directory`），必须落成文件用 `-f` 指给它；
而且不写 `-f` 时它会去问你「Enter file in which the key is (/home/…/.ssh/id_ed25519):」——
脚本里跑到这一步就等于把默认路径猜了一遍，别这么用。
还有：转回来的那一行**注释丢了**（RFC 4716 的 `Comment:` 头不会变回行尾注释），别指望它无损。

---

### 实战 3：指纹 —— 人和人之间靠它核对，机器和机器之间不靠它（demo 第 3 节）

**公钥文件和私钥文件算出来的指纹必须一模一样**

```text
$ ssh-keygen -lf <沙箱>/id_ed25519.pub; ssh-keygen -lf <沙箱>/id_ed25519
256 SHA256:KFI9m8A9NHsMG4vFu4n/8KuMlf6Pzi2aqnbMb4bkiLw me@laptop (ED25519)
256 SHA256:KFI9m8A9NHsMG4vFu4n/8KuMlf6Pzi2aqnbMb4bkiLw me@laptop (ED25519)
```

这两行不一致就说明 .pub 和私钥**不是同一把**（`.pub` 被人覆盖过、或者 `-i` 指错文件）。
这是排查「服务器装了 A 的公钥、你手里是 B 的私钥」最快的动作。

**同一条命令三种摘要**

```text
$ for e in md5 sha256 sha512; do ssh-keygen -lf <沙箱>/id_ed25519.pub -E $e; done
256 MD5:53:96:e9:4c:cc:cd:6c:15:e0:a4:e2:9a:4f:fe:17:14 me@laptop (ED25519)
256 SHA256:KFI9m8A9NHsMG4vFu4n/8KuMlf6Pzi2aqnbMb4bkiLw me@laptop (ED25519)
256 SHA512:f6D7llPMLzyx6HMKQ3wrphuCRcYn9qBx31o26L+0TIsuBG4w5v/Kd+j/TlX+YUGAtwU2T2bguhVbFZJ551kglA me@laptop (ED25519)
```

- 默认就是 `SHA256:`（OpenSSH 6.8 起换掉的）。**别人问你指纹，你给的就是这一行。**
- `MD5:` 那串冒号十六进制是**老格式**，很多老运维/老文档还用它。核对前先确认对方说的是哪种，
  拿 MD5 去比 SHA256 一定「不一致」，白白惊动一次「是不是被中间人了」。
- SHA256 base64 只有 43 个字符（`=` 填充被去掉了），比 32 段冒号短一半，还更抗撞。

**核对服务器指纹：管道技巧（本章最实用的一招）**

```text
$ ssh-keyscan -t ed25519 127.0.0.1 2>/dev/null | ssh-keygen -lf -
256 SHA256:ZiBpW4VGzrtnDYcX80tWmYPtY6K9jw4OzMVPXmDRk4w 127.0.0.1 (ED25519)
```

`ssh-keygen -lf -` 直接吃 stdin。拿到这个指纹后，**用另一条渠道**（云控制台、带外管理、
同事、服务器的 web 面板）问一句「你的 ed25519 主机密钥指纹是不是 `ZiBpW4…`」，
一致才算数。

⚠ 必须说清楚的边界（01 章实战 5 讲透，这里只做提醒）：
`ssh-keyscan` 拿到的公钥**没有任何信任含量** —— 它和你要连的是同一条网络路径，
中间人递什么它就打什么。keyscan 的价值只在「把一行凭据准备好」，不在「验证」。

**第一次连接那句告警，长这样（原文已由 01 章 demo 抓到，本章 demo 的 grep 条件没放行它）**

```text
The authenticity of host '127.0.0.1 (127.0.0.1)' can't be established.
ED25519 key fingerprint is: SHA256:ZiBpW4VGzrtnDYcX80tWmYPtY6K9jw4OzMVPXmDRk4w
This key is not known by any other names.
Are you sure you want to continue connecting (yes/no/[fingerprint])?
```

第三个答案 `[fingerprint]` 就是给「你已经从别的渠道问到了指纹」的人用的：
把期望指纹当答案打进去，一致才记账 —— 强制自己做了核对，比盲按 `yes` 强。
本章 demo 因为一律带 `-o StrictHostKeyChecking=accept-new`，
所以第一次连本机时打的是那行 `Warning: Permanently added ...`，而不是这个提问。

---

### 实战 4：口令（passphrase）—— 它锁的是文件，不是密钥（demo 第 4 节）

**造一把带口令的 key，看文件的变化**

```text
$ cd <沙箱> && ssh-keygen -t ed25519 -C 'with-passphrase' -f <沙箱>/id_lock -N 'hunter2-demo'
Generating public/private ed25519 key pair.
Your identification has been saved in <沙箱>/id_lock
Your public key has been saved in <沙箱>/id_lock.pub
The key fingerprint is:
SHA256:zS/5RhEHrMp+7KddOUjjZv0MCXj+3J/EPzGWE1YRzSA with-passphrase
The key's randomart image is:
+--[ED25519 256]--+
|           .E..=+|
|            o.. +|
|           . o  .|
|         o...  o |
|       .S.+ =.. o|
|        o  B.=.O |
|       . .o.B Oo+|
|        . o*o+.Bo|
|         oo+o o.O|
+----[SHA256]-----+

$ printf '未加密私钥 %s 字节 / 加密后 %s 字节\n' $(wc -c < <沙箱>/id_ed25519) $(wc -c < <沙箱>/id_lock)
未加密私钥 399 字节 / 加密后 464 字节
```

多的那 65 字节 = 信封头 + KDF 参数（算法名、盐、轮数）。文件是「OpenSSH 私钥格式 v1」，
`-----BEGIN OPENSSH PRIVATE KEY-----` 那个外壳下面才是这层加密。
**注意**：本 demo 全程没有 `cat` 过任何私钥，只有 `wc -c` 数长度。

**换口令 ≠ 换钥匙**

```text
$ cd <沙箱> && ssh-keygen -p -P 'hunter2-demo' -N 'newpass-demo' -f <沙箱>/id_lock
Key has comment 'with-passphrase'
Your identification has been saved with the new passphrase.

$ ssh-keygen -lf <沙箱>/id_lock.pub
256 SHA256:zS/5RhEHrMp+7KddOUjjZv0MCXj+3J/EPzGWE1YRzSA with-passphrase (ED25519)
```

指纹 `LPNzBxLy…` 和换之前**一模一样** → 所以「觉得口令不够强，换一个」这件事
**不需要**重新部署任何服务器的 `authorized_keys`。这是 `-p` 最大的价值。

**错口令 / 对口令**

```text
$ ssh-keygen -y -f <沙箱>/id_lock -P 'wrong-pass'
Load key "<沙箱>/id_lock": incorrect passphrase supplied to decrypt private key
  （退出码 255 —— 这条就是要看它失败/报错）

$ ssh-keygen -y -f <沙箱>/id_lock -P 'newpass-demo'
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIMogAFw0mFGGqOcR6Kp4eP1AAnIJMHeIM87WPm8LJCZ2 with-passphrase
```

这条报错是你以后最常见的「钥匙没错、口令输错了」的信号原文，值得背下来。

**KDF 轮数 `-a` 的真实代价**

```text
$ cd <沙箱> && ssh-keygen -t ed25519 -C 'kdf' -f <沙箱>/id_kdf -N 'pw-demo' -a 100 -q && time ssh-keygen -p -P 'pw-demo' -N 'pw2-demo' -a 100 -f <沙箱>/id_kdf
Key has comment 'kdf'
Your identification has been saved with the new passphrase.

real	0m1.324s
```

默认 16 轮时解一次口令是毫秒级；100 轮要 1.28 秒。
`-a` 只影响**离线爆破的单价**（攻击者拷走文件后每猜一次口令的耗时），
对你每次正常使用同样加 1.28 秒 —— 而那个人肉代价由 agent 承担（实战 5），所以可以放心调高。
但顺序别搞反：**口令强度 >> 轮数**。`hunter2` 配 `-a 100000` 依然是脆的。

⚠ 诚实标注一处：demo 在「证据二」那一步同时打印了 `id_lock.pub` 和 `id_ed25519.pub` 两个指纹，
而这两个是**分别生成的两把不同 key**，所以它们的指纹当然不同
（`LPNzBxLy…` vs `KFI9m8A9…`）。demo 那行小标题写的是「公钥和指纹完全一致」，
这句标注不准确 —— 真正证明「换口令不动钥匙」的是上面 `-p` 那一步。
结论照抄没问题，只是别看错那两行。

---

### 实战 5：agent 先「能用」就行（demo 第 5 节；深挖在 04 章）

这节只解决一个问题：**口令只输一次，怎么做到**。

```text
$ cd <沙箱> && ssh-agent -a <沙箱>/agent.sock -s > <沙箱>/agent.env && cat <沙箱>/agent.env
SSH_AUTH_SOCK=<沙箱>/agent.sock; export SSH_AUTH_SOCK;
SSH_AGENT_PID=126981; export SSH_AGENT_PID;
echo Agent pid 126981;

$ ssh-add -l
The agent has no identities.
  （退出码 1 —— 这条就是要看它失败/报错）

$ SSH_ASKPASS=<沙箱>/askpass SSH_ASKPASS_REQUIRE=force ssh-add <沙箱>/id_lock
Identity added: <沙箱>/id_lock (with-passphrase)

$ ssh-add -l
256 SHA256:zS/5RhEHrMp+7KddOUjjZv0MCXj+3J/EPzGWE1YRzSA with-passphrase (ED25519)

$ ssh-add -L
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIMogAFw0mFGGqOcR6Kp4eP1AAnIJMHeIM87WPm8LJCZ2 with-passphrase
```

- agent 是一个**普通后台进程 + 一个 Unix socket**。`SSH_AUTH_SOCK` 就是它的门牌号。
- `ssh-add -l` 列指纹（给自己看），`ssh-add -L` 列公钥全文（给别人用 —— 这行就是
  `authorized_keys` 要的内容，所以「agent 里有哪些钥匙」=「我能登哪些机器」的候选集）。
- 没口令的 key 直接 `ssh-add <文件>` 就行，不用 askpass。

**真连一次，看日志里「钥匙从哪来」的标签**

```text
$ ssh -F /dev/null -vv -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh -o IdentityAgent=<沙箱>/agent.sock -o IdentityFile=<沙箱>/_no-default-probe learner@127.0.0.1 true 2>&1 | grep -E 'Server host key|Will attempt key|Offering public key|Permission denied'
debug1: Server host key: ssh-ed25519 SHA256:ZiBpW4VGzrtnDYcX80tWmYPtY6K9jw4OzMVPXmDRk4w
debug1: Will attempt key: with-passphrase ED25519 SHA256:zS/5RhEHrMp+7KddOUjjZv0MCXj+3J/EPzGWE1YRzSA agent
debug1: Will attempt key: me@laptop ED25519 SHA256:KFI9m8A9NHsMG4vFu4n/8KuMlf6Pzi2aqnbMb4bkiLw agent
debug1: Will attempt key: <沙箱>/_no-default-probe  explicit
debug1: Offering public key: with-passphrase ED25519 SHA256:zS/5RhEHrMp+7KddOUjjZv0MCXj+3J/EPzGWE1YRzSA agent
debug1: Offering public key: me@laptop ED25519 SHA256:KFI9m8A9NHsMG4vFu4n/8KuMlf6Pzi2aqnbMb4bkiLw agent
learner@127.0.0.1: Permission denied (publickey,password).
```

行尾那个 `agent` 就是**出处标签**：这把钥匙是 ssh 进程顺着 socket 向 agent 要来的，
它自己**一个私钥字节都没读**。`explicit` = `-i`/`IdentityFile` 明确指定的。
（那个不存在的 `_no-default-probe` 是挡箭牌，专门用来阻止 ssh 去探测你真实的 `~/.ssh/id_*`。）
最后的用户名是 `learner` —— **一个本机不存在的中性用户**（章末约定那条）。
本章每一次「连一下」都停在 `Permission denied`，用它还是用真登录名结论完全一样。

**删钥匙**

```text
$ ssh-add -d <沙箱>/id_ed25519 && ssh-add -l
Identity removed: <沙箱>/id_ed25519 ED25519 (me@laptop)
256 SHA256:zS/5RhEHrMp+7KddOUjjZv0MCXj+3J/EPzGWE1YRzSA with-passphrase (ED25519)

$ ssh-add -D; ssh-add -l || echo '（agent 已空）'
All identities removed.
The agent has no identities.
（agent 已空）
```

为什么最后仍然 `Permission denied`：这些公钥不在任何 `authorized_keys` 里（本章红线）。
**「Offering 了」和「服务器认了」是两回事**，两者的分界线上有一个关键日志行
（`Server accepts key`），本机拿不到 —— 01 章实战 2 末尾专门讲了这件事，排错时必看。

`SSH_AUTH_SOCK` 为什么换个终端就失效、agent 里钥匙太多为什么会 `Too many authentication failures`、
怎么给 agent 上锁 —— 全是 **04 章**，别在这节停留。

---

### 实战 6：权限模型 —— SSH 为什么对权限这么不客气（demo 第 6 节）

**把私钥改成 644（最常见的翻车姿势：从别的机器 `cp` 过来 / 从 git 仓库里 clone 出来）**

```text
$ cp <沙箱>/id_ed25519 <沙箱>/noperm && chmod 644 <沙箱>/noperm && ls -l <沙箱>/noperm
-rw-r--r-- 1 <本机用户> <本机用户> 399 Oct  5 12:57 <沙箱>/noperm

$ ssh-keygen -y -f <沙箱>/noperm
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@         WARNING: UNPROTECTED PRIVATE KEY FILE!          @
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
Permissions 0644 for '<沙箱>/noperm' are too open.
It is required that your private key files are NOT accessible by others.
This private key will be ignored.
Load key "<沙箱>/noperm": bad permissions
  （退出码 255 —— 这条就是要看它失败/报错）

$ ssh-keygen -p -f <沙箱>/noperm -N 'x' -P ''
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@         WARNING: UNPROTECTED PRIVATE KEY FILE!          @
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
Permissions 0644 for '<沙箱>/noperm' are too open.
It is required that your private key files are NOT accessible by others.
This private key will be ignored.
Failed to load key <沙箱>/noperm: bad permissions
```

`ssh-keygen -y` 和 `ssh-keygen -p` 都拒绝 → 这是**「读私钥」这条统一防线**，
不是某个子命令的怪癖。修复只有一条命令：`chmod 600 <私钥>`。

**反直觉的那一半：ssh 连接时，这把 644 的私钥照样「亮」出去了**

```text
$ ssh -F /dev/null -vv -o BatchMode=yes -o IdentitiesOnly=yes -o IdentityAgent=none -i <沙箱>/noperm -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=<沙箱>/kh learner@127.0.0.1 true 2>&1 | grep -E 'identity file|loaded pubkey|Offering|we sent a publickey|bad permissions|Permissions|UNPROTECTED|Permission denied'
debug1: loaded pubkey from <沙箱>/noperm: ED25519 SHA256:KFI9m8A9NHsMG4vFu4n/8KuMlf6Pzi2aqnbMb4bkiLw
debug1: identity file <沙箱>/noperm type 2
debug1: Offering public key: <沙箱>/noperm ED25519 SHA256:KFI9m8A9NHsMG4vFu4n/8KuMlf6Pzi2aqnbMb4bkiLw explicit
debug2: we sent a publickey packet, wait for reply
learner@127.0.0.1: Permission denied (publickey,password).
```

**权限报错没有出现！** 读法（这段是本章最值钱的一页）：

1. `loaded pubkey from ...` —— 公钥部分**不需要**私钥权限检查就能拿到（在文件头部 / 同名 `.pub` 里）。
2. `Offering public key` —— 第一步只递公钥，不签名。
3. 服务器回「这把不认识」（于是 `Permission denied`）→ ssh **根本没用**私钥做签名，
   所以那道「要打开私钥」的权限检查**没有被触发**。

推论：**你看到 `UNPROTECTED PRIVATE KEY FILE` 的时候，服务器其实已经认了你这把公钥**，
只差本地最后一步签名 —— 这是好消息不是坏消息；而你看不到它、只看到 `Permission denied`，
反而说明要查的是服务器端 `authorized_keys`（权限/内容/StrictModes），不是本地。

**标准答案（可背）**

```text
~/.ssh 目录                      700 (drwx------)
私钥 id_xxx                      600 (-rw-------，不能 644)
公钥 id_xxx.pub                  644 (-rw-r--r--)
known_hosts                        644
authorized_keys                    600 或 644（多数发行版 600）
```

**另一套权限检查：配置文件（症状完全不同）**

```text
$ mkdir -p <沙箱>/conf.d; printf 'Include <沙箱>/incperm\n' > <沙箱>/cfg; chmod 600 <沙箱>/cfg; printf 'Host permprobe\n  User zzz\n' > <沙箱>/incperm; chmod 666 <沙箱>/incperm

$ ssh -F <沙箱>/cfg -G permprobe 2>&1 | grep -iE 'bad owner|^user '
Bad owner or permissions on <沙箱>/incperm

$ chmod 644 <沙箱>/incperm; ssh -F <沙箱>/cfg -G permprobe 2>&1 | grep -E '^user |Bad owner'
user zzz

$ chmod 666 <沙箱>/cfg; ssh -F <沙箱>/cfg -G permprobe 2>&1 | grep -E '^user |Bad owner'; chmod 600 <沙箱>/cfg
user zzz
```

三条结论，全都能背：

- **Include 进来的文件别人不可写**：666 → `Bad owner or permissions`，并且**这条 Include 被整段跳过**
  （`user zzz` 消失了，退回 `user <本机用户>`，也就是你本机的登录名）。这就是「我的 Host 段忽然不生效」的隐形原因之一。
  644 就够了，不用 600 —— 门限是**别人不可写**。
- **`-F` 指定的那个文件本身不查权限**（上面第三条：cfg 自己 666 照样读）。别把安全感放这儿。
  你 06 章会撞到的 `/etc/ssh/ssh_config.d/*` 报 `Bad owner or permissions`，走的正是 Include 那条检查。
- **Include 的相对路径是相对 `~/.ssh` 解析的**，不是相对当前配置文件。
  写 `Include incperm` 会去找 `~/.ssh/incperm`，在 `-F` 指向沙箱配置时**静默失效**。
  实验脚本一律写绝对路径（本 demo 就因此从 `Include incperm` 改成了绝对路径）。

**服务端那一侧（StrictModes）—— 需要你自己有服务器时验证**

本机不能改 `/etc/ssh/`、也不制造登录成功，所以这段只给模板，不贴输出：

```text
服务端查配置： sshd -T | grep -i strictmodes
服务端查权限： ls -ld ~/.ssh ~/.ssh/authorized_keys
服务端看日志： sudo journalctl -u ssh -u sshd --since "10 min ago" | grep -i bad
典型日志原文： Authentication refused: bad ownership or modes for directory /home/you/.ssh
```

最后一行那句话说的是**服务端**因为 `~/.ssh` 目录权限过开而拒绝公钥登录；
症状是客户端只看到一句冷冰冰的 `Permission denied (publickey)`，**原因只在服务器日志里**。

---

### 实战 7：known_hosts 的日常维护四件套（demo 第 7 节）

先造一个有两条记录的沙箱 known_hosts（一台机器多把主机密钥是常态）：

```text
$ cd <沙箱> && ssh-keyscan -t ed25519,rsa 127.0.0.1 2>/dev/null > <沙箱>/kh2; cat <沙箱>/kh2
# 127.0.0.1:22 SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
127.0.0.1 ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABgQCv0L/nyt70qt48pb8NnYSiqad3b2hC98Tu7QAJPpxe7lFu/MTuqNPKdXwhlXPoixsisd/vpB0Ldyb3WI0n4rFf/pEODetAjEFNqRxrvzM+NMYutkvJ43nt7OdmKwuPUY2lLolo11U2SmZksID9jI7yfPTRlXngvM5nVBuChoAZs5EcUS0gVKLWoHGP8ZhKt/evUJE0tMFfC00nQ2636SaxXtU5hEXXE1N+De6bgbq6PeytuTxWe/YFSHb9qxJrWC2JfORcY2rzVucuXP82H5VxFNbDdAPayn4dVQGKCNDrHQCsIQpY2CAhaFwIKNz8QXfu+pIk1Q2nygyx7Sqjk/FClNHNPcGJ8XE5bO+wc/LcMMrJEySsvbcKu60BfVfo7JAz/x1pqa2ERLj3W7m9Zi/QIpKE1xEJsq1sAuN7dSX9Gyuhgxwa9agDGj+DOUT23DubDJsqCuI0/jwWx3gEBq5NfWT8ahow/kRUmTXZ1AsDKfxDnudF3Yw1Tkf9p3NK5Us=
# 127.0.0.1:22 SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
127.0.0.1 ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIJ8COfYZlWVtvsITvesDuL0e7PzZiQCCKeFi9lLjMMhY
```

（`#` 开头的是 keyscan 顺便打的版本横幅注释，`grep -v '^#'` 可以丢掉。
同一台机器 rsa / ecdsa / ed25519 各一行是**正常**的，也就意味着它有**多个合法指纹**，
核对时先问清是哪一种。）

> 上面那条 `ssh-rsa` 在屏幕上**就是一整行**（约 590 个字符）。README 不替它折行 —— 折一行就和真跑输出对不上了；看的时候横向滚动就行。

**① `-F` 查**

```text
$ ssh-keygen -F 127.0.0.1 -f <沙箱>/kh2
# Host 127.0.0.1 found: line 2 
127.0.0.1 ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABgQCv0L/nyt70qt48pb8NnYSiqad3b2hC98Tu7QAJPpxe7lFu/MTuqNPKdXwhlXPoixsisd/vpB0Ldyb3WI0n4rFf/pEODetAjEFNqRxrvzM+NMYutkvJ43nt7OdmKwuPUY2lLolo11U2SmZksID9jI7yfPTRlXngvM5nVBuChoAZs5EcUS0gVKLWoHGP8ZhKt/evUJE0tMFfC00nQ2636SaxXtU5hEXXE1N+De6bgbq6PeytuTxWe/YFSHb9qxJrWC2JfORcY2rzVucuXP82H5VxFNbDdAPayn4dVQGKCNDrHQCsIQpY2CAhaFwIKNz8QXfu+pIk1Q2nygyx7Sqjk/FClNHNPcGJ8XE5bO+wc/LcMMrJEySsvbcKu60BfVfo7JAz/x1pqa2ERLj3W7m9Zi/QIpKE1xEJsq1sAuN7dSX9Gyuhgxwa9agDGj+DOUT23DubDJsqCuI0/jwWx3gEBq5NfWT8ahow/kRUmTXZ1AsDKfxDnudF3Yw1Tkf9p3NK5Us=
# Host 127.0.0.1 found: line 4 
127.0.0.1 ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIJ8COfYZlWVtvsITvesDuL0e7PzZiQCCKeFi9lLjMMhY
```

**② `-R` 删**（服务器重装后那句 `REMOTE HOST IDENTIFICATION HAS CHANGED` 的修复动作）

```text
$ ssh-keygen -R 127.0.0.1 -f <沙箱>/kh2
# Host 127.0.0.1 found: line 2
# Host 127.0.0.1 found: line 4
<沙箱>/kh2 updated.
Original contents retained as <沙箱>/kh2.old

$ echo '--- 删完以后：'; cat <沙箱>/kh2; echo '--- 备份：'; ls -l <沙箱>/kh2.old
--- 删完以后：
# 127.0.0.1:22 SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
# 127.0.0.1:22 SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
--- 备份：
-rw-rw-r-- 1 <本机用户> <本机用户> 766 Oct  5 12:57 <沙箱>/kh2.old
```

两个细节：**两条记录一起删**（rsa + ed25519，按主机名匹配而不是按密钥匹配）；
它**先备份成 `.old` 再改**，所以手滑删错还能救。
⚠ 顺序仍然是 01 章那句：**先在别的渠道确认新指纹，再 `-R`**。直接删 = 放弃唯一一次能发现中间人的机会。

**③ `-H` 哈希**

```text
$ cp <沙箱>/kh3 <沙箱>/kh_hash; ssh-keygen -H -f <沙箱>/kh_hash; head -2 <沙箱>/kh_hash
WARNING: <沙箱>/kh_hash.old contains unhashed entries
Delete this file to ensure privacy of hostnames
<沙箱>/kh_hash updated.
Original contents retained as <沙箱>/kh_hash.old
# 127.0.0.1:22 SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6
|1|cOapg+mV45K6fjmJT765ZonkzpE=|Le++3wUomBhr+PA6ZMIYfYejPsY= ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIJ8COfYZlWVtvsITvesDuL0e7PzZiQCCKeFi9lLjMMhY
```

- 格式：`|1|<盐 base64>|<HMAC-SHA1(盐, 主机名) base64> <算法> <公钥>`。
- 哈希的是**主机名**，公钥仍是明文。所以它防的是「别人读你的 known_hosts 看出你连过哪些机器」，
  **不防中间人**（比对逻辑没变）。
- 哈希后 `-F` 照样能查（ssh 拿每个候选主机名现算 HMAC 去比）：

```text
$ ssh-keygen -F 127.0.0.1 -f <沙箱>/kh_hash | head -2
# Host 127.0.0.1 found: line 2 
|1|cOapg+mV45K6fjmJT765ZonkzpE=|Le++3wUomBhr+PA6ZMIYfYejPsY= ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIJ8COfYZlWVtvsITvesDuL0e7PzZiQCCKeFi9lLjMMhY
```

- 副作用：**不能再 `grep 主机名 known_hosts`**（要用 `-F`）；而且那个 `.old` 备份是**明文**的，
  上面那条 WARNING 就是在提醒你这件「哈希了个寂寞」的事，删掉它。

**④ `-c` 改注释**

```text
$ cat <沙箱>/id_ed25519.pub
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIIrNCvZBjA9dBDA5KZ6wmDV/vQ7ckjjrKhPI91WOl+Sp me@laptop

$ ssh-keygen -c -C 'laptop-2026-relabel' -f <沙箱>/id_ed25519 -P ''
Old comment: me@laptop
Comment 'laptop-2026-relabel' applied

$ cat <沙箱>/id_ed25519.pub
ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIIrNCvZBjA9dBDA5KZ6wmDV/vQ7ckjjrKhPI91WOl+Sp laptop-2026-relabel

$ ssh-keygen -lf <沙箱>/id_ed25519.pub
256 SHA256:KFI9m8A9NHsMG4vFu4n/8KuMlf6Pzi2aqnbMb4bkiLw laptop-2026-relabel (ED25519)
```

注释是自由文本：改了它**指纹不变**（`KFI9m8A9…` 前后一字不差），
所以「钥匙名字起错了想改」是零风险操作；
反过来说，**注释不能当身份用** —— 任何人都能写 `root@prod`。

---

### 实战 8：把公钥装上服务器 —— 三条路（demo 第 8 节）

**路线 A：`ssh-copy-id -n`（dry-run，本机可跑，一个字都不写）**

```text
$ ssh-copy-id -n -i <沙箱>/id_ed25519.pub -F /dev/null -o UserKnownHostsFile=<沙箱>/kh -o StrictHostKeyChecking=accept-new -o IdentityAgent=none -o PreferredAuthentications=publickey learner@127.0.0.1 2>&1
/usr/bin/ssh-copy-id: INFO: Source of key(s) to be installed: "<沙箱>/id_ed25519.pub"
/usr/bin/ssh-copy-id: INFO: attempting to log in with the new key(s), to filter out any that are already installed
/usr/bin/ssh-copy-id: INFO: 1 key(s) remain to be installed -- if you are prompted now it is to install the new keys
=-=-=-=-=-=-=-=
Would have added the following key(s):

ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIIrNCvZBjA9dBDA5KZ6wmDV/vQ7ckjjrKhPI91WOl+Sp laptop-2026-relabel
=-=-=-=-=-=-=-=
```

读法：**`-n` 也要真的连一次服务器** —— 因为它得先「拿新钥匙试登录」，
才能过滤掉「已经装过的」（第二行 INFO 说的就是这件事）。
所以 `-n` 能验证的比你想的多：算法能不能谈拢、known_hosts 有没有、这把公钥是否已经装过。

**去掉 `-n` 真跑（本章演的是「停在认证失败」，不是成功）**

```text
$ ssh-copy-id -i <沙箱>/id_ed25519.pub -F /dev/null -o UserKnownHostsFile=<沙箱>/kh -o StrictHostKeyChecking=accept-new -o IdentityAgent=none -o PreferredAuthentications=publickey -o BatchMode=yes learner@127.0.0.1 2>&1
/usr/bin/ssh-copy-id: INFO: Source of key(s) to be installed: "<沙箱>/id_ed25519.pub"
/usr/bin/ssh-copy-id: INFO: attempting to log in with the new key(s), to filter out any that are already installed
/usr/bin/ssh-copy-id: INFO: 1 key(s) remain to be installed -- if you are prompted now it is to install the new keys
learner@127.0.0.1: Permission denied (publickey,password).
```

没有 `Would have added`、更没有 `Number of key(s) added` —— 它连认证都没过去，脚本原样退出
（demo 里这条命令末尾跟了 `; true`，免得这条**预期的失败**把整个脚本带停）。

⚠ 顺带一条实操提醒：这台机器如果**同时**在跑别的 ssh 章节 demo，任何一条要连 22 端口的命令都可能
撞上限流，原文长这样：`ERROR: Connection closed by 127.0.0.1 port 22`。
这**不是配置坏了**：`MaxStartups`（未完成认证的并发连接数）超限，sshd 直接掐 —— 01 章坑 8 是同一件事。
本章 demo 的重试包装（见章末说明）就是为它加的；上一版捕获里这一条就撞了两次，重跑即恢复。
**真服务器上你看到的会是：输一次口令 → `Number of key(s) added: 1` → 之后免密。这段需要你自己有服务器时验证**，模板：

```text
ssh-copy-id -i ~/.ssh/id_ed25519.pub -p 22 user@服务器IP
```

**路线 B：手工贴（只有控制台 / 没有 ssh-copy-id 时）**

```text
客户端： cat ~/.ssh/id_ed25519.pub
服务端： install -d -m 700 ~/.ssh && printf "%s\n" "<上面那一行>" >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys
```

用 `install -d -m 700` 而不是 `mkdir ~/.ssh`，是因为它**顺便把权限定死了**（实战 6 讲过 700）。
追加用 `>>`，千万别 `>`（会覆盖别人/别的设备的钥匙）。

**路线 C：云面板导入** —— 各家控制台的「密钥对」页，贴的还是 `.pub` 那一行；本质 = 路线 B，只是它替你 append。

**authorized_keys 的读法（格式演示，真实生效需服务端配置，见下方标注）**

```text
# 一行一把钥匙；顺序不重要；# 开头是注释
ssh-ed25519 AAAA...comment1
from="10.0.0.7,192.168.1.0/24" ssh-ed25519 AAAA...只允许这些来源
command="/usr/bin/rsync --server",no-pty,no-agent-forwarding ssh-ed25519 AAAA...只允许跑 rsync（部署 key 的原理）
restrict ssh-ed25519 AAAA...关掉几乎所有能力（新版本里的总开关）
```

- 结构：`[选项] <算法> <blob> [注释]`。**选项和钥匙必须在同一行** —— 选项单独成行是无效写法。
- `command=` 就是 GitHub 那种「deploy key 只能拉不能登」的原理；配合 `no-pty` 更严。
- `restrict` 是新版的「一键全关」，再单独放开需要的能力（比逐个 `no-xxx` 更不容易漏）。
- 这些行的**生效**需要服务端 `AuthorizedKeysFile` 指向它、语法完全正确（写错一个逗号整行报废）。
  **需要你自己有服务器时验证**，命令模板：

```text
服务端： sshd -T | grep -iE "authorizedkeysfile|permitrootlogin|passwordauthentication"
强制命令（deploy key / 备份账号常用）：
command="/usr/bin/rsync --server --sender -logDtpre.iLsfxCIvu . /data/backup",no-port-forwarding,no-agent-forwarding,no-pty,no-user-rc ssh-ed25519 AAAA...
客户端验证： rsync -av 服务器:/data/backup/ ./local/  —— 只能拉，登不进 shell
```

---

### 实战 9：证书 —— 把「每台机器贴一次公钥」变成「信任一个 CA」（demo 第 9 节）

```text
$ cd <沙箱> && ssh-keygen -t ed25519 -C 'demo-ca' -f <沙箱>/ca_key -N '' -q && ssh-keygen -lf <沙箱>/ca_key.pub
256 SHA256:TeR13u1zF+86RsiZmrHIg5JGzfJEheL2F2lF0amggqo demo-ca (ED25519)

$ cd <沙箱> && ssh-keygen -s <沙箱>/ca_key -I 'demo-identity' -n learner -V +52w -z 42 <沙箱>/id_ed25519.pub
Signed user key <沙箱>/id_ed25519-cert.pub: id "demo-identity" serial 42 for learner valid from 2026-10-05T12:56:00 to 2027-10-04T12:57:16

$ ssh-keygen -Lf <沙箱>/id_ed25519-cert.pub
<沙箱>/id_ed25519-cert.pub:
        Type: ssh-ed25519-cert-v01@openssh.com user certificate
        Public key: ED25519-CERT SHA256:KFI9m8A9NHsMG4vFu4n/8KuMlf6Pzi2aqnbMb4bkiLw
        Signing CA: ED25519 SHA256:TeR13u1zF+86RsiZmrHIg5JGzfJEheL2F2lF0amggqo (using ssh-ed25519)
        Key ID: "demo-identity"
        Serial: 42
        Valid: from 2026-10-05T12:56:00 to 2027-10-04T12:57:16
        Principals: 
                learner
        Critical Options: (none)
        Extensions: 
                permit-X11-forwarding
                permit-agent-forwarding
                permit-port-forwarding
                permit-pty
                permit-user-rc
```

**读法（每一行都有用）**

- 证书**不产生新的密钥对**：`Public key: ... SHA256:KFI9m8A9…` 和裸公钥的指纹**完全相同**，
  它只是给同一把公钥附加了「CA 的签名 + 有效期 + 谁可以用 + 允许干什么」。
  私钥还是原来那把，`ssh-add` 的用法不变。
- `-I` = Key ID（写日志用），`-n learner` = principals（**服务器上的用户名**要和它匹配），
  `-V +52w` = 有效期，`-z 42` = 序列号（吊销时靠它）。
- `Extensions` 那五项就是「这张证书被允许做的事」。生产上应**收紧**：
  例如去掉 `permit-user-rc`、`permit-agent-forwarding`，比在 authorized_keys 里堆 `no-xxx` 干净得多。
- 文件名必须是 `<私钥名>-cert.pub`，而且**旁边那个不带 `-cert` 的 `.pub` 不能删**：

```text
$ ls -l <沙箱> | grep id_ed25519
-rw------- 1 <本机用户> <本机用户>  411 Oct  5 12:57 id_ed25519
-rw-r--r-- 1 <本机用户> <本机用户>  658 Oct  5 12:57 id_ed25519-cert.pub
-rw-r--r-- 1 <本机用户> <本机用户>  101 Oct  5 12:57 id_ed25519.pub
-rw-rw-r-- 1 <本机用户> <本机用户>  201 Oct  5 12:57 id_ed25519.rfc4716
```

  ssh 是靠同名 `.pub` 找回原始公钥、再带上证书一起递给服务器的；删了 `.pub` 证书等于没用。
- 服务器侧要配 `TrustedUserCAKeys`（或 signed CA 的 host key 走 `@cert-authority`），
  **需要你自己有服务器时验证**。本地能验证的只有「签得出、读得懂」这一半。

⚠ 顺手一个辨识度技巧：`ssh-keygen -lf` 对证书会打 `(ED25519-CERT)`，
普通公钥打 `(ED25519)` —— **看到 `-CERT` 后缀就知道服务器上是证书方案，不是裸公钥**。

---

## 命令速查表

| 我想干什么 | 命令 |
|---|---|
| 生成一把现代 key | `ssh-keygen -t ed25519 -C '用途@机器' -f ~/.ssh/id_xxx`（不设口令再加 `-N ''`，一般**不该**不设） |
| 看看自己有哪些 key | `ls -l ~/.ssh/*.pub` → `for f in ~/.ssh/*.pub; do ssh-keygen -lf "$f"; done` |
| 算指纹（公钥/私钥/known_hosts 都能喂） | `ssh-keygen -lf <文件>`；老 MD5 加 `-E md5`；证书用 `-Lf` 看详情 |
| 从私钥只导出公钥（不泄露私钥） | `ssh-keygen -y -f <私钥>` |
| 换口令（不动公钥，不用重部署） | `ssh-keygen -p -f <私钥>`；脚本里 `-P 旧 -N 新`；轮数 `-a 100` |
| 改公钥那行的注释（零风险） | `ssh-keygen -c -C '新注释' -f <私钥>` |
| 修「UNPROTECTED PRIVATE KEY FILE」 | `chmod 600 <私钥> && chmod 700 ~/.ssh` |
| 修「Bad owner or permissions on…」 | 对**被 Include 的文件/配置目录**去写权限：`chmod 644`（门限：别人不可写） |
| 查 known_hosts 里某台机器 | `ssh-keygen -f ~/.ssh/known_hosts -F <主机名>`（哈希过也能查） |
| 删 known_hosts 里某台机器 | `ssh-keygen -f ~/.ssh/known_hosts -R <主机名>`（自动留 `.old`） |
| 把 known_hosts 全部哈希 | `ssh-keygen -H -f ~/.ssh/known_hosts`，然后**删掉 `.old`** |
| 抓主机公钥（**不含任何信任**） | `ssh-keyscan -t ed25519 <主机>`；直接算指纹：`… \| ssh-keygen -lf -` |
| 把公钥装上服务器 | `ssh-copy-id -i ~/.ssh/id_ed25519.pub user@服务器`（先 `-n` 试一遍） |
| 起一个专用 agent / 加钥匙 / 看 / 删 | `eval "$(ssh-agent -s)"`；`ssh-add <私钥>`；`ssh-add -l` / `-L`；`ssh-add -d` / `-D` |
| 这次连接不许用 agent | `ssh -o IdentityAgent=none ...` |
| 只允许指定那把 key | `ssh -o IdentitiesOnly=yes -i <私钥> ...` |
| 格式转换（老面板要 RFC 4716） | 出去 `ssh-keygen -e -f x.pub`；回来 `ssh-keygen -i -m RFC4716 -f x.rfc4716`（**不能用管道**） |
| 签一张用户证书 / 读证书 | `ssh-keygen -s ca_key -I id -n <用户> -V +52w -z 42 id.pub` / `ssh-keygen -Lf id-cert.pub` |
| 检查配置到底被读到没有 | `ssh -F <文件> -G <主机>`（03 章的支柱；本章用它验 Include 权限） |

---

## 这章的坑（本机全部踩过/复现过）

1. **`-t ed25519 -b 2048` 不报错、也不生效。** 位数被静默忽略（实测指纹还是 256）。
   任何「我明明设了强度」的自信，都要用 `ssh-keygen -lf` 复核一遍。
2. **以为 RSA 默认 4096。** 默认 **3072**（实测）。安全基线写「RSA ≥ 4096」的场合，必须显式 `-b 4096`。
3. **拿 MD5 指纹去比 SHA256 指纹**，得出「服务器被攻击了」的假警报。核对前先问清是哪一种摘要、
   以及是哪一把主机密钥（一台机器同时有 rsa/ecdsa/ed25519 三个合法指纹）。
4. **以为口令保护的是「密钥」。** 它保护的是磁盘文件；口令一旦给出（或被 agent 记住），
   签名能力和裸私钥等价。口令的价值只在「文件被拷走之后」。
5. **看到 `UNPROTECTED PRIVATE KEY FILE` 就以为钥匙错了。** 恰恰相反：报错出现在
   **服务器已经接受这把公钥、本地要签名**的那一刻（实战 6 现场演示：同一把 644 私钥
   在认证被拒时**根本不报这个错**）。看到它 = 方向对了，`chmod 600` 就完事。
6. **以为「只要不是 644 就行」。** 门限是**同组和其他人都不许有任何访问权**。
   demo 没演这条，我用同一套离线命令（`ssh-keygen -y`）单独复跑验证过：
   `400`、`600` 通过；`640`、`644`、`664` 全部报 `Permissions 0xxx … are too open`
   —— 注意 `640` 也只是「同组可读」就足以触发。
   而**目录权限**（`~/.ssh` 要 700）这一侧主要由服务端 `StrictModes` 管（实战 6）。
7. **`Bad owner or permissions` 不一定是私钥的锅。** 大概率是**配置文件 / 被 Include 的文件**权限过开
   （门限：别人不可写，644 即可）。而且此时那条 Include 会被**整段静默跳过**，
   表现成「我的 Host 配置忽然不生效了」，比报错更难查。
8. **`Include` 写相对路径 = 相对 `~/.ssh` 解析**，不是相对当前配置文件（实测）。
   `-F 沙箱配置` 里写 `Include incperm` 会去找 `~/.ssh/incperm`，演示会「看起来通过了」但其实没读到。
9. **`ssh-keygen -i` 不吃管道。** 必须 `-f <文件>`；不给 `-f` 它会问你默认路径。跨格式转换脚本里
   这是最常见的静默失败点。
10. **用 `>` 覆盖 `authorized_keys`。** 一行就抹掉别人/别的设备所有钥匙，
    而且是「把自己锁在门外」的经典自伤。永远 `>>`，或者先 `cp -a` 备份。
11. **以为 `-n`（ssh-copy-id dry-run）不连网络。** 它会真连一次（要试登录才能过滤已装钥匙），
    所以 known_hosts、算法协商这些毛病在 `-n` 阶段就能暴露 —— 这是优点，别跳过。
12. **本机演示撞限流被误读成配置坏了。** `ERROR: Connection closed by 127.0.0.1 port 22`
    是 `MaxStartups`（并发未完成认证连接）超限；脚本里连得太密就会看到，**隔几秒重试即可**。
13. **删了 `id_ed25519.pub` 只留 `-cert.pub`，然后奇怪证书怎么没用。** OpenSSH 靠同名不带
    `-cert` 的 `.pub` 找原始公钥（实测那三个文件必须同时在）。
14. **公钥文件里的注释当身份用。** `ssh-keygen -c` 实测改注释**不动指纹**，
    所以注释里的 `root@prod` 没有任何含金量 —— 它是给你自己看的标签，不是凭据。

---

## 自检问题

1. 你要给运维同事发一把公钥，发文件前应该顺口报哪一行？为什么不是发 base64 全文？
   **答**：报 `ssh-keygen -lf <公钥>` 那行（`256 SHA256:KFI9m8A9… (ED25519)`）。
   指纹 43 个字符，可以电话/IM 逐字核对；base64 公钥几百字符，人核对不了。

2. `ssh-keygen -t ed25519 -b 4096` 的结果是什么？怎么验证你的判断？
   **答**：正常生成一把 256 位 ed25519，`-b` 被静默忽略、不报错。
   验证：`ssh-keygen -lf id.pub` 开头是 `256`（demo 第 1 节实测输出就是这个）。

3. 私钥换了口令，服务器上要改动什么？
   **答**：什么都不用改。`ssh-keygen -p` 只重加密磁盘文件，公钥和指纹不变
   （demo 第 4 节：`-p` 前后 `LPNzBxLy…` 完全一致）。要改的只有你本地 agent 里那份旧的 —— 重新 `ssh-add`。

4. 同一把私钥，`644` 权限下 `ssh` 连一台「没装你公钥」的服务器，为什么不报 UNPROTECTED？
   而连一台「装了你的公钥」的服务器就报了？
   **答**：认证的第一步只递**公钥**（`Offering public key`），不需要打开私钥，所以权限检查没被触发；
   服务器拒了就没有下一步。只有服务器**认这把公钥**、客户端要做签名时才会读私钥 → 报错。
   所以这个报错是「钥匙对了、只差本地权限」的信号（demo 第 6 节）。

5. `Bad owner or permissions on /etc/ssh/ssh_config.d/xxx.conf` 是私钥的问题吗？后果是什么？
   **答**：不是，那是配置/Include 的权限检查（门限「别人不可写」）。后果是**该文件被整段跳过**，
   里面的 Host 段全部失效，症状像「配置莫名不生效」而不是「连不上」（demo 第 6 节实测 666 vs 644）。

6. 第一次连服务器，你已经从云控制台拿到了 `SHA256:ZiBpW4…`，
   提问那句 `(yes/no/[fingerprint])?` 里最稳的回答方式是什么？
   **答**：把**指纹本身**当答案打进去。一致才写入 known_hosts；`yes` 只是「我不核对，先签了」。

7. `ssh-copy-id -n` 报 `Host key verification failed`，但 `ssh -n` 同名主机能连，可能差在哪？
   **答**：`ssh-copy-id` 自己拼 ssh 参数，`-o` 要**写在 `-i` 之后、目标之前**才会透传；
   另外它一定要先「试登录一次」，所以 known_hosts / 算法协商 / agent 任一环节都会在这里暴露。
   排错先看它 INFO 那两行停在哪一步（demo 第 8 节）。

8. 一张证书和一把裸公钥，指纹一样吗？怎么一眼分辨服务器用的是哪种？
   **答**：一样（证书不产生新密钥，`-Lf` 里 `Public key: ED25519 SHA256:KFI9m8A9…` 与裸公钥同值）。
   分辨方法：`ssh-keygen -lf` 的类型标注带 `-CERT` 后缀（`(ED25519-CERT)`）。

---

## 怎么跑这章的 demo

```bash
cd ssh-handbook/02-keys
bash demo.sh          # 跑完自动删沙箱、自动杀掉它起的 ssh-agent
bash demo.sh --keep   # 保留沙箱并打印路径，进去自己接着敲；看完自己 rm -rf
```

一次真跑的规模：**680 行输出、退出码 0、10 个场景**（沙箱 `<沙箱>`）。
场景顺序 = 本章实战顺序：

| demo 场景 | 内容 |
|---|---|
| 1 | 三种算法生成 + 计时 + 权限 + 默认位数 + `-b` 静默忽略 |
| 2 | 公钥那行拆开、`-y` 单向、blob 结构、坏公钥、格式互转 |
| 3 | 指纹三种摘要、私钥=公钥、`keyscan \| ssh-keygen -lf -` |
| 4 | 口令：文件变大、`-p` 换口令不动指纹、错口令报错、KDF `-a` 计时 |
| 5 | 沙箱 agent：起 / 空 / 加 / 列 / `-L` / 真连（`agent` 标签）/ 删 / `IdentityAgent=none` 屏蔽 |
| 6 | 权限模型：644 私钥的两条报错、ssh 为何没报、配置 Include 权限 666 vs 644 |
| 7 | known_hosts：keyscan 造记录、`-F` 查 / `-R` 删（留 `.old`）/ `-H` 哈希 / `-c` 改注释 |
| 8 | `ssh-copy-id -n` 原文 + 真跑 + authorized_keys 格式与限制选项 |
| 9 | 证书：建 CA、签 52 周、`-Lf` 读全部字段、`-CERT` 指纹 |
| 10 | 收尾自证：`ss -lntp` 监听端口、`pgrep -af ssh-agent`、沙箱文件权限清单 |

**demo 里已验证 / 需要你自己有服务器才能验证的**

- 已验证（本章输出随手可查）：生成与计时、权限位、三种摘要指纹、keyscan 与主机指纹、
  口令前后指纹一致、agent 全流程与 `agent` 标签、`IdentityAgent=none` 屏蔽、
  权限两类报错、known_hosts 四个开关、证书签发与解析、`-n` dry-run 原文。
- **需要你自己有服务器时验证**：`Number of key(s) added: 1` 与之后免密登录、
  `Server accepts key` / `Authentication succeeded` 那几行（本章一律停在 `Permission denied`）、
  服务端 `StrictModes` 拒绝的真实日志、`authorized_keys` 的 `from=`/`command=` 实际生效、
  `TrustedUserCAKeys` 证书登录、服务器本机 `ssh-keygen -lf /etc/ssh/ssh_host_*_key.pub`。
  文中对应位置都写了「未验证/需你自己验证」并给了命令模板，**没有编造任何成功输出**。

**安全**：本 demo 不监听任何端口（`ss -lntp` 自证）、只连 `127.0.0.1`、
不读写 `~/.ssh`、不碰 `/etc/ssh`、不重启系统 sshd（22 端口上跑的是真实系统 sshd，只做被动观察）、
不起 authorized_keys、不猜密码；起的唯一后台进程是沙箱内的 `ssh-agent`，
退出时由 `trap` 杀掉并打印 pid。全程没有一条 `cat`/`head`/`tail`/`dd` 落在私钥文件上
（场景 10 逐条自证文件清单）。

⚠ 关于 askpass 的一句提醒（本章 demo 曾经在这里挂死）：脚本喂口令要靠
`SSH_ASKPASS=<辅助脚本> SSH_ASKPASS_REQUIRE=force`，而那个辅助脚本**必须输出「口令 + 换行」**。
写成 `printf %s\n`（dash 下变成字面量 `n`、行尾没有换行）会让 `ssh-add` **100% CPU 死循环**挂住；
本 demo 已改成 heredoc + `echo`。你写 CI 脚本时值得记住这个坑。

---

## 下一步

- **04-agent-and-multiplexing**：`SSH_AUTH_SOCK` 为什么换个终端就失效、agent 里钥匙太多的后果、
  agent 上锁、以及 `ControlMaster` 连接复用与 `ServerAliveInterval` 保活。
  本章只教你「把钥匙交给 agent」，04 章教你「这套机制在真实一天里怎么坏掉」。
- **03-config-file**：把本章那串 `-o IdentityAgent=none -o IdentitiesOnly=yes -o UserKnownHostsFile=…`
  变成 `~/.ssh/config` 里的三行（效率提升最大的一章）。
- **06-troubleshooting**：`Permission denied (publickey)`、`UNPROTECTED PRIVATE KEY FILE`、
  `Bad owner or permissions`、`Too many authentication failures` 逐条对照本章的原文。
- 现在就可以做的动作：`for f in ~/.ssh/*.pub; do ssh-keygen -lf "$f"; done`
  把你手上每把 key 的指纹、类型、位数抄下来 —— 顺便就能发现自己有几把是 2048 的 RSA、
  有几把 `id_rsa` 从来没在用。
