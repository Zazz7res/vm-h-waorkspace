#!/usr/bin/env bash
# ============================================================================
# 01 SSH 到底在干什么 —— 一键复现整章场景
#
# 用法：
#   bash demo.sh            # 跑完自动删掉沙箱
#   bash demo.sh --keep     # 跑完保留沙箱并打印路径，你可以进去自己敲命令
#
# 安全声明（先读这段，照着抄到你自己的实验里）：
#   * 全程只在 mktemp 出来的沙箱目录里造 key / known_hosts。
#     绝不读写 ~/.ssh/，绝不改 /etc/ssh/。
#   * 每条 ssh 都带 -F /dev/null：不读真实的 ~/.ssh/config，也不读
#     /etc/ssh/ssh_config 和 /etc/ssh/ssh_config.d/（那边有 systemd 塞进来的规则）。
#   * 每条 ssh 都带 -o IdentityAgent=none：不把你真实 agent 里的钥匙递出去。
#     （这条最容易被忘 —— 不加的话 -vvv 日志里会出现 "Offering public key: <你的真 key>"，
#      本 demo 第一次写的时候就犯了这个错，README 的坑清单里有记录。）
#   * 只连 127.0.0.1，不连任何外部主机。
#   * **不追求登录成功**：沙箱公钥不在任何 authorized_keys 里，所以每一脚都会自然停在
#     认证失败，这正是要演示的东西。
#   * **远端用户名一律写 `learner@127.0.0.1`**（本机不存在这个用户）。不写 user@ 时 ssh 会拿
#     你本地的登录名去登，那会把真实姓名印进日志/报错；换成一个不存在的用户，结论一个字都不变
#     （照样停在 Permission denied），而且书里不出现任何真实身份。
#   * 不起后台进程、不监听端口，收尾会自证。
#
# 下面每条命令都把选项写全（不用变量缩写），这样打印出来的东西你可以直接复制去敲。
# ============================================================================
set -euo pipefail

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done

SB=$(mktemp -d "${TMPDIR:-/tmp}/ssh01-XXXXXX")
chmod 700 "$SB"
cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去自己敲： cd "%s" && nl -ba full_vvv.log\n  看完自己删： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
}
trap cleanup EXIT

scene() { printf '\n\n================ 场景 %s ================\n' "$*"; }
say()   { printf '\n--- %s\n' "$*"; }
# raw：原样打印命令再执行。grep「没匹配到」返回 1 是正常情况，不让它把脚本带崩。
raw()     { printf '\n$ %s\n' "$*"; bash -c "$*" || printf '  [退出码 %s：多半是 grep 没匹配到，不是脚本报错]\n' "$?"; }
# mayfail：故意演示一条会失败的命令（报错原文本身就是教学内容）
mayfail() { printf '\n$ %s\n' "$*"; bash -c "$*" || printf '  （退出码 %s —— 这条就是要看它失败）\n' "$?"; }

# 本机环境事实：127.0.0.1:22 上有一个真实的 OpenSSH 服务端（ssh.socket + /usr/sbin/sshd）。
if ! grep -qE ':22[[:space:]]' <(ss -lnt 2>/dev/null); then
    echo "【提醒】本机 127.0.0.1:22 上没有监听的 sshd：本 demo 的握手部分跑不出真结果，"
    echo "        只有算法查询 / 签名验签 / known_hosts 操作这几节能看。"
fi

# wait_sshd：sshd 被并发握手淹了会临时回一行 "Not allowed at this time" 然后 RST。
# 这不是配置错，是 MaxStartups 限流。等一秒再试就行。（这条也是真实观察到的现象）
wait_sshd() {
    local i banner=""
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        banner=$(timeout 5 bash -c 'exec 3<>/dev/tcp/127.0.0.1/22; head -n 1 <&3' 2>/dev/null || true)
        case "$banner" in
            SSH-2.0*) return 0 ;;
            *) printf '  （sshd 现在回：%s —— 1 秒后重试，第 %s 次）\n' "${banner:-没有响应}" "$i"; sleep 1 ;;
        esac
    done
    echo "  【跳过】sshd 持续限流，接下来所有「连 127.0.0.1」的演示跳过；重跑一遍通常就好。"
    return 1
}

# ksc：带重试的 ssh-keyscan。并发压力下 sshd 会直接关连接、keyscan 会返回空，
#      所以必须重试；返回空的时候千万别以为「这台服务器没有这个算法」。
ksc() {   # 用法：ksc <算法列表> <输出文件>
    local algos=$1 outfile=$2 i out
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        out=$(ssh-keyscan -t "$algos" 127.0.0.1 2>/dev/null | grep '^127' || true)
        if [ -n "$out" ]; then
            printf '%s\n' "$out" > "$outfile"
            return 0
        fi
        sleep 1
    done
    : > "$outfile"
    return 1
}

# rssh：跑一条"理应走到认证阶段"的 ssh。sshd 忙的时候会以
#   "Not allowed at this time" / "Connection reset by peer" / "Connection closed by"
# 的形式失败 —— 那是限流不是配置问题，重试即可。
rssh() {
    local i out rc=0
    for i in 1 2 3 4 5 6 7 8 9 10; do
        out=$(bash -c "$*" 2>&1) || true
        case "$out" in
            *"Not allowed at this time"*|*"Connection reset by peer"*|*"Connection closed by"*|*"No route to host"*)
                printf '  （sshd 正忙，第 %s 次重试…）\n' "$i"; sleep 1; continue ;;
        esac
        printf '%s\n' "$out"
        return 0
    done
    printf '%s\n' "$out"
    return 0
}

# raw/mayfail 是用 bash -c 跑字符串的，所以要把上面的函数导出去才看得见
export -f ksc wait_sshd rssh

mkdir -p "$SB/keys"
: > "$SB/known_hosts"
printf '沙箱：%s\n' "$SB"

# ============================================================================
scene "0：先搞清 ssh 到底从哪几层读配置（本章所有实验的前提）"
# ============================================================================
say "手册原文（man ssh_config）写的优先级顺序 —— 记住这条整章就不用猜"
raw "man ssh_config | col -b | sed -n '/obtains configuration data/,/system-wide configuration file/p' | head -8"
mayfail "man ssh_config | col -b | sed -n '/first obtained value/,+3p' | head -8"
say "不 -F 的时候，ssh 会按上面顺序去开 ~/.ssh/config 和 /etc/ssh/ssh_config。
    本 demo 故意不跑这一条（免得真去读你的用户配置）；你自己在终端敲
    \`ssh -vvv -G somehost | grep 'Reading configuration'\` 就能亲眼看到那三行。
    （注意：这段说明里的反引号是转义过的 —— 双引号里的反引号会被 bash 当命令替换真的执行出去，
     那样这条 ssh 就没有 -F /dev/null 了，会读到你的 ~/.ssh/config。本 demo 曾经踩过。）"
say "加了 -F /dev/null 之后，连系统级配置都不读，只剩一行 /dev/null"
raw "ssh -F /dev/null -vvv -G somehost 2>&1 | grep -E 'Reading configuration data'"
say "-F 指到沙箱文件，就只读沙箱文件 —— 本章所有实验都靠这一条把配置隔离开"
raw "printf 'Host demo-alias\\n  User sandbox-user\\n' > $SB/cfg0"
raw "ssh -F $SB/cfg0 -vvv -G demo-alias 2>&1 | grep -E 'Reading configuration data'"
raw "ssh -F $SB/cfg0 -G demo-alias 2>/dev/null | grep -E '^(host|user|hostname|port) '"

# ============================================================================
scene "1：最原始的两行版本号 —— 明文，不加密，也还没认证"
# ============================================================================
if wait_sshd; then
    say "用一个裸 TCP 连接读服务端说的第一行（注意：完全没用 ssh 命令）"
    mayfail "timeout 5 bash -c 'exec 3<>/dev/tcp/127.0.0.1/22; head -n 1 <&3'"
    say "客户端第一行由本地决定，格式固定：SSH-2.0-<软件名+版本>"
    raw "printf '%s\\n' 'SSH-2.0-OpenSSH_10.2p1 Ubuntu-2ubuntu3.6'"
    say "版本行之后紧跟着的第一个包就是 KEXINIT —— 里面的算法名是明文可读的"
    mayfail "timeout 5 bash -c \"exec 3<>/dev/tcp/127.0.0.1/22; head -n 1 <&3 >/dev/null; printf 'SSH-2.0-DemoClient\\r\\n' >&3; head -c 128 <&3 | strings | head -6\""
    say "上面那坨二进制里能直接读到 mlkem768x25519-sha256 这样的算法名 —— 说明「协商阶段本身没有任何机密性」，机密性从 NEWKEYS 之后才有。"
fi

# ============================================================================
scene "2：完整一次 ssh -vvv（本章的主教材，日志留在沙箱里慢慢看）"
# ============================================================================
say "先在沙箱造一把 key，待会儿拿它当身份，好让 debug 日志里出现的路径全在沙箱内"
raw "ssh-keygen -t ed25519 -C 'sandbox-demo-key' -f $SB/keys/id_demo -N '' -q"
raw "ls -l $SB/keys"
if wait_sshd; then
    say "2a) known_hosts 是空的：日志在「服务器身份验证」这步戛然而止，根本进不到认证"
    mayfail "ssh -F /dev/null -o IdentityAgent=none -o UserKnownHostsFile=$SB/known_hosts -vvv -o BatchMode=yes -o IdentitiesOnly=yes -i $SB/keys/id_demo learner@127.0.0.1 'echo hi' </dev/null > $SB/stuck.log 2>&1"
    raw "tail -5 $SB/stuck.log"
    say "2b) 加 StrictHostKeyChecking=accept-new（只写沙箱文件）后，日志才走得到 Permission denied"
    mayfail "ssh -F /dev/null -o IdentityAgent=none -o UserKnownHostsFile=$SB/known_hosts -o StrictHostKeyChecking=accept-new -vvv -o BatchMode=yes -o IdentitiesOnly=yes -i $SB/keys/id_demo learner@127.0.0.1 'echo hi' </dev/null > $SB/full_vvv.log 2>&1"
    raw "wc -l $SB/full_vvv.log"
    say "① 版本交换（对应场景 1 里那两行）"
    raw "grep -nE 'Local version string|Remote protocol version|compat_banner' $SB/full_vvv.log"
    say "② KEX 报价：客户端列表 / 服务端列表（各自出现的顺序就是各自的偏好顺序）"
    raw "grep -nE 'local client KEXINIT|peer server KEXINIT' $SB/full_vvv.log"
    raw "sed -n '/local client KEXINIT proposal/,/^debug2: ciphers ctos/p' $SB/full_vvv.log"
    raw "sed -n '/peer server KEXINIT proposal/,/^debug2: ciphers ctos/p' $SB/full_vvv.log"
    say "③ 成交：四类算法各自定了谁"
    raw "grep -n '^debug1: kex: ' $SB/full_vvv.log"
    say "④ 服务端把主机公钥递过来，顺手给出 SHA256 指纹 —— 这行就是 known_hosts 要比对的东西"
    raw "grep -n 'Server host key:' $SB/full_vvv.log"
    say "⑤ NEWKEYS：从这里开始，后面所有包都被对称加密了"
    raw "grep -nE 'SSH2_MSG_NEWKEYS|ssh_set_newkeys|rekey (in|out) after' $SB/full_vvv.log"
    say "⑥ server-sig-algs：服务端声明它接受哪些「签名」算法（决定 RSA key 用哪个哈希签）"
    raw "grep -n 'server-sig-algs' $SB/full_vvv.log"
    say "⑦ 认证阶段：服务端先亮出可用方法，客户端逐个试，最后没辙"
    raw "grep -nE 'Authenticating to|Authentications that can continue|Next authentication method|Will attempt key|Offering public key|no such identity|No more authentication|Permission denied' $SB/full_vvv.log"
    say "⑧ 前 25 行长这样（想逐行对照 README 就 cat 整份文件）"
    raw "nl -ba $SB/full_vvv.log | sed -n '1,25p'"
fi

# ============================================================================
scene "3：查「我这台机器支持哪些算法」—— ssh -Q（纯本地，不联网）"
# ============================================================================
for q in kex key cipher mac key-cert protocol-version cipher-auth; do
    say "ssh -Q $q"
    raw "ssh -Q $q | tr '\\n' ' '"
    echo
done
say "ssh -Q 列的是「编译进来支持的」；握手时默认只开放其中一部分。差集就是被新版关掉的："
raw "printf '本次握手客户端实际报价：\n'; grep -m1 '^debug2: KEX algorithms' $SB/full_vvv.log | sed 's/^debug2: KEX algorithms: //' | tr ',' '\n' | grep -vE 'ext-info|kex-strict' | tr '\n' ' '; echo"
raw "printf '\n编译支持但默认不报价的：\n'; comm -13 <(grep -m1 '^debug2: KEX algorithms' $SB/full_vvv.log | sed 's/^debug2: KEX algorithms: //' | tr ',' '\n' | sort) <(ssh -Q kex | sort) | tr '\n' ' '; echo"

# ============================================================================
scene "4：known_hosts 为什么存在 —— 第一次连接 = TOFU（Trust On First Use）"
# ============================================================================
: > "$SB/kh_first"
if wait_sshd; then
    say "空 known_hosts + 交互式第一次连接，ssh 会停下来问你一句（用 python 起个假终端，把提示原样录下来，然后回答 no）"
    mayfail "python3 - '$SB' <<'PY'
import os, pty, sys, time, select
sb = sys.argv[1]
cmd = ['ssh', '-F', '/dev/null', '-o', 'IdentityAgent=none',
       '-o', 'UserKnownHostsFile=' + sb + '/kh_first', 'learner@127.0.0.1', 'echo hi']
pid, fd = pty.fork()
if pid == 0:
    os.execvp(cmd[0], cmd)
buf = b''
t0 = time.time()
while time.time() - t0 < 12:
    r, _, _ = select.select([fd], [], [], 0.3)
    if not r:
        continue
    try:
        d = os.read(fd, 4096)
    except OSError:
        break
    if not d:
        break
    sys.stdout.write(d.decode(errors='replace'))
    buf += d
    if b'yes/no' in buf:
        os.write(fd, b'no\n')      # 故意回答 no：什么都不写入，最安全
        break
os.close(fd)
try:
    os.kill(pid, 15)
    os.waitpid(pid, 0)
except Exception:
    pass
PY"
    say "回答 no 之后 known_hosts 依然是 0 字节 —— 这就是「还没建立任何信任」的状态"
    raw "wc -c < $SB/kh_first"
    say "accept-new：把「第一次」自动化（只写沙箱文件）"
    raw "rssh 'ssh -F /dev/null -o IdentityAgent=none -o BatchMode=yes -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh_first learner@127.0.0.1 true </dev/null | head -3'"
    raw "cat $SB/kh_first"
    say "对这条算指纹，结果和 -vvv 日志里 Server host key: 一模一样"
    raw "ssh-keygen -lf $SB/kh_first"
fi

# ============================================================================
scene "5：known_hosts 记错 / 服务器换了密钥 —— 那个最吓人的报错（本机就能演）"
# ============================================================================
raw "ssh-keygen -t ed25519 -C 'wrong-server-key' -f $SB/keys/bogus -N '' -q"
raw "{ printf '127.0.0.1 '; cut -d' ' -f1,2 $SB/keys/bogus.pub; } > $SB/kh_wrong"
say "known_hosts 里放了一把「不属于这台服务器的」公钥，然后连接："
if wait_sshd; then
    raw "rssh 'ssh -F /dev/null -o IdentityAgent=none -o BatchMode=yes -o UserKnownHostsFile=$SB/kh_wrong learner@127.0.0.1 true </dev/null'"
    say "ssh 已经把删除命令替你想好了（-f 指定文件、-R 指定主机）："
    raw "ssh-keygen -f $SB/kh_wrong -R 127.0.0.1 | tail -3"
    raw "cat $SB/kh_wrong"
fi

# ============================================================================
scene "6：ssh-keyscan —— 抓得到的东西，一点信任含量都没有"
# ============================================================================
say "下面几条里的 ksc 就是脚本开头那个带重试的 ssh-keyscan 包装；你手敲时直接写
    ssh-keyscan -t ed25519,rsa,ecdsa 127.0.0.1  就行"
if wait_sshd; then
    say "keyscan 的本质：把服务端「自报」的公钥抄下来，顺手把版本行当注释打出来"
    mayfail "ksc ed25519,rsa,ecdsa $SB/scan_all.txt && cut -c1-78 $SB/scan_all.txt"
    say "把 keyscan 结果直接写进 known_hosts，服务器身份这一关照样通过 —— 因为它根本没校验过任何东西："
    mayfail "ksc ed25519 $SB/kh_scan && cut -c1-78 $SB/kh_scan"
    raw "rssh 'ssh -F /dev/null -o IdentityAgent=none -o BatchMode=yes -o UserKnownHostsFile=$SB/kh_scan learner@127.0.0.1 true </dev/null'"
    say "危险点：keyscan 用的传输通道和真正的连接是同一条，中间人递什么它就记什么。"
    say "分层认知：known_hosts 里写的 ssh-rsa 是「密钥类型」，实际签名用 rsa-sha2-512，两码事"
    mayfail "ksc rsa $SB/kh_only_rsa && cut -c1-60 $SB/kh_only_rsa"
    raw "rssh 'ssh -F /dev/null -o IdentityAgent=none -o BatchMode=yes -o UserKnownHostsFile=$SB/kh_only_rsa -vvv learner@127.0.0.1 true </dev/null 2>&1 | grep -E \"kex: host key algorithm|Server host key:\"'"
    say "同一份 keyscan 结果，用 ssh-keygen -lf 算出的指纹应该和服务端自报的一致"
    mayfail "while read -r h type blob; do printf '%-24s' \"\$type\"; printf '%s %s\\n' \"\$type\" \"\$blob\" > $SB/one.pub; ssh-keygen -lf $SB/one.pub | awk '{print \$2}'; done < $SB/scan_all.txt"
    say "顺手记一下：这台机器上真实的 ED25519 主机密钥指纹是 —— 换个终端重跑一次 keyscan，值不变；
        但它变不变只取决于服务器，不取决于你信任与否。"
fi

# ============================================================================
scene "7：公钥加密 ≠ 签名 —— 直接做一遍（全程离线，不连任何主机）"
# ============================================================================
mkdir -p "$SB/sign"; cd "$SB/sign"
raw "printf 'session-log line 1\ntransfered 42 files\n' > payload.txt"
raw "ssh-keygen -t ed25519 -C mine  -f ./mine  -N '' -q"
raw "ssh-keygen -t ed25519 -C other -f ./other -N '' -q"
say "方向：私钥签名、公钥验证 —— 登录用的就是这个方向，不是「用私钥加密一段密码」"
raw "ssh-keygen -Y sign -n file-verify -f ./mine payload.txt"
raw "ssh-keygen -Y check-novalidate -n file-verify -f ./mine.pub -s payload.txt.sig < payload.txt"
say "改一个字节 —— 立刻验不过（这防的是篡改，不是偷看）"
raw "printf 'session-log line X\ntransfered 42 files\n' > payload.txt"
mayfail "ssh-keygen -Y check-novalidate -n file-verify -f ./mine.pub -s payload.txt.sig < payload.txt"
raw "printf 'session-log line 1\ntransfered 42 files\n' > payload.txt"
raw "ssh-keygen -Y sign -n file-verify -f ./mine payload.txt >/dev/null 2>&1"
say "换用途标签（namespace）也验不过 —— 签名绑死了用途，不能被搬到别处复用"
mayfail "ssh-keygen -Y check-novalidate -n other-purpose -f ./mine.pub -s payload.txt.sig < payload.txt"
say "坑：check-novalidate 只保证「签名自洽」，不保证「是谁签的」—— 拿别人的公钥问它照样说 Good"
mayfail "ssh-keygen -Y check-novalidate -n file-verify -f ./other.pub -s payload.txt.sig < payload.txt"
say "真查身份要用 -Y verify + allowed_signers 名单（证书体系、git 验 tag 用的就是这套思路）"
raw "printf 'mine-id  ssh-ed25519 %s\nother-id ssh-ed25519 %s\n' \"\$(cut -d' ' -f2 mine.pub)\" \"\$(cut -d' ' -f2 other.pub)\" > allowed_signers"
raw "cut -c1-58 allowed_signers"
raw "ssh-keygen -Y verify -f allowed_signers -I mine-id  -n file-verify -s payload.txt.sig < payload.txt"
mayfail "ssh-keygen -Y verify -f allowed_signers -I other-id -n file-verify -s payload.txt.sig < payload.txt"
cd "$SB"

# ============================================================================
scene "8：主机密钥 vs 用户密钥 —— 同一种东西，两种用途"
# ============================================================================
if wait_sshd; then
    say "服务端的主机密钥（属于这台机器；私钥在服务器 /etc/ssh/ssh_host_*；公钥由客户端记在 known_hosts）"
    mayfail "ksc ed25519 $SB/hostkey.pub"
    raw "ssh-keygen -lf $SB/hostkey.pub"
    say "它的内容格式："
    raw "cut -c1-60 $SB/hostkey.pub"
fi
say "你的用户密钥（属于你这个人；私钥在你手上；公钥贴到服务器的 ~/.ssh/authorized_keys）"
raw "ssh-keygen -lf $SB/keys/id_demo.pub"
raw "cut -c1-60 $SB/keys/id_demo.pub"
say "两者格式一模一样，区别全在「存在谁的机器上、被谁信任、被用来证明什么」。"

# ============================================================================
scene "9：算法相关的四种真实报错（背下来能省半小时）"
# ============================================================================
if wait_sshd; then
    say "只允许一个服务器不支持的 KEX —— 报错会把双方的清单都念给你听"
    raw "rssh 'ssh -F /dev/null -o IdentityAgent=none -o BatchMode=yes -o UserKnownHostsFile=$SB/known_hosts -o KexAlgorithms=diffie-hellman-group1-sha1 learner@127.0.0.1 true </dev/null 2>&1 | head -2'"
    say "只允许一个服务器不再提供的主机密钥签名类型（SHA-1 的 ssh-rsa 在 OpenSSH 8.8 起默认关闭）"
    raw "rssh 'ssh -F /dev/null -o IdentityAgent=none -o BatchMode=yes -o UserKnownHostsFile=$SB/known_hosts -o HostKeyAlgorithms=ssh-rsa learner@127.0.0.1 true </dev/null 2>&1 | head -2'"
    say "只留一个老掉牙的对称算法"
    raw "rssh 'ssh -F /dev/null -o IdentityAgent=none -o BatchMode=yes -o UserKnownHostsFile=$SB/known_hosts -o Ciphers=3des-cbc learner@127.0.0.1 true </dev/null 2>&1 | head -2'"
    say "正确姿势：用 + 追加，而不是覆盖整张列表"
    raw "rssh 'ssh -F /dev/null -o IdentityAgent=none -o BatchMode=yes -o UserKnownHostsFile=$SB/known_hosts -o '\''KexAlgorithms=+diffie-hellman-group14-sha1'\'' learner@127.0.0.1 true </dev/null 2>&1 | head -2'"
    say "被限流时你看到的其实是这个（不是你的配置坏了）"
    raw "printf '%s\n' 'kex_exchange_identification: banner line 0: Not allowed at this time' 'Connection reset by 127.0.0.1 port 22'"
fi

printf '\n\n================ 收尾自证 ================\n'
printf '本 demo 不启动任何后台进程。检查残留：\n'
printf '  1) 监听端口（不应出现 2222 / 8080 / 1080 之类新增）：\n'
ss -lnt 2>/dev/null | tail -n +2 | awk '{print "     " $4}'
printf '  2) 遗留的 ssh 客户端 / pty 子进程（应为空）：\n'
pgrep -af 'ssh -F /dev/null' 2>/dev/null || echo "     （无）"
printf '  3) 沙箱里都生了什么：\n'
raw "find $SB -type f | sed \"s|$SB/|     |\" | sort | head -40"
