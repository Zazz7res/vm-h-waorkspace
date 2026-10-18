#!/usr/bin/env bash
# ============================================================================
# 04 ssh-agent 与连接复用 —— 一键复现整章场景
#
# 用法：
#   bash demo.sh            # 跑完自动清理沙箱 + 关掉自己起的 agent
#   bash demo.sh --keep     # 保留沙箱和自己起的 agent（打印路径，你可以进去接着敲）
#
# 安全边界（这个脚本严格遵守）：
#   * 所有 key / config / known_hosts 都在 mktemp 沙箱里：不读写 ~/.ssh，不碰 /etc/ssh
#   * 每条 ssh 都带 -F /dev/null（不读你真实的 config）+ 显式 UserKnownHostsFile（不写你真实的 known_hosts）
#   * 每条 ssh 都带 -o PreferredAuthentications=publickey -o BatchMode=yes
#     => 永远不会提交口令、永远不会登录成功；连本机 sshd 只会撞到「认证失败」这一步
#   * 脚本自己起的 ssh-agent 用 PID 记牢，退出时 ssh-agent -k 关掉
#   * 连本机 sshd 之间留 pause：连太多次会被服务端限流（限流长什么样见 06 章）
#   * 收尾四条自检：残留 agent 进程 / 本脚本那支 agent 的 PID 已退出 /
#     ss -lnt 与开工前基线一致 / ss -xl 里没有本脚本沙箱路径下的 socket
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

SB=$(mktemp -d "${TMPDIR:-/tmp}/ssh04-XXXXXX")   # 在任何 cwd 下运行都可以，内部一律绝对路径
mkdir -p "$SB/keys" "$SB/cm"
MYAGENT_PID=""
MYAGENT_SOCK=""
AGENT_LAUNCHED_PID=""
# 开工前先把「本机原有的 LISTEN 端口集合」存档，收尾拿它做对比自证（22 端口是真实 sshd 的，
# 本章只被动连接，绝不重启 / 停 / 改它 —— 对比结果里没有新增行就是证据）。
LISTEN_BEFORE=$(ss -lnt 2>/dev/null | tail -n +2 | awk '{print $4}' | sort)

cleanup() {
    if [ -n "$MYAGENT_PID" ]; then
        if [ "$KEEP" -eq 1 ]; then
            printf '\n[保留] 本脚本起的 ssh-agent 还活着：pid=%s socket=%s\n' "$MYAGENT_PID" "$MYAGENT_SOCK"
            printf '        自己关掉它：SSH_AUTH_SOCK=%s SSH_AGENT_PID=%s ssh-agent -k\n' "$MYAGENT_SOCK" "$MYAGENT_PID"
        else
            SSH_AUTH_SOCK="$MYAGENT_SOCK" SSH_AGENT_PID="$MYAGENT_PID" ssh-agent -k >/dev/null 2>&1 || true
        fi
    fi
    if [ "$KEEP" -eq 1 ]; then
        printf '\n[沙箱已保留] %s\n  进去自己敲： cd "%s"\n  看完自己删： rm -rf "%s"\n' "$SB" "$SB" "$SB"
    else
        rm -rf "$SB"
        printf '\n[沙箱已删除] %s\n' "$SB"
    fi
    printf '\n[收尾自检 1] 现在系统上还剩哪些 ssh-agent 进程\n'
    printf '              （你桌面环境原本就有的那几支不属于本脚本；本脚本起的那支应当已消失）\n'
    pgrep -a ssh-agent | sed 's/^/  /' || printf '  （一个都没有）\n'
    if [ -n "$AGENT_LAUNCHED_PID" ]; then
        printf '[收尾自检 2] 本脚本起过的那支 agent（pid=%s）还在吗：' "$AGENT_LAUNCHED_PID"
        if [ "$KEEP" -eq 1 ]; then
            printf '  --keep 模式，故意留着（自己按上面的命令关掉）\n'
        elif kill -0 "$AGENT_LAUNCHED_PID" 2>/dev/null; then
            printf '  还在（不正常，请手工关掉它）\n'
        else
            printf '  已退出\n'
        fi
    fi
    printf '[收尾自检 3] 监听端口和开工前基线对比（ss -lnt，只看第四列的地址:端口）：\n'
    LISTEN_AFTER=$(ss -lnt 2>/dev/null | tail -n +2 | awk '{print $4}' | sort)
    if [ "$LISTEN_BEFORE" = "$LISTEN_AFTER" ]; then
        printf '  干净：一个新增监听都没有（2222/8098/8099/1080/9100 全空，0.0.0.0:22 与 [::]:22 仍是那台真实 sshd）\n'
    else
        printf '  ⚠ 下面 diff 里带 > 的端口是本次新增的监听，需要人工确认：\n'
        diff <(printf '%s\n' "$LISTEN_BEFORE") <(printf '%s\n' "$LISTEN_AFTER") | sed 's/^/  /' || true
    fi
    printf '[收尾自检 4] 本脚本的沙箱路径下还有没有【在听的】Unix socket（ss -xl）：\n'
    if ss -xl 2>/dev/null | grep -F "$SB" >/dev/null; then
        printf '  ⚠ 还有残留，列出来：\n'
        ss -xl 2>/dev/null | grep -F "$SB" | sed 's/^/  /'
    else
        printf '  干净：ss -xl 里没有任何 %s 下的 socket（agent.sock 随 -k 消失，cm/ 里那个残留测试 socket 随沙箱删掉）\n' "$SB"
    fi
}
trap cleanup EXIT

# ---- 输出助手 -------------------------------------------------------------
scene() { printf '\n\n======== 场景 %s ========\n' "$*"; }
say()   { printf '\n--- %s\n' "$*"; }
line()  { printf '%s\n' "$*"; }
run()   { printf '\n$ %s\n' "$*"; bash -c "$*"; }                 # 期望成功
note()  { printf '\n$ %s\n  （这条按设计会失败，就为了看真实报错原文）\n' "$*"
          bash -c "$*" 2>&1 || printf '  [退出码 %s —— 非 0 是预期的]\n' "$?"; }   # 期望失败
pause() { sleep 8; }   # 给本机 sshd 喘口气

# 远端用户名故意用本机【不存在】的中性名 learner：
#   * 本章每一条 ssh 都按设计失败在认证阶段，用哪个用户名都不改变结论（Permission denied 照旧）；
#   * 但 `%r`、ControlPath 展开出的 socket 文件名、`%C` 哈希都由它派生 —— 用真实登录名会把
#     你的身份印进公开文档，用 learner 就把这一类派生值一并洗干净了。
#   * 本机用户名（`%u` / `%l` / `%d`）没法用假名跑：OpenSSH 拿 getpwuid 比对，`-l` 只管远端用户，
#     所以 README 里那几处统一写成 <本机用户> / <主机名> / <家目录>。
SSH_TARGET="learner@127.0.0.1"
# 所有 ssh 调用的公共护栏：不读真实 config、不写真实 known_hosts、不许用口令、不许交互、
# accept-new 让第一次连本机时自动把指纹记进沙箱 kh（顺带能看到那条 Warning）。
# 最后那个不存在的 IdentityFile 是「挡箭牌」：只要显式给了 IdentityFile，ssh 就不再把
# ~/.ssh/id_rsa / id_ed25519 … 这些默认文件名挨个探测一遍（本手册绝不碰你的 ~/.ssh）。
SSH_BASE="ssh -F /dev/null -o BatchMode=yes -o PreferredAuthentications=publickey -o StrictHostKeyChecking=accept-new -o UserKnownHostsFile=$SB/kh -o IdentityFile=$SB/keys/_no-default-probe -o ConnectTimeout=8"

printf '沙箱目录：%s\nOpenSSH 版本：%s\n目标（本机真实 sshd；learner 在本机【不存在】，专门用来撞认证失败）：%s\n' \
    "$SB" "$(ssh -V 2>&1)" "$SSH_TARGET"

# ============================================================================
scene "0：先把『agent 不在』这件事演一遍（后面所有诡异问题的起点）"
# ============================================================================
note "env -u SSH_AUTH_SOCK ssh-add -l"
say "SSH_AUTH_SOCK 指向一个不存在的路径（= 重启过 / 进容器 / 换了终端之后的样子）"
note "env SSH_AUTH_SOCK=$SB/这个文件不存在.sock ssh-add -l"
say "SSH_AUTH_SOCK 指向一个存在、但并不是 socket 的文件（= 把别人的路径硬抄过来）"
printf 'hello\n' > "$SB/notasocket"
note "env SSH_AUTH_SOCK=$SB/notasocket ssh-add -l"

# ============================================================================
scene "1：起一个真 agent，看清它到底是个什么东西"
# ============================================================================
say "先看 ssh-agent -s 打印了什么（这个临时 agent 下面立刻被关掉）"
TMP_OUT=$(ssh-agent -s -a "$SB/tmp-print.sock")
printf '\n$ ssh-agent -s -a %s/tmp-print.sock\n%s\n' "$SB" "$TMP_OUT"
TMP_PID=$(printf '%s\n' "$TMP_OUT" | sed -n 's/.*SSH_AGENT_PID=\([0-9]*\).*/\1/p')
TMP_SOCK=$(printf '%s\n' "$TMP_OUT" | sed -n 's/.*SSH_AUTH_SOCK=\([^;]*\).*/\1/p')
run "env SSH_AUTH_SOCK=$TMP_SOCK SSH_AGENT_PID=$TMP_PID ssh-agent -k"
line "  ↑ 看：ssh-agent -s 干的两件事 = 在后台起一个进程 + 打印几行『该怎么设环境变量』的文本。"
line "    所以必须 eval 它，否则进程起来了而你的 shell 不知道门牌号 —— 这就是『加了 key 却没生效』的第一种形态。"
say "现在起一个真正要用的 agent"
printf '\n$ eval "$(ssh-agent -s -a %s/agent.sock)"\n' "$SB"
eval "$(ssh-agent -s -a "$SB/agent.sock")"
export SSH_AUTH_SOCK SSH_AGENT_PID
MYAGENT_PID=$SSH_AGENT_PID
MYAGENT_SOCK=$SSH_AUTH_SOCK
AGENT_LAUNCHED_PID=$SSH_AGENT_PID
say "环境变量里那个『门牌号』—— 它是后面一切问题的根源"
run "echo \"SSH_AUTH_SOCK=\$SSH_AUTH_SOCK\""
run "echo \"SSH_AGENT_PID=\$SSH_AGENT_PID\""
say "它是一个真实进程："
run "ps -o pid,ppid,cmd -p $MYAGENT_PID"
say "它是一个在 Unix socket 上【监听】的进程 —— 用 ss 亲眼看："
run "ss -xl | grep 'agent.sock' || echo '  (没命中)'"
say "socket 文件本身（权限位第一位是 s = socket，不是普通文件；0600 = 只有你能连）："
run "ls -l $SB/agent.sock"

# ============================================================================
scene "2：往里装钥匙"
# ============================================================================
say "生成三把不同算法的 key（都在沙箱，注释写清楚是谁的）"
run "ssh-keygen -q -t ed25519 -N '' -C key-alpha@sandbox -f $SB/keys/id_alpha"
run "ssh-keygen -q -t rsa -b 2048 -N '' -C key-beta@sandbox -f $SB/keys/id_beta"
run "ssh-keygen -q -t ecdsa -N '' -C key-gamma@sandbox -f $SB/keys/id_gamma"
run "ls -l $SB/keys"
note "ssh-add -l"
say "ssh-add 干的事：把私钥【交给 agent 进程保管】，不是复制到某个文件里"
run "ssh-add $SB/keys/id_alpha"
run "ssh-add $SB/keys/id_beta"
run "ssh-add $SB/keys/id_gamma"
say "-l 列指纹（短），-L 列公钥全文（能贴到 authorized_keys 的就是它，公钥可以随便给人）"
run "ssh-add -l"
run "ssh-add -L | cut -c1-60"
say "删一把 / 全删："
run "ssh-add -d $SB/keys/id_beta"
run "ssh-add -l"
run "ssh-add -D"
note "ssh-add -l"
run "ssh-add $SB/keys/id_alpha >/dev/null; ssh-add $SB/keys/id_beta >/dev/null; ssh-add $SB/keys/id_gamma >/dev/null; ssh-add -l"

# ============================================================================
scene "3：证明 ssh 用的是 agent 里的钥匙，而不是磁盘上的文件"
# ============================================================================
pause
say "手法：【故意不给任何 -i】，也不设 IdentitiesOnly —— ssh 只能从 agent 拿钥匙"
note "$SSH_BASE -vv $SSH_TARGET true 2>&1 | tee $SB/vv_agent.txt | grep -E 'Permanently added|get_agent_identities|Will attempt key|Offering public key|Permission denied'"
say "逐行读法（对照上面的真实输出）："
line "  agent returned 3 keys        <- 客户端顺着 socket 问了 agent 一句『你手里有哪些钥匙』，拿到 3 把"
line "  Will attempt key: ... agent  <- 行尾那个 agent 就是出处标签：来自 agent，不是来自磁盘文件"
line "  Offering public key: ...     <- 只把【公钥部分】亮给服务器，问『这把你的 authorized_keys 里有没有』"
line "  Permission denied(...)       <- 服务器说一把都没有。本机没有 authorized_keys，这就是预期结局。"
line "  ⚠ 真登录成功时中间会多一行『Server accepts key: ...』，客户端那时才去让 agent 签名。"
line "     本机不允许装 authorized_keys，所以那一行必须到你自己的服务器上验证（README 给了完整命令）。"
say "确认本地文件里确实没有那一行："
run "wc -l $SB/vv_agent.txt; grep -c 'Server accepts key' $SB/vv_agent.txt || echo '  （0 次命中 = 本机拿不到，符合预期）'"
say "对照实验 A：IdentityAgent=none 明确绕开 agent → agent 里的 3 把全都不见了"
pause
note "$SSH_BASE -o IdentityAgent=none -v $SSH_TARGET true 2>&1 | grep -E 'get_agent_identities|Offering public key|No more auth|Permission denied'"
say "对照实验 B：IdentitiesOnly=yes + 只给一把 -i → 行尾标签变成 explicit，且只亮这一把"
pause
note "$SSH_BASE -o IdentitiesOnly=yes -i $SB/keys/id_alpha -v $SSH_TARGET true 2>&1 | grep -E 'Will attempt key|Offering public key|Permission denied'"

# ============================================================================
scene "4：ssh-add 的高级开关（全部本机真跑）"
# ============================================================================
say "-t 3600：装进去的同时设一个『一小时后自动过期』"
run "ssh-add -t 3600 $SB/keys/id_alpha"
say "想看『过期真的会发生』？用 -t 1 现场等一次："
run "ssh-add -D >/dev/null; ssh-add -t 1 $SB/keys/id_alpha; ssh-add -l"
say "睡 3 秒，再看同一个 agent："
sleep 3
note "ssh-add -l"
say "-x / -X：给 agent 上锁/解锁（要输口令；脚本里用 SSH_ASKPASS 喂口令才跑得动）"
cat > "$SB/askpass.sh" <<'EOF'
#!/bin/sh
echo "sandbox-lock-phrase"
EOF
chmod +x "$SB/askpass.sh"
run "ssh-add -D >/dev/null; ssh-add $SB/keys/id_alpha >/dev/null; ssh-add -l"
run "SSH_ASKPASS=$SB/askpass.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0 ssh-add -x"
note "ssh-add -L"
pause
say "锁上之后，ssh 的 debug 里会出现这行（极具辨识度）：agent contains no identities"
note "$SSH_BASE -v $SSH_TARGET true 2>&1 | grep -E 'get_agent_identities|Offering|No more auth|Permission denied'"
run "SSH_ASKPASS=$SB/askpass.sh SSH_ASKPASS_REQUIRE=force DISPLAY=:0 ssh-add -X; ssh-add -l"
say "钥匙串集成：本机能验证的只有『配置项能不能设上』这一半"
run "ssh -F /dev/null -o AddKeysToAgent=yes -G $SSH_TARGET 2>/dev/null | grep -i addkeystoagent"
line "  Linux：config 里写 AddKeysToAgent yes（配合 askpass / gnome-keyring）；"
line "  macOS：老写法 ssh-add -K 已废弃，现在是 ssh-add --apple-use-keychain + UseKeychain yes。"
line "  ⚠ AddKeysToAgent 只有在【认证真的成功过】之后才会把 key 落进 agent。本机认证不可能成功，"
line "     所以下面这条实验只能证明『认证失败时它不会加』，成功路径需你自己的服务器验证。"
pause
run "ssh-add -D >/dev/null"
say "加之前 agent 里有几把（1 = 只剩那句提示语本身，说明是空的）："
run "ssh-add -l 2>&1 | head -1"
note "$SSH_BASE -o AddKeysToAgent=yes -o IdentitiesOnly=yes -i $SB/keys/id_alpha $SSH_TARGET true"
run "ssh-add -l 2>&1 | head -1"

# ============================================================================
scene "5：『我明明加了 key 怎么还要口令』的头号原因 —— 环境变量传不过去"
# ============================================================================
run "echo \"当前 shell:   SSH_AUTH_SOCK=\$SSH_AUTH_SOCK\""
say "同一个用户、换一个干净的进程树（env -i 什么都不继承），门牌号就没了："
run "env -i PATH=\$PATH HOME=\$HOME bash -c 'echo \"  子 shell 里: SSH_AUTH_SOCK=\${SSH_AUTH_SOCK:-（空）}\"; ssh-add -l 2>&1 || echo \"  => 这就是新开终端 / 进容器 / sudo 之后遇到的情形\"'"
say "本机的 sudo 需要交互认证，跑不动（诚实标注：只给出真实报错，绝不猜密码）"
note "sudo -n env 2>&1 | head -1"
line "  在你能 sudo 的机器上，请自己对比这三条（这才是『sudo 把 agent 洗掉』的真实证据）："
line "    ssh-add -l"
line "    sudo ssh-add -l          # 大概率：Could not open a connection to your authentication agent."
line "    sudo -E ssh-add -l       # -E 保留环境变量，才可能延续同一个 agent"
line "  同理：su - 用户、docker exec、systemd 服务的 ExecStart、cron，都会重做一遍环境。"

# ============================================================================
scene "6：agent 里 key 太多 → Too many authentication failures（本机真能复现）"
# ============================================================================
say "往 agent 里塞到 8 把（模拟『公司/个人/临时项目一共十几把 key』）"
for i in 1 2 3 4 5 6 7 8; do
    ssh-keygen -q -t ed25519 -N '' -C "junk-$i@sandbox" -f "$SB/keys/j$i"
    ssh-add "$SB/keys/j$i" >/dev/null
done
run "ssh-add -l | wc -l"
pause
say "连一次：客户端会把 agent 里的钥匙一把一把亮出去，服务器数到第 6 次直接翻脸"
note "$SSH_BASE -v $SSH_TARGET true 2>&1 | grep -E 'Offering public key|Received disconnect|Disconnected' | tail -5"
say "修法是只给它一把（IdentitiesOnly=yes + -i），于是永远只亮 1 次："
pause
note "$SSH_BASE -o IdentitiesOnly=yes -i $SB/keys/j1 -v $SSH_TARGET true 2>&1 | grep -E 'Offering public key|Received disconnect|Permission denied'"
run "ssh-add -D"

# ============================================================================
scene "7：连接复用 ControlMaster —— 本机只能演『看得见的那一半』"
# ============================================================================
say "默认值：复用是关的（用 ssh -G 看最终生效配置，见 03 章）"
run "ssh -F /dev/null -G $SSH_TARGET | grep -E '^(controlmaster|controlpath|controlpersist) '"
line "  ↑ 输出里没有 controlpath 这一行 = 压根没设（这是最常见的误读）"
say "推荐的三件套（为什么是这三件见 README）："
line "    ControlMaster auto"
line "    ControlPath   ~/.ssh/cm-%r@%h:%p"
line "    ControlPersist 10m"
say "%r %h %p %C %n 这些记号展开成什么？用 -O check 的报错把它印出来最直观："
note "ssh -O check -o ControlPath=$SB/cm/%r@%h:%p $SSH_TARGET"
note "ssh -O check -o ControlPath=$SB/cm/%C $SSH_TARGET"
note "ssh -O check -o ControlPath=$SB/cm/%n-%h-%p $SSH_TARGET"
say "ControlPath 的目录不存在时是什么行为？—— 不报错，静默不复用（最坑人的一处）"
pause
note "rm -rf $SB/cm_noexist; $SSH_BASE -v -o ControlMaster=auto -o ControlPath=$SB/cm_noexist/%r@%h:%p $SSH_TARGET true 2>&1 | grep -E 'auto-mux|Control socket|Permission denied'"
say "socket 残留、或那个位置上被放了普通文件时，-O check 的真实症状："
printf 'not a socket\n' > "$SB/cm/stale-regular"
note "ssh -O check -o ControlPath=$SB/cm/stale-regular $SSH_TARGET"
python3 - "$SB/cm/stale.sock" <<'PY'
import socket, sys, os
p = sys.argv[1]
if os.path.exists(p): os.remove(p)
s = socket.socket(socket.AF_UNIX); s.bind(p); s.close()   # 故意不 unlink => 留下没人监听的 socket 文件
print("  已留下一个进程已死、但文件还在的 socket:", p)
PY
note "ssh -O check -o ControlPath=$SB/cm/stale.sock $SSH_TARGET"
say "登录成功才会有的东西，本机给不了（看：认证失败后 cm 目录里没有 master socket）"
run "ls -A $SB/cm | grep -v stale || echo '  （没有 learner@127.0.0.1:22 这种复用 socket）'"
line "  需要在有服务器时验证的三条（README 里有完整模板和预期数字）："
line "    1) time ssh 你的服务器 true        # 第一次：完整握手 + 认证"
line "    2) time ssh 你的服务器 true        # 10 分钟内第二次：直接复用，省掉握手+认证"
line "    3) ls -l ~/.ssh/cm-*; ssh -O check 你的服务器; ssh -O stop 你的服务器"

# ============================================================================
scene "8：收尾 —— 亲手关掉 agent，看进程和 socket 一起消失"
# ============================================================================
run "pgrep -a ssh-agent | grep -w $MYAGENT_PID"
run "SSH_AUTH_SOCK=$MYAGENT_SOCK SSH_AGENT_PID=$MYAGENT_PID ssh-agent -k"
MYAGENT_PID=""
say "socket 文件被自己回收了："
run "ls -l $SB/agent.sock 2>&1 || echo '  （文件已不存在）'"
note "ssh-add -l"

printf '\n以上 8 个场景就是 README 里各小节的证据链。\n'
