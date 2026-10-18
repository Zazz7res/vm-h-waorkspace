#!/usr/bin/env bash
# ============================================================================
# 05 三种端口转发 —— 一键复现整章场景
#
# 用法：
#   bash demo.sh            # 跑完自动杀掉所有自己起的进程、删掉沙箱
#   bash demo.sh --keep     # 保留沙箱和进程（打印 PID 和端口，你自己玩完记得 kill）
#
# 本章的主打思路：先用【人肉隧道】把 ssh -L 的本质在你眼前复现一遍（本机 100% 可跑），
# 然后再给等价 ssh 命令。真正需要服务器的部分（隧道打通、-g 被服务端允许、GatewayPorts）
# 全部明确标注，脚本里不假装成功。
#
# 安全边界：
#   * 全部端口都在 127.0.0.1 / 0.0.0.0 的本机监听，绝不连外部主机
#   * 每条 ssh 都带 -F /dev/null + 沙箱 UserKnownHostsFile + BatchMode + 只允许 publickey
#     => 不可能登录成功，不可能提交口令
#   * 每个后台进程都写 PID 文件，trap 里统一 kill，退出前用 ss -lnt 复核没有遗留监听
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

SB=$(mktemp -d "${TMPDIR:-/tmp}/ssh05-XXXXXX")
mkdir -p "$SB/site" "$SB/site2" "$SB/pids"
PIDS=()

# ---- 挑一串空闲端口（一次问内核要，避免和你机器上已有服务撞车）------------
find_free_ports() {
    python3 - "$1" <<'PY'
import socket, sys
n = int(sys.argv[1]); ports = []
for _ in range(n):
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    ports.append(s.getsockname()[1]); s.close()
print(" ".join(map(str, ports)))
PY
}
read -r T1 T2 DOOR SOCKDOOR BUSY SOCKS PORT22HOST <<< "$(find_free_ports 7)"
# 两个「内网服务」的页面内容（等下用来证明门后面接的是谁）
printf '你好，我是【目标服务 A】，监听在 127.0.0.1:%s（把它想成内网数据库/内部 API）\n' "$T1" > "$SB/site/hello.txt"
printf '你好，我是【目标服务 B】，监听在 127.0.0.1:%s（另一个内部服务）\n' "$T2" > "$SB/site2/hello.txt"

cleanup() {
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[保留] 沙箱=%s  起的进程 PID=%s\n' "$SB" "${PIDS[*]:-无}"
        printf '        自己收尾： for p in %s; do kill $p; done\n' "${PIDS[*]:-}"
        return
    fi
    for p in "${PIDS[@]:-}"; do kill "$p" 2>/dev/null || true; done
    sleep 0.4
    for p in "${PIDS[@]:-}"; do kill -9 "$p" 2>/dev/null || true; done
    rm -rf "$SB"
    printf '\n[沙箱已删除] %s\n' "$SB"
    printf '[收尾自检 1] 本脚本起的进程还在不在：\n'
    if [ -n "${PIDS[*]:-}" ]; then
        for p in "${PIDS[@]}"; do
            if kill -0 "$p" 2>/dev/null; then printf '  ⚠ PID %s 仍存活\n' "$p"; else printf '  PID %s 已回收\n' "$p"; fi
        done
    else printf '  （没有起过进程）\n'; fi
    printf '[收尾自检 2] 有没有遗留监听端口（本章用过的端口都应消失）：\n'
    ss -lnt 2>/dev/null | grep -E ":($T1|$T2|$DOOR|$SOCKDOOR|$BUSY|$SOCKS) " | sed 's/^/  /' || printf '  干净：没有遗留监听\n'
    printf '[收尾自检 3] 顺便确认没有当年那种 2222 僵尸监听：\n'
    ss -lnt 2>/dev/null | grep -E ':(2222|8098|8099|1080) ' | sed 's/^/  /' || printf '  干净\n'
}
trap cleanup EXIT

scene() { printf '\n\n======== 场景 %s ========\n' "$*"; }
say()   { printf '\n--- %s\n' "$*"; }
line()  { printf '%s\n' "$*"; }
run()   { printf '\n$ %s\n' "$*"; bash -c "$*"; }
note()  { printf '\n$ %s\n  （这条按设计会失败，就为了看真实报错原文）\n' "$*"
          bash -c "$*" 2>&1 || printf '  [退出码 %s —— 非 0 是预期的]\n' "$?"; }
# startbg：起一个后台进程并登记 PID（第一个参数是名字，其余是命令）
startbg() {
    local name=$1; shift
    printf '\n$ %s &        # 后台起【%s】\n' "$*" "$name"
    bash -c "$*" >/dev/null 2>&1 &
    local p=$!
    PIDS+=("$p"); printf '%s\n' "$p" > "$SB/pids/$name"
    printf '  [PID %s 已登记，脚本退出时会自动 kill]\n' "$p"
    sleep 1
}

# 远端用户名用中性名 learner：本章每一个连接都按设计停在认证（Permission denied），
# 用一个本机不存在的用户名和一个真实登录名得到的报错、退出码、结论完全一样，
# 而文档里因此不会印出跑脚本那个人的真实登录名（全书红线「屏幕上不出现真实身份」）。
SSH_TARGET="learner@127.0.0.1"
SSH_BASE="ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh -o IdentityFile=$SB/keys/_no-default-probe -o ConnectTimeout=8"

# ---- 本章的两个小工具（都写在沙箱里）--------------------------------------
cat > "$SB/relay.py" <<'PY'
#!/usr/bin/env python3
"""人肉隧道中转：本地 listen，收到的字节原样送到上游，反向亦然。
用法: relay.py LISTEN_HOST LISTEN_PORT_or_PATH UP_HOST UP_PORT LOGFILE
     LISTEN_HOST=unix 时，第二个参数是 unix socket 路径"""
import socket, sys, threading, time, os
lh, lp_arg, uh, up, logfile = sys.argv[1:6]
log = open(logfile, "a")
def lg(m): print(f"[relay {time.strftime('%H:%M:%S')}] {m}", file=log, flush=True)
def pump(a, b, tag):
    n = 0
    try:
        while True:
            d = a.recv(4096)
            if not d: break
            n += len(d); b.sendall(d)
    except OSError: pass
    finally:
        try: b.shutdown(socket.SHUT_WR)
        except OSError: pass
        lg(f"  {tag} 转发 {n} 字节后关闭")
if lh == "unix":
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    if os.path.exists(lp_arg): os.remove(lp_arg)
    s.bind(lp_arg); lg(f"监听 unix socket {lp_arg} -> 上游 {uh}:{up}")
else:
    s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind((lh, int(lp_arg))); lg(f"监听 {lh}:{lp_arg} -> 上游 {uh}:{up}")
s.listen(5)
while True:
    c, peer = s.accept()
    try:
        u = socket.create_connection((uh, int(up)), timeout=3)
    except Exception as e:
        lg(f"  {peer}: 连上游 ({uh},{up}) 失败 -> {type(e).__name__}: {e}"); c.close(); continue
    lg(f"  {peer}: 已连上上游 ({uh},{up})")
    threading.Thread(target=pump, args=(c, u, "本地->上游"), daemon=True).start()
    threading.Thread(target=pump, args=(u, c, "上游->本地"), daemon=True).start()
PY

cat > "$SB/fake_socks5.py" <<'PY'
#!/usr/bin/env python3
"""最小 SOCKS5 服务端：把客户端请求的目标【原样打印】出来然后拒绝连接。
   目的：亲眼看到 --socks5 发来的是 IP（本地已解析），--socks5-hostname 发来的是域名。
用法: fake_socks5.py PORT LOGFILE"""
import socket, sys, threading, struct
port, logfile = int(sys.argv[1]), sys.argv[2]
log = open(logfile, "a")
def lg(m): print(m, file=log, flush=True)
def decode(b, i):
    atyp = b[i]; i += 1
    if atyp == 1: return atyp, ".".join(str(x) for x in b[i:i+4]), i+4
    if atyp == 3:
        n = b[i]; return atyp, b[i+1:i+1+n].decode(), i+1+n
    if atyp == 4: return atyp, ":".join(f"{b[i+j]:02x}" for j in range(16)), i+16
    return atyp, f"<未知 ATYP {atyp}>", i
def handle(c):
    try:
        g = c.recv(256)
        if len(g) < 2: return
        lg(f"[socks5] 问候: VER={g[0]} 方法数={g[1]} 客户端会的方法={list(g[2:2+g[1]])}")
        c.sendall(bytes([5, 0]))                       # 选「无需认证」
        r = c.recv(512)
        if len(r) < 4: return
        atyp, host, nxt = decode(r, 3)
        dport = struct.unpack("!H", r[nxt:nxt+2])[0]
        lg(f"[socks5] 请求: CMD={r[1]}(1=CONNECT) 目标类型={'IPv4' if atyp==1 else '域名' if atyp==3 else 'IPv6'} 目标={host}:{dport}")
        c.sendall(bytes([5, 5, 0, 1, 0, 0, 0, 0, 0, 0]))   # 05 = connection refused
    except Exception as e:
        lg(f"[socks5] 异常: {e}")
    finally:
        c.close()
s = socket.socket(); s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", port)); s.listen(5)
lg(f"[socks5] 已在 127.0.0.1:{port} 监听")
while True:
    c, _ = s.accept(); threading.Thread(target=handle, args=(c,), daemon=True).start()
PY

printf '沙箱：%s\nOpenSSH：%s\n本轮用到的本机端口：目标A=%s 目标B=%s 门=%s 另一个门=%s 被占的门=%s SOCKS=%s\n' \
    "$SB" "$(ssh -V 2>&1)" "$T1" "$T2" "$DOOR" "$SOCKDOOR" "$BUSY" "$SOCKS"

# ============================================================================
scene "1：先搭一个『目标服务』—— 等价于内网里那台你连不到的机器"
# ============================================================================
startbg targetA "python3 -m http.server $T1 --bind 127.0.0.1 --directory $SB/site"
say "它只绑 127.0.0.1，所以 ss 里看到的是这一行："
run "ss -lnt | grep ':$T1 ' || echo '  (没命中 = 它没在监听)'"
say "基准：直接访问它，等下好对比"
run "curl -s --max-time 5 http://127.0.0.1:$T1/hello.txt"

# ============================================================================
scene "2：人肉版 ssh -L —— 在本地再开一个『门』，把字节搬给目标"
# ============================================================================
startbg relay_door "python3 $SB/relay.py 127.0.0.1 $DOOR 127.0.0.1 $T1 $SB/relay.log"
say "现在 curl 那个门（$DOOR），拿回来的却是目标（$T1）的内容："
run "curl -s --max-time 5 http://127.0.0.1:$DOOR/hello.txt"
say "ss 一眼看清：两个端口都在监听，一个是目标、一个是门"
run "ss -lnt | grep -E ':($T1|$DOOR) '"
say "门自己的日志（铁证：是它在替你搬字节，还告诉你它连的是谁）"
run "cat $SB/relay.log"
line "  ↑ 把上面这套东西换个壳子，就是： ssh -N -L $DOOR:127.0.0.1:$T1 用户@服务器"
line "    ssh 帮你写的就是这个『本地 listen + 顺着已有加密连接搬字节』的中转进程，"
line "    区别只在于：真实场景里『连目标』这一步发生在【服务器那侧】，所以能穿过防火墙。"

# ============================================================================
scene "3：门后面接谁是配置决定的 —— 目标是『相对中转点』看的"
# ============================================================================
startbg targetB "python3 -m http.server $T2 --bind 127.0.0.1 --directory $SB/site2"
say "把同一扇门改接目标 B：客户端一个字都没改，拿到的内容换了一个服务"
kill "$(cat "$SB/pids/relay_door")" 2>/dev/null || true
sleep 0.4
startbg relay_door2 "python3 $SB/relay.py 127.0.0.1 $DOOR 127.0.0.1 $T2 $SB/relay2.log"
run "curl -s --max-time 5 http://127.0.0.1:$DOOR/hello.txt"
run "tail -2 $SB/relay2.log"
say "更狠一点：把门接向本机的 22 端口（sshd 自己）和 631 端口（打印服务），客户端看到的完全变了："
startbg relay_ssh "python3 $SB/relay.py 127.0.0.1 $SOCKDOOR 127.0.0.1 22 $SB/relay_ssh.log"
run "timeout 3 nc 127.0.0.1 $SOCKDOOR </dev/null | head -c 40; echo"
say "同一个门，把上游改成 631（本机另一个真实服务）："
kill "$(cat "$SB/pids/relay_ssh")" 2>/dev/null || true
sleep 0.4
startbg relay_cups "python3 $SB/relay.py 127.0.0.1 $SOCKDOOR 127.0.0.1 631 $SB/relay_cups.log"
run "printf 'GET / HTTP/1.0\r\n\r\n' | timeout 3 nc 127.0.0.1 $SOCKDOOR | head -2"
line "  ↑ 结论（本章唯一的难点）：-L 里冒号后面那个地址，是【中转点那侧】的地址。"
line "    所以 -L 8080:localhost:80 里的 localhost = 服务器自己的 localhost，不是你的。"

# ============================================================================
scene "4：绑定地址 —— 为什么 ssh 默认只听 127.0.0.1（这就是 -g 的安全含义）"
# ============================================================================
startbg relay_all "python3 $SB/relay.py 0.0.0.0 $BUSY 127.0.0.1 $T1 $SB/relay_all.log"
say "两种门并排看：左边只绑回环（ssh 默认），右边绑 0.0.0.0（等价于加了 -g）"
run "ss -lnt | grep -E ':($DOOR|$BUSY) '"
run "ip -4 -o addr show | awk '{print \"  本机地址:\", \$4}' | head -6 || true"
line "  ↑ 0.0.0.0 的门会被同网段任何人连走，等于把内网服务公开一小块。"
line "    ssh 要 -g 才肯这么绑，而且服务端还有 GatewayPorts 管着远端那一侧（见 05 章 README）。"

# ============================================================================
scene "5：端口被占 —— 真实报错原文"
# ============================================================================
say "再起一个门去抢同一个端口（$DOOR 已经在监听）："
note "python3 $SB/relay.py 127.0.0.1 $DOOR 127.0.0.1 $T1 /dev/null 2>&1 | tail -2"
line "  ↑ 这就是 bind() 失败时内核给的话：[Errno 98] Address already in use。"
line "    ssh 客户端打印的是同一个 strerror，文案模板在它二进制里能查到："
run "strings /usr/bin/ssh | grep -E '^bind \[%s\]:%s' | head -2 || echo '  (这条模板没找到)'"
line "    真实形态大约是：bind [127.0.0.1]:$DOOR: Address already in use"
line "    ⚠ 但『让 ssh 自己打印这行』要先过认证这一关（本地门是在登录成功之后才建的）。"
line "      本机不允许登录成功，所以这条只能到服务器验证；下面是本机真跑到的那一半："
run "$SSH_BASE -N -L $DOOR:127.0.0.1:$T1 $SSH_TARGET 2>&1 | tail -2 || true"
line "  ↑ 看到了吗：ssh 先死在认证上，压根走不到建门那一步。这就是为什么本章隧道效果需服务器验证。"

# ============================================================================
scene "6：unix socket 形态的门（对应 -L /path.sock:目标 和 docker socket 转发）"
# ============================================================================
startbg relay_unix "python3 $SB/relay.py unix $SB/tun.sock 127.0.0.1 $T1 $SB/relay_unix.log"
say "socket 文件长这样（第一位 s = socket）："
run "ls -l $SB/tun.sock"
run "ss -xlp 2>/dev/null | grep tun.sock | head -2 || echo '  (没命中)'"
say "用 curl --unix-socket 走它，拿到的还是目标 A 的内容："
run "curl -s --max-time 5 --unix-socket $SB/tun.sock http://localhost/hello.txt"
line "  ↑ 等价 ssh 写法（需服务器验证）： ssh -N -L $SB/tun.sock:127.0.0.1:$T1 用户@服务器"
line "    以及更常用的『把远端 docker 的 socket 拿到本地』：-L /tmp/r.docker.sock:/var/run/docker.sock"

# ============================================================================
scene "7：隧道进程死了，用你的程序会看到什么"
# ============================================================================
kill "$(cat "$SB/pids/relay_door2")" 2>/dev/null || true
sleep 0.5
say "监听消失了："
run "ss -lnt | grep ':$DOOR ' || echo '  （门已经没了）'"
note "curl -sS --max-time 5 http://127.0.0.1:$DOOR/hello.txt"
line "  ↑ 『隧道悄悄断了』在客户端就是一个普通连接错误；服务端那一侧不留痕，所以脚本里要加"
line "    -o ExitOnForwardFailure=yes + ServerAliveInterval，见下一个场景的配置。"

# ============================================================================
scene "8：-D 动态代理 —— 用真 SOCKS5 服务证明 DNS 到底在谁那边解析"
# ============================================================================
startbg socks "python3 $SB/fake_socks5.py $SOCKS $SB/socks.log"
say "写法一：curl --socks5（或 -x socks5://）= 本地先把域名解析成 IP，再把 IP 交给代理"
note "curl -sS --max-time 6 --socks5 127.0.0.1:$SOCKS http://never-resolves-invalid.example/"
say "上面那句 『Could not resolve proxy』是 curl 的文案在骗人 —— 加 -v 才看得见真实原因"
note "curl -sS -v --max-time 6 --socks5 127.0.0.1:$SOCKS http://never-resolves-invalid.example/ 2>&1 | grep -E 'Trying|Failed to resolve|Store negative|curl:'"
say "写法二：curl --socks5-hostname（或 -x socks5h://）= 域名原样交给代理，由代理解析"
note "curl -sS --max-time 6 --socks5-hostname 127.0.0.1:$SOCKS http://never-resolves-invalid.example/"
say "再给一个更有说服力的对比：目标换成【能解析的】 localhost"
note "curl -sS --max-time 6 -x socks5://127.0.0.1:$SOCKS http://localhost:$T1/hello.txt"
note "curl -sS --max-time 6 -x socks5h://127.0.0.1:$SOCKS http://localhost:$T1/hello.txt"
say "代理那一侧收到的东西（这就是两者的全部区别）："
run "cat $SB/socks.log"
line "  ↑ 铁证三条，全在上面的日志里："
line "    · 写法一（--socks5 / socks5://）那次，代理日志里只有『问候』没有『请求』—— 因为 curl 先在本地解析"
line "      域名就失败了，压根没把请求送出去（还顺手把解析结果缓存成 negative）；"
line "    · 写法一 + localhost 那次，代理收到的目标类型是 IPv6 —— 说明本地已经解析过了，"
line "      而且 localhost 被解析成 ::1 而不是 127.0.0.1，这是很常见的一类『为什么我走隧道连不上 127.0.0.1 的服务』；"
line "    · 写法二（--socks5-hostname / socks5h://）代理收到的是域名字符串本身，DNS 发生在代理那一侧 —— "
line "      这才是 -D 隧道想要的效果：用服务器那边的 DNS 视图、也不泄露你的查询。"
say "对不存在 SOCKS 端口的真实报错（脚本收尾后就是这个状态）："
kill "$(cat "$SB/pids/socks")" 2>/dev/null || true
sleep 0.4
note "curl -sS --max-time 6 -x socks5h://127.0.0.1:$SOCKS http://example.com/"

# ============================================================================
scene "9：ssh 侧的参数：语法错误能在本机真跑，效果需服务器"
# ============================================================================
say "这些是本机真跑出来的客户端报错（连不上都轮不到，纯语法）："
for spec in "-L badspec" "-D abc" "-L $T1" "-L :80" "-L 0:127.0.0.1:80"; do
    printf '  ssh %s -> ' "$spec"
    # 注意这里要 || true：head -1 提前关管道会让 ssh 收到 SIGPIPE（退出码 141），
    # set -o pipefail 会把这种「正常打印完就收工」当成失败而中断脚本。
    { timeout 15 ssh -F /dev/null -o BatchMode=yes -o UserKnownHostsFile="$SB/kh" $spec "$SSH_TARGET" 2>&1 || true; } | head -1
done
say "生产脚本里必须加的两个选项（默认值本机可查）："
run "ssh -F /dev/null -G $SSH_TARGET | grep -E '^(exitonforwardfailure|serveraliveinterval|serveralivecountmax|tcpkeepalive) '"
line "  exitonforwardfailure no ← 默认『门没开成功也算连接成功』，这是最坑人的静默失败"
line "  serveraliveinterval 0   ← 默认不发保活包，NAT/防火墙掐线后你毫无察觉"
say "推荐组合（需服务器验证，命令模板）："
line "    ssh -f -N -o ExitOnForwardFailure=yes -o ServerAliveInterval=30 \\"
line "        -o ServerAliveCountMax=3 -L $DOOR:127.0.0.1:$T1 用户@服务器"
line "    验证门确实开了：ss -lnt | grep $DOOR    然后 curl -sS http://127.0.0.1:$DOOR/"
say "-R / -D 的服务端开关（本机查不到，因为在 sshd 那边）：需要服务器验证"
line "    sshd 侧：GatewayPorts no|clientspecified|yes   PermitTunnel no|yes|ethernet|ip"
line "             AllowTcpForwarding yes|no|local|remote"
line "    自查模板： ssh -G 用户@服务器 | grep -i forward   （客户端侧）"
line "             服务器端 root： sshd -T | grep -iE 'gatewayports|allowtcpforward|permittunnel'"

# ============================================================================
scene "10：README 的补充实测 —— 语法边界 / -G 的转发键 / 谁占了端口 / LocalForward 的两段式"
# ============================================================================
# 这一节收的是原来散在 demo 之外的补跑取证（旧的 C1…C6），并进 demo 只有一个目的：
# README 里每一条围栏都能被 `bash demo.sh` 一次跑出来，不靠「另外一份文件」。
# 护栏和前面完全一样：-F /dev/null + BatchMode + 沙箱 known_hosts + 不存在的 IdentityFile。
# 每条之间 sleep 2：这些是会真连 22 端口的连接，别把本机 sshd 打进 MaxStartups 限流。
say "① -L / -R / -D 的段数与端口边界（Bad … specification = 停在解析；Permission denied = 过了 parsing、死在认证）"
for spec in "-R badspec" "-D 0" "-D 127.0.0.1:abc" "-D $DOOR" \
            "-L 8080" "-L 8080:127.0.0.1:99999" \
            "-L 127.0.0.1:0:127.0.0.1:$T1" "-L :0:127.0.0.1:$T1" \
            "-L 0.0.0.0:8080:127.0.0.1:$T1" "-L [::1]:8080:127.0.0.1:$T1" \
            "-L $SB/a.sock:127.0.0.1:80" \
            "-R 80" "-R 9000:localhost:3000" "-R 0:localhost:3000"; do
    note "$SSH_BASE $spec $SSH_TARGET"
    sleep 2
done
say "② 客户端侧 -G 能 dump 出来的转发相关键（服务端那三个开关不在这张表里）"
run "ssh -F /dev/null -G $SSH_TARGET | grep -iE 'forward|clearall|gateway|tunnel'"
say "③ 端口被占时怎么查『是谁』—— 非 root 只对自己 OWN 的进程有 Process 列"
run "ss -lntp | grep -E ':(631|$T1) '"
run 'fuser 631/tcp; echo exit=$?'
run "ss -lntp | grep ':22 ' || echo '  (22 这一行没找到)'"
say "④ 这个 ssh 二进制里的转发文案模板全表（README 引的就是这里面的原句）"
run "strings /usr/bin/ssh | grep -iE 'forward|gatewayports|specification' | grep -E '%s|%d|%u'"
say "⑤ 配置文件里写转发是『两个参数』，不是命令行那一串"
printf 'Host t\n  LocalForward 8080:127.0.0.1:3306\n' > "$SB/onearg"
printf 'Host t\n  LocalForward 8080 127.0.0.1:3306\n' > "$SB/twoarg"
printf 'Host t\n  LocalForward 8080 127.0.0.1:3306\n  RemoteForward 9000 localhost:3000\n  DynamicForward 1080\n  ExitOnForwardFailure yes\n  ServerAliveInterval 30\n  ServerAliveCountMax 3\n' > "$SB/cfg"
note "ssh -F $SB/onearg -G t 2>&1 | tail -2"
# grep -c 在「一条都没命中」时退出码是 1，所以这里必须 || true（否则 set -e 当场中断脚本）
run "ssh -F $SB/twoarg -G t | grep -icE 'bad|missing' || true"
run "ssh -F $SB/cfg -G t | grep -icE 'bad|missing' || true"
run "ssh -F $SB/cfg -G t | grep -E '^(exitonforwardfailure|serveraliveinterval|serveralivecountmax) '"

printf '\n以上 10 个场景就是 README 里各小节的证据链。\n'
