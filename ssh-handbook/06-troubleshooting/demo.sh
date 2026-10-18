#!/usr/bin/env bash
# ============================================================================
# 06 排错手册 —— 每条『真实报错原文』都在这台机器上真跑出来
#
# 用法：
#   bash demo.sh              # 跑完自动删沙箱、杀掉自己起的进程/agent
#   bash demo.sh --keep       # 保留沙箱（打印路径和 PID，自己收尾）
#   HAMMER=1 bash demo.sh     # 额外加演「并发打满被随机丢包」（对本机 sshd 连 14 次）
#
# 本章按【报错原文】组织，脚本也照原文组织：每段先给命令，再贴它真实打印的东西。
# 跑不出来的（需要登录成功 / 需要服务器 / 需要 root）一律标注「需服务器验证」，
# 并给完整命令模板 —— 不假装成功、不编造输出。
#
# 安全边界（全书统一）：
#   * 只连 127.0.0.1，绝不碰外部主机
#   * 第一件事就是 unset SSH_AUTH_SOCK/SSH_AGENT_PID：脚本绝不用你桌面上的 agent，
#     也就绝不会把你真实私钥的注释（通常是你邮箱）打进输出里
#   * 每条 ssh 都带 -F /dev/null 或沙箱 config + 沙箱 UserKnownHostsFile + BatchMode
#     + 一条不存在的 IdentityFile 当挡箭牌 => 不可能登录成功、不可能弹口令、
#     不可能读到你真实的 ~/.ssh
#   * 服务器侧（/etc/ssh、sshd、ssh.socket、systemctl、防火墙）一律只读不改
#   * 私钥只在沙箱现生成；脚本从头到尾没 cat 过任何私钥内容
#   * 本机 sshd 有并发/频率保护，连太快会被随机 reset —— 所以每条会真连接的命令
#     之间都 sleep 10（这本身就是场景 9 要讲的现象）
# ============================================================================
set -euo pipefail

KEEP=0
for arg in "$@"; do
    case "$arg" in
        --keep) KEEP=1 ;;
        -h|--help) echo "用法: bash demo.sh [--keep]   （可选 HAMMER=1）"; exit 0 ;;
        *) echo "未知参数：$arg（本脚本只认 --keep）" >&2; exit 2 ;;
    esac
done
HAMMER=${HAMMER:-0}

unset SSH_AUTH_SOCK SSH_AGENT_PID 2>/dev/null || true

SB=$(mktemp -d "${TMPDIR:-/tmp}/ssh06-XXXXXX")
mkdir -p "$SB/keys" "$SB/pids" "$SB/kh" "$SB/bin" "$SB/cfg/d" "$SB/repo" "$SB/fakehome/.ssh"
PIDS=()
AGENTS=()

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
read -r P_CLOSED P_FULL P_RST P_FIN P_GARBAGE P_BANNER <<< "$(find_free_ports 6)"

cleanup() {
    for p in "${PIDS[@]:-}"; do kill "$p" 2>/dev/null || true; done
    for a in "${AGENTS[@]:-}"; do kill "$a" 2>/dev/null || true; done
    sleep 0.3
    for p in "${PIDS[@]:-}"; do kill -9 "$p" 2>/dev/null || true; done
    for a in "${AGENTS[@]:-}"; do kill -9 "$a" 2>/dev/null || true; done
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[保留] 沙箱=%s  进程 PID=%s  agent PID=%s\n' \
            "$SB" "${PIDS[*]:-无}" "${AGENTS[*]:-无}"
        return
    fi
    rm -rf "$SB"
    printf '\n[沙箱已删除] %s\n' "$SB"
    printf '[收尾自检 1] 本脚本起的进程是否都回收了：\n'
    if [ -n "${PIDS[*]:-}" ]; then
        for p in "${PIDS[@]}"; do
            if kill -0 "$p" 2>/dev/null; then printf '  ⚠ PID %s 仍存活\n' "$p"
            else printf '  PID %s 已回收\n' "$p"; fi
        done
    else printf '  （没有起过后台进程）\n'; fi
    printf '[收尾自检 2] 有没有遗留监听端口（本章用过的都应消失）：\n'
    ss -lnt 2>/dev/null \
        | grep -E ":($P_CLOSED|$P_FULL|$P_RST|$P_FIN|$P_GARBAGE|$P_BANNER) " \
        | sed 's/^/  /' || printf '  干净：没有遗留监听\n'
    printf '[收尾自检 3] 有没有本脚本留下的 ssh-agent：\n'
    pgrep -af "ssh-agent.*$SB" | sed 's/^/  /' || printf '  干净：没有遗留 agent\n'
    printf '[收尾自检 4] 顺便确认没有当年那种 2222 僵尸监听：\n'
    ss -lnt 2>/dev/null | grep -E ':(2222|8098|8099|1080) ' | sed 's/^/  /' || printf '  干净\n'
}
trap cleanup EXIT

scene() { printf '\n\n======== 场景 %s ========\n' "$*"; }
say()   { printf '\n--- %s\n' "$*"; }
line()  { printf '%s\n' "$*"; }
run()   { printf '\n$ %s\n' "$*"; bash -c "$*"; }
note()  { printf '\n$ %s\n  （这条按设计会失败，就为了看真实报错原文）\n' "$*"
          bash -c "$*" 2>&1 || printf '  [退出码 %s —— 非 0 是预期的]\n' "$?"; }
pause() { sleep 10; }   # 让本机 sshd 喘气；连太快会被随机 reset，正是场景 9 的主题

startbg() {
    local name=$1; shift
    printf '\n$ %s &        # 后台起【%s】\n' "$*" "$name"
    bash -c "$*" >/dev/null 2>&1 &
    local p=$!
    PIDS+=("$p"); printf '%s\n' "$p" > "$SB/pids/$name"
    printf '  [PID %s 已登记，脚本退出时会自动 kill]\n' "$p"
    sleep 1
}

# 远端用户名用中性名 learner：本章每个连接都按设计失败（refused / 认证不过 / 算法谈崩），
# 用一个本机不存在的用户名和一个真实登录名拿到的报错原文、退出码、结论完全一样，
# 而文档里因此不会印出跑脚本那个人的真实登录名（全书红线「屏幕上不出现真实身份」）。
SSH_TARGET="learner@127.0.0.1"
# 公共护栏里【只放不会被单条命令覆盖的东西】。
# 为什么 -o UserKnownHostsFile / StrictHostKeyChecking / PreferredAuthentications 要每条自己写？
# 因为 ssh 的 -o 和配置文件同规矩：同一个关键字出现多次，【第一次赢】。
# 如果把它们塞进公共前缀，你在后面补的 -o 就全被静默忽略 —— 这是场景 12 的活教材。
S="ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=8 -o IdentityFile=$SB/keys/_no-default-probe"

printf '沙箱：%s\nOpenSSH：%s\n目标：127.0.0.1:22（本机真实 sshd）+ 现搭的假服务器\n' \
    "$SB" "$(ssh -V 2>&1)"
printf '本轮端口：没人听=%s 队列打满=%s 假服务器=%s/%s/%s/%s\n' \
    "$P_CLOSED" "$P_FULL" "$P_RST" "$P_FIN" "$P_GARBAGE" "$P_BANNER"
printf '（本脚本一开始就 unset 了 SSH_AUTH_SOCK，所以用的绝不是你桌面上的 agent）\n'

# ---- 沙箱里的小工具 --------------------------------------------------------
cat > "$SB/fakeserver.py" <<'PY'
#!/usr/bin/env python3
"""假 SSH 服务器：accept 之后只做一件「破坏协议」的事，用来在本机造出各种真实报错。
用法: fakeserver.py MODE PORT
  rst      SO_LINGER=0 关闭，发 RST  -> kex_exchange_identification: read: Connection reset by peer
  fin      先把客户端字节读干净再 close（发 FIN） -> Connection closed by 127.0.0.1 port N
  garbage  先读一次，再发一段 HTTP 垃圾然后关掉（模拟端口打错：后面是 Web 服务）
  banner   发一个看起来合法的版本横幅，再读一次，然后关（模拟老服务器半路断）
"""
import socket, struct, sys, time

mode, port = sys.argv[1], int(sys.argv[2])
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", port)); s.listen(1)
c, peer = s.accept()
c.settimeout(1.5)
try:                                  # 先把客户端发来的横幅吃掉，免得内核替我们发 RST
    c.recv(4096)
except OSError:
    pass
try:
    if mode == "rst":
        c.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        c.close()
    elif mode == "fin":
        c.close()
    elif mode == "garbage":
        c.sendall(b"HTTP/1.1 400 Bad Request\r\n\r\n"); time.sleep(0.4); c.close()
    elif mode == "banner":
        c.sendall(b"SSH-2.0-OpenSSH_5.3 fake\r\n"); time.sleep(0.4)
        try:
            c.recv(4096)
        except OSError:
            pass
        c.close()
finally:
    s.close()
PY

cat > "$SB/fullbacklog.py" <<'PY'
#!/usr/bin/env python3
"""把 accept 队列占满且永不 accept：后来的 connect() 只会静静排队 -> 超时（不是 refused）。
这就是「Connection timed out」和「Connection refused」在内核层面最根本的区别。"""
import socket, sys, time
port = int(sys.argv[1])
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("127.0.0.1", port)); s.listen(0)          # backlog 0 => accept 队列只有 1 格
held = []
for _ in range(8):                                 # 自己先把队列塞满，永不 accept
    t = socket.socket()
    try:
        t.connect(("127.0.0.1", port))
    except OSError:
        break
    held.append(t)
print("accept 队列已被 %d 条连接占满，端口 %s 上新来的连接只会排队" % (len(held), port),
      flush=True)
time.sleep(60)
PY

cat > "$SB/askpass.sh" <<'SH'
#!/usr/bin/env bash
printf '%s\n' 'sandbox-passphrase-1234'
SH
chmod +x "$SB/askpass.sh"

cat > "$SB/bin/fake-ssh" <<'SH'
#!/usr/bin/env bash
# 假 ssh：只把 git 传下来的 argv 原样记进日志，然后失败退出
printf '%s\n' "$@" >> "$GIT_ARGV_LOG"
exit 127
SH
chmod +x "$SB/bin/fake-ssh"

# ---- 沙箱里的一堆 key（各种坏法）------------------------------------------
ssh-keygen -q -t ed25519 -N '' -C plain@sb  -f "$SB/keys/id_plain"
ssh-keygen -q -t ed25519 -N 'sandbox-passphrase-1234' -C locked@sb -f "$SB/keys/id_locked"
cp "$SB/keys/id_plain.pub" "$SB/keys/id_pub_as_priv"; chmod 600 "$SB/keys/id_pub_as_priv"
{
    printf '%s\n' '-----BEGIN RSA PRIVATE KEY-----'
    printf '%s\n' 'QUJDREVGR0hJSktMTU5PUFFSU1RVVldYWVoxMjM0NTY3ODk='
    printf '%s\n' '-----END RSA PRIVATE KEY-----'
} > "$SB/keys/id_badpem"; chmod 600 "$SB/keys/id_badpem"
head -c 200 "$SB/keys/id_plain" > "$SB/keys/id_trunc"
printf '%s\n' '-----END OPENSSH PRIVATE KEY-----' >> "$SB/keys/id_trunc"
chmod 600 "$SB/keys/id_trunc"
: > "$SB/keys/id_empty"; chmod 600 "$SB/keys/id_empty"

# ============================================================================
scene "1：『连不上』的几种说法 —— 每一种各自排除了哪一类原因"
# ============================================================================
say "① 端口没人听：Connection refused（内核直接回 RST）"
note "nc -vz 127.0.0.1 $P_CLOSED"
note "$S -p $P_CLOSED $SSH_TARGET true"
line "  含义：包到了对面机器，但那台机器上【没有进程在听这个端口】。"
line "  => 排查方向：IP/端口写错、sshd 没起、端口不是 22、被 NAT/负载均衡接了。"
line "  => 一定不是「密钥不对」「known_hosts 不对」—— 那些要等 TCP 连上才有资格报。"

say "② 队列打满：Connection timed out（SYN 发出去，石沉大海）"
startbg fullbacklog "python3 $SB/fullbacklog.py $P_FULL"
note "timeout 15 $S -p $P_FULL $SSH_TARGET true"
line "  含义：SYN 重传了几次都没回应，也不是 RST。ConnectTimeout 到点才报出来。"
line "  => 排查方向：防火墙 DROP、对端 IP 根本不通、中间链路黑洞、"
line "     对面 sshd 并发超限后随机丢包（场景 9 的 HAMMER 就是这一类）。"
line "  记法：refused = 到了但没人接；timed out = 没到，或到了被故意不理。"

say "③ 域名都没解析出来：Could not resolve hostname"
note "ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=5 learner@nosuchhost.invalid true"
line "  这连 TCP 都没开始：主机名写错、内网 DNS 不生效、/etc/hosts 没配、VPN 没连。"

say "④ 源地址绑不上（VPN / 多网卡 / 容器里最常见）"
note "$S -b 10.10.10.10 $SSH_TARGET true"
line "  第二行那个 failure 是 ssh 的收尾废话，别看它，看第一行的 strerror。"

say "⑤ 参照组：本机 22 端口是真的有人听"
run "ss -lnt | grep ':22 '"
run "nc -vz 127.0.0.1 22"

# ============================================================================
scene "2：Permission denied (publickey) —— 用 -vv 把三段路走一遍"
# ============================================================================
say "先起一个沙箱 agent 并只放一把 key（这样输出里出现的一定是沙箱指纹）"
eval "$(ssh-agent -s -a "$SB/agent.sock")" >/dev/null
AGENTS+=("$SSH_AGENT_PID")
ssh-add "$SB/keys/id_plain" >/dev/null
run "ssh-add -l"
pause
say "① 不加 -v 你只能看到这一句"
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/real $SSH_TARGET true"
say "② 同一条命令加 -vv，看清它死在第几段："
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/real -vv $SSH_TARGET true 2>&1 | tee $SB/vvv.txt | grep -E 'Connecting to|Connection established|kex: algorithm|kex: host key|Authentications that can continue|Offering public key|No more authentication|Permission denied' | head -20"
line "  这三段就是本章『万能排查顺序』的骨架："
line "    Connecting to / Connection established        <- 第 1 段 TCP。挂了看场景 1。"
line "    kex: algorithm / kex: host key                <- 第 2 段 密钥交换+主机指纹。挂了看场景 5/8/9。"
line "    Authentications that can continue / Offering  <- 第 3 段 认证。挂了看场景 2/3/4/6/7。"
say "③ 第 3 段里『客户端手里有什么牌』全写在这几行："
run "grep -E 'identity file|Will attempt key|Offering public key' $SB/vvv.txt | head -8"
line "  行尾标签的含义：agent = 从 agent 拿的；explicit = 命令行/配置里 -i 指定的；"
line "  什么都没有 = 客户端一张牌都没亮，那『Permission denied (publickey)』就完全合理。"
say "④『服务器还肯接受哪些方式』就这一行 —— 它决定你往哪个方向修"
run "grep -E 'Authentications that can continue' $SB/vvv.txt | head -1"
say "⑤ 一张牌都没有的情形（把 agent 关掉：IdentityAgent=none）"
pause
note "$S -o IdentityAgent=none -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/real -v $SSH_TARGET true 2>&1 | grep -E 'get_agent_identities|Will attempt key|Offering|No more auth|Permission denied' | head -4"
line "  看到没有？连 Offering 都没有一行 -> 这时报错虽然一模一样，但根因是『你没给 key』，"
line "  不是『服务器不认你的 key』。这两种情况的修法完全相反，只有 -vv 能区分。"

say "⑥ 三个隐形杀手（都在服务器侧，本机只能给自查命令 —— 需服务器验证）"
line "  1) StrictModes：服务器嫌弃 ~/.ssh 或 authorized_keys 权限太开"
line "     症状：同一把 key 在别的机器好使、在这台不行；客户端只看到 Permission denied (publickey)。"
line "  2) SELinux/AppArmor：权限数字全对，但安全上下文不对，sshd 读不到那个文件。"
line "     症状：从别的目录 cp 过来的 authorized_keys 失效（或 mv 过来的失效），改内容没用。"
line "  3) AuthorizedKeysFile 被改过：你写的文件根本不是 sshd 在读的文件。"
line "     症状：怎么加 key 都不生效，而且服务器日志里连『尝试过这把 key』都没有。"
line "  自查模板（都在服务器上用 root 跑）："
line "      sshd -T | grep -iE 'authorizedkeysfile|strictmodes'"
line "      namei -l ~youruser/.ssh/authorized_keys    # 逐级看权限，比 ls -l 好用"
line "      grep -i sshd /var/log/auth.log             # RHEL 系是 /var/log/secure"
line "      getenforce; ausearch -m avc -ts recent | grep -i ssh"
line "      /usr/sbin/sshd -D -d -p 2222               # 前台单连接调试；用完必须 kill"

# ============================================================================
scene "3：Permission denied (password) 与『根本不给我输口令的机会』"
# ============================================================================
pause
say "① BatchMode=yes 时绝不提问（脚本/CI 里『为什么它不问我要密码』的答案）"
note "$S -o PreferredAuthentications=password -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/real $SSH_TARGET true"
line "  BatchMode=yes 会把 gssapi-keyex/gssapi-with-mic/batch 之外的交互式方式全掐掉。"
run "ssh -F /dev/null -o BatchMode=yes -G $SSH_TARGET | grep -E '^(preferredauthentications|batchmode) '"
say "② 口令这条路打开、但仍 BatchMode（所以还是直接失败，注意看括号里的内容）"
pause
note "$S -o PubkeyAuthentication=no -o PreferredAuthentications=password,keyboard-interactive -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/real $SSH_TARGET true"
line "  括号里列的是【服务器此刻还肯接受的方式】，不是你能选的方式。"
line "  如果服务器写了 PasswordAuthentication no，你看到的是孤零零一句："
line "      user@example.com: Permission denied (publickey)."
line "  => 括号里只有 publickey：别再试密码了，去修 key/agent（场景 2、4、7）。"
line "  => 括号里有 password 但你就是登不上：先怀疑大小写/输入法/复制带空格，"
line "     再怀疑服务器侧的 PAM 与访问限制（AllowUsers、DenyUsers、Match Address、faillock）。"
line "  => 交互式键盘认证（keyboard-interactive）常常是双因子：它弹两次框很正常，"
line "     第二次才是口令，别在第一次乱填。"
say "③ 客户端侧相关开关的本机默认值"
run "ssh -F /dev/null -G $SSH_TARGET | grep -E '^(pubkeyauthentication|passwordauthentication|kbdinteractiveauthentication|numberofpasswordprompts|authenticationmethods) '"
line "  注意 pubkeyauthentication 打印的是 true，passwordauthentication 打印的是 yes ——"
line "  同一个 -G 里两种写法，是 OpenSSH 自己的历史遗留，别当成两个不同的语义。"

# ============================================================================
scene "4：Too many authentication failures —— agent 里 key 太多"
# ============================================================================
pause
say "往沙箱 agent 里再加 8 把 key（模拟公司/个人/临时项目一共十几把）"
for i in 1 2 3 4 5 6 7 8; do
    ssh-keygen -q -t ed25519 -N '' -C "junk-$i@sandbox" -f "$SB/keys/j$i"
    ssh-add "$SB/keys/j$i" >/dev/null
done
run "ssh-add -l | wc -l"
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/real -v $SSH_TARGET true 2>&1 | grep -E 'Offering public key|Received disconnect|Disconnected' | tail -4"
line "  含义：不是密码错了，是客户端把 agent 里的 key 挨个亮了一遍，"
line "        服务器数到第 6 次（MaxAuthTries 默认 6）就直接掐线。"
line "  本地现象经常是『我明明有对的 key』—— 因为对的那把排在第 7 个，永远轮不到。"
run "grep -E '^#?(MaxAuthTries|MaxStartups|PubkeyAuthentication|PasswordAuthentication)' /etc/ssh/sshd_config || true"
line "  （只读展示：这几行都被注释着，所以用的就是默认值 6 / 10:30:100。）"
say "修法：只递一把"
pause
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/real -o IdentitiesOnly=yes -i $SB/keys/j3 -v $SSH_TARGET true 2>&1 | grep -E 'Offering public key|Received disconnect|Permission denied'"
line "  另一条：把不要的 key 从 agent 里摘掉，ssh-add -d 删一把、ssh-add -D 全清（04 章）。"
say "把 agent 清回一把，后面所有场景都只亮一张牌（也免得再触发限流）"
run "ssh-add -D >/dev/null; ssh-add $SB/keys/id_plain >/dev/null; ssh-add -l"

# ============================================================================
scene "5：WARNING: REMOTE HOST IDENTIFICATION HAS CHANGED!"
# ============================================================================
pause
say "造一个『指纹变了』的现场：known_hosts 里先写一把【假】的 127.0.0.1 指纹，再连真 sshd"
ssh-keygen -q -t ed25519 -N '' -C fake@sb -f "$SB/keys/fake"
printf '127.0.0.1 ssh-ed25519 %s\n' "$(cut -d' ' -f2 "$SB/keys/fake.pub")" > "$SB/kh/changed"
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=yes -o UserKnownHostsFile=$SB/kh/changed $SSH_TARGET true"
say "三种成因怎么分（本章的判断法）"
line "  A) 服务器真的换了（重装/换机/换了 host key）—— 新指纹能从可信渠道核对 -> ssh-keygen -R 后重连"
line "  B) 只是访问方式变了（IP vs 域名、换了端口）—— known_hosts 里本来就是【按主机名分别存】的，"
line "     看着像『变了』，其实那条压根没写过。用 ssh-keygen -F 分别查两种写法就能区分。"
line "  C) 中间人：既不是 A 也不是 B，又无法从可信渠道核对新指纹 —— 停手，别 -R，"
line "     更别用 -o StrictHostKeyChecking=no 把它盖过去。"
say "区分 A/B 的工具：-F 查（哈希过的也能查）"
run "printf '  真指纹那份（场景 2 里 accept-new 自动写进去的）：\n'; ssh-keygen -F 127.0.0.1 -f $SB/kh/real | head -2"
run "printf '  假指纹那份（现在就长这样）：\n'; cat $SB/kh/changed"
say "修法一：删掉旧记录（自动留 .old 备份，方便回滚）"
cp "$SB/kh/changed" "$SB/kh/todelete"
run "ssh-keygen -R 127.0.0.1 -f $SB/kh/todelete"
run "ls $SB/kh"
pause
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/todelete $SSH_TARGET true 2>&1 | head -2"
say "修法二：StrictHostKeyChecking 四个值各自的真实行为（逐条本机真跑）"
line "no —— 指纹完全不看，等于放弃防中间人（CI 图省事会这么写，风险自负）："
pause
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=no -o UserKnownHostsFile=$SB/kh/v_no $SSH_TARGET true 2>&1 | tail -1"
line "yes —— 首次也要人工确认；没有终端时直接死在这里："
pause
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=yes -o UserKnownHostsFile=$SB/kh/v_yes $SSH_TARGET true 2>&1 | head -2"
line "ask —— 默认值：首次弹提示；脚本里没有终端时表现和 yes 一样："
pause
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=ask -o UserKnownHostsFile=$SB/kh/v_ask $SSH_TARGET true 2>&1 | head -2"
line "  ↑ 上面 accept-new 那条已经在『修法一』里演示过：首次自动收下并打印 Warning。"
line "  accept-new vs no 的关键区别：accept-new 只在【从来没见过】时放行，"
line "     见过之后再变照样拦 —— 所以它是 no 和 yes 之间那个正确的折中，也是 CI 的推荐值。"
say "HashKnownHosts：隐私更好，但你的 grep 会当场失效（真实踩坑）"
mkdir -p "$SB/kh/hash"
printf '127.0.0.1 ssh-ed25519 %s\n' "$(cut -d' ' -f2 "$SB/keys/fake.pub")" > "$SB/kh/hash/known_hosts"
chmod 600 "$SB/kh/hash/known_hosts"
run "ssh-keygen -H -f $SB/kh/hash/known_hosts 2>&1 | tail -3"
note "grep -c '127.0.0.1' $SB/kh/hash/known_hosts"
run "printf '  但 ssh-keygen -F 照样找得到：\n'; ssh-keygen -F 127.0.0.1 -f $SB/kh/hash/known_hosts | head -2"
line "  => CI 脚本里想用 grep 判断『这台主机是否已知』，在哈希过的 known_hosts 上永远返回 0，"
line "     于是每次都白装一遍 key。能用的工具只有 ssh-keygen -F。"

# ============================================================================
scene "6：私钥文件本身的错（权限 / 格式 / 口令）"
# ============================================================================
say "① 权限太开：ssh-add 和 ssh-keygen 会吼，ssh 客户端【一声不吭】"
chmod 644 "$SB/keys/id_plain"
note "ssh-add $SB/keys/id_plain"
note "ssh-keygen -y -f $SB/keys/id_plain"
pause
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/real -o IdentityAgent=none -o IdentitiesOnly=yes -i $SB/keys/id_plain -v $SSH_TARGET true 2>&1 | grep -E 'Offering public key|too open|Load key|Permission denied'"
chmod 600 "$SB/keys/id_plain"
line "  ↑ 第三条（用 IdentityAgent=none 强制它只能读磁盘，好把话说清楚）里 ssh 连一句抱怨都没有，"
line "     它只是【不再用这把 key】。所以『我的 key 明明就在这里，为什么没被递出去』的"
line "     第一反应应该是 ls -l 看权限。（如果这把 key 恰好也在 agent 里，它照样会被递出去，"
line "     因为用的是 agent 内存里的那份，跟文件权限无关 —— 这就是『有时好有时坏』的来源。）"
line "     服务端反过来很吵：Bad owner or permissions on /home/user/.ssh/authorized_keys"
line "     （这句在 sshd 日志/调试输出里，需服务器验证；它就是场景 2 的杀手 1。）"
line "     服务端反过来很吵：Bad owner or permissions on /home/user/.ssh/authorized_keys"
line "     （这句在 sshd 日志/调试输出里，需服务器验证；它就是场景 2 的杀手 1。）"
say "② 权限根本读不了（chmod 000）：-vv 里是 Load key ...: Permission denied"
cp "$SB/keys/id_plain" "$SB/keys/id_unreadable"; chmod 000 "$SB/keys/id_unreadable"
note "ssh-keygen -y -f $SB/keys/id_unreadable"
say "③ invalid format vs error in libcrypto —— 一句话分清"
note "ssh-keygen -y -f $SB/keys/id_pub_as_priv"
note "ssh-keygen -y -f $SB/keys/id_badpem"
note "ssh-keygen -y -f $SB/keys/id_trunc"
note "ssh-keygen -y -f $SB/keys/id_empty"
line "  分法：PEM 头在、身体按那个类型解析不出来 -> invalid format（常见：拿公钥/证书当私钥，"
line "        或者格式转换工具把文件转坏了）；"
line "        OpenSSH 自有格式被截断/空文件/口令块坏 -> error in libcrypto。"
line "  本机实测：把【公钥】当私钥喂进去也是 error in libcrypto（它按 OpenSSH 格式解析失败）。"
line "  两条万能自救："
line "    ssh-keygen -y -f 你的私钥            # 能打出公钥就说明文件本身是好的，问题在别处"
line "    ssh-keygen -p -m PEM -f 你的私钥     # 给只认老格式的程序（部分 PHP/JS 库、老 Paramiko）用"
say "④ 口令不对 / 被问口令却没有终端："
note "ssh-keygen -y -f $SB/keys/id_locked"
note "env SSH_ASKPASS_REQUIRE=never ssh-keygen -y -f $SB/keys/id_locked"
line "  顺带：改口令只动私钥，公钥和服务器上的 authorized_keys 一个字都不用改："
line "    ssh-keygen -p -f ~/.ssh/id_ed25519"
say "⑤ 这些字符串在本机 OpenSSH 10.2 客户端里到底存不存在（免得抄到过期报错）"
run "for s in 'Bad owner or permissions on' 'invalid format' 'error in libcrypto' 'agent refused operation' 'unsupported cache option' 'sign_and_send_pubkey' 'kex_exchange_identification' 'ssh_exchange_identification' 'Connection closed by remote host'; do
  n=\$(strings /usr/bin/ssh | grep -c \"\$s\")
  printf '  %-38s 二进制里命中 %s 次\n' \"\$s\" \"\$n\"
done"
line "  => 『unsupported cache option』命中 0：那是 macOS keychain / 老版本相关的报错，"
line "     这台 Ubuntu 的 10.2 客户端不会给你这句。查资料第一件事是对版本（ssh -V）。"
line "  => 『ssh_exchange_identification』命中 0：这个旧前缀在 10.2 客户端里【根本没有】，"
line "     现在给你的是 kex_exchange_identification（含义相同，见场景 9）。"
line "     在网上抄到带 ssh_exchange_identification 的帖子，先对一下人家那台客户端的版本。"
say "⑥ 『Bad owner or permissions』会不会是客户端报的？本机实测：不会"
note "printf 'Host *\n  ServerAliveInterval 42\n' > $SB/cfg/loose; chmod 666 $SB/cfg/loose; ssh -F $SB/cfg/loose -G $SSH_TARGET 2>/dev/null | grep -c serveraliveinterval"
pause
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/loose $SSH_TARGET true"
line "  ↑ 世界可写的 -F 配置照样生效（返回 1），世界可写的 known_hosts 也照样能连 —— 客户端不管。"
line "     结论：这句基本来自【服务端】（sshd 检查 authorized_keys/私钥路径），"
line "     或者是 macOS/老版本客户端。别在自己的客户端配置里死找。"

# ============================================================================
scene "7：agent 类报错：『我加了 key 为什么还要口令』与 agent refused operation"
# ============================================================================
say "① 排查三件套（顺序不能换）"
run "echo \"第一步 门牌号：SSH_AUTH_SOCK=\${SSH_AUTH_SOCK:-（空）}\""
run "ssh-add -l | head -2; printf '  （沙箱 agent 里现在有 %s 把）\n' \"\$(ssh-add -l | wc -l)\""
line "  第二步看上面这条的结果，四种原文对应四种走法："
line "    Could not open a connection to your authentication agent.  -> 门牌号是空的：agent 没起 / 环境变量没传过来"
line "    Error connecting to agent: No such file or directory       -> 门牌号有，但 socket 文件没了（重启过 / 换机器 / 进容器）"
line "    Error connecting to agent: Connection refused              -> socket 文件在，但进程早死了（残留）"
line "    The agent has no identities.                               -> agent 好好的，只是里面是空的：ssh-add 你的 key"
run "printf '  三条前两种本机真跑（04 章也有）：\n'"
note "env -u SSH_AUTH_SOCK ssh-add -l"
note "env SSH_AUTH_SOCK=$SB/nope.sock ssh-add -l"
printf 'x\n' > "$SB/notasocket"
note "env SSH_AUTH_SOCK=$SB/notasocket ssh-add -l"
say "② 第三步：凡是穿过 sudo / su / docker exec / systemd / cron 的，环境都会被重做一遍"
note "env -i PATH=\$PATH HOME=\$HOME bash -c 'echo \"  干净子进程里的 SSH_AUTH_SOCK=[\${SSH_AUTH_SOCK:-空}]\"; ssh-add -l'"
line "  （本机 sudo 需要交互认证，跑不动，所以用 env -i 演同一件事。能 sudo 的机器自己对比："
line "     ssh-add -l / sudo ssh-add -l / sudo -E ssh-add -l —— 这是『sudo 把 agent 洗掉』的铁证。）"
say "③ agent 上锁之后：客户端 debug 里那句 agent contains no identities"
run "SSH_ASKPASS=$SB/askpass.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0 ssh-add -x"
pause
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/real -v $SSH_TARGET true 2>&1 | grep -E 'get_agent_identities|Offering|No more auth'"
run "SSH_ASKPASS=$SB/askpass.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0 ssh-add -X >/dev/null; printf '  解锁了：\n'; ssh-add -l"
say "④ sign_and_send_pubkey: signing failed for ...: agent refused operation"
line "  ⚠ 这句必须【服务器接受了你的公钥】才会出现：签名发生在 Offer 之后。"
line "     本机不允许装 authorized_keys（不可能登录成功），所以拿不到，需服务器验证："
line "       ssh -vv -o IdentitiesOnly=yes -i ~/.ssh/id_ed25519 用户@服务器 true 2>&1 \\"
line "         | grep -E 'Server accepts key|sign_and_send_pubkey|agent refused'"
line "  常见成因（按概率排）：agent 是别的用户/别的容器的（你连的根本不是同一个 socket）；"
line "     agent 被 ssh-add -x 锁了；confirm 模式（ssh-add -c）下你在另一个终端按了拒绝；"
line "     gpg-agent 接管了 SSH_AUTH_SOCK 但那把 key 不在智能卡里；ssh-add -t 设的寿命到点了。"
line "  两步自救：ssh-add -l 看到 key 吗 -> 看到就 ssh-add -D 再干净地重新 ssh-add（不带 -c/-t）。"

# ============================================================================
scene "8：算法谈不拢：no matching key exchange method / host key type / cipher"
# ============================================================================
say "先看本机客户端到底支持什么（ssh -Q 是唯一权威，别抄网上的清单）"
run "for q in kex key sig cipher mac; do printf '  %-6s 本机支持 %3s 种，前 3 个：%s\n' \"\$q\" \"\$(ssh -Q \$q | wc -l)\" \"\$(ssh -Q \$q | head -3 | tr '\n' ' ')\"; done"
run "printf '  顺手确认几个『传说中有』的到底有没有：\n'; for a in cast128-cbc 3des-cbc hmac-md5 ssh-dss ssh-rsa; do printf '    %-12s : cipher=%s mac=%s key=%s\n' \"\$a\" \"\$(ssh -Q cipher | grep -cx \"\$a\")\" \"\$(ssh -Q mac | grep -cx \"\$a\")\" \"\$(ssh -Q key | grep -cx \"\$a\")\"; done"
say "① 客户端压根不支持的算法，连门都出不了（本机真跑）"
note "$S -o Ciphers=cast128-cbc -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/real $SSH_TARGET true 2>&1 | head -1"
line "  『Bad SSH2 cipher spec』是【本地解析失败】，跟服务器一点关系都没有；"
line "  看到这句就回去 ssh -Q，别去改服务器。"
say "② 客户端支持、但本机这台新 sshd 不肯用 -> 真正的协商失败（真实原文）"
pause
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/real -o KexAlgorithms=diffie-hellman-group1-sha1 $SSH_TARGET true 2>&1 | head -1"
pause
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=no -o UserKnownHostsFile=$SB/kh/neg -o HostKeyAlgorithms=ssh-rsa $SSH_TARGET true 2>&1 | head -1"
pause
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=no -o UserKnownHostsFile=$SB/kh/neg -o Ciphers=3des-cbc $SSH_TARGET true 2>&1 | head -1"
line "  三句同一个模板：Unable to negotiate with <IP> port <N>: no matching XXX found."
line "  Their offer: ...  <- 这一串是【对方】支持的清单，比任何攻略都权威：照它挑一个就行。"
line "  注意 HostKeyAlgorithms 那次的报错还可能长成另一句："
line "     It is also possible that old OpenSSH versions (< 7.0) are not supported."
say "③ 反例：MACs=hmac-md5 在这台机器上【居然谈成了】—— 别把攻略当定律"
pause
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=no -o UserKnownHostsFile=$SB/kh/neg -o MACs=hmac-md5 $SSH_TARGET true 2>&1 | tail -1"
line "  同样的降级配置，换一台服务器结论就反过来。永远以 Their offer 为准。"
say "④ = 和 + 的区别：= 是【整表替换】，+ 是【追加】"
run "printf '  默认 kex 列表（截断）：\n'; ssh -F /dev/null -G $SSH_TARGET 2>/dev/null | grep '^kexalgorithms' | cut -c1-100"
run "printf '  = 覆盖成只剩一个：\n'; ssh -F /dev/null -o KexAlgorithms=diffie-hellman-group14-sha1 -G $SSH_TARGET 2>/dev/null | grep '^kexalgorithms'"
run "printf '  + 追加（默认整表都留着）：\n'; ssh -F /dev/null -o 'KexAlgorithms=+diffie-hellman-group14-sha1' -G $SSH_TARGET 2>/dev/null | grep '^kexalgorithms' | cut -c1-100"
pause
run "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=no -o UserKnownHostsFile=$SB/kh/neg -o 'KexAlgorithms=+diffie-hellman-group14-sha1' $SSH_TARGET true 2>&1 | tail -1 || true"
line "  ↑ 同一条配置：用 = 死在 kex（见 ②），用 + 顺利走到认证并报 Permission denied。"
line "     这就是网上『加个 + 就好了』的老方子有时管用、有时更糟的原因。"
line "  连老服务器的正解顺序：ssh -Q 看自己有什么 -> 只把对方支持的用 = 顶上 ->"
line "     并且一定写在 Host 老IP 作用域里，别让全局降级传染到你的新机器。"
line "  新客户端连老服务器的另一个坑：host key 是 ssh-rsa 时，"
line "     HostKeyAlgorithms=+ssh-rsa 和 PubkeyAcceptedAlgorithms=+ssh-rsa 两个都要加，漏一个照样死。"

# ============================================================================
scene "9：半路断线：kex_exchange_identification / Connection closed / reset"
# ============================================================================
say "以下四种都用手搭的假服务器造（127.0.0.1 上独立端口，完全不碰 sshd）"
for m in rst fin garbage banner; do
    case $m in
        rst)     port=$P_RST ;;
        fin)     port=$P_FIN ;;
        garbage) port=$P_GARBAGE ;;
        banner)  port=$P_BANNER ;;
    esac
    startbg "fake_$m" "python3 $SB/fakeserver.py $m $port"
    note "ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=6 -o IdentityFile=$SB/keys/_no-default-probe -o StrictHostKeyChecking=no -o UserKnownHostsFile=$SB/kh/fake -p $port $SSH_TARGET true"
done
line "  怎么读这几句："
line "    kex_exchange_identification: read: Connection reset by peer"
line "        对面发了 RST。防火墙 REJECT、端口后面根本不是 sshd、fail2ban 踢你、"
line "        或者 sshd 并发超限随机丢，都会长这样。"
line "    Connection closed by 127.0.0.1 port N（对面发 FIN）"
line "        版本横幅还没换完就正常关了：多半是 sshd 不想理你（版本、限速、探针式关闭）。"
line "    旧客户端会写成 ssh_exchange_identification: read: Connection reset by peer"
line "        同一件事，只是 8.x 及更早的前缀。看到旧前缀说明你那台客户端也老了，先 ssh -V 记版本。"
line "    完全没有 kex/identification 字样，只有 connect to host ... 一行"
line "        那是 TCP 阶段的失败，回到场景 1，不要在这里查密钥。"
line "  ⚠ 本机拿不到的两句（需服务器验证）："
line "     kex_exchange_identification: Connection closed by remote host"
line "        字符串在二进制里（上面场景 6⑤ 已证明），但 10.2 在这条路上给你的是"
line "        『Connection closed by <IP> port N』这句更具体的形式。含义完全一样。"
line "     Timeout, server not responding."
line "        只出现在【已登录成功、会话进行中】：ServerAliveInterval 连续 N 次没回应。"
line "        自查模板： ssh -o ServerAliveInterval=15 -o ServerAliveCountMax=4 用户@服务器 'sleep 5; echo ok'"
say "但相关的默认值本机可查（这一半能验证）："
run "ssh -F /dev/null -G $SSH_TARGET | grep -E '^(serveraliveinterval|serveralivecountmax|tcpkeepalive|connectiontimeout) '"
say "并发打满被随机丢包 —— 作者写这本手册时真的踩过：连太快就收到本场景那两句"
run "grep -E '^#?MaxStartups' /etc/ssh/sshd_config || true"
if [ "$HAMMER" -eq 1 ]; then
    say "HAMMER=1：同时开 14 条连接，看有没有被随机拒绝"
    : > "$SB/hammer.log"
    for i in $(seq 1 14); do
        ( ssh -F /dev/null -o BatchMode=yes -o ConnectTimeout=6 \
              -o IdentityFile="$SB/keys/_no-default-probe" \
              -o PreferredAuthentications=publickey -o StrictHostKeyChecking=no \
              -o UserKnownHostsFile="$SB/kh/real" "$SSH_TARGET" true 2>&1 | tail -1 \
              >> "$SB/hammer.log" ) &
    done
    wait
    run "sort $SB/hammer.log | uniq -c | sort -rn"
    line "  如果这里出现 reset/closed，而单条连接却又正常：那就是 MaxStartups 生效，属于服务器自我保护。"
    line "  修法不是写重试循环，而是降并发：上连接池（04 章 ControlMaster），或者让脚本串行。"
else
    line "  （想亲眼看这一场：HAMMER=1 bash demo.sh。默认不跑，免得把本机 sshd 打得短期拒连。）"
fi

# ============================================================================
scene "10：Warning: Permanently added ... —— 这根本不是错误"
# ============================================================================
pause
run "rm -f $SB/kh/fresh; $S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/fresh $SSH_TARGET true 2>&1 | head -1 || true"
line "  含义：首次见这台服务器，指纹已经记下了。它是 WARNING 级别，连接照常继续。"
line "  想让日志干净：降日志级别，而不是关掉主机校验。"
note "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o LogLevel=ERROR -o UserKnownHostsFile=$SB/kh/quiet $SSH_TARGET true 2>&1 | head -2"
run "printf '  quiet 那份 known_hosts 里确实写进去了（校验照样做了）：\n'; head -c 120 $SB/kh/quiet; echo ' ...'"
line "  同一类『看着像报错其实不是』的还有："
line "     Pseudo-terminal will not be allocated because stdin is not a terminal.   （没终端却要不要 pty）"
line "     Received disconnect from ... 2: Too many authentication failures          （那是真报错，见场景 4）"
line "     no such identity: /path  : No such file or directory                      （-i 指向了不存在的文件）"

# ============================================================================
scene "11：git 报的 Permission denied (publickey) —— 多半 git 用的不是你那个 ssh"
# ============================================================================
pause
say "① 先问：git 调的是哪个 ssh？（PATH 里可能不止一个）"
run "which -a ssh; printf '  git 版本：%s\n' \"\$(git --version)\""
say "② 真跑一次 git（本机 sshd 照样把认证挡回去，这就是你熟悉的那两句）"
note "GIT_SSH_COMMAND='$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/real' GIT_TERMINAL_PROMPT=0 git ls-remote git@127.0.0.1:/nope.git 2>&1 | head -4"
line "  git 后面那四行提示（Please make sure you have the correct access rights…）"
line "  其实一点信息都没给。真正的线索永远在它调用的那条 ssh 上："
line "     GIT_SSH_COMMAND='ssh -vv' git ls-remote ...     # 把 -vv 塞给 git 用的 ssh，场景 2 立刻能用"
say "③ 用假 ssh 把 git 传下去的 argv 打出来（理解 GIT_SSH_COMMAND 的最短路径）"
run "cd $SB/repo && git init -q . 2>/dev/null || true
GIT_ARGV_LOG=$SB/git_argv.log GIT_SSH_COMMAND=$SB/bin/fake-ssh git ls-remote git@127.0.0.1:nope.git 2>&1 | head -2
printf '  git 实际执行的 ssh 参数（一行一个 argv）：\n'; sed 's/^/    /' $SB/git_argv.log"
line "  => 看到 git-upload-pack '...' 那一段：git 是把子命令当成【远端要执行的文件】交给 ssh 的。"
line "     所以 GIT_SSH_COMMAND 里只能放 ssh 自己的选项；要放 shell 逻辑必须包一层脚本。"
say "④ 一个真实坑：GIT_SSH_COMMAND 会让 git 用『simple』ssh 变体，于是 ssh:// 带端口就不干活"
note "GIT_ARGV_LOG=$SB/g0.log GIT_SSH_COMMAND=$SB/bin/fake-ssh git ls-remote ssh://git@127.0.0.1:22/nope.git 2>&1 | head -2"
line "  ↑ 这句『致命错误：ssh 变体 simple 不支持设置端口』和 SSH 没关系，是 git 自己拒绝的。"
line "     修法（任选其一）：改用 scp 式地址 git@host:path（端口靠 ssh 配置文件里 Host 段指定），"
line "     或者显式告诉 git 这是个真 ssh：GIT_SSH_VARIANT=ssh。"
pause
note "GIT_SSH_VARIANT=ssh GIT_SSH_COMMAND='$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=no -o UserKnownHostsFile=$SB/kh/git' git ls-remote ssh://git@127.0.0.1:22/nope.git 2>&1 | head -3"
say "⑤ 三种指定方式（都让假 ssh 把 argv 打出来，优先级从高到低）"
run "cd $SB/repo
printf '  a) GIT_SSH_COMMAND（临时，优先级最高，推荐）：\n'
GIT_ARGV_LOG=$SB/g1.log GIT_SSH_COMMAND=\"$SB/bin/fake-ssh -o BatchMode=yes\" git ls-remote git@127.0.0.1:x.git 2>&1 | head -1
printf '     实际传参：%s\n' \"\$(head -3 $SB/g1.log | tr '\n' ' ')\"
printf '  b) core.sshCommand（仓库长期，优先级低于环境变量）：\n'
git config core.sshCommand \"$SB/bin/fake-ssh -F /dev/null -o BatchMode=yes\"
GIT_ARGV_LOG=$SB/g2.log git ls-remote git@127.0.0.1:x.git 2>&1 | head -1
printf '     仓库里存的：%s\n' \"\$(git config core.sshCommand)\"
printf '     实际传参：%s\n' \"\$(head -3 $SB/g2.log | tr '\n' ' ')\"
printf '     再套一层环境变量（变量赢）：\n'
GIT_ARGV_LOG=$SB/g2b.log GIT_SSH_COMMAND=\"$SB/bin/fake-ssh -o Visual=yes\" git ls-remote git@127.0.0.1:x.git 2>&1 | head -1
printf '     实际传参：%s\n' \"\$(head -3 $SB/g2b.log | tr '\n' ' ')\"
printf '  c) GIT_SSH（老变量：只认一个可执行文件路径，不能带参数）：\n'
GIT_ARGV_LOG=$SB/g3.log GIT_SSH=$SB/bin/fake-ssh git ls-remote git@127.0.0.1:y.git 2>&1 | head -1
printf '     实际传参：%s\n' \"\$(head -3 $SB/g3.log | tr '\n' ' ')\""
say "⑤ 一张对症表（README 有完整版）"
line "  GitHub 好使、ssh -T git@github.com 也好使，只有某个仓库不行 -> core.sshCommand / 部署 key"
line "  WSL / Git Bash / IDE 内置终端不行，普通终端行                  -> which -a ssh 看到两个不同的 ssh"
line "  cron / CI runner 里不行，手动行                               -> 环境变量与 HOME/agent 没跟过去（04 章）"
line "  报错里永远是 git@主机 而不是你的登录名                        -> 那是【远端用户】，正常；"
line "     端口非 22 时用 ssh://git@主机:端口/path 写全，scp 式简写会骗人"

# ============================================================================
scene "12：原因其实在客户端配置：first-match-wins、Include、两个不存在的环境变量"
# ============================================================================
say "① 同一个 Host 模式写了两遍：只有第一次生效（-G 是唯一权威）"
cat > "$SB/cfg/dup" <<'EOF'
Host dup
    HostName 10.0.0.1
    Port 2200
    User first-wins
Host dup
    HostName 10.0.0.2
    Port 2222
    User second-ignored
EOF
run "ssh -F $SB/cfg/dup -G dup 2>/dev/null | grep -E '^(hostname|port|user) '"
say "② 命令行 -o 也守同一条规矩：重复的关键字【第一次赢】"
run "printf '  先 no 后 yes -> 生效的是哪一个：\n'; ssh -F /dev/null -o StrictHostKeyChecking=no -o StrictHostKeyChecking=yes -G $SSH_TARGET 2>/dev/null | grep -i '^stricthostkeychecking'"
run "printf '  先 yes 后 no -> 生效的是哪一个：\n'; ssh -F /dev/null -o StrictHostKeyChecking=yes -o StrictHostKeyChecking=no -G $SSH_TARGET 2>/dev/null | grep -i '^stricthostkeychecking'"
line "  => 把默认护栏写在一个变量里、然后在后面『覆盖』，是这类脚本最常见的静默失效原因"
line "     （本手册的脚本因此把可能被覆盖的选项都写在每条命令自己身上）"
say "③ 通配 * 写在前面会吃掉后面所有具体 Host（反直觉但正确）"
cat > "$SB/cfg/star" <<'EOF'
Host *
    User eaten-by-star
Host myserver
    User never-mind
    Port 2205
EOF
run "ssh -F $SB/cfg/star -G myserver 2>/dev/null | grep -E '^(user|port) '"
line "  => * 块里只放『所有主机都需要、而且不会被覆盖』的东西；例外要写在 * 之前。"
say "④ Include 的位置决定谁赢"
cat > "$SB/cfg/d/10-base" <<'EOF'
Host inc
    Port 1000
    User from-include
EOF
cat > "$SB/cfg/d/20-override" <<'EOF'
Host inc
    Port 2000
EOF
cat > "$SB/cfg/inc" <<EOF
Include $SB/cfg/d/*
Host inc
    Port 3000
EOF
run "ssh -F $SB/cfg/inc -G inc 2>/dev/null | grep -E '^(port|user) '"
line "  ↑ Include 在最前 => 10-base 赢（port 1000）；20-override 和主文件自己的 port 3000 都被忽略。"
cat > "$SB/cfg/inc2" <<EOF
Host inc
    Port 3000
Include $SB/cfg/d/*
EOF
run "printf '  把 Include 挪到最后：\n'; ssh -F $SB/cfg/inc2 -G inc 2>/dev/null | grep -E '^(port|user) '"
line "  => 想把发行版/系统级配置当兜底，就把 Include 放在自己文件的最下面。"
say "⑤ 传说中存在的环境变量 SSH_OPTS / SSH_CONFIG_FILE —— 根本不存在（本机实测）"
run "for v in SSH_CONFIG_FILE SSH_OPTIONS SSH_OPTS SSH_CONFIG; do
  printf '  作为独立字符串出现在二进制里 %-18s : %s 次\n' \"\$v\" \"\$(strings /usr/bin/ssh | grep -xc \"\$v\")\"
done"
run "printf '  设了 SSH_CONFIG_FILE 也没用 —— 下面这条生效的仍是 -F 指定的那个文件（user=eaten-by-star）：\n'
SSH_CONFIG_FILE=$SB/cfg/dup ssh -F $SB/cfg/star -G myserver 2>/dev/null | grep -E '^(hostname|user) '"
line "  => 没有任何变量能指定配置文件。能换文件的只有 -F。"
run "printf '  ssh 客户端真正会读的环境变量（本机 strings 抓的，权威）：\n'; strings /usr/bin/ssh | grep -E '^SSH_[A-Z_]+\$' | sort -u | tr '\n' ' '; echo"
say "⑥ 那换个 HOME 能换掉默认 key 吗？本机实测：不能"
cat > "$SB/cfg/tilde" <<'EOF'
IdentityFile ~/.ssh/__probe_does_not_exist__
EOF
pause
run "printf '  正常 HOME 下 ssh 找的是：\n'; ssh -F $SB/cfg/tilde -o BatchMode=yes -o ConnectTimeout=8 -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/t1 $SSH_TARGET true 2>&1 | grep 'no such identity' || true"
run "printf '  HOME 换成沙箱 fakehome 之后：\n'; env HOME=$SB/fakehome ssh -F $SB/cfg/tilde -o BatchMode=yes -o ConnectTimeout=8 -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/t2 $SSH_TARGET true 2>&1 | grep 'no such identity' || true"
line "  ↑ 两次展开出来的路径【一模一样】：config 里的 ~ 按密码本(passwd)里的家目录展开，不看 \$HOME。"
line "     所以『给 cron/容器设个 HOME 就换了一套 key』这个方子在这台 OpenSSH 上无效。"
line "     有效做法只有两条：每条命令 -F 配置文件；IdentityFile 写绝对路径。"

# ============================================================================
scene "13：万能排查顺序 —— 每步都真跑，并写明它排除了哪一类"
# ============================================================================
line "第 0 步 先确认你到底连的是谁（九成怪案死在这里；-G 把最终生效值全打出来）："
run "ssh -F $SB/cfg/dup -G dup 2>/dev/null | grep -E '^(hostname|port|user|identityfile|controlmaster|forwardagent) '"
line "第 1 步 机器在不在：ping。很多服务器禁 ICMP，所以它只能当第一步，不能当结论。"
note "ping -c 1 -W 1 127.0.0.1 2>&1 | tail -2"
line "第 2 步 端口有没有人听：nc -vz（一句话区分 refused 和 无响应）"
run "nc -vz 127.0.0.1 22 2>&1 | tail -1"
note "nc -vz 127.0.0.1 $P_CLOSED 2>&1 | tail -1"
line "  => refused 归场景 1①；一直不返回归场景 1②（防火墙/黑洞/并发超限）。"
line "  => 这一同时排除了『密钥、指纹、算法』三类：端口都没通，那些还轮不到。"
line "第 3 步 服务端确实在跑 sshd（本机可查；服务器上要 root）"
run "ss -lnt | grep ':22 '; printf '  sshd 进程：%s\n' \"\$(pgrep -a sshd | head -2 | tr '\n' '; ')\""
line "  在有 systemd 的服务器上自己跑（本手册不动系统服务，这里只给模板）："
line "    systemctl is-active ssh       # Debian/Ubuntu 叫 ssh，RHEL 系叫 sshd"
line "    ss -lnt | grep -E ':(22|2222) '"
line "第 4 步 ssh -vvv 三段定位（读法见场景 2）："
pause
run "$S -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh/real -vv $SSH_TARGET true 2>&1 | grep -nE 'Connecting to|Connection established|kex: algorithm|kex: host key|Authentications that can continue|Offering public key|Permission denied' | head -8 || true"
line "  停在 Connection established 之前   -> 场景 1（网络/端口）"
line "  停在 kex 两行之前                 -> 场景 5（指纹）/ 8（算法）/ 9（半路断）"
line "  停在 Offering/Permission denied   -> 场景 2/3/4/6/7（key、agent、权限）"
line "  一路顺利却没有你要的效果           -> 场景 12（配置没生效）或 05 章（转发/服务端开关）"
line "第 5 步 服务器侧（需 root，需服务器验证）："
line "    journalctl -u sshd -n 50 --no-pager   # 或 /var/log/auth.log、/var/log/secure"
line "    /usr/sbin/sshd -D -d -p 2222          # 前台单连接调试，服务器侧看到的一切都会打出来"
line "    sshd -T | grep -iE 'authorizedkeysfile|permitrootlogin|passwordauth|pubkeyauth|maxauthtries|allowtcpforward'"
line "    用完记得 kill 掉临时 sshd，别留监听。"
line "第 6 步 还查不出来就换客户端/换机器：ssh -V 记版本，同一份 config 在别的机器试一次 —— "
line "     这一步能区分『配置写错了』和『这台机器的 OpenSSH 太老/太新，不支持某个算法』。"
run "printf '  本机客户端版本：%s\n' \"\$(ssh -V 2>&1)\""

# ============================================================================
scene "14：README 附录 A/B/C 的补跑取证 —— 级别阶梯 / Include 权限 / ask 没有终端时"
# ============================================================================
# 这三件以前是「README 另外单独跑一次」的孤证（读者拿到命令也复现不了那次的输出）。
# 现在全部并进 demo：跑一次本脚本就能拿到下面每一行原文。
# 边界照旧：只连 127.0.0.1、每条 ssh 带 -F /dev/null 或沙箱 config、沙箱 known_hosts、
# 一条不存在的 IdentityFile、只允许 publickey —— 不可能登录成功、不可能弹出口令提示。
say "① 附录 A：-v / -vv / -vvv 的行数阶梯（同一目标、同一套护栏，只差 -v 的个数）"
line "   （加 IdentityAgent=none 是为了让行数可比：不然每次会多出若干条 agent 里那张牌的尝试行）"
for lvl in v vv vvv; do
    pause
    printf '\n$ ssh -%s -o IdentityAgent=none … learner@127.0.0.1 true > 沙箱里的日志（屏幕只数行数）\n' "$lvl"
    bash -c "$S -o IdentityAgent=none -o PreferredAuthentications=publickey \
             -o StrictHostKeyChecking=accept-new \
             -o UserKnownHostsFile=$SB/kh/ladder -$lvl $SSH_TARGET true" \
        > "$SB/log_$lvl" 2>&1 || true
    awk -v tag="-$lvl" '{ n++ }
        /^debug1:/ { a++ } /^debug2:/ { b++ } /^debug3:/ { c++ }
        END { printf "  ssh %-5s 总行数=%3d  debug1=%3d  debug2=%3d  debug3=%3d\n",
                     tag, n, a + 0, b + 0, c + 0 }' "$SB/log_$lvl"
done
line "  ↑ 对照结论：-v 十几行就够定「死在第几段」；-vv 才给得出整张算法表；-vvv 才看得见等哪个包。"

say "② 每级各自【独有】的东西（这三段就是实战 2/8/9 说的那三个倍数）"
run "printf '  -v 的尾部（三段路的判决就藏在这几行里）：\n'; grep -E '^(debug1: |learner@|ssh: |Connection)' $SB/log_v | tail -7"
run "printf '  -vv 独有的 debug2：双方完整提案表（实战 8 为什么非 -vv 不可）：\n'; grep -E '^debug2: (local client KEXINIT proposal|KEX algorithms|host key algorithms|ciphers ctos|MACs ctos)' $SB/log_vv | head -5"
run "printf '  -vv 独有：服务器的完整提案（对照上面那份才知道差在哪个算法）：\n'; grep -E '^debug2: (server proposal|KEX algorithms|host key algorithms|ciphers ctos)' $SB/log_vv | tail -4"
run "printf '  -vvv 独有的 debug3：报文级 / 文件级 / 计时级：\n'; grep -E '^debug3: ' $SB/log_vvv | head -10"
run "printf '  -vvv 会把【你的完整命令行】原样打出来（贴网上之前先看这行泄露了什么）：\n'; grep -E '^debug3: Started with' $SB/log_vvv"
run "printf '  ConnectTimeout 是整个连接阶段的预算、不是每次 read 单独计时的证据：\n'; grep -E '^debug3: timeout:' $SB/log_vvv"
run "printf '  卡在等哪一个包，只有 -vvv 能回答（type 20 = KEX 刚完成、正要进认证的那道交界）：\n'; grep -E '^debug3: (send|receive) packet' $SB/log_vvv | head -6"

say "③ 附录 B：Bad owner or permissions 在客户端侧【能】复现 —— 条件是那个文件被 Include 进来（这四条都不联网）"
printf 'Include %s/cfg/d/10-x\n' "$SB" > "$SB/cfg/withinc"
printf 'Host *\n  ServerAliveInterval 42\n' > "$SB/cfg/plain666"
printf 'Host *\n  ServerAliveInterval 99\n' > "$SB/cfg/d/10-x"
chmod 666 "$SB/cfg/d/10-x" "$SB/cfg/plain666"
note "ssh -F $SB/cfg/withinc -G inc          # A1：被 Include 的文件 666"
run "printf '  A2：只把被 Include 的那个文件改回 600，同一条命令立刻正常（值是被包含文件里的 99）：\n'; chmod 600 $SB/cfg/d/10-x; ssh -F $SB/cfg/withinc -G inc 2>/dev/null | grep -E '^serveraliveinterval '"
run "printf '  A3：对照组 —— 顶层 -F 文件自己 666，客户端【不查】它，值照样生效：\n'; ssh -F $SB/cfg/plain666 -G inc 2>/dev/null | grep -E '^serveraliveinterval '"
run "printf '  D：known_hosts 权限太开会不会被客户端嫌弃？数一下 ssh 二进制里这句报错出现几次：\n'; strings /usr/bin/ssh | grep -ic 'bad owner or permissions' || true"
run "printf '  E：二进制里的原文格式串（%s 就是那个被嫌弃的文件）：\n'; strings /usr/bin/ssh | grep -E 'Bad owner or permissions' | head -2"
line "  ↑ 四条结论：① 顶层 -F 文件不查权限；② 被 Include 的文件查，而且一查就是本地当场退出 255、"
line "     serveraliveinterval 一行都没打印 —— 整条命令死在配置解析阶段，一个网络包都没发；"
line "     ③ known_hosts 太开客户端根本不管；④ 这句字符串确实在 ssh 二进制里 ——"
line "     所以它既可能是服务端 sshd 报的，也可能是你客户端的 Include 链报的，别一看到就往服务器上跑。"

say "④ 附录 C：实战 5 那两条判决本轮已由场景 5 演全（yes / ask+BatchMode 都拿到了干净原文），"
say "   这里只补最后一格：ask 且【不带 BatchMode】、又没有控制终端时（cron/CI 的真实形状），它卡在哪"
pause
run "printf '  ask + 不挂 BatchMode + 只允许 publickey（所以既不可能弹口令、也不可能登录成功）：\n'; timeout 20 setsid ssh -F /dev/null -o ConnectTimeout=8 -o IdentityFile=$SB/keys/_no-default-probe -o PreferredAuthentications=publickey -o StrictHostKeyChecking=ask -o UserKnownHostsFile=$SB/kh/v_ask2 $SSH_TARGET true </dev/null 2>&1 | tail -3"
line "  ↑ setsid 是故意让它没有控制终端，模拟 cron/CI。ssh 想问你「是不是这台机器」，"
line "     却找不到可以问的窗口（ssh_askpass 不存在），于是只留下光秃秃一句 Host key verification failed.。"
line "     比 ask+BatchMode 多的就只有那行 ssh_askpass —— 同一件事，报错信息量的差别就这一行。"

line ""
line "以上 14 个场景就是 README 里每条报错的证据链。"
