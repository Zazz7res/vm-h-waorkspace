#!/usr/bin/env bash
# ============================================================================
# 03 SSH 配置文件（客户端侧）+ 核心武器 ssh -G —— 一键复现整章场景
#
# 用法：
#   bash demo.sh            # 跑完自动杀掉自己起的进程、删掉沙箱
#   bash demo.sh --keep     # 保留沙箱（打印路径，你可以进去接着敲 ssh -F config -G box-a）
#
# 本章复现什么：
#   * 配置来源与优先级：命令行 -o  >  用户/指定的配置文件  >  /etc/ssh/ssh_config（只读观察）
#   * Host 模式匹配：* ? ! 、多模式空格并排、逗号在 Host 行里【不是】分隔符
#   * 全章最重要的机制：first-obtained-wins（第一个拿到的值生效）—— 逐条实测
#   * Match：all / host(逗号表) / user / localuser / exec / final / canonical / !xxx，
#     以及 -G 打的「第二遍解析」
#   * 常用关键字逐个用 ssh -G 看最终值：HostName User Port IdentityFile IdentitiesOnly
#     ProxyJump ProxyCommand ForwardAgent ServerAliveInterval ServerAliveCountMax
#     ControlMaster ControlPath ControlPersist StrictHostKeyChecking UserKnownHostsFile
#     ConnectTimeout Canonicalize* Include
#   * Include 分片管理（config.d 思路）+ 配置文件权限门限实测
#   * 三个真实场景完整配置块，每个都用 ssh -G 验证生效
#   * 一整面「真实报错墙」：拼错关键字 / 端口写错 / Match 用法错 / 权限不对
#
# 安全声明（照着抄到你自己的实验里）：
#   * 所有配置、密钥、known_hosts、控制套接字目录都在 mktemp 沙箱里：
#     绝不读写 ~/.ssh/，绝不改 /etc/ssh/。/etc/ssh/ssh_config 只做【只读观察】。
#   * 每条 ssh 一律带 -F 沙箱配置（或 -F /dev/null / -F /etc/ssh/ssh_config）。
#     -F 一旦指定，你真实的 ~/.ssh/config 和系统配置【都不读】。
#   * 每条 ssh 都带 -o IdentityAgent=none：不借用你真实 agent 里的钥匙。
#   * 少数几条【会去连接】的 ssh 一律带上这组护栏（脚本里叫 $CS，打印出来照抄即可）：
#     -o BatchMode=yes（永不弹口令）
#     -o IdentitiesOnly=yes -o IdentityFile=<沙箱key>（不会去打开 ~/.ssh/id_rsa 这些真实文件）
#     -o UserKnownHostsFile=<沙箱文件>（不会读/写你真实的 known_hosts）
#   * 本 demo【一次都不连 22 端口】：所有 HostName 都写死成 127.0.0.1 的高端口，
#     端口是脚本向内核现要的空闲端口。场景 6 里的 Connection refused /
#     banner exchange timeout / proxy 执行失败 全是【故意制造的预期失败】，
#     失败信息里的地址和端口正是「配置文件生效」的证据。
#   * 假服务端只用 nc 监听 127.0.0.1 高端口，并且都套了 timeout 兜底；
#     收尾用 ss -lnt 证明端口已释放、进程已杀干净。
#   * 全程不打印任何私钥内容（cat/head/tail/dd 一个都不碰私钥文件）；公钥可以。
#   * 场景 4 会现场证明：配置里的 Match exec 会被【真的执行】（退出码 0 才算匹配），
#     而且 ssh -G 就会执行它。本 demo 只 exec 一条 touch/echo 到沙箱里；
#     你抄这个实验时也别放有副作用的命令。
#   * 场景 7 有一条【故意不跑】的实验：相对路径 Include 会按 ~/.ssh 解析 —— 那等于
#     去读你真实的 ~/.ssh，本章红线不允许，所以只在手册原文里给结论并标注未验证。
#   * 复用连接真正生效（ControlMaster 建套接字）需要「能登录成功」的服务器，
#     本章红线不制造登录成功，所以那部分是【未验证】，脚本里也明说了。
#   标注【未验证】的三条（收尾自证里也列了一遍）：
#     1) ControlSocket ... already exists, disabling multiplexing 这类复用成功日志；
#     2) 真实 ~/.ssh/config 权限不对时的 Bad owner or permissions on ~/.ssh/config；
#     3) 相对路径 Include 落在 ~/.ssh 下的真实效果。
# ============================================================================
set -euo pipefail
export LC_ALL=C.UTF-8 LANG=C

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

SB=$(mktemp -d "${TMPDIR:-/tmp}/ssh03-XXXXXX")
chmod 700 "$SB"
mkdir -p "$SB/keys" "$SB/config.d" "$SB/cm"
: > "$SB/kh"
# 开工前先把「本机原有的 LISTEN 端口」存档，收尾拿它做 diff 自证
ss -lnt 2>/dev/null | tail -n +2 | awk '{print $4}' | sort > "$SB/listen_before.txt"

# ---- 端口：向内核要三个空闲端口，只在 127.0.0.1 上用，绝不碰 22 -------------
free_port() {
    python3 - <<'PY'
import socket
s = socket.socket()
s.bind(("127.0.0.1", 0))
print(s.getsockname()[1])
s.close()
PY
}
DEADPORT=$(free_port)    # 什么都不监听  => Connection refused
FAKEPORT=$(free_port)    # nc 只吐一行 SSH-2.0 横幅 => 算法协商阶段就没下文
SLOWPORT=$(free_port)    # nc 连着但一个字不发 => banner exchange 超时

# CS：连接类命令的统一护栏（会被展开进 raw 打印的命令里，你可以直接复制）
ssh-keygen -t ed25519 -C 'sandbox-03-demo' -f "$SB/keys/id_demo" -N '' -q >/dev/null 2>&1
CS="-o BatchMode=yes -o IdentityAgent=none -o IdentitiesOnly=yes -o IdentityFile=$SB/keys/id_demo -o UserKnownHostsFile=$SB/kh"

PIDS=()
start_fake_banner_server() {   # 假服务端①：发一行横幅后不再说话
    (printf 'SSH-2.0-FakeDemoServer\r\n'; sleep 3) | timeout 15 nc -l 127.0.0.1 "$FAKEPORT" -w 10 \
        >"$SB/nc_fake_capture.txt" 2>&1 &
    PIDS+=($!)
    printf '  [沙箱假服务端] 127.0.0.1:%s 开始监听（后台 pid=%s，timeout 15 + 空闲 10 秒双重兜底）\n' \
        "$FAKEPORT" "$!"
    sleep 0.4
}
start_silent_server() {        # 假服务端②：接了连接但什么都不发
    timeout 15 nc -l 127.0.0.1 "$SLOWPORT" -w 12 >/dev/null 2>&1 &
    PIDS+=($!)
    printf '  [沙箱假服务端] 127.0.0.1:%s 开始监听（后台 pid=%s，什么都不发）\n' "$SLOWPORT" "$!"
    sleep 0.4
}

cleanup() {
    for p in "${PIDS[@]:-}"; do kill "$p" 2>/dev/null || true; done
    sleep 0.3
    for p in "${PIDS[@]:-}"; do kill -9 "$p" 2>/dev/null || true; done
    printf '\n[收尾 1] 本脚本记下的后台 PID：%s —— 已全部 kill\n' "${PIDS[*]:-无}"
    ncp=$(pgrep -xc nc 2>/dev/null || true)
    printf '  现在系统上还剩几个 nc 进程：%s（0 = 干净）\n' "${ncp:-0}"
    printf '[收尾 2] 监听端口和开工前基线做 diff（空 = 本章起过的端口全释放了）\n'
    printf '         本章用过的三个沙箱端口：%s / %s / %s\n' "$DEADPORT" "$FAKEPORT" "$SLOWPORT"
    ss -lnt 2>/dev/null | tail -n +2 | awk '{print $4}' | sort > "$SB/listen_after.txt"
    if diff "$SB/listen_before.txt" "$SB/listen_after.txt"; then
        printf '  干净：diff 无输出，一个端口都没留下（基线如下）\n'
        sed 's/^/     /' "$SB/listen_before.txt"
    else
        printf '  ⚠ 上面 diff 里带 > 的端口是新增监听，需要人工确认\n'
    fi
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去自己敲： cd "%s" && ssh -F config -G box-a\n  看完自己删： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

# ---- 输出助手 -------------------------------------------------------------
scene() { printf '\n\n================ 场景 %s ================\n' "$*"; }
say()   { printf '\n--- %s\n' "$*"; }
raw()     { printf '\n$ %s\n' "$*"; bash -c "$*" || printf '  [退出码 %s：多半是 grep 没匹配到，不是脚本报错]\n' "$?"; }
mayfail() { printf '\n$ %s\n' "$*"; bash -c "$*" || printf '  （退出码 %s —— 这条就是要看它失败/报错）\n' "$?"; }
export -f start_fake_banner_server start_silent_server free_port
export SB DEADPORT FAKEPORT SLOWPORT CS

printf '沙箱：%s\n' "$SB"
printf '本机 ssh： %s\n' "$(ssh -V 2>&1)"
printf '沙箱端口： 空口=%s 假横幅=%s 静默=%s（全是 127.0.0.1 高端口，一次都不碰 22）\n' \
    "$DEADPORT" "$FAKEPORT" "$SLOWPORT"

# ============================================================================
scene "0：先认识本章的武器 ssh -G（它不开连接，所以可以放心敲）"
# ============================================================================
say "是什么：把「这个主机名最终会被解析成什么」整份打出来，然后立刻退出；不建 TCP、不认证"
raw "ssh -G -F /dev/null demo.example 2>&1 | head -5"
say "上面第一行 Pseudo-terminal 警告是 stderr 的固有噪音（stdin 不是 tty 就来一句），不是配置错误。"
say "本 demo 用 2>&1 保留真报错、只滤掉这一行；你交互敲的时候不用理会它。"
raw "ssh -G -F /dev/null demo.example 2>&1 | grep -c '^Pseudo-terminal'"
raw "ssh -G -F /dev/null demo.example 2>/dev/null | wc -l"
say "它为什么安全：-G 只读配置文件，连身份文件都不打开（下面 grep 三条：只出现读配置那一行）"
raw "ssh -F /dev/null -vvv -G demo.example 2>&1 | grep -E 'Reading configuration data|loaded pubkey|identity file'"
say "对照：真去连接时日志立刻会出现下面这种行（这就是本 demo 只在高端口上跑连接的原因）"
raw "printf '%s\n' 'debug1: loaded pubkey from /home/you/.ssh/id_ed25519: ED25519 SHA256:...' 'debug1: identity file /home/you/.ssh/id_ed25519 type 2'"
say "-G 的输出里会出现你真实 ~/.ssh 路径的【字符串】（编译进来的默认值），但它只是打印，不会去读："
raw "ssh -G -F /dev/null demo.example 2>/dev/null | grep -E '^(hostname|user|userknownhostsfile|identityfile) '"

# ============================================================================
scene "1：配置文件在哪、优先级从高到低（全程 -F 隔离，绝不碰你真实的 ~/.ssh/config）"
# ============================================================================
say "手册原文（man ssh_config 开头两段）—— 本章整章就建在这两句话上"
raw "man ssh_config 2>/dev/null | col -b 2>/dev/null | sed -n '/obtains configuration data/,/relevant lines from these/p' | head -10"
mayfail "man ssh_config 2>/dev/null | col -b 2>/dev/null | grep -m1 -A2 'first obtained value'"
raw "man ssh_config 2>/dev/null | col -b 2>/dev/null | grep -m1 -B1 -A1 'general defaults at the end'"
raw "man ssh_config 2>/dev/null | col -b 2>/dev/null | grep -m1 -A3 'ssh_config.d/\\*.conf files are included'"
say "1a) 优先级最高的是命令行 -o：沙箱配置里写的是 box-user，命令行一压就赢"
cat > "$SB/config" <<EOF
# 沙箱主配置（本 demo 唯一被 -F 指向的主文件）
Host box-a
  HostName 127.0.0.1
  Port $DEADPORT
  User box-user
  IdentityFile $SB/keys/id_demo
  IdentitiesOnly yes
  UserKnownHostsFile $SB/kh
EOF
raw "cat -n $SB/config"
raw "ssh -F $SB/config -G box-a 2>/dev/null | grep -E '^(host|hostname|port|user|identitiesonly) '"
raw "ssh -F $SB/config -o User=from-command-line -G box-a 2>/dev/null | grep -E '^user '"
say "1b) -F 一指定，用户配置和系统配置【都不读】：看 Reading configuration data 出现的行数"
raw "printf '沙箱配置跑一遍，读文件次数 = ' ; ssh -F $SB/config -vvv -G box-a 2>&1 | grep -c 'Reading configuration data'"
raw "ssh -F /dev/null -vvv -G box-a 2>&1 | grep 'Reading configuration data'"
say "1c) /etc/ssh/ssh_config 只读观察（只 cat / 只 -G，一个字都不写）：它自己的 Host * 会改默认值"
raw "grep -vE '^[[:space:]]*#|^[[:space:]]*\$' /etc/ssh/ssh_config | head -14"
raw "ls -l /etc/ssh/ssh_config.d/"
raw "ssh -G -F /etc/ssh/ssh_config box-a 2>/dev/null | grep -E '^(hashknownhosts|gssapiauthentication|sendenv|stricthostkeychecking) '"
say "同一个主机名，-F 沙箱 和 -F 系统配置 给出不同答案 => 证明 -F 把整条读取链都换掉了"
raw "ssh -G -F $SB/config box-a 2>/dev/null | grep -E '^(hashknownhosts|gssapiauthentication) '"
say "顺手看到系统里塞的机关：主机名写成 unix/... vsock/... machine/... 会走本地 socket，"
say "而且它自带 StrictHostKeyChecking no + UserKnownHostsFile /dev/null —— 这解释了"
say "「为什么有人连 machine/foo 从来不问 yes/no」（生产上想关它就在自己配置里覆盖）"
raw "ssh -G -F /etc/ssh/ssh_config 'unix/run/ssh-unix-local/socket' 2>/dev/null | grep -E '^(proxycommand|proxyusefdpass|stricthostkeychecking|userknownhostsfile|checkhostip) '"
say "1d) 不加 -F 就是默认三层（你的 ~/.ssh/config + /etc/ssh/ssh_config + 系统分片）。"
say "本手册红线不读你真实的 ~/.ssh/config，所以这条【不跑】。你想亲眼看到三行，自己在终端敲："
raw "echo 'ssh -vvv -G somehost 2>&1 | grep \"Reading configuration data\"'"

# ============================================================================
scene "2：first-obtained-wins —— 全章最重要的机制（配错文件 90% 是它）"
# ============================================================================
say "2a) 同一关键字写两次：只有【第一次】生效，后一句被静默丢弃（不报错、不警告）"
raw "printf 'Host dup\n  User first-user\n  User second-user\n  Port 2200\n  Port 2201\n' > $SB/cfg2a"
raw "ssh -G -F $SB/cfg2a dup 2>/dev/null | grep -E '^(user|port) '"
say "2b) 两个同名 Host 块分属【不同】关键字：都生效，跟块的位置无关 —— 规则是「按关键字」不是「按块」"
raw "printf 'Host dup\n  User first-user\n\nHost dup\n  HostName 10.0.0.9\n  Port 2200\n' > $SB/cfg2b"
raw "ssh -G -F $SB/cfg2b dup 2>/dev/null | grep -E '^(user|hostname|port) '"
say "2c) 例外（最常用的那个）：IdentityFile 是【叠加】的，写几条就列几条，列出的顺序就是逐把试钥匙的顺序"
raw "printf 'Host addi\n  IdentityFile $SB/keys/id_first\n  IdentityFile $SB/keys/id_second\n  IdentitiesOnly yes\n' > $SB/cfg2c"
raw "ssh -G -F $SB/cfg2c addi 2>/dev/null | grep -E '^(identityfile|identitiesonly) '"
say "2d) 【最典型的错法】Host * 写在文件最前面：通配先把 User 抢走，具体别名永远轮不到"
raw "printf 'Host *\n  User wildcard-first\n\nHost box-a\n  User box-user\n  HostName 10.0.0.1\n' > $SB/cfg2d"
raw "ssh -G -F $SB/cfg2d box-a 2>/dev/null | grep -E '^(user|hostname) '"
say "解读：user 被 Host * 抢了（这就是「连上了但用户名不对」的成因），hostname 反倒生效了"
say "（因为 Host * 那块没写 HostName）。一半生效一半不生效，是最难排查的症状长相。"
say "2e) 正解：具体别名在前、Host * 兜底在最后（系统配置里那句注释就是在说这个）"
raw "printf 'Host box-a\n  User box-user\n  HostName 10.0.0.1\n\nHost *\n  User wildcard-last\n' > $SB/cfg2e"
raw "ssh -G -F $SB/cfg2e box-a 2>/dev/null | grep -E '^(user|hostname) '"
say "2f) 同一条规则也适用于「对某台机器开小灶」：先设的值把后面的挡住"
raw "printf 'Host *\n  ServerAliveInterval 0\n  ForwardAgent no\n\nHost longline\n  ServerAliveInterval 60\n  ForwardAgent yes\n' > $SB/cfg2h"
raw "ssh -G -F $SB/cfg2h longline 2>/dev/null | grep -E '^(serveraliveinterval|forwardagent) '"
raw "printf 'Host longline\n  ServerAliveInterval 60\n  ForwardAgent yes\n\nHost *\n  ServerAliveInterval 0\n  ForwardAgent no\n' > $SB/cfg2h2"
raw "ssh -G -F $SB/cfg2h2 longline 2>/dev/null | grep -E '^(serveraliveinterval|forwardagent) '"
say "2g) 机制层面能看到的证据：-vvv 给每个命中的块打一行 Applying options for <你敲的名字>"
raw "ssh -F $SB/config -vvv -G box-a 2>&1 | grep -E 'Reading configuration|Applying options|checking match' | head -5"
say "2h) 关键字拼错 = 整个文件 fatal 退出（不是「跳过那一行」），ssh 直接罢工"
mayfail "printf 'Host typo\n  Usermame learner\n' > $SB/cfg2g; ssh -G -F $SB/cfg2g typo 2>&1 | grep -v '^Pseudo'"
raw "ssh -G -F $SB/cfg2g typo >/dev/null 2>&1; echo \"真实退出码=\$?\""

# ============================================================================
scene "3：Host 模式匹配：* ? ! 、并排多模式、逗号、大小写、以及匹配的是【你敲的名字】"
# ============================================================================
say "3a) * 通配、? 单字符：Host web? 匹配 web1 不匹配 web12；Host web* 匹配所有以 web 开头的"
say "    手册原文（模式语法只有这三个符号，别编别的）："
raw "man ssh_config 2>/dev/null | col -b 2>/dev/null | grep -m1 -A2 'A pattern consists of'"
raw "printf 'Host web?\\n  User one-char\\nHost web*\\n  User star\\n' > $SB/cfg3a"
raw "for h in web1 web12 webx web; do printf '%-8s ' \"\$h\"; ssh -G -F $SB/cfg3a \"\$h\" 2>/dev/null | grep -m1 '^user '; done"
say "3b) ! 取反：Host * !secret-www 意思是「除了 secret-www 全都是」"
raw "printf 'Host * !secret-www\\n  User wildcard-except\\nHost secret-www\\n  User secret-user\\n' > $SB/cfg3b"
raw "for h in secret-www secret-db laptop; do printf '%-12s ' \"\$h\"; ssh -G -F $SB/cfg3b \"\$h\" 2>/dev/null | grep -m1 '^user '; done"
say "3b2) 全是 ! 的一行永远匹配不到（手册：取反项自己不会成立），必须补一个正项"
raw "printf 'Host !h1 !h2\\n  User neg-only\\n' > $SB/cfg3b2; for h in h1 h3; do printf '%-4s ' \"\$h\"; ssh -G -F $SB/cfg3b2 \"\$h\" 2>/dev/null | grep -m1 '^user '; done"
mayfail "printf '>>> 补一个 * 之后（Host 行里的多个模式是空格分隔的）：\\n'; printf 'Host !h1 !h2 *\\n  User neg-plus-star\\n' > $SB/cfg3b3; for h in h1 h3; do printf '%-4s ' \"\$h\"; ssh -G -F $SB/cfg3b3 \"\$h\" 2>/dev/null | grep -m1 '^user '; done"
say "3c) 一行并排多个模式用【空格】；逗号在 Host 行里【不是】分隔符 —— 它只是模式里的一个字符"
raw "printf 'Host a1 a2 a3\\n  User spaced\\n' > $SB/cfg3c"
raw "for h in a1 a2 a3 a4; do printf '%-4s ' \"\$h\"; ssh -G -F $SB/cfg3c \"\$h\" 2>/dev/null | grep -m1 '^user '; done"
say "证据①：Host c1,c2 是一条叫「c1,c2」的模式；而主机名里不允许有逗号 => 这一条永远配不上，且不报错"
mayfail "printf 'Host c1,c2\\n  User comma-is-literal\\n' > $SB/cfg3c2; for h in c1 c2; do printf '%-4s ' \"\$h\"; ssh -G -F $SB/cfg3c2 \"\$h\" 2>/dev/null | grep -m1 '^user '; done; printf '>>> 那直接把逗号敲进主机名呢：\\n'; ssh -G -F $SB/cfg3c2 'c1,c2' 2>&1 | grep -v '^Pseudo'"
say "证据②：Host b1, b2 被空格切成两个模式 —— 「b1,」和「b2」，所以 b2 命中、b1 不命中"
raw "printf 'Host b1, b2\\n  User two-patterns-one-broken\\n' > $SB/cfg3c3; for h in b1 b2; do printf '%-4s ' \"\$h\"; ssh -G -F $SB/cfg3c3 \"\$h\" 2>/dev/null | grep -m1 '^user '; done"
say "3d) *.example.com 匹配 a.example.com 和 a.b.example.com，但【不匹配】裸 example.com"
raw "printf 'Host *.example.com\\n  User subdomain-only\\n' > $SB/cfg3d"
raw "for h in example.com a.example.com a.b.example.com; do printf '%-18s ' \"\$h\"; ssh -G -F $SB/cfg3d \"\$h\" 2>/dev/null | grep -m1 '^user '; done"
say "3e) 语法宽容度：关键字大小写不敏感、值前可以用 =、缩进随你（Tab 也行）、值能加引号、# 是注释"
raw "printf '# 整行注释\\nHost case\\n  user LowerKey\\nHost eq\\n  User=eq-user\\nHost tab\\n\\t  User tab-user\\nHost quoted\\n  User \"带空格的用户\"\\n' > $SB/cfg3e"
raw "for h in case eq tab quoted; do printf '%-8s ' \"\$h\"; ssh -G -F $SB/cfg3e \"\$h\" 2>/dev/null | grep -m1 '^user '; done"
say "3f) 【最容易懵的一点】Host 匹配的是你在命令行敲的那个名字，不是 HostName 指向的真名"
raw "printf 'Host myalias\n  HostName real.server.name\n\nHost real.server.name\n  User only-if-you-type-the-real-name\n' > $SB/cfg3f"
raw "printf '>>> 敲别名 myalias:\n'; ssh -G -F $SB/cfg3f myalias 2>/dev/null | grep -E '^(host|hostname|user) '"
raw "printf '>>> 敲真名 real.server.name:\n'; ssh -G -F $SB/cfg3f real.server.name 2>/dev/null | grep -E '^(host|hostname|user) '"
say "推论（很实用）：known_hosts 记的是真名（HostName），所以改别名不影响主机密钥校验，"
say "改 DNS 指向才会触发 REMOTE HOST IDENTIFICATION HAS CHANGED。"

# ============================================================================
scene "4：Match —— all / host 逗号表 / user / localuser / exec / final / canonical / !xxx"
# ============================================================================
say "4a) Match host 用【逗号】分隔多个模式（和 Host 行的空格正好相反）"
say "    手册原文，可用的 Match 条件关键字都在这一行里："
raw "man ssh_config 2>/dev/null | col -b 2>/dev/null | grep -m1 -A6 'The available criteria keywords are'"
raw "printf 'Match host w1,w2\\n  User match-host-list\\nMatch host *.db.internal\\n  User db-user\\n' > $SB/cfg4a"
raw "for h in w1 w2 w3 prod.db.internal; do printf '%-18s ' \"\$h\"; ssh -G -F $SB/cfg4a \"\$h\" 2>/dev/null | grep -m1 '^user '; done"
mayfail "printf 'Match host w1 w2\\n  User space-is-wrong\\n' > $SB/cfg4a2; ssh -G -F $SB/cfg4a2 w1 2>&1 | grep -v '^Pseudo'"
say "4b) Match user 看的是【登录用的用户名】（会被 User 或 -l 改写）；Match localuser 才看你操作系统用户名"
raw "printf 'Match user alice\\n  ForwardAgent yes\\n' > $SB/cfg4b"
raw "printf '>>> -l alice: '; ssh -G -F $SB/cfg4b -l alice h 2>/dev/null | grep -m1 '^forwardagent '"
raw "printf '>>> -l bob  : '; ssh -G -F $SB/cfg4b -l bob h 2>/dev/null | grep -m1 '^forwardagent '"
raw "printf 'Match localuser %s\\n  User by-localuser\\n' \"\$(id -un)\" > $SB/cfg4b2; printf '>>> 不带 -l，本机用户 %s 命中: ' \"\$(id -un)\"; ssh -G -F $SB/cfg4b2 h 2>/dev/null | grep -m1 '^user '"
raw "printf 'Match localuser somebody-who-is-not-me\\n  User by-localuser\\n' > $SB/cfg4b3; printf '>>> localuser 写别人（不命中，退回默认）: '; ssh -G -F $SB/cfg4b3 h 2>/dev/null | grep -m1 '^user '"
raw "printf 'Match localuser %s\\n  User by-localuser\\n' \"\$(id -un)\" > $SB/cfg4b4; printf '>>> 命令行 -l bob 优先级最高，Match 抢不过它: '; ssh -G -F $SB/cfg4b4 -l bob h 2>/dev/null | grep -m1 '^user '"
say "4c) Match all 无条件成立；Match !host xxx 是「不是这台」；它们同样排队等 first-wins"
raw "printf 'Host m1\\n  User target-user\\nMatch host m1\\n  ForwardAgent yes\\nMatch !host m1\\n  User fallback-user\\n' > $SB/cfg4c"
raw "for h in m1 m2; do printf '>>> %s : ' \"\$h\"; ssh -G -F $SB/cfg4c \"\$h\" 2>/dev/null | grep -E '^(user|forwardagent) ' | tr '\\n' ' '; printf '\\n'; done"
say "4d) Match exec：ssh 会【真的在本地执行】这条命令，退出码 0 才算匹配 —— 本章最重要的安全提醒"
mayfail "printf 'Match exec \"exit 7\"\\n  User should-not-appear\\nMatch exec \"true\"\\n  User by-exec\\n' > $SB/cfg4d; printf 'exec \"exit 7\" 不匹配、exec \"true\" 匹配 -> '; ssh -G -F $SB/cfg4d anyhost 2>/dev/null | grep -m1 '^user '"
say "证据：只是 ssh -G（没连接、没登录）就把 touch 执行了，文件真的出现（本 demo 只 exec 一条写到沙箱的 touch）："
mayfail "printf 'Match exec \"touch $SB/EXEC_RAN_BY_MINUS_G\"\\n  User exec-wrote-a-file\\n' > $SB/cfg4d2; rm -f $SB/EXEC_RAN_BY_MINUS_G; ssh -G -F $SB/cfg4d2 anyhost 2>/dev/null | grep -m1 '^user '; ls -l $SB/EXEC_RAN_BY_MINUS_G"
say "exec 还能吃到 token（手册：Arguments to exec accept the tokens ...）—— 这里用 %n 把敲进去的 host 记一行："
mayfail "printf 'Match exec \"echo %%n=%%h:%%p >> $SB/EXEC_TOKENS_LOG\"\\n  User by-exec-token\\n' > $SB/cfg4d3; rm -f $SB/EXEC_TOKENS_LOG; for h in box-a box-b; do ssh -G -F $SB/cfg4d3 \"\$h\" >/dev/null 2>&1; done; cat -n $SB/EXEC_TOKENS_LOG"
say "现实里常见用法是 exec「问一个本地脚本：这台该不该走代理」。代价：每条 ssh 都会跑一次它，"
say "而且 exec 内容是本地代码执行入口 —— 别拼进不可信输入，别人给的 config 片段不要直接 Include。"
say "4e) 两遍解析 + Match final：-G 会打【两次】Reading configuration data，第二遍才是 final"
raw "printf 'Match final all\\n  User final-user\\n  HostName 10.9.9.9\\n  Ciphers +3des-cbc\\n' > $SB/cfg4e"
raw "printf 'final 段被求值时读文件次数 = ' ; ssh -F $SB/cfg4e -vvv -G anyhost 2>&1 | grep -c 'Reading configuration data'"
raw "ssh -G -F $SB/cfg4e anyhost 2>/dev/null | grep -E '^(user|hostname|ciphers) '"
say "解读①：Ciphers 用 + 追加，3des-cbc 落到列表【末尾】=> 默认仍然用不上它（要显式选才用）。"
say "解读②：final 段里 User 改成功了，HostName 却没改（还是 anyhost）—— 连接目标在第一遍就定死了，"
say "  final 只能影响「连上之后才用到」的东西。反过来说：把 HostName/ProxyCommand 写进 Match final 是无效配置。"
say "4f) CanonicalizeHostname + Match canonical：把短名补成 FQDN 后【再解析一遍配置】"
raw "printf 'CanonicalizeHostname yes\\nCanonicalizeFallbackLocal yes\\nCanonicalDomains localhost\\nMatch canonical\\n  User via-canonical\\nMatch !canonical\\n  User via-raw\\n' > $SB/cfg4f"
raw "getent ahosts web01.localhost | head -2"
raw "ssh -G -F $SB/cfg4f web01 2>/dev/null | grep -E '^(hostname|canonicalizehostname|canonicaldomains|user) '"
say "本机 .localhost 由 systemd-resolved 直接本地应答（::1），没有出网；换成你自己的域名才是真用法。"
say "注意 user 落的是 via-raw：第一遍（还不 canonical）先把 User 设上了，第二遍再匹配也轮不到 —— first-wins 的活教材。"
say "4g) CanonicalizeMaxDots：主机名点数超过它就跳过规范化（默认 1），这是「短名补域名、FQDN 别动」的开关"
raw "printf 'CanonicalizeHostname yes\\nCanonicalizeMaxDots 1\\nCanonicalDomains localhost\\n' > $SB/cfg4g"
raw "ssh -F $SB/cfg4g -vvv -G web01.dc.corp 2>&1 | grep -E 'resolve_canonicalize|Canonicalized' | head -3"
raw "ssh -G -F $SB/cfg4g web01.dc.corp 2>/dev/null | grep -E '^(hostname|canonicalizemaxdots|canonicalizehostname) '"
say "4h) Match addr 在 10.2 的客户端配置里已经不存在（不在可用条件列表里），-G 直接 fatal"
mayfail "printf 'Match addr 127.0.0.1\\n  User by-addr\\n' > $SB/cfg4h; ssh -G -F $SB/cfg4h 127.0.0.1 2>&1 | grep -v '^Pseudo'"
say "4i) Match 段不会「吃掉」后面的 Host 块：下一个 Host 行重新起算（很多人以为会一直延续）"
raw "printf 'Match host w1\\n  User in-match\\n\\nHost w2\\n  User w2-user\\n' > $SB/cfg4i"
raw "for h in w1 w2 w3; do printf '>>> %-4s ' \"\$h\"; ssh -G -F $SB/cfg4i \"\$h\" 2>/dev/null | grep -m1 '^user '; done"

# ============================================================================
scene "5：常用关键字逐个用 -G 看最终值（一份沙箱配置，全部指向 127.0.0.1 高端口）"
# ============================================================================
cat > "$SB/config" <<EOF
# 沙箱主配置：本章所有 -G / 连接演示都走这份文件
Host box-a
  HostName 127.0.0.1
  Port $DEADPORT
  User box-user
  IdentityFile $SB/keys/id_box
  IdentitiesOnly yes
  UserKnownHostsFile $SB/kh
  StrictHostKeyChecking accept-new

Host jumper
  HostName 127.0.0.1
  Port $DEADPORT
  User jump-user
  IdentityFile $SB/keys/id_jump
  IdentitiesOnly yes
  UserKnownHostsFile $SB/kh
  ServerAliveInterval 30
  ServerAliveCountMax 4
  ConnectTimeout 10

Host behind-jump
  HostName 127.0.0.1
  Port $DEADPORT
  User inner-user
  IdentityFile $SB/keys/id_demo
  IdentitiesOnly yes
  UserKnownHostsFile $SB/kh
  ProxyJump jumper

Host ssh-cat
  HostName 127.0.0.1
  Port $DEADPORT
  User cat-user
  IdentityFile $SB/keys/id_demo
  IdentitiesOnly yes
  UserKnownHostsFile $SB/kh
  ProxyCommand ssh -F $SB/config -W %h:%p jumper

Host mux
  HostName 127.0.0.1
  Port $DEADPORT
  User mux-user
  IdentityFile $SB/keys/id_demo
  IdentitiesOnly yes
  UserKnownHostsFile $SB/kh
  ControlMaster auto
  ControlPath $SB/cm/%r@%h:%p
  ControlPersist 10m
  ForwardAgent yes
EOF
say "文件本体（每个块都自带沙箱护栏，这样任何一条命令都不会去碰真实 ~/.ssh）："
raw "cat -n $SB/config"
say "① HostName / Port / User：别名的全部秘密就这三行"
raw "ssh -G -F $SB/config box-a 2>/dev/null | grep -E '^(host|hostname|port|user|connecttimeout) '"
say "② IdentityFile + IdentitiesOnly：只递这一把，别把 agent 里所有钥匙端上去（01 章 MaxAuthTries 的解药）"
raw "ssh -G -F $SB/config box-a 2>/dev/null | grep -E '^(identityfile|identitiesonly) '"
say "没写 IdentitiesOnly 会怎样：它默认 no，agent 里的钥匙照样会被递出去"
raw "printf 'Host noio\n  IdentityFile $SB/keys/id_demo\n' > $SB/cfg5io; ssh -G -F $SB/cfg5io noio 2>/dev/null | grep -E '^(identityfile|identitiesonly) '"
say "③ ProxyJump：-G 里只是干干净净一行 proxyjump；真跑时 ssh 把它展开成一条 ProxyCommand（见场景 6e）"
raw "ssh -G -F $SB/config behind-jump 2>/dev/null | grep -E '^(host|hostname|port|user|proxyjump|proxycommand|proxyusefdpass) '"
say "跳板机自己的参数要单独查 —— ProxyJump 用的是【另一个块】，漏查它是最常见的排错死角"
raw "ssh -G -F $SB/config jumper 2>/dev/null | grep -E '^(host|hostname|port|user|identityfile|serveralive[a-z]+|connecttimeout) '"
say "④ ProxyCommand：注意里面的 %h %p 在 -G 里【原样不展开】（和下面的 ControlPath 相反）"
raw "ssh -G -F $SB/config ssh-cat 2>/dev/null | grep -E '^(proxycommand|proxyusefdpass) '"
say "⑤ ServerAliveInterval / ServerAliveCountMax / TCPKeepAlive：探测节奏三件套"
raw "ssh -G -F $SB/config jumper 2>/dev/null | grep -E '^(serveraliveinterval|serveralivecountmax|tcpkeepalive) '"
say "算一下：空闲 30 秒才发第一次探测；每 30 秒一次，连续 4 次没回音（≈120 秒）ssh 自己断开；interval 写 0 = 关掉。"
say "⑥ ControlMaster / ControlPath / ControlPersist：这里 %r %h %p 被 -G 展开成真值，10m 归一化成 600 秒"
raw "ssh -G -F $SB/config mux 2>/dev/null | grep -E '^(controlmaster|controlpath|controlpersist|forwardagent) '"
say "-O（复用控制命令）靠 ControlPath 找套接字；没配 ControlPath 它直接告诉你没得查："
mayfail "timeout 10 ssh -O check -F $SB/config box-a 2>&1 | head -2"
mayfail "timeout 10 ssh -O check -F $SB/config mux 2>&1 | head -2"
say "⑦ StrictHostKeyChecking / UserKnownHostsFile：把 known_hosts 钉在沙箱，绝不污染你真实那份"
raw "ssh -G -F $SB/config box-a 2>/dev/null | grep -E '^(stricthostkeychecking|userknownhostsfile|hashknownhosts|updatehostkeys) '"
say "⑧ 想知道自己敲的名字最终用了哪条：把上面几行合成一条命令，日常就 grep 这五个"
raw "ssh -G -F $SB/config behind-jump 2>/dev/null | grep -E '^(host|hostname|user|port|identityfile|proxyjump) '"
say "⑨ -G 打出来的是【全小写关键字】，和配置里的驼峰一一对应：IdentityFile->identityfile，"
say "ServerAliveCountMax->serveralivecountmax。所以配完就用 grep -E '^(要查的关键字小写) ' 验收。"
raw "printf '%s\n' 'IdentityFile          ->  identityfile' 'StrictHostKeyChecking ->  stricthostkeychecking' 'ServerAliveCountMax   ->  serveralivecountmax' 'ControlPersist        ->  controlpersist' 'ProxyJump             ->  proxyjump'"

# ============================================================================
scene "6：-G 与真实报错对上：配置到底有没有生效（三个【预期失败】，全在 127.0.0.1 高端口）"
# ============================================================================
say "【先说清楚】下面每一条都会失败，而且失败本身就是证据：报错里的地址/端口/命令行 = 配置里的值。"
say "这些端口是脚本刚向内核要的空闲端口，不是任何真实服务；跟 22 端口和外部网络毫无关系。"
say "6a) 端口空着 => Connection refused。看 Connecting to 那行的端口是不是配置里那个（不是 22）"
raw "timeout 10 ssh -F $SB/config -v $CS box-a true 2>&1 | grep -E 'Applying options|Connecting to|connect to host' | head -4"
say "6b) 假服务端只发一行 SSH-2.0 横幅 => 版本号被打印 + compat_banner 匹配不上 + 连接被关"
start_fake_banner_server
raw "printf 'Host fake-banner\n  HostName 127.0.0.1\n  Port $FAKEPORT\n  User fb-user\n  IdentityFile $SB/keys/id_demo\n  IdentitiesOnly yes\n  UserKnownHostsFile $SB/kh\n' > $SB/cfg6b"
mayfail "timeout 12 ssh -F $SB/cfg6b -v $CS fake-banner true 2>&1 | grep -E 'Applying options|Connecting to|Local version|Remote protocol|compat_banner|KEXINIT sent|Connection closed' | head -7"
say "nc 那侧抓到的客户端横幅（对面确实是本机 ssh，端口也就是配置里那个）："
raw "sed -n '1p' $SB/nc_fake_capture.txt | tr -d '\\r'; printf '整个抓包文件 %s 字节：第一行之后是二进制 KEXINIT 包，不该往终端打\n' \"\$(wc -c < $SB/nc_fake_capture.txt)\""
say "6c) 服务端接了连接但一个字不发 => banner exchange 超时；ConnectTimeout 管的就是这一段"
start_silent_server
raw "printf 'Host hang-server\n  HostName 127.0.0.1\n  Port $SLOWPORT\n  User hs-user\n  ConnectTimeout 3\n  IdentityFile $SB/keys/id_demo\n  IdentitiesOnly yes\n  UserKnownHostsFile $SB/kh\n' > $SB/cfg6c"
mayfail "time timeout 12 ssh -F $SB/cfg6c -v $CS hang-server true 2>&1 | grep -E 'Connecting to|Local version|timed out' | head -4"
say "6d) ProxyCommand 的展开证据：故意写一个不存在的程序，日志那行已经把 %h %p 换成真值"
raw "printf 'Host bad-proxy\n  HostName 127.0.0.1\n  Port $DEADPORT\n  User bp-user\n  IdentityFile $SB/keys/id_demo\n  IdentitiesOnly yes\n  UserKnownHostsFile $SB/kh\n  ProxyCommand /nonexistent-binary --%%h %%p\n' > $SB/cfg6d"
mayfail "timeout 10 ssh -F $SB/cfg6d -v $CS bad-proxy true 2>&1 | grep -E 'Executing proxy|No such|Connection|proxy command' | head -4"
say "6e) ProxyJump 的真实展开：先打印它替你造的 ProxyCommand，子进程【继承 -F】所以沙箱隔离没破"
mayfail "timeout 12 ssh -F $SB/config -v $CS behind-jump true 2>&1 | grep -E 'Applying options|implicit ProxyCommand|Executing proxy|Connecting to|connect to host' | head -6"
say "看点：跳板那一段也是 127.0.0.1 高端口 => 预期 refused。真服务器上这里会是两次 Connection established。"
say "6f) %token 手册原文（TOKENS 整段照抄，别背我瞎编的版本）："
raw "man ssh_config 2>/dev/null | col -b 2>/dev/null | awk '/^TOKENS/{f=1} f{print} /^Note that some of these/{if(f)exit}' | head -46"
say "实测一份把 token 全用上的配置（注意 -G 的两副面孔）："
raw "printf 'Host tk\n  HostName 127.0.0.1\n  Port $DEADPORT\n  IdentityFile $SB/keys/id_%%r\n  UserKnownHostsFile $SB/kh/%%n@%%h:%%p_%%r_%%l_%%i_%%C_%%u_%%d\n  ControlPath $SB/cm/%%r@%%h:%%p\n  ProxyCommand nc %%h %%p\n' > $SB/cfg6f"
raw "ssh -F $SB/cfg6f -vvv -G tk 2>&1 | grep -E 'expanded'"
raw "ssh -G -F $SB/cfg6f tk 2>/dev/null | grep -E '^(identityfile|userknownhostsfile|controlpath|proxycommand) '"
say "读法：UserKnownHostsFile 有【一行 debug3: expanded】（-G 里只有它打这条日志），ControlPath 直接给展开后的路径；"
say "IdentityFile / ProxyCommand 里的 %xx 原样留着 —— 要到真正用的那一刻才展开。别在 -G 里判断这两个对不对。"
say "ProxyJump 的内容本身就是个 token（%j），运行时才知道的值也能拼进路径："
raw "printf 'Host tj\n  HostName 127.0.0.1\n  ProxyJump jumper\n  UserKnownHostsFile $SB/kh/j=[%%j]\n' > $SB/cfg6f2; ssh -G -F $SB/cfg6f2 tj 2>/dev/null | grep -E '^(userknownhostsfile|proxyjump) '"
say "顺手留一份「哪一行在生效」的行号证据（日常排错就看这个）："
raw "ssh -F $SB/config -vvv -G mux 2>&1 | grep -E 'Applying options|checking match' | head -5"

# ============================================================================
scene "7：Include 与配置分片（config.d 思路，同样全在沙箱里）"
# ============================================================================
say "7a) 结构：入口文件只留一行 Include，分片按【词法顺序】加载，用编号控制先后"
say "    手册原文（Include 的四条规则一次看全：通配按词法序 / 相对路径基准 / 可以写在 Match 里）："
raw "man ssh_config 2>/dev/null | col -b 2>/dev/null | grep -m1 -A5 'Wildcards will be expanded'"
raw "printf 'Host box-a\n  User shard-10-user\n  HostName 10.0.0.10\n  IdentityFile $SB/keys/id_shard10\n' > $SB/config.d/10-aliases.conf"
raw "printf 'Host *\n  User shard-90-default\n  IdentityFile $SB/keys/id_default\n' > $SB/config.d/90-catchall.conf"
raw "printf 'Include %s/config.d/*\n' $SB > $SB/cfg7; cat -n $SB/cfg7; ls -l $SB/config.d/"
raw "ssh -F $SB/cfg7 -vvv -G box-a 2>&1 | grep 'Reading configuration data'"
raw "ssh -G -F $SB/cfg7 box-a 2>/dev/null | grep -E '^(user|hostname|identityfile) '"
say "解读①：10 在 90 前面 => user/hostname 由 10 号分片先抢下（first-wins）。"
say "解读②：identityfile 有【两行】，顺序就是加载顺序（10 号在前）—— 叠加关键字不受 first-wins 管，"
say "  这就是「分片里各放各的钥匙」能成立的原因，也是「递钥匙顺序」能被文件名控制的原理。"
say "词法顺序就是优先级 —— 这就是 config.d 用 00/10/90 前缀的原因（和 /etc/systemd 一个套路）。"
say "7b) 推荐的真实写法：入口一行 Include + 分片，业务改动落在分片里，git diff 干净、多人不打架"
raw "printf '%s\n' 'Include ~/.ssh/config.d/*' '' '# 想临时全线关掉连接复用，就在 config.d/99-emergency.conf 里写：' '# Host *' '#   ControlMaster no' > $SB/config.suggested; cat -n $SB/config.suggested"
say "7c) 两个静默坑（都不报错）：通配没匹配到任何文件 / Include 写得比目标块晚"
mayfail "printf 'Include %s/no-such-dir/*\n' $SB > $SB/cfg7c2; printf '>>> 目录不存在时：'; ssh -G -F $SB/cfg7c2 box-a 2>/dev/null | grep -m1 '^user '"
say "user 退回默认值，一声不吭 => 「我明明 Include 了怎么没生效」的第一答案就是路径拼错/文件不在。"
raw "printf 'Include %s/config.d/*.conf\n' $SB > $SB/cfg7c3; printf '>>> 用绝对路径 + 通配（本 demo 的正解写法）：'; ssh -G -F $SB/cfg7c3 box-a 2>/dev/null | grep -E '^(user|hostname|identityfile) '"
say "第三个坑只抄手册不实测：相对路径 Include 是按【~/.ssh】解析的（不是配置文件所在目录），"
say "见上面手册那句 Files without absolute paths are assumed to be in ~/.ssh。"
say "本 demo 红线是不读写你真实的 ~/.ssh，所以【故意不跑】这条 —— 结论标注为【未验证】。"
say "7d) Include 的位置就是优先级：放前面=分片先抢，放后面=入口块先抢（规则完全不变）"
mayfail "printf 'Include %s/config.d/90-catchall.conf\nHost box-a\n  User entry-wins\n' $SB > $SB/cfg7d; cat -n $SB/cfg7d; printf '>>> Include 行在 Host 块【之前】（分片先被读到）：'; ssh -G -F $SB/cfg7d box-a 2>/dev/null | grep -E '^(user|hostname) '"
raw "printf 'Host box-a\n  User entry-wins\n\nInclude %s/config.d/90-catchall.conf\n' $SB > $SB/cfg7d2; cat -n $SB/cfg7d2; printf '>>> 把入口 Host 块挪到 Include【之前】：'; ssh -G -F $SB/cfg7d2 box-a 2>/dev/null | grep -E '^(user|hostname) '"
say "7e) 一条经常被忽略的：Include 进来的文件权限不合格 → 整个配置罢工（场景 8 现场演）"

# ============================================================================
scene "8：权限要求 —— 配置文件权限不对会怎样（沙箱里造，绝不碰真实 ~/.ssh）"
# ============================================================================
say "8a) 被 Include 进来的文件：world-writable（其他人可写）直接被拒，整条配置罢工"
raw "printf 'Include %s/inc_perm.conf\n' $SB > $SB/cfg8; printf 'Host permprobe\n  User perm-user\n' > $SB/inc_perm.conf; chmod 644 $SB/cfg8 $SB/inc_perm.conf"
raw "printf '>>> 644 正常读：'; ssh -G -F $SB/cfg8 permprobe 2>/dev/null | grep -m1 '^user '"
mayfail "chmod 666 $SB/inc_perm.conf; printf '>>> 666 之后：'; ssh -G -F $SB/cfg8 permprobe 2>&1 | grep -v '^Pseudo' | head -3"
say "8b) 门限实测：组可写(664/620)放行，【其他人可写】(666/646)才报错 => 规则是「不能被其他人写」"
raw "for m in 600 640 604 644 664 620 666 646 444; do chmod \$m $SB/inc_perm.conf; printf '分片 %-4s => ' \"\$m\"; ssh -G -F $SB/cfg8 permprobe 2>&1 | grep -m1 -E 'perm-user|Bad owner'; done; chmod 644 $SB/inc_perm.conf"
say "8c) 反直觉的一条：-F 指定的【主文件本身】不查权限 —— 别把安全感放在这上面"
mayfail "chmod 666 $SB/cfg8; printf '>>> 主文件 666：'; ssh -G -F $SB/cfg8 permprobe 2>&1 | grep -m1 -E 'perm-user|Bad owner'; chmod 600 $SB/cfg8"
say "8d) -F 指到一个【目录】：不报错，静默当成「什么都没配」—— 比报错更难查"
mayfail "mkdir -p $SB/adir; ssh -G -F $SB/adir box-a >/dev/null 2>&1; printf '>>> 退出码=%s（居然成功）\n' \"\$?\"; ssh -G -F $SB/adir box-a 2>&1 | grep -E '^(hostname|user) ' | head -2"
say "8e) -F 指到【不存在的文件】：这条会明确报错，退出码 255。脚本里判断配置文件在不在就用它。"
mayfail "ssh -F $SB/nope.cfg -G box-a 2>&1 | grep -v '^Pseudo'"
raw "ssh -F $SB/nope.cfg -G box-a >/dev/null 2>&1; echo \"真实退出码=\$?\""
say "8f) 你真实的 ~/.ssh/config 权限不对时的报错（Bad owner or permissions on ~/.ssh/config）本 demo【不验证】，原因有二："
say "   ① 本章红线：不读写 ~/.ssh；② OpenSSH 取的是 getpwuid 的目录，改 HOME 也不会改配置路径，"
say "   所以没法用 HOME=<沙箱> 造一个假 ~/.ssh/config 来演同一件事。②本身可以只打印路径来验："
raw "HOME=$SB/fakehome ssh -G -F /dev/null demo.example 2>/dev/null | grep -m1 '^userknownhostsfile '"
say "   换了 HOME 也还是你家目录那条 —— 说明这些路径来自口令表，不来自环境变量（这条只打印，不打开文件）。"
say "   结论照手册抄：目录 700、文件 600、其他人绝对不可写；自查命令（在你自己机器上跑，别在脚本里跑）："
raw "echo 'ls -ld ~/.ssh ~/.ssh/config && ssh -G somehost | grep ^hostname'"

# ============================================================================
scene "9：三个真实场景的完整配置块（每个都用 ssh -G 验证生效）"
# ============================================================================
say "【9-1】给常用机器起别名 + 固定用户和密钥：从此只敲别名，不用背 IP 和用户名"
cat > "$SB/config" <<EOF
# ---- 具体别名：写在前面，first-wins 才轮得到它们 ----
Host gh-push
  HostName 127.0.0.1
  Port $DEADPORT
  User git
  IdentityFile $SB/keys/id_demo
  IdentitiesOnly yes

Host work
  HostName 127.0.0.1
  Port $DEADPORT
  User deploy
  IdentityFile $SB/keys/id_box

# ---- 兜底：只放「对全世界都有益、不会和别名抢」的项 ----
Host *
  ServerAliveInterval 30
  ServerAliveCountMax 4
  StrictHostKeyChecking accept-new
  UserKnownHostsFile $SB/kh
  AddKeysToAgent yes
EOF
raw "cat -n $SB/config"
raw "for h in gh-push work some-random-host; do printf '===== %s\n' \"\$h\"; ssh -G -F $SB/config \"\$h\" 2>/dev/null | grep -E '^(host|user|hostname|port|identityfile|identitiesonly|serveralive[a-z]+|stricthostkeychecking|addkeystoagent|userknownhostsfile) '; done"
say "逐行解读：gh-push 的 user 是 git（这类服务用户名固定就是 git），IdentitiesOnly 挡住 agent 里其余钥匙；"
say "work 没写 IdentitiesOnly => 用默认 no；some-random-host 只拿到 Host * 那五条兜底。"
say "为什么 ServerAlive/StrictHostKeyChecking 可以放 Host *：它们不与具体别名重名，抢不到东西；"
say "为什么 User/IdentityFile 不要放 Host *：first-wins 会把具体别名的同名设置全部挡掉（场景 2d 已现场演示）。"

say "【9-2】内网机通过跳板机（ProxyJump）：一次配置，scp / rsync / git clone 全都跟着走"
cat > "$SB/config" <<EOF
# 沙箱护栏（真实使用时不需要这几行：默认值本来就指向 ~/.ssh）
IdentityFile $SB/keys/id_demo
IdentitiesOnly yes
UserKnownHostsFile $SB/kh

# 跳板机本身：必须写在通配之前，否则会被下面的 ProxyJump 套娃
Host bastion
  HostName 127.0.0.1
  Port $DEADPORT
  User jumpuser
  IdentityFile $SB/keys/id_jump
  ServerAliveInterval 30

# 所有内网目标机：不用开 22 端口，全靠跳板转发
Host 10.* *.corp.internal idrac-*
  ProxyJump bastion

# 多级跳板：逗号，从左到右依次是「先跳这里」
Host deep-box
  HostName 127.0.0.1
  Port $DEADPORT
  User inner
  ProxyJump bastion,10.1.1.1

# 反面教材（故意写错，看下一行解释）：ProxyCommand 里的 ssh 不会自动带 -F
Host legacy
  HostName 127.0.0.1
  Port $DEADPORT
  ProxyCommand ssh -W %h:%p bastion
EOF
raw "cat -n $SB/config"
raw "for h in 10.1.2.3 web.corp.internal idrac-01 deep-box bastion legacy; do printf '===== %s\n' \"\$h\"; ssh -G -F $SB/config \"\$h\" 2>/dev/null | grep -E '^(host|user|hostname|port|proxyjump|proxycommand|identityfile) '; done"
say "看点①：通配 10.* 一把罩住全部内网机；bastion 自己写在前面 => 它匹配到的第一个 ProxyJump 需求没有设置，"
say "  所以不会被套娃（proxyjump 那一行压根不出现，这就是「先设的值挡住后面」的正面用法）。"
say "看点②：deep-box 的 proxyjump 是两级逗号串，从左往右依次跳。"
say "看点③：顶格那三行护栏先设了 IdentityFile，所以各块自己的 id_jump/id_demo 是【追加】在后面 —— 叠加关键字的顺序效应。"
mayfail "timeout 12 ssh -F $SB/config -v $CS 10.1.2.3 true 2>&1 | grep -E 'Applying options|implicit ProxyCommand|Executing proxy|Connecting to|connect to host' | head -6"
say "真实跑一次（预期失败：bastion 端口空着 => refused）。日志里两条 Applying options 说明「目标块 + 跳板块」都读到了。"
raw "printf '%s\n' 'ProxyCommand 手写版的正确姿势（自己把 -F 和钥匙带上）：' '  ProxyCommand ssh -F ~/.ssh/config -W %h:%p bastion' '  ProxyCommand ssh -o IdentityFile=~/.ssh/id_bastion -W %h:%p bastion' '能用 ProxyJump 就别手写：ssh 会替你把 -F、-v 这些继承给子进程（场景 6e 已实测）'"

say "【9-3】复用连接（ControlMaster / ControlPath / ControlPersist）：反复登录只握手一次"
cat > "$SB/config" <<EOF
# 沙箱护栏
IdentityFile $SB/keys/id_demo
IdentitiesOnly yes
UserKnownHostsFile $SB/kh

Host *
  ControlMaster auto
  ControlPath  $SB/cm/%C
  ControlPersist 10m
EOF
raw "cat -n $SB/config"
raw "ssh -G -F $SB/config anyhost 2>/dev/null | grep -E '^(controlmaster|controlpath|controlpersist) '"
say "ControlPath 用 %C（手册：Hash of %l%h%p%r%j，定长 40 字符）而不是字面 %r@%h:%p：socket 路径有长度上限，"
say "主机名一长就会撞上（报错带 Path too long 字样，本 demo【未复现】）；%C 定长最稳。两种写法实测对比："
raw "printf 'Host by-token\n  HostName 127.0.0.1\n  Port $DEADPORT\n  User u1\n  ControlPath $SB/cm/%%r@%%h:%%p\n' > $SB/cfg93b"
raw "printf 'Host by-hash\n  HostName 127.0.0.1\n  Port $DEADPORT\n  User u1\n  ControlPath $SB/cm/%%C\n' > $SB/cfg93c"
raw "printf 'token 版 -> '; ssh -G -F $SB/cfg93b by-token 2>/dev/null | grep -m1 '^controlpath '"
raw "printf '哈希版 -> '; ssh -G -F $SB/cfg93c by-hash 2>/dev/null | grep -m1 '^controlpath '"
say "token 版一眼看出是哪台机（好排查），哈希版名字短（不容易超限）。日常推荐 %C。"
say "ControlPersist 10m 归一化成 600 秒：主连接退出后套接字再活 10 分钟，这期间新命令直接复用（省掉握手和认证）。"
mayfail "timeout 10 ssh -O check -F $SB/config by-token 2>&1 | head -2"
say "这句「Control socket connect(...): No such file or directory」就是【还没有主连接】的标准样子。"
say "⚠ 未验证：复用【真正生效】需要能登录成功的服务器（主连接认证通过才会创建套接字）。"
say "本 demo 不制造登录成功，所以 cm/ 目录一直是空的 —— 现场核验一下："
raw "printf 'cm/ 目录里的文件数 = %s（应为 0）\n' \"\$(ls -A $SB/cm | wc -l)\""
say "你自己有服务器时的验收动作：登录一次 -> ls ~/.ssh/cm/（或 %C 目录）看到套接字；"
say "再开第二条命令时 ssh -v 里会出现一行 ControlSocket ... already exists, disabling multiplexing（这才是复用成功的意思）；"
say "关掉用 ssh -O exit 别名；这些行需要你机器上验证，本 demo 不假装看到过。"

# ============================================================================
scene "10：真实报错墙（一条命令一个症状，背下来能省半小时）"
# ============================================================================
mayfail "printf 'Host e1\n  Usermame x\n' > $SB/err1; ssh -G -F $SB/err1 e1 2>&1 | grep -v '^Pseudo'"
mayfail "printf 'Host e2\n  Port notanumber\n' > $SB/err2; ssh -G -F $SB/err2 e2 2>&1 | grep -v '^Pseudo'"
mayfail "printf 'Host e3\n  StrictHostKeyChecking maybe\n' > $SB/err3; ssh -G -F $SB/err3 e3 2>&1 | grep -v '^Pseudo'"
mayfail "printf 'Host e4\n  User y\nMatch\n  User z\n' > $SB/err4; ssh -G -F $SB/err4 e4 2>&1 | grep -v '^Pseudo'"
mayfail "printf 'Include\n' > $SB/err5; ssh -G -F $SB/err5 e5 2>&1 | grep -v '^Pseudo'"
mayfail "printf 'Host\n  User z\n' > $SB/err6; ssh -G -F $SB/err6 e6 2>&1 | grep -v '^Pseudo'"
say "共同点：第一行都带【文件名 + 行号】，直接定位；只要出现 terminating，就是整个文件作废（不是跳过一行）"
raw "printf '%s\n' 'Bad configuration option: xxx          -> 关键字拼错（消息里是全小写）' \"Bad port 'xxx'.                     -> Port 写了非数字\" 'unsupported option \"xxx\"           -> 关键字对但值不认（StrictHostKeyChecking / ProxyUseFdpass 常见）' 'no argument after keyword \"host\"   -> Host / Match / Include 后面漏了参数' \"Can't open user config file ...    -> -F 指向的文件不存在（退出码 255）\" 'Bad owner or permissions on ...    -> 被 Include 的文件其他人可写' 'Unsupported Match attribute addr   -> Match addr 在连上之前不可用（-G 会 fatal）' 'Bad Match condition              -> Match 里把关键字和模式写错了（例如空格分隔多模式）'"

# ============================================================================
scene "11：把这一章变成肌肉记忆的三条命令"
# ============================================================================
raw "printf '%s\n' '1) 改完配置先验收（不联网、不读写私钥）：' '   ssh -F ~/.ssh/config -G 别名 | grep -E \"^(host|user|hostname|port|identityfile|proxyjump|proxycommand|controlpath) \"' '' '2) 怀疑某项被前面的块抢了：把整份解析结果连行号打出来' '   ssh -F ~/.ssh/config -vvv -G 别名 2>&1 | grep -E \"Reading configuration|Applying options|checking match\"' '' '3) 配置加载了哪几个文件、有没有被权限拦：' '   ssh -F ~/.ssh/config -vvv -G 别名 2>&1 | grep -E \"Reading configuration|Bad owner\"'"
raw "printf '>>> 三条各真跑一遍（拿本沙箱配置当例子）：\n'"
raw "ssh -G -F $SB/config 10.1.2.3 2>/dev/null | grep -E '^(host|user|hostname|port|identityfile|proxyjump|proxycommand|controlpath) '"
raw "ssh -F $SB/config -vvv -G 10.1.2.3 2>&1 | grep -E 'Reading configuration|Applying options|checking match' | head -6"
raw "ssh -F $SB/config -vvv -G 10.1.2.3 2>&1 | grep -cE 'Reading configuration|Bad owner'"

printf '\n\n================ 收尾自证 ================\n'
nc_now=$(pgrep -xc nc 2>/dev/null || true)
printf '1) 后台进程：本脚本只起过 nc 假服务端（都套了 timeout 15 兜底），cleanup 稍后会 kill。\n'
printf '   此刻系统上的 nc 进程数 = %s；本脚本记录的 PID = %s\n' "${nc_now:-0}" "${PIDS[*]:-无}"
printf '2) 有没有残留 ssh 子进程（ProxyJump / ProxyCommand 生的那条）：\n'
pgrep -af 'ssh -F' 2>/dev/null | grep -v pgrep || printf '   （无）\n'
printf '3) 沙箱里生了哪些文件（私钥只被【引用路径】，内容一次都没打印过）：\n'
printf '   文件总数 = %s；私钥路径 = %s\n' "$(find "$SB" -type f | wc -l)" "$SB/keys/id_demo"
find "$SB" -type f -printf '   %M %P\n' 2>/dev/null | sort -k2
printf '4) 本章标注【未验证】的三条（都需要一台你能登录的服务器，本章红线不制造登录成功）：\n'
printf '   - 复用真正生效时的 ControlSocket ... already exists, disabling multiplexing 日志\n'
printf '   - 你真实 ~/.ssh/config 权限不对时的 Bad owner or permissions on /home/<you>/.ssh/config\n'
printf '   - 相对路径 Include 落到 ~/.ssh/... 的实际效果（只抄了手册，没在你的目录上跑）\n'
printf '5) 红线复核：不读写 ~/.ssh、不写 /etc/ssh、不连 22、不连外部主机、不制造登录成功、不打印私钥。\n'
printf '\n全部演示结束（端口与进程的最终自证由 cleanup 在退出时打印）。\n'
